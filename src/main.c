#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/common.h"
#include "include/chunk.h"
#include "include/util.h"
#include "include/vm.h"
#include "include/debug.h"

#define RESIN_VERSION "09-28-2022"

static void repl() {
  printf("Resin, Version %s.\n\n", RESIN_VERSION);
  char line[1024];
  for (;;) {
    printf("[resin]> ");
    if (!fgets(line, sizeof(line), stdin)) {
      printf("\n");
      break;
    }
    interpret(line);
  }
}

int main(int argc, const char* argv[]) {
  initVM();
  if (argc == 1) {
    repl();
  }
  else if (argc == 2 && strcmp(argv[1], "ver") == 0) {
    printf("Resin Compiler, Version %s.\n", RESIN_VERSION);
    exit(0);
  }
  else if (argc == 2) {
    runFile(argv[1]);
  }
  else {
    fprintf(stderr, "Usage: resin <path>\n");
    exit(64);
  }
  freeVM();
  return 0;
}