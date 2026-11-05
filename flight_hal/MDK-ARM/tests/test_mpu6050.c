#include "my_mpu6050.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

I2C_HandleTypeDef hi2c1;
static uint32_t tick;
static int reads, writes, fail_read, fail_write;
static uint8_t reset_stuck;
static HAL_StatusTypeDef failure = HAL_ERROR;
extern int16_t offset_accel_x, offset_accel_y, offset_accel_z;
extern int16_t offset_gyro_x, offset_gyro_y, offset_gyro_z;
void HAL_Delay(uint32_t delay)
{
    tick += delay;
}
void vTaskDelay(TickType_t delay)
{
    tick += delay;
}
HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *bus, uint16_t address, uint16_t reg, uint16_t width, uint8_t *data, uint16_t size, uint32_t timeout)
{
    assert(timeout == 5);
    writes++;
    if (writes == fail_write)
    {
        return failure;
    }
    return HAL_OK;
}
HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *bus, uint16_t address, uint16_t reg, uint16_t width, uint8_t *data, uint16_t size, uint32_t timeout)
{
    assert(timeout == 5);
    reads++;
    if (reads == fail_read)
    {
        return failure;
    }
    if (size == 1)
    {
        data[0] = 0;
        if (!reset_stuck)
        {
            data[0] = 0x40;
        }
        return HAL_OK;
    }
    int16_t sample[] = {20, -30, 16424, 0, 4, -5, 6};
    unsigned start = 0;
    if (reg == MPU6050_GYRO_XOUT_H)
    {
        start = 4;
    }
    for (unsigned i = 0; i < size / 2; i++)
    {
        data[2 * i] = (uint16_t)sample[start + i] >> 8;
        data[2 * i + 1] = sample[start + i] & 255;
    }
    return HAL_OK;
}
int main(void)
{
    assert(my_mpu6050_init() == 0 && writes == 10);
    reset_stuck = 1;
    reads = 0;
    tick = 0;
    assert(my_mpu6050_init() == 1 && reads == 100 && tick == 100);
    reset_stuck = 0;
    fail_write = writes + 4;
    assert(my_mpu6050_init() == 1);
    fail_write = 0;
    for (failure = HAL_ERROR; failure <= HAL_TIMEOUT; failure++)
    {
        Accel_Gyro_struct value = {{1, 2, 3}, {4, 5, 6}};
        Accel_Gyro_struct old = value;
        fail_read = reads + 1;
        assert(my_mpu6050_get_data(&value) == 1);
        assert(memcmp(&value, &old, sizeof(value)) == 0);
        fail_read = reads + 1;
        assert(my_mpu6050_init() == 1);
    }
    failure = HAL_ERROR;
    fail_read = 0;
    assert(my_mpu6050_zero_offset() == 0);
    assert(offset_accel_x == 20 && offset_accel_y == -30 && offset_accel_z == 40);
    assert(offset_gyro_x == 4 && offset_gyro_y == -5 && offset_gyro_z == 6);
    assert(my_mpu6050_zero_offset() == 0);
    assert(offset_accel_x == 20 && offset_accel_y == -30 && offset_accel_z == 40);
    assert(offset_gyro_x == 4 && offset_gyro_y == -5 && offset_gyro_z == 6);
    fail_read = reads + 150;
    assert(my_mpu6050_zero_offset() == 1);
    assert(offset_accel_x == 20 && offset_accel_y == -30 && offset_accel_z == 40);
    assert(offset_gyro_x == 4 && offset_gyro_y == -5 && offset_gyro_z == 6);
    puts("PASS MPU reset deadline, HAL errors, unchanged samples, repeated/partial calibration");
    return 0;
}
