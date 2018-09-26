#include "data/modelData.h"
#include "lib/jsmn/jsmn.h"
#include <stdio.h>

#define MAGIC_glTF 0x46546c67
#define MAGIC_JSON 0x4e4f534a
#define MAGIC_BIN 0x004e4942

typedef struct {
  char* string;
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
  gltfString copyright;
  gltfString generator;
  gltfString version;
  gltfString minVersion;
} gltfAsset;

static int nomString(const char* data, jsmntok_t* token, char** str, size_t* len) {
  lovrAssert(token->type == JSMN_STRING, "Expected string");
  *str = (char*) data + token->start;
  *len = token->end - token->start;
  return 1;
}

static int nomValue(const char* data, jsmntok_t* token, int count, int sum) {
  if (count == 0) {
    return sum;
  }

  switch (token->type) {
    case JSMN_OBJECT:
      return nomValue(data, token + 1, count - 1 + 2 * token->size, sum + 1);
    case JSMN_ARRAY:
      return nomValue(data, token + 1, count - 1 + token->size, sum + 1);
    default:
      return nomValue(data, token + 1, count - 1, sum + 1);
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

  for (jsmntok_t* token = tokens + 1; token < tokens + tokenCount;) {
    char* key;
    size_t keyLength;
    token += nomString(jsonData, token, &key, &keyLength);
    printf("%.*s\n", (int) keyLength, key);

    token += nomValue(jsonData, token, 1, 0);
  }

  return NULL;
}
