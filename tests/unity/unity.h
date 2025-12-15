/* Unity Test Framework - Header File */
/* This is a minimal Unity header for Gateway tests */
/* Full Unity framework can be added as git submodule later */

#ifndef UNITY_H
#define UNITY_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Unity Test Results */
typedef struct {
    int tests_run;
    int tests_passed;
    int tests_failed;
    int tests_ignored;
} UnityTestResults;

static UnityTestResults unity_results = {0, 0, 0, 0};

/* Unity Macros */
#define UNITY_BEGIN() \
    do { \
        unity_results.tests_run = 0; \
        unity_results.tests_passed = 0; \
        unity_results.tests_failed = 0; \
        unity_results.tests_ignored = 0; \
        printf("\n========== Unity Test Suite ==========\n"); \
    } while(0)

#define UNITY_END() \
    (unity_results.tests_failed == 0 ? 0 : 1)

#define RUN_TEST(test_func) \
    do { \
        printf("\n[RUN] %s\n", #test_func); \
        unity_results.tests_run++; \
        test_func(); \
        unity_results.tests_passed++; \
        printf("[PASS] %s\n", #test_func); \
    } while(0)

/* Assertions */
#define TEST_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            printf("[FAIL] %s:%d: Assertion failed: %s\n", __FILE__, __LINE__, #condition); \
            unity_results.tests_failed++; \
            unity_results.tests_passed--; \
            return; \
        } \
    } while(0)

#define TEST_ASSERT_EQUAL(expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            printf("[FAIL] %s:%d: Expected %d, got %d\n", __FILE__, __LINE__, (int)(expected), (int)(actual)); \
            unity_results.tests_failed++; \
            unity_results.tests_passed--; \
            return; \
        } \
    } while(0)

#define TEST_ASSERT_EQUAL_STRING(expected, actual) \
    do { \
        if (strcmp((expected), (actual)) != 0) { \
            printf("[FAIL] %s:%d: Expected '%s', got '%s'\n", __FILE__, __LINE__, (expected), (actual)); \
            unity_results.tests_failed++; \
            unity_results.tests_passed--; \
            return; \
        } \
    } while(0)

#define TEST_ASSERT_NOT_NULL(ptr) \
    do { \
        if ((ptr) == NULL) { \
            printf("[FAIL] %s:%d: Pointer is NULL\n", __FILE__, __LINE__); \
            unity_results.tests_failed++; \
            unity_results.tests_passed--; \
            return; \
        } \
    } while(0)

#define TEST_ASSERT_NULL(ptr) \
    do { \
        if ((ptr) != NULL) { \
            printf("[FAIL] %s:%d: Pointer is not NULL\n", __FILE__, __LINE__); \
            unity_results.tests_failed++; \
            unity_results.tests_passed--; \
            return; \
        } \
    } while(0)

#define TEST_ASSERT_TRUE(condition) TEST_ASSERT(condition)
#define TEST_ASSERT_FALSE(condition) TEST_ASSERT(!(condition))

/* Setup and Teardown */
#define setUp() void setUp(void)
#define tearDown() void tearDown(void)

/* Test function declaration */
#define TEST(test_name) void test_##test_name(void)

#endif /* UNITY_H */

