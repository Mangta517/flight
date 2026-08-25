/**
 * @file unity.c
 * @brief Unity 单元测试框架实现
 */

#include "unity.h"
#include <stdio.h>

/* ========== 全局状态变量 ========== */
int unity_tests_run = 0;
int unity_tests_passed = 0;
int unity_tests_failed = 0;
jmp_buf unity_test_env;

/**
 * @brief 输出测试总结
 */
void unity_test_summary(void) {
    printf("\n----------------------------------------\n");
    printf("  测试总数: %d\n", unity_tests_run);
    printf("  通过:     %d\n", unity_tests_passed);
    printf("  失败:     %d\n", unity_tests_failed);
    
    if (unity_tests_failed == 0) {
        printf("  结果:     ALL TESTS PASSED ✓\n");
    } else {
        float pass_rate = (float)unity_tests_passed / unity_tests_run * 100;
        printf("  通过率:   %.1f%%\n", pass_rate);
        printf("  结果:     SOME TESTS FAILED ✗\n");
    }
    printf("----------------------------------------\n");
}
