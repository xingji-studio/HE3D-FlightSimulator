/*
 * HE3D Engine for 3D - C++ implementation.
 * HE3D 三维引擎 C++ 实现。
 *
 * Contains the OBJ loader, image texture loaders, physics helpers, and software rasterizer.
 * 包含 OBJ 加载器、图片纹理加载器、物理辅助功能和软件光栅化器。
 *
 * Platform services are supplied through he3d_platform.hpp.
 * 平台服务由 he3d_platform.hpp 提供。
 */
#include "he3d.hpp"

typedef __SIZE_TYPE__ size_t;

static void *he3d_alloc_or_trap(unsigned long size)
{
    void *ptr = HE3D::Alloc((HE3D::uint64_t)size);
    if (!ptr)
    {
        __builtin_trap();
    }
    return ptr;
}

static void *he3d_stbi_memcpy(void *dst, const void *src, size_t count)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    for (size_t i = 0; i < count; i++)
    {
        d[i] = s[i];
    }
    return dst;
}

static void *he3d_stbi_memset(void *dst, int value, size_t count)
{
    unsigned char *d = (unsigned char *)dst;
    for (size_t i = 0; i < count; i++)
    {
        d[i] = (unsigned char)value;
    }
    return dst;
}

static void *he3d_stbi_malloc(size_t size)
{
    return HE3D::Alloc((HE3D::uint64_t)size);
}

static void he3d_stbi_free(void *ptr)
{
    HE3D::Free(ptr);
}

static void *he3d_stbi_realloc_sized(void *ptr, size_t oldSize, size_t newSize)
{
    void *next = HE3D::Alloc((HE3D::uint64_t)newSize);
    if (!next)
    {
        return nullptr;
    }
    if (ptr)
    {
        size_t copySize = oldSize < newSize ? oldSize : newSize;
        he3d_stbi_memcpy(next, ptr, copySize);
        HE3D::Free(ptr);
    }
    return next;
}

static double he3d_stbi_fabs(double value)
{
    return value < 0.0 ? -value : value;
}

#ifndef NULL
#define NULL 0
#endif
#ifndef INT_MAX
#define INT_MAX 2147483647
#endif
#ifndef INT_MIN
#define INT_MIN (-2147483647 - 1)
#endif
#ifndef UINT_MAX
#define UINT_MAX 4294967295U
#endif
#ifndef SHRT_MAX
#define SHRT_MAX 32767
#endif
#ifndef SHRT_MIN
#define SHRT_MIN (-32767 - 1)
#endif

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STBI_NO_SIMD
#define STBI_NO_THREAD_LOCALS
#define STBI_NO_FAILURE_STRINGS
#define STBI_ASSERT(x) ((void)0)
#define STBI_MALLOC(sz) he3d_stbi_malloc(sz)
#define STBI_FREE(p) he3d_stbi_free(p)
#define STBI_REALLOC_SIZED(p,oldsz,newsz) he3d_stbi_realloc_sized(p,oldsz,newsz)
#define fabs he3d_stbi_fabs
#define memcpy he3d_stbi_memcpy
#define memset he3d_stbi_memset
#include "../third_party/stbi.h"
#undef memcpy
#undef memset
#undef fabs

namespace HE3D {

static const Platform *g_platform = nullptr;
static uint32_t g_frameRateLimit = 0;
static bool g_fxaaEnabled = false;
static bool g_taaEnabled = false;
static bool g_msaaEnabled = false;
static uint32_t g_ssaaScale = 1;

const Platform *GetPlatform()
{
    if (!g_platform)
    {
        g_platform = GetBuiltinPlatform();
    }
    return g_platform;
}

void SetPlatform(const Platform *platform)
{
    g_platform = platform;
}

void *Alloc(uint64_t size)
{
    return GetPlatform()->alloc(size);
}

void Free(void *ptr)
{
    GetPlatform()->free(ptr);
}

void SetFrameRateLimit(uint32_t fps)
{
    g_frameRateLimit = fps;
}

uint32_t GetFrameRateLimit()
{
    return g_frameRateLimit;
}

void SetFxaaEnabled(bool enabled)
{
    g_fxaaEnabled = enabled;
}

bool IsFxaaEnabled()
{
    return g_fxaaEnabled;
}

void SetTaaEnabled(bool enabled)
{
    g_taaEnabled = enabled;
}

bool IsTaaEnabled()
{
    return g_taaEnabled;
}

void SetMsaaEnabled(bool enabled)
{
    g_msaaEnabled = enabled;
}

bool IsMsaaEnabled()
{
    return g_msaaEnabled;
}

void SetSsaaScale(uint32_t scale)
{
    if (scale < 1)
    {
        scale = 1;
    }
    if (scale > 4)
    {
        scale = 4;
    }
    g_ssaaScale = scale;
}

uint32_t GetSsaaScale()
{
    return g_ssaaScale;
}

void PaceFrame(double frameStart)
{
    if (g_frameRateLimit == 0)
    {
        return;
    }

    double targetFrameSeconds = 1.0 / (double)g_frameRateLimit;
    double elapsed = TimeSeconds() - frameStart;
    if (elapsed < targetFrameSeconds)
    {
        uint64_t sleepMs = (uint64_t)((targetFrameSeconds - elapsed) * 1000.0);
        if (sleepMs > 0)
        {
            SleepMilliseconds(sleepMs);
        }
    }
}

}

// Route C++ allocation through HE3D's platform allocator.
// 将 C++ 分配转发到 HE3D 当前平台分配器。
void *operator new(unsigned long size)
{
    return he3d_alloc_or_trap(size);
}

void *operator new[](unsigned long size)
{
    return he3d_alloc_or_trap(size);
}

void operator delete(void *ptr) noexcept
{
    HE3D::Free(ptr);
}

void operator delete[](void *ptr) noexcept
{
    HE3D::Free(ptr);
}

namespace HE3D {

// ============================================================================
// [0] Float parser for builds without strtod/strtof.
// [0] 用于没有 strtod/strtof 环境的浮点解析器。
// ============================================================================
static const char *ParseFloat(const char *s, float *out)
{
    while (*s == ' ' || *s == '\t') s++;
    float sign = 1.0f;
    if (*s == '-') { sign = -1.0f; s++; }
    else if (*s == '+') { s++; }
    float ip = 0.0f;
    while (*s >= '0' && *s <= '9') { ip = ip * 10.0f + (float)(*s - '0'); s++; }
    float fp = 0.0f;
    if (*s == '.') {
        s++;
        float mul = 0.1f;
        while (*s >= '0' && *s <= '9') { fp += (float)(*s - '0') * mul; mul *= 0.1f; s++; }
    }
    float v = sign * (ip + fp);
    if (*s == 'e' || *s == 'E') {
        s++;
        float es = 1.0f;
        if (*s == '-') { es = -1.0f; s++; }
        else if (*s == '+') { s++; }
        int ev = 0;
        while (*s >= '0' && *s <= '9') { ev = ev * 10 + (*s - '0'); s++; }
        float em = 1.0f, eb = (es > 0) ? 10.0f : 0.1f;
        for (int i = 0; i < ev; i++) em *= eb;
        v *= em;
    }
    *out = v;
    return s;
}

// ============================================================================
// [1] In-memory line reader for OBJ parsing.
// [1] OBJ 解析使用的内存行读取器。
// ============================================================================
static const char *MemGetLine(const char *buf, const char *end, char *out, int maxLen)
{
    if (buf >= end) return nullptr;
    const char* ln = buf;
    while (ln < end && *ln != '\n' && *ln != '\r' && (ln - buf) < maxLen - 1) ln++;
    int len = (int)(ln - buf);
    if (len > maxLen - 1) len = maxLen - 1;
    for (int i = 0; i < len; i++) out[i] = buf[i];
    out[len] = 0;
    const char* nx = ln;
    while (nx < end && *nx != '\n' && *nx != '\r') nx++;
    if (nx < end && *nx == '\r') nx++;
    if (nx < end && *nx == '\n') nx++;
    return nx;
}

// Count the vertices in one OBJ face line.
// 统计一行 OBJ face 中的顶点数量。
static int CountFaceVerts(const char *p)
{
    int cnt = 0;
    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == 0 || *p == '\n' || *p == '\r') break;
        cnt++;
        while (*p && *p != ' ' && *p != '\t') p++;
    }
    return cnt;
}

struct Mat3
{
    float m00, m01, m02;
    float m10, m11, m12;
    float m20, m21, m22;

    static Mat3 FromQuat(const quat& q)
    {
        float xx = q.x * q.x;
        float yy = q.y * q.y;
        float zz = q.z * q.z;
        float xy = q.x * q.y;
        float xz = q.x * q.z;
        float yz = q.y * q.z;
        float wx = q.w * q.x;
        float wy = q.w * q.y;
        float wz = q.w * q.z;

        Mat3 r;
        r.m00 = 1.0f - 2.0f * (yy + zz);
        r.m01 = 2.0f * (xy - wz);
        r.m02 = 2.0f * (xz + wy);
        r.m10 = 2.0f * (xy + wz);
        r.m11 = 1.0f - 2.0f * (xx + zz);
        r.m12 = 2.0f * (yz - wx);
        r.m20 = 2.0f * (xz - wy);
        r.m21 = 2.0f * (yz + wx);
        r.m22 = 1.0f - 2.0f * (xx + yy);
        return r;
    }

    Mat3 Transpose() const
    {
        Mat3 r;
        r.m00 = m00; r.m01 = m10; r.m02 = m20;
        r.m10 = m01; r.m11 = m11; r.m12 = m21;
        r.m20 = m02; r.m21 = m12; r.m22 = m22;
        return r;
    }

    float3 Mul(const float3& v) const
    {
        return {
            m00 * v.x + m01 * v.y + m02 * v.z,
            m10 * v.x + m11 * v.y + m12 * v.z,
            m20 * v.x + m21 * v.y + m22 * v.z
        };
    }
};

static bool QuatIsIdentity(const quat& q)
{
    return HE3D_ABS(q.x) < 0.000001f &&
           HE3D_ABS(q.y) < 0.000001f &&
           HE3D_ABS(q.z) < 0.000001f &&
           HE3D_ABS(q.w - 1.0f) < 0.000001f;
}

static const float HE3D_NEAR_Z = 0.1f;

struct ClipVertex
{
    float3 view;
    float2 uv;
};

static ClipVertex LerpClipVertex(const ClipVertex& a, const ClipVertex& b, float t)
{
    ClipVertex out;
    out.view = a.view + (b.view - a.view) * t;
    out.uv = a.uv + (b.uv - a.uv) * t;
    return out;
}

static int ClipTriangleToNearPlane(const ClipVertex *input, ClipVertex *output)
{
    ClipVertex temp[4];
    int count = 0;

    for (int i = 0; i < 3; i++)
    {
        const ClipVertex& a = input[i];
        const ClipVertex& b = input[(i + 1) % 3];
        bool aInside = a.view.z > HE3D_NEAR_Z;
        bool bInside = b.view.z > HE3D_NEAR_Z;

        if (aInside && bInside)
        {
            temp[count++] = b;
        }
        else if (aInside && !bInside)
        {
            float t = (HE3D_NEAR_Z - a.view.z) / (b.view.z - a.view.z);
            temp[count++] = LerpClipVertex(a, b, t);
        }
        else if (!aInside && bInside)
        {
            float t = (HE3D_NEAR_Z - a.view.z) / (b.view.z - a.view.z);
            temp[count++] = LerpClipVertex(a, b, t);
            temp[count++] = b;
        }
    }

    for (int i = 0; i < count; i++)
    {
        output[i] = temp[i];
    }
    return count;
}

static void ProjectViewTriangle(const float3 *vv, float2 *ps,
                                float halfW, float halfH,
                                float scaleX, float scaleY,
                                float jitterX, float jitterY)
{
    for (int i = 0; i < 3; i++)
    {
        float invZ = 1.0f / vv[i].z;
        ps[i] = {halfW + vv[i].x * invZ * scaleX + jitterX,
                 halfH - vv[i].y * invZ * scaleY + jitterY};
    }
}

static float ScreenTriangleArea(const float2 *ps)
{
    return (ps[1].x - ps[0].x) * (ps[2].y - ps[0].y)
         - (ps[1].y - ps[0].y) * (ps[2].x - ps[0].x);
}

static bool TriangleOutsideViewport(const float2 *ps, int width, int height)
{
    if (ps[0].x < 0.0f && ps[1].x < 0.0f && ps[2].x < 0.0f) return true;
    if (ps[0].x >= (float)width && ps[1].x >= (float)width && ps[2].x >= (float)width) return true;
    if (ps[0].y < 0.0f && ps[1].y < 0.0f && ps[2].y < 0.0f) return true;
    if (ps[0].y >= (float)height && ps[1].y >= (float)height && ps[2].y >= (float)height) return true;
    return false;
}

// ============================================================================
// [3] CollisionBox / 碰撞箱
// ============================================================================
CollisionBox::CollisionBox()
    : object(nullptr), centerOffset{0,0,0}, halfExtents{0,0,0}
{
}

