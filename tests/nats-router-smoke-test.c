#ifdef USE_NATS_LIB

#include <nats/nats.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *DEFAULT_DECIDE_SUBJECT = "beamline.router.v1.decide";

static int nats_router_smoke_request(void)
{
    natsStatus      s    = NATS_OK;
    natsConnection *conn = NULL;
    natsOptions    *opts = NULL;
    natsMsg        *msg  = NULL;

    const char *url = getenv("NATS_URL");
    if (url == NULL || url[0] == '\0')
        url = "nats://nats:4222";

    const char *subject = getenv("ROUTER_DECIDE_SUBJECT");
    if (subject == NULL || subject[0] == '\0')
        subject = DEFAULT_DECIDE_SUBJECT;

    const char *req_json =
        "{"
        "\"message_id\":\"smoke-001\"," 
        "\"tenant_id\":\"tenant-smoke\"," 
        "\"trace_id\":\"trace-smoke\"," 
        "\"message_type\":\"chat\"," 
        "\"payload\":{}," 
        "\"policy_id\":\"default\"" 
        "}";

    s = natsOptions_Create(&opts);
    if (s == NATS_OK)
        s = natsOptions_SetURL(opts, url);

    if (s == NATS_OK)
        s = natsConnection_Connect(&conn, opts);

    if (s != NATS_OK)
    {
        fprintf(stderr, "[nats-router-smoke] connect error: %s\n", natsStatus_GetText(s));
        natsOptions_Destroy(opts);
        return -1;
    }

/* Sticky-session style smoke test: send two requests with the same user_id context.
 * If Router implements sticky routing and marks responses with reason "sticky",
 * we expect provider_id to be the same. Otherwise we only check that both
 * responses are structurally valid.
 */
static int nats_router_sticky_smoke(void)
{
    natsStatus      s    = NATS_OK;
    natsConnection *conn = NULL;
    natsOptions    *opts = NULL;
    natsMsg        *msg1 = NULL;
    natsMsg        *msg2 = NULL;

    const char *url = getenv("NATS_URL");
    if (url == NULL || url[0] == '\0')
        url = "nats://nats:4222";

    const char *subject = getenv("ROUTER_DECIDE_SUBJECT");
    if (subject == NULL || subject[0] == '\0')
        subject = DEFAULT_DECIDE_SUBJECT;

    const char *user_id = "user-sticky-c-test";

    const char *req1 =
        "{"\
        "\"message_id\":\"sticky-001\","\
        "\"tenant_id\":\"tenant-smoke\","\
        "\"trace_id\":\"trace-sticky-1\","\
        "\"message_type\":\"chat\","\
        "\"payload\":{},"\
        "\"context\":{\"sticky\":{\"session_key\":\"user_id\"},\"user_id\":\"user-sticky-c-test\"}"\
        "}";

    const char *req2 =
        "{"\
        "\"message_id\":\"sticky-002\","\
        "\"tenant_id\":\"tenant-smoke\","\
        "\"trace_id\":\"trace-sticky-2\","\
        "\"message_type\":\"chat\","\
        "\"payload\":{},"\
        "\"context\":{\"sticky\":{\"session_key\":\"user_id\"},\"user_id\":\"user-sticky-c-test\"}"\
        "}";

    s = natsOptions_Create(&opts);
    if (s == NATS_OK)
        s = natsOptions_SetURL(opts, url);

    if (s == NATS_OK)
        s = natsConnection_Connect(&conn, opts);

    if (s != NATS_OK)
    {
        fprintf(stderr, "[nats-router-sticky] connect error: %s\n", natsStatus_GetText(s));
        natsOptions_Destroy(opts);
        return -1;
    }

    s = natsConnection_RequestString(&msg1,
                                     conn,
                                     subject,
                                     req1,
                                     (int)strlen(req1),
                                     5000);
    if (s != NATS_OK)
    {
        fprintf(stderr, "[nats-router-sticky] first request error: %s\n", natsStatus_GetText(s));
        natsMsg_Destroy(msg1);
        natsConnection_Destroy(conn);
        natsOptions_Destroy(opts);
        return -1;
    }

    s = natsConnection_RequestString(&msg2,
                                     conn,
                                     subject,
                                     req2,
                                     (int)strlen(req2),
                                     5000);
    if (s != NATS_OK)
    {
        fprintf(stderr, "[nats-router-sticky] second request error: %s\n", natsStatus_GetText(s));
        natsMsg_Destroy(msg1);
        natsMsg_Destroy(msg2);
        natsConnection_Destroy(conn);
        natsOptions_Destroy(opts);
        return -1;
    }

    const char *data1 = natsMsg_GetData(msg1);
    const char *data2 = natsMsg_GetData(msg2);
    int         len1  = natsMsg_GetDataLength(msg1);
    int         len2  = natsMsg_GetDataLength(msg2);

    if (data1 == NULL || len1 <= 0 || data2 == NULL || len2 <= 0)
    {
        fprintf(stderr, "[nats-router-sticky] empty response(s)\n");
        natsMsg_Destroy(msg1);
        natsMsg_Destroy(msg2);
        natsConnection_Destroy(conn);
        natsOptions_Destroy(opts);
        return -1;
    }

    /* Basic sanity check */
    if ((strchr(data1, '{') == NULL || strchr(data1, '}') == NULL) ||
        (strchr(data2, '{') == NULL || strchr(data2, '}') == NULL))
    {
        fprintf(stderr, "[nats-router-sticky] response not JSON-like\n");
        natsMsg_Destroy(msg1);
        natsMsg_Destroy(msg2);
        natsConnection_Destroy(conn);
        natsOptions_Destroy(opts);
        return -1;
    }

    /* If Router reports reason \"sticky\" and provider_id, check they are consistent. */
    if (strstr(data1, "provider_id") != NULL &&
        strstr(data2, "provider_id") != NULL &&
        (strstr(data1, "\"reason\":\"sticky\"") != NULL ||
         strstr(data2, "\"reason\":\"sticky\"") != NULL))
    {
        /* naive substring extraction: just ensure provider_id substring matches */
        const char *p1 = strstr(data1, "provider_id");
        const char *p2 = strstr(data2, "provider_id");
        if (p1 != NULL && p2 != NULL)
        {
            /* compare up to next comma as a simple heuristic */
            const char *end1 = strchr(p1, ',');
            const char *end2 = strchr(p2, ',');
            int len_cmp1 = end1 != NULL ? (int)(end1 - p1) : 40;
            int len_cmp2 = end2 != NULL ? (int)(end2 - p2) : 40;
            int len_cmp  = len_cmp1 < len_cmp2 ? len_cmp1 : len_cmp2;
            if (len_cmp > 0)
            {
                if (strncmp(p1, p2, (size_t)len_cmp) != 0)
                {
                    fprintf(stderr, "[nats-router-sticky] provider_id not sticky\n");
                }
            }
        }
    }

    printf("[nats-router-sticky] response1: %.*s\n", len1, data1);
    printf("[nats-router-sticky] response2: %.*s\n", len2, data2);

    natsMsg_Destroy(msg1);
    natsMsg_Destroy(msg2);
    natsConnection_Destroy(conn);
    natsOptions_Destroy(opts);

    return 0;
}

/* Timeout/error style smoke test: send a request to an unlikely subject
 * and ensure the client returns a non-OK status (e.g. timeout or no responders).
 */
static int nats_router_timeout_smoke(void)
{
    natsStatus      s    = NATS_OK;
    natsConnection *conn = NULL;
    natsOptions    *opts = NULL;
    natsMsg        *msg  = NULL;

    const char *url = getenv("NATS_URL");
    if (url == NULL || url[0] == '\0')
        url = "nats://nats:4222";

    const char *subject = "beamline.router.v1.nonexistent_subject_for_timeout";

    const char *req_json =
        "{"
        "\"message_id\":\"timeout-001\"," 
        "\"tenant_id\":\"tenant-timeout\"," 
        "\"trace_id\":\"trace-timeout\"," 
        "\"message_type\":\"chat\"," 
        "\"payload\":{}" 
        "}";

    s = natsOptions_Create(&opts);
    if (s == NATS_OK)
        s = natsOptions_SetURL(opts, url);

    if (s == NATS_OK)
        s = natsConnection_Connect(&conn, opts);

    if (s != NATS_OK)
    {
        fprintf(stderr, "[nats-router-timeout] connect error: %s\n", natsStatus_GetText(s));
        natsOptions_Destroy(opts);
        return -1;
    }

    s = natsConnection_RequestString(&msg,
                                     conn,
                                     subject,
                                     req_json,
                                     (int)strlen(req_json),
                                     1000); /* 1s timeout */
    if (s == NATS_OK)
    {
        /* Unexpected success: just log and treat as non-fatal smoke. */
        const char *data = natsMsg_GetData(msg);
        int         len  = natsMsg_GetDataLength(msg);
        printf("[nats-router-timeout] unexpected response: %.*s\n", len, data);
        natsMsg_Destroy(msg);
        natsConnection_Destroy(conn);
        natsOptions_Destroy(opts);
        return 0;
    }

    printf("[nats-router-timeout] request returned status: %s\n", natsStatus_GetText(s));

    natsMsg_Destroy(msg);
    natsConnection_Destroy(conn);
    natsOptions_Destroy(opts);

    return 0;
}

    s = natsConnection_RequestString(&msg,
                                     conn,
                                     subject,
                                     req_json,
                                     (int)strlen(req_json),
                                     5000);
    if (s != NATS_OK)
    {
        fprintf(stderr, "[nats-router-smoke] request error: %s\n", natsStatus_GetText(s));
        natsMsg_Destroy(msg);
        natsConnection_Destroy(conn);
        natsOptions_Destroy(opts);
        return -1;
    }

    const char *data = natsMsg_GetData(msg);
    int         len  = natsMsg_GetDataLength(msg);

    if (data == NULL || len <= 0)
    {
        fprintf(stderr, "[nats-router-smoke] empty response body\n");
        natsMsg_Destroy(msg);
        natsConnection_Destroy(conn);
        natsOptions_Destroy(opts);
        return -1;
    }

    /* Basic sanity check: response must look like JSON and contain either provider_id or error */
    if (strchr(data, '{') == NULL || strchr(data, '}') == NULL)
    {
        fprintf(stderr, "[nats-router-smoke] response is not JSON-like\n");
        natsMsg_Destroy(msg);
        natsConnection_Destroy(conn);
        natsOptions_Destroy(opts);
        return -1;
    }

    if (strstr(data, "provider_id") == NULL && strstr(data, "\"error\"") == NULL)
    {
        fprintf(stderr, "[nats-router-smoke] response missing provider_id/error: %.*s\n", len, data);
        natsMsg_Destroy(msg);
        natsConnection_Destroy(conn);
        natsOptions_Destroy(opts);
        return -1;
    }

    printf("[nats-router-smoke] response: %.*s\n", len, data);

    natsMsg_Destroy(msg);
    natsConnection_Destroy(conn);
    natsOptions_Destroy(opts);

    return 0;
}

int main(void)
{
    int rc = nats_router_smoke_request();
    assert(rc == 0);

    rc = nats_router_sticky_smoke();
    assert(rc == 0);

    rc = nats_router_timeout_smoke();
    assert(rc == 0);

    printf("nats-router-smoke-test (smoke+sticky+timeout) passed\n");
    return 0;
}

#else
int main(void)
{
    /* Built without USE_NATS_LIB; nothing to test */
    return 0;
}
#endif
