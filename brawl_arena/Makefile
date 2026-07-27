# Brawl Arena - Makefile
# Top-down 3D arena brawler built with raylib

CC = clang
CFLAGS = -Wall -Wextra -std=c99 -O2
DEBUG_FLAGS = -g -DDEBUG -O0
SANITIZE_FLAGS = -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer

RAYLIB_FLAGS = $(shell pkg-config --cflags raylib)
RAYLIB_LIBS  = $(shell pkg-config --libs raylib)
PROJECT_INCLUDES = \
	-Isrc/core \
	-Isrc/content \
	-Isrc/game \
	-Isrc/presentation \
	-Isrc/ui \
	-Isrc/devtools \
	-Isrc/app

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    FRAMEWORKS = -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
    # Apple clang's ASan runtime deadlocks during initialization on the current
    # macOS 26 toolchain. Keep the maintained target useful with strict UBSan here;
    # non-Darwin builders still run the combined ASan/UBSan suite.
    SANITIZE_FLAGS = -g -O1 -fsanitize=undefined -fno-omit-frame-pointer
    SANITIZER_ENV = UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
else
    SANITIZER_ENV = ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
endif

SRC_DIR = src
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj

SOURCES = $(shell find $(SRC_DIR) -name '*.c' -print)
OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SOURCES))

TARGET = $(BUILD_DIR)/brawl_arena
TEST_TARGET = $(BUILD_DIR)/test_config
HEALER_TEST_TARGET = $(BUILD_DIR)/test_healer
NO_SHAKE_TEST_TARGET = $(BUILD_DIR)/test_no_attack_shake
ARENA_TEST_TARGET = $(BUILD_DIR)/test_arena
GAMEPLAY_TEST_TARGET = $(BUILD_DIR)/test_gameplay
GAME_CORE_OBJECTS = \
	$(OBJ_DIR)/game/ai.o \
	$(OBJ_DIR)/game/arena.o \
	$(OBJ_DIR)/game/brawler.o \
	$(OBJ_DIR)/content/content_catalog.o \
	$(OBJ_DIR)/game/game_events.o \
	$(OBJ_DIR)/core/game_random.o \
	$(OBJ_DIR)/game/gems.o \
	$(OBJ_DIR)/content/map_content.o \
	$(OBJ_DIR)/game/weapons.o
CONFIG_TEST_OBJECTS = $(GAME_CORE_OBJECTS) $(OBJ_DIR)/content/config.o
DEPFILES = $(OBJECTS:.o=.d)

all: check-architecture $(TARGET)

# Order-only prerequisite, so `make rebuild` recreates the dir that `clean` removed.
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(OBJECTS) $(RAYLIB_LIBS) $(FRAMEWORKS) -lm

$(TEST_TARGET): tests/test_config.c $(CONFIG_TEST_OBJECTS)
	$(CC) $(CFLAGS) $(RAYLIB_FLAGS) $(PROJECT_INCLUDES) -o $@ tests/test_config.c $(CONFIG_TEST_OBJECTS) \
		$(RAYLIB_LIBS) $(FRAMEWORKS) -lm

$(HEALER_TEST_TARGET): tests/test_healer.c $(CONFIG_TEST_OBJECTS)
	$(CC) $(CFLAGS) $(RAYLIB_FLAGS) $(PROJECT_INCLUDES) -o $@ tests/test_healer.c $(CONFIG_TEST_OBJECTS) \
		$(RAYLIB_LIBS) $(FRAMEWORKS) -lm

$(NO_SHAKE_TEST_TARGET): tests/test_no_attack_shake.c $(CONFIG_TEST_OBJECTS) $(OBJ_DIR)/presentation/effects.o
	$(CC) $(CFLAGS) $(RAYLIB_FLAGS) $(PROJECT_INCLUDES) -o $@ tests/test_no_attack_shake.c \
		$(CONFIG_TEST_OBJECTS) $(OBJ_DIR)/presentation/effects.o \
		$(RAYLIB_LIBS) $(FRAMEWORKS) -lm

$(ARENA_TEST_TARGET): tests/test_arena.c $(OBJ_DIR)/game/arena.o $(OBJ_DIR)/content/map_content.o
	$(CC) $(CFLAGS) $(RAYLIB_FLAGS) $(PROJECT_INCLUDES) -o $@ tests/test_arena.c \
		$(OBJ_DIR)/game/arena.o $(OBJ_DIR)/content/map_content.o \
		$(RAYLIB_LIBS) $(FRAMEWORKS) -lm

$(GAMEPLAY_TEST_TARGET): tests/test_gameplay.c $(GAME_CORE_OBJECTS) $(OBJ_DIR)/app/player.o
	$(CC) $(CFLAGS) $(RAYLIB_FLAGS) $(PROJECT_INCLUDES) -o $@ tests/test_gameplay.c \
		$(GAME_CORE_OBJECTS) $(OBJ_DIR)/app/player.o \
		$(RAYLIB_LIBS) $(FRAMEWORKS) -lm

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -MP $(RAYLIB_FLAGS) $(PROJECT_INCLUDES) -c $< -o $@

-include $(DEPFILES)

run: $(TARGET)
	./$(TARGET)

debug: CFLAGS = -Wall -Wextra -std=c99 $(DEBUG_FLAGS)
debug: clean $(TARGET)

sanitize: CFLAGS = -Wall -Wextra -std=c99 $(SANITIZE_FLAGS)
sanitize: clean check-architecture $(TEST_TARGET) $(HEALER_TEST_TARGET) $(NO_SHAKE_TEST_TARGET) $(ARENA_TEST_TARGET) $(GAMEPLAY_TEST_TARGET)
	$(SANITIZER_ENV) ./$(TEST_TARGET)
	$(SANITIZER_ENV) ./$(HEALER_TEST_TARGET)
	$(SANITIZER_ENV) ./$(NO_SHAKE_TEST_TARGET)
	$(SANITIZER_ENV) ./$(ARENA_TEST_TARGET)
	$(SANITIZER_ENV) ./$(GAMEPLAY_TEST_TARGET)

clean:
	rm -rf $(BUILD_DIR)

rebuild: clean all

test: check-architecture $(TEST_TARGET) $(HEALER_TEST_TARGET) $(NO_SHAKE_TEST_TARGET) $(ARENA_TEST_TARGET) $(GAMEPLAY_TEST_TARGET)
	./$(TEST_TARGET)
	./$(HEALER_TEST_TARGET)
	./$(NO_SHAKE_TEST_TARGET)
	./$(ARENA_TEST_TARGET)
	./$(GAMEPLAY_TEST_TARGET)

validate-config: $(TEST_TARGET)
	./$(TEST_TARGET) --validate config/gameplay.cfg

check-architecture:
	./tools/check_architecture.sh

help:
	@echo "Brawl Arena"
	@echo "  make        - build"
	@echo "  make run    - build and play"
	@echo "  make debug  - build with debug symbols"
	@echo "  make test   - run headless config and combat tests"
	@echo "  make sanitize - run headless tests under ASan/UBSan"
	@echo "  make check-architecture - enforce source dependency boundaries"
	@echo "  make validate-config - validate tracked project defaults"
	@echo "  make clean  - remove build artifacts"

.PHONY: all run debug sanitize clean rebuild test validate-config check-architecture help
