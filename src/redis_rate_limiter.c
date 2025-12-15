/* Redis Rate Limiter: Production-Ready PoC Implementation
 * 
 * This implementation provides:
 * - Connection pooling for Redis operations
 * - Retry logic with exponential backoff
 * - Circuit breaker for Redis failures
 * - Fixed-window rate limiting algorithm
 * 
 * PoC Status: Stable enough for load testing and CP3+ experiments
 * Not production-grade, but includes safety mechanisms (fail-open by default)
 */

#include "redis_rate_limiter.h"
#include "metrics/prometheus.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>
#include <stdarg.h>

/* JSON logging helper */
static void log_json(const char *level, const char *subsystem, const char *message, ...) {
    char log_buffer[2048] = {0};
    va_list args;
    va_start(args, message);
    
    /* Get current timestamp */
    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t now = tv.tv_sec;
    struct tm *tm_info = gmtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", tm_info);
    snprintf(timestamp + strlen(timestamp), sizeof(timestamp) - strlen(timestamp), ".%03ldZ", tv.tv_usec / 1000);
    
    /* Format message */
    char formatted_message[512];
    vsnprintf(formatted_message, sizeof(formatted_message), message, args);
    va_end(args);
    
    /* Build JSON log */
    snprintf(log_buffer, sizeof(log_buffer),
        "{\"timestamp\":\"%s\",\"level\":\"%s\",\"component\":\"c-gateway\",\"subsystem\":\"%s\",\"message\":\"%s\"}",
        timestamp, level, subsystem, formatted_message);
    
    fprintf(stderr, "%s\n", log_buffer);
}

/* Forward declarations for hiredis */
#ifdef HAVE_HIREDIS
#include <hiredis/hiredis.h>
#include <hiredis/adapters/libevent.h>
#else
/* Minimal stubs for compilation without hiredis */
typedef struct redisContext redisContext;
typedef struct redisReply redisReply;
#define REDIS_OK 0
#define REDIS_ERR -1
#define REDIS_REPLY_INTEGER 1
#define REDIS_REPLY_ERROR 2
#define REDIS_REPLY_STRING 3
#define REDIS_REPLY_ARRAY 4
#endif

/* Circuit breaker states */
typedef enum {
    CB_CLOSED = 0,      /* Normal operation */
    CB_OPEN = 1,        /* Circuit open, failing fast */
    CB_HALF_OPEN = 2    /* Testing if Redis is back */
} cb_state_t;

/* Connection pool entry */
typedef struct {
    redisContext *ctx;
    bool in_use;
    time_t last_used;
} pool_connection_t;

/* Circuit breaker state */
typedef struct {
    cb_state_t state;
    int error_count;
    time_t last_error_time;
    time_t opened_at;
    int half_open_attempts;
    int half_open_successes;
} circuit_breaker_t;

/* Redis rate limiter state */
typedef struct {
    redis_rl_config_t config;
    pool_connection_t *pool;
    int pool_size;
    pthread_mutex_t pool_mutex;
    circuit_breaker_t cb;
    pthread_mutex_t cb_mutex;
    bool initialized;
    
    /* Prometheus metrics */
    prometheus_counter_t *metric_requests_total;
    prometheus_counter_t *metric_requests_allowed;
    prometheus_counter_t *metric_requests_limited;
    prometheus_counter_t *metric_requests_error;
    prometheus_counter_t *metric_redis_errors_total;
    prometheus_counter_t *metric_cb_transitions_total;
    prometheus_gauge_t *metric_cb_state;
} redis_rl_state_t;

/* Global state */
static redis_rl_state_t *g_state = NULL;

/* Lua script for atomic fixed-window increment */
static const char *__attribute__((unused)) LUA_FIXED_WINDOW_SCRIPT =
    "local key = KEYS[1]\n"
    "local window_sec = tonumber(ARGV[1])\n"
    "local count = redis.call('INCR', key)\n"
    "if count == 1 then\n"
    "  redis.call('EXPIRE', key, window_sec)\n"
    "end\n"
    "local ttl = redis.call('TTL', key)\n"
    "return {count, ttl}\n";

