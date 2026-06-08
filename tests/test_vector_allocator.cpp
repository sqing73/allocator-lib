// tests/vector_allocator_test.cpp
#include <catch2/catch_test_macros.hpp>
#include <alloc/vector.hpp>
#include <alloc/stl_slab_allocator.hpp>
#include <alloc/slab.hpp>
#include <cstddef>

using alloc::SlabAllocator;
using alloc::STLSlabAllocator;
using alloc::Vector;

TEST_CASE("Vector still works with std::allocator default", "[vector][allocator]")
{
    Vector<int> v;
    for (int i = 0; i < 1000; ++i)
        v.push_back(i);
    REQUIRE(v.size() == 1000);
    for (int i = 0; i < 1000; ++i)
        REQUIRE(v[i] == i);
}

TEST_CASE("Vector works with STLSlabAllocator", "[vector][allocator]")
{
    SlabAllocator slab(64 * 1024 * 1024);

    Vector<int, 0, STLSlabAllocator<int>> v{STLSlabAllocator<int>(&slab)};
    for (int i = 0; i < 100; ++i)
        v.push_back(i);
    REQUIRE(v.size() == 100);
    for (int i = 0; i < 100; ++i)
        REQUIRE(v[i] == i);
}

TEST_CASE("allocator equality reflects underlying resource", "[vector][allocator]")
{
    SlabAllocator slab1(1024);
    SlabAllocator slab2(1024);

    STLSlabAllocator<int> a(&slab1);
    STLSlabAllocator<int> b(&slab1);
    STLSlabAllocator<int> c(&slab2);

    REQUIRE(a == b);
    REQUIRE(a != c);
}

TEST_CASE("rebind ctor preserves the underlying slab", "[vector][allocator]")
{
    SlabAllocator slab(1024);

    STLSlabAllocator<int> ai(&slab);
    STLSlabAllocator<double> ad(ai); // converting / rebind ctor
    REQUIRE(ai.slab_ == ad.slab_);
}

TEST_CASE("get_allocator round-trip", "[vector][allocator]")
{
    SlabAllocator slab(1024);

    Vector<int, 0, STLSlabAllocator<int>> v{STLSlabAllocator<int>(&slab)};
    REQUIRE(v.get_allocator().slab_ == &slab);
}
