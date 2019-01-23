#include <stdio.h>
#include "../util.h"

const char* strings[] = {
  "test",
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
