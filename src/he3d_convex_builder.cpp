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

static bool IsFiniteFloat(float value);

static bool BuildSingleHull(const Mesh &mesh, ConvexBuildData &output);
static bool AnalyzeInput(const Mesh &mesh);

struct AnalyzedTriangle {
   int32_t vertex[3];
};

struct AnalyzedEdge {
   int32_t first;
   int32_t second;
   int32_t direction;
   int32_t uses;
};

static const int32_t kVoxelResolution = 32;
static const int32_t kVoxelCount      = kVoxelResolution * kVoxelResolution * kVoxelResolution;
static const int32_t kVoxelCornerResolution = kVoxelResolution + 1;
static const int32_t kVoxelCornerCount =
    kVoxelCornerResolution * kVoxelCornerResolution * kVoxelCornerResolution;
static const int32_t kMaximumHullFaces = 124;

struct VoxelTransform {
   float3 origin;
   float  voxelSize;
};

struct VoxelWorkspace {
   unsigned char surface[kVoxelCount];
   unsigned char solid[kVoxelCount];
   unsigned char visited[kVoxelCount];
   unsigned char regionMask[kVoxelCount];
   unsigned char splitMask[kVoxelCount];
   unsigned char splitSeen[kVoxelCount];
   unsigned char corners[kVoxelCornerCount];
   int32_t       queue[kVoxelCount];
};

struct HullFace {
   int32_t indices[3];
   float3  normal;
   float   offset;
   bool    visible;
};

struct HorizonEdge {
   int32_t first;
   int32_t second;
};

static void    VoxelCoordinates(int32_t index, int32_t *x, int32_t *y, int32_t *z);
static int32_t VoxelIndex(int32_t x, int32_t y, int32_t z);
static int32_t CornerIndex(int32_t x, int32_t y, int32_t z);

struct VoxelRegion {
   int32_t *voxels;
   int32_t  count;
   int32_t  minX, minY, minZ;
   int32_t  maxX, maxY, maxZ;
   int32_t  empty;
   int32_t  closure;

   VoxelRegion()
       : voxels(nullptr), count(0), minX(kVoxelResolution), minY(kVoxelResolution),
         minZ(kVoxelResolution), maxX(0), maxY(0), maxZ(0), empty(0), closure(0)
   {
   }
};

static bool CollectRegionCorners(const VoxelRegion &region, VoxelWorkspace &workspace,
                                 float3 *points, int32_t *pointCount);
static bool BuildHull(const float3 *points, int32_t pointCount, HullFace *faces,
                      int32_t *faceCount);
static bool PointInsideHull(const HullFace *faces, int32_t faceCount, float3 point);

static void FreeRegion(VoxelRegion *region)
{
   if (!region) return;
   delete[] region->voxels;
   *region = VoxelRegion();
}

static void FreeRegions(VoxelRegion *regions, int32_t count)
{
   for (int32_t i = 0; i < count; i++) FreeRegion(&regions[i]);
}

static void RecomputeRegionBounds(VoxelRegion *region)
{
   region->minX = region->minY = region->minZ = kVoxelResolution;
   region->maxX = region->maxY = region->maxZ = 0;
   for (int32_t i = 0; i < region->count; i++) {
      int32_t x, y, z;
      VoxelCoordinates(region->voxels[i], &x, &y, &z);
      if (x < region->minX) region->minX = x;
      if (y < region->minY) region->minY = y;
      if (z < region->minZ) region->minZ = z;
      if (x > region->maxX) region->maxX = x;
      if (y > region->maxY) region->maxY = y;
      if (z > region->maxZ) region->maxZ = z;
   }
}

static bool ExtractSolidRegion(const unsigned char *solid, unsigned char *marks, int32_t start,
                               VoxelRegion *region)
{
   region->voxels = new int32_t[kVoxelCount];
   int32_t *queue = new int32_t[kVoxelCount];
   if (!region->voxels || !queue) {
      delete[] queue;
      FreeRegion(region);
      return false;
   }
   int32_t read = 0, write = 0;
   queue[write++] = start;
   marks[start]   = 1;
   while (read < write) {
      int32_t index                   = queue[read++];
      region->voxels[region->count++] = index;
      int32_t x, y, z;
      VoxelCoordinates(index, &x, &y, &z);
      const int32_t neighbors[6] = {
          x > 0 ? index - 1 : -1,
          x + 1 < kVoxelResolution ? index + 1 : -1,
          y > 0 ? index - kVoxelResolution : -1,
          y + 1 < kVoxelResolution ? index + kVoxelResolution : -1,
          z > 0 ? index - kVoxelResolution * kVoxelResolution : -1,
          z + 1 < kVoxelResolution ? index + kVoxelResolution * kVoxelResolution : -1};
      for (int32_t side = 0; side < 6; side++) {
         int32_t next = neighbors[side];
         if (next >= 0 && solid[next] && !marks[next]) {
            marks[next]    = 1;
            queue[write++] = next;
         }
      }
   }
   delete[] queue;
   RecomputeRegionBounds(region);
   return region->count > 0;
}

