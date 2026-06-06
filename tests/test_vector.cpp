#include <alloc/vector.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string>
#include <stdexcept>
#include <type_traits>

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

struct ThrowOnCopy
{
    static inline int copies_made = 0;
    static inline int throw_at = -1;

    int value;

    explicit ThrowOnCopy(int v) : value(v) {}

    ThrowOnCopy(const ThrowOnCopy &o) : value(o.value)
    {
        if (copies_made == throw_at)
            throw std::runtime_error("planned");
        ++copies_made;
    }

    // Move exists but is NOT noexcept → move_if_noexcept falls back to copy
    ThrowOnCopy(ThrowOnCopy &&o) : value(o.value) {}

    static void reset(int n)
    {
        copies_made = 0;
        throw_at = n;
    }
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

TEST_CASE("Vector copy ctor produces independent storage", "[vector]")
{
    alloc::Vector<int> a;
    a.push_back(1);
    a.push_back(2);
    a.push_back(3);

    alloc::Vector<int> b(a);
    REQUIRE(b.size() == 3);
    REQUIRE(b[0] == 1);
    REQUIRE(b[1] == 2);
    REQUIRE(b[2] == 3);
    REQUIRE(a.data() != b.data());

    b[0] = 99;
    REQUIRE(a[0] == 1);
}

TEST_CASE("Vector move ctor is noexcept and steals storage", "[vector]")
{
    static_assert(std::is_nothrow_move_constructible_v<alloc::Vector<int>>);

    alloc::Vector<int> a;
    a.push_back(1);
    a.push_back(2);
    int *original = a.data();

    alloc::Vector<int> b(std::move(a));
    REQUIRE(b.size() == 2);
    REQUIRE(b.data() == original);
    REQUIRE(a.size() == 0);
    REQUIRE(a.capacity() == 0);
    REQUIRE(a.data() == nullptr);
}

TEST_CASE("Vector copy assignment via copy-and-swap", "[vector]")
{
    alloc::Vector<std::string> a;
    a.push_back("one");
    a.push_back("two");

    alloc::Vector<std::string> b;
    b.push_back("old");

    b = a;
    REQUIRE(b.size() == 2);
    REQUIRE(b[0] == "one");
    REQUIRE(b[1] == "two");
    REQUIRE(a.size() == 2); // source unchanged
}

TEST_CASE("Vector move assignment", "[vector]")
{
    static_assert(std::is_nothrow_move_assignable_v<alloc::Vector<int>>);

    alloc::Vector<int> a;
    a.push_back(1);
    a.push_back(2);
    int *original = a.data();

    alloc::Vector<int> b;
    b.push_back(99);

    b = std::move(a);
    REQUIRE(b.size() == 2);
    REQUIRE(b.data() == original);
    REQUIRE(a.size() == 0);
}

TEST_CASE("Vector move-assigning to self is safe", "[vector]")
{
    alloc::Vector<int> a;
    a.push_back(1);
    a.push_back(2);
    a = std::move(a); // valid-but-unspecified after; just no UB
    SUCCEED();
}

TEST_CASE("Vector emplace_back constructs in place — no copies, no moves", "[vector]")
{
    g_stats.reset();
    alloc::Vector<Tracked> v;
    v.reserve(4); // avoid grow noise
    v.emplace_back(42);

    REQUIRE(g_stats.total_constructors() == 1);
    REQUIRE(g_stats.copy_ctors == 0);
    REQUIRE(g_stats.move_ctors == 0);
    REQUIRE(v[0].value == 42);
}

TEST_CASE("Vector emplace_back forwards value categories", "[vector]")
{
    alloc::Vector<std::string> v;
    v.reserve(2);

    std::string s = "hello";
    v.emplace_back(s); // lvalue → copy-constructs string
    REQUIRE(s == "hello");

    std::string t = "world";
    v.emplace_back(std::move(t)); // rvalue → move-constructs string
    REQUIRE(v[1] == "world");
}

TEST_CASE("strong exception guarantee: throwing grow leaves vector unchanged",
          "[vector]")
{
    alloc::Vector<ThrowOnCopy> v;
    v.reserve(2);
    v.push_back(ThrowOnCopy(1)); // moves into storage (not a copy)
    v.push_back(ThrowOnCopy(2));

    auto *before = v.data();
    auto size_before = v.size();
    auto cap_before = v.capacity();

    // Force grow. Grow copies (move isn't noexcept). Throw on the 2nd copy.
    ThrowOnCopy::reset(/*throw_at=*/1);
    REQUIRE_THROWS_AS(v.push_back(ThrowOnCopy(3)), std::runtime_error);

    REQUIRE(v.size() == size_before);
    REQUIRE(v.capacity() == cap_before);
    REQUIRE(v.data() == before);
    REQUIRE(v[0].value == 1);
    REQUIRE(v[1].value == 2);
}
