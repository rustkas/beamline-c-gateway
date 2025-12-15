/* registry block moved below includes */
#define _POSIX_C_SOURCE 200809L  /* For strdup, strcasecmp, strtok_r */
#include "http_server.h"

#include "nats_client_stub.h"
#include "metrics/metrics_registry.h"
#include "handlers/metrics_handler.h"
#include "tracing/otel.h"
#include "rate_limiter.h"
#include "abuse_detection.h"
#include "backpressure_client.h"
#include "redis_rate_limiter.h"
#include <sys/time.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>  /* For strcasecmp */
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#include <jansson.h>

/* Request context available for prototypes below */
typedef struct {
    char request_id[64];
    char trace_id[64];
    char tenant_id[64];
    char run_id[64];
    otel_span_t *otel_span;  // OpenTelemetry span for this request
} request_context_t;

/* NATS status (implemented in nats_client_stub/real) */
const char *nats_get_status_string(void);

/* Forward declarations to avoid implicit prototypes */
static json_t *filter_pii_json(json_t *obj);
static json_t *filter_pii_json_array(json_t *arr);
int map_router_error_status(const char *resp_json);

/* ---------------- SSE clients pool (non-blocking, simple) ---------------- */
#define SSE_MAX_CLIENTS 64
typedef struct {
    int fd;
    char tenant_id[64];
    time_t last_write;
} sse_client_t;

static sse_client_t sse_clients[SSE_MAX_CLIENTS];

static void sse_init_pool(void)
{
    for (int i=0;i<SSE_MAX_CLIENTS;i++) { sse_clients[i].fd = -1; sse_clients[i].tenant_id[0]='\0'; sse_clients[i].last_write=0; }
}

static void sse_unregister_fd(int fd)
{
    for (int i=0;i<SSE_MAX_CLIENTS;i++) {
        if (sse_clients[i].fd == fd) {
            close(sse_clients[i].fd);
            sse_clients[i].fd = -1;
            sse_clients[i].tenant_id[0] = '\0';
            sse_clients[i].last_write = 0;
            break;
        }
    }
}

static int sse_register_client(int client_fd, const char *tenant_id)
{
    /* set non-blocking */
    int flags = fcntl(client_fd, F_GETFL, 0);
    if (flags != -1) (void)fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

    const char *hdr =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/event-stream\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: keep-alive\r\n"
        "Access-Control-Allow-Origin: *\r\n\r\n";
    (void)write(client_fd, hdr, strlen(hdr));
    /* initial ping/comment to flush proxies */
    (void)write(client_fd, ": connected\n\n", strlen(": connected\n\n"));

    for (int i=0;i<SSE_MAX_CLIENTS;i++) {
        if (sse_clients[i].fd == -1) {
            sse_clients[i].fd = client_fd;
            strncpy(sse_clients[i].tenant_id, tenant_id, sizeof(sse_clients[i].tenant_id)-1);
            sse_clients[i].tenant_id[sizeof(sse_clients[i].tenant_id)-1] = '\0';
            sse_clients[i].last_write = time(NULL);
            return 0;
        }
    }
    /* pool full */
    const char *msg = ":pool_full\n\n";
    (void)write(client_fd, msg, strlen(msg));
    return -1;
}

static void sse_broadcast_json(const char *tenant_id, const char *event, const char *json)
{
    char line1[128];
    int l1 = snprintf(line1, sizeof(line1), "event: %s\n", event);
    for (int i=0;i<SSE_MAX_CLIENTS;i++) {
        if (sse_clients[i].fd == -1) continue;
        if (tenant_id && tenant_id[0] && strcmp(tenant_id, sse_clients[i].tenant_id) != 0) continue;
        int fd = sse_clients[i].fd;
        if (write(fd, line1, (size_t)l1) < 0) { sse_unregister_fd(fd); continue; }
        if (write(fd, "data: ", 6) < 0) { sse_unregister_fd(fd); continue; }
        if (write(fd, json, strlen(json)) < 0) { sse_unregister_fd(fd); continue; }
        if (write(fd, "\n\n", 2) < 0) { sse_unregister_fd(fd); continue; }
        sse_clients[i].last_write = time(NULL);
    }
}

static const char *query_get_param(const char *path_with_query, const char *key, char *buf, size_t buflen)
{
    const char *q = strchr(path_with_query, '?');
    if (!q) return NULL;
    q++;
    size_t keylen = strlen(key);
    while (*q) {
        const char *eq = strchr(q, '=');
        if (!eq) break;
        const char *amp = strchr(q, '&');
        if (!amp) amp = q + strlen(q);
        if ((size_t)(eq - q) == keylen && strncmp(q, key, keylen) == 0) {
            size_t vlen = (size_t)(amp - (eq + 1));
            if (vlen >= buflen) vlen = buflen - 1;
            strncpy(buf, eq + 1, vlen); buf[vlen] = '\0';
            return buf;
        }
        q = (*amp) ? amp + 1 : amp;
    }
    return NULL;
}

/* Forward declarations for helpers used before their definitions */
static void send_response(int client_fd, const char *status_line,
                          const char *content_type, const char *body);
static void send_response_with_headers(int client_fd, const char *status_line,
                                        const char *extra_headers, const char *body);
static void send_error_response(int client_fd,
                                const char *status_line,
                                const char *error_code,
                                const char *message,
                                const request_context_t *ctx);
static void send_error_response_with_retry_after(int client_fd,
                                                  const char *status_line,
                                                  const char *error_code,
                                                  const char *message,
                                                  int retry_after_seconds,
                                                  const request_context_t *ctx);

/* Forward declarations for conflict-aware error handling */
/* Note: conflict_type_t is defined later in the file, so we use a forward declaration */
typedef enum {
    CONFLICT_TYPE_NONE,
    CONFLICT_TYPE_RATE_LIMIT,
    CONFLICT_TYPE_AUTH_GATEWAY,
    CONFLICT_TYPE_REQUEST_GATEWAY,
    CONFLICT_TYPE_ROUTER_INTAKE,
    CONFLICT_TYPE_ROUTER_RUNTIME,
    CONFLICT_TYPE_INTERNAL_GATEWAY
} conflict_type_t;

static void log_error_with_conflict_info(const char *stage,
                                         const request_context_t *ctx,
                                         const char *code,
                                         const char *message,
                                         conflict_type_t conflict_type,
                                         const char *intake_error_code,
                                         int http_status);
static void send_error_response_with_conflict(int client_fd,
                                              const char *status_line,
                                              const char *error_code,
                                              const char *message,
                                              const request_context_t *ctx,
                                              conflict_type_t conflict_type,
                                              const char *intake_error_code);

/* ---------------- Extensions Registry (in-memory, CP1) ---------------- */
typedef struct {
    char type[128];
    char version[64];
    char *manifest_json; /* owned heap json string */
} block_entry_t;

#define REGISTRY_CAP 64
static block_entry_t registry_entries[REGISTRY_CAP];
static int registry_count = 0;

static int registry_find(const char *type, const char *version)
{
    for (int i = 0; i < registry_count; i++) {
        if (strcmp(registry_entries[i].type, type) == 0 && strcmp(registry_entries[i].version, version) == 0) {
            return i;
        }
    }
    return -1;
}

static int registry_upsert(const char *type, const char *version, const char *manifest_json, int *created)
{
    int idx = registry_find(type, version);
    if (idx >= 0) {
        /* update */
        free(registry_entries[idx].manifest_json);
        registry_entries[idx].manifest_json = strdup(manifest_json);
        if (created) *created = 0;
        return 0;
    }
    if (registry_count >= REGISTRY_CAP) return -1;
    strncpy(registry_entries[registry_count].type, type, sizeof(registry_entries[registry_count].type)-1);
    registry_entries[registry_count].type[sizeof(registry_entries[registry_count].type)-1] = '\0';
    strncpy(registry_entries[registry_count].version, version, sizeof(registry_entries[registry_count].version)-1);
    registry_entries[registry_count].version[sizeof(registry_entries[registry_count].version)-1] = '\0';
    registry_entries[registry_count].manifest_json = strdup(manifest_json);
    registry_count++;
    if (created) *created = 1;
    return 0;
}

static int registry_delete(const char *type, const char *version)
{
    int idx = registry_find(type, version);
    if (idx < 0) return -1;
    free(registry_entries[idx].manifest_json);
    registry_entries[idx] = registry_entries[registry_count - 1];
    registry_count--;
    return 0;
}

/* ---------------- Strict JSON Schema validator (Draft-07 subset) in C ---------------- */
static int is_valid_type_string(const char *s)
{
    static const char *allowed[] = {"object","array","string","number","integer","boolean","null"};
    for (size_t i=0;i<sizeof(allowed)/sizeof(allowed[0]);i++) if (strcmp(s, allowed[i])==0) return 1;
    return 0;
}

static int validate_jsonschema_node(json_t *node, int depth);

static int validate_string_array(json_t *arr)
{
    if (!json_is_array(arr)) return 0;
    size_t i, n = json_array_size(arr);
    for (i=0;i<n;i++) if (!json_is_string(json_array_get(arr, i))) return 0;
    return 1;
}

static int validate_type(json_t *type)
{
    if (json_is_string(type)) return is_valid_type_string(json_string_value(type));
    if (json_is_array(type)) {
        size_t i, n = json_array_size(type);
        if (n==0) return 0;
        for (i=0;i<n;i++) {
            json_t *v = json_array_get(type, i);
            if (!json_is_string(v) || !is_valid_type_string(json_string_value(v))) return 0;
        }
        return 1;
    }
    return 0;
}

static int validate_properties(json_t *props, int depth)
{
    if (!json_is_object(props)) return 0;
    const char *key; json_t *val;
    json_object_foreach(props, key, val) {
        if (!json_is_object(val)) return 0;
        if (!validate_jsonschema_node(val, depth+1)) return 0;
    }
    return 1;
}

static int validate_composition_array(json_t *arr, int depth)
{
    if (!json_is_array(arr)) return 0;
    size_t i, n = json_array_size(arr);
    if (n==0) return 0;
    for (i=0;i<n;i++) {
        json_t *v = json_array_get(arr, i);
        if (!json_is_object(v) || !validate_jsonschema_node(v, depth+1)) return 0;
    }
    return 1;
}

static int validate_jsonschema_node(json_t *node, int depth)
{
    if (depth > 64) return 0;
    if (!json_is_object(node)) return 0;

    /* $schema (optional) */
    json_t *js = json_object_get(node, "$schema");
    if (js && (!json_is_string(js) || strstr(json_string_value(js), "draft-07") == NULL)) return 0;

    /* type */
    json_t *jtype = json_object_get(node, "type");
    if (jtype && !validate_type(jtype)) return 0;

    /* properties */
    json_t *jprops = json_object_get(node, "properties");
    if (jprops && !validate_properties(jprops, depth)) return 0;

    /* required */
    json_t *jreq = json_object_get(node, "required");
    if (jreq && !validate_string_array(jreq)) return 0;

    /* items */
    json_t *jitems = json_object_get(node, "items");
    if (jitems) {
        if (json_is_object(jitems)) { if (!validate_jsonschema_node(jitems, depth+1)) return 0; }
        else return 0;
    }

    /* additionalProperties */
    json_t *jaddp = json_object_get(node, "additionalProperties");
    if (jaddp && !(json_is_boolean(jaddp) || (json_is_object(jaddp) && validate_jsonschema_node(jaddp, depth+1)))) return 0;

    /* enum */
    json_t *jenum = json_object_get(node, "enum");
    if (jenum && !json_is_array(jenum)) return 0;

    /* oneOf/anyOf/allOf */
    json_t *jone = json_object_get(node, "oneOf");
    if (jone && !validate_composition_array(jone, depth)) return 0;
    json_t *jany = json_object_get(node, "anyOf");
    if (jany && !validate_composition_array(jany, depth)) return 0;
    json_t *jall = json_object_get(node, "allOf");
    if (jall && !validate_composition_array(jall, depth)) return 0;

    /* simple numeric/string constraints if present */
    json_t *jmin = json_object_get(node, "minimum"); if (jmin && !json_is_number(jmin)) return 0;
    json_t *jmax = json_object_get(node, "maximum"); if (jmax && !json_is_number(jmax)) return 0;
    json_t *jminl = json_object_get(node, "minLength"); if (jminl && !json_is_integer(jminl)) return 0;
    json_t *jmaxl = json_object_get(node, "maxLength"); if (jmaxl && !json_is_integer(jmaxl)) return 0;
    json_t *jformat = json_object_get(node, "format"); if (jformat && !json_is_string(jformat)) return 0;
    json_t *jref = json_object_get(node, "$ref"); if (jref && !json_is_string(jref)) return 0;

    /* definitions/$defs can be objects; skip deep validation for brevity */
    json_t *jdefs = json_object_get(node, "definitions"); if (jdefs && !json_is_object(jdefs)) return 0;
    json_t *jdefs2 = json_object_get(node, "$defs"); if (jdefs2 && !json_is_object(jdefs2)) return 0;

    return 1;
}

static int validate_jsonschemas_strict(json_t *schema_input, json_t *schema_output)
{
    if (!json_is_object(schema_input) || !json_is_object(schema_output)) return -1;
    return (validate_jsonschema_node(schema_input, 0) && validate_jsonschema_node(schema_output, 0)) ? 0 : -1;
}

static int handle_registry_write_common(int client_fd, const char *method,
                                        const char *type, const char *version,
                                        const char *body)
{
    (void)method; /* unused parameter */
    /* parse body */
    json_error_t jerr; json_t *root = json_loads(body, 0, &jerr);
    if (!root || !json_is_object(root)) { if (root) json_decref(root); send_error_response(client_fd, "HTTP/1.1 400 Bad Request","invalid_request","invalid JSON", NULL); return -1; }

    /* Validate body.type/version match path */
    const char *type_b = NULL; const char *version_b = NULL;
    json_t *t = json_object_get(root, "type"); if (json_is_string(t)) type_b = json_string_value(t);
    json_t *v = json_object_get(root, "version"); if (json_is_string(v)) version_b = json_string_value(v);
    if (!type_b || !version_b || strcmp(type_b, type)!=0 || strcmp(version_b, version)!=0) {
        json_decref(root);
        send_error_response(client_fd, "HTTP/1.1 409 Conflict","conflict","type/version mismatch with path", NULL);
        return -1;
    }

    /* Optional: validate capabilities (array of strings) and metadata (object) */
    json_t *cap = json_object_get(root, "capabilities");
    if (cap && !json_is_array(cap)) { json_decref(root); send_error_response(client_fd, "HTTP/1.1 400 Bad Request","invalid_request","capabilities must be array", NULL); return -1; }
    if (cap) {
        size_t i, n = json_array_size(cap);
        for (i=0;i<n;i++) { if (!json_is_string(json_array_get(cap, i))) { json_decref(root); send_error_response(client_fd, "HTTP/1.1 400 Bad Request","invalid_request","capabilities items must be strings", NULL); return -1; } }
    }
    json_t *meta = json_object_get(root, "metadata");
    if (meta && !json_is_object(meta)) { json_decref(root); send_error_response(client_fd, "HTTP/1.1 400 Bad Request","invalid_request","metadata must be object", NULL); return -1; }

    /* Strict validate JSON Schema fields */
    json_t *schema = json_object_get(root, "schema");
    json_t *schema_input = schema ? json_object_get(schema, "input") : NULL;
    json_t *schema_output = schema ? json_object_get(schema, "output") : NULL;
    if (validate_jsonschemas_strict(schema_input, schema_output) != 0) {
        json_decref(root);
        send_error_response(client_fd, "HTTP/1.1 400 Bad Request","invalid_schema","schema validation failed", NULL);
        return -1;
    }

    /* Store manifest */
    char *manifest_json = json_dumps(root, JSON_COMPACT);
    json_decref(root);
    if (!manifest_json) { send_error_response(client_fd, "HTTP/1.1 500 Internal Server Error","internal","failed to serialize manifest", NULL); return -1; }

    int created = 0; int rc = registry_upsert(type, version, manifest_json, &created);
    free(manifest_json);
    if (rc != 0) { send_error_response(client_fd, "HTTP/1.1 500 Internal Server Error","internal","registry capacity reached", NULL); return -1; }

    /* Build response */
    char resp[256]; time_t now = time(NULL);
    snprintf(resp, sizeof(resp), "{\"type\":\"%s\",\"version\":\"%s\",\"status\":\"%s\",\"ts\":%ld}", type, version, created?"created":"updated", (long)now);
    send_response(client_fd, created?"HTTP/1.1 201 Created":"HTTP/1.1 200 OK", "application/json", resp);
    return 0;
}

