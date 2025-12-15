/* NATS Real Client Implementation
 * This file is only compiled when USE_NATS_LIB is defined.
 * When USE_NATS_LIB is not defined, this file provides stub implementations.
 */

#ifdef USE_NATS_LIB

#include <nats/nats.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *DEFAULT_DECIDE_SUBJECT      = "beamline.router.v1.decide";
static const char *DEFAULT_GET_DECISION_SUBJECT = "beamline.router.v1.get_decision";

static const char *g_last_nats_status = "unknown"; /* connected|disconnected|unknown */

const char *nats_get_status_string(void)
{
    return g_last_nats_status;
}

static int nats_request_common(const char *subject,
                               const char *req_json,
                               char *resp_buf,
                               size_t resp_size)
{
    if (subject == NULL || subject[0] == '\0' ||
        req_json == NULL || resp_buf == NULL || resp_size == 0U)
    {
        return -1;
    }

    natsStatus      s       = NATS_OK;
    natsConnection *conn    = NULL;
    natsMsg        *reply   = NULL;
    natsOptions    *opts    = NULL;

    const char *url = getenv("NATS_URL");
    if (url == NULL || url[0] == '\0')
    {
        url = "nats://nats:4222";
    }

    int timeout_ms = 5000;
    const char *timeout_env = getenv("ROUTER_REQUEST_TIMEOUT_MS");
    if (timeout_env != NULL && timeout_env[0] != '\0')
    {
        int val = atoi(timeout_env);
        if (val > 0)
        {
            timeout_ms = val;
        }
    }

    s = natsOptions_Create(&opts);
    if (s == NATS_OK)
    {
        s = natsOptions_SetURL(opts, url);
    }

    if (s == NATS_OK)
    {
        s = natsConnection_Connect(&conn, opts);
    }

    if (s != NATS_OK)
    {
        fprintf(stderr, "[c-gateway] nats connect error: %s\n", natsStatus_GetText(s));
        natsOptions_Destroy(opts);
        g_last_nats_status = "disconnected";
        return -1;
    }

    s = natsConnection_RequestString(&reply,
                                     conn,
                                     subject,
                                     req_json,
                                     (int)strlen(req_json),
                                     timeout_ms);
    if (s != NATS_OK)
    {
        fprintf(stderr, "[c-gateway] nats request error: %s\n", natsStatus_GetText(s));
        natsMsg_Destroy(reply);
        natsConnection_Destroy(conn);
        natsOptions_Destroy(opts);
        g_last_nats_status = "disconnected";
        return -1;
    }

    const char *data = natsMsg_GetData(reply);
    int         len  = natsMsg_GetDataLength(reply);

    if (data == NULL || len <= 0)
    {
        natsMsg_Destroy(reply);
        natsConnection_Destroy(conn);
        natsOptions_Destroy(opts);
        return -1;
    }

    if ((size_t)len + 1U > resp_size)
    {
        /* Not enough space in buffer */
        natsMsg_Destroy(reply);
        natsConnection_Destroy(conn);
        natsOptions_Destroy(opts);
        return -1;
    }

    memcpy(resp_buf, data, (size_t)len);
    resp_buf[len] = '\0';

    natsMsg_Destroy(reply);
    natsConnection_Destroy(conn);
    natsOptions_Destroy(opts);

    g_last_nats_status = "connected";
    return 0;
}

int nats_request_decide(const char *req_json, char *resp_buf, size_t resp_size)
{
    const char *subject = getenv("ROUTER_DECIDE_SUBJECT");
    if (subject == NULL || subject[0] == '\0')
    {
        subject = DEFAULT_DECIDE_SUBJECT;
    }
    return nats_request_common(subject, req_json, resp_buf, resp_size);
}

int nats_request_get_decision(const char *tenant_id,
                              const char *message_id,
                              char *resp_buf,
                              size_t resp_size)
{
    (void)tenant_id; /* currently unused, but reserved for future Router contracts */

    const char *subject = getenv("ROUTER_GET_DECISION_SUBJECT");
    if (subject == NULL || subject[0] == '\0')
    {
        subject = DEFAULT_GET_DECISION_SUBJECT;
    }

    /* For now Router expects only message_id in the request JSON or reuses existing contract.
     * If a dedicated DTO is required later, this function can build it here.
     */
    if (message_id == NULL || message_id[0] == '\0')
    {
        return -1;
    }

    /* Minimal JSON body: { "message_id": "..." } */
    char req_json[256];
    int  len = snprintf(req_json, sizeof(req_json),
                        "{\"message_id\":\"%s\"}", message_id);
    if (len < 0 || (size_t)len >= sizeof(req_json))
    {
        return -1;
    }

    return nats_request_common(subject, req_json, resp_buf, resp_size);
}

int nats_request_get_extension_health(char *resp_buf, size_t resp_size)
{
    const char *subject = getenv("ROUTER_ADMIN_GET_EXTENSION_HEALTH_SUBJECT");
    if (subject == NULL || subject[0] == '\0')
    {
        subject = "beamline.router.v1.admin.get_extension_health";
    }

    /* Empty request body (no parameters needed) */
    const char *req_json = "{}";
    return nats_request_common(subject, req_json, resp_buf, resp_size);
}

int nats_request_get_circuit_breaker_states(char *resp_buf, size_t resp_size)
{
    const char *subject = getenv("ROUTER_ADMIN_GET_CIRCUIT_BREAKER_STATES_SUBJECT");
    if (subject == NULL || subject[0] == '\0')
    {
        subject = "beamline.router.v1.admin.get_circuit_breaker_states";
    }

    /* Empty request body (no parameters needed) */
    const char *req_json = "{}";
    return nats_request_common(subject, req_json, resp_buf, resp_size);
}

int nats_request_dry_run_pipeline(const char *req_json, char *resp_buf, size_t resp_size)
{
    if (req_json == NULL || req_json[0] == '\0')
    {
        return -1;
    }

    const char *subject = getenv("ROUTER_ADMIN_DRY_RUN_PIPELINE_SUBJECT");
    if (subject == NULL || subject[0] == '\0')
    {
        subject = "beamline.router.v1.admin.dry_run_pipeline";
    }

    return nats_request_common(subject, req_json, resp_buf, resp_size);
}

int nats_request_get_pipeline_complexity(const char *tenant_id,
                                        const char *policy_id,
                                        char *resp_buf,
                                        size_t resp_size)
{
    if (tenant_id == NULL || tenant_id[0] == '\0' ||
        policy_id == NULL || policy_id[0] == '\0')
    {
        return -1;
    }

    const char *subject = getenv("ROUTER_ADMIN_GET_PIPELINE_COMPLEXITY_SUBJECT");
    if (subject == NULL || subject[0] == '\0')
    {
        subject = "beamline.router.v1.admin.get_pipeline_complexity";
    }

    /* Build request JSON: { "tenant_id": "...", "policy_id": "..." } */
    char req_json[512];
    int  len = snprintf(req_json, sizeof(req_json),
                        "{\"tenant_id\":\"%s\",\"policy_id\":\"%s\"}",
                        tenant_id, policy_id);
    if (len < 0 || (size_t)len >= sizeof(req_json))
    {
        return -1;
    }

    return nats_request_common(subject, req_json, resp_buf, resp_size);
}

#else /* USE_NATS_LIB not defined */

/* Stub implementations when NATS library is not available */
#include "nats_client_stub.h"

/* This file is intentionally empty when USE_NATS_LIB is not defined.
 * All functions are provided by nats_client_stub.c instead.
 */

#endif /* USE_NATS_LIB */
