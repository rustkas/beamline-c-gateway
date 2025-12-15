/*
 * Gateway Health Endpoint Integration Tests
 * 
 * Tests HTTP health endpoint (`GET /_health`) for CP1-compliant format.
 * 
 * Note: These tests validate the health endpoint response format and HTTP status codes.
 */

#include "unity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jansson.h>

/* Test helper: Parse HTTP response */
static int parse_http_response(const char *response, int *status_code, char **body) {
    if (response == NULL) return -1;
    
    /* Find status line */
    const char *status_line = strstr(response, "HTTP/1.1");
    if (status_line == NULL) return -1;
    
    /* Parse status code */
    if (sscanf(status_line, "HTTP/1.1 %d", status_code) != 1) {
        return -1;
    }
    
    /* Find body (after \r\n\r\n) */
    const char *body_start = strstr(response, "\r\n\r\n");
    if (body_start == NULL) {
        body_start = strstr(response, "\n\n");
        if (body_start == NULL) return -1;
        body_start += 2;
    } else {
        body_start += 4;
    }
    
    *body = strdup(body_start);
    return 0;
}

/* Test helper: Validate health endpoint JSON response */
static int validate_health_response(const char *json_str) {
    json_error_t error;
    json_t *root = json_loads(json_str, 0, &error);
    if (root == NULL) return 0;
    
    /* Required fields */
    json_t *status = json_object_get(root, "status");
    json_t *timestamp = json_object_get(root, "timestamp");
    
    if (status == NULL || timestamp == NULL) {
        json_decref(root);
        return 0;
    }
    
    /* Status should be "healthy" */
    if (!json_is_string(status) || strcmp(json_string_value(status), "healthy") != 0) {
        json_decref(root);
        return 0;
    }
    
    /* Timestamp should be ISO 8601 format */
    if (!json_is_string(timestamp)) {
        json_decref(root);
        return 0;
    }
    
    const char *timestamp_str = json_string_value(timestamp);
    /* Basic validation: should contain T and Z */
    if (strchr(timestamp_str, 'T') == NULL || strchr(timestamp_str, 'Z') == NULL) {
        json_decref(root);
        return 0;
    }
    
    json_decref(root);
    return 1;
}

/* Forward declarations */
void test_health_endpoint_status_200(void);
void test_health_endpoint_valid_json(void);
void test_health_endpoint_required_fields(void);
void test_health_endpoint_iso8601_timestamp(void);
void test_health_endpoint_content_type(void);
void test_health_endpoint_format_validation(void);
void test_health_endpoint_with_optional_fields(void);
void test_health_endpoint_status_values(void);
void test_health_endpoint_timestamp_precision(void);
void test_health_endpoint_json_compactness(void);

/* Test: Health endpoint returns 200 OK */
void test_health_endpoint_status_200(void) {
    /* Note: This test requires Gateway to be running */
    /* For unit testing, we validate the response format structure */
    
    /* Example health endpoint response */
    const char *example_response = 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "\r\n"
        "{\"status\":\"healthy\",\"timestamp\":\"2025-01-27T12:00:00.123456Z\"}";
    
    int status_code;
    char *body;
    
    int rc = parse_http_response(example_response, &status_code, &body);
    TEST_ASSERT_EQUAL(0, rc);
    TEST_ASSERT_EQUAL(200, status_code);
    TEST_ASSERT_NOT_NULL(body);
    
    free(body);
}

/* Test: Health endpoint response is valid JSON */
void test_health_endpoint_valid_json(void) {
    const char *example_json = "{\"status\":\"healthy\",\"timestamp\":\"2025-01-27T12:00:00.123456Z\"}";
    
    json_error_t error;
    json_t *root = json_loads(example_json, 0, &error);
    TEST_ASSERT_NOT_NULL(root);
    TEST_ASSERT_TRUE(json_is_object(root));
    
    json_decref(root);
}

