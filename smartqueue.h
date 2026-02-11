pragma once
#include <atomic>
#include <new>
#include <cstdint>
#include <utility>
#include <cassert>
#include <stdlib.h> 

// --- CPU OPTIMIZATIONS ---

// 1. Cache Line Size: Most modern CPUs (x64) use 64 bytes.
// We use 64 as a safe default.
constexpr size_t CACHE_LINE_SIZE = 64;

