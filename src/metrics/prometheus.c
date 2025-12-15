#include "prometheus.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <math.h>
#include <stdatomic.h>

// Global metrics storage
#define MAX_METRICS 100
static prometheus_counter_t *counters[MAX_METRICS];
static prometheus_gauge_t *gauges[MAX_METRICS];
static prometheus_histogram_t *histograms[MAX_METRICS];
static size_t num_counters = 0;
static size_t num_gauges = 0;
static size_t num_histograms = 0;
static pthread_mutex_t registry_mutex = PTHREAD_MUTEX_INITIALIZER;

// Default histogram buckets (in seconds)
static const double default_buckets[] = {
    0.001,  // 1ms
    0.005,  // 5ms
    0.01,   // 10ms
    0.05,   // 50ms
    0.1,    // 100ms
    0.5,    // 500ms
    1.0,    // 1s
    5.0,    // 5s
    10.0,   // 10s
    INFINITY // +Inf
};
static const size_t default_buckets_count = 10;

int prometheus_init(void) {
    // Initialize registry
    num_counters = 0;
    num_gauges = 0;
    num_histograms = 0;
    return 0;
}

void prometheus_cleanup(void) {
    pthread_mutex_lock(&registry_mutex);
    
    // Free all counters
    for (size_t i = 0; i < num_counters; i++) {
        free(counters[i]);
    }
    
    // Free all gauges
    for (size_t i = 0; i < num_gauges; i++) {
        free(gauges[i]);
    }
    
    // Free all histograms
    for (size_t i = 0; i < num_histograms; i++) {
        free(histograms[i]);
    }
    
    num_counters = 0;
    num_gauges = 0;
    num_histograms = 0;
    
    pthread_mutex_unlock(&registry_mutex);
}

prometheus_counter_t* prometheus_counter_create(const char *name, const char *help) {
    if (num_counters >= MAX_METRICS) {
        return NULL;
    }
    
    prometheus_counter_t *counter = calloc(1, sizeof(prometheus_counter_t));
    if (!counter) return NULL;
    
    strncpy(counter->name, name, MAX_METRIC_NAME_LENGTH - 1);
    counter->name[MAX_METRIC_NAME_LENGTH - 1] = '\0';
    strncpy(counter->help, help, 255);
    counter->help[255] = '\0';
    atomic_counter_init(&counter->value);
    counter->num_labels = 0;
    
    pthread_mutex_lock(&registry_mutex);
    counters[num_counters++] = counter;
    pthread_mutex_unlock(&registry_mutex);
    
    return counter;
}

void prometheus_counter_inc(prometheus_counter_t *counter) {
    if (counter) {
        atomic_counter_inc(&counter->value);
    }
}

void prometheus_counter_add_label(prometheus_counter_t *counter, const char *key, const char *value) {
    if (!counter || counter->num_labels >= MAX_LABELS) return;
    
    metric_label_t *label = &counter->labels[counter->num_labels++];
    strncpy(label->key, key, MAX_LABEL_LENGTH - 1);
    label->key[MAX_LABEL_LENGTH - 1] = '\0';
    strncpy(label->value, value, MAX_LABEL_LENGTH - 1);
    label->value[MAX_LABEL_LENGTH - 1] = '\0';
}

prometheus_gauge_t* prometheus_gauge_create(const char *name, const char *help) {
    if (num_gauges >= MAX_METRICS) {
        return NULL;
    }
    
    prometheus_gauge_t *gauge = calloc(1, sizeof(prometheus_gauge_t));
    if (!gauge) return NULL;
    
    strncpy(gauge->name, name, MAX_METRIC_NAME_LENGTH - 1);
    gauge->name[MAX_METRIC_NAME_LENGTH - 1] = '\0';
    strncpy(gauge->help, help, 255);
    gauge->help[255] = '\0';
    atomic_init(&gauge->value, 0);
    gauge->num_labels = 0;
    
    pthread_mutex_lock(&registry_mutex);
    gauges[num_gauges++] = gauge;
    pthread_mutex_unlock(&registry_mutex);
    
    return gauge;
}

void prometheus_gauge_set(prometheus_gauge_t *gauge, int64_t value) {
    if (gauge) {
        atomic_store_explicit(&gauge->value, value, memory_order_relaxed);
    }
}

void prometheus_gauge_add(prometheus_gauge_t *gauge, int64_t delta) {
    if (gauge) {
        atomic_fetch_add_explicit(&gauge->value, delta, memory_order_relaxed);
    }
}

prometheus_histogram_t* prometheus_histogram_create(const char *name, const char *help) {
    if (num_histograms >= MAX_METRICS) {
        return NULL;
    }
    
    prometheus_histogram_t *histogram = calloc(1, sizeof(prometheus_histogram_t));
    if (!histogram) return NULL;
    
    strncpy(histogram->name, name, MAX_METRIC_NAME_LENGTH - 1);
    histogram->name[MAX_METRIC_NAME_LENGTH - 1] = '\0';
    strncpy(histogram->help, help, 255);
    histogram->help[255] = '\0';
    histogram->num_buckets = default_buckets_count;
    histogram->num_labels = 0;
    
    // Initialize buckets
    for (size_t i = 0; i < default_buckets_count; i++) {
        histogram->buckets[i].upper_bound = default_buckets[i];
        atomic_counter_init(&histogram->buckets[i].count);
    }
    
    atomic_counter_init(&histogram->sum);
    atomic_counter_init(&histogram->count);
    
    pthread_mutex_lock(&registry_mutex);
    histograms[num_histograms++] = histogram;
    pthread_mutex_unlock(&registry_mutex);
    
    return histogram;
}

