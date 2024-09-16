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

/*******************************************************************************
*                E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#if 0				/* to do---- need check why need this header file */
#include <linux/types.h>
#include <linux/major.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/fcntl.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/timer.h>
#include <linux/ctype.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/bitops.h>
#include <linux/audit.h>
#include <linux/file.h>
#include <linux/module.h>

#include <linux/spinlock.h>
#include <linux/delay.h>	/* udelay() */

#include <linux/uaccess.h>
#include <asm/system.h>
#endif
#include "osal_typedef.h"
#include "stp_core.h"
#include "stp_exp.h"
#include "hif_sdio.h"
#include "stp_sdio.h"
#include "stp_dbg.h"

/*******************************************************************************
*                          C O N S T A N T S
********************************************************************************
*/


/*******************************************************************************
*                         D A T A   T Y P E S
********************************************************************************
*/


/*******************************************************************************
*                        P U B L I C   D A T A
********************************************************************************
*/
/*******************************************************************************
*                       P R I V A T E   D A T A
********************************************************************************
*/
static MTK_WCN_STP_IF_TX stp_uart_if_tx;
static MTK_WCN_STP_IF_TX stp_sdio_if_tx;
static MTK_WCN_STP_IF_TX stp_btif_if_tx;
static MTK_WCN_STP_RX_HAS_PENDING_DATA stp_btif_rx_has_pending_data;
static MTK_WCN_STP_TX_HAS_PENDING_DATA stp_btif_tx_has_pending_data;
static MTK_WCN_STP_RX_THREAD_GET stp_btif_rx_thread_get;
static ENUM_STP_TX_IF_TYPE g_stp_if_type = STP_MAX_IF_TX;
static MTK_WCN_STP_IF_RX stp_if_rx;
static MTK_WCN_STP_EVENT_CB event_callback_tbl[MTKSTP_MAX_TASK_NUM] = { 0x0 };
static MTK_WCN_STP_EVENT_CB tx_event_callback_tbl[MTKSTP_MAX_TASK_NUM] = { 0x0 };

/******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
*******************************************************************************
*/

/*******************************************************************************
*                          F U N C T I O N S
********************************************************************************
*/

INT32 mtk_wcn_sys_if_rx(PUINT8 data, INT32 size)
{
	if (stp_if_rx == 0x0)
		return -1;
	(*stp_if_rx)(data, size);
	return 0;
}

static INT32 mtk_wcn_sys_if_tx(const PUINT8 data, const UINT32 size, PUINT32 written_size)
{
	if (g_stp_if_type == STP_UART_IF_TX)
		return stp_uart_if_tx != NULL ? (*stp_uart_if_tx)(data, size, written_size) : -1;
	else if (g_stp_if_type == STP_SDIO_IF_TX)
		return stp_sdio_if_tx != NULL ? (*stp_sdio_if_tx)(data, size, written_size) : -1;
	else if (g_stp_if_type == STP_BTIF_IF_TX)
		return stp_btif_if_tx != NULL ? (*stp_btif_if_tx) (data, size, written_size) : -1;
	/*if (g_stp_if_type >= STP_MAX_IF_TX) *//* George: remove ALWAYS TRUE condition */
	return -1;
}

static INT32 mtk_wcn_sys_rx_has_pending_data(VOID)
{
	if (g_stp_if_type == STP_BTIF_IF_TX)
		return stp_btif_rx_has_pending_data != NULL ? (*stp_btif_rx_has_pending_data) () : -1;
	return -1;
}

static INT32 mtk_wcn_sys_tx_has_pending_data(VOID)
{
	if (g_stp_if_type == STP_BTIF_IF_TX)
		return stp_btif_tx_has_pending_data != NULL ? (*stp_btif_tx_has_pending_data) () : -1;
	return -1;
}

static P_OSAL_THREAD mtk_wcn_sys_rx_thread_get(VOID)
{
	if (g_stp_if_type == STP_BTIF_IF_TX)
		return stp_btif_rx_thread_get != NULL ? (*stp_btif_rx_thread_get) () : NULL;
	return NULL;
}

static INT32 mtk_wcn_sys_event_set(UINT8 function_type)
{
	if ((function_type < MTKSTP_MAX_TASK_NUM) && (event_callback_tbl[function_type] != 0x0))
		(*event_callback_tbl[function_type])();
	else {
		/* FIXME: error handling */
		osal_dbg_print("[%s] STP set event fail. It seems the function is not active.\n",
			       __func__);
	}

	return 0;
}

static INT32 mtk_wcn_sys_event_tx_resume(UINT8 winspace)
{
	int type = 0;

	for (type = 0; type < MTKSTP_MAX_TASK_NUM; type++) {
		if (tx_event_callback_tbl[type])
			tx_event_callback_tbl[type]();
	}

	return 0;
}

