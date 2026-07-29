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
# Build flavors (plain/debug/sanitize) keep separate object dirs and binary names.
# A shared object dir let sanitizer-instrumented objects leak into the next plain
# link, which failed with missing UBSan runtime symbols.
OBJ_FLAVOR ?=
OBJ_DIR = $(BUILD_DIR)/obj$(OBJ_FLAVOR)

SOURCES = $(shell find $(SRC_DIR) -name '*.c' -print)
OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SOURCES))

TARGET = $(BUILD_DIR)/brawl_arena$(OBJ_FLAVOR)
TEST_TARGET = $(BUILD_DIR)/test_config$(OBJ_FLAVOR)
HEALER_TEST_TARGET = $(BUILD_DIR)/test_healer$(OBJ_FLAVOR)
NO_SHAKE_TEST_TARGET = $(BUILD_DIR)/test_no_attack_shake$(OBJ_FLAVOR)
ARENA_TEST_TARGET = $(BUILD_DIR)/test_arena$(OBJ_FLAVOR)
GAMEPLAY_TEST_TARGET = $(BUILD_DIR)/test_gameplay$(OBJ_FLAVOR)
REGEN_TEST_TARGET = $(BUILD_DIR)/test_regeneration$(OBJ_FLAVOR)
TANK_TEST_TARGET = $(BUILD_DIR)/test_tank$(OBJ_FLAVOR)
LONGSHOT_TEST_TARGET = $(BUILD_DIR)/test_longshot$(OBJ_FLAVOR)
SCRAPPER_TEST_TARGET = $(BUILD_DIR)/test_scrapper$(OBJ_FLAVOR)
SECONDARIES_TEST_TARGET = $(BUILD_DIR)/test_secondaries$(OBJ_FLAVOR)
UI_TEST_TARGET = $(BUILD_DIR)/test_ui$(OBJ_FLAVOR)
CHARACTER_ANIMATION_TEST_TARGET = $(BUILD_DIR)/test_character_animation$(OBJ_FLAVOR)
VFX_TEST_TARGET = $(BUILD_DIR)/test_vfx$(OBJ_FLAVOR)
VFX_EVENTS_TEST_TARGET = $(BUILD_DIR)/test_vfx_events$(OBJ_FLAVOR)
CAMERA_TEST_TARGET = $(BUILD_DIR)/test_camera$(OBJ_FLAVOR)
STUDIO_TEST_TARGET = $(BUILD_DIR)/test_studio$(OBJ_FLAVOR)
CHARACTER_MANIFEST = data/characters/asset_manifest.json
CHARACTER_ASSET_STAMP = $(BUILD_DIR)/assets/characters/.built
CHARACTER_PIPELINE_SOURCES = \
	tools/build_character_assets.py \
	tools/check_character_assets.py \
	$(shell find tools/character_pipeline -name '*.py' -print)
CHARACTER_ASSET_INPUT_LIST := $(shell python3 tools/build_character_assets.py \
		--manifest $(CHARACTER_MANIFEST) --print-inputs)
ifeq ($(strip $(CHARACTER_ASSET_INPUT_LIST)),)
$(error character asset inputs could not be resolved; run \
	'python3 tools/build_character_assets.py --manifest $(CHARACTER_MANIFEST) --print-inputs')
endif
CHARACTER_ASSET_INPUTS = \
	$(CHARACTER_MANIFEST) \
	$(CHARACTER_ASSET_INPUT_LIST)
VFX_MANIFEST = data/vfx/asset_manifest.json
VFX_ASSET_STAMP = $(BUILD_DIR)/assets/vfx/.built
VFX_PIPELINE_SOURCES = tools/build_vfx_assets.py tools/check_vfx_assets.py
VFX_ASSET_INPUTS := $(shell python3 tools/build_vfx_assets.py \
		--manifest $(VFX_MANIFEST) --print-inputs)
