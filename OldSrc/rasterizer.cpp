#include "rasterizer.h"
#include <sstream>
#include <algorithm>

static float ClampFloat(float value, float minValue, float maxValue) {
    return std::max(minValue, std::min(value, maxValue));
}

// Rasterizer.cpp
RenderTarget::RenderTarget(int w, int h) : Width(w), Height(h) {
    ColourBuffer.resize(w * h, {0.0f, 0.0f, 0.0f});
    // 记得给 DepthBuffer 分配空间，并填入一个很大的数
    DepthBuffer.resize(w * h, 100000.0f); 
}

float3& RenderTarget::GetPixel(int x, int y) {
    return ColourBuffer[y * Width + x];
}

Model::Model(const std::vector<float3>& pts, const std::vector<float3>& cols) 
    : points(pts), colors(cols) {}

float Cross(float2 a, float2 b, float2 p) {
    return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
}

bool PointInTriangle(float2 a, float2 b, float2 c, float2 p) {
    float c1 = Cross(a, b, p);
    float c2 = Cross(b, c, p);
    float c3 = Cross(c, a, p);
    bool hasNeg = (c1 < 0) || (c2 < 0) || (c3 < 0);
    bool hasPos = (c1 > 0) || (c2 > 0) || (c3 > 0);
    return !(hasNeg && hasPos);
}

std::vector<float3> LoadObjFile(std::string objString) {
    std::vector<float3> allPoints;
    std::vector<float3> trianglePoints;
    std::stringstream ss(objString);
    std::string line;

    while (std::getline(ss, line)) {
        if (line.substr(0, 2) == "v ") {
            std::stringstream ls(line.substr(2));
            float3 v;
            ls >> v.x >> v.y >> v.z;
            allPoints.push_back(v);
        } else if (line.substr(0, 2) == "f ") {
            std::stringstream ls(line.substr(2));
            std::string segment;
            std::vector<int> indices;
            while (ls >> segment) {
                indices.push_back(std::stoi(segment.substr(0, segment.find('/'))) - 1);
            }
            for (size_t i = 1; i < indices.size() - 1; i++) {
                trianglePoints.push_back(allPoints[indices[0]]);
                trianglePoints.push_back(allPoints[indices[i]]);
                trianglePoints.push_back(allPoints[indices[i+1]]);
            }
        }
    }
    return trianglePoints;
}

// 辅助函数：计算重心坐标
float3 Barycentric(float2 a, float2 b, float2 c, float2 p) {
    float3 res;
    float det = (b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y);
    res.x = ((b.y - c.y) * (p.x - c.x) + (c.x - b.x) * (p.y - c.y)) / det;
    res.y = ((c.y - a.y) * (p.x - c.x) + (a.x - c.x) * (p.y - c.y)) / det;
    res.z = 1.0f - res.x - res.y;
    return res;
}

void RasterizeTriangle(float3 v0, float3 v1, float3 v2, 
                       float2 p0, float2 p1, float2 p2, 
                       float3 color, RenderTarget& target) {
    int minX = (int)std::max(0.0f, std::min({p0.x, p1.x, p2.x}));
    int maxX = (int)std::min((float)target.Width - 1, std::max({p0.x, p1.x, p2.x}));
    int minY = (int)std::max(0.0f, std::min({p0.y, p1.y, p2.y}));
    int maxY = (int)std::min((float)target.Height - 1, std::max({p0.y, p1.y, p2.y}));

    for (int y = minY; y <= maxY; y++) {
        for (int x = minX; x <= maxX; x++) {
            float2 p = {(float)x, (float)y};
            float3 bary = Barycentric(p0, p1, p2, p);

            // 如果重心坐标都在 [0, 1] 范围内，说明在三角形内
            if (bary.x >= 0 && bary.y >= 0 && bary.z >= 0) {
                // 精确插值当前像素的深度
                float currentZ = bary.x * v0.z + bary.y * v1.z + bary.z * v2.z;
                int idx = y * target.Width + x;

                if (currentZ < target.DepthBuffer[idx]) {
                    target.DepthBuffer[idx] = currentZ;
                    target.GetPixel(x, y) = color;
                }
            }
        }
    }
}

