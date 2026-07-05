#pragma once
/*
 * HE3D Engine for 3D - C++ class interface.
 * HE3D 三维引擎 C++ 类接口。
 *
 * Depends on he3d_math.h and he3d_platform.hpp.
 * 依赖 he3d_math.h 和 he3d_platform.hpp。
 */
#include "he3d_math.h"
#include "he3d_platform.hpp"

// ============================================================================
// Global new/delete must stay at global scope and forward to HE3D::Alloc/Free.
// 全局 new/delete 必须位于全局命名空间，并转发到 HE3D::Alloc/Free。
// ============================================================================
void *operator new(unsigned long size);
void *operator new[](unsigned long size);
void  operator delete(void *ptr) noexcept;
void  operator delete[](void *ptr) noexcept;

namespace HE3D {

// Basic idea for users:
// Mesh is the shape, GameObject is the object, Camera is the view, Renderer draws the frame.
// 给使用者的基本概念：
// Mesh 是形状，GameObject 是物体，Camera 是视角，Renderer 绘制画面。

// ============================================================================
// DirectionalLight / 方向光
// ============================================================================
struct DirectionalLight {
    float3 direction = {0, -1, 1}; // light ray direction, not light position / 光线方向，不是光源位置
    color3 color     = {1, 1, 1};  // light color and strength / 光的颜色和强度
    float  ambient   = 0.15f;      // base brightness added to visible surfaces / 给可见表面增加的基础亮度
};

// ============================================================================
// Mesh owns vertex, UV, and triangle-normal arrays.
// Mesh 拥有顶点、UV 和三角形法线数组。
// ============================================================================
class Mesh {
public:
    float3 *vertices;   // new[] allocated; 3 vertices per triangle / new[] 分配；每个三角形 3 个顶点
    float2 *uvs;        // new[] allocated; same capacity as vertices / new[] 分配；容量与 vertices 相同
    float3 *triNormals; // one normal per triangle / 每个三角形一条法线
    int32_t vertCount;  // active vertex count / 当前有效顶点数
    int32_t capacity;   // allocated vertex capacity / 已分配顶点容量

    Mesh() : vertices(nullptr), uvs(nullptr), triNormals(nullptr), vertCount(0), capacity(0) {}
    ~Mesh() { delete[] vertices; delete[] uvs; delete[] triNormals; }

    // Copying is disabled because Mesh owns raw arrays.
    // Mesh 拥有原始数组，因此禁止拷贝。
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    bool Init(int32_t vertexCount);
    bool Init(const float3 *srcVertices, const float2 *srcUvs, int32_t vertexCount);
    void RecalculateTriangleNormals();

    static Mesh *Create(int32_t vertexCount);
    static Mesh *Create(const float3 *srcVertices, const float2 *srcUvs,
                        int32_t vertexCount);
    static Mesh *CreateTriangle(float width = 1.0f, float height = 1.0f);
    static Mesh *CreatePlane(float width = 1.0f, float depth = 1.0f);
    static Mesh *CreateCube(float width = 1.0f, float height = 1.0f, float depth = 1.0f);
    static Mesh *CreateSphere(float radius = 0.5f, int32_t segments = 16, int32_t rings = 8);
    static Mesh *LoadOBJ(const char *filename);
};

// ============================================================================
// Texture owns an RGBA pixel buffer.
// Texture 拥有 RGBA 像素缓冲区。
// ============================================================================
class Texture {
public:
    int32_t width;
    int32_t height;
    ColorA *pixels; // new[] allocated; width * height pixels / new[] 分配；共 width * height 个像素
    bool    valid;

    Texture() : width(0), height(0), pixels(nullptr), valid(false) {}
    ~Texture() { delete[] pixels; }

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    color3 Sample(float u, float v) const;
    static Texture *LoadBMP(const char *filename);
    static Texture *LoadPNG(const char *filename);
    static Texture *LoadJPG(const char *filename);
    static Texture *LoadImage(const char *filename);
};

// ============================================================================
// GameObject is a mesh instance with position and orientation.
// GameObject 是带位置和方向的网格实例。
// ============================================================================
class GameObject {
public:
    Mesh   *mesh;        // shape to draw; GameObject does not own it / 要绘制的形状；GameObject 不负责释放它
    float3  position;    // world-space position / 世界空间位置
    quat    orientation; // world-space rotation / 世界空间旋转

    GameObject() : mesh(nullptr), position{0,0,0}, orientation{1,0,0,0} {}

    // Return local +Z transformed to world space.
    // 返回局部 +Z 方向转换到世界空间后的方向。
    float3 Forward() const { return orientation.rotate({0, 0, 1}); }
};

// ============================================================================
// CollisionBox is an oriented box bound to an optional GameObject.
// CollisionBox 是可绑定到 GameObject 的旋转碰撞箱。
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
    bool Contact(const CollisionBox& other, struct PhysicsContact *outContact = nullptr) const;
    AABB  WorldAABB() const;

private:
    float3 AxisX() const;
    float3 AxisY() const;
    float3 AxisZ() const;
};