static void handle_registry_delete(int client_fd, const char *type, const char *version)
{
    if (registry_delete(type, version) != 0) {
        send_error_response(client_fd, "HTTP/1.1 404 Not Found","not_found","block not found", NULL);
        return;
    }
    char resp[256]; time_t now = time(NULL);
    snprintf(resp, sizeof(resp), "{\"status\":\"unregistered\",\"type\":\"%s\",\"version\":\"%s\",\"ts\":%ld}", type, version, (long)now);
    send_response(client_fd, "HTTP/1.1 200 OK", "application/json", resp);
}

#define MAX_REQUEST_SIZE  65536U
#define MAX_RESPONSE_SIZE 65536U

/* request_context_t moved above */

typedef enum {
    ENDPOINT_UNKNOWN = 0,
    ENDPOINT_HEALTH,
    ENDPOINT_METRICS_JSON,
    ENDPOINT_METRICS,
    ENDPOINT_REGISTRY_POST,
    ENDPOINT_REGISTRY_PUT,
    ENDPOINT_REGISTRY_DELETE,
    ENDPOINT_ROUTES_DECIDE_POST,
    ENDPOINT_ROUTES_DECIDE_GET,
} endpoint_id_t;

/*
 * Rate limiting for CP1/CP2+.
 *
 * CP1: Simple in-memory fixed-window rate limiting (default).
 * CP2+: Distributed rate limiting with Redis backend (optional, via feature flags).
 *
 * Configuration:
 * - GATEWAY_DISTRIBUTED_RATE_LIMIT_ENABLED=true to enable distributed mode
 * - GATEWAY_RATE_LIMIT_BACKEND=redis|memory (default: memory)
 * - GATEWAY_RATE_LIMIT_REDIS_HOST, GATEWAY_RATE_LIMIT_REDIS_PORT for Redis
 * - GATEWAY_RATE_LIMIT_FALLBACK_TO_LOCAL=true (default) for automatic fallback
 */

static void send_rate_limit_error(int client_fd,
                                  rl_endpoint_id_t endpoint,
                                  const request_context_t *ctx);

/* Global rate limiter instance */
static rate_limiter_t *g_rate_limiter = NULL;
static int rl_initialized = 0;
static const char *rl_mode = "unknown"; /* "memory" | "redis" | "fallback" */

/* Rate limiting metrics (for backward compatibility) */
static unsigned long rl_total_hits = 0;
static unsigned long rl_total_exceeded = 0;
static unsigned long rl_exceeded_by_endpoint[RL_ENDPOINT_MAX] = {0};

/*
 * Minimal auth skeleton for CP1.
 *
 * If GATEWAY_AUTH_REQUIRED=true, C-Gateway will require an Authorization
 * header for API calls and return 401 if it is missing. This prepares
 * the surface for future Router Admin integration without implementing
 * full JWT/API-key validation yet.
 */

static int auth_required = 0;

static int env_to_int(const char *name, int def_val)
{
    const char *val = getenv(name);
    if (val == NULL || *val == '\0') {
        return def_val;
    }
    int parsed = atoi(val);
    if (parsed <= 0) {
        return def_val;
    }
    return parsed;
}

/* JSON metrics endpoint implemented later (after helpers) */

static int env_to_bool(const char *name, int def_val)
{
    const char *val = getenv(name);
    if (val == NULL || *val == '\0') {
        return def_val;
    }
    if (strcmp(val, "1") == 0 || strcasecmp(val, "true") == 0 || strcasecmp(val, "yes") == 0) {
        return 1;
    }
    if (strcmp(val, "0") == 0 || strcasecmp(val, "false") == 0 || strcasecmp(val, "no") == 0) {
        return 0;
    }
    return def_val;
}

/* Initialize rate limiter (CP1/CP2+ compatible) */
static void rate_limit_init_if_needed(void)
{
    if (rl_initialized) {
        return;
    }

    /* Parse configuration from environment */
    distributed_rl_config_t config;
    if (rate_limiter_parse_config(&config) != 0) {
        /* Fallback to defaults */
        rate_limiter_get_default_config(&config);
    }

    /* Create rate limiter based on configuration */
    g_rate_limiter = rate_limiter_create(&config);
    if (!g_rate_limiter) {
        fprintf(stderr, "ERROR: Failed to create rate limiter, using memory fallback\n");
        /* Fallback to memory mode */
        distributed_rl_config_t default_config;
        rate_limiter_get_default_config(&default_config);
        default_config.enabled = 0; /* Force memory mode */
        g_rate_limiter = rate_limiter_create(&default_config);
        rl_mode = "memory";
    } else {
        /* Determine mode */
        if (config.enabled && config.backend && strcmp(config.backend, "redis") == 0) {
            rl_mode = "redis";
        } else {
            rl_mode = "memory";
        }
        fprintf(stderr, "INFO: Rate limiter initialized in %s mode\n", rl_mode);
    }

    /* Initialize rate limiter */
    if (g_rate_limiter && g_rate_limiter->init) {
        if (g_rate_limiter->init(g_rate_limiter, &config) != 0) {
            fprintf(stderr, "WARNING: Rate limiter init failed (requested: %s), checking fallback\n", rl_mode);
            
            /* Check if fallback is enabled */
            if (config.fallback_to_local) {
                fprintf(stderr, "INFO: Fallback to local mode enabled, switching to memory mode\n");
                /* Destroy failed limiter and create memory fallback */
                if (g_rate_limiter->cleanup) {
                    g_rate_limiter->cleanup(g_rate_limiter);
                }
                free(g_rate_limiter);
                
                /* Create memory fallback */
                distributed_rl_config_t fallback_config;
                rate_limiter_get_default_config(&fallback_config);
                fallback_config.enabled = 0; /* Force memory mode */
                g_rate_limiter = rate_limiter_create(&fallback_config);
                if (g_rate_limiter && g_rate_limiter->init) {
                    g_rate_limiter->init(g_rate_limiter, &fallback_config);
                }
                rl_mode = "fallback";
                fprintf(stderr, "INFO: Rate limiter fallback to memory mode successful\n");
            } else {
                fprintf(stderr, "ERROR: Rate limiter init failed and fallback disabled, rate limiting may not work\n");
                rl_mode = "error";
            }
        } else {
            /* Log successful initialization with mode */
            fprintf(stderr, "INFO: Rate limiter initialized successfully in %s mode\n", rl_mode);
            if (config.enabled && config.backend && strcmp(config.backend, "redis") == 0) {
                fprintf(stderr, "INFO: Redis backend: %s:%d (timeout: %dms)\n", 
                        config.redis_host ? config.redis_host : "localhost",
                        config.redis_port,
                        config.redis_timeout_ms);
            }
        }
    }

    rl_initialized = 1;
    auth_required = env_to_bool("GATEWAY_AUTH_REQUIRED", 0);
    
    /* Initialize abuse detection */
    abuse_detection_config_t abuse_config;
    if (abuse_detection_parse_config(&abuse_config) == 0) {
        abuse_detection_init(&abuse_config);
    }
    
    /* Initialize backpressure client */
    backpressure_client_config_t backpressure_config;
    if (backpressure_client_parse_config(&backpressure_config) == 0) {
        backpressure_client_init(&backpressure_config);
    }
    
    /* Initialize Redis rate limiter (PoC) */
    redis_rl_config_t redis_rl_config;
    if (redis_rate_limiter_parse_config(&redis_rl_config) == 0) {
        if (redis_rate_limiter_init(&redis_rl_config) != 0) {
            fprintf(stderr, "Warning: Failed to initialize Redis rate limiter\n");
        } else {
            /* Register cleanup on exit */
            atexit(redis_rate_limiter_cleanup);
        }
    }
}

/* Get endpoint limit (for headers) */
static int get_endpoint_limit(rl_endpoint_id_t endpoint) {
    const char *env_var = NULL;
    int default_limit = 0;
    
    switch (endpoint) {
        case RL_ENDPOINT_ROUTES_DECIDE:
            env_var = "GATEWAY_RATE_LIMIT_ROUTES_DECIDE_LIMIT";
            default_limit = 50;
            break;
        case RL_ENDPOINT_MESSAGES:
            env_var = "GATEWAY_RATE_LIMIT_MESSAGES";
            default_limit = 100;
            break;
        case RL_ENDPOINT_REGISTRY_BLOCKS:
            env_var = "GATEWAY_RATE_LIMIT_REGISTRY_BLOCKS";
            default_limit = 200;
            break;
        default:
            return 1000; /* Global limit */
    }
    
    if (env_var) {
        return env_to_int(env_var, default_limit);
    }
    return default_limit;
}

/* Get TTL for reset header */
static int get_ttl_seconds(void) {
    return env_to_int("GATEWAY_RATE_LIMIT_TTL_SECONDS", 60);
}

static void add_rate_limit_headers(char *header_buf, size_t buf_size, 
                                     int limit, unsigned int remaining) {
    time_t now = time(NULL);
    int ttl = get_ttl_seconds();
    time_t reset_at = now + ttl;
    int retry_after = ttl;
    
    snprintf(header_buf, buf_size,
        "X-RateLimit-Limit: %d\r\n"
        "X-RateLimit-Remaining: %u\r\n"
        "X-RateLimit-Reset: %ld\r\n"
        "Retry-After: %d\r\n",
        limit, remaining, (long)reset_at, retry_after);
}

/* Check rate limit using rate_limiter API (CP1/CP2+ compatible) */
/* Returns: 0 = allowed, 1 = rate limit exceeded, 2 = error */
static int rate_limit_check(rl_endpoint_id_t endpoint, const char *tenant_id, const char *api_key, unsigned int *remaining_out) {
    rate_limit_init_if_needed();
    
    if (!g_rate_limiter || !g_rate_limiter->check) {
        /* Fallback: allow request if rate limiter unavailable */
        fprintf(stderr, "WARNING: Rate limiter unavailable, allowing request\n");
        if (remaining_out) *remaining_out = 0;
        return 0;
    }
    
    /* Track total rate limit checks */
    rl_total_hits++;
    
    /* Call rate limiter check */
    rl_result_t result = g_rate_limiter->check(g_rate_limiter, endpoint, tenant_id, api_key, remaining_out);
    
    switch (result) {
        case RL_ALLOWED:
            metrics_record_rate_limit_allowed();
            return 0; /* Allowed */
            
        case RL_EXCEEDED:
            rl_total_exceeded++;
            rl_exceeded_by_endpoint[endpoint]++;
            metrics_record_rate_limit_hit(tenant_id);
            if (remaining_out) *remaining_out = 0;
            return 1; /* Rate limit exceeded */
            
        case RL_ERROR:
            /* Log error and check fallback behavior */
            fprintf(stderr, "WARNING: Rate limiter error (mode: %s, endpoint: %d), checking fallback\n", rl_mode, endpoint);
            
            /* If fallback enabled, allow request */
            const char *fallback_str = getenv("GATEWAY_RATE_LIMIT_FALLBACK_TO_LOCAL");
            int fallback_enabled = 1; /* Default: enabled */
            if (fallback_str && (strcmp(fallback_str, "false") == 0 || strcmp(fallback_str, "0") == 0)) {
                fallback_enabled = 0;
            }
            
            if (fallback_enabled) {
                /* Track fallback usage */
                static unsigned long fallback_count = 0;
                fallback_count++;
                if (fallback_count == 1 || (fallback_count % 100) == 0) {
                    fprintf(stderr, "INFO: Fallback to local mode enabled, allowing request (fallback count: %lu)\n", fallback_count);
                }
                
                /* Update mode if not already in fallback */
                if (strcmp(rl_mode, "fallback") != 0) {
                    rl_mode = "fallback";
                    fprintf(stderr, "INFO: Rate limiter mode changed to fallback due to errors\n");
                }
                
                if (remaining_out) *remaining_out = 0;
                return 0; /* Allow on fallback */
            } else {
                /* Reject on error if fallback disabled */
                fprintf(stderr, "ERROR: Rate limiter error and fallback disabled, rejecting request\n");
                if (remaining_out) *remaining_out = 0;
                return 2; /* Error */
            }
    }
    
    return 0; /* Default: allow */
}

static int rate_limit_check_routes_decide(int client_fd, const request_context_t *ctx)
{
    unsigned int remaining = 0;
    const char *tenant_id = (ctx && ctx->tenant_id[0] != '\0') ? ctx->tenant_id : NULL;
    const char *api_key = NULL; /* TODO: Extract from Authorization header if needed */
    
    int result = rate_limit_check(RL_ENDPOINT_ROUTES_DECIDE, tenant_id, api_key, &remaining);
    
    if (result == 1) {
        /* Rate limit exceeded - send 429 error */
        send_rate_limit_error(client_fd, RL_ENDPOINT_ROUTES_DECIDE, ctx);
    } else if (result == 2) {
        /* Rate limiter error - send 503 Service Unavailable */
        send_error_response(client_fd, "HTTP/1.1 503 Service Unavailable", 
                          "rate_limiter_error", 
                          "Rate limiter service unavailable", ctx);
    }
    
    return result;
}

/* Generate ISO 8601 timestamp with microseconds */
static void get_iso8601_timestamp(char *buf, size_t buflen)
{
    struct timeval tv;
    struct tm *tm_info;
    time_t now;
    
    gettimeofday(&tv, NULL);
    now = tv.tv_sec;
    tm_info = gmtime(&now);
    
    if (tm_info != NULL && buflen >= 32)
    {
        snprintf(buf, buflen, "%04d-%02d-%02dT%02d:%02d:%02d.%06ldZ",
                 tm_info->tm_year + 1900,
                 tm_info->tm_mon + 1,
                 tm_info->tm_mday,
                 tm_info->tm_hour,
                 tm_info->tm_min,
                 tm_info->tm_sec,
                 (long)tv.tv_usec);
    }
    else
    {
        snprintf(buf, buflen, "1970-01-01T00:00:00.000000Z");
    }
}

