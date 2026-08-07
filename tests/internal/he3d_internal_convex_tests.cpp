#define private public
#include "he3d_convex_builder.hpp"
#undef private

#include <cstdio>
#include <limits>

static int g_failures = 0;

static void Check(bool condition, const char *name)
{
   if (!condition) {
      std::printf("FAIL: %s\n", name);
      g_failures++;
   }
}

static bool SameFloat3(HE3D::float3 a, HE3D::float3 b)
{
   return a.x == b.x && a.y == b.y && a.z == b.z;
}

static bool SameAABB(HE3D::AABB a, HE3D::AABB b)
{
   return SameFloat3(a.min, b.min) && SameFloat3(a.max, b.max);
}

static bool SameBuildData(const HE3D::ConvexBuildData &a, const HE3D::ConvexBuildData &b)
{
   if (a.mode != b.mode || a.partCount != b.partCount || !SameAABB(a.bounds, b.bounds)) {
      return false;
   }
   for (HE3D::int32_t part = 0; part < a.partCount; part++) {
      const HE3D::ConvexBuildPart &left  = a.parts[part];
      const HE3D::ConvexBuildPart &right = b.parts[part];
      if (left.vertexCount != right.vertexCount || left.faceAxisCount != right.faceAxisCount ||
          left.edgeAxisCount != right.edgeAxisCount || !SameAABB(left.bounds, right.bounds)) {
         return false;
      }
      for (HE3D::int32_t i = 0; i < left.vertexCount; i++) {
         if (!SameFloat3(left.vertices[i], right.vertices[i])) return false;
      }
      for (HE3D::int32_t i = 0; i < left.faceAxisCount; i++) {
         if (!SameFloat3(left.faceAxes[i], right.faceAxes[i])) return false;
      }
      for (HE3D::int32_t i = 0; i < left.edgeAxisCount; i++) {
         if (!SameFloat3(left.edgeAxes[i], right.edgeAxes[i])) return false;
      }
   }
   return true;
}

static void BuilderProducesSingleHullAndPreservesOldDataOnFailure()
{
   HE3D::Mesh            mesh = HE3D::Mesh::CreateCube(1.0f, 1.0f, 1.0f);
   HE3D::ConvexBuildData data;
   Check(HE3D::ConvexBuilder::Build(mesh, HE3D::ConvexBuildMode::SingleHull, data),
         "convex builder accepts a closed cube mesh");
   Check(data.partCount == 1, "convex builder produces one part for single hull mode");
   Check(data.parts[0].vertexCount == 8, "convex part deduplicates repeated cube vertices");
   Check(data.parts[0].faceAxisCount >= 3, "convex part stores face axes");
   Check(data.parts[0].edgeAxisCount >= 3, "convex part stores edge axes");

   HE3D::ConvexBuildData previous = data;
   HE3D::Mesh            invalid  = HE3D::Mesh::CreateTriangle(1.0f, 1.0f);
   Check(!HE3D::ConvexBuilder::Build(invalid, HE3D::ConvexBuildMode::SingleHull, data),
         "invalid mesh is rejected");
   Check(SameBuildData(previous, data), "failed build does not mutate caller-owned data snapshot");
}

static void BuilderRejectsNonFiniteAndUnsupportedInput()
{
   const float  nan          = std::numeric_limits<float>::quiet_NaN();
   HE3D::float3 vertices[12] = {
       {-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f}, {nan, 0.5f, -0.5f},    {0.5f, -0.5f, -0.5f},
       {0.5f, 0.5f, -0.5f},   {0.5f, 0.5f, 0.5f},   {0.5f, 0.5f, 0.5f},    {-0.5f, 0.5f, 0.5f},
       {-0.5f, -0.5f, 0.5f},  {-0.5f, -0.5f, 0.5f}, {-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, 0.5f},
   };
   HE3D::Mesh            mesh = HE3D::Mesh::Create(vertices, 12);
   HE3D::ConvexBuildData data;
   Check(!HE3D::ConvexBuilder::Build(mesh, HE3D::ConvexBuildMode::SingleHull, data),
         "non-finite vertex is rejected");
   Check(!HE3D::ConvexBuilder::Build(HE3D::Mesh::CreateCube(),
                                     static_cast<HE3D::ConvexBuildMode>(99), data),
         "unsupported build mode is rejected");
}

