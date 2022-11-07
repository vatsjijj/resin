#include <stdio.h>
#include <stdlib.h>
#include "include/memory.h"
#include "include/vm.h"
#include "include/compiler.h"

#ifdef DEBUG_LOG_GC
#include "include/debug.h"
#endif

#define GC_HEAP_GROW_FACTOR 2

void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
  vm.allocatedBytes += newSize - oldSize;
  if (newSize > oldSize) {
    #ifdef DEBUG_STRESS_GC
    garbageCollect();
    #endif
    if (vm.allocatedBytes > vm.nextGC) {
      garbageCollect();
    }
  }
  if (newSize == 0) {
    free(pointer);
    return NULL;
  }
  void* result = realloc(pointer, newSize);
  if (result == NULL) {
    if (system("/usr/bin/fortune 2> /dev/null") != 0) {
      printf("Oh, the horror!\n");
    }
    printf("\nCould not allocate memory.\n");
    exit(1);
  }
  return result;
}

void markObj(Obj* object) {
  if (object == NULL) {
    return;
  }
  if (object->isMarked) {
    return;
  }
  #ifdef DEBUG_LOG_GC
  printf("[%p] mark ", (void*)object);
  printValue(OBJ_VAL(object));
  printf("\n");
  #endif
  object->isMarked = true;
  if (vm.grayCapacity < vm.grayCount + 1) {
    vm.grayCapacity = GROW_CAPACITY(vm.grayCapacity);
    vm.grayStack = (Obj**)realloc(
      vm.grayStack,
      sizeof(Obj*) * vm.grayCapacity
    );
    if (vm.grayStack == NULL) {
      if (system("/usr/bin/fortune 2> /dev/null") != 0) {
        printf("There are many beasts within your wake.\n");
      }
      printf(
        "\nFailure to allocate or create gray stack.\n"
      );
      exit(1);
    }
  }
  vm.grayStack[vm.grayCount++] = object;
}

void markVal(Value value) {
  if (IS_OBJ(value)) {
    markObj(AS_OBJ(value));
  }
}

static void markArray(ValueArray* array) {
  for (int i = 0; i < array->count; i++) {
    markVal(array->values[i]);
  }
}

static void blackenObj(Obj* object) {
  #ifdef DEBUG_LOG_GC
  printf("[%p] blacken ", (void*)object);
  printValue(OBJ_VAL(object));
  printf("\n");
  #endif
  switch (object->type) {
    case OBJ_LIST: {
      ObjList* list = (ObjList*)object;
      for (int i = 0; i < list->count; i++) {
        markVal(list->items[i]);
      }
      break;
    }
    case OBJ_BOUND_METHOD: {
      ObjBoundMethod* bound = (ObjBoundMethod*)object;
      markVal(bound->receiver);
      markObj((Obj*)bound->method);
      break;
    }
    case OBJ_CLASS: {
      ObjClass* class = (ObjClass*)object;
      markObj((Obj*)class->name);
      markTable(&class->methods);
      break;
    }
    case OBJ_CLOSURE: {
      ObjClosure* closure = (ObjClosure*)object;
      markObj((Obj*)closure->func);
      for (int i = 0; i < closure->upvalCount; i++) {
        markObj((Obj*)closure->upvals[i]);
      }
      break;
    }
    case OBJ_FUNC: {
      ObjFunc* func = (ObjFunc*)object;
      markObj((Obj*)func->name);
      markArray(&func->chunk.constants);
      break;
    }
    case OBJ_INSTANCE: {
      ObjInstance* instance = (ObjInstance*)object;
      markObj((Obj*)instance->class);
      markTable(&instance->fields);
      break;
    }
    case OBJ_UPVAL:
      markVal(((ObjUpval*)object)->closed);
      break;
    case OBJ_NATIVE:
    case OBJ_STR:
      break;
  }
}

static void freeObj(Obj* object) {
  #ifdef DEBUG_LOG_GC
  printf("[%p] free type %d\n", (void*)object, object->type);
  #endif
  switch (object->type) {
    case OBJ_LIST: {
      ObjList* list = (ObjList*)object;
      FREE_ARRAY(Value*, list->items, list->count);
      FREE(ObjList, object);
      break;
    }
    case OBJ_BOUND_METHOD:
      FREE(ObjBoundMethod, object);
      break;
    case OBJ_CLASS: {
      ObjClass* class = (ObjClass*)object;
      freeTable(&class->methods);
      FREE(ObjClass, object);
      break;
    }
    case OBJ_CLOSURE: {
      ObjClosure* closure = (ObjClosure*)object;
      FREE_ARRAY(
        ObjUpval*,
        closure->upvals,
        closure->upvalCount
      );
      FREE(ObjClosure, object);
      break;
    }
    case OBJ_FUNC: {
      ObjFunc* func = (ObjFunc*)object;
      freeChunk(&func->chunk);
      FREE(ObjFunc, object);
      break;
    }
    case OBJ_INSTANCE: {
      ObjInstance* instance = (ObjInstance*)object;
      freeTable(&instance->fields);
      FREE(ObjInstance, object);
      break;
    }
    case OBJ_NATIVE:
      FREE(ObjNative, object);
      break;
    case OBJ_STR: {
      ObjStr* string = (ObjStr*)object;
      FREE_ARRAY(char, string->chars, string->length + 1);
      FREE(ObjStr, object);
      break;
    }
    case OBJ_UPVAL:
      FREE(ObjUpval, object);
      break;
  }
}

static void markRoots() {
  for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
    markVal(*slot);
  }
  for (int i = 0; i < vm.frameCount; i++) {
    markObj((Obj*)vm.frames[i].closure);
  }
  for (
    ObjUpval* upval = vm.openUpvals;
    upval != NULL;
    upval = upval->next
  ) {
    markObj((Obj*)upval);
  }
  markTable(&vm.globals);
  markCompilerRoots();
  markObj((Obj*)vm.initString);
}

static void traceRefs() {
  while (vm.grayCount > 0) {
    Obj* object = vm.grayStack[--vm.grayCount];
    blackenObj(object);
  }
}

// Sweep 'em away!
static void sweep() {
  Obj* previous = NULL;
  Obj* object = vm.objects;
  while (object != NULL) {
    if (object->isMarked) {
      object->isMarked = false;
      previous = object;
      object = object->next;
    }
    else {
      Obj* unreached = object;
      object = object->next;
      if (previous != NULL) {
        previous->next = object;
      }
      else {
        vm.objects = object;
      }
      freeObj(unreached);
    }
  }
}

void garbageCollect() {
  #ifdef DEBUG_LOG_GC
  printf("== begin gc ==\n");
  size_t before = vm.allocatedBytes;
  #endif
  markRoots();
  traceRefs();
  tableRemoveWhite(&vm.strings);
  sweep();
  vm.nextGC = vm.allocatedBytes * GC_HEAP_GROW_FACTOR;
  #ifdef DEBUG_LOG_GC
  printf("== end gc ==\n");
  printf(
    "\tcollected %zu bytes (from %zu to %zu) next at %zu\n",
    before - vm.allocatedBytes, before,
    vm.allocatedBytes, vm.nextGC
  );
  #endif
}

void freeObjs() {
  Obj* object = vm.objects;
  while (object != NULL) {
    Obj* next = object->next;
    freeObj(object);
    object = next;
  }
  free(vm.grayStack);
}