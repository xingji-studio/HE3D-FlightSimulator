/*
 * HE3D Engine for 3D - Flight Simulator
 * Portable HE3D example. C++11 classes. No standard library in XJ380 builds.
 *
 * This file is intentionally written as readable sample code. It shows how a
 * small game can use HE3D without depending on a desktop-only framework.
 */
#include "flight_sim.hpp"

// ============================================================================
// Input state (accessed by callback + main loop)
// ============================================================================
static bool keys[256];
static volatile bool g_quit = false;
static const char *WINDOW_TITLE = "HE3D Flight Simulator";

#ifdef __XJ380_OS__
extern "C" void xapi_OutputSerial(char *str);
#endif

static void KeyHandler(int key, bool pressed, void *) {
    if (key >= 0 && key < 256) {
        keys[key] = pressed;
    }
    if (key == 27 && pressed) {
        g_quit = true;
    }
}

static void AppendUnsigned(char *dst, int *pos, int maxLen, unsigned int value)
{
    char tmp[16];
    int n = 0;
    if (value == 0) {
        tmp[n++] = '0';
    } else {
        while (value > 0 && n < (int)sizeof(tmp)) {
            tmp[n++] = (char)('0' + (value % 10U));
            value /= 10U;
        }
    }
    while (n > 0 && *pos < maxLen - 1) {
        dst[(*pos)++] = tmp[--n];
    }
}

