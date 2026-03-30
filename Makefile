# Compiler settings
CXX = g++
CXXFLAGS = -fPIC -Wall -O2
LDFLAGS = -shared

# Directories
CPP_DIR = cpp_engine
BUILD_DIR = build
GO_DIR = go_network

# Output
LIB_NAME = libengine.so
LIB_PATH = $(BUILD_DIR)/$(LIB_NAME)

# Source files
CPP_SRCS = $(wildcard $(CPP_DIR)/*.cpp)

# Default target
all: build

# Build C++ shared library
build:
	@echo "🔧 Building C++ shared library..."
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(CPP_SRCS) $(LDFLAGS) -o $(LIB_PATH)
	@echo "✅ Built $(LIB_PATH)"

# Run Go program
run: build
	@echo "🚀 Running Go app..."
	cd $(GO_DIR) && LD_LIBRARY_PATH=../$(BUILD_DIR) go run main.go

# Clean build files
clean:
	@echo "🧹 Cleaning..."
	rm -rf $(BUILD_DIR)
	@echo "✅ Cleaned"

# Rebuild from scratch
rebuild: clean build

.PHONY: all build run clean rebuild