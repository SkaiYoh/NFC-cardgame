.DEFAULT_GOAL := cardgame

.PHONY: clean run init-db preview tools test sprite-frame-atlas

CC ?= gcc
PYTHON ?= python3
CFLAGS ?= -Wall -Wextra -O2
CPPFLAGS += -Ithird_party/cjson
MACFLAGS ?= -I/opt/homebrew/include -L/opt/homebrew/lib

RAYLIB_LDLIBS = -lraylib
SQLITE_LDLIBS = -lsqlite3
MATH_LDLIBS = -lm

# Source files
SRC_CORE = src/core/game.c src/core/battlefield.c src/core/battlefield_math.c src/core/debug_events.c src/core/sustenance.c
SRC_DATA = src/data/db.c src/data/cards.c
SRC_RENDERING = src/rendering/card_renderer.c src/rendering/tilemap_renderer.c src/rendering/viewport.c src/rendering/sprite_renderer.c src/rendering/spawn_fx.c src/rendering/status_bars.c src/rendering/biome.c src/rendering/ui.c src/rendering/debug_overlay.c src/rendering/debug_overlay_input.c src/rendering/sustenance_renderer.c src/rendering/hand_ui.c src/rendering/uvulite_font.c
SRC_ENTITIES = src/entities/entities.c src/entities/entity_animation.c src/entities/troop.c src/entities/building.c src/entities/projectile.c
SRC_SYSTEMS = src/systems/player.c src/systems/audio.c src/systems/energy.c src/systems/spawn.c src/systems/spawn_placement.c src/systems/match.c src/systems/progression.c
SRC_LOGIC = src/logic/card_effects.c src/logic/combat.c src/logic/deposit_slots.c src/logic/farmer.c src/logic/nav_frame.c src/logic/pathfinding.c src/logic/win_condition.c
SRC_HARDWARE = src/hardware/nfc_reader.c src/hardware/arduino_protocol.c
SRC_LIB = third_party/cjson/cJSON.c

GAME_SOURCES = $(SRC_CORE) $(SRC_DATA) $(SRC_RENDERING) $(SRC_ENTITIES) $(SRC_SYSTEMS) $(SRC_LOGIC) $(SRC_HARDWARE) $(SRC_LIB)
CARD_PREVIEW_SOURCES = tools/card_preview.c tools/card_psd_export.c src/rendering/card_renderer.c $(SRC_LIB)
BIOME_PREVIEW_SOURCES = tools/biome_preview.c src/rendering/biome.c src/rendering/tilemap_renderer.c
CARD_ENROLL_SOURCES = tools/card_enroll.c src/data/db.c src/data/cards.c src/hardware/nfc_reader.c src/hardware/arduino_protocol.c

TOOL_BINS = card_preview biome_preview card_enroll
TEST_SOURCES := $(wildcard tests/test_*.c)
TEST_BINS := $(sort $(patsubst tests/%.c,%,$(TEST_SOURCES)))
STALE_BINS = test_assault_slots test_ore
CLEAN_BINS = cardgame $(TOOL_BINS) $(TEST_BINS) $(STALE_BINS)

cardgame: $(GAME_SOURCES)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(GAME_SOURCES) -o $@ $(MACFLAGS) $(SQLITE_LDLIBS) $(RAYLIB_LDLIBS) $(MATH_LDLIBS)

card_preview: $(CARD_PREVIEW_SOURCES)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(CARD_PREVIEW_SOURCES) -o $@ $(MACFLAGS) $(RAYLIB_LDLIBS) $(MATH_LDLIBS)

biome_preview: $(BIOME_PREVIEW_SOURCES)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(BIOME_PREVIEW_SOURCES) -o $@ $(MACFLAGS) $(RAYLIB_LDLIBS) $(MATH_LDLIBS)

card_enroll: $(CARD_ENROLL_SOURCES)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(CARD_ENROLL_SOURCES) -o $@ $(MACFLAGS) $(SQLITE_LDLIBS)

preview: card_preview

tools: $(TOOL_BINS)

$(TEST_BINS): %: tests/%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $< -o $@ $(MACFLAGS) $(MATH_LDLIBS)

test: $(TEST_BINS)
	@for test_bin in $(TEST_BINS); do \
		./$$test_bin || exit $$?; \
	done

sprite-frame-atlas:
	$(PYTHON) tools/generate_sprite_frame_atlas.py

# Initialize a fresh SQLite database from schema + seed data
init-db:
	sqlite3 cardgame.db < sqlite/schema.sql
	sqlite3 cardgame.db < sqlite/seed.sql
	@echo "cardgame.db initialized"

run: clean cardgame
	@if [ -z "$(NFC_PORT_P1)" ] || [ -z "$(NFC_PORT_P2)" ]; then \
		echo "Set NFC_PORT_P1 and NFC_PORT_P2 before running 'make run'."; \
		exit 1; \
	fi
	NFC_PORT_P1="$(NFC_PORT_P1)" NFC_PORT_P2="$(NFC_PORT_P2)" ./cardgame

clean:
	rm -f $(CLEAN_BINS)
