#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include "include/common.h"
#include "include/vm.h"
#include "include/compiler.h"
#include "include/debug.h"
#include "include/memory.h"
#include "include/object.h"
#include "include/memory.h"

VM vm;

// Native helpers

static void printList(ObjList* list) {
  printf("[");
  for (int i = 0; i < list->count; i++) {
    if (i == list->count - 1) {
      if (IS_STR(list->items[i])) {
        printf("\"");
        printValue(list->items[i]);
        printf("\"");
      }
      else if (IS_LIST(list->items[i])) {
        printList(AS_LIST(list->items[i]));
      }
      else {
        printValue(list->items[i]);
      }
    }
    else {
      if (IS_STR(list->items[i])) {
        printf("\"");
        printValue(list->items[i]);
        printf("\", ");
      }
      else if (IS_LIST(list->items[i])) {
        printList(AS_LIST(list->items[i]));
        printf(", ");
      }
      else {
        printValue(list->items[i]);
        printf(", ");
      }
    }
  }
  printf("]");
}

// End native helpers

// Natives

static Value printNative(int argCount, Value* args) {
  if (argCount <= 0) {
    // Add later.
    return NIL_VAL;
  }
  if (IS_BOOL(args[0])) {
    printf("%s", AS_BOOL(args[0]) ? "true" : "false");
  }
  else if (IS_NUM(args[0])) {
    printf("%.16g", AS_NUM(args[0]));
  }
  else if (IS_NIL(args[0])) {
    printf("nil");
  }
  else if (IS_OBJ(args[0])) {
    switch (OBJ_TYPE(args[0])) {
      case OBJ_STR: printf("%s", AS_STR(args[0])->chars); break;
      case OBJ_LIST: {
        printList(AS_LIST(args[0]));
        break;
      }
      default: return NIL_VAL;
    }
  }
  return NIL_VAL;
}

static Value printlnNative(int argCount, Value* args) {
  if (argCount <= 0) {
    // Add later.
    return NIL_VAL;
  }
  if (IS_BOOL(args[0])) {
    printf("%s\n", AS_BOOL(args[0]) ? "true" : "false");
  }
  else if (IS_NUM(args[0])) {
    printf("%.16g\n", AS_NUM(args[0]));
  }
  else if (IS_NIL(args[0])) {
    printf("nil\n");
  }
  else if (IS_OBJ(args[0])) {
    switch (OBJ_TYPE(args[0])) {
      case OBJ_STR: printf("%s\n", AS_STR(args[0])->chars); break;
      case OBJ_LIST: {
        printList(AS_LIST(args[0]));
        printf("\n");
        break;
      }
      default: return NIL_VAL;
    }
  }
  return NIL_VAL;
}

static Value readStrNative(int argCount, Value* args) {
  if (argCount > 0) {
    // Do later.
  }
  char input[255]; // Big, yes.
  scanf("%s", input);
  return OBJ_VAL(copyStr(input, strlen(input)));
}

static Value readNumNative(int argCount, Value* args) {
  if (argCount > 0) {
    // Do later.
  }
  double input;
  scanf("%lf", &input);
  return NUM_VAL(input);
}

static Value appendNative(int argCount, Value* args) {
  if (argCount != 2 || !IS_LIST(args[0])) {
    // Add later.
  }
  ObjList* list = AS_LIST(args[0]);
  Value item = args[1];
  appendToList(list, item);
  return NIL_VAL;
}

static Value delNative(int argCount, Value* args) {
  if (argCount != 0 || !IS_LIST(args[0])) {
    // Add later.
  }
  ObjList* list = AS_LIST(args[0]);
  int index = AS_NUM(args[1]);
  if (!isValidListIndex(list, index)) {
    // Add later.
  }
  deleteFromList(list, index);
  return NIL_VAL;
}

// End natives

static void resetStack() {
  vm.stackTop = vm.stack;
  vm.frameCount = 0;
  vm.openUpvals = NULL;
}

