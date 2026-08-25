#ifndef __SPL006_H__
#define __SPL006_H__

#include "i2c.h"
#include "FreeRTOS.h"
#include "task.h"
#include "common_debug.h"
/* I2C 地址 */
#define SPL06_I2C_ADDR_HIGH    0x77  // SDO 引脚拉高（默认）
#define SPL06_I2C_ADDR_LOW     0x76  // SDO 引脚拉低
#define SPL006_ADDR_WRITE     0xEE  // 写地址 (SPL06_I2C_ADDR << 1 | 0)
#define SPL006_ADDR_READ      0xEF  // 读地址 (SPL06
/* 寄存器地址 */
#define SPL06_PRS_B2           0x00  // 压力数据高字节 (23:16)
#define SPL06_PRS_B1           0x01  // 压力数据中字节 (15:8)
#define SPL06_PRS_B0           0x02  // 压力数据低字节 (7:0)
#define SPL06_TMP_B2           0x03  // 温度数据高字节 (23:16)
#define SPL06_TMP_B1           0x04  // 温度数据中字节 (15:8)
#define SPL06_TMP_B0           0x05  // 温度数据低字节 (7:0)
#define SPL06_PRS_CFG          0x06  // 压力配置             只需要配置压力传感器来通过写寄存器的bit0-bit3为0100设置采样率16次为标准模式，写寄存器的bit4-bit6为110设置采样率64次，若过采样率 >8倍，需在 CFG_REG（0x09）中设置 P_SHIFT=1 和 T_SHIFT=1
#define SPL06_TMP_CFG          0x07  // 温度配置
#define SPL06_MEAS_CFG         0x08  // 测量配置与状态      先延时12ms，通过检测bit6来判断传感器是否准备就绪，bit4置1表示为压力，bit0-bit2设置为101配置为背景模式持续测量压力，等待40ms后bit7置1表示为校准系数就绪
#define SPL06_CFG_REG          0x09  // 中断与 FIFO 配置
#define SPL06_INT_STS          0x0A  // 中断状态
#define SPL06_FIFO_STS         0x0B  // FIFO 状态
#define SPL06_RESET            0x0C  // 软复位 / FIFO 刷新
#define SPL06_ID               0x0D  // 产品 ID 与版本
#define SPL06_COEF_START       0x10  // 校准系数起始地址 (0x10~0x21)
#define SPL06_COEF_END         0x21

//如果需要获得标准的数据，需要先判断MEAS_CFG(0x08)的bit6和bit4和bit7是否为1，如果为1则表示传感器准备就绪和压力数据和校准系数就绪，在PRS_B2(0x00)、PRS_B1(0x01)、PRS_B0(0x02)中读取压力数据
//也要读取压力的校准系数从0x13到0x21用来计算标准的数据

/* Praw_sc = Praw / kP（kP根据过采样率查表）
Pcomp(Pa) = c00 + Praw_sc*(c10 + Praw_sc*(c20 + Praw_sc*c30)) 
            + Traw_sc*c01 + Traw_sc*Praw_sc*(c11 + Praw_sc*c21)
            kp=253952
            praw=从压力传感器读出来的数据
 */
/* PRS_CFG (0x06) 位定义 */
#define SPL06_PM_RATE_MASK     0x70  // bit6:4
#define SPL06_PM_RATE_1PS      0x00  // 000: 1 次/秒
#define SPL06_PM_RATE_2PS      0x10  // 001: 2 次/秒
#define SPL06_PM_RATE_4PS      0x20  // 010: 4 次/秒
#define SPL06_PM_RATE_8PS      0x30  // 011: 8 次/秒
#define SPL06_PM_RATE_16PS     0x40  // 100: 16 次/秒
#define SPL06_PM_RATE_32PS     0x50  // 101: 32 次/秒
#define SPL06_PM_RATE_64PS     0x60  // 110: 64 次/秒
#define SPL06_PM_RATE_128PS    0x70  // 111: 128 次/秒

#define SPL06_PM_PRC_MASK      0x0F  // bit3:0
#define SPL06_PM_PRC_SINGLE    0x00  // 0000: 单次
#define SPL06_PM_PRC_2X        0x01  // 0001: 2倍（低功耗）
#define SPL06_PM_PRC_4X        0x02  // 0010: 4倍
#define SPL06_PM_PRC_8X        0x03  // 0011: 8倍
#define SPL06_PM_PRC_16X       0x04  // 0100: 16倍（标准）
#define SPL06_PM_PRC_32X       0x05  // 0101: 32倍
#define SPL06_PM_PRC_64X       0x06  // 0110: 64倍（高精度）
#define SPL06_PM_PRC_128X      0x07  // 0111: 128倍

/* TMP_CFG (0x07) 位定义 */
#define SPL06_TMP_EXT          0x80  // bit7: 温度源 (1=MEMS内)
#define SPL06_TMP_RATE_MASK    0x70  // bit6:4
// 速率取值同 PM_RATE_xxx
#define SPL06_TMP_PRC_MASK     0x07  // bit2:0
#define SPL06_TMP_PRC_SINGLE   0x00  // 000: 单次
#define SPL06_TMP_PRC_2X       0x01  // 2倍
#define SPL06_TMP_PRC_4X       0x02  // 4倍
#define SPL06_TMP_PRC_8X       0x03  // 8倍
#define SPL06_TMP_PRC_16X      0x04  // 16倍
#define SPL06_TMP_PRC_32X      0x05  // 32倍
#define SPL06_TMP_PRC_64X      0x06  // 64倍
#define SPL06_TMP_PRC_128X     0x07  // 128倍

