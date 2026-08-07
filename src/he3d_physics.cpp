#include "he3d_internal.hpp"

namespace HE3D
{

enum class PhysicsMotionType { Static, Kinematic, Dynamic };

struct PhysicsRuntimeState {
   float3 velocity;
   float3 angularVelocity;
   float3 force;
   float3 torque;

   PhysicsRuntimeState()
       : velocity{0, 0, 0}, angularVelocity{0, 0, 0}, force{0, 0, 0}, torque{0, 0, 0}
   {
   }
};

struct PhysicsBodySnapshot {
   float3              position;
   quat                orientation;
   PhysicsRuntimeState runtime;
};

struct PhysicsSceneEntry {
   GameObject         *object;
   PhysicsProperties  *properties;
   PhysicsMaterial    *material;
   Collider           *collider;
   PhysicsMotionType   motion;
   PhysicsRuntimeState runtime;

   PhysicsSceneEntry()
       : object(nullptr), properties(nullptr), material(nullptr), collider(nullptr),
         motion(PhysicsMotionType::Static), runtime()
   {
   }
};

struct PhysicsSceneState {
   PhysicsSceneEntry   *entries;
   int32_t              entryCount;
   int32_t              entryCapacity;
   float3               gravity;
   float                drag;
   PhysicsBodySnapshot *snapshots;

   explicit PhysicsSceneState(int32_t capacity)
       : entries(nullptr), entryCount(0), entryCapacity(0), gravity{0, -9.8f, 0}, drag(0.02f),
         snapshots(nullptr)
   {
      if (capacity < 1) return;
      entries   = new PhysicsSceneEntry[capacity];
      snapshots = new PhysicsBodySnapshot[capacity];
      if (entries && snapshots) {
         entryCapacity = capacity;
      } else {
         delete[] entries;
         delete[] snapshots;
         entries   = nullptr;
         snapshots = nullptr;
      }
   }

   ~PhysicsSceneState()
   {
      delete[] snapshots;
      delete[] entries;
   }

   bool HasCapacity() const { return entryCount < entryCapacity; }

   PhysicsSceneEntry *Find(const GameObject &object)
   {
      for (int32_t i = 0; i < entryCount; i++) {
         if (entries[i].object == &object) {
            return &entries[i];
         }
      }
      return nullptr;
   }

   const PhysicsSceneEntry *Find(const GameObject &object) const
   {
      for (int32_t i = 0; i < entryCount; i++) {
         if (entries[i].object == &object) {
            return &entries[i];
         }
      }
      return nullptr;
   }

   PhysicsSceneEntry &Append()
   {
      PhysicsSceneEntry &entry = entries[entryCount];
      entryCount++;
      return entry;
   }

