#ifndef resin_chunk_h
#define resin_chunk_h

#include "common.h"
#include "value.h"

typedef enum {
  OP_CONST,
  OP_NIL,
  OP_TRUE, OP_FALSE,
  OP_DUP,
  OP_POP,
  OP_GET_LOCAL, OP_SET_LOCAL,
  OP_GET_GLOBAL, OP_DEF_GLOBAL, OP_SET_GLOBAL,
  OP_GET_UPVAL, OP_SET_UPVAL,
  OP_GET_PROP, OP_SET_PROP,
  OP_BUILD_LIST, OP_INDEX_SUB, OP_STORE_SUB,
  OP_GET_SUPER,
  OP_EQU,
  OP_GT, OP_LT,
  OP_GT_EQU, OP_LT_EQU,
  OP_ADD, OP_SUB,
  OP_MUL, OP_DIV, OP_MOD, OP_POW,
  OP_NOT,
  OP_NOT_EQU,
  OP_NEGATE,
  OP_THROW,
  OP_JMP,
  OP_JMPF,
  OP_LOOP,
  OP_CALL,
  OP_INVOKE,
  OP_INVOKE_SUPER,
  OP_CLOSURE,
  OP_CLOSE_UPVAL,
  OP_RETURN,
  OP_CLASS,
  OP_INHERIT,
  OP_METHOD
} OpCode;

typedef struct {
  int offset;
  int line;
} LineStart;

typedef struct {
  int count;
  int capacity;
  uint8_t* code;
  int lineCount;
  int lineCapacity;
  LineStart* lines;
  ValueArray constants;
} Chunk;


void initChunk(Chunk* chunk);
void freeChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte, int line);
int addConst(Chunk* chunk, Value value);
int getLine(Chunk* chunk, int instruction);

#endif