static bool ComputeClosureMetric(VoxelRegion *region, VoxelWorkspace &workspace)
{
   for (int32_t i = 0; i < kVoxelCount; i++) workspace.regionMask[i] = 0;
   for (int32_t i = 0; i < region->count; i++) workspace.regionMask[region->voxels[i]] = 1;
   float3   points[kVoxelCornerCount];
   int32_t  pointCount = 0;
   HullFace faces[kMaximumHullFaces];
   int32_t  faceCount = 0;
   if (!CollectRegionCorners(*region, workspace, points, &pointCount) ||
       !BuildHull(points, pointCount, faces, &faceCount))
      return false;
   int32_t closure = 0;
   int32_t empty   = 0;
   for (int32_t z = region->minZ; z <= region->maxZ; z++)
      for (int32_t y = region->minY; y <= region->maxY; y++)
         for (int32_t x = region->minX; x <= region->maxX; x++) {
            float3 center = {static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f,
                             static_cast<float>(z) + 0.5f};
            if (!PointInsideHull(faces, faceCount, center)) continue;
            closure++;
            if (!workspace.regionMask[VoxelIndex(x, y, z)]) empty++;
         }
   region->closure = closure;
   region->empty   = empty;
   return closure >= region->count;
}

static bool ExtractSplitComponents(const VoxelRegion &source, int32_t axis, int32_t slice,
                                   VoxelWorkspace &workspace, VoxelRegion *output,
                                   int32_t *outputCount)
{
   for (int32_t i = 0; i < kVoxelCount; i++) {
      workspace.splitMask[i] = 0;
      workspace.splitSeen[i] = 0;
   }
   for (int32_t i = 0; i < source.count; i++) workspace.splitMask[source.voxels[i]] = 1;
   *outputCount = 0;
   for (int32_t start = 0; start < kVoxelCount; start++) {
      if (!workspace.splitMask[start] || workspace.splitSeen[start]) continue;
      if (*outputCount >= 16) return false;
      VoxelRegion &region = output[(*outputCount)++];
      region.voxels       = new int32_t[kVoxelCount];
      if (!region.voxels) return false;
      int32_t read = 0, write = 0;
      workspace.queue[write++]   = start;
      workspace.splitSeen[start] = 1;
      while (read < write) {
         int32_t index                 = workspace.queue[read++];
         region.voxels[region.count++] = index;
         int32_t x, y, z;
         VoxelCoordinates(index, &x, &y, &z);
         const int32_t neighbors[6] = {x > 0 ? index - 1 : -1,    x + 1 < 32 ? index + 1 : -1,
                                       y > 0 ? index - 32 : -1,   y + 1 < 32 ? index + 32 : -1,
                                       z > 0 ? index - 1024 : -1, z + 1 < 32 ? index + 1024 : -1};
         for (int32_t side = 0; side < 6; side++) {
            int32_t next = neighbors[side];
            if (next < 0 || !workspace.splitMask[next] || workspace.splitSeen[next]) continue;
            int32_t nx, ny, nz;
            VoxelCoordinates(next, &nx, &ny, &nz);
            int32_t a = axis == 0 ? x : axis == 1 ? y : z;
            int32_t b = axis == 0 ? nx : axis == 1 ? ny : nz;
            if ((a <= slice) != (b <= slice)) continue;
            workspace.splitSeen[next] = 1;
            workspace.queue[write++]  = next;
         }
      }
      RecomputeRegionBounds(&region);
   }
   return *outputCount > 0;
}