static void BuilderRejectsInvalidDecompositionInputAtomically()
{
   HE3D::Mesh            cube = HE3D::Mesh::CreateCube();
   HE3D::ConvexBuildData data;
   Check(HE3D::ConvexBuilder::Build(cube, HE3D::ConvexBuildMode::SingleHull, data),
         "single hull seeds decomposition atomicity data");
   HE3D::ConvexBuildData previous = data;

   const HE3D::float3 *cubeVertices = cube.GetVertices();
   HE3D::float3        openVertices[18];
   for (int i = 0; i < 18; i++) openVertices[i] = cubeVertices[i];
   Check(!HE3D::ConvexBuilder::BuildInternal(HE3D::Mesh::Create(openVertices, 18),
                                             HE3D::InternalConvexBuildMode::Decomposition, data) &&
             SameBuildData(previous, data),
         "open decomposition input fails atomically");

   HE3D::float3 nonManifold[39];
   for (int i = 0; i < 36; i++) nonManifold[i] = cubeVertices[i];
   for (int i = 0; i < 3; i++) nonManifold[36 + i] = cubeVertices[i];
   Check(!HE3D::ConvexBuilder::BuildInternal(HE3D::Mesh::Create(nonManifold, 39),
                                             HE3D::InternalConvexBuildMode::Decomposition, data) &&
             SameBuildData(previous, data),
         "non-manifold decomposition input fails atomically");

   HE3D::float3 oversized[4098];
   for (int i = 0; i < 4098; i++) oversized[i] = {static_cast<float>(i), 0.0f, 0.0f};
   Check(!HE3D::ConvexBuilder::BuildInternal(HE3D::Mesh::Create(oversized, 4098),
                                             HE3D::InternalConvexBuildMode::Decomposition, data) &&
             SameBuildData(previous, data),
         "oversized decomposition input fails atomically");
}

static void AddTriangle(HE3D::float3 *vertices, int &count, HE3D::float3 a, HE3D::float3 b,
                        HE3D::float3 c)
{
   vertices[count++] = a;
   vertices[count++] = b;
   vertices[count++] = c;
}

static void AddRectangleFaces(HE3D::float3 *vertices, int &count, float minX, float minY,
                              float maxX, float maxY)
{
   HE3D::float3 a = {minX, minY, 0.5f}, b = {maxX, minY, 0.5f};
   HE3D::float3 c = {maxX, maxY, 0.5f}, d = {minX, maxY, 0.5f};
   AddTriangle(vertices, count, a, b, c);
   AddTriangle(vertices, count, a, c, d);
   a.z = b.z = c.z = d.z = -0.5f;
   AddTriangle(vertices, count, a, c, b);
   AddTriangle(vertices, count, a, d, c);
}

static HE3D::Mesh CreateConcaveU()
{
   HE3D::float3 vertices[132];
   int          count = 0;
   AddRectangleFaces(vertices, count, -1.5f, -1.5f, -0.5f, -0.5f);
   AddRectangleFaces(vertices, count, -0.5f, -1.5f, 0.5f, -0.5f);
   AddRectangleFaces(vertices, count, 0.5f, -1.5f, 1.5f, -0.5f);
   AddRectangleFaces(vertices, count, -1.5f, -0.5f, -0.5f, 1.5f);
   AddRectangleFaces(vertices, count, 0.5f, -0.5f, 1.5f, 1.5f);
   const HE3D::float3 boundary[] = {{-1.5f, -1.5f, 0}, {-.5f, -1.5f, 0}, {.5f, -1.5f, 0},
                                    {1.5f, -1.5f, 0},  {1.5f, -.5f, 0},  {1.5f, 1.5f, 0},
                                    {.5f, 1.5f, 0},    {.5f, -.5f, 0},   {-.5f, -.5f, 0},
                                    {-.5f, 1.5f, 0},   {-1.5f, 1.5f, 0}, {-1.5f, -.5f, 0}};
   for (int edge = 0; edge < 12; edge++) {
      HE3D::float3 a = boundary[edge], b = boundary[(edge + 1) % 12];
      a.z = b.z      = -.5f;
      HE3D::float3 c = a, d = b;
      c.z = d.z = .5f;
      AddTriangle(vertices, count, a, b, d);
      AddTriangle(vertices, count, a, d, c);
   }
   return HE3D::Mesh::Create(vertices, count);
}

static bool AnyPartHullContains(const HE3D::ConvexBuildData &data, HE3D::float3 point);

