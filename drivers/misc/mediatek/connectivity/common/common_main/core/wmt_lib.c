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
#define DFT_TAG         "[WMT-LIB]"



/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "osal_typedef.h"
#include "wmt_dbg.h"

#include "wmt_dev.h"
#include "wmt_lib.h"
#include "wmt_conf.h"
#include "wmt_core.h"
#include "wmt_plat.h"
#include "wmt_plat_stub.h"
#include "wmt_detect.h"
#include "mtk_wcn_consys_hw.h"

#include "stp_core.h"
#include "btm_core.h"
#include "psm_core.h"
#include "stp_sdio.h"
#include "stp_dbg.h"
#include "wmt_step.h"
#include <linux/workqueue.h>
#include <linux/rtc.h>

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/



/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/* A table for translation: enum CMB_STUB_AIF_X=>WMT_IC_PIN_STATE */
static const WMT_IC_PIN_STATE cmb_aif2pin_stat[] = {
	[CMB_STUB_AIF_0] = WMT_IC_AIF_0,
	[CMB_STUB_AIF_1] = WMT_IC_AIF_1,
	[CMB_STUB_AIF_2] = WMT_IC_AIF_2,
	[CMB_STUB_AIF_3] = WMT_IC_AIF_3,
	[CMB_STUB_AIF_4] = WMT_IC_PIN_STATE_MAX,
};

#if CFG_WMT_PS_SUPPORT
static UINT32 gPsIdleTime = STP_PSM_IDLE_TIME_SLEEP;
static UINT32 gPsEnable = 1;
static PF_WMT_SDIO_PSOP sdio_own_ctrl;
static PF_WMT_SDIO_DEBUG sdio_reg_rw;
#endif
#ifdef CONFIG_MTK_COMBO_CHIP_DEEP_SLEEP_SUPPORT
static PF_WMT_SDIO_DEEP_SLEEP sdio_deep_sleep_flag_set;
#endif


#define WMT_STP_CPUPCR_BUF_SIZE 73728
static UINT8 g_cpupcr_buf[WMT_STP_CPUPCR_BUF_SIZE] = { 0 };
static UINT32 g_quick_sleep_ctrl = 1;
static UINT32 g_fw_patch_update_rst;
static u64 fw_patch_rst_time;

#define ASSERT_KEYWORD_LENGTH 20
struct assert_work_st {
	struct work_struct work;
	ENUM_WMTDRV_TYPE_T type;
	UINT32 reason;
	UINT8 keyword[ASSERT_KEYWORD_LENGTH];
};

static struct assert_work_st wmt_assert_work;

static INT32 g_bt_no_acl_link = 1;
static INT32 g_bt_no_br_acl_link = 1;

#define CONSYS_MET_WAIT	(1000*10) /* ms */
#define MET_DUMP_MAX_NUM (1)
#define MET_DUMP_SIZE (4*MET_DUMP_MAX_NUM)
#define EMI_MET_READ_OFFSET	0x0
#define EMI_MET_WRITE_OFFSET	0x4
#define EMI_MET_DATA_OFFSET	0x8
#define FW_PATCH_UPDATE_RST_DURATION 180 /* 180 seconds */

#define WMT_LIB_DMP_CONSYS_MAX_TIMES 10

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

DEV_WMT gDevWmt;

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

#if CFG_WMT_PS_SUPPORT
static MTK_WCN_BOOL wmt_lib_ps_action(MTKSTP_PSM_ACTION_T action);
static MTK_WCN_BOOL wmt_lib_ps_do_sleep(VOID);
static MTK_WCN_BOOL wmt_lib_ps_do_wakeup(VOID);
static MTK_WCN_BOOL wmt_lib_ps_do_host_awake(VOID);
static INT32 wmt_lib_ps_handler(MTKSTP_PSM_ACTION_T action);
#endif

static MTK_WCN_BOOL wmt_lib_put_op(P_OSAL_OP_Q pOpQ, P_OSAL_OP pLxOp);

static P_OSAL_OP wmt_lib_get_op(P_OSAL_OP_Q pOpQ);

static INT32 wmtd_thread(PVOID pvData);
static INT32 met_thread(PVOID pvData);
static INT32 wmtd_worker_thread(PVOID pvData);

static INT32 wmt_lib_pin_ctrl(WMT_IC_PIN_ID id, WMT_IC_PIN_STATE stat, UINT32 flag);
static MTK_WCN_BOOL wmt_lib_hw_state_show(VOID);
static VOID wmt_lib_utc_sync_timeout_handler(timer_handler_arg arg);
static VOID wmt_lib_utc_sync_worker_handler(struct work_struct *work);
static VOID wmt_lib_wmtd_worker_thread_timeout_handler(timer_handler_arg);
static VOID wmt_lib_wmtd_worker_thread_work_handler(struct work_struct *work);

static VOID wmt_lib_assert_work_cb(struct work_struct *work);
/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
INT32 __weak mtk_wcn_consys_stp_btif_dpidle_ctrl(UINT32 en_flag)
{
	WMT_ERR_FUNC("mtk_wcn_consys_stp_btif_dpidle_ctrl is not define!!!\n");

	return 0;
}

INT32 wmt_lib_wlan_lock_aquire(VOID)
{
	return osal_lock_sleepable_lock(&gDevWmt.wlan_lock);
}

VOID wmt_lib_wlan_lock_release(VOID)
{
	osal_unlock_sleepable_lock(&gDevWmt.wlan_lock);
}

INT32 wmt_lib_wlan_lock_trylock(VOID)
{
	return osal_trylock_sleepable_lock(&gDevWmt.wlan_lock);
}

INT32 wmt_lib_idc_lock_aquire(VOID)
{
	return osal_lock_sleepable_lock(&gDevWmt.idc_lock);
}

VOID wmt_lib_idc_lock_release(VOID)
{
	osal_unlock_sleepable_lock(&gDevWmt.idc_lock);
}

INT32 wmt_lib_psm_lock_aquire(VOID)
{
	return osal_lock_sleepable_lock(&gDevWmt.psm_lock);
}

void wmt_lib_psm_lock_release(VOID)
{
	osal_unlock_sleepable_lock(&gDevWmt.psm_lock);
}

INT32 wmt_lib_psm_lock_trylock(VOID)
{
	return osal_trylock_sleepable_lock(&gDevWmt.psm_lock);
}

INT32 wmt_lib_assert_lock_aquire(VOID)
{
	return osal_lock_sleepable_lock(&gDevWmt.assert_lock);
}

VOID wmt_lib_assert_lock_release(VOID)
{
	osal_unlock_sleepable_lock(&gDevWmt.assert_lock);
}

INT32 wmt_lib_assert_lock_trylock(VOID)
{
	return osal_trylock_sleepable_lock(&gDevWmt.assert_lock);
}

INT32 wmt_lib_mpu_lock_aquire(VOID)
{
	return osal_lock_sleepable_lock(&gDevWmt.mpu_lock);
}

VOID wmt_lib_mpu_lock_release(VOID)
{
	osal_unlock_sleepable_lock(&gDevWmt.mpu_lock);
}

INT32 wmt_lib_power_lock_aquire(VOID)
{
	return osal_lock_sleepable_lock(&gDevWmt.power_lock);
}

VOID wmt_lib_power_lock_release(VOID)
{
	osal_unlock_sleepable_lock(&gDevWmt.power_lock);
}

INT32 wmt_lib_power_lock_trylock(VOID)
{
	return osal_trylock_sleepable_lock(&gDevWmt.power_lock);
}

INT32 DISABLE_PSM_MONITOR(VOID)
{
	INT32 ret = 0;
	PUINT8 pbuf = NULL;
	INT32 len = 0;

	/* osal_lock_sleepable_lock(&gDevWmt.psm_lock); */
	ret = wmt_lib_psm_lock_aquire();
	if (ret) {
		WMT_ERR_FUNC("--->lock psm_lock failed, ret=%d\n", ret);
		return ret;
	}
#if CFG_WMT_PS_SUPPORT
	ret = wmt_lib_ps_disable();
	if (ret) {
		WMT_ERR_FUNC("wmt_lib_ps_disable fail, ret=%d\n", ret);
		wmt_lib_psm_lock_release();
		if (mtk_wcn_stp_coredump_start_get() == 0 &&
			chip_reset_only == 0 &&
			mtk_wcn_stp_get_wmt_trg_assert() == 0) {
			pbuf = "wmt_lib_ps_disable fail, just collect SYS_FTRACE to DB";
			len = osal_strlen(pbuf);
			stp_dbg_trigger_collect_ftrace(pbuf, len);
			wmt_lib_trigger_reset();
		}
	}
#endif
	return ret;
}

VOID ENABLE_PSM_MONITOR(VOID)
{
#if CFG_WMT_PS_SUPPORT
	wmt_lib_ps_enable();
#endif
	/* osal_unlock_sleepable_lock(&gDevWmt.psm_lock); */
	wmt_lib_psm_lock_release();
}


INT32 wmt_lib_init(VOID)
{
	INT32 iRet;
	UINT32 i;
	P_DEV_WMT pDevWmt;
	P_OSAL_THREAD pThread;
	P_OSAL_THREAD pWorkerThread;
	ENUM_WMT_CHIP_TYPE chip_type;

	/* create->init->start */
	/* 1. create: static allocation with zero initialization */
	pDevWmt = &gDevWmt;
	osal_memset(&gDevWmt, 0, sizeof(gDevWmt));
	if (wmt_detect_get_chip_type() == WMT_CHIP_TYPE_SOC) {
		iRet = wmt_conf_read_file();
		if (iRet) {
			WMT_ERR_FUNC("read wmt config file fail(%d)\n", iRet);
			return -1;
		}
	}
	osal_op_history_init(&pDevWmt->wmtd_op_history, 16);
	osal_op_history_init(&pDevWmt->worker_op_history, 8);

	pThread = &gDevWmt.thread;

	/* Create mtk_wmtd thread */
	osal_strncpy(pThread->threadName, "mtk_wmtd", sizeof(pThread->threadName));
	pThread->pThreadData = (PVOID) pDevWmt;
	pThread->pThreadFunc = (PVOID) wmtd_thread;
	iRet = osal_thread_create(pThread);
	if (iRet) {
		WMT_ERR_FUNC("osal_thread_create(0x%p) fail(%d)\n", pThread, iRet);
		return -2;
	}

	/* create worker timer */
	gDevWmt.worker_timer.timeoutHandler = wmt_lib_wmtd_worker_thread_timeout_handler;
	gDevWmt.worker_timer.timeroutHandlerData = 0;
	osal_timer_create(&gDevWmt.worker_timer);
	pWorkerThread = &gDevWmt.worker_thread;
	INIT_WORK(&pDevWmt->wmtd_worker_thread_work, wmt_lib_wmtd_worker_thread_work_handler);

	/* Create wmtd_worker thread */
	osal_strncpy(pWorkerThread->threadName, "mtk_wmtd_worker", sizeof(pWorkerThread->threadName));
	pWorkerThread->pThreadData = (PVOID) pDevWmt;
	pWorkerThread->pThreadFunc = (PVOID) wmtd_worker_thread;
	iRet = osal_thread_create(pWorkerThread);
	if (iRet) {
		WMT_ERR_FUNC("osal_thread_create(0x%p) fail(%d)\n", pWorkerThread, iRet);
		return -2;
	}

	/* init timer */
	pDevWmt->utc_sync_timer.timeoutHandler = wmt_lib_utc_sync_timeout_handler;
	osal_timer_create(&pDevWmt->utc_sync_timer);
	osal_timer_start(&pDevWmt->utc_sync_timer, UTC_SYNC_TIME);
	INIT_WORK(&pDevWmt->utcSyncWorker, wmt_lib_utc_sync_worker_handler);

	/* 2. initialize */
	/* Initialize wmt_core */

	iRet = wmt_core_init();
	if (iRet) {
		WMT_ERR_FUNC("wmt_core_init() fail(%d)\n", iRet);
		return -1;
	}

	/* Initialize WMTd Thread Information: Thread */
	osal_event_init(&pDevWmt->rWmtdWq);
	osal_event_init(&pDevWmt->rWmtdWorkerWq);
	osal_sleepable_lock_init(&pDevWmt->psm_lock);
	osal_sleepable_lock_init(&pDevWmt->idc_lock);
	osal_sleepable_lock_init(&pDevWmt->wlan_lock);
	osal_sleepable_lock_init(&pDevWmt->assert_lock);
	osal_sleepable_lock_init(&pDevWmt->mpu_lock);
	osal_sleepable_lock_init(&pDevWmt->power_lock);
	osal_sleepable_lock_init(&pDevWmt->rActiveOpQ.sLock);
	osal_sleepable_lock_init(&pDevWmt->rWorkerOpQ.sLock);
	osal_sleepable_lock_init(&pDevWmt->rFreeOpQ.sLock);
	pDevWmt->state.data = 0;

	atomic_set(&pDevWmt->state_dmp_req.version, 0);
	for (i = 0; i < WMT_LIB_DMP_SLOT; i++)
		osal_sleepable_lock_init(&(pDevWmt->state_dmp_req.consys_ops[i].lock));

	/* Initialize op queue */
	RB_INIT(&pDevWmt->rFreeOpQ, WMT_OP_BUF_SIZE);
	RB_INIT(&pDevWmt->rActiveOpQ, WMT_OP_BUF_SIZE);
	RB_INIT(&pDevWmt->rWorkerOpQ, WMT_OP_BUF_SIZE);
	/* Put all to free Q */
	for (i = 0; i < WMT_OP_BUF_SIZE; i++) {
		osal_signal_init(&(pDevWmt->arQue[i].signal));
		wmt_lib_put_op(&pDevWmt->rFreeOpQ, &(pDevWmt->arQue[i]));
	}

	/* initialize stp resources */
	osal_event_init(&pDevWmt->rWmtRxWq);

	/*function driver callback */
	for (i = 0; i < WMTDRV_TYPE_WIFI; i++)
		pDevWmt->rFdrvCb.fDrvRst[i] = NULL;

	pDevWmt->hw_ver = WMTHWVER_MAX;
	WMT_DBG_FUNC("***********Init, hw->ver = %x\n", pDevWmt->hw_ver);

	/* TODO:[FixMe][GeorgeKuo]: wmt_lib_conf_init */
	/* initialize default configurations */
	/* i4Result = wmt_lib_conf_init(VOID); */
	/* WMT_WARN_FUNC("wmt_drv_conf_init(%d)\n", i4Result); */

	osal_signal_init(&pDevWmt->cmdResp);
	osal_event_init(&pDevWmt->cmdReq);
	/* initialize platform resources */

	if (gDevWmt.rWmtGenConf.cfgExist != 0) {
		PWR_SEQ_TIME pwrSeqTime;

		pwrSeqTime.ldoStableTime = gDevWmt.rWmtGenConf.pwr_on_ldo_slot;
		pwrSeqTime.rstStableTime = gDevWmt.rWmtGenConf.pwr_on_rst_slot;
		pwrSeqTime.onStableTime = gDevWmt.rWmtGenConf.pwr_on_on_slot;
		pwrSeqTime.offStableTime = gDevWmt.rWmtGenConf.pwr_on_off_slot;
		pwrSeqTime.rtcStableTime = gDevWmt.rWmtGenConf.pwr_on_rtc_slot;
		WMT_INFO_FUNC("set pwr on seq par to hw conf\n");
		WMT_INFO_FUNC("ldo(%d)rst(%d)on(%d)off(%d)rtc(%d)\n", pwrSeqTime.ldoStableTime,
				pwrSeqTime.rstStableTime, pwrSeqTime.onStableTime,
				pwrSeqTime.offStableTime, pwrSeqTime.rtcStableTime);
		iRet = wmt_plat_init(&pwrSeqTime, gDevWmt.rWmtGenConf.co_clock_flag & 0x0f);
	} else {
		WMT_ERR_FUNC("no pwr on seq and clk par found\n");
		iRet = wmt_plat_init(NULL, 0);
	}
	chip_type = wmt_detect_get_chip_type();
	if (chip_type == WMT_CHIP_TYPE_SOC)
		gDevWmt.rWmtGenConf.co_clock_flag = wmt_plat_soc_co_clock_flag_get();

	if (iRet) {
		WMT_ERR_FUNC("wmt_plat_init() fail(%d)\n", iRet);
		return -3;
	}

#if CFG_WMT_PS_SUPPORT
	iRet = wmt_lib_ps_init();
	if (iRet) {
		WMT_ERR_FUNC("wmt_lib_ps_init() fail(%d)\n", iRet);
		return -4;
	}
#endif

	/* 3. start: start running mtk_wmtd */
	iRet = osal_thread_run(pThread);
	if (iRet) {
		WMT_ERR_FUNC("osal_thread_run(wmtd 0x%p) fail(%d)\n", pThread, iRet);
		return -5;
	}

	iRet = osal_thread_run(pWorkerThread);
	if (iRet) {
		WMT_ERR_FUNC("osal_thread_run(worker 0x%p) fail(%d)\n", pWorkerThread, iRet);
		return -5;
	}
	/*4. register irq callback to WMT-PLAT */
	wmt_plat_irq_cb_reg(wmt_lib_ps_irq_cb);
	/*5. register audio if control callback to WMT-PLAT */
	wmt_plat_aif_cb_reg(wmt_lib_set_aif);
	/*6. register function control callback to WMT-PLAT */
	wmt_plat_func_ctrl_cb_reg(mtk_wcn_wmt_func_ctrl_for_plat);

	wmt_plat_deep_idle_ctrl_cb_reg(mtk_wcn_consys_stp_btif_dpidle_ctrl);
	/*7 reset gps/bt state */

	mtk_wcn_wmt_system_state_reset();

#ifndef MTK_WCN_WMT_STP_EXP_SYMBOL_ABSTRACT
	mtk_wcn_wmt_exp_init();
#endif

#if CFG_WMT_LTE_COEX_HANDLING
	wmt_idc_init();
#endif

	INIT_WORK(&(wmt_assert_work.work), wmt_lib_assert_work_cb);

	WMT_DBG_FUNC("init success\n");
	return 0;
}


