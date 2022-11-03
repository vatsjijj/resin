#include <stdio.h>
#include <string.h>
#include "include/common.h"
#include "include/scanner.h"

typedef struct {
  const char* start;
  const char* current;
  int line;
} Scanner;

Scanner scanner;

void initScanner(const char* source) {
  scanner.start = source;
  scanner.current = source;
  scanner.line = 1;
}

static bool isAlpha(char c) {
  return (c >= 'a' && c <= 'z')
      || (c >= 'A' && c <= 'Z');
  //  || c == '_';
}

static bool isDigit(char c) {
  return c >= '0' && c <= '9';
}

static bool atEnd() {
  return *scanner.current == '\0';
}

static char advance() {
  scanner.current++;
  return scanner.current[-1];
}

static char peek() {
  return *scanner.current;
}

static char peekNext() {
  if (atEnd()) {
    return '\0';
  }
  return scanner.current[1];
}

static bool match(char expected) {
  if (atEnd()) {
    return false;
  }
  if (*scanner.current != expected) {
    return false;
  }
  scanner.current++;
  return true;
}

static Token makeToken(TokenType type) {
  Token token;
  token.type = type;
  token.start = scanner.start;
  token.length = (int)(scanner.current - scanner.start);
  token.line = scanner.line;
  return token;
}

static Token errToken(const char* message) {
  Token token;
  token.type = ERR;
  token.start = message;
  token.length = (int)strlen(message);
  token.line = scanner.line;
  return token;
}

static void skipWhitespace() {
  for (;;) {
    char c = peek();
    switch (c) {
      case ' ':
      case '\r':
      case '\t':
        advance();
        break;
      case '\n':
        scanner.line++;
        advance();
        break;
      case '/':
        if (peekNext() == '/') {
          while (peek() != '\n' && !atEnd()) {
            advance();
          }
        }
        else {
          return;
        }
        break;
      default:
        return;
    }
  }
}

static TokenType checkKeyword(
  int start,
  int length,
  const char* rest,
  TokenType type
) {
  if (
    scanner.current - scanner.start == start + length &&
    memcmp(scanner.start + start, rest, length) == 0
  ) {
    return type;
  }
  return IDENT;
}

static TokenType identifierType() {
  switch (scanner.start[0]) {
    case 'c': return checkKeyword(1, 4, "lass", CLASS);
    case 'e':
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'l': return checkKeyword(2, 2, "se", ELSE);
          case 'x': return checkKeyword(2, 5, "tends", EXTENDS);
        }
      }
    case 'i': return checkKeyword(1, 1, "f", IF);
    case 'n': return checkKeyword(1, 2, "il", NIL);
    case 'r': return checkKeyword(1, 5, "eturn", RETURN);
    case 's': return checkKeyword(1, 4, "uper", SUPER);
    case 'l': return checkKeyword(1, 2, "et", LET);
    case 'w':
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'h': return checkKeyword(2, 3, "ile", WHILE);
          case 'i': return checkKeyword(2, 2, "th", WITH);
        }
      }
    case 'f':
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'a': return checkKeyword(2, 3, "lse", TFALSE);
          case 'u': return checkKeyword(2, 2, "nc", FUNC);
          case 'o': return checkKeyword(2, 1, "r", FOR);
        }
      }
      break;
    case 't':
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'h': return checkKeyword(2, 2, "is", THIS);
          case 'r': return checkKeyword(2, 2, "ue", TTRUE);
        }
      }
      break;
    case 'm': return checkKeyword(1, 4, "atch", MATCH);
  }
  return IDENT;
}

static Token identifier() {
  while (isAlpha(peek()) || isDigit(peek())) {
    if (peek() == '_') {
      advance();
    }
    else {
      advance();
    }
  }
  return makeToken(identifierType());
}

static Token number() {
  while (isDigit(peek())) {
    advance();
  }
  if (peek() == '.' && isDigit(peekNext())) {
    advance();
    while (isDigit(peek())) {
      advance();
    }
  }
  return makeToken(NUM);
}

static Token string() {
  while (peek() != '"' && !atEnd()) {
    if (peek() == '\n') {
      scanner.line++;
    }
    advance();
  }
  if (atEnd()) {
    return errToken("Non-terminated string.");
  }
  advance();
  return makeToken(STR);
}

Token scanToken() {
  skipWhitespace();
  scanner.start = scanner.current;
  if (atEnd()) {
    return makeToken(TEOF);
  }

  char c = advance();

  if (isAlpha(c)) {
    return identifier();
  }
  if (isDigit(c)) {
    return number();
  }

  switch (c) {
    case '(': return makeToken(LEFT_PAREN);
    case ')': return makeToken(RIGHT_PAREN);
    case '{': return makeToken(LEFT_BRACE);
    case '}': return makeToken(RIGHT_BRACE);
    case '[': return makeToken(LEFT_BRACK);
    case ']': return makeToken(RIGHT_BRACK);
    case ';': return makeToken(SEMICOLON);
    case ',': return makeToken(COMMA);
    case '.': return makeToken(DOT);
    case '-': return makeToken(
      match('>') ? RARROW : DASH
    );
    case '+': return makeToken(PLUS);
    case '*': return makeToken(STAR);
    case '^': return makeToken(CARET);
    case '/': return makeToken(SLASH);
    case '%': return makeToken(PERCENT);
    case '|':
      return makeToken(
        match('|') ? OR : ERR
      );
    case '&':
      return makeToken(
        match('&') ? AND : ERR
      );
    case '!':
      return makeToken(
        match('=') ? BANG_EQU : BANG
      );
    case '=':
      return makeToken(
        match('=') ? EQU_EQU : EQU
      );
    case '<':
      return makeToken(
        match('=') ? LT_EQU : LT
      );
    case '>':
      return makeToken(
        match('=') ? GT_EQU : GT
      );
    case '"': return string();
    case '_': return makeToken(UNDERSCORE);
  }
  return errToken("Unexpected character.");
}