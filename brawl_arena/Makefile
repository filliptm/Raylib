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
TEST_TARGET = $(BUILD_DIR)/test_config
HEALER_TEST_TARGET = $(BUILD_DIR)/test_healer
NO_SHAKE_TEST_TARGET = $(BUILD_DIR)/test_no_attack_shake
TEST_OBJECTS = $(filter-out $(OBJ_DIR)/main.o,$(OBJECTS))

all: $(TARGET)

# Order-only prerequisite, so `make rebuild` recreates the dir that `clean` removed.
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(OBJECTS) $(RAYLIB_LIBS) $(FRAMEWORKS) -lm

$(TEST_TARGET): tests/test_config.c $(TEST_OBJECTS)
	$(CC) $(CFLAGS) $(RAYLIB_FLAGS) -o $@ tests/test_config.c $(TEST_OBJECTS) \
		$(RAYLIB_LIBS) $(FRAMEWORKS) -lm

$(HEALER_TEST_TARGET): tests/test_healer.c $(TEST_OBJECTS)
	$(CC) $(CFLAGS) $(RAYLIB_FLAGS) -o $@ tests/test_healer.c $(TEST_OBJECTS) \
		$(RAYLIB_LIBS) $(FRAMEWORKS) -lm

$(NO_SHAKE_TEST_TARGET): tests/test_no_attack_shake.c $(TEST_OBJECTS)
	$(CC) $(CFLAGS) $(RAYLIB_FLAGS) -o $@ tests/test_no_attack_shake.c $(TEST_OBJECTS) \
		$(RAYLIB_LIBS) $(FRAMEWORKS) -lm

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

test: $(TEST_TARGET) $(HEALER_TEST_TARGET) $(NO_SHAKE_TEST_TARGET)
	./$(TEST_TARGET)
	./$(HEALER_TEST_TARGET)
	./$(NO_SHAKE_TEST_TARGET)

validate-config: $(TEST_TARGET)
	./$(TEST_TARGET) --validate config/gameplay.cfg

help:
	@echo "Brawl Arena"
	@echo "  make        - build"
	@echo "  make run    - build and play"
	@echo "  make debug  - build with debug symbols"
	@echo "  make test   - run headless config and combat tests"
	@echo "  make validate-config - validate tracked project defaults"
	@echo "  make clean  - remove build artifacts"

.PHONY: all run debug clean rebuild test validate-config help
