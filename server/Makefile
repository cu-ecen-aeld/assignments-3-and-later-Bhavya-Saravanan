# server/Makefile  (same style, just outputs aesdsocket)
COMPILER ?= $(CROSS_COMPILE)gcc

BUILD_FLAGS ?= -g -Wall -Werror
LINK_FLAGS  ?= -pthread -lrt

default: aesdsocket
all: aesdsocket

aesdsocket.o: aesdsocket.c
	$(COMPILER) $(BUILD_FLAGS) -c $^ -o $@

aesdsocket: aesdsocket.o
	$(COMPILER) $(BUILD_FLAGS) $^ -o $@ $(LINK_FLAGS)

clean:
	-rm -f *.o aesdsocket

