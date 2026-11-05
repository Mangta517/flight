#include "my_mpu6050.h"
#include "common_debug.h"

Gyro_struct gyro_data = {0};
Accel_struct accel_data = {0};

int16_t offset_accel_x;
int16_t offset_accel_y;
int16_t offset_accel_z;

int16_t offset_gyro_x;
int16_t offset_gyro_y;
int16_t offset_gyro_z;
//写寄存器（仅初始化使用；I2C1 总线由飞行任务单所有权访问，无需临界区）
uint8_t my_mpu6050_write_reg(uint8_t reg, uint8_t data)
{
    if (HAL_I2C_Mem_Write(&hi2c1, MPU6050_ADDR_WRITE, reg, I2C_MEMADD_SIZE_8BIT, &data, 1, 5) != HAL_OK)
    {
        return 1;
    }
    return 0;
}

//读单个寄存器（仅初始化使用）
uint8_t my_mpu6050_read_reg(uint8_t reg, uint8_t *data)
{
    if (HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDR_READ, reg, I2C_MEMADD_SIZE_8BIT, data, 1, 5) != HAL_OK)
    {
        return 1;
    }
    return 0;
}


//在初始化之后进行零偏校准
uint8_t my_mpu6050_zero_offset(void)
{
    // 在静止且水平时进行零偏校准
    // 静止判定: 两次加速度值相减小于300，连续100次
    // 水平判定: |Accel_x|<200 且 |Accel_y|<200 且 |Accel_z-16384|<200(约±0.7°)
    //           原阈值800(±2.8°)太宽，1~2°的微坡面会被当水平基准，起飞后整机倾斜前漂
    // 超时保护: 约3s仍未达标则放弃本次校准，保留原偏移量，避免飞行任务被永久阻塞
    Accel_struct current_accel = {0};
    Accel_struct last_accel = {0};
    uint8_t count = 0;
    uint32_t attempts = 0;
    const uint32_t CALIB_TIMEOUT = 500; // 500次 × 6ms ≈ 3s
    if (my_mpu6050_get_accel(&last_accel) != 0)
    {
        return 1;
    }
    while (count < 100)
    {
        if (my_mpu6050_get_accel(&current_accel) != 0)
        {
            return 1;
        }
        //如果误差判断的值过小，则无法进行零偏校准
        if (abs(current_accel.Accel_x - last_accel.Accel_x) < 300 && abs(current_accel.Accel_y - last_accel.Accel_y) < 300 && abs(current_accel.Accel_z - last_accel.Accel_z) < 300
            && abs(current_accel.Accel_x) < 200 && abs(current_accel.Accel_y) < 200 && abs(current_accel.Accel_z - 16384) < 200)
        {
            count++;
        }
        else
        {
            count = 0;
        }
        last_accel = current_accel;
        vTaskDelay(pdMS_TO_TICKS(6));
        if (++attempts >= CALIB_TIMEOUT)
        {
            debug_printf("MPU6050校准失败: 请将飞机放平并保持静止后重新连接遥控器\n");
            return 1; // 放弃本次校准，偏移量保持原值
        }
    }

    Accel_Gyro_struct accel_gyro_data = {0};
    int32_t offset_sum_x = 0;
    int32_t offset_sum_y = 0;
    int32_t offset_sum_z = 0;

    int32_t offset_sum_x_gyro = 0;
    int32_t offset_sum_y_gyro = 0;
    int32_t offset_sum_z_gyro = 0;
    for (uint8_t i = 0; i < 100; i++)
    {
        // 重新读取加速度
        if (my_mpu6050_get_accel(&accel_gyro_data.Accel) != 0)
        {
            return 1;
        }
        offset_sum_x += accel_gyro_data.Accel.Accel_x;
        offset_sum_y += accel_gyro_data.Accel.Accel_y;
        // Z轴原本有1G重力，所以减去16384就是重力加速度
        offset_sum_z += (accel_gyro_data.Accel.Accel_z - 16384);

        if (my_mpu6050_get_gyro(&accel_gyro_data.Gyro) != 0)
        {
            return 1;
        }
        offset_sum_x_gyro += accel_gyro_data.Gyro.Gyro_x;
        offset_sum_y_gyro += accel_gyro_data.Gyro.Gyro_y;
        offset_sum_z_gyro += accel_gyro_data.Gyro.Gyro_z;
    }
    offset_accel_x += offset_sum_x / 100;
    offset_accel_y += offset_sum_y / 100;
    offset_accel_z += offset_sum_z / 100;
    offset_gyro_x += offset_sum_x_gyro / 100;
    offset_gyro_y += offset_sum_y_gyro / 100;
    offset_gyro_z += offset_sum_z_gyro / 100;
    return 0;
}
//静止时增量微调陀螺仪零偏：把当前角速度读数当作残余零偏累加进offset，
//用于消除yaw积分零漂(偏航无绝对参考，只能靠陀螺仪零点准)
//注意: 仅在确认飞机静止时调用，且每次只累加当前残余(通常很小)，多次收敛
void my_mpu6050_gyro_trim(int16_t gyro_x, int16_t gyro_y, int16_t gyro_z)
{
    offset_gyro_x += gyro_x;
    offset_gyro_y += gyro_y;
    offset_gyro_z += gyro_z;
}