 private:
   PhysicsSceneState(const PhysicsSceneState &)            = delete;
   PhysicsSceneState &operator=(const PhysicsSceneState &) = delete;
};

static bool IsFiniteFloat(float value)
{
   return value == value && value <= 340282346638528859811704183484516925440.0f &&
          value >= -340282346638528859811704183484516925440.0f;
}

static bool IsFiniteFloat3(float3 value)
{
   return IsFiniteFloat(value.x) && IsFiniteFloat(value.y) && IsFiniteFloat(value.z);
}

static bool IsFiniteQuat(quat value)
{
   return IsFiniteFloat(value.w) && IsFiniteFloat(value.x) && IsFiniteFloat(value.y) &&
          IsFiniteFloat(value.z);
}

static bool IsNormalizedQuat(quat value)
{
   float lengthSq = value.w * value.w + value.x * value.x + value.y * value.y + value.z * value.z;
   return IsFiniteQuat(value) && HE3D_ABS(lengthSq - 1.0f) <= 0.001f;
}

PhysicsMaterial::PhysicsMaterial() : m_friction(0.5f), m_restitution(0.0f) {}

float PhysicsMaterial::GetFriction() const { return m_friction; }
float PhysicsMaterial::GetRestitution() const { return m_restitution; }

PhysicsSettingResult PhysicsMaterial::SetFriction(float value)
{
   if (!IsFiniteFloat(value) || value < 0.0f) return PhysicsSettingResult::Rejected;
   m_friction = value;
   return PhysicsSettingResult::Applied;
}

PhysicsSettingResult PhysicsMaterial::SetRestitution(float value)
{
   if (!IsFiniteFloat(value) || value < 0.0f) return PhysicsSettingResult::Rejected;
   m_restitution = value;
   return value > 1.0f ? PhysicsSettingResult::AppliedWithWarning : PhysicsSettingResult::Applied;
}

PhysicsProperties::PhysicsProperties()
    : m_mass(1.0f), m_inertia{1.0f, 1.0f, 1.0f}, m_damping(0.0f), m_material()
{
}

float                  PhysicsProperties::GetMass() const { return m_mass; }
float3                 PhysicsProperties::GetInertia() const { return m_inertia; }
float                  PhysicsProperties::GetDamping() const { return m_damping; }
const PhysicsMaterial &PhysicsProperties::GetMaterial() const { return m_material; }

PhysicsSettingResult PhysicsProperties::SetMass(float value)
{
   if (!IsFiniteFloat(value) || value <= 0.0f) {
      return PhysicsSettingResult::Rejected;
   }
   m_mass = value;
   return PhysicsSettingResult::Applied;
}

PhysicsSettingResult PhysicsProperties::SetInertia(float3 value)
{
   if (!IsFiniteFloat3(value) || value.x <= 0.0f || value.y <= 0.0f || value.z <= 0.0f) {
      return PhysicsSettingResult::Rejected;
   }
   m_inertia = value;
   return PhysicsSettingResult::Applied;
}

PhysicsSettingResult PhysicsProperties::SetDamping(float value)
{
   if (!IsFiniteFloat(value) || value < 0.0f) {
      return PhysicsSettingResult::Rejected;
   }
   m_damping = value;
   return PhysicsSettingResult::Applied;
}

void PhysicsProperties::SetMaterial(const PhysicsMaterial &value) { m_material = value; }

PhysicsScene::PhysicsScene(int32_t capacity) : m_impl(new PhysicsSceneState(capacity)) {}

PhysicsScene::~PhysicsScene() { delete m_impl; }

void PhysicsScene::Clear()
{
   if (m_impl) {
      m_impl->entryCount = 0;
   }
}

static bool IsDynamicKind(PhysicsMotionType motion) { return motion == PhysicsMotionType::Dynamic; }

static bool HasRuntimeMotion(PhysicsMotionType motion)
{
   return motion == PhysicsMotionType::Dynamic || motion == PhysicsMotionType::Kinematic;
}

static float InverseMass(const PhysicsSceneEntry *entry)
{
   if (!entry || !IsDynamicKind(entry->motion) || !entry->properties ||
       entry->properties->GetMass() <= 0.0f) {
      return 0.0f;
   }
   return 1.0f / entry->properties->GetMass();
}

static float InverseInertia(const PhysicsSceneEntry *entry)
{
   if (!entry || !IsDynamicKind(entry->motion) || !entry->properties ||
       entry->properties->GetInertia().x <= 0.0f) {
      return 0.0f;
   }
   return 1.0f / entry->properties->GetInertia().x;
}

static bool AddBody(PhysicsSceneState *scene, GameObject &object, Collider &collider,
                    PhysicsMotionType motion, PhysicsProperties *properties,
                    PhysicsMaterial *material)
{
   if (!scene || !collider.IsValid() || !scene->HasCapacity() || scene->Find(object) ||
       !IsFiniteFloat3(object.position) || !IsNormalizedQuat(object.orientation) ||
       (motion == PhysicsMotionType::Dynamic && collider.GetKind() == ColliderKind::Mesh)) {
      return false;
   }
   PhysicsSceneEntry &entry = scene->Append();
   entry.object             = &object;
   entry.properties         = properties;
   entry.material           = material;
   entry.collider           = &collider;
   entry.motion             = motion;
   entry.runtime            = PhysicsRuntimeState();
   return true;
}

bool PhysicsScene::IsValid() const { return m_impl && m_impl->entries; }

bool PhysicsScene::AddStaticBody(GameObject &object, Collider &collider, PhysicsMaterial &material)
{
   return AddBody(m_impl, object, collider, PhysicsMotionType::Static, nullptr, &material);
}

bool PhysicsScene::AddKinematicBody(GameObject &object, Collider &collider,
                                    PhysicsMaterial &material)
{
   return AddBody(m_impl, object, collider, PhysicsMotionType::Kinematic, nullptr, &material);
}

bool PhysicsScene::AddDynamicBody(GameObject &object, Collider &collider,
                                  PhysicsProperties &properties)
{
   return AddBody(m_impl, object, collider, PhysicsMotionType::Dynamic, &properties, nullptr);
}

bool PhysicsScene::RemoveBody(GameObject &object)
{
   if (!m_impl) {
      return false;
   }
   for (int32_t i = 0; i < m_impl->entryCount; i++) {
      if (m_impl->entries[i].object == &object) {
         for (int32_t j = i; j < m_impl->entryCount - 1; j++) {
            m_impl->entries[j] = m_impl->entries[j + 1];
         }
         m_impl->entryCount--;
         m_impl->entries[m_impl->entryCount] = PhysicsSceneEntry();
         return true;
      }
   }
   return false;
}

bool PhysicsScene::HasBody(const GameObject &object) const
{
   return m_impl && m_impl->Find(object);
}

PhysicsSettingResult PhysicsScene::SetGravity(float3 value)
{
   if (!m_impl || !IsFiniteFloat3(value)) return PhysicsSettingResult::Rejected;
   m_impl->gravity = value;
   return PhysicsSettingResult::Applied;
}

float3 PhysicsScene::GetGravity() const { return m_impl ? m_impl->gravity : float3(0, 0, 0); }

PhysicsSettingResult PhysicsScene::SetDrag(float value)
{
   if (!m_impl || !IsFiniteFloat(value) || value < 0.0f) return PhysicsSettingResult::Rejected;
   m_impl->drag = value;
   return PhysicsSettingResult::Applied;
}

float PhysicsScene::GetDrag() const { return m_impl ? m_impl->drag : 0.0f; }

float3 PhysicsScene::GetVelocity(const GameObject &object) const
{
   const PhysicsSceneEntry *entry = m_impl ? m_impl->Find(object) : nullptr;
   return entry && HasRuntimeMotion(entry->motion) ? entry->runtime.velocity : float3(0, 0, 0);
}

float3 PhysicsScene::GetAngularVelocity(const GameObject &object) const
{
   const PhysicsSceneEntry *entry = m_impl ? m_impl->Find(object) : nullptr;
   return entry && HasRuntimeMotion(entry->motion) ? entry->runtime.angularVelocity
                                                   : float3(0, 0, 0);
}

bool PhysicsScene::SetVelocity(GameObject &object, const float3 &velocity)
{
   PhysicsSceneEntry *entry = m_impl ? m_impl->Find(object) : nullptr;
   if (!entry || !HasRuntimeMotion(entry->motion) || !IsFiniteFloat3(velocity)) {
      return false;
   }
   entry->runtime.velocity = velocity;
   return true;
}

bool PhysicsScene::SetAngularVelocity(GameObject &object, const float3 &angularVelocity)
{
   PhysicsSceneEntry *entry = m_impl ? m_impl->Find(object) : nullptr;
   if (!entry || !HasRuntimeMotion(entry->motion) || !IsFiniteFloat3(angularVelocity)) {
      return false;
   }
   entry->runtime.angularVelocity = angularVelocity;
   return true;
}

bool PhysicsScene::AddForce(GameObject &object, const float3 &force)
{
   PhysicsSceneEntry *entry = m_impl ? m_impl->Find(object) : nullptr;
   if (!entry || !IsDynamicKind(entry->motion) || !IsFiniteFloat3(force)) {
      return false;
   }
   float3 accumulated = entry->runtime.force + force;
   if (!IsFiniteFloat3(accumulated)) return false;
   entry->runtime.force = accumulated;
   return true;
}

bool PhysicsScene::AddTorque(GameObject &object, const float3 &torque)
{
   PhysicsSceneEntry *entry = m_impl ? m_impl->Find(object) : nullptr;
   if (!entry || !IsDynamicKind(entry->motion) || !IsFiniteFloat3(torque)) {
      return false;
   }
   float3 accumulated = entry->runtime.torque + torque;
   if (!IsFiniteFloat3(accumulated)) return false;
   entry->runtime.torque = accumulated;
   return true;
}

bool PhysicsScene::AddImpulse(GameObject &object, const float3 &impulse)
{
   PhysicsSceneEntry *entry = m_impl ? m_impl->Find(object) : nullptr;
   if (!entry || !IsDynamicKind(entry->motion)) {
      return false;
   }
   if (!IsFiniteFloat3(impulse)) return false;
   float3 velocity = entry->runtime.velocity + impulse * InverseMass(entry);
   if (!IsFiniteFloat3(velocity)) return false;
   entry->runtime.velocity = velocity;
   return true;
}

bool PhysicsScene::AddAngularImpulse(GameObject &object, const float3 &impulse)
{
   PhysicsSceneEntry *entry = m_impl ? m_impl->Find(object) : nullptr;
   if (!entry || !IsDynamicKind(entry->motion)) {
      return false;
   }
   if (!IsFiniteFloat3(impulse)) return false;
   float3 angularVelocity = entry->runtime.angularVelocity + impulse * InverseInertia(entry);
   if (!IsFiniteFloat3(angularVelocity)) return false;
   entry->runtime.angularVelocity = angularVelocity;
   return true;
}

static bool IntegrateSceneEntry(PhysicsSceneEntry &entry, float deltaTime, float sceneDrag,
                                float3 gravity)
{
   if (!entry.object || deltaTime <= 0.0f) {
      return true;
   }

   if (entry.motion == PhysicsMotionType::Kinematic) {
      entry.object->position    = entry.object->position + entry.runtime.velocity * deltaTime;
      quat dq                   = quat(1.0f, 0.5f * entry.runtime.angularVelocity.x * deltaTime,
                                       0.5f * entry.runtime.angularVelocity.y * deltaTime,
                                       0.5f * entry.runtime.angularVelocity.z * deltaTime);
      entry.object->orientation = (entry.object->orientation * dq).normalizeFast();
      return IsFiniteFloat3(entry.object->position) && IsNormalizedQuat(entry.object->orientation);
   }
   if (!entry.properties || !IsDynamicKind(entry.motion)) return true;

   float inverseMass    = InverseMass(&entry);
   float inverseInertia = InverseInertia(&entry);
   entry.runtime.velocity =
       entry.runtime.velocity + (entry.runtime.force * inverseMass + gravity) * deltaTime;
   entry.runtime.angularVelocity =
       entry.runtime.angularVelocity + entry.runtime.torque * inverseInertia * deltaTime;

   if (entry.properties->GetDamping() > 0.0f) {
      float damp = 1.0f - entry.properties->GetDamping() * deltaTime;
      if (damp < 0.0f) damp = 0.0f;
      entry.runtime.velocity        = entry.runtime.velocity * damp;
      entry.runtime.angularVelocity = entry.runtime.angularVelocity * damp;
   }

   if (sceneDrag > 0.0f) {
      float dragScale = 1.0f - sceneDrag * deltaTime;
      if (dragScale < 0.0f) dragScale = 0.0f;
      entry.runtime.velocity        = entry.runtime.velocity * dragScale;
      entry.runtime.angularVelocity = entry.runtime.angularVelocity * dragScale;
   }

   entry.object->position    = entry.object->position + entry.runtime.velocity * deltaTime;
   quat dq                   = quat(1.0f, 0.5f * entry.runtime.angularVelocity.x * deltaTime,
                                    0.5f * entry.runtime.angularVelocity.y * deltaTime,
                                    0.5f * entry.runtime.angularVelocity.z * deltaTime);
   entry.object->orientation = (entry.object->orientation * dq).normalizeFast();
   return IsFiniteFloat3(entry.object->position) && IsNormalizedQuat(entry.object->orientation) &&
          IsFiniteFloat3(entry.runtime.velocity) && IsFiniteFloat3(entry.runtime.angularVelocity);
}

static float3 EntryVelocityAtPoint(const PhysicsSceneEntry *entry, float3 point)
{
   if (!entry || !entry->object || !IsDynamicKind(entry->motion)) {
      return {0, 0, 0};
   }
   float3 r = point - entry->object->position;
   return entry->runtime.velocity + float3::cross(entry->runtime.angularVelocity, r);
}

static void ApplySceneImpulse(PhysicsSceneEntry *entry, float3 point, float3 impulse)
{
   if (!entry || !entry->object || !IsDynamicKind(entry->motion)) {
      return;
   }
   entry->runtime.velocity = entry->runtime.velocity + impulse * InverseMass(entry);
   float3 r                = point - entry->object->position;
   entry->runtime.angularVelocity =
       entry->runtime.angularVelocity + float3::cross(r, impulse) * InverseInertia(entry);
}

static const PhysicsMaterial &EntryMaterial(const PhysicsSceneEntry &entry)
{
   if (entry.properties) return entry.properties->GetMaterial();
   if (entry.material) return *entry.material;
   static PhysicsMaterial fallback;
   return fallback;
}

static bool ResolveSceneContact(PhysicsSceneEntry &entryA, PhysicsSceneEntry &entryB,
                                const InternalContact &contact)
{
   if (!contact.hit || contact.penetration <= 0.0f) {
      return true;
   }

   float invMassA   = InverseMass(&entryA);
   float invMassB   = InverseMass(&entryB);
   float invMassSum = invMassA + invMassB;
   if (invMassSum <= 0.0f) {
      return true;
   }

   const float slop            = 0.0005f;
   float       correctionDepth = contact.penetration - slop;
   if (correctionDepth > 0.0f) {
      float  correctionMagnitude = correctionDepth * 0.6f / invMassSum;
      float3 correction          = contact.normal * correctionMagnitude;
      if (entryA.object) {
         entryA.object->position = entryA.object->position + correction * invMassA;
      }
      if (entryB.object) {
         entryB.object->position = entryB.object->position - correction * invMassB;
      }
   }

   float3 relativeVelocity =
       EntryVelocityAtPoint(&entryA, contact.point) - EntryVelocityAtPoint(&entryB, contact.point);
   float normalVelocity = float3::dot(relativeVelocity, contact.normal);

   float inertiaTerm = invMassSum;
   if (entryA.object) {
      float3 ra       = contact.point - entryA.object->position;
      float3 raCrossN = float3::cross(ra, contact.normal);
      inertiaTerm += raCrossN.lengthSq() * InverseInertia(&entryA);
   }
   if (entryB.object) {
      float3 rb       = contact.point - entryB.object->position;
      float3 rbCrossN = float3::cross(rb, contact.normal);
      inertiaTerm += rbCrossN.lengthSq() * InverseInertia(&entryB);
   }
   if (inertiaTerm <= 0.000001f) {
      return true;
   }

   float impulseMagnitude = 0.0f;
   if (normalVelocity < 0.0f) {
      float restitutionA = EntryMaterial(entryA).GetRestitution();
      float restitutionB = EntryMaterial(entryB).GetRestitution();
      float restitution  = restitutionA > restitutionB ? restitutionA : restitutionB;
      if (-normalVelocity < 0.35f) {
         restitution = 0.0f;
      }
      impulseMagnitude     = -((1.0f + restitution) * normalVelocity) / inertiaTerm;
      float3 normalImpulse = contact.normal * impulseMagnitude;
      ApplySceneImpulse(&entryA, contact.point, normalImpulse);
      ApplySceneImpulse(&entryB, contact.point, -normalImpulse);
   }

   float frictionA = EntryMaterial(entryA).GetFriction();
   float frictionB = EntryMaterial(entryB).GetFriction();
   float friction  = (frictionA + frictionB) * 0.5f;
   relativeVelocity =
       EntryVelocityAtPoint(&entryA, contact.point) - EntryVelocityAtPoint(&entryB, contact.point);
   float  normalComponent = float3::dot(relativeVelocity, contact.normal);
   float3 tangent         = relativeVelocity - contact.normal * normalComponent;
   if (tangent.lengthSq() > 0.000001f) {
      tangent                 = tangent.normalizeFast();
      float frictionMagnitude = -float3::dot(relativeVelocity, tangent) / inertiaTerm;
      float support           = HE3D_MAX(contact.penetration - 0.0005f, 0.0f) * 2.0f;
      if (support > 0.02f) support = 0.02f;
      float maxFriction = (impulseMagnitude + support) * friction;
      if (frictionMagnitude > maxFriction) frictionMagnitude = maxFriction;
      if (frictionMagnitude < -maxFriction) frictionMagnitude = -maxFriction;
      float3 frictionImpulse = tangent * frictionMagnitude;
      ApplySceneImpulse(&entryA, contact.point, frictionImpulse);
      ApplySceneImpulse(&entryB, contact.point, -frictionImpulse);
   }
   return (!entryA.object ||
           (IsFiniteFloat3(entryA.object->position) && IsFiniteFloat3(entryA.runtime.velocity) &&
            IsFiniteFloat3(entryA.runtime.angularVelocity))) &&
          (!entryB.object ||
           (IsFiniteFloat3(entryB.object->position) && IsFiniteFloat3(entryB.runtime.velocity) &&
            IsFiniteFloat3(entryB.runtime.angularVelocity)));
}

namespace Detail
{

static const int32_t kMaximumBodyPairContacts = 2048;
static const int32_t kSolverManifoldContacts  = 8;

struct BodyPairContact {
   InternalContact   contact;
   const GameObject *other;
};

static int32_t GetContactShapeCount(const PhysicsSceneEntry &entry)
{
   if (!entry.collider || !entry.collider->IsValid()) return 0;
   if (entry.collider->GetKind() != ColliderKind::Convex) return 1;
   return Detail::ColliderAccess::GetShapeCount(*static_cast<ConvexCollider *>(entry.collider));
}

static bool TryBuildContactShape(const PhysicsSceneEntry &entry, int32_t partIndex,
                                 InternalContactShape *outShape)
{
   if (!outShape || !entry.object || !entry.collider) {
      return false;
   }

   if (entry.collider->GetKind() == ColliderKind::Box) {
      BoxCollider *collider = static_cast<BoxCollider *>(entry.collider);
      if (!collider->IsValid()) {
         return false;
      }
      if (partIndex != 0) return false;
      *outShape = Detail::ColliderAccess::From(*entry.object, *collider);
      return true;
   }

   if (entry.collider->GetKind() == ColliderKind::Convex) {
      ConvexCollider *collider = static_cast<ConvexCollider *>(entry.collider);
      if (!collider->IsValid()) {
         return false;
      }
      *outShape = Detail::ColliderAccess::GetShape(*entry.object, *collider, partIndex);
      return outShape->vertices != nullptr && outShape->vertexCount > 0;
   }

   if (entry.collider->GetKind() == ColliderKind::Mesh) {
      MeshCollider *collider = static_cast<MeshCollider *>(entry.collider);
      if (!collider->IsValid()) {
         return false;
      }
      if (partIndex != 0) return false;
      *outShape = Detail::ColliderAccess::From(*entry.object, *collider);
      return true;
   }

   return false;
}

static bool TryBuildContactShape(const PhysicsSceneEntry &entry, InternalContactShape *outShape)
{
   return TryBuildContactShape(entry, 0, outShape);
}

static void ProjectContactShape(const InternalContactShape &shape, float3 axis, float *minimum,
                                float *maximum)
{
   float3 first = shape.kind == InternalShapeKind::Box
                      ? float3((0 & 1) ? shape.halfExtents.x : -shape.halfExtents.x,
                               (0 & 2) ? shape.halfExtents.y : -shape.halfExtents.y,
                               (0 & 4) ? shape.halfExtents.z : -shape.halfExtents.z)
                      : shape.vertices[0];
   first        = shape.object->position + shape.object->orientation.rotate(first);
   *minimum = *maximum = float3::dot(first, axis);
   for (int32_t i = 1; i < shape.vertexCount; i++) {
      float3 local = shape.kind == InternalShapeKind::Box
                         ? float3((i & 1) ? shape.halfExtents.x : -shape.halfExtents.x,
                                  (i & 2) ? shape.halfExtents.y : -shape.halfExtents.y,
                                  (i & 4) ? shape.halfExtents.z : -shape.halfExtents.z)
                         : shape.vertices[i];
      float  projection =
          float3::dot(shape.object->position + shape.object->orientation.rotate(local), axis);
      if (projection < *minimum) *minimum = projection;
      if (projection > *maximum) *maximum = projection;
   }
}

static bool ShapeContainsPoint(const InternalContactShape &shape, float3 point)
{
   if (shape.kind == InternalShapeKind::StaticMesh || !shape.object || shape.vertexCount < 1)
      return false;
   const int32_t axisCount = shape.kind == InternalShapeKind::Box ? 3 : shape.faceAxisCount;
   for (int32_t axisIndex = 0; axisIndex < axisCount; axisIndex++) {
      float3 localAxis = shape.kind == InternalShapeKind::Box ? (axisIndex == 0   ? float3(1, 0, 0)
                                                                 : axisIndex == 1 ? float3(0, 1, 0)
                                                                                  : float3(0, 0, 1))
                                                              : shape.faceAxes[axisIndex];
      float3 axis      = shape.object->orientation.rotate(localAxis);
      float  minimum   = 0.0f;
      float  maximum   = 0.0f;
      ProjectContactShape(shape, axis, &minimum, &maximum);
      float projection = float3::dot(point, axis);
      if (projection <= minimum + 0.001f || projection >= maximum - 0.001f) return false;
   }
   return true;
}

static bool IsSiblingInteriorContact(const PhysicsSceneEntry &entry, int32_t sourcePart,
                                     float3 point, float3 outward)
{
   int32_t count = GetContactShapeCount(entry);
   if (count < 2) return false;
   for (int32_t part = 0; part < count; part++) {
      if (part == sourcePart) continue;
      InternalContactShape sibling;
      if (TryBuildContactShape(entry, part, &sibling) &&
          ShapeContainsPoint(sibling, point + outward * 0.002f))
         return true;
   }
   return false;
}

static bool ContactLess(const InternalContact &left, const InternalContact &right)
{
   if (left.normal.x != right.normal.x) return left.normal.x < right.normal.x;
   if (left.normal.y != right.normal.y) return left.normal.y < right.normal.y;
   if (left.normal.z != right.normal.z) return left.normal.z < right.normal.z;
   if (left.point.x != right.point.x) return left.point.x < right.point.x;
   if (left.point.y != right.point.y) return left.point.y < right.point.y;
   if (left.point.z != right.point.z) return left.point.z < right.point.z;
   return left.penetration > right.penetration;
}

static void SortContacts(InternalContact *contacts, int32_t count)
{
   for (int32_t i = 1; i < count; i++) {
      InternalContact value    = contacts[i];
      int32_t         position = i;
      while (position > 0 && ContactLess(value, contacts[position - 1])) {
         contacts[position] = contacts[position - 1];
         position--;
      }
      contacts[position] = value;
   }
}

static bool ContactRecordLess(const BodyPairContact &left, const BodyPairContact &right)
{
   if (ContactLess(left.contact, right.contact)) return true;
   if (ContactLess(right.contact, left.contact)) return false;
   return left.other < right.other;
}

static void SortContactRecords(BodyPairContact *contacts, int32_t count)
{
   for (int32_t i = 1; i < count; i++) {
      BodyPairContact value    = contacts[i];
      int32_t         position = i;
      while (position > 0 && ContactRecordLess(value, contacts[position - 1])) {
         contacts[position] = contacts[position - 1];
         position--;
      }
      contacts[position] = value;
   }
}

static void ReduceBodyPairContacts(InternalContact *contacts, int32_t *count, int32_t capacity)
{
   if (!contacts || !count || capacity <= 0) return;
   if (capacity > kSolverManifoldContacts) capacity = kSolverManifoldContacts;
   if (*count <= capacity) return;
   SortContacts(contacts, *count);
   InternalContact selected[kSolverManifoldContacts];
   bool            used[kMaximumBodyPairContacts] = {};
   int32_t         selectedCount                  = 0;
   for (int32_t i = 0; i < *count && selectedCount < capacity; i++) {
      bool familySeen = false;
      for (int32_t chosen = 0; chosen < selectedCount; chosen++)
         familySeen =
             familySeen || float3::dot(contacts[i].normal, selected[chosen].normal) > 0.98f;
      if (!familySeen) {
         selected[selectedCount++] = contacts[i];
         used[i]                   = true;
      }
   }
   while (selectedCount < capacity) {
      int32_t best      = -1;
      float   bestScore = -1.0f;
      for (int32_t i = 0; i < *count; i++) {
         if (used[i]) continue;
         float nearest = (contacts[i].point - selected[0].point).lengthSq();
         for (int32_t chosen = 1; chosen < selectedCount; chosen++) {
            float distance = (contacts[i].point - selected[chosen].point).lengthSq();
            if (distance < nearest) nearest = distance;
         }
         float score = nearest + contacts[i].penetration * 0.01f;
         if (score > bestScore) {
            bestScore = score;
            best      = i;
         }
      }
      if (best < 0) break;
      selected[selectedCount++] = contacts[best];
      used[best]                = true;
   }
   for (int32_t i = 0; i < selectedCount; i++) contacts[i] = selected[i];
   *count = selectedCount;
}

static int32_t CollectBodyPairContacts(const PhysicsSceneEntry &entryA,
                                       const PhysicsSceneEntry &entryB, InternalContact *contacts,
                                       int32_t capacity)
{
   if (!contacts || capacity <= 0) return 0;
   int32_t count  = 0;
   int32_t partsA = GetContactShapeCount(entryA);
   int32_t partsB = GetContactShapeCount(entryB);
   for (int32_t partA = 0; partA < partsA; partA++) {
      InternalContactShape shapeA;
      if (!TryBuildContactShape(entryA, partA, &shapeA)) continue;
      for (int32_t partB = 0; partB < partsB; partB++) {
         InternalContactShape shapeB;
         if (!TryBuildContactShape(entryB, partB, &shapeB)) continue;
         InternalContact pairContacts[8];
         int32_t         pairCount = GenerateInternalContacts(shapeA, shapeB, pairContacts, 8);
         for (int32_t i = 0; i < pairCount; i++) {
            if (IsSiblingInteriorContact(entryA, partA, pairContacts[i].point,
                                         -pairContacts[i].normal) ||
                IsSiblingInteriorContact(entryB, partB, pairContacts[i].point,
                                         pairContacts[i].normal))
               continue;
            bool duplicate = false;
            for (int32_t prior = 0; prior < count; prior++) {
               if ((contacts[prior].point - pairContacts[i].point).lengthSq() < 0.000001f &&
                   float3::dot(contacts[prior].normal, pairContacts[i].normal) > 0.9999f) {
                  duplicate = true;
                  if (pairContacts[i].penetration > contacts[prior].penetration)
                     contacts[prior] = pairContacts[i];
                  break;
               }
            }
            if (!duplicate && count < capacity) contacts[count++] = pairContacts[i];
         }
      }
   }
   SortContacts(contacts, count);
   return count;
}

class PhysicsStepper
{
 public:
   static PhysicsStepResult Step(PhysicsSceneState *scene, float sceneDrag, float deltaTime,
                                 int32_t iterations)
   {
      if (!scene || !scene->snapshots || !IsFiniteFloat(deltaTime) || deltaTime <= 0.0f ||
          iterations < 1 || iterations > 16) {
         return PhysicsStepResult::InvalidInput;
      }
      if (!CaptureAndValidate(scene)) return PhysicsStepResult::InvalidBodyState;
      if (!IntegrateBodies(scene, sceneDrag, deltaTime) || !ResolveContacts(scene, iterations)) {
         Restore(scene);
         return PhysicsStepResult::InvalidBodyState;
      }
      ClearBodyAccumulators(scene);
      return PhysicsStepResult::Completed;
   }

