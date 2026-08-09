# HE3D API 指南

[English](api-guide.en.md)

## Mesh 和 Texture

`Mesh` 和 `Texture` 是可移动 value。它们拥有自己的内存，并用 `IsValid()` 表示创建失败。
`Mesh::Create()` 只接受完整的三角形列表：`vertexCount` 必须为正数且能被三整除。
`HeightFieldCollider` 的 grid count 必须在 2 到 1024 之间，cell size 必须是有限正数。
`BuildTile()` 遇到非有限 origin 或采样值时会失败且不会激活部分 tile；`NormalAt()` 的输入或采样
非有限时返回向上方向。Renderer 在 SSAA 缩放后的像素分配超过 16,777,216 像素时拒绝尺寸；无效的
`Resize()` 不会提供 presented frame。

```cpp
HE3D::Mesh mesh = HE3D::Mesh::CreateCube(1.0f, 1.0f, 1.0f);
HE3D::Texture texture = HE3D::Texture::LoadImage("model.png");

if (!mesh.IsValid() || !texture.IsValid()) {
    return 1;
}
```

`Texture::Create(pixels, width, height)` 会复制调用者的 RGBA 像素。`LoadImage()` 按文件内容识别 BMP、PNG、JPEG，不依赖扩展名。

`GameObject` 必须用 mesh 引用构造：

```cpp
HE3D::GameObject object(mesh);
object.position = {0.0f, 0.0f, 3.0f};
```

## 渲染

```cpp
HE3D::Renderer renderer(window, width, height);
renderer.SetCamera(camera);
renderer.AddObject(object);
renderer.RenderFrame({0.1f, 0.1f, 0.12f});
```

`Renderer` 借用 camera 和已注册的 object。重复调用 `AddObject()` 会返回 `false`；
`RemoveObject()` 返回是否移除了 object，`ClearObjects()` 清空所有注册。渲染器每帧读取 object
当前的 transform、color、texture 和 `visible` 状态，因此应用可以直接更新这些值，无需重复注册。

## 物理

`PhysicsProperties` 保存可复用配置。`PhysicsScene` 拥有已注册动态物体的运行时速度、角速度、力和力矩。

```cpp
HE3D::BoxCollider collider(1.0f, 1.0f, 1.0f);
HE3D::PhysicsProperties properties;
properties.SetMass(1.0f);
properties.SetInertia(collider.EstimateInertia(properties.GetMass()));
properties.SetRestitution(1.2f); // applied with warning

HE3D::PhysicsScene scene;
scene.SetGravity({0.0f, -9.8f, 0.0f});
scene.AddDynamicBody(object, collider, properties);
scene.SetVelocity(object, {1.0f, 0.0f, 0.0f});
scene.Step(dt, 4);
```

静态三角形场景几何使用 `MeshCollider`：

```cpp
HE3D::MeshCollider floorCollider(floorMesh);
HE3D::GameObject floor(floorMesh);
HE3D::PhysicsMaterial material;
scene.AddStaticBody(floor, floorCollider, material);
```

如果替换了被借用的 mesh value，调用 `MeshCollider::Refresh()` 或 `PhysicsScene::RefreshCollider()`。

`ConvexCollider(mesh, ConvexBuildMode::ConvexDecomposition)` 要求输入是闭合 manifold mesh，并使用固定
`32^3` voxelized approximation，而不是精确的 convex decomposition。已是凸体的 mesh 可以生成一个
part；一般最多生成十六个局部凸 part，细薄特征可能丢失。拓扑无效、不支持的几何或超过 part 上限时，
`BuildFromMesh()` 返回 `false`，并以原子方式保留之前有效的 collider cache。

对于 multipart convex collider，`PhysicsScene::GetContacts()` 会遍历所有 part pair，保留完整的几何
contact patch，再进行确定性的全局排序。调用者只能得到排序后的前 `capacity` 个 contact；较小的输出
缓冲区只会截断查询结果，不改变排序规则。物理 solver 使用独立的 manifold reduction，每个 body pair
最多使用八个 contact，因此 solver reduction 不会删除 `GetContacts()` 返回的几何 patch。

## Ray 查询

```cpp
HE3D::RayHit hit = HE3D::RaycastMeshTriangles(ray, object);
if (hit.hit) {
    // hit.distance, hit.position, hit.triangleIndex
}
```
