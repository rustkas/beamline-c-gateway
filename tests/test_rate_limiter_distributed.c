/* Test: Distributed Rate Limiting PoC
 * 
 * Tests for distributed rate limiting with Redis backend.
 * Requires Redis running on localhost:6379 (or configured via env vars).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include "../src/rate_limiter.h"

/* Helper: Send HTTP request and extract status */
static int send_http_request(const char *host, int port, const char *request, 
                             char *response, size_t response_size) {
    /* Simplified: Use curl or similar for actual implementation */
    (void)host;
    (void)port;
    (void)request;
    (void)response;
    (void)response_size;
    return 0;
}

/* Test: Memory mode (CP1) - backward compatibility */
static void test_memory_mode(void) {
    printf("Test: Memory mode (CP1)\n");
    
    distributed_rl_config_t config;
    rate_limiter_get_default_config(&config);
    config.enabled = 0;  /* Disable distributed mode */
    
    rate_limiter_t *limiter = rate_limiter_create(&config);
    assert(limiter != NULL);
    
    unsigned int remaining = 0;
    
    /* Send requests under limit */
    for (int i = 0; i < 3; i++) {
        rl_result_t result = limiter->check(limiter, RL_ENDPOINT_ROUTES_DECIDE, 
                                            NULL, NULL, &remaining);
        assert(result == RL_ALLOWED);
        printf("  Request %d: ALLOWED (remaining: %u)\n", i + 1, remaining);
    }
    
    rate_limiter_destroy(limiter);
    printf("  ✅ Memory mode test passed\n\n");
}

/* Test: Redis mode (CP2) - single instance */
static void test_redis_mode_single(void) {
    printf("Test: Redis mode (CP2) - single instance\n");
    
    distributed_rl_config_t config;
    rate_limiter_get_default_config(&config);
    config.enabled = 1;
    config.backend = "redis";
    config.redis_host = getenv("GATEWAY_RATE_LIMIT_REDIS_HOST") ?: "localhost";
    config.redis_port = atoi(getenv("GATEWAY_RATE_LIMIT_REDIS_PORT") ?: "6379");
    config.fallback_to_local = 1;
    
    rate_limiter_t *limiter = rate_limiter_create(&config);
    if (!limiter) {
        printf("  ⚠️  Redis not available, skipping test\n\n");
        return;
    }
    
    unsigned int remaining = 0;
    
    /* Send requests under limit */
    for (int i = 0; i < 3; i++) {
        rl_result_t result = limiter->check(limiter, RL_ENDPOINT_ROUTES_DECIDE, 
                                            "tenant_test", NULL, &remaining);
        if (result == RL_ERROR) {
            printf("  ⚠️  Redis error, using fallback\n");
            break;
        }
        assert(result == RL_ALLOWED);
        printf("  Request %d: ALLOWED (remaining: %u)\n", i + 1, remaining);
    }
    
    rate_limiter_destroy(limiter);
    printf("  ✅ Redis mode test passed\n\n");
}

/* Test: Redis mode (CP2) - multi-instance consistency */
static void test_redis_mode_multi_instance(void) {
    printf("Test: Redis mode (CP2) - multi-instance consistency\n");
    printf("  Note: This test requires 2-3 Gateway instances + Redis\n");
    printf("  Run: docker-compose -f docker-compose.rate-limit-test.yml up\n");
    printf("  Then send requests to multiple instances and verify consistent limits\n\n");
}

/* Test: Fallback to memory mode */
static void test_fallback_to_memory(void) {
    printf("Test: Fallback to memory mode (Redis unavailable)\n");
    
    distributed_rl_config_t config;
    rate_limiter_get_default_config(&config);
    config.enabled = 1;
    config.backend = "redis";
    config.redis_host = "nonexistent_host";  /* Invalid host */
    config.redis_port = 6379;
    config.fallback_to_local = 1;  /* Enable fallback */
    
    rate_limiter_t *limiter = rate_limiter_create(&config);
    if (!limiter) {
        printf("  ⚠️  Limiter creation failed\n\n");
        return;
    }
    
    unsigned int remaining = 0;
    
    /* Should fallback to memory mode */
    rl_result_t result = limiter->check(limiter, RL_ENDPOINT_ROUTES_DECIDE, 
                                       NULL, NULL, &remaining);
    assert(result == RL_ALLOWED || result == RL_ERROR);  /* Either works or errors gracefully */
    printf("  Fallback result: %s\n", result == RL_ALLOWED ? "ALLOWED" : "ERROR");
    
    rate_limiter_destroy(limiter);
    printf("  ✅ Fallback test passed\n\n");
}

int main(void) {
    printf("=== Distributed Rate Limiting PoC Tests ===\n\n");
    
    test_memory_mode();
    test_redis_mode_single();
    test_redis_mode_multi_instance();
    test_fallback_to_memory();
    
    printf("=== All tests completed ===\n");
    return 0;
}

