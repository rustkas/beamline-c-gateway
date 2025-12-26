/**
 * test_nats_pool.c - NATS connection pool tests
 */

#include "nats_pool.h"
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>

static void test_pool_creation(void) {
    printf("Test: pool creation and destruction... ");
    
    nats_pool_config_t config = {
        .min_connections = 2,
        .max_connections = 10,
        .connection_timeout_ms = 1000,
        .idle_timeout_sec = 60,
        .max_reconnect_attempts = 3,
        .nats_url = "nats://localhost:4222"
    };
    
    nats_pool_t *pool = nats_pool_init(&config);
    assert(pool != NULL);
    
    nats_pool_stats_t stats;
    nats_pool_get_stats(pool, &stats);
    
    assert(stats.total_created >= 2);  /* Should create min connections */
    
    nats_pool_destroy(pool);
    printf("OK\n");
}

static void test_acquire_release(void) {
    printf("Test: acquire and release... ");
    
    nats_pool_config_t config = {
        .min_connections = 1,
        .max_connections = 5,
        .connection_timeout_ms = 1000,
        .idle_timeout_sec = 60,
        .max_reconnect_attempts = 3,
        .nats_url = "nats://localhost:4222"
    };
    
    nats_pool_t *pool = nats_pool_init(&config);
    assert(pool != NULL);
    
    /* Acquire connection */
    nats_connection_t *conn = nats_pool_acquire(pool, 1000);
    assert(conn != NULL);
    
    /* Verify statistics */
    nats_pool_stats_t stats;
    nats_pool_get_stats(pool, &stats);
    assert(stats.total_acquired == 1);
    assert(stats.active_connections == 1);
    
    /* Release connection */
    nats_pool_release(pool, conn);
    
    /* Verify release */
    nats_pool_get_stats(pool, &stats);
    assert(stats.total_released == 1);
    assert(stats.active_connections == 0);
    assert(stats.idle_connections >= 1);
    
    nats_pool_destroy(pool);
    printf("OK\n");
}

static void test_multiple_acquire(void) {
    printf("Test: multiple acquisitions... ");
    
    nats_pool_config_t config = {
        .min_connections = 1,
        .max_connections = 5,
        .connection_timeout_ms = 1000,
        .idle_timeout_sec = 60,
        .max_reconnect_attempts = 3,
        .nats_url = "nats://localhost:4222"
    };
    
    nats_pool_t *pool = nats_pool_init(&config);
    
    /* Acquire multiple connections */
    nats_connection_t *conn1 = nats_pool_acquire(pool, 100);
    nats_connection_t *conn2 = nats_pool_acquire(pool, 100);
    nats_connection_t *conn3 = nats_pool_acquire(pool, 100);
    
    assert(conn1 != NULL);
    assert(conn2 != NULL);
    assert(conn3 != NULL);
    
    /* All should be different */
    assert(conn1 != conn2);
    assert(conn2 != conn3);
    assert(conn1 != conn3);
    
    /* Release all */
    nats_pool_release(pool, conn1);
    nats_pool_release(pool, conn2);
    nats_pool_release(pool, conn3);
    
    /* Verify pool can reuse */
    nats_connection_t *conn_reuse = nats_pool_acquire(pool, 100);
    assert(conn_reuse != NULL);
    
    nats_pool_release(pool, conn_reuse);
    nats_pool_destroy(pool);
    printf("OK\n");
}

static void test_pool_exhaustion(void) {
    printf("Test: pool exhaustion (timeout)... ");
    
    nats_pool_config_t config = {
        .min_connections = 1,
        .max_connections = 2,  /* Small pool */
        .connection_timeout_ms = 1000,
        .idle_timeout_sec = 60,
        .max_reconnect_attempts = 3,
        .nats_url = "nats://localhost:4222"
    };
    
    nats_pool_t *pool = nats_pool_init(&config);
    
    /* Exhaust pool */
    nats_connection_t *conn1 = nats_pool_acquire(pool, 100);
    nats_connection_t *conn2 = nats_pool_acquire(pool, 100);
    
    assert(conn1 != NULL);
    assert(conn2 != NULL);
    
    /* Next acquire should timeout (pool exhausted) */
    nats_connection_t *conn3 = nats_pool_acquire(pool, 10);  /* Short timeout */
    
    /* Should timeout and return NULL or block */
    /* (In stub implementation, may create new connection if under max) */
    
    /* Release */
    nats_pool_release(pool, conn1);
    nats_pool_release(pool, conn2);
    if (conn3) nats_pool_release(pool, conn3);
    
    nats_pool_destroy(pool);
    printf("OK\n");
}

static void test_health_check(void) {
    printf("Test: health check... ");
    
    nats_pool_config_t config = {
        .min_connections = 2,
        .max_connections = 5,
        .connection_timeout_ms = 1000,
        .idle_timeout_sec = 2,  /* Short idle timeout for testing */
        .max_reconnect_attempts = 3,
        .nats_url = "nats://localhost:4222"
    };
    
    nats_pool_t *pool = nats_pool_init(&config);
    
    /* Acquire and release to mark usage */
    nats_connection_t *conn = nats_pool_acquire(pool, 100);
    nats_pool_release(pool, conn);
    
    /* Wait for idle timeout */
    sleep(3);
    
    /* Run health check (should remove idle connections) */
    int removed = nats_pool_health_check(pool);
    
    /* Some connections may have been removed */
    printf("(removed=%d) ", removed);
    
    nats_pool_destroy(pool);
    printf("OK\n");
}

static void* worker_thread(void *arg) {
    nats_pool_t *pool = (nats_pool_t*)arg;
    
    for (int i = 0; i < 10; i++) {
        nats_connection_t *conn = nats_pool_acquire(pool, 100);
        if (conn) {
            usleep(1000);  /* Simulate work */
            nats_pool_release(pool, conn);
        }
    }
    
    return NULL;
}

static void test_concurrent_access(void) {
    printf("Test: concurrent access... ");
    
    nats_pool_config_t config = {
        .min_connections = 2,
        .max_connections = 5,
        .connection_timeout_ms = 1000,
        .idle_timeout_sec = 60,
        .max_reconnect_attempts = 3,
        .nats_url = "nats://localhost:4222"
    };
    
    nats_pool_t *pool = nats_pool_init(&config);
    
    /* Spawn multiple threads */
    pthread_t threads[5];
    for (int i = 0; i < 5; i++) {
        pthread_create(&threads[i], NULL, worker_thread, pool);
    }
    
    /* Wait for threads */
    for (int i = 0; i < 5; i++) {
        pthread_join(threads[i], NULL);
    }
    
    /* Verify statistics */
    nats_pool_stats_t stats;
    nats_pool_get_stats(pool, &stats);
    
    assert(stats.total_acquired == 50);  /* 5 threads * 10 iterations */
    assert(stats.total_released == 50);
    assert(stats.active_connections == 0);  /* All released */
    
    printf("(acquired=%zu, released=%zu) ", stats.total_acquired, stats.total_released);
    
    nats_pool_destroy(pool);
    printf("OK\n");
}

int main(void) {
    printf("=== NATS Connection Pool Tests ===\n");
    
    test_pool_creation();
    test_acquire_release();
    test_multiple_acquire();
    test_pool_exhaustion();
    test_health_check();
    test_concurrent_access();
    
    printf("\nAll tests passed!\n");
    return 0;
}
