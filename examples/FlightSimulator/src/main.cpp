/*
 * HE3D Engine for 3D - Flight Simulator
 * Portable HE3D example. C++11 classes. No standard library in XJ380 builds.
 * HE3D 便携飞行模拟示例。XJ380 构建中不依赖 C++ 标准库。
 *
 * This file is intentionally written as readable sample code. It shows how a
 * small game can use HE3D without depending on a desktop-only framework.
 *
 * 本文件刻意写成易读的示例代码，用来展示一个小型游戏如何使用 HE3D，
 * 同时不依赖只在桌面平台可用的框架。
 */
#include "flight_sim.hpp"

static bool          g_keys[256];
static volatile bool g_quit      = false;
static const char   *windowTitle = "HE3D Flight Simulator";

#ifdef __XJ380_OS__
extern "C" void xapi_OutputSerial(char *str);
#endif

// Smoothed input values for pitch, yaw, and roll.
// 俯仰、偏航和滚转的平滑输入值。
struct FlightControls {
   float pitch;
   float yaw;
   float roll;

   FlightControls() : pitch(0.0f), yaw(0.0f), roll(0.0f) {}
};

// One reusable terrain tile with a cached static mesh collider.
// 一个带静态 mesh 碰撞缓存的可复用地形块。
struct TerrainTile {
   HE3D::Mesh         mesh;
   HE3D::GameObject   object;
   HE3D::MeshCollider collider;
   int                gridX;
   int                gridZ;
   bool               active;

   TerrainTile() : mesh(), object(mesh), collider(mesh), gridX(0), gridZ(0), active(false) {}
};

// Grid coordinate queued for terrain generation.
// 等待生成的地形网格坐标。
struct TerrainTileRequest {
   int gridX;
   int gridZ;
};

// HE3D sends keyboard events here after PollEvents() is called.
// 调用 PollEvents() 后，HE3D 会把键盘事件发送到这里。
static void KeyHandler(int key, bool pressed, void *)
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

// Append an unsigned integer to a fixed-size C string buffer.
// 将无符号整数追加到固定长度 C 字符串缓冲区。
static void AppendUnsigned(char *dst, int *pos, int maxLen, unsigned int value)
{
   char tmp[16];
   int  count = 0;
   if (value == 0) {
      tmp[count++] = '0';
   } else {
      while (value > 0 && count < (int)sizeof(tmp)) {
         tmp[count++] = (char)('0' + (value % 10U));
         value /= 10U;
      }
   }
   while (count > 0 && *pos < maxLen - 1) {
      dst[(*pos)++] = tmp[--count];
   }
}

// Build the window title with the averaged FPS value.
// 构造带平均 FPS 的窗口标题。
static void BuildFpsTitle(char *dst, int maxLen, unsigned int fps)
{
   int         pos    = 0;
   const char *prefix = windowTitle;
   while (*prefix && pos < maxLen - 1) {
      dst[pos++] = *prefix++;
   }
   const char *mid = " - FPS ";
   while (*mid && pos < maxLen - 1) {
      dst[pos++] = *mid++;
   }
   AppendUnsigned(dst, &pos, maxLen, fps);
   dst[pos] = 0;
}

// Report FPS through the platform title on desktop or serial output on XJ380.
// 在桌面平台通过窗口标题报告 FPS，在 XJ380 上通过串口输出。
static void ReportFps(HE3D::Window *window, unsigned int fps)
{
#ifdef __XJ380_OS__
   (void)window;
   char        line[32];
   int         pos    = 0;
   const char *prefix = "HE3D FPS ";
   while (*prefix && pos < (int)sizeof(line) - 1) {
      line[pos++] = *prefix++;
   }
   AppendUnsigned(line, &pos, (int)sizeof(line), fps);
   if (pos < (int)sizeof(line) - 1) {
      line[pos++] = '\n';
   }
   line[pos] = 0;
   xapi_OutputSerial(line);
#else
   char title[64];
   BuildFpsTitle(title, (int)sizeof(title), fps);
   HE3D::SetWindowTitle(window, title);
#endif
}

