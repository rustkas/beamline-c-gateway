/**
 * prometheus_exporter.c - Prometheus metrics implementation
 */

#define _GNU_SOURCE
#include "prometheus_exporter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define MAX_METRICS 128
#define MAX_NAME_LEN 128
#define MAX_HELP_LEN 256

/**
 * Histogram bucket
 */
typedef struct {
    double le;      /* Upper bound (less than or equal) */
    uint64_t count; /* Observations <= le */
} prom_bucket_t;

/**
 * Metric structure
 */
struct prom_metric {
    char name[MAX_NAME_LEN];
    char help[MAX_HELP_LEN];
    prom_metric_type_t type;
    pthread_mutex_t lock;
    
    union {
        /* Counter/Gauge: single value */
        double value;
        
        /* Histogram */
        struct {
            prom_bucket_t *buckets;
            size_t num_buckets;
            double sum;
            uint64_t count;
        } histogram;
    } data;
};

/**
 * Registry structure
 */
struct prom_registry {
    prom_metric_t *metrics[MAX_METRICS];
    size_t num_metrics;
    pthread_mutex_t lock;
};

prom_registry_t* prom_registry_create(void) {
    prom_registry_t *registry = calloc(1, sizeof(prom_registry_t));
    if (!registry) {
        return NULL;
    }
    
    pthread_mutex_init(&registry->lock, NULL);
    return registry;
}

void prom_registry_destroy(prom_registry_t *registry) {
    if (!registry) {
        return;
    }
    
    for (size_t i = 0; i < registry->num_metrics; i++) {
        prom_metric_t *m = registry->metrics[i];
        if (m) {
            if (m->type == PROM_METRIC_HISTOGRAM && m->data.histogram.buckets) {
                free(m->data.histogram.buckets);
            }
            pthread_mutex_destroy(&m->lock);
            free(m);
        }
    }
    
    pthread_mutex_destroy(&registry->lock);
    free(registry);
}

prom_metric_t* prom_counter_register(
    prom_registry_t *registry,
    const char *name,
    const char *help
) {
    if (!registry || !name || !help) {
        return NULL;
    }
    
    pthread_mutex_lock(&registry->lock);
    
    if (registry->num_metrics >= MAX_METRICS) {
        pthread_mutex_unlock(&registry->lock);
        return NULL;
    }
    
    prom_metric_t *metric = calloc(1, sizeof(prom_metric_t));
    if (!metric) {
        pthread_mutex_unlock(&registry->lock);
        return NULL;
    }
    
    snprintf(metric->name, sizeof(metric->name), "%s", name);
    snprintf(metric->help, sizeof(metric->help), "%s", help);
    metric->type = PROM_METRIC_COUNTER;
    metric->data.value = 0.0;
    pthread_mutex_init(&metric->lock, NULL);
    
    registry->metrics[registry->num_metrics++] = metric;
    pthread_mutex_unlock(&registry->lock);
    
    return metric;
}

void prom_counter_inc(prom_metric_t *metric, double value) {
    if (!metric || value < 0) {
        return;
    }
    
    pthread_mutex_lock(&metric->lock);
    metric->data.value += value;
    pthread_mutex_unlock(&metric->lock);
}

prom_metric_t* prom_gauge_register(
    prom_registry_t *registry,
    const char *name,
    const char *help
) {
    if (!registry || !name || !help) {
        return NULL;
    }
    
    pthread_mutex_lock(&registry->lock);
    
    if (registry->num_metrics >= MAX_METRICS) {
        pthread_mutex_unlock(&registry->lock);
        return NULL;
    }
    
    prom_metric_t *metric = calloc(1, sizeof(prom_metric_t));
    if (!metric) {
        pthread_mutex_unlock(&registry->lock);
        return NULL;
    }
    
    snprintf(metric->name, sizeof(metric->name), "%s", name);
    snprintf(metric->help, sizeof(metric->help), "%s", help);
    metric->type = PROM_METRIC_GAUGE;
    metric->data.value = 0.0;
    pthread_mutex_init(&metric->lock, NULL);
    
    registry->metrics[registry->num_metrics++] = metric;
    pthread_mutex_unlock(&registry->lock);
    
    return metric;
}

void prom_gauge_set(prom_metric_t *metric, double value) {
    if (!metric) {
        return;
    }
    
    pthread_mutex_lock(&metric->lock);
    metric->data.value = value;
    pthread_mutex_unlock(&metric->lock);
}

void prom_gauge_inc(prom_metric_t *metric, double value) {
    if (!metric) {
        return;
    }
    
    pthread_mutex_lock(&metric->lock);
    metric->data.value += value;
    pthread_mutex_unlock(&metric->lock);
}

void prom_gauge_dec(prom_metric_t *metric, double value) {
    if (!metric) {
        return;
    }
    
    pthread_mutex_lock(&metric->lock);
    metric->data.value -= value;
    pthread_mutex_unlock(&metric->lock);
}

