/* Abuse Detection: Gateway Implementation
 * 
 * Tracks requests per tenant, API key, and IP for abuse pattern detection.
 * Implements detection logic for:
 * - Targeted tenant attacks
 * - Rate limit evasion (multiple API keys/IPs)
 * - Multi-tenant floods
 */

#include "abuse_detection.h"
#include "rate_limiter.h"
#include "metrics/metrics_registry.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>

/* Simple hash table for tracking (in-memory) */
#define MAX_TRACKING_ENTRIES 10000
#define CLEANUP_INTERVAL_SECONDS 300  /* 5 minutes */

static abuse_detection_config_t g_config = {0};
static abuse_tracking_entry_t g_tracking_table[MAX_TRACKING_ENTRIES];
static int g_tracking_count = 0;
static time_t g_last_cleanup = 0;
static int g_initialized = 0;

/* Multi-tenant flood tracking */
static uint32_t g_active_tenants_count = 0;
static time_t g_multi_tenant_window_start = 0;
static uint32_t g_multi_tenant_total_requests = 0;

/* Simple hash function for tenant_id */
static uint32_t hash_tenant(const char *tenant_id) {
    uint32_t hash = 5381;
    int c;
    while ((c = *tenant_id++)) {
        hash = ((hash << 5) + hash) + (uint32_t)c;
    }
    return hash % MAX_TRACKING_ENTRIES;
}

/* Find or create tracking entry */
static abuse_tracking_entry_t *find_or_create_entry(const char *tenant_id) {
    if (!tenant_id || strlen(tenant_id) == 0) {
        return NULL;
    }
    
    uint32_t hash = hash_tenant(tenant_id);
    int start_idx = (int)hash;
    int idx = start_idx;
    
    /* Linear probing */
    do {
        if (g_tracking_table[idx].tenant_id[0] == '\0') {
            /* Empty slot - create new entry */
            memset(&g_tracking_table[idx], 0, sizeof(abuse_tracking_entry_t));
            strncpy(g_tracking_table[idx].tenant_id, tenant_id, sizeof(g_tracking_table[idx].tenant_id) - 1);
            g_tracking_table[idx].first_seen = time(NULL);
            g_tracking_table[idx].last_seen = time(NULL);
            if (g_tracking_count < MAX_TRACKING_ENTRIES) {
                g_tracking_count++;
            }
            return &g_tracking_table[idx];
        } else if (strcmp(g_tracking_table[idx].tenant_id, tenant_id) == 0) {
            /* Found existing entry */
            g_tracking_table[idx].last_seen = time(NULL);
            return &g_tracking_table[idx];
        }
        
        idx = (idx + 1) % MAX_TRACKING_ENTRIES;
    } while (idx != start_idx);
    
    /* Table full - use first slot */
    return &g_tracking_table[start_idx];
}

/* Cleanup old entries */
static void cleanup_old_entries(void) {
    time_t now = time(NULL);
    if (now - g_last_cleanup < CLEANUP_INTERVAL_SECONDS) {
        return;
    }
    
    int removed = 0;
    for (int i = 0; i < MAX_TRACKING_ENTRIES; i++) {
        if (g_tracking_table[i].tenant_id[0] != '\0') {
            time_t age = now - g_tracking_table[i].last_seen;
            if (age > g_config.retention_window_seconds) {
                /* Remove old entry */
                memset(&g_tracking_table[i], 0, sizeof(abuse_tracking_entry_t));
                removed++;
            }
        }
    }
    
    g_tracking_count -= removed;
    g_last_cleanup = now;
}

/* Get default configuration */
void abuse_detection_get_default_config(abuse_detection_config_t *config) {
    if (!config) return;
    
    memset(config, 0, sizeof(abuse_detection_config_t));
    config->enabled = 0;  /* Disabled by default */
    config->min_payload_size = 10;  /* 10 bytes minimum */
    config->large_payload_threshold = 524288;  /* 500KB */
    config->large_payload_ratio_threshold = 80;  /* 80% */
    config->targeted_tenant_rate_threshold = 500;  /* 500 req/min */
    config->evasion_api_keys_threshold = 10;  /* 10 API keys */
    config->evasion_ips_threshold = 10;  /* 10 IPs */
    config->multi_tenant_active_threshold = 20;  /* 20 active tenants */
    config->retention_window_seconds = 300;  /* 5 minutes */
}

