/**
 * ipc_capabilities.c - Capabilities implementation
 */

#include "ipc_capabilities.h"
#include <stdio.h>
#include <string.h>

/* Supported message types */
static const ipc_message_type_t SUPPORTED_TYPES[] = {
    IPC_MSG_TASK_SUBMIT,
    IPC_MSG_TASK_QUERY,
    IPC_MSG_TASK_CANCEL,
    IPC_MSG_PING,
    IPC_MSG_PONG,
    IPC_MSG_CAPABILITIES,
    IPC_MSG_RESPONSE_OK,
    IPC_MSG_RESPONSE_ERROR,
};

#define NUM_SUPPORTED_TYPES (sizeof(SUPPORTED_TYPES) / sizeof(SUPPORTED_TYPES[0]))

int ipc_get_capabilities_json(char *out_json, size_t buf_size) {
    if (!out_json || buf_size == 0) {
        return -1;
    }
    
    /* Build JSON */
    int written = snprintf(out_json, buf_size,
        "{"
        "\"protocol_version\":\"1.0\","
        "\"supported_versions\":[\"1.0\"],"
        "\"supported_message_types\":[");
    
    if (written < 0 || (size_t)written >= buf_size) {
        return -1;
    }
    
    /* Add message types */
    for (size_t i = 0; i < NUM_SUPPORTED_TYPES; i++) {
        int n = snprintf(out_json + written, buf_size - (size_t)written,
                        "%s0x%02x", i > 0 ? "," : "", SUPPORTED_TYPES[i]);
        if (n < 0 || written + n >= (int)buf_size) {
            return -1;
        }
        written += n;
    }
    
    /* Add remaining fields */
    int n = snprintf(out_json + written, buf_size - (size_t)written,
        "],"
        "\"max_payload_size\":%d,"
        "\"features\":[\"basic\"]"
        "}",
        IPC_MAX_PAYLOAD_SIZE);
    
    if (n < 0 || written + n >= (int)buf_size) {
        return -1;
    }
    
    return 0;
}

int ipc_is_message_type_supported(ipc_message_type_t msg_type) {
    for (size_t i = 0; i < NUM_SUPPORTED_TYPES; i++) {
        if (SUPPORTED_TYPES[i] == msg_type) {
            return 1;
        }
    }
    return 0;
}

int ipc_is_version_supported(uint8_t version) {
    return (version == IPC_PROTOCOL_VERSION) ? 1 : 0;
}
