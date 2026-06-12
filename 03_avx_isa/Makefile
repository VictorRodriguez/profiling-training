CC       = gcc
CFLAGS   = -Wall -Wextra
SRC      = avx_example.c

TARGET_SCALAR  = avx_scalar
TARGET_AUTOVEC = avx_autovec
TARGET_AVX2    = avx_avx2

.PHONY: all scalar autovec avx2 clean run asm

all: scalar autovec avx2

scalar: $(SRC)
	$(CC) $(CFLAGS) -O2 -o $(TARGET_SCALAR) $(SRC)

# Let the compiler decide which SIMD instructions to use
autovec: $(SRC)
	$(CC) $(CFLAGS) -O3 -march=native -ftree-vectorize \
	    -o $(TARGET_AUTOVEC) $(SRC)

# Explicit AVX2 + FMA; -mavx2 -mfma define __AVX2__ for the #ifdef guard
avx2: $(SRC)
	$(CC) $(CFLAGS) -O3 -mavx2 -mfma -o $(TARGET_AVX2) $(SRC)

run: all
	@echo "=== Scalar (-O2) ==="
	./$(TARGET_SCALAR)
	@echo ""
	@echo "=== Auto-vectorised (-O3 -march=native) ==="
	./$(TARGET_AUTOVEC)
	@echo ""
	@echo "=== Explicit AVX2 (-O3 -mavx2 -mfma) ==="
	./$(TARGET_AVX2)

# Print the generated SIMD instructions for the dot product
asm:
	@echo "--- scalar dot_scalar ---"
	objdump -d $(TARGET_SCALAR)  | awk '/<dot_scalar>/,/^$$/' | head -40
	@echo "--- auto-vec dot_scalar ---"
	objdump -d $(TARGET_AUTOVEC) | awk '/<dot_scalar>/,/^$$/' | head -40
	@echo "--- explicit AVX2 dot_avx2 ---"
	objdump -d $(TARGET_AVX2)    | awk '/<dot_avx2>/,/^$$/'   | head -40

clean:
	rm -f $(TARGET_SCALAR) $(TARGET_AUTOVEC) $(TARGET_AVX2) \
	    perf.data perf.data.old out.perf out.folded flamegraph.svg
