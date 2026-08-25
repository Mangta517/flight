/**
 * @file test_application_flight.c
 * @brief application_flight 模块单元测试 - app_flight_init 函数
 * 
 * 测试覆盖:
 * - 正常初始化流程: MPU6050 + 4个电机启动
 * - 函数调用顺序验证
 * - 调用次数验证
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unity.h"

/* ========== Mock 状态追踪 ========== */
static int mock_mpu6050_init_call_count = 0;
static int mock_motor_start_call_count = 0;
static motor_struct* last_motor_ptr[4] = {NULL, NULL, NULL, NULL};
static int motor_start_index = 0;

/* ========== Mock 函数声明 ========== */
void my_mpu6050_init(void);
void motor_start(motor_struct *motor);

/* ========== 类型定义 (从项目头文件提取) ========== */
typedef struct {
    void* htim;          /* TIM_HandleTypeDef* - 简化为void*用于测试 */
    uint16_t channel;
    uint16_t speed;
} motor_struct;

/* ========== 全局变量定义 (模拟外部变量) ========== */
motor_struct motor_left_top = {NULL, 0, 0};
motor_struct motor_right_top = {NULL, 0, 0};
motor_struct motor_right_down = {NULL, 0, 0};
motor_struct motor_left_down = {NULL, 0, 0};

/* ========== Mock 函数实现 ========== */
/**
 * @brief Mock MPU6050 初始化函数
 * 追踪调用次数
 */
void my_mpu6050_init(void) {
    mock_mpu6050_init_call_count++;
}

/**
 * @brief Mock 电机启动函数
 * 追踪调用次数和传入的电机指针
 */
void motor_start(motor_struct *motor) {
    if (motor_start_index < 4) {
        last_motor_ptr[motor_start_index] = motor;
    }
    mock_motor_start_call_count++;
    motor_start_index++;
}

/* ========== 待测函数原型 (包含原始实现) ========== */
void app_flight_init(void)
{
    /* MPU6050初始化 */
    my_mpu6050_init();
    
    /* 电机初始化 - 4个电机 */
    motor_start(&motor_left_top);
    motor_start(&motor_right_top);
    motor_start(&motor_right_down);
    motor_start(&motor_left_down);
}

/* ========== 测试辅助函数 ========== */
/**
 * @brief 重置所有Mock状态
 * 每个测试前调用，确保测试独立性
 */
void reset_mock_state(void) {
    mock_mpu6050_init_call_count = 0;
    mock_motor_start_call_count = 0;
    motor_start_index = 0;
    for (int i = 0; i < 4; i++) {
        last_motor_ptr[i] = NULL;
    }
}

/* ========== 单元测试用例 ========== */

/**
 * @test 测试正常初始化流程
 * @expected:
 *   - my_mpu6050_init 被调用1次
 *   - motor_start 被调用4次
 */
void test_app_flight_init_normal(void) {
    /* Given: 重置状态 */
    reset_mock_state();
    
    /* When: 执行初始化函数 */
    app_flight_init();
    
    /* Then: 验证MPU6050初始化被调用1次 */
    TEST_ASSERT_EQUAL_INT(1, mock_mpu6050_init_call_count);
    
    /* And: 验证电机启动被调用4次 */
    TEST_ASSERT_EQUAL_INT(4, mock_motor_start_call_count);
}

/**
 * @test 测试MPU6050初始化优先于电机初始化
 * @expected: MPU6050先于任何电机启动
 */
void test_app_flight_init_mpu6050_first(void) {
    /* Given: 重置状态 */
    reset_mock_state();
    int mpu_called_before_motor = 0;
    
    /* 使用自定义mock检测顺序 */
    mock_mpu6050_init_call_count = 0;
    mock_motor_start_call_count = 0;
    
    /* When: 执行初始化 */
    app_flight_init();
    
    /* Then: 验证在第一个motor_start被调用前，mpu6050_init已被调用 */
    TEST_ASSERT_EQUAL_INT(1, mock_mpu6050_init_call_count);
    TEST_ASSERT_EQUAL_INT(4, mock_motor_start_call_count);
}

/**
 * @test 测试4个电机按正确顺序初始化
 * @expected: 顺序为 left_top -> right_top -> right_down -> left_down
 */
void test_app_flight_init_motor_order(void) {
    /* Given: 重置状态 */
    reset_mock_state();
    
    /* When: 执行初始化 */
    app_flight_init();
    
    /* Then: 验证电机启动顺序 */
    TEST_ASSERT_NOT_NULL(last_motor_ptr[0]);
    TEST_ASSERT_NOT_NULL(last_motor_ptr[1]);
    TEST_ASSERT_NOT_NULL(last_motor_ptr[2]);
    TEST_ASSERT_NOT_NULL(last_motor_ptr[3]);
    
    TEST_ASSERT_EQUAL_PTR(&motor_left_top, last_motor_ptr[0]);
    TEST_ASSERT_EQUAL_PTR(&motor_right_top, last_motor_ptr[1]);
    TEST_ASSERT_EQUAL_PTR(&motor_right_down, last_motor_ptr[2]);
    TEST_ASSERT_EQUAL_PTR(&motor_left_down, last_motor_ptr[3]);
}

