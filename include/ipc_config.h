/**
 * ipc_config.h - IPC Gateway configuration contract
 * 
 * Environment Variables:
 * - CGW_IPC_ENABLE (0/1, default: 1)
 * - CGW_IPC_SOCKET_PATH (string, default: /tmp/beamline-gateway.sock)
 * - CGW_IPC_NATS_ENABLE (0/1, default: 0 for stub mode)
 * - CGW_IPC_NATS_URL (string, default: nats://localhost:4222)
 * - CGW_IPC_ROUTER_SUBJECT (string, default: beamline.router.v1.decide)
 * - CGW_IPC_TIMEOUT_MS (int, default: 30000)
 * - CGW_IPC_MAX_CONNECTIONS (int, default: 64)
 */

#ifndef IPC_CONFIG_H
#define IPC_CONFIG_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * IPC Configuration
 */
typedef struct {
    /* IPC Server */
    int ipc_enable;               /* Enable IPC server (0/1) */
    char socket_path[256];         /* Unix socket path */
    int max_connections;           /* Max concurrent connections */
    
    /* NATS Integration */
    int nats_enable;               /* Enable NATS forwarding (0/1 for stub/real) */
    char nats_url[256];            /* NATS server URL */
    char router_subject[128];      /* Router NATS subject */
    int timeout_ms;                /* Request timeout (milliseconds) */
    
    /* Internal */
    int validated;                 /* Config has been validated */
} ipc_config_t;

/**
 * Config validation error codes
 */
typedef enum {
    IPC_CONFIG_OK = 0,
    IPC_CONFIG_ERR_INVALID_BOOL,      /* Invalid boolean value */
    IPC_CONFIG_ERR_INVALID_INT,       /* Invalid integer value */
    IPC_CONFIG_ERR_OUT_OF_RANGE,      /* Value out of valid range */
    IPC_CONFIG_ERR_EMPTY_REQUIRED,    /* Required field is empty */
    IPC_CONFIG_ERR_PATH_TOO_LONG,     /* Path exceeds buffer size */
} ipc_config_error_t;

/**
 * Load configuration from environment variables
 * 
 * @param config  Output configuration structure
 * @return Error code (IPC_CONFIG_OK on success)
 */
ipc_config_error_t ipc_config_load(ipc_config_t *config);

/**
 * Validate configuration
 * 
 * @param config  Configuration to validate
 * @return Error code (IPC_CONFIG_OK on success)
 */
ipc_config_error_t ipc_config_validate(const ipc_config_t *config);

/**
 * Print configuration (sanitized, no secrets)
 * 
 * @param config  Configuration to print
 */
void ipc_config_print(const ipc_config_t *config);

/**
 * Get error message for config error code
 * 
 * @param err  Error code
 * @return Error message string
 */
const char* ipc_config_strerror(ipc_config_error_t err);

#ifdef __cplusplus
}
#endif

#endif /* IPC_CONFIG_H */
