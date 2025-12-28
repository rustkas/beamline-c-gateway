/**
 * soak_test_buffer_pool.c - Long-running buffer pool stress test
 * 
 * Purpose: Validate stability under sustained load
 */

#include "buffer_pool.h"
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
static unsigned long g_errors = 0;

static void sigint_handler(int sig) {
    (void)sig;
    printf("\n[soak] Received SIGINT, stopping...\n");
    g_running = 0;
}

static void* worker_thread(void *arg) {
    buffer_pool_t *pool = (buffer_pool_t*)arg;
    unsigned long local_acquired = 0;
    unsigned long local_released = 0;
    unsigned long local_errors = 0;
    
    while (g_running) {
        /* Acquire buffer */
        pooled_buffer_t *buf = buffer_pool_acquire(pool);
        if (buf) {
            local_acquired++;
            __sync_fetch_and_add(&g_total_acquired, 1);
            
            /* Simulate work */
            for (size_t i = 0; i < 100; i++) {
                ((char*)buf->data)[i % buf->capacity] = (char)i;
            }
            usleep((unsigned int)(rand() % 1000));  /* 0-1ms */
            
            /* Release */
            buffer_pool_release(pool, buf);
            local_released++;
            __sync_fetch_and_add(&g_total_released, 1);
        } else {
            local_errors++;
            __sync_fetch_and_add(&g_errors, 1);
        }
        
        /* Occasional yield */
        if (local_acquired % 100 == 0) {
            sched_yield();
        }
    }
    
    printf("[worker] Thread exiting: acquired=%lu, released=%lu, errors=%lu\n",
           local_acquired, local_released, local_errors);
    
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
    printf("Buffer Pool Soak Test\n");
    printf("========================================\n");
    printf("Duration:  %d seconds\n", duration_sec);
    printf("Threads:   %d\n", num_threads);
    printf("Press Ctrl+C to stop early\n");
    printf("\n");
    
    /* Setup signal handler */
    signal(SIGINT, sigint_handler);
    
    /* Create pool */
    buffer_pool_config_t config = {
        .buffer_size = 4096,
        .pool_size = 32,
        .thread_safe = 1
    };
    
    buffer_pool_t *pool = buffer_pool_create(&config);
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
            size_t total, available;
            uint64_t acquisitions, releases;
            buffer_pool_stats(pool, &total, &available, &acquisitions, &releases);
            
            unsigned long current_acquired = __sync_fetch_and_add(&g_total_acquired, 0);
            unsigned long current_released = __sync_fetch_and_add(&g_total_released, 0);
            unsigned long current_errors = __sync_fetch_and_add(&g_errors, 0);
            
            double rate = (double)current_acquired / (double)(now - start);
            
            printf("[%lds] acquired=%lu, released=%lu, errors=%lu, rate=%.0f/s, pool=%zu/%zu\n",
                   now - start, current_acquired, current_released, current_errors,
                   rate, available, total);
            
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
    size_t total, available;
    uint64_t acquisitions, releases;
    buffer_pool_stats(pool, &total, &available, &acquisitions, &releases);
    
    time_t elapsed = time(NULL) - start;
    
    printf("\n========================================\n");
    printf("Soak Test Complete\n");
    printf("========================================\n");
    printf("Duration:           %ld seconds\n", elapsed);
    printf("Total Acquired:     %lu\n", g_total_acquired);
    printf("Total Released:     %lu\n", g_total_released);
    printf("Errors:             %lu\n", g_errors);
    printf("Pool Stats:\n");
    printf("  Total buffers:    %zu\n", total);
    printf("  Available:        %zu\n", available);
    printf("  Pool acquired:    %lu\n", acquisitions);
    printf("  Pool released:    %lu\n", releases);
    printf("\nRate:               %.0f ops/sec\n", (double)g_total_acquired / (double)elapsed);
    
    /* Validate */
    int leak_detected = (g_total_acquired != g_total_released);
    int pool_mismatch = (acquisitions != releases);
    
    if (leak_detected) {
        printf("\n❌ FAILURE: Leak detected (acquired != released)\n");
    } else if (pool_mismatch) {
        printf("\n❌ FAILURE: Pool mismatch (pool stats inconsistent)\n");
    } else if (available != total) {
        printf("\n❌ FAILURE: Not all buffers returned to pool\n");
    } else {
        printf("\n✅ SUCCESS: No leaks, all buffers accounted for\n");
    }
    
    buffer_pool_destroy(pool);
    
    return (leak_detected || pool_mismatch) ? 1 : 0;
}
