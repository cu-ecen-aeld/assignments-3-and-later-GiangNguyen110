# Specify the cross-compile executable
CROSS_COMPILE ?= 

# Define variables
CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall -Werror -g
DEBUG_FLAGS = -g -DDEBUG
TARGET = aesdsocket
SOURCES = aesdsocket.c 
OBJECTS = $(SOURCES:.c=.o)

$(info Using compiler: $(CC))

# Default target (first one listed)
all: $(TARGET)

# Debug target
debug: CFLAGS += $(DEBUG_FLAGS)
debug: clean $(TARGET)

# Building the Executable ($(TARGET)):
# This rule specifies how to link the object files into the final executable.
# $^ represents all prerequisites (the object files).
# $@ represents the target name (the executable).
$(TARGET) : $(OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@

# Compiling Source Files (%.o: %.c):
# This is an implicit rule that tells make how to compile any .c file into a .o file.
# $< represents the first prerequisite (the .c file).
$(OBJECTS): %.o: %.c 
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJECTS)
