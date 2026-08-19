#include <sycl/sycl.hpp>

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace sycl = sycl;

static void fill_matrix(std::vector<float>& m, int n, float scale) {
    for (int i = 0; i < n * n; ++i) {
        m[i] = scale * static_cast<float>((i % 251) - 125) / 125.0f;
    }
}

int main(int argc, char** argv) {
    const int n = (argc > 1) ? std::atoi(argv[1]) : 1024;
    const int repeats = (argc > 2) ? std::atoi(argv[2]) : 8;

    std::vector<float> A(n * n), B(n * n), C(n * n, 0.0f);

    fill_matrix(A, n, 0.5f);
    fill_matrix(B, n, 0.25f);

    try {
        sycl::queue q(sycl::gpu_selector_v, sycl::property::queue::in_order{});

        std::cout << "Running BAD version on: "
                  << q.get_device().get_info<sycl::info::device::name>() << "\n";
        std::cout << "Matrix: " << n << "x" << n
                  << ", repeats: " << repeats << "\n";

        float* dA = sycl::malloc_device<float>(n * n, q);
        float* dB = sycl::malloc_device<float>(n * n, q);
        float* dC = sycl::malloc_device<float>(n * n, q);

        q.memcpy(dA, A.data(), sizeof(float) * n * n).wait();
        q.memcpy(dB, B.data(), sizeof(float) * n * n).wait();
        q.memset(dC, 0, sizeof(float) * n * n).wait();

        auto start = std::chrono::high_resolution_clock::now();

        for (int r = 0; r < repeats; ++r) {
            /*
             * INTENTIONALLY BAD GPU DESIGN
             *
             * One kernel is launched for every matrix row.
             *
             * For n = 1024 and repeats = 8, this creates 8192 kernel
             * submissions. Each individual kernel exposes only n work-items,
             * so the GPU repeatedly pays submission/scheduling overhead and
             * has less work available per launch.
             *
             * VTune GPU Offload should make this behavior visible as many
             * short compute tasks and gaps/overhead around GPU execution.
             */
            for (int row = 0; row < n; ++row) {
                q.parallel_for(
                    sycl::range<1>(static_cast<size_t>(n)),
                    [=](sycl::id<1> idx) {
                        const int col = static_cast<int>(idx[0]);
                        float sum = 0.0f;

                        for (int k = 0; k < n; ++k) {
                            sum += dA[row * n + k] * dB[k * n + col];
                        }

                        dC[row * n + col] =
                            sum + static_cast<float>(r) * 0.000001f;
                    });
            }
        }

        q.wait();

        auto end = std::chrono::high_resolution_clock::now();
        double seconds = std::chrono::duration<double>(end - start).count();

        q.memcpy(C.data(), dC, sizeof(float) * n * n).wait();

        double checksum = 0.0;
        const int step = std::max(1, n * n / 16);
        for (int i = 0; i < n * n; i += step) {
            checksum += C[i];
        }

        const double flops = 2.0 * n * n * n * repeats;

        std::cout << "Elapsed kernel region: " << seconds << " s\n";
        std::cout << "Approx throughput: "
                  << (flops / seconds / 1.0e12) << " TFLOP/s\n";
        std::cout << "Checksum: " << checksum << "\n";

        sycl::free(dA, q);
        sycl::free(dB, q);
        sycl::free(dC, q);
    } catch (const sycl::exception& e) {
        std::cerr << "SYCL exception: " << e.what() << "\n";
        std::cerr << "Confirm Intel GPU drivers and oneAPI DPC++ are installed.\n";
        return 2;
    }

    return 0;
}
