/*
 * Gateway Rate Limiting Integration Tests
 * 
 * Tests rate limiting behavior via HTTP endpoints:
 * - Under-limit requests (should succeed)
 * - At-limit requests (should succeed)
 * - Over-limit requests (should return 429)
 * - Window reset (after TTL expiration)
 * - Multi-endpoint isolation (different endpoints have separate limits)
 * - Rate limit headers (X-RateLimit-*, Retry-After)
 * - Error response format
 * 
 * Note: These tests require Gateway to be running on localhost:8080
 * Start Gateway with: cd apps/c-gateway && make run
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <jansson.h>

/* Test helper: Send HTTP request and capture response */
static int send_http_request(const char *host, int port, const char *request, 
                             char *resp_buf, size_t resp_buf_size) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }

    size_t req_len = strlen(request);
    if (write(sock, request, req_len) != (ssize_t)req_len) {
        close(sock);
        return -1;
    }

    ssize_t n = read(sock, resp_buf, resp_buf_size - 1);
    if (n <= 0) {
        close(sock);
        return -1;
    }

    resp_buf[n] = '\0';
    close(sock);
    return 0;
}

/* Test helper: Extract HTTP status code from response */
static int extract_http_status(const char *response) {
    if (response == NULL) return -1;
    
    /* Look for "HTTP/1.1 XXX" pattern */
    const char *status_start = strstr(response, "HTTP/1.1 ");
    if (status_start == NULL) return -1;
    
    status_start += 9; /* Skip "HTTP/1.1 " */
    return atoi(status_start);
}

/* Test helper: Extract rate limit headers from response */
static int extract_rate_limit_header(const char *response, const char *header_name, int *value) {
    if (response == NULL || header_name == NULL || value == NULL) return -1;
    
    char search_pattern[128];
    snprintf(search_pattern, sizeof(search_pattern), "%s: ", header_name);
    
    const char *header_start = strstr(response, search_pattern);
    if (header_start == NULL) return -1;
    
    header_start += strlen(search_pattern);
    *value = atoi(header_start);
    return 0;
}

/* Test helper: Parse JSON error response */
static json_t *parse_error_response(const char *response) {
    if (response == NULL) return NULL;
    
    /* Find JSON body (after \r\n\r\n) */
    const char *body_start = strstr(response, "\r\n\r\n");
    if (body_start == NULL) return NULL;
    
    body_start += 4; /* Skip \r\n\r\n */
    
    json_error_t error;
    json_t *root = json_loads(body_start, 0, &error);
    return root;
}

/* Test: Under-limit requests should succeed */
static void test_rate_limit_under_limit(void) {
    /* Set low limit for testing */
    setenv("GATEWAY_RATE_LIMIT_ROUTES_DECIDE_LIMIT", "5", 1);
    setenv("GATEWAY_RATE_LIMIT_TTL_SECONDS", "60", 1);
    
    const char *req_template =
        "POST /api/v1/routes/decide HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Type: application/json\r\n"
        "X-Tenant-ID: tenant-rl-test\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s";
    
    const char *body = "{\"version\":\"1\",\"tenant_id\":\"tenant-rl-test\",\"request_id\":\"req-001\",\"task\":{\"type\":\"text.generate\",\"payload\":{}}}";
    char request[1024];
    snprintf(request, sizeof(request), req_template, strlen(body), body);
    
    char resp[4096];
    
    /* Send 3 requests (under limit of 5) */
    for (int i = 0; i < 3; i++) {
        int rc = send_http_request("127.0.0.1", 8080, request, resp, sizeof(resp));
        assert(rc == 0);
        
        int status = extract_http_status(resp);
        /* May be 200 (success) or 503 (Router unavailable), but NOT 429 */
        assert(status != 429);
    }
}

