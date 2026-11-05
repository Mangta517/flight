#ifndef FLIGHT_HOST_MAIN_H
#define FLIGHT_HOST_MAIN_H
#include <stdint.h>
#include <stddef.h>
typedef enum
{
    HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT
} HAL_StatusTypeDef;
typedef struct
{
    int unused;
} I2C_HandleTypeDef;
typedef struct
{
    int unused;
} SPI_HandleTypeDef;
typedef struct
{
    int unused;
} TIM_HandleTypeDef;
extern I2C_HandleTypeDef hi2c1;
extern SPI_HandleTypeDef hspi2;
extern TIM_HandleTypeDef htim2;
#define I2C_MEMADD_SIZE_8BIT 1
#define GPIO_PIN_RESET 0
#define GPIO_PIN_SET 1
#define SPI_NSS_GPIO_Port ((void *)1)
#define SPI_NSS_Pin 1
#define SPI_EN_GPIO_Port ((void *)1)
#define SPI_EN_Pin 2
void HAL_Delay(uint32_t delay);
void HAL_GPIO_WritePin(void *port, uint16_t pin, uint32_t state);
HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef *spi, uint8_t *tx, uint8_t *rx, uint16_t size, uint32_t timeout);
HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *i2c, uint16_t address, uint16_t reg, uint16_t width, uint8_t *data, uint16_t size, uint32_t timeout);
HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *i2c, uint16_t address, uint16_t reg, uint16_t width, uint8_t *data, uint16_t size, uint32_t timeout);
#endif
