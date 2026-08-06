#pragma once

#include "he3d_internal.hpp"

namespace HE3D
{

struct ConvexBuildPart;
struct ConvexBuildData;

class ConvexBuilder
{
 public:
   static bool Build(const Mesh &mesh, ConvexBuildMode mode, ConvexBuildData &output);
};

} // namespace HE3D
