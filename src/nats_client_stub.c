#include "nats_client_stub.h"

#include <stdio.h>
#include <string.h>

const char *nats_get_status_string(void)
{
    return "stub"; /* indicates stubbed NATS client */
}

int nats_request_decide(const char *req_json, char *resp_buf, size_t resp_size) {
    (void)req_json; /* unused for stub */

    if (resp_buf == NULL || resp_size == 0U) {
        return -1;
    }

    /*
     * Stubbed RouteDecisionResponse JSON.
     * In real implementation this will call NATS and Router.
     */
    const char *dummy =
        "{"
        "\"message_id\":\"dummy\"," 
        "\"provider_id\":\"provider-1\"," 
        "\"reason\":\"stub\"," 
        "\"priority\":1," 
        "\"expected_latency_ms\":42," 
        "\"expected_cost\":0.001," 
        "\"currency\":\"USD\"," 
        "\"trace_id\":\"trace-stub\"" 
        "}";

    size_t len = strlen(dummy);
    if (len + 1U > resp_size) {
        return -1;
    }

    memcpy(resp_buf, dummy, len + 1U);

    return 0;
}

int nats_request_get_decision(const char *tenant_id,
                              const char *message_id,
                              char *resp_buf,
                              size_t resp_size)
{
    (void)tenant_id;  /* unused for stub */
    (void)message_id; /* unused for stub */

    if (resp_buf == NULL || resp_size == 0U) {
        return -1;
    }

    /* For now reuse a dummy decision JSON. Real implementation will call Router via NATS. */
    const char *dummy =
        "{"
        "\"message_id\":\"dummy\"," 
        "\"provider_id\":\"provider-1\"," 
        "\"reason\":\"stub-get-by-id\"," 
        "\"priority\":1," 
        "\"expected_latency_ms\":42," 
        "\"expected_cost\":0.001," 
        "\"currency\":\"USD\"," 
        "\"trace_id\":\"trace-stub\"" 
        "}";

    size_t len = strlen(dummy);
    if (len + 1U > resp_size) {
        return -1;
    }

    memcpy(resp_buf, dummy, len + 1U);

    return 0;
}

int nats_request_get_extension_health(char *resp_buf, size_t resp_size)
{
    if (resp_buf == NULL || resp_size == 0U) {
        return -1;
    }

    const char *dummy =
        "{"
        "\"health\":{"
        "\"extension_1\":{"
        "\"extension_id\":\"extension_1\","
        "\"status\":\"healthy\","
        "\"success_rate\":0.95,"
        "\"avg_latency_ms\":25.5"
        "}"
        "}"
        "}";

    size_t len = strlen(dummy);
    if (len + 1U > resp_size) {
        return -1;
    }

    memcpy(resp_buf, dummy, len + 1U);
    return 0;
}

int nats_request_get_circuit_breaker_states(char *resp_buf, size_t resp_size)
{
    if (resp_buf == NULL || resp_size == 0U) {
        return -1;
    }

    const char *dummy =
        "{"
        "\"states\":["
        "{"
        "\"extension_id\":\"extension_1\","
        "\"state\":\"closed\","
        "\"opened_at_ms\":0"
        "}"
        "]"
        "}";

    size_t len = strlen(dummy);
    if (len + 1U > resp_size) {
        return -1;
    }

    memcpy(resp_buf, dummy, len + 1U);
    return 0;
}

int nats_request_dry_run_pipeline(const char *req_json, char *resp_buf, size_t resp_size)
{
    (void)req_json; /* unused for stub */

    if (resp_buf == NULL || resp_size == 0U) {
        return -1;
    }

    const char *dummy =
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

    size_t len = strlen(dummy);
    if (len + 1U > resp_size) {
        return -1;
    }

    memcpy(resp_buf, dummy, len + 1U);
    return 0;
}

int nats_request_get_pipeline_complexity(const char *tenant_id,
                                        const char *policy_id,
                                        char *resp_buf,
                                        size_t resp_size)
{
    (void)tenant_id; /* unused for stub */
    (void)policy_id; /* unused for stub */

    if (resp_buf == NULL || resp_size == 0U) {
        return -1;
    }

    const char *dummy =
        "{"
        "\"complexity_score\":45,"
        "\"total_extensions\":3,"
        "\"pre_count\":1,"
        "\"validators_count\":1,"
        "\"post_count\":1,"
        "\"estimated_latency_ms\":90,"
        "\"recommended_max_total\":4,"
        "\"recommended_max_pre\":2,"
        "\"recommended_max_validators\":2,"
        "\"recommended_max_post\":2,"
        "\"warnings\":[],"
        "\"recommendations\":[]"
        "}";

    size_t len = strlen(dummy);
    if (len + 1U > resp_size) {
        return -1;
    }

    memcpy(resp_buf, dummy, len + 1U);
    return 0;
}
