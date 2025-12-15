# Gateway Admin gRPC API

**Version**: 1.0  
**Date**: 2025-01-27  
**Status**: CP2-LC  
**Component**: C-Gateway (`apps/c-gateway/`)

## Purpose

This document defines the Gateway Admin gRPC API contract, including health, status, authentication, and metrics endpoints. The admin API provides operational control and monitoring capabilities for Gateway.

## API Overview

### Admin Functions

The Gateway Admin API provides the following functions:

| Function | Purpose | Auth Required |
|----------|---------|---------------|
| `admin_init()` | Initialize admin module | No |
| `admin_health()` | Get Gateway health status | No |
| `admin_status()` | Get Gateway status information | No |
| `admin_authorize()` | Authorize admin action | Yes (API key) |
| `admin_get_metrics()` | Get Prometheus metrics | Yes (API key) |
| `admin_metrics_inc()` | Increment admin metric | Internal |
| `admin_trace_ctx()` | Create trace context | No |

## Function Specifications

### `admin_init()`

**Purpose**: Initialize admin gRPC module and metrics.

**Returns**: `admin_resp_t` with `code = 0` on success.

**Implementation**:
- Initializes admin metrics (`gateway_admin_requests_total`, `gateway_admin_errors_total`)
- Sets up internal state
- Returns success response

**Example**:
```c
admin_resp_t resp = admin_init();
assert(resp.code == 0);
```

### `admin_health()`

**Purpose**: Get Gateway health status.

**Returns**: `admin_resp_t` with:
- `code = 200`: Gateway is healthy
- `code = 503`: Gateway is unhealthy
- `message`: Status string ("healthy" or "unhealthy")

**Health Checks**:
1. NATS connection status (must be "connected")
2. HTTP server status (assumed healthy if function is callable)
3. Memory status (basic check)

**Example**:
```c
admin_resp_t health = admin_health();
if (health.code == 200) {
    // Gateway is healthy
}
```

**Synchronization with HTTP Health Endpoint**:
- HTTP `GET /_health` uses same health check logic
- Both endpoints return consistent status

### `admin_status()`

**Purpose**: Get Gateway status information.

**Returns**: `admin_resp_t` with:
- `code = 200`: Success
- `message`: JSON string with status information

**Status Information**:
```json
{
  "status": "operational",
  "nats": "connected",
  "timestamp": 1706356800
}
```

**Example**:
```c
admin_resp_t status = admin_status();
// Parse status.message as JSON
```

### `admin_authorize(api_key, role, action)`

**Purpose**: Authorize admin action based on API key, role, and action.

**Parameters**:
- `api_key`: API key for authentication (required)
- `role`: User role ("admin", "operator", "viewer")
- `action`: Action to authorize ("restart", "config", "metrics", "health", "status")

**Returns**: `true` if authorized, `false` otherwise.

**Authorization Matrix**:

| Role | health | status | metrics | config | restart |
|------|--------|--------|---------|--------|---------|
| admin | ✅ | ✅ | ✅ | ✅ | ✅ |
| operator | ✅ | ✅ | ✅ | ❌ | ❌ |
| viewer | ❌ | ❌ | ✅ | ❌ | ❌ |

**API Key Validation**:
- Checks `GATEWAY_ADMIN_API_KEY` environment variable
- Falls back to test key for development (not for production)

**Example**:
```c
bool authorized = admin_authorize("test_key_123", "admin", "restart");
assert(authorized == true);
```

**Negative Scenarios**:
- Invalid API key → `false`
- NULL parameters → `false`
- Unknown tenant → `false` (future enhancement)
- Insufficient role permissions → `false`

### `admin_get_metrics(buffer, buffer_size)`

**Purpose**: Get Prometheus metrics in text format.

**Parameters**:
- `buffer`: Output buffer for metrics
- `buffer_size`: Buffer size

**Returns**: Number of bytes written, or `-1` on error.

**Format**: Prometheus text format (RFC 4180)

**Example**:
```c
char metrics[1024];
int written = admin_get_metrics(metrics, sizeof(metrics));
assert(written > 0);
```

**Synchronization with HTTP Metrics Endpoint**:
- HTTP `GET /metrics` uses same metrics export logic
- Both endpoints return same Prometheus format

### `admin_metrics_inc(name)`

**Purpose**: Increment admin metric counter (internal use).

**Parameters**:
- `name`: Metric name ("gateway_admin_requests_total", "gateway_admin_errors_total")

**Example**:
```c
admin_metrics_inc("gateway_admin_requests_total");
```

### `admin_trace_ctx(trace_id, span_id, tenant_id)`

**Purpose**: Create trace context for admin operations.

**Parameters**:
- `trace_id`: Trace ID (optional, can be NULL)
- `span_id`: Span ID (optional, can be NULL)
- `tenant_id`: Tenant ID (optional, can be NULL)

**Returns**: `admin_ctx_t` with trace context (empty strings if NULL).

**Example**:
```c
admin_ctx_t ctx = admin_trace_ctx("trace_123", "span_456", "tenant_789");
```

## Integration with Router Admin API

**Gateway Admin API** (this document):
- Local Gateway health, status, auth, metrics
- Gateway-specific operational control

**Router Admin API** (`router_admin_grpc.erl`):
- Router health via `GetValidatorsHealth`
- Router status via `GetCheckpointStatus`
- Policy management (gRPC-only)

