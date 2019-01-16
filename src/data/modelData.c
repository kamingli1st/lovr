#include "data/modelData.h"
#include "lib/math.h"
#include "lib/jsmn/jsmn.h"
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#define MAGIC_glTF 0x46546c67
#define MAGIC_JSON 0x4e4f534a
#define MAGIC_BIN 0x004e4942

#define STR_EQ(k, s) !strncmp(k.data, s, k.length)
#define NOM_VALUE(j, t) nomValue(j, t, 1, 0)
#define NOM_INT(j, t) strtol(j + (t++)->start, NULL, 10)
#define NOM_STR(j, t) (gltfString) { (char* )j + t->start, t->end - t->start }; t++
#define NOM_BOOL(j, t) (*(j + (t++)->start) == 't')
#define NOM_FLOAT(j, t) strtof(j + (t++)->start, NULL)

typedef struct {
  char* data;
  size_t length;
} gltfString;

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
  int view;
  int count;
  int offset;
  AttributeType type;
  uint8_t components;
  bool normalized;
  bool matrix;
  bool hasMin;
  bool hasMax;
  float min[4];
  float max[4];
} gltfAccessor;

typedef struct {
  int buffer;
  int offset;
  int length;
  int stride;
} gltfBufferView;

typedef struct {
  uint32_t primitiveIndex;
  uint32_t primitiveCount;
} gltfMesh;

typedef struct {
  TextureFilter filter;
  TextureWrap wrap;
  bool mipmaps;
} gltfSampler;

static int nomValue(const char* data, jsmntok_t* token, int count, int sum) {
  if (count == 0) { return sum; }
  switch (token->type) {
    case JSMN_OBJECT: return nomValue(data, token + 1, count - 1 + 2 * token->size, sum + 1);
    case JSMN_ARRAY: return nomValue(data, token + 1, count - 1 + token->size, sum + 1);
    default: return nomValue(data, token + 1, count - 1, sum + 1);
  }
}

// Kinda like total += sum(map(arr, obj => #obj[key]))
static jsmntok_t* aggregate(const char* json, jsmntok_t* token, const char* target, int* total) {
  for (int i = (token++)->size; i > 0; i--) {
    if (token->size > 0) {
      for (int k = (token++)->size; k > 0; k--) {
        gltfString key = NOM_STR(json, token);
        if (STR_EQ(key, target)) {
          *total += token->size;
        }
        token += NOM_VALUE(json, token);
      }
    }
  }
  return token;
}

static void accessorToAttribute(gltfAccessor* accessor, ModelAttribute* attribute) {
  //
}

static jsmntok_t* parseAccessors(const char* json, jsmntok_t* token, gltfAccessor* accessor) {
  for (int i = (token++)->size; i > 0; i--, accessor++) {
    for (int k = (token++)->size; k > 0; k--) {
      gltfString key = NOM_STR(json, token);
      if (STR_EQ(key, "bufferView")) { accessor->view = NOM_INT(json, token); }
      else if (STR_EQ(key, "count")) { accessor->count = NOM_INT(json, token); }
      else if (STR_EQ(key, "byteOffset")) { accessor->offset = NOM_INT(json, token); }
      else if (STR_EQ(key, "normalized")) { accessor->normalized = NOM_BOOL(json, token); }
      else if (STR_EQ(key, "componentType")) {
        switch (NOM_INT(json, token)) {
          case 5120: accessor->type = I8; break;
          case 5121: accessor->type = U8; break;
          case 5122: accessor->type = I16; break;
          case 5123: accessor->type = U16; break;
          case 5125: accessor->type = U32; break;
          case 5126: accessor->type = F32; break;
          default: break;
        }
      } else if (STR_EQ(key, "type")) {
        gltfString type = NOM_STR(json, token);
        if (STR_EQ(type, "SCALAR")) {
          accessor->components = 1;
        } else if (type.length == 4) {
          accessor->components = type.data[3] - '0';
          accessor->matrix = type.data[0] == 'M';
        }
      } else if (STR_EQ(key, "min") && token->size <= 4) {
        int count = (token++)->size;
        accessor->hasMin = true;
        for (int j = 0; j < count; j++) {
          accessor->min[j] = NOM_FLOAT(json, token);
        }
      } else if (STR_EQ(key, "max") && token->size <= 4) {
        int count = (token++)->size;
        accessor->hasMax = true;
        for (int j = 0; j < count; j++) {
          accessor->max[j] = NOM_FLOAT(json, token);
        }
      } else {
        token += NOM_VALUE(json, token);
      }
    }
  }
  return token;
}

