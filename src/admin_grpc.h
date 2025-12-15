#ifndef ADMIN_GRPC_H
#define ADMIN_GRPC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Admin response structure
 */
typedef struct {
  int code;
  const char* message;  /* Optional message (can be NULL) */
} admin_resp_t;

/**
 * @brief Admin trace context structure
 */
typedef struct {
  const char* trace_id;
  const char* span_id;
  const char* tenant_id;
} admin_ctx_t;

/* Function declarations */

/**
 * @brief Initialize admin gRPC module
 * @return admin_resp_t with initialization status
 */
admin_resp_t admin_init(void);

/**
 * @brief Get Gateway health status
 * @return admin_resp_t with health status code (200 = healthy, 503 = unhealthy)
 */
admin_resp_t admin_health(void);

/**
 * @brief Get Gateway status information
 * @return admin_resp_t with status code and message
 */
admin_resp_t admin_status(void);

/**
 * @brief Authorize admin action based on API key, role and action
 * @param api_key API key for authentication
 * @param role User role (e.g., "admin", "operator")
 * @param action Action to authorize (e.g., "restart", "config", "metrics")
 * @return true if authorized, false otherwise
 */
bool admin_authorize(const char* api_key, const char* role, const char* action);

/**
 * @brief Get admin metrics (Prometheus format)
 * @param buffer Output buffer for metrics
 * @param buffer_size Buffer size
 * @return Number of bytes written, or -1 on error
 */
int admin_get_metrics(char* buffer, size_t buffer_size);

/**
 * @brief Increment admin metric counter
 * @param name Metric name
 */
void admin_metrics_inc(const char* name);

/**
 * @brief Create trace context for admin operations
 * @param trace_id Trace ID
 * @param span_id Span ID
 * @param tenant_id Tenant ID
 * @return admin_ctx_t with trace context
 */
admin_ctx_t admin_trace_ctx(const char* trace_id, const char* span_id, const char* tenant_id);

#endif /* ADMIN_GRPC_H */

