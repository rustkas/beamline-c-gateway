/**
 * log_sanitizer.c - Log sanitization implementation
 */

#define _GNU_SOURCE
#include "jsonl_logger.h"
#include <string.h>
#include <strings.h>

/* Sensitive keys list */
static const char* SENSITIVE_KEYS[] = {
    "token", "api_key", "authorization", "password", 
    "secret", "auth", "bearer", "key",
    NULL
};

int jsonl_is_sensitive_key(const char *key) {
    if (!key) return 0;
    
    for (int i = 0; SENSITIVE_KEYS[i] != NULL; i++) {
        if (strcasestr(key, SENSITIVE_KEYS[i]) != NULL) {
            return 1;
        }
    }
    return 0;
}

int jsonl_sanitize_json(const char *json_str, char *out_buf, size_t buf_size) {
    if (!json_str || !out_buf || buf_size == 0) {
        return -1;
    }
    
    const char *src = json_str;
    char *dst = out_buf;
    size_t remaining = buf_size - 1;
    
    while (*src && remaining > 0) {
        if (*src == '"') {
            /* Copy opening quote */
            *dst++ = *src++;
            remaining--;
            if (remaining == 0) break;
            
            /* Extract key */
            char key[64] = {0};
            int idx = 0;
            while (*src && *src != '"' && idx < 63 && remaining > 0) {
                key[idx++] = *src;
                *dst++ = *src++;
                remaining--;
            }
            
            /* Copy closing quote */
            if (*src == '"' && remaining > 0) {
                *dst++ = *src++;
                remaining--;
            }
            
            /* Check if sensitive */
            if (jsonl_is_sensitive_key(key)) {
                /* Skip : and spaces */
                while (*src && (*src == ' ' || *src == ':') && remaining > 0) {
                    *dst++ = *src++;
                    remaining--;
                }
                
                /* Mask the value */
                if (*src == '"') {
                    /* String value */
                    *dst++ = *src++; /* opening " */
                    remaining--;
                    
                    /* Skip original value */
                    while (*src && *src != '"') src++;
                    
                    /* Write *** */
                    if (remaining >= 3) {
                        *dst++ = '*';
                        *dst++ = '*';
                        *dst++ = '*';
                        remaining -= 3;
                    }
                    
                    /* Closing " */
                    if (*src == '"' && remaining > 0) {
                        *dst++ = *src++;
                        remaining--;
                    }
                } else {
                    /* Non-string, skip to delimiter */
                    while (*src && *src != ',' && *src != '}' && *src != ']') src++;
                    
                    /* Write masked */
                    const char *mask = "\"***\"";
                    while (*mask && remaining > 0) {
                        *dst++ = *mask++;
                        remaining--;
                    }
                }
            }
        } else {
            /* Copy as-is */
            *dst++ = *src++;
            remaining--;
        }
    }
    
    *dst = '\0';
    return 0;
}
