# Cross-compile friendly Makefile
CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Wextra -Werror -O2
LDFLAGS ?= -pthread -lrt

TARGET ?= aesdsocket
SRC := aesdsocket.c
OBJ := $(SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)
.PHONY: all clean

