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
#define DFT_TAG         "[WMT-CTRL]"


/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "osal_typedef.h"
#include "osal.h"

#include "wmt_ctrl.h"
#include "wmt_core.h"
#include "wmt_lib.h"
#include "wmt_dev.h"
#include "wmt_plat.h"
#include "hif_sdio.h"
#include "stp_core.h"
#include "stp_dbg.h"
#include "wmt_ic.h"
#include "wmt_step.h"

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/



/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/


/*******************************************************************************
*                    F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
/* moved to wmt_ctrl.h */
/*static INT32  wmt_ctrl_tx_ex (UINT8 *pData, UINT32 size, UINT32 *writtenSize, MTK_WCN_BOOL bRawFlag);*/

static INT32 wmt_ctrl_stp_conf_ex(WMT_STP_CONF_TYPE type, UINT32 value);

static INT32 wmt_ctrl_hw_pwr_off(P_WMT_CTRL_DATA);
static INT32 wmt_ctrl_hw_pwr_on(P_WMT_CTRL_DATA);
static INT32 wmt_ctrl_hw_rst(P_WMT_CTRL_DATA);
static INT32 wmt_ctrl_stp_close(P_WMT_CTRL_DATA);
static INT32 wmt_ctrl_stp_open(P_WMT_CTRL_DATA);
static INT32 wmt_ctrl_stp_conf(P_WMT_CTRL_DATA);
static INT32 wmt_ctrl_free_patch(P_WMT_CTRL_DATA);
static INT32 wmt_ctrl_get_patch(P_WMT_CTRL_DATA);
static INT32 wmt_ctrl_host_baudrate_set(P_WMT_CTRL_DATA);
static INT32 wmt_ctrl_sdio_hw(P_WMT_CTRL_DATA);
static INT32 wmt_ctrl_sdio_func(P_WMT_CTRL_DATA);
static INT32 wmt_ctrl_hwidver_set(P_WMT_CTRL_DATA);
static INT32 wmt_ctrl_stp_rst(P_WMT_CTRL_DATA);
static INT32 wmt_ctrl_get_wmt_conf(P_WMT_CTRL_DATA);
static INT32 wmt_ctrl_others(P_WMT_CTRL_DATA);
static INT32 wmt_ctrl_tx(P_WMT_CTRL_DATA);
static INT32 wmt_ctrl_rx(P_WMT_CTRL_DATA);
static INT32 wmt_ctrl_patch_search(P_WMT_CTRL_DATA);
static INT32 wmt_ctrl_crystal_triming_put(P_WMT_CTRL_DATA pWmtCtrlData);
static INT32 wmt_ctrl_crystal_triming_get(P_WMT_CTRL_DATA pWmtCtrlData);
static INT32 wmt_ctrl_hw_state_show(P_WMT_CTRL_DATA pWmtCtrlData);
static INT32 wmt_ctrl_get_patch_num(P_WMT_CTRL_DATA);
static INT32 wmt_ctrl_get_patch_info(P_WMT_CTRL_DATA);
static INT32 wmt_ctrl_rx_flush(P_WMT_CTRL_DATA);
static INT32 wmt_ctrl_soc_paldo_ctrl(P_WMT_CTRL_DATA);
static INT32 wmt_ctrl_soc_wakeup_consys(P_WMT_CTRL_DATA);
static INT32 wmt_ctrl_set_stp_dbg_info(P_WMT_CTRL_DATA);
static INT32 wmt_ctrl_bgw_desense_ctrl(P_WMT_CTRL_DATA);
static INT32 wmt_ctrl_trg_assert(P_WMT_CTRL_DATA);
static INT32 wmt_ctrl_evt_parser(P_WMT_CTRL_DATA pWmtCtrlData);
#if CFG_WMT_LTE_COEX_HANDLING
static INT32 wmt_ctrl_get_tdm_req_antsel(P_WMT_CTRL_DATA);
#endif

static INT32 wmt_ctrl_gps_sync_set(P_WMT_CTRL_DATA pData);

static INT32 wmt_ctrl_gps_lna_set(P_WMT_CTRL_DATA pData);

static INT32 wmt_ctrl_get_patch_name(P_WMT_CTRL_DATA pWmtCtrlData);

static INT32 wmt_ctrl_get_rom_patch_info(P_WMT_CTRL_DATA pWmtCtrlData);

static INT32 wmt_ctrl_update_patch_version(P_WMT_CTRL_DATA);

/* TODO: [FixMe][GeorgeKuo]: remove unused function */
/*static INT32  wmt_ctrl_hwver_get(P_WMT_CTRL_DATA);*/



/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/* GeorgeKuo: Use designated initializers described in
 * http://gcc.gnu.org/onlinedocs/gcc-4.0.4/gcc/Designated-Inits.html
 */
