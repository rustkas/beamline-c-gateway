/**
 * test_ipc_streaming.c - Test streaming
 */

#include "ipc_streaming.h"
#include <stdio.h>
#include <assert.h>

static void test_stream_session(void) {
    printf("Test: stream session lifecycle... ");
    
    ipc_streaming_t *mgr = ipc_streaming_init();
    assert(mgr != NULL);
    
    /* Start session */
    int rc = ipc_streaming_start(mgr, "stream_123");
    assert(rc == 0);
    
    /* Stats */
    int active = 0;
    ipc_streaming_get_stats(mgr, &active, NULL);
    assert(active == 1);
    
    /* Send chunks */
    char data[128];
    rc = ipc_streaming_chunk(mgr, "stream_123", data, sizeof(data));
    assert(rc == 0);
    
    rc = ipc_streaming_chunk(mgr, "stream_123", data, 64);
    assert(rc == 0);
    
    /* Complete */
    rc = ipc_streaming_complete(mgr, "stream_123");
    assert(rc == 0);
    
    ipc_streaming_get_stats(mgr, &active, NULL);
    assert(active == 0);
    
    ipc_streaming_destroy(mgr);
    printf("OK\n");
}

static void test_multiple_streams(void) {
    printf("Test: multiple concurrent streams... ");
    
    ipc_streaming_t *mgr = ipc_streaming_init();
    
    ipc_streaming_start(mgr, "stream_1");
    ipc_streaming_start(mgr, "stream_2");
    ipc_streaming_start(mgr, "stream_3");
    
    int active = 0;
    ipc_streaming_get_stats(mgr, &active, NULL);
    assert(active == 3);
    
    ipc_streaming_complete(mgr, "stream_1");
    ipc_streaming_complete(mgr, "stream_2");
    
    ipc_streaming_get_stats(mgr, &active, NULL);
    assert(active == 1);
    
    ipc_streaming_destroy(mgr);
    printf("OK\n");
}

int main(void) {
    printf("=== IPC Streaming Tests ===\n");
    
    test_stream_session();
    test_multiple_streams();
    
    printf("\nAll tests passed!\n");
    return 0;
}
