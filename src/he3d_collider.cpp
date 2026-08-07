#include "he3d_convex_builder.hpp"

namespace HE3D
{

static void IncludePoint(AABB &bounds, float3 point)
{
   if (point.x < bounds.min.x) bounds.min.x = point.x;
   if (point.y < bounds.min.y) bounds.min.y = point.y;
   if (point.z < bounds.min.z) bounds.min.z = point.z;
   if (point.x > bounds.max.x) bounds.max.x = point.x;
   if (point.y > bounds.max.y) bounds.max.y = point.y;
   if (point.z > bounds.max.z) bounds.max.z = point.z;
}

static bool PointsNear(float3 a, float3 b, float epsilon = 0.0001f)
{
   return (a - b).lengthSq() <= epsilon * epsilon;
}

static bool DirectionsNear(float3 a, float3 b)
{
   float dot = float3::dot(a, b);
   return HE3D_ABS(dot) >= 0.9999f;
}

static bool IsFiniteFloat(float value)
{
   return value >= -340282346638528859811704183484516925440.0f &&
          value <= 340282346638528859811704183484516925440.0f;
}

HeightFieldCollider::Tile::Tile()
    : active(false), originX(0.0f), originZ(0.0f), bounds(), cellBounds(nullptr), cellCount(0),
      cellCapacity(0)
{
}

HeightFieldCollider::Tile::~Tile() { delete[] cellBounds; }

bool HeightFieldCollider::Tile::EnsureCapacity(int32_t count)
{
   if (count <= cellCapacity) {
      return true;
   }

   AABB *next = new AABB[count];
   if (!next) {
      return false;
   }
   for (int32_t i = 0; i < cellCount; i++) {
      next[i] = cellBounds[i];
   }
   delete[] cellBounds;
   cellBounds   = next;
   cellCapacity = count;
   return true;
}

HeightFieldCollider::HeightFieldCollider()
    : sampleHeight(nullptr), sampleUser(nullptr), gridCount(0), cellSize(0.0f)
{
}

HeightFieldCollider::HeightFieldCollider(HeightSampleCallback callback, void *user,
                                         int32_t gridCountValue, float cellSizeValue)
    : sampleHeight(nullptr), sampleUser(nullptr), gridCount(0), cellSize(0.0f)
{
   Configure(callback, user, gridCountValue, cellSizeValue);
}

void HeightFieldCollider::Configure(HeightSampleCallback callback, void *user,
                                    int32_t gridCountValue, float cellSizeValue)
{
   sampleHeight = callback;
   sampleUser   = user;
   gridCount    = gridCountValue;
   cellSize     = cellSizeValue;
}

bool HeightFieldCollider::IsValid() const
{
   return sampleHeight != nullptr && gridCount >= 2 && cellSize > 0.0f;
}

float HeightFieldCollider::Sample(float x, float z) const
{
   return sampleHeight ? sampleHeight(x, z, sampleUser) : 0.0f;
}

bool HeightFieldCollider::BuildTile(Tile &tile, float originX, float originZ) const
{
   tile.active    = false;
   tile.originX   = originX;
   tile.originZ   = originZ;
   tile.cellCount = 0;
   tile.bounds    = AABB({0, 0, 0}, {0, 0, 0});

   if (!IsValid()) {
      return false;
   }

   int32_t cellsPerSide = gridCount - 1;
   int32_t cellTotal    = cellsPerSide * cellsPerSide;
   if (cellTotal <= 0 || !tile.EnsureCapacity(cellTotal)) {
      return false;
   }

   float half  = static_cast<float>(cellsPerSide) * cellSize * 0.5f;
   bool  first = true;
   for (int32_t z = 0; z < cellsPerSide; z++) {
      for (int32_t x = 0; x < cellsPerSide; x++) {
         float  x0  = originX + static_cast<float>(x) * cellSize - half;
         float  z0  = originZ + static_cast<float>(z) * cellSize - half;
         float  x1  = x0 + cellSize;
         float  z1  = z0 + cellSize;
         float3 p00 = {x0, Sample(x0, z0), z0};
         float3 p01 = {x0, Sample(x0, z1), z1};
         float3 p10 = {x1, Sample(x1, z0), z0};
         float3 p11 = {x1, Sample(x1, z1), z1};
         AABB   cell(p00, p00);
         IncludePoint(cell, p01);
         IncludePoint(cell, p10);
         IncludePoint(cell, p11);
         cell.min.y -= 0.05f;
         cell.max.y += 0.05f;
         tile.cellBounds[tile.cellCount++] = cell;

         if (first) {
            tile.bounds = cell;
            first       = false;
         } else {
            IncludePoint(tile.bounds, cell.min);
            IncludePoint(tile.bounds, cell.max);
         }
      }
   }

   tile.active = true;
   return true;
}

float3 HeightFieldCollider::NormalAt(float x, float z) const
{
   float epsilon = cellSize > 0.0f ? cellSize * 0.0625f : 0.75f;
   if (epsilon < 0.1f) epsilon = 0.1f;
   float hx0 = Sample(x - epsilon, z);
   float hx1 = Sample(x + epsilon, z);
   float hz0 = Sample(x, z - epsilon);
   float hz1 = Sample(x, z + epsilon);
   return float3(hx0 - hx1, 2.0f * epsilon, hz0 - hz1).normalizeFast();
}

Collider::Collider(ColliderKind kind)
    : m_kind(kind), m_localBounds(AABB({0, 0, 0}, {0, 0, 0})), m_valid(false)
{
}

void Collider::SetColliderState(bool valid, AABB localBounds)
{
   m_valid       = valid;
   m_localBounds = valid ? localBounds : AABB({0, 0, 0}, {0, 0, 0});
}

bool Collider::IsValid() const { return m_valid; }

AABB Collider::LocalAABB() const { return m_valid ? m_localBounds : AABB({0, 0, 0}, {0, 0, 0}); }

ColliderKind Collider::GetKind() const { return m_kind; }

BoxCollider::BoxCollider() : Collider(ColliderKind::Box), m_halfExtents{0, 0, 0} {}

BoxCollider::BoxCollider(float width, float height, float depth)
    : Collider(ColliderKind::Box), m_halfExtents{0, 0, 0}
{
   SetSize(width, height, depth);
}

bool BoxCollider::SetSize(float width, float height, float depth)
{
   bool valid    = IsFiniteFloat(width) && IsFiniteFloat(height) && IsFiniteFloat(depth) &&
                   width > 0.0f && height > 0.0f && depth > 0.0f;
   m_halfExtents = valid ? float3(width * 0.5f, height * 0.5f, depth * 0.5f) : float3(0, 0, 0);
   SetColliderState(valid, AABB(-m_halfExtents, m_halfExtents));
   return valid;
}

static float3 EstimateBoundsInertia(AABB bounds, float mass)
{
   if (!IsFiniteFloat(mass) || mass <= 0.0f) {
      return {0, 0, 0};
   }
   float3 size = bounds.max - bounds.min;
   if (size.x < 0.001f) size.x = 0.001f;
   if (size.y < 0.001f) size.y = 0.001f;
   if (size.z < 0.001f) size.z = 0.001f;
   float ix = mass * (size.y * size.y + size.z * size.z) / 12.0f;
   float iy = mass * (size.x * size.x + size.z * size.z) / 12.0f;
   float iz = mass * (size.x * size.x + size.y * size.y) / 12.0f;
   return {ix, iy, iz};
}

static float EstimateBoundsDamping(AABB bounds, float mass)
{
   float3 size     = bounds.max - bounds.min;
   float  area     = HE3D_MAX(size.x * size.y, HE3D_MAX(size.x * size.z, size.y * size.z));
   float  safeMass = mass > 0.001f ? mass : 1.0f;
   float  value    = area / safeMass * 0.02f;
   if (value < 0.0f) value = 0.0f;
   if (value > 1.0f) value = 1.0f;
   return value;
}

float3 Collider::EstimateInertia(float mass) const
{
   return EstimateBoundsInertia(LocalAABB(), mass);
}

float Collider::EstimateDamping(float mass) const
{
   return EstimateBoundsDamping(LocalAABB(), mass);
}

ConvexCollider::ConvexCollider() : Collider(ColliderKind::Convex), m_cache(), m_hasBuild(false) {}

ConvexCollider::ConvexCollider(const Mesh &mesh, ConvexBuildMode mode) : ConvexCollider()
{
   BuildFromMesh(mesh, mode);
}

ConvexCollider::~ConvexCollider() { Clear(); }

void ConvexCollider::Clear()
{
   m_cache    = ConvexFeatureCache();
   m_hasBuild = false;
   SetColliderState(false, AABB());
}

static bool AddUniqueDirection(float3 *directions, int32_t *count, int32_t capacity,
                               float3 direction)
{
   if (direction.lengthSq() <= 0.0000001f) {
      return false;
   }
   direction = direction.normalizeFast();
   for (int32_t i = 0; i < *count; i++) {
      if (DirectionsNear(directions[i], direction)) {
         return true;
      }
   }
   if (*count >= capacity) {
      return false;
   }
   directions[(*count)++] = direction;
   return true;
}

int32_t ConvexCollider::GetPartCount() const
{
   return m_hasBuild ? m_cache.buildData.partCount : 0;
}

bool ConvexCollider::BuildFromMesh(const Mesh &mesh, ConvexBuildMode mode)
{
   ConvexBuildData buildData;
   if (!ConvexBuilder::Build(mesh, mode, buildData) || buildData.partCount < 1) {
      return false;
   }

   ConvexFeatureCache next;
   next.buildData = buildData;
   m_cache        = next;
   m_hasBuild     = true;
   SetColliderState(true, buildData.bounds);
   return true;
}

MeshCollider::MeshCollider(const Mesh &mesh) : Collider(ColliderKind::Mesh), m_mesh(&mesh)
{
   Refresh();
}

void MeshCollider::Refresh()
{
   SetColliderState(false, AABB());
   if (!m_mesh || !m_mesh->IsValid() || m_mesh->GetVertexCount() < 3 ||
       (m_mesh->GetVertexCount() % 3) != 0) {
      return;
   }

   const float3 *vertices = m_mesh->GetVertices();
   m_localBounds          = AABB(vertices[0], vertices[0]);
   for (int32_t i = 1; i < m_mesh->GetVertexCount(); i++) {
      IncludePoint(m_localBounds, vertices[i]);
   }
   SetColliderState(true, m_localBounds);
}

InternalContactShape Detail::ColliderAccess::From(const GameObject  &object,
                                                  const BoxCollider &collider)
{
   InternalContactShape shape;
   shape.kind          = InternalShapeKind::Box;
   shape.object        = &object;
   shape.localCenter   = {0, 0, 0};
   shape.halfExtents   = collider.m_halfExtents;
   shape.vertices      = nullptr;
   shape.vertexCount   = 8;
   shape.faceAxes      = nullptr;
   shape.faceAxisCount = 3;
   shape.edgeAxes      = nullptr;
   shape.edgeAxisCount = 3;
   shape.mesh          = nullptr;
   shape.localBounds   = collider.LocalAABB();
   return shape;
}

InternalContactShape Detail::ColliderAccess::From(const GameObject     &object,
                                                  const ConvexCollider &collider)
{
   return Detail::ColliderAccess::GetShape(object, collider, 0);
}

int32_t Detail::ColliderAccess::GetShapeCount(const ConvexCollider &collider)
{
   return collider.m_hasBuild ? collider.m_cache.buildData.partCount : 0;
}

InternalContactShape Detail::ColliderAccess::GetShape(const GameObject     &object,
                                                      const ConvexCollider &collider,
                                                      int32_t               partIndex)
{
   InternalContactShape shape;
   shape.kind   = InternalShapeKind::Convex;
   shape.object = &object;
   if (!collider.m_hasBuild || partIndex < 0 || partIndex >= collider.m_cache.buildData.partCount) {
      shape.localCenter   = {0, 0, 0};
      shape.halfExtents   = {0, 0, 0};
      shape.vertices      = nullptr;
      shape.vertexCount   = 0;
      shape.faceAxes      = nullptr;
      shape.faceAxisCount = 0;
      shape.edgeAxes      = nullptr;
      shape.edgeAxisCount = 0;
      shape.mesh          = nullptr;
      shape.localBounds   = AABB();
      return shape;
   }
   const ConvexBuildPart &part = collider.m_cache.buildData.parts[partIndex];
   shape.localCenter           = (part.bounds.min + part.bounds.max) * 0.5f;
   shape.halfExtents           = (part.bounds.max - part.bounds.min) * 0.5f;
   shape.vertices              = part.vertices;
   shape.vertexCount           = part.vertexCount;
   shape.faceAxes              = part.faceAxes;
   shape.faceAxisCount         = part.faceAxisCount;
   shape.edgeAxes              = part.edgeAxes;
   shape.edgeAxisCount         = part.edgeAxisCount;
   shape.mesh                  = nullptr;
   shape.localBounds           = part.bounds;
   return shape;
}

InternalContactShape Detail::ColliderAccess::From(const GameObject   &object,
                                                  const MeshCollider &collider)
{
   InternalContactShape shape;
   shape.kind          = InternalShapeKind::StaticMesh;
   shape.object        = &object;
   shape.localCenter   = (collider.m_localBounds.min + collider.m_localBounds.max) * 0.5f;
   shape.halfExtents   = (collider.m_localBounds.max - collider.m_localBounds.min) * 0.5f;
   shape.vertices      = collider.m_mesh ? collider.m_mesh->GetVertices() : nullptr;
   shape.vertexCount   = collider.m_mesh ? collider.m_mesh->GetVertexCount() : 0;
   shape.faceAxes      = nullptr;
   shape.faceAxisCount = 0;
   shape.edgeAxes      = nullptr;
   shape.edgeAxisCount = 0;
   shape.mesh          = collider.m_mesh;
   shape.localBounds   = collider.m_localBounds;
   return shape;
}

static float3 BoxLocalVertex(const InternalContactShape &shape, int32_t index)
{
   return {(index & 1) ? shape.halfExtents.x : -shape.halfExtents.x,
           (index & 2) ? shape.halfExtents.y : -shape.halfExtents.y,
           (index & 4) ? shape.halfExtents.z : -shape.halfExtents.z};
}

static float3 ShapeLocalVertex(const InternalContactShape &shape, int32_t index)
{
   return shape.kind == InternalShapeKind::Box ? BoxLocalVertex(shape, index)
                                               : shape.vertices[index];
}

static float3 ShapeWorldVertex(const InternalContactShape &shape, int32_t index)
{
   return shape.object->position + shape.object->orientation.rotate(ShapeLocalVertex(shape, index));
}

static float3 ShapeWorldCenter(const InternalContactShape &shape)
{
   return shape.object->position + shape.object->orientation.rotate(shape.localCenter);
}

static float3 ShapeLocalFaceAxis(const InternalContactShape &shape, int32_t index)
{
   if (shape.kind != InternalShapeKind::Box) {
      return shape.faceAxes[index];
   }
   if (index == 0) return {1, 0, 0};
   if (index == 1) return {0, 1, 0};
   return {0, 0, 1};
}

static float3 ShapeLocalEdgeAxis(const InternalContactShape &shape, int32_t index)
{
   if (shape.kind != InternalShapeKind::Box) {
      return shape.edgeAxes[index];
   }
   return ShapeLocalFaceAxis(shape, index);
}

static AABB ShapeWorldAabb(const InternalContactShape &shape)
{
   float3 first = ShapeWorldVertex(shape, 0);
   AABB   bounds(first, first);
   for (int32_t i = 1; i < shape.vertexCount; i++) {
      IncludePoint(bounds, ShapeWorldVertex(shape, i));
   }
   return bounds;
}

static AABB TriangleWorldAabb(float3 a, float3 b, float3 c)
{
   AABB bounds(a, a);
   IncludePoint(bounds, b);
   IncludePoint(bounds, c);
   return bounds;
}

static void ProjectShape(const InternalContactShape &shape, float3 axis, float *outMin,
                         float *outMax)
{
   float projection = float3::dot(ShapeWorldVertex(shape, 0), axis);
   *outMin          = projection;
   *outMax          = projection;
   for (int32_t i = 1; i < shape.vertexCount; i++) {
      projection = float3::dot(ShapeWorldVertex(shape, i), axis);
      if (projection < *outMin) *outMin = projection;
      if (projection > *outMax) *outMax = projection;
   }
}

static bool TestSatAxis(const InternalContactShape &a, const InternalContactShape &b, float3 axis,
                        float3 centerDelta, float *minimumOverlap, float3 *bestNormal)
{
   if (axis.lengthSq() <= 0.0000001f) {
      return true;
   }
   axis       = axis.normalizeFast();
   float minA = 0.0f;
   float maxA = 0.0f;
   float minB = 0.0f;
   float maxB = 0.0f;
   ProjectShape(a, axis, &minA, &maxA);
   ProjectShape(b, axis, &minB, &maxB);
   float overlap = HE3D_MIN(maxA, maxB) - HE3D_MAX(minA, minB);
   if (overlap <= 0.0f) {
      return false;
   }
   if (overlap < *minimumOverlap) {
      *minimumOverlap = overlap;
      *bestNormal     = float3::dot(centerDelta, axis) >= 0.0f ? -axis : axis;
   }
   return true;
}

static bool PointInsideTriangle(float3 point, float3 a, float3 b, float3 c)
{
   float3 v0          = c - a;
   float3 v1          = b - a;
   float3 v2          = point - a;
   float  dot00       = float3::dot(v0, v0);
   float  dot01       = float3::dot(v0, v1);
   float  dot02       = float3::dot(v0, v2);
   float  dot11       = float3::dot(v1, v1);
   float  dot12       = float3::dot(v1, v2);
   float  denominator = dot00 * dot11 - dot01 * dot01;
   if (HE3D_ABS(denominator) <= 0.0000001f) {
      return false;
   }
   float       inverse   = 1.0f / denominator;
   float       u         = (dot11 * dot02 - dot01 * dot12) * inverse;
   float       v         = (dot00 * dot12 - dot01 * dot02) * inverse;
   const float tolerance = 0.001f;
   return u >= -tolerance && v >= -tolerance && u + v <= 1.0f + tolerance;
}

static bool AddUniqueContact(InternalContact *contacts, int32_t *count, int32_t capacity,
                             const InternalContact &contact)
{
   if (!contacts || !count || *count >= capacity || contact.penetration <= 0.0f) {
      return false;
   }
   for (int32_t i = 0; i < *count; i++) {
      if ((contacts[i].point - contact.point).lengthSq() < 0.000001f) {
         if (contact.penetration > contacts[i].penetration) {
            contacts[i] = contact;
         }
         return true;
      }
   }
   contacts[(*count)++] = contact;
   return true;
}

static bool ContactPreferred(const InternalContact &left, const InternalContact &right)
{
   if (left.penetration != right.penetration) return left.penetration > right.penetration;
   if (left.point.x != right.point.x) return left.point.x < right.point.x;
   if (left.point.y != right.point.y) return left.point.y < right.point.y;
   if (left.point.z != right.point.z) return left.point.z < right.point.z;
   if (left.normal.x != right.normal.x) return left.normal.x < right.normal.x;
   if (left.normal.y != right.normal.y) return left.normal.y < right.normal.y;
   return left.normal.z < right.normal.z;
}

static void SortContactsByPriority(InternalContact *contacts, int32_t count)
{
   for (int32_t i = 1; i < count; i++) {
      InternalContact value    = contacts[i];
      int32_t         position = i;
      while (position > 0 && ContactPreferred(value, contacts[position - 1])) {
         contacts[position] = contacts[position - 1];
         position--;
      }
      contacts[position] = value;
   }
}

static void AddStaticContactCandidate(InternalContact *contacts, int32_t *count,
                                      const InternalContact &contact)
{
   if (!contacts || !count || contact.penetration <= 0.0f) return;
   for (int32_t i = 0; i < *count; i++) {
      if ((contacts[i].point - contact.point).lengthSq() >= 0.000001f) continue;
      if (ContactPreferred(contact, contacts[i])) contacts[i] = contact;
      return;
   }
   if (*count < 32) {
      contacts[(*count)++] = contact;
      return;
   }
   int32_t worst = 0;
   for (int32_t i = 1; i < *count; i++)
      if (ContactPreferred(contacts[worst], contacts[i])) worst = i;
   if (ContactPreferred(contact, contacts[worst])) contacts[worst] = contact;
}

static void ReduceManifold(InternalContact *contacts, int32_t *count, int32_t maxContacts)
{
   if (!contacts || !count || *count <= 0 || maxContacts <= 0) {
      return;
   }
   int32_t limit = maxContacts < 4 ? maxContacts : 4;
   if (*count <= limit) {
      return;
   }

   InternalContact selected[4];
   bool            used[32];
   int32_t         sourceCount = *count < 32 ? *count : 32;
   for (int32_t i = 0; i < sourceCount; i++) {
      used[i] = false;
   }

   int32_t deepest = 0;
   for (int32_t i = 1; i < sourceCount; i++) {
      if (contacts[i].penetration > contacts[deepest].penetration) {
         deepest = i;
      }
   }
   selected[0]           = contacts[deepest];
   used[deepest]         = true;
   int32_t selectedCount = 1;

   while (selectedCount < limit) {
      int32_t best      = -1;
      float   bestScore = -1.0f;
      for (int32_t i = 0; i < sourceCount; i++) {
         if (used[i]) continue;
         float nearest = (contacts[i].point - selected[0].point).lengthSq();
         for (int32_t j = 1; j < selectedCount; j++) {
            float distance = (contacts[i].point - selected[j].point).lengthSq();
            if (distance < nearest) nearest = distance;
         }
         float score = nearest + contacts[i].penetration * 0.01f;
         if (score > bestScore) {
            bestScore = score;
            best      = i;
         }
      }
      if (best < 0) break;
      selected[selectedCount++] = contacts[best];
      used[best]                = true;
   }

   for (int32_t i = 0; i < selectedCount; i++) {
      contacts[i] = selected[i];
   }
   *count = selectedCount;
}

int32_t Detail::ContactPipeline::CollectStaticTriangles(const InternalContactShape &dynamicShape,
                                                        const InternalContactShape &staticShape,
                                                        InternalContact            *contacts,
                                                        int32_t                     maxContacts)
{
   if (!staticShape.mesh || !staticShape.object || !dynamicShape.object) {
      return 0;
   }

   AABB        queryBounds = ShapeWorldAabb(dynamicShape);
   const float queryMargin = 0.02f;
   queryBounds.min         = queryBounds.min - float3(queryMargin, queryMargin, queryMargin);
   queryBounds.max         = queryBounds.max + float3(queryMargin, queryMargin, queryMargin);

   InternalContact rawContacts[32];
   int32_t         rawCount = 0;
   const float3   *vertices = staticShape.mesh->GetVertices();
   for (int32_t triangle = 0; triangle < staticShape.mesh->GetVertexCount(); triangle += 3) {
      float3 a =
          staticShape.object->position + staticShape.object->orientation.rotate(vertices[triangle]);
      float3 b = staticShape.object->position +
                 staticShape.object->orientation.rotate(vertices[triangle + 1]);
      float3 c = staticShape.object->position +
                 staticShape.object->orientation.rotate(vertices[triangle + 2]);
      if (!TriangleWorldAabb(a, b, c).Intersects(queryBounds)) {
         continue;
      }

      float3 normal = float3::cross(b - a, c - a).normalizeFast();
      if (normal.lengthSq() <= 0.000001f) {
         continue;
      }
      if (float3::dot(ShapeWorldCenter(dynamicShape) - a, normal) < 0.0f) {
         normal = -normal;
      }

      for (int32_t vertex = 0; vertex < dynamicShape.vertexCount; vertex++) {
         float3 point    = ShapeWorldVertex(dynamicShape, vertex);
         float  distance = float3::dot(point - a, normal);
         if (distance >= 0.0f) {
            continue;
         }
         float3 projected = point - normal * distance;
         if (!PointInsideTriangle(projected, a, b, c)) {
            continue;
         }

         InternalContact contact;
         contact.hit         = true;
         contact.separation  = distance;
         contact.penetration = -distance;
         contact.normal      = normal;
         contact.point       = projected;
         AddStaticContactCandidate(rawContacts, &rawCount, contact);
      }
   }

   SortContactsByPriority(rawContacts, rawCount);
   ReduceManifold(rawContacts, &rawCount, maxContacts);
   int32_t count = rawCount < maxContacts ? rawCount : maxContacts;
   for (int32_t i = 0; i < count; i++) {
      contacts[i] = rawContacts[i];
   }
   return count;
}

int32_t Detail::ContactPipeline::CollectConvexPair(const InternalContactShape &a,
                                                   const InternalContactShape &b,
                                                   InternalContact *contacts, int32_t maxContacts)
{
   if (!ShapeWorldAabb(a).Intersects(ShapeWorldAabb(b))) {
      return 0;
   }

   float  minimumOverlap = 340282346638528859811704183484516925440.0f;
   float3 bestNormal     = {0, 1, 0};
   float3 centerDelta    = ShapeWorldCenter(b) - ShapeWorldCenter(a);

   for (int32_t i = 0; i < a.faceAxisCount; i++) {
      float3 axis = a.object->orientation.rotate(ShapeLocalFaceAxis(a, i));
      if (!TestSatAxis(a, b, axis, centerDelta, &minimumOverlap, &bestNormal)) {
         return 0;
      }
   }
   for (int32_t i = 0; i < b.faceAxisCount; i++) {
      float3 axis = b.object->orientation.rotate(ShapeLocalFaceAxis(b, i));
      if (!TestSatAxis(a, b, axis, centerDelta, &minimumOverlap, &bestNormal)) {
         return 0;
      }
   }
   for (int32_t edgeA = 0; edgeA < a.edgeAxisCount; edgeA++) {
      float3 axisA = a.object->orientation.rotate(ShapeLocalEdgeAxis(a, edgeA));
      for (int32_t edgeB = 0; edgeB < b.edgeAxisCount; edgeB++) {
         float3 axisB = b.object->orientation.rotate(ShapeLocalEdgeAxis(b, edgeB));
         if (!TestSatAxis(a, b, float3::cross(axisA, axisB), centerDelta, &minimumOverlap,
                          &bestNormal)) {
            return 0;
         }
      }
   }

   float minA = 0.0f;
   float maxA = 0.0f;
   float minB = 0.0f;
   float maxB = 0.0f;
   ProjectShape(a, bestNormal, &minA, &maxA);
   ProjectShape(b, bestNormal, &minB, &maxB);

   int32_t     count         = 0;
   const float faceTolerance = 0.002f;
   for (int32_t i = 0; i < a.vertexCount; i++) {
      float3 point      = ShapeWorldVertex(a, i);
      float  projection = float3::dot(point, bestNormal);
      if (projection <= minA + faceTolerance) {
         float           penetration = maxB - projection;
         InternalContact contact;
         contact.hit         = penetration > 0.0f;
         contact.separation  = -penetration;
         contact.penetration = penetration;
         contact.normal      = bestNormal;
         contact.point       = point;
         AddUniqueContact(contacts, &count, maxContacts, contact);
      }
   }

   for (int32_t i = 0; i < b.vertexCount && count < maxContacts; i++) {
      float3 point      = ShapeWorldVertex(b, i);
      float  projection = float3::dot(point, bestNormal);
      if (projection >= maxB - faceTolerance) {
         float           penetration = projection - minA;
         InternalContact contact;
         contact.hit         = penetration > 0.0f;
         contact.separation  = -penetration;
         contact.penetration = penetration;
         contact.normal      = bestNormal;
         contact.point       = point;
         AddUniqueContact(contacts, &count, maxContacts, contact);
      }
   }

   ReduceManifold(contacts, &count, maxContacts);
   return count;
}

int32_t Detail::ContactPipeline::Collect(const InternalContactShape &a,
                                         const InternalContactShape &b, InternalContact *contacts,
                                         int32_t maxContacts)
{
   if (!contacts || maxContacts <= 0 || !a.object || !b.object) {
      return 0;
   }
   if (a.kind == InternalShapeKind::StaticMesh && b.kind == InternalShapeKind::StaticMesh) {
      return 0;
   }
   if (a.kind == InternalShapeKind::StaticMesh) {
      int32_t count = CollectStaticTriangles(b, a, contacts, maxContacts);
      for (int32_t i = 0; i < count; i++) {
         contacts[i].normal = -contacts[i].normal;
      }
      return count;
   }
   if (b.kind == InternalShapeKind::StaticMesh) {
      return CollectStaticTriangles(a, b, contacts, maxContacts);
   }
   return CollectConvexPair(a, b, contacts, maxContacts);
}

int32_t GenerateInternalContacts(const InternalContactShape &a, const InternalContactShape &b,
                                 InternalContact *contacts, int32_t maxContacts)
{
   return Detail::ContactPipeline::Collect(a, b, contacts, maxContacts);
}

} // namespace HE3D