static const WMT_CTRL_FUNC wmt_ctrl_func[] = {
	[WMT_CTRL_HW_PWR_OFF] = wmt_ctrl_hw_pwr_off,
	[WMT_CTRL_HW_PWR_ON] = wmt_ctrl_hw_pwr_on,
	[WMT_CTRL_HW_RST] = wmt_ctrl_hw_rst,
	[WMT_CTRL_STP_CLOSE] = wmt_ctrl_stp_close,
	[WMT_CTRL_STP_OPEN] = wmt_ctrl_stp_open,
	[WMT_CTRL_STP_CONF] = wmt_ctrl_stp_conf,
	[WMT_CTRL_FREE_PATCH] = wmt_ctrl_free_patch,
	[WMT_CTRL_GET_PATCH] = wmt_ctrl_get_patch,
	[WMT_CTRL_GET_PATCH_NAME] = wmt_ctrl_get_patch_name,
	[WMT_CTRL_HOST_BAUDRATE_SET] = wmt_ctrl_host_baudrate_set,
	[WMT_CTRL_SDIO_HW] = wmt_ctrl_sdio_hw,
	[WMT_CTRL_SDIO_FUNC] = wmt_ctrl_sdio_func,
	[WMT_CTRL_HWIDVER_SET] = wmt_ctrl_hwidver_set,
	[WMT_CTRL_HWVER_GET] = NULL,	/* TODO: [FixMe][GeorgeKuo]: remove unused function. */
	[WMT_CTRL_STP_RST] = wmt_ctrl_stp_rst,
	[WMT_CTRL_GET_WMT_CONF] = wmt_ctrl_get_wmt_conf,
	[WMT_CTRL_TX] = wmt_ctrl_tx,
	[WMT_CTRL_RX] = wmt_ctrl_rx,
	[WMT_CTRL_RX_FLUSH] = wmt_ctrl_rx_flush,
	[WMT_CTRL_GPS_SYNC_SET] = wmt_ctrl_gps_sync_set,
	[WMT_CTRL_GPS_LNA_SET] = wmt_ctrl_gps_lna_set,
	[WMT_CTRL_PATCH_SEARCH] = wmt_ctrl_patch_search,
	[WMT_CTRL_CRYSTAL_TRIMING_GET] = wmt_ctrl_crystal_triming_get,
	[WMT_CTRL_CRYSTAL_TRIMING_PUT] = wmt_ctrl_crystal_triming_put,
	[WMT_CTRL_HW_STATE_DUMP] = wmt_ctrl_hw_state_show,
	[WMT_CTRL_GET_PATCH_NUM] = wmt_ctrl_get_patch_num,
	[WMT_CTRL_GET_PATCH_INFO] = wmt_ctrl_get_patch_info,
	[WMT_CTRL_SOC_PALDO_CTRL] = wmt_ctrl_soc_paldo_ctrl,
	[WMT_CTRL_SOC_WAKEUP_CONSYS] = wmt_ctrl_soc_wakeup_consys,
	[WMT_CTRL_SET_STP_DBG_INFO] = wmt_ctrl_set_stp_dbg_info,
	[WMT_CTRL_BGW_DESENSE_CTRL] = wmt_ctrl_bgw_desense_ctrl,
	[WMT_CTRL_TRG_ASSERT] = wmt_ctrl_trg_assert,
	#if CFG_WMT_LTE_COEX_HANDLING
	[WMT_CTRL_GET_TDM_REQ_ANTSEL] = wmt_ctrl_get_tdm_req_antsel,
#endif
	[WMT_CTRL_EVT_PARSER] = wmt_ctrl_evt_parser,
	[WMT_CTRL_GET_ROM_PATCH_INFO] = wmt_ctrl_get_rom_patch_info,
	[WMT_CTRL_UPDATE_PATCH_VERSION] = wmt_ctrl_update_patch_version,
	[WMT_CTRL_MAX] = wmt_ctrl_others,
};

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
INT32 __weak mtk_wcn_consys_stp_btif_parser_wmt_evt(const PUINT8 str, UINT32 len)
{
	return 0;
}

INT32 wmt_ctrl(P_WMT_CTRL_DATA pWmtCtrlData)
{
	UINT32 ctrlId = 0;

	if (pWmtCtrlData == NULL) {
		osal_assert(0);
		return -1;
	}

	ctrlId = pWmtCtrlData->ctrlId;
	/*1sanity check, including wmtCtrlId */
	if ((pWmtCtrlData == NULL)
	    || (ctrlId >= WMT_CTRL_MAX))
		/* || (ctrlId < WMT_CTRL_HW_PWR_OFF) ) [FixMe][GeorgeKuo]: useless comparison */
	{
		osal_assert(pWmtCtrlData != NULL);
		osal_assert(ctrlId < WMT_CTRL_MAX);
		/* osal_assert(ctrlId >= WMT_CTRL_HW_PWR_OFF); [FixMe][GeorgeKuo]: useless comparison */
		return -2;
	}
	/* TODO: [FixMe][GeorgeKuo] do sanity check to const function table when init and skip checking here */
	if (wmt_ctrl_func[ctrlId]) {
		/*call servicd handling API */
		return (*(wmt_ctrl_func[ctrlId])) (pWmtCtrlData);	/* serviceHandlerPack[ctrlId].serviceHandler */
	}
	osal_assert(wmt_ctrl_func[ctrlId] != NULL);
	return -3;
}

INT32 wmt_ctrl_tx(P_WMT_CTRL_DATA pWmtCtrlData /*UINT8 *pData, UINT32 size, UINT32 *writtenSize */)
{
	PUINT8 pData = (PUINT8) pWmtCtrlData->au4CtrlData[0];
	UINT32 size = pWmtCtrlData->au4CtrlData[1];
	PUINT32 writtenSize = (PUINT32) pWmtCtrlData->au4CtrlData[2];
	MTK_WCN_BOOL bRawFlag = pWmtCtrlData->au4CtrlData[3];

	return wmt_ctrl_tx_ex(pData, size, writtenSize, bRawFlag);
}

static VOID wmt_ctrl_show_sched_stats_log(P_OSAL_THREAD pThread, P_OSAL_THREAD_SCHEDSTATS pSchedstats)
{
	if ((pThread) && (pThread->pThread) && (pSchedstats))
		WMT_ERR_FUNC("WMT rx_timeout, pid[%d/%s] stats duration:%llums, sched(x%llu/r%llu/i%llu)\n",
			     pThread->pThread->pid,
			     pThread->threadName,
			     pSchedstats->time,
			     pSchedstats->exec,
			     pSchedstats->runnable,
			     pSchedstats->iowait);
}

