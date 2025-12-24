/**
 * ipc_server.h - Unix domain socket IPC server
 */

#ifndef IPC_SERVER_H
#define IPC_SERVER_H

#include "ipc_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque server handle */
typedef struct ipc_server_t ipc_server_t;

/**
 * Message handler callback
 * 
 * @param request     Incoming request message
 * @param response    Response message to fill (handler must set type and payload)
 * @param user_data   User data passed to ipc_server_set_handler
 */
typedef void (*ipc_message_handler_fn)(const ipc_message_t *request, 
                                       ipc_message_t *response,
                                       void *user_data);

/**
 * Initialize IPC server
 * 
 * @param socket_path  Unix socket path (NULL for default: /tmp/beamline-gateway.sock)
 * @return Server handle, or NULL on error
 */
ipc_server_t* ipc_server_init(const char *socket_path);

/**
 * Set message handler callback
 * 
 * @param server      Server handle
 * @param handler     Message handler function
 * @param user_data   User data to pass to handler
 */
void ipc_server_set_handler(ipc_server_t *server, 
                            ipc_message_handler_fn handler,
                            void *user_data);

/**
 * Run server event loop (blocks until ipc_server_stop called)
 * 
 * @param server Server handle
 */
void ipc_server_run(ipc_server_t *server);

/**
 * Stop server event loop
 * 
 * @param server Server handle
 */
void ipc_server_stop(ipc_server_t *server);

/**
 * Cleanup server and close all connections
 * 
 * @param server Server handle
 */
void ipc_server_destroy(ipc_server_t *server);

#ifdef __cplusplus
}
#endif

#endif /* IPC_SERVER_H */
