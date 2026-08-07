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
   HE3D::Mesh mesh = HE3D::Mesh::CreateCube(1.0f, 1.0f, 1.0f);
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
   Check(SameBuildData(previous, data),
         "failed build does not mutate caller-owned data snapshot");
}

static void BuilderRejectsNonFiniteAndUnsupportedInput()
{
   const float nan = std::numeric_limits<float>::quiet_NaN();
   HE3D::float3 vertices[12] = {
       {-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f}, {nan, 0.5f, -0.5f},
       {0.5f, -0.5f, -0.5f},  {0.5f, 0.5f, -0.5f},   {0.5f, 0.5f, 0.5f},
       {0.5f, 0.5f, 0.5f},    {-0.5f, 0.5f, 0.5f},   {-0.5f, -0.5f, 0.5f},
       {-0.5f, -0.5f, 0.5f},   {-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, 0.5f},
   };
   HE3D::Mesh mesh = HE3D::Mesh::Create(vertices, 12);
   HE3D::ConvexBuildData data;
   Check(!HE3D::ConvexBuilder::Build(mesh, HE3D::ConvexBuildMode::SingleHull, data),
         "non-finite vertex is rejected");
   Check(!HE3D::ConvexBuilder::Build(HE3D::Mesh::CreateCube(),
                                     static_cast<HE3D::ConvexBuildMode>(99), data),
         "unsupported build mode is rejected");
}

static void BuilderCanonicalizesInputOrder()
{
   HE3D::Mesh original = HE3D::Mesh::CreateCube();
   HE3D::float3 reorderedVertices[36];
   const HE3D::float3 *source = original.GetVertices();
   for (HE3D::int32_t triangle = 0; triangle < 12; triangle++) {
      HE3D::int32_t sourceTriangle = 11 - triangle;
      reorderedVertices[triangle * 3 + 0] = source[sourceTriangle * 3 + 0];
      reorderedVertices[triangle * 3 + 1] = source[sourceTriangle * 3 + 2];
      reorderedVertices[triangle * 3 + 2] = source[sourceTriangle * 3 + 1];
   }
   HE3D::Mesh reordered = HE3D::Mesh::Create(reorderedVertices, 36);
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
   HE3D::float3 tooManyVertices[4098];
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
      float value = static_cast<float>(i + 1) * 0.1f;
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
       {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f},
       {-1.0f, -1.0f, 0.0f}, {1.0f, -1.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
       {-1.0f, 1.0f, 0.0f}, {-1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 0.0f},
       {1.0f, 1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f},
   };
   HE3D::ConvexBuildData data;
   Check(!HE3D::ConvexBuilder::Build(HE3D::Mesh::Create(degenerate, 12),
                                     HE3D::ConvexBuildMode::SingleHull, data),
         "degenerate triangle is rejected");

   HE3D::float3 planar[12] = {
       {-1.0f, -1.0f, 0.0f}, {1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 0.0f},
       {1.0f, 1.0f, 0.0f}, {-1.0f, 1.0f, 0.0f}, {-1.0f, -1.0f, 0.0f},
       {-1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f},
       {0.0f, 0.0f, 0.0f}, {1.0f, -1.0f, 0.0f}, {-1.0f, 1.0f, 0.0f},
   };
   Check(!HE3D::ConvexBuilder::Build(HE3D::Mesh::Create(planar, 12),
                                     HE3D::ConvexBuildMode::SingleHull, data),
         "zero volume bounds are rejected");
}

static HE3D::int32_t AppendTranslatedCube(HE3D::float3 *output, HE3D::int32_t offset,
                                          HE3D::float3 center)
{
   HE3D::Mesh cube = HE3D::Mesh::CreateCube();
   const HE3D::float3 *vertices = cube.GetVertices();
   for (HE3D::int32_t i = 0; i < cube.GetVertexCount(); i++) {
      output[offset + i] = vertices[i] + center;
   }
   return offset + cube.GetVertexCount();
}

