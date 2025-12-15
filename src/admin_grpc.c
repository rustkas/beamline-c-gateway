#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>

#include "admin_grpc.h"
#include "metrics/metrics_registry.h"
#include "metrics/prometheus.h"
#include "nats_client_stub.h"

/* Admin metrics (initialized in admin_init) */
static prometheus_counter_t *metric_gateway_admin_requests_total = NULL;
static prometheus_counter_t *metric_gateway_admin_errors_total = NULL;

/* Forward declarations */
static bool validate_api_key(const char* api_key);
static int get_gateway_health_status(void);
static const char* get_gateway_status_string(void);
static void record_admin_metric(const char* metric_name, bool success);

/**
 * @brief Initialize admin gRPC module
 * @return admin_resp_t with initialization status
 */
admin_resp_t admin_init(void) {
  admin_resp_t r = { .code = 0, .message = NULL };
  
  /* Initialize admin metrics if not already initialized */
  /* Note: Metrics should be initialized in metrics_registry_init() for production */
  /* For now, initialize here if prometheus is available */
  if (!metric_gateway_admin_requests_total) {
    /* Try to create metrics (will be NULL if prometheus not initialized) */
    metric_gateway_admin_requests_total = prometheus_counter_create(
      "gateway_admin_requests_total",
      "Total number of admin API requests"
    );
    metric_gateway_admin_errors_total = prometheus_counter_create(
      "gateway_admin_errors_total",
      "Total number of admin API errors"
    );
  }
  
  record_admin_metric("gateway_admin_init_total", true);
  return r;
}

/**
 * @brief Get Gateway health status
 * @return admin_resp_t with health status code (200 = healthy, 503 = unhealthy)
 */
admin_resp_t admin_health(void) {
  admin_resp_t r;
  
  /* Check Gateway health components */
  int health_status = get_gateway_health_status();
  
  if (health_status == 200) {
    r.code = 200;
    r.message = "healthy";
    record_admin_metric("gateway_admin_health_total", true);
  } else {
    r.code = 503;
    r.message = "unhealthy";
    record_admin_metric("gateway_admin_health_total", false);
  }
  
  return r;
}

/**
 * @brief Get Gateway status information
 * @return admin_resp_t with status code and message
 */
admin_resp_t admin_status(void) {
  admin_resp_t r;
  
  /* Get Gateway status */
  const char* status_str = get_gateway_status_string();
  
  r.code = 200;
  r.message = status_str;
  record_admin_metric("gateway_admin_status_total", true);
  
  return r;
}

/**
 * @brief Authorize admin action based on API key, role and action
 * @param api_key API key for authentication (required)
 * @param role User role (e.g., "admin", "operator")
 * @param action Action to authorize (e.g., "restart", "config", "metrics")
 * @return true if authorized, false otherwise
 */
bool admin_authorize(const char* api_key, const char* role, const char* action) {
  if (!api_key || !role || !action) {
    record_admin_metric("gateway_admin_auth_total", false);
    return false;
  }
  
  /* Validate API key */
  if (!validate_api_key(api_key)) {
    record_admin_metric("gateway_admin_auth_total", false);
    return false;
  }
  
  /* Check role-based authorization */
  bool authorized = false;
  
  if (strcmp(role, "admin") == 0) {
    /* Admin can perform all actions */
    authorized = true;
  } else if (strcmp(role, "operator") == 0) {
    /* Operator can perform read-only actions */
    if (strcmp(action, "health") == 0 || 
        strcmp(action, "status") == 0 || 
        strcmp(action, "metrics") == 0) {
      authorized = true;
    }
  } else if (strcmp(role, "viewer") == 0) {
    /* Viewer can only read metrics */
    if (strcmp(action, "metrics") == 0) {
      authorized = true;
    }
  }
  
  record_admin_metric("gateway_admin_auth_total", authorized);
  return authorized;
}

/**
 * @brief Get admin metrics (Prometheus format)
 * @param buffer Output buffer for metrics
 * @param buffer_size Buffer size
 * @return Number of bytes written, or -1 on error
 */
