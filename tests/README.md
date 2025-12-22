# Gateway Tests

## Overview

This directory contains unit and integration tests for the Gateway component.

## Test Infrastructure

### Unity Test Framework

Gateway tests use **Unity** test framework (minimal header-only implementation).

**Location**: `tests/unity/unity.h`

**Full Unity Framework**: Can be added as git submodule later:
```bash
git submodule add https://github.com/ThrowTheSwitch/Unity tests/unity
```

### Test Structure

```
tests/
├── unity/
│   └── unity.h              # Unity test framework header
├── test_observability.c     # Observability unit tests
├── test_health_endpoint.c   # Health endpoint integration tests
├── c-gateway-json-test.c   # JSON parsing tests
├── c-gateway-router-test.c # Router status mapping tests
├── c-gateway-nats-test.c   # NATS stub tests
├── c-gateway-http-test.c   # HTTP integration tests
└── nats-router-smoke-test.c # NATS ↔ Router smoke test
```

## Building Tests

### Using CMake

```bash
cd apps/c-gateway
mkdir -p build
cd build
cmake ..
make c-gateway-observability-test
make c-gateway-health-test
```

### Using Makefile

```bash
cd apps/c-gateway

# Build and run all tests
make test

# Build and run observability tests only
make test-observability

# Build and run health endpoint tests only
make test-health

# Build and run performance tests only
make test-performance

# Build and run tests with code coverage
make test-coverage
make coverage-report
```

## Running Tests

### Observability Tests

```bash
cd apps/c-gateway/build
./c-gateway-observability-test
```

**Tests included**:
- Log format JSON structure validation
- Required fields validation
- CP1 fields at top level validation
- ISO 8601 timestamp format validation
- All log levels (ERROR, WARN, INFO, DEBUG)
- PII filtering validation
- Edge case tests (very long messages, special characters, large context objects)

### Health Endpoint Tests

```bash
cd apps/c-gateway/build
./c-gateway-health-test
```

**Tests included**:
- HTTP status code 200 OK
- Valid JSON response format
- Required fields (status, timestamp)
- ISO 8601 timestamp format
- Content-Type header validation
- Response format validation

### Performance Tests

```bash
cd apps/c-gateway/build
./c-gateway-performance-test
```

**Tests included**:
- Log generation throughput (logs/second)
- PII filtering latency (time per log entry)
- JSON serialization performance
- Memory usage during logging

**Note**: Performance tests measure metrics but don't fail on slow systems. They provide baseline measurements for performance monitoring.

## Test Coverage

### Observability Tests (`test_observability.c`)

| Test | Description |
|------|-------------|
| `test_log_format_json_structure` | Validates CP1-compliant JSON log structure |
| `test_log_required_fields` | Validates required fields (timestamp, level, component, message) |
| `test_cp1_fields_at_top_level` | Validates CP1 fields (tenant_id, trace_id, run_id) are at top level |
| `test_iso8601_timestamp_format` | Validates ISO 8601 timestamp format with microseconds (6 digits) |
| `test_all_log_levels` | Validates all log levels (ERROR, WARN, INFO, DEBUG) |
| `test_pii_filtering_sensitive_fields` | Validates PII filtering concept |
| `test_log_context_object` | Validates context object structure |
| `test_error_logging_format` | Validates ERROR level log format |
| `test_warn_logging_format` | Validates WARN level log format |
| `test_debug_logging_format` | Validates DEBUG level log format |
| `test_log_minimal_format` | Validates minimal log format (no optional fields) |
| `test_very_long_message` | Edge case: Very long log messages |
| `test_very_long_cp1_fields` | Edge case: Very long CP1 field values |
| `test_empty_null_cp1_fields` | Edge case: Empty/null CP1 fields |
| `test_special_characters` | Edge case: Special characters (JSON escaping) |
| `test_very_large_context` | Edge case: Very large context objects |

