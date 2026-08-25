#include "joystick.h"

uint16_t adc_buf[4]={0}; // 存储ADC转换结果的数组

void Joystick_Init(void)
{
    // ADC初始化
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_buf, 4); // 启动ADC并使用DMA将数据存储到adc_buf数组中
}


void Joystick_GetValue(Joystick_struct *joystick_value)
{
//DMA转运不依赖CPU的计算，数据实时保存在adc_buf中
joystick_value->thr=adc_buf[0]; // 油门
joystick_value->yaw=adc_buf[1]; // 偏航
joystick_value->pit=adc_buf[2]; // 俯仰
joystick_value->rol=adc_buf[3]; // 横滚



}

