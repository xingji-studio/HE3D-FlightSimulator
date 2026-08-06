#include "he3d_convex_builder.hpp"

namespace HE3D
{

static void IncludePoint(AABB &bounds, float3 point);

static bool PointsNear(float3 a, float3 b, float epsilon = 0.0001f);

static bool DirectionsNear(float3 a, float3 b);

static bool IsFinitePoint(float3 point);

static void CanonicalizeDirection(float3 *direction);

static bool LessPoint(float3 a, float3 b);

static void SortPoints(float3 *points, int32_t count);

static bool AddUniqueDirection(float3 *directions, int32_t *count, int32_t capacity,
                               float3 direction);

static bool BuildSingleHull(const Mesh &mesh, ConvexBuildData &output);

static void IncludePoint(AABB &bounds, float3 point)
{
   if (point.x < bounds.min.x) bounds.min.x = point.x;
   if (point.y < bounds.min.y) bounds.min.y = point.y;
   if (point.z < bounds.min.z) bounds.min.z = point.z;
   if (point.x > bounds.max.x) bounds.max.x = point.x;
   if (point.y > bounds.max.y) bounds.max.y = point.y;
   if (point.z > bounds.max.z) bounds.max.z = point.z;
}

static bool PointsNear(float3 a, float3 b, float epsilon)
{
   return (a - b).lengthSq() <= epsilon * epsilon;
}

static bool DirectionsNear(float3 a, float3 b)
{
   float dot = float3::dot(a, b);
   return dot >= 0.9999f;
}

static bool IsFiniteFloat(float value)
{
   return value >= -340282346638528859811704183484516925440.0f &&
          value <= 340282346638528859811704183484516925440.0f;
}

static bool IsFinitePoint(float3 point)
{
   return IsFiniteFloat(point.x) && IsFiniteFloat(point.y) && IsFiniteFloat(point.z);
}

static void CanonicalizeDirection(float3 *direction)
{
   const float epsilon = 0.0000001f;
   if (direction->x < -epsilon ||
       (HE3D_ABS(direction->x) <= epsilon && direction->y < -epsilon) ||
       (HE3D_ABS(direction->x) <= epsilon && HE3D_ABS(direction->y) <= epsilon &&
        direction->z < -epsilon)) {
      *direction = -*direction;
   }
}

static bool LessPoint(float3 a, float3 b)
{
   if (a.x != b.x) return a.x < b.x;
   if (a.y != b.y) return a.y < b.y;
   return a.z < b.z;
}

static void SortPoints(float3 *points, int32_t count)
{
   for (int32_t i = 0; i < count - 1; i++) {
      for (int32_t j = i + 1; j < count; j++) {
         if (LessPoint(points[j], points[i])) {
            float3 swap = points[i];
            points[i]   = points[j];
            points[j]   = swap;
         }
      }
   }
}

static bool AddUniqueDirection(float3 *directions, int32_t *count, int32_t capacity,
                               float3 direction)
{
   if (direction.lengthSq() <= 0.0000001f) {
      return false;
   }
   direction = direction.normalizeFast();
   if (!IsFinitePoint(direction)) {
      return false;
   }
   CanonicalizeDirection(&direction);
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

static bool BuildSingleHull(const Mesh &mesh, ConvexBuildData &output)
{
   if (!mesh.IsValid() || mesh.GetVertexCount() < 12 || mesh.GetVertexCount() > 4096 ||
       (mesh.GetVertexCount() % 3) != 0) {
      return false;
   }

   const int32_t meshVertexCount = mesh.GetVertexCount();
   const float3 *meshVertices    = mesh.GetVertices();

   ConvexBuildPart part;
   int32_t vertexCount   = 0;
   int32_t faceAxisCount = 0;
   int32_t edgeAxisCount = 0;
   AABB    localBounds(meshVertices[0], meshVertices[0]);
   float3  centroid = {0, 0, 0};

   for (int32_t i = 0; i < meshVertexCount; i++) {
      if (!IsFinitePoint(meshVertices[i])) {
         return false;
      }
      bool duplicate = false;
      for (int32_t j = 0; j < vertexCount; j++) {
         if (PointsNear(meshVertices[i], part.vertices[j])) {
            duplicate = true;
            break;
         }
      }
      if (duplicate) {
         continue;
      }
      if (vertexCount >= 64) {
         return false;
      }
      part.vertices[vertexCount++] = meshVertices[i];
   }

   if (vertexCount < 4) {
      return false;
   }

   SortPoints(part.vertices, vertexCount);

   for (int32_t i = 0; i < vertexCount; i++) {
      IncludePoint(localBounds, part.vertices[i]);
      centroid = centroid + part.vertices[i];
   }
   centroid = centroid / static_cast<float>(vertexCount);
   if (!IsFinitePoint(localBounds.min) || !IsFinitePoint(localBounds.max) || !IsFinitePoint(centroid)) {
      return false;
   }

   float3 size = localBounds.max - localBounds.min;
   if (size.x <= 0.0001f || size.y <= 0.0001f || size.z <= 0.0001f) {
      return false;
   }

   for (int32_t triangle = 0; triangle < meshVertexCount; triangle += 3) {
      float3 a      = meshVertices[triangle];
      float3 b      = meshVertices[triangle + 1];
      float3 c      = meshVertices[triangle + 2];
      float3 normal = float3::cross(b - a, c - a);
      if (!IsFinitePoint(normal) || !IsFiniteFloat(normal.lengthSq()) ||
          normal.lengthSq() <= 0.0000001f) {
         return false;
      }
      normal = normal.normalizeFast();
      float3 faceCenter = (a + b + c) / 3.0f;
      if (!IsFinitePoint(normal) || !IsFinitePoint(faceCenter)) {
         return false;
      }
      if (float3::dot(normal, faceCenter - centroid) < 0.0f) {
         normal = -normal;
      }

      if (!AddUniqueDirection(part.faceAxes, &faceAxisCount, 16, normal) ||
          !AddUniqueDirection(part.edgeAxes, &edgeAxisCount, 64, b - a) ||
          !AddUniqueDirection(part.edgeAxes, &edgeAxisCount, 64, c - b) ||
          !AddUniqueDirection(part.edgeAxes, &edgeAxisCount, 64, a - c)) {
         return false;
      }
   }

   if (faceAxisCount < 3 || edgeAxisCount < 3) {
      return false;
   }

   SortPoints(part.faceAxes, faceAxisCount);
   SortPoints(part.edgeAxes, edgeAxisCount);

   part.vertexCount   = vertexCount;
   part.faceAxisCount = faceAxisCount;
   part.edgeAxisCount = edgeAxisCount;
   part.bounds        = localBounds;
   output.mode        = ConvexBuildMode::SingleHull;
   output.partCount   = 1;
   output.bounds      = localBounds;
   output.parts[0]    = part;
   return true;
}

bool ConvexBuilder::Build(const Mesh &mesh, ConvexBuildMode mode, ConvexBuildData &output)
{
   if (mode != ConvexBuildMode::SingleHull) {
      return false;
   }
   ConvexBuildData built;
   if (!BuildSingleHull(mesh, built)) {
      return false;
   }
   output = built;
   return true;
}

} // namespace HE3D
