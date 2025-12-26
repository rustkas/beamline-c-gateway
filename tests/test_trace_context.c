/**
 * test_trace_context.c - Trace context tests
 */

#include "trace_context.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

static void test_generate(void) {
    printf("Test: generate trace context... ");
    
    trace_context_t ctx;
    trace_context_generate(&ctx, 1);
    
    assert(trace_context_is_valid(&ctx));
    assert(ctx.sampled == 1);
    
    printf("OK\n");
}

static void test_to_string(void) {
    printf("Test: trace context to string... ");
    
    trace_context_t ctx;
    ctx.trace_id_high = 0x0123456789abcdefUL;
    ctx.trace_id_low = 0xfedcba9876543210UL;
    ctx.span_id = 0x0000000000000001UL;
    ctx.sampled = 1;
    
    char buffer[128];
    int rc = trace_context_to_string(&ctx, buffer, sizeof(buffer));
    
    assert(rc == 0);
    assert(strstr(buffer, "0123456789abcdef") != NULL);
    assert(strstr(buffer, "fedcba9876543210") != NULL);
    
    printf("OK (str=%s)\n", buffer);
}

static void test_from_string(void) {
    printf("Test: trace context from string... ");
    
    const char *trace_str = "0123456789abcdef-fedcba9876543210-0000000000000042-1";
    
    trace_context_t ctx;
    int rc = trace_context_from_string(trace_str, &ctx);
    
    assert(rc == 0);
    assert(ctx.trace_id_high == 0x0123456789abcdefUL);
    assert(ctx.trace_id_low == 0xfedcba9876543210UL);
    assert(ctx.span_id == 0x42UL);
    assert(ctx.sampled == 1);
    
    printf("OK\n");
}

static void test_child_span(void) {
    printf("Test: create child span... ");
    
    trace_context_t parent;
    trace_context_generate(&parent, 1);
    
    trace_context_t child;
    trace_context_create_span(&parent, &child);
    
    /* Child inherits trace ID */
    assert(child.trace_id_high == parent.trace_id_high);
    assert(child.trace_id_low == parent.trace_id_low);
    
    /* But has different span ID */
    assert(child.span_id != parent.span_id);
    
    /* Inherits sampling decision */
    assert(child.sampled == parent.sampled);
    
    printf("OK\n");
}

static void test_roundtrip(void) {
    printf("Test: roundtrip to/from string... ");
    
    trace_context_t original;
    trace_context_generate(&original, 1);
    
    char buffer[128];
    trace_context_to_string(&original, buffer, sizeof(buffer));
    
    trace_context_t parsed;
    trace_context_from_string(buffer, &parsed);
    
    assert(parsed.trace_id_high == original.trace_id_high);
    assert(parsed.trace_id_low == original.trace_id_low);
    assert(parsed.span_id == original.span_id);
    assert(parsed.sampled == original.sampled);
    
    printf("OK\n");
}

int main(void) {
    printf("=== Trace Context Tests ===\n");
    
    test_generate();
    test_to_string();
    test_from_string();
    test_child_span();
    test_roundtrip();
    
    printf("\nAll tests passed!\n");
    return 0;
}
