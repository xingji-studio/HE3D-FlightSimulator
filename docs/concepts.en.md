# HE3D Concepts

[中文版](concepts.md)

- `Mesh` is owned geometry. It is a movable value and exposes read-only vertex, UV, and triangle-normal views.
- `Texture` is owned RGBA pixels. It is a movable value and can be created from copied RGBA pixels or loaded with `LoadImage()`.
- `GameObject` is a mesh instance with position and orientation. It always borrows a mesh.
- `BoxCollider`, `ConvexCollider`, and `MeshCollider` are shape caches.
- `PhysicsProperties` is configuration: mass, inertia, friction, restitution, and damping.
- `PhysicsScene` owns runtime state for registered dynamic objects and is the only physics progression entry point.

Scene gravity is configured on `PhysicsScene`. Per-object exceptional motion should be expressed with forces or impulses.
