#ifndef resin_compiler_h
#define resin_compiler_h

#include "chunk.h"
#include "object.h"

ObjFunc* compile(const char* source);
void markCompilerRoots();

#endif