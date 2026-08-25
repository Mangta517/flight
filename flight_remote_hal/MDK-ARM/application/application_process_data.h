#ifndef __APPLICATION_PROCESS_DATA_H__
#define __APPLICATION_PROCESS_DATA_H__

#include "joystick.h"
#include "key.h"
#include "commob_tool.h"
#include "FreeRTOS.h"
#include "task.h"    
#include "semphr.h"
typedef struct
{
    int16_t thr; // 油门
    int16_t yaw; // 航向
    int16_t pit; // 俯仰
    int16_t rol; // 横滚
    uint8_t shutdown; // 关机开关,1关机，0开机
    uint8_t fix_high; // 定高开关，1切换定高开关状态，0不切换
} remote_data;
//处理按键数据,对按下按键的数据进行记录
void process_key_data(void);
//处理摇杆数据,对摇杆数据进行修正
void process_joystick_data(void);
//校准摇杆: 采样当前摇杆位置作为零点偏移(要求摇杆回中)
//calib_thr: 1=同时校准油门零点(手动校准,要求油门推到底); 0=不校准油门(自动校准)
void calibrate_joystick(uint8_t calib_thr);
#endif

