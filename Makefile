CC ?= gcc
debug ?= no

LIB_NAME := libgrrengine

.PHONY: all clean FORCE

all: $(LIB_NAME).so $(LIB_NAME).a

$(LIB_NAME).so: source/$(LIB_NAME).so
	cp $< $@

$(LIB_NAME).a: source/$(LIB_NAME).a
	cp $< $@

source/%: FORCE
	cd source && make $(notdir $@) CC=$(CC) debug=$(debug)

clean:
	rm -f $(LIB_NAME).so $(LIB_NAME).a
	cd source && make clean
