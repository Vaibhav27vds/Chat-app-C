CC = gcc
CFLAGS = -Wall -Wextra -pthread -fPIC
LDFLAGS = -pthread -lm -lcrypto

# Directories
SRC_DIR = src
INC_DIR = include
BIN_DIR = bin
OBJ_DIR = obj

# Source and object files
SOURCES = $(SRC_DIR)/main.c \
          $(SRC_DIR)/database.c \
          $(SRC_DIR)/http_server.c \
          $(SRC_DIR)/websocket_server.c \
          $(SRC_DIR)/thread_pool.c \
          $(SRC_DIR)/authentication.c \
          $(SRC_DIR)/utils.c

OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
TARGET = $(BIN_DIR)/chat_server

# Default target
all: $(TARGET)

# Create directories
$(BIN_DIR) $(OBJ_DIR):
	mkdir -p $@

# Compile object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) -c $< -o $@

# Link executable
$(TARGET): $(OBJECTS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) $^ -o $@ $(LDFLAGS)
	@echo "✓ Build successful: $(TARGET)"

# Run the server
run: $(TARGET)
	./$(TARGET)

# Clean build artifacts
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
	@echo "✓ Clean complete"

# Rebuild everything
rebuild: clean all

# Help
help:
	@echo "Available targets:"
	@echo "  all     - Build the chat server (default)"
	@echo "  run     - Build and run the server"
	@echo "  clean   - Remove build artifacts"
	@echo "  rebuild - Clean and build"
	@echo "  help    - Show this help message"

.PHONY: all run clean rebuild help
