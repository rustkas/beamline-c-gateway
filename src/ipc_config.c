/**
 * ipc_config.c - IPC configuration implementation
 */

#include "ipc_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Default values */
#define DEFAULT_IPC_ENABLE           1
#define DEFAULT_SOCKET_PATH          "/tmp/beamline-gateway.sock"
#define DEFAULT_MAX_CONNECTIONS      64
#define DEFAULT_NATS_ENABLE          0
#define DEFAULT_NATS_URL             "nats://localhost:4222"
#define DEFAULT_ROUTER_SUBJECT       "beamline.router.v1.decide"
#define DEFAULT_TIMEOUT_MS           30000

/* Validation constants */
#define MIN_TIMEOUT_MS               100
#define MAX_TIMEOUT_MS               300000    /* 5 minutes */
#define MIN_MAX_CONNECTIONS          1
#define MAX_MAX_CONNECTIONS          1024

/**
 * Parse boolean from string
 */
static int parse_bool(const char *str, int *out) {
    if (!str) return -1;
    
    if (strcmp(str, "1") == 0 || strcasecmp(str, "true") == 0 || 
        strcasecmp(str, "yes") == 0 || strcasecmp(str, "on") == 0) {
        *out = 1;
        return 0;
    }
    
    if (strcmp(str, "0") == 0 || strcasecmp(str, "false") == 0 || 
        strcasecmp(str, "no") == 0 || strcasecmp(str, "off") == 0) {
        *out = 0;
        return 0;
    }
    
    return -1;
}

/**
 * Parse integer from string
 */
static int parse_int(const char *str, int *out) {
    if (!str) return -1;
    
    char *endptr;
    long val = strtol(str, &endptr, 10);
    
    if (*endptr != '\0' || endptr == str) {
        return -1;
    }
    
    *out = (int)val;
    return 0;
}

ipc_config_error_t ipc_config_load(ipc_config_t *config) {
    if (!config) {
        return IPC_CONFIG_ERR_INVALID_BOOL;
    }
    
    memset(config, 0, sizeof(*config));
    
    /* IPC_ENABLE */
    const char *ipc_enable_str = getenv("CGW_IPC_ENABLE");
    if (ipc_enable_str) {
        if (parse_bool(ipc_enable_str, &config->ipc_enable) < 0) {
            return IPC_CONFIG_ERR_INVALID_BOOL;
        }
    } else {
        config->ipc_enable = DEFAULT_IPC_ENABLE;
    }
    
    /* SOCKET_PATH */
    const char *socket_path = getenv("CGW_IPC_SOCKET_PATH");
    if (socket_path) {
        if (strlen(socket_path) >= sizeof(config->socket_path)) {
            return IPC_CONFIG_ERR_PATH_TOO_LONG;
        }
        strncpy(config->socket_path, socket_path, sizeof(config->socket_path) - 1);
    } else {
        strncpy(config->socket_path, DEFAULT_SOCKET_PATH, sizeof(config->socket_path) - 1);
    }
    
    /* MAX_CONNECTIONS */
    const char *max_conn_str = getenv("CGW_IPC_MAX_CONNECTIONS");
    if (max_conn_str) {
        if (parse_int(max_conn_str, &config->max_connections) < 0) {
            return IPC_CONFIG_ERR_INVALID_INT;
        }
    } else {
        config->max_connections = DEFAULT_MAX_CONNECTIONS;
    }
    
    /* NATS_ENABLE */
    const char *nats_enable_str = getenv("CGW_IPC_NATS_ENABLE");
    if (nats_enable_str) {
        if (parse_bool(nats_enable_str, &config->nats_enable) < 0) {
            return IPC_CONFIG_ERR_INVALID_BOOL;
        }
    } else {
        config->nats_enable = DEFAULT_NATS_ENABLE;
    }
    
    /* NATS_URL */
    const char *nats_url = getenv("CGW_IPC_NATS_URL");
    if (nats_url) {
        if (strlen(nats_url) >= sizeof(config->nats_url)) {
            return IPC_CONFIG_ERR_PATH_TOO_LONG;
        }
        strncpy(config->nats_url, nats_url, sizeof(config->nats_url) - 1);
    } else {
        strncpy(config->nats_url, DEFAULT_NATS_URL, sizeof(config->nats_url) - 1);
    }
    
    /* ROUTER_SUBJECT */
    const char *router_subject = getenv("CGW_IPC_ROUTER_SUBJECT");
    if (router_subject) {
        if (strlen(router_subject) >= sizeof(config->router_subject)) {
            return IPC_CONFIG_ERR_PATH_TOO_LONG;
        }
        strncpy(config->router_subject, router_subject, sizeof(config->router_subject) - 1);
    } else {
        strncpy(config->router_subject, DEFAULT_ROUTER_SUBJECT, sizeof(config->router_subject) - 1);
    }
    
    /* TIMEOUT_MS */
    const char *timeout_str = getenv("CGW_IPC_TIMEOUT_MS");
    if (timeout_str) {
        if (parse_int(timeout_str, &config->timeout_ms) < 0) {
            return IPC_CONFIG_ERR_INVALID_INT;
        }
    } else {
        config->timeout_ms = DEFAULT_TIMEOUT_MS;
    }
    
    config->validated = 0;
    return IPC_CONFIG_OK;
}

