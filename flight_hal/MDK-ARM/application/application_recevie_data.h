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
// 原值5(≈50ms)对遥控器"发→等回复(20ms)→再发"的节奏过紧, 推油门时易误判失联闪故障灯。
// 放宽到8(≈80ms), 留足余量; 仍能在数秒内可靠识别真正失联。
#define REMOTE_TIMEOUT_MS 80
//

typedef struct
{
    int16_t thr; // 油门
    int16_t yaw; // 航向
    int16_t pit; // 俯仰
    int16_t roll; // 横滚
    uint8_t shutdown; // 关机开关,1关机，0开机
    uint8_t fix_height; // 定高开关，1切换定高开关状态，0不切换
} remote_data;

extern Remote_State remote_state;
extern Flight_State flight_state;
extern volatile remote_data remote_data_receive;
/* ��������ֻ��������֡�����������ڶ��ٽ���ȡ��һ���������ڵĿ��ա� */
typedef struct
{
    remote_data control;
    TickType_t last_valid_tick;
    uint8_t seen;
} remote_snapshot_struct;

uint8_t application_recevie_data(void);
void application_remote_snapshot(remote_snapshot_struct *snapshot);
void application_process_connect_state(const remote_snapshot_struct *snapshot);
void application_process_flight_state(const remote_snapshot_struct *snapshot, uint8_t can_arm);
#endif
