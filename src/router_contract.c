/**
 * router_contract.c - Router contract implementation (MVP)
 */

#include "router_contract.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ROUTER_VERSION "1.0"

router_contract_error_t router_request_validate(const router_request_t *req) {
    if (!req) {
        return ROUTER_CONTRACT_ERR_NULL_PTR;
    }
    
    /* Version */
    if (req->version[0] == '\0') {
        return ROUTER_CONTRACT_ERR_EMPTY_REQUIRED;
    }
    
    if (strcmp(req->version, ROUTER_VERSION) != 0) {
        return ROUTER_CONTRACT_ERR_INVALID_VERSION;
    }
    
    /* Required fields */
    if (req->message_id[0] == '\0' ||
        req->tenant_id[0] == '\0' ||
        req->policy_id[0] == '\0') {
        return ROUTER_CONTRACT_ERR_EMPTY_REQUIRED;
    }
    
    /* Input JSON (can be empty object but must exist) */
    if (!req->input_json) {
        return ROUTER_CONTRACT_ERR_NULL_PTR;
    }
    
    return ROUTER_CONTRACT_OK;
}

int router_request_to_json(const router_request_t *req, char *json_buf, size_t buf_size) {
    if (!req || !json_buf) {
        return -1;
    }
    
    /* Build JSON manually (MVP - later use proper JSON library) */
    int written = snprintf(json_buf, buf_size,
        "{"
        "\"version\":\"%s\","
        "\"message_id\":\"%s\",",
        req->version,
        req->message_id);
    
    if (written < 0 || (size_t)written >= buf_size) {
        return -1;
    }
    
    /* Optional trace_id */
    if (req->trace_id[0] != '\0') {
        int n = snprintf(json_buf + written, buf_size - (size_t)written,
            "\"trace_id\":\"%s\",",
            req->trace_id);
        if (n < 0 || written + n >= (int)buf_size) return -1;
        written += n;
    }
    
    /* Required fields */
    int n = snprintf(json_buf + written, buf_size - (size_t)written,
        "\"tenant_id\":\"%s\","
        "\"policy_id\":\"%s\","
        "\"input\":%s"
        "}",
        req->tenant_id,
        req->policy_id,
        req->input_json ? req->input_json : "{}");
    
    if (n < 0 || written + n >= (int)buf_size) {
        return -1;
    }
    
    return 0;
}

router_contract_error_t router_response_from_json(const char *json_str, router_response_t *resp) {
    if (!json_str || !resp) {
        return ROUTER_CONTRACT_ERR_NULL_PTR;
    }
    
    memset(resp, 0, sizeof(*resp));
    
    /* Simple parsing (MVP - assumes Router returns valid JSON) */
    /* Look for "ok":true or "ok":false */
    if (strstr(json_str, "\"ok\":true") || strstr(json_str, "\"ok\": true")) {
        resp->ok = 1;
        /* Extract result (simplified: just copy entire response for now) */
        resp->result_json = strdup(json_str);
    } else {
        resp->ok = 0;
        /* Extract error fields (simplified) */
        const char *code_start = strstr(json_str, "\"code\":");
        if (code_start) {
            sscanf(code_start, "\"code\":\"%63[^\"]\"", resp->error_code);
        }
        
        const char *msg_start = strstr(json_str, "\"message\":");
        if (msg_start) {
            sscanf(msg_start, "\"message\":\"%255[^\"]\"", resp->error_message);
        }
    }
    
    /* Extract message_id */
    const char *msgid_start = strstr(json_str, "\"message_id\":");
    if (msgid_start) {
        sscanf(msgid_start, "\"message_id\":\"%63[^\"]\"", resp->message_id);
    }
    
    return ROUTER_CONTRACT_OK;
}

void router_request_free(router_request_t *req) {
    if (!req) return;
    free(req->input_json);
    req->input_json = NULL;
}

void router_response_free(router_response_t *resp) {
    if (!resp) return;
    free(resp->result_json);
    free(resp->error_context_json);
    resp->result_json = NULL;
    resp->error_context_json = NULL;
}

const char* router_contract_strerror(router_contract_error_t err) {
    switch (err) {
        case ROUTER_CONTRACT_OK:
            return "No error";
        case ROUTER_CONTRACT_ERR_NULL_PTR:
            return "NULL pointer";
        case ROUTER_CONTRACT_ERR_EMPTY_REQUIRED:
            return "Required field is empty";
        case ROUTER_CONTRACT_ERR_INVALID_VERSION:
            return "Invalid protocol version";
        case ROUTER_CONTRACT_ERR_INVALID_JSON:
            return "Invalid JSON";
        default:
            return "Unknown error";
    }
}
