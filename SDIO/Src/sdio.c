#include "stm32f4xx.h"
#include "system_stm32F4xx.h"
#include "stdlib.h"
#include "cmsis_delay.h"
#include "sdio.h"

#define INIT_FREQ 400000
#define SDIO_INPUT_FREQ 48000000
#define BLOCK_SIZE 9 //2^9=512
#define SCR_SD_BUS_WIDTHS 0x000F000

uint16_t SDIO_RCA;

uint16_t SDIO_Get_RCA()
{
	return SDIO_RCA;
}

void SDIO_Init()
{
	
	RCC->APB2ENR |= RCC_APB2ENR_SDIOEN; 	// SDIO clock enable
	SDIO->POWER=SDIO_POWER_PWRCTRL_Msk; 	//SDIO Power up
	
	RCC->AHB1ENR|=RCC_AHB1ENR_DMA2EN;			//DMA clock enable
	SDIO->DCTRL|=SDIO_DCTRL_DMAEN;				//DMA SDIO enable
	
	SDIO->CLKCR|=
	SDIO_CLKCR_CLKEN| 						//SDIO_CK clock enable
	(0x0<<SDIO_CLKCR_WIDBUS_Pos)|//1 wire bus enable
	(((SDIO_INPUT_FREQ/INIT_FREQ)-2)<<SDIO_CLKCR_CLKDIV_Pos);//CLK devider 118
	
	GPIOC->MODER|=		//Alternate function
	0x2<<8*2|		//PC8
	0x2<<9*2|		//PC9
	0x2<<10*2|	//PC10
	0x2<<11*2|	//PC11
	0x2<<12*2;	//PC12
	
	GPIOC->PUPDR|=		//Pull-Up
	0x1<<8*2|		//PC8
	0x1<<9*2|		//PC9
	0x1<<10*2|	//PC10
	0x1<<11*2|	//PC11
	0x1<<12*2;	//PC12
	
	GPIOC->OTYPER|=		//Push-Pull
	0x0<<8|		//PC8
	0x0<<9|		//PC9
	0x0<<10|	//PC10
	0x0<<11|	//PC11
	0x0<<12;	//PC12
	
	GPIOC->OSPEEDR|=	//High speed
	0x3<<8*2|		//PC8
	0x3<<9*2|		//PC9
	0x3<<10*2|	//PC10
	0x3<<11*2|	//PC11
	0x3<<12*2;	//PC12
	
	GPIOC->AFR[1]|=		//AF12 (SDIO)
	12<<(8-8)*4|	//PC8
	12<<(9-8)*4|	//PC9
	12<<(10-8)*4|	//PC10
	12<<(11-8)*4|	//PC11
	12<<(12-8)*4;	//PC12
	
	
	GPIOD->MODER|=		//Alternate function
	0x2<<2*2;		//PD2

	
	GPIOD->PUPDR|=		//Pull-Up
	0x1<<2*2;		//PD2

	
	GPIOD->OTYPER|=		//Push-Pull
	0x0<<2;			//PD2

	
	GPIOD->OSPEEDR|=	//High speed
	0x3<<2*2;		//PD2

	
	GPIOD->AFR[0]|=		//AF12 (SDIO)
	12<<(2*4);	//PD2

	SDIO->DCTRL|=(BLOCK_SIZE<<SDIO_DCTRL_DBLOCKSIZE_Pos); //Block size 2^12=4096
	
}
uint8_t SDIO_Command(uint8_t cmd, uint8_t RespType, uint32_t argument, uint32_t response[4])
{
	uint32_t tempreg=0;
	SDIO->ICR=0xFFFFFFFF;
	SDIO->ARG=argument;
	tempreg|=RespType<<SDIO_CMD_WAITRESP_Pos;
	tempreg|=cmd<<SDIO_CMD_CMDINDEX_Pos;
	tempreg|=SDIO_CMD_CPSMEN;
	SDIO->CMD=tempreg;
	while((SDIO->STA&SDIO_STA_CMDACT));
	SDIO->CMD&=~SDIO_CMD_CPSMEN;
	if(RespType==0x0|RespType==0x2) return 0;
	else
	{
		if(RespType==0x1|RespType==0x3)
		{
			response[0]=SDIO->RESP1;
			response[1]=SDIO->RESP2;
			response[2]=SDIO->RESP3;
			response[3]=SDIO->RESP4;	
		}
		
	}
	return 0;
}

