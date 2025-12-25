/**
 * nats_resilience.h - NATS connection resilience management
 * 
 * Features:
 * - Connection state tracking (connected/degraded/reconnecting)
 * - Exponential backoff for reconnections
 * - Inflight request limiting
 * - Quick error responses when NATS unavailable
 */

#ifndef NATS_RESILIENCE_H
#define NATS_RESILIENCE_H

#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * NATS connection state
 */
typedef enum {
    NATS_STATE_DISCONNECTED,   /* Not connected */
    NATS_STATE_CONNECTED,       /* Connected and healthy */
    NATS_STATE_DEGRADED,        /* Connected but experiencing errors */
    NATS_STATE_RECONNECTING,    /* Attempting to reconnect */
} nats_connection_state_t;

/**
 * Resilience configuration
 */
typedef struct {
    int max_inflight;           /* Max concurrent requests (default: 100) */
    int min_backoff_ms;         /* Min backoff delay (default: 100ms) */
    int max_backoff_ms;         /* Max backoff delay (default: 30000ms) */
    int degraded_threshold;     /* Error count to enter degraded (default: 3) */
    int reconnect_attempts;     /* Max reconnect attempts (0 = infinite) */
} nats_resilience_config_t;

/**
 * Resilience context (opaque)
 */
typedef struct nats_resilience_t nats_resilience_t;

/**
 * Create resilience manager
 * 
 * @param config  Configuration (NULL for defaults)
 * @return Resilience handle, or NULL on error
 */
nats_resilience_t* nats_resilience_init(const nats_resilience_config_t *config);

/**
 * Get current connection state
 * 
 * @param res  Resilience handle
 * @return Current state
 */
nats_connection_state_t nats_resilience_get_state(const nats_resilience_t *res);

/**
 * Check if can accept new request (inflight limit)
 * 
 * @param res  Resilience handle
 * @return 1 if can accept, 0 if overloaded
 */
int nats_resilience_can_accept(nats_resilience_t *res);

/**
 * Mark request start (increment inflight)
 * 
 * @param res  Resilience handle
 */
void nats_resilience_request_start(nats_resilience_t *res);

/**
 * Mark request complete (decrement inflight)
 * 
 * @param res      Resilience handle
 * @param success  1 if successful, 0 if error
 */
void nats_resilience_request_complete(nats_resilience_t *res, int success);

/**
 * Mark connection successful
 * 
 * @param res  Resilience handle
 */
void nats_resilience_mark_connected(nats_resilience_t *res);

/**
 * Get backoff delay (ms) for reconnection
 * 
 * @param res  Resilience handle
 * @return Delay in milliseconds
 */
int nats_resilience_get_backoff_ms(const nats_resilience_t *res);

/**
 * Get statistics
 * 
 * @param res                Resilience handle
 * @param inflight           Output: current inflight count
 * @param total_errors       Output: total error count
 * @param reconnect_count    Output: reconnection attempts
 */
void nats_resilience_get_stats(const nats_resilience_t *res,
                                int *inflight,
                                size_t *total_errors,
                                int *reconnect_count);

/**
 * Destroy resilience manager
 * 
 * @param res  Resilience handle
 */
void nats_resilience_destroy(nats_resilience_t *res);

#ifdef __cplusplus
}
#endif

#endif /* NATS_RESILIENCE_H */
