.PHONY: clean run

CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lpq -lraylib

cardgame: cardgame.c db.c
	$(CC) $(CFLAGS) cardgame.c db.c -o cardgame $(LDFLAGS)

run: clean cardgame
	./cardgame

clean:
	rm -f cardgame
