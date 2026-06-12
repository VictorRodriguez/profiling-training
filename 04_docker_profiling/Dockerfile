# Two-stage Dockerfile
# Stage 1: compile the workloads
# Stage 2a: cpu_hog image
# Stage 2b: mem_hog image

# ── Builder ──────────────────────────────────────────────────────────
FROM debian:bookworm-slim AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
        gcc make libm-dev && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY app/ .
RUN make

# ── CPU-hog runtime image ─────────────────────────────────────────────
FROM debian:bookworm-slim AS cpu_hog
COPY --from=builder /src/cpu_hog /usr/local/bin/cpu_hog
CMD ["/usr/local/bin/cpu_hog"]

# ── Memory-hog runtime image ──────────────────────────────────────────
FROM debian:bookworm-slim AS mem_hog
COPY --from=builder /src/mem_hog /usr/local/bin/mem_hog
CMD ["/usr/local/bin/mem_hog"]
