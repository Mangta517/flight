#ifndef __APPLICATION_RECEVIE_DATA_H__
#define __APPLICATION_RECEVIE_DATA_H__

#include "nRF24L01P.h"
#include "common_debug.h"
#include "common_config.h"
#include <string.h>
#include "spl006.h"

#define FRAME_HEADER1 'h'
#define FRAME_HEADER2 'y'
#define FRAME_HEADER3 'f'

//最大重试次数
// 连续 8 次接收失败后判定失联；接收任务每轮结束延时 10 tick（当前为 10ms）。
// 实际判定耗时还包含接收、回传重试和调度耗时，不能直接等同于固定 80ms。
#define MAX_RETRY_COUNT 8
//

typedef struct
{
    int16_t thr;        // 油门
    int16_t yaw;        // 航向
    int16_t pit;        // 俯仰
    int16_t roll;       // 横滚
    uint8_t shutdown;   // 关机开关,1关机，0开机
    uint8_t fix_height; // 定高开关，1切换定高开关状态，0不切换
} remote_data;

extern Remote_State remote_state;
extern Flight_State flight_state;
extern volatile remote_data remote_data_receive;
//接收遥控器发送的数据，再解析为结构体,如果返回0，表示接收成功，返回1表示接收失败
uint8_t application_recevie_data(void);
//处理连接状态,res是上一次连接状态的返回值
void application_process_connect_state(uint8_t res); // 处理接收到的遥控器数据，根据遥控器状态更新连接状态

void application_process_flight_state(void); //处理飞行状态
#endif
