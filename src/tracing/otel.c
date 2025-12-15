#include "otel.h"
#include "otlp_exporter.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

static char service_name[128] = "c-gateway";
static bool otel_initialized = false;

int otel_init(const char *svc_name, const char *otlp_endpoint) {
    if (!svc_name || !otlp_endpoint) {
        return -1;
    }
    
    strncpy(service_name, svc_name, sizeof(service_name) - 1);
    service_name[sizeof(service_name) - 1] = '\0';
    
    // Initialize OTLP exporter (use HTTP mode by default)
    int result = otlp_exporter_init(otlp_endpoint, OTLP_MODE_HTTP);
    if (result != 0) {
        return -1;
    }
    
    otel_initialized = true;
    return 0;
}

void otel_cleanup(void) {
    if (otel_initialized) {
        otlp_exporter_flush();
        otlp_exporter_cleanup();
        otel_initialized = false;
    }
}

// Generate random span ID (64-bit)
static void generate_span_id(span_id_t *span_id) {
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        read(fd, span_id->bytes, 8);
        close(fd);
    } else {
        // Fallback to pseudo-random
        for (int i = 0; i < 8; i++) {
            span_id->bytes[i] = (uint8_t)(rand() & 0xFF);
        }
    }
}

// Generate random trace ID (128-bit)
static void generate_trace_id(trace_id_t *trace_id) {
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        read(fd, trace_id->bytes, 16);
        close(fd);
    } else {
        // Fallback to pseudo-random
        for (int i = 0; i < 16; i++) {
            trace_id->bytes[i] = (uint8_t)(rand() & 0xFF);
        }
    }
}

otel_span_t* otel_span_start(const char *name, span_kind_t kind, otel_span_t *parent) {
    if (!name) return NULL;
    
    otel_span_t *span = calloc(1, sizeof(otel_span_t));
    if (!span) return NULL;
    
    strncpy(span->name, name, sizeof(span->name) - 1);
    span->name[sizeof(span->name) - 1] = '\0';
    span->kind = kind;
    span->status = SPAN_STATUS_UNSET;
    span->parent = parent;
    
    // Inherit trace_id from parent or generate new
    if (parent) {
        memcpy(&span->trace_id, &parent->trace_id, sizeof(trace_id_t));
        memcpy(&span->parent_span_id, &parent->span_id, sizeof(span_id_t));
    } else {
        generate_trace_id(&span->trace_id);
        memset(&span->parent_span_id, 0, sizeof(span_id_t));
    }
    
    generate_span_id(&span->span_id);
    
    clock_gettime(CLOCK_REALTIME, &span->start_time);
    span->num_attributes = 0;
    
    return span;
}

void otel_span_end(otel_span_t *span) {
    if (!span) return;
    
    clock_gettime(CLOCK_REALTIME, &span->end_time);
    
    // Export span to OTLP collector (non-blocking, may fail silently)
    if (otel_initialized) {
        otlp_exporter_export_span(span);
    }
    
    free(span);
}

void otel_span_set_attribute(otel_span_t *span, const char *key, const char *value) {
    if (!span || !key || !value || span->num_attributes >= 32) return;
    
    span_attribute_t *attr = &span->attributes[span->num_attributes++];
    strncpy(attr->key, key, sizeof(attr->key) - 1);
    attr->key[sizeof(attr->key) - 1] = '\0';
    strncpy(attr->value, value, sizeof(attr->value) - 1);
    attr->value[sizeof(attr->value) - 1] = '\0';
}

void otel_span_set_attribute_int(otel_span_t *span, const char *key, int64_t value) {
    if (!span || !key || span->num_attributes >= 32) return;
    
    char value_str[32];
    snprintf(value_str, sizeof(value_str), "%ld", (long)value);
    otel_span_set_attribute(span, key, value_str);
}

void otel_span_set_status(otel_span_t *span, span_status_t status) {
    if (span) {
        span->status = status;
    }
}

// Parse W3C traceparent: "00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01"
int otel_extract_trace_context(const char *traceparent, trace_id_t *trace_id, span_id_t *span_id) {
    if (!traceparent || !trace_id || !span_id || strlen(traceparent) < 55) {
        return -1;
    }
    
    // Parse version (00), trace_id (32 hex chars), span_id (16 hex chars), flags (01)
    char version[3], tid[33], sid[17], flags[3];
    
    if (sscanf(traceparent, "%2s-%32s-%16s-%2s", version, tid, sid, flags) != 4) {
        return -1;
    }
    
    // Convert hex strings to bytes
    for (int i = 0; i < 16; i++) {
        unsigned int byte_val;
        if (sscanf(&tid[i*2], "%2x", &byte_val) != 1) {
            return -1;
        }
        trace_id->bytes[i] = (uint8_t)byte_val;
    }
    
    for (int i = 0; i < 8; i++) {
        unsigned int byte_val;
        if (sscanf(&sid[i*2], "%2x", &byte_val) != 1) {
            return -1;
        }
        span_id->bytes[i] = (uint8_t)byte_val;
    }
    
    return 0;
}

// Inject W3C traceparent: "00-{trace_id}-{span_id}-01"
int otel_inject_trace_context(const trace_id_t *trace_id, const span_id_t *span_id, char *buffer) {
    if (!trace_id || !span_id || !buffer) {
        return -1;
    }
    
    int offset = sprintf(buffer, "00-");
    
    // Trace ID (32 hex chars)
    for (int i = 0; i < 16; i++) {
        offset += sprintf(buffer + offset, "%02x", trace_id->bytes[i]);
    }
    
    offset += sprintf(buffer + offset, "-");
    
    // Span ID (16 hex chars)
    for (int i = 0; i < 8; i++) {
        offset += sprintf(buffer + offset, "%02x", span_id->bytes[i]);
    }
    
    offset += sprintf(buffer + offset, "-01");
    
    return offset;
}
