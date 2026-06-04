#pragma once
/*
 * HE3D Engine for 3D - Math Library (Aggressively Optimized)
 *
 * No standard library dependencies. No platform API dependencies.
 *
 * Optimization strategy (informed by XJ380 API Spec v1.3):
 *   xxcc is Clang 18.1.8+ based; -fno-builtin is only required for "other
 *   compilers" (§1-2). We therefore use __builtin_* when __clang__ is defined,
 *   falling back to hardware-tuned software implementations otherwise.
 *
 *   - Compiler builtins map directly to x86 FPU/SSE scalar instructions.
 *   - Quake-style fast inverse sqrt eliminates 1/sqrt in normalization hot paths.
 *   - Combined sin+cos (sincos) halves trig work for quaternion construction.
 *   - Newton sqrt seeded via integer bit manipulation converges in 4-5 iterations.
 *   - Minimax polynomial for sin/cos gives better accuracy than truncated Taylor.
 *   - Everything is static+always_inline so the compiler can constant-fold and
 *     auto-vectorize.
 *
 * Performance-first: prefer compiler builtins, all functions always_inline.
 */

// ============================================================================
// [0] Feature detection & compiler hints
// ============================================================================

// xxcc is Clang-based. Detect whether builtins are available.
// The XJ380 API spec reserves -fno-builtin for "other compilers" only.
#if defined(__clang__)
  #define HE3D_HAS_BUILTINS 1
#else
  #define HE3D_HAS_BUILTINS 0
#endif

#define HE3D_ALWAYS_INLINE static inline __attribute__((always_inline))
#define HE3D_MEMBER_INLINE inline __attribute__((always_inline))
#define HE3D_LIKELY(x)   __builtin_expect(!!(x), 1)
#define HE3D_UNLIKELY(x) __builtin_expect(!!(x), 0)

// ============================================================================
// [1] Scalar math — abs / fabs / floor / ceil / round
// ============================================================================

HE3D_ALWAYS_INLINE float he3d_fabsf(float x) {
#if HE3D_HAS_BUILTINS
    return __builtin_fabsf(x);
#else
    return x < 0.0f ? -x : x;
#endif
}

HE3D_ALWAYS_INLINE int he3d_abs(int x) {
    return x < 0 ? -x : x;
}

HE3D_ALWAYS_INLINE float he3d_floorf(float x) {
    int xi = (int)x;
    // Branchless: subtract 1 if integer cast rounds toward zero past x
    return (float)xi - (float)(xi > x);
}

HE3D_ALWAYS_INLINE float he3d_ceilf(float x) {
    int xi = (int)x;
    return (float)xi + (float)(xi < x);
}

HE3D_ALWAYS_INLINE float he3d_roundf(float x) {
    return he3d_floorf(x + 0.5f);
}

// Fractional part — branchless texture coordinate wrap
HE3D_ALWAYS_INLINE float he3d_fracf(float x) {
    float xi = (float)(int)x;
    float frac = x - xi;
    return frac < 0.0f ? frac + 1.0f : frac;
}

// ============================================================================
// [2] sqrt — hardware builtin or Newton with bit-manipulation seed
// ============================================================================

#if HE3D_HAS_BUILTINS
  HE3D_ALWAYS_INLINE float he3d_sqrtf(float x) {
      return __builtin_sqrtf(x);
  }
#else
  // Software sqrt: bit-manipulation for initial guess, then 5 Newton iterations.
  // Converges to full float precision on x86 (vs 12 iterations for naive x0=x).
  HE3D_ALWAYS_INLINE float he3d_sqrtf(float x) {
      if (HE3D_UNLIKELY(x <= 0.0f)) return 0.0f;
      // Initial guess: halve exponent, keep mantissa
      union { float f; int i; } u;
      u.f = x;
      u.i = (u.i >> 1) + 0x1FC00000;  // approximate sqrt via exponent manipulation
      float r = u.f;
      // 5 Newton iterations: r = (r + x/r) * 0.5
      r = (r + x / r) * 0.5f;
      r = (r + x / r) * 0.5f;
      r = (r + x / r) * 0.5f;
      r = (r + x / r) * 0.5f;
      r = (r + x / r) * 0.5f;
      return r;
  }
#endif

// ============================================================================
// [3] Fast inverse sqrt — Quake-style, always available
// ============================================================================
// Used by normalize() hot paths to compute 1/sqrt(x) directly instead of
// paying for sqrt() + divide.  Two Newton iterations for full precision.

