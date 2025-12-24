/**
 * ipc_nats_demo.c - IPC server with NATS integration demo
 * 
 * Usage:
 *   ./ipc_nats_demo [socket_path] [enable_nats]
 * 
 * Examples:
 *   ./ipc_nats_demo /tmp/beamline-gateway.sock 0   # Stub mode
 *   ./ipc_nats_demo /tmp/beamline-gateway.sock 1   # Real NATS
 * 
 * Test with:
 *   python3 tests/test_ipc_client.py
 */

#include "ipc_server.h"
#include "ipc_nats_bridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

static ipc_server_t *g_server = NULL;
static ipc_nats_bridge_t *g_bridge = NULL;

void signal_handler(int sig) {
    (void)sig;
    printf("\nShutting down...\n");
    if (g_server) {
        ipc_server_stop(g_server);
    }
}

int main(int argc, char *argv[]) {
    const char *socket_path = (argc > 1) ? argv[1] : NULL;
    int enable_nats = (argc > 2) ? atoi(argv[2]) : 0;
    
    printf("IPC Server with NATS Integration\n");
    printf("=================================\n\n");
    
    /* Setup signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Create NATS bridge */
    ipc_nats_config_t bridge_config = {
        .nats_url = "nats://localhost:4222",
        .router_subject = "beamline.router.v1.decide",
        .timeout_ms = 30000,
        .enable_nats = enable_nats
    };
    
    g_bridge = ipc_nats_bridge_init(&bridge_config);
    if (!g_bridge) {
        fprintf(stderr, "Failed to initialize NATS bridge\n");
        return 1;
    }
    
    printf("Mode: %s\n", enable_nats ? "REAL NATS" : "STUB (no Router needed)");
    printf("NATS URL: %s\n", bridge_config.nats_url);
    printf("Router Subject: %s\n\n", bridge_config.router_subject);
    
    /* Create IPC server */
    g_server = ipc_server_init(socket_path);
    if (!g_server) {
        fprintf(stderr, "Failed to initialize IPC server\n");
        ipc_nats_bridge_destroy(g_bridge);
        return 1;
    }
    
    /* Connect bridge to server */
    ipc_server_set_handler(g_server, 
                           ipc_nats_bridge_get_handler(g_bridge),
                           g_bridge);
    
    printf("Server ready. Press Ctrl+C to stop.\n\n");
    printf("Test with:\n");
    printf("  python3 tests/test_ipc_client.py\n\n");
    
    if (!enable_nats) {
        printf("NOTE: Running in STUB mode (no Router connection).\n");
        printf("      Set enable_nats=1 to forward to real Router.\n\n");
    }
    
    /* Run event loop */
    ipc_server_run(g_server);
    
    /* Print statistics */
    size_t total_reqs, nats_errors, timeouts;
    ipc_nats_bridge_get_stats(g_bridge, &total_reqs, &nats_errors, &timeouts);
    
    printf("\nStatistics:\n");
    printf("  Total requests: %zu\n", total_reqs);
    printf("  NATS errors:    %zu\n", nats_errors);
    printf("  Timeouts:       %zu\n", timeouts);
    
    /* Cleanup */
    ipc_server_destroy(g_server);
    ipc_nats_bridge_destroy(g_bridge);
    
    printf("Server stopped.\n");
    return 0;
}
