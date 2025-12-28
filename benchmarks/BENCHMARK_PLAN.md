# IPC Gateway - Performance Benchmark Suite

**Updated**: 2025-12-27  
**Purpose**: Measure IPC protocol performance (latency, throughput, memory)

---

## Protocol Contract (REQUIRED)

All benchmarks MUST:
- ✅ Use `ipc_protocol.h` framing: `[len:4 BE][ver:1][type:1][payload:N]`
- ✅ Connect to canonical socket: `/tmp/beamline-gateway.sock`
- ✅ Support override via `IPC_SOCKET_PATH` env var or `-s` flag
- ✅ Implement `send_all()`/`recv_all()` with EINTR/EAGAIN handling
- ✅ Use `MSG_NOSIGNAL` to avoid SIGPIPE
- ✅ Set `SO_RCVTIMEO`/`SO_SNDTIMEO` timeouts (10 seconds)
- ✅ Include warmup phase (100 requests)
- ✅ Support payload size sweep (64/256/1024 bytes)

---

## Benchmarks

### 1. Throughput (`bench_ipc_throughput.c`)

**Measures**: Sustained requests/second

**Implementation**:
- Multi-threaded (default: 4 threads)
- Uses pthreads for concurrent clients
- Atomic counters for thread-safe metrics
- Runs for fixed duration (default: 10s)

**Parameters**:
- `-d <seconds>`: Duration
- `-t <threads>`: Number of concurrent threads
- `-p <bytes>`: Payload size
- `-s <path>`: Socket path

**Output**:
```
IPC Throughput (real frame)
socket: /tmp/beamline-gateway.sock
payload_bytes: 256
duration_s: 10
ops: 52070
elapsed_s: 10.003
throughput_ops_s: 5203.45
```

---

### 2. Latency (`bench_ipc_latency.c`)

**Measures**: Round-trip latency percentiles

**Implementation**:
- Single-threaded for accurate RTT measurement
- Warmup phase (100 requests)
- Calculates p50/p95/p99 percentiles
- Payload size configurable

**Parameters**:
- `<iterations>`: Number of requests (positional)
- `-p <bytes>`: Payload size
- `-s <path>`: Socket path

**Output**:
```
IPC Latency (real frame)
socket: /tmp/beamline-gateway.sock
payload_bytes: 64
iterations: 10000
avg_us: 123.45
p50_us: 100.23
p95_us: 250.67
p99_us: 450.89
```

---

### 3. Memory (`bench_memory.c`)

**Measures**: RSS/FD stability under IPC load

**Implementation**:
- Sends real IPC requests (uses ipc_protocol)
- Samples RSS/FD count every N requests
- Reports baseline/peak/final
- Detects memory growth

**Parameters**:
- `<socket_path>`: Socket (positional, optional)
- `<requests>`: Number of requests (default: 10000)

**Output**:
```
IPC Memory Benchmark
socket: /tmp/beamline-gateway.sock
requests: 10000

Baseline RSS: 12345 KB (12.1 MB)
Peak RSS: 12600 KB (12.3 MB)
Final RSS: 12400 KB (12.1 MB)
RSS Growth: 255 KB (0.2 MB)

Baseline FDs: 4
Peak FDs: 5
Final FDs: 4
FD Growth: 1
```

---

## Results Structure

**Timestamped artifacts**:
```
results/YYYYMMDD_HHMMSS/
  throughput_64b.txt
  throughput_256b.txt
  throughput_1024b.txt
  latency_64b.txt
  latency_256b.txt
  latency_1024b.txt
  memory.txt
  summary.md
```

**summary.md**: Aggregates all results with analysis

---

## Usage

### Prerequisites

**Start IPC server**:
```bash
# For IPC protocol benchmarks (simple echo):
./build/ipc-server-demo /tmp/beamline-gateway.sock

# For production gateway (HTTP workload):
./build/c-gateway

# For full E2E (IPC + NATS):
./build/ipc-nats-demo /tmp/beamline-gateway.sock 1
```

### Run All Benchmarks

```bash
./benchmarks/run_benchmarks.sh
```

**What it does**:
1. Checks socket exists
2. Runs warmup (100 requests)
3. Runs throughput @ 64/256/1024 bytes
4. Runs latency @ 64/256/1024 bytes
5. Generates timestamped summary

### Run Individual Benchmarks

```bash
# Throughput (10s, 4 threads, 256 bytes)
./build/bench-ipc-throughput -d 10 -t 4 -p 256 -s /tmp/beamline-gateway.sock

# Latency (10000 iterations, 64 bytes)
./build/bench-ipc-latency 10000 -p 64 -s /tmp/beamline-gateway.sock

# Memory (10000 requests)
./build/bench-memory /tmp/beamline-gateway.sock 10000
```

---

## Target Metrics

Based on Unix socket IPC with real protocol framing:

**Throughput**:
- Single thread: > 1,000 req/sec
- 4 threads: > 5,000 req/sec
- Target: > 10,000 req/sec

**Latency (p99)**:
- Excellent: < 1ms
- Good: < 10ms
- Acceptable: < 50ms

**Memory**:
- Baseline: < 20MB
- Under load: < 50MB
- Growth: < 10MB

---

## Build

Benchmarks link against `ipc_protocol`:

```bash
# Using project Makefile
make benchmarks

# Or manually
gcc -O2 -I include -o bench-ipc-latency \
    benchmarks/bench_ipc_latency.c \
    src/ipc_protocol.c

gcc -O2 -pthread -I include -o bench-ipc-throughput \
    benchmarks/bench_ipc_throughput.c \
    src/ipc_protocol.c
```

---

## Notes

- All benchmarks use **real IPC protocol** (not raw JSON)
- Warmup eliminates cold-start bias
- Payload sweep tests realistic workloads
- Results are machine-readable for automation
- Zero dependencies beyond POSIX + project libs

---

**Version**: v2.0 with real protocol  
**Last updated**: 2025-12-27
