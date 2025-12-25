/**
 * circuit_breaker.h - Circuit breaker pattern for fault tolerance
 * 
 * Prevents cascade failures by fast-failing when error threshold exceeded
 */

#ifndef CIRCUIT_BREAKER_H
#define CIRCUIT_BREAKER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Circuit breaker state
 */
typedef enum {
    CB_STATE_CLOSED = 0,      /* Normal operation */
    CB_STATE_OPEN = 1,        /* Failing fast */
    CB_STATE_HALF_OPEN = 2    /* Testing recovery */
} cb_state_t;

/**
 * Circuit breaker handle (opaque)
 */
typedef struct circuit_breaker circuit_breaker_t;

/**
 * Circuit breaker configuration
 */
typedef struct {
    uint32_t failure_threshold;    /* Failures before opening */
    uint32_t success_threshold;    /* Successes to close from half-open */
    uint32_t timeout_ms;           /* Time in open state before half-open */
    uint32_t half_open_max_calls;  /* Max calls in half-open before re-opening */
} cb_config_t;

/**
 * Create circuit breaker
 * 
 * @param config  Configuration
 * @return Circuit breaker handle, or NULL on error
 */
circuit_breaker_t* circuit_breaker_create(const cb_config_t *config);

/**
 * Destroy circuit breaker
 */
void circuit_breaker_destroy(circuit_breaker_t *cb);

/**
 * Check if call is allowed
 * 
 * @param cb  Circuit breaker
 * @return 1 if allowed, 0 if circuit open (fast-fail)
 */
int circuit_breaker_allow_request(circuit_breaker_t *cb);

/**
 * Record successful call
 * 
 * @param cb  Circuit breaker
 */
void circuit_breaker_on_success(circuit_breaker_t *cb);

/**
 * Record failed call
 * 
 * @param cb  Circuit breaker
 */
void circuit_breaker_on_failure(circuit_breaker_t *cb);

/**
 * Get current state
 * 
 * @param cb  Circuit breaker
 * @return Current state
 */
cb_state_t circuit_breaker_get_state(circuit_breaker_t *cb);

/**
 * Reset circuit breaker to closed state
 * 
 * @param cb  Circuit breaker
 */
void circuit_breaker_reset(circuit_breaker_t *cb);

/**
 * Get stats
 * 
 * @param cb              Circuit breaker
 * @param success_count   Output success count
 * @param failure_count   Output failure count
 * @param reject_count    Output rejection count (fast-fails)
 */
void circuit_breaker_get_stats(
    circuit_breaker_t *cb,
    uint64_t *success_count,
    uint64_t *failure_count,
    uint64_t *reject_count
);

#ifdef __cplusplus
}
#endif

#endif /* CIRCUIT_BREAKER_H */