**Relationship**:
- Gateway admin API is **independent** from Router admin API
- Gateway can call Router admin API via gRPC client (future enhancement)
- Both APIs follow similar patterns but serve different components

## Error Handling

### Authorization Errors

**Invalid API Key**:
- `admin_authorize()` returns `false`
- Metric `gateway_admin_errors_total` incremented

**Insufficient Permissions**:
- `admin_authorize()` returns `false`
- Metric `gateway_admin_errors_total` incremented

### Health Check Errors

**NATS Not Connected**:
- `admin_health()` returns `code = 503`
- `message = "unhealthy"`

**Service Unavailable**:
- `admin_health()` returns `code = 503`
- `message = "unhealthy"`

## Metrics

### Admin Metrics

**Counters**:
- `gateway_admin_requests_total`: Total admin API requests
- `gateway_admin_errors_total`: Total admin API errors
- `gateway_admin_health_total`: Total health checks (with success/failure label)
- `gateway_admin_status_total`: Total status checks
- `gateway_admin_auth_total`: Total authorization checks (with success/failure label)
- `gateway_admin_metrics_total`: Total metrics exports

**Labels** (future enhancement):
- `role`: User role (admin, operator, viewer)
- `action`: Action type (health, status, metrics, config, restart)
- `status`: Success or failure

## Testing

### Test Coverage

**Positive Scenarios**:
- ✅ Initialization succeeds
- ✅ Health check returns 200 when healthy
- ✅ Status check returns operational status
- ✅ Authorization succeeds with valid API key and role
- ✅ Metrics export returns valid Prometheus format
- ✅ Trace context creation works

**Negative Scenarios**:
- ✅ Invalid API key → authorization fails
- ✅ NULL parameters → authorization fails
- ✅ Insufficient role permissions → authorization fails
- ✅ Invalid buffer → metrics export fails
- ✅ NULL trace context → empty strings returned

**Deterministic Behavior**:
- ✅ Same inputs produce same outputs
- ✅ No random values
- ✅ No time-dependent failures
- ✅ CI-friendly (no flaky tests)

### Test File

**Location**: `apps/c-gateway/tests/admin_grpc_test.c`

**Test Cases**:
1. `test_init`: Initialization
2. `test_health`: Health check (positive)
3. `test_status`: Status check (positive)
4. `test_authorize_admin`: Authorization (admin role)
5. `test_authorize_operator`: Authorization (operator role)
6. `test_authorize_viewer`: Authorization (viewer role)
7. `test_authorize_invalid_key`: Authorization (invalid API key)
8. `test_authorize_null_params`: Authorization (NULL parameters)
9. `test_metrics_inc`: Metrics increment
10. `test_get_metrics`: Get metrics (positive)
11. `test_get_metrics_invalid_buffer`: Get metrics (invalid buffer)
12. `test_trace_ctx`: Trace context creation
13. `test_trace_ctx_null`: Trace context with NULL values
14. `test_deterministic`: Deterministic behavior verification

## Synchronization with Documentation

### API Contracts

**Reference**: `docs/API_CONTRACTS.md`

**Gateway Admin API**:
- Health endpoint: `GET /_health` (HTTP) ↔ `admin_health()` (gRPC helper)
- Metrics endpoint: `GET /metrics` (HTTP) ↔ `admin_get_metrics()` (gRPC helper)
- Status endpoint: `GET /_status` (HTTP, if exists) ↔ `admin_status()` (gRPC helper)

### PROTO_NATS_MAPPING

**Reference**: `docs/ARCHITECTURE/PROTO_NATS_MAPPING.md`

**Note**: Gateway Admin API is **local to Gateway** and does not use NATS. Router Admin API uses NATS for some operations, but Gateway Admin API is independent.

**Future Enhancement**: Gateway Admin API could call Router Admin API via gRPC client for Router health/status.

## Implementation Notes

### Current Implementation

**File**: `apps/c-gateway/src/admin_grpc.c`

**Status**: CP2 implementation with:
- ✅ Real health check logic (NATS connection)
- ✅ Real status information (JSON format)
- ✅ Real authorization (API key + role-based)
- ✅ Real metrics export (Prometheus format)
- ✅ Trace context support

### Future Enhancements

1. **gRPC Server Integration**:
   - Add gRPC server for admin API (separate from HTTP)
   - Expose admin functions via gRPC protocol

2. **Router Integration**:
   - Call Router Admin API via gRPC client
   - Aggregate Gateway + Router health/status

3. **Enhanced Metrics**:
   - Add labels to metrics (role, action, status)
   - Add histogram metrics for latency

4. **Tenant Validation**:
   - Add tenant validation to authorization
   - Support tenant-specific admin operations

## References

- `docs/API_CONTRACTS.md` - API contracts specification
- `docs/ARCHITECTURE/PROTO_NATS_MAPPING.md` - Proto/NATS mapping
- `docs/CP2_CHECKLIST.md` - CP2 checklist (Admin gRPC task)
- `apps/otp/router/src/router_admin_grpc.erl` - Router Admin API (reference)
- `apps/c-gateway/src/admin_grpc.c` - Implementation
- `apps/c-gateway/tests/admin_grpc_test.c` - Tests

