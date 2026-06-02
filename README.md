## Benchmarks

Headline workload: 1M allocations of a fixed size, then 1M frees, repeated.
Compared against `malloc`/`free` and `operator new`/`delete`. Freelists are
warmed before timing; `benchmark::DoNotOptimize` guards every allocation so the
optimizer can't elide it. Release build (`-O3 -DNDEBUG`), 5 repetitions,
aggregates only.

Machine: 12 × 3.4 GHz, L1d 32 KiB, L2 512 KiB, L3 16 MiB.

Time is **ns per alloc+free pair** (lower is better).

| size  | malloc | new/delete | pool | slab | pool vs malloc | slab vs malloc |
|-------|-------:|-----------:|-----:|-----:|---------------:|---------------:|
| 16 B  |  10.24 |      11.98 | 2.04 | 3.92 |          5.0×  |          2.6×  |
| 32 B  |  10.88 |      12.58 | 3.52 | 4.44 |          3.1×  |          2.5×  |
| 64 B  |  16.44 |      18.11 | 6.97 | 7.38 |          2.4×  |          2.2×  |

### Reading the numbers

The speedup shrinks as block size grows, and that's the whole story. A freelist
pop is O(1) — a pointer load and a store, independent of block size — yet the
pool's time scales nearly linearly (2.0 → 3.5 → 7.0 ns). The benchmark isn't
measuring allocator logic; it's measuring the cache-line fetch behind each pop.
With 1M live blocks the working set is 16/32/64 MB for the three sizes, crossing
the 16 MiB L3 and pushing the larger sizes into DRAM. Each pop becomes a
memory-bound pointer chase.

This also explains `malloc`'s flat 16→32 B curve: it carries a fixed per-call
tax (size-class lookup, bin management, locking) that dominates at small sizes
and masks the working-set cost until 64 B swamps it. The pool has effectively no
fixed overhead, so its time is pure working-set cost. Where the pool wins is by
*removing* the fixed tax — which is why the advantage is largest (5×) at 16 B,
where that tax is the entire gap, and collapses (2.4×) at 64 B, where both are
bandwidth-bound and converge.

The slab adds size-class dispatch on top of the pool. The gap over the raw pool
shrinks with size (1.9 → 0.9 → 0.4 ns), consistent with the dispatch executing
in the shadow of the cache miss at larger sizes.

<!-- TODO (Day 10): add perf stat — L1-dcache-load-misses and cache-misses
     per op for the 16/32/64 B cases. These are the *why* behind the table. -->