INT32 wmt_ctrl_rx(P_WMT_CTRL_DATA pWmtCtrlData /*UINT8 *pBuff, UINT32 buffLen, UINT32 *readSize */)
{
	P_DEV_WMT pDev = &gDevWmt;	/* single instance */
	INT32 readLen;
	INT32 waitRet = -1;
	INT32 loopCnt = 1;
	INT32 leftCnt;
	PUINT8 pBuff = (PUINT8) pWmtCtrlData->au4CtrlData[0];
	UINT32 buffLen = pWmtCtrlData->au4CtrlData[1];
	PUINT32 readSize = (PUINT32) pWmtCtrlData->au4CtrlData[2];
	INT32 stpRxState;
	INT32 extended = 0;
	P_OSAL_THREAD p_rx_thread = NULL;
	OSAL_THREAD_SCHEDSTATS schedstats;

	if (readSize)
		*readSize = 0;

	/* sanity check */
	if (!buffLen) {
		WMT_WARN_FUNC("buffLen = 0\n");
		osal_assert(buffLen);
		return 0;
	}

	if (!osal_test_bit(WMT_STAT_STP_OPEN, &pDev->state)) {
		WMT_WARN_FUNC("state(0x%lx)\n", pDev->state.data);
		osal_assert(osal_test_bit(WMT_STAT_STP_OPEN, &pDev->state));
		return -2;
	}
	if (wmt_lib_get_drv_status(WMTDRV_TYPE_WMT) == DRV_STS_FUNC_ON)
		loopCnt = 4;
	leftCnt = loopCnt;

	/* sanity ok, proceeding rx operation */
	readLen = mtk_wcn_stp_receive_data(pBuff, buffLen, WMT_TASK_INDX);
	p_rx_thread = mtk_stp_rx_thread_get();
	osal_thread_sched_mark(p_rx_thread, &schedstats);

	while (readLen == 0 && leftCnt > 0) {	/* got nothing, wait for STP's signal */
		/* if assert happen, do not wait for any signal again */
		if (mtk_wcn_stp_get_wmt_trg_assert() == 1
		    && mtk_wcn_stp_is_wmt_last_close() == 0)
			break;
		pDev->rWmtRxWq.timeoutValue = WMT_LIB_RX_TIMEOUT/loopCnt;
		waitRet = wmt_dev_rx_timeout(&pDev->rWmtRxWq);
		if (waitRet == 0) {
			leftCnt--;
			/* dump btif_rxd's backtrace to check whether it is blocked or not */
			osal_dump_thread_state("btif_rxd");
			stp_dbg_poll_cpupcr(5, 1, 1);
			if (!mtk_wcn_stp_is_sdio_mode())
				mtk_wcn_consys_stp_btif_logger_ctrl(BTIF_DUMP_BTIF_IRQ);

			if (leftCnt <= 0) {
				if (extended == 0) {
					stpRxState = mtk_stp_check_rx_has_pending_data();
					WMT_INFO_FUNC("check rx pending data(%d)\n", stpRxState);
					readLen = mtk_wcn_stp_receive_data(pBuff, buffLen, WMT_TASK_INDX);
					if (readLen > 0) {
						WMT_INFO_FUNC("rx data received, rx done.\n");
						break;
					} else if (stpRxState > 0) {
						osal_thread_sched_unmark(p_rx_thread, &schedstats);
						wmt_ctrl_show_sched_stats_log(p_rx_thread, &schedstats);
						WMT_INFO_FUNC("stp has pending data, extend ~4 seconds\n");
						leftCnt = WMT_LIB_RX_EXTEND_TIMEOUT/pDev->rWmtRxWq.timeoutValue;
						extended = 1;
						osal_thread_sched_mark(p_rx_thread, &schedstats);
						continue;
					}
					/* wmt is closed, device is shuting down */
					if (wmt_dev_is_close() || mtk_wcn_stp_is_wmt_last_close() == 1) {
						leftCnt = 10;
						continue;
					}
				}

				osal_thread_sched_unmark(p_rx_thread, &schedstats);
				wmt_ctrl_show_sched_stats_log(p_rx_thread, &schedstats);
				stp_dbg_poll_cpupcr(5, 1, 1);
				WMT_ERR_FUNC("wmt_dev_rx_timeout: timeout,jiffies(%lu),timeoutvalue(%d)\n",
					     jiffies, pDev->rWmtRxWq.timeoutValue);
				WMT_STEP_COMMAND_TIMEOUT_DO_ACTIONS_FUNC("STP RX timeout");

				/* Reason number 44 means that stp data path still has data,
				 * possibly a driver problem
				 */
				if (stpRxState != 0)
					wmt_lib_trigger_assert(WMTDRV_TYPE_WMT, 44);
				return -1;
			}
		} else if (waitRet < 0) {
			WMT_WARN_FUNC("wmt_dev_rx_timeout: interrupted by signal (%d)\n", waitRet);
			WMT_STEP_COMMAND_TIMEOUT_DO_ACTIONS_FUNC("STP RX timeout");
			return waitRet;
		}
		readLen = mtk_wcn_stp_receive_data(pBuff, buffLen, WMT_TASK_INDX);

		if (readLen == 0)
			WMT_DBG_FUNC("wmt_ctrl_rx be signaled, but no rx data(%d)\n", waitRet);
		WMT_DBG_FUNC("stp_receive_data: readLen(%d)\n", readLen);
	}

	if (readSize)
		*readSize = readLen;

	return 0;
}


INT32
wmt_ctrl_tx_ex(const PUINT8 pData,
	       const UINT32 size, PUINT32 writtenSize, const MTK_WCN_BOOL bRawFlag)
{
	P_DEV_WMT pDev = &gDevWmt;	/* single instance */
	INT32 iRet;

	if (writtenSize != NULL)
		*writtenSize = 0;

	/* sanity check */
	if (size == 0) {
		WMT_WARN_FUNC("size to tx is 0\n");
		osal_assert(size);
		return -1;
	}

	/* if STP is not enabled yet, can't use this function. Use tx_raw instead */
	if (!osal_test_bit(WMT_STAT_STP_OPEN, &pDev->state) ||
	    !osal_test_bit(WMT_STAT_STP_EN, &pDev->state)) {
		WMT_ERR_FUNC("wmt state(0x%lx)\n", pDev->state.data);
		osal_assert(osal_test_bit(WMT_STAT_STP_EN, &pDev->state));
		osal_assert(osal_test_bit(WMT_STAT_STP_OPEN, &pDev->state));
		iRet = -2;
		if (writtenSize)
			*writtenSize = iRet;
		return iRet;
	}

	/* sanity ok, proceeding tx operation */
	/*retval = mtk_wcn_stp_send_data(data, size, WMTDRV_TYPE_WMT); */
	mtk_wcn_stp_flush_rx_queue(WMT_TASK_INDX);
	if (bRawFlag)
		iRet = mtk_wcn_stp_send_data_raw(pData, size, WMT_TASK_INDX);
	else
		iRet = mtk_wcn_stp_send_data(pData, size, WMT_TASK_INDX);

	if (iRet != size) {
		WMT_WARN_FUNC("write(%d) written(%d)\n", size, iRet);
		osal_assert(iRet == size);
	}

	if (writtenSize)
		*writtenSize = iRet;

	return 0;

}

INT32 wmt_ctrl_rx_flush(P_WMT_CTRL_DATA pWmtCtrlData)
{
	UINT32 type = pWmtCtrlData->au4CtrlData[0];

	WMT_INFO_FUNC("flush rx %d queue\n", type);
	mtk_wcn_stp_flush_rx_queue(type);

	return 0;
}


INT32 wmt_ctrl_hw_pwr_off(P_WMT_CTRL_DATA pWmtCtrlData)
{
	INT32 iret;
	/*psm should be disabled before wmt_ic_deinit*/
	P_DEV_WMT pDev = &gDevWmt;

	if (osal_test_and_clear_bit(WMT_STAT_PWR, &pDev->state)) {
		WMT_DBG_FUNC("on->off\n");
		iret = wmt_plat_pwr_ctrl(FUNC_OFF);
	} else {
		WMT_WARN_FUNC("already off\n");
		iret = 0;
	}

	return iret;
}

INT32 wmt_ctrl_hw_pwr_on(P_WMT_CTRL_DATA pWmtCtrlData)
{
	INT32 iret;
	/*psm should be enabled right after wmt_ic_init */
	P_DEV_WMT pDev = &gDevWmt;

	if (osal_test_and_set_bit(WMT_STAT_PWR, &pDev->state)) {
		WMT_WARN_FUNC("already on\n");
		iret = 0;
	} else {
		WMT_DBG_FUNC("off->on\n");
		iret = wmt_plat_pwr_ctrl(FUNC_ON);
	}

	return iret;
}

