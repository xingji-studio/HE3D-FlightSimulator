#define private public
#include "he3d_internal.hpp"
#undef private

#include <cstdio>

static int g_failures = 0;

static void Check(bool condition, const char *name)
{
   if (!condition) {
      std::printf("FAIL: %s\n", name);
      g_failures++;
   }
}

static int CollectBoxMesh(HE3D::GameObject &boxObject, HE3D::BoxCollider &boxCollider,
                          HE3D::GameObject &floorObject, HE3D::MeshCollider &floorCollider,
                          HE3D::InternalContact *contacts, int maxContacts)
{
   HE3D::InternalContactShape boxShape = HE3D::Detail::ColliderAccess::From(boxObject, boxCollider);
   HE3D::InternalContactShape floorShape =
       HE3D::Detail::ColliderAccess::From(floorObject, floorCollider);
   return HE3D::GenerateInternalContacts(boxShape, floorShape, contacts, maxContacts);
}

static void MeshContactsSeparateGapFromPenetration()
{
   HE3D::Mesh         floorMesh = HE3D::Mesh::CreatePlane(4.0f, 4.0f);
   HE3D::GameObject   floorObject(floorMesh);
   HE3D::MeshCollider floorCollider(floorMesh);
   HE3D::Mesh         cubeMesh = HE3D::Mesh::CreateCube(1.0f, 1.0f, 1.0f);
   HE3D::GameObject   boxObject(cubeMesh);
   HE3D::BoxCollider  boxCollider(1.0f, 1.0f, 1.0f);

   HE3D::InternalContact contacts[4];
   boxObject.position = {0.0f, 0.511f, 0.0f};
   Check(CollectBoxMesh(boxObject, boxCollider, floorObject, floorCollider, contacts, 4) == 0,
         "small positive gap does not generate mesh contact");

   boxObject.position = {0.0f, 0.49f, 0.0f};
   int count = CollectBoxMesh(boxObject, boxCollider, floorObject, floorCollider, contacts, 4);
   Check(count > 0 && contacts[0].penetration > 0.0f,
         "real penetration generates positive mesh contacts");
}

static HE3D::Mesh CreateLayeredTriangles(bool deepFirst)
{
   HE3D::float3  vertices[99];
   HE3D::int32_t count = 0;
   for (HE3D::int32_t layer = 0; layer < 33; layer++) {
      bool  deep        = deepFirst ? layer == 0 : layer == 32;
      float y           = deep ? 0.4f : 0.0f;
      vertices[count++] = {-2.0f, y, -2.0f};
      vertices[count++] = {2.0f, y, 2.0f};
      vertices[count++] = {2.0f, y, -2.0f};
   }
   return HE3D::Mesh::Create(vertices, count);
}

static void StaticTriangleDiscoveryScansPastWorkspacePrefix()
{
   HE3D::Mesh        cubeMesh = HE3D::Mesh::CreateCube();
   HE3D::GameObject  boxObject(cubeMesh);
   HE3D::BoxCollider boxCollider(1.0f, 1.0f, 1.0f);
   boxObject.position = {0.0f, 0.49f, 0.0f};
   HE3D::InternalContact contacts[1];

   HE3D::Mesh         lateMesh = CreateLayeredTriangles(false);
   HE3D::GameObject   lateObject(lateMesh);
   HE3D::MeshCollider lateCollider(lateMesh);
   int   lateCount = CollectBoxMesh(boxObject, boxCollider, lateObject, lateCollider, contacts, 1);
   float latePenetration = lateCount == 1 ? contacts[0].penetration : 0.0f;

   HE3D::Mesh         earlyMesh = CreateLayeredTriangles(true);
   HE3D::GameObject   earlyObject(earlyMesh);
   HE3D::MeshCollider earlyCollider(earlyMesh);
   int earlyCount = CollectBoxMesh(boxObject, boxCollider, earlyObject, earlyCollider, contacts, 1);
   float earlyPenetration = earlyCount == 1 ? contacts[0].penetration : 0.0f;
   Check(lateCount == 1 && earlyCount == 1 && latePenetration > 0.4f &&
             latePenetration == earlyPenetration,
         "static triangle contacts select late deep candidates independent of triangle order");
}