// Ease a digital key pair into a smooth cubic control value.
// 将一对数字按键平滑成三次曲线控制值。
static float InputCurve(float &current, bool positive, bool negative, float deltaTime,
                        float acceleration, float recovery)
{
   float target = 0.0f;
   if (positive) {
      target = 1.0f;
   } else if (negative) {
      target = -1.0f;
   }

   if (target != 0.0f) {
      current += target * acceleration * deltaTime;
   } else if (current > 0.0f) {
      current = HE3D_MAX(0.0f, current - recovery * deltaTime);
   } else if (current < 0.0f) {
      current = HE3D_MIN(0.0f, current + recovery * deltaTime);
   }

   current      = HE3D_CLAMP(current, -1.0f, 1.0f);
   float sign   = (current >= 0.0f) ? 1.0f : -1.0f;
   float amount = HE3D_ABS(current);
   return sign * amount * amount * amount;
}

// Normalize an angle to the -PI..PI range.
// 将角度归一化到 -PI 到 PI 范围。
static float NormalizeAngle(float angle)
{
   if (angle > HE3D::HE3D_PI || angle < -HE3D::HE3D_PI) {
      angle -= (int)(angle * 0.159154943f) * HE3D::HE3D_TAU;
      if (angle > HE3D::HE3D_PI) {
         angle -= HE3D::HE3D_TAU;
      }
      if (angle < -HE3D::HE3D_PI) {
         angle += HE3D::HE3D_TAU;
      }
   }
   return angle;
}

// Create a quaternion from an axis and a radians angle.
// 根据旋转轴和弧度角创建四元数。
static HE3D::quat AxisAngleQuat(HE3D::float3 axis, float angle)
{
   axis = axis.normalizeFast();
   float sine, cosine;
   HE3D::sincosf(angle * 0.5f, &sine, &cosine);
   return {cosine, axis.x * sine, axis.y * sine, axis.z * sine};
}

// Toggle FXAA on the key-down edge so holding F does not toggle repeatedly.
// 在按键按下边沿切换 FXAA，避免长按 F 时反复切换。
static void HandleToggleKeys(bool &fxaaKeyWasDown)
{
   bool fxaaKeyDown = g_keys['f'];
   if (fxaaKeyDown && !fxaaKeyWasDown) {
      HE3D::SetFxaaEnabled(!HE3D::IsFxaaEnabled());
   }
   fxaaKeyWasDown = fxaaKeyDown;
}

// Update aircraft orientation from controls and write its forward velocity.
// 根据控制输入更新飞机朝向，并写入向前飞行速度。
static HE3D::float3 UpdateFlightControls(FlightControls &controls, HE3D::PhysicsScene &scene,
                                         HE3D::GameObject &plane, float deltaTime)
{
   const float inputAcceleration = 4.5f;
   const float inputRecovery     = 2.5f;
   float       pitchControl      = InputCurve(controls.pitch, g_keys['w'], g_keys['s'], deltaTime,
                                              inputAcceleration, inputRecovery);
   float       yawControl        = InputCurve(controls.yaw, g_keys['e'], g_keys['q'], deltaTime,
                                              inputAcceleration, inputRecovery);
   float       rollControl       = InputCurve(controls.roll, g_keys['a'], g_keys['d'], deltaTime,
                                              inputAcceleration, inputRecovery);

   HE3D::quat   pitchDelta = HE3D::quat::FromEuler({pitchControl * 1.35f * deltaTime, 0.0f, 0.0f});
   HE3D::float3 right      = plane.orientation.rotate({1.0f, 0.0f, 0.0f});
   HE3D::float3 localUp    = plane.orientation.rotate({0.0f, 1.0f, 0.0f});
   HE3D::float3 worldUp    = {0.0f, 1.0f, 0.0f};
   float        bankInfluence = HE3D_CLAMP(HE3D_ABS(right.y), 0.0f, 1.0f);
   HE3D::float3 yawAxis =
       (worldUp * (1.0f - bankInfluence) + localUp * bankInfluence).normalizeFast();
   HE3D::quat yawDelta  = AxisAngleQuat(yawAxis, yawControl * 0.95f * deltaTime);
   HE3D::quat rollDelta = HE3D::quat::FromEuler({0.0f, 0.0f, rollControl * 1.75f * deltaTime});

   plane.orientation    = (yawDelta * plane.orientation * pitchDelta * rollDelta).normalizeFast();
   HE3D::float3 forward = plane.Forward();
   scene.SetVelocity(plane, forward * 15.0f);
   return forward;
}

