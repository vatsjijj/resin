#include <stdio.h>
#include <string.h>
#include "include/memory.h"
#include "include/value.h"
#include "include/object.h"
#include "include/vm.h"
#include "include/table.h"

#define ALLOCATE_OBJ(type, objType) \
  (type*)allocObj(sizeof(type), objType)

static Obj* allocObj(size_t size, ObjType type) {
  Obj* object = (Obj*)reallocate(NULL, 0, size);
  object->type = type;
  object->isMarked = false;
  object->next = vm.objects;
  vm.objects = object;
  #ifdef DEBUG_LOG_GC
  printf("[%p] allocate %zu for %d\n", (void*)object, size, type);
  #endif
  return object;
}

ObjBoundMethod* newBoundMethod(
  Value receiver,
  ObjClosure* method
) {
  ObjBoundMethod* bound = ALLOCATE_OBJ(
    ObjBoundMethod,
    OBJ_BOUND_METHOD
  );
  bound->receiver = receiver;
  bound->method = method;
  return bound;
}

ObjClass* newClass(ObjStr* name) {
  ObjClass* class = ALLOCATE_OBJ(ObjClass, OBJ_CLASS);
  class->name = name;
  initTable(&class->methods);
  return class;
}

ObjClosure* newClosure(ObjFunc* func) {
  ObjUpval** upvals = ALLOCATE(
    ObjUpval*,
    func->upvalCount
  );
  for (int i = 0; i < func->upvalCount; i++) {
    upvals[i] = NULL;
  }
  ObjClosure* closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
  closure->func = func;
  closure->upvals = upvals;
  closure->upvalCount = func->upvalCount;
  return closure;
}

ObjFunc* newFunc() {
  ObjFunc* func = ALLOCATE_OBJ(ObjFunc, OBJ_FUNC);
  func->arity = 0;
  func->upvalCount = 0;
  func->name = NULL;
  initChunk(&func->chunk);
  return func;
}

ObjInstance* newInstance(ObjClass* class) {
  ObjInstance* instance = ALLOCATE_OBJ(ObjInstance, OBJ_INSTANCE);
  instance->class = class;
  initTable(&instance->fields);
  return instance;
}

ObjNative* newNative(NativeFn func) {
  ObjNative* native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
  native->func = func;
  return native;
}

static ObjStr* allocStr(char* chars, int length, uint32_t hash) {
  ObjStr* string = ALLOCATE_OBJ(ObjStr, OBJ_STR);
  string->length = length;
  string->chars = chars;
  string->hash = hash;
  push(OBJ_VAL(string));
  tableSet(&vm.strings, string, NIL_VAL);
  pop();
  return string;
}

static uint32_t hashStr(const char* key, int length) {
  uint32_t hash = 2166136261u;
  for (int i = 0; i < length; i++) {
    hash ^= (uint8_t)key[i];
    hash *= 16777619;
  }
  return hash;
}

ObjStr* takeStr(char* chars, int length) {
  uint32_t hash = hashStr(chars, length);
  ObjStr* interned = tableFindStr(
    &vm.strings, chars,
    length, hash
  );
  if (interned != NULL) {
    FREE_ARRAY(char, chars, length + 1);
    return interned;
  }
  return allocStr(chars, length, hash);
}

ObjStr* copyStr(const char* chars, int length) {
  uint32_t hash = hashStr(chars, length);
  ObjStr* interned = tableFindStr(
    &vm.strings, chars,
    length, hash
  );
  if (interned != NULL) {
    return interned;
  }
  char* heapChars = ALLOCATE(char, length + 1);
  memcpy(heapChars, chars, length);
  heapChars[length] = '\0';
  return allocStr(heapChars, length, hash);
}

ObjUpval* newUpval(Value* slot) {
  ObjUpval* upval = ALLOCATE_OBJ(ObjUpval, OBJ_UPVAL);
  upval->closed = NIL_VAL;
  upval->location = slot;
  upval->next = NULL;
  return upval;
}

ObjList* newList() {
  ObjList* list = ALLOCATE_OBJ(ObjList, OBJ_LIST);
  list->items = NULL;
  list->count = 0;
  list->capacity = 0;
  return list;
}

void appendToList(ObjList* list, Value value) {
  if (list->capacity < list->count + 1) {
    int oldCapacity = list->capacity;
    list->capacity = GROW_CAPACITY(oldCapacity);
    list->items = GROW_ARRAY(
      Value, list->items,
      oldCapacity, list->capacity
    );
  }
  list->items[list->count] = value;
  list->count++;
  return;
}

void storeToList(ObjList* list, int index, Value value) {
  list->items[index] = value;
}

Value indexFromList(ObjList* list, int index) {
  return list->items[index];
}

void deleteFromList(ObjList* list, int index) {
  for (int i = 0; i < list->count - 1; i++) {
    list->items[i] = list->items[i + 1];
  }
  list->items[list->count - 1] = NIL_VAL;
  list->count--;
}

bool isValidListIndex(ObjList* list, int index) {
  if (index < 0 || index > list->count - 1) {
    return false;
  }
  return true;
}

static void printFunc(ObjFunc* func) {
  if (func->name == NULL) {
    printf("<module>");
    return;
  }
  printf("<fn %s>", func->name->chars);
}

void printObj(Value value) {
  switch (OBJ_TYPE(value)) {
    case OBJ_LIST:
      // Add later.
      break;
    case OBJ_BOUND_METHOD:
      printFunc(AS_BOUND_METHOD(value)->method->func);
      break;
    case OBJ_CLASS:
      printf("%s", AS_CLASS(value)->name->chars);
      break;
    case OBJ_CLOSURE:
      printFunc(AS_CLOSURE(value)->func);
      break;
    case OBJ_FUNC:
      printFunc(AS_FUNC(value));
      break;
    case OBJ_INSTANCE:
      printf(
        "%s instance",
        AS_INSTANCE(value)->class->name->chars
      );
      break;
    case OBJ_NATIVE:
      printf("<native fn>");
      break;
    case OBJ_STR:
      printf("%s", AS_CSTR(value));
      break;
    case OBJ_UPVAL:
      printf("upval");
      break;
  }
}