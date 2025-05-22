CC ?= gcc #aarch64-none-linux-gnu-gcc

CFLAGS = -Wall -Wextra  -Werror

SRCS = aesdsocket.c

OBJS = $(SRCS:.c=.o)
LDFLAGS ?= -lpthread -lrt

all: aesdsocket

aesdsocket: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -g -o aesdsocket $(OBJS)

clean:
	rm -f aesdsocket $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