static void DynamicSatAndConvexCachesGenerateContacts()
{
   HE3D::Mesh       cubeMeshA = HE3D::Mesh::CreateCube(1.0f, 1.0f, 1.0f);
   HE3D::Mesh       cubeMeshB = HE3D::Mesh::CreateCube(1.0f, 1.0f, 1.0f);
   HE3D::GameObject lower(cubeMeshA);
   HE3D::GameObject upper(cubeMeshB);
   upper.position = {0.0f, 0.95f, 0.0f};

   HE3D::BoxCollider          lowerBox(1.0f, 1.0f, 1.0f);
   HE3D::BoxCollider          upperBox(1.0f, 1.0f, 1.0f);
   HE3D::InternalContactShape lowerShape = HE3D::Detail::ColliderAccess::From(lower, lowerBox);
   HE3D::InternalContactShape upperShape = HE3D::Detail::ColliderAccess::From(upper, upperBox);
   HE3D::InternalContact      contacts[4];
   int count = HE3D::GenerateInternalContacts(upperShape, lowerShape, contacts, 4);
   Check(count > 0 && count <= 4, "box SAT generates capped dynamic contacts");

   HE3D::ConvexCollider lowerConvex(cubeMeshA, HE3D::ConvexBuildMode::SingleHull);
   HE3D::ConvexCollider upperConvex(cubeMeshB, HE3D::ConvexBuildMode::SingleHull);
   lowerShape = HE3D::Detail::ColliderAccess::From(lower, lowerConvex);
   upperShape = HE3D::Detail::ColliderAccess::From(upper, upperConvex);
   count      = HE3D::GenerateInternalContacts(upperShape, lowerShape, contacts, 4);
   Check(count > 0 && count <= 4, "convex SAT uses collider caches");

   upper.position = {3.0f, 0.0f, 0.0f};
   upperShape     = HE3D::Detail::ColliderAccess::From(upper, upperConvex);
   Check(HE3D::GenerateInternalContacts(upperShape, lowerShape, contacts, 4) == 0,
         "convex SAT rejects separated shapes");
}

static void ConvexColliderAccessExposesIndexedParts()
{
   HE3D::float3        parts[108];
   HE3D::int32_t       count    = 0;
   HE3D::Mesh          cube     = HE3D::Mesh::CreateCube();
   const HE3D::float3 *vertices = cube.GetVertices();
   for (HE3D::int32_t part = 0; part < 3; part++) {
      for (HE3D::int32_t vertex = 0; vertex < cube.GetVertexCount(); vertex++) {
         parts[count++] = vertices[vertex] + HE3D::float3(static_cast<float>((part - 1) * 2), 0, 0);
      }
   }
   HE3D::Mesh           mesh = HE3D::Mesh::Create(parts, count);
   HE3D::ConvexCollider collider(mesh, HE3D::ConvexBuildMode::ConvexDecomposition);
   HE3D::GameObject     object(mesh);
   HE3D::int32_t        shapeCount = HE3D::Detail::ColliderAccess::GetShapeCount(collider);
   Check(collider.IsValid() && shapeCount == collider.GetPartCount() && shapeCount > 1,
         "collider access exposes every decomposition part");
   for (HE3D::int32_t part = 0; part < shapeCount; part++) {
      HE3D::InternalContactShape shape =
          HE3D::Detail::ColliderAccess::GetShape(object, collider, part);
      Check(shape.kind == HE3D::InternalShapeKind::Convex && shape.vertices &&
                shape.vertexCount >= 4 && shape.faceAxes && shape.edgeAxes,
            "indexed decomposition shape exposes valid convex geometry");
   }
   HE3D::InternalContactShape invalid =
       HE3D::Detail::ColliderAccess::GetShape(object, collider, shapeCount);
   Check(invalid.vertexCount == 0 && invalid.vertices == nullptr,
         "out-of-range decomposition shape access is empty");
}

