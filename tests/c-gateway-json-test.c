#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
    char request_id[64];
    char trace_id[64];
    char tenant_id[64];
} request_context_t;

int validate_decide_request(const char *request_body, request_context_t *ctx);
int build_route_request_json(const char *http_body,
                             const request_context_t *ctx,
                             char **out_json);

static void test_validate_decide_request_ok(void)
{
    const char *body =
        "{"\
        "\"version\":\"1\","\
        "\"tenant_id\":\"t1\","\
        "\"request_id\":\"r1\","\
        "\"task\":{\"type\":\"chat\",\"payload\":{}}"\
        "}";

    request_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    int rc = validate_decide_request(body, &ctx);
    assert(rc == 0);
    assert(strcmp(ctx.request_id, "r1") == 0);
}

static void test_validate_decide_request_missing_task(void)
{
    const char *body =
        "{"\
        "\"version\":\"1\","\
        "\"tenant_id\":\"t1\","\
        "\"request_id\":\"r1\""\
        "}";

    request_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    int rc = validate_decide_request(body, &ctx);
    assert(rc != 0);
}

static void test_validate_decide_request_wrong_version(void)
{
    const char *body =
        "{"\
        "\"version\":\"2\","\
        "\"tenant_id\":\"t1\","\
        "\"request_id\":\"r1\","\
        "\"task\":{\"type\":\"chat\",\"payload\":{}}"\
        "}";

    request_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    int rc = validate_decide_request(body, &ctx);
    assert(rc != 0);
}

static void test_build_route_request_json_basic(void)
{
    const char *body =
        "{"\
        "\"version\":\"1\","\
        "\"tenant_id\":\"t1\","\
        "\"request_id\":\"r1\","\
        "\"message_id\":\"m1\","\
        "\"message_type\":\"chat\","\
        "\"payload\":{},"\
        "\"metadata\":{},"\
        "\"policy_id\":\"p1\","\
        "\"context\":{\"foo\":\"bar\"}"\
        "}";

    request_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    strcpy(ctx.tenant_id, "t-header");
    strcpy(ctx.trace_id, "trace-1");

    char *route_json = NULL;
    int rc = build_route_request_json(body, &ctx, &route_json);
    assert(rc == 0);
    assert(route_json != NULL);

    /* simple substring checks */
    assert(strstr(route_json, "\"tenant_id\":\"t-header\"") != NULL);
    assert(strstr(route_json, "\"request_id\":\"r1\"") != NULL);
    assert(strstr(route_json, "\"message_id\":\"m1\"") != NULL);
    assert(strstr(route_json, "\"message_type\":\"chat\"") != NULL);
    assert(strstr(route_json, "\"policy_id\":\"p1\"") != NULL);

    free(route_json);
}

static void test_build_route_request_json_missing_message_id(void)
{
    const char *body =
        "{"\
        "\"version\":\"1\","\
        "\"tenant_id\":\"t1\","\
        "\"request_id\":\"r1\","\
        "\"message_type\":\"chat\","\
        "\"payload\":{}"\
        "}";

    request_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    char *route_json = NULL;
    int rc = build_route_request_json(body, &ctx, &route_json);
    assert(rc != 0);
    assert(route_json == NULL);
}

static void test_build_route_request_json_missing_message_type(void)
{
    const char *body =
        "{"\
        "\"version\":\"1\","\
        "\"tenant_id\":\"t1\","\
        "\"request_id\":\"r1\","\
        "\"message_id\":\"m1\","\
        "\"payload\":{}"\
        "}";

    request_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    char *route_json = NULL;
    int rc = build_route_request_json(body, &ctx, &route_json);
    assert(rc != 0);
    assert(route_json == NULL);
}

static void test_build_route_request_json_invalid_json(void)
{
    /* Truncated JSON string */
    const char *body = "{ \"version\": \"1\", \"tenant_id\": \"t1\"";

    request_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));

    char *route_json = NULL;
    int rc = build_route_request_json(body, &ctx, &route_json);
    assert(rc != 0);
    assert(route_json == NULL);
}

static void test_build_route_request_json_header_overrides_body(void)
{
    const char *body =
        "{"\
        "\"version\":\"1\","\
        "\"tenant_id\":\"t-body\","\
        "\"request_id\":\"r1\","\
        "\"message_id\":\"m1\","\
        "\"message_type\":\"chat\","\
        "\"payload\":{},"\
        "\"metadata\":{}"\
        "}";

    request_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    strcpy(ctx.tenant_id, "t-header");
    strcpy(ctx.trace_id, "trace-ctx");

    char *route_json = NULL;
    int rc = build_route_request_json(body, &ctx, &route_json);
    assert(rc == 0);
    assert(route_json != NULL);

    /* tenant_id and trace_id from context must override body values */
    assert(strstr(route_json, "\"tenant_id\":\"t-header\"") != NULL);
    assert(strstr(route_json, "\"trace_id\":\"trace-ctx\"") != NULL);

    free(route_json);
}

int main(void)
{
    test_validate_decide_request_ok();
    test_validate_decide_request_missing_task();
    test_validate_decide_request_wrong_version();
    test_build_route_request_json_basic();
    test_build_route_request_json_header_overrides_body();
    test_build_route_request_json_missing_message_id();
    test_build_route_request_json_missing_message_type();
    test_build_route_request_json_invalid_json();
    printf("c-gateway JSON tests passed\n");
    return 0;
}
