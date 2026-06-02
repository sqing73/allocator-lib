#include <benchmark/benchmark.h>
#include <cstdlib>
#include <vector>

#include <alloc/bump.hpp>
#include <alloc/pool.hpp>
#include <alloc/slab.hpp>

static constexpr int kN = 1'000'000;

static void BM_Malloc(benchmark::State &state)
{
    const std::size_t sz = static_cast<std::size_t>(state.range(0));
    std::vector<void *> ptrs(kN);

    for (auto _ : state)
    {
        for (int i = 0; i < kN; ++i)
        {
            ptrs[i] = std::malloc(sz);
            benchmark::DoNotOptimize(ptrs[i]);
        }
        for (int i = 0; i < kN; ++i)
        {
            std::free(ptrs[i]);
            benchmark::DoNotOptimize(ptrs[i]);
        }
    }
    state.SetItemsProcessed(state.iterations() * kN);
}

BENCHMARK(BM_Malloc)->Arg(16)->Arg(32)->Arg(64);

static void BM_NewDelete(benchmark::State &state)
{
    const std::size_t sz = static_cast<std::size_t>(state.range(0));
    std::vector<void *> ptrs(kN);

    for (auto _ : state)
    {
        for (int i = 0; i < kN; ++i)
        {
            ptrs[i] = ::operator new(sz);
            benchmark::DoNotOptimize(ptrs[i]);
        }
        for (int i = 0; i < kN; ++i)
        {
            ::operator delete(ptrs[i]);
            benchmark::DoNotOptimize(ptrs[i]);
        }
    }
    state.SetItemsProcessed(state.iterations() * kN);
}

BENCHMARK(BM_NewDelete)->Arg(16)->Arg(32)->Arg(64);

static void BM_Pool(benchmark::State &state)
{
    const std::size_t sz = static_cast<size_t>(state.range(0));
    const std::size_t align = std::min(sz, alignof(std::max_align_t));

    const std::size_t arena_bytes = sz * kN * 2;
    std::vector<std::byte> buffer(arena_bytes);
    alloc::BumpAllocator arena(buffer.data(), arena_bytes);
    alloc::PoolAllocator pool(arena, sz, align, kN);

    std::vector<void *> ptrs(kN);

    // Warm pass: fill the freelist by allocating and freeing once.
    // After this, pool.alloc() will pop from a fully warm freelist.
    // Mostly helpful for stddev
    for (int i = 0; i < kN; ++i)
        ptrs[i] = pool.alloc();
    for (int i = 0; i < kN; ++i)
        pool.free(ptrs[i]);

    for (auto _ : state)
    {
        for (int i = 0; i < kN; ++i)
        {
            ptrs[i] = pool.alloc();
            benchmark::DoNotOptimize(ptrs[i]);
        }
        for (int i = 0; i < kN; ++i)
        {
            pool.free(ptrs[i]);
            benchmark::DoNotOptimize(ptrs[i]);
        }
    }
    state.SetItemsProcessed(state.iterations() * kN);
}

BENCHMARK(BM_Pool)->Arg(16)->Arg(32)->Arg(64);

static void BM_Slab(benchmark::State &state)
{
    const std::size_t sz = static_cast<std::size_t>(state.range(0));
    alloc::SlabAllocator slab(sz * kN * 2, kN);

    std::vector<void *> ptrs(kN);

    for (int i = 0; i < kN; ++i)
        ptrs[i] = slab.alloc(sz);
    for (int i = 0; i < kN; ++i)
        slab.dealloc(ptrs[i], sz);

    for (auto _ : state)
    {
        for (int i = 0; i < kN; ++i)
        {
            ptrs[i] = slab.alloc(sz);
            benchmark::DoNotOptimize(ptrs[i]);
        }
        for (int i = 0; i < kN; ++i)
        {
            slab.dealloc(ptrs[i], sz);
            benchmark::DoNotOptimize(ptrs[i]);
        }
    }
    state.SetItemsProcessed(state.iterations() * kN);
}

BENCHMARK(BM_Slab)->Arg(16)->Arg(32)->Arg(64);

BENCHMARK_MAIN();
