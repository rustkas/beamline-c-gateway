# Observability Metrics Dashboard (CP2 Planning)

**Component**: Gateway (`apps/c-gateway/`)  
**Purpose**: Planning document for CP2 Prometheus metrics dashboard  
**Status**: 📋 **PLANNING** (Not Implemented)  
**Target**: CP2 (Prometheus Metrics Implementation)  
**Last Updated**: 2025-01-27

---

## Overview

This document plans the observability metrics dashboard for Gateway when Prometheus metrics are implemented in CP2. This is a **planning document only** - actual implementation will be done in CP2.

**Current Status**: Gateway observability is CP1-compliant with structured JSON logging and health endpoints. Prometheus metrics are planned for CP2.

---

## Planned Metrics

### HTTP Request Metrics

**Request Rate**:
```
gateway_requests_total{method, endpoint, status}
```
- **Description**: Total number of HTTP requests
- **Labels**: `method` (GET, POST, etc.), `endpoint` (path), `status` (HTTP status code)
- **Type**: Counter
- **Panel**: Time series graph showing request rate over time

**Request Duration**:
```
gateway_request_duration_seconds{method, endpoint}
```
- **Description**: HTTP request latency in seconds
- **Labels**: `method`, `endpoint`
- **Type**: Histogram
- **Buckets**: 0.001, 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.5, 5.0
- **Panel**: Histogram showing p50, p95, p99 latencies

**Request Size**:
```
gateway_request_size_bytes{method, endpoint}
```
- **Description**: HTTP request body size in bytes
- **Labels**: `method`, `endpoint`
- **Type**: Histogram
- **Panel**: Request size distribution

### Rate Limiting Metrics

**Rate Limit Hits**:
```
gateway_rate_limit_hits_total{endpoint, tenant}
```
- **Description**: Number of requests that hit rate limits
- **Labels**: `endpoint`, `tenant` (optional, for CP2 per-tenant rate limiting)
- **Type**: Counter
- **Panel**: Rate limit hits over time, grouped by endpoint

**Rate Limit Allowed**:
```
gateway_rate_limit_allowed_total{endpoint, tenant}
```
- **Description**: Number of requests allowed (not rate limited)
- **Labels**: `endpoint`, `tenant`
- **Type**: Counter
- **Panel**: Rate limit allowed vs hits ratio

### NATS Metrics

**NATS Messages Sent**:
```
gateway_nats_messages_sent_total{subject, status}
```
- **Description**: Total NATS messages sent
- **Labels**: `subject` (NATS subject), `status` (success, failure)
- **Type**: Counter
- **Panel**: NATS message send rate

**NATS Messages Received**:
```
gateway_nats_messages_received_total{subject}
```
- **Description**: Total NATS messages received
- **Labels**: `subject`
- **Type**: Counter
- **Panel**: NATS message receive rate

**NATS Request Duration**:
```
gateway_nats_request_duration_seconds{subject}
```
- **Description**: NATS request/response latency
- **Labels**: `subject`
- **Type**: Histogram
- **Panel**: NATS latency distribution

**NATS Publish Failures**:
```
gateway_nats_publish_failures_total{subject, error_type}
```
- **Description**: NATS publish failures
- **Labels**: `subject`, `error_type` (timeout, connection_error, etc.)
- **Type**: Counter
- **Panel**: NATS failure rate

### Idempotency Metrics

**Idempotency Hits**:
```
gateway_idempotency_hits_total{endpoint}
```
- **Description**: Number of idempotent requests (duplicate detected)
- **Labels**: `endpoint`
- **Type**: Counter
- **Panel**: Idempotency hit rate

**Idempotency Misses**:
```
gateway_idempotency_misses_total{endpoint}
```
- **Description**: Number of non-idempotent requests (new requests)
- **Labels**: `endpoint`
- **Type**: Counter
- **Panel**: Idempotency miss rate

### System Metrics

**Active Connections**:
```
gateway_connections_active{type}
```
- **Description**: Number of active connections
- **Labels**: `type` (http, sse)
- **Type**: Gauge
- **Panel**: Active connections over time

**Memory Usage**:
```
gateway_memory_bytes{type}
```
- **Description**: Memory usage in bytes
- **Labels**: `type` (heap, stack, total)
- **Type**: Gauge
- **Panel**: Memory usage graph

