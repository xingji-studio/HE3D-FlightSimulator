# Getting Started With HE3D

[中文版](getting-started.md)

Configure and build a console example:

```bash
cmake -S . -B build -DHE3D_BACKEND=CONSOLE -DHE3D_EXAMPLE=TriangleTest
cmake --build build
```

Core object pattern:

```cpp
HE3D::Mesh mesh = HE3D::Mesh::CreateCube(1.0f, 1.0f, 1.0f);
if (!mesh.IsValid()) return 1;

HE3D::GameObject object(mesh);
object.position = {0.0f, 0.0f, 3.0f};
```

Physics pattern:

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

Read the examples for complete programs.
