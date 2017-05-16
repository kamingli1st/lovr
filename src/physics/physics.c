#include "physics.h"
#include "math/quat.h"
#include <stdlib.h>

void lovrPhysicsInit() {
  dInitODE();

  if (!dCheckConfiguration("ODE_single_precision")) {
    error("lovr.physics must use single precision");
  }

  atexit(lovrPhysicsDestroy);
}

void lovrPhysicsDestroy() {
  dCloseODE();
}

World* lovrWorldCreate() {
  World* world = lovrAlloc(sizeof(World), lovrWorldDestroy);
  if (!world) return NULL;

  world->id = dWorldCreate();

  return world;
}

void lovrWorldDestroy(const Ref* ref) {
  World* world = containerof(ref, World);
  dWorldDestroy(world->id);
  free(world);
}

void lovrWorldGetGravity(World* world, float* x, float* y, float* z) {
  dReal gravity[3];
  dWorldGetGravity(world->id, gravity);
  *x = gravity[0];
  *y = gravity[1];
  *z = gravity[2];
}

void lovrWorldSetGravity(World* world, float x, float y, float z) {
  dWorldSetGravity(world->id, x, y, z);
}

void lovrWorldGetLinearDamping(World* world, float* damping, float* threshold) {
  *damping = dWorldGetLinearDamping(world->id);
  *threshold = dWorldGetLinearDampingThreshold(world->id);
}

void lovrWorldSetLinearDamping(World* world, float damping, float threshold) {
  dWorldSetLinearDamping(world->id, damping);
  dWorldSetLinearDampingThreshold(world->id, threshold);
}

void lovrWorldGetAngularDamping(World* world, float* damping, float* threshold) {
  *damping = dWorldGetAngularDamping(world->id);
  *threshold = dWorldGetAngularDampingThreshold(world->id);
}

void lovrWorldSetAngularDamping(World* world, float damping, float threshold) {
  dWorldSetAngularDamping(world->id, damping);
  dWorldSetAngularDampingThreshold(world->id, threshold);
}

int lovrWorldIsSleepingAllowed(World* world) {
  return dWorldGetAutoDisableFlag(world->id);
}

void lovrWorldSetSleepingAllowed(World* world, int allowed) {
  dWorldSetAutoDisableFlag(world->id, allowed);
}

void lovrWorldUpdate(World* world, float dt) {
  dWorldQuickStep(world->id, dt);
}

Body* lovrBodyCreate(World* world) {
  if (!world) {
    error("No world specified");
  }

  Body* body = lovrAlloc(sizeof(Body), lovrBodyDestroy);
  if (!body) return NULL;

  body->id = dBodyCreate(world->id);
  body->world = world;

  return body;
}

void lovrBodyDestroy(const Ref* ref) {
  Body* body = containerof(ref, Body);
  dBodyDestroy(body->id);
  free(body);
}

void lovrBodyGetPosition(Body* body, float* x, float* y, float* z) {
  const dReal* position = dBodyGetPosition(body->id);
  *x = position[0];
  *y = position[1];
  *z = position[2];
}

void lovrBodySetPosition(Body* body, float x, float y, float z) {
  dBodySetPosition(body->id, x, y, z);
}

void lovrBodyGetOrientation(Body* body, float* angle, float* x, float* y, float* z) {
  const dReal* q = dBodyGetQuaternion(body->id);
  float quaternion[4] = { q[0], q[1], q[2], q[3] };
  quat_getAngleAxis(quaternion, angle, x, y, z);
}

void lovrBodySetOrientation(Body* body, float angle, float x, float y, float z) {
  float axis[3] = { x, y, z };
  float quaternion[4];
  quat_fromAngleAxis(quaternion, angle, axis);
  dBodySetQuaternion(body->id, quaternion);
}

void lovrBodyGetLinearVelocity(Body* body, float* x, float* y, float* z) {
  const dReal* velocity = dBodyGetLinearVel(body->id);
  *x = velocity[0];
  *y = velocity[1];
  *z = velocity[2];
}

void lovrBodySetLinearVelocity(Body* body, float x, float y, float z) {
  dBodySetLinearVel(body->id, x, y, z);
}

void lovrBodyGetAngularVelocity(Body* body, float* x, float* y, float* z) {
  const dReal* velocity = dBodyGetAngularVel(body->id);
  *x = velocity[0];
  *y = velocity[1];
  *z = velocity[2];
}

void lovrBodySetAngularVelocity(Body* body, float x, float y, float z) {
  dBodySetAngularVel(body->id, x, y, z);
}

void lovrBodyGetLinearDamping(Body* body, float* damping, float* threshold) {
  *damping = dBodyGetLinearDamping(body->id);
  *threshold = dBodyGetLinearDampingThreshold(body->id);
}

void lovrBodySetLinearDamping(Body* body, float damping, float threshold) {
  dBodySetLinearDamping(body->id, damping);
  dBodySetLinearDampingThreshold(body->id, threshold);
}