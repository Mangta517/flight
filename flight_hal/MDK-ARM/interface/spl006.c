#include "spl006.h"

static int32_t spl006_c00;
static int32_t spl006_c10;
static int16_t spl006_c01;
static int16_t spl006_c11;
static int16_t spl006_c20;
static int16_t spl006_c21;
static int16_t spl006_c30;

int32_t spl006_pressure;
int32_t spl006_temperature;
int32_t spl006_compensated_pressure;
HAL_StatusTypeDef spl_write_reg(uint8_t reg, uint8_t data)
{
    // I2C1 总线由飞行任务单所有权访问(MPU6050+SPL006均在飞行任务中读写)，无需临界区
    HAL_StatusTypeDef ret = HAL_I2C_Mem_Write(&hi2c1, SPL006_ADDR_WRITE, reg, I2C_MEMADD_SIZE_8BIT, &data, 1, 100);
    return ret;
}

HAL_StatusTypeDef spl_read_reg(uint8_t reg, uint8_t *data)
{
    HAL_StatusTypeDef ret = HAL_I2C_Mem_Read(&hi2c1, SPL006_ADDR_READ, reg, I2C_MEMADD_SIZE_8BIT, data, 1, 100);
    return ret;
}

void spl_Init(void)
{
    // 检查 I2C 通信
    uint8_t chip_id;
    if (spl_read_reg(SPL06_ID, &chip_id) != HAL_OK)
    {
         //debug_printf("SPL06 I2C read failed\r\n");
         return;
    }
    debug_printf("SPL06 chip ID: 0x%02X\r\n", chip_id);

    // 复位传感器
    if (spl_write_reg(SPL06_RESET, SPL06_SOFT_RESET) != HAL_OK)
    {
         //debug_printf("SPL06 reset write failed\r\n");
         return;
    }
    HAL_Delay(20);
    uint8_t MEAS_status;
    spl_read_reg(SPL06_MEAS_CFG , &MEAS_status);
    if ((MEAS_status & SPL06_SENSOR_RDY) == 0)
    {
         //debug_printf("SPL06 senor failed, status: 0x%02X\r\n", MEAS_status);
         return;
    }
     debug_printf("SPL06 senor successful\r\n");
       HAL_Delay(50);
    spl_read_reg(SPL06_MEAS_CFG, &MEAS_status);
    if ((MEAS_status & SPL06_COEF_RDY) == 0)
    {
         //debug_printf("SPL06 cofe failed, status: 0x%02X\r\n", MEAS_status);
         return;
    }
    debug_printf("SPL06 cofe successful\r\n");

        // 传感器和校准系数就绪，可以进行数据读取
        uint8_t spl006_high_byte;
        uint8_t spl006_mid_byte;
        uint8_t spl006_low_byte;
        spl_read_reg(SPL06_C00_BYTE2, &spl006_high_byte);
        spl_read_reg(SPL06_C00_BYTE1, &spl006_mid_byte);
        spl_read_reg(SPL06_C00_BYTE0, &spl006_low_byte);
        int32_t raw = ((uint32_t)spl006_high_byte << 12) | ((uint32_t)spl006_mid_byte << 4) | (spl006_low_byte >> 4);
        if (raw & 0x80000) raw |= 0xFFF00000;
        spl006_c00 = raw;

        spl_read_reg(SPL06_C10_BYTE2 , &spl006_high_byte);
        spl_read_reg(SPL06_C10_BYTE1, &spl006_mid_byte);
        spl_read_reg(SPL06_C10_BYTE0, &spl006_low_byte);
        raw = ((uint32_t)(spl006_high_byte & 0x0F) << 16) | ((uint32_t)spl006_mid_byte << 8) | spl006_low_byte;
        if (raw & 0x80000) raw |= 0xFFF00000;
        spl006_c10 = raw;

        spl_read_reg(SPL06_C01_BYTE1, &spl006_high_byte);
        spl_read_reg(SPL06_C01_BYTE0, &spl006_low_byte);
        raw = ((uint32_t)spl006_high_byte << 8) | spl006_low_byte;
        if (raw & 0x8000) raw |= 0xFFFF0000;
        spl006_c01 = raw;

        spl_read_reg(SPL06_C11_BYTE1, &spl006_high_byte);
        spl_read_reg(SPL06_C11_BYTE0, &spl006_low_byte);
        raw = ((uint32_t)spl006_high_byte << 8) | spl006_low_byte;
        if (raw & 0x8000) raw |= 0xFFFF0000;
        spl006_c11 = raw;

        spl_read_reg(SPL06_C20_BYTE1, &spl006_high_byte);
        spl_read_reg(SPL06_C20_BYTE0, &spl006_low_byte);
        raw = ((uint32_t)spl006_high_byte << 8) | spl006_low_byte;
        if (raw & 0x8000) raw |= 0xFFFF0000;
        spl006_c20 = raw;

        spl_read_reg(SPL06_C21_BYTE1, &spl006_high_byte);
        spl_read_reg(SPL06_C21_BYTE0, &spl006_low_byte);
        raw = ((uint32_t)spl006_high_byte << 8) | spl006_low_byte;
        if (raw & 0x8000) raw |= 0xFFFF0000;
        spl006_c21 = raw;

        spl_read_reg(SPL06_C30_BYTE1, &spl006_high_byte);
        spl_read_reg(SPL06_C30_BYTE0, &spl006_low_byte);
        raw = ((uint32_t)spl006_high_byte << 8) | spl006_low_byte;
        if (raw & 0x8000) raw |= 0xFFFF0000;
        spl006_c30 = raw;
        debug_printf("Calibration coefficients:\r\n");
        debug_printf("c00: %ld\r\n", spl006_c00);
        debug_printf("c10: %ld\r\n", spl006_c10);
        debug_printf("c01: %ld\r\n", spl006_c01);
        debug_printf("c11: %ld\r\n", spl006_c11);
        debug_printf("c20: %ld\r\n", spl006_c20);
        debug_printf("c21: %ld\r\n", spl006_c21);
        debug_printf("c30: %ld\r\n", spl006_c30);

     //配置压力传感器为标准模式，过采样率16倍，采样率64次/秒,如果过采样率 >8倍时需在 CFG_REG（0x09）中设置 P_SHIFT=1 和 T_SHIFT=1
    spl_write_reg(SPL06_PRS_CFG, SPL06_PM_PRC_16X|SPL06_PM_RATE_64PS);
    //配置温度传感器为内部传感器，过采样率16倍，采样率64次/秒，bit7=0为内部传感器
    spl_write_reg(SPL06_TMP_CFG, 0x80|SPL06_PM_RATE_64PS|SPL06_TMP_PRC_16X);
     //配置为检测压力和温度，背景模式

    spl_write_reg(SPL06_CFG_REG, SPL06_P_SHIFT|SPL06_T_SHIFT);
    //配置为检测压力和温度，背景模式
    spl_write_reg(SPL06_MEAS_CFG, SPL06_MEAS_BG_PRSTMP);


}

