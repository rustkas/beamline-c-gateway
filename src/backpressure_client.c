/* Backpressure Client: Gateway Implementation
 * 
 * ⚠️ EXPERIMENTAL / PoC CODE ⚠️
 * 
 * Reads Router backpressure status via Prometheus metrics endpoint.
 * Implements caching and periodic updates to avoid excessive HTTP requests.
 * 
 * TODO (CP3/Release):
 * - Replace HTTP polling with NATS pub/sub for real-time updates
 * - Replace with gRPC health check integration
 * - Add connection pooling for HTTP client
 * - Add retry logic with exponential backoff
 * - Add metrics for backpressure client operations
 * - Consider using Router's gRPC health service instead of HTTP
 */

#define _POSIX_C_SOURCE 200809L  /* For usleep */
#include "backpressure_client.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>

/* Note: For CP2, we use a simple approach: read from Router metrics via HTTP
 * For production, consider using NATS pub/sub or gRPC health check
 * This implementation uses basic socket-based HTTP requests (no curl dependency)
 * 
 * ⚠️ EXPERIMENTAL: Simple HTTP client implementation (PoC)
 * TODO (CP3/Release): Replace with proper HTTP client library or gRPC
 */

/* Simple cache for backpressure status */
static backpressure_status_t g_cached_status = BACKPRESSURE_INACTIVE;
static time_t g_last_check = 0;
static backpressure_client_config_t g_config = {0};
static int g_initialized = 0;

/* Simple HTTP client (socket-based, no curl dependency) */
static int http_get(const char *url, char *response_buf, size_t response_size) {
    /* Parse URL (simplified: http://host:port/path) */
    char host[256] = {0};
    uint16_t port = 8080;
    char path[256] = "/_metrics";
    
    if (strncmp(url, "http://", 7) != 0) {
        return -1; /* Only HTTP supported */
    }
    
    const char *host_start = url + 7;
    const char *port_start = strchr(host_start, ':');
    const char *path_start = strchr(host_start, '/');
    
    if (port_start && (!path_start || port_start < path_start)) {
        /* Port specified */
        strncpy(host, host_start, (size_t)(port_start - host_start));
        host[(size_t)(port_start - host_start)] = '\0';
        port = (uint16_t)atoi(port_start + 1);
        if (path_start) {
            strncpy(path, path_start, sizeof(path) - 1);
        }
    } else if (path_start) {
        /* No port, path specified */
        strncpy(host, host_start, (size_t)(path_start - host_start));
        host[(size_t)(path_start - host_start)] = '\0';
        strncpy(path, path_start, sizeof(path) - 1);
    } else {
        /* No port, no path */
        strncpy(host, host_start, sizeof(host) - 1);
    }
    
    /* Create socket */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return -1;
    }
    
    /* Set non-blocking for timeout */
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    
    /* Connect */
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0) {
        close(sock);
        return -1; /* Invalid host */
    }
    
    /* Try connect with timeout */
    int connect_result = connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (connect_result < 0 && errno != EINPROGRESS) {
        close(sock);
        return -1;
    }
    
    /* Wait for connection (simplified - for production use select/poll) */
    /* Use nanosleep instead of usleep for better POSIX compliance */
    struct timespec ts = { .tv_sec = 0, .tv_nsec = 100000000 }; /* 100ms */
    nanosleep(&ts, NULL);
    
    /* Build HTTP request */
    char request[1024];
    snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Connection: close\r\n"
        "\r\n",
        path, host, port);
    
    /* Send request */
    if (send(sock, request, strlen(request), 0) < 0) {
        close(sock);
        return -1;
    }
    
    /* Read response (simplified - read up to response_size) */
    ssize_t bytes_read = recv(sock, response_buf, response_size - 1, 0);
    close(sock);
    
    if (bytes_read < 0) {
        return -1;
    }
    
    response_buf[bytes_read] = '\0';
    return 0;
}

