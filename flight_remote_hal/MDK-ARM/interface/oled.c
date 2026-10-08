#include "oled.h"
#include "stdlib.h"
#include "oledfont.h"
#include "i2c.h"
#include "common_debug.h"

// OLED_GRAM[128][8] 为 1024 字节，每字节表示同一列的 8 个像素
uint8_t OLED_GRAM[128][8];
static uint8_t oled_i2c_addr = OLED_I2C_ADDR;

/* 脏页标志：某页显存被修改过就置1，OLED_Refresh_Gram 只刷新脏页 */
static uint8_t oled_page_dirty[8];

/* 硬件 I2C 传输超时时间 (ms) */
#define OLED_I2C_TIMEOUT 100U

#define OLED_WIDTH            128U
#define OLED_HEIGHT           64U
#define OLED_ASCII_1206_WIDTH 6U
#define OLED_ASCII_1608_WIDTH 8U

/* 页写临时缓冲：GRAM 按列存放(GRAM[x][page])，刷屏时按页组装后一次发送 */
static uint8_t oled_page_buf[OLED_WIDTH];

static uint8_t OLED_GetAsciiWidth(uint8_t size)
{
    return (size == 12U) ? OLED_ASCII_1206_WIDTH : OLED_ASCII_1608_WIDTH;
}

static uint8_t OLED_GetBytesPerColumn(uint8_t size)
{
    return (size + 7U) / 8U;
}

/* ==================== 硬件 I2C 底层 (HAL I2C1, PB8=SCL, PB9=SDA) ==================== */

/* I2C 传输失败后复位外设，释放总线，避免后续传输卡死 */
static void OLED_I2C_Reset(void)
{
    HAL_I2C_DeInit(&hi2c1);
    HAL_I2C_Init(&hi2c1);
}

/* ==================== OLED 写入函数 ==================== */

/*
 * 向 SSD1306 写入一个字节。
 * I2C 写时序: 起始 → 从机地址(0x78) → 控制字节(0x00/0x40) → 数据/命令 → 停止
 * HAL_I2C_Mem_Write(8位寄存器地址) 恰好生成同样的时序: 起始→地址→控制字节→数据→停止
 */
void OLED_WR_Byte(uint8_t data, uint8_t cmd)
{
    uint8_t ctrl = cmd ? OLED_CTRL_DATA : OLED_CTRL_CMD;

    if (HAL_I2C_Mem_Write(&hi2c1, oled_i2c_addr, ctrl, I2C_MEMADD_SIZE_8BIT, &data, 1U, OLED_I2C_TIMEOUT) != HAL_OK)
    {
        OLED_I2C_Reset();
    }
}

/* 探测从机地址：返回 1=有应答, 0=无应答 */
static uint8_t OLED_Probe_Address(uint8_t addr)
{
    return (HAL_I2C_IsDeviceReady(&hi2c1, addr, 3U, OLED_I2C_TIMEOUT) == HAL_OK);
}

/* 一次写完一页(128字节)显存数据 */
static void OLED_Write_Gram_Page(uint8_t page)
{
    uint8_t x;

    /* GRAM 按列存放，先组装成连续的一页数据 */
    for (x = 0; x < OLED_WIDTH; x++)
    {
        oled_page_buf[x] = OLED_GRAM[x][page];
    }

    if (HAL_I2C_Mem_Write(
            &hi2c1, oled_i2c_addr, OLED_CTRL_DATA, I2C_MEMADD_SIZE_8BIT, oled_page_buf, OLED_WIDTH, OLED_I2C_TIMEOUT) !=
        HAL_OK)
    {
        OLED_I2C_Reset();
    }
}

/* ==================== 显存刷新 ==================== */

void OLED_Refresh_Gram(void)
{
    uint8_t i;
    for (i = 0; i < 8; i++)
    {
        /* 该页无变化则跳过，不产生任何 I2C 通信 */
        if (oled_page_dirty[i] == 0U)
        {
            continue;
        }
        OLED_WR_Byte(0xb0 + i, OLED_CMD); // 设置页地址（0~7）
        OLED_WR_Byte(0x00, OLED_CMD);     // 设置列低地址
        OLED_WR_Byte(0x10, OLED_CMD);     // 设置列高地址
        OLED_Write_Gram_Page(i);
        oled_page_dirty[i] = 0U; // 页已同步
    }
}

/* ==================== 显示开关 ==================== */

void OLED_Display_On(void)
{
    OLED_WR_Byte(0X8D, OLED_CMD); // 电荷泵使能
    OLED_WR_Byte(0X14, OLED_CMD); // 电荷泵开
    OLED_WR_Byte(0XAF, OLED_CMD); // 显示开
}

