/*
 * HE3D PhysicsTest - scene physics and mesh collision example.
 * HE3D PhysicsTest：场景物理和 mesh 碰撞示例。
 *
 * This example shows the intended public physics path:
 * PhysicsBody stores state, BoxCollider/StaticMeshCollider store shapes, and
 * PhysicsScene::Step() is the only function that advances the simulation.
 *
 * 本示例展示推荐的公开物理路径：
 * PhysicsBody 保存状态，BoxCollider/StaticMeshCollider 保存形状，PhysicsScene::Step()
 * 是唯一推进模拟的函数。
 */
#include "he3d.hpp"

static bool g_keys[256];
static bool g_quit = false;

// HE3D sends keyboard events here after PollEvents() is called.
// 调用 PollEvents() 后，HE3D 会把键盘事件发送到这里。
static void OnKey(HE3D::int32_t key, bool pressed, void *)
{
   if (key >= 'A' && key <= 'Z') {
      key = key - 'A' + 'a';
   }
   if (key >= 0 && key < 256) {
      g_keys[key] = pressed;
   }
   if (key == 27 && pressed) {
      g_quit = true;
   }
}

// Reset the dynamic cube and clear one-frame force/torque accumulators.
// 重置动态方块，并清空单帧累计力和力矩。
static void ResetCube(HE3D::GameObject &cube, HE3D::PhysicsBody &body)
{
   cube.position    = {0.0f, 3.0f, 4.0f};
   cube.orientation = HE3D::quat();
   body.SetVelocity({1.0f, 0.0f, 0.0f});
   body.SetAngularVelocity({0.0f, 0.0f, 0.0f});
   body.ClearForces();
   body.ClearTorques();
}

// Detect whether the cube is close enough to the floor to allow jumping.
// 判断方块是否足够接近地板，以决定是否允许跳跃。
//
// This is intentionally a small example-side helper. It is not a general
// physics query; it uses AABB overlap because the example only uses a cube and
// a flat floor.
//
// 这是示例侧的小辅助函数，不是通用物理查询。它使用 AABB 重叠判断，
// 因为本示例只使用方块和平坦地板。
static bool IsGrounded(const HE3D::GameObject &cube, const HE3D::GameObject &floor,
                       const HE3D::PhysicsBody &body)
{
   HE3D::AABB cubeBounds(cube.position - HE3D::float3(0.5f, 0.5f, 0.5f),
                         cube.position + HE3D::float3(0.5f, 0.5f, 0.5f));
   HE3D::AABB floorBounds(floor.position - HE3D::float3(3.5f, 0.1f, 3.5f),
                          floor.position + HE3D::float3(3.5f, 0.1f, 3.5f));

   const float horizontalSlop = 0.02f;
   bool        overlapsX      = cubeBounds.max.x >= floorBounds.min.x - horizontalSlop &&
                                cubeBounds.min.x <= floorBounds.max.x + horizontalSlop;
   bool        overlapsZ      = cubeBounds.max.z >= floorBounds.min.z - horizontalSlop &&
                                cubeBounds.min.z <= floorBounds.max.z + horizontalSlop;
   if (!overlapsX || !overlapsZ) {
      return false;
   }

   const float groundSlop              = 0.06f;
   const float maxPenetrationForGround = 0.35f;
   bool        nearFloorTop = cubeBounds.min.y <= floorBounds.max.y + groundSlop &&
                              cubeBounds.min.y >= floorBounds.max.y - maxPenetrationForGround;
   bool        notMovingUp  = body.velocity.y <= 0.25f;
   return nearFloorTop && notMovingUp;
}

// Toggle FXAA on the key-down edge so holding F does not toggle repeatedly.
// 在按键按下边沿切换 FXAA，避免长按 F 时反复切换。
static void HandleFxaaToggle(bool &fxaaWasDown)
{
   bool fxaaDown = g_keys['f'];
   if (fxaaDown && !fxaaWasDown) {
      HE3D::SetFxaaEnabled(!HE3D::IsFxaaEnabled());
   }
   fxaaWasDown = fxaaDown;
}

// Apply player input as force, torque, and jump impulse.
// 把玩家输入转换为力、力矩和跳跃冲量。
static void ApplyInput(HE3D::PhysicsBody &body, bool grounded, bool &jumpWasDown)
{
   if (g_keys['a']) {
      body.AddForce({-14.0f, 0.0f, 0.0f});
      body.AddTorque({0.0f, 0.0f, 4.0f});
   }
   if (g_keys['d']) {
      body.AddForce({14.0f, 0.0f, 0.0f});
      body.AddTorque({0.0f, 0.0f, -4.0f});
   }

   bool jumpDown = g_keys[' '];
   if (jumpDown && !jumpWasDown && grounded) {
      body.AddImpulse({0.0f, 5.0f, 0.0f});
   }
   jumpWasDown = jumpDown;
}

// Draw the floor and cube. The cube turns yellow while grounded.
// 绘制地板和方块。方块接地时显示为黄色。
static void DrawScene(HE3D::Renderer &renderer, const HE3D::GameObject &floor,
                      const HE3D::GameObject &cube, const HE3D::Camera &camera, bool grounded)
{
   renderer.Clear({0.08f, 0.10f, 0.13f});
   renderer.DrawGameObject(floor, camera, HE3D::color3(0.25f, 0.55f, 0.35f));
   renderer.DrawGameObject(cube, camera,
                           grounded ? HE3D::color3(0.95f, 0.75f, 0.25f)
                                    : HE3D::color3(0.35f, 0.65f, 1.0f));
   renderer.Present();
}