/* MEAS_CFG (0x08) 位定义 */
#define SPL06_COEF_RDY         0x80  // bit7: 系数就绪
#define SPL06_SENSOR_RDY       0x40  // bit6: 传感器就绪
#define SPL06_TMP_RDY          0x20  // bit5: 温度新数据
#define SPL06_PRS_RDY          0x10  // bit4: 压力新数据
#define SPL06_MEAS_CTRL_MASK   0x07  // bit2:0
#define SPL06_MEAS_STANDBY     0x00  // 待机
#define SPL06_MEAS_CMD_PRS     0x01  // 单次压力
#define SPL06_MEAS_CMD_TMP     0x02  // 单次温度
#define SPL06_MEAS_CMD_BOTH    0x03  // 单次压力+温度
#define SPL06_MEAS_BG_PRSTMP   0x07  // 背景模式 (压力+温度)
#define SPL06_MEAS_BG_PRS      0x05  // 背景模式 (仅压力)
#define SPL06_MEAS_BG_TMP      0x06  // 背景模式 (仅温度)

/* CFG_REG (0x09) 位定义 */
#define SPL06_INT_HL           0x80  // bit7: 中断极性 (1=高有效)
#define SPL06_INT_FIFO         0x40  // bit6: FIFO满中断
#define SPL06_INT_PRS          0x20  // bit5: 压力就绪中断
#define SPL06_INT_TMP          0x10  // bit4: 温度就绪中断
#define SPL06_T_SHIFT          0x08  // bit3: 温度右移 (过采样>8倍时置1)
#define SPL06_P_SHIFT          0x04  // bit2: 压力右移 (过采样>8倍时置1)
#define SPL06_FIFO_EN          0x02  // bit1: FIFO使能
#define SPL06_SPI_3WIRE        0x01  // bit0: SPI 3线模式 (0=4线)

/* INT_STS (0x0A) 位定义 */
#define SPL06_INT_FIFO_FULL    0x04  // bit2: FIFO满中断标志
#define SPL06_INT_TMP_FLAG     0x02  // bit1: 温度就绪标志
#define SPL06_INT_PRS_FLAG     0x01  // bit0: 压力就绪标志

/* FIFO_STS (0x0B) 位定义 */
#define SPL06_FIFO_FULL        0x04  // bit2: FIFO已满
#define SPL06_FIFO_EMPTY       0x02  // bit1: FIFO为空

/* RESET (0x0C) 命令 */
#define SPL06_FIFO_FLUSH       0x80  // bit7: 写1清空FIFO
#define SPL06_SOFT_RESET       0x09  // 写0x09产生软复位 (低4位1001)

/* 补偿用的缩放因子 (根据过采样率选择) */
// 定义查表值，实际使用时根据 PM_PRC / TMP_PRC 选择
// 可定义为数组或常量
#define SPL06_SCALE_FACTOR_1X      524288
#define SPL06_SCALE_FACTOR_2X      1572864
#define SPL06_SCALE_FACTOR_4X      3670016
#define SPL06_SCALE_FACTOR_8X      7864320
#define SPL06_SCALE_FACTOR_16X     253952
#define SPL06_SCALE_FACTOR_32X     516096
#define SPL06_SCALE_FACTOR_64X     1040384
#define SPL06_SCALE_FACTOR_128X    2088960
/* 压力校准系数相关寄存器地址（按手册表10定义） */
/* 注意：部分地址与温度系数 c0/c1 共享，此处仅列出压力需要用到的寄存器字节地址 */

/* c00 系数 (20位) 分布在三个字节 */
#define SPL06_C00_BYTE2      0x13  // c00[19:12]
#define SPL06_C00_BYTE1      0x14  // c00[11:4]
#define SPL06_C00_BYTE0      0x15  // c00[3:0]  (寄存器高4位)

/* c10 系数 (20位) 跨三个字节，与 c00 共用 0x15 */
#define SPL06_C10_BYTE2      0x15  // c10[19:16] (寄存器低4位)
#define SPL06_C10_BYTE1      0x16  // c10[15:8]
#define SPL06_C10_BYTE0      0x17  // c10[7:0]

/* c01 系数 (16位) */
#define SPL06_C01_BYTE1      0x18  // c01[15:8]
#define SPL06_C01_BYTE0      0x19  // c01[7:0]

/* c11 系数 (16位) */
#define SPL06_C11_BYTE1      0x1A  // c11[15:8]
#define SPL06_C11_BYTE0      0x1B  // c11[7:0]

/* c20 系数 (16位) */
#define SPL06_C20_BYTE1      0x1C  // c20[15:8]
#define SPL06_C20_BYTE0      0x1D  // c20[7:0]

/* c21 系数 (16位) */
#define SPL06_C21_BYTE1      0x1E  // c21[15:8]
#define SPL06_C21_BYTE0      0x1F  // c21[7:0]

/* c30 系数 (16位) */
#define SPL06_C30_BYTE1      0x20  // c30[15:8]
#define SPL06_C30_BYTE0      0x21  // c30[7:0]

/* 如果希望一次性读取所有系数，可以使用连续地址起始 */
#define SPL06_COEF_START     0x10  // 系数区起始地址 (共18字节, 0x10~0x21)
#define SPL06_COEF_LEN       18



void spl_Init(void);
int spl_read_pressure(int32_t *pressure);
int spl_read_temperature(int32_t *temperature);
int spl_process_pressure(int32_t raw_pressure, int32_t raw_temperature, int32_t *compensated_pressure);
// 周期采样气压/温度到全局缓存(由飞行任务调用)
void spl_update_data(void);

#endif /* SPL06_001_H */





