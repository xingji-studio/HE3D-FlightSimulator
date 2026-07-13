#pragma once
/*
 * HE3D Flight Simulator - simulation-specific data and mesh builders.
 * HE3D 飞行模拟器：示例专用数据和网格构建函数。
 */
#include "he3d.hpp"

// Vertex count for the built-in aircraft fallback mesh.
// 内置飞机备用 mesh 的顶点数量。
extern const HE3D::int32_t flightFallbackAircraftVertexCount;

// Vertices for the built-in aircraft fallback mesh.
// 内置飞机备用 mesh 的顶点数据。
extern const HE3D::float3 flightFallbackAircraftVertices[];

// UVs for the built-in aircraft fallback mesh.
// 内置飞机备用 mesh 的 UV 数据。
extern const HE3D::float2 flightFallbackAircraftUvs[];

// Create a terrain mesh tile centered at the given world coordinate.
// 创建以指定世界坐标为中心的地形 mesh 块。
HE3D::Mesh *CreatePlane(HE3D::int32_t gridCount, float step, float worldX, float worldZ);

// Rewrite an existing terrain mesh tile for a new world coordinate.
// 将已有地形 mesh 块改写到新的世界坐标。
bool UpdatePlaneMesh(HE3D::Mesh *mesh, HE3D::int32_t gridCount, float step, float worldX, float worldZ);

// Return procedural terrain height at a world-space x/z coordinate.
// 返回世界空间 x/z 坐标处的程序化地形高度。
float TerrainHeightAt(float x, float z);
