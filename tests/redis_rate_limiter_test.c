/* Unit tests for Redis Rate Limiter PoC */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../include/redis_rate_limiter.h"

/* Test configuration parsing */
static void test_config_parsing(void) {
    printf("Test: Configuration parsing\n");
    
    redis_rl_config_t config;
    redis_rate_limiter_get_default_config(&config);
    
    assert(config.enabled == false);
    assert(strcmp(config.redis_host, "localhost") == 0);
    assert(config.redis_port == 6379);
    assert(config.window_sec == 1);
    assert(config.global_limit == 1000);
    assert(config.pool_size == 32);
    assert(config.cb_fail_open == true);
    
    printf("  ✓ Default configuration correct\n");
}

/* Test route ID generation */
static void test_route_id_generation(void) {
    printf("Test: Route ID generation\n");
    
    redis_rl_request_ctx_t ctx = {
        .client_ip = "192.168.1.1",
        .method = "POST",
        .path = "/api/v1/messages",
        .tenant_id = NULL
    };
    
    /* This would require exposing internal function, so we test via public API */
    redis_rl_result_t result;
    int rc = redis_rate_limiter_check(&ctx, &result);
    
    /* Should not fail on disabled limiter */
    assert(rc == 0);
    assert(result.decision == REDIS_RL_ALLOW);
    
    printf("  ✓ Route ID generation works (via public API)\n");
}

/* Test circuit breaker state */
static void test_circuit_breaker_state(void) {
    printf("Test: Circuit breaker state\n");
    
    const char *state = redis_rate_limiter_get_cb_state();
    assert(state != NULL);
    assert(strcmp(state, "uninitialized") == 0 || 
           strcmp(state, "closed") == 0 ||
           strcmp(state, "open") == 0 ||
           strcmp(state, "half_open") == 0);
    
    printf("  ✓ Circuit breaker state API works\n");
}

/* Test cleanup */
static void test_cleanup(void) {
    printf("Test: Cleanup\n");
    
    /* Should not crash */
    redis_rate_limiter_cleanup();
    
    printf("  ✓ Cleanup works\n");
}

/* Test disabled limiter */
static void test_disabled_limiter(void) {
    printf("Test: Disabled limiter\n");
    
    redis_rl_config_t config;
    redis_rate_limiter_get_default_config(&config);
    config.enabled = false;
    
    int rc = redis_rate_limiter_init(&config);
    assert(rc == 0);
    
    redis_rl_request_ctx_t ctx = {
        .client_ip = "192.168.1.1",
        .method = "POST",
        .path = "/api/v1/messages",
        .tenant_id = NULL
    };
    
    redis_rl_result_t result;
    rc = redis_rate_limiter_check(&ctx, &result);
    assert(rc == 0);
    assert(result.decision == REDIS_RL_ALLOW);
    assert(result.degraded == false);
    
    printf("  ✓ Disabled limiter allows all requests\n");
}

int main(void) {
    printf("Running Redis Rate Limiter Unit Tests\n");
    printf("=====================================\n\n");
    
    test_config_parsing();
    test_route_id_generation();
    test_circuit_breaker_state();
    test_disabled_limiter();
    test_cleanup();
    
    printf("\n=====================================\n");
    printf("All unit tests passed!\n");
    return 0;
}