**Total**: 16 tests

### Health Endpoint Tests (`test_health_endpoint.c`)

| Test | Description |
|------|-------------|
| `test_health_endpoint_status_200` | Validates HTTP status code 200 OK |
| `test_health_endpoint_valid_json` | Validates response is valid JSON |
| `test_health_endpoint_required_fields` | Validates required fields (status, timestamp) |
| `test_health_endpoint_iso8601_timestamp` | Validates ISO 8601 timestamp format |
| `test_health_endpoint_content_type` | Validates Content-Type header |
| `test_health_endpoint_format_validation` | Validates response format structure |

**Total**: 10 tests

### Performance Tests (`test_performance.c`)

| Test | Description |
|------|-------------|
| `test_log_generation_performance` | Measures log generation throughput (logs/second) |
| `test_pii_filtering_performance` | Measures PII filtering latency (time per log entry) |
| `test_json_serialization_performance` | Measures JSON serialization performance |
| `test_memory_usage_during_logging` | Verifies memory usage during logging (no leaks) |

**Total**: 4 tests

## Test Examples

### Writing a New Test

Here's an example of how to write a new test using Unity framework:

```c
#include "unity.h"
#include <jansson.h>

/* Test helper: Parse JSON from string */
static json_t *parse_json_string(const char *json_str) {
    json_error_t error;
    json_t *root = json_loads(json_str, 0, &error);
    return root;
}

/* Test: Validate log entry structure */
void test_my_new_feature(void) {
    const char *log_str = 
        "{"
        "\"timestamp\":\"2025-01-27T12:00:00.123456Z\","
        "\"level\":\"INFO\","
        "\"component\":\"gateway\","
        "\"message\":\"Test message\""
        "}";
    
    json_t *log_entry = parse_json_string(log_str);
    TEST_ASSERT_NOT_NULL(log_entry);
    
    /* Your test assertions here */
    json_t *level = json_object_get(log_entry, "level");
    TEST_ASSERT_NOT_NULL(level);
    TEST_ASSERT_EQUAL_STRING("INFO", json_string_value(level));
    
    json_decref(log_entry);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_my_new_feature);
    return UNITY_END();
}
```

### Test Patterns and Best Practices

1. **Use helper functions**: Create reusable helper functions for common operations (JSON parsing, validation, etc.)

2. **Clean up resources**: Always free JSON objects and strings after use:
   ```c
   json_t *obj = json_object();
   char *str = json_dumps(obj, JSON_COMPACT);
   // ... use obj and str ...
   free(str);
   json_decref(obj);
   ```

3. **Test edge cases**: Include tests for boundary conditions, empty values, null values, very long strings

4. **Use descriptive test names**: Test function names should clearly describe what they test

5. **Group related tests**: Keep related tests in the same test file

## Debugging Tests

### How to Debug Failing Tests

1. **Run tests with verbose output**:
   ```bash
   cd apps/c-gateway/build
   ./c-gateway-observability-test
   ```

2. **Use debugger (GDB)**:
   ```bash
   gdb ./c-gateway-observability-test
   (gdb) break test_my_new_feature
   (gdb) run
   ```

3. **Add printf statements**:
   ```c
   void test_my_new_feature(void) {
       printf("Debug: Starting test\n");
       // ... test code ...
       printf("Debug: Test completed\n");
   }
   ```

4. **Check JSON parsing errors**:
   ```c
   json_error_t error;
   json_t *root = json_loads(json_str, 0, &error);
   if (!root) {
       printf("JSON Error: %s at line %d, column %d\n",
              error.text, error.line, error.column);
   }
   ```

### Common Test Issues

**Issue**: Test passes locally but fails in CI

**Solution**: 
- Check for platform-specific differences (endianness, timezone)
- Verify all dependencies are installed in CI environment
- Check for race conditions in multi-threaded tests

**Issue**: Memory leaks detected