ifeq ($(strip $(VFX_ASSET_INPUTS)),)
$(error VFX asset inputs could not be resolved; run \
	'python3 tools/build_vfx_assets.py --manifest $(VFX_MANIFEST) --print-inputs')
endif
VFX_TEST_OBJECTS = \
	$(OBJ_DIR)/presentation/vfx.o \
	$(OBJ_DIR)/presentation/vfx_catalog.o
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

all: check-architecture check-ui check-character-assets check-vfx-assets $(TARGET)

# Order-only prerequisite, so `make rebuild` recreates the dir that `clean` removed.
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(TARGET): $(CHARACTER_ASSET_STAMP) $(VFX_ASSET_STAMP) $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(OBJECTS) $(RAYLIB_LIBS) $(FRAMEWORKS) -lm

$(TEST_TARGET): tests/test_config.c $(CONFIG_TEST_OBJECTS)
	$(CC) $(CFLAGS) $(RAYLIB_FLAGS) $(PROJECT_INCLUDES) -o $@ tests/test_config.c $(CONFIG_TEST_OBJECTS) \
		$(RAYLIB_LIBS) $(FRAMEWORKS) -lm

$(HEALER_TEST_TARGET): tests/test_healer.c $(CONFIG_TEST_OBJECTS)
	$(CC) $(CFLAGS) $(RAYLIB_FLAGS) $(PROJECT_INCLUDES) -o $@ tests/test_healer.c $(CONFIG_TEST_OBJECTS) \
		$(RAYLIB_LIBS) $(FRAMEWORKS) -lm

$(NO_SHAKE_TEST_TARGET): tests/test_no_attack_shake.c $(CONFIG_TEST_OBJECTS) \
		$(OBJ_DIR)/presentation/effects.o $(OBJ_DIR)/presentation/character_animation.o \
		$(VFX_TEST_OBJECTS)
	$(CC) $(CFLAGS) $(RAYLIB_FLAGS) $(PROJECT_INCLUDES) -o $@ tests/test_no_attack_shake.c \
		$(CONFIG_TEST_OBJECTS) $(OBJ_DIR)/presentation/effects.o \
		$(OBJ_DIR)/presentation/character_animation.o $(VFX_TEST_OBJECTS) \
		$(RAYLIB_LIBS) $(FRAMEWORKS) -lm

$(ARENA_TEST_TARGET): tests/test_arena.c $(OBJ_DIR)/game/arena.o $(OBJ_DIR)/content/map_content.o
	$(CC) $(CFLAGS) $(RAYLIB_FLAGS) $(PROJECT_INCLUDES) -o $@ tests/test_arena.c \
		$(OBJ_DIR)/game/arena.o $(OBJ_DIR)/content/map_content.o \
		$(RAYLIB_LIBS) $(FRAMEWORKS) -lm

$(GAMEPLAY_TEST_TARGET): tests/test_gameplay.c $(GAME_CORE_OBJECTS) $(OBJ_DIR)/app/player.o $(OBJ_DIR)/app/game_commands.o
	$(CC) $(CFLAGS) $(RAYLIB_FLAGS) $(PROJECT_INCLUDES) -o $@ tests/test_gameplay.c \
		$(GAME_CORE_OBJECTS) $(OBJ_DIR)/app/player.o $(OBJ_DIR)/app/game_commands.o \
		$(RAYLIB_LIBS) $(FRAMEWORKS) -lm

$(REGEN_TEST_TARGET): tests/test_regeneration.c $(GAME_CORE_OBJECTS)
	$(CC) $(CFLAGS) $(RAYLIB_FLAGS) $(PROJECT_INCLUDES) -o $@ tests/test_regeneration.c \
		$(GAME_CORE_OBJECTS) $(RAYLIB_LIBS) $(FRAMEWORKS) -lm