static jsmntok_t* parseAnimationChannel(const char* json, jsmntok_t* token, int index, ModelData* model) {
  ModelAnimationChannel* channel = &model->animationChannels[index];
  int keyCount = (token++)->size;
  for (int k = 0; k < keyCount; k++) {
    gltfString key = NOM_STR(json, token);
    if (STR_EQ(key, "sampler")) { channel->sampler = NOM_INT(json, token); }
    else if (STR_EQ(key, "target")) {
      int targetKeyCount = (token++)->size;
      for (int tk = 0; tk < targetKeyCount; tk++) {
        gltfString targetKey = NOM_STR(json, token);
        if (STR_EQ(targetKey, "node")) { channel->nodeIndex = NOM_INT(json, token); }
        else if (STR_EQ(targetKey, "path")) {
          gltfString property = NOM_STR(json, token);
          if (STR_EQ(property, "translation")) { channel->property = PROP_TRANSLATION; }
          else if (STR_EQ(property, "rotation")) { channel->property = PROP_ROTATION; }
          else if (STR_EQ(property, "scale")) { channel->property = PROP_SCALE; }
        }
      }
    } else {
      token += NOM_VALUE(json, token);
    }
  }
  return token;
}

static jsmntok_t* parseAnimationSampler(const char* json, jsmntok_t* token, int index, ModelData* model) {
  ModelAnimationSampler* sampler = &model->animationSamplers[index];
  int keyCount = (token++)->size;
  for (int k = 0; k < keyCount; k++) {
    gltfString key = NOM_STR(json, token);
    if (STR_EQ(key, "input")) { sampler->times = NOM_INT(json, token); }
    else if (STR_EQ(key, "output")) { sampler->values = NOM_INT(json, token); }
    else if (STR_EQ(key, "interpolation")) {
      gltfString smoothing = NOM_STR(json, token);
      if (STR_EQ(smoothing, "LINEAR")) { sampler->smoothing = SMOOTH_LINEAR; }
      else if (STR_EQ(smoothing, "STEP")) { sampler->smoothing = SMOOTH_STEP; }
      else if (STR_EQ(smoothing, "CUBICSPLINE")) { sampler->smoothing = SMOOTH_CUBIC; }
    } else {
      token += NOM_VALUE(json, token);
    }
  }
  return token;
}

static void parseAnimations(const char* json, jsmntok_t* token, ModelData* model) {
  if (!token) return;

  int channelIndex = 0;
  int samplerIndex = 0;
  int count = (token++)->size;
  for (int i = 0; i < count; i++) {
    ModelAnimation* animation = &model->animations[i];

    int keyCount = (token++)->size;
    for (int k = 0; k < keyCount; k++) {
      gltfString key = NOM_STR(json, token);
      if (STR_EQ(key, "channels")) {
        animation->channelCount = (token++)->size;
        animation->channels = &model->animationChannels[channelIndex];
        for (int j = 0; j < animation->channelCount; j++) {
          token = parseAnimationChannel(json, token, channelIndex++, model);
        }
      } else if (STR_EQ(key, "samplers")) {
        animation->samplerCount = (token++)->size;
        animation->samplers = &model->animationSamplers[samplerIndex];
        for (int j = 0; j < animation->samplerCount; j++) {
          token = parseAnimationSampler(json, token, samplerIndex++, model);
        }
      } else {
        token += NOM_VALUE(json, token);
      }
    }
  }
}

