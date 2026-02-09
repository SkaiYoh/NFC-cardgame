.PHONY: clean run

CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lpq -lraylib

cardgame: cardgame.c db.c cards.c tilemap.c
	$(CC) $(CFLAGS) cardgame.c db.c cards.c tilemap.c -o cardgame $(LDFLAGS)

run: clean cardgame
	DB_CONNECTION="host=localhost port=5432 dbname=appdb user=postgres password=postgres" ./cardgame

clean:
	rm -f cardgame
