# Observability (CP1 → CP2/Pre‑release)

This document describes the observability features for Gateway in CP1, including structured JSON logging and health endpoints.

CP2/Pre‑release extends Gateway observability with OpenTelemetry tracing (W3C context, OTLP), Prometheus `/metrics` on port 3001, and Grafana dashboards with CP1 filters. See `docs/dev/CP2_OBSERVABILITY_PLAN.md` and `docs/OBSERVABILITY_CP2_TEST_PROFILE.md`.

## Structured JSON Logging

Gateway uses structured JSON logging for all log entries. Logs are written to standard error (stderr) in JSON format, one entry per line (JSONL format).

### Log Format

Each log entry is a JSON object with the following structure:

```json
{
  "timestamp": "2025-01-27T12:00:00.123456Z",
  "level": "INFO",
  "component": "gateway",
  "message": "Request processed successfully",
  "tenant_id": "tenant_123",
  "run_id": "run_789",
  "trace_id": "trace_456",
  "context": {
    "stage": "http_request",
    "method": "POST",
    "path": "/api/v1/messages",
    "status_code": 200,
    "latency_ms": 150,
    "request_id": "req_123"
  }
}
```

### Required Fields

- **timestamp**: ISO-8601 timestamp in UTC (format: `YYYY-MM-DDTHH:MM:SS.ssssssZ` with 6 digits for microseconds)
- **level**: Log level (`ERROR`, `WARN`, `INFO`, `DEBUG`)
- **component**: Component identifier (always `"gateway"` for Gateway component)
- **message**: Human-readable log message

### Optional Fields

#### CP1 Correlation Fields

CP1 correlation fields are placed at the **top level** of the log entry (not in a nested object):

- **tenant_id**: Tenant identifier (when available)
- **run_id**: Run identifier for workflow runs (when available)
- **trace_id**: Trace identifier for distributed tracing (when available)

All CP1 fields are optional. They are only included when the corresponding context is available.

#### Context

- **context**: Additional structured context (JSON object)
  - **stage**: Processing stage (e.g., `"http_request"`, `"nats_request"`, `"rate_limit_check"`)
  - **method**: HTTP method (e.g., `"GET"`, `"POST"`)
  - **path**: HTTP path (e.g., `"/api/v1/messages"`)
  - **status_code**: HTTP status code (integer)
  - **latency_ms**: Request latency in milliseconds (integer)
  - **request_id**: Request identifier (when available)
  - **error_code**: Error code string (for ERROR level logs)
  - **error_message**: Error message (for ERROR level logs)

### Log Levels

- **ERROR**: Critical errors requiring immediate attention
  - Example: Invalid request format, NATS connection failures, internal errors
- **WARN**: Warnings, potential issues
  - Example: Rate limit approaching, deprecated API usage
- **INFO**: Informational messages (requests, responses, state changes)
  - Example: Request processed successfully, health check passed
- **DEBUG**: Detailed debugging information
  - Example: Request parsing details, internal state transitions

### PII Filtering

All log entries are automatically filtered for PII (Personally Identifiable Information) and secrets:

**Filtered Fields**:
- `password`, `api_key`, `secret`, `token`, `access_token`, `refresh_token`
- `authorization`, `credit_card`, `ssn`, `email`, `phone`
- Header-like fields: `bearer`, `x-api-key`, `x-auth-token`, `x-authorization` (case-insensitive)

**Replacement**: All PII fields are replaced with `"[REDACTED]"` before logging.

**Filtering Method**: Recursive filtering of JSON objects by field name (case-insensitive). The filtering is applied to:
- Message strings (keyword-based filtering)
- Context objects (recursive JSON filtering)
- All nested JSON structures

### Log Output Location

Logs are written to:
- **stderr**: All log levels (ERROR, WARN, INFO, DEBUG)
- Format: JSONL (one JSON object per line)
- Each log entry is a single JSON object, compact format (no extra whitespace)

For production, logs should be redirected to files or log aggregation systems:

```bash
# Redirect stderr to log file
./c-gateway 2> gateway.log

# Or use systemd/journald for log management
```

### Usage Examples

#### Basic Logging

Gateway logging functions are called internally during request processing. Logs are automatically generated for:

- HTTP request processing
- NATS communication
- Rate limiting checks
- Error handling

#### Logging with CP1 Fields

