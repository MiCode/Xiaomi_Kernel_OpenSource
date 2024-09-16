/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify it under the terms of the
* GNU General Public License version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

/*
 * Id: @(#) gl_rst.c@@
 */

/*
 * ! \file   gl_rst.c
 *  \brief  Main routines for supporintg MT6620 whole-chip reset mechanism
 *
 *   This file contains the support routines of Linux driver for MediaTek Inc. 802.11
 *   Wireless LAN Adapters.
 */

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include <linux/kernel.h>
#include <linux/workqueue.h>

#include "precomp.h"
#include "gl_rst.h"



/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/
static BOOLEAN fgResetTriggered = FALSE;
static BOOLEAN fgIsResetting = FALSE;

#if CFG_CHIP_RESET_SUPPORT

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/
static RESET_STRUCT_T wifi_rst;

static void mtk_wifi_reset(struct work_struct *work);
static void mtk_wifi_trigger_reset(struct work_struct *work);

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
static void *glResetCallback(enum ENUM_WMTDRV_TYPE eSrcType,
			     enum ENUM_WMTDRV_TYPE eDstType,
			     enum ENUM_WMTMSG_TYPE eMsgType, void *prMsgBody, unsigned int u4MsgLength);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for
 *        1. register wifi reset callback
 *        2. initialize wifi reset work
 *
 * @param none
 *
 * @retval none
 */