/* Check if field name is sensitive (case-insensitive) */
static int is_sensitive_field(const char *field_name)
{
    if (field_name == NULL) return 0;
    
    /* List of sensitive field names to filter */
    const char *sensitive_fields[] = {
        "password", "api_key", "secret", "token", "access_token",
        "refresh_token", "authorization", "credit_card", "ssn", "email", "phone",
        /* Header-like fields (case variations) */
        "bearer", "x-api-key", "x-auth-token", "x-authorization"
    };
    size_t num_fields = sizeof(sensitive_fields) / sizeof(sensitive_fields[0]);
    
    /* Case-insensitive comparison */
    for (size_t i = 0; i < num_fields; i++)
    {
        if (strcasecmp(field_name, sensitive_fields[i]) == 0)
        {
            return 1;
        }
    }
    
    return 0;
}

/* Filter PII/sensitive data from string (simple keyword-based filtering for messages) */
static void filter_pii(const char *input, char *output, size_t outlen)
{
    if (input == NULL || output == NULL || outlen == 0)
    {
        if (output && outlen > 0) output[0] = '\0';
        return;
    }
    
    /* List of sensitive keywords to filter */
    const char *sensitive_keywords[] = {
        "password", "api_key", "secret", "token", "access_token",
        "refresh_token", "authorization", "credit_card", "ssn", "email", "phone"
    };
    size_t num_keywords = sizeof(sensitive_keywords) / sizeof(sensitive_keywords[0]);
    
    /* Simple check: if input contains any sensitive keyword, mask it */
    int should_mask = 0;
    for (size_t i = 0; i < num_keywords; i++)
    {
        if (strstr(input, sensitive_keywords[i]) != NULL)
        {
            should_mask = 1;
            break;
        }
    }
    
    if (should_mask)
    {
        snprintf(output, outlen, "[REDACTED]");
    }
    else
    {
        strncpy(output, input, outlen - 1);
        output[outlen - 1] = '\0';
    }
}

/* Filter PII from JSON array recursively */
static json_t *filter_pii_json_array(json_t *arr)
{
    if (!json_is_array(arr)) return arr;
    
    json_t *filtered = json_array();
    if (filtered == NULL) return arr;
    
    size_t index;
    json_t *value;
    json_array_foreach(arr, index, value)
    {
        if (json_is_object(value))
        {
            /* filter_pii_json returns new object, use append_new to transfer ownership */
            json_t *filtered_value = filter_pii_json(value);
            json_array_append_new(filtered, filtered_value);
        }
        else if (json_is_array(value))
        {
            /* filter_pii_json_array returns new array, use append_new to transfer ownership */
            json_t *filtered_value = filter_pii_json_array(value);
            json_array_append_new(filtered, filtered_value);
        }
        else
        {
            /* For primitive values, use append (no ownership transfer) */
            json_array_append(filtered, value);
        }
    }
    
    return filtered;
}

/* Filter PII from JSON object recursively */
static json_t *filter_pii_json(json_t *obj)
{
    if (obj == NULL) return NULL;
    
    /* If not an object, return as-is (or filter array) */
    if (!json_is_object(obj))
    {
        if (json_is_array(obj))
        {
            return filter_pii_json_array(obj);
        }
        return obj;
    }
    
    /* Create new filtered object */
    json_t *filtered = json_object();
    if (filtered == NULL) return obj;
    
    const char *key;
    json_t *value;
    
    json_object_foreach(obj, key, value)
    {
        /* Check if field name is sensitive */
        if (is_sensitive_field(key))
        {
            /* Replace sensitive field with [REDACTED] */
            json_object_set_new(filtered, key, json_string("[REDACTED]"));
        }
        else if (json_is_object(value))
        {
            /* Recursively filter nested objects */
            json_t *filtered_value = filter_pii_json(value);
            /* Use set_new to transfer ownership of the new filtered object */
            json_object_set_new(filtered, key, filtered_value);
        }
        else if (json_is_array(value))
        {
            /* Recursively filter arrays */
            json_t *filtered_value = filter_pii_json_array(value);
            /* Use set_new to transfer ownership of the new filtered array */
            json_object_set_new(filtered, key, filtered_value);
        }
        else
        {
            /* Copy non-sensitive fields as-is */
            json_object_set(filtered, key, value);
        }
    }
    
    return filtered;
}

/* Conflict contract error types */
/* conflict_type_t is already defined in forward declarations above */

/* Get error type string from conflict type */
static const char *get_error_type_string(conflict_type_t type) {
    switch (type) {
        case CONFLICT_TYPE_RATE_LIMIT: return "rate_limit";
        case CONFLICT_TYPE_AUTH_GATEWAY: return "auth_gateway";
        case CONFLICT_TYPE_REQUEST_GATEWAY: return "request_gateway";
        case CONFLICT_TYPE_ROUTER_INTAKE: return "router_intake";
        case CONFLICT_TYPE_ROUTER_RUNTIME: return "router_runtime";
        case CONFLICT_TYPE_INTERNAL_GATEWAY: return "internal_gateway";
        default: return "unknown";
    }
}

/* Get severity from conflict type */
static const char *get_severity_from_type(conflict_type_t type) {
    switch (type) {
        case CONFLICT_TYPE_RATE_LIMIT:
        case CONFLICT_TYPE_AUTH_GATEWAY:
        case CONFLICT_TYPE_REQUEST_GATEWAY:
            return "WARN";
        case CONFLICT_TYPE_ROUTER_INTAKE:
        case CONFLICT_TYPE_ROUTER_RUNTIME:
        case CONFLICT_TYPE_INTERNAL_GATEWAY:
            return "ERROR";
        default:
            return "ERROR";
    }
}

/* log_error is kept for backward compatibility but marked as potentially unused */
static void __attribute__((unused)) log_error(const char *stage,
                      const request_context_t *ctx,
                      const char *code,
                      const char *message)
{
    log_error_with_conflict_info(stage, ctx, code, message, 
                                  CONFLICT_TYPE_INTERNAL_GATEWAY, NULL, 0);
}

/* Enhanced log_error with conflict contract fields */
static void log_error_with_conflict_info(const char *stage,
                                         const request_context_t *ctx,
                                         const char *code,
                                         const char *message,
                                         conflict_type_t conflict_type,
                                         const char *intake_error_code,
                                         int http_status)
{
    char timestamp[32];
    get_iso8601_timestamp(timestamp, sizeof(timestamp));
    
    const char *rid = (ctx != NULL && ctx->request_id[0] != '\0')
                          ? ctx->request_id
                          : "";
    const char *tid = (ctx != NULL && ctx->trace_id[0] != '\0')
                          ? ctx->trace_id
                          : "";
    const char *ten = (ctx != NULL && ctx->tenant_id[0] != '\0')
                          ? ctx->tenant_id
                          : "";
    const char *run = (ctx != NULL && ctx->run_id[0] != '\0')
                          ? ctx->run_id
                          : "";
    
    char filtered_message[512];
    filter_pii(message, filtered_message, sizeof(filtered_message));

    /* Build JSON log entry using jansson */
    json_t *log_entry = json_object();
    if (log_entry == NULL) return;
    
    json_object_set_new(log_entry, "timestamp", json_string(timestamp));
    json_object_set_new(log_entry, "level", json_string(get_severity_from_type(conflict_type)));
    json_object_set_new(log_entry, "component", json_string("c-gateway"));
    json_object_set_new(log_entry, "subsystem", json_string(stage ? stage : "http_response"));
    json_object_set_new(log_entry, "message", json_string(filtered_message));
    
    /* Conflict contract fields */
    json_object_set_new(log_entry, "severity", json_string(get_severity_from_type(conflict_type)));
    json_object_set_new(log_entry, "error_type", json_string(get_error_type_string(conflict_type)));
    json_object_set_new(log_entry, "http_status", json_integer(http_status));
    json_object_set_new(log_entry, "gateway_error_code", json_string(code ? code : "internal"));
    json_object_set_new(log_entry, "intake_error_code", 
                        intake_error_code ? json_string(intake_error_code) : json_null());
    json_object_set_new(log_entry, "conflict_priority_level", json_integer((int)conflict_type));
    
    if (ten[0] != '\0')
    {
        json_object_set_new(log_entry, "tenant_id", json_string(ten));
    }
    if (run[0] != '\0')
    {
        json_object_set_new(log_entry, "run_id", json_string(run));
    }
    if (tid[0] != '\0')
    {
        json_object_set_new(log_entry, "trace_id", json_string(tid));
    }
    if (rid[0] != '\0')
    {
        json_object_set_new(log_entry, "request_id", json_string(rid));
    }
    
    /* Build context object */
    json_t *context = json_object();
    if (context != NULL)
    {
        if (stage != NULL)
        {
            json_object_set_new(context, "stage", json_string(stage));
        }
        if (code != NULL)
        {
            json_object_set_new(context, "error_code", json_string(code));
        }
        if (rid[0] != '\0')
        {
            json_object_set_new(context, "request_id", json_string(rid));
        }
        
        /* Filter PII from context before adding to log entry */
        json_t *filtered_context = filter_pii_json(context);
        json_object_set_new(log_entry, "context", filtered_context);
        json_decref(context); /* Free original context */
    }
    
    /* Output JSON log */
    char *json_str = json_dumps(log_entry, JSON_COMPACT);
    if (json_str != NULL)
    {
        fprintf(stderr, "%s\n", json_str);
        free(json_str);
    }
    
    json_decref(log_entry);
}

static unsigned long metric_requests_total        = 0UL;
static unsigned long metric_requests_errors_total = 0UL;
static unsigned long metric_requests_errors_4xx   = 0UL;
static unsigned long metric_requests_errors_5xx   = 0UL;

static unsigned long metric_requests_routes_decide_post = 0UL;
static unsigned long metric_requests_routes_decide_get  = 0UL;

static void send_rate_limit_error(int client_fd,
                                  rl_endpoint_id_t endpoint,
                                  const request_context_t *ctx)
{
    char headers[512];
    char body[MAX_RESPONSE_SIZE];
    
    /* Get current rate limit info */
    unsigned int remaining = 0;
    int limit = get_endpoint_limit(endpoint);
    time_t reset_at = time(NULL) + get_ttl_seconds();
    int retry_after = (int)(reset_at - time(NULL));
    if (retry_after < 0) retry_after = 0;
    
    /* Conflict Contract: Priority 1 - Rate Limiting (RL) */
    /* error_type: rate_limit, conflict_priority_level: 1, intake_error_code: null */
    
    /* Add rate limit headers */
    add_rate_limit_headers(headers, sizeof(headers), limit, remaining);
    
    /* Get endpoint name for error details */
    const char *endpoint_name = "unknown";
    switch (endpoint) {
        case RL_ENDPOINT_ROUTES_DECIDE:
            endpoint_name = "/api/v1/routes/decide";
            break;
        case RL_ENDPOINT_MESSAGES:
            endpoint_name = "/api/v1/messages";
            break;
        case RL_ENDPOINT_REGISTRY_BLOCKS:
            endpoint_name = "/api/v1/registry/blocks";
            break;
        default:
            break;
    }
    
    /* Enhanced JSON error response */
    const char *rid = (ctx != NULL && ctx->request_id[0] != '\0') ? ctx->request_id : "";
    const char *tid = (ctx != NULL && ctx->trace_id[0] != '\0') ? ctx->trace_id : "";
    const char *tenant_id = (ctx != NULL && ctx->tenant_id[0] != '\0') ? ctx->tenant_id : "";
    
    /* Conflict Contract: Priority 1 - Rate Limiting (RL) */
    /* intake_error_code must be present (null for Gateway errors) */
    int len = snprintf(body, sizeof(body),
                       "{\"ok\":false,"
                       "\"error\":{\"code\":\"rate_limit_exceeded\","
                       "\"message\":\"Rate limit exceeded for endpoint %s\","
                       "\"intake_error_code\":null,"
                       "\"details\":{\"endpoint\":\"%s\","
                       "\"limit\":%d,"
                       "\"retry_after_seconds\":%d}},"
                       "\"context\":{\"request_id\":\"%s\","
                       "\"trace_id\":\"%s\","
                       "\"tenant_id\":\"%s\"}}",
                       endpoint_name, endpoint_name, limit, retry_after,
                       rid, tid, tenant_id);
    
    if (len < 0 || (size_t)len >= sizeof(body)) {
        /* Fallback: use conflict-aware error response */
        send_error_response_with_conflict(client_fd, 
                                         "HTTP/1.1 429 Too Many Requests", 
                                         "rate_limit_exceeded", 
                                         "Rate limit exceeded", 
                                         ctx,
                                         CONFLICT_TYPE_RATE_LIMIT,
                                         NULL);
        return;
    }
    
    /* Log with conflict contract fields */
    log_error_with_conflict_info("rate_limiter", ctx, "rate_limit_exceeded",
                                  "Rate limit exceeded for endpoint", 
                                  CONFLICT_TYPE_RATE_LIMIT, NULL, 429);
    
    /* Send response with rate limit headers */
    char full_response[MAX_RESPONSE_SIZE + 512];
    len = snprintf(full_response, sizeof(full_response),
                   "HTTP/1.1 429 Too Many Requests\r\n"
                   "Content-Type: application/json\r\n"
                   "Content-Length: %zu\r\n"
                   "%s"  /* Rate limit headers */
                   "\r\n"
                   "%s",
                   strlen(body), headers, body);
    
    if (len < 0 || (size_t)len >= sizeof(full_response)) {
        /* Fallback if full response formatting fails */
        send_response(client_fd, "HTTP/1.1 429 Too Many Requests", "application/json", body);
    } else {
        (void)write(client_fd, full_response, strlen(full_response));
    }
    
    /* Increment error metric */
    metric_requests_errors_total++;
    
    /* Record rate limit hit metric */
    metrics_record_rate_limit_hit(ctx != NULL ? ctx->tenant_id : NULL);
}

/* Simple latency ring buffer to estimate percentiles */
#define LAT_BUF_SIZE 256
static int latency_buf[LAT_BUF_SIZE];
static int latency_count = 0;
static int latency_index = 0;

static void record_latency_ms(int ms)
{
    if (ms < 0) ms = 0;
    latency_buf[latency_index] = ms;
    latency_index = (latency_index + 1) % LAT_BUF_SIZE;
    if (latency_count < LAT_BUF_SIZE) latency_count++;
}

static int cmp_int(const void *a, const void *b)
{
    int ia = *(const int *)a;
    int ib = *(const int *)b;
    return (ia > ib) - (ia < ib);
}

static int percentile_ms(int p)
{
    if (latency_count == 0) return -1;
    int n = latency_count;
    int tmp[LAT_BUF_SIZE];
    for (int i = 0; i < n; i++) tmp[i] = latency_buf[i];
    qsort(tmp, (size_t)n, sizeof(int), cmp_int);
    int idx = (p * (n - 1)) / 100; /* nearest-rank */
    if (idx < 0) idx = 0; if (idx >= n) idx = n - 1;
    return tmp[idx];
}

/* crude RPS since start */
static time_t start_time_sec = 0;

