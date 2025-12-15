#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/nats_client_stub.h"

static void test_nats_request_decide_stub(void)
{
    char buf[512];
    int rc = nats_request_decide("{}", buf, sizeof(buf));
    assert(rc == 0);
    assert(strstr(buf, "\"message_id\"") != NULL);
    assert(strstr(buf, "\"provider_id\"") != NULL);
}

static void test_nats_request_get_decision_stub(void)
{
    char buf[512];
    int rc = nats_request_get_decision("tenant-1", "msg-1", buf, sizeof(buf));
    assert(rc == 0);
    assert(strstr(buf, "stub-get-by-id") != NULL || strstr(buf, "message_id") != NULL);
}

static void test_nats_request_decide_invalid_args(void)
{
    char buf[16];

    /* NULL req_json should fail */
    int rc = nats_request_decide(NULL, buf, sizeof(buf));
    assert(rc != 0);

    /* NULL buffer or zero size should fail */
    rc = nats_request_decide("{}", NULL, sizeof(buf));
    assert(rc != 0);
    rc = nats_request_decide("{}", buf, 0U);
    assert(rc != 0);

    /* Too small buffer should fail */
    rc = nats_request_decide("{}", buf, 4U);
    assert(rc != 0);
}

static void test_nats_request_get_decision_invalid_args(void)
{
    char buf[16];

    /* Empty message_id should fail */
    int rc = nats_request_get_decision("tenant-1", "", buf, sizeof(buf));
    assert(rc != 0);

    /* NULL buffer or zero size should fail */
    rc = nats_request_get_decision("tenant-1", "msg-1", NULL, sizeof(buf));
    assert(rc != 0);
    rc = nats_request_get_decision("tenant-1", "msg-1", buf, 0U);
    assert(rc != 0);
}

int main(void)
{
    test_nats_request_decide_stub();
    test_nats_request_get_decision_stub();
    test_nats_request_decide_invalid_args();
    test_nats_request_get_decision_invalid_args();
    printf("c-gateway NATS stub tests passed\n");
    return 0;
}
