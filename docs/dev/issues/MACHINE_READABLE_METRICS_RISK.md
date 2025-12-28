# CRITICAL ISSUE: Machine-Readable Metrics

**Date**: 2025-12-28T12:40:00+07:00  
**Severity**: P1 (Technical Debt)  
**Issue**: grep -P and stdout-as-API fragility

---

## Current Problem

### Bad Pattern: Regex Parsing of stdout

**Current implementation** (run_benchmarks.sh):
```bash
parse_throughput_metrics() {
    local rps=$(grep -oP 'Throughput:\s+\K[0-9]+' "$file" 2>/dev/null || echo "0")
    local total=$(grep -oP 'Total Requests:\s+\K[0-9]+' "$file" 2>/dev/null || echo "0")
}
```

### Problems

1. **grep -P not portable**:
   - PCRE not available in busybox/alpine
   - CI breaks in minimal containers
   - Deployment environments may fail

2. **stdout becomes API**:
   - Any text change breaks CI
   - "Throughput: 1000" vs "Throughput:  1000" (space!)
   - Versioning stdout format is impossible

3. **Fragile parsing**:
   - Regex can break on edge cases
   - Silent failures (|| echo "0")
   - No validation

---

## Better Solution: JSON Output

### Ideal: Benchmarks Output JSON

**Benchmark responsibility**:
```c
// bench_ipc_throughput.c
void print_results_json() {
    printf("{\"benchmark\":\"throughput_64b\",");
    printf("\"rps\":%lu,", total_rps);
    printf("\"total_requests\":%lu,", total_requests);
    printf("\"duration_s\":%d,", duration);
    printf("\"threads\":%d,", num_threads);
    printf("\"exit_code\":0}\n");
}
```

**Wrapper responsibility**:
```bash
# run_benchmarks.sh - NO REGEX!
./build/bench-ipc-throughput > output.txt 2>&1
EXIT=$?

# Extract JSON line (last line, or specific marker)
RESULT=$(tail -1 output.txt)

# Append to summary
echo "$RESULT" >> "$RESULTS_DIR/metrics.jsonl"
```

**Gate responsibility**:
```bash
# With jq
jq -r '.rps' metrics.jsonl

# Without jq (simple case)
# JSON: {"rps":1000,"exit_code":0}
rps=$(echo "$RESULT" | tr ',' '\n' | grep '"rps"' | cut -d: -f2)
```

---

## Alternative: Benchmarks Write results.json

**Even better**:
```c
// bench_ipc_throughput.c
FILE *f = fopen("throughput_64b.json", "w");
fprintf(f, "{\"benchmark\":\"throughput_64b\",");
fprintf(f, "\"rps\":%lu,", total_rps);
fprintf(f, "\"timestamp\":\"%s\"}\n", timestamp);
fclose(f);
```

**Wrapper**:
```bash
# NO PARSING AT ALL!
./build/bench-ipc-throughput
# Benchmark writes throughput_64b.json

# Collect into summary
cat throughput_*.json latency_*.json > "$RESULTS_DIR/all_metrics.jsonl"
```

---

## Migration Path

### Phase 1: Add JSON Output (non-breaking)

**Benchmarks output BOTH**:
```
Throughput: 1000 req/s    # Human-readable (stdout)
Total Requests: 10000     # Human-readable (stdout)
{"rps":1000,"total":10000}  # Machine-readable (last line)
```

**Wrapper tries JSON first, falls back to grep**:
```bash
# Try JSON (last line)
JSON=$(tail -1 output.txt)
if echo "$JSON" | jq -e . >/dev/null 2>&1; then
    # Valid JSON!
    rps=$(echo "$JSON" | jq -r '.rps')
else
    # Fallback to grep (deprecated)
    rps=$(grep -oP 'Throughput:\s+\K[0-9]+' output.txt || echo "0")
fi
```

### Phase 2: Deprecate grep parsing

**After all benchmarks output JSON**:
- Remove grep -P code
- Require JSON
- Fail if not JSON

