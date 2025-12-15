#ifndef RATE_LIMITER_H
#define RATE_LIMITER_H

#include <stdint.h>
#include <stdbool.h>

/* Rate limiter endpoint identifiers */
typedef enum {
    RL_ENDPOINT_ROUTES_DECIDE = 0,
    RL_ENDPOINT_MESSAGES = 1,
    RL_ENDPOINT_REGISTRY_BLOCKS = 2,
    RL_ENDPOINT_MAX = 3
} rl_endpoint_id_t;

/* Rate limiter result */
typedef enum {
    RL_ALLOWED = 0,           /* Request allowed */
    RL_EXCEEDED = 1,          /* Rate limit exceeded */
    RL_ERROR = 2              /* Error checking rate limit */
} rl_result_t;

/* Rate limiter configuration */
typedef struct {
    int enabled;                    /* Enable distributed rate limiting */
    const char *backend;            /* "redis" | "nats" | "memory" */
    const char *redis_host;         /* Redis host (if backend=redis) */
    int redis_port;                 /* Redis port */
    int redis_timeout_ms;           /* Redis connection timeout */
    int local_cache_ttl_seconds;   /* Local cache TTL */
    int sync_interval_seconds;     /* Background sync interval */
    int fallback_to_local;         /* Fallback to local-only if backend unavailable */
} distributed_rl_config_t;

/* Rate limiter interface */
typedef struct rate_limiter rate_limiter_t;

struct rate_limiter {
    /* Initialize rate limiter */
    int (*init)(rate_limiter_t *self, const distributed_rl_config_t *config);
    
    /* Check rate limit */
    rl_result_t (*check)(rate_limiter_t *self, 
                         rl_endpoint_id_t endpoint,
                         const char *tenant_id,
                         const char *api_key,
                         unsigned int *remaining_out);
    
    /* Cleanup rate limiter */
    void (*cleanup)(rate_limiter_t *self);
    
    /* Internal state (opaque) */
    void *internal;
};

/* Create rate limiter based on configuration */
rate_limiter_t *rate_limiter_create(const distributed_rl_config_t *config);

/* Destroy rate limiter */
void rate_limiter_destroy(rate_limiter_t *limiter);

/* Get default configuration */
void rate_limiter_get_default_config(distributed_rl_config_t *config);

/* Parse configuration from environment variables */
int rate_limiter_parse_config(distributed_rl_config_t *config);

/* Memory backend factory (CP1) */
rate_limiter_t *rate_limiter_memory_create(void);

/* Redis backend factory (CP2) */
rate_limiter_t *rate_limiter_redis_create(const distributed_rl_config_t *config);

#endif /* RATE_LIMITER_H */
