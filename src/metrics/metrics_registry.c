#include "metrics_registry.h"

#include <string.h>
#include <stdio.h>

// Global metric pointers
prometheus_counter_t *metric_http_requests_total = NULL;
prometheus_histogram_t *metric_http_request_duration_seconds = NULL;
prometheus_counter_t *metric_http_requests_by_status = NULL;

prometheus_counter_t *metric_rate_limit_hits_total = NULL;
prometheus_counter_t *metric_rate_limit_allowed_total = NULL;

prometheus_counter_t *metric_idempotency_hits_total = NULL;
prometheus_counter_t *metric_idempotency_misses_total = NULL;

prometheus_counter_t *metric_nats_messages_sent_total = NULL;
prometheus_counter_t *metric_nats_messages_received_total = NULL;
prometheus_counter_t *metric_nats_publish_failures_total = NULL;
prometheus_gauge_t *metric_nats_connection_status = NULL;

prometheus_counter_t *metric_json_parse_success_total = NULL;
prometheus_counter_t *metric_json_parse_failure_total = NULL;
prometheus_histogram_t *metric_json_parse_duration_seconds = NULL;

prometheus_counter_t *metric_abuse_events_total = NULL;
prometheus_counter_t *metric_abuse_empty_payload_total = NULL;
prometheus_counter_t *metric_abuse_targeted_tenant_total = NULL;
prometheus_counter_t *metric_abuse_rate_limit_evasion_total = NULL;
prometheus_counter_t *metric_abuse_heavy_payload_total = NULL;
prometheus_counter_t *metric_abuse_multi_tenant_flood_total = NULL;
prometheus_gauge_t *metric_abuse_blocked_tenants = NULL;

