#include "he3d_internal.hpp"

namespace HE3D
{

enum class ColliderEntryKind { Empty, Box, Convex, StaticMesh };

struct PhysicsSceneEntry {
   GameObject       *object;
   PhysicsBody      *body;
   void             *collider;
   ColliderEntryKind kind;

   PhysicsSceneEntry()
       : object(nullptr), body(nullptr), collider(nullptr), kind(ColliderEntryKind::Empty)
   {
   }
};

struct PhysicsSceneState {
   PhysicsSceneEntry *entries;
   int32_t            entryCount;
   int32_t            entryCapacity;

   explicit PhysicsSceneState(int32_t capacity) : entries(nullptr), entryCount(0), entryCapacity(0)
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

PhysicsBody::PhysicsBody()
    : object(nullptr), velocity{0, 0, 0}, angularVelocity{0, 0, 0}, force{0, 0, 0}, torque{0, 0, 0},
      gravity{0, -9.8f, 0}, mass(1.0f), inverseMass(1.0f), inertia(1.0f), inverseInertia(1.0f),
      restitution(0.0f), friction(0.5f), damping(0.0f), m_enabled(false), m_inertiaManual(false)
{
}

PhysicsBody::PhysicsBody(GameObject *gameObject)
    : object(gameObject), velocity{0, 0, 0}, angularVelocity{0, 0, 0}, force{0, 0, 0},
      torque{0, 0, 0}, gravity{0, -9.8f, 0}, mass(1.0f), inverseMass(1.0f), inertia(1.0f),
      inverseInertia(1.0f), restitution(0.0f), friction(0.5f), damping(0.0f), m_enabled(false),
      m_inertiaManual(false)
{
}

void PhysicsBody::BindGameObject(GameObject *gameObject) { object = gameObject; }

void PhysicsBody::SetEnabled(bool enabled) { m_enabled = enabled; }

bool PhysicsBody::IsEnabled() const { return m_enabled; }

bool PhysicsBody::UsesManualInertia() const { return m_inertiaManual; }

void PhysicsBody::SetMass(float value)
{
   mass            = value;
   inverseMass     = value > 0.0f ? 1.0f / value : 0.0f;
   m_inertiaManual = false;
}

void PhysicsBody::SetInertia(float value)
{
   inertia         = value;
   inverseInertia  = value > 0.0f ? 1.0f / value : 0.0f;
   m_inertiaManual = true;
}

void PhysicsBody::SetVelocity(float3 value) { velocity = value; }

void PhysicsBody::SetAngularVelocity(float3 value) { angularVelocity = value; }

void PhysicsBody::AddForce(float3 value) { force = force + value; }

void PhysicsBody::AddImpulse(float3 impulse) { velocity = velocity + impulse * inverseMass; }

void PhysicsBody::AddAngularImpulse(float3 impulse)
{
   angularVelocity = angularVelocity + impulse * inverseInertia;
}

void PhysicsBody::AddTorque(float3 value) { torque = torque + value; }

void PhysicsBody::ClearForces() { force = {0, 0, 0}; }

void PhysicsBody::ClearTorques() { torque = {0, 0, 0}; }

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

static float EstimateBoxInertia(float3 halfExtents, float mass)
{
   if (mass <= 0.0f) {
      return 0.0f;
   }

   float width  = halfExtents.x * 2.0f;
   float height = halfExtents.y * 2.0f;
   float depth  = halfExtents.z * 2.0f;
   if (width < 0.001f) width = 0.001f;
   if (height < 0.001f) height = 0.001f;
   if (depth < 0.001f) depth = 0.001f;

   float ix = mass * (height * height + depth * depth) / 12.0f;
   float iy = mass * (width * width + depth * depth) / 12.0f;
   float iz = mass * (width * width + height * height) / 12.0f;
   return (ix + iy + iz) / 3.0f;
}

static void UpdateAutomaticInertia(PhysicsBody *body, AABB localBounds)
{
   if (!body || body->UsesManualInertia()) {
      return;
   }
   float3 halfExtents      = (localBounds.max - localBounds.min) * 0.5f;
   float  estimatedInertia = EstimateBoxInertia(halfExtents, body->mass);
   body->inertia           = estimatedInertia;
   body->inverseInertia    = estimatedInertia > 0.0f ? 1.0f / estimatedInertia : 0.0f;
}

bool PhysicsScene::AddBody(GameObject *object, PhysicsBody *body, BoxCollider *collider)
{
   if (!m_impl || !object || !body || !collider || !collider->IsValid() || !m_impl->HasCapacity()) {
      return false;
   }
   body->object = object;
   UpdateAutomaticInertia(body, collider->LocalAABB());
   PhysicsSceneEntry &entry = m_impl->Append();
   entry.object             = object;
   entry.body               = body;
   entry.collider           = collider;
   entry.kind               = ColliderEntryKind::Box;
   return true;
}

bool PhysicsScene::AddBody(GameObject *object, PhysicsBody *body, ConvexCollider *collider)
{
   if (!m_impl || !object || !body || !collider || !collider->IsValid() || !m_impl->HasCapacity()) {
      return false;
   }
   body->object = object;
   UpdateAutomaticInertia(body, collider->LocalAABB());
   PhysicsSceneEntry &entry = m_impl->Append();
   entry.object             = object;
   entry.body               = body;
   entry.collider           = collider;
   entry.kind               = ColliderEntryKind::Convex;
   return true;
}

bool PhysicsScene::AddStatic(GameObject *object, StaticMeshCollider *collider)
{
   if (!m_impl || !object || !collider || !collider->IsValid() || !m_impl->HasCapacity()) {
      return false;
   }
   PhysicsSceneEntry &entry = m_impl->Append();
   entry.object             = object;
   entry.body               = nullptr;
   entry.collider           = collider;
   entry.kind               = ColliderEntryKind::StaticMesh;
   return true;
}

static void IntegrateSceneBody(GameObject *object, PhysicsBody *body, float deltaTime,
                               float sceneDrag)
{
   if (!object || !body || !body->IsEnabled() || deltaTime <= 0.0f) {
      return;
   }

   float3 totalForce     = body->force + body->gravity * body->mass;
   float3 totalTorque    = body->torque;
   body->velocity        = body->velocity + totalForce * body->inverseMass * deltaTime;
   body->angularVelocity = body->angularVelocity + totalTorque * body->inverseInertia * deltaTime;

   if (body->damping > 0.0f) {
      float damp = 1.0f - body->damping * deltaTime;
      if (damp < 0.0f) damp = 0.0f;
      body->velocity        = body->velocity * damp;
      body->angularVelocity = body->angularVelocity * damp;
   }

   if (sceneDrag > 0.0f) {
      float dragScale = 1.0f - sceneDrag * deltaTime;
      if (dragScale < 0.0f) dragScale = 0.0f;
      body->velocity        = body->velocity * dragScale;
      body->angularVelocity = body->angularVelocity * dragScale;
   }

   object->position = object->position + body->velocity * deltaTime;
   quat dq =
       quat(1.0f, 0.5f * body->angularVelocity.x * deltaTime,
            0.5f * body->angularVelocity.y * deltaTime, 0.5f * body->angularVelocity.z * deltaTime);
   object->orientation = (object->orientation * dq).normalizeFast();
}

static float BodyInverseMass(const PhysicsBody *body)
{
   return body && body->IsEnabled() ? body->inverseMass : 0.0f;
}

static float BodyInverseInertia(const PhysicsBody *body)
{
   return body && body->IsEnabled() ? body->inverseInertia : 0.0f;
}

static float3 BodyVelocityAtPoint(const GameObject *object, const PhysicsBody *body, float3 point)
{
   if (!object || !body || !body->IsEnabled()) {
      return {0, 0, 0};
   }
   float3 r = point - object->position;
   return body->velocity + float3::cross(body->angularVelocity, r);
}

static void ApplySceneImpulse(GameObject *object, PhysicsBody *body, float3 point, float3 impulse)
{
   if (!object || !body || !body->IsEnabled()) {
      return;
   }
   body->velocity        = body->velocity + impulse * body->inverseMass;
   float3 r              = point - object->position;
   body->angularVelocity = body->angularVelocity + float3::cross(r, impulse) * body->inverseInertia;
}

static void ResolveSceneContact(GameObject *objectA, PhysicsBody *bodyA, GameObject *objectB,
                                PhysicsBody *bodyB, const InternalContact &contact)
{
   if (!contact.hit || contact.penetration <= 0.0f) {
      return;
   }

   float invMassA   = BodyInverseMass(bodyA);
   float invMassB   = BodyInverseMass(bodyB);
   float invMassSum = invMassA + invMassB;
   if (invMassSum <= 0.0f) {
      return;
   }

   const float slop            = 0.0005f;
   float       correctionDepth = contact.penetration - slop;
   if (correctionDepth > 0.0f) {
      float  correctionMagnitude = correctionDepth * 0.6f / invMassSum;
      float3 correction          = contact.normal * correctionMagnitude;
      if (objectA) {
         objectA->position = objectA->position + correction * invMassA;
      }
      if (objectB) {
         objectB->position = objectB->position - correction * invMassB;
      }
   }

   float3 relativeVelocity = BodyVelocityAtPoint(objectA, bodyA, contact.point) -
                             BodyVelocityAtPoint(objectB, bodyB, contact.point);
   float  normalVelocity   = float3::dot(relativeVelocity, contact.normal);

   float inertiaTerm = invMassSum;
   if (objectA) {
      float3 ra       = contact.point - objectA->position;
      float3 raCrossN = float3::cross(ra, contact.normal);
      inertiaTerm += raCrossN.lengthSq() * BodyInverseInertia(bodyA);
   }
   if (objectB) {
      float3 rb       = contact.point - objectB->position;
      float3 rbCrossN = float3::cross(rb, contact.normal);
      inertiaTerm += rbCrossN.lengthSq() * BodyInverseInertia(bodyB);
   }
   if (inertiaTerm <= 0.000001f) {
      return;
   }

   float impulseMagnitude = 0.0f;
   if (normalVelocity < 0.0f) {
      float restitutionA = bodyA ? bodyA->restitution : 0.0f;
      float restitutionB = bodyB ? bodyB->restitution : 0.0f;
      float restitution  = restitutionA > restitutionB ? restitutionA : restitutionB;
      if (-normalVelocity < 0.35f) {
         restitution = 0.0f;
      }
      impulseMagnitude     = -((1.0f + restitution) * normalVelocity) / inertiaTerm;
      float3 normalImpulse = contact.normal * impulseMagnitude;
      ApplySceneImpulse(objectA, bodyA, contact.point, normalImpulse);
      ApplySceneImpulse(objectB, bodyB, contact.point, -normalImpulse);
   }

   float frictionA        = bodyA ? bodyA->friction : 0.5f;
   float frictionB        = bodyB ? bodyB->friction : 0.5f;
   float friction         = (frictionA + frictionB) * 0.5f;
   relativeVelocity       = BodyVelocityAtPoint(objectA, bodyA, contact.point) -
                            BodyVelocityAtPoint(objectB, bodyB, contact.point);
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
      ApplySceneImpulse(objectA, bodyA, contact.point, frictionImpulse);
      ApplySceneImpulse(objectB, bodyB, contact.point, -frictionImpulse);
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

   if (entry.kind == ColliderEntryKind::StaticMesh) {
      StaticMeshCollider *collider = static_cast<StaticMeshCollider *>(entry.collider);
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
         IntegrateSceneBody(scene->entries[i].object, scene->entries[i].body, deltaTime, sceneDrag);
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
      if (!entryA.body && !entryB.body) {
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
         ResolveSceneContact(entryA.object, entryA.body, entryB.object, entryB.body, contacts[c]);
      }
   }

   static void ClearBodyAccumulators(PhysicsSceneState *scene)
   {
      for (int32_t i = 0; i < scene->entryCount; i++) {
         if (scene->entries[i].body) {
            scene->entries[i].body->ClearForces();
            scene->entries[i].body->ClearTorques();
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
