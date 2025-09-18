# Raylib Hello World Makefile

# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -std=c99 -D_DEFAULT_SOURCE -Wno-missing-braces

# Source files
HELLO_SRC = hello_world.c
CARD_SRC = card_game_3d.c
HEARTHSTONE_SRC = hearthstone_clone.c

# Output executables
HELLO_TARGET = hello_world
CARD_TARGET = card_game_3d
HEARTHSTONE_TARGET = hearthstone_clone

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
    TARGET = hello_world.exe
endif

# Default target
all: $(HELLO_TARGET) $(CARD_TARGET) $(HEARTHSTONE_TARGET)

# Build targets
$(HELLO_TARGET): $(HELLO_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(HELLO_TARGET) $(HELLO_SRC) $(LIBS)

$(CARD_TARGET): $(CARD_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(CARD_TARGET) $(CARD_SRC) $(LIBS)

$(HEARTHSTONE_TARGET): $(HEARTHSTONE_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(HEARTHSTONE_TARGET) $(HEARTHSTONE_SRC) $(LIBS)

# Individual build targets
hello: $(HELLO_TARGET)
card: $(CARD_TARGET)
hearthstone: $(HEARTHSTONE_TARGET)

# Clean target
clean:
	rm -f $(HELLO_TARGET) $(CARD_TARGET) $(HEARTHSTONE_TARGET)

# Run targets
run-hello: $(HELLO_TARGET)
	./$(HELLO_TARGET)

run-card: $(CARD_TARGET)
	./$(CARD_TARGET)

run-hearthstone: $(HEARTHSTONE_TARGET)
	./$(HEARTHSTONE_TARGET)

# Default run (hearthstone clone)
run: $(HEARTHSTONE_TARGET)
	./$(HEARTHSTONE_TARGET)

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
	@echo "Available targets:"
	@echo "  all              - Build all executables (hello_world, card_game_3d, hearthstone_clone)"
	@echo "  hello            - Build only hello_world"
	@echo "  card             - Build only card_game_3d"
	@echo "  hearthstone      - Build only hearthstone_clone"
	@echo "  clean            - Remove all executables"
	@echo "  run              - Build and run the hearthstone clone (default)"
	@echo "  run-hello        - Build and run hello_world"
	@echo "  run-card         - Build and run card_game_3d"
	@echo "  run-hearthstone  - Build and run hearthstone_clone"
	@echo "  install-raylib-mac   - Install raylib via Homebrew (macOS)"
	@echo "  install-raylib-linux - Install raylib from source (Linux)"
	@echo "  help             - Show this help message"

# Phony targets
.PHONY: all hello card hearthstone clean run run-hello run-card run-hearthstone install-raylib-mac install-raylib-linux help