# Redis Rate Limiting PoC for C-Gateway

**Status**: PoC (Proof of Concept)  
**Target**: CP3+ experiments and load testing  
**Version**: 1.0.0

## Overview

This document describes the Redis-backed rate limiting PoC implementation for C-Gateway. The PoC provides production-ready features including connection pooling, retry logic, and circuit breaker, but is not intended for production use without further validation.

## Features

- **Fixed-window rate limiting** using Redis
- **Connection pooling** for efficient Redis operations
- **Retry logic** with configurable backoff
- **Circuit breaker** with fail-open/fail-closed modes
- **Per-IP and per-route limits**
- **Observability** (metrics and JSON logs)

## Configuration

### Environment Variables

#### Core Configuration

| Variable | Default | Description |
|----------|---------|-------------|
| `C_GATEWAY_REDIS_RATE_LIMIT_ENABLED` | `false` | Enable Redis rate limiting PoC |
| `C_GATEWAY_REDIS_RATE_LIMIT_REDIS_URI` | - | Redis URI (`redis://host:port` or `redis://:password@host:port`) |
| `C_GATEWAY_REDIS_RATE_LIMIT_REDIS_HOST` | `localhost` | Redis hostname (if URI not set) |
| `C_GATEWAY_REDIS_RATE_LIMIT_REDIS_PORT` | `6379` | Redis port (if URI not set) |
| `C_GATEWAY_REDIS_RATE_LIMIT_WINDOW_SEC` | `1` | Fixed window size in seconds |
| `C_GATEWAY_REDIS_RATE_LIMIT_GLOBAL_LIMIT` | `1000` | Global rate limit per window |
| `C_GATEWAY_REDIS_RATE_LIMIT_ROUTE_LIMIT_MESSAGES` | `200` | Limit for `/api/v1/messages` |
| `C_GATEWAY_REDIS_RATE_LIMIT_ROUTE_LIMIT_CHAT` | `100` | Limit for `/api/v1/chat` |

#### Connection Pool Configuration

| Variable | Default | Description |
|----------|---------|-------------|
| `C_GATEWAY_REDIS_RATE_LIMIT_POOL_SIZE` | `32` | Redis connection pool size |
| `C_GATEWAY_REDIS_RATE_LIMIT_POOL_ACQUIRE_TIMEOUT_MS` | `10` | Timeout to acquire connection from pool (ms) |
| `C_GATEWAY_REDIS_RATE_LIMIT_REDIS_TIMEOUT_MS` | `30` | Total timeout for Redis operation including retries (ms) |

#### Retry Configuration

| Variable | Default | Description |
|----------|---------|-------------|
| `C_GATEWAY_REDIS_RATE_LIMIT_RETRIES` | `2` | Number of retries for transient errors |
| `C_GATEWAY_REDIS_RATE_LIMIT_RETRY_BACKOFF_MS` | `5` | Fixed backoff between retries (ms) |

#### Circuit Breaker Configuration

| Variable | Default | Description |
|----------|---------|-------------|
| `C_GATEWAY_REDIS_RATE_LIMIT_CB_MODE` | `fail_open` | Circuit breaker mode: `fail_open` or `fail_closed` |
| `C_GATEWAY_REDIS_RATE_LIMIT_CB_ERROR_THRESHOLD` | `5` | Error count to open circuit breaker |
| `C_GATEWAY_REDIS_RATE_LIMIT_CB_SLIDING_WINDOW_SEC` | `30` | Sliding window for error observation (seconds) |
| `C_GATEWAY_REDIS_RATE_LIMIT_CB_COOLDOWN_SEC` | `15` | Cooldown before half-open transition (seconds) |
| `C_GATEWAY_REDIS_RATE_LIMIT_CB_HALF_OPEN_ATTEMPTS` | `2` | Successful attempts needed to close from half-open |

### Example Configuration

**Basic Setup (Local Redis)**:
```bash
export C_GATEWAY_REDIS_RATE_LIMIT_ENABLED=true
export C_GATEWAY_REDIS_RATE_LIMIT_REDIS_HOST=localhost
export C_GATEWAY_REDIS_RATE_LIMIT_REDIS_PORT=6379
export C_GATEWAY_REDIS_RATE_LIMIT_GLOBAL_LIMIT=1000
export C_GATEWAY_REDIS_RATE_LIMIT_WINDOW_SEC=1
```

**Production-like Setup (with Circuit Breaker)**:
```bash
export C_GATEWAY_REDIS_RATE_LIMIT_ENABLED=true
export C_GATEWAY_REDIS_RATE_LIMIT_REDIS_URI=redis://:password@redis.example.com:6379
export C_GATEWAY_REDIS_RATE_LIMIT_POOL_SIZE=64
export C_GATEWAY_REDIS_RATE_LIMIT_REDIS_TIMEOUT_MS=50
export C_GATEWAY_REDIS_RATE_LIMIT_RETRIES=3
export C_GATEWAY_REDIS_RATE_LIMIT_CB_MODE=fail_open
export C_GATEWAY_REDIS_RATE_LIMIT_CB_ERROR_THRESHOLD=10
```

## Architecture

### Rate Limiting Algorithm