// ============================================================================
// PhysicsContact stores a collision contact result.
// PhysicsContact 保存一次碰撞接触结果。
// ============================================================================
struct PhysicsContact {
    bool   hit;
    float  penetration;
    float3 normal;
    float3 point;

    PhysicsContact() : hit(false), penetration(0.0f), normal{0,1,0}, point{0,0,0} {}
};

// ============================================================================
// PhysicsMaterial stores surface response parameters.
// PhysicsMaterial 保存表面响应参数。
// ============================================================================
struct PhysicsMaterial {
    float restitution;
    float friction;
    float linearDamping;
    float angularDamping;
    float drag;

    PhysicsMaterial()
        : restitution(0.0f), friction(0.5f), linearDamping(0.0f),
          angularDamping(0.0f), drag(0.0f) {}
};

typedef float (*HeightSampleCallback)(float x, float z, void *user);

class HeightFieldCollider {
public:
    struct Tile {
        bool  active;
        float originX;
        float originZ;
        AABB  bounds;
        AABB *cellBounds;
        int32_t cellCount;
        int32_t cellCapacity;

        Tile();
        ~Tile();
        bool EnsureCapacity(int32_t count);

    private:
        Tile(const Tile&) = delete;
        Tile& operator=(const Tile&) = delete;
    };

    HeightSampleCallback sampleHeight;
    void *sampleUser;
    int32_t gridCount;
    float cellSize;

    HeightFieldCollider();
    HeightFieldCollider(HeightSampleCallback callback, void *user,
                        int32_t gridCountValue, float cellSizeValue);

    void Configure(HeightSampleCallback callback, void *user,
                   int32_t gridCountValue, float cellSizeValue);
    bool BuildTile(Tile& tile, float originX, float originZ) const;
    bool ContactBox(const CollisionBox& box, float sweep,
                    PhysicsContact *outContact = nullptr) const;
    bool ContactBox(const CollisionBox& box, const Tile *tiles, int32_t tileCount, float sweep,
                    PhysicsContact *outContact = nullptr) const;
    float3 NormalAt(float x, float z) const;
    bool IsValid() const;

private:
    float Sample(float x, float z) const;
    bool TriangleContact(const Tile& tile, int32_t cellIndex, float x, float z,
                         float *outY, float3 *outNormal) const;
};

// ============================================================================
// CollisionBoxSet stores multiple fitted boxes for finer contact tests.
// CollisionBoxSet 保存多个拟合碰撞箱，用于更细的接触检测。
// ============================================================================
class CollisionBoxSet {
public:
    GameObject   *object;
    CollisionBox *boxes;
    int32_t       boxCount;

    CollisionBoxSet();
    explicit CollisionBoxSet(GameObject *gameObject);
    ~CollisionBoxSet();

    void BindGameObject(GameObject *gameObject);
    bool FitMeshGrid(int32_t xParts, int32_t yParts, int32_t zParts);
    bool FitMeshGrid(const Mesh& mesh, int32_t xParts, int32_t yParts, int32_t zParts);
    bool IsValid() const;

    bool Contains(float3 point) const;
    bool Intersects(const CollisionBox& other) const;
    RayHit Raycast(const Ray& ray) const;

private:
    int32_t m_boxCapacity;

    bool EnsureCapacity(int32_t count);

    CollisionBoxSet(const CollisionBoxSet&) = delete;
    CollisionBoxSet& operator=(const CollisionBoxSet&) = delete;
};

bool RaycastCollisionBox(const Ray& ray, const CollisionBox& box, RayHit *outHit = nullptr);
bool RaycastCollisionBoxes(const Ray& ray, const CollisionBox *boxes, int32_t boxCount,
                           RayHit *outHit = nullptr, int32_t *outIndex = nullptr);
bool RaycastCollisionBoxSet(const Ray& ray, const CollisionBoxSet& boxSet,
                            RayHit *outHit = nullptr, int32_t *outIndex = nullptr);
bool RaycastMeshTriangles(const Ray& ray, const GameObject& object,
                          RayHit *outHit = nullptr, int32_t *outTriangleIndex = nullptr);

// ============================================================================
// PhysicsBody is a lightweight motion integrator bound to a GameObject.
// PhysicsBody 是绑定到 GameObject 的轻量运动积分器。
// ============================================================================
class PhysicsBody {
public:
    GameObject *object;
    float3      velocity;
    float3      angularVelocity;
    float3      force;
    float3      torque;
    float3      gravity;
    float       mass;
    float       inverseMass;
    float       inertia;
    float       inverseInertia;
    float       linearDamping;
    float       angularDamping;
    float       restitution;
    float       friction;
    float       drag;
    PhysicsMaterial material;

