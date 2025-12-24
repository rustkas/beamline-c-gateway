/**
 * router_contract.h - Router DecideRequest/Response contract definitions
 * 
 * Canonical Router Contract (CP2 alignment)
 */

#ifndef ROUTER_CONTRACT_H
#define ROUTER_CONTRACT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Router request envelope (canonical)
 * 
 * Example:
 * {
 *   "version": "1.0",
 *   "message_id": "req_123",
 *   "trace_id": "trace_456",
 *   "tenant_id": "tenant1",
 *   "policy_id": "default",
 *   "input": { ... }
 * }
 */
typedef struct {
    char version[16];          /* Protocol version (e.g., "1.0") */
    char message_id[64];       /* Unique message ID */
    char trace_id[64];         /* Trace ID for correlation (optional) */
    char tenant_id[64];        /* Tenant ID (required) */
    char policy_id[64];        /* Policy ID (required) */
    char *input_json;          /* Input payload (owned) */
} router_request_t;

/**
 * Router response envelope (canonical)
 * 
 * Success:
 * {
 *   "ok": true,
 *   "message_id": "req_123",
 *   "result": { ... }
 * }
 * 
 * Error:
 * {
 *   "ok": false,
 *   "message_id": "req_123",
 *   "error": {
 *     "code": "invalid_input",
 *     "message": "Missing required field",
 *     "context": { ... }
 *   }
 * }
 */
typedef struct {
    int ok;                    /* Success flag */
    char message_id[64];       /* Echoed message ID */
    char *result_json;         /* Result payload (if ok=true, owned) */
    
    /* Error fields (if ok=false) */
    char error_code[64];       /* Error code */
    char error_message[256];   /* Error message */
    char *error_context_json;  /* Error context (owned, optional) */
} router_response_t;

/**
 * Contract validation errors
 */
typedef enum {
    ROUTER_CONTRACT_OK = 0,
    ROUTER_CONTRACT_ERR_NULL_PTR,
    ROUTER_CONTRACT_ERR_EMPTY_REQUIRED,
    ROUTER_CONTRACT_ERR_INVALID_VERSION,
    ROUTER_CONTRACT_ERR_INVALID_JSON,
} router_contract_error_t;

/**
 * Validate router request
 * 
 * @param req  Request to validate
 * @return Error code (ROUTER_CONTRACT_OK on success)
 */
router_contract_error_t router_request_validate(const router_request_t *req);

/**
 * Build JSON string from router request
 * 
 * @param req       Request structure
 * @param json_buf  Output buffer
 * @param buf_size  Buffer size
 * @return 0 on success, -1 on error
 */
int router_request_to_json(const router_request_t *req, char *json_buf, size_t buf_size);

/**
 * Parse JSON string to router response
 * 
 * @param json_str  JSON string
 * @param resp      Output response structure (caller must free fields)
 * @return Error code
 */
router_contract_error_t router_response_from_json(const char *json_str, router_response_t *resp);

/**
 * Free router request fields
 */
void router_request_free(router_request_t *req);

/**
 * Free router response fields
 */
void router_response_free(router_response_t *resp);

/**
 * Get error message
 */
const char* router_contract_strerror(router_contract_error_t err);

#ifdef __cplusplus
}
#endif

#endif /* ROUTER_CONTRACT_H */
