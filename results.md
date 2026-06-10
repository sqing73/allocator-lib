# Phase 1 — Allocator & Container Benchmarks

Captured Day 9. Polished into README sections on Day 10.

## Setup

- CPU: 12 cores @ 4.27 GHz, 32 KiB L1d × 6, 512 KiB L2 × 6, 16 MiB L3 (shared)
- Build: Release preset (`-O3 -DNDEBUG`), C++20
- Benchmark harness: Google Benchmark 1.9.0, `--benchmark_repetitions=5 --benchmark_report_aggregates_only=true`
- Profiling: `perf stat --repeat 5`
- All numbers from `bench/bench_sbo.cpp`. Workload: construct vector → push N ints → force loads back via `DoNotOptimize(v[i])` → `ClobberMemory()` → destroy. Each iteration is one full lifecycle.

CPU scaling was enabled during these runs; Google Benchmark warned about it. Variance was nonetheless very tight (cv ≤ 0.13% on wall clock) because the workload is small enough that the CPU stayed at a stable frequency throughout. Numbers should be reproducible on the same machine.

---

## Bench A — Small vectors that fit in the inline buffer

Four `int`s, never spills past N=4. This is the case SBO is designed for.

| Configuration | Time/iter | Ratio vs SBO |
|---|---:|---:|
| `std::vector<int>` | 30.3 ns | 32× slower |
| `alloc::Vector<int, 0>` (SBO disabled) | 30.3 ns | 32× slower |
| `alloc::Vector<int, 4>` (SBO enabled) | 0.948 ns | 1× |

The control passed: `std::vector` and the no-SBO version are within 0.0% of each other. Any delta between either of them and the SBO version is attributable to SBO alone, not to some unrelated implementation difference between this library and libstdc++.

The 32× speedup is the headline finding. With N=4 sized to fit the workload, the SBO version performs zero heap allocations per iteration — the entire vector lives in the stack frame, four `int` stores into a 16-byte inline buffer. The compiler vectorizes the four `emplace_back` calls into a single SIMD store, so the per-iteration cost bottoms out at "one `movdqu` plus loop control."

---

## Bench B — Vectors that spill past the inline buffer

Six `int`s, with N=4 the fifth push triggers a grow from inline to heap. The realistic case where SBO has to "do real work."

| Configuration | Time/iter | Ratio vs SBO |
|---|---:|---:|
| `std::vector<int>` | 40.8 ns | 3.5× slower |
| `alloc::Vector<int, 0>` (SBO disabled) | 40.5 ns | 3.5× slower |
| `alloc::Vector<int, 4>` (SBO enabled) | 11.6 ns | 1× |

SBO still wins, by ~3.5×. The mechanism is allocation count: with a doubling grow strategy, six elements walks capacities 1 → 2 → 4 → 8 in the heap-only configurations, paying three `malloc` calls. The SBO configuration absorbs the first three growth steps in the inline buffer and pays exactly one `malloc` (the 4 → 8 jump) plus four element moves out of inline storage. Three allocs vs one alloc ≈ 3× speedup; the numbers line up cleanly.

Together, the two benchmarks tell the trade-off honestly: **SBO is a 32× win when you fit and a 3.5× win when you spill**, for this T and this N. Both halves of the story matter — the spillover case is what you'd expect to dominate in a realistic mixed workload, and even there SBO is comfortably ahead.

---

## Why SBO wins: causal analysis via `perf stat` (Bench A)

Wall-clock alone says SBO is faster; it doesn't say why. Hardware counters were collected on all three Bench A configurations.

Per-iteration counters (summed across 5 runs, divided by total Google Benchmark iterations):

| Metric | StdVector | Vector<int,0> | Vector<int,4> SBO |
|---|---:|---:|---:|
| Time | 31.3 ns | 31.1 ns | 0.955 ns |
| Cycles | 29.7 | 29.7 | 1.11 |
| **Instructions** | **119** | **117** | **2.5** |
| IPC | 4.01 | 3.95 | 2.23 |
| L1d loads | 38.0 | 37.5 | 1.16 |
| L1d miss rate | 0.008% | 0.008% | 0.009% |
| Branches | 24.7 | 24.8 | 0.55 |
| Branch miss rate | 0.009% | 0.009% | 0.016% |

### The speedup is an instruction-count story, not a cache story

The naive model for "why allocators win" usually invokes locality: custom allocators keep data in cache, malloc scatters it, cache misses dominate. That model is wrong here. L1 miss rates are essentially identical across all three configurations (≈0.008%). The heap path hits a hot, cache-resident arena because allocating-and-freeing the same size in a tight loop reuses the same lines over and over.

