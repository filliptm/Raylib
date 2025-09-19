# Hearthstone Clone - Modular Implementation
# Makefile for building the modular Hearthstone card game

# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -std=c99 -D_DEFAULT_SOURCE -Wno-missing-braces

# Source files
HEARTHSTONE_DIR = src/hearthstone
HEARTHSTONE_SRCS = $(HEARTHSTONE_DIR)/main.c \
                   $(HEARTHSTONE_DIR)/card.c \
                   $(HEARTHSTONE_DIR)/player.c \
                   $(HEARTHSTONE_DIR)/game_state.c \
                   $(HEARTHSTONE_DIR)/combat.c \
                   $(HEARTHSTONE_DIR)/effects.c \
                   $(HEARTHSTONE_DIR)/render.c \
                   $(HEARTHSTONE_DIR)/input.c \
                   $(HEARTHSTONE_DIR)/errors.c \
                   $(HEARTHSTONE_DIR)/config.c \
                   $(HEARTHSTONE_DIR)/resources.c \
                   $(HEARTHSTONE_DIR)/animation.c \
                   $(HEARTHSTONE_DIR)/audio.c

# Render module source files
RENDER_SRCS = $(HEARTHSTONE_DIR)/render/board_renderer.c \
              $(HEARTHSTONE_DIR)/render/card_renderer.c \
              $(HEARTHSTONE_DIR)/render/ui_renderer.c \
              $(HEARTHSTONE_DIR)/render/effect_renderer.c

# All source files
ALL_SRCS = $(HEARTHSTONE_SRCS) $(RENDER_SRCS)
HEARTHSTONE_OBJS = $(ALL_SRCS:.c=.o)

# Output executable
TARGET = hearthstone

# Platform detection
UNAME_S := $(shell uname -s)

# macOS specific settings
ifeq ($(UNAME_S),Darwin)
    # Check if raylib is installed via Homebrew
    RAYLIB_PATH = $(shell brew --prefix raylib 2>/dev/null || echo "")
    
    ifneq ($(RAYLIB_PATH),)
        # Raylib installed via Homebrew
        INCLUDES = -I$(RAYLIB_PATH)/include
        LIBS = -L$(RAYLIB_PATH)/lib -lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
    else
        # System-wide or manual installation
        INCLUDES = -I/usr/local/include -I/opt/homebrew/include
        LIBS = -L/usr/local/lib -L/opt/homebrew/lib -lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
    endif
endif

# Linux specific settings
ifeq ($(UNAME_S),Linux)
    INCLUDES = -I/usr/local/include
    LIBS = -L/usr/local/lib -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
endif

# Windows (MinGW) specific settings
ifeq ($(OS),Windows_NT)
    INCLUDES = -I./include
    LIBS = -L./lib -lraylib -lopengl32 -lgdi32 -lwinmm
    TARGET = hearthstone.exe
endif

# Default target
all: $(TARGET)

# Build object files for main directory
$(HEARTHSTONE_DIR)/%.o: $(HEARTHSTONE_DIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -I$(HEARTHSTONE_DIR) -c $< -o $@

# Build object files for render subdirectory
$(HEARTHSTONE_DIR)/render/%.o: $(HEARTHSTONE_DIR)/render/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -I$(HEARTHSTONE_DIR) -c $< -o $@

# Build main executable
$(TARGET): $(HEARTHSTONE_OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(TARGET) $(HEARTHSTONE_OBJS) $(LIBS)

# Clean target
clean:
	rm -f $(TARGET) $(HEARTHSTONE_OBJS)
	rm -f $(HEARTHSTONE_DIR)/render/*.o

# Default run
run: $(TARGET)
	./$(TARGET)

# Development targets
build: $(TARGET)

debug: CFLAGS += -g -DDEBUG
debug: $(TARGET)

release: CFLAGS += -O2 -DNDEBUG
release: clean $(TARGET)

# Install raylib (macOS with Homebrew)
install-raylib-mac:
	brew install raylib

# Install raylib (Linux - Ubuntu/Debian)
install-raylib-linux:
	sudo apt update
	sudo apt install build-essential git cmake libasound2-dev mesa-common-dev libx11-dev libxrandr-dev libxi-dev xorg-dev libgl1-mesa-dev libglu1-mesa-dev
	git clone https://github.com/raysan5/raylib.git /tmp/raylib
	cd /tmp/raylib && mkdir build && cd build && cmake .. && make && sudo make install
	rm -rf /tmp/raylib

# Help target
help:
	@echo "Hearthstone Clone - Modular Implementation"
	@echo "=========================================="
	@echo ""
	@echo "Available targets:"
	@echo "  all                  - Build the game (default)"
	@echo "  build                - Same as 'all'"
	@echo "  run                  - Build and run the game"
	@echo "  debug                - Build with debug symbols"
	@echo "  release              - Build optimized release version"
	@echo "  clean                - Remove all build artifacts"
	@echo "  install-raylib-mac   - Install raylib via Homebrew (macOS)"
	@echo "  install-raylib-linux - Install raylib from source (Linux)"
	@echo "  help                 - Show this help message"
	@echo ""
	@echo "Quick start:"
	@echo "  make run             - Build and play the game"
	@echo ""
	@echo "Advanced Development:"
	@echo "  For advanced build options and unit tests, use:"
	@echo "  cd src/hearthstone && make help"

# Phony targets
.PHONY: all build run clean debug release install-raylib-mac install-raylib-linux help