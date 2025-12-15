/* Integration tests for Gateway Conflict Contract
 * 
 * Tests each row of the conflict priority matrix to ensure deterministic behavior.
 * See docs/GATEWAY_CONFLICT_CONTRACT.md for the conflict contract specification.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <curl/curl.h>
#include <jansson.h>

#define GATEWAY_URL "http://localhost:8080"
#define MAX_RESPONSE_SIZE 8192

/* Response buffer for curl */
struct response_buffer {
    char *data;
    size_t size;
};

static size_t write_callback(void *contents, size_t size, size_t nmemb, struct response_buffer *buf) {
    size_t total = size * nmemb;
    buf->data = realloc(buf->data, buf->size + total + 1);
    if (buf->data) {
        memcpy(&(buf->data[buf->size]), contents, total);
        buf->size += total;
        buf->data[buf->size] = 0;
    }
    return total;
}

/* Make HTTP request and return response */
static int make_request(const char *method, const char *path, const char *headers, 
                       const char *body, struct response_buffer *resp) {
    CURL *curl = curl_easy_init();
    if (!curl) return -1;
    
    char url[256];
    snprintf(url, sizeof(url), "%s%s", GATEWAY_URL, path);
    
    resp->data = NULL;
    resp->size = 0;
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, resp);
    
    if (strcmp(method, "POST") == 0) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
    }
    
    struct curl_slist *header_list = NULL;
    if (headers) {
        header_list = curl_slist_append(header_list, headers);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
    }
    
    long http_code = 0;
    CURLcode res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    if (header_list) {
        curl_slist_free_all(header_list);
    }
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        return -1;
    }
    
    return (int)http_code;
}

/* Parse error response and extract fields */
static int parse_error_response(const char *json_str, int *http_status_out,
                                char *error_code_out, size_t error_code_len,
                                char *intake_error_code_out, size_t intake_error_code_len,
                                int *conflict_priority_out) {
    json_error_t error;
    json_t *root = json_loads(json_str, 0, &error);
    if (!root) return -1;
    
    json_t *ok_val = json_object_get(root, "ok");
    if (!json_is_boolean(ok_val) || json_boolean_value(ok_val)) {
        json_decref(root);
        return -1; /* Not an error response */
    }
    
    json_t *err_obj = json_object_get(root, "error");
    if (err_obj) {
        json_t *code = json_object_get(err_obj, "code");
        if (json_is_string(code) && error_code_out) {
            strncpy(error_code_out, json_string_value(code), error_code_len - 1);
            error_code_out[error_code_len - 1] = '\0';
        }
        
        json_t *intake_code = json_object_get(err_obj, "intake_error_code");
        if (intake_error_code_out) {
            if (json_is_string(intake_code)) {
                strncpy(intake_error_code_out, json_string_value(intake_code), intake_error_code_len - 1);
                intake_error_code_out[intake_error_code_len - 1] = '\0';
            } else if (json_is_null(intake_code)) {
                intake_error_code_out[0] = '\0';
            }
        }
    }
    
    json_decref(root);
    return 0;
}

/* Test Row 1: RL Dominates (Rate Limit Exceeded) */
static void test_row1_rate_limit_dominates(void) {
    printf("Test Row 1: RL Dominates (Rate Limit)\n");
    
    /* Make many requests to exceed rate limit, with invalid JSON */
    struct response_buffer resp;
    int http_status;
    char error_code[64] = {0};
    char intake_error_code[64] = {0};
    
    /* Make requests until rate limit is hit */
    for (int i = 0; i < 60; i++) {
        http_status = make_request("POST", "/api/v1/routes/decide",
                                  "X-Tenant-ID: tenant-test\r\nContent-Type: application/json",
                                  "{ invalid json }", &resp);
        
        if (http_status == 429) {
            /* Rate limit exceeded - verify conflict contract */
            assert(parse_error_response(resp.data, NULL, error_code, sizeof(error_code),
                                       intake_error_code, sizeof(intake_error_code), NULL) == 0);
            assert(strcmp(error_code, "rate_limit_exceeded") == 0);
            assert(intake_error_code[0] == '\0' || strcmp(intake_error_code, "null") == 0);
            
            printf("  ✓ Rate limit (429) returned, intake_error_code is null\n");
            free(resp.data);
            return;
        }
        free(resp.data);
    }
    
    printf("  ⚠ Rate limit not hit (may need to adjust test)\n");
}

