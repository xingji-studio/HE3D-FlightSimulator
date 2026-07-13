# HE3D API 指南

[English](api-guide.en.md)

## Mesh 和 Texture

`Mesh` 和 `Texture` 是可移动 value。它们拥有自己的内存，并用 `IsValid()` 表示创建失败。

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
renderer.Clear({0.1f, 0.1f, 0.12f});
renderer.DrawGameObject(object, camera, HE3D::color3(1, 0, 0));
renderer.Present();
```

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
scene.AddStaticBody(floor, floorCollider);
```

如果替换了被借用的 mesh value，调用 `MeshCollider::Refresh()` 或 `PhysicsScene::RefreshCollider()`。

## Ray 查询

```cpp
HE3D::RayHit hit = HE3D::RaycastMeshTriangles(ray, object);
if (hit.hit) {
    // hit.distance, hit.position, hit.triangleIndex
}
```
