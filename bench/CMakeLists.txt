add_executable(bench_allocators bench_allocators.cpp)
target_link_libraries(bench_allocators PRIVATE
    alloc::alloc
    benchmark::benchmark
)
