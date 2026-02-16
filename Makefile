.PHONY: clean run preview-run

CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lpq -lraylib -lm
MACFLAGS = -I/opt/homebrew/opt/libpq/include -L/opt/homebrew/opt/libpq/lib -I/opt/homebrew/include -L/opt/homebrew/lib

# Source files
SRC_CORE = src/core/game.c
SRC_DATA = src/data/db.c src/data/cards.c
SRC_RENDERING = src/rendering/card_renderer.c src/rendering/tilemap_renderer.c src/rendering/viewport.c src/rendering/sprite_renderer.c
SRC_SYSTEMS = src/systems/player.c
SRC_LOGIC = src/logic/card_effects.c
SRC_LIB = lib/cJSON.c

SOURCES = $(SRC_CORE) $(SRC_DATA) $(SRC_RENDERING) $(SRC_SYSTEMS) $(SRC_LOGIC) $(SRC_LIB)

cardgame: $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) -o cardgame $(MACFLAGS) $(LDFLAGS)

preview: tools/card_preview.c src/rendering/card_renderer.c lib/cJSON.c
	$(CC) $(CFLAGS) tools/card_preview.c src/rendering/card_renderer.c lib/cJSON.c -o card_preview $(MACFLAGS) -lraylib -lm

run: clean cardgame
	DB_CONNECTION="host=localhost port=5432 dbname=appdb user=postgres password=postgres" ./cardgame

preview-run: preview
	./card_preview

clean:
	rm -f cardgame card_preview
