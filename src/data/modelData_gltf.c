#include "data/modelData.h"
#include "lib/math.h"
#include "lib/jsmn/jsmn.h"
#include <stdio.h>

// Notes:
//   - We parse in two passes.
//     - In the first pass we figure out how much memory we need to allocate.
//     - Then we allocate the memory and do a second pass to fill everything in.
//   - Plan is to make this parser destructive, so it mutates the input Blob in order to avoid doing
//     work in some situations, for speed.  May make sense to provide a non-destructive option.
//     - Currently this is most useful for reusing string memory by changing quotes to \0's.
//   - Caveats:
//     - The IO callback must not hang onto the filenames that are passed into it.

#define MAGIC_glTF 0x46546c67
#define MAGIC_JSON 0x4e4f534a
#define MAGIC_BIN 0x004e4942

#define KEY_EQ(k, s) !strncmp(k.data, s, k.length)
#define TOK_INT(j, t) strtol(j + t->start, NULL, 10)
#define TOK_FLOAT(j, t) strtof(j + t->start, NULL)

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
  struct { int count; jsmntok_t* token; } buffers;
  struct { int count; jsmntok_t* token; } bufferViews;
  struct { int count; jsmntok_t* token; } nodes;
  struct { int count; jsmntok_t* token; } meshes;
  int childCount;
  int primitiveCount;
} gltfInfo;

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
  float transform[16];
  uint32_t* children;
  uint32_t childCount;
  int mesh;
} gltfNode;

typedef struct {
  int material;
  int drawMode;
} gltfPrimitive;

typedef struct {
  gltfPrimitive* primitives;
  uint32_t primitiveCount;
} gltfMesh;

typedef struct {
  Ref ref;
  uint8_t* data;
  gltfBuffer* buffers;
  gltfBufferView* bufferViews;
  gltfNode* nodes;
  gltfMesh* meshes;
  gltfPrimitive* primitives;
  uint32_t* childMap;
} gltfModel;

static int nomString(const char* data, jsmntok_t* token, gltfString* string) {
  lovrAssert(token->type == JSMN_STRING, "Expected string");
  string->data = (char*) data + token->start;
  string->length = token->end - token->start;
  return 1;
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
        gltfString key;
        token += nomString(json, token, &key);
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
    gltfString key;
    token += nomString(json, token, &key);

    if (KEY_EQ(key, "buffers")) {
      info->buffers.token = token;
      info->buffers.count = token->size;
      *dataSize += info->buffers.count * sizeof(gltfBuffer);
      token += nomValue(json, token, 1, 0);
    } else if (KEY_EQ(key, "bufferViews")) {
      info->bufferViews.token = token;
      info->bufferViews.count = token->size;
      *dataSize += info->bufferViews.count * sizeof(gltfBufferView);
      token += nomValue(json, token, 1, 0);
    } else if (KEY_EQ(key, "nodes")) {
      info->nodes.token = token;
      info->nodes.count = token->size;
      *dataSize += info->nodes.count * sizeof(gltfNode);
      token = aggregate(json, token, "children", &info->childCount);
    } else if (KEY_EQ(key, "meshes")) {
      info->meshes.token = token;
      info->meshes.count = token->size;
      *dataSize += info->meshes.count * sizeof(gltfMesh);
      token = aggregate(json, token, "primitives", &info->primitiveCount);
      *dataSize += info->primitiveCount * sizeof(gltfPrimitive);
    } else {
      token += nomValue(json, token, 1, 0); // Skip
    }
  }
}

static void parseBuffers(const char* json, jsmntok_t* token, gltfModel* gltf, ModelDataIO io, void* binData) {
  if (!token) return;

  int count = (token++)->size;
  for (int i = 0; i < count; i++) {
    gltfBuffer* buffer = &gltf->buffers[i];
    gltfString key;
    int keyCount = (token++)->size;
    size_t bytesRead = 0;
    bool hasUri = false;

    for (int k = 0; k < keyCount; k++) {
      token += nomString(json, token, &key);
      if (KEY_EQ(key, "byteLength")) {
        buffer->size = TOK_INT(json, token), token++;
      } else if (KEY_EQ(key, "uri")) {
        hasUri = true;
        gltfString filename;
        token += nomString(json, token, &filename);
        filename.data[filename.length] = '\0'; // Change the quote into a terminator (I'll be b0k)
        buffer->data = io.read(filename.data, &bytesRead);
        lovrAssert(buffer->data, "Unable to read %s", filename.data);
      } else {
        token += nomValue(json, token, 1, 0); // Skip
      }
    }

    if (hasUri) {
      lovrAssert(bytesRead == buffer->size, "Couldn't read all of buffer data");
    } else {
      lovrAssert(binData && i == 0, "Buffer is missing URI");
      buffer->data = binData;
    }
  }
}

static void parseBufferViews(const char* json, jsmntok_t* token, gltfModel* gltf) {
  if (!token) return;

  int count = (token++)->size;
  for (int i = 0; i < count; i++) {
    gltfBufferView* bufferView = &gltf->bufferViews[i];
    gltfString key;
    int keyCount = (token++)->size;

    for (int k = 0; k < keyCount; k++) {
      token += nomString(json, token, &key);
      if (KEY_EQ(key, "buffer")) {
        bufferView->buffer = TOK_INT(json, token), token++;
      } else if (KEY_EQ(key, "byteOffset")) {
        bufferView->offset = TOK_INT(json, token), token++;
      } else if (KEY_EQ(key, "byteLength")) {
        bufferView->length = TOK_INT(json, token), token++;
      } else if (KEY_EQ(key, "byteStride")) {
        bufferView->stride = TOK_INT(json, token), token++;
      } else {
        token += nomValue(json, token, 1, 0); // Skip
      }
    }
  }
}

