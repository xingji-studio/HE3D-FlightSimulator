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

static bool Near(float a, float b, float tolerance)
{
   float d = a - b;
   if (d < 0.0f) d = -d;
   return d <= tolerance;
}

static HE3D::Mesh *CreateBoxMesh() { return HE3D::Mesh::CreateCube(1.0f, 1.0f, 1.0f); }

static float SlopedHeightSample(float x, float z, void *user)
{
   float offset = user ? *static_cast<float *>(user) : 0.0f;
   return offset + x * 0.5f + z * 0.25f;
}

static void PlatformStateFunctionsClampAndReportValues()
{
   HE3D::SetFrameRateLimit(144);
   Check(HE3D::GetFrameRateLimit() == 144,
         "frame-rate limit reports the value that was set");

   HE3D::SetFxaaEnabled(true);
   HE3D::SetTaaEnabled(false);
   HE3D::SetMsaaEnabled(true);
   Check(HE3D::IsFxaaEnabled(), "FXAA state reports enabled");
   Check(!HE3D::IsTaaEnabled(), "TAA state reports disabled");
   Check(HE3D::IsMsaaEnabled(), "MSAA state reports enabled");

   HE3D::SetSsaaScale(0);
   Check(HE3D::GetSsaaScale() == 1, "SSAA scale clamps low values to 1");
   HE3D::SetSsaaScale(99);
   Check(HE3D::GetSsaaScale() == 4, "SSAA scale clamps high values to 4");
}

static void ResourceLoadersCreateMeshFromObjFile()
{
   const char *path = "/tmp/he3d_public_api_triangle.obj";
   FILE       *file = std::fopen(path, "wb");
   Check(file != nullptr, "public API test can create temporary OBJ file");
   if (!file) {
      return;
   }

   const char obj[] =
       "v 0 0 0\n"
       "v 1 0 0\n"
       "v 0 1 0\n"
       "vt 0 0\n"
       "vt 1 0\n"
       "vt 0 1\n"
       "f 1/1 2/2 3/3\n";
   std::fwrite(obj, 1, sizeof(obj) - 1, file);
   std::fclose(file);

   HE3D::Mesh *mesh = HE3D::Mesh::LoadOBJ(path);
   Check(mesh != nullptr, "Mesh::LoadOBJ loads a simple triangle OBJ");
   if (mesh) {
      Check(mesh->vertCount == 3, "OBJ triangle expands to three vertices");
      Check(Near(mesh->vertices[1].x, 1.0f, 0.0001f),
            "OBJ loader preserves vertex positions");
      Check(Near(mesh->uvs[2].y, 0.0f, 0.0001f),
            "OBJ loader converts OBJ V to HE3D texture-space V");
      delete mesh;
   }

   std::remove(path);
}

static void TextureSamplingWrapsUvsAndReportsInvalidTexture()
{
   HE3D::Texture invalid;
   HE3D::color3  invalidSample = invalid.Sample(0.0f, 0.0f);
   Check(Near(invalidSample.r, 1.0f, 0.0001f) &&
             Near(invalidSample.g, 0.0f, 0.0001f) &&
             Near(invalidSample.b, 1.0f, 0.0001f),
         "invalid texture samples as magenta");

   HE3D::Texture texture;
   texture.width  = 2;
   texture.height = 2;
   texture.valid  = true;
   texture.pixels = new HE3D::ColorA[4];
   texture.pixels[0] = {255, 0, 0, 255};
   texture.pixels[1] = {0, 255, 0, 255};
   texture.pixels[2] = {0, 0, 255, 255};
   texture.pixels[3] = {255, 255, 255, 255};

   HE3D::color3 wrapped = texture.Sample(1.25f, -0.25f);
   Check(Near(wrapped.r, 0.0f, 0.0001f) && Near(wrapped.g, 0.0f, 0.0001f) &&
             Near(wrapped.b, 1.0f, 0.0001f),
         "Texture::Sample wraps UVs before nearest-neighbor lookup");
}