static INT32 mtk_wcn_sys_check_function_status(UINT8 type, UINT8 op)
{

	/*op == FUNCTION_ACTIVE, to check if funciton[type] is active ? */
	if (type >= MTKSTP_MAX_TASK_NUM)
		return STATUS_FUNCTION_INVALID;

	if (op == OP_FUNCTION_ACTIVE) {
		if (event_callback_tbl[type] != 0x0)
			return STATUS_FUNCTION_ACTIVE;
		return STATUS_FUNCTION_INACTIVE;
	}
	/*you can define more operation here ..., to queury function's status/information */

	return STATUS_OP_INVALID;
}

#if STP_EXP_HID_API_EXPORT
INT32 _mtk_wcn_stp_register_if_rx(MTK_WCN_STP_IF_RX func)
#else
INT32 mtk_wcn_stp_register_if_rx(MTK_WCN_STP_IF_RX func)
#endif
{
	stp_if_rx = func;

	return 0;
}
#if !STP_EXP_HID_API_EXPORT
EXPORT_SYMBOL(mtk_wcn_stp_register_if_rx);
#endif

VOID mtk_wcn_stp_set_if_tx_type(ENUM_STP_TX_IF_TYPE stp_if_type)
{
	g_stp_if_type = stp_if_type;
	osal_dbg_print("[%s] set STP_IF_TX to %s.\n",
		       __func__,
		       (STP_UART_IF_TX ==
			stp_if_type) ? "UART" : ((STP_SDIO_IF_TX ==
						  stp_if_type) ? "SDIO" : "NULL"));
}

#if STP_EXP_HID_API_EXPORT
INT32 _mtk_wcn_stp_register_if_tx(ENUM_STP_TX_IF_TYPE stp_if, MTK_WCN_STP_IF_TX func)
#else
INT32 mtk_wcn_stp_register_if_tx(ENUM_STP_TX_IF_TYPE stp_if, MTK_WCN_STP_IF_TX func)
#endif
{
	if (stp_if == STP_UART_IF_TX)
		stp_uart_if_tx = func;
	else if (stp_if == STP_SDIO_IF_TX)
		stp_sdio_if_tx = func;
	else if (stp_if == STP_BTIF_IF_TX)
		stp_btif_if_tx = func;
	else {
		osal_dbg_print("[%s] STP_IF_TX(%d) out of boundary.\n", __func__, stp_if);
		return -1;
	}

	return 0;
}
#if !STP_EXP_HID_API_EXPORT
EXPORT_SYMBOL(mtk_wcn_stp_register_if_tx);
#endif

#if STP_EXP_HID_API_EXPORT
INT32 _mtk_wcn_stp_register_event_cb(INT32 type, MTK_WCN_STP_EVENT_CB func)
#else
INT32 mtk_wcn_stp_register_event_cb(INT32 type, MTK_WCN_STP_EVENT_CB func)
#endif
{
	if ((type < MTKSTP_MAX_TASK_NUM) && (type >= BT_TASK_INDX)) {
		event_callback_tbl[type] = func;

		/*clear rx queue */
		mtk_wcn_stp_flush_rx_queue(type);
	}

	return 0;
}
#if !STP_EXP_HID_API_EXPORT
EXPORT_SYMBOL(mtk_wcn_stp_register_event_cb);
#endif

#if STP_EXP_HID_API_EXPORT
INT32 _mtk_wcn_stp_register_tx_event_cb(INT32 type, MTK_WCN_STP_EVENT_CB func)
#else
INT32 mtk_wcn_stp_register_tx_event_cb(INT32 type, MTK_WCN_STP_EVENT_CB func)
#endif
{
	if ((type < MTKSTP_MAX_TASK_NUM) && (type >= BT_TASK_INDX))
		tx_event_callback_tbl[type] = func;
	else
		osal_bug_on(0);

	return 0;
}
#if !STP_EXP_HID_API_EXPORT
EXPORT_SYMBOL(mtk_wcn_stp_register_tx_event_cb);
#endif

#if STP_EXP_HID_API_EXPORT
INT32 _mtk_wcn_stp_register_rx_has_pending_data(ENUM_STP_TX_IF_TYPE stp_if, MTK_WCN_STP_RX_HAS_PENDING_DATA func)
#else
INT32 mtk_wcn_stp_register_rx_has_pending_data(ENUM_STP_TX_IF_TYPE stp_if, MTK_WCN_STP_RX_HAS_PENDING_DATA func)
#endif
{
	if (stp_if == STP_BTIF_IF_TX)
		stp_btif_rx_has_pending_data = func;
	else {
		osal_dbg_print("[%s] STP_IF_TX(%d) out of boundary.\n", __func__, stp_if);
		return -1;
	}

	return 0;
}
#if !STP_EXP_HID_API_EXPORT
EXPORT_SYMBOL(mtk_wcn_stp_register_rx_has_pending_data);
#endif

