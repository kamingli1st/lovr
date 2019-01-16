#include "data/blob.h"
#include "data/textureData.h"
#include "util.h"
#include "lib/map/map.h"
#include "lib/vec/vec.h"

#pragma once

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

typedef enum {
  FILTER_NEAREST,
  FILTER_BILINEAR,
  FILTER_TRILINEAR,
  FILTER_ANISOTROPIC
} FilterMode;

typedef struct {
  FilterMode mode;
  float anisotropy;
} TextureFilter;

typedef enum {
  WRAP_CLAMP,
  WRAP_REPEAT,
  WRAP_MIRRORED_REPEAT
} WrapMode;

typedef struct {
  WrapMode s;
  WrapMode t;
  WrapMode r;
} TextureWrap;

typedef enum {
  SCALAR_METALNESS,
  SCALAR_ROUGHNESS,
  MAX_MATERIAL_SCALARS
} MaterialScalar;

typedef enum {
  COLOR_DIFFUSE,
  COLOR_EMISSIVE,
  MAX_MATERIAL_COLORS
} MaterialColor;

typedef enum {
  TEXTURE_DIFFUSE,
  TEXTURE_EMISSIVE,
  TEXTURE_METALNESS,
  TEXTURE_ROUGHNESS,
  TEXTURE_OCCLUSION,
  TEXTURE_NORMAL,
  TEXTURE_ENVIRONMENT_MAP,
  MAX_MATERIAL_TEXTURES
} MaterialTexture;

typedef enum {
  SMOOTH_STEP,
  SMOOTH_LINEAR,
  SMOOTH_CUBIC,
} SmoothMode;

typedef enum {
  PROP_TRANSLATION,
  PROP_ROTATION,
  PROP_SCALE,
} AnimationProperty;

typedef enum { I8, U8, I16, U16, I32, U32, F32 } AttributeType;

typedef struct {
  int nodeIndex;
  AnimationProperty property;
  int sampler;
} ModelAnimationChannel;

typedef struct {
  int times;
  int values;
  SmoothMode smoothing;
} ModelAnimationSampler;

typedef struct {
  ModelAnimationChannel* channels;
  ModelAnimationSampler* samplers;
  int channelCount;
  int samplerCount;
} ModelAnimation;

typedef struct {
  void* data;
  size_t stride;
  int count;
  AttributeType type;
  unsigned int components : 3;
  bool normalized : 1;
} ModelAttribute;

typedef struct {
  int textureDataIndex;
  TextureFilter filter;
  TextureWrap wrap;
  bool mipmaps;
} ModelTexture;

typedef struct {
  float scalars[MAX_MATERIAL_SCALARS];
  Color colors[MAX_MATERIAL_COLORS];
  int textures[MAX_MATERIAL_TEXTURES];
} ModelMaterial;

typedef struct {
  DrawMode mode;
  ModelAttribute attributes[MAX_DEFAULT_ATTRIBUTES];
  ModelAttribute indices;
  int material;
} ModelPrimitive;

typedef struct {
  float transform[16];
  float globalTransform[16];
  uint32_t* children;
  uint32_t childCount;
  uint32_t primitiveIndex;
  uint32_t primitiveCount;
  int skin;
} ModelNode;

typedef struct {
  uint32_t* joints;
  uint32_t jointCount;
  int skeleton;
  float* inverseBindMatrices;
} ModelSkin;

typedef struct {
  Ref ref;
  uint8_t* data;
  ModelAnimationChannel* animationChannels;
  ModelAnimationSampler* animationSamplers;
  ModelAnimation* animations;
  Blob** blobs;
  TextureData** images;
  ModelTexture* textures;
  ModelMaterial* materials;
  ModelPrimitive* primitives;
  ModelNode* nodes;
  ModelSkin* skins;
  int animationChannelCount;
  int animationSamplerCount;
  int animationCount;
  int blobCount;
  int imageCount;
  int textureCount;
  int materialCount;
  int primitiveCount;
  int nodeCount;
  int skinCount;
} ModelData;

typedef struct {
  void* (*read)(const char* path, size_t* bytesRead);
} ModelDataIO;

ModelData* lovrModelDataInit(ModelData* model, Blob* blob, ModelDataIO io);
#define lovrModelDataCreate(...) lovrModelDataInit(lovrAlloc(ModelData), __VA_ARGS__)
void lovrModelDataDestroy(void* ref);
