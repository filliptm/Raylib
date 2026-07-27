# Brawl Arena - Makefile
# Top-down 3D arena brawler built with raylib

CC = clang
CFLAGS = -Wall -Wextra -std=c99 -O2
DEBUG_FLAGS = -g -DDEBUG -O0

RAYLIB_FLAGS = $(shell pkg-config --cflags raylib)
RAYLIB_LIBS  = $(shell pkg-config --libs raylib)

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    FRAMEWORKS = -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
endif

SRC_DIR = src
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj

SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

TARGET = $(BUILD_DIR)/brawl_arena

all: $(TARGET)

# Order-only prerequisite, so `make rebuild` recreates the dir that `clean` removed.
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(OBJECTS) $(RAYLIB_LIBS) $(FRAMEWORKS) -lm

# Every object depends on every header. Header edits (especially types.h, which
# defines struct layouts shared by all modules) must rebuild everything, or stale
# objects read struct fields at old offsets and fail silently at runtime.
HDRS = $(wildcard $(SRC_DIR)/*.h)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(HDRS) | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(RAYLIB_FLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

debug: CFLAGS = -Wall -Wextra -std=c99 $(DEBUG_FLAGS)
debug: clean $(TARGET)

clean:
	rm -rf $(BUILD_DIR)

rebuild: clean all

help:
	@echo "Brawl Arena"
	@echo "  make        - build"
	@echo "  make run    - build and play"
	@echo "  make debug  - build with debug symbols"
	@echo "  make clean  - remove build artifacts"

.PHONY: all run debug clean rebuild help