/*----------------------------------------------------------------------------*/
VOID glResetInit(VOID)
{
	/* 1. Register reset callback */
	mtk_wcn_wmt_msgcb_reg(WMTDRV_TYPE_WIFI, (PF_WMT_CB) glResetCallback);

	/* 2. Initialize reset work */
	INIT_WORK(&(wifi_rst.rst_work), mtk_wifi_reset);
	INIT_WORK(&(wifi_rst.rst_trigger_work), mtk_wifi_trigger_reset);
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is responsible for
 *        1. deregister wifi reset callback
 *
 * @param none
 *
 * @retval none
 */
/*----------------------------------------------------------------------------*/
VOID glResetUninit(VOID)
{
	/* 1. Deregister reset callback */
	mtk_wcn_wmt_msgcb_unreg(WMTDRV_TYPE_WIFI);
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is invoked when there is reset messages indicated
 *
 * @param   eSrcType
 *          eDstType
 *          eMsgType
 *          prMsgBody
 *          u4MsgLength
 *
 * @retval
 */
/*----------------------------------------------------------------------------*/
static void *glResetCallback(enum ENUM_WMTDRV_TYPE eSrcType,
			     enum ENUM_WMTDRV_TYPE eDstType,
			     enum ENUM_WMTMSG_TYPE eMsgType, void *prMsgBody, unsigned int u4MsgLength)
{
	switch (eMsgType) {
	case WMTMSG_TYPE_RESET:
		if (u4MsgLength == sizeof(enum ENUM_WMTRSTMSG_TYPE)) {
			enum ENUM_WMTRSTMSG_TYPE *prRstMsg = (enum ENUM_WMTRSTMSG_TYPE *) prMsgBody;

			switch (*prRstMsg) {
			case WMTRSTMSG_RESET_START:
				DBGLOG(INIT, WARN, "Whole chip reset start!\n");
				fgIsResetting = TRUE;
				fgResetTriggered = FALSE;
				wifi_reset_start();
				break;

			case WMTRSTMSG_RESET_END:
				DBGLOG(INIT, WARN, "Whole chip reset end!\n");
				fgIsResetting = FALSE;
				wifi_rst.rst_data = RESET_SUCCESS;
				schedule_work(&(wifi_rst.rst_work));
				break;

			case WMTRSTMSG_RESET_END_FAIL:
				DBGLOG(INIT, WARN, "Whole chip reset fail!\n");
				fgIsResetting = FALSE;
				wifi_rst.rst_data = RESET_FAIL;
				schedule_work(&(wifi_rst.rst_work));
				break;

			default:
				break;
			}
		}
		break;

	default:
		break;
	}

	return NULL;
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is called for wifi reset
 *
 * @param   skb
 *          info
 *
 * @retval  0
 *          nonzero
 */
/*----------------------------------------------------------------------------*/
static void mtk_wifi_reset(struct work_struct *work)
{
	RESET_STRUCT_T *rst = container_of(work, RESET_STRUCT_T, rst_work);

	wifi_reset_end(rst->rst_data);
}

/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is called for generating reset request to WMT
 *
 * @param   None
 *
 * @retval  None
 */
/*----------------------------------------------------------------------------*/
VOID glSendResetRequest(VOID)
{
	/* WMT thread would trigger whole chip reset itself */
}



static void mtk_wifi_trigger_reset(struct work_struct *work)
{
	BOOLEAN fgResult = FALSE;
	RESET_STRUCT_T *rst = container_of(work, RESET_STRUCT_T, rst_trigger_work);

	/* Set the power off flag to FALSE in WMT to prevent chip power off after
	** wlanProbe return failure, because we need to do core dump afterward.
	*/
	if (rst->rst_trigger_flag & RST_FLAG_PREVENT_POWER_OFF)
		mtk_wcn_set_connsys_power_off_flag(FALSE);
	if ((rst->rst_trigger_flag & RST_FLAG_DO_CORE_DUMP) && !fgIsBusAccessFailed)
		fgResult = mtk_wcn_wmt_assert_timeout(WMTDRV_TYPE_WIFI, 0x40, 0);
	else
		fgResult = mtk_wcn_wmt_do_reset(WMTDRV_TYPE_WIFI);
	DBGLOG(INIT, INFO, "reset result %d, trigger flag 0x%x\n", fgResult, rst->rst_trigger_flag);
}

BOOLEAN glResetTrigger(P_ADAPTER_T prAdapter, UINT_32 u4RstFlag, const PUINT_8 pucFile, UINT_32 u4Line)
{
	BOOLEAN fgResult = TRUE;

	if (fgIsResetting || fgResetTriggered) {
		DBGLOG(INIT, ERROR,
		       "Skip trigger whole-chip reset in %s line %u, during resetting! Chip[%04X E%u]\n",
		       pucFile, u4Line,
		       nicGetChipID(prAdapter),
		       wlanGetEcoVersion(prAdapter));
		DBGLOG(INIT, ERROR,
		       "FW Ver DEC[%u.%u] HEX[%x.%x], Driver Ver[%u.%u]\n",
		       (prAdapter->rVerInfo.u2FwOwnVersion >> 8),
		       (UINT_16)(prAdapter->rVerInfo.u2FwOwnVersion & BITS(0, 7)),
		       (prAdapter->rVerInfo.u2FwOwnVersion >> 8),
		       (UINT_16)(prAdapter->rVerInfo.u2FwOwnVersion & BITS(0, 7)),
		       (prAdapter->rVerInfo.u2FwPeerVersion >> 8),
		       (UINT_16)(prAdapter->rVerInfo.u2FwPeerVersion & BITS(0, 7)));

		fgResult = TRUE;
	} else {
		DBGLOG(INIT, ERROR,
		"Trigger chip reset in %s line %u! Chip[%04X E%u] FW Ver DEC[%u.%u] HEX[%x.%x], Driver Ver[%u.%u]\n",
			pucFile, u4Line,
			nicGetChipID(prAdapter),
			wlanGetEcoVersion(prAdapter),
		       (prAdapter->rVerInfo.u2FwOwnVersion >> 8),
		       (UINT_16)(prAdapter->rVerInfo.u2FwOwnVersion & BITS(0, 7)),
		       (prAdapter->rVerInfo.u2FwOwnVersion >> 8),
		       (UINT_16)(prAdapter->rVerInfo.u2FwOwnVersion & BITS(0, 7)),
		       (prAdapter->rVerInfo.u2FwPeerVersion >> 8),
		       (UINT_16)(prAdapter->rVerInfo.u2FwPeerVersion & BITS(0, 7)));

		fgResetTriggered = TRUE;
		wifi_rst.rst_trigger_flag = u4RstFlag;
		schedule_work(&(wifi_rst.rst_trigger_work));
	}

	return fgResult;
}

#endif /* CFG_CHIP_RESET_SUPPORT */
/*----------------------------------------------------------------------------*/
/*!
 * @brief This routine is called for checking if connectivity chip is resetting
 *
 * @param   None
 *
 * @retval  TRUE
 *          FALSE
 */
/*----------------------------------------------------------------------------*/
inline BOOLEAN kalIsResetting(VOID)
{
	return fgIsResetting;
}

inline BOOLEAN kalIsResetTriggered(VOID)
{
	return fgResetTriggered;
}

UINT32 wlanPollingCpupcr(UINT32 u4Times, UINT32 u4Sleep)
{
#if defined(MT6631)
	UINT32 u4Count;

	for (u4Count = 0; u4Count < u4Times; u4Count++) {
		DBGLOG(INIT, ERROR, "i:%d,cpupcr:%08x\n", u4Count, wmt_plat_read_cpupcr());
		kalMsleep(u4Sleep);
	}
#else
	DBGLOG(INIT, ERROR, "This chip don't support polling cpupcr\n");
#endif
	return 0;
}

WLAN_STATUS wlanGetCpupcr(PUINT32 pu4Cpupcr)
{
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;

#if defined(MT6631)
	if (pu4Cpupcr) {
		*pu4Cpupcr = wmt_plat_read_cpupcr();
		rStatus = WLAN_STATUS_SUCCESS;
	}
#else
	DBGLOG(INIT, ERROR, "This chip don't support polling cpupcr\n");
#endif
	return rStatus;
}