void OLED_Display_Off(void)
{
    OLED_WR_Byte(0X8D, OLED_CMD); // 电荷泵
    OLED_WR_Byte(0X10, OLED_CMD); // 电荷泵关
    OLED_WR_Byte(0XAE, OLED_CMD); // 显示关
}

/* ==================== 清屏 ==================== */

void OLED_Clear(void)
{
    uint8_t i, n;
    for (i = 0; i < 8; i++)
    {
        for (n = 0; n < 128; n++)
        {
            OLED_GRAM[n][i] = 0X00;
        }
        oled_page_dirty[i] = 1U;
    }
    OLED_Refresh_Gram();
}

/* ==================== 画点 ==================== */

void OLED_DrawPoint(uint8_t x, uint8_t y, uint8_t t)
{
    uint8_t pos, bx, temp;
    if (x > 127 || y > 63)
        return;
    pos = 7 - y / 8;
    bx = y % 8;
    temp = 1 << (7 - bx);
    if (t)
        OLED_GRAM[x][pos] |= temp;
    else
        OLED_GRAM[x][pos] &= ~temp;
    oled_page_dirty[pos] = 1U; // 标记该页有改动
}

/* ==================== 字符显示 ==================== */

void OLED_ShowChar(uint8_t x, uint8_t y, uint8_t chr, uint8_t size, uint8_t mode)
{
    uint8_t temp, t, t1;
    uint8_t y0 = y;
    if (chr < ' ' || chr > '~')
        return;
    chr = chr - ' ';
    for (t = 0; t < size; t++)
    {
        if (size == 12)
            temp = oled_asc2_1206[chr][t];
        else
            temp = oled_asc2_1608[chr][t];
        for (t1 = 0; t1 < 8; t1++)
        {
            if (temp & 0x80)
                OLED_DrawPoint(x, y, mode);
            else
                OLED_DrawPoint(x, y, !mode);
            temp <<= 1;
            y++;
            if ((y - y0) == size)
            {
                y = y0;
                x++;
                break;
            }
        }
    }
}

/* ==================== 汉字显示 ==================== */

void OLED_ShowCH(uint8_t x, uint8_t y, uint8_t chr, uint8_t size, uint8_t mode)
{
    uint8_t temp, t, t1;
    uint8_t y0 = y;

    for (t = 0; t < OLED_CH_BYTES_PER_HALF; t++)
    {
        temp = oled_CH_1616[chr][t];
        for (t1 = 0; t1 < 8; t1++)
        {
            if (temp & 0x80)
                OLED_DrawPoint(x, y, mode);
            else
                OLED_DrawPoint(x, y, !mode);
            temp <<= 1;
            y++;
            if ((y - y0) == size)
            {
                y = y0;
                x++;
                break;
            }
        }
    }
}

void OLED_Show_CH(uint8_t x, uint8_t y, uint8_t chr, uint8_t size, uint8_t mode)
{
    uint8_t half_width = OLED_CH_BYTES_PER_HALF / OLED_GetBytesPerColumn(size);
    OLED_ShowCH(x, y, chr * 2, size, mode);
    OLED_ShowCH(x + half_width, y, chr * 2 + 1, size, mode);
}

void OLED_Show_CH_String(uint8_t x, uint8_t y, const uint8_t *p, uint8_t size, uint8_t mode)
{
    uint8_t temp, t, t1;
    uint8_t y0 = y;

    for (t = 0; t < size; t++)
    {
        temp = p[t];
        for (t1 = 0; t1 < 8; t1++)
        {
            if (temp & 0x80)
                OLED_DrawPoint(x, y, mode);
            else
                OLED_DrawPoint(x, y, !mode);
            temp <<= 1;
            y++;
            if ((y - y0) == size)
            {
                y = y0;
                x++;
                break;
            }
        }
    }
}

/* ==================== 数字显示 ==================== */

static uint32_t oled_pow(uint8_t m, uint8_t n)
{
    uint32_t result = 1;
    while (n--)
        result *= m;
    return result;
}

void OLED_ShowNumber(uint8_t x, uint8_t y, uint32_t num, uint8_t len, uint8_t size)
{
    uint8_t t, temp;
    uint8_t enshow = 0;
    for (t = 0; t < len; t++)
    {
        temp = (num / oled_pow(10, len - t - 1)) % 10;
        if (enshow == 0 && t < (len - 1))
        {
            if (temp == 0)
            {
                OLED_ShowChar(x + (size / 2) * t, y, ' ', size, 1);
                continue;
            }
            else
                enshow = 1;
        }
        OLED_ShowChar(x + (size / 2) * t, y, temp + '0', size, 1);
    }
}

