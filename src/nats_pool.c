/**
 * nats_pool.c - NATS connection pool implementation
 * 
 * Task 23: Connection pooling to reduce connection overhead
 */

#define _GNU_SOURCE
#include "nats_pool.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>

/**
 * Pooled connection wrapper
 */
struct nats_connection_t {
    void *nats_handle;              /* Opaque NATS connection (natsConnection*) */
    time_t created_at;              /* Creation timestamp */
    time_t last_used;               /* Last usage timestamp */
    int in_use;                     /* 1 if currently acquired */
    int healthy;                    /* 1 if health check passed */
    int reconnect_attempts;         /* Reconnection attempts */
};

/**
 * Connection pool
 */
struct nats_pool_t {
    nats_pool_config_t config;
    
    /* Connection array */
    nats_connection_t *connections;
    size_t num_connections;
    size_t max_connections;
    
    /* Synchronization */
    pthread_mutex_t mutex;
    pthread_cond_t cond_available; /* Signal when connection available */
    
    /* Statistics */
    nats_pool_stats_t stats;
    
    /* Shutdown flag */
    int shutdown;
};

/**
 * Get current time in seconds
 */
static time_t now_sec(void) {
    return time(NULL);
}

/**
 * Create a new NATS connection
 * 
 * NOTE: This is a stub. In real implementation, would use natsConnection_ConnectTo()
 */
static void* create_nats_connection(const char *url, int timeout_ms) {
    (void)url;
    (void)timeout_ms;
    
    /* Stub: In real implementation, use natsConnection_ConnectTo() */
    /* For now, return a dummy pointer to indicate "connection created" */
    void *conn = malloc(16);  /* Placeholder */
    return conn;
}

/**
 * Destroy a NATS connection
 */
static void destroy_nats_connection(void *handle) {
    if (!handle) return;
    
    /* Stub: In real implementation, use natsConnection_Destroy() */
    free(handle);
}

/**
 * Health check a NATS connection
 */
static int health_check_connection(void *handle) {
    if (!handle) return 0;
    
    /* Stub: In real implementation, use natsConnection_Status() */
    /* For now, assume healthy */
    return 1;
}

nats_pool_t* nats_pool_init(const nats_pool_config_t *config) {
    if (!config || !config->nats_url) {
        return NULL;
    }
    
    nats_pool_t *pool = (nats_pool_t*)calloc(1, sizeof(nats_pool_t));
    if (!pool) {
        return NULL;
    }
    
    /* Copy configuration */
    pool->config = *config;
    
    /* Validate config */
    if (pool->config.min_connections < 1) pool->config.min_connections = 1;
    if (pool->config.max_connections < pool->config.min_connections) {
        pool->config.max_connections = pool->config.min_connections;
    }
    if (pool->config.connection_timeout_ms <= 0) pool->config.connection_timeout_ms = 5000;
    if (pool->config.idle_timeout_sec <= 0) pool->config.idle_timeout_sec = 60;
    
    /* Allocate connection array */
    pool->max_connections = config->max_connections;
    pool->connections = (nats_connection_t*)calloc(pool->max_connections, 
                                                   sizeof(nats_connection_t));
    if (!pool->connections) {
        free(pool);
        return NULL;
    }
    
    /* Initialize synchronization */
    pthread_mutex_init(&pool->mutex, NULL);
    pthread_cond_init(&pool->cond_available, NULL);
    
    /* Create minimum connections */
    time_t now = now_sec();
    for (size_t i = 0; i < pool->config.min_connections; i++) {
        void *handle = create_nats_connection(pool->config.nats_url,
                                              pool->config.connection_timeout_ms);
        if (handle) {
            pool->connections[i].nats_handle = handle;
            pool->connections[i].created_at = now;
            pool->connections[i].last_used = now;
            pool->connections[i].in_use = 0;
            pool->connections[i].healthy = 1;
            pool->connections[i].reconnect_attempts = 0;
            pool->num_connections++;
            pool->stats.total_created++;
            pool->stats.idle_connections++;  /* Track idle */
        }
    }
    
    printf("[nats_pool] Initialized with %zu/%zu connections (url=%s)\n",
           pool->num_connections, pool->max_connections, pool->config.nats_url);
    
    return pool;
}