static void runtimeErr(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);
  for (int i = vm.frameCount - 1; i >= 0; i--) {
    CallFrame* frame = &vm.frames[i];
    ObjFunc* func = frame->closure->func;
    size_t instruction = frame->ip - func->chunk.code - 1;
    fprintf(
      stderr, "\n[Line %d] in ",
      getLine(&func->chunk, instruction)
    );
    if (func->name == NULL) {
      fprintf(stderr, "<module>\n");
    }
    else {
      fprintf(stderr, "function '%s'\n", func->name->chars);
    }
  }
  resetStack();
}

static void defNative(const char* name, NativeFn func) {
  push(OBJ_VAL(copyStr(name, (int)strlen(name))));
  push(OBJ_VAL(newNative(func)));
  tableSet(&vm.globals, AS_STR(vm.stack[0]), vm.stack[1]);
  pop();
  pop();
}

void initVM() {
  resetStack();
  vm.objects = NULL;
  vm.allocatedBytes = 0;
  vm.nextGC = 1024 * 1024;
  vm.grayCount = 0;
  vm.grayCapacity = 0;
  vm.grayStack = NULL;
  initTable(&vm.globals);
  initTable(&vm.strings);
  vm.initString = NULL;
  vm.initString = copyStr("init", 4);
  // defNative("type", typeNative);
  defNative("append", appendNative);
  defNative("del", delNative);
  defNative("print", printNative);
  defNative("println", printlnNative);
  defNative("readStr", readStrNative);
  defNative("readNum", readNumNative);
}

void freeVM() {
  freeTable(&vm.globals);
  freeTable(&vm.strings);
  vm.initString = NULL;
  freeObjs();
}

void push(Value value) {
  *vm.stackTop = value;
  vm.stackTop++;
}

Value pop() {
  vm.stackTop--;
  return *vm.stackTop;
}

static Value peek(int distance) {
  return vm.stackTop[-1 - distance];
}

static bool call(ObjClosure* closure, int argCount) {
  if (
    argCount != closure->func->arity &&
    closure->func->arity == 1
  ) {
    runtimeErr(
      "Function '%s' expected %d argument but got %d instead.",
      closure->func->name->chars, closure->func->arity, argCount
    );
    return false;
  }
  else if (argCount != closure->func->arity) {
    runtimeErr(
      "Function '%s' expected %d arguments but got %d instead.",
      closure->func->name->chars, closure->func->arity, argCount
    );
    return false;
  }
  if (vm.frameCount == FRAMES_MAX) {
    runtimeErr("Stack overflow.");
    return false;
  }
  CallFrame* frame = &vm.frames[vm.frameCount++];
  frame->closure = closure;
  frame->ip = closure->func->chunk.code;
  frame->slots = vm.stackTop - argCount - 1;
  return true;
}

static bool callVal(Value callee, int argCount) {
  if (IS_OBJ(callee)) {
    switch (OBJ_TYPE(callee)) {
      case OBJ_BOUND_METHOD: {
        ObjBoundMethod* bound = AS_BOUND_METHOD(callee);
        vm.stackTop[-argCount - 1] = bound->receiver;
        return call(bound->method, argCount);
      }
      case OBJ_CLASS: {
        ObjClass* class = AS_CLASS(callee);
        vm.stackTop[-argCount - 1] = OBJ_VAL(newInstance(class));
        Value initializer;
        if (tableGet(
          &class->methods,
          vm.initString,
          &initializer
        )) {
          return call(AS_CLOSURE(initializer), argCount);
        }
        else if (argCount != 0) {
          runtimeErr(
            "Class '%s' expected 0 arguments but got %d instead.",
            class->name->chars, argCount
          );
          return false;
        }
        return true;
      }
      case OBJ_CLOSURE:
        return call(AS_CLOSURE(callee), argCount);
      case OBJ_NATIVE: {
        NativeFn native = AS_NATIVE(callee);
        Value result = native(argCount, vm.stackTop - argCount);
        vm.stackTop -= argCount + 1;
        push(result);
        return true;
      }
      default:
        break;
    }
  }
  runtimeErr("Only functions and classes are callable.");
  return false;
}