When CP1 correlation fields are available (from request headers or context), they are automatically included in log entries:

```json
{
  "timestamp": "2025-01-27T12:00:00.123456Z",
  "level": "INFO",
  "component": "gateway",
  "message": "Request processed successfully",
  "tenant_id": "tenant_123",
  "trace_id": "trace_abc123",
  "run_id": "run_789",
  "context": {
    "stage": "http_request",
    "method": "POST",
    "path": "/api/v1/messages",
    "status_code": 200,
    "latency_ms": 150
  }
}
```

#### Error Logging

Error logs include error codes and error messages in the context:

```json
{
  "timestamp": "2025-01-27T12:00:00.123456Z",
  "level": "ERROR",
  "component": "gateway",
  "message": "Invalid request format",
  "tenant_id": "tenant_123",
  "trace_id": "trace_abc123",
  "context": {
    "stage": "http_request",
    "error_code": "E_INVALID_REQUEST",
    "error_message": "Invalid JSON payload",
    "method": "POST",
    "path": "/api/v1/messages"
  }
}
```

#### Warning Logging

Warning logs indicate potential issues:

```json
{
  "timestamp": "2025-01-27T12:00:00.123456Z",
  "level": "WARN",
  "component": "gateway",
  "message": "Rate limit approaching",
  "tenant_id": "tenant_123",
  "context": {
    "stage": "rate_limit_check",
    "remaining": 5,
    "limit": 100
  }
}
```

#### Debug Logging

Debug logs provide detailed information for troubleshooting:

```json
{
  "timestamp": "2025-01-27T12:00:00.123456Z",
  "level": "DEBUG",
  "component": "gateway",
  "message": "Request parsing details",
  "context": {
    "stage": "http_request",
    "method": "POST",
    "path": "/api/v1/messages",
    "content_length": 1024
  }
}
```

## Health Endpoints

Gateway provides health checking via HTTP endpoint.

### HTTP Health Endpoint

**Path**: `GET /_health`

**Port**: 8080 (default, configurable via `GATEWAY_PORT` environment variable)

**Authentication**: Not required (public endpoint)

**Response Format**:

```json
{
  "status": "healthy",
  "timestamp": "2025-01-27T12:00:00.123456Z"
}
```

**Response Fields**:
- **status**: Health status (always `"healthy"` when endpoint is accessible)
- **timestamp**: ISO-8601 timestamp in UTC (format: `YYYY-MM-DDTHH:MM:SS.ssssssZ` with 6 digits for microseconds)

**HTTP Status Codes**:
- `200 OK`: Service is healthy
- `503 Service Unavailable`: Service is unhealthy (not currently implemented)

**Content-Type**: `application/json`

### Health Check Usage

#### Using curl

```bash
# Basic health check
curl http://localhost:8080/_health

# With JSON formatting
curl http://localhost:8080/_health | jq

# Expected output:
# {
#   "status": "healthy",
#   "timestamp": "2025-01-27T12:00:00.123456Z"
# }
```

#### Using test script

Gateway provides a test script for observability validation:

```bash
# Run observability tests
bash scripts/observability/test_gateway_observability.sh

# With custom port
GATEWAY_PORT=8080 bash scripts/observability/test_gateway_observability.sh
```

See [Gateway Observability Test Documentation](../../../docs/dev/GATEWAY_OBSERVABILITY_TEST.md) for details.

### Health Endpoint Implementation

The health endpoint is implemented in `src/http_server.c`:

- **Function**: `handle_health(int client_fd)`
- **Response**: CP1-compliant JSON format
- **Timestamp**: Generated using `get_iso8601_timestamp()` with microsecond precision

## CP1 Compliance

Gateway observability is fully CP1-compliant:

### ✅ CP1 Requirements Met

1. **Structured JSON Logging**: ✅ All logs are structured JSON
2. **ISO 8601 Timestamps**: ✅ Timestamps use ISO 8601 format with microsecond precision (6 digits)
3. **CP1 Fields at Top Level**: ✅ CP1 correlation fields (tenant_id, run_id, trace_id) are at top level
4. **PII Filtering**: ✅ All PII and secrets are filtered before logging
5. **Health Endpoint**: ✅ HTTP health endpoint with CP1-compliant format
6. **All Log Levels**: ✅ ERROR, WARN, INFO, DEBUG levels supported

### CP1 Field Mapping

