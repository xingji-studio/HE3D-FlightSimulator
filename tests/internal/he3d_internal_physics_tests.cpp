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

static bool Near(float a, float b, float tolerance)
{
   float d = a - b;
   if (d < 0.0f) d = -d;
   return d <= tolerance;
}

static HE3D::Mesh *CreateBoxMesh() { return HE3D::Mesh::CreateCube(1.0f, 1.0f, 1.0f); }

static HE3D::Mesh *CreateDegenerateTriangleMesh()
{
   HE3D::float3 vertices[3] = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
   HE3D::float2 uvs[3]      = {{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}};
   return HE3D::Mesh::Create(vertices, uvs, 3);
}

static int CollectBoxStatic(HE3D::GameObject &boxObject, HE3D::BoxCollider &boxCollider,
                            HE3D::GameObject &floorObject, HE3D::StaticMeshCollider &floorCollider,
                            HE3D::InternalContact *contacts, int maxContacts)
{
   HE3D::InternalContactShape boxShape = HE3D::Detail::ColliderAccess::From(boxObject, boxCollider);
   HE3D::InternalContactShape floorShape =
       HE3D::Detail::ColliderAccess::From(floorObject, floorCollider);
   return HE3D::GenerateInternalContacts(boxShape, floorShape, contacts, maxContacts);
}

static void GapDoesNotGenerateSupportContact()
{
   HE3D::GameObject floorObject;
   floorObject.mesh = HE3D::Mesh::CreatePlane(4.0f, 4.0f);
   HE3D::StaticMeshCollider floorCollider(floorObject.mesh);

   HE3D::GameObject boxObject;
   boxObject.position = {0.0f, 0.511f, 0.0f};
   HE3D::BoxCollider boxCollider(1.0f, 1.0f, 1.0f);

   HE3D::InternalContact contacts[4];
   int count = CollectBoxStatic(boxObject, boxCollider, floorObject, floorCollider, contacts, 4);
   Check(count == 0, "0.011 gap does not enter the solver contact set");

   HE3D::PhysicsBody body;
   body.SetEnabled(true);
   body.gravity = {0.0f, 0.0f, 0.0f};
   HE3D::PhysicsScene scene;
   scene.drag = 0.0f;
   Check(scene.AddBody(&boxObject, &body, &boxCollider), "gap scene accepts dynamic box");
   Check(scene.AddStatic(&floorObject, &floorCollider), "gap scene accepts static floor");
   float initialY = boxObject.position.y;
   scene.Step(1.0f / 60.0f, 4);
   Check(boxObject.position.y <= initialY + 0.0001f, "0.011 gap is not pushed upward by solver");

   delete floorObject.mesh;
}

static void PenetrationGeneratesContactAndCorrection()
{
   HE3D::GameObject floorObject;
   floorObject.mesh = HE3D::Mesh::CreatePlane(4.0f, 4.0f);
   HE3D::StaticMeshCollider floorCollider(floorObject.mesh);

   HE3D::GameObject boxObject;
   boxObject.position = {0.0f, 0.49f, 0.0f};
   HE3D::BoxCollider boxCollider(1.0f, 1.0f, 1.0f);

   HE3D::InternalContact contacts[4];
   int count = CollectBoxStatic(boxObject, boxCollider, floorObject, floorCollider, contacts, 4);
   Check(count > 0, "real penetration generates contacts");
   Check(count == 0 || contacts[0].penetration > 0.0f, "generated penetration is positive");

   HE3D::PhysicsBody body;
   body.SetEnabled(true);
   body.gravity = {0.0f, 0.0f, 0.0f};
   HE3D::PhysicsScene scene;
   scene.drag = 0.0f;
   Check(scene.AddBody(&boxObject, &body, &boxCollider), "penetration scene accepts dynamic box");
   Check(scene.AddStatic(&floorObject, &floorCollider), "penetration scene accepts static floor");
   float initialY = boxObject.position.y;
   scene.Step(1.0f / 60.0f, 4);
   Check(boxObject.position.y > initialY, "positive penetration receives upward correction");

   delete floorObject.mesh;
}