**Fixed Window**:
1. Calculate window start: `floor(current_time / window_sec) * window_sec`
2. Build Redis key: `rl:ip:<route_id>:<client_ip_hash>:<bucket_ts>`
3. Execute Lua script atomically:
   - `INCR key`
   - If `count == 1` → `EXPIRE key window_sec`
   - Return `{count, ttl}`
4. Check limit: if `count > limit` → deny with HTTP 429

### Connection Pooling

- Pool of Redis connections per C-Gateway instance
- Thread-safe connection acquisition/release
- Automatic connection creation on demand
- Connection validation before reuse

### Retry Logic

- Retries on network errors (timeouts, connection failures)
- Fixed backoff between retries
- Total timeout includes all retries
- Non-retryable errors fail immediately

### Circuit Breaker

**States**:
- **CLOSED**: Normal operation, all requests go to Redis
- **OPEN**: Circuit open, failing fast (fail-open allows traffic, fail-closed denies)
- **HALF_OPEN**: Testing if Redis is back, limited requests allowed

**Transitions**:
- `CLOSED → OPEN`: Error count ≥ threshold in sliding window
- `OPEN → HALF_OPEN`: After cooldown period
- `HALF_OPEN → CLOSED`: Successful attempts ≥ threshold
- `HALF_OPEN → OPEN`: Error during half-open test

## Request Flow

```
HTTP Request
    ↓
Backpressure Check (Router status)
    ↓
Redis Rate Limiter Check
    ├─ Circuit Breaker OPEN?
    │   ├─ fail_open → Allow (degraded)
    │   └─ fail_closed → Deny (429)
    ├─ Acquire Connection from Pool
    ├─ Execute Lua Script (INCR + EXPIRE)
    ├─ Check Limit
    │   ├─ Exceeded → Deny (429)
    │   └─ Within Limit → Allow
    └─ Release Connection
    ↓
Abuse Detection
    ↓
Existing Rate Limiter (if Redis disabled)
    ↓
Router Processing
```

## Response Semantics

### HTTP 429 (Rate Limit Exceeded)

**Status**: `429 Too Many Requests`

**Headers**:
```
Retry-After: <seconds>
X-RateLimit-Limit: <limit>
X-RateLimit-Remaining: 0
X-RateLimit-Reset: <epoch_seconds>
```

**Body** (JSON):
```json
{
  "ok": false,
  "error": {
    "code": "rate_limit_exceeded",
    "message": "Too many requests",
    "endpoint": "/api/v1/routes/decide",
    "retry_after_seconds": 1
  }
}
```

## Observability

### Metrics (Prometheus-style)

- `c_gateway_rate_limiter_requests_total{route,scope="ip",status="allowed|limited|error"}`
- `c_gateway_rate_limiter_redis_errors_total{type}`
- `c_gateway_rate_limiter_cb_state{state="closed|open|half_open"}` (gauge)

### Logs (JSON)

**Rate Limit Denied**:
```json
{
  "component": "c-gateway",
  "subsystem": "redis_rate_limiter",
  "level": "info",
  "route_id": "POST_/api/v1/routes/decide",
  "client_ip_hash": "1234567890",
  "limit": 200,
  "count": 201,
  "window_sec": 1,
  "decision": "limited"
}
```

**Circuit Breaker State Change**:
```json
{
  "component": "c-gateway",
  "subsystem": "redis_rate_limiter",
  "level": "warn",
  "cb_old_state": "closed",
  "cb_new_state": "open",
  "error_type": "connection_failed",
  "retries": 2
}
```

## Testing

### Unit Tests

- Redis key generation
- Route ID normalization
- Limit selection logic
- Circuit breaker state transitions
- Retry logic

### Integration Tests

- End-to-end HTTP requests with rate limiting
- Redis outage scenarios (circuit breaker)
- Connection pool exhaustion
- Concurrent requests

### Performance Tests

- Latency impact: p95 ≤ +5ms
- Throughput: minimal degradation
- Connection pool efficiency

## Limitations (PoC)

- **No per-tenant quotas**: Only per-IP and per-route limits
- **No admin API**: No introspection of current limits/state
- **No hybrid cache**: No local cache + Redis hybrid mode
- **No multi-cluster**: Single Redis instance/cluster only
- **Basic Lua script**: Simple fixed-window, no sliding window

## Future Enhancements (CP3+)

- Sliding window algorithm
- Per-tenant quotas
- Admin API for introspection
- Hybrid cache (Redis + local)
- Multi-cluster Redis support
- Advanced metrics and dashboards

## Dependencies

- **hiredis**: Redis client library for C
- **pthread**: Thread-safe connection pool
- **Lua support in Redis**: For atomic operations

## Files

- `apps/c-gateway/include/redis_rate_limiter.h` - Public API
- `apps/c-gateway/src/redis_rate_limiter.c` - Implementation
- `apps/c-gateway/src/http_server.c` - Integration point

## References

- [Redis Rate Limiting Patterns](https://redis.io/docs/manual/patterns/rate-limiting/)
- [Circuit Breaker Pattern](https://martinfowler.com/bliki/CircuitBreaker.html)
- [Fixed Window vs Sliding Window](https://konghq.com/blog/how-to-design-a-scalable-rate-limiting-algorithm)