int admin_get_metrics(char* buffer, size_t buffer_size) {
  if (!buffer || buffer_size == 0) {
    return -1;
  }
  
  /* Get metrics from metrics registry */
  /* This is a simplified version - in production, use prometheus_export_text */
  int written = snprintf(buffer, buffer_size,
    "# HELP gateway_admin_requests_total Total admin API requests\n"
    "# TYPE gateway_admin_requests_total counter\n"
    "gateway_admin_requests_total %d\n"
    "# HELP gateway_admin_errors_total Total admin API errors\n"
    "# TYPE gateway_admin_errors_total counter\n"
    "gateway_admin_errors_total %d\n",
    /* TODO: Get actual metric values from metrics registry */
    0, 0
  );
  
  record_admin_metric("gateway_admin_metrics_total", written > 0);
  return written;
}

/**
 * @brief Increment admin metric counter
 * @param name Metric name
 */
void admin_metrics_inc(const char* name) {
  if (!name) return;
  
  /* Record metric via metrics registry */
  if (strcmp(name, "gateway_admin_requests_total") == 0) {
    if (metric_gateway_admin_requests_total) {
      prometheus_counter_inc(metric_gateway_admin_requests_total);
    }
  } else if (strcmp(name, "gateway_admin_errors_total") == 0) {
    if (metric_gateway_admin_errors_total) {
      prometheus_counter_inc(metric_gateway_admin_errors_total);
    }
  }
}

/**
 * @brief Create trace context for admin operations
 * @param trace_id Trace ID
 * @param span_id Span ID
 * @param tenant_id Tenant ID
 * @return admin_ctx_t with trace context
 */
admin_ctx_t admin_trace_ctx(const char* trace_id, const char* span_id, const char* tenant_id) {
  admin_ctx_t c = { 
    .trace_id = trace_id ? trace_id : "",
    .span_id = span_id ? span_id : "",
    .tenant_id = tenant_id ? tenant_id : ""
  };
  return c;
}

/* Internal helper functions */

/**
 * @brief Validate API key
 * @param api_key API key to validate
 * @return true if valid, false otherwise
 */
static bool validate_api_key(const char* api_key) {
  if (!api_key || strlen(api_key) == 0) {
    return false;
  }
  
  /* TODO: Implement real API key validation
   * For CP2, check against configured admin keys
   * Environment variable: GATEWAY_ADMIN_API_KEY
   * Or from config file
   */
  const char* expected_key = getenv("GATEWAY_ADMIN_API_KEY");
  if (expected_key && strcmp(api_key, expected_key) == 0) {
    return true;
  }
  
  /* Fallback: Check against default test key (for development only) */
  if (strcmp(api_key, "test_admin_key") == 0) {
    return true;
  }
  
  return false;
}

/**
 * @brief Get Gateway health status
 * @return 200 if healthy, 503 if unhealthy
 */
static int get_gateway_health_status(void) {
  /* Check critical components */
  
  /* 1. Check NATS connection */
  const char* nats_status = nats_get_status_string();
  if (!nats_status || strcmp(nats_status, "connected") != 0) {
    return 503;  /* NATS not connected */
  }
  
  /* 2. Check HTTP server (assumed healthy if function is called) */
  /* HTTP server is running if we can process requests */
  
  /* 3. Check memory (basic check) */
  /* TODO: Add memory check if needed */
  
  return 200;  /* All checks passed */
}

/**
 * @brief Get Gateway status string
 * @return Status string
 */
static const char* get_gateway_status_string(void) {
  static char status_buffer[256];
  
  const char* nats_status = nats_get_status_string();
  time_t now = time(NULL);
  
  snprintf(status_buffer, sizeof(status_buffer),
    "{\"status\":\"operational\",\"nats\":\"%s\",\"timestamp\":%ld}",
    nats_status ? nats_status : "unknown",
    (long)now
  );
  
  return status_buffer;
}

/**
 * @brief Record admin metric
 * @param metric_name Metric name (unused, kept for API compatibility)
 * @param success Whether operation was successful
 */
static void record_admin_metric(const char* metric_name, bool success) {
  (void)metric_name;  /* Parameter kept for API compatibility */
  admin_metrics_inc("gateway_admin_requests_total");
  if (!success) {
    admin_metrics_inc("gateway_admin_errors_total");
  }
}

