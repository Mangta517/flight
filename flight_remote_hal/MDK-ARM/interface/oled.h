#ifndef __OLED_H
#define __OLED_H

#include "main.h"

/*
 * OLED(SSD1306) 通过硬件 I2C1 驱动:
 *   I2C1_SCL = PB8, I2C1_SDA = PB9 (I2C1 重映射)
 *   引脚与 I2C1 外设初始化见 Core/Src/i2c.c (MX_I2C1_Init / HAL_I2C_MspInit)
 */

/* I2C 从机地址 */
#define OLED_I2C_ADDR  0x78   // 0x3C << 1 | 写位

/* 控制字节 */
#define OLED_CTRL_CMD   0x00  // 命令控制字节
#define OLED_CTRL_DATA  0x40  // 数据控制字节

#define OLED_CMD   0
#define OLED_DATA  1

/* 中文字库参数：每个半字 12 字节 */
#define OLED_CH_BYTES_PER_HALF  12

/* OLED 控制函数 */
void OLED_WR_Byte(uint8_t data, uint8_t cmd);
void OLED_Display_On(void);
void OLED_Display_Off(void);
void OLED_Refresh_Gram(void);
void OLED_Init(void);
void OLED_Clear(void);
void OLED_DrawPoint(uint8_t x, uint8_t y, uint8_t t);
void OLED_ShowChar(uint8_t x, uint8_t y, uint8_t chr, uint8_t size, uint8_t mode);
void OLED_ShowNumber(uint8_t x, uint8_t y, uint32_t num, uint8_t len, uint8_t size);
void OLED_ShowString(uint8_t x, uint8_t y, const uint8_t *p, uint8_t size, uint8_t mode);
void OLED_ShowCH(uint8_t x, uint8_t y, uint8_t chr, uint8_t size, uint8_t mode);
void OLED_Show_CH(uint8_t x, uint8_t y, uint8_t chr, uint8_t size, uint8_t mode);
void OLED_Show_CH_String(uint8_t x, uint8_t y, const uint8_t *p, uint8_t size, uint8_t mode);

#endif
