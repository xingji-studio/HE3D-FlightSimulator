#pragma once
#include <cmath>
#include <algorithm>
#include <vector>

// 二维向量：主要用于 UV 坐标
struct float2 {
    float x, y;

    float2(float _x = 0, float _y = 0) : x(_x), y(_y) {}

    // --- 关键修正：添加运算符重载 ---
    
    // 支持 float2 * float (例如：uv * invZ)
    float2 operator*(float s) const { 
        return { x * s, y * s }; 
    }

    // 支持 float2 + float2 (如果你在插值时用了加法)
    float2 operator+(const float2& v) const { 
        return { x + v.x, y + v.y }; 
    }
};

struct float3 {
    float x, y, z;
    float3(float _x = 0, float _y = 0, float _z = 0) : x(_x), y(_y), z(_z) {}

    // --- 补全运算符 ---
    float3 operator+(const float3& v) const { return { x + v.x, y + v.y, z + v.z }; }
    float3 operator-(const float3& v) const { return { x - v.x, y - v.y, z - v.z }; }
    float3 operator*(float s) const { return { x * s, y * s, z * s }; }
    float3 operator*(const float3& v) const { return { x * v.x, y * v.y, z * v.z }; }

    // --- 核心几何属性 ---
    // 长度平方：用于距离比较，省去 sqrt 开销
    float lengthSq() const { return x * x + y * y + z * z; } 
    
    float length() const { return sqrtf(lengthSq()); }

    float3 normalize() const {
        float len = length();
        return len > 0.00001f ? (*this) * (1.0f / len) : float3{ 0, 0, 0 };
    }

    // --- 核心几何函数 ---
    static float dot(const float3& a, const float3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    static float3 cross(const float3& a, const float3& b) {
        return {
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        };
    }

    // --- 旋转数学 ---
    // 绕 Y 轴旋转（偏航角 Yaw）
    float3 rotateY(float angle) const {
        float s = sinf(angle), c = cosf(angle);
        return { x * c + z * s, y, -x * s + z * c };
    }

    // 绕 X 轴旋转（俯仰角 Pitch）
    float3 rotateX(float angle) const {
        float s = sinf(angle), c = cosf(angle);
        return { x, y * c - z * s, y * s + z * c };
    }

    // 绕 Z 轴旋转（翻滚角 Roll）
    float3 rotateZ(float angle) const {
        float s = sinf(angle), c = cosf(angle);
        return { x * c - y * s, x * s + y * c, z };
    }
};

struct quat {
    float w, x, y, z;
    quat(float _w = 1, float _x = 0, float _y = 0, float _z = 0) : w(_w), x(_x), y(_y), z(_z) {}

    // 在 Math.h 的 quat 结构体内部
    static quat FromEuler(float3 e) {
        // 假设 e.x=Pitch, e.y=Yaw, e.z=Roll (弧度制)
        float c1 = cosf(e.y * 0.5f), s1 = sinf(e.y * 0.5f); // Yaw
        float c2 = cosf(e.x * 0.5f), s2 = sinf(e.x * 0.5f); // Pitch
        float c3 = cosf(e.z * 0.5f), s3 = sinf(e.z * 0.5f); // Roll

        return quat(
            c1 * c2 * c3 + s1 * s2 * s3, // w
            c1 * s2 * c3 + s1 * c2 * s3, // x
            s1 * c2 * c3 - c1 * s2 * s3, // y
            c1 * c2 * s3 - s1 * s2 * c3  // z
        );
    }

    // --- 添加这个函数 ---
    quat normalize() const {
        float mag = sqrtf(w * w + x * x + y * y + z * z);
        if (mag > 0.00001f) {
            float invMag = 1.0f / mag;
            return quat(w * invMag, x * invMag, y * invMag, z * invMag);
        }
        return quat(1, 0, 0, 0); // 如果模长太小，返回恒等旋转
    }

    // 【必须有】四元数乘法 (用于旋转叠加)
    quat operator*(const quat& q) const {
        return quat(
            w * q.w - x * q.x - y * q.y - z * q.z,
            w * q.x + x * q.w + y * q.z - z * q.y,
            w * q.y - x * q.z + y * q.w + z * q.x,
            w * q.z + x * q.y - y * q.x + z * q.w
        );
    }

    // 【必须有】旋转向量 (用于顶点变换和计算飞行方向)
    float3 rotate(float3 v) const {
        float3 qv = {x, y, z};
        float3 t = float3::cross(qv, v) * 2.0f;
        return v + t * w + float3::cross(qv, t);
    }

    // 【必须有】求逆 (用于相机 View 变换)
    quat inverse() const { return quat(w, -x, -y, -z); }
};