INT32 wmt_lib_deinit(VOID)
{
	INT32 iRet;
	P_DEV_WMT pDevWmt;
	P_OSAL_THREAD pThread;
	P_OSAL_THREAD pWorkerThread;
	INT32 i;
	INT32 iResult;
	struct vendor_patch_table *table = &(gDevWmt.patch_table);

	pDevWmt = &gDevWmt;
	pThread = &gDevWmt.thread;
	pWorkerThread = &gDevWmt.worker_thread;
	iResult = 0;

	/* stop->deinit->destroy */

	/* 1. stop: stop running mtk_wmtd */
	iRet = osal_thread_stop(pThread);
	if (iRet) {
		WMT_ERR_FUNC("osal_thread_stop(0x%p) fail(%d)\n", pThread, iRet);
		iResult += 1;
	}

	iRet = osal_thread_stop(pWorkerThread);
	if (iRet) {
		WMT_ERR_FUNC("osal_thread_stop(0x%p) fail(%d)\n", pWorkerThread, iRet);
		iResult += 1;
	}

	/* 2. deinit: */

#if CFG_WMT_PS_SUPPORT
	iRet = wmt_lib_ps_deinit();
	if (iRet) {
		WMT_ERR_FUNC("wmt_lib_ps_deinit fail(%d)\n", iRet);
		iResult += 2;
	}
#endif

	iRet = wmt_plat_deinit();
	if (iRet) {
		WMT_ERR_FUNC("wmt_plat_deinit fail(%d)\n", iRet);
		iResult += 4;
	}

	osal_event_deinit(&pDevWmt->cmdReq);
	osal_signal_deinit(&pDevWmt->cmdResp);

	/* de-initialize stp resources */
	osal_event_deinit(&pDevWmt->rWmtRxWq);

	for (i = 0; i < WMT_OP_BUF_SIZE; i++)
		osal_signal_deinit(&(pDevWmt->arQue[i].signal));

	osal_sleepable_lock_deinit(&pDevWmt->rFreeOpQ.sLock);
	osal_sleepable_lock_deinit(&pDevWmt->rActiveOpQ.sLock);
	osal_sleepable_lock_deinit(&pDevWmt->rWorkerOpQ.sLock);
	osal_sleepable_lock_deinit(&pDevWmt->power_lock);
	osal_sleepable_lock_deinit(&pDevWmt->mpu_lock);
	osal_sleepable_lock_deinit(&pDevWmt->idc_lock);
	osal_sleepable_lock_deinit(&pDevWmt->wlan_lock);
	osal_sleepable_lock_deinit(&pDevWmt->assert_lock);
	osal_sleepable_lock_deinit(&pDevWmt->psm_lock);

	for (i = 0; i < WMT_LIB_DMP_SLOT; i++)
		osal_sleepable_lock_deinit(&(pDevWmt->state_dmp_req.consys_ops[i].lock));

	osal_event_deinit(&pDevWmt->rWmtdWq);
	osal_event_deinit(&pDevWmt->rWmtdWorkerWq);

	for (i = 0; i < WMTDRV_TYPE_ANT; i++) {
		kfree(pDevWmt->pWmtRomPatchInfo[i]);
		pDevWmt->pWmtRomPatchInfo[i] = NULL;
	}

	iRet = wmt_core_deinit();
	if (iRet) {
		WMT_ERR_FUNC("wmt_core_deinit fail(%d)\n", iRet);
		iResult += 8;
	}

	/* 3. destroy */
	iRet = osal_thread_destroy(pThread);
	if (iRet) {
		WMT_ERR_FUNC("osal_thread_stop(0x%p) fail(%d)\n", pThread, iRet);
		iResult += 16;
	}

	iRet = osal_thread_destroy(pWorkerThread);
	if (iRet) {
		WMT_ERR_FUNC("osal_thread_stop(0x%p) fail(%d)\n", pWorkerThread, iRet);
		iResult += 32;
	}

	iRet = wmt_conf_deinit();
	if (iRet) {
		WMT_ERR_FUNC("wmt_conf_deinit fail(%d)\n", iRet);
		iResult += 64;
	}

	osal_memset(&gDevWmt, 0, sizeof(gDevWmt));
#if 0
#ifdef MTK_WCN_WMT_STP_EXP_SYMBOL_ABSTRACT
	mtk_wcn_wmt_exp_deinit();
#endif
#endif

#if CFG_WMT_LTE_COEX_HANDLING
	wmt_idc_deinit();
#endif

	if (table->active_version != NULL) {
		for (i = 0; i < table->num; i++) {
			if (table->active_version[i])
				osal_free(table->active_version[i]);
		}
		osal_free(table->active_version);
		table->active_version = NULL;
	}

	WMT_STEP_DEINIT_FUNC();

	return iResult;
}

VOID wmt_lib_flush_rx(VOID)
{
	mtk_wcn_stp_flush_rx_queue(WMT_TASK_INDX);
}

INT32 wmt_lib_trigger_cmd_signal(INT32 result)
{
	P_OSAL_SIGNAL pSignal = &gDevWmt.cmdResp;

	gDevWmt.cmdResult = result;
	osal_raise_signal(pSignal);
	WMT_DBG_FUNC("wakeup cmdResp\n");
	return 0;
}

P_OSAL_EVENT wmt_lib_get_cmd_event(VOID)
{
	return &gDevWmt.cmdReq;
}

INT32 wmt_lib_set_patch_name(PUINT8 cPatchName)
{
	osal_strncpy(gDevWmt.cPatchName, cPatchName, NAME_MAX);
	return 0;
}

INT32 wmt_lib_set_uart_name(PINT8 cUartName)
{
#if WMT_PLAT_ALPS

	WMT_DBG_FUNC("orig uart: %s\n", wmt_uart_port_desc);
#endif
	osal_strncpy(gDevWmt.cUartName, cUartName, NAME_MAX);
#if WMT_PLAT_ALPS
	wmt_uart_port_desc = gDevWmt.cUartName;
	WMT_DBG_FUNC("new uart: %s\n", wmt_uart_port_desc);
#endif
	return 0;
}

INT32 wmt_lib_set_hif(ULONG hifconf)
{
	UINT32 val;
	P_WMT_HIF_CONF pHif = &gDevWmt.rWmtHifConf;

	val = hifconf & 0xF;
	switch (val) {
	case STP_UART_FULL:
		pHif->hifType = WMT_HIF_UART;
		pHif->uartFcCtrl = ((hifconf & 0xc) >> 2);
		val = (hifconf >> 8);
		pHif->au4HifConf[0] = val;
		pHif->au4HifConf[1] = val;
		mtk_wcn_stp_set_if_tx_type(STP_UART_IF_TX);
		wmt_plat_set_comm_if_type(STP_UART_IF_TX);
		break;
	case STP_SDIO:
		pHif->hifType = WMT_HIF_SDIO;
		mtk_wcn_stp_set_if_tx_type(STP_SDIO_IF_TX);
		wmt_plat_set_comm_if_type(STP_SDIO_IF_TX);
		break;
	case STP_BTIF_FULL:
		pHif->hifType = WMT_HIF_BTIF;
		mtk_wcn_stp_set_if_tx_type(STP_BTIF_IF_TX);
		break;
	default:
		WMT_WARN_FUNC("invalid stp mode: %lu %u\n", hifconf, val);
		return -1;
	}

	val = (hifconf & 0xF0) >> 4;
	if (val == WMT_FM_COMM) {
		pHif->au4StrapConf[0] = WMT_FM_COMM;
	} else if (val == WMT_FM_I2C) {
		pHif->au4StrapConf[0] = WMT_FM_I2C;
	} else {
		WMT_WARN_FUNC("invalid fm mode: %u\n", val);
		return -2;
	}

	WMT_DBG_FUNC("new hifType:%d, fcCtrl:%d, baud:%d, fm:%d\n",
		      pHif->hifType, pHif->uartFcCtrl, pHif->au4HifConf[0], pHif->au4StrapConf[0]);
	return 0;
}


P_WMT_HIF_CONF wmt_lib_get_hif(VOID)
{
	return &gDevWmt.rWmtHifConf;
}

PUINT8 wmt_lib_get_cmd(VOID)
{
	if (osal_test_and_clear_bit(WMT_STAT_CMD, &gDevWmt.state))
		return gDevWmt.cCmd;

	return NULL;
}

MTK_WCN_BOOL wmt_lib_get_cmd_status(VOID)
{
	return osal_test_bit(WMT_STAT_CMD, &gDevWmt.state) ? MTK_WCN_BOOL_TRUE : MTK_WCN_BOOL_FALSE;
}

MTK_WCN_BOOL wmt_lib_stp_is_btif_fullset_mode(VOID)
{
	return mtk_wcn_stp_is_btif_fullset_mode();
}

#if CFG_WMT_PS_SUPPORT
INT32 wmt_lib_ps_set_idle_time(UINT32 psIdleTime)
{
	gPsIdleTime = psIdleTime;
	return gPsIdleTime;
}

INT32 wmt_lib_ps_ctrl(UINT32 state)
{
	if (state == 0) {
		wmt_lib_ps_disable();
		gPsEnable = 0;
	} else {
		gPsEnable = 1;
		wmt_lib_ps_enable();
	}
	return 0;
}


INT32 wmt_lib_ps_enable(VOID)
{
	if (gPsEnable)
		mtk_wcn_stp_psm_enable(gPsIdleTime);

	return 0;
}

INT32 wmt_lib_ps_disable(VOID)
{
	if (gPsEnable)
		return mtk_wcn_stp_psm_disable();

	return 0;
}

INT32 wmt_lib_ps_init(VOID)
{
	/* mtk_wcn_stp_psm_register_wmt_cb(wmt_lib_ps_stp_cb); */
	return 0;
}

INT32 wmt_lib_ps_deinit(VOID)
{
	/* mtk_wcn_stp_psm_unregister_wmt_cb(); */
	return 0;
}

static MTK_WCN_BOOL wmt_lib_ps_action(MTKSTP_PSM_ACTION_T action)
{
	P_OSAL_OP lxop;
	MTK_WCN_BOOL bRet;
	UINT32 u4Wait;
	P_OSAL_SIGNAL pSignal;

	lxop = wmt_lib_get_free_op();
	if (!lxop) {
		WMT_DBG_FUNC("get_free_lxop fail\n");
		return MTK_WCN_BOOL_FALSE;
	}

	pSignal = &lxop->signal;
	pSignal->timeoutValue = 0;
	lxop->op.opId = WMT_OPID_PWR_SV;
	lxop->op.au4OpData[0] = action;
	lxop->op.au4OpData[1] = (SIZE_T) mtk_wcn_stp_psm_notify_stp;
	u4Wait = 0;
	bRet = wmt_lib_put_act_op(lxop);
	return bRet;
}

#if CFG_WMT_LTE_COEX_HANDLING
MTK_WCN_BOOL wmt_lib_handle_idc_msg(conn_md_ipc_ilm_t *idc_infor)
{
	P_OSAL_OP lxop;
	MTK_WCN_BOOL bRet = MTK_WCN_BOOL_FALSE;
	P_OSAL_SIGNAL pSignal;
	INT32 ret = 0;
	UINT16 msg_len = 0;

#if	CFG_WMT_LTE_ENABLE_MSGID_MAPPING
	MTK_WCN_BOOL unknown_msgid = MTK_WCN_BOOL_FALSE;
#endif
	WMT_DBG_FUNC("idc_infor from conn_md is 0x%p\n", idc_infor);

	ret = wmt_lib_idc_lock_aquire();
	if (ret) {
		WMT_ERR_FUNC("--->lock idc_lock failed, ret=%d\n", ret);
		return MTK_WCN_BOOL_FALSE;
	}
	msg_len = idc_infor->local_para_ptr->msg_len - osal_sizeof(struct local_para);
	if (msg_len > WMT_IDC_MSG_MAX_SIZE) {
		wmt_lib_idc_lock_release();
		WMT_ERR_FUNC("abnormal idc msg len:%d\n", msg_len);
		return -2;
	}
	osal_memcpy(&gDevWmt.msg_local_buffer[0], &msg_len, osal_sizeof(msg_len));
	osal_memcpy(&gDevWmt.msg_local_buffer[osal_sizeof(msg_len)],
			&(idc_infor->local_para_ptr->data[0]), msg_len - 1);
	wmt_lib_idc_lock_release();

	lxop = wmt_lib_get_free_op();
	if (!lxop) {
		WMT_DBG_FUNC("get_free_lxop fail\n");
		return MTK_WCN_BOOL_FALSE;
	}
	pSignal = &lxop->signal;
	pSignal->timeoutValue = MAX_EACH_WMT_CMD;
	lxop->op.opId = WMT_OPID_IDC_MSG_HANDLING;
	lxop->op.au4OpData[0] = (size_t) gDevWmt.msg_local_buffer;

	/*msg opcode fill rule is still not clrear,need scott comment */
	/***********************************************************/
	WMT_DBG_FUNC("ilm msg id is (0x%08x)\n", idc_infor->msg_id);

#if	CFG_WMT_LTE_ENABLE_MSGID_MAPPING
	switch (idc_infor->msg_id) {
	case IPC_MSG_ID_EL1_LTE_DEFAULT_PARAM_IND:
		lxop->op.au4OpData[1] = WMT_IDC_TX_OPCODE_LTE_PARA;
		break;
	case IPC_MSG_ID_EL1_LTE_OPER_FREQ_PARAM_IND:
		lxop->op.au4OpData[1] = WMT_IDC_TX_OPCODE_LTE_FREQ;
		break;
	case IPC_MSG_ID_EL1_WIFI_MAX_PWR_IND:
		lxop->op.au4OpData[1] = WMT_IDC_TX_OPCODE_WIFI_MAX_POWER;
		break;
	case IPC_MSG_ID_EL1_LTE_TX_IND:
		lxop->op.au4OpData[1] = WMT_IDC_TX_OPCODE_LTE_INDICATION;
		break;
	case IPC_MSG_ID_EL1_LTE_CONNECTION_STATUS_IND:
		lxop->op.au4OpData[1] = WMT_IDC_TX_OPCODE_LTE_CONNECTION_STAS;
		break;
	case IPC_MSG_ID_EL1_LTE_HW_INTERFACE_IND:
		lxop->op.au4OpData[1] = WMT_IDC_TX_OPCODE_LTE_HW_IF_INDICATION;
		break;
	default:
		unknown_msgid = MTK_WCN_BOOL_TRUE;
		break;
	}
	if (unknown_msgid == MTK_WCN_BOOL_FALSE) {
		/*wake up chip first */
		if (DISABLE_PSM_MONITOR()) {
			WMT_ERR_FUNC("wake up failed\n");
			wmt_lib_put_op_to_free_queue(lxop);
			return MTK_WCN_BOOL_FALSE;
		}

		bRet = wmt_lib_put_act_op(lxop);
		ENABLE_PSM_MONITOR();
		if (bRet == MTK_WCN_BOOL_FALSE) {
			WMT_WARN_FUNC("WMT_OPID_IDC_MSG_HANDLING fail(%d)\n", bRet);
		} else {
			WMT_DBG_FUNC("OPID(%d) type(%zu) ok\n",
				     lxop->op.opId, lxop->op.au4OpData[1]);
		}
	} else {
		bRet = MTK_WCN_BOOL_FALSE;
		wmt_lib_put_op_to_free_queue(lxop);
		WMT_ERR_FUNC("unknown msgid from LTE(%d)\n", idc_infor->msg_id);
	}
#else
	if ((idc_infor->msg_id >= IPC_EL1_MSG_ID_BEGIN)
	    && (idc_infor->msg_id <= IPC_EL1_MSG_ID_BEGIN + IPC_EL1_MSG_ID_RANGE)) {
		lxop->op.au4OpData[1] = idc_infor->msg_id - IPC_EL1_MSG_ID_BEGIN + LTE_MSG_ID_OFFSET - 1;

		/*wake up chip first */
		if (DISABLE_PSM_MONITOR()) {
			WMT_ERR_FUNC("wake up failed\n");
			wmt_lib_put_op_to_free_queue(lxop);
			return MTK_WCN_BOOL_FALSE;
		}

		bRet = wmt_lib_put_act_op(lxop);
		ENABLE_PSM_MONITOR();
		if (bRet == MTK_WCN_BOOL_FALSE) {
			WMT_WARN_FUNC("WMT_OPID_IDC_MSG_HANDLING fail(%d)\n", bRet);
		} else {
		    WMT_DBG_FUNC("wmt_lib_handle_idc_msg OPID(%d) type(%zu) ok\n",
				     lxop->op.opId, lxop->op.au4OpData[1]);
		}
	} else {
		wmt_lib_put_op_to_free_queue(lxop);
		WMT_ERR_FUNC("msgid(%d) out of range,wmt drop it!\n", idc_infor->msg_id);
	}

#endif
	return bRet;
}
#endif

