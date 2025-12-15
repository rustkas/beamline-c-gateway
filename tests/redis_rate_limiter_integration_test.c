/* Integration tests for Redis Rate Limiter PoC
 * 
 * These tests require a running Redis instance on localhost:6379
 * Run with: C_GATEWAY_REDIS_RATE_LIMIT_ENABLED=true ./redis_rate_limiter_integration_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include "../include/redis_rate_limiter.h"

/* Test basic rate limiting */
static void test_basic_rate_limiting(void) {
    printf("Test: Basic rate limiting\n");
    
    redis_rl_config_t config;
    redis_rate_limiter_get_default_config(&config);
    config.enabled = true;
    config.global_limit = 5;  /* Very low limit for testing */
    config.window_sec = 1;
    config.redis_host = "localhost";
    config.redis_port = 6379;
    config.cb_fail_open = true;
    
    int rc = redis_rate_limiter_init(&config);
    if (rc != 0) {
        printf("  ⚠ Skipped: Redis not available or initialization failed\n");
        return;
    }
    
    redis_rl_request_ctx_t ctx = {
        .client_ip = "192.168.1.100",
        .method = "POST",
        .path = "/api/v1/messages",
        .tenant_id = NULL
    };
    
    redis_rl_result_t result;
    int allowed = 0;
    int denied = 0;
    
    /* Make 10 requests - should allow 5, deny 5 */
    for (int i = 0; i < 10; i++) {
        rc = redis_rate_limiter_check(&ctx, &result);
        assert(rc == 0);
        
        if (result.decision == REDIS_RL_ALLOW) {
            allowed++;
        } else if (result.decision == REDIS_RL_DENY) {
            denied++;
            assert(result.limit == 5);
            assert(result.remaining == 0);
        }
        
        usleep(10000); /* 10ms between requests */
    }
    
    printf("  ✓ Allowed: %d, Denied: %d (expected: 5 allowed, 5 denied)\n", allowed, denied);
    assert(allowed >= 4 && allowed <= 6); /* Allow some variance */
    assert(denied >= 4 && denied <= 6);
    
    redis_rate_limiter_cleanup();
}

/* Test circuit breaker fail-open */
static void test_circuit_breaker_fail_open(void) {
    printf("Test: Circuit breaker fail-open\n");
    
    redis_rl_config_t config;
    redis_rate_limiter_get_default_config(&config);
    config.enabled = true;
    config.redis_host = "localhost";
    config.redis_port = 9999; /* Invalid port to trigger errors */
    config.cb_fail_open = true;
    config.cb_error_threshold = 2; /* Low threshold for testing */
    config.cb_sliding_window_sec = 5;
    config.cb_cooldown_sec = 2;
    
    int rc = redis_rate_limiter_init(&config);
    if (rc != 0) {
        printf("  ⚠ Skipped: Initialization failed\n");
        return;
    }
    
    redis_rl_request_ctx_t ctx = {
        .client_ip = "192.168.1.200",
        .method = "POST",
        .path = "/api/v1/messages",
        .tenant_id = NULL
    };
    
    redis_rl_result_t result;
    
    /* Make requests that will fail */
    for (int i = 0; i < 5; i++) {
        rc = redis_rate_limiter_check(&ctx, &result);
        assert(rc == 0);
        
        /* Should allow (fail-open) even with errors */
        assert(result.decision == REDIS_RL_ALLOW);
        assert(result.degraded == true);
        
        usleep(100000); /* 100ms between requests */
    }
    
    /* Check circuit breaker state */
    const char *cb_state = redis_rate_limiter_get_cb_state();
    printf("  ✓ Circuit breaker state: %s (should be open or half_open)\n", cb_state);
    assert(strcmp(cb_state, "open") == 0 || strcmp(cb_state, "half_open") == 0);
    
    redis_rate_limiter_cleanup();
}

/* Test per-route limits */
static void test_per_route_limits(void) {
    printf("Test: Per-route limits\n");
    
    redis_rl_config_t config;
    redis_rate_limiter_get_default_config(&config);
    config.enabled = true;
    config.global_limit = 100;
    config.route_limit_messages = 10; /* Lower limit for /api/v1/messages */
    config.window_sec = 1;
    config.redis_host = "localhost";
    config.redis_port = 6379;
    
    int rc = redis_rate_limiter_init(&config);
    if (rc != 0) {
        printf("  ⚠ Skipped: Redis not available\n");
        return;
    }
    
    redis_rl_request_ctx_t ctx = {
        .client_ip = "192.168.1.300",
        .method = "POST",
        .path = "/api/v1/messages",
        .tenant_id = NULL
    };
    
    redis_rl_result_t result;
    int denied = 0;
    
    /* Make 15 requests - should deny after 10 */
    for (int i = 0; i < 15; i++) {
        rc = redis_rate_limiter_check(&ctx, &result);
        assert(rc == 0);
        
        if (result.decision == REDIS_RL_DENY) {
            denied++;
            assert(result.limit == 10); /* Should use route limit, not global */
        }
        
        usleep(10000);
    }
    
    printf("  ✓ Denied: %d requests (expected: ~5 denied)\n", denied);
    assert(denied >= 4);
    
    redis_rate_limiter_cleanup();
}

int main(void) {
    printf("Running Redis Rate Limiter Integration Tests\n");
    printf("===========================================\n");
    printf("Note: These tests require Redis on localhost:6379\n\n");
    
    test_basic_rate_limiting();
    test_circuit_breaker_fail_open();
    test_per_route_limits();
    
    printf("\n===========================================\n");
    printf("Integration tests completed!\n");
    return 0;
}

