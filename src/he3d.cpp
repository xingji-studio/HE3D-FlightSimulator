/*
 * HE3D Engine for 3D - C++ Implementation
 * OBJ loader, terrain generation, BMP texture, software rasterizer.
 * Uses new/delete -> XJ380 malloc/free. No standard library. No BridgeEngine.
 */
#include "he3d.hpp"

// new/delete -> XJ380 malloc/free
void* operator new(unsigned long sz)          { return malloc((size_t)sz); }
void* operator new[](unsigned long sz)        { return malloc((size_t)sz); }
void  operator delete(void* p) noexcept       { free(p); }
void  operator delete[](void* p) noexcept     { free(p); }

// ============================================================================
// [0] Float parser (XJ380 has no strtod/strtof)
// ============================================================================
static const char* ParseFloat(const char* s, float* out) {
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
static const char* MemGetLine(const char* buf, const char* end, char* out, int maxLen) {
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
static int CountFaceVerts(const char* p) {
    int cnt = 0;
    while (*p) {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == 0 || *p == '\n' || *p == '\r') break;
        cnt++;
        while (*p && *p != ' ' && *p != '\t') p++;
    }
    return cnt;
}

// ============================================================================
// [2] Terrain noise functions
// ============================================================================
static float Noise(int x, int z) {
    int n = x + z * 57;
    n = (n << 13) ^ n;
    return 1.0f - (float)((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f;
}
static float GetHeight(float x, float z) {
    return Noise((int)(x * 0.05f), (int)(z * 0.05f)) * 8.0f
         + Noise((int)(x * 0.2f),  (int)(z * 0.2f))  * 1.5f;
}

// ============================================================================
// [3] Mesh::CreateAirplane — Procedural airplane (no file I/O needed)
// ============================================================================
Mesh* Mesh::CreateAirplane() {
    // Simple biplane: fuselage + 2 wings + tail
    //  fuselage: elongated box (8 triangles)
    //  upper wing: flat rectangle (2 triangles)
    //  lower wing: flat rectangle (2 triangles)
    //  tail fin: triangle (1 triangle)
    const int NT = 13;
    Mesh* m = new Mesh();
    m->vertCount = NT * 3;
    m->vertices  = new float3[NT * 3];
    m->uvs       = new float2[NT * 3];
    float3* v = m->vertices;
    float2* uv = m->uvs;
    int idx = 0;

    // Fuselage: front point at (0,0,0.5), back at (0,0,-0.5), radius ~0.08
    // 4-sided tube: front face (0,0,0.5), back face (0,0,-0.5)
    float fz = 0.45f, bz = -0.45f, r = 0.07f;
    float3 ff = {0, 0, fz}, bf = {0, 0, bz};
    float3 fq[4] = {{ r, 0, fz}, { 0, r, fz}, {-r, 0, fz}, { 0,-r, fz}};
    float3 bq[4] = {{ r, 0, bz}, { 0, r, bz}, {-r, 0, bz}, { 0,-r, bz}};
    for (int i = 0; i < 4; i++) {
        int j = (i+1)%4;
        // Two triangles per quad face
        v[idx] = fq[i]; uv[idx]={0,0}; idx++;
        v[idx] = fq[j]; uv[idx]={1,0}; idx++;
        v[idx] = bq[i]; uv[idx]={0,1}; idx++;
        v[idx] = fq[j]; uv[idx]={1,0}; idx++;
        v[idx] = bq[j]; uv[idx]={1,1}; idx++;
        v[idx] = bq[i]; uv[idx]={0,1}; idx++;
    }

    // Upper wing (flat, slightly above fuselage)
    float wx = 0.35f, wy = 0.15f, wz = 0.06f;
    v[idx] = {-wx,  wy, -wz}; uv[idx]={0,0}; idx++;
    v[idx] = { wx,  wy, -wz}; uv[idx]={1,0}; idx++;
    v[idx] = {-wx,  wy,  wz}; uv[idx]={0,1}; idx++;
    v[idx] = { wx,  wy, -wz}; uv[idx]={1,0}; idx++;
    v[idx] = { wx,  wy,  wz}; uv[idx]={1,1}; idx++;
    v[idx] = {-wx,  wy,  wz}; uv[idx]={0,1}; idx++;

    // Lower wing
    v[idx] = {-wx*0.9f, -wy, -wz}; uv[idx]={0,0}; idx++;
    v[idx] = { wx*0.9f, -wy, -wz}; uv[idx]={1,0}; idx++;
    v[idx] = {-wx*0.9f, -wy,  wz}; uv[idx]={0,1}; idx++;
    v[idx] = { wx*0.9f, -wy, -wz}; uv[idx]={1,0}; idx++;
    v[idx] = { wx*0.9f, -wy,  wz}; uv[idx]={1,1}; idx++;
    v[idx] = {-wx*0.9f, -wy,  wz}; uv[idx]={0,1}; idx++;

    // Tail fin (vertical, at back)
    v[idx] = {0, 0, bz-0.05f}; uv[idx]={0,0}; idx++;
    v[idx] = {0, 0.1f, bz-0.15f}; uv[idx]={1,0}; idx++;
    v[idx] = {0, 0, bz-0.15f}; uv[idx]={0.5f,1}; idx++;

    m->vertCount = idx;
    return m;
}

// ============================================================================
// [4] Mesh::LoadOBJ — Two-pass over in-memory file data
// ============================================================================
Mesh* Mesh::LoadOBJ(const char* filename) {
    XFILE* xf = xapi_OpenFile((WSTR)filename);
    if (!xf || !xf->buffer || xf->length < 4) {
        if (xf) xapi_CloseFile(xf);
        return nullptr;
    }

    const char* buf = (const char*)xf->buffer;
    const char* end = buf + xf->length;

    // ---- Pass 1: count ----
    int vCount = 0, vtCount = 0, triCount = 0;
    const char* c = buf;
    char line[512];
    while ((c = MemGetLine(c, end, line, sizeof(line))) != nullptr) {
        char* p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (p[0] == 'v' && p[1] == ' ') vCount++;
        else if (p[0] == 'v' && p[1] == 't' && (p[2] == ' ' || p[2] == '\t')) vtCount++;
        else if (p[0] == 'f' && p[1] == ' ') {
            int fv = CountFaceVerts(p + 2);
            if (fv >= 3) triCount += fv - 2;
        }
    }
    if (vCount == 0 || triCount == 0) { xapi_CloseFile(xf); return nullptr; }

    // Allocate
    float3* all_v  = new float3[vCount];
    float2* all_vt = (vtCount > 0) ? new float2[vtCount] : nullptr;
    int vc = 0, vtc = 0;

    Mesh* mesh = new Mesh();
    int totalVerts = triCount * 3;
    mesh->vertices  = new float3[totalVerts];
    mesh->uvs       = new float2[totalVerts];
    mesh->vertCount = 0; // will increment during face processing

    // ---- Pass 2: fill ----
    c = buf;
    while ((c = MemGetLine(c, end, line, sizeof(line))) != nullptr) {
        char* p = line;
        while (*p == ' ' || *p == '\t') p++;

        if (p[0] == 'v' && p[1] == ' ') {
            float3 vtx;
            ParseFloat(p + 2, &vtx.x);
            char* py = p + 2; while (*py && *py != ' ' && *py != '\t') py++; while (*py == ' ' || *py == '\t') py++;
            ParseFloat(py, &vtx.y);
            char* pz = py;     while (*pz && *pz != ' ' && *pz != '\t') pz++; while (*pz == ' ' || *pz == '\t') pz++;
            ParseFloat(pz, &vtx.z);
            if (vc < vCount) all_v[vc++] = vtx;
        }
        else if (p[0] == 'v' && p[1] == 't' && (p[2] == ' ' || p[2] == '\t') && all_vt) {
            float2 uv;
            char* pu = p + 3; ParseFloat(pu, &uv.x);
            char* pv = pu;     while (*pv && *pv != ' ' && *pv != '\t') pv++; while (*pv == ' ' || *pv == '\t') pv++;
            float rawV; ParseFloat(pv, &rawV); uv.y = 1.0f - rawV;
            if (vtc < vtCount) all_vt[vtc++] = uv;
        }
        else if (p[0] == 'f' && p[1] == ' ') {
            struct FV { int v, vt; };
            FV fv[32]; int fc = 0;
            char* tok = p + 2;
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
                fv[fc].v = vi - 1; fv[fc].vt = ti - 1; fc++;
            }
            for (int i = 1; i < fc - 1; i++) {
                int idx = mesh->vertCount;
                mesh->vertices[idx]   = all_v[fv[0].v];
                mesh->vertices[idx+1] = all_v[fv[i].v];
                mesh->vertices[idx+2] = all_v[fv[i+1].v];
                if (all_vt) {
                    mesh->uvs[idx]   = all_vt[fv[0].vt];
                    mesh->uvs[idx+1] = all_vt[fv[i].vt];
                    mesh->uvs[idx+2] = all_vt[fv[i+1].vt];
                } else {
                    mesh->uvs[idx] = mesh->uvs[idx+1] = mesh->uvs[idx+2] = {0,0};
                }
                mesh->vertCount += 3;
            }
        }
    }

    delete[] all_v;
    delete[] all_vt;
    if (mesh->vertCount == 0) { delete mesh; xapi_CloseFile(xf); return nullptr; }
    xapi_CloseFile(xf);
    return mesh;
}

// ============================================================================
// [4] Mesh::CreatePlane — Procedural terrain
// ============================================================================
Mesh* Mesh::CreatePlane(int gridCount, float step, float worldX, float worldZ) {
    int triCount  = (gridCount - 1) * (gridCount - 1) * 2;
    int vertCount = triCount * 3;
    float half    = (float)(gridCount - 1) * step * 0.5f;

    Mesh* mesh = new Mesh();
    mesh->vertices  = new float3[vertCount];
    mesh->uvs       = new float2[vertCount];
    mesh->vertCount = 0;

    for (int z = 0; z < gridCount - 1; z++) {
        for (int x = 0; x < gridCount - 1; x++) {
            float lx0 = x * step - half, lz0 = z * step - half;
            float lx1 = (x + 1) * step - half, lz1 = (z + 1) * step - half;
            float wx0 = lx0 + worldX, wz0 = lz0 + worldZ;
            float wx1 = lx1 + worldX, wz1 = lz1 + worldZ;

            float3 v1 = {lx0, GetHeight(wx0, wz0), lz0};
            float3 v2 = {lx0, GetHeight(wx0, wz1), lz1};
            float3 v3 = {lx1, GetHeight(wx1, wz0), lz0};
            float3 v4 = {lx1, GetHeight(wx1, wz1), lz1};

            int i = mesh->vertCount;
            mesh->vertices[i] = v1; mesh->vertices[i+1] = v2; mesh->vertices[i+2] = v3;
            mesh->uvs[i] = {0,0};  mesh->uvs[i+1] = {0,1};  mesh->uvs[i+2] = {1,0};
            mesh->vertices[i+3] = v3; mesh->vertices[i+4] = v2; mesh->vertices[i+5] = v4;
            mesh->uvs[i+3] = {1,0};  mesh->uvs[i+4] = {0,1};  mesh->uvs[i+5] = {1,1};
            mesh->vertCount += 6;
        }
    }
    return mesh;
}

// ============================================================================
// [5] Texture::Sample — Nearest-neighbor with wrap
// ============================================================================
float3 Texture::Sample(float u, float v) const {
    if (!valid || !pixels) return {1.0f, 0.0f, 1.0f};
    u -= (int)u; if (u < 0) u += 1.0f;
    v -= (int)v; if (v < 0) v += 1.0f;
    int x = (int)(u * width), y = (int)(v * height);
    if (x < 0) x = 0; if (x >= width)  x = width  - 1;
    if (y < 0) y = 0; if (y >= height) y = height - 1;
    return pixels[y * width + x];
}

// ============================================================================
// [6] Texture::LoadBMP — Via XJ380 VFS
// ============================================================================
Texture* Texture::LoadBMP(const char* filename) {
    XFILE* xf = xapi_OpenFile((WSTR)filename);
    if (!xf || !xf->buffer || xf->length < 54) { if (xf) xapi_CloseFile(xf); return nullptr; }

    unsigned char* d = (unsigned char*)xf->buffer;
    unsigned int   flen = (unsigned int)xf->length;
    int            bw = 0, bh = 0, bpp = 0, compr = 0;
    unsigned int   off = 0;

    if (d[0] != 'B' || d[1] != 'M') { xapi_CloseFile(xf); return nullptr; }
    off   = d[10] | (d[11] << 8) | (d[12] << 16) | (d[13] << 24);
    if (flen < off) { xapi_CloseFile(xf); return nullptr; }
    bw    = (int)(d[18] | (d[19] << 8) | (d[20] << 16) | (d[21] << 24));
    bh    = (int)(d[22] | (d[23] << 8) | (d[24] << 16) | (d[25] << 24));
    bpp   = (int)(d[28] | (d[29] << 8));
    compr = (int)(d[30] | (d[31] << 8) | (d[32] << 16) | (d[33] << 24));
    if (bpp != 24 || compr != 0) { xapi_CloseFile(xf); return nullptr; }

    Texture* tex = new Texture();
    tex->width  = bw;
    tex->height = (bh < 0) ? -bh : bh;
    float3* pbuf = new float3[tex->width * tex->height];

    {
        int  rsize   = (bw * 3 + 3) & ~3;
        bool topDown = (bh < 0);
        unsigned char* src = d + off;
        for (int y = 0; y < tex->height; y++) {
            int sY = topDown ? y : (tex->height - 1 - y);
            unsigned char* row = src + sY * rsize;
            float3* dst = pbuf + y * tex->width;
            for (int x = 0; x < tex->width; x++) {
                dst[x] = {row[2] / 255.0f, row[1] / 255.0f, row[0] / 255.0f};
                row += 3;
            }
        }
    }
    tex->pixels = pbuf;
    tex->valid  = true;

    xapi_CloseFile(xf);
    return tex;
}

// ============================================================================
// [7] Renderer
// ============================================================================
Renderer::Renderer(HDLE windowHandle, int w, int h)
    : m_width(w), m_height(h), m_window(windowHandle)
{
    int sz = w * h;
    m_colorBuf = new XCOLOR[sz];
    m_depthBuf = new float[sz];
    if (!m_colorBuf || !m_depthBuf) { m_width = 0; m_height = 0; }
}

Renderer::~Renderer() {
    delete[] m_colorBuf;
    delete[] m_depthBuf;
}

void Renderer::Resize(int w, int h) {
    delete[] m_colorBuf;
    delete[] m_depthBuf;
    m_width  = w;
    m_height = h;
    int sz = w * h;
    m_colorBuf = new XCOLOR[sz];
    m_depthBuf = new float[sz];
}

void Renderer::Clear(float3 color) {
    XCOLOR c;
    c.Red   = (UINT8)HE3D_CLAMP((int)(color.r * 255.0f), 0, 255);
    c.Green = (UINT8)HE3D_CLAMP((int)(color.g * 255.0f), 0, 255);
    c.Blue  = (UINT8)HE3D_CLAMP((int)(color.b * 255.0f), 0, 255);

    int total = m_width * m_height;
    for (int i = 0; i < total; i++) m_colorBuf[i] = c;
    for (int i = 0; i < total; i++) m_depthBuf[i] = 10000.0f;
}

// ============================================================================
// [8] DrawGameObject (solid)
// ============================================================================
void Renderer::DrawGameObject(const GameObject& obj, const Camera& cam, float3 color) {
    if (m_width <= 0 || m_height <= 0) return;
    if (!obj.mesh || obj.mesh->vertCount < 3) return;
    if (!obj.mesh->vertices || !obj.mesh->uvs) return;

    float fovRad   = cam.fov * 0.01745329252f;
    float fovScale = 1.0f / he3d_tanf(fovRad * 0.5f);
    float aspect   = (float)m_width / (float)m_height;
    float3 lightDir = mainLight.direction.normalize();
    quat camInv    = cam.orientation.inverse();

    float3* verts = obj.mesh->vertices;
    int vc = obj.mesh->vertCount;

    for (int i = 0; i < vc; i += 3) {
        float3 vw[3], vv[3];
        float2 ps[3];
        bool cull = false;

        for (int j = 0; j < 3; j++) {
            float3 v = obj.orientation.rotate(verts[i + j]) + obj.position;
            vw[j] = v;
            v = camInv.rotate(v - cam.position);
            vv[j] = v;
            if (v.z <= 0.1f) { cull = true; break; }
            float px = (v.x / v.z) * fovScale / aspect;
            float py = (v.y / v.z) * fovScale;
            ps[j] = {(px + 1.0f) * 0.5f * m_width, (1.0f - (py + 1.0f) * 0.5f) * m_height};
        }
        if (cull) continue;

        float area = (ps[1].x - ps[0].x) * (ps[2].y - ps[0].y)
                   - (ps[1].y - ps[0].y) * (ps[2].x - ps[0].x);
        if (area <= 0.0f) continue;

        float3 e1 = vw[1] - vw[0], e2 = vw[2] - vw[0];
        float3 n = float3::cross(e1, e2).normalize();
        float diff = HE3D_MAX(0.0f, float3::dot(n, lightDir));
        float intens = mainLight.ambient + diff;

        RasterizeSolid(vv, ps, color * intens);
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

    float fovRad   = cam.fov * 0.01745329252f;
    float fovScale = 1.0f / he3d_tanf(fovRad * 0.5f);
    float aspect   = (float)m_width / (float)m_height;
    float3 lightDir = mainLight.direction.normalize();
    quat camInv    = cam.orientation.inverse();

    float3* verts = obj.mesh->vertices;
    float2* uvs   = obj.mesh->uvs;
    int vc = obj.mesh->vertCount;

    for (int i = 0; i < vc; i += 3) {
        float3 vw[3], vv[3];
        float2 tuvs[3], ps[3];
        bool cull = false;

        for (int j = 0; j < 3; j++) {
            float3 v = obj.orientation.rotate(verts[i + j]) + obj.position;
            vw[j]   = v;
            tuvs[j] = uvs[i + j];
            v = camInv.rotate(v - cam.position);
            vv[j] = v;
            if (v.z <= 0.1f) { cull = true; break; }
            float px = (v.x / v.z) * fovScale / aspect;
            float py = (v.y / v.z) * fovScale;
            ps[j] = {(px + 1.0f) * 0.5f * m_width, (1.0f - (py + 1.0f) * 0.5f) * m_height};
        }
        if (cull) continue;

        float3 ab = vv[1] - vv[0], ac = vv[2] - vv[0];
        if (float3::dot(float3::cross(ab, ac), vv[0]) >= 0) continue;

        float3 we1 = vw[1] - vw[0], we2 = vw[2] - vw[0];
        float diff = HE3D_MAX(0.0f, float3::dot(float3::cross(we1, we2).normalize(), lightDir));
        float intens = mainLight.ambient + diff;

        RasterizeTextured(vv, ps, tuvs, intens, tex);
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

    float x0 = ps[0].x, y0 = ps[0].y;
    float x1 = ps[1].x, y1 = ps[1].y;
    float x2 = ps[2].x, y2 = ps[2].y;

    float area = (x1 - x0) * (y2 - y0) - (y1 - y0) * (x2 - x0);
    if (HE3D_ABS(area) < 0.0001f) return;
    float invA = 1.0f / area;

    float dw0 = (y1 - y2) * invA;
    float dw1 = (y2 - y0) * invA;
    float iz0 = 1.0f / vv[0].z, iz1 = 1.0f / vv[1].z, iz2 = 1.0f / vv[2].z;

    XCOLOR xc;
    xc.Red   = (UINT8)HE3D_CLAMP((int)(color.r * 255.0f), 0, 255);
    xc.Green = (UINT8)HE3D_CLAMP((int)(color.g * 255.0f), 0, 255);
    xc.Blue  = (UINT8)HE3D_CLAMP((int)(color.b * 255.0f), 0, 255);

    int stride = m_width;

    for (int y = mnY; y <= mxY; y++) {
        float py = (float)y + 0.5f, px0 = (float)mnX + 0.5f;
        float w0 = ((x1 - px0) * (y2 - py) - (y1 - py) * (x2 - px0)) * invA;
        float w1 = ((x2 - px0) * (y0 - py) - (y2 - py) * (x0 - px0)) * invA;

        int maxIdx = m_width * m_height - 1;
        int rb = y * stride;
        for (int x = mnX; x <= mxX; x++) {
            float w2 = 1.0f - w0 - w1;
            if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
                float ciz = w0 * iz0 + w1 * iz1 + w2 * iz2;
                if (ciz > 0.000001f) {
                    float z = 1.0f / ciz;
                    int idx = rb + x;
                    if (idx >= 0 && idx <= maxIdx && z < m_depthBuf[idx]) {
                        m_depthBuf[idx] = z;
                        m_colorBuf[idx] = xc;
                    }
                }
            }
            w0 += dw0; w1 += dw1;
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

    float x0 = ps[0].x, y0 = ps[0].y;
    float x1 = ps[1].x, y1 = ps[1].y;
    float x2 = ps[2].x, y2 = ps[2].y;

    float area = (x1 - x0) * (y2 - y0) - (y1 - y0) * (x2 - x0);
    if (HE3D_ABS(area) < 0.0001f) return;
    float invA = 1.0f / area;

    float dw0 = (y1 - y2) * invA;
    float dw1 = (y2 - y0) * invA;
    float iz0 = 1.0f / vv[0].z, iz1 = 1.0f / vv[1].z, iz2 = 1.0f / vv[2].z;
    float2 uz0 = uvs[0] * iz0, uz1 = uvs[1] * iz1, uz2 = uvs[2] * iz2;

    float3 lc = mainLight.color * intens;
    float3* tp = tex.pixels;
    int   tw = tex.width, th = tex.height;
    int stride = m_width;

    for (int y = mnY; y <= mxY; y++) {
        float py = (float)y + 0.5f, px0 = (float)mnX + 0.5f;
        float w0 = ((x1 - px0) * (y2 - py) - (y1 - py) * (x2 - px0)) * invA;
        float w1 = ((x2 - px0) * (y0 - py) - (y2 - py) * (x0 - px0)) * invA;

        int maxIdx = m_width * m_height - 1;
        int rb = y * stride;
        for (int x = mnX; x <= mxX; x++) {
            float w2 = 1.0f - w0 - w1;
            if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
                float ciz = w0 * iz0 + w1 * iz1 + w2 * iz2;
                if (ciz > 0.000001f) {
                    float z = 1.0f / ciz;
                    int idx = rb + x;
                    if (idx >= 0 && idx <= maxIdx && z < m_depthBuf[idx]) {
                        m_depthBuf[idx] = z;
                        float u = (w0 * uz0.x + w1 * uz1.x + w2 * uz2.x) * z;
                        float v = (w0 * uz0.y + w1 * uz1.y + w2 * uz2.y) * z;
                        u -= (int)u; if (u < 0) u += 1.0f;
                        v -= (int)v; if (v < 0) v += 1.0f;
                        int tx = (int)(u * tw), ty = (int)(v * th);
                        if (tx < 0) tx = 0; if (tx >= tw) tx = tw - 1;
                        if (ty < 0) ty = 0; if (ty >= th) ty = th - 1;
                        float3 tl = tp[ty * tw + tx] * lc;
                        m_colorBuf[idx].Red   = (UINT8)HE3D_CLAMP((int)(tl.r * 255.0f), 0, 255);
                        m_colorBuf[idx].Green = (UINT8)HE3D_CLAMP((int)(tl.g * 255.0f), 0, 255);
                        m_colorBuf[idx].Blue  = (UINT8)HE3D_CLAMP((int)(tl.b * 255.0f), 0, 255);
                    }
                }
            }
            w0 += dw0; w1 += dw1;
        }
    }
}

// ============================================================================
// [12] Present
// ============================================================================
void Renderer::Present() {
    xapi_WriteBuffer(m_window, 0, 0, m_width, m_height, m_colorBuf);
    xapi_RefreshWindow(m_window);
}