CollisionBox::CollisionBox(GameObject *gameObject)
    : object(gameObject), centerOffset{0,0,0}, halfExtents{0,0,0}
{
}

void CollisionBox::BindGameObject(GameObject *gameObject)
{
    object = gameObject;
}

bool CollisionBox::FitMesh()
{
    if (!object || !object->mesh)
    {
        return false;
    }
    return FitMesh(*object->mesh);
}

bool CollisionBox::FitMesh(const Mesh& mesh)
{
    if (!mesh.vertices || mesh.vertCount <= 0)
    {
        centerOffset = {0,0,0};
        halfExtents = {0,0,0};
        return false;
    }

    float3 mn = mesh.vertices[0];
    float3 mx = mesh.vertices[0];
    for (int i = 1; i < mesh.vertCount; i++)
    {
        const float3& v = mesh.vertices[i];
        if (v.x < mn.x) mn.x = v.x;
        if (v.y < mn.y) mn.y = v.y;
        if (v.z < mn.z) mn.z = v.z;
        if (v.x > mx.x) mx.x = v.x;
        if (v.y > mx.y) mx.y = v.y;
        if (v.z > mx.z) mx.z = v.z;
    }

    centerOffset = (mn + mx) * 0.5f;
    halfExtents = (mx - mn) * 0.5f;
    return IsValid();
}

float3 CollisionBox::Center() const
{
    if (!object)
    {
        return centerOffset;
    }
    return object->position + object->orientation.rotate(centerOffset);
}

quat CollisionBox::Orientation() const
{
    return object ? object->orientation : quat();
}

bool CollisionBox::IsValid() const
{
    return halfExtents.x > 0.0f && halfExtents.y > 0.0f && halfExtents.z > 0.0f;
}

float3 CollisionBox::AxisX() const
{
    return Orientation().rotate({1,0,0}).normalizeFast();
}

float3 CollisionBox::AxisY() const
{
    return Orientation().rotate({0,1,0}).normalizeFast();
}

float3 CollisionBox::AxisZ() const
{
    return Orientation().rotate({0,0,1}).normalizeFast();
}

bool CollisionBox::Contains(float3 point) const
{
    if (!IsValid())
    {
        return false;
    }

    float3 d = point - Center();
    float x = float3::dot(d, AxisX());
    float y = float3::dot(d, AxisY());
    float z = float3::dot(d, AxisZ());
    return HE3D_ABS(x) <= halfExtents.x &&
           HE3D_ABS(y) <= halfExtents.y &&
           HE3D_ABS(z) <= halfExtents.z;
}

bool CollisionBox::Contact(const CollisionBox& other, PhysicsContact *outContact) const
{
    PhysicsContact contact;
    if (!IsValid() || !other.IsValid())
    {
        return false;
    }

    float3 a[3] = {AxisX(), AxisY(), AxisZ()};
    float3 b[3] = {other.AxisX(), other.AxisY(), other.AxisZ()};
    float  ea[3] = {halfExtents.x, halfExtents.y, halfExtents.z};
    float  eb[3] = {other.halfExtents.x, other.halfExtents.y, other.halfExtents.z};
    float  r[3][3];
    float  ar[3][3];
    float  minOverlap = 340282346638528859811704183484516925440.0f;
    float3 bestAxis = {0, 1, 0};

    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            r[i][j] = float3::dot(a[i], b[j]);
            ar[i][j] = fabsf(r[i][j]) + 0.000001f;
        }
    }

    float3 delta = other.Center() - Center();
    float t[3] = {
        float3::dot(delta, a[0]),
        float3::dot(delta, a[1]),
        float3::dot(delta, a[2])
    };

    for (int i = 0; i < 3; i++)
    {
        float ra = ea[i];
        float rb = eb[0] * ar[i][0] + eb[1] * ar[i][1] + eb[2] * ar[i][2];
        float overlap = ra + rb - fabsf(t[i]);
        if (overlap < 0.0f) return false;
        if (overlap < minOverlap)
        {
            minOverlap = overlap;
            bestAxis = t[i] < 0.0f ? a[i] : -a[i];
        }
    }

    for (int j = 0; j < 3; j++)
    {
        float ra = ea[0] * ar[0][j] + ea[1] * ar[1][j] + ea[2] * ar[2][j];
        float rb = eb[j];
        float tj = fabsf(t[0] * r[0][j] + t[1] * r[1][j] + t[2] * r[2][j]);
        float overlap = ra + rb - tj;
        if (overlap < 0.0f) return false;
        if (overlap < minOverlap)
        {
            minOverlap = overlap;
            float sign = (t[0] * r[0][j] + t[1] * r[1][j] + t[2] * r[2][j]) < 0.0f ? 1.0f : -1.0f;
            bestAxis = b[j] * sign;
        }
    }

    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            float3 axis = float3::cross(a[i], b[j]);
            if (axis.lengthSq() <= 0.0000001f)
            {
                continue;
            }

            float ra = ea[(i + 1) % 3] * ar[(i + 2) % 3][j] +
                       ea[(i + 2) % 3] * ar[(i + 1) % 3][j];
            float rb = eb[(j + 1) % 3] * ar[i][(j + 2) % 3] +
                       eb[(j + 2) % 3] * ar[i][(j + 1) % 3];
            float tv = t[(i + 2) % 3] * r[(i + 1) % 3][j] -
                       t[(i + 1) % 3] * r[(i + 2) % 3][j];
            float overlap = ra + rb - fabsf(tv);
            if (overlap < 0.0f) return false;
            if (overlap < minOverlap)
            {
                minOverlap = overlap;
                bestAxis = (tv < 0.0f ? axis : -axis).normalizeFast();
            }
        }
    }

    contact.hit = true;
    contact.penetration = minOverlap;
    contact.normal = bestAxis.normalizeFast();
    contact.point = (Center() + other.Center()) * 0.5f;
    if (outContact)
    {
        *outContact = contact;
    }
    return true;
}

AABB CollisionBox::WorldAABB() const
{
    float3 c = Center();
    float3 ax = AxisX();
    float3 ay = AxisY();
    float3 az = AxisZ();
    float3 e = {
        HE3D_ABS(ax.x) * halfExtents.x + HE3D_ABS(ay.x) * halfExtents.y + HE3D_ABS(az.x) * halfExtents.z,
        HE3D_ABS(ax.y) * halfExtents.x + HE3D_ABS(ay.y) * halfExtents.y + HE3D_ABS(az.y) * halfExtents.z,
        HE3D_ABS(ax.z) * halfExtents.x + HE3D_ABS(ay.z) * halfExtents.y + HE3D_ABS(az.z) * halfExtents.z
    };
    return AABB(c - e, c + e);
}

bool RaycastCollisionBox(const Ray& ray, const CollisionBox& box, RayHit *outHit)
{
    if (outHit) *outHit = RayHit();

    RayHit hit = box.Raycast(ray);
    if (!hit.hit)
    {
        return false;
    }
    if (outHit)
    {
        *outHit = hit;
    }
    return true;
}

bool RaycastCollisionBoxes(const Ray& ray, const CollisionBox *boxes, int32_t boxCount,
                           RayHit *outHit, int32_t *outIndex)
{
    if (outHit) *outHit = RayHit();
    if (outIndex) *outIndex = -1;

    if (!boxes || boxCount <= 0)
    {
        return false;
    }

    bool found = false;
    RayHit bestHit;
    int32_t bestIndex = -1;
    for (int i = 0; i < boxCount; i++)
    {
        RayHit hit = boxes[i].Raycast(ray);
        if (!hit.hit)
        {
            continue;
        }
        if (!found || hit.distance < bestHit.distance)
        {
            bestHit = hit;
            bestIndex = i;
            found = true;
        }
    }

    if (found)
    {
        if (outHit) *outHit = bestHit;
        if (outIndex) *outIndex = bestIndex;
    }
    return found;
}

bool RaycastMeshTriangles(const Ray& ray, const GameObject& object,
                          RayHit *outHit, int32_t *outTriangleIndex)
{
    if (outHit) *outHit = RayHit();
    if (outTriangleIndex) *outTriangleIndex = -1;

    if (!object.mesh || !object.mesh->vertices || object.mesh->vertCount < 3)
    {
        return false;
    }

    bool found = false;
    RayHit bestHit;
    int32_t bestTri = -1;
    quat objRot = object.orientation;
    bool identityObjRot = QuatIsIdentity(objRot);
    Mat3 rot = Mat3::FromQuat(objRot);

    for (int i = 0; i < object.mesh->vertCount; i += 3)
    {
        float3 v0 = object.mesh->vertices[i];
        float3 v1 = object.mesh->vertices[i + 1];
        float3 v2 = object.mesh->vertices[i + 2];
        if (!identityObjRot)
        {
            v0 = rot.Mul(v0) + object.position;
            v1 = rot.Mul(v1) + object.position;
            v2 = rot.Mul(v2) + object.position;
        }
        else
        {
            v0 = v0 + object.position;
            v1 = v1 + object.position;
            v2 = v2 + object.position;
        }

        RayHit hit = ray.CastTriangle(v0, v1, v2);
        if (!hit.hit)
        {
            continue;
        }
        if (!found || hit.distance < bestHit.distance)
        {
            bestHit = hit;
            bestTri = i / 3;
            found = true;
        }
    }

    if (found)
    {
        if (outHit) *outHit = bestHit;
        if (outTriangleIndex) *outTriangleIndex = bestTri;
    }
    return found;
}

static void IncludePoint(AABB& bounds, float3 point)
{
    if (point.x < bounds.min.x) bounds.min.x = point.x;
    if (point.y < bounds.min.y) bounds.min.y = point.y;
    if (point.z < bounds.min.z) bounds.min.z = point.z;
    if (point.x > bounds.max.x) bounds.max.x = point.x;
    if (point.y > bounds.max.y) bounds.max.y = point.y;
    if (point.z > bounds.max.z) bounds.max.z = point.z;
}

static bool TriangleHeightAtXZ(float3 p0, float3 p1, float3 p2,
                               float x, float z, float *outY, float3 *outNormal)
{
    float v0x = p1.x - p0.x;
    float v0z = p1.z - p0.z;
    float v1x = p2.x - p0.x;
    float v1z = p2.z - p0.z;
    float v2x = x - p0.x;
    float v2z = z - p0.z;
    float den = v0x * v1z - v1x * v0z;
    if (HE3D_ABS(den) < 0.000001f)
    {
        return false;
    }

    float invDen = 1.0f / den;
    float a = (v2x * v1z - v1x * v2z) * invDen;
    float b = (v0x * v2z - v2x * v0z) * invDen;
    if (a < -0.0001f || b < -0.0001f || a + b > 1.0001f)
    {
        return false;
    }

    if (outY)
    {
        *outY = p0.y + (p1.y - p0.y) * a + (p2.y - p0.y) * b;
    }
    if (outNormal)
    {
        float3 n = float3::cross(p1 - p0, p2 - p0).normalizeFast();
        if (n.y < 0.0f) n = -n;
        *outNormal = n;
    }
    return true;
}

HeightFieldCollider::Tile::Tile()
    : active(false), originX(0.0f), originZ(0.0f), bounds(), cellBounds(nullptr),
      cellCount(0), cellCapacity(0)
{
}

HeightFieldCollider::Tile::~Tile()
{
    delete[] cellBounds;
}

bool HeightFieldCollider::Tile::EnsureCapacity(int32_t count)
{
    if (count <= cellCapacity)
    {
        return true;
    }

    AABB *next = new AABB[count];
    if (!next)
    {
        return false;
    }
    for (int i = 0; i < cellCount; i++)
    {
        next[i] = cellBounds[i];
    }
    delete[] cellBounds;
    cellBounds = next;
    cellCapacity = count;
    return true;
}

HeightFieldCollider::HeightFieldCollider()
    : sampleHeight(nullptr), sampleUser(nullptr), gridCount(0), cellSize(0.0f)
{
}

HeightFieldCollider::HeightFieldCollider(HeightSampleCallback callback, void *user,
                                         int32_t gridCountValue, float cellSizeValue)
    : sampleHeight(nullptr), sampleUser(nullptr), gridCount(0), cellSize(0.0f)
{
    Configure(callback, user, gridCountValue, cellSizeValue);
}

void HeightFieldCollider::Configure(HeightSampleCallback callback, void *user,
                                    int32_t gridCountValue, float cellSizeValue)
{
    sampleHeight = callback;
    sampleUser = user;
    gridCount = gridCountValue;
    cellSize = cellSizeValue;
}

bool HeightFieldCollider::IsValid() const
{
    return sampleHeight != nullptr && gridCount >= 2 && cellSize > 0.0f;
}

float HeightFieldCollider::Sample(float x, float z) const
{
    return sampleHeight ? sampleHeight(x, z, sampleUser) : 0.0f;
}