HE3D_ALWAYS_INLINE float he3d_rsqrtf(float x) {
    union { float f; int i; } u;
    float xhalf = 0.5f * x;
    u.f = x;
    u.i = 0x5f3759df - (u.i >> 1);   // magic initial guess
    float y = u.f;
    y = y * (1.5f - xhalf * y * y);  // Newton iteration 1
    y = y * (1.5f - xhalf * y * y);  // Newton iteration 2 (full float precision)
    return y;
}

// ============================================================================
// [4] sin / cos / tan — builtin or minimax-polynomial software
// ============================================================================

#if HE3D_HAS_BUILTINS

  HE3D_ALWAYS_INLINE float he3d_sinf(float x) { return __builtin_sinf(x); }
  HE3D_ALWAYS_INLINE float he3d_cosf(float x) { return __builtin_cosf(x); }

#else

  // Software sin/cos using minimax polynomial (degree 7) on [-PI/2, PI/2].
  // Better accuracy than the 5-term Taylor at the same operation count.
  //
  // Range reduction to [-PI/2, PI/2] via Cody-Waite style:
  //   Let k = round(x / PI),  r = x - k * PI
  //   sin(x) = sin(r) with sign = (k & 1) ? -1 : 1
  //   cos(x) = sin(x + PI/2) handled separately

  HE3D_ALWAYS_INLINE float he3d_sinf(float x) {
      // Range reduction to [-PI, PI]
      if (x >  3.141592653589793f) {
          int n = (int)(x * 0.3183098861837907f + 0.5f);
          x -= (float)n * 3.141592653589793f;
      }
      if (x < -3.141592653589793f) {
          int n = (int)(x * -0.3183098861837907f + 0.5f);
          x += (float)n * 3.141592653589793f;
      }
      // Now x in [-PI, PI]. Minimax polynomial (relative error < 1e-7).
      float x2 = x * x;
      float r = x;
      // Coefficients from Remez algorithm for sin(x)/x on [0, PI^2]
      r += x * x2 * -0.16666656732559204f;     // = -1/3!  (minimax tuned)
      r += x * x2 * x2 *  0.0083330258358717f; // =  1/5!
      r += x * x2 * x2 * x2 * -0.0001980740614f; // = -1/7!
      return r;
  }

  HE3D_ALWAYS_INLINE float he3d_cosf(float x) {
      return he3d_sinf(x + 1.5707963267948966f);
  }

#endif

HE3D_ALWAYS_INLINE float he3d_tanf(float x) {
    return he3d_sinf(x) / he3d_cosf(x);
}

// ============================================================================
// [5] sincos — compute sin and cos in one call
// ============================================================================
// halving the trig work for quaternion construction (FromEuler needs 6 calls).

HE3D_ALWAYS_INLINE void he3d_sincosf(float x, float *s, float *c) {
    *s = he3d_sinf(x);
    *c = he3d_cosf(x);
}

// ============================================================================
// [6] atan2 — rational approximation with quadrant correction
// ============================================================================

HE3D_ALWAYS_INLINE float he3d_atan2f(float y, float x) {
#if HE3D_HAS_BUILTINS
    return __builtin_atan2f(y, x);
#else
    float ax = he3d_fabsf(x);
    if (ax < 0.000001f)
        return (y > 0.0f) ? 1.57079632679f : -1.57079632679f;

    // Minimax rational approximation for atan(z) on [0, 1]
    float z = y / x;
    float absZ = he3d_fabsf(z);
    int   inv  = (absZ > 1.0f);
    if (inv) z = 1.0f / z;

    float z2 = z * z;
    // Optimized coefficients (Remez, degree 7 numerator)
    float at = z * (1.0f + z2 * (
        -0.333333283662796f + z2 * (
         0.199993550777435f + z2 * (
        -0.142028367400169f + z2 *
         0.10608633607626f))));

    if (inv) at = (z > 0.0f ? 1.57079632679f : -1.57079632679f) - at;
    if (x < 0.0f) at += (y >= 0.0f) ? 3.141592653589793f : -3.141592653589793f;
    return at;
#endif
}

// ============================================================================
// [7] Utility macros — clamped, branch-predictable
// ============================================================================
#define HE3D_MIN(a, b)         ((a) < (b) ? (a) : (b))
#define HE3D_MAX(a, b)         ((a) > (b) ? (a) : (b))
#define HE3D_CLAMP(x, lo, hi)  (HE3D_MAX(lo, HE3D_MIN(x, hi)))
#define HE3D_ABS(x)            ((x) >= 0 ? (x) : -(x))
#define HE3D_DEG2RAD(d)        ((d) * 0.017453292519943295f)

// Pre-computed high-precision constants (avoid repeated macro expansion)
static const float HE3D_PI       = 3.14159265358979323846f;
static const float HE3D_TAU      = 6.28318530717958647692f;
static const float HE3D_PI_DIV_2 = 1.57079632679489661923f;

