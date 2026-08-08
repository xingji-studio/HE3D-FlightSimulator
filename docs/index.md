# HE3D 文档索引

[English version](index.en.md)

这里是 HE3D 文档入口。第一次使用从新手入门开始；查概念看概念页；写新后端看移植指南。完整 API 手册维护在 [技术总览](technical.md)，`include/` 里的双语注释是实现侧提示和最终声明来源。

## 阅读顺序

1. [新手入门](getting-started.md)：构建、运行示例、写第一个 HE3D 程序。
2. [核心概念](concepts.md)：理解 Platform、Renderer、Mesh/GameObject、PhysicsScene。
3. [示例说明](examples.md)：每个 example 展示什么、按什么顺序读源码。
4. [API 使用指南](api-guide.md)：按模块看常用 API 的调用方式和边界。
5. [移植指南](porting.md)：为新平台实现 HE3D backend。
6. [技术总览](technical.md)：仓库结构、模块边界和维护规则。

## 维护规则

- 中文和英文文档成对维护，标题结构尽量一致。
- 文档解释“怎么用”和“为什么这么用”，并在技术总览中维护完整公开 API 参考。
- API 变化先改 `include/` 注释，再改相关指南和示例说明。
- 不在文档中复制大段 class 声明，避免和头文件漂移。
