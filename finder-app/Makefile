

# Define the compiler and flags
CC = gcc
CFLAGS = -Wall -g
OBJECTS = writer.o
TARGET = writer
LDFLAGS =

ifneq ($(CROSS_COMPILE),)
	CC = $(CROSS_COMPILE)gcc
	LDFLAGS += -static
endif

# Default target: builds the application
all: $(TARGET)

# Rule to link the object files into the final executable
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $(TARGET)

# Rule to compile .c files into .o files
writer.o: writer.c
	$(CC) $(CFLAGS) -c writer.c

# Clean target: removes the executable and all object files
clean:
	rm -f $(TARGET) $(OBJECTS)

# Phony targets to avoid conflicts with files named "clean" or "all"
.PHONY: all clean