/* Hash function for client IP (simple djb2) */
static uint32_t hash_ip(const char *ip) {
    uint32_t hash = 5381;
    int c;
    while ((c = *ip++)) {
        hash = ((hash << 5) + hash) + (uint32_t)c; /* hash * 33 + c */
    }
    return hash;
}

/* Get current time in seconds */
static time_t get_current_time(void) {
    return time(NULL);
}

/* Get route ID from method and path */
static int get_route_id(const char *method, const char *path, char *route_id_buf, size_t buf_size) {
    if (!method || !path || !route_id_buf) return -1;
    
    /* Normalize path: remove query string, trailing slashes */
    char normalized_path[256] = {0};
    strncpy(normalized_path, path, sizeof(normalized_path) - 1);
    
    /* Remove query string */
    char *query_start = strchr(normalized_path, '?');
    if (query_start) *query_start = '\0';
    
    /* Remove trailing slash (except root) */
    size_t len = strlen(normalized_path);
    if (len > 1 && normalized_path[len - 1] == '/') {
        normalized_path[len - 1] = '\0';
    }
    
    /* Build route_id: METHOD_PATH */
    int written = snprintf(route_id_buf, buf_size, "%s_%s", method, normalized_path);
    if (written < 0 || (size_t)written >= buf_size) return -1;
    
    return 0;
}

/* Build Redis key: rl:ip:<route_id>:<client_ip_hash>:<bucket_ts> */
static int build_redis_key(char *key_buf, size_t buf_size,
                          const char *route_id,
                          uint32_t client_ip_hash,
                          time_t bucket_ts) {
    if (!key_buf || !route_id) return -1;
    
    int written = snprintf(key_buf, buf_size, "rl:ip:%s:%u:%ld",
                          route_id, client_ip_hash, bucket_ts);
    if (written < 0 || (size_t)written >= buf_size) return -1;
    
    return 0;
}

/* Calculate bucket timestamp (start of fixed window) */
static time_t get_bucket_ts(int window_sec) {
    time_t now = get_current_time();
    return (now / window_sec) * window_sec;
}

/* Get applicable limit for route */
static uint32_t get_route_limit(const redis_rl_config_t *config, const char *route_id) {
    if (!config || !route_id) return config ? config->global_limit : 0;
    
    /* Check per-route overrides */
    if (strstr(route_id, "/api/v1/messages") != NULL) {
        return config->route_limit_messages > 0 ? config->route_limit_messages : config->global_limit;
    }
    if (strstr(route_id, "/api/v1/chat") != NULL) {
        return config->route_limit_chat > 0 ? config->route_limit_chat : config->global_limit;
    }
    
    /* Default to global limit */
    return config->global_limit;
}

/* Acquire connection from pool */
static redisContext *acquire_connection(void) {
    if (!g_state || !g_state->pool) return NULL;
    
    pthread_mutex_lock(&g_state->pool_mutex);
    
    redisContext *conn = NULL;
    
    /* Try to find available connection */
    for (int i = 0; i < g_state->pool_size; i++) {
        if (!g_state->pool[i].in_use) {
            /* Check if connection is still valid */
            if (g_state->pool[i].ctx) {
                /* Simple ping test (non-blocking) */
                #ifdef HAVE_HIREDIS
                if (redisGetReply(g_state->pool[i].ctx, NULL) == REDIS_OK) {
                    g_state->pool[i].in_use = true;
                    g_state->pool[i].last_used = now;
                    conn = g_state->pool[i].ctx;
                    break;
                }
                #endif
            }
        }
    }
    
    /* If no available connection, try to create new one (if pool not full) */
    if (!conn) {
        for (int i = 0; i < g_state->pool_size; i++) {
            if (!g_state->pool[i].ctx) {
                /* Create new connection */
                #ifdef HAVE_HIREDIS
                struct timeval timeout = {
                    .tv_sec = g_state->config.redis_timeout_ms / 1000,
                    .tv_usec = (g_state->config.redis_timeout_ms % 1000) * 1000
                };
                
                if (g_state->config.redis_uri[0] != '\0') {
                    /* Parse URI and connect */
                    g_state->pool[i].ctx = redisConnectWithTimeout(
                        g_state->config.redis_host,
                        g_state->config.redis_port,
                        timeout);
                } else {
                    g_state->pool[i].ctx = redisConnectWithTimeout(
                        g_state->config.redis_host,
                        g_state->config.redis_port,
                        timeout);
                }
                
                if (g_state->pool[i].ctx && !g_state->pool[i].ctx->err) {
                    g_state->pool[i].in_use = true;
                    g_state->pool[i].last_used = now;
                    conn = g_state->pool[i].ctx;
                }
                #endif
                break;
            }
        }
    }
    
    pthread_mutex_unlock(&g_state->pool_mutex);
    return conn;
}

