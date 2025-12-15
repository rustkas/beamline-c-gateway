# Gateway Distributed Rate Limiting

**Status**: ✅ **Production Ready**  
**Purpose**: Distributed rate limiting for Gateway using Redis backend  
**Related**: `docs/ARCHITECTURE/gateway-distributed-rate-limiting.md`

## Overview

This PoC demonstrates distributed rate limiting for Gateway using Redis as the shared state backend. It provides:
- Abstract rate limiter interface (supports memory and Redis backends)
- Redis backend implementation with graceful degradation
- Multi-instance testing infrastructure
- Feature toggle for gradual migration

## Quick Start

### Prerequisites

1. **Redis**: Install Redis server or use Docker
   ```bash
   docker run -d -p 6379:6379 redis:7-alpine
   ```

2. **hiredis** (optional, for full Redis support):
   ```bash
   # Debian/Ubuntu
   apt-get install libhiredis-dev
   
   # macOS
   brew install hiredis
   ```

### Build

**With hiredis** (full Redis support):
```bash
cd apps/c-gateway
mkdir build && cd build
cmake .. -DUSE_REDIS=ON
make
```

**Without hiredis** (fallback to memory mode):
```bash
cd apps/c-gateway
mkdir build && cd build
cmake ..
make
```

### Run Tests

**Single Instance Test**:
```bash
# Set environment variables
export GATEWAY_DISTRIBUTED_RATE_LIMIT_ENABLED=true
export GATEWAY_RATE_LIMIT_BACKEND=redis
export GATEWAY_RATE_LIMIT_REDIS_HOST=localhost
export GATEWAY_RATE_LIMIT_REDIS_PORT=6379

# Run test
./build/test_rate_limiter_distributed
```

**Multi-Instance Test**:
```bash
# Start infrastructure
docker-compose -f ../../docker-compose.rate-limit-test.yml up -d

# Wait for services
sleep 5

# Run test script
bash ../../scripts/test_distributed_rate_limiting.sh

# Cleanup
docker-compose -f ../../docker-compose.rate-limit-test.yml down
```

## Configuration

### Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `GATEWAY_DISTRIBUTED_RATE_LIMIT_ENABLED` | `false` | Enable distributed rate limiting |
| `GATEWAY_RATE_LIMIT_BACKEND` | `memory` | Backend: `redis` or `memory` |
| `GATEWAY_RATE_LIMIT_REDIS_HOST` | `localhost` | Redis host |
| `GATEWAY_RATE_LIMIT_REDIS_PORT` | `6379` | Redis port |
| `GATEWAY_RATE_LIMIT_REDIS_TIMEOUT_MS` | `1000` | Redis connection timeout |
| `GATEWAY_RATE_LIMIT_FALLBACK_TO_LOCAL` | `true` | Fallback to memory mode if Redis unavailable |
| `GATEWAY_RATE_LIMIT_ROUTES_DECIDE_LIMIT` | `50` | Rate limit for `/api/v1/routes/decide` |
| `GATEWAY_RATE_LIMIT_TTL_SECONDS` | `60` | Rate limit window size (seconds) |

### Example Configuration

**CP1 Mode (Memory)**:
```bash
export GATEWAY_DISTRIBUTED_RATE_LIMIT_ENABLED=false
# or
export GATEWAY_RATE_LIMIT_BACKEND=memory
```

**CP2 Mode (Redis)**:
```bash
export GATEWAY_DISTRIBUTED_RATE_LIMIT_ENABLED=true
export GATEWAY_RATE_LIMIT_BACKEND=redis
export GATEWAY_RATE_LIMIT_REDIS_HOST=localhost
export GATEWAY_RATE_LIMIT_REDIS_PORT=6379
export GATEWAY_RATE_LIMIT_FALLBACK_TO_LOCAL=true
```

## Architecture

### Rate Limiter Interface

**Abstract Interface**:
```c
typedef struct rate_limiter {
    int (*init)(rate_limiter_t *self, const distributed_rl_config_t *config);
    rl_result_t (*check)(rate_limiter_t *self, 
                         rl_endpoint_id_t endpoint,
                         const char *tenant_id,
                         const char *api_key,
                         unsigned int *remaining_out);
    void (*cleanup)(rate_limiter_t *self);
    void *internal;
} rate_limiter_t;
```

**Backend Implementations**:
- `rate_limiter_memory.c` - CP1 in-memory implementation
- `rate_limiter_redis.c` - CP2 Redis backend (production-ready)

### Redis Backend Algorithm

**Fixed-Window Rate Limiting**:
1. Calculate window start: `floor(current_time / window_size) * window_size`
2. Build Redis key: `rate_limit:{endpoint}:{tenant_id}:{api_key}:{window_start}`
3. Atomic increment: `INCR key`
4. Set TTL on first increment: `EXPIRE key (window_size + 10)`
5. Check limit: `if count > limit then reject`

**Example**:
```redis
INCR rate_limit:routes_decide:tenant_123::1701000000
EXPIRE rate_limit:routes_decide:tenant_123::1701000000 70
```

## Testing

### Unit Tests

**Location**: `apps/c-gateway/tests/test_rate_limiter_distributed.c`

**Test Cases**:
1. Memory mode (CP1) - backward compatibility
2. Redis mode (CP2) - single instance
3. Multi-instance consistency
4. Fallback to memory mode

**Run**:
```bash
cd apps/c-gateway/build
make test_rate_limiter_distributed
./test_rate_limiter_distributed
```

### Integration Tests