**Solution**:
- Use valgrind to detect leaks: `valgrind --leak-check=full ./c-gateway-observability-test`
- Ensure all `json_t*` objects are freed with `json_decref()`
- Ensure all `malloc()`/`json_dumps()` strings are freed with `free()`

**Issue**: Tests timeout

**Solution**:
- Check for infinite loops
- Verify performance tests have reasonable iteration counts
- Use timeouts for external dependencies

## Test Coverage

### Current Coverage Metrics

**Observability Tests**: 16 tests covering:
- ✅ Log format validation
- ✅ CP1 compliance
- ✅ PII filtering
- ✅ Edge cases
- ✅ All log levels

**Health Endpoint Tests**: 10 tests covering:
- ✅ HTTP response format
- ✅ JSON structure
- ✅ Required fields
- ✅ Timestamp format

**Performance Tests**: 4 tests covering:
- ✅ Log generation throughput
- ✅ PII filtering latency
- ✅ JSON serialization
- ✅ Memory usage

### Coverage Goals

- **Line Coverage**: Target 80%+ for observability code
- **Branch Coverage**: Target 70%+ for critical paths
- **Function Coverage**: Target 90%+ for public APIs

### How to Improve Coverage

1. **Run coverage analysis**:
   ```bash
   cd apps/c-gateway
   make test-coverage
   make coverage-report
   ```

2. **View coverage report**:
   ```bash
   open build/coverage_html/index.html
   ```

3. **Identify uncovered code**: Look for `.gcov` files or HTML report

4. **Add tests for uncovered paths**: Write tests that exercise uncovered code branches

5. **Re-run coverage**: Verify coverage improved after adding tests

### Coverage Tools

- **gcov**: GCC coverage tool (built-in)
- **lcov**: Coverage report generator
- **genhtml**: HTML report generator (part of lcov)

**Installation**:
```bash
# Ubuntu/Debian
sudo apt-get install lcov

# macOS
brew install lcov
```

## Dependencies

### Required Libraries

- **jansson**: JSON parsing and validation
- **pthread**: Threading support (for Gateway)

### System Requirements

- C11 compiler (GCC or Clang)
- CMake 3.15 or later
- Make (optional, for Makefile wrapper)

## CI/CD Integration

Tests are integrated with CMake's CTest:

```bash
cd apps/c-gateway/build
cmake ..
ctest
```

Or using Makefile:

```bash
cd apps/c-gateway
make test
```

## Future Improvements

1. **Full Unity Framework**: Replace minimal header with full Unity git submodule
2. **Mocking**: Add mocking support for HTTP server functions
3. **Coverage**: ✅ Code coverage reporting available with `ENABLE_COVERAGE=ON`
4. **Integration Tests**: Add tests that require Gateway to be running
5. **Performance Tests**: ✅ Performance benchmarks added for logging and health endpoint
6. **Concurrent Logging Tests**: Add thread safety tests for concurrent logging
7. **Memory Exhaustion Tests**: Add tests for memory limit scenarios

## Troubleshooting

### Tests Fail to Compile

**Error**: `unity.h: No such file or directory`

**Solution**: Ensure Unity header is in `tests/unity/unity.h`:
```bash
ls -la apps/c-gateway/tests/unity/unity.h
```

### Tests Fail to Link

**Error**: `undefined reference to 'jansson'`

**Solution**: Install jansson library:
```bash
# Ubuntu/Debian
sudo apt-get install libjansson-dev

# macOS
brew install jansson
```

### Tests Pass but No Output

**Solution**: Tests should print test results. Check that Unity macros are working correctly.

## References

- [Unity Test Framework](https://github.com/ThrowTheSwitch/Unity)
- [Gateway Observability Documentation](../docs/OBSERVABILITY.md) (when created)
- [Gateway Test Tasks](../../../docs/archive/dev/GATEWAY_OBSERVABILITY_TASKS.md)