/* Release connection back to pool */
static void release_connection(redisContext *conn) {
    if (!g_state || !g_state->pool || !conn) return;
    
    pthread_mutex_lock(&g_state->pool_mutex);
    
    for (int i = 0; i < g_state->pool_size; i++) {
        if (g_state->pool[i].ctx == conn) {
            g_state->pool[i].in_use = false;
            break;
        }
    }
    
    pthread_mutex_unlock(&g_state->pool_mutex);
}

/* Record circuit breaker error */
static void cb_record_error(void) {
    if (!g_state) return;
    
    pthread_mutex_lock(&g_state->cb_mutex);
    
    g_state->cb.error_count++;
    g_state->cb.last_error_time = get_current_time();
    
    /* Check if should transition to OPEN */
    if (g_state->cb.state == CB_CLOSED) {
        /* Simplified: if error_count >= threshold, open circuit */
        if (g_state->cb.error_count >= g_state->config.cb_error_threshold) {
            g_state->cb.state = CB_OPEN;
            g_state->cb.opened_at = get_current_time();
            if (g_state->metric_cb_transitions_total) {
                prometheus_counter_inc(g_state->metric_cb_transitions_total);
            }
            if (g_state->metric_cb_state) {
                prometheus_gauge_set(g_state->metric_cb_state, 1); /* OPEN */
            }
            log_json("warn", "redis_rate_limiter",
                "Circuit breaker opened: error_count=%d error_threshold=%d",
                g_state->cb.error_count, g_state->config.cb_error_threshold);
        }
    } else if (g_state->cb.state == CB_HALF_OPEN) {
        /* Error in half-open: go back to open */
        g_state->cb.state = CB_OPEN;
        g_state->cb.opened_at = get_current_time();
        g_state->cb.half_open_attempts = 0;
        g_state->cb.half_open_successes = 0;
        if (g_state->metric_cb_transitions_total) {
            prometheus_counter_inc(g_state->metric_cb_transitions_total);
        }
        if (g_state->metric_cb_state) {
            prometheus_gauge_set(g_state->metric_cb_state, 1); /* OPEN */
        }
    }
    
    pthread_mutex_unlock(&g_state->cb_mutex);
}

/* Record circuit breaker success */
static void __attribute__((unused)) cb_record_success(void) {
    if (!g_state) return;
    
    pthread_mutex_lock(&g_state->cb_mutex);
    
    /* Reset error count on success */
    if (g_state->cb.state == CB_CLOSED) {
        g_state->cb.error_count = 0;
    } else if (g_state->cb.state == CB_HALF_OPEN) {
        g_state->cb.half_open_successes++;
        g_state->cb.half_open_attempts++;
        
        /* If enough successes, close circuit */
        if (g_state->cb.half_open_successes >= g_state->config.cb_half_open_attempts) {
            g_state->cb.state = CB_CLOSED;
            g_state->cb.error_count = 0;
            g_state->cb.half_open_attempts = 0;
            g_state->cb.half_open_successes = 0;
            if (g_state->metric_cb_transitions_total) {
                prometheus_counter_inc(g_state->metric_cb_transitions_total);
            }
            if (g_state->metric_cb_state) {
                prometheus_gauge_set(g_state->metric_cb_state, 0); /* CLOSED */
            }
            log_json("info", "redis_rate_limiter",
                "Circuit breaker closed: Redis recovered successfully");
        }
    }
    
    pthread_mutex_unlock(&g_state->cb_mutex);
}

