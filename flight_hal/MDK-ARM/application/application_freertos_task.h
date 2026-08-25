#ifndef __APPLICATION_FREERTOS_TASK_H__
#define __APPLICATION_FREERTOS_TASK_H__

#include "FreeRTOS.h"
#include "task.h"
#include "common_debug.h"
#include "timer_motor.h"
#include "LED.h"
#include "common_config.h"
#include "nRF24L01P.h"
#include "application_recevie_data.h"
#include "application_flight.h"
#include "spl006.h"


#define FLIGHT_TASK_STACK_SIZE 128
#define FLIGHT_TASK_PRIORITY 3
#define flight_task_period 2   // 飞行任务周期2ms = 500Hz修正率


#define LED_TASK_STACK_SIZE 128
#define LED_TASK_PRIORITY 1
#define led_task_period 100

#define flight_remote_control_TASK_STACK_SIZE 128
#define flight_remote_control_PRIORITY 4
#define flight_remote_control_period 10

#define power_task_STACK_SIZE 128
#define power_task_PRIORITY 4

//启动freertos任务系统
void shutdown_task(void);
void open_task(void);
void power_task(void *parameter);
void application_freertos_start(void);
void flight_task(void *parameter);
void led_task(void *parameter);
void flight_remote_control_task(void *parameter);

#endif
