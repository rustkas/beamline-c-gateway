/**
 * buffer_pool.c - Buffer pool implementation
 * 
 * Task 21: Zero-copy optimization
 */

#include "buffer_pool.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

/**
 * Pool buffer entry
 */
typedef struct pool_entry_t {
    pooled_buffer_t buffer;
    int in_use;
    struct pool_entry_t *next;
} pool_entry_t;

/**
 * Buffer pool structure
 */
struct buffer_pool_t {
    size_t buffer_size;
    size_t pool_size;
    int thread_safe;
    
    pool_entry_t *entries;
    pool_entry_t *free_list;
    
    pthread_mutex_t mutex;
    
    /* Statistics */
    uint64_t acquisitions;
    uint64_t releases;
    size_t available;
};

buffer_pool_t* buffer_pool_create(const buffer_pool_config_t *config) {
    if (!config || config->buffer_size == 0 || config->pool_size == 0) {
        return NULL;
    }
    
    buffer_pool_t *pool = (buffer_pool_t*)calloc(1, sizeof(buffer_pool_t));
    if (!pool) {
        return NULL;
    }
    
    pool->buffer_size = config->buffer_size;
    pool->pool_size = config->pool_size;
    pool->thread_safe = config->thread_safe;
    pool->available = config->pool_size;
    
    /* Allocate pool entries */
    pool->entries = (pool_entry_t*)calloc(config->pool_size, sizeof(pool_entry_t));
    if (!pool->entries) {
        free(pool);
        return NULL;
    }
    
    /* Initialize each buffer and build free list */
    pool->free_list = NULL;
    for (size_t i = 0; i < config->pool_size; i++) {
        pool_entry_t *entry = &pool->entries[i];
        
        /* Allocate buffer data */
        entry->buffer.data = malloc(config->buffer_size);
        if (!entry->buffer.data) {
            /* Cleanup on allocation failure */
            for (size_t j = 0; j < i; j++) {
                free(pool->entries[j].buffer.data);
            }
            free(pool->entries);
            free(pool);
            return NULL;
        }
        
        entry->buffer.size = 0;
        entry->buffer.capacity = config->buffer_size;
        entry->buffer._internal = entry;
        entry->in_use = 0;
        
        /* Add to free list */
        entry->next = pool->free_list;
        pool->free_list = entry;
    }
    
    if (pool->thread_safe) {
        pthread_mutex_init(&pool->mutex, NULL);
    }
    
    printf("[buffer_pool] Created pool: %zu buffers x %zu bytes = %zu KB total\n",
           config->pool_size, config->buffer_size,
           (config->pool_size * config->buffer_size) / 1024);
    
    return pool;
}

pooled_buffer_t* buffer_pool_acquire(buffer_pool_t *pool) {
    if (!pool) {
        return NULL;
    }
    
    if (pool->thread_safe) {
        pthread_mutex_lock(&pool->mutex);
    }
    
    pool_entry_t *entry = pool->free_list;
    if (entry) {
        /* Remove from free list */
        pool->free_list = entry->next;
        entry->in_use = 1;
        entry->buffer.size = 0;  /* Reset size */
        pool->available--;
        pool->acquisitions++;
    }
    
    if (pool->thread_safe) {
        pthread_mutex_unlock(&pool->mutex);
    }
    
    return entry ? &entry->buffer : NULL;
}

void buffer_pool_release(buffer_pool_t *pool, pooled_buffer_t *buffer) {
    if (!pool || !buffer || !buffer->_internal) {
        return;
    }
    
    pool_entry_t *entry = (pool_entry_t*)buffer->_internal;
    
    if (pool->thread_safe) {
        pthread_mutex_lock(&pool->mutex);
    }
    
    if (entry->in_use) {
        /* Reset buffer */
        entry->buffer.size = 0;
        entry->in_use = 0;
        
        /* Add back to free list */
        entry->next = pool->free_list;
        pool->free_list = entry;
        
        pool->available++;
        pool->releases++;
    }
    
    if (pool->thread_safe) {
        pthread_mutex_unlock(&pool->mutex);
    }
}

void buffer_pool_stats(const buffer_pool_t *pool,
                       size_t *total_buffers,
                       size_t *available,
                       uint64_t *acquisitions,
                       uint64_t *releases) {
    if (!pool) {
        return;
    }
    
    if (pool->thread_safe) {
        pthread_mutex_lock((pthread_mutex_t*)&pool->mutex);
    }
    
    if (total_buffers) *total_buffers = pool->pool_size;
    if (available) *available = pool->available;
    if (acquisitions) *acquisitions = pool->acquisitions;
    if (releases) *releases = pool->releases;
    
    if (pool->thread_safe) {
        pthread_mutex_unlock((pthread_mutex_t*)&pool->mutex);
    }
}

void buffer_pool_destroy(buffer_pool_t *pool) {
    if (!pool) {
        return;
    }
    
    printf("[buffer_pool] Destroying pool (acquired=%lu, released=%lu)\n",
           (unsigned long)pool->acquisitions, (unsigned long)pool->releases);
    
    /* Free all buffer data */
    for (size_t i = 0; i < pool->pool_size; i++) {
        free(pool->entries[i].buffer.data);
    }
    
    free(pool->entries);
    
    if (pool->thread_safe) {
        pthread_mutex_destroy(&pool->mutex);
    }
    
    free(pool);
}
