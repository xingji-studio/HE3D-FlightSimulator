#include "Engine.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

// 1. 引入 stb_image (只需在一个 cpp 中定义 IMPLEMENTATION)
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

static float ClampFloat(float value, float minValue, float maxValue) {
    return std::max(minValue, std::min(value, maxValue));
}

static int ClampInt(int value, int minValue, int maxValue) {
    return std::max(minValue, std::min(value, maxValue));
}

// --- 放在 Engine.cpp 底部 ---

// 私有辅助函数，不需要写在 .h 里
static float Internal_GetNoise(int x, int z) {
    int n = x + z * 57;
    n = (n << 13) ^ n;
    return (1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
}

static float Internal_GetHeight(float x, float z) {
    float h = 0;
    h += Internal_GetNoise((int)(x * 0.05f), (int)(z * 0.05f)) * 8.0f; // 大山脉
    h += Internal_GetNoise((int)(x * 0.2f), (int)(z * 0.2f)) * 1.5f;   // 小乱石
    return h;
}

// 在 Engine.h 中修改声明，在 Engine.cpp 中修改实现
Mesh Mesh::CreatePlane(int size, float step, float worldOffsetX, float worldOffsetZ) {
    Mesh mesh;
    float halfSize = (size * step) / 2.0f;

    for (int z = 0; z < size - 1; z++) {
        for (int x = 0; x < size - 1; x++) {
            // 局部坐标
            float lx0 = x * step - halfSize;
            float lz0 = z * step - halfSize;
            float lx1 = (x + 1) * step - halfSize;
            float lz1 = (z + 1) * step - halfSize;

            // 【关键修改】计算该点在世界中的绝对坐标
            float wx0 = lx0 + worldOffsetX;
            float wz0 = lz0 + worldOffsetZ;
            float wx1 = lx1 + worldOffsetX;
            float wz1 = lz1 + worldOffsetZ;

            // 使用世界坐标采样高度，确保边缘对齐
            float3 v1 = { lx0, Internal_GetHeight(wx0, wz0), lz0 };
            float3 v2 = { lx0, Internal_GetHeight(wx0, wz1), lz1 };
            float3 v3 = { lx1, Internal_GetHeight(wx1, wz0), lz0 };
            float3 v4 = { lx1, Internal_GetHeight(wx1, wz1), lz1 };

            // 三角形 1 (保持逆时针顺序)
            mesh.vertices.push_back(v1); mesh.vertices.push_back(v2); mesh.vertices.push_back(v3);
            mesh.uvs.push_back({0, 0}); mesh.uvs.push_back({0, 1}); mesh.uvs.push_back({1, 0});

            // 三角形 2
            mesh.vertices.push_back(v3); mesh.vertices.push_back(v2); mesh.vertices.push_back(v4);
            mesh.uvs.push_back({1, 0}); mesh.uvs.push_back({0, 1}); mesh.uvs.push_back({1, 1});
        }
    }
    return mesh;
}

// --- Texture 类实现 ---
Texture::Texture(const std::string& filename) {
    int channels;
    unsigned char* data = stbi_load(filename.c_str(), &width, &height, &channels, 3);
    if (data) {
        pixels.resize(width * height);
        for (int i = 0; i < width * height; i++) {
            pixels[i] = { data[i*3]/255.0f, data[i*3+1]/255.0f, data[i*3+2]/255.0f };
        }
        stbi_image_free(data);
    } else {
        // --- 起源引擎 2x2 粉黑格子 ---
        width = 64; 
        height = 64;
        pixels.resize(width * height);

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                // 将 64x64 划分为 2x2 的区域
                // row 和 col 只会是 0 或 1
                int col = x / 32; 
                int row = y / 32;

                // 逻辑：(0,0)和(1,1)是粉色，(0,1)和(1,0)是黑色
                // 也就是当 row == col 时为粉色
                if (row == col) {
                    pixels[y * width + x] = { 1.0f, 0.0f, 1.0f }; // 粉色
                } else {
                    pixels[y * width + x] = { 0.0f, 0.0f, 0.0f }; // 黑色
                }
            }
        }
    }
}