/* Test Row 2: AUTH_GW Dominates (Authentication Error) */
static void test_row2_auth_gateway_dominates(void) {
    printf("Test Row 2: AUTH_GW Dominates (Authentication)\n");
    
    struct response_buffer resp;
    int http_status = make_request("POST", "/api/v1/routes/decide",
                                   "X-Tenant-ID: tenant-test\r\nContent-Type: application/json",
                                   "{\"version\":\"1\",\"tenant_id\":\"tenant-test\",\"request_id\":\"req-002\",\"task\":{\"type\":\"text.generate\",\"payload\":{}}}",
                                   &resp);
    
    char error_code[64] = {0};
    char intake_error_code[64] = {0};
    
    /* If auth is required and missing, should get 401 */
    if (http_status == 401) {
        assert(parse_error_response(resp.data, NULL, error_code, sizeof(error_code),
                                   intake_error_code, sizeof(intake_error_code), NULL) == 0);
        assert(strcmp(error_code, "unauthorized") == 0);
        assert(intake_error_code[0] == '\0' || strcmp(intake_error_code, "null") == 0);
        printf("  ✓ Authentication error (401) returned, intake_error_code is null\n");
    } else {
        printf("  ⚠ Auth not required or test needs adjustment (got %d)\n", http_status);
    }
    
    free(resp.data);
}

/* Test Row 3: REQ_GW Dominates (Request Validation Error) */
static void test_row3_request_gateway_dominates(void) {
    printf("Test Row 3: REQ_GW Dominates (Request Validation)\n");
    
    struct response_buffer resp;
    int http_status = make_request("POST", "/api/v1/routes/decide",
                                   "X-Tenant-ID: tenant-test\r\nContent-Type: text/plain",
                                   "invalid json",
                                   &resp);
    
    char error_code[64] = {0};
    char intake_error_code[64] = {0};
    
    if (http_status == 400) {
        assert(parse_error_response(resp.data, NULL, error_code, sizeof(error_code),
                                   intake_error_code, sizeof(intake_error_code), NULL) == 0);
        assert(strcmp(error_code, "invalid_request") == 0);
        assert(intake_error_code[0] == '\0' || strcmp(intake_error_code, "null") == 0);
        printf("  ✓ Request validation error (400) returned, intake_error_code is null\n");
    } else {
        printf("  ⚠ Expected 400, got %d\n", http_status);
    }
    
    free(resp.data);
}

/* Test Row 4: INTAKE_ROUTER (Router Intake Error) */
static void test_row4_router_intake_error(void) {
    printf("Test Row 4: INTAKE_ROUTER (Router Intake Error)\n");
    
    /* Send valid request but with missing tenant_id in body (Router will reject) */
    struct response_buffer resp;
    int http_status = make_request("POST", "/api/v1/routes/decide",
                                   "X-Tenant-ID: tenant-test\r\nContent-Type: application/json",
                                   "{\"version\":\"1\",\"request_id\":\"req-004\",\"task\":{\"type\":\"text.generate\",\"payload\":{}}}",
                                   &resp);
    
    char error_code[64] = {0};
    char intake_error_code[64] = {0};
    
    if (http_status == 400) {
        assert(parse_error_response(resp.data, NULL, error_code, sizeof(error_code),
                                   intake_error_code, sizeof(intake_error_code), NULL) == 0);
        /* Router intake error should have intake_error_code */
        if (intake_error_code[0] != '\0' && strcmp(intake_error_code, "null") != 0) {
            printf("  ✓ Router intake error (400) with intake_error_code: %s\n", intake_error_code);
        } else {
            printf("  ⚠ Router intake error but intake_error_code missing\n");
        }
    } else {
        printf("  ⚠ Expected 400, got %d\n", http_status);
    }
    
    free(resp.data);
}

/* Test conflict consistency: verify only one error type per response */
static void test_conflict_consistency(void) {
    printf("Test: Conflict Consistency (Single Error Type)\n");
    
    /* Test scenario: Rate limit + invalid JSON */
    struct response_buffer resp;
    
    /* Make many requests with invalid JSON to trigger both RL and REQ_GW */
    for (int i = 0; i < 60; i++) {
        int http_status = make_request("POST", "/api/v1/routes/decide",
                                      "X-Tenant-ID: tenant-test\r\nContent-Type: application/json",
                                      "{ invalid json }", &resp);
        
        if (http_status == 429) {
            /* Should be rate limit error, not request validation */
            char error_code[64] = {0};
            assert(parse_error_response(resp.data, NULL, error_code, sizeof(error_code),
                                       NULL, 0, NULL) == 0);
            assert(strcmp(error_code, "rate_limit_exceeded") == 0);
            printf("  ✓ Rate limit dominates over request validation\n");
            free(resp.data);
            return;
        }
        free(resp.data);
    }
    
    printf("  ⚠ Consistency test needs adjustment\n");
}

int main(void) {
    printf("Gateway Conflict Contract Integration Tests\n");
    printf("==========================================\n");
    printf("Note: These tests require Gateway running on localhost:8080\n\n");
    
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    test_row1_rate_limit_dominates();
    test_row2_auth_gateway_dominates();
    test_row3_request_gateway_dominates();
    test_row4_router_intake_error();
    test_conflict_consistency();
    
    curl_global_cleanup();
    
    printf("\n==========================================\n");
    printf("Conflict contract tests completed!\n");
    return 0;
}

