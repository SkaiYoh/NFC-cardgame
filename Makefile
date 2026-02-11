.PHONY: clean run preview-run

CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lpq -lraylib -lm

cardgame: cardgame.c db.c cards.c tilemap.c card_render.c cJSON.c
	$(CC) $(CFLAGS) cardgame.c db.c cards.c tilemap.c card_render.c cJSON.c -o cardgame $(LDFLAGS)

preview: card_preview.c card_render.c cJSON.c
	$(CC) $(CFLAGS) card_preview.c card_render.c cJSON.c -o card_preview -lraylib -lm

run: clean cardgame
	DB_CONNECTION="host=localhost port=5432 dbname=appdb user=postgres password=postgres" ./cardgame

preview-run: preview
	./card_preview

clean:
	rm -f cardgame card_preview
