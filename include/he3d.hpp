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
void *operator new(__SIZE_TYPE__ size);
void *operator new[](__SIZE_TYPE__ size);
void  operator delete(void *ptr) noexcept;
void  operator delete[](void *ptr) noexcept;
void  operator delete(void *ptr, __SIZE_TYPE__ size) noexcept;
void  operator delete[](void *ptr, __SIZE_TYPE__ size) noexcept;

namespace HE3D
{

struct PhysicsSceneState;
namespace Detail
{
class ColliderAccess;
}
enum class ColliderKind { Box, Convex, Mesh };
/// SingleHull preserves the input mesh's one-hull behavior. ConvexDecomposition builds up to
/// sixteen convex parts from a fixed 32^3 voxel approximation of a closed mesh.
enum class ConvexBuildMode { SingleHull, ConvexDecomposition };

struct ConvexBuildPart {
   float3  vertices[64];
   int32_t vertexCount;
   float3  faceAxes[16];
   int32_t faceAxisCount;
   float3  edgeAxes[64];
   int32_t edgeAxisCount;
   AABB    bounds;

   ConvexBuildPart()
       : vertices(), vertexCount(0), faceAxes(), faceAxisCount(0), edgeAxes(), edgeAxisCount(0),
         bounds()
   {
   }
};

struct ConvexBuildData {
   ConvexBuildMode mode;
   ConvexBuildPart parts[16];
   int32_t         partCount;
   AABB            bounds;

   ConvexBuildData() : mode(ConvexBuildMode::SingleHull), parts(), partCount(0), bounds() {}
};

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
   Mesh();
   ~Mesh();

   Mesh(Mesh &&other);
   Mesh &operator=(Mesh &&other);
   Mesh(const Mesh &)            = delete;
   Mesh &operator=(const Mesh &) = delete;

   bool          IsValid() const;
   int32_t       GetVertexCount() const;
   const float3 *GetVertices() const;
   const float2 *GetUVs() const;
   const float3 *GetTriangleNormals() const;

    /// vertexCount must be positive and divisible by 3.
    static Mesh Create(const float3 *srcVertices, int32_t vertexCount);
   static Mesh Create(const float3 *srcVertices, const float2 *srcUvs, int32_t vertexCount);
   static Mesh CreateTriangle(float width = 1.0f, float height = 1.0f);
   static Mesh CreatePlane(float width = 1.0f, float depth = 1.0f);
   static Mesh CreateCube(float width = 1.0f, float height = 1.0f, float depth = 1.0f);
   static Mesh CreateSphere(float radius = 0.5f, int32_t segments = 16, int32_t rings = 8);
   static Mesh LoadOBJ(const char *filename);
   static Mesh LoadGLTF(const char *filename);

 private:
   friend class MeshCollider;
   friend class ConvexCollider;
   float3 *m_vertices;
   float2 *m_uvs;
   float3 *m_triangleNormals;
   int32_t m_vertexCount;
   int32_t m_capacity;

   static Mesh Create(int32_t vertexCount);

   bool Init(int32_t vertexCount);
   bool Init(const float3 *srcVertices, const float2 *srcUvs, int32_t vertexCount);
   void Reset();
   void RecalculateTriangleNormals();
};

// ============================================================================
// Texture owns an RGBA pixel buffer.
// Texture 拥有 RGBA 像素缓冲区。
// ============================================================================
class Texture
{
 public:
   Texture();
   ~Texture();

   Texture(Texture &&other);
   Texture &operator=(Texture &&other);
   Texture(const Texture &)            = delete;
   Texture &operator=(const Texture &) = delete;

   int32_t GetWidth() const;
   int32_t GetHeight() const;
   bool    IsValid() const;
   color3  Sample(float u, float v) const;

   static Texture Create(const ColorA *pixels, int32_t width, int32_t height);
   static Texture LoadImage(const char *filename);

 private:
   friend class Renderer;
   int32_t m_width;
   int32_t m_height;
   ColorA *m_pixels;
   bool    m_valid;

   void Reset();
};

// ============================================================================
// GameObject is a mesh instance with position and orientation.
// GameObject 是带位置和方向的网格实例。
// ============================================================================
class GameObject
{
 public:
   float3 position;    // world-space position / 世界空间位置
   quat   orientation; // world-space rotation / 世界空间旋转

   explicit GameObject(Mesh &mesh);

   Mesh       &GetMesh();
   const Mesh &GetMesh() const;
   void        SetMesh(Mesh &mesh);

   // Return local +Z transformed to world space.
   // 返回局部 +Z 方向转换到世界空间后的方向。
   float3 Forward() const { return orientation.rotate({0, 0, 1}); }