/* Parse Prometheus metrics to extract backpressure status */
static backpressure_status_t parse_backpressure_from_metrics(const char *metrics_text) {
    if (!metrics_text) {
        return BACKPRESSURE_INACTIVE;
    }
    
    /* Look for router_intake_backpressure_active metric */
    const char *search_pattern = "router_intake_backpressure_active";
    const char *found = strstr(metrics_text, search_pattern);
    if (!found) {
        /* Metric not found - assume inactive */
        return BACKPRESSURE_INACTIVE;
    }
    
    /* Find the value after the metric name */
    const char *value_start = strchr(found, ' ');
    if (!value_start) {
        return BACKPRESSURE_INACTIVE;
    }
    
    /* Skip whitespace */
    while (*value_start == ' ' || *value_start == '\t') {
        value_start++;
    }
    
    /* Parse value */
    int value = atoi(value_start);
    if (value == 1) {
        return BACKPRESSURE_ACTIVE;
    }
    
    /* Check for warning indicators (queue depth, latency) */
    const char *pending_pattern = "router_jetstream_pending_messages";
    const char *latency_pattern = "router_intake_processing_latency_p95";
    
    if (strstr(metrics_text, pending_pattern) || strstr(metrics_text, latency_pattern)) {
        /* Parse values to determine warning status */
        /* For simplicity, if backpressure_active is 0 but metrics exist, return WARNING */
        return BACKPRESSURE_WARNING;
    }
    
    return BACKPRESSURE_INACTIVE;
}

/* Fetch Router metrics via HTTP */
static backpressure_status_t fetch_router_metrics(void) {
    if (!g_config.router_metrics_url || strlen(g_config.router_metrics_url) == 0) {
        return BACKPRESSURE_INACTIVE;
    }
    
    char response_buf[8192] = {0}; /* 8KB buffer for metrics */
    int result = http_get(g_config.router_metrics_url, response_buf, sizeof(response_buf));
    
    if (result != 0) {
        /* HTTP request failed - return cached status or inactive */
        return g_cached_status;
    }
    
    /* Parse metrics from response (skip HTTP headers) */
    const char *body_start = strstr(response_buf, "\r\n\r\n");
    if (!body_start) {
        body_start = strstr(response_buf, "\n\n");
        if (body_start) {
            body_start += 2;
        } else {
            body_start = response_buf; /* No headers found, assume entire response is body */
        }
    } else {
        body_start += 4;
    }
    
    return parse_backpressure_from_metrics(body_start);
}

/* Get default configuration */
void backpressure_client_get_default_config(backpressure_client_config_t *config) {
    if (!config) return;
    
    memset(config, 0, sizeof(backpressure_client_config_t));
    config->router_metrics_url = "http://localhost:8080/_metrics";
    config->check_interval_seconds = 5;
    config->timeout_ms = 1000;
}

/* Parse configuration from environment variables */
int backpressure_client_parse_config(backpressure_client_config_t *config) {
    if (!config) return -1;
    
    /* Start with defaults */
    backpressure_client_get_default_config(config);
    
    /* Get Router metrics URL */
    const char *metrics_url = getenv("GATEWAY_ROUTER_METRICS_URL");
    if (metrics_url) {
        config->router_metrics_url = metrics_url;
    }
    
    /* Get check interval */
    const char *interval_str = getenv("GATEWAY_BACKPRESSURE_CHECK_INTERVAL_SECONDS");
    if (interval_str) {
        int val = atoi(interval_str);
        if (val > 0) {
            config->check_interval_seconds = val;
        }
    }
    
    /* Get timeout */
    const char *timeout_str = getenv("GATEWAY_BACKPRESSURE_TIMEOUT_MS");
    if (timeout_str) {
        int val = atoi(timeout_str);
        if (val > 0) {
            config->timeout_ms = val;
        }
    }
    
    return 0;
}

/* Initialize backpressure client */
int backpressure_client_init(const backpressure_client_config_t *config) {
    if (!config) {
        backpressure_client_get_default_config(&g_config);
    } else {
        memcpy(&g_config, config, sizeof(backpressure_client_config_t));
    }
    
    g_cached_status = BACKPRESSURE_INACTIVE;
    g_last_check = 0;
    g_initialized = 1;
    
    return 0;
}

/* Cleanup backpressure client */
void backpressure_client_cleanup(void) {
    g_initialized = 0;
}

/* Check Router backpressure status (with caching) */
backpressure_status_t backpressure_client_check_router_status(void) {
    if (!g_initialized) {
        return BACKPRESSURE_INACTIVE;
    }
    
    time_t now = time(NULL);
    time_t elapsed = now - g_last_check;
    
    /* Check cache first */
    if (elapsed < g_config.check_interval_seconds && g_last_check > 0) {
        return g_cached_status;
    }
    
    /* Fetch fresh status */
    backpressure_status_t status = fetch_router_metrics();
    
    /* Update cache */
    g_cached_status = status;
    g_last_check = now;
    
    return status;
}

/* Get cached backpressure status (without making HTTP request) */
backpressure_status_t backpressure_client_get_cached_status(void) {
    return g_cached_status;
}

