#include "otlp_exporter.h"

#ifdef HAVE_CURL
#include <curl/curl.h>
#endif
#include <jansson.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static char otlp_endpoint[256];
static otlp_mode_t export_mode = OTLP_MODE_HTTP;
#ifdef HAVE_CURL
static CURL *curl_handle = NULL;
#endif
static bool exporter_initialized = false;

int otlp_exporter_init(const char *endpoint, otlp_mode_t mode) {
    if (!endpoint) {
        return -1;
    }
    
    strncpy(otlp_endpoint, endpoint, sizeof(otlp_endpoint) - 1);
    otlp_endpoint[sizeof(otlp_endpoint) - 1] = '\0';
    export_mode = mode;
    
    // For HTTP mode, append /v1/traces if not already present
    if (mode == OTLP_MODE_HTTP) {
        if (strstr(otlp_endpoint, "/v1/traces") == NULL) {
            size_t len = strlen(otlp_endpoint);
            if (len < sizeof(otlp_endpoint) - 10) {
                strncat(otlp_endpoint, "/v1/traces", sizeof(otlp_endpoint) - len - 1);
            }
        }
    }
    
#ifdef HAVE_CURL
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl_handle = curl_easy_init();
    
    if (!curl_handle) {
        curl_global_cleanup();
        return -1;
    }
    
    exporter_initialized = true;
    return 0;
#else
    // libcurl not available - tracing disabled
    exporter_initialized = false;
    return -1;
#endif
}

void otlp_exporter_cleanup(void) {
#ifdef HAVE_CURL
    if (curl_handle) {
        curl_easy_cleanup(curl_handle);
        curl_handle = NULL;
    }
    curl_global_cleanup();
#endif
    exporter_initialized = false;
}

void otlp_exporter_flush(void) {
    // For HTTP mode, no batching - spans are sent immediately
    // This is a placeholder for future batching support
    (void)0;
}

// Convert trace_id to hex string
#ifdef HAVE_CURL
static void trace_id_to_hex(const trace_id_t *trace_id, char *hex, size_t hex_size) {
    if (hex_size < 33) return;
    for (int i = 0; i < 16; i++) {
        sprintf(&hex[i*2], "%02x", trace_id->bytes[i]);
    }
    hex[32] = '\0';
}

// Convert span_id to hex string
static void span_id_to_hex(const span_id_t *span_id, char *hex, size_t hex_size) {
    if (hex_size < 17) return;
    for (int i = 0; i < 8; i++) {
        sprintf(&hex[i*2], "%02x", span_id->bytes[i]);
    }
    hex[16] = '\0';
}
#endif

// Convert span to OTLP JSON format
#ifdef HAVE_CURL
static json_t* span_to_otlp_json(const otel_span_t *span) {
    if (!span) return NULL;
    
    json_t *span_json = json_object();
    
    // Trace ID (hex string)
    char trace_id_hex[33];
    trace_id_to_hex(&span->trace_id, trace_id_hex, sizeof(trace_id_hex));
    json_object_set_new(span_json, "traceId", json_string(trace_id_hex));
    
    // Span ID (hex string)
    char span_id_hex[17];
    span_id_to_hex(&span->span_id, span_id_hex, sizeof(span_id_hex));
    json_object_set_new(span_json, "spanId", json_string(span_id_hex));
    
    // Parent span ID (if present)
    bool has_parent = false;
    for (int i = 0; i < 8; i++) {
        if (span->parent_span_id.bytes[i] != 0) {
            has_parent = true;
            break;
        }
    }
    if (has_parent) {
        char parent_span_id_hex[17];
        span_id_to_hex(&span->parent_span_id, parent_span_id_hex, sizeof(parent_span_id_hex));
        json_object_set_new(span_json, "parentSpanId", json_string(parent_span_id_hex));
    }
    
    // Span name
    json_object_set_new(span_json, "name", json_string(span->name));
    
    // Kind
    json_object_set_new(span_json, "kind", json_integer(span->kind));
    
    // Start/End time (nanoseconds since epoch)
    uint64_t start_ns = (uint64_t)span->start_time.tv_sec * 1000000000ULL + (uint64_t)span->start_time.tv_nsec;
    uint64_t end_ns = (uint64_t)span->end_time.tv_sec * 1000000000ULL + (uint64_t)span->end_time.tv_nsec;
    // jansson doesn't support uint64_t directly, use string representation
    char start_ns_str[32], end_ns_str[32];
    snprintf(start_ns_str, sizeof(start_ns_str), "%llu", (unsigned long long)start_ns);
    snprintf(end_ns_str, sizeof(end_ns_str), "%llu", (unsigned long long)end_ns);
    json_object_set_new(span_json, "startTimeUnixNano", json_string(start_ns_str));
    json_object_set_new(span_json, "endTimeUnixNano", json_string(end_ns_str));
    
    // Attributes
    json_t *attributes = json_array();
    for (size_t i = 0; i < span->num_attributes; i++) {
        json_t *attr = json_object();
        json_object_set_new(attr, "key", json_string(span->attributes[i].key));
        
        json_t *value_obj = json_object();
        json_object_set_new(value_obj, "stringValue", json_string(span->attributes[i].value));
        json_object_set_new(attr, "value", value_obj);
        
        json_array_append_new(attributes, attr);
    }
    json_object_set_new(span_json, "attributes", attributes);
    
    // Status
    json_t *status = json_object();
    json_object_set_new(status, "code", json_integer(span->status));
    json_object_set_new(span_json, "status", status);
    
    return span_json;
}
#endif

