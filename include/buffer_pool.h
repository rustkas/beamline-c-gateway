/**
 * buffer_pool.h - Pre-allocated buffer pool for zero-copy operations
 * 
 * Task 21: Reduce memory allocations in hot path
 */

#ifndef BUFFER_POOL_H
#define BUFFER_POOL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Buffer pool handle
 */
typedef struct buffer_pool_t buffer_pool_t;

/**
 * Pooled buffer handle
 */
typedef struct {
    void *data;        /* Buffer data pointer */
    size_t size;       /* Buffer size */
    size_t capacity;   /* Buffer capacity */
    void *_internal;   /* Internal pool reference */
} pooled_buffer_t;

/**
 * Buffer pool configuration
 */
typedef struct {
    size_t buffer_size;      /* Size of each buffer */
    size_t pool_size;        /* Number of buffers in pool */
    int thread_safe;         /* 1 for thread-safe, 0 for faster single-threaded */
} buffer_pool_config_t;

/**
 * Create buffer pool
 * 
 * @param config  Pool configuration
 * @return Pool handle on success, NULL on error
 */
buffer_pool_t* buffer_pool_create(const buffer_pool_config_t *config);

/**
 * Acquire buffer from pool
 * 
 * Returns a pre-allocated buffer. If pool is empty, may allocate new buffer
 * or return NULL depending on configuration.
 * 
 * @param pool  Pool handle
 * @return Buffer on success, NULL if pool exhausted
 */
pooled_buffer_t* buffer_pool_acquire(buffer_pool_t *pool);

/**
 * Release buffer back to pool
 * 
 * @param pool    Pool handle
 * @param buffer  Buffer to release
 */
void buffer_pool_release(buffer_pool_t *pool, pooled_buffer_t *buffer);

/**
 * Get pool statistics
 * 
 * @param pool             Pool handle
 * @param total_buffers    Output: total buffers in pool
 * @param available        Output: available buffers
 * @param acquisitions     Output: total acquisitions
 * @param releases         Output: total releases
 */
void buffer_pool_stats(const buffer_pool_t *pool,
                       size_t *total_buffers,
                       size_t *available,
                       uint64_t *acquisitions,
                       uint64_t *releases);

/**
 * Destroy buffer pool
 * 
 * Frees all buffers in pool.
 * 
 * @param pool  Pool handle
 */
void buffer_pool_destroy(buffer_pool_t *pool);

#ifdef __cplusplus
}
#endif

#endif /* BUFFER_POOL_H */
