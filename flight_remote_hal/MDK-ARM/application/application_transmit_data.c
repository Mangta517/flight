#include "application_transmit_data.h"


uint8_t transmit_buf[TX_PLOAD_WIDTH] = {0};        // 32 bytes payload width for TX
uint8_t post_buff[TX_PLOAD_WIDTH] = {0};        // 32 bytes payload width for post-processing
uint8_t remote_connected = 0;        // 连接状态：收到过飞控回复即为连接成功
// 飞控回传的MPU6050欧拉角(单位0.01°，供OLED显示)
int16_t imu_pitch = 0;
int16_t imu_roll = 0;
int16_t imu_yaw = 0;
void application_transmit_data(void)
{
     //使用nRF24L01P进行遥控数据的发送
    //1.先进入发送
    Int_nRF24L01P_TX_Mode();
    uint32_t sum=0;
    //2.发送数据,保证数据唯一性和可靠性,1.帧头检验2.帧尾检验3.重发机制
    //数据本身10字节，帧头检验3字节，帧尾检验和4字节
    transmit_buf[0] = FRAME_HEADER1;
    transmit_buf[1] = FRAME_HEADER2;
    transmit_buf[2] = FRAME_HEADER3;
    //高8位在前
    transmit_buf[3] = (remote_data_value.thr >> 8) & 0xFF;
    transmit_buf[4] = remote_data_value.thr & 0xFF;
   
    transmit_buf[5] = (remote_data_value.yaw >> 8) & 0xFF;
    transmit_buf[6] = remote_data_value.yaw & 0xFF;

    transmit_buf[7] = (remote_data_value.pit >> 8) & 0xFF;
    transmit_buf[8] = remote_data_value.pit & 0xFF;

    transmit_buf[9] = (remote_data_value.rol >> 8) & 0xFF;
    transmit_buf[10] = remote_data_value.rol & 0xFF;

    taskENTER_CRITICAL();
    transmit_buf[11] = remote_data_value.shutdown; // 关机切换命令
    transmit_buf[12] = remote_data_value.fix_high; // 定高开关
    remote_data_value.shutdown = 0; // 发送完成后清零，避免持续发送同一个关机命令
    remote_data_value.fix_high=0; // 发送完成后重置定高开关状态
    taskEXIT_CRITICAL();

    for(int i=0;i<TX_PLOAD_WIDTH-4;i++)
    {
        sum+=transmit_buf[i];
    }
    //帧尾校验帧，高位在前
    transmit_buf[TX_PLOAD_WIDTH-4] = (sum >> 24) & 0xFF;
    transmit_buf[TX_PLOAD_WIDTH-3] = (sum >> 16) & 0xFF;
    transmit_buf[TX_PLOAD_WIDTH-2] = (sum >> 8) & 0xFF;
    transmit_buf[TX_PLOAD_WIDTH-1] = sum & 0xFF;

    //debug_printf(":%d,%d,%d,%d,%d,%d\n", remote_data_value.yaw, remote_data_value.thr, remote_data_value.rol, remote_data_value.pit, remote_data_value.shutdown, remote_data_value.fix_high);
    uint8_t res=Int_nRF24L01P_TxPacket(transmit_buf);
    //3.进入接收模式，等待飞控回复（带超时，防止飞控回复慢/丢失时最高优先级任务死循环卡死遥控）
    if(res==0)
    {
        Int_nRF24L01P_RX_Mode();
        uint32_t wait_start = HAL_GetTick();
        uint8_t got_reply = 0;
        while (got_reply == 0)
        {
            if (Int_nRF24L01P_RxPacket(post_buff) == 0)
            {
                got_reply = 1; // 收到飞控回复
            }
            else if ((HAL_GetTick() - wait_start) >= 20U)
            {
                break;         // 超时未收到回复，继续下一轮发送，不阻塞任务
            }
        }
        if (got_reply)
        {
            remote_connected = 1; // 成功收到飞控回复，连接成功
            // 解析飞控回传的MPU6050欧拉角(放大100倍,高字节在前)
            imu_pitch = (int16_t)((post_buff[1] << 8) | post_buff[2]);
            imu_roll  = (int16_t)((post_buff[3] << 8) | post_buff[4]);
            imu_yaw   = (int16_t)((post_buff[5] << 8) | post_buff[6]);
            // 仅在故障标志变化时打印一次，便于观察飞控状态而不刷屏
            static uint8_t last_fault = 0xFF;
            if (post_buff[0] != last_fault)
            {
                last_fault = post_buff[0];
                debug_printf("post buff:%d\n", post_buff[0]);
            }
        }
        Int_nRF24L01P_RX_Mode();
    } 
   

    //10ms执行一次遥控任务
}


