#ifndef __TIMER_MOTOR_H__
#define __TIMER_MOTOR_H__

#include "tim.h"
#include "common_debug.h"
typedef struct
{
    TIM_HandleTypeDef *htim;
    uint16_t channel;
    int16_t speed;
} motor_struct;

void timer_motor_speed(motor_struct *motor);
void motor_start(motor_struct *motor);
#endif
