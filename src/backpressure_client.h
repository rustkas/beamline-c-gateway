#ifndef BACKPRESSURE_CLIENT_H
#define BACKPRESSURE_CLIENT_H

#include <stdint.h>
#include <stdbool.h>

/* Backpressure status */
typedef enum {
    BACKPRESSURE_INACTIVE = 0,
    BACKPRESSURE_WARNING = 1,
    BACKPRESSURE_ACTIVE = 2
} backpressure_status_t;

/* Backpressure client configuration */
typedef struct {
    const char *router_metrics_url;  /* Router Prometheus metrics URL (e.g., "http://localhost:8080/_metrics") */
    int check_interval_seconds;      /* How often to check backpressure status (default: 5) */
    int timeout_ms;                  /* HTTP timeout for metrics request (default: 1000) */
} backpressure_client_config_t;

/* Initialize backpressure client */
int backpressure_client_init(const backpressure_client_config_t *config);

/* Cleanup backpressure client */
void backpressure_client_cleanup(void);

/* Check Router backpressure status */
backpressure_status_t backpressure_client_check_router_status(void);

/* Get cached backpressure status (without making HTTP request) */
backpressure_status_t backpressure_client_get_cached_status(void);

/* Get default configuration */
void backpressure_client_get_default_config(backpressure_client_config_t *config);

/* Parse configuration from environment variables */
int backpressure_client_parse_config(backpressure_client_config_t *config);

#endif /* BACKPRESSURE_CLIENT_H */