INT32 wmt_ctrl_ul_cmd(P_DEV_WMT pWmtDev, const PUINT8 pCmdStr)
{
	INT32 waitRet = -1;
	P_OSAL_SIGNAL pCmdSignal;
	P_OSAL_EVENT pCmdReq;

	if (osal_test_and_set_bit(WMT_STAT_CMD, &pWmtDev->state)) {
		WMT_WARN_FUNC("cmd buf is occupied by (%s)\n", pWmtDev->cCmd);
		return -1;
	}

	/* indicate baud rate change to user space app */
#if 0
	INIT_COMPLETION(pWmtDev->cmd_comp);
	pWmtDev->cmd_result = -1;
	strncpy(pWmtDev->cCmd, pCmdStr, NAME_MAX);
	pWmtDev->cCmd[NAME_MAX] = '\0';
	wake_up_interruptible(&pWmtDev->cmd_wq);
#endif

	pCmdSignal = &pWmtDev->cmdResp;
	osal_signal_init(pCmdSignal);
	pCmdSignal->timeoutValue = 6000;
	osal_strncpy(pWmtDev->cCmd, pCmdStr, NAME_MAX);
	pWmtDev->cCmd[NAME_MAX] = '\0';

	pCmdReq = &pWmtDev->cmdReq;

	osal_trigger_event(&pWmtDev->cmdReq);
	WMT_DBG_FUNC("str(%s) request ok\n", pCmdStr);

/* waitRet = wait_for_completion_interruptible_timeout(&pWmtDev->cmd_comp, msecs_to_jiffies(2000)); */
	waitRet = osal_wait_for_signal_timeout(pCmdSignal, NULL);
	WMT_LOUD_FUNC("wait signal iRet:%d\n", waitRet);
	if (waitRet == 0) {
		WMT_ERR_FUNC("wait signal timeout\n");
		return -2;
	}

	WMT_DBG_FUNC("str(%s) result(%d)\n", pCmdStr, pWmtDev->cmdResult);

	return pWmtDev->cmdResult;
}

INT32 wmt_ctrl_hw_rst(P_WMT_CTRL_DATA pWmtCtrlData)
{
	wmt_plat_pwr_ctrl(FUNC_RST);
	return 0;
}

INT32 wmt_ctrl_hw_state_show(P_WMT_CTRL_DATA pWmtCtrlData)
{
	wmt_plat_pwr_ctrl(FUNC_STAT);
	return 0;
}

INT32 wmt_ctrl_stp_close(P_WMT_CTRL_DATA pWmtCtrlData)
{
	P_DEV_WMT pDev = &gDevWmt;	/* single instance */
	INT32 iRet = 0;
	UINT8 cmdStr[NAME_MAX + 1] = { 0 };
	/* un-register to STP-core for rx */

	iRet = mtk_wcn_stp_register_event_cb(WMT_TASK_INDX, NULL);	/* mtk_wcn_stp_register_event_cb */
	if (iRet) {
		WMT_WARN_FUNC("stp_reg cb unregister fail(%d)\n", iRet);
		return -1;
	}

	if (pDev->rWmtHifConf.hifType == WMT_HIF_UART) {

		osal_snprintf(cmdStr, NAME_MAX, "close_stp");

		iRet = wmt_ctrl_ul_cmd(pDev, cmdStr);
		if (iRet) {
			WMT_WARN_FUNC("wmt_ctrl_ul_cmd fail(%d)\n", iRet);
			return -2;
		}
	}
	if (pDev->rWmtHifConf.hifType == WMT_HIF_BTIF) {
		/*un-register rxcb to btif */
		iRet = mtk_wcn_stp_rxcb_register(NULL);
		if (iRet) {
			WMT_WARN_FUNC("mtk_wcn_stp_rxcb_unregister fail(%d)\n", iRet);
			return -2;
		}

		iRet = mtk_wcn_stp_close_btif();
		if (iRet) {
			WMT_WARN_FUNC("mtk_wcn_stp_close_btif fail(%d)\n", iRet);
			return -3;
		}
	}

	osal_clear_bit(WMT_STAT_STP_OPEN, &pDev->state);

	return 0;
}

INT32 wmt_ctrl_stp_open(P_WMT_CTRL_DATA pWmtCtrlData)
{
	P_DEV_WMT pDev = &gDevWmt;	/* single instance */
	INT32 iRet;
	UINT8 cmdStr[NAME_MAX + 1] = { 0 };

	if (pDev->rWmtHifConf.hifType == WMT_HIF_UART) {
		osal_snprintf(cmdStr, NAME_MAX, "open_stp");
		iRet = wmt_ctrl_ul_cmd(pDev, cmdStr);
		if (iRet) {
			WMT_WARN_FUNC("wmt_ctrl_ul_cmd fail(%d)\n", iRet);
			return -1;
		}
	}
	if (pDev->rWmtHifConf.hifType == WMT_HIF_BTIF) {
		iRet = mtk_wcn_stp_open_btif();
		if (iRet) {
			WMT_WARN_FUNC("mtk_wcn_stp_open_btif fail(%d)\n", iRet);
			return -1;
		}

		/*register stp rx call back to btif */
		iRet = mtk_wcn_stp_rxcb_register((MTK_WCN_BTIF_RX_CB)mtk_wcn_stp_parser_data);
		if (iRet) {
			WMT_WARN_FUNC("mtk_wcn_stp_rxcb_register fail(%d)\n", iRet);
			return -2;
		}
	}
	/* register to STP-core for rx */
	/* mtk_wcn_stp_register_event_cb */
	iRet = mtk_wcn_stp_register_event_cb(WMT_TASK_INDX, wmt_dev_rx_event_cb);
	if (iRet) {
		WMT_WARN_FUNC("stp_reg cb fail(%d)\n", iRet);
		return -2;
	}

	osal_set_bit(WMT_STAT_STP_OPEN, &pDev->state);
#if 0
	iRet = mtk_wcn_stp_lpbk_ctrl(1);
#endif

	return 0;
}


INT32 wmt_ctrl_patch_search(P_WMT_CTRL_DATA pWmtCtrlData)
{
	P_DEV_WMT pDev = &gDevWmt;	/* single instance */
	INT32 iRet;
	UINT8 cmdStr[NAME_MAX + 1] = { 0 };

	osal_snprintf(cmdStr, NAME_MAX, "srh_patch");
	iRet = wmt_ctrl_ul_cmd(pDev, cmdStr);
	if (iRet) {
		WMT_WARN_FUNC("wmt_ctrl_ul_cmd fail(%d)\n", iRet);
		return -1;
	}
	return 0;
}


INT32 wmt_ctrl_get_patch_num(P_WMT_CTRL_DATA pWmtCtrlData)
{
	P_DEV_WMT pDev = &gDevWmt;	/* single instance */

	pWmtCtrlData->au4CtrlData[0] = pDev->patchNum;
	return 0;
}


