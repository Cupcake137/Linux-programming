CC = gcc
CFLAGS = -Wall -Wextra -O2 -pthread -Iinc
TARGET = gateway

SRCS = src/main.c \
       src/logging.c \
       src/logger_proc.c \
       src/sbuffer.c \
       src/thread_workers.c

OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(CFLAGS)

clean:
	rm -f $(TARGET) $(OBJS) logFifo gateway.log

.PHONY: all clean