ipc_config_error_t ipc_config_validate(const ipc_config_t *config) {
    if (!config) {
        return IPC_CONFIG_ERR_INVALID_BOOL;
    }
    
    /* Validate boolean fields */
    if (config->ipc_enable != 0 && config->ipc_enable != 1) {
        return IPC_CONFIG_ERR_INVALID_BOOL;
    }
    
    if (config->nats_enable != 0 && config->nats_enable != 1) {
        return IPC_CONFIG_ERR_INVALID_BOOL;
    }
    
    /* Validate socket path */
    if (config->socket_path[0] == '\0') {
        return IPC_CONFIG_ERR_EMPTY_REQUIRED;
    }
    
    /* Validate max_connections */
    if (config->max_connections < MIN_MAX_CONNECTIONS || 
        config->max_connections > MAX_MAX_CONNECTIONS) {
        return IPC_CONFIG_ERR_OUT_OF_RANGE;
    }
    
    /* Validate NATS URL (if NATS enabled) */
    if (config->nats_enable && config->nats_url[0] == '\0') {
        return IPC_CONFIG_ERR_EMPTY_REQUIRED;
    }
    
    /* Validate router subject */
    if (config->router_subject[0] == '\0') {
        return IPC_CONFIG_ERR_EMPTY_REQUIRED;
    }
    
    /* Validate timeout */
    if (config->timeout_ms < MIN_TIMEOUT_MS || 
        config->timeout_ms > MAX_TIMEOUT_MS) {
        return IPC_CONFIG_ERR_OUT_OF_RANGE;
    }
    
    return IPC_CONFIG_OK;
}

void ipc_config_print(const ipc_config_t *config) {
    if (!config) {
        return;
    }
    
    printf("[ipc_config] Configuration:\n");
    printf("  ipc_enable:       %d\n", config->ipc_enable);
    printf("  socket_path:      %s\n", config->socket_path);
    printf("  max_connections:  %d\n", config->max_connections);
    printf("  nats_enable:      %d\n", config->nats_enable);
    
    /* Sanitize NATS URL (hide credentials if present) */
    if (config->nats_url[0] != '\0') {
        char sanitized_url[256];
        strncpy(sanitized_url, config->nats_url, sizeof(sanitized_url) - 1);
        
        /* Simple sanitization: hide user:pass@ part */
        char *at_sign = strchr(sanitized_url, '@');
        if (at_sign) {
            char *scheme_end = strstr(sanitized_url, "://");
            if (scheme_end && scheme_end < at_sign) {
                printf("  nats_url:         %.*s://***@%s\n", 
                       (int)(scheme_end - sanitized_url), sanitized_url, at_sign + 1);
            } else {
                printf("  nats_url:         %s\n", config->nats_url);
            }
        } else {
            printf("  nats_url:         %s\n", config->nats_url);
        }
    }
    
    printf("  router_subject:   %s\n", config->router_subject);
    printf("  timeout_ms:       %d\n", config->timeout_ms);
    printf("  validated:        %d\n", config->validated);
}

const char* ipc_config_strerror(ipc_config_error_t err) {
    switch (err) {
        case IPC_CONFIG_OK:
            return "No error";
        case IPC_CONFIG_ERR_INVALID_BOOL:
            return "Invalid boolean value (expected: 0/1/true/false/yes/no)";
        case IPC_CONFIG_ERR_INVALID_INT:
            return "Invalid integer value";
        case IPC_CONFIG_ERR_OUT_OF_RANGE:
            return "Value out of valid range";
        case IPC_CONFIG_ERR_EMPTY_REQUIRED:
            return "Required field is empty";
        case IPC_CONFIG_ERR_PATH_TOO_LONG:
            return "Path exceeds maximum length";
        default:
            return "Unknown error";
    }
}
