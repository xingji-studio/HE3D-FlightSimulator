#pragma once
/*
 * HE3D Engine for 3D - Math Library / HE3D 三维引擎数学库
 *
 * No standard library dependencies. No platform API dependencies.
 * 不依赖标准库，也不依赖平台 API。
 *
 * Optimization strategy / 优化策略:
 *   - Math is self-contained for freestanding XAPI builds.
 *     数学函数自包含，适用于 freestanding XAPI 构建。
 *   - Reciprocal square root uses two Newton steps.
 *     快速平方根倒数使用两次 Newton 迭代。
 *   - sincosf computes sin and cos together for quaternion construction.
 *     sincosf 为四元数构造同时计算 sin 和 cos。
 *   - sin/cos use minimax polynomial approximation.
 *     sin/cos 使用 minimax 多项式近似。
 *
 * Performance-first: no libm dependency, all functions are always_inline.
 * 性能优先：不依赖 libm，所有函数强制内联。
 */

// ============================================================================
// [0] Feature detection and compiler hints / 功能检测和编译器提示
// ============================================================================

// XJ380/XAPI builds are freestanding; libm and math builtins are not required.
// XJ380/XAPI 构建是 freestanding 环境，不要求 libm 或数学内建函数。

#define HE3D_ALWAYS_INLINE static inline __attribute__((always_inline))
#define HE3D_MEMBER_INLINE inline __attribute__((always_inline))
#define HE3D_LIKELY(x) __builtin_expect(!!(x), 1)
#define HE3D_UNLIKELY(x) __builtin_expect(!!(x), 0)