bool HeightFieldCollider::BuildTile(Tile& tile, float originX, float originZ) const
{
    tile.active = false;
    tile.originX = originX;
    tile.originZ = originZ;
    tile.cellCount = 0;
    tile.bounds = AABB({0,0,0}, {0,0,0});

    if (!IsValid())
    {
        return false;
    }

    int cellsPerSide = gridCount - 1;
    int cellTotal = cellsPerSide * cellsPerSide;
    if (cellTotal <= 0 || !tile.EnsureCapacity(cellTotal))
    {
        return false;
    }

    float half = (float)cellsPerSide * cellSize * 0.5f;
    bool first = true;
    for (int z = 0; z < cellsPerSide; z++)
    {
        for (int x = 0; x < cellsPerSide; x++)
        {
            float x0 = originX + (float)x * cellSize - half;
            float z0 = originZ + (float)z * cellSize - half;
            float x1 = x0 + cellSize;
            float z1 = z0 + cellSize;
            float3 p00 = {x0, Sample(x0, z0), z0};
            float3 p01 = {x0, Sample(x0, z1), z1};
            float3 p10 = {x1, Sample(x1, z0), z0};
            float3 p11 = {x1, Sample(x1, z1), z1};
            AABB cell(p00, p00);
            IncludePoint(cell, p01);
            IncludePoint(cell, p10);
            IncludePoint(cell, p11);
            cell.min.y -= 0.05f;
            cell.max.y += 0.05f;
            tile.cellBounds[tile.cellCount++] = cell;

            if (first)
            {
                tile.bounds = cell;
                first = false;
            }
            else
            {
                IncludePoint(tile.bounds, cell.min);
                IncludePoint(tile.bounds, cell.max);
            }
        }
    }

    tile.active = true;
    return true;
}

float3 HeightFieldCollider::NormalAt(float x, float z) const
{
    float e = cellSize > 0.0f ? cellSize * 0.0625f : 0.75f;
    if (e < 0.1f) e = 0.1f;
    float hx0 = Sample(x - e, z);
    float hx1 = Sample(x + e, z);
    float hz0 = Sample(x, z - e);
    float hz1 = Sample(x, z + e);
    return float3(hx0 - hx1, 2.0f * e, hz0 - hz1).normalizeFast();
}

bool HeightFieldCollider::TriangleContact(const Tile& tile, int32_t cellIndex,
                                          float x, float z,
                                          float *outY, float3 *outNormal) const
{
    int cellsPerSide = gridCount - 1;
    if (cellsPerSide <= 0)
    {
        return false;
    }

    int cellX = cellIndex % cellsPerSide;
    int cellZ = cellIndex / cellsPerSide;
    float half = (float)cellsPerSide * cellSize * 0.5f;
    float x0 = tile.originX + (float)cellX * cellSize - half;
    float z0 = tile.originZ + (float)cellZ * cellSize - half;
    float x1 = x0 + cellSize;
    float z1 = z0 + cellSize;

    float3 p00 = {x0, Sample(x0, z0), z0};
    float3 p01 = {x0, Sample(x0, z1), z1};
    float3 p10 = {x1, Sample(x1, z0), z0};
    float3 p11 = {x1, Sample(x1, z1), z1};

    if (TriangleHeightAtXZ(p00, p01, p10, x, z, outY, outNormal))
    {
        return true;
    }
    return TriangleHeightAtXZ(p10, p01, p11, x, z, outY, outNormal);
}

bool HeightFieldCollider::ContactBox(const CollisionBox& box, float sweep,
                                     PhysicsContact *outContact) const
{
    return ContactBox(box, nullptr, 0, sweep, outContact);
}

bool HeightFieldCollider::ContactBox(const CollisionBox& box, const Tile *tiles,
                                     int32_t tileCount, float sweep,
                                     PhysicsContact *outContact) const
{
    if (outContact) *outContact = PhysicsContact();
    if (!IsValid() || !box.IsValid())
    {
        return false;
    }

    AABB bounds = box.WorldAABB();
    if (sweep > 0.0f)
    {
        bounds.min.x -= sweep;
        bounds.min.y -= sweep;
        bounds.min.z -= sweep;
        bounds.max.x += sweep;
        bounds.max.y += sweep;
        bounds.max.z += sweep;
    }

    quat orientation = box.Orientation();
    float3 axisX = orientation.rotate({1, 0, 0});
    float3 axisY = orientation.rotate({0, 1, 0});
    float3 axisZ = orientation.rotate({0, 0, 1});
    float3 center = box.Center();
    float3 samples[27];
    int sampleCount = 0;
    for (int sx = -1; sx <= 1; sx++)
    {
        for (int sy = -1; sy <= 1; sy++)
        {
            for (int sz = -1; sz <= 1; sz++)
            {
                samples[sampleCount++] =
                    center +
                    axisX * (box.halfExtents.x * (float)sx) +
                    axisY * (box.halfExtents.y * (float)sy) +
                    axisZ * (box.halfExtents.z * (float)sz);
            }
        }
    }

    bool hit = false;
    bool checkedAnyCell = false;
    PhysicsContact best;

    if (tiles && tileCount > 0)
    {
        for (int t = 0; t < tileCount; t++)
        {
            if (!tiles[t].active || tiles[t].cellCount <= 0 || !bounds.Intersects(tiles[t].bounds))
            {
                continue;
            }

            for (int c = 0; c < tiles[t].cellCount; c++)
            {
                if (!bounds.Intersects(tiles[t].cellBounds[c]))
                {
                    continue;
                }
                checkedAnyCell = true;

                for (int s = 0; s < sampleCount; s++)
                {
                    float y = 0.0f;
                    float3 normal = {0, 1, 0};
                    if (!TriangleContact(tiles[t], c, samples[s].x, samples[s].z, &y, &normal))
                    {
                        continue;
                    }
                    float penetration = y - samples[s].y;
                    if (penetration > best.penetration)
                    {
                        best.hit = true;
                        best.penetration = penetration;
                        best.normal = normal;
                        best.point = samples[s];
                        hit = true;
                    }
                }
            }
        }
    }

    if (!checkedAnyCell)
    {
        for (int s = 0; s < sampleCount; s++)
        {
            float y = Sample(samples[s].x, samples[s].z);
            float penetration = y - samples[s].y;
            if (penetration > best.penetration)
            {
                best.hit = true;
                best.penetration = penetration;
                best.normal = NormalAt(samples[s].x, samples[s].z);
                best.point = samples[s];
                hit = true;
            }
        }
    }

    if (hit && outContact)
    {
        *outContact = best;
    }
    return hit;
}

CollisionBoxSet::CollisionBoxSet()
    : object(nullptr), boxes(nullptr), boxCount(0), m_boxCapacity(0)
{
}

CollisionBoxSet::CollisionBoxSet(GameObject *gameObject)
    : object(gameObject), boxes(nullptr), boxCount(0), m_boxCapacity(0)
{
}

CollisionBoxSet::~CollisionBoxSet()
{
    delete[] boxes;
}

void CollisionBoxSet::BindGameObject(GameObject *gameObject)
{
    object = gameObject;
    for (int i = 0; i < boxCount; i++)
    {
        boxes[i].BindGameObject(gameObject);
    }
}

bool CollisionBoxSet::EnsureCapacity(int32_t count)
{
    if (count <= m_boxCapacity)
    {
        return true;
    }

    CollisionBox *next = new CollisionBox[count];
    if (!next)
    {
        return false;
    }
    for (int i = 0; i < boxCount; i++)
    {
        next[i] = boxes[i];
    }
    delete[] boxes;
    boxes = next;
    m_boxCapacity = count;
    for (int i = 0; i < boxCount; i++)
    {
        boxes[i].BindGameObject(object);
    }
    return true;
}

bool CollisionBoxSet::FitMeshGrid(int32_t xParts, int32_t yParts, int32_t zParts)
{
    return object && object->mesh ? FitMeshGrid(*object->mesh, xParts, yParts, zParts) : false;
}

bool CollisionBoxSet::FitMeshGrid(const Mesh& mesh, int32_t xParts, int32_t yParts, int32_t zParts)
{
    if (!mesh.vertices || mesh.vertCount <= 0 || xParts < 1 || yParts < 1 || zParts < 1)
    {
        boxCount = 0;
        return false;
    }

    float3 mn = mesh.vertices[0];
    float3 mx = mesh.vertices[0];
    for (int i = 1; i < mesh.vertCount; i++)
    {
        const float3& v = mesh.vertices[i];
        if (v.x < mn.x) mn.x = v.x;
        if (v.y < mn.y) mn.y = v.y;
        if (v.z < mn.z) mn.z = v.z;
        if (v.x > mx.x) mx.x = v.x;
        if (v.y > mx.y) mx.y = v.y;
        if (v.z > mx.z) mx.z = v.z;
    }

    int count = xParts * yParts * zParts;
    if (count <= 0 || !EnsureCapacity(count))
    {
        boxCount = 0;
        return false;
    }

    float3 size = mx - mn;
    float3 step = {size.x / (float)xParts, size.y / (float)yParts, size.z / (float)zParts};
    boxCount = 0;
    for (int z = 0; z < zParts; z++)
    {
        for (int y = 0; y < yParts; y++)
        {
            for (int x = 0; x < xParts; x++)
            {
                float3 minCorner = {
                    mn.x + step.x * (float)x,
                    mn.y + step.y * (float)y,
                    mn.z + step.z * (float)z
                };
                float3 maxCorner = {
                    (x == xParts - 1) ? mx.x : minCorner.x + step.x,
                    (y == yParts - 1) ? mx.y : minCorner.y + step.y,
                    (z == zParts - 1) ? mx.z : minCorner.z + step.z
                };

                CollisionBox& box = boxes[boxCount++];
                box.object = object;
                box.centerOffset = (minCorner + maxCorner) * 0.5f;
                box.halfExtents = (maxCorner - minCorner) * 0.5f;
            }
        }
    }
    return true;
}

bool CollisionBoxSet::IsValid() const
{
    return boxCount > 0 && boxes;
}

bool CollisionBoxSet::Contains(float3 point) const
{
    if (!IsValid())
    {
        return false;
    }
    for (int i = 0; i < boxCount; i++)
    {
        if (boxes[i].Contains(point))
        {
            return true;
        }
    }
    return false;
}

bool CollisionBoxSet::Intersects(const CollisionBox& other) const
{
    if (!IsValid())
    {
        return false;
    }
    for (int i = 0; i < boxCount; i++)
    {
        if (boxes[i].Intersects(other))
        {
            return true;
        }
    }
    return false;
}

RayHit CollisionBoxSet::Raycast(const Ray& ray) const
{
    RayHit hit;
    RaycastCollisionBoxes(ray, boxes, boxCount, &hit, nullptr);
    return hit;
}

bool CollisionBox::Intersects(const CollisionBox& other) const
{
    if (!IsValid() || !other.IsValid())
    {
        return false;
    }

    float3 a[3] = {AxisX(), AxisY(), AxisZ()};
    float3 b[3] = {other.AxisX(), other.AxisY(), other.AxisZ()};
    float  ea[3] = {halfExtents.x, halfExtents.y, halfExtents.z};
    float  eb[3] = {other.halfExtents.x, other.halfExtents.y, other.halfExtents.z};
    float  r[3][3];
    float  ar[3][3];

    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            r[i][j] = float3::dot(a[i], b[j]);
            ar[i][j] = fabsf(r[i][j]) + 0.000001f;
        }
    }

    float3 delta = other.Center() - Center();
    float t[3] = {
        float3::dot(delta, a[0]),
        float3::dot(delta, a[1]),
        float3::dot(delta, a[2])
    };

    for (int i = 0; i < 3; i++)
    {
        float ra = ea[i];
        float rb = eb[0] * ar[i][0] + eb[1] * ar[i][1] + eb[2] * ar[i][2];
        if (fabsf(t[i]) > ra + rb) return false;
    }

    for (int j = 0; j < 3; j++)
    {
        float ra = ea[0] * ar[0][j] + ea[1] * ar[1][j] + ea[2] * ar[2][j];
        float rb = eb[j];
        float tj = fabsf(t[0] * r[0][j] + t[1] * r[1][j] + t[2] * r[2][j]);
        if (tj > ra + rb) return false;
    }

    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            if (float3::cross(a[i], b[j]).lengthSq() <= 0.0000001f)
            {
                continue;
            }

            float ra = ea[(i + 1) % 3] * ar[(i + 2) % 3][j] +
                       ea[(i + 2) % 3] * ar[(i + 1) % 3][j];
            float rb = eb[(j + 1) % 3] * ar[i][(j + 2) % 3] +
                       eb[(j + 2) % 3] * ar[i][(j + 1) % 3];
            float tv = fabsf(t[(i + 2) % 3] * r[(i + 1) % 3][j] -
                             t[(i + 1) % 3] * r[(i + 2) % 3][j]);
            if (tv > ra + rb) return false;
        }
    }

    return true;
}

