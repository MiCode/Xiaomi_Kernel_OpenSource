/*
** $Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/os/windows/ce/hif/spi/include/colibri.h#1 $
*/

/*! \file   "colibri.h"
    \brief  The header file of colibri platform

*/

/*******************************************************************************
* SKIP LEGAL DISCLAIMER FOR THIS FILE.
********************************************************************************
*/

/*
** $Log: colibri.h $
**
** 09 17 2012 cm.chang
** [BORA00002149] [MT6630 Wi-Fi] Initial software development
** Duplicate source from MT6620 v2.3 driver branch
** (Davinci label: MT6620_WIFI_Driver_V2_3_120913_1942_As_MT6630_Base)
 *
 * 07 08 2010 cp.wu
 *
 * [WPD00003833] [MT6620 and MT5931] Driver migration - move to new repository.
 *
 * 06 06 2010 kevin.huang
 * [WPD00003832][MT6620 5931] Create driver base
 * [MT6620 5931] Create driver base
**  \main\maintrunk.MT6620WiFiDriver_Prj\1 2009-04-24 21:15:35 GMT mtk01104
**  Initial version
*/

#ifndef _COLIBRI_H
#define _COLIBRI_H

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

#define CONFIG_ARCH_PXA270      1
#define CONFIG_SPI_8_BIT_MODE   1

#if CONFIG_SPI_8_BIT_MODE
#define SPI_LOOP_COUNT          4
#else				/* define for 32 bit mode */
#define SPI_LOOP_COUNT          1
#endif

#define CLKMGR_BASE_ADD         0x41300000
#define SSP1_BASE_ADD           0x41000000
#define SSP2_BASE_ADD           0x41700000
#define SSP3_BASE_ADD           0x41900000
#define GPIO_BASE_ADD           0x40E00000

/* GPIO pin for SPI */
#define GPIO23_SCLK             23
#define GPIO24_SFRM             24
#define GPIO25_STXD             25
#define GPIO26_SRXD             26

/*** SPI Register ***/
#define SSSR_TNF                   (0x1<<2)
#define SSSR_RNE                   (0x1<<3)
#define SSSR_BSY                   (0x1<<4)

/* SSCR0 : Macro definition for port 0/1/2 */
#define SSCR0_MOD                  (0x1<<31)	/* 0: Normal SSP mode */
#define SSCR0_ACS                  (0x1<<30)	/* 1: Audio Clock, 0: decided by NCS and ECS */
#define SSCR0_TIM                  (0x1<<23)	/* Transmit FIFO underrun Interrupt Mask, 0: TUR will generate SSP interrupt */
#define SSCR0_RIM                  (0x1<<22)	/* Receive FIFO Over Run Interrupt Mask 0: will generate interrupt */
#define SSCR0_NCS                  (0x1<<21)	/* Select network Clock 0: ECS bit determine clock selection */
#define SSCR0_EDSS                 (0x1<<20)	/* Extended data size select is used in conjunction with DSS to select the size of data transimitted */
						     /* 1: one is pre-appended to the DSS value that sets DSS range from 17-32 bit. */
#define SSCR0_SCR_512K             (0x19<<8)	/* SCR value is 25 based on system SSP clock is 13MHZ */
#define SSCR0_SCR_128K             (0x67<<8)
#define SSCR0_SSE                  (0x1<<7)	/* 1: SSP operation is enabled */
#define SSCR0_ECS                  (0x1<<6)	/* External Clock Select, 0: on_chip clock used to produce the SSP port's serial clock */
#define SSCR0_FRF_TISSP            (0x1<<4)	/* TI SSP */
#define SSCR0_FRF_PSP              (0x3<<4)	/* PSP: programmable serial protocol */
#define SSCR0_DSS_14BIT            0xD	/* 14 bit data transfer */
#define SSCR0_DSS_32BIT            0xF	/* 32 bit data transfer */
#define SSCR0_DSS_8BIT             0x7	/* 8 bit data */
#define SSCR0_DSS_16BIT            0xF	/* 16 bit data EDSS = 0 */

