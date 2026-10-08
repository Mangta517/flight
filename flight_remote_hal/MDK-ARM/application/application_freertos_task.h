#ifndef __APPLICATION_FREERTOS_TASK_H__
#define __APPLICATION_FREERTOS_TASK_H__

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "common_debug.h"
#include "nrf24l01p.h"
#include "application_process_data.h"
#include "application_transmit_data.h"
#include "application_display.h"
#include "buzz.h"

#define remote_control_TASK_STACK_SIZE 128
#define remote_control_PRIORITY        4
#define remote_task_period             10

#define key_get_TASK_STACK_SIZE 128
#define key_get_task_PRIORITY   3
#define key_get_task_period     20

#define joystick_TASK_STACK_SIZE 128
#define joystick_task_PRIORITY   3
#define joystick_task_period     6

#define oled_TASK_STACK_SIZE 258
#define oled_task_PRIORITY   2
#define oled_task_period     80

#define flight_fail_TASK_STACK_SIZE 128
#define flight_fail_task_PRIORITY   1
#define flight_fail_task_period     100

//启动freertos任务系统

void application_freertos_start(void);

#endif
