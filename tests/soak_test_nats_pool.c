/**
 * soak_test_nats_pool.c - Long-running NATS pool stress test
 * 
 * Purpose: Validate NATS connection pool stability under sustained load
 */

#include "nats_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sched.h>

static volatile int g_running = 1;
static unsigned long g_total_acquired = 0;
static unsigned long g_total_released = 0;
static unsigned long g_total_timeouts = 0;
static unsigned long g_errors = 0;

static void sigint_handler(int sig) {
    (void)sig;
    printf("\n[soak] Received SIGINT, stopping...\n");
    g_running = 0;
}

static void* worker_thread(void *arg) {
    nats_pool_t *pool = (nats_pool_t*)arg;
    unsigned long local_acquired = 0;
    unsigned long local_released = 0;
    unsigned long local_timeouts = 0;
    unsigned long local_errors = 0;
    
    while (g_running) {
        /* Acquire connection with timeout */
        nats_connection_t *conn = nats_pool_acquire(pool, 1000);  /* 1s timeout */
        if (conn) {
            local_acquired++;
            __sync_fetch_and_add(&g_total_acquired, 1);
            
            /* Simulate work - small delay */
            usleep((unsigned int)(rand() % 5000));  /* 0-5ms */
            
            /* Release */
            nats_pool_release(pool, conn);
            local_released++;
            __sync_fetch_and_add(&g_total_released, 1);
        } else {
            local_timeouts++;
            __sync_fetch_and_add(&g_total_timeouts, 1);
        }
        
        /* Occasional yield */
        if (local_acquired % 100 == 0) {
            sched_yield();
        }
    }
    
    printf("[worker] Thread exiting: acquired=%lu, released=%lu, timeouts=%lu, errors=%lu\n",
           local_acquired, local_released, local_timeouts, local_errors);
    
    return NULL;
}

int main(int argc, char *argv[]) {
    int duration_sec = 300;  /* Default: 5 minutes */
    int num_threads = 8;
    
    if (argc > 1) {
        duration_sec = atoi(argv[1]);
    }
    if (argc > 2) {
        num_threads = atoi(argv[2]);
    }
    
    printf("========================================\n");
    printf("NATS Pool Soak Test\n");
    printf("========================================\n");
    printf("Duration:  %d seconds\n", duration_sec);
    printf("Threads:   %d\n", num_threads);
    printf("Press Ctrl+C to stop early\n");
    printf("\n");
    
    /* Setup signal handler */
    signal(SIGINT, sigint_handler);
    
    /* Create pool */
    nats_pool_config_t config = {
        .nats_url = "nats://localhost:4222",  /* Stub - won't actually connect */
        .min_connections = 4,
        .max_connections = 16,
        .connection_timeout_ms = 5000,
        .idle_timeout_sec = 60,
        .max_reconnect_attempts = 3
    };
    
    nats_pool_t *pool = nats_pool_init(&config);
    if (!pool) {
        fprintf(stderr, "Failed to create pool\n");
        return 1;
    }
    
    /* Create worker threads */
    pthread_t *threads = malloc((size_t)num_threads * sizeof(pthread_t));
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&threads[i], NULL, worker_thread, pool);
    }
    
    /* Monitor progress */
    time_t start = time(NULL);
    time_t last_report = start;
    
    while (g_running && (time(NULL) - start) < duration_sec) {
        sleep(1);
        
        time_t now = time(NULL);
        if (now - last_report >= 10) {
            nats_pool_stats_t stats;
            nats_pool_get_stats(pool, &stats);
            
            unsigned long current_acquired = __sync_fetch_and_add(&g_total_acquired, 0);
            unsigned long current_released = __sync_fetch_and_add(&g_total_released, 0);
            unsigned long current_timeouts = __sync_fetch_and_add(&g_total_timeouts, 0);
            
            double rate = (double)current_acquired / (double)(now - start);
            
            printf("[%lds] acquired=%lu, released=%lu, timeouts=%lu, rate=%.0f/s, pool: %zu active, %zu idle\n",
                   now - start, current_acquired, current_released, current_timeouts,
                   rate, stats.active_connections, stats.idle_connections);
            
            last_report = now;
        }
    }
    
    /* Stop workers */
    g_running = 0;
    
    /* Wait for threads */
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    free(threads);
    
    /* Final stats */
    nats_pool_stats_t stats;
    nats_pool_get_stats(pool, &stats);
    
    time_t elapsed = time(NULL) - start;
    
    printf("\n========================================\n");
    printf("Soak Test Complete\n");
    printf("========================================\n");
    printf("Duration:           %ld seconds\n", elapsed);
    printf("Total Acquired:     %lu\n", g_total_acquired);
    printf("Total Released:     %lu\n", g_total_released);
    printf("Total Timeouts:     %lu\n", g_total_timeouts);
    printf("Errors:             %lu\n", g_errors);
    printf("Pool Stats:\n");
    printf("  Total created:    %zu\n", stats.total_created);
    printf("  Active:           %zu\n", stats.active_connections);
    printf("  Idle:             %zu\n", stats.idle_connections);
    printf("  Pool acquired:    %zu\n", stats.total_acquired);
    printf("  Pool released:    %zu\n", stats.total_released);
    printf("  Pool timeouts:    %zu\n", stats.acquire_timeouts);
    printf("  Health failures:  %zu\n", stats.health_check_failures);
    printf("\nRate:               %.0f ops/sec\n", (double)g_total_acquired / (double)elapsed);
    
    /* Validate */
    int leak_detected = (g_total_acquired != g_total_released);
    int pool_mismatch = (stats.total_acquired != stats.total_released);
    int connections_still_active = (stats.active_connections > stats.idle_connections);
    
    if (leak_detected) {
        printf("\n❌ FAILURE: Leak detected (acquired != released)\n");
    } else if (pool_mismatch) {
        printf("\n❌ FAILURE: Pool mismatch (pool stats inconsistent)\n");
    } else if (connections_still_active) {
        printf("\n❌ FAILURE: Connections still borrower (active > idle)\n");
    } else {
        printf("\n✅ SUCCESS: No leaks, all connections returned\n");
    }
    
    nats_pool_destroy(pool);
    
    return (leak_detected || pool_mismatch || connections_still_active) ? 1 : 0;
}
