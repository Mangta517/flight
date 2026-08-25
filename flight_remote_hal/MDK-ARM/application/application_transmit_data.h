#ifndef __APPLICATION_TRANSMIT_DATA_H__
#define __APPLICATION_TRANSMIT_DATA_H__ 

#include "main.h"
#include "nRF24L01P.h"
#include "application_process_data.h"
#include "FreeRTOS.h"
#include "task.h"
//定义帧头校验的值
#define FRAME_HEADER1 'h'
#define FRAME_HEADER2 'y'
#define FRAME_HEADER3 'f'

extern remote_data remote_data_value;        // 遥控数据结构体
//自动切换nrf24r1的发送模式，将采集完成的遥控数据打包发送
void application_transmit_data(void);
#endif