$(TANK_TEST_TARGET): tests/test_tank.c $(GAME_CORE_OBJECTS)
	$(CC) $(CFLAGS) $(RAYLIB_FLAGS) $(PROJECT_INCLUDES) -o $@ tests/test_tank.c \
		$(GAME_CORE_OBJECTS) $(RAYLIB_LIBS) $(FRAMEWORKS) -lm

$(LONGSHOT_TEST_TARGET): tests/test_longshot.c $(GAME_CORE_OBJECTS)
	$(CC) $(CFLAGS) $(RAYLIB_FLAGS) $(PROJECT_INCLUDES) -o $@ tests/test_longshot.c \
		$(GAME_CORE_OBJECTS) $(RAYLIB_LIBS) $(FRAMEWORKS) -lm

$(SCRAPPER_TEST_TARGET): tests/test_scrapper.c $(GAME_CORE_OBJECTS)
	$(CC) $(CFLAGS) $(RAYLIB_FLAGS) $(PROJECT_INCLUDES) -o $@ tests/test_scrapper.c \
		$(GAME_CORE_OBJECTS) $(RAYLIB_LIBS) $(FRAMEWORKS) -lm

$(STUDIO_TEST_TARGET): tests/test_studio.c $(GAME_CORE_OBJECTS) $(OBJ_DIR)/app/studio_session.o
	$(CC) $(CFLAGS) $(RAYLIB_FLAGS) $(PROJECT_INCLUDES) -o $@ tests/test_studio.c \
		$(GAME_CORE_OBJECTS) $(OBJ_DIR)/app/studio_session.o \
		$(RAYLIB_LIBS) $(FRAMEWORKS) -lm

$(SECONDARIES_TEST_TARGET): tests/test_secondaries.c $(GAME_CORE_OBJECTS)
	$(CC) $(CFLAGS) $(RAYLIB_FLAGS) $(PROJECT_INCLUDES) -o $@ tests/test_secondaries.c \
		$(GAME_CORE_OBJECTS) $(RAYLIB_LIBS) $(FRAMEWORKS) -lm

$(UI_TEST_TARGET): tests/test_ui.c $(OBJ_DIR)/ui/ui_system.o $(OBJ_DIR)/ui/ui_theme.o \
	$(OBJ_DIR)/ui/ui_icons.o $(OBJ_DIR)/ui/ui_skin.o $(OBJ_DIR)/content/content_catalog.o
	$(CC) $(CFLAGS) $(RAYLIB_FLAGS) $(PROJECT_INCLUDES) -o $@ tests/test_ui.c \
		$(OBJ_DIR)/ui/ui_system.o $(OBJ_DIR)/ui/ui_theme.o $(OBJ_DIR)/ui/ui_icons.o \
		$(OBJ_DIR)/ui/ui_skin.o \
		$(OBJ_DIR)/content/content_catalog.o $(RAYLIB_LIBS) $(FRAMEWORKS) -lm

$(CHARACTER_ANIMATION_TEST_TARGET): tests/test_character_animation.c \
		$(OBJ_DIR)/presentation/character_animation.o
	$(CC) $(CFLAGS) $(RAYLIB_FLAGS) $(PROJECT_INCLUDES) -o $@ \
		tests/test_character_animation.c $(OBJ_DIR)/presentation/character_animation.o \
		$(RAYLIB_LIBS) $(FRAMEWORKS) -lm

$(VFX_TEST_TARGET): tests/test_vfx.c $(VFX_TEST_OBJECTS)
	$(CC) $(CFLAGS) $(RAYLIB_FLAGS) $(PROJECT_INCLUDES) -o $@ tests/test_vfx.c \
		$(VFX_TEST_OBJECTS) $(RAYLIB_LIBS) $(FRAMEWORKS) -lm

$(VFX_EVENTS_TEST_TARGET): tests/test_vfx_events.c $(GAME_CORE_OBJECTS)
	$(CC) $(CFLAGS) $(RAYLIB_FLAGS) $(PROJECT_INCLUDES) -o $@ tests/test_vfx_events.c \
		$(GAME_CORE_OBJECTS) $(RAYLIB_LIBS) $(FRAMEWORKS) -lm

