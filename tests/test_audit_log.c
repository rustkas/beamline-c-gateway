/**
 * test_audit_log.c - Audit log tests
 */

#include "audit_log.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#define TEST_LOG_PATH "/tmp/test_audit.log"

static int replay_counter(const audit_entry_t *entry, void *userdata) {
    int *count = (int*)userdata;
    (*count)++;
    
    printf("  Entry %d: type=%u, len=%u, ts=%lu\n",
           *count, entry->msg_type, entry->payload_len, entry->timestamp_ms);
    
    return 0;  /* Continue */
}

static void test_write_read(void) {
    printf("Test: write and read entries... ");
    
    /* Remove old test file */
    unlink(TEST_LOG_PATH);
    
    /* Write entries */
    audit_log_t *log = audit_log_open(TEST_LOG_PATH);
    assert(log != NULL);
    
    audit_log_write(log, 1, "payload1", 8);
    audit_log_write(log, 2, "payload2", 8);
    audit_log_write(log, 3, "payload3", 8);
    
    audit_log_close(log);
    
    /* Replay */
    log = audit_log_open(TEST_LOG_PATH);
    assert(log != NULL);
    
    int count = 0;
    ssize_t replayed = audit_log_replay(log, replay_counter, &count);
    
    assert(replayed == 3);
    assert(count == 3);
    
    audit_log_close(log);
    unlink(TEST_LOG_PATH);
    
    printf("OK\n");
}

static void test_large_payload(void) {
    printf("Test: large payload... ");
    
    unlink(TEST_LOG_PATH);
    
    audit_log_t *log = audit_log_open(TEST_LOG_PATH);
    assert(log != NULL);
    
    /* Write 1KB payload */
    char large[1024];
    memset(large, 'A', sizeof(large));
    
    int rc = audit_log_write(log, 100, large, sizeof(large));
    assert(rc == 0);
    
    audit_log_close(log);
    unlink(TEST_LOG_PATH);
    
    printf("OK\n");
}

static void test_rotation(void) {
    printf("Test: log rotation... ");
    
    unlink(TEST_LOG_PATH);
    
    audit_log_t *log = audit_log_open(TEST_LOG_PATH);
    assert(log != NULL);
    
    audit_log_write(log, 1, "before", 6);
    
    /* Rotate */
    int rc = audit_log_rotate(log, "20251225");
    assert(rc == 0);
    
    /* Write after rotation */
    audit_log_write(log, 2, "after", 5);
    
    audit_log_close(log);
    
    /* Verify rotated file exists */
    FILE *fp = fopen(TEST_LOG_PATH ".20251225", "rb");
    assert(fp != NULL);
    fclose(fp);
    
    unlink(TEST_LOG_PATH);
    unlink(TEST_LOG_PATH ".20251225");
    
    printf("OK\n");
}

int main(void) {
    printf("=== Audit Log Tests ===\n");
    
    test_write_read();
    test_large_payload();
    test_rotation();
    
    printf("\nAll tests passed!\n");
    return 0;
}
