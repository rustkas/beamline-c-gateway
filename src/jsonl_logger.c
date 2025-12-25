/**
 * jsonl_logger.c - JSONL logging implementation
 */

#include "jsonl_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <sys/time.h>

/* Get ISO8601 timestamp */
static void get_timestamp(char *buf, size_t size) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    
    struct tm tm;
    localtime_r(&tv.tv_sec, &tm);
    
    snprintf(buf, size, "%04d-%02d-%02dT%02d:%02d:%02d.%03ldZ",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec,
             tv.tv_usec / 1000);
}

/* Get log level string */
static const char* level_to_string(log_level_t level) {
    switch (level) {
        case LOG_LEVEL_DEBUG: return "DEBUG";
        case LOG_LEVEL_INFO: return "INFO";
        case LOG_LEVEL_WARN: return "WARN";
        case LOG_LEVEL_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

void jsonl_log(log_level_t level, const log_context_t *ctx, const char *message) {
    char timestamp[32];
    get_timestamp(timestamp, sizeof(timestamp));
    
    /* Build JSONL output */
    fprintf(stderr, "{\"timestamp\":\"%s\",\"level\":\"%s\"",
            timestamp, level_to_string(level));
    
    if (ctx) {
        if (ctx->component) {
            fprintf(stderr, ",\"component\":\"%s\"", ctx->component);
        }
        if (ctx->request_id) {
            fprintf(stderr, ",\"request_id\":\"%s\"", ctx->request_id);
        }
        if (ctx->trace_id) {
            fprintf(stderr, ",\"trace_id\":\"%s\"", ctx->trace_id);
        }
        if (ctx->tenant_id) {
            fprintf(stderr, ",\"tenant_id\":\"%s\"", ctx->tenant_id);
        }
    }
    
    fprintf(stderr, ",\"message\":\"%s\"}\n", message ? message : "");
    fflush(stderr);
}

void jsonl_logf(log_level_t level, const log_context_t *ctx, const char *fmt, ...) {
    char message[1024];
    
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);
    
    jsonl_log(level, ctx, message);
}

void jsonl_log_request(const char *component, const char *request_id,
                       const char *method, size_t payload_size) {
    char timestamp[32];
    get_timestamp(timestamp, sizeof(timestamp));
    
    fprintf(stderr, 
        "{\"timestamp\":\"%s\",\"level\":\"INFO\",\"event\":\"request_received\","
        "\"component\":\"%s\",\"request_id\":\"%s\",\"method\":\"%s\","
        "\"payload_size\":%zu}\n",
        timestamp, component ? component : "unknown",
        request_id ? request_id : "none",
        method ? method : "unknown",
        payload_size);
    
    fflush(stderr);
}

void jsonl_log_response(const char *component, const char *request_id,
                        int status_code, size_t response_size, int duration_ms) {
    char timestamp[32];
    get_timestamp(timestamp, sizeof(timestamp));
    
    fprintf(stderr,
        "{\"timestamp\":\"%s\",\"level\":\"INFO\",\"event\":\"response_sent\","
        "\"component\":\"%s\",\"request_id\":\"%s\",\"status_code\":%d,"
        "\"response_size\":%zu,\"duration_ms\":%d}\n",
        timestamp, component ? component : "unknown",
        request_id ? request_id : "none",
        status_code, response_size, duration_ms);
    
    fflush(stderr);
}

void jsonl_log_error(const char *component, const char *request_id,
                     const char *error_code, const char *error_message) {
    char timestamp[32];
    get_timestamp(timestamp, sizeof(timestamp));
    
    fprintf(stderr,
        "{\"timestamp\":\"%s\",\"level\":\"ERROR\",\"event\":\"error\","
        "\"component\":\"%s\",\"request_id\":\"%s\",\"error_code\":\"%s\","
        "\"error_message\":\"%s\"}\n",
        timestamp, component ? component : "unknown",
        request_id ? request_id : "none",
        error_code ? error_code : "unknown",
        error_message ? error_message : "");
    
    fflush(stderr);
}