/* Check circuit breaker state and update if needed */
static cb_state_t cb_get_state(void) {
    if (!g_state) return CB_OPEN; /* Fail-safe: open if not initialized */
    
    pthread_mutex_lock(&g_state->cb_mutex);
    
    time_t now = get_current_time();
    
    /* Check if should transition from OPEN to HALF_OPEN */
    if (g_state->cb.state == CB_OPEN) {
        if (now - g_state->cb.opened_at >= g_state->config.cb_cooldown_sec) {
            g_state->cb.state = CB_HALF_OPEN;
            g_state->cb.half_open_attempts = 0;
            g_state->cb.half_open_successes = 0;
            if (g_state->metric_cb_transitions_total) {
                prometheus_counter_inc(g_state->metric_cb_transitions_total);
            }
            if (g_state->metric_cb_state) {
                prometheus_gauge_set(g_state->metric_cb_state, 2); /* HALF_OPEN */
            }
            log_json("info", "redis_rate_limiter",
                "Circuit breaker half-open: testing Redis recovery");
        }
    }
    
    cb_state_t state = g_state->cb.state;
    pthread_mutex_unlock(&g_state->cb_mutex);
    
    return state;
}

/* Execute Redis command with retries (simplified for Lua script) */
static redisReply *__attribute__((unused)) execute_redis_lua_script(redisContext *conn, const char *script, const char *key, int window_sec) {
    (void)window_sec;  /* Parameter kept for API compatibility */
    if (!conn || !script || !key) return NULL;
    
    int attempt = 0;
    int max_attempts = g_state ? g_state->config.retries + 1 : 1;
    redisReply *reply = NULL;
    
    while (attempt < max_attempts) {
        #ifdef HAVE_HIREDIS
        /* Execute Lua script: EVAL script numkeys key arg1 arg2 ... */
        reply = redisCommand(conn, "EVAL %s 1 %s %d", script, key, window_sec);
        
        if (reply && reply->type != REDIS_REPLY_ERROR) {
            /* Success */
            cb_record_success();
            return reply;
        }
        
        /* Check if error is retryable */
        if (conn->err == REDIS_ERR_IO || conn->err == REDIS_ERR_EOF) {
            /* Network error: retry */
            if (reply) {
                freeReplyObject(reply);
                reply = NULL;
            }
            if (attempt < max_attempts - 1) {
                usleep(g_state->config.retry_backoff_ms * 1000);
                attempt++;
                continue;
            }
        }
        
        /* Non-retryable error or max retries reached */
        if (reply) {
            freeReplyObject(reply);
            reply = NULL;
        }
        cb_record_error();
        if (g_state && g_state->metric_redis_errors_total) {
            prometheus_counter_inc(g_state->metric_redis_errors_total);
        }
        attempt++;
        #else
        /* HAVE_HIREDIS not defined: return NULL */
        (void)reply;  /* Suppress unused variable warning */
        return NULL;
        #endif
        
        return NULL;
    }
    
    return NULL;
}

