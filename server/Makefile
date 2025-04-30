CC = gcc
all: aesdsocket


ifneq ($(CROSS_COMPILE),)
	CC = $(CROSS_COMPILE)gcc
endif

aesdsocket: aesdsocket.c
	$(CC) -Wall -g aesdsocket.c -o aesdsocket

clean:
	rm -f aesdsocket
