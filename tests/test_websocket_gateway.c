/**
 * test_websocket_gateway.c - WebSocket gateway tests
 */

#include "websocket_gateway.h"
#include <stdio.h>
#include <assert.h>
#include <unistd.h>

static void test_init_destroy(void) {
    printf("Test: WebSocket gateway init/destroy... ");
    
    websocket_gateway_config_t config = {
        .port = 8081,
        .ipc_socket = "/tmp/test_ipc.sock",
        .max_connections = 100
    };
    
    websocket_gateway_t *gateway = websocket_gateway_init(&config);
    assert(gateway != NULL);
    
    websocket_gateway_destroy(gateway);
    printf("OK\n");
}

static void test_start_stop(void) {
    printf("Test: WebSocket gateway start/stop... ");
    
    websocket_gateway_config_t config = {
        .port = 8082,
        .ipc_socket = "/tmp/test_ipc.sock",
        .max_connections = 50
    };
    
    websocket_gateway_t *gateway = websocket_gateway_init(&config);
    
    int rc = websocket_gateway_start(gateway);
    assert(rc == 0);
    
    sleep(1);  /* Let server start */
    
    websocket_gateway_stop(gateway);
    websocket_gateway_destroy(gateway);
    
    printf("OK\n");
}

static void test_stats(void) {
    printf("Test: WebSocket gateway stats... ");
    
    websocket_gateway_config_t config = {
        .port = 8083,
        .ipc_socket = "/tmp/test_ipc.sock",
        .max_connections = 10
    };
    
    websocket_gateway_t *gateway = websocket_gateway_init(&config);
    
    int active = -1;
    uint64_t messages = 999, errors = 999;
    
    websocket_gateway_stats(gateway, &active, &messages, &errors);
    
    assert(active == 0);
    assert(messages == 0);
    assert(errors == 0);
    
    websocket_gateway_destroy(gateway);
    printf("OK\n");
}

int main(void) {
    printf("=== WebSocket Gateway Tests ===\n");
    
    test_init_destroy();
    test_start_stop();
    test_stats();
    
    printf("\nAll tests passed!\n");
    printf("NOTE: This tests stub implementation only\n");
    return 0;
}
