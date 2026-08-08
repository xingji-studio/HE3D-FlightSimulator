#include "he3d.hpp"

static bool g_keys[256];
static bool g_quit = false;

static void OnKey(HE3D::int32_t key, bool pressed, void *)
{
   if (key >= 'A' && key <= 'Z') key = key - 'A' + 'a';
   if (key >= 0 && key < 256) g_keys[key] = pressed;
   if (key == 27 && pressed) g_quit = true;
}

static void ResetCube(HE3D::PhysicsScene &scene, HE3D::GameObject &cube)
{
   cube.position    = {0.0f, 3.0f, 4.0f};
   cube.orientation = HE3D::quat();
   scene.SetVelocity(cube, {1.0f, 0.0f, 0.0f});
   scene.SetAngularVelocity(cube, {0.0f, 0.0f, 0.0f});
}

static bool IsGrounded(const HE3D::PhysicsScene &scene, const HE3D::GameObject &cube,
                       const HE3D::GameObject &floor)
{
   HE3D::AABB cubeBounds(cube.position - HE3D::float3(0.5f, 0.5f, 0.5f),
                         cube.position + HE3D::float3(0.5f, 0.5f, 0.5f));
   HE3D::AABB floorBounds(floor.position - HE3D::float3(3.5f, 0.1f, 3.5f),
                          floor.position + HE3D::float3(3.5f, 0.1f, 3.5f));
   bool       overlapsX = cubeBounds.max.x >= floorBounds.min.x - 0.02f &&
                          cubeBounds.min.x <= floorBounds.max.x + 0.02f;
   bool       overlapsZ = cubeBounds.max.z >= floorBounds.min.z - 0.02f &&
                          cubeBounds.min.z <= floorBounds.max.z + 0.02f;
   bool       nearFloor = cubeBounds.min.y <= floorBounds.max.y + 0.06f &&
                          cubeBounds.min.y >= floorBounds.max.y - 0.35f;
   return overlapsX && overlapsZ && nearFloor && scene.GetVelocity(cube).y <= 0.25f;
}

static void ApplyInput(HE3D::PhysicsScene &scene, HE3D::GameObject &cube, bool grounded,
                       bool &jumpWasDown)
{
   if (g_keys['a']) {
      scene.AddForce(cube, {-14.0f, 0.0f, 0.0f});
      scene.AddTorque(cube, {0.0f, 0.0f, 4.0f});
   }
   if (g_keys['d']) {
      scene.AddForce(cube, {14.0f, 0.0f, 0.0f});
      scene.AddTorque(cube, {0.0f, 0.0f, -4.0f});
   }

   bool jumpDown = g_keys[' '];
   if (jumpDown && !jumpWasDown && grounded) {
      scene.AddImpulse(cube, {0.0f, 5.0f, 0.0f});
   }
   jumpWasDown = jumpDown;
}

int main()
{
   HE3D::WindowDesc windowDesc;
   windowDesc.width  = 640;
   windowDesc.height = 360;
   windowDesc.title  = "HE3D PhysicsTest";
   windowDesc.flags  = 0;

   HE3D::Window *window = HE3D::CreateWindow(&windowDesc);
   if (!window) return 1;
   HE3D::SetKeyCallback(window, OnKey, nullptr);
   HE3D::SetFrameRateLimit(260);

   HE3D::Renderer renderer(window, windowDesc.width, windowDesc.height);
   HE3D::Camera   camera;
   camera.position = {0.0f, 1.4f, -5.0f};
   camera.fov      = 50.0f;

   HE3D::Mesh floorMesh = HE3D::Mesh::CreateCube(7.0f, 0.2f, 7.0f);
   HE3D::Mesh cubeMesh  = HE3D::Mesh::CreateCube(1.0f, 1.0f, 1.0f);
   if (!floorMesh.IsValid() || !cubeMesh.IsValid()) {
      HE3D::DestroyWindow(window);
      return 1;
   }

   HE3D::GameObject floor(floorMesh);
   floor.position = {0.0f, -0.6f, 4.0f};
   HE3D::GameObject cube(cubeMesh);

   HE3D::MeshCollider      floorCollider(floorMesh);
   HE3D::BoxCollider       cubeCollider(1.0f, 1.0f, 1.0f);
   HE3D::PhysicsProperties cubeProperties;
   cubeProperties.SetMass(1.0f);
   cubeProperties.SetInertia(cubeCollider.EstimateInertia(cubeProperties.GetMass()));
   cubeProperties.SetFriction(0.45f);
   cubeProperties.SetRestitution(0.5f);
   cubeProperties.SetDamping(0.05f);

   HE3D::PhysicsScene physics;
   if (!physics.AddDynamicBody(cube, cubeCollider, cubeProperties) ||
       !physics.AddStaticBody(floor, floorCollider)) {
      HE3D::DestroyWindow(window);
      return 1;
   }
   ResetCube(physics, cube);

   bool   fxaaWasDown = false;
   bool   jumpWasDown = false;
   bool   grounded    = false;
   double lastTime    = HE3D::TimeSeconds();

   while (!g_quit && !HE3D::WindowShouldClose(window)) {
      double frameStart = HE3D::TimeSeconds();
      HE3D::PollEvents(window);

      double now       = HE3D::TimeSeconds();
      float  deltaTime = static_cast<float>(now - lastTime);
      lastTime         = now;
      if (deltaTime > 0.05f) deltaTime = 0.05f;

      bool fxaaDown = g_keys['f'];
      if (fxaaDown && !fxaaWasDown) HE3D::SetFxaaEnabled(!HE3D::IsFxaaEnabled());
      fxaaWasDown = fxaaDown;

      if (g_keys['r']) {
         ResetCube(physics, cube);
         grounded = false;
      }

      ApplyInput(physics, cube, grounded, jumpWasDown);
      physics.Step(deltaTime, 4);
      grounded = IsGrounded(physics, cube, floor);
      if (cube.position.y < -5.0f) {
         ResetCube(physics, cube);
         grounded = false;
      }

      renderer.Clear({0.08f, 0.10f, 0.13f});
      renderer.DrawGameObject(floor, camera, HE3D::color3(0.25f, 0.55f, 0.35f));
      renderer.DrawGameObject(cube, camera,
                              grounded ? HE3D::color3(0.95f, 0.75f, 0.25f)
                                       : HE3D::color3(0.35f, 0.65f, 1.0f));
      renderer.Present();
      HE3D::PaceFrame(frameStart);
   }

   HE3D::DestroyWindow(window);
   return 0;
}
