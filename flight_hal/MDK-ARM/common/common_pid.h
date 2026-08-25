#ifndef __COMMON_PID_H__
#define __COMMON_PID_H__

#define PID_Period 0.002
#include "main.h"
//PID结构体
typedef struct {
    float kp;
    float ki;
    float kd;
    float err;
    float desire;
    float measure;
    float last_err;
    float integral;
    float integral_limit;
    float output;
    float dt;          // 实际控制周期(s)，每次计算前由调用方设置
} pid_struct;

void com_pid_calc(pid_struct *pid);

void com_pid_calc_chain(pid_struct *out_pid, pid_struct *in_pid);

void com_pid_reset(pid_struct *pid);
//限制电机速度
int16_t com_limit(int16_t speed,int16_t min_speed,int16_t max_speed);

#endif 