INT32 wmt_ctrl_get_patch_info(P_WMT_CTRL_DATA pWmtCtrlData)
{
	P_DEV_WMT pDev = &gDevWmt;	/* single instance */
	UINT32 downLoadSeq = 0;
	P_WMT_PATCH_INFO pPatchinfo = NULL;
	PUINT8 pNbuf = NULL;
	PUINT8 pAbuf = NULL;

	if (pDev->pWmtPatchInfo == NULL) {
		WMT_ERR_FUNC("pWmtPatchInfo is NULL\n");
		return -1;
	}

	downLoadSeq = pWmtCtrlData->au4CtrlData[0];
	WMT_DBG_FUNC("download seq is %d\n", downLoadSeq);

	pPatchinfo = pDev->pWmtPatchInfo + downLoadSeq - 1;
	pNbuf = (PUINT8) pWmtCtrlData->au4CtrlData[1];
	pAbuf = (PUINT8) pWmtCtrlData->au4CtrlData[2];
	if (pPatchinfo) {
		osal_memcpy(pNbuf, pPatchinfo->patchName, osal_sizeof(pPatchinfo->patchName));
		osal_memcpy(pAbuf, pPatchinfo->addRess, osal_sizeof(pPatchinfo->addRess));
		WMT_DBG_FUNC("get 4 address bytes is 0x%2x,0x%2x,0x%2x,0x%2x", pAbuf[0], pAbuf[1],
			     pAbuf[2], pAbuf[3]);
	} else
		WMT_ERR_FUNC("NULL patchinfo pointer\n");

	return 0;
}

INT32 wmt_ctrl_get_rom_patch_info(P_WMT_CTRL_DATA pWmtCtrlData)
{
	P_DEV_WMT pDev = &gDevWmt;	/* single instance */
	UINT32 type = 0;
	struct wmt_rom_patch_info *pPatchinfo = NULL;
	PUINT8 pNbuf = NULL;
	PUINT8 pAbuf = NULL;
	INT32 ret = 0;
	UINT8 cmdStr[NAME_MAX + 1] = { 0 };

	type = pWmtCtrlData->au4CtrlData[0];
	WMT_DBG_FUNC("rom patch type is %d\n", type);
	pDev->ip_ver = pWmtCtrlData->au4CtrlData[3];
	pDev->fw_ver = pWmtCtrlData->au4CtrlData[4];
	WMT_DBG_FUNC("ip version is [%x] [%x]\n", pDev->ip_ver, pDev->fw_ver);

	if (!pDev->pWmtRomPatchInfo[WMTDRV_TYPE_WMT]) {
		osal_snprintf(cmdStr, NAME_MAX, "srh_rom_patch");
		ret = wmt_ctrl_ul_cmd(pDev, cmdStr);
		if (ret) {
			WMT_WARN_FUNC("wmt_ctrl_ul_cmd fail(%d)\n", ret);
			return -1;
		}
	}

	if (type < WMTDRV_TYPE_ANT) {
		pPatchinfo = pDev->pWmtRomPatchInfo[type];
		pNbuf = (PUINT8) pWmtCtrlData->au4CtrlData[1];
		pAbuf = (PUINT8) pWmtCtrlData->au4CtrlData[2];
		if (pPatchinfo) {
			osal_memcpy(pNbuf, pPatchinfo->patchName, osal_sizeof(pPatchinfo->patchName));
			osal_memcpy(pAbuf, pPatchinfo->addRess, osal_sizeof(pPatchinfo->addRess));
			WMT_DBG_FUNC("get 4 address bytes is 0x%2x,0x%2x,0x%2x,0x%2x",
					pAbuf[0], pAbuf[1], pAbuf[2], pAbuf[3]);
		} else {
			WMT_ERR_FUNC("NULL patchinfo pointer\n");
			ret = 1;
		}
	} else {
		WMT_ERR_FUNC("rom patch type %d is error!\n", type);
		ret = -1;
	}

	return ret;
}

INT32 wmt_ctrl_update_patch_version(P_WMT_CTRL_DATA pWmtCtrlData)
{
	P_DEV_WMT pDev = &gDevWmt;	/* single instance */
	INT32 iRet;
	UINT8 cmdStr[NAME_MAX + 1] = { 0 };

	osal_snprintf(cmdStr, NAME_MAX, "update_patch_version");
	iRet = wmt_ctrl_ul_cmd(pDev, cmdStr);
	if (iRet) {
		WMT_WARN_FUNC("wmt_ctrl_ul_cmd fail(%d)\n", iRet);
		return -1;
	}
	return 0;
}

INT32 wmt_ctrl_soc_paldo_ctrl(P_WMT_CTRL_DATA pWmtCtrlData)
{
	INT32 iRet = 0;
	ENUM_PALDO_TYPE ept = pWmtCtrlData->au4CtrlData[0];
	ENUM_PALDO_OP epo = pWmtCtrlData->au4CtrlData[1];

	WMT_DBG_FUNC("ept(%d),epo(%d)\n", ept, epo);
	iRet = wmt_plat_soc_paldo_ctrl(ept, epo);
	if (iRet) {
		if (ept == PMIC_CHIPID_PALDO) {
			/* special handling for PMIC CHIPID */
			pWmtCtrlData->au4CtrlData[2] = iRet;
		} else {
			/* for other PA handling */
			WMT_ERR_FUNC("soc palod ctrl fail(%d)\n", iRet);
		}
	}

	return iRet;
}

INT32 wmt_ctrl_soc_wakeup_consys(P_WMT_CTRL_DATA pWmtCtrlData)
{
	INT32 iRet = 0;

	iRet = mtk_wcn_stp_wakeup_consys();
	if (iRet)
		WMT_ERR_FUNC("soc palod ctrl fail(%d)\n", iRet);

	return iRet;
}

static INT32 wmt_ctrl_bgw_desense_ctrl(P_WMT_CTRL_DATA pWmtCtrlData)
{
	UINT32 cmd = pWmtCtrlData->au4CtrlData[0];

	WMT_INFO_FUNC("wmt-ctrl:send native cmd(%d)\n", cmd);
	wmt_dev_send_cmd_to_daemon(cmd);

	return 0;
}

#if CFG_WMT_LTE_COEX_HANDLING
static INT32 wmt_ctrl_get_tdm_req_antsel(P_WMT_CTRL_DATA pWmtCtrlData)
{
	INT32 antsel_index = wmt_plat_get_tdm_antsel_index();

	if (antsel_index >= 0)
		pWmtCtrlData->au4CtrlData[0] = antsel_index;
	else
		pWmtCtrlData->au4CtrlData[0] = 0xff;

	WMT_INFO_FUNC("get tdm req antsel index is %d\n", antsel_index);

	return 0;
}
#endif