$(CAMERA_TEST_TARGET): tests/test_camera.c $(OBJ_DIR)/presentation/camera.o \
		$(OBJ_DIR)/game/arena.o $(OBJ_DIR)/content/map_content.o
	$(CC) $(CFLAGS) $(RAYLIB_FLAGS) $(PROJECT_INCLUDES) -o $@ tests/test_camera.c \
		$(OBJ_DIR)/presentation/camera.o $(OBJ_DIR)/game/arena.o \
		$(OBJ_DIR)/content/map_content.o $(RAYLIB_LIBS) $(FRAMEWORKS) -lm

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -MMD -MP $(RAYLIB_FLAGS) $(PROJECT_INCLUDES) -c $< -o $@

-include $(DEPFILES)

$(CHARACTER_ASSET_STAMP): $(CHARACTER_ASSET_INPUTS) $(CHARACTER_PIPELINE_SOURCES)
	python3 tools/build_character_assets.py --manifest $(CHARACTER_MANIFEST)
	touch $@

character-assets: $(CHARACTER_ASSET_STAMP)

$(VFX_ASSET_STAMP): $(VFX_ASSET_INPUTS) $(VFX_PIPELINE_SOURCES)
	python3 tools/build_vfx_assets.py --manifest $(VFX_MANIFEST)
	touch $@

vfx-assets: $(VFX_ASSET_STAMP)

run: check-character-assets check-vfx-assets $(TARGET)
	./$(TARGET)

debug:
	$(MAKE) OBJ_FLAVOR=-debug CFLAGS="-Wall -Wextra -std=c99 $(DEBUG_FLAGS)" \
		check-ui check-character-assets check-vfx-assets $(BUILD_DIR)/brawl_arena-debug

sanitize:
	$(MAKE) OBJ_FLAVOR=-sanitize CFLAGS="-Wall -Wextra -std=c99 $(SANITIZE_FLAGS)" \
		sanitize-run

sanitize-run: check-architecture check-ui check-character-assets check-vfx-assets $(TEST_TARGET) $(HEALER_TEST_TARGET) $(NO_SHAKE_TEST_TARGET) $(ARENA_TEST_TARGET) $(GAMEPLAY_TEST_TARGET) $(REGEN_TEST_TARGET) $(TANK_TEST_TARGET) $(LONGSHOT_TEST_TARGET) $(SCRAPPER_TEST_TARGET) $(SECONDARIES_TEST_TARGET) $(STUDIO_TEST_TARGET) $(UI_TEST_TARGET) $(CHARACTER_ANIMATION_TEST_TARGET) $(VFX_TEST_TARGET) $(VFX_EVENTS_TEST_TARGET) $(CAMERA_TEST_TARGET)
	python3 tests/test_character_pipeline.py
	python3 tests/test_vfx_pipeline.py
	$(SANITIZER_ENV) ./$(TEST_TARGET)
	$(SANITIZER_ENV) ./$(HEALER_TEST_TARGET)
	$(SANITIZER_ENV) ./$(NO_SHAKE_TEST_TARGET)
	$(SANITIZER_ENV) ./$(ARENA_TEST_TARGET)
	$(SANITIZER_ENV) ./$(GAMEPLAY_TEST_TARGET)
	$(SANITIZER_ENV) ./$(REGEN_TEST_TARGET)
	$(SANITIZER_ENV) ./$(TANK_TEST_TARGET)
	$(SANITIZER_ENV) ./$(LONGSHOT_TEST_TARGET)
	$(SANITIZER_ENV) ./$(SCRAPPER_TEST_TARGET)
	$(SANITIZER_ENV) ./$(SECONDARIES_TEST_TARGET)
	$(SANITIZER_ENV) ./$(STUDIO_TEST_TARGET)
	$(SANITIZER_ENV) ./$(UI_TEST_TARGET)
	$(SANITIZER_ENV) ./$(CHARACTER_ANIMATION_TEST_TARGET)
	$(SANITIZER_ENV) ./$(VFX_TEST_TARGET)
	$(SANITIZER_ENV) ./$(VFX_EVENTS_TEST_TARGET)
	$(SANITIZER_ENV) ./$(CAMERA_TEST_TARGET)

