/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

/*file: stp_btif, mainly control stp & btif interaction*/

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/
#ifdef DFT_TAG
#undef DFT_TAG
#endif
#define DFT_TAG "[STP-BTIF]"

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "osal_typedef.h"
#include "wmt_exp.h"
#include "stp_exp.h"
#include "stp_btif.h"

#include <asm/current.h>
/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#define BTIF_OWNER_NAME "CONSYS_STP"

#define STP_MAX_PACKAGE_ALLOWED (2000)

#define STP_BTIF_TX_RTY_LMT (10)
#define STP_BTIF_TX_RTY_DLY (5)
/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/
struct stp_btif g_stp_btif;
/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

INT32 mtk_wcn_consys_stp_btif_open(VOID)
{
	INT32 iRet = -1;
	P_OSAL_THREAD thread = &g_stp_btif.btif_thread;

	iRet = mtk_wcn_btif_open(BTIF_OWNER_NAME, &g_stp_btif.stpBtifId);
	if (iRet) {
		WMT_WARN_FUNC("STP open btif fail(%d)\n", iRet);
		return -1;
	}
	WMT_DBG_FUNC("STP open bitf OK\n");

	thread->pThread = mtk_btif_exp_rx_thread_get(g_stp_btif.stpBtifId);
	if (!thread->pThread) {
		WMT_INFO_FUNC("thread->pThread is NULL\n");
		return -1;
	}

	osal_strncpy(thread->threadName, thread->pThread->comm, sizeof(thread->pThread->comm));
	mtk_wcn_stp_register_if_tx(STP_BTIF_IF_TX, (MTK_WCN_STP_IF_TX) mtk_wcn_consys_stp_btif_tx);
	mtk_wcn_stp_register_rx_has_pending_data(STP_BTIF_IF_TX,
		(MTK_WCN_STP_RX_HAS_PENDING_DATA) mtk_wcn_consys_stp_btif_rx_has_pending_data);
	mtk_wcn_stp_register_tx_has_pending_data(STP_BTIF_IF_TX,
		(MTK_WCN_STP_TX_HAS_PENDING_DATA) mtk_wcn_consys_stp_btif_tx_has_pending_data);
	mtk_wcn_stp_register_rx_thread_get(STP_BTIF_IF_TX,
		(MTK_WCN_STP_RX_THREAD_GET) mtk_wcn_consys_stp_btif_rx_thread_get);

	return 0;
}

INT32 mtk_wcn_consys_stp_btif_close(VOID)
{
	INT32 iRet = 0;

	if (!g_stp_btif.stpBtifId)
		WMT_WARN_FUNC("NULL BTIF ID reference!\n");
	else {
		iRet = mtk_wcn_btif_close(g_stp_btif.stpBtifId);
		if (iRet) {
			WMT_WARN_FUNC("STP close btif fail(%d)\n", iRet);
			iRet = -2;
		} else {
			g_stp_btif.stpBtifId = 0;
			WMT_DBG_FUNC("STP close btif OK\n");
		}
	}

	return iRet;
}

INT32 mtk_wcn_consys_stp_btif_rx_cb_register(MTK_WCN_BTIF_RX_CB rx_cb)
{
	INT32 iRet = 0;

	if (!g_stp_btif.stpBtifId) {
		WMT_WARN_FUNC("NULL BTIF ID reference\n!");
		if (rx_cb)
			iRet = -1;
	} else {
		iRet = mtk_wcn_btif_rx_cb_register(g_stp_btif.stpBtifId, rx_cb);
		if (iRet) {
			WMT_WARN_FUNC("STP register rxcb to btif fail(%d)\n", iRet);
			iRet = -2;
		} else
			WMT_DBG_FUNC("STP register rxcb to  btif OK\n");
	}

	return iRet;
}

INT32 mtk_wcn_consys_stp_btif_tx(const PUINT8 pBuf, const UINT32 len, PUINT32 written_len)
{
	INT32 retry_left = STP_BTIF_TX_RTY_LMT;
	INT32 wr_count = 0;
	INT32 written = 0;

	if (!g_stp_btif.stpBtifId) {
		WMT_WARN_FUNC("NULL BTIF ID reference!\n");
		return -1;
	}

	if (len == 0) {
		*written_len = 0;
		WMT_INFO_FUNC("special case for STP-CORE,pbuf(%p)\n", pBuf);
		return 0;
	}

	*written_len = 0;

	if (len > STP_MAX_PACKAGE_ALLOWED) {
		WMT_WARN_FUNC("abnormal pacage length,len(%d),pid[%d/%s]\n", len, current->pid, current->comm);
		return -2;
	}
	wr_count = mtk_wcn_btif_write(g_stp_btif.stpBtifId, pBuf, len);

	if (wr_count < 0) {
		WMT_ERR_FUNC("mtk_wcn_btif_write err(%d)\n", wr_count);
		*written_len = 0;
		return -3;
	}
	if (wr_count == len) {
		/*perfect case */
		*written_len = wr_count;
		return wr_count;
	}

	while ((retry_left--) && (wr_count < len)) {
		osal_sleep_ms(STP_BTIF_TX_RTY_DLY);
		written = mtk_wcn_btif_write(g_stp_btif.stpBtifId, pBuf + wr_count, len - wr_count);
		if (written < 0) {
			WMT_ERR_FUNC("mtk_wcn_btif_write err(%d)when do recovered\n", written);
			break;
		}
		wr_count += written;
	}

	if (wr_count == len) {
		WMT_INFO_FUNC("recovered,len(%d),retry_left(%d)\n", len, retry_left);
		/*recovered case */
		*written_len = wr_count;
		return wr_count;
	}

	WMT_ERR_FUNC("stp btif write fail,len(%d),written(%d),retry_left(%d),pid[%d/%s]\n",
		     len, wr_count, retry_left, current->pid, current->comm);
	*written_len = 0;

	return -wr_count;
}