static void TiltedStaticPlaneContactKeepsMultiplePoints()
{
   HE3D::GameObject floorObject;
   floorObject.mesh = HE3D::Mesh::CreatePlane(4.0f, 4.0f);
   HE3D::StaticMeshCollider floorCollider(floorObject.mesh);

   HE3D::GameObject boxObject;
   boxObject.position    = {0.0f, 0.46f, 0.0f};
   boxObject.orientation = HE3D::quat::FromEuler({0.0f, 0.0f, 0.18f});
   HE3D::BoxCollider boxCollider(1.0f, 1.0f, 1.0f);

   HE3D::InternalContact contacts[4];
   int count = CollectBoxStatic(boxObject, boxCollider, floorObject, floorCollider, contacts, 4);
   Check(count > 1, "tilted static plane contact keeps multiple points");
   Check(count <= 4, "tilted static plane manifold is capped at four points");

   delete floorObject.mesh;
}

static void StaticContactReductionHonorsRequestedCapacity()
{
   HE3D::GameObject floorObject;
   floorObject.mesh = HE3D::Mesh::CreatePlane(4.0f, 4.0f);
   HE3D::StaticMeshCollider floorCollider(floorObject.mesh);

   HE3D::GameObject boxObject;
   boxObject.position    = {0.0f, 0.44f, 0.0f};
   boxObject.orientation = HE3D::quat::FromEuler({0.21f, 0.0f, 0.19f});
   HE3D::BoxCollider boxCollider(1.0f, 1.0f, 1.0f);

   HE3D::InternalContact contacts[2];
   int count = CollectBoxStatic(boxObject, boxCollider, floorObject, floorCollider, contacts, 2);
   Check(count == 2, "static manifold reduction honors smaller contact capacity");
   Check(contacts[0].penetration > 0.0f && contacts[1].penetration > 0.0f,
         "reduced static manifold keeps positive contacts");

   delete floorObject.mesh;
}

static void TiltedStaticPlaneContactKeepsPerPointPenetration()
{
   HE3D::GameObject floorObject;
   floorObject.mesh = HE3D::Mesh::CreatePlane(4.0f, 4.0f);
   HE3D::StaticMeshCollider floorCollider(floorObject.mesh);

   HE3D::GameObject boxObject;
   boxObject.position    = {0.0f, 0.45f, 0.0f};
   boxObject.orientation = HE3D::quat::FromEuler({0.13f, 0.0f, 0.23f});
   HE3D::BoxCollider boxCollider(1.0f, 1.0f, 1.0f);

   HE3D::InternalContact contacts[4];
   int count = CollectBoxStatic(boxObject, boxCollider, floorObject, floorCollider, contacts, 4);
   Check(count > 1, "per-point penetration test has multiple contacts");

   bool hasDifferentPenetration = false;
   for (int i = 1; i < count; i++) {
      if (!Near(contacts[i].penetration, contacts[0].penetration, 0.0001f)) {
         hasDifferentPenetration = true;
      }
   }
   Check(hasDifferentPenetration, "contact manifold keeps each point's own penetration");

   delete floorObject.mesh;
}

