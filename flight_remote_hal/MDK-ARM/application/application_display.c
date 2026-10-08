#include "application_display.h"
#include <stdio.h>

extern remote_data remote_data_value;
// 摇杆数据互斥锁(定义在application_freertos_task.c; 显示任务与写方共用同一把锁)
extern SemaphoreHandle_t joystick_mutex;
// 飞控回传的MPU6050欧拉角(单位0.01°,定义在application_transmit_data.c)
extern int16_t imu_pitch;
extern int16_t imu_roll;
extern int16_t imu_yaw;

void app_oled_show_bar(uint8_t x, uint8_t y, uint8_t count)
{
    if (count < 13)
    {
        OLED_Show_CH(x, y, 12 + count, 12, 1);
    }
}

// 显示油门百分比，两位数字最大显示 99%，如 "88%"
static void app_oled_show_thr_pct(uint8_t x, uint8_t y, uint8_t pct)
{
    uint8_t buf[3] = {0};
    if (pct > 99U)
        pct = 99U;                  // 两位百分比，最大 99
    buf[0] = pct / 10U % 10U + '0'; // 十位
    buf[1] = pct % 10U + '0';       // 个位
    buf[2] = '%';
    OLED_ShowString(x, y, buf, 12, 1);
}

/* 在(x,y)显示一个欧拉角: 0.01°原始值→整数度, 固定4字符宽(符号+3位,不足补空格)。
 * 仅当 raw 与上次不同才重绘, 避免无谓的I2C刷新
 * is_yaw: 偏航为积分角、会持续增长, 显示前归一化到[-180,180]便于观察 */
static void app_oled_show_angle(uint8_t x, uint8_t y, int16_t raw, int16_t *saved, uint8_t is_yaw)
{
    if (raw == *saved)
        return;
    *saved = raw;
    uint8_t buf[8] = {0};
    int16_t v = raw / 100; // 转整数度
    if (is_yaw)
    {
        v %= 360; // 归一化到 [-359,359]
        if (v > 180)
            v -= 360;
        if (v < -180)
            v += 360;
    }
    uint8_t i = 0;
    if (v < 0)
    {
        buf[i++] = '-';
        v = -v;
    }
    else
    {
        buf[i++] = ' ';
    }
    if (v >= 100)
    {
        buf[i++] = (uint8_t)('0' + v / 100);
    }
    else
    {
        buf[i++] = ' ';
    }
    if (v >= 10)
    {
        buf[i++] = (uint8_t)('0' + (v / 10) % 10);
    }
    else
    {
        buf[i++] = ' ';
    }
    buf[i++] = (uint8_t)('0' + v % 10);
    buf[i] = 0;
    OLED_ShowString(x, y, buf, 12, 1);
}
//初始化OLED
void app_OLED_Init(void)
{
    OLED_Init();
}