RayHit CollisionBox::Raycast(const Ray& ray) const
{
    RayHit result;
    if (!IsValid())
    {
        return result;
    }

    float3 c = Center();
    float3 ax = AxisX();
    float3 ay = AxisY();
    float3 az = AxisZ();
    float3 localOriginDelta = ray.origin - c;
    float3 localOrigin = {
        float3::dot(localOriginDelta, ax),
        float3::dot(localOriginDelta, ay),
        float3::dot(localOriginDelta, az)
    };
    float3 localDirection = {
        float3::dot(ray.direction, ax),
        float3::dot(ray.direction, ay),
        float3::dot(ray.direction, az)
    };

    Ray localRay(localOrigin, localDirection);
    AABB localBox(-halfExtents, halfExtents);
    float nearDistance = 0.0f;
    float farDistance = 0.0f;
    if (!localRay.IntersectAABB(localBox, &nearDistance, &farDistance))
    {
        return result;
    }

    result.hit = true;
    result.distance = nearDistance >= 0.0f ? nearDistance : farDistance;
    result.position = ray.At(result.distance);

    float3 p = localRay.At(result.distance);
    float3 n = {0,0,0};
    float dx = fabsf(fabsf(p.x) - halfExtents.x);
    float dy = fabsf(fabsf(p.y) - halfExtents.y);
    float dz = fabsf(fabsf(p.z) - halfExtents.z);
    if (dx <= dy && dx <= dz) n = {p.x < 0.0f ? -1.0f : 1.0f, 0, 0};
    else if (dy <= dz)        n = {0, p.y < 0.0f ? -1.0f : 1.0f, 0};
    else                      n = {0, 0, p.z < 0.0f ? -1.0f : 1.0f};
    result.normal = (ax * n.x + ay * n.y + az * n.z).normalizeFast();
    return result;
}

// ============================================================================
// [4] PhysicsBody / 物理刚体
// ============================================================================
PhysicsBody::PhysicsBody()
    : object(nullptr), velocity{0,0,0}, angularVelocity{0,0,0},
      force{0,0,0}, torque{0,0,0}, gravity{0,-9.8f,0},
      mass(1.0f), inverseMass(1.0f), inertia(1.0f), inverseInertia(1.0f),
      linearDamping(0.0f), angularDamping(0.0f),
      restitution(0.0f), friction(0.5f), drag(0.0f),
      material(), m_enabled(false)
{
}

PhysicsBody::PhysicsBody(GameObject *gameObject)
    : object(gameObject), velocity{0,0,0}, angularVelocity{0,0,0},
      force{0,0,0}, torque{0,0,0}, gravity{0,-9.8f,0},
      mass(1.0f), inverseMass(1.0f), inertia(1.0f), inverseInertia(1.0f),
      linearDamping(0.0f), angularDamping(0.0f),
      restitution(0.0f), friction(0.5f), drag(0.0f),
      material(), m_enabled(false)
{
}

void PhysicsBody::BindGameObject(GameObject *gameObject)
{
    object = gameObject;
}

void PhysicsBody::SetEnabled(bool enabled)
{
    m_enabled = enabled;
}

bool PhysicsBody::IsEnabled() const
{
    return m_enabled;
}

void PhysicsBody::SetMass(float value)
{
    mass = value;
    inverseMass = (value > 0.0f) ? 1.0f / value : 0.0f;
}

void PhysicsBody::SetInertia(float value)
{
    inertia = value;
    inverseInertia = (value > 0.0f) ? 1.0f / value : 0.0f;
}

void PhysicsBody::SetMaterial(const PhysicsMaterial& value)
{
    material = value;
    restitution = value.restitution;
    friction = value.friction;
    linearDamping = value.linearDamping;
    angularDamping = value.angularDamping;
    drag = value.drag;
}

void PhysicsBody::SetVelocity(float3 value)
{
    velocity = value;
}

void PhysicsBody::SetAngularVelocity(float3 value)
{
    angularVelocity = value;
}

void PhysicsBody::AddForce(float3 value)
{
    force = force + value;
}

void PhysicsBody::AddImpulse(float3 impulse)
{
    velocity = velocity + impulse * inverseMass;
}

void PhysicsBody::AddAngularImpulse(float3 impulse)
{
    angularVelocity = angularVelocity + impulse * inverseInertia;
}

void PhysicsBody::AddTorque(float3 value)
{
    torque = torque + value;
}

void PhysicsBody::ClearForces()
{
    force = {0,0,0};
}

void PhysicsBody::ClearTorques()
{
    torque = {0,0,0};
}

void PhysicsBody::Step(float deltaTime)
{
    if (!m_enabled || !object || deltaTime <= 0.0f)
    {
        return;
    }

    float3 totalForce = force + gravity * mass;
    float3 totalTorque = torque;
    velocity = velocity + totalForce * inverseMass * deltaTime;
    angularVelocity = angularVelocity + totalTorque * inverseInertia * deltaTime;

    if (linearDamping > 0.0f)
    {
        float damp = 1.0f - linearDamping * deltaTime;
        if (damp < 0.0f) damp = 0.0f;
        velocity = velocity * damp;
    }
    if (angularDamping > 0.0f)
    {
        float damp = 1.0f - angularDamping * deltaTime;
        if (damp < 0.0f) damp = 0.0f;
        angularVelocity = angularVelocity * damp;
    }
    if (drag > 0.0f)
    {
        float dragScale = 1.0f - drag * deltaTime;
        if (dragScale < 0.0f) dragScale = 0.0f;
        velocity = velocity * dragScale;
    }
    object->position = object->position + velocity * deltaTime;
    quat dq = quat(1.0f,
                   0.5f * angularVelocity.x * deltaTime,
                   0.5f * angularVelocity.y * deltaTime,
                   0.5f * angularVelocity.z * deltaTime);
    object->orientation = (object->orientation * dq).normalizeFast();

    ClearForces();
    ClearTorques();
}

bool PhysicsBody::StepWithCollisions(float deltaTime, CollisionBox& selfBox,
                                     const CollisionBox *obstacles, int32_t obstacleCount,
                                     int32_t substeps, PhysicsContact *outContact)
{
    return StepWithCollisions(deltaTime, selfBox, obstacles, nullptr, obstacleCount, substeps, outContact);
}

bool PhysicsBody::StepWithCollisions(float deltaTime, CollisionBox& selfBox,
                                     const CollisionBox *obstacles, const AABB *obstacleBounds,
                                     int32_t obstacleCount, int32_t substeps, PhysicsContact *outContact)
{
    if (outContact) *outContact = PhysicsContact();

    if (!m_enabled || !object || deltaTime <= 0.0f)
    {
        return false;
    }

    if (!obstacles || obstacleCount <= 0)
    {
        Step(deltaTime);
        return false;
    }

    if (selfBox.object != object)
    {
        selfBox.BindGameObject(object);
    }

    if (substeps < 1)
    {
        substeps = 1;
    }
    if (substeps > 64)
    {
        substeps = 64;
    }

    float3 totalForce = force + gravity * mass;
    float3 totalTorque = torque;
    velocity = velocity + totalForce * inverseMass * deltaTime;
    angularVelocity = angularVelocity + totalTorque * inverseInertia * deltaTime;
    if (linearDamping > 0.0f)
    {
        float damp = 1.0f - linearDamping * deltaTime;
        if (damp < 0.0f) damp = 0.0f;
        velocity = velocity * damp;
    }
    if (angularDamping > 0.0f)
    {
        float damp = 1.0f - angularDamping * deltaTime;
        if (damp < 0.0f) damp = 0.0f;
        angularVelocity = angularVelocity * damp;
    }
    if (drag > 0.0f)
    {
        float dragScale = 1.0f - drag * deltaTime;
        if (dragScale < 0.0f) dragScale = 0.0f;
        velocity = velocity * dragScale;
    }

    float stepDeltaTime = deltaTime / (float)substeps;

    bool collided = false;
    for (int step = 0; step < substeps; step++)
    {
        float3 stepMove = velocity * stepDeltaTime;
        if (stepMove.lengthSq() < 0.00000001f)
        {
            stepMove = {0, 0, 0};
        }
        object->position = object->position + stepMove;

        quat stepRotation = quat(1.0f,
                                 0.5f * angularVelocity.x * stepDeltaTime,
                                 0.5f * angularVelocity.y * stepDeltaTime,
                                 0.5f * angularVelocity.z * stepDeltaTime);
        object->orientation = (object->orientation * stepRotation).normalizeFast();

        for (int resolve = 0; resolve < 4; resolve++)
        {
            bool hit = false;
            PhysicsContact bestContact;
            AABB selfBounds = selfBox.WorldAABB();
            for (int i = 0; i < obstacleCount; i++)
            {
                if (obstacleBounds && !selfBounds.Intersects(obstacleBounds[i]))
                {
                    continue;
                }
                PhysicsContact contact;
                if (selfBox.Contact(obstacles[i], &contact))
                {
                    if (!hit || contact.penetration < bestContact.penetration)
                    {
                        bestContact = contact;
                    }
                    hit = true;
                }
            }

            if (!hit)
            {
                break;
            }

            const float slop = 0.0005f;
            object->position = object->position + bestContact.normal * (bestContact.penetration + slop);
            if (outContact)
            {
                *outContact = bestContact;
            }

            float normalLengthSq = bestContact.normal.lengthSq();
            float invNormalLengthSq = normalLengthSq > 0.0000001f ? 1.0f / normalLengthSq : 0.0f;
            float vn = float3::dot(velocity, bestContact.normal);
            if (vn < 0.0f)
            {
                velocity = velocity - bestContact.normal * ((1.0f + restitution) * vn * invNormalLengthSq);
            }

            float3 tangent = velocity - bestContact.normal * (float3::dot(velocity, bestContact.normal) * invNormalLengthSq);
            float frictionScale = HE3D_MAX(0.0f, 1.0f - friction);
            velocity = tangent * frictionScale + bestContact.normal * (float3::dot(velocity, bestContact.normal) * invNormalLengthSq);
            angularVelocity = angularVelocity * HE3D_MAX(0.0f, 1.0f - friction * 0.2f);
            collided = true;
        }
    }

    ClearForces();
    ClearTorques();
    return collided;
}

bool PhysicsBody::ResolveHeightField(CollisionBox& selfBox,
                                     const HeightFieldCollider& heightField,
                                     float sweep,
                                     PhysicsContact *outContact)
{
    return ResolveHeightField(selfBox, heightField, nullptr, 0, sweep, outContact);
}

bool PhysicsBody::ResolveHeightField(CollisionBox& selfBox,
                                     const HeightFieldCollider& heightField,
                                     const HeightFieldCollider::Tile *tiles,
                                     int32_t tileCount,
                                     float sweep,
                                     PhysicsContact *outContact)
{
    if (outContact) *outContact = PhysicsContact();
    if (!m_enabled || !object || !heightField.IsValid())
    {
        return false;
    }

    if (selfBox.object != object)
    {
        selfBox.BindGameObject(object);
    }

    PhysicsContact contact;
    if (!heightField.ContactBox(selfBox, tiles, tileCount, sweep, &contact))
    {
        return false;
    }

    object->position = object->position + contact.normal * (contact.penetration + 0.01f);
    float normalLengthSq = contact.normal.lengthSq();
    float invNormalLengthSq = normalLengthSq > 0.0000001f ? 1.0f / normalLengthSq : 0.0f;
    float vn = float3::dot(velocity, contact.normal);
    if (vn < 0.0f)
    {
        velocity = velocity - contact.normal * (vn * invNormalLengthSq);
    }

    float3 normalVelocity = contact.normal * (float3::dot(velocity, contact.normal) * invNormalLengthSq);
    float3 tangentVelocity = velocity - normalVelocity;
    velocity = normalVelocity + tangentVelocity * HE3D_MAX(0.0f, 1.0f - friction * 0.2f);
    angularVelocity = angularVelocity * HE3D_MAX(0.0f, 1.0f - friction * 0.1f);

    if (outContact)
    {
        *outContact = contact;
    }
    return true;
}

// ============================================================================
// [5] Mesh allocation helpers.
// [5] Mesh 分配辅助函数。
// ============================================================================
bool Mesh::Init(int32_t vertexCount)
{
    delete[] vertices;
    delete[] uvs;
    delete[] triNormals;
    vertices = nullptr;
    uvs = nullptr;
    triNormals = nullptr;
    vertCount = 0;
    capacity = 0;

    if (vertexCount <= 0)
    {
        return false;
    }

    vertices = new float3[vertexCount];
    uvs = new float2[vertexCount];
    triNormals = new float3[(vertexCount + 2) / 3];
    if (!vertices || !uvs || !triNormals)
    {
        delete[] vertices;
        delete[] uvs;
        delete[] triNormals;
        vertices = nullptr;
        uvs = nullptr;
        triNormals = nullptr;
        return false;
    }

    vertCount = vertexCount;
    capacity = vertexCount;
    return true;
}

Mesh *Mesh::Create(int32_t vertexCount)
{
    Mesh *mesh = new Mesh();
    if (!mesh || !mesh->Init(vertexCount))
    {
        delete mesh;
        return nullptr;
    }
    return mesh;
}

