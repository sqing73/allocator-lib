add_executable(test_bump 
    test_bump.cpp
    utils.hpp
)
target_link_libraries(test_bump PRIVATE
    alloc::alloc
    Catch2::Catch2WithMain
)
add_test(NAME test_bump COMMAND test_bump)
target_include_directories(test_bump PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(test_pool test_pool.cpp)
target_link_libraries(test_pool PRIVATE
    alloc::alloc
    Catch2::Catch2WithMain
)
add_test(NAME test_pool COMMAND test_pool)
target_include_directories(test_pool PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