static MTK_WCN_BOOL wmt_lib_ps_do_sleep(VOID)
{
	return wmt_lib_ps_action(SLEEP);
}

static MTK_WCN_BOOL wmt_lib_ps_do_wakeup(VOID)
{
	return wmt_lib_ps_action(WAKEUP);
}

static MTK_WCN_BOOL wmt_lib_ps_do_host_awake(VOID)
{
	return wmt_lib_ps_action(HOST_AWAKE);
}

/* extern int g_block_tx; */
static INT32 wmt_lib_ps_handler(MTKSTP_PSM_ACTION_T action)
{
	INT32 ret;
	MTK_WCN_BOOL bRet = MTK_WCN_BOOL_FALSE;
	static DEFINE_RATELIMIT_STATE(_rs, 2 * HZ, 1);

	ret = 0;		/* TODO:[FixMe][George] initial value or compile warning? */
	/* if(g_block_tx && (action == SLEEP)) */
	if ((mtk_wcn_stp_coredump_start_get() != 0) && (action == SLEEP)) {
		ret = mtk_wcn_stp_psm_notify_stp(SLEEP);
		return ret;
	}

	/*MT662x Not Ready */
	if (!mtk_wcn_stp_is_ready()) {
		if (!mtk_wcn_stp_is_sdio_mode()) {
			WMT_DBG_FUNC("MT662x Not Ready, Dont Send Sleep/Wakeup Command\n");
			ret = mtk_wcn_stp_psm_notify_stp(ROLL_BACK);
		} else {
			WMT_DBG_FUNC("MT662x Not Ready, SDIO mode, skip EIRQ");
		}
		return ret;
	}

	if (action == SLEEP) {
		WMT_DBG_FUNC("send op--------------------------------> sleep job\n");

		if (!mtk_wcn_stp_is_sdio_mode()) {
			bRet = wmt_lib_ps_do_sleep();
			ret = bRet ? 0 : -1;
			WMT_DBG_FUNC("enable host eirq\n");
			wmt_plat_eirq_ctrl(PIN_BGF_EINT, PIN_STA_EINT_EN);
#if CFG_WMT_DUMP_INT_STATUS
			if (wmt_plat_dump_BGF_irq_status() == MTK_WCN_BOOL_TRUE)
				wmt_plat_BGF_irq_dump_status();
#endif
		} else {
			/* ret = mtk_wcn_stp_sdio_do_own_set(); */
			if (sdio_own_ctrl) {
				ret = (*sdio_own_ctrl) (OWN_SET);
				mtk_wcn_stp_dbg_pkt_log(8, PKT_DIR_TX);
			} else {
				WMT_ERR_FUNC("sdio_own_ctrl is not registered\n");
				ret = -1;
			}

			if (!ret) {
				mtk_wcn_stp_psm_notify_stp(SLEEP);
			} else if (ret == -2) {
				mtk_wcn_stp_psm_notify_stp(ROLL_BACK);
				WMT_WARN_FUNC
				    ("========[SDIO-PS] rollback due to tx busy ========%%\n");
			} else {
				mtk_wcn_stp_psm_notify_stp(SLEEP);
				WMT_ERR_FUNC
				    ("========[SDIO-PS] set own fails! ========%%\n");
			}
		}

		WMT_DBG_FUNC("send op<--------------------------------- sleep job\n");
	} else if (action == WAKEUP) {
		WMT_DBG_FUNC("send op --------------------------------> wake job\n");

		if (!mtk_wcn_stp_is_sdio_mode()) {
			WMT_DBG_FUNC("disable host eirq\n");
#if CFG_WMT_DUMP_INT_STATUS
			if (wmt_plat_dump_BGF_irq_status() == MTK_WCN_BOOL_TRUE)
				wmt_plat_BGF_irq_dump_status();
#endif
			wmt_plat_eirq_ctrl(PIN_BGF_EINT, PIN_STA_EINT_DIS);
			bRet = wmt_lib_ps_do_wakeup();
			ret = bRet ? 0 : -1;
		} else {
			/* ret = mtk_wcn_stp_sdio_do_own_clr(); */

			if (sdio_own_ctrl) {
				ret = (*sdio_own_ctrl) (OWN_CLR);
			} else {
				WMT_ERR_FUNC("sdio_own_ctrl is not registered\n");
				ret = -1;
			}

			if (!ret) {
				mtk_wcn_stp_psm_notify_stp(WAKEUP);
			} else {
				mtk_wcn_stp_psm_notify_stp(WAKEUP);
				WMT_ERR_FUNC
				    ("========[SDIO-PS] set own back fails! ========%%\n");
			}
		}

		WMT_DBG_FUNC("send op<--------------------------------- wake job\n");
	} else if (action == HOST_AWAKE) {
		WMT_DBG_FUNC("send op --------------------------------> host awake job\n");

		if (!mtk_wcn_stp_is_sdio_mode()) {
			WMT_DBG_FUNC("disable host eirq\n");
			/* IRQ already disabled */
			/* wmt_plat_eirq_ctrl(PIN_BGF_EINT, PIN_STA_EINT_DIS); */
#if 0
			if (wmt_plat_dump_BGF_irq_status() == MTK_WCN_BOOL_TRUE)
				wmt_plat_BGF_irq_dump_status();
#endif
			bRet = wmt_lib_ps_do_host_awake();
			ret = bRet ? 0 : -1;
		} else {
			WMT_DBG_FUNC("[SDIO-PS] SDIO host awake! ####\n");

			/* ret = mtk_wcn_stp_sdio_do_own_clr(); */

			if (sdio_own_ctrl) {
				ret = (*sdio_own_ctrl) (OWN_CLR);
			} else {
				WMT_ERR_FUNC("sdio_own_ctrl is not registered\n");
				ret = -1;
			}

			mtk_wcn_stp_psm_notify_stp(HOST_AWAKE);
		}

		WMT_DBG_FUNC("send op<--------------------------------- host awake job\n");
	} else if (action == EIRQ) {
		WMT_DBG_FUNC("send op --------------------------------> eirq job\n");

		if (!mtk_wcn_stp_is_sdio_mode()) {
			if (__ratelimit(&_rs))
				pr_info("conn2ap_btif0_wakeup_out_b EIRQ handler\n");
			WMT_DBG_FUNC("disable host eirq\n");
			/* Disable interrupt */
			/*wmt_plat_eirq_ctrl(PIN_BGF_EINT, PIN_STA_EINT_DIS);*/
			ret = mtk_wcn_stp_psm_notify_stp(EIRQ);
		} else {
			WMT_DBG_FUNC("[SDIO-PS]sdio own-back eirq!######\n");
			ret = mtk_wcn_stp_psm_notify_stp(EIRQ);
		}

		WMT_DBG_FUNC("send op<--------------------------------- eirq job\n");
	}

	return ret;
}
#endif				/* end of CFG_WMT_PS_SUPPORT */

INT32 wmt_lib_ps_stp_cb(MTKSTP_PSM_ACTION_T action)
{
#if CFG_WMT_PS_SUPPORT
	return wmt_lib_ps_handler(action);
#else
	WMT_WARN_FUNC("CFG_WMT_PS_SUPPORT is not set\n");
	return 0;
#endif
}

VOID wmt_lib_set_bt_link_status(INT32 type, INT32 value)
{
	WMT_INFO_FUNC("t = %d, v = %d, no_acl = %d, no_br = %d\n",
		type, value, g_bt_no_acl_link, g_bt_no_br_acl_link);

	if (type == 0)
		g_bt_no_acl_link = value;
	else if (type == 1)
		g_bt_no_br_acl_link = value;
}

/*
 * Allow BT to reset as long as one of the conditions is true.
 * 1. no ACL link
 * 2. no BR ACL link at 2 AM
 */
static INT32 wmt_lib_is_bt_able_to_reset(VOID)
{
	if (g_bt_no_acl_link)
		return 1;
	else if (g_bt_no_br_acl_link) {
		struct timeval time;
		ULONG local_time;
		struct rtc_time tm;

		osal_do_gettimeofday(&time);
		local_time = (ULONG)(time.tv_sec - (sys_tz.tz_minuteswest * 60));
		rtc_time_to_tm(local_time, &tm);
		if (tm.tm_hour == 2)
			return 1;
	}
	return 0;
}

INT32 wmt_lib_update_fw_patch_chip_rst(VOID)
{
	MTK_WCN_BOOL wifiDrvOwn = MTK_WCN_BOOL_FALSE;

	if (g_fw_patch_update_rst == 0)
		return 0;

	if (chip_reset_only == 1)
		return 0;

	if (time_before_eq64(get_jiffies_64(), fw_patch_rst_time))
		return 0;

	if (wmt_lib_get_drv_status(WMTDRV_TYPE_WIFI) == DRV_STS_FUNC_ON) {
		if (wmt_lib_wlan_lock_trylock() == 0)
			return 0;

		if (mtk_wcn_wlan_is_wifi_drv_own != NULL)
			wifiDrvOwn = ((*mtk_wcn_wlan_is_wifi_drv_own)() == 0) ? MTK_WCN_BOOL_FALSE : MTK_WCN_BOOL_TRUE;

		wmt_lib_wlan_lock_release();
	}

	if (wmt_lib_get_drv_status(WMTDRV_TYPE_BT) == DRV_STS_FUNC_ON &&
		wmt_lib_is_bt_able_to_reset() == 0)
		return 0;

	if (wmt_dev_get_early_suspend_state() == MTK_WCN_BOOL_FALSE
		|| wmt_lib_get_drv_status(WMTDRV_TYPE_FM) == DRV_STS_FUNC_ON
		|| mtk_wcn_stp_is_ready() == MTK_WCN_BOOL_FALSE
		|| wifiDrvOwn == MTK_WCN_BOOL_TRUE)
		return 0;

	if (wmt_lib_psm_lock_trylock() == 0)
		return 0;
	wmt_lib_psm_lock_release();

	wmt_lib_fw_patch_update_rst_ctrl(0);
	chip_reset_only = 1;
	fw_patch_rst_time = get_jiffies_64() + (FW_PATCH_UPDATE_RST_DURATION * HZ);
	WMT_INFO_FUNC("Invoke whole chip reset from fw patch update!!!\n");
	return wmt_lib_trigger_reset();
}

MTK_WCN_BOOL wmt_lib_is_quick_ps_support(VOID)
{
	if ((g_quick_sleep_ctrl) && (wmt_dev_get_early_suspend_state() == MTK_WCN_BOOL_TRUE))
		return wmt_core_is_quick_ps_support();
	else
		return MTK_WCN_BOOL_FALSE;
}

VOID wmt_lib_ps_irq_cb(VOID)
{
#if CFG_WMT_PS_SUPPORT
	wmt_lib_ps_handler(EIRQ);
#else
	WMT_DBG_FUNC("CFG_WMT_PS_SUPPORT is not set\n");
	return;
#endif
}

VOID wmt_lib_ps_set_sdio_psop(PF_WMT_SDIO_PSOP own_cb)
{
#if CFG_WMT_PS_SUPPORT
	sdio_own_ctrl = own_cb;
#endif
}

#ifdef CONFIG_MTK_COMBO_CHIP_DEEP_SLEEP_SUPPORT
VOID wmt_lib_sdio_deep_sleep_flag_set_cb_reg(PF_WMT_SDIO_DEEP_SLEEP flag_cb)
{
	sdio_deep_sleep_flag_set = flag_cb;
}
#endif

VOID wmt_lib_sdio_reg_rw_cb(PF_WMT_SDIO_DEBUG reg_rw_cb)
{
	sdio_reg_rw = reg_rw_cb;
}

UINT32 wmt_lib_wait_event_checker(P_OSAL_THREAD pThread)
{
	P_DEV_WMT pDevWmt;

	if (pThread) {
		pDevWmt = (P_DEV_WMT) (pThread->pThreadData);
		return !RB_EMPTY(&pDevWmt->rActiveOpQ);
	}
	WMT_ERR_FUNC("pThread(NULL)\n");
	return 0;
}

UINT32 wmt_lib_worker_wait_event_checker(P_OSAL_THREAD pThread)
{
	P_DEV_WMT pDevWmt;

	if (pThread) {
		pDevWmt = (P_DEV_WMT) (pThread->pThreadData);
		return !RB_EMPTY(&pDevWmt->rWorkerOpQ);
	}
	WMT_ERR_FUNC("pThread(NULL)\n");
	return 0;
}

static INT32 wmtd_thread(void *pvData)
{
	P_DEV_WMT pWmtDev = (P_DEV_WMT) pvData;
	P_OSAL_EVENT pEvent = NULL;
	P_OSAL_OP pOp;
	INT32 iResult;

	if (pWmtDev == NULL) {
		WMT_ERR_FUNC("pWmtDev(NULL)\n");
		return -1;
	}
	WMT_INFO_FUNC("wmtd thread starts\n");

	pEvent = &(pWmtDev->rWmtdWq);

	for (;;) {
		pOp = NULL;
		pEvent->timeoutValue = 0;
/*        osal_thread_wait_for_event(&pWmtDev->thread, pEvent);*/
		osal_thread_wait_for_event(&pWmtDev->thread, pEvent, wmt_lib_wait_event_checker);

		if (osal_thread_should_stop(&pWmtDev->thread)) {
			WMT_INFO_FUNC("wmtd thread should stop now...\n");
			/* TODO: clean up active opQ */
			break;
		}

		/* get Op from activeQ */
		pOp = wmt_lib_get_op(&pWmtDev->rActiveOpQ);
		if (!pOp) {
			WMT_WARN_FUNC("get_lxop activeQ fail\n");
			continue;
		}

		osal_op_history_save(&pWmtDev->wmtd_op_history, pOp);

#if 0				/* wmt_core_opid_handler will do sanity check on opId, so no usage here */
		id = lxop_get_opid(pLxOp);
		if (id >= WMT_OPID_MAX) {
			WMT_WARN_FUNC("abnormal opid id: 0x%x\n", id);
			iResult = -1;
			goto handlerDone;
		}
#endif

		if (osal_test_bit(WMT_STAT_RST_ON, &pWmtDev->state)) {
			/* when whole chip reset, only HW RST and SW RST cmd can execute */
			if ((pOp->op.opId == WMT_OPID_HW_RST)
			    || (pOp->op.opId == WMT_OPID_SW_RST)
			    || (pOp->op.opId == WMT_OPID_GPIO_STATE)
			    || (pOp->op.opId == WMT_OPID_GET_CONSYS_STATE)) {
				iResult = wmt_core_opid(&pOp->op);
			} else {
				iResult = -2;
				WMT_WARN_FUNC
				    ("Whole chip resetting, opid (0x%x) failed, iRet(%d)\n",
				     pOp->op.opId, iResult);
			}
		} else {
			wmt_lib_set_current_op(pWmtDev, pOp);
			iResult = wmt_core_opid(&pOp->op);
			wmt_lib_set_current_op(pWmtDev, NULL);
		}

		if (iResult)
			WMT_WARN_FUNC("opid (0x%x) failed, iRet(%d)\n", pOp->op.opId, iResult);

		if (iResult == 0 &&
			(pOp->op.opId == WMT_OPID_WLAN_PROBE || pOp->op.opId == WMT_OPID_WLAN_REMOVE))
			continue;

		if (atomic_dec_and_test(&pOp->ref_count)) {
			wmt_lib_put_op(&pWmtDev->rFreeOpQ, pOp);
		} else if (osal_op_is_wait_for_signal(pOp)) {
			osal_op_raise_signal(pOp, iResult);
		}

		if (pOp->op.opId == WMT_OPID_EXIT) {
			WMT_INFO_FUNC("wmtd thread received exit signal\n");
			break;
		}
	}

	WMT_INFO_FUNC("wmtd thread exits succeed\n");

	return 0;
};

