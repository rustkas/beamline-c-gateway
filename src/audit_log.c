/**
 * audit_log.c - Audit log implementation
 */

#include "audit_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <arpa/inet.h>

struct audit_log {
    FILE *fp;
    char path[256];
    pthread_mutex_t lock;
    uint64_t entry_count;
};

static uint64_t get_timestamp_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

audit_log_t* audit_log_open(const char *path) {
    if (!path) {
        return NULL;
    }
    
    audit_log_t *log = calloc(1, sizeof(audit_log_t));
    if (!log) {
        return NULL;
    }
    
    /* Open in append mode */
    log->fp = fopen(path, "ab");
    if (!log->fp) {
        free(log);
        return NULL;
    }
    
    snprintf(log->path, sizeof(log->path), "%s", path);
    pthread_mutex_init(&log->lock, NULL);
    
    return log;
}

void audit_log_close(audit_log_t *log) {
    if (!log) {
        return;
    }
    
    if (log->fp) {
        fclose(log->fp);
    }
    
    pthread_mutex_destroy(&log->lock);
    free(log);
}

int audit_log_write(
    audit_log_t *log,
    uint32_t msg_type,
    const void *payload,
    size_t payload_len
) {
    if (!log || !payload || payload_len == 0) {
        return -1;
    }
    
    pthread_mutex_lock(&log->lock);
    
    /* Entry format: timestamp(8) | type(4) | len(4) | payload(len) */
    uint64_t ts = get_timestamp_ms();
    uint32_t type_net = htonl(msg_type);
    uint32_t len_net = htonl((uint32_t)payload_len);
    
    /* Write entry */
    fwrite(&ts, sizeof(ts), 1, log->fp);
    fwrite(&type_net, sizeof(type_net), 1, log->fp);
    fwrite(&len_net, sizeof(len_net), 1, log->fp);
    fwrite(payload, 1, payload_len, log->fp);
    
    /* Flush to ensure persistence */
    fflush(log->fp);
    
    log->entry_count++;
    
    pthread_mutex_unlock(&log->lock);
    return 0;
}

ssize_t audit_log_replay(
    audit_log_t *log,
    audit_replay_fn_t callback,
    void *userdata
) {
    if (!log || !callback) {
        return -1;
    }
    
    /* Open for reading */
    FILE *fp = fopen(log->path, "rb");
    if (!fp) {
        return -1;
    }
    
    ssize_t count = 0;
    
    while (1) {
        audit_entry_t entry;
        uint32_t type_net, len_net;
        
        /* Read header */
        if (fread(&entry.timestamp_ms, sizeof(entry.timestamp_ms), 1, fp) != 1) {
            break;
        }
        if (fread(&type_net, sizeof(type_net), 1, fp) != 1) {
            break;
        }
        if (fread(&len_net, sizeof(len_net), 1, fp) != 1) {
            break;
        }
        
        entry.msg_type = ntohl(type_net);
        entry.payload_len = ntohl(len_net);
        
        /* Read payload */
        entry.payload = malloc(entry.payload_len + 1);
        if (!entry.payload) {
            break;
        }
        
        if (fread(entry.payload, 1, entry.payload_len, fp) != entry.payload_len) {
            free(entry.payload);
            break;
        }
        entry.payload[entry.payload_len] = '\0';
        
        /* Callback */
        int rc = callback(&entry, userdata);
        free(entry.payload);
        
        count++;
        
        if (rc != 0) {
            break;  /* User requested stop */
        }
    }
    
    fclose(fp);
    return count;
}

int audit_log_rotate(audit_log_t *log, const char *suffix) {
    if (!log || !suffix) {
        return -1;
    }
    
    pthread_mutex_lock(&log->lock);
    
    /* Close current file */
    if (log->fp) {
        fclose(log->fp);
        log->fp = NULL;
    }
    
    /* Rename current file */
    char rotated_path[512];
    snprintf(rotated_path, sizeof(rotated_path), "%s.%s", log->path, suffix);
    rename(log->path, rotated_path);
    
    /* Reopen new file */
    log->fp = fopen(log->path, "ab");
    if (!log->fp) {
        pthread_mutex_unlock(&log->lock);
        return -1;
    }
    
    pthread_mutex_unlock(&log->lock);
    return 0;
}
