/**
 * test_ipc_backpressure.c - Backpressure tests
 */

#include "ipc_backpressure.h"
#include <stdio.h>
#include <assert.h>

static void test_init_defaults(void) {
    printf("Test: init with defaults... ");
    
    ipc_backpressure_t *bp = ipc_backpressure_init(NULL);
    assert(bp != NULL);
    
    int inflight = 0;
    size_t rejections = 0;
    ipc_backpressure_get_stats(bp, &inflight, &rejections);
    
    assert(inflight == 0);
    assert(rejections == 0);
    
    ipc_backpressure_destroy(bp);
    printf("OK\n");
}

static void test_per_conn_limit(void) {
    printf("Test: per-connection limit... ");
    
    ipc_backpressure_config_t config = {
        .global_max_inflight = 1000,
        .per_conn_max_inflight = 5
    };
    
    ipc_backpressure_t *bp = ipc_backpressure_init(&config);
    assert(bp != NULL);
    
    int conn_id = 0;
    
    /* Accept first 5 */
    for (int i = 0; i < 5; i++) {
        assert(ipc_backpressure_can_accept(bp, conn_id) == 1);
        ipc_backpressure_request_start(bp, conn_id);
    }
    
    /* 6th should be rejected */
    assert(ipc_backpressure_can_accept(bp, conn_id) == 0);
    
    /* Complete one */
    ipc_backpressure_request_complete(bp, conn_id);
    
    /* Now can accept again */
    assert(ipc_backpressure_can_accept(bp, conn_id) == 1);
    
    ipc_backpressure_destroy(bp);
    printf("OK\n");
}

static void test_global_limit(void) {
    printf("Test: global limit... ");
    
    ipc_backpressure_config_t config = {
        .global_max_inflight = 10,
        .per_conn_max_inflight = 5
    };
    
    ipc_backpressure_t *bp = ipc_backpressure_init(&config);
    
    /* Fill from two connections */
    for (int i = 0; i < 5; i++) {
        ipc_backpressure_request_start(bp, 0);
        ipc_backpressure_request_start(bp, 1);
    }
    
    /* Global limit reached (10 total) */
    assert(ipc_backpressure_can_accept(bp, 2) == 0);
    
    int inflight = 0;
    ipc_backpressure_get_stats(bp, &inflight, NULL);
    assert(inflight == 10);
    
    ipc_backpressure_destroy(bp);
    printf("OK\n");
}

static void test_burst_scenario(void) {
    printf("Test: burst scenario... ");
    
    ipc_backpressure_config_t config = {
        .global_max_inflight = 100,
        .per_conn_max_inflight = 10
    };
    
    ipc_backpressure_t *bp = ipc_backpressure_init(&config);
    
    /* Simulate burst: try to add 20 requests on conn 0 */
    int accepted = 0;
    for (int i = 0; i < 20; i++) {
        if (ipc_backpressure_can_accept(bp, 0)) {
            ipc_backpressure_request_start(bp, 0);
            accepted++;
        }
    }
    
    /* Should only accept up to per-conn limit */
    assert(accepted == 10);
    
    size_t rejections = 0;
    ipc_backpressure_get_stats(bp, NULL, &rejections);
    printf("(accepted=%d, rejections=%zu) ", accepted, rejections);
    
    ipc_backpressure_destroy(bp);
    printf("OK\n");
}

int main(void) {
    printf("=== IPC Backpressure Tests ===\n");
    
    test_init_defaults();
    test_per_conn_limit();
    test_global_limit();
    test_burst_scenario();
    
    printf("\nAll tests passed!\n");
    return 0;
}