static INT32 wmt_ctrl_evt_parser(P_WMT_CTRL_DATA pWmtCtrlData)
{
	INT32 ret = -1;
	UINT32 evt_idx = (UINT32) pWmtCtrlData->au4CtrlData[0];
	UINT8 *p_buf = NULL;

	static UINT8 sleep_evt[] = { 0x02, 0x03, 0x02, 0x00, 0x00, 0x01 };
	static UINT8 wakeup_evt[] = { 0x02, 0x03, 0x02, 0x00, 0x00, 0x03 };
	static UINT8 hostawake_evt[] = { 0x02, 0x03, 0x02, 0x00, 0x00, 0x02 };
	static UINT8 *evt_array[] = { sleep_evt, wakeup_evt, hostawake_evt };

	p_buf = evt_array[evt_idx - 1];

	WMT_INFO_FUNC("evt index:%d,p_buf:%p\n", evt_idx, p_buf);

	ret = mtk_wcn_consys_stp_btif_parser_wmt_evt(p_buf, 6);
	if (ret == 1) {
		WMT_INFO_FUNC("parser wmt evt from BTIF buf is OK\n");
		return 0;
	}
	WMT_ERR_FUNC("parser wmt evt from BTIF buf fail(%d)\n", ret);
	return -1;
}

INT32 wmt_ctrl_stp_conf_ex(WMT_STP_CONF_TYPE type, UINT32 value)
{
	INT32 iRet = -1;

	switch (type) {
	case WMT_STP_CONF_EN:
		iRet = mtk_wcn_stp_enable(value);
		break;

	case WMT_STP_CONF_RDY:
		iRet = mtk_wcn_stp_ready(value);
		break;

	case WMT_STP_CONF_MODE:
		mtk_wcn_stp_set_mode(value);
		iRet = 0;
		break;

	default:
		WMT_WARN_FUNC("invalid type(%d) value(%d)\n", type, value);
		break;
	}
	return iRet;
}


INT32 wmt_ctrl_stp_conf(P_WMT_CTRL_DATA pWmtCtrlData)
{
	INT32 iRet = -1;
	P_DEV_WMT pDev = &gDevWmt;	/* single instance */
	UINT32 type;
	UINT32 value;

	if (!osal_test_bit(WMT_STAT_STP_OPEN, &pDev->state)) {
		WMT_WARN_FUNC("CTRL_STP_ENABLE but invalid Handle of WmtStp\n");
		return -1;
	}

	type = pWmtCtrlData->au4CtrlData[0];
	value = pWmtCtrlData->au4CtrlData[1];
	iRet = wmt_ctrl_stp_conf_ex(type, value);

	if (!iRet) {
		if (type == WMT_STP_CONF_EN) {
			if (value) {
				osal_set_bit(WMT_STAT_STP_EN, &pDev->state);
				WMT_DBG_FUNC("enable STP\n");
			} else {
				osal_clear_bit(WMT_STAT_STP_EN, &pDev->state);
				WMT_DBG_FUNC("disable STP\n");
			}
		}
		if (type == WMT_STP_CONF_RDY) {
			if (value) {
				osal_set_bit(WMT_STAT_STP_RDY, &pDev->state);
				WMT_DBG_FUNC("STP ready\n");
			} else {
				osal_clear_bit(WMT_STAT_STP_RDY, &pDev->state);
				WMT_DBG_FUNC("STP not ready\n");
			}
		}
	}

	return iRet;
}


INT32 wmt_ctrl_free_patch(P_WMT_CTRL_DATA pWmtCtrlData)
{
	UINT32 patchSeq = pWmtCtrlData->au4CtrlData[0];

	WMT_DBG_FUNC("BF free patch, gDevWmt.pPatch(%p)\n", gDevWmt.pPatch);
	if (gDevWmt.pPatch != NULL)
		wmt_dev_patch_put((osal_firmware **) &(gDevWmt.pPatch));
	WMT_DBG_FUNC("AF free patch, gDevWmt.pPatch(%p)\n", gDevWmt.pPatch);
	if (patchSeq == gDevWmt.patchNum)
		WMT_DBG_FUNC("the %d patch has been download\n", patchSeq);
	return 0;
}

INT32 wmt_ctrl_get_patch_name(P_WMT_CTRL_DATA pWmtCtrlData)
{
	PUINT8 pBuf = (PUINT8) pWmtCtrlData->au4CtrlData[0];

	osal_memcpy(pBuf, gDevWmt.cPatchName, osal_sizeof(gDevWmt.cPatchName));
	return 0;
}

INT32 wmt_ctrl_crystal_triming_put(P_WMT_CTRL_DATA pWmtCtrlData)
{
	WMT_DBG_FUNC("BF free patch, gDevWmt.pPatch(%p)\n", gDevWmt.pPatch);
	if (gDevWmt.pNvram != NULL)
		wmt_dev_patch_put((osal_firmware **) &(gDevWmt.pNvram));
	WMT_DBG_FUNC("AF free patch, gDevWmt.pNvram(%p)\n", gDevWmt.pNvram);
	return 0;
}


INT32 wmt_ctrl_crystal_triming_get(P_WMT_CTRL_DATA pWmtCtrlData)
{
	INT32 iRet = 0x0;
	PUINT8 pFileName = (PUINT8) pWmtCtrlData->au4CtrlData[0];
	PPUINT8 ppBuf = (PPUINT8) pWmtCtrlData->au4CtrlData[1];
	PUINT32 pSize = (PUINT32) pWmtCtrlData->au4CtrlData[2];
	osal_firmware *pNvram = NULL;

	if ((pFileName == NULL) || (pSize == NULL)) {
		WMT_ERR_FUNC("parameter error, pFileName(%p), pSize(%p)\n", pFileName, pSize);
		iRet = -1;
		return iRet;
	}
	if (wmt_dev_patch_get(pFileName, &pNvram) == 0) {
		*ppBuf = (PUINT8)(pNvram)->data;
		*pSize = (pNvram)->size;
		gDevWmt.pNvram = pNvram;
		return 0;
	}
	return -1;
}


