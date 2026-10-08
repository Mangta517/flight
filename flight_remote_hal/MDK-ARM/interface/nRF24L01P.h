#ifndef __nRF24L01P__
#define __nRF24L01P__

#include "spi.h"
#include "common_debug.h"
#include "FreeRTOS.h"
#include "task.h"
//在stm32需要到cubemax中配置SPI
//拉低片选选中
#define CSN_LOW HAL_GPIO_WritePin(SPI_NSS_GPIO_Port, SPI_NSS_Pin, GPIO_PIN_RESET)
//拉高片选取消选中
#define CSN_HIGH HAL_GPIO_WritePin(SPI_NSS_GPIO_Port, SPI_NSS_Pin, GPIO_PIN_SET)
//拉低使能
#define CE_LOW HAL_GPIO_WritePin(SPI_EN_GPIO_Port, SPI_EN_Pin, GPIO_PIN_RESET)
//拉高使能
#define CE_HIGH HAL_GPIO_WritePin(SPI_EN_GPIO_Port, SPI_EN_Pin, GPIO_PIN_SET)
//nRF24L01+ PIN DEFINITION

#define CHANNEL        40 // Default RF channel
#define TX_ADR_WIDTH   5  // 5 bytes address width for TX
#define TX_PLOAD_WIDTH 17 // TX_PLOAD_WIDTH bytes payload width for TX

//********************************************************************************************************************//
// SPI(nRF24L01+) commands
#define nRF24R1_READ_REG  0x00 // Define read command to register
#define nRF24R1_WRITE_REG 0x20 // Define write command to register
#define RD_RX_PLOAD       0x61 // Define RX payload register address
#define WR_TX_PLOAD       0xA0 // Define TX payload register address
#define FLUSH_TX          0xE1 // Define flush TX register command
#define FLUSH_RX          0xE2 // Define flush RX register command
#define REUSE_TX_PL       0xE3 // Define reuse TX payload register command
#define NOP               0xFF // Define No Operation, might be used to read status register

//********************************************************************************************************************//
// SPI(nRF24L01+) registers(addresses)
#define CONFIG      0x00 // 'Config' register address
#define EN_AA       0x01 // 'Enable Auto Acknowledgment' register address
#define EN_RXADDR   0x02 // 'Enabled RX addresses' register address
#define SETUP_AW    0x03 // 'Setup address width' register address
#define SETUP_RETR  0x04 // 'Setup Auto. Retrans' register address
#define RF_CH       0x05 // 'RF channel' register address
#define RF_SETUP    0x06 // 'RF setup' register address
#define STATUS      0x07 // 'Status' register address
#define OBSERVE_TX  0x08 // 'Observe TX' register address
#define RPD         0x09 // 'Received Power Detector' register address
#define RX_ADDR_P0  0x0A // 'RX address pipe0' register address
#define RX_ADDR_P1  0x0B // 'RX address pipe1' register address
#define RX_ADDR_P2  0x0C // 'RX address pipe2' register address
#define RX_ADDR_P3  0x0D // 'RX address pipe3' register address
#define RX_ADDR_P4  0x0E // 'RX address pipe4' register address
#define RX_ADDR_P5  0x0F // 'RX address pipe5' register address
#define TX_ADDR     0x10 // 'TX address' register address
#define RX_PW_P0    0x11 // 'RX payload width, pipe0' register address
#define RX_PW_P1    0x12 // 'RX payload width, pipe1' register address
#define RX_PW_P2    0x13 // 'RX payload width, pipe2' register address
#define RX_PW_P3    0x14 // 'RX payload width, pipe3' register address
#define RX_PW_P4    0x15 // 'RX payload width, pipe4' register address
#define RX_PW_P5    0x16 // 'RX payload width, pipe5' register address
#define FIFO_STATUS 0x17 // 'FIFO Status Register' register address

//********************************************************************************************************************//
// STATUS Register
#define RX_DR  0x40 /**/
#define TX_DS  0x20
#define MAX_RT 0x10

//********************************************************************************************************************//
//                                        FUNCTION's PROTOTYPES                                                       //
//********************************************************************************************************************//
//nRF24L01+ API Functions
//24L01+ Pin Init
/* 写一个寄存器的值 */
uint8_t Int_nRF24L01P_Write_Reg(uint8_t reg, uint8_t value);
/* 写多个寄存器的值 */
uint8_t Int_nRF24L01P_Write_Buf(uint8_t reg, const uint8_t *pBuf, uint8_t size);
uint8_t Int_nRF24L01P_Read_Reg(uint8_t reg);
uint8_t Int_nRF24L01P_Read_Buf(uint8_t reg, uint8_t *pBuf, uint8_t size);

void Int_nRF24L01P_RX_Mode(void);
void Int_nRF24L01P_TX_Mode(void);
uint8_t Int_nRF24L01P_RxPacket(uint8_t *rxbuf);
uint8_t Int_nRF24L01P_TxPacket(uint8_t *txbuf);

/* nre2401的硬件接口层初始化 */
void Int_nRF24L01P_Init(void);
uint8_t Int_nRF24L01P_check(void);
//void Int_nRF24L01P_Init(void);
//********************************************************************************************************************//
#endif
