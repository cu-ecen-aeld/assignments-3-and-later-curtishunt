CC = gcc #aarch64-none-linux-gnu-gcc
all: aesdsocket


aesdsocket: aesdsocket.c
	$(CC) -Wall -g aesdsocket.c -o aesdsocket -pthread

clean:
	rm -f aesdsocket