static INT32 met_thread(void *pvData)
{
	P_DEV_WMT p_wmtdev = (P_DEV_WMT) pvData;
	INT32 log_ctrl;
	UINT32 read_ptr = 0;
	UINT32 write_ptr = 0;
	UINT32 emi_met_size = 0;
	UINT32 emi_met_offset = 0;
	P_CONSYS_EMI_ADDR_INFO emi_info;
	PUINT8 emi_met_base = NULL;
	PINT32 met_dump_buf = 0;
	UINT32 met_buf_offset = 0;
	UINT32 value = 0;

	if (p_wmtdev == NULL) {
		WMT_ERR_FUNC("pWmtDev(NULL)\n");
		return -1;
	}

	WMT_INFO_FUNC("met thread starts\n");

	emi_info = mtk_wcn_consys_soc_get_emi_phy_add();
	if (!emi_info) {
		WMT_ERR_FUNC("get EMI info failed.\n");
		return -1;
	}

	emi_met_size = emi_info->emi_met_size;
	if (!emi_met_size) {
		WMT_ERR_FUNC("get met emi size fail\n");
		return -1;
	}

	emi_met_offset = emi_info->emi_met_data_offset;
	if (!emi_met_offset) {
		WMT_ERR_FUNC("get met emi offset fail\n");
		return -1;
	}

	met_dump_buf = osal_malloc(MET_DUMP_SIZE);
	if (!met_dump_buf) {
		WMT_ERR_FUNC("alloc dump buffer fail\n");
		return -1;
	}
	osal_memset(met_dump_buf, 0, MET_DUMP_SIZE);

	emi_met_base = ioremap_nocache(emi_info->emi_ap_phy_addr + emi_met_offset, emi_met_size);
	if (!emi_met_base) {
		osal_free(met_dump_buf);
		WMT_ERR_FUNC("met emi ioremap fail\n");
		return -1;
	}

	WMT_INFO_FUNC("emi phy base:%x, emi vir base:%p, met offset:%x, size:%x\n",
			emi_info->emi_ap_phy_addr,
			emi_met_base,
			emi_met_offset,
			emi_met_size);


	log_ctrl = p_wmtdev->met_log_ctrl;
	if (log_ctrl)
		osal_ftrace_print_ctrl(1);

	for (;;) {
		if (osal_thread_should_stop(&p_wmtdev->met_thread)) {
			WMT_INFO_FUNC("met thread should stop now...\n");
			goto met_exit;
		}

		read_ptr = CONSYS_REG_READ(emi_met_base + EMI_MET_READ_OFFSET);
		write_ptr = CONSYS_REG_READ(emi_met_base + EMI_MET_WRITE_OFFSET);

		if (read_ptr == write_ptr)
			WMT_DBG_FUNC("read_ptr(0x%x) == write_ptr(0x%x) no met data need dump!!!\n",
					read_ptr, write_ptr);
		else if (write_ptr > (emi_met_size - EMI_MET_DATA_OFFSET)) {
			WMT_ERR_FUNC("write_ptr(0x%x) overflow!!!\n", write_ptr);
			wmt_lib_trigger_assert(WMTDRV_TYPE_WMT, 42);
			goto met_exit;
		} else {
			if (read_ptr > write_ptr) {
				for (; read_ptr < emi_met_size; read_ptr += 0x4) {
					value = CONSYS_REG_READ(emi_met_base + EMI_MET_DATA_OFFSET + read_ptr);
					met_dump_buf[met_buf_offset] = value;
					met_buf_offset++;
					if (met_buf_offset >= MET_DUMP_MAX_NUM) {
						met_buf_offset = 0;
						osal_buffer_dump_data(met_dump_buf, "MCU_MET_DATA:",
								      MET_DUMP_MAX_NUM, MET_DUMP_MAX_NUM,
								      log_ctrl);
					}
				}
				read_ptr = 0;
			}

			for (; read_ptr < write_ptr; read_ptr += 0x4) {
				value = CONSYS_REG_READ(emi_met_base + EMI_MET_DATA_OFFSET + read_ptr);
				met_dump_buf[met_buf_offset] = value;
				met_buf_offset++;
				if (met_buf_offset >= MET_DUMP_MAX_NUM) {
					met_buf_offset = 0;
					osal_buffer_dump_data(met_dump_buf, "MCU_MET_DATA:", MET_DUMP_MAX_NUM,
							      MET_DUMP_MAX_NUM,
							      log_ctrl);
				}
			}
			CONSYS_REG_WRITE(emi_met_base, read_ptr);
		}
		osal_usleep_range(CONSYS_MET_WAIT, CONSYS_MET_WAIT);
	}

met_exit:
	osal_free(met_dump_buf);
	iounmap(emi_met_base);
	WMT_INFO_FUNC("met thread exits succeed\n");

	return 0;
};

static VOID wmt_lib_wmtd_worker_thread_timeout_handler(timer_handler_arg arg)
{
	schedule_work(&gDevWmt.wmtd_worker_thread_work);
}

static VOID wmt_lib_wmtd_worker_thread_work_handler(struct work_struct *work)
{
	PUINT8 pbuf = NULL;
	INT32 len = 0;
	P_OSAL_OP pOp;

	pOp = wmt_lib_get_worker_op(&gDevWmt);
	if (pOp) {
		switch (pOp->op.opId) {
		case WMT_OPID_WLAN_PROBE:
			pbuf = "DrvWMT turn on wifi fail, just collect SYS_FTRACE to DB";
			len = osal_strlen(pbuf);
		break;
		case WMT_OPID_WLAN_REMOVE:
			pbuf = "DrvWMT turn off wifi fail, just collect SYS_FTRACE to DB";
			len = osal_strlen(pbuf);
		break;
		default:
			pbuf = "DrvWMT unknown op fail, just collect SYS_FTRACE to DB";
			len = osal_strlen(pbuf);
		break;
		}
		wmt_lib_trigger_assert_keyword(WMTDRV_TYPE_WIFI, 0, pbuf);
	}
}

static INT32 wmtd_worker_thread(void *pvData)
{
	P_DEV_WMT pWmtDev = (P_DEV_WMT) pvData;
	P_OSAL_EVENT pEvent = NULL;
	P_OSAL_OP pOp;
	INT32 iResult = 0;

	pEvent = &(pWmtDev->rWmtdWorkerWq);

	for (;;) {
		osal_thread_wait_for_event(&pWmtDev->worker_thread, pEvent, wmt_lib_worker_wait_event_checker);

		if (osal_thread_should_stop(&pWmtDev->worker_thread)) {
			WMT_INFO_FUNC("wmtd worker thread should stop now...\n");
			/* TODO: clean up active opQ */
			break;
		}

		/* get Op from activeWorkerQ */
		pOp = wmt_lib_get_op(&pWmtDev->rWorkerOpQ);
		if (!pOp) {
			WMT_WARN_FUNC("get activeWorkerQ fail\n");
			continue;
		}
		osal_op_history_save(&pWmtDev->worker_op_history, pOp);

		if (osal_test_bit(WMT_STAT_RST_ON, &pWmtDev->state)) {
			iResult = -2;
			WMT_WARN_FUNC("Whole chip resetting, opid (0x%x) failed, iRet(%d)\n", pOp->op.opId, iResult);
		} else {
			WMT_WARN_FUNC("opid: 0x%x", pOp->op.opId);
			wmt_lib_set_worker_op(pWmtDev, pOp);
			osal_timer_start(&gDevWmt.worker_timer, MAX_FUNC_ON_TIME);
			iResult = wmt_core_opid(&pOp->op);
			osal_timer_stop(&gDevWmt.worker_timer);
			wmt_lib_set_worker_op(pWmtDev, NULL);
		}

		if (iResult)
			WMT_WARN_FUNC("opid (0x%x) failed, iRet(%d)\n", pOp->op.opId, iResult);

		if (atomic_dec_and_test(&pOp->ref_count))
			wmt_lib_put_op(&pWmtDev->rFreeOpQ, pOp);
		else if (osal_op_is_wait_for_signal(pOp))
			osal_op_raise_signal(pOp, iResult);
	}

	return 0;
}

static MTK_WCN_BOOL wmt_lib_put_op(P_OSAL_OP_Q pOpQ, P_OSAL_OP pOp)
{
	INT32 iRet;

	if (!pOpQ || !pOp) {
		WMT_WARN_FUNC("invalid input param: pOpQ(0x%p), pLxOp(0x%p)\n", pOpQ, pOp);
		osal_assert(pOpQ);
		osal_assert(pOp);
		return MTK_WCN_BOOL_FALSE;
	}

	iRet = osal_lock_sleepable_lock(&pOpQ->sLock);
	if (iRet) {
		WMT_WARN_FUNC("osal_lock_sleepable_lock iRet(%d)\n", iRet);
		return MTK_WCN_BOOL_FALSE;
	}

#if defined(CONFIG_MTK_ENG_BUILD) || defined(CONFIG_MT_ENG_BUILD)
	if (osal_opq_has_op(pOpQ, pOp)) {
		WMT_ERR_FUNC("Op(%p) already exists in queue(%p)\n", pOp, pOpQ);
		iRet = -2;
	}
#endif

	/* acquire lock success */
	if (!RB_FULL(pOpQ))
		RB_PUT(pOpQ, pOp);
	else {
		WMT_WARN_FUNC("RB_FULL(%p -> %p)\n", pOp, pOpQ);
		iRet = -1;
	}

	osal_unlock_sleepable_lock(&pOpQ->sLock);

	if (iRet) {
		osal_opq_dump("FreeOpQ", &gDevWmt.rFreeOpQ);
		osal_opq_dump("ActiveOpQ", &gDevWmt.rActiveOpQ);
		return MTK_WCN_BOOL_FALSE;
	}
	return MTK_WCN_BOOL_TRUE;
}



static P_OSAL_OP wmt_lib_get_op(P_OSAL_OP_Q pOpQ)
{
	P_OSAL_OP pOp;
	INT32 iRet;

	if (pOpQ == NULL) {
		WMT_ERR_FUNC("pOpQ = NULL\n");
		osal_assert(pOpQ);
		return NULL;
	}

	iRet = osal_lock_sleepable_lock(&pOpQ->sLock);
	if (iRet) {
		WMT_ERR_FUNC("osal_lock_sleepable_lock iRet(%d)\n", iRet);
		return NULL;
	}

	/* acquire lock success */
	RB_GET(pOpQ, pOp);
	osal_unlock_sleepable_lock(&pOpQ->sLock);

	if (pOp == NULL) {
		P_OSAL_OP pCurOp = wmt_lib_get_current_op(&gDevWmt);

		WMT_WARN_FUNC("RB_GET(%p) return NULL\n", pOpQ);
		if (pCurOp != NULL)
			WMT_WARN_FUNC("Current opId (%d)\n", pCurOp->op.opId);

		wmt_lib_print_wmtd_op_history();
		wmt_lib_print_worker_op_history();
		osal_opq_dump("FreeOpQ", &gDevWmt.rFreeOpQ);
		osal_opq_dump("ActiveOpQ", &gDevWmt.rActiveOpQ);
		osal_assert(pOp);
	}

	return pOp;
}


INT32 wmt_lib_put_op_to_free_queue(P_OSAL_OP pOp)
{
	P_DEV_WMT pWmtDev = &gDevWmt;

	if (wmt_lib_put_op(&pWmtDev->rFreeOpQ, pOp) == MTK_WCN_BOOL_FALSE)
		return -1;

	return 0;
}


P_OSAL_OP wmt_lib_get_free_op(VOID)
{
	P_OSAL_OP pOp = NULL;
	P_DEV_WMT pDevWmt = &gDevWmt;

	osal_assert(pDevWmt);
	pOp = wmt_lib_get_op(&pDevWmt->rFreeOpQ);
	if (pOp) {
		osal_memset(pOp, 0, osal_sizeof(OSAL_OP));
	}
	return pOp;
}

MTK_WCN_BOOL wmt_lib_put_act_op(P_OSAL_OP pOp)
{
	P_DEV_WMT pWmtDev = &gDevWmt;
	MTK_WCN_BOOL bRet = MTK_WCN_BOOL_FALSE;
	P_OSAL_SIGNAL pSignal = NULL;
	INT32 waitRet = -1;

	osal_assert(pWmtDev);
	osal_assert(pOp);

	do {
		if (!pWmtDev || !pOp) {
			WMT_ERR_FUNC("pWmtDev(0x%p), pOp(0x%p)\n", pWmtDev, pOp);
			break;
		}

		/* Init ref_count to 1 indicating that current thread holds a ref to it */
		atomic_set(&pOp->ref_count, 1);

		if ((mtk_wcn_stp_coredump_start_get() != 0) &&
		    (pOp->op.opId != WMT_OPID_HW_RST) &&
		    (pOp->op.opId != WMT_OPID_SW_RST) && (pOp->op.opId != WMT_OPID_GPIO_STATE)) {
			WMT_WARN_FUNC("block tx flag is set\n");
			break;
		}
		pSignal = &pOp->signal;
/* pOp->u4WaitMs = u4WaitMs; */
		if (pSignal->timeoutValue) {
			pOp->result = -9;
			osal_signal_init(pSignal);
		}

		/* Increment ref_count by 1 as wmtd thread will hold a reference also,
		 * this must be done here instead of on target thread, because
		 * target thread might not be scheduled until a much later time,
		 * allowing current thread to decrement ref_count at the end of function,
		 * putting op back to free queue before target thread has a chance to process.
		 */
		atomic_inc(&pOp->ref_count);

		/* put to active Q */
		bRet = wmt_lib_put_op(&pWmtDev->rActiveOpQ, pOp);
		if (bRet == MTK_WCN_BOOL_FALSE) {
			WMT_WARN_FUNC("put to active queue fail\n");
			atomic_dec(&pOp->ref_count);
			break;
		}

		/* wake up wmtd */
		/* wake_up_interruptible(&pWmtDev->rWmtdWq); */
		osal_trigger_event(&pWmtDev->rWmtdWq);

		if (pSignal->timeoutValue == 0) {
			bRet = MTK_WCN_BOOL_TRUE;
			/* clean it in wmtd */
			break;
		}

		/* check result */
		/* wait_ret = wait_for_completion_interruptible_timeout(&pOp->comp, msecs_to_jiffies(u4WaitMs)); */
		/* wait_ret = wait_for_completion_timeout(&pOp->comp, msecs_to_jiffies(u4WaitMs)); */
		if (pOp->op.opId == WMT_OPID_FUNC_ON &&
			pOp->op.au4OpData[0] == WMTDRV_TYPE_WIFI)
			waitRet = osal_wait_for_signal_timeout(pSignal, &pWmtDev->worker_thread);
		else
			waitRet = osal_wait_for_signal_timeout(pSignal, &pWmtDev->thread);
		WMT_DBG_FUNC("osal_wait_for_signal_timeout:%d\n", waitRet);

		/* if (unlikely(!wait_ret)) { */
		if (waitRet == 0)
			WMT_ERR_FUNC("opId(%d) completion timeout\n", pOp->op.opId);
		else if (pOp->result)
			WMT_WARN_FUNC("opId(%d) result:%d\n", pOp->op.opId, pOp->result);

		/* op completes, check result */
		bRet = (pOp->result) ? MTK_WCN_BOOL_FALSE : MTK_WCN_BOOL_TRUE;
	} while (0);

	if (pOp && atomic_dec_and_test(&pOp->ref_count)) {
		/* put Op back to freeQ */
		wmt_lib_put_op(&pWmtDev->rFreeOpQ, pOp);
	}

	return bRet;
}