static jsmntok_t* parseBuffers(const char* json, jsmntok_t* token, Blob** blob, ModelDataIO io, const char* basePath, Blob* glb) {
  for (int i = (token++)->size; i > 0; i--, blob++) {
    gltfString uri = { 0 };
    size_t size = 0;

    for (int k = (token++)->size; k > 0; k--) {
      gltfString key = NOM_STR(json, token);
      if (STR_EQ(key, "byteLength")) { size = NOM_INT(json, token); }
      else if (STR_EQ(key, "uri")) { uri = NOM_STR(json, token); }
      else { token += NOM_VALUE(json, token); }
    }

    if (uri.data) {
      size_t bytesRead;
      char filename[1024];
      int length = token->end - token->start;
      lovrAssert(length < 1024, "Buffer filename is too long");
      snprintf(filename, 1023, "%s/%.*s", basePath, length, (char*) json + token->start);
      *blob = lovrBlobCreate(io.read(filename, &bytesRead), length, NULL);
      lovrAssert((*blob)->data && bytesRead == (size_t) length, "Unable to read %s", filename);
    } else {
      lovrAssert(glb, "Buffer is missing URI");
      lovrRetain(glb);
      *blob = glb;
    }
  }
  return token;
}

static jsmntok_t* parseBufferViews(const char* json, jsmntok_t* token, gltfBufferView* bufferView, Blob* glb, ptrdiff_t binOffset) {
  for (int i = (token++)->size; i > 0; i--, bufferView++) {
    for (int k = (token++)->size; k > 0; k--) {
      gltfString key = NOM_STR(json, token);
      if (STR_EQ(key, "buffer")) { bufferView->buffer = NOM_INT(json, token); }
      else if (STR_EQ(key, "byteOffset")) { bufferView->offset = NOM_INT(json, token); }
      else if (STR_EQ(key, "byteLength")) { bufferView->length = NOM_INT(json, token); }
      else if (STR_EQ(key, "byteStride")) { bufferView->stride = NOM_INT(json, token); }
      else { token += NOM_VALUE(json, token); }

      if (bufferView->buffer == 0 && glb) {
        bufferView->offset += binOffset;
      }
    }
  }
  return token;
}

static void parseImages(const char* json, jsmntok_t* token, ModelData* model, ModelDataIO io, const char* basePath) {
  if (!token) return;

  int count = (token++)->size;
  for (int i = 0; i < count; i++) {
    TextureData** image = &model->images[i];
    int keyCount = (token++)->size;
    for (int k = 0; k < keyCount; k++) {
      gltfString key = NOM_STR(json, token);
      if (STR_EQ(key, "bufferView")) {
        /*int viewIndex = NOM_INT(json, token);
        ModelView* view = &model->views[viewIndex];
        void* data = (uint8_t*) model->blobs[view->blob].data + view->offset;
        Blob* blob = lovrBlobCreate(data, view->length, NULL);
        *image = lovrTextureDataCreateFromBlob(blob, true);
        blob->data = NULL; // FIXME
        lovrRelease(blob);*/
      } else if (STR_EQ(key, "uri")) {
        size_t size = 0;
        char filename[1024];
        int length = token->end - token->start;
        snprintf(filename, 1024, "%s/%.*s%c", basePath, length, (char*) json + token->start, 0);
        void* data = io.read(filename, &size);
        lovrAssert(data && size > 0, "Unable to read image from '%s'", filename);
        Blob* blob = lovrBlobCreate(data, size, NULL);
        *image = lovrTextureDataCreateFromBlob(blob, true);
        lovrRelease(blob);
      } else {
        token += NOM_VALUE(json, token);
      }
    }
  }
}