 private:
   static bool CaptureAndValidate(PhysicsSceneState *scene)
   {
      for (int32_t i = 0; i < scene->entryCount; i++) {
         PhysicsSceneEntry &entry = scene->entries[i];
         if (!entry.object || !IsFiniteFloat3(entry.object->position) ||
             !IsNormalizedQuat(entry.object->orientation) ||
             !IsFiniteFloat3(entry.runtime.velocity) ||
             !IsFiniteFloat3(entry.runtime.angularVelocity) ||
             !IsFiniteFloat3(entry.runtime.force) || !IsFiniteFloat3(entry.runtime.torque))
            return false;
         scene->snapshots[i].position    = entry.object->position;
         scene->snapshots[i].orientation = entry.object->orientation;
         scene->snapshots[i].runtime     = entry.runtime;
      }
      return true;
   }

   static void Restore(PhysicsSceneState *scene)
   {
      for (int32_t i = 0; i < scene->entryCount; i++) {
         scene->entries[i].object->position    = scene->snapshots[i].position;
         scene->entries[i].object->orientation = scene->snapshots[i].orientation;
         scene->entries[i].runtime             = scene->snapshots[i].runtime;
      }
   }

   static bool IntegrateBodies(PhysicsSceneState *scene, float sceneDrag, float deltaTime)
   {
      for (int32_t i = 0; i < scene->entryCount; i++) {
         if (!IntegrateSceneEntry(scene->entries[i], deltaTime, sceneDrag, scene->gravity))
            return false;
      }
      return true;
   }

