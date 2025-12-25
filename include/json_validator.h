/**
 * json_validator.h - JSON schema validation for IPC payloads
 */

#ifndef JSON_VALIDATOR_H
#define JSON_VALIDATOR_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Validation error codes
 */
typedef enum {
    JSON_VALID = 0,
    JSON_ERR_NULL_INPUT,
    JSON_ERR_PARSE_ERROR,
    JSON_ERR_NOT_OBJECT,
    JSON_ERR_MISSING_REQUIRED,
    JSON_ERR_INVALID_TYPE,
    JSON_ERR_OUT_OF_RANGE,
} json_validation_error_t;

/**
 * Validate JSON string is well-formed
 * 
 * @param json_str  JSON string
 * @return JSON_VALID on success, error code otherwise
 */
json_validation_error_t json_validate_wellformed(const char *json_str);

/**
 * Validate task submit payload
 * 
 * Required fields: task_type, file
 * Optional: line, context, params
 * 
 * @param json_str  JSON payload
 * @return JSON_VALID on success, error code otherwise
 */
json_validation_error_t json_validate_task_submit(const char *json_str);

/**
 * Get validation error message
 * 
 * @param err  Error code
 * @return Error message string
 */
const char* json_validation_strerror(json_validation_error_t err);

#ifdef __cplusplus
}
#endif

#endif /* JSON_VALIDATOR_H */