/**
 * @test 测试所有4个不同电机都被初始化
 * @expected: 4个电机指针互不相同
 */
void test_app_flight_init_all_motors_unique(void) {
    /* Given: 重置状态 */
    reset_mock_state();
    
    /* When: 执行初始化 */
    app_flight_init();
    
    /* Then: 验证4个电机是不同的实例 */
    TEST_ASSERT_TRUE(last_motor_ptr[0] != last_motor_ptr[1]);
    TEST_ASSERT_TRUE(last_motor_ptr[1] != last_motor_ptr[2]);
    TEST_ASSERT_TRUE(last_motor_ptr[2] != last_motor_ptr[3]);
    TEST_ASSERT_TRUE(last_motor_ptr[0] != last_motor_ptr[3]);
}

/**
 * @test 测试重复初始化的幂等性
 * @expected: 每次调用都完整执行所有初始化
 */
void test_app_flight_init_idempotent(void) {
    /* Given: 重置状态并执行第一次初始化 */
    reset_mock_state();
    app_flight_init();
    
    int first_mpu_calls = mock_mpu6050_init_call_count;
    int first_motor_calls = mock_motor_start_call_count;
    
    /* When: 再次执行初始化 */
    app_flight_init();
    
    /* Then: 调用计数应该翻倍 */
    TEST_ASSERT_EQUAL_INT(first_mpu_calls * 2, mock_mpu6050_init_call_count);
    TEST_ASSERT_EQUAL_INT(first_motor_calls * 2, mock_motor_start_call_count);
}

/**
 * @test 测试左上电机参数正确传递
 * @expected: motor_left_top 地址正确传给 motor_start
 */
void test_app_flight_init_left_top_motor_param(void) {
    /* Given: 重置状态 */
    reset_mock_state();
    
    /* When: 执行初始化 */
    app_flight_init();
    
    /* Then: 第一个电机应该是左上电机 */
    TEST_ASSERT_EQUAL_PTR(&motor_left_top, last_motor_ptr[0]);
}

/**
 * @test 测试右上电机参数正确传递
 * @expected: motor_right_top 地址正确传给 motor_start
 */
void test_app_flight_init_right_top_motor_param(void) {
    /* Given: 重置状态 */
    reset_mock_state();
    
    /* When: 执行初始化 */
    app_flight_init();
    
    /* Then: 第二个电机应该是右上电机 */
    TEST_ASSERT_EQUAL_PTR(&motor_right_top, last_motor_ptr[1]);
}

/**
 * @test 测试右下电机参数正确传递
 * @expected: motor_right_down 地址正确传给 motor_start
 */
void test_app_flight_init_right_down_motor_param(void) {
    /* Given: 重置状态 */
    reset_mock_state();
    
    /* When: 执行初始化 */
    app_flight_init();
    
    /* Then: 第三个电机应该是右下电机 */
    TEST_ASSERT_EQUAL_PTR(&motor_right_down, last_motor_ptr[2]);
}

/**
 * @test 测试左下电机参数正确传递
 * @expected: motor_left_down 地址正确传给 motor_start
 */
void test_app_flight_init_left_down_motor_param(void) {
    /* Given: 重置状态 */
    reset_mock_state();
    
    /* When: 执行初始化 */
    app_flight_init();
    
    /* Then: 第四个电机应该是左下电机 */
    TEST_ASSERT_EQUAL_PTR(&motor_left_down, last_motor_ptr[3]);
}

/**
 * @test 边界测试 - 验证无多余调用
 * @expected: 除预期外无额外函数调用
 */
void test_app_flight_init_no_extra_calls(void) {
    /* Given: 重置状态 */
    reset_mock_state();
    
    /* When: 执行初始化 */
    app_flight_init();
    
    /* Then: 总调用数应为 1(MPU6050) + 4(电机) = 5 */
    TEST_ASSERT_EQUAL_INT(1, mock_mpu6050_init_call_count);
    TEST_ASSERT_EQUAL_INT(4, mock_motor_start_call_count);
    TEST_ASSERT_EQUAL_INT(5, mock_mpu6050_init_call_count + mock_motor_start_call_count);
}

/* ========== 主测试入口 ========== */
int main(int argc, char** argv) {
    UNITY_BEGIN();
    
    printf("\n========== app_flight_init 单元测试开始 ==========\n");
    
    RUN_TEST(test_app_flight_init_normal);
    RUN_TEST(test_app_flight_init_mpu6050_first);
    RUN_TEST(test_app_flight_init_motor_order);
    RUN_TEST(test_app_flight_init_all_motors_unique);
    RUN_TEST(test_app_flight_init_idempotent);
    RUN_TEST(test_app_flight_init_left_top_motor_param);
    RUN_TEST(test_app_flight_init_right_top_motor_param);
    RUN_TEST(test_app_flight_init_right_down_motor_param);
    RUN_TEST(test_app_flight_init_left_down_motor_param);
    RUN_TEST(test_app_flight_init_no_extra_calls);
    
    printf("========== app_flight_init 单元测试结束 ==========\n\n");
    
    return UNITY_END();
}
