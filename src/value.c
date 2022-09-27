#include <stdio.h>
#include <string.h>
#include "include/memory.h"
#include "include/value.h"
#include "include/object.h"

void initValueArray(ValueArray* array) {
  array->values = NULL;
  array->capacity = 0;
  array->count = 0;
}

void writeValueArray(ValueArray* array, Value value) {
  if (array->capacity < array->count + 1) {
    int oldCapacity = array->capacity;
    array->capacity = GROW_CAPACITY(oldCapacity);
    array->values = GROW_ARRAY(
      Value, array->values,
      oldCapacity, array->capacity
    );
  }
  array->values[array->count] = value;
  array->count++;
}

void freeValueArray(ValueArray* array) {
  FREE_ARRAY(Value, array->values, array->capacity);
  initValueArray(array);
}

void printValue(Value value) {
  #ifdef NAN_BOXING
  if (IS_BOOL(value)) {
    printf(AS_BOOL(value) ? "true" : "false");
  }
  else if (IS_NIL(value)) {
    printf("nil");
  }
  else if (IS_NUM(value)) {
    printf("%.16g", AS_NUM(value));
  }
  else if (IS_OBJ(value)) {
    printObj(value);
  }
  #else
  switch (value.type) {
    case VAL_BOOL:
      printf(AS_BOOL(value) ? "true" : "false");
      break;
    case VAL_NIL: printf("nil"); break;
    case VAL_NUM: printf("%.16g", AS_NUM(value)); break;
    case VAL_OBJ: printObj(value); break;
  }
  #endif
}

bool valsEqu(Value a, Value b) {
  #ifdef NAN_BOXING
  if (IS_NUM(a) && IS_NUM(b)) {
    return AS_NUM(a) == AS_NUM(b);
  }
  return a == b;
  #else
  if (a.type != b.type) {
    return false;
  }
  switch (a.type) {
    case VAL_BOOL: return AS_BOOL(a) == AS_BOOL(b);
    case VAL_NIL: return true;
    case VAL_NUM: return AS_NUM(a) == AS_NUM(b);
    case VAL_OBJ: return AS_OBJ(a) == AS_OBJ(b);
    default: return false;
  }
  #endif
}

bool valsNotEqu(Value a, Value b) {
  #ifdef NAN_BOXING
  if (IS_NUM(a) && IS_NUM(b)) {
    return AS_NUM(a) != AS_NUM(b);
  }
  return a != b;
  #else
  if (a.type != b.type) {
    return true;
  }
  switch (a.type) {
    case VAL_BOOL: return AS_BOOL(a) != AS_BOOL(b);
    case VAL_NIL: return true;
    case VAL_NUM: return AS_NUM(a) != AS_NUM(b);
    case VAL_OBJ: return AS_OBJ(a) != AS_OBJ(b);
    default: return true;
  }
  #endif
}

bool valsGtEqu(Value a, Value b) {
  #ifdef NAN_BOXING
  if (IS_NUM(a) && IS_NUM(b)) {
    return AS_NUM(a) >= AS_NUM(b);
  }
  return a >= b;
  #else
  // If the types are not the same,
  // return false. I feel as if this
  // could be some strange behavior,
  // but it'll work.
  if (a.type != b.type) {
    return false;
  }
  switch (a.type) {
    case VAL_BOOL: return AS_BOOL(a) >= AS_BOOL(b);
    case VAL_NIL: return false;
    case VAL_NUM: return AS_NUM(a) >= AS_NUM(b);
    case VAL_OBJ: return AS_OBJ(a) >= AS_OBJ(b);
    default: return false;
  }
  #endif
}

bool valsLtEqu(Value a, Value b) {
  #ifdef NAN_BOXING
  if (IS_NUM(a) && IS_NUM(b)) {
    return AS_NUM(a) <= AS_NUM(b);
  }
  return a <= b;
  #else
  // If the types are not the same,
  // return false, same as above.
  if (a.type != b.type) {
    return false;
  }
  switch (a.type) {
    case VAL_BOOL: return AS_BOOL(a) <= AS_BOOL(b);
    case VAL_NIL: return true;
    case VAL_NUM: return AS_NUM(a) <= AS_NUM(b);
    case VAL_OBJ: return AS_OBJ(a) <= AS_OBJ(b);
    default: return false;
  }
  #endif
}

bool valsGt(Value a, Value b) {
  #ifdef NAN_BOXING
  if (IS_NUM(a) && IS_NUM(b)) {
    return AS_NUM(a) > AS_NUM(b);
  }
  return a > b;
  #else
  if (a.type != b.type) {
    return false;
  }
  switch (a.type) {
    case VAL_BOOL: return AS_BOOL(a) > AS_BOOL(b);
    case VAL_NIL: return false;
    case VAL_NUM: return AS_NUM(a) > AS_NUM(b);
    case VAL_OBJ: return AS_OBJ(a) > AS_OBJ(b);
    default: return false;
  }
  #endif
}

bool valsLt(Value a, Value b) {
  #ifdef NAN_BOXING
  if (IS_NUM(a) && IS_NUM(b)) {
    return AS_NUM(a) < AS_NUM(b);
  }
  return a < b;
  #else
  if (a.type != b.type) {
    return false;
  }
  switch (a.type) {
    case VAL_BOOL: return AS_BOOL(a) < AS_BOOL(b);
    case VAL_NIL: return false;
    case VAL_NUM: return AS_NUM(a) < AS_NUM(b);
    case VAL_OBJ: return AS_OBJ(a) < AS_OBJ(b);
    default: return false;
  }
  #endif
}