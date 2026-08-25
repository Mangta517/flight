#include "application_recevie_data.h"
#include <string.h>
#include "application_flight.h"

uint8_t rx_buff[TX_PLOAD_WIDTH] = {0}; // 定义一个缓冲区来保存接收到的原始数据
// 遥控器连接状态
extern Remote_State remote_state;
// 飞机飞行状态
extern Flight_State flight_state;
// 解锁状态
Thr_State thr_state = FREE;
//气压数据
extern int32_t spl006_pressure;
extern int32_t spl006_temperature;
extern int32_t spl006_compensated_pressure;
//定高气压值
extern int32_t fix_height_pressure;
//发送的数据数组
uint8_t flight[TX_PLOAD_WIDTH]={0};
// 欧拉角(在飞行任务中更新,此处只读用于回传显示)
extern Euler_struct euler_angle;
// 记录一开始进入MAX状态的时间
uint32_t start_max_time = 0;
// 记录一开始进入MIN状态的时间
uint32_t start_min_time = 0;
uint8_t retry_count = 0; // 定义一个变量来记录重试次数
uint8_t application_recevie_data(void)
{
    memset(rx_buff, 0, TX_PLOAD_WIDTH);       // 清空缓冲区
    uint8_t res=Int_nRF24L01P_RxPacket(rx_buff) ;
   if(res==0)
   {
      // 修复：故障标志每次回包都按当前状态赋值，退出FAIL后必须回传0，
      // 否则一旦进过FAIL，flight[0]永远为1，遥控端蜂鸣器会一直响
      flight[0] = (flight_state == FAIL) ? 1 : 0;
      // 回传MPU6050欧拉角(放大100倍取整，遥控端OLED显示)：
      // flight[1..2]=pitch  flight[3..4]=roll  flight[5..6]=yaw (高字节在前)
      {
          int16_t p = (int16_t)(euler_angle.pitch * 100.0f);
          int16_t r = (int16_t)(euler_angle.roll  * 100.0f);
          int16_t y = (int16_t)(euler_angle.yaw   * 100.0f);
          flight[1] = (p >> 8) & 0xFF; flight[2] = p & 0xFF;
          flight[3] = (r >> 8) & 0xFF; flight[4] = r & 0xFF;
          flight[5] = (y >> 8) & 0xFF; flight[6] = y & 0xFF;
      }
      uint16_t count=5; // 修复：回包重试上限从500改为5，防止阻塞接收任务数秒导致遥控被判失联
      Int_nRF24L01P_TX_Mode(); // 切换发送模式
      while(Int_nRF24L01P_TxPacket(flight)==1&&count--); // 发送数据，直到发送成功
      {}
     Int_nRF24L01P_RX_Mode(); // 切换回接收模式
   }
    // 帧头校验
    if (rx_buff[0] != FRAME_HEADER1 || rx_buff[1] != FRAME_HEADER2 || rx_buff[2] != FRAME_HEADER3)
    {
        return 1; // 返回1表示接收失败
    }
    // 帧尾校验
    uint32_t sum = 0;
    uint32_t sum_receive = 0;
    for (int i = 0; i < TX_PLOAD_WIDTH - 4; i++)
    {
        sum += rx_buff[i];
    }
    // 高位在前
    sum_receive = rx_buff[TX_PLOAD_WIDTH - 4] << 24 | rx_buff[TX_PLOAD_WIDTH - 3] << 16 | rx_buff[TX_PLOAD_WIDTH - 2] << 8 | rx_buff[TX_PLOAD_WIDTH - 1];

    if (sum != sum_receive)
    {
        return 1; // 返回1表示接收失败
    }
    remote_data_receive.thr = (rx_buff[3] << 8) | rx_buff[4];
    remote_data_receive.yaw = (rx_buff[5] << 8) | rx_buff[6];
    remote_data_receive.pit = (rx_buff[7] << 8) | rx_buff[8];
    remote_data_receive.roll = (rx_buff[9] << 8) | rx_buff[10];
    if (rx_buff[11] == 1)
    {
        remote_data_receive.shutdown = !remote_data_receive.shutdown;
    }
    remote_data_receive.fix_height = rx_buff[12];

    //debug_printf(":%d,%d,%d,%d,%d,%d\n", remote_data_receive.yaw, remote_data_receive.thr, remote_data_receive.roll, remote_data_receive.pit, remote_data_receive.shutdown, remote_data_receive.fix_height);
    return 0; // 返回0表示接收成功
}

