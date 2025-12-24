/**
 * ipc_nats_bridge.c - IPC to NATS bridge implementation
 */

#include "ipc_nats_bridge.h"
#include "nats_client_stub.h"  /* For nats_request_decide */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/**
 * Bridge state
 */
struct ipc_nats_bridge_t {
    ipc_nats_config_t config;
    
    /* Statistics */
    size_t total_requests;
    size_t nats_errors;
    size_t timeouts;
};

/**
 * Generate unique task ID
 */
static void generate_task_id(char *buf, size_t buf_size) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    snprintf(buf, buf_size, "task_%ld_%ld", ts.tv_sec, ts.tv_nsec);
}

/**
 * Transform IPC message to NATS request
 * 
 * IPC payload (JSON) needs to be wrapped in Router envelope:
 * {
 *   "from": "ide@localhost",
 *   "to": "router",
 *   "message_id": "generated-uuid",
 *   "tenant_id": "default",
 *   "policy_id": "default",
 *   "input": <IPC payload>
 * }
 */
static int transform_to_nats_request(const ipc_message_t *ipc_msg, 
                                     char *nats_req_buf, 
                                     size_t nats_req_size) {
    char task_id[64];
    generate_task_id(task_id, sizeof(task_id));
    
    /* Build NATS request envelope */
    int written = snprintf(nats_req_buf, nats_req_size,
        "{"
        "\"from\":\"ide@localhost\","
        "\"to\":\"router\","
        "\"message_id\":\"%s\","
        "\"tenant_id\":\"default\","
        "\"policy_id\":\"default\","
        "\"input\":%s"
        "}",
        task_id,
        ipc_msg->payload ? ipc_msg->payload : "{}"
    );
    
    if (written < 0 || (size_t)written >= nats_req_size) {
        fprintf(stderr, "[bridge] NATS request buffer overflow\n");
        return -1;
    }
    
    return 0;
}

/**
 * Transform NATS response to IPC message
 * 
 * Router returns:
 * {
 *   "message_id": "...",
 *   "status": "ok|error",
 *   "result": {...}
 * }
 * 
 * We pass through as-is (IPC client sees Router response directly)
 */
static void transform_to_ipc_response(const char *nats_resp,
                                      int nats_rc,
                                      ipc_message_t *ipc_resp) {
    if (nats_rc == 0 && nats_resp) {
        /* Success */
        ipc_resp->type = IPC_MSG_RESPONSE_OK;
        ipc_resp->payload = strdup(nats_resp);
        ipc_resp->payload_len = strlen(nats_resp);
    } else {
        /* NATS error */
        ipc_create_error_response(IPC_ERR_INTERNAL, 
                                  "NATS request failed", 
                                  ipc_resp);
    }
}

/**
 * Message handler: IPC → NATS → IPC
 */
static void bridge_message_handler(const ipc_message_t *request,
                                   ipc_message_t *response,
                                   void *user_data) {
    ipc_nats_bridge_t *bridge = (ipc_nats_bridge_t*)user_data;
    
    if (!bridge) {
        ipc_create_error_response(IPC_ERR_INTERNAL, "Bridge not initialized", response);
        return;
    }
    
    bridge->total_requests++;
    
    /* Log request */
    printf("[bridge] Received IPC message type=0x%02x payload_len=%zu\n",
           request->type, request->payload_len);
    
    /* Handle based on message type */
    switch (request->type) {
        case IPC_MSG_PING:
            /* Respond with PONG immediately (no NATS) */
            response->type = IPC_MSG_PONG;
            response->payload = NULL;
            response->payload_len = 0;
            return;
            
        case IPC_MSG_TASK_SUBMIT:
        case IPC_MSG_TASK_QUERY:
        case IPC_MSG_TASK_CANCEL:
            /* Forward to NATS */
            break;
            
        default:
            ipc_create_error_response(IPC_ERR_INVALID_TYPE, 
                                     "Unsupported message type", 
                                     response);
            return;
    }
    
    /* Transform IPC request to NATS format */
    char nats_req[8192];
    if (transform_to_nats_request(request, nats_req, sizeof(nats_req)) < 0) {
        ipc_create_error_response(IPC_ERR_INVALID_PAYLOAD,
                                 "Failed to transform request",
                                 response);
        bridge->nats_errors++;
        return;
    }
    
    printf("[bridge] NATS request: %s\n", nats_req);
    
    /* Call NATS client */
    char nats_resp[8192];
    int nats_rc;
    
    if (bridge->config.enable_nats) {
        /* Real NATS client */
        nats_rc = nats_request_decide(nats_req, nats_resp, sizeof(nats_resp));
    } else {
        /* Stub mode (for testing without Router) */
        nats_rc = 0;
        snprintf(nats_resp, sizeof(nats_resp),
            "{\"message_id\":\"stub\",\"status\":\"ok\","
            "\"result\":{\"echo\":%s}}",
            request->payload ? request->payload : "null");
    }
    
    printf("[bridge] NATS response (rc=%d): %s\n", nats_rc, nats_resp);
    
    if (nats_rc != 0) {
        bridge->nats_errors++;
    }
    
    /* Transform NATS response to IPC format */
    transform_to_ipc_response(nats_resp, nats_rc, response);
}

ipc_nats_bridge_t* ipc_nats_bridge_init(const ipc_nats_config_t *config) {
    if (!config) {
        fprintf(stderr, "[bridge] NULL config\n");
        return NULL;
    }
    
    ipc_nats_bridge_t *bridge = (ipc_nats_bridge_t*)calloc(1, sizeof(ipc_nats_bridge_t));
    if (!bridge) {
        return NULL;
    }
    
    /* Copy config */
    bridge->config.nats_url = config->nats_url ? strdup(config->nats_url) : NULL;
    bridge->config.router_subject = config->router_subject ? strdup(config->router_subject) : NULL;
    bridge->config.timeout_ms = config->timeout_ms > 0 ? config->timeout_ms : 30000;
    bridge->config.enable_nats = config->enable_nats;
    
    printf("[bridge] Initialized (enable_nats=%d, timeout=%d ms)\n",
           bridge->config.enable_nats, bridge->config.timeout_ms);
    
    return bridge;
}

ipc_message_handler_fn ipc_nats_bridge_get_handler(ipc_nats_bridge_t *bridge) {
    (void)bridge;  /* Handler uses bridge via user_data */
    return bridge_message_handler;
}

void ipc_nats_bridge_get_stats(ipc_nats_bridge_t *bridge,
                                size_t *total_reqs,
                                size_t *nats_errors,
                                size_t *timeouts) {
    if (!bridge) {
        return;
    }
    
    if (total_reqs) *total_reqs = bridge->total_requests;
    if (nats_errors) *nats_errors = bridge->nats_errors;
    if (timeouts) *timeouts = bridge->timeouts;
}

void ipc_nats_bridge_destroy(ipc_nats_bridge_t *bridge) {
    if (!bridge) {
        return;
    }
    
    printf("[bridge] Shutting down (total_reqs=%zu, errors=%zu)\n",
           bridge->total_requests, bridge->nats_errors);
    
    free((void*)bridge->config.nats_url);
    free((void*)bridge->config.router_subject);
    free(bridge);
}
