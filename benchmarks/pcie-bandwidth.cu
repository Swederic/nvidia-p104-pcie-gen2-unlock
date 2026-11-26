// SPDX-License-Identifier: MIT
// Small pinned-host-memory CUDA PCIe copy benchmark.
// Test configuration: 64 MiB, 50 transfers per direction.
#include <cuda_runtime.h>
#include <cstdio>
#include <cstdlib>

#define CHECK(call) do {                                      \
    cudaError_t e = (call);                                  \
    if (e != cudaSuccess) {                                  \
        fprintf(stderr, "%s: %s\n", #call,                   \
                cudaGetErrorString(e));                      \
        return 1;                                            \
    }                                                        \
} while (0)

int main(int argc, char** argv) {
    int device = argc > 1 ? atoi(argv[1]) : 1;
    const size_t bytes = 64ULL * 1024 * 1024;
    const int repeats = 50;

    CHECK(cudaSetDevice(device));

    cudaDeviceProp prop;
    CHECK(cudaGetDeviceProperties(&prop, device));
    printf("GPU: %s (CUDA%d)\n", prop.name, device);

    void *host = nullptr, *gpu = nullptr;
    CHECK(cudaMallocHost(&host, bytes));
    CHECK(cudaMalloc(&gpu, bytes));

    cudaEvent_t start, stop;
    CHECK(cudaEventCreate(&start));
    CHECK(cudaEventCreate(&stop));

    for (int direction = 0; direction < 2; direction++) {
        cudaMemcpyKind kind = direction == 0
            ? cudaMemcpyHostToDevice : cudaMemcpyDeviceToHost;

        void *dst = direction == 0 ? gpu : host;
        const void *src = direction == 0 ? host : gpu;

        // Warm up transfer path
        CHECK(cudaMemcpy(dst, src, bytes, kind));

        CHECK(cudaEventRecord(start));

        for (int i = 0; i < repeats; i++)
            CHECK(cudaMemcpyAsync(dst, src, bytes, kind));

        CHECK(cudaEventRecord(stop));
        CHECK(cudaEventSynchronize(stop));

        float ms;
        CHECK(cudaEventElapsedTime(&ms, start, stop));

        double gbps = (double)bytes * repeats / (ms / 1000.0) / 1e9;

        printf("%s: %.1f ms, %.3f GB/s\n",
               direction == 0 ? "Host -> GPU" : "GPU -> Host",
               ms, gbps);
    }

    CHECK(cudaEventDestroy(start));
    CHECK(cudaEventDestroy(stop));
    CHECK(cudaFree(gpu));
    CHECK(cudaFreeHost(host));
    return 0;
}