static void MathAndPrimitiveSmokeTests()
{
   HE3D::float3 x = {1.0f, 0.0f, 0.0f};
   HE3D::float3 y = {0.0f, 1.0f, 0.0f};
   HE3D::float3 z = HE3D::float3::cross(x, y);
   Check(Near(z.z, 1.0f, 0.0001f), "float3 cross product uses right-handed basis");
   Check(Near(HE3D::fracf(-0.25f), 0.75f, 0.0001f), "fracf wraps negative fractions");
   HE3D::quat   yaw     = HE3D::quat::FromEuler({0.0f, HE3D::HE3D_PI_DIV_2, 0.0f});
   HE3D::float3 forward = yaw.rotate({0.0f, 0.0f, 1.0f});
   Check(Near(forward.x, 1.0f, 0.01f) && Near(forward.z, 0.0f, 0.01f),
         "quat yaw rotates local forward toward +X");

   HE3D::Mesh *plane = HE3D::Mesh::CreatePlane(4.0f, 5.0f);
   HE3D::Mesh *cube  = HE3D::Mesh::CreateCube(2.0f, 3.0f, 4.0f);
   HE3D::Mesh *sphere = HE3D::Mesh::CreateSphere(1.0f, 3, 2);
   Check(plane && plane->vertCount == 6, "plane primitive has two triangles");
   Check(cube && cube->vertCount == 36, "cube primitive has twelve triangles");
   Check(sphere && sphere->vertCount == 36, "sphere primitive uses segments * rings * 6 vertices");
   HE3D::Mesh *invalid       = HE3D::Mesh::CreateCube(1.0f, 0.0f, 1.0f);
   HE3D::Mesh *clampedSphere = HE3D::Mesh::CreateSphere(1.0f, 1, 1);
   Check(invalid == nullptr, "primitive factories reject non-positive dimensions");
   Check(clampedSphere && clampedSphere->vertCount == 36,
         "sphere primitive clamps low segment and ring counts");
   delete plane;
   delete cube;
   delete sphere;
   delete clampedSphere;
}

static void RendererDrawsValidObjectAcrossResize()
{
   HE3D::WindowDesc desc;
   desc.width  = 64;
   desc.height = 48;
   desc.title  = "HE3D public API test";
   desc.flags  = 0;

   HE3D::Window *window = HE3D::CreateWindow(&desc);
   Check(window != nullptr, "CreateWindow returns a test window");
   if (!window) {
      return;
   }

   HE3D::Renderer renderer(window, desc.width, desc.height);
   HE3D::Camera   camera;
   camera.position = {0.0f, 0.0f, 0.0f};
   camera.fov      = 70.0f;

   HE3D::GameObject object;
   object.mesh     = HE3D::Mesh::CreateTriangle(1.0f, 1.0f);
   object.position = {0.0f, 0.0f, 3.0f};
   Check(object.mesh != nullptr, "renderer test mesh is created");

   if (object.mesh) {
      renderer.Clear({0.0f, 0.0f, 0.0f});
      renderer.DrawGameObject(object, camera, HE3D::color3(1.0f, 0.0f, 0.0f));
      renderer.Resize(80, 60);
      renderer.Clear({0.0f, 0.0f, 0.0f});
      renderer.DrawGameObject(object, camera, HE3D::color3(0.0f, 1.0f, 0.0f));
   }

   delete object.mesh;
   HE3D::DestroyWindow(window);
}

static void NewColliderApiValidatesShapes()
{
   HE3D::BoxCollider box;
   Check(!box.IsValid(), "default BoxCollider is invalid");
   Check(box.SetSize(2.0f, 4.0f, 6.0f), "BoxCollider accepts positive dimensions");
   HE3D::AABB boxBounds = box.LocalAABB();
   Check(Near(boxBounds.min.x, -1.0f, 0.0001f) && Near(boxBounds.max.y, 2.0f, 0.0001f) &&
             Near(boxBounds.max.z, 3.0f, 0.0001f),
         "BoxCollider exposes local bounds");
   Check(!box.SetSize(1.0f, 0.0f, 1.0f), "BoxCollider rejects zero dimensions");

   HE3D::Mesh          *cube = CreateBoxMesh();
   HE3D::ConvexCollider convex;
   Check(convex.BuildFromMesh(cube), "ConvexCollider builds from a closed convex mesh");
   Check(convex.IsValid(), "ConvexCollider reports valid after build");

   HE3D::float3 badVertices[3] = {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}};
   HE3D::float2 badUvs[3]      = {{0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}};
   HE3D::Mesh  *triangle       = HE3D::Mesh::Create(badVertices, badUvs, 3);
   HE3D::ConvexCollider invalidConvex;
   Check(!invalidConvex.BuildFromMesh(triangle),
         "ConvexCollider rejects open or degenerate meshes");

   HE3D::StaticMeshCollider staticMesh;
   Check(staticMesh.BuildFromMesh(triangle), "StaticMeshCollider accepts open triangle geometry");
   Check(staticMesh.IsValid(), "StaticMeshCollider reports valid after build");

   delete cube;
   delete triangle;
}

