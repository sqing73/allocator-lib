#include <alloc/vector.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string>
#include <stdexcept>

struct TrackedStats
{
    int default_ctors = 0;
    int copy_ctors = 0;
    int move_ctors = 0;
    int copy_assigns = 0;
    int move_assigns = 0;
    int dtors = 0;

    int total_constructors() const
    {
        return default_ctors + copy_ctors + move_ctors;
    }

    bool balanced() const
    {
        return total_constructors() == dtors;
    }

    void reset()
    {
        *this = TrackedStats{};
    }
};

static TrackedStats g_stats;

struct Tracked
{
    int value;

    explicit Tracked(int v = 0) : value(v) { ++g_stats.default_ctors; }

    Tracked(const Tracked &o) : value(o.value) { ++g_stats.copy_ctors; }

    Tracked(Tracked &&o) noexcept : value(o.value)
    {
        o.value = -1;
        ++g_stats.move_ctors;
    }

    Tracked &operator=(const Tracked &o)
    {
        value = o.value;
        ++g_stats.copy_assigns;
        return *this;
    }

    Tracked &operator=(Tracked &&o) noexcept
    {
        value = o.value;
        o.value = -1;
        ++g_stats.move_assigns;
        return *this;
    }

    ~Tracked() { ++g_stats.dtors; }
};

struct ThrowOnMove
{
    int value;
    explicit ThrowOnMove(int v = 0) : value(v) {}
    ThrowOnMove(const ThrowOnMove &) = default;
    ThrowOnMove(ThrowOnMove &&) noexcept(false)
    {
        throw std::runtime_error("move failed");
    }
    ~ThrowOnMove() = default;
};

TEST_CASE("Vector default constructor", "[vector]")
{
    alloc::Vector<int> v;
    CHECK(v.size() == 0);
    CHECK(v.capacity() == 0);
    CHECK(v.empty());
    CHECK(v.data() == nullptr);
}

TEST_CASE("Vector push_back int - grows correctly", "[vector]")
{
    alloc::Vector<int> v;

    for (int i = 0; i < 8; ++i)
    {
        v.push_back(i);
    }

    REQUIRE(v.size() == 8);
    for (int i = 0; i < 8; ++i)
    {
        CHECK(v[i] == i);
    }
}

TEST_CASE("Vector push_back string - copy and move overloads", "[vector]")
{
    alloc::Vector<std::string> v;

    std::string s = "hello";
    v.push_back(s);
    CHECK(s == "hello");
    CHECK(v[0] == "hello");

    v.push_back(std::move(s));
    CHECK(v[1] == "hello");

    v.push_back("world");
    CHECK(v[2] == "world");
    CHECK(v.size() == 3);
}

TEST_CASE("Vector reserve - no construction of elements", "[vector]")
{
    g_stats.reset();

    alloc::Vector<Tracked> v;
    v.reserve(16);

    CHECK(v.size() == 0);
    CHECK(v.capacity() == 16);
    CHECK(g_stats.total_constructors() == 0);
    CHECK(g_stats.dtors == 0);
}

TEST_CASE("Vector Tracked: push_back uses move for noexcept-move type", "[vector]")
{
    g_stats.reset();

    {
        alloc::Vector<Tracked> v;
        v.reserve(4);

        for (int i = 0; i < 4; ++i)
        {
            Tracked t(i);
            v.push_back(std::move(t));
        }

        CHECK(v.size() == 4);
        for (int i = 0; i < 4; ++i)
            CHECK(v[i].value == i);
        CHECK(g_stats.default_ctors == 4);
        CHECK(g_stats.move_ctors == 4);
        CHECK(g_stats.copy_ctors == 0);
        CHECK(g_stats.dtors == 4);
    }

    CHECK(g_stats.balanced());
}

TEST_CASE("Vector Tracked: grow uses move (noexcept move type)", "[vector]")
{
    g_stats.reset();

    {
        alloc::Vector<Tracked> v;
        // 1 -> 2 -> 4 -> 8. 5 elements will force at leaset one realloc
        for (int i = 0; i < 5; ++i)
        {
            v.push_back(Tracked(i));
        }

        CHECK(v.size() == 5);
        for (int i = 0; i < 5; ++i)
            CHECK(v[i].value == i);

        CHECK(g_stats.copy_ctors == 0);
    }

    CHECK(g_stats.balanced());
}

TEST_CASE("Vector ThrowOnMove: push_back copy path", "[vector]")
{
    g_stats.reset();
    {
        alloc::Vector<ThrowOnMove> v;
        for (int i = 0; i < 8; ++i)
        {
            ThrowOnMove tm(i);
            v.push_back(tm); // push_back(const T&) — copy
        }
        CHECK(v.size() == 8);
        for (int i = 0; i < 8; ++i)
            CHECK(v[i].value == i);
        CHECK(g_stats.move_ctors == 0);
    }
    CHECK(g_stats.balanced());
}

TEST_CASE("Vector destructor: no leaks (Tracked balance)", "[vector][tracked]")
{
    g_stats.reset();

    {
        alloc::Vector<Tracked> v;
        for (int i = 0; i < 10; ++i)
            v.push_back(Tracked(i));
    }
    // All 10 elements should be destroyed when v goes out of scope.
    CHECK(g_stats.balanced());
}

TEST_CASE("Vector clear: destroys elements, keeps capacity", "[vector]")
{
    g_stats.reset();

    alloc::Vector<Tracked> v;
    for (int i = 0; i < 4; ++i)
        v.push_back(Tracked(i));

    size_t cap_before = v.capacity();
    v.clear();

    CHECK(v.size() == 0);
    CHECK(v.capacity() == cap_before); // capacity unchanged
    CHECK(g_stats.dtors >= 4);         // elements destroyed

    // Push back again — should reuse existing buffer
    v.push_back(Tracked(99));
    CHECK(v.size() == 1);
    CHECK(v[0].value == 99);
    CHECK(v.capacity() == cap_before);
}

TEST_CASE("Vector at(): bounds checking", "[vector]")
{
    alloc::Vector<int> v;
    v.push_back(1);
    v.push_back(2);

    CHECK(v.at(0) == 1);
    CHECK(v.at(1) == 2);
    CHECK_THROWS_AS(v.at(2), std::out_of_range);
}

TEST_CASE("Vector iterators: range-for works", "[vector]")
{
    alloc::Vector<int> v;
    for (int i = 0; i < 5; ++i)
        v.push_back(i * 10);

    int sum = 0;
    for (int x : v)
        sum += x;
    CHECK(sum == 0 + 10 + 20 + 30 + 40);
}

TEST_CASE("Vector large push_back: correct values after multiple grows", "[vector]")
{
    alloc::Vector<int> v;
    const int N = 1000;
    for (int i = 0; i < N; ++i)
        v.push_back(i);

    REQUIRE(v.size() == N);
    for (int i = 0; i < N; ++i)
        CHECK(v[i] == i);
}
