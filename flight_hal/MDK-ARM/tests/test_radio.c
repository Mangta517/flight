#include "nRF24L01P.h"
#include <assert.h>
#include <stdio.h>

SPI_HandleTypeDef hspi2;
static TickType_t tick;
static int byte_index, calls, fail_at, ce, flushes;
static uint8_t command, status;
static HAL_StatusTypeDef failure = HAL_ERROR;
TickType_t xTaskGetTickCount(void)
{
    return tick;
}
void vTaskDelay(TickType_t delay)
{
    tick += delay;
}
void HAL_Delay(uint32_t delay)
{
    tick += delay;
}
void HAL_GPIO_WritePin(void *port, uint16_t pin, uint32_t state)
{
    if (pin == SPI_NSS_Pin && state == GPIO_PIN_RESET)
    {
        byte_index = 0;
    }
    if (pin == SPI_EN_Pin)
    {
        ce = state;
    }
}
HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef *spi, uint8_t *tx, uint8_t *rx, uint16_t size, uint32_t timeout)
{
    assert(size == 1 && timeout == 2);
    calls++;
    assert(calls < 1000);
    *rx = 0;
    if (byte_index++ == 0)
    {
        command = *tx;
        if (command == FLUSH_TX)
        {
            flushes++;
        }
    }
    else if (command == STATUS)
    {
        *rx = status;
    }
    if (calls == fail_at)
    {
        tick += timeout;
        *rx = TX_DS | RX_DR;
        return failure;
    }
    return HAL_OK;
}
static void reset(void)
{
    calls = 0;
    flushes = 0;
    fail_at = 0;
    status = 0;
    tick = 10;
}
int main(void)
{
    uint8_t packet[TX_PLOAD_WIDTH] = {0};
    reset();
    assert(Int_nRF24L01P_TxPacket(packet) == 1);
    assert(tick >= 20 && tick < 25 && ce == GPIO_PIN_RESET && flushes == 1);
    reset();
    tick = UINT32_MAX - 5;
    assert(Int_nRF24L01P_TxPacket(packet) == 1 && tick < 10);
    reset();
    status = TX_DS;
    assert(Int_nRF24L01P_TxPacket(packet) == 0 && ce == GPIO_PIN_RESET);
    reset();
    status = MAX_RT;
    assert(Int_nRF24L01P_TxPacket(packet) == 1 && flushes == 1);
    for (failure = HAL_ERROR; failure <= HAL_TIMEOUT; failure++)
    {
        reset();
        status = TX_DS;
        fail_at = 1;
        assert(Int_nRF24L01P_TxPacket(packet) == 1 && calls == 1);
        reset();
        status = TX_DS;
        fail_at = TX_PLOAD_WIDTH + 3;
        assert(Int_nRF24L01P_TxPacket(packet) == 1 && calls == fail_at);
        reset();
        status = RX_DR;
        fail_at = 7;
        assert(Int_nRF24L01P_RxPacket(packet) == 1 && calls == fail_at);
        reset();
        status = TX_DS;
        fail_at = 1;
        Int_nRF24L01P_TX_Mode();
        assert(calls == 1);
        assert(Int_nRF24L01P_TxPacket(packet) == 1 && calls == 1);
        Int_nRF24L01P_RX_Mode();
        assert(tick == 12);
    }
    reset();
    status = RX_DR;
    assert(Int_nRF24L01P_RxPacket(packet) == 0);
    puts("PASS radio deadline, wrap, MAX_RT, HAL errors, fail-fast modes and TX/RX");
    return 0;
}