static void BuilderConnectedUHasMultiplePartsWithoutCavity()
{
   HE3D::ConvexBuildData data;
   HE3D::Mesh            mesh = CreateConcaveU();
   bool                  built =
       HE3D::ConvexBuilder::BuildInternal(mesh, HE3D::InternalConvexBuildMode::Decomposition, data);
   Check(built && data.partCount > 1, "connected U decomposition produces multiple parts");
   const HE3D::float3 solids[] = {{-1.0f, -1.0f, 0.0f},
                                  {0.0f, -1.0f, 0.0f},
                                  {1.0f, -1.0f, 0.0f},
                                  {-1.0f, 0.5f, 0.0f},
                                  {1.0f, 0.5f, 0.0f}};
   for (HE3D::int32_t i = 0; built && i < 5; i++)
      Check(AnyPartHullContains(data, solids[i]), "connected U solid sample is covered by a hull");
   Check(built && !AnyPartHullContains(data, {0.0f, 0.5f, 0.0f}),
         "connected U cavity is excluded from part hulls");
   Check(built && !AnyPartHullContains(data, {2.0f, 2.0f, 0.0f}),
         "connected U external sample is excluded from part hulls");
}

static HE3D::Mesh CreateConcaveCross()
{
   const HE3D::float3 boundary[] = {
       {-0.5f, -1.5f, 0.0f}, {0.5f, -1.5f, 0.0f}, {0.5f, -0.5f, 0.0f},  {1.5f, -0.5f, 0.0f},
       {1.5f, 0.5f, 0.0f},   {0.5f, 0.5f, 0.0f},  {0.5f, 1.5f, 0.0f},   {-0.5f, 1.5f, 0.0f},
       {-0.5f, 0.5f, 0.0f},  {-1.5f, 0.5f, 0.0f}, {-1.5f, -0.5f, 0.0f}, {-0.5f, -0.5f, 0.0f}};
   HE3D::float3 vertices[156];
   int          count = 0;
   for (int edge = 0; edge < 12; edge++) {
      HE3D::float3 a = {0.0f, 0.0f, 0.5f}, b = boundary[edge], c = boundary[(edge + 1) % 12];
      a.z = b.z = c.z = 0.5f;
      AddTriangle(vertices, count, a, b, c);
      a.z = b.z = c.z = -0.5f;
      AddTriangle(vertices, count, a, c, b);
      a   = boundary[edge];
      b   = boundary[(edge + 1) % 12];
      a.z = b.z          = -0.5f;
      HE3D::float3 sideC = a, sideD = b;
      sideC.z = sideD.z = 0.5f;
      AddTriangle(vertices, count, a, b, sideD);
      AddTriangle(vertices, count, a, sideD, sideC);
   }
   return HE3D::Mesh::Create(vertices, count);
}

static bool PartHullContains(const HE3D::ConvexBuildPart &part, HE3D::float3 point)
{
   const float tolerance  = 0.0001f;
   bool        foundPlane = false;
   for (HE3D::int32_t first = 0; first < part.vertexCount - 2; first++) {
      for (HE3D::int32_t second = first + 1; second < part.vertexCount - 1; second++) {
         for (HE3D::int32_t third = second + 1; third < part.vertexCount; third++) {
            HE3D::float3 normal = HE3D::float3::cross(part.vertices[second] - part.vertices[first],
                                                      part.vertices[third] - part.vertices[first]);
            if (normal.lengthSq() <= tolerance * tolerance) continue;
            float offset   = HE3D::float3::dot(normal, part.vertices[first]);
            bool  positive = false;
            bool  negative = false;
            for (HE3D::int32_t vertex = 0; vertex < part.vertexCount; vertex++) {
               float distance = HE3D::float3::dot(normal, part.vertices[vertex]) - offset;
               positive       = positive || distance > tolerance;
               negative       = negative || distance < -tolerance;
            }
            if (positive && negative) continue;
            if (positive) {
               normal = -normal;
               offset = -offset;
            }
            foundPlane = true;
            if (HE3D::float3::dot(normal, point) - offset > tolerance) return false;
         }
      }
   }
   return foundPlane;
}

static bool AnyPartHullContains(const HE3D::ConvexBuildData &data, HE3D::float3 point)
{
   for (HE3D::int32_t i = 0; i < data.partCount; i++)
      if (PartHullContains(data.parts[i], point)) return true;
   return false;
}

