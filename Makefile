.PHONY: clean

CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lpq

cardgame: cardgame.c
	$(CC) $(CFLAGS) cardgame.c db.c -o cardgame $(LDFLAGS)

clean:
	rm -f cardgame