INT32 mtk_wcn_consys_stp_btif_rx(PUINT8 pBuf, UINT32 len)
{
	return 0;
}

INT32 mtk_wcn_consys_stp_btif_wakeup(VOID)
{
	INT32 iRet = 0;

	if (!g_stp_btif.stpBtifId) {
		WMT_WARN_FUNC("NULL BTIF ID reference!\n");
		iRet = -1;
	} else {
		iRet = mtk_wcn_btif_wakeup_consys(g_stp_btif.stpBtifId);
		if (iRet) {
			WMT_WARN_FUNC("STP btif wakeup consys fail(%d)\n", iRet);
			iRet = -2;
		} else
			WMT_DBG_FUNC("STP btif wakeup consys ok\n");
	}

	return iRet;
}

INT32 mtk_wcn_consys_stp_btif_dpidle_ctrl(UINT32 en_flag)
{
	INT32 iRet = 0;

	if (!g_stp_btif.stpBtifId) {
		WMT_WARN_FUNC("NULL BTIF ID reference!\n");
		iRet = -1;
	} else {
		mtk_wcn_btif_dpidle_ctrl(g_stp_btif.stpBtifId, (enum _ENUM_BTIF_DPIDLE_) en_flag);
		WMT_DBG_FUNC("stp btif dpidle ctrl done,en_flag(%d)\n", en_flag);
	}

	return iRet;
}

INT32 mtk_wcn_consys_stp_btif_lpbk_ctrl(enum _ENUM_BTIF_LPBK_MODE_ mode)
{
	INT32 iRet = 0;

	if (!g_stp_btif.stpBtifId) {
		WMT_WARN_FUNC("NULL BTIF ID reference!\n");
		iRet = -1;
	} else {
		iRet = mtk_wcn_btif_loopback_ctrl(g_stp_btif.stpBtifId, mode);
		if (iRet) {
			WMT_WARN_FUNC("STP btif lpbk ctrl fail(%d)\n", iRet);
			iRet = -2;
		} else
			WMT_INFO_FUNC("stp btif lpbk ctrl ok,mode(%d)\n", mode);
	}

	return iRet;
}

INT32 mtk_wcn_consys_stp_btif_logger_ctrl(enum _ENUM_BTIF_DBG_ID_ flag)
{
	INT32 iRet = 0;

	if (!g_stp_btif.stpBtifId) {
		WMT_WARN_FUNC("NULL BTIF ID reference!\n");
		iRet = -1;
	} else {
		iRet = mtk_wcn_btif_dbg_ctrl(g_stp_btif.stpBtifId, flag);
		if (iRet) {
			WMT_WARN_FUNC("STP btif log dbg ctrl fail(%d)\n", iRet);
			iRet = -2;
		} else
			WMT_INFO_FUNC("stp btif log dbg ctrl ok,flag(%d)\n", flag);
	}

	return iRet;
}

INT32 mtk_wcn_consys_stp_btif_parser_wmt_evt(const PUINT8 str, UINT32 len)
{
	if (!g_stp_btif.stpBtifId) {
		WMT_WARN_FUNC("NULL BTIF ID reference!\n");
		return -1;
	} else
		return (INT32) mtk_wcn_btif_parser_wmt_evt(g_stp_btif.stpBtifId, str, len);
}

INT32 mtk_wcn_consys_stp_btif_rx_has_pending_data(VOID)
{
	return mtk_btif_exp_rx_has_pending_data(g_stp_btif.stpBtifId);
}

INT32 mtk_wcn_consys_stp_btif_tx_has_pending_data(VOID)
{
	return mtk_btif_exp_tx_has_pending_data(g_stp_btif.stpBtifId);
}

P_OSAL_THREAD mtk_wcn_consys_stp_btif_rx_thread_get(VOID)
{
	return &g_stp_btif.btif_thread;
}
