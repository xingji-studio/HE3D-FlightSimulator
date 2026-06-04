#pragma once
/*
 * HE3D Flight Simulator - simulation-specific data and mesh builders.
 */
#include "he3d.hpp"

extern const int FLIGHT_FALLBACK_AIRCRAFT_VERTEX_COUNT;
extern const float3 FLIGHT_FALLBACK_AIRCRAFT_VERTICES[];
extern const float2 FLIGHT_FALLBACK_AIRCRAFT_UVS[];

Mesh *CreatePlane(int gridCount, float step, float worldX, float worldZ);
