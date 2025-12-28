# Socket Path Contract

**Canonical Path**: `/tmp/beamline-gateway.sock`

**Priority Order**:
1. CLI argument (`-s /path/to/socket`)
2. Environment variable `IPC_SOCKET_PATH`
3. Default: `/tmp/beamline-gateway.sock`

**Usage**:
```bash
# Method 1: CLI
./bench-ipc-throughput -s /tmp/beamline-gateway.sock

# Method 2: Environment
export IPC_SOCKET_PATH=/tmp/beamline-gateway.sock
./bench-ipc-throughput

# Method 3: Default (no args)
./bench-ipc-throughput  # uses /tmp/beamline-gateway.sock
```

**All tools must use this contract**:
- `bench_ipc_throughput.c`
- `bench_ipc_latency.c`
- `run_benchmarks.sh`
- `load_test.sh`
- Any future tools