void SDIO_Connect()
{
	uint8_t HCS;
	uint32_t resp[4];
	SDIO_Command(0,0,0,0);																			//GO_IDLE_STATE
	SDIO_Command(8,1,0x000001AA,resp);													//Spec version?
	if(SDIO->STA&SDIO_STA_CTIMEOUT)HCS=0;
	else HCS=1;
	SDIO_Command(55,1,0x0,0);																		//ACMD prefix
	if(HCS)
	{
		if(resp[0]==0x000001AA)HCS=1;
		else HCS=0;
	}
	SDIO_Command(41,1,0x00000000|HCS<<30|0xFF80<<8,resp);
	while(!(resp[0]>>31))
	{
		SDIO_Command(55,1,0x0,0);
		Delay(10);
		SDIO_Command(41,1,0x00000000|HCS<<30|0xFF80<<8,resp);
		Delay(10);
	}
	//SdioCommand(11,3,0,0);
	SDIO_Command(2,1,0,0);
	Delay(1);
	SDIO_Command(3,1,0,resp);
	SDIO_RCA=resp[0]>>16;
	
	SDIO->CLKCR&=~SDIO_CLKCR_CLKDIV_Msk;
	SDIO->CLKCR|=0x2<<SDIO_CLKCR_CLKDIV_Pos;
	
	//Get SCR register
	SDIO_Command(55,1,SDIO_RCA<<16,0);
	SDIO_Command(6,1,0x2,resp);
	
	//Check support bus width
	SDIO->CLKCR&=~SDIO_CLKCR_WIDBUS_Msk;
	SDIO->CLKCR|=1<<SDIO_CLKCR_WIDBUS_Pos;
	
	//debug blink
	if(SDIO->STA&SDIO_STA_CMDREND)
	{
		GPIOA->BSRR|=GPIO_BSRR_BS1;
	}
	
	
}

int SDIO_disk_initialize()
{
	return 0;
}

int SDIO_disk_status()
{
	return 0;
}


int SDIO_disk_read(BYTE *buff, LBA_t sector, UINT count)
{
	uint32_t tempreg; //Create template register
	uint8_t data[512]={0};
	
	
	//DMA config
	DMA2_Stream3->CR&=~DMA_SxCR_EN;
	tempreg=
	4<<DMA_SxCR_CHSEL_Pos|		//Channel 4
	1<<DMA_SxCR_MBURST_Pos|		//4 burst memory mode
	1<<DMA_SxCR_PBURST_Pos|		//4 burst periphal mode
	0<<DMA_SxCR_DBM_Pos|			//Double buffer disable
	3<<DMA_SxCR_PL_Pos|				//Very high priotity
	0<<DMA_SxCR_PINCOS_Pos|		
	2<<DMA_SxCR_MSIZE_Pos|		//Memory size word
	2<<DMA_SxCR_PSIZE_Pos|		//Periphal size word
	1<<DMA_SxCR_MINC_Pos|			//Memory increment enable
	0<<DMA_SxCR_PINC_Pos|			//Periphal increment disable
	0<<DMA_SxCR_CIRC_Pos|			//Circular mode enable
	0<<DMA_SxCR_DIR_Pos|			//From periphal to memory
	1<<DMA_SxCR_PFCTRL_Pos|		//Periphal is flow controller
	0<<DMA_SxCR_TCIE_Pos|			//Interrupt disable
	0<<DMA_SxCR_HTIE_Pos|			//Interrupt disable
	0<<DMA_SxCR_TEIE_Pos|			//Interrupt disable
	0<<DMA_SxCR_DMEIE_Pos;		//Interrupt disable
	DMA2_Stream3->CR=tempreg;
	tempreg=0;
	
	tempreg=
	0<<DMA_SxFCR_FEIE_Pos|		//FIFO Interrupt disable
	1<<DMA_SxFCR_DMDIS_Pos|		//Use FIFO
	3<<DMA_SxFCR_FTH_Pos;			//Full FIFO
	DMA2_Stream3->FCR=tempreg;
	tempreg=0;
	
	DMA2_Stream3->NDTR=512/4;
	
	DMA2_Stream3->PAR=0x40012C80;
	
	DMA2_Stream3->M0AR=(uint32_t)data;
	
	DMA2_Stream3->CR|=DMA_SxCR_EN;
	
	
	SDIO_Command(7,0x1,0,0); 
	SDIO_Command(7,0x1,SDIO_Get_RCA()<<16,0); //Select sd card
	
	SDIO->DLEN=512; //select data length
	
	SDIO->DTIMER=SDIO_DTIMER_DATATIME_Msk; //Set data timer
	
	tempreg=0;
	tempreg|=(uint32_t) 9 << 4; //block size 512 
	tempreg|=SDIO_DCTRL_DTDIR; //Data direction from card to controller
	tempreg|=SDIO_DCTRL_DMAEN; //DMA enable
	tempreg|=SDIO_DCTRL_DTEN; //Data enable
	SDIO->DCTRL=tempreg;
	
	SDIO_Command(17,0x1,0x000000000,0); //Get data
	//TODO: DMA read from buffer

	//TODO: DMA STOP
	while((SDIO->STA&SDIO_STA_DATAEND)!=0){};
	return 0;
}

int SDIO_disk_write(const BYTE *buff, LBA_t sector, UINT count)
{
	SDIO_Command(7,0x1,0,0);
	SDIO_Command(7,0x1,SDIO_Get_RCA()<<16,0);
	return 0;
}
