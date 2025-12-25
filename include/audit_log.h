/**
 * audit_log.h - Append-only audit log for message replay
 * 
 * Provides persistent audit trail with replay capability
 */

#ifndef AUDIT_LOG_H
#define AUDIT_LOG_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Audit log handle (opaque)
 */
typedef struct audit_log audit_log_t;

/**
 * Audit log entry
 */
typedef struct {
    uint64_t timestamp_ms;
    uint32_t msg_type;
    uint32_t payload_len;
    char *payload;
} audit_entry_t;

/**
 * Open audit log
 * 
 * @param path  Log file path
 * @return Audit log handle, or NULL on error
 */
audit_log_t* audit_log_open(const char *path);

/**
 * Close audit log
 */
void audit_log_close(audit_log_t *log);

/**
 * Write entry to audit log
 * 
 * @param log         Audit log
 * @param msg_type    Message type
 * @param payload     Payload data
 * @param payload_len Payload length
 * @return 0 on success, -1 on error
 */
int audit_log_write(
    audit_log_t *log,
    uint32_t msg_type,
    const void *payload,
    size_t payload_len
);

/**
 * Replay callback function
 * 
 * @param entry    Audit entry
 * @param userdata User context
 * @return 0 to continue, non-zero to stop
 */
typedef int (*audit_replay_fn_t)(const audit_entry_t *entry, void *userdata);

/**
 * Replay audit log
 * 
 * @param log      Audit log
 * @param callback Replay callback
 * @param userdata User context
 * @return Number of entries replayed, or -1 on error
 */
ssize_t audit_log_replay(
    audit_log_t *log,
    audit_replay_fn_t callback,
    void *userdata
);

/**
 * Rotate log file
 * 
 * @param log   Audit log
 * @param suffix  Suffix for rotated file (e.g. timestamp)
 * @return 0 on success, -1 on error
 */
int audit_log_rotate(audit_log_t *log, const char *suffix);

#ifdef __cplusplus
}
#endif

#endif /* AUDIT_LOG_H */
