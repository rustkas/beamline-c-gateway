# IPC Gateway v2.0 - Practical Testing & Deployment Guide

**Last Updated**: 2025-12-26  
**Status**: Production-ready core features with comprehensive testing

---

## Quick Start

### 1. Build Everything

```bash
cd /home/rustkas/aigroup/apps/c-gateway
mkdir -p build && cd build
cmake ..
make

# Key executables will be in build/:
#  - ipc-server-demo (IPC gateway demo)
#  - bench-ipc-throughput (real protocol benchmark)
#  - bench-ipc-latency (real protocol benchmark)
#  - soak-test-buffer-pool (long-running stability test)
#  - soak-test-nats-pool (long-running NATS pool test)
#  - test-* (all unit tests)
```

### 2. Run Unit Tests

```bash
cd build
ctest --output-on-failure

# Or run specific v2.0 tests:
./test-buffer-pool
./test-trace-context
./test-nats-pool
./test-circuit-breaker
./test-audit-log
./test-websocket-gateway
./test-grpc-gateway
```

---

## Performance Testing (REAL PROTOCOL)

### Prerequisites

**Start IPC Gateway**:
```bash
# In terminal 1:
cd build
export IPC_SOCKET_PATH=/tmp/beamline-gateway.sock
./ipc-server-demo $IPC_SOCKET_PATH
```

**Verify Socket**:
```bash
# In terminal 2:
ls -l /tmp/beamline-gateway.sock
# Should show: srwxr-xr-x (socket type)
```

### Run Benchmarks

#### Option A: Automated Suite
```bash
cd benchmarks
./run_benchmarks.sh

# Results will be in:
# results/YYYYMMDD_HHMMSS/
#   ├── throughput.txt
#   ├── latency_64b.txt
#   ├── latency_256b.txt
#   ├── latency_1024b.txt
#   └── summary.md
```

#### Option B: Manual Benchmarks

**Throughput**:
```bash
./build/bench-ipc-throughput \
    -d 30 \              # Duration: 30 seconds
    -t 8 \               # Threads: 8
    -s $IPC_SOCKET_PATH  # Socket path

# Expected output:
# Throughput: 5000-15000 req/sec (depending on hardware)
# Success Rate: >99%
```

**Latency** (multiple payload sizes):
```bash
# Small payload (64 bytes)
./build/bench-ipc-latency -n 10000 -p 64 -s $IPC_SOCKET_PATH

# Typical payload (256 bytes)
./build/bench-ipc-latency -n 10000 -p 256 -s $IPC_SOCKET_PATH

# Large payload (1KB)
./build/bench-ipc-latency -n 10000 -p 1024 -s $IPC_SOCKET_PATH

# Expected p99 latency: <10ms (excellent), <50ms (good)
```

---

## Stability Testing

### Soak Tests (Long-Running)

**Buffer Pool Soak Test**:
```bash
# Run for 5 minutes with 8 threads
./build/soak-test-buffer-pool 300 8

# Expected: 0 leaks, all buffers returned
# Typical rate: 5000-10000 ops/sec
```

**NATS Pool Soak Test**:
```bash
# Run for 5 minutes with 8 threads
./build/soak-test-nats-pool 300 8

# Expected: 0 leaks, all connections returned
# Validates connection reuse, health checks
```

### Sanitizer Tests (Memory Safety)

**Prerequisites**:
```bash
sudo apt-get install valgrind
```

**Run Sanitizers**:
```bash
cd tests
./sanitizer_tests.sh

# Tests:
# 1. Valgrind - memory leak detection
# 2. AddressSanitizer (ASan) - buffer overflows
# 3. ThreadSanitizer (TSan) - race conditions

# Expected: All tests pass, 0 leaks, 0 races
```

---

## E2E Integration Testing

### With NATS Server

**Start NATS**:
```bash
docker run -d --name nats-test -p 4222:4222 nats:latest

# Or if NATS is installed:
nats-server
```

**Run E2E Test**:
```bash
cd tests
./e2e_integration_test.sh

# Tests:
# - IPC socket connectivity
# - Message exchange
# - NATS integration (partial)
# - Health monitoring
# - Graceful shutdown
```

---

## Production Deployment

### Docker

**Build Image**:
```bash
docker build -t ipc-gateway:v2.0 .
```

**Run Container**:
```bash
docker run -d \
    --name ipc-gateway \
    -p 8080:8080 \
    -v /tmp:/tmp \
    ipc-gateway:v2.0
```

**Health Check**:
```bash
curl http://localhost:8080/health
# {"status":"healthy","message":"Alive"}

curl http://localhost:8080/ready
# {"status":"healthy","message":"Ready"}
```

