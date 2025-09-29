CC ?= $(CROSS_COMPILE)gcc
TARGET?=aesdsocket
OBJS = $(SRC:.c=.o)
SRC  = aesdsocket.c
CFLAGS ?= -Werror -Wall -Wunused -Wunused-variable -Wextra
LDFLAGS ?= -lpthread -lrt

all:$(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(INCLUDES) -o $(TARGET) $(OBJS) 

%.o: %.c
	$(CC) -c $< -o $@
clean: 
	rm -f $(OBJS) $(TARGET)