static void DynamicSatKeepsMultiplePointsAndRejectsSeparation()
{
   HE3D::GameObject lowerObject;
   lowerObject.position = {0.0f, 0.0f, 0.0f};
   HE3D::BoxCollider lowerCollider(1.0f, 1.0f, 1.0f);

   HE3D::GameObject upperObject;
   upperObject.position = {0.0f, 0.95f, 0.0f};
   HE3D::BoxCollider upperCollider(1.0f, 1.0f, 1.0f);

   HE3D::InternalContactShape lower = HE3D::Detail::ColliderAccess::From(lowerObject, lowerCollider);
   HE3D::InternalContactShape upper = HE3D::Detail::ColliderAccess::From(upperObject, upperCollider);
   HE3D::InternalContact      contacts[4];
   int                        count = HE3D::GenerateInternalContacts(upper, lower, contacts, 4);
   Check(count > 1, "dynamic SAT contact keeps multiple representative points");
   Check(count <= 4, "dynamic SAT manifold is capped at four points");

   upperObject.position = {0.0f, 3.0f, 0.0f};
   upper                = HE3D::Detail::ColliderAccess::From(upperObject, upperCollider);
   count                = HE3D::GenerateInternalContacts(upper, lower, contacts, 4);
   Check(count == 0, "dynamic SAT contact rejects separated boxes");
}

static void ConvexSatUsesConvexShapeCaches()
{
   HE3D::Mesh          *leftMesh  = CreateBoxMesh();
   HE3D::Mesh          *rightMesh = CreateBoxMesh();
   HE3D::ConvexCollider leftCollider(leftMesh);
   HE3D::ConvexCollider rightCollider(rightMesh);
   HE3D::GameObject     leftObject;
   HE3D::GameObject     rightObject;
   rightObject.position = {0.0f, 0.9f, 0.0f};

   HE3D::InternalContactShape left  = HE3D::Detail::ColliderAccess::From(leftObject, leftCollider);
   HE3D::InternalContactShape right = HE3D::Detail::ColliderAccess::From(rightObject, rightCollider);
   HE3D::InternalContact      contacts[4];
   int                        count = HE3D::GenerateInternalContacts(right, left, contacts, 4);
   Check(count > 0, "convex SAT path generates contacts from ConvexCollider caches");
   Check(count <= 4, "convex SAT path caps contact count");

   rightObject.position = {3.0f, 0.0f, 0.0f};
   right                = HE3D::Detail::ColliderAccess::From(rightObject, rightCollider);
   count                = HE3D::GenerateInternalContacts(right, left, contacts, 4);
   Check(count == 0, "convex SAT path rejects separated convex shapes");

   delete leftMesh;
   delete rightMesh;
}

static void PipelineRejectsInvalidOutputsAndStaticPairs()
{
   HE3D::GameObject floorObject;
   floorObject.mesh = HE3D::Mesh::CreatePlane(2.0f, 2.0f);
   HE3D::StaticMeshCollider   floorCollider(floorObject.mesh);
   HE3D::InternalContactShape floor = HE3D::Detail::ColliderAccess::From(floorObject, floorCollider);

   HE3D::InternalContact contacts[4];
   Check(HE3D::GenerateInternalContacts(floor, floor, contacts, 4) == 0,
         "pipeline skips static-static pairs");

   HE3D::GameObject           boxObject;
   HE3D::BoxCollider          boxCollider(1.0f, 1.0f, 1.0f);
   HE3D::InternalContactShape box = HE3D::Detail::ColliderAccess::From(boxObject, boxCollider);
   Check(HE3D::GenerateInternalContacts(box, floor, nullptr, 4) == 0,
         "pipeline rejects null contact output");
   Check(HE3D::GenerateInternalContacts(box, floor, contacts, 0) == 0,
         "pipeline rejects zero contact capacity");

   delete floorObject.mesh;
}

