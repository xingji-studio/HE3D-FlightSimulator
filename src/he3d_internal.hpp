#pragma once

/*
 * HE3D private implementation declarations.
 * HE3D 私有实现声明。
 *
 * This header is only for implementation files. Public users include he3d.hpp.
 * 本头文件只给实现文件使用。公开用户只包含 he3d.hpp。
 */
#include "he3d.hpp"

namespace HE3D
{

struct InternalContact {
   bool   hit;
   float  separation;
   float  penetration;
   float3 normal;
   float3 point;

   InternalContact()
       : hit(false), separation(0.0f), penetration(0.0f), normal{0, 1, 0}, point{0, 0, 0}
   {
   }
};

enum class InternalShapeKind { Box, Convex, StaticMesh };

struct InternalContactShape {
   InternalShapeKind kind;
   const GameObject *object;
   float3            localCenter;
   float3            halfExtents;
   const float3     *vertices;
   int32_t           vertexCount;
   const float3     *faceAxes;
   int32_t           faceAxisCount;
   const float3     *edgeAxes;
   int32_t           edgeAxisCount;
   const Mesh       *mesh;
   AABB              localBounds;

};

namespace Detail
{

class ColliderAccess
{
 public:
   static InternalContactShape From(const GameObject &object, const BoxCollider &collider);
   static InternalContactShape From(const GameObject &object, const ConvexCollider &collider);
   static InternalContactShape From(const GameObject &object, const StaticMeshCollider &collider);
};

class ContactPipeline
{
 public:
   static int32_t Collect(const InternalContactShape &a, const InternalContactShape &b,
                          InternalContact *contacts, int32_t maxContacts);

 private:
   static int32_t CollectStaticTriangles(const InternalContactShape &dynamicShape,
                                         const InternalContactShape &staticShape,
                                         InternalContact *contacts, int32_t maxContacts);
   static int32_t CollectConvexPair(const InternalContactShape &a, const InternalContactShape &b,
                                    InternalContact *contacts, int32_t maxContacts);
};

} // namespace Detail

int32_t GenerateInternalContacts(const InternalContactShape &a, const InternalContactShape &b,
                                 InternalContact *contacts, int32_t maxContacts);

} // namespace HE3D
