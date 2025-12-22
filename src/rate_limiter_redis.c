/* Rate Limiter: Redis Backend Implementation
 * 
 * ⚠️ EXPERIMENTAL / PoC CODE ⚠️
 * 
 * PoC implementation of distributed rate limiting using Redis backend.
 * Uses hiredis library for Redis operations.
 * 
 * Algorithm: Fixed-window using Redis INCR + EXPIRE
 * Key format: "rate_limit:{endpoint}:{tenant_id}:{api_key}:{window_start}"
 * 
 * Configuration via environment variables:
 * - GATEWAY_DISTRIBUTED_RATE_LIMIT_ENABLED=true to enable
 * - GATEWAY_RATE_LIMIT_BACKEND=redis to use Redis backend
 * - GATEWAY_RATE_LIMIT_REDIS_HOST, GATEWAY_RATE_LIMIT_REDIS_PORT for Redis connection
 * - GATEWAY_RATE_LIMIT_FALLBACK_TO_LOCAL=true (default) for automatic fallback to memory mode
 * 
 * TODO (CP3/Pre-Release - Productionization):
 * - Connection pooling for better performance (min 5, max 20 connections)
 * - Retry logic with exponential backoff (max 3 retries)
 * - Enhanced metrics for Redis operations (queries, errors, latency, connection pool stats)
 * - Circuit breaker for Redis failures
 * - Performance optimization (latency < 5ms p95 for Redis operations)
 * - Alternative backends (NATS JetStream, SQL)
 * 
 * Reference: docs/archive/dev/CP2_TECH_DEBT_SUMMARY.md (Gateway: Distributed Rate Limiting section)
 */

#include "rate_limiter.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <errno.h>

/* Forward declaration */
extern rate_limiter_t *rate_limiter_memory_create(void);

/* hiredis headers (will be available after installing hiredis) */
#ifdef HAVE_HIREDIS
#include <hiredis/hiredis.h>
#else
/* Minimal stubs for PoC compilation without hiredis */
typedef struct redisContext redisContext;
typedef struct redisReply redisReply;
#define REDIS_OK 0
#define REDIS_ERR -1
#endif

#define MAX_KEY_LEN 256
#define MAX_REDIS_ERROR_LEN 128

/* Redis rate limiter state */
typedef struct {
    redisContext *redis_ctx;
    char redis_host[64];
    int redis_port;
    int redis_timeout_ms;
    int ttl_seconds;
    int limits[RL_ENDPOINT_MAX];
    int fallback_to_local;
    rate_limiter_t *fallback_limiter; /* Fallback to memory mode */
    unsigned long redis_queries;
    unsigned long redis_errors;
    unsigned long fallback_used;
} redis_rl_state_t;

/* Get endpoint limit (same as memory mode) */
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

#ifdef HAVE_HIREDIS
/* Build Redis key */
static int build_redis_key(char *key_buf, size_t key_len,
                           rl_endpoint_id_t endpoint,
                           const char *tenant_id,
                           const char *api_key,
                           time_t window_start) {
    const char *endpoint_str = NULL;
    
    switch (endpoint) {
        case RL_ENDPOINT_ROUTES_DECIDE:
            endpoint_str = "routes_decide";
            break;
        case RL_ENDPOINT_MESSAGES:
            endpoint_str = "messages";
            break;
        case RL_ENDPOINT_REGISTRY_BLOCKS:
            endpoint_str = "registry_blocks";
            break;
        default:
            return -1;
    }
    
    const char *tid = tenant_id ? tenant_id : "";
    const char *key = api_key ? api_key : "";
    
    int len = snprintf(key_buf, key_len, "rate_limit:%s:%s:%s:%ld",
                       endpoint_str, tid, key, window_start);
    
    if (len < 0 || (size_t)len >= key_len) {
        return -1;
    }
    
    return 0;
}

/* Calculate window start */
static time_t get_window_start(int ttl_seconds) {
    time_t now = time(NULL);
    return (now / ttl_seconds) * ttl_seconds;
}
#endif

/* Initialize Redis rate limiter */
static int redis_rl_init(rate_limiter_t *self, const distributed_rl_config_t *config) {
    redis_rl_state_t *state = (redis_rl_state_t *)calloc(1, sizeof(redis_rl_state_t));
    if (!state) return -1;
    
    /* Copy configuration */
    strncpy(state->redis_host, config->redis_host ? config->redis_host : "localhost", 
            sizeof(state->redis_host) - 1);
    state->redis_port = config->redis_port > 0 ? config->redis_port : 6379;
    state->redis_timeout_ms = config->redis_timeout_ms > 0 ? config->redis_timeout_ms : 1000;
    state->ttl_seconds = get_ttl_seconds();
    state->fallback_to_local = config->fallback_to_local;
    
    /* Initialize limits */
    for (int i = 0; i < RL_ENDPOINT_MAX; i++) {
        state->limits[i] = get_endpoint_limit((rl_endpoint_id_t)i);
    }
    
    /* Connect to Redis */
#ifdef HAVE_HIREDIS
    struct timeval timeout = { 
        .tv_sec = state->redis_timeout_ms / 1000,
        .tv_usec = (state->redis_timeout_ms % 1000) * 1000
    };
    
    state->redis_ctx = redisConnectWithTimeout(state->redis_host, 
                                                state->redis_port, 
                                                timeout);
    
    if (!state->redis_ctx || state->redis_ctx->err) {
        /* Redis connection failed - use fallback if enabled */
        if (state->fallback_to_local) {
            /* Create fallback memory limiter */
            state->fallback_limiter = rate_limiter_memory_create();
            if (!state->fallback_limiter) {
                free(state);
                return -1;
            }
            /* Continue with fallback mode */
        } else {
            free(state);
            return -1;
        }
    }
#else
    /* No hiredis - use fallback */
    if (state->fallback_to_local) {
        state->fallback_limiter = rate_limiter_memory_create();
        if (!state->fallback_limiter) {
            free(state);
            return -1;
        }
    } else {
        free(state);
        return -1;
    }
#endif
    
    self->internal = state;
    return 0;
}

