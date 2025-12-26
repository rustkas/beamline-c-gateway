/**
 * gateway_health.c - Health check implementations for gateway components
 * 
 * Task 18: Health check functions for NATS and IPC server
 */

#include "gateway_health.h"
#include "health_check.h"
#include "nats_resilience.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>

/* Global state (set by main) */
static nats_resilience_t *g_nats_resilience = NULL;
static const char *g_ipc_socket_path = NULL;

/**
 * Set NATS resilience handle for health checks
 */
static void gateway_health_set_nats(nats_resilience_t *resilience) {
    g_nats_resilience = resilience;
}

/**
 * Set IPC socket path for health checks
 */
static void gateway_health_set_ipc_socket(const char *socket_path) {
    g_ipc_socket_path = socket_path;
}

/**
 * Check NATS connection health
 */
int gateway_health_check_nats(void) {
    if (!g_nats_resilience) {
        return -1;  /* Not configured */
    }
    
    nats_connection_state_t state = nats_resilience_get_state(g_nats_resilience);
    
    /* Healthy if connected or degraded (but not disconnected) */
    if (state == NATS_STATE_CONNECTED || state == NATS_STATE_DEGRADED) {
        return 0;  /* Healthy */
    }
    
    return -1;  /* Unhealthy */
}

/**
 * Check IPC server health
 */
int gateway_health_check_ipc(void) {
    if (!g_ipc_socket_path) {
        return -1;  /* Not configured */
    }
    
    /* Check if socket file exists */
    struct stat st;
    if (stat(g_ipc_socket_path, &st) == 0) {
        /* Socket exists - assume healthy */
        return 0;
    }
    
    return -1;  /* Socket missing - unhealthy */
}

/**
 * Initialize gateway health checks
 * 
 * @param port            HTTP port for health endpoints
 * @param resilience      NATS resilience handle
 * @param ipc_socket_path IPC socket path
 * @return 0 on success, -1 on error
 */
int gateway_health_init(uint16_t port, 
                        nats_resilience_t *resilience,
                        const char *ipc_socket_path) {
    /* Initialize health check system */
    if (health_check_init(port) != 0) {
        fprintf(stderr, "[gateway_health] Failed to initialize health check system\n");
        return -1;
    }
    
    /* Store component handles */
    gateway_health_set_nats(resilience);
    gateway_health_set_ipc_socket(ipc_socket_path);
    
    /* Register NATS health check (critical) */
    if (health_check_register("nats_connection", gateway_health_check_nats, 1) != 0) {
        fprintf(stderr, "[gateway_health] Failed to register NATS health check\n");
        return -1;
    }
    
    /* Register IPC server health check (critical) */
    if (health_check_register("ipc_server", gateway_health_check_ipc, 1) != 0) {
        fprintf(stderr, "[gateway_health] Failed to register IPC health check\n");
        return -1;
    }
    
    /* Start HTTP server for health endpoints */
    if (health_check_start_server() != 0) {
        fprintf(stderr, "[gateway_health] Failed to start health check server\n");
        return -1;
    }
    
    printf("[gateway_health] Initialized on port %d\n", port);
    printf("[gateway_health] Endpoints: /health (liveness), /ready (readiness)\n");
    
    return 0;
}

/**
 * Shutdown gateway health checks
 */
void gateway_health_shutdown(void) {
    health_check_shutdown();
    printf("[gateway_health] Shutdown complete\n");
}
