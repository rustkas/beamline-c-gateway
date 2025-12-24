/**
 * ipc_protocol.h - Binary IPC protocol for Unix socket communication
 * 
 * Protocol Design:
 *   Frame: [Length: 4 bytes][Version: 1 byte][Type: 1 byte][Payload: N bytes]
 *   
 *   Length:  Total frame size (including header), network byte order (big-endian)
 *   Version: Protocol version (currently 0x01)
 *   Type:    Message type (TaskSubmit, TaskQuery, etc.)
 *   Payload: JSON message (UTF-8 encoded)
 */

#ifndef IPC_PROTOCOL_H
#define IPC_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>  /* for ssize_t */

#ifdef __cplusplus
extern "C" {
#endif

/* Protocol version */
#define IPC_PROTOCOL_VERSION 0x01

/* Frame header size: length(4) + version(1) + type(1) */
#define IPC_HEADER_SIZE 6

/* Maximum frame size (4MB) */
#define IPC_MAX_FRAME_SIZE (4 * 1024 * 1024)

/* Maximum payload size */
#define IPC_MAX_PAYLOAD_SIZE (IPC_MAX_FRAME_SIZE - IPC_HEADER_SIZE)

/**
 * Message types
 */
typedef enum {
    IPC_MSG_TASK_SUBMIT      = 0x01,  /* Submit new task */
    IPC_MSG_TASK_QUERY       = 0x02,  /* Query task status */
    IPC_MSG_TASK_CANCEL      = 0x03,  /* Cancel running task */
    IPC_MSG_STREAM_SUBSCRIBE = 0x04,  /* Subscribe to stream */
    IPC_MSG_STREAM_DATA      = 0x05,  /* Streaming data chunk */
    IPC_MSG_STREAM_COMPLETE  = 0x06,  /* Stream completed */
    IPC_MSG_STREAM_ERROR     = 0x07,  /* Stream error */
    IPC_MSG_RESPONSE_OK      = 0x10,  /* Success response */
    IPC_MSG_RESPONSE_ERROR   = 0x11,  /* Error response */
    IPC_MSG_PING             = 0xF0,  /* Ping (keepalive) */
    IPC_MSG_PONG             = 0xF1,  /* Pong (response to ping) */
} ipc_message_type_t;

/**
 * Error codes
 */
typedef enum {
    IPC_ERR_OK                = 0,     /* No error */
    IPC_ERR_INVALID_VERSION   = 1,     /* Unsupported protocol version */
    IPC_ERR_INVALID_TYPE      = 2,     /* Unknown message type */
    IPC_ERR_FRAME_TOO_LARGE   = 3,     /* Frame exceeds max size */
    IPC_ERR_INVALID_PAYLOAD   = 4,     /* Malformed JSON payload */
    IPC_ERR_TIMEOUT           = 5,     /* Operation timed out */
    IPC_ERR_CONNECTION_CLOSED = 6,     /* Connection closed by peer */
    IPC_ERR_INTERNAL          = 99,    /* Internal server error */
} ipc_error_t;

/**
 * IPC frame structure (wire format)
 */
typedef struct {
    uint32_t length;              /* Total frame size (network byte order) */
    uint8_t  version;             /* Protocol version */
    uint8_t  type;                /* Message type */
    char     payload[];           /* Variable-length payload */
} __attribute__((packed)) ipc_frame_t;

/**
 * IPC message structure (in-memory representation)
 */
typedef struct {
    ipc_message_type_t type;      /* Message type */
    char              *payload;    /* JSON payload (null-terminated) */
    size_t             payload_len; /* Payload length (excluding null) */
} ipc_message_t;

/**
 * Encode message into wire frame
 * 
 * @param msg         Message to encode
 * @param frame_buf   Output buffer for frame (must be at least IPC_HEADER_SIZE + msg->payload_len)
 * @param frame_size  Size of frame_buf
 * @return Number of bytes written, or -1 on error
 */
ssize_t ipc_encode_message(const ipc_message_t *msg, void *frame_buf, size_t frame_size);

/**
 * Decode wire frame into message
 * 
 * @param frame       Frame buffer
 * @param frame_size  Size of frame
 * @param msg         Output message (caller must free msg->payload)
 * @return IPC_ERR_OK on success, error code otherwise
 */
ipc_error_t ipc_decode_message(const void *frame, size_t frame_size, ipc_message_t *msg);

/**
 * Create error response message
 * 
 * @param error_code  Error code
 * @param error_msg   Human-readable error message
 * @param msg         Output message (caller must free msg->payload)
 * @return IPC_ERR_OK on success
 */
ipc_error_t ipc_create_error_response(ipc_error_t error_code, const char *error_msg, ipc_message_t *msg);

/**
 * Free message payload
 */
void ipc_free_message(ipc_message_t *msg);

/**
 * Get error string for error code
 */
const char* ipc_strerror(ipc_error_t err);

#ifdef __cplusplus
}
#endif

#endif /* IPC_PROTOCOL_H */