// Run the interactive physics scene example.
// 运行交互式物理场景示例。
int main()
{
   // WindowDesc describes the window we want.
   // WindowDesc 描述我们要创建的窗口。
   HE3D::WindowDesc windowDesc;
   windowDesc.width  = 640;
   windowDesc.height = 360;
   windowDesc.title  = "HE3D PhysicsTest";
   windowDesc.flags  = 0;

   // CreateWindow creates the window used by Renderer and input.
   // CreateWindow 创建 Renderer 和输入系统要使用的窗口。
   HE3D::Window *window = HE3D::CreateWindow(&windowDesc);
   if (!window) {
      return 1;
   }

   // This example is interactive, so 260 FPS is enough and avoids wasting CPU.
   // 这是交互示例，260 FPS 已经足够，也避免浪费 CPU。
   HE3D::SetKeyCallback(window, OnKey, nullptr);
   HE3D::SetFrameRateLimit(260);
   HE3D::SetFxaaEnabled(false);

   // Renderer draws the scene into the window.
   // Renderer 把场景绘制到窗口里。
   HE3D::Renderer renderer(window, windowDesc.width, windowDesc.height);

   // DirectionalLight is a simple sunlight-like light source.
   // DirectionalLight 是类似日光的简单方向光。
   HE3D::DirectionalLight light;
   light.direction = {0.4f, 0.8f, -1.0f};
   light.color     = {1.0f, 1.0f, 1.0f};
   light.ambient   = 0.25f;
   renderer.SetMainLight(light);

   // The camera looks at the floor and cube from slightly behind.
   // 相机从稍靠后的位置观察地板和方块。
   HE3D::Camera camera;
   camera.position    = {0.0f, 1.4f, -5.0f};
   camera.orientation = HE3D::quat();
   camera.fov         = 50.0f;

   // The floor is a static mesh. It is drawn and used as a static collider.
   // 地板是静态 mesh。它既会被绘制，也会作为静态碰撞体使用。
   HE3D::GameObject floor;
   floor.mesh     = HE3D::Mesh::CreateCube(7.0f, 0.2f, 7.0f);
   floor.position = {0.0f, -0.6f, 4.0f};

   // The cube is the dynamic body controlled by input and gravity.
   // 方块是动态刚体，受输入和重力影响。
   HE3D::GameObject cube;
   cube.mesh = HE3D::Mesh::CreateCube(1.0f, 1.0f, 1.0f);

   if (!floor.mesh || !cube.mesh) {
      delete floor.mesh;
      delete cube.mesh;
      HE3D::DestroyWindow(window);
      return 1;
   }

   // Colliders are shape caches; PhysicsScene binds them to GameObjects.
   // Collider 是形状缓存；PhysicsScene 把它们和 GameObject 绑定起来。
   HE3D::StaticMeshCollider floorCollider(floor.mesh);
   HE3D::BoxCollider        cubeCollider(1.0f, 1.0f, 1.0f);
   if (!floorCollider.IsValid() || !cubeCollider.IsValid()) {
      delete floor.mesh;
      delete cube.mesh;
      HE3D::DestroyWindow(window);
      return 1;
   }

   // PhysicsBody stores dynamic state. It does not step itself.
   // PhysicsBody 保存动态状态。它不会自己推进。
   HE3D::PhysicsBody cubeBody(&cube);
   cubeBody.SetEnabled(true);
   cubeBody.SetMass(1.0f);
   cubeBody.SetInertia(1.0f);
   cubeBody.friction    = 0.45f;
   cubeBody.restitution = 0.5f;
   cubeBody.damping     = 0.05f;
   ResetCube(cube, cubeBody);

   // PhysicsScene owns the list of bodies/colliders to solve together.
   // PhysicsScene 保存需要统一求解的 body/collider 列表。
   //
   // This scene is fixed: the cube and floor stay alive until shutdown, so it
   // does not need to be cleared and rebuilt every frame.
   //
   // 这个 scene 的条目是固定的：方块和地板会一直存在到退出，因此不需要每帧 Clear 再重新 Add。
   HE3D::PhysicsScene physics;
   if (!physics.AddBody(&cube, &cubeBody, &cubeCollider) ||
       !physics.AddStatic(&floor, &floorCollider)) {
      delete floor.mesh;
      delete cube.mesh;
      HE3D::DestroyWindow(window);
      return 1;
   }

   bool   fxaaWasDown = false;
   bool   jumpWasDown = false;
   bool   grounded    = false;
   double lastTime    = HE3D::TimeSeconds();

   // Main loop: collect input, add forces, step physics, then draw.
   // 主循环：收集输入，施加力，推进物理，然后绘制。
   while (!g_quit && !HE3D::WindowShouldClose(window)) {
      double frameStart = HE3D::TimeSeconds();
      HE3D::PollEvents(window);

      double now       = HE3D::TimeSeconds();
      float  deltaTime = (float)(now - lastTime);
      lastTime         = now;
      if (deltaTime > 0.05f) {
         deltaTime = 0.05f;
      }

      HandleFxaaToggle(fxaaWasDown);

      if (g_keys['r']) {
         ResetCube(cube, cubeBody);
         grounded = false;
      }

      ApplyInput(cubeBody, grounded, jumpWasDown);

      // PhysicsScene::Step integrates body velocity, solves contacts, and clears forces.
      // PhysicsScene::Step 会积分刚体速度、解算接触，并清空累计力。
      physics.Step(deltaTime, 4);
      grounded = IsGrounded(cube, floor, cubeBody);
      if (cube.position.y < -5.0f) {
         ResetCube(cube, cubeBody);
         grounded = false;
      }

      DrawScene(renderer, floor, cube, camera, grounded);
      HE3D::PaceFrame(frameStart);
   }

   // Release resources created by this example.
   // 释放这个示例创建的资源。
   delete floor.mesh;
   delete cube.mesh;
   HE3D::DestroyWindow(window);
   return 0;
}
