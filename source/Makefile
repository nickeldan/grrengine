CC ?= gcc
debug ?= no

COMPILER_FLAGS := -std=gnu11 -fpic -fdiagnostics-color -Wall -Wextra -I../include
ifeq ($(debug),yes)
	COMPILER_FLAGS += -O0 -g -DDEBUG
else
	COMPILER_FLAGS += -O3 -DNDEBUG
endif

OBJECT_FILES := nfa.o nfaCompiler.o nfaRuntime.o

LIBNAME := grrengine

.PHONY: all clean

all: lib$(LIBNAME).so lib$(LIBNAME).a matchTest searchTest

lib$(LIBNAME).so: $(OBJECT_FILES)
	$(CC) -shared -o $@ $^

lib$(LIBNAME).a: $(OBJECT_FILES)
	ar rcs $@ $^

%Test: %Test.o lib$(LIBNAME).a
	$(CC) $^ -o $@

nfa.o: nfa.c ../include/*.h
	$(CC) $(COMPILER_FLAGS) -c $<

nfaCompiler.o: nfaCompiler.c ../include/*.h
	$(CC) $(COMPILER_FLAGS) -c $<

nfaRuntime.o: nfaRuntime.c ../include/*.h
	$(CC) $(COMPILER_FLAGS) -c $<

%Test.o: %Test.c ../include/*.h
	$(CC) $(COMPILER_FLAGS) -c $<

clean:
	rm -f lib$(LIBNAME).so lib$(LIBNAME).a *.o matchTest searchTest
