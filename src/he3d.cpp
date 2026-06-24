/*
 * HE3D Engine for 3D - C++ Implementation
 * OBJ loader, BMP texture loader, and software rasterizer.
 * Platform services are supplied through he3d_platform.hpp.
 */
#include "he3d.hpp"

namespace HE3D {

static const Platform *g_platform = nullptr;
static unsigned int g_frameRateLimit = 0;
static bool g_fxaaEnabled = false;

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

void *Alloc(unsigned long size)
{
    return GetPlatform()->alloc(size);
}

void Free(void *ptr)
{
    GetPlatform()->free(ptr);
}

void SetFrameRateLimit(unsigned int fps)
{
    g_frameRateLimit = fps;
}

unsigned int GetFrameRateLimit()
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
        unsigned long long sleepMs = (unsigned long long)((targetFrameSeconds - elapsed) * 1000.0);
        if (sleepMs > 0)
        {
            SleepMilliseconds(sleepMs);
        }
    }
}

}

// Route C++ allocation through HE3D's platform allocator.
void *operator new(unsigned long size)
{
    return HE3D::Alloc(size);
}

void *operator new[](unsigned long size)
{
    return HE3D::Alloc(size);
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
// [0] Float parser (XJ380 has no strtod/strtof)
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
// [1] In-memory line reader for OBJ parsing
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
    if (nx < end && *nx == '\r') nx++;
    if (nx < end && *nx == '\n') nx++;
    return nx;
}

// Count a face's vertex count & triangulate count
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

static int ClipTriangleNear(const ClipVertex *input, ClipVertex *output)
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
                                float scaleX, float scaleY)
{
    for (int i = 0; i < 3; i++)
    {
        float invZ = 1.0f / vv[i].z;
        ps[i] = {halfW + vv[i].x * invZ * scaleX,
                 halfH - vv[i].y * invZ * scaleY};
    }
}

static float ScreenTriangleArea(const float2 *ps)
{
    return (ps[1].x - ps[0].x) * (ps[2].y - ps[0].y)
         - (ps[1].y - ps[0].y) * (ps[2].x - ps[0].x);
}

static bool ScreenTriangleOutside(const float2 *ps, int width, int height)
{
    if (ps[0].x < 0.0f && ps[1].x < 0.0f && ps[2].x < 0.0f) return true;
    if (ps[0].x >= (float)width && ps[1].x >= (float)width && ps[2].x >= (float)width) return true;
    if (ps[0].y < 0.0f && ps[1].y < 0.0f && ps[2].y < 0.0f) return true;
    if (ps[0].y >= (float)height && ps[1].y >= (float)height && ps[2].y >= (float)height) return true;
    return false;
}

// ============================================================================
// [3] Mesh allocation helpers
// ============================================================================
bool Mesh::Init(int vertexCount)
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

Mesh *Mesh::Create(int vertexCount)
{
    Mesh *mesh = new Mesh();
    if (!mesh || !mesh->Init(vertexCount))
    {
        delete mesh;
        return nullptr;
    }
    return mesh;
}

bool Mesh::Init(const float3 *srcVertices, const float2 *srcUvs, int vertexCount)
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
                   int vertexCount)
{
    Mesh *mesh = new Mesh();
    if (!mesh || !mesh->Init(srcVertices, srcUvs, vertexCount))
    {
        delete mesh;
        return nullptr;
    }
    return mesh;
}