bool Mesh::Init(const float3 *srcVertices, const float2 *srcUvs, int32_t vertexCount)
{
    if (!srcVertices || !Init(vertexCount))
    {
        return false;
    }

    for (int i = 0; i < vertexCount; i++)
    {
        vertices[i] = srcVertices[i];
        uvs[i]      = srcUvs ? srcUvs[i] : float2(0.0f, 0.0f);
    }
    RecalculateTriangleNormals();
    return true;
}

void Mesh::RecalculateTriangleNormals()
{
    if (!vertices || !triNormals)
    {
        return;
    }

    int triCount = vertCount / 3;
    for (int i = 0; i < triCount; i++)
    {
        int v = i * 3;
        float3 e1 = vertices[v + 1] - vertices[v];
        float3 e2 = vertices[v + 2] - vertices[v];
        triNormals[i] = float3::cross(e1, e2).normalizeFast();
    }
}

Mesh *Mesh::Create(const float3 *srcVertices, const float2 *srcUvs,
                   int32_t vertexCount)
{
    Mesh *mesh = new Mesh();
    if (!mesh || !mesh->Init(srcVertices, srcUvs, vertexCount))
    {
        delete mesh;
        return nullptr;
    }
    return mesh;
}

static void WriteMeshTri(float3 *vertices, float2 *uvs, int32_t *index,
                         float3 a, float3 b, float3 c,
                         float2 uva, float2 uvb, float2 uvc)
{
    int32_t i = *index;
    vertices[i + 0] = a;
    vertices[i + 1] = b;
    vertices[i + 2] = c;
    uvs[i + 0] = uva;
    uvs[i + 1] = uvb;
    uvs[i + 2] = uvc;
    *index = i + 3;
}

Mesh *Mesh::CreateTriangle(float width, float height)
{
    if (width <= 0.0f || height <= 0.0f)
    {
        return nullptr;
    }

    float hx = width * 0.5f;
    float hy = height * 0.5f;
    float3 vertices[3] = {
        {-hx, -hy, 0.0f},
        { 0.0f,  hy, 0.0f},
        { hx, -hy, 0.0f}
    };
    float2 uvs[3] = {
        {0.0f, 0.0f},
        {0.5f, 1.0f},
        {1.0f, 0.0f}
    };
    return Mesh::Create(vertices, uvs, 3);
}

Mesh *Mesh::CreatePlane(float width, float depth)
{
    if (width <= 0.0f || depth <= 0.0f)
    {
        return nullptr;
    }

    float hx = width * 0.5f;
    float hz = depth * 0.5f;
    float3 vertices[6] = {
        {-hx, 0.0f, -hz}, { hx, 0.0f,  hz}, { hx, 0.0f, -hz},
        {-hx, 0.0f, -hz}, {-hx, 0.0f,  hz}, { hx, 0.0f,  hz}
    };
    float2 uvs[6] = {
        {0.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 0.0f},
        {0.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f}
    };
    return Mesh::Create(vertices, uvs, 6);
}

Mesh *Mesh::CreateCube(float width, float height, float depth)
{
    if (width <= 0.0f || height <= 0.0f || depth <= 0.0f)
    {
        return nullptr;
    }

    float hx = width * 0.5f;
    float hy = height * 0.5f;
    float hz = depth * 0.5f;
    float3 p[8] = {
        {-hx, -hy, -hz}, { hx, -hy, -hz}, { hx,  hy, -hz}, {-hx,  hy, -hz},
        {-hx, -hy,  hz}, { hx, -hy,  hz}, { hx,  hy,  hz}, {-hx,  hy,  hz}
    };

    float3 vertices[36];
    float2 uvs[36];
    int32_t i = 0;
    float2 uv0 = {0.0f, 0.0f};
    float2 uv1 = {1.0f, 0.0f};
    float2 uv2 = {1.0f, 1.0f};
    float2 uv3 = {0.0f, 1.0f};

    WriteMeshTri(vertices, uvs, &i, p[4], p[5], p[6], uv0, uv1, uv2);
    WriteMeshTri(vertices, uvs, &i, p[4], p[6], p[7], uv0, uv2, uv3);
    WriteMeshTri(vertices, uvs, &i, p[1], p[0], p[3], uv0, uv1, uv2);
    WriteMeshTri(vertices, uvs, &i, p[1], p[3], p[2], uv0, uv2, uv3);
    WriteMeshTri(vertices, uvs, &i, p[0], p[4], p[7], uv0, uv1, uv2);
    WriteMeshTri(vertices, uvs, &i, p[0], p[7], p[3], uv0, uv2, uv3);
    WriteMeshTri(vertices, uvs, &i, p[5], p[1], p[2], uv0, uv1, uv2);
    WriteMeshTri(vertices, uvs, &i, p[5], p[2], p[6], uv0, uv2, uv3);
    WriteMeshTri(vertices, uvs, &i, p[3], p[7], p[6], uv0, uv1, uv2);
    WriteMeshTri(vertices, uvs, &i, p[3], p[6], p[2], uv0, uv2, uv3);
    WriteMeshTri(vertices, uvs, &i, p[0], p[1], p[5], uv0, uv1, uv2);
    WriteMeshTri(vertices, uvs, &i, p[0], p[5], p[4], uv0, uv2, uv3);

    return Mesh::Create(vertices, uvs, 36);
}

Mesh *Mesh::CreateSphere(float radius, int32_t segments, int32_t rings)
{
    if (radius <= 0.0f)
    {
        return nullptr;
    }
    if (segments < 3) segments = 3;
    if (rings < 2) rings = 2;

    int32_t vertexCount = segments * rings * 6;
    Mesh *mesh = Mesh::Create(vertexCount);
    if (!mesh)
    {
        return nullptr;
    }

    int32_t i = 0;
    for (int32_t ring = 0; ring < rings; ring++)
    {
        float v0 = (float)ring / (float)rings;
        float v1 = (float)(ring + 1) / (float)rings;
        float lat0 = (v0 - 0.5f) * HE3D_PI;
        float lat1 = (v1 - 0.5f) * HE3D_PI;
        float y0 = sinf(lat0) * radius;
        float y1 = sinf(lat1) * radius;
        float r0 = cosf(lat0) * radius;
        float r1 = cosf(lat1) * radius;

        for (int32_t seg = 0; seg < segments; seg++)
        {
            float u0 = (float)seg / (float)segments;
            float u1 = (float)(seg + 1) / (float)segments;
            float lon0 = u0 * HE3D_PI * 2.0f;
            float lon1 = u1 * HE3D_PI * 2.0f;
            float3 p00 = {sinf(lon0) * r0, y0, cosf(lon0) * r0};
            float3 p01 = {sinf(lon1) * r0, y0, cosf(lon1) * r0};
            float3 p10 = {sinf(lon0) * r1, y1, cosf(lon0) * r1};
            float3 p11 = {sinf(lon1) * r1, y1, cosf(lon1) * r1};

            WriteMeshTri(mesh->vertices, mesh->uvs, &i, p00, p11, p10,
                         {u0, v0}, {u1, v1}, {u0, v1});
            WriteMeshTri(mesh->vertices, mesh->uvs, &i, p00, p01, p11,
                         {u0, v0}, {u1, v0}, {u1, v1});
        }
    }

    mesh->RecalculateTriangleNormals();
    return mesh;
}

// ============================================================================
// [6] Mesh::LoadOBJ: two passes over in-memory file data.
// [6] Mesh::LoadOBJ：对内存文件数据执行两次扫描。
// ============================================================================
Mesh *Mesh::LoadOBJ(const char *filename)
{
    FileData file = {};
    if (!LoadFile(filename, &file) || !file.data || file.length < 4)
    {
        if (file.data)
        {
            CloseFile(&file);
        }
        return nullptr;
    }

    const char *buf = (const char *)file.data;
    const char *end = buf + file.length;

    // Pass 1 counts vertices, UVs, and output triangles.
    // 第一遍统计顶点、UV 和输出三角形数量。
    int vCount = 0, vtCount = 0, triCount = 0;
    const char *c = buf;
    char line[512];
    while ((c = MemGetLine(c, end, line, sizeof(line))) != nullptr) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (p[0] == 'v' && p[1] == ' ') vCount++;
        else if (p[0] == 'v' && p[1] == 't' && (p[2] == ' ' || p[2] == '\t')) vtCount++;
        else if (p[0] == 'f' && p[1] == ' ') {
            int fv = CountFaceVerts(p + 2);
            if (fv >= 3) triCount += fv - 2;
        }
    }
    if (vCount == 0 || triCount == 0)
    {
        CloseFile(&file);
        return nullptr;
    }

    // Allocate temporary OBJ buffers after the first pass establishes exact sizes.
    // 第一遍得到准确数量后，再分配 OBJ 临时缓冲区。
    float3 *all_v  = new float3[vCount];
    float2 *all_vt = (vtCount > 0) ? new float2[vtCount] : nullptr;
    if (!all_v || (vtCount > 0 && !all_vt))
    {
        delete[] all_v;
        delete[] all_vt;
        CloseFile(&file);
        return nullptr;
    }
    int vc = 0, vtc = 0;

    int totalVerts = triCount * 3;
    Mesh *mesh = Mesh::Create(totalVerts);
    if (!mesh)
    {
        delete[] all_v;
        delete[] all_vt;
        CloseFile(&file);
        return nullptr;
    }
    mesh->vertCount = 0; // incremented while faces are emitted / 输出 face 时递增

    // Pass 2 writes triangulated vertices and UVs.
    // 第二遍写入三角化后的顶点和 UV。
    c = buf;
    while ((c = MemGetLine(c, end, line, sizeof(line))) != nullptr) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;

        if (p[0] == 'v' && p[1] == ' ') {
            float3 vtx;
            const char *py = ParseFloat(p + 2, &vtx.x);
            const char *pz = ParseFloat(py, &vtx.y);
            ParseFloat(pz, &vtx.z);
            if (vc < vCount) all_v[vc++] = vtx;
        }
        else if (p[0] == 'v' && p[1] == 't' && (p[2] == ' ' || p[2] == '\t') && all_vt) {
            float2 uv;
            const char *pv = ParseFloat(p + 3, &uv.x);
            float rawV; ParseFloat(pv, &rawV); uv.y = 1.0f - rawV;
            if (vtc < vtCount) all_vt[vtc++] = uv;
        }
        else if (p[0] == 'f' && p[1] == ' ') {
            struct FV { int v, vt; };
            FV fv[32]; int fc = 0;
            char *tok = p + 2;
            bool validFaceSyntax = true;
            while (*tok) {
                while (*tok == ' ' || *tok == '\t') tok++;
                if (*tok == 0 || *tok == '\n' || *tok == '\r') break;
                if (*tok == '-') { validFaceSyntax = false; break; }
                if (fc >= 32) { validFaceSyntax = false; break; }
                int vi = 0, ti = 0;
                bool hasVertexIndex = false;
                while (*tok >= '0' && *tok <= '9') { vi = vi * 10 + (*tok - '0'); tok++; }
                hasVertexIndex = vi > 0;
                if (*tok == '/') {
                    tok++;
                    if (*tok != '/') { ti = 0; while (*tok >= '0' && *tok <= '9') { ti = ti * 10 + (*tok - '0'); tok++; } }
                    if (*tok == '/') { tok++; while (*tok >= '0' && *tok <= '9') tok++; }
                }
                while (*tok && *tok != ' ' && *tok != '\t' && *tok != '\n' && *tok != '\r') tok++;
                if (!hasVertexIndex) { validFaceSyntax = false; break; }
                fv[fc].v = vi - 1;
                fv[fc].vt = ti - 1;
                fc++;
            }

            bool validFace = validFaceSyntax && (fc >= 3);
            for (int i = 0; i < fc && validFace; i++)
            {
                if (fv[i].v < 0 || fv[i].v >= vc)
                {
                    validFace = false;
                }
                if (fv[i].vt >= vtc)
                {
                    validFace = false;
                }
            }
            if (!validFace)
            {
                continue;
            }

            for (int i = 1; i < fc - 1 && mesh->vertCount + 3 <= mesh->capacity; i++)
            {
                int idx = mesh->vertCount;
                bool hasUvs = all_vt && fv[0].vt >= 0 && fv[i].vt >= 0
                           && fv[i + 1].vt >= 0;

                mesh->vertices[idx]     = all_v[fv[0].v];
                mesh->vertices[idx + 1] = all_v[fv[i].v];
                mesh->vertices[idx + 2] = all_v[fv[i + 1].v];
                if (hasUvs)
                {
                    mesh->uvs[idx]     = all_vt[fv[0].vt];
                    mesh->uvs[idx + 1] = all_vt[fv[i].vt];
                    mesh->uvs[idx + 2] = all_vt[fv[i + 1].vt];
                }
                else
                {
                    mesh->uvs[idx] = mesh->uvs[idx + 1] = mesh->uvs[idx + 2] = {0,0};
                }
                mesh->vertCount += 3;
            }
        }
    }

    delete[] all_v;
    delete[] all_vt;
    if (mesh->vertCount == 0)
    {
        delete mesh;
        CloseFile(&file);
        return nullptr;
    }
    mesh->RecalculateTriangleNormals();
    CloseFile(&file);
    return mesh;
}

