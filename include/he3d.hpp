#pragma once
/*
 * HE3D Engine for 3D - C++ Class Interface
 * Depends: he3d_math.h, he3d_platform.hpp
 */
#include "he3d_math.h"
#include "he3d_platform.hpp"

// ============================================================================
// Global new/delete must stay at global scope; they forward to HE3D::Alloc/Free.
// ============================================================================
void *operator new(unsigned long size);
void *operator new[](unsigned long size);
void  operator delete(void *ptr) noexcept;
void  operator delete[](void *ptr) noexcept;

namespace HE3D {

// ============================================================================
// DirectionalLight
// ============================================================================
struct DirectionalLight {
    float3 direction = {0, -1, 1};
    color3 color     = {1, 1, 1};
    float  ambient   = 0.15f;
};

// ============================================================================
// Mesh - owns vertex + UV arrays
// ============================================================================
class Mesh {
public:
    float3 *vertices;  // new[]-allocated, 3*N for N triangles
    float2 *uvs;       // new[]-allocated, same capacity as vertices
    float3 *triNormals; // one normal per triangle
    int     vertCount; // active vertex count, 3 per triangle
    int     capacity;  // allocated vertex count

    Mesh() : vertices(nullptr), uvs(nullptr), triNormals(nullptr), vertCount(0), capacity(0) {}
    ~Mesh() { delete[] vertices; delete[] uvs; delete[] triNormals; }

    // Disallow copy (owns memory)
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    bool Init(int vertexCount);
    bool Init(const float3 *srcVertices, const float2 *srcUvs, int vertexCount);
    void RecalculateTriangleNormals();

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
    ColorA *pixels; // new[]-allocated, width*height
    bool    valid;

    Texture() : width(0), height(0), pixels(nullptr), valid(false) {}
    ~Texture() { delete[] pixels; }

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    color3 Sample(float u, float v) const;
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
// CollisionBox - oriented box bound to an optional GameObject
// ============================================================================
class CollisionBox {
public:
    GameObject *object;
    float3      centerOffset;
    float3      halfExtents;

    CollisionBox();
    explicit CollisionBox(GameObject *gameObject);

    void BindGameObject(GameObject *gameObject);
    bool FitMesh();
    bool FitMesh(const Mesh& mesh);

    float3 Center() const;
    quat   Orientation() const;
    bool   IsValid() const;

    bool Contains(float3 point) const;
    bool Intersects(const CollisionBox& other) const;
    RayHit Raycast(const Ray& ray) const;

private:
    float3 AxisX() const;
    float3 AxisY() const;
    float3 AxisZ() const;
};

// ============================================================================
// PhysicsBody - lightweight motion integrator bound to a GameObject
// ============================================================================
class PhysicsBody {
public:
    GameObject *object;
    float3      velocity;
    float3      acceleration;
    float       linearDamping;

    PhysicsBody();
    explicit PhysicsBody(GameObject *gameObject);

    void BindGameObject(GameObject *gameObject);
    void SetEnabled(bool enabled);
    bool IsEnabled() const;

    void SetVelocity(float3 value);
    void AddVelocity(float3 delta);
    void AddAcceleration(float3 delta);
    void AddImpulse(float3 impulse);
    void AddDisplacement(float3 delta);
    void ClearForces();
    void Step(float deltaTime);
    bool StepWithCollisions(float deltaTime, CollisionBox& selfBox,
                            const CollisionBox *obstacles, int obstacleCount,
                            int substeps = 4);

private:
    bool m_enabled;
};

// ============================================================================
// Camera
// ============================================================================
class Camera {
public:
    float3 position;
    quat   orientation;
    float  fov       = 90.0f;
    float  targetFov = 90.0f;
    float  zoomSpeed = 0.2f;

    Camera() : position{0, 0, 5}, orientation{1,0,0,0} {}

    void Update(float deltaTime) {
        fov += (targetFov - fov) * zoomSpeed;
    }
};

// ============================================================================
// Renderer - software rasterizer with platform window output
// ============================================================================
class Renderer {
public:
    DirectionalLight mainLight;

    Renderer(Window *window, int w, int h);
    ~Renderer();

    void Clear(color3 color);
    void DrawGameObject(const GameObject& obj, const Camera& cam, color3 color);
    void DrawGameObject(const GameObject& obj, const Camera& cam, const Texture& tex);
    void Present();
    void Resize(int w, int h);
    void SetMainLight(const DirectionalLight& light) { mainLight = light; }

private:
    int     m_width;
    int     m_height;
    Window *m_window;

    ColorA *m_colorBuf; // m_width * m_height
    ColorA *m_fxaaBuf;  // m_width * m_height, allocated lazily
    float  *m_depthBuf; // m_width * m_height

    bool ApplyFxaa();
    void RasterizeSolid(const float3 *v_view, const float2 *p_screen, color3 color);
    void RasterizeTextured(const float3 *v_view, const float2 *p_screen,
                           const float2 *uvs, float intensity, const Texture& tex);

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
};

}
