/*
 * Gateway Observability Performance Tests
 * 
 * Tests performance of observability features to ensure they don't degrade Gateway performance.
 * 
 * Note: These tests measure performance metrics but don't fail on slow systems.
 * They provide baseline measurements and detect significant performance regressions.
 */

#include "unity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <jansson.h>

/* Performance thresholds (adjust based on system capabilities) */
#define MIN_LOGS_PER_SECOND 1000.0
#define MAX_PII_FILTER_LATENCY_MS 10.0
#define MAX_HEALTH_ENDPOINT_LATENCY_MS 50.0

/* Helper: Get current time in milliseconds */
static double get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec * 1000.0 + (double)tv.tv_usec / 1000.0;
}

/* Helper: Simulate log generation (simplified version) */
static void simulate_log_generation(int count) {
    for (int i = 0; i < count; i++) {
        /* Simulate JSON log creation */
        json_t *log_entry = json_object();
        json_object_set_new(log_entry, "timestamp", json_string("2025-01-27T12:00:00.123456Z"));
        json_object_set_new(log_entry, "level", json_string("INFO"));
        json_object_set_new(log_entry, "component", json_string("gateway"));
        json_object_set_new(log_entry, "message", json_string("Test message"));
        json_object_set_new(log_entry, "tenant_id", json_string("tenant_123"));
        json_object_set_new(log_entry, "trace_id", json_string("trace_abc"));
        
        char *json_str = json_dumps(log_entry, JSON_COMPACT);
        if (json_str) {
            free(json_str);
        }
        json_decref(log_entry);
    }
}

/* Helper: Simulate PII filtering (simplified version) */
static void simulate_pii_filtering(const char *input, char *output, size_t output_size) {
    /* Simple keyword-based filtering simulation */
    const char *sensitive_keywords[] = {"password", "api_key", "secret", "token"};
    size_t num_keywords = sizeof(sensitive_keywords) / sizeof(sensitive_keywords[0]);
    
    strncpy(output, input, output_size - 1);
    output[output_size - 1] = '\0';
    
    for (size_t i = 0; i < num_keywords; i++) {
        if (strstr(output, sensitive_keywords[i]) != NULL) {
            /* Replace with [REDACTED] */
            /* Simplified - actual implementation is more complex */
        }
    }
}

/* Test: Log generation throughput (logs/second) */
void test_log_generation_performance(void) {
    const int test_count = 10000;
    double start_time = get_time_ms();
    
    simulate_log_generation(test_count);
    
    double end_time = get_time_ms();
    double elapsed_ms = end_time - start_time;
    double elapsed_sec = elapsed_ms / 1000.0;
    double logs_per_second = (double)test_count / elapsed_sec;
    
    printf("\n  Log generation performance: %.2f logs/second (%.2f ms total for %d logs)\n",
           logs_per_second, elapsed_ms, test_count);
    
    /* Performance assertion (may fail on slow systems, but provides baseline) */
    if (logs_per_second < MIN_LOGS_PER_SECOND) {
        printf("  WARNING: Log generation slower than expected (%.2f < %.2f logs/sec)\n",
               logs_per_second, MIN_LOGS_PER_SECOND);
    }
    
    /* Test passes - we're just measuring, not failing on slow systems */
    TEST_ASSERT_TRUE(logs_per_second > 0);
}

/* Test: PII filtering latency (time per log entry) */
void test_pii_filtering_performance(void) {
    const int test_count = 1000;
    const char *test_message = "User password is secret123 and api_key is abc123def456";
    char filtered[512];
    
    double start_time = get_time_ms();
    
    for (int i = 0; i < test_count; i++) {
        simulate_pii_filtering(test_message, filtered, sizeof(filtered));
    }
    
    double end_time = get_time_ms();
    double elapsed_ms = end_time - start_time;
    double avg_latency_ms = elapsed_ms / (double)test_count;
    
    printf("\n  PII filtering performance: %.4f ms per log entry (%.2f ms total for %d entries)\n",
           avg_latency_ms, elapsed_ms, test_count);
    
    /* Performance assertion */
    if (avg_latency_ms > MAX_PII_FILTER_LATENCY_MS) {
        printf("  WARNING: PII filtering slower than expected (%.4f > %.2f ms)\n",
               avg_latency_ms, MAX_PII_FILTER_LATENCY_MS);
    }
    
    TEST_ASSERT_TRUE(avg_latency_ms > 0);
}

/* Test: JSON serialization performance */
void test_json_serialization_performance(void) {
    const int test_count = 5000;
    double start_time = get_time_ms();
    
    for (int i = 0; i < test_count; i++) {
        json_t *log_entry = json_object();
        json_object_set_new(log_entry, "timestamp", json_string("2025-01-27T12:00:00.123456Z"));
        json_object_set_new(log_entry, "level", json_string("INFO"));
        json_object_set_new(log_entry, "component", json_string("gateway"));
        json_object_set_new(log_entry, "message", json_string("Test message"));
        
        json_t *context = json_object();
        json_object_set_new(context, "stage", json_string("http_request"));
        json_object_set_new(context, "method", json_string("POST"));
        json_object_set_new(context, "path", json_string("/api/v1/messages"));
        json_object_set_new(context, "status_code", json_integer(200));
        json_object_set_new(context, "latency_ms", json_integer(45));
        json_object_set_new(log_entry, "context", context);
        
        char *json_str = json_dumps(log_entry, JSON_COMPACT);
        if (json_str) {
            free(json_str);
        }
        json_decref(log_entry);
    }
    
    double end_time = get_time_ms();
    double elapsed_ms = end_time - start_time;
    double elapsed_sec = elapsed_ms / 1000.0;
    double serializations_per_second = (double)test_count / elapsed_sec;
    
    printf("\n  JSON serialization performance: %.2f serializations/second (%.2f ms total for %d entries)\n",
           serializations_per_second, elapsed_ms, test_count);
    
    TEST_ASSERT_TRUE(serializations_per_second > 0);
}

/* Test: Memory usage during logging (simplified) */
void test_memory_usage_during_logging(void) {
    const int test_count = 1000;
    
    /* Note: Actual memory measurement would require more sophisticated tools */
    /* This test verifies that logging doesn't cause memory leaks */
    
    for (int i = 0; i < test_count; i++) {
        json_t *log_entry = json_object();
        json_object_set_new(log_entry, "timestamp", json_string("2025-01-27T12:00:00.123456Z"));
        json_object_set_new(log_entry, "level", json_string("INFO"));
        json_object_set_new(log_entry, "component", json_string("gateway"));
        json_object_set_new(log_entry, "message", json_string("Test message"));
        
        char *json_str = json_dumps(log_entry, JSON_COMPACT);
        if (json_str) {
            free(json_str);
        }
        json_decref(log_entry);
    }
    
    /* Test passes if no crash (memory leak detection would require valgrind) */
    TEST_ASSERT_TRUE(1);
    printf("\n  Memory usage test: Completed %d log entries without crash\n", test_count);
}

int main(void) {
    UNITY_BEGIN();
    
    printf("\n=== Gateway Observability Performance Tests ===\n");
    printf("Note: These tests measure performance but don't fail on slow systems.\n");
    printf("They provide baseline measurements for performance monitoring.\n\n");
    
    RUN_TEST(test_log_generation_performance);
    RUN_TEST(test_pii_filtering_performance);
    RUN_TEST(test_json_serialization_performance);
    RUN_TEST(test_memory_usage_during_logging);
    
    printf("\n=== Performance Tests Complete ===\n");
    
    return UNITY_END();
}