prom_metric_t* prom_histogram_register(
    prom_registry_t *registry,
    const char *name,
    const char *help,
    const double *buckets,
    size_t num_buckets
) {
    if (!registry || !name || !help || !buckets || num_buckets == 0) {
        return NULL;
    }
    
    pthread_mutex_lock(&registry->lock);
    
    if (registry->num_metrics >= MAX_METRICS) {
        pthread_mutex_unlock(&registry->lock);
        return NULL;
    }
    
    prom_metric_t *metric = calloc(1, sizeof(prom_metric_t));
    if (!metric) {
        pthread_mutex_unlock(&registry->lock);
        return NULL;
    }
    
    /* Allocate buckets (+1 for +Inf) */
    prom_bucket_t *hist_buckets = calloc(num_buckets + 1, sizeof(prom_bucket_t));
    if (!hist_buckets) {
        free(metric);
        pthread_mutex_unlock(&registry->lock);
        return NULL;
    }
    
    /* Copy bucket boundaries */
    for (size_t i = 0; i < num_buckets; i++) {
        hist_buckets[i].le = buckets[i];
        hist_buckets[i].count = 0;
    }
    hist_buckets[num_buckets].le = __builtin_inf();  /* +Inf bucket */
    hist_buckets[num_buckets].count = 0;
    
    snprintf(metric->name, sizeof(metric->name), "%s", name);
    snprintf(metric->help, sizeof(metric->help), "%s", help);
    metric->type = PROM_METRIC_HISTOGRAM;
    metric->data.histogram.buckets = hist_buckets;
    metric->data.histogram.num_buckets = num_buckets + 1;
    metric->data.histogram.sum = 0.0;
    metric->data.histogram.count = 0;
    pthread_mutex_init(&metric->lock, NULL);
    
    registry->metrics[registry->num_metrics++] = metric;
    pthread_mutex_unlock(&registry->lock);
    
    return metric;
}

void prom_histogram_observe(prom_metric_t *metric, double value) {
    if (!metric || metric->type != PROM_METRIC_HISTOGRAM) {
        return;
    }
    
    pthread_mutex_lock(&metric->lock);
    
    metric->data.histogram.sum += value;
    metric->data.histogram.count++;
    
    /* Update buckets */
    for (size_t i = 0; i < metric->data.histogram.num_buckets; i++) {
        if (value <= metric->data.histogram.buckets[i].le) {
            metric->data.histogram.buckets[i].count++;
        }
    }
    
    pthread_mutex_unlock(&metric->lock);
}

ssize_t prom_registry_export(
    prom_registry_t *registry,
    char *buf,
    size_t bufsize
) {
    if (!registry || !buf || bufsize == 0) {
        return -1;
    }
    
    size_t offset = 0;
    
    pthread_mutex_lock(&registry->lock);
    
    for (size_t i = 0; i < registry->num_metrics; i++) {
        prom_metric_t *m = registry->metrics[i];
        
        pthread_mutex_lock(&m->lock);
        
        /* Write HELP */
        int n = snprintf(buf + offset, bufsize - offset,
                        "# HELP %s %s\n", m->name, m->help);
        if (n < 0 || (size_t)n >= bufsize - offset) {
            pthread_mutex_unlock(&m->lock);
            goto overflow;
        }
        offset += (size_t)n;
        
        /* Write TYPE */
        const char *type_str = "untyped";
        if (m->type == PROM_METRIC_COUNTER) type_str = "counter";
        else if (m->type == PROM_METRIC_GAUGE) type_str = "gauge";
        else if (m->type == PROM_METRIC_HISTOGRAM) type_str = "histogram";
        
        n = snprintf(buf + offset, bufsize - offset,
                    "# TYPE %s %s\n", m->name, type_str);
        if (n < 0 || (size_t)n >= bufsize - offset) {
            pthread_mutex_unlock(&m->lock);
            goto overflow;
        }
        offset += (size_t)n;
        
        /* Write value(s) */
        if (m->type == PROM_METRIC_COUNTER || m->type == PROM_METRIC_GAUGE) {
            n = snprintf(buf + offset, bufsize - offset,
                        "%s %.6f\n", m->name, m->data.value);
            if (n < 0 || (size_t)n >= bufsize - offset) {
                pthread_mutex_unlock(&m->lock);
                goto overflow;
            }
            offset += (size_t)n;
        } else if (m->type == PROM_METRIC_HISTOGRAM) {
            /* Write buckets */
            for (size_t j = 0; j < m->data.histogram.num_buckets; j++) {
                n = snprintf(buf + offset, bufsize - offset,
                            "%s_bucket{le=\"%.6f\"} %lu\n",
                            m->name,
                            m->data.histogram.buckets[j].le,
                            m->data.histogram.buckets[j].count);
                if (n < 0 || (size_t)n >= bufsize - offset) {
                    pthread_mutex_unlock(&m->lock);
                    goto overflow;
                }
                offset += (size_t)n;
            }
            
            /* Write sum */
            n = snprintf(buf + offset, bufsize - offset,
                        "%s_sum %.6f\n", m->name, m->data.histogram.sum);
            if (n < 0 || (size_t)n >= bufsize - offset) {
                pthread_mutex_unlock(&m->lock);
                goto overflow;
            }
            offset += (size_t)n;
            
            /* Write count */
            n = snprintf(buf + offset, bufsize - offset,
                        "%s_count %lu\n", m->name, m->data.histogram.count);
            if (n < 0 || (size_t)n >= bufsize - offset) {
                pthread_mutex_unlock(&m->lock);
                goto overflow;
            }
            offset += (size_t)n;
        }
        
        pthread_mutex_unlock(&m->lock);
    }
    
    pthread_mutex_unlock(&registry->lock);
    return (ssize_t)offset;

overflow:
    pthread_mutex_unlock(&registry->lock);
    return -1;
}