// Return whether a terrain tile is still inside the active square.
// 判断地形块是否仍位于活动区域内。
static bool IsTileInRange(const TerrainTile &tile, int centerGridX, int centerGridZ, int tileRadius)
{
   return tile.gridX >= centerGridX - tileRadius && tile.gridX <= centerGridX + tileRadius &&
          tile.gridZ >= centerGridZ - tileRadius && tile.gridZ <= centerGridZ + tileRadius;
}

// Return whether the requested grid coordinate already has an active tile.
// 判断指定网格坐标是否已有活动地形块。
static bool HasActiveTile(const TerrainTile *tiles, int tileCount, int gridX, int gridZ)
{
   for (int i = 0; i < tileCount; i++) {
      if (tiles[i].active && tiles[i].gridX == gridX && tiles[i].gridZ == gridZ) {
         return true;
      }
   }
   return false;
}

// Recycle out-of-range tiles and queue missing nearby terrain tiles.
// 回收超出范围的地形块，并把附近缺失的地形块加入生成队列。
static void QueueTerrainTiles(TerrainTile *tiles, int tileCount, int centerGridX, int centerGridZ,
                              int tileRadius, TerrainTileRequest *pendingTiles, int &pendingCount,
                              int &pendingCursor)
{
   pendingCount  = 0;
   pendingCursor = 0;

   for (int i = 0; i < tileCount; i++) {
      if (tiles[i].active && !IsTileInRange(tiles[i], centerGridX, centerGridZ, tileRadius)) {
         tiles[i].active = false;
      }
   }

   for (int offsetX = -tileRadius; offsetX <= tileRadius; offsetX++) {
      for (int offsetZ = -tileRadius; offsetZ <= tileRadius; offsetZ++) {
         int gridX = centerGridX + offsetX;
         int gridZ = centerGridZ + offsetZ;
         if (!HasActiveTile(tiles, tileCount, gridX, gridZ) && pendingCount < tileCount) {
            pendingTiles[pendingCount].gridX = gridX;
            pendingTiles[pendingCount].gridZ = gridZ;
            pendingCount++;
         }
      }
   }
}

// Generate or refresh at most one queued terrain tile.
// 最多生成或刷新一个排队中的地形块。
static void RefreshNextTerrainTile(TerrainTile *tiles, int tileCount,
                                   TerrainTileRequest *pendingTiles, int &pendingCursor,
                                   int pendingCount, int terrainGridCount, float terrainStep,
                                   float tileSize)
{
   if (pendingCursor >= pendingCount) {
      return;
   }

   int gridX = pendingTiles[pendingCursor].gridX;
   int gridZ = pendingTiles[pendingCursor].gridZ;
   pendingCursor++;

   if (HasActiveTile(tiles, tileCount, gridX, gridZ)) {
      return;
   }

   for (int i = 0; i < tileCount; i++) {
      if (tiles[i].active) {
         continue;
      }

      tiles[i].gridX = gridX;
      tiles[i].gridZ = gridZ;
      float worldX   = tiles[i].gridX * tileSize;
      float worldZ   = tiles[i].gridZ * tileSize;
      tiles[i].mesh  = CreatePlane(terrainGridCount, terrainStep, worldX, worldZ);
      tiles[i].object.SetMesh(tiles[i].mesh);
      tiles[i].object.position = {worldX, 0.0f, worldZ};
      tiles[i].collider.Refresh();
      tiles[i].active = tiles[i].mesh.IsValid() && tiles[i].collider.IsValid();
      break;
   }
}

