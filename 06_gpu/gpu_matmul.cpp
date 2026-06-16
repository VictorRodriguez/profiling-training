#include <sycl/sycl.hpp>
#include <chrono>
#include <cmath>
#include <iostream>
#include <vector>
#include <cstdlib>

namespace sycl = sycl;

static void fill_matrix(std::vector<float>& m, int n, float scale) {
    for (int i = 0; i < n * n; ++i) {
        m[i] = scale * static_cast<float>((i % 251) - 125) / 125.0f;
    }
}

int main(int argc, char** argv) {
    const int n = (argc > 1) ? std::atoi(argv[1]) : 2048;
    const int repeats = (argc > 2) ? std::atoi(argv[2]) : 40;
    const int tile = 16;

    if (n % tile != 0) {
        std::cerr << "Matrix size must be divisible by " << tile << "\n";
        return 1;
    }

    std::vector<float> A(n * n), B(n * n), C(n * n, 0.0f);
    fill_matrix(A, n, 0.5f);
    fill_matrix(B, n, 0.25f);

    try {
        sycl::queue q(sycl::gpu_selector_v, sycl::property::queue::in_order{});
        std::cout << "Running on: "
                  << q.get_device().get_info<sycl::info::device::name>() << "\n";
        std::cout << "Matrix: " << n << "x" << n << ", repeats: " << repeats << "\n";

        float* dA = sycl::malloc_device<float>(n * n, q);
        float* dB = sycl::malloc_device<float>(n * n, q);
        float* dC = sycl::malloc_device<float>(n * n, q);

        q.memcpy(dA, A.data(), sizeof(float) * n * n).wait();
        q.memcpy(dB, B.data(), sizeof(float) * n * n).wait();
        q.memset(dC, 0, sizeof(float) * n * n).wait();

        auto start = std::chrono::high_resolution_clock::now();

        for (int r = 0; r < repeats; ++r) {
            q.submit([&](sycl::handler& h) {
                sycl::local_accessor<float, 2> Asub({tile, tile}, h);
                sycl::local_accessor<float, 2> Bsub({tile, tile}, h);

                h.parallel_for<class matrix_multiply_kernel>(
                    sycl::nd_range<2>({static_cast<size_t>(n), static_cast<size_t>(n)},
                                      {static_cast<size_t>(tile), static_cast<size_t>(tile)}),
                    [=](sycl::nd_item<2> it) {
                        const int row = static_cast<int>(it.get_global_id(0));
                        const int col = static_cast<int>(it.get_global_id(1));
                        const int local_row = static_cast<int>(it.get_local_id(0));
                        const int local_col = static_cast<int>(it.get_local_id(1));

                        float sum = 0.0f;
                        for (int block = 0; block < n; block += tile) {
                            Asub[local_row][local_col] = dA[row * n + block + local_col];
                            Bsub[local_row][local_col] = dB[(block + local_row) * n + col];
                            it.barrier(sycl::access::fence_space::local_space);

                            #pragma unroll
                            for (int k = 0; k < tile; ++k) {
                                sum += Asub[local_row][k] * Bsub[k][local_col];
                            }
                            it.barrier(sycl::access::fence_space::local_space);
                        }
                        dC[row * n + col] = sum + static_cast<float>(r) * 0.000001f;
                    });
            });
        }
        q.wait();

        auto end = std::chrono::high_resolution_clock::now();
        double seconds = std::chrono::duration<double>(end - start).count();
        q.memcpy(C.data(), dC, sizeof(float) * n * n).wait();

        double checksum = 0.0;
        for (int i = 0; i < n * n; i += n * n / 16) checksum += C[i];

        double flops = 2.0 * n * n * n * repeats;
        std::cout << "Elapsed kernel time: " << seconds << " s\n";
        std::cout << "Approx throughput: " << (flops / seconds / 1.0e12) << " TFLOP/s\n";
        std::cout << "Checksum: " << checksum << "\n";

        sycl::free(dA, q);
        sycl::free(dB, q);
        sycl::free(dC, q);
    } catch (const sycl::exception& e) {
        std::cerr << "SYCL exception: " << e.what() << "\n";
        std::cerr << "Tip: confirm Intel GPU drivers and oneAPI DPC++ are installed.\n";
        return 2;
    }

    return 0;
}
