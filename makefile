CC = gcc
# Use all warnings; c99
CFLAGS = -Wall -Wextra -pedantic -std=c99

all: tex.o tex

tex.o: tex.c tex.h
	$(CC) $(CFLAGS) -c tex.c tex.h

tex: tex.o
	$(CC) $(CFLAGS) -o tex tex.o