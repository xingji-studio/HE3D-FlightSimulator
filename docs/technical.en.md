# HE3D Technical Notes

[中文版](technical.md)

`include/he3d.hpp` is the main public API. It exposes `Renderer`, `Mesh`, `Texture`, `GameObject`, `Camera`, `BoxCollider`, `ConvexCollider`, `MeshCollider`, `PhysicsProperties`, and `PhysicsScene`.

## Resources

`Mesh` privately owns vertex, UV, and triangle-normal arrays. It is movable and non-copyable.

| API | Description |
| --- | --- |
| `IsValid()` | Returns whether the mesh contains usable geometry. |
| `GetVertexCount()` | Returns active vertex count. |
| `GetVertices()` / `GetUVs()` / `GetTriangleNormals()` | Return read-only views. |
| `Mesh::Create(vertices, vertexCount)` | Copies caller-owned positions. |
| `Mesh::Create(vertices, uvs, vertexCount)` | Copies caller-owned positions and UVs. |
| `Mesh::CreateTriangle/Plane/Cube/Sphere()` | Creates built-in primitives. |
| `Mesh::LoadOBJ(filename)` | Loads OBJ and returns an invalid mesh on failure. |

`Texture` privately owns RGBA8 pixels. `Texture::Create()` copies pixels. `Texture::LoadImage()` reads once and identifies BMP, PNG, and JPEG by file header.

## GameObject and raycast

`GameObject(Mesh &mesh)` creates an object borrowing a mesh. There is no no-mesh object state. `RaycastMeshTriangles(ray, object)` returns a `RayHit`; misses have `hit=false` and `triangleIndex=-1`.

## Physics

| Type | Responsibility |
| --- | --- |
| `PhysicsProperties` | Shared configuration: mass, inertia, friction, restitution, damping. |
| `BoxCollider` / `ConvexCollider` / `MeshCollider` | Shape caches and suggested inertia/damping helpers. |
| `PhysicsScene` | Runtime state owner and physics progression entry point. |

`PhysicsScene` owns velocity, angular velocity, accumulated force, and accumulated torque for registered dynamic objects. Runtime state is accessed with `GetVelocity()`, `SetVelocity()`, `AddForce()`, `AddTorque()`, `AddImpulse()`, and `AddAngularImpulse()`.

`SetRestitution()` rejects negative values and accepts values greater than 1 with `PhysicsSettingResult::AppliedWithWarning`.

`MeshCollider` borrows a mesh. After replacing that mesh value, call `Refresh()` or `PhysicsScene::RefreshCollider()`.

## XAPI build

`HE3D_BACKEND=XAPI` requires an explicit matched SDK root:

```bash
cmake -S . -B build-xapi -DHE3D_BACKEND=XAPI -DXJ380_SDK_ROOT=/path/to/sdk
```
