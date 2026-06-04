#pragma once
/*
 * HE3D Engine for 3D - C++ Class Interface
 * Depends: he3d_math.h, XJ380 x3api.h
 */
#include "he3d_math.h"
#include "x3api.h"

// ============================================================================
// Override new/delete -> XAPI memory management (no libstdc++)
// Note: XJ380 size_t=unsigned long long but compiler expects unsigned long
// ============================================================================
void *operator new(unsigned long sz);
void *operator new[](unsigned long sz);
void  operator delete(void *p) noexcept;
void  operator delete[](void *p) noexcept;

// ============================================================================
// DirectionalLight
// ============================================================================
struct DirectionalLight {
    float3 direction;
    float3 color;
    float  ambient;
    DirectionalLight() : direction{0.57735f, 0.57735f, 0.57735f},
                         color{1.0f, 1.0f, 1.0f}, ambient(0.15f) {}
};

// ============================================================================
// Mesh - owns vertex + UV arrays
// ============================================================================
class Mesh {
public:
    float3 *vertices;  // new[]-allocated, 3*N for N triangles
    float2 *uvs;       // new[]-allocated, same count as vertices
    int     vertCount; // total vertex count (3 per triangle)

    Mesh() : vertices(nullptr), uvs(nullptr), vertCount(0) {}
    ~Mesh() { delete[] vertices; delete[] uvs; }

    // Disallow copy (owns memory)
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    bool Init(int vertexCount);
    bool Init(const float3 *srcVertices, const float2 *srcUvs, int vertexCount);

    static Mesh *Create(int vertexCount);
    static Mesh *Create(const float3 *srcVertices, const float2 *srcUvs,
                        int vertexCount);
    static Mesh *LoadOBJ(const char *filename);
};

// ============================================================================
// Texture - owns pixel buffer
// ============================================================================
class Texture {
public:
    int     width;
    int     height;
    float3 *pixels; // new[]-allocated, width*height
    bool    valid;

    Texture() : width(0), height(0), pixels(nullptr), valid(false) {}
    ~Texture() { delete[] pixels; }

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    float3 Sample(float u, float v) const;
    static Texture *LoadBMP(const char *filename);
};

// ============================================================================
// GameObject - mesh instance with position + orientation
// ============================================================================
class GameObject {
public:
    Mesh   *mesh;
    float3  position;
    quat    orientation;

    GameObject() : mesh(nullptr), position{0,0,0}, orientation{1,0,0,0} {}

    // Returns the forward direction in world space
    float3 Forward() const { return orientation.rotate({0, 0, 1}); }
};

// ============================================================================
// Camera
// ============================================================================
class Camera {
public:
    float3 position;
    quat   orientation;
    float  fov;

    Camera() : position{0, 0, 5}, orientation{1,0,0,0}, fov(90.0f) {}
};

// ============================================================================
// Renderer - software rasterizer with XJ380 window output
// ============================================================================
class Renderer {
public:
    DirectionalLight mainLight;

    Renderer(HDLE windowHandle, int w, int h);
    ~Renderer();

    void Clear(float3 color);
    void DrawGameObject(const GameObject& obj, const Camera& cam, float3 color);
    void DrawGameObject(const GameObject& obj, const Camera& cam, const Texture& tex);
    void Present();
    void Resize(int w, int h);

private:
    int     m_width;
    int     m_height;
    HDLE    m_window;

    XCOLOR *m_colorBuf; // m_width * m_height
    float  *m_depthBuf; // m_width * m_height

    void RasterizeSolid(const float3 *v_view, const float2 *p_screen, float3 color);
    void RasterizeTextured(const float3 *v_view, const float2 *p_screen,
                           const float2 *uvs, float intensity, const Texture& tex);

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
};
