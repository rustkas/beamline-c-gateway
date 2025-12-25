/**
 * health_check.c - Health check implementation with HTTP endpoints
 */

#define _GNU_SOURCE
#include "health_check.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_CHECKS 32
#define HTTP_BUFFER_SIZE 4096

/**
 * Registered health check
 */
typedef struct {
    char name[64];
    health_check_fn_t check_fn;
    int critical;  /* Affects readiness */
} registered_check_t;

/**
 * Health check system state
 */
static struct {
    registered_check_t checks[MAX_CHECKS];
    size_t num_checks;
    pthread_mutex_t lock;
    uint16_t port;
    int server_fd;
    pthread_t server_thread;
    int running;
} g_health = {
    .num_checks = 0,
    .port = 8080,
    .server_fd = -1,
    .running = 0
};

int health_check_init(uint16_t port) {
    pthread_mutex_init(&g_health.lock, NULL);
    g_health.port = port;
    g_health.num_checks = 0;
    return 0;
}

int health_check_register(
    const char *name,
    health_check_fn_t check_fn,
    int critical
) {
    if (!name || !check_fn) {
        return -1;
    }
    
    pthread_mutex_lock(&g_health.lock);
    
    if (g_health.num_checks >= MAX_CHECKS) {
        pthread_mutex_unlock(&g_health.lock);
        return -1;
    }
    
    registered_check_t *check = &g_health.checks[g_health.num_checks++];
    snprintf(check->name, sizeof(check->name), "%s", name);
    check->check_fn = check_fn;
    check->critical = critical;
    
    pthread_mutex_unlock(&g_health.lock);
    return 0;
}

int health_check_get_status(health_result_t *result) {
    if (!result) {
        return -1;
    }
    
    pthread_mutex_lock(&g_health.lock);
    
    int failed = 0;
    int degraded = 0;
    
    for (size_t i = 0; i < g_health.num_checks; i++) {
        registered_check_t *check = &g_health.checks[i];
        int rc = check->check_fn();
        
        if (rc != 0) {
            if (check->critical) {
                failed++;
            } else {
                degraded++;
            }
        }
    }
    
    if (failed > 0) {
        result->status = HEALTH_STATUS_UNHEALTHY;
        snprintf(result->message, sizeof(result->message),
                "Critical checks failed: %d", failed);
    } else if (degraded > 0) {
        result->status = HEALTH_STATUS_DEGRADED;
        snprintf(result->message, sizeof(result->message),
                "Non-critical checks failed: %d", degraded);
    } else {
        result->status = HEALTH_STATUS_HEALTHY;
        snprintf(result->message, sizeof(result->message), "All checks passing");
    }
    
    pthread_mutex_unlock(&g_health.lock);
    return 0;
}

int health_check_get_readiness(health_result_t *result) {
    if (!result) {
        return -1;
    }
    
    pthread_mutex_lock(&g_health.lock);
    
    int failed = 0;
    
    /* Only check critical checks for readiness */
    for (size_t i = 0; i < g_health.num_checks; i++) {
        registered_check_t *check = &g_health.checks[i];
        if (check->critical) {
            int rc = check->check_fn();
            if (rc != 0) {
                failed++;
            }
        }
    }
    
    if (failed > 0) {
        result->status = HEALTH_STATUS_UNHEALTHY;
        snprintf(result->message, sizeof(result->message),
                "Not ready: %d critical checks failing", failed);
    } else {
        result->status = HEALTH_STATUS_HEALTHY;
        snprintf(result->message, sizeof(result->message), "Ready");
    }
    
    pthread_mutex_unlock(&g_health.lock);
    return 0;
}

static void handle_health_request(int client_fd, const char *path) {
    char response[HTTP_BUFFER_SIZE];
    health_result_t result;
    int http_status = 200;
    
    if (strcmp(path, "/health") == 0) {
        /* Liveness: always healthy if process running */
        result.status = HEALTH_STATUS_HEALTHY;
        snprintf(result.message, sizeof(result.message), "Alive");
    } else if (strcmp(path, "/ready") == 0) {
        /* Readiness: check critical components */
        health_check_get_readiness(&result);
        if (result.status != HEALTH_STATUS_HEALTHY) {
            http_status = 503;  /* Service Unavailable */
        }
    } else {
        /* 404 Not Found */
        http_status = 404;
        snprintf(response, sizeof(response),
                "HTTP/1.1 404 Not Found\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 9\r\n"
                "\r\n"
                "Not Found");
        (void)write(client_fd, response, strlen(response));
        return;
    }
    
    /* Build JSON response */
    const char *status_str = "unknown";
    if (result.status == HEALTH_STATUS_HEALTHY) status_str = "healthy";
    else if (result.status == HEALTH_STATUS_DEGRADED) status_str = "degraded";
    else if (result.status == HEALTH_STATUS_UNHEALTHY) status_str = "unhealthy";
    
    char body[512];
    int body_len = snprintf(body, sizeof(body),
        "{\"status\":\"%s\",\"message\":\"%s\"}\n",
        status_str, result.message);
    
    snprintf(response, sizeof(response),
            "HTTP/1.1 %d %s\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %d\r\n"
            "\r\n"
            "%s",
            http_status,
            (http_status == 200) ? "OK" : "Service Unavailable",
            body_len,
            body);
    
    (void)write(client_fd, response, strlen(response));
}

static void* health_server_thread(void *arg) {
    (void)arg;
    
    while (g_health.running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_fd = accept(g_health.server_fd,
                              (struct sockaddr*)&client_addr,
                              &client_len);
        if (client_fd < 0) {
            if (g_health.running) {
                perror("accept");
            }
            continue;
        }
        
        /* Read HTTP request */
        char request[HTTP_BUFFER_SIZE];
        ssize_t n = read(client_fd, request, sizeof(request) - 1);
        if (n > 0) {
            request[n] = '\0';
            
            /* Parse path from "GET /path HTTP/1.1" */
            char method[16], path[256], version[16];
            if (sscanf(request, "%15s %255s %15s", method, path, version) == 3) {
                if (strcmp(method, "GET") == 0) {
                    handle_health_request(client_fd, path);
                }
            }
        }
        
        close(client_fd);
    }
    
    return NULL;
}

int health_check_start_server(void) {
    /* Create socket */
    g_health.server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_health.server_fd < 0) {
        perror("socket");
        return -1;
    }
    
    /* Set SO_REUSEADDR */
    int opt = 1;
    setsockopt(g_health.server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    /* Bind */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(g_health.port);
    
    if (bind(g_health.server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(g_health.server_fd);
        return -1;
    }
    
    /* Listen */
    if (listen(g_health.server_fd, 5) < 0) {
        perror("listen");
        close(g_health.server_fd);
        return -1;
    }
    
    /* Start server thread */
    g_health.running = 1;
    if (pthread_create(&g_health.server_thread, NULL, health_server_thread, NULL) != 0) {
        perror("pthread_create");
        close(g_health.server_fd);
        return -1;
    }
    
    printf("[health] HTTP server started on port %d\n", g_health.port);
    return 0;
}

void health_check_stop_server(void) {
    if (g_health.running) {
        g_health.running = 0;
        
        /* Close server socket to unblock accept() */
        if (g_health.server_fd >= 0) {
            close(g_health.server_fd);
            g_health.server_fd = -1;
        }
        
        pthread_join(g_health.server_thread, NULL);
    }
}

void health_check_shutdown(void) {
    health_check_stop_server();
    pthread_mutex_destroy(&g_health.lock);
}
