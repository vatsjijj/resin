#ifndef resin_scanner_h
#define resin_scanner_h

typedef enum {
  // Single char
  LEFT_PAREN, RIGHT_PAREN,
  LEFT_BRACE, RIGHT_BRACE,
  LEFT_BRACK, RIGHT_BRACK,
  COMMA,
  DOT,
  DASH,
  PLUS,
  PERCENT,
  SEMICOLON,
  SLASH,
  STAR,
  CARET,
  BANG,
  EQU,
  GT, LT,
  UNDERSCORE,
  // Two char
  BANG_EQU,
  EQU_EQU,
  GT_EQU, LT_EQU,
  RARROW,
  // Literals
  IDENT,
  STR,
  NUM,
  // Keywords
  EXTENDS,
  MATCH,
  WITH,
  AND, OR,
  IF, ELSE,
  FOR, WHILE,
  CLASS,
  TFALSE, TTRUE,
  NIL,
  DEF,
  SUPER,
  PRINT,
  RETURN,
  THIS,
  LET,
  // Other
  ERR, TEOF
} TokenType;

typedef struct {
  TokenType type;
  const char* start;
  int length;
  int line;
} Token;

void initScanner(const char* source);
Token scanToken();

#endif