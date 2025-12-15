# Redis Rate Limiter Tests

## Unit Tests

**File**: `redis_rate_limiter_test.c`

**Description**: Unit tests for Redis rate limiter that don't require Redis.

**Run**:
```bash
cd apps/c-gateway/build
make c-gateway-redis-rate-limiter-test
./c-gateway-redis-rate-limiter-test
```

**Tests**:
- Configuration parsing
- Route ID generation
- Circuit breaker state API
- Disabled limiter behavior
- Cleanup

## Integration Tests

**File**: `redis_rate_limiter_integration_test.c`

**Description**: Integration tests that require a running Redis instance.

**Prerequisites**:
- Redis running on `localhost:6379`
- `hiredis` library installed
- Build with `USE_REDIS_FOR_TESTS=ON`

**Run**:
```bash
# Start Redis (if not running)
redis-server

# Build with Redis support
cd apps/c-gateway/build
cmake -DUSE_REDIS_FOR_TESTS=ON ..
make c-gateway-redis-rate-limiter-integration-test

# Run tests
export C_GATEWAY_REDIS_RATE_LIMIT_ENABLED=true
./c-gateway-redis-rate-limiter-integration-test
```

**Tests**:
- Basic rate limiting (allow/deny)
- Circuit breaker fail-open behavior
- Per-route limits

## Running All Tests with CTest

```bash
cd apps/c-gateway/build
cmake -DUSE_REDIS_FOR_TESTS=ON ..
make
ctest -V
```

## Test Coverage

To enable code coverage:

```bash
cd apps/c-gateway/build
cmake -DENABLE_COVERAGE=ON -DUSE_REDIS_FOR_TESTS=ON ..
make
./c-gateway-redis-rate-limiter-test
gcov src/redis_rate_limiter.c
```

