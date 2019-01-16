/*
** $Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/os/windows/ce/hif/spi/include/hif.h#1 $
*/

/*! \file   "hif.h"
    \brief  spi specific structure for GLUE layer
*/



/*
** $Log: hif.h $
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
**  \main\maintrunk.MT6620WiFiDriver_Prj\1 2009-04-24 21:15:45 GMT mtk01104
**  Initial version
*/

#ifndef _HIF_H
#define _HIF_H

/*******************************************************************************
*                E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#if _PF_COLIBRI
#include "colibri.h"
#endif

#include "mt6620_reg.h"

/*******************************************************************************
*                          C O N S T A N T S
********************************************************************************
*/

#define NIC_INTERRUPT_MODE      NdisInterruptLevelSensitive

/* Interface type, we use Host-specific interface */
#define NIC_INTERFACE_TYPE      NdisInterfaceInternal
#define NIC_ATTRIBUTE           (NDIS_ATTRIBUTE_DESERIALIZE | \
				 NDIS_ATTRIBUTE_NO_HALT_ON_SUSPEND)
#define NIC_DMA_MAPPED          0
#define NIC_MINIPORT_INT_REG    1

/* buffer size passed in NdisMQueryAdapterResources
   We should only need three adapter resources (IO, interrupt and memory),
   Some devices get extra resources, so have room for 10 resources */
#define NIC_RESOURCE_BUF_SZ     (sizeof(NDIS_RESOURCE_LIST) + \
				(10 * sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR)))

#define GPIO_PIN_RESERVED_BITS  0xfe000000u
#define GPIO_ALT_RESERVED_BITS  0xfffc0000u

/* #define REQ_FLAG_HALT               (0x01) */

/*==========================================================*/
/*** MTK MT6620 SPI Command ***/

/* Function Command */
#define SPI_FUN_WR              0x80000000UL
#define SPI_FUN_RD              0x0
#define SPI_FUN_LEN_msk         0x00000FFFUL
#define SPI_FUN_LEN(x)          ((x & SPI_FUN_LEN_msk) << 16)

#define BUS_RETRY_COUNT         10000


/* Todo: these three values shall be shared with SDIO when enhanced mode
 *       is enabled
 */
#define WIFI_HIF_MAX_RX0_LEN_CNT    4
#define WIFI_HIF_MAX_RX1_LEN_CNT    4
#define WIFI_HIF_ENHANCED_MODE      0

#define WIFI_HIF_SPI_DATAOUT_MODE   1
#define WIFI_HIF_SPI_INTOUT_MODE    1
#define WIFI_HIF_SPI_ENDIAN         0
#if CONFIG_SPI_8_BIT_MODE
#define WIFI_HIF_SPI_MODE_SEL   0
#else
#define WIFI_HIF_SPI_MODE_SEL   2
#endif

/* In general, host shall be configured to 8-bit mode first, but
 * chip is also in 8-bit mode. So during mode configuration, do not swap
 * both Write CMD and its data.
 */
#define SPICSR_8BIT_MODE_DATA \
	( \
	    (WIFI_HIF_SPI_DATAOUT_MODE) | \
	    (WIFI_HIF_SPI_INTOUT_MODE << 1) | \
	    (WIFI_HIF_SPI_ENDIAN << 4) | \
	    (WIFI_HIF_SPI_MODE_SEL << 5) | \
	    (WIFI_HIF_MAX_RX0_LEN_CNT << 16) | \
	    (WIFI_HIF_MAX_RX1_LEN_CNT << 20) | \
	    (WIFI_HIF_ENHANCED_MODE << 24) \
	)

#define SPICSR_8BIT_MODE_ADDR \
	( \
	    (MCR_WCSR & 0xFFFF) | (4 << 16) | SPI_FUN_WR \
	)

/* In general, host shall be configured to 32-bit mode first, but
 * chip is still in 8-bit mode. So during mode configuration, swap
 * both Write CMD and its data.
 */
#define SPICSR_32BIT_MODE_DATA \
	SWAP32( \
	    (WIFI_HIF_SPI_DATAOUT_MODE) | \
	    (WIFI_HIF_SPI_INTOUT_MODE << 1) | \
	    (WIFI_HIF_SPI_ENDIAN << 4) | \
	    (WIFI_HIF_SPI_MODE_SEL << 5) | \
	    (WIFI_HIF_MAX_RX0_LEN_CNT << 16) | \
	    (WIFI_HIF_MAX_RX1_LEN_CNT << 20) | \
	    (WIFI_HIF_ENHANCED_MODE << 24) \
	)

#define SPICSR_32BIT_MODE_ADDR \
	SWAP32( \
	    (MCR_WCSR & 0xFFFF) | (4 << 16) | SPI_FUN_WR \
	)


/*******************************************************************************
*                         D A T A   T Y P E S
********************************************************************************
*/


/* Windows glue layer's private data structure, which is
 * attached to adapter_p structure
 */
typedef struct _GL_HIF_INFO_T {

	NDIS_MINIPORT_INTERRUPT rInterrupt;	/* Holds the interrupt object for
						   this adapter. */

	UINT_32 u4InterruptLevel;
	UINT_32 u4InterruptVector;

#if SC32442_SPI
	HANDLE gWaitEvent;
	UINT_32 u4sysIntr;
#endif

	UINT_32 u4ReqFlag;	/* REQ_FLAG_XXX */

} GL_HIF_INFO_T, *P_GL_HIF_INFO_T;


/*******************************************************************************
*              F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
#endif				/* _HIF_H */
