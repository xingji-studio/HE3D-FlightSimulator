#include "he3d.hpp"

#include <cstdio>

static int g_failures = 0;

static void Check(bool condition, const char *name)
{
   if (!condition) {
      std::printf("FAIL: %s\n", name);
      g_failures++;
   }
}

static void ConvexColliderModeRequiredConstruction()
{
   HE3D::Mesh cubeMesh = HE3D::Mesh::CreateCube(1.0f, 1.0f, 1.0f);
   HE3D::ConvexCollider collider(cubeMesh, HE3D::ConvexBuildMode::SingleHull);
   Check(collider.IsValid(), "mode-required convex collider builds a valid cube");
   Check(collider.GetPartCount() == 1, "mode-required convex collider reports one part");
   HE3D::AABB bounds = collider.LocalAABB();
   Check(bounds.min.x == -0.5f && bounds.max.x == 0.5f,
         "mode-required convex collider exposes cube bounds");
}

static void ConvexColliderOwnershipAndRebuildFailure()
{
   HE3D::Mesh cubeMesh = HE3D::Mesh::CreateCube(1.0f, 1.0f, 1.0f);
   HE3D::ConvexCollider collider(cubeMesh, HE3D::ConvexBuildMode::SingleHull);
   HE3D::AABB before = collider.LocalAABB();
   HE3D::Mesh triangle = HE3D::Mesh::CreateTriangle(1.0f, 1.0f);
   Check(!collider.BuildFromMesh(triangle, HE3D::ConvexBuildMode::SingleHull),
         "invalid rebuild is rejected");
   Check(collider.IsValid() && collider.GetPartCount() == 1 && collider.LocalAABB().min.x == before.min.x &&
             collider.LocalAABB().max.x == before.max.x,
         "failed rebuild preserves existing collider state");
}

static void ConvexColliderInertiaAndRegistration()
{
   HE3D::Mesh cubeMesh = HE3D::Mesh::CreateCube(1.0f, 1.0f, 1.0f);
   HE3D::ConvexCollider collider(cubeMesh, HE3D::ConvexBuildMode::SingleHull);
   HE3D::float3 inertia = collider.EstimateInertia(2.0f);
   Check(inertia.x > 0.0f && inertia.y > 0.0f && inertia.z > 0.0f,
         "single-hull convex inertia is finite and positive");

   HE3D::GameObject cube(cubeMesh);
   HE3D::PhysicsMaterial material;
   HE3D::PhysicsProperties properties;
   HE3D::PhysicsScene scene(2);
   Check(scene.AddStaticBody(cube, collider, material), "convex collider registers as static body");
   Check(scene.RemoveBody(cube), "registered convex collider can be removed");
   Check(scene.AddDynamicBody(cube, collider, properties), "convex collider registers as dynamic body");
}

int main()
{
   ConvexColliderModeRequiredConstruction();
   ConvexColliderOwnershipAndRebuildFailure();
   ConvexColliderInertiaAndRegistration();
   if (g_failures == 0) {
      std::printf("he3d_convex_collider_tests passed\n");
   }
   return g_failures == 0 ? 0 : 1;
}
