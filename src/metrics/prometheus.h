#ifndef PROMETHEUS_H
#define PROMETHEUS_H

#include <stdint.h>
#include <stdbool.h>
#include "../utils/atomic_counter.h"

// Maximum label length
#define MAX_LABEL_LENGTH 128
#define MAX_METRIC_NAME_LENGTH 128
#define MAX_LABELS 8

/**
 * Metric types supported
 */
typedef enum {
    METRIC_TYPE_COUNTER,   // Monotonically increasing counter
    METRIC_TYPE_GAUGE,     // Value that can go up or down
    METRIC_TYPE_HISTOGRAM  // Latency/duration histogram
} metric_type_t;

/**
 * Label key-value pair for metric dimensions
 */
typedef struct {
    char key[MAX_LABEL_LENGTH];
    char value[MAX_LABEL_LENGTH];
} metric_label_t;

/**
 * Counter metric (monotonically increasing)
 */
typedef struct {
    char name[MAX_METRIC_NAME_LENGTH];
    char help[256];
    atomic_counter_t value;
    metric_label_t labels[MAX_LABELS];
    size_t num_labels;
} prometheus_counter_t;

/**
 * Gauge metric (can increase or decrease)
 */
typedef struct {
    char name[MAX_METRIC_NAME_LENGTH];
    char help[256];
    atomic_int_fast64_t value;  // Can be negative
    metric_label_t labels[MAX_LABELS];
    size_t num_labels;
} prometheus_gauge_t;

/**
 * Histogram bucket for latency tracking
 */
typedef struct {
    double upper_bound;  // Upper bound in seconds (e.g., 0.001 = 1ms)
    atomic_counter_t count;
} histogram_bucket_t;

/**
 * Histogram metric for latency/duration
 */
typedef struct {
    char name[MAX_METRIC_NAME_LENGTH];
    char help[256];
    histogram_bucket_t buckets[10];  // Standard buckets: 1ms, 5ms, 10ms, 50ms, 100ms, 500ms, 1s, 5s, 10s, +Inf
    size_t num_buckets;
    atomic_counter_t sum;   // Total sum in microseconds
    atomic_counter_t count; // Total count
    metric_label_t labels[MAX_LABELS];
    size_t num_labels;
} prometheus_histogram_t;

// === API Functions ===

/**
 * Initialize Prometheus metrics subsystem
 * Must be called once at gateway startup
 */
int prometheus_init(void);

/**
 * Cleanup Prometheus metrics subsystem
 * Call at gateway shutdown
 */
void prometheus_cleanup(void);

/**
 * Create a new counter metric
 * @param name Metric name (e.g., "gateway_http_requests_total")
 * @param help Help text describing the metric
 * @return Pointer to counter or NULL on error
 */
prometheus_counter_t* prometheus_counter_create(const char *name, const char *help);

/**
 * Increment counter by 1
 */
void prometheus_counter_inc(prometheus_counter_t *counter);

/**
 * Add labels to counter (before first use)
 */
void prometheus_counter_add_label(prometheus_counter_t *counter, const char *key, const char *value);

/**
 * Create a new gauge metric
 */
prometheus_gauge_t* prometheus_gauge_create(const char *name, const char *help);

/**
 * Set gauge to specific value
 */
void prometheus_gauge_set(prometheus_gauge_t *gauge, int64_t value);

/**
 * Increment gauge by delta
 */
void prometheus_gauge_add(prometheus_gauge_t *gauge, int64_t delta);

/**
 * Create a new histogram metric with default buckets
 * Default buckets: 0.001, 0.005, 0.01, 0.05, 0.1, 0.5, 1.0, 5.0, 10.0, +Inf (seconds)
 */
prometheus_histogram_t* prometheus_histogram_create(const char *name, const char *help);

/**
 * Observe a value in histogram (duration in microseconds)
 */
void prometheus_histogram_observe(prometheus_histogram_t *histogram, uint64_t value_us);

/**
 * Export all metrics in Prometheus text format
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 * @return Number of bytes written or -1 on error
 */
int prometheus_export_text(char *buffer, size_t buffer_size);

#endif // PROMETHEUS_H

