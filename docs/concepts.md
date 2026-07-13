# HE3D 概念

[English](concepts.en.md)

- `Mesh` 是拥有几何数据的 value，可移动，提供只读顶点、UV、三角形法线视图。
- `Texture` 是拥有 RGBA 像素的 value，可移动，可从复制的 RGBA 像素创建，也可用 `LoadImage()` 加载。
- `GameObject` 是带位置和朝向的 mesh 实例，始终借用一个 mesh。
- `BoxCollider`、`ConvexCollider`、`MeshCollider` 是碰撞形状缓存。
- `PhysicsProperties` 是配置：质量、惯量、摩擦、反弹、阻尼。
- `PhysicsScene` 拥有已注册动态物体的运行时状态，并且是唯一物理推进入口。

重力统一配置在 `PhysicsScene`。单个物体的特殊运动用力或冲量表达。
