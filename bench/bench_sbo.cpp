#include <benchmark/benchmark.h>
#include <vector>
#include <alloc/vector.hpp>

static void BM_StdVector_4ints(benchmark::State &state)
{
    for (auto _ : state) {
        std::vector<int> v;
        v.push_back(1);
        v.push_back(2);
        v.push_back(3);
        v.push_back(4);
        benchmark::DoNotOptimize(v.data());
        benchmark::ClobberMemory();
    } 
}
BENCHMARK(BM_StdVector_4ints);

static void BM_VectorNoSBO_4ints(benchmark::State &state) {
    for (auto _ : state) {
        alloc::Vector<int, 0> v;
        v.emplace_back(1);
        v.emplace_back(2);
        v.emplace_back(3);
        v.emplace_back(4);
        benchmark::DoNotOptimize(v.data());
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_VectorNoSBO_4ints);

static void BM_VectorSBO_4ints(benchmark::State &state)
{
    for (auto _ : state)
    {
        alloc::Vector<int, 4> v;
        v.emplace_back(1);
        v.emplace_back(2);
        v.emplace_back(3);
        v.emplace_back(4);
        benchmark::DoNotOptimize(v.data());
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_VectorSBO_4ints);

static void BM_StdVector_6ints(benchmark::State &state)
{
    for (auto _ : state)
    {
        std::vector<int> v;
        v.push_back(1);
        v.push_back(2);
        v.push_back(3);
        v.push_back(4);
        v.push_back(5);
        v.push_back(6);
        benchmark::DoNotOptimize(v.data());
        benchmark::DoNotOptimize(v[0]);
        benchmark::DoNotOptimize(v[5]);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_StdVector_6ints);

static void BM_VectorNoSBO_6ints(benchmark::State &state)
{
    for (auto _ : state)
    {
        alloc::Vector<int, 0> v;
        v.emplace_back(1);
        v.emplace_back(2);
        v.emplace_back(3);
        v.emplace_back(4);
        v.emplace_back(5);
        v.emplace_back(6);
        benchmark::DoNotOptimize(v.data());
        benchmark::DoNotOptimize(v[0]);
        benchmark::DoNotOptimize(v[5]);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_VectorNoSBO_6ints);

static void BM_VectorSBO_6ints(benchmark::State &state)
{
    for (auto _ : state)
    {
        alloc::Vector<int, 4> v;
        v.emplace_back(1);
        v.emplace_back(2);
        v.emplace_back(3);
        v.emplace_back(4);
        v.emplace_back(5); // triggers inline → heap migration
        v.emplace_back(6);
        benchmark::DoNotOptimize(v.data());
        benchmark::DoNotOptimize(v[0]);
        benchmark::DoNotOptimize(v[5]);
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_VectorSBO_6ints);

BENCHMARK_MAIN();