MTK_WCN_BOOL wmt_lib_put_worker_op(P_OSAL_OP pOp)
{
	P_DEV_WMT pWmtDev = &gDevWmt;
	MTK_WCN_BOOL bRet = MTK_WCN_BOOL_FALSE;

	osal_assert(pWmtDev);
	osal_assert(pOp);

	do {
		if (!pWmtDev || !pOp) {
			WMT_ERR_FUNC("pWmtDev(0x%p), pOp(0x%p)\n", pWmtDev, pOp);
			break;
		}

		/* put to activeWorker Q */
		bRet = wmt_lib_put_op(&pWmtDev->rWorkerOpQ, pOp);
		if (bRet == MTK_WCN_BOOL_FALSE) {
			WMT_WARN_FUNC("put to ActiveWorker queue fail\n");
			break;
		}

		/* wake up wmtd_worker */
		osal_trigger_event(&pWmtDev->rWmtdWorkerWq);
	} while (0);

	return bRet;
}

/* TODO:[ChangeFeature][George] is this function obsoleted? */
#if 0
INT32 wmt_lib_reg_rw(UINT32 isWrite, UINT32 offset, PUINT32 pvalue, UINT32 mask)
{
	P_WMT_LXOP lxop;
	MTK_WCN_BOOL bRet;
	PUINT32 plv = NULL;
	UINT32 pbuf[2];
	P_OSAL_EVENT pSignal = NULL;

	if (!pvalue) {
		WMT_WARN_FUNC("!pvalue\n");
		return -1;
	}
	lxop = wmt_lib_get_free_lxop();
	if (!lxop) {
		WMT_DBG_FUNC("get_free_lxop fail\n");

		return -1;
	}

	plv = (PUINT32) (((UINT32) pbuf + 0x3) & ~0x3UL);
	*plv = *pvalue;
	pSignal = &lxop->signal;
	WMT_DBG_FUNC("OPID_REG_RW isWrite(%d) offset(0x%x) value(0x%x) mask(0x%x)\n",
		     isWrite, offset, *pvalue, mask);

	lxop->op.opId = WMT_OPID_REG_RW;
	lxop->op.au4OpData[0] = isWrite;
	lxop->op.au4OpData[1] = offset;
	lxop->op.au4OpData[2] = (UINT32) plv;
	lxop->op.au4OpData[3] = mask;
	pSignal->timeoutValue = MAX_EACH_WMT_CMD;

	DISABLE_PSM_MONITOR();
	bRet = wmt_lib_put_act_lxop(lxop);
	ENABLE_PSM_MONITOR();

	if (bRet != MTK_WCN_BOOL_FALSE) {
		WMT_DBG_FUNC("OPID_REG_RW isWrite(%u) offset(0x%x) value(0x%x) mask(0x%x) ok\n",
			     isWrite, offset, *plv, mask);
		if (!isWrite)
			*pvalue = *plv;
	} else {
		WMT_WARN_FUNC
		    ("OPID_REG_RW isWrite(%u) offset(0x%x) value(0x%x) mask(0x%x) bRet(%d)\n",
		     isWrite, offset, *plv, mask, bRet);
	}

	return bRet;
}
#endif

/* TODO:[ChangeFeature][George] is this function obsoleted? */
#if 0
static VOID wmt_lib_clear_chip_id(VOID)
{
/*
 *   gDevWmt.pChipInfo = NULL;
*/
	gDevWmt.hw_ver = WMTHWVER_INVALID;
}
#endif

UINT32 wmt_lib_get_icinfo(ENUM_WMT_CHIPINFO_TYPE_T index)
{
	UINT32 chip_id = 0;

	if (index == WMTCHIN_CHIPID) {
		if (wmt_detect_get_chip_type() == WMT_CHIP_TYPE_COMBO)
			chip_id = gDevWmt.chip_id;
		else
			chip_id = mtk_wcn_consys_soc_chipid();
		WMT_INFO_FUNC("chip_id=[%x]", chip_id);
		return chip_id;
	} else if (index == WMTCHIN_HWVER)
		return gDevWmt.hw_ver;
	else if (index == WMTCHIN_FWVER)
		return gDevWmt.fw_ver;
	else if (index == WMTCHIN_IPVER)
		return gDevWmt.ip_ver;
	else if (index == WMTCHIN_ADIE)
		return mtk_wcn_consys_get_adie_chipid();

	return 0;

}


PUINT8 wmt_lib_def_patch_name(VOID)
{
	WMT_INFO_FUNC("wmt-lib: use default patch name (%s)\n", gDevWmt.cPatchName);
	return gDevWmt.cPatchName;
}


MTK_WCN_BOOL wmt_lib_is_therm_ctrl_support(ENUM_WMTTHERM_TYPE_T eType)
{
	MTK_WCN_BOOL bIsSupportTherm = MTK_WCN_BOOL_TRUE;
	/* TODO:[FixMe][GeorgeKuo]: move IC-dependent checking to ic-implementation file */
	if ((gDevWmt.chip_id == 0x6620) && (gDevWmt.hw_ver == 0x8A00 /*E1*/ || gDevWmt.hw_ver == 0x8A01 /*E2*/)) {
		WMT_ERR_FUNC("thermal command fail: chip version(HWVER:0x%04x) is not valid\n",
			     gDevWmt.hw_ver);
		bIsSupportTherm = MTK_WCN_BOOL_FALSE;
	}
	if ((!osal_test_bit(WMT_STAT_STP_EN, &gDevWmt.state))
	    || (!osal_test_bit(WMT_STAT_STP_RDY, &gDevWmt.state))) {
		if (eType == WMTTHERM_READ)
			WMT_INFO_FUNC
				("thermal command can`t send: STP is not enable(%d) or ready(%d)\n",
				osal_test_bit(WMT_STAT_STP_EN, &gDevWmt.state),
				osal_test_bit(WMT_STAT_STP_RDY, &gDevWmt.state));
		bIsSupportTherm = MTK_WCN_BOOL_FALSE;
	}

	return bIsSupportTherm;
}

MTK_WCN_BOOL wmt_lib_is_dsns_ctrl_support(VOID)
{
	/* TODO:[FixMe][GeorgeKuo]: move IC-dependent checking to ic-implementation file */
	if ((gDevWmt.chip_id == 0x6620) && (gDevWmt.hw_ver == 0x8A00 /*E1*/ || gDevWmt.hw_ver == 0x8A01 /*E2*/)) {
		WMT_ERR_FUNC("thermal command fail: chip version(HWVER:0x%04x) is not valid\n",
			     gDevWmt.hw_ver);
		return MTK_WCN_BOOL_FALSE;
	}

	return MTK_WCN_BOOL_TRUE;
}


/*!
 * \brief Update combo chip pin settings (GPIO)
 *
 * An internal library function to support various settings for chip GPIO. It is
 * updated in a grouping way: configure all required pins in a single call.
 *
 * \param id desired pin ID to be controlled
 * \param stat desired pin states to be set
 * \param flag supplementary options for this operation
 *
 * \retval 0 operation success
 * \retval -1 invalid id
 * \retval -2 invalid stat
 * \retval < 0 error for operation fail
 */
static INT32 wmt_lib_pin_ctrl(WMT_IC_PIN_ID id, WMT_IC_PIN_STATE stat, UINT32 flag)
{
	P_OSAL_OP pOp;
	MTK_WCN_BOOL bRet;
	P_OSAL_SIGNAL pSignal;

	/* input sanity check */
	if (id >= WMT_IC_PIN_MAX) {
		WMT_ERR_FUNC("invalid ic pin id(%d)\n", id);
		return -1;
	}
	if (stat >= WMT_IC_PIN_STATE_MAX) {
		WMT_ERR_FUNC("invalid ic pin state (%d)\n", stat);
		return -2;
	}

	pOp = wmt_lib_get_free_op();
	if (!pOp) {
		WMT_DBG_FUNC("get_free_lxop fail\n");
		return MTK_WCN_BOOL_FALSE;
	}

	WMT_DBG_FUNC("call WMT_OPID_GPIO_CTRL (ic pin id:%d, stat:%d, flag:0x%x)\n", id, stat,
		     flag);

	pSignal = &pOp->signal;
	pOp->op.opId = WMT_OPID_GPIO_CTRL;
	pOp->op.au4OpData[0] = id;
	pOp->op.au4OpData[1] = stat;
	pOp->op.au4OpData[2] = flag;
	pSignal->timeoutValue = MAX_EACH_WMT_CMD;

	/*wake up chip first */
	if (DISABLE_PSM_MONITOR()) {
		WMT_ERR_FUNC("wake up failed\n");
		wmt_lib_put_op_to_free_queue(pOp);
		return -1;
	}

	bRet = wmt_lib_put_act_op(pOp);
	ENABLE_PSM_MONITOR();
	if (bRet == MTK_WCN_BOOL_FALSE)
		WMT_WARN_FUNC("PIN_ID(%d) PIN_STATE(%d) flag(%d) fail\n", id, stat, flag);
	else
		WMT_DBG_FUNC("OPID(%d) type(%zu) ok\n", pOp->op.opId, pOp->op.au4OpData[0]);

	return 0;
}

INT32 wmt_lib_reg_rw(UINT32 isWrite, UINT32 offset, PUINT32 pvalue, UINT32 mask)
{
	P_OSAL_OP pOp;
	MTK_WCN_BOOL bRet;
	UINT32 value;
	P_OSAL_SIGNAL pSignal;

	if (!pvalue) {
		WMT_WARN_FUNC("!pvalue\n");
		return -1;
	}

	pOp = wmt_lib_get_free_op();
	if (!pOp) {
		WMT_DBG_FUNC("get_free_lxop fail\n");
		return -1;
	}

	pSignal = &pOp->signal;
	pSignal->timeoutValue = MAX_WMT_OP_TIMEOUT;
	value = *pvalue;
	WMT_DBG_FUNC("OPID_REG_RW isWrite(%u) offset(0x%x) value(0x%x) mask(0x%x)\n\n",
		     isWrite, offset, *pvalue, mask);
	pOp->op.opId = WMT_OPID_REG_RW;
	pOp->op.au4OpData[0] = isWrite;
	pOp->op.au4OpData[1] = offset;
	pOp->op.au4OpData[2] = (size_t) &value;
	pOp->op.au4OpData[3] = mask;
	if (DISABLE_PSM_MONITOR()) {
		WMT_ERR_FUNC("wake up failed\n");
		wmt_lib_put_op_to_free_queue(pOp);
		return -1;
	}

	bRet = wmt_lib_put_act_op(pOp);
	ENABLE_PSM_MONITOR();

	if (bRet != MTK_WCN_BOOL_FALSE) {
		WMT_DBG_FUNC("OPID_REG_RW isWrite(%u) offset(0x%x) value(0x%x) mask(0x%x) ok\n",
			     isWrite, offset, value, mask);
		if (!isWrite)
			*pvalue = value;

		return 0;
	}
	WMT_WARN_FUNC("OPID_REG_RW isWrite(%u) offset(0x%x) value(0x%x) mask(0x%x) bRet(%d)\n",
			isWrite, offset, value, mask, bRet);

	return -1;
}

INT32 wmt_lib_efuse_rw(UINT32 isWrite, UINT32 offset, PUINT32 pvalue, UINT32 mask)
{
	P_OSAL_OP pOp;
	MTK_WCN_BOOL bRet;
	UINT32 value;
	P_OSAL_SIGNAL pSignal;

	if (!pvalue) {
		WMT_WARN_FUNC("!pvalue\n");
		return -1;
	}

	pOp = wmt_lib_get_free_op();
	if (!pOp) {
		WMT_DBG_FUNC("get_free_lxop fail\n");
		return -1;
	}

	pSignal = &pOp->signal;
	pSignal->timeoutValue = MAX_WMT_OP_TIMEOUT;
	value = *pvalue;
	WMT_DBG_FUNC("OPID_EFUSE_RW isWrite(%u) offset(0x%x) value(0x%x) mask(0x%x)\n\n",
		     isWrite, offset, *pvalue, mask);
	pOp->op.opId = WMT_OPID_EFUSE_RW;
	pOp->op.au4OpData[0] = isWrite;
	pOp->op.au4OpData[1] = offset;
	pOp->op.au4OpData[2] = (size_t) &value;
	pOp->op.au4OpData[3] = mask;
	if (DISABLE_PSM_MONITOR()) {
		WMT_ERR_FUNC("wake up failed\n");
		wmt_lib_put_op_to_free_queue(pOp);
		return -1;
	}

	bRet = wmt_lib_put_act_op(pOp);
	ENABLE_PSM_MONITOR();

	if (bRet != MTK_WCN_BOOL_FALSE) {
		WMT_DBG_FUNC("OPID_EFUSE_RW isWrite(%u) offset(0x%x) value(0x%x) mask(0x%x) ok\n",
			     isWrite, offset, value, mask);
		if (!isWrite)
			*pvalue = value;
		return 0;
	}
	WMT_WARN_FUNC("OPID_REG_RW isWrite(%u) offset(0x%x) value(0x%x) mask(0x%x) bRet(%d)\n",
			isWrite, offset, value, mask, bRet);
	return -1;
}

INT32 wmt_lib_utc_time_sync(VOID)
{
	P_OSAL_OP pOp;
	MTK_WCN_BOOL bRet;
	P_OSAL_SIGNAL pSignal;

	pOp = wmt_lib_get_free_op();
	if (!pOp) {
		WMT_WARN_FUNC("get_free_lxop fail\n");
		return -1;
	}

	pSignal = &pOp->signal;
	pSignal->timeoutValue = MAX_EACH_WMT_CMD;
	pOp->op.opId = WMT_OPID_UTC_TIME_SYNC;
	if (DISABLE_PSM_MONITOR()) {
		WMT_ERR_FUNC("wake up failed\n");
		wmt_lib_put_op_to_free_queue(pOp);
		return -2;
	}

	bRet = wmt_lib_put_act_op(pOp);
	ENABLE_PSM_MONITOR();
	if (bRet == MTK_WCN_BOOL_FALSE) {
		WMT_WARN_FUNC("WMT_OPID_UTC_TIME_SYNC fail(%d)\n", bRet);
		return -3;
	}
	WMT_DBG_FUNC("wmt_lib_utc_time_sync OPID(%d) ok\n", pOp->op.opId);

	return 0;
}

INT32 wmt_lib_try_pwr_off(VOID)
{
	P_OSAL_OP pOp;
	MTK_WCN_BOOL bRet;
	P_OSAL_SIGNAL pSignal;

	pOp = wmt_lib_get_free_op();
	if (!pOp) {
		WMT_WARN_FUNC("get_free_lxop fail\n");
		return -1;
	}

	pSignal = &pOp->signal;
	pSignal->timeoutValue = MAX_FUNC_OFF_TIME;
	pOp->op.opId = WMT_OPID_TRY_PWR_OFF;
	if (DISABLE_PSM_MONITOR()) {
		WMT_ERR_FUNC("wake up failed\n");
		wmt_lib_put_op_to_free_queue(pOp);
		return -2;
	}

	bRet = wmt_lib_put_act_op(pOp);
	ENABLE_PSM_MONITOR();
	if (bRet == MTK_WCN_BOOL_FALSE) {
		WMT_WARN_FUNC("WMT_OPID_TRY_PWR_OFF fail(%d)\n", bRet);
		return -2;
	}
	WMT_DBG_FUNC("wmt_lib_try_pwr_off OPID(%d) ok\n", pOp->op.opId);

	return 0;
}

P_WMT_PATCH_INFO wmt_lib_get_patch_info(VOID)
{
	return gDevWmt.pWmtPatchInfo;
}

/*!
 * \brief update combo chip AUDIO Interface (AIF) settings
 *
 * A library function to support updating chip AUDIO pin settings. A group of
 * pins is updated as a whole.
 *
 * \param aif desired audio interface state to use
 * \param flag whether audio pin is shared or not
 *
 * \retval 0 operation success
 * \retval -1 invalid aif
 * \retval < 0 error for invalid parameters or operation fail
 */
