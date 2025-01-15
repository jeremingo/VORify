# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -O2

# Include directories (if any)
INCLUDES =

# Libraries to link against
LIBS = -lrtlsdr

# Source files
SRCS = vorify.c

# Object files (derived from source files)
OBJS = $(SRCS:.c=.o)

# Executable name
EXEC = vorify

# Default target: build the executable
all: $(EXEC)

# Link the executable from object files
$(EXEC): $(OBJS)
    $(CC) $(CFLAGS) -o $(EXEC) $(OBJS) $(LIBS)

# Compile source files into object files
%.o: %.c
    $(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Clean up build files
clean:
    rm -f $(OBJS) $(EXEC)

# Phony targets (not actual files)
.PHONY: all clean