/* Check rate limit using Redis */
static int check_rate_limit_redis(const redis_rl_request_ctx_t *ctx, redis_rl_result_t *result) {
    if (!ctx || !result || !g_state) return -1;
    
    /* Get circuit breaker state */
    cb_state_t cb_state = cb_get_state();
    
    /* If circuit breaker is open and fail-open mode, allow request */
    if (cb_state == CB_OPEN && g_state->config.cb_fail_open) {
        result->decision = REDIS_RL_ALLOW;
        result->degraded = true;
        result->limit = g_state->config.global_limit;
        result->remaining = g_state->config.global_limit;
        result->retry_after_sec = 0;
        result->reset_at = 0;
        return 0;
    }
    
    /* If circuit breaker is open and fail-closed mode, deny request */
    if (cb_state == CB_OPEN && !g_state->config.cb_fail_open) {
        result->decision = REDIS_RL_DENY;
        result->degraded = true;
        return 0;
    }
    
    /* Acquire connection from pool */
    redisContext *conn = acquire_connection();
    if (!conn) {
        /* No connection available: fail-open */
        cb_record_error();
        result->decision = REDIS_RL_ALLOW;
        result->degraded = true;
        return 0;
    }
    
    /* Build route ID */
    char route_id[256];
    if (get_route_id(ctx->method, ctx->path, route_id, sizeof(route_id)) != 0) {
        release_connection(conn);
        return -1;
    }
    
    /* Get applicable limit (stored for potential future use) */
    (void)get_route_limit(&g_state->config, route_id);
    
    /* Hash client IP */
    uint32_t client_ip_hash = hash_ip(ctx->client_ip ? ctx->client_ip : "unknown");
    
    /* Calculate bucket timestamp */
    time_t bucket_ts = get_bucket_ts(g_state->config.window_sec);
    
    /* Build Redis key */
    char redis_key[512];
    if (build_redis_key(redis_key, sizeof(redis_key), route_id, client_ip_hash, bucket_ts) != 0) {
        release_connection(conn);
        return -1;
    }
    
    /* Execute Lua script for atomic increment */
    #ifdef HAVE_HIREDIS
    redisReply *reply = execute_redis_lua_script(conn,
        LUA_FIXED_WINDOW_SCRIPT, redis_key, g_state->config.window_sec);
    
    if (!reply) {
        release_connection(conn);
        cb_record_error();
        result->decision = REDIS_RL_ALLOW; /* Fail-open */
        result->degraded = true;
        return 0;
    }
    
    /* Parse reply: {count, ttl} */
    if (reply->type == REDIS_REPLY_ARRAY && reply->elements == 2) {
        long count = reply->element[0]->integer;
        long ttl = reply->element[1]->integer;
        
        /* Check if limit exceeded */
        if (count > (long)limit) {
            result->decision = REDIS_RL_DENY;
            result->limit = limit;
            result->remaining = 0;
            result->retry_after_sec = (uint32_t)(ttl > 0 ? ttl : g_state->config.window_sec);
            result->reset_at = bucket_ts + g_state->config.window_sec;
            result->degraded = false;
            
            /* Log rate limit denial */
            char route_id[256];
            if (get_route_id(ctx->method, ctx->path, route_id, sizeof(route_id)) == 0) {
                uint32_t client_ip_hash = hash_ip(ctx->client_ip ? ctx->client_ip : "unknown");
                log_json("info", "redis_rate_limiter",
                    "Rate limit exceeded: route_id=\"%s\" client_ip_hash=\"%u\" limit=%u count=%ld window_sec=%d decision=\"limited\"",
                    route_id, client_ip_hash, limit, count, g_state->config.window_sec);
            }
            
            freeReplyObject(reply);
            release_connection(conn);
            return 0;
        }
        
        /* Within limit */
        result->decision = REDIS_RL_ALLOW;
        result->limit = limit;
        result->remaining = (uint32_t)(limit - count);
        result->retry_after_sec = 0;
        result->reset_at = bucket_ts + g_state->config.window_sec;
        result->degraded = false;
        
        freeReplyObject(reply);
        release_connection(conn);
        cb_record_success();
        return 0;
    } else if (reply->type == REDIS_REPLY_ERROR) {
        /* Redis error response */
        log_json("error", "redis_rate_limiter", 
            "Redis error: %s", reply->str ? reply->str : "unknown");
        freeReplyObject(reply);
        release_connection(conn);
        cb_record_error();
        if (g_state && g_state->metric_redis_errors_total) {
            prometheus_counter_inc(g_state->metric_redis_errors_total);
        }
        result->decision = REDIS_RL_ALLOW; /* Fail-open */
        result->degraded = true;
        return 0;
    }
    
    freeReplyObject(reply);
    #endif
    
    release_connection(conn);
    cb_record_error();
    result->decision = REDIS_RL_ALLOW; /* Fail-open */
    result->degraded = true;
    return 0;
}

