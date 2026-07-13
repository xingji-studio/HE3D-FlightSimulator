#include "he3d_internal.hpp"

namespace HE3D
{

enum class ColliderEntryKind { Empty, Box, Convex, Mesh };

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

struct PhysicsSceneEntry {
   GameObject         *object;
   PhysicsProperties  *properties;
   void               *collider;
   ColliderEntryKind   kind;
   PhysicsRuntimeState runtime;

   PhysicsSceneEntry()
       : object(nullptr), properties(nullptr), collider(nullptr), kind(ColliderEntryKind::Empty),
         runtime()
   {
   }
};

struct PhysicsSceneState {
   PhysicsSceneEntry *entries;
   int32_t            entryCount;
   int32_t            entryCapacity;
   float3             gravity;

   explicit PhysicsSceneState(int32_t capacity)
       : entries(nullptr), entryCount(0), entryCapacity(0), gravity{0, -9.8f, 0}
   {
      if (capacity < 1) {
         capacity = 1;
      }
      entries = new PhysicsSceneEntry[capacity];
      if (entries) {
         entryCapacity = capacity;
      }
   }

   ~PhysicsSceneState() { delete[] entries; }

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

PhysicsProperties::PhysicsProperties()
    : m_mass(1.0f), m_inertia(1.0f), m_friction(0.5f), m_restitution(0.0f), m_damping(0.0f)
{
}

float PhysicsProperties::GetMass() const { return m_mass; }
float PhysicsProperties::GetInertia() const { return m_inertia; }
float PhysicsProperties::GetFriction() const { return m_friction; }
float PhysicsProperties::GetRestitution() const { return m_restitution; }
float PhysicsProperties::GetDamping() const { return m_damping; }

PhysicsSettingResult PhysicsProperties::SetMass(float value)
{
   if (value <= 0.0f) {
      return PhysicsSettingResult::Rejected;
   }
   m_mass = value;
   return PhysicsSettingResult::Applied;
}

PhysicsSettingResult PhysicsProperties::SetInertia(float value)
{
   if (value <= 0.0f) {
      return PhysicsSettingResult::Rejected;
   }
   m_inertia = value;
   return PhysicsSettingResult::Applied;
}

PhysicsSettingResult PhysicsProperties::SetFriction(float value)
{
   if (value < 0.0f) {
      return PhysicsSettingResult::Rejected;
   }
   m_friction = value;
   return PhysicsSettingResult::Applied;
}

PhysicsSettingResult PhysicsProperties::SetRestitution(float value)
{
   if (value < 0.0f) {
      return PhysicsSettingResult::Rejected;
   }
   m_restitution = value;
   return value > 1.0f ? PhysicsSettingResult::AppliedWithWarning : PhysicsSettingResult::Applied;
}

PhysicsSettingResult PhysicsProperties::SetDamping(float value)
{
   if (value < 0.0f) {
      return PhysicsSettingResult::Rejected;
   }
   m_damping = value;
   return PhysicsSettingResult::Applied;
}

PhysicsScene::PhysicsScene(int32_t capacity) : drag(0.02f), m_impl(new PhysicsSceneState(capacity))
{
}

PhysicsScene::~PhysicsScene() { delete m_impl; }

void PhysicsScene::Clear()
{
   if (m_impl) {
      m_impl->entryCount = 0;
   }
}

static bool IsDynamicKind(ColliderEntryKind kind)
{
   return kind == ColliderEntryKind::Box || kind == ColliderEntryKind::Convex;
}

static float InverseMass(const PhysicsSceneEntry *entry)
{
   if (!entry || !IsDynamicKind(entry->kind) || !entry->properties ||
       entry->properties->GetMass() <= 0.0f) {
      return 0.0f;
   }
   return 1.0f / entry->properties->GetMass();
}

static float InverseInertia(const PhysicsSceneEntry *entry)
{
   if (!entry || !IsDynamicKind(entry->kind) || !entry->properties ||
       entry->properties->GetInertia() <= 0.0f) {
      return 0.0f;
   }
   return 1.0f / entry->properties->GetInertia();
}

bool PhysicsScene::AddStaticBody(GameObject &object, MeshCollider &collider)
{
   if (!m_impl || !collider.IsValid() || !m_impl->HasCapacity() || m_impl->Find(object)) {
      return false;
   }
   PhysicsSceneEntry &entry = m_impl->Append();
   entry.object             = &object;
   entry.properties         = nullptr;
   entry.collider           = &collider;
   entry.kind               = ColliderEntryKind::Mesh;
   entry.runtime            = PhysicsRuntimeState();
   return true;
}

bool PhysicsScene::AddDynamicBody(GameObject &object, BoxCollider &collider,
                                  PhysicsProperties &properties)
{
   if (!m_impl || !collider.IsValid() || !m_impl->HasCapacity() || m_impl->Find(object)) {
      return false;
   }
   PhysicsSceneEntry &entry = m_impl->Append();
   entry.object             = &object;
   entry.properties         = &properties;
   entry.collider           = &collider;
   entry.kind               = ColliderEntryKind::Box;
   entry.runtime            = PhysicsRuntimeState();
   return true;
}

bool PhysicsScene::AddDynamicBody(GameObject &object, ConvexCollider &collider,
                                  PhysicsProperties &properties)
{
   if (!m_impl || !collider.IsValid() || !m_impl->HasCapacity() || m_impl->Find(object)) {
      return false;
   }
   PhysicsSceneEntry &entry = m_impl->Append();
   entry.object             = &object;
   entry.properties         = &properties;
   entry.collider           = &collider;
   entry.kind               = ColliderEntryKind::Convex;
   entry.runtime            = PhysicsRuntimeState();
   return true;
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

bool PhysicsScene::RefreshCollider(GameObject &object)
{
   PhysicsSceneEntry *entry = m_impl ? m_impl->Find(object) : nullptr;
   if (!entry) {
      return false;
   }
   if (entry->kind == ColliderEntryKind::Mesh) {
      static_cast<MeshCollider *>(entry->collider)->Refresh();
      return static_cast<MeshCollider *>(entry->collider)->IsValid();
   }
   return true;
}

bool PhysicsScene::HasBody(const GameObject &object) const
{
   return m_impl && m_impl->Find(object);
}

bool PhysicsScene::IsDynamicBody(const GameObject &object) const
{
   const PhysicsSceneEntry *entry = m_impl ? m_impl->Find(object) : nullptr;
   return entry && IsDynamicKind(entry->kind);
}

void PhysicsScene::SetGravity(float3 value)
{
   if (m_impl) {
      m_impl->gravity = value;
   }
}

float3 PhysicsScene::GetGravity() const { return m_impl ? m_impl->gravity : float3(0, 0, 0); }

float3 PhysicsScene::GetVelocity(const GameObject &object) const
{
   const PhysicsSceneEntry *entry = m_impl ? m_impl->Find(object) : nullptr;
   return entry && IsDynamicKind(entry->kind) ? entry->runtime.velocity : float3(0, 0, 0);
}

float3 PhysicsScene::GetAngularVelocity(const GameObject &object) const
{
   const PhysicsSceneEntry *entry = m_impl ? m_impl->Find(object) : nullptr;
   return entry && IsDynamicKind(entry->kind) ? entry->runtime.angularVelocity : float3(0, 0, 0);
}

bool PhysicsScene::SetVelocity(GameObject &object, const float3 &velocity)
{
   PhysicsSceneEntry *entry = m_impl ? m_impl->Find(object) : nullptr;
   if (!entry || !IsDynamicKind(entry->kind)) {
      return false;
   }
   entry->runtime.velocity = velocity;
   return true;
}

bool PhysicsScene::SetAngularVelocity(GameObject &object, const float3 &angularVelocity)
{
   PhysicsSceneEntry *entry = m_impl ? m_impl->Find(object) : nullptr;
   if (!entry || !IsDynamicKind(entry->kind)) {
      return false;
   }
   entry->runtime.angularVelocity = angularVelocity;
   return true;
}

bool PhysicsScene::AddForce(GameObject &object, const float3 &force)
{
   PhysicsSceneEntry *entry = m_impl ? m_impl->Find(object) : nullptr;
   if (!entry || !IsDynamicKind(entry->kind)) {
      return false;
   }
   entry->runtime.force = entry->runtime.force + force;
   return true;
}

bool PhysicsScene::AddTorque(GameObject &object, const float3 &torque)
{
   PhysicsSceneEntry *entry = m_impl ? m_impl->Find(object) : nullptr;
   if (!entry || !IsDynamicKind(entry->kind)) {
      return false;
   }
   entry->runtime.torque = entry->runtime.torque + torque;
   return true;
}

bool PhysicsScene::AddImpulse(GameObject &object, const float3 &impulse)
{
   PhysicsSceneEntry *entry = m_impl ? m_impl->Find(object) : nullptr;
   if (!entry || !IsDynamicKind(entry->kind)) {
      return false;
   }
   entry->runtime.velocity = entry->runtime.velocity + impulse * InverseMass(entry);
   return true;
}

bool PhysicsScene::AddAngularImpulse(GameObject &object, const float3 &impulse)
{
   PhysicsSceneEntry *entry = m_impl ? m_impl->Find(object) : nullptr;
   if (!entry || !IsDynamicKind(entry->kind)) {
      return false;
   }
   entry->runtime.angularVelocity =
       entry->runtime.angularVelocity + impulse * InverseInertia(entry);
   return true;
}

static void IntegrateSceneEntry(PhysicsSceneEntry &entry, float deltaTime, float sceneDrag,
                                float3 gravity)
{
   if (!entry.object || !entry.properties || !IsDynamicKind(entry.kind) || deltaTime <= 0.0f) {
      return;
   }

   float  inverseMass     = InverseMass(&entry);
   float  inverseInertia  = InverseInertia(&entry);
   float3 totalForce      = entry.runtime.force + gravity * entry.properties->GetMass();
   entry.runtime.velocity = entry.runtime.velocity + totalForce * inverseMass * deltaTime;
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
}

static float3 EntryVelocityAtPoint(const PhysicsSceneEntry *entry, float3 point)
{
   if (!entry || !entry->object || !IsDynamicKind(entry->kind)) {
      return {0, 0, 0};
   }
   float3 r = point - entry->object->position;
   return entry->runtime.velocity + float3::cross(entry->runtime.angularVelocity, r);
}

static void ApplySceneImpulse(PhysicsSceneEntry *entry, float3 point, float3 impulse)
{
   if (!entry || !entry->object || !IsDynamicKind(entry->kind)) {
      return;
   }
   entry->runtime.velocity = entry->runtime.velocity + impulse * InverseMass(entry);
   float3 r                = point - entry->object->position;
   entry->runtime.angularVelocity =
       entry->runtime.angularVelocity + float3::cross(r, impulse) * InverseInertia(entry);
}

static void ResolveSceneContact(PhysicsSceneEntry &entryA, PhysicsSceneEntry &entryB,
                                const InternalContact &contact)
{
   if (!contact.hit || contact.penetration <= 0.0f) {
      return;
   }

   float invMassA   = InverseMass(&entryA);
   float invMassB   = InverseMass(&entryB);
   float invMassSum = invMassA + invMassB;
   if (invMassSum <= 0.0f) {
      return;
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
      return;
   }

   float impulseMagnitude = 0.0f;
   if (normalVelocity < 0.0f) {
      float restitutionA = entryA.properties ? entryA.properties->GetRestitution() : 0.0f;
      float restitutionB = entryB.properties ? entryB.properties->GetRestitution() : 0.0f;
      float restitution  = restitutionA > restitutionB ? restitutionA : restitutionB;
      if (-normalVelocity < 0.35f) {
         restitution = 0.0f;
      }
      impulseMagnitude     = -((1.0f + restitution) * normalVelocity) / inertiaTerm;
      float3 normalImpulse = contact.normal * impulseMagnitude;
      ApplySceneImpulse(&entryA, contact.point, normalImpulse);
      ApplySceneImpulse(&entryB, contact.point, -normalImpulse);
   }

   float frictionA = entryA.properties ? entryA.properties->GetFriction() : 0.5f;
   float frictionB = entryB.properties ? entryB.properties->GetFriction() : 0.5f;
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
}

namespace Detail
{

static bool TryBuildContactShape(const PhysicsSceneEntry &entry, InternalContactShape *outShape)
{
   if (!outShape || !entry.object || !entry.collider) {
      return false;
   }

   if (entry.kind == ColliderEntryKind::Box) {
      BoxCollider *collider = static_cast<BoxCollider *>(entry.collider);
      if (!collider->IsValid()) {
         return false;
      }
      *outShape = Detail::ColliderAccess::From(*entry.object, *collider);
      return true;
   }

   if (entry.kind == ColliderEntryKind::Convex) {
      ConvexCollider *collider = static_cast<ConvexCollider *>(entry.collider);
      if (!collider->IsValid()) {
         return false;
      }
      *outShape = Detail::ColliderAccess::From(*entry.object, *collider);
      return true;
   }

   if (entry.kind == ColliderEntryKind::Mesh) {
      MeshCollider *collider = static_cast<MeshCollider *>(entry.collider);
      if (!collider->IsValid()) {
         return false;
      }
      *outShape = Detail::ColliderAccess::From(*entry.object, *collider);
      return true;
   }

   return false;
}

class PhysicsStepper
{
 public:
   static void Step(PhysicsSceneState *scene, float sceneDrag, float deltaTime, int32_t iterations)
   {
      if (!scene || deltaTime <= 0.0f) {
         return;
      }

      if (iterations < 1) iterations = 1;
      if (iterations > 16) iterations = 16;

      IntegrateBodies(scene, sceneDrag, deltaTime);
      ResolveContacts(scene, iterations);
      ClearBodyAccumulators(scene);
   }

 private:
   static void IntegrateBodies(PhysicsSceneState *scene, float sceneDrag, float deltaTime)
   {
      for (int32_t i = 0; i < scene->entryCount; i++) {
         IntegrateSceneEntry(scene->entries[i], deltaTime, sceneDrag, scene->gravity);
      }
   }

   static void ResolveContacts(PhysicsSceneState *scene, int32_t iterations)
   {
      for (int32_t iter = 0; iter < iterations; iter++) {
         for (int32_t a = 0; a < scene->entryCount; a++) {
            for (int32_t b = a + 1; b < scene->entryCount; b++) {
               ResolveEntryPair(scene->entries[a], scene->entries[b]);
            }
         }
      }
   }

   static void ResolveEntryPair(PhysicsSceneEntry &entryA, PhysicsSceneEntry &entryB)
   {
      if (!IsDynamicKind(entryA.kind) && !IsDynamicKind(entryB.kind)) {
         return;
      }

      InternalContactShape shapeA;
      InternalContactShape shapeB;
      if (!TryBuildContactShape(entryA, &shapeA) || !TryBuildContactShape(entryB, &shapeB)) {
         return;
      }

      InternalContact contacts[8];
      int32_t         count = GenerateInternalContacts(shapeA, shapeB, contacts, 8);
      for (int32_t c = 0; c < count; c++) {
         ResolveSceneContact(entryA, entryB, contacts[c]);
      }
   }

   static void ClearBodyAccumulators(PhysicsSceneState *scene)
   {
      for (int32_t i = 0; i < scene->entryCount; i++) {
         if (IsDynamicKind(scene->entries[i].kind)) {
            scene->entries[i].runtime.force  = {0, 0, 0};
            scene->entries[i].runtime.torque = {0, 0, 0};
         }
      }
   }
};

} // namespace Detail

void PhysicsScene::Step(float deltaTime, int32_t iterations)
{
   Detail::PhysicsStepper::Step(m_impl, drag, deltaTime, iterations);
}

} // namespace HE3D