static void FrictionChangesOnlyTangentVelocityOnRestingContact()
{
   HE3D::GameObject floorObject;
   floorObject.mesh = HE3D::Mesh::CreatePlane(4.0f, 4.0f);
   HE3D::StaticMeshCollider floorCollider(floorObject.mesh);

   HE3D::GameObject boxObject;
   boxObject.position = {0.0f, 0.49f, 0.0f};
   HE3D::BoxCollider boxCollider(1.0f, 1.0f, 1.0f);

   HE3D::InternalContact contacts[4];
   int count = CollectBoxStatic(boxObject, boxCollider, floorObject, floorCollider, contacts, 4);
   HE3D::float3 normal          = count > 0 ? contacts[0].normal : HE3D::float3(0.0f, 1.0f, 0.0f);
   HE3D::float3 initialVelocity = {0.4f, 0.0f, 0.0f};
   initialVelocity = initialVelocity - normal * HE3D::float3::dot(initialVelocity, normal);
   float initialNormalVelocity = HE3D::float3::dot(initialVelocity, normal);

   HE3D::PhysicsBody body;
   body.SetEnabled(true);
   body.SetInertia(0.0f);
   body.gravity  = {0.0f, 0.0f, 0.0f};
   body.friction = 1.0f;
   body.SetVelocity(initialVelocity);

   HE3D::PhysicsScene scene;
   scene.drag = 0.0f;
   Check(scene.AddBody(&boxObject, &body, &boxCollider), "friction scene accepts dynamic box");
   Check(scene.AddStatic(&floorObject, &floorCollider), "friction scene accepts static floor");
   scene.Step(1.0f / 60.0f, 4);
   float finalNormalVelocity = HE3D::float3::dot(body.velocity, normal);
   Check(body.velocity.lengthSq() < initialVelocity.lengthSq(),
         "resting contact friction reduces tangent velocity");
   Check(Near(finalNormalVelocity, initialNormalVelocity, 0.0001f),
         "resting contact friction leaves normal velocity unchanged");

   delete floorObject.mesh;
}

static void ConvexBuildRejectsDegenerateAndAcceptsClosedMesh()
{
   HE3D::Mesh          *boxMesh = CreateBoxMesh();
   HE3D::ConvexCollider convex;
   Check(convex.BuildFromMesh(boxMesh), "ConvexCollider accepts cube mesh");
   Check(convex.m_vertexCount == 8, "ConvexCollider stores unique vertices");
   Check(convex.m_faceAxisCount >= 3, "ConvexCollider stores SAT face axes");
   Check(convex.m_edgeAxisCount >= 3, "ConvexCollider stores edge axes");

   HE3D::Mesh          *degenerate = CreateDegenerateTriangleMesh();
   HE3D::ConvexCollider broken;
   Check(!broken.BuildFromMesh(degenerate), "ConvexCollider rejects degenerate triangle mesh");

   delete boxMesh;
   delete degenerate;
}

static void StaticDegenerateTrianglesDoNotCreateContacts()
{
   HE3D::GameObject staticObject;
   staticObject.mesh = CreateDegenerateTriangleMesh();
   HE3D::StaticMeshCollider staticCollider(staticObject.mesh);

   HE3D::GameObject  boxObject;
   HE3D::BoxCollider boxCollider(1.0f, 1.0f, 1.0f);

   HE3D::InternalContact contacts[4];
   int count = CollectBoxStatic(boxObject, boxCollider, staticObject, staticCollider, contacts, 4);
   Check(count == 0, "degenerate static triangles create no contacts");

   delete staticObject.mesh;
}

int main()
{
   GapDoesNotGenerateSupportContact();
   PenetrationGeneratesContactAndCorrection();
   TiltedStaticPlaneContactKeepsMultiplePoints();
   StaticContactReductionHonorsRequestedCapacity();
   TiltedStaticPlaneContactKeepsPerPointPenetration();
   DynamicSatKeepsMultiplePointsAndRejectsSeparation();
   ConvexSatUsesConvexShapeCaches();
   PipelineRejectsInvalidOutputsAndStaticPairs();
   FrictionChangesOnlyTangentVelocityOnRestingContact();
   ConvexBuildRejectsDegenerateAndAcceptsClosedMesh();
   StaticDegenerateTrianglesDoNotCreateContacts();

   if (g_failures != 0) {
      std::printf("%d HE3D internal physics test(s) failed\n", g_failures);
      return 1;
   }

   std::printf("HE3D internal physics tests passed\n");
   return 0;
}
