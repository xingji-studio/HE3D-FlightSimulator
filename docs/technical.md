# HE3D 技术说明

[English](technical.en.md)

`include/he3d.hpp` 是主要公开 API。它公开 `Renderer`、`Mesh`、`Texture`、`GameObject`、`Camera`、`BoxCollider`、`ConvexCollider`、`MeshCollider`、`PhysicsProperties` 和 `PhysicsScene`。

## 资源

`Mesh` 私有拥有顶点、UV 和三角形法线数组。它可移动、不可拷贝。

| API | 说明 |
| --- | --- |
| `IsValid()` | 返回 mesh 是否包含可用几何。 |
| `GetVertexCount()` | 返回有效顶点数。 |
| `GetVertices()` / `GetUVs()` / `GetTriangleNormals()` | 返回只读视图。 |
| `Mesh::Create(vertices, vertexCount)` | 复制调用者持有的位置数组。 |
| `Mesh::Create(vertices, uvs, vertexCount)` | 复制调用者持有的位置和 UV。 |
| `Mesh::CreateTriangle/Plane/Cube/Sphere()` | 创建内置基础形状。 |
| `Mesh::LoadOBJ(filename)` | 加载 OBJ，失败返回 invalid mesh。 |

`Texture` 私有拥有 RGBA8 像素。`Texture::Create()` 会复制像素。`Texture::LoadImage()` 只读取一次文件，并按文件头识别 BMP、PNG、JPEG。

## GameObject 和 raycast

`GameObject(Mesh &mesh)` 创建借用 mesh 的对象。不再存在无 mesh 的对象状态。`RaycastMeshTriangles(ray, object)` 返回 `RayHit`；未命中时 `hit=false` 且 `triangleIndex=-1`。

## 物理

| 类型 | 职责 |
| --- | --- |
| `PhysicsProperties` | 共享配置：质量、惯量、摩擦、反弹、阻尼。 |
| `BoxCollider` / `ConvexCollider` / `MeshCollider` | 形状缓存和建议惯量/阻尼 helper。 |
| `PhysicsScene` | 运行时状态拥有者和物理推进入口。 |

`PhysicsScene` 拥有已注册动态对象的速度、角速度、累计力和累计力矩。运行时状态通过 `GetVelocity()`、`SetVelocity()`、`AddForce()`、`AddTorque()`、`AddImpulse()`、`AddAngularImpulse()` 访问。

`SetRestitution()` 拒绝负数，允许大于 1 的值并返回 `PhysicsSettingResult::AppliedWithWarning`。

`MeshCollider` 借用 mesh。替换该 mesh value 后，调用 `Refresh()` 或 `PhysicsScene::RefreshCollider()`。

`ConvexCollider(mesh, ConvexBuildMode::ConvexDecomposition)` 最多构建十六个局部凸 part，并通过
`GetPartCount()` 返回数量。结果是 closed source mesh 的固定 `32^3` voxelized approximation，
不是精确的 mesh convex decomposition。细薄特征可能丢失或导致构建失败。该 mode 只公开 multipart
collider 数据；multipart PhysicsScene contact traversal 是另一项独立能力。

## XAPI 构建

`HE3D_BACKEND=XAPI` 需要显式指定匹配的 SDK root：

```bash
cmake -S . -B build-xapi -DHE3D_BACKEND=XAPI -DXJ380_SDK_ROOT=/path/to/sdk
```