| CP1 Field | Gateway Field | Location | Notes |
|-----------|--------------|----------|-------|
| tenant_id | tenant_id | Top level | From request headers or context |
| run_id | run_id | Top level | From request headers or context |
| trace_id | trace_id | Top level | From request headers or context |
| flow_id | N/A | N/A | Not applicable for Gateway |
| step_id | N/A | N/A | Not applicable for Gateway |

## Testing

### Unit Tests

Gateway observability includes comprehensive unit tests:

```bash
cd apps/c-gateway
make test-observability
```

**Test Coverage**:
- Log format JSON structure validation
- Required fields validation
- CP1 fields at top level validation
- ISO 8601 timestamp format validation
- All log levels (ERROR, WARN, INFO, DEBUG)
- PII filtering validation
- Context object structure validation
- Error logging format validation
- Warning logging format validation
- Debug logging format validation
- Minimal log format validation

See `tests/test_observability.c` for details.

### Integration Tests

Health endpoint integration tests:

```bash
cd apps/c-gateway
make test-health
```

**Test Coverage**:
- HTTP status code 200 OK
- Valid JSON response format
- Required fields (status, timestamp)
- ISO 8601 timestamp format
- Content-Type header validation
- Response format validation
- Optional fields support
- Status values validation
- Timestamp precision (microseconds)
- JSON compactness

See `tests/test_health_endpoint.c` for details.

### E2E Test Script

Gateway provides an E2E test script for real HTTP testing:

```bash
bash scripts/observability/test_gateway_observability.sh
```

See [Gateway Observability Test Documentation](../../../docs/dev/GATEWAY_OBSERVABILITY_TEST.md) for details.

## Local Development

### Viewing Logs

Gateway logs are written to stderr. To view logs during development:

```bash
# Run Gateway and view logs
./build/c-gateway 2>&1 | jq

# Or redirect to file
./build/c-gateway 2> gateway.log
tail -f gateway.log | jq
```

### Testing Health Endpoint

```bash
# Start Gateway
./build/c-gateway &

# Test health endpoint
curl http://localhost:8080/_health | jq

# Expected output:
# {
#   "status": "healthy",
#   "timestamp": "2025-01-27T12:00:00.123456Z"
# }
```

### Testing Observability

```bash
# Run all observability tests
cd apps/c-gateway
make test

# Or run specific test suites
make test-observability
make test-health
```

## MVP Scope

### ✅ Included in CP1 (MVP)

- Structured JSON logging (CP1-compliant format)
- ISO 8601 timestamps with microsecond precision
- CP1 correlation fields at top level
- PII/secret filtering (recursive JSON filtering)
- All log levels (ERROR, WARN, INFO, DEBUG)
- HTTP health endpoint (`GET /_health`)
- Unit tests for observability
- Integration tests for health endpoint
- E2E test script
- Comprehensive documentation

### ❌ Excluded from CP1 (Future Iterations)

- Prometheus metrics (planned for CP2)
- OpenTelemetry tracing (planned for CP2)
- Grafana dashboards (planned for CP2)
- Log aggregation (Loki) (planned for CP2)
- Alerting (Alertmanager) (planned for CP2)
- Distributed tracing (planned for CP2)

## Troubleshooting

### Logs Not Appearing

**Problem**: Logs are not visible in console.

**Solution**: Gateway logs are written to stderr. Ensure stderr is not redirected:

```bash
# Correct: Logs visible
./build/c-gateway

# Incorrect: Logs hidden
./build/c-gateway > /dev/null

# Correct: Redirect stderr to file
./build/c-gateway 2> gateway.log
```

### Health Endpoint Not Responding

**Problem**: Health endpoint returns connection refused.

**Solution**: 
1. Verify Gateway is running: `ps aux | grep c-gateway`
2. Check port configuration: `GATEWAY_PORT=8080 ./build/c-gateway`
3. Verify port is not in use: `netstat -tuln | grep 8080`

### PII Not Filtered

**Problem**: Sensitive data appears in logs.

**Solution**: 
1. Verify PII filtering is enabled (default: enabled)
2. Check that field names match filtered patterns (case-insensitive)
3. Verify recursive filtering is working for nested JSON objects

### Timestamp Format Issues

**Problem**: Timestamps are not in ISO 8601 format.