// Keep terrain tiles centered around the aircraft and refresh one tile per frame.
// 让地形块围绕飞机所在区域，并且每帧只刷新一个地形块。
static void UpdateTerrainTiles(TerrainTile *tiles, int tileCount, TerrainTileRequest *pendingTiles,
                               int &pendingCount, int &pendingCursor, int &centerGridX,
                               int &centerGridZ, bool &terrainDirty,
                               const HE3D::float3 &planePosition, int tileRadius,
                               int terrainGridCount, float terrainStep, float tileSize)
{
   int planeGridX = (int)HE3D::floorf(planePosition.x / tileSize);
   int planeGridZ = (int)HE3D::floorf(planePosition.z / tileSize);
   if (planeGridX != centerGridX || planeGridZ != centerGridZ || terrainDirty) {
      centerGridX  = planeGridX;
      centerGridZ  = planeGridZ;
      terrainDirty = false;
      QueueTerrainTiles(tiles, tileCount, centerGridX, centerGridZ, tileRadius, pendingTiles,
                        pendingCount, pendingCursor);
   }

   RefreshNextTerrainTile(tiles, tileCount, pendingTiles, pendingCursor, pendingCount,
                          terrainGridCount, terrainStep, tileSize);
}

// Add only the closest terrain colliders to keep the mesh collider cost bounded.
// 只加入最近的地形碰撞体，避免 mesh 碰撞开销无限增长。
static void AddNearbyTerrainColliders(HE3D::PhysicsScene &worldScene, TerrainTile *tiles,
                                      int tileCount, const HE3D::float3 &planePosition)
{
   const int maxColliderCount = 4;
   int       bestIndices[maxColliderCount];
   float     bestDistances[maxColliderCount];
   for (int i = 0; i < maxColliderCount; i++) {
      bestIndices[i]   = -1;
      bestDistances[i] = 340282346638528859811704183484516925440.0f;
   }

   for (int i = 0; i < tileCount; i++) {
      if (!tiles[i].active || !tiles[i].collider.IsValid()) {
         continue;
      }

      HE3D::float3 delta      = tiles[i].object.position - planePosition;
      float        distanceSq = delta.x * delta.x + delta.z * delta.z;
      for (int slot = 0; slot < maxColliderCount; slot++) {
         if (distanceSq >= bestDistances[slot]) {
            continue;
         }
         for (int move = maxColliderCount - 1; move > slot; move--) {
            bestDistances[move] = bestDistances[move - 1];
            bestIndices[move]   = bestIndices[move - 1];
         }
         bestDistances[slot] = distanceSq;
         bestIndices[slot]   = i;
         break;
      }
   }

   for (int i = 0; i < maxColliderCount; i++) {
      if (bestIndices[i] >= 0) {
         worldScene.AddStaticBody(tiles[bestIndices[i]].object, tiles[bestIndices[i]].collider);
      }
   }
}

// Rebuild the small per-frame physics scene for the aircraft and nearby terrain.
// 为飞机和附近地形重建小型逐帧物理场景。
static void BuildPhysicsScene(HE3D::PhysicsScene      &worldScene,
                              HE3D::PhysicsProperties &planeProperties,
                              HE3D::BoxCollider &planeCollider, HE3D::GameObject &plane,
                              TerrainTile *tiles, int tileCount, const HE3D::float3 &planePosition)
{
   worldScene.Clear();
   worldScene.AddDynamicBody(plane, planeCollider, planeProperties);
   AddNearbyTerrainColliders(worldScene, tiles, tileCount, planePosition);
}