/* SSCR1 */
#define SSCR1_THLD_8               (0x8<<10)	/* threshold is 8 bytes, written to */
#define SSCR1_TTELP                (0x1<<31)	/* 1: TXD line will be tristated 1/2 clock after TXD is to be flopped */
#define SSCR1_TTE                  (0x1<<30)	/* 1: TXD line will not be tristated, 0: TXD line will be tristated when not transmitting data */
#define SSCR1_EBCE1                (0x1<<29)	/* 1: Interrupt due to a bit error is enabled. 00: Interrupt due to a bit count is disabled. */
#define SSCR1_SCFR                 (0x1<<28)	/* 1: clock iput to SSPSCLK is active only during trnasfers */
#define SSCR1_ECRA                 (0x1<<27)	/* Enable Clock request A, 1 = Clcok request from other SSP port is enabled */
#define SSCR1_ECRB                 (0x1<<26)	/* Enable Clock Request B */
#define SSCR1_SCLKDIR              (0x1<<25)	/* 0: Master Mode, the port generate SSPSCLK internally, acts as master and drive SSPSCLK */
#define SSCR1_SFRMDIR              (0x1<<24)	/* SSP frame direction determine whether SSP port is the master or slave 0: Master */
#define SSCR1_RWOT                 (0x1<<23)	/* 0: Transmit/Receive mode, 1: Receive with out Transmit mode */
#define SSCR1_TRAIL                (0x1<<22)	/* Trailing byte is used to configure how trailing byte are handled, 0: Processor Based, Trailing byte are handled by processor */
#define SSCR1_TSRE                 (0x1<<21)	/* Transmit Service Request Enables the transmit FIFO DMA service request.1: DMA service request is enabled */
#define SSCR1_RSRE                 (0x1<<20)	/* Receive Service Request enables the Receive FIFO DMa Service Request */
#define SSCR1_TINTE                (0x1<<19)	/* Receiver time-out interrupt enables the receiver time-out interrupt, 1: receive time-out interrupts are enabled, 1 -- peripheral trailing byte interrupt are enabled */
#define SSCR1_PINTE                (0x1<<18)	/* Peripheral trailing byte interrupt enables the peripheral trailing byte interrupt */
#define SSCR1_STRF                 (0x1<<15)	/* Select whether the transmit FIFO or receive FIFO is enabled for writes and reads (test mode) */
#define SSCR1_EFWR                 (0x1<<14)	/* Enable FIFO Write/Read (Test mode bit) for the SSP port */
#define SSCR1_MWDS                 (0x1<<5)	/* Mircowire Transmit Data size.1 = 16 bit command word is transmitted */
#define SSCR1_SPH                  (0x1<<4)	/* SPI SSPSCLK phase setting  0 = SSPSCLK is inactive one cycle at the start of frame and 1/2 cycle at the end of frame */
#define SSCR1_SPO                  (0x1<<3)	/* Motoroloa SPI SSPSCLK polarity setting selects the polarity of the inactive state of the SSPSCLK pins */
#define SSCR1_LBM                  (0x1<<2)	/* Loop-back mode */
#define SSCR1_TIE                  (0x1<<1)	/* 0: Transmit FIFO level interrupt is disabled. */
#define SSCR1_RIE                  (0x1)	/* 0: Receive FIFO level interrupt is disabled. The interrupt is masked. */

/* SSPSP : programmable Serial Protocol Register */
/* SSTO : SSP time-out register   0-23 bits R/W time-out interval =
					(TIMEOUT)/Peripheral Clock Frequency */

/* SSSP : SSP status Register */
#define SSSP_BCE                   (0x1<<23)	/* Bit Count Error 1: the SSPSFRM signal has been asserted when the bit counter was not 0 */
#define SSSP_CSS                   (0x1<<22)	/* Clock Synchronous Status  1: The SSP is currently busy synchronizing slave mode operation. */
#define SSSP_TUR                   (0x1<<21)	/* 1: Attempted read from the tramit FIFO when the FIFO was empty */
#define SSSP_EOC                   (0x1<<20)	/* 1: DMA has singaled an end of chain condition, there is no more data to be processed */
#define SSSP_TINT                  (0x1<<19)	/* 1: Receiver Time-out pending */
#define SSSP_PINT                  (0x1<<18)	/* 1: Peripheral Trailing Byte Interrupt Pending */
#define SSSP_RFL_MASK              (0xf<<12)	/* Receive FIFO Level is the number of valid entries. */
#define SSSP_TFL_MASK              (0xf<<8)	/* The Transmit FIFO level the number of valid entries currently in the transmit FIFO */
#define SSSP_ROR                   (0x1<<7)	/* 1: attempted data write to full receive FIFO */
#define SSSP_RFS                   (0x1<<6)	/* 1: Receive FIFO level is at or above RFT tirgger */
#define SSSP_TFS                   (0x1<<5)	/* 1: Transmit FIFO level is at or below TFT tigger threshold, request interrupt */
#define SSSP_BSY                   (0x1<<4)	/* 1: SSP port is currently transmitting or receiving a frame */
#define SSSP_RNE                   (0x1<<3)	/* 1: Receive FIFO is not empty */
#define SSSP_TNF                   (0x1<<2)	/* 1: Transmit FIFO is not full */