clean:
	rm -rf $(BUILD_DIR)

rebuild: clean all

test: check-architecture check-ui check-character-assets check-vfx-assets $(TEST_TARGET) $(HEALER_TEST_TARGET) $(NO_SHAKE_TEST_TARGET) $(ARENA_TEST_TARGET) $(GAMEPLAY_TEST_TARGET) $(REGEN_TEST_TARGET) $(TANK_TEST_TARGET) $(LONGSHOT_TEST_TARGET) $(SCRAPPER_TEST_TARGET) $(SECONDARIES_TEST_TARGET) $(STUDIO_TEST_TARGET) $(UI_TEST_TARGET) $(CHARACTER_ANIMATION_TEST_TARGET) $(VFX_TEST_TARGET) $(VFX_EVENTS_TEST_TARGET) $(CAMERA_TEST_TARGET)
	python3 tests/test_character_pipeline.py
	python3 tests/test_vfx_pipeline.py
	./$(TEST_TARGET)
	./$(HEALER_TEST_TARGET)
	./$(NO_SHAKE_TEST_TARGET)
	./$(ARENA_TEST_TARGET)
	./$(GAMEPLAY_TEST_TARGET)
	./$(REGEN_TEST_TARGET)
	./$(TANK_TEST_TARGET)
	./$(LONGSHOT_TEST_TARGET)
	./$(SCRAPPER_TEST_TARGET)
	./$(SECONDARIES_TEST_TARGET)
	./$(STUDIO_TEST_TARGET)
	./$(UI_TEST_TARGET)
	./$(CHARACTER_ANIMATION_TEST_TARGET)
	./$(VFX_TEST_TARGET)
	./$(VFX_EVENTS_TEST_TARGET)
	./$(CAMERA_TEST_TARGET)

validate-config: $(TEST_TARGET)
	./$(TEST_TARGET) --validate config/gameplay.cfg

check-architecture:
	./tools/check_architecture.sh

check-ui:
	sh tools/check_ui.sh

ui-assets:
	python3 tools/build_ui_assets.py

check-character-assets: character-assets
	python3 tools/check_character_assets.py $(CHARACTER_MANIFEST)

check-vfx-assets: vfx-assets
	python3 tools/check_vfx_assets.py $(VFX_MANIFEST)

help:
	@echo "Brawl Arena"
	@echo "  make        - build"
	@echo "  make run    - build and play"
	@echo "  make debug  - build with debug symbols"
	@echo "  make test   - run headless config and combat tests"
	@echo "  make sanitize - run headless tests under ASan/UBSan"
	@echo "  make check-architecture - enforce source dependency boundaries"
	@echo "  make check-ui - enforce Helios UI ownership and resource policy"
	@echo "  make ui-assets - rebuild tracked tintable UI motifs from source sheets"
	@echo "  make character-assets - bake shared animations into runtime GLBs"
	@echo "  make check-character-assets - validate model, animation, and 1K texture contracts"
	@echo "  make vfx-assets - build the curated ability-VFX atlases"
	@echo "  make check-vfx-assets - validate VFX atlas and license metadata"
	@echo "  make validate-config - validate tracked project defaults"
	@echo "  make clean  - remove build artifacts"

.PHONY: all run debug sanitize sanitize-run clean rebuild test validate-config check-architecture check-ui ui-assets character-assets check-character-assets vfx-assets check-vfx-assets help
