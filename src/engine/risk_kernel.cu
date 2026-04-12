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
        // Heavy Dummy Risk Simulation (from Codebase B)
        float p = static_cast<float>(orders[idx].price);
        float q = static_cast<float>(orders[idx].quantity);
        float v = 0.05f;
        
        for (int i = 0; i < 50; ++i) {
            p = p * cosf(v) + sinf(p) * 0.01f;
            q = q * expf(-v) + 1.0f;
            v += 0.001f;
        }

        results[idx] = p * q * v;
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

// Host Memory Registration for zero-copy async transfers (from Codebase B)
void register_host_memory(void* ptr, std::size_t size) {
    CUDA_CHECK(cudaHostRegister(ptr, size, cudaHostRegisterDefault));
}

void unregister_host_memory(void* ptr) {
    CUDA_CHECK(cudaHostUnregister(ptr));
}

// Pinned Memory Allocators
Order* alloc_pinned_orders(std::size_t count) {
    Order* ptr = nullptr;
    CUDA_CHECK(cudaMallocHost((void**)&ptr, count * sizeof(Order)));
    return ptr;
}

void free_pinned_orders(Order* ptr) {
    CUDA_CHECK(cudaFreeHost(ptr));
}

float* alloc_pinned_results(std::size_t count) {
    float* ptr = nullptr;
    CUDA_CHECK(cudaMallocHost((void**)&ptr, count * sizeof(float)));
    return ptr;
}

void free_pinned_results(float* ptr) {
    CUDA_CHECK(cudaFreeHost(ptr));
}

// Async Risk Engine
AsyncRiskEngine::AsyncRiskEngine(std::size_t batch_size)
    : max_batch_size_(batch_size) {
    cudaSetDeviceFlags(cudaDeviceScheduleSpin);
    CUDA_CHECK(cudaStreamCreate((cudaStream_t*)&stream_));
    
    for (auto& slot : graph_slots_) {
        CUDA_CHECK(cudaEventCreateWithFlags((cudaEvent_t*)&slot.event, cudaEventDisableTiming));
    }

    // Allocate device memory once
    CUDA_CHECK(cudaMalloc(&d_orders_, batch_size * sizeof(Order) * GRAPH_SLOT_COUNT));
    CUDA_CHECK(cudaMalloc(&d_results_, batch_size * sizeof(float) * GRAPH_SLOT_COUNT));
}

AsyncRiskEngine::~AsyncRiskEngine() {
    for (auto& slot : graph_slots_) {
        if (slot.graph_exec) cudaGraphExecDestroy((cudaGraphExec_t)slot.graph_exec);
        if (slot.graph) cudaGraphDestroy((cudaGraph_t)slot.graph);
        if (slot.event) cudaEventDestroy((cudaEvent_t)slot.event);
    }
    cudaStreamDestroy((cudaStream_t)stream_);
    cudaFree(d_orders_);
    cudaFree(d_results_);
}

void AsyncRiskEngine::submit_batch(const Order* host_orders, float* host_results, std::size_t count) {
    if (count == 0 || count > max_batch_size_) return;
    
    int idx = 0;
    for (int i = 0; i < GRAPH_SLOT_COUNT; ++i) {
        if (graph_slots_[i].host_orders == host_orders) {
            idx = i;
            break;
        }
        if (graph_slots_[i].host_orders == nullptr) {
            graph_slots_[i].host_orders = host_orders;
            idx = i;
            break;
        }
    }

    auto& slot = graph_slots_[idx];
    cudaStream_t s = (cudaStream_t)stream_;
    
    Order* d_ord = d_orders_ + (idx * max_batch_size_);
    float* d_res = d_results_ + (idx * max_batch_size_);

    size_t order_bytes = count * sizeof(Order);
    size_t result_bytes = count * sizeof(float);

    CUDA_CHECK(cudaMemcpyAsync(d_ord, host_orders, order_bytes, cudaMemcpyHostToDevice, s));

    if (!slot.is_captured || slot.count != count) {
        if (slot.graph_exec) cudaGraphExecDestroy((cudaGraphExec_t)slot.graph_exec);
        if (slot.graph) cudaGraphDestroy((cudaGraph_t)slot.graph);

        CUDA_CHECK(cudaStreamBeginCapture(s, cudaStreamCaptureModeGlobal));
        
        int threads_per_block = 256;
        int blocks_per_grid = static_cast<int>((count + threads_per_block - 1) / threads_per_block);
        dummy_risk_kernel_device<<<blocks_per_grid, threads_per_block, 0, s>>>(d_ord, d_res, count);
        
        CUDA_CHECK(cudaStreamEndCapture(s, (cudaGraph_t*)&slot.graph));
        CUDA_CHECK(cudaGraphInstantiate((cudaGraphExec_t*)&slot.graph_exec, (cudaGraph_t)slot.graph, NULL, NULL, 0));
        
        slot.is_captured = true;
        slot.count = count;
    }

    CUDA_CHECK(cudaGraphLaunch((cudaGraphExec_t)slot.graph_exec, s));
    CUDA_CHECK(cudaMemcpyAsync(host_results, d_res, result_bytes, cudaMemcpyDeviceToHost, s));
    
    // Record completion event
    CUDA_CHECK(cudaEventRecord((cudaEvent_t)slot.event, s));
}

void AsyncRiskEngine::synchronize(int slot_idx) {
    if (slot_idx == -1) {
        for (auto& slot : graph_slots_) {
            if (slot.event) {
                cudaEvent_t e = (cudaEvent_t)slot.event;
                cudaError_t status;
                while ((status = cudaEventQuery(e)) == cudaErrorNotReady) {
#if defined(_MSC_VER)
                    _mm_pause();
#else
                    __builtin_ia32_pause();
#endif
                }
            }
        }
    } else {
        auto& slot = graph_slots_[slot_idx];
        if (slot.event) {
            cudaEvent_t e = (cudaEvent_t)slot.event;
            cudaError_t status;
            while ((status = cudaEventQuery(e)) == cudaErrorNotReady) {
#if defined(_MSC_VER)
                _mm_pause();
#else
                __builtin_ia32_pause();
#endif
            }
        }
    }
}

} // namespace titanium
