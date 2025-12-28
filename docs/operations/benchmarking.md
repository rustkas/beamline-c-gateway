# Benchmarking Guide

**Purpose**: Performance benchmarking for IPC Gateway  
**Last Updated**: 2025-12-28

---

## Binary Matrix

### Which Binary to Benchmark

| Goal | Binary | Purpose | Command Example |
|------|--------|---------|-----------------|
| **IPC Protocol Only** | `ipc-server-demo` | Simple echo server, measures pure IPC performance | `./build/ipc-server-demo /tmp/beamline-gateway.sock` |
| **Production HTTP Gateway** | `c-gateway` | Full HTTP server with IPC backend | `./build/c-gateway` (uses env GATEWAY_PORT) |
| **Full E2E (IPC + NATS)** | `ipc-nats-demo` | IPC server with NATS bridge integration | `./build/ipc-nats-demo /tmp/beamline-gateway.sock 1` |

### Recommendation by Use Case

**For benchmark suite** (`run_benchmarks.sh`):
- Use: `ipc-server-demo`
- Why: Measures IPC protocol layer in isolation
- Pros: Consistent, no HTTP overhead, repeatable
- Cons: Doesn't reflect full production stack

**For production validation**:
- Use: `c-gateway` or `ipc-nats-demo`
- Why: Reflects actual deployment
- Pros: Real-world performance
- Cons: More variables (NATS latency, HTTP parsing)

**For Router E2E tests**:
- Use: `ipc-nats-demo`
- Why: Full integration with Router over NATS
- Best: End-to-end validation

---

## Benchmark Suite

### Available Benchmarks

| Benchmark | Measures | Output |
|-----------|----------|--------|
| `bench-ipc-latency` | Round-trip latency (p50/p95/p99) | latency_<size>b.txt |
| `bench-ipc-throughput` | Sustained requests/second | throughput_<size>b.txt |
| `bench-memory` | RSS/FD stability under load | memory.txt |

### Running Benchmarks

#### Quick Start

```bash
# 1. Start IPC server
./build/ipc-server-demo /tmp/beamline-gateway.sock &

# 2. Run full benchmark suite
./benchmarks/run_benchmarks.sh

# Results in: results/<timestamp>/
```

#### Individual Benchmarks

**Latency**:
```bash
./build/bench-ipc-latency 10000 -p 256 -s /tmp/beamline-gateway.sock

# Options:
#   <iterations>  Number of requests (positional)
#   -p <bytes>    Payload size (default: 64)
#   -s <path>     Socket path (default: /tmp/beamline-gateway.sock)
#   --no-warmup   Skip warmup phase
```

**Throughput**:
```bash
./build/bench-ipc-throughput -d 10 -t 4 -p 256 -s /tmp/beamline-gateway.sock

# Options:
#   -d <seconds>  Duration (default: 10)
#   -t <threads>  Thread count (default: 4)
#   -p <bytes>    Payload size (default: 2)
#   -s <path>     Socket path
```

**Memory**:
```bash
./build/bench-memory /tmp/beamline-gateway.sock 10000

# Arguments:
#   <socket>      Socket path (positional, optional)
#   <requests>    Number of requests (default: 10000)
```

---

## Socket Path Configuration

All benchmarks follow the same priority order:

**Priority**: CLI `-s` > ENV `IPC_SOCKET_PATH` > DEFAULT `/tmp/beamline-gateway.sock`

```bash
# Example 1: Use default
./build/bench-ipc-latency 1000

# Example 2: Override with ENV
export IPC_SOCKET_PATH=/custom/path.sock
./build/bench-ipc-latency 1000

# Example 3: Override with CLI (highest priority)
./build/bench-ipc-latency 1000 -s /override.sock
```

---

## Benchmark Protocol Contract

All benchmarks MUST:

- ✅ Use `ipc_protocol.h` framing: `[len:4 BE][ver:1][type:1][payload:N]`
- ✅ Connect to canonical socket (see priority above)
- ✅ Implement `send_all()`/`recv_all()` with EINTR/EAGAIN handling
- ✅ Use `MSG_NOSIGNAL` to avoid SIGPIPE
- ✅ Set `SO_RCVTIMEO`/`SO_SNDTIMEO` timeouts (10 seconds)
- ✅ Include warmup phase (100 requests)
- ✅ Support payload size sweep (64/256/1024 bytes)

---

## Artifacts Structure

### Benchmark Results

