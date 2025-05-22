CC ?= gcc #aarch64-none-linux-gnu-gcc

CFLAGS = -Wall -Wextra  -Werror

SRCS = aesdsocket.c

OBJS = $(SRCS:.c=.o)

all: aesdsocket

aesdsocket: $(OBJS)
	$(CC) $(CFLAGS) -g -o aesdsocket $(OBJS) -lpthread -lrt

clean:
	rm -f aesdsocket $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
