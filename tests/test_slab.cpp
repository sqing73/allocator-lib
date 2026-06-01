#include <alloc/slab.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("slab: dispatches to correct pool by size", "[slab]")
{
    alloc::SlabAllocator slab(64 * 1024);

    void *p8 = slab.alloc(8);
    void *p16 = slab.alloc(16);
    void *p32 = slab.alloc(32);
    REQUIRE(p8 != nullptr);
    REQUIRE(p16 != nullptr);
    REQUIRE(p32 != nullptr);

    const alloc::PoolAllocator &pool8 = slab.pool(8);
    const alloc::PoolAllocator &pool16 = slab.pool(16);
    const alloc::PoolAllocator &pool32 = slab.pool(32);

    REQUIRE(pool8.used() == 1);
    REQUIRE(pool16.used() == 1);
    REQUIRE(pool32.used() == 1);
}

TEST_CASE("slab: boundary sizes round up correctly", "[slab]")
{
    alloc::SlabAllocator slab(256 * 1024);

    void *p1 = slab.alloc(1);     // -> 8-byte class
    void *p9 = slab.alloc(9);     // -> 16 byte class
    void *p512 = slab.alloc(512); // -> 512 byte class

    REQUIRE(p1 != nullptr);
    REQUIRE(p9 != nullptr);
    REQUIRE(p512 != nullptr);

    const alloc::PoolAllocator &pool8 = slab.pool(8);
    const alloc::PoolAllocator &pool16 = slab.pool(16);
    const alloc::PoolAllocator &pool512 = slab.pool(512);

    REQUIRE(pool8.used() == 1);
    REQUIRE(pool16.used() == 1);
    REQUIRE(pool512.used() == 1);
}

TEST_CASE("slab: oversized requests get rejected", "[slab]")
{
    alloc::SlabAllocator slab(64 * 1024);

    void* big = slab.alloc(1024);
    REQUIRE(big == nullptr);
}

TEST_CASE("slab: many allocs in one size class force pool growth", "[slab]") {
    alloc::SlabAllocator slab(1 * 1024 * 1024, 10);

    std::vector<void*> ptrs;
    // 100 blocks requested > 10 blocks per slab
    for (int i = 0; i < 100; i++) {
        void* p = slab.alloc(32);
        REQUIRE(p != nullptr);
        ptrs.push_back(p);
    }
}

TEST_CASE("slab: alloc(0) edge case", "[slab]") {
    alloc::SlabAllocator slab(64 * 1024);
    void* p = slab.alloc(0);

    REQUIRE(p != nullptr);
}
