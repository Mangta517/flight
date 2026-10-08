#ifndef __MY_MPU6050_H__

#define __MY_MPU6050_H__

#include "main.h"
#include "i2c.h"
#include "common_config.h"
#include "FreeRTOS.h"
#include "task.h"
#include "stdlib.h"

//从设备地址
#define MPU6050_ADDR 0x68
// 写地址
#define MPU6050_ADDR_WRITE 0xD0
// 读地址
#define MPU6050_ADDR_READ 0xD1
// 采样率分频寄存器
#define MPU6050_SMPLRT_DIV 0x19

// 配置寄存器
#define MPU6050_CONFIG 0x1A

// 陀螺仪配置寄存器
#define MPU6050_GYRO_CONFIG 0x1B

// 加速度计配置寄存器
#define MPU6050_ACCEL_CONFIG 0x1C

// 运动检测阈值
#define MPU6050_MOT_THR 0x1F

// FIFO 使能
#define MPU6050_FIFO_EN 0x23

// I2C 主机控制
#define MPU6050_I2C_MST_CTRL 0x24

// 中断引脚配置
#define MPU6050_INT_PIN_CFG 0x37

// 中断使能
#define MPU6050_INT_ENABLE 0x38

// 中断状态
#define MPU6050_INT_STATUS 0x3A

// 加速度计输出寄存器
#define MPU6050_ACCEL_XOUT_H 0x3B
#define MPU6050_ACCEL_XOUT_L 0x3C
#define MPU6050_ACCEL_YOUT_H 0x3D
#define MPU6050_ACCEL_YOUT_L 0x3E
#define MPU6050_ACCEL_ZOUT_H 0x3F
#define MPU6050_ACCEL_ZOUT_L 0x40

// 温度输出寄存器
#define MPU6050_TEMP_OUT_H 0x41
#define MPU6050_TEMP_OUT_L 0x42

// 陀螺仪输出寄存器
#define MPU6050_GYRO_XOUT_H 0x43
#define MPU6050_GYRO_XOUT_L 0x44
#define MPU6050_GYRO_YOUT_H 0x45
#define MPU6050_GYRO_YOUT_L 0x46
#define MPU6050_GYRO_ZOUT_H 0x47
#define MPU6050_GYRO_ZOUT_L 0x48

// 信号路径复位
#define MPU6050_SIGNAL_PATH_RESET 0x68

// 用户控制
#define MPU6050_USER_CTRL 0x6A

// 电源管理 1
#define MPU6050_PWR_MGMT_1 0x6B

// 电源管理 2
#define MPU6050_PWR_MGMT_2 0x6C

// FIFO 计数寄存器
#define MPU6050_FIFO_COUNTH 0x72
#define MPU6050_FIFO_COUNTL 0x73

// FIFO 读写寄存器
#define MPU6050_FIFO_R_W 0x74

// 设备 ID 寄存器（默认值 0x68）
#define MPU6050_WHO_AM_I 0x75

// *****************************************
// 常用配置宏（方便直接配置量程）
// *****************************************
// 陀螺仪量程
#define MPU6050_GYRO_RANGE_250DPS  0x00
#define MPU6050_GYRO_RANGE_500DPS  0x08
#define MPU6050_GYRO_RANGE_1000DPS 0x10
#define MPU6050_GYRO_RANGE_2000DPS 0x18

// 加速度计量程
#define MPU6050_ACCEL_RANGE_2G  0x00
#define MPU6050_ACCEL_RANGE_4G  0x08
#define MPU6050_ACCEL_RANGE_8G  0x10
#define MPU6050_ACCEL_RANGE_16G 0x18

// 常用寄存器
//电源管理寄存器1
#define MPU6050_REG_PWR_MGMT_1 0x6B

void my_mpu6050_init(void);
//零偏校准：要求飞机静止时调用(阻塞)。不再于上电初始化时自动执行，
//改由飞行任务在遥控器连接成功(REMOTE_CONNECTED)后触发一次
void my_mpu6050_zero_offset(void);
//静止时增量微调陀螺仪零偏：把当前角速度读数当作残余零偏累加进offset，
//用于消除yaw积分零漂(偏航无绝对参考，只能靠陀螺仪零点准)
void my_mpu6050_gyro_trim(int16_t gyro_x, int16_t gyro_y, int16_t gyro_z);
void my_mpu6050_write_reg(uint8_t reg, uint8_t data);
void my_mpu6050_read_reg(uint8_t reg, uint8_t *data);
//读取三轴角速度
void my_mpu6050_get_gyro(Gyro_struct *gyro);
//读取三轴加速度
void my_mpu6050_get_accel(Accel_struct *accel);
// 读取所有数据
void my_mpu6050_get_data(Accel_Gyro_struct *accel_gyro);

#endif
