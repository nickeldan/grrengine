CC ?= gcc
debug ?= no

LIB_NAME := grrengine

.PHONY: all clean FORCE

all: lib$(LIB_NAME).so lib$(LIB_NAME).a

lib$(LIB_NAME).so: source/lib$(LIB_NAME).so
	cp $< $@

lib$(LIB_NAME).a: source/lib$(LIB_NAME).a
	cp $< $@

source/%: FORCE
	cd source && make $(notdir $@) CC=$(CC) debug=$(debug)

clean:
	rm -f lib$(LIB_NAME).so lib$(LIB_NAME).a
	cd source && make clean
