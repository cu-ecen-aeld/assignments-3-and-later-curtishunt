CC ?= $(CROSS_COMPILE)gcc
CFLAGS= -g -Wall -Werror -Wno-unused-result -Wno-maybe-uninitialized -I. 
LDFLAGS ?= -lpthread -lrt

all: aesdsocket_build

aesdsocket_build: aesdsocket.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o aesdsocket aesdsocket.o 

%.o: %.c 
	$(CC) $(CFLAGS) -c -o $@ $< $(CFLAGS)

clean:
	rm -rf *.o
	rm -rf aesdsocket