/* Test: At-limit requests should return 429 */
static void test_rate_limit_at_limit(void) {
    /* Set very low limit for testing */
    setenv("GATEWAY_RATE_LIMIT_ROUTES_DECIDE_LIMIT", "2", 1);
    setenv("GATEWAY_RATE_LIMIT_TTL_SECONDS", "60", 1);
    
    const char *req_template =
        "POST /api/v1/routes/decide HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Type: application/json\r\n"
        "X-Tenant-ID: tenant-rl-test-2\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s";
    
    const char *body = "{\"version\":\"1\",\"tenant_id\":\"tenant-rl-test-2\",\"request_id\":\"req-002\",\"task\":{\"type\":\"text.generate\",\"payload\":{}}}";
    char request[1024];
    snprintf(request, sizeof(request), req_template, strlen(body), body);
    
    char resp[4096];
    
    /* Send 2 requests (at limit) - should succeed */
    for (int i = 0; i < 2; i++) {
        int rc = send_http_request("127.0.0.1", 8080, request, resp, sizeof(resp));
        assert(rc == 0);
        
        int status = extract_http_status(resp);
        /* First 2 requests should NOT be 429 */
        assert(status != 429);
    }
    
    /* Third request (exceeds limit) - should return 429 */
    int rc = send_http_request("127.0.0.1", 8080, request, resp, sizeof(resp));
    assert(rc == 0);
    
    int status = extract_http_status(resp);
    assert(status == 429);
    
    /* Verify error response format */
    json_t *error_resp = parse_error_response(resp);
    assert(error_resp != NULL);
    
    json_t *ok_val = json_object_get(error_resp, "ok");
    assert(ok_val != NULL);
    assert(json_is_boolean(ok_val) && !json_boolean_value(ok_val));
    
    json_t *error_obj = json_object_get(error_resp, "error");
    assert(error_obj != NULL);
    
    json_t *error_code = json_object_get(error_obj, "code");
    assert(error_code != NULL);
    assert(strcmp(json_string_value(error_code), "rate_limit_exceeded") == 0);
    
    json_decref(error_resp);
}

/* Test: Rate limit headers in 429 response */
static void test_rate_limit_headers(void) {
    /* Set very low limit for testing */
    setenv("GATEWAY_RATE_LIMIT_ROUTES_DECIDE_LIMIT", "1", 1);
    setenv("GATEWAY_RATE_LIMIT_TTL_SECONDS", "60", 1);
    
    const char *req_template =
        "POST /api/v1/routes/decide HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Type: application/json\r\n"
        "X-Tenant-ID: tenant-rl-test-3\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s";
    
    const char *body = "{\"version\":\"1\",\"tenant_id\":\"tenant-rl-test-3\",\"request_id\":\"req-003\",\"task\":{\"type\":\"text.generate\",\"payload\":{}}}";
    char request[1024];
    snprintf(request, sizeof(request), req_template, strlen(body), body);
    
    char resp[4096];
    
    /* First request - should succeed */
    int rc = send_http_request("127.0.0.1", 8080, request, resp, sizeof(resp));
    assert(rc == 0);
    
    /* Second request - should return 429 with headers */
    rc = send_http_request("127.0.0.1", 8080, request, resp, sizeof(resp));
    assert(rc == 0);
    
    int status = extract_http_status(resp);
    assert(status == 429);
    
    /* Verify rate limit headers */
    int limit = -1, remaining = -1, reset = -1, retry_after = -1;
    
    assert(extract_rate_limit_header(resp, "X-RateLimit-Limit", &limit) == 0);
    assert(limit > 0);
    
    assert(extract_rate_limit_header(resp, "X-RateLimit-Remaining", &remaining) == 0);
    assert(remaining == 0); /* Should be 0 when limit exceeded */
    
    assert(extract_rate_limit_header(resp, "X-RateLimit-Reset", &reset) == 0);
    assert(reset > 0); /* Should be future timestamp */
    
    assert(extract_rate_limit_header(resp, "Retry-After", &retry_after) == 0);
    assert(retry_after >= 0); /* Should be non-negative */
}

