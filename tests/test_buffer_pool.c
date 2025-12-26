/**
 * test_buffer_pool.c - Buffer pool tests
 */

#include "buffer_pool.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>

static void test_pool_creation(void) {
    printf("Test: buffer pool creation... ");
    
    buffer_pool_config_t config = {
        .buffer_size = 1024,
        .pool_size = 10,
        .thread_safe = 0
    };
    
    buffer_pool_t *pool = buffer_pool_create(&config);
    assert(pool != NULL);
    
    size_t total, available;
    buffer_pool_stats(pool, &total, &available, NULL, NULL);
    
    assert(total == 10);
    assert(available == 10);
    
    buffer_pool_destroy(pool);
    printf("OK\n");
}

static void test_acquire_release(void) {
    printf("Test: acquire and release... ");
    
    buffer_pool_config_t config = {
        .buffer_size = 512,
        .pool_size = 5,
        .thread_safe = 0
    };
    
    buffer_pool_t *pool = buffer_pool_create(&config);
    
    /* Acquire buffer */
    pooled_buffer_t *buf = buffer_pool_acquire(pool);
    assert(buf != NULL);
    assert(buf->capacity == 512);
    assert(buf->size == 0);
    
    /* Check stats */
    size_t available;
    uint64_t acquisitions;
    buffer_pool_stats(pool, NULL, &available, &acquisitions, NULL);
    assert(available == 4);
    assert(acquisitions == 1);
    
    /* Use buffer */
    strcpy((char*)buf->data, "test data");
    buf->size = strlen("test data");
    
    /* Release */
    buffer_pool_release(pool, buf);
    
    buffer_pool_stats(pool, NULL, &available, NULL, NULL);
    assert(available == 5);
    
    buffer_pool_destroy(pool);
    printf("OK\n");
}

static void test_pool_exhaustion(void) {
    printf("Test: pool exhaustion... ");
    
    buffer_pool_config_t config = {
        .buffer_size = 256,
        .pool_size = 3,
        .thread_safe = 0
    };
    
    buffer_pool_t *pool = buffer_pool_create(&config);
    
    /* Acquire all buffers */
    pooled_buffer_t *buf1 = buffer_pool_acquire(pool);
    pooled_buffer_t *buf2 = buffer_pool_acquire(pool);
    pooled_buffer_t *buf3 = buffer_pool_acquire(pool);
    
    assert(buf1 != NULL);
    assert(buf2 != NULL);
    assert(buf3 != NULL);
    
    /* Pool should be exhausted */
    pooled_buffer_t *buf4 = buffer_pool_acquire(pool);
    assert(buf4 == NULL);
    
    /* Release one */
    buffer_pool_release(pool, buf2);
    
    /* Now should be able to acquire */
    buf4 = buffer_pool_acquire(pool);
    assert(buf4 != NULL);
    
    /* Cleanup */
    buffer_pool_release(pool, buf1);
    buffer_pool_release(pool, buf3);
    buffer_pool_release(pool, buf4);
    
    buffer_pool_destroy(pool);
    printf("OK\n");
}

static void test_reuse(void) {
    printf("Test: buffer reuse... ");
    
    buffer_pool_config_t config = {
        .buffer_size = 128,
        .pool_size = 2,
        .thread_safe = 0
    };
    
    buffer_pool_t *pool = buffer_pool_create(&config);
    
    /* Acquire, use, release */
    pooled_buffer_t *buf1 = buffer_pool_acquire(pool);
    void *ptr1 = buf1->data;
    buffer_pool_release(pool, buf1);
    
    /* Acquire again - should get same buffer */
    pooled_buffer_t *buf2 = buffer_pool_acquire(pool);
    void *ptr2 = buf2->data;
    
    assert(ptr1 == ptr2);  /* Same underlying buffer */
    assert(buf2->size == 0);  /* Reset */
    
    buffer_pool_release(pool, buf2);
    buffer_pool_destroy(pool);
    printf("OK\n");
}

static void* thread_worker(void *arg) {
    buffer_pool_t *pool = (buffer_pool_t*)arg;
    
    for (int i = 0; i < 100; i++) {
        pooled_buffer_t *buf = buffer_pool_acquire(pool);
        if (buf) {
            /* Simulate work */
            memset(buf->data, 0xAB, 64);
            buf->size = 64;
            
            /* Release */
            buffer_pool_release(pool, buf);
        }
    }
    
    return NULL;
}

static void test_thread_safety(void) {
    printf("Test: thread safety... ");
    
    buffer_pool_config_t config = {
        .buffer_size = 512,
        .pool_size = 10,
        .thread_safe = 1  /* Thread-safe mode */
    };
    
    buffer_pool_t *pool = buffer_pool_create(&config);
    
    /* Spawn threads */
    pthread_t threads[4];
    for (int i = 0; i < 4; i++) {
        pthread_create(&threads[i], NULL, thread_worker, pool);
    }
    
    /* Wait */
    for (int i = 0; i < 4; i++) {
        pthread_join(threads[i], NULL);
    }
    
    /* Verify stats */
    uint64_t acquisitions, releases;
    buffer_pool_stats(pool, NULL, NULL, &acquisitions, &releases);
    
    assert(acquisitions == 400);  /* 4 threads * 100 */
    assert(releases == 400);
    
    buffer_pool_destroy(pool);
    printf("OK\n");
}

int main(void) {
    printf("=== Buffer Pool Tests ===\n");
    
    test_pool_creation();
    test_acquire_release();
    test_pool_exhaustion();
    test_reuse();
    test_thread_safety();
    
    printf("\nAll tests passed!\n");
    return 0;
}