/* Parse configuration from environment variables */
int abuse_detection_parse_config(abuse_detection_config_t *config) {
    if (!config) return -1;
    
    /* Start with defaults */
    abuse_detection_get_default_config(config);
    
    /* Check if abuse detection is enabled */
    const char *enabled_str = getenv("GATEWAY_ABUSE_DETECTION_ENABLED");
    if (enabled_str && (strcmp(enabled_str, "true") == 0 || strcmp(enabled_str, "1") == 0)) {
        config->enabled = 1;
    }
    
    /* Get thresholds from environment */
    const char *min_payload_str = getenv("GATEWAY_ABUSE_MIN_PAYLOAD_SIZE");
    if (min_payload_str) {
        int val = atoi(min_payload_str);
        if (val > 0) config->min_payload_size = val;
    }
    
    const char *large_payload_str = getenv("GATEWAY_ABUSE_LARGE_PAYLOAD_THRESHOLD");
    if (large_payload_str) {
        int val = atoi(large_payload_str);
        if (val > 0) config->large_payload_threshold = val;
    }
    
    const char *targeted_tenant_str = getenv("GATEWAY_ABUSE_TARGETED_TENANT_THRESHOLD");
    if (targeted_tenant_str) {
        int val = atoi(targeted_tenant_str);
        if (val > 0) config->targeted_tenant_rate_threshold = val;
    }
    
    const char *evasion_keys_str = getenv("GATEWAY_ABUSE_EVASION_API_KEYS_THRESHOLD");
    if (evasion_keys_str) {
        int val = atoi(evasion_keys_str);
        if (val > 0) config->evasion_api_keys_threshold = val;
    }
    
    const char *retention_str = getenv("GATEWAY_ABUSE_RETENTION_WINDOW_SECONDS");
    if (retention_str) {
        int val = atoi(retention_str);
        if (val > 0) config->retention_window_seconds = val;
    }
    
    return 0;
}

/* Initialize abuse detection */
int abuse_detection_init(const abuse_detection_config_t *config) {
    if (g_initialized) {
        return 0;
    }
    
    if (config) {
        memcpy(&g_config, config, sizeof(abuse_detection_config_t));
    } else {
        abuse_detection_get_default_config(&g_config);
    }
    
    /* Clear tracking table */
    memset(g_tracking_table, 0, sizeof(g_tracking_table));
    g_tracking_count = 0;
    g_last_cleanup = time(NULL);
    
    g_initialized = 1;
    return 0;
}

/* Cleanup abuse detection */
void abuse_detection_cleanup(void) {
    if (!g_initialized) {
        return;
    }
    
    memset(g_tracking_table, 0, sizeof(g_tracking_table));
    g_tracking_count = 0;
    g_initialized = 0;
}

/* Track request for abuse detection */
int abuse_detection_track_request(const char *tenant_id, 
                                   const char *api_key,
                                   const char *client_ip,
                                   int payload_size,
                                   rl_endpoint_id_t endpoint) {
    (void)endpoint; /* Not used yet */
    
    if (!g_initialized || !g_config.enabled) {
        return 0;
    }
    
    if (!tenant_id || strlen(tenant_id) == 0) {
        return 0;
    }
    
    /* Cleanup old entries periodically */
    cleanup_old_entries();
    
    /* Find or create tracking entry */
    abuse_tracking_entry_t *entry = find_or_create_entry(tenant_id);
    if (!entry) {
        return -1;
    }
    
    /* Update tracking data */
    entry->request_count++;
    entry->last_seen = time(NULL);
    entry->total_payload_size += (uint32_t)payload_size;
    
    /* Track unique API keys */
    if (api_key && strlen(api_key) > 0) {
        /* Simple check: if API key different from last, increment count */
        if (strcmp(entry->api_key, api_key) != 0) {
            if (entry->api_key[0] == '\0') {
                /* First API key */
                strncpy(entry->api_key, api_key, sizeof(entry->api_key) - 1);
            } else {
                /* Different API key - increment count */
                entry->api_key_count++;
            }
        }
    }
    
    /* Track unique IPs */
    if (client_ip && strlen(client_ip) > 0) {
        /* Simple check: if IP different from last, increment count */
        if (strcmp(entry->client_ip, client_ip) != 0) {
            if (entry->client_ip[0] == '\0') {
                /* First IP */
                strncpy(entry->client_ip, client_ip, sizeof(entry->client_ip) - 1);
            } else {
                /* Different IP - increment count */
                entry->ip_count++;
            }
        }
    }
    
    /* Track large payloads */
    if (payload_size > g_config.large_payload_threshold) {
        entry->large_payload_count++;
    }
    
    return 0;
}

