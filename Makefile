exec = resin
CC = gcc
UNAME_S = uname -s
SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
FLAGS = -std=c17 -Wall -fPIC -Ofast -lm

ifeq ($(OS), Windows_NT)
	RM_COM = del
	DELSRC = del src\*.o
	INST = echo "Cannot run `make install` on Windows."
	UNINST = echo "Run `del resin.exe` instead."
else
	RM_COM = rm -v
	DELSRC = rm -v src/*.o
	INST = cp resin /usr/bin
	UNINST = rm -v /usr/bin/resin
endif

$(exec): $(OBJ)
	$(CC) $(OBJ) $(FLAGS) -o $(exec)

%.o: %.c include/%.h
	$(CC) -c $(FLAGS) $< -o $@

crossbuild:
	# Specifically made to run for
	# cross platform compilation on
	# Linux.
	x86_64-w64-mingw32-gcc src/*.c -o resin.exe

clean:
	@$(DELSRC)
	@# $(RM_COM) resin*

install:
	@$(INST)

uninstall:
	@$(DELSRC)
	@$(RM_COM) resin*
	@$(UNINST)