/**
 * grpc_gateway.h - gRPC to IPC gateway
 * 
 * Task 30: gRPC Gateway (simplified implementation)
 */

#ifndef GRPC_GATEWAY_H
#define GRPC_GATEWAY_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * gRPC gateway configuration
 */
typedef struct {
    uint16_t port;              /* gRPC port (default: 50051) */
    const char *ipc_socket;     /* IPC socket path */
    int max_connections;        /* Max concurrent connections */
} grpc_gateway_config_t;

/**
 * gRPC gateway handle
 */
typedef struct grpc_gateway_t grpc_gateway_t;

/**
 * Initialize gRPC gateway
 * 
 * Creates gRPC server that forwards requests to IPC socket.
 * 
 * @param config  Gateway configuration
 * @return Gateway handle on success, NULL on error
 */
grpc_gateway_t* grpc_gateway_init(const grpc_gateway_config_t *config);

/**
 * Start gRPC gateway server
 * 
 * @param gateway  Gateway handle
 * @return 0 on success, -1 on error
 */
int grpc_gateway_start(grpc_gateway_t *gateway);

/**
 * Stop gRPC gateway server
 * 
 * @param gateway  Gateway handle
 */
void grpc_gateway_stop(grpc_gateway_t *gateway);

/**
 * Get gateway statistics
 * 
 * @param gateway         Gateway handle
 * @param total_requests  Output: total requests handled
 * @param total_errors    Output: total errors
 */
void grpc_gateway_stats(const grpc_gateway_t *gateway,
                        uint64_t *total_requests,
                        uint64_t *total_errors);

/**
 * Destroy gRPC gateway
 * 
 * @param gateway  Gateway handle
 */
void grpc_gateway_destroy(grpc_gateway_t *gateway);

#ifdef __cplusplus
}
#endif

#endif /* GRPC_GATEWAY_H */
