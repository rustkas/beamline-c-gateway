# IPC Gateway

High-performance IPC Gateway for Beamline project with NATS integration.

---

## Quick Start

```bash
# Build
make

# Run benchmarks
./benchmarks/run_benchmarks.sh

# Run E2E tests
./tests/run_router_e2e_evidence_pack.sh

# Check production readiness
./.gitlab-ci/check-production-readiness.sh
```

---

## Status

✅ **All 15 Requirements Complete**  
✅ **Production Ready**

See: [STATUS.md](STATUS.md) | [FINAL_PROOF.md](FINAL_PROOF.md)

---

## Key Features

- **Real IPC Protocol**: Length-prefixed framing with JSON payloads
- **NATS Integration**: Reliable message routing
- **Comprehensive Benchmarks**: Latency, throughput, memory profiling
- **E2E Evidence**: Facts-only production readiness gate
- **CI Enforcement**: Automated quality gates

---

## Documentation

### Essential
- [STATUS.md](STATUS.md) - Current project status
- [FINAL_PROOF.md](FINAL_PROOF.md) - Proof of all requirements met
- [PROOF.md](PROOF.md) - Initial critical fixes proof

### Implementation Details
- [docs/implementation/](docs/implementation/) - Task completion, fixes
- [docs/operations/benchmarking.md](docs/operations/benchmarking.md) - Benchmark guide
- [.gitlab-ci/CHECK_TAXONOMY.md](.gitlab-ci/CHECK_TAXONOMY.md) - Gate check taxonomy

### Operations
- [benchmarks/BENCHMARK_PLAN.md](benchmarks/BENCHMARK_PLAN.md) - Benchmarking strategy
- [docs/operations/](docs/operations/) - Deployment, testing guides

---

## Architecture

```
src/
  ├── ipc_protocol.c/h     # Core protocol implementation
  ├── c_gateway.c          # Main gateway
  └── abuse_detection.c    # Rate limiting

benchmarks/
  ├── bench_ipc_latency.c       # Latency measurement
  ├── bench_ipc_throughput.c    # Throughput measurement
  ├── bench_memory.c            # Memory profiling
  └── run_benchmarks.sh         # Orchestration script

tests/
  └── run_router_e2e_evidence_pack.sh  # E2E evidence generation

.gitlab-ci/
  ├── check-production-readiness.sh    # Facts-only gate
  └── CHECK_TAXONOMY.md                # SYS_/INFO_/PERF_ rules
```

---

## Production Readiness

**Gate**: Facts-only, artifact-based

**Enforced by CI**:
- Build verification
- Benchmark suite
- E2E evidence pack
- No subjective claims allowed

**Evidence**: `artifacts/router-e2e/*/checks.tsv`, `summary.json`

---

## License

See [docs/LICENCE.md](docs/LICENCE.md)

---

## Contributing

See [docs/CONTRIBUTING.md](docs/CONTRIBUTING.md)

---

## Security

See [docs/SECURITY.md](docs/SECURITY.md)