// ============================================================================
// [7] Texture::Sample: nearest-neighbor sampling with UV wrap.
// [7] Texture::Sample：带 UV 环绕的最近邻采样。
// ============================================================================
color3 Texture::Sample(float u, float v) const {
    if (!valid || !pixels || width <= 0 || height <= 0) return {1.0f, 0.0f, 1.0f};
    u = fracf(u);
    v = fracf(v);
    int x = HE3D_CLAMP((int)(u * width),  0, width  - 1);
    int y = HE3D_CLAMP((int)(v * height), 0, height - 1);
    const ColorA& c = pixels[y * width + x];
    return {c.r * (1.0f / 255.0f), c.g * (1.0f / 255.0f), c.b * (1.0f / 255.0f)};
}

// ============================================================================
// [8] Texture loaders / 纹理加载
// ============================================================================
Texture *Texture::LoadBMP(const char *filename)
{
    FileData file = {};
    if (!LoadFile(filename, &file) || !file.data || file.length < 54)
    {
        if (file.data)
        {
            CloseFile(&file);
        }
        return nullptr;
    }

    const uint8_t *d = file.data;
    unsigned int   flen = (unsigned int)file.length;
    int            bw = 0, bh = 0, bpp = 0, compr = 0;
    unsigned int   off = 0;

    if (d[0] != 'B' || d[1] != 'M')
    {
        CloseFile(&file);
        return nullptr;
    }
    off   = d[10] | (d[11] << 8) | (d[12] << 16) | (d[13] << 24);
    if (off >= flen)
    {
        CloseFile(&file);
        return nullptr;
    }
    bw    = (int)(d[18] | (d[19] << 8) | (d[20] << 16) | (d[21] << 24));
    bh    = (int)(d[22] | (d[23] << 8) | (d[24] << 16) | (d[25] << 24));
    bpp   = (int)(d[28] | (d[29] << 8));
    compr = (int)(d[30] | (d[31] << 8) | (d[32] << 16) | (d[33] << 24));

    int texHeight = (bh < 0) ? -bh : bh;
    if (bw <= 0 || texHeight <= 0 || compr != 0)
    {
        CloseFile(&file);
        return nullptr;
    }

    int bytesPerPixel = 0;
    if (bpp == 24)
    {
        bytesPerPixel = 3;
    }
    else if (bpp == 32)
    {
        bytesPerPixel = 4;
    }
    else
    {
        CloseFile(&file);
        return nullptr;
    }

    const int MAX_BMP_DIMENSION = 8192;
    if (bw > MAX_BMP_DIMENSION || texHeight > MAX_BMP_DIMENSION)
    {
        CloseFile(&file);
        return nullptr;
    }

    unsigned int rowSize = ((unsigned int)bw * (unsigned int)bytesPerPixel + 3U) & ~3U;
    unsigned int dataSize = flen - off;
    if (rowSize == 0 || dataSize / rowSize < (unsigned int)texHeight)
    {
        CloseFile(&file);
        return nullptr;
    }

    Texture *tex = new Texture();
    if (!tex)
    {
        CloseFile(&file);
        return nullptr;
    }

    tex->width  = bw;
    tex->height = texHeight;
    ColorA *pbuf = new ColorA[tex->width * tex->height];
    if (!pbuf)
    {
        delete tex;
        CloseFile(&file);
        return nullptr;
    }

    {
        bool topDown = (bh < 0);
        const uint8_t *src = d + off;
        for (int y = 0; y < tex->height; y++) {
            int sY = topDown ? y : (tex->height - 1 - y);
            const uint8_t *row = src + sY * rowSize;
            ColorA *dst = pbuf + y * tex->width;
            for (int x = 0; x < tex->width; x++) {
                dst[x] = {row[2], row[1], row[0], bytesPerPixel == 4 ? row[3] : (uint8_t)255};
                row += bytesPerPixel;
            }
        }
    }
    tex->pixels = pbuf;
    tex->valid  = true;

    CloseFile(&file);
    return tex;
}

static Texture *LoadStbiTexture(const char *filename, bool requirePng, bool requireJpg)
{
    FileData file = {};
    if (!LoadFile(filename, &file) || !file.data || file.length < 8)
    {
        if (file.data)
        {
            CloseFile(&file);
        }
        return nullptr;
    }

    const uint8_t *d = file.data;
    bool isPng = file.length >= 8 &&
                 d[0] == 0x89 && d[1] == 'P' && d[2] == 'N' && d[3] == 'G' &&
                 d[4] == 0x0D && d[5] == 0x0A && d[6] == 0x1A && d[7] == 0x0A;
    bool isJpg = file.length >= 3 && d[0] == 0xFF && d[1] == 0xD8 && d[2] == 0xFF;
    if ((requirePng && !isPng) || (requireJpg && !isJpg))
    {
        CloseFile(&file);
        return nullptr;
    }
    if (file.length > 0x7fffffffU)
    {
        CloseFile(&file);
        return nullptr;
    }

    int width = 0;
    int height = 0;
    int comp = 0;
    stbi_uc *decoded = stbi_load_from_memory((const stbi_uc *)file.data,
                                             (int)file.length,
                                             &width, &height, &comp, 4);
    CloseFile(&file);
    if (!decoded || width <= 0 || height <= 0)
    {
        if (decoded)
        {
            stbi_image_free(decoded);
        }
        return nullptr;
    }

    const int MAX_IMAGE_DIMENSION = 8192;
    if (width > MAX_IMAGE_DIMENSION || height > MAX_IMAGE_DIMENSION)
    {
        stbi_image_free(decoded);
        return nullptr;
    }

    Texture *tex = new Texture();
    if (!tex)
    {
        stbi_image_free(decoded);
        return nullptr;
    }

    ColorA *pixels = new ColorA[width * height];
    if (!pixels)
    {
        delete tex;
        stbi_image_free(decoded);
        return nullptr;
    }

    const stbi_uc *src = decoded;
    for (int i = 0; i < width * height; i++)
    {
        pixels[i].r = src[i * 4 + 0];
        pixels[i].g = src[i * 4 + 1];
        pixels[i].b = src[i * 4 + 2];
        pixels[i].a = src[i * 4 + 3];
    }
    stbi_image_free(decoded);

    tex->width = width;
    tex->height = height;
    tex->pixels = pixels;
    tex->valid = true;
    return tex;
}

Texture *Texture::LoadPNG(const char *filename)
{
    return LoadStbiTexture(filename, true, false);
}

Texture *Texture::LoadJPG(const char *filename)
{
    return LoadStbiTexture(filename, false, true);
}

Texture *Texture::LoadImage(const char *filename)
{
    FileData file = {};
    if (!LoadFile(filename, &file) || !file.data || file.length < 3)
    {
        if (file.data)
        {
            CloseFile(&file);
        }
        return nullptr;
    }

    const uint8_t *d = file.data;
    bool isBmp = file.length >= 2 && d[0] == 'B' && d[1] == 'M';
    bool isPng = file.length >= 8 &&
                 d[0] == 0x89 && d[1] == 'P' && d[2] == 'N' && d[3] == 'G' &&
                 d[4] == 0x0D && d[5] == 0x0A && d[6] == 0x1A && d[7] == 0x0A;
    bool isJpg = d[0] == 0xFF && d[1] == 0xD8 && d[2] == 0xFF;
    CloseFile(&file);

    if (isBmp)
    {
        return LoadBMP(filename);
    }
    if (isPng)
    {
        return LoadPNG(filename);
    }
    if (isJpg)
    {
        return LoadJPG(filename);
    }
    return nullptr;
}

// ============================================================================
// [9] Renderer / 渲染器
// ============================================================================
Renderer::Renderer(Window *window, int32_t w, int32_t h)
    : m_width(w), m_height(h), m_outputWidth(w), m_outputHeight(h),
      m_ssaaScale(GetSsaaScale()), m_window(window)
{
    if (w <= 0 || h <= 0)
    {
        m_width = 0;
        m_height = 0;
        m_outputWidth = 0;
        m_outputHeight = 0;
        m_ssaaScale = 1;
        m_colorBuf = nullptr;
        m_fxaaBuf = nullptr;
        m_taaBuf = nullptr;
        m_taaHistory = nullptr;
        m_taaDepth = nullptr;
        m_ssaaBuf = nullptr;
        m_taaValid = false;
        m_taaFrameIndex = 0;
        m_depthBuf = nullptr;
        return;
    }

    if (m_ssaaScale < 1) m_ssaaScale = 1;
    if (m_ssaaScale > 4) m_ssaaScale = 4;
    m_width = w * (int32_t)m_ssaaScale;
    m_height = h * (int32_t)m_ssaaScale;

    int sz = m_width * m_height;
    m_colorBuf = new ColorA[sz];
    m_fxaaBuf = nullptr;
    m_taaBuf = nullptr;
    m_taaHistory = nullptr;
    m_taaDepth = nullptr;
    m_ssaaBuf = nullptr;
    m_taaValid = false;
    m_taaFrameIndex = 0;
    m_depthBuf = new float[sz];
    if (!m_colorBuf || !m_depthBuf)
    {
        delete[] m_colorBuf;
        delete[] m_depthBuf;
        m_colorBuf = nullptr;
        m_fxaaBuf = nullptr;
        m_taaBuf = nullptr;
        m_taaHistory = nullptr;
        m_taaDepth = nullptr;
        m_ssaaBuf = nullptr;
        m_taaValid = false;
        m_taaFrameIndex = 0;
        m_depthBuf = nullptr;
        m_width = 0;
        m_height = 0;
        m_outputWidth = 0;
        m_outputHeight = 0;
        m_ssaaScale = 1;
    }
}

Renderer::~Renderer() {
    delete[] m_colorBuf;
    delete[] m_fxaaBuf;
    delete[] m_taaBuf;
    delete[] m_taaHistory;
    delete[] m_taaDepth;
    delete[] m_ssaaBuf;
    delete[] m_depthBuf;
}

void Renderer::Resize(int32_t w, int32_t h) {
    delete[] m_colorBuf;
    delete[] m_fxaaBuf;
    delete[] m_taaBuf;
    delete[] m_taaHistory;
    delete[] m_taaDepth;
    delete[] m_ssaaBuf;
    delete[] m_depthBuf;
    m_fxaaBuf = nullptr;
    m_taaBuf = nullptr;
    m_taaHistory = nullptr;
    m_taaDepth = nullptr;
    m_ssaaBuf = nullptr;
    m_taaValid = false;
    m_taaFrameIndex = 0;
    m_colorBuf = nullptr;
    m_depthBuf = nullptr;

    if (w <= 0 || h <= 0)
    {
        m_width = 0;
        m_height = 0;
        m_outputWidth = 0;
        m_outputHeight = 0;
        m_ssaaScale = 1;
        return;
    }

    m_outputWidth = w;
    m_outputHeight = h;
    m_ssaaScale = GetSsaaScale();
    if (m_ssaaScale < 1) m_ssaaScale = 1;
    if (m_ssaaScale > 4) m_ssaaScale = 4;
    m_width = w * (int32_t)m_ssaaScale;
    m_height = h * (int32_t)m_ssaaScale;

    int sz = m_width * m_height;
    ColorA *nextColor = new ColorA[sz];
    float *nextDepth = new float[sz];
    if (!nextColor || !nextDepth)
    {
        delete[] nextColor;
        delete[] nextDepth;
        m_width = 0;
        m_height = 0;
        m_outputWidth = 0;
        m_outputHeight = 0;
        m_ssaaScale = 1;
        return;
    }

    m_colorBuf = nextColor;
    m_depthBuf = nextDepth;
}

void Renderer::Clear(color3 color) {
    if (m_width <= 0 || m_height <= 0 || !m_colorBuf || !m_depthBuf)
    {
        return;
    }

    ColorA c;
    c.r = (uint8_t)HE3D_CLAMP((int)(color.r * 255.0f), 0, 255);
    c.g = (uint8_t)HE3D_CLAMP((int)(color.g * 255.0f), 0, 255);
    c.b = (uint8_t)HE3D_CLAMP((int)(color.b * 255.0f), 0, 255);
    c.a = 255;

    int total = m_width * m_height;
    ColorA *colorBuf = m_colorBuf;
    float *depth = m_depthBuf;
    for (int i = 0; i < total; i++)
    {
        colorBuf[i] = c;
        depth[i] = 0.0f;
    }
}

static float Halton(uint32_t index, uint32_t base)
{
    float f = 1.0f;
    float r = 0.0f;
    while (index > 0)
    {
        f /= (float)base;
        r += f * (float)(index % base);
        index /= base;
    }
    return r;
}

float2 Renderer::CurrentTaaJitter() const
{
    if (!IsTaaEnabled() || m_width <= 0 || m_height <= 0)
    {
        return {0.0f, 0.0f};
    }

    uint32_t sample = (m_taaFrameIndex & 7U) + 1U;
    float x = Halton(sample, 2U) - 0.5f;
    float y = Halton(sample, 3U) - 0.5f;
    return {x * 0.75f, y * 0.75f};
}