/* Check for abuse patterns */
abuse_event_type_t abuse_detection_check_patterns(const char *tenant_id,
                                                    const char *api_key,
                                                    const char *client_ip,
                                                    int payload_size,
                                                    rl_endpoint_id_t endpoint) {
    (void)api_key; /* Not used in pattern detection yet */
    (void)client_ip; /* Not used in pattern detection yet */
    (void)endpoint; /* Not used in pattern detection yet */
    
    if (!g_initialized || !g_config.enabled) {
        return ABUSE_EMPTY_PAYLOAD; /* No abuse detected */
    }
    
    if (!tenant_id || strlen(tenant_id) == 0) {
        return ABUSE_EMPTY_PAYLOAD;
    }
    
    /* Find tracking entry */
    abuse_tracking_entry_t *entry = find_or_create_entry(tenant_id);
    if (!entry || entry->request_count == 0) {
        return ABUSE_EMPTY_PAYLOAD;
    }
    
    time_t now = time(NULL);
    time_t window_start = entry->first_seen;
    time_t window_duration = now - window_start;
    
    if (window_duration <= 0) {
        window_duration = 1; /* Avoid division by zero */
    }
    
    /* Calculate request rate (requests per minute) */
    uint32_t request_rate = (entry->request_count * 60U) / (uint32_t)window_duration;
    
    /* Check for targeted tenant attack */
    if (request_rate > (uint32_t)g_config.targeted_tenant_rate_threshold) {
        return ABUSE_TARGETED_TENANT;
    }
    
    /* Check for rate limit evasion */
    if (entry->api_key_count > (uint32_t)g_config.evasion_api_keys_threshold ||
        entry->ip_count > (uint32_t)g_config.evasion_ips_threshold) {
        return ABUSE_RATE_LIMIT_EVASION;
    }
    
    /* Check for heavy payload pattern */
    if (entry->request_count > 10) {
        uint32_t large_payload_ratio = (entry->large_payload_count * 100) / entry->request_count;
        if (large_payload_ratio > (uint32_t)g_config.large_payload_ratio_threshold) {
            return ABUSE_HEAVY_PAYLOAD;
        }
    }
    
    /* Check for empty payload */
    if (payload_size < g_config.min_payload_size) {
        return ABUSE_EMPTY_PAYLOAD;
    }
    
    /* Check for multi-tenant flood */
    time_t now_time = time(NULL);
    if (g_multi_tenant_window_start == 0) {
        g_multi_tenant_window_start = now_time;
    }
    
    time_t window_duration_sec = now_time - g_multi_tenant_window_start;
    if (window_duration_sec > 60) { /* Reset window every minute */
        g_multi_tenant_window_start = now_time;
        g_multi_tenant_total_requests = 0;
        g_active_tenants_count = (uint32_t)g_tracking_count; /* Approximate active tenants */
    } else {
        g_multi_tenant_total_requests++;
    }
    
    /* Update active tenants count */
    if (entry->request_count == 1) {
        /* New tenant in this window */
        g_active_tenants_count++;
    }
    
    /* Check if we have too many active tenants with high request rate */
    if (window_duration > 0 && g_active_tenants_count > (uint32_t)g_config.multi_tenant_active_threshold) {
        uint32_t avg_requests_per_tenant = g_multi_tenant_total_requests / (g_active_tenants_count > 0 ? g_active_tenants_count : 1);
        if (avg_requests_per_tenant > 10) { /* High average requests per tenant */
            return ABUSE_MULTI_TENANT_FLOOD;
        }
    }
    
    return ABUSE_EMPTY_PAYLOAD; /* No abuse detected */
}

/* Block tenant for specified duration (stub implementation) */
int abuse_detection_block_tenant(const char *tenant_id, int duration_seconds) {
    (void)tenant_id;
    (void)duration_seconds;
    /* TODO: Implement tenant blocking logic */
    return 0; /* Success */
}

/* Check if tenant is blocked (stub implementation) */
int abuse_detection_is_tenant_blocked(const char *tenant_id) {
    (void)tenant_id;
    /* TODO: Implement tenant blocking check */
    return 0; /* Not blocked */
}