namespace HE3D {

typedef __INT8_TYPE__ int8_t;
typedef __INT16_TYPE__ int16_t;
typedef __INT32_TYPE__ int32_t;
typedef __INT64_TYPE__ int64_t;
typedef __UINT8_TYPE__ uint8_t;
typedef __UINT16_TYPE__ uint16_t;
typedef __UINT32_TYPE__ uint32_t;
typedef __UINT64_TYPE__ uint64_t;

// ============================================================================
// [1] Scalar math: abs, fabs, floor, ceil, round / 标量数学函数
// ============================================================================

HE3D_ALWAYS_INLINE float fabsf(float x) {
    return x < 0.0f ? -x : x;
}

HE3D_ALWAYS_INLINE int32_t abs(int32_t x) {
    return x < 0 ? -x : x;
}

HE3D_ALWAYS_INLINE float floorf(float x) {
    int32_t xi = (int32_t)x;
    // Correct the truncation-toward-zero cast for negative fractions.
    // 修正负小数转整数时向零截断造成的 floor 偏差。
    return (float)xi - (float)(xi > x);
}

HE3D_ALWAYS_INLINE float ceilf(float x) {
    int32_t xi = (int32_t)x;
    return (float)xi + (float)(xi < x);
}

HE3D_ALWAYS_INLINE float roundf(float x) {
    return floorf(x + 0.5f);
}

// Fractional part for wrapped texture coordinates.
// 用于纹理坐标环绕的小数部分。
HE3D_ALWAYS_INLINE float fracf(float x) {
    float xi = (float)(int32_t)x;
    float frac = x - xi;
    return frac < 0.0f ? frac + 1.0f : frac;
}

// ============================================================================
// [2] sqrt: public sqrt via reciprocal sqrt, plus precise helper
// [2] sqrt：公开 sqrt 使用快速平方根倒数，另保留高精度辅助函数
// ============================================================================

// Precise software sqrt: exponent-based seed plus five Newton iterations.
// 高精度软件 sqrt：使用指数位生成初值，再执行五次 Newton 迭代。
HE3D_ALWAYS_INLINE float sqrtf_precise(float x) {
    if (HE3D_UNLIKELY(x <= 0.0f))
        return 0.0f;
    // Initial guess: halve the exponent and keep the mantissa.
    // 初值：指数减半，保留尾数。
    union {
        float f;
        int32_t i;
    } u;
    u.f = x;
    u.i = (u.i >> 1) + 0x1FC00000; // exponent-based sqrt seed / 基于指数位的 sqrt 初值
    float r = u.f;
    // Five Newton iterations: r = (r + x/r) * 0.5.
    // 五次 Newton 迭代：r = (r + x/r) * 0.5。
    r = (r + x / r) * 0.5f;
    r = (r + x / r) * 0.5f;
    r = (r + x / r) * 0.5f;
    r = (r + x / r) * 0.5f;
    r = (r + x / r) * 0.5f;
    return r;
}

// ============================================================================
// [3] Reciprocal sqrt / 平方根倒数
// ============================================================================
// Bit-level seed plus two Newton iterations for normalizeFast().
// 位级初值加两次 Newton 迭代，用于 normalizeFast()。

HE3D_ALWAYS_INLINE float rsqrtf(float x) {
    if (HE3D_UNLIKELY(x <= 0.0f))
        return 0.0f;
    float x2 = x * 0.5f;
    union {
        float f;
        uint32_t i;
    } u;
    u.f = x;
    u.i = 0x5f3759dfu - (u.i >> 1);
    float y = u.f;
    y = y * (1.5f - (x2 * y * y));
    y = y * (1.5f - (x2 * y * y));
    return y;
}

HE3D_ALWAYS_INLINE float sqrtf(float x) {
    if (HE3D_UNLIKELY(x <= 0.0f))
        return 0.0f;
    return 1.0f / rsqrtf(x);
}

// ============================================================================
// [4] sin, cos, tan: minimax polynomial software path / 三角函数软件实现
// ============================================================================

// Software sin/cos use a minimax polynomial after range reduction.
// 软件 sin/cos 先做范围规约，再使用 minimax 多项式。
HE3D_ALWAYS_INLINE float sinf(float x) {
    // Reduce to [-PI, PI] with period TAU.
    // 用 TAU 作为周期，将角度规约到 [-PI, PI]。
    if (x > 3.141592653589793f) {
        int32_t n = (int32_t)(x * 0.15915494309189535f + 0.5f);
        x -= (float)n * 6.283185307179586f;
    }
    if (x < -3.141592653589793f) {
        int32_t n = (int32_t)(x * -0.15915494309189535f + 0.5f);
        x += (float)n * 6.283185307179586f;
    }
    // Mirror to [-PI/2, PI/2] before evaluating the polynomial.
    // 计算多项式前先镜像到 [-PI/2, PI/2]。
    if (x > 1.5707963267948966f) {
        x = 3.141592653589793f - x;
    } else if (x < -1.5707963267948966f) {
        x = -3.141592653589793f - x;
    }

    // x is now in [-PI/2, PI/2]; evaluate the minimax polynomial.
    // 此时 x 位于 [-PI/2, PI/2]，开始计算 minimax 多项式。
    float x2 = x * x;
    float r = x;
    r += x * x2 * -0.16666656732559204f;       // minimax term near -1/3! / 接近 -1/3! 的 minimax 项
    r += x * x2 * x2 * 0.0083330258358717f;    // minimax term near  1/5! / 接近  1/5! 的 minimax 项
    r += x * x2 * x2 * x2 * -0.0001980740614f; // minimax term near -1/7! / 接近 -1/7! 的 minimax 项
    return r;
}

HE3D_ALWAYS_INLINE float cosf(float x) {
    return sinf(x + 1.5707963267948966f);
}

HE3D_ALWAYS_INLINE float tanf(float x) {
    return sinf(x) / cosf(x);
}

// ============================================================================
// [5] sincos: compute sin and cos in one call / 一次调用同时计算 sin 和 cos
// ============================================================================
// Quaternion construction uses three sincosf calls instead of six trig calls.
// 四元数构造使用三次 sincosf，而不是六次三角函数调用。

HE3D_ALWAYS_INLINE void sincosf(float x, float *s, float *c) {
    *s = sinf(x);
    *c = cosf(x);
}

// ============================================================================
// [6] atan2: rational approximation with quadrant correction / 带象限修正的有理近似
// ============================================================================

HE3D_ALWAYS_INLINE float atan2f(float y, float x) {
    float ax = fabsf(x);
    if (ax < 0.000001f) {
        if (fabsf(y) < 0.000001f)
            return 0.0f;
        return (y > 0.0f) ? 1.57079632679f : -1.57079632679f;
    }

    // Minimax rational approximation for atan(z) on [0, 1].
    // atan(z) 在 [0, 1] 上的 minimax 有理近似。
    float z = y / x;
    float absZ = fabsf(z);
    int32_t inv = (absZ > 1.0f);
    if (inv)
        z = 1.0f / z;

    float z2 = z * z;
    // Coefficients generated for this approximation.
    // 该近似式使用的系数。
    float at = z * (1.0f + z2 * (-0.333333283662796f + z2 * (0.199993550777435f + z2 * (-0.142028367400169f + z2 *
                                                                                                                  0.10608633607626f))));

    if (inv)
        at = (z > 0.0f ? 1.57079632679f : -1.57079632679f) - at;
    if (x < 0.0f)
        at += (y >= 0.0f) ? 3.141592653589793f : -3.141592653589793f;
    return at;
}

// ============================================================================
// [7] Utility macros / 工具宏
// ============================================================================
#define HE3D_MIN(a, b) ((a) < (b) ? (a) : (b))
#define HE3D_MAX(a, b) ((a) > (b) ? (a) : (b))
#define HE3D_CLAMP(x, lo, hi) (HE3D_MAX(lo, HE3D_MIN(x, hi)))
#define HE3D_ABS(x) ((x) >= 0 ? (x) : -(x))
#define HE3D_DEG2RAD(d) ((d) * 0.017453292519943295f)

// Precomputed constants used by math, transforms, and projection code.
// 数学、变换和投影代码使用的预计算常量。
static const float HE3D_PI = 3.14159265358979323846f;
static const float HE3D_TAU = 6.28318530717958647692f;
static const float HE3D_PI_DIV_2 = 1.57079632679489661923f;

// ============================================================================
// [8] float2: 2D vector for UVs and screen positions / 二维向量，用于 UV 和屏幕坐标
// ============================================================================
struct float2 {
    float x, y;