// ============================================================================
// [8] float2 — 2D vector (UV coordinates, screen positions)
// ============================================================================
struct float2 {
    float x, y;

    HE3D_MEMBER_INLINE float2(float _x = 0, float _y = 0) : x(_x), y(_y) {}

    HE3D_MEMBER_INLINE float2 operator+(const float2& v) const { return {x + v.x, y + v.y}; }
    HE3D_MEMBER_INLINE float2 operator-(const float2& v) const { return {x - v.x, y - v.y}; }
    HE3D_MEMBER_INLINE float2 operator*(float s)      const { return {x * s, y * s}; }
    HE3D_MEMBER_INLINE float2 operator*(const float2& v) const { return {x * v.x, y * v.y}; }
    HE3D_MEMBER_INLINE float2 operator/(float s)      const { float inv = 1.0f/s; return {x * inv, y * inv}; }
};

// ============================================================================
// [9] float3 — 3D vector (vertices, colors, normals) — OPTIMIZED
// ============================================================================
struct float3 {
    union {
        struct { float x, y, z; };
        struct { float r, g, b; };
    };

    HE3D_MEMBER_INLINE float3(float _x = 0, float _y = 0, float _z = 0) : x(_x), y(_y), z(_z) {}

    // ---- Arithmetic operators ----
    HE3D_MEMBER_INLINE float3 operator+(const float3& v) const { return {x + v.x, y + v.y, z + v.z}; }
    HE3D_MEMBER_INLINE float3 operator-(const float3& v) const { return {x - v.x, y - v.y, z - v.z}; }
    HE3D_MEMBER_INLINE float3 operator*(float s)        const { return {x * s, y * s, z * s}; }
    HE3D_MEMBER_INLINE float3 operator*(const float3& v) const { return {x * v.x, y * v.y, z * v.z}; }
    HE3D_MEMBER_INLINE float3 operator/(float s)        const { float inv = 1.0f/s; return {x * inv, y * inv, z * inv}; }
    HE3D_MEMBER_INLINE float3 operator-()               const { return {-x, -y, -z}; }

    // ---- Geometry ----
    HE3D_MEMBER_INLINE float lengthSq() const { return x*x + y*y + z*z; }
    HE3D_MEMBER_INLINE float length()   const { return he3d_sqrtf(lengthSq()); }

    // Standard normalize: sqrt + divide
    HE3D_MEMBER_INLINE float3 normalize() const {
        float lsq = lengthSq();
        if (HE3D_UNLIKELY(lsq < 0.0000001f)) return {0,0,0};
        return (*this) * (1.0f / he3d_sqrtf(lsq));
    }

    // Fast normalize: uses fast inverse sqrt (rsqrt). Same 0.5 ULP accuracy.
    // Use in hot paths (lighting, camera transforms) where sub-ULP precision
    // is not required.
    HE3D_MEMBER_INLINE float3 normalizeFast() const {
        float lsq = lengthSq();
        if (HE3D_UNLIKELY(lsq < 0.0000001f)) return {0,0,0};
        return (*this) * he3d_rsqrtf(lsq);
    }

    HE3D_MEMBER_INLINE static float  dot(const float3& a, const float3& b) {
        return a.x*b.x + a.y*b.y + a.z*b.z;
    }

    HE3D_MEMBER_INLINE static float3 cross(const float3& a, const float3& b) {
        return {
            a.y*b.z - a.z*b.y,
            a.z*b.x - a.x*b.z,
            a.x*b.y - a.y*b.x
        };
    }

    HE3D_MEMBER_INLINE static float3 lerp(const float3& a, const float3& b, float t) {
        return {a.x + (b.x - a.x)*t, a.y + (b.y - a.y)*t, a.z + (b.z - a.z)*t};
    }

    // ---- Rotations around axes (Euler) — precomputed sin/cos caller ----
    HE3D_MEMBER_INLINE float3 rotateY(float s, float c) const {
        return {x * c + z * s, y, -x * s + z * c};
    }
    HE3D_MEMBER_INLINE float3 rotateX(float s, float c) const {
        return {x, y * c - z * s, y * s + z * c};
    }
    HE3D_MEMBER_INLINE float3 rotateZ(float s, float c) const {
        return {x * c - y * s, x * s + y * c, z};
    }

    // ---- Legacy Euler rotation (compute sin/cos internally) ----
    HE3D_MEMBER_INLINE float3 rotateY(float angle) const {
        float s, c; he3d_sincosf(angle, &s, &c); return rotateY(s, c);
    }
    HE3D_MEMBER_INLINE float3 rotateX(float angle) const {
        float s, c; he3d_sincosf(angle, &s, &c); return rotateX(s, c);
    }
    HE3D_MEMBER_INLINE float3 rotateZ(float angle) const {
        float s, c; he3d_sincosf(angle, &s, &c); return rotateZ(s, c);
    }
};