INT32 wmt_lib_set_aif(enum CMB_STUB_AIF_X aif, MTK_WCN_BOOL share)
{
	if (aif < 0 || aif >= CMB_STUB_AIF_MAX) {
		WMT_ERR_FUNC("invalid aif (%d)\n", aif);
		return -1;
	}
	WMT_DBG_FUNC("call pin_ctrl for aif:%d, share:%d\n", aif,
		     (share == MTK_WCN_BOOL_TRUE) ? 1 : 0);
	/* Translate enum CMB_STUB_AIF_X into WMT_IC_PIN_STATE by array */
	return wmt_lib_pin_ctrl(WMT_IC_PIN_AUDIO,
				cmb_aif2pin_stat[aif],
				(MTK_WCN_BOOL_TRUE ==
				 share) ? WMT_LIB_AIF_FLAG_SHARE : WMT_LIB_AIF_FLAG_SEPARATE);
}

INT32 wmt_lib_host_awake_get(VOID)
{
	return wmt_plat_wake_lock_ctrl(WL_OP_GET);
}

INT32 wmt_lib_host_awake_put(VOID)
{
	return wmt_plat_wake_lock_ctrl(WL_OP_PUT);
}

MTK_WCN_BOOL wmt_lib_btm_cb(MTKSTP_BTM_WMT_OP_T op)
{
	MTK_WCN_BOOL bRet = MTK_WCN_BOOL_FALSE;

	if (op == BTM_RST_OP) {
		/* high priority, not to enqueue into the queue of wmtd */
		WMT_INFO_FUNC("Invoke whole chip reset from stp_btm!!!\n");
		wmt_lib_cmb_rst(WMTRSTSRC_RESET_STP);
		bRet = MTK_WCN_BOOL_TRUE;
	} else if (op == BTM_DMP_OP) {

		WMT_WARN_FUNC("TBD!!!\n");
	} else if (op == BTM_GET_AEE_SUPPORT_FLAG) {
		bRet = wmt_core_get_aee_dump_flag();
	} else if (op == BTM_TRIGGER_STP_ASSERT_OP) {
		bRet = wmt_core_trigger_stp_assert();
	}
	return bRet;
}

MTK_WCN_BOOL wmt_cdev_rstmsg_snd(ENUM_WMTRSTMSG_TYPE_T msg)
{

	INT32 i = 0;
	P_DEV_WMT pDevWmt = &gDevWmt;
	UINT8 *drv_name[] = {
		"DRV_TYPE_BT",
		"DRV_TYPE_FM",
		"DRV_TYPE_GPS",
		"DRV_TYPE_WIFI",
		"DRV_TYPE_ANT",
		"UNKNOWN"
	};

	for (i = 0; i <= WMTDRV_TYPE_ANT; i++) {
		/* <1> check if reset callback is registered */
		if (pDevWmt->rFdrvCb.fDrvRst[i]) {
			/* <2> send the msg to this subfucntion */
			/*src, dst, msg_type, msg_data, msg_size */
			pDevWmt->rFdrvCb.fDrvRst[i] (WMTDRV_TYPE_WMT, i, WMTMSG_TYPE_RESET, &msg,
						     sizeof(ENUM_WMTRSTMSG_TYPE_T));
			WMT_INFO_FUNC("type = %s, msg sent\n", drv_name[i]);
		} else {
			WMT_DBG_FUNC("type = %s, unregistered\n", drv_name[i]);
		}
	}

	return MTK_WCN_BOOL_TRUE;
}

VOID wmt_lib_state_init(VOID)
{
	/* UINT32 i = 0; */
	P_DEV_WMT pDevWmt = &gDevWmt;
	P_OSAL_OP pOp;

	/* Initialize op queue */
	/* RB_INIT(&pDevWmt->rFreeOpQ, WMT_OP_BUF_SIZE); */
	/* RB_INIT(&pDevWmt->rActiveOpQ, WMT_OP_BUF_SIZE); */

	while (!RB_EMPTY(&pDevWmt->rActiveOpQ)) {
		pOp = wmt_lib_get_op(&pDevWmt->rActiveOpQ);
		if (pOp) {
			if (atomic_dec_and_test(&pOp->ref_count))
				wmt_lib_put_op(&pDevWmt->rFreeOpQ, pOp);
			else if (osal_op_is_wait_for_signal(pOp))
				osal_op_raise_signal(pOp, -1);
		}
	}
}


INT32 wmt_lib_sdio_ctrl(UINT32 on)
{

	P_OSAL_OP pOp;
	MTK_WCN_BOOL bRet;
	P_OSAL_SIGNAL pSignal;

	pOp = wmt_lib_get_free_op();
	if (!pOp) {
		WMT_DBG_FUNC("get_free_lxop fail\n");
		return MTK_WCN_BOOL_FALSE;
	}

	WMT_DBG_FUNC("call WMT_OPID_SDIO_CTRL\n");

	pSignal = &pOp->signal;
	pOp->op.opId = WMT_OPID_SDIO_CTRL;
	pOp->op.au4OpData[0] = on;
	pSignal->timeoutValue = MAX_GPIO_CTRL_TIME;


	bRet = wmt_lib_put_act_op(pOp);
	if (bRet == MTK_WCN_BOOL_FALSE) {
		WMT_WARN_FUNC("WMT_OPID_SDIO_CTRL failed\n");
		return -1;
	}
	WMT_DBG_FUNC("OPID(WMT_OPID_SDIO_CTRL)ok\n");
	return 0;
}

MTK_WCN_BOOL wmt_lib_hw_state_show(VOID)
{
	P_OSAL_OP pOp;
	MTK_WCN_BOOL bRet;
	P_OSAL_SIGNAL pSignal;

	pOp = wmt_lib_get_free_op();
	if (!pOp) {
		WMT_DBG_FUNC("get_free_lxop fail\n");
		return MTK_WCN_BOOL_FALSE;
	}

	WMT_DBG_FUNC("call WMT_OPID_HW_STATE_SHOW\n");

	pSignal = &pOp->signal;
	pOp->op.opId = WMT_OPID_GPIO_STATE;
	pSignal->timeoutValue = MAX_GPIO_CTRL_TIME;

	bRet = wmt_lib_put_act_op(pOp);
	if (bRet == MTK_WCN_BOOL_FALSE) {
		WMT_WARN_FUNC("WMT_OPID_HW_STATE_SHOW failed\n");
		return MTK_WCN_BOOL_FALSE;
	}
	WMT_DBG_FUNC("OPID(WMT_OPID_HW_STATE_SHOW)ok\n");

	return MTK_WCN_BOOL_TRUE;
}


MTK_WCN_BOOL wmt_lib_hw_rst(VOID)
{

	P_OSAL_OP pOp;
	MTK_WCN_BOOL bRet;
	P_OSAL_SIGNAL pSignal;
	P_DEV_WMT pDevWmt = &gDevWmt;

	wmt_lib_state_init();

	osal_clear_bit(WMT_STAT_STP_REG, &pDevWmt->state);
	osal_clear_bit(WMT_STAT_STP_OPEN, &pDevWmt->state);
	osal_clear_bit(WMT_STAT_STP_EN, &pDevWmt->state);
	osal_clear_bit(WMT_STAT_STP_RDY, &pDevWmt->state);
	osal_clear_bit(WMT_STAT_RX, &pDevWmt->state);
	osal_clear_bit(WMT_STAT_CMD, &pDevWmt->state);

	/*Before do hardware reset, we show GPIO state to check if others modified our pin state accidentially */
	wmt_lib_hw_state_show();
	pOp = wmt_lib_get_free_op();
	if (!pOp) {
		WMT_DBG_FUNC("get_free_lxop fail\n");
		return MTK_WCN_BOOL_FALSE;
	}

	WMT_DBG_FUNC("call WMT_OPID_HW_RST\n");

	pSignal = &pOp->signal;
	pOp->op.opId = WMT_OPID_HW_RST;
	pSignal->timeoutValue = MAX_GPIO_CTRL_TIME;


	bRet = wmt_lib_put_act_op(pOp);
	if (bRet == MTK_WCN_BOOL_FALSE) {
		WMT_WARN_FUNC("WMT_OPID_HW_RST failed\n");
		return MTK_WCN_BOOL_FALSE;
	}
	WMT_DBG_FUNC("OPID(WMT_OPID_HW_RST)ok\n");

	return MTK_WCN_BOOL_TRUE;
}

MTK_WCN_BOOL wmt_lib_sw_rst(INT32 baudRst)
{

	P_OSAL_OP pOp;
	MTK_WCN_BOOL bRet;
	P_OSAL_SIGNAL pSignal;

	/* <1> wmt state reset */
	wmt_lib_state_init();

	/* <2> Reset STP data structure */
	WMT_DBG_FUNC("Cleanup STP context\n");
	mtk_wcn_stp_flush_context();
	stp_dbg_reset();

	/* <3> Reset STP-PSM data structure */
	WMT_DBG_FUNC("Cleanup STP-PSM context\n");
	mtk_wcn_stp_psm_reset();

	/* <4> do sw reset in wmt-core */
	pOp = wmt_lib_get_free_op();
	if (!pOp) {
		WMT_WARN_FUNC("get_free_lxop fail\n");
		return MTK_WCN_BOOL_FALSE;
	}

	WMT_DBG_FUNC("call WMT_OPID_SW_RST\n");

	pSignal = &pOp->signal;
	pSignal->timeoutValue = MAX_FUNC_ON_TIME;

	pOp->op.opId = WMT_OPID_SW_RST;
	pOp->op.au4OpData[0] = baudRst;



	bRet = wmt_lib_put_act_op(pOp);
	if (bRet == MTK_WCN_BOOL_FALSE) {
		WMT_WARN_FUNC("WMT_OPID_SW_RST failed\n");
		return MTK_WCN_BOOL_FALSE;
	}
	WMT_DBG_FUNC("OPID(WMT_OPID_SW_RST)ok\n");

	return MTK_WCN_BOOL_TRUE;
}


ENUM_WMTRSTRET_TYPE_T wmt_lib_cmb_rst(ENUM_WMTRSTSRC_TYPE_T src)
{
#define RETRYTIMES 10
	MTK_WCN_BOOL bRet;
	ENUM_WMTRSTRET_TYPE_T retval = WMTRSTRET_MAX;
	ENUM_WMTRSTMSG_TYPE_T rstMsg = WMTRSTMSG_RESET_MAX;
	INT32 retries = RETRYTIMES;
	P_DEV_WMT pDevWmt = &gDevWmt;
	P_OSAL_OP pOp;
	UINT8 *srcName[] = { "WMTRSTSRC_RESET_BT",
		"WMTRSTSRC_RESET_FM",
		"WMTRSTSRC_RESET_GPS",
		"WMTRSTSRC_RESET_WIFI",
		"WMTRSTSRC_RESET_STP",
		"WMTRSTSRC_RESET_TEST"
	};
	INT32 coredump_mode = mtk_wcn_stp_coredump_flag_get();

	WMT_INFO_FUNC("coredump mode == %d. Connsys coredump is %s.",
			coredump_mode, coredump_mode ? "enabled" : "disabled");

	if (src >= 0 && src < WMTRSTSRC_RESET_MAX)
		WMT_INFO_FUNC("reset source = %s\n", srcName[src]);

	if (src == WMTRSTSRC_RESET_TEST) {
		pOp = wmt_lib_get_current_op(pDevWmt);
		if (pOp && ((pOp->op.opId == WMT_OPID_FUNC_ON)
			    || (pOp->op.opId == WMT_OPID_FUNC_OFF))) {
			WMT_INFO_FUNC("can't do reset by test src when func on/off\n");
			return -1;
		}
	}
	/* <1> Consider the multi-context combo_rst case. */
	if (osal_test_and_set_bit(WMT_STAT_RST_ON, &pDevWmt->state)) {
		retval = WMTRSTRET_ONGOING;
		goto rstDone;
	}
	/* <2> Block all STP request */
	if (wmt_lib_psm_lock_trylock() == 0) {
		if (chip_reset_only == 1) {
			wmt_lib_fw_patch_update_rst_ctrl(1);
			fw_patch_rst_time = 0;
			retval = WMTRSTRET_RETRY;
			goto rstDone;
		}
		mtk_wcn_stp_enable(0);
	} else {
		mtk_wcn_stp_enable(0);
		wmt_lib_psm_lock_release();
	}

	/* <3> RESET_START notification */
	bRet = wmt_cdev_rstmsg_snd(WMTRSTMSG_RESET_START);
	if (bRet == MTK_WCN_BOOL_FALSE) {
		WMT_ERR_FUNC("[whole chip reset] fail at wmt_lib_rstmsg_snd!\n");
		retval = WMTRSTRET_FAIL;
		goto rstDone;
	}
	/* wakeup blocked opid */
	pOp = wmt_lib_get_current_op(pDevWmt);
	if (osal_op_is_wait_for_signal(pOp))
		osal_op_raise_signal(pOp, -1);
	/* wakeup blocked cmd */
	wmt_dev_rx_event_cb();

	/* <4> retry until reset flow successful */
	while (retries > 0) {
		/* <4.1> reset combo hw */
		bRet = wmt_lib_hw_rst();
		if (bRet == MTK_WCN_BOOL_FALSE) {
			WMT_ERR_FUNC("[whole chip reset] fail at wmt_lib_hw_rst!\n");
			retries--;
			continue;
		}
		/* <4.2> reset driver/combo sw */
		bRet = wmt_lib_sw_rst(1);
		if (bRet == MTK_WCN_BOOL_FALSE) {
			WMT_ERR_FUNC("[whole chip reset] fail at wmt_lib_sw_rst!\n");
			retries--;
			continue;
		}
		break;
	}
	osal_clear_bit(WMT_STAT_RST_ON, &pDevWmt->state);
	if (bRet == MTK_WCN_BOOL_FALSE) {
		rstMsg = WMTRSTMSG_RESET_END_FAIL;
		WMT_INFO_FUNC("[whole chip reset] fail! retries = %d\n", RETRYTIMES - retries);
	} else {
		rstMsg = WMTRSTMSG_RESET_END;
		WMT_INFO_FUNC("[whole chip reset] ok! retries = %d\n", RETRYTIMES - retries);
	}


	/* <5> RESET_END notification */
	bRet = wmt_cdev_rstmsg_snd(rstMsg);
	if (bRet == MTK_WCN_BOOL_FALSE) {
		WMT_ERR_FUNC("[whole chip reset] fail at wmt_lib_rstmsg_snd!\n");
		retval = WMTRSTRET_FAIL;
	} else {
		retval = rstMsg == WMTRSTMSG_RESET_END ? WMTRSTRET_SUCCESS : WMTRSTRET_FAIL;
	}
	mtk_wcn_stp_assert_flow_ctrl(0);
	mtk_wcn_stp_coredump_start_ctrl(0);
	mtk_wcn_stp_set_wmt_trg_assert(0);
	mtk_wcn_stp_emi_dump_flag_ctrl(0);
rstDone:
	osal_clear_bit(WMT_STAT_RST_ON, &pDevWmt->state);
	chip_reset_only = 0;
	mtk_wcn_consys_sleep_info_restore();
	return retval;
}


MTK_WCN_BOOL wmt_lib_msgcb_reg(ENUM_WMTDRV_TYPE_T eType, PF_WMT_CB pCb)
{

	MTK_WCN_BOOL bRet = MTK_WCN_BOOL_FALSE;
	P_DEV_WMT pWmtDev = &gDevWmt;

	if (eType >= 0 && eType <= WMTDRV_TYPE_ANT) {
		WMT_DBG_FUNC("reg ok!\n");
		pWmtDev->rFdrvCb.fDrvRst[eType] = pCb;
		bRet = MTK_WCN_BOOL_TRUE;
	} else {
		WMT_WARN_FUNC("reg fail!\n");
	}

	return bRet;
}

MTK_WCN_BOOL wmt_lib_msgcb_unreg(ENUM_WMTDRV_TYPE_T eType)
{
	MTK_WCN_BOOL bRet = MTK_WCN_BOOL_FALSE;
	P_DEV_WMT pWmtDev = &gDevWmt;

	if (eType >= 0 && eType <= WMTDRV_TYPE_WIFI) {
		WMT_DBG_FUNC("unreg ok!\n");
		pWmtDev->rFdrvCb.fDrvRst[eType] = NULL;
		bRet = MTK_WCN_BOOL_TRUE;
	} else {
		WMT_WARN_FUNC("unreg fail!\n");
	}

	return bRet;
}


