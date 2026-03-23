#include <iostream>
#include <chrono>
#include <cstddef>
#include "order_generator.h"

int main() {
    std::cout << "[Titanium] Generating 10 million test orders..." << std::endl;

    auto start = std::chrono::high_resolution_clock::now();
    
    std::size_t count = 10'000'000;
    auto orders = generate_dummy_orders(count);
    
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    std::cout << "Generated " << orders.size() << " orders in " << elapsed.count() << " seconds.\n";
    
    if (elapsed.count() < 1.0) {
        std::cout << "✅ Acceptance criteria met: Generated in under 1 second." << std::endl;
    } else {
        std::cout << "❌ Acceptance criteria failed: Took longer than 1 second." << std::endl;
    }
    return 0;
}