CC ?= aarch64-linux-gnu-gcc
all: aesdsocket


ifneq ($(CROSS_COMPILE),)
	CC = aarch64-linux-gnu-gcc
endif

aesdsocket: aesdsocket.c
	$(CC) -Wall -g aesdsocket.c -o aesdsocket

clean:
	rm -f aesdsocket