static void PipelineRejectsInvalidOutputsAndStaticPairs()
{
   HE3D::Mesh                 floorMesh = HE3D::Mesh::CreatePlane(2.0f, 2.0f);
   HE3D::GameObject           floorObject(floorMesh);
   HE3D::MeshCollider         floorCollider(floorMesh);
   HE3D::InternalContactShape floor =
       HE3D::Detail::ColliderAccess::From(floorObject, floorCollider);

   HE3D::InternalContact contacts[4];
   Check(HE3D::GenerateInternalContacts(floor, floor, contacts, 4) == 0,
         "pipeline skips static-static pairs");

   HE3D::Mesh                 cubeMesh = HE3D::Mesh::CreateCube(1.0f, 1.0f, 1.0f);
   HE3D::GameObject           boxObject(cubeMesh);
   HE3D::BoxCollider          boxCollider(1.0f, 1.0f, 1.0f);
   HE3D::InternalContactShape box = HE3D::Detail::ColliderAccess::From(boxObject, boxCollider);
   Check(HE3D::GenerateInternalContacts(box, floor, nullptr, 4) == 0,
         "pipeline rejects null contact output");
   Check(HE3D::GenerateInternalContacts(box, floor, contacts, 0) == 0,
         "pipeline rejects zero contact capacity");
}

static void ConvexManifoldAndSolverRemainFinite()
{
   HE3D::Mesh              meshA = HE3D::Mesh::CreateCube(1.0f, 1.0f, 1.0f);
   HE3D::Mesh              meshB = HE3D::Mesh::CreateCube(1.0f, 1.0f, 1.0f);
   HE3D::GameObject        body(meshA);
   HE3D::GameObject        floor(meshB);
   HE3D::BoxCollider       bodyCollider(1.0f, 1.0f, 1.0f);
   HE3D::BoxCollider       floorCollider(4.0f, 1.0f, 4.0f);
   HE3D::PhysicsMaterial   material;
   HE3D::PhysicsProperties properties;
   HE3D::PhysicsScene      scene(2);
   body.position  = {0.0f, 0.49f, 0.0f};
   floor.position = {0.0f, -0.5f, 0.0f};
   properties.SetMaterial(material);
   scene.SetGravity({0.0f, 0.0f, 0.0f});
   scene.SetDrag(0.0f);
   HE3D::InternalContact      contacts[8];
   HE3D::InternalContactShape bodyShape  = HE3D::Detail::ColliderAccess::From(body, bodyCollider);
   HE3D::InternalContactShape floorShape = HE3D::Detail::ColliderAccess::From(floor, floorCollider);
   int count = HE3D::GenerateInternalContacts(bodyShape, floorShape, contacts, 8);
   Check(count > 0 && count <= 4, "convex pair manifold is reduced to four contacts");
   Check(scene.AddDynamicBody(body, bodyCollider, properties) &&
             scene.AddStaticBody(floor, floorCollider, material) &&
             scene.SetVelocity(body, {0.5f, -1.0f, 0.0f}) &&
             scene.Step(0.02f, 4) == HE3D::PhysicsStepResult::Completed &&
             body.position.y > 0.49f && scene.GetVelocity(body).lengthSq() < 100.0f,
         "solver applies finite position correction and impulses");
}

int main()
{
   MeshContactsSeparateGapFromPenetration();
   StaticTriangleDiscoveryScansPastWorkspacePrefix();
   DynamicSatAndConvexCachesGenerateContacts();
   ConvexColliderAccessExposesIndexedParts();
   PipelineRejectsInvalidOutputsAndStaticPairs();
   ConvexManifoldAndSolverRemainFinite();

   if (g_failures == 0) {
      std::printf("he3d_internal_physics_tests passed\n");
   }
   return g_failures == 0 ? 0 : 1;
}