int metrics_registry_init(void) {
    // Initialize Prometheus subsystem
    if (prometheus_init() != 0) {
        return -1;
    }
    
    // HTTP metrics
    metric_http_requests_total = prometheus_counter_create(
        "gateway_http_requests_total",
        "Total number of HTTP requests processed"
    );
    if (!metric_http_requests_total) return -1;
    
    metric_http_request_duration_seconds = prometheus_histogram_create(
        "gateway_http_request_duration_seconds",
        "HTTP request duration in seconds"
    );
    if (!metric_http_request_duration_seconds) return -1;
    
    metric_http_requests_by_status = prometheus_counter_create(
        "gateway_http_requests_by_status",
        "Total number of HTTP requests by status code"
    );
    if (!metric_http_requests_by_status) return -1;
    
    // Rate limiting metrics
    metric_rate_limit_hits_total = prometheus_counter_create(
        "gateway_rate_limit_hits_total",
        "Total number of rate limit hits"
    );
    if (!metric_rate_limit_hits_total) return -1;
    
    metric_rate_limit_allowed_total = prometheus_counter_create(
        "gateway_rate_limit_allowed_total",
        "Total number of requests allowed by rate limiter"
    );
    if (!metric_rate_limit_allowed_total) return -1;
    
    // Idempotency metrics
    metric_idempotency_hits_total = prometheus_counter_create(
        "gateway_idempotency_hits_total",
        "Total number of idempotency cache hits"
    );
    if (!metric_idempotency_hits_total) return -1;
    
    metric_idempotency_misses_total = prometheus_counter_create(
        "gateway_idempotency_misses_total",
        "Total number of idempotency cache misses"
    );
    if (!metric_idempotency_misses_total) return -1;
    
    // NATS metrics
    metric_nats_messages_sent_total = prometheus_counter_create(
        "gateway_nats_messages_sent_total",
        "Total number of NATS messages sent"
    );
    if (!metric_nats_messages_sent_total) return -1;
    
    metric_nats_messages_received_total = prometheus_counter_create(
        "gateway_nats_messages_received_total",
        "Total number of NATS messages received"
    );
    if (!metric_nats_messages_received_total) return -1;
    
    metric_nats_publish_failures_total = prometheus_counter_create(
        "gateway_nats_publish_failures_total",
        "Total number of NATS publish failures"
    );
    if (!metric_nats_publish_failures_total) return -1;
    
    metric_nats_connection_status = prometheus_gauge_create(
        "gateway_nats_connection_status",
        "NATS connection status (1=connected, 0=disconnected)"
    );
    if (!metric_nats_connection_status) return -1;
    prometheus_gauge_set(metric_nats_connection_status, 0);
    
    // JSON parsing metrics
    metric_json_parse_success_total = prometheus_counter_create(
        "gateway_json_parse_success_total",
        "Total number of successful JSON parse operations"
    );
    if (!metric_json_parse_success_total) return -1;
    
    metric_json_parse_failure_total = prometheus_counter_create(
        "gateway_json_parse_failure_total",
        "Total number of failed JSON parse operations"
    );
    if (!metric_json_parse_failure_total) return -1;
    
    metric_json_parse_duration_seconds = prometheus_histogram_create(
        "gateway_json_parse_duration_seconds",
        "JSON parse operation duration in seconds"
    );
    if (!metric_json_parse_duration_seconds) return -1;
    
    // Abuse detection metrics
    metric_abuse_events_total = prometheus_counter_create(
        "gateway_abuse_events_total",
        "Total number of abuse events detected (legacy)"
    );
    if (!metric_abuse_events_total) return -1;
    
    metric_abuse_empty_payload_total = prometheus_counter_create(
        "gateway_abuse_empty_payload_total",
        "Total number of empty payload abuse events detected"
    );
    if (!metric_abuse_empty_payload_total) return -1;
    
    metric_abuse_targeted_tenant_total = prometheus_counter_create(
        "gateway_abuse_targeted_tenant_total",
        "Total number of targeted tenant abuse events detected"
    );
    if (!metric_abuse_targeted_tenant_total) return -1;
    
    metric_abuse_rate_limit_evasion_total = prometheus_counter_create(
        "gateway_abuse_rate_limit_evasion_total",
        "Total number of rate limit evasion abuse events detected"
    );
    if (!metric_abuse_rate_limit_evasion_total) return -1;
    
    metric_abuse_heavy_payload_total = prometheus_counter_create(
        "gateway_abuse_heavy_payload_total",
        "Total number of heavy payload abuse events detected"
    );
    if (!metric_abuse_heavy_payload_total) return -1;
    
    metric_abuse_multi_tenant_flood_total = prometheus_counter_create(
        "gateway_abuse_multi_tenant_flood_total",
        "Total number of multi-tenant flood abuse events detected"
    );
    if (!metric_abuse_multi_tenant_flood_total) return -1;
    
    metric_abuse_blocked_tenants = prometheus_gauge_create(
        "gateway_abuse_blocked_tenants",
        "Number of currently blocked tenants"
    );
    if (!metric_abuse_blocked_tenants) return -1;
    
    return 0;
}

void metrics_registry_cleanup(void) {
    prometheus_cleanup();
}

void metrics_record_http_request(const char *method, const char *path, 
                                  int status, uint64_t duration_us) {
    // Note: status parameter reserved for CP2 label support enhancement
    // See apps/c-gateway/TODO.md GATEWAY-CP2-3 for implementation plan
    (void)status;
    if (!method || !path) return;
    
    // Increment total requests counter
    prometheus_counter_inc(metric_http_requests_total);
    
    // Record duration in histogram
    prometheus_histogram_observe(metric_http_request_duration_seconds, duration_us);
    
    // Record status code counter
    prometheus_counter_inc(metric_http_requests_by_status);
}

void metrics_record_rate_limit_hit(const char *tenant_id) {
    // Note: tenant_id parameter reserved for CP2 label support enhancement
    // See apps/c-gateway/TODO.md GATEWAY-CP2-3 for implementation plan
    (void)tenant_id;
    prometheus_counter_inc(metric_rate_limit_hits_total);
}

