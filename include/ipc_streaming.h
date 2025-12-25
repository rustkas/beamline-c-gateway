/**
 * ipc_streaming.h - IPC streaming support (Phase 3)
 */

#ifndef IPC_STREAMING_H
#define IPC_STREAMING_H

#include "ipc_protocol.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_STREAM_SESSIONS 64

/**
 * Stream session state
 */
typedef enum {
    STREAM_STATE_IDLE,
    STREAM_STATE_ACTIVE,
    STREAM_STATE_COMPLETE,
    STREAM_STATE_ERROR,
} stream_state_t;

/**
 * Stream session
 */
typedef struct {
    char session_id[64];
    stream_state_t state;
    size_t chunks_received;
    size_t total_bytes;
} stream_session_t;

/**
 * Streaming manager (opaque)
 */
typedef struct ipc_streaming_t ipc_streaming_t;

/**
 * Create streaming manager
 */
ipc_streaming_t* ipc_streaming_init(void);

/**
 * Start new stream session
 * 
 * @param mgr        Streaming manager
 * @param session_id Session identifier
 * @return 0 on success, -1 on error
 */
int ipc_streaming_start(ipc_streaming_t *mgr, const char *session_id);

/**
 * Handle stream data chunk
 * 
 * @param mgr        Streaming manager
 * @param session_id Session identifier
 * @param chunk_data Chunk data
 * @param chunk_size Chunk size
 * @return 0 on success, -1 on error
 */
int ipc_streaming_chunk(ipc_streaming_t *mgr, const char *session_id,
                        const void *chunk_data, size_t chunk_size);

/**
 * Complete stream session
 * 
 * @param mgr        Streaming manager
 * @param session_id Session identifier
 * @return 0 on success, -1 on error
 */
int ipc_streaming_complete(ipc_streaming_t *mgr, const char *session_id);

/**
 * Handle stream error
 * 
 * @param mgr        Streaming manager
 * @param session_id Session identifier
 * @return 0 on success, -1 on error
 */
int ipc_streaming_error(ipc_streaming_t *mgr, const char *session_id);

/**
 * Get stream statistics
 */
void ipc_streaming_get_stats(ipc_streaming_t *mgr,
                             int *active_streams,
                             size_t *total_chunks);

/**
 * Destroy streaming manager
 */
void ipc_streaming_destroy(ipc_streaming_t *mgr);

#ifdef __cplusplus
}
#endif

#endif /* IPC_STREAMING_H */
