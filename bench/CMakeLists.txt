add_executable(bench_allocators bench_allocators.cpp)
target_link_libraries(bench_allocators PRIVATE
    alloc::alloc
    benchmark::benchmark
)

add_executable(bench_vector_allocator bench_vector_allocator.cpp)
target_link_libraries(bench_vector_allocator PRIVATE
    alloc::alloc
    benchmark::benchmark
)

add_executable(bench_sbo bench_sbo.cpp)
target_link_libraries(bench_sbo PRIVATE
    alloc::alloc
    benchmark::benchmark
)
