# HE3D 入门

[English](getting-started.en.md)

配置并构建 console 示例：

```bash
cmake -S . -B build -DHE3D_BACKEND=CONSOLE -DHE3D_EXAMPLE=TriangleTest
cmake --build build
```

核心对象写法：

```cpp
HE3D::Mesh mesh = HE3D::Mesh::CreateCube(1.0f, 1.0f, 1.0f);
if (!mesh.IsValid()) return 1;

HE3D::GameObject object(mesh);
object.position = {0.0f, 0.0f, 3.0f};
```

物理写法：

```cpp
HE3D::BoxCollider collider(1.0f, 1.0f, 1.0f);
HE3D::PhysicsProperties properties;
properties.SetMass(1.0f);
properties.SetInertia(collider.EstimateInertia(properties.GetMass()));

HE3D::PhysicsScene scene;
scene.AddDynamicBody(object, collider, properties);
scene.AddForce(object, {1.0f, 0.0f, 0.0f});
scene.Step(dt);
```

完整程序见 examples。
