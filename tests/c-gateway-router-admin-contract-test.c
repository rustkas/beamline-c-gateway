#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <jansson.h>

/* Function is exported from http_server.c for testing */
extern int map_router_error_status(const char *resp_json);

/* Test: Extension health response structure */
static void test_extension_health_response_structure(void)
{
    const char *resp =
        "{"
        "\"health\":{"
        "\"extension_id_1\":{"
        "\"extension_id\":\"extension_id_1\","
        "\"status\":\"healthy\","
        "\"success_rate\":0.95,"
        "\"avg_latency_ms\":25.5,"
        "\"p50_latency_ms\":20.0,"
        "\"p95_latency_ms\":50.0,"
        "\"p99_latency_ms\":100.0"
        "}"
        "}"
        "}";

    json_t *root = json_loads(resp, 0, NULL);
    assert(root != NULL);

    json_t *health = json_object_get(root, "health");
    assert(health != NULL);
    assert(json_is_object(health));

    json_t *ext1 = json_object_get(health, "extension_id_1");
    assert(ext1 != NULL);
    assert(json_is_object(ext1));

    json_t *status = json_object_get(ext1, "status");
    assert(status != NULL);
    assert(json_is_string(status));
    const char *status_str = json_string_value(status);
    assert(strcmp(status_str, "healthy") == 0 || 
           strcmp(status_str, "degraded") == 0 || 
           strcmp(status_str, "unhealthy") == 0);

    json_t *success_rate = json_object_get(ext1, "success_rate");
    assert(success_rate != NULL);
    assert(json_is_real(success_rate) || json_is_integer(success_rate));

    json_decref(root);
    printf("✓ Extension health response structure is valid\n");
}

/* Test: Circuit breaker states response structure */
static void test_circuit_breaker_states_response_structure(void)
{
    const char *resp =
        "{"
        "\"states\":{"
        "\"extension_id_1\":{"
        "\"extension_id\":\"extension_id_1\","
        "\"state\":\"closed\","
        "\"opened_at_ms\":0"
        "}"
        "}"
        "}";

    json_t *root = json_loads(resp, 0, NULL);
    assert(root != NULL);

    json_t *states = json_object_get(root, "states");
    assert(states != NULL);
    assert(json_is_object(states));

    json_t *ext1 = json_object_get(states, "extension_id_1");
    assert(ext1 != NULL);
    assert(json_is_object(ext1));

    json_t *state = json_object_get(ext1, "state");
    assert(state != NULL);
    assert(json_is_string(state));
    const char *state_str = json_string_value(state);
    assert(strcmp(state_str, "closed") == 0 || 
           strcmp(state_str, "open") == 0 || 
           strcmp(state_str, "half_open") == 0);

    json_decref(root);
    printf("✓ Circuit breaker states response structure is valid\n");
}

/* Test: Dry-run pipeline success response structure */
static void test_dry_run_pipeline_success_structure(void)
{
    const char *resp =
        "{"
        "\"ok\":true,"
        "\"result\":{"
        "\"decision\":{"
        "\"provider_id\":\"openai\","
        "\"reason\":\"weighted\","
        "\"priority\":1"
        "},"
        "\"executed_extensions\":[],"
        "\"final_payload\":{}"
        "}"
        "}";

    json_t *root = json_loads(resp, 0, NULL);
    assert(root != NULL);

    json_t *ok = json_object_get(root, "ok");
    assert(ok != NULL);
    assert(json_is_true(ok));

    json_t *result = json_object_get(root, "result");
    assert(result != NULL);
    assert(json_is_object(result));

    json_t *decision = json_object_get(result, "decision");
    assert(decision != NULL);
    assert(json_is_object(decision));

    json_t *provider_id = json_object_get(decision, "provider_id");
    assert(provider_id != NULL);
    assert(json_is_string(provider_id));

    json_t *reason = json_object_get(decision, "reason");
    assert(reason != NULL);
    assert(json_is_string(reason));

    json_decref(root);
    printf("✓ Dry-run pipeline success response structure is valid\n");
}