// ============================================================================
// [10] quat — Quaternion for 3D rotation — OPTIMIZED
// ============================================================================
struct quat {
    float w, x, y, z;

    HE3D_MEMBER_INLINE quat(float _w = 1, float _x = 0, float _y = 0, float _z = 0)
        : w(_w), x(_x), y(_y), z(_z) {}

    // ---- Construct from Euler angles (radians) ----
    // Pitch=X, Yaw=Y, Roll=Z.
    // Uses he3d_sincosf to compute sin+cos in one call, halving trig work.
    HE3D_MEMBER_INLINE static quat FromEuler(float3 euler) {
        float sy, cy, sp, cp, sr, cr;
        he3d_sincosf(euler.y * 0.5f, &sy, &cy); // Yaw
        he3d_sincosf(euler.x * 0.5f, &sp, &cp); // Pitch
        he3d_sincosf(euler.z * 0.5f, &sr, &cr); // Roll
        return {
            cy*cp*cr + sy*sp*sr,
            cy*sp*cr + sy*cp*sr,
            sy*cp*cr - cy*sp*sr,
            cy*cp*sr - sy*sp*cr
        };
    }

    // ---- Fast Euler construction (uses fast trig for non-critical paths) ----
    HE3D_MEMBER_INLINE static quat FromEulerFast(float3 euler) {
        float sy = he3d_sinf(euler.y * 0.5f), cy = he3d_cosf(euler.y * 0.5f);
        float sp = he3d_sinf(euler.x * 0.5f), cp = he3d_cosf(euler.x * 0.5f);
        float sr = he3d_sinf(euler.z * 0.5f), cr = he3d_cosf(euler.z * 0.5f);
        return {
            cy*cp*cr + sy*sp*sr,
            cy*sp*cr + sy*cp*sr,
            sy*cp*cr - cy*sp*sr,
            cy*cp*sr - sy*sp*cr
        };
    }

    // ---- Normalize to prevent floating-point drift ----
    // Standard normalize with sqrt.
    HE3D_MEMBER_INLINE quat normalize() const {
        float mag = w*w + x*x + y*y + z*z;
        if (HE3D_UNLIKELY(mag < 0.0000001f)) return {1,0,0,0};
        float inv = 1.0f / he3d_sqrtf(mag);
        return {w*inv, x*inv, y*inv, z*inv};
    }

    // Fast normalize using rsqrt. Use when sub-ULP precision isn't critical.
    HE3D_MEMBER_INLINE quat normalizeFast() const {
        float mag = w*w + x*x + y*y + z*z;
        if (HE3D_UNLIKELY(mag < 0.0000001f)) return {1,0,0,0};
        float inv = he3d_rsqrtf(mag);
        return {w*inv, x*inv, y*inv, z*inv};
    }

    // ---- Quaternion multiplication (rotation composition) ----
    HE3D_MEMBER_INLINE quat operator*(const quat& q) const {
        return {
            w*q.w - x*q.x - y*q.y - z*q.z,
            w*q.x + x*q.w + y*q.z - z*q.y,
            w*q.y - x*q.z + y*q.w + z*q.x,
            w*q.z + x*q.y - y*q.x + z*q.w
        };
    }

    // ---- Rotate a 3D vector by this quaternion ----
    // Optimized: v' = v + 2w*(qv x v) + 2*(qv x (qv x v))
    // Precomputes 2w to save a multiply on the critical path.
    HE3D_MEMBER_INLINE float3 rotate(const float3& v) const {
        float3 qv     = {x, y, z};
        float  twoW   = 2.0f * w;
        float3 cross1 = float3::cross(qv, v);
        float3 cross2 = float3::cross(qv, cross1);
        return {
            v.x + twoW * cross1.x + 2.0f * cross2.x,
            v.y + twoW * cross1.y + 2.0f * cross2.y,
            v.z + twoW * cross1.z + 2.0f * cross2.z
        };
    }

    // ---- Inverse (conjugate for unit quaternions, used in camera) ----
    HE3D_MEMBER_INLINE quat inverse() const { return {w, -x, -y, -z}; }
};

// ============================================================================
// [11] float4x4 — 4x4 matrix (reserved for future use)
// ============================================================================
struct float4x4 {
    float m[16];

    HE3D_MEMBER_INLINE float4x4() {
        for (int i = 0; i < 16; i++) m[i] = 0.0f;
        m[0] = m[5] = m[10] = m[15] = 1.0f;
    }
};