static jsmntok_t* parseSamplers(const char* json, jsmntok_t* token, gltfSampler* sampler) {
  for (int i = (token++)->size; i > 0; i--, sampler++) {
    sampler->wrap.s = sampler->wrap.t = sampler->wrap.r = WRAP_REPEAT;
    int min = -1;
    int mag = -1;

    for (int k = (token++)->size; k > 0; k--) {
      gltfString key = NOM_STR(json, token);
      if (STR_EQ(key, "minFilter")) { min = NOM_INT(json, token); }
      else if (STR_EQ(key, "magFilter")) { mag = NOM_INT(json, token); }
      else if (STR_EQ(key, "wrapS")) {
        switch (NOM_INT(json, token)) {
          case 33071: sampler->wrap.s = WRAP_CLAMP; break;
          case 33648: sampler->wrap.s = WRAP_MIRRORED_REPEAT; break;
          case 10497: sampler->wrap.s = WRAP_REPEAT; break;
          default: lovrThrow("Unknown sampler wrapS mode for sampler %d", i);
        }
      } else if (STR_EQ(key, "wrapT")) {
        switch (NOM_INT(json, token)) {
          case 33071: sampler->wrap.t = WRAP_CLAMP; break;
          case 33648: sampler->wrap.t = WRAP_MIRRORED_REPEAT; break;
          case 10497: sampler->wrap.t = WRAP_REPEAT; break;
          default: lovrThrow("Unknown sampler wrapT mode for sampler %d", i);
        }
      } else {
        token += NOM_VALUE(json, token);
      }
    }

    if (min == 9728 || min == 9984 || min == 9986 || mag == 9728) {
      sampler->filter.mode = FILTER_NEAREST;
    } else {
      switch (min) {
        case 9729: sampler->filter.mode = FILTER_BILINEAR, sampler->mipmaps = false; break;
        case 9985: sampler->filter.mode = FILTER_BILINEAR, sampler->mipmaps = true; break;
        case 9987: sampler->filter.mode = FILTER_TRILINEAR, sampler->mipmaps = true; break;
      }
    }
  }
  return token;
}

static jsmntok_t* parseTextureInfo(const char* json, jsmntok_t* token, int* dest) {
  int keyCount = (token++)->size;
  for (int k = 0; k < keyCount; k++) {
    gltfString key = NOM_STR(json, token);
    if (STR_EQ(key, "index")) { *dest = NOM_INT(json, token); }
    else if (STR_EQ(key, "texCoord")) {
      lovrAssert(NOM_INT(json, token) == 0, "Only one set of texture coordinates is supported");
    } else {
      token += NOM_VALUE(json, token);
    }
  }
  return token;
}

static void parseMaterials(const char* json, jsmntok_t* token, ModelData* model) {
  if (!token) return;

  int count = (token++)->size;
  for (int i = 0; i < count; i++) {
    ModelMaterial* material = &model->materials[i];
    int keyCount = (token++)->size;
    for (int k = 0; k < keyCount; k++) {
      gltfString key = NOM_STR(json, token);
      if (STR_EQ(key, "pbrMetallicRoughness")) {
        int pbrKeyCount = (token++)->size;
        for (int j = 0; j < pbrKeyCount; j++) {
          gltfString pbrKey = NOM_STR(json, token);
          if (STR_EQ(pbrKey, "baseColorFactor")) {
            token++; // Enter array
            material->colors[COLOR_DIFFUSE].r = NOM_FLOAT(json, token);
            material->colors[COLOR_DIFFUSE].g = NOM_FLOAT(json, token);
            material->colors[COLOR_DIFFUSE].b = NOM_FLOAT(json, token);
            material->colors[COLOR_DIFFUSE].a = NOM_FLOAT(json, token);
          } else if (STR_EQ(pbrKey, "baseColorTexture")) {
            token = parseTextureInfo(json, token, &material->textures[TEXTURE_DIFFUSE]);
          } else if (STR_EQ(pbrKey, "metallicFactor")) {
            material->scalars[SCALAR_METALNESS] = NOM_FLOAT(json, token);
          } else if (STR_EQ(pbrKey, "roughnessFactor")) {
            material->scalars[SCALAR_ROUGHNESS] = NOM_FLOAT(json, token);
          } else if (STR_EQ(pbrKey, "metallicRoughnessTexture")) {
            token = parseTextureInfo(json, token, &material->textures[TEXTURE_METALNESS]);
            material->textures[TEXTURE_ROUGHNESS] = material->textures[TEXTURE_METALNESS];
          } else {
            token += NOM_VALUE(json, token);
          }
        }
      } else if (STR_EQ(key, "normalTexture")) {
        token = parseTextureInfo(json, token, &material->textures[TEXTURE_NORMAL]);
      } else if (STR_EQ(key, "occlusionTexture")) {
        token = parseTextureInfo(json, token, &material->textures[TEXTURE_OCCLUSION]);
      } else if (STR_EQ(key, "emissiveTexture")) {
        token = parseTextureInfo(json, token, &material->textures[TEXTURE_EMISSIVE]);
      } else if (STR_EQ(key, "emissiveFactor")) {
        token++; // Enter array
        material->colors[COLOR_EMISSIVE].r = NOM_FLOAT(json, token);
        material->colors[COLOR_EMISSIVE].g = NOM_FLOAT(json, token);
        material->colors[COLOR_EMISSIVE].b = NOM_FLOAT(json, token);
        material->colors[COLOR_EMISSIVE].a = NOM_FLOAT(json, token);
      } else {
        token += NOM_VALUE(json, token);
      }
    }
  }
}