/* ==================== 字符串显示 ==================== */

void OLED_ShowString(uint8_t x, uint8_t y, const uint8_t *p, uint8_t size, uint8_t mode)
{
    uint8_t char_width = OLED_GetAsciiWidth(size);

    while (*p != '\0')
    {
        if (x > (OLED_WIDTH - char_width))
        {
            x = 0;
            y += size;
        }
        if (y > (OLED_HEIGHT - size))
        {
            y = x = 0;
        }
        OLED_ShowChar(x, y, *p, size, mode);
        x += char_width;
        p++;
    }
}

void OLED_ShowString_16(uint8_t x, uint8_t y, const uint8_t *p)
{
    while (*p != '\0')
    {
        if (x > (OLED_WIDTH - OLED_ASCII_1608_WIDTH))
        {
            x = 0;
            y += 16;
        }
        if (y > (OLED_HEIGHT - 16U))
        {
            y = x = 0;
            OLED_Clear();
        }
        OLED_ShowChar(x, y, *p, 16, 1);
        x += 8;
        p++;
    }
}

/* ==================== OLED 初始化 ==================== */

void OLED_Init(void)
{
    /* PB8(SCL)/PB9(SDA) 已在 HAL_I2C_MspInit 中配置为 I2C1 复用开漏输出，
   * 这里不再重复配置 GPIO。
   */

    /* 兼容常见 SSD1306 地址：0x3C(0x78) / 0x3D(0x7A) */
    if (OLED_Probe_Address(0x78U))
    {
        oled_i2c_addr = 0x78U;
    }
    else if (OLED_Probe_Address(0x7AU))
    {
        oled_i2c_addr = 0x7AU;
    }
    else
    {
        oled_i2c_addr = OLED_I2C_ADDR;
        debug_printf("OLED I2C not found (tried 0x3C/0x3D)\r\n");
    }

    /* SSD1306 初始化命令序列 */
    OLED_WR_Byte(0xAE, OLED_CMD); // 关闭显示
    OLED_WR_Byte(0x00, OLED_CMD); // 设置低列地址
    OLED_WR_Byte(0x10, OLED_CMD); // 设置高列地址
    OLED_WR_Byte(0x40, OLED_CMD); // 设置起始行地址 (0x00~0x3F)
    OLED_WR_Byte(0x81, OLED_CMD); // 对比度控制
    OLED_WR_Byte(0xCF, OLED_CMD); // 亮度值
    OLED_WR_Byte(0xA1, OLED_CMD); // 列映射：0xA0 左右反置, 0xA1 正常
    OLED_WR_Byte(0xC0, OLED_CMD); // COM 扫描方向：0xC0 正常, 0xC8 上下反置
    OLED_WR_Byte(0xA6, OLED_CMD); // 正常显示（0xA7 反色）
    OLED_WR_Byte(0xA8, OLED_CMD); // 复用比设置
    OLED_WR_Byte(0x3F, OLED_CMD); // 1/64 Duty
    OLED_WR_Byte(0xD3, OLED_CMD); // 显示偏移
    OLED_WR_Byte(0x00, OLED_CMD); // 无偏移
    OLED_WR_Byte(0xD5, OLED_CMD); // 时钟分频 / 振荡频率
    OLED_WR_Byte(0x80, OLED_CMD); // 默认值
    OLED_WR_Byte(0xD9, OLED_CMD); // 预充电周期
    OLED_WR_Byte(0xF1, OLED_CMD); // Phase1=15, Phase2=1
    OLED_WR_Byte(0xDA, OLED_CMD); // COM 引脚硬件配置
    OLED_WR_Byte(0x12, OLED_CMD); // 顺序模式，左右反置
    OLED_WR_Byte(0xDB, OLED_CMD); // VCOMH 电平
    OLED_WR_Byte(0x30, OLED_CMD); // ~0.77 × VCC
    OLED_WR_Byte(0x20, OLED_CMD); // 寻址模式
    OLED_WR_Byte(0x02, OLED_CMD); // 页寻址模式
    OLED_WR_Byte(0x8D, OLED_CMD); // 电荷泵设置
    OLED_WR_Byte(0x14, OLED_CMD); // 使能内部 DC-DC
    OLED_Clear();
    OLED_WR_Byte(0xAF, OLED_CMD); // 开启显示
}