//显示OLED,循环刷新屏幕
//局部刷新：静态内容(标题/信道/标签)只画一次；通道数值变化时才重绘对应进度条和百分比
void app_OLED_Show(void)
{
    static int16_t s_thr = -1, s_rol = -1, s_pit = -1, s_yaw = -1; // 上次显示的值(-1=首次)
    static uint8_t s_first = 1;                                    // 静态内容标志
    uint8_t count = 0;
    remote_data remote_snapshot; // 本帧显示快照(锁内整拷)
    int16_t imu_p, imu_r, imu_y; // 欧拉角本帧快照

    // 快照: 显示全程只读局部副本,避免与写方交错读到混合值。
    // 与写方(摇杆/按键任务)共用 joystick_mutex,锁内仅做几微秒结构体整拷即释放,
    // 后续刷屏等慢操作全部在锁外进行。imu_* 由发送任务无锁写(16位原子,不会撕裂),
    // 三轴分别读取，可能来自不同回传帧；这些值仅用于显示。
    xSemaphoreTake(joystick_mutex, portMAX_DELAY);
    remote_snapshot = remote_data_value;
    xSemaphoreGive(joystick_mutex);
    imu_p = imu_pitch;
    imu_r = imu_roll;
    imu_y = imu_yaw;

    // 静态内容只画一次：标题、信道、四个通道标签
    if (s_first)
    {
        uint8_t buff[4] = {0};

        OLED_ShowString(LINE1_BEGIN, Y0, "Mangta flight", 12, 1);
        buff[0] = (CHANNEL / 100U) % 10U + '0';
        buff[1] = (CHANNEL / 10U) % 10U + '0';
        buff[2] = CHANNEL % 10U + '0';
        OLED_ShowString(LINE2_BEGIN, Y1, buff, 12, 1);
        // 信道右侧固定标签: 飞控回传的MPU6050欧拉角(整数度, 4字符宽含符号)
        OLED_ShowString(24, Y1, "P", 12, 1);
        OLED_ShowString(60, Y1, "R", 12, 1);
        OLED_ShowString(96, Y1, "Y", 12, 1);
        OLED_ShowString(LINE3_BEGIN, Y2, "THR:", 12, 1);
        OLED_ShowString(LINE3_BEGIN2, Y2, "ROL:", 12, 1);
        OLED_ShowString(LINE4_BEGIN, Y3, "PIT:", 12, 1);
        OLED_ShowString(LINE4_BEGIN2, Y3, "YAW:", 12, 1);
        s_first = 0;
    }

    // 第二行信道右侧: 显示飞控MPU6050欧拉角(0.01° → 显示整数度, 固定4字符宽)
    {
        static int16_t s_imu_p = 0x7FFF, s_imu_r = 0x7FFF, s_imu_y = 0x7FFF;
        app_oled_show_angle(30, Y1, imu_p, &s_imu_p, 0);
        app_oled_show_angle(66, Y1, imu_r, &s_imu_r, 0);
        app_oled_show_angle(102, Y1, imu_y, &s_imu_y, 1); // yaw归一化±180°
    }

    // 第三行展示遥控数据=》THR ROL（数值变化才重绘）
    if (remote_snapshot.thr != s_thr)
    {
        s_thr = remote_snapshot.thr;
        if (s_thr > 500)
        {
            count = (s_thr - 500) / 41;
            app_oled_show_bar(BAR1_BEGIN, Y2, 12);
            app_oled_show_bar(BAR2_BEGIN, Y2, count);
            // 前一个字符涂满，后一个字符部分涂满
        }
        else if (s_thr < 500)
        {
            count = s_thr / 41;
            app_oled_show_bar(BAR1_BEGIN, Y2, count);
            app_oled_show_bar(BAR2_BEGIN, Y2, 0);
            // 前一个字符涂部分
        }
        // s_thr == 500：与相邻值显示一致，沿用原逻辑不重画

        // 油门进度条右侧显示百分比（thr 范围 0~1000，对应 0%~100%）
        {
            uint16_t pct = (uint16_t)s_thr / 10U;
            app_oled_show_thr_pct(THR_PCT_BEGIN, Y2, (uint8_t)pct);
        }
    }

    if (remote_snapshot.rol != s_rol)
    {
        s_rol = remote_snapshot.rol;
        if (s_rol > 500)
        {
            count = (s_rol - 500) / 41;
            app_oled_show_bar(BAR1_BEGIN2, Y2, 12);
            app_oled_show_bar(BAR2_BEGIN2, Y2, count);
        }
        else if (s_rol < 500)
        {
            count = s_rol / 41;
            app_oled_show_bar(BAR1_BEGIN2, Y2, count);
            app_oled_show_bar(BAR2_BEGIN2, Y2, 0);
        }
    }

    // 第四行展示遥控数据=》PIT YAW（数值变化才重绘）
    if (remote_snapshot.pit != s_pit)
    {
        s_pit = remote_snapshot.pit;
        if (s_pit > 500)
        {
            count = (s_pit - 500) / 41;
            app_oled_show_bar(BAR1_BEGIN, Y3, 12);
            app_oled_show_bar(BAR2_BEGIN, Y3, count);
        }
        else if (s_pit < 500)
        {
            count = s_pit / 41;
            app_oled_show_bar(BAR1_BEGIN, Y3, count);
            app_oled_show_bar(BAR2_BEGIN, Y3, 0);
        }
    }

    if (remote_snapshot.yaw != s_yaw)
    {
        s_yaw = remote_snapshot.yaw;
        if (s_yaw > 500)
        {
            count = (s_yaw - 500) / 41;
            app_oled_show_bar(BAR1_BEGIN2, Y3, 12);
            app_oled_show_bar(BAR2_BEGIN2, Y3, count);
        }
        else if (s_yaw < 500)
        {
            count = s_yaw / 41;
            app_oled_show_bar(BAR1_BEGIN2, Y3, count);
            app_oled_show_bar(BAR2_BEGIN2, Y3, 0);
        }
    }

    // 刷写OLED：OLED_Refresh_Gram 内部只刷新有变化的页，无变化时零 I2C 通信
    OLED_Refresh_Gram();
}