static void HeightFieldColliderBuildsTilesAndSamplesNormals()
{
   HE3D::HeightFieldCollider       invalid;
   HE3D::HeightFieldCollider::Tile tile;
   Check(!invalid.IsValid(), "default HeightFieldCollider is invalid");
   Check(!invalid.BuildTile(tile, 0.0f, 0.0f), "invalid HeightFieldCollider does not build a tile");

   float                     offset = 2.0f;
   HE3D::HeightFieldCollider heightfield(SlopedHeightSample, &offset, 4, 1.0f);
   Check(heightfield.IsValid(), "configured HeightFieldCollider is valid");
   Check(heightfield.BuildTile(tile, 10.0f, -4.0f), "HeightFieldCollider builds tile bounds");
   Check(tile.active && tile.cellCount == 9, "HeightFieldCollider caches one bound per cell");
   Check(tile.bounds.max.y > tile.bounds.min.y,
         "HeightFieldCollider tile bounds include sampled heights");

   int previousCapacity = tile.cellCapacity;
   Check(tile.EnsureCapacity(4), "HeightFieldCollider tile reuses capacity");
   Check(tile.cellCapacity == previousCapacity, "EnsureCapacity keeps existing larger allocation");

   HE3D::float3 normal = heightfield.NormalAt(1.0f, 2.0f);
   Check(normal.y > 0.7f && normal.lengthSq() > 0.9f,
         "HeightFieldCollider samples an upward terrain normal");

   heightfield.Configure(nullptr, nullptr, 0, 0.0f);
   Check(!heightfield.IsValid(), "HeightFieldCollider can be reconfigured invalid");
}

static void PhysicsBodyDefaultsAndForces()
{
   HE3D::PhysicsBody body;
   Check(!body.IsEnabled(), "default PhysicsBody starts disabled");
   Check(Near(body.mass, 1.0f, 0.0001f), "default PhysicsBody has unit mass");
   body.SetMass(2.0f);
   Check(Near(body.inverseMass, 0.5f, 0.0001f), "SetMass updates inverse mass");
   body.SetInertia(4.0f);
   Check(Near(body.inverseInertia, 0.25f, 0.0001f), "SetInertia updates inverse inertia");
   Check(body.UsesManualInertia(), "SetInertia marks inertia as manual");
   body.BindGameObject(nullptr);
   body.SetMass(0.0f);
   Check(body.inverseMass == 0.0f && !body.UsesManualInertia(),
         "non-positive SetMass clears inverse mass and manual inertia");
   body.SetInertia(0.0f);
   Check(body.inverseInertia == 0.0f, "non-positive SetInertia clears inverse inertia");
   body.SetMass(2.0f);
   body.AddForce({1.0f, 2.0f, 3.0f});
   body.AddTorque({4.0f, 5.0f, 6.0f});
   body.AddImpulse({2.0f, 0.0f, 0.0f});
   body.AddAngularImpulse({0.0f, 2.0f, 0.0f});
   Check(body.velocity.x > 0.9f && body.angularVelocity.y == 0.0f,
         "immediate impulses use inverse mass and inverse inertia");
   body.ClearForces();
   body.ClearTorques();
   Check(body.force.lengthSq() == 0.0f && body.torque.lengthSq() == 0.0f,
         "PhysicsBody clears force and torque accumulators");
}

