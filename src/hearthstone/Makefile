# Hearthstone Clone Makefile

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
DEBUG_FLAGS = -g -DDEBUG

# Detect OS and set appropriate flags
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    # macOS
    RAYLIB_PATH = $(shell brew --prefix raylib 2>/dev/null)
    ifneq ($(RAYLIB_PATH),)
        CFLAGS += -I$(RAYLIB_PATH)/include
        RAYLIB_FLAGS = -L$(RAYLIB_PATH)/lib -lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
    else
        RAYLIB_FLAGS = -lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
    endif
else
    # Linux
    RAYLIB_FLAGS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
endif

# Directories
SRC_DIR = .
RENDER_DIR = render
TEST_DIR = tests
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
TEST_OBJ_DIR = $(BUILD_DIR)/test_obj

# Create directories
$(shell mkdir -p $(OBJ_DIR) $(TEST_OBJ_DIR))

# Source files
SOURCES = main.c \
          card.c \
          player.c \
          game_state.c \
          combat.c \
          effects.c \
          render.c \
          input.c \
          errors.c \
          config.c \
          resources.c \
          animation.c \
          audio.c

# Render module sources
RENDER_SOURCES = $(RENDER_DIR)/board_renderer.c \
                 $(RENDER_DIR)/card_renderer.c \
                 $(RENDER_DIR)/ui_renderer.c \
                 $(RENDER_DIR)/effect_renderer.c

# Test sources
TEST_SOURCES = $(TEST_DIR)/test_errors.c \
               $(TEST_DIR)/test_config.c \
               $(TEST_DIR)/test_animation.c

# Object files
OBJECTS = $(SOURCES:%.c=$(OBJ_DIR)/%.o)
RENDER_OBJECTS = $(RENDER_SOURCES:$(RENDER_DIR)/%.c=$(OBJ_DIR)/%.o)
TEST_OBJECTS = $(TEST_SOURCES:$(TEST_DIR)/%.c=$(TEST_OBJ_DIR)/%.o)

# Library objects (everything except main.c)
LIB_SOURCES = $(filter-out main.c, $(SOURCES))
LIB_OBJECTS = $(LIB_SOURCES:%.c=$(OBJ_DIR)/%.o)

# Target executable
TARGET = $(BUILD_DIR)/hearthstone
TEST_TARGET = $(BUILD_DIR)/test_runner

# Default target
all: $(TARGET)

# Debug build
debug: CFLAGS += $(DEBUG_FLAGS)
debug: $(TARGET)

# Main executable
$(TARGET): $(OBJECTS) $(RENDER_OBJECTS)
	$(CC) $(OBJECTS) $(RENDER_OBJECTS) -o $@ $(RAYLIB_FLAGS)

# Object files for main sources
$(OBJ_DIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Object files for render sources
$(OBJ_DIR)/%.o: $(RENDER_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Test object files
$(TEST_OBJ_DIR)/%.o: $(TEST_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Test runner (without raylib for unit tests)
test: $(TEST_TARGET)
	$(TEST_TARGET)

$(TEST_TARGET): $(TEST_OBJECTS) $(LIB_OBJECTS) $(RENDER_OBJECTS)
	$(CC) $(TEST_OBJECTS) $(LIB_OBJECTS) $(RENDER_OBJECTS) -o $@ -lm

# Individual test executables
test_errors: $(TEST_OBJ_DIR)/test_errors.o $(OBJ_DIR)/errors.o
	$(CC) $^ -o $(BUILD_DIR)/$@ -lm
	$(BUILD_DIR)/$@

test_config: $(TEST_OBJ_DIR)/test_config.o $(OBJ_DIR)/config.o $(OBJ_DIR)/errors.o
	$(CC) $^ -o $(BUILD_DIR)/$@ -lm
	$(BUILD_DIR)/$@

test_animation: $(TEST_OBJ_DIR)/test_animation.o $(OBJ_DIR)/animation.o $(OBJ_DIR)/errors.o
	$(CC) $^ -o $(BUILD_DIR)/$@ -lm
	$(BUILD_DIR)/$@

# Install raylib (if needed)
install-raylib:
	@echo "Please install raylib manually or via package manager"
	@echo "Ubuntu/Debian: sudo apt install libraylib-dev"
	@echo "macOS: brew install raylib"
	@echo "Or build from source: https://github.com/raysan5/raylib"

# Clean build files
clean:
	rm -rf $(BUILD_DIR)
	rm -f *.o

# Clean everything including old object files
clean-all: clean
	rm -f *.o $(RENDER_DIR)/*.o $(TEST_DIR)/*.o

# Format code (if clang-format is available)
format:
	@if command -v clang-format >/dev/null 2>&1; then \
		find . -name "*.c" -o -name "*.h" | grep -v tests | xargs clang-format -i; \
		echo "Code formatted"; \
	else \
		echo "clang-format not found, skipping formatting"; \
	fi

# Static analysis (if cppcheck is available)
analyze:
	@if command -v cppcheck >/dev/null 2>&1; then \
		cppcheck --enable=all --suppress=missingIncludeSystem .; \
	else \
		echo "cppcheck not found, skipping static analysis"; \
	fi

# Generate documentation (if doxygen is available)
docs:
	@if command -v doxygen >/dev/null 2>&1; then \
		doxygen; \
	else \
		echo "doxygen not found, skipping documentation generation"; \
	fi

# Help
help:
	@echo "Available targets:"
	@echo "  all          - Build the game (default)"
	@echo "  debug        - Build with debug symbols"
	@echo "  test         - Run all unit tests"
	@echo "  test_errors  - Run error handling tests"
	@echo "  test_config  - Run configuration tests"
	@echo "  test_animation - Run animation tests"
	@echo "  clean        - Remove build files"
	@echo "  clean-all    - Remove all object files"
	@echo "  format       - Format code with clang-format"
	@echo "  analyze      - Run static analysis with cppcheck"
	@echo "  docs         - Generate documentation with doxygen"
	@echo "  help         - Show this help message"

# Phony targets
.PHONY: all debug test clean clean-all format analyze docs help install-raylib test_errors test_config test_animation

# Dependencies (simplified - in a real project you'd use automatic dependency generation)
$(OBJ_DIR)/main.o: main.c game_state.h render.h input.h
$(OBJ_DIR)/card.o: card.c card.h types.h errors.h
$(OBJ_DIR)/player.o: player.c player.h card.h types.h errors.h
$(OBJ_DIR)/game_state.o: game_state.c game_state.h player.h card.h types.h errors.h
$(OBJ_DIR)/combat.o: combat.c combat.h card.h player.h types.h errors.h
$(OBJ_DIR)/effects.o: effects.c effects.h types.h errors.h
$(OBJ_DIR)/render.o: render.c render.h game_state.h input.h
$(OBJ_DIR)/input.o: input.c input.h game_state.h card.h combat.h
$(OBJ_DIR)/errors.o: errors.c errors.h
$(OBJ_DIR)/config.o: config.c config.h errors.h
$(OBJ_DIR)/resources.o: resources.c resources.h errors.h
$(OBJ_DIR)/animation.o: animation.c animation.h types.h errors.h
$(OBJ_DIR)/audio.o: audio.c audio.h errors.h config.h