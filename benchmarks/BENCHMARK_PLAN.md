# IPC Gateway v2.0 - Performance Benchmark Suite

**Generated**: 2025-12-26
**Task**: Task 20 - Performance Benchmarks
**Purpose**: Establish performance baseline for v1.0 before optimizations

---

## Benchmark Goals

1. **Throughput**: Measure requests/second sustained
2. **Latency**: Measure p50, p95, p99 response times
3. **Memory**: Track RSS, heap usage under load
4. **CPU**: Profile CPU utilization

---

## Target Metrics (v1.0 Baseline)

Based on C + Unix socket IPC architecture:

- **Throughput**: > 5,000 req/sec (single-threaded server)
- **Latency p99**: < 10ms
- **Memory (RSS)**: < 50MB under load
- **CPU**: < 30% single core at 2,500 req/sec

---

## Benchmark Structure

```
benchmarks/
├── bench_ipc_throughput.c       # Requests/second measurement
├── bench_ipc_latency.c          # Latency percentiles
├── bench_memory.c               # Memory profiling
├── bench_cpu.c                  # CPU profiling wrapper
├── results/
│   ├── v1.0_baseline.md         # v1.0 performance data
│   └── v2.0_comparison.md       # v2.0 vs v1.0
└── README.md                    # Benchmark usage guide
```

---

## Implementation Plan

### 1. Throughput Benchmark (bench_ipc_throughput.c)
**Effort**: 2 hours

**Features**:
- Spawn multiple client threads
- Send requests at maximum rate
- Measure sustained throughput
- Report requests/second

**Output**:
```
IPC Throughput Benchmark
Duration: 60 seconds
Threads: 4
Total Requests: 312,450
Throughput: 5,207 req/sec
```

### 2. Latency Benchmark (bench_ipc_latency.c)
**Effort**: 2 hours

**Features**:
- Send requests with timestamps
- Record round-trip time
- Calculate percentiles (p50, p95, p99, p99.9)
- Histogram visualization

**Output**:
```
IPC Latency Benchmark
Requests: 10,000
p50: 0.5ms
p95: 2.1ms
p99: 5.3ms
p99.9: 12.7ms
```

### 3. Memory Benchmark (bench_memory.c)
**Effort**: 1 hour

**Features**:
- Track RSS before/during/after load
- Monitor heap allocations
- Detect memory leaks
- Integration with valgrind/massif

**Output**:
```
Memory Benchmark
Baseline RSS: 12MB
Peak RSS: 45MB
Final RSS: 13MB
Leak detected: No
```

### 4. CPU Profiling (bench_cpu.c)
**Effort**: 1 hour

**Features**:
- Wrapper around `perf` or `gprof`
- Measure CPU % under load
- Identify hot functions
- Generate flamegraph

**Output**:
```
CPU Benchmark
Load: 2,500 req/sec
CPU Usage: 28% (single core)
Top functions:
  1. ipc_protocol_decode - 35%
  2. nats_publish - 25%
  3. json_parse - 15%
```

---

## Usage

### Run All Benchmarks
```bash
cd benchmarks
./run_all.sh

# Output: results/v1.0_baseline.md
```

### Run Individual Benchmarks
```bash
# Throughput
./build/bench-ipc-throughput

# Latency
./build/bench-ipc-latency

# Memory
valgrind --tool=massif ./build/bench-memory

# CPU
perf record -g ./build/ipc-gateway &
./build/bench-ipc-throughput
perf report
```

---

## Integration with v2.0

1. **Run benchmarks for v1.0** (this task)
2. **Apply Task 21 optimizations** (zero-copy)
3. **Re-run benchmarks** (v2.0)
4. **Compare results** (expect 20-30% improvement)

---

## Next Steps

1. Create `benchmarks/` directory
2. Implement `bench_ipc_throughput.c`
3. Implement `bench_ipc_latency.c`
4. Implement `bench_memory.c`
5. Create `run_all.sh` script
6. Run v1.0 baseline
7. Document results

**Estimated Time**: 1 week (6 hours implementation + testing)