---

## Grafana Dashboard Structure

### Dashboard: Gateway Overview

**Panels**:

1. **Request Rate** (Time Series)
   - Query: `rate(gateway_requests_total[5m])`
   - Legend: `{{method}} {{endpoint}}`
   - Y-axis: Requests per second

2. **Request Duration (p50, p95, p99)** (Time Series)
   - Query: `histogram_quantile(0.50, rate(gateway_request_duration_seconds_bucket[5m]))`
   - Query: `histogram_quantile(0.95, rate(gateway_request_duration_seconds_bucket[5m]))`
   - Query: `histogram_quantile(0.99, rate(gateway_request_duration_seconds_bucket[5m]))`
   - Y-axis: Seconds

3. **Error Rate** (Time Series)
   - Query: `rate(gateway_requests_total{status=~"5.."}[5m])`
   - Y-axis: Errors per second

4. **Rate Limit Hits vs Allowed** (Time Series)
   - Query: `rate(gateway_rate_limit_hits_total[5m])` (Total Hits)
   - Query: `rate(gateway_rate_limit_allowed_total[5m])` (Allowed)
   - Query: `rate(gateway_rate_limit_exceeded_total[5m])` (Exceeded)
   - Legend: `Hits / Allowed / Exceeded`
   - Y-axis: Requests per second
   - **What to Look For**:
     - **Under Limit**: `allowed_total` ≈ `hits_total`, `exceeded_total` = 0
     - **At Limit**: `allowed_total` = limit, `exceeded_total` > 0
     - **Ratio**: `exceeded_total / hits_total` should match expected rate limit hit ratio

5. **NATS Message Rate** (Time Series)
   - Query: `rate(gateway_nats_messages_sent_total[5m])`
   - Query: `rate(gateway_nats_messages_received_total[5m])`
   - Legend: Sent / Received
   - Y-axis: Messages per second

6. **Top Endpoints by Request Rate** (Table)
   - Query: `topk(10, rate(gateway_requests_total[5m]))`
   - Columns: Endpoint, Requests/sec

7. **Error Rate by Endpoint** (Table)
   - Query: `topk(10, rate(gateway_requests_total{status=~"5.."}[5m]))`
   - Columns: Endpoint, Errors/sec

8. **Active Connections** (Gauge)
   - Query: `gateway_connections_active`
   - Thresholds: Green (0-100), Yellow (100-500), Red (>500)

9. **HTTP Status Codes (429 vs Others)** (Stacked Area Chart)
   - Query: `rate(gateway_http_requests_total[5m])` by `status`
   - Legend: `{{status}}`
   - **What to Look For**:
     - **Rate Limit Exceeded**: `status="429"` > 0 when limit exceeded
     - **Router Errors**: `status="400"`, `status="401"`, `status="500"` when Router errors occur
     - **Priority**: 429 should appear BEFORE Router errors (rate limit checked first)

10. **Router Intake vs Rate Limiting** (Dual Y-axis Time Series)
    - Query: `rate(router_intake_messages_total[5m])` (Router Intake)
    - Query: `rate(gateway_rate_limit_allowed_total[5m])` (Rate Limit Allowed)
    - **What to Look For**:
      - **Rate Limit Active**: When `exceeded_total` > 0, `router_intake_messages_total` should be lower (Router not called)
      - **No Conflict**: Router intake errors don't occur when rate limit exceeded
      - **Correlation**: `router_intake_messages_total` ≈ `gateway_rate_limit_allowed_total`

### Abuse Detection Metrics (CP2+)

**Abuse Events**:
```
gateway_abuse_events_total{abuse_type, tenant_id}
```
- **Description**: Total number of abuse events detected
- **Labels**: `abuse_type` (empty_payload, targeted_tenant, rate_limit_evasion, heavy_payload, multi_tenant_flood), `tenant_id` (optional)
- **Type**: Counter
- **Panel**: Time series graph showing abuse events over time by type

**Abuse Event Rate**:
```
rate(gateway_abuse_events_total[5m])
```
- **Description**: Rate of abuse events per second
- **Panel**: Stacked area chart showing abuse event rate by type

