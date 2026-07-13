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

namespace HE3D
{

struct PhysicsSceneState;
namespace Detail
{
class ColliderAccess;
}

// Basic idea for users:
// Mesh is the shape, GameObject is the object, Camera is the view, Renderer
// draws the frame. 给使用者的基本概念： Mesh 是形状，GameObject 是物体，Camera
// 是视角，Renderer 绘制画面。

// ============================================================================
// DirectionalLight / 方向光
// ============================================================================
struct DirectionalLight {
   float3 direction = {0, -1,
                       1};       // light ray direction, not light position / 光线方向，不是光源位置
   color3 color     = {1, 1, 1}; // light color and strength / 光的颜色和强度
   float  ambient   = 0.15f;     // base brightness added to visible surfaces /
                                 // 给可见表面增加的基础亮度
};

// ============================================================================
// Mesh owns vertex, UV, and triangle-normal arrays.
// Mesh 拥有顶点、UV 和三角形法线数组。
// ============================================================================
class Mesh
{
 public:
   float3 *vertices;   // new[] allocated; 3 vertices per triangle / new[]
                       // 分配；每个三角形 3 个顶点
   float2 *uvs;        // new[] allocated; same capacity as vertices / new[]
                       // 分配；容量与 vertices 相同
   float3 *triNormals; // one normal per triangle / 每个三角形一条法线
   int32_t vertCount;  // active vertex count / 当前有效顶点数
   int32_t capacity;   // allocated vertex capacity / 已分配顶点容量

   Mesh() : vertices(nullptr), uvs(nullptr), triNormals(nullptr), vertCount(0), capacity(0) {}
   ~Mesh()
   {
      delete[] vertices;
      delete[] uvs;
      delete[] triNormals;
   }

   // Copying is disabled because Mesh owns raw arrays.
   // Mesh 拥有原始数组，因此禁止拷贝。
   Mesh(const Mesh &)            = delete;
   Mesh &operator=(const Mesh &) = delete;

   bool Init(int32_t vertexCount);
   bool Init(const float3 *srcVertices, const float2 *srcUvs, int32_t vertexCount);
   void RecalculateTriangleNormals();

   static Mesh *Create(int32_t vertexCount);
   static Mesh *Create(const float3 *srcVertices, const float2 *srcUvs, int32_t vertexCount);
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
class Texture
{
 public:
   int32_t width;
   int32_t height;
   ColorA *pixels; // new[] allocated; width * height pixels / new[] 分配；共
                   // width * height 个像素
   bool valid;

   Texture() : width(0), height(0), pixels(nullptr), valid(false) {}
   ~Texture() { delete[] pixels; }

   Texture(const Texture &)            = delete;
   Texture &operator=(const Texture &) = delete;

   color3          Sample(float u, float v) const;
   static Texture *LoadBMP(const char *filename);
   static Texture *LoadPNG(const char *filename);
   static Texture *LoadJPG(const char *filename);
   static Texture *LoadImage(const char *filename);
};

// ============================================================================
// GameObject is a mesh instance with position and orientation.
// GameObject 是带位置和方向的网格实例。
// ============================================================================
class GameObject
{
 public:
   Mesh *mesh;         // shape to draw; GameObject does not own it /
                       // 要绘制的形状；GameObject 不负责释放它
   float3 position;    // world-space position / 世界空间位置
   quat   orientation; // world-space rotation / 世界空间旋转

   GameObject() : mesh(nullptr), position{0, 0, 0}, orientation{1, 0, 0, 0} {}

   // Return local +Z transformed to world space.
   // 返回局部 +Z 方向转换到世界空间后的方向。
   float3 Forward() const { return orientation.rotate({0, 0, 1}); }
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

   PhysicsContact() : hit(false), penetration(0.0f), normal{0, 1, 0}, point{0, 0, 0} {}
};

/// HeightSampleCallback returns terrain height at world x/z for a caller-owned
/// user pointer. HeightSampleCallback 通过调用者提供的 user 指针返回世界 x/z
/// 位置的地形高度。
typedef float (*HeightSampleCallback)(float x, float z, void *user);

/// HeightFieldCollider builds cached heightfield tile bounds and samples
/// height/normal queries. HeightFieldCollider 构建高度场 tile
/// 边界缓存，并提供高度/法线查询。
class HeightFieldCollider
{
 public:
   /// Tile stores cached bounds for one heightfield tile; it owns cellBounds.
   /// Tile 保存一个高度场 tile 的边界缓存，并拥有 cellBounds。
   struct Tile {
      bool    active;
      float   originX;
      float   originZ;
      AABB    bounds;
      AABB   *cellBounds;
      int32_t cellCount;
      int32_t cellCapacity;