static void BuildFpsTitle(char *dst, int maxLen, unsigned int fps)
{
    int pos = 0;
    const char *prefix = WINDOW_TITLE;
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

static void ReportFps(HE3D::Window *win, unsigned int fps)
{
#ifdef __XJ380_OS__
    (void)win;
    char line[32];
    int pos = 0;
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
    HE3D::SetWindowTitle(win, title);
#endif
}

// ============================================================================
// Input curve (nonlinear flight controls)
// ============================================================================
static float InputCurve(float& cur, bool pos, bool neg, float dt, float acc, float rec) {
    // Smooth key input into an analog-like control value. This keeps the sample
    // readable while avoiding instant full-force pitch, yaw, or roll changes.
    float tgt = 0.0f;
    if (pos) tgt = 1.0f; else if (neg) tgt = -1.0f;
    if (tgt != 0.0f) cur += tgt * acc * dt;
    else {
        if (cur > 0) cur = HE3D_MAX(0.0f, cur - rec * dt);
        else if (cur < 0) cur = HE3D_MIN(0.0f, cur + rec * dt);
    }
    cur = HE3D_CLAMP(cur, -1.0f, 1.0f);
    float s = (cur >= 0) ? 1.0f : -1.0f;
    float a = HE3D_ABS(cur);
    return s * a * a * a;
}

static float NormAngle(float a) {
    // Single fmod-style wrap instead of iterative while loops.
    if (a > HE3D::HE3D_PI || a < -HE3D::HE3D_PI) {
        a -= (int)(a * 0.159154943f) * HE3D::HE3D_TAU;  // 1/TAU ≈ 0.159154943
        if (a >  HE3D::HE3D_PI) a -= HE3D::HE3D_TAU;
        if (a < -HE3D::HE3D_PI) a += HE3D::HE3D_TAU;
    }
    return a;
}

static HE3D::quat AxisAngleQuat(HE3D::float3 axis, float angle)
{
    axis = axis.normalizeFast();
    float s, c;
    HE3D::sincosf(angle * 0.5f, &s, &c);
    return {c, axis.x * s, axis.y * s, axis.z * s};
}

// ============================================================================
// Main
// ============================================================================
int main(int argc, char** argv, char** envp) {
    // ---- Window ----
    const int W = 800, H = 600;
    const int RW = W, RH = H;
    HE3D::WindowDesc windowDesc;
    windowDesc.width = W;
    windowDesc.height = H;
    windowDesc.title = WINDOW_TITLE;
    windowDesc.flags = 0;

    HE3D::Window *win = HE3D::CreateWindow(&windowDesc);
    if (!win) {
        return 1;
    }
    HE3D::SetKeyCallback(win, KeyHandler, nullptr);
    HE3D::SetFrameRateLimit(0);
    HE3D::SetFxaaEnabled(false);

    // ---- Renderer ----
    HE3D::Renderer eng(win, RW, RH);

    // ---- Scene ----
    HE3D::Camera cam;
    cam.position = {0, 12, 5};

    HE3D::GameObject plane;
    // Prefer the external aircraft model. If the asset is missing, fall back to
    // a tiny built-in mesh so the example still demonstrates rendering.
    plane.mesh = HE3D::Mesh::LoadOBJ("biplane.obj");
    if (!plane.mesh) {
        plane.mesh = HE3D::Mesh::Create(FLIGHT_FALLBACK_AIRCRAFT_VERTICES,
                                  FLIGHT_FALLBACK_AIRCRAFT_UVS,
                                  FLIGHT_FALLBACK_AIRCRAFT_VERTEX_COUNT);
    }
    plane.position = {0, 13, 0};
    HE3D::PhysicsBody planeBody(&plane);
    planeBody.SetEnabled(true);
    HE3D::CollisionBox planeBox(&plane);
    if (!planeBox.FitMesh()) {
        planeBox.halfExtents = {0.5f, 0.2f, 0.7f};
    }
    HE3D::Texture* pTex  = HE3D::Texture::LoadBMP("biplane.bmp");
    bool hasTex    = (pTex && pTex->valid);

    // ---- Terrain tiles ----
    // The terrain is split into a small 3x3 grid around the plane. Tiles are
    // recycled as the aircraft moves so memory use stays fixed.
    const int RD  = 1;      // 3x3 tile radius.
    const float GS = 12.0f;
    const int GC   = 7;
    const float TS = (GC - 1) * GS;
    const int TN   = (RD * 2 + 1) * (RD * 2 + 1);

    struct Tile { HE3D::GameObject obj; HE3D::CollisionBox box; int gx, gz; bool active; };
    struct TileRequest { int gx, gz; };
    Tile* tiles = new Tile[TN];
    for (int i = 0; i < TN; i++) {
        tiles[i].active = false;
        tiles[i].obj.mesh = nullptr;
        tiles[i].box.BindGameObject(&tiles[i].obj);
    }

    int centerX = 0, centerZ = 0;
    bool dirty = true;
    TileRequest pendingTiles[TN];
    int pendingCount = 0;
    int pendingCursor = 0;

    // ---- Lighting ----
    HE3D::DirectionalLight sun;
    sun.direction = {1.0f, 1.0f, 0.5f};
    sun.color     = {1.5f, 1.4f, 1.2f};
    eng.SetMainLight(sun);

    // ---- Input ----
    // These accumulators store smoothed pitch, yaw, and roll input.
    float iP = 0, iY = 0, iR = 0;
    const float ACC = 4.5f, REC = 2.5f;

    // ---- Camera ----
    float  camYaw = 0.0f;
    HE3D::float3 camOff = {0.0f, 0.7f, -1.8f};

    // ---- Timing ----
    double lt = HE3D::TimeSeconds();
    double fpsStart = lt;
    unsigned int fpsFrames = 0;
    bool fxaaKeyWasDown = false;

    // ---- Main loop ----
    while (!g_quit && !HE3D::WindowShouldClose(win)) {
        double frameStart = HE3D::TimeSeconds();
        HE3D::PollEvents(win);

        double now = frameStart;
        float dt = (float)(now - lt);
        lt = now;
        if (dt > 0.1f) dt = 0.1f;

        cam.Update(dt);

        bool fxaaKeyDown = keys['F'] || keys['f'];
        if (fxaaKeyDown && !fxaaKeyWasDown) {
            HE3D::SetFxaaEnabled(!HE3D::IsFxaaEnabled());
        }
        fxaaKeyWasDown = fxaaKeyDown;

        // Input mapping:
        // W/S pitch the nose, Q/E yaw the aircraft, and A/D roll the wings.
        float cP = InputCurve(iP, keys['W']||keys['w'], keys['S']||keys['s'], dt, ACC, REC);
        float cY = InputCurve(iY, keys['E']||keys['e'], keys['Q']||keys['q'], dt, ACC, REC);
        float cR = InputCurve(iR, keys['A']||keys['a'], keys['D']||keys['d'], dt, ACC, REC);

        // Flight physics
        // Yaw uses world-up when the wings are level, then blends toward the
        // aircraft's local up vector as bank angle increases.
        HE3D::quat dP = HE3D::quat::FromEuler({cP * 1.35f * dt, 0, 0});
        HE3D::float3 right = plane.orientation.rotate({1, 0, 0});
        HE3D::float3 localUp = plane.orientation.rotate({0, 1, 0});
        HE3D::float3 worldUp = {0, 1, 0};
        float bankInfluence = HE3D_CLAMP(HE3D_ABS(right.y), 0.0f, 1.0f);
        HE3D::float3 yawAxis = (worldUp * (1.0f - bankInfluence) + localUp * bankInfluence).normalizeFast();
        HE3D::quat dY = AxisAngleQuat(yawAxis, cY * 0.95f * dt);
        HE3D::quat dR = HE3D::quat::FromEuler({0, 0, cR * 1.75f * dt});
        plane.orientation = (dY * plane.orientation * dP * dR).normalizeFast();
        HE3D::float3 fwd = plane.Forward();
        HE3D::CollisionBox obstacles[TN];
        int obstacleCount = 0;
        for (int i = 0; i < TN; i++) {
            if (tiles[i].active && tiles[i].box.IsValid() && obstacleCount < TN) {
                obstacles[obstacleCount++] = tiles[i].box;
            }
        }
        planeBody.SetVelocity(fwd * 15.0f);
        planeBody.StepWithCollisions(dt, planeBox, obstacles, obstacleCount, 8);

        // Camera
        // Follow the aircraft from behind while smoothing the yaw so quick
        // maneuvers remain readable.
        HE3D::float3 hf = {fwd.x, 0, fwd.z};
        float hsq = hf.x*hf.x + hf.z*hf.z;
        if (hsq > 0.001f) {
            float tYaw = HE3D::atan2f(hf.x, hf.z);
            camYaw += NormAngle(tYaw - camYaw) * HE3D_MIN(5.0f * dt, 1.0f);
        }
        cam.orientation = HE3D::quat::FromEuler({0.15f, camYaw, 0});
        HE3D::quat camYQ = HE3D::quat::FromEuler({0, camYaw, 0});
        HE3D::float3 tgtPos = plane.position + camYQ.rotate(camOff);
        HE3D::float3 diff = tgtPos - cam.position;
        if (diff.lengthSq() > 100.0f) cam.position = tgtPos;
        else cam.position = cam.position + diff * HE3D_MIN(8.0f * dt, 1.0f);

        // Terrain management
        // Keep only the tiles near the current aircraft grid coordinate.
        int pGX = (int)HE3D::floorf(plane.position.x / TS);
        int pGZ = (int)HE3D::floorf(plane.position.z / TS);
        if (pGX != centerX || pGZ != centerZ || dirty) {
            centerX = pGX; centerZ = pGZ; dirty = false;
            pendingCount = 0;
            pendingCursor = 0;

            // Recycle out-of-range tiles.
            for (int i = 0; i < TN; i++) {
                if (!tiles[i].active) continue;
                bool found = false;
                for (int dx = -RD; dx <= RD && !found; dx++)
                    for (int dz = -RD; dz <= RD; dz++)
                        if (tiles[i].gx == centerX + dx && tiles[i].gz == centerZ + dz)
                            { found = true; break; }
                if (!found) { tiles[i].active = false; }
            }

            for (int dx = -RD; dx <= RD; dx++) {
                for (int dz = -RD; dz <= RD; dz++) {
                    int gx = centerX + dx;
                    int gz = centerZ + dz;
                    bool found = false;
                    for (int i = 0; i < TN; i++) {
                        if (tiles[i].active && tiles[i].gx == gx && tiles[i].gz == gz) {
                            found = true;
                            break;
                        }
                    }
                    if (!found && pendingCount < TN) {
                        pendingTiles[pendingCount++] = {gx, gz};
                    }
                }
            }
        }

        // Generate or refresh at most one terrain tile per frame. Crossing a
        // tile boundary used to update several meshes in one frame, which made
        // XJ380 stall visibly and could pull the measured FPS down for a whole
        // reporting interval.
        if (pendingCursor < pendingCount) {
            int gx = pendingTiles[pendingCursor].gx;
            int gz = pendingTiles[pendingCursor].gz;
            pendingCursor++;

            bool alreadyActive = false;
            for (int i = 0; i < TN; i++) {
                if (tiles[i].active && tiles[i].gx == gx && tiles[i].gz == gz) {
                    alreadyActive = true;
                    break;
                }
            }

            if (!alreadyActive) {
                for (int i = 0; i < TN; i++) {
                    if (!tiles[i].active) {
                        tiles[i].gx = gx; tiles[i].gz = gz;
                        float wx = tiles[i].gx * TS, wz = tiles[i].gz * TS;
                        if (!tiles[i].obj.mesh) {
                            tiles[i].obj.mesh = CreatePlane(GC, GS, wx, wz);
                        } else if (!UpdatePlaneMesh(tiles[i].obj.mesh, GC, GS, wx, wz)) {
                            delete tiles[i].obj.mesh;
                            tiles[i].obj.mesh = CreatePlane(GC, GS, wx, wz);
                        }
                        tiles[i].obj.position = {wx, 0, wz};
                        tiles[i].active = (tiles[i].obj.mesh != nullptr);
                        if (tiles[i].active) {
                            tiles[i].box.BindGameObject(&tiles[i].obj);
                            tiles[i].box.FitMesh();
                        }
                        break;
                    }
                }
            }
        }

        // Render
        // Draw terrain first, then the plane, and finally present the platform
        // framebuffer through XAPI or SDL3.
        eng.Clear(HE3D::color3(0.45f, 0.75f, 1.0f));
        HE3D::color3 tc = {0.3f, 0.7f, 0.3f};
        float drawRadiusSq = (TS * 1.45f) * (TS * 1.45f);
        for (int i = 0; i < TN; i++)
            if (tiles[i].active && tiles[i].obj.mesh) {
                HE3D::float3 td = tiles[i].obj.position - cam.position;
                if (td.x * td.x + td.z * td.z > drawRadiusSq) continue;
                eng.DrawGameObject(tiles[i].obj, cam, tc);
            }
        if (hasTex) eng.DrawGameObject(plane, cam, *pTex);
        else       eng.DrawGameObject(plane, cam, HE3D::color3(0.8f, 0.2f, 0.2f));
        eng.Present();

        fpsFrames++;
        double fpsElapsed = now - fpsStart;
        if (fpsElapsed >= 1.0) {
            unsigned int fps = (unsigned int)((double)fpsFrames / fpsElapsed + 0.5);
            ReportFps(win, fps);
            fpsStart = now;
            fpsFrames = 0;
        }
        HE3D::PaceFrame(frameStart);
    }

    // Cleanup
    delete plane.mesh;
    delete pTex;
    for (int i = 0; i < TN; i++) delete tiles[i].obj.mesh;
    delete[] tiles;
    HE3D::DestroyWindow(win);
    return 0;
}
