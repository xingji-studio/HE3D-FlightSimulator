#pragma once
/*
 * HE3D Flight Simulator - simulation-specific data and mesh builders.
 */
#include "he3d.hpp"

extern const int FLIGHT_FALLBACK_AIRCRAFT_VERTEX_COUNT;
extern const HE3D::float3 FLIGHT_FALLBACK_AIRCRAFT_VERTICES[];
extern const HE3D::float2 FLIGHT_FALLBACK_AIRCRAFT_UVS[];

HE3D::Mesh *CreatePlane(int gridCount, float step, float worldX, float worldZ);
bool UpdatePlaneMesh(HE3D::Mesh *mesh, int gridCount, float step, float worldX, float worldZ);
float TerrainHeightAt(float x, float z);
