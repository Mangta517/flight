#include "application_freertos_task.h"

static TaskHandle_t remote_control_handle;
static TaskHandle_t key_get_handle;
static TaskHandle_t joystick_handle;
static TaskHandle_t oled_handle;
static TaskHandle_t flight_fail_handle;
SemaphoreHandle_t joystick_mutex; // 摇杆数据互斥锁
//启动freertos任务系统
extern uint8_t post_buff[TX_PLOAD_WIDTH]; // TX_PLOAD_WIDTH bytes payload width for post-processing
extern uint8_t remote_connected;          // 遥控器与飞控连接状态
extern Joystick_struct joystick_value;    // 摇杆数据(用于自动校准回中判断)
//遥控任务
void remote_control_task(void *parameter);
//按键任务
void key_get_task(void *parameter);
//摇杆任务
void joystick_task(void *parameter);
//OLED显示任务
void oled_task(void *parameter);
//飞行异常状态任务
void flight_fail_task(void *parameter);

void application_freertos_start(void)
{
    // 在启动调度器前先创建互斥锁，避免任务先运行时句柄仍为空
    joystick_mutex = xSemaphoreCreateMutex();
    if (joystick_mutex == NULL)
    {
        Error_Handler();
    }

    //1.创建通讯任务
    xTaskCreate(remote_control_task,
                "remote_control_task",
                remote_control_TASK_STACK_SIZE,
                NULL,
                remote_control_PRIORITY,
                &remote_control_handle);
    //2.创建按键任务
    xTaskCreate(key_get_task, "key_get_task", key_get_TASK_STACK_SIZE, NULL, key_get_task_PRIORITY, &key_get_handle);
    //3.创建摇杆任务
    xTaskCreate(
        joystick_task, "joystick_task", joystick_TASK_STACK_SIZE, NULL, joystick_task_PRIORITY, &joystick_handle);
    //4.创建OLED显示任务
    xTaskCreate(oled_task, "oled_task", oled_TASK_STACK_SIZE, NULL, oled_task_PRIORITY, &oled_handle);
    //5.创建飞行异常状态任务
    xTaskCreate(flight_fail_task,
                "flight_fail_task",
                flight_fail_TASK_STACK_SIZE,
                NULL,
                flight_fail_task_PRIORITY,
                &flight_fail_handle);
    //启动调度器
    vTaskStartScheduler();
}
uint8_t remote_data_buf[TX_PLOAD_WIDTH] = {0}; // TX_PLOAD_WIDTH bytes payload width for TX

void key_get_task(void *parameter)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();

    while (1)
    {
        process_key_data();
        //20ms执行一次按键任务
        vTaskDelayUntil(&xLastWakeTime, key_get_task_period);
    }
}

void remote_control_task(void *parameter)
{
    while (1)
    {
        //将打包完成的遥控数据发送出去
        application_transmit_data();
        //固定间隔发送：任务单次执行时间可能超过周期(等待飞控回复)，若用 vTaskDelayUntil
        //会连续空转刷爆射频导致链路不稳，这里改用 vTaskDelay 保证每次发送之间至少间隔一个周期
        vTaskDelay(remote_task_period);
    }
}

void joystick_task(void *parameter)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    // 初始化ADC摇杆
    Joystick_Init();
    // 连接上升沿自动校准: 遥控器与飞控连接成功后自动校准摇杆一次
    // 自动校准不校准油门(calib_thr=0),因此无需油门最低,只要求 yaw/pit/rol 回中;
    // 若需要校准油门,长按校准键(KEY_RIGHT_TOP_LONG)手动校准(此时油门推到底)
    // 8秒内未回中则放弃本次自动校准
    uint8_t last_connected = 0;
    uint8_t calib_pending = 0;
    uint16_t calib_wait_ms = 0;
    while (1)
    { // 获取摇杆的ADC值
        process_joystick_data();
        // 检测连接上升沿(断开->连接)
        if (remote_connected && !last_connected)
        {
            calib_pending = 1;
            calib_wait_ms = 0;
        }
        last_connected = remote_connected;
        // 连接成功后等待回中并自动校准
        if (calib_pending)
        {
            calib_wait_ms += joystick_task_period;
            if (joystick_value.yaw > 350 && joystick_value.yaw < 650 && joystick_value.pit > 350 &&
                joystick_value.pit < 650 && joystick_value.rol > 350 && joystick_value.rol < 650)
            {
                calibrate_joystick(0); // 自动校准: 不校准油门
                calib_pending = 0;
                debug_printf("自动摇杆校准完成\n");
            }
            else if (calib_wait_ms >= 8000)
            {
                calib_pending = 0; // 超时跳过,保留手动校准
                debug_printf("摇杆未回中,跳过自动校准,可长按校准键手动校准\n");
            }
        }
        vTaskDelayUntil(&xLastWakeTime, joystick_task_period);
    }
}

void oled_task(void *parameter)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    // 初始化OLED显示
    app_OLED_Init();
    while (1)
    {
        // 在OLED上显示遥控数据
        app_OLED_Show();
        vTaskDelayUntil(&xLastWakeTime, oled_task_period);
    }
}

void flight_fail_task(void *parameter)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    while (1)
    {
        // 蜂鸣器报警：连接成功后飞控上报异常才报警
        if (remote_connected && post_buff[0] == 1)
        {
            buzz_on();
        }
        else
        {
            buzz_off();
        }
        vTaskDelayUntil(&xLastWakeTime, flight_fail_task_period);
    }
}
