#include "timer_motor.h"


//修改电机速度
void timer_motor_speed(motor_struct *motor)
{
    if(motor->speed > 1000)
    {
        motor->speed = 1000;
    }
    else if(motor->speed < 0)
    {
        motor->speed = 0;
    }
    __HAL_TIM_SET_COMPARE(motor->htim, motor->channel, (uint16_t)motor->speed);
}
//电机启动
void motor_start(motor_struct *motor)
{
    HAL_TIM_PWM_Start(motor->htim, motor->channel);
}
