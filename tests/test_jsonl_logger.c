/**
 * test_jsonl_logger.c - Test JSONL logger
 */

#include "jsonl_logger.h"
#include <stdio.h>

int main(void) {
    printf("=== JSONL Logger Test ===\n");
    printf("Output should be valid JSON Lines:\n\n");
    
    /* Test 1: Simple log */
    log_context_t ctx = {
        .component = "test",
        .request_id = "req_123",
        .trace_id = "trace_456",
        .tenant_id = "tenant1"
    };
    
    jsonl_log(LOG_LEVEL_INFO, &ctx, "Test message");
    
    /* Test 2: Formatted log */
    jsonl_logf(LOG_LEVEL_WARN, &ctx, "Warning: value=%d", 42);
    
    /* Test 3: Request log */
    jsonl_log_request("ipc_bridge", "req_789", "task_submit", 1024);
    
    /* Test 4: Response log */
    jsonl_log_response("ipc_bridge", "req_789", 200, 512, 150);
    
    /* Test 5: Error log */
    jsonl_log_error("ipc_bridge", "req_789", "timeout", "NATS request timed out");
    
    /* Test 6: No context */
    jsonl_log(LOG_LEVEL_DEBUG, NULL, "Debug message without context");
    
    printf("\n=== All tests complete ===\n");
    return 0;
}