static void parseNodes(const char* json, jsmntok_t* token, gltfModel* gltf) {
  if (!token) return;

  int childIndex = 0;
  int count = (token++)->size; // Enter array
  for (int i = 0; i < count; i++) {
    gltfNode* node = &gltf->nodes[i];
    float translation[3] = { 0, 0, 0 };
    float rotation[4] = { 0, 0, 0, 0 };
    float scale[3] = { 1, 1, 1 };
    bool matrix = false;

    gltfString key;
    int keyCount = (token++)->size; // Enter object
    for (int k = 0; k < keyCount; k++) {
      token += nomString(json, token, &key);

      if (KEY_EQ(key, "children")) {
        node->children = &gltf->childMap[childIndex];
        node->childCount = (token++)->size;
        for (uint32_t j = 0; j < node->childCount; j++) {
          gltf->childMap[childIndex++] = TOK_INT(json, token), token++;
        }
      } else if (KEY_EQ(key, "mesh")) {
        node->mesh = TOK_INT(json, token), token++;
      } else if (KEY_EQ(key, "matrix")) {
        lovrAssert(token->size == 16, "Node matrix needs 16 elements");
        matrix = true;
        for (int j = 0; j < token->size; j++) {
          node->transform[j] = TOK_FLOAT(json, token), token++;
        }
      } else if (KEY_EQ(key, "translation")) {
        lovrAssert(token->size == 3, "Node translation needs 3 elements");
        translation[0] = TOK_FLOAT(json, token), token++;
        translation[1] = TOK_FLOAT(json, token), token++;
        translation[2] = TOK_FLOAT(json, token), token++;
      } else if (KEY_EQ(key, "rotation")) {
        lovrAssert(token->size == 4, "Node rotation needs 4 elements");
        rotation[0] = TOK_FLOAT(json, token), token++;
        rotation[1] = TOK_FLOAT(json, token), token++;
        rotation[2] = TOK_FLOAT(json, token), token++;
        rotation[3] = TOK_FLOAT(json, token), token++;
      } else if (KEY_EQ(key, "scale")) {
        lovrAssert(token->size == 3, "Node scale needs 3 elements");
        scale[0] = TOK_FLOAT(json, token), token++;
        scale[1] = TOK_FLOAT(json, token), token++;
        scale[2] = TOK_FLOAT(json, token), token++;
      } else {
        token += nomValue(json, token, 1, 0); // Skip
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

static jsmntok_t* parsePrimitive(const char* json, jsmntok_t* token, int index, gltfModel* gltf) {
  gltfString key;
  gltfPrimitive* primitive = &gltf->primitives[index];
  int keyCount = (token++)->size; // Enter object
  for (int k = 0; k < keyCount; k++) {
    token += nomString(json, token, &key);

    if (KEY_EQ(key, "material")) {
      primitive->material = TOK_INT(json, token), token++;
    } else if (KEY_EQ(key, "mode")) {
      primitive->drawMode = TOK_INT(json, token), token++;
    } else {
      token += nomValue(json, token, 1, 0); // Skip
    }
  }
  return token;
}

static void parseMeshes(const char* json, jsmntok_t* token, gltfModel* gltf) {
  if (!token) return;

  int primitiveIndex = 0;
  int count = (token++)->size; // Enter array
  for (int i = 0; i < count; i++) {
    gltfString key;
    gltfMesh* mesh = &gltf->meshes[i];
    int keyCount = (token++)->size; // Enter object
    for (int k = 0; k < keyCount; k++) {
      token += nomString(json, token, &key);

      if (KEY_EQ(key, "primitives")) {
        mesh->primitives = &gltf->primitives[primitiveIndex];
        mesh->primitiveCount = (token++)->size;
        for (uint32_t j = 0; j < mesh->primitiveCount; j++) {
          token = parsePrimitive(json, token, primitiveIndex++, gltf);
        }
      } else {
        token += nomValue(json, token, 1, 0); // Skip
      }
    }
  }
}

ModelData* lovrModelDataInitFromGltf(ModelData* modelData, Blob* blob, ModelDataIO io) {
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
  gltfModel model;
  model.data = calloc(1, dataSize);
  model.buffers = (gltfBuffer*) (model.data + offset), offset += info.buffers.count * sizeof(gltfBuffer);
  model.bufferViews = (gltfBufferView*) (model.data + offset), offset += info.bufferViews.count * sizeof(gltfBufferView);
  model.nodes = (gltfNode*) (model.data + offset), offset += info.nodes.count * sizeof(gltfNode);
  model.meshes = (gltfMesh*) (model.data + offset), offset += info.meshes.count * sizeof(gltfMesh);
  model.primitives = (gltfPrimitive*) (model.data + offset), offset += info.primitiveCount * sizeof(gltfPrimitive);
  model.childMap = (uint32_t*) (model.data + offset), offset += info.childCount * sizeof(uint32_t);

  parseBuffers(jsonData, info.buffers.token, &model, io, (void*) binData);
  parseBufferViews(jsonData, info.bufferViews.token, &model);
  parseNodes(jsonData, info.nodes.token, &model);
  parseMeshes(jsonData, info.meshes.token, &model);

  return NULL;
}
