#ifndef __JOYSTICK_H__
#define __JOYSTICK_H__

#include "main.h"
#include "adc.h"
#include "common_debug.h"
typedef struct
{
    int16_t thr;
    int16_t yaw;
    int16_t pit;
    int16_t rol;
} Joystick_struct;
//初始化ADC摇杆
void Joystick_Init(void);

//获取ADC摇杆的值

void Joystick_GetValue(Joystick_struct *joystick_value);
#endif