 private:
   Mesh *m_mesh;

   GameObject()                              = delete;
   GameObject(const GameObject &)            = delete;
   GameObject &operator=(const GameObject &) = delete;
};

// ============================================================================
// PhysicsContact reserves the public shape of a collision contact result.
// PhysicsContact 预留碰撞接触结果的公开形状。
// ============================================================================
struct PhysicsContact {
   float             penetration;
   float3            normal;
   float3            point;
   const GameObject *other;

   PhysicsContact() : penetration(0.0f), normal{0, 1, 0}, point{0, 0, 0}, other(nullptr) {}
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
RayHit RaycastMeshTriangles(const Ray &ray, const GameObject &object);

// ============================================================================
// Collider is the common public base for all collision shapes.
// Collider 是所有碰撞形状的公开公共基类。
// ============================================================================
class Collider
{
 public:
   virtual bool IsValid() const;
   AABB         LocalAABB() const;
   ColliderKind GetKind() const;
   float3       EstimateInertia(float mass) const;
   float        EstimateDamping(float mass = 1.0f) const;

 protected:
   explicit Collider(ColliderKind kind);
   void SetColliderState(bool valid, AABB localBounds);

   ColliderKind m_kind;
   AABB         m_localBounds;
   bool         m_valid;

 private:
   friend class PhysicsScene;
   friend class Detail::ColliderAccess;

   Collider()                            = delete;
   Collider(const Collider &)            = delete;
   Collider &operator=(const Collider &) = delete;
};

// ============================================================================
// BoxCollider stores a dynamic oriented box shape.
// BoxCollider 保存动态有向盒形状。
// ============================================================================
class BoxCollider : public Collider
{
 public:
   BoxCollider();
   BoxCollider(float width, float height, float depth);

   bool SetSize(float width, float height, float depth);

 private:
   friend class PhysicsScene;
   friend class Detail::ColliderAccess;

   float3 m_halfExtents;

   BoxCollider(const BoxCollider &)            = delete;
   BoxCollider &operator=(const BoxCollider &) = delete;
};

// ============================================================================
// ConvexCollider stores a validated local convex mesh cache. ConvexDecomposition is a fixed 32^3
// voxelized approximation, not an exact decomposition of the source mesh.
// ConvexCollider 保存经过校验的局部凸网格缓存。
// ============================================================================
class ConvexCollider : public Collider
{
 public:
   ConvexCollider();
   explicit ConvexCollider(const Mesh &mesh, ConvexBuildMode mode);
   ~ConvexCollider();

   bool BuildFromMesh(const Mesh &mesh, ConvexBuildMode mode);
   /// Return one for SingleHull or the number of voxelized convex parts after decomposition.
   int32_t GetPartCount() const;

 private:
   friend class PhysicsScene;
   friend class Detail::ColliderAccess;

   struct ConvexFeatureCache {
      ConvexBuildData buildData;
   };

   ConvexFeatureCache m_cache;
   bool               m_hasBuild;

   void Clear();

   ConvexCollider(const ConvexCollider &)            = delete;
   ConvexCollider &operator=(const ConvexCollider &) = delete;
};

// ============================================================================
// MeshCollider borrows a mesh and uses its current triangle geometry.
// MeshCollider 借用 mesh，并使用它当前的三角形几何。
// ============================================================================
class MeshCollider : public Collider
{
 public:
   explicit MeshCollider(const Mesh &mesh);

   void Refresh();

 private:
   friend class PhysicsScene;
   friend class Detail::ColliderAccess;

   const Mesh *m_mesh;

   MeshCollider()                                = delete;
   MeshCollider(const MeshCollider &)            = delete;
   MeshCollider &operator=(const MeshCollider &) = delete;
};

// ============================================================================
// PhysicsProperties stores reusable body configuration.
// PhysicsProperties 保存可复用的物理配置。
// ============================================================================
enum class PhysicsSettingResult { Applied, AppliedWithWarning, Rejected };
enum class PhysicsStepResult { Completed, InvalidInput, InvalidBodyState };

class PhysicsMaterial
{
 public:
   PhysicsMaterial();

   float GetFriction() const;
   float GetRestitution() const;

   PhysicsSettingResult SetFriction(float value);
   PhysicsSettingResult SetRestitution(float value);

 private:
   float m_friction;
   float m_restitution;
};

class PhysicsProperties
{
 public:
   PhysicsProperties();

   float                  GetMass() const;
   float3                 GetInertia() const;
   float                  GetDamping() const;
   const PhysicsMaterial &GetMaterial() const;

