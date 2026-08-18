CC       = gcc
CFLAGS   = -Wall -Wextra
LDFLAGS  = -static
SRC      = fp_precision.c

GCC_IMAGE = gcc:14.4.0

TARGET_BASE       = fp_precision_base
TARGET_AGGRESSIVE = fp_precision_aggressive
TARGET_FP16       = fp_precision_fp16

DOCKER_GCC = docker run --rm \
	-v "$(CURDIR):/work" \
	-w /work \
	$(GCC_IMAGE) gcc

.PHONY: all base aggressive fp16 run asm clean

all: base aggressive fp16

#
# Baseline:
#   GCC 11
#   -O2
#   FP64 + FP32 only
#
base: $(SRC)
	$(CC) $(CFLAGS) -O2 \
	    -o $(TARGET_BASE) $(SRC) \
	    $(LDFLAGS) -lm

#
# Aggressive:
#   GCC 11
#   FP64 + FP32
#
aggressive: $(SRC)
	$(CC) $(CFLAGS) \
	    -O3 \
	    -march=native \
	    -ffast-math \
	    -funroll-loops \
	    -ftree-vectorize \
	    -o $(TARGET_AGGRESSIVE) $(SRC) \
	    $(LDFLAGS) -lm

#
# Aggressive FP16:
#   GCC 14 container
#   FP64 + FP32 + FP16
#
fp16: $(SRC)
	$(DOCKER_GCC) $(CFLAGS) \
	    -DENABLE_FP16 \
	    -O3 \
	    -march=native \
	    -ffast-math \
	    -funroll-loops \
	    -ftree-vectorize \
	    -mavx512fp16 \
	    -o $(TARGET_FP16) $(SRC) \
	    $(LDFLAGS) -lm

run: all
	@echo "============================================================"
	@echo "BASELINE: GCC 11 -O2"
	@echo "FP64 + FP32"
	@echo "============================================================"
	./$(TARGET_BASE)

	@echo ""
	@echo "============================================================"
	@echo "AGGRESSIVE: GCC 11 -O3 -march=native -ffast-math"
	@echo "FP64 + FP32"
	@echo "============================================================"
	./$(TARGET_AGGRESSIVE)

	@echo ""
	@echo "============================================================"
	@echo "AGGRESSIVE FP16: GCC 14 -O3 -mavx512fp16"
	@echo "FP64 + FP32 + FP16"
	@echo "============================================================"
	./$(TARGET_FP16)

asm: all
	@echo "=== Baseline FP64 ==="
	objdump -d -M intel $(TARGET_BASE) \
	    | awk '/<dot_fp64>:/,/^$$/' | head -80

	@echo ""
	@echo "=== Baseline FP32 ==="
	objdump -d -M intel $(TARGET_BASE) \
	    | awk '/<dot_fp32>:/,/^$$/' | head -80

	@echo ""
	@echo "=== Aggressive FP64 ==="
	objdump -d -M intel $(TARGET_AGGRESSIVE) \
	    | awk '/<dot_fp64>:/,/^$$/' | head -80

	@echo ""
	@echo "=== Aggressive FP32 ==="
	objdump -d -M intel $(TARGET_AGGRESSIVE) \
	    | awk '/<dot_fp32>:/,/^$$/' | head -80

	@echo ""
	@echo "=== Aggressive FP16 ==="
	objdump -d -M intel $(TARGET_FP16) \
	    | awk '/<dot_fp16>:/,/^$$/' | head -80

clean:
	rm -f $(TARGET_BASE) \
	    $(TARGET_AGGRESSIVE) \
	    $(TARGET_FP16) \
	    perf.data perf.data.old \
	    out.perf out.folded flamegraph.svg
