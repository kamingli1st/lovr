#include "data/modelData.h"
#include "lib/math.h"
#include "lib/jsmn/jsmn.h"
#include <stdbool.h>

#define MAGIC_glTF 0x46546c67
#define MAGIC_JSON 0x4e4f534a
#define MAGIC_BIN 0x004e4942

#define KEY_EQ(k, s) !strncmp(k.data, s, k.length)
#define NOM_STR(j, t) (gltfString) { (char*) j + t->start, t->end - t->start }; t++
#define NOM_KEY(j, t) hashKey((char*) j + (t++)->start)
#define NOM_INT(j, t) strtol(j + (t++)->start, NULL, 10)
#define NOM_BOOL(j, t) (*(j + (t++)->start) == 't')
#define NOM_FLOAT(j, t) strtof(j + (t++)->start, NULL)

typedef struct {
  char* data;
  size_t length;
} gltfString;

typedef struct {
  struct { int count; jsmntok_t* token; } accessors;
  struct { int count; jsmntok_t* token; } animations;
  struct { int count; jsmntok_t* token; } blobs;
  struct { int count; jsmntok_t* token; } views;
  struct { int count; jsmntok_t* token; } nodes;
  struct { int count; jsmntok_t* token; } meshes;
  struct { int count; jsmntok_t* token; } skins;
  int childCount;
  int primitiveCount;
  int jointCount;
} gltfInfo;

static uint32_t hashKey(char* key) {
  uint32_t hash = 0;
  for (int i = 0; key[i] != '"'; i++) {
    hash = (hash * 65599) + key[i];
  }
  return hash ^ (hash >> 16);
}

static int nomValue(const char* data, jsmntok_t* token, int count, int sum) {
  if (count == 0) { return sum; }
  switch (token->type) {
    case JSMN_OBJECT: return nomValue(data, token + 1, count - 1 + 2 * token->size, sum + 1);
    case JSMN_ARRAY: return nomValue(data, token + 1, count - 1 + token->size, sum + 1);
    default: return nomValue(data, token + 1, count - 1, sum + 1);
  }
}

// Kinda like sum(map(arr, obj => #obj[key]))
static jsmntok_t* aggregate(const char* json, jsmntok_t* token, const char* target, int* total) {
  *total = 0;
  int size = (token++)->size;
  for (int i = 0; i < size; i++) {
    if (token->size > 0) {
      int keys = (token++)->size;
      for (int k = 0; k < keys; k++) {
        gltfString key = NOM_STR(json, token);
        if (KEY_EQ(key, target)) {
          *total += token->size;
        }
        token += nomValue(json, token, 1, 0);
      }
    }
  }
  return token;
}

static void preparse(const char* json, jsmntok_t* tokens, int tokenCount, gltfInfo* info, size_t* dataSize) {
  for (jsmntok_t* token = tokens + 1; token < tokens + tokenCount;) { // +1 to skip root object
    switch (NOM_KEY(json, token)) {
      case HASH16("accessors"):
        info->accessors.token = token;
        info->accessors.count = token->size;
        *dataSize += info->accessors.count * sizeof(ModelAccessor);
        token += nomValue(json, token, 1, 0);
        break;
      case HASH16("animations"):
        info->animations.token = token;
        info->animations.count = token->size;
        *dataSize += info->animations.count * sizeof(ModelAnimation);
        token += nomValue(json, token, 1, 0);
        break;
      case HASH16("buffers"):
        info->blobs.token = token;
        info->blobs.count = token->size;
        *dataSize += info->blobs.count * sizeof(ModelBlob);
        token += nomValue(json, token, 1, 0);
        break;
      case HASH16("bufferViews"):
        info->views.token = token;
        info->views.count = token->size;
        *dataSize += info->views.count * sizeof(ModelView);
        token += nomValue(json, token, 1, 0);
        break;
      case HASH16("nodes"):
        info->nodes.token = token;
        info->nodes.count = token->size;
        *dataSize += info->nodes.count * sizeof(ModelNode);
        token = aggregate(json, token, "children", &info->childCount);
        *dataSize += info->childCount * sizeof(uint32_t);
        break;
      case HASH16("meshes"):
        info->meshes.token = token;
        info->meshes.count = token->size;
        *dataSize += info->meshes.count * sizeof(ModelMesh);
        token = aggregate(json, token, "primitives", &info->primitiveCount);
        *dataSize += info->primitiveCount * sizeof(ModelPrimitive);
        break;
      case HASH16("skins"):
        info->skins.token = token;
        info->skins.count = token->size;
        *dataSize += info->skins.count * sizeof(ModelSkin);
        token = aggregate(json, token, "joints", &info->jointCount);
        *dataSize += info->jointCount * sizeof(uint32_t);
        break;
      default: token += nomValue(json, token, 1, 0); break;
    }
  }
}

