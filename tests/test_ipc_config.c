/**
 * test_ipc_config.c - Unit tests for IPC configuration
 */

#include "ipc_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static void test_defaults(void) {
    printf("Test: defaults... ");
    
    /* Clear env */
    unsetenv("CGW_IPC_ENABLE");
    unsetenv("CGW_IPC_SOCKET_PATH");
    
    ipc_config_t config;
    ipc_config_error_t err = ipc_config_load(&config);
    
    assert(err == IPC_CONFIG_OK);
    assert(config.ipc_enable == 1);
    assert(strcmp(config.socket_path, "/tmp/beamline-gateway.sock") == 0);
    assert(config.max_connections == 64);
    assert(config.nats_enable == 0);
    assert(config.timeout_ms == 30000);
    
    /* Validation should pass */
    err = ipc_config_validate(&config);
    assert(err == IPC_CONFIG_OK);
    
    printf("OK\n");
}

static void test_override_env(void) {
    printf("Test: override env vars... ");
    
    setenv("CGW_IPC_ENABLE", "0", 1);
    setenv("CGW_IPC_SOCKET_PATH", "/tmp/custom.sock", 1);
    setenv("CGW_IPC_MAX_CONNECTIONS", "128", 1);
    setenv("CGW_IPC_NATS_ENABLE", "1", 1);
    setenv("CGW_IPC_TIMEOUT_MS", "5000", 1);
    
    ipc_config_t config;
    ipc_config_error_t err = ipc_config_load(&config);
    
    assert(err == IPC_CONFIG_OK);
    assert(config.ipc_enable == 0);
    assert(strcmp(config.socket_path, "/tmp/custom.sock") == 0);
    assert(config.max_connections == 128);
    assert(config.nats_enable == 1);
    assert(config.timeout_ms == 5000);
    
    err = ipc_config_validate(&config);
    assert(err == IPC_CONFIG_OK);
    
    printf("OK\n");
}

static void test_invalid_bool(void) {
    printf("Test: invalid boolean... ");
    
    setenv("CGW_IPC_ENABLE", "invalid", 1);
    
    ipc_config_t config;
    ipc_config_error_t err = ipc_config_load(&config);
    
    assert(err == IPC_CONFIG_ERR_INVALID_BOOL);
    
    printf("OK\n");
}

static void test_invalid_int(void) {
    printf("Test: invalid integer... ");
    
    unsetenv("CGW_IPC_ENABLE");
    setenv("CGW_IPC_TIMEOUT_MS", "abc", 1);
    
    ipc_config_t config;
    ipc_config_error_t err = ipc_config_load(&config);
    
    assert(err == IPC_CONFIG_ERR_INVALID_INT);
    
    printf("OK\n");
}

static void test_out_of_range(void) {
    printf("Test: out of range... ");
    
    unsetenv("CGW_IPC_ENABLE");
    unsetenv("CGW_IPC_TIMEOUT_MS");
    setenv("CGW_IPC_MAX_CONNECTIONS", "99999", 1);
    
    ipc_config_t config;
    ipc_config_error_t err = ipc_config_load(&config);
    
    assert(err == IPC_CONFIG_OK);  /* Load succeeds */
    
    err = ipc_config_validate(&config);
    assert(err == IPC_CONFIG_ERR_OUT_OF_RANGE);  /* Validation fails */
    
    printf("OK\n");
}

static void test_sanitized_print(void) {
    printf("Test: sanitized print... ");
    
    unsetenv("CGW_IPC_ENABLE");
    setenv("CGW_IPC_NATS_URL", "nats://user:pass@localhost:4222", 1);
    
    ipc_config_t config;
    ipc_config_error_t err = ipc_config_load(&config);
    
    assert(err == IPC_CONFIG_OK);
    
    printf("\n");
    ipc_config_print(&config);  /* Should sanitize credentials */
    
    printf("OK\n");
}

int main(void) {
    printf("=== IPC Config Unit Tests ===\n");
    
    test_defaults();
    test_override_env();
    test_invalid_bool();
    test_invalid_int();
    test_out_of_range();
    test_sanitized_print();
    
    printf("\nAll tests passed!\n");
    return 0;
}