//初始化mpu6050
uint8_t my_mpu6050_init(void)
{
    uint8_t data = 0;
    if (my_mpu6050_write_reg(MPU6050_REG_PWR_MGMT_1, 0x80) != 0)
    {
        return 1;
    }
    /* ��λ�ȴ���� 100 ms��I2C ʧ�������ϱ�����ֹ���޵ȴ��� */
    uint8_t reset_ready = 0;
    for (uint8_t attempt = 0; attempt < 100; attempt++)
    {
        if (my_mpu6050_read_reg(MPU6050_REG_PWR_MGMT_1, &data) != 0)
        {
            return 1;
        }
        if (data == 0x40)
        {
            reset_ready = 1;
            break;
        }
        HAL_Delay(1);
    }
    if (!reset_ready)
    {
        return 1;
    }
    const uint8_t config[][2] = {{0x6B, 0x00}, {0x1B, 0x18}, {0x1C, 0x00}, {0x38, 0x00},
        {0x6A, 0x00}, {0x19, 0x01}, {0x1A, 0x01}, {0x6B, 0x01}, {0x6C, 0x00}};
    for (uint8_t i = 0; i < sizeof(config) / sizeof(config[0]); i++)
    {
        if (my_mpu6050_write_reg(config[i][0], config[i][1]) != 0)
        {
            return 1;
        }
    }
    return 0;
}

//需要进行零偏处理
uint8_t my_mpu6050_get_gyro(Gyro_struct *gyro_data)
{
    uint8_t buf[6];
    // 一次突发读6字节(0x43~0x48)，替代6次单字节读，大幅缩短总线占用
    if (HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDR_READ, MPU6050_GYRO_XOUT_H,
                         I2C_MEMADD_SIZE_8BIT, buf, 6, 5) != HAL_OK)
    {
        return 1;
    }
    gyro_data->Gyro_x = ((buf[0] << 8) | buf[1]) - offset_gyro_x;
    gyro_data->Gyro_y = ((buf[2] << 8) | buf[3]) - offset_gyro_y;
    gyro_data->Gyro_z = ((buf[4] << 8) | buf[5]) - offset_gyro_z;
    return 0;
}
//抖动比较严重需要进行滤波，还有零偏校准
uint8_t my_mpu6050_get_accel(Accel_struct *accel_data)
{
    uint8_t buf[6];
    // 一次突发读6字节(0x3B~0x40)
    if (HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDR_READ, MPU6050_ACCEL_XOUT_H,
                         I2C_MEMADD_SIZE_8BIT, buf, 6, 5) != HAL_OK)
    {
        return 1;
    }
    accel_data->Accel_x = ((buf[0] << 8) | buf[1]) - offset_accel_x;
    accel_data->Accel_y = ((buf[2] << 8) | buf[3]) - offset_accel_y;
    accel_data->Accel_z = ((buf[4] << 8) | buf[5]) - offset_accel_z;
    return 0;
}

uint8_t my_mpu6050_get_data(Accel_Gyro_struct *data)
{
    uint8_t buf[14];
    // 一次突发读14字节(0x3B~0x48): 加速度6 + 温度2 + 陀螺仪6
    if (HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDR_READ, MPU6050_ACCEL_XOUT_H,
                         I2C_MEMADD_SIZE_8BIT, buf, 14, 5) != HAL_OK)
    {
        return 1;
    }
    data->Accel.Accel_x = ((buf[0] << 8) | buf[1]) - offset_accel_x;
    data->Accel.Accel_y = ((buf[2] << 8) | buf[3]) - offset_accel_y;
    data->Accel.Accel_z = ((buf[4] << 8) | buf[5]) - offset_accel_z;
    data->Gyro.Gyro_x = ((buf[8] << 8) | buf[9]) - offset_gyro_x;
    data->Gyro.Gyro_y = ((buf[10] << 8) | buf[11]) - offset_gyro_y;
    data->Gyro.Gyro_z = ((buf[12] << 8) | buf[13]) - offset_gyro_z;
    return 0;
}