static bool invokeFromClass(
  ObjClass* class,
  ObjStr* name,
  int argCount
) {
  Value method;
  if (!tableGet(&class->methods, name, &method)) {
    runtimeErr("Property '%s' is undefined.", name->chars);
    return false;
  }
  return call(AS_CLOSURE(method), argCount);
}

static bool invoke(ObjStr* name, int argCount) {
  Value receiver = peek(argCount);
  if (!IS_INSTANCE(receiver)) {
    runtimeErr("Only instances can have methods.");
    return false;
  }
  ObjInstance* instance = AS_INSTANCE(receiver);
  Value value;
  if (tableGet(&instance->fields, name, &value)) {
    vm.stackTop[-argCount - 1] = value;
    return callVal(value, argCount);
  }
  return invokeFromClass(instance->class, name, argCount);
}

static bool bindMethod(ObjClass* class, ObjStr* name) {
  Value method;
  if (!tableGet(&class->methods, name, &method)) {
    runtimeErr("Property '%s' is undefined.", name->chars);
    return false;
  }
  ObjBoundMethod* bound = newBoundMethod(
    peek(0), AS_CLOSURE(method)
  );
  pop();
  push(OBJ_VAL(bound));
  return true;
}

static ObjUpval* captureUpval(Value* local) {
  ObjUpval* prevUpval = NULL;
  ObjUpval* upval = vm.openUpvals;
  while (upval != NULL && upval->location > local) {
    prevUpval = upval;
    upval = upval->next;
  }
  if (upval != NULL && upval->location == local) {
    return upval;
  }
  ObjUpval* createdUpval = newUpval(local);
  createdUpval->next = upval;
  if (prevUpval == NULL) {
    vm.openUpvals = createdUpval;
  }
  else {
    prevUpval->next = createdUpval;
  }
  return createdUpval;
}

static void closeUpvals(Value* last) {
  while (
    vm.openUpvals != NULL &&
    vm.openUpvals->location >= last
  ) {
    ObjUpval* upval = vm.openUpvals;
    upval->closed = *upval->location;
    upval->location = &upval->closed;
    vm.openUpvals = upval->next;
  }
}

static void defMethod(ObjStr* name) {
  Value method = peek(0);
  ObjClass* class = AS_CLASS(peek(1));
  tableSet(&class->methods, name, method);
  pop();
}

