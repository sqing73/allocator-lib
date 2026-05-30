#include <alloc/pool.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <set>
#include <type_traits>
#include <utility>
#include <vector>
#include <utils.hpp>

TEST_CASE("Repairs: tiny size/align are raised to hold a free-list pointer", "[pool]")
{
    constexpr std::size_t arena_bytes = 1 * 4;
    std::byte buffer[arena_bytes];
    alloc::BumpAllocator arena(buffer, arena_bytes);
    alloc::PoolAllocator p(arena, 1, 1, 4);

    REQUIRE(p.block_size() >= sizeof(void *));
    REQUIRE(p.block_align() >= alignof(void *));
}

TEST_CASE("Basic alloc returns distinct, aligned, in-bounds pointers", "[pool]")
{
    constexpr std::size_t arena_bytes = 32 * 8;
    std::byte buffer[arena_bytes];
    alloc::BumpAllocator arena(buffer, arena_bytes);
    constexpr std::size_t N = 8;
    alloc::PoolAllocator p(arena, 32, 16, 8);

    std::vector<void *> ptrs;

    for (std::size_t i = 0; i < N; i++)
    {
        void *q = p.alloc();
        REQUIRE(q != nullptr);
        REQUIRE(is_aligned(q, p.block_align()));
        ptrs.push_back(q);
    }

    std::set<void *> unique(ptrs.begin(), ptrs.end());
    REQUIRE(unique.size() == N);

    REQUIRE(p.used() == N);
    REQUIRE(p.available() == 0);
}

TEST_CASE("Exhaustion returns null and is recoverable after free", "[pool]")
{
    constexpr std::size_t arena_bytes = 16 * 3;
    std::byte buffer[arena_bytes];
    alloc::BumpAllocator arena(buffer, arena_bytes);
    alloc::PoolAllocator p(arena, 16, 8, 3);

    void *a = p.alloc();
    void *b = p.alloc();
    void *c = p.alloc();

    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    REQUIRE(c != nullptr);

    REQUIRE(p.alloc() == nullptr);
    REQUIRE(p.used() == 3);
    REQUIRE(p.available() == 0);

    p.free(b);
    REQUIRE(p.available() == 1);
    REQUIRE(p.used() == 2);

    void *d = p.alloc();
    REQUIRE(d == b);
    REQUIRE(p.alloc() == nullptr);

    p.free(nullptr);
    REQUIRE(p.available() == 0);
}

TEST_CASE("LIFO reuse: freed blocks come back in reverse free order", "[pool]")
{
    constexpr std::size_t arena_bytes = 24 * 4;
    std::byte buffer[arena_bytes];
    alloc::BumpAllocator arena(buffer, arena_bytes);
    alloc::PoolAllocator p(arena, 24, 8, 4);

    void *a = p.alloc();
    void *b = p.alloc();
    void *c = p.alloc();
    void *d = p.alloc();
    REQUIRE(p.available() == 0);

    // head -> c -> a -> d -> b -> nullptr
    p.free(b);
    p.free(d);
    p.free(a);
    p.free(c);

    REQUIRE(p.alloc() == c);
    REQUIRE(p.alloc() == a);
    REQUIRE(p.alloc() == d);
    REQUIRE(p.alloc() == b);
}

TEST_CASE("Fuzz: random alloc/free preserves all invariants", "[pool]")
{
    constexpr std::size_t N_BLOCKS = 64;
    constexpr std::size_t N_OPS = 2000;
    constexpr std::size_t BLOCK = 32;
    constexpr std::size_t ALIGN = 16;

    constexpr std::size_t arena_bytes = BLOCK * N_BLOCKS;
    std::byte buffer[arena_bytes];
    alloc::BumpAllocator arena(buffer, arena_bytes);

    alloc::PoolAllocator p(arena, BLOCK, ALIGN, N_BLOCKS);

    const std::size_t buffer_bytes = p.block_size() * N_BLOCKS;
    std::uintptr_t min_seen = ~std::uintptr_t{0};
    std::uintptr_t max_seen = 0;

    std::vector<void *> live; // blocks current held

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> coin(0, 1);

    for (std::size_t op = 0; op < N_OPS; op++)
    {
        bool do_alloc;
        if (live.empty())
            do_alloc = true;
        else if (live.size() == N_BLOCKS)
            do_alloc = false;
        else
            do_alloc = coin(rng) == 0;

        if (do_alloc)
        {
            void *q = p.alloc();
            REQUIRE(q != nullptr);
            REQUIRE(is_aligned(q, p.block_align()));

            // Track address span; it must always fit in one buffer
            auto qi = reinterpret_cast<std::uintptr_t>(q);
            if (qi < min_seen)
                min_seen = qi;
            if (qi > max_seen)
                max_seen = qi;
            REQUIRE(max_seen - min_seen < buffer_bytes);

            // Uniqueness: this pointer must not already be in 'live'
            REQUIRE(std::find(live.begin(), live.end(), q) == live.end());
            live.push_back(q);
        }
        else
        {
            // Free a random live block
            std::uniform_int_distribution<std::size_t> pick(0, live.size() - 1);
            std::size_t idx = pick(rng);
            void *q = live[idx];
            live[idx] = live.back();
            live.pop_back();
            p.free(q);
        }

        // Invariant after every op: counts add up
        REQUIRE(p.used() == live.size());
    }

    // Drain everything and confirm we can refill the pool completely
    for (void *q : live)
        p.free(q);
    REQUIRE(p.available() == N_BLOCKS);

    for (std::size_t i = 0; i < N_BLOCKS; i++)
    {
        REQUIRE(p.alloc() != nullptr);
    }
    REQUIRE(p.alloc() == nullptr);
}

TEST_CASE("Growth: more space should be allocated from arena when running out of memory", "[pool]")
{
    constexpr std::size_t arena_bytes = 8 * 2;
    alignas(8) std::byte buffer[arena_bytes];
    alloc::BumpAllocator arena(buffer, arena_bytes);
    alloc::PoolAllocator p(arena, 8, 8, 1);

    void* a = p.alloc();
    REQUIRE(p.available() == 0);
    REQUIRE(p.used() == 1);
    REQUIRE(p.total_blocks() == 1);

    void* b = p.alloc();
    REQUIRE(p.available() == 0);
    REQUIRE(p.used() == 2);
    REQUIRE(p.total_blocks() == 2);
    REQUIRE(a != b);
}