UINT32 wmt_lib_dbg_level_set(UINT32 level)
{
	gWmtDbgLvl = level > WMT_LOG_LOUD ? WMT_LOG_LOUD : level;
	return 0;
}
#ifdef CONFIG_MTK_COMBO_CHIP_DEEP_SLEEP_SUPPORT
INT32 wmt_lib_deep_sleep_ctrl(INT32 value)
{
	MTK_WCN_BOOL ret = MTK_WCN_BOOL_FALSE;

	WMT_INFO_FUNC("g_deep_sleep_flag value (%d) set form wmt_dbg.\n", value);
	ret = wmt_core_deep_sleep_ctrl(value);
	if (sdio_deep_sleep_flag_set) {
		if (value)
			(*sdio_deep_sleep_flag_set)(MTK_WCN_BOOL_TRUE);
		else
			(*sdio_deep_sleep_flag_set)(MTK_WCN_BOOL_FALSE);
	} else {
		WMT_ERR_FUNC("sdio_deep_sleep_flag_set is not register");
		return -1;
	}
	return 0;
}

MTK_WCN_BOOL wmt_lib_deep_sleep_flag_set(MTK_WCN_BOOL flag)
{
	if (sdio_deep_sleep_flag_set) {
		(*sdio_deep_sleep_flag_set)(flag);
	} else {
		WMT_ERR_FUNC("sdio_deep_sleep_flag_set is not register");
		return MTK_WCN_BOOL_FALSE;
	}
		return MTK_WCN_BOOL_TRUE;
}
#endif
INT32 wmt_lib_set_stp_wmt_last_close(UINT32 value)
{
	return mtk_wcn_stp_set_wmt_last_close(value);
}

INT32 wmt_lib_notify_stp_sleep(VOID)
{
	INT32 iRet = 0x0;

	iRet = wmt_lib_psm_lock_aquire();
	if (iRet) {
		WMT_ERR_FUNC("--->lock psm_lock failed, iRet=%d\n", iRet);
		return iRet;
	}

	iRet = mtk_wcn_stp_notify_sleep_for_thermal();
	wmt_lib_psm_lock_release();

	return iRet;
}

VOID wmt_lib_set_patch_num(UINT32 num)
{
	P_DEV_WMT pWmtDev = &gDevWmt;

	pWmtDev->patchNum = num;
}

VOID wmt_lib_set_patch_info(P_WMT_PATCH_INFO pPatchinfo)
{
	P_DEV_WMT pWmtDev = &gDevWmt;

	pWmtDev->pWmtPatchInfo = pPatchinfo;
}

VOID wmt_lib_set_rom_patch_info(struct wmt_rom_patch_info *PatchInfo, ENUM_WMTDRV_TYPE_T type)
{
	P_DEV_WMT pWmtDev = &gDevWmt;

	if (type < 0)
		return;

	/* Allow info of a type to be set only once, to avoid inproper usage */
	if (pWmtDev->pWmtRomPatchInfo[type])
		return;

	pWmtDev->pWmtRomPatchInfo[type] = kcalloc(1, sizeof(struct wmt_rom_patch_info),
							  GFP_ATOMIC);

	if (pWmtDev->pWmtRomPatchInfo[type])
		osal_memcpy(pWmtDev->pWmtRomPatchInfo[type], PatchInfo,
			    sizeof(struct wmt_rom_patch_info));
}

INT32 wmt_lib_set_current_op(P_DEV_WMT pWmtDev, P_OSAL_OP pOp)
{
	if (pWmtDev) {
		pWmtDev->pCurOP = pOp;
		WMT_DBG_FUNC("pOp=0x%p\n", pOp);
		return 0;
	}
	WMT_ERR_FUNC("Invalid pointer\n");
	return -1;
}

P_OSAL_OP wmt_lib_get_current_op(P_DEV_WMT pWmtDev)
{
	if (pWmtDev)
		return pWmtDev->pCurOP;
	WMT_ERR_FUNC("Invalid pointer\n");
	return NULL;
}

INT32 wmt_lib_set_worker_op(P_DEV_WMT pWmtDev, P_OSAL_OP pOp)
{
	if (pWmtDev) {
		pWmtDev->pWorkerOP = pOp;
		WMT_DBG_FUNC("pOp=0x%p\n", pOp);
		return 0;
	}
	WMT_ERR_FUNC("Invalid pointer\n");
	return -1;
}

P_OSAL_OP wmt_lib_get_worker_op(P_DEV_WMT pWmtDev)
{
	if (pWmtDev)
		return pWmtDev->pWorkerOP;
	WMT_ERR_FUNC("Invalid pointer\n");
	return NULL;
}

UINT8 *wmt_lib_get_fwinfor_from_emi(UINT8 section, UINT32 offset, UINT8 *buf, UINT32 len)
{
	UINT8 *pAddr = NULL;
	UINT32 sublen1 = 0;
	UINT32 sublen2 = 0;
	P_CONSYS_EMI_ADDR_INFO p_consys_info;

	p_consys_info = wmt_plat_get_emi_phy_add();
	osal_assert(p_consys_info);

	if (section == 0) {
		pAddr = wmt_plat_get_emi_virt_add(0x0);
		if (len > 1024)
			len = 1024;
		if (!pAddr) {
			WMT_ERR_FUNC("wmt-lib: get EMI virtual base address fail\n");
		} else {
			WMT_INFO_FUNC("vir addr(0x%p)\n", pAddr);
			osal_memcpy_fromio(&buf[0], pAddr, len);
		}
	} else {
		if (p_consys_info == NULL) {
			WMT_ERR_FUNC("wmt-lib: get EMI physical address fail!\n");
			return 0;
		}

		if (offset >= 0x7fff)
			offset = 0x0;

		if (offset + len > 32768) {
			pAddr = wmt_plat_get_emi_virt_add(offset + p_consys_info->paged_trace_off);
			if (!pAddr) {
				WMT_ERR_FUNC("wmt-lib: get part1 EMI virtual base address fail\n");
			} else {
				WMT_INFO_FUNC("part1 vir addr(0x%p)\n", pAddr);
				sublen1 = 0x7fff - offset;
				osal_memcpy_fromio(&buf[0], pAddr, sublen1);
			}
			pAddr = wmt_plat_get_emi_virt_add(p_consys_info->paged_trace_off);
			if (!pAddr) {
				WMT_ERR_FUNC("wmt-lib: get part2 EMI virtual base address fail\n");
			} else {
				WMT_INFO_FUNC("part2 vir addr(0x%p)\n", pAddr);
				sublen2 = len - sublen1;
				osal_memcpy_fromio(&buf[sublen1], pAddr, sublen2);
			}
		} else {
			pAddr = wmt_plat_get_emi_virt_add(offset + p_consys_info->paged_trace_off);
			if (!pAddr) {
				WMT_ERR_FUNC("wmt-lib: get EMI virtual base address fail\n");
			} else {
				WMT_INFO_FUNC("vir addr(0x%p)\n", pAddr);
				osal_memcpy_fromio(&buf[0], pAddr, len);
			}
		}
	}

	return 0;
}
EXPORT_SYMBOL(wmt_lib_get_fwinfor_from_emi);

INT32 wmt_lib_merge_if_flag_ctrl(UINT32 enable)
{
#if WMT_PLAT_ALPS
	return wmt_plat_merge_if_flag_ctrl(enable);
#endif
}


INT32 wmt_lib_merge_if_flag_get(UINT32 enable)
{
#if WMT_PLAT_ALPS
	return wmt_plat_merge_if_flag_get();
#endif
}


PUINT8 wmt_lib_get_cpupcr_xml_format(PUINT32 pLen)
{
	PUINT8 temp;

	osal_memset(&g_cpupcr_buf[0], 0, WMT_STP_CPUPCR_BUF_SIZE);
	temp = g_cpupcr_buf;
	*pLen += stp_dbg_cpupcr_infor_format(temp, WMT_STP_CPUPCR_BUF_SIZE);
	*pLen += mtk_stp_dbg_dmp_append(temp + *pLen, WMT_STP_CPUPCR_BUF_SIZE - *pLen);

	WMT_INFO_FUNC("print xml buffer,len(%d):\n\n", *pLen);

	WMT_INFO_FUNC("%s", g_cpupcr_buf);

	return &g_cpupcr_buf[0];
}


/**
 * called by wmt_dev wmt_dev_proc_for_dump_info_read
 */
PUINT8 wmt_lib_get_cpupcr_reg_info(PUINT32 pLen, PUINT32 consys_reg)
{
	osal_memset(&g_cpupcr_buf[0], 0, WMT_STP_CPUPCR_BUF_SIZE);
	if (consys_reg != NULL)
		*pLen += stp_dbg_dump_cpupcr_reg_info(g_cpupcr_buf, consys_reg[1]);
	else
		*pLen += osal_sprintf(g_cpupcr_buf + *pLen, "0\n");
	WMT_INFO_FUNC("print buffer,len(%d):\n\n", *pLen);
	WMT_INFO_FUNC("%s", g_cpupcr_buf);
	return &g_cpupcr_buf[0];
}

INT32 wmt_lib_tm_temp_query(VOID)
{
	return wmt_dev_tm_temp_query();
}

INT32 wmt_lib_register_thermal_ctrl_cb(thermal_query_ctrl_cb thermal_ctrl)
{
	wmt_plat_thermal_ctrl_cb_reg(thermal_ctrl);
	return 0;
}

INT32 wmt_lib_register_trigger_assert_cb(trigger_assert_cb trigger_assert)
{
	wmt_plat_trigger_assert_cb_reg(trigger_assert);
	return 0;
}

UINT32 wmt_lib_set_host_assert_info(UINT32 type, UINT32 reason, UINT32 en)
{
	return stp_dbg_set_host_assert_info(type, reason, en);
}

INT8 wmt_lib_co_clock_get(void)
{
	if (gDevWmt.rWmtGenConf.cfgExist)
		return gDevWmt.rWmtGenConf.co_clock_flag;
	else
		return -1;
}


UINT32 wmt_lib_get_drv_status(UINT32 type)
{
	return wmt_core_get_drv_status((ENUM_WMTDRV_TYPE_T) type);
}

INT32 wmt_lib_trigger_reset(VOID)
{
	return wmt_btm_trigger_reset();
}

INT32 wmt_lib_trigger_assert(ENUM_WMTDRV_TYPE_T type, UINT32 reason)
{
	return wmt_lib_trigger_assert_keyword(type, reason, NULL);
}

INT32 wmt_lib_trigger_assert_keyword(ENUM_WMTDRV_TYPE_T type, UINT32 reason, PUINT8 keyword)
{
	INT32 iRet = -1;
	WMT_CTRL_DATA ctrlData;

	if (wmt_lib_assert_lock_trylock() == 0) {
		WMT_INFO_FUNC("Can't lock assert mutex which might be held by another trigger assert procedure.\n");
		return iRet;
	}

	wmt_core_set_coredump_state(DRV_STS_FUNC_ON);

	ctrlData.ctrlId = (SIZE_T) WMT_CTRL_TRG_ASSERT;
	ctrlData.au4CtrlData[0] = (SIZE_T) type;
	ctrlData.au4CtrlData[1] = (SIZE_T) reason;
	ctrlData.au4CtrlData[2] = (SIZE_T) keyword;

	iRet = wmt_ctrl(&ctrlData);
	if (iRet) {
		/* ERROR */
		WMT_ERR_FUNC
		    ("WMT-CORE: wmt_core_ctrl failed: type(%d), reason(%d), keyword(%s), iRet(%d)\n",
		     type, reason, keyword, iRet);
		osal_assert(0);
	}
	wmt_lib_assert_lock_release();

	return iRet;
}

#if CFG_WMT_PS_SUPPORT
UINT32 wmt_lib_quick_sleep_ctrl(UINT32 en)
{
	WMT_WARN_FUNC("%s quick sleep mode\n", en ? "enable" : "disable");
	g_quick_sleep_ctrl = en;
	return 0;
}
#endif

UINT32 wmt_lib_fw_patch_update_rst_ctrl(UINT32 en)
{
	WMT_WARN_FUNC("%s fw patch update reset\n", en ? "enable" : "disable");
	g_fw_patch_update_rst = en;
	return 0;
}

#if CONSYS_ENALBE_SET_JTAG
UINT32 wmt_lib_jtag_flag_set(UINT32 en)
{
	return wmt_plat_jtag_flag_ctrl(en);
}
#endif

UINT32 wmt_lib_soc_set_wifiver(UINT32 wifiver)
{
	return stp_dbg_set_wifiver(wifiver);
}

UINT32 wmt_lib_co_clock_flag_get(VOID)
{
	return wmt_plat_soc_co_clock_flag_get();
}

INT32 wmt_lib_wifi_fem_cfg_report(PVOID pvInfoBuf)
{
	INT32 iRet = 0;
	ULONG addr = 0;
	WMT_GEN_CONF *pWmtGenConf = NULL;

	/* sanity check */
	ASSERT(pvInfoBuf);

	iRet = wmt_core_ctrl(WMT_CTRL_GET_WMT_CONF, &addr, 0);

	if (iRet) {
		WMT_ERR_FUNC("ctrl GET_WMT_CONF fail(%d)\n", iRet);
		return -2;
	}

	pWmtGenConf = (P_WMT_GEN_CONF) addr;

	WMT_DBG_FUNC("pWmtGenConf->coex_wmt_wifi_path=0x%x\n", pWmtGenConf->coex_wmt_wifi_path);

	/* Memory copy */
	osal_memcpy((PUINT8)(pvInfoBuf), &pWmtGenConf->coex_wmt_wifi_path,
		osal_sizeof(pWmtGenConf->coex_wmt_wifi_path));
	return iRet;
}

INT32 wmt_lib_sdio_reg_rw(INT32 func_num, INT32 direction, UINT32 offset, UINT32 value)
{
	INT32 ret = -1;
	ENUM_WMT_CHIP_TYPE chip_type;

	chip_type = wmt_detect_get_chip_type();

	if (chip_type == WMT_CHIP_TYPE_COMBO) {
		if (sdio_reg_rw)
			ret = sdio_reg_rw(func_num, direction, offset, value);
		else
			WMT_ERR_FUNC("sdio_reg_rw callback is not set, maybe the sdio funcxx write/read not used\n");
	} else
		WMT_ERR_FUNC("It`s soc project, this function is not used\n");
	return ret;
}

VOID wmt_lib_dump_wmtd_backtrace(VOID)
{
	osal_thread_show_stack(&gDevWmt.thread);
}

INT32 wmt_lib_met_cmd(UINT32 value)
{
	P_OSAL_OP pOp;
	MTK_WCN_BOOL bRet;
	P_OSAL_SIGNAL pSignal;

	pOp = wmt_lib_get_free_op();
	if (!pOp) {
		WMT_DBG_FUNC("get_free_lxop fail\n");
		return -1;
	}

	pSignal = &pOp->signal;
	pSignal->timeoutValue = MAX_EACH_WMT_CMD;
	WMT_DBG_FUNC("met ctrl value(0x%x)\n", value);
	pOp->op.opId = WMT_OPID_MET_CTRL;
	pOp->op.au4OpData[0] = value;
	if (DISABLE_PSM_MONITOR()) {
		WMT_ERR_FUNC("wake up failed\n");
		wmt_lib_put_op_to_free_queue(pOp);
		return -1;
	}

	bRet = wmt_lib_put_act_op(pOp);
	ENABLE_PSM_MONITOR();

	if (bRet != MTK_WCN_BOOL_FALSE)
		return 0;

	return -1;
}

UINT32 wmt_lib_get_gps_lna_pin_num(VOID)
{
	return mtk_consys_get_gps_lna_pin_num();
}

