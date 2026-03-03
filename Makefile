CC = gcc
CFLAGS_BASE = -Wall -Wextra -std=c99
CFLAGS = $(CFLAGS_BASE) -O2
INCLUDES = -Iinclude
UNAME_S := $(shell uname -s)
LDLIBS =
ifeq ($(UNAME_S),Linux)
LDLIBS += -lrt
endif
SRCS = src/main.c src/hw_io_mmio.c src/driver_io.c src/app.c src/time_utils.c
OBJS = $(SRCS:.c=.o)
TARGET = tinytime

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) $(OBJS) -o $(TARGET) $(LDLIBS)

%.o: %.c include/*.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

debug: CFLAGS = $(CFLAGS_BASE) -O0 -g
debug: clean $(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean debug