```
results/YYYYMMDD_HHMMSS/
  throughput_64b.txt        # Multi-threaded throughput @ 64 bytes
  throughput_256b.txt
  throughput_1024b.txt
  latency_64b.txt           # Latency percentiles @ 64 bytes
  latency_256b.txt
  latency_1024b.txt
  summary.md                # Human-readable summary
  summary.json              # Machine-readable (gate_pass: bool)
  exit_codes.tsv            # Per-benchmark exit codes
  meta.env                  # Environment metadata
  meta.git                  # Git commit hash
```

### Router E2E Results

```
artifacts/router-e2e/YYYYMMDD_HHMMSS/
  checks.tsv                # Machine-readable facts (SYS_*)
  summary.json              # Gate status + metrics
  client.jsonl              # Per-request data
  router.log                # Router process log
  gateway.log               # Gateway process log
  meta.env, meta.git, meta.versions, command.txt
```

---

## Interpreting Results

### Throughput

**Target**: >= 10,000 req/sec (4 threads)  
**Minimum Acceptable**: >= 1,000 req/sec  

```
Duration: 10 seconds, Threads: 4
Total Requests: 52,070
Throughput: 5,207 req/sec  ← Compare to target
```

### Latency

**Excellent**: p99 < 1ms  
**Good**: p99 < 10ms  
**Acceptable**: p99 < 50ms  

```
Latency Results:
  p50: 100µs
  p95: 250µs
  p99: 450µs  ← Key metric
```

### Memory

**Baseline**: < 20MB  
**Under Load**: < 50MB  
**Growth**: < 10MB  

```
Baseline RSS: 12.3 MB
Peak RSS: 13.1 MB
RSS Growth: 0.8 MB  ← Should be minimal
```

---

## CI Integration

### GitLab CI Jobs

**`router_e2e_evidence`**: Generates evidence pack  
**`production_readiness_gate`**: Blocks deployment if FAIL  
**`benchmark_regression`**: Checks p95 < 10ms  

### Local CI Simulation

```bash
# Generate evidence
./tests/run_router_e2e_evidence_pack.sh

# Run CI guard (facts-only)
./.gitlab-ci/check-production-readiness.sh

# Check exit code
echo $?  # 0 = PASS, 1 = FAIL
```

---

## Troubleshooting

### "Socket not found"

```bash
# Check socket exists
ls -la /tmp/beamline-gateway.sock

# Start server if missing
./build/ipc-server-demo /tmp/beamline-gateway.sock &

# Or use custom path
export IPC_SOCKET_PATH=/custom.sock
./build/ipc-server-demo $IPC_SOCKET_PATH &
```

### "Connection refused"

```bash
# Check server is running
ps aux | grep ipc-server-demo

# Check socket permissions
ls -la /tmp/beamline-gateway.sock

# Try kill and restart
pkill ipc-server-demo
./build/ipc-server-demo /tmp/beamline-gateway.sock &
```

### "Timeout"

- Increase timeout in benchmark code (SO_RCVTIMEO/SO_SNDTIMEO)
- Check server isn't overloaded
- Check for network issues (should be Unix socket = local only)

---

## Best Practices

### 1. Isolate Benchmarks
- Run on dedicated machine
- Close other applications
- Disable power management

### 2. Consistent Environment
- Same socket path across runs
- Same payload sizes
- Same thread count

### 3. Multiple Runs
- Run 3-5 times
- Report median/average
- Flag anomalies

### 4. Version Control
- Tag benchmark commits
- Save results with git hash
- Compare across versions

---

## Advanced: Custom Benchmarks

### Creating a New Benchmark

```c
#include "ipc_protocol.h"

#define DEFAULT_SOCKET_PATH "/tmp/beamline-gateway.sock"

int main(int argc, char *argv[]) {
    // 1. Parse args (CLI > ENV > default)
    char socket_path[256] = DEFAULT_SOCKET_PATH;
    if (argc > 1) {
        strncpy(socket_path, argv[1], sizeof(socket_path) - 1);
        socket_path[sizeof(socket_path) - 1] = '\0';
    } else {
        const char *env = getenv("IPC_SOCKET_PATH");
        if (env && env[0]) {
            strncpy(socket_path, env, sizeof(socket_path) - 1);
            socket_path[sizeof(socket_path) - 1] = '\0';
        }
    }
    
    // 2. Connect
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    // ... connect to socket_path
    
    // 3. Warmup
    for (int i = 0; i < 100; i++) {
        // send request, recv response
    }
    
    // 4. Measure
    // ...
    
    return 0;
}
```

---

**See Also**:
- `benchmarks/BENCHMARK_PLAN.md` - Implementation plan
- `.gitlab-ci/README.md` - CI enforcement
- `tests/run_router_e2e_evidence_pack.sh` - E2E runner
