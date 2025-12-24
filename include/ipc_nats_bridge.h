/**
 * ipc_nats_bridge.h - Bridge between IPC server and NATS client
 * 
 * Forwards IDE commands from IPC to Router via NATS request-reply
 */

#ifndef IPC_NATS_BRIDGE_H
#define IPC_NATS_BRIDGE_H

#include "ipc_protocol.h"
#include "ipc_server.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * JSON Schemas for IDE Commands
 * 
 * TaskSubmit:
 * {
 *   "command": "task_submit",
 *   "task_type": "code_completion|error_analysis|...",
 *   "file": "/path/to/file.ext",
 *   "line": 42,
 *   "context": "surrounding code...",
 *   "params": {...}  // task-specific parameters
 * }
 * 
 * TaskQuery:
 * {
 *   "command": "task_query",
 *   "task_id": "uuid-..."
 * }
 * 
 * TaskCancel:
 * {
 *   "command": "task_cancel",
 *   "task_id": "uuid-..."
 * }
 */

/**
 * Bridge configuration
 */
typedef struct {
    const char *nats_url;            /* NATS server URL (e.g., "nats://localhost:4222") */
    const char *router_subject;      /* NATS subject for Router (e.g., "beamline.router.v1.decide") */
    int timeout_ms;                  /* Request timeout in milliseconds */
    int enable_nats;                 /* 0 = stub mode, 1 = real NATS */
} ipc_nats_config_t;

/**
 * Bridge context (opaque)
 */
typedef struct ipc_nats_bridge_t ipc_nats_bridge_t;

/**
 * Create IPC-NATS bridge
 * 
 * @param config  Bridge configuration
 * @return Bridge handle, or NULL on error
 */
ipc_nats_bridge_t* ipc_nats_bridge_init(const ipc_nats_config_t *config);

/**
 * Get message handler for IPC server
 * 
 * This handler forwards IPC messages to NATS and returns Router responses.
 * 
 * Usage:
 *   ipc_server_t *server = ipc_server_init(socket_path);
 *   ipc_nats_bridge_t *bridge = ipc_nats_bridge_init(&config);
 *   ipc_server_set_handler(server, ipc_nats_bridge_get_handler(bridge), bridge);
 * 
 * @param bridge  Bridge handle
 * @return Message handler function pointer
 */
ipc_message_handler_fn ipc_nats_bridge_get_handler(ipc_nats_bridge_t *bridge);

/**
 * Get bridge statistics
 * 
 * @param bridge       Bridge handle
 * @param total_reqs   Output: total requests processed
 * @param nats_errors  Output: NATS request errors
 * @param timeouts     Output: timeout count
 */
void ipc_nats_bridge_get_stats(ipc_nats_bridge_t *bridge,
                                size_t *total_reqs,
                                size_t *nats_errors,
                                size_t *timeouts);

/**
 * Destroy bridge
 * 
 * @param bridge Bridge handle
 */
void ipc_nats_bridge_destroy(ipc_nats_bridge_t *bridge);

#ifdef __cplusplus
}
#endif

#endif /* IPC_NATS_BRIDGE_H */
