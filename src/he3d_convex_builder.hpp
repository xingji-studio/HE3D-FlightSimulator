#pragma once

#include "he3d_internal.hpp"

namespace HE3D
{

enum class InternalConvexBuildMode { SingleHull, Decomposition };

class ConvexBuilder
{
 public:
   static bool Build(const Mesh &mesh, ConvexBuildMode mode, ConvexBuildData &output);
   static bool BuildInternal(const Mesh &mesh, InternalConvexBuildMode mode,
                             ConvexBuildData &output);
};

} // namespace HE3D
