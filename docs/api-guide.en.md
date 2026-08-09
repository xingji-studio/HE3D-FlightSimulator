# HE3D API Guide

[中文版](api-guide.md)

## Mesh and Texture

`Mesh` and `Texture` are movable values. They own their memory and report creation failure with `IsValid()`.
`Mesh::Create()` accepts complete triangle lists only: `vertexCount` must be positive and divisible
by three.
`HeightFieldCollider` accepts grid counts from 2 through 1024 and finite positive cell sizes.
`BuildTile()` rejects non-finite origins or samples without activating a partial tile; `NormalAt()`
returns the up vector when its inputs or samples are non-finite. Renderer dimensions are rejected
when SSAA-scaled pixel allocation would exceed 16,777,216 pixels; an invalid `Resize()` leaves no
presented frame available.

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
renderer.SetCamera(camera);
renderer.AddObject(object);
renderer.RenderFrame({0.1f, 0.1f, 0.12f});
```

`Renderer` borrows the camera and registered objects. Each `SetCamera()` call replaces the previously
borrowed camera; the current camera must remain address-stable until it is replaced or the renderer
is destroyed. Registered objects must remain address-stable until they are removed, `ClearObjects()`
is called, or the renderer is destroyed. `AddObject()` returns `false` for an object already
registered; `RemoveObject()` reports whether it removed an object, and `ClearObjects()` removes every
registration. Without a camera or registered objects, `RenderFrame()` presents only its background.

`GameObject::visible` defaults to `true`. Setting it to `false` keeps the object registered but skips
its drawing. The renderer reads each object's current transform, color, texture, and `visible` state
for every frame, so applications update those values directly without re-registering. A texture set
with `SetTexture()` is also borrowed: do not move, replace, or destroy it while it remains bound to a
registered object. Call `ClearTexture()`, bind a replacement, remove or clear the object registration,
or destroy the renderer first.

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
HE3D::PhysicsMaterial material;
scene.AddStaticBody(floor, floorCollider, material);
```

If the borrowed mesh value is replaced, call `MeshCollider::Refresh()` or `PhysicsScene::RefreshCollider()`.

`ConvexCollider(mesh, ConvexBuildMode::ConvexDecomposition)` accepts a closed manifold mesh and
builds a fixed `32^3` voxelized approximation, not an exact convex decomposition. It can produce
one part for an already-convex mesh, or up to sixteen local convex parts. Thin features may be
lost. Invalid topology, unsupported geometry, or a build that exceeds the part limit fails
atomically: `BuildFromMesh()` returns `false` and preserves the previous valid collider cache.

For a multipart convex collider, `PhysicsScene::GetContacts()` traverses every part pair and keeps
the complete geometric contact patches before sorting them deterministically. It writes the first
`capacity` contacts after that global sort; a smaller caller buffer therefore truncates the query
result without changing which contacts precede it. The physics solver uses a separate manifold
reduction of at most eight contacts per body pair, so solver reduction does not remove patches
returned by `GetContacts()`.

## Ray Queries

```cpp
HE3D::RayHit hit = HE3D::RaycastMeshTriangles(ray, object);
if (hit.hit) {
    // hit.distance, hit.position, hit.triangleIndex
}
```