### Kubernetes

**deployment.yaml**:
```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: ipc-gateway
spec:
  replicas: 3
  selector:
    matchLabels:
      app: ipc-gateway
  template:
    metadata:
      labels:
        app: ipc-gateway
    spec:
      containers:
      - name: gateway
        image: ipc-gateway:v2.0
        ports:
        - containerPort: 8080
          name: http
        livenessProbe:
          httpGet:
            path: /health
            port: 8080
          initialDelaySeconds: 10
          periodSeconds: 30
        readinessProbe:
          httpGet:
            path: /ready
            port: 8080
          initialDelaySeconds: 5
          periodSeconds: 10
        resources:
          requests:
            memory: "128Mi"
            cpu: "100m"
          limits:
            memory: "512Mi"
            cpu: "500m"
```

---

## Monitoring

### Prometheus Metrics

**Scrape Config**:
```yaml
scrape_configs:
  - job_name: 'ipc-gateway'
    static_configs:
      - targets: ['localhost:8080']
    metrics_path: '/metrics'
```

**Key Metrics**:
- `ipc_requests_total{type="submit"}` - Total requests
- `ipc_request_duration_seconds_bucket` - Latency histogram
- `ipc_buffer_pool_available` - Available buffers
- `ipc_nats_pool_connections_idle` - Idle NATS connections
- `ipc_circuit_breaker_state` - Circuit breaker status

### Grafana Dashboard

Import pre-built dashboard from `docs/grafana-dashboard.json` (TBD).

---

## Troubleshooting

### Benchmarks Fail to Connect

**Problem**: `ERROR: Cannot connect to /tmp/beamline-gateway.sock`

**Solution**:
1. Check if gateway is running: `ps aux | grep ipc-server`
2. Check socket exists: `ls -l /tmp/beamline-gateway.sock`
3. Verify socket path: `echo $IPC_SOCKET_PATH`
4. Start gateway: `./build/ipc-server-demo /tmp/beamline-gateway.sock`

### Low Throughput

**Problem**: Throughput < 1000 req/sec

**Possible causes**:
1. Gateway not optimized build: rebuild with `-O3`
2. Too few threads: increase `-t` parameter
3. Network latency: check local socket performance
4. CPU throttling: check `cpufreq-info`

### Memory Leaks Detected

**Problem**: Valgrind reports leaks

**Steps**:
1. Run specific test: `valgrind --leak-check=full ./test-buffer-pool`
2. Check test output for leak summary
3. If genuine leak, file issue with stack trace

### Soak Test Fails

**Problem**: Soak test reports leaks after hours

**Investigation**:
1. Check test output: acquired vs released count
2. Run with shorter duration to isolate: `./soak-test-buffer-pool 60 4`
3. Add debug logging if needed
4. Check for race conditions with TSan

---

## Performance Targets

### Throughput
- **Excellent**: ≥10,000 req/sec (multi-threaded)
- **Good**: ≥5,000 req/sec
- **Minimum**: ≥1,000 req/sec

### Latency (p99)
- **Excellent**: ≤10ms
- **Good**: ≤50ms
- **Needs Work**: >50ms

### Memory
- **RSS Baseline**: ~50-100 MB
- **Under Load**: <500 MB for sustained operation
- **Leaks**: 0 bytes leaked

### Stability
- **Soak Test**: Pass 5+ minute test with 0 leaks
- **Uptime**: Target 99.9% availability

---

## Next Steps

### If All Tests Pass ✅

You're ready for production! Consider:
1. Capture baseline metrics
2. Deploy to staging environment
3. Run load tests with production-like traffic
4. Monitor for 24-48 hours
5. Gradual rollout to production

### If Tests Fail ❌

1. Check this troubleshooting guide
2. Review test output carefully
3. Run tests individually to isolate issues
4. Check `.ai/V2_HONEST_STATUS.md` for known limitations
5. File issues with detailed logs

---

## Documentation

- **Architecture**: `docs/ARCHITECTURE.md`
- **API Reference**: `docs/API.md`
- **Benchmark Details**: `benchmarks/BENCHMARK_PLAN.md`
- **v2.0 Status**: `.ai/V2_HONEST_STATUS.md`
- **Fixes Applied**: `.ai/BENCHMARKS_FIXED.md`

---

## Support

For issues or questions:
1. Check existing documentation
2. Review test output
3. Check GitHub issues
4. Contact: [your contact method]

---

**Version**: v2.0  
**Last Tested**: 2025-12-26  
**Status**: ✅ Core features production-ready
