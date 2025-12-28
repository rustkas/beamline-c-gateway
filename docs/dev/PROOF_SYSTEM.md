# Cryptographically-Bound Proof System

**Implemented**: 2025-12-28T12:12:00+07:00  
**Principle**: "исправлено" = git SHA + checksums + reproducible build

---

## Problem

**Old (токсичный)**:
- "Fixed in Step 1583" ❌
- "Trust me" ❌
- "You have old version" ❌

**Gap**: No cryptographic/procedural binding to exact source version!

---

## Solution: Proof-First Culture

### Minimum Binding Requirements

1. ✅ **Git SHA**: `git rev-parse HEAD` in `commit_sha.txt`
2. ✅ **Diff**: `git diff --stat` + `git diff` in artifacts
3. ✅ **SHA256**: Key files (bench_*.c, run_benchmarks.sh, etc.)
4. ✅ **Build Commands**: `gcc ...` commands in `command.txt`
5. ✅ **Build Logs**: stdout/stderr in `build.log`
6. ✅ **Source Archive**: `git archive` as exact version

---

## Implementation

### Script: `scripts/generate_proof_pack.sh`

**Generates**:
```
artifacts/proof-packs/YYYYMMDD_HHMMSS/
├── commit_sha.txt              # Exact commit
├── commit_info.txt             # Full commit details
├── diff_stat.txt               # Git diff stats
├── diff_full.txt               # Full git diff
├── checksums.txt               # SHA256 of ALL key files + binaries
├── source_<SHA>.tar.gz         # Exact source archive
├── source_archive_checksum.txt # Archive SHA256
├── build_commands.txt          # Exact build commands
├── build_*.log                 # Build stdout/stderr
├── build_exit_codes.txt        # All exit codes
├── environment.txt             # Build environment
└── MANIFEST.md                 # How to verify
```

---

## Cryptographic Binding

### Key Files Checksummed
- `benchmarks/bench_ipc_latency.c`
- `benchmarks/bench_ipc_throughput.c`
- `benchmarks/bench_memory.c`
- `benchmarks/run_benchmarks.sh`
- `benchmarks/load_test.sh`
- `benchmarks/check_bench_gate.sh`
- `src/ipc_protocol.c`
- `src/ipc_protocol.h`
- `tests/run_router_e2e_evidence_pack.sh`
- `.gitlab-ci/check-production-readiness.sh`
- `scripts/check_percent_readiness.sh`

### Binaries Checksummed
- `build/bench-ipc-latency`
- `build/bench-ipc-throughput`
- `build/bench-memory`

---

## Verification Process

### 1. Verify Commit
```bash
cd artifacts/proof-packs/<timestamp>/
git checkout $(cat commit_sha.txt)
```

### 2. Verify File Checksums
```bash
sha256sum -c checksums.txt
# Should show OK for all files
```

### 3. Extract and Rebuild
```bash
tar -xzf source_<SHA>.tar.gz
cd c-gateway-<SHA>/
gcc -o build/bench-ipc-latency ... # Use commands from build_commands.txt
```

### 4. Compare Binaries
```bash
sha256sum build/bench-ipc-latency
# Should match checksums.txt binary section
```

---

## Usage

### Generate Proof Pack
```bash
./scripts/generate_proof_pack.sh
```

### CI Integration
```yaml
# .gitlab-ci.yml
proof_pack:
  stage: validate
  script:
    - ./scripts/generate_proof_pack.sh
  artifacts:
    paths:
      - artifacts/proof-packs/
    expire_in: 1 year
```

---

## Trust Model

**Before**: "Trust me, I fixed it"
- No verification possible
- No reproducibility
- Trust based on claims

**After**: Cryptographically verified
- ✅ Exact commit SHA
- ✅ SHA256 of all key files
- ✅ SHA256 of binaries
- ✅ Reproducible from source archive
- ✅ Build commands and logs

**Trust**: Math, not claims!

---

## Example Proof Pack

```bash
./scripts/generate_proof_pack.sh

# Output:
artifacts/proof-packs/20251228_121500/
  commit_sha.txt: 97371dd85d3385787814efcf7de657beeb58ea10
  checksums.txt: 11 key files + 3 binaries
  source_97371dd.tar.gz: Exact source (compressed)
  MANIFEST.md: Verification instructions
```

**Verification**:
```bash
cd artifacts/proof-packs/20251228_121500/
sha256sum -c checksums.txt
# ✓ All OK
```

---

## Benefits

1. **No "old version" arguments** - SHA256 proves exact version
2. **Reproducible** - Anyone can rebuild from source archive
3. **Cryptographic proof** - Math, not trust
4. **CI enforced** - Automated proof generation
5. **Audit trail** - Complete evidence chain

---

## Rules

**New project rule**:

> Any claim "исправлено" MUST include:
> - Commit SHA
> - Proof pack location
> - SHA256 verification
> 
> NO "step numbers"  
> NO "trust me"  
> ONLY cryptographic evidence

---

**Implementation**: ✅ COMPLETE  
**Script**: `scripts/generate_proof_pack.sh`  
**Documentation**: This file  
**Culture**: Proof-first, trust math!
