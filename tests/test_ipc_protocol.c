/**
 * test_ipc_protocol.c - Unit tests for IPC protocol
 */

#include "ipc_protocol.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

void test_encode_decode() {
    printf("Test: encode/decode roundtrip... ");
    
    /* Create test message */
    ipc_message_t msg_in = {
        .type = IPC_MSG_TASK_SUBMIT,
        .payload = "{\"task\":\"test\"}",
        .payload_len = 16
    };
    
    /* Encode */
    uint8_t frame_buf[1024];
    ssize_t frame_size = ipc_encode_message(&msg_in, frame_buf, sizeof(frame_buf));
    assert(frame_size > 0);
    assert(frame_size == IPC_HEADER_SIZE + msg_in.payload_len);
    
    /* Decode */
    ipc_message_t msg_out;
    ipc_error_t err = ipc_decode_message(frame_buf, frame_size, &msg_out);
    assert(err == IPC_ERR_OK);
    assert(msg_out.type == msg_in.type);
    assert(msg_out.payload_len == msg_in.payload_len);
    assert(strcmp(msg_out.payload, msg_in.payload) == 0);
    
    ipc_free_message(&msg_out);
    printf("OK\n");
}

void test_error_response() {
    printf("Test: error response creation... ");
    
    ipc_message_t msg;
    ipc_error_t err = ipc_create_error_response(IPC_ERR_TIMEOUT, "Request timed out", &msg);
    
    assert(err == IPC_ERR_OK);
    assert(msg.type == IPC_MSG_RESPONSE_ERROR);
    assert(msg.payload != NULL);
    assert(msg.payload_len > 0);
    assert(strstr(msg.payload, "\"ok\":false") != NULL);
    assert(strstr(msg.payload, "Request timed out") != NULL);
    
    ipc_free_message(&msg);
    printf("OK\n");
}

void test_invalid_version() {
    printf("Test: invalid version handling... ");
    
    uint8_t frame[IPC_HEADER_SIZE + 4];
    frame[0] = 0;  /* length */
    frame[1] = 0;
    frame[2] = 0;
    frame[3] = sizeof(frame);
    frame[4] = 0xFF;  /* Invalid version */
    frame[5] = IPC_MSG_PING;
    
    /* Set correct length */
    *(uint32_t*)frame = htonl(sizeof(frame));
    
    ipc_message_t msg;
    ipc_error_t err = ipc_decode_message(frame, sizeof(frame), &msg);
    
    assert(err == IPC_ERR_INVALID_VERSION);
    printf("OK\n");
}

void test_frame_too_large() {
    printf("Test: frame size limit... ");
    
    /* Create message with oversized payload */
    char large_payload[IPC_MAX_PAYLOAD_SIZE + 100];
    memset(large_payload, 'A', sizeof(large_payload));
    large_payload[sizeof(large_payload) - 1] = '\0';
    
    ipc_message_t msg = {
        .type = IPC_MSG_TASK_SUBMIT,
        .payload = large_payload,
        .payload_len = sizeof(large_payload)
    };
    
    uint8_t frame_buf[IPC_MAX_FRAME_SIZE + 1000];
    ssize_t frame_size = ipc_encode_message(&msg, frame_buf, sizeof(frame_buf));
    
    assert(frame_size < 0);  /* Should fail */
    printf("OK\n");
}

void test_empty_payload() {
    printf("Test: empty payload... ");
    
    ipc_message_t msg_in = {
        .type = IPC_MSG_PING,
        .payload = NULL,
        .payload_len = 0
    };
    
    uint8_t frame_buf[256];
    ssize_t frame_size = ipc_encode_message(&msg_in, frame_buf, sizeof(frame_buf));
    assert(frame_size == IPC_HEADER_SIZE);
    
    ipc_message_t msg_out;
    ipc_error_t err = ipc_decode_message(frame_buf, frame_size, &msg_out);
    assert(err == IPC_ERR_OK);
    assert(msg_out.type == msg_in.type);
    assert(msg_out.payload_len == 0);
    assert(msg_out.payload == NULL);
    
    ipc_free_message(&msg_out);
    printf("OK\n");
}

int main() {
    printf("=== IPC Protocol Unit Tests ===\n");
    
    test_encode_decode();
    test_error_response();
    test_invalid_version();
    test_frame_too_large();
    test_empty_payload();
    
    printf("All tests passed!\n");
    return 0;
}