      Tile();
      ~Tile();
      bool EnsureCapacity(int32_t count);

    private:
      Tile(const Tile &)            = delete;
      Tile &operator=(const Tile &) = delete;
   };

   /// Height sampling callback used by this heightfield.
   /// 当前高度场使用的高度采样回调。
   HeightSampleCallback sampleHeight;
   /// User data passed to sampleHeight.
   /// 传给 sampleHeight 的用户数据。
   void *sampleUser;
   /// Number of grid points along one tile edge.
   /// 一个 tile 边上网格点数量。
   int32_t gridCount;
   /// Distance between neighboring height samples.
   /// 相邻高度采样点距离。
   float cellSize;

   /// Construct an invalid heightfield collider.
   /// 构造一个无效高度场碰撞体。
   HeightFieldCollider();
   /// Construct a configured heightfield collider.
   /// 构造一个已配置的高度场碰撞体。
   HeightFieldCollider(HeightSampleCallback callback, void *user, int32_t gridCountValue,
                       float cellSizeValue);

   /// Configure the sampling callback, grid count, and cell size.
   /// 配置采样回调、网格数量和单元尺寸。
   void Configure(HeightSampleCallback callback, void *user, int32_t gridCountValue,
                  float cellSizeValue);
   /// Build or refresh cached bounds for one tile at world originX/originZ.
   /// 为世界 originX/originZ 处的 tile 构建或刷新边界缓存。
   bool BuildTile(Tile &tile, float originX, float originZ) const;
   /// Return an approximate terrain normal at world x/z.
   /// 返回世界 x/z 位置的近似地形法线。
   float3 NormalAt(float x, float z) const;
   /// Return true when callback, grid count, and cell size are usable.
   /// 当回调、网格数量和单元尺寸可用时返回 true。
   bool IsValid() const;

 private:
   float Sample(float x, float z) const;
};

/// Raycast triangles in a GameObject mesh and optionally return hit details and
/// triangle index. 对 GameObject mesh
/// 的三角形做射线检测，并可返回命中信息和三角形索引。
bool RaycastMeshTriangles(const Ray &ray, const GameObject &object, RayHit *outHit = nullptr,
                          int32_t *outTriangleIndex = nullptr);

// ============================================================================
// BoxCollider stores a dynamic oriented box shape.
// BoxCollider 保存动态有向盒形状。
// ============================================================================
class BoxCollider
{
 public:
   BoxCollider();
   BoxCollider(float width, float height, float depth);

   bool SetSize(float width, float height, float depth);
   bool IsValid() const;
   AABB LocalAABB() const;

 private:
   friend class PhysicsScene;
   friend class Detail::ColliderAccess;

   float3 m_halfExtents;
   bool   m_valid;

   BoxCollider(const BoxCollider &)            = delete;
   BoxCollider &operator=(const BoxCollider &) = delete;
};

// ============================================================================
// ConvexCollider stores a validated local convex mesh cache.
// ConvexCollider 保存经过校验的局部凸网格缓存。
// ============================================================================
class ConvexCollider
{
 public:
   ConvexCollider();
   explicit ConvexCollider(const Mesh *mesh);
   ~ConvexCollider();

   bool BuildFromMesh(const Mesh *mesh);
   bool IsValid() const;
   AABB LocalAABB() const;

 private:
   friend class PhysicsScene;
   friend class Detail::ColliderAccess;

   float3 *m_vertices;
   int32_t m_vertexCount;
   float3 *m_faceAxes;
   int32_t m_faceAxisCount;
   float3 *m_edgeAxes;
   int32_t m_edgeAxisCount;
   AABB    m_localBounds;
   bool    m_valid;

   void Clear();

   ConvexCollider(const ConvexCollider &)            = delete;
   ConvexCollider &operator=(const ConvexCollider &) = delete;
};

// ============================================================================
// StaticMeshCollider stores immovable triangle scene geometry.
// StaticMeshCollider 保存不可移动的三角形场景几何体。
// ============================================================================
class StaticMeshCollider
{
 public:
   StaticMeshCollider();
   explicit StaticMeshCollider(const Mesh *mesh);

   bool BuildFromMesh(const Mesh *mesh);
   bool IsValid() const;
   AABB LocalAABB() const;

 private:
   friend class PhysicsScene;
   friend class Detail::ColliderAccess;

   const Mesh *m_mesh;
   AABB        m_localBounds;
   bool        m_valid;

