CC = gcc
CFLAGS = -std=gnu11 -ffreestanding -Wall -Wextra -Iinclude

SRCS = core/main.c core/graphics/framebuffer.c
OBJS = $(SRCS:.c=.o)

.PHONY: all clean

all: $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS)