### Dashboard: Abuse Detection (CP2+)

**Panels**:

1. **Abuse Events by Type** (Stacked Area Chart)
   - Query: `rate(gateway_abuse_events_total[5m])` by `abuse_type`
   - Legend: `{{abuse_type}}`
   - **What to Look For**:
     - **Empty Payload Flood**: High `empty_payload` rate indicates flood attack
     - **Targeted Tenant Attack**: High `targeted_tenant` rate indicates DDoS on specific tenant
     - **Rate Limit Evasion**: `rate_limit_evasion` indicates attempts to bypass limits
     - **Heavy Payload**: `heavy_payload` indicates resource exhaustion attempts
     - **Multi-Tenant Flood**: `multi_tenant_flood` indicates coordinated attack

2. **Abuse Events by Tenant** (Table)
   - Query: `sum by (tenant_id, abuse_type) (rate(gateway_abuse_events_total[5m]))`
   - Columns: Tenant ID, Abuse Type, Event Rate
   - **What to Look For**:
     - **Top Abusers**: Tenants with highest abuse event rates
     - **Pattern Analysis**: Which tenants trigger which abuse types

3. **Abuse Response Actions** (Time Series)
   - Query: `rate(gateway_abuse_events_total{abuse_type="targeted_tenant"}[5m])` (Temporary Blocks)
   - Query: `rate(gateway_abuse_events_total{abuse_type="rate_limit_evasion"}[5m])` (Stricter Rate Limiting)
   - **What to Look For**:
     - **Block Effectiveness**: Reduction in abuse events after blocking
     - **False Positives**: High block rate with low actual abuse

4. **Abuse Events vs HTTP 429** (Dual Y-axis Time Series)
   - Query: `rate(gateway_abuse_events_total[5m])` (Abuse Events)
   - Query: `rate(gateway_http_requests_total{status="429"}[5m])` (HTTP 429)
   - **What to Look For**:
     - **Correlation**: Abuse events should correlate with 429 responses
     - **Response Effectiveness**: 429 responses should reduce abuse events

5. **Router Abuse Events** (Time Series)
   - Query: `rate(router_abuse_heavy_payload_total[5m])`
   - **What to Look For**:
     - **Heavy Payload Pattern**: Router-side detection of heavy payload abuse
     - **Correlation with Gateway**: Router abuse should correlate with Gateway abuse

### Dashboard: Gateway Performance

**Panels**:

1. **Request Latency Distribution** (Heatmap)
   - Query: `rate(gateway_request_duration_seconds_bucket[5m])`
   - Y-axis: Latency buckets
   - X-axis: Time

2. **Request Size Distribution** (Histogram)
   - Query: `rate(gateway_request_size_bytes_bucket[5m])`
   - Y-axis: Size buckets

3. **NATS Latency** (Time Series)
   - Query: `histogram_quantile(0.95, rate(gateway_nats_request_duration_seconds_bucket[5m]))`
   - Y-axis: Seconds

4. **Memory Usage** (Time Series)
   - Query: `gateway_memory_bytes{type="total"}`
   - Y-axis: Bytes

---

## Grafana Dashboard JSON Template

**Note**: This is a placeholder template. Actual dashboard will be created when CP2 metrics are implemented.

```json
{
  "dashboard": {
    "title": "Gateway Observability",
    "tags": ["gateway", "observability", "cp2"],
    "timezone": "browser",
    "panels": [
      {
        "id": 1,
        "title": "Request Rate",
        "type": "graph",
        "targets": [
          {
            "expr": "rate(gateway_requests_total[5m])",
            "legendFormat": "{{method}} {{endpoint}}"
          }
        ]
      },
      {
        "id": 2,
        "title": "Request Duration (p95)",
        "type": "graph",
        "targets": [
          {
            "expr": "histogram_quantile(0.95, rate(gateway_request_duration_seconds_bucket[5m]))",
            "legendFormat": "p95"
          }
        ]
      }
    ]
  }
}
```

---

## Alerting Rules (Future Alertmanager Integration)

### High Error Rate

