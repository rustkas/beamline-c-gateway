/**
 * test_task_cancel.c - Test task cancellation
 */

#include "ipc_protocol.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static void test_cancel_message(void) {
    printf("Test: cancel message encoding... ");
    
    ipc_message_t msg = {
        .type = IPC_MSG_TASK_CANCEL,
        .payload = "{\"task_id\":\"task_123\"}",
        .payload_len = 23
    };
    
    uint8_t frame[256];
    ssize_t size = ipc_encode_message(&msg, frame, sizeof(frame));
    
    assert(size > 0);
    assert(size == IPC_HEADER_SIZE + 23);
    
    /* Decode */
    ipc_message_t decoded;
    ipc_error_t err = ipc_decode_message(frame, (size_t)size, &decoded);
    
    assert(err == IPC_ERR_OK);
    assert(decoded.type == IPC_MSG_TASK_CANCEL);
    assert(strcmp(decoded.payload, "{\"task_id\":\"task_123\"}") == 0);
    
    ipc_free_message(&decoded);
    
    printf("OK\n");
}

int main(void) {
    printf("=== Task Cancel Tests ===\n");
    
    test_cancel_message();
    
    printf("\nAll tests passed!\n");
    return 0;
}
