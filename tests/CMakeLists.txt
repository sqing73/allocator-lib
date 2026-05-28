add_executable(test_bump test_bump.cpp)
target_link_libraries(test_bump PRIVATE
    alloc::alloc
    Catch2::Catch2WithMain
)
add_test(NAME test_bump COMMAND test_bump)
