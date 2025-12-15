/* Rate Limiter: Main Implementation
 * 
 * Factory functions and configuration parsing for rate limiter.
 */

#include "rate_limiter.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Forward declarations */
extern rate_limiter_t *rate_limiter_memory_create(void);
extern rate_limiter_t *rate_limiter_redis_create(const distributed_rl_config_t *config);

/* Get default configuration */
void rate_limiter_get_default_config(distributed_rl_config_t *config) {
    if (!config) return;
    
    memset(config, 0, sizeof(distributed_rl_config_t));
    config->enabled = 0;  /* Disabled by default (CP1 mode) */
    config->backend = "memory";
    config->redis_host = "localhost";
    config->redis_port = 6379;
    config->redis_timeout_ms = 1000;
    config->local_cache_ttl_seconds = 10;
    config->sync_interval_seconds = 5;
    config->fallback_to_local = 1;  /* Fallback enabled by default */
}

/* Parse configuration from environment variables */
int rate_limiter_parse_config(distributed_rl_config_t *config) {
    if (!config) return -1;
    
    /* Start with defaults */
    rate_limiter_get_default_config(config);
    
    /* Check if distributed rate limiting is enabled */
    const char *enabled_str = getenv("GATEWAY_DISTRIBUTED_RATE_LIMIT_ENABLED");
    if (enabled_str && (strcmp(enabled_str, "true") == 0 || strcmp(enabled_str, "1") == 0)) {
        config->enabled = 1;
    }
    
    /* Get backend selection */
    /* Priority: GATEWAY_RATE_LIMIT_MODE > GATEWAY_RATE_LIMIT_BACKEND */
    const char *mode_str = getenv("GATEWAY_RATE_LIMIT_MODE");
    const char *backend_str = NULL;
    if (mode_str) {
        /* Map mode to backend: local=memory, redis=redis, hybrid=redis (with local cache) */
        if (strcmp(mode_str, "local") == 0) {
            backend_str = "memory";
        } else if (strcmp(mode_str, "redis") == 0) {
            backend_str = "redis";
        } else if (strcmp(mode_str, "hybrid") == 0) {
            backend_str = "redis"; /* Hybrid uses Redis with local cache */
        } else {
            /* Unknown mode, fallback to backend env var */
            backend_str = getenv("GATEWAY_RATE_LIMIT_BACKEND");
        }
    } else {
        backend_str = getenv("GATEWAY_RATE_LIMIT_BACKEND");
    }
    
    if (backend_str) {
        /* Allocate memory for backend string (will be freed by caller) */
        static char backend_buf[32];
        strncpy(backend_buf, backend_str, sizeof(backend_buf) - 1);
        backend_buf[sizeof(backend_buf) - 1] = '\0';
        config->backend = backend_buf;
    }
    
    /* Get Redis configuration */
    /* Priority: GATEWAY_RATE_LIMIT_REDIS_URI > GATEWAY_RATE_LIMIT_REDIS_HOST:PORT */
    const char *redis_uri = getenv("GATEWAY_RATE_LIMIT_REDIS_URI");
    if (redis_uri && strlen(redis_uri) > 0) {
        /* Parse URI: redis://host:port or redis://:password@host:port */
        /* For now, extract host:port (password support can be added later) */
        if (strncmp(redis_uri, "redis://", 8) == 0) {
            const char *host_start = redis_uri + 8;
            /* Skip password if present (format: redis://:password@host:port) */
            const char *at_sign = strchr(host_start, '@');
            if (at_sign) {
                host_start = at_sign + 1;
            }
            /* Extract host and port */
            const char *colon = strchr(host_start, ':');
            if (colon) {
                static char redis_host_buf[64];
                size_t host_len = (size_t)(colon - host_start);
                if (host_len < sizeof(redis_host_buf)) {
                    strncpy(redis_host_buf, host_start, host_len);
                    redis_host_buf[host_len] = '\0';
                    config->redis_host = redis_host_buf;
                }
                int port = atoi(colon + 1);
                if (port > 0) config->redis_port = port;
            } else {
                /* No port in URI, use default */
                static char redis_host_buf[64];
                strncpy(redis_host_buf, host_start, sizeof(redis_host_buf) - 1);
                redis_host_buf[sizeof(redis_host_buf) - 1] = '\0';
                config->redis_host = redis_host_buf;
            }
        }
    } else {
        /* Fallback to separate HOST:PORT env vars */
        const char *redis_host = getenv("GATEWAY_RATE_LIMIT_REDIS_HOST");
        if (redis_host) {
            static char redis_host_buf[64];
            strncpy(redis_host_buf, redis_host, sizeof(redis_host_buf) - 1);
            redis_host_buf[sizeof(redis_host_buf) - 1] = '\0';
            config->redis_host = redis_host_buf;
        }
        
        const char *redis_port_str = getenv("GATEWAY_RATE_LIMIT_REDIS_PORT");
        if (redis_port_str) {
            int port = atoi(redis_port_str);
            if (port > 0) config->redis_port = port;
        }
    }
    
    const char *redis_timeout_str = getenv("GATEWAY_RATE_LIMIT_REDIS_TIMEOUT_MS");
    if (redis_timeout_str) {
        int timeout = atoi(redis_timeout_str);
        if (timeout > 0) config->redis_timeout_ms = timeout;
    }
    
    /* Get local cache configuration */
    const char *cache_ttl_str = getenv("GATEWAY_RATE_LIMIT_LOCAL_CACHE_TTL_SECONDS");
    if (cache_ttl_str) {
        int ttl = atoi(cache_ttl_str);
        if (ttl > 0) config->local_cache_ttl_seconds = ttl;
    }
    
    const char *sync_interval_str = getenv("GATEWAY_RATE_LIMIT_SYNC_INTERVAL_SECONDS");
    if (sync_interval_str) {
        int interval = atoi(sync_interval_str);
        if (interval > 0) config->sync_interval_seconds = interval;
    }
    
    /* Get fallback configuration */
    const char *fallback_str = getenv("GATEWAY_RATE_LIMIT_FALLBACK_TO_LOCAL");
    if (fallback_str) {
        if (strcmp(fallback_str, "false") == 0 || strcmp(fallback_str, "0") == 0) {
            config->fallback_to_local = 0;
        } else {
            config->fallback_to_local = 1;
        }
    }
    
    return 0;
}

/* Create rate limiter based on configuration */
rate_limiter_t *rate_limiter_create(const distributed_rl_config_t *config) {
    if (!config) {
        /* Use defaults */
        distributed_rl_config_t default_config;
        rate_limiter_get_default_config(&default_config);
        return rate_limiter_memory_create();
    }
    
    /* Check if distributed mode is enabled */
    if (config->enabled && config->backend && strcmp(config->backend, "redis") == 0) {
        /* CP2: Distributed mode (Redis) */
        return rate_limiter_redis_create(config);
    } else {
        /* CP1: Memory mode (backward compatible) */
        return rate_limiter_memory_create();
    }
}

/* Destroy rate limiter */
void rate_limiter_destroy(rate_limiter_t *limiter) {
    if (!limiter) return;
    
    if (limiter->cleanup) {
        limiter->cleanup(limiter);
    }
    
    free(limiter);
}