INT32 wmt_ctrl_get_patch(P_WMT_CTRL_DATA pWmtCtrlData)
{
	PUINT8 pFullPatchName = NULL;
	PUINT8 pDefPatchName = NULL;
	PPUINT8 ppBuf = (PPUINT8) pWmtCtrlData->au4CtrlData[2];
	PUINT32 pSize = (PUINT32) pWmtCtrlData->au4CtrlData[3];
	osal_firmware *pPatch = NULL;

	pFullPatchName = (PUINT8) pWmtCtrlData->au4CtrlData[1];
	WMT_DBG_FUNC("BF get patch, pPatch(%p)\n", pPatch);
	if ((pFullPatchName != NULL)
	    && (wmt_dev_patch_get(pFullPatchName, &pPatch) == 0)) {
		/*get full name patch success */
		WMT_DBG_FUNC("get full patch name(%s) buf(0x%p) size(%zu)\n",
			     pFullPatchName, (pPatch)->data, (pPatch)->size);
		WMT_DBG_FUNC("AF get patch, pPatch(%p)\n", pPatch);
		*ppBuf = (PUINT8)(pPatch)->data;
		*pSize = (pPatch)->size;
		gDevWmt.pPatch = pPatch;
		return 0;
	}

	pDefPatchName = (PUINT8) pWmtCtrlData->au4CtrlData[0];
	if ((pDefPatchName != NULL)
	    && (wmt_dev_patch_get(pDefPatchName, &pPatch) == 0)) {
		WMT_DBG_FUNC("get def patch name(%s) buf(0x%p) size(%zu)\n",
			     pDefPatchName, (pPatch)->data, (pPatch)->size);
		WMT_DBG_FUNC("AF get patch, pPatch(%p)\n", pPatch);
		/*get full name patch success */
		*ppBuf = (PUINT8)(pPatch)->data;
		*pSize = (pPatch)->size;
		gDevWmt.pPatch = pPatch;
		return 0;
	}
	return -1;

}

INT32 wmt_ctrl_host_baudrate_set(P_WMT_CTRL_DATA pWmtCtrlData)
{
	INT32 iRet = -1;
	INT8 cmdStr[NAME_MAX + 1] = { 0 };
	UINT32 u4Baudrate = pWmtCtrlData->au4CtrlData[0];
	UINT32 u4FlowCtrl = pWmtCtrlData->au4CtrlData[1];

	WMT_DBG_FUNC("baud(%d), flowctrl(%d)\n", u4Baudrate, u4FlowCtrl);

	if (osal_test_bit(WMT_STAT_STP_OPEN, &gDevWmt.state)) {
		osal_snprintf(cmdStr, NAME_MAX, "baud_%d_%d", u4Baudrate, u4FlowCtrl);
		iRet = wmt_ctrl_ul_cmd(&gDevWmt, cmdStr);
		if (iRet)
			WMT_WARN_FUNC("CTRL_BAUDRATE baud(%d), flowctrl(%zu) fail(%d)\n",
				      u4Baudrate, pWmtCtrlData->au4CtrlData[1], iRet);
		else
			WMT_DBG_FUNC("CTRL_BAUDRATE baud(%d), flowctrl(%d) ok\n",
				     u4Baudrate, u4FlowCtrl);
	} else
		WMT_INFO_FUNC("CTRL_BAUDRATE but invalid Handle of WmtStp\n");
	return iRet;
}

INT32 wmt_ctrl_sdio_hw(P_WMT_CTRL_DATA pWmtCtrlData)
{
	INT32 iRet = 0;
	UINT32 statBit = WMT_STAT_SDIO1_ON;
	P_DEV_WMT pDev = &gDevWmt;	/* single instance */

	WMT_SDIO_SLOT_NUM sdioSlotNum = pWmtCtrlData->au4CtrlData[0];
	ENUM_FUNC_STATE funcState = pWmtCtrlData->au4CtrlData[1];

	if ((sdioSlotNum == WMT_SDIO_SLOT_INVALID)
	    || (sdioSlotNum >= WMT_SDIO_SLOT_MAX)) {
		WMT_WARN_FUNC("CTRL_SDIO_SLOT(%d) but invalid slot num\n", sdioSlotNum);
		return -1;
	}

	WMT_DBG_FUNC("WMT_CTRL_SDIO_HW (0x%x, %d)\n", sdioSlotNum, funcState);

	if (sdioSlotNum == WMT_SDIO_SLOT_SDIO2)
		statBit = WMT_STAT_SDIO2_ON;

	if (funcState) {
		if (osal_test_and_set_bit(statBit, &pDev->state)) {
			WMT_WARN_FUNC("CTRL_SDIO_SLOT slotNum(%d) already ON\n", sdioSlotNum);
			/* still return 0 */
			iRet = 0;
		} else
			iRet = wmt_plat_sdio_ctrl(sdioSlotNum, FUNC_ON);
	} else {
		if (osal_test_and_clear_bit(statBit, &pDev->state))
			iRet = wmt_plat_sdio_ctrl(sdioSlotNum, FUNC_OFF);
		else {
			WMT_WARN_FUNC("CTRL_SDIO_SLOT slotNum(%d) already OFF\n", sdioSlotNum);
			/* still return 0 */
			iRet = 0;
		}
	}

	return iRet;
}

INT32 wmt_ctrl_sdio_func(P_WMT_CTRL_DATA pWmtCtrlData)
{
	INT32 iRet = -1;
	UINT32 statBit = WMT_STAT_SDIO_WIFI_ON;
	INT32 retry = 10;
	P_DEV_WMT pDev = &gDevWmt;	/* single instance */
	WMT_SDIO_FUNC_TYPE sdioFuncType = pWmtCtrlData->au4CtrlData[0];
	UINT32 u4On = pWmtCtrlData->au4CtrlData[1];

	if (sdioFuncType >= WMT_SDIO_FUNC_MAX) {
		WMT_ERR_FUNC("CTRL_SDIO_FUNC, invalid func type (%d)\n", sdioFuncType);
		return -1;
	}

	if (sdioFuncType == WMT_SDIO_FUNC_STP)
		statBit = WMT_STAT_SDIO_STP_ON;

	if (u4On) {
		if (osal_test_bit(statBit, &pDev->state)) {
			WMT_WARN_FUNC("CTRL_SDIO_FUNC(%d) but already ON\n", sdioFuncType);
			iRet = 0;
		} else {
			while (retry-- > 0 && iRet != 0) {
				if (iRet) {
					/* sleep 150ms before sdio slot ON ready */
					osal_sleep_ms(150);
				}
				iRet =
				    mtk_wcn_hif_sdio_wmt_control(sdioFuncType, MTK_WCN_BOOL_TRUE);
				if (iRet == HIF_SDIO_ERR_NOT_PROBED) {
					/* not probed case, retry */
					continue;
				} else if (iRet == HIF_SDIO_ERR_CLT_NOT_REG) {
					/* For WiFi, client not reg yet, no need to retry,
					 * WiFi function can work any time when wlan.ko is insert into system
					 */
					iRet = 0;
				} else {
					/* other fail cases, stop */
					break;
				}
			}
			if (iRet)
				WMT_ERR_FUNC
				    ("mtk_wcn_hif_sdio_wmt_control(%d, TRUE) fail(%d) retry(%d)\n",
				     sdioFuncType, iRet, retry);
			else
				osal_set_bit(statBit, &pDev->state);
		}
	} else {
		if (osal_test_bit(statBit, &pDev->state)) {
			iRet = mtk_wcn_hif_sdio_wmt_control(sdioFuncType, MTK_WCN_BOOL_FALSE);
			if (iRet)
				WMT_ERR_FUNC("mtk_wcn_hif_sdio_wmt_control(%d, FALSE) fail(%d)\n",
					     sdioFuncType, iRet);
			/*any way, set to OFF state */
			osal_clear_bit(statBit, &pDev->state);
		} else {
			WMT_WARN_FUNC("CTRL_SDIO_FUNC(%d) but already OFF\n", sdioFuncType);
			iRet = 0;
		}
	}

	return iRet;
}

