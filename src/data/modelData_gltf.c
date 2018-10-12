#include "data/modelData.h"
#include "math/mat4.h"
#include "lib/jsmn/jsmn.h"
#include <stdio.h>

#define MAGIC_glTF 0x46546c67
#define MAGIC_JSON 0x4e4f534a
#define MAGIC_BIN 0x004e4942

#define G_STR_EQ(s, t) !strncmp(s.data, t, s.length)
#define G_INT(s) strtol(s, NULL, 10)
#define G_FLOAT(s) strtof(s, NULL)

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
  int mesh;
  float transform[16];
  uint32_t childrenIndex; // Index into children array where children start
  uint32_t childrenCount;
} gltfNode;

typedef struct {
  int count;
  jsmntok_t* token;
} gltfProperty;

typedef struct {
  gltfProperty nodes;
  gltfProperty children;
} gltfInfo;

typedef struct {
  Ref ref;
  uint8_t* data;
  gltfNode* nodes;
  uint32_t* children;
} gltfModel;

static int nomString(const char* data, jsmntok_t* token, gltfString* string) {
  lovrAssert(token->type == JSMN_STRING, "Expected string");
  string->data = (char*) data + token->start;
  string->length = token->end - token->start;
  return 1;
}

static int nomValue(const char* data, jsmntok_t* token, int count, int sum) {
  if (count == 0) {
    return sum;
  }

  switch (token->type) {
    case JSMN_OBJECT: return nomValue(data, token + 1, count - 1 + 2 * token->size, sum + 1);
    case JSMN_ARRAY: return nomValue(data, token + 1, count - 1 + token->size, sum + 1);
    default: return nomValue(data, token + 1, count - 1, sum + 1);
  }
}

static void preparse(const char* json, jsmntok_t* tokens, int tokenCount, gltfInfo* info, size_t* dataSize) {
  for (jsmntok_t* token = tokens + 1; token < tokens + tokenCount;) { // +1 to skip root object
    gltfString key;
    token += nomString(json, token, &key);

    if (G_STR_EQ(key, "nodes")) {
      info->nodes.token = token;
      info->nodes.count = (token++)->size; // Enter array
      *dataSize += info->nodes.count * sizeof(gltfNode);

      for (int i = 0; i < info->nodes.count; i++) {
        if (token->size > 0) {
          int keys = (token++)->size; // Enter object
          for (int k = 0; k < keys; k++) {
            gltfString nodeKey;
            token += nomString(json, token, &nodeKey);
            if (G_STR_EQ(nodeKey, "children")) {
              info->children.count += token->size;
            }
            token += nomValue(json, token, 1, 0);
          }
        }
      }
    } else {
      token += nomValue(json, token, 1, 0); // Skip
    }
  }
}

static void parseNodes(const char* json, jsmntok_t* tokens, gltfInfo* info, gltfModel* model) {
  if (!info->nodes.token) {
    return;
  }

  int childIndex = 0;
  int nodeCount = info->nodes.count;
  jsmntok_t* token = info->nodes.token++; // Enter array
  for (int i = 0; i < nodeCount; i++) {
    gltfNode* node = &model->nodes[i];
    node->childrenIndex = -1;
    float translation[3] = { 0, 0, 0 };
    float rotation[4] = { 0, 0, 0, 0 };
    float scale[3] = { 1, 1, 1 };
    bool matrix = false;

    gltfString key;
    int keyCount = (token++)->size; // Enter object
    for (int k = 0; k < keyCount; k++) {

      // Read key
      token += nomString(json, token, &key);

      // Process value
      if (G_STR_EQ(key, "children")) {
        node->childrenIndex = childIndex;
        node->childrenCount = token->size, token++;
        for (uint32_t j = 0; j < node->childrenCount; j++) {
          model->children[childIndex++] = G_INT(json + token->start), token++;
        }
      } else if (G_STR_EQ(key, "mesh")) {
        node->mesh = G_INT(json + token->start), token++;
      } else if (G_STR_EQ(key, "matrix")) {
        lovrAssert(token->size == 16, "Node matrix needs 16 elements");
        matrix = true;
        for (int j = 0; j < token->size; j++) {
          node->transform[j] = G_FLOAT(json + token->start), token++;
        }
      } else if (G_STR_EQ(key, "translation")) {
        lovrAssert(token->size == 3, "Node translation needs 3 elements");
        for (int j = 0; j < 3; j++) {
          translation[j] = G_FLOAT(json + token->start), token++;
        }
      } else if (G_STR_EQ(key, "rotation")) {
        lovrAssert(token->size == 4, "Node rotation needs 4 elements");
        for (int j = 0; j < 4; j++) {
          rotation[j] = G_FLOAT(json + token->start), token++;
        }
      } else if (G_STR_EQ(key, "scale")) {
        lovrAssert(token->size == 3, "Node scale needs 3 elements");
        for (int j = 0; j < 3; j++) {
          scale[j] = G_FLOAT(json + token->start), token++;
        }
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

ModelData* lovrModelDataInitFromGltf(ModelData* modelData, Blob* blob) {
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

  jsmntok_t tokens[256];
  int tokenCount = jsmn_parse(&parser, jsonData, jsonLength, tokens, 256);
  lovrAssert(tokenCount >= 0, "Invalid JSON");
  lovrAssert(tokens[0].type == JSMN_OBJECT, "No root object");

  gltfInfo info = { 0 };
  size_t dataSize = 0;
  preparse(jsonData, tokens, tokenCount, &info, &dataSize);

  size_t offset = 0;
  gltfModel model = { 0 };
  model.data = malloc(dataSize);
  model.nodes = (gltfNode*) (model.data + offset), offset += info.nodes.count * sizeof(gltfNode);
  model.children = (uint32_t*) (model.data + offset), offset += info.children.count * sizeof(uint32_t);

  parseNodes(jsonData, tokens, &info, &model);

  return NULL;
}