### Phase 3: Dedicated results.json files

**Benchmarks write files**:
- `throughput_64b.json`
- `latency_64b.json`
- Wrapper just collects, no parsing

---

## Portable Alternatives to grep -P

### If stuck with text parsing:

**1. Use grep -E (POSIX)**:
```bash
# Instead of: grep -oP 'Throughput:\s+\K[0-9]+'
# Use:
grep -E 'Throughput:.*[0-9]+' | sed 's/.*Throughput:[^0-9]*\([0-9]*\).*/\1/'
```

**2. Use awk (always available)**:
```bash
# More robust than grep
awk '/Throughput:/ {print $2}' output.txt
```

**3. Use sed**:
```bash
sed -n 's/.*Throughput:[^0-9]*\([0-9]*\).*/\1/p' output.txt
```

---

## Strict Format Definition

If using text output, **define strict format**:

### Machine-Readable Section

```
=== METRICS ===
rps=1000
total_requests=10000
duration_s=10
exit_code=0
=== END METRICS ===
```

**Parser** (no regex needed):
```bash
# Extract metrics section
awk '/=== METRICS ===/,/=== END METRICS ===/' output.txt | \
grep -v "===" | \
while IFS='=' read key value; do
    echo "$key: $value"
done
```

---

## Recommended Approach

### Short-term (quick fix):

1. Add JSON output to benchmarks (LAST LINE of stdout)
2. Wrapper tries JSON first, falls back to grep
3. Document that grep -P is deprecated

### Long-term (proper fix):

1. Benchmarks write `<name>.json` files
2. Wrapper collects JSON files (no parsing!)
3. Gate reads JSON with jq (or simple cut/grep on known format)
4. Remove all grep -P / regex parsing

---

## Action Items

### P1 (Should Do):

- [ ] Add JSON output to bench_ipc_throughput.c
- [ ] Add JSON output to bench_ipc_latency.c
- [ ] Add JSON output to bench_memory.c
- [ ] Update run_benchmarks.sh to try JSON first
- [ ] Document grep -P deprecation

### P2 (Nice to Have):

- [ ] Migrate to results.json files
- [ ] Remove all grep -P usage
- [ ] Add JSON schema validation
- [ ] CI check: fail if benchmark doesn't output JSON

---

## Example Implementation

### bench_ipc_throughput.c

```c
// After normal output, print JSON
void print_json_results(unsigned long rps, unsigned long total, int duration) {
    printf("{");
    printf("\"benchmark\":\"throughput_%zub\",", g_payload_size);
    printf("\"rps\":%lu,", rps);
    printf("\"total_requests\":%lu,", total);
    printf("\"duration_s\":%d,", duration);
    printf("\"payload_bytes\":%zu,", g_payload_size);
    printf("\"exit_code\":0");
    printf("}\n");
}

// In main():
print_json_results(rps, total_completed, duration);
return 0;
```

### run_benchmarks.sh

```bash
parse_throughput_json() {
    local file=$1
    local json=$(tail -1 "$file")
    
    # Try JSON first
    if echo "$json" | grep -q '^{.*}$'; then
        # Valid JSON-like (basic check)
        local rps=$(echo "$json" | tr ',' '\n' | grep '"rps"' | cut -d: -f2 | tr -d ' ')
        local total=$(echo "$json" | tr ',' '\n' | grep '"total_requests"' | cut -d: -f2 | tr -d ' ')
        echo "$rps" "$total"
        return 0
    fi
    
    # Fallback to grep (deprecated)
    echo "WARN: No JSON output, using deprecated grep parsing" >&2
    local rps=$(grep -oP 'Throughput:\s+\K[0-9]+' "$file" 2>/dev/null || echo "0")
    echo "$rps" "0"
}
```

---

**Status**: DOCUMENTED ✅  
**Risk**: HIGH (grep -P portability, stdout-as-API)  
**Solution**: JSON output from benchmarks  
**Timeline**: P1 - implement in next iteration
