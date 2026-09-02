#ifndef __APPLICATION_FLIGHT_H__
#define __APPLICATION_FLIGHT_H__


#include "my_mpu6050.h"
#include "common_debug.h"
#include "common_filter.h"
#include "math.h"
#include "common_IMU.h"
#include "common_pid.h"
#include "application_recevie_data.h"
#include "timer_motor.h"
#include "spl006.h"

/* ============================================================
 * 俯仰配平角(°)：给俯仰角度环目标叠加一个固定偏移，让飞控主动保持
 * "机头抬起 X 度"悬停，抵消桨盘面机械前倾造成的持续前漂。
 * 原理: 前漂 = 推力带前向分量；机头抬起→推力向量后倾→前向分量抵消。
 * 注意: 与电机转速微调不同，角度配平会改变姿态环参考，不会被姿态环吸收。
 * 方向: 正值=机头抬起(抵消前漂)。若试飞前漂加剧，说明符号反了，取反即可。
 * 量级: 从 1~2° 起试，每次 ±1° 观察悬停是否稳住。
 * 调参记录(2026):
 *   5.5° → 积分生效后执行到位, 配平过头 → 向后飞
 *   4.75° → 仍向后飞(偏大)
 *   3.0° → 仍向后飞(偏大)
 *   1.5° → 当前值: 继续二分
 * ============================================================ */
#define PITCH_TRIM_DEG  2.75f

/* ============================================================
 * 横滚配平角(°)：与PITCH_TRIM_DEG同理，抵消桨盘面机械侧倾造成的持续横漂。
 * 方向: 左漂→先试正值;若左漂加剧或变右漂→取反(负值)。
 * 调参记录(2026):
 *   -7.0° / -4.5° → 配平方案验证期用
 *   0.0° → 当前值: 配平已去除, 依赖内环积分自动补偿
 * ============================================================ */
#define ROLL_TRIM_DEG  0.0f

void app_flight_init(void);
//根据陀螺仪数据计算欧拉角
void app_flight_get_euler_angle(void);
//根据欧拉角计算PID
void app_flight_pid_process(void);
//重置所有PID积分，防止积分饱和
void app_flight_pid_reset(void);
//根据PID值设置电机速度
void app_flight_motor_process(void);
//进入定高功能后的PID
void app_fixed_height_pid_process(void);

extern motor_struct motor_left_top;
extern motor_struct motor_right_top;
extern motor_struct motor_right_down;
extern motor_struct motor_left_down;
// 全局MPU6050六轴数据(定义于application_flight.c, 供飞行任务做静止零偏微调)
extern Accel_Gyro_struct accel_gyro_data;

#endif


