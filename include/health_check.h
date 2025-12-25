/**
 * health_check.h - Health and readiness check endpoints
 * 
 * Provides liveness and readiness probes for orchestration (K8s)
 */

#ifndef HEALTH_CHECK_H
#define HEALTH_CHECK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Health status
 */
typedef enum {
    HEALTH_STATUS_HEALTHY = 0,    /* All checks passing */
    HEALTH_STATUS_DEGRADED = 1,   /* Some checks failing */
    HEALTH_STATUS_UNHEALTHY = 2   /* Critical checks failing */
} health_status_t;

/**
 * Component health check function
 * 
 * @return 0 if healthy, non-zero if unhealthy
 */
typedef int (*health_check_fn_t)(void);

/**
 * Health check result
 */
typedef struct {
    health_status_t status;
    char message[256];
} health_result_t;

/**
 * Initialize health check system
 * 
 * @param port  HTTP port for health endpoints
 * @return 0 on success, -1 on error
 */
int health_check_init(uint16_t port);

/**
 * Register a component health check
 * 
 * @param name     Component name
 * @param check_fn Check function
 * @param critical Is this check critical? (affects readiness)
 * @return 0 on success, -1 on error
 */
int health_check_register(
    const char *name,
    health_check_fn_t check_fn,
    int critical
);

/**
 * Get overall health status
 * 
 * @param result  Output health result
 * @return 0 on success, -1 on error
 */
int health_check_get_status(health_result_t *result);

/**
 * Get readiness status (for K8s readiness probe)
 * 
 * @param result  Output readiness result
 * @return 0 on success, -1 on error
 */
int health_check_get_readiness(health_result_t *result);

/**
 * Start health check HTTP server
 * 
 * Serves:
 * - GET /health - Liveness probe (always 200 if process running)
 * - GET /ready - Readiness probe (200 if all critical checks pass)
 * 
 * @return 0 on success, -1 on error
 */
int health_check_start_server(void);

/**
 * Stop health check server
 */
void health_check_stop_server(void);

/**
 * Cleanup health check system
 */
void health_check_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* HEALTH_CHECK_H */
