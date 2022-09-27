#ifndef resin_obj_h
#define resin_obj_h

#include "common.h"
#include "value.h"
#include "chunk.h"
#include "table.h"

#define OBJ_TYPE(value)   (AS_OBJ(value)->type)

#define IS_BOUND_METHOD(value)  isObjType(value, OBJ_BOUND_METHOD)
#define IS_CLASS(value)         isObjType(value, OBJ_CLASS)
#define IS_CLOSURE(value)       isObjType(value, OBJ_CLOSURE)
#define IS_FUNC(value)          isObjType(value, OBJ_FUNC)
#define IS_INSTANCE(value)      isObjType(value, OBJ_INSTANCE)
#define IS_NATIVE(value)        isObjType(value, OBJ_NATIVE)
#define IS_STR(value)           isObjType(value, OBJ_STR)
#define IS_LIST(value)          isObjType(value, OBJ_LIST)

#define AS_BOUND_METHOD(value)  ((ObjBoundMethod*)AS_OBJ(value))
#define AS_CLASS(value)         ((ObjClass*)AS_OBJ(value))
#define AS_CLOSURE(value)       ((ObjClosure*)AS_OBJ(value))
#define AS_FUNC(value)          ((ObjFunc*)AS_OBJ(value))
#define AS_INSTANCE(value)      ((ObjInstance*)AS_OBJ(value))
#define AS_NATIVE(value) \
  (((ObjNative*)AS_OBJ(value))->func)
#define AS_STR(value)           ((ObjStr*)AS_OBJ(value))
#define AS_CSTR(value)          (((ObjStr*)AS_OBJ(value))->chars)
#define AS_LIST(value)          ((ObjList*)AS_OBJ(value))

typedef enum {
  OBJ_BOUND_METHOD,
  OBJ_CLASS,
  OBJ_CLOSURE,
  OBJ_FUNC,
  OBJ_INSTANCE,
  OBJ_NATIVE,
  OBJ_STR,
  OBJ_UPVAL,
  OBJ_LIST
} ObjType;

struct Obj {
  ObjType type;
  bool isMarked;
  struct Obj* next;
};

typedef struct {
  Obj obj;
  int arity;
  int upvalCount;
  Chunk chunk;
  ObjStr* name;
} ObjFunc;

typedef Value (*NativeFn)(int argCount, Value* args);

typedef struct {
  Obj obj;
  NativeFn func;
} ObjNative;

struct ObjStr {
  Obj obj;
  int length;
  char* chars;
  uint32_t hash;
};

typedef struct ObjUpval {
  Obj obj;
  Value* location;
  Value closed;
  struct ObjUpval* next;
} ObjUpval;

typedef struct {
  Obj obj;
  ObjFunc* func;
  ObjUpval** upvals;
  int upvalCount;
} ObjClosure;

typedef struct {
  Obj obj;
  ObjStr* name;
  Table methods;
} ObjClass;

typedef struct {
  Obj obj;
  ObjClass* class;
  Table fields;
} ObjInstance;

typedef struct {
  Obj obj;
  Value receiver;
  ObjClosure* method;
} ObjBoundMethod;

typedef struct {
  Obj obj;
  int count;
  int capacity;
  Value* items;
} ObjList;

ObjBoundMethod* newBoundMethod(
  Value receiver,
  ObjClosure* method
);
ObjClass* newClass(ObjStr* name);
ObjClosure* newClosure(ObjFunc* func);
ObjFunc* newFunc();
ObjInstance* newInstance(ObjClass* class);
ObjNative* newNative(NativeFn func);
ObjList* newList();

void appendToList(ObjList* list, Value value);
void storeToList(ObjList* list, int index, Value value);
Value indexFromList(ObjList* list, int index);
void deleteFromList(ObjList* list, int index);
bool isValidListIndex(ObjList* list, int index);

ObjStr* takeStr(char* chars, int length);
ObjStr* copyStr(const char* chars, int length);
ObjUpval* newUpval(Value* slot);
void printObj(Value value);

static inline bool isObjType(Value value, ObjType type) {
  return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif