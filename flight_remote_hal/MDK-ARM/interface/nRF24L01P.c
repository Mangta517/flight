#include "nRF24L01P.h"

// 定义一个静态发送地址
uint8_t TX_ADDRESS[TX_ADR_WIDTH] = {0x0A, 0x01, 0x07, 0x0E, 0x01}; // ����һ����̬���͵�ַ
uint8_t nRF24R1_rxbuff[5] = {0};

uint8_t Int_nRF24L01P_check(void)
{
	
	// nrf24l01p芯片需要先读取一次寄存器值，才能正常读写寄存器，否则会一直读到0xff的值，无法正确配置芯片。
	// 测试SPI能不能正常读写寄存器
	Int_nRF24L01P_Read_Buf(nRF24R1_READ_REG + TX_ADDR, nRF24R1_rxbuff, TX_ADR_WIDTH);
	Int_nRF24L01P_Write_Buf(nRF24R1_WRITE_REG + TX_ADDR, TX_ADDRESS, TX_ADR_WIDTH);
	// 读取寄存器值进行验证
	Int_nRF24L01P_Read_Buf(nRF24R1_READ_REG + TX_ADDR, nRF24R1_rxbuff, TX_ADR_WIDTH);
	for(int i = 0; i < TX_ADR_WIDTH; i++)
	{
		if (nRF24R1_rxbuff[i] != TX_ADDRESS[i])
		{
			return 1; // 检测失败
		}
		
	}
	return 0; // 检测成功
}
void Int_nRF24L01P_Init(void)
{
	HAL_Delay(200); // 上电后等待nRF24L01P芯片稳定
	while(Int_nRF24L01P_check()!=0)
	{
		HAL_Delay(10);
	}
	Int_nRF24L01P_RX_Mode(); // 默认进入接收模式
	debug_printf("nRF24L01P init success\r\n");
}

static uint8_t SPI_RW(uint8_t byte)
{
	uint8_t rx_data;
	HAL_SPI_TransmitReceive(&hspi2, &byte, &rx_data, 1, 1000);
	return rx_data;
}

/********************************************************
�������ܣ�д�Ĵ�����ֵ�����ֽڣ�
��ڲ�����reg:�Ĵ���ӳ���ַ����ʽ��nRF24R1_WRITE_REG��reg��
					value:�Ĵ�����ֵ
����  ֵ��״̬�Ĵ�����ֵ
*********************************************************/
uint8_t Int_nRF24L01P_Write_Reg(uint8_t reg, uint8_t value)
{
	uint8_t status;

	CSN_LOW;
	status = SPI_RW(reg);
	SPI_RW(value);
	CSN_HIGH;

	return (status);
}

/********************************************************
�������ܣ�д�Ĵ�����ֵ�����ֽڣ�
��ڲ�����reg:�Ĵ���ӳ���ַ����ʽ��nRF24R1_WRITE_REG��reg��
					pBuf:д�����׵�ַ
					bytes:д�����ֽ���
����  ֵ��״̬�Ĵ�����ֵ
*********************************************************/
uint8_t Int_nRF24L01P_Write_Buf(uint8_t reg, const uint8_t *pBuf, uint8_t size)
{
	uint8_t status, byte_ctr;

	CSN_LOW;
	status = SPI_RW(reg);
	for (byte_ctr = 0; byte_ctr < size; byte_ctr++)
	{
		SPI_RW(*pBuf++);
	}
	CSN_HIGH;

	return (status);
}

/********************************************************
�������ܣ���ȡ�Ĵ�����ֵ�����ֽڣ�
��ڲ�����reg:�Ĵ���ӳ���ַ����ʽ��READ_REG��reg��
����  ֵ���Ĵ���ֵ
*********************************************************/
uint8_t Int_nRF24L01P_Read_Reg(uint8_t reg)
{
	uint8_t value;

	CSN_LOW;
	SPI_RW(reg);
	value = SPI_RW(0);
	CSN_HIGH;

	return (value);
}

/********************************************************
�������ܣ���ȡ�Ĵ�����ֵ�����ֽڣ�
��ڲ�����reg:�Ĵ���ӳ���ַ��READ_REG��reg��
					pBuf:���ջ��������׵�ַ
					bytes:��ȡ�ֽ���
����  ֵ��״̬�Ĵ�����ֵ
*********************************************************/
uint8_t Int_nRF24L01P_Read_Buf(uint8_t reg, uint8_t *pBuf, uint8_t size)
{
	uint8_t status, byte_ctr;

	CSN_LOW;
	status = SPI_RW(reg);
	for (byte_ctr = 0; byte_ctr < size; byte_ctr++)
	{
		pBuf[byte_ctr] = SPI_RW(0);
	} // ȡݣֽǰ
	CSN_HIGH;

	return (status);
}