static void log_info(const char *stage,
                     const request_context_t *ctx,
                     const char *method,
                     const char *path,
                     int status_code,
                     int latency_ms)
{
    char timestamp[32];
    get_iso8601_timestamp(timestamp, sizeof(timestamp));
    
    const char *rid = (ctx != NULL && ctx->request_id[0] != '\0')
                          ? ctx->request_id
                          : "";
    const char *tid = (ctx != NULL && ctx->trace_id[0] != '\0')
                          ? ctx->trace_id
                          : "";
    const char *ten = (ctx != NULL && ctx->tenant_id[0] != '\0')
                          ? ctx->tenant_id
                          : "";
    const char *run = (ctx != NULL && ctx->run_id[0] != '\0')
                          ? ctx->run_id
                          : "";
    
    /* Build message based on stage */
    char message[256];
    if (stage != NULL && strcmp(stage, "http_request") == 0)
    {
        snprintf(message, sizeof(message), "Request processed successfully");
    }
    else
    {
        snprintf(message, sizeof(message), "%s completed", stage != NULL ? stage : "operation");
    }
    
    /* Filter message for PII */
    char filtered_message[512];
    filter_pii(message, filtered_message, sizeof(filtered_message));

    /* Build JSON log entry using jansson */
    json_t *log_entry = json_object();
    if (log_entry == NULL) return;
    
    json_object_set_new(log_entry, "timestamp", json_string(timestamp));
    json_object_set_new(log_entry, "level", json_string("INFO"));
    json_object_set_new(log_entry, "component", json_string("gateway"));
    json_object_set_new(log_entry, "message", json_string(filtered_message));
    
    if (ten[0] != '\0')
    {
        json_object_set_new(log_entry, "tenant_id", json_string(ten));
    }
    if (run[0] != '\0')
    {
        json_object_set_new(log_entry, "run_id", json_string(run));
    }
    if (tid[0] != '\0')
    {
        json_object_set_new(log_entry, "trace_id", json_string(tid));
    }
    
    json_object_set_new(log_entry, "latency_ms", json_integer(latency_ms));
    
    /* Build context object */
    json_t *context = json_object();
    if (context != NULL)
    {
        if (stage != NULL)
        {
            json_object_set_new(context, "stage", json_string(stage));
        }
        if (method != NULL)
        {
            json_object_set_new(context, "method", json_string(method));
        }
        if (path != NULL)
        {
            json_object_set_new(context, "path", json_string(path));
        }
        json_object_set_new(context, "status_code", json_integer(status_code));
        if (rid[0] != '\0')
        {
            json_object_set_new(context, "request_id", json_string(rid));
        }
        
        /* Filter PII from context before adding to log entry */
        json_t *filtered_context = filter_pii_json(context);
        json_object_set_new(log_entry, "context", filtered_context);
        json_decref(context); /* Free original context */
    }
    
    /* Output JSON log */
    char *json_str = json_dumps(log_entry, JSON_COMPACT);
    if (json_str != NULL)
    {
        fprintf(stderr, "%s\n", json_str);
        free(json_str);
    }
    
    json_decref(log_entry);
}

static void __attribute__((unused)) log_warn(const char *stage,
                     const request_context_t *ctx,
                     const char *message)
{
    char timestamp[32];
    get_iso8601_timestamp(timestamp, sizeof(timestamp));
    
    const char *rid = (ctx != NULL && ctx->request_id[0] != '\0')
                          ? ctx->request_id
                          : "";
    const char *tid = (ctx != NULL && ctx->trace_id[0] != '\0')
                          ? ctx->trace_id
                          : "";
    const char *ten = (ctx != NULL && ctx->tenant_id[0] != '\0')
                          ? ctx->tenant_id
                          : "";
    const char *run = (ctx != NULL && ctx->run_id[0] != '\0')
                          ? ctx->run_id
                          : "";
    
    char filtered_message[512];
    filter_pii(message, filtered_message, sizeof(filtered_message));

    /* Build JSON log entry using jansson */
    json_t *log_entry = json_object();
    if (log_entry == NULL) return;
    
    json_object_set_new(log_entry, "timestamp", json_string(timestamp));
    json_object_set_new(log_entry, "level", json_string("WARN"));
    json_object_set_new(log_entry, "component", json_string("gateway"));
    json_object_set_new(log_entry, "message", json_string(filtered_message));
    
    if (ten[0] != '\0')
    {
        json_object_set_new(log_entry, "tenant_id", json_string(ten));
    }
    if (run[0] != '\0')
    {
        json_object_set_new(log_entry, "run_id", json_string(run));
    }
    if (tid[0] != '\0')
    {
        json_object_set_new(log_entry, "trace_id", json_string(tid));
    }
    
    /* Build context object */
    json_t *context = json_object();
    if (context != NULL)
    {
        if (stage != NULL)
        {
            json_object_set_new(context, "stage", json_string(stage));
        }
        if (rid[0] != '\0')
        {
            json_object_set_new(context, "request_id", json_string(rid));
        }
        
        /* Filter PII from context before adding to log entry */
        json_t *filtered_context = filter_pii_json(context);
        json_object_set_new(log_entry, "context", filtered_context);
        json_decref(context); /* Free original context */
    }
    
    /* Output JSON log */
    char *json_str = json_dumps(log_entry, JSON_COMPACT);
    if (json_str != NULL)
    {
        fprintf(stderr, "%s\n", json_str);
        free(json_str);
    }
    
    json_decref(log_entry);
}

static void __attribute__((unused)) log_debug(const char *stage,
                      const request_context_t *ctx,
                      const char *message)
{
    char timestamp[32];
    get_iso8601_timestamp(timestamp, sizeof(timestamp));
    
    const char *rid = (ctx != NULL && ctx->request_id[0] != '\0')
                          ? ctx->request_id
                          : "";
    const char *tid = (ctx != NULL && ctx->trace_id[0] != '\0')
                          ? ctx->trace_id
                          : "";
    const char *ten = (ctx != NULL && ctx->tenant_id[0] != '\0')
                          ? ctx->tenant_id
                          : "";
    const char *run = (ctx != NULL && ctx->run_id[0] != '\0')
                          ? ctx->run_id
                          : "";
    
    char filtered_message[512];
    filter_pii(message, filtered_message, sizeof(filtered_message));

    /* Build JSON log entry using jansson */
    json_t *log_entry = json_object();
    if (log_entry == NULL) return;
    
    json_object_set_new(log_entry, "timestamp", json_string(timestamp));
    json_object_set_new(log_entry, "level", json_string("DEBUG"));
    json_object_set_new(log_entry, "component", json_string("gateway"));
    json_object_set_new(log_entry, "message", json_string(filtered_message));
    
    if (ten[0] != '\0')
    {
        json_object_set_new(log_entry, "tenant_id", json_string(ten));
    }
    if (run[0] != '\0')
    {
        json_object_set_new(log_entry, "run_id", json_string(run));
    }
    if (tid[0] != '\0')
    {
        json_object_set_new(log_entry, "trace_id", json_string(tid));
    }
    
    /* Build context object */
    json_t *context = json_object();
    if (context != NULL)
    {
        if (stage != NULL)
        {
            json_object_set_new(context, "stage", json_string(stage));
        }
        if (rid[0] != '\0')
        {
            json_object_set_new(context, "request_id", json_string(rid));
        }
        
        /* Filter PII from context before adding to log entry */
        json_t *filtered_context = filter_pii_json(context);
        json_object_set_new(log_entry, "context", filtered_context);
        json_decref(context); /* Free original context */
    }
    
    /* Output JSON log */
    char *json_str = json_dumps(log_entry, JSON_COMPACT);
    if (json_str != NULL)
    {
        fprintf(stderr, "%s\n", json_str);
        free(json_str);
    }
    
    json_decref(log_entry);
}

static void send_response(int client_fd, const char *status_line,
                          const char *content_type, const char *body) {
    char header[512];
    size_t body_len = body ? strlen(body) : 0U;

    int header_len = snprintf(header, sizeof(header),
                              "%s\r\n"
                              "Content-Type: %s\r\n"
                              "Content-Length: %zu\r\n"
                              "Connection: close\r\n"
                              "\r\n",
                              status_line,
                              content_type,
                              body_len);

    if (header_len > 0) {
        (void)write(client_fd, header, (size_t)header_len);
    }
    if (body_len > 0) {
        (void)write(client_fd, body, body_len);
    }
}

static void send_response_with_headers(int client_fd, const char *status_line,
                                        const char *extra_headers, const char *body) {
    char header[1024];
    size_t body_len = body ? strlen(body) : 0U;

    int header_len = snprintf(header, sizeof(header),
                              "%s\r\n"
                              "%s"
                              "Content-Length: %zu\r\n"
                              "Connection: close\r\n"
                              "\r\n",
                              status_line,
                              extra_headers ? extra_headers : "",
                              body_len);

    if (header_len > 0) {
        (void)write(client_fd, header, (size_t)header_len);
    }
    if (body_len > 0) {
        (void)write(client_fd, body, body_len);
    }
}

static void send_error_response_with_retry_after(int client_fd,
                                                  const char *status_line,
                                                  const char *error_code,
                                                  const char *message,
                                                  int retry_after_seconds,
                                                  const request_context_t *ctx)
{
    char body[MAX_RESPONSE_SIZE];

    const char *rid = (ctx != NULL && ctx->request_id[0] != '\0')
                          ? ctx->request_id
                          : "";
    const char *tid = (ctx != NULL && ctx->trace_id[0] != '\0')
                          ? ctx->trace_id
                          : "";
    const char *ten = (ctx != NULL && ctx->tenant_id[0] != '\0')
                          ? ctx->tenant_id
                          : "";

    /* Extract HTTP status for conflict contract */
    int http_status = 503;
    if (strncmp(status_line, "HTTP/1.1 503", 12) == 0) http_status = 503;
    else if (strncmp(status_line, "HTTP/1.1 429", 12) == 0) http_status = 429;
    
    /* Determine conflict type from error code */
    conflict_type_t conflict_type = CONFLICT_TYPE_ROUTER_RUNTIME;
    if (error_code != NULL) {
        if (strcmp(error_code, "service_overloaded") == 0) {
            conflict_type = CONFLICT_TYPE_ROUTER_RUNTIME; /* Backpressure = Router runtime */
        } else if (strcmp(error_code, "rate_limit_exceeded") == 0) {
            conflict_type = CONFLICT_TYPE_RATE_LIMIT;
        }
    }

    int len = snprintf(body, sizeof(body),
                       "{\"ok\":false,"
                       "\"error\":{\"code\":\"%s\",\"message\":\"%s\",\"intake_error_code\":null,\"details\":{}},"
                       "\"context\":{\"request_id\":\"%s\",\"trace_id\":\"%s\",\"tenant_id\":\"%s\"}}",
                       error_code != NULL ? error_code : "internal",
                       message != NULL ? message : "",
                       rid, tid, ten);

    if (len < 0 || (size_t)len >= sizeof(body))
    {
        /* Fallback to minimal error JSON if formatting fails */
        const char *fallback = "{\"ok\":false,\"error\":{\"code\":\"internal\",\"message\":\"internal error\",\"intake_error_code\":null,\"details\":{}},\"context\":{\"request_id\":\"\",\"trace_id\":\"\",\"tenant_id\":\"\"}}";
        char headers[256];
        snprintf(headers, sizeof(headers),
                 "Content-Type: application/json\r\n"
                 "Content-Length: %zu\r\n"
                 "Retry-After: %d\r\n",
                 strlen(fallback), retry_after_seconds);
        send_response_with_headers(client_fd, status_line, headers, fallback);
        metric_requests_errors_total++;
        if (strncmp(status_line, "HTTP/1.1 4", 10) == 0) {
            metric_requests_errors_4xx++;
        } else if (strncmp(status_line, "HTTP/1.1 5", 10) == 0) {
            metric_requests_errors_5xx++;
        }
        log_error_with_conflict_info("backpressure", ctx, error_code, message,
                                      conflict_type, NULL, http_status);
        return;
    }

    char headers[256];
    snprintf(headers, sizeof(headers),
             "Content-Type: application/json\r\n"
             "Content-Length: %zu\r\n"
             "Retry-After: %d\r\n",
             strlen(body), retry_after_seconds);
    
    send_response_with_headers(client_fd, status_line, headers, body);
    metric_requests_errors_total++;
    if (strncmp(status_line, "HTTP/1.1 4", 10) == 0) {
        metric_requests_errors_4xx++;
    } else if (strncmp(status_line, "HTTP/1.1 5", 10) == 0) {
        metric_requests_errors_5xx++;
    }
    
    log_error_with_conflict_info("backpressure", ctx, error_code, message,
                                  conflict_type, NULL, http_status);
}

/* Send error response with conflict contract compliance */
static void send_error_response_with_conflict(int client_fd,
                                               const char *status_line,
                                               const char *error_code,
                                               const char *message,
                                               const request_context_t *ctx,
                                               conflict_type_t conflict_type,
                                               const char *intake_error_code)
{
    char body[MAX_RESPONSE_SIZE];

    const char *rid = (ctx != NULL && ctx->request_id[0] != '\0')
                          ? ctx->request_id
                          : "";
    const char *tid = (ctx != NULL && ctx->trace_id[0] != '\0')
                          ? ctx->trace_id
                          : "";
    const char *ten = (ctx != NULL && ctx->tenant_id[0] != '\0')
                          ? ctx->tenant_id
                          : "";

    /* Extract HTTP status code from status_line */
    int http_status = 500;
    if (strncmp(status_line, "HTTP/1.1 429", 12) == 0) http_status = 429;
    else if (strncmp(status_line, "HTTP/1.1 400", 12) == 0) http_status = 400;
    else if (strncmp(status_line, "HTTP/1.1 401", 12) == 0) http_status = 401;
    else if (strncmp(status_line, "HTTP/1.1 403", 12) == 0) http_status = 403;
    else if (strncmp(status_line, "HTTP/1.1 500", 12) == 0) http_status = 500;
    else if (strncmp(status_line, "HTTP/1.1 503", 12) == 0) http_status = 503;

    /* Build error response with intake_error_code (always present, may be null) */
    int len;
    if (intake_error_code != NULL && intake_error_code[0] != '\0') {
        len = snprintf(body, sizeof(body),
                       "{\"ok\":false,"
                       "\"error\":{\"code\":\"%s\",\"message\":\"%s\",\"intake_error_code\":\"%s\",\"details\":{}},"
                       "\"context\":{\"request_id\":\"%s\",\"trace_id\":\"%s\",\"tenant_id\":\"%s\"}}",
                       error_code != NULL ? error_code : "internal",
                       message != NULL ? message : "",
                       intake_error_code,
                       rid, tid, ten);
    } else {
        len = snprintf(body, sizeof(body),
                       "{\"ok\":false,"
                       "\"error\":{\"code\":\"%s\",\"message\":\"%s\",\"intake_error_code\":null,\"details\":{}},"
                       "\"context\":{\"request_id\":\"%s\",\"trace_id\":\"%s\",\"tenant_id\":\"%s\"}}",
                       error_code != NULL ? error_code : "internal",
                       message != NULL ? message : "",
                       rid, tid, ten);
    }

    if (len < 0 || (size_t)len >= sizeof(body))
    {
        /* Fallback to minimal error JSON if formatting fails */
        const char *fallback = "{\"ok\":false,\"error\":{\"code\":\"internal\",\"message\":\"internal error\",\"intake_error_code\":null,\"details\":{}},\"context\":{\"request_id\":\"\",\"trace_id\":\"\",\"tenant_id\":\"\"}}";
        send_response(client_fd, status_line, "application/json", fallback);
        metric_requests_errors_total++;
        if (strncmp(status_line, "HTTP/1.1 4", 10) == 0) {
            metric_requests_errors_4xx++;
        } else if (strncmp(status_line, "HTTP/1.1 5", 10) == 0) {
            metric_requests_errors_5xx++;
        }
        log_error_with_conflict_info("http_response", ctx, error_code, message,
                                     conflict_type, intake_error_code, http_status);
        return;
    }

    /* Log with conflict contract fields */
    log_error_with_conflict_info("http_response", ctx, error_code, message,
                                  conflict_type, intake_error_code, http_status);
    
    metric_requests_errors_total++;
    if (strncmp(status_line, "HTTP/1.1 4", 10) == 0) {
        metric_requests_errors_4xx++;
    } else if (strncmp(status_line, "HTTP/1.1 5", 10) == 0) {
        metric_requests_errors_5xx++;
    }

    send_response(client_fd, status_line, "application/json", body);
}

