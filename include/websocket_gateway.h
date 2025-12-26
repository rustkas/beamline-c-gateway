/**
 * websocket_gateway.h - Simple WebSocket to IPC gateway
 * 
 * Task 29: WebSocket Gateway (simplified implementation)
 * No external dependencies - uses manual WebSocket framing
 */

#ifndef WEBSOCKET_GATEWAY_H
#define WEBSOCKET_GATEWAY_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * WebSocket gateway configuration
 */
typedef struct {
    uint16_t port;              /* HTTP port for WebSocket (default: 8081) */
    const char *ipc_socket;     /* IPC socket path to connect to */
    int max_connections;        /* Max concurrent WebSocket connections */
} websocket_gateway_config_t;

/**
 * WebSocket gateway handle
 */
typedef struct websocket_gateway_t websocket_gateway_t;

/**
 * Initialize WebSocket gateway
 * 
 * Starts HTTP server that:
 * 1. Accepts WebSocket upgrade requests
 * 2. Forwards messages to IPC socket
 * 3. Returns IPC responses to WebSocket client
 * 
 * @param config  Gateway configuration
 * @return Gateway handle on success, NULL on error
 */
websocket_gateway_t* websocket_gateway_init(const websocket_gateway_config_t *config);

/**
 * Start WebSocket gateway server
 * 
 * Runs in background thread.
 * 
 * @param gateway  Gateway handle
 * @return 0 on success, -1 on error
 */
int websocket_gateway_start(websocket_gateway_t *gateway);

/**
 * Stop WebSocket gateway server
 * 
 * @param gateway  Gateway handle
 */
void websocket_gateway_stop(websocket_gateway_t *gateway);

/**
 * Get gateway statistics
 * 
 * @param gateway              Gateway handle
 * @param active_connections   Output: active WebSocket connections
 * @param total_messages       Output: total messages forwarded
 * @param total_errors         Output: total errors
 */
void websocket_gateway_stats(const websocket_gateway_t *gateway,
                              int *active_connections,
                              uint64_t *total_messages,
                              uint64_t *total_errors);

/**
 * Destroy WebSocket gateway
 * 
 * @param gateway  Gateway handle
 */
void websocket_gateway_destroy(websocket_gateway_t *gateway);

#ifdef __cplusplus
}
#endif

#endif /* WEBSOCKET_GATEWAY_H */
