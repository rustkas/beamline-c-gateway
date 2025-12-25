/**
 * nats_subjects.h - NATS subject path management
 */

#ifndef NATS_SUBJECTS_H
#define NATS_SUBJECTS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Standard subject paths */
#define NATS_SUBJECT_ROUTER_DECIDE  "beamline.router.v1.decide"
#define NATS_SUBJECT_ROUTER_STREAM  "beamline.router.v1.stream"
#define NATS_SUBJECT_ROUTER_CANCEL  "beamline.router.v1.cancel"

/**
 * Validate NATS subject format
 * 
 * @param subject  Subject string
 * @return 1 if valid, 0 otherwise
 */
int nats_subject_is_valid(const char *subject);

/**
 * Build router subject (canonical)
 * 
 * @param out_buf   Output buffer
 * @param buf_size  Buffer size
 * @return 0 on success, -1 on error
 */
int nats_subject_build_router(char *out_buf, size_t buf_size);

#ifdef __cplusplus
}
#endif

#endif /* NATS_SUBJECTS_H */
