/**
 * prometheus_exporter.h - Prometheus metrics exporter
 * 
 * Provides Prometheus-compatible metric types and text format exposition
 */

#ifndef PROMETHEUS_EXPORTER_H
#define PROMETHEUS_EXPORTER_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Metric types
 */
typedef enum {
    PROM_METRIC_COUNTER,    /* Monotonically increasing */
    PROM_METRIC_GAUGE,      /* Can go up/down */
    PROM_METRIC_HISTOGRAM   /* Distribution with buckets */
} prom_metric_type_t;

/**
 * Metric handle (opaque)
 */
typedef struct prom_metric prom_metric_t;

/**
 * Registry handle (opaque)
 */
typedef struct prom_registry prom_registry_t;

/**
 * Create metric registry
 * 
 * @return Registry handle, or NULL on error
 */
prom_registry_t* prom_registry_create(void);

/**
 * Destroy registry and all metrics
 */
void prom_registry_destroy(prom_registry_t *registry);

/**
 * Register a counter metric
 * 
 * @param registry  Registry
 * @param name      Metric name (must follow Prometheus naming)
 * @param help      Help text
 * @return Metric handle, or NULL on error
 */
prom_metric_t* prom_counter_register(
    prom_registry_t *registry,
    const char *name,
    const char *help
);

/**
 * Increment counter
 * 
 * @param metric  Counter metric
 * @param value   Amount to add (must be >= 0)
 */
void prom_counter_inc(prom_metric_t *metric, double value);

/**
 * Register a gauge metric
 * 
 * @param registry  Registry
 * @param name      Metric name
 * @param help      Help text
 * @return Metric handle, or NULL on error
 */
prom_metric_t* prom_gauge_register(
    prom_registry_t *registry,
    const char *name,
    const char *help
);

/**
 * Set gauge value
 */
void prom_gauge_set(prom_metric_t *metric, double value);

/**
 * Increment gauge
 */
void prom_gauge_inc(prom_metric_t *metric, double value);

/**
 * Decrement gauge
 */
void prom_gauge_dec(prom_metric_t *metric, double value);

/**
 * Register a histogram metric
 * 
 * @param registry  Registry
 * @param name      Metric name
 * @param help      Help text
 * @param buckets   Array of bucket boundaries (must be sorted)
 * @param num_buckets  Number of buckets
 * @return Metric handle, or NULL on error
 */
prom_metric_t* prom_histogram_register(
    prom_registry_t *registry,
    const char *name,
    const char *help,
    const double *buckets,
    size_t num_buckets
);

/**
 * Observe value in histogram
 * 
 * @param metric  Histogram metric
 * @param value   Observed value
 */
void prom_histogram_observe(prom_metric_t *metric, double value);

/**
 * Export metrics in Prometheus text format
 * 
 * @param registry  Registry
 * @param buf       Output buffer
 * @param bufsize   Buffer size
 * @return Number of bytes written, or -1 on error
 */
ssize_t prom_registry_export(
    prom_registry_t *registry,
    char *buf,
    size_t bufsize
);

#ifdef __cplusplus
}
#endif

#endif /* PROMETHEUS_EXPORTER_H */
