/**
 * test_nats_subjects.c - Test NATS subjects
 */

#include "nats_subjects.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

static void test_subject_validation(void) {
    printf("Test: subject validation... ");
    
    /* Valid */
    assert(nats_subject_is_valid("beamline.router.v1.decide") == 1);
    assert(nats_subject_is_valid("beamline.router.v1.stream") == 1);
    
    /* Invalid */
    assert(nats_subject_is_valid(NULL) == 0);
    assert(nats_subject_is_valid("") == 0);
    assert(nats_subject_is_valid("wrong.subject") == 0);
    assert(nats_subject_is_valid("beamline.*") == 0);  /* No wildcards */
    
    printf("OK\n");
}

static void test_subject_builder(void) {
    printf("Test: subject builder... ");
    
    char subject[128];
    int rc = nats_subject_build_router(subject, sizeof(subject));
    
    assert(rc == 0);
    assert(strcmp(subject, "beamline.router.v1.decide") == 0);
    
    printf("OK\n");
    printf("  Built subject: %s\n", subject);
}

int main(void) {
    printf("=== NATS Subjects Tests ===\n");
    
    test_subject_validation();
    test_subject_builder();
    
    printf("\nAll tests passed!\n");
    return 0;
}