// Follow the aircraft from behind with smoothed yaw.
// 从飞机后方跟随，并平滑相机偏航。
static void UpdateCamera(HE3D::Camera &camera, float &cameraYaw, const HE3D::GameObject &plane,
                         HE3D::float3 forward, HE3D::float3 cameraOffset, float deltaTime)
{
   HE3D::float3 horizontalForward = {forward.x, 0.0f, forward.z};
   if (horizontalForward.lengthSq() > 0.001f) {
      float targetYaw = HE3D::atan2f(horizontalForward.x, horizontalForward.z);
      cameraYaw += NormalizeAngle(targetYaw - cameraYaw) * HE3D_MIN(5.0f * deltaTime, 1.0f);
   }

   camera.orientation             = HE3D::quat::FromEuler({0.15f, cameraYaw, 0.0f});
   HE3D::quat   cameraYawRotation = HE3D::quat::FromEuler({0.0f, cameraYaw, 0.0f});
   HE3D::float3 targetPosition    = plane.position + cameraYawRotation.rotate(cameraOffset);
   HE3D::float3 delta             = targetPosition - camera.position;
   if (delta.lengthSq() > 100.0f) {
      camera.position = targetPosition;
   } else {
      camera.position = camera.position + delta * HE3D_MIN(8.0f * deltaTime, 1.0f);
   }
}

// Draw active terrain tiles and then draw the aircraft.
// 绘制活动地形块，然后绘制飞机。
static void DrawScene(HE3D::Renderer &renderer, TerrainTile *tiles, int tileCount,
                      const HE3D::GameObject &plane, const HE3D::Texture &planeTexture,
                      bool hasPlaneTexture, const HE3D::Camera &camera, float tileSize)
{
   renderer.Clear(HE3D::color3(0.45f, 0.75f, 1.0f));

   HE3D::color3 terrainColor = {0.3f, 0.7f, 0.3f};
   float        drawRadiusSq = (tileSize * 1.45f) * (tileSize * 1.45f);
   for (int i = 0; i < tileCount; i++) {
      if (!tiles[i].active || !tiles[i].mesh.IsValid()) {
         continue;
      }
      HE3D::float3 tileDelta = tiles[i].object.position - camera.position;
      if (tileDelta.x * tileDelta.x + tileDelta.z * tileDelta.z > drawRadiusSq) {
         continue;
      }
      renderer.DrawGameObject(tiles[i].object, camera, terrainColor);
   }

   if (hasPlaneTexture) {
      renderer.DrawGameObject(plane, camera, planeTexture);
   } else {
      renderer.DrawGameObject(plane, camera, HE3D::color3(0.8f, 0.2f, 0.2f));
   }
   renderer.Present();
}

