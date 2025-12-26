/**
 * trace_context.c - Trace context implementation
 * 
 * Task 17: Simple distributed tracing
 */

#include "trace_context.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/**
 * Generate random 64-bit value
 */
static uint64_t random_uint64(void) {
    static int seeded = 0;
    if (!seeded) {
        srand((unsigned int)time(NULL));
        seeded = 1;
    }
    
    uint64_t high = (uint64_t)rand();
    uint64_t low = (uint64_t)rand();
    return (high << 32) | low;
}

void trace_context_generate(trace_context_t *ctx, int sampled) {
    if (!ctx) return;
    
    ctx->trace_id_high = random_uint64();
    ctx->trace_id_low = random_uint64();
    ctx->span_id = random_uint64();
    ctx->sampled = sampled ? 1 : 0;
}

void trace_context_create_span(const trace_context_t *parent, trace_context_t *child) {
    if (!parent || !child) return;
    
    /* Inherit trace ID from parent */
    child->trace_id_high = parent->trace_id_high;
    child->trace_id_low = parent->trace_id_low;
    
    /* Generate new span ID */
    child->span_id = random_uint64();
    
    /* Inherit sampling decision */
    child->sampled = parent->sampled;
}

int trace_context_to_string(const trace_context_t *ctx, char *buffer, size_t size) {
    if (!ctx || !buffer || size < 72) {
        return -1;
    }
    
    int written = snprintf(buffer, size,
                          "%016lx-%016lx-%016lx-%d",
                          (unsigned long)ctx->trace_id_high,
                          (unsigned long)ctx->trace_id_low,
                          (unsigned long)ctx->span_id,
                          ctx->sampled);
    
    return (written > 0 && (size_t)written < size) ? 0 : -1;
}

int trace_context_from_string(const char *str, trace_context_t *ctx) {
    if (!str || !ctx) {
        return -1;
    }
    
    unsigned long trace_high, trace_low, span;
    int sampled;
    
    int parsed = sscanf(str, "%lx-%lx-%lx-%d",
                       &trace_high, &trace_low, &span, &sampled);
    
    if (parsed != 4) {
        return -1;
    }
    
    ctx->trace_id_high = (uint64_t)trace_high;
    ctx->trace_id_low = (uint64_t)trace_low;
    ctx->span_id = (uint64_t)span;
    ctx->sampled = sampled ? 1 : 0;
    
    return 0;
}

int trace_context_is_valid(const trace_context_t *ctx) {
    if (!ctx) {
        return 0;
    }
    
    /* Valid if trace ID is non-zero */
    return (ctx->trace_id_high != 0 || ctx->trace_id_low != 0);
}
