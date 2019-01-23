#include <stdio.h>
#include "../util.h"

const char* strings[] = {
  "Blob",
  "BoxShape",
  "CapsuleShape",
  "Collider",
  "Curve",
  "CylinderShape",
  "ModelData",
  "Pool",
  "Shape",
  "SoundData",
  "SphereShape",
  "TextureData",
  "VertexData",
  "World",
  NULL
};

int main(int argc, char** argv) {
  printf("#define HASH(s) HASH_##s\n\n");

  const char** string = strings;
  while (*string) {
    printf("#define HASH_%s %u\n", *string, hash(*string));
    string++;
  }

  return 0;
}
