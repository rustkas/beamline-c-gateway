/**
 * test_circuit_breaker.c - Circuit breaker tests
 */

#include "circuit_breaker.h"
#include <stdio.h>
#include <assert.h>
#include <unistd.h>

static void test_create_destroy(void) {
    printf("Test: create/destroy... ");
    
    cb_config_t config = {
        .failure_threshold = 5,
        .success_threshold = 3,
        .timeout_ms = 1000,
        .half_open_max_calls = 2
    };
    
    circuit_breaker_t *cb = circuit_breaker_create(&config);
    assert(cb != NULL);
    assert(circuit_breaker_get_state(cb) == CB_STATE_CLOSED);
    
    circuit_breaker_destroy(cb);
    printf("OK\n");
}

static void test_failure_opens_circuit(void) {
    printf("Test: failures open circuit... ");
    
    cb_config_t config = {
        .failure_threshold = 3,
        .success_threshold = 2,
        .timeout_ms = 1000,
        .half_open_max_calls = 2
    };
    
    circuit_breaker_t *cb = circuit_breaker_create(&config);
    
    /* Record failures */
    for (int i = 0; i < 3; i++) {
        assert(circuit_breaker_allow_request(cb) == 1);
        circuit_breaker_on_failure(cb);
    }
    
    /* Circuit should be open */
    assert(circuit_breaker_get_state(cb) == CB_STATE_OPEN);
    
    /* Requests should be rejected */ assert(circuit_breaker_allow_request(cb) == 0);
    
    circuit_breaker_destroy(cb);
    printf("OK\n");
}

static void test_timeout_to_half_open(void) {
    printf("Test: timeout transitions to half-open... ");
    
    cb_config_t config = {
        .failure_threshold = 2,
        .success_threshold = 2,
        .timeout_ms = 500,  /* 500ms timeout */
        .half_open_max_calls = 3
    };
    
    circuit_breaker_t *cb = circuit_breaker_create(&config);
    
    /* Open circuit */
    circuit_breaker_allow_request(cb);
    circuit_breaker_on_failure(cb);
    circuit_breaker_allow_request(cb);
    circuit_breaker_on_failure(cb);
    
    assert(circuit_breaker_get_state(cb) == CB_STATE_OPEN);
    
    /* Wait for timeout */
    usleep(600000);  /* 600ms */
    
    /* Next request should transition to half-open */
    assert(circuit_breaker_allow_request(cb) == 1);
    assert(circuit_breaker_get_state(cb) == CB_STATE_HALF_OPEN);
    
    circuit_breaker_destroy(cb);
    printf("OK\n");
}

static void test_half_open_recovery(void) {
    printf("Test: half-open recovery to closed... ");
    
    cb_config_t config = {
        .failure_threshold = 2,
        .success_threshold = 2,
        .timeout_ms = 100,
        .half_open_max_calls = 5
    };
    
    circuit_breaker_t *cb = circuit_breaker_create(&config);
    
    /* Open circuit */
    circuit_breaker_allow_request(cb);
    circuit_breaker_on_failure(cb);
    circuit_breaker_allow_request(cb);
    circuit_breaker_on_failure(cb);
    
    /* Wait for half-open */
    usleep(150000);
    
    /* Successes in half-open */
    circuit_breaker_allow_request(cb);
    circuit_breaker_on_success(cb);
    circuit_breaker_allow_request(cb);
    circuit_breaker_on_success(cb);
    
    /* Should transition to closed */
    assert(circuit_breaker_get_state(cb) == CB_STATE_CLOSED);
    
    circuit_breaker_destroy(cb);
    printf("OK\n");
}

static void test_stats(void) {
    printf("Test: statistics tracking... ");
    
    cb_config_t config = {
        .failure_threshold = 10,
        .success_threshold = 2,
        .timeout_ms = 1000,
        .half_open_max_calls = 2
    };
    
    circuit_breaker_t *cb = circuit_breaker_create(&config);
    
    /* Some successes */
    for (int i = 0; i < 5; i++) {
        circuit_breaker_allow_request(cb);
        circuit_breaker_on_success(cb);
    }
    
    /* Some failures */
    for (int i = 0; i < 3; i++) {
        circuit_breaker_allow_request(cb);
        circuit_breaker_on_failure(cb);
    }
    
    uint64_t success, failure, reject;
    circuit_breaker_get_stats(cb, &success, &failure, &reject);
    
    assert(success == 5);
    assert(failure == 3);
    
    circuit_breaker_destroy(cb);
    printf("OK\n");
}

int main(void) {
    printf("=== Circuit Breaker Tests ===\n");
    
    test_create_destroy();
    test_failure_opens_circuit();
    test_timeout_to_half_open();
    test_half_open_recovery();
    test_stats();
    
    printf("\nAll tests passed!\n");
    return 0;
}