/* Get response action for abuse event (stub implementation) */
abuse_response_action_t abuse_detection_get_response_action(abuse_event_type_t event_type) {
    (void)event_type;
    /* TODO: Implement response action logic */
    return ABUSE_RESPONSE_LOG_ONLY; /* Default: log only */
}

/* Log abuse event */
void abuse_detection_log_event(abuse_event_type_t event_type,
                                const char *tenant_id,
                                const char *api_key,
                                const char *client_ip,
                                const char *request_id,
                                const char *trace_id,
                                const char *endpoint,
                                const void *context) {
    (void)api_key; /* Not logged for security */
    (void)context; /* Not used yet */
    
    const char *event_type_str = NULL;
    const char *message = NULL;
    
    switch (event_type) {
        case ABUSE_EMPTY_PAYLOAD:
            event_type_str = "abuse.empty_payload";
            message = "Abuse detected: empty payload request";
            break;
        case ABUSE_TARGETED_TENANT:
            event_type_str = "abuse.targeted_tenant";
            message = "Abuse detected: targeted tenant attack";
            break;
        case ABUSE_RATE_LIMIT_EVASION:
            event_type_str = "abuse.rate_limit_evasion";
            message = "Abuse detected: rate limit evasion attempt";
            break;
        case ABUSE_HEAVY_PAYLOAD:
            event_type_str = "abuse.heavy_payload";
            message = "Abuse detected: heavy payload pattern";
            break;
        case ABUSE_MULTI_TENANT_FLOOD:
            event_type_str = "abuse.multi_tenant_flood";
            message = "Abuse detected: multi-tenant flood";
            break;
        default:
            return; /* Unknown event type */
    }
    
    /* Get timestamp */
    char timestamp[64];
    struct timeval tv;
    struct tm *tm_info;
    time_t now;
    
    gettimeofday(&tv, NULL);
    now = tv.tv_sec;
    tm_info = gmtime(&now);
    
    if (tm_info != NULL) {
        snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02dT%02d:%02d:%02d.%06ldZ",
                 tm_info->tm_year + 1900,
                 tm_info->tm_mon + 1,
                 tm_info->tm_mday,
                 tm_info->tm_hour,
                 tm_info->tm_min,
                 tm_info->tm_sec,
                 (long)tv.tv_usec);
    } else {
        snprintf(timestamp, sizeof(timestamp), "1970-01-01T00:00:00.000000Z");
    }
    
    /* Log abuse event (structured JSON) */
    fprintf(stderr, "{\"timestamp\":\"%s\",\"level\":\"WARN\",\"component\":\"gateway\","
            "\"message\":\"%s\",\"event_type\":\"%s\"",
            timestamp, message, event_type_str);
    
    if (tenant_id && strlen(tenant_id) > 0) {
        fprintf(stderr, ",\"tenant_id\":\"%s\"", tenant_id);
    }
    
    if (request_id && strlen(request_id) > 0) {
        fprintf(stderr, ",\"request_id\":\"%s\"", request_id);
    }
    
    if (trace_id && strlen(trace_id) > 0) {
        fprintf(stderr, ",\"trace_id\":\"%s\"", trace_id);
    }
    
    if (client_ip && strlen(client_ip) > 0) {
        fprintf(stderr, ",\"context\":{\"client_ip\":\"%s\"", client_ip);
    } else {
        fprintf(stderr, ",\"context\":{");
    }
    
    if (endpoint && strlen(endpoint) > 0) {
        fprintf(stderr, ",\"endpoint\":\"%s\"", endpoint);
    }
    
    fprintf(stderr, "}}\n");
    
    /* Record specific abuse metric */
    switch (event_type) {
        case ABUSE_EMPTY_PAYLOAD:
            metrics_record_abuse_empty_payload();
            break;
        case ABUSE_TARGETED_TENANT:
            metrics_record_abuse_targeted_tenant();
            break;
        case ABUSE_RATE_LIMIT_EVASION:
            metrics_record_abuse_rate_limit_evasion();
            break;
        case ABUSE_HEAVY_PAYLOAD:
            metrics_record_abuse_heavy_payload();
            break;
        case ABUSE_MULTI_TENANT_FLOOD:
            metrics_record_abuse_multi_tenant_flood();
            break;
        default:
            break;
    }
}

