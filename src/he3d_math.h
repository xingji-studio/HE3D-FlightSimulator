#pragma once
/*
 * HE3D Engine for 3D - Math Library
 * No standard library dependencies.
 * Performance-first: prefer compiler builtins, everything static inline.
 */

// ============================================================================
// [1] Math functions - self-implemented static inline (no libm dependency)
// ============================================================================

static inline float he3d_fabsf(float x)  { return x < 0.0f ? -x : x; }
static inline int   he3d_abs(int x)      { return x < 0 ? -x : x; }

static inline float he3d_floorf(float x) {
    int xi = (int)x;
    return (float)xi - ((float)xi > x ? 1.0f : 0.0f);
}
static inline float he3d_ceilf(float x) {
    int xi = (int)x;
    return (float)xi + ((float)xi < x ? 1.0f : 0.0f);
}

// sqrt - Newton's method
static inline float he3d_sqrtf(float x) {
    if (x <= 0.0f) return 0.0f;
    float r = x;
    for (int i = 0; i < 12; i++) r = (r + x / r) * 0.5f;
    return r;
}

// sin - Taylor series with range reduction
static inline float he3d_sinf(float x) {
    if (x >  3.141592653589793f) { int n = (int)(x * 0.318309886f); x -= (float)n * 3.141592653589793f; }
    if (x < -3.141592653589793f) { int n = (int)(x * -0.318309886f); x += (float)n * 3.141592653589793f; }
    float x2 = x * x, r = x, t = x;
    r += (t *= x2) * -0.16666666666666666f;
    r += (t *= x2) *  0.008333333333333333f;
    r += (t *= x2) * -0.0001984126984126984f;
    r += (t *= x2) *  0.00000275573192239859f;
    return r;
}

// cos(x) = sin(x + PI/2)
static inline float he3d_cosf(float x) { return he3d_sinf(x + 1.5707963267948966f); }

// tan(x) = sin(x) / cos(x)
static inline float he3d_tanf(float x) { return he3d_sinf(x) / he3d_cosf(x); }

// atan2
static inline float he3d_atan2f(float y, float x) {
    if (he3d_fabsf(x) < 0.000001f)
        return (y > 0.0f) ? 1.57079632679f : -1.57079632679f;
    float z  = y / x;
    float z2 = z * z;
    float at = z * (1.0f + z2 * (-0.3333333333333333f + z2 * (0.2f - z2 * 0.14285714285714285f)));
    if (x < 0.0f) at += (y >= 0.0f) ? 3.141592653589793f : -3.141592653589793f;
    return at;
}

static inline float he3d_roundf(float x) { return he3d_floorf(x + 0.5f); }

// ============================================================================
// [2] Utility macros (performance: use macros not functions in hot paths)
// ============================================================================
#define HE3D_MIN(a, b)         ((a) < (b) ? (a) : (b))
#define HE3D_MAX(a, b)         ((a) > (b) ? (a) : (b))
#define HE3D_CLAMP(x, lo, hi)  (HE3D_MAX(lo, HE3D_MIN(x, hi)))
#define HE3D_ABS(x)            ((x) >= 0 ? (x) : -(x))
#define HE3D_DEG2RAD(d)        ((d) * 0.017453292519943295f)
#define HE3D_PI                3.14159265358979323846f
#define HE3D_TAU               6.28318530717958647692f
#define HE3D_PI_DIV_2          1.57079632679489661923f

// ============================================================================
// [3] float2 - 2D vector (UV coordinates, screen positions)
// ============================================================================
struct float2 {
    float x, y;
    float2() : x(0), y(0) {}
    float2(float _x, float _y) : x(_x), y(_y) {}
    float2 operator+(const float2& v) const { return {x + v.x, y + v.y}; }
    float2 operator-(const float2& v) const { return {x - v.x, y - v.y}; }
    float2 operator*(float s)      const { return {x * s, y * s}; }
    float2 operator*(const float2& v) const { return {x * v.x, y * v.y}; }
    float2 operator/(float s)      const { float inv = 1.0f/s; return {x * inv, y * inv}; }
};

// ============================================================================
// [4] float3 - 3D vector (vertices, colors, normals)
// ============================================================================
struct float3 {
    union {
        struct { float x, y, z; };
        struct { float r, g, b; };
    };

    float3() : x(0), y(0), z(0) {}
    float3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

