/**
 * @file unity.h
 * @brief Unity 单元测试框架头文件 - 轻量级嵌入式版本
 * 
 * 为 app_flight_init 测试提供最小化断言宏支持
 */

#ifndef UNITY_H
#define UNITY_H

#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>

/* ========== 测试结果统计 ========== */
extern int unity_tests_run;
extern int unity_tests_passed;
extern int unity_tests_failed;

/* ========== 长跳转支持 ========== */
extern jmp_buf unity_test_env;

/* ========== 框架初始化与结束 ========== */
#define UNITY_BEGIN()      (unity_tests_run = 0, unity_tests_passed = 0, unity_tests_failed = 0, 0)
#define UNITY_END()        (unity_test_summary(), unity_tests_failed)

void unity_test_summary(void);

/* ========== 断言宏 ========== */

/* 整数相等断言 */
#define TEST_ASSERT_EQUAL_INT(expected, actual) \
    do { \
        unity_tests_run++; \
        if ((expected) == (actual)) { \
            unity_tests_passed++; \
        } else { \
            unity_tests_failed++; \
            printf("  FAIL: %s:%d - Expected %d but got %d\n", __FILE__, __LINE__, (int)(expected), (int)(actual)); \
            longjmp(unity_test_env, 1); \
        } \
    } while(0)

/* 指针相等断言 */
#define TEST_ASSERT_EQUAL_PTR(expected, actual) \
    do { \
        unity_tests_run++; \
        if ((const void*)(expected) == (const void*)(actual)) { \
            unity_tests_passed++; \
        } else { \
            unity_tests_failed++; \
            printf("  FAIL: %s:%d - Expected pointer %p but got %p\n", __FILE__, __LINE__, (const void*)(expected), (const void*)(actual)); \
            longjmp(unity_test_env, 1); \
        } \
    } while(0)

/* 非空指针断言 */
#define TEST_ASSERT_NOT_NULL(pointer) \
    do { \
        unity_tests_run++; \
        if ((pointer) != NULL) { \
            unity_tests_passed++; \
        } else { \
            unity_tests_failed++; \
            printf("  FAIL: %s:%d - Expected non-NULL but got NULL\n", __FILE__, __LINE__); \
            longjmp(unity_test_env, 1); \
        } \
    } while(0)

/* 真值断言 */
#define TEST_ASSERT_TRUE(condition) \
    do { \
        unity_tests_run++; \
        if ((condition)) { \
            unity_tests_passed++; \
        } else { \
            unity_tests_failed++; \
            printf("  FAIL: %s:%d - Expected TRUE but got FALSE\n", __FILE__, __LINE__); \
            longjmp(unity_test_env, 1); \
        } \
    } while(0)

/* 假值断言 */
#define TEST_ASSERT_FALSE(condition) \
    do { \
        unity_tests_run++; \
        if (!(condition)) { \
            unity_tests_passed++; \
        } else { \
            unity_tests_failed++; \
            printf("  FAIL: %s:%d - Expected FALSE but got TRUE\n", __FILE__, __LINE__); \
            longjmp(unity_test_env, 1); \
        } \
    } while(0)

/* 运行单个测试用例 */
#define RUN_TEST(test_func) \
    do { \
        printf("  [RUN ] %s\n", #test_func); \
        if (setjmp(unity_test_env) == 0) { \
            test_func(); \
            printf("  [PASS] %s\n", #test_func); \
        } else { \
            printf("  [FAIL] %s\n", #test_func); \
        } \
    } while(0)

#endif /* UNITY_H */