static void BuilderConnectedCrossPreservesConcavity()
{
   HE3D::ConvexBuildData data;
   bool                  built = HE3D::ConvexBuilder::BuildInternal(
       CreateConcaveCross(), HE3D::InternalConvexBuildMode::Decomposition, data);
   Check(built && data.partCount > 1, "connected Cross decomposition produces multiple parts");
   const HE3D::float3 solids[] = {{0.0f, -1.0f, 0.0f},
                                  {0.0f, 0.0f, 0.0f},
                                  {1.0f, 0.0f, 0.0f},
                                  {0.0f, 1.0f, 0.0f},
                                  {-1.0f, 0.0f, 0.0f}};
   for (HE3D::int32_t i = 0; built && i < 5; i++)
      Check(AnyPartHullContains(data, solids[i]),
            "connected Cross solid sample is covered by a hull");
   Check(built && !AnyPartHullContains(data, {1.0f, 1.0f, 0.0f}),
         "connected Cross concave corner is excluded");
   Check(built && !AnyPartHullContains(data, {2.0f, 2.0f, 0.0f}),
         "connected Cross external sample is excluded");
}

static void BuilderCanonicalizesInputOrder()
{
   HE3D::Mesh          original = HE3D::Mesh::CreateCube();
   HE3D::float3        reorderedVertices[36];
   const HE3D::float3 *source = original.GetVertices();
   for (HE3D::int32_t triangle = 0; triangle < 12; triangle++) {
      HE3D::int32_t sourceTriangle        = 11 - triangle;
      reorderedVertices[triangle * 3 + 0] = source[sourceTriangle * 3 + 0];
      reorderedVertices[triangle * 3 + 1] = source[sourceTriangle * 3 + 2];
      reorderedVertices[triangle * 3 + 2] = source[sourceTriangle * 3 + 1];
   }
   HE3D::Mesh            reordered = HE3D::Mesh::Create(reorderedVertices, 36);
   HE3D::ConvexBuildData first;
   HE3D::ConvexBuildData second;
   Check(HE3D::ConvexBuilder::Build(original, HE3D::ConvexBuildMode::SingleHull, first) &&
             HE3D::ConvexBuilder::Build(reordered, HE3D::ConvexBuildMode::SingleHull, second) &&
             SameBuildData(first, second),
         "convex output is canonical across triangle order");
}

static void BuilderEnforcesInputAndAxisLimits()
{
   HE3D::ConvexBuildData data;
   HE3D::float3          tooManyVertices[4098];
   for (HE3D::int32_t i = 0; i < 4098; i++) tooManyVertices[i] = {1.0f, 1.0f, 1.0f};
   Check(!HE3D::ConvexBuilder::Build(HE3D::Mesh::Create(tooManyVertices, 4098),
                                     HE3D::ConvexBuildMode::SingleHull, data),
         "input vertex limit is enforced");

   HE3D::float3 tooManyUnique[66];
   for (HE3D::int32_t i = 0; i < 22; i++) {
      tooManyUnique[i * 3 + 0] = {static_cast<float>(i), 0.0f, 0.0f};
      tooManyUnique[i * 3 + 1] = {static_cast<float>(i), 1.0f, 0.0f};
      tooManyUnique[i * 3 + 2] = {static_cast<float>(i), 0.0f, 1.0f};
   }
   Check(!HE3D::ConvexBuilder::Build(HE3D::Mesh::Create(tooManyUnique, 66),
                                     HE3D::ConvexBuildMode::SingleHull, data),
         "unique vertex limit is enforced");

   HE3D::float3 tooManyAxes[51];
   for (HE3D::int32_t i = 0; i < 17; i++) {
      float value            = static_cast<float>(i + 1) * 0.1f;
      tooManyAxes[i * 3 + 0] = {0.0f, 0.0f, 0.0f};
      tooManyAxes[i * 3 + 1] = {1.0f, value, 0.0f};
      tooManyAxes[i * 3 + 2] = {0.0f, 1.0f, value};
   }
   Check(!HE3D::ConvexBuilder::Build(HE3D::Mesh::Create(tooManyAxes, 51),
                                     HE3D::ConvexBuildMode::SingleHull, data),
         "axis limits are enforced");
}

