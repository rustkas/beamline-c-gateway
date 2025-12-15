#ifndef OTLP_EXPORTER_H
#define OTLP_EXPORTER_H

#include "otel.h"

/**
 * OTLP export mode
 */
typedef enum {
    OTLP_MODE_GRPC = 0,  // Export via gRPC (default, port 4317)
    OTLP_MODE_HTTP = 1   // Export via HTTP/JSON (port 4318)
} otlp_mode_t;

/**
 * Initialize OTLP exporter
 * @param endpoint OTLP endpoint (e.g., "http://localhost:4317")
 * @param mode Export mode (gRPC or HTTP)
 * @return 0 on success, -1 on error
 */
int otlp_exporter_init(const char *endpoint, otlp_mode_t mode);

/**
 * Export a single span to OTLP collector
 * @param span Span to export
 * @return 0 on success, -1 on error
 */
int otlp_exporter_export_span(const otel_span_t *span);

/**
 * Export multiple spans in batch
 * @param spans Array of spans
 * @param count Number of spans
 * @return Number of successfully exported spans
 */
int otlp_exporter_export_batch(const otel_span_t **spans, size_t count);

/**
 * Flush pending spans (blocking)
 * Call before shutdown
 */
void otlp_exporter_flush(void);

/**
 * Cleanup exporter
 */
void otlp_exporter_cleanup(void);

#endif // OTLP_EXPORTER_H