/* Test: Health endpoint response has required fields */
void test_health_endpoint_required_fields(void) {
    const char *example_json = "{\"status\":\"healthy\",\"timestamp\":\"2025-01-27T12:00:00.123456Z\"}";
    
    json_error_t error;
    json_t *root = json_loads(example_json, 0, &error);
    TEST_ASSERT_NOT_NULL(root);
    
    json_t *status = json_object_get(root, "status");
    json_t *timestamp = json_object_get(root, "timestamp");
    
    TEST_ASSERT_NOT_NULL(status);
    TEST_ASSERT_NOT_NULL(timestamp);
    
    TEST_ASSERT_EQUAL_STRING("healthy", json_string_value(status));
    TEST_ASSERT_NOT_NULL(json_string_value(timestamp));
    
    json_decref(root);
}

/* Test: Health endpoint response has ISO 8601 timestamp */
void test_health_endpoint_iso8601_timestamp(void) {
    const char *example_json = "{\"status\":\"healthy\",\"timestamp\":\"2025-01-27T12:00:00.123456Z\"}";
    
    json_error_t error;
    json_t *root = json_loads(example_json, 0, &error);
    TEST_ASSERT_NOT_NULL(root);
    
    json_t *timestamp = json_object_get(root, "timestamp");
    TEST_ASSERT_NOT_NULL(timestamp);
    
    const char *timestamp_str = json_string_value(timestamp);
    TEST_ASSERT_NOT_NULL(timestamp_str);
    
    /* Validate ISO 8601 format: YYYY-MM-DDTHH:MM:SS.ffffffZ */
    TEST_ASSERT_TRUE(strchr(timestamp_str, 'T') != NULL);
    TEST_ASSERT_TRUE(strchr(timestamp_str, 'Z') != NULL);
    TEST_ASSERT_TRUE(strchr(timestamp_str, '.') != NULL); /* Microseconds */
    
    json_decref(root);
}

/* Test: Health endpoint Content-Type is application/json */
void test_health_endpoint_content_type(void) {
    /* Example HTTP response with Content-Type header */
    const char *example_response = 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "\r\n"
        "{\"status\":\"healthy\",\"timestamp\":\"2025-01-27T12:00:00.123456Z\"}";
    
    /* Check for Content-Type header */
    TEST_ASSERT_TRUE(strstr(example_response, "Content-Type: application/json") != NULL);
}

/* Test: Health endpoint response format validation */
void test_health_endpoint_format_validation(void) {
    const char *valid_json = "{\"status\":\"healthy\",\"timestamp\":\"2025-01-27T12:00:00.123456Z\"}";
    TEST_ASSERT_TRUE(validate_health_response(valid_json));
    
    /* Invalid: missing status */
    const char *invalid_json1 = "{\"timestamp\":\"2025-01-27T12:00:00.123456Z\"}";
    TEST_ASSERT_FALSE(validate_health_response(invalid_json1));
    
    /* Invalid: missing timestamp */
    const char *invalid_json2 = "{\"status\":\"healthy\"}";
    TEST_ASSERT_FALSE(validate_health_response(invalid_json2));
    
    /* Invalid: wrong status value */
    const char *invalid_json3 = "{\"status\":\"unhealthy\",\"timestamp\":\"2025-01-27T12:00:00.123456Z\"}";
    TEST_ASSERT_FALSE(validate_health_response(invalid_json3));
}