static void PhysicsSceneRejectsInvalidEntriesAndReusesCapacity()
{
   HE3D::GameObject   object;
   HE3D::BoxCollider  box(1.0f, 1.0f, 1.0f);
   HE3D::BoxCollider  invalidBox;
   HE3D::PhysicsBody  body;
   HE3D::PhysicsScene scene(1);

   Check(!scene.AddBody(nullptr, &body, &box), "PhysicsScene rejects null object");
   Check(!scene.AddBody(&object, nullptr, &box), "PhysicsScene rejects null body");
   Check(!scene.AddBody(&object, &body, static_cast<HE3D::BoxCollider *>(nullptr)),
         "PhysicsScene rejects null dynamic collider");
   Check(!scene.AddBody(&object, &body, &invalidBox),
         "PhysicsScene rejects invalid dynamic collider");
   HE3D::StaticMeshCollider invalidStatic;
   Check(!scene.AddStatic(nullptr, &invalidStatic), "PhysicsScene rejects null static object");
   Check(!scene.AddStatic(&object, nullptr), "PhysicsScene rejects null static collider");
   Check(!scene.AddStatic(&object, &invalidStatic), "PhysicsScene rejects invalid static collider");

   Check(scene.AddBody(&object, &body, &box), "PhysicsScene accepts valid dynamic entry");
   HE3D::PhysicsBody extraBody;
   Check(!scene.AddBody(&object, &extraBody, &box), "PhysicsScene enforces entry capacity");
   scene.Clear();
   Check(scene.AddBody(&object, &extraBody, &box), "PhysicsScene::Clear frees capacity for reuse");
}

static void PhysicsSceneIntegratesAndClearsForces()
{
   HE3D::GameObject  object;
   HE3D::BoxCollider box(1.0f, 1.0f, 1.0f);
   HE3D::PhysicsBody body;
   body.SetEnabled(true);
   body.gravity = {0.0f, 0.0f, 0.0f};
   body.AddForce({10.0f, 0.0f, 0.0f});
   body.AddTorque({0.0f, 1.0f, 0.0f});

   HE3D::PhysicsScene scene;
   scene.drag = 0.0f;
   Check(scene.AddBody(&object, &body, &box), "PhysicsScene accepts body for integration");
   scene.Step(0.1f, 1);
   Check(object.position.x > 0.09f, "PhysicsScene integrates linear force");
   Check(object.orientation.y > 0.0f, "PhysicsScene integrates torque");
   Check(body.force.lengthSq() == 0.0f && body.torque.lengthSq() == 0.0f,
         "PhysicsScene::Step clears accumulators");
}

static void PhysicsSceneNonPositiveStepDoesNotAdvanceOrClear()
{
   HE3D::GameObject  object;
   HE3D::BoxCollider box(1.0f, 1.0f, 1.0f);
   HE3D::PhysicsBody body;
   body.SetEnabled(true);
   body.gravity = {0.0f, 0.0f, 0.0f};
   body.SetVelocity({1.0f, 0.0f, 0.0f});
   body.AddForce({4.0f, 0.0f, 0.0f});
   body.AddTorque({0.0f, 3.0f, 0.0f});

   HE3D::PhysicsScene scene;
   scene.drag = 0.0f;
   Check(scene.AddBody(&object, &body, &box), "non-positive step scene accepts body");
   scene.Step(0.0f, 4);
   Check(Near(object.position.x, 0.0f, 0.0001f), "zero delta step does not integrate");
   Check(body.force.lengthSq() > 0.0f && body.torque.lengthSq() > 0.0f,
         "zero delta step preserves accumulators");
   scene.Step(-0.1f, 4);
   Check(Near(object.position.x, 0.0f, 0.0001f), "negative delta step does not integrate");
   Check(body.force.lengthSq() > 0.0f && body.torque.lengthSq() > 0.0f,
         "negative delta step preserves accumulators");
}

