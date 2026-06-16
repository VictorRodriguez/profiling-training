#!/usr/bin/env python3
"""
NPU CNN stress workload for Intel VTune + OpenVINO.

Purpose:
  - Build a synthetic CNN model directly with OpenVINO ops.
  - Compile it for CPU/GPU/NPU.
  - Run inference in a loop for a fixed duration.
  - Generate enough activity to profile with VTune NPU Exploration.

Example:
  python npu_cnn_stress.py --device NPU --seconds 60 --requests 4

VTune:
  vtune -collect npu -r vtune_npu_result -- python npu_cnn_stress.py --device NPU --seconds 60 --requests 4
"""

import argparse
import time
from typing import List

import numpy as np
import openvino as ov
from openvino import opset10 as ops


def build_cnn_model(batch: int = 1, height: int = 224, width: int = 224) -> ov.Model:
    """
    Build a simple CNN-like model:

      Input NCHW: [batch, 3, height, width]
      Conv -> ReLU -> AvgPool
      Conv -> ReLU -> AvgPool
      Conv -> ReLU
      ReduceMean over H/W
      MatMul classifier-like layer
      Softmax

    The model is synthetic but useful for exercising accelerator inference.
    """

    input_node = ops.parameter(
        [batch, 3, height, width],
        dtype=np.float32,
        name="input",
    )

    rng = np.random.default_rng(seed=123)

    # Conv 1: 3 -> 32
    w1 = rng.normal(0, 0.05, size=(32, 3, 3, 3)).astype(np.float32)
    node = ops.convolution(
        input_node,
        ops.constant(w1),
        strides=[1, 1],
        pads_begin=[1, 1],
        pads_end=[1, 1],
        dilations=[1, 1],
    )
    node = ops.relu(node)

    node = ops.avg_pool(
        node,
        strides=[2, 2],
        pads_begin=[0, 0],
        pads_end=[0, 0],
        kernel_shape=[2, 2],
        exclude_pad=False,
    )

    # Conv 2: 32 -> 64
    w2 = rng.normal(0, 0.05, size=(64, 32, 3, 3)).astype(np.float32)
    node = ops.convolution(
        node,
        ops.constant(w2),
        strides=[1, 1],
        pads_begin=[1, 1],
        pads_end=[1, 1],
        dilations=[1, 1],
    )
    node = ops.relu(node)

    node = ops.avg_pool(
        node,
        strides=[2, 2],
        pads_begin=[0, 0],
        pads_end=[0, 0],
        kernel_shape=[2, 2],
        exclude_pad=False,
    )

    # Conv 3: 64 -> 128
    w3 = rng.normal(0, 0.05, size=(128, 64, 3, 3)).astype(np.float32)
    node = ops.convolution(
        node,
        ops.constant(w3),
        strides=[1, 1],
        pads_begin=[1, 1],
        pads_end=[1, 1],
        dilations=[1, 1],
    )
    node = ops.relu(node)

    # Global average pooling over H and W.
    # Use positional args for compatibility with current OpenVINO Python API.
    axes = ops.constant(np.array([2, 3], dtype=np.int64))
    node = ops.reduce_mean(node, axes, False)

    # Classifier-like dense layer: 128 -> 1000
    w_fc = rng.normal(0, 0.05, size=(128, 1000)).astype(np.float32)
    node = ops.matmul(node, ops.constant(w_fc), False, False)

    # Softmax over class dimension.
    node = ops.softmax(node, axis=1)

    output = ops.result(node, name="output")
    model = ov.Model([output], [input_node], name="synthetic_cnn_npu_stress")

    return model


def run(args: argparse.Namespace) -> None:
    core = ov.Core()

    available_devices = core.available_devices
    print(f"Available devices: {', '.join(available_devices)}")

    if args.device not in available_devices:
        raise RuntimeError(
            f"Requested device '{args.device}' is not available. "
            f"Available devices: {available_devices}"
        )

    print(
        f"Building model: batch={args.batch}, "
        f"input=3x{args.height}x{args.width}"
    )

    model = build_cnn_model(
        batch=args.batch,
        height=args.height,
        width=args.width,
    )

    print(f"Compiling model for device: {args.device}")

    compile_config = {}

    # Keep the config minimal for portability. For real demos, you can expose
    # device-specific properties later.
    compiled_model = core.compile_model(model, args.device, compile_config)

    input_port = compiled_model.input(0)
    output_port = compiled_model.output(0)

    rng = np.random.default_rng(seed=456)
    input_data = rng.random(
        size=(args.batch, 3, args.height, args.width),
        dtype=np.float32,
    )

    print("Warming up...")
    for _ in range(args.warmup):
        result = compiled_model([input_data])[output_port]
        _ = float(np.sum(result))

    print(
        f"Running stress loop for {args.seconds} seconds "
        f"with {args.requests} infer requests..."
    )

    infer_requests = [compiled_model.create_infer_request() for _ in range(args.requests)]

    start_time = time.perf_counter()
    end_time = start_time + args.seconds

    completed_inferences = 0
    checksum = 0.0

    # Prime all requests.
    for req in infer_requests:
        req.start_async({input_port: input_data})

    while time.perf_counter() < end_time:
        for req in infer_requests:
            req.wait()
            output = req.get_output_tensor(0).data
            checksum += float(np.sum(output))
            completed_inferences += 1
            req.start_async({input_port: input_data})

    # Drain outstanding requests.
    for req in infer_requests:
        req.wait()
        output = req.get_output_tensor(0).data
        checksum += float(np.sum(output))
        completed_inferences += 1

    elapsed = time.perf_counter() - start_time
    throughput = completed_inferences / elapsed if elapsed > 0 else 0.0

    print("Done.")
    print(f"Device: {args.device}")
    print(f"Elapsed seconds: {elapsed:.3f}")
    print(f"Completed inferences: {completed_inferences}")
    print(f"Throughput: {throughput:.2f} infer/sec")
    print(f"Checksum: {checksum:.6f}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Synthetic OpenVINO CNN workload for Intel NPU/GPU/CPU profiling."
    )

    parser.add_argument(
        "--device",
        default="NPU",
        help="OpenVINO device to use: NPU, GPU, CPU, AUTO, etc.",
    )

    parser.add_argument(
        "--seconds",
        type=int,
        default=60,
        help="Duration of the stress loop in seconds.",
    )

    parser.add_argument(
        "--requests",
        type=int,
        default=4,
        help="Number of parallel asynchronous inference requests.",
    )

    parser.add_argument(
        "--batch",
        type=int,
        default=1,
        help="Input batch size.",
    )

    parser.add_argument(
        "--height",
        type=int,
        default=224,
        help="Input image height.",
    )

    parser.add_argument(
        "--width",
        type=int,
        default=224,
        help="Input image width.",
    )

    parser.add_argument(
        "--warmup",
        type=int,
        default=5,
        help="Number of synchronous warmup iterations.",
    )

    return parser.parse_args()


if __name__ == "__main__":
    run(parse_args())