float3 Texture::Sample(float u, float v) const {
    u = fmod(u, 1.0f); if (u < 0) u += 1.0f;
    v = fmod(v, 1.0f); if (v < 0) v += 1.0f;
    int x = ClampInt((int)(u * width), 0, width - 1);
    int y = ClampInt((int)(v * height), 0, height - 1);
    return pixels[y * width + x];
}

// --- Mesh 加载实现 ---
Mesh Mesh::LoadOBJ(const std::string& filename) {
    Mesh mesh;
    std::vector<float3> all_v;
    std::vector<float2> all_vt;
    std::ifstream f(filename);
    std::string line;

    while (std::getline(f, line)) {
        if (line.substr(0, 2) == "v ") {
            std::stringstream ss(line.substr(2));
            float3 v; ss >> v.x >> v.y >> v.z;
            all_v.push_back(v);
        } else if (line.substr(0, 3) == "vt ") {
            std::stringstream ss(line.substr(3));
            float2 uv; ss >> uv.x >> uv.y;
            uv.y = 1.0f - uv.y; // 翻转 Y 适配屏幕坐标
            all_vt.push_back(uv);
        } else if (line.substr(0, 2) == "f ") {
            std::stringstream ss(line.substr(2));
            std::string seg;
            struct VInfo { int v, vt; };
            std::vector<VInfo> face;
            while (ss >> seg) {
                size_t s1 = seg.find('/');
                size_t s2 = seg.find('/', s1 + 1);
                int vIdx = std::stoi(seg.substr(0, s1)) - 1;
                int vtIdx = (s1 != std::string::npos && s2 - s1 > 1) ? std::stoi(seg.substr(s1 + 1, s2 - s1 - 1)) - 1 : 0;
                face.push_back({vIdx, vtIdx});
            }
            for (size_t i = 1; i < face.size() - 1; i++) {
                mesh.vertices.push_back(all_v[face[0].v]);
                mesh.vertices.push_back(all_v[face[i].v]);
                mesh.vertices.push_back(all_v[face[i+1].v]);
                if(!all_vt.empty()){
                    mesh.uvs.push_back(all_vt[face[0].vt]);
                    mesh.uvs.push_back(all_vt[face[i].vt]);
                    mesh.uvs.push_back(all_vt[face[i+1].vt]);
                }
            }
        }
    }
    return mesh;
}

// --- 渲染器核心实现 ---
// Engine.cpp

// DrawGameObject 的纯色重载版本
void Renderer::DrawGameObject(const GameObject& obj, const Camera& cam, float3 color) {
    // 1. 光照方向预设
    float3 lightDir = mainLight.direction.normalize();

    float fovRad = cam.fov * 3.14159f / 180.0f;
    float fovScale = 1.0f / tanf(fovRad / 2.0f);
    float aspect = (float)width / height;

    for (size_t i = 0; i < obj.mesh.vertices.size() / 3; i++) {
        float3 v_world[3], v_view[3];
        float2 p_screen[3];
        bool skip = false;

        // --- 顶点变换阶段 ---
        for (int j = 0; j < 3; j++) {
            // --- 1. Model -> World ---
            float3 v = obj.mesh.vertices[i*3+j];
            
            // 【关键修改】使用物体的四元数进行旋转
            v = obj.orientation.rotate(v); 
            
            v = v + obj.position;
            v_world[j] = v;

            // --- 2. World -> View (相机变换) ---
            v = v - cam.position;
            
            // 【关键修改】使用相机四元数的“逆”进行旋转
            // 就像相机往左转，世界看起来就像往右转一样
            v = cam.orientation.inverse().rotate(v);
            
            v_view[j] = v;

            if (v.z <= 0.1f) { skip = true; break; }

            float px = (v.x / v.z) * fovScale / aspect;
            float py = (v.y / v.z) * fovScale;
            p_screen[j] = { (px + 1.0f) * 0.5f * width, (1.0f - (py + 1.0f) * 0.5f) * height };
        }
        if (skip) continue;

        // --- 高效背面剔除 ---
        float area = (p_screen[1].x - p_screen[0].x) * (p_screen[2].y - p_screen[0].y) - 
                     (p_screen[1].y - p_screen[0].y) * (p_screen[2].x - p_screen[0].x);
        if (area <= 0) continue; 

        // --- 光照计算 ---
        float3 edge1 = v_world[1] - v_world[0];
        float3 edge2 = v_world[2] - v_world[0];
        float3 normal = float3::cross(edge1, edge2).normalize();
        
        float diffuse = std::max(0.0f, float3::dot(normal, lightDir));
        float intensity = mainLight.ambient + diffuse;

        // 算出最终的纯色（基础灰 0.8 * 强度）
        float3 finalColor = color * intensity;
        
        // 调用纯色版光栅化
        Rasterize(v_view, p_screen, finalColor);
    }
}

