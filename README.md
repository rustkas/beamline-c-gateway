# IPC Gateway v2.0

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](./LICENCE.md)
[![Status: Staging Ready](https://img.shields.io/badge/Status-Staging%20Ready-brightgreen.svg)]()

High-performance IPC Gateway for beamline scheduling system, providing Unix socket communication with NATS message broker integration.

---

## Overview

The IPC Gateway v2.0 is a C-based gateway service that bridges Unix domain socket (IPC) clients with NATS message broker, enabling efficient request-reply communication for the beamline scheduling Router.

### What's New in v2.0

#### 🔍 Observability & Monitoring
- ✅ Prometheus metrics (`/metrics` endpoint)
- ✅ Health checks (`/health`, `/ready` - Kubernetes-ready)
- ✅ Distributed tracing (trace context propagation)
- ✅ Structured metrics library
- ✅ Audit logging with JSONL format

#### ⚡ Performance & Scalability
- ✅ NATS connection pooling (configurable pool size)
- ✅ Zero-copy buffer pool (pre-allocated buffers)
- ✅ 13,400+ ops/sec sustained throughput
- ✅ Performance benchmarks included
- ✅ Load testing framework

#### 🛡️ Reliability & Resilience
- ✅ Circuit breakers for fault tolerance
- ✅ Enhanced health monitoring
- ✅ Connection pool management
- ✅ Memory-safe (validated with ASan & Valgrind)
- ✅ 2-hour soak test passed (96M operations)

#### 🌐 Platform Support
- ✅ Linux (primary)
- ✅ macOS/BSD support
- ✅ WebSocket gateway (foundation)
- ✅ gRPC gateway (foundation)
- ⏳ TLS/SSL support (planned)
- ⏳ Redis rate limiting (optional backend)

---

## Key Features

| Feature | v1.0 | v2.0 |
|---------|------|------|
| IPC Protocol | ✅ | ✅ + versioning + tracing |
| NATS Bridge | ✅ | ✅ + pooling + resilience |
| Metrics | Basic | ✅ Prometheus |
| Health Checks | ❌ | ✅ K8s-ready |
| Circuit Breakers | ❌ | ✅ |
| Buffer Pool | ❌ | ✅ Zero-copy |
| Performance Tests | ❌ | ✅ Benchmarks + Soak |
| Memory Safety | ❌ | ✅ ASan + Valgrind |

---

## Quick Start

### Prerequisites

- **C Compiler**: GCC 12.3+ or Clang
- **CMake**: 4.1+
- **NATS Server**: Running instance (e.g., `nats-server`)
- **Dependencies**: `libnats`, `pthread`

### Build

```bash
# Clone repository
git clone <repository-url>
cd c-gateway

# Build
mkdir -p build && cd build
cmake ..
make

# Run tests
make test
```

### Run

```bash
# Start IPC Gateway
./build/ipc-server-demo /tmp/beamline-gateway.sock

# In another terminal, test connectivity
echo '{"task_id":"test1","payload":"hello"}' | nc -U /tmp/beamline-gateway.sock

# Check health (if HTTP server enabled)
curl http://localhost:8080/health
curl http://localhost:8080/ready

# View metrics
curl http://localhost:8080/metrics
```

---

## Architecture

```
┌─────────────┐         ┌──────────────┐         ┌────────┐         ┌────────┐
│   Client    │  IPC    │  IPC Gateway │  NATS   │ Router │  NATS   │ Worker │
│             ├────────→│              ├────────→│        ├────────→│        │
│ (Unix Socket)│         │  (This repo) │         │        │         │        │
└─────────────┘         └──────────────┘         └────────┘         └────────┘
```

### Core Components

- **IPC Server**: Unix socket listener and handler
- **Buffer Pool**: Zero-copy pre-allocated buffer management
- **NATS Pool**: Connection pool with health checking and resilience
- **Protocol**: Framed binary protocol with versioning
- **Trace Context**: Distributed tracing support (propagation)
- **Circuit Breaker**: Fault tolerance and graceful degradation
- **Audit Log**: Security and compliance logging
- **Health Checks**: Kubernetes-compatible readiness probes

---

## Testing

### Run Unit Tests

```bash
cd build

# Core component tests
make test-buffer-pool test-nats-pool test-trace-context
./test-buffer-pool
./test-nats-pool
./test-trace-context

# Additional v2.0 tests
./test-circuit-breaker
./test-audit-log
./test-gateway-health
```

### Run Integration Tests

```bash
# Start NATS server
nats-server -p 4222

# Run E2E tests
./tests/e2e_core_validation.sh

# Run local validation
./tests/local_validation.sh
```

### Run Performance Benchmarks

```bash
# Start IPC server first
./build/ipc-server-demo /tmp/beamline-gateway.sock

# Run benchmarks
cd benchmarks
./run_benchmarks.sh

# Load testing
./load_test.sh 60 10  # 60 seconds, 10 concurrent connections
```

### Run Sanitizers

```bash
# Build with AddressSanitizer
mkdir -p build-san && cd build-san
cmake .. -DCMAKE_C_FLAGS="-fsanitize=address -g"
make
./test-buffer-pool

# Run with Valgrind
valgrind --leak-check=full ./test-buffer-pool
```

See [docs/operations/TESTING_GUIDE.md](./docs/operations/TESTING_GUIDE.md) for comprehensive testing documentation.

---

## Configuration

### Environment Variables

```bash
# IPC socket path
export IPC_SOCKET_PATH=/tmp/beamline-gateway.sock

# NATS connection
export NATS_URL=nats://localhost:4222

# NATS pool configuration
export NATS_POOL_MIN_CONNECTIONS=2
export NATS_POOL_MAX_CONNECTIONS=10

# Buffer pool
export BUFFER_POOL_SIZE=32
export BUFFER_SIZE=4096
```

### Buffer Pool Configuration

```c
buffer_pool_config_t config = {
    .initial_count = 32,
    .buffer_size = 4096,
    .max_count = 128
};
```

### NATS Pool Configuration

```c
nats_pool_config_t config = {
    .min_connections = 2,
    .max_connections = 10,
    .nats_url = "nats://localhost:4222",
    .max_reconnect_attempts = 5
};
```

See `include/buffer_pool.h` and `include/nats_pool.h` for full configuration options.

---

## Performance

### Validated Performance (v2.0)

- **Throughput**: 13,400 ops/sec (sustained over 2 hours)
- **Latency**: p50 < 10ms, p99 < 50ms
- **Memory**: Zero leaks (proven over 96M operations)
- **Stability**: 2-hour soak test (96,632,058 operations, 0 leaks)
- **RSS Growth**: 0.00% over 2-hour sustained load
- **FD Leaks**: 0 (monitored every 5 seconds)

### Test Evidence

- **Sanitizers**: ASan (strict mode) + Valgrind on 4 core components
- **Soak Test**: 7,200 seconds, 1,426 monitoring samples
- **Artifacts**: Available in `artifacts/sanitizers/` and `artifacts/soak/`

See [validation reports](./artifacts/) for detailed results.

---

## Production Readiness

### Current Status (Honest Assessment)

**Overall Readiness**: **60-70%**

| Aspect | Confidence | Evidence |
|--------|------------|----------|
| **Core Stability** | 85-90% | ASan, Valgrind, 96M ops, 2h soak ✅ |
| **System Integration** | 30-40% | Mock tests (requires Router E2E) ⚠️ |
| **Overall** | 60-70% | Core proven, staging validation needed |

**Deployment Recommendations**:
- ✅ **Staging**: APPROVED (60-70% is appropriate)
- ❌ **Production**: NOT READY (requires Router E2E in staging)

See [docs/readiness/MANAGEMENT_DECISION.md](./docs/readiness/MANAGEMENT_DECISION.md) for authoritative deployment decision.

### Deployment Path

```
Current → Staging → Production
60-70%  → 80-85%  → 90-95%
```

**Next Steps**:
1. Deploy to staging environment
2. Execute Router E2E (4 critical scenarios)
3. Validate system integration
4. Production deployment (after 80-85% achieved)

**Estimated Timeline**: 3 days (optimistic) to 3 weeks (realistic)

---

## Production Deployment

### Docker

```yaml
services:
  ipc-gateway:
    image: ipc-gateway:v2.0
    ports:
      - "8080:8080"
    environment:
      - IPC_SOCKET_PATH=/tmp/beamline-gateway.sock
      - NATS_URL=nats://nats:4222
    healthcheck:
      test: ["CMD", "curl", "-f", "http://localhost:8080/health"]
      interval: 30s
      timeout: 10s
      retries: 3
```

### Kubernetes

```yaml
apiVersion: v1
kind: Pod
metadata:
  name: ipc-gateway
spec:
  containers:
  - name: gateway
    image: ipc-gateway:v2.0
    ports:
    - containerPort: 8080
    livenessProbe:
      httpGet:
        path: /health
        port: 8080
      initialDelaySeconds: 30
      periodSeconds: 10
    readinessProbe:
      httpGet:
        path: /ready
        port: 8080
      initialDelaySeconds: 5
      periodSeconds: 5
```

---

## Documentation

### Essential Documents
- **[README.md](./README.md)** - This file (project overview)
- **[CONTRIBUTING.md](./CONTRIBUTING.md)** - Contribution guidelines
- **[SECURITY.md](./SECURITY.md)** - Security policy

### Technical Documentation
- **[docs/INDEX.md](./docs/INDEX.md)** - Complete documentation index
- **[docs/readiness/MANAGEMENT_DECISION.md](./docs/readiness/MANAGEMENT_DECISION.md)** - Deployment decision
- **[docs/operations/TESTING_GUIDE.md](./docs/operations/TESTING_GUIDE.md)** - Testing guide
- **[docs/validation/EVIDENCE_PACK_SYSTEM.md](./docs/validation/EVIDENCE_PACK_SYSTEM.md)** - Evidence validation

### Architecture & Design
- `docs/architecture/` - System architecture
- `docs/contracts/` - API contracts
- `docs/DOCUMENTATION_STRUCTURE.md` - Documentation organization rules

---

## Development

### Code Style

- **Language**: C (C11 standard)
- **Formatting**: Consistent style (4 spaces, K&R braces)
- **Safety**: Use `snprintf`, `strncpy` (NO unsafe functions like `strcpy`, `sprintf`)

### Safety Requirements

**REQUIRED**:
- ✅ `snprintf` instead of `sprintf`
- ✅ `strncpy` instead of `strcpy`
- ✅ Check all return values
- ✅ Initialize all variables
- ✅ Free allocated memory

**FORBIDDEN**:
- ❌ `gets`, `strcpy`, `sprintf`, `strcat`

### Contributing

See [CONTRIBUTING.md](./CONTRIBUTING.md) for detailed contribution guidelines.

### Security

Report vulnerabilities privately. See [SECURITY.md](./SECURITY.md) for reporting process.

---

## Statistics (v2.0)

- **Lines of Code**: 5,000+
- **Files Created**: 30+
- **Test Suites**: 17 (all passing)
- **Test Pass Rate**: 100%
- **Core Components**: 8 (buffer pool, NATS pool, trace context, circuit breaker, etc.)
- **External Dependencies**: 0 (for core components)
- **Validation**: ASan, Valgrind, 2-hour soak test
- **Evidence Artifacts**: 150+ files

---

## License

This project is licensed under the MIT License - see [LICENCE.md](./LICENCE.md) file for details.

---

## Support & Contact

- **Issues**: Use GitHub Issues for bug reports
- **Documentation**: See [docs/INDEX.md](./docs/INDEX.md)
- **Validation Reports**: See `artifacts/` directory
- **Security**: See [SECURITY.md](./SECURITY.md)

---

## Acknowledgments

- Built with [NATS.io](https://nats.io) for message broker integration
- Validated with AddressSanitizer and Valgrind
- Tested with extensive soak and integration tests (96M+ operations)
- Inspired by production-grade C systems design

---

**Last Updated**: 2025-12-27  
**Version**: 2.0.0  
**Status**: Staging-ready (60-70% overall, 85-90% core)  
**Next Milestone**: Router E2E in staging → Production ready (80-85%)
