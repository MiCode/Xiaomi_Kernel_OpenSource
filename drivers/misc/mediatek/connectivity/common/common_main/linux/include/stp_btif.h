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

#ifndef _STP_BTIF_H_
#define _STP_BTIF_H_

#include "osal_typedef.h"
#include "mtk_btif_exp.h"
#include "osal.h"

struct stp_btif {
	ULONG stpBtifId;
	OSAL_THREAD btif_thread;
};

INT32 mtk_wcn_consys_stp_btif_open(VOID);
INT32 mtk_wcn_consys_stp_btif_close(VOID);
INT32 mtk_wcn_consys_stp_btif_rx_cb_register(MTK_WCN_BTIF_RX_CB rx_cb);
INT32 mtk_wcn_consys_stp_btif_tx(const PUINT8 pBuf, const UINT32 len, PUINT32 written_len);
INT32 mtk_wcn_consys_stp_btif_wakeup(VOID);
INT32 mtk_wcn_consys_stp_btif_dpidle_ctrl(UINT32 en_flag);
INT32 mtk_wcn_consys_stp_btif_lpbk_ctrl(enum _ENUM_BTIF_LPBK_MODE_ mode);
INT32 mtk_wcn_consys_stp_btif_logger_ctrl(enum _ENUM_BTIF_DBG_ID_ flag);
INT32 mtk_wcn_consys_stp_btif_parser_wmt_evt(const PUINT8 str, UINT32 len);
INT32 mtk_wcn_consys_stp_btif_rx_has_pending_data(VOID);
INT32 mtk_wcn_consys_stp_btif_tx_has_pending_data(VOID);
P_OSAL_THREAD mtk_wcn_consys_stp_btif_rx_thread_get(VOID);

#endif
