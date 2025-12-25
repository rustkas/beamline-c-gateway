/**
 * ipc_streaming.c - Streaming implementation
 */

#include "ipc_streaming.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

struct ipc_streaming_t {
    stream_session_t sessions[MAX_STREAM_SESSIONS];
    int active_count;
    size_t total_chunks;
};

ipc_streaming_t* ipc_streaming_init(void) {
    ipc_streaming_t *mgr = (ipc_streaming_t*)calloc(1, sizeof(ipc_streaming_t));
    return mgr;
}

static stream_session_t* find_session(ipc_streaming_t *mgr, const char *session_id) {
    if (!mgr || !session_id) return NULL;
    
    for (int i = 0; i < MAX_STREAM_SESSIONS; i++) {
        if (strcmp(mgr->sessions[i].session_id, session_id) == 0) {
            return &mgr->sessions[i];
        }
    }
    return NULL;
}

static stream_session_t* alloc_session(ipc_streaming_t *mgr) {
    if (!mgr) return NULL;
    
    for (int i = 0; i < MAX_STREAM_SESSIONS; i++) {
        if (mgr->sessions[i].state == STREAM_STATE_IDLE) {
            return &mgr->sessions[i];
        }
    }
    return NULL;
}

int ipc_streaming_start(ipc_streaming_t *mgr, const char *session_id) {
    if (!mgr || !session_id) return -1;
    
    stream_session_t *session = alloc_session(mgr);
    if (!session) return -1;
    
    strncpy(session->session_id, session_id, sizeof(session->session_id) - 1);
    session->state = STREAM_STATE_ACTIVE;
    session->chunks_received = 0;
    session->total_bytes = 0;
    
    mgr->active_count++;
    
    printf("[streaming] Started session: %s\n", session_id);
    return 0;
}

int ipc_streaming_chunk(ipc_streaming_t *mgr, const char *session_id,
                        const void *chunk_data, size_t chunk_size) {
    if (!mgr || !session_id) return -1;
    
    (void)chunk_data;  /* Not stored in this simple implementation */
    
    stream_session_t *session = find_session(mgr, session_id);
    if (!session || session->state != STREAM_STATE_ACTIVE) {
        return -1;
    }
    
    session->chunks_received++;
    session->total_bytes += chunk_size;
    mgr->total_chunks++;
    
    return 0;
}

int ipc_streaming_complete(ipc_streaming_t *mgr, const char *session_id) {
    if (!mgr || !session_id) return -1;
    
    stream_session_t *session = find_session(mgr, session_id);
    if (!session) return -1;
    
    session->state = STREAM_STATE_COMPLETE;
    mgr->active_count--;
    
    printf("[streaming] Completed session: %s (%zu chunks, %zu bytes)\n",
           session_id, session->chunks_received, session->total_bytes);
    
    return 0;
}

int ipc_streaming_error(ipc_streaming_t *mgr, const char *session_id) {
    if (!mgr || !session_id) return -1;
    
    stream_session_t *session = find_session(mgr, session_id);
    if (!session) return -1;
    
    session->state = STREAM_STATE_ERROR;
    mgr->active_count--;
    
    printf("[streaming] Error in session: %s\n", session_id);
    
    return 0;
}

void ipc_streaming_get_stats(ipc_streaming_t *mgr,
                             int *active_streams,
                             size_t *total_chunks) {
    if (!mgr) return;
    
    if (active_streams) *active_streams = mgr->active_count;
    if (total_chunks) *total_chunks = mgr->total_chunks;
}

void ipc_streaming_destroy(ipc_streaming_t *mgr) {
    if (!mgr) return;
    
    printf("[streaming] Destroyed (total_chunks=%zu)\n", mgr->total_chunks);
    free(mgr);
}
