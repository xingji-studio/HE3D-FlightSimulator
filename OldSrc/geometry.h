#pragma once
#include <cmath>

struct float2 { float x, y; };

struct float3 {
    union {
        struct { float x, y, z; };
        struct { float r, g, b; };
    };

    // 绕 Y 轴旋转 (水平旋转)
    float3 rotateY(float angle) const {
        float s = sin(angle);
        float c = cos(angle);
        return {
            x * c + z * s,
            y,
            -x * s + z * c
        };
    }

    // 绕 X 轴旋转 (上下翻转)
    float3 rotateX(float angle) const {
        float s = sin(angle);
        float c = cos(angle);
        return {
            x,
            y * c - z * s,
            y * s + z * c
        };
    }
};

struct Camera {
    float3 position = {0, 0, 0};
    float3 rotation = {0, 0, 0}; // 摄像机的欧拉角
    float fov = 60.0f;
};