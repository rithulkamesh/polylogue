#include "support/AllocationGuard.h"

#include <cstdlib>
#include <new>

#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define POLYLOGUE_ASAN 1
#endif
#endif
#if defined(__SANITIZE_ADDRESS__)
#define POLYLOGUE_ASAN 1
#endif

namespace polylogue::test {
namespace {

thread_local bool tracking = false;
thread_local std::size_t allocations = 0;

}  // namespace

#ifdef POLYLOGUE_ASAN
bool allocationTrackingAvailable()
{
    return false;
}
#else
bool allocationTrackingAvailable()
{
    return true;
}
#endif

AllocationGuard::AllocationGuard()
{
    allocations = 0;
    tracking = true;
}

AllocationGuard::~AllocationGuard()
{
    tracking = false;
}

std::size_t AllocationGuard::count() const
{
    return allocations;
}

}  // namespace polylogue::test

#ifndef POLYLOGUE_ASAN

void* operator new(std::size_t size)
{
    if (polylogue::test::tracking)
        ++polylogue::test::allocations;
    if (void* pointer = std::malloc(size))
        return pointer;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size)
{
    return operator new(size);
}

void operator delete(void* pointer) noexcept
{
    std::free(pointer);
}

void operator delete[](void* pointer) noexcept
{
    std::free(pointer);
}

void operator delete(void* pointer, std::size_t) noexcept
{
    std::free(pointer);
}

void operator delete[](void* pointer, std::size_t) noexcept
{
    std::free(pointer);
}

#endif
