#ifndef ABUSE_DETECTION_H
#define ABUSE_DETECTION_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "rate_limiter.h"  /* For rl_endpoint_id_t */

/* Abuse event types */
typedef enum {
    ABUSE_EMPTY_PAYLOAD = 0,
    ABUSE_TARGETED_TENANT = 1,
    ABUSE_RATE_LIMIT_EVASION = 2,
    ABUSE_HEAVY_PAYLOAD = 3,
    ABUSE_MULTI_TENANT_FLOOD = 4
} abuse_event_type_t;

/* Abuse detection configuration */
typedef struct {
    int enabled;                    /* Enable abuse detection */
    int min_payload_size;          /* Minimum payload size (bytes) */
    int large_payload_threshold;   /* Large payload threshold (bytes) */
    int large_payload_ratio_threshold; /* Large payload ratio threshold (0-100) */
    int targeted_tenant_rate_threshold; /* Targeted tenant rate threshold (req/min) */
    int evasion_api_keys_threshold; /* Evasion detection: API keys threshold */
    int evasion_ips_threshold;     /* Evasion detection: IPs threshold */
    int multi_tenant_active_threshold; /* Multi-tenant flood: active tenants threshold */
    int retention_window_seconds;  /* Retention window for tracking (seconds) */
} abuse_detection_config_t;

/* Abuse tracking entry */
typedef struct {
    char tenant_id[64];
    char api_key[128];
    char client_ip[64];
    time_t first_seen;
    time_t last_seen;
    uint32_t request_count;
    uint32_t api_key_count;
    uint32_t ip_count;
    uint32_t large_payload_count;
    uint32_t total_payload_size;
} abuse_tracking_entry_t;

/* Initialize abuse detection */
int abuse_detection_init(const abuse_detection_config_t *config);

/* Cleanup abuse detection */
void abuse_detection_cleanup(void);

/* Track request for abuse detection */
int abuse_detection_track_request(const char *tenant_id, 
                                   const char *api_key,
                                   const char *client_ip,
                                   int payload_size,
                                   rl_endpoint_id_t endpoint);

/* Check for abuse patterns */
abuse_event_type_t abuse_detection_check_patterns(const char *tenant_id,
                                                    const char *api_key,
                                                    const char *client_ip,
                                                    int payload_size,
                                                    rl_endpoint_id_t endpoint);

/* Log abuse event */
void abuse_detection_log_event(abuse_event_type_t event_type,
                                const char *tenant_id,
                                const char *api_key,
                                const char *client_ip,
                                const char *request_id,
                                const char *trace_id,
                                const char *endpoint,
                                const void *context);

/* Get default configuration */
void abuse_detection_get_default_config(abuse_detection_config_t *config);

/* Parse configuration from environment variables */
int abuse_detection_parse_config(abuse_detection_config_t *config);

/* Response actions */
typedef enum {
    ABUSE_RESPONSE_LOG_ONLY = 0,      /* Only log abuse events */
    ABUSE_RESPONSE_RATE_LIMIT = 1,    /* Apply stricter rate limiting */
    ABUSE_RESPONSE_TEMPORARY_BLOCK = 2 /* Temporarily block tenant */
} abuse_response_action_t;

/* Get response action for abuse event type */
abuse_response_action_t abuse_detection_get_response_action(abuse_event_type_t event_type);

/* Check if tenant is temporarily blocked */
int abuse_detection_is_tenant_blocked(const char *tenant_id);

/* Block tenant temporarily */
int abuse_detection_block_tenant(const char *tenant_id, int duration_seconds);

/* Unblock tenant */
int abuse_detection_unblock_tenant(const char *tenant_id);

#endif /* ABUSE_DETECTION_H */

