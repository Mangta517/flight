#include "application_process_data.h"

Joystick_struct joystick_value={0}; // 声明外部变量
remote_data remote_data_value={0}; // 声明外部变量

extern SemaphoreHandle_t joystick_mutex; // 摇杆数据互斥锁
//区分摇杆值和按键微调值
int16_t key_pit_offset=0; // 按键微调值
int16_t key_rol_offset=0; // 按键微调值

//摇杆偏移量
int16_t thr_offset=0; // 油门偏移量
int16_t yaw_offset=0; // 航向偏移量
int16_t pit_offset=0; // 俯仰偏移量
int16_t rol_offset=0; // 横滚偏移量

//校准摇杆函数
//calib_thr: 1=同时校准油门零点(手动校准,要求油门推到底); 0=不校准油门(自动校准)
void calibrate_joystick(uint8_t calib_thr)
{
    //在进行摇杆校准时把按键微调值清零
    xSemaphoreTake(joystick_mutex, portMAX_DELAY); 
    key_pit_offset=0;
    key_rol_offset=0;
    xSemaphoreGive(joystick_mutex);
    int16_t thr_sum = 0, yaw_sum = 0, pit_sum = 0, rol_sum = 0;
    for (int i = 0; i < 10; i++)
    {
        process_joystick_data(); // 获取当前摇杆数据
        if (calib_thr)
        {
            thr_sum += joystick_value.thr - 0; // 油门零点为0(仅手动校准时采样)
        }
        yaw_sum += joystick_value.yaw - 500;
        pit_sum += joystick_value.pit - 500;
        rol_sum += joystick_value.rol - 500;
        vTaskDelay(10);
    }
    if (calib_thr)
    {
        thr_offset += thr_sum / 10;
    }
    yaw_offset += yaw_sum / 10;
    pit_offset += pit_sum / 10;
    rol_offset += rol_sum / 10;

}

void process_key_data(void)
{
    //使用互斥锁保护按键数据和摇杆数据的访问，防止数据冲突
    key_type key = key_get();
    if(key==KEY_RIGHT_TOP_LONG)
    {//校准摇杆(手动校准: 校准油门,要求油门推到底)
        calibrate_joystick(1);
        return;
    }
    xSemaphoreTake(joystick_mutex, portMAX_DELAY); // 获取互斥锁
    if (key == KEY_UP)
    { // 向前飞微调，俯仰角＋
        key_pit_offset += 10;
    }
    else if (key == KEY_DOWN)
    { // 向后飞微调，俯仰角－
        key_pit_offset -= 10;
    }
    else if (key == KEY_LEFT)
    { // 向左飞微调，横滚角－
        key_rol_offset -= 10;
    }
    else if (key == KEY_RIGHT)
    { // 向右飞微调，横滚角＋
        key_rol_offset += 10;
    }else if(key==KEY_RIGHT_TOP)
    { // 定高开关
        remote_data_value.fix_high = 1; // 切换定高开关状态
    }else if(key==KEY_LEFT_TOP)
    {//关机按键，按下一次只发送一次切换命令
        remote_data_value.shutdown = 1;
    }
    xSemaphoreGive(joystick_mutex); // 释放互斥锁
}

void process_joystick_data(void)
{
    //同一个全局变量，摇杆和按键都使用这个变量，为了防止故障需要进入临界区
    //taskENTER_CRITICAL();
    xSemaphoreTake(joystick_mutex, portMAX_DELAY); // 获取互斥锁，等待直到可用
    Joystick_GetValue(&joystick_value);
    // 把ADC数据的范围和极性转换为有效控制信号,摇杆范围是0-1000
    joystick_value.thr = joystick_value.thr * 1000 / 4095;
    joystick_value.yaw = joystick_value.yaw * 1000 / 4095;
    joystick_value.pit = joystick_value.pit * 1000 / 4095;
    joystick_value.rol = joystick_value.rol * 1000 / 4095;

    // 零偏修正：应用校准结果，消除摇杆中位偏移(如PIT中位505~520)
    // 否则中位偏移会变成持续的俯仰/横滚/偏航指令，导致飞机匀速往一个方向飘
    joystick_value.thr -= thr_offset;
    joystick_value.yaw -= yaw_offset;
    joystick_value.pit -= pit_offset;
    joystick_value.rol -= rol_offset;
    // 按键微调修正
    joystick_value.pit += key_pit_offset;
    joystick_value.rol += key_rol_offset;

    joystick_value.thr = com_limit(joystick_value.thr, 0, 1000); // 油门范围限制在0-1000
    joystick_value.yaw = com_limit(joystick_value.yaw, 0, 1000); // 航向范围限制在0-1000
    joystick_value.pit = com_limit(joystick_value.pit, 0, 1000); // 俯仰范围限制在0到1000
    joystick_value.rol = com_limit(joystick_value.rol, 0, 1000); // 横滚范围限制在0到1000

    xSemaphoreGive(joystick_mutex); // 释放互斥锁
    // 将摇杆数据赋值给遥控数据结构体
    remote_data_value.thr = joystick_value.thr;
    remote_data_value.yaw = joystick_value.yaw;
    remote_data_value.pit = joystick_value.pit;
    remote_data_value.rol = joystick_value.rol;

    //debug_printf(":%d,%d,%d,%d\n", joystick_value.yaw, joystick_value.thr, joystick_value.rol, joystick_value.pit);
}
