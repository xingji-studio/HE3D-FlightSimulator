#pragma once

#include "geometry.h"
#include <vector>
#include <string>

// Rasterizer.h
class RenderTarget {
public:
    std::vector<float3> ColourBuffer;
    std::vector<float> DepthBuffer; 
    int Width, Height;

    // 只留声明，不要大括号里的内容
    RenderTarget(int w, int h); 

    float3& GetPixel(int x, int y);
    void Clear(float3 clearColor);
};

struct Model {
    std::vector<float3> points;
    std::vector<float3> colors;
    Model(const std::vector<float3>& pts, const std::vector<float3>& cols);
};

// 渲染核心函数
std::vector<float3> LoadObjFile(std::string objString);
void DrawModel(const Model& model, RenderTarget& target, float angle = 0.0f);
void DrawModel(const Model& model, RenderTarget& target, const Camera& cam, float3 modelPos);
bool PointInTriangle(float2 a, float2 b, float2 c, float2 p);