// Rasterize 的纯色重载版本
void Renderer::Rasterize(float3* v_view, float2* p_screen, float3 color) {
    // 1. 确定包围盒
    int minX = std::max(0, (int)std::floor(std::min({p_screen[0].x, p_screen[1].x, p_screen[2].x})));
    int maxX = std::min(width - 1, (int)std::ceil(std::max({p_screen[0].x, p_screen[1].x, p_screen[2].x})));
    int minY = std::max(0, (int)std::floor(std::min({p_screen[0].y, p_screen[1].y, p_screen[2].y})));
    int maxY = std::min(height - 1, (int)std::ceil(std::max({p_screen[0].y, p_screen[1].y, p_screen[2].y})));

    float x0 = p_screen[0].x, y0 = p_screen[0].y;
    float x1 = p_screen[1].x, y1 = p_screen[1].y;
    float x2 = p_screen[2].x, y2 = p_screen[2].y;

    float area = (x1 - x0) * (y2 - y0) - (y1 - y0) * (x2 - x0);
    if (std::abs(area) < 0.0001f) return;
    float invArea = 1.0f / area;

    // 2. 预计算 X 轴增量
    float dw0_dx = (y1 - y2) * invArea;
    float dw1_dx = (y2 - y0) * invArea;

    // 3. 深度插值准备 (1/Z)
    float invZ[3] = { 1.0f / v_view[0].z, 1.0f / v_view[1].z, 1.0f / v_view[2].z };

    // 4. 渲染循环
    for (int y = minY; y <= maxY; y++) {
        // 每一行重新计算准确的起点，彻底杜绝 Y 轴累加误差
        float py = (float)y + 0.5f;
        float px_start = (float)minX + 0.5f;
        
        float w0 = ((x1 - px_start) * (y2 - py) - (y1 - py) * (x2 - px_start)) * invArea;
        float w1 = ((x2 - px_start) * (y0 - py) - (y2 - py) * (x0 - px_start)) * invArea;

        int row_offset = y * width;

        for (int x = minX; x <= maxX; x++) {
            float w2 = 1.0f - w0 - w1;

            // 边缘判定
            if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
                // 插值当前的 1/Z
                float currInvZ = w0 * invZ[0] + w1 * invZ[1] + w2 * invZ[2];
                
                if (currInvZ > 0.000001f) {
                    float currentZ = 1.0f / currInvZ;
                    int idx = row_offset + x;

                    // 深度测试
                    if (currentZ < depthBuffer[idx]) {
                        depthBuffer[idx] = currentZ;
                        colorBuffer[idx] = color; // 这里的 color 是 DrawGameObject 传进来的 (基础色 * 强度)
                    }
                }
            }
            // 步进
            w0 += dw0_dx;
            w1 += dw1_dx;
        }
    }
}

