#ifndef NATS_CLIENT_STUB_H
#define NATS_CLIENT_STUB_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Get NATS client status string.
 * Returns "stub" for stubbed implementation.
 */
const char *nats_get_status_string(void);

/*
 * Minimal stub for NATS request-reply to Router.
 *
 * req_json  - request JSON (from HTTP DTO)
 * resp_buf  - buffer for response JSON
 * resp_size - size of resp_buf
 *
 * Returns 0 on success, non-zero on error.
 */
int nats_request_decide(const char *req_json, char *resp_buf, size_t resp_size);

/*
 * Fetch decision by tenant_id + message_id.
 *
 * tenant_id  - resolved from X-Tenant-ID
 * message_id - path parameter from HTTP URL
 *
 * Returns 0 on success, non-zero on error.
 */
int nats_request_get_decision(const char *tenant_id,
                              const char *message_id,
                              char *resp_buf,
                              size_t resp_size);

/*
 * Get extension health (admin endpoint).
 *
 * resp_buf  - buffer for response JSON
 * resp_size - size of resp_buf
 *
 * Returns 0 on success, non-zero on error.
 */
int nats_request_get_extension_health(char *resp_buf, size_t resp_size);

/*
 * Get circuit breaker states (admin endpoint).
 *
 * resp_buf  - buffer for response JSON
 * resp_size - size of resp_buf
 *
 * Returns 0 on success, non-zero on error.
 */
int nats_request_get_circuit_breaker_states(char *resp_buf, size_t resp_size);

/*
 * Execute dry-run pipeline (admin endpoint).
 *
 * req_json  - request JSON (tenant_id, policy_id, payload)
 * resp_buf  - buffer for response JSON
 * resp_size - size of resp_buf
 *
 * Returns 0 on success, non-zero on error.
 */
int nats_request_dry_run_pipeline(const char *req_json, char *resp_buf, size_t resp_size);

/*
 * Get pipeline complexity (admin endpoint).
 *
 * tenant_id  - tenant identifier
 * policy_id  - policy identifier
 * resp_buf   - buffer for response JSON
 * resp_size  - size of resp_buf
 *
 * Returns 0 on success, non-zero on error.
 */
int nats_request_get_pipeline_complexity(const char *tenant_id,
                                        const char *policy_id,
                                        char *resp_buf,
                                        size_t resp_size);

#ifdef __cplusplus
}
#endif

#endif /* NATS_CLIENT_STUB_H */
