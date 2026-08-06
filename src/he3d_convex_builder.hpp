#pragma once

#include "he3d_internal.hpp"

namespace HE3D
{

struct ConvexBuildPart
{
   float3 vertices[64];
   int32_t vertexCount;
   float3 faceAxes[16];
   int32_t faceAxisCount;
   float3 edgeAxes[64];
   int32_t edgeAxisCount;
   AABB bounds;

   ConvexBuildPart()
       : vertices(), vertexCount(0), faceAxes(), faceAxisCount(0), edgeAxes(), edgeAxisCount(0),
         bounds()
   {
   }
};

struct ConvexBuildData
{
   ConvexBuildMode mode;
   ConvexBuildPart parts[16];
   int32_t         partCount;
   AABB            bounds;

   ConvexBuildData() : mode(ConvexBuildMode::SingleHull), parts(), partCount(0), bounds() {}
};

class ConvexBuilder
{
 public:
   static bool Build(const Mesh &mesh, ConvexBuildMode mode, ConvexBuildData &output);
};

} // namespace HE3D