/*** Clock Enable Register (CLKEN) Bits ***/
#define CLKEN_PWM0_2        (0x1u << 0)
#define CLKEN_PWM1_3        (0x1u << 1)
#define CLKEN_AC97          (0x1u << 2)
#define CLKEN_SSP2          (0x1u << 3)
#define CLKEN_SSP3          (0x1u << 4)
#define CLKEN_STUART        (0x1u << 5)
#define CLKEN_FFUART        (0x1u << 6)
#define CLKEN_BTUART        (0x1u << 7)
#define CLKEN_I2S           (0x1u << 8)
#define CLKEN_OST           (0x1u << 9)
#define CLKEN_USBHOST       (0x1u << 10)
#define CLKEN_USBCLIENT     (0x1u << 11)
#define CLKEN_MMC           (0x1u << 12)
#define CLKEN_ICP           (0x1u << 13)
#define CLKEN_I2C           (0x1u << 14)
#define CLKEN_PWRI2C        (0x1u << 15)
#define CLKEN_LCD           (0x1u << 16)
#define CLKEN_BASEBAND      (0x1u << 17)
#define CLKEN_USIM          (0x1u << 18)
#define CLKEN_KEYPAD        (0x1u << 19)
#define CLKEN_MEMCLOCK      (0x1u << 20)
#define CLKEN_MEMSTICK      (0x1u << 21)
#define CLKEN_MEMC          (0x1u << 22)
#define CLKEN_SSP1          (0x1u << 23)
#define CLKEN_CAMERA        (0x1u << 24)	/* Camera Capture interface */
#define CLKEN_TPM           (0x1u << 25)	/* Trusted Platform Module (Caddo) */

#define SSP_BASE_ADD        SSP1_BASE_ADD	/* use SSP 1 */
#define CLKEN_SSP           CLKEN_SSP1	/* use SSP 1 */
#define GPIO_INTR           27	/* use SSP_EXTCLK as interrupt source */

#define IO_MASK_INTR        BIT(27)	/*!< for GPDR0 */
#define FUN_MASK_INTR       BITS(22, 23)	/*!< for GAFR0_U */

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

typedef volatile UINT_32 VUINT_32;

/* OST Register Definitions */
typedef struct {
	VUINT_32 osmr0;		/* OS timer match register 0 */
	VUINT_32 osmr1;		/* OS timer match register 1 */
	VUINT_32 osmr2;		/* OS timer match register 2 */
	VUINT_32 osmr3;		/* OS timer match register 3 */
	VUINT_32 oscr0;		/* OS timer counter register 0(compatible) */
	VUINT_32 ossr;		/* OS timer status register */
	VUINT_32 ower;		/* OS timer watchdog enable register */
	VUINT_32 oier;		/* OS timer interrupt enable register */
	VUINT_32 osnr;		/* OS timer snapshot register */
	VUINT_32 reserved1[7];
	VUINT_32 oscr4;		/* OS timer counter register 4 */
	VUINT_32 oscr5;		/* OS timer counter register  5 */
	VUINT_32 oscr6;		/* OS timer counter register  6 */
	VUINT_32 oscr7;		/* OS timer counter register  7 */
	VUINT_32 oscr8;		/* OS timer counter register  8 */
	VUINT_32 oscr9;		/* OS timer counter register  9 */
	VUINT_32 oscr10;	/* OS timer counter register  10 */
	VUINT_32 oscr11;	/* OS timer counter register  11 */
	VUINT_32 reserved2[8];
	VUINT_32 osmr4;		/* OS timer match register 4 */
	VUINT_32 osmr5;		/* OS timer match register 5 */
	VUINT_32 osmr6;		/* OS timer match register 6 */
	VUINT_32 osmr7;		/* OS timer match register 7 */
	VUINT_32 osmr8;		/* OS timer match register 8 */
	VUINT_32 osmr9;		/* OS timer match register 9 */
	VUINT_32 osmr10;	/* OS timer match register 10 */
	VUINT_32 osmr11;	/* OS timer match register 11 */
	VUINT_32 reserved3[8];
	VUINT_32 omcr4;		/* OS timer match control register 4 */
	VUINT_32 omcr5;		/* OS timer match control register 5 */
	VUINT_32 omcr6;		/* OS timer match control register 6 */
	VUINT_32 omcr7;		/* OS timer match control register 7 */
	VUINT_32 omcr8;		/* OS timer match control register 8 */
	VUINT_32 omcr9;		/* OS timer match control register 9 */
	VUINT_32 omcr10;	/* OS timer match control register 10 */
	VUINT_32 omcr11;	/* OS timer match control register 11 */
} OST_Timer, *P_OST_Timer;