static void parseAccessors(const char* json, jsmntok_t* token, ModelData* model) {
  if (!token) return;

  int count = (token++)->size;
  for (int i = 0; i < count; i++) {
    ModelAccessor* accessor = &model->accessors[i];
    int keyCount = (token++)->size;

    for (int k = 0; k < keyCount; k++) {
      switch (NOM_KEY(json, token)) {
        case HASH16("bufferView"): accessor->view = NOM_INT(json, token); break;
        case HASH16("count"): accessor->count = NOM_INT(json, token); break;
        case HASH16("byteOffset"): accessor->offset = NOM_INT(json, token); break;
        case HASH16("normalized"): accessor->normalized = NOM_BOOL(json, token); break;
        case HASH16("componentType"):
          switch (NOM_INT(json, token)) {
            case 5120: accessor->type = I8; break;
            case 5121: accessor->type = U8; break;
            case 5122: accessor->type = I16; break;
            case 5123: accessor->type = U16; break;
            case 5125: accessor->type = U32; break;
            case 5126: accessor->type = F32; break;
            default: break;
          }
          break;
        case HASH16("type"):
          switch (NOM_KEY(json, token)) {
            case HASH16("SCALAR"): accessor->components += 1; break;
            case HASH16("VEC2"): accessor->components += 2; break;
            case HASH16("VEC3"): accessor->components += 3; break;
            case HASH16("VEC4"): accessor->components += 4; break;
            default: lovrThrow("Unsupported accessor type"); break;
          }
          break;
        default: token += nomValue(json, token, 1, 0); break;
      }
    }
  }
}

static void parseAnimations(const char* json, jsmntok_t* token, ModelData* model) {
  if (!token) return;

  int count = (token++)->size;
  for (int i = 0; i < count; i++) {
    ModelAnimation* animation = &model->animations[i];

    int keyCount = (token++)->size;
    for (int k = 0; k < keyCount; k++) {
      token += nomValue(json, token, 1, 0);
    }
  }
}

static void parseBlobs(const char* json, jsmntok_t* token, ModelData* model, ModelDataIO io, void* binData) {
  if (!token) return;

  int count = (token++)->size;
  for (int i = 0; i < count; i++) {
    ModelBlob* blob = &model->blobs[i];
    gltfString key;
    int keyCount = (token++)->size;
    size_t bytesRead = 0;
    bool hasUri = false;

    for (int k = 0; k < keyCount; k++) {
      switch (NOM_KEY(json, token)) {
        case HASH16("byteLength"): blob->size = NOM_INT(json, token); break;
        case HASH16("uri"):
          hasUri = true;
          gltfString filename = NOM_STR(json, token);
          filename.data[filename.length] = '\0'; // Change the quote into a terminator (I'll be b0k)
          blob->data = io.read(filename.data, &bytesRead);
          lovrAssert(blob->data, "Unable to read %s", filename.data);
          break;
        default: token += nomValue(json, token, 1, 0); break;
      }
    }

    if (hasUri) {
      lovrAssert(bytesRead == blob->size, "Couldn't read all of buffer data");
    } else {
      lovrAssert(binData && i == 0, "Buffer is missing URI");
      blob->data = binData;
    }
  }
}

static void parseViews(const char* json, jsmntok_t* token, ModelData* model) {
  if (!token) return;

  int count = (token++)->size;
  for (int i = 0; i < count; i++) {
    ModelView* view = &model->views[i];
    int keyCount = (token++)->size;

    for (int k = 0; k < keyCount; k++) {
      switch (NOM_KEY(json, token)) {
        case HASH16("buffer"): view->blob = NOM_INT(json, token); break;
        case HASH16("byteOffset"): view->offset = NOM_INT(json, token); break;
        case HASH16("byteLength"): view->length = NOM_INT(json, token); break;
        case HASH16("byteStride"): view->stride = NOM_INT(json, token); break;
        default: token += nomValue(json, token, 1, 0); break;
      }
    }
  }
}

