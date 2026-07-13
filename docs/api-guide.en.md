# HE3D API Guide

[中文版](api-guide.md)

## Mesh and Texture

`Mesh` and `Texture` are movable values. They own their memory and report creation failure with `IsValid()`.

```cpp
HE3D::Mesh mesh = HE3D::Mesh::CreateCube(1.0f, 1.0f, 1.0f);
HE3D::Texture texture = HE3D::Texture::LoadImage("model.png");

if (!mesh.IsValid() || !texture.IsValid()) {
    return 1;
}
```

`Texture::Create(pixels, width, height)` copies the caller-owned RGBA pixels. `LoadImage()` detects BMP, PNG, and JPEG from file contents, not the file extension.

`GameObject` must be constructed with a mesh reference:

```cpp
HE3D::GameObject object(mesh);
object.position = {0.0f, 0.0f, 3.0f};
```

## Rendering

```cpp
HE3D::Renderer renderer(window, width, height);
renderer.Clear({0.1f, 0.1f, 0.12f});
renderer.DrawGameObject(object, camera, HE3D::color3(1, 0, 0));
renderer.Present();
```

## Physics

`PhysicsProperties` stores reusable configuration. `PhysicsScene` owns runtime velocity, angular velocity, force, and torque for registered dynamic bodies.

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

Static triangle geometry uses `MeshCollider`:

```cpp
HE3D::MeshCollider floorCollider(floorMesh);
HE3D::GameObject floor(floorMesh);
scene.AddStaticBody(floor, floorCollider);
```

If the borrowed mesh value is replaced, call `MeshCollider::Refresh()` or `PhysicsScene::RefreshCollider()`.

## Ray Queries

```cpp
HE3D::RayHit hit = HE3D::RaycastMeshTriangles(ray, object);
if (hit.hit) {
    // hit.distance, hit.position, hit.triangleIndex
}
```
