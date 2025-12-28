# Proof Pack Manifest

**Generated**: 2025-12-28T12:11:30+07:00  
**Commit**: 97371dd09ee96704a1bda86ebd9657fc337a0ee1  
**Type**: Cryptographically-Bound Evidence

---

## Contents

### Git Evidence
- `commit_sha.txt` - Exact commit SHA
- `commit_info.txt` - Full commit details
- `diff_stat.txt` - Diff statistics
- `diff_full.txt` - Full diff

### Cryptographic Binding
- `checksums.txt` - SHA256 of all key files + binaries
- `source_97371dd09ee96704a1bda86ebd9657fc337a0ee1.tar.gz` - Exact source code archive
- `source_archive_checksum.txt` - Archive SHA256

### Build Evidence
- `build_commands.txt` - Exact build commands
- `build_*.log` - Build stdout/stderr
- `build_exit_codes.txt` - All build exit codes

### Environment
- `environment.txt` - Build environment details

---

## Verification

### 1. Verify Commit
```bash
git checkout $(cat commit_sha.txt)
```

### 2. Verify Checksums
```bash
sha256sum -c checksums.txt
```

### 3. Extract Source
```bash
tar -xzf source_97371dd09ee96704a1bda86ebd9657fc337a0ee1.tar.gz
cd c-gateway-97371dd09ee96704a1bda86ebd9657fc337a0ee1/
```

### 4. Rebuild
```bash
# Use exact commands from build_commands.txt
gcc -o build/bench-ipc-latency benchmarks/bench_ipc_latency.c src/ipc_protocol.c -I./include -Wall -O2
# ... etc
```

### 5. Compare Binaries
```bash
sha256sum build/bench-ipc-latency
# Should match checksums.txt
```

---

## Binding Proof

This proof pack provides:
- ✅ Exact commit SHA
- ✅ SHA256 of all key files
- ✅ SHA256 of built binaries
- ✅ Exact source archive
- ✅ Build commands and logs
- ✅ Reproducible build environment

**No "trust me" - only cryptographic proof!**