/* Clock Manager (CLKMGR) Register Bank */
typedef struct {
	VUINT_32 cccr;		/* Core Clock Configuration register */
	VUINT_32 cken;		/* Clock Enable register */
	VUINT_32 oscc;		/* Oscillator Configuration register */
	VUINT_32 ccsr;		/* Core Clock Status register */
} CLK_MGR, *P_CLK_MGR;


/* SSP */
typedef struct {
	unsigned long sscr0;	/* SSP control register 0 */
	unsigned long sscr1;	/* SSP control register 1 */
	unsigned long sssr;	/* SSP status register */
	unsigned long ssitr;	/* SSP interrupt test register */
	unsigned long ssdr;	/* SSP data read/write register */
	unsigned long reserved1;
	unsigned long reserved2;
	unsigned long reserved3;
	unsigned long reserved4;
	unsigned long reserved5;
	unsigned long ssto;	/* Time out register */
	unsigned long sspsp;	/* SSP programmable serial protocol */
	unsigned long sstsa;	/* SSP2 TX time slot active */
	unsigned long ssrsa;	/* SSP2 RX time slot active */
} SSP_REG, *P_SSP_REG;


/* GPIO Register Definitions */
typedef struct {
	VUINT_32 GPLR0;		/* Level Detect Reg. Bank 0 */
	VUINT_32 GPLR1;		/* Level Detect Reg. Bank 1 */
	VUINT_32 GPLR2;		/* Level Detect Reg. Bank 2 */
	VUINT_32 GPDR0;		/* Data Direction Reg. Bank 0 */
	VUINT_32 GPDR1;		/* Data Direction Reg. Bank 1 */
	VUINT_32 GPDR2;		/* Data Direction Reg. Bank 2 */
	VUINT_32 GPSR0;		/* Pin Output Set Reg. Bank 0 */
	VUINT_32 GPSR1;		/* Pin Output Set Reg. Bank 1 */
	VUINT_32 GPSR2;		/* Pin Output Set Reg. Bank 2 */
	VUINT_32 GPCR0;		/* Pin Output Clr Reg. Bank 0 */
	VUINT_32 GPCR1;		/* Pin Output Clr Reg. Bank 1 */
	VUINT_32 GPCR2;		/* Pin Output Clr Reg. Bank 2 */
	VUINT_32 GRER0;		/* Ris. Edge Detect Enable Reg. Bank 0 */
	VUINT_32 GRER1;		/* Ris. Edge Detect Enable Reg. Bank 1 */
	VUINT_32 GRER2;		/* Ris. Edge Detect Enable Reg. Bank 2 */
	VUINT_32 GFER0;		/* Fal. Edge Detect Enable Reg. Bank 0 */
	VUINT_32 GFER1;		/* Fal. Edge Detect Enable Reg. Bank 1 */
	VUINT_32 GFER2;		/* Fal. Edge Detect Enable Reg. Bank 2 */
	VUINT_32 GEDR0;		/* Edge Detect Status Reg. Bank 0 */
	VUINT_32 GEDR1;		/* Edge Detect Status Reg. Bank 1 */
	VUINT_32 GEDR2;		/* Edge Detect Status Reg. Bank 2 */
	VUINT_32 GAFR0_L;	/* Alt. Function Select Reg.[  0:15 ] */
	VUINT_32 GAFR0_U;	/* Alt. Function Select Reg.[ 16:31 ] */
	VUINT_32 GAFR1_L;	/* Alt. Function Select Reg.[ 32:47 ] */
	VUINT_32 GAFR1_U;	/* Alt. Function Select Reg.[ 48:63 ] */
	VUINT_32 GAFR2_L;	/* Alt. Function Select Reg.[ 64:79 ] */
	VUINT_32 GAFR2_U;	/* Alt. Function Select Reg.[ 80:95 ] */
	VUINT_32 GAFR3_L;	/* Alt. Function Select Reg.[ 96:111] */
	VUINT_32 GAFR3_U;	/* Alt. Function Select Reg.[112:120] */
	VUINT_32 RESERVED1[35];	/* addr. offset 0x074-0x0fc */
	VUINT_32 GPLR3;		/* Level Detect Reg. Bank 3 */
	VUINT_32 RESERVED2[2];	/* addr. offset 0x104-0x108 */
	VUINT_32 GPDR3;		/* Data Direction Reg. Bank 3 */
	VUINT_32 RESERVED3[2];	/* addr. offset 0x110-0x114 */
	VUINT_32 GPSR3;		/* Pin Output Set Reg. Bank 3 */
	VUINT_32 RESERVED4[2];	/* addr. offset 0x11c-0x120 */
	VUINT_32 GPCR3;		/* Pin Output Clr Reg. Bank 3 */
	VUINT_32 RESERVED5[2];	/* addr. offset 0x128-0x12c */
	VUINT_32 GRER3;		/* Ris. Edge Detect Enable Reg. Bank 3 */
	VUINT_32 RESERVED6[2];	/* addr. offset 0x134-0x138 */
	VUINT_32 GFER3;		/* Fal. Edge Detect Enable Reg. Bank 3 */
	VUINT_32 RESERVED7[2];	/* addr. offset 0x140-0x144 */
	VUINT_32 GEDR3;		/* Edge Detect Status Reg. Bank 3 */
} GPIO_REG, *P_GPIO_REG;

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

