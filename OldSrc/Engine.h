#pragma once
#include "Math.h"
#include <string>
#include <vector>
#include <SDL2/SDL.h>

// --- 1. 定义基础结构 ---

struct DirectionalLight {
    float3 direction = { 0, -1, 1 };
    float3 color = { 1, 1, 1 };
    float ambient = 0.15f;
};

struct Mesh {
    std::vector<float3> vertices;
    std::vector<float2> uvs;

    static Mesh LoadOBJ(const std::string& filename);
    // 新增：生成网格的函数声明
    static Mesh CreatePlane(int size, float step, float worldOffsetX, float worldOffsetZ);
};

struct GameObject {
    Mesh mesh;
    float3 position = { 0, 0, 0 };
    quat orientation = quat(1, 0, 0, 0);
};

struct Camera {
    float3 position = { 0, 0, 5 }; // 初始位置离原点远一点
    quat orientation = quat(1, 0, 0, 0);
    float fov = 90.0f;          // 默认 FOV
    float targetFov = 90.0f;    // 目标 FOV
    float zoomSpeed = 0.2f;     // 平滑缩放的速度 (0.0~1.0)
    // 每帧调用，用来平滑 FOV
    void Update(float deltaTime) {
        // 简单的线性插值 (Lerp) 效果
        fov += (targetFov - fov) * zoomSpeed;
    }
};

// --- 2. 纹理类定义 ---

class Texture {
public:
    int width, height;
    std::vector<float3> pixels;
    Texture(const std::string& filename);
    float3 Sample(float u, float v) const;
};

// --- 3. 渲染器类定义 ---

class Renderer {
private:
    int width, height;
    std::vector<float3> colorBuffer;
    std::vector<float> depthBuffer;
    std::vector<uint32_t> sdlPixels; 

    SDL_Renderer* sdlRenderer;
    SDL_Texture* sdlTexture;

    // 内部计算用的重心坐标函数
    float3 CalculateBarycentric(int x, int y, float2* p);

public:
    DirectionalLight mainLight;

    Renderer(SDL_Renderer* renderer, int w, int h);
    ~Renderer();

    void Clear(float3 color);
    
    // 核心渲染逻辑
    void DrawGameObject(const GameObject& obj, const Camera& cam, float3 color = {0.8f, 0.8f, 0.8f});
    void DrawGameObject(const GameObject& obj, const Camera& cam, const Texture& tex);
    
    // 核心光栅化逻辑 (带纹理和光照)
    void Rasterize(float3* v_view, float2* p_screen, float3 color);
    void Rasterize(float3* v_view, float2* p_screen, float2* uvs, float intensity, const Texture& tex);
    
    void Present();
    
    // 提供光源控制接口
    void SetMainLight(const DirectionalLight& light) { mainLight = light; }
};