    HE3D_MEMBER_INLINE float2(float _x = 0, float _y = 0) : x(_x), y(_y) {}

    HE3D_MEMBER_INLINE float2 operator+(const float2 &v) const { return {x + v.x, y + v.y}; }
    HE3D_MEMBER_INLINE float2 operator-(const float2 &v) const { return {x - v.x, y - v.y}; }
    HE3D_MEMBER_INLINE float2 operator*(float s) const { return {x * s, y * s}; }
    HE3D_MEMBER_INLINE float2 operator*(const float2 &v) const { return {x * v.x, y * v.y}; }
    HE3D_MEMBER_INLINE float2 operator/(float s) const {
        float inv = 1.0f / s;
        return {x * inv, y * inv};
    }
};

// ============================================================================
// [9] float3: 3D vector for vertices, normals, and positions / 三维向量，用于顶点、法线和位置
// ============================================================================
struct float3 {
    float x, y, z;

    HE3D_MEMBER_INLINE float3(float _x = 0, float _y = 0, float _z = 0) : x(_x), y(_y), z(_z) {}

    // Arithmetic operators / 算术运算符
    HE3D_MEMBER_INLINE float3 operator+(const float3 &v) const { return {x + v.x, y + v.y, z + v.z}; }
    HE3D_MEMBER_INLINE float3 operator-(const float3 &v) const { return {x - v.x, y - v.y, z - v.z}; }
    HE3D_MEMBER_INLINE float3 operator*(float s) const { return {x * s, y * s, z * s}; }
    HE3D_MEMBER_INLINE float3 operator*(const float3 &v) const { return {x * v.x, y * v.y, z * v.z}; }
    HE3D_MEMBER_INLINE float3 operator/(float s) const {
        float inv = 1.0f / s;
        return {x * inv, y * inv, z * inv};
    }
    HE3D_MEMBER_INLINE float3 operator-() const { return {-x, -y, -z}; }

    // Geometry helpers / 几何辅助函数
    HE3D_MEMBER_INLINE float lengthSq() const { return x * x + y * y + z * z; }
    HE3D_MEMBER_INLINE float length() const { return sqrtf(lengthSq()); }

    // Standard normalize: sqrt plus divide.
    // 标准归一化：sqrt 加除法。
    HE3D_MEMBER_INLINE float3 normalize() const {
        float lsq = lengthSq();
        if (HE3D_UNLIKELY(lsq < 0.0000001f))
            return {0, 0, 0};
        return (*this) * (1.0f / sqrtf(lsq));
    }

    // Fast normalize: use the reciprocal-square-root path.
    // 快速归一化：使用快速平方根倒数路径。
    HE3D_MEMBER_INLINE float3 normalizeFast() const {
        float lsq = lengthSq();
        if (HE3D_UNLIKELY(lsq < 0.0000001f))
            return {0, 0, 0};
        return (*this) * rsqrtf(lsq);
    }