// Run the flight simulator example.
// 运行飞行模拟示例。
int main(int argc, char **argv, char **envp)
{
   (void)argc;
   (void)argv;
   (void)envp;

   const int windowWidth  = 800;
   const int windowHeight = 600;
   const int renderWidth  = windowWidth;
   const int renderHeight = windowHeight;

   HE3D::WindowDesc windowDesc;
   windowDesc.width  = windowWidth;
   windowDesc.height = windowHeight;
   windowDesc.title  = windowTitle;
   windowDesc.flags  = 0;

   HE3D::Window *window = HE3D::CreateWindow(&windowDesc);
   if (!window) {
      return 1;
   }
   HE3D::SetKeyCallback(window, KeyHandler, nullptr);
   HE3D::SetFrameRateLimit(0);
   HE3D::SetFxaaEnabled(false);
   HE3D::SetTaaEnabled(false);

   HE3D::Renderer renderer(window, renderWidth, renderHeight);

   HE3D::Camera camera;
   camera.fov      = 90.0f;
   camera.position = {0.0f, 12.0f, 5.0f};

   HE3D::Mesh planeMesh = HE3D::Mesh::LoadOBJ("biplane.obj");
   if (!planeMesh.IsValid()) {
      planeMesh = HE3D::Mesh::Create(flightFallbackAircraftVertices, flightFallbackAircraftUvs,
                                     flightFallbackAircraftVertexCount);
   }
   if (!planeMesh.IsValid()) {
      HE3D::DestroyWindow(window);
      return 1;
   }
   HE3D::GameObject plane(planeMesh);
   plane.position = {0.0f, 13.0f, 0.0f};

   HE3D::Texture planeTexture    = HE3D::Texture::LoadImage("biplane.bmp");
   bool          hasPlaneTexture = planeTexture.IsValid();

   HE3D::BoxCollider       planeCollider(1.0f, 0.45f, 1.4f);
   HE3D::PhysicsProperties planeProperties;
   planeProperties.SetMass(1.0f);
   planeProperties.SetInertia(planeCollider.EstimateInertia(planeProperties.GetMass()));
   planeProperties.SetRestitution(0.1f);
   planeProperties.SetFriction(0.5f);
   planeProperties.SetDamping(0.1f);

   const int   tileRadius       = 1;
   const float terrainStep      = 12.0f;
   const int   terrainGridCount = 7;
   const float tileSize         = (terrainGridCount - 1) * terrainStep;
   const int   tileCount        = (tileRadius * 2 + 1) * (tileRadius * 2 + 1);

   TerrainTile        tiles[tileCount];
   TerrainTileRequest pendingTiles[tileCount];
   int                terrainCenterX = 0;
   int                terrainCenterZ = 0;
   bool               terrainDirty   = true;
   int                pendingCount   = 0;
   int                pendingCursor  = 0;

   HE3D::PhysicsScene worldScene(tileCount + 1);
   worldScene.drag = 0.02f;
   worldScene.SetGravity({0.0f, -100.0f, 0.0f});

   HE3D::DirectionalLight sun;
   sun.direction = {1.0f, 1.0f, 0.5f};
   sun.color     = {1.5f, 1.4f, 1.2f};
   renderer.SetMainLight(sun);

   FlightControls controls;
   float          cameraYaw    = 0.0f;
   HE3D::float3   cameraOffset = {0.0f, 0.7f, -1.8f};

   double       lastTime       = HE3D::TimeSeconds();
   double       fpsStart       = lastTime;
   unsigned int fpsFrames      = 0;
   bool         fxaaKeyWasDown = false;

   while (!g_quit && !HE3D::WindowShouldClose(window)) {
      double frameStart = HE3D::TimeSeconds();
      HE3D::PollEvents(window);

      double now       = frameStart;
      float  deltaTime = (float)(now - lastTime);
      lastTime         = now;
      if (deltaTime > 0.1f) {
         deltaTime = 0.1f;
      }

      HandleToggleKeys(fxaaKeyWasDown);
      BuildPhysicsScene(worldScene, planeProperties, planeCollider, plane, tiles, tileCount,
                        plane.position);
      HE3D::float3 forward = UpdateFlightControls(controls, worldScene, plane, deltaTime);

      UpdateTerrainTiles(tiles, tileCount, pendingTiles, pendingCount, pendingCursor,
                         terrainCenterX, terrainCenterZ, terrainDirty, plane.position, tileRadius,
                         terrainGridCount, terrainStep, tileSize);

      worldScene.Step(deltaTime, 1);

      UpdateCamera(camera, cameraYaw, plane, forward, cameraOffset, deltaTime);
      DrawScene(renderer, tiles, tileCount, plane, planeTexture, hasPlaneTexture, camera, tileSize);

      fpsFrames++;
      double fpsElapsed = now - fpsStart;
      if (fpsElapsed >= 1.0) {
         unsigned int fps = (unsigned int)((double)fpsFrames / fpsElapsed + 0.5);
         ReportFps(window, fps);
         fpsStart  = now;
         fpsFrames = 0;
      }
      HE3D::PaceFrame(frameStart);
   }

   HE3D::DestroyWindow(window);
   return 0;
}
