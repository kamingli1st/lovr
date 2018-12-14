#include "data/blob.h"
#include "data/textureData.h"
#include "data/vertexData.h"
#include "util.h"
#include "lib/map/map.h"
#include "lib/vec/vec.h"

#pragma once

#define MAX_BONES_PER_VERTEX 4
#define MAX_BONES 48

typedef enum {
  ATTR_POSITION,
  ATTR_NORMAL,
  ATTR_TEXCOORD,
  ATTR_COLOR,
  ATTR_TANGENT,
  ATTR_BONES,
  ATTR_WEIGHTS,
  MAX_DEFAULT_ATTRIBUTES
} DefaultAttribute;

typedef enum {
  DRAW_POINTS,
  DRAW_LINES,
  DRAW_LINE_LOOP,
  DRAW_LINE_STRIP,
  DRAW_TRIANGLES,
  DRAW_TRIANGLE_STRIP,
  DRAW_TRIANGLE_FAN
} DrawMode;

typedef enum { I8, U8, I16, U16, I32, U32, F32 } NumberType;

typedef struct {
  uint32_t magic;
  uint32_t version;
  uint32_t length;
} gltfHeader;

typedef struct {
  uint32_t length;
  uint32_t type;
} gltfChunkHeader;

typedef struct {
  char* data;
  size_t length;
} gltfString;

typedef struct {
  int bufferView;
  int count;
  int offset;
  NumberType type;
  int components : 3;
  int normalized : 1;
} gltfAccessor;

typedef struct {
  void* data;
  size_t size;
} gltfBuffer;

typedef struct {
  int buffer;
  int offset;
  int length;
  int stride;
} gltfBufferView;

typedef struct {
  DrawMode mode;
  int attributes[MAX_DEFAULT_ATTRIBUTES];
  int indices;
  int material;
} gltfPrimitive;

typedef struct {
  gltfPrimitive* primitives;
  uint32_t primitiveCount;
} gltfMesh;

typedef struct {
  float transform[16];
  uint32_t* children;
  uint32_t childCount;
  int mesh;
} gltfNode;

typedef struct {
  Ref ref;
  uint8_t* data;
  gltfAccessor* accessors;
  gltfBuffer* buffers;
  gltfBufferView* bufferViews;
  gltfMesh* meshes;
  gltfNode* nodes;
  gltfPrimitive* primitives;
  uint32_t* childMap;
  int accessorCount;
  int bufferCount;
  int bufferViewCount;
  int meshCount;
  int nodeCount;
  int primitiveCount;
} gltfModelData;

//

typedef struct {
  const char* name;
  float offset[16];
} Bone;

typedef struct {
  int material;
  int drawStart;
  int drawCount;
  Bone bones[MAX_BONES];
  map_int_t boneMap;
  int boneCount;
} ModelPrimitive;

typedef vec_t(unsigned int) vec_uint_t;

typedef struct ModelNode {
  const char* name;
  float transform[16];
  float globalTransform[16];
  int parent;
  vec_uint_t children;
  vec_uint_t primitives;
} ModelNode;

typedef struct {
  Color diffuseColor;
  Color emissiveColor;
  int diffuseTexture;
  int emissiveTexture;
  int metalnessTexture;
  int roughnessTexture;
  int occlusionTexture;
  int normalTexture;
  float metalness;
  float roughness;
} ModelMaterial;

typedef struct {
  double time;
  float data[4];
} Keyframe;

typedef vec_t(Keyframe) vec_keyframe_t;

typedef struct {
  const char* node;
  vec_keyframe_t positionKeyframes;
  vec_keyframe_t rotationKeyframes;
  vec_keyframe_t scaleKeyframes;
} AnimationChannel;

typedef map_t(AnimationChannel) map_channel_t;

typedef struct {
  const char* name;
  float duration;
  map_channel_t channels;
  int channelCount;
} Animation;

typedef struct {
  Ref ref;
  VertexData* vertexData;
  IndexPointer indices;
  int indexCount;
  size_t indexSize;
  ModelNode* nodes;
  map_int_t nodeMap;
  ModelPrimitive* primitives;
  Animation* animations;
  ModelMaterial* materials;
  vec_void_t textures;
  int nodeCount;
  int primitiveCount;
  int animationCount;
  int materialCount;
} ModelData;

typedef struct {
  void* (*read)(const char* path, size_t* bytesRead);
} ModelDataIO;

ModelData* lovrModelDataInit(ModelData* modelData, Blob* blob);
ModelData* lovrModelDataInitEmpty(ModelData* modelData);
gltfModelData* lovrModelDataInitFromGltf(gltfModelData* modelData, Blob* blob, ModelDataIO io);
#define lovrModelDataCreate(...) lovrModelDataInit(lovrAlloc(ModelData), __VA_ARGS__)
#define lovrModelDataCreateEmpty(...) lovrModelDataInitEmpty(lovrAlloc(ModelData), __VA_ARGS__)
#define lovrModelDataCreateFromGltf(...) lovrModelDataInitFromGltf(lovrAlloc(gltfModelData), __VA_ARGS__)
void lovrgltfModelDataDestroy(void* ref);
void lovrModelDataDestroy(void* ref);
void lovrModelDataGetAABB(ModelData* modelData, float aabb[6]);
