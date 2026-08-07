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

static HE3D::int32_t AppendCube(HE3D::float3 *output, HE3D::int32_t count, HE3D::float3 center)
{
   HE3D::Mesh          cube     = HE3D::Mesh::CreateCube();
   const HE3D::float3 *vertices = cube.GetVertices();
   for (HE3D::int32_t i = 0; i < cube.GetVertexCount(); i++)
      output[count + i] = vertices[i] + center;
   return count + cube.GetVertexCount();
}

static HE3D::Mesh CreateMultipartMesh()
{
   HE3D::float3  vertices[108];
   HE3D::int32_t count = 0;
   count               = AppendCube(vertices, count, {-2.0f, 0.0f, 0.0f});
   count               = AppendCube(vertices, count, {0.0f, 0.0f, 0.0f});
   count               = AppendCube(vertices, count, {2.0f, 0.0f, 0.0f});
   return HE3D::Mesh::Create(vertices, count);
}

static HE3D::Mesh CreateUGrooveMesh()
{
   HE3D::float3  vertices[162];
   HE3D::int32_t count           = 0;
   const float   rectangles[][4] = {{-1.5f, -1.5f, -0.5f, -0.5f},
                                    {-0.5f, -1.5f, 0.5f, -0.5f},
                                    {0.5f, -1.5f, 1.5f, -0.5f},
                                    {-1.5f, -0.5f, -0.5f, 1.5f},
                                    {0.5f, -0.5f, 1.5f, 1.5f}};
   for (HE3D::int32_t rectangle = 0; rectangle < 5; rectangle++) {
      HE3D::float3 a    = {rectangles[rectangle][0], rectangles[rectangle][1], 0.5f};
      HE3D::float3 b    = {rectangles[rectangle][2], rectangles[rectangle][1], 0.5f};
      HE3D::float3 c    = {rectangles[rectangle][2], rectangles[rectangle][3], 0.5f};
      HE3D::float3 d    = {rectangles[rectangle][0], rectangles[rectangle][3], 0.5f};
      vertices[count++] = a;
      vertices[count++] = b;
      vertices[count++] = c;
      vertices[count++] = a;
      vertices[count++] = c;
      vertices[count++] = d;
      a.z = b.z = c.z = d.z = -0.5f;
      vertices[count++]     = a;
      vertices[count++]     = c;
      vertices[count++]     = b;
      vertices[count++]     = a;
      vertices[count++]     = d;
      vertices[count++]     = c;
   }
   const HE3D::float3 boundary[] = {
       {-1.5f, -1.5f, 0.0f}, {-0.5f, -1.5f, 0.0f}, {0.5f, -1.5f, 0.0f}, {1.5f, -1.5f, 0.0f},
       {1.5f, -0.5f, 0.0f},  {1.5f, 1.5f, 0.0f},   {0.5f, 1.5f, 0.0f},  {0.5f, -0.5f, 0.0f},
       {-0.5f, -0.5f, 0.0f}, {-0.5f, 1.5f, 0.0f},  {-1.5f, 1.5f, 0.0f}, {-1.5f, -0.5f, 0.0f}};
   for (HE3D::int32_t edge = 0; edge < 12; edge++) {
      HE3D::float3 a = boundary[edge], b = boundary[(edge + 1) % 12];
      a.z = b.z      = -0.5f;
      HE3D::float3 c = a, d = b;
      c.z = d.z         = 0.5f;
      vertices[count++] = a;
      vertices[count++] = b;
      vertices[count++] = d;
      vertices[count++] = a;
      vertices[count++] = d;
      vertices[count++] = c;
   }
   return HE3D::Mesh::Create(vertices, count);
}

