# Makefile for Remote Console Application
# Supports compilation with MinGW-w64 or MSVC

# Compiler settings
CC = gcc
CXX = g++
CFLAGS = -Wall -O2
CXXFLAGS = -Wall -O2 -std=c++11
LDFLAGS = -lws2_32 -ladvapi32

# Target executable
TARGET = my.exe
CPP_EXAMPLE = process_wrapper_example.exe

# Source files
C_SOURCES = my.c
CPP_SOURCES = process_wrapper.cpp
CPP_EXAMPLE_SOURCES = process_wrapper_example.cpp process_wrapper.cpp

# Object files
C_OBJECTS = $(C_SOURCES:.c=.o)
CPP_OBJECTS = $(CPP_SOURCES:.cpp=.o)

.PHONY: all clean c cpp example

# Default target - build C version
all: $(TARGET)

# Build C version (main program)
$(TARGET): $(C_OBJECTS)
	@echo "Linking $@..."
	$(CC) -o $@ $^ $(LDFLAGS)
	@echo "Build complete: $@"

# Build C++ wrapper example
cpp: $(CPP_EXAMPLE)

$(CPP_EXAMPLE): $(CPP_EXAMPLE_SOURCES)
	@echo "Building C++ example..."
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Build complete: $@"

# Compile C source files
%.o: %.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Compile C++ source files
%.o: %.cpp
	@echo "Compiling $<..."
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	@echo "Cleaning..."
	-del /Q *.o *.exe 2>nul || rm -f *.o *.exe
	@echo "Clean complete"

# Help target
help:
	@echo "Available targets:"
	@echo "  all     - Build main C application (default)"
	@echo "  cpp     - Build C++ wrapper example"
	@echo "  clean   - Remove build artifacts"
	@echo "  help    - Show this help message"
