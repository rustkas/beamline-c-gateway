#ifndef METRICS_REGISTRY_H
#define METRICS_REGISTRY_H

#include "prometheus.h"

/**
 * Global metrics registry for C-Gateway
 * All metrics are registered here for export via /metrics endpoint
 */

// === HTTP Metrics ===

// Counter: Total HTTP requests by method, path, status
extern prometheus_counter_t *metric_http_requests_total;

// Histogram: HTTP request duration by method, path
extern prometheus_histogram_t *metric_http_request_duration_seconds;

// Counter: HTTP requests by status code
extern prometheus_counter_t *metric_http_requests_by_status;

// === Rate Limiting Metrics ===

// Counter: Rate limit hits by tenant_id
extern prometheus_counter_t *metric_rate_limit_hits_total;

// Counter: Rate limit allowed requests
extern prometheus_counter_t *metric_rate_limit_allowed_total;

// === Idempotency Metrics ===

// Counter: Idempotency cache hits
extern prometheus_counter_t *metric_idempotency_hits_total;

// Counter: Idempotency cache misses
extern prometheus_counter_t *metric_idempotency_misses_total;

// === NATS Metrics ===

// Counter: NATS messages sent by subject
extern prometheus_counter_t *metric_nats_messages_sent_total;

// Counter: NATS messages received by subject
extern prometheus_counter_t *metric_nats_messages_received_total;

// Counter: NATS publish failures
extern prometheus_counter_t *metric_nats_publish_failures_total;

// Gauge: NATS connection status (1=connected, 0=disconnected)
extern prometheus_gauge_t *metric_nats_connection_status;

// === JSON Parsing Metrics ===

// Counter: JSON parse successes
extern prometheus_counter_t *metric_json_parse_success_total;

// Counter: JSON parse failures
extern prometheus_counter_t *metric_json_parse_failure_total;

// Histogram: JSON parse duration
extern prometheus_histogram_t *metric_json_parse_duration_seconds;

/**
 * Initialize all metrics in the registry
 * Call once at gateway startup after prometheus_init()
 */
int metrics_registry_init(void);

/**
 * Cleanup metrics registry
 * Call at gateway shutdown
 */
void metrics_registry_cleanup(void);

/**
 * Helper: Record HTTP request metrics
 * @param method HTTP method (GET, POST, etc.)
 * @param path Request path (/api/v1/messages, etc.)
 * @param status HTTP status code (200, 400, 500, etc.)
 * @param duration_us Request duration in microseconds
 */
void metrics_record_http_request(const char *method, const char *path, 
                                  int status, uint64_t duration_us);

/**
 * Helper: Record rate limit hit
 * @param tenant_id Tenant identifier
 */
void metrics_record_rate_limit_hit(const char *tenant_id);

/**
 * Helper: Record rate limit allowed
 */
void metrics_record_rate_limit_allowed(void);

/**
 * Helper: Record idempotency cache hit
 */
void metrics_record_idempotency_hit(void);

/**
 * Helper: Record idempotency cache miss
 */
void metrics_record_idempotency_miss(void);

/**
 * Helper: Record NATS message sent
 * @param subject NATS subject
 */
void metrics_record_nats_sent(const char *subject);

/**
 * Helper: Record NATS message received
 * @param subject NATS subject
 */
void metrics_record_nats_received(const char *subject);

/**
 * Helper: Record NATS publish failure
 */
void metrics_record_nats_publish_failure(void);

/**
 * Helper: Update NATS connection status
 * @param connected 1 if connected, 0 if disconnected
 */
void metrics_update_nats_connection_status(int connected);

/**
 * Helper: Record JSON parse success
 * @param duration_us Parse duration in microseconds
 */
void metrics_record_json_parse_success(uint64_t duration_us);

/**
 * Helper: Record JSON parse failure
 */
void metrics_record_json_parse_failure(void);

// === Abuse Detection Metrics ===

// Counter: Total abuse events (legacy, use specific metrics below)
extern prometheus_counter_t *metric_abuse_events_total;

// Counter: Empty payload abuse events
extern prometheus_counter_t *metric_abuse_empty_payload_total;

// Counter: Targeted tenant abuse events
extern prometheus_counter_t *metric_abuse_targeted_tenant_total;

// Counter: Rate limit evasion abuse events
extern prometheus_counter_t *metric_abuse_rate_limit_evasion_total;

// Counter: Heavy payload abuse events
extern prometheus_counter_t *metric_abuse_heavy_payload_total;

// Counter: Multi-tenant flood abuse events
extern prometheus_counter_t *metric_abuse_multi_tenant_flood_total;

// Gauge: Number of currently blocked tenants
extern prometheus_gauge_t *metric_abuse_blocked_tenants;

/**
 * Helper: Record abuse event (legacy, use specific functions below)
 * @param abuse_type Abuse event type (0=empty payload, 1=targeted tenant, etc.)
 * @param tenant_id Tenant identifier (optional, can be NULL)
 */
void metrics_record_abuse_event(int abuse_type, const char *tenant_id);

/**
 * Helper: Record empty payload abuse event
 */
void metrics_record_abuse_empty_payload(void);

/**
 * Helper: Record targeted tenant abuse event
 */
void metrics_record_abuse_targeted_tenant(void);

/**
 * Helper: Record rate limit evasion abuse event
 */
void metrics_record_abuse_rate_limit_evasion(void);

/**
 * Helper: Record heavy payload abuse event
 */
void metrics_record_abuse_heavy_payload(void);

/**
 * Helper: Record multi-tenant flood abuse event
 */
void metrics_record_abuse_multi_tenant_flood(void);

/**
 * Helper: Update blocked tenants count
 * @param count Number of currently blocked tenants
 */
void metrics_update_abuse_blocked_tenants(int count);

#endif // METRICS_REGISTRY_H

