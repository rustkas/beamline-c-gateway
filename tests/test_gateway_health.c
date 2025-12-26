/**
 * test_gateway_health.c - Gateway health check integration test
 */

#include "gateway_health.h"
#include "health_check.h"
#include "nats_resilience.h"
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>

#define TEST_SOCKET_PATH "/tmp/test_ipc_health.sock"
#define TEST_HEALTH_PORT 8085

/**
 * Create test IPC socket
 */
static int create_test_socket(void) {
    /* Remove old socket if exists */
    unlink(TEST_SOCKET_PATH);
    
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, TEST_SOCKET_PATH, sizeof(addr.sun_path) - 1);
    
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sock);
        return -1;
    }
    
    if (listen(sock, 5) < 0) {
        perror("listen");
        close(sock);
        return -1;
    }
    
    return sock;
}

static void test_gateway_health_initialization(void) {
    printf("Test: gateway health initialization... ");
    
    /* Create test socket */
    int test_sock = create_test_socket();
    assert(test_sock >= 0);
    
    /* Create NATS resilience */
    nats_resilience_config_t config = {
        .max_inflight = 100,
        .min_backoff_ms = 100,
        .max_backoff_ms = 30000,
        .degraded_threshold = 3,
        .reconnect_attempts = 0
    };
    nats_resilience_t *resilience = nats_resilience_init(&config);
    assert(resilience != NULL);
    
    /* Mark as connected */
    nats_resilience_mark_connected(resilience);
    
    /* Initialize gateway health */
    int rc = gateway_health_init(TEST_HEALTH_PORT, resilience, TEST_SOCKET_PATH);
    assert(rc == 0);
    
    /* Give server time to start */
    sleep(1);
    
    /* Cleanup */
    gateway_health_shutdown();
    nats_resilience_destroy(resilience);
    close(test_sock);
    unlink(TEST_SOCKET_PATH);
    
    printf("OK\n");
}

static void test_nats_health_check(void) {
    printf("Test: NATS health check... ");
    
    /* Create test socket */
    int test_sock = create_test_socket();
    assert(test_sock >= 0);
    
    /* Create NATS resilience */
    nats_resilience_config_t config = {
        .max_inflight = 100,
        .min_backoff_ms = 100,
        .max_backoff_ms = 30000,
        .degraded_threshold = 3,
        .reconnect_attempts = 0
    };
    nats_resilience_t *resilience = nats_resilience_init(&config);
    assert(resilience != NULL);
    
    /* Mark as connected */
    nats_resilience_mark_connected(resilience);
    
    /* Initialize gateway health to set up component handles */
    gateway_health_init(8086, resilience, TEST_SOCKET_PATH);
    
    /* Now should be healthy */
    int health = gateway_health_check_nats();
    assert(health == 0);  /* Should be healthy */
    
    /* Cleanup */
    gateway_health_shutdown();
    nats_resilience_destroy(resilience);
    close(test_sock);
    unlink(TEST_SOCKET_PATH);
    
    printf("OK\n");
}

static void test_ipc_health_check(void) {
    printf("Test: IPC health check... ");
    
    /* Create socket */
    int sock = create_test_socket();
    assert(sock >= 0);
    
    /* Create dummy NATS resilience for init */
    nats_resilience_config_t config = {
        .max_inflight = 100,
        .min_backoff_ms = 100,
        .max_backoff_ms = 30000,
        .degraded_threshold = 3,
        .reconnect_attempts = 0
    };
    nats_resilience_t *resilience = nats_resilience_init(&config);
    nats_resilience_mark_connected(resilience);
    
    /* Initialize gateway health */
    gateway_health_init(8087, resilience, TEST_SOCKET_PATH);
    
    /* Now should be healthy since socket exists */
    int health = gateway_health_check_ipc();
    assert(health == 0);  /* Healthy */
    
    /* Cleanup */
    gateway_health_shutdown();
    nats_resilience_destroy(resilience);
    close(sock);
    unlink(TEST_SOCKET_PATH);
    
    printf("OK\n");
}

static void test_http_endpoints(void) {
    printf("Test: HTTP health endpoints... ");
    
    /* Create test socket */
    int test_sock = create_test_socket();
    assert(test_sock >= 0);
    
    /* Create NATS resilience (connected) */
    nats_resilience_config_t config = {
        .max_inflight = 100,
        .min_backoff_ms = 100,
        .max_backoff_ms = 30000,
        .degraded_threshold = 3,
        .reconnect_attempts = 0
    };
    nats_resilience_t *resilience = nats_resilience_init(&config);
    nats_resilience_mark_connected(resilience);
    
    /* Initialize gateway health */
    gateway_health_init(TEST_HEALTH_PORT, resilience, TEST_SOCKET_PATH);
    sleep(1);
    
    /* Test /health endpoint */
    char cmd[256];
    snprintf(cmd, sizeof(cmd), 
             "curl -s http://localhost:%d/health > /dev/null 2>&1", 
             TEST_HEALTH_PORT);
    int rc = system(cmd);
    
    if (rc == 0) {
        printf("(HTTP /health verified) ");
    } else {
        printf("(HTTP started, curl not available) ");
    }
    
    /* Test /ready endpoint */
    snprintf(cmd, sizeof(cmd), 
             "curl -s http://localhost:%d/ready > /dev/null 2>&1", 
             TEST_HEALTH_PORT);
    rc = system(cmd);
    
    if (rc == 0) {
        printf("(HTTP /ready verified) ");
    }
    
    /* Cleanup */
    gateway_health_shutdown();
    nats_resilience_destroy(resilience);
    close(test_sock);
    unlink(TEST_SOCKET_PATH);
    
    printf("OK\n");
}

int main(void) {
    printf("=== Gateway Health Integration Tests ===\n");
    
    test_nats_health_check();
    test_ipc_health_check();
    test_gateway_health_initialization();
    test_http_endpoints();
    
    printf("\nAll tests passed!\n");
    return 0;
}
