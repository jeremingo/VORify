# Compiler
CC = g++

# Compiler flags
CFLAGS = -Ofast -W -I /usr/local/include/librtlsdr

# Include directories (if any)
INCLUDES =

# Libraries to link against
LIBS = -lliquid -lrtlsdr

# Source files (add .cpp if needed)
SRCS = identify-station.cpp

# Object files (derived from source files)
OBJS = $(SRCS:.cpp=.o)

# Executable name
EXEC = identify-station

# Default target: build the executable
all: $(EXEC)

# Link the executable from object files
$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) -o $(EXEC) $(OBJS) $(LIBS)

# Compile C++ source files into object files
%.o: %.cpp
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Clean up build files
clean:
	rm -f $(OBJS) $(EXEC)

.PHONY: all clean