#include "metrics_handler.h"
#include "../metrics/prometheus.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>

// Buffer for metrics export (512KB should be enough)
#define METRICS_BUFFER_SIZE (512 * 1024)
static char metrics_buffer[METRICS_BUFFER_SIZE];

/**
 * Handle GET /metrics request
 * Returns Prometheus text format metrics
 */
int handle_metrics_request(int client_fd) {
    // Export metrics to text format
    int bytes_written = prometheus_export_text(metrics_buffer, METRICS_BUFFER_SIZE);
    
    if (bytes_written < 0) {
        // Export failed
        const char *error_response =
            "HTTP/1.1 500 Internal Server Error\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 35\r\n"
            "\r\n"
            "Internal Server Error: metrics export failed\n";
        (void)write(client_fd, error_response, strlen(error_response));
        return -1;
    }
    
    // Success - return metrics
    char headers[512];
    int header_len = snprintf(headers, sizeof(headers),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain; version=0.0.4; charset=utf-8\r\n"
        "Content-Length: %d\r\n"
        "\r\n",
        bytes_written);
    
    if (header_len < 0 || (size_t)header_len >= sizeof(headers)) {
        return -1;
    }
    
    // Send headers
    if (write(client_fd, headers, (size_t)header_len) < 0) {
        return -1;
    }
    
    // Send metrics body
    if (write(client_fd, metrics_buffer, (size_t)bytes_written) < 0) {
        return -1;
    }
    
    return 0;
}

