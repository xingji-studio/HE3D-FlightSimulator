#include "Engine.h"
#include <algorithm>

static float ClampFloat(float value, float minValue, float maxValue) {
    return std::max(minValue, std::min(value, maxValue));
}

#define DEG2RAD(a) ((a) * 3.1415926f / 180.0f)

int main(int argc, char* argv[]) {
    // --- 1. 初始化 ---
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("Flight Simulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, 0);
    SDL_Renderer* sdlRenderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_SetRelativeMouseMode(SDL_TRUE);

    Renderer engine(sdlRenderer, 800, 600);
    Camera cam;

    // 资源加载
    GameObject Biplane;
    Biplane.mesh = Mesh::LoadOBJ("Biplane.obj");
    Texture BiplaneTex("biplane.png");

    // 地形设置
    int renderDistance = 2; // 渲染周围 5x5 的瓦片
    float gridSize = 10.0f;     // 格点间距
    int gridCount = 10;        // 单个瓦片的格点数量
    // float tileSize = gridCount * gridSize; // 计算出单个瓦片的物理尺寸 (120.0f)
    float tileSize = (gridCount - 1) * gridSize;

    GameObject terrain;
    // 传 0, 0 作为初始的世界偏移
    terrain.mesh = Mesh::CreatePlane(gridCount, gridSize, 0.0f, 0.0f);
    
    DirectionalLight sun;
    sun.direction = { 1.0f, 1.0f, 0.5f };
    sun.color = { 1.5f, 1.4f, 1.2f };
    engine.SetMainLight(sun);

    // --- 2. 状态变量 ---
    uint64_t lastTime = SDL_GetPerformanceCounter();
    double freq = (double)SDL_GetPerformanceFrequency();

    float3 cameraPosOffset = { 0.0f, 0.7f, -1.8f }; // 相机相对飞机的后上方偏移
    float smoothedCamYaw = 0.0f;
    bool quit = false;
    SDL_Event e;

    // --- 在循环外定义累积的输入状态 (范围 -1.0 到 1.0) ---
    float inputP = 0, inputY = 0, inputR = 0;

    // 控制参数
    float accumulationSpeed = 5.0f; // 累积速度：按住多久达到最大值
    float recoverySpeed = 0.5f;     // 回弹速度：松开后回弹到 0 的速度
    float exponent = 3.0f;          // 曲线指数：1.0是线性，3.0是经典的飞行非线性曲线

    // --- 3. 主循环 ---
    while (!quit) {
        // A. 时间步
        uint64_t currentTime = SDL_GetPerformanceCounter();
        float deltaTime = (float)((currentTime - lastTime) / freq);
        lastTime = currentTime;

        // 防止物理跳变（例如窗口拖动后 deltaTime 变得巨大）
        if (deltaTime > 0.1f) deltaTime = 0.1f;

        // B. 事件处理
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) quit = true;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) quit = true;
        }

        // C. 输入处理与非线性映射
        const uint8_t* state = SDL_GetKeyboardState(NULL);
        
        auto updateInput = [&](float& current, bool positive, bool negative) {
            float target = 0.0f;
            if (positive) target = 1.0f;
            else if (negative) target = -1.0f;

            if (target != 0) {
                current += target * accumulationSpeed * deltaTime;
            } else {
                if (current > 0) current = std::max(0.0f, current - recoverySpeed * deltaTime);
                else if (current < 0) current = std::min(0.0f, current + recoverySpeed * deltaTime);
            }
            current = ClampFloat(current, -1.0f, 1.0f);

            // 安全映射：防止负数底数导致 pow 出错
            float sign = (current >= 0.0f) ? 1.0f : -1.0f;
            return sign * std::pow(std::abs(current), exponent);
        };

        float coefP = updateInput(inputP, state[SDL_SCANCODE_W], state[SDL_SCANCODE_S]);
        float coefY = updateInput(inputY, state[SDL_SCANCODE_E], state[SDL_SCANCODE_Q]);
        float coefR = updateInput(inputR, state[SDL_SCANCODE_A], state[SDL_SCANCODE_D]);

        // D. 物理与朝向更新
        float p = coefP * 1.8f;
        float y = coefY * 1.2f;
        float r = coefR * 2.5f;

        quat deltaP = quat::FromEuler({ p * deltaTime, 0, 0 });
        quat deltaY = quat::FromEuler({ 0, y * deltaTime, 0 });
        quat deltaR = quat::FromEuler({ 0, 0, r * deltaTime });
        
        // 更新旋转并标准化，防止浮点数漂移
        Biplane.orientation = (Biplane.orientation * deltaY * deltaP * deltaR).normalize();

        // 【修复：关键位移更新】
        // 飞机的局部 Z 轴通常是前方 {0, 0, 1}
        float3 forward = Biplane.orientation.rotate({ 0, 0, 1 });
        float airSpeed = 15.0f; // 调整这个数值来改变飞行速度
        Biplane.position = Biplane.position + forward * airSpeed * deltaTime;

        // E. 相机逻辑（水平跟随）
        float3 planeFwd = Biplane.orientation.rotate({ 0, 0, 1 });
        float3 horizFwd = { planeFwd.x, 0, planeFwd.z };
        float horizLenSq = horizFwd.x * horizFwd.x + horizFwd.z * horizFwd.z;

        // 只有当飞机不是垂直向上/向下时才更新 Yaw，防止相机自旋
        if (horizLenSq > 0.001f) {
            float targetYaw = atan2f(horizFwd.x, horizFwd.z);
            float diff = targetYaw - smoothedCamYaw;
            
            // 角度环绕处理
            while (diff >  3.14159f) diff -= 6.28318f;
            while (diff < -3.14159f) diff += 6.28318f;
            
            smoothedCamYaw += diff * 5.0f * deltaTime;
        }

        quat camYawQuat = quat::FromEuler({ 0, smoothedCamYaw, 0 });
        cam.orientation = quat::FromEuler({ 0.15f, smoothedCamYaw, 0 }); // 0.15 为俯视角
        
        float3 targetCamPos = Biplane.position + camYawQuat.rotate(cameraPosOffset);
        
        // 相机位置平滑插值：如果距离太远（初始状态），直接瞬移
        float distSq = (targetCamPos - cam.position).lengthSq(); // 假设有 lengthSq 函数
        if (distSq > 100.0f) {
            cam.position = targetCamPos;
        } else {
            cam.position = cam.position + (targetCamPos - cam.position) * 8.0f * deltaTime;
        }

        // F. 渲染
        engine.Clear({0.45f, 0.75f, 1.0f}); // 天蓝色背景

        // --- 无限地形渲染（优化后） ---
        int camTileX = (int)floorf(Biplane.position.x / tileSize);
        int camTileZ = (int)floorf(Biplane.position.z / tileSize);

        // --- 渲染循环内 ---
        for (int x = -renderDistance; x <= renderDistance; x++) {
            for (int z = -renderDistance; z <= renderDistance; z++) {
                float worldX = (camTileX + x) * tileSize;
                float worldZ = (camTileZ + z) * tileSize;

                // 【方案 A：实时生成】虽然慢，但能解决连续性问题
                // 注意：这会造成严重的性能问题，每帧都在 new 内存！
                GameObject tempTile;
                tempTile.mesh = Mesh::CreatePlane(gridCount, gridSize, worldX, worldZ);
                tempTile.position = { worldX, 0, worldZ };
                
                engine.DrawGameObject(tempTile, cam, float3(0.3f, 0.7f, 0.3f));
            }
        }

        // 渲染飞机
        engine.DrawGameObject(Biplane, cam, BiplaneTex);
        
        // 提交渲染
        engine.Present();
    }

    SDL_DestroyRenderer(sdlRenderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}