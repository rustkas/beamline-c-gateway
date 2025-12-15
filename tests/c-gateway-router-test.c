#include <assert.h>
#include <stdio.h>

int map_router_error_status(const char *resp_json);

static void test_status_ok_defaults_to_0(void)
{
    const char *resp =
        "{"\
        "\"ok\":true,"\
        "\"decision\":{\"provider_id\":\"p1\"},"\
        "\"context\":{\"request_id\":\"r1\"}"\
        "}";

    int status = map_router_error_status(resp);
    assert(status == 0);
}

static void test_status_invalid_request_400(void)
{
    const char *resp =
        "{"\
        "\"ok\":false,"\
        "\"error\":{\"code\":\"invalid_request\",\"message\":\"bad\"},"\
        "\"context\":{\"request_id\":\"r1\"}"\
        "}";

    int status = map_router_error_status(resp);
    assert(status == 400);
}

static void test_status_policy_not_found_404(void)
{
    const char *resp =
        "{"\
        "\"ok\":false,"\
        "\"error\":{\"code\":\"policy_not_found\",\"message\":\"not found\"},"\
        "\"context\":{\"request_id\":\"r1\"}"\
        "}";

    int status = map_router_error_status(resp);
    assert(status == 404);
}

static void test_status_internal_500(void)
{
    const char *resp =
        "{"\
        "\"ok\":false,"\
        "\"error\":{\"code\":\"internal\",\"message\":\"err\"},"\
        "\"context\":{\"request_id\":\"r1\"}"\
        "}";

    int status = map_router_error_status(resp);
    assert(status == 500);
}

static void test_status_unauthorized_401(void)
{
    const char *resp =
        "{"\
        "\"ok\":false,"\
        "\"error\":{\"code\":\"unauthorized\",\"message\":\"unauth\"},"\
        "\"context\":{\"request_id\":\"r1\"}"\
        "}";

    int status = map_router_error_status(resp);
    assert(status == 401);
}

static void test_status_decision_failed_500(void)
{
    const char *resp =
        "{"\
        "\"ok\":false,"\
        "\"error\":{\"code\":\"decision_failed\",\"message\":\"failed\"},"\
        "\"context\":{\"request_id\":\"r1\"}"\
        "}";

    int status = map_router_error_status(resp);
    assert(status == 500);
}

int main(void)
{
    test_status_ok_defaults_to_0();
    test_status_invalid_request_400();
    test_status_policy_not_found_404();
    test_status_internal_500();
    test_status_unauthorized_401();
    test_status_decision_failed_500();
    printf("c-gateway Router status mapping tests passed\n");
    printf("Note: Extension error tests are in c-gateway-router-extension-errors-test.c\n");
    return 0;
}
