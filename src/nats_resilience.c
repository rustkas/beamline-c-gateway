/**
 * nats_resilience.c - NATS resilience implementation
 */

#include "nats_resilience.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Defaults */
#define DEFAULT_MAX_INFLIGHT        100
#define DEFAULT_MIN_BACKOFF_MS      100
#define DEFAULT_MAX_BACKOFF_MS      30000
#define DEFAULT_DEGRADED_THRESHOLD  3
#define DEFAULT_RECONNECT_ATTEMPTS  0   /* infinite */

/**
 * Resilience state
 */
struct nats_resilience_t {
    nats_resilience_config_t config;
    
    /* State tracking */
    nats_connection_state_t state;
    int inflight_count;
    size_t consecutive_errors;
    size_t total_errors;
    int reconnect_attempts;
    
    /* Backoff */
    int current_backoff_ms;
    time_t last_error_time;
};

nats_resilience_t* nats_resilience_init(const nats_resilience_config_t *config) {
    nats_resilience_t *res = (nats_resilience_t*)calloc(1, sizeof(nats_resilience_t));
    if (!res) {
        return NULL;
    }
    
    /* Apply config or defaults */
    if (config) {
        res->config = *config;
    } else {
        res->config.max_inflight = DEFAULT_MAX_INFLIGHT;
        res->config.min_backoff_ms = DEFAULT_MIN_BACKOFF_MS;
        res->config.max_backoff_ms = DEFAULT_MAX_BACKOFF_MS;
        res->config.degraded_threshold = DEFAULT_DEGRADED_THRESHOLD;
        res->config.reconnect_attempts = DEFAULT_RECONNECT_ATTEMPTS;
    }
    
    /* Validate */
    if (res->config.max_inflight <= 0) res->config.max_inflight = DEFAULT_MAX_INFLIGHT;
    if (res->config.min_backoff_ms <= 0) res->config.min_backoff_ms = DEFAULT_MIN_BACKOFF_MS;
    if (res->config.max_backoff_ms <= 0) res->config.max_backoff_ms = DEFAULT_MAX_BACKOFF_MS;
    
    res->state = NATS_STATE_DISCONNECTED;
    res->current_backoff_ms = res->config.min_backoff_ms;
    
    return res;
}

nats_connection_state_t nats_resilience_get_state(const nats_resilience_t *res) {
    return res ? res->state : NATS_STATE_DISCONNECTED;
}

int nats_resilience_can_accept(nats_resilience_t *res) {
    if (!res) return 0;
    
    /* Check inflight limit */
    if (res->inflight_count >= res->config.max_inflight) {
        return 0;  /* Overloaded */
    }
    
    /* Check if degraded/disconnected */
    if (res->state == NATS_STATE_DISCONNECTED || 
        res->state == NATS_STATE_RECONNECTING) {
        return 0;  /* Not ready */
    }
    
    return 1;  /* OK */
}

void nats_resilience_request_start(nats_resilience_t *res) {
    if (!res) return;
    res->inflight_count++;
}

void nats_resilience_request_complete(nats_resilience_t *res, int success) {
    if (!res) return;
    
    if (res->inflight_count > 0) {
        res->inflight_count--;
    }
    
    if (success) {
        /* Reset error tracking */
        res->consecutive_errors = 0;
        
        /* Return to connected if was degraded */
        if (res->state == NATS_STATE_DEGRADED) {
            res->state = NATS_STATE_CONNECTED;
            res->current_backoff_ms = res->config.min_backoff_ms;
            printf("[nats_resilience] Recovered to CONNECTED state\n");
        }
    } else {
        /* Error occurred */
        res->consecutive_errors++;
        res->total_errors++;
        res->last_error_time = time(NULL);
        
        /* Check if should enter degraded state */
        if (res->consecutive_errors >= (size_t)res->config.degraded_threshold &&
            res->state == NATS_STATE_CONNECTED) {
            res->state = NATS_STATE_DEGRADED;
            printf("[nats_resilience] Entered DEGRADED state (errors=%zu)\n", 
                   res->consecutive_errors);
        }
        
        /* Increase backoff (exponential) */
        res->current_backoff_ms *= 2;
        if (res->current_backoff_ms > res->config.max_backoff_ms) {
            res->current_backoff_ms = res->config.max_backoff_ms;
        }
    }
}

void nats_resilience_mark_connected(nats_resilience_t *res) {
    if (!res) return;
    
    res->state = NATS_STATE_CONNECTED;
    res->consecutive_errors = 0;
    res->current_backoff_ms = res->config.min_backoff_ms;
    res->reconnect_attempts = 0;
    
    printf("[nats_resilience] Connection established\n");
}

int nats_resilience_get_backoff_ms(const nats_resilience_t *res) {
    return res ? res->current_backoff_ms : DEFAULT_MIN_BACKOFF_MS;
}

void nats_resilience_get_stats(const nats_resilience_t *res,
                                int *inflight,
                                size_t *total_errors,
                                int *reconnect_count) {
    if (!res) return;
    
    if (inflight) *inflight = res->inflight_count;
    if (total_errors) *total_errors = res->total_errors;
    if (reconnect_count) *reconnect_count = res->reconnect_attempts;
}

void nats_resilience_destroy(nats_resilience_t *res) {
    if (!res) return;
    
    printf("[nats_resilience] Destroyed (total_errors=%zu, state=%d)\n",
           res->total_errors, res->state);
    
    free(res);
}
