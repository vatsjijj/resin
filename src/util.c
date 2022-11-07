#include <stdio.h>
#include <stdlib.h>
#include "include/util.h"
#include "include/vm.h"

char* readFile(const char* path) {
  FILE* file = fopen(path, "rb");
  if (file == NULL) {
    fprintf(stderr, "Could not open file '%s'\n", path);
    exit(74);
  }
  fseek(file, 0L, SEEK_END);
  size_t fileSize = ftell(file);
  rewind(file);
  char* buffer = (char*)malloc(fileSize + 1);
  if (buffer == NULL) {
    if (system("/usr/bin/fortune 2> /dev/null") != 0) {
      printf("How dare they!!\n");
    }
    fprintf(stderr, "\nNot enough memory to read '%s'\n", path);
    exit(74);
  }
  size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
  if (bytesRead < fileSize) {
    fprintf(stderr, "Could not read file '%s'\n", path);
    exit(74);
  }
  buffer[bytesRead] = '\0';
  fclose(file);
  return buffer;
}

void runFile(const char* path) {
  char* source = readFile(path);
  InterpretResult result = interpret(source);
  free(source);
  if (result == INTERPRET_COMPILE_ERROR) {
    exit(65);
  }
  if (result == INTERPRET_RUNTIME_ERROR) {
    exit(70);
  }
}