   StaticMeshCollider(const StaticMeshCollider &)            = delete;
   StaticMeshCollider &operator=(const StaticMeshCollider &) = delete;
};

// ============================================================================
// PhysicsBody stores physical state for PhysicsScene.
// PhysicsBody 保存交给 PhysicsScene 求解的物理状态。
// ============================================================================
class PhysicsBody
{
 public:
   /// Bound object moved by PhysicsScene; PhysicsBody does not own it.
   /// 由 PhysicsScene 推进的绑定物体；PhysicsBody 不拥有它。
   GameObject *object;
   /// Linear velocity in world units per second; affects object position.
   /// 世界单位每秒的线速度；影响物体位置。
   float3 velocity;
   /// Angular velocity in radians per second; affects object orientation.
   /// 弧度每秒的角速度；影响物体朝向。
   float3 angularVelocity;
   /// Accumulated force applied during the next PhysicsScene step.
   /// 下一次 PhysicsScene 推进时使用的累计力。
   float3 force;
   /// Accumulated torque applied during the next PhysicsScene step.
   /// 下一次 PhysicsScene 推进时使用的累计力矩。
   float3 torque;
   /// Per-body gravity acceleration.
   /// 此刚体使用的重力加速度。
   float3 gravity;
   /// Mass value; use SetMass so inverseMass stays synchronized.
   /// 质量；请用 SetMass 保持 inverseMass 同步。
   float mass;
   /// Cached inverse mass used by the solver.
   /// 求解器使用的质量倒数缓存。
   float inverseMass;
   /// Rotational inertia value; AddBody estimates it from collider bounds unless
   /// SetInertia overrides it. 转动惯量；AddBody 会按 collider 边界估算，除非
   /// SetInertia 手动覆盖。
   float inertia;
   /// Cached inverse inertia used by the solver.
   /// 求解器使用的转动惯量倒数缓存。
   float inverseInertia;
   /// Contact bounce coefficient; 0 means no bounce.
   /// 接触反弹系数；0 表示不反弹。
   float restitution;
   /// Contact friction coefficient used only while resolving collisions.
   /// 接触摩擦系数，只在碰撞解算时使用。
   float friction;
   /// Per-body damping that reduces both velocity and angularVelocity.
   /// 单体阻尼，同时衰减线速度和角速度。
   float damping;

   /// Construct a disabled body with no bound object.
   /// 构造未绑定物体且默认禁用的刚体。
   PhysicsBody();
   /// Construct a disabled body bound to gameObject.
   /// 构造绑定到 gameObject 且默认禁用的刚体。
   explicit PhysicsBody(GameObject *gameObject);

   /// Bind this body to a different GameObject.
   /// 将此刚体绑定到另一个 GameObject。
   void BindGameObject(GameObject *gameObject);
   /// Enable or disable integration and impulse response for this body.
   /// 启用或禁用此刚体的积分和冲量响应。
   void SetEnabled(bool enabled);
   /// Return whether this body participates as a dynamic body.
   /// 返回此刚体是否作为动态刚体参与求解。
   bool IsEnabled() const;
   /// Return whether inertia was manually configured.
   /// 返回惯量是否由调用方手动配置。
   bool UsesManualInertia() const;
   /// Set mass and update inverseMass; AddBody will estimate inertia from
   /// collider bounds. 设置质量并更新 inverseMass；AddBody 会按 collider
   /// 边界估算惯量。
   void SetMass(float value);
   /// Set rotational inertia manually and update inverseInertia; non-positive
   /// inertia disables angular impulse. 手动设置转动惯量并更新
   /// inverseInertia；非正惯量会禁用角冲量。
   void SetInertia(float value);

   /// Set linear velocity directly.
   /// 直接设置线速度。
   void SetVelocity(float3 value);
   /// Set angular velocity directly.
   /// 直接设置角速度。
   void SetAngularVelocity(float3 value);
   /// Accumulate force for the next PhysicsScene step.
   /// 为下一次 PhysicsScene 推进累计力。
   void AddForce(float3 value);
   /// Apply an immediate linear impulse.
   /// 立即施加线性冲量。
   void AddImpulse(float3 impulse);
   /// Apply an immediate angular impulse.
   /// 立即施加角冲量。
   void AddAngularImpulse(float3 impulse);
   /// Accumulate torque for the next PhysicsScene step.
   /// 为下一次 PhysicsScene 推进累计力矩。
   void AddTorque(float3 value);
   /// Clear accumulated force.
   /// 清空累计力。
   void ClearForces();
   /// Clear accumulated torque.
   /// 清空累计力矩。
   void ClearTorques();

 private:
   friend class PhysicsScene;

