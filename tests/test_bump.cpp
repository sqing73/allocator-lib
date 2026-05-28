#include <alloc/bump.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

static bool is_aligned(void* p, std::size_t align) {
    return (reinterpret_cast<std::uintptr_t>(p) % align) == 0;
}

TEST_CASE("BumpAllocator: basic allocaitons are aligned and non-overlapping", "[bump]") {
    alignas(16) std::byte buffer[1024];
    alloc::BumpAllocator a(buffer, sizeof(buffer));

    void* p1 = a.alloc(8, 8);
    void* p2 = a.alloc(16, 16);
    void* p3 = a.alloc(4, 4);

    REQUIRE(p1 != nullptr);
    REQUIRE(p2 != nullptr);
    REQUIRE(p3 != nullptr);

    REQUIRE(is_aligned(p1, 8));
    REQUIRE(is_aligned(p2, 16));
    REQUIRE(is_aligned(p3, 4));

    REQUIRE(p1 < p2);
    REQUIRE(p2 < p3);
}

TEST_CASE("BumpAllocator: padding is correct when realigning", "[bump]") {
    alignas(16) std::byte buffer[1024];
    alloc::BumpAllocator a(buffer, sizeof(buffer));

    // Allocate 1 byte with align 1 - leaves the bump pointer at offset 1.
    void* p1 = a.alloc(1, 1);
    REQUIRE(p1 != nullptr);
    REQUIRE(a.used() == 1);

    // Now ask for 8-aligned 8 bytes. Must skip 7 bytes of padding.
    void* p2 = a.alloc(8, 8);
    REQUIRE(p2 != nullptr);
    REQUIRE(is_aligned(p2, 8));

    // Used should be: 1 (first alloc) + 7 (padding) + 8 (second alloc) = 16
    REQUIRE(a.used() == 16);

    // The two pointers should be exactly 8 bytes apart (1 alloc + 7 padding).
    auto diff = static_cast<std::byte*>(p2) - static_cast<std::byte*>(p1);
    REQUIRE(diff == 8);
}

TEST_CASE("BumpAllocator: OOM returns nullptr and preserves state", "[bump]") {
    alignas(16) std::byte buffer[64];
    alloc::BumpAllocator a(buffer, sizeof(buffer));

    // Fill 48 bytes - succeeds.
    void* p1 = a.alloc(48, 1);
    REQUIRE(p1 != nullptr);
    REQUIRE(a.used() == 48);

    // Try to allocate 32 more - only 16 remain. Must fail.
    void* p2 = a.alloc(32, 1);
    REQUIRE(p2 == nullptr);

    // CRITICAL: the failed allocation must not have changed offset.
    REQUIRE(a.used() == 48);
    REQUIRE(a.remaining() == 16);

    // The 16 bytes that ARE left must still be used.
    void* p3 = a.alloc(16, 1);
    REQUIRE(p3 != nullptr);
    REQUIRE(a.used() == 64);
    REQUIRE(a.remaining() == 0);
}

TEST_CASE("BumpAllocator: reset reclaims the entire buffer", "[bump]") {
    alignas(16) std::byte buffer[256];
    alloc::BumpAllocator a(buffer, sizeof(buffer));

    void* first_alloc_before_reset = a.alloc(64, 16);
    REQUIRE(first_alloc_before_reset != nullptr);

    // Allocate more until we've consumed substantial space
    REQUIRE(a.alloc(64, 16) != nullptr);
    REQUIRE(a.alloc(64, 16) != nullptr);

    a.reset();
    REQUIRE(a.used() == 0);
    REQUIRE(a.remaining() == 256);

    void* first_alloc_after_reset = a.alloc(64, 16);
    REQUIRE(first_alloc_after_reset == first_alloc_before_reset);
}

TEST_CASE("BumpAllocator: many random allocations remain aligned and non-overlapping", "[bump]") {
    constexpr std::size_t kBufSize = 64 * 1024;
    alignas(64) std::byte buffer[kBufSize];

    alloc::BumpAllocator a(buffer, sizeof(buffer));

    std::mt19937 rng(42); // fixed seed - reproducible failures
    std::uniform_int_distribution<std::size_t> size_dist(1, 64);

    constexpr std::size_t kAlignments[] = {1, 2, 4, 8, 16, 32};
    std::uniform_int_distribution<std::size_t> align_idx_dist(
        0, std::size(kAlignments) - 1
    );

    struct Allocation {
        std::byte* ptr;
        std::size_t size;
        std::size_t aign;
    };
    std::vector<Allocation> allocations;

    for(int i = 0; i < 500; ++i) {
        std::size_t size = size_dist(rng);
        std::size_t align = kAlignments[align_idx_dist(rng)];
        void* p = a.alloc(size, align);
        if (p == nullptr) break;

        REQUIRE(is_aligned(p, align));
        allocations.push_back({static_cast<std::byte*>(p), size, align});
    }

    REQUIRE(!allocations.empty());

    // No two allocations may overlap. Since bump allocator returns 
    // stricly increasing address, checking adjacency is sufficient.
    for (std::size_t i = 1; i < allocations.size(); ++i) {
        std::byte* prev_end = allocations[i-1].ptr + allocations[i-1].size;
        REQUIRE(allocations[i].ptr >= prev_end);
    }

    // All allocations lie inside the buffer.
    for (const auto& alloc: allocations) {
        REQUIRE(alloc.ptr >= buffer);
        REQUIRE(alloc.ptr + alloc.size <= buffer + kBufSize);
    }
}
