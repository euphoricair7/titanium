# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O3 -pthread
DEBUG_FLAGS = -g -DDEBUG -O0
RELEASE_FLAGS = -O3 -DNDEBUG

# Directories
SRC_DIR = .
INC_DIR = .
RWQUEUE_DIR = readerwriterqueue-master

# Include paths
INCLUDES = -I$(INC_DIR) -I$(RWQUEUE_DIR)

# Source files
SOURCES = code.cpp
HEADERS = code.h

# Object files
OBJECTS = $(SOURCES:.cpp=.o)

# Target executable
TARGET = high_freq_prog

# Default target
all: $(TARGET)

# Release build (default)
$(TARGET): CXXFLAGS += $(RELEASE_FLAGS)
$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^

# Debug build
debug: CXXFLAGS += $(DEBUG_FLAGS)
debug: clean $(TARGET)

# Object file compilation
%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(TARGET)

# Install (optional - copies to /usr/local/bin)
install: $(TARGET)
	sudo cp $(TARGET) /usr/local/bin/

# Uninstall
uninstall:
	sudo rm -f /usr/local/bin/$(TARGET)

# Run the program
run: $(TARGET)
	./$(TARGET)

# Run with debugging
run-debug: debug
	./$(TARGET)

# Check for memory leaks with valgrind
valgrind: debug
	valgrind --leak-check=full --show-leak-kinds=all ./$(TARGET)

# Performance profiling with perf
profile: $(TARGET)
	perf record -g ./$(TARGET)
	perf report

# Format code (requires clang-format)
format:
	clang-format -i *.cpp *.h

# Static analysis with cppcheck
analyze:
	cppcheck --enable=all --std=c++17 $(SOURCES) $(HEADERS)

# Help target
help:
	@echo "Available targets:"
	@echo "  all (default) - Build release version"
	@echo "  debug         - Build debug version"
	@echo "  clean         - Remove build artifacts"
	@echo "  run           - Build and run the program"
	@echo "  run-debug     - Build debug version and run"
	@echo "  valgrind      - Run with valgrind memory checker"
	@echo "  profile       - Run performance profiling"
	@echo "  format        - Format source code"
	@echo "  analyze       - Run static code analysis"
	@echo "  install       - Install to /usr/local/bin"
	@echo "  uninstall     - Remove from /usr/local/bin"
	@echo "  help          - Show this help message"

# Phony targets
.PHONY: all debug clean install uninstall run run-debug valgrind profile format analyze help