static void PhysicsSceneAcceptsConvexBodiesAndStaticOnlyScenes()
{
   HE3D::Mesh          *cube = CreateBoxMesh();
   HE3D::ConvexCollider convex(cube);
   HE3D::GameObject     object;
   HE3D::PhysicsBody    body;
   body.SetEnabled(true);
   body.gravity = {0.0f, 0.0f, 0.0f};
   body.SetVelocity({1.0f, 0.0f, 0.0f});

   HE3D::PhysicsScene scene;
   scene.drag = 0.0f;
   Check(scene.AddBody(&object, &body, &convex), "PhysicsScene accepts dynamic convex collider");
   scene.Step(0.1f, 0);
   Check(object.position.x > 0.09f, "PhysicsScene integrates dynamic convex collider entries");

   HE3D::GameObject floorA;
   HE3D::GameObject floorB;
   floorA.mesh = HE3D::Mesh::CreatePlane(2.0f, 2.0f);
   floorB.mesh = HE3D::Mesh::CreatePlane(2.0f, 2.0f);
   HE3D::StaticMeshCollider staticA(floorA.mesh);
   HE3D::StaticMeshCollider staticB(floorB.mesh);
   HE3D::PhysicsScene       staticScene(2);
   Check(staticScene.AddStatic(&floorA, &staticA), "PhysicsScene accepts first static collider");
   Check(staticScene.AddStatic(&floorB, &staticB), "PhysicsScene accepts second static collider");
   staticScene.Step(0.01f, 20);

   delete cube;
   delete floorA.mesh;
   delete floorB.mesh;
}

static void DynamicBoxesCollideThroughSceneStep()
{
   HE3D::GameObject lowerObject;
   lowerObject.position = {0.0f, 0.0f, 0.0f};
   HE3D::GameObject upperObject;
   upperObject.position = {0.0f, 0.95f, 0.0f};
   HE3D::BoxCollider lowerCollider(1.0f, 1.0f, 1.0f);
   HE3D::BoxCollider upperCollider(1.0f, 1.0f, 1.0f);
   HE3D::PhysicsBody lowerBody;
   HE3D::PhysicsBody upperBody;
   lowerBody.SetEnabled(true);
   upperBody.SetEnabled(true);
   lowerBody.gravity = {0.0f, 0.0f, 0.0f};
   upperBody.gravity = {0.0f, 0.0f, 0.0f};
   lowerBody.SetVelocity({0.0f, 0.2f, 0.0f});
   upperBody.SetVelocity({0.0f, -0.2f, 0.0f});

   HE3D::PhysicsScene scene;
   scene.drag = 0.0f;
   Check(scene.AddBody(&lowerObject, &lowerBody, &lowerCollider),
         "dynamic box scene accepts lower body");
   Check(scene.AddBody(&upperObject, &upperBody, &upperCollider),
         "dynamic box scene accepts upper body");
   float initialSeparation = upperObject.position.y - lowerObject.position.y;
   scene.Step(1.0f / 60.0f, 4);
   float finalSeparation = upperObject.position.y - lowerObject.position.y;
   Check(finalSeparation > initialSeparation, "dynamic boxes receive positional correction");
   Check(lowerBody.velocity.y <= 0.2f && upperBody.velocity.y >= -0.2f,
         "dynamic boxes receive velocity response");
}

static void ConvexBodiesCollideThroughSceneStep()
{
   HE3D::Mesh          *lowerMesh = CreateBoxMesh();
   HE3D::Mesh          *upperMesh = CreateBoxMesh();
   HE3D::ConvexCollider lowerCollider(lowerMesh);
   HE3D::ConvexCollider upperCollider(upperMesh);
   HE3D::GameObject     lowerObject;
   HE3D::GameObject     upperObject;
   upperObject.position = {0.0f, 0.95f, 0.0f};
   HE3D::PhysicsBody lowerBody;
   HE3D::PhysicsBody upperBody;
   lowerBody.SetEnabled(true);
   upperBody.SetEnabled(true);
   lowerBody.gravity = {0.0f, 0.0f, 0.0f};
   upperBody.gravity = {0.0f, 0.0f, 0.0f};

   HE3D::PhysicsScene scene;
   scene.drag = 0.0f;
   Check(scene.AddBody(&lowerObject, &lowerBody, &lowerCollider),
         "convex scene accepts lower body");
   Check(scene.AddBody(&upperObject, &upperBody, &upperCollider),
         "convex scene accepts upper body");
   float initialSeparation = upperObject.position.y - lowerObject.position.y;
   scene.Step(1.0f / 60.0f, 4);
   Check(upperObject.position.y - lowerObject.position.y > initialSeparation,
         "convex bodies collide through PhysicsScene::Step");

   delete lowerMesh;
   delete upperMesh;
}