static void BuilderBuildsCanonicalInternalDecomposition()
{
   HE3D::float3 uShape[108];
   HE3D::int32_t count = 0;
   count = AppendTranslatedCube(uShape, count, {-2.0f, 0.0f, 0.0f});
   count = AppendTranslatedCube(uShape, count, {0.0f, 0.0f, 0.0f});
   count = AppendTranslatedCube(uShape, count, {2.0f, 0.0f, 0.0f});
   HE3D::Mesh uMesh = HE3D::Mesh::Create(uShape, count);

   HE3D::ConvexBuildData first;
   HE3D::ConvexBuildData second;
   Check(HE3D::ConvexBuilder::BuildInternal(
             uMesh, HE3D::InternalConvexBuildMode::Decomposition, first) &&
             first.partCount == 3,
         "internal U-shaped decomposition produces three hull parts");
   Check(HE3D::ConvexBuilder::BuildInternal(
             uMesh, HE3D::InternalConvexBuildMode::Decomposition, second) &&
             SameBuildData(first, second),
         "internal decomposition output is canonical across repeated builds");
   Check(first.bounds.min.x < -2.4f && first.bounds.max.x > 2.4f,
         "decomposition stores aggregate bounds");
}

static void BuilderDecompositionFailureIsAtomic()
{
   HE3D::float3 cross[180];
   HE3D::int32_t count = 0;
   for (HE3D::int32_t i = 0; i < 5; i++) {
      HE3D::float3 center = {static_cast<float>((i - 2) * 2), 0.0f, 0.0f};
      count = AppendTranslatedCube(cross, count, center);
   }
   HE3D::Mesh mesh = HE3D::Mesh::Create(cross, count);
   HE3D::ConvexBuildData data;
   Check(HE3D::ConvexBuilder::BuildInternal(
             mesh, HE3D::InternalConvexBuildMode::Decomposition, data),
         "internal cross-shaped decomposition succeeds");
   HE3D::ConvexBuildData previous = data;
   Check(!HE3D::ConvexBuilder::BuildInternal(
             HE3D::Mesh::CreateCube(), HE3D::InternalConvexBuildMode::Decomposition, data) &&
             SameBuildData(previous, data),
         "decomposition failure preserves existing build data");
}

static void BuilderDecompositionEnforcesPartLimit()
{
   HE3D::float3 manyParts[612];
   HE3D::int32_t count = 0;
   for (HE3D::int32_t i = 0; i < 17; i++) {
      HE3D::float3 center = {static_cast<float>(i * 2), 0.0f, 0.0f};
      count = AppendTranslatedCube(manyParts, count, center);
   }
   HE3D::ConvexBuildData data;
   Check(!HE3D::ConvexBuilder::BuildInternal(
             HE3D::Mesh::Create(manyParts, count),
             HE3D::InternalConvexBuildMode::Decomposition, data),
         "internal decomposition enforces the sixteen-part limit");
   Check(!HE3D::ConvexBuilder::BuildInternal(
             HE3D::Mesh::CreateCube(), HE3D::InternalConvexBuildMode::SingleHull, data),
         "internal single-hull mode is not exposed through decomposition entry point");
}

int main()
{
   BuilderProducesSingleHullAndPreservesOldDataOnFailure();
   BuilderRejectsNonFiniteAndUnsupportedInput();
   BuilderCanonicalizesInputOrder();
   BuilderEnforcesInputAndAxisLimits();
   BuilderRejectsDegenerateAndZeroVolumeGeometry();
   BuilderBuildsCanonicalInternalDecomposition();
   BuilderDecompositionFailureIsAtomic();
   BuilderDecompositionEnforcesPartLimit();
   if (g_failures == 0) {
      std::printf("he3d_internal_convex_tests passed\n");
   }
   return g_failures == 0 ? 0 : 1;
}
