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

static void *he3d_alloc_or_trap(__SIZE_TYPE__ size)
{
   void *ptr = HE3D::Alloc((HE3D::uint64_t)size);
   if (!ptr) {
      __builtin_trap();
   }
   return ptr;
}

static void *he3d_stbi_memcpy(void *dst, const void *src, size_t count)
{
   unsigned char       *d = (unsigned char *)dst;
   const unsigned char *s = (const unsigned char *)src;
   for (size_t i = 0; i < count; i++) {
      d[i] = s[i];
   }
   return dst;
}

static void *he3d_stbi_memset(void *dst, int value, size_t count)
{
   unsigned char *d = (unsigned char *)dst;
   for (size_t i = 0; i < count; i++) {
      d[i] = (unsigned char)value;
   }
   return dst;
}

static void *he3d_stbi_malloc(size_t size) { return HE3D::Alloc((HE3D::uint64_t)size); }

static void he3d_stbi_free(void *ptr) { HE3D::Free(ptr); }

static void *he3d_stbi_realloc_sized(void *ptr, size_t oldSize, size_t newSize)
{
   void *next = HE3D::Alloc((HE3D::uint64_t)newSize);
   if (!next) {
      return nullptr;
   }
   if (ptr) {
      size_t copySize = oldSize < newSize ? oldSize : newSize;
      he3d_stbi_memcpy(next, ptr, copySize);
      HE3D::Free(ptr);
   }
   return next;
}

static double he3d_stbi_fabs(double value) { return value < 0.0 ? -value : value; }

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
#define STBI_REALLOC_SIZED(p, oldsz, newsz) he3d_stbi_realloc_sized(p, oldsz, newsz)
#define fabs he3d_stbi_fabs
#define memcpy he3d_stbi_memcpy
#define memset he3d_stbi_memset
#include "stbi.h"
#undef memcpy
#undef memset
#undef fabs

namespace HE3D
{

static const Platform *g_platform       = nullptr;
static uint32_t        g_frameRateLimit = 0;
static bool            g_fxaaEnabled    = false;
static bool            g_taaEnabled     = false;
static bool            g_msaaEnabled    = false;
static uint32_t        g_ssaaScale      = 1;

const Platform *GetPlatform()
{
   if (!g_platform) {
      g_platform = GetBuiltinPlatform();
   }
   return g_platform;
}

void SetPlatform(const Platform *platform) { g_platform = platform; }

void *Alloc(uint64_t size) { return GetPlatform()->alloc(size); }

void Free(void *ptr) { GetPlatform()->free(ptr); }

void SetFrameRateLimit(uint32_t fps) { g_frameRateLimit = fps; }

uint32_t GetFrameRateLimit() { return g_frameRateLimit; }

void SetFxaaEnabled(bool enabled) { g_fxaaEnabled = enabled; }

bool IsFxaaEnabled() { return g_fxaaEnabled; }

void SetTaaEnabled(bool enabled) { g_taaEnabled = enabled; }

bool IsTaaEnabled() { return g_taaEnabled; }

void SetMsaaEnabled(bool enabled) { g_msaaEnabled = enabled; }

bool IsMsaaEnabled() { return g_msaaEnabled; }

void SetSsaaScale(uint32_t scale)
{
   if (scale < 1) {
      scale = 1;
   }
   if (scale > 4) {
      scale = 4;
   }
   g_ssaaScale = scale;
}

uint32_t GetSsaaScale() { return g_ssaaScale; }

void PaceFrame(double frameStart)
{
   if (g_frameRateLimit == 0) {
      return;
   }

   double targetFrameSeconds = 1.0 / (double)g_frameRateLimit;
   double elapsed            = TimeSeconds() - frameStart;
   if (elapsed < targetFrameSeconds) {
      uint64_t sleepMs = (uint64_t)((targetFrameSeconds - elapsed) * 1000.0);
      if (sleepMs > 0) {
         SleepMilliseconds(sleepMs);
      }
   }
}

} // namespace HE3D

// Route C++ allocation through HE3D's platform allocator.
// 将 C++ 分配转发到 HE3D 当前平台分配器。
void *operator new(__SIZE_TYPE__ size) { return he3d_alloc_or_trap(size); }

void *operator new[](__SIZE_TYPE__ size) { return he3d_alloc_or_trap(size); }

void operator delete(void *ptr) noexcept { HE3D::Free(ptr); }

void operator delete[](void *ptr) noexcept { HE3D::Free(ptr); }

void operator delete(void *ptr, __SIZE_TYPE__) noexcept { HE3D::Free(ptr); }

void operator delete[](void *ptr, __SIZE_TYPE__) noexcept { HE3D::Free(ptr); }

