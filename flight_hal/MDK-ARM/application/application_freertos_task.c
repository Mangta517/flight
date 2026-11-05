#include "application_freertos_task.h"
static TaskHandle_t flight_task_handle;
static TaskHandle_t led_task_handle;
TaskHandle_t flight_remote_control_handle;

extern int32_t spl006_pressure;
extern int32_t spl006_temperature;
extern int32_t spl006_compensated_pressure;

int32_t fix_height_pressure;
//内存管理=》c语言中的结构体一般保存在堆中，不会自动释放。解决方法，使用同一个结构体循环使用
//电机结构体
/* ============================================================
 * 电机布局（TOP 侧 = 机头/前）
 *
 *              机头(前)
 *            ┌───────────┐
 *            │ LT     RT │   LT=left_top    TIM2_CH3 (PA2)
 *            │           │   RT=right_top   TIM2_CH2 (PA1)
 *            │ LD     RD │   LD=left_down   TIM2_CH4 (PA3)
 *            └───────────┘   RD=right_down  TIM2_CH1 (PA0)
 *              机尾(后)
 *
 * 混控约定: +pitch→前电机加速 -pitch→后电机加速
 *           +roll →右电机加速  -roll →左电机加速
 *           +yaw  →对角配对参与
 * 电机转向(俯视): 标准X布局为对角同向、相邻反向，按实际装桨确认
 * ============================================================ */
motor_struct motor_left_top = {.htim = &htim2, .channel = TIM_CHANNEL_3, .speed = 0};
motor_struct motor_right_top = {.htim = &htim2, .channel = TIM_CHANNEL_2, .speed = 0};
motor_struct motor_right_down = {.htim = &htim2, .channel = TIM_CHANNEL_1, .speed = 0};
motor_struct motor_left_down = {.htim = &htim2, .channel = TIM_CHANNEL_4, .speed = 0};
//LED结构体
led_struct led_1_1={.GPIOx=LED1_1_GPIO_Port, .GPIO_Pin=LED1_1_Pin};
led_struct led_1_2={.GPIOx=LED1_2_GPIO_Port, .GPIO_Pin=LED1_2_Pin};



//表示连接状态
Remote_State remote_state=REMOTE_DISCONNECTED;

//表示飞行状态
Flight_State flight_state=IDLE;

volatile remote_data remote_data_receive={.thr=0,.yaw=500,.pit=500,.roll=500,.shutdown=0,.fix_height=0}; // 定义一个全局变量来保存接收到的遥控器数据
void application_freertos_start(void)
{
//1.创建飞行任务
xTaskCreate(flight_task, "flight_task", FLIGHT_TASK_STACK_SIZE, NULL, FLIGHT_TASK_PRIORITY, &flight_task_handle);
//2.创建led任务
xTaskCreate(led_task, "led_task", LED_TASK_STACK_SIZE, NULL,LED_TASK_PRIORITY,&led_task_handle);
//3.创建遥控器控制任务
xTaskCreate(flight_remote_control_task, "flight_remote_control_task",flight_remote_control_TASK_STACK_SIZE, NULL, flight_remote_control_PRIORITY, &flight_remote_control_handle);
vTaskStartScheduler();
}
//关机函数
void flight_task(void *parameter)
{
    TickType_t last_wake_time = xTaskGetTickCount();
    app_flight_init();
    while (1)
    {
        app_flight_cycle();
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(flight_task_period));
    }
}

void led_task(void *parameter)
{
    uint8_t count=0;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    while (1)
    {
        count++;
        //LED1_1表示连接状态
        if (remote_state == REMOTE_CONNECTED)
        {
            led_on(&led_1_1);
        }else
        {
            led_off(&led_1_1);
        }
        //LED1_2表示飞行状态
        if (flight_state == IDLE)
        {
            led_on(&led_1_2);
        }
        else if(flight_state==NORMAL)
        {
            if(count%2==0)
            {led_toggle(&led_1_2);}
            
        }
        else if(flight_state==FIX_HEIGHT)
        {
            if(count%4==0)
            {led_toggle(&led_1_2);}
           
        }
        else if(flight_state==FAIL)
        {
            if(count%1==0)
            {led_toggle(&led_1_2);}
           
        }
        else if(flight_state==SHUTDOWN)
        {
            led_off(&led_1_2);
        }
         if(count==100)
         {count=0;}
        vTaskDelayUntil(&xLastWakeTime, led_task_period);
    }
}

uint8_t rx_data[TX_PLOAD_WIDTH+1]={0};
void flight_remote_control_task(void *parameter)
{
    while (1)
    {
        application_recevie_data();
        vTaskDelay(pdMS_TO_TICKS(flight_remote_control_period));
    }
}