#if STP_EXP_HID_API_EXPORT
INT32 _mtk_wcn_stp_register_tx_has_pending_data(ENUM_STP_TX_IF_TYPE stp_if, MTK_WCN_STP_TX_HAS_PENDING_DATA func)
#else
INT32 mtk_wcn_stp_register_tx_has_pending_data(ENUM_STP_TX_IF_TYPE stp_if, MTK_WCN_STP_TX_HAS_PENDING_DATA func)
#endif
{
	if (stp_if == STP_BTIF_IF_TX)
		stp_btif_tx_has_pending_data = func;
	else {
		osal_dbg_print("[%s] STP_IF_TX(%d) out of boundary.\n", __func__, stp_if);
		return -1;
	}

	return 0;
}
#if !STP_EXP_HID_API_EXPORT
EXPORT_SYMBOL(mtk_wcn_stp_register_tx_has_pending_data);
#endif

#if STP_EXP_HID_API_EXPORT
INT32 _mtk_wcn_stp_register_rx_thread_get(ENUM_STP_TX_IF_TYPE stp_if, MTK_WCN_STP_RX_THREAD_GET func)
#else
INT32 mtk_wcn_stp_register_rx_thread_get(ENUM_STP_TX_IF_TYPE stp_if, MTK_WCN_STP_RX_THREAD_GET func)
#endif
{
	if (stp_if == STP_BTIF_IF_TX)
		stp_btif_rx_thread_get = func;
	else {
		osal_dbg_print("[%s] STP_IF_TX(%d) out of boundary.\n", __func__, stp_if);
		return -1;
	}

	return 0;
}
#if !STP_EXP_HID_API_EXPORT
EXPORT_SYMBOL(mtk_wcn_stp_register_rx_thread_get);
#endif

INT32 stp_drv_init(VOID)
{
	INT32 ret = 0;

	mtkstp_callback cb = {
		.cb_if_tx = mtk_wcn_sys_if_tx,
		.cb_rx_has_pending_data = mtk_wcn_sys_rx_has_pending_data,
		.cb_tx_has_pending_data = mtk_wcn_sys_tx_has_pending_data,
		.cb_rx_thread_get = mtk_wcn_sys_rx_thread_get,
		.cb_event_set = mtk_wcn_sys_event_set,
		.cb_event_tx_resume = mtk_wcn_sys_event_tx_resume,
		.cb_check_funciton_status = mtk_wcn_sys_check_function_status
	};
#if 0
#ifdef MTK_WCN_WMT_STP_EXP_SYMBOL_ABSTRACT
	MTK_WCN_STP_EXP_CB_INFO stpExpCb = {
		.stp_send_data_cb = _mtk_wcn_stp_send_data,
		.stp_send_data_raw_cb = _mtk_wcn_stp_send_data_raw,
		.stp_parser_data_cb = _mtk_wcn_stp_parser_data,
		.stp_receive_data_cb = _mtk_wcn_stp_receive_data,
		.stp_is_rxqueue_empty_cb = _mtk_wcn_stp_is_rxqueue_empty,
		.stp_is_ready_cb = _mtk_wcn_stp_is_ready,
		.stp_set_bluez_cb = _mtk_wcn_stp_set_bluez,
		.stp_if_tx_cb = _mtk_wcn_stp_register_if_tx,
		.stp_if_rx_cb = _mtk_wcn_stp_register_if_rx,
		.stp_reg_event_cb = _mtk_wcn_stp_register_event_cb,
		.stp_reg_tx_event_cb = _mtk_wcn_stp_register_tx_event_cb,
		.stp_coredump_start_get_cb = _mtk_wcn_stp_coredump_start_get
	};
	mtk_wcn_stp_exp_cb_reg(&stpExpCb);

#endif
#endif
	ret = mtk_wcn_stp_init(&cb);

	return ret;
}

VOID stp_drv_exit(VOID)
{
	mtk_wcn_stp_deinit();
#if 0
#ifdef MTK_WCN_WMT_STP_EXP_SYMBOL_ABSTRACT
	mtk_wcn_stp_exp_cb_unreg();
#endif
#endif
}

INT32 mtk_wcn_stp_sdio_wake_up_ctrl(MTK_WCN_HIF_SDIO_CLTCTX ctx)
{
	stp_sdio_wake_up_ctrl(ctx);
	return 0;
}
EXPORT_SYMBOL(mtk_wcn_stp_sdio_wake_up_ctrl);

INT32 mtk_stp_dbg_poll_cpupcr(UINT32 times, UINT32 sleep, UINT32 cmd)
{
	return stp_dbg_poll_cpupcr(times, sleep, cmd);
}
EXPORT_SYMBOL(mtk_stp_dbg_poll_cpupcr);
