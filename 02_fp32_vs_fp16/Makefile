CC        = gcc
CFLAGS    = -Wall -Wextra
SRC       = fp_precision.c

# FP64 baseline – no vectorisation
TARGET_FP64  = fp64_scalar
# FP32 – compiler may auto-vectorise with 256-bit ymm registers
TARGET_FP32  = fp32_scalar
# FP32 with aggressive vectorisation + fast-math
TARGET_FP32V = fp32_vector

.PHONY: all fp64 fp32 fp32v clean run

all: fp64 fp32 fp32v

fp64: $(SRC)
	$(CC) $(CFLAGS) -O2 -o $(TARGET_FP64) $(SRC) -lm

fp32: $(SRC)
	$(CC) $(CFLAGS) -O2 -o $(TARGET_FP32) $(SRC) -lm

fp32v: $(SRC)
	$(CC) $(CFLAGS) -O3 -march=native -ffast-math -funroll-loops \
	    -ftree-vectorize -o $(TARGET_FP32V) $(SRC) -lm

run: all
	@echo "=== FP64 scalar (-O2) ==="
	./$(TARGET_FP64)
	@echo ""
	@echo "=== FP32 scalar (-O2) ==="
	./$(TARGET_FP32)
	@echo ""
	@echo "=== FP32 vectorised (-O3 -march=native -ffast-math) ==="
	./$(TARGET_FP32V)

# Show disassembly of the hot loop for each binary
asm:
	@echo "--- FP64 ---"
	objdump -d $(TARGET_FP64)  | awk '/dot_fp64/,/^$$/' | head -60
	@echo "--- FP32 vectorised ---"
	objdump -d $(TARGET_FP32V) | awk '/dot_fp32/,/^$$/' | head -60

clean:
	rm -f $(TARGET_FP64) $(TARGET_FP32) $(TARGET_FP32V) \
	    perf.data perf.data.old out.perf out.folded flamegraph.svg