void Renderer::DrawGameObject(const GameObject& obj, const Camera& cam, const Texture& tex) {
    // 预计算投影所需的系数
    float aspect = (float)width / height;
    float fovRad = cam.fov * (3.1415926f / 180.0f);
    float tanHalfFov = tanf(fovRad / 2.0f);

    // 遍历模型的所有三角形
    for (size_t i = 0; i < obj.mesh.vertices.size() / 3; i++) {
        float3 v_world[3], v_view[3];
        float2 uvs[3];

        for (int j = 0; j < 3; j++) {
            float3 v = obj.mesh.vertices[i * 3 + j];
            uvs[j] = obj.mesh.uvs[i * 3 + j];

            // --- 1. Model -> World (物体的旋转与位移) ---
            // 使用物体的四元数直接旋转顶点
            v = obj.orientation.rotate(v); 
            v = v + obj.position;
            v_world[j] = v;

            // --- 2. World -> View (相机的变换) ---
            v = v - cam.position;
            // 相机变换是朝向的逆过程（Inverse Rotation）
            // 如果相机向右看，世界物体相对相机就是向左转
            v = cam.orientation.inverse().rotate(v);
            v_view[j] = v;
        }

        // --- 3. 背面剔除 (Back-face Culling) ---
        // 在相机空间计算法线，判断三角形是否面向相机
        float3 ab = v_view[1] - v_view[0];
        float3 ac = v_view[2] - v_view[0];
        float3 normal = float3::cross(ab, ac);
        if (float3::dot(normal, v_view[0]) >= 0) continue;

        // --- 4. 投影与屏幕映射 ---
        float2 p_screen[3];
        bool clipped = false;
        for (int j = 0; j < 3; j++) {
            // 近平面裁剪简单处理
            if (v_view[j].z <= 0.1f) { clipped = true; break; }

            // 透视投影
            float px = v_view[j].x / (v_view[j].z * tanHalfFov * aspect);
            float py = v_view[j].y / (v_view[j].z * tanHalfFov);

            // 映射到屏幕像素坐标
            p_screen[j].x = (px + 1.0f) * 0.5f * width;
            p_screen[j].y = (1.0f - py) * 0.5f * height;
        }

        if (!clipped) {
            // 简单的光照计算（基于世界空间法线）
            float3 world_ab = v_world[1] - v_world[0];
            float3 world_ac = v_world[2] - v_world[0];
            float3 world_normal = float3::cross(world_ab, world_ac).normalize();
            float light = std::max(0.2f, float3::dot(world_normal, {0, 1, -1})); // 假定一个平行光

            // 调用你的光栅化函数画三角形
            // 改成这行
            this->Rasterize(v_view, p_screen, uvs, light, tex);
        }
    }
}

void Renderer::Rasterize(float3* v_view, float2* p_screen, float2* uvs, float intensity, const Texture& tex) {
    // 1. 包围盒
    int minX = std::max(0, (int)std::floor(std::min({p_screen[0].x, p_screen[1].x, p_screen[2].x})));
    int maxX = std::min(width - 1, (int)std::ceil(std::max({p_screen[0].x, p_screen[1].x, p_screen[2].x})));
    int minY = std::max(0, (int)std::floor(std::min({p_screen[0].y, p_screen[1].y, p_screen[2].y})));
    int maxY = std::min(height - 1, (int)std::ceil(std::max({p_screen[0].y, p_screen[1].y, p_screen[2].y})));

    float x0 = p_screen[0].x, y0 = p_screen[0].y;
    float x1 = p_screen[1].x, y1 = p_screen[1].y;
    float x2 = p_screen[2].x, y2 = p_screen[2].y;

    float area = (x1 - x0) * (y2 - y0) - (y1 - y0) * (x2 - x0);
    if (std::abs(area) < 0.0001f) return;
    float invArea = 1.0f / area;

    // 2. 只需要计算对 X 的偏导数 (抛弃了容易积累误差的 Y 偏导数)
    float dw0_dx = (y1 - y2) * invArea;
    float dw1_dx = (y2 - y0) * invArea;

    // 3. 预计算数据
    float invZ[3] = { 1.0f / v_view[0].z, 1.0f / v_view[1].z, 1.0f / v_view[2].z };
    float2 uvZ[3] = { uvs[0] * invZ[0], uvs[1] * invZ[1], uvs[2] * invZ[2] };
    float3 finalLight = mainLight.color * intensity;

    // 4. 稳如老狗的核心循环
    for (int y = minY; y <= maxY; y++) {
        // 【关键修复 1】：每行开头，老老实实重新计算准确的起点！绝不继承上一行的浮点误差！
        float py = (float)y + 0.5f;
        float px_start = (float)minX + 0.5f;
        
        float w0 = ((x1 - px_start) * (y2 - py) - (y1 - py) * (x2 - px_start)) * invArea;
        float w1 = ((x2 - px_start) * (y0 - py) - (y2 - py) * (x0 - px_start)) * invArea;

        int row_offset = y * width;

        for (int x = minX; x <= maxX; x++) {
            // 【关键修复 2】：强制求得 w2，保证 w0 + w1 + w2 永远等于物理学意义上的 1.0！
            float w2 = 1.0f - w0 - w1;

            // 【关键修复 3】：去掉负数容差，恢复纯净的 >= 0。既然 Z 不错乱了，就不需要容差来掩盖裂缝了。
            if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
                
                float currInvZ = w0 * invZ[0] + w1 * invZ[1] + w2 * invZ[2];
                
                // 防止极端透视下的除零崩溃
                if (currInvZ > 0.000001f) {
                    float currentZ = 1.0f / currInvZ;
                    int idx = row_offset + x;

                    // 正常的深度测试
                    if (currentZ < depthBuffer[idx]) {
                        depthBuffer[idx] = currentZ;
                        
                        // 透视校正 UV
                        float u = (w0 * uvZ[0].x + w1 * uvZ[1].x + w2 * uvZ[2].x) * currentZ;
                        float v = (w0 * uvZ[0].y + w1 * uvZ[1].y + w2 * uvZ[2].y) * currentZ;

                        colorBuffer[idx] = tex.Sample(u, v) * finalLight;
                    }
                }
            }
            
            // X 方向步进：单行最多几百次加法，误差极小，完全可控
            w0 += dw0_dx;
            w1 += dw1_dx;
        }
    }
}

