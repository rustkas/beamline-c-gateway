/**
 * ipc_backpressure.h - IPC backpressure management
 * 
 * Prevents memory explosions by limiting inflight requests
 */

#ifndef IPC_BACKPRESSURE_H
#define IPC_BACKPRESSURE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Backpressure configuration
 */
typedef struct {
    int global_max_inflight;      /* Global limit (all connections) */
    int per_conn_max_inflight;    /* Per-connection limit */
} ipc_backpressure_config_t;

/**
 * Backpressure context (opaque)
 */
typedef struct ipc_backpressure_t ipc_backpressure_t;

/**
 * Create backpressure manager
 * 
 * @param config  Configuration (NULL for defaults: 1000 global, 10 per-conn)
 * @return Backpressure handle, or NULL on error
 */
ipc_backpressure_t* ipc_backpressure_init(const ipc_backpressure_config_t *config);

/**
 * Check if can accept new request
 * 
 * @param bp         Backpressure handle
 * @param conn_id    Connection ID
 * @return 1 if can accept, 0 if overloaded
 */
int ipc_backpressure_can_accept(ipc_backpressure_t *bp, int conn_id);

/**
 * Mark request start
 * 
 * @param bp         Backpressure handle
 * @param conn_id    Connection ID
 */
void ipc_backpressure_request_start(ipc_backpressure_t *bp, int conn_id);

/**
 * Mark request complete
 * 
 * @param bp         Backpressure handle
 * @param conn_id    Connection ID
 */
void ipc_backpressure_request_complete(ipc_backpressure_t *bp, int conn_id);

/**
 * Get statistics
 * 
 * @param bp                Backpressure handle
 * @param global_inflight   Output: global inflight count
 * @param rejections        Output: total rejections
 */
void ipc_backpressure_get_stats(ipc_backpressure_t *bp,
                                int *global_inflight,
                                size_t *rejections);

/**
 * Destroy backpressure manager
 * 
 * @param bp  Backpressure handle
 */
void ipc_backpressure_destroy(ipc_backpressure_t *bp);

#ifdef __cplusplus
}
#endif

#endif /* IPC_BACKPRESSURE_H */