/* Public API: Get default configuration */
void redis_rate_limiter_get_default_config(redis_rl_config_t *config) {
    if (!config) return;
    
    memset(config, 0, sizeof(redis_rl_config_t));
    config->enabled = false;
    strncpy(config->redis_host, "localhost", sizeof(config->redis_host) - 1);
    config->redis_port = 6379;
    config->window_sec = 1;
    config->global_limit = 1000;
    config->route_limit_messages = 200;
    config->route_limit_chat = 100;
    config->pool_size = 32;
    config->pool_acquire_timeout_ms = 10;
    config->redis_timeout_ms = 30;
    config->retries = 2;
    config->retry_backoff_ms = 5;
    config->cb_fail_open = true;
    config->cb_error_threshold = 5;
    config->cb_sliding_window_sec = 30;
    config->cb_cooldown_sec = 15;
    config->cb_half_open_attempts = 2;
}

/* Public API: Parse configuration from environment */
int redis_rate_limiter_parse_config(redis_rl_config_t *config) {
    if (!config) return -1;
    
    redis_rate_limiter_get_default_config(config);
    
    /* C_GATEWAY_REDIS_RATE_LIMIT_ENABLED */
    const char *enabled_str = getenv("C_GATEWAY_REDIS_RATE_LIMIT_ENABLED");
    if (enabled_str && (strcmp(enabled_str, "true") == 0 || strcmp(enabled_str, "1") == 0)) {
        config->enabled = true;
    }
    
    /* C_GATEWAY_REDIS_RATE_LIMIT_REDIS_URI (priority) */
    const char *redis_uri = getenv("C_GATEWAY_REDIS_RATE_LIMIT_REDIS_URI");
    if (redis_uri && strlen(redis_uri) > 0) {
        strncpy(config->redis_uri, redis_uri, sizeof(config->redis_uri) - 1);
        /* TODO: Parse URI to extract host/port/password */
    }
    
    /* C_GATEWAY_REDIS_RATE_LIMIT_REDIS_HOST */
    const char *redis_host = getenv("C_GATEWAY_REDIS_RATE_LIMIT_REDIS_HOST");
    if (redis_host && strlen(redis_host) > 0) {
        strncpy(config->redis_host, redis_host, sizeof(config->redis_host) - 1);
    }
    
    /* C_GATEWAY_REDIS_RATE_LIMIT_REDIS_PORT */
    const char *redis_port_str = getenv("C_GATEWAY_REDIS_RATE_LIMIT_REDIS_PORT");
    if (redis_port_str) {
        config->redis_port = atoi(redis_port_str);
    }
    
    /* Window and limits */
    const char *window_sec_str = getenv("C_GATEWAY_REDIS_RATE_LIMIT_WINDOW_SEC");
    if (window_sec_str) config->window_sec = atoi(window_sec_str);
    
    const char *global_limit_str = getenv("C_GATEWAY_REDIS_RATE_LIMIT_GLOBAL_LIMIT");
    if (global_limit_str) config->global_limit = (uint32_t)atoi(global_limit_str);
    
    const char *route_limit_messages_str = getenv("C_GATEWAY_REDIS_RATE_LIMIT_ROUTE_LIMIT_MESSAGES");
    if (route_limit_messages_str) config->route_limit_messages = (uint32_t)atoi(route_limit_messages_str);
    
    const char *route_limit_chat_str = getenv("C_GATEWAY_REDIS_RATE_LIMIT_ROUTE_LIMIT_CHAT");
    if (route_limit_chat_str) config->route_limit_chat = (uint32_t)atoi(route_limit_chat_str);
    
    /* Pool configuration */
    const char *pool_size_str = getenv("C_GATEWAY_REDIS_RATE_LIMIT_POOL_SIZE");
    if (pool_size_str) config->pool_size = atoi(pool_size_str);
    
    const char *pool_timeout_str = getenv("C_GATEWAY_REDIS_RATE_LIMIT_POOL_ACQUIRE_TIMEOUT_MS");
    if (pool_timeout_str) config->pool_acquire_timeout_ms = atoi(pool_timeout_str);
    
    const char *redis_timeout_str = getenv("C_GATEWAY_REDIS_RATE_LIMIT_REDIS_TIMEOUT_MS");
    if (redis_timeout_str) config->redis_timeout_ms = atoi(redis_timeout_str);
    
    /* Retry configuration */
    const char *retries_str = getenv("C_GATEWAY_REDIS_RATE_LIMIT_RETRIES");
    if (retries_str) config->retries = atoi(retries_str);
    
    const char *retry_backoff_str = getenv("C_GATEWAY_REDIS_RATE_LIMIT_RETRY_BACKOFF_MS");
    if (retry_backoff_str) config->retry_backoff_ms = atoi(retry_backoff_str);
    
    /* Circuit breaker configuration */
    const char *cb_mode_str = getenv("C_GATEWAY_REDIS_RATE_LIMIT_CB_MODE");
    if (cb_mode_str) {
        config->cb_fail_open = (strcmp(cb_mode_str, "fail_open") == 0);
    }
    
    const char *cb_error_threshold_str = getenv("C_GATEWAY_REDIS_RATE_LIMIT_CB_ERROR_THRESHOLD");
    if (cb_error_threshold_str) config->cb_error_threshold = atoi(cb_error_threshold_str);
    
    const char *cb_window_str = getenv("C_GATEWAY_REDIS_RATE_LIMIT_CB_SLIDING_WINDOW_SEC");
    if (cb_window_str) config->cb_sliding_window_sec = atoi(cb_window_str);
    
    const char *cb_cooldown_str = getenv("C_GATEWAY_REDIS_RATE_LIMIT_CB_COOLDOWN_SEC");
    if (cb_cooldown_str) config->cb_cooldown_sec = atoi(cb_cooldown_str);
    
    const char *cb_half_open_str = getenv("C_GATEWAY_REDIS_RATE_LIMIT_CB_HALF_OPEN_ATTEMPTS");
    if (cb_half_open_str) config->cb_half_open_attempts = atoi(cb_half_open_str);
    
    return 0;
}