static void MultipartPublicContactsTraverseAllParts()
{
   HE3D::Mesh            multipartMesh = CreateMultipartMesh();
   HE3D::Mesh            probeMesh     = HE3D::Mesh::CreateCube();
   HE3D::GameObject      multipartObject(multipartMesh);
   HE3D::GameObject      probeObject(probeMesh);
   HE3D::ConvexCollider  multipart(multipartMesh, HE3D::ConvexBuildMode::ConvexDecomposition);
   HE3D::BoxCollider     probe(1.0f, 1.0f, 1.0f);
   HE3D::PhysicsMaterial material;
   HE3D::PhysicsScene    scene(2);
   probeObject.position = {2.0f, 0.0f, 0.0f};
   Check(multipart.GetPartCount() == 3 &&
             scene.AddStaticBody(multipartObject, multipart, material) &&
             scene.AddKinematicBody(probeObject, probe, material),
         "multipart public contact fixture registers all decomposition parts");
   HE3D::PhysicsContact first[16];
   HE3D::PhysicsContact second[16];
   HE3D::int32_t        firstCount  = scene.GetContacts(probeObject, first, 16);
   HE3D::int32_t        secondCount = scene.GetContacts(probeObject, second, 16);
   Check(firstCount > 0 && firstCount == secondCount,
         "multipart public contact query traverses the nonzero indexed part deterministically");
   for (HE3D::int32_t i = 0; i < firstCount; i++) {
      Check(first[i].other == &multipartObject && first[i].penetration > 0.0f,
            "multipart public contact retains geometric patch ownership");
      Check(first[i].point.x == second[i].point.x && first[i].point.y == second[i].point.y &&
                first[i].point.z == second[i].point.z && first[i].normal.x == second[i].normal.x &&
                first[i].normal.y == second[i].normal.y && first[i].normal.z == second[i].normal.z,
            "multipart public contact ordering is stable across repeated queries");
   }
}

static void MultipartPhysicsStepsAllMotionKinds()
{
   HE3D::Mesh              multipartMesh = CreateMultipartMesh();
   HE3D::Mesh              cubeMesh      = HE3D::Mesh::CreateCube();
   HE3D::GameObject        staticObject(multipartMesh);
   HE3D::GameObject        kinematicObject(cubeMesh);
   HE3D::GameObject        dynamicObject(cubeMesh);
   HE3D::ConvexCollider    multipart(multipartMesh, HE3D::ConvexBuildMode::ConvexDecomposition);
   HE3D::BoxCollider       box(1.0f, 1.0f, 1.0f);
   HE3D::PhysicsMaterial   material;
   HE3D::PhysicsProperties properties;
   HE3D::PhysicsScene      scene(3);
   scene.SetGravity({0.0f, 0.0f, 0.0f});
   scene.SetDrag(0.0f);
   kinematicObject.position = {2.0f, 0.0f, 0.0f};
   dynamicObject.position   = {-2.0f, 0.0f, 0.0f};
   Check(scene.AddStaticBody(staticObject, multipart, material) &&
             scene.AddKinematicBody(kinematicObject, box, material) &&
             scene.AddDynamicBody(dynamicObject, box, properties) &&
             scene.SetVelocity(kinematicObject, {0.1f, 0.0f, 0.0f}) &&
             scene.Step(0.01f, 2) == HE3D::PhysicsStepResult::Completed,
         "multipart static collider steps with dynamic and kinematic bodies without "
         "allocation-visible failure");
}

static void UGrooveDoesNotReportCavityContact()
{
   HE3D::Mesh            grooveMesh = CreateUGrooveMesh();
   HE3D::Mesh            probeMesh  = HE3D::Mesh::CreateCube();
   HE3D::GameObject      grooveObject(grooveMesh);
   HE3D::GameObject      probeObject(probeMesh);
   HE3D::ConvexCollider  groove(grooveMesh, HE3D::ConvexBuildMode::ConvexDecomposition);
   HE3D::BoxCollider     probe(0.5f, 0.5f, 0.5f);
   HE3D::PhysicsMaterial material;
   HE3D::PhysicsScene    scene(2);
   probeObject.position = {0.0f, 0.25f, 0.0f};
   Check(scene.AddStaticBody(grooveObject, groove, material) &&
             scene.AddKinematicBody(probeObject, probe, material),
         "U-groove cavity fixture registers multipart collider");
   HE3D::PhysicsContact contacts[16];
   Check(scene.GetContacts(probeObject, contacts, 16) == 0,
         "U-groove cavity does not report sibling interior contact");
   probeObject.position = {-1.0f, 0.75f, 0.0f};
   Check(scene.GetContacts(probeObject, contacts, 16) > 0,
         "U-groove solid wall reports multipart contact");
}

int main()
{
   MultipartPublicContactsTraverseAllParts();
   MultipartPhysicsStepsAllMotionKinds();
   UGrooveDoesNotReportCavityContact();
   if (g_failures == 0) std::printf("he3d_physics_edge_tests passed\n");
   return g_failures == 0 ? 0 : 1;
}