static bool DecomposeRegion(const VoxelRegion &region, VoxelWorkspace &workspace,
                            VoxelRegion *leaves, int32_t *leafCount, int32_t depth)
{
   VoxelRegion current;
   current.voxels = new int32_t[kVoxelCount];
   if (!current.voxels) return false;
   current.count = region.count;
   for (int32_t i = 0; i < region.count; i++) current.voxels[i] = region.voxels[i];
   RecomputeRegionBounds(&current);
   if (!ComputeClosureMetric(&current, workspace)) {
      FreeRegion(&current);
      return false;
   }
   if (current.empty == 0) {
      if (*leafCount >= 16) {
         FreeRegion(&current);
         return false;
      }
      leaves[(*leafCount)++] = current;
      return true;
   }
   if (depth >= 15) {
      FreeRegion(&current);
      return false;
   }
   VoxelRegion best[16];
   int32_t     bestCount = 0;
   int64_t     bestEmpty = 0, bestClosure = 1;
   bool        found = false;
   for (int32_t axis = 0; axis < 3; axis++)
      for (int32_t slice = 0; slice < 31; slice++) {
         VoxelRegion candidates[16];
         int32_t     count = 0;
         if (!ExtractSplitComponents(current, axis, slice, workspace, candidates, &count) ||
             count < 2) {
            for (int32_t i = 0; i < count; i++) FreeRegion(&candidates[i]);
            continue;
         }
         int64_t empty = 0, closure = 0;
         bool    valid = true;
         for (int32_t i = 0; i < count; i++) {
            valid = valid && ComputeClosureMetric(&candidates[i], workspace);
            empty += candidates[i].empty;
            closure += candidates[i].closure;
         }
         bool improve = valid && closure > 0 &&
                        empty * current.closure < static_cast<int64_t>(current.empty) * closure;
         if (improve && (!found || empty * bestClosure < bestEmpty * closure ||
                         (empty * bestClosure == bestEmpty * closure && count < bestCount))) {
            for (int32_t i = 0; i < bestCount; i++) FreeRegion(&best[i]);
            for (int32_t i = 0; i < count; i++) {
               best[i]       = candidates[i];
               candidates[i] = VoxelRegion();
            }
            bestCount   = count;
            bestEmpty   = empty;
            bestClosure = closure;
            found       = true;
         }
         for (int32_t i = 0; i < count; i++) FreeRegion(&candidates[i]);
      }
   if (!found) {
      FreeRegion(&current);
      return false;
   }
   FreeRegion(&current);
   bool result = true;
   for (int32_t i = 0; i < bestCount; i++) {
      if (result) result = DecomposeRegion(best[i], workspace, leaves, leafCount, depth + 1);
      FreeRegion(&best[i]);
   }
   return result;
}

static int32_t VoxelIndex(int32_t x, int32_t y, int32_t z)
{
   return x + kVoxelResolution * (y + kVoxelResolution * z);
}

static int32_t CornerIndex(int32_t x, int32_t y, int32_t z)
{
   return x + kVoxelCornerResolution * (y + kVoxelCornerResolution * z);
}

static void VoxelCoordinates(int32_t index, int32_t *x, int32_t *y, int32_t *z)
{
   *x = index % kVoxelResolution;
   index /= kVoxelResolution;
   *y = index % kVoxelResolution;
   *z = index / kVoxelResolution;
}

static bool IsRedundantCorner(const unsigned char *corners, int32_t x, int32_t y, int32_t z)
{
   if (x > 0 && x + 1 < kVoxelCornerResolution && corners[CornerIndex(x - 1, y, z)] &&
       corners[CornerIndex(x + 1, y, z)])
      return true;
   if (y > 0 && y + 1 < kVoxelCornerResolution && corners[CornerIndex(x, y - 1, z)] &&
       corners[CornerIndex(x, y + 1, z)])
      return true;
   return z > 0 && z + 1 < kVoxelCornerResolution && corners[CornerIndex(x, y, z - 1)] &&
          corners[CornerIndex(x, y, z + 1)];
}

static bool CollectRegionCorners(const VoxelRegion &region, VoxelWorkspace &workspace,
                                 float3 *points, int32_t *pointCount)
{
   for (int32_t i = 0; i < kVoxelCornerCount; i++) workspace.corners[i] = 0;
   for (int32_t i = 0; i < region.count; i++) {
      int32_t x, y, z;
      VoxelCoordinates(region.voxels[i], &x, &y, &z);
      for (int32_t dz = 0; dz <= 1; dz++)
         for (int32_t dy = 0; dy <= 1; dy++)
            for (int32_t dx = 0; dx <= 1; dx++)
               workspace.corners[CornerIndex(x + dx, y + dy, z + dz)] = 1;
   }
   *pointCount = 0;
   for (int32_t z = 0; z < kVoxelCornerResolution; z++) {
      for (int32_t y = 0; y < kVoxelCornerResolution; y++) {
         for (int32_t x = 0; x < kVoxelCornerResolution; x++) {
            if (!workspace.corners[CornerIndex(x, y, z)] ||
                IsRedundantCorner(workspace.corners, x, y, z))
               continue;
            points[(*pointCount)++] = {static_cast<float>(x), static_cast<float>(y),
                                       static_cast<float>(z)};
         }
      }
   }
   SortPoints(points, *pointCount);
   return *pointCount >= 4;
}

