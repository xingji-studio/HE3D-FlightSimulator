#pragma once
/*
 * HE3D Flight Simulator - simulation-specific data and mesh builders.
 * HE3D 飞行模拟器：示例专用数据和网格构建函数。
 */
#include "he3d.hpp"

extern const HE3D::int32_t FLIGHT_FALLBACK_AIRCRAFT_VERTEX_COUNT;
extern const HE3D::float3 FLIGHT_FALLBACK_AIRCRAFT_VERTICES[];
extern const HE3D::float2 FLIGHT_FALLBACK_AIRCRAFT_UVS[];

HE3D::Mesh *CreatePlane(HE3D::int32_t gridCount, float step, float worldX, float worldZ);
bool UpdatePlaneMesh(HE3D::Mesh *mesh, HE3D::int32_t gridCount, float step, float worldX, float worldZ);
float TerrainHeightAt(float x, float z);
