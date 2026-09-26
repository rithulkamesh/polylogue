#pragma once

#include <cstddef>
#include <cstdint>

namespace polylogue::test {

// True when the test binary counts heap allocations (not available under AddressSanitizer, which
// owns the allocator).
bool allocationTrackingAvailable();

// Counts heap allocations made on this thread while it is alive. Use it to prove a code path
// never allocates.
class AllocationGuard {
public:
    AllocationGuard();
    ~AllocationGuard();

    AllocationGuard(const AllocationGuard&) = delete;
    AllocationGuard& operator=(const AllocationGuard&) = delete;

    std::size_t count() const;
};

}  // namespace polylogue::test