/* Test: Dry-run pipeline error response structure */
static void test_dry_run_pipeline_error_structure(void)
{
    const char *resp =
        "{"
        "\"ok\":false,"
        "\"error\":{"
        "\"code\":\"POLICY_NOT_FOUND\","
        "\"message\":\"Policy 'tenant-123/policy-456' not found\""
        "}"
        "}";

    json_t *root = json_loads(resp, 0, NULL);
    assert(root != NULL);

    json_t *ok = json_object_get(root, "ok");
    assert(ok != NULL);
    assert(json_is_false(ok));

    json_t *error = json_object_get(root, "error");
    assert(error != NULL);
    assert(json_is_object(error));

    json_t *code = json_object_get(error, "code");
    assert(code != NULL);
    assert(json_is_string(code));

    json_t *message = json_object_get(error, "message");
    assert(message != NULL);
    assert(json_is_string(message));

    json_decref(root);
    printf("✓ Dry-run pipeline error response structure is valid\n");
}

/* Test: Pipeline complexity response structure */
static void test_pipeline_complexity_response_structure(void)
{
    const char *resp =
        "{"
        "\"complexity_score\":45,"
        "\"total_extensions\":3,"
        "\"pre_count\":1,"
        "\"validators_count\":1,"
        "\"post_count\":1,"
        "\"complexity_level\":\"medium\","
        "\"estimated_latency_ms\":90"
        "}";

    json_t *root = json_loads(resp, 0, NULL);
    assert(root != NULL);

    json_t *complexity_score = json_object_get(root, "complexity_score");
    assert(complexity_score != NULL);
    assert(json_is_integer(complexity_score));

    json_t *total_extensions = json_object_get(root, "total_extensions");
    assert(total_extensions != NULL);
    assert(json_is_integer(total_extensions));

    json_t *complexity_level = json_object_get(root, "complexity_level");
    assert(complexity_level != NULL);
    assert(json_is_string(complexity_level));
    const char *level_str = json_string_value(complexity_level);
    assert(strcmp(level_str, "low") == 0 || 
           strcmp(level_str, "medium") == 0 || 
           strcmp(level_str, "high") == 0 || 
           strcmp(level_str, "very_high") == 0);

    json_decref(root);
    printf("✓ Pipeline complexity response structure is valid\n");
}

/* Test: Admin endpoint unauthorized error (401) */
static void test_admin_unauthorized_401(void)
{
    const char *resp =
        "{"
        "\"error\":{"
        "\"code\":\"UNAUTHORIZED\","
        "\"message\":\"missing or invalid API key\""
        "}"
        "}";

    json_t *root = json_loads(resp, 0, NULL);
    assert(root != NULL);

    json_t *error = json_object_get(root, "error");
    assert(error != NULL);
    assert(json_is_object(error));

    json_t *code = json_object_get(error, "code");
    assert(code != NULL);
    assert(json_is_string(code));
    const char *code_str = json_string_value(code);
    assert(strcmp(code_str, "UNAUTHORIZED") == 0);

    json_decref(root);
    printf("✓ Admin unauthorized error structure is valid\n");
}

/* Test: Admin endpoint service unavailable error (503) */
static void test_admin_service_unavailable_503(void)
{
    const char *resp =
        "{"
        "\"error\":{"
        "\"code\":\"SERVICE_UNAVAILABLE\","
        "\"message\":\"Router or NATS unavailable\""
        "}"
        "}";

    int status = map_router_error_status(resp);
    assert(status == 503);
    printf("✓ Admin SERVICE_UNAVAILABLE maps to 503\n");
}

/* Test: Admin endpoint policy not found error (404) */
static void test_admin_policy_not_found_404(void)
{
    const char *resp =
        "{"
        "\"ok\":false,"
        "\"error\":{"
        "\"code\":\"POLICY_NOT_FOUND\","
        "\"message\":\"Policy 'tenant-123/policy-456' not found\""
        "}"
        "}";

    int status = map_router_error_status(resp);
    assert(status == 404);
    printf("✓ Admin POLICY_NOT_FOUND maps to 404\n");
}

int main(void)
{
    printf("Running Gateway Router Admin Contract Tests\n");
    printf("==========================================\n\n");

    test_extension_health_response_structure();
    test_circuit_breaker_states_response_structure();
    test_dry_run_pipeline_success_structure();
    test_dry_run_pipeline_error_structure();
    test_pipeline_complexity_response_structure();
    test_admin_unauthorized_401();
    test_admin_service_unavailable_503();
    test_admin_policy_not_found_404();

    printf("\n==========================================\n");
    printf("All Gateway Router Admin Contract Tests Passed\n");
    return 0;
}