/* Legacy send_error_response - wraps new function with default conflict type */
static void send_error_response(int client_fd,
                                const char *status_line,
                                const char *error_code,
                                const char *message,
                                const request_context_t *ctx)
{
    /* Determine conflict type from error code */
    conflict_type_t conflict_type = CONFLICT_TYPE_INTERNAL_GATEWAY;
    if (error_code != NULL) {
        if (strcmp(error_code, "rate_limit_exceeded") == 0) {
            conflict_type = CONFLICT_TYPE_RATE_LIMIT;
        } else if (strcmp(error_code, "unauthorized") == 0) {
            conflict_type = CONFLICT_TYPE_AUTH_GATEWAY;
        } else if (strcmp(error_code, "invalid_request") == 0) {
            conflict_type = CONFLICT_TYPE_REQUEST_GATEWAY;
        }
    }
    
    send_error_response_with_conflict(client_fd, status_line, error_code, message, ctx,
                                      conflict_type, NULL);
}

static int validate_decide_request(const char *request_body,
                                   request_context_t *ctx)
{
    json_error_t error;
    json_t      *root = json_loads(request_body, 0, &error);

    if (root == NULL)
    {
        return -1;
    }

    if (!json_is_object(root))
    {
        json_decref(root);
        return -1;
    }

    json_t *version      = json_object_get(root, "version");
    json_t *tenant_id    = json_object_get(root, "tenant_id");
    json_t *request_id   = json_object_get(root, "request_id");
    json_t *run_id       = json_object_get(root, "run_id");
    json_t *task         = json_object_get(root, "task");
    json_t *task_type    = task != NULL ? json_object_get(task, "type") : NULL;
    json_t *task_payload = task != NULL ? json_object_get(task, "payload") : NULL;

    int ok = 1;

    if (!json_is_string(version) || strcmp(json_string_value(version), "1") != 0)
    {
        ok = 0;
    }

    if (!json_is_string(tenant_id) || !json_is_string(request_id))
    {
        ok = 0;
    }

    if (!json_is_object(task) || !json_is_string(task_type) || !json_is_object(task_payload))
    {
        ok = 0;
    }

    if (ok && ctx != NULL)
    {
        const char *rid_val = json_string_value(request_id);
        if (rid_val != NULL)
        {
            strncpy(ctx->request_id, rid_val, sizeof(ctx->request_id) - 1U);
            ctx->request_id[sizeof(ctx->request_id) - 1U] = '\0';
        }
        
        /* Extract run_id if present (optional field) */
        if (run_id != NULL && json_is_string(run_id))
        {
            const char *run_id_val = json_string_value(run_id);
            if (run_id_val != NULL)
            {
                strncpy(ctx->run_id, run_id_val, sizeof(ctx->run_id) - 1U);
                ctx->run_id[sizeof(ctx->run_id) - 1U] = '\0';
            }
        }
    }

    json_decref(root);

    return ok ? 0 : -1;
}

static int build_route_request_json(const char *http_body,
                                   request_context_t *ctx,
                                   char **out_json)
{
    if (out_json == NULL)
    {
        return -1;
    }

    *out_json = NULL;

    json_error_t error;
    json_t      *in_root = json_loads(http_body, 0, &error);
    if (in_root == NULL || !json_is_object(in_root))
    {
        if (in_root != NULL)
        {
            json_decref(in_root);
        }
        return -1;
    }

    /* Build dedicated RouteRequest object */
    json_t *route = json_object();
    if (route == NULL)
    {
        json_decref(in_root);
        return -1;
    }

    /* version: proxy as-is if present */
    json_t *version = json_object_get(in_root, "version");
    if (json_is_string(version))
    {
        json_object_set(route, "version", version);
    }

    /* tenant_id: prefer header/context, fallback to body */
    const char *tenant_src = (ctx != NULL && ctx->tenant_id[0] != '\0')
                                 ? ctx->tenant_id
                                 : NULL;
    if (tenant_src != NULL)
    {
        json_t *tenant = json_string(tenant_src);
        if (tenant != NULL)
        {
            json_object_set_new(route, "tenant_id", tenant);
        }
    }
    else
    {
        json_t *tenant_body = json_object_get(in_root, "tenant_id");
        if (json_is_string(tenant_body))
        {
            json_object_set(route, "tenant_id", tenant_body);
        }
    }

    /* request_id */
    json_t *request_id = json_object_get(in_root, "request_id");
    if (json_is_string(request_id))
    {
        json_object_set(route, "request_id", request_id);
    }

    /* trace_id: prefer context, fallback to body */
    const char *trace_src = (ctx != NULL && ctx->trace_id[0] != '\0')
                                ? ctx->trace_id
                                : NULL;
    if (trace_src != NULL)
    {
        json_t *trace = json_string(trace_src);
        if (trace != NULL)
        {
            json_object_set_new(route, "trace_id", trace);
        }
    }
    else
    {
        json_t *trace_body = json_object_get(in_root, "trace_id");
        if (json_is_string(trace_body))
        {
            json_object_set(route, "trace_id", trace_body);
        }
    }

    /* run_id: CP2+ optional field for multi-step workflows */
    json_t *run_id = json_object_get(in_root, "run_id");
    if (json_is_string(run_id))
    {
        json_object_set(route, "run_id", run_id);
        
        /* Save run_id to context if not already set */
        if (ctx != NULL && ctx->run_id[0] == '\0')
        {
            const char *run_id_val = json_string_value(run_id);
            if (run_id_val != NULL)
            {
                strncpy(ctx->run_id, run_id_val, sizeof(ctx->run_id) - 1U);
                ctx->run_id[sizeof(ctx->run_id) - 1U] = '\0';
            }
        }
    }

    /* message.* fields */
    json_t *message_obj = json_object();
    if (message_obj == NULL)
    {
        json_decref(in_root);
        json_decref(route);
        return -1;
    }

    json_t *msg_id    = json_object_get(in_root, "message_id");
    json_t *msg_type  = json_object_get(in_root, "message_type");
    json_t *payload   = json_object_get(in_root, "payload");
    json_t *metadata  = json_object_get(in_root, "metadata");

    if (json_is_string(msg_id))
    {
        json_object_set(message_obj, "message_id", msg_id);
    }
    if (json_is_string(msg_type))
    {
        json_object_set(message_obj, "message_type", msg_type);
    }
    if (payload != NULL && json_is_object(payload))
    {
        json_object_set(message_obj, "payload", payload);
    }
    if (metadata != NULL && json_is_object(metadata))
    {
        json_object_set(message_obj, "metadata", metadata);
    }

    json_object_set_new(route, "message", message_obj);

    /* policy_id */
    json_t *policy_id = json_object_get(in_root, "policy_id");
    if (json_is_string(policy_id))
    {
        json_object_set(route, "policy_id", policy_id);
    }

    /* context object (transparent passthrough) */
    json_t *context = json_object_get(in_root, "context");
    if (context != NULL && json_is_object(context))
    {
        json_object_set(route, "context", context);
    }

    char *dumped = json_dumps(route, JSON_COMPACT);
    json_decref(in_root);
    json_decref(route);

    if (dumped == NULL)
    {
        return -1;
    }

    *out_json = dumped;
    return 0;
}

/* Exported for testing - map_router_error_status is used by contract tests */
int map_router_error_status(const char *resp_json)
{
    if (resp_json == NULL)
    {
        return 0;
    }

    json_error_t error;
    json_t      *resp_root = json_loads(resp_json, 0, &error);
    if (resp_root == NULL || !json_is_object(resp_root))
    {
        if (resp_root != NULL)
        {
            json_decref(resp_root);
        }
        return 0;
    }

    int status_code = 0;

    json_t *ok_val = json_object_get(resp_root, "ok");
    if (json_is_boolean(ok_val) && !json_boolean_value(ok_val))
    {
        json_t *err_obj = json_object_get(resp_root, "error");
        json_t *code    = err_obj != NULL ? json_object_get(err_obj, "code") : NULL;
        const char *code_str = json_is_string(code) ? json_string_value(code) : NULL;

        if (code_str != NULL)
        {
            if (strcmp(code_str, "invalid_request") == 0)
            {
                status_code = 400;
            }
            else if (strcmp(code_str, "unauthorized") == 0)
            {
                status_code = 401;
            }
            else if (strcmp(code_str, "policy_not_found") == 0)
            {
                status_code = 404;
            }
            /* Extension error codes (CP2-LC) */
            else if (strcmp(code_str, "extension_not_found") == 0)
            {
                status_code = 404;
            }
            else if (strcmp(code_str, "extension_timeout") == 0)
            {
                status_code = 504;
            }
            else if (strcmp(code_str, "validator_blocked") == 0)
            {
                status_code = 403;
            }
            else if (strcmp(code_str, "post_processor_failed") == 0)
            {
                status_code = 500;
            }
            else if (strcmp(code_str, "extension_unavailable") == 0)
            {
                status_code = 503;
            }
            else if (strcmp(code_str, "extension_error") == 0)
            {
                status_code = 500;
            }
            else if (strcmp(code_str, "decision_failed") == 0 ||
                     strcmp(code_str, "internal") == 0)
            {
                status_code = 500;
            }
        }
    }

    json_decref(resp_root);
    return status_code;
}

static void handle_health(int client_fd) {
    char timestamp[32];
    get_iso8601_timestamp(timestamp, sizeof(timestamp));
    
    /* Build JSON response using jansson */
    json_t *health_response = json_object();
    if (health_response == NULL) {
        /* Fallback to simple response if JSON object creation fails */
        const char *body = "{\"status\":\"healthy\",\"timestamp\":\"error\"}";
        send_response(client_fd, "HTTP/1.1 200 OK", "application/json", body);
        return;
    }
    
    json_object_set_new(health_response, "status", json_string("healthy"));
    json_object_set_new(health_response, "timestamp", json_string(timestamp));
    
    char *body = json_dumps(health_response, JSON_COMPACT);
    if (body != NULL) {
        send_response(client_fd, "HTTP/1.1 200 OK", "application/json", body);
        free(body);
    } else {
        /* Fallback if json_dumps fails */
        const char *fallback = "{\"status\":\"healthy\",\"timestamp\":\"error\"}";
        send_response(client_fd, "HTTP/1.1 200 OK", "application/json", fallback);
    }
    
    json_decref(health_response);
}

/* Old handle_metrics function removed - now using handle_metrics_request from metrics_handler.c */

static void handle_metrics_json(int client_fd)
{
    char body[512];
    time_t now = time(NULL);
    if (start_time_sec == 0) start_time_sec = now;
    double uptime = difftime(now, start_time_sec);
    if (uptime < 1.0) uptime = 1.0;

    double rps = ((double)metric_requests_total) / uptime;
    int p50 = percentile_ms(50);
    int p95 = percentile_ms(95);
    double err_rate = 0.0;
    if (metric_requests_total > 0) {
        err_rate = ((double)metric_requests_errors_total) / ((double)metric_requests_total);
    }

    const char *nats = nats_get_status_string();
    if (nats == NULL) nats = "unknown";

    int len = snprintf(body, sizeof(body),
                       "{\"rps\":%.3f,\"latency\":{\"p50\":%d,\"p95\":%d},\"error_rate\":%.5f,"
                       "\"rate_limit\":{\"total_hits\":%lu,\"total_exceeded\":%lu,"
                       "\"exceeded_by_endpoint\":{\"routes_decide\":%lu,\"messages\":%lu,\"registry_blocks\":%lu}}},"
                       "\"nats\":\"%s\",\"ts\":%ld}",
                       rps, p50, p95, err_rate, 
                       rl_total_hits, rl_total_exceeded,
                       rl_exceeded_by_endpoint[RL_ENDPOINT_ROUTES_DECIDE],
                       rl_exceeded_by_endpoint[RL_ENDPOINT_MESSAGES],
                       rl_exceeded_by_endpoint[RL_ENDPOINT_REGISTRY_BLOCKS],
                       nats, (long)now);
    if (len < 0 || (size_t)len >= sizeof(body)) {
        const char *fallback = "{\"rps\":0,\"latency\":{\"p50\":-1,\"p95\":-1},\"error_rate\":0,"
                              "\"rate_limit\":{\"total_hits\":0,\"total_exceeded\":0,"
                              "\"exceeded_by_endpoint\":{\"routes_decide\":0,\"messages\":0,\"registry_blocks\":0}}},"
                              "\"nats\":\"unknown\"}";
        send_response(client_fd, "HTTP/1.1 200 OK", "application/json", fallback);
        return;
    }
    send_response(client_fd, "HTTP/1.1 200 OK", "application/json", body);
}

static void handle_get_decision(int client_fd,
                                const char *message_id,
                                request_context_t *ctx)
{
    if (message_id == NULL || message_id[0] == '\0') {
        send_error_response(client_fd,
                            "HTTP/1.1 400 Bad Request",
                            "invalid_request",
                            "empty message_id",
                            ctx);
        return;
    }

    if (ctx == NULL || ctx->tenant_id[0] == '\0') {
        send_error_response(client_fd,
                            "HTTP/1.1 400 Bad Request",
                            "invalid_request",
                            "missing tenant_id for decision lookup",
                            ctx);
        return;
    }

    char resp_buf[MAX_RESPONSE_SIZE];
    memset(resp_buf, 0, sizeof(resp_buf));

    int rc = nats_request_get_decision(ctx->tenant_id,
                                       message_id,
                                       resp_buf,
                                       sizeof(resp_buf));
    if (rc != 0) {
        send_error_response(client_fd,
                            "HTTP/1.1 503 Service Unavailable",
                            "internal",
                            "router or NATS unavailable",
                            ctx);
        return;
    }

    int status_code = map_router_error_status(resp_buf);
    const char *status_line = "HTTP/1.1 200 OK";

    switch (status_code)
    {
        case 400: status_line = "HTTP/1.1 400 Bad Request"; break;
        case 401: status_line = "HTTP/1.1 401 Unauthorized"; break;
        case 404: status_line = "HTTP/1.1 404 Not Found"; break;
        case 500: status_line = "HTTP/1.1 500 Internal Server Error"; break;
        default:  status_line = "HTTP/1.1 200 OK"; break;
    }

    send_response(client_fd, status_line, "application/json", resp_buf);
    if (strcmp(status_line, "HTTP/1.1 200 OK") == 0 && ctx && ctx->tenant_id[0] != '\0') {
        /* Broadcast creation event for realtime UI */
        sse_broadcast_json(ctx->tenant_id, "message_created", resp_buf);
    }
}