#define SSCR0_DSS_SIZE(x) ((x > 16) ? (((x-17) & BITS(0, 3)) | (1<<20)) : ((x-1) & BITS(0, 3)))

#define WAIT_BUS_CLEAR(cmdRespBuff)    do { \
	    unsigned long u4Value; \
            while ((pVSsp->sssr & BITS(8, 11)) || !(pVSsp->sssr & SSSR_TNF));  \
	    while (pVSsp->sssr & SSSR_RNE) {    \
		u4Value = pVSsp->ssdr;   \
	    }   \
	} while (0)

#define WAIT_BUS_DONE() do { \
            while ((pVSsp->sssr & BITS(8, 11)) || !(pVSsp->sssr & SSSR_TNF));  \
	    while (pVSsp->sssr & SSSR_BSY); \
	} while (0)

#define WAIT_BUS_READY_TX()    do { \
	    while (!(pVSsp->sssr & SSSR_TNF));   \
	} while (0)

#define WAIT_BUS_READY_RX(x)    do { \
	    x = 10000;   \
	    while (!(pVSsp->sssr & SSSR_RNE) && (x--));   \
	} while (0)

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

void platformBusInit(IN P_GLUE_INFO_T prGlueInfo);

static INT_32 SpiSetBusWidth(UINT_32 u4BusSize);

static VOID SpiSetOpMode(VOID);

static VOID GpioSetDirectionIn(P_GPIO_REG prGPIO, UINT_32 au4GpioPinArray[]
    );

static VOID GpioSetDirectionOut(P_GPIO_REG prGPIO, UINT_32 au4GpioPinArray[]
    );

static VOID
GpioSetAlternateFn(P_GPIO_REG prGPIO, UINT_32 au4GpioPinArray[], UINT_32 au4AfValueArray[]
    );

static VOID GpioSetFallingEdgeDetectEnable(P_GPIO_REG prGPIO, UINT_32 au4GpioPinArray[]
    );

VOID GPIOConfigBackup(IN VOID);

VOID GPIOConfigRestore(IN VOID);

VOID SpiSendCmd32(UINT_32 u4Cmd, UINT_32 u4Offset, UINT_32 *pU4Value);

void SpiReadWriteData32(UINT_32 u4Cmd, UINT_32 u4Offset, UINT_8 *pucDataBuff, UINT_32 u4Size);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

#endif				/* _COLIBRI_H */
