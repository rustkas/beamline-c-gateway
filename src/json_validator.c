/**
 * json_validator.c - Simple JSON validation
 */

#include "json_validator.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* Simple JSON well-formedness check */
json_validation_error_t json_validate_wellformed(const char *json_str) {
    if (!json_str) {
        return JSON_ERR_NULL_INPUT;
    }
    
    /* Skip whitespace */
    while (*json_str && isspace(*json_str)) json_str++;
    
    /* Must start with { or [ */
    if (*json_str != '{' && *json_str != '[') {
        return JSON_ERR_PARSE_ERROR;
    }
    
    /* Count braces */
    int brace_count = 0;
    int bracket_count = 0;
    int in_string = 0;
    
    for (const char *p = json_str; *p; p++) {
        if (*p == '"' && (p == json_str || *(p-1) != '\\')) {
            in_string = !in_string;
        }
        
        if (!in_string) {
            if (*p == '{') brace_count++;
            if (*p == '}') brace_count--;
            if (*p == '[') bracket_count++;
            if (*p == ']') bracket_count--;
        }
    }
    
    if (brace_count != 0 || bracket_count != 0) {
        return JSON_ERR_PARSE_ERROR;
    }
    
    return JSON_VALID;
}

/* Check if JSON contains required field */
static int json_has_field(const char *json_str, const char *field) {
    if (!json_str || !field) return 0;
    
    char search[256];
    snprintf(search, sizeof(search), "\"%s\"", field);
    
    return strstr(json_str, search) != NULL;
}

json_validation_error_t json_validate_task_submit(const char *json_str) {
    /* First check well-formed */
    json_validation_error_t err = json_validate_wellformed(json_str);
    if (err != JSON_VALID) {
        return err;
    }
    
    /* Must be object */
    const char *p = json_str;
    while (*p && isspace(*p)) p++;
    if (*p != '{') {
        return JSON_ERR_NOT_OBJECT;
    }
    
    /* Required fields */
    if (!json_has_field(json_str, "task_type")) {
        return JSON_ERR_MISSING_REQUIRED;
    }
    
    if (!json_has_field(json_str, "file")) {
        return JSON_ERR_MISSING_REQUIRED;
    }
    
    return JSON_VALID;
}

const char* json_validation_strerror(json_validation_error_t err) {
    switch (err) {
        case JSON_VALID:
            return "Valid JSON";
        case JSON_ERR_NULL_INPUT:
            return "NULL input";
        case JSON_ERR_PARSE_ERROR:
            return "JSON parse error";
        case JSON_ERR_NOT_OBJECT:
            return "Expected JSON object";
        case JSON_ERR_MISSING_REQUIRED:
            return "Missing required field";
        case JSON_ERR_INVALID_TYPE:
            return "Invalid field type";
        case JSON_ERR_OUT_OF_RANGE:
            return "Value out of range";
        default:
            return "Unknown error";
    }
}