static void handle_extensions_health(int client_fd, const request_context_t *ctx)
{
    char resp_buf[MAX_RESPONSE_SIZE];
    memset(resp_buf, 0, sizeof(resp_buf));

    int rc = nats_request_get_extension_health(resp_buf, sizeof(resp_buf));
    if (rc != 0) {
        send_error_response(client_fd,
                            "HTTP/1.1 503 Service Unavailable",
                            "SERVICE_UNAVAILABLE",
                            "Router or NATS unavailable",
                            ctx);
        return;
    }

    send_response(client_fd, "HTTP/1.1 200 OK", "application/json", resp_buf);
}

static void handle_circuit_breakers(int client_fd, const request_context_t *ctx)
{
    char resp_buf[MAX_RESPONSE_SIZE];
    memset(resp_buf, 0, sizeof(resp_buf));

    int rc = nats_request_get_circuit_breaker_states(resp_buf, sizeof(resp_buf));
    if (rc != 0) {
        send_error_response(client_fd,
                            "HTTP/1.1 503 Service Unavailable",
                            "SERVICE_UNAVAILABLE",
                            "Router or NATS unavailable",
                            ctx);
        return;
    }

    send_response(client_fd, "HTTP/1.1 200 OK", "application/json", resp_buf);
}

static void handle_dry_run_pipeline(int client_fd,
                                    const char *request_body,
                                    const request_context_t *ctx)
{
    if (request_body == NULL || request_body[0] == '\0') {
        send_error_response(client_fd,
                            "HTTP/1.1 400 Bad Request",
                            "INVALID_REQUEST",
                            "empty request body",
                            ctx);
        return;
    }

    char resp_buf[MAX_RESPONSE_SIZE];
    memset(resp_buf, 0, sizeof(resp_buf));

    int rc = nats_request_dry_run_pipeline(request_body, resp_buf, sizeof(resp_buf));
    if (rc != 0) {
        send_error_response(client_fd,
                            "HTTP/1.1 503 Service Unavailable",
                            "SERVICE_UNAVAILABLE",
                            "Router or NATS unavailable",
                            ctx);
        return;
    }

    /* Check for error in response JSON */
    int status_code = map_router_error_status(resp_buf);
    const char *status_line = "HTTP/1.1 200 OK";

    switch (status_code)
    {
        case 400: status_line = "HTTP/1.1 400 Bad Request"; break;
        case 401: status_line = "HTTP/1.1 401 Unauthorized"; break;
        case 404: status_line = "HTTP/1.1 404 Not Found"; break;
        case 500: status_line = "HTTP/1.1 500 Internal Server Error"; break;
        default:  status_line = "HTTP/1.1 200 OK"; break;
    }

    send_response(client_fd, status_line, "application/json", resp_buf);
}

static void handle_pipeline_complexity(int client_fd,
                                      const char *tenant_id,
                                      const char *policy_id,
                                      const request_context_t *ctx)
{
    if (tenant_id == NULL || tenant_id[0] == '\0' ||
        policy_id == NULL || policy_id[0] == '\0') {
        send_error_response(client_fd,
                            "HTTP/1.1 400 Bad Request",
                            "INVALID_REQUEST",
                            "missing tenant_id or policy_id",
                            ctx);
        return;
    }

    char resp_buf[MAX_RESPONSE_SIZE];
    memset(resp_buf, 0, sizeof(resp_buf));

    int rc = nats_request_get_pipeline_complexity(tenant_id, policy_id, resp_buf, sizeof(resp_buf));
    if (rc != 0) {
        send_error_response(client_fd,
                            "HTTP/1.1 503 Service Unavailable",
                            "SERVICE_UNAVAILABLE",
                            "Router or NATS unavailable",
                            ctx);
        return;
    }

    /* Check for error in response JSON */
    int status_code = map_router_error_status(resp_buf);
    const char *status_line = "HTTP/1.1 200 OK";

    switch (status_code)
    {
        case 400: status_line = "HTTP/1.1 400 Bad Request"; break;
        case 401: status_line = "HTTP/1.1 401 Unauthorized"; break;
        case 404: status_line = "HTTP/1.1 404 Not Found"; break;
        case 500: status_line = "HTTP/1.1 500 Internal Server Error"; break;
        default:  status_line = "HTTP/1.1 200 OK"; break;
    }

    send_response(client_fd, status_line, "application/json", resp_buf);
}

static void handle_decide(int client_fd,
                          const char *request_body,
                          request_context_t *ctx,
                          otel_span_t *parent_span) {
    (void)parent_span; // Will be used for NATS span creation
    
    /* Extract client IP for abuse detection */
    char client_ip[64] = {0};
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    if (getpeername(client_fd, (struct sockaddr *)&client_addr, &addr_len) == 0) {
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
    } else {
        strcpy(client_ip, "unknown");
    }
    
    /* Extract API key from Authorization header (if present) */
    const char *api_key = NULL; /* TODO: Extract from Authorization header */
    
    /* Conflict Contract: Priority 3 - Request Gateway Validation (REQ_GW) */
    if (request_body == NULL || request_body[0] == '\0') {
        send_error_response_with_conflict(client_fd,
                            "HTTP/1.1 400 Bad Request",
                            "invalid_request",
                            "empty request body",
                            ctx,
                            CONFLICT_TYPE_REQUEST_GATEWAY,
                            NULL);
        return;
    }
    
    /* Check Router backpressure status before processing */
    backpressure_status_t backpressure_status = backpressure_client_check_router_status();
    switch (backpressure_status) {
        case BACKPRESSURE_ACTIVE:
            /* Router is overloaded - return 503 Service Unavailable */
            send_error_response_with_retry_after(client_fd,
                                                "HTTP/1.1 503 Service Unavailable",
                                                "service_overloaded",
                                                "Router is overloaded, please retry later",
                                                30, /* Retry-After: 30 seconds */
                                                ctx);
            return;
        case BACKPRESSURE_WARNING:
            /* Router is under stress - apply stricter rate limiting */
            /* Continue processing but with reduced rate limits */
            /* Rate limiter will handle this automatically */
            break;
        case BACKPRESSURE_INACTIVE:
        default:
            /* No backpressure - process normally */
            break;
    }
    
    /* Check Redis rate limiter (PoC) - after backpressure, before abuse detection */
    redis_rl_request_ctx_t redis_rl_ctx = {
        .client_ip = client_ip,
        .method = "POST",
        .path = "/api/v1/routes/decide",
        .tenant_id = (ctx && ctx->tenant_id[0] != '\0') ? ctx->tenant_id : NULL
    };
    redis_rl_result_t redis_rl_result;
    if (redis_rate_limiter_check(&redis_rl_ctx, &redis_rl_result) == 0) {
        if (redis_rl_result.decision == REDIS_RL_DENY) {
            /* Rate limit exceeded - return 429 */
            char headers[1024] = {0};
            char body[512] = {0};
            
            /* Build headers */
            snprintf(headers, sizeof(headers),
                "HTTP/1.1 429 Too Many Requests\r\n"
                "Content-Type: application/json\r\n"
                "Retry-After: %u\r\n"
                "X-RateLimit-Limit: %u\r\n"
                "X-RateLimit-Remaining: %u\r\n"
                "X-RateLimit-Reset: %lu\r\n"
                "\r\n",
                redis_rl_result.retry_after_sec,
                redis_rl_result.limit,
                redis_rl_result.remaining,
                redis_rl_result.reset_at);
            
            /* Build body with conflict contract compliance */
            const char *rid = (ctx && ctx->request_id[0] != '\0') ? ctx->request_id : "";
            const char *tid = (ctx && ctx->trace_id[0] != '\0') ? ctx->trace_id : "";
            const char *ten = (ctx && ctx->tenant_id[0] != '\0') ? ctx->tenant_id : "";
            
            snprintf(body, sizeof(body),
                "{\"ok\":false,\"error\":{\"code\":\"rate_limit_exceeded\","
                "\"message\":\"Too many requests\","
                "\"intake_error_code\":null,"
                "\"details\":{\"endpoint\":\"/api/v1/routes/decide\","
                "\"retry_after_seconds\":%u}},"
                "\"context\":{\"request_id\":\"%s\",\"trace_id\":\"%s\",\"tenant_id\":\"%s\"}}",
                redis_rl_result.retry_after_sec, rid, tid, ten);
            
            send(client_fd, headers, strlen(headers), 0);
            send(client_fd, body, strlen(body), 0);
            
            /* Log with conflict contract fields */
            log_error_with_conflict_info("redis_rate_limiter", ctx, "rate_limit_exceeded",
                                          "Too many requests", 
                                          CONFLICT_TYPE_RATE_LIMIT, NULL, 429);
            
            if (ctx && ctx->otel_span) {
                otel_span_set_attribute_int(ctx->otel_span, "http.status_code", 429);
                otel_span_set_status(ctx->otel_span, SPAN_STATUS_ERROR);
                otel_span_end(ctx->otel_span);
            }
            return;
        }
        /* If degraded (Redis unavailable), log but continue */
        if (redis_rl_result.degraded) {
            /* Log degradation (would use proper logging in production) */
            fprintf(stderr, "Redis rate limiter degraded (circuit breaker open), allowing request\n");
        }
    }
    
    /* Calculate payload size for abuse detection */
    int payload_size = (int)strlen(request_body);
    
    /* Track request for abuse detection */
    const char *tenant_id = (ctx && ctx->tenant_id[0] != '\0') ? ctx->tenant_id : NULL;
    if (tenant_id) {
        abuse_detection_track_request(tenant_id, api_key, client_ip, payload_size, RL_ENDPOINT_ROUTES_DECIDE);
        
        /* Check if tenant is temporarily blocked */
        if (abuse_detection_is_tenant_blocked(tenant_id)) {
            send_error_response(client_fd,
                                "HTTP/1.1 429 Too Many Requests",
                                "rate_limit_exceeded",
                                "Tenant temporarily blocked due to abuse detection",
                                ctx);
            return;
        }
        
        /* Check for abuse patterns */
        abuse_event_type_t abuse_type = abuse_detection_check_patterns(tenant_id, api_key, client_ip, payload_size, RL_ENDPOINT_ROUTES_DECIDE);
        if (abuse_type >= 0 && abuse_type <= ABUSE_MULTI_TENANT_FLOOD) {
            /* Abuse pattern detected - log event */
            const char *request_id = (ctx && ctx->request_id[0] != '\0') ? ctx->request_id : NULL;
            const char *trace_id = (ctx && ctx->trace_id[0] != '\0') ? ctx->trace_id : NULL;
            abuse_detection_log_event(abuse_type, tenant_id, api_key, client_ip, request_id, trace_id, "/api/v1/routes/decide", NULL);
            /* Metrics are recorded in abuse_detection_log_event() */
            
            /* Apply response action */
            abuse_response_action_t response_action = abuse_detection_get_response_action(abuse_type);
            switch (response_action) {
                case ABUSE_RESPONSE_TEMPORARY_BLOCK:
                    /* Block tenant for 5 minutes */
                    abuse_detection_block_tenant(tenant_id, 300);
                    send_error_response(client_fd,
                                        "HTTP/1.1 429 Too Many Requests",
                                        "rate_limit_exceeded",
                                        "Tenant temporarily blocked due to abuse detection",
                                        ctx);
                    return;
                case ABUSE_RESPONSE_RATE_LIMIT:
                    /* Apply stricter rate limiting (already handled by rate limiter) */
                    /* Continue processing but with stricter limits */
                    break;
                case ABUSE_RESPONSE_LOG_ONLY:
                default:
                    /* Only log - continue processing */
                    break;
            }
        }
    }

    /* Conflict Contract: Priority 3 - Request Gateway Validation (REQ_GW) */
    if (validate_decide_request(request_body, ctx) != 0)
    {
        send_error_response_with_conflict(client_fd,
                            "HTTP/1.1 400 Bad Request",
                            "invalid_request",
                            "invalid decide request DTO",
                            ctx,
                            CONFLICT_TYPE_REQUEST_GATEWAY,
                            NULL);
        return;
    }

    char *route_req_json = NULL;
    if (build_route_request_json(request_body, ctx, &route_req_json) != 0)
    {
        send_error_response(client_fd,
                            "HTTP/1.1 400 Bad Request",
                            "invalid_request",
                            "failed to build RouteRequest",
                            ctx);
        return;
    }

    char resp_buf[MAX_RESPONSE_SIZE];
    memset(resp_buf, 0, sizeof(resp_buf));

    // Create NATS publish span (child of HTTP span)
    otel_span_t *nats_span = NULL;
    if (parent_span) {
        nats_span = otel_span_start("gateway.nats.publish", SPAN_KIND_CLIENT, parent_span);
        if (nats_span) {
            const char *subject = getenv("ROUTER_DECIDE_SUBJECT");
            if (!subject || subject[0] == '\0') {
                subject = "beamline.router.v1.decide";
            }
            otel_span_set_attribute(nats_span, "nats.subject", subject);
        }
    }

    int rc = nats_request_decide(route_req_json, resp_buf, sizeof(resp_buf));
    free(route_req_json);
    
    // End NATS span
    if (nats_span) {
        otel_span_set_status(nats_span, (rc == 0) ? SPAN_STATUS_OK : SPAN_STATUS_ERROR);
        otel_span_end(nats_span);
    }

    /* Conflict Contract: Priority 5 - Router Runtime Error (RUNTIME_ROUTER) */
    if (rc != 0) {
        send_error_response_with_conflict(client_fd,
                            "HTTP/1.1 503 Service Unavailable",
                            "unavailable",
                            "router or NATS unavailable",
                            ctx,
                            CONFLICT_TYPE_ROUTER_RUNTIME,
                            NULL);
        return;
    }

    /* Map Router ErrorResponse.error.code to HTTP status if ok == false */
    int status_code = map_router_error_status(resp_buf);
    const char *status_line = "HTTP/1.1 200 OK";
    
    /* Extract intake_error_code from Router response (if error) */
    const char *intake_error_code = NULL;
    conflict_type_t conflict_type = CONFLICT_TYPE_ROUTER_INTAKE;
    
    if (status_code >= 400) {
        /* Parse Router response to extract intake_error_code */
        json_error_t json_err;
        json_t *resp_root = json_loads(resp_buf, 0, &json_err);
        if (resp_root != NULL) {
            json_t *ok_val = json_object_get(resp_root, "ok");
            if (json_is_boolean(ok_val) && !json_boolean_value(ok_val)) {
                json_t *err_obj = json_object_get(resp_root, "error");
                if (err_obj != NULL) {
                    json_t *intake_code = json_object_get(err_obj, "intake_error_code");
                    if (json_is_string(intake_code)) {
                        intake_error_code = json_string_value(intake_code);
                    }
                    
                    /* Determine conflict type: intake vs runtime */
                    json_t *code_val = json_object_get(err_obj, "code");
                    if (json_is_string(code_val)) {
                        const char *gateway_code = json_string_value(code_val);
                        /* Runtime errors typically have codes like "internal", "unavailable" */
                        if (strcmp(gateway_code, "internal") == 0 || 
                            strcmp(gateway_code, "unavailable") == 0) {
                            conflict_type = CONFLICT_TYPE_ROUTER_RUNTIME;
                        } else {
                            conflict_type = CONFLICT_TYPE_ROUTER_INTAKE;
                        }
                    }
                }
            }
            json_decref(resp_root);
        }
        
        /* If Router returned error, log with conflict contract */
        if (status_code >= 400) {
            json_error_t json_err2;
            json_t *resp_root2 = json_loads(resp_buf, 0, &json_err2);
            if (resp_root2 != NULL) {
                json_t *err_obj = json_object_get(resp_root2, "error");
                if (err_obj != NULL) {
                    json_t *msg_val = json_object_get(err_obj, "message");
                    json_t *code_val = json_object_get(err_obj, "code");
                    const char *message = json_is_string(msg_val) ? json_string_value(msg_val) : "Router error";
                    const char *error_code = json_is_string(code_val) ? json_string_value(code_val) : "internal";
                    
                    log_error_with_conflict_info("router_response", ctx, error_code, message,
                                                  conflict_type, intake_error_code, status_code);
                }
                json_decref(resp_root2);
            }
        }
    }

    switch (status_code)
    {
        case 400: status_line = "HTTP/1.1 400 Bad Request"; break;
        case 401: status_line = "HTTP/1.1 401 Unauthorized"; break;
        case 404: status_line = "HTTP/1.1 404 Not Found"; break;
        case 500: status_line = "HTTP/1.1 500 Internal Server Error"; break;
        case 503: status_line = "HTTP/1.1 503 Service Unavailable"; break;
        default:  status_line = "HTTP/1.1 200 OK"; break;
    }

    /* For error responses, ensure intake_error_code is present in body */
    if (status_code >= 400) {
        json_error_t json_err3;
        json_t *resp_root3 = json_loads(resp_buf, 0, &json_err3);
        if (resp_root3 != NULL) {
            json_t *err_obj = json_object_get(resp_root3, "error");
            if (err_obj != NULL) {
                json_t *intake_code_check = json_object_get(err_obj, "intake_error_code");
                if (intake_code_check == NULL) {
                    /* Add intake_error_code field (null if not present) */
                    json_object_set_new(err_obj, "intake_error_code", 
                                       intake_error_code ? json_string(intake_error_code) : json_null());
                    char *updated_json = json_dumps(resp_root3, JSON_COMPACT);
                    if (updated_json != NULL) {
                        strncpy(resp_buf, updated_json, MAX_RESPONSE_SIZE - 1);
                        resp_buf[MAX_RESPONSE_SIZE - 1] = '\0';
                        free(updated_json);
                    }
                }
            }
            json_decref(resp_root3);
        }
    }

    send_response(client_fd, status_line, "application/json", resp_buf);
}

