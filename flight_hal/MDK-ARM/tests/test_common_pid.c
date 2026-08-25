#include <stdio.h>
#include "unity.h"
#include "common_pid.h"

void test_com_pid_calc_output_positive_when_desire_above_measure(void)
{
    pid_struct pid = {
        .kp = 1.0f,
        .ki = 0.0f,
        .kd = 0.0f,
        .desire = 10.0f,
        .measure = 4.0f,
    };

    com_pid_calc(&pid);

    TEST_ASSERT_TRUE(pid.err > 0.0f);
    TEST_ASSERT_TRUE(pid.output > 0.0f);
}

void test_com_pid_calc_output_negative_when_measure_above_desire(void)
{
    pid_struct pid = {
        .kp = 1.0f,
        .ki = 0.0f,
        .kd = 0.0f,
        .desire = 2.0f,
        .measure = 7.0f,
    };

    com_pid_calc(&pid);

    TEST_ASSERT_TRUE(pid.err < 0.0f);
    TEST_ASSERT_TRUE(pid.output < 0.0f);
}

int main(void)
{
    UNITY_BEGIN();

    printf("\n========== common_pid 单元测试开始 ==========\n");

    RUN_TEST(test_com_pid_calc_output_positive_when_desire_above_measure);
    RUN_TEST(test_com_pid_calc_output_negative_when_measure_above_desire);

    printf("========== common_pid 单元测试结束 ==========\n\n");

    return UNITY_END();
}
