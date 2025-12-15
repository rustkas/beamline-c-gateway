#ifndef ATOMIC_COUNTER_H
#define ATOMIC_COUNTER_H

#include <stdatomic.h>
#include <stdint.h>

/**
 * Thread-safe atomic counter for Prometheus metrics
 * Uses C11 atomic operations for lock-free increments
 */
typedef struct {
    atomic_uint_fast64_t value;
} atomic_counter_t;

/**
 * Initialize atomic counter to 0
 */
static inline void atomic_counter_init(atomic_counter_t *counter) {
    atomic_init(&counter->value, 0);
}

/**
 * Increment counter by 1 (thread-safe)
 */
static inline void atomic_counter_inc(atomic_counter_t *counter) {
    atomic_fetch_add_explicit(&counter->value, 1, memory_order_relaxed);
}

/**
 * Increment counter by delta (thread-safe)
 */
static inline void atomic_counter_add(atomic_counter_t *counter, uint64_t delta) {
    atomic_fetch_add_explicit(&counter->value, delta, memory_order_relaxed);
}

/**
 * Get current counter value (thread-safe read)
 */
static inline uint64_t atomic_counter_get(const atomic_counter_t *counter) {
    return atomic_load_explicit(&counter->value, memory_order_relaxed);
}

/**
 * Reset counter to 0 (use with caution in multi-threaded context)
 */
static inline void atomic_counter_reset(atomic_counter_t *counter) {
    atomic_store_explicit(&counter->value, 0, memory_order_relaxed);
}

#endif // ATOMIC_COUNTER_H