**Solution**: 
1. Verify `get_iso8601_timestamp()` function is used
2. Check that microseconds are included (6 digits)
3. Ensure UTC timezone (`Z` suffix)

## Best Practices

### When to Use Each Log Level

**ERROR**: Use for critical errors that require immediate attention:
- Invalid request format
- NATS connection failures
- Internal server errors
- Authentication failures

**WARN**: Use for warnings and potential issues:
- Rate limit approaching
- Deprecated API usage
- Configuration issues
- Performance degradation

**INFO**: Use for informational messages:
- Request processed successfully
- Health check passed
- State changes
- Normal operation events

**DEBUG**: Use for detailed debugging information:
- Request parsing details
- Internal state transitions
- Detailed context information
- Development and troubleshooting

### How to Structure Context Objects

**Best Practices**:
- Keep context objects small and focused
- Use consistent field names across logs
- Include relevant identifiers (request_id, trace_id)
- Avoid duplicating information already in top-level fields

**Example**:
```json
{
  "timestamp": "2025-01-27T12:00:00.123456Z",
  "level": "INFO",
  "component": "gateway",
  "message": "Request processed successfully",
  "tenant_id": "tenant_123",
  "trace_id": "trace_abc",
  "context": {
    "stage": "http_request",
    "method": "POST",
    "path": "/api/v1/messages",
    "status_code": 200,
    "latency_ms": 45
  }
}
```

### PII Filtering Guidelines

**Always filter sensitive data**:
- Never log passwords, API keys, or tokens
- Use PII filtering for all user input
- Verify filtered fields match patterns (case-insensitive)
- Test PII filtering in development

**Filtered patterns** (case-insensitive):
- `password`, `api_key`, `secret`, `token`
- `access_token`, `refresh_token`, `authorization`
- `credit_card`, `ssn`, `email`, `phone`
- Header-like fields: `bearer`, `x-api-key`, `x-auth-token`

**Manual filtering** (if needed):
```c
char filtered_message[512];
filter_pii(message, filtered_message, sizeof(filtered_message));
```

## Migration Guide

### Upgrading from Older Versions

**From pre-CP1 versions**:

1. **Log Format Changes**:
   - Old: Plain text or custom format
   - New: Structured JSON with CP1 fields at top level
   - Action: Update log parsing scripts

2. **Timestamp Format**:
   - Old: Various formats
   - New: ISO 8601 with microsecond precision (6 digits)
   - Action: Update timestamp parsing

3. **CP1 Fields**:
   - Old: Fields may be in nested objects
   - New: CP1 fields (tenant_id, run_id, trace_id) at top level
   - Action: Update log querying logic

4. **PII Filtering**:
   - Old: Manual filtering or no filtering
   - New: Automatic recursive JSON filtering
   - Action: Verify filtered patterns match requirements

### Breaking Changes

**CP1 Compliance (v1.0.0)**:
- ✅ Log format changed to structured JSON
- ✅ CP1 fields moved to top level
- ✅ Timestamp format standardized to ISO 8601
- ✅ PII filtering enabled by default

**Compatibility Notes**:
- Old log parsers may need updates
- Log aggregation systems should support JSON parsing
- Health endpoint format changed to CP1-compliant JSON

### Compatibility

**Backward Compatibility**:
- Log format: Not backward compatible (structured JSON required)
- Health endpoint: Not backward compatible (CP1 format required)
- API: Fully backward compatible (no API changes)

**Migration Steps**:
1. Update log parsing scripts to handle JSON format
2. Update health check scripts to handle CP1 format
3. Verify PII filtering works correctly
4. Test with existing log aggregation systems

## API Reference

### Logging Functions

#### `log_error()`

Log an error message with context.

**Signature**:
```c
static void log_error(const char *stage,
                      const request_context_t *ctx,
                      const char *code,
                      const char *message);
```

**Parameters**:
- `stage`: Processing stage (e.g., `"http_request"`, `"nats_request"`)
- `ctx`: Request context (contains tenant_id, run_id, trace_id)
- `code`: Error code string (e.g., `"INVALID_REQUEST"`)
- `message`: Error message (automatically filtered for PII)

**Return Value**: None (void)

**Example**:
```c
log_error("http_request", &ctx, "INVALID_REQUEST", "Missing required field: tenant_id");
```

#### `log_warn()`

Log a warning message with context.

