CC     = gcc
CFLAGS = -Wall -Wextra -O2

.PHONY: all cpu_hog mem_hog clean

all: cpu_hog mem_hog

cpu_hog: cpu_hog.c
	$(CC) $(CFLAGS) -o cpu_hog cpu_hog.c -lm

mem_hog: mem_hog.c
	$(CC) $(CFLAGS) -o mem_hog mem_hog.c

clean:
	rm -f cpu_hog mem_hog
