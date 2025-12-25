/**
 * test_log_sanitizer.c - Test log sanitization
 */

#include "jsonl_logger.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static void test_sensitive_key_detection(void) {
    printf("Test: sensitive key detection... ");
    
    assert(jsonl_is_sensitive_key("api_key") == 1);
    assert(jsonl_is_sensitive_key("authorization") == 1);
    assert(jsonl_is_sensitive_key("password") == 1);
    assert(jsonl_is_sensitive_key("bearer_token") == 1);
    assert(jsonl_is_sensitive_key("normal_field") == 0);
    assert(jsonl_is_sensitive_key("user_id") == 0);
    
    printf("OK\n");
}

static void test_sanitize_simple(void) {
    printf("Test: sanitize simple JSON... ");
    
    const char *input = "{\"user\":\"john\",\"api_key\":\"secret123\",\"count\":42}";
    char output[256];
    
    int rc = jsonl_sanitize_json(input, output, sizeof(output));
    assert(rc == 0);
    
    /* Should mask api_key */
    assert(strstr(output, "secret123") == NULL);
    assert(strstr(output, "***") != NULL);
    assert(strstr(output, "john") != NULL);  /* normal fields preserved */
    assert(strstr(output, "42") != NULL);
    
    printf("OK\n");
}

static void test_sanitize_multiple_secrets(void) {
    printf("Test: multiple secrets... ");
    
    const char *input = "{\"token\":\"abc\",\"password\":\"xyz\",\"name\":\"test\"}";
    char output[256];
    
    jsonl_sanitize_json(input, output, sizeof(output));
    
    assert(strstr(output, "abc") == NULL);
    assert(strstr(output, "xyz") == NULL);
    assert(strstr(output, "test") != NULL);
    
    printf("OK\n");
}

static void test_sanitize_nested(void) {
    printf("Test: nested JSON... ");
    
    const char *input = "{\"data\":{\"authorization\":\"bearer xyz\"},\"id\":1}";
    char output[256];
    
    jsonl_sanitize_json(input, output, sizeof(output));
    
    /* authorization should be masked */
    assert(strstr(output, "bearer xyz") == NULL);
    
    printf("OK\n");
}

int main(void) {
    printf("=== Log Sanitizer Tests ===\n");
    
    test_sensitive_key_detection();
    test_sanitize_simple();
    test_sanitize_multiple_secrets();
    test_sanitize_nested();
    
    printf("\nAll tests passed!\n");
    return 0;
}
