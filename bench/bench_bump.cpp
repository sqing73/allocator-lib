#include <alloc/bump.hpp>

#include <benchmark/benchmark.h>

#include <cstddef>
#include <vector>

static void BM_BumpAlloc(benchmark::State& state) {
    const std::size_t alloc_size = static_cast<std::size_t>(state.range(0));

    std::vector<std::byte> buffer(1 << 20);
    alloc::BumpAllocator a(buffer.data(), buffer.size());

    for (auto _ : state) {
        void* p = a.alloc(alloc_size, alignof(std::max_align_t));
        benchmark::DoNotOptimize(p);
        a.reset();
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_BumpAlloc)->Arg(16)->Arg(64)->Arg(256);

BENCHMARK_MAIN();