int spl_read_pressure(int32_t *pressure)
{
    uint8_t meas_cfg;

    spl_read_reg(SPL06_MEAS_CFG, &meas_cfg);
    if (!(meas_cfg & SPL06_PRS_RDY))
        return -1;

    uint8_t buf[3];
    // 突发读3字节(0x00~0x02)，替代3次单字节读
    if (HAL_I2C_Mem_Read(&hi2c1, SPL006_ADDR_READ, SPL06_PRS_B2,
                         I2C_MEMADD_SIZE_8BIT, buf, 3, 100) != HAL_OK)
        return -1;

    *pressure = ((uint32_t)buf[0] << 16) | ((uint32_t)buf[1] << 8) | buf[2];
    if (*pressure & 0x800000) *pressure |= 0xFF000000;
    return 0;
}

int spl_read_temperature(int32_t *temperature)
{
    uint8_t meas_cfg;

    spl_read_reg(SPL06_MEAS_CFG, &meas_cfg);
    if (!(meas_cfg & SPL06_TMP_RDY))
        return -1;

    uint8_t buf[3];
    // 突发读3字节(0x03~0x05)
    if (HAL_I2C_Mem_Read(&hi2c1, SPL006_ADDR_READ, SPL06_TMP_B2,
                         I2C_MEMADD_SIZE_8BIT, buf, 3, 100) != HAL_OK)
        return -1;

    *temperature = ((uint32_t)buf[0] << 16) | ((uint32_t)buf[1] << 8) | buf[2];
    if (*temperature & 0x800000) *temperature |= 0xFF000000;
    return 0;
}

/* 周期采样气压/温度到全局缓存，由飞行任务调用(约每6ms一次)。
 * 之后 spl_process_pressure 只做计算不再访问 I2C，保证总线单任务所有权。 */
void spl_update_data(void)
{
    if (spl_read_pressure(&spl006_pressure) != 0)
    {
        return;
    }
    spl_read_temperature(&spl006_temperature);
}

int spl_process_pressure(int32_t pressure, int32_t temperature, int32_t *compensated_pressure)
{
    float Praw_sc = (float)pressure / SPL06_SCALE_FACTOR_16X;
    float Traw_sc = (float)temperature / SPL06_SCALE_FACTOR_16X;

    *compensated_pressure = spl006_c00
                          + Praw_sc * (spl006_c10 + Praw_sc * (spl006_c20 + Praw_sc * spl006_c30))
                          + Traw_sc * spl006_c01
                          + Traw_sc * Praw_sc * (spl006_c11 + Praw_sc * spl006_c21);
    return *compensated_pressure;
}