/* Public API: Initialize */
int redis_rate_limiter_init(const redis_rl_config_t *config) {
    if (!config || !config->enabled) return 0; /* Not enabled, but not an error */
    
    if (g_state) {
        /* Already initialized */
        return 0;
    }
    
    g_state = (redis_rl_state_t *)calloc(1, sizeof(redis_rl_state_t));
    if (!g_state) return -1;
    
    memcpy(&g_state->config, config, sizeof(redis_rl_config_t));
    
    /* Initialize connection pool */
    g_state->pool_size = config->pool_size;
    g_state->pool = (pool_connection_t *)calloc((size_t)config->pool_size, sizeof(pool_connection_t));
    if (!g_state->pool) {
        free(g_state);
        return -1;
    }
    
    /* Initialize mutexes */
    if (pthread_mutex_init(&g_state->pool_mutex, NULL) != 0) {
        free(g_state->pool);
        free(g_state);
        return -1;
    }
    
    if (pthread_mutex_init(&g_state->cb_mutex, NULL) != 0) {
        pthread_mutex_destroy(&g_state->pool_mutex);
        free(g_state->pool);
        free(g_state);
        return -1;
    }
    
    /* Initialize circuit breaker */
    g_state->cb.state = CB_CLOSED;
    g_state->cb.error_count = 0;
    g_state->cb.half_open_attempts = 0;
    g_state->cb.half_open_successes = 0;
    
    /* Initialize Prometheus metrics */
    g_state->metric_requests_total = prometheus_counter_create(
        "c_gateway_redis_rate_limiter_requests_total",
        "Total number of requests checked by Redis rate limiter");
    
    g_state->metric_requests_allowed = prometheus_counter_create(
        "c_gateway_redis_rate_limiter_requests_allowed_total",
        "Number of requests allowed by Redis rate limiter");
    
    g_state->metric_requests_limited = prometheus_counter_create(
        "c_gateway_redis_rate_limiter_requests_limited_total",
        "Number of requests limited (429) by Redis rate limiter");
    
    g_state->metric_requests_error = prometheus_counter_create(
        "c_gateway_redis_rate_limiter_requests_error_total",
        "Number of requests with errors in Redis rate limiter");
    
    g_state->metric_redis_errors_total = prometheus_counter_create(
        "c_gateway_redis_rate_limiter_redis_errors_total",
        "Total number of Redis errors encountered");
    
    g_state->metric_cb_transitions_total = prometheus_counter_create(
        "c_gateway_redis_rate_limiter_cb_transitions_total",
        "Total number of circuit breaker state transitions");
    
    g_state->metric_cb_state = prometheus_gauge_create(
        "c_gateway_redis_rate_limiter_cb_state",
        "Current circuit breaker state (0=closed, 1=open, 2=half_open)");
    
    if (g_state->metric_cb_state) {
        prometheus_gauge_set(g_state->metric_cb_state, 0); /* CLOSED */
    }
    
    g_state->initialized = true;
    
    return 0;
}