ModelData* lovrModelDataInit(ModelData* model, Blob* blob, ModelDataIO io) {
  uint8_t* data = blob->data;
  gltfHeader* header = (gltfHeader*) data;
  bool glb = header->magic == MAGIC_glTF;
  const char *json, *binData;
  size_t jsonLength, binLength;
  ptrdiff_t binOffset;

  char basePath[1024];
  strncpy(basePath, blob->name, 1023);
  char* slash = strrchr(basePath, '/');
  if (slash) *slash = 0;

  if (glb) {
    gltfChunkHeader* jsonHeader = (gltfChunkHeader*) &header[1];
    lovrAssert(jsonHeader->type == MAGIC_JSON, "Invalid JSON header");

    json = (char*) &jsonHeader[1];
    jsonLength = jsonHeader->length;

    gltfChunkHeader* binHeader = (gltfChunkHeader*) &json[jsonLength];
    lovrAssert(binHeader->type == MAGIC_BIN, "Invalid BIN header");

    binData = (char*) &binHeader[1];
    binLength = binHeader->length;
    binOffset = (char*) binData - (char*) blob->data;
  } else {
    json = (char*) data;
    jsonLength = blob->size;
    binData = NULL;
    binLength = 0;
    binOffset = 0;
  }

  jsmn_parser parser;
  jsmn_init(&parser);
  int tokenCount = jsmn_parse(&parser, json, jsonLength, NULL, 0);
  jsmntok_t* tokens = malloc(tokenCount * sizeof(jsmntok_t));
  jsmn_init(&parser);
  tokenCount = jsmn_parse(&parser, json, jsonLength, tokens, tokenCount);
  lovrAssert(tokenCount >= 0, "Invalid JSON");
  lovrAssert(tokens[0].type == JSMN_OBJECT, "No root object");

  // Preparse
  struct {
    size_t totalSize;
    jsmntok_t* animations;
    jsmntok_t* images;
    jsmntok_t* textures;
    jsmntok_t* materials;
    jsmntok_t* meshes;
    jsmntok_t* nodes;
    jsmntok_t* skins;
    int childCount;
    int jointCount;
  } info = { 0 };

  gltfAccessor* accessors = NULL;
  gltfBufferView* bufferViews = NULL;
  gltfMesh* meshes = NULL;
  gltfSampler* samplers = NULL;

  for (jsmntok_t* token = tokens + 1; token < tokens + tokenCount;) {
    gltfString key = NOM_STR(json, token);
    if (STR_EQ(key, "accessors")) {
      accessors = malloc(token->size * sizeof(gltfAccessor));
      token = parseAccessors(json, token, accessors);
    } else if (STR_EQ(key, "animations")){
      info.animations = token;
      model->animationCount = token->size;
      info.totalSize += token->size * sizeof(ModelAnimation);
      // Almost like aggregate, but we gotta aggregate 2 keys in a single pass
      int size = (token++)->size;
      for (int i = 0; i < size; i++) {
        if (token->size > 0) {
          int keyCount = (token++)->size;
          for (int k = 0; k < keyCount; k++) {
            gltfString animationKey = NOM_STR(json, token);
            if (STR_EQ(animationKey, "channels")) { model->animationChannelCount += token->size; }
            else if (STR_EQ(animationKey, "samplers")) { model->animationSamplerCount += token->size; }
            token += NOM_VALUE(json, token);
          }
        }
      }
      info.totalSize += model->animationChannelCount * sizeof(ModelAnimationChannel);
      info.totalSize += model->animationSamplerCount * sizeof(ModelAnimationSampler);
    } else if (STR_EQ(key, "buffers")) {
      model->blobCount = token->size;
      model->blobs = malloc(model->blobCount * sizeof(Blob*));
      token = parseBuffers(json, token, model->blobs, io, basePath, glb ? blob : NULL);
    } else if (STR_EQ(key, "bufferViews")) {
      bufferViews = malloc(token->size * sizeof(gltfBufferView));
      token = parseBufferViews(json, token, bufferViews, glb ? blob : NULL, binOffset);
    } else if (STR_EQ(key, "images")) {
      info.images = token;
      model->imageCount = token->size;
      info.totalSize += token->size * sizeof(TextureData*);
      token += NOM_VALUE(json, token);
    } else if (STR_EQ(key, "samplers")) {
      samplers = malloc(token->size * sizeof(gltfSampler));
      token = parseSamplers(json, token, samplers);
    } else if (STR_EQ(key, "textures")) {
      info.textures = token;
      model->textureCount = token->size;
      info.totalSize += token->size * sizeof(ModelTexture);
      token += NOM_VALUE(json, token);
    } else if (STR_EQ(key, "materials")) {
      info.materials = token;
      model->materialCount = token->size;
      info.totalSize += token->size * sizeof(ModelMaterial);
      token += NOM_VALUE(json, token);
    } else if (STR_EQ(key, "meshes")) {
      info.meshes = token;
      meshes = malloc(token->size * sizeof(gltfMesh));
      gltfMesh* mesh = meshes;
      model->primitiveCount = 0;
      for (int i = (token++)->size; i > 0; i--, mesh++) {
        for (int k = (token++)->size; k > 0; k--) {
          gltfString key = NOM_STR(json, token);
          if (STR_EQ(key, "primitives")) {
            mesh->primitiveIndex = model->primitiveCount;
            model->primitiveCount += token->size;
          }
          token += NOM_VALUE(json, token);
        }
      }
      info.totalSize += model->primitiveCount * sizeof(ModelPrimitive);
    } else if (STR_EQ(key, "nodes")) {
      info.nodes = token;
      model->nodeCount = token->size;
      info.totalSize += token->size * sizeof(ModelNode);
      token = aggregate(json, token, "children", &info.childCount);
      info.totalSize += info.childCount * sizeof(uint32_t);
    } else if (STR_EQ(key, "skins")) {
      info.skins = token;
      model->skinCount = token->size;
      info.totalSize += token->size * sizeof(ModelSkin);
      token = aggregate(json, token, "joints", &info.jointCount);
      info.totalSize += info.jointCount * sizeof(uint32_t);
    } else {
      token += NOM_VALUE(json, token);
    }
  }

  size_t offset = 0;
  model->data = calloc(1, info.totalSize);
  model->animationChannels = (ModelAnimationChannel*) (model->data + offset), offset += model->animationChannelCount * sizeof(ModelAnimationChannel);
  model->animationSamplers = (ModelAnimationSampler*) (model->data + offset), offset += model->animationSamplerCount * sizeof(ModelAnimationSampler);
  model->animations = (ModelAnimation*) (model->data + offset), offset += model->animationCount * sizeof(ModelAnimation);
  model->images = (TextureData**) (model->data + offset), offset += model->imageCount * sizeof(TextureData*);
  model->textures = (ModelTexture*) (model->data + offset), offset += model->textureCount * sizeof(ModelTexture);
  model->materials = (ModelMaterial*) (model->data + offset), offset += model->materialCount * sizeof(ModelMaterial);
  model->nodes = (ModelNode*) (model->data + offset), offset += model->nodeCount * sizeof(ModelNode);
  model->skins = (ModelSkin*) (model->data + offset), offset += model->skinCount * sizeof(ModelSkin);

  uint32_t* nodeChildren = (uint32_t*) (model->data + offset); offset += info.childCount * sizeof(uint32_t);
  uint32_t* skinJoints = (uint32_t*) (model->data + offset); offset += info.jointCount * sizeof(uint32_t);

  // Textures
  if (info.textures) {
    jsmntok_t* token = info.textures;
    ModelTexture* texture = model->textures;
    for (int i = (token++)->size; i > 0; i--, texture++) {
      for (int k = (token++)->size; k > 0; k--) {
        gltfString key = NOM_STR(json, token);
        if (STR_EQ(key, "source")) { texture->textureDataIndex = NOM_INT(json, token); }
        else if (STR_EQ(key, "sampler")) {
          gltfSampler* sampler = &samplers[NOM_INT(json, token)];
          texture->filter = sampler->filter;
          texture->wrap = sampler->wrap;
          texture->mipmaps = sampler->mipmaps;
        }
        else { token += NOM_VALUE(json, token); }
      }
    }
  }

  parseAnimations(json, info.animations, model);
  parseImages(json, info.images, model, io, basePath);
  parseMaterials(json, info.materials, model);

  // Primitives
  if (info.meshes) {
    int primitiveIndex = 0;
    jsmntok_t* token = info.meshes;
    ModelPrimitive* primitive = model->primitives;
    for (int i = (token++)->size; i > 0; i--) {
      for (int k = (token++)->size; k > 0; k--) {
        gltfString key = NOM_STR(json, token);
        if (STR_EQ(key, "primitives")) {
          for (uint32_t j = (token++)->size; j > 0; j--, primitive++) {
            primitive->mode = DRAW_TRIANGLES;

            for (int kk = (token++)->size; kk > 0; kk--) {
              gltfString key = NOM_STR(json, token);
              if (STR_EQ(key, "material")) {
                primitive->material = NOM_INT(json, token);
              } else if (STR_EQ(key, "indices")) {
                accessorToAttribute(&accessors[NOM_INT(json, token)], &primitive->indices);
              } else if (STR_EQ(key, "mode")) {
                switch (NOM_INT(json, token)) {
                  case 0: primitive->mode = DRAW_POINTS; break;
                  case 1: primitive->mode = DRAW_LINES; break;
                  case 2: primitive->mode = DRAW_LINE_LOOP; break;
                  case 3: primitive->mode = DRAW_LINE_STRIP; break;
                  case 4: primitive->mode = DRAW_TRIANGLES; break;
                  case 5: primitive->mode = DRAW_TRIANGLE_STRIP; break;
                  case 6: primitive->mode = DRAW_TRIANGLE_FAN; break;
                  default: lovrThrow("Unknown primitive mode");
                }
              } else if (STR_EQ(key, "attributes")) {
                int attributeCount = (token++)->size;
                for (int a = 0; a < attributeCount; a++) {
                  int attribute = -1;
                  gltfString name = NOM_STR(json, token);
                  gltfAccessor* accessor = &accessors[NOM_INT(json, token)];
                  if (STR_EQ(name, "POSITION")) { attribute = ATTR_POSITION; }
                  else if (STR_EQ(name, "NORMAL")) { attribute = ATTR_NORMAL; }
                  else if (STR_EQ(name, "TEXCOORD_0")) { attribute = ATTR_TEXCOORD; }
                  else if (STR_EQ(name, "COLOR_0")) { attribute = ATTR_COLOR; }
                  else if (STR_EQ(name, "TANGENT")) { attribute = ATTR_TANGENT; }
                  else if (STR_EQ(name, "JOINTS_0")) { attribute = ATTR_BONES; }
                  else if (STR_EQ(name, "WEIGHTS_0")) { attribute = ATTR_WEIGHTS; }

                  if (attribute >= 0) {
                    accessorToAttribute(accessor, &primitive->attributes[attribute]);
                  }
                }
              } else {
                token += NOM_VALUE(json, token);
              }
            }
          }
        } else {
          token += NOM_VALUE(json, token);
        }
      }
    }
  }

  // Nodes
  if (info.nodes) {
    int childIndex = 0;
    jsmntok_t* token = info.nodes;
    ModelNode* node = model->nodes;
    for (int i = (token++)->size; i > 0; i--, node++) {
      float translation[3] = { 0, 0, 0 };
      float rotation[4] = { 0, 0, 0, 0 };
      float scale[3] = { 1, 1, 1 };
      bool matrix = false;
      node->primitiveCount = 0;
      node->skin = -1;

      for (int k = (token++)->size; k > 0; k--) {
        gltfString key = NOM_STR(json, token);
        if (STR_EQ(key, "mesh")) {
          gltfMesh* mesh = &meshes[NOM_INT(json, token)];
          node->primitiveIndex = mesh->primitiveIndex;
          node->primitiveCount = mesh->primitiveCount;
        } else if (STR_EQ(key, "skin")) {
          node->skin = NOM_INT(json, token);
        } else if (STR_EQ(key, "children")) {
          node->children = &nodeChildren[childIndex];
          node->childCount = (token++)->size;
          for (uint32_t j = 0; j < node->childCount; j++) {
            nodeChildren[childIndex++] = NOM_INT(json, token);
          }
        } else if (STR_EQ(key, "matrix")) {
          lovrAssert((token++)->size == 16, "Node matrix needs 16 elements");
          matrix = true;
          for (int j = 0; j < 16; j++) {
            node->transform[j] = NOM_FLOAT(json, token);
          }
        } else if (STR_EQ(key, "translation")) {
          lovrAssert((token++)->size == 3, "Node translation needs 3 elements");
          translation[0] = NOM_FLOAT(json, token);
          translation[1] = NOM_FLOAT(json, token);
          translation[2] = NOM_FLOAT(json, token);
        } else if (STR_EQ(key, "rotation")) {
          lovrAssert((token++)->size == 4, "Node rotation needs 4 elements");
          rotation[0] = NOM_FLOAT(json, token);
          rotation[1] = NOM_FLOAT(json, token);
          rotation[2] = NOM_FLOAT(json, token);
          rotation[3] = NOM_FLOAT(json, token);
        } else if (STR_EQ(key, "scale")) {
          lovrAssert((token++)->size == 3, "Node scale needs 3 elements");
          scale[0] = NOM_FLOAT(json, token);
          scale[1] = NOM_FLOAT(json, token);
          scale[2] = NOM_FLOAT(json, token);
        } else {
          token += NOM_VALUE(json, token);
        }
      }

      // Fix it in post
      if (!matrix) {
        mat4_identity(node->transform);
        mat4_translate(node->transform, translation[0], translation[1], translation[2]);
        mat4_rotateQuat(node->transform, rotation);
        mat4_scale(node->transform, scale[0], scale[1], scale[2]);
      }
    }
  }

  // Skins
  if (info.skins) {
    int jointIndex = 0;
    jsmntok_t* token = info.skins;
    ModelSkin* skin = model->skins;
    for (int i = (token++)->size; i > 0; i--, skin++) {
      int keyCount = (token++)->size;
      for (int k = (token++)->size; k > 0; k--) {
        gltfString key = NOM_STR(json, token);
        if (STR_EQ(key, "inverseBindMatrices")) {
          gltfAccessor* accessor = &accessors[NOM_INT(json, token)];
          gltfBufferView* bufferView = &bufferViews[accessor->view];
          uint8_t* data = model->blobs[bufferView->buffer]->data;
          skin->inverseBindMatrices = (float*) (data + bufferView->offset + accessor->offset);
        } else if (STR_EQ(key, "skeleton")) {
          skin->skeleton = NOM_INT(json, token);
        } else if (STR_EQ(key, "joints")) {
          skin->joints = &skinJoints[jointIndex];
          skin->jointCount = (token++)->size;
          for (uint32_t j = 0; j < skin->jointCount; j++) {
            skinJoints[jointIndex++] = NOM_INT(json, token);
          }
        } else {
          token += NOM_VALUE(json, token);
        }
      }
    }
  }

  free(accessors);
  free(bufferViews);
  free(meshes);
  free(tokens);
  return model;
}

void lovrModelDataDestroy(void* ref) {
  ModelData* model = ref;
  free(model->data);
}
