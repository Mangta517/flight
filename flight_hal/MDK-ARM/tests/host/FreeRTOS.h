#ifndef FLIGHT_HOST_RTOS_H
#define FLIGHT_HOST_RTOS_H
#include "main.h"
typedef uint32_t TickType_t;
typedef void *TaskHandle_t;
#define configTICK_RATE_HZ 1000
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
void test_enter_critical(void);
void test_exit_critical(void);
#define taskENTER_CRITICAL() test_enter_critical()
#define taskEXIT_CRITICAL() test_exit_critical()
#endif