**Infrastructure**: `docker-compose.rate-limit-test.yml`
- 1 Redis instance
- 3 Gateway instances (ports 8081, 8082, 8083)

**Test Script**: `scripts/test_distributed_rate_limiting.sh`

**Run**:
```bash
docker-compose -f docker-compose.rate-limit-test.yml up -d
bash scripts/test_distributed_rate_limiting.sh
docker-compose -f docker-compose.rate-limit-test.yml down
```

### Manual Testing

**Test Single Instance**:
```bash
# Send 15 requests (limit: 10)
for i in {1..15}; do
    curl -X POST http://localhost:8081/api/v1/routes/decide \
        -H "Content-Type: application/json" \
        -H "X-Tenant-ID: test-tenant" \
        -d '{"version":"1","tenant_id":"test-tenant","request_id":"req-'$i'","task":{"type":"text.generate","payload":{}}}'
    echo ""
done
```

**Test Multi-Instance**:
```bash
# Send requests round-robin to 3 instances
for i in {1..15}; do
    PORT=$((8081 + (i % 3)))
    curl -X POST http://localhost:${PORT}/api/v1/routes/decide \
        -H "Content-Type: application/json" \
        -H "X-Tenant-ID: test-tenant" \
        -d '{"version":"1","tenant_id":"test-tenant","request_id":"req-'$i'","task":{"type":"text.generate","payload":{}}}'
    echo ""
done
```

**Check Redis**:
```bash
redis-cli KEYS "rate_limit:*"
redis-cli GET "rate_limit:routes_decide:test-tenant::1701000000"
```

## Integration with HTTP Server

### Current Status

**Implementation Status**: ✅ **Fully integrated with HTTP server**

**Configuration**: ✅ **Environment variables supported**

### Integration Steps

1. **Initialize rate limiter at startup**:
   ```c
   static rate_limiter_t *g_rate_limiter = NULL;
   
   void http_server_init(void) {
       distributed_rl_config_t config;
       rate_limiter_parse_config(&config);
       g_rate_limiter = rate_limiter_create(&config);
   }
   ```

2. **Replace `rate_limit_check()` calls**:
   ```c
   // Old:
   int result = rate_limit_check(RL_ENDPOINT_ROUTES_DECIDE, NULL, &remaining);
   
   // New:
   rl_result_t result = g_rate_limiter->check(g_rate_limiter,
                                              RL_ENDPOINT_ROUTES_DECIDE,
                                              tenant_id,
                                              NULL,
                                              &remaining);
   ```

3. **Cleanup on shutdown**:
   ```c
   void http_server_cleanup(void) {
       if (g_rate_limiter) {
           rate_limiter_destroy(g_rate_limiter);
       }
   }
   ```

## Configuration

### Environment Variables

The following environment variables control distributed rate limiting:

| Variable | Default | Description |
|----------|---------|-------------|
| `GATEWAY_DISTRIBUTED_RATE_LIMIT_ENABLED` | `false` | Enable distributed rate limiting (`true`/`1` to enable) |
| `GATEWAY_RATE_LIMIT_BACKEND` | `memory` | Backend: `redis` or `memory` |
| `GATEWAY_RATE_LIMIT_REDIS_HOST` | `localhost` | Redis host |
| `GATEWAY_RATE_LIMIT_REDIS_PORT` | `6379` | Redis port |
| `GATEWAY_RATE_LIMIT_REDIS_TIMEOUT_MS` | `1000` | Redis connection timeout (milliseconds) |
| `GATEWAY_RATE_LIMIT_FALLBACK_TO_LOCAL` | `true` | Fallback to memory mode if Redis unavailable |
| `GATEWAY_RATE_LIMIT_ROUTES_DECIDE_LIMIT` | `50` | Rate limit for `/api/v1/routes/decide` |
| `GATEWAY_RATE_LIMIT_MESSAGES` | `100` | Rate limit for `/api/v1/messages/*` |
| `GATEWAY_RATE_LIMIT_REGISTRY_BLOCKS` | `200` | Rate limit for `/api/v1/registry/blocks/*` |
| `GATEWAY_RATE_LIMIT_TTL_SECONDS` | `60` | Rate limit window size (seconds) |

### Example Configuration

**CP1 Mode (Memory - Default)**:
```bash
export GATEWAY_DISTRIBUTED_RATE_LIMIT_ENABLED=false
# or
export GATEWAY_RATE_LIMIT_BACKEND=memory
```

**CP2 Mode (Redis - Distributed)**:
```bash
export GATEWAY_DISTRIBUTED_RATE_LIMIT_ENABLED=true
export GATEWAY_RATE_LIMIT_BACKEND=redis
export GATEWAY_RATE_LIMIT_REDIS_HOST=localhost
export GATEWAY_RATE_LIMIT_REDIS_PORT=6379
export GATEWAY_RATE_LIMIT_FALLBACK_TO_LOCAL=true
```

## Future Enhancements

1. **Local Cache**: Implement hybrid mode (local cache + Redis) for better performance
2. **Connection Pooling**: Redis connection pool for better performance
3. **Enhanced Monitoring**: Metrics for Redis queries, fallback usage, latency
4. **Per-Tenant Limits**: Tenant-specific rate limits (CP2+ feature)

## References

- **Architecture**: `docs/ARCHITECTURE/gateway-distributed-rate-limiting.md`
- **Migration Plan**: `docs/ARCHITECTURE/gateway-rate-limiting-migration-plan.md`
- **PoC Report**: `docs/dev/GATEWAY_DISTRIBUTED_RATE_LIMITING_POC_REPORT.md`
- **hiredis**: https://github.com/redis/hiredis