   bool m_enabled;
   bool m_inertiaManual;
};

// ============================================================================
// PhysicsScene steps bodies and resolves collider contacts together.
// PhysicsScene 统一推进刚体，并解算 collider 接触。
// ============================================================================
class PhysicsScene
{
 public:
   /// Construct a physics scene with storage for capacity dynamic/static
   /// entries. 构造可容纳 capacity 个动态/静态条目的物理场景。
   explicit PhysicsScene(int32_t capacity = 32);
   ~PhysicsScene();

   /// Scene-wide medium drag that damps velocity and angularVelocity of all
   /// enabled bodies. 场景介质阻力，会衰减所有启用刚体的线速度和角速度。
   float drag;

   /// Add a dynamic box body to this scene.
   /// 将动态盒体刚体加入此场景。
   bool AddBody(GameObject *object, PhysicsBody *body, BoxCollider *collider);
   /// Add a dynamic convex body to this scene.
   /// 将动态凸体刚体加入此场景。
   bool AddBody(GameObject *object, PhysicsBody *body, ConvexCollider *collider);
   /// Add immovable static mesh geometry to this scene.
   /// 将不可移动静态网格加入此场景。
   bool AddStatic(GameObject *object, StaticMeshCollider *collider);
   /// Remove all dynamic bodies and static colliders from this scene.
   /// 移除此场景中的所有动态刚体和静态碰撞体。
   void Clear();
   /// Integrate bodies, resolve mesh contacts, and clear accumulated
   /// force/torque. 推进刚体、解算 mesh 接触，并清空累计力/力矩。
   void Step(float deltaTime, int32_t iterations = 4);

 private:
   PhysicsSceneState *m_impl;

   PhysicsScene(const PhysicsScene &)            = delete;
   PhysicsScene &operator=(const PhysicsScene &) = delete;
};

// ============================================================================
// Camera / 相机
// ============================================================================
class Camera
{
 public:
   float3 position;    // camera position in world space / 相机在世界空间的位置
   quat   orientation; // camera rotation; default looks along +Z /
                       // 相机旋转；默认看向 +Z
   float fov = 90.0f;  // vertical field of view in degrees / 垂直视场角，单位是度

   Camera() : position{0, 0, 5}, orientation{1, 0, 0, 0} {}
};

// ============================================================================
// Renderer is a software rasterizer that presents through a platform window.
// Renderer 是软件光栅化器，通过平台窗口显示结果。
// ============================================================================
class Renderer
{
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
   void DrawGameObject(const GameObject &obj, const Camera &cam, color3 color);
   void DrawGameObject(const GameObject &obj, const Camera &cam, const Texture &tex);

   // Present shows the finished frame.
   // Present 显示完成的画面。
   void Present();
   void Resize(int32_t w, int32_t h);
   void SetMainLight(const DirectionalLight &light) { mainLight = light; }

 private:
   int32_t  m_width;
   int32_t  m_height;
   int32_t  m_outputWidth;
   int32_t  m_outputHeight;
   uint32_t m_ssaaScale;
   Window  *m_window;

   // Color buffer, m_width * m_height.
   // 颜色缓冲区，大小为 m_width * m_height。
   ColorA *m_colorBuf;
   ColorA *m_fxaaBuf;       // FXAA buffer, allocated lazily / FXAA 缓冲区，按需分配
   ColorA *m_taaBuf;        // TAA output buffer, allocated lazily / TAA
                            // 输出缓冲区，按需分配
   ColorA *m_taaHistory;    // TAA previous color history / TAA 上一帧颜色历史
   float  *m_taaDepth;      // TAA previous 1/z depth history / TAA 上一帧 1/z 深度历史
   ColorA *m_ssaaBuf;       // SSAA resolved output buffer / SSAA 降采样输出缓冲区
   float  *m_depthBuf;      // depth buffer, m_width * m_height / 深度缓冲区，大小为
                            // m_width * m_height
   ColorA  *m_msaaColorBuf; // 4 samples per pixel / 每像素 4 个采样颜色
   float   *m_msaaDepthBuf; // 4 samples per pixel / 每像素 4 个采样深度
   bool     m_taaValid;
   uint32_t m_taaFrameIndex;

   bool          EnsureMsaaBuffers();
   void          ResolveMsaa();
   bool          ApplyFxaa();
   bool          ApplyTaa(const ColorA *source);
   const ColorA *ApplySsaa(const ColorA *source);
   float2        CurrentTaaJitter() const;
   void          RasterizeSolid(const float3 *v_view, const float2 *p_screen, color3 color);
   void          RasterizeTextured(const float3 *v_view, const float2 *p_screen, const float2 *uvs,
                                   float intensity, const Texture &tex);

   Renderer(const Renderer &)            = delete;
   Renderer &operator=(const Renderer &) = delete;
};

} // namespace HE3D
