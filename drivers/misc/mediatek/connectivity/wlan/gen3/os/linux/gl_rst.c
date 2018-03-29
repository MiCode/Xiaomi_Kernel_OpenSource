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
** Id: @(#) gl_rst.c@@
*/

/*! \file   gl_rst.c
    \brief  Main routines for supporintg MT6620 whole-chip reset mechanism

    This file contains the support routines of Linux driver for MediaTek Inc. 802.11
    Wireless LAN Adapters.
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

#if CFG_CHIP_RESET_SUPPORT

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/
static BOOLEAN fgResetTriggered = FALSE;
BOOLEAN fgIsResetting = FALSE;
UINT32 g_IsNeedDoChipReset;

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
static void *glResetCallback(ENUM_WMTDRV_TYPE_T eSrcType,
			     ENUM_WMTDRV_TYPE_T eDstType,
			     ENUM_WMTMSG_TYPE_T eMsgType, void *prMsgBody, unsigned int u4MsgLength);

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
#if (defined(MT6797) && (MTK_WCN_SINGLE_MODULE == 0)) || defined(MT6630)
	/* 1. Register reset callback */
	mtk_wcn_wmt_msgcb_reg(WMTDRV_TYPE_WIFI, (PF_WMT_CB) glResetCallback);
#endif
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
#if (defined(MT6797) && (MTK_WCN_SINGLE_MODULE == 0)) || defined(MT6630)
	/* 1. Deregister reset callback */
	mtk_wcn_wmt_msgcb_unreg(WMTDRV_TYPE_WIFI);
#endif
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
static void *glResetCallback(ENUM_WMTDRV_TYPE_T eSrcType,
			     ENUM_WMTDRV_TYPE_T eDstType,
			     ENUM_WMTMSG_TYPE_T eMsgType, void *prMsgBody, unsigned int u4MsgLength)
{
	switch (eMsgType) {
	case WMTMSG_TYPE_RESET:
		if (u4MsgLength == sizeof(ENUM_WMTRSTMSG_TYPE_T)) {
			P_ENUM_WMTRSTMSG_TYPE_T prRstMsg = (P_ENUM_WMTRSTMSG_TYPE_T) prMsgBody;

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
BOOLEAN kalIsResetting(VOID)
{
	return fgIsResetting;
}

static void mtk_wifi_trigger_reset(struct work_struct *work)
{
	BOOLEAN fgResult = FALSE;

	fgResetTriggered = TRUE;
	fgResult = mtk_wcn_wmt_assert(WMTDRV_TYPE_WIFI, 0x40);
	DBGLOG(INIT, INFO, "reset result %d\n", fgResult);
}

BOOLEAN glResetTrigger(P_ADAPTER_T prAdapter)
{
	BOOLEAN fgResult = TRUE;

#if CFG_WMT_RESET_API_SUPPORT
	if (kalIsResetting() || fgResetTriggered) {
		DBGLOG(INIT, ERROR,
			"Skip triggering whole-chip reset during resetting! Chip[%04X E%u]\n",
			MTK_CHIP_REV,
			wlanGetEcoVersion(prAdapter));
		DBGLOG(INIT, ERROR,
			"FW Ver DEC[%u.%u] HEX[%x.%x], Driver Ver[%u.%u]\n",
			(prAdapter->rVerInfo.u2FwOwnVersion >> 8),
			(prAdapter->rVerInfo.u2FwOwnVersion & BITS(0, 7)),
			(prAdapter->rVerInfo.u2FwOwnVersion >> 8),
			(prAdapter->rVerInfo.u2FwOwnVersion & BITS(0, 7)),
			(prAdapter->rVerInfo.u2FwPeerVersion >> 8),
			(prAdapter->rVerInfo.u2FwPeerVersion & BITS(0, 7)));

		fgResult = TRUE;
	} else {
		DBGLOG(INIT, ERROR,
		"Trigger whole-chip reset! Chip[%04X E%u] FW Ver DEC[%u.%u] HEX[%x.%x], Driver Ver[%u.%u]\n",
			     MTK_CHIP_REV,
			     wlanGetEcoVersion(prAdapter),
			     (prAdapter->rVerInfo.u2FwOwnVersion >> 8),
			     (prAdapter->rVerInfo.u2FwOwnVersion & BITS(0, 7)),
			     (prAdapter->rVerInfo.u2FwOwnVersion >> 8),
			     (prAdapter->rVerInfo.u2FwOwnVersion & BITS(0, 7)),
			     (prAdapter->rVerInfo.u2FwPeerVersion >> 8),
			     (prAdapter->rVerInfo.u2FwPeerVersion & BITS(0, 7)));

		schedule_work(&(wifi_rst.rst_trigger_work));
	}
#endif

	return fgResult;
}

ENUM_CHIP_RESET_REASON_TYPE_T eResetReason;
UINT_64 u8ResetTime;
VOID glGetRstReason(ENUM_CHIP_RESET_REASON_TYPE_T eReason)
{
	u8ResetTime = sched_clock();
	eResetReason = eReason;
}

#endif /* CFG_CHIP_RESET_SUPPORT */