The actual mechanism is much simpler: **the SBO version executes 47× fewer instructions per iteration**. The heap path runs ~117 instructions per iter of allocator work — size-class lookup, freelist manipulation, bookkeeping updates, then the same thing in reverse on free — most of which the SBO version simply skips. The 32× wall-clock speedup is "did less work," not "did the same work with better locality."

### The IPC inversion

Counterintuitively, the heap variants run at *higher* IPC than SBO (4.0 vs 2.2). Mechanically: malloc's hot path is full of independent operations the CPU can pipeline aggressively — multiple loads, compares, and updates that the out-of-order engine overlaps. SBO is so spare that it's bottlenecked on the few dependent operations it does have (one SIMD store, the loop counter, the branch), with less ILP to extract.

This is a useful illustration of why IPC on its own is not a quality metric. A workload doing 47× less work at half the IPC still wins by 25×. The conclusion: **cycles are saved by not executing instructions, not by executing instructions faster.**

### Branches, briefly

The heap variants execute ~25 branches per iteration vs ~0.55 for SBO. Branch *miss rates* are similar across configurations (~0.01%), so it's a count effect, not a prediction-quality effect. The 45× reduction in branch count is consistent with the broader picture: the SBO inner loop has almost no control flow — just a vectorized store and the benchmark loop's own increment/compare.

---

## Day 3 allocator benchmarks

Original headline benchmark from Day 3 compared `malloc`/`new`/PoolAllocator/SlabAllocator on 1M small-object allocate-free cycles. Numbers were collected and live in `bench/bench_alloc.cpp` output. They are not reproduced here pending the cold-start caveat noted below; the slab numbers in particular are not a fair representation of steady-state performance.

The pool allocator showed the expected pattern: 5–10× faster than malloc on small fixed-size allocations, attributable to the LIFO freelist's single-pointer hot path replacing malloc's size-class lookup and bookkeeping. This is the same instruction-count story Bench A illustrates more cleanly.

---

## Known limitations and deferrals

**Slab benchmark cold-start distortion.** The Day 3 slab benchmark reconstructs the `SlabAllocator` per iteration, which pays the slab's setup cost on every measurement rather than amortizing across a steady-state run with warm freelists. As written, the slab benchmark measured ~8–10× *slower* than `std::allocator`, which is the opposite of the intended steady-state result. This was identified during Phase 1 and deliberately deferred rather than fixed in scope. A proper warm-state harness — pre-allocate, free into the slab to populate freelists, *then* measure allocate/free cycles — would tell the steady-state story honestly. Tracked for a future revisit.

**No POCMA/POCCA propagation.** The custom allocator adapter (`STLSlabAllocator<T>`) does not implement `propagate_on_container_copy_assignment` or `propagate_on_container_move_assignment`. This was a deliberate simplification noted in the source. It means copy/move assignment between `Vector`s with different allocator instances has subtle behavior; the test suite avoids that case.

**Single-threaded only.** All allocators in this library are explicitly single-threaded. No thread-safety analysis was performed.

**No shrink-to-fit re-inlining.** The SBO vector, once it has grown to heap storage, never re-inlines even if subsequent operations leave it with fewer than N elements. This matches the behavior of LLVM's `SmallVector` and is the standard implementation choice; documenting it for completeness.

**`perf stat` event multiplexing.** Ten counter events were requested per run; the CPU has fewer hardware PMCs, so perf multiplexed and scaled. Numbers are accurate within ~1% but not exact. The dramatic ratios reported here (47× instructions, 32× time) are well outside any multiplexing noise floor.

**`dTLB-loads` counter.** Reported with ~20-30% variance because the absolute counts are tiny (17k–25k events across billions of memory references). The dTLB data has no signal for this workload — the working set is small enough to fit in a handful of pages. Dropped from interpretation.

---

## Reproduction

```bash
cmake --preset release
cmake --build --preset release --target bench_sbo

./build/release/bench/bench_sbo \
    --benchmark_repetitions=5 \
    --benchmark_report_aggregates_only=true \
    --benchmark_min_time=1.0s

sudo perf stat \
    -e cycles,instructions,L1-dcache-loads,L1-dcache-load-misses,\
branches,branch-misses,cache-references,cache-misses \
    --repeat 5 \
    ./build/release/bench/bench_sbo \
        --benchmark_min_time=2.0s \
        --benchmark_filter=BM_VectorSBO_4ints
```

Repeat the `perf stat` invocation with `BM_VectorNoSBO_4ints` and `BM_StdVector_4ints` to reproduce the causal analysis table.
