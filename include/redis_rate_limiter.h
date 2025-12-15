#ifndef REDIS_RATE_LIMITER_H
#define REDIS_RATE_LIMITER_H

#include <stdint.h>
#include <stdbool.h>

/* Request context for rate limiting */
typedef struct {
    const char *client_ip;        /* Client IP address (from X-Forwarded-For or remote_addr) */
    const char *method;            /* HTTP method (GET, POST, etc.) */
    const char *path;              /* HTTP path (e.g., /api/v1/messages) */
    const char *tenant_id;         /* Optional tenant ID */
} redis_rl_request_ctx_t;

/* Rate limit decision */
typedef enum {
    REDIS_RL_ALLOW = 0,           /* Request allowed */
    REDIS_RL_DENY = 1,            /* Request denied (rate limit exceeded) */
    REDIS_RL_ERROR = 2            /* Error checking rate limit (fail-open) */
} redis_rl_decision_t;

/* Rate limit decision result */
typedef struct {
    redis_rl_decision_t decision;  /* Decision: allow, deny, or error */
    uint32_t limit;                /* Applied limit */
    uint32_t remaining;            /* Remaining requests in window */
    uint32_t retry_after_sec;      /* Retry-After header value */
    uint64_t reset_at;             /* Unix timestamp when window resets */
    bool degraded;                 /* True if Redis unavailable (circuit breaker open) */
} redis_rl_result_t;

/* Configuration structure */
typedef struct {
    bool enabled;                  /* Enable Redis rate limiting */
    char redis_uri[256];           /* Redis URI (redis://host:port or redis://:password@host:port) */
    char redis_host[64];           /* Redis host (if URI not set) */
    int redis_port;                /* Redis port (if URI not set) */
    int window_sec;                /* Fixed window size in seconds */
    uint32_t global_limit;         /* Global rate limit per window */
    uint32_t route_limit_messages; /* Limit for /api/v1/messages */
    uint32_t route_limit_chat;      /* Limit for /api/v1/chat */
    int pool_size;                 /* Redis connection pool size */
    int pool_acquire_timeout_ms;   /* Timeout to acquire connection from pool */
    int redis_timeout_ms;          /* Total timeout for Redis operation (including retries) */
    int retries;                   /* Number of retries for transient errors */
    int retry_backoff_ms;          /* Fixed backoff between retries */
    bool cb_fail_open;             /* Circuit breaker mode: true = fail-open, false = fail-closed */
    int cb_error_threshold;        /* Error count to open circuit breaker */
    int cb_sliding_window_sec;     /* Sliding window for error observation */
    int cb_cooldown_sec;           /* Cooldown before half-open transition */
    int cb_half_open_attempts;     /* Successful attempts needed to close from half-open */
} redis_rl_config_t;

/* Initialize Redis rate limiter */
int redis_rate_limiter_init(const redis_rl_config_t *config);

/* Check rate limit for a request */
int redis_rate_limiter_check(const redis_rl_request_ctx_t *ctx, redis_rl_result_t *result);

/* Get default configuration */
void redis_rate_limiter_get_default_config(redis_rl_config_t *config);

/* Parse configuration from environment variables */
int redis_rate_limiter_parse_config(redis_rl_config_t *config);

/* Cleanup and shutdown */
void redis_rate_limiter_cleanup(void);

/* Get current circuit breaker state (for metrics) */
const char *redis_rate_limiter_get_cb_state(void);

#endif /* REDIS_RATE_LIMITER_H */