#if 0
/* TODO: [FixMe][GeorgeKuo]: remove unused function. get hwver from core is not needed. */
INT32 wmt_ctrl_hwver_get(P_WMT_CTRL_DATA pWmtCtrlData)
{
	P_DEV_WMT pDev = &gDevWmt;	/* single instance */

	return 0;
}
#endif

INT32 wmt_ctrl_hwidver_set(P_WMT_CTRL_DATA pWmtCtrlData)
{
	P_DEV_WMT pDev = &gDevWmt;	/* single instance */

	/* input sanity check is done in wmt_ctrl() */
	pDev->chip_id = (pWmtCtrlData->au4CtrlData[0] & 0xFFFF0000) >> 16;
	pDev->hw_ver = pWmtCtrlData->au4CtrlData[0] & 0x0000FFFF;
	pDev->fw_ver = pWmtCtrlData->au4CtrlData[1] & 0x0000FFFF;

	return 0;
}

static INT32 wmt_ctrl_gps_sync_set(P_WMT_CTRL_DATA pData)
{
	INT32 iret;

	WMT_DBG_FUNC("ctrl GPS_SYNC(%d)\n",
		      (pData->au4CtrlData[0] == 0) ? PIN_STA_DEINIT : PIN_STA_MUX);
	iret =
	    wmt_plat_gpio_ctrl(PIN_GPS_SYNC,
			       (pData->au4CtrlData[0] == 0) ? PIN_STA_DEINIT : PIN_STA_MUX);

	if (iret)
		WMT_WARN_FUNC("ctrl GPS_SYNC(%d) fail!(%d) ignore it...\n",
			      (pData->au4CtrlData[0] == 0) ? PIN_STA_DEINIT : PIN_STA_MUX, iret);

	return 0;
}


static INT32 wmt_ctrl_gps_lna_set(P_WMT_CTRL_DATA pData)
{
	INT32 iret;

	WMT_DBG_FUNC("ctrl GPS_LNA(%d)\n",
		      (pData->au4CtrlData[0] == 0) ? PIN_STA_DEINIT : PIN_STA_OUT_H);
	iret =
	    wmt_plat_gpio_ctrl(PIN_GPS_LNA,
			       (pData->au4CtrlData[0] == 0) ? PIN_STA_DEINIT : PIN_STA_OUT_H);

	if (iret)
		WMT_WARN_FUNC("ctrl GPS_SYNC(%d) fail!(%d) ignore it...\n",
			      (pData->au4CtrlData[0] == 0) ? PIN_STA_DEINIT : PIN_STA_OUT_H, iret);

	return 0;
}


INT32 wmt_ctrl_stp_rst(P_WMT_CTRL_DATA pWmtCtrlData)
{
	return 0;
}

INT32 wmt_ctrl_get_wmt_conf(P_WMT_CTRL_DATA pWmtCtrlData)
{
	P_DEV_WMT pDev = &gDevWmt;	/* single instance */

	pWmtCtrlData->au4CtrlData[0] = (size_t) &pDev->rWmtGenConf;

	return 0;
}

INT32 wmt_ctrl_others(P_WMT_CTRL_DATA pWmtCtrlData)
{
	WMT_ERR_FUNC("wmt_ctrl_others, invalid CTRL ID (%d)\n", pWmtCtrlData->ctrlId);
	return -1;
}


INT32 wmt_ctrl_set_stp_dbg_info(P_WMT_CTRL_DATA pWmtCtrlData)
{
	PUINT8 pRomVer = NULL;
	P_WMT_PATCH pPatch = NULL;
	UINT32 chipID = 0;

	chipID = pWmtCtrlData->au4CtrlData[0];
	pRomVer = (PUINT8) (pWmtCtrlData->au4CtrlData[1]);
	pPatch = (P_WMT_PATCH)(pWmtCtrlData->au4CtrlData[2]);
	if (!pRomVer) {
		WMT_ERR_FUNC("pRomVer null pointer\n");
		return -1;
	}

	if (!pPatch) {
		WMT_ERR_FUNC("pPatch null pointer\n");
		return -2;
	}
	WMT_DBG_FUNC("chipid(0x%x),rom(%s),patch date(%s),patch plat(%s)\n", chipID, pRomVer,
		     pPatch->ucDateTime, pPatch->ucPLat);
	return stp_dbg_set_version_info(chipID, pRomVer, &(pPatch->ucDateTime[0]),
					&(pPatch->ucPLat[0]));
}

static INT32 wmt_ctrl_trg_assert(P_WMT_CTRL_DATA pWmtCtrlData)
{
	INT32 iRet = -1;

	ENUM_WMTDRV_TYPE_T drv_type;
	UINT32 reason = 0;
	PUINT8 keyword;

	drv_type = pWmtCtrlData->au4CtrlData[0];
	reason = pWmtCtrlData->au4CtrlData[1];
	keyword = (PUINT8) pWmtCtrlData->au4CtrlData[2];
	WMT_INFO_FUNC("wmt-ctrl:drv_type(%d),reason(%d),keyword(%s)\n", drv_type, reason, keyword);

	if (wmt_dev_is_close())
		WMT_INFO_FUNC("WMT is closing, don't trigger assert\n");
	else if (chip_reset_only == 1)
		WMT_INFO_FUNC("Do chip reset only, don't trigger assert\n");
	else if (mtk_wcn_stp_get_wmt_trg_assert() == 0) {
		mtk_wcn_stp_dbg_dump_package();
		mtk_wcn_stp_set_wmt_trg_assert(1);
		mtk_wcn_stp_assert_flow_ctrl(1);

		iRet = mtk_wcn_stp_wmt_trg_assert();
		if (iRet == 0) {
			wmt_lib_set_host_assert_info(drv_type, reason, 1);
			stp_dbg_set_keyword(keyword);
		}
	} else
		WMT_INFO_FUNC("do trigger assert & chip reset in stp noack\n");

	return 0;
}
