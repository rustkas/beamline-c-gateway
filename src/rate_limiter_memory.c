/* Rate Limiter: In-Memory Implementation (CP1)
 * 
 * This is the CP1 implementation: simple in-memory fixed-window rate limiting.
 * Used as fallback when distributed rate limiting is disabled or unavailable.
 */

#include "rate_limiter.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

#define MAX_ENDPOINTS RL_ENDPOINT_MAX

/* In-memory rate limiter state */
typedef struct {
    unsigned int counters[MAX_ENDPOINTS];
    time_t window_started_at;
    int ttl_seconds;
    int limits[MAX_ENDPOINTS];
    unsigned long total_hits;
    unsigned long total_exceeded;
} memory_rl_state_t;

/* Get endpoint limit */
static int get_endpoint_limit(rl_endpoint_id_t endpoint) {
    const char *env_var = NULL;
    int default_limit = 0;
    
    switch (endpoint) {
        case RL_ENDPOINT_ROUTES_DECIDE:
            env_var = "GATEWAY_RATE_LIMIT_ROUTES_DECIDE_LIMIT";
            default_limit = 50;
            break;
        case RL_ENDPOINT_MESSAGES:
            env_var = "GATEWAY_RATE_LIMIT_MESSAGES";
            default_limit = 100;
            break;
        case RL_ENDPOINT_REGISTRY_BLOCKS:
            env_var = "GATEWAY_RATE_LIMIT_REGISTRY_BLOCKS";
            default_limit = 200;
            break;
        default:
            return 0;
    }
    
    if (env_var) {
        const char *env_val = getenv(env_var);
        if (env_val) {
            int limit = atoi(env_val);
            if (limit > 0) return limit;
        }
    }
    
    return default_limit;
}

/* Get TTL from environment */
static int get_ttl_seconds(void) {
    const char *env_val = getenv("GATEWAY_RATE_LIMIT_TTL_SECONDS");
    if (env_val) {
        int ttl = atoi(env_val);
        if (ttl > 0) return ttl;
    }
    return 60; /* Default: 60 seconds */
}

/* Initialize memory rate limiter */
static int memory_rl_init(rate_limiter_t *self, const distributed_rl_config_t *config) {
    (void)config; /* Not used in memory mode */
    
    memory_rl_state_t *state = (memory_rl_state_t *)calloc(1, sizeof(memory_rl_state_t));
    if (!state) return -1;
    
    state->ttl_seconds = get_ttl_seconds();
    state->window_started_at = 0;
    
    /* Initialize limits for each endpoint */
    for (int i = 0; i < MAX_ENDPOINTS; i++) {
        state->limits[i] = get_endpoint_limit((rl_endpoint_id_t)i);
        state->counters[i] = 0;
    }
    
    self->internal = state;
    return 0;
}

/* Check rate limit (memory mode) */
static rl_result_t memory_rl_check(rate_limiter_t *self,
                                   rl_endpoint_id_t endpoint,
                                   const char *tenant_id,
                                   const char *api_key,
                                   unsigned int *remaining_out) {
    (void)tenant_id; /* Not used in CP1 */
    (void)api_key;   /* Not used in CP1 */
    
    memory_rl_state_t *state = (memory_rl_state_t *)self->internal;
    if (!state) return RL_ERROR;
    
    time_t now = time(NULL);
    
    /* Reset window if expired */
    if (state->window_started_at == 0 || 
        (now - state->window_started_at) >= state->ttl_seconds) {
        state->window_started_at = now;
        for (int i = 0; i < MAX_ENDPOINTS; i++) {
            state->counters[i] = 0;
        }
    }
    
    if (endpoint >= MAX_ENDPOINTS) return RL_ERROR;
    
    int limit = state->limits[endpoint];
    unsigned int current = state->counters[endpoint];
    
    /* Track total rate limit checks */
    state->total_hits++;
    
    if (current >= (unsigned int)limit) {
        /* Rate limit exceeded */
        state->total_exceeded++;
        if (remaining_out) *remaining_out = 0;
        return RL_EXCEEDED;
    }
    
    /* Increment counter */
    state->counters[endpoint]++;
    if (remaining_out) {
        *remaining_out = (unsigned int)limit - state->counters[endpoint];
    }
    return RL_ALLOWED;
}

/* Cleanup memory rate limiter */
static void memory_rl_cleanup(rate_limiter_t *self) {
    if (self && self->internal) {
        free(self->internal);
        self->internal = NULL;
    }
}

/* Create memory rate limiter */
rate_limiter_t *rate_limiter_memory_create(void) {
    rate_limiter_t *limiter = (rate_limiter_t *)calloc(1, sizeof(rate_limiter_t));
    if (!limiter) return NULL;
    
    limiter->init = memory_rl_init;
    limiter->check = memory_rl_check;
    limiter->cleanup = memory_rl_cleanup;
    limiter->internal = NULL;
    
    if (limiter->init(limiter, NULL) != 0) {
        free(limiter);
        return NULL;
    }
    
    return limiter;
}
