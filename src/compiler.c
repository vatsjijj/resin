#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/common.h"
#include "include/compiler.h"
#include "include/scanner.h"
#include "include/object.h"
#include "include/memory.h"
#ifdef DEBUG_PRINT_CODE
#include "include/debug.h"
#endif

#define MAX_CASES 256

typedef struct {
  Token current;
  Token previous;
  bool err;
  bool panic;
} Parser;

typedef enum {
  PREC_NONE,
  PREC_ASSIGN,
  PREC_OR, PREC_AND,
  PREC_EQU,
  PREC_COMP,
  PREC_TERM,
  PREC_FACTOR,
  PREC_UNARY,
  PREC_SUB,
  PREC_CALL,
  PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(bool canAssign);

typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;

typedef struct {
  Token name;
  int depth;
  bool isCaptured;
} Local;

typedef struct {
  uint8_t index;
  bool isLocal;
} Upvalue;

typedef enum {
  TYPE_FUNC,
  TYPE_INITIALIZER,
  TYPE_METHOD,
  TYPE_SCRIPT // No, this isn't TypeScript.
} FuncType;

typedef struct Compiler {
  struct Compiler* enclosing;
  ObjFunc* func;
  FuncType type;
  Local locals[UINT8_COUNT];
  int localCount;
  Upvalue upvals[UINT8_COUNT];
  int scopeDepth;
} Compiler;

typedef struct ClassCompiler {
  struct ClassCompiler* enclosing;
  bool hasSuperclass;
} ClassCompiler;

Parser parser;
Compiler* current = NULL;
ClassCompiler* currentClass = NULL;

static Chunk* currentChunk() {
  return &current->func->chunk;
}

static void errAt(Token* token, const char* message) {
  if (parser.panic) {
    return;
  }
  parser.panic = true;
  fprintf(stderr, "Line %d: Error", token->line);
  if (token->type == TEOF) {
    fprintf(stderr, " at end");
  }
  else if (token->type == ERR) {
    // Do nothing.
  }
  else {
    fprintf(stderr, " at '%.*s'", token->length, token->start);
  }
  fprintf(stderr, ": %s\n", message);
  parser.err = true;
}

static void err(const char* message) {
  errAt(&parser.previous, message);
}

static void errAtCurrent(const char* message) {
  errAt(&parser.current, message);
}

static void advance() {
  parser.previous = parser.current;

  for (;;) {
    parser.current = scanToken();
    if (parser.current.type != ERR) {
      break;
    }
    errAtCurrent(parser.current.start);
  }
}

static void consume(TokenType type, const char* message) {
  if (parser.current.type == type) {
    advance();
    return;
  }
  errAtCurrent(message);
}

static bool check(TokenType type) {
  return parser.current.type == type;
}

static bool match(TokenType type) {
  if (!check(type)) {
    return false;
  }
  advance();
  return true;
}

static void emitByte(uint8_t byte) {
  writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {
  emitByte(byte1);
  emitByte(byte2);
}

static void emitLoop(int loopStart) {
  emitByte(OP_LOOP);
  int offset = currentChunk()->count - loopStart + 2;
  if (offset > UINT16_MAX) {
    err("Loop body is too large.");
  }
  emitByte((offset >> 8) & 0xff);
  emitByte(offset & 0xff);
}

static int emitJmp(uint8_t instruction) {
  emitByte(instruction);
  emitByte(0xff);
  emitByte(0xff);
  return currentChunk()->count - 2;
}

static void emitReturn() {
  if (current->type == TYPE_INITIALIZER) {
    emitBytes(OP_GET_LOCAL, 0);
  }
  else {
    emitByte(OP_NIL);
  }
  emitByte(OP_RETURN);
}

static uint8_t makeConst(Value value) {
  int constant = addConst(currentChunk(), value);
  if (constant > UINT8_MAX) {
    err("Too many constants in one chunk.");
    return 0;
  }
  return (uint8_t)constant;
}

static void emitConst(Value value) {
  emitBytes(OP_CONST, makeConst(value));
}

static void patchJmp(int offset) {
  // -2 for adjust.
  int jump = currentChunk()->count - offset - 2;
  if (jump > UINT16_MAX) {
    err("Jump too long.");
  }
  currentChunk()->code[offset] = (jump >> 8) & 0xff;
  currentChunk()->code[offset + 1] = jump & 0xff;
}

static void initCompiler(Compiler* compiler, FuncType type) {
  compiler->enclosing = current;
  compiler->func = NULL;
  compiler->type = type;
  compiler->localCount = 0;
  compiler->scopeDepth = 0;
  compiler->func = newFunc();
  current = compiler;
  if (type != TYPE_SCRIPT) {
    current->func->name = copyStr(
        parser.previous.start,
        parser.previous.length
      );
  }
  Local* local = &current->locals[current->localCount++];
  local->depth = 0;
  local->isCaptured = false;
  if (type != TYPE_FUNC) {
    local->name.start = "this";
    local->name.length = 4;
  }
  else {
    local->name.start = "";
    local->name.length = 0;
  }
}

static ObjFunc* endCompile() {
  emitReturn();
  ObjFunc* func = current->func;
  #ifdef DEBUG_PRINT_CODE
  if (!parser.err) {
    disassembleChunk(
      currentChunk(),
      func->name != NULL
      ? func->name->chars : "<script>"
    );
  }
  #endif
  current = current->enclosing;
  return func;
}

static void beginScope() {
  current->scopeDepth++;
}

static void endScope() {
  current->scopeDepth--;
  while (
    current->localCount > 0 &&
    current->locals[current->localCount - 1].depth >
    current->scopeDepth
  ) {
    if (current->locals[current->localCount - 1].isCaptured) {
      emitByte(OP_CLOSE_UPVAL);
    }
    else {
      emitByte(OP_POP);
    }
    current->localCount--;
  }
}

static void expression();
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);

static void binary(bool canAssign) {
  TokenType opType = parser.previous.type;
  ParseRule* rule = getRule(opType);
  parsePrecedence((Precedence)(rule->precedence + 1));
  switch (opType) {
    case BANG_EQU: emitByte(OP_NOT_EQU); break;
    case EQU_EQU: emitByte(OP_EQU); break;
    case GT: emitByte(OP_GT); break;
    case GT_EQU: emitByte(OP_GT_EQU); break;
    case LT: emitByte(OP_LT); break;
    case LT_EQU: emitByte(OP_LT_EQU); break;
    case PLUS: emitByte(OP_ADD); break;
    case DASH: emitByte(OP_SUB); break;
    case STAR: emitByte(OP_MUL); break;
    case CARET: emitByte(OP_POW); break;
    case PERCENT: emitByte(OP_MOD); break;
    case SLASH: emitByte(OP_DIV); break;
    default: return;
  }
}

static uint8_t argList() {
  uint8_t argCount = 0;
  if (!check(RIGHT_PAREN)) {
    do {
      expression();
      if (argCount == 255) {
        err("Cannot have more than 255 arguments.");
      }
      argCount++;
    } while (match(COMMA));
  }
  consume(RIGHT_PAREN, "Expected ')' after arguments.");
  return argCount;
}

static void call(bool canAssign) {
  uint8_t argCount = argList();
  emitBytes(OP_CALL, argCount);
}

static void literal(bool canAssign) {
  switch (parser.previous.type) {
    case TFALSE: emitByte(OP_FALSE); break;
    case NIL: emitByte(OP_NIL); break;
    case TTRUE: emitByte(OP_TRUE); break;
    default: return;
  }
}

static void grouping(bool canAssign) {
  expression();
  consume(RIGHT_PAREN, "Expected ')' after expression.");
}

static void number(bool canAssign) {
  double value = strtod(parser.previous.start, NULL);
  emitConst(NUM_VAL(value));
}

static void str(bool canAssign) {
  emitConst(
    OBJ_VAL(copyStr(
      parser.previous.start + 1,
      parser.previous.length - 2
    ))
  );
}

static uint8_t identConst(Token* name) {
  return makeConst(OBJ_VAL(
    copyStr(name->start, name->length)
  ));
}

static void dot(bool canAssign) {
  consume(IDENT, "Expected a property name after '.'.");
  uint8_t name = identConst(&parser.previous);
  if (canAssign && match(EQU)) {
    expression();
    emitBytes(OP_SET_PROP, name);
  }
  else if (match(LEFT_PAREN)) {
    uint8_t argCount = argList();
    emitBytes(OP_INVOKE, name);
    emitByte(argCount);
  }
  else {
    emitBytes(OP_GET_PROP, name);
  }
}

static bool identsEqu(Token* a, Token* b) {
  if (a->length != b->length) {
    return false;
  }
  return memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(Compiler* compiler, Token* name) {
  for (int i = compiler->localCount - 1; i >= 0; i--) {
    Local* local = &compiler->locals[i];
    if (identsEqu(name, &local->name)) {
      if (local->depth == -1) {
        err("Cannot read a local variable in its own initializer.");
      }
      return i;
    }
  }
  return -1;
}

static int addUpval(
  Compiler* compiler,
  uint8_t index,
  bool isLocal
) {
  int upvalCount = compiler->func->upvalCount;
  for (int i = 0; i < upvalCount; i++) {
    Upvalue* upval = &compiler->upvals[i];
    if (upval->index == index && upval->isLocal == isLocal) {
      return i;
    }
  }
  if (upvalCount == UINT8_COUNT) {
    err("Too many variables in function.");
    return 0;
  }
  compiler->upvals[upvalCount].isLocal = isLocal;
  compiler->upvals[upvalCount].index = index;
  return compiler->func->upvalCount++;
}

static int resolveUpval(Compiler* compiler, Token* name) {
  if (compiler->enclosing == NULL) {
    return -1;
  }
  int local = resolveLocal(compiler->enclosing, name);
  if (local != -1) {
    compiler->enclosing->locals[local].isCaptured = true;
    return addUpval(compiler, (uint8_t)local, true);
  }
  int upval = resolveUpval(compiler->enclosing, name);
  if (upval != -1) {
    return addUpval(compiler, (uint8_t)upval, false);
  }
  return -1;
}

static void addLocal(Token name) {
  if (current->localCount == UINT8_COUNT) {
    err("Too many locals in function.");
    return;
  }
  Local* local = &current->locals[current->localCount++];
  local->name = name;
  local->depth = -1;
  local->isCaptured = false;
}

static void declareVar() {
  if (current->scopeDepth == 0) {
    return;
  }
  Token* name = &parser.previous;
  for (int i = current->localCount - 1; i >= 0; i--) {
    Local* local = &current->locals[i];
    if (local->depth != -1 && local->depth < current->scopeDepth) {
      break;
    }
    if (identsEqu(name, &local->name)) {
      err("Duplicate variable in scope.");
    }
  }
  addLocal(*name);
}

static void namedVar(Token name, bool canAssign) {
  uint8_t getOp, setOp;
  int arg = resolveLocal(current, &name);
  if (arg != -1) {
    getOp = OP_GET_LOCAL;
    setOp = OP_SET_LOCAL;
  }
  else if ((arg = resolveUpval(current, &name)) != -1) {
    getOp = OP_GET_UPVAL;
    setOp = OP_SET_UPVAL;
  }
  else {
    arg = identConst(&name);
    getOp = OP_GET_GLOBAL;
    setOp = OP_SET_GLOBAL;
  }
  if (canAssign && match(EQU)) {
    expression();
    emitBytes(setOp, (uint8_t)arg);
  }
  else {
    emitBytes(getOp, (uint8_t)arg);
  }
}

static void variable(bool canAssign) {
  namedVar(parser.previous, canAssign);
}

static Token syntheticToken(const char* text) {
  Token token;
  token.start = text;
  token.length = (int)strlen(text);
  return token;
}

static void super(bool canAssign) {
  if (currentClass == NULL) {
    err("Cannot use 'super' outside of a class.");
  }
  else if (!currentClass->hasSuperclass) {
    err("Cannot use 'super' in a class with no superclass.");
  }
  consume(DOT, "Expected '.' after 'super'.");
  consume(IDENT, "Expected superclass method name.");
  uint8_t name = identConst(&parser.previous);
  namedVar(syntheticToken("this"), false);
  if (match(LEFT_PAREN)) {
    uint8_t argCount = argList();
    namedVar(syntheticToken("super"), false);
    emitBytes(OP_INVOKE_SUPER, name);
    emitByte(argCount);
  }
  else {
    namedVar(syntheticToken("super"), false);
    emitBytes(OP_GET_SUPER, name);
  }
}

static void this(bool canAssign) {
  if (currentClass == NULL) {
    err("Cannot use 'this' outside of a class.");
    return;
  }
  variable(false);
}

static void unary(bool canAssign) {
  TokenType opType = parser.previous.type;
  parsePrecedence(PREC_UNARY);
  switch (opType) {
    case BANG: emitByte(OP_NOT); break;
    case DASH: emitByte(OP_NEGATE); break;
    default: return;
  }
}

static void and_(bool canAssign) {
  int endJmp = emitJmp(OP_JMPF);
  emitByte(OP_POP);
  parsePrecedence(PREC_AND);
  patchJmp(endJmp);
}

static void or_(bool canAssign) {
  int elseJmp = emitJmp(OP_JMPF);
  int endJmp = emitJmp(OP_JMP);
  patchJmp(elseJmp);
  emitByte(OP_POP);
  parsePrecedence(PREC_OR);
  patchJmp(endJmp);
}

static void list(bool canAssign) {
  int itemCount = 0;
  if (!check(RIGHT_BRACK)) {
    do {
      if (check(RIGHT_BRACK)) {
        break;
      }
      parsePrecedence(PREC_OR);
      if (itemCount == UINT8_COUNT) {
        err("Cannot have more than 256 items in a list.");
      }
      itemCount++;
    } while (match(COMMA));
  }
  consume(RIGHT_BRACK, "Expected ']' after list.");
  emitByte(OP_BUILD_LIST);
  emitByte(itemCount);
  return;
}

static void sub(bool canAssign) {
  parsePrecedence(PREC_OR);
  consume(RIGHT_BRACK, "Expected ']' after index.");
  if (canAssign && match(EQU)) {
    expression();
    emitByte(OP_STORE_SUB);
  }
  else {
    emitByte(OP_INDEX_SUB);
  }
  return;
}

// This is the big boy.
ParseRule rules[] = {
  [LEFT_PAREN]    = {grouping, call,   PREC_CALL},
  [RIGHT_PAREN]   = {NULL,     NULL,   PREC_NONE},
  [LEFT_BRACE]    = {NULL,     NULL,   PREC_NONE},
  [RIGHT_BRACE]   = {NULL,     NULL,   PREC_NONE},
  [LEFT_BRACK]    = {list,     sub,    PREC_SUB},
  [RIGHT_BRACK]   = {NULL,     NULL,   PREC_NONE},
  [COMMA]         = {NULL,     NULL,   PREC_NONE},
  [DOT]           = {NULL,     dot,    PREC_CALL},
  [DASH]          = {unary,    binary, PREC_TERM},
  [PLUS]          = {NULL,     binary, PREC_TERM},
  [SEMICOLON]     = {NULL,     NULL,   PREC_NONE},
  [SLASH]         = {NULL,     binary, PREC_FACTOR},
  [STAR]          = {NULL,     binary, PREC_FACTOR},
  [CARET]         = {NULL,     binary, PREC_FACTOR},
  [PERCENT]       = {NULL,     binary, PREC_FACTOR},
  [BANG]          = {unary,    NULL,   PREC_NONE},
  [BANG_EQU]      = {NULL,     binary, PREC_EQU},
  [EQU]           = {NULL,     NULL,   PREC_NONE},
  [EQU_EQU]       = {NULL,     binary, PREC_EQU},
  [GT]            = {NULL,     binary, PREC_COMP},
  [GT_EQU]        = {NULL,     binary, PREC_COMP},
  [LT]            = {NULL,     binary, PREC_COMP},
  [LT_EQU]        = {NULL,     binary, PREC_COMP},
  [UNDERSCORE]    = {NULL,     NULL,   PREC_NONE},
  [RARROW]        = {NULL,     NULL,   PREC_NONE},
  [IDENT]         = {variable, NULL,   PREC_NONE},
  [STR]           = {str,      NULL,   PREC_NONE},
  [NUM]           = {number,   NULL,   PREC_NONE},
  [AND]           = {NULL,     and_,   PREC_AND},
  [MATCH]         = {NULL,     NULL,   PREC_NONE},
  [CLASS]         = {NULL,     NULL,   PREC_NONE},
  [ELSE]          = {NULL,     NULL,   PREC_NONE},
  [TFALSE]        = {literal,  NULL,   PREC_NONE},
  [FOR]           = {NULL,     NULL,   PREC_NONE},
  [DEF]           = {NULL,     NULL,   PREC_NONE},
  [IF]            = {NULL,     NULL,   PREC_NONE},
  [NIL]           = {literal,  NULL,   PREC_NONE},
  [OR]            = {NULL,     or_,    PREC_OR},
  [RETURN]        = {NULL,     NULL,   PREC_NONE},
  [SUPER]         = {super,    NULL,   PREC_NONE},
  [THIS]          = {this,     NULL,   PREC_NONE},
  [TTRUE]         = {literal,  NULL,   PREC_NONE},
  [LET]           = {NULL,     NULL,   PREC_NONE},
  [WHILE]         = {NULL,     NULL,   PREC_NONE},
  [EXTENDS]       = {NULL,     NULL,   PREC_NONE},
  [WITH]          = {NULL,     NULL,   PREC_NONE},
  [ERR]           = {NULL,     NULL,   PREC_NONE},
  [TEOF]          = {NULL,     NULL,   PREC_NONE}
};

static void statement();
static void declaration();

static void block() {
  while (!check(RIGHT_BRACE) && !check(TEOF)) {
    declaration();
  }
  consume(RIGHT_BRACE, "Expected '}' after block.");
}

static void markInitialized() {
  if (current->scopeDepth == 0) {
    return;
  }
  current->locals[current->localCount - 1].depth =
    current->scopeDepth;
}

static void defVar(uint8_t global) {
  if (current->scopeDepth > 0) {
    markInitialized();
    return;
  }
  emitBytes(OP_DEF_GLOBAL, global);
}

static uint8_t parseVar(const char* message) {
  consume(IDENT, message);
  declareVar();
  if (current->scopeDepth > 0) {
    return 0;
  }
  return identConst(&parser.previous);
}

static void func(FuncType type) {
  Compiler compiler;
  initCompiler(&compiler, type);
  beginScope();
  consume(LEFT_PAREN, "Expected '(' after function name.");
  if (!check(RIGHT_PAREN)) {
    do {
      current->func->arity++;
      if (current->func->arity > 255) {
        errAtCurrent("Cannot have more than 255 parameters.");
      }
      uint8_t constant = parseVar("Expected a parameter name.");
      defVar(constant);
    } while (match(COMMA));
  }
  consume(RIGHT_PAREN, "Expected ')' after function parameters.");
  consume(LEFT_BRACE, "Expected a block after function parameters.");
  block();
  ObjFunc* func = endCompile();
  emitBytes(OP_CLOSURE, makeConst(OBJ_VAL(func)));
  for (int i = 0; i < func->upvalCount; i++) {
    emitByte(compiler.upvals[i].isLocal ? 1 : 0);
    emitByte(compiler.upvals[i].index);
  }
}

static void method() {
  consume(IDENT, "Expected a method name.");
  uint8_t constant = identConst(&parser.previous);
  FuncType type = TYPE_METHOD;
  if (
    parser.previous.length == 4 &&
    memcmp(parser.previous.start, "init", 4) == 0
  ) {
    type = TYPE_INITIALIZER;
  }
  func(type);
  emitBytes(OP_METHOD, constant);
}

static void expression() {
  parsePrecedence(PREC_ASSIGN);
}

static void expressionStatement() {
  expression();
  emitByte(OP_POP);
}

static void varDeclaration() {
  uint8_t global = parseVar("Expected variable name.");
  if (match(EQU)) {
    expression();
  }
  else {
    emitByte(OP_NIL);
  }
  defVar(global);
}

static void classDeclaration() {
  consume(IDENT, "Expected a class name.");
  Token className = parser.previous;
  uint8_t nameConst = identConst(&parser.previous);
  declareVar();
  emitBytes(OP_CLASS, nameConst);
  defVar(nameConst);
  ClassCompiler classCompiler;
  classCompiler.hasSuperclass = false;
  classCompiler.enclosing = currentClass;
  currentClass = &classCompiler;
  if (match(EXTENDS)) {
    consume(IDENT, "Expected a superclass name.");
    variable(false);
    if (identsEqu(&className, &parser.previous)) {
      err("Classes cannot inherit from themselves.");
    }
    beginScope();
    addLocal(syntheticToken("super"));
    defVar(0);
    namedVar(className, false);
    emitByte(OP_INHERIT);
    classCompiler.hasSuperclass = true;
  }
  namedVar(className, false);
  consume(LEFT_BRACE, "Expected '{' before class body.");
  while (!check(RIGHT_BRACE) && !check(TEOF)) {
    if (match(DEF)) {
      method();
    }
    else {
      errAtCurrent("Invalid class body contents.");
      break;
    }
  }
  consume(RIGHT_BRACE, "Expected '}' after class body.");
  emitByte(OP_POP);
  if (classCompiler.hasSuperclass) {
    endScope();
  }
  currentClass = currentClass->enclosing;
}

static void funcDeclaration() {
  uint8_t global = parseVar("Expected a function name.");
  markInitialized();
  func(TYPE_FUNC);
  defVar(global);
}

// I love Pratt parsers.
static void parsePrecedence(Precedence precedence) {
  advance();
  ParseFn prefixRule = getRule(parser.previous.type)->prefix;
  if (prefixRule == NULL) {
    err("Expected an expression.");
    return;
  }
  bool canAssign = precedence <= PREC_ASSIGN;
  prefixRule(canAssign);
  while (precedence <= getRule(parser.current.type)->precedence) {
    advance();
    ParseFn infixRule = getRule(parser.previous.type)->infix;
    infixRule(canAssign);
  }
  if (canAssign && match(EQU)) {
    err("Invalid assignment target.");
  }
}

static ParseRule* getRule(TokenType type) {
  return &rules[type];
}

static void forStatement() {
  beginScope();
  consume(LEFT_PAREN, "Expected '(' after 'for'.");
  if (match(SEMICOLON)) {
    // Nothing.
  }
  else if (match(LET)) {
    varDeclaration();
    consume(
      SEMICOLON,
      "Expected ';' after variable declaration in for loop."
    );
  }
  else {
    expressionStatement();
  }
  int loopStart = currentChunk()->count;
  int exitJmp = -1;
  if (!match(SEMICOLON)) {
    expression();
    consume(SEMICOLON, "Expected ';' after loop condition.");
    exitJmp = emitJmp(OP_JMPF);
    emitByte(OP_POP);
  }
  if (!match(RIGHT_PAREN)) {
    int bodyJmp = emitJmp(OP_JMP);
    int incStart = currentChunk()->count;
    expression();
    emitByte(OP_POP);
    consume(RIGHT_PAREN, "Expected ')' after for clause.");
    emitLoop(loopStart);
    loopStart = incStart;
    patchJmp(bodyJmp);
  }
  consume(LEFT_BRACE, "Expected a block after for clause.");
  block();
  emitLoop(loopStart);
  if (exitJmp != -1) {
    patchJmp(exitJmp);
    emitByte(OP_POP);
  }
  endScope();
}

static void ifStatement() {
  consume(LEFT_PAREN, "Expected '(' after 'if'.");
  expression();
  consume(RIGHT_PAREN, "Expected ')' after condition.");
  int thenJmp = emitJmp(OP_JMPF);
  emitByte(OP_POP);
  consume(LEFT_BRACE, "Expected a block after condition.");
  beginScope();
  block();
  int elseJmp = emitJmp(OP_JMP);
  patchJmp(thenJmp);
  emitByte(OP_POP);
  endScope();
  if (match(ELSE)) {
    if (match(IF)) {
      ifStatement();
    }
    else {
      consume(LEFT_BRACE, "Expected a block after 'else'.");
      beginScope();
      block();
      endScope();
    }
  }
  patchJmp(elseJmp);
}

static void returnStatement() {
  if (current->type == TYPE_SCRIPT) {
    err("Cannot return from top-level.");
  }
  // This is the only time you'll use a semicolon.
  if (match(SEMICOLON)) {
    emitReturn();
  }
  else {
    if (current->type == TYPE_INITIALIZER) {
      err("Cannot return a value from an initializer.");
    }
    expression();
    emitByte(OP_RETURN);
  }
}

static void whileStatement() {
  int loopStart = currentChunk()->count;
  consume(LEFT_PAREN, "Expected '(' after 'while'.");
  expression();
  consume(RIGHT_PAREN, "Expected ')' after condition.");
  int exitJmp = emitJmp(OP_JMPF);
  emitByte(OP_POP);
  consume(LEFT_BRACE, "Expected a block after condition.");
  beginScope();
  block();
  emitLoop(loopStart);
  patchJmp(exitJmp);
  emitByte(OP_POP);
  endScope();
}

static void matchStatement() {
  consume(LEFT_PAREN, "Expected '(' after 'match'.");
  expression();
  consume(RIGHT_PAREN, "Expected ')' after value.");
  consume(LEFT_BRACE, "Expected '{' before cases.");
  int state = 0;
  int caseEnds[MAX_CASES];
  int caseCount = 0;
  int previousCaseSkip = -1;
  while (!match(RIGHT_BRACE) && !check(TEOF)) {
    if (match(WITH) || match(UNDERSCORE)) {
      TokenType caseType = parser.previous.type;
      if (state == 2) {
        err("Cannot have another case after the default case.");
      }
      if (state == 1) {
        caseEnds[caseCount++] = emitJmp(OP_JMP);
        patchJmp(previousCaseSkip);
        emitByte(OP_POP);
      }
      if (caseType == WITH) {
        state = 1;
        emitByte(OP_DUP);
        expression();
        consume(RARROW, "Expected '->' after each case.");
        emitByte(OP_EQU);
        previousCaseSkip = emitJmp(OP_JMPF);
        emitByte(OP_POP);
      }
      else {
        state = 2;
        consume(RARROW, "Expected '->' after the default case.");
        previousCaseSkip = -1;
      }
    }
    else {
      if (state == 0) {
        err("Cannot have statements before any case.");
      }
      statement();
    }
  }
  if (caseCount == 0 && state != 2) {
    err("Cannot have an empty match statement.");
  }
  if (state == 1) {
    patchJmp(previousCaseSkip);
    emitByte(OP_POP);
  }
  if (state == 2 && caseCount == 0) {
    err("Cannot have a default-only match statement.");
  }
  for (int i = 0; i < caseCount; i++) {
    patchJmp(caseEnds[i]);
  }
  emitByte(OP_POP);
}

static void sync() {
  parser.panic = false;
  while (parser.current.type != TEOF) {
    if (parser.previous.type == SEMICOLON) {
      return;
    }
    switch (parser.current.type) {
      case CLASS:
      case DEF:
      case LET:
      case FOR:
      case IF:
      case WHILE:
      case RETURN:
        return;
      default: // Nothing
    }
    advance();
  }
}

static void declaration() {
  if (match(DEF)) {
    funcDeclaration();
  }
  else if (match(CLASS)) {
    classDeclaration();
  }
  else if (match(LET)) {
    varDeclaration();
  }
  else {
    statement();
  }
  if (parser.panic) {
    sync();
  }
}

static void statement() {
  if (match(IF)) {
    ifStatement();
  }
  else if (match(RETURN)) {
    returnStatement();
  }
  else if (match(WHILE)) {
    whileStatement();
  }
  else if (match(FOR)) {
    forStatement();
  }
  else if (match(MATCH)) {
    matchStatement();
  }
  else if (match(LEFT_BRACE)) {
    beginScope();
    block();
    endScope();
  }
  else {
    expressionStatement();
  }
}

ObjFunc* compile(const char* source) {
  initScanner(source);
  Compiler compiler;
  initCompiler(&compiler, TYPE_SCRIPT);
  parser.err = false;
  parser.panic = false;
  advance();
  while (!match(TEOF)) {
    declaration();
  }
  ObjFunc* func = endCompile();
  return parser.err ? NULL : func;
}

void markCompilerRoots() {
  Compiler* compiler = current;
  while (compiler != NULL) {
    markObj((Obj*)compiler->func);
    compiler = compiler->enclosing;
  }
}