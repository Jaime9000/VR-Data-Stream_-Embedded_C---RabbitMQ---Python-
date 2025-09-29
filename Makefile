# VR Telemetry Simulation Makefile
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -g
INCLUDES = -Iinclude
LDFLAGS = -lrabbitmq -lm -lpthread -lrt

# Directories
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
INCLUDE_DIR = include

# Source files
SOURCES = $(SRC_DIR)/main.c $(SRC_DIR)/vr_embedded.c $(SRC_DIR)/vr_rabbitmq.c
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
TARGET = $(BIN_DIR)/vr_telemetry_sim

# Default target
all: $(TARGET)

# Create directories
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Build target
$(TARGET): $(OBJECTS) | $(BIN_DIR)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

# Compile source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Clean build artifacts
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

# Install dependencies (Ubuntu/Debian)
install-deps:
	sudo apt-get update
	sudo apt-get install -y librabbitmq-dev librabbitmq4

# Install Python dependencies
install-python-deps:
	pip install -r requirements.txt

# Run the simulation
run: $(TARGET)
	./$(TARGET)

# Run Python consumer
run-consumer:
	python python/vr_consumer.py

# Run system test
test:
	python test_system.py

# Debug build
debug: CFLAGS += -DDEBUG -g3
debug: $(TARGET)

# Release build
release: CFLAGS += -DNDEBUG -O3
release: clean $(TARGET)

# Help
help:
	@echo "Available targets:"
	@echo "  all              - Build the VR telemetry simulation"
	@echo "  clean            - Remove build artifacts"
	@echo "  install-deps     - Install system dependencies"
	@echo "  install-python-deps - Install Python dependencies"
	@echo "  run              - Run the simulation"
	@echo "  run-consumer     - Run the Python consumer"
	@echo "  test             - Run system tests"
	@echo "  debug            - Build with debug symbols"
	@echo "  release          - Build optimized release version"
	@echo "  help             - Show this help message"

.PHONY: all clean install-deps install-python-deps run run-consumer test debug release help
