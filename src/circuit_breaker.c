/**
 * circuit_breaker.c - Circuit breaker implementation
 */

#include "circuit_breaker.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

struct circuit_breaker {
    cb_config_t config;
    cb_state_t state;
    uint32_t consecutive_failures;
    uint32_t consecutive_successes;
    uint32_t half_open_calls;
    uint64_t total_successes;
    uint64_t total_failures;
    uint64_t total_rejects;
    uint64_t state_changed_at_ms;  /* Timestamp of last state change */
    pthread_mutex_t lock;
};

static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

circuit_breaker_t* circuit_breaker_create(const cb_config_t *config) {
    if (!config) {
        return NULL;
    }
    
    circuit_breaker_t *cb = calloc(1, sizeof(circuit_breaker_t));
    if (!cb) {
        return NULL;
    }
    
    cb->config = *config;
    cb->state = CB_STATE_CLOSED;
    cb->state_changed_at_ms = get_time_ms();
    pthread_mutex_init(&cb->lock, NULL);
    
    return cb;
}

void circuit_breaker_destroy(circuit_breaker_t *cb) {
    if (!cb) {
        return;
    }
    
    pthread_mutex_destroy(&cb->lock);
    free(cb);
}

static void transition_to_open(circuit_breaker_t *cb) {
    cb->state = CB_STATE_OPEN;
    cb->consecutive_failures = 0;
    cb->consecutive_successes = 0;
    cb->state_changed_at_ms = get_time_ms();
}

static void transition_to_half_open(circuit_breaker_t *cb) {
    cb->state = CB_STATE_HALF_OPEN;
    cb->half_open_calls = 0;
    cb->consecutive_successes = 0;
    cb->state_changed_at_ms = get_time_ms();
}

static void transition_to_closed(circuit_breaker_t *cb) {
    cb->state = CB_STATE_CLOSED;
    cb->consecutive_failures = 0;
    cb->consecutive_successes = 0;
    cb->half_open_calls = 0;
    cb->state_changed_at_ms = get_time_ms();
}

int circuit_breaker_allow_request(circuit_breaker_t *cb) {
    if (!cb) {
        return 0;
    }
    
    pthread_mutex_lock(&cb->lock);
    
    if (cb->state == CB_STATE_CLOSED) {
        /* Normal operation */
        pthread_mutex_unlock(&cb->lock);
        return 1;
    }
    
    if (cb->state == CB_STATE_OPEN) {
        /* Check if timeout expired */
        uint64_t now = get_time_ms();
        uint64_t elapsed = now - cb->state_changed_at_ms;
        
        if (elapsed >= cb->config.timeout_ms) {
            /* Try recovery */
            transition_to_half_open(cb);
            pthread_mutex_unlock(&cb->lock);
            return 1;
        }
        
        /* Still open - fast fail */
        cb->total_rejects++;
        pthread_mutex_unlock(&cb->lock);
        return 0;
    }
    
    if (cb->state == CB_STATE_HALF_OPEN) {
        /* Allow limited requests */
        if (cb->half_open_calls < cb->config.half_open_max_calls) {
            cb->half_open_calls++;
            pthread_mutex_unlock(&cb->lock);
            return 1;
        }
        
        /* Too many calls in half-open - reject */
        cb->total_rejects++;
        pthread_mutex_unlock(&cb->lock);
        return 0;
    }
    
    pthread_mutex_unlock(&cb->lock);
    return 0;
}

void circuit_breaker_on_success(circuit_breaker_t *cb) {
    if (!cb) {
        return;
    }
    
    pthread_mutex_lock(&cb->lock);
    
    cb->total_successes++;
    cb->consecutive_successes++;
    cb->consecutive_failures = 0;
    
    if (cb->state == CB_STATE_HALF_OPEN) {
        /* Check if we've had enough successes to close */
        if (cb->consecutive_successes >= cb->config.success_threshold) {
            transition_to_closed(cb);
        }
    }
    
    pthread_mutex_unlock(&cb->lock);
}

void circuit_breaker_on_failure(circuit_breaker_t *cb) {
    if (!cb) {
        return;
    }
    
    pthread_mutex_lock(&cb->lock);
    
    cb->total_failures++;
    cb->consecutive_failures++;
    cb->consecutive_successes = 0;
    
    if (cb->state == CB_STATE_CLOSED) {
        /* Check if we've exceeded failure threshold */
        if (cb->consecutive_failures >= cb->config.failure_threshold) {
            transition_to_open(cb);
        }
    } else if (cb->state == CB_STATE_HALF_OPEN) {
        /* Any failure in half-open reopens circuit */
        transition_to_open(cb);
    }
    
    pthread_mutex_unlock(&cb->lock);
}

cb_state_t circuit_breaker_get_state(circuit_breaker_t *cb) {
    if (!cb) {
        return CB_STATE_CLOSED;
    }
    
    pthread_mutex_lock(&cb->lock);
    cb_state_t state = cb->state;
    pthread_mutex_unlock(&cb->lock);
    
    return state;
}

void circuit_breaker_reset(circuit_breaker_t *cb) {
    if (!cb) {
        return;
    }
    
    pthread_mutex_lock(&cb->lock);
    transition_to_closed(cb);
    pthread_mutex_unlock(&cb->lock);
}

void circuit_breaker_get_stats(
    circuit_breaker_t *cb,
    uint64_t *success_count,
    uint64_t *failure_count,
    uint64_t *reject_count
) {
    if (!cb) {
        return;
    }
    
    pthread_mutex_lock(&cb->lock);
    
    if (success_count) *success_count = cb->total_successes;
    if (failure_count) *failure_count = cb->total_failures;
    if (reject_count) *reject_count = cb->total_rejects;
    
    pthread_mutex_unlock(&cb->lock);
}