static bool SetHullFace(HullFace *face, int32_t first, int32_t second, int32_t third,
                        const float3 *points, float3 interior, float areaToleranceSquared)
{
   float3 normal = float3::cross(points[second] - points[first], points[third] - points[first]);
   if (normal.lengthSq() <= areaToleranceSquared) return false;
   if (float3::dot(normal, interior - points[first]) > 0.0f) {
      int32_t swap = second;
      second       = third;
      third        = swap;
      normal       = -normal;
   }
   normal           = normal.normalizeFast();
   face->indices[0] = first;
   face->indices[1] = second;
   face->indices[2] = third;
   face->normal     = normal;
   face->offset     = float3::dot(normal, points[first]);
   face->visible    = false;
   return true;
}

static bool AddHorizonEdge(HorizonEdge *edges, int32_t *edgeCount, int32_t first, int32_t second)
{
   for (int32_t i = 0; i < *edgeCount; i++) {
      if (edges[i].first == second && edges[i].second == first) {
         edges[i] = edges[--(*edgeCount)];
         return true;
      }
   }
   if (*edgeCount >= kMaximumHullFaces * 3) return false;
   edges[*edgeCount].first  = first;
   edges[*edgeCount].second = second;
   (*edgeCount)++;
   return true;
}

static bool BuildHull(const float3 *points, int32_t pointCount, HullFace *faces, int32_t *faceCount)
{
   const float tolerance        = 0.0001f;
   const float toleranceSquared = tolerance * tolerance;
   if (pointCount < 4) return false;
   int32_t second   = -1;
   float   farthest = 0.0f;
   for (int32_t i = 1; i < pointCount; i++) {
      float distance = (points[i] - points[0]).lengthSq();
      if (distance > farthest) {
         farthest = distance;
         second   = i;
      }
   }
   if (second < 0 || farthest <= toleranceSquared) return false;
   int32_t third            = -1;
   farthest                 = 0.0f;
   float3 line              = points[second] - points[0];
   float  lineLengthSquared = line.lengthSq();
   for (int32_t i = 1; i < pointCount; i++) {
      if (i == second) continue;
      float distance = float3::cross(points[i] - points[0], line).lengthSq() / lineLengthSquared;
      if (distance > farthest) {
         farthest = distance;
         third    = i;
      }
   }
   if (third < 0 || farthest <= toleranceSquared) return false;
   float3  planeNormal = float3::cross(points[second] - points[0], points[third] - points[0]);
   float   planeLength = planeNormal.length();
   int32_t fourth      = -1;
   farthest            = 0.0f;
   for (int32_t i = 1; i < pointCount; i++) {
      if (i == second || i == third) continue;
      float distance = HE3D_ABS(float3::dot(planeNormal, points[i] - points[0])) / planeLength;
      if (distance > farthest) {
         farthest = distance;
         fourth   = i;
      }
   }
   if (fourth < 0 || farthest <= tolerance) return false;
   float3 interior = (points[0] + points[second] + points[third] + points[fourth]) / 4.0f;
   float  areaToleranceSquared = toleranceSquared * lineLengthSquared;
   *faceCount                  = 4;
   if (!SetHullFace(&faces[0], 0, second, third, points, interior, areaToleranceSquared) ||
       !SetHullFace(&faces[1], 0, fourth, second, points, interior, areaToleranceSquared) ||
       !SetHullFace(&faces[2], second, fourth, third, points, interior, areaToleranceSquared) ||
       !SetHullFace(&faces[3], third, fourth, 0, points, interior, areaToleranceSquared))
      return false;
   int32_t hullVertexCount = 4;
   while (true) {
      int32_t pointIndex      = -1;
      float   maximumDistance = tolerance;
      for (int32_t candidate = 0; candidate < pointCount; candidate++) {
         for (int32_t face = 0; face < *faceCount; face++) {
            float distance =
                float3::dot(faces[face].normal, points[candidate]) - faces[face].offset;
            if (distance > maximumDistance) {
               maximumDistance = distance;
               pointIndex      = candidate;
            }
         }
      }
      if (pointIndex < 0) break;
      if (hullVertexCount >= 64) return false;
      HorizonEdge horizon[kMaximumHullFaces * 3];
      int32_t     horizonCount = 0;
      int32_t     retained     = 0;
      for (int32_t face = 0; face < *faceCount; face++) {
         faces[face].visible =
             float3::dot(faces[face].normal, points[pointIndex]) - faces[face].offset > tolerance;
         if (!faces[face].visible) {
            faces[retained++] = faces[face];
            continue;
         }
         for (int32_t edge = 0; edge < 3; edge++)
            if (!AddHorizonEdge(horizon, &horizonCount, faces[face].indices[edge],
                                faces[face].indices[(edge + 1) % 3]))
               return false;
      }
      if (horizonCount < 3 || retained + horizonCount > kMaximumHullFaces) return false;
      *faceCount = retained;
      for (int32_t edge = 0; edge < horizonCount; edge++)
         if (!SetHullFace(&faces[(*faceCount)++], horizon[edge].first, horizon[edge].second,
                          pointIndex, points, interior, areaToleranceSquared))
            return false;
      hullVertexCount++;
   }
   for (int32_t face = 0; face < *faceCount; face++)
      for (int32_t point = 0; point < pointCount; point++)
         if (float3::dot(faces[face].normal, points[point]) - faces[face].offset > tolerance)
            return false;
   return *faceCount >= 4;
}

