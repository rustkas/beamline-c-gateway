# Evidence Pack - Definition of Done

**Type**: Facts-only specification  
**Version**: 1.0

---

## STRUCTURE (MANDATORY)

```
artifacts/<test-type>/<timestamp>/
├── meta/
│   ├── commit.txt           # git commit hash
│   ├── git_status.txt       # git status --porcelain
│   ├── uname.txt            # uname -a
│   ├── versions.txt         # tool versions
│   └── env.txt              # environment sorted
├── setup/                   # service logs
│   ├── nats.log
│   ├── router.log
│   └── gateway.log
├── tests/
│   └── <scenario>/
│       ├── command.txt      # exact command
│       ├── stdout.log       # stdout
│       ├── stderr.log       # stderr
│       └── exit_code.txt    # exit code number
└── SUMMARY.json             # machine-readable results
```

---

## SUMMARY.json SCHEMA

```json
{
  "commit": "string (git hash)",
  "router_commit": "string (optional)",
  "timestamp": "ISO8601 UTC",
  "scenarios": {
    "<scenario_name>": {
      "executed": boolean,
      "exit_code": integer
    }
  }
}
```

**Required scenarios for Router E2E**:
- happy_path
- errors
- timeouts
- backpressure
- reconnect

---

## CALCULATION RULES

**System Score**:
```
passed = count(scenarios where executed=true AND exit_code=0)
total = count(scenarios)
system_score = passed / total
```

**Production Gate**:
```
IF passed == total AND total >= 5:
  production_approved = true
ELSE:
  production_approved = false
```

---

## VALIDATION

**Evidence pack is VALID if**:
✅ SUMMARY.json exists
✅ All required scenarios present
✅ meta/commit.txt not empty
✅ At least one setup/*.log exists

**Evidence pack is INVALID if**:
❌ No SUMMARY.json
❌ SUMMARY.json parse error
❌ Missing required scenario
❌ commit.txt empty or missing

---

## READINESS FROM EVIDENCE

**ONLY sources of truth**:
1. SUMMARY.json (scenarios)
2. setup/*.log (non-empty = services ran)
3. meta/commit.txt (reproducibility)

**NEVER sources**:
❌ Markdown documents
❌ Subjective assessments
❌ Verbal descriptions

---

## SCRIPTS

**Create**: `scripts/create_evidence_pack.sh`  
**Calculate**: `scripts/calculate_readiness.sh <pack-dir>`

**Output**: JSON only

---

**Status**: CANONICAL  
**Enforcement**: Automatic via scripts