void prometheus_histogram_observe(prometheus_histogram_t *histogram, uint64_t value_us) {
    if (!histogram) return;
    
    double value_seconds = (double)value_us / 1000000.0;
    
    // Update buckets
    for (size_t i = 0; i < histogram->num_buckets; i++) {
        if (value_seconds <= histogram->buckets[i].upper_bound || isinf(histogram->buckets[i].upper_bound)) {
            atomic_counter_inc(&histogram->buckets[i].count);
        }
    }
    
    // Update sum and count
    atomic_counter_add(&histogram->sum, value_us);
    atomic_counter_inc(&histogram->count);
}

int prometheus_export_text(char *buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) {
        return -1;
    }
    
    size_t offset = 0;
    
    pthread_mutex_lock(&registry_mutex);
    
    // Export counters
    for (size_t i = 0; i < num_counters; i++) {
        prometheus_counter_t *c = counters[i];
        uint64_t value = atomic_counter_get(&c->value);
        
        // HELP line
        int written = snprintf(buffer + offset, buffer_size - offset,
                          "# HELP %s %s\n", c->name, c->help);
        if (written < 0 || (size_t)written >= buffer_size - offset) {
            pthread_mutex_unlock(&registry_mutex);
            return -1;
        }
        offset += (size_t)written;
        
        // TYPE line
        written = snprintf(buffer + offset, buffer_size - offset,
                          "# TYPE %s counter\n", c->name);
        if (written < 0 || (size_t)written >= buffer_size - offset) {
            pthread_mutex_unlock(&registry_mutex);
            return -1;
        }
        offset += (size_t)written;
        
        // Metric line with labels
        written = snprintf(buffer + offset, buffer_size - offset, "%s", c->name);
        if (written < 0 || (size_t)written >= buffer_size - offset) {
            pthread_mutex_unlock(&registry_mutex);
            return -1;
        }
        offset += (size_t)written;
        
        if (c->num_labels > 0) {
            written = snprintf(buffer + offset, buffer_size - offset, "{");
            if (written < 0 || (size_t)written >= buffer_size - offset) {
                pthread_mutex_unlock(&registry_mutex);
                return -1;
            }
            offset += (size_t)written;
            
            for (size_t j = 0; j < c->num_labels; j++) {
                if (j > 0) {
                    written = snprintf(buffer + offset, buffer_size - offset, ",");
                    if (written < 0 || (size_t)written >= buffer_size - offset) {
                        pthread_mutex_unlock(&registry_mutex);
                        return -1;
                    }
                    offset += (size_t)written;
                }
                written = snprintf(buffer + offset, buffer_size - offset,
                                  "%s=\"%s\"", c->labels[j].key, c->labels[j].value);
                if (written < 0 || (size_t)written >= buffer_size - offset) {
                    pthread_mutex_unlock(&registry_mutex);
                    return -1;
                }
                offset += (size_t)written;
            }
            
            written = snprintf(buffer + offset, buffer_size - offset, "}");
            if (written < 0 || (size_t)written >= buffer_size - offset) {
                pthread_mutex_unlock(&registry_mutex);
                return -1;
            }
            offset += (size_t)written;
        }
        
        written = snprintf(buffer + offset, buffer_size - offset, " %lu\n", (unsigned long)value);
        if (written < 0 || (size_t)written >= buffer_size - offset) {
            pthread_mutex_unlock(&registry_mutex);
            return -1;
        }
        offset += (size_t)written;
    }
    
    // Export gauges
    for (size_t i = 0; i < num_gauges; i++) {
        prometheus_gauge_t *g = gauges[i];
        int64_t value = atomic_load_explicit(&g->value, memory_order_relaxed);
        
        // HELP line
        int written = snprintf(buffer + offset, buffer_size - offset,
                          "# HELP %s %s\n", g->name, g->help);
        if (written < 0 || (size_t)written >= buffer_size - offset) {
            pthread_mutex_unlock(&registry_mutex);
            return -1;
        }
        offset += (size_t)written;
        
        // TYPE line
        written = snprintf(buffer + offset, buffer_size - offset,
                          "# TYPE %s gauge\n", g->name);
        if (written < 0 || (size_t)written >= buffer_size - offset) {
            pthread_mutex_unlock(&registry_mutex);
            return -1;
        }
        offset += (size_t)written;
        
        // Metric line with labels
        written = snprintf(buffer + offset, buffer_size - offset, "%s", g->name);
        if (written < 0 || (size_t)written >= buffer_size - offset) {
            pthread_mutex_unlock(&registry_mutex);
            return -1;
        }
        offset += (size_t)written;
        
        if (g->num_labels > 0) {
            written = snprintf(buffer + offset, buffer_size - offset, "{");
            if (written < 0 || (size_t)written >= buffer_size - offset) {
                pthread_mutex_unlock(&registry_mutex);
                return -1;
            }
            offset += (size_t)written;
            
            for (size_t j = 0; j < g->num_labels; j++) {
                if (j > 0) {
                    written = snprintf(buffer + offset, buffer_size - offset, ",");
                    if (written < 0 || (size_t)written >= buffer_size - offset) {
                        pthread_mutex_unlock(&registry_mutex);
                        return -1;
                    }
                    offset += (size_t)written;
                }
                written = snprintf(buffer + offset, buffer_size - offset,
                                  "%s=\"%s\"", g->labels[j].key, g->labels[j].value);
                if (written < 0 || (size_t)written >= buffer_size - offset) {
                    pthread_mutex_unlock(&registry_mutex);
                    return -1;
                }
                offset += (size_t)written;
            }
            
            written = snprintf(buffer + offset, buffer_size - offset, "}");
            if (written < 0 || (size_t)written >= buffer_size - offset) {
                pthread_mutex_unlock(&registry_mutex);
                return -1;
            }
            offset += (size_t)written;
        }
        
        written = snprintf(buffer + offset, buffer_size - offset, " %ld\n", (long)value);
        if (written < 0 || (size_t)written >= buffer_size - offset) {
            pthread_mutex_unlock(&registry_mutex);
            return -1;
        }
        offset += (size_t)written;
    }
    
    // Export histograms
    for (size_t i = 0; i < num_histograms; i++) {
        prometheus_histogram_t *h = histograms[i];
        
        // HELP and TYPE
        int written = snprintf(buffer + offset, buffer_size - offset,
                          "# HELP %s %s\n", h->name, h->help);
        if (written < 0 || (size_t)written >= buffer_size - offset) {
            pthread_mutex_unlock(&registry_mutex);
            return -1;
        }
        offset += (size_t)written;
        
        written = snprintf(buffer + offset, buffer_size - offset,
                          "# TYPE %s histogram\n", h->name);
        if (written < 0 || (size_t)written >= buffer_size - offset) {
            pthread_mutex_unlock(&registry_mutex);
            return -1;
        }
        offset += (size_t)written;
        
        // Build label string for histogram
        char label_str[512] = "";
        if (h->num_labels > 0) {
            size_t label_offset = 0;
            label_str[label_offset++] = '{';
            for (size_t j = 0; j < h->num_labels; j++) {
                if (j > 0) {
                    label_str[label_offset++] = ',';
                }
                int label_written = snprintf(label_str + label_offset, sizeof(label_str) - label_offset,
                                           "%s=\"%s\"", h->labels[j].key, h->labels[j].value);
                if (label_written > 0 && (size_t)label_written < sizeof(label_str) - label_offset) {
                    label_offset += (size_t)label_written;
                }
            }
            label_str[label_offset++] = '}';
            label_str[label_offset] = '\0';
        }
        
        // Buckets
        for (size_t j = 0; j < h->num_buckets; j++) {
            uint64_t count = atomic_counter_get(&h->buckets[j].count);
            char le_str[32];
            if (!isinf(h->buckets[j].upper_bound)) {
                snprintf(le_str, sizeof(le_str), "%.3f", h->buckets[j].upper_bound);
            } else {
                strncpy(le_str, "+Inf", sizeof(le_str) - 1);
                le_str[sizeof(le_str) - 1] = '\0';
            }
            
            // Format bucket line with labels
            if (h->num_labels > 0) {
                written = snprintf(buffer + offset, buffer_size - offset,
                                  "%s_bucket{le=\"%s\",%s} %lu\n",
                                  h->name, le_str, label_str + 1, (unsigned long)count); // +1 to skip '{'
            } else {
                written = snprintf(buffer + offset, buffer_size - offset,
                                  "%s_bucket{le=\"%s\"} %lu\n",
                                  h->name, le_str, (unsigned long)count);
            }
            if (written < 0 || (size_t)written >= buffer_size - offset) {
                pthread_mutex_unlock(&registry_mutex);
                return -1;
            }
            offset += (size_t)written;
        }
        
        // Sum and count
        uint64_t sum = atomic_counter_get(&h->sum);
        uint64_t count = atomic_counter_get(&h->count);
        
        written = snprintf(buffer + offset, buffer_size - offset,
                          "%s_sum%s %lu\n",
                          h->name, label_str, (unsigned long)sum);
        if (written < 0 || (size_t)written >= buffer_size - offset) {
            pthread_mutex_unlock(&registry_mutex);
            return -1;
        }
        offset += (size_t)written;
        
        written = snprintf(buffer + offset, buffer_size - offset,
                          "%s_count%s %lu\n",
                          h->name, label_str, (unsigned long)count);
        if (written < 0 || (size_t)written >= buffer_size - offset) {
            pthread_mutex_unlock(&registry_mutex);
            return -1;
        }
        offset += (size_t)written;
    }
    
    pthread_mutex_unlock(&registry_mutex);
    
    return (int)offset;
}

