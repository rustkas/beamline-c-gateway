#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

static int send_http_request(const char *host,
                             int port,
                             const char *request,
                             char *resp_buf,
                             size_t resp_buf_size)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }

    size_t req_len = strlen(request);
    if (write(sock, request, req_len) != (ssize_t)req_len) {
        close(sock);
        return -1;
    }

    ssize_t n = read(sock, resp_buf, resp_buf_size - 1);
    if (n <= 0) {
        close(sock);
        return -1;
    }

    resp_buf[n] = '\0';
    close(sock);
    return 0;
}

static void test_health(void)
{
    char resp[2048];
    const char *req =
        "GET /health HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: close\r\n"
        "\r\n";

    int rc = send_http_request("127.0.0.1", 8080, req, resp, sizeof(resp));
    assert(rc == 0);
    assert(strstr(resp, "HTTP/1.1 200") != NULL);
    assert(strstr(resp, "{\"status\":\"ok\"}") != NULL);
}

static void test_post_routes_decide_missing_tenant(void)
{
    char resp[4096];
    const char *req =
        "POST /api/v1/routes/decide HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 2\r\n"
        "Connection: close\r\n"
        "\r\n"
        "{}";

    int rc = send_http_request("127.0.0.1", 8080, req, resp, sizeof(resp));
    assert(rc == 0);
    assert(strstr(resp, "HTTP/1.1 400") != NULL);
    assert(strstr(resp, "\"invalid_request\"") != NULL);
}

static void test_get_decide_missing_tenant(void)
{
    char resp[4096];
    const char *req =
        "GET /api/v1/routes/decide/msg-http-001 HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Connection: close\r\n"
        "\r\n";

    int rc = send_http_request("127.0.0.1", 8080, req, resp, sizeof(resp));
    assert(rc == 0);
    assert(strstr(resp, "HTTP/1.1 400") != NULL);
    assert(strstr(resp, "\"invalid_request\"") != NULL);
}

static void test_get_decide_with_tenant(void)
{
    char resp[4096];
    const char *req =
        "GET /api/v1/routes/decide/msg-http-002 HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "X-Tenant-ID: tenant-http\r\n"
        "Connection: close\r\n"
        "\r\n";

    int rc = send_http_request("127.0.0.1", 8080, req, resp, sizeof(resp));
    assert(rc == 0);
    /* Depending on environment, Router may return 200/404 or C-Gateway may return 503 */
    assert(strstr(resp, "HTTP/1.1 200") != NULL ||
           strstr(resp, "HTTP/1.1 404") != NULL ||
           strstr(resp, "HTTP/1.1 503") != NULL);
}

static void test_post_routes_decide_happy(void)
{
    char resp[4096];
    const char *req =
        "POST /api/v1/routes/decide HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Type: application/json\r\n"
        "X-Tenant-ID: tenant-http\r\n"
        "Content-Length: 120\r\n" /* approximate, enough for smoke test */
        "Connection: close\r\n"
        "\r\n"
        "{"
        "\"message_id\":\"m-http-1\"," 
        "\"message_type\":\"chat\"," 
        "\"payload\":{}," 
        "\"metadata\":{}," 
        "\"policy_id\":\"default\"," 
        "\"context\":{}" 
        "}";

    int rc = send_http_request("127.0.0.1", 8080, req, resp, sizeof(resp));
    assert(rc == 0);
    /* With real Router we expect 200, with Router down we may get 503 */
    assert(strstr(resp, "HTTP/1.1 200") != NULL ||
           strstr(resp, "HTTP/1.1 503") != NULL);
}

static void test_post_routes_decide_auth_required(void)
{
    char resp[4096];

    /* Without Authorization header -> 401 when GATEWAY_AUTH_REQUIRED=true */
    const char *req_no_auth =
        "POST /api/v1/routes/decide HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Type: application/json\r\n"
        "X-Tenant-ID: tenant-http\r\n"
        "Content-Length: 2\r\n"
        "Connection: close\r\n"
        "\r\n"
        "{}";

    int rc = send_http_request("127.0.0.1", 8080, req_no_auth, resp, sizeof(resp));
    assert(rc == 0);
    /* In auth-required mode Gateway must return 401 */
    if (getenv("GATEWAY_AUTH_REQUIRED") != NULL &&
        (strcmp(getenv("GATEWAY_AUTH_REQUIRED"), "true") == 0 ||
         strcmp(getenv("GATEWAY_AUTH_REQUIRED"), "1") == 0)) {
        assert(strstr(resp, "HTTP/1.1 401") != NULL);
    }

    /* With Authorization header -> 200/503 */
    const char *req_with_auth =
        "POST /api/v1/routes/decide HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Type: application/json\r\n"
        "X-Tenant-ID: tenant-http\r\n"
        "Authorization: Bearer test\r\n"
        "Content-Length: 2\r\n"
        "Connection: close\r\n"
        "\r\n"
        "{}";

    rc = send_http_request("127.0.0.1", 8080, req_with_auth, resp, sizeof(resp));
    assert(rc == 0);
    assert(strstr(resp, "HTTP/1.1 200") != NULL ||
           strstr(resp, "HTTP/1.1 503") != NULL);
}

static void test_post_routes_decide_rate_limit(void)
{
    char resp[4096];
    const char *req =
        "POST /api/v1/routes/decide HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Type: application/json\r\n"
        "X-Tenant-ID: tenant-http\r\n"
        "Content-Length: 2\r\n"
        "Connection: close\r\n"
        "\r\n"
        "{}";

    /* First request: allowed (200/503) */
    int rc = send_http_request("127.0.0.1", 8080, req, resp, sizeof(resp));
    assert(rc == 0);

    /* Second request within the same window: may hit rate limit (429) */
    rc = send_http_request("127.0.0.1", 8080, req, resp, sizeof(resp));
    assert(rc == 0);

    /* Only assert 429 if rate limiting is configured aggressively via env. */
    if (getenv("GATEWAY_RATE_LIMIT_ROUTES_DECIDE_LIMIT") != NULL) {
        if (strcmp(getenv("GATEWAY_RATE_LIMIT_ROUTES_DECIDE_LIMIT"), "1") == 0) {
            assert(strstr(resp, "HTTP/1.1 429") != NULL);
        }
    }
}

int main(void)
{
    test_health();
    test_post_routes_decide_missing_tenant();
    test_get_decide_missing_tenant();
    test_get_decide_with_tenant();
    test_post_routes_decide_happy();
    test_post_routes_decide_auth_required();
    test_post_routes_decide_rate_limit();
    printf("c-gateway HTTP tests passed\n");
    return 0;
}