/********************************************************
�������ܣ�nRF24L01+����ģʽ��ʼ��
��ڲ�������
����  ֵ����
*********************************************************/
void Int_nRF24L01P_RX_Mode(void)
{
	CE_LOW;
	Int_nRF24L01P_Write_Buf(nRF24R1_WRITE_REG + RX_ADDR_P0, TX_ADDRESS, TX_ADR_WIDTH); // �����豸����ͨ��0ʹ�úͷ����豸��ͬ�ķ��͵�ַ
	Int_nRF24L01P_Write_Reg(nRF24R1_WRITE_REG + EN_AA, 0x01);						   // ʹ�ܽ���ͨ��0�Զ�Ӧ��
	Int_nRF24L01P_Write_Reg(nRF24R1_WRITE_REG + EN_RXADDR, 0x01);					   // ʹܽͨ0
	Int_nRF24L01P_Write_Reg(nRF24R1_WRITE_REG + RF_CH, CHANNEL);					   // ѡƵͨ0x40
	Int_nRF24L01P_Write_Reg(nRF24R1_WRITE_REG + RX_PW_P0, TX_PLOAD_WIDTH);			   // ͨ0ѡͷͨͬЧݿ
	Int_nRF24L01P_Write_Reg(nRF24R1_WRITE_REG + RF_SETUP, 0x0f);					   // ݴ2Mbps书0dBmŴ(nRF24L01+Ըλ
	Int_nRF24L01P_Write_Reg(nRF24R1_WRITE_REG + CONFIG, 0x0f);						   // CRCʹܣ16λCRCУ飬ϵ磬ģʽ
	Int_nRF24L01P_Write_Reg(nRF24R1_WRITE_REG + STATUS, 0xff);						   // ежϱ־λ
	CE_HIGH;																		   // CE豸
}

/********************************************************
ܣnRF24L01+ģʽʼ
��ڲ�������
����  ֵ����
*********************************************************/
void Int_nRF24L01P_TX_Mode(void)
{
	CE_LOW;
	Int_nRF24L01P_Write_Buf(nRF24R1_WRITE_REG + TX_ADDR, TX_ADDRESS, TX_ADR_WIDTH);	   // д뷢͵ַ
	Int_nRF24L01P_Write_Buf(nRF24R1_WRITE_REG + RX_ADDR_P0, TX_ADDRESS, TX_ADR_WIDTH); // ΪӦ豸ͨ0ַͷ͵ַͬ
	Int_nRF24L01P_Write_Reg(nRF24R1_WRITE_REG + EN_AA, 0x01);						   // ʹܽͨ0ԶӦ
	Int_nRF24L01P_Write_Reg(nRF24R1_WRITE_REG + EN_RXADDR, 0x01);					   // ʹܽͨ0
	Int_nRF24L01P_Write_Reg(nRF24R1_WRITE_REG + SETUP_RETR, 0x0a);					   // Զطʱȴ250us+86usԶط10
	Int_nRF24L01P_Write_Reg(nRF24R1_WRITE_REG + RF_CH, CHANNEL);					   // ѡƵͨ0x40
	Int_nRF24L01P_Write_Reg(nRF24R1_WRITE_REG + RF_SETUP, 0x0f);					   // ݴ2Mbps书0dBmŴ(nRF24L01+Ըλ
	Int_nRF24L01P_Write_Reg(nRF24R1_WRITE_REG + CONFIG, 0x0e);						   // CRCʹܣ16λCRCУ飬ϵ
	CE_HIGH;
}

/********************************************************
�������ܣ���ȡ��������
��ڲ�����rxbuf:�������ݴ���׵�ַ
����  ֵ��0:���յ�����
		  1:û�н��յ�����
*********************************************************/
uint8_t Int_nRF24L01P_RxPacket(uint8_t *rxbuf)
{
	uint8_t state;
	// 因为状态寄存器的标志位设计为写1清除，所以每次读取状态寄存器后都要写回去清除标志位，否则会一直保持之前的状态，导致无法正确判断是否有新的数据到来。
	state = Int_nRF24L01P_Read_Reg(STATUS);						// ��ȡ״̬�Ĵ�����ֵ
	Int_nRF24L01P_Write_Reg(nRF24R1_WRITE_REG + STATUS, state); // ���RX_DS�жϱ�־

	if (state & RX_DR) // ���յ�����
	{
		Int_nRF24L01P_Read_Buf(RD_RX_PLOAD, rxbuf, TX_PLOAD_WIDTH); // ��ȡ����
		Int_nRF24L01P_Write_Reg(FLUSH_RX, 0xff);					// RX FIFOĴ
		return 0;
	}
	return 1; // û�յ��κ�����
}

/********************************************************
0：表示发送成功
1：表示发送失败，达到最大重发次数
*********************************************************/
uint8_t Int_nRF24L01P_TxPacket(uint8_t *txbuf)
{
	uint8_t state;
	CE_LOW;														 // CE���ͣ�ʹ��24L01����
	Int_nRF24L01P_Write_Buf(WR_TX_PLOAD, txbuf, TX_PLOAD_WIDTH); // дݵTX FIFO,32ֽ
	CE_HIGH;													 // CEøߣʹܷ

	// while(IRQ == 1);
	// 不用中断信号，使用轮询等待
	state = Int_nRF24L01P_Read_Reg(STATUS);
	while (((state & TX_DS) == 0) && ((state & MAX_RT) == 0)) // ȴ
	{
		state = Int_nRF24L01P_Read_Reg(STATUS);
		vTaskDelay(1); // 等待1ms，避免一直轮询占用CPU资源
	}															// ȡ״̬Ĵֵ
	Int_nRF24L01P_Write_Reg(nRF24R1_WRITE_REG + STATUS, state); // TX_DSMAX_RTжϱ־
	if (state & MAX_RT)											// ﵽط
	{
		Int_nRF24L01P_Write_Reg(FLUSH_TX, 0xff); // TX FIFOĴ
		return 1;
	}
	if (state & TX_DS) //
	{
		return 0; // 发送成功
	}
	return 1; // ����ʧ��
}
