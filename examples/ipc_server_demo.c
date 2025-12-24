/**
 * ipc_server_demo.c - Simple IPC server demo
 * 
 * Usage:
 *   ./ipc_server_demo [socket_path]
 * 
 * Test with:
 *   echo '{"task":"hello"}' | socat - UNIX-CONNECT:/tmp/beamline-gateway.sock
 */

#include "ipc_server.h"
#include "ipc_protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

static ipc_server_t *g_server = NULL;

void signal_handler(int sig) {
    (void)sig;
    printf("\nShutting down...\n");
    if (g_server) {
        ipc_server_stop(g_server);
    }
}

void handle_message(const ipc_message_t *request, ipc_message_t *response, void *user_data) {
    (void)user_data;
    
    printf("[handler] Received message type=%d payload_len=%zu\n", 
           request->type, request->payload_len);
    
    if (request->payload) {
        printf("[handler] Payload: %s\n", request->payload);
    }
    
    /* Echo back with success */
    response->type = IPC_MSG_RESPONSE_OK;
    
    char resp_buf[512];
    int len = snprintf(resp_buf, sizeof(resp_buf),
        "{\"ok\":true,\"echo\":%s}",
        request->payload ? request->payload : "null");
    
    if (len > 0 && (size_t)len < sizeof(resp_buf)) {
        response->payload = strdup(resp_buf);
        response->payload_len = (size_t)len;
    } else {
        response->payload = strdup("{\"ok\":true}");
        response->payload_len = strlen(response->payload);
    }
}

int main(int argc, char *argv[]) {
    const char *socket_path = (argc > 1) ? argv[1] : NULL;
    
    printf("IPC Server Demo\n");
    printf("===============\n\n");
    
    /* Setup signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Create server */
    g_server = ipc_server_init(socket_path);
    if (!g_server) {
        fprintf(stderr, "Failed to initialize IPC server\n");
        return 1;
    }
    
    /* Set message handler */
    ipc_server_set_handler(g_server, handle_message, NULL);
    
    printf("Server ready. Press Ctrl+C to stop.\n\n");
    printf("Test with:\n");
    printf("  echo '{\"task\":\"hello\"}' | socat - UNIX-CONNECT:%s\n\n",
           socket_path ? socket_path : "/tmp/beamline-gateway.sock");
    
    /* Run event loop */
    ipc_server_run(g_server);
    
    /* Cleanup */
    ipc_server_destroy(g_server);
    printf("Server stopped.\n");
    
    return 0;
}
