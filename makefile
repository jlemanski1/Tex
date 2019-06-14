# Macros
CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99 # Use all warnings; c99

# Compile all
all: tex.o tex

# Compile object file
tex.o: tex.c tex.h
	$(CC) $(CFLAGS) -c tex.c tex.h

# Build & Link
tex: tex.o
	$(CC) $(CFLAGS) -o tex tex.o

# Compress directory for distribution
tar: all
	tar -zcvf Tex.tar.gz *.c *h *.md makefile

# Add to binaries
install:
	# Make tex file executable
	sudo chmod a+rx tex
	# Add to user binaries
	sudo cp tex /usr/local/bin