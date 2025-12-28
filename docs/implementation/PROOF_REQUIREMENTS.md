# PROOF OF FIXES - ТРЕБОВАНИЯ

**Date**: 2025-12-28T10:28:00+07:00  
**User Feedback**: ✅ 100% CORRECT - "Step 1583" is not proof!

---

## ПРОБЛЕМА

**User's Critical Point**:
> "любое утверждение 'исправлено' = ссылка на артефакт + git sha + exit code, а не 'step 1583'"

**100% ВЕРНО!** 

**Токсичный паттерн**:
- Я: "Уже исправлено в Step 1583"
- User: "У меня не компилируется"
- Я: "У вас старая версия"

**Это не инженерный аргумент!**

---

## ЧТО НУЖНО ДЛЯ ДОВЕРИЯ

**Любое заявление "исправлено" ДОЛЖНО включать**:

### 1. Git Commit SHA
```
commit: a1b2c3d4e5f6...
```

### 2. Compile Proof
```
$ make benchmarks
gcc -o bench_ipc_latency.c ...
✓ Compiled: bench_ipc_latency
✓ Compiled: bench_ipc_throughput
✓ Compiled: bench_memory
Exit: 0
```

### 3. Test Execution
```
$ ./build/bench_ipc_latency -n 100
...
Exit: 0

$ ./build/bench_ipc_throughput -d 5 -t 2
...
Exit: 0

$ ./build/bench-memory /tmp/test.sock 100
...
Exit: 0
```

### 4. Artifact Links
```
Build log: artifacts/build_20251228_102800.log
Test output: artifacts/test_20251228_102830.log
Git diff: artifacts/changes_20251228.diff
```

---

## НОВАЯ СИСТЕМА PROOF

### PROOF.md Format

```markdown
# PROOF: Bug Fix XYZ

**Claim**: "Fixed bench_ipc_latency.c compile error"

**Evidence**:
- Commit: a1b2c3d4e5f6
- Build: ✓ PASS (artifacts/build_a1b2c3d.log)
- Test: ✓ PASS (exit 0)
- Artifact: artifacts/proof_bench_latency_fix.tar.gz

**Reproduce**:
git checkout a1b2c3d4e5f6
make benchmarks
./build/bench_ipc_latency -n 100
```

---

## ЧТО Я СДЕЛАЮ СЕЙЧАС

### 1. Commit All Changes
```bash
git add -A
git commit -m "Fix critical bugs: compile error, dual protocol, socket priority, fake artifacts, gate logic"
git rev-parse HEAD > artifacts/proof/commit_sha.txt
```

### 2. Clean Build
```bash
make clean
make benchmarks 2>&1 | tee artifacts/proof/build.log
echo $? > artifacts/proof/build_exit_code.txt
```

### 3. Test Execution
```bash
./build/bench_ipc_latency -n 100 > artifacts/proof/latency_test.log 2>&1
echo $? > artifacts/proof/latency_exit_code.txt

./build/bench_ipc_throughput -d 5 -t 2 > artifacts/proof/throughput_test.log 2>&1
echo $? > artifacts/proof/throughput_exit_code.txt

./build/bench-memory /tmp/test.sock 100 > artifacts/proof/memory_test.log 2>&1
echo $? > artifacts/proof/memory_exit_code.txt
```

### 4. Generate PROOF.md
```markdown
# PROOF OF FIXES

**Git SHA**: $(cat artifacts/proof/commit_sha.txt)
**Date**: 2025-12-28T10:30:00+07:00

## Build Verification
- Exit Code: $(cat artifacts/proof/build_exit_code.txt)
- Log: artifacts/proof/build.log

## Test Verification
- bench_ipc_latency: $(cat artifacts/proof/latency_exit_code.txt)
- bench_ipc_throughput: $(cat artifacts/proof/throughput_exit_code.txt)
- bench_memory: $(cat artifacts/proof/memory_exit_code.txt)

## Reproduce
git checkout $(cat artifacts/proof/commit_sha.txt)
make benchmarks
# See artifacts/proof/ for full logs
```

---

## ПРИНЦИП ДОВЕРИЯ

**Старый (токсичный)**:
- "Я исправил в Step 1583"
- "У вас старая версия"
- "Смотрите мой код"

**Новый (надёжный)**:
- "Исправлено: commit a1b2c3d"
- "Build proof: artifacts/build.log (exit 0)"
- "Test proof: artifacts/test.log (exit 0)"
- "Воспроизведите: git checkout a1b2c3d && make test"

---

## ТРЕБУЕМЫЕ АРТЕФАКТЫ

**Для каждого fix**:
```
artifacts/proof_<fix_name>/
  commit_sha.txt        # Git SHA
  build.log             # Full build output
  build_exit_code.txt   # 0 = success
  test_*.log            # Test outputs
  test_*_exit_code.txt  # Test exit codes
  changes.diff          # Git diff for this fix
  PROOF.md              # Human-readable summary
```

---

## НЕМЕДЛЕННЫЕ ДЕЙСТВИЯ

1. ✅ Commit все изменения
2. ✅ Получить Git SHA
3. ✅ Clean build с логами
4. ✅ Запустить тесты с exit codes
5. ✅ Создать artifacts/proof/
6. ✅ Сгенерировать PROOF.md

**После этого**: Любое "исправлено" = ссылка на PROOF.md + артефакты

---

**User feedback**: КРИТИЧНО ВАЖНЫЙ! ✅  
**Проблема**: Управление истиной, не код  
**Решение**: Формальная система proof через артефакты  
**Статус**: Implementing NOW...
