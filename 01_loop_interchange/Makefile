CC      = gcc
CFLAGS  = -O2 -g -Wall -Wextra

PERF_EVENTS = cycles,instructions,cache-references,cache-misses

TARGET_BAD      = loop_bad
TARGET_COMPILER = loop_compiler
TARGET_GOOD     = loop_good

SRC = loop_interchange.c

.PHONY: all bad compiler good run perf perf-bad perf-compiler perf-good clean

all: bad compiler good

# 1. Original cache-unfriendly implementation
bad:
	$(CC) $(CFLAGS) \
		-fno-loop-interchange \
		-DCOLUMN_MAJOR \
		$(SRC) -o $(TARGET_BAD)

# 2. Same bad source, but GCC is allowed to perform loop interchange
compiler:
	$(CC) $(CFLAGS) \
		-floop-interchange \
		-fopt-info-loop-optimized \
		-DCOLUMN_MAJOR \
		$(SRC) -o $(TARGET_COMPILER)

# 3. Manually optimized row-major implementation
good:
	$(CC) $(CFLAGS) \
		-DROW_MAJOR \
		$(SRC) -o $(TARGET_GOOD)

run: all
	@echo "========================================"
	@echo "1. Column-major: original bad version"
	@echo "========================================"
	./$(TARGET_BAD)
	@echo

	@echo "========================================"
	@echo "2. Column-major + compiler optimization"
	@echo "========================================"
	./$(TARGET_COMPILER)
	@echo

	@echo "========================================"
	@echo "3. Row-major: manually optimized"
	@echo "========================================"
	./$(TARGET_GOOD)
	@echo

perf-bad: bad
	@echo "========================================"
	@echo "PERF: Column-major / bad"
	@echo "========================================"
	perf stat -e $(PERF_EVENTS) ./$(TARGET_BAD)

perf-compiler: compiler
	@echo "========================================"
	@echo "PERF: Column-major + -floop-interchange"
	@echo "========================================"
	perf stat -e $(PERF_EVENTS) ./$(TARGET_COMPILER)

perf-good: good
	@echo "========================================"
	@echo "PERF: Row-major / manually optimized"
	@echo "========================================"
	perf stat -e $(PERF_EVENTS) ./$(TARGET_GOOD)

perf: all
	@echo
	@echo "========================================"
	@echo "1. PERF: BAD - column-major"
	@echo "========================================"
	perf stat -e $(PERF_EVENTS) ./$(TARGET_BAD)
	@echo

	@echo "========================================"
	@echo "2. PERF: COMPILER - column-major"
	@echo "   with -floop-interchange"
	@echo "========================================"
	perf stat -e $(PERF_EVENTS) ./$(TARGET_COMPILER)
	@echo

	@echo "========================================"
	@echo "3. PERF: GOOD - row-major"
	@echo "========================================"
	perf stat -e $(PERF_EVENTS) ./$(TARGET_GOOD)
	@echo

clean:
	rm -f $(TARGET_BAD) $(TARGET_COMPILER) $(TARGET_GOOD)
	rm -f perf.data perf.data.old