void metrics_record_rate_limit_allowed(void) {
    prometheus_counter_inc(metric_rate_limit_allowed_total);
}

void metrics_record_idempotency_hit(void) {
    prometheus_counter_inc(metric_idempotency_hits_total);
}

void metrics_record_idempotency_miss(void) {
    prometheus_counter_inc(metric_idempotency_misses_total);
}

void metrics_record_nats_sent(const char *subject) {
    // Note: subject parameter reserved for CP2 label support enhancement
    // See apps/c-gateway/TODO.md GATEWAY-CP2-3 for implementation plan
    (void)subject;
    prometheus_counter_inc(metric_nats_messages_sent_total);
}

void metrics_record_nats_received(const char *subject) {
    // Note: subject parameter reserved for CP2 label support enhancement
    // See apps/c-gateway/TODO.md GATEWAY-CP2-3 for implementation plan
    (void)subject;
    prometheus_counter_inc(metric_nats_messages_received_total);
}

void metrics_record_nats_publish_failure(void) {
    prometheus_counter_inc(metric_nats_publish_failures_total);
}

void metrics_update_nats_connection_status(int connected) {
    prometheus_gauge_set(metric_nats_connection_status, connected ? 1 : 0);
}

void metrics_record_json_parse_success(uint64_t duration_us) {
    prometheus_counter_inc(metric_json_parse_success_total);
    prometheus_histogram_observe(metric_json_parse_duration_seconds, duration_us);
}

void metrics_record_json_parse_failure(void) {
    prometheus_counter_inc(metric_json_parse_failure_total);
}

void metrics_record_abuse_event(int abuse_type, const char *tenant_id) {
    // Note: abuse_type and tenant_id parameters reserved for CP2 label support enhancement
    // See apps/c-gateway/TODO.md GATEWAY-CP2-3 for implementation plan
    (void)abuse_type;
    (void)tenant_id;
    if (metric_abuse_events_total) {
        prometheus_counter_inc(metric_abuse_events_total);
    }
}

void metrics_record_abuse_empty_payload(void) {
    if (metric_abuse_empty_payload_total) {
        prometheus_counter_inc(metric_abuse_empty_payload_total);
    }
    if (metric_abuse_events_total) {
        prometheus_counter_inc(metric_abuse_events_total);
    }
}

void metrics_record_abuse_targeted_tenant(void) {
    if (metric_abuse_targeted_tenant_total) {
        prometheus_counter_inc(metric_abuse_targeted_tenant_total);
    }
    if (metric_abuse_events_total) {
        prometheus_counter_inc(metric_abuse_events_total);
    }
}

void metrics_record_abuse_rate_limit_evasion(void) {
    if (metric_abuse_rate_limit_evasion_total) {
        prometheus_counter_inc(metric_abuse_rate_limit_evasion_total);
    }
    if (metric_abuse_events_total) {
        prometheus_counter_inc(metric_abuse_events_total);
    }
}

void metrics_record_abuse_heavy_payload(void) {
    if (metric_abuse_heavy_payload_total) {
        prometheus_counter_inc(metric_abuse_heavy_payload_total);
    }
    if (metric_abuse_events_total) {
        prometheus_counter_inc(metric_abuse_events_total);
    }
}

void metrics_record_abuse_multi_tenant_flood(void) {
    if (metric_abuse_multi_tenant_flood_total) {
        prometheus_counter_inc(metric_abuse_multi_tenant_flood_total);
    }
    if (metric_abuse_events_total) {
        prometheus_counter_inc(metric_abuse_events_total);
    }
}

void metrics_update_abuse_blocked_tenants(int count) {
    if (metric_abuse_blocked_tenants) {
        prometheus_gauge_set(metric_abuse_blocked_tenants, count);
    }
}

