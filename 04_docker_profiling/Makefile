# Build the containerised workloads
# Usage:
#   make build-cpu   – build the CPU-hog image
#   make build-mem   – build the memory-hog image
#   make run-cpu     – run CPU-hog container (background)
#   make run-mem     – run memory-hog container (background)
#   make stop        – stop all profiling containers
#   make clean       – remove images

IMAGE_CPU = profiling-cpu-hog
IMAGE_MEM = profiling-mem-hog

.PHONY: all build-cpu build-mem run-cpu run-mem stop clean

all: build-cpu build-mem

build-cpu:
	docker build --target cpu_hog -t $(IMAGE_CPU) .

build-mem:
	docker build --target mem_hog -t $(IMAGE_MEM) .

run-cpu:
	docker run -d --name cpu_hog_container \
	    --cap-drop ALL \
	    $(IMAGE_CPU)

run-mem:
	docker run -d --name mem_hog_container \
	    --cap-drop ALL \
	    --memory=1g \
	    $(IMAGE_MEM)

stop:
	-docker stop cpu_hog_container mem_hog_container
	-docker rm   cpu_hog_container mem_hog_container

clean: stop
	-docker rmi $(IMAGE_CPU) $(IMAGE_MEM)
