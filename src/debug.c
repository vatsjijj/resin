#include <stdio.h>
#include "include/debug.h"
#include "include/value.h"
#include "include/object.h"

static int simpleInstruction(const char* name, int offset) {
  printf("%s\n", name);
  return offset + 1;
}

static int byteInstruction(
  const char* name,
  Chunk* chunk,
  int offset
) {
  uint8_t slot = chunk->code[offset + 1];
  printf("%-16s %4d\n", name, slot);
  return offset + 2;
}

static int jmpInstruction(
  const char* name,
  int sign,
  Chunk* chunk,
  int offset
) {
  uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
  jump |= chunk->code[offset + 2];
  printf(
    "%-16s %4d -> %d\n",
    name,
    offset,
    offset + 3 + sign * jump
  );
  return offset + 3;
}

static int constInstruction(
  const char* name,
  Chunk* chunk,
  int offset
) {
  uint8_t constant = chunk->code[offset + 1];
  printf("%-16s %4d '", name, constant);
  printValue(chunk->constants.values[constant]);
  printf("'\n");
  return offset + 2;
}

static int invokeInstruction(
  const char* name,
  Chunk* chunk,
  int offset
) {
  uint8_t constant = chunk->code[offset + 1];
  uint8_t argCount = chunk->code[offset + 2];
  printf("%-16s (%d args) %4d '", name, argCount, constant);
  printValue(chunk->constants.values[constant]);
  printf("'\n");
  return offset + 3;
}

int disassembleInstruction(Chunk* chunk, int offset) {
  printf("%04d ", offset);
  int line = getLine(chunk, offset);
  if (
    offset > 0 &&
    line == getLine(chunk, offset - 1)
  ) {
    printf("\t| ");
  }
  else {
    printf("%4d ", line);
  }
  uint8_t instruction = chunk->code[offset];
  switch (instruction) {
    case OP_CONST:
      return constInstruction("OP_CONST", chunk, offset);
    case OP_NIL:
      return simpleInstruction("OP_NIL", offset);
    case OP_TRUE:
      return simpleInstruction("OP_TRUE", offset);
    case OP_FALSE:
      return simpleInstruction("OP_FALSE", offset);
    case OP_DUP:
      return simpleInstruction("OP_DUP", offset);
    case OP_POP:
      return simpleInstruction("OP_POP", offset);
    case OP_GET_LOCAL:
      return byteInstruction("OP_GET_LOCAL", chunk, offset);
    case OP_SET_LOCAL:
      return byteInstruction("OP_SET_LOCAL", chunk, offset);
    case OP_GET_GLOBAL:
      return constInstruction("OP_GET_GLOBAL", chunk, offset);
    case OP_DEF_GLOBAL:
      return constInstruction(
        "OP_DEF_GLOBAL",
        chunk,
        offset
      );
    case OP_SET_GLOBAL:
      return constInstruction("OP_SET_GLOBAL", chunk, offset);
    case OP_GET_UPVAL:
      return byteInstruction("OP_GET_UPVAL", chunk, offset);
    case OP_SET_UPVAL:
      return byteInstruction("OP_SET_UPVAL", chunk, offset);
    case OP_GET_PROP:
      return constInstruction("OP_GET_PROP", chunk, offset);
    case OP_SET_PROP:
      return constInstruction("OP_SET_PROP", chunk, offset);
    case OP_GET_SUPER:
      return constInstruction("OP_GET_SUPER", chunk, offset);
    // These will be simple instructions for now.
    case OP_BUILD_LIST:
      return simpleInstruction("OP_BUILD_LIST", offset);
    case OP_INDEX_SUB:
      return simpleInstruction("OP_INDEX_SUB", offset);
    case OP_STORE_SUB:
      return simpleInstruction("OP_STORE_SUB", offset);
    case OP_EQU:
      return simpleInstruction("OP_EQU", offset);
    case OP_GT:
      return simpleInstruction("OP_GT", offset);
    case OP_LT:
      return simpleInstruction("OP_LT", offset);
    case OP_GT_EQU:
      return simpleInstruction("OP_GT_EQU", offset);
    case OP_LT_EQU:
      return simpleInstruction("OP_LT_EQU", offset);
    case OP_NOT_EQU:
      return simpleInstruction("OP_NOT_EQU", offset);
    case OP_ADD:
      return simpleInstruction("OP_ADD", offset);
    case OP_SUB:
      return simpleInstruction("OP_SUB", offset);
    case OP_MUL:
      return simpleInstruction("OP_MUL", offset);
    case OP_DIV:
      return simpleInstruction("OP_DIV", offset);
    case OP_MOD:
      return simpleInstruction("OP_MOD", offset);
    case OP_NOT:
      return simpleInstruction("OP_NOT", offset);
    case OP_NEGATE:
      return simpleInstruction("OP_NEGATE", offset);
    case OP_JMP:
      return jmpInstruction("OP_JMP", 1, chunk, offset);
    case OP_JMPF:
      return jmpInstruction("OP_JMPF", 1, chunk, offset);
    case OP_LOOP:
      return jmpInstruction("OP_LOOP", -1, chunk, offset);
    case OP_CALL:
      return byteInstruction("OP_CALL", chunk, offset);
    case OP_INVOKE:
      return invokeInstruction("OP_INVOKE", chunk, offset);
    case OP_INVOKE_SUPER:
      return invokeInstruction("OP_INVOKE_SUPER", chunk, offset);
    case OP_CLOSURE: {
      offset++;
      uint8_t constant = chunk->code[offset++];
      printf("%-16s %4d ", "OP_CLOSURE", constant);
      printValue(chunk->constants.values[constant]);
      printf("\n");
      ObjFunc* func = AS_FUNC(
        chunk->constants.values[constant]
      );
      for (int j = 0; j < func->upvalCount; j++) {
        int isLocal = chunk->code[offset++];
        int index = chunk->code[offset++];
        printf(
          "%04d\t|\t\t\t%s %d\n",
          offset - 2, isLocal ? "local" : "upval",
          index
        );
      }
      return offset;
    }
    case OP_CLOSE_UPVAL:
      return simpleInstruction("OP_CLOSE_UPVAL", offset);
    case OP_RETURN:
      return simpleInstruction("OP_RETURN", offset);
    case OP_CLASS:
      return constInstruction("OP_CLASS", chunk, offset);
    case OP_INHERIT:
      return simpleInstruction("OP_INHERIT", offset);
    case OP_METHOD:
      return constInstruction("OP_METHOD", chunk, offset);
    default:
      printf("Unknown opcode: %d\n", instruction);
      return offset + 1;
  }
}

void disassembleChunk(Chunk* chunk, const char* name) {
  printf("[ %s ]\n", name);

  for (int offset = 0; offset < chunk->count;) {
    offset = disassembleInstruction(chunk, offset);
  }
}