int otlp_exporter_export_span(const otel_span_t *span) {
    if (!span) {
        return -1;
    }
    
#ifdef HAVE_CURL
    if (!curl_handle || !exporter_initialized) {
        return -1;
    }
    
    // Build OTLP JSON payload
    json_t *root = json_object();
    json_t *resource_spans = json_array();
    json_t *resource_span = json_object();
    json_t *scope_spans = json_array();
    json_t *scope_span = json_object();
    json_t *spans = json_array();
    
    json_t *span_json = span_to_otlp_json(span);
    if (!span_json) {
        json_decref(root);
        return -1;
    }
    
    json_array_append_new(spans, span_json);
    json_object_set_new(scope_span, "spans", spans);
    
    // Scope info
    json_t *scope = json_object();
    json_object_set_new(scope, "name", json_string("c-gateway"));
    json_object_set_new(scope, "version", json_string("1.0.0"));
    json_object_set_new(scope_span, "scope", scope);
    
    json_array_append_new(scope_spans, scope_span);
    json_object_set_new(resource_span, "scopeSpans", scope_spans);
    
    // Resource info
    json_t *resource = json_object();
    json_t *resource_attrs = json_array();
    json_t *service_name_attr = json_object();
    json_object_set_new(service_name_attr, "key", json_string("service.name"));
    json_t *service_name_value = json_object();
    json_object_set_new(service_name_value, "stringValue", json_string("c-gateway"));
    json_object_set_new(service_name_attr, "value", service_name_value);
    json_array_append_new(resource_attrs, service_name_attr);
    json_object_set_new(resource, "attributes", resource_attrs);
    json_object_set_new(resource_span, "resource", resource);
    
    json_array_append_new(resource_spans, resource_span);
    json_object_set_new(root, "resourceSpans", resource_spans);
    
    // Convert to JSON string
    char *json_str = json_dumps(root, JSON_COMPACT);
    json_decref(root);
    
    if (!json_str) {
        return -1;
    }
    
    // Send via HTTP POST
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    
    curl_easy_setopt(curl_handle, CURLOPT_URL, otlp_endpoint);
    curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, json_str);
    curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 5L);  // 5 second timeout
    curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, 2L);  // 2 second connect timeout
    
    // Don't fail if collector is unavailable (graceful degradation)
    CURLcode res = curl_easy_perform(curl_handle);
    
    free(json_str);
    curl_slist_free_all(headers);
    
    // Return 0 even on error to avoid breaking request flow
    // Errors are logged by curl if CURLOPT_VERBOSE is set
    (void)res;
    return 0;
#else
    // libcurl not available - tracing disabled
    (void)span;
    return -1;
#endif
}

int otlp_exporter_export_batch(const otel_span_t **spans, size_t count) {
    if (!spans || count == 0) {
        return 0;
    }
    
    int exported = 0;
    for (size_t i = 0; i < count; i++) {
        if (otlp_exporter_export_span(spans[i]) == 0) {
            exported++;
        }
    }
    
    return exported;
}