// ============================================================================
// [4] Mesh::LoadOBJ — Two-pass over in-memory file data
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

    // ---- Pass 1: count ----
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

    // Allocate temporary OBJ buffers after the first pass has established sizes.
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
    mesh->vertCount = 0; // will increment during face processing

    // ---- Pass 2: fill ----
    c = buf;
    while ((c = MemGetLine(c, end, line, sizeof(line))) != nullptr) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;

        if (p[0] == 'v' && p[1] == ' ') {
            float3 vtx;
            ParseFloat(p + 2, &vtx.x);
            char *py = p + 2; while (*py && *py != ' ' && *py != '\t') py++; while (*py == ' ' || *py == '\t') py++;
            ParseFloat(py, &vtx.y);
            char *pz = py;     while (*pz && *pz != ' ' && *pz != '\t') pz++; while (*pz == ' ' || *pz == '\t') pz++;
            ParseFloat(pz, &vtx.z);
            if (vc < vCount) all_v[vc++] = vtx;
        }
        else if (p[0] == 'v' && p[1] == 't' && (p[2] == ' ' || p[2] == '\t') && all_vt) {
            float2 uv;
            char *pu = p + 3; ParseFloat(pu, &uv.x);
            char *pv = pu;     while (*pv && *pv != ' ' && *pv != '\t') pv++; while (*pv == ' ' || *pv == '\t') pv++;
            float rawV; ParseFloat(pv, &rawV); uv.y = 1.0f - rawV;
            if (vtc < vtCount) all_vt[vtc++] = uv;
        }
        else if (p[0] == 'f' && p[1] == ' ') {
            struct FV { int v, vt; };
            FV fv[32]; int fc = 0;
            char *tok = p + 2;
            while (*tok && fc < 32) {
                while (*tok == ' ' || *tok == '\t') tok++;
                if (*tok == 0 || *tok == '\n' || *tok == '\r') break;
                int vi = 0, ti = 0;
                while (*tok >= '0' && *tok <= '9') { vi = vi * 10 + (*tok - '0'); tok++; }
                if (*tok == '/') {
                    tok++;
                    if (*tok != '/') { ti = 0; while (*tok >= '0' && *tok <= '9') { ti = ti * 10 + (*tok - '0'); tok++; } }
                    if (*tok == '/') { tok++; while (*tok >= '0' && *tok <= '9') tok++; }
                }
                fv[fc].v = vi - 1;
                fv[fc].vt = ti - 1;
                fc++;
            }

            bool validFace = (fc >= 3);
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
// [5] Texture::Sample — Nearest-neighbor with wrap
// ============================================================================
float3 Texture::Sample(float u, float v) const {
    if (!valid || !pixels) return {1.0f, 0.0f, 1.0f};
    u = fracf(u);
    v = fracf(v);
    int x = HE3D_CLAMP((int)(u * width),  0, width  - 1);
    int y = HE3D_CLAMP((int)(v * height), 0, height - 1);
    const ColorA& c = pixels[y * width + x];
    return {c.r * (1.0f / 255.0f), c.g * (1.0f / 255.0f), c.b * (1.0f / 255.0f)};
}

// ============================================================================
// [6] Texture::LoadBMP
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

    const unsigned char *d = file.data;
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
        const unsigned char *src = d + off;
        for (int y = 0; y < tex->height; y++) {
            int sY = topDown ? y : (tex->height - 1 - y);
            const unsigned char *row = src + sY * rowSize;
            ColorA *dst = pbuf + y * tex->width;
            for (int x = 0; x < tex->width; x++) {
                dst[x] = {row[2], row[1], row[0], 255};
                row += bytesPerPixel;
            }
        }
    }
    tex->pixels = pbuf;
    tex->valid  = true;

    CloseFile(&file);
    return tex;
}

// ============================================================================
// [7] Renderer
// ============================================================================
Renderer::Renderer(Window *window, int w, int h)
    : m_width(w), m_height(h), m_window(window)
{
    int sz = w * h;
    m_colorBuf = new ColorA[sz];
    m_fxaaBuf = nullptr;
    m_depthBuf = new float[sz];
    if (!m_colorBuf || !m_depthBuf) { m_width = 0; m_height = 0; }
}

Renderer::~Renderer() {
    delete[] m_colorBuf;
    delete[] m_fxaaBuf;
    delete[] m_depthBuf;
}

void Renderer::Resize(int w, int h) {
    delete[] m_colorBuf;
    delete[] m_fxaaBuf;
    delete[] m_depthBuf;
    m_width  = w;
    m_height = h;
    int sz = w * h;
    m_colorBuf = new ColorA[sz];
    m_fxaaBuf = nullptr;
    m_depthBuf = new float[sz];
}

