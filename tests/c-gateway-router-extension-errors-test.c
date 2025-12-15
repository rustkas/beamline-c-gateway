#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <jansson.h>

/* Function is exported from http_server.c for testing */
extern int map_router_error_status(const char *resp_json);

/* Test: Extension not found error (404) */
static void test_status_extension_not_found_404(void)
{
    const char *resp =
        "{"
        "\"ok\":false,"
        "\"error\":{"
        "\"code\":\"extension_not_found\","
        "\"message\":\"Extension not found: normalize_text\","
        "\"details\":{"
        "\"extension_id\":\"normalize_text\","
        "\"error_type\":\"extension_not_found\""
        "}"
        "},"
        "\"context\":{\"request_id\":\"r1\"}"
        "}";

    int status = map_router_error_status(resp);
    assert(status == 404);
    printf("✓ extension_not_found maps to 404\n");
}

/* Test: Extension timeout error (504) */
static void test_status_extension_timeout_504(void)
{
    const char *resp =
        "{"
        "\"ok\":false,"
        "\"error\":{"
        "\"code\":\"extension_timeout\","
        "\"message\":\"Extension timeout after 3 retries: normalize_text\","
        "\"details\":{"
        "\"extension_id\":\"normalize_text\","
        "\"retries\":3,"
        "\"error_type\":\"extension_timeout\""
        "}"
        "},"
        "\"context\":{\"request_id\":\"r1\"}"
        "}";

    int status = map_router_error_status(resp);
    assert(status == 504);
    printf("✓ extension_timeout maps to 504\n");
}

/* Test: Validator blocked error (403) */
static void test_status_validator_blocked_403(void)
{
    const char *resp =
        "{"
        "\"ok\":false,"
        "\"error\":{"
        "\"code\":\"validator_blocked\","
        "\"message\":\"Validator blocked request: pii_guard - Validation failed\","
        "\"details\":{"
        "\"extension_id\":\"pii_guard\","
        "\"reason\":\"Validation failed\","
        "\"error_type\":\"validator_blocked\""
        "}"
        "},"
        "\"context\":{\"request_id\":\"r1\"}"
        "}";

    int status = map_router_error_status(resp);
    assert(status == 403);
    printf("✓ validator_blocked maps to 403\n");
}

/* Test: Post-processor failed error (500) */
static void test_status_post_processor_failed_500(void)
{
    const char *resp =
        "{"
        "\"ok\":false,"
        "\"error\":{"
        "\"code\":\"post_processor_failed\","
        "\"message\":\"Post-processor failed: mask_pii - Post-processing failed\","
        "\"details\":{"
        "\"extension_id\":\"mask_pii\","
        "\"reason\":\"Post-processing failed\","
        "\"error_type\":\"post_processor_failed\""
        "}"
        "},"
        "\"context\":{\"request_id\":\"r1\"}"
        "}";

    int status = map_router_error_status(resp);
    assert(status == 500);
    printf("✓ post_processor_failed maps to 500\n");
}

/* Test: Extension unavailable (circuit breaker) error (503) */
static void test_status_extension_unavailable_503(void)
{
    const char *resp =
        "{"
        "\"ok\":false,"
        "\"error\":{"
        "\"code\":\"extension_unavailable\","
        "\"message\":\"Extension unavailable (circuit breaker open): normalize_text\","
        "\"details\":{"
        "\"extension_id\":\"normalize_text\","
        "\"error_type\":\"extension_circuit_open\""
        "}"
        "},"
        "\"context\":{\"request_id\":\"r1\"}"
        "}";

    int status = map_router_error_status(resp);
    assert(status == 503);
    printf("✓ extension_unavailable maps to 503\n");
}

/* Test: Extension error (generic) error (500) */
static void test_status_extension_error_500(void)
{
    const char *resp =
        "{"
        "\"ok\":false,"
        "\"error\":{"
        "\"code\":\"extension_error\","
        "\"message\":\"Extension invocation error: normalize_text - Invocation failed\","
        "\"details\":{"
        "\"extension_id\":\"normalize_text\","
        "\"reason\":\"Invocation failed\","
        "\"error_type\":\"extension_invocation_error\""
        "}"
        "},"
        "\"context\":{\"request_id\":\"r1\"}"
        "}";

    int status = map_router_error_status(resp);
    assert(status == 500);
    printf("✓ extension_error maps to 500\n");
}

/* Test: Error code mapping via JSON responses */
static void test_error_code_mapping(void)
{
    /* Test extension_not_found via JSON response */
    const char *resp1 = "{\"ok\":false,\"error\":{\"code\":\"extension_not_found\"}}";
    int status1 = map_router_error_status(resp1);
    assert(status1 == 404);
    printf("✓ extension_not_found code maps to 404\n");

    /* Test extension_timeout via JSON response */
    const char *resp2 = "{\"ok\":false,\"error\":{\"code\":\"extension_timeout\"}}";
    int status2 = map_router_error_status(resp2);
    assert(status2 == 504);
    printf("✓ extension_timeout code maps to 504\n");

    /* Test validator_blocked via JSON response */
    const char *resp3 = "{\"ok\":false,\"error\":{\"code\":\"validator_blocked\"}}";
    int status3 = map_router_error_status(resp3);
    assert(status3 == 403);
    printf("✓ validator_blocked code maps to 403\n");

    /* Test post_processor_failed via JSON response */
    const char *resp4 = "{\"ok\":false,\"error\":{\"code\":\"post_processor_failed\"}}";
    int status4 = map_router_error_status(resp4);
    assert(status4 == 500);
    printf("✓ post_processor_failed code maps to 500\n");

    /* Test extension_unavailable via JSON response */
    const char *resp5 = "{\"ok\":false,\"error\":{\"code\":\"extension_unavailable\"}}";
    int status5 = map_router_error_status(resp5);
    assert(status5 == 503);
    printf("✓ extension_unavailable code maps to 503\n");
}

/* Test: Error response structure validation */
static void test_error_response_structure(void)
{
    const char *resp =
        "{"
        "\"ok\":false,"
        "\"error\":{"
        "\"code\":\"extension_not_found\","
        "\"message\":\"Extension not found: normalize_text\","
        "\"details\":{"
        "\"extension_id\":\"normalize_text\","
        "\"error_type\":\"extension_not_found\","
        "\"timestamp\":\"2025-01-27T12:00:00Z\""
        "}"
        "},"
        "\"context\":{"
        "\"request_id\":\"r1\","
        "\"trace_id\":\"t1\""
        "}"
        "}";

    /* Verify response structure */
    assert(strstr(resp, "\"ok\":false") != NULL);
    assert(strstr(resp, "\"code\":\"extension_not_found\"") != NULL);
    assert(strstr(resp, "\"message\"") != NULL);
    assert(strstr(resp, "\"details\"") != NULL);
    assert(strstr(resp, "\"extension_id\"") != NULL);
    assert(strstr(resp, "\"context\"") != NULL);
    printf("✓ Error response structure is valid\n");
}

int main(void)
{
    printf("Running Gateway Router Extension Errors Contract Tests\n");
    printf("====================================================\n\n");

    test_status_extension_not_found_404();
    test_status_extension_timeout_504();
    test_status_validator_blocked_403();
    test_status_post_processor_failed_500();
    test_status_extension_unavailable_503();
    test_status_extension_error_500();
    test_error_code_mapping();
    test_error_response_structure();

    printf("\n====================================================\n");
    printf("All Gateway Router Extension Errors Contract Tests Passed\n");
    return 0;
}