static void BuilderRejectsDegenerateAndZeroVolumeGeometry()
{
   HE3D::float3 degenerate[12] = {
       {0.0f, 0.0f, 0.0f},  {1.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f},  {-1.0f, -1.0f, 0.0f},
       {1.0f, -1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {-1.0f, 1.0f, 0.0f}, {-1.0f, -1.0f, 0.0f},
       {1.0f, 1.0f, 0.0f},  {1.0f, 1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f},
   };
   HE3D::ConvexBuildData data;
   Check(!HE3D::ConvexBuilder::Build(HE3D::Mesh::Create(degenerate, 12),
                                     HE3D::ConvexBuildMode::SingleHull, data),
         "degenerate triangle is rejected");

   HE3D::float3 planar[12] = {
       {-1.0f, -1.0f, 0.0f}, {1.0f, -1.0f, 0.0f},  {1.0f, 1.0f, 0.0f},   {1.0f, 1.0f, 0.0f},
       {-1.0f, 1.0f, 0.0f},  {-1.0f, -1.0f, 0.0f}, {-1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 0.0f},
       {0.0f, 0.0f, 0.0f},   {0.0f, 0.0f, 0.0f},   {1.0f, -1.0f, 0.0f},  {-1.0f, 1.0f, 0.0f},
   };
   Check(!HE3D::ConvexBuilder::Build(HE3D::Mesh::Create(planar, 12),
                                     HE3D::ConvexBuildMode::SingleHull, data),
         "zero volume bounds are rejected");
}

static HE3D::int32_t AppendTranslatedCube(HE3D::float3 *output, HE3D::int32_t offset,
                                          HE3D::float3 center)
{
   HE3D::Mesh          cube     = HE3D::Mesh::CreateCube();
   const HE3D::float3 *vertices = cube.GetVertices();
   for (HE3D::int32_t i = 0; i < cube.GetVertexCount(); i++) {
      output[offset + i] = vertices[i] + center;
   }
   return offset + cube.GetVertexCount();
}

static void BuilderBuildsCanonicalInternalDecomposition()
{
   HE3D::float3  uShape[108];
   HE3D::int32_t count = 0;
   count               = AppendTranslatedCube(uShape, count, {-2.0f, 0.0f, 0.0f});
   count               = AppendTranslatedCube(uShape, count, {0.0f, 0.0f, 0.0f});
   count               = AppendTranslatedCube(uShape, count, {2.0f, 0.0f, 0.0f});
   HE3D::Mesh uMesh    = HE3D::Mesh::Create(uShape, count);

   HE3D::ConvexBuildData first;
   HE3D::ConvexBuildData second;
   Check(HE3D::ConvexBuilder::BuildInternal(uMesh, HE3D::InternalConvexBuildMode::Decomposition,
                                            first) &&
             first.partCount == 3,
         "internal U-shaped decomposition produces three hull parts");
   Check(HE3D::ConvexBuilder::BuildInternal(uMesh, HE3D::InternalConvexBuildMode::Decomposition,
                                            second) &&
             SameBuildData(first, second),
         "internal decomposition output is canonical across repeated builds");
   Check(first.bounds.min.x < -2.4f && first.bounds.max.x > 2.4f,
         "decomposition stores aggregate bounds");
}

static void BuilderDecompositionCanonicalizesOrderAndWinding()
{
   HE3D::Mesh          original = CreateConcaveU();
   HE3D::float3        reordered[132];
   const HE3D::float3 *source = original.GetVertices();
   for (HE3D::int32_t triangle = 0; triangle < original.GetVertexCount() / 3; triangle++) {
      HE3D::int32_t from          = original.GetVertexCount() / 3 - triangle - 1;
      reordered[triangle * 3]     = source[from * 3];
      reordered[triangle * 3 + 1] = source[from * 3 + 2];
      reordered[triangle * 3 + 2] = source[from * 3 + 1];
   }
   HE3D::ConvexBuildData first;
   HE3D::ConvexBuildData second;
   Check(HE3D::ConvexBuilder::BuildInternal(original, HE3D::InternalConvexBuildMode::Decomposition,
                                            first) &&
             HE3D::ConvexBuilder::BuildInternal(
                 HE3D::Mesh::Create(reordered, original.GetVertexCount()),
                 HE3D::InternalConvexBuildMode::Decomposition, second) &&
             SameBuildData(first, second),
         "decomposition output is canonical across triangle order and winding");
}

static void BuilderDecompositionFailureIsAtomic()
{
   HE3D::float3  cross[180];
   HE3D::int32_t count = 0;
   for (HE3D::int32_t i = 0; i < 5; i++) {
      HE3D::float3 center = {static_cast<float>((i - 2) * 2), 0.0f, 0.0f};
      count               = AppendTranslatedCube(cross, count, center);
   }
   HE3D::Mesh            mesh = HE3D::Mesh::Create(cross, count);
   HE3D::ConvexBuildData data;
   Check(
       HE3D::ConvexBuilder::BuildInternal(mesh, HE3D::InternalConvexBuildMode::Decomposition, data),
       "internal cross-shaped decomposition succeeds");
   HE3D::ConvexBuildData previous = data;
   Check(!HE3D::ConvexBuilder::BuildInternal(HE3D::Mesh::CreateCube(),
                                             HE3D::InternalConvexBuildMode::Decomposition, data) &&
             SameBuildData(previous, data),
         "decomposition failure preserves existing build data");
}

static void BuilderDecompositionEnforcesPartLimit()
{
   HE3D::float3  manyParts[612];
   HE3D::int32_t count = 0;
   for (HE3D::int32_t i = 0; i < 17; i++) {
      HE3D::float3 center = {static_cast<float>(i * 2), 0.0f, 0.0f};
      count               = AppendTranslatedCube(manyParts, count, center);
   }
   HE3D::ConvexBuildData data;
   Check(!HE3D::ConvexBuilder::BuildInternal(HE3D::Mesh::Create(manyParts, count),
                                             HE3D::InternalConvexBuildMode::Decomposition, data),
         "internal decomposition enforces the sixteen-part limit");
   Check(!HE3D::ConvexBuilder::BuildInternal(HE3D::Mesh::CreateCube(),
                                             HE3D::InternalConvexBuildMode::SingleHull, data),
         "internal single-hull mode is not exposed through decomposition entry point");
}

static void BuilderPartsAreConvexHullData()
{
   HE3D::ConvexBuildData data;
   bool                  built = HE3D::ConvexBuilder::BuildInternal(
       CreateConcaveCross(), HE3D::InternalConvexBuildMode::Decomposition, data);
   for (HE3D::int32_t partIndex = 0; built && partIndex < data.partCount; partIndex++) {
      const HE3D::ConvexBuildPart &part = data.parts[partIndex];
      Check(part.vertexCount >= 4 && part.vertexCount <= 64,
            "leaf stores a bounded real hull vertex set");
      Check(part.faceAxisCount >= 3 && part.edgeAxisCount >= 3,
            "leaf stores SAT axes derived from hull topology");
      bool hasNonAxisAlignedFace = false;
      for (HE3D::int32_t vertex = 0; vertex < part.vertexCount; vertex++) {
         const HE3D::float3 point = part.vertices[vertex];
         Check(point.x >= part.bounds.min.x && point.x <= part.bounds.max.x &&
                   point.y >= part.bounds.min.y && point.y <= part.bounds.max.y &&
                   point.z >= part.bounds.min.z && point.z <= part.bounds.max.z,
               "leaf bounds contain every hull vertex");
         Check(PartHullContains(part, point), "leaf hull contains each boundary vertex");
      }
      for (HE3D::int32_t axis = 0; axis < part.faceAxisCount; axis++) {
         const HE3D::float3 normal = part.faceAxes[axis];
         const float        largest =
             HE3D_MAX(HE3D_ABS(normal.x), HE3D_MAX(HE3D_ABS(normal.y), HE3D_ABS(normal.z)));
         hasNonAxisAlignedFace = hasNonAxisAlignedFace || largest < 0.9999f;
      }
      Check(hasNonAxisAlignedFace || part.vertexCount == 8,
            "leaf data represents its hull rather than a substituted bounds shape");
   }
}

int main()
{
   BuilderProducesSingleHullAndPreservesOldDataOnFailure();
   BuilderRejectsNonFiniteAndUnsupportedInput();
   BuilderRejectsInvalidDecompositionInputAtomically();
   BuilderConnectedUHasMultiplePartsWithoutCavity();
   BuilderConnectedCrossPreservesConcavity();
   BuilderPartsAreConvexHullData();
   BuilderCanonicalizesInputOrder();
   BuilderEnforcesInputAndAxisLimits();
   BuilderRejectsDegenerateAndZeroVolumeGeometry();
   BuilderBuildsCanonicalInternalDecomposition();
   BuilderDecompositionCanonicalizesOrderAndWinding();
   BuilderDecompositionFailureIsAtomic();
   BuilderDecompositionEnforcesPartLimit();
   if (g_failures == 0) {
      std::printf("he3d_internal_convex_tests passed\n");
   }
   return g_failures == 0 ? 0 : 1;
}
