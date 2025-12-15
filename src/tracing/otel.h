#ifndef OTEL_H
#define OTEL_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

// Trace ID (128-bit)
typedef struct {
    uint8_t bytes[16];
} trace_id_t;

// Span ID (64-bit)
typedef struct {
    uint8_t bytes[8];
} span_id_t;

// Span kind
typedef enum {
    SPAN_KIND_INTERNAL = 0,
    SPAN_KIND_SERVER = 1,    // For HTTP server spans
    SPAN_KIND_CLIENT = 2,    // For NATS client spans
    SPAN_KIND_PRODUCER = 3,
    SPAN_KIND_CONSUMER = 4
} span_kind_t;

// Span status
typedef enum {
    SPAN_STATUS_UNSET = 0,
    SPAN_STATUS_OK = 1,
    SPAN_STATUS_ERROR = 2
} span_status_t;

// Span attribute (key-value pair)
typedef struct {
    char key[64];
    char value[256];
} span_attribute_t;

// Span representation
typedef struct otel_span {
    char name[128];
    trace_id_t trace_id;
    span_id_t span_id;
    span_id_t parent_span_id;  // 0 if root span
    span_kind_t kind;
    span_status_t status;
    
    struct timespec start_time;
    struct timespec end_time;
    
    span_attribute_t attributes[32];
    size_t num_attributes;
    
    struct otel_span *parent;
} otel_span_t;

/**
 * Initialize OpenTelemetry tracing subsystem
 * @param service_name Service name (e.g., "c-gateway")
 * @param otlp_endpoint OTLP collector endpoint (e.g., "http://localhost:4317")
 * @return 0 on success, -1 on error
 */
int otel_init(const char *service_name, const char *otlp_endpoint);

/**
 * Cleanup OpenTelemetry subsystem
 */
void otel_cleanup(void);

/**
 * Create a new span
 * @param name Span name (e.g., "gateway.http.request")
 * @param kind Span kind
 * @param parent Parent span (NULL for root span)
 * @return Pointer to created span or NULL on error
 */
otel_span_t* otel_span_start(const char *name, span_kind_t kind, otel_span_t *parent);

/**
 * End a span (marks end time and exports)
 * @param span Span to end
 */
void otel_span_end(otel_span_t *span);

/**
 * Add string attribute to span
 * @param span Target span
 * @param key Attribute key (e.g., "http.method")
 * @param value Attribute value (e.g., "POST")
 */
void otel_span_set_attribute(otel_span_t *span, const char *key, const char *value);

/**
 * Add integer attribute to span
 */
void otel_span_set_attribute_int(otel_span_t *span, const char *key, int64_t value);

/**
 * Set span status
 * @param span Target span
 * @param status Status (OK, ERROR, UNSET)
 */
void otel_span_set_status(otel_span_t *span, span_status_t status);

/**
 * Extract trace context from W3C traceparent header
 * @param traceparent Header value (e.g., "00-4bf92f...736-00f067...b7-01")
 * @param trace_id Output trace ID
 * @param span_id Output span ID
 * @return 0 on success, -1 on parse error
 */
int otel_extract_trace_context(const char *traceparent, trace_id_t *trace_id, span_id_t *span_id);

/**
 * Inject trace context into W3C traceparent format
 * @param trace_id Trace ID
 * @param span_id Span ID
 * @param buffer Output buffer (min 55 bytes)
 * @return Number of bytes written
 */
int otel_inject_trace_context(const trace_id_t *trace_id, const span_id_t *span_id, char *buffer);

#endif // OTEL_H