   PhysicsSettingResult SetMass(float value);
   PhysicsSettingResult SetInertia(float3 value);
   PhysicsSettingResult SetDamping(float value);
   void                 SetMaterial(const PhysicsMaterial &value);

 private:
   friend class PhysicsScene;

   float           m_mass;
   float3          m_inertia;
   float           m_damping;
   PhysicsMaterial m_material;
};

// ============================================================================
// PhysicsScene registers bodies and owns their runtime state.
// PhysicsScene 注册刚体并拥有其运行时状态。
// ============================================================================
class PhysicsScene
{
 public:
   /// Construct a physics scene with storage for at most capacity bodies.
   /// 构造最多容纳 capacity 个刚体的场景。
   explicit PhysicsScene(int32_t capacity = 32);
   ~PhysicsScene();

   bool IsValid() const;
   bool AddStaticBody(GameObject &object, Collider &collider, PhysicsMaterial &material);
   bool AddKinematicBody(GameObject &object, Collider &collider, PhysicsMaterial &material);
   bool AddDynamicBody(GameObject &object, Collider &collider, PhysicsProperties &properties);
   bool RemoveBody(GameObject &object);
   bool HasBody(const GameObject &object) const;
   PhysicsSettingResult SetGravity(float3 value);
   float3               GetGravity() const;
   PhysicsSettingResult SetDrag(float value);
   float                GetDrag() const;

   float3 GetVelocity(const GameObject &object) const;
   float3 GetAngularVelocity(const GameObject &object) const;
   bool   SetVelocity(GameObject &object, const float3 &velocity);
   bool   SetAngularVelocity(GameObject &object, const float3 &angularVelocity);
   bool   AddForce(GameObject &object, const float3 &force);
   bool   AddTorque(GameObject &object, const float3 &torque);
   bool   AddImpulse(GameObject &object, const float3 &impulse);
   bool   AddAngularImpulse(GameObject &object, const float3 &impulse);
   /// Write up to capacity current contacts for object.
   int32_t GetContacts(const GameObject &object, PhysicsContact *output, int32_t capacity) const;

   /// Remove all dynamic bodies and static colliders from this scene.
   /// 移除此场景中的所有动态刚体和静态碰撞体。
   void Clear();
   /// Integrate bodies, resolve contacts, and clear accumulated force/torque.
   /// 推进刚体、解算接触，并清空累计力/力矩。
   PhysicsStepResult Step(float deltaTime, int32_t iterations = 4);

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
   void          Present();
   void          Resize(int32_t w, int32_t h);
   void          SetMainLight(const DirectionalLight &light) { mainLight = light; }
   int32_t       GetPresentedWidth() const;
   int32_t       GetPresentedHeight() const;
   const ColorA *GetPresentedPixels() const;

 private:
   struct PresentedFrameView {
      const ColorA *pixels;
      int32_t       width;
      int32_t       height;

      PresentedFrameView() : pixels(nullptr), width(0), height(0) {}
      PresentedFrameView(const ColorA *viewPixels, int32_t viewWidth, int32_t viewHeight)
          : pixels(viewPixels), width(viewWidth), height(viewHeight)
      {
      }
   };

   int32_t  m_width;
   int32_t  m_height;
   int32_t  m_outputWidth;
   int32_t  m_outputHeight;
   uint32_t m_ssaaScale;
   Window  *m_window;

   // Color buffer, m_width * m_height.
   // 颜色缓冲区，大小为 m_width * m_height。
   ColorA *m_colorBuf;
   ColorA *m_fxaaBuf;                 // FXAA buffer, allocated lazily / FXAA 缓冲区，按需分配
   ColorA *m_taaBuf;                  // TAA output buffer, allocated lazily / TAA
                                      // 输出缓冲区，按需分配
   ColorA *m_taaHistory;              // TAA previous color history / TAA 上一帧颜色历史
   float  *m_taaDepth;                // TAA previous 1/z depth history / TAA 上一帧 1/z 深度历史
   ColorA *m_ssaaBuf;                 // SSAA resolved output buffer / SSAA 降采样输出缓冲区
   float  *m_depthBuf;                // depth buffer, m_width * m_height / 深度缓冲区，大小为
                                      // m_width * m_height
   ColorA            *m_msaaColorBuf; // 4 samples per pixel / 每像素 4 个采样颜色
   float             *m_msaaDepthBuf; // 4 samples per pixel / 每像素 4 个采样深度
   bool               m_taaValid;
   uint32_t           m_taaFrameIndex;
   PresentedFrameView m_presentedFrame;

   void          InvalidatePresentedView();
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