void Renderer::Clear(float3 color) {
    ColorA c;
    c.r = (unsigned char)HE3D_CLAMP((int)(color.r * 255.0f), 0, 255);
    c.g = (unsigned char)HE3D_CLAMP((int)(color.g * 255.0f), 0, 255);
    c.b = (unsigned char)HE3D_CLAMP((int)(color.b * 255.0f), 0, 255);
    c.a = 255;

    int total = m_width * m_height;
    unsigned int packed = ((unsigned int)c.r)
                        | ((unsigned int)c.g << 8)
                        | ((unsigned int)c.b << 16)
                        | ((unsigned int)c.a << 24);
    unsigned long long packed2 = ((unsigned long long)packed << 32) | packed;

    unsigned long long *color64 = (unsigned long long *)m_colorBuf;
    int pairs = total >> 1;
    for (int i = 0; i < pairs; i++)
    {
        color64[i] = packed2;
    }
    if (total & 1)
    {
        m_colorBuf[total - 1] = c;
    }

    unsigned long long *depth64 = (unsigned long long *)m_depthBuf;
    for (int i = 0; i < pairs; i++)
    {
        depth64[i] = 0ULL;
    }
    if (total & 1)
    {
        m_depthBuf[total - 1] = 0.0f;
    }
}

// ============================================================================
// [8] DrawGameObject (solid)
// ============================================================================
void Renderer::DrawGameObject(const GameObject& obj, const Camera& cam, float3 color) {
    if (m_width <= 0 || m_height <= 0) return;
    if (!obj.mesh || obj.mesh->vertCount < 3) return;
    if (!obj.mesh->vertices || !obj.mesh->uvs) return;

    float fovRad    = cam.fov * 0.01745329252f;
    float fovScale  = 1.0f / tanf(fovRad * 0.5f);
    float halfW     = m_width  * 0.5f;
    float halfH     = m_height * 0.5f;
    float scaleX    = fovScale * halfW / ((float)m_width / (float)m_height);
    float scaleY    = fovScale * halfH;
    float3 lightDir = mainLight.direction.normalizeFast();
    bool identityObjRot = QuatIsIdentity(obj.orientation);
    Mat3 objRot     = identityObjRot ? Mat3{1,0,0,0,1,0,0,0,1} : Mat3::FromQuat(obj.orientation);
    Mat3 camInvRot  = Mat3::FromQuat(cam.orientation).Transpose();

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

        int clippedCount = ClipTriangleNear(clipIn, clipOut);
        if (clippedCount < 3) continue;

        float3 n = triNormals ? (identityObjRot ? triNormals[triIndex] : objRot.Mul(triNormals[triIndex]))
                              : float3::cross(vw[1] - vw[0], vw[2] - vw[0]).normalizeFast();
        float diff = HE3D_MAX(0.0f, float3::dot(n, lightDir));
        float intens = mainLight.ambient + diff;

        for (int k = 1; k < clippedCount - 1; k++)
        {
            float3 clippedVv[3] = {clipOut[0].view, clipOut[k].view, clipOut[k + 1].view};
            float2 ps[3];
            ProjectViewTriangle(clippedVv, ps, halfW, halfH, scaleX, scaleY);
            if (ScreenTriangleOutside(ps, m_width, m_height)) continue;
            if (ScreenTriangleArea(ps) <= 0.0f) continue;
            RasterizeSolid(clippedVv, ps, color * intens);
        }
    }
}

// ============================================================================
// [9] DrawGameObject (textured)
// ============================================================================
void Renderer::DrawGameObject(const GameObject& obj, const Camera& cam, const Texture& tex) {
    if (m_width <= 0 || m_height <= 0) return;
    if (!obj.mesh || obj.mesh->vertCount < 3) return;
    if (!obj.mesh->vertices || !obj.mesh->uvs) return;
    if (!tex.valid) { DrawGameObject(obj, cam, {0.8f, 0.8f, 0.8f}); return; }

    float fovRad    = cam.fov * 0.01745329252f;
    float fovScale  = 1.0f / tanf(fovRad * 0.5f);
    float halfW     = m_width  * 0.5f;
    float halfH     = m_height * 0.5f;
    float scaleX    = fovScale * halfW / ((float)m_width / (float)m_height);
    float scaleY    = fovScale * halfH;
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

        int clippedCount = ClipTriangleNear(clipIn, clipOut);
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
            ProjectViewTriangle(clippedVv, ps, halfW, halfH, scaleX, scaleY);
            if (ScreenTriangleOutside(ps, m_width, m_height)) continue;
            if (ScreenTriangleArea(ps) <= 0.0f) continue;
            RasterizeTextured(clippedVv, ps, clippedUvs, intens, tex);
        }
    }
}