/* Test: Window reset after TTL */
static void test_rate_limit_reset(void) {
    /* Set very short TTL for testing */
    setenv("GATEWAY_RATE_LIMIT_ROUTES_DECIDE_LIMIT", "2", 1);
    setenv("GATEWAY_RATE_LIMIT_TTL_SECONDS", "2", 1); /* 2 seconds window */
    
    const char *req_template =
        "POST /api/v1/routes/decide HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Type: application/json\r\n"
        "X-Tenant-ID: tenant-rl-test-4\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s";
    
    const char *body = "{\"version\":\"1\",\"tenant_id\":\"tenant-rl-test-4\",\"request_id\":\"req-004\",\"task\":{\"type\":\"text.generate\",\"payload\":{}}}";
    char request[1024];
    snprintf(request, sizeof(request), req_template, strlen(body), body);
    
    char resp[4096];
    
    /* Exhaust limit */
    for (int i = 0; i < 2; i++) {
        send_http_request("127.0.0.1", 8080, request, resp, sizeof(resp));
    }
    
    /* Third request should be 429 */
    int rc = send_http_request("127.0.0.1", 8080, request, resp, sizeof(resp));
    assert(rc == 0);
    int status = extract_http_status(resp);
    assert(status == 429);
    
    /* Wait for window to reset (2 seconds + small buffer) */
    sleep(3);
    
    /* After reset, request should succeed */
    rc = send_http_request("127.0.0.1", 8080, request, resp, sizeof(resp));
    assert(rc == 0);
    status = extract_http_status(resp);
    assert(status != 429); /* Should NOT be 429 after reset */
}

/* Test: Multi-endpoint isolation (different endpoints have separate limits) */
static void test_rate_limit_multi_endpoint_isolation(void) {
    /* Set different limits for different endpoints */
    setenv("GATEWAY_RATE_LIMIT_ROUTES_DECIDE_LIMIT", "2", 1);
    setenv("GATEWAY_RATE_LIMIT_MESSAGES", "3", 1);
    setenv("GATEWAY_RATE_LIMIT_TTL_SECONDS", "60", 1);
    
    const char *decide_req_template =
        "POST /api/v1/routes/decide HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Type: application/json\r\n"
        "X-Tenant-ID: tenant-rl-test-5\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s";
    
    const char *messages_req_template =
        "POST /api/v1/messages HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Type: application/json\r\n"
        "X-Tenant-ID: tenant-rl-test-5\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s";
    
    const char *decide_body = "{\"version\":\"1\",\"tenant_id\":\"tenant-rl-test-5\",\"request_id\":\"req-005\",\"task\":{\"type\":\"text.generate\",\"payload\":{}}}";
    const char *messages_body = "{\"message_id\":\"msg-005\",\"message_type\":\"chat\",\"payload\":{}}";
    
    char decide_req[1024], messages_req[1024];
    snprintf(decide_req, sizeof(decide_req), decide_req_template, strlen(decide_body), decide_body);
    snprintf(messages_req, sizeof(messages_req), messages_req_template, strlen(messages_body), messages_body);
    
    char resp[4096];
    
    /* Exhaust /api/v1/routes/decide limit (2 requests) */
    for (int i = 0; i < 2; i++) {
        send_http_request("127.0.0.1", 8080, decide_req, resp, sizeof(resp));
    }
    
    /* Third decide request should be 429 */
    int rc = send_http_request("127.0.0.1", 8080, decide_req, resp, sizeof(resp));
    assert(rc == 0);
    int status = extract_http_status(resp);
    assert(status == 429);
    
    /* But /api/v1/messages should still work (different endpoint, limit 3) */
    rc = send_http_request("127.0.0.1", 8080, messages_req, resp, sizeof(resp));
    assert(rc == 0);
    status = extract_http_status(resp);
    assert(status != 429); /* Should NOT be 429 (different endpoint) */
}

