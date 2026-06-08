#include <benchmark/benchmark.h>
#include <alloc/vector.hpp>
#include <alloc/stl_slab_allocator.hpp>
#include <alloc/slab.hpp>
#include <vector>
#include <cstddef>

using alloc::SlabAllocator;
using alloc::STLSlabAllocator;
using alloc::Vector;

static constexpr std::size_t kArena = 1u << 20; // 1 MB

static void BM_StdVector(benchmark::State &state)
{
    for (auto _ : state)
    {
        std::vector<int> v;
        for (int i = 0; i < state.range(0); ++i)
            v.push_back(i);
        benchmark::DoNotOptimize(v.data());
    }
}
BENCHMARK(BM_StdVector)->Range(8, 128);

static void BM_OurVector_StdAlloc(benchmark::State &state)
{
    for (auto _ : state)
    {
        Vector<int> v;
        for (int i = 0; i < state.range(0); ++i)
            v.push_back(i);
        benchmark::DoNotOptimize(v.data());
    }
}
BENCHMARK(BM_OurVector_StdAlloc)->Range(8, 128);

static void BM_OurVector_Slab(benchmark::State &state)
{
    for (auto _ : state)
    {
        SlabAllocator slab(kArena);
        {
            Vector<int, 0, STLSlabAllocator<int>> v{STLSlabAllocator<int>(&slab)};
            for (int i = 0; i < state.range(0); ++i)
                v.push_back(i);
            benchmark::DoNotOptimize(v.data());
        }
    }
}
BENCHMARK(BM_OurVector_Slab)->Range(8, 128);

BENCHMARK_MAIN();