INT32 wmt_lib_met_ctrl(INT32 met_ctrl, INT32 log_ctrl)
{
	P_DEV_WMT p_devwmt;
	P_OSAL_THREAD p_thread;
	INT32 ret;
	P_CONSYS_EMI_ADDR_INFO emi_info;

	emi_info = mtk_wcn_consys_soc_get_emi_phy_add();
	if (emi_info == NULL) {
		WMT_ERR_FUNC("get EMI info failed\n");
		return -1;
	}

	if (!emi_info->emi_met_size) {
		WMT_ERR_FUNC("met debug function is not support\n");
		return -1;
	}

	ret = wmt_lib_met_cmd(met_ctrl);
	if (ret) {
		WMT_ERR_FUNC("send MET ctrl command fail(%d)\n", ret);
		return -1;
	}

	p_devwmt = &gDevWmt;
	p_thread = &gDevWmt.met_thread;
	if (met_ctrl & 0x1) {
		/*met enable*/
		/* Create mtk_wmt_met thread */
		osal_strncpy(p_thread->threadName, "mtk_wmt_met", sizeof(p_thread->threadName));
		p_devwmt->met_log_ctrl = log_ctrl;
		p_thread->pThreadData = (PVOID) p_devwmt;
		p_thread->pThreadFunc = (PVOID) met_thread;
		ret = osal_thread_create(p_thread);
		if (ret) {
			WMT_ERR_FUNC("osal_thread_create(0x%p) fail(%d)\n", p_thread, ret);
			return -1;
		}
		/* start running mtk_wmt_met */
		ret = osal_thread_run(p_thread);
		if (ret) {
			WMT_ERR_FUNC("osal_thread_run(0x%p) fail(%d)\n", p_thread, ret);
			return -1;
		}
	} else {
		/*met disable*/
		/* stop running mtk_wmt_met */
		ret = osal_thread_stop(p_thread);
		if (ret) {
			WMT_ERR_FUNC("osal_thread_stop(0x%p) fail(%d)\n", p_thread, ret);
			return -1;
		}
	}

	return 0;
}

VOID wmt_lib_set_ext_ldo(UINT32 flag)
{
	gDevWmt.ext_ldo_flag = flag;
}

UINT32 wmt_lib_get_ext_ldo(VOID)
{
	return gDevWmt.ext_ldo_flag;
}

static VOID wmt_lib_utc_sync_timeout_handler(timer_handler_arg arg)
{
	schedule_work(&gDevWmt.utcSyncWorker);
}

static VOID wmt_lib_utc_sync_worker_handler(struct work_struct *work)
{
	wmt_lib_utc_time_sync();
}

INT32 wmt_lib_fw_log_ctrl(enum wmt_fw_log_type type, UINT8 onoff, UINT8 level)
{
	P_OSAL_OP pOp;
	MTK_WCN_BOOL bRet;
	P_OSAL_SIGNAL pSignal;

	pOp = wmt_lib_get_free_op();
	if (!pOp) {
		WMT_WARN_FUNC("get_free_lxop fail\n");
		return -1;
	}

	pSignal = &pOp->signal;
	pSignal->timeoutValue = 0;
	pOp->op.opId = WMT_OPID_FW_LOG_CTRL;
	pOp->op.au4OpData[0] = type;
	pOp->op.au4OpData[1] = onoff;
	pOp->op.au4OpData[2] = level;

	if (DISABLE_PSM_MONITOR()) {
		WMT_ERR_FUNC("wake up failed\n");
		wmt_lib_put_op_to_free_queue(pOp);
		return -2;
	}

	bRet = wmt_lib_put_act_op(pOp);
	ENABLE_PSM_MONITOR();
	if (bRet == MTK_WCN_BOOL_FALSE) {
		WMT_WARN_FUNC("OPID(%d) fail\n", pOp->op.opId);
		return -3;
	}
	WMT_DBG_FUNC("OPID(%d) ok\n", pOp->op.opId);

	return 0;
}

INT32 wmt_lib_gps_mcu_ctrl(PUINT8 p_tx_data_buf, UINT32 tx_data_len, PUINT8 p_rx_data_buf,
			   UINT32 rx_data_buf_len, PUINT32 p_rx_data_len)
{
	P_OSAL_OP pOp;
	MTK_WCN_BOOL bRet;
	P_OSAL_SIGNAL pSignal;

	pOp = wmt_lib_get_free_op();
	if (!pOp) {
		WMT_DBG_FUNC("get_free_lxop fail\n");
		return -1;
	}

	pSignal = &pOp->signal;
	pSignal->timeoutValue = MAX_WMT_OP_TIMEOUT;
	pOp->op.opId = WMT_OPID_GPS_MCU_CTRL;
	pOp->op.au4OpData[0] = (SIZE_T)p_tx_data_buf;
	pOp->op.au4OpData[1] = tx_data_len;
	pOp->op.au4OpData[2] = (SIZE_T)p_rx_data_buf;
	pOp->op.au4OpData[3] = rx_data_buf_len;
	pOp->op.au4OpData[4] = (SIZE_T)p_rx_data_len;
	if (DISABLE_PSM_MONITOR()) {
		WMT_ERR_FUNC("wake up failed\n");
		wmt_lib_put_op_to_free_queue(pOp);
		return -1;
	}

	bRet = wmt_lib_put_act_op(pOp);
	ENABLE_PSM_MONITOR();

	if (bRet == MTK_WCN_BOOL_FALSE) {
		WMT_WARN_FUNC("WMT_OPID_GPS_MCU_CTRL fail(%zu)\n", pOp->op.au4OpData[5]);
		return -1;
	}

	return 0;
}

VOID wmt_lib_print_wmtd_op_history(VOID)
{
	osal_op_history_print(&gDevWmt.wmtd_op_history, "wmtd_thread");
}

VOID wmt_lib_print_worker_op_history(VOID)
{
	osal_op_history_print(&gDevWmt.worker_op_history, "wmtd_worker_thread");
}
VOID wmt_lib_set_blank_status(UINT32 on_off_flag)
{
	wmt_core_set_blank_status(on_off_flag);
}

UINT32 wmt_lib_get_blank_status(VOID)
{
	return wmt_core_get_blank_status();
}

INT32 wmt_lib_blank_status_ctrl(UINT32 on_off_flag)
{
	P_OSAL_OP pOp;
	MTK_WCN_BOOL bRet;
	P_OSAL_SIGNAL pSignal;

	pOp = wmt_lib_get_free_op();
	if (!pOp) {
		WMT_DBG_FUNC("get_free_lxop fail\n");
		return -1;
	}

	pSignal = &pOp->signal;
	pSignal->timeoutValue = MAX_EACH_WMT_CMD;
	WMT_DBG_FUNC("WMT_OPID_BLANK_STATUS_CTRL on_off_flag(0x%x)\n\n", on_off_flag);
	pOp->op.opId = WMT_OPID_BLANK_STATUS_CTRL;
	pOp->op.au4OpData[0] = on_off_flag;
	if (DISABLE_PSM_MONITOR()) {
		WMT_ERR_FUNC("wake up failed\n");
		wmt_lib_put_op_to_free_queue(pOp);
		return -1;
	}

	bRet = wmt_lib_put_act_op(pOp);
	ENABLE_PSM_MONITOR();

	if (bRet != MTK_WCN_BOOL_FALSE) {
		WMT_DBG_FUNC("WMT_OPID_BLANK_STATUS_CTRL on_off_flag(0x%x) ok\n", on_off_flag);
		return 0;
	}
	WMT_WARN_FUNC("WMT_OPID_BLANK_STATUS_CTRL on_off_flag(0x%x) bRet(%d)\n", on_off_flag, bRet);
	return -1;
}

/**
 * Desinged for native service to get number of patches
 * resides in /vendor/firmware
 */
INT32 wmt_lib_get_vendor_patch_num(VOID)
{
	return gDevWmt.patch_table.num;
}

INT32 wmt_lib_set_vendor_patch_version(struct wmt_vendor_patch *p)
{
	struct vendor_patch_table *table = &(gDevWmt.patch_table);
	struct wmt_vendor_patch *patch = table->patch;

	if (patch == NULL) {
		INT32 init_capacity = 5;

		patch = (struct wmt_vendor_patch *)osal_malloc(
			 sizeof(struct wmt_vendor_patch) * init_capacity);
		if (patch == NULL) {
			WMT_ERR_FUNC("[oom]set vendor patch version");
			return -1;
		}

		table->patch = patch;
		table->capacity = init_capacity;
		table->num = 0;

		table->active_version = (PUINT8 *)osal_malloc(sizeof(PUINT8) * init_capacity);
		if (table->active_version == NULL) {
			osal_free(table->patch);
			table->patch = NULL;
			WMT_ERR_FUNC("[oom]alloc active patch");
			return -1;
		}
		osal_memset(table->active_version, 0, sizeof(PUINT8) * init_capacity);
	}

	if (table->capacity == table->num) {
		WMT_ERR_FUNC("reach to limit");
		return -1;
	}

	/* copy patch info to table */
	patch = patch + table->num;
	patch->type = p->type;
	osal_strncpy(patch->file_name, p->file_name, sizeof(p->file_name));
	osal_strncpy(patch->version, p->version, sizeof(p->version));

	table->num++;
	WMT_INFO_FUNC("set version %s %s %d",
		patch->file_name, patch->version, patch->type);
	return 0;
}

INT32 wmt_lib_get_vendor_patch_version(struct wmt_vendor_patch *p)
{
	struct vendor_patch_table *table = &(gDevWmt.patch_table);

	if (p->id >= table->num || p->id < 0) {
		WMT_ERR_FUNC("id %d out of range", p->id);
		return -1;
	}

	osal_memcpy(p, &table->patch[p->id], sizeof(struct wmt_vendor_patch));
	WMT_INFO_FUNC("get version: %s %s t:%d",
		p->file_name, p->version, p->type);
	return 0;
}

INT32 wmt_lib_set_check_patch_status(INT32 status)
{
	gDevWmt.patch_table.status = status;
	return 0;
}

INT32 wmt_lib_get_check_patch_status(VOID)
{
	return gDevWmt.patch_table.status;
}

INT32 wmt_lib_set_active_patch_version(struct wmt_vendor_patch *p)
{
	struct vendor_patch_table *table = &(gDevWmt.patch_table);

	if (p->id < 0 || p->id >= table->num) {
		WMT_ERR_FUNC("patch id: %d is invalid. num = %d", p->id, table->num);
		return -1;
	}

	if (table->active_version == NULL) {
		WMT_ERR_FUNC("active version is NULL");
		return -1;
	}

	if (table->active_version[p->id] == NULL) {
		table->active_version[p->id] = osal_malloc(sizeof(UINT8) * (WMT_FIRMWARE_VERSION_LENGTH + 1));
		if (table->active_version[p->id] == NULL) {
			WMT_ERR_FUNC("oom when alloc active_version");
			return -1;
		}
	} else if (osal_strcmp(p->version, table->active_version[p->id]) == 0)
		return 0;

	wmt_lib_set_need_update_patch_version(1);
	osal_strncpy(table->active_version[p->id], p->version, WMT_FIRMWARE_VERSION_LENGTH + 1);
	return 0;
}

INT32 wmt_lib_get_active_patch_version(struct wmt_vendor_patch *p)
{
	struct vendor_patch_table *table = &(gDevWmt.patch_table);
	INT32 id = p->id;

	if (id >= table->num || id < 0) {
		WMT_ERR_FUNC("id %d out of range", p->id);
		return -1;
	}
	if (table->active_version[id] == NULL) {
		WMT_ERR_FUNC("active_version is null: id = %d", id);
		return -1;
	}

	osal_memcpy(p, &table->patch[id], sizeof(struct wmt_vendor_patch));
	osal_strncpy(p->version, table->active_version[id],
		WMT_FIRMWARE_VERSION_LENGTH + 1);
	WMT_INFO_FUNC("get active version: %s %s t:%d id:%d",
		p->file_name, p->version, p->type, id);
	return 0;
}

INT32 wmt_lib_get_need_update_patch_version(VOID)
{
	return gDevWmt.patch_table.need_update;
}


INT32 wmt_lib_set_need_update_patch_version(INT32 need)
{
	gDevWmt.patch_table.need_update = need > 0 ? 1 : 0;
	return 0;
}

VOID mtk_lib_set_mcif_mpu_protection(MTK_WCN_BOOL enable)
{
	mtk_consys_set_mcif_mpu_protection(enable);
}

static VOID wmt_lib_assert_work_cb(struct work_struct *work)
{
	struct assert_work_st *a = &wmt_assert_work;

	wmt_lib_trigger_assert_keyword(a->type, a->reason, a->keyword);
}

VOID wmt_lib_trigger_assert_keyword_delay(ENUM_WMTDRV_TYPE_T type, UINT32 reason, PUINT8 keyword)
{
	struct assert_work_st *a = &wmt_assert_work;

	a->type = type;
	a->reason = reason;
	if (snprintf(a->keyword, sizeof(a->keyword), "%s", keyword) < 0) {
		WMT_INFO_FUNC("snprintf a->keyword fail\n");
	} else {
		WMT_ERR_FUNC("Assert: type = %d, reason = %d, keyword = %s", type, reason, keyword);
		schedule_work(&(a->work));
	}
}

INT32 wmt_lib_dmp_consys_state(P_CONSYS_STATE_DMP_INFO dmp_info,
			unsigned int cpupcr_times, unsigned int slp_ms)
{
	P_OSAL_OP pOp;
	MTK_WCN_BOOL bRet = MTK_WCN_BOOL_TRUE;
	P_OSAL_SIGNAL pSignal;
	P_CONSYS_STATE_DMP_OP dmp_op = NULL;
	P_CONSYS_STATE_DMP_OP tmp_op;
	int i, wait_ms = 1000, tmp;
	struct consys_state_dmp_req *p_req = &gDevWmt.state_dmp_req;


	if (cpupcr_times > WMT_LIB_DMP_CONSYS_MAX_TIMES) {
		pr_warn("dump too many times [%d]\n", cpupcr_times);
		return MTK_WCN_BOOL_FALSE;
	}

	/* make sure:						*/
	/* 1. consys already power on		*/
	/* 2. consys register is readable	*/
	if (wmt_lib_get_drv_status(WMTDRV_TYPE_WMT) != DRV_STS_FUNC_ON
			|| osal_test_bit(WMT_STAT_PWR, &gDevWmt.state) == 0) {
		return MTK_WCN_BOOL_FALSE;
	}

	for (i = 0; i < WMT_LIB_DMP_SLOT; i++) {
		tmp_op = &p_req->consys_ops[i];
		if (osal_trylock_sleepable_lock(&tmp_op->lock) == 1) {
			if (tmp_op->status == WMT_DUMP_STATE_NONE) {
				tmp = atomic_add_return(1, &p_req->version);
				dmp_op = tmp_op;
				dmp_op->status = WMT_DUMP_STATE_SCHEDULED;
				dmp_op->version = tmp;
			}
			osal_unlock_sleepable_lock(&tmp_op->lock);
			if (dmp_op != NULL)
				break;
		}
	}

	if (dmp_op == NULL)
		return MTK_WCN_BOOL_FALSE;

	memset(&dmp_op->dmp_info, 0, sizeof(struct consys_state_dmp_info));
	dmp_op->times = cpupcr_times;
	dmp_op->cpu_sleep_ms = slp_ms;

	pOp = wmt_lib_get_free_op();
	if (!pOp) {
		WMT_DBG_FUNC("get_free_op fail\n");
		bRet = MTK_WCN_BOOL_FALSE;
		goto err;
	}

	tmp = cpupcr_times * slp_ms;
	if (wait_ms < tmp)
		wait_ms = tmp + 300;

	pSignal = &pOp->signal;
	pOp->op.opId = WMT_OPID_GET_CONSYS_STATE;
	pOp->op.au4OpData[0] = (SIZE_T)dmp_op;
	pOp->op.au4OpData[1] = (SIZE_T)dmp_op->version;
	pSignal->timeoutValue = wait_ms;

	bRet = wmt_lib_put_act_op(pOp);

	if (bRet == MTK_WCN_BOOL_FALSE) {
		WMT_WARN_FUNC("WMT_OPID_GET_CONSYS_STATE failed\n");
		goto err;
	}

	memcpy(dmp_info, &dmp_op->dmp_info, sizeof(struct consys_state_dmp_info));
err:
	osal_lock_sleepable_lock(&dmp_op->lock);
	dmp_op->status = WMT_DUMP_STATE_NONE;
	osal_unlock_sleepable_lock(&dmp_op->lock);
	return bRet;
}

INT32 wmt_lib_reg_readable(VOID)
{
	return wmt_lib_reg_readable_by_addr(0);
}

INT32 wmt_lib_reg_readable_by_addr(SIZE_T addr)
{
	if (wmt_lib_get_drv_status(WMTDRV_TYPE_WMT) == DRV_STS_POWER_OFF
			|| osal_test_bit(WMT_STAT_PWR, &gDevWmt.state) == 0) {
		return MTK_WCN_BOOL_FALSE;
	}
	return mtk_consys_check_reg_readable_by_addr(addr);
}