static bool PointInsideHull(const HullFace *faces, int32_t faceCount, float3 point)
{
   for (int32_t face = 0; face < faceCount; face++)
      if (float3::dot(faces[face].normal, point) - faces[face].offset > 0.0001f) return false;
   return true;
}

static float3 ToVoxelPoint(float3 point, const VoxelTransform &transform)
{
   return (point - transform.origin) / transform.voxelSize;
}

static bool TriangleOverlapsAxis(float3 a, float3 b, float3 c, float3 axis)
{
   if (axis.lengthSq() <= 0.000000000001f) return true;
   float pa      = float3::dot(a, axis);
   float pb      = float3::dot(b, axis);
   float pc      = float3::dot(c, axis);
   float minimum = HE3D_MIN(pa, HE3D_MIN(pb, pc));
   float maximum = HE3D_MAX(pa, HE3D_MAX(pb, pc));
   float radius  = 0.50001f * (HE3D_ABS(axis.x) + HE3D_ABS(axis.y) + HE3D_ABS(axis.z));
   return minimum <= radius && maximum >= -radius;
}

static bool TriangleOverlapsVoxel(float3 a, float3 b, float3 c, int32_t x, int32_t y, int32_t z)
{
   float3 center         = {static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f,
                            static_cast<float>(z) + 0.5f};
   a                     = a - center;
   b                     = b - center;
   c                     = c - center;
   float3       edges[3] = {b - a, c - b, a - c};
   const float3 axes[3]  = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
   for (int32_t axis = 0; axis < 3; axis++)
      if (!TriangleOverlapsAxis(a, b, c, axes[axis])) return false;
   if (!TriangleOverlapsAxis(a, b, c, float3::cross(edges[0], edges[1]))) return false;
   for (int32_t edge = 0; edge < 3; edge++)
      for (int32_t axis = 0; axis < 3; axis++)
         if (!TriangleOverlapsAxis(a, b, c, float3::cross(edges[edge], axes[axis]))) return false;
   return true;
}

static bool MarkSurfaceVoxels(const Mesh &mesh, const VoxelTransform &transform,
                              VoxelWorkspace &workspace)
{
   const float3 *vertices = mesh.GetVertices();
   for (int32_t triangle = 0; triangle < mesh.GetVertexCount(); triangle += 3) {
      float3 a      = ToVoxelPoint(vertices[triangle], transform);
      float3 b      = ToVoxelPoint(vertices[triangle + 1], transform);
      float3 c      = ToVoxelPoint(vertices[triangle + 2], transform);
      float3 normal = float3::cross(b - a, c - a);
      if (!IsFinitePoint(a) || !IsFinitePoint(b) || !IsFinitePoint(c) ||
          normal.lengthSq() <= 0.000000000001f)
         return false;
      float3  minimum = {HE3D_MIN(a.x, HE3D_MIN(b.x, c.x)), HE3D_MIN(a.y, HE3D_MIN(b.y, c.y)),
                         HE3D_MIN(a.z, HE3D_MIN(b.z, c.z))};
      float3  maximum = {HE3D_MAX(a.x, HE3D_MAX(b.x, c.x)), HE3D_MAX(a.y, HE3D_MAX(b.y, c.y)),
                         HE3D_MAX(a.z, HE3D_MAX(b.z, c.z))};
      int32_t firstX  = static_cast<int32_t>(minimum.x - 0.50001f);
      int32_t firstY  = static_cast<int32_t>(minimum.y - 0.50001f);
      int32_t firstZ  = static_cast<int32_t>(minimum.z - 0.50001f);
      int32_t lastX   = static_cast<int32_t>(maximum.x + 0.50001f);
      int32_t lastY   = static_cast<int32_t>(maximum.y + 0.50001f);
      int32_t lastZ   = static_cast<int32_t>(maximum.z + 0.50001f);
      firstX          = HE3D_MAX(0, HE3D_MIN(kVoxelResolution - 1, firstX));
      firstY          = HE3D_MAX(0, HE3D_MIN(kVoxelResolution - 1, firstY));
      firstZ          = HE3D_MAX(0, HE3D_MIN(kVoxelResolution - 1, firstZ));
      lastX           = HE3D_MAX(0, HE3D_MIN(kVoxelResolution - 1, lastX));
      lastY           = HE3D_MAX(0, HE3D_MIN(kVoxelResolution - 1, lastY));
      lastZ           = HE3D_MAX(0, HE3D_MIN(kVoxelResolution - 1, lastZ));
      for (int32_t z = firstZ; z <= lastZ; z++)
         for (int32_t y = firstY; y <= lastY; y++)
            for (int32_t x = firstX; x <= lastX; x++)
               if (TriangleOverlapsVoxel(a, b, c, x, y, z))
                  workspace.surface[VoxelIndex(x, y, z)] = 1;
   }
   return true;
}