    // ---- Arithmetic operators ----
    float3 operator+(const float3& v) const { return {x + v.x, y + v.y, z + v.z}; }
    float3 operator-(const float3& v) const { return {x - v.x, y - v.y, z - v.z}; }
    float3 operator*(float s)        const { return {x * s, y * s, z * s}; }
    float3 operator*(const float3& v) const { return {x * v.x, y * v.y, z * v.z}; }
    float3 operator/(float s)        const { float inv = 1.0f/s; return {x * inv, y * inv, z * inv}; }
    float3 operator-()               const { return {-x, -y, -z}; }

    // ---- Geometry ----
    float  lengthSq() const { return x*x + y*y + z*z; }
    float  length()   const { return he3d_sqrtf(lengthSq()); }

    float3 normalize() const {
        float lsq = lengthSq();
        if (lsq < 0.0000001f) return {0,0,0};
        return (*this) * (1.0f / he3d_sqrtf(lsq));
    }

    static float  dot(const float3& a, const float3& b)   { return a.x*b.x + a.y*b.y + a.z*b.z; }
    static float3 cross(const float3& a, const float3& b) {
        return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
    }
    static float3 lerp(const float3& a, const float3& b, float t) { return a + (b - a) * t; }

    // ---- Rotations around axes (Euler) ----
    float3 rotateY(float angle) const {
        float s = he3d_sinf(angle), c = he3d_cosf(angle);
        return {x * c + z * s, y, -x * s + z * c};
    }
    float3 rotateX(float angle) const {
        float s = he3d_sinf(angle), c = he3d_cosf(angle);
        return {x, y * c - z * s, y * s + z * c};
    }
    float3 rotateZ(float angle) const {
        float s = he3d_sinf(angle), c = he3d_cosf(angle);
        return {x * c - y * s, x * s + y * c, z};
    }
};

// ============================================================================
// [5] quat - Quaternion for 3D rotation (no Gimbal lock)
// ============================================================================
struct quat {
    float w, x, y, z;

    quat() : w(1), x(0), y(0), z(0) {}
    quat(float _w, float _x, float _y, float _z) : w(_w), x(_x), y(_y), z(_z) {}

    // ---- Construct from Euler angles (radians) ----
    // Pitch=X, Yaw=Y, Roll=Z
    static quat FromEuler(float3 euler) {
        float c1 = he3d_cosf(euler.y * 0.5f), s1 = he3d_sinf(euler.y * 0.5f); // Yaw
        float c2 = he3d_cosf(euler.x * 0.5f), s2 = he3d_sinf(euler.x * 0.5f); // Pitch
        float c3 = he3d_cosf(euler.z * 0.5f), s3 = he3d_sinf(euler.z * 0.5f); // Roll
        return {
            c1*c2*c3 + s1*s2*s3,
            c1*s2*c3 + s1*c2*s3,
            s1*c2*c3 - c1*s2*s3,
            c1*c2*s3 - s1*s2*c3
        };
    }

    // ---- Normalize to prevent floating-point drift ----
    quat normalize() const {
        float mag = w*w + x*x + y*y + z*z;
        if (mag < 0.0000001f) return {1,0,0,0};
        float inv = 1.0f / he3d_sqrtf(mag);
        return {w*inv, x*inv, y*inv, z*inv};
    }

    // ---- Quaternion multiplication (rotation composition) ----
    quat operator*(const quat& q) const {
        return {
            w*q.w - x*q.x - y*q.y - z*q.z,
            w*q.x + x*q.w + y*q.z - z*q.y,
            w*q.y - x*q.z + y*q.w + z*q.x,
            w*q.z + x*q.y - y*q.x + z*q.w
        };
    }

    // ---- Rotate a 3D vector by this quaternion ----
    float3 rotate(const float3& v) const {
        // Optimized: v' = v + 2*w*(qv x v) + 2*(qv x (qv x v))
        float3 qv    = {x, y, z};
        float3 cross1 = float3::cross(qv, v);
        float3 t     = cross1 * (2.0f * w);
        float3 cross2 = float3::cross(qv, cross1);
        float3 u     = cross2 * 2.0f;
        return {v.x + t.x + u.x, v.y + t.y + u.y, v.z + t.z + u.z};
    }

    // ---- Inverse (for camera view transform) ----
    quat inverse() const { return {w, -x, -y, -z}; }
};

// ============================================================================
// [6] float4x4 - 4x4 matrix (reserved for future use)
// ============================================================================
struct float4x4 {
    float m[16];
    float4x4() {
        for (int i = 0; i < 16; i++) m[i] = 0.0f;
        m[0] = m[5] = m[10] = m[15] = 1.0f;
    }
};
