/**
 * nats_pool.h - NATS connection pooling
 * 
 * Task 23: Connection pooling for NATS to improve performance
 * and reduce overhead of connection establishment.
 */

#ifndef NATS_POOL_H
#define NATS_POOL_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * NATS connection pool configuration
 */
typedef struct {
    size_t min_connections;          /* Minimum pool size */
    size_t max_connections;          /* Maximum pool size */
    int connection_timeout_ms;       /* Connection timeout */
    int idle_timeout_sec;            /* Idle connection timeout */
    int max_reconnect_attempts;      /* Max reconnect attempts per connection */
    const char *nats_url;            /* NATS server URL */
} nats_pool_config_t;

/**
 * Opaque NATS connection pool handle
 */
typedef struct nats_pool_t nats_pool_t;

/**
 * Opaque pooled connection handle
 */
typedef struct nats_connection_t nats_connection_t;

/**
 * Connection pool statistics
 */
typedef struct {
    size_t active_connections;       /* Currently active */
    size_t idle_connections;         /* Currently idle */
    size_t total_created;            /* Total connections created */
    size_t total_destroyed;          /* Total connections destroyed */
    size_t total_acquired;           /* Total acquisitions */
    size_t total_released;           /* Total releases */
    size_t acquire_timeouts;         /* Acquisition timeouts */
    size_t health_check_failures;    /* Failed health checks */
} nats_pool_stats_t;

/**
 * Initialize NATS connection pool
 * 
 * @param config  Pool configuration
 * @return Pool handle on success, NULL on error
 */
nats_pool_t* nats_pool_init(const nats_pool_config_t *config);

/**
 * Acquire a connection from the pool
 * 
 * Blocks until a connection is available or timeout occurs.
 * The caller must release the connection back to pool when done.
 * 
 * @param pool           Pool handle
 * @param timeout_ms     Acquisition timeout (0 = no wait, -1 = infinite)
 * @return Connection handle on success, NULL on timeout/error
 */
nats_connection_t* nats_pool_acquire(nats_pool_t *pool, int timeout_ms);

/**
 * Release a connection back to the pool
 * 
 * @param pool  Pool handle
 * @param conn  Connection to release
 */
void nats_pool_release(nats_pool_t *pool, nats_connection_t *conn);

/**
 * Get pool statistics
 * 
 * @param pool   Pool handle
 * @param stats  Output statistics
 * @return 0 on success, -1 on error
 */
int nats_pool_get_stats(const nats_pool_t *pool, nats_pool_stats_t *stats);

/**
 * Health check idle connections
 * 
 * Removes stale or unhealthy connections from pool.
 * Should be called periodically (e.g., every 30 seconds).
 * 
 * @param pool  Pool handle
 * @return Number of connections removed
 */
int nats_pool_health_check(nats_pool_t *pool);

/**
 * Destroy connection pool
 * 
 * Closes all connections and frees resources.
 * 
 * @param pool  Pool handle
 */
void nats_pool_destroy(nats_pool_t *pool);

/**
 * Get underlying NATS handle from pooled connection
 * 
 * For use with NATS C library functions.
 * 
 * @param conn  Pooled connection
 * @return NATS connection handle (void* to avoid nats.h dependency)
 */
void* nats_pool_get_handle(nats_connection_t *conn);

#ifdef __cplusplus
}
#endif

#endif /* NATS_POOL_H */