static bool falsey(Value value) {
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static ObjStr* toStr(Value value) {
  ObjStr* str;
  if (IS_BOOL(value)) {
    str =
      AS_BOOL(value) ? copyStr("true", 4) : copyStr("false", 5);
  }
  else if (IS_NUM(value)) {
    char nstr[24];
    double num = AS_NUM(value);
    sprintf(nstr, "%.16g", num);
    str =
      copyStr(nstr, strlen(nstr));
  }
  else if (IS_NIL(value)) {
    str = copyStr("nil", 3);
  }
  else if (IS_STR(value)) {
    str = AS_STR(value);
  }
  else {
    runtimeErr("Invalid concatenation type.");
    str = copyStr("nil", 3);
  }
  return str;
}

static void concat() {
  ObjStr* b = toStr(peek(0));
  ObjStr* a = toStr(peek(1));
  int length = a->length + b->length;
  char* chars = ALLOCATE(char, length + 1);
  memcpy(chars, a->chars, a->length);
  memcpy(chars + a->length, b->chars, b->length);
  chars[length] = '\0';
  ObjStr* result = takeStr(chars, length);
  pop();
  pop();
  push(OBJ_VAL(result));
}

static InterpretResult run() {
  CallFrame* frame = &vm.frames[vm.frameCount - 1];
  register uint8_t* ip = frame->ip;
  #define READ_BYTE() (*ip++)
  #define READ_CONST() \
    (frame->closure->func->chunk.constants.values[READ_BYTE()])
  #define READ_SHORT() \
    (ip += 2, \
    (uint16_t)((ip[-2] << 8) | ip[-1]))
  #define READ_STR() AS_STR(READ_CONST())
  #define BINARY_OP(valType, op) \
    do { \
      if (!IS_NUM(peek(0)) || !IS_NUM(peek(1))) { \
        frame->ip = ip; \
        runtimeErr("Operands must be numbers."); \
        return INTERPRET_RUNTIME_ERROR; \
      } \
      double b = AS_NUM(pop()); \
      double a = AS_NUM(pop()); \
      push(valType(a op b)); \
    } while (false)
  
  for (;;) {
    #ifdef DEBUG_TRACE_EXEC
    printf("\t\t");
    for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
      printf("[ ");
      printValue(*slot);
      printf(" ]");
    }
    printf("\n");
    disassembleInstruction(
      &frame->closure->func->chunk,
      (int)(frame->ip - frame->closure->func->chunk.code)
      );
    #endif
    uint8_t instruction;
    switch (instruction = READ_BYTE()) {
      case OP_CONST: {
        Value constant = READ_CONST();
        push(constant);
        break;
      }
      case OP_NIL: push(NIL_VAL); break;
      case OP_TRUE: push(BOOL_VAL(true)); break;
      case OP_FALSE: push(BOOL_VAL(false)); break;
      case OP_DUP: push(peek(0)); break;
      case OP_POP: pop(); break;
      case OP_GET_LOCAL: {
        uint8_t slot = READ_BYTE();
        push(frame->slots[slot]);
        break;
      }
      case OP_SET_LOCAL: {
        uint8_t slot = READ_BYTE();
        frame->slots[slot] = peek(0);
        break;
      }
      case OP_GET_GLOBAL: {
        ObjStr* name = READ_STR();
        Value value;
        if (!tableGet(&vm.globals, name, &value)) {
          frame->ip = ip;
          runtimeErr("Variable '%s' is undefined.", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }
        push(value);
        break;
      }
      case OP_DEF_GLOBAL: {
        ObjStr* name = READ_STR();
        tableSet(&vm.globals, name, peek(0));
        pop();
        break;
      }
      case OP_SET_GLOBAL: {
        ObjStr* name = READ_STR();
        if (tableSet(&vm.globals, name, peek(0))) {
          tableDel(&vm.globals, name);
          frame->ip = ip;
          runtimeErr("'%s' is undefined.", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
      case OP_GET_UPVAL: {
        uint8_t slot = READ_BYTE();
        push(*frame->closure->upvals[slot]->location);
        break;
      }
      case OP_SET_UPVAL: {
        uint8_t slot = READ_BYTE();
        *frame->closure->upvals[slot]->location = peek(0);
        break;
      }
      case OP_GET_PROP: {
        if (!IS_INSTANCE(peek(0))) {
          runtimeErr("Only instances can have properties.");
          return INTERPRET_RUNTIME_ERROR;
        }
        ObjInstance* instance = AS_INSTANCE(peek(0));
        ObjStr* name = READ_STR();
        Value value;
        if (tableGet(&instance->fields, name, &value)) {
          pop();
          push(value);
          break;
        }
        if (!bindMethod(instance->class, name)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
      case OP_SET_PROP: {
        if (!IS_INSTANCE(peek(1)) && !IS_CLASS(peek(1))) {
          runtimeErr("Only instances and classes can have fields.");
          return INTERPRET_RUNTIME_ERROR;
        }
        ObjInstance* instance = AS_INSTANCE(peek(1));
        tableSet(&instance->fields, READ_STR(), peek(0));
        Value value = pop();
        pop();
        push(value);
        break;
      }
      case OP_GET_SUPER: {
        ObjStr* name = READ_STR();
        ObjClass* superclass = AS_CLASS(pop());
        if (!bindMethod(superclass, name)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
      case OP_BUILD_LIST: {
        ObjList* list = newList();
        uint8_t itemCount = READ_BYTE();
        push(OBJ_VAL(list));
        for (int i = itemCount; i > 0; i--) {
          appendToList(list, peek(i));
        }
        pop();
        while (itemCount-- > 0) {
          pop();
        }
        push(OBJ_VAL(list));
        break;
      }
      case OP_INDEX_SUB: {
        Value index = pop();
        Value list = pop();
        Value result;

        if (!IS_LIST(list)) {
          runtimeErr("Invalid type to index.");
          return INTERPRET_RUNTIME_ERROR;
        }
        ObjList* olist = AS_LIST(list);
        if (!IS_NUM(index)) {
          runtimeErr("List index must be a number.");
          return INTERPRET_RUNTIME_ERROR;
        }
        int oindex = AS_NUM(index);
        if (!isValidListIndex(olist, oindex)) {
          runtimeErr("List index is out of range.");
          return INTERPRET_RUNTIME_ERROR;
        }
        result = indexFromList(olist, AS_NUM(index));
        push(result);
        break;
      }
      case OP_STORE_SUB: {
        Value item = pop();
        Value index = pop();
        Value list = pop();

        if (!IS_LIST(list)) {
          runtimeErr(
            "Cannot store value in something other than a list."
          );
          return INTERPRET_RUNTIME_ERROR;
        }
        ObjList* olist = AS_LIST(list);
        if (!IS_NUM(index)) {
          runtimeErr("List index must be a number.");
          return INTERPRET_RUNTIME_ERROR;
        }
        int oindex = AS_NUM(index);
        if (!isValidListIndex(olist, oindex)) {
          runtimeErr("Invalid list index.");
          return INTERPRET_RUNTIME_ERROR;
        }
        storeToList(olist, oindex, item);
        push(item);
        break;
      }
      case OP_EQU: {
        Value b = pop();
        Value a = pop();
        push(BOOL_VAL(valsEqu(a, b)));
        break;
      }
      case OP_GT: {
        Value b = pop();
        Value a = pop();
        push(BOOL_VAL(valsGt(a, b)));
        break;
      }
      case OP_LT: {
        Value b = pop();
        Value a = pop();
        push(BOOL_VAL(valsLt(a, b)));
        break;
      }
      case OP_GT_EQU: {
        Value b = pop();
        Value a = pop();
        push(BOOL_VAL(valsGtEqu(a, b)));
        break;
      }
      case OP_LT_EQU: {
        Value b = pop();
        Value a = pop();
        push(BOOL_VAL(valsLtEqu(a, b)));
        break;
      }
      case OP_NOT_EQU: {
        Value b = pop();
        Value a = pop();
        push(BOOL_VAL(valsNotEqu(a, b)));
        break;
      }
      case OP_ADD: {
        if (IS_STR(peek(0)) || IS_STR(peek(1))) {
          concat();
        }
        else if (IS_NUM(peek(0)) && IS_NUM(peek(1))) {
          double b = AS_NUM(pop());
          double a = AS_NUM(pop());
          push(NUM_VAL(a + b));
        }
        else {
          frame->ip = ip;
          runtimeErr(
            "Invalid types for operator."
          );
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
      case OP_SUB: BINARY_OP(NUM_VAL, -); break;
      case OP_MUL: BINARY_OP(NUM_VAL, *); break;
      case OP_POW: {
        if (IS_NUM(peek(0)) && IS_NUM(peek(1))) {
          double b = AS_NUM(pop());
          double a = AS_NUM(pop());
          push(NUM_VAL(pow(a, b)));
        }
        else {
          runtimeErr("Operands must be numbers.");
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
      case OP_DIV: {
        if (IS_NUM(peek(0)) && IS_NUM(peek(1))) {
          double b = AS_NUM(pop());
          double a = AS_NUM(pop());
          if (b == 0) {
            runtimeErr("Division by zero.");
            return INTERPRET_RUNTIME_ERROR;
          }
          push(NUM_VAL(a / b));
        }
        else {
          runtimeErr("Operands must be numbers.");
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
      case OP_MOD: {
        if (IS_NUM(peek(0)) && IS_NUM(peek(1))) {
          double b = AS_NUM(pop());
          double a = AS_NUM(pop());
          push(NUM_VAL((int)a % (int)b));
        }
        else {
          runtimeErr("Operands must be numbers.");
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
      case OP_NOT:
        push(BOOL_VAL(falsey(pop())));
        break;
      case OP_NEGATE:
        if (!IS_NUM(peek(0))) {
          frame->ip = ip;
          runtimeErr("Operand must be a number.");
          return INTERPRET_RUNTIME_ERROR;
        }
        push(NUM_VAL(-AS_NUM(pop())));
        break;
      case OP_JMP: {
        uint16_t offset = READ_SHORT();
        ip += offset;
        break;
      }
      case OP_JMPF: {
        uint16_t offset = READ_SHORT();
        if (falsey(peek(0))) {
          ip += offset;
        }
        break;
      }
      case OP_LOOP: {
        uint16_t offset = READ_SHORT();
        ip -= offset;
        break;
      }
      case OP_CALL: {
        int argCount = READ_BYTE();
        frame->ip = ip;
        if (!callVal(peek(argCount), argCount)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        frame = &vm.frames[vm.frameCount - 1];
        ip = frame->ip;
        break;
      }
      case OP_INVOKE: {
        ObjStr* method = READ_STR();
        int argCount = READ_BYTE();
        if (!invoke(method, argCount)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        frame = &vm.frames[vm.frameCount - 1];
        break;
      }
      case OP_INVOKE_SUPER: {
        ObjStr* method = READ_STR();
        int argCount = READ_BYTE();
        ObjClass* superclass = AS_CLASS(pop());
        if (!invokeFromClass(superclass, method, argCount)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        frame = &vm.frames[vm.frameCount - 1];
        break;
      }
      case OP_CLOSURE: {
        ObjFunc* func = AS_FUNC(READ_CONST());
        ObjClosure* closure = newClosure(func);
        push(OBJ_VAL(closure));
        for (int i = 0; i < closure->upvalCount; i++) {
          uint8_t isLocal = READ_BYTE();
          uint8_t index = READ_BYTE();
          if (isLocal) {
            closure->upvals[i] =
              captureUpval(frame->slots + index);
          }
          else {
            closure->upvals[i] = frame->closure->upvals[index];
          }
        }
        break;
      }
      case OP_CLOSE_UPVAL:
        closeUpvals(vm.stackTop - 1);
        pop();
        break;
      case OP_RETURN: {
        Value result = pop();
        closeUpvals(frame->slots);
        vm.frameCount--;
        if (vm.frameCount == 0) {
          pop();
          return INTERPRET_OK;
        }
        vm.stackTop = frame->slots;
        push(result);
        frame = &vm.frames[vm.frameCount - 1];
        ip = frame->ip;
        break;
      }
      case OP_CLASS:
        push(OBJ_VAL(newClass(READ_STR())));
        break;
      case OP_INHERIT: {
        Value superclass = peek(1);
        if (!IS_CLASS(superclass)) {
          runtimeErr("Superclass must be a class.");
          return INTERPRET_RUNTIME_ERROR;
        }
        ObjClass* subclass = AS_CLASS(peek(0));
        tableAddAll(
          &AS_CLASS(superclass)->methods,
          &subclass->methods
        );
        pop();
        break;
      }
      case OP_METHOD:
        defMethod(READ_STR());
        break;
    }
  }
  #undef READ_BYTE
  #undef READ_SHORT
  #undef READ_CONST
  #undef READ_STR
  #undef BINARY_OP
}

InterpretResult interpret(const char* source) {
  ObjFunc* func = compile(source);
  if (func == NULL) {
    return INTERPRET_COMPILE_ERROR;
  }
  push(OBJ_VAL(func));
  ObjClosure* closure = newClosure(func);
  pop();
  push(OBJ_VAL(closure));
  call(closure, 0);
  return run();
}