/* Test: Error response format for rate limit exceeded */
static void test_rate_limit_error_response_format(void) {
    /* Set very low limit */
    setenv("GATEWAY_RATE_LIMIT_ROUTES_DECIDE_LIMIT", "1", 1);
    setenv("GATEWAY_RATE_LIMIT_TTL_SECONDS", "60", 1);
    
    const char *req_template =
        "POST /api/v1/routes/decide HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Type: application/json\r\n"
        "X-Tenant-ID: tenant-rl-test-6\r\n"
        "X-Trace-ID: trace-rl-test-6\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s";
    
    const char *body = "{\"version\":\"1\",\"tenant_id\":\"tenant-rl-test-6\",\"request_id\":\"req-006\",\"task\":{\"type\":\"text.generate\",\"payload\":{}}}";
    char request[1024];
    snprintf(request, sizeof(request), req_template, strlen(body), body);
    
    char resp[4096];
    
    /* First request - should succeed */
    send_http_request("127.0.0.1", 8080, request, resp, sizeof(resp));
    
    /* Second request - should return 429 */
    int rc = send_http_request("127.0.0.1", 8080, request, resp, sizeof(resp));
    assert(rc == 0);
    
    json_t *error_resp = parse_error_response(resp);
    assert(error_resp != NULL);
    
    /* Verify error structure */
    json_t *ok_val = json_object_get(error_resp, "ok");
    assert(ok_val != NULL);
    assert(json_is_boolean(ok_val) && !json_boolean_value(ok_val));
    
    json_t *error_obj = json_object_get(error_resp, "error");
    assert(error_obj != NULL);
    
    json_t *error_code = json_object_get(error_obj, "code");
    assert(error_code != NULL);
    assert(strcmp(json_string_value(error_code), "rate_limit_exceeded") == 0);
    
    json_t *error_details = json_object_get(error_obj, "details");
    assert(error_details != NULL);
    
    json_t *endpoint = json_object_get(error_details, "endpoint");
    assert(endpoint != NULL);
    assert(strcmp(json_string_value(endpoint), "/api/v1/routes/decide") == 0);
    
    json_t *limit = json_object_get(error_details, "limit");
    assert(limit != NULL);
    assert(json_is_integer(limit));
    
    json_t *retry_after = json_object_get(error_details, "retry_after_seconds");
    assert(retry_after != NULL);
    assert(json_is_integer(retry_after));
    
    /* Verify context */
    json_t *context = json_object_get(error_resp, "context");
    assert(context != NULL);
    
    json_t *request_id = json_object_get(context, "request_id");
    assert(request_id != NULL);
    
    json_t *trace_id = json_object_get(context, "trace_id");
    assert(trace_id != NULL);
    assert(strcmp(json_string_value(trace_id), "trace-rl-test-6") == 0);
    
    json_t *tenant_id = json_object_get(context, "tenant_id");
    assert(tenant_id != NULL);
    assert(strcmp(json_string_value(tenant_id), "tenant-rl-test-6") == 0);
    
    json_decref(error_resp);
}

int main(void) {
    printf("\n=== Gateway Rate Limiting Integration Tests ===\n");
    printf("Note: These tests require Gateway to be running on localhost:8080\n");
    printf("Start Gateway with: cd apps/c-gateway && make run\n\n");
    
    /* Set environment variables for rate limiting configuration */
    setenv("GATEWAY_RATE_LIMIT_ROUTES_DECIDE_LIMIT", "5", 1);
    setenv("GATEWAY_RATE_LIMIT_MESSAGES", "3", 1);
    setenv("GATEWAY_RATE_LIMIT_TTL_SECONDS", "60", 1);
    
    test_rate_limit_under_limit();
    printf("✓ test_rate_limit_under_limit passed\n");
    
    test_rate_limit_at_limit();
    printf("✓ test_rate_limit_at_limit passed\n");
    
    test_rate_limit_headers();
    printf("✓ test_rate_limit_headers passed\n");
    
    test_rate_limit_reset();
    printf("✓ test_rate_limit_reset passed\n");
    
    test_rate_limit_multi_endpoint_isolation();
    printf("✓ test_rate_limit_multi_endpoint_isolation passed\n");
    
    test_rate_limit_error_response_format();
    printf("✓ test_rate_limit_error_response_format passed\n");
    
    printf("\n=== All Rate Limiting Tests Passed ===\n");
    return 0;
}