float3 Renderer::CalculateBarycentric(int x, int y, float2* p) {
    float3 c1 = { p[1].x - p[0].x, p[2].x - p[0].x, p[0].x - (float)x };
    float3 c2 = { p[1].y - p[0].y, p[2].y - p[0].y, p[0].y - (float)y };
    float3 u = float3::cross(c1, c2);

    // 如果三角形退化成一条线，返回负值让像素被跳过
    if (std::abs(u.z) < 1.0f) return { -1, 1, 1 };
    
    return { 1.0f - (u.x + u.y) / u.z, u.x / u.z, u.y / u.z };
}

// Engine.cpp
void Renderer::Clear(float3 color) { // 必须有 Renderer::
    // 填充颜色缓存
    std::fill(colorBuffer.begin(), colorBuffer.end(), color);
    // 重置深度缓存（10000.0f 代表远平面）
    std::fill(depthBuffer.begin(), depthBuffer.end(), 10000.0f);
}

// Engine.cpp
void Renderer::Present() { // 必须有 Renderer::
    // 1. 将 float3 (0.0~1.0) 转换为 SDL 接受的 uint32 (ARGB8888)
    for (int i = 0; i < width * height; i++) {
        uint8_t r = (uint8_t)(ClampFloat(colorBuffer[i].x, 0.0f, 1.0f) * 255);
        uint8_t g = (uint8_t)(ClampFloat(colorBuffer[i].y, 0.0f, 1.0f) * 255);
        uint8_t b = (uint8_t)(ClampFloat(colorBuffer[i].z, 0.0f, 1.0f) * 255);
        
        // 拼接成 0xAARRGGBB 格式
        sdlPixels[i] = (255 << 24) | (r << 16) | (g << 8) | b;
    }

    // 2. 更新纹理并将数据推送到显卡（SDL 内部流程）
    SDL_UpdateTexture(sdlTexture, NULL, sdlPixels.data(), width * sizeof(uint32_t));
    SDL_RenderClear(sdlRenderer);
    SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL);
    SDL_RenderPresent(sdlRenderer);
}

// Engine.cpp
// 确保函数名全面带上了 Renderer:: 且参数顺序一致
Renderer::Renderer(SDL_Renderer* renderer, int w, int h) 
    : sdlRenderer(renderer), width(w), height(h) 
{
    colorBuffer.resize(w * h);
    depthBuffer.resize(w * h);
    sdlPixels.resize(w * h);
    
    // 别忘了创建纹理
    sdlTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, 
                                   SDL_TEXTUREACCESS_STREAMING, w, h);
}

// 别忘了析构函数
Renderer::~Renderer() {
    if (sdlTexture) SDL_DestroyTexture(sdlTexture);
}