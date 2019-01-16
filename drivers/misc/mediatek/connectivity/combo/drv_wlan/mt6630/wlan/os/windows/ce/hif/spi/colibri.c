/*
** $Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/os/windows/ce/hif/spi/colibri.c#1 $
*/

/*! \file   "colibri.c"
    \brief  Colibri platform specific functions

*/



/*
** $Log: colibri.c $
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
**  \main\maintrunk.MT6620WiFiDriver_Prj\2 2009-04-29 20:13:47 GMT mtk01104
**  Fix bug. It needs OR, not AND in insteaGPIOConfigBackup() and GPIOConfigRestore()
**  \main\maintrunk.MT6620WiFiDriver_Prj\1 2009-04-24 21:13:20 GMT mtk01104
**  Initial version
*/

/******************************************************************************
*                         C O M P I L E R   F L A G S
*******************************************************************************
*/

/******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
*******************************************************************************
*/
#include "gl_os.h"
LINT_EXT_HEADER_BEGIN 
#include <ceddk.h>
    LINT_EXT_HEADER_END
#include "hif.h"
#include "colibri.h"
/******************************************************************************
*                              C O N S T A N T S
*******************************************************************************
*/
/* Colibri custom IOCTL */
#define GPIO_EDGE_RISING    1
#define GPIO_EDGE_FALLING   2
/* For Interrupt GPIO to sysIRQ */
#define IOCTL_HAL_GPIO2IRQ \
	CTL_CODE(FILE_DEVICE_HAL, 2048, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_HAL_IRQEDGE \
	CTL_CODE(FILE_DEVICE_HAL, 2050, METHOD_BUFFERED, FILE_ANY_ACCESS)
/******************************************************************************
*                             D A T A   T Y P E S
*******************************************************************************
*/
/******************************************************************************
*                            P U B L I C   D A T A
*******************************************************************************
*/
/* volatile OST_Timer* pVOSTRegs = NULL; */
volatile CLK_MGR * pVClkMgr = NULL;
volatile SSP_REG *pVSsp = NULL;
volatile GPIO_REG *pVMem = NULL;

/* For backup GPIO config */
UINT_32 Fun_GAFR0_U;
UINT_32 IO_GPDR0;

/******************************************************************************
*                           P R I V A T E   D A T A
*******************************************************************************
*/

/******************************************************************************
*                                 M A C R O S
*******************************************************************************
*/

/******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
*******************************************************************************
*/

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is to used for register access.
*
* \param[in] u4Cmd The Read/Write command use the MT5921 defined.
* \param[in] u4Offset The register address
* \param[in] pU4Value The read / wite buffer pointer
*
* \return None
*
*/
/*----------------------------------------------------------------------------*/
VOID SpiSendCmd32(UINT_32 u4Cmd, UINT_32 u4Offset, UINT_32 *pU4Value)
{
	UINT_32 u4CmdRespBuff = u4Cmd | (4 << 16) | u4Offset;
#if CONFIG_SPI_8_BIT_MODE
	UINT_32 i;
	PUINT_8 pucBuff = (PUINT_8) &u4CmdRespBuff;
#endif
	UINT_32 u4Value = 0;

	ASSERT(pU4Value);

	WAIT_BUS_CLEAR(u4CmdRespBuff);

#if CONFIG_SPI_8_BIT_MODE
	i = 0;
	while (i < 4) {
		WAIT_BUS_READY_TX();
		pVSsp->ssdr = pucBuff[i++];	/* for write cmd */
		WAIT_BUS_READY_RX(u4Value);
		if (u4Value != 0) {
			(void)pVSsp->ssdr;
		}
	}

	if (u4Cmd & BIT(31)) {	/* write */
		pucBuff = (PUINT_8) pU4Value;
		i = 0;
		while (i < 4) {
			WAIT_BUS_READY_TX();
			pVSsp->ssdr = pucBuff[i++];	/* write data */
		}
	} else {		/* read */
		i = 0;
		while (i < 4) {
			WAIT_BUS_READY_TX();
			pVSsp->ssdr = 0xff;	/* for rsp */
			WAIT_BUS_READY_RX(u4Value);
			pucBuff[i++] = (UINT_8) pVSsp->ssdr;
		}
		pucBuff = (PUINT_8) pU4Value;
		i = 0;
		while (i < 4) {
			WAIT_BUS_READY_TX();
			pVSsp->ssdr = 0xff;	/* for write data */
			WAIT_BUS_READY_RX(u4Value);
			pucBuff[i++] = (UINT_8) pVSsp->ssdr;
		}
	}

#else

	WAIT_BUS_READY_TX();
	pVSsp->ssdr = u4CmdRespBuff;	/* write cmd */
	WAIT_BUS_READY_RX(u4Value);
	if (u4Value != 0) {
		(void)pVSsp->ssdr;
	} else {
		ASSERT(FALSE);
	}

	if (u4Cmd & BIT(31)) {	/* write */
		WAIT_BUS_READY_TX();
		pVSsp->ssdr = pU4Value[0];	/* write data */
	} else {		/* read */
		WAIT_BUS_READY_TX();
		pVSsp->ssdr = 0xffffffff;
		WAIT_BUS_READY_RX(u4Value);
		if (u4Value != 0) {
			u4CmdRespBuff = pVSsp->ssdr;
		} else {
			ASSERT(FALSE);
		}

		WAIT_BUS_READY_TX();
		pVSsp->ssdr = 0xffffffff;
		WAIT_BUS_READY_RX(u4Value);
		if (u4Value != 0) {
			pU4Value[0] = pVSsp->ssdr;
		} else {
			ASSERT(FALSE);
		}
	}
#endif

	WAIT_BUS_DONE();

}


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is to used for port data access.
*
* \param[in] u4Cmd The Read/Write command use the MT5921 defined.
* \param[in] u4Offset The register address
* \param[in] pU4Value The read / wite buffer pointer
* \param[in] u4Size The read / wite buffer size
*
* \return None
*
*/
/*----------------------------------------------------------------------------*/
void SpiReadWriteData32(UINT_32 u4Cmd, UINT_32 u4Offset, UINT_8 *pucDataBuff, UINT_32 u4Size)
{
	UINT_32 u4CmdRespBuff = u4Cmd | (u4Size << 16) | u4Offset;
	UINT_32 u4Value = 0;
	UINT_32 i = 0, j = 0, temp = 0;
	UINT_32 u4Len32bit = (u4Size >> 2);
	UINT_32 *pu4DataBuff;
#if CONFIG_SPI_8_BIT_MODE
	UINT_32 k = 0;
	PUINT_8 pucBuff = (PUINT_8) &u4CmdRespBuff;
#endif

	ASSERT(pucDataBuff);

	pu4DataBuff = (UINT_32 *) pucDataBuff;

	WAIT_BUS_CLEAR(u4CmdRespBuff);

#if CONFIG_SPI_8_BIT_MODE

	k = 0;
	while (k < SPI_LOOP_COUNT) {
		WAIT_BUS_READY_TX();
		pVSsp->ssdr = pucBuff[k++];	/* write cmd */

		WAIT_BUS_READY_RX(u4Value);
		(void)pVSsp->ssdr;	/* for write cmd */
	}
	/* Data, use double word size */
	if (u4Cmd & BIT(31)) {	/* write */
		pucBuff = (PUINT_8) pucDataBuff;
		if ((u4Len32bit >> 4) > 0) {
			for (i = 0; i < (u4Len32bit >> 4); i++) {
				for (k = 0; k < SPI_LOOP_COUNT; k++) {
					WAIT_BUS_READY_TX();
					pVSsp->ssdr = pucBuff[j];
					WAIT_BUS_READY_TX();
					pVSsp->ssdr = pucBuff[j + 1];
					WAIT_BUS_READY_TX();
					pVSsp->ssdr = pucBuff[j + 2];
					WAIT_BUS_READY_TX();
					pVSsp->ssdr = pucBuff[j + 3];
					WAIT_BUS_READY_TX();
					pVSsp->ssdr = pucBuff[j + 4];
					WAIT_BUS_READY_TX();
					pVSsp->ssdr = pucBuff[j + 5];
					WAIT_BUS_READY_TX();
					pVSsp->ssdr = pucBuff[j + 6];
					WAIT_BUS_READY_TX();
					pVSsp->ssdr = pucBuff[j + 7];

					WAIT_BUS_READY_TX();
					pVSsp->ssdr = pucBuff[j + 8];
					WAIT_BUS_READY_TX();
					pVSsp->ssdr = pucBuff[j + 9];
					WAIT_BUS_READY_TX();
					pVSsp->ssdr = pucBuff[j + 10];
					WAIT_BUS_READY_TX();
					pVSsp->ssdr = pucBuff[j + 11];
					WAIT_BUS_READY_TX();
					pVSsp->ssdr = pucBuff[j + 12];
					WAIT_BUS_READY_TX();
					pVSsp->ssdr = pucBuff[j + 13];
					WAIT_BUS_READY_TX();
					pVSsp->ssdr = pucBuff[j + 14];
					WAIT_BUS_READY_TX();
					pVSsp->ssdr = pucBuff[j + 15];
					j += 16;
				}
			}
		}

		if ((u4Len32bit & 0x8) != 0) {
			for (k = 0; k < SPI_LOOP_COUNT; k++) {
				WAIT_BUS_READY_TX();
				pVSsp->ssdr = pucBuff[j];
				WAIT_BUS_READY_TX();
				pVSsp->ssdr = pucBuff[j + 1];
				WAIT_BUS_READY_TX();
				pVSsp->ssdr = pucBuff[j + 2];
				WAIT_BUS_READY_TX();
				pVSsp->ssdr = pucBuff[j + 3];
				WAIT_BUS_READY_TX();
				pVSsp->ssdr = pucBuff[j + 4];
				WAIT_BUS_READY_TX();
				pVSsp->ssdr = pucBuff[j + 5];
				WAIT_BUS_READY_TX();
				pVSsp->ssdr = pucBuff[j + 6];
				WAIT_BUS_READY_TX();
				pVSsp->ssdr = pucBuff[j + 7];
				j += 8;
			}
		}

		if ((u4Len32bit & 0x4) != 0) {
			for (k = 0; k < SPI_LOOP_COUNT; k++) {
				WAIT_BUS_READY_TX();
				pVSsp->ssdr = pucBuff[j];
				WAIT_BUS_READY_TX();
				pVSsp->ssdr = pucBuff[j + 1];
				WAIT_BUS_READY_TX();
				pVSsp->ssdr = pucBuff[j + 2];
				WAIT_BUS_READY_TX();
				pVSsp->ssdr = pucBuff[j + 3];
				j += 4;
			}
		}

		temp = (u4Len32bit & 0x3);
		while (temp--) {
			for (k = 0; k < SPI_LOOP_COUNT; k++) {
				WAIT_BUS_READY_TX();
				pVSsp->ssdr = pucBuff[j++];
			}
		}

	} else {		/* read */

		for (k = 0; k < SPI_LOOP_COUNT; k++) {
			WAIT_BUS_READY_TX();
			pVSsp->ssdr = 0xff;	/* rsp */
			WAIT_BUS_READY_RX(u4Value);
			(void)pVSsp->ssdr;	/* for rsp */
		}
		pucBuff = (PUINT_8) &pu4DataBuff[0];
		if ((u4Len32bit >> 4) > 0) {
			for (i; i < (u4Len32bit >> 4); i++) {
				for (k = 0; k < SPI_LOOP_COUNT; k++) {
					WAIT_BUS_READY_TX();
					pVSsp->ssdr = 0xFFFFFFFF;
					pVSsp->ssdr = 0xFFFFFFFF;
					pVSsp->ssdr = 0xFFFFFFFF;
					pVSsp->ssdr = 0xFFFFFFFF;
					pVSsp->ssdr = 0xFFFFFFFF;

					pVSsp->ssdr = 0xFFFFFFFF;
					pVSsp->ssdr = 0xFFFFFFFF;
					pVSsp->ssdr = 0xFFFFFFFF;
					pVSsp->ssdr = 0xFFFFFFFF;
					pVSsp->ssdr = 0xFFFFFFFF;

					pVSsp->ssdr = 0xFFFFFFFF;
					pVSsp->ssdr = 0xFFFFFFFF;
					pVSsp->ssdr = 0xFFFFFFFF;
					pVSsp->ssdr = 0xFFFFFFFF;
					pVSsp->ssdr = 0xFFFFFFFF;

					pVSsp->ssdr = 0xFFFFFFFF;

					WAIT_BUS_READY_RX(u4Value);
					pucBuff[j] = (UINT_8) pVSsp->ssdr;
					WAIT_BUS_READY_RX(u4Value);
					pucBuff[j + 1] = (UINT_8) pVSsp->ssdr;
					WAIT_BUS_READY_RX(u4Value);
					pucBuff[j + 2] = (UINT_8) pVSsp->ssdr;
					WAIT_BUS_READY_RX(u4Value);
					pucBuff[j + 3] = (UINT_8) pVSsp->ssdr;
					WAIT_BUS_READY_RX(u4Value);
					pucBuff[j + 4] = (UINT_8) pVSsp->ssdr;

					WAIT_BUS_READY_RX(u4Value);
					pucBuff[j + 5] = (UINT_8) pVSsp->ssdr;
					WAIT_BUS_READY_RX(u4Value);
					pucBuff[j + 6] = (UINT_8) pVSsp->ssdr;
					WAIT_BUS_READY_RX(u4Value);
					pucBuff[j + 7] = (UINT_8) pVSsp->ssdr;
					WAIT_BUS_READY_RX(u4Value);
					pucBuff[j + 8] = (UINT_8) pVSsp->ssdr;
					WAIT_BUS_READY_RX(u4Value);
					pucBuff[j + 9] = (UINT_8) pVSsp->ssdr;

					WAIT_BUS_READY_RX(u4Value);
					pucBuff[j + 10] = (UINT_8) pVSsp->ssdr;
					WAIT_BUS_READY_RX(u4Value);
					pucBuff[j + 11] = (UINT_8) pVSsp->ssdr;
					WAIT_BUS_READY_RX(u4Value);
					pucBuff[j + 12] = (UINT_8) pVSsp->ssdr;
					WAIT_BUS_READY_RX(u4Value);
					pucBuff[j + 13] = (UINT_8) pVSsp->ssdr;
					WAIT_BUS_READY_RX(u4Value);
					pucBuff[j + 14] = (UINT_8) pVSsp->ssdr;

					WAIT_BUS_READY_RX(u4Value);
					pucBuff[j + 15] = (UINT_8) pVSsp->ssdr;
					j += 16;
				}
			}
		}

		if ((u4Len32bit & 0x8) != 0) {
			for (k = 0; k < SPI_LOOP_COUNT; k++) {
				WAIT_BUS_READY_TX();
				pVSsp->ssdr = 0xFFFFFFFF;
				pVSsp->ssdr = 0xFFFFFFFF;
				pVSsp->ssdr = 0xFFFFFFFF;
				pVSsp->ssdr = 0xFFFFFFFF;
				pVSsp->ssdr = 0xFFFFFFFF;

				pVSsp->ssdr = 0xFFFFFFFF;
				pVSsp->ssdr = 0xFFFFFFFF;
				pVSsp->ssdr = 0xFFFFFFFF;

				WAIT_BUS_READY_RX(u4Value);
				pucBuff[j] = (UINT_8) pVSsp->ssdr;
				WAIT_BUS_READY_RX(u4Value);
				pucBuff[j + 1] = (UINT_8) pVSsp->ssdr;
				WAIT_BUS_READY_RX(u4Value);
				pucBuff[j + 2] = (UINT_8) pVSsp->ssdr;
				WAIT_BUS_READY_RX(u4Value);
				pucBuff[j + 3] = (UINT_8) pVSsp->ssdr;
				WAIT_BUS_READY_RX(u4Value);
				pucBuff[j + 4] = (UINT_8) pVSsp->ssdr;

				WAIT_BUS_READY_RX(u4Value);
				pucBuff[j + 5] = (UINT_8) pVSsp->ssdr;
				WAIT_BUS_READY_RX(u4Value);
				pucBuff[j + 6] = (UINT_8) pVSsp->ssdr;
				WAIT_BUS_READY_RX(u4Value);
				pucBuff[j + 7] = (UINT_8) pVSsp->ssdr;
				j += 8;
			}
		}

		if ((u4Len32bit & 0x4) != 0) {
			for (k = 0; k < SPI_LOOP_COUNT; k++) {
				WAIT_BUS_READY_TX();
				pVSsp->ssdr = 0xFFFFFFFF;
				pVSsp->ssdr = 0xFFFFFFFF;
				pVSsp->ssdr = 0xFFFFFFFF;
				pVSsp->ssdr = 0xFFFFFFFF;

				WAIT_BUS_READY_RX(u4Value);
				pucBuff[j] = (UINT_8) pVSsp->ssdr;
				WAIT_BUS_READY_RX(u4Value);
				pucBuff[j + 1] = (UINT_8) pVSsp->ssdr;
				WAIT_BUS_READY_RX(u4Value);
				pucBuff[j + 2] = (UINT_8) pVSsp->ssdr;
				WAIT_BUS_READY_RX(u4Value);
				pucBuff[j + 3] = (UINT_8) pVSsp->ssdr;
				j += 4;
			}
		}

		temp = (u4Len32bit & 0x3);
		while (temp--) {
			for (k = 0; k < SPI_LOOP_COUNT; k++) {
				WAIT_BUS_READY_TX();
				pVSsp->ssdr = 0xFF;
				WAIT_BUS_READY_RX(u4Value);
				pucBuff[j++] = (UINT_8) pVSsp->ssdr;
			}
		}
	}

#else				/* 32-bit mode */
	WAIT_BUS_READY_TX();
	pVSsp->ssdr = u4CmdRespBuff;	/* write cmd */

	WAIT_BUS_READY_RX(u4Value);
	u4Value = pVSsp->ssdr;	/* for write cmd */

	/* Data, use double word size */
	if (u4Cmd & BIT(31)) {	/* write */
		if ((u4Len32bit >> 4) > 0) {
			for (i = 0; i < (u4Len32bit >> 4); i++) {
				WAIT_BUS_READY_TX();
				pVSsp->ssdr = pu4DataBuff[j];
				WAIT_BUS_READY_TX();
				pVSsp->ssdr = pu4DataBuff[j + 1];
				WAIT_BUS_READY_TX();
				pVSsp->ssdr = pu4DataBuff[j + 2];
				WAIT_BUS_READY_TX();
				pVSsp->ssdr = pu4DataBuff[j + 3];
				WAIT_BUS_READY_TX();
				pVSsp->ssdr = pu4DataBuff[j + 4];
				WAIT_BUS_READY_TX();
				pVSsp->ssdr = pu4DataBuff[j + 5];
				WAIT_BUS_READY_TX();
				pVSsp->ssdr = pu4DataBuff[j + 6];
				WAIT_BUS_READY_TX();
				pVSsp->ssdr = pu4DataBuff[j + 7];
				WAIT_BUS_READY_TX();
				pVSsp->ssdr = pu4DataBuff[j + 8];
				WAIT_BUS_READY_TX();
				pVSsp->ssdr = pu4DataBuff[j + 9];
				WAIT_BUS_READY_TX();
				pVSsp->ssdr = pu4DataBuff[j + 10];
				WAIT_BUS_READY_TX();
				pVSsp->ssdr = pu4DataBuff[j + 11];
				WAIT_BUS_READY_TX();
				pVSsp->ssdr = pu4DataBuff[j + 12];
				WAIT_BUS_READY_TX();
				pVSsp->ssdr = pu4DataBuff[j + 13];
				WAIT_BUS_READY_TX();
				pVSsp->ssdr = pu4DataBuff[j + 14];
				WAIT_BUS_READY_TX();
				pVSsp->ssdr = pu4DataBuff[j + 15];
				j += 16;
			}
		}

		if ((u4Len32bit & 0x8) != 0) {
			WAIT_BUS_READY_TX();
			pVSsp->ssdr = pu4DataBuff[j];
			WAIT_BUS_READY_TX();
			pVSsp->ssdr = pu4DataBuff[j + 1];
			WAIT_BUS_READY_TX();
			pVSsp->ssdr = pu4DataBuff[j + 2];
			WAIT_BUS_READY_TX();
			pVSsp->ssdr = pu4DataBuff[j + 3];
			WAIT_BUS_READY_TX();
			pVSsp->ssdr = pu4DataBuff[j + 4];
			WAIT_BUS_READY_TX();
			pVSsp->ssdr = pu4DataBuff[j + 5];
			WAIT_BUS_READY_TX();
			pVSsp->ssdr = pu4DataBuff[j + 6];
			WAIT_BUS_READY_TX();
			pVSsp->ssdr = pu4DataBuff[j + 7];
			j += 8;
		}

		if ((u4Len32bit & 0x4) != 0) {
			WAIT_BUS_READY_TX();
			pVSsp->ssdr = pu4DataBuff[j];
			WAIT_BUS_READY_TX();
			pVSsp->ssdr = pu4DataBuff[j + 1];
			WAIT_BUS_READY_TX();
			pVSsp->ssdr = pu4DataBuff[j + 2];
			WAIT_BUS_READY_TX();
			pVSsp->ssdr = pu4DataBuff[j + 3];
			j += 4;
		}

		temp = (u4Len32bit & 0x3);
		while (temp--) {
			WAIT_BUS_READY_TX();
			pVSsp->ssdr = pu4DataBuff[j++];
		}

	} else {		/* read */

		WAIT_BUS_READY_TX();
		pVSsp->ssdr = 0xffffffff;	/* rsp */
		WAIT_BUS_READY_RX(u4Value);
		u4Value = pVSsp->ssdr;	/* for rsp */

		if ((u4Len32bit >> 4) > 0) {
			for (i; i < (u4Len32bit >> 4); i++) {
				WAIT_BUS_READY_TX();
				pVSsp->ssdr = 0xFFFFFFFF;
				pVSsp->ssdr = 0xFFFFFFFF;
				pVSsp->ssdr = 0xFFFFFFFF;
				pVSsp->ssdr = 0xFFFFFFFF;
				pVSsp->ssdr = 0xFFFFFFFF;

				pVSsp->ssdr = 0xFFFFFFFF;
				pVSsp->ssdr = 0xFFFFFFFF;
				pVSsp->ssdr = 0xFFFFFFFF;
				pVSsp->ssdr = 0xFFFFFFFF;
				pVSsp->ssdr = 0xFFFFFFFF;

				pVSsp->ssdr = 0xFFFFFFFF;
				pVSsp->ssdr = 0xFFFFFFFF;
				pVSsp->ssdr = 0xFFFFFFFF;
				pVSsp->ssdr = 0xFFFFFFFF;
				pVSsp->ssdr = 0xFFFFFFFF;

				pVSsp->ssdr = 0xFFFFFFFF;

				WAIT_BUS_READY_RX(u4Value);
				pu4DataBuff[j] = pVSsp->ssdr;
				WAIT_BUS_READY_RX(u4Value);
				pu4DataBuff[j + 1] = pVSsp->ssdr;
				WAIT_BUS_READY_RX(u4Value);
				pu4DataBuff[j + 2] = pVSsp->ssdr;
				WAIT_BUS_READY_RX(u4Value);
				pu4DataBuff[j + 3] = pVSsp->ssdr;
				WAIT_BUS_READY_RX(u4Value);
				pu4DataBuff[j + 4] = pVSsp->ssdr;

				WAIT_BUS_READY_RX(u4Value);
				pu4DataBuff[j + 5] = pVSsp->ssdr;
				WAIT_BUS_READY_RX(u4Value);
				pu4DataBuff[j + 6] = pVSsp->ssdr;
				WAIT_BUS_READY_RX(u4Value);
				pu4DataBuff[j + 7] = pVSsp->ssdr;
				WAIT_BUS_READY_RX(u4Value);
				pu4DataBuff[j + 8] = pVSsp->ssdr;
				WAIT_BUS_READY_RX(u4Value);
				pu4DataBuff[j + 9] = pVSsp->ssdr;

				WAIT_BUS_READY_RX(u4Value);
				pu4DataBuff[j + 10] = pVSsp->ssdr;
				WAIT_BUS_READY_RX(u4Value);
				pu4DataBuff[j + 11] = pVSsp->ssdr;
				WAIT_BUS_READY_RX(u4Value);
				pu4DataBuff[j + 12] = pVSsp->ssdr;
				WAIT_BUS_READY_RX(u4Value);
				pu4DataBuff[j + 13] = pVSsp->ssdr;
				WAIT_BUS_READY_RX(u4Value);
				pu4DataBuff[j + 14] = pVSsp->ssdr;

				WAIT_BUS_READY_RX(u4Value);
				pu4DataBuff[j + 15] = pVSsp->ssdr;
				j += 16;
			}
		}

		if ((u4Len32bit & 0x8) != 0) {
			WAIT_BUS_READY_TX();
			pVSsp->ssdr = 0xFFFFFFFF;
			pVSsp->ssdr = 0xFFFFFFFF;
			pVSsp->ssdr = 0xFFFFFFFF;
			pVSsp->ssdr = 0xFFFFFFFF;
			pVSsp->ssdr = 0xFFFFFFFF;

			pVSsp->ssdr = 0xFFFFFFFF;
			pVSsp->ssdr = 0xFFFFFFFF;
			pVSsp->ssdr = 0xFFFFFFFF;

			WAIT_BUS_READY_RX(u4Value);
			pu4DataBuff[j] = pVSsp->ssdr;
			WAIT_BUS_READY_RX(u4Value);
			pu4DataBuff[j + 1] = pVSsp->ssdr;
			WAIT_BUS_READY_RX(u4Value);
			pu4DataBuff[j + 2] = pVSsp->ssdr;
			WAIT_BUS_READY_RX(u4Value);
			pu4DataBuff[j + 3] = pVSsp->ssdr;
			WAIT_BUS_READY_RX(u4Value);
			pu4DataBuff[j + 4] = pVSsp->ssdr;

			WAIT_BUS_READY_RX(u4Value);
			pu4DataBuff[j + 5] = pVSsp->ssdr;
			WAIT_BUS_READY_RX(u4Value);
			pu4DataBuff[j + 6] = pVSsp->ssdr;
			WAIT_BUS_READY_RX(u4Value);
			pu4DataBuff[j + 7] = pVSsp->ssdr;
			j += 8;
		}
		if ((u4Len32bit & 0x4) != 0) {
			WAIT_BUS_READY_TX();
			pVSsp->ssdr = 0xFFFFFFFF;
			pVSsp->ssdr = 0xFFFFFFFF;
			pVSsp->ssdr = 0xFFFFFFFF;
			pVSsp->ssdr = 0xFFFFFFFF;

			WAIT_BUS_READY_RX(u4Value);
			pu4DataBuff[j] = pVSsp->ssdr;
			WAIT_BUS_READY_RX(u4Value);
			pu4DataBuff[j + 1] = pVSsp->ssdr;
			WAIT_BUS_READY_RX(u4Value);
			pu4DataBuff[j + 2] = pVSsp->ssdr;
			WAIT_BUS_READY_RX(u4Value);
			pu4DataBuff[j + 3] = pVSsp->ssdr;
			j += 4;
		}

		temp = (u4Len32bit & 0x3);
		while (temp--) {
			WAIT_BUS_READY_TX();
			pVSsp->ssdr = 0xFFFFFFFF;
			WAIT_BUS_READY_RX(u4Value);
			pu4DataBuff[j++] = pVSsp->ssdr;
		}

	}
#endif

	WAIT_BUS_DONE();
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to set spi bus width.
*
* \param[in] u4BusSize - Bus size.
*
* \retval 0 TRUE
* \retval -1 FALSE
*/
/*----------------------------------------------------------------------------*/
static INT_32 SpiSetBusWidth(UINT_32 u4BusSize)
{
	/* if size is not valid, return with size unchanged */
	/* if (u4BusSize > 32 || u4BusSize < 4) */
	if (u4BusSize != 32 && u4BusSize != 8)
		return -1;

	/* 1) disable port */
	pVSsp->sscr0 &= ~SSCR0_SSE;

	/* 2) clear size field */
	pVSsp->sscr0 &= ~(SSCR0_DSS_32BIT | SSCR0_EDSS);

	/* MUST restore SSCR1 value after disable SSE */
	pVSsp->sscr1 = SSCR1_SPH | SSCR1_SPO | (3 << 6) | (0 << 10);

	/* 3) set size and enable port */
	pVSsp->sscr0 |= (SSCR0_DSS_SIZE(u4BusSize) | SSCR0_SSE);

	return 0;
}				/* SpiSetBusWidth */


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to set operation mode to n-bit mode and Endian.
*
* \param[in] u4BusSize - Bus size.
*
* \return none
*/
/*----------------------------------------------------------------------------*/
static VOID SpiSetOpMode(VOID)
{
#if !CONFIG_SPI_8_BIT_MODE
	UINT_32 u4Value = 0;
#endif

	/* Set bus to 32-bit mode */
#if CONFIG_SPI_8_BIT_MODE
	if (SpiSetBusWidth(8) != 0) {
		ASSERT(FALSE);
		return;
	}
	DBGLOG(INIT, TRACE, ("Set bus to 8-bit mode\n"));
#else
	if (SpiSetBusWidth(32) != 0) {
		ASSERT(FALSE);
		return;
	}
	DBGLOG(INIT, TRACE, ("Set bus to 32-bit mode\n"));
#endif

#if !CONFIG_SPI_8_BIT_MODE
	/* SPI CSR set from 8 bit mode to 32 bit mode, notice the byte order */
	/* Set this after set bus to 32-bit mode */
	WAIT_BUS_READY_TX();
	pVSsp->ssdr = SPICSR_32BIT_MODE_ADDR;
	WAIT_BUS_READY_RX(u4Value);
	if (u4Value != 0) {
		(void)pVSsp->ssdr;
	} else {
		ASSERT(FALSE);
	}

	WAIT_BUS_READY_TX();
	pVSsp->ssdr = SPICSR_32BIT_MODE_DATA;
	WAIT_BUS_READY_RX(u4Value);
	if (u4Value != 0) {
		(void)pVSsp->ssdr;
	} else {
		ASSERT(FALSE);
	}
#endif

}				/* SpiSetOpMode */


/*----------------------------------------------------------------------------*/
/*!
* \brief Modify GP DDR For Input Direction
*                  aGpioPinArray[] = array of GPIO pins,
*                  aGpioPinArray[0] = size of array
*
* \param[in] aGpioPinArray[] - rray of GPIO pins.
*
* \return none
*/
/*----------------------------------------------------------------------------*/
static VOID GpioSetDirectionIn(P_GPIO_REG prGPIO, UINT_32 au4GpioPinArray[]
    )
{
	UINT_32 u4GpioPinMask;
	UINT_32 u4SizeArray;
	UINT_32 u4Mask0, u4Mask1, u4Mask2, u4Mask3;
	BOOL fgSet0, fgSet1, fgSet2, fgSet3;
	UINT_32 i;

	ASSERT(prGPIO);
	ASSERT(au4GpioPinArray);

	/* determine size of array */
	u4SizeArray = au4GpioPinArray[0];
	u4Mask0 = u4Mask1 = u4Mask2 = u4Mask3 = 0;
	fgSet0 = fgSet1 = fgSet2 = fgSet3 = FALSE;

	for (i = 1; i <= u4SizeArray; i++) {
		u4GpioPinMask = 0x1u << (au4GpioPinArray[i] & 0x1F);
		if (au4GpioPinArray[i] > 95) {
			u4Mask3 |= u4GpioPinMask;
			fgSet3 = TRUE;
		} else if (au4GpioPinArray[i] > 63) {
			u4Mask2 |= u4GpioPinMask;
			fgSet2 = TRUE;
		} else if (au4GpioPinArray[i] > 31) {
			u4Mask1 |= u4GpioPinMask;
			fgSet1 = TRUE;
		} else {
			u4Mask0 |= u4GpioPinMask;
			fgSet0 = TRUE;
		}

	}

	if (fgSet3) {
		prGPIO->GPDR3 = ((prGPIO->GPDR3 & ~u4Mask3) & ~GPIO_PIN_RESERVED_BITS);
	}
	if (fgSet2) {
		prGPIO->GPDR2 = ((prGPIO->GPDR2) & ~u4Mask2);
	}
	if (fgSet1) {
		prGPIO->GPDR1 = ((prGPIO->GPDR1) & ~u4Mask1);
	}
	if (fgSet0) {
		prGPIO->GPDR0 = ((prGPIO->GPDR0) & ~u4Mask0);
	}
}				/* GpioSetDirectionIn */


/*----------------------------------------------------------------------------*/
/*!
* \brief Modify GP DDR For Output Direction,
*                    aGpioPinArray[]=array of GPIO pins,
*                    aGpioPinArray[0] = size of array
*
* \param[in] aGpioPinArray[] - array of GPIO pins
*
* \return none
*/
/*----------------------------------------------------------------------------*/
static VOID GpioSetDirectionOut(P_GPIO_REG prGPIO, UINT_32 au4GpioPinArray[]
    )
{
	UINT_32 u4GpioPinMask;
	UINT_32 u4SizeArray;
	UINT_32 u4Mask0, u4Mask1, u4Mask2, u4Mask3;
	BOOL fgSet0, fgSet1, fgSet2, fgSet3;
	UINT_32 i;

	ASSERT(prGPIO);
	ASSERT(au4GpioPinArray);

	/* determine size of array */
	u4SizeArray = au4GpioPinArray[0];
	u4Mask0 = u4Mask1 = u4Mask2 = u4Mask3 = 0;
	fgSet0 = fgSet1 = fgSet2 = fgSet3 = FALSE;

	for (i = 1; i <= u4SizeArray; i++) {
		u4GpioPinMask = 0x1u << (au4GpioPinArray[i] & 0x1F);
		if (au4GpioPinArray[i] > 95) {
			u4Mask3 |= u4GpioPinMask;
			fgSet3 = TRUE;
		} else if (au4GpioPinArray[i] > 63) {
			u4Mask2 |= u4GpioPinMask;
			fgSet2 = TRUE;
		} else if (au4GpioPinArray[i] > 31) {
			u4Mask1 |= u4GpioPinMask;
			fgSet1 = TRUE;
		} else {
			u4Mask0 |= u4GpioPinMask;
			fgSet0 = TRUE;
		}
	}
	if (fgSet3) {
		prGPIO->GPDR3 = ((prGPIO->GPDR3 | u4Mask3) & ~GPIO_PIN_RESERVED_BITS);
	}
	if (fgSet2) {
		prGPIO->GPDR2 = ((prGPIO->GPDR2) | u4Mask2);
	}
	if (fgSet1) {
		prGPIO->GPDR1 = ((prGPIO->GPDR1) | u4Mask1);
	}
	if (fgSet0) {
		prGPIO->GPDR0 = ((prGPIO->GPDR0) | u4Mask0);
	}
}				/* GpioSetDirectionOut */


/*----------------------------------------------------------------------------*/
/*!
* \brief Set GPIO pins alternate function values,
*                        aGpioPinArray[]=array of GPIO pins,
*                        aGpioPinArray[0] = size of array
*
*                        aAfValueArray[]=array of GPIO pins alternate function values,
*                        aAfValueArray[0] = size of array
* \param[in] aGpioPinArray[] - array of GPIO pins
* \param[in] aAfValueArray[] - array of GPIO pins alternate function values
*
* \return none
* \note: IMPORTANT:THE ORDER OF aAfValueArray[] HAS TO MATCH THE ORDER OF aGpioPinArray[]
*/
/*----------------------------------------------------------------------------*/
static VOID GpioSetAlternateFn(P_GPIO_REG prGPIO, UINT_32 au4GpioPinArray[], UINT_32 aAfValueArray[]
    )
{
	UINT_32 u4GpioPinAFMask;
	UINT_32 u4GpioPinAFValue;
	UINT_32 u4SizeArray;
	UINT_32 u4Mask0_U, u4Mask0_L, u4Mask1_U, u4Mask1_L;
	UINT_32 u4Mask2_U, u4Mask2_L, u4Mask3_U, u4Mask3_L;
	UINT_32 u4AFnV0_U, u4AFnV0_L, u4AFnV1_U, u4AFnV1_L;
	UINT_32 u4AFnV2_U, u4AFnV2_L, u4AFnV3_U, u4AFnV3_L;
	BOOL fgSet0_U, fgSet0_L, fgSet1_U, fgSet1_L;
	BOOL fgSet2_U, fgSet2_L, fgSet3_U, fgSet3_L;
	UINT_32 i;

	ASSERT(prGPIO);
	ASSERT(au4GpioPinArray);
	ASSERT(aAfValueArray);

	/* determine size of array */
	u4SizeArray = au4GpioPinArray[0];
	u4Mask0_U = u4Mask0_L = u4Mask1_U = u4Mask1_L = 0;
	u4Mask2_U = u4Mask2_L = u4Mask3_U = u4Mask3_L = 0;
	u4AFnV0_U = u4AFnV0_L = u4AFnV1_U = u4AFnV1_L = 0;
	u4AFnV2_U = u4AFnV2_L = u4AFnV3_U = u4AFnV3_L = 0;
	fgSet0_U = fgSet0_L = fgSet1_U = fgSet1_L = FALSE;
	fgSet2_U = fgSet2_L = fgSet3_U = fgSet3_L = FALSE;

	for (i = 1; i <= u4SizeArray; i++) {
		u4GpioPinAFMask = 0x3u << ((au4GpioPinArray[i] & 0xF) * 2);
		u4GpioPinAFValue = aAfValueArray[i] << ((au4GpioPinArray[i] & 0xF) * 2);
		if (au4GpioPinArray[i] > 111) {
			u4AFnV3_U |= u4GpioPinAFValue;
			u4Mask3_U |= u4GpioPinAFMask;
			fgSet3_U = TRUE;
		} else if (au4GpioPinArray[i] > 95) {
			u4AFnV3_L |= u4GpioPinAFValue;
			u4Mask3_L |= u4GpioPinAFMask;
			fgSet3_L = TRUE;
		} else if (au4GpioPinArray[i] > 79) {
			u4AFnV2_U |= u4GpioPinAFValue;
			u4Mask2_U |= u4GpioPinAFMask;
			fgSet2_U = TRUE;
		} else if (au4GpioPinArray[i] > 63) {
			u4AFnV2_L |= u4GpioPinAFValue;
			u4Mask2_L |= u4GpioPinAFMask;
			fgSet2_L = TRUE;
		} else if (au4GpioPinArray[i] > 47) {
			u4AFnV1_U |= u4GpioPinAFValue;
			u4Mask1_U |= u4GpioPinAFMask;
			fgSet1_U = TRUE;
		} else if (au4GpioPinArray[i] > 31) {
			u4AFnV1_L |= u4GpioPinAFValue;
			u4Mask1_L |= u4GpioPinAFMask;
			fgSet1_L = TRUE;
		} else if (au4GpioPinArray[i] > 15) {
			u4AFnV0_U |= u4GpioPinAFValue;
			u4Mask0_U |= u4GpioPinAFMask;
			fgSet0_U = TRUE;
		} else {
			u4AFnV0_L |= u4GpioPinAFValue;
			u4Mask0_L |= u4GpioPinAFMask;
			fgSet0_L = TRUE;
		}
	}
	if (fgSet3_U) {
		prGPIO->GAFR3_U =
		    (((prGPIO->GAFR3_U & ~u4Mask3_U) | u4AFnV3_U) & ~GPIO_ALT_RESERVED_BITS);
	}
	if (fgSet3_L) {
		prGPIO->GAFR3_L = ((prGPIO->GAFR3_L & ~u4Mask3_L) | u4AFnV3_L);
	}
	if (fgSet2_U) {
		prGPIO->GAFR2_U = ((prGPIO->GAFR2_U & ~u4Mask2_U) | u4AFnV2_U);
	}
	if (fgSet2_L) {
		prGPIO->GAFR2_L = ((prGPIO->GAFR2_L & ~u4Mask2_L) | u4AFnV2_L);
	}
	if (fgSet1_U) {
		prGPIO->GAFR1_U = ((prGPIO->GAFR1_U & ~u4Mask1_U) | u4AFnV1_U);
	}
	if (fgSet1_L) {
		prGPIO->GAFR1_L = ((prGPIO->GAFR1_L & ~u4Mask1_L) | u4AFnV1_L);
	}
	if (fgSet0_U) {
		prGPIO->GAFR0_U = ((prGPIO->GAFR0_U & ~u4Mask0_U) | u4AFnV0_U);
	}
	if (fgSet0_L) {
		prGPIO->GAFR0_L = ((prGPIO->GAFR0_L & ~u4Mask0_L) | u4AFnV0_L);
	}
}				/* GpioSetAlternateFn */


/*----------------------------------------------------------------------------*/
/*!
* \brief Enable Falling Edge Detect,
*                    aGpioPinArray[]=array of GPIO pins,
*                    aGpioPinArray[0] = size of array
* \param[in] aGpioPinArray[] - array of GPIO pins
*
* \return none
*/
/*----------------------------------------------------------------------------*/
static VOID GpioSetFallingEdgeDetectEnable(P_GPIO_REG prGPIO, UINT_32 au4GpioPinArray[]
    )
{
	UINT_32 u4GpioPinMask;
	UINT_32 u4SizeArray;
	UINT_32 u4Mask0, u4Mask1, u4Mask2, u4Mask3;
	BOOL fgSet0, fgSet1, fgSet2, fgSet3;
	UINT_32 i;

	ASSERT(prGPIO);
	ASSERT(au4GpioPinArray);

	/* determine size of array */
	u4SizeArray = au4GpioPinArray[0];
	u4Mask0 = u4Mask1 = u4Mask2 = u4Mask3 = 0;
	fgSet0 = fgSet1 = fgSet2 = fgSet3 = FALSE;

	for (i = 1; i <= u4SizeArray; i++) {
		u4GpioPinMask = 0x1u << (au4GpioPinArray[i] & 0x1F);
		if (au4GpioPinArray[i] > 95) {
			u4Mask3 |= u4GpioPinMask;
			fgSet3 = TRUE;
		} else if (au4GpioPinArray[i] > 63) {
			u4Mask2 |= u4GpioPinMask;
			fgSet2 = TRUE;
		} else if (au4GpioPinArray[i] > 31) {
			u4Mask1 |= u4GpioPinMask;
			fgSet1 = TRUE;
		} else {
			u4Mask0 |= u4GpioPinMask;
			fgSet0 = TRUE;
		}
	}
	if (fgSet3) {
		prGPIO->GFER3 = ((prGPIO->GFER3 | u4Mask3) & ~GPIO_PIN_RESERVED_BITS);
	}
	if (fgSet2) {
		prGPIO->GFER2 = ((prGPIO->GFER2) | u4Mask2);
	}
	if (fgSet1) {
		prGPIO->GFER1 = ((prGPIO->GFER1) | u4Mask1);
	}
	if (fgSet0) {
		prGPIO->GFER0 = ((prGPIO->GFER0) | u4Mask0);
	}
}				/* GpioSetFallingEdgeDetectEnable */


/*----------------------------------------------------------------------------*/
/*!
* \brief Enable Initiate SPI interface for Intel PXA270
*
* \param[in] none
*
* \return none
*/
/*----------------------------------------------------------------------------*/
void platformBusInit(IN P_GLUE_INFO_T prGlueInfo)
{
	P_GL_HIF_INFO_T prHifInfo;
	UINT_32 u4Gpio, u4Irq = 0;

	/* Memory Map */
	NDIS_PHYSICAL_ADDRESS CLKMGR_Base = { CLKMGR_BASE_ADD };	/* PA Clock Manager */
	NDIS_PHYSICAL_ADDRESS SSP_Base = { SSP_BASE_ADD };	/* SSP */
	NDIS_PHYSICAL_ADDRESS GPIO_Base = { GPIO_BASE_ADD };	/* GPIO */

	/* Set GPIO mode */
	UINT_32 GpioDirOutList[] = { 3, GPIO23_SCLK, GPIO24_SFRM, GPIO25_STXD };
	UINT_32 GpioDirInList[] = { 2, GPIO26_SRXD, GPIO_INTR };
	UINT_32 GpioAltFnPinList[] =
	    { 5, GPIO23_SCLK, GPIO24_SFRM, GPIO25_STXD, GPIO26_SRXD, GPIO_INTR };
	UINT_32 GpioAltFnValList[] = { 5, 2, 2, 2, 1, 1 };
	UINT_32 GpioAltFnIntPinList[] = { 1, GPIO_INTR };	/* interrupt */

	DEBUGFUNC("windowsFindAdapter");

	ASSERT(prGlueInfo);

	prHifInfo = &prGlueInfo->rHifInfo;

	/* Memory Map */
	pVClkMgr = (CLK_MGR *) MmMapIoSpace(CLKMGR_Base, sizeof(CLK_MGR), FALSE);

	pVSsp = (SSP_REG *) MmMapIoSpace(SSP_Base, sizeof(SSP_REG), FALSE);

	pVMem = (GPIO_REG *) MmMapIoSpace(GPIO_Base, sizeof(GPIO_REG), FALSE);

	/* Backup GPIO config for restore when unload driver */
	GPIOConfigBackup();

	/* The InterruptVector is the interrupt line that the card asserts to
	   interrupt the system, and the InterruptLevel is ignored on Windows CE */
	u4Gpio = GPIO_INTR;
	KernelIoControl(IOCTL_HAL_GPIO2IRQ, (LPVOID) & u4Gpio, 1, (LPVOID) & u4Irq, 1, NULL);
	prHifInfo->u4InterruptVector = u4Irq;	/* IRQ    : 27:0x7a */
	prHifInfo->u4InterruptLevel = u4Irq;	/* ignore : 27:0x7a */
	DbgPrint("GPIO INTR %d %d %x\n", GPIO_INTR, u4Gpio, u4Irq);

	if (pVMem) {
		/* set GPIO mode */
		GpioSetDirectionIn((P_GPIO_REG) pVMem, GpioDirInList);
		GpioSetDirectionOut((P_GPIO_REG) pVMem, GpioDirOutList);
		GpioSetAlternateFn((P_GPIO_REG) pVMem, GpioAltFnPinList, GpioAltFnValList);
		GpioSetFallingEdgeDetectEnable((P_GPIO_REG) pVMem, GpioAltFnIntPinList);	/* interrupt */
	}

	if (pVSsp) {
		/* Configure SSP */
		pVClkMgr->cken |= CLKEN_SSP;
		pVSsp->sssr = 0;

		pVSsp->sscr1 = 0;
		pVSsp->sscr1 = SSCR1_SPH | SSCR1_SPO | SSCR1_TTE;	/* fifi TTE */
		pVSsp->sscr1 |= (3 << 6) | (0 << 10);	/* TFT, RFT */

		pVSsp->sscr0 = 0;
#if CONFIG_SPI_8_BIT_MODE
		pVSsp->sscr0 = SSCR0_DSS_8BIT | SSCR0_RIM;
#else
		pVSsp->sscr0 = SSCR0_EDSS | SSCR0_DSS_32BIT | SSCR0_RIM;
#endif
		pVSsp->sscr0 |= SSCR0_SSE;	/* operation enable at least */
	}

	/* Set operation mode */
	/* Intel PXA27x supports 4~32-bits mode */
	SpiSetOpMode();

}				/* platformBusInit */


/*----------------------------------------------------------------------------*/
/*!
* \brief The PROC function for backup GPIO configuration.
*
* This service save the preveous config of GPIO we will use.
* This procedure only present when initialization.
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID GPIOConfigBackup(VOID)
{
	IO_GPDR0 = pVMem->GPDR0 & (BITS(23, 26) | IO_MASK_INTR);	/* 23,24,25,26,intr */
	Fun_GAFR0_U = pVMem->GAFR0_U & (BITS(14, 21) | FUN_MASK_INTR);	/* 23,24,25,26,intr */
}				/* BackupGPIOConfig */


/*----------------------------------------------------------------------------*/
/*!
* \brief The PROC function for restore GPIO configuration.
*
* This service restore the preveous config of GPIO.
* This procedure only present when unload driver.
*
* \return  (none)
*/
/*----------------------------------------------------------------------------*/
VOID GPIOConfigRestore(VOID)
{
	UINT_32 tempA = 0, tempB = 0;

	tempA = IO_GPDR0 & (BITS(23, 26) | IO_MASK_INTR);	/* 23,24,25,26,intr */
	tempB = pVMem->GPDR0 & ~(BITS(23, 26) | IO_MASK_INTR);
	pVMem->GPDR0 = tempA | tempB;

	tempA = Fun_GAFR0_U & (BITS(14, 21) | FUN_MASK_INTR);	/* 23,24,25,26,intr */
	tempB = pVMem->GAFR0_U & ~(BITS(14, 21) | FUN_MASK_INTR);
	pVMem->GAFR0_U = tempA | tempB;
}				/* RestoreGPIOConfig */