static void handle_client(int client_fd) {
    char buffer[MAX_REQUEST_SIZE];
    clock_t start_clock = clock();
    struct timeval start_time, end_time;
    gettimeofday(&start_time, NULL);
    
    ssize_t received = read(client_fd, buffer, sizeof(buffer) - 1U);
    if (received <= 0) {
        close(client_fd);
        return;
    }
    buffer[received] = '\0';

    metric_requests_total++;

    /* Split headers and body by first \r\n\r\n */
    char *headers_end = strstr(buffer, "\r\n\r\n");
    char *body = NULL;
    if (headers_end != NULL) {
        *headers_end = '\0';
        body = headers_end + 4; /* skip CRLF CRLF */
    } else {
        body = (char *)"";
    }

    char *method = NULL;
    char *path = NULL;
    char *saveptr = NULL;

    request_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    
    /* Extract client IP address */
    char client_ip[64] = {0};
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    if (getpeername(client_fd, (struct sockaddr *)&client_addr, &addr_len) == 0) {
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
    } else {
        strcpy(client_ip, "unknown");
    }

    endpoint_id_t endpoint = ENDPOINT_UNKNOWN;
    (void)endpoint; /* currently unused, but may be needed for future features */
    int keep_open = 0; /* for SSE stream */

    char *line = strtok_r(buffer, "\r\n", &saveptr);
    if (line != NULL) {
        method = strtok(line, " ");
        path = strtok(NULL, " ");
    }

    /* Conflict Contract: Priority 3 - Request Gateway Validation (REQ_GW) */
    if (method == NULL || path == NULL) {
        send_error_response_with_conflict(client_fd,
                            "HTTP/1.1 400 Bad Request",
                            "invalid_request",
                            "malformed request line",
                            &ctx,
                            CONFLICT_TYPE_REQUEST_GATEWAY,
                            NULL);
        gettimeofday(&end_time, NULL);
        uint64_t duration_us = ((uint64_t)(end_time.tv_sec - start_time.tv_sec)) * 1000000ULL + 
                              (uint64_t)(end_time.tv_usec - start_time.tv_usec);
        metrics_record_http_request("UNKNOWN", "UNKNOWN", 400, duration_us);
        // End span if created
        if (ctx.otel_span) {
            otel_span_set_attribute_int(ctx.otel_span, "http.status_code", 400);
            otel_span_set_status(ctx.otel_span, SPAN_STATUS_ERROR);
            otel_span_end(ctx.otel_span);
        }
        close(client_fd);
        return;
    }

    /* Minimal header validation: X-Tenant-ID is required for API calls.
     * Also extract X-Trace-ID into context if present, and optionally
     * detect presence of Authorization header for auth skeleton.
     */
    int has_tenant_header = 0;
    int has_auth_header   = 0;
    const char *tenant_header_name = "X-Tenant-ID:";
    const char *trace_header_name  = "X-Trace-ID:";
    const char *traceparent_header_name = "traceparent:";
    const char *auth_header_name   = "Authorization:";
    
    // Extract traceparent for OpenTelemetry tracing
    const char *traceparent_value = NULL;

    while ((line = strtok_r(NULL, "\r\n", &saveptr)) != NULL) {
        if (strncmp(line, tenant_header_name, strlen(tenant_header_name)) == 0) {
            has_tenant_header = 1;
            const char *value = line + strlen(tenant_header_name);
            while (*value == ' ' || *value == '\t') {
                value++;
            }
            strncpy(ctx.tenant_id, value, sizeof(ctx.tenant_id) - 1U);
            ctx.tenant_id[sizeof(ctx.tenant_id) - 1U] = '\0';
        } else if (strncmp(line, trace_header_name, strlen(trace_header_name)) == 0) {
            const char *value = line + strlen(trace_header_name);
            while (*value == ' ' || *value == '\t') {
                value++;
            }
            strncpy(ctx.trace_id, value, sizeof(ctx.trace_id) - 1U);
            ctx.trace_id[sizeof(ctx.trace_id) - 1U] = '\0';
        } else if (strncasecmp(line, traceparent_header_name, strlen(traceparent_header_name)) == 0) {
            traceparent_value = line + strlen(traceparent_header_name);
            while (*traceparent_value == ' ' || *traceparent_value == '\t') {
                traceparent_value++;
            }
        } else if (strncmp(line, auth_header_name, strlen(auth_header_name)) == 0) {
            has_auth_header = 1;
        }
    }

    // Extract trace context from traceparent header if present
    otel_span_t *parent_span = NULL;
    trace_id_t extracted_trace_id;
    span_id_t extracted_span_id;
    
    if (traceparent_value && otel_extract_trace_context(traceparent_value, &extracted_trace_id, &extracted_span_id) == 0) {
        // Create a temporary parent span from extracted context
        // This allows us to create a child span with the correct trace_id
        parent_span = calloc(1, sizeof(otel_span_t));
        if (parent_span) {
            memcpy(&parent_span->trace_id, &extracted_trace_id, sizeof(trace_id_t));
            memcpy(&parent_span->span_id, &extracted_span_id, sizeof(span_id_t));
        }
    }
    
    // Create HTTP request span (child of extracted parent if traceparent present)
    otel_span_t *http_span = otel_span_start("gateway.http.request", SPAN_KIND_SERVER, parent_span);
    if (http_span) {
        otel_span_set_attribute(http_span, "http.method", method);
        otel_span_set_attribute(http_span, "http.url", path);
        if (ctx.tenant_id[0] != '\0') {
            otel_span_set_attribute(http_span, "tenant_id", ctx.tenant_id);
        }
        if (ctx.request_id[0] != '\0') {
            otel_span_set_attribute(http_span, "request_id", ctx.request_id);
        }
        if (ctx.trace_id[0] != '\0') {
            otel_span_set_attribute(http_span, "trace_id", ctx.trace_id);
        }
    }
    
    // Free temporary parent span if created
    if (parent_span && parent_span != http_span) {
        free(parent_span);
    }
    
    // Store span in context for NATS propagation
    ctx.otel_span = http_span;
    
    int latency_ms = 0;
    int http_status_code = 200;
    if (strcmp(method, "GET") == 0 && (strcmp(path, "/health") == 0 || strcmp(path, "/_health") == 0)) {
        endpoint = ENDPOINT_HEALTH;
        handle_health(client_fd);
        latency_ms = (int)((clock() - start_clock) * 1000 / CLOCKS_PER_SEC);
        record_latency_ms(latency_ms);
        gettimeofday(&end_time, NULL);
        uint64_t duration_us = ((uint64_t)(end_time.tv_sec - start_time.tv_sec)) * 1000000ULL + 
                              (uint64_t)(end_time.tv_usec - start_time.tv_usec);
        metrics_record_http_request(method, path, 200, duration_us);
        http_status_code = 200;
        log_info("http_request", &ctx, method, path, 200, latency_ms);
    } else if (strcmp(method, "GET") == 0 && strcmp(path, "/_metrics") == 0) {
        endpoint = ENDPOINT_METRICS_JSON;
        handle_metrics_json(client_fd);
        latency_ms = (int)((clock() - start_clock) * 1000 / CLOCKS_PER_SEC);
        record_latency_ms(latency_ms);
        gettimeofday(&end_time, NULL);
        uint64_t duration_us = ((uint64_t)(end_time.tv_sec - start_time.tv_sec)) * 1000000ULL + 
                              (uint64_t)(end_time.tv_usec - start_time.tv_usec);
        metrics_record_http_request(method, path, 200, duration_us);
        http_status_code = 200;
        log_info("http_request", &ctx, method, path, 200, latency_ms);
    } else if (strcmp(method, "GET") == 0 && strcmp(path, "/metrics") == 0) {
        endpoint = ENDPOINT_METRICS;
        struct timeval metrics_start_time, metrics_end_time;
        gettimeofday(&metrics_start_time, NULL);
        handle_metrics_request(client_fd);
        gettimeofday(&metrics_end_time, NULL);
        uint64_t duration_us = ((uint64_t)(metrics_end_time.tv_sec - metrics_start_time.tv_sec)) * 1000000ULL + 
                              (uint64_t)(metrics_end_time.tv_usec - metrics_start_time.tv_usec);
        latency_ms = (int)((clock() - start_clock) * 1000 / CLOCKS_PER_SEC);
        record_latency_ms(latency_ms);
        metrics_record_http_request(method, path, 200, duration_us);
        http_status_code = 200;
        log_info("http_request", &ctx, method, path, 200, latency_ms);
    } else if ((strcmp(method, "POST") == 0 || strcmp(method, "PUT") == 0 || strcmp(method, "DELETE") == 0) &&
               strncmp(path, "/api/v1/registry/blocks/", strlen("/api/v1/registry/blocks/")) == 0) {
        /* Parse /api/v1/registry/blocks/:type/:version */
        const char *prefix = "/api/v1/registry/blocks/";
        const char *p = path + strlen(prefix);
        const char *slash = strchr(p, '/');
        if (!slash) {
            send_error_response(client_fd, "HTTP/1.1 400 Bad Request", "invalid_request", "missing version segment", &ctx);
            latency_ms = (int)((clock() - start_clock) * 1000 / CLOCKS_PER_SEC);
            record_latency_ms(latency_ms);
            close(client_fd);
            return;
        }
        char type[128]; memset(type, 0, sizeof(type));
        size_t tlen = (size_t)(slash - p);
        if (tlen >= sizeof(type)) tlen = sizeof(type) - 1;
        strncpy(type, p, tlen);
        const char *version = slash + 1;
        if (version == NULL || *version == '\0') {
            send_error_response(client_fd, "HTTP/1.1 400 Bad Request", "invalid_request", "empty version", &ctx);
            latency_ms = (int)((clock() - start_clock) * 1000 / CLOCKS_PER_SEC);
            record_latency_ms(latency_ms);
            if (ctx.otel_span) {
                otel_span_set_attribute_int(ctx.otel_span, "http.status_code", 400);
                otel_span_set_status(ctx.otel_span, SPAN_STATUS_ERROR);
                otel_span_end(ctx.otel_span);
            }
            close(client_fd);
            return;
        }
        
        /* Apply rate limiting to registry endpoints */
        unsigned int remaining = 0;
        if (rate_limit_check(RL_ENDPOINT_REGISTRY_BLOCKS, ctx.tenant_id, NULL, &remaining) != 0) {
            send_rate_limit_error(client_fd, RL_ENDPOINT_REGISTRY_BLOCKS, &ctx);
            if (ctx.otel_span) {
                otel_span_set_attribute_int(ctx.otel_span, "http.status_code", 429);
                otel_span_set_status(ctx.otel_span, SPAN_STATUS_ERROR);
                otel_span_end(ctx.otel_span);
            }
            close(client_fd);
            return;
        }
        
        if (strcmp(method, "DELETE") == 0) {
            endpoint = ENDPOINT_REGISTRY_DELETE;
            handle_registry_delete(client_fd, type, version);
        } else {
            endpoint = (strcmp(method, "POST") == 0) ? ENDPOINT_REGISTRY_POST : ENDPOINT_REGISTRY_PUT;
            handle_registry_write_common(client_fd, method, type, version, body);
        }
        latency_ms = (int)((clock() - start_clock) * 1000 / CLOCKS_PER_SEC);
        record_latency_ms(latency_ms);
        log_info("http_request", &ctx, method, path, 200, latency_ms);
    } else if (strcmp(method, "GET") == 0 &&
               strncmp(path, "/api/v1/routes/decide/", strlen("/api/v1/routes/decide/")) == 0) {
        /* Extract message_id part after the prefix. */
        const char *prefix = "/api/v1/routes/decide/";
        const char *message_id = path + strlen(prefix);

        if (message_id == NULL || message_id[0] == '\0') {
            send_error_response(client_fd,
                                "HTTP/1.1 400 Bad Request",
                                "invalid_request",
                                "missing message_id path parameter",
                                &ctx);
            if (ctx.otel_span) {
                otel_span_set_attribute_int(ctx.otel_span, "http.status_code", 400);
                otel_span_set_status(ctx.otel_span, SPAN_STATUS_ERROR);
                otel_span_end(ctx.otel_span);
            }
            close(client_fd);
            return;
        }

        /* Apply rate limiting to GET /api/v1/routes/decide/:messageId */
        unsigned int remaining = 0;
        if (rate_limit_check(RL_ENDPOINT_ROUTES_DECIDE, ctx.tenant_id, NULL, &remaining) != 0) {
            send_rate_limit_error(client_fd, RL_ENDPOINT_ROUTES_DECIDE, &ctx);
            if (ctx.otel_span) {
                otel_span_set_attribute_int(ctx.otel_span, "http.status_code", 429);
                otel_span_set_status(ctx.otel_span, SPAN_STATUS_ERROR);
                otel_span_end(ctx.otel_span);
            }
            close(client_fd);
            return;
        }

        endpoint = ENDPOINT_ROUTES_DECIDE_GET;
        metric_requests_routes_decide_get++;
        handle_get_decision(client_fd, message_id, &ctx);
        /* handle_get_decision already sends response and logs error if needed */
        latency_ms = (int)((clock() - start_clock) * 1000 / CLOCKS_PER_SEC);
        record_latency_ms(latency_ms);
    } else if ((strcmp(method, "PUT") == 0 || strcmp(method, "DELETE") == 0) &&
               strncmp(path, "/api/v1/messages/", strlen("/api/v1/messages/")) == 0) {
        /* Minimal CP1 handlers to emit SSE update/delete */
        const char *prefix = "/api/v1/messages/";
        const char *message_id = path + strlen(prefix);
        if (message_id == NULL || message_id[0] == '\0') {
            send_error_response(client_fd, "HTTP/1.1 400 Bad Request", "invalid_request", "missing message_id", &ctx);
            close(client_fd);
            return;
        }
        if (ctx.tenant_id[0] == '\0') {
            send_error_response(client_fd, "HTTP/1.1 400 Bad Request", "invalid_request", "missing X-Tenant-ID header", &ctx);
            close(client_fd);
            return;
        }
        
        /* Apply rate limiting to messages endpoints */
        unsigned int remaining = 0;
        if (rate_limit_check(RL_ENDPOINT_MESSAGES, ctx.tenant_id, NULL, &remaining) != 0) {
            send_rate_limit_error(client_fd, RL_ENDPOINT_MESSAGES, &ctx);
            close(client_fd);
            return;
        }
        
        if (strcmp(method, "PUT") == 0) {
            if (body == NULL || body[0] == '\0') {
                send_error_response(client_fd, "HTTP/1.1 400 Bad Request", "invalid_request", "empty body", &ctx);
                if (ctx.otel_span) {
                    otel_span_set_attribute_int(ctx.otel_span, "http.status_code", 400);
                    otel_span_set_status(ctx.otel_span, SPAN_STATUS_ERROR);
                    otel_span_end(ctx.otel_span);
                }
                close(client_fd);
                return;
            }
            /* validate JSON and broadcast */
            json_error_t jerr; json_t *root = json_loads(body, 0, &jerr);
            if (!root || !json_is_object(root)) {
                if (root) json_decref(root);
                send_error_response(client_fd, "HTTP/1.1 400 Bad Request", "invalid_request", "invalid JSON", &ctx);
                if (ctx.otel_span) {
                    otel_span_set_attribute_int(ctx.otel_span, "http.status_code", 400);
                    otel_span_set_status(ctx.otel_span, SPAN_STATUS_ERROR);
                    otel_span_end(ctx.otel_span);
                }
                close(client_fd);
                return;
            }
            /* enforce message_id match if present in body */
            json_t *mid = json_object_get(root, "message_id");
            if (mid && json_is_string(mid)) {
                const char *mid_s = json_string_value(mid);
                if (strcmp(mid_s, message_id) != 0) {
                    json_decref(root);
                    send_error_response(client_fd, "HTTP/1.1 409 Conflict", "conflict", "message_id mismatch with path", &ctx);
                    if (ctx.otel_span) {
                        otel_span_set_attribute_int(ctx.otel_span, "http.status_code", 409);
                        otel_span_set_status(ctx.otel_span, SPAN_STATUS_ERROR);
                        otel_span_end(ctx.otel_span);
                    }
                    close(client_fd);
                    return;
                }
            }
            send_response(client_fd, "HTTP/1.1 200 OK", "application/json", body);
            sse_broadcast_json(ctx.tenant_id, "message_updated", body);
            json_decref(root);
        } else { /* DELETE */
            char resp[256];
            int len = snprintf(resp, sizeof(resp), "{\"status\":\"deleted\",\"message_id\":\"%s\"}", message_id);
            if (len < 0 || (size_t)len >= sizeof(resp)) {
                send_error_response(client_fd, "HTTP/1.1 400 Bad Request", "invalid_request", "bad message_id", &ctx);
                close(client_fd);
                return;
            }
            send_response(client_fd, "HTTP/1.1 200 OK", "application/json", resp);
            char evt[128];
            int l = snprintf(evt, sizeof(evt), "{\"message_id\":\"%s\"}", message_id);
            if (l > 0 && (size_t)l < sizeof(evt)) {
                sse_broadcast_json(ctx.tenant_id, "message_deleted", evt);
            }
        }
        latency_ms = (int)((clock() - start_clock) * 1000 / CLOCKS_PER_SEC);
        record_latency_ms(latency_ms);
    } else if (strcmp(method, "GET") == 0 &&
               strncmp(path, "/api/v1/messages/stream", strlen("/api/v1/messages/stream")) == 0) {
        char tenant_q[64]; tenant_q[0] = '\0';
        if (!query_get_param(path, "tenant_id", tenant_q, sizeof(tenant_q)) || tenant_q[0] == '\0') {
            send_error_response(client_fd,
                                "HTTP/1.1 400 Bad Request",
                                "invalid_request",
                                "missing tenant_id",
                                &ctx);
            close(client_fd);
            return;
        }
        if (sse_register_client(client_fd, tenant_q) == 0) {
            keep_open = 1;
            endpoint = ENDPOINT_UNKNOWN;
        } else {
            /* registration failed, close */
        }
        latency_ms = (int)((clock() - start_clock) * 1000 / CLOCKS_PER_SEC);
        record_latency_ms(latency_ms);
    } else if (strcmp(method, "POST") == 0 &&
               (strcmp(path, "/api/v1/messages") == 0 ||
                strcmp(path, "/api/v1/routes/decide") == 0)) {
        /* Conflict Contract: Priority 2 - Authentication Gateway (AUTH_GW) */
        if (auth_required && !has_auth_header) {
            send_error_response_with_conflict(client_fd,
                                "HTTP/1.1 401 Unauthorized",
                                "unauthorized",
                                "missing Authorization header",
                                &ctx,
                                CONFLICT_TYPE_AUTH_GATEWAY,
                                NULL);
            close(client_fd);
            return;
        }

        /* Conflict Contract: Priority 3 - Request Gateway Validation (REQ_GW) */
        if (!has_tenant_header) {
            send_error_response_with_conflict(client_fd,
                                "HTTP/1.1 400 Bad Request",
                                "invalid_request",
                                "missing X-Tenant-ID header",
                                &ctx,
                                CONFLICT_TYPE_REQUEST_GATEWAY,
                                NULL);
            close(client_fd);
            return;
        }

        if (strcmp(path, "/api/v1/routes/decide") == 0) {
            if (rate_limit_check_routes_decide(client_fd, &ctx) != 0) {
                /* Enhanced 429 error response is already sent by send_rate_limit_error */
                if (ctx.otel_span) {
                    otel_span_set_attribute_int(ctx.otel_span, "http.status_code", 429);
                    otel_span_set_status(ctx.otel_span, SPAN_STATUS_ERROR);
                    otel_span_end(ctx.otel_span);
                }
                close(client_fd);
                return;
            }
            endpoint = ENDPOINT_ROUTES_DECIDE_POST;
            metric_requests_routes_decide_post++;
        } else if (strcmp(path, "/api/v1/messages") == 0) {
            /* Apply rate limiting to POST /api/v1/messages */
            unsigned int remaining = 0;
            if (rate_limit_check(RL_ENDPOINT_MESSAGES, ctx.tenant_id, NULL, &remaining) != 0) {
                send_rate_limit_error(client_fd, RL_ENDPOINT_MESSAGES, &ctx);
                if (ctx.otel_span) {
                    otel_span_set_attribute_int(ctx.otel_span, "http.status_code", 429);
                    otel_span_set_status(ctx.otel_span, SPAN_STATUS_ERROR);
                    otel_span_end(ctx.otel_span);
                }
                close(client_fd);
                return;
            }
            /* For now, just handle it as a simple message - no specific metric tracking */
        }

        handle_decide(client_fd, body, &ctx, http_span);
        /* Для успешного кейса логируем 200, коды ошибок уже покрыты log_error */
        latency_ms = (int)((clock() - start_clock) * 1000 / CLOCKS_PER_SEC);
        record_latency_ms(latency_ms);
        gettimeofday(&end_time, NULL);
        uint64_t duration_us = ((uint64_t)(end_time.tv_sec - start_time.tv_sec)) * 1000000ULL + 
                              (uint64_t)(end_time.tv_usec - start_time.tv_usec);
        metrics_record_http_request(method, path, 200, duration_us);
        http_status_code = 200;
        log_info("http_request", &ctx, method, path, 200, latency_ms);
    } else if (strcmp(method, "GET") == 0 && strcmp(path, "/api/v1/extensions/health") == 0) {
        handle_extensions_health(client_fd, &ctx);
        latency_ms = (int)((clock() - start_clock) * 1000 / CLOCKS_PER_SEC);
        record_latency_ms(latency_ms);
        gettimeofday(&end_time, NULL);
        uint64_t duration_us = ((uint64_t)(end_time.tv_sec - start_time.tv_sec)) * 1000000ULL + 
                              (uint64_t)(end_time.tv_usec - start_time.tv_usec);
        metrics_record_http_request(method, path, 200, duration_us);
        http_status_code = 200;
        log_info("http_request", &ctx, method, path, 200, latency_ms);
    } else if (strcmp(method, "GET") == 0 && strcmp(path, "/api/v1/extensions/circuit-breakers") == 0) {
        handle_circuit_breakers(client_fd, &ctx);
        latency_ms = (int)((clock() - start_clock) * 1000 / CLOCKS_PER_SEC);
        record_latency_ms(latency_ms);
        gettimeofday(&end_time, NULL);
        uint64_t duration_us = ((uint64_t)(end_time.tv_sec - start_time.tv_sec)) * 1000000ULL + 
                              (uint64_t)(end_time.tv_usec - start_time.tv_usec);
        metrics_record_http_request(method, path, 200, duration_us);
        http_status_code = 200;
        log_info("http_request", &ctx, method, path, 200, latency_ms);
    } else if (strcmp(method, "POST") == 0 && strcmp(path, "/api/v1/policies/dry-run") == 0) {
        handle_dry_run_pipeline(client_fd, body, &ctx);
        latency_ms = (int)((clock() - start_clock) * 1000 / CLOCKS_PER_SEC);
        record_latency_ms(latency_ms);
        gettimeofday(&end_time, NULL);
        uint64_t duration_us = ((uint64_t)(end_time.tv_sec - start_time.tv_sec)) * 1000000ULL + 
                              (uint64_t)(end_time.tv_usec - start_time.tv_usec);
        metrics_record_http_request(method, path, 200, duration_us);
        http_status_code = 200;
        log_info("http_request", &ctx, method, path, 200, latency_ms);
    } else if (strcmp(method, "GET") == 0 &&
               strncmp(path, "/api/v1/policies/", strlen("/api/v1/policies/")) == 0) {
        /* Parse /api/v1/policies/:tenant_id/:policy_id/complexity */
        const char *prefix = "/api/v1/policies/";
        const char *suffix = strstr(path + strlen(prefix), "/complexity");
        if (suffix != NULL) {
            /* Extract tenant_id and policy_id from path */
            char tenant_id[64] = {0};
            char policy_id[64] = {0};
            size_t path_len = (size_t)(suffix - (path + strlen(prefix)));
            if (path_len > 0 && path_len < 128) {
                char temp_path[128];
                strncpy(temp_path, path + strlen(prefix), path_len);
                temp_path[path_len] = '\0';
                
                /* Split by '/' to get tenant_id and policy_id */
                char *slash = strchr(temp_path, '/');
                if (slash != NULL) {
                    *slash = '\0';
                    strncpy(tenant_id, temp_path, sizeof(tenant_id) - 1);
                    strncpy(policy_id, slash + 1, sizeof(policy_id) - 1);
                } else {
                    /* If no slash, treat entire path as policy_id (fallback) */
                    strncpy(policy_id, temp_path, sizeof(policy_id) - 1);
                }
            }
            
            if (tenant_id[0] != '\0' && policy_id[0] != '\0') {
                handle_pipeline_complexity(client_fd, tenant_id, policy_id, &ctx);
                latency_ms = (int)((clock() - start_clock) * 1000 / CLOCKS_PER_SEC);
                record_latency_ms(latency_ms);
                gettimeofday(&end_time, NULL);
                uint64_t duration_us = ((uint64_t)(end_time.tv_sec - start_time.tv_sec)) * 1000000ULL + 
                                      (uint64_t)(end_time.tv_usec - start_time.tv_usec);
                metrics_record_http_request(method, path, 200, duration_us);
                http_status_code = 200;
                log_info("http_request", &ctx, method, path, 200, latency_ms);
            } else {
                send_error_response(client_fd, "HTTP/1.1 400 Bad Request", "INVALID_REQUEST", "missing tenant_id or policy_id", &ctx);
                close(client_fd);
                return;
            }
        } else {
            /* Not a complexity endpoint, continue with other handlers */
            send_error_response(client_fd, "HTTP/1.1 404 Not Found", "NOT_FOUND", "endpoint not found", &ctx);
            close(client_fd);
            return;
        }
    } else {
        http_status_code = 404;
        send_error_response(client_fd,
                            "HTTP/1.1 404 Not Found",
                            "invalid_request",
                            "route not found",
                            &ctx);
        latency_ms = (int)((clock() - start_clock) * 1000 / CLOCKS_PER_SEC);
        record_latency_ms(latency_ms);
        gettimeofday(&end_time, NULL);
        uint64_t duration_us = ((uint64_t)(end_time.tv_sec - start_time.tv_sec)) * 1000000ULL + 
                              (uint64_t)(end_time.tv_usec - start_time.tv_usec);
        metrics_record_http_request(method ? method : "UNKNOWN", path ? path : "UNKNOWN", 404, duration_us);
    }
    
    // End HTTP span with status
    if (http_span) {
        otel_span_set_attribute_int(http_span, "http.status_code", http_status_code);
        otel_span_set_status(http_span, (http_status_code < 400) ? SPAN_STATUS_OK : SPAN_STATUS_ERROR);
        otel_span_end(http_span);
    }

    if (!keep_open) close(client_fd);
}

