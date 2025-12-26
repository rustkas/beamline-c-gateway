/**
 * trace_context.h - Simple trace context propagation
 * 
 * Task 17: Distributed tracing without OpenTelemetry SDK
 * Simple trace ID propagation through IPC and NATS
 */

#ifndef TRACE_CONTEXT_H
#define TRACE_CONTEXT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Trace context structure
 */
typedef struct {
    uint64_t trace_id_high;  /* High 64 bits of trace ID */
    uint64_t trace_id_low;   /* Low 64 bits of trace ID */
    uint64_t span_id;        /* Span ID */
    uint8_t sampled;         /* 1 if sampled, 0 if not */
} trace_context_t;

/**
 * Generate new trace context
 * 
 * Creates a new root trace with random trace ID.
 * 
 * @param ctx     Output trace context
 * @param sampled 1 to sample this trace, 0 otherwise
 */
void trace_context_generate(trace_context_t *ctx, int sampled);

/**
 * Create child span from parent context
 * 
 * @param parent  Parent trace context
 * @param child   Output child context
 */
void trace_context_create_span(const trace_context_t *parent, trace_context_t *child);

/**
 * Format trace context to string (for headers)
 * 
 * Format: "{trace_id_high:016x}-{trace_id_low:016x}-{span_id:016x}-{sampled}"
 * Example: "0123456789abcdef-fedcba9876543210-0000000000000001-1"
 * 
 * @param ctx     Trace context
 * @param buffer  Output buffer
 * @param size    Buffer size (should be at least 72 bytes)
 * @return 0 on success, -1 on error
 */
int trace_context_to_string(const trace_context_t *ctx, char *buffer, size_t size);

/**
 * Parse trace context from string
 * 
 * @param str  String to parse
 * @param ctx  Output trace context
 * @return 0 on success, -1 on error
 */
int trace_context_from_string(const char *str, trace_context_t *ctx);

/**
 * Check if trace context is valid
 * 
 * @param ctx  Trace context
 * @return 1 if valid, 0 if invalid (all zeros)
 */
int trace_context_is_valid(const trace_context_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* TRACE_CONTEXT_H */
