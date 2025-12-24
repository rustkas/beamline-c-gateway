/**
 * ipc_protocol.c - IPC protocol implementation
 */

#include "ipc_protocol.h"
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>  /* htonl, ntohl */
#include <stdio.h>

const char* ipc_strerror(ipc_error_t err) {
    switch (err) {
        case IPC_ERR_OK:                return "Success";
        case IPC_ERR_INVALID_VERSION:   return "Unsupported protocol version";
        case IPC_ERR_INVALID_TYPE:      return "Unknown message type";
        case IPC_ERR_FRAME_TOO_LARGE:   return "Frame too large";
        case IPC_ERR_INVALID_PAYLOAD:   return "Invalid payload";
        case IPC_ERR_TIMEOUT:           return "Timeout";
        case IPC_ERR_CONNECTION_CLOSED: return "Connection closed";
        case IPC_ERR_INTERNAL:          return "Internal error";
        default:                        return "Unknown error";
    }
}

ssize_t ipc_encode_message(const ipc_message_t *msg, void *frame_buf, size_t frame_size) {
    if (!msg || !frame_buf) {
        return -1;
    }

    size_t total_size = IPC_HEADER_SIZE + msg->payload_len;
    
    if (total_size > frame_size) {
        return -1;  /* Buffer too small */
    }
    
    if (total_size > IPC_MAX_FRAME_SIZE) {
        return -1;  /* Frame too large */
    }

    ipc_frame_t *frame = (ipc_frame_t*)frame_buf;
    
    /* Encode header (network byte order for length) */
    frame->length  = htonl((uint32_t)total_size);
    frame->version = IPC_PROTOCOL_VERSION;
    frame->type    = (uint8_t)msg->type;
    
    /* Copy payload */
    if (msg->payload_len > 0 && msg->payload) {
        memcpy(frame->payload, msg->payload, msg->payload_len);
    }
    
    return (ssize_t)total_size;
}

ipc_error_t ipc_decode_message(const void *frame, size_t frame_size, ipc_message_t *msg) {
    if (!frame || !msg || frame_size < IPC_HEADER_SIZE) {
        return IPC_ERR_INVALID_PAYLOAD;
    }

    const ipc_frame_t *f = (const ipc_frame_t*)frame;
    
    /* Decode length (network byte order) */
    uint32_t length = ntohl(f->length);
    
    if (length != frame_size) {
        return IPC_ERR_INVALID_PAYLOAD;  /* Length mismatch */
    }
    
    if (length > IPC_MAX_FRAME_SIZE) {
        return IPC_ERR_FRAME_TOO_LARGE;
    }
    
    /* Check version */
    if (f->version != IPC_PROTOCOL_VERSION) {
        return IPC_ERR_INVALID_VERSION;
    }
    
    /* Decode type */
    msg->type = (ipc_message_type_t)f->type;
    
    /* Extract payload */
    msg->payload_len = length - IPC_HEADER_SIZE;
    
    if (msg->payload_len > 0) {
        msg->payload = (char*)malloc(msg->payload_len + 1);  /* +1 for null terminator */
        if (!msg->payload) {
            return IPC_ERR_INTERNAL;
        }
        
        memcpy(msg->payload, f->payload, msg->payload_len);
        msg->payload[msg->payload_len] = '\0';  /* Null-terminate for JSON parsing */
    } else {
        msg->payload = NULL;
    }
    
    return IPC_ERR_OK;
}

ipc_error_t ipc_create_error_response(ipc_error_t error_code, const char *error_msg, ipc_message_t *msg) {
    if (!msg) {
        return IPC_ERR_INVALID_PAYLOAD;
    }

    msg->type = IPC_MSG_RESPONSE_ERROR;
    
    /* Create JSON error response */
    const char *err_str = error_msg ? error_msg : ipc_strerror(error_code);
    
    /* Allocate buffer for JSON (conservative estimate) */
    size_t json_size = 128 + strlen(err_str);
    msg->payload = (char*)malloc(json_size);
    if (!msg->payload) {
        return IPC_ERR_INTERNAL;
    }
    
    int written = snprintf(msg->payload, json_size,
        "{\"ok\":false,\"error\":{\"code\":%d,\"message\":\"%s\"}}",
        error_code, err_str);
    
    if (written < 0 || (size_t)written >= json_size) {
        free(msg->payload);
        msg->payload = NULL;
        return IPC_ERR_INTERNAL;
    }
    
    msg->payload_len = (size_t)written;
    return IPC_ERR_OK;
}

void ipc_free_message(ipc_message_t *msg) {
    if (msg && msg->payload) {
        free(msg->payload);
        msg->payload = NULL;
        msg->payload_len = 0;
    }
}