static void StaticMeshSupportsAndReleasesDynamicBox()
{
   HE3D::GameObject floorObject;
   floorObject.mesh = HE3D::Mesh::CreatePlane(4.0f, 4.0f);
   HE3D::StaticMeshCollider floorCollider(floorObject.mesh);

   HE3D::GameObject boxObject;
   boxObject.position = {0.0f, 0.49f, 0.0f};
   HE3D::BoxCollider boxCollider(1.0f, 1.0f, 1.0f);
   HE3D::PhysicsBody body;
   body.SetEnabled(true);
   body.gravity = {0.0f, 0.0f, 0.0f};

   HE3D::PhysicsScene scene;
   scene.drag = 0.0f;
   Check(scene.AddBody(&boxObject, &body, &boxCollider),
         "PhysicsScene accepts box body on static mesh");
   Check(scene.AddStatic(&floorObject, &floorCollider),
         "PhysicsScene accepts static mesh collider");
   float initialY = boxObject.position.y;
   scene.Step(1.0f / 60.0f, 4);
   Check(boxObject.position.y > initialY, "static mesh corrects real penetration");

   HE3D::GameObject outsideBox;
   outsideBox.position = {3.0f, 1.0f, 0.0f};
   HE3D::PhysicsBody fallingBody;
   fallingBody.SetEnabled(true);
   HE3D::PhysicsScene fallScene;
   fallScene.drag = 0.0f;
   Check(fallScene.AddBody(&outsideBox, &fallingBody, &boxCollider),
         "PhysicsScene accepts body outside platform");
   Check(fallScene.AddStatic(&floorObject, &floorCollider),
         "PhysicsScene accepts static platform for fall test");
   float fallInitialY = outsideBox.position.y;
   fallScene.Step(0.25f, 4);
   Check(outsideBox.position.y < fallInitialY, "body outside static platform falls under gravity");

   delete floorObject.mesh;
}

static void DampingDragAndInertiaStayStable()
{
   HE3D::GameObject  object;
   HE3D::BoxCollider box(2.0f, 4.0f, 6.0f);
   HE3D::PhysicsBody body;
   body.SetEnabled(true);
   body.SetMass(3.0f);
   body.gravity = {0.0f, 0.0f, 0.0f};
   body.SetVelocity({1.0f, 0.0f, 0.0f});
   body.SetAngularVelocity({0.0f, 1.0f, 0.0f});
   body.damping = 1.0f;

   HE3D::PhysicsScene scene;
   scene.drag = 1.0f;
   Check(scene.AddBody(&object, &body, &box), "PhysicsScene accepts body for damping and inertia");
   Check(body.inertia > 0.0f && body.inverseInertia > 0.0f,
         "PhysicsScene estimates finite box inertia");
   scene.Step(0.25f, 1);
   Check(body.velocity.x < 1.0f && body.angularVelocity.y < 1.0f,
         "body damping and scene drag reduce velocities");

   body.SetVelocity({1.0f, 0.0f, 0.0f});
   body.SetAngularVelocity({0.0f, 1.0f, 0.0f});
   body.damping = 20.0f;
   scene.drag   = 20.0f;
   scene.Step(1.0f, 1);
   Check(body.velocity.lengthSq() == 0.0f && body.angularVelocity.lengthSq() == 0.0f,
         "body damping and scene drag clamp velocity to zero");
}

int main()
{
   PlatformStateFunctionsClampAndReportValues();
   ResourceLoadersCreateMeshFromObjFile();
   TextureSamplingWrapsUvsAndReportsInvalidTexture();
   MathAndPrimitiveSmokeTests();
   RendererDrawsValidObjectAcrossResize();
   NewColliderApiValidatesShapes();
   HeightFieldColliderBuildsTilesAndSamplesNormals();
   PhysicsBodyDefaultsAndForces();
   PhysicsSceneRejectsInvalidEntriesAndReusesCapacity();
   PhysicsSceneIntegratesAndClearsForces();
   PhysicsSceneNonPositiveStepDoesNotAdvanceOrClear();
   PhysicsSceneAcceptsConvexBodiesAndStaticOnlyScenes();
   DynamicBoxesCollideThroughSceneStep();
   ConvexBodiesCollideThroughSceneStep();
   StaticMeshSupportsAndReleasesDynamicBox();
   DampingDragAndInertiaStayStable();

   if (g_failures != 0) {
      std::printf("%d HE3D public API test(s) failed\n", g_failures);
      return 1;
   }

   std::printf("HE3D public API tests passed\n");
   return 0;
}