static void ClassifySolidVoxels(VoxelWorkspace &workspace)
{
   int32_t read             = 0;
   int32_t write            = 0;
   workspace.queue[write++] = 0;
   workspace.visited[0]     = 1;
   while (read < write) {
      int32_t index = workspace.queue[read++];
      int32_t x, y, z;
      VoxelCoordinates(index, &x, &y, &z);
      const int32_t neighbors[6] = {
          x > 0 ? index - 1 : -1,
          x + 1 < kVoxelResolution ? index + 1 : -1,
          y > 0 ? index - kVoxelResolution : -1,
          y + 1 < kVoxelResolution ? index + kVoxelResolution : -1,
          z > 0 ? index - kVoxelResolution * kVoxelResolution : -1,
          z + 1 < kVoxelResolution ? index + kVoxelResolution * kVoxelResolution : -1};
      for (int32_t side = 0; side < 6; side++) {
         int32_t next = neighbors[side];
         if (next >= 0 && !workspace.surface[next] && !workspace.visited[next]) {
            workspace.visited[next]  = 1;
            workspace.queue[write++] = next;
         }
      }
   }
   for (int32_t i = 0; i < kVoxelCount; i++)
      if (!workspace.visited[i]) workspace.solid[i] = 1;
}

static bool AnalyzeInput(const Mesh &mesh)
{
   const int32_t inputCount = mesh.GetVertexCount();
   if (!mesh.IsValid() || !mesh.GetVertices() || inputCount < 12 || inputCount > 4096 ||
       (inputCount % 3) != 0)
      return false;
   const float3 *input = mesh.GetVertices();
   float3        points[4096];
   int32_t       pointCount = 0;
   AABB          bounds(input[0], input[0]);
   for (int32_t i = 0; i < inputCount; i++) {
      if (!IsFinitePoint(input[i])) return false;
      IncludePoint(bounds, input[i]);
      bool duplicate = false;
      for (int32_t j = 0; j < pointCount; j++) {
         if (PointsNear(input[i], points[j])) {
            duplicate = true;
            break;
         }
      }
      if (!duplicate) {
         if (pointCount >= 4096) return false;
         points[pointCount++] = input[i];
      }
   }
   float3 extent = bounds.max - bounds.min;
   float  scale  = HE3D_MAX(extent.x, HE3D_MAX(extent.y, extent.z));
   if (!IsFiniteFloat(scale) || scale <= 0.0001f) return false;
   float            tolerance            = scale * 0.00001f;
   float            areaToleranceSquared = tolerance * tolerance * scale * scale;
   AnalyzedTriangle triangles[1365];
   int32_t          triangleCount = 0;
   for (int32_t t = 0; t < inputCount / 3; t++) {
      AnalyzedTriangle triangle;
      for (int32_t v = 0; v < 3; v++) {
         int32_t best = -1;
         for (int32_t p = 0; p < pointCount; p++)
            if (PointsNear(input[t * 3 + v], points[p], tolerance)) {
               best = p;
               break;
            }
         if (best < 0) return false;
         triangle.vertex[v] = best;
      }
      if (triangle.vertex[0] == triangle.vertex[1] || triangle.vertex[1] == triangle.vertex[2] ||
          triangle.vertex[2] == triangle.vertex[0])
         return false;
      float3 normal = float3::cross(points[triangle.vertex[1]] - points[triangle.vertex[0]],
                                    points[triangle.vertex[2]] - points[triangle.vertex[0]]);
      if (!IsFinitePoint(normal) || normal.lengthSq() <= areaToleranceSquared) return false;
      int32_t sorted[3] = {triangle.vertex[0], triangle.vertex[1], triangle.vertex[2]};
      for (int32_t pass = 0; pass < 2; pass++) {
         for (int32_t i = 0; i < 2 - pass; i++)
            if (sorted[i] > sorted[i + 1]) {
               int32_t swap  = sorted[i];
               sorted[i]     = sorted[i + 1];
               sorted[i + 1] = swap;
            }
      }
      for (int32_t prior = 0; prior < triangleCount; prior++) {
         int32_t previous[3] = {triangles[prior].vertex[0], triangles[prior].vertex[1],
                                triangles[prior].vertex[2]};
         for (int32_t pass = 0; pass < 2; pass++)
            for (int32_t i = 0; i < 2 - pass; i++)
               if (previous[i] > previous[i + 1]) {
                  int32_t swap    = previous[i];
                  previous[i]     = previous[i + 1];
                  previous[i + 1] = swap;
               }
         if (previous[0] == sorted[0] && previous[1] == sorted[1] && previous[2] == sorted[2])
            return false;
      }
      if (triangleCount >= 1365) return false;
      triangles[triangleCount++] = triangle;
   }
   AnalyzedEdge edges[4096];
   int32_t      edgeCount = 0;
   for (int32_t t = 0; t < triangleCount; t++) {
      for (int32_t side = 0; side < 3; side++) {
         int32_t directedFirst  = triangles[t].vertex[side];
         int32_t directedSecond = triangles[t].vertex[(side + 1) % 3];
         int32_t first          = directedFirst < directedSecond ? directedFirst : directedSecond;
         int32_t second         = directedFirst < directedSecond ? directedSecond : directedFirst;
         int32_t direction      = directedFirst == first ? 1 : -1;
         int32_t found          = -1;
         for (int32_t e = 0; e < edgeCount; e++)
            if (edges[e].first == first && edges[e].second == second) {
               found = e;
               break;
            }
         if (found < 0) {
            if (edgeCount >= 4096) return false;
            edges[edgeCount++] = {first, second, direction, 1};
         } else {
            if (edges[found].uses >= 2 || edges[found].direction == direction) return false;
            edges[found].direction += direction;
            edges[found].uses++;
         }
      }
   }
   if (triangleCount < 4 || edgeCount == 0) return false;
   for (int32_t e = 0; e < edgeCount; e++)
      if (edges[e].uses != 2 || edges[e].direction != 0) return false;
   return true;
}

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
   if (direction->x < -epsilon || (HE3D_ABS(direction->x) <= epsilon && direction->y < -epsilon) ||
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
   int32_t         vertexCount   = 0;
   int32_t         faceAxisCount = 0;
   int32_t         edgeAxisCount = 0;
   AABB            localBounds(meshVertices[0], meshVertices[0]);
   float3          centroid = {0, 0, 0};

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
   if (!IsFinitePoint(localBounds.min) || !IsFinitePoint(localBounds.max) ||
       !IsFinitePoint(centroid)) {
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
      normal            = normal.normalizeFast();
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

struct HullEdge {
   int32_t first;
   int32_t second;
   int32_t direction;
   int32_t uses;
   float3  firstNormal;
   float3  secondNormal;
};

static bool BuildRegionPart(const VoxelRegion &region, const VoxelTransform &transform,
                            VoxelWorkspace &workspace, ConvexBuildPart *part)
{
   float3   points[kVoxelCornerCount];
   int32_t  pointCount = 0;
   HullFace faces[kMaximumHullFaces];
   int32_t  faceCount = 0;
   if (!CollectRegionCorners(region, workspace, points, &pointCount) ||
       !BuildHull(points, pointCount, faces, &faceCount))
      return false;
   int32_t remap[kVoxelCornerCount];
   for (int32_t point = 0; point < kVoxelCornerCount; point++) remap[point] = -1;
   for (int32_t face = 0; face < faceCount; face++)
      for (int32_t vertex = 0; vertex < 3; vertex++) remap[faces[face].indices[vertex]] = -2;
   int32_t vertexCount = 0;
   for (int32_t point = 0; point < pointCount; point++) {
      if (remap[point] == -2) {
         if (vertexCount >= 64) return false;
         remap[point] = vertexCount++;
      }
   }
   if (vertexCount < 4) return false;
   ConvexBuildPart built;
   AABB            bounds;
   bool            haveBounds = false;
   for (int32_t point = 0; point < pointCount; point++) {
      if (remap[point] < 0) continue;
      float3 world                 = transform.origin + points[point] * transform.voxelSize;
      built.vertices[remap[point]] = world;
      if (!haveBounds) {
         bounds     = AABB(world, world);
         haveBounds = true;
      } else IncludePoint(bounds, world);
   }
   HullEdge edges[kMaximumHullFaces * 3];
   int32_t  edgeCount = 0;
   for (int32_t face = 0; face < faceCount; face++) {
      if (!AddUniqueDirection(built.faceAxes, &built.faceAxisCount, 16, faces[face].normal))
         return false;
      for (int32_t side = 0; side < 3; side++) {
         int32_t first     = remap[faces[face].indices[side]];
         int32_t second    = remap[faces[face].indices[(side + 1) % 3]];
         int32_t low       = first < second ? first : second;
         int32_t high      = first < second ? second : first;
         int32_t direction = first == low ? 1 : -1;
         int32_t existing  = -1;
         for (int32_t edge = 0; edge < edgeCount; edge++)
            if (edges[edge].first == low && edges[edge].second == high) {
               existing = edge;
               break;
            }
         if (existing < 0) {
            edges[edgeCount++] = {low, high, direction, 1, faces[face].normal, {0, 0, 0}};
         } else {
            if (edges[existing].uses != 1) return false;
            edges[existing].uses = 2;
            edges[existing].direction += direction;
            edges[existing].secondNormal = faces[face].normal;
         }
      }
   }
   for (int32_t edge = 0; edge < edgeCount; edge++) {
      if (edges[edge].uses != 2 || edges[edge].direction != 0) return false;
      if (DirectionsNear(edges[edge].firstNormal, edges[edge].secondNormal)) continue;
      if (!AddUniqueDirection(built.edgeAxes, &built.edgeAxisCount, 64,
                              built.vertices[edges[edge].second] -
                                  built.vertices[edges[edge].first]))
         return false;
   }
   if (built.faceAxisCount < 3 || built.edgeAxisCount < 3) return false;
   SortPoints(built.vertices, vertexCount);
   SortPoints(built.faceAxes, built.faceAxisCount);
   SortPoints(built.edgeAxes, built.edgeAxisCount);
   built.vertexCount = vertexCount;
   built.bounds      = bounds;
   *part             = built;
   return true;
}

static bool BuildDecomposition(const Mesh &mesh, ConvexBuildData &output)
{
   if (!AnalyzeInput(mesh)) {
      return false;
   }
   const float3 *source = mesh.GetVertices();
   AABB          bounds(source[0], source[0]);
   for (int32_t i = 0; i < mesh.GetVertexCount(); i++) IncludePoint(bounds, source[i]);
   float3 extent = bounds.max - bounds.min;
   float  scale  = HE3D_MAX(extent.x, HE3D_MAX(extent.y, extent.z));
   if (!IsFiniteFloat(scale) || scale <= 0.0001f) return false;
   VoxelTransform transform;
   transform.voxelSize = scale / 30.0f;
   transform.origin =
       (bounds.min + bounds.max) * 0.5f - float3(16.0f, 16.0f, 16.0f) * transform.voxelSize;
   VoxelWorkspace workspace = {};
   if (!MarkSurfaceVoxels(mesh, transform, workspace)) return false;
   ClassifySolidVoxels(workspace);
   VoxelRegion roots[16];
   int32_t     rootCount = 0;
   for (int32_t i = 0; i < kVoxelCount; i++) {
      if (!workspace.solid[i] || workspace.visited[i]) continue;
      if (rootCount >= 16) {
         FreeRegions(roots, rootCount);
         return false;
      }
      if (!ExtractSolidRegion(workspace.solid, workspace.visited, i, &roots[rootCount])) {
         FreeRegions(roots, rootCount + 1);
         return false;
      }
      rootCount++;
   }
   if (rootCount == 0) return false;
   VoxelRegion leaves[16];
   int32_t     leafCount = 0;
   for (int32_t i = 0; i < rootCount; i++) {
      if (!DecomposeRegion(roots[i], workspace, leaves, &leafCount, 0)) {
         FreeRegions(roots, rootCount);
         FreeRegions(leaves, leafCount);
         return false;
      }
      FreeRegion(&roots[i]);
   }
   if (leafCount < 2) {
      FreeRegions(leaves, leafCount);
      return false;
   }
   ConvexBuildData built;
   built.mode = ConvexBuildMode::SingleHull;
   for (int32_t i = 0; i < leafCount; i++) {
      if (!BuildRegionPart(leaves[i], transform, workspace, &built.parts[built.partCount++])) {
         FreeRegions(leaves, leafCount);
         return false;
      }
      FreeRegion(&leaves[i]);
   }
   for (int32_t i = 0; i < built.partCount - 1; i++) {
      for (int32_t j = i + 1; j < built.partCount; j++) {
         if (LessPoint(built.parts[j].bounds.min, built.parts[i].bounds.min)) {
            ConvexBuildPart swap = built.parts[i];
            built.parts[i]       = built.parts[j];
            built.parts[j]       = swap;
         }
      }
   }
   built.bounds = built.parts[0].bounds;
   for (int32_t i = 1; i < built.partCount; i++) {
      IncludePoint(built.bounds, built.parts[i].bounds.min);
      IncludePoint(built.bounds, built.parts[i].bounds.max);
   }
   output = built;
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

bool ConvexBuilder::BuildInternal(const Mesh &mesh, InternalConvexBuildMode mode,
                                  ConvexBuildData &output)
{
   if (mode != InternalConvexBuildMode::Decomposition) return false;
   ConvexBuildData built;
   if (!BuildDecomposition(mesh, built)) return false;
   output = built;
   return true;
}

} // namespace HE3D