    PhysicsBody();
    explicit PhysicsBody(GameObject *gameObject);

    void BindGameObject(GameObject *gameObject);
    void SetEnabled(bool enabled);
    bool IsEnabled() const;
    void SetMass(float value);
    void SetInertia(float value);
    void SetMaterial(const PhysicsMaterial& value);

    void SetVelocity(float3 value);
    void SetAngularVelocity(float3 value);
    void AddForce(float3 value);
    void AddImpulse(float3 impulse);
    void AddAngularImpulse(float3 impulse);
    void AddTorque(float3 value);
    void ClearForces();
    void ClearTorques();
    void Step(float deltaTime);
    bool StepWithCollisions(float deltaTime, CollisionBox& selfBox,
                            const CollisionBox *obstacles, int32_t obstacleCount,
                            int32_t substeps = 4, PhysicsContact *outContact = nullptr);
    bool StepWithCollisions(float deltaTime, CollisionBox& selfBox,
                            const CollisionBox *obstacles, const AABB *obstacleBounds,
                            int32_t obstacleCount, int32_t substeps = 4,
                            PhysicsContact *outContact = nullptr);
    bool ResolveHeightField(CollisionBox& selfBox, const HeightFieldCollider& heightField,
                            float sweep = 0.0f, PhysicsContact *outContact = nullptr);
    bool ResolveHeightField(CollisionBox& selfBox, const HeightFieldCollider& heightField,
                            const HeightFieldCollider::Tile *tiles, int32_t tileCount,
                            float sweep = 0.0f, PhysicsContact *outContact = nullptr);

private:
    bool m_enabled;
};

// ============================================================================
// Camera / 相机
// ============================================================================
class Camera {
public:
    float3 position;          // camera position in world space / 相机在世界空间的位置
    quat   orientation;       // camera rotation; default looks along +Z / 相机旋转；默认看向 +Z
    float  fov       = 90.0f; // vertical field of view in degrees / 垂直视场角，单位是度

    Camera() : position{0, 0, 5}, orientation{1,0,0,0} {}
};

// ============================================================================
// Renderer is a software rasterizer that presents through a platform window.
// Renderer 是软件光栅化器，通过平台窗口显示结果。
// ============================================================================
class Renderer {
public:
    DirectionalLight mainLight;

    // window is the target window; w/h are the render size.
    // window 是目标窗口；w/h 是渲染尺寸。
    Renderer(Window *window, int32_t w, int32_t h);
    ~Renderer();

    // Clear starts a new frame. Call it once before drawing objects.
    // Clear 开始新的一帧。绘制物体前调用一次。
    void Clear(color3 color);

    // Draw one object with a solid color or a texture.
    // 用纯色或纹理绘制一个物体。
    void DrawGameObject(const GameObject& obj, const Camera& cam, color3 color);
    void DrawGameObject(const GameObject& obj, const Camera& cam, const Texture& tex);

    // Present shows the finished frame.
    // Present 显示完成的画面。
    void Present();
    void Resize(int32_t w, int32_t h);
    void SetMainLight(const DirectionalLight& light) { mainLight = light; }

private:
    int32_t m_width;
    int32_t m_height;
    int32_t m_outputWidth;
    int32_t m_outputHeight;
    uint32_t m_ssaaScale;
    Window *m_window;

    ColorA *m_colorBuf; // color buffer, m_width * m_height / 颜色缓冲区，大小为 m_width * m_height
    ColorA *m_fxaaBuf;  // FXAA buffer, allocated lazily / FXAA 缓冲区，按需分配
    ColorA *m_taaBuf;   // TAA output buffer, allocated lazily / TAA 输出缓冲区，按需分配
    ColorA *m_taaHistory; // TAA previous color history / TAA 上一帧颜色历史
    float  *m_taaDepth; // TAA previous 1/z depth history / TAA 上一帧 1/z 深度历史
    ColorA *m_ssaaBuf;  // SSAA resolved output buffer / SSAA 降采样输出缓冲区
    float  *m_depthBuf; // depth buffer, m_width * m_height / 深度缓冲区，大小为 m_width * m_height
    bool    m_taaValid;
    uint32_t m_taaFrameIndex;

    bool ApplyFxaa();
    bool ApplyTaa(const ColorA *source);
    const ColorA *ApplySsaa(const ColorA *source);
    float2 CurrentTaaJitter() const;
    void RasterizeSolid(const float3 *v_view, const float2 *p_screen, color3 color);
    void RasterizeTextured(const float3 *v_view, const float2 *p_screen,
                           const float2 *uvs, float intensity, const Texture& tex);

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
};

}