// ============================================================================
// [10] DrawGameObject: solid color path.
// [10] DrawGameObject：纯色绘制路径。
// ============================================================================
void Renderer::DrawGameObject(const GameObject& obj, const Camera& cam, color3 color) {
    if (m_width <= 0 || m_height <= 0) return;
    if (!obj.mesh || obj.mesh->vertCount < 3) return;
    if (!obj.mesh->vertices || !obj.mesh->uvs) return;

    float fovRad    = cam.fov * 0.01745329252f;
    float fovScale  = 1.0f / tanf(fovRad * 0.5f);
    float halfW     = m_width  * 0.5f;
    float halfH     = m_height * 0.5f;
    float scaleX    = fovScale * halfW / ((float)m_width / (float)m_height);
    float scaleY    = fovScale * halfH;
    float2 jitter   = CurrentTaaJitter();
    float3 lightDir = mainLight.direction.normalizeFast();
    Mat3 camInvRot  = Mat3::FromQuat(cam.orientation).Transpose();
    bool identityObjRot = QuatIsIdentity(obj.orientation);
    Mat3 objRot     = identityObjRot ? Mat3{1,0,0,0,1,0,0,0,1} : Mat3::FromQuat(obj.orientation);

    float3* verts = obj.mesh->vertices;
    float3* triNormals = obj.mesh->triNormals;
    int vc = obj.mesh->vertCount;

    int triIndex = 0;
    for (int i = 0; i < vc; i += 3, triIndex++) {
        float3 vw[3], vv[3];
        ClipVertex clipIn[3];
        ClipVertex clipOut[4];

        for (int j = 0; j < 3; j++) {
            float3 v = identityObjRot ? (verts[i + j] + obj.position)
                                      : (objRot.Mul(verts[i + j]) + obj.position);
            vw[j] = v;
            v = camInvRot.Mul(v - cam.position);
            vv[j] = v;
            clipIn[j].view = v;
            clipIn[j].uv = {0, 0};
        }

        float3 ab = vv[1] - vv[0], ac = vv[2] - vv[0];
        if (float3::dot(float3::cross(ab, ac), vv[0]) >= 0) continue;

        int clippedCount = ClipTriangleToNearPlane(clipIn, clipOut);
        if (clippedCount < 3) continue;

        float3 n = triNormals ? (identityObjRot ? triNormals[triIndex] : objRot.Mul(triNormals[triIndex]))
                              : float3::cross(vw[1] - vw[0], vw[2] - vw[0]).normalizeFast();
        float diff = HE3D_MAX(0.0f, float3::dot(n, lightDir));
        float intens = mainLight.ambient + diff;

        for (int k = 1; k < clippedCount - 1; k++)
        {
            float3 clippedVv[3] = {clipOut[0].view, clipOut[k].view, clipOut[k + 1].view};
            float2 ps[3];
            ProjectViewTriangle(clippedVv, ps, halfW, halfH, scaleX, scaleY, jitter.x, jitter.y);
            if (TriangleOutsideViewport(ps, m_width, m_height)) continue;
            if (HE3D_ABS(ScreenTriangleArea(ps)) <= 0.0001f) continue;
            RasterizeSolid(clippedVv, ps, color * intens);
        }
    }
}

// ============================================================================
// [11] DrawGameObject: textured path.
// [11] DrawGameObject：纹理绘制路径。
// ============================================================================
void Renderer::DrawGameObject(const GameObject& obj, const Camera& cam, const Texture& tex) {
    if (m_width <= 0 || m_height <= 0) return;
    if (!obj.mesh || obj.mesh->vertCount < 3) return;
    if (!obj.mesh->vertices || !obj.mesh->uvs) return;
    if (!tex.valid) { DrawGameObject(obj, cam, color3(0.8f, 0.8f, 0.8f)); return; }

    float fovRad    = cam.fov * 0.01745329252f;
    float fovScale  = 1.0f / tanf(fovRad * 0.5f);
    float halfW     = m_width  * 0.5f;
    float halfH     = m_height * 0.5f;
    float scaleX    = fovScale * halfW / ((float)m_width / (float)m_height);
    float scaleY    = fovScale * halfH;
    float2 jitter   = CurrentTaaJitter();
    float3 lightDir = mainLight.direction.normalizeFast();
    bool identityObjRot = QuatIsIdentity(obj.orientation);
    Mat3 objRot     = identityObjRot ? Mat3{1,0,0,0,1,0,0,0,1} : Mat3::FromQuat(obj.orientation);
    Mat3 camInvRot  = Mat3::FromQuat(cam.orientation).Transpose();

    float3* verts = obj.mesh->vertices;
    float2* uvs   = obj.mesh->uvs;
    float3* triNormals = obj.mesh->triNormals;
    int vc = obj.mesh->vertCount;

    int triIndex = 0;
    for (int i = 0; i < vc; i += 3, triIndex++) {
        float3 vw[3], vv[3];
        ClipVertex clipIn[3];
        ClipVertex clipOut[4];

        for (int j = 0; j < 3; j++) {
            float3 v = identityObjRot ? (verts[i + j] + obj.position)
                                      : (objRot.Mul(verts[i + j]) + obj.position);
            vw[j]   = v;
            v = camInvRot.Mul(v - cam.position);
            vv[j] = v;
            clipIn[j].view = v;
            clipIn[j].uv = uvs[i + j];
        }

        float3 ab = vv[1] - vv[0], ac = vv[2] - vv[0];
        if (float3::dot(float3::cross(ab, ac), vv[0]) >= 0) continue;

        int clippedCount = ClipTriangleToNearPlane(clipIn, clipOut);
        if (clippedCount < 3) continue;

        float3 n = triNormals ? (identityObjRot ? triNormals[triIndex] : objRot.Mul(triNormals[triIndex]))
                              : float3::cross(vw[1] - vw[0], vw[2] - vw[0]).normalizeFast();
        float diff = HE3D_MAX(0.0f, float3::dot(n, lightDir));
        float intens = mainLight.ambient + diff;

        for (int k = 1; k < clippedCount - 1; k++)
        {
            float3 clippedVv[3] = {clipOut[0].view, clipOut[k].view, clipOut[k + 1].view};
            float2 clippedUvs[3] = {clipOut[0].uv, clipOut[k].uv, clipOut[k + 1].uv};
            float2 ps[3];
            ProjectViewTriangle(clippedVv, ps, halfW, halfH, scaleX, scaleY, jitter.x, jitter.y);
            if (TriangleOutsideViewport(ps, m_width, m_height)) continue;
            if (HE3D_ABS(ScreenTriangleArea(ps)) <= 0.0001f) continue;
            RasterizeTextured(clippedVv, ps, clippedUvs, intens, tex);
        }
    }
}

// ============================================================================
// [12] RasterizeSolid / 纯色三角形光栅化
// ============================================================================
static int TriangleCoverage4(int x, int y,
                             float x0, float y0, float x1, float y1, float x2, float y2,
                             float invA)
{
    static const float offsets[4][2] = {
        {0.25f, 0.25f},
        {0.75f, 0.25f},
        {0.25f, 0.75f},
        {0.75f, 0.75f}
    };

    int coverage = 0;
    for (int i = 0; i < 4; i++)
    {
        float px = (float)x + offsets[i][0];
        float py = (float)y + offsets[i][1];
        float w0 = ((x1 - px) * (y2 - py) - (y1 - py) * (x2 - px)) * invA;
        float w1 = ((x2 - px) * (y0 - py) - (y2 - py) * (x0 - px)) * invA;
        float w2 = 1.0f - w0 - w1;
        if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f)
        {
            coverage++;
        }
    }
    return coverage;
}

static ColorA BlendCoverage(ColorA dst, ColorA src, int coverage)
{
    if (coverage >= 4)
    {
        return src;
    }
    if (coverage <= 0)
    {
        return dst;
    }
    int inv = 4 - coverage;
    ColorA out;
    out.r = (uint8_t)(((int)src.r * coverage + (int)dst.r * inv) >> 2);
    out.g = (uint8_t)(((int)src.g * coverage + (int)dst.g * inv) >> 2);
    out.b = (uint8_t)(((int)src.b * coverage + (int)dst.b * inv) >> 2);
    out.a = 255;
    return out;
}

void Renderer::RasterizeSolid(const float3* vv, const float2* ps, color3 color) {
    float minXf = HE3D_MIN(HE3D_MIN(ps[0].x, ps[1].x), ps[2].x);
    float maxXf = HE3D_MAX(HE3D_MAX(ps[0].x, ps[1].x), ps[2].x);
    float minYf = HE3D_MIN(HE3D_MIN(ps[0].y, ps[1].y), ps[2].y);
    float maxYf = HE3D_MAX(HE3D_MAX(ps[0].y, ps[1].y), ps[2].y);

    int mnX = HE3D_MAX(0,             (int)minXf);
    int mxX = HE3D_MIN(m_width  - 1, (int)maxXf + 1);
    int mnY = HE3D_MAX(0,             (int)minYf);
    int mxY = HE3D_MIN(m_height - 1, (int)maxYf + 1);
    if (mnX > mxX || mnY > mxY) return;

    float x0 = ps[0].x, y0 = ps[0].y;
    float x1 = ps[1].x, y1 = ps[1].y;
    float x2 = ps[2].x, y2 = ps[2].y;

    float area = (x1 - x0) * (y2 - y0) - (y1 - y0) * (x2 - x0);
    if (HE3D_ABS(area) < 0.0001f) return;
    float invA = 1.0f / area;

    float dw0 = (y1 - y2) * invA;
    float dw1 = (y2 - y0) * invA;
    float dw2 = -dw0 - dw1;
    float iz0 = 1.0f / vv[0].z, iz1 = 1.0f / vv[1].z, iz2 = 1.0f / vv[2].z;
    float dizDx = dw0 * iz0 + dw1 * iz1 + dw2 * iz2;

    // Clamp the color once before the inner raster loop.
    // 进入内部光栅循环前只钳制一次颜色。
    ColorA xc;
    xc.r = (uint8_t)HE3D_CLAMP((int)(color.r * 255.0f), 0, 255);
    xc.g = (uint8_t)HE3D_CLAMP((int)(color.g * 255.0f), 0, 255);
    xc.b = (uint8_t)HE3D_CLAMP((int)(color.b * 255.0f), 0, 255);
    xc.a = 255;

    int stride = m_width;
    // idx is valid because the raster bounds are clamped to the framebuffer.
    // 由于光栅范围已钳制到 framebuffer 内，idx 一定有效。
    float *db = m_depthBuf;
    ColorA *cbuf = m_colorBuf;
    bool msaa = IsMsaaEnabled();

    for (int y = mnY; y <= mxY; y++) {
        float py = (float)y + 0.5f, px0 = (float)mnX + 0.5f;
        float w0 = ((x1 - px0) * (y2 - py) - (y1 - py) * (x2 - px0)) * invA;
        float w1 = ((x2 - px0) * (y0 - py) - (y2 - py) * (x0 - px0)) * invA;
        float w2 = 1.0f - w0 - w1;
        float ciz = w0 * iz0 + w1 * iz1 + w2 * iz2;

        int rb = y * stride;
        for (int x = mnX; x <= mxX; x++) {
            int idx = rb + x;
            bool centerInside = w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f;
            int coverage = centerInside ? 4 : 0;
            if (msaa)
            {
                coverage = TriangleCoverage4(x, y, x0, y0, x1, y1, x2, y2, invA);
            }
            if (coverage > 0 && ciz > db[idx]) {
                db[idx] = ciz;
                cbuf[idx] = msaa ? BlendCoverage(cbuf[idx], xc, coverage) : xc;
            }
            w0 += dw0;
            w1 += dw1;
            w2 += dw2;
            ciz += dizDx;
        }
    }
}