nats_connection_t* nats_pool_acquire(nats_pool_t *pool, int timeout_ms) {
    if (!pool) return NULL;
    
    pthread_mutex_lock(&pool->mutex);
    
    pool->stats.total_acquired++;
    
    struct timespec ts;
    if (timeout_ms > 0) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        ts.tv_sec = tv.tv_sec + (timeout_ms / 1000);
        ts.tv_nsec = (tv.tv_usec * 1000) + ((timeout_ms % 1000) * 1000000);
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_sec++;
            ts.tv_nsec -= 1000000000;
        }
    }
    
    nats_connection_t *conn = NULL;
    time_t now = now_sec();
    
    /* Wait loop */
    while (!pool->shutdown) {
        /* Try to find idle healthy connection */
        for (size_t i = 0; i < pool->num_connections; i++) {
            if (!pool->connections[i].in_use && pool->connections[i].healthy) {
                conn = &pool->connections[i];
                conn->in_use = 1;
                conn->last_used = now;
                pool->stats.active_connections++;
                pool->stats.idle_connections--;
                goto done;
            }
        }
        
        /* Try to create new connection if under max */
        if (pool->num_connections < pool->max_connections) {
            void *handle = create_nats_connection(pool->config.nats_url,
                                                  pool->config.connection_timeout_ms);
            if (handle) {
                nats_connection_t *new_conn = &pool->connections[pool->num_connections];
                new_conn->nats_handle = handle;
                new_conn->created_at = now;
                new_conn->last_used = now;
                new_conn->in_use = 1;
                new_conn->healthy = 1;
                new_conn->reconnect_attempts = 0;
                
                pool->num_connections++;
                pool->stats.total_created++;
                pool->stats.active_connections++;
                
                conn = new_conn;
                goto done;
            }
        }
        
        /* Wait for available connection */
        if (timeout_ms == 0) {
            /* No wait */
            pool->stats.acquire_timeouts++;
            break;
        } else if (timeout_ms < 0) {
            /* Infinite wait */
            pthread_cond_wait(&pool->cond_available, &pool->mutex);
        } else {
            /* Timed wait */
            int rc = pthread_cond_timedwait(&pool->cond_available, &pool->mutex, &ts);
            if (rc == ETIMEDOUT) {
                pool->stats.acquire_timeouts++;
                break;
            }
        }
    }
    
done:
    pthread_mutex_unlock(&pool->mutex);
    return conn;
}

void nats_pool_release(nats_pool_t *pool, nats_connection_t *conn) {
    if (!pool || !conn) return;
    
    pthread_mutex_lock(&pool->mutex);
    
    conn->in_use = 0;
    conn->last_used = now_sec();
    
    pool->stats.active_connections--;
    pool->stats.idle_connections++;
    pool->stats.total_released++;
    
    /* Signal waiting threads */
    pthread_cond_signal(&pool->cond_available);
    
    pthread_mutex_unlock(&pool->mutex);
}

int nats_pool_get_stats(const nats_pool_t *pool, nats_pool_stats_t *stats) {
    if (!pool || !stats) return -1;
    
    pthread_mutex_lock((pthread_mutex_t*)&pool->mutex);
    *stats = pool->stats;
    pthread_mutex_unlock((pthread_mutex_t*)&pool->mutex);
    
    return 0;
}

int nats_pool_health_check(nats_pool_t *pool) {
    if (!pool) return 0;
    
    pthread_mutex_lock(&pool->mutex);
    
    int removed = 0;
    time_t now = now_sec();
    
    for (size_t i = 0; i < pool->num_connections; i++) {
        nats_connection_t *conn = &pool->connections[i];
        
        /* Skip in-use connections */
        if (conn->in_use) continue;
        
        /* Check idle timeout */
        time_t idle_time_sec = now - conn->last_used;
        if (idle_time_sec > (time_t)pool->config.idle_timeout_sec) {
            destroy_nats_connection(conn->nats_handle);
            conn->nats_handle = NULL;
            conn->healthy = 0;
            pool->stats.idle_connections--;
            pool->stats.total_destroyed++;
            removed++;
            continue;
        }
        
        /* Health check */
        if (conn->nats_handle && !health_check_connection(conn->nats_handle)) {
            destroy_nats_connection(conn->nats_handle);
            conn->nats_handle = NULL;
            conn->healthy = 0;
            pool->stats.health_check_failures++;
            pool->stats.idle_connections--;
            pool->stats.total_destroyed++;
            removed++;
        }
    }
    
    /* Compact array (remove dead connections) */
    if (removed > 0) {
        size_t write_idx = 0;
        for (size_t read_idx = 0; read_idx < pool->num_connections; read_idx++) {
            if (pool->connections[read_idx].nats_handle) {
                if (write_idx != read_idx) {
                    pool->connections[write_idx] = pool->connections[read_idx];
                }
                write_idx++;
            }
        }
        pool->num_connections = write_idx;
    }
    
    pthread_mutex_unlock(&pool->mutex);
    
    if (removed > 0) {
        printf("[nats_pool] Health check removed %d connections\n", removed);
    }
    
    return removed;
}

void nats_pool_destroy(nats_pool_t *pool) {
    if (!pool) return;
    
    pthread_mutex_lock(&pool->mutex);
    pool->shutdown = 1;
    pthread_cond_broadcast(&pool->cond_available);
    pthread_mutex_unlock(&pool->mutex);
    
    /* Close all connections */
    for (size_t i = 0; i < pool->num_connections; i++) {
        if (pool->connections[i].nats_handle) {
            destroy_nats_connection(pool->connections[i].nats_handle);
        }
    }
    
    printf("[nats_pool] Destroyed (created=%zu, destroyed=%zu, acquired=%zu)\n",
           pool->stats.total_created, pool->stats.total_destroyed,
           pool->stats.total_acquired);
    
    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->cond_available);
    free(pool->connections);
    free(pool);
}

void* nats_pool_get_handle(nats_connection_t *conn) {
    return conn ? conn->nats_handle : NULL;
}