/* Check rate limit (Redis mode) */
static rl_result_t redis_rl_check(rate_limiter_t *self,
                                   rl_endpoint_id_t endpoint,
                                   const char *tenant_id,
                                   const char *api_key,
                                   unsigned int *remaining_out) {
    redis_rl_state_t *state = (redis_rl_state_t *)self->internal;
    if (!state) return RL_ERROR;
    
    /* Use fallback if Redis unavailable */
    if (!state->redis_ctx && state->fallback_limiter) {
        state->fallback_used++;
        return state->fallback_limiter->check(state->fallback_limiter, 
                                               endpoint, tenant_id, api_key, remaining_out);
    }
    
#ifdef HAVE_HIREDIS
    if (!state->redis_ctx) return RL_ERROR;
    
    /* Calculate window start */
    time_t window_start = get_window_start(state->ttl_seconds);
    
    /* Build Redis key */
    char redis_key[MAX_KEY_LEN];
    if (build_redis_key(redis_key, sizeof(redis_key), endpoint, tenant_id, api_key, window_start) != 0) {
        return RL_ERROR;
    }
    
    /* INCR key (atomic increment) */
    redisReply *reply = redisCommand(state->redis_ctx, "INCR %s", redis_key);
    if (!reply) {
        state->redis_errors++;
        /* Fallback to memory mode if enabled */
        if (state->fallback_to_local && state->fallback_limiter) {
            state->fallback_used++;
            return state->fallback_limiter->check(state->fallback_limiter, 
                                                   endpoint, tenant_id, api_key, remaining_out);
        }
        return RL_ERROR;
    }
    
    state->redis_queries++;
    
    /* Check if this is the first increment (set TTL) */
    if (reply->type == REDIS_REPLY_INTEGER) {
        long count = reply->integer;
        
        /* Set TTL on first increment */
        if (count == 1) {
            redisReply *expire_reply = redisCommand(state->redis_ctx, "EXPIRE %s %d", 
                                                    redis_key, state->ttl_seconds + 10);
            if (expire_reply) {
                freeReplyObject(expire_reply);
            }
        }
        
        /* Check limit */
        int limit = state->limits[endpoint];
        if (count > (long)limit) {
            freeReplyObject(reply);
            if (remaining_out) *remaining_out = 0;
            return RL_EXCEEDED;
        }
        
        /* Calculate remaining */
        if (remaining_out) {
            *remaining_out = (unsigned int)(limit - count);
        }
        
        freeReplyObject(reply);
        return RL_ALLOWED;
    }
    
    freeReplyObject(reply);
    return RL_ERROR;
#else
    /* No hiredis - use fallback */
    if (state->fallback_limiter) {
        state->fallback_used++;
        return state->fallback_limiter->check(state->fallback_limiter, 
                                               endpoint, tenant_id, api_key, remaining_out);
    }
    return RL_ERROR;
#endif
}

/* Cleanup Redis rate limiter */
static void redis_rl_cleanup(rate_limiter_t *self) {
    if (!self || !self->internal) return;
    
    redis_rl_state_t *state = (redis_rl_state_t *)self->internal;
    
#ifdef HAVE_HIREDIS
    if (state->redis_ctx) {
        redisFree(state->redis_ctx);
        state->redis_ctx = NULL;
    }
#endif
    
    if (state->fallback_limiter) {
        rate_limiter_destroy(state->fallback_limiter);
        state->fallback_limiter = NULL;
    }
    
    free(state);
    self->internal = NULL;
}

/* Create Redis rate limiter */
rate_limiter_t *rate_limiter_redis_create(const distributed_rl_config_t *config) {
    rate_limiter_t *limiter = (rate_limiter_t *)calloc(1, sizeof(rate_limiter_t));
    if (!limiter) return NULL;
    
    limiter->init = redis_rl_init;
    limiter->check = redis_rl_check;
    limiter->cleanup = redis_rl_cleanup;
    limiter->internal = NULL;
    
    if (limiter->init(limiter, config) != 0) {
        free(limiter);
        return NULL;
    }

    return limiter;
}
