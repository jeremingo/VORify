# Compiler
CC = g++

# Compiler flags
CFLAGS = -Ofast -W

# Include directories (if any)
INCLUDES =

# Libraries to link against
LIBS = -lGeographicLib

# Source files (add .cpp if needed)
SRCS = main.cpp stations_within_range.cpp generate_nmea.cpp calculate_bearing.cpp intersection.cpp stations_to_json.cpp

# Object files (derived from source files)
OBJS = $(SRCS:.cpp=.o)

# Executable name
EXEC = vorify

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