int http_server_run(const char *port_str) {
    int port = 8080;
    if (port_str != NULL) {
        port = atoi(port_str);
        if (port <= 0) {
            port = 8080;
        }
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    (void)setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 16) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("C-Gateway listening on port %d\n", port);
    start_time_sec = time(NULL);
    sse_init_pool();
    
    // Initialize Prometheus metrics
    if (metrics_registry_init() != 0) {
        fprintf(stderr, "Failed to initialize metrics registry\n");
        close(server_fd);
        return 1;
    }
    
    // Initialize OpenTelemetry tracing
    {
        const char *otlp_endpoint = getenv("OTLP_ENDPOINT");
        if (!otlp_endpoint || otlp_endpoint[0] == '\0') {
            otlp_endpoint = "http://localhost:4318";  // Default OTLP HTTP endpoint
        }
        if (otel_init("c-gateway", otlp_endpoint) != 0) {
            fprintf(stderr, "Warning: Failed to initialize OpenTelemetry tracing (tracing disabled)\n");
            // Continue without tracing - graceful degradation
        } else {
            printf("OpenTelemetry tracing enabled: %s\n", otlp_endpoint);
        }
    }

    for (;;) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }
        handle_client(client_fd);
    }

    /* not reached */
    /* close(server_fd); */
    /* return 0; */
}
