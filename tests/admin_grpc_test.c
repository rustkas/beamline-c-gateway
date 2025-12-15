#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "../src/admin_grpc.h"
#include "../src/nats_client_stub.h"
#include "../src/metrics/prometheus.h"

/* Forward declarations for mock functions */
static const char *mock_nats_get_status_string(void);
static prometheus_counter_t* mock_prometheus_counter_create(const char *name, const char *help);
static void mock_prometheus_counter_inc(prometheus_counter_t *counter);

/* Mock NATS status function */
static const char *mock_nats_get_status_string(void) {
  return "connected";
}

/* Override nats_get_status_string from nats_client_stub.h */
const char *nats_get_status_string(void) {
  return mock_nats_get_status_string();
}

/* Mock prometheus functions - use real type but simple implementation */
static prometheus_counter_t* mock_prometheus_counter_create(const char *name, const char *help) {
  (void)name;
  (void)help;
  static prometheus_counter_t counter_storage;
  static bool initialized = false;
  if (!initialized) {
    memset(&counter_storage, 0, sizeof(counter_storage));
    initialized = true;
  }
  return &counter_storage;
}

/* Override prometheus_counter_create */
prometheus_counter_t* prometheus_counter_create(const char *name, const char *help) {
  return mock_prometheus_counter_create(name, help);
}

static void mock_prometheus_counter_inc(prometheus_counter_t *counter) {
  (void)counter;
  /* Mock implementation - do nothing */
}

/* Override prometheus_counter_inc */
void prometheus_counter_inc(prometheus_counter_t *counter) {
  mock_prometheus_counter_inc(counter);
}

int main(void) {
  /* Test 1: Initialization */
  admin_resp_t r1 = admin_init();
  assert(r1.code == 0);
  assert(r1.message == NULL);
  
  /* Test 2: Health check (positive) */
  admin_resp_t h = admin_health();
  assert(h.code == 200);
  assert(h.message != NULL);
  assert(strcmp(h.message, "healthy") == 0);
  
  /* Test 3: Status check (positive) */
  admin_resp_t s = admin_status();
  assert(s.code == 200);
  assert(s.message != NULL);
  assert(strstr(s.message, "status") != NULL);
  
  /* Test 4: Authorization (positive - admin role) */
  setenv("GATEWAY_ADMIN_API_KEY", "test_key_123", 1);
  assert(admin_authorize("test_key_123", "admin", "restart") == true);
  assert(admin_authorize("test_key_123", "admin", "config") == true);
  assert(admin_authorize("test_key_123", "admin", "metrics") == true);
  
  /* Test 5: Authorization (positive - operator role) */
  assert(admin_authorize("test_key_123", "operator", "health") == true);
  assert(admin_authorize("test_key_123", "operator", "status") == true);
  assert(admin_authorize("test_key_123", "operator", "metrics") == true);
  assert(admin_authorize("test_key_123", "operator", "restart") == false);  /* Operator cannot restart */
  
  /* Test 6: Authorization (positive - viewer role) */
  assert(admin_authorize("test_key_123", "viewer", "metrics") == true);
  assert(admin_authorize("test_key_123", "viewer", "health") == false);  /* Viewer cannot access health */
  assert(admin_authorize("test_key_123", "viewer", "restart") == false);  /* Viewer cannot restart */
  
  /* Test 7: Authorization (negative - invalid API key) */
  assert(admin_authorize("invalid_key", "admin", "restart") == false);
  assert(admin_authorize(NULL, "admin", "restart") == false);
  assert(admin_authorize("test_key_123", NULL, "restart") == false);
  assert(admin_authorize("test_key_123", "admin", NULL) == false);
  
  /* Test 8: Authorization (negative - unknown tenant) */
  /* Note: Tenant validation would be added in production */
  
  /* Test 9: Metrics increment */
  admin_metrics_inc("gateway_admin_requests_total");
  admin_metrics_inc("gateway_admin_errors_total");
  admin_metrics_inc(NULL);  /* Should not crash */
  
  /* Test 10: Get metrics */
  char metrics_buffer[1024];
  int written = admin_get_metrics(metrics_buffer, sizeof(metrics_buffer));
  assert(written > 0);
  assert(strstr(metrics_buffer, "gateway_admin_requests_total") != NULL);
  
  /* Test 11: Get metrics with invalid buffer */
  assert(admin_get_metrics(NULL, 0) == -1);
  assert(admin_get_metrics(metrics_buffer, 0) == -1);
  
  /* Test 12: Trace context */
  admin_ctx_t c = admin_trace_ctx("trace_123", "span_456", "tenant_789");
  assert(c.trace_id != NULL);
  assert(strcmp(c.trace_id, "trace_123") == 0);
  assert(c.span_id != NULL);
  assert(strcmp(c.span_id, "span_456") == 0);
  assert(c.tenant_id != NULL);
  assert(strcmp(c.tenant_id, "tenant_789") == 0);
  
  /* Test 13: Trace context with NULL values */
  admin_ctx_t c2 = admin_trace_ctx(NULL, NULL, NULL);
  assert(c2.trace_id != NULL);
  assert(strlen(c2.trace_id) == 0);
  assert(c2.span_id != NULL);
  assert(strlen(c2.span_id) == 0);
  assert(c2.tenant_id != NULL);
  assert(strlen(c2.tenant_id) == 0);
  
  /* Test 14: Deterministic behavior - same inputs produce same outputs */
  admin_resp_t h1 = admin_health();
  admin_resp_t h2 = admin_health();
  assert(h1.code == h2.code);
  assert(strcmp(h1.message, h2.message) == 0);
  
  /* Test 15: CI-friendly - no random values, no time-dependent failures */
  /* All tests use deterministic inputs and check deterministic outputs */
  
  unsetenv("GATEWAY_ADMIN_API_KEY");
  
  return 0;
}

