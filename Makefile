CC=gcc
FLAGS=-g -Wall -pedantic -std=c99 -Wextra

kilo: kilo.c
	$(CC) kilo.c $(FLAGS) -o kilo

clean:
	rm -f kilo