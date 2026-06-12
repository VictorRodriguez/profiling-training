CC      = gcc
CFLAGS_BAD  = -O0 -Wall -Wextra
CFLAGS_GOOD = -O2 -Wall -Wextra
TARGET_BAD  = loop_bad
TARGET_GOOD = loop_good
SRC         = loop_interchange.c

.PHONY: all bad good clean run

all: bad good

bad: $(SRC)
	$(CC) $(CFLAGS_BAD) -o $(TARGET_BAD) $(SRC)

good: $(SRC)
	$(CC) $(CFLAGS_GOOD) -o $(TARGET_GOOD) $(SRC)

run: all
	@echo "=== Running column-major (cache-unfriendly) binary ==="
	./$(TARGET_BAD)
	@echo ""
	@echo "=== Running row-major (cache-friendly) binary ==="
	./$(TARGET_GOOD)

clean:
	rm -f $(TARGET_BAD) $(TARGET_GOOD) perf.data perf.data.old out.perf out.folded flamegraph.svg