void DrawModel(const Model& model, RenderTarget& target, float angle) {
    
    for (size_t i = 0; i < model.points.size() / 3; i++) {
        float3 v_transformed[3];
        float2 p_screen[3];

        for (int j = 0; j < 3; j++) {
            // 1. 旋转模型
            float3 v = model.points[i * 3 + j].rotateY(angle);
            
            // 2. 平移（非常重要！要把物体移到相机前面，否则 z=0 会导致除以0）
            v.z += 2.5f; 
            v_transformed[j] = v;

            // 3. 透视投影：近大远小的关键
            // 假设视口距离为 1.0
            float screenX = v.x / v.z; 
            float screenY = v.y / v.z;

            // 4. 映射到屏幕像素
            p_screen[j].x = (screenX + 1.0f) * 0.5f * target.Width;
            p_screen[j].y = (1.0f - (screenY + 1.0f) * 0.5f) * target.Height;
        }

        // 调用你写的带 Z-Buffer 的光栅化函数
        RasterizeTriangle(v_transformed[0], v_transformed[1], v_transformed[2],
                          p_screen[0], p_screen[1], p_screen[2], 
                          model.colors[i], target);
    }
}

void DrawModel(const Model& model, RenderTarget& target, const Camera& cam, float3 modelPos) {
    // 1. 预计算常量
    float fovRadians = cam.fov * (3.14159265f / 180.0f);
    float fovScale = 1.0f / tanf(fovRadians / 2.0f);
    float aspect = (float)target.Width / (float)target.Height;
    
    // 简单的光照方向（在世界空间中，光从斜上方射下来）
    float3 lightDir = {0.5f, 1.0f, -0.5f}; 

    for (size_t i = 0; i < model.points.size() / 3; i++) {
        float3 v[3]; // 存储变换后的观察空间顶点

        for (int j = 0; j < 3; j++) {
            // --- [步骤 1] 获取原始顶点 ---
            float3 vertex = model.points[i * 3 + j];

            // --- [步骤 2] 模型空间 -> 世界空间 ---
            // 这里你可以让猴子自转（angle），也可以加上它在世界里的位置
            // 这里我们先只做位移
            vertex.x += modelPos.x;
            vertex.y += modelPos.y;
            vertex.z += modelPos.z;

            // --- [步骤 3] 世界空间 -> 观察空间 (Camera View) ---
            // A. 平移：相对于摄像机的位置
            vertex.x -= cam.position.x;
            vertex.y -= cam.position.y;
            vertex.z -= cam.position.z;

            // B. 旋转：摄像机的逆变换
            // 摄像机抬头，世界就向下转；摄像机右转，世界就向左转
            vertex = vertex.rotateY(-cam.rotation.y);
            vertex = vertex.rotateX(-cam.rotation.x);
            
            v[j] = vertex;
        }

        // --- [步骤 4] 背面剔除 (观察空间) ---
        float3 edge1 = {v[1].x - v[0].x, v[1].y - v[0].y, v[1].z - v[0].z};
        float3 edge2 = {v[2].x - v[0].x, v[2].y - v[0].y, v[2].z - v[0].z};
        float3 normal = {
            edge1.y * edge2.z - edge1.z * edge2.y,
            edge1.z * edge2.x - edge1.x * edge2.z,
            edge1.x * edge2.y - edge1.y * edge2.x
        };

        // 在观察空间，如果法线 Z >= 0，说明面朝向相机背面
        if (normal.z >= 0) continue;

        // --- [步骤 5] 投影与裁剪 ---
        float2 p_screen[3];
        bool isBehindCamera = false;
        for (int j = 0; j < 3; j++) {
            // 近裁剪面检查 (防止除以 0)
            if (v[j].z <= 0.1f) { isBehindCamera = true; break; }

            float projX = (v[j].x / v[j].z) * fovScale / aspect;
            float projY = (v[j].y / v[j].z) * fovScale;

            // 映射到像素坐标 (注意 Y 轴反转)
            p_screen[j].x = (projX + 1.0f) * 0.5f * target.Width;
            p_screen[j].y = (1.0f - (projY + 1.0f) * 0.5f) * target.Height;
        }

        if (isBehindCamera) continue;

        // --- [步骤 6] 简单光照计算 ---
        // 注意：这里的法线和光照计算可以更复杂，目前先用简单的面法线点积
        float nLen = sqrt(normal.x*normal.x + normal.y*normal.y + normal.z*normal.z);
        float dotLight = (normal.z / nLen); // 简化版：越正对相机的越亮
        float3 finalColor = {
            model.colors[i].r * ClampFloat(dotLight, 0.2f, 1.0f),
            model.colors[i].g * ClampFloat(dotLight, 0.2f, 1.0f),
            model.colors[i].b * ClampFloat(dotLight, 0.2f, 1.0f)
        };

        // --- [步骤 7] 绘制三角形 ---
        RasterizeTriangle(v[0], v[1], v[2], p_screen[0], p_screen[1], p_screen[2], finalColor, target);
    }
}

void RenderTarget::Clear(float3 clearColor) {
    // 重置颜色缓冲
    std::fill(ColourBuffer.begin(), ColourBuffer.end(), clearColor);
    // 重置深度缓冲为无穷远
    std::fill(DepthBuffer.begin(), DepthBuffer.end(), 100000.0f);
}