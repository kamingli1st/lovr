#include "data/modelData.h"
#include "lib/jsmn/jsmn.h"

#define MAGIC_glTF 0x46546c67
#define MAGIC_JSON 0x4e4f534a
#define MAGIC_BIN 0x004e4942

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
  const char* copyright;
  const char* generator;
  const char* version;
  const char* minVersion;
} gltfAsset;

ModelData* lovrModelDataInitFromGltf(ModelData* modelData, Blob* blob) {
  uint8_t* data = blob->data;
  gltfHeader* header = (gltfHeader*) data;
  bool glb = header->magic == MAGIC_glTF;
  uint8_t* jsonData, binData;
  size_t jsonLength, binLength;

  if (glb) {
    gltfChunkHeader* jsonHeader = &header[1];
    lovrAssert(jsonHeader->type == MAGIC_JSON, "Invalid JSON header");

    jsonData = &jsonHeader[1];
    jsonLength = jsonHeader->length;

    gltfChunkHeader* binHeader = &jsonData[jsonLength];
    lovrAssert(binHeader->type == MAGIC_BIN, "Invalid BIN header");

    binData = &binHeader[1];
    binLength = binHeader->length;
  } else {
    jsonData = data;
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

  for (int t = 1; t < tokenCount; t++) {
    jsmntok_t* token = &tokens[t];
  }

hell:
  return NULL;
}