// ============================================================================
// [13] RasterizeTextured / 纹理三角形光栅化
// ============================================================================
void Renderer::RasterizeTextured(const float3* vv, const float2* ps,
                                  const float2* uvs, float intens, const Texture& tex) {
    float minXf = HE3D_MIN(HE3D_MIN(ps[0].x, ps[1].x), ps[2].x);
    float maxXf = HE3D_MAX(HE3D_MAX(ps[0].x, ps[1].x), ps[2].x);
    float minYf = HE3D_MIN(HE3D_MIN(ps[0].y, ps[1].y), ps[2].y);
    float maxYf = HE3D_MAX(HE3D_MAX(ps[0].y, ps[1].y), ps[2].y);

    int mnX = HE3D_MAX(0,             (int)minXf);
    int mxX = HE3D_MIN(m_width  - 1, (int)maxXf + 1);
    int mnY = HE3D_MAX(0,             (int)minYf);
    int mxY = HE3D_MIN(m_height - 1, (int)maxYf + 1);
    if (mnX > mxX || mnY > mxY) return;

    float x0 = ps[0].x, y0 = ps[0].y;
    float x1 = ps[1].x, y1 = ps[1].y;
    float x2 = ps[2].x, y2 = ps[2].y;

    float area = (x1 - x0) * (y2 - y0) - (y1 - y0) * (x2 - x0);
    if (HE3D_ABS(area) < 0.0001f) return;
    float invA = 1.0f / area;

    float dw0 = (y1 - y2) * invA;
    float dw1 = (y2 - y0) * invA;
    float dw2 = -dw0 - dw1;
    float iz0 = 1.0f / vv[0].z, iz1 = 1.0f / vv[1].z, iz2 = 1.0f / vv[2].z;
    float2 uz0 = uvs[0] * iz0, uz1 = uvs[1] * iz1, uz2 = uvs[2] * iz2;
    float dizDx = dw0 * iz0 + dw1 * iz1 + dw2 * iz2;
    float duzDx = dw0 * uz0.x + dw1 * uz1.x + dw2 * uz2.x;
    float dvzDx = dw0 * uz0.y + dw1 * uz1.y + dw2 * uz2.y;

    color3 lc = mainLight.color * intens;
    int lr = HE3D_CLAMP((int)(lc.r * 256.0f), 0, 512);
    int lg = HE3D_CLAMP((int)(lc.g * 256.0f), 0, 512);
    int lb = HE3D_CLAMP((int)(lc.b * 256.0f), 0, 512);
    ColorA* tp  = tex.pixels;
    int     tw  = tex.width;
    int     th  = tex.height;
    int     twm1 = tw - 1;
    int     thm1 = th - 1;
    int     stride = m_width;

    float  *db   = m_depthBuf;
    ColorA *cbuf = m_colorBuf;
    bool msaa = IsMsaaEnabled();

    for (int y = mnY; y <= mxY; y++) {
        float py = (float)y + 0.5f, px0 = (float)mnX + 0.5f;
        float w0 = ((x1 - px0) * (y2 - py) - (y1 - py) * (x2 - px0)) * invA;
        float w1 = ((x2 - px0) * (y0 - py) - (y2 - py) * (x0 - px0)) * invA;
        float w2 = 1.0f - w0 - w1;
        float ciz = w0 * iz0 + w1 * iz1 + w2 * iz2;
        float cuz = w0 * uz0.x + w1 * uz1.x + w2 * uz2.x;
        float cvz = w0 * uz0.y + w1 * uz1.y + w2 * uz2.y;

        int rb = y * stride;
        for (int x = mnX; x <= mxX; x++) {
            int idx = rb + x;
            bool centerInside = w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f;
            int coverage = centerInside ? 4 : 0;
            if (msaa)
            {
                coverage = TriangleCoverage4(x, y, x0, y0, x1, y1, x2, y2, invA);
            }
            if (coverage > 0 && ciz > db[idx]) {
                db[idx] = ciz;
                float invCiz = 1.0f / ciz;
                float u = cuz * invCiz;
                float v = cvz * invCiz;
                if (u < 0.0f || u >= 1.0f) u = fracf(u);
                if (v < 0.0f || v >= 1.0f) v = fracf(v);
                int tx = (int)(u * tw);
                int ty = (int)(v * th);
                tx = HE3D_CLAMP(tx, 0, twm1);
                ty = HE3D_CLAMP(ty, 0, thm1);
                ColorA texel = tp[ty * tw + tx];
                ColorA shaded;
                shaded.r = (uint8_t)HE3D_CLAMP(((int)texel.r * lr) >> 8, 0, 255);
                shaded.g = (uint8_t)HE3D_CLAMP(((int)texel.g * lg) >> 8, 0, 255);
                shaded.b = (uint8_t)HE3D_CLAMP(((int)texel.b * lb) >> 8, 0, 255);
                shaded.a = 255;
                cbuf[idx] = msaa ? BlendCoverage(cbuf[idx], shaded, coverage) : shaded;
            }
            w0 += dw0;
            w1 += dw1;
            w2 += dw2;
            ciz += dizDx;
            cuz += duzDx;
            cvz += dvzDx;
        }
    }
}

// ============================================================================
// [14] Present / 提交显示
// ============================================================================
static int he3d_luma(const ColorA& c)
{
    return ((int)c.r * 77 + (int)c.g * 150 + (int)c.b * 29) >> 8;
}

static uint8_t he3d_blend_u8(uint8_t a, uint8_t b)
{
    return (uint8_t)(((int)a * 141 + (int)b * 115) >> 8);
}

bool Renderer::ApplyFxaa()
{
    if (m_width <= 2 || m_height <= 2 || !m_colorBuf)
    {
        return false;
    }

    int total = m_width * m_height;
    if (!m_fxaaBuf)
    {
        m_fxaaBuf = new ColorA[total];
        if (!m_fxaaBuf)
        {
            return false;
        }
    }

    for (int x = 0; x < m_width; x++)
    {
        m_fxaaBuf[x] = m_colorBuf[x];
        m_fxaaBuf[(m_height - 1) * m_width + x] = m_colorBuf[(m_height - 1) * m_width + x];
    }
    for (int y = 1; y < m_height - 1; y++)
    {
        m_fxaaBuf[y * m_width] = m_colorBuf[y * m_width];
        m_fxaaBuf[y * m_width + m_width - 1] = m_colorBuf[y * m_width + m_width - 1];
    }

    const int edgeThreshold = 20;
    const int edgeThresholdMin = 8;

    for (int y = 1; y < m_height - 1; y++)
    {
        int row = y * m_width;
        for (int x = 1; x < m_width - 1; x++)
        {
            int idx = row + x;
            const ColorA& center = m_colorBuf[idx];
            int lM = he3d_luma(center);
            int lN = he3d_luma(m_colorBuf[idx - m_width]);
            int lS = he3d_luma(m_colorBuf[idx + m_width]);
            int lW = he3d_luma(m_colorBuf[idx - 1]);
            int lE = he3d_luma(m_colorBuf[idx + 1]);

            int lMin = HE3D_MIN(lM, HE3D_MIN(HE3D_MIN(lN, lS), HE3D_MIN(lW, lE)));
            int lMax = HE3D_MAX(lM, HE3D_MAX(HE3D_MAX(lN, lS), HE3D_MAX(lW, lE)));
            int contrast = lMax - lMin;
            if (contrast < HE3D_MAX(edgeThresholdMin, (lMax * 20) >> 8) || contrast < edgeThreshold)
            {
                m_fxaaBuf[idx] = center;
                continue;
            }

            int horizontal = HE3D_ABS(lN + lS - 2 * lM);
            int vertical = HE3D_ABS(lW + lE - 2 * lM);
            const ColorA& a = horizontal >= vertical ? m_colorBuf[idx - 1] : m_colorBuf[idx - m_width];
            const ColorA& b = horizontal >= vertical ? m_colorBuf[idx + 1] : m_colorBuf[idx + m_width];

            ColorA out;
            out.r = he3d_blend_u8(center.r, (uint8_t)(((int)a.r + (int)b.r) >> 1));
            out.g = he3d_blend_u8(center.g, (uint8_t)(((int)a.g + (int)b.g) >> 1));
            out.b = he3d_blend_u8(center.b, (uint8_t)(((int)a.b + (int)b.b) >> 1));
            out.a = center.a;
            m_fxaaBuf[idx] = out;
        }
    }

    return true;
}

bool Renderer::ApplyTaa(const ColorA *source)
{
    if (m_width <= 0 || m_height <= 0 || !source || !m_depthBuf)
    {
        return false;
    }

    int total = m_width * m_height;
    if (!m_taaBuf)
    {
        m_taaBuf = new ColorA[total];
        if (!m_taaBuf)
        {
            return false;
        }
    }
    if (!m_taaHistory)
    {
        m_taaHistory = new ColorA[total];
        if (!m_taaHistory)
        {
            return false;
        }
    }
    if (!m_taaDepth)
    {
        m_taaDepth = new float[total];
        if (!m_taaDepth)
        {
            return false;
        }
    }

    if (!m_taaValid)
    {
        for (int i = 0; i < total; i++)
        {
            m_taaBuf[i] = source[i];
            m_taaHistory[i] = source[i];
            m_taaDepth[i] = m_depthBuf[i];
        }
        m_taaValid = true;
        m_taaFrameIndex++;
        return true;
    }

    for (int y = 0; y < m_height; y++)
    {
        for (int x = 0; x < m_width; x++)
        {
            int idx = y * m_width + x;
            const ColorA& cur = source[idx];
            const ColorA& hist = m_taaHistory[idx];

            int minR = 255, minG = 255, minB = 255;
            int maxR = 0, maxG = 0, maxB = 0;
            for (int oy = -1; oy <= 1; oy++)
            {
                int sy = HE3D_CLAMP(y + oy, 0, m_height - 1);
                for (int ox = -1; ox <= 1; ox++)
                {
                    int sx = HE3D_CLAMP(x + ox, 0, m_width - 1);
                    const ColorA& c = source[sy * m_width + sx];
                    minR = HE3D_MIN(minR, (int)c.r);
                    minG = HE3D_MIN(minG, (int)c.g);
                    minB = HE3D_MIN(minB, (int)c.b);
                    maxR = HE3D_MAX(maxR, (int)c.r);
                    maxG = HE3D_MAX(maxG, (int)c.g);
                    maxB = HE3D_MAX(maxB, (int)c.b);
                }
            }

            int histR = HE3D_CLAMP((int)hist.r, minR, maxR);
            int histG = HE3D_CLAMP((int)hist.g, minG, maxG);
            int histB = HE3D_CLAMP((int)hist.b, minB, maxB);
            int colorDelta = HE3D_ABS((int)cur.r - histR) +
                             HE3D_ABS((int)cur.g - histG) +
                             HE3D_ABS((int)cur.b - histB);
            float depthDelta = HE3D_ABS(m_depthBuf[idx] - m_taaDepth[idx]);
            bool rejectHistory = (m_depthBuf[idx] > 0.0f && depthDelta > HE3D_MAX(0.0025f, m_depthBuf[idx] * 0.05f)) ||
                                 colorDelta > 180;

            ColorA out;
            if (rejectHistory)
            {
                out = cur;
            }
            else
            {
                out.r = (uint8_t)(((int)cur.r * 192 + histR * 64) >> 8);
                out.g = (uint8_t)(((int)cur.g * 192 + histG * 64) >> 8);
                out.b = (uint8_t)(((int)cur.b * 192 + histB * 64) >> 8);
                out.a = cur.a;
            }
            m_taaBuf[idx] = out;
        }
    }

    for (int i = 0; i < total; i++)
    {
        m_taaHistory[i] = m_taaBuf[i];
        m_taaDepth[i] = m_depthBuf[i];
    }
    m_taaFrameIndex++;
    return true;
}

const ColorA *Renderer::ApplySsaa(const ColorA *source)
{
    if (m_ssaaScale <= 1 || !source || m_outputWidth <= 0 || m_outputHeight <= 0)
    {
        return source;
    }

    int outTotal = m_outputWidth * m_outputHeight;
    if (!m_ssaaBuf)
    {
        m_ssaaBuf = new ColorA[outTotal];
        if (!m_ssaaBuf)
        {
            return source;
        }
    }

    int scale = (int)m_ssaaScale;
    int sampleCount = scale * scale;
    for (int y = 0; y < m_outputHeight; y++)
    {
        for (int x = 0; x < m_outputWidth; x++)
        {
            int sumR = 0;
            int sumG = 0;
            int sumB = 0;
            int sumA = 0;
            int baseY = y * scale;
            int baseX = x * scale;
            for (int sy = 0; sy < scale; sy++)
            {
                int row = (baseY + sy) * m_width;
                for (int sx = 0; sx < scale; sx++)
                {
                    const ColorA& c = source[row + baseX + sx];
                    sumR += c.r;
                    sumG += c.g;
                    sumB += c.b;
                    sumA += c.a;
                }
            }
            ColorA out;
            out.r = (uint8_t)(sumR / sampleCount);
            out.g = (uint8_t)(sumG / sampleCount);
            out.b = (uint8_t)(sumB / sampleCount);
            out.a = (uint8_t)(sumA / sampleCount);
            m_ssaaBuf[y * m_outputWidth + x] = out;
        }
    }

    return m_ssaaBuf;
}

void Renderer::Present() {
    if (m_width <= 0 || m_height <= 0 || !m_colorBuf)
    {
        return;
    }

    const ColorA *pixels = m_colorBuf;
    if (IsFxaaEnabled() && ApplyFxaa())
    {
        pixels = m_fxaaBuf;
    }
    if (IsTaaEnabled() && ApplyTaa(pixels))
    {
        pixels = m_taaBuf;
    }
    else if (!IsTaaEnabled())
    {
        m_taaValid = false;
        m_taaFrameIndex = 0;
    }
    pixels = ApplySsaa(pixels);
    int32_t presentWidth = (m_ssaaScale > 1) ? m_outputWidth : m_width;
    int32_t presentHeight = (m_ssaaScale > 1) ? m_outputHeight : m_height;
    HE3D::Present(m_window, presentWidth, presentHeight, pixels);
}

}
