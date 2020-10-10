CC ?= gcc
debug ?= no

COMPILER_FLAGS := -std=gnu11 -fpic -fdiagnostics-color -Wall -Wextra -I../include
ifeq ($(debug),yes)
	COMPILER_FLAGS += -O0 -g -DDEBUG
else
	COMPILER_FLAGS += -O3 -DNDEBUG
endif

OBJECT_FILES := nfa.o nfaCompiler.o nfaRuntime.o

LIB_NAME := libgrrengine

.PHONY: all clean

all: $(LIB_NAME).so $(LIB_NAME).a matchTest searchTest

$(LIB_NAME).so: $(OBJECT_FILES)
	$(CC) -shared -o $@ $^

$(LIB_NAME).a: $(OBJECT_FILES)
	ar rcs $@ $^

%Test: %Test.o $(LIB_NAME).a
	$(CC) $^ -o $@

nfa.o: nfa.c ../include/nfaDef.h ../include/nfaInternals.h
	$(CC) $(COMPILER_FLAGS) -c $<

nfaCompiler.o: nfaCompiler.c ../include/nfaDef.h ../include/nfaInternals.h
	$(CC) $(COMPILER_FLAGS) -c $<

nfaRuntime.o: nfaRuntime.c ../include/nfaRuntime.h ../include/nfaDef.h ../include/nfaInternals.h
	$(CC) $(COMPILER_FLAGS) -c $<

%Test.o: %Test.c ../include/nfa.h ../include/nfaDef.h ../include/nfaCompiler.h ../include/nfaRuntime.h
	$(CC) $(COMPILER_FLAGS) -c $<

clean:
	rm -f $(LIB_NAME).so $(LIB_NAME).a *.o matchTest searchTest