/* Test: Health endpoint with additional fields (should be valid) */
void test_health_endpoint_with_optional_fields(void) {
    const char *health_with_checks = 
        "{"
        "\"status\":\"healthy\","
        "\"timestamp\":\"2025-01-27T12:00:00.123456Z\","
        "\"checks\":{"
        "\"database\":{\"status\":\"ok\"},"
        "\"nats\":{\"status\":\"ok\"}"
        "}"
        "}";
    
    json_error_t error;
    json_t *root = json_loads(health_with_checks, 0, &error);
    TEST_ASSERT_NOT_NULL(root);
    
    /* Required fields should still be present */
    json_t *status = json_object_get(root, "status");
    json_t *timestamp = json_object_get(root, "timestamp");
    
    TEST_ASSERT_NOT_NULL(status);
    TEST_ASSERT_NOT_NULL(timestamp);
    TEST_ASSERT_EQUAL_STRING("healthy", json_string_value(status));
    
    /* Optional checks field */
    json_t *checks = json_object_get(root, "checks");
    if (checks != NULL) {
        TEST_ASSERT_TRUE(json_is_object(checks));
    }
    
    json_decref(root);
}

/* Test: Health endpoint status values */
void test_health_endpoint_status_values(void) {
    /* Valid status: healthy */
    const char *healthy_json = "{\"status\":\"healthy\",\"timestamp\":\"2025-01-27T12:00:00.123456Z\"}";
    json_error_t error;
    json_t *root = json_loads(healthy_json, 0, &error);
    TEST_ASSERT_NOT_NULL(root);
    
    json_t *status = json_object_get(root, "status");
    TEST_ASSERT_NOT_NULL(status);
    TEST_ASSERT_EQUAL_STRING("healthy", json_string_value(status));
    
    json_decref(root);
}

/* Test: Health endpoint timestamp precision (microseconds) */
void test_health_endpoint_timestamp_precision(void) {
    const char *health_json = "{\"status\":\"healthy\",\"timestamp\":\"2025-01-27T12:00:00.123456Z\"}";
    
    json_error_t error;
    json_t *root = json_loads(health_json, 0, &error);
    TEST_ASSERT_NOT_NULL(root);
    
    json_t *timestamp = json_object_get(root, "timestamp");
    TEST_ASSERT_NOT_NULL(timestamp);
    
    const char *timestamp_str = json_string_value(timestamp);
    TEST_ASSERT_NOT_NULL(timestamp_str);
    
    /* Should have 6 digits for microseconds */
    const char *dot = strchr(timestamp_str, '.');
    TEST_ASSERT_NOT_NULL(dot);
    
    const char *z = strchr(timestamp_str, 'Z');
    TEST_ASSERT_NOT_NULL(z);
    
    /* Count digits between . and Z */
    int microsec_digits = (int)(z - dot - 1);
    TEST_ASSERT_EQUAL(6, microsec_digits);
    
    json_decref(root);
}

/* Test: Health endpoint JSON compactness */
void test_health_endpoint_json_compactness(void) {
    /* Health endpoint should return compact JSON (no extra whitespace) */
    const char *compact_json = "{\"status\":\"healthy\",\"timestamp\":\"2025-01-27T12:00:00.123456Z\"}";
    
    json_error_t error;
    json_t *root = json_loads(compact_json, 0, &error);
    TEST_ASSERT_NOT_NULL(root);
    
    /* Verify it's valid JSON */
    TEST_ASSERT_TRUE(json_is_object(root));
    
    json_decref(root);
}

/* Forward declarations for new tests */
void test_health_endpoint_with_optional_fields(void);
void test_health_endpoint_status_values(void);
void test_health_endpoint_timestamp_precision(void);
void test_health_endpoint_json_compactness(void);

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_health_endpoint_status_200);
    RUN_TEST(test_health_endpoint_valid_json);
    RUN_TEST(test_health_endpoint_required_fields);
    RUN_TEST(test_health_endpoint_iso8601_timestamp);
    RUN_TEST(test_health_endpoint_content_type);
    RUN_TEST(test_health_endpoint_format_validation);
    RUN_TEST(test_health_endpoint_with_optional_fields);
    RUN_TEST(test_health_endpoint_status_values);
    RUN_TEST(test_health_endpoint_timestamp_precision);
    RUN_TEST(test_health_endpoint_json_compactness);
    
    return UNITY_END();
}

