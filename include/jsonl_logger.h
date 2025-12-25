/**
 * jsonl_logger.h - JSONL structured logging
 * 
 * Logs events in JSON Lines format for observability
 */

#ifndef JSONL_LOGGER_H
#define JSONL_LOGGER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Log levels
 */
typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO = 1,
    LOG_LEVEL_WARN = 2,
    LOG_LEVEL_ERROR = 3,
} log_level_t;

/**
 * Log event context (for correlation)
 */
typedef struct {
    const char *component;      /* Component name (e.g., "ipc_bridge") */
    const char *request_id;     /* Request ID (optional) */
    const char *trace_id;       /* Trace ID (optional) */
    const char *tenant_id;      /* Tenant ID (optional) */
} log_context_t;

/**
 * Log a structured event
 * 
 * @param level    Log level
 * @param ctx      Event context (can be NULL)
 * @param message  Message text
 */
void jsonl_log(log_level_t level, const log_context_t *ctx, const char *message);

/**
 * Log with formatted message
 * 
 * @param level    Log level
 * @param ctx      Event context (can be NULL)
 * @param fmt      Printf-style format
 * @param ...      Format arguments
 */
void jsonl_logf(log_level_t level, const log_context_t *ctx, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

/**
 * Log request received
 */
void jsonl_log_request(const char *component, const char *request_id, 
                       const char *method, size_t payload_size);

/**
 * Log response sent
 */
void jsonl_log_response(const char *component, const char *request_id,
                        int status_code, size_t response_size, int duration_ms);

/**
 * Log error event
 */
void jsonl_log_error(const char *component, const char *request_id,
                     const char *error_code, const char *error_message);

#ifdef __cplusplus
}
#endif

#endif /* JSONL_LOGGER_H */
