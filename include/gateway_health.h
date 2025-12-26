/**
 * gateway_health.h - Gateway health check integration
 * 
 * Task 18: Health check API for gateway components
 */

#ifndef GATEWAY_HEALTH_H
#define GATEWAY_HEALTH_H

#include <stdint.h>
#include "nats_resilience.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize gateway health checks
 * 
 * Registers health checks for:
 * - NATS connection
 * - IPC server
 * 
 * Starts HTTP server on specified port with endpoints:
 * - GET /health - Liveness probe
 * - GET /ready - Readiness probe
 * 
 * @param port            HTTP port for health endpoints (e.g., 8080)
 * @param resilience      NATS resilience handle
 * @param ipc_socket_path IPC socket path to monitor
 * @return 0 on success, -1 on error
 */
int gateway_health_init(uint16_t port,
                        nats_resilience_t *resilience,
                        const char *ipc_socket_path);

/**
 * Shutdown gateway health checks
 */
void gateway_health_shutdown(void);

/**
 * Check NATS connection health
 * 
 * @return 0 if healthy, -1 if unhealthy
 */
int gateway_health_check_nats(void);

/**
 * Check IPC server health
 * 
 * @return 0 if healthy, -1 if unhealthy
 */
int gateway_health_check_ipc(void);

#ifdef __cplusplus
}
#endif

#endif /* GATEWAY_HEALTH_H */
