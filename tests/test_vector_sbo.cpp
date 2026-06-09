#include <alloc/vector.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("SBO: small vector stays inline", "[vector][sbo]")
{
    alloc::Vector<int, 4> v;
    v.push_back(1);
    v.push_back(2);
    v.push_back(3);
    REQUIRE_FALSE(v.is_heap());

    // data() points inside the Vector object itself.
    auto *obj_lo = reinterpret_cast<std::byte *>(&v);
    auto *obj_hi = obj_lo + sizeof(v);
    auto *dp = reinterpret_cast<std::byte *>(v.data());
    REQUIRE(dp >= obj_lo);
    REQUIRE(dp < obj_hi);
}

TEST_CASE("SBO: crossing the boundary spills to heap", "[vector][sbo]")
{
    alloc::Vector<int, 4> v;
    for (int i = 0; i < 5; ++i)
        v.push_back(i);
    REQUIRE(v.is_heap());
    REQUIRE(v.size() == 5);
    for (int i = 0; i < 5; ++i)
        REQUIRE(v[i] == i);
}

TEST_CASE("SBO: N == 0 behaves like the pre-SBO Vector", "[vector][sbo]")
{
    alloc::Vector<int, 0> v;
    REQUIRE_FALSE(v.is_heap()); // nullptr => no storage yet
    v.push_back(42);
    REQUIRE(v.is_heap()); // first push allocates
}
