#include "titanium/engine/risk_kernel.cuh"
#include <cuda_runtime.h>
#include <iostream>
#include <stdexcept>

// Helper macro for error checking
#define CUDA_CHECK(call) \
    do { \
        cudaError_t err = call; \
        if (err != cudaSuccess) { \
            std::cerr << "CUDA error at " << __FILE__ << ":" << __LINE__ << " code=" << err << " \"" << cudaGetErrorString(err) << "\"" << std::endl; \
            throw std::runtime_error("CUDA error"); \
        } \
    } while (0)

namespace titanium {

// Using __global__ for the device kernel
__global__ void dummy_risk_kernel_device(const Order* orders, float* results, std::size_t count) {
    std::size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < count) {
        // Dummy risk calculation: price * quantity * 0.05
        // Just demonstrating that the GPU can access the 64-byte aligned structs correctly
        results[idx] = static_cast<float>(orders[idx].price) * static_cast<float>(orders[idx].quantity) * 0.05f;
    }
}

void run_dummy_risk_check(const Order* orders, std::size_t count, float* results) {
    if (count == 0) return;

    Order* d_orders = nullptr;
    float* d_results = nullptr;

    const std::size_t orders_size = count * sizeof(Order);
    const std::size_t results_size = count * sizeof(float);

    // 1. Allocate device memory
    CUDA_CHECK(cudaMalloc(&d_orders, orders_size));
    CUDA_CHECK(cudaMalloc(&d_results, results_size));

    // 2. Copy data from host to device
    CUDA_CHECK(cudaMemcpy(d_orders, orders, orders_size, cudaMemcpyHostToDevice));

    // 3. Launch kernel
    int threads_per_block = 256;
    int blocks_per_grid = (count + threads_per_block - 1) / threads_per_block;
    
    // Launch synchronous to default stream for now
    dummy_risk_kernel_device<<<blocks_per_grid, threads_per_block>>>(d_orders, d_results, count);

    // Wait for completion (optional on default stream as Memcpy will wait, but good practice for errors)
    CUDA_CHECK(cudaDeviceSynchronize());

    // 4. Copy results back to host
    CUDA_CHECK(cudaMemcpy(results, d_results, results_size, cudaMemcpyDeviceToHost));

    // 5. Free device memory
    CUDA_CHECK(cudaFree(d_orders));
    CUDA_CHECK(cudaFree(d_results));
}

// Pinned Memory Allocators
Order* alloc_pinned_orders(std::size_t count) {
    Order* ptr = nullptr;
    CUDA_CHECK(cudaHostAlloc((void**)&ptr, count * sizeof(Order), cudaHostAllocDefault));
    return ptr;
}

void free_pinned_orders(Order* ptr) {
    CUDA_CHECK(cudaFreeHost(ptr));
}

float* alloc_pinned_results(std::size_t count) {
    float* ptr = nullptr;
    CUDA_CHECK(cudaHostAlloc((void**)&ptr, count * sizeof(float), cudaHostAllocDefault));
    return ptr;
}

void free_pinned_results(float* ptr) {
    CUDA_CHECK(cudaFreeHost(ptr));
}

// Async Risk Engine
AsyncRiskEngine::AsyncRiskEngine(std::size_t batch_size) : max_batch_size_(batch_size) {
    CUDA_CHECK(cudaStreamCreate((cudaStream_t*)&stream_));
    CUDA_CHECK(cudaMalloc(&d_orders_, batch_size * sizeof(Order)));
    CUDA_CHECK(cudaMalloc(&d_results_, batch_size * sizeof(float)));
}

AsyncRiskEngine::~AsyncRiskEngine() {
    cudaStreamDestroy((cudaStream_t)stream_);
    cudaFree(d_orders_);
    cudaFree(d_results_);
}

void AsyncRiskEngine::submit_batch(const Order* host_orders, float* host_results, std::size_t count) {
    if (count == 0 || count > max_batch_size_) return;

    size_t order_bytes = count * sizeof(Order);
    size_t result_bytes = count * sizeof(float);

    cudaStream_t s = (cudaStream_t)stream_;

    CUDA_CHECK(cudaMemcpyAsync(d_orders_, host_orders, order_bytes, cudaMemcpyHostToDevice, s));

    int threads_per_block = 256;
    int blocks_per_grid = (count + threads_per_block - 1) / threads_per_block;
    dummy_risk_kernel_device<<<blocks_per_grid, threads_per_block, 0, s>>>(d_orders_, d_results_, count);

    CUDA_CHECK(cudaMemcpyAsync(host_results, d_results_, result_bytes, cudaMemcpyDeviceToHost, s));
}

void AsyncRiskEngine::synchronize() {
    CUDA_CHECK(cudaStreamSynchronize((cudaStream_t)stream_));
}

} // namespace titanium