// ============================================================================
// [10] RasterizeSolid
// ============================================================================
void Renderer::RasterizeSolid(const float3* vv, const float2* ps, float3 color) {
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

    // Pre-clamp color once, set fields by name for clarity
    ColorA xc;
    xc.r = (unsigned char)HE3D_CLAMP((int)(color.r * 255.0f), 0, 255);
    xc.g = (unsigned char)HE3D_CLAMP((int)(color.g * 255.0f), 0, 255);
    xc.b = (unsigned char)HE3D_CLAMP((int)(color.b * 255.0f), 0, 255);
    xc.a = 255;

    int stride = m_width;
    // idx = y*m_width + x is always in [0, m_width*m_height-1]
    // because mnX/mnY/mxX/mxY are clamped to valid range.
    float *db = m_depthBuf;
    ColorA *cbuf = m_colorBuf;

    for (int y = mnY; y <= mxY; y++) {
        float py = (float)y + 0.5f, px0 = (float)mnX + 0.5f;
        float w0 = ((x1 - px0) * (y2 - py) - (y1 - py) * (x2 - px0)) * invA;
        float w1 = ((x2 - px0) * (y0 - py) - (y2 - py) * (x0 - px0)) * invA;
        float w2 = 1.0f - w0 - w1;
        float ciz = w0 * iz0 + w1 * iz1 + w2 * iz2;

        int rb = y * stride;
        for (int x = mnX; x <= mxX; x++) {
            if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
                if (ciz > 0.000001f) {
                    int idx = rb + x;
                    // Depth buffer stores 1/z: closer = larger value
                    if (ciz > db[idx]) {
                        db[idx] = ciz;
                        cbuf[idx] = xc;
                    }
                }
            }
            w0 += dw0;
            w1 += dw1;
            w2 += dw2;
            ciz += dizDx;
        }
    }
}

// ============================================================================
// [11] RasterizeTextured
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

    float3 lc = mainLight.color * intens;
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
            if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
                if (ciz > 0.000001f) {
                    int idx = rb + x;
                    // Depth buffer stores 1/z: closer = larger value
                    if (ciz > db[idx]) {
                        db[idx] = ciz;
                        float invCiz = 1.0f / ciz;
                        // Perspective-correct UV with branchless fractional wrap
                        float u = cuz * invCiz;
                        float v = cvz * invCiz;
                        if (u < 0.0f || u >= 1.0f) u = fracf(u);
                        if (v < 0.0f || v >= 1.0f) v = fracf(v);
                        int tx = (int)(u * tw);
                        int ty = (int)(v * th);
                        tx = HE3D_CLAMP(tx, 0, twm1);
                        ty = HE3D_CLAMP(ty, 0, thm1);
                        ColorA texel = tp[ty * tw + tx];
                        cbuf[idx].r = (unsigned char)HE3D_CLAMP(((int)texel.r * lr) >> 8, 0, 255);
                        cbuf[idx].g = (unsigned char)HE3D_CLAMP(((int)texel.g * lg) >> 8, 0, 255);
                        cbuf[idx].b = (unsigned char)HE3D_CLAMP(((int)texel.b * lb) >> 8, 0, 255);
                        cbuf[idx].a = 255;
                    }
                }
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
// [12] Present
// ============================================================================
static int he3d_luma(const ColorA& c)
{
    return ((int)c.r * 77 + (int)c.g * 150 + (int)c.b * 29) >> 8;
}

static unsigned char he3d_blend_u8(unsigned char a, unsigned char b)
{
    return (unsigned char)(((int)a * 141 + (int)b * 115) >> 8);
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
            out.r = he3d_blend_u8(center.r, (unsigned char)(((int)a.r + (int)b.r) >> 1));
            out.g = he3d_blend_u8(center.g, (unsigned char)(((int)a.g + (int)b.g) >> 1));
            out.b = he3d_blend_u8(center.b, (unsigned char)(((int)a.b + (int)b.b) >> 1));
            out.a = center.a;
            m_fxaaBuf[idx] = out;
        }
    }

    return true;
}

void Renderer::Present() {
    const ColorA *pixels = m_colorBuf;
    if (IsFxaaEnabled() && ApplyFxaa())
    {
        pixels = m_fxaaBuf;
    }
    HE3D::Present(m_window, m_width, m_height, pixels);
}

}