   static bool ResolveContacts(PhysicsSceneState *scene, int32_t iterations)
   {
      for (int32_t iter = 0; iter < iterations; iter++) {
         for (int32_t a = 0; a < scene->entryCount; a++) {
            for (int32_t b = a + 1; b < scene->entryCount; b++) {
               if (!ResolveEntryPair(scene->entries[a], scene->entries[b])) {
                  return false;
               }
            }
         }
      }
      return true;
   }

   static bool ResolveEntryPair(PhysicsSceneEntry &entryA, PhysicsSceneEntry &entryB)
   {
      if (!IsDynamicKind(entryA.motion) && !IsDynamicKind(entryB.motion)) {
         return true;
      }

      InternalContact contacts[kMaximumBodyPairContacts];
      int32_t count = CollectBodyPairContacts(entryA, entryB, contacts, kMaximumBodyPairContacts);
      ReduceBodyPairContacts(contacts, &count, kSolverManifoldContacts);
      for (int32_t c = 0; c < count; c++) {
         if (!ResolveSceneContact(entryA, entryB, contacts[c])) return false;
      }
      return true;
   }

   static void ClearBodyAccumulators(PhysicsSceneState *scene)
   {
      for (int32_t i = 0; i < scene->entryCount; i++) {
         if (IsDynamicKind(scene->entries[i].motion)) {
            scene->entries[i].runtime.force  = {0, 0, 0};
            scene->entries[i].runtime.torque = {0, 0, 0};
         }
      }
   }
};

} // namespace Detail

PhysicsStepResult PhysicsScene::Step(float deltaTime, int32_t iterations)
{
   return Detail::PhysicsStepper::Step(m_impl, GetDrag(), deltaTime, iterations);
}

int32_t PhysicsScene::GetContacts(const GameObject &object, PhysicsContact *output,
                                  int32_t capacity) const
{
   PhysicsSceneEntry *entry = m_impl ? m_impl->Find(object) : nullptr;
   if (!entry || !output || capacity <= 0) return 0;
   Detail::BodyPairContact allContacts[Detail::kMaximumBodyPairContacts];
   int32_t                 allCount = 0;
   for (int32_t i = 0; i < m_impl->entryCount; i++) {
      const PhysicsSceneEntry &other = m_impl->entries[i];
      if (&other == entry) continue;
      InternalContact contacts[Detail::kMaximumBodyPairContacts];
      int32_t         count = Detail::CollectBodyPairContacts(*entry, other, contacts,
                                                              Detail::kMaximumBodyPairContacts);
      for (int32_t contact = 0; contact < count && allCount < Detail::kMaximumBodyPairContacts;
           contact++) {
         allContacts[allCount++] = {contacts[contact], other.object};
      }
   }
   Detail::SortContactRecords(allContacts, allCount);
   int32_t written = allCount < capacity ? allCount : capacity;
   for (int32_t contact = 0; contact < written; contact++) {
      output[contact].penetration = allContacts[contact].contact.penetration;
      output[contact].normal      = allContacts[contact].contact.normal;
      output[contact].point       = allContacts[contact].contact.point;
      output[contact].other       = allContacts[contact].other;
   }
   return written;
}

} // namespace HE3D
