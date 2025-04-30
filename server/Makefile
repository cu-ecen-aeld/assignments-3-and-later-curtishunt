CC ?= gcc
all: aesdsocket

aesdsocket: aesdsocket.c
	$(CROSS_COMPILE)$(CC) -Wall -g aesdsocket.c -o aesdsocket

clean:
	rm -f aesdsocket
