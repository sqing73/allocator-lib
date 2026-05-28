add_executable(bench_bump bench_bump.cpp)
target_link_libraries(bench_bump PRIVATE
    alloc::alloc
    benchmark::benchmark
)