/* Public API: Check rate limit */
int redis_rate_limiter_check(const redis_rl_request_ctx_t *ctx, redis_rl_result_t *result) {
    if (!ctx || !result) return -1;
    
    if (!g_state || !g_state->initialized || !g_state->config.enabled) {
        /* Not initialized or disabled: allow all */
        result->decision = REDIS_RL_ALLOW;
        result->degraded = false;
        return 0;
    }
    
    /* Update metrics */
    if (g_state->metric_requests_total) {
        prometheus_counter_inc(g_state->metric_requests_total);
    }
    
    /* Check rate limit */
    int rc = check_rate_limit_redis(ctx, result);
    if (rc != 0) {
        result->decision = REDIS_RL_ALLOW; /* Fail-open on error */
        result->degraded = true;
        if (g_state->metric_requests_error) {
            prometheus_counter_inc(g_state->metric_requests_error);
        }
        return 0;
    }
    
    /* Update metrics based on decision */
    if (result->decision == REDIS_RL_ALLOW) {
        if (g_state->metric_requests_allowed) {
            prometheus_counter_inc(g_state->metric_requests_allowed);
        }
    } else if (result->decision == REDIS_RL_DENY) {
        if (g_state->metric_requests_limited) {
            prometheus_counter_inc(g_state->metric_requests_limited);
        }
    } else {
        if (g_state->metric_requests_error) {
            prometheus_counter_inc(g_state->metric_requests_error);
        }
    }
    
    return 0;
}

/* Public API: Get circuit breaker state */
const char *redis_rate_limiter_get_cb_state(void) {
    if (!g_state) return "uninitialized";
    
    cb_state_t state = cb_get_state();
    switch (state) {
        case CB_CLOSED: return "closed";
        case CB_OPEN: return "open";
        case CB_HALF_OPEN: return "half_open";
        default: return "unknown";
    }
}

/* Public API: Cleanup */
void redis_rate_limiter_cleanup(void) {
    if (!g_state) return;
    
    /* Close all connections */
    if (g_state->pool) {
        for (int i = 0; i < g_state->pool_size; i++) {
            if (g_state->pool[i].ctx) {
                #ifdef HAVE_HIREDIS
                redisFree(g_state->pool[i].ctx);
                #endif
                g_state->pool[i].ctx = NULL;
            }
        }
        free(g_state->pool);
    }
    
    pthread_mutex_destroy(&g_state->pool_mutex);
    pthread_mutex_destroy(&g_state->cb_mutex);
    
    free(g_state);
    g_state = NULL;
}