static void parseNodes(const char* json, jsmntok_t* token, ModelData* model) {
  if (!token) return;

  int childIndex = 0;
  int count = (token++)->size; // Enter array
  for (int i = 0; i < count; i++) {
    ModelNode* node = &model->nodes[i];
    float translation[3] = { 0, 0, 0 };
    float rotation[4] = { 0, 0, 0, 0 };
    float scale[3] = { 1, 1, 1 };
    bool matrix = false;

    int keyCount = (token++)->size; // Enter object
    for (int k = 0; k < keyCount; k++) {
      switch (NOM_KEY(json, token)) {
        case HASH16("mesh"): node->mesh = NOM_INT(json, token); break;
        case HASH16("skin"): node->skin = NOM_INT(json, token); break;
        case HASH16("children"):
          node->children = &model->nodeChildren[childIndex];
          node->childCount = (token++)->size;
          for (uint32_t j = 0; j < node->childCount; j++) {
            model->nodeChildren[childIndex++] = NOM_INT(json, token);
          }
          break;
        case HASH16("matrix"):
          lovrAssert(token->size == 16, "Node matrix needs 16 elements");
          matrix = true;
          for (int j = 0; j < token->size; j++) {
            node->transform[j] = NOM_FLOAT(json, token);
          }
          break;
        case HASH16("translation"):
          lovrAssert(token->size == 3, "Node translation needs 3 elements");
          translation[0] = NOM_FLOAT(json, token);
          translation[1] = NOM_FLOAT(json, token);
          translation[2] = NOM_FLOAT(json, token);
          break;
        case HASH16("rotation"):
          lovrAssert(token->size == 4, "Node rotation needs 4 elements");
          rotation[0] = NOM_FLOAT(json, token);
          rotation[1] = NOM_FLOAT(json, token);
          rotation[2] = NOM_FLOAT(json, token);
          rotation[3] = NOM_FLOAT(json, token);
          break;
        case HASH16("scale"):
          lovrAssert(token->size == 3, "Node scale needs 3 elements");
          scale[0] = NOM_FLOAT(json, token);
          scale[1] = NOM_FLOAT(json, token);
          scale[2] = NOM_FLOAT(json, token);
          break;
        default: token += nomValue(json, token, 1, 0); break;
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

static jsmntok_t* parsePrimitive(const char* json, jsmntok_t* token, int index, ModelData* model) {
  ModelPrimitive* primitive = &model->primitives[index];
  int keyCount = (token++)->size; // Enter object
  memset(primitive->attributes, 0xff, sizeof(primitive->attributes));
  primitive->indices = -1;
  primitive->mode = DRAW_TRIANGLES;

  for (int k = 0; k < keyCount; k++) {
    switch (NOM_KEY(json, token)) {
      case HASH16("material"): primitive->material = NOM_INT(json, token); break;
      case HASH16("indices"): primitive->indices = NOM_INT(json, token); break;
      case HASH16("mode"):
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
        break;
      case HASH16("attributes"): {
        int attributeCount = (token++)->size;
        for (int i = 0; i < attributeCount; i++) {
          switch (NOM_KEY(json, token)) {
            case HASH16("POSITION"): primitive->attributes[ATTR_POSITION] = NOM_INT(json, token); break;
            case HASH16("NORMAL"): primitive->attributes[ATTR_NORMAL] = NOM_INT(json, token); break;
            case HASH16("TEXCOORD_0"): primitive->attributes[ATTR_TEXCOORD] = NOM_INT(json, token); break;
            case HASH16("COLOR_0"): primitive->attributes[ATTR_COLOR] = NOM_INT(json, token); break;
            case HASH16("TANGENT"): primitive->attributes[ATTR_TANGENT] = NOM_INT(json, token); break;
            case HASH16("JOINTS_0"): primitive->attributes[ATTR_BONES] = NOM_INT(json, token); break;
            case HASH16("WEIGHTS_0"): primitive->attributes[ATTR_WEIGHTS] = NOM_INT(json, token); break;
            default: break;
          }
        }
        break;
      }
      default: token += nomValue(json, token, 1, 0); break;
    }
  }
  return token;
}

static void parseMeshes(const char* json, jsmntok_t* token, ModelData* model) {
  if (!token) return;

  int primitiveIndex = 0;
  int count = (token++)->size; // Enter array
  for (int i = 0; i < count; i++) {
    ModelMesh* mesh = &model->meshes[i];
    int keyCount = (token++)->size; // Enter object
    for (int k = 0; k < keyCount; k++) {
      switch (NOM_KEY(json, token)) {
        case HASH16("primitives"):
          mesh->primitives = &model->primitives[primitiveIndex];
          mesh->primitiveCount = (token++)->size;
          for (uint32_t j = 0; j < mesh->primitiveCount; j++) {
            token = parsePrimitive(json, token, primitiveIndex++, model);
          }
          break;
        default: token += nomValue(json, token, 1, 0); break;
      }
    }
  }
}

static void parseSkins(const char* json, jsmntok_t* token, ModelData* model) {
  if (!token) return;

  int jointIndex = 0;
  int count = (token++)->size; // Enter array
  for (int i = 0; i < count; i++) {
    gltfString key;
    ModelSkin* skin = &model->skins[i];
    int keyCount = (token++)->size;
    for (int k = 0; k < keyCount; k++) {
      switch (NOM_KEY(json, token)) {
        case HASH64("inverseBindMatrices"): skin->inverseBindMatrices = NOM_INT(json, token); break;
        case HASH16("skeleton"): skin->skeleton = NOM_INT(json, token); break;
        case HASH16("joints"):
          skin->joints = &model->skinJoints[jointIndex];
          skin->jointCount = (token++)->size;
          for (uint32_t j = 0; j < skin->jointCount; j++) {
            model->skinJoints[jointIndex++] = NOM_INT(json, token);
          }
          break;
        default: token += nomValue(json, token, 1, 0); break;
      }
    }
  }
}

ModelData* lovrModelDataInit(ModelData* model, Blob* blob, ModelDataIO io) {
  uint8_t* data = blob->data;
  gltfHeader* header = (gltfHeader*) data;
  bool glb = header->magic == MAGIC_glTF;
  const char *jsonData, *binData;
  size_t jsonLength, binLength;

  if (glb) {
    gltfChunkHeader* jsonHeader = (gltfChunkHeader*) &header[1];
    lovrAssert(jsonHeader->type == MAGIC_JSON, "Invalid JSON header");

    jsonData = (char*) &jsonHeader[1];
    jsonLength = jsonHeader->length;

    gltfChunkHeader* binHeader = (gltfChunkHeader*) &jsonData[jsonLength];
    lovrAssert(binHeader->type == MAGIC_BIN, "Invalid BIN header");

    binData = (char*) &binHeader[1];
    binLength = binHeader->length;

    // Hang onto the data since it's already here rather than make a copy of it
    lovrRetain(blob);
  } else {
    jsonData = (char*) data;
    jsonLength = blob->size;
    binData = NULL;
    binLength = 0;
  }

  jsmn_parser parser;
  jsmn_init(&parser);

  jsmntok_t tokens[1024]; // TODO malloc or token queue
  int tokenCount = jsmn_parse(&parser, jsonData, jsonLength, tokens, 1024);
  lovrAssert(tokenCount >= 0, "Invalid JSON");
  lovrAssert(tokens[0].type == JSMN_OBJECT, "No root object");

  gltfInfo info = { 0 };
  size_t dataSize = 0;
  preparse(jsonData, tokens, tokenCount, &info, &dataSize);

  size_t offset = 0;
  model->data = calloc(1, dataSize);
  model->glbBlob = glb ? blob : NULL;
  model->accessorCount = info.accessors.count;
  model->animationCount = info.animations.count;
  model->blobCount = info.blobs.count;
  model->viewCount = info.views.count;
  model->primitiveCount = info.primitiveCount;
  model->meshCount = info.meshes.count;
  model->nodeCount = info.nodes.count;
  model->skinCount = info.skins.count;
  model->accessors = (ModelAccessor*) (model->data + offset), offset += info.accessors.count * sizeof(ModelAccessor);
  model->animations = (ModelAnimation*) (model->data + offset), offset += info.animations.count * sizeof(ModelAnimation);
  model->blobs = (ModelBlob*) (model->data + offset), offset += info.blobs.count * sizeof(ModelBlob);
  model->views = (ModelView*) (model->data + offset), offset += info.views.count * sizeof(ModelView);
  model->primitives = (ModelPrimitive*) (model->data + offset), offset += info.primitiveCount * sizeof(ModelPrimitive);
  model->meshes = (ModelMesh*) (model->data + offset), offset += info.meshes.count * sizeof(ModelMesh);
  model->nodes = (ModelNode*) (model->data + offset), offset += info.nodes.count * sizeof(ModelNode);
  model->skins = (ModelSkin*) (model->data + offset), offset += info.skins.count * sizeof(ModelSkin);
  model->nodeChildren = (uint32_t*) (model->data + offset), offset += info.childCount * sizeof(uint32_t);
  model->skinJoints = (uint32_t*) (model->data + offset), offset += info.jointCount * sizeof(uint32_t);

  parseAccessors(jsonData, info.accessors.token, model);
  parseAnimations(jsonData, info.animations.token, model);
  parseBlobs(jsonData, info.blobs.token, model, io, (void*) binData);
  parseViews(jsonData, info.views.token, model);
  parseNodes(jsonData, info.nodes.token, model);
  parseMeshes(jsonData, info.meshes.token, model);
  parseSkins(jsonData, info.skins.token, model);

  return model;
}

void lovrModelDataDestroy(void* ref) {
  //
}
