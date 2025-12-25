/**
 * test_json_validator.c - Test JSON validation
 */

#include "json_validator.h"
#include <stdio.h>
#include <assert.h>

static void test_wellformed(void) {
    printf("Test: well-formed JSON... ");
    
    assert(json_validate_wellformed("{\"key\":\"value\"}") == JSON_VALID);
    assert(json_validate_wellformed("[1,2,3]") == JSON_VALID);
    assert(json_validate_wellformed("{\"nested\":{\"key\":\"val\"}}") == JSON_VALID);
    
    /* Invalid */
    assert(json_validate_wellformed(NULL) == JSON_ERR_NULL_INPUT);
    assert(json_validate_wellformed("{\"key\":") == JSON_ERR_PARSE_ERROR);
    assert(json_validate_wellformed("not json") == JSON_ERR_PARSE_ERROR);
    assert(json_validate_wellformed("{unclosed") == JSON_ERR_PARSE_ERROR);
    
    printf("OK\n");
}

static void test_task_submit(void) {
    printf("Test: task submit validation... ");
    
    /* Valid */
    const char *valid = "{\"task_type\":\"code_completion\",\"file\":\"/path/to/file.py\"}";
    assert(json_validate_task_submit(valid) == JSON_VALID);
    
    /* With optional fields */
    const char *valid_full = "{\"task_type\":\"analysis\",\"file\":\"/test.js\",\"line\":42,\"context\":\"...\"}";
    assert(json_validate_task_submit(valid_full) == JSON_VALID);
    
    /* Missing task_type */
    const char *missing_type = "{\"file\":\"/path\"}";
    assert(json_validate_task_submit(missing_type) == JSON_ERR_MISSING_REQUIRED);
    
    /* Missing file */
    const char *missing_file = "{\"task_type\":\"completion\"}";
    assert(json_validate_task_submit(missing_file) == JSON_ERR_MISSING_REQUIRED);
    
    /* Not an object */
    const char *array = "[\"task_type\",\"file\"]";
    assert(json_validate_task_submit(array) == JSON_ERR_NOT_OBJECT);
    
    printf("OK\n");
}

static void test_error_messages(void) {
    printf("Test: error messages... ");
    
    assert(json_validation_strerror(JSON_VALID) != NULL);
    assert(json_validation_strerror(JSON_ERR_PARSE_ERROR) != NULL);
    assert(json_validation_strerror(JSON_ERR_MISSING_REQUIRED) != NULL);
    
    printf("OK\n");
}

int main(void) {
    printf("=== JSON Validator Tests ===\n");
    
    test_wellformed();
    test_task_submit();
    test_error_messages();
    
    printf("\nAll tests passed!\n");
    return 0;
}
