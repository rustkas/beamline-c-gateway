/**
 * nats_subjects.c - NATS subject implementation
 */

#include "nats_subjects.h"
#include <string.h>
#include <stdio.h>

int nats_subject_is_valid(const char *subject) {
    if (!subject || *subject == '\0') {
        return 0;
    }
    
    /* Must start with beamline. */
    if (strncmp(subject, "beamline.", 9) != 0) {
        return 0;
    }
    
    /* No wildcards in canonical subjects */
    if (strchr(subject, '*') || strchr(subject, '>')) {
        return 0;
    }
    
    return 1;
}

int nats_subject_build_router(char *out_buf, size_t buf_size) {
    if (!out_buf || buf_size == 0) {
        return -1;
    }
    
    int n = snprintf(out_buf, buf_size, "%s", NATS_SUBJECT_ROUTER_DECIDE);
    
    if (n < 0 || (size_t)n >= buf_size) {
        return -1;
    }
    
    return 0;
}