namespace HE3D
{

// ============================================================================
// [0] Float parser for builds without strtod/strtof.
// [0] 用于没有 strtod/strtof 环境的浮点解析器。
// ============================================================================
static const char *ParseFloat(const char *s, float *out)
{
   while (*s == ' ' || *s == '\t') s++;
   float sign = 1.0f;
   if (*s == '-') {
      sign = -1.0f;
      s++;
   } else if (*s == '+') {
      s++;
   }
   float ip = 0.0f;
   while (*s >= '0' && *s <= '9') {
      ip = ip * 10.0f + (float)(*s - '0');
      s++;
   }
   float fp = 0.0f;
   if (*s == '.') {
      s++;
      float mul = 0.1f;
      while (*s >= '0' && *s <= '9') {
         fp += (float)(*s - '0') * mul;
         mul *= 0.1f;
         s++;
      }
   }
   float v = sign * (ip + fp);
   if (*s == 'e' || *s == 'E') {
      s++;
      float es = 1.0f;
      if (*s == '-') {
         es = -1.0f;
         s++;
      } else if (*s == '+') {
         s++;
      }
      int ev = 0;
      while (*s >= '0' && *s <= '9') {
         ev = ev * 10 + (*s - '0');
         s++;
      }
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
   const char *ln = buf;
   while (ln < end && *ln != '\n' && *ln != '\r' && (ln - buf) < maxLen - 1) ln++;
   int len = (int)(ln - buf);
   if (len > maxLen - 1) len = maxLen - 1;
   for (int i = 0; i < len; i++) out[i] = buf[i];
   out[len]       = 0;
   const char *nx = ln;
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

struct Mat3 {
   float m00, m01, m02;
   float m10, m11, m12;
   float m20, m21, m22;

   static Mat3 FromQuat(const quat &q)
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
      r.m00 = m00;
      r.m01 = m10;
      r.m02 = m20;
      r.m10 = m01;
      r.m11 = m11;
      r.m12 = m21;
      r.m20 = m02;
      r.m21 = m12;
      r.m22 = m22;
      return r;
   }

   float3 Mul(const float3 &v) const
   {
      return {m00 * v.x + m01 * v.y + m02 * v.z, m10 * v.x + m11 * v.y + m12 * v.z,
              m20 * v.x + m21 * v.y + m22 * v.z};
   }
};

static bool QuatIsIdentity(const quat &q)
{
   return HE3D_ABS(q.x) < 0.000001f && HE3D_ABS(q.y) < 0.000001f && HE3D_ABS(q.z) < 0.000001f &&
          HE3D_ABS(q.w - 1.0f) < 0.000001f;
}

static const float HE3D_NEAR_Z = 0.1f;

struct ClipVertex {
   float3 view;
   float2 uv;
};

static ClipVertex LerpClipVertex(const ClipVertex &a, const ClipVertex &b, float t)
{
   ClipVertex out;
   out.view = a.view + (b.view - a.view) * t;
   out.uv   = a.uv + (b.uv - a.uv) * t;
   return out;
}

static int ClipTriangleToNearPlane(const ClipVertex *input, ClipVertex *output)
{
   ClipVertex temp[4];
   int        count = 0;

   for (int i = 0; i < 3; i++) {
      const ClipVertex &a       = input[i];
      const ClipVertex &b       = input[(i + 1) % 3];
      bool              aInside = a.view.z > HE3D_NEAR_Z;
      bool              bInside = b.view.z > HE3D_NEAR_Z;

      if (aInside && bInside) {
         if (count < 4) temp[count++] = b;
      } else if (aInside && !bInside) {
         float t = (HE3D_NEAR_Z - a.view.z) / (b.view.z - a.view.z);
         if (count < 4) temp[count++] = LerpClipVertex(a, b, t);
      } else if (!aInside && bInside) {
         float t = (HE3D_NEAR_Z - a.view.z) / (b.view.z - a.view.z);
         if (count < 4) temp[count++] = LerpClipVertex(a, b, t);
         if (count < 4) temp[count++] = b;
      }
   }

   for (int i = 0; i < count; i++) {
      output[i] = temp[i];
   }
   return count;
}

static void ProjectViewTriangle(const float3 *vv, float2 *ps, float halfW, float halfH,
                                float scaleX, float scaleY, float jitterX, float jitterY)
{
   for (int i = 0; i < 3; i++) {
      float invZ = 1.0f / vv[i].z;
      ps[i]      = {halfW + vv[i].x * invZ * scaleX + jitterX,
                    halfH - vv[i].y * invZ * scaleY + jitterY};
   }
}

static float ScreenTriangleArea(const float2 *ps)
{
   return (ps[1].x - ps[0].x) * (ps[2].y - ps[0].y) - (ps[1].y - ps[0].y) * (ps[2].x - ps[0].x);
}

static bool TriangleOutsideViewport(const float2 *ps, int width, int height)
{
   if (ps[0].x < 0.0f && ps[1].x < 0.0f && ps[2].x < 0.0f) return true;
   if (ps[0].x >= (float)width && ps[1].x >= (float)width && ps[2].x >= (float)width) return true;
   if (ps[0].y < 0.0f && ps[1].y < 0.0f && ps[2].y < 0.0f) return true;
   if (ps[0].y >= (float)height && ps[1].y >= (float)height && ps[2].y >= (float)height)
      return true;
   return false;
}

RayHit RaycastMeshTriangles(const Ray &ray, const GameObject &object)
{
   RayHit      result;
   const Mesh &mesh = object.GetMesh();
   if (!mesh.IsValid() || mesh.GetVertexCount() < 3) {
      return result;
   }

   bool   found = false;
   RayHit bestHit;
   quat   objRot         = object.orientation;
   bool   identityObjRot = QuatIsIdentity(objRot);
   Mat3   rot            = Mat3::FromQuat(objRot);

   const float3 *vertices = mesh.GetVertices();
   for (int i = 0; i < mesh.GetVertexCount(); i += 3) {
      float3 v0 = vertices[i];
      float3 v1 = vertices[i + 1];
      float3 v2 = vertices[i + 2];
      if (!identityObjRot) {
         v0 = rot.Mul(v0) + object.position;
         v1 = rot.Mul(v1) + object.position;
         v2 = rot.Mul(v2) + object.position;
      } else {
         v0 = v0 + object.position;
         v1 = v1 + object.position;
         v2 = v2 + object.position;
      }

      RayHit hit = ray.CastTriangle(v0, v1, v2);
      if (!hit.hit) {
         continue;
      }
      if (!found || hit.distance < bestHit.distance) {
         bestHit               = hit;
         bestHit.triangleIndex = i / 3;
         found                 = true;
      }
   }

   return found ? bestHit : result;
}

GameObject::GameObject(Mesh &mesh) : position{0, 0, 0}, orientation{1, 0, 0, 0}, m_mesh(&mesh) {}

Mesh &GameObject::GetMesh() { return *m_mesh; }

const Mesh &GameObject::GetMesh() const { return *m_mesh; }

void GameObject::SetMesh(Mesh &mesh) { m_mesh = &mesh; }

// ============================================================================
// [5] Mesh allocation helpers.
// [5] Mesh 分配辅助函数。
// ============================================================================
Mesh::Mesh()
    : m_vertices(nullptr), m_uvs(nullptr), m_triangleNormals(nullptr), m_vertexCount(0),
      m_capacity(0)
{
}

Mesh::~Mesh() { Reset(); }

Mesh::Mesh(Mesh &&other)
    : m_vertices(other.m_vertices), m_uvs(other.m_uvs), m_triangleNormals(other.m_triangleNormals),
      m_vertexCount(other.m_vertexCount), m_capacity(other.m_capacity)
{
   other.m_vertices        = nullptr;
   other.m_uvs             = nullptr;
   other.m_triangleNormals = nullptr;
   other.m_vertexCount     = 0;
   other.m_capacity        = 0;
}

Mesh &Mesh::operator=(Mesh &&other)
{
   if (this != &other) {
      Reset();
      m_vertices              = other.m_vertices;
      m_uvs                   = other.m_uvs;
      m_triangleNormals       = other.m_triangleNormals;
      m_vertexCount           = other.m_vertexCount;
      m_capacity              = other.m_capacity;
      other.m_vertices        = nullptr;
      other.m_uvs             = nullptr;
      other.m_triangleNormals = nullptr;
      other.m_vertexCount     = 0;
      other.m_capacity        = 0;
   }
   return *this;
}

bool Mesh::IsValid() const { return m_vertices && m_uvs && m_triangleNormals && m_vertexCount > 0; }
int32_t       Mesh::GetVertexCount() const { return m_vertexCount; }
const float3 *Mesh::GetVertices() const { return m_vertices; }
const float2 *Mesh::GetUVs() const { return m_uvs; }
const float3 *Mesh::GetTriangleNormals() const { return m_triangleNormals; }

void Mesh::Reset()
{
   delete[] m_vertices;
   delete[] m_uvs;
   delete[] m_triangleNormals;
   m_vertices        = nullptr;
   m_uvs             = nullptr;
   m_triangleNormals = nullptr;
   m_vertexCount     = 0;
   m_capacity        = 0;
}

bool Mesh::Init(int32_t vertexCount)
{
   delete[] m_vertices;
   delete[] m_uvs;
   delete[] m_triangleNormals;
   m_vertices        = nullptr;
   m_uvs             = nullptr;
   m_triangleNormals = nullptr;
   m_vertexCount     = 0;
   m_capacity        = 0;

   if (vertexCount <= 0) {
      return false;
   }

   m_vertices        = new float3[vertexCount];
   m_uvs             = new float2[vertexCount];
   m_triangleNormals = new float3[(vertexCount + 2) / 3];
   if (!m_vertices || !m_uvs || !m_triangleNormals) {
      delete[] m_vertices;
      delete[] m_uvs;
      delete[] m_triangleNormals;
      m_vertices        = nullptr;
      m_uvs             = nullptr;
      m_triangleNormals = nullptr;
      return false;
   }

   m_vertexCount = vertexCount;
   m_capacity    = vertexCount;
   return true;
}

Mesh Mesh::Create(int32_t vertexCount)
{
   Mesh mesh;
   if (!mesh.Init(vertexCount)) {
      return Mesh();
   }
   return mesh;
}

bool Mesh::Init(const float3 *srcVertices, const float2 *srcUvs, int32_t vertexCount)
{
   if (!srcVertices || !Init(vertexCount)) {
      return false;
   }

   for (int i = 0; i < vertexCount; i++) {
      m_vertices[i] = srcVertices[i];
      m_uvs[i]      = srcUvs ? srcUvs[i] : float2(0.0f, 0.0f);
   }
   RecalculateTriangleNormals();
   return true;
}

void Mesh::RecalculateTriangleNormals()
{
   if (!m_vertices || !m_triangleNormals) {
      return;
   }

   int triCount = m_vertexCount / 3;
   for (int i = 0; i < triCount; i++) {
      int    v             = i * 3;
      float3 e1            = m_vertices[v + 1] - m_vertices[v];
      float3 e2            = m_vertices[v + 2] - m_vertices[v];
      m_triangleNormals[i] = float3::cross(e1, e2).normalizeFast();
   }
}

Mesh Mesh::Create(const float3 *srcVertices, int32_t vertexCount)
{
   return Mesh::Create(srcVertices, nullptr, vertexCount);
}

Mesh Mesh::Create(const float3 *srcVertices, const float2 *srcUvs, int32_t vertexCount)
{
   Mesh mesh;
   if (!mesh.Init(srcVertices, srcUvs, vertexCount)) {
      return Mesh();
   }
   return mesh;
}

static void WriteMeshTri(float3 *vertices, float2 *uvs, int32_t *index, float3 a, float3 b,
                         float3 c, float2 uva, float2 uvb, float2 uvc)
{
   int32_t i       = *index;
   vertices[i + 0] = a;
   vertices[i + 1] = b;
   vertices[i + 2] = c;
   uvs[i + 0]      = uva;
   uvs[i + 1]      = uvb;
   uvs[i + 2]      = uvc;
   *index          = i + 3;
}

Mesh Mesh::CreateTriangle(float width, float height)
{
   if (width <= 0.0f || height <= 0.0f) {
      return Mesh();
   }

   float  hx          = width * 0.5f;
   float  hy          = height * 0.5f;
   float3 vertices[3] = {{-hx, -hy, 0.0f}, {0.0f, hy, 0.0f}, {hx, -hy, 0.0f}};
   float2 uvs[3]      = {{0.0f, 0.0f}, {0.5f, 1.0f}, {1.0f, 0.0f}};
   return Mesh::Create(vertices, uvs, 3);
}

Mesh Mesh::CreatePlane(float width, float depth)
{
   if (width <= 0.0f || depth <= 0.0f) {
      return Mesh();
   }

   float  hx          = width * 0.5f;
   float  hz          = depth * 0.5f;
   float3 vertices[6] = {{-hx, 0.0f, -hz}, {hx, 0.0f, hz},  {hx, 0.0f, -hz},
                         {-hx, 0.0f, -hz}, {-hx, 0.0f, hz}, {hx, 0.0f, hz}};
   float2 uvs[6]      = {{0.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 0.0f},
                         {0.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f}};
   return Mesh::Create(vertices, uvs, 6);
}

Mesh Mesh::CreateCube(float width, float height, float depth)
{
   if (width <= 0.0f || height <= 0.0f || depth <= 0.0f) {
      return Mesh();
   }

   float  hx   = width * 0.5f;
   float  hy   = height * 0.5f;
   float  hz   = depth * 0.5f;
   float3 p[8] = {{-hx, -hy, -hz}, {hx, -hy, -hz}, {hx, hy, -hz}, {-hx, hy, -hz},
                  {-hx, -hy, hz},  {hx, -hy, hz},  {hx, hy, hz},  {-hx, hy, hz}};

   float3  vertices[36];
   float2  uvs[36];
   int32_t i   = 0;
   float2  uv0 = {0.0f, 0.0f};
   float2  uv1 = {1.0f, 0.0f};
   float2  uv2 = {1.0f, 1.0f};
   float2  uv3 = {0.0f, 1.0f};

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

Mesh Mesh::CreateSphere(float radius, int32_t segments, int32_t rings)
{
   if (radius <= 0.0f) {
      return Mesh();
   }
   if (segments < 3) segments = 3;
   if (rings < 2) rings = 2;

   int32_t vertexCount = segments * rings * 6;
   Mesh    mesh        = Mesh::Create(vertexCount);
   if (!mesh.IsValid()) {
      return Mesh();
   }

   int32_t i = 0;
   for (int32_t ring = 0; ring < rings; ring++) {
      float v0   = (float)ring / (float)rings;
      float v1   = (float)(ring + 1) / (float)rings;
      float lat0 = (v0 - 0.5f) * HE3D_PI;
      float lat1 = (v1 - 0.5f) * HE3D_PI;
      float y0   = sinf(lat0) * radius;
      float y1   = sinf(lat1) * radius;
      float r0   = cosf(lat0) * radius;
      float r1   = cosf(lat1) * radius;

      for (int32_t seg = 0; seg < segments; seg++) {
         float  u0   = (float)seg / (float)segments;
         float  u1   = (float)(seg + 1) / (float)segments;
         float  lon0 = u0 * HE3D_PI * 2.0f;
         float  lon1 = u1 * HE3D_PI * 2.0f;
         float3 p00  = {sinf(lon0) * r0, y0, cosf(lon0) * r0};
         float3 p01  = {sinf(lon1) * r0, y0, cosf(lon1) * r0};
         float3 p10  = {sinf(lon0) * r1, y1, cosf(lon0) * r1};
         float3 p11  = {sinf(lon1) * r1, y1, cosf(lon1) * r1};

         WriteMeshTri(mesh.m_vertices, mesh.m_uvs, &i, p00, p11, p10, {u0, v0}, {u1, v1}, {u0, v1});
         WriteMeshTri(mesh.m_vertices, mesh.m_uvs, &i, p00, p01, p11, {u0, v0}, {u1, v0}, {u1, v1});
      }
   }

   mesh.RecalculateTriangleNormals();
   return mesh;
}

// ============================================================================
// [6] Mesh::LoadOBJ: two passes over in-memory file data.
// [6] Mesh::LoadOBJ：对内存文件数据执行两次扫描。
// ============================================================================
Mesh Mesh::LoadOBJ(const char *filename)
{
   FileData file = {};
   if (!LoadFile(filename, &file) || !file.data || file.length < 4) {
      if (file.data) {
         CloseFile(&file);
      }
      return Mesh();
   }

   const char *buf = (const char *)file.data;
   const char *end = buf + file.length;

   // Pass 1 counts vertices, UVs, and output triangles.
   // 第一遍统计顶点、UV 和输出三角形数量。
   int         vCount = 0, vtCount = 0, triCount = 0;
   const char *c = buf;
   char        line[512];
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
   if (vCount == 0 || triCount == 0) {
      CloseFile(&file);
      return Mesh();
   }

   // Allocate temporary OBJ buffers after the first pass establishes exact sizes.
   // 第一遍得到准确数量后，再分配 OBJ 临时缓冲区。
   float3 *all_v  = new float3[vCount];
   float2 *all_vt = (vtCount > 0) ? new float2[vtCount] : nullptr;
   if (!all_v || (vtCount > 0 && !all_vt)) {
      delete[] all_v;
      delete[] all_vt;
      CloseFile(&file);
      return Mesh();
   }
   int vc = 0, vtc = 0;

   int  totalVerts = triCount * 3;
   Mesh mesh       = Mesh::Create(totalVerts);
   if (!mesh.IsValid()) {
      delete[] all_v;
      delete[] all_vt;
      CloseFile(&file);
      return Mesh();
   }
   mesh.m_vertexCount = 0; // incremented while faces are emitted / 输出 face 时递增

   // Pass 2 writes triangulated vertices and UVs.
   // 第二遍写入三角化后的顶点和 UV。
   c = buf;
   while ((c = MemGetLine(c, end, line, sizeof(line))) != nullptr) {
      char *p = line;
      while (*p == ' ' || *p == '\t') p++;

      if (p[0] == 'v' && p[1] == ' ') {
         float3      vtx;
         const char *py = ParseFloat(p + 2, &vtx.x);
         const char *pz = ParseFloat(py, &vtx.y);
         ParseFloat(pz, &vtx.z);
         if (vc < vCount) all_v[vc++] = vtx;
      } else if (p[0] == 'v' && p[1] == 't' && (p[2] == ' ' || p[2] == '\t') && all_vt) {
         float2      uv;
         const char *pv = ParseFloat(p + 3, &uv.x);
         float       rawV;
         ParseFloat(pv, &rawV);
         uv.y = 1.0f - rawV;
         if (vtc < vtCount) all_vt[vtc++] = uv;
      } else if (p[0] == 'f' && p[1] == ' ') {
         struct FV {
            int v, vt;
         };
         FV    fv[32];
         int   fc              = 0;
         char *tok             = p + 2;
         bool  validFaceSyntax = true;
         while (*tok) {
            while (*tok == ' ' || *tok == '\t') tok++;
            if (*tok == 0 || *tok == '\n' || *tok == '\r') break;
            if (*tok == '-') {
               validFaceSyntax = false;
               break;
            }
            if (fc >= 32) {
               validFaceSyntax = false;
               break;
            }
            int  vi = 0, ti = 0;
            bool hasVertexIndex = false;
            while (*tok >= '0' && *tok <= '9') {
               vi = vi * 10 + (*tok - '0');
               tok++;
            }
            hasVertexIndex = vi > 0;
            if (*tok == '/') {
               tok++;
               if (*tok != '/') {
                  ti = 0;
                  while (*tok >= '0' && *tok <= '9') {
                     ti = ti * 10 + (*tok - '0');
                     tok++;
                  }
               }
               if (*tok == '/') {
                  tok++;
                  while (*tok >= '0' && *tok <= '9') tok++;
               }
            }
            while (*tok && *tok != ' ' && *tok != '\t' && *tok != '\n' && *tok != '\r') tok++;
            if (!hasVertexIndex) {
               validFaceSyntax = false;
               break;
            }
            fv[fc].v  = vi - 1;
            fv[fc].vt = ti - 1;
            fc++;
         }

         bool validFace = validFaceSyntax && (fc >= 3);
         for (int i = 0; i < fc && validFace; i++) {
            if (fv[i].v < 0 || fv[i].v >= vc) {
               validFace = false;
            }
            if (fv[i].vt >= vtc) {
               validFace = false;
            }
         }
         if (!validFace) {
            continue;
         }

         for (int i = 1; i < fc - 1 && mesh.m_vertexCount + 3 <= mesh.m_capacity; i++) {
            int  idx    = mesh.m_vertexCount;
            bool hasUvs = all_vt && fv[0].vt >= 0 && fv[i].vt >= 0 && fv[i + 1].vt >= 0;

            mesh.m_vertices[idx]     = all_v[fv[0].v];
            mesh.m_vertices[idx + 1] = all_v[fv[i].v];
            mesh.m_vertices[idx + 2] = all_v[fv[i + 1].v];
            if (hasUvs) {
               mesh.m_uvs[idx]     = all_vt[fv[0].vt];
               mesh.m_uvs[idx + 1] = all_vt[fv[i].vt];
               mesh.m_uvs[idx + 2] = all_vt[fv[i + 1].vt];
            } else {
               mesh.m_uvs[idx] = mesh.m_uvs[idx + 1] = mesh.m_uvs[idx + 2] = {0, 0};
            }
            mesh.m_vertexCount += 3;
         }
      }
   }

   delete[] all_v;
   delete[] all_vt;
   if (mesh.m_vertexCount == 0) {
      CloseFile(&file);
      return Mesh();
   }
   mesh.RecalculateTriangleNormals();
   CloseFile(&file);
   return mesh;
}

// ============================================================================
// [7] Texture::Sample: nearest-neighbor sampling with UV wrap.
// [7] Texture::Sample：带 UV 环绕的最近邻采样。
// ============================================================================
color3 Texture::Sample(float u, float v) const
{
   if (!m_valid || !m_pixels || m_width <= 0 || m_height <= 0) return {1.0f, 0.0f, 1.0f};
   u               = fracf(u);
   v               = fracf(v);
   int           x = HE3D_CLAMP((int)(u * m_width), 0, m_width - 1);
   int           y = HE3D_CLAMP((int)(v * m_height), 0, m_height - 1);
   const ColorA &c = m_pixels[y * m_width + x];
   return {c.r * (1.0f / 255.0f), c.g * (1.0f / 255.0f), c.b * (1.0f / 255.0f)};
}

// ============================================================================
// [8] Texture loaders / 纹理加载
// ============================================================================
Texture::Texture() : m_width(0), m_height(0), m_pixels(nullptr), m_valid(false) {}
Texture::~Texture() { Reset(); }
Texture::Texture(Texture &&other)
    : m_width(other.m_width), m_height(other.m_height), m_pixels(other.m_pixels),
      m_valid(other.m_valid)
{
   other.m_width  = 0;
   other.m_height = 0;
   other.m_pixels = nullptr;
   other.m_valid  = false;
}

Texture &Texture::operator=(Texture &&other)
{
   if (this != &other) {
      Reset();
      m_width        = other.m_width;
      m_height       = other.m_height;
      m_pixels       = other.m_pixels;
      m_valid        = other.m_valid;
      other.m_width  = 0;
      other.m_height = 0;
      other.m_pixels = nullptr;
      other.m_valid  = false;
   }
   return *this;
}

void Texture::Reset()
{
   delete[] m_pixels;
   m_width  = 0;
   m_height = 0;
   m_pixels = nullptr;
   m_valid  = false;
}

int32_t Texture::GetWidth() const { return m_width; }
int32_t Texture::GetHeight() const { return m_height; }
bool    Texture::IsValid() const { return m_valid; }

Texture Texture::Create(const ColorA *pixels, int32_t width, int32_t height)
{
   const int32_t maxDimension = 8192;
   if (!pixels || width <= 0 || height <= 0 || width > maxDimension || height > maxDimension ||
       width > 0x7fffffff / height) {
      return Texture();
   }
   Texture texture;
   ColorA *copy = new ColorA[width * height];
   if (!copy) {
      return Texture();
   }
   for (int32_t index = 0; index < width * height; index++) {
      copy[index] = pixels[index];
   }
   texture.m_width  = width;
   texture.m_height = height;
   texture.m_pixels = copy;
   texture.m_valid  = true;
   return texture;
}

static Texture DecodeBmpTexture(const uint8_t *d, uint64_t length)
{
   if (!d || length < 54 || length > 0xffffffffU || d[0] != 'B' || d[1] != 'M') {
      return Texture();
   }
   unsigned int flen = (unsigned int)length;
   int          bw = 0, bh = 0, bpp = 0, compr = 0;
   unsigned int off = 0;

   if (d[0] != 'B' || d[1] != 'M') {
      return Texture();
   }
   off = d[10] | (d[11] << 8) | (d[12] << 16) | (d[13] << 24);
   if (off >= flen) {
      return Texture();
   }
   bw    = (int)(d[18] | (d[19] << 8) | (d[20] << 16) | (d[21] << 24));
   bh    = (int)(d[22] | (d[23] << 8) | (d[24] << 16) | (d[25] << 24));
   bpp   = (int)(d[28] | (d[29] << 8));
   compr = (int)(d[30] | (d[31] << 8) | (d[32] << 16) | (d[33] << 24));

   int texHeight = (bh < 0) ? -bh : bh;
   if (bw <= 0 || texHeight <= 0 || compr != 0) {
      return Texture();
   }

   int bytesPerPixel = 0;
   if (bpp == 24) {
      bytesPerPixel = 3;
   } else if (bpp == 32) {
      bytesPerPixel = 4;
   } else {
      return Texture();
   }

   const int MAX_BMP_DIMENSION = 8192;
   if (bw > MAX_BMP_DIMENSION || texHeight > MAX_BMP_DIMENSION) {
      return Texture();
   }

   unsigned int rowSize  = ((unsigned int)bw * (unsigned int)bytesPerPixel + 3U) & ~3U;
   unsigned int dataSize = flen - off;
   if (rowSize == 0 || dataSize / rowSize < (unsigned int)texHeight) {
      return Texture();
   }

   ColorA *pbuf = new ColorA[bw * texHeight];
   if (!pbuf) {
      return Texture();
   }

   {
      bool           topDown = (bh < 0);
      const uint8_t *src     = d + off;
      for (int y = 0; y < texHeight; y++) {
         int            sY  = topDown ? y : (texHeight - 1 - y);
         const uint8_t *row = src + sY * rowSize;
         ColorA        *dst = pbuf + y * bw;
         for (int x = 0; x < bw; x++) {
            dst[x] = {row[2], row[1], row[0], bytesPerPixel == 4 ? row[3] : (uint8_t)255};
            row += bytesPerPixel;
         }
      }
   }
   Texture texture = Texture::Create(pbuf, bw, texHeight);
   delete[] pbuf;
   return texture;
}

static Texture DecodeStbiTexture(const uint8_t *data, uint64_t length)
{
   if (!data || length > 0x7fffffffU) {
      return Texture();
   }
   int      width  = 0;
   int      height = 0;
   int      comp   = 0;
   stbi_uc *decoded =
       stbi_load_from_memory((const stbi_uc *)data, (int)length, &width, &height, &comp, 4);
   if (!decoded || width <= 0 || height <= 0) {
      if (decoded) {
         stbi_image_free(decoded);
      }
      return Texture();
   }

   const int MAX_IMAGE_DIMENSION = 8192;
   if (width > MAX_IMAGE_DIMENSION || height > MAX_IMAGE_DIMENSION) {
      stbi_image_free(decoded);
      return Texture();
   }

   ColorA *pixels = new ColorA[width * height];
   if (!pixels) {
      stbi_image_free(decoded);
      return Texture();
   }

   const stbi_uc *src = decoded;
   for (int i = 0; i < width * height; i++) {
      pixels[i].r = src[i * 4 + 0];
      pixels[i].g = src[i * 4 + 1];
      pixels[i].b = src[i * 4 + 2];
      pixels[i].a = src[i * 4 + 3];
   }
   stbi_image_free(decoded);

   Texture texture = Texture::Create(pixels, width, height);
   delete[] pixels;
   return texture;
}

Texture Texture::LoadImage(const char *filename)
{
   FileData file = {};
   if (!LoadFile(filename, &file) || !file.data || file.length < 3) {
      if (file.data) {
         CloseFile(&file);
      }
      return Texture();
   }

   const uint8_t *d     = file.data;
   bool           isBmp = file.length >= 2 && d[0] == 'B' && d[1] == 'M';
   bool    isPng = file.length >= 8 && d[0] == 0x89 && d[1] == 'P' && d[2] == 'N' && d[3] == 'G' &&
                   d[4] == 0x0D && d[5] == 0x0A && d[6] == 0x1A && d[7] == 0x0A;
   bool    isJpg = file.length >= 3 && d[0] == 0xFF && d[1] == 0xD8 && d[2] == 0xFF;
   Texture texture;
   if (isBmp) {
      texture = DecodeBmpTexture(d, file.length);
   } else if (isPng || isJpg) {
      texture = DecodeStbiTexture(d, file.length);
   }
   CloseFile(&file);
   return texture;
}

// ============================================================================
// [9] Renderer / 渲染器
// ============================================================================
Renderer::Renderer(Window *window, int32_t w, int32_t h)
    : m_width(w), m_height(h), m_outputWidth(w), m_outputHeight(h), m_ssaaScale(GetSsaaScale()),
      m_window(window)
{
   if (w <= 0 || h <= 0) {
      m_width         = 0;
      m_height        = 0;
      m_outputWidth   = 0;
      m_outputHeight  = 0;
      m_ssaaScale     = 1;
      m_colorBuf      = nullptr;
      m_fxaaBuf       = nullptr;
      m_taaBuf        = nullptr;
      m_taaHistory    = nullptr;
      m_taaDepth      = nullptr;
      m_ssaaBuf       = nullptr;
      m_msaaColorBuf  = nullptr;
      m_msaaDepthBuf  = nullptr;
      m_taaValid      = false;
      m_taaFrameIndex = 0;
      m_depthBuf      = nullptr;
      return;
   }

   if (m_ssaaScale < 1) m_ssaaScale = 1;
   if (m_ssaaScale > 4) m_ssaaScale = 4;
   m_width  = w * (int32_t)m_ssaaScale;
   m_height = h * (int32_t)m_ssaaScale;

   int sz          = m_width * m_height;
   m_colorBuf      = new ColorA[sz];
   m_fxaaBuf       = nullptr;
   m_taaBuf        = nullptr;
   m_taaHistory    = nullptr;
   m_taaDepth      = nullptr;
   m_ssaaBuf       = nullptr;
   m_msaaColorBuf  = nullptr;
   m_msaaDepthBuf  = nullptr;
   m_taaValid      = false;
   m_taaFrameIndex = 0;
   m_depthBuf      = new float[sz];
   if (!m_colorBuf || !m_depthBuf) {
      delete[] m_colorBuf;
      delete[] m_depthBuf;
      m_colorBuf      = nullptr;
      m_fxaaBuf       = nullptr;
      m_taaBuf        = nullptr;
      m_taaHistory    = nullptr;
      m_taaDepth      = nullptr;
      m_ssaaBuf       = nullptr;
      m_msaaColorBuf  = nullptr;
      m_msaaDepthBuf  = nullptr;
      m_taaValid      = false;
      m_taaFrameIndex = 0;
      m_depthBuf      = nullptr;
      m_width         = 0;
      m_height        = 0;
      m_outputWidth   = 0;
      m_outputHeight  = 0;
      m_ssaaScale     = 1;
   }
}

Renderer::~Renderer()
{
   delete[] m_colorBuf;
   delete[] m_fxaaBuf;
   delete[] m_taaBuf;
   delete[] m_taaHistory;
   delete[] m_taaDepth;
   delete[] m_ssaaBuf;
   delete[] m_msaaColorBuf;
   delete[] m_msaaDepthBuf;
   delete[] m_depthBuf;
}

void Renderer::Resize(int32_t w, int32_t h)
{
   delete[] m_colorBuf;
   delete[] m_fxaaBuf;
   delete[] m_taaBuf;
   delete[] m_taaHistory;
   delete[] m_taaDepth;
   delete[] m_ssaaBuf;
   delete[] m_msaaColorBuf;
   delete[] m_msaaDepthBuf;
   delete[] m_depthBuf;
   m_fxaaBuf       = nullptr;
   m_taaBuf        = nullptr;
   m_taaHistory    = nullptr;
   m_taaDepth      = nullptr;
   m_ssaaBuf       = nullptr;
   m_msaaColorBuf  = nullptr;
   m_msaaDepthBuf  = nullptr;
   m_taaValid      = false;
   m_taaFrameIndex = 0;
   m_colorBuf      = nullptr;
   m_depthBuf      = nullptr;

   if (w <= 0 || h <= 0) {
      m_width        = 0;
      m_height       = 0;
      m_outputWidth  = 0;
      m_outputHeight = 0;
      m_ssaaScale    = 1;
      return;
   }

   m_outputWidth  = w;
   m_outputHeight = h;
   m_ssaaScale    = GetSsaaScale();
   if (m_ssaaScale < 1) m_ssaaScale = 1;
   if (m_ssaaScale > 4) m_ssaaScale = 4;
   m_width  = w * (int32_t)m_ssaaScale;
   m_height = h * (int32_t)m_ssaaScale;

   int     sz        = m_width * m_height;
   ColorA *nextColor = new ColorA[sz];
   float  *nextDepth = new float[sz];
   if (!nextColor || !nextDepth) {
      delete[] nextColor;
      delete[] nextDepth;
      m_width        = 0;
      m_height       = 0;
      m_outputWidth  = 0;
      m_outputHeight = 0;
      m_ssaaScale    = 1;
      return;
   }

   m_colorBuf = nextColor;
   m_depthBuf = nextDepth;
}

bool Renderer::EnsureMsaaBuffers()
{
   if (m_width <= 0 || m_height <= 0) {
      return false;
   }
   if (m_msaaColorBuf && m_msaaDepthBuf) {
      return true;
   }

   delete[] m_msaaColorBuf;
   delete[] m_msaaDepthBuf;
   m_msaaColorBuf = nullptr;
   m_msaaDepthBuf = nullptr;

   int sampleTotal = m_width * m_height * 4;
   m_msaaColorBuf  = new ColorA[sampleTotal];
   m_msaaDepthBuf  = new float[sampleTotal];
   if (!m_msaaColorBuf || !m_msaaDepthBuf) {
      delete[] m_msaaColorBuf;
      delete[] m_msaaDepthBuf;
      m_msaaColorBuf = nullptr;
      m_msaaDepthBuf = nullptr;
      return false;
   }

   int total = m_width * m_height;
   for (int i = 0; i < total; i++) {
      int base = i * 4;
      for (int sample = 0; sample < 4; sample++) {
         m_msaaColorBuf[base + sample] = m_colorBuf ? m_colorBuf[i] : ColorA{0, 0, 0, 255};
         m_msaaDepthBuf[base + sample] = m_depthBuf ? m_depthBuf[i] : 0.0f;
      }
   }
   return true;
}

void Renderer::ResolveMsaa()
{
   if (!IsMsaaEnabled() || !m_msaaColorBuf || !m_msaaDepthBuf || !m_colorBuf || !m_depthBuf) {
      return;
   }

   int total = m_width * m_height;
   for (int i = 0; i < total; i++) {
      int           base = i * 4;
      const ColorA &c0   = m_msaaColorBuf[base + 0];
      const ColorA &c1   = m_msaaColorBuf[base + 1];
      const ColorA &c2   = m_msaaColorBuf[base + 2];
      const ColorA &c3   = m_msaaColorBuf[base + 3];

      ColorA out;
      out.r         = (uint8_t)(((int)c0.r + (int)c1.r + (int)c2.r + (int)c3.r) >> 2);
      out.g         = (uint8_t)(((int)c0.g + (int)c1.g + (int)c2.g + (int)c3.g) >> 2);
      out.b         = (uint8_t)(((int)c0.b + (int)c1.b + (int)c2.b + (int)c3.b) >> 2);
      out.a         = 255;
      m_colorBuf[i] = out;

      float d       = m_msaaDepthBuf[base + 0];
      d             = HE3D_MAX(d, m_msaaDepthBuf[base + 1]);
      d             = HE3D_MAX(d, m_msaaDepthBuf[base + 2]);
      d             = HE3D_MAX(d, m_msaaDepthBuf[base + 3]);
      m_depthBuf[i] = d;
   }
}

void Renderer::Clear(color3 color)
{
   if (m_width <= 0 || m_height <= 0 || !m_colorBuf || !m_depthBuf) {
      return;
   }

   ColorA c;
   c.r = (uint8_t)HE3D_CLAMP((int)(color.r * 255.0f), 0, 255);
   c.g = (uint8_t)HE3D_CLAMP((int)(color.g * 255.0f), 0, 255);
   c.b = (uint8_t)HE3D_CLAMP((int)(color.b * 255.0f), 0, 255);
   c.a = 255;

   int     total    = m_width * m_height;
   ColorA *colorBuf = m_colorBuf;
   float  *depth    = m_depthBuf;
   for (int i = 0; i < total; i++) {
      colorBuf[i] = c;
      depth[i]    = 0.0f;
   }

   if (IsMsaaEnabled() && EnsureMsaaBuffers()) {
      int sampleTotal = total * 4;
      for (int i = 0; i < sampleTotal; i++) {
         m_msaaColorBuf[i] = c;
         m_msaaDepthBuf[i] = 0.0f;
      }
   }
}

static float Halton(uint32_t index, uint32_t base)
{
   float f = 1.0f;
   float r = 0.0f;
   while (index > 0) {
      f /= (float)base;
      r += f * (float)(index % base);
      index /= base;
   }
   return r;
}

float2 Renderer::CurrentTaaJitter() const
{
   if (!IsTaaEnabled() || m_width <= 0 || m_height <= 0) {
      return {0.0f, 0.0f};
   }

   uint32_t sample = (m_taaFrameIndex & 7U) + 1U;
   float    x      = Halton(sample, 2U) - 0.5f;
   float    y      = Halton(sample, 3U) - 0.5f;
   return {x * 0.5f, y * 0.5f};
}

// ============================================================================
// [10] DrawGameObject: solid color path.
// [10] DrawGameObject：纯色绘制路径。
// ============================================================================
void Renderer::DrawGameObject(const GameObject &obj, const Camera &cam, color3 color)
{
   if (m_width <= 0 || m_height <= 0) return;
   const Mesh &mesh = obj.GetMesh();
   if (!mesh.IsValid() || mesh.GetVertexCount() < 3) return;
   if (!mesh.GetVertices() || !mesh.GetUVs()) return;

   float  fovRad         = cam.fov * 0.01745329252f;
   float  fovScale       = 1.0f / tanf(fovRad * 0.5f);
   float  halfW          = m_width * 0.5f;
   float  halfH          = m_height * 0.5f;
   float  scaleX         = fovScale * halfW / ((float)m_width / (float)m_height);
   float  scaleY         = fovScale * halfH;
   float2 jitter         = CurrentTaaJitter();
   float3 lightDir       = mainLight.direction.normalizeFast();
   Mat3   camInvRot      = Mat3::FromQuat(cam.orientation).Transpose();
   bool   identityObjRot = QuatIsIdentity(obj.orientation);
   Mat3 objRot = identityObjRot ? Mat3{1, 0, 0, 0, 1, 0, 0, 0, 1} : Mat3::FromQuat(obj.orientation);

   const float3 *verts      = mesh.GetVertices();
   const float3 *triNormals = mesh.GetTriangleNormals();
   int           vc         = mesh.GetVertexCount();

   int triIndex = 0;
   for (int i = 0; i < vc; i += 3, triIndex++) {
      float3     vw[3], vv[3];
      ClipVertex clipIn[3];
      ClipVertex clipOut[4];

      for (int j = 0; j < 3; j++) {
         float3 v       = identityObjRot ? (verts[i + j] + obj.position)
                                         : (objRot.Mul(verts[i + j]) + obj.position);
         vw[j]          = v;
         v              = camInvRot.Mul(v - cam.position);
         vv[j]          = v;
         clipIn[j].view = v;
         clipIn[j].uv   = {0, 0};
      }

      float3 ab = vv[1] - vv[0], ac = vv[2] - vv[0];
      if (float3::dot(float3::cross(ab, ac), vv[0]) >= 0) continue;

      int clippedCount = ClipTriangleToNearPlane(clipIn, clipOut);
      if (clippedCount < 3) continue;

      float3 n    = triNormals
                        ? (identityObjRot ? triNormals[triIndex] : objRot.Mul(triNormals[triIndex]))
                        : float3::cross(vw[1] - vw[0], vw[2] - vw[0]).normalizeFast();
      float  diff = HE3D_MAX(0.0f, float3::dot(n, lightDir));
      float  intens = mainLight.ambient + diff;

      for (int k = 1; k < clippedCount - 1; k++) {
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
void Renderer::DrawGameObject(const GameObject &obj, const Camera &cam, const Texture &tex)
{
   if (m_width <= 0 || m_height <= 0) return;
   const Mesh &mesh = obj.GetMesh();
   if (!mesh.IsValid() || mesh.GetVertexCount() < 3) return;
   if (!mesh.GetVertices() || !mesh.GetUVs()) return;
   if (!tex.IsValid()) {
      DrawGameObject(obj, cam, color3(0.8f, 0.8f, 0.8f));
      return;
   }

   float  fovRad         = cam.fov * 0.01745329252f;
   float  fovScale       = 1.0f / tanf(fovRad * 0.5f);
   float  halfW          = m_width * 0.5f;
   float  halfH          = m_height * 0.5f;
   float  scaleX         = fovScale * halfW / ((float)m_width / (float)m_height);
   float  scaleY         = fovScale * halfH;
   float2 jitter         = CurrentTaaJitter();
   float3 lightDir       = mainLight.direction.normalizeFast();
   bool   identityObjRot = QuatIsIdentity(obj.orientation);
   Mat3 objRot = identityObjRot ? Mat3{1, 0, 0, 0, 1, 0, 0, 0, 1} : Mat3::FromQuat(obj.orientation);
   Mat3 camInvRot = Mat3::FromQuat(cam.orientation).Transpose();

   const float3 *verts      = mesh.GetVertices();
   const float2 *uvs        = mesh.GetUVs();
   const float3 *triNormals = mesh.GetTriangleNormals();
   int           vc         = mesh.GetVertexCount();

   int triIndex = 0;
   for (int i = 0; i < vc; i += 3, triIndex++) {
      float3     vw[3], vv[3];
      ClipVertex clipIn[3];
      ClipVertex clipOut[4];

      for (int j = 0; j < 3; j++) {
         float3 v       = identityObjRot ? (verts[i + j] + obj.position)
                                         : (objRot.Mul(verts[i + j]) + obj.position);
         vw[j]          = v;
         v              = camInvRot.Mul(v - cam.position);
         vv[j]          = v;
         clipIn[j].view = v;
         clipIn[j].uv   = uvs[i + j];
      }

      float3 ab = vv[1] - vv[0], ac = vv[2] - vv[0];
      if (float3::dot(float3::cross(ab, ac), vv[0]) >= 0) continue;

      int clippedCount = ClipTriangleToNearPlane(clipIn, clipOut);
      if (clippedCount < 3) continue;

      float3 n    = triNormals
                        ? (identityObjRot ? triNormals[triIndex] : objRot.Mul(triNormals[triIndex]))
                        : float3::cross(vw[1] - vw[0], vw[2] - vw[0]).normalizeFast();
      float  diff = HE3D_MAX(0.0f, float3::dot(n, lightDir));
      float  intens = mainLight.ambient + diff;

      for (int k = 1; k < clippedCount - 1; k++) {
         float3 clippedVv[3]  = {clipOut[0].view, clipOut[k].view, clipOut[k + 1].view};
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
static const float he3d_msaa_offsets4[4][2] = {
    {0.25f, 0.25f}, {0.75f, 0.25f}, {0.25f, 0.75f}, {0.75f, 0.75f}};

static uint8_t TriangleSampleMask4(int x, int y, float x0, float y0, float x1, float y1, float x2,
                                   float y2, float invA)
{
   uint8_t mask = 0;
   for (int i = 0; i < 4; i++) {
      float px = (float)x + he3d_msaa_offsets4[i][0];
      float py = (float)y + he3d_msaa_offsets4[i][1];
      float w0 = ((x1 - px) * (y2 - py) - (y1 - py) * (x2 - px)) * invA;
      float w1 = ((x2 - px) * (y0 - py) - (y2 - py) * (x0 - px)) * invA;
      float w2 = 1.0f - w0 - w1;
      if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
         mask = (uint8_t)(mask | (uint8_t)(1U << i));
      }
   }
   return mask;
}

void Renderer::RasterizeSolid(const float3 *vv, const float2 *ps, color3 color)
{
   float minXf = HE3D_MIN(HE3D_MIN(ps[0].x, ps[1].x), ps[2].x);
   float maxXf = HE3D_MAX(HE3D_MAX(ps[0].x, ps[1].x), ps[2].x);
   float minYf = HE3D_MIN(HE3D_MIN(ps[0].y, ps[1].y), ps[2].y);
   float maxYf = HE3D_MAX(HE3D_MAX(ps[0].y, ps[1].y), ps[2].y);

   int mnX = HE3D_MAX(0, (int)minXf);
   int mxX = HE3D_MIN(m_width - 1, (int)maxXf + 1);
   int mnY = HE3D_MAX(0, (int)minYf);
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
   float  *db        = m_depthBuf;
   ColorA *cbuf      = m_colorBuf;
   bool    msaa      = IsMsaaEnabled() && EnsureMsaaBuffers();
   ColorA *msaaColor = m_msaaColorBuf;
   float  *msaaDepth = m_msaaDepthBuf;

   for (int y = mnY; y <= mxY; y++) {
      float py = (float)y + 0.5f, px0 = (float)mnX + 0.5f;
      float w0  = ((x1 - px0) * (y2 - py) - (y1 - py) * (x2 - px0)) * invA;
      float w1  = ((x2 - px0) * (y0 - py) - (y2 - py) * (x0 - px0)) * invA;
      float w2  = 1.0f - w0 - w1;
      float ciz = w0 * iz0 + w1 * iz1 + w2 * iz2;

      int rb = y * stride;
      for (int x = mnX; x <= mxX; x++) {
         int idx = rb + x;
         if (msaa) {
            uint8_t mask          = TriangleSampleMask4(x, y, x0, y0, x1, y1, x2, y2, invA);
            float   pixelMaxDepth = db[idx];
            for (int sample = 0; sample < 4; sample++) {
               if ((mask & (1U << sample)) == 0) {
                  continue;
               }
               float spx         = (float)x + he3d_msaa_offsets4[sample][0];
               float spy         = (float)y + he3d_msaa_offsets4[sample][1];
               float sw0         = ((x1 - spx) * (y2 - spy) - (y1 - spy) * (x2 - spx)) * invA;
               float sw1         = ((x2 - spx) * (y0 - spy) - (y2 - spy) * (x0 - spx)) * invA;
               float sw2         = 1.0f - sw0 - sw1;
               float sampleDepth = sw0 * iz0 + sw1 * iz1 + sw2 * iz2;
               int   sampleIdx   = idx * 4 + sample;
               if (sampleDepth > msaaDepth[sampleIdx]) {
                  msaaDepth[sampleIdx] = sampleDepth;
                  msaaColor[sampleIdx] = xc;
                  pixelMaxDepth        = HE3D_MAX(pixelMaxDepth, sampleDepth);
               }
            }
            db[idx] = pixelMaxDepth;
         } else if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f && ciz > db[idx]) {
            db[idx]   = ciz;
            cbuf[idx] = xc;
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
void Renderer::RasterizeTextured(const float3 *vv, const float2 *ps, const float2 *uvs,
                                 float intens, const Texture &tex)
{
   float minXf = HE3D_MIN(HE3D_MIN(ps[0].x, ps[1].x), ps[2].x);
   float maxXf = HE3D_MAX(HE3D_MAX(ps[0].x, ps[1].x), ps[2].x);
   float minYf = HE3D_MIN(HE3D_MIN(ps[0].y, ps[1].y), ps[2].y);
   float maxYf = HE3D_MAX(HE3D_MAX(ps[0].y, ps[1].y), ps[2].y);

   int mnX = HE3D_MAX(0, (int)minXf);
   int mxX = HE3D_MIN(m_width - 1, (int)maxXf + 1);
   int mnY = HE3D_MAX(0, (int)minYf);
   int mxY = HE3D_MIN(m_height - 1, (int)maxYf + 1);
   if (mnX > mxX || mnY > mxY) return;

   float x0 = ps[0].x, y0 = ps[0].y;
   float x1 = ps[1].x, y1 = ps[1].y;
   float x2 = ps[2].x, y2 = ps[2].y;

   float area = (x1 - x0) * (y2 - y0) - (y1 - y0) * (x2 - x0);
   if (HE3D_ABS(area) < 0.0001f) return;
   float invA = 1.0f / area;

   float  dw0 = (y1 - y2) * invA;
   float  dw1 = (y2 - y0) * invA;
   float  dw2 = -dw0 - dw1;
   float  iz0 = 1.0f / vv[0].z, iz1 = 1.0f / vv[1].z, iz2 = 1.0f / vv[2].z;
   float2 uz0 = uvs[0] * iz0, uz1 = uvs[1] * iz1, uz2 = uvs[2] * iz2;
   float  dizDx = dw0 * iz0 + dw1 * iz1 + dw2 * iz2;
   float  duzDx = dw0 * uz0.x + dw1 * uz1.x + dw2 * uz2.x;
   float  dvzDx = dw0 * uz0.y + dw1 * uz1.y + dw2 * uz2.y;

   color3        lc     = mainLight.color * intens;
   int           lr     = HE3D_CLAMP((int)(lc.r * 256.0f), 0, 512);
   int           lg     = HE3D_CLAMP((int)(lc.g * 256.0f), 0, 512);
   int           lb     = HE3D_CLAMP((int)(lc.b * 256.0f), 0, 512);
   const ColorA *tp     = tex.m_pixels;
   int           tw     = tex.m_width;
   int           th     = tex.m_height;
   int           twm1   = tw - 1;
   int           thm1   = th - 1;
   int           stride = m_width;

   float  *db        = m_depthBuf;
   ColorA *cbuf      = m_colorBuf;
   bool    msaa      = IsMsaaEnabled() && EnsureMsaaBuffers();
   ColorA *msaaColor = m_msaaColorBuf;
   float  *msaaDepth = m_msaaDepthBuf;

   for (int y = mnY; y <= mxY; y++) {
      float py = (float)y + 0.5f, px0 = (float)mnX + 0.5f;
      float w0  = ((x1 - px0) * (y2 - py) - (y1 - py) * (x2 - px0)) * invA;
      float w1  = ((x2 - px0) * (y0 - py) - (y2 - py) * (x0 - px0)) * invA;
      float w2  = 1.0f - w0 - w1;
      float ciz = w0 * iz0 + w1 * iz1 + w2 * iz2;
      float cuz = w0 * uz0.x + w1 * uz1.x + w2 * uz2.x;
      float cvz = w0 * uz0.y + w1 * uz1.y + w2 * uz2.y;

      int rb = y * stride;
      for (int x = mnX; x <= mxX; x++) {
         int idx = rb + x;
         if (msaa) {
            uint8_t mask          = TriangleSampleMask4(x, y, x0, y0, x1, y1, x2, y2, invA);
            float   pixelMaxDepth = db[idx];
            for (int sample = 0; sample < 4; sample++) {
               if ((mask & (1U << sample)) == 0) {
                  continue;
               }

               float spx         = (float)x + he3d_msaa_offsets4[sample][0];
               float spy         = (float)y + he3d_msaa_offsets4[sample][1];
               float sw0         = ((x1 - spx) * (y2 - spy) - (y1 - spy) * (x2 - spx)) * invA;
               float sw1         = ((x2 - spx) * (y0 - spy) - (y2 - spy) * (x0 - spx)) * invA;
               float sw2         = 1.0f - sw0 - sw1;
               float sampleDepth = sw0 * iz0 + sw1 * iz1 + sw2 * iz2;
               int   sampleIdx   = idx * 4 + sample;
               if (sampleDepth > msaaDepth[sampleIdx]) {
                  float sampleUz       = sw0 * uz0.x + sw1 * uz1.x + sw2 * uz2.x;
                  float sampleVz       = sw0 * uz0.y + sw1 * uz1.y + sw2 * uz2.y;
                  float invSampleDepth = 1.0f / sampleDepth;
                  float u              = sampleUz * invSampleDepth;
                  float v              = sampleVz * invSampleDepth;
                  if (u < 0.0f || u >= 1.0f) u = fracf(u);
                  if (v < 0.0f || v >= 1.0f) v = fracf(v);
                  int tx       = (int)(u * tw);
                  int ty       = (int)(v * th);
                  tx           = HE3D_CLAMP(tx, 0, twm1);
                  ty           = HE3D_CLAMP(ty, 0, thm1);
                  ColorA texel = tp[ty * tw + tx];
                  ColorA shaded;
                  shaded.r             = (uint8_t)HE3D_CLAMP(((int)texel.r * lr) >> 8, 0, 255);
                  shaded.g             = (uint8_t)HE3D_CLAMP(((int)texel.g * lg) >> 8, 0, 255);
                  shaded.b             = (uint8_t)HE3D_CLAMP(((int)texel.b * lb) >> 8, 0, 255);
                  shaded.a             = 255;
                  msaaDepth[sampleIdx] = sampleDepth;
                  msaaColor[sampleIdx] = shaded;
                  pixelMaxDepth        = HE3D_MAX(pixelMaxDepth, sampleDepth);
               }
            }
            db[idx] = pixelMaxDepth;
         } else if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f && ciz > db[idx]) {
            db[idx]      = ciz;
            float invCiz = 1.0f / ciz;
            float u      = cuz * invCiz;
            float v      = cvz * invCiz;
            if (u < 0.0f || u >= 1.0f) u = fracf(u);
            if (v < 0.0f || v >= 1.0f) v = fracf(v);
            int tx       = (int)(u * tw);
            int ty       = (int)(v * th);
            tx           = HE3D_CLAMP(tx, 0, twm1);
            ty           = HE3D_CLAMP(ty, 0, thm1);
            ColorA texel = tp[ty * tw + tx];
            ColorA shaded;
            shaded.r  = (uint8_t)HE3D_CLAMP(((int)texel.r * lr) >> 8, 0, 255);
            shaded.g  = (uint8_t)HE3D_CLAMP(((int)texel.g * lg) >> 8, 0, 255);
            shaded.b  = (uint8_t)HE3D_CLAMP(((int)texel.b * lb) >> 8, 0, 255);
            shaded.a  = 255;
            cbuf[idx] = shaded;
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
static int he3d_luma(const ColorA &c)
{
   return ((int)c.r * 77 + (int)c.g * 150 + (int)c.b * 29) >> 8;
}

static uint8_t he3d_blend_u8(uint8_t a, uint8_t b)
{
   return (uint8_t)(((int)a * 141 + (int)b * 115) >> 8);
}

bool Renderer::ApplyFxaa()
{
   if (m_width <= 2 || m_height <= 2 || !m_colorBuf) {
      return false;
   }

   int total = m_width * m_height;
   if (!m_fxaaBuf) {
      m_fxaaBuf = new ColorA[total];
      if (!m_fxaaBuf) {
         return false;
      }
   }

   for (int x = 0; x < m_width; x++) {
      m_fxaaBuf[x]                            = m_colorBuf[x];
      m_fxaaBuf[(m_height - 1) * m_width + x] = m_colorBuf[(m_height - 1) * m_width + x];
   }
   for (int y = 1; y < m_height - 1; y++) {
      m_fxaaBuf[y * m_width]               = m_colorBuf[y * m_width];
      m_fxaaBuf[y * m_width + m_width - 1] = m_colorBuf[y * m_width + m_width - 1];
   }

   const int edgeThreshold    = 20;
   const int edgeThresholdMin = 8;

   for (int y = 1; y < m_height - 1; y++) {
      int row = y * m_width;
      for (int x = 1; x < m_width - 1; x++) {
         int           idx    = row + x;
         const ColorA &center = m_colorBuf[idx];
         int           lM     = he3d_luma(center);
         int           lN     = he3d_luma(m_colorBuf[idx - m_width]);
         int           lS     = he3d_luma(m_colorBuf[idx + m_width]);
         int           lW     = he3d_luma(m_colorBuf[idx - 1]);
         int           lE     = he3d_luma(m_colorBuf[idx + 1]);

         int lMin     = HE3D_MIN(lM, HE3D_MIN(HE3D_MIN(lN, lS), HE3D_MIN(lW, lE)));
         int lMax     = HE3D_MAX(lM, HE3D_MAX(HE3D_MAX(lN, lS), HE3D_MAX(lW, lE)));
         int contrast = lMax - lMin;
         if (contrast < HE3D_MAX(edgeThresholdMin, (lMax * 20) >> 8) || contrast < edgeThreshold) {
            m_fxaaBuf[idx] = center;
            continue;
         }

         int           horizontal = HE3D_ABS(lN + lS - 2 * lM);
         int           vertical   = HE3D_ABS(lW + lE - 2 * lM);
         const ColorA &a = horizontal >= vertical ? m_colorBuf[idx - 1] : m_colorBuf[idx - m_width];
         const ColorA &b = horizontal >= vertical ? m_colorBuf[idx + 1] : m_colorBuf[idx + m_width];

         ColorA out;
         out.r          = he3d_blend_u8(center.r, (uint8_t)(((int)a.r + (int)b.r) >> 1));
         out.g          = he3d_blend_u8(center.g, (uint8_t)(((int)a.g + (int)b.g) >> 1));
         out.b          = he3d_blend_u8(center.b, (uint8_t)(((int)a.b + (int)b.b) >> 1));
         out.a          = center.a;
         m_fxaaBuf[idx] = out;
      }
   }

   return true;
}

bool Renderer::ApplyTaa(const ColorA *source)
{
   if (m_width <= 0 || m_height <= 0 || !source || !m_depthBuf) {
      return false;
   }

   int total = m_width * m_height;
   if (!m_taaBuf) {
      m_taaBuf = new ColorA[total];
      if (!m_taaBuf) {
         return false;
      }
   }
   if (!m_taaHistory) {
      m_taaHistory = new ColorA[total];
      if (!m_taaHistory) {
         return false;
      }
   }
   if (!m_taaDepth) {
      m_taaDepth = new float[total];
      if (!m_taaDepth) {
         return false;
      }
   }

   if (!m_taaValid) {
      for (int i = 0; i < total; i++) {
         m_taaBuf[i]     = source[i];
         m_taaHistory[i] = source[i];
         m_taaDepth[i]   = m_depthBuf[i];
      }
      m_taaValid = true;
      m_taaFrameIndex++;
      return true;
   }

   for (int y = 0; y < m_height; y++) {
      for (int x = 0; x < m_width; x++) {
         int           idx  = y * m_width + x;
         const ColorA &cur  = source[idx];
         const ColorA &hist = m_taaHistory[idx];

         int minR = 255, minG = 255, minB = 255;
         int maxR = 0, maxG = 0, maxB = 0;
         for (int oy = -1; oy <= 1; oy++) {
            int sy = HE3D_CLAMP(y + oy, 0, m_height - 1);
            for (int ox = -1; ox <= 1; ox++) {
               int           sx = HE3D_CLAMP(x + ox, 0, m_width - 1);
               const ColorA &c  = source[sy * m_width + sx];
               minR             = HE3D_MIN(minR, (int)c.r);
               minG             = HE3D_MIN(minG, (int)c.g);
               minB             = HE3D_MIN(minB, (int)c.b);
               maxR             = HE3D_MAX(maxR, (int)c.r);
               maxG             = HE3D_MAX(maxG, (int)c.g);
               maxB             = HE3D_MAX(maxB, (int)c.b);
            }
         }

         int   histR             = HE3D_CLAMP((int)hist.r, minR, maxR);
         int   histG             = HE3D_CLAMP((int)hist.g, minG, maxG);
         int   histB             = HE3D_CLAMP((int)hist.b, minB, maxB);
         int   rawColorDelta     = HE3D_ABS((int)cur.r - (int)hist.r) +
                                   HE3D_ABS((int)cur.g - (int)hist.g) +
                                   HE3D_ABS((int)cur.b - (int)hist.b);
         int   clampedColorDelta = HE3D_ABS((int)cur.r - histR) + HE3D_ABS((int)cur.g - histG) +
                                   HE3D_ABS((int)cur.b - histB);
         float depthDelta        = HE3D_ABS(m_depthBuf[idx] - m_taaDepth[idx]);
         bool  rejectHistory =
             (m_depthBuf[idx] > 0.0f && depthDelta > HE3D_MAX(0.0025f, m_depthBuf[idx] * 0.05f)) ||
             rawColorDelta > 120 || clampedColorDelta > 80;

         ColorA out;
         if (rejectHistory) {
            out = cur;
         } else {
            out.r = (uint8_t)(((int)cur.r * 192 + histR * 64) >> 8);
            out.g = (uint8_t)(((int)cur.g * 192 + histG * 64) >> 8);
            out.b = (uint8_t)(((int)cur.b * 192 + histB * 64) >> 8);
            out.a = cur.a;
         }
         m_taaBuf[idx] = out;
      }
   }

   for (int i = 0; i < total; i++) {
      m_taaHistory[i] = m_taaBuf[i];
      m_taaDepth[i]   = m_depthBuf[i];
   }
   m_taaFrameIndex++;
   return true;
}

const ColorA *Renderer::ApplySsaa(const ColorA *source)
{
   if (m_ssaaScale <= 1 || !source || m_outputWidth <= 0 || m_outputHeight <= 0) {
      return source;
   }

   int outTotal = m_outputWidth * m_outputHeight;
   if (!m_ssaaBuf) {
      m_ssaaBuf = new ColorA[outTotal];
      if (!m_ssaaBuf) {
         return source;
      }
   }

   int scale       = (int)m_ssaaScale;
   int sampleCount = scale * scale;
   for (int y = 0; y < m_outputHeight; y++) {
      for (int x = 0; x < m_outputWidth; x++) {
         int sumR  = 0;
         int sumG  = 0;
         int sumB  = 0;
         int sumA  = 0;
         int baseY = y * scale;
         int baseX = x * scale;
         for (int sy = 0; sy < scale; sy++) {
            int row = (baseY + sy) * m_width;
            for (int sx = 0; sx < scale; sx++) {
               const ColorA &c = source[row + baseX + sx];
               sumR += c.r;
               sumG += c.g;
               sumB += c.b;
               sumA += c.a;
            }
         }
         ColorA out;
         out.r                            = (uint8_t)(sumR / sampleCount);
         out.g                            = (uint8_t)(sumG / sampleCount);
         out.b                            = (uint8_t)(sumB / sampleCount);
         out.a                            = (uint8_t)(sumA / sampleCount);
         m_ssaaBuf[y * m_outputWidth + x] = out;
      }
   }

   return m_ssaaBuf;
}

void Renderer::Present()
{
   if (m_width <= 0 || m_height <= 0 || !m_colorBuf) {
      return;
   }

   ResolveMsaa();

   const ColorA *pixels = m_colorBuf;
   if (IsFxaaEnabled() && ApplyFxaa()) {
      pixels = m_fxaaBuf;
   }
   if (IsTaaEnabled() && ApplyTaa(pixels)) {
      pixels = m_taaBuf;
   } else if (!IsTaaEnabled()) {
      m_taaValid      = false;
      m_taaFrameIndex = 0;
   }
   pixels                = ApplySsaa(pixels);
   int32_t presentWidth  = (m_ssaaScale > 1) ? m_outputWidth : m_width;
   int32_t presentHeight = (m_ssaaScale > 1) ? m_outputHeight : m_height;
   HE3D::Present(m_window, presentWidth, presentHeight, pixels);
}

} // namespace HE3D
