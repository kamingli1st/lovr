#include "data/modelData.h"
#include "lib/jsmn/jsmn.h"
#include <stdio.h>

#define MAGIC_glTF 0x46546c67
#define MAGIC_JSON 0x4e4f534a
#define MAGIC_BIN 0x004e4942

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
  gltfString version;
  int accessorCount;
  int animationCount;
  int bufferCount;
  int bufferViewCount;
  int cameraCount;
  int imageCount;
  int materialCount;
  int meshCount;
  int nodeCount;
  int samplerCount;
  int sceneCount;
  int skinCount;
  int textureCount;
} gltfInfo;

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

  // Parse info
  gltfInfo info = { 0 };
  for (jsmntok_t* token = tokens + 1; token < tokens + tokenCount;) {
    gltfString key;
    token += nomString(jsonData, token, &key);

    if (!strncmp(key.data, "asset", key.length)) {
      int keys = token->size;
      token++;

      for (int i = 0; i < keys; i++) {
        gltfString assetKey;
        token += nomString(jsonData, token, &assetKey);
        if (!strncmp(assetKey.data, "version", assetKey.length)) {
          token += nomString(jsonData, token, &info.version);
        } else {
          token += nomValue(jsonData, token, 1, 0);
        }
      }

      continue;
    } else if (!strncmp(key.data, "scenes", key.length)) {
      info.sceneCount = token->size;
    } else if (!strncmp(key.data, "nodes", key.length)) {
      info.nodeCount = token->size;
    } else if (!strncmp(key.data, "meshes", key.length)) {
      info.meshCount = token->size;
    } else if (!strncmp(key.data, "accessors", key.length)) {
      info.accessorCount = token->size;
    } else if (!strncmp(key.data, "materials", key.length)) {
      info.materialCount = token->size;
    } else if (!strncmp(key.data, "bufferViews", key.length)) {
      info.bufferViewCount = token->size;
    } else if (!strncmp(key.data, "buffers", key.length)) {
      info.bufferCount = token->size;
    } else if (!strncmp(key.data, "animations", key.length)) {
      info.animationCount = token->size;
    } else if (!strncmp(key.data, "images", key.length)) {
      info.imageCount = token->size;
    } else if (!strncmp(key.data, "samplers", key.length)) {
      info.samplerCount = token->size;
    } else if (!strncmp(key.data, "skins", key.length)) {
      info.skinCount = token->size;
    } else if (!strncmp(key.data, "textures", key.length)) {
      info.textureCount = token->size;
    } else if (!strncmp(key.data, "cameras", key.length)) {
      info.cameraCount = token->size;
    }

    token += nomValue(jsonData, token, 1, 0);
  }

  printf("version %.*s\n", (int) info.version.length, info.version.data);
  printf("accessorCount %d\n", info.accessorCount);
  printf("animationCount %d\n", info.animationCount);
  printf("bufferCount %d\n", info.bufferCount);
  printf("bufferViewCount %d\n", info.bufferViewCount);
  printf("cameraCount %d\n", info.cameraCount);
  printf("imageCount %d\n", info.imageCount);
  printf("materialCount %d\n", info.materialCount);
  printf("meshCount %d\n", info.meshCount);
  printf("nodeCount %d\n", info.nodeCount);
  printf("samplerCount %d\n", info.samplerCount);
  printf("sceneCount %d\n", info.sceneCount);
  printf("skinCount %d\n", info.skinCount);
  printf("textureCount %d\n", info.textureCount);

  return NULL;
}