```yaml
groups:
  - name: gateway_alerts
    interval: 30s
    rules:
      - alert: GatewayHighErrorRate
        expr: rate(gateway_requests_total{status=~"5.."}[5m]) > 0.1
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "Gateway error rate is high"
          description: "Error rate is {{ $value }} errors/sec"

      - alert: GatewayHighLatency
        expr: histogram_quantile(0.95, rate(gateway_request_duration_seconds_bucket[5m])) > 1.0
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "Gateway latency is high"
          description: "p95 latency is {{ $value }} seconds"

      - alert: GatewayRateLimitExceeded
        expr: rate(gateway_rate_limit_hits_total[5m]) > 10
        for: 5m
        labels:
          severity: info
        annotations:
          summary: "Gateway rate limit frequently hit"
          description: "Rate limit hits: {{ $value }} hits/sec"

      - alert: GatewayNATSFailures
        expr: rate(gateway_nats_publish_failures_total[5m]) > 1
        for: 5m
        labels:
          severity: critical
        annotations:
          summary: "Gateway NATS publish failures"
          description: "NATS failures: {{ $value }} failures/sec"
```

---

## Metric Naming Conventions

### Naming Rules

1. **Prefix**: All metrics start with `gateway_`
2. **Suffix**: 
   - `_total` for counters
   - `_seconds` for duration histograms
   - `_bytes` for size histograms
   - No suffix for gauges
3. **Labels**: Use lowercase with underscores (e.g., `tenant_id`, `endpoint`)
4. **Units**: Always include units in metric name (e.g., `_seconds`, `_bytes`)

### Examples

✅ **Good**:
- `gateway_requests_total`
- `gateway_request_duration_seconds`
- `gateway_memory_bytes`

❌ **Bad**:
- `requests` (missing prefix)
- `gateway_request_duration` (missing unit suffix)
- `gateway_requests` (missing `_total` for counter)

---

## Cardinality Control

### Label Cardinality

**High-cardinality labels** (use with caution):
- `tenant_id`: Can have many unique values
- `trace_id`: Very high cardinality (unique per request)

**Low-cardinality labels** (safe to use):
- `method`: Limited values (GET, POST, PUT, DELETE)
- `endpoint`: Limited number of endpoints
- `status`: Limited HTTP status codes

### Cardinality Control Strategy

1. **Tenant Label Allowlist**:
   ```bash
   METRICS_TENANT_LABEL_ALLOWLIST=premium-1,premium-2,premium-3
   ```
   - Only include tenant label for allowlisted tenants
   - Aggregate other tenants as `tenant="other"`

2. **Trace ID Sampling**:
   - Don't include `trace_id` in metrics (too high cardinality)
   - Use `trace_id` only in logs

3. **Subject Aggregation**:
   - Aggregate NATS subjects by pattern (e.g., `beamline.router.*` → `beamline.router.*`)
   - Limit unique subject labels

---

## Implementation Notes

### CP2 Requirements

1. **Prometheus Metrics Endpoint**: `GET /_metrics`
   - Support Prometheus text format
   - Optional: Support JSON format for backward compatibility

2. **Label Support Enhancement**:
   - Implement label support in `metrics_registry.c`
   - Add labels to all metrics (see `apps/c-gateway/TODO.md` GATEWAY-CP2-3)

3. **Histogram Implementation**:
   - Use Prometheus histogram buckets
   - Track p50, p95, p99 percentiles

4. **Cardinality Control**:
   - Implement tenant allowlist
   - Aggregate high-cardinality labels

### Testing

- Unit tests for metric collection
- Integration tests for Prometheus endpoint
- Load tests to verify metric performance
- Cardinality tests to verify label limits

---

## References

- [Gateway Observability Documentation](./OBSERVABILITY.md) - Current CP1 observability
- [Prometheus Best Practices](https://prometheus.io/docs/practices/naming/) - Metric naming conventions
- [Grafana Dashboard Documentation](https://grafana.com/docs/grafana/latest/dashboards/) - Dashboard creation
- [Alertmanager Configuration](https://prometheus.io/docs/alerting/latest/configuration/) - Alerting rules
- [Gateway TODO](../TODO.md) - CP2 implementation tasks

---

## Status

**Current**: 📋 Planning document (CP2)  
**Implementation**: Planned for CP2  
**Dashboard Creation**: After CP2 metrics implementation

