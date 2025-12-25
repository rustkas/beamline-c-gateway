/**
 * ipc_backpressure.c - Backpressure implementation
 */

#include "ipc_backpressure.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define DEFAULT_GLOBAL_MAX    1000
#define DEFAULT_PER_CONN_MAX  10
#define MAX_CONNECTIONS       64

/**
 * Per-connection state
 */
typedef struct {
    int inflight_count;
} conn_state_t;

/**
 * Backpressure state
 */
struct ipc_backpressure_t {
    ipc_backpressure_config_t config;
    
    /* Tracking */
    int global_inflight;
    conn_state_t connections[MAX_CONNECTIONS];
    
    /* Statistics */
    size_t total_rejections;
};

ipc_backpressure_t* ipc_backpressure_init(const ipc_backpressure_config_t *config) {
    ipc_backpressure_t *bp = (ipc_backpressure_t*)calloc(1, sizeof(ipc_backpressure_t));
    if (!bp) {
        return NULL;
    }
    
    if (config) {
        bp->config = *config;
    } else {
        bp->config.global_max_inflight = DEFAULT_GLOBAL_MAX;
        bp->config.per_conn_max_inflight = DEFAULT_PER_CONN_MAX;
    }
    
    /* Validate */
    if (bp->config.global_max_inflight <= 0) {
        bp->config.global_max_inflight = DEFAULT_GLOBAL_MAX;
    }
    if (bp->config.per_conn_max_inflight <= 0) {
        bp->config.per_conn_max_inflight = DEFAULT_PER_CONN_MAX;
    }
    
    return bp;
}

int ipc_backpressure_can_accept(ipc_backpressure_t *bp, int conn_id) {
    if (!bp) return 0;
    
    if (conn_id < 0 || conn_id >= MAX_CONNECTIONS) {
        return 0;
    }
    
    /* Check global limit */
    if (bp->global_inflight >= bp->config.global_max_inflight) {
        return 0;  /* Global overload */
    }
    
    /* Check per-connection limit */
    if (bp->connections[conn_id].inflight_count >= bp->config.per_conn_max_inflight) {
        return 0;  /* Connection overload */
    }
    
    return 1;  /* OK */
}

void ipc_backpressure_request_start(ipc_backpressure_t *bp, int conn_id) {
    if (!bp) return;
    
    if (conn_id < 0 || conn_id >= MAX_CONNECTIONS) {
        return;
    }
    
    /* Check before incrementing (caller should have called can_accept) */
    if (!ipc_backpressure_can_accept(bp, conn_id)) {
        bp->total_rejections++;
        return;
    }
    
    bp->global_inflight++;
    bp->connections[conn_id].inflight_count++;
}

void ipc_backpressure_request_complete(ipc_backpressure_t *bp, int conn_id) {
    if (!bp) return;
    
    if (conn_id < 0 || conn_id >= MAX_CONNECTIONS) {
        return;
    }
    
    if (bp->global_inflight > 0) {
        bp->global_inflight--;
    }
    
    if (bp->connections[conn_id].inflight_count > 0) {
        bp->connections[conn_id].inflight_count--;
    }
}

void ipc_backpressure_get_stats(ipc_backpressure_t *bp,
                                int *global_inflight,
                                size_t *rejections) {
    if (!bp) return;
    
    if (global_inflight) {
        *global_inflight = bp->global_inflight;
    }
    
    if (rejections) {
        *rejections = bp->total_rejections;
    }
}

void ipc_backpressure_destroy(ipc_backpressure_t *bp) {
    if (!bp) return;
    
    printf("[backpressure] Destroyed (rejections=%zu, final_inflight=%d)\n",
           bp->total_rejections, bp->global_inflight);
    
    free(bp);
}
