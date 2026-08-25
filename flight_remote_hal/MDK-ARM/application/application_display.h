#ifndef __APPLICATION_DISPLAY_H__
#define __APPLICATION_DISPLAY_H__

#include "oled.h"
#include "oledfont.h"
#include "main.h"
#include "nRF24L01P.h"
#include "application_process_data.h"


#define LINE1_BEGIN 28
#define LINE2_BEGIN 5
#define LINE3_BEGIN 5
#define LINE3_BEGIN2 79
#define LINE4_BEGIN 5
#define LINE4_BEGIN2 79



#define BAR1_BEGIN 35
#define BAR2_BEGIN 47
#define BAR1_BEGIN2 104
#define BAR2_BEGIN2 116

/* 油门百分比显示起始位置（紧跟油门进度条右侧） */
#define THR_PCT_BEGIN 61


#define Y0 0
#define Y1 14
#define Y2 26
#define Y3 38
//初始化OLED
void app_OLED_Init(void);

//显示OLED,循环刷新屏幕
void app_OLED_Show(void);

#endif



