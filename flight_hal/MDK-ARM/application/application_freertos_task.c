#include "application_freertos_task.h"
static TaskHandle_t flight_task_handle;
static TaskHandle_t led_task_handle;
TaskHandle_t flight_remote_control_handle;
static TaskHandle_t power_task_handle;

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
//4.创建电源任务
xTaskCreate(power_task,"power_task",power_task_STACK_SIZE,NULL,power_task_PRIORITY,&power_task_handle);
//2.启动调度器
vTaskStartScheduler();
}
//关机函数
void shutdown_task(void)
{

    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, 0);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, 0);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_4, 0);
   
}

void open_task(void)
{

   /*  __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 100);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, 100);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, 100);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_4, 100); */
    //remote_state = REMOTE_CONNECTED;
  
}

void power_task(void *parameter)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    while (1)
    {
        uint32_t res = ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (res != 0)
        {

            if (remote_data_receive.shutdown == 1)
            {
                shutdown_task();
            }
            else
            {
                open_task();
            }
        }
    }
}
void flight_task(void *parameter)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    app_flight_init();
    // 记录上一次连接状态，用于检测"断开->连接"的跳变沿
    // MPU6050零偏校准不再在上电时自动执行，改为遥控器连接成功后校准一次
    // 注意: 初始化为DISCONNECTED，防止遥控任务(优先级更高)先置CONNECTED
    //       导致飞行任务首次进入循环时错过跳变沿、漏掉校准
    Remote_State last_remote_state = REMOTE_DISCONNECTED;
    while (1)

    {
        // 遥控器连接成功(断开->连接跳变沿)后，进行MPU6050零偏校准
        // 校准会阻塞本任务约0.6s，此时飞行状态仍为IDLE且电机速度为0，安全
        if (remote_state == REMOTE_CONNECTED && last_remote_state != REMOTE_CONNECTED)
        {
            // 调试确认: 打印配平角x10,验证固件是否包含 PITCH_TRIM_DEG 修改
            // (用整数避免microlib不支持浮点printf; 打印20=2.0°)
            //debug_printf("PITCH_TRIM=%d\n", (int)(PITCH_TRIM_DEG * 10.0f));
            my_mpu6050_zero_offset();
        }
        last_remote_state = remote_state;
        // 电机启动
       /*  motor_start(&motor_left_top);
        motor_start(&motor_right_top);
        motor_start(&motor_right_down);
        motor_start(&motor_left_down); */
        //根据MPU6050获取欧拉角
        app_flight_get_euler_angle();

        // 偏航零漂抑制: 偏航只有陀螺仪积分、无绝对参考，残余零偏会被积分放大。
        // IDLE(未解锁、电机停转)且三轴角速度均很小(约<3°/s)时，先累积100次读数
        // 取平均(抵消静止噪声)，再把平均残余增量累加进陀螺仪零点，收敛后yaw不再漂移。
        // 用户手持转动(角速度大)立即复位重来，不会误校准。
        if (flight_state == IDLE)
        {
            static uint8_t still_cnt = 0;
            static int32_t trim_sum_x = 0, trim_sum_y = 0, trim_sum_z = 0;
            static uint16_t trim_cnt = 0;
            if (abs(accel_gyro_data.Gyro.Gyro_x) < 50 &&
                abs(accel_gyro_data.Gyro.Gyro_y) < 50 &&
                abs(accel_gyro_data.Gyro.Gyro_z) < 50)
            {
                if (still_cnt < 250) still_cnt++;
                if (still_cnt >= 250) // 静止稳定约1s后开始累积平均
                {
                    trim_sum_x += accel_gyro_data.Gyro.Gyro_x;
                    trim_sum_y += accel_gyro_data.Gyro.Gyro_y;
                    trim_sum_z += accel_gyro_data.Gyro.Gyro_z;
                    trim_cnt++;
                    if (trim_cnt >= 100) // 100次×2ms≈0.2s平均一次
                    {
                        my_mpu6050_gyro_trim((int16_t)(trim_sum_x / 100),
                                             (int16_t)(trim_sum_y / 100),
                                             (int16_t)(trim_sum_z / 100));
                        trim_sum_x = trim_sum_y = trim_sum_z = 0;
                        trim_cnt = 0;
                    }
                }
            }
            else
            {
                still_cnt = 0;
                trim_sum_x = trim_sum_y = trim_sum_z = 0;
                trim_cnt = 0;
            }
        }
        remote_data remote_snapshot = remote_data_receive;
        // 仅在飞行状态且油门足够时才运行PID，防止地面振动导致积分饱和
        if ((flight_state == NORMAL || flight_state == FIX_HEIGHT)
            && remote_snapshot.thr >= 50)
        {
            app_flight_pid_process();
        }
        //判断定高状态，进入定高PID
        if(flight_state==FIX_HEIGHT)
        {
            static uint8_t count=0;
            count++;
            if(count>=3)
            {
                app_fixed_height_pid_process();
                count=0;
            }
        }
        //气压计周期采样：I2C1 总线由本任务(飞行任务)独占访问，MPU6050 与 SPL006
        //均在此任务内读写，避免与其他任务竞争总线(原实现用关中断保护，已移除)
        {
            static uint8_t spl_cnt = 0;
            spl_cnt++;
            if (spl_cnt >= 3)
            {
                spl_cnt = 0;
                spl_update_data();
            }
        }
        //根据PID值设置电机速度
        app_flight_motor_process();

        vTaskDelayUntil(&xLastWakeTime, flight_task_period);
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
    TickType_t xLastWakeTime = xTaskGetTickCount();
    while (1)
    {   // 接收遥控器数据并更新状态
        uint8_t res=application_recevie_data(); 
        // 处理连接状态，根据接收结果更新飞行状态
        application_process_connect_state(res); 
        //处理关机命令
        if(remote_data_receive.shutdown==1)
        {
           xTaskNotifyGive(power_task_handle);
        }
       
        //遇到故障等待飞行任务处理完成
        application_process_flight_state();
        
        //10ms接收一次遥控器数据，更新遥控器状态
        vTaskDelay(flight_remote_control_period);
    }
}


