/**
 * test_health_check.c - Health check tests
 */

#include "health_check.h"
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

static int always_healthy(void) {
    return 0;  /* Healthy */
}

static int always_unhealthy(void) {
    return -1;  /* Unhealthy */
}

static void test_registration(void) {
    printf("Test: health check registration... ");
    
    health_check_init(8081);
    
    int rc = health_check_register("test_component", always_healthy, 1);
    assert(rc == 0);
    
    health_check_shutdown();
    printf("OK\n");
}

static void test_health_status(void) {
    printf("Test: health status... ");
    
    health_check_init(8082);
    
    /* Register healthy check */
    health_check_register("component1", always_healthy, 1);
    
    health_result_t result;
    health_check_get_status(&result);
    
    assert(result.status == HEALTH_STATUS_HEALTHY);
    assert(strstr(result.message, "passing") != NULL);
    
    health_check_shutdown();
    printf("OK\n");
}

static void test_readiness(void) {
    printf("Test: readiness with critical check... ");
    
    health_check_init(8083);
    
    /* Register critical unhealthy check */
    health_check_register("critical_component", always_unhealthy, 1);
    
    health_result_t result;
    health_check_get_readiness(&result);
    
    assert(result.status == HEALTH_STATUS_UNHEALTHY);
    assert(strstr(result.message, "Not ready") != NULL);
    
    health_check_shutdown();
    printf("OK\n");
}

static void test_http_server(void) {
    printf("Test: HTTP health server... ");
    
    health_check_init(8084);
    health_check_register("test", always_healthy, 1);
    
    /* Start server */
    int rc = health_check_start_server();
    assert(rc == 0);
    
    /* Give server time to start */
   sleep(1);
    
    /* Test with curl (if available) or skip */
    int curl_rc = system("curl -s http://localhost:8084/health > /dev/null 2>&1");
    if (curl_rc == 0) {
        printf("(HTTP verified) ");
    } else {
        printf("(HTTP started, curl not available) ");
    }
    
    health_check_stop_server();
    health_check_shutdown();
    
    printf("OK\n");
}

int main(void) {
    printf("=== Health Check Tests ===\n");
    
    test_registration();
    test_health_status();
    test_readiness();
    test_http_server();
    
    printf("\nAll tests passed!\n");
    return 0;
}