    HE3D_MEMBER_INLINE static float dot(const float3 &a, const float3 &b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    HE3D_MEMBER_INLINE static float3 cross(const float3 &a, const float3 &b) {
        return {
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
    }

    HE3D_MEMBER_INLINE static float3 lerp(const float3 &a, const float3 &b, float t) {
        return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
    }

    // Axis rotations with caller-provided sin/cos.
    // 使用调用方预计算 sin/cos 的轴旋转。
    HE3D_MEMBER_INLINE float3 rotateY(float s, float c) const {
        return {x * c + z * s, y, -x * s + z * c};
    }
    HE3D_MEMBER_INLINE float3 rotateX(float s, float c) const {
        return {x, y * c - z * s, y * s + z * c};
    }
    HE3D_MEMBER_INLINE float3 rotateZ(float s, float c) const {
        return {x * c - y * s, x * s + y * c, z};
    }

    // Axis rotations that compute sin/cos internally.
    // 内部计算 sin/cos 的轴旋转。
    HE3D_MEMBER_INLINE float3 rotateY(float angle) const {
        float s, c;
        sincosf(angle, &s, &c);
        return rotateY(s, c);
    }
    HE3D_MEMBER_INLINE float3 rotateX(float angle) const {
        float s, c;
        sincosf(angle, &s, &c);
        return rotateX(s, c);
    }
    HE3D_MEMBER_INLINE float3 rotateZ(float angle) const {
        float s, c;
        sincosf(angle, &s, &c);
        return rotateZ(s, c);
    }
};

// ============================================================================
// [10] color3: linear RGB color / 线性 RGB 颜色
// ============================================================================
struct color3 {
    float r, g, b;

    HE3D_MEMBER_INLINE color3(float _r = 0, float _g = 0, float _b = 0)
        : r(_r), g(_g), b(_b) {}

    HE3D_MEMBER_INLINE color3 operator+(const color3 &c) const { return {r + c.r, g + c.g, b + c.b}; }
    HE3D_MEMBER_INLINE color3 operator-(const color3 &c) const { return {r - c.r, g - c.g, b - c.b}; }
    HE3D_MEMBER_INLINE color3 operator*(float s) const { return {r * s, g * s, b * s}; }
    HE3D_MEMBER_INLINE color3 operator*(const color3 &c) const { return {r * c.r, g * c.g, b * c.b}; }
    HE3D_MEMBER_INLINE color3 operator/(float s) const {
        float inv = 1.0f / s;
        return {r * inv, g * inv, b * inv};
    }
};

// ============================================================================
// [11] Ray: 3D query helper / 三维查询辅助类型
// ============================================================================
struct RayHit {
    bool hit;
    float distance;
    float3 position;
    float3 normal;
    float u;
    float v;

    HE3D_MEMBER_INLINE RayHit()
        : hit(false), distance(0.0f), position{0, 0, 0}, normal{0, 0, 0}, u(0.0f), v(0.0f) {}
};

struct AABB {
    float3 min;
    float3 max;

    HE3D_MEMBER_INLINE AABB(float3 _min = {0, 0, 0}, float3 _max = {0, 0, 0})
        : min(_min), max(_max) {}

    HE3D_MEMBER_INLINE bool Intersects(const AABB &other) const {
        return !(max.x < other.min.x || min.x > other.max.x ||
                 max.y < other.min.y || min.y > other.max.y ||
                 max.z < other.min.z || min.z > other.max.z);
    }
};

struct Ray {
    float3 origin;
    float3 direction;

    HE3D_MEMBER_INLINE Ray(float3 _origin = {0, 0, 0}, float3 _direction = {0, 0, 1})
        : origin(_origin), direction(_direction) {}

    HE3D_MEMBER_INLINE static Ray FromTo(float3 from, float3 to) {
        return Ray(from, (to - from).normalizeFast());
    }

    HE3D_MEMBER_INLINE Ray Normalized() const {
        return Ray(origin, direction.normalizeFast());
    }

    HE3D_MEMBER_INLINE float3 At(float distance) const {
        return origin + direction * distance;
    }

    HE3D_MEMBER_INLINE bool IntersectSphere(float3 center, float radius, float *outDistance = nullptr) const {
        float3 oc = origin - center;
        float a = float3::dot(direction, direction);
        if (HE3D_UNLIKELY(a < 0.0000001f))
            return false;
        float b = 2.0f * float3::dot(oc, direction);
        float c = float3::dot(oc, oc) - radius * radius;
        float discriminant = b * b - 4.0f * a * c;
        if (discriminant < 0.0f)
            return false;

        float root = sqrtf(discriminant);
        float invDenom = 0.5f / a;
        float t = (-b - root) * invDenom;
        if (t < 0.0f)
            t = (-b + root) * invDenom;
        if (t < 0.0f)
            return false;

        if (outDistance)
            *outDistance = t;
        return true;
    }

    HE3D_MEMBER_INLINE bool IntersectPlane(float3 point, float3 normal, float *outDistance = nullptr) const {
        float denom = float3::dot(normal, direction);
        if (fabsf(denom) < 0.000001f)
            return false;

        float t = float3::dot(point - origin, normal) / denom;
        if (t < 0.0f)
            return false;

        if (outDistance)
            *outDistance = t;
        return true;
    }

    HE3D_MEMBER_INLINE bool IntersectTriangle(float3 v0, float3 v1, float3 v2,
                                              float *outDistance = nullptr,
                                              float *outU = nullptr,
                                              float *outV = nullptr) const {
        float3 edge1 = v1 - v0;
        float3 edge2 = v2 - v0;
        float3 pvec = float3::cross(direction, edge2);
        float det = float3::dot(edge1, pvec);
        if (fabsf(det) < 0.000001f)
            return false;

        float invDet = 1.0f / det;
        float3 tvec = origin - v0;
        float u = float3::dot(tvec, pvec) * invDet;
        if (u < 0.0f || u > 1.0f)
            return false;

        float3 qvec = float3::cross(tvec, edge1);
        float v = float3::dot(direction, qvec) * invDet;
        if (v < 0.0f || u + v > 1.0f)
            return false;

        float t = float3::dot(edge2, qvec) * invDet;
        if (t < 0.0f)
            return false;

        if (outDistance)
            *outDistance = t;
        if (outU)
            *outU = u;
        if (outV)
            *outV = v;
        return true;
    }

    HE3D_MEMBER_INLINE bool IntersectAABB(const AABB &box,
                                          float *outNear = nullptr,
                                          float *outFar = nullptr) const {
        float tmin = 0.0f;
        float tmax = 340282346638528859811704183484516925440.0f;

#define HE3D_RAY_AABB_AXIS(originAxis, directionAxis, minAxis, maxAxis) \
    do {                                                                \
        if (fabsf(directionAxis) < 0.000001f) {                         \
            if ((originAxis) < (minAxis) || (originAxis) > (maxAxis))   \
                return false;                                           \
        } else {                                                        \
            float invD = 1.0f / (directionAxis);                        \
            float t0 = ((minAxis) - (originAxis)) * invD;               \
            float t1 = ((maxAxis) - (originAxis)) * invD;               \
            if (t0 > t1) {                                              \
                float tmp = t0;                                         \
                t0 = t1;                                                \
                t1 = tmp;                                               \
            }                                                           \
            if (t0 > tmin)                                              \
                tmin = t0;                                              \
            if (t1 < tmax)                                              \
                tmax = t1;                                              \
            if (tmax < tmin)                                            \
                return false;                                           \
        }                                                               \
    } while (0)

        HE3D_RAY_AABB_AXIS(origin.x, direction.x, box.min.x, box.max.x);
        HE3D_RAY_AABB_AXIS(origin.y, direction.y, box.min.y, box.max.y);
        HE3D_RAY_AABB_AXIS(origin.z, direction.z, box.min.z, box.max.z);

#undef HE3D_RAY_AABB_AXIS

        if (outNear)
            *outNear = tmin;
        if (outFar)
            *outFar = tmax;
        return true;
    }

    HE3D_MEMBER_INLINE RayHit CastSphere(float3 center, float radius) const {
        RayHit result;
        if (!IntersectSphere(center, radius, &result.distance))
            return result;
        result.hit = true;
        result.position = At(result.distance);
        result.normal = (result.position - center).normalizeFast();
        return result;
    }

    HE3D_MEMBER_INLINE RayHit CastPlane(float3 point, float3 normal) const {
        RayHit result;
        if (!IntersectPlane(point, normal, &result.distance))
            return result;
        result.hit = true;
        result.position = At(result.distance);
        result.normal = normal.normalizeFast();
        return result;
    }

    HE3D_MEMBER_INLINE RayHit CastTriangle(float3 v0, float3 v1, float3 v2) const {
        RayHit result;
        if (!IntersectTriangle(v0, v1, v2, &result.distance, &result.u, &result.v))
            return result;
        result.hit = true;
        result.position = At(result.distance);
        result.normal = float3::cross(v1 - v0, v2 - v0).normalizeFast();
        return result;
    }
};

// ============================================================================
// [12] quat: quaternion rotation / 四元数旋转
// ============================================================================
struct quat {
    float w, x, y, z;

    HE3D_MEMBER_INLINE quat(float _w = 1, float _x = 0, float _y = 0, float _z = 0)
        : w(_w), x(_x), y(_y), z(_z) {}

    // Construct from Euler angles in radians. Pitch=X, Yaw=Y, Roll=Z.
    // 从弧度制欧拉角构造。Pitch=X，Yaw=Y，Roll=Z。
    HE3D_MEMBER_INLINE static quat FromEuler(float3 euler) {
        float sy, cy, sp, cp, sr, cr;
        sincosf(euler.y * 0.5f, &sy, &cy); // Yaw / 偏航
        sincosf(euler.x * 0.5f, &sp, &cp); // Pitch / 俯仰
        sincosf(euler.z * 0.5f, &sr, &cr); // Roll / 滚转
        return {
            cy * cp * cr + sy * sp * sr,
            cy * sp * cr + sy * cp * sr,
            sy * cp * cr - cy * sp * sr,
            cy * cp * sr - sy * sp * cr};
    }

    // Fast Euler construction; kept for call sites that prefer the fast path.
    // 快速欧拉角构造；用于明确选择快速路径的调用点。
    HE3D_MEMBER_INLINE static quat FromEulerFast(float3 euler) {
        float sy = sinf(euler.y * 0.5f), cy = cosf(euler.y * 0.5f);
        float sp = sinf(euler.x * 0.5f), cp = cosf(euler.x * 0.5f);
        float sr = sinf(euler.z * 0.5f), cr = cosf(euler.z * 0.5f);
        return {
            cy * cp * cr + sy * sp * sr,
            cy * sp * cr + sy * cp * sr,
            sy * cp * cr - cy * sp * sr,
            cy * cp * sr - sy * sp * cr};
    }

    // Normalize to limit floating-point drift.
    // 归一化以限制浮点误差漂移。
    HE3D_MEMBER_INLINE quat normalize() const {
        float mag = w * w + x * x + y * y + z * z;
        if (HE3D_UNLIKELY(mag < 0.0000001f))
            return {1, 0, 0, 0};
        float inv = 1.0f / sqrtf(mag);
        return {w * inv, x * inv, y * inv, z * inv};
    }

    // Fast normalize using the reciprocal-square-root path.
    // 使用快速平方根倒数路径进行快速归一化。
    HE3D_MEMBER_INLINE quat normalizeFast() const {
        float mag = w * w + x * x + y * y + z * z;
        if (HE3D_UNLIKELY(mag < 0.0000001f))
            return {1, 0, 0, 0};
        float inv = rsqrtf(mag);
        return {w * inv, x * inv, y * inv, z * inv};
    }

    // Quaternion multiplication for rotation composition.
    // 四元数乘法，用于组合旋转。
    HE3D_MEMBER_INLINE quat operator*(const quat &q) const {
        return {
            w * q.w - x * q.x - y * q.y - z * q.z,
            w * q.x + x * q.w + y * q.z - z * q.y,
            w * q.y - x * q.z + y * q.w + z * q.x,
            w * q.z + x * q.y - y * q.x + z * q.w};
    }

    // Rotate a vector by this quaternion.
    // 使用该四元数旋转向量。
    HE3D_MEMBER_INLINE float3 rotate(const float3 &v) const {
        float3 qv = {x, y, z};
        float twoW = 2.0f * w;
        float3 cross1 = float3::cross(qv, v);
        float3 cross2 = float3::cross(qv, cross1);
        return {
            v.x + twoW * cross1.x + 2.0f * cross2.x,
            v.y + twoW * cross1.y + 2.0f * cross2.y,
            v.z + twoW * cross1.z + 2.0f * cross2.z};
    }

    // Inverse for unit quaternions is the conjugate.
    // 单位四元数的逆等于共轭。
    HE3D_MEMBER_INLINE quat inverse() const { return {w, -x, -y, -z}; }
};

// ============================================================================
// [13] float4x4: 4x4 matrix / 4x4 矩阵
// ============================================================================
struct float4x4 {
    float m[16];

    HE3D_MEMBER_INLINE float4x4() {
        for (int32_t i = 0; i < 16; i++)
            m[i] = 0.0f;
        m[0] = m[5] = m[10] = m[15] = 1.0f;
    }
};

} // namespace HE3D