**Signature**:
```c
static void log_warn(const char *stage,
                     const request_context_t *ctx,
                     const char *message);
```

**Parameters**:
- `stage`: Processing stage
- `ctx`: Request context
- `message`: Warning message (automatically filtered for PII)

**Return Value**: None (void)

**Example**:
```c
log_warn("rate_limit_check", &ctx, "Rate limit approaching: 90%");
```

#### `log_info()`

Log an informational message with context.

**Signature**:
```c
static void log_info(const char *stage,
                     const request_context_t *ctx,
                     const char *method,
                     const char *path,
                     int status_code,
                     int latency_ms);
```

**Parameters**:
- `stage`: Processing stage (typically `"http_request"`)
- `ctx`: Request context
- `method`: HTTP method (e.g., `"GET"`, `"POST"`)
- `path`: HTTP path (e.g., `"/api/v1/messages"`)
- `status_code`: HTTP status code (e.g., `200`, `404`)
- `latency_ms`: Request latency in milliseconds

**Return Value**: None (void)

**Example**:
```c
log_info("http_request", &ctx, "POST", "/api/v1/messages", 200, 45);
```

#### `log_debug()`

Log a debug message with context.

**Signature**:
```c
static void log_debug(const char *stage,
                      const request_context_t *ctx,
                      const char *message);
```

**Parameters**:
- `stage`: Processing stage
- `ctx`: Request context
- `message`: Debug message (automatically filtered for PII)

**Return Value**: None (void)

**Example**:
```c
log_debug("http_request", &ctx, "Request parsing details: content_length=1024");
```

### Health Endpoint Functions

#### `handle_health()`

Handle HTTP health endpoint request.

**Signature**:
```c
void handle_health(int client_fd);
```

**Parameters**:
- `client_fd`: Client file descriptor (socket)

**Return Value**: None (void)

**Response Format**:
```json
{
  "status": "healthy",
  "timestamp": "2025-01-27T12:00:00.123456Z"
}
```

**HTTP Status**: `200 OK`

### Utility Functions

#### `get_iso8601_timestamp()`

Generate ISO 8601 timestamp with microsecond precision.

**Signature**:
```c
void get_iso8601_timestamp(char *buffer, size_t buffer_size);
```

**Parameters**:
- `buffer`: Output buffer for timestamp string
- `buffer_size`: Size of buffer (must be at least 32 bytes)

**Return Value**: None (void)

**Output Format**: `YYYY-MM-DDTHH:MM:SS.ssssssZ` (6 digits for microseconds)

#### `filter_pii()`

Filter PII from a string message.

**Signature**:
```c
void filter_pii(const char *input, char *output, size_t output_size);
```

**Parameters**:
- `input`: Input string (may contain PII)
- `output`: Output buffer for filtered string
- `output_size`: Size of output buffer

**Return Value**: None (void)

**Filtered Patterns**: Case-insensitive matching for `password`, `api_key`, `secret`, `token`, etc.

#### `filter_pii_json()`

Filter PII from a JSON object recursively.

**Signature**:
```c
json_t *filter_pii_json(json_t *input);
```

**Parameters**:
- `input`: Input JSON object (may contain PII)

**Return Value**: New JSON object with PII filtered (caller must free with `json_decref()`)

**Filtering**: Recursive filtering of all nested JSON structures

## Production Logging

For production deployments, see [Production Logging Guide](./PRODUCTION_LOGGING.md) for:
- Log rotation strategies (systemd, logrotate, Docker, Kubernetes)
- Log aggregation setup (Loki, ELK Stack)
- Best practices for production logging
- Troubleshooting guide

## References

- [Production Logging Guide](./PRODUCTION_LOGGING.md) - Production log rotation and management
- [Gateway Observability Test Documentation](../../../docs/dev/GATEWAY_OBSERVABILITY_TEST.md)
- [Gateway Observability Tasks](../../../docs/dev/GATEWAY_OBSERVABILITY_TASKS.md)
- [Gateway Observability Additional Tasks](../../../docs/dev/GATEWAY_OBSERVABILITY_ADDITIONAL_TASKS.md)
- [CP1 Observability Invariants](../../../docs/OBSERVABILITY_CP1_INVARIANTS.md)
- [Router Observability Documentation](../../otp/router/docs/OBSERVABILITY.md)
- [Worker Observability Documentation](../../caf/processor/docs/OBSERVABILITY.md)
