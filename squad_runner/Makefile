# Squad Runner - Makefile
# Hybrid 3D/2D runner game built with raylib

# Compiler settings
CC = clang
CFLAGS = -Wall -std=c99 -O2

# raylib settings (using pkg-config)
RAYLIB_FLAGS = $(shell pkg-config --cflags raylib)
RAYLIB_LIBS = $(shell pkg-config --libs raylib)

# macOS specific frameworks
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    FRAMEWORKS = -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
endif

# Source and output
SRC = src/main.c
OUT = squad_runner

# Default target
all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) $(RAYLIB_FLAGS) -o $(OUT) $(SRC) $(RAYLIB_LIBS) $(FRAMEWORKS)

# Run the game
run: $(OUT)
	./$(OUT)

# Clean build
clean:
	rm -f $(OUT)

# Rebuild
rebuild: clean all

.PHONY: all run clean rebuild
