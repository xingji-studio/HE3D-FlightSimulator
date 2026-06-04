/*
 * HE3D Engine for 3D - Flight Simulator
 * XJ380 platform. C++11 classes. No standard library. No BridgeEngine.
 */
#include "flight_sim.hpp"
#include <time.h>

// ============================================================================
// Input state (accessed by callback + main loop)
// ============================================================================
static bool keys[256];
static volatile bool g_quit = false;

extern "C" {
static void MsgHandler(UINT64 Type, UINT64 hData, UINT64 lData) {
    if (Type == MSG_CHAR) {
        bool pressed = (lData & 1) != 0;
        int ch = (int)(hData & 0xFF);
        if (ch < 256) keys[ch] = pressed;
        if (ch == 27 && pressed) g_quit = true;
    }
}
}

// ============================================================================
// Input curve (nonlinear flight controls)
// ============================================================================
static float InputCurve(float& cur, bool pos, bool neg, float dt, float acc, float rec) {
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
    while (a >  HE3D_PI) a -= HE3D_TAU;
    while (a < -HE3D_PI) a += HE3D_TAU;
    return a;
}

// ============================================================================
// Main
// ============================================================================
int main(int argc, char** argv, char** envp) {
    // ---- Window ----
    const int W = 800, H = 600;
    HDLE win;
    XWINDOW xw;
    xw.width  = W; xw.height = H;
    xw.title  = (WSTR)"HE3D Flight Simulator";
    xw.sets   = XWIN_NORMAL;
    xapi_CreateWindow(&win, &xw);
    SetMsgPrcor(win, MsgHandler);

    // ---- Renderer ----
    Renderer eng(win, W, H);

    // ---- Scene ----
    Camera cam;
    cam.position = {0, 12, 5};

    GameObject plane;
    plane.mesh = Mesh::LoadOBJ("Biplane.obj");
    if (!plane.mesh) {
        plane.mesh = Mesh::Create(FLIGHT_FALLBACK_AIRCRAFT_VERTICES,
                                  FLIGHT_FALLBACK_AIRCRAFT_UVS,
                                  FLIGHT_FALLBACK_AIRCRAFT_VERTEX_COUNT);
    }
    plane.position = {0, 13, 0};
    Texture* pTex  = Texture::LoadBMP("biplane.bmp");
    bool hasTex    = (pTex && pTex->valid);

    // ---- Terrain tiles ----
    const int RD  = 1;      // 3x3
    const float GS = 12.0f;
    const int GC   = 7;
    const float TS = (GC - 1) * GS;
    const int TN   = (RD * 2 + 1) * (RD * 2 + 1);

    struct Tile { GameObject obj; int gx, gz; bool active; };
    Tile* tiles = new Tile[TN];
    for (int i = 0; i < TN; i++) { tiles[i].active = false; tiles[i].obj.mesh = nullptr; }

    int centerX = 0, centerZ = 0;
    bool dirty = true;

    // ---- Lighting ----
    DirectionalLight sun;
    sun.direction = {1.0f, 1.0f, 0.5f};
    sun.color     = {1.5f, 1.4f, 1.2f};
    eng.SetMainLight(sun);

    // ---- Input ----
    float iP = 0, iY = 0, iR = 0;
    const float ACC = 5.0f, REC = 0.5f;

    // ---- Camera ----
    float  camYaw = 0.0f;
    float3 camOff = {0.0f, 0.7f, -1.8f};

    // ---- Timing ----
    struct timespec lt;
    clock_gettime(CLOCK_MONOTONIC, &lt);

    // ---- Main loop ----
    while (!g_quit) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        float dt = (float)(now.tv_sec - lt.tv_sec)
                 + (float)(now.tv_nsec - lt.tv_nsec) * 0.000000001f;
        lt = now;
        if (dt > 0.1f) dt = 0.1f;

        cam.Update(dt);

        // Input
        float cP = InputCurve(iP, keys['W']||keys['w'], keys['S']||keys['s'], dt, ACC, REC);
        float cY = InputCurve(iY, keys['E']||keys['e'], keys['Q']||keys['q'], dt, ACC, REC);
        float cR = InputCurve(iR, keys['D']||keys['d'], keys['A']||keys['a'], dt, ACC, REC);

        // Flight physics
        quat dP = quat::FromEuler({cP * 1.8f * dt, 0, 0});
        quat dY = quat::FromEuler({0, cY * 1.2f * dt, 0});
        quat dR = quat::FromEuler({0, 0, cR * 2.5f * dt});
        plane.orientation = (plane.orientation * dY * dP * dR).normalizeFast();
        float3 fwd = plane.Forward();
        plane.position = plane.position + fwd * (15.0f * dt);

        // Camera
        float3 hf = {fwd.x, 0, fwd.z};
        float hsq = hf.x*hf.x + hf.z*hf.z;
        if (hsq > 0.001f) {
            float tYaw = he3d_atan2f(hf.x, hf.z);
            camYaw += NormAngle(tYaw - camYaw) * HE3D_MIN(5.0f * dt, 1.0f);
        }
        cam.orientation = quat::FromEuler({0.15f, camYaw, 0});
        quat camYQ = quat::FromEuler({0, camYaw, 0});
        float3 tgtPos = plane.position + camYQ.rotate(camOff);
        float3 diff = tgtPos - cam.position;
        if (diff.lengthSq() > 100.0f) cam.position = tgtPos;
        else cam.position = cam.position + diff * HE3D_MIN(8.0f * dt, 1.0f);

        // Terrain management
        int pGX = (int)he3d_floorf(plane.position.x / TS);
        int pGZ = (int)he3d_floorf(plane.position.z / TS);
        if (pGX != centerX || pGZ != centerZ || dirty) {
            centerX = pGX; centerZ = pGZ; dirty = false;
            struct { int x, z; bool done; } need[25];
            int nCnt = 0;
            for (int dx = -RD; dx <= RD; dx++)
                for (int dz = -RD; dz <= RD; dz++)
                    need[nCnt++] = {centerX + dx, centerZ + dz, false};

            // Recycle out-of-range tiles
            for (int i = 0; i < TN; i++) {
                if (!tiles[i].active) continue;
                bool found = false;
                for (int j = 0; j < nCnt; j++) {
                    if (!need[j].done && tiles[i].gx == need[j].x && tiles[i].gz == need[j].z)
                        { need[j].done = true; found = true; break; }
                }
                if (!found) { delete tiles[i].obj.mesh; tiles[i].obj.mesh = nullptr; tiles[i].active = false; }
            }
            // Fill new tiles
            for (int j = 0; j < nCnt; j++) {
                if (need[j].done) continue;
                for (int i = 0; i < TN; i++) {
                    if (!tiles[i].active) {
                        tiles[i].gx = need[j].x; tiles[i].gz = need[j].z;
                        tiles[i].active = true;
                        float wx = tiles[i].gx * TS, wz = tiles[i].gz * TS;
                        tiles[i].obj.mesh = CreatePlane(GC, GS, wx, wz);
                        tiles[i].obj.position = {wx, 0, wz};
                        need[j].done = true;
                        break;
                    }
                }
            }
        }

        // Render
        eng.Clear({0.45f, 0.75f, 1.0f});
        float3 tc = {0.3f, 0.7f, 0.3f};
        for (int i = 0; i < TN; i++)
            if (tiles[i].active && tiles[i].obj.mesh)
                eng.DrawGameObject(tiles[i].obj, cam, tc);
        if (hasTex) eng.DrawGameObject(plane, cam, *pTex);
        else       eng.DrawGameObject(plane, cam, {0.8f, 0.2f, 0.2f});
        eng.Present();
    }

    // Cleanup
    delete plane.mesh;
    delete pTex;
    for (int i = 0; i < TN; i++) delete tiles[i].obj.mesh;
    delete[] tiles;
    xapi_CloseWindow(win);
    return 0;
}