void application_process_connect_state(uint8_t res)
{
    // 如果返回值为0，表示接收成功，更新连接状态为已连接,此处使用的全局变量只有当前一个地方会修改，所以不需要使用互斥锁等机制来保护
    if (res == 0)
    {
        remote_state = REMOTE_CONNECTED;
        retry_count = 0; // 接收成功，重置重试次数
    }
    else if (res == 1) // 如果返回值为1，表示接收失败，更新连接状态为未连接
    {
        retry_count++;                      // 增加重试次数
        if (retry_count >= MAX_RETRY_COUNT) // 如果重试次数超过最大值，认为连接断开
        {
            remote_state = REMOTE_DISCONNECTED;
            flight_state = FAIL; // 连接断开时将飞行状态设置为IDLE
            retry_count = 0;     // 重置重试次数
        }
    }
}

// 处理解锁逻辑,返回0表示解锁成功，返回1表示解锁失败
static uint8_t application_process_unlock(void)
{
    // 考虑安全问题，解锁状态后油门应该为0
    // 漏洞修复：未连接（失联）时禁止解锁，并打断进行中的解锁序列，防止基于残留数据自动解锁
    if (remote_state != REMOTE_CONNECTED)
    {
        thr_state = FREE;
        return 1;
    }
    switch (thr_state)
    {
    case FREE:
        if (remote_data_receive.thr >= 900)
        {
            thr_state = MAX;
            // 记录一开始进入MAX状态的时间
            start_max_time = xTaskGetTickCount();
        }
        break;
    case MAX:
        if (remote_data_receive.thr < 900)
        { // 判断油门保持最大值是否超过1秒
            if (xTaskGetTickCount() - start_max_time >= pdMS_TO_TICKS(1000))
            {
                // 超过1秒，进入LEAVE_MAX状态
                thr_state = LEAVE_MAX;
            }
            else
            { 
                // 未超过1秒，回到FREE
                thr_state = FREE;
            }
        }
        break;
    case LEAVE_MAX:
        if (remote_data_receive.thr < 100)
        {
            thr_state = MIN;
            // 记录一开始进入MIN状态的时间
            start_min_time = xTaskGetTickCount();
        }
        break;
    case MIN:
        // 进入MIN状态后，油门保持在100以下，超过1秒后进入UNLOCK状态
        if (xTaskGetTickCount() - start_min_time <= pdMS_TO_TICKS(1000))
        {
            // 如果，没有保持在最低限度就重头开始
            if (remote_data_receive.thr > 100)
            {
                thr_state = FREE;
            }
        }
        else
        {
            // 漏洞修复：计时到期时再校验一次油门确实在低位，防止油门恰好在临界点抬起仍解锁
            if (remote_data_receive.thr < 100)
            {
                thr_state = UNLOCK;
            }
            else
            {
                thr_state = FREE;
            }
        }
        break;
    case UNLOCK:
        break;
    default:
        break;
    }
       
    if (thr_state == UNLOCK)
    {
        return 0;
    }
    return 1;

}

void application_process_flight_state(void)
{
    // 通过轮询判断飞行状态
    switch (flight_state)
    {
    case IDLE:
        // 判断解锁状态
        if (application_process_unlock() == 0)
        {
            app_flight_pid_reset();  // 清零PID积分，防止带入上次飞行的残留
            flight_state = NORMAL;
            //解锁成功后需要重置状态
            thr_state = FREE;

        }
        break;
    case NORMAL:
        // 判断定高状态
        if (remote_data_receive.fix_height == 1)
        {
            flight_state = FIX_HEIGHT;
            remote_data_receive.fix_height = 0;

            //记录进入定高状态时的气压值
            fix_height_pressure = spl_process_pressure(spl006_pressure, spl006_temperature, &spl006_compensated_pressure);
        }
        else if (remote_state == REMOTE_DISCONNECTED) // 判断故障状态
        {
            flight_state = FAIL;
        }
        break;
    case FIX_HEIGHT:
        // 解除定高状态
        if (remote_data_receive.fix_height == 1)
        {
            flight_state = NORMAL;
            remote_data_receive.fix_height = 0;
        }
        else if (remote_state == REMOTE_DISCONNECTED) // 判断失联状态
        {
            flight_state = FAIL;
        }
        break;
    case FAIL:
        // 故障时直接回空闲，重置PID防止残留积分
        app_flight_pid_reset();
        flight_state = IDLE;
        break;

    default:
        break;
    }
}
