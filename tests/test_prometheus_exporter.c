/**
 * test_prometheus_exporter.c - Unit tests for Prometheus exporter
 */

#include "prometheus_exporter.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <math.h>

static void test_counter(void) {
    printf("Test: counter metric... ");
    
    prom_registry_t *registry = prom_registry_create();
    assert(registry != NULL);
    
    prom_metric_t *counter = prom_counter_register(
        registry, "test_counter", "Test counter metric"
    );
    assert(counter != NULL);
    
    /* Increment */
    prom_counter_inc(counter, 1.0);
    prom_counter_inc(counter, 2.5);
    
    /* Export */
    char buf[4096];
    ssize_t len = prom_registry_export(registry, buf, sizeof(buf));
    assert(len > 0);
    
    /* Verify output */
    assert(strstr(buf, "# HELP test_counter Test counter metric") != NULL);
    assert(strstr(buf, "# TYPE test_counter counter") != NULL);
    assert(strstr(buf, "test_counter 3.5") != NULL);
    
    prom_registry_destroy(registry);
    printf("OK\n");
}

static void test_gauge(void) {
    printf("Test: gauge metric... ");
    
    prom_registry_t *registry = prom_registry_create();
    assert(registry != NULL);
    
    prom_metric_t *gauge = prom_gauge_register(
        registry, "test_gauge", "Test gauge metric"
    );
    assert(gauge != NULL);
    
    /* Set/inc/dec */
    prom_gauge_set(gauge, 10.0);
    prom_gauge_inc(gauge, 5.0);
    prom_gauge_dec(gauge, 3.0);
    
    /* Export */
    char buf[4096];
    ssize_t len = prom_registry_export(registry, buf, sizeof(buf));
    assert(len > 0);
    
    /* Verify output */
    assert(strstr(buf, "# TYPE test_gauge gauge") != NULL);
    assert(strstr(buf, "test_gauge 12.0") != NULL);
    
    prom_registry_destroy(registry);
    printf("OK\n");
}

static void test_histogram(void) {
    printf("Test: histogram metric... ");
    
    prom_registry_t *registry = prom_registry_create();
    assert(registry != NULL);
    
    /* Define buckets: 0.001, 0.01, 0.1, 1.0 */
    double buckets[] = {0.001, 0.01, 0.1, 1.0};
    prom_metric_t *hist = prom_histogram_register(
        registry, "test_histogram", "Test histogram",
        buckets, 4
    );
    assert(hist != NULL);
    
    /* Observe values */
    prom_histogram_observe(hist, 0.0005);  /* bucket 0.001 */
    prom_histogram_observe(hist, 0.005);   /* bucket 0.01 */
    prom_histogram_observe(hist, 0.05);    /* bucket 0.1 */
    prom_histogram_observe(hist, 0.5);     /* bucket 1.0 */
    prom_histogram_observe(hist, 2.0);     /* bucket +Inf */
    
    /* Export */
    char buf[4096];
    ssize_t len = prom_registry_export(registry, buf, sizeof(buf));
    assert(len > 0);
    
    /* Verify output */
    assert(strstr(buf, "# TYPE test_histogram histogram") != NULL);
    assert(strstr(buf, "test_histogram_bucket{le=\"0.001000\"} 1") != NULL);
    assert(strstr(buf, "test_histogram_bucket{le=\"0.010000\"} 2") != NULL);
    assert(strstr(buf, "test_histogram_bucket{le=\"0.100000\"} 3") != NULL);
    assert(strstr(buf, "test_histogram_bucket{le=\"1.000000\"} 4") != NULL);
    assert(strstr(buf, "test_histogram_bucket{le=\"inf\"} 5") != NULL);
    assert(strstr(buf, "test_histogram_sum 2.5555") != NULL);
    assert(strstr(buf, "test_histogram_count 5") != NULL);
    
    prom_registry_destroy(registry);
    printf("OK\n");
}

static void test_multiple_metrics(void) {
    printf("Test: multiple metrics... ");
    
    prom_registry_t *registry = prom_registry_create();
    assert(registry != NULL);
    
    prom_metric_t *c1 = prom_counter_register(registry, "counter1", "First counter");
    prom_metric_t *g1 = prom_gauge_register(registry, "gauge1", "First gauge");
    
    assert(c1 != NULL);
    assert(g1 != NULL);
    
    prom_counter_inc(c1, 42.0);
    prom_gauge_set(g1, 99.9);
    
    /* Export */
    char buf[4096];
    ssize_t len = prom_registry_export(registry, buf, sizeof(buf));
    assert(len > 0);
    
    /* Verify both metrics present */
    assert(strstr(buf, "counter1 42.0") != NULL);
    assert(strstr(buf, "gauge1 99.9") != NULL);
    
    prom_registry_destroy(registry);
    printf("OK\n");
}

int main(void) {
    printf("=== Prometheus Exporter Tests ===\n");
    
    test_counter();
    test_gauge();
    test_histogram();
    test_multiple_metrics();
    
    printf("\nAll tests passed!\n");
    return 0;
}
