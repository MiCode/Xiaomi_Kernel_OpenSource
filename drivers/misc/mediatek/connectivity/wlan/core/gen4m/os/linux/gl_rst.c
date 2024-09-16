/*******************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2 of the GNU General Public License ("GPLv2 License")
 * or BSD License.
 *
 * GPLv2 License
 *
 * Copyright(C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * BSD LICENSE
 *
 * Copyright(C) 2016 MediaTek Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************/
/*
 ** Id: @(#) gl_rst.c@@
 */

/*! \file   gl_rst.c
 *    \brief  Main routines for supporintg MT6620 whole-chip reset mechanism
 *
 *    This file contains the support routines of Linux driver for MediaTek Inc.
 *    802.11 Wireless LAN Adapters.
 */


/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#include <linux/kernel.h>
#include <linux/workqueue.h>

#include "precomp.h"
#include "gl_rst.h"

#if (CFG_ANDORID_CONNINFRA_COREDUMP_SUPPORT == 1)
#include "fw_log_wifi.h"
#endif

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */



/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */
u_int8_t fgSimplifyResetFlow = FALSE;
uint64_t u8ResetTime;

#if CFG_CHIP_RESET_HANG
u_int8_t fgIsResetHangState = SER_L0_HANG_RST_NONE;
#endif

#if (CFG_SUPPORT_CONNINFRA == 1)
uint32_t g_u4WlanRstThreadPid;
wait_queue_head_t g_waitq_rst;
unsigned long g_ulFlag;/* GLUE_FLAG_XXX */
struct completion g_RstOffComp;
struct completion g_RstOnComp;
struct completion g_triggerComp;
KAL_WAKE_LOCK_T *g_IntrWakeLock;
struct task_struct *wlan_reset_thread;
static int g_rst_data;
u_int8_t g_IsWholeChipRst = FALSE;
u_int8_t g_SubsysRstCnt;
int g_SubsysRstTotalCnt;
int g_WholeChipRstTotalCnt;
bool g_IsTriggerTimeout = FALSE;
u_int8_t g_IsSubsysRstOverThreshold = FALSE;
u_int8_t g_IsWfsysBusHang = FALSE;
char *g_reason;
enum consys_drv_type g_WholeChipRstType;
char *g_WholeChipRstReason;
u_int8_t g_IsWfsysResetOnFail = FALSE;
u_int8_t g_IsWfsysRstDone = TRUE;
u_int8_t g_fgRstRecover = FALSE;
#endif

#if (CFG_ANDORID_CONNINFRA_COREDUMP_SUPPORT == 1)
static uint8_t *apucRstReason[RST_REASON_MAX] = {
	(uint8_t *) DISP_STRING("RST_UNKNOWN"),
	(uint8_t *) DISP_STRING("RST_PROCESS_ABNORMAL_INT"),
	(uint8_t *) DISP_STRING("RST_DRV_OWN_FAIL"),
	(uint8_t *) DISP_STRING("RST_FW_ASSERT"),
	(uint8_t *) DISP_STRING("RST_BT_TRIGGER"),
	(uint8_t *) DISP_STRING("RST_OID_TIMEOUT"),
	(uint8_t *) DISP_STRING("RST_CMD_TRIGGER"),
};
u_int8_t g_IsNeedWaitCoredump = FALSE;
#endif

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */
static enum _ENUM_CHIP_RESET_REASON_TYPE_T eResetReason;

#if CFG_CHIP_RESET_SUPPORT
static struct RESET_STRUCT wifi_rst;
u_int8_t fgIsResetting = FALSE;
#if (CFG_SUPPORT_CONNINFRA == 1)
enum ENUM_WF_RST_SOURCE g_eWfRstSource = WF_RST_SOURCE_NONE;
#endif
#endif

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */
#if CFG_CHIP_RESET_SUPPORT

#if CFG_WMT_RESET_API_SUPPORT
#if (CFG_SUPPORT_CONNINFRA == 0)
static void mtk_wifi_reset(struct work_struct *work);
static void mtk_wifi_trigger_reset(struct work_struct *work);
static void glResetCallback(enum _ENUM_WMTDRV_TYPE_T eSrcType,
			     enum _ENUM_WMTDRV_TYPE_T eDstType,
			     enum _ENUM_WMTMSG_TYPE_T eMsgType, void *prMsgBody,
			     unsigned int u4MsgLength);
#endif /*end of CFG_SUPPORT_CONNINFRA == 0*/
#else
static u_int8_t is_bt_exist(void);
static u_int8_t rst_L0_notify_step1(void);
static void wait_core_dump_end(void);
#endif
#endif

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */
void glSetRstReason(enum _ENUM_CHIP_RESET_REASON_TYPE_T
		    eReason)
{
	if (kalIsResetting())
		return;

	u8ResetTime = sched_clock();
	eResetReason = eReason;
}

int glGetRstReason(void)
{
	return eResetReason;
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
u_int8_t kalIsResetting(void)
{
#if CFG_CHIP_RESET_SUPPORT
	return fgIsResetting;
#else
	return FALSE;
#endif
}

#if CFG_CHIP_RESET_SUPPORT

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
void glResetInit(struct GLUE_INFO *prGlueInfo)
{
#if CFG_WMT_RESET_API_SUPPORT
	/* 1. Register reset callback */
#if (CFG_SUPPORT_CONNINFRA == 0)
	mtk_wcn_wmt_msgcb_reg(WMTDRV_TYPE_WIFI, glResetCallback);
	/* 2. Initialize reset work */
	INIT_WORK(&(wifi_rst.rst_trigger_work),
		  mtk_wifi_trigger_reset);
	INIT_WORK(&(wifi_rst.rst_work), mtk_wifi_reset);
#endif
#endif
	fgIsResetting = FALSE;
	wifi_rst.prGlueInfo = prGlueInfo;

#if (CFG_SUPPORT_CONNINFRA == 1)

#if (CFG_ANDORID_CONNINFRA_COREDUMP_SUPPORT == 1)
	fw_log_connsys_coredump_init();
#endif
	update_driver_reset_status(fgIsResetting);
	KAL_WAKE_LOCK_INIT(NULL, g_IntrWakeLock, "WLAN Reset");
	init_waitqueue_head(&g_waitq_rst);
	init_completion(&g_RstOffComp);
	init_completion(&g_RstOnComp);
	init_completion(&g_triggerComp);
	wlan_reset_thread = kthread_run(wlan_reset_thread_main,
					&g_rst_data, "wlan_rst_thread");
	g_SubsysRstCnt = 0;

#endif /* CFG_SUPPORT_CONNINFRA */
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
void glResetUninit(void)
{
#if CFG_WMT_RESET_API_SUPPORT
	/* 1. Deregister reset callback */
#if (CFG_SUPPORT_CONNINFRA == 0)
	mtk_wcn_wmt_msgcb_unreg(WMTDRV_TYPE_WIFI);
#else

#if (CFG_ANDORID_CONNINFRA_COREDUMP_SUPPORT == 1)
	fw_log_connsys_coredump_deinit();
#endif

	set_bit(GLUE_FLAG_HALT_BIT, &g_ulFlag);
	wake_up_interruptible(&g_waitq_rst);
#endif
#endif
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
void glSendResetRequest(void)
{
#if CFG_WMT_RESET_API_SUPPORT

	/* WMT thread would trigger whole chip reset itself */
#endif
}

u_int8_t glResetTrigger(struct ADAPTER *prAdapter,
		uint32_t u4RstFlag, const uint8_t *pucFile, uint32_t u4Line)
{
	u_int8_t fgResult = TRUE;
	uint16_t u2FwOwnVersion;
	uint16_t u2FwPeerVersion;
#if (CFG_SUPPORT_CONNINFRA == 1)
	struct mt66xx_chip_info *prChipInfo;
#endif
	dump_stack();
	if (kalIsResetting())
		return fgResult;

	fgIsResetting = TRUE;
#if (CFG_SUPPORT_CONNINFRA == 1)
	update_driver_reset_status(fgIsResetting);
#endif
#if (CFG_SUPPORT_CONNINFRA == 0)
	if (eResetReason != RST_BT_TRIGGER)
		DBGLOG(INIT, STATE, "[SER][L0] wifi trigger eResetReason=%d\n",
								eResetReason);
	else
		DBGLOG(INIT, STATE, "[SER][L0] BT trigger\n");
#endif

#if CFG_WMT_RESET_API_SUPPORT
	if (u4RstFlag & RST_FLAG_DO_CORE_DUMP)
		if (glIsWmtCodeDump())
			DBGLOG(INIT, WARN, "WMT is code dumping !\n");
#endif
	if (prAdapter == NULL)
		prAdapter = wifi_rst.prGlueInfo->prAdapter;
#if (CFG_SUPPORT_CONNINFRA == 1)
	prChipInfo = prAdapter->chip_info;
#endif
	u2FwOwnVersion = prAdapter->rVerInfo.u2FwOwnVersion;
	u2FwPeerVersion = prAdapter->rVerInfo.u2FwPeerVersion;

	DBGLOG(INIT, ERROR,
		"Trigger chip reset in %s line %u! Chip[%04X E%u] FW Ver DEC[%u.%u] HEX[%x.%x], Driver Ver[%u.%u]\n",
		 pucFile, u4Line, MTK_CHIP_REV,
	wlanGetEcoVersion(prAdapter),
		(uint16_t)(u2FwOwnVersion >> 8),
		(uint16_t)(u2FwOwnVersion & BITS(0, 7)),
		(uint16_t)(u2FwOwnVersion >> 8),
		(uint16_t)(u2FwOwnVersion & BITS(0, 7)),
		(uint16_t)(u2FwPeerVersion >> 8),
		(uint16_t)(u2FwPeerVersion & BITS(0, 7)));

	prAdapter->u4HifDbgFlag |= DEG_HIF_DEFAULT_DUMP;
	halPrintHifDbgInfo(prAdapter);

#if CFG_WMT_RESET_API_SUPPORT
#if (CFG_SUPPORT_CONNINFRA == 0)
	wifi_rst.rst_trigger_flag = u4RstFlag;
	schedule_work(&(wifi_rst.rst_trigger_work));
#else
	if (u4RstFlag & RST_FLAG_DO_CORE_DUMP)
		g_fgRstRecover = FALSE;
	else
		g_fgRstRecover = TRUE;

	if (u4RstFlag & RST_FLAG_DO_WHOLE_RESET) {
		if (prChipInfo->trigger_wholechiprst)
			prChipInfo->trigger_wholechiprst(g_reason);
	} else {
		if (prChipInfo->triggerfwassert)
			prChipInfo->triggerfwassert();
	}
#endif /*end of CFG_SUPPORT_CONNINFRA == 0*/

#else
	wifi_rst.prGlueInfo = prAdapter->prGlueInfo;
	schedule_work(&(wifi_rst.rst_work));
#endif

	return fgResult;
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
static void mtk_wifi_reset_main(struct RESET_STRUCT *rst)
{
	u_int8_t fgResult = FALSE;
	int32_t ret;
#if CFG_WMT_RESET_API_SUPPORT
	/* wlanOnAtReset(); */
	ret = wifi_reset_end(rst->rst_data);
#if (CFG_SUPPORT_CONNINFRA == 1)
	update_driver_reset_status(fgIsResetting);
	if (g_IsWholeChipRst == TRUE) {
		g_IsWholeChipRst = FALSE;
		g_IsWfsysBusHang = FALSE;
		complete(&g_RstOnComp);
	}
#endif
#else
	fgResult = rst_L0_notify_step1();

	wait_core_dump_end();

	fgResult = rst->prGlueInfo->prAdapter->chip_info->rst_L0_notify_step2();

#if CFG_CHIP_RESET_HANG
	if (fgIsResetHangState == SER_L0_HANG_RST_NONE)
		fgIsResetHangState = SER_L0_HANG_RST_TRGING;
#endif

	if (is_bt_exist() == FALSE)
		kalRemoveProbe(rst->prGlueInfo);

#endif
	if (fgSimplifyResetFlow) {
		DBGLOG(INIT, INFO, "Force down the reset flag.\n");
		fgSimplifyResetFlow = FALSE;
	}
#if (CFG_SUPPORT_CONNINFRA == 1)
	if (ret != 0) {
		g_IsWfsysResetOnFail = TRUE;
		fgSimplifyResetFlow = TRUE;
		DBGLOG(INIT, STATE,
			"Wi-Fi reset on fail, set flag(%d).\n",
			g_IsWfsysResetOnFail);
	} else {
		g_IsWfsysResetOnFail = FALSE;
		DBGLOG(INIT, STATE,
			"Wi-Fi reset on success, set flag(%d).\n",
			g_IsWfsysResetOnFail);
	}
#endif
	DBGLOG(INIT, STATE, "[SER][L0] flow end, fgResult=%d\n", fgResult);
}

#if CFG_WMT_RESET_API_SUPPORT
#if (CFG_SUPPORT_CONNINFRA == 0)
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
	struct RESET_STRUCT *rst = container_of(work,
						struct RESET_STRUCT, rst_work);
	mtk_wifi_reset_main(rst);
}

static void mtk_wifi_trigger_reset(struct work_struct *work)
{
	u_int8_t fgResult = FALSE;
	struct RESET_STRUCT *rst = container_of(work,
					struct RESET_STRUCT, rst_trigger_work);

	fgIsResetting = TRUE;
	/* Set the power off flag to FALSE in WMT to prevent chip power off
	 * after wlanProbe return failure, because we need to do core dump
	 * afterward.
	 */
	if (rst->rst_trigger_flag & RST_FLAG_PREVENT_POWER_OFF)
		mtk_wcn_set_connsys_power_off_flag(FALSE);

	fgResult = mtk_wcn_wmt_assert_timeout(WMTDRV_TYPE_WIFI, 0x40, 0);
	DBGLOG(INIT, INFO, "reset result %d, trigger flag 0x%x\n",
				fgResult, rst->rst_trigger_flag);
}
#endif
/* Weak reference for those platform doesn't support wmt functions */
int32_t __weak mtk_wcn_stp_coredump_start_get(void)
{
	return FALSE;
}


/*0= f/w assert flag is not set, others=f/w assert flag is set */
int32_t glIsWmtCodeDump(void)
{
	return mtk_wcn_stp_coredump_start_get();
}

static void triggerHifDumpIfNeed(void)
{
	struct GLUE_INFO *prGlueInfo;
	struct ADAPTER *prAdapter;

	if (fgIsResetting)
		return;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wlanGetWiphy());
	if (!prGlueInfo || !prGlueInfo->u4ReadyFlag || !prGlueInfo->prAdapter)
		return;

	prAdapter = prGlueInfo->prAdapter;
	prAdapter->u4HifDbgFlag |= DEG_HIF_DEFAULT_DUMP;
	kalSetHifDbgEvent(prAdapter->prGlueInfo);
	/* wait for hif_thread finish dump */
	kalMsleep(100);
}

#if (CFG_SUPPORT_CONNINFRA == 0)
static void dumpWlanThreadsIfNeed(void)
{
	struct GLUE_INFO *prGlueInfo;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wlanGetWiphy());
	if (!prGlueInfo || !prGlueInfo->u4ReadyFlag || !prGlueInfo->prAdapter)
		return;

	if (fgIsResetting)
		return;

	DBGLOG(INIT, INFO, "prGlueInfo->ulFlag: 0x%lx\n", prGlueInfo->ulFlag);

	if (prGlueInfo->main_thread) {
		DBGLOG(INIT, INFO, "Show backtrace of main_thread.\n");
		kal_show_stack(prGlueInfo->prAdapter, prGlueInfo->main_thread,
				NULL);
	}
	if (prGlueInfo->rx_thread) {
		DBGLOG(INIT, INFO, "Show backtrace of rx_thread.\n");
		kal_show_stack(prGlueInfo->prAdapter, prGlueInfo->rx_thread,
				NULL);
	}
	if (prGlueInfo->hif_thread) {
		DBGLOG(INIT, INFO, "Show backtrace of hif_thread.\n");
		kal_show_stack(prGlueInfo->prAdapter, prGlueInfo->hif_thread,
				NULL);
	}
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
static void glResetCallback(enum _ENUM_WMTDRV_TYPE_T eSrcType,
			     enum _ENUM_WMTDRV_TYPE_T eDstType,
			     enum _ENUM_WMTMSG_TYPE_T eMsgType, void *prMsgBody,
			     unsigned int u4MsgLength)
{
	switch (eMsgType) {
	case WMTMSG_TYPE_RESET:
		if (u4MsgLength == sizeof(enum _ENUM_WMTRSTMSG_TYPE_T)) {
			enum _ENUM_WMTRSTMSG_TYPE_T *prRstMsg =
				(enum _ENUM_WMTRSTMSG_TYPE_T *) prMsgBody;

			switch (*prRstMsg) {
			case WMTRSTMSG_RESET_START:
				DBGLOG(INIT, WARN, "Whole chip reset start!\n");
				dumpWlanThreadsIfNeed();
				triggerHifDumpIfNeed();
				fgIsResetting = TRUE;
				fgSimplifyResetFlow = TRUE;
				wifi_reset_start();
				break;

			case WMTRSTMSG_RESET_END:
				DBGLOG(INIT, WARN, "Whole chip reset end!\n");
				wifi_rst.rst_data = RESET_SUCCESS;
				fgIsResetting = FALSE;
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
}
#else

void glSetRstReasonString(char *reason)
{
	g_reason = reason;
}


static u_int8_t glResetMsgHandler(enum ENUM_WMTMSG_TYPE eMsgType,
				  enum ENUM_WMTRSTMSG_TYPE MsgBody)
{
	switch (eMsgType) {
	case WMTMSG_TYPE_RESET:

			switch (MsgBody) {
			case WMTRSTMSG_RESET_START:
				DBGLOG(INIT, WARN, "Whole chip reset start!\n");
				fgIsResetting = TRUE;
				fgSimplifyResetFlow = TRUE;
				wifi_reset_start();
				hifAxiRemove();
				complete(&g_RstOffComp);
				break;

			case WMTRSTMSG_RESET_END:
				DBGLOG(INIT, WARN, "WF reset end!\n");
				fgIsResetting = FALSE;
				wifi_rst.rst_data = RESET_SUCCESS;
				mtk_wifi_reset_main(&wifi_rst);
				break;

			case WMTRSTMSG_RESET_END_FAIL:
				DBGLOG(INIT, WARN, "Whole chip reset fail!\n");
				fgIsResetting = FALSE;
				wifi_rst.rst_data = RESET_FAIL;
				schedule_work(&(wifi_rst.rst_work));
				break;
			case WMTRSTMSG_0P5RESET_START:
				DBGLOG(INIT, WARN, "WF chip reset start!\n");
				fgIsResetting = TRUE;
				fgSimplifyResetFlow = TRUE;
				wifi_reset_start();
				hifAxiRemove();
				break;
			default:
				break;
			}
		break;

	default:
		break;
	}

	return TRUE;
}
bool glRstCheckRstCriteria(void)
{
	/*
	 * for those cases which need to trigger whole chip reset
	 * when fgIsResetting = TRUE
	 */
	if (g_IsSubsysRstOverThreshold || g_IsWfsysBusHang)
		return FALSE;
	else
		return TRUE;
}
void glRstWholeChipRstParamInit(void)
{
	g_IsSubsysRstOverThreshold = FALSE;
	g_SubsysRstCnt = 0;
	g_IsTriggerTimeout = FALSE;
	g_WholeChipRstTotalCnt++;
}
void glRstSetRstEndEvent(void)
{
	KAL_WAKE_LOCK(NULL, g_IntrWakeLock);

	set_bit(GLUE_FLAG_RST_END_BIT, &g_ulFlag);

	/* when we got interrupt, we wake up servie thread */
	wake_up_interruptible(&g_waitq_rst);

}

int glRstwlanPreWholeChipReset(enum consys_drv_type type, char *reason)
{
	bool bRet = 0;
	struct GLUE_INFO *prGlueInfo;
	struct ADAPTER *prAdapter = NULL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wlanGetWiphy());
	prAdapter = prGlueInfo->prAdapter;

	DBGLOG(INIT, INFO,
			"Enter glRstwlanPreWholeChipReset.\n");
	while (get_wifi_process_status() == 1) {
		DBGLOG(REQ, WARN,
			"Wi-Fi on/off process is ongoing, wait here.\n");
		msleep(100);
	}
	if (!get_wifi_powered_status()) {
		DBGLOG(REQ, WARN, "wifi driver is off now\n");
		return bRet;
	}

	triggerHifDumpIfNeed();

	g_WholeChipRstType = type;
	g_WholeChipRstReason = reason;

	if (glRstCheckRstCriteria()) {
		while (kalIsResetting()) {
			DBGLOG(REQ, WARN, "wifi driver is resetting\n");
			msleep(100);
		}
		while ((!prGlueInfo) ||
			(prGlueInfo->u4ReadyFlag == 0) ||
			(g_IsWfsysRstDone == FALSE)) {
			prGlueInfo =
				(struct GLUE_INFO *) wiphy_priv(wlanGetWiphy());
			DBGLOG(REQ, WARN, "wifi driver is not ready\n");
			if (g_IsWfsysResetOnFail == TRUE) {
				DBGLOG(REQ, WARN,
					"wifi driver reset fail, need whole chip reset.\n");
				g_IsWholeChipRst = TRUE;
				return bRet;
			}
			msleep(100);
		}
		g_IsWholeChipRst = TRUE;
		DBGLOG(INIT, INFO,
				"Wi-Fi Driver processes whole chip reset start.\n");
			GL_RESET_TRIGGER(prGlueInfo->prAdapter,
							 RST_FLAG_WF_RESET);
	} else {
		if (g_IsSubsysRstOverThreshold)
			DBGLOG(INIT, INFO, "Reach subsys reset threshold!!!\n");
		else if (g_IsWfsysBusHang)
			DBGLOG(INIT, INFO, "WFSYS bus hang!!!\n");
		g_IsWholeChipRst = TRUE;
#if (CFG_ANDORID_CONNINFRA_COREDUMP_SUPPORT == 1)
		if (!prAdapter->prGlueInfo->u4ReadyFlag)
			g_IsNeedWaitCoredump = TRUE;
#endif
		kalSetRstEvent();
	}
	wait_for_completion(&g_RstOffComp);
	DBGLOG(INIT, INFO, "Wi-Fi is off successfully.\n");

	return bRet;
}

int glRstwlanPostWholeChipReset(void)
{
	while (get_wifi_process_status() == 1) {
		DBGLOG(REQ, WARN,
			"Wi-Fi on/off process is ongoing, wait here.\n");
		msleep(100);
	}
	if (!get_wifi_powered_status()) {
		DBGLOG(REQ, WARN, "wifi driver is off now\n");
		return 0;
	}
	glRstSetRstEndEvent();
	DBGLOG(INIT, INFO, "Wait Wi-Fi state recover.\n");
	wait_for_completion(&g_RstOnComp);

	DBGLOG(INIT, INFO,
		"Leave glRstwlanPostWholeChipReset (%d).\n",
		g_IsWholeChipRst);
	return 0;
}
u_int8_t kalIsWholeChipResetting(void)
{
#if CFG_CHIP_RESET_SUPPORT
	return g_IsWholeChipRst;
#else
	return FALSE;
#endif
}
void glReset_timeinit(struct timeval *rNowTs, struct timeval *rLastTs)
{
	rNowTs->tv_sec = 0;
	rNowTs->tv_usec = 0;
	rLastTs->tv_sec = 0;
	rLastTs->tv_usec = 0;
}

bool IsOverRstTimeThreshold(struct timeval *rNowTs, struct timeval *rLastTs)
{
	struct timeval rTimeout, rTime = {0};
	bool fgIsTimeout = FALSE;

	rTimeout.tv_sec = 30;
	rTimeout.tv_usec = 0;
	do_gettimeofday(rNowTs);
	DBGLOG(INIT, INFO,
		"Reset happen time :%d.%d, last happen time :%d.%d\n",
		rNowTs->tv_sec,
		rNowTs->tv_usec,
		rLastTs->tv_sec,
		rLastTs->tv_usec);
	if (rLastTs->tv_sec != 0) {
		/* Ignore now time < token time */
		if (halTimeCompare(rNowTs, rLastTs) > 0) {
			rTime.tv_sec = rNowTs->tv_sec - rLastTs->tv_sec;
			rTime.tv_usec = rNowTs->tv_usec;
			if (rLastTs->tv_usec > rNowTs->tv_usec) {
				rTime.tv_sec -= 1;
				rTime.tv_usec += SEC_TO_USEC(1);
			}
			rTime.tv_usec -= rLastTs->tv_usec;
			if (halTimeCompare(&rTime, &rTimeout) >= 0)
				fgIsTimeout = TRUE;
			else
				fgIsTimeout = FALSE;
		}
		DBGLOG(INIT, INFO,
			"Reset rTimeout :%d.%d, calculate time :%d.%d\n",
			rTimeout.tv_sec,
			rTimeout.tv_usec,
			rTime.tv_sec,
			rTime.tv_usec);
	}
	return fgIsTimeout;
}
void glResetSubsysRstProcedure(
	struct ADAPTER *prAdapter,
	struct timeval *rNowTs,
	struct timeval *rLastTs)
{
	bool fgIsTimeout;
	struct mt66xx_chip_info *prChipInfo;
	struct WIFI_VAR *prWifiVar = NULL;
	struct GLUE_INFO *prGlueInfo = NULL;

	prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wlanGetWiphy());
	if (prGlueInfo && prGlueInfo->u4ReadyFlag) {
		prWifiVar = &prAdapter->rWifiVar;
		if (prWifiVar->fgRstRecover == 1)
			g_fgRstRecover = TRUE;
	}
#if 0
	if (prAdapter->chip_info->checkbushang)
		prAdapter->chip_info->checkbushang(FALSE);
#endif
	fgIsTimeout = IsOverRstTimeThreshold(rNowTs, rLastTs);
	if (g_IsWfsysBusHang == TRUE) {
		if (prGlueInfo && prGlueInfo->u4ReadyFlag) {
			/* dump host cr */
			if (prAdapter->chip_info->dumpBusHangCr)
				prAdapter->chip_info->dumpBusHangCr(prAdapter);
			glSetRstReasonString(
				"fw detect bus hang");
			prChipInfo = prAdapter->chip_info;
			if (prChipInfo->trigger_wholechiprst)
				prChipInfo->trigger_wholechiprst(g_reason);
			g_IsTriggerTimeout = FALSE;
		} else {
			DBGLOG(INIT, INFO,
				"Don't trigger whole chip reset due to driver is not ready\n");
		}
		return;
	}
	if (g_SubsysRstCnt > 3) {
		if (fgIsTimeout == TRUE) {
		/*
		 * g_SubsysRstCnt > 3, > 30 sec,
		 * need to update rLastTs, still do wfsys reset
		 */
#if (CFG_ANDORID_CONNINFRA_COREDUMP_SUPPORT == 1)
			if (eResetReason >= RST_REASON_MAX)
				eResetReason = 0;
			if (g_fgRstRecover == TRUE)
				g_fgRstRecover = FALSE;
			else {
				if (g_eWfRstSource == WF_RST_SOURCE_FW)
					fw_log_connsys_coredump_start(
						-1, NULL);
				else
					fw_log_connsys_coredump_start(
						CONNDRV_TYPE_WIFI,
						apucRstReason[eResetReason]);
			}
#endif
			if (prGlueInfo && prGlueInfo->u4ReadyFlag) {
				glResetMsgHandler(WMTMSG_TYPE_RESET,
						  WMTRSTMSG_0P5RESET_START);
				glResetMsgHandler(WMTMSG_TYPE_RESET,
						  WMTRSTMSG_RESET_END);
			} else {
				DBGLOG(INIT, INFO,
					"Don't trigger subsys reset due to driver is not ready\n");
			}
			g_SubsysRstTotalCnt++;
			g_SubsysRstCnt = 1;
		} else {
			/*g_SubsysRstCnt > 3, < 30 sec, do whole chip reset */
			g_IsSubsysRstOverThreshold = TRUE;
			/*coredump is done, no need do again*/
			g_IsTriggerTimeout = TRUE;
			glSetRstReasonString(
				"subsys reset more than 3 times");
			prChipInfo = prAdapter->chip_info;
			if (prChipInfo->trigger_wholechiprst)
				prChipInfo->trigger_wholechiprst(g_reason);
		}
	} else {
#if (CFG_ANDORID_CONNINFRA_COREDUMP_SUPPORT == 1)
		if (eResetReason >= RST_REASON_MAX)
			eResetReason = 0;
		if (g_fgRstRecover == TRUE)
			g_fgRstRecover = FALSE;
		else {
			if (g_eWfRstSource == WF_RST_SOURCE_FW)
				fw_log_connsys_coredump_start(-1, NULL);
			else
				fw_log_connsys_coredump_start(
					CONNDRV_TYPE_WIFI,
					apucRstReason[eResetReason]);
		}

#endif
		if (prGlueInfo && prGlueInfo->u4ReadyFlag) {
			glResetMsgHandler(WMTMSG_TYPE_RESET,
					  WMTRSTMSG_0P5RESET_START);
			glResetMsgHandler(WMTMSG_TYPE_RESET,
					  WMTRSTMSG_RESET_END);
		} else {
			DBGLOG(INIT, INFO,
				"Don't trigger subsys reset due to driver is not ready\n");
		}
		g_SubsysRstTotalCnt++;
		/*g_SubsysRstCnt < 3, but >30 sec,need to update rLastTs*/
		if (fgIsTimeout == TRUE)
			g_SubsysRstCnt = 1;
	}
	if (g_SubsysRstCnt == 1) {
		rLastTs->tv_sec = rNowTs->tv_sec;
		rLastTs->tv_usec = rNowTs->tv_usec;
	}
	g_IsTriggerTimeout = FALSE;
#if (CFG_ANDORID_CONNINFRA_COREDUMP_SUPPORT == 1)
	g_eWfRstSource = WF_RST_SOURCE_NONE;
#endif
}
int wlan_reset_thread_main(void *data)
{
	int ret = 0;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct timeval rNowTs, rLastTs;

#if defined(CONFIG_ANDROID) && (CFG_ENABLE_WAKE_LOCK)
	KAL_WAKE_LOCK_T *prWlanRstThreadWakeLock;

	KAL_WAKE_LOCK_INIT(NULL,
			   prWlanRstThreadWakeLock, "WLAN rst_thread");
	KAL_WAKE_LOCK(NULL, prWlanRstThreadWakeLock);
#endif

	glReset_timeinit(&rNowTs, &rLastTs);

	DBGLOG(INIT, INFO, "%s:%u starts running...\n",
	       KAL_GET_CURRENT_THREAD_NAME(), KAL_GET_CURRENT_THREAD_ID());

	g_u4WlanRstThreadPid = KAL_GET_CURRENT_THREAD_ID();

	while (TRUE) {
		/* Unlock wakelock if hif_thread going to idle */
		KAL_WAKE_UNLOCK(NULL, prWlanRstThreadWakeLock);
		/*
		 * sleep on waitqueue if no events occurred. Event contain
		 * (1) GLUE_FLAG_HALT (2) GLUE_FLAG_RST
		 *
		 */
		do {
			ret = wait_event_interruptible(g_waitq_rst,
				((g_ulFlag & GLUE_FLAG_RST_PROCESS)
				!= 0));
		} while (ret != 0);
#if defined(CONFIG_ANDROID) && (CFG_ENABLE_WAKE_LOCK)
		if (!KAL_WAKE_LOCK_ACTIVE(NULL,
					  prWlanRstThreadWakeLock))
			KAL_WAKE_LOCK(NULL,
				      prWlanRstThreadWakeLock);
#endif
		prGlueInfo = (struct GLUE_INFO *) wiphy_priv(wlanGetWiphy());
		if (test_and_clear_bit(GLUE_FLAG_RST_START_BIT, &g_ulFlag)) {
			if (KAL_WAKE_LOCK_ACTIVE(NULL, g_IntrWakeLock))
				KAL_WAKE_UNLOCK(NULL, g_IntrWakeLock);

			if (g_IsWholeChipRst) {
#if (CFG_ANDORID_CONNINFRA_COREDUMP_SUPPORT == 1)
				if (eResetReason >= RST_REASON_MAX)
					eResetReason = 0;
				fw_log_connsys_coredump_start(
					g_WholeChipRstType,
					g_WholeChipRstReason);
#endif
				if (prGlueInfo && prGlueInfo->u4ReadyFlag) {
					glResetMsgHandler(WMTMSG_TYPE_RESET,
						WMTRSTMSG_RESET_START);
					glRstWholeChipRstParamInit();
					glReset_timeinit(&rNowTs, &rLastTs);
				} else {
					DBGLOG(INIT, INFO,
						"Don't trigger whole chip reset due to driver is not ready\n");
				}
			} else {
				/*wfsys reset start*/
				g_IsWfsysRstDone = FALSE;
				g_SubsysRstCnt++;
				DBGLOG(INIT, INFO,
					"WF reset count = %d.\n",
					g_SubsysRstCnt);
				glResetSubsysRstProcedure(prGlueInfo->prAdapter,
							 &rNowTs,
							 &rLastTs);
				/*wfsys reset done*/
				g_IsWfsysRstDone = TRUE;
			}
#if (CFG_ANDORID_CONNINFRA_COREDUMP_SUPPORT == 1)
			g_IsNeedWaitCoredump = FALSE;
#endif
			DBGLOG(INIT, INFO,
			"Whole Chip rst count /WF reset total count = (%d)/(%d).\n",
				g_WholeChipRstTotalCnt,
				g_SubsysRstTotalCnt);
		}
		if (test_and_clear_bit(GLUE_FLAG_RST_END_BIT, &g_ulFlag)) {
			if (KAL_WAKE_LOCK_ACTIVE(NULL, g_IntrWakeLock))
				KAL_WAKE_UNLOCK(NULL, g_IntrWakeLock);
			DBGLOG(INIT, INFO, "Whole chip reset end start\n");
			glResetMsgHandler(WMTMSG_TYPE_RESET,
				WMTRSTMSG_RESET_END);
		}
		if (test_and_clear_bit(GLUE_FLAG_HALT_BIT, &g_ulFlag)) {
			DBGLOG(INIT, INFO, "rst_thread should stop now...\n");
			break;
		}
	}

#if defined(CONFIG_ANDROID) && (CFG_ENABLE_WAKE_LOCK)
	if (KAL_WAKE_LOCK_ACTIVE(NULL,
				 prWlanRstThreadWakeLock))
		KAL_WAKE_UNLOCK(NULL, prWlanRstThreadWakeLock);
	KAL_WAKE_LOCK_DESTROY(NULL,
			      prWlanRstThreadWakeLock);
#endif

	DBGLOG(INIT, TRACE, "%s:%u stopped!\n",
	       KAL_GET_CURRENT_THREAD_NAME(), KAL_GET_CURRENT_THREAD_ID());

	return 0;
}
#endif
#else
static u_int8_t is_bt_exist(void)
{
	typedef int (*p_bt_fun_type) (int);
	p_bt_fun_type bt_func;
	char *bt_func_name = "WF_rst_L0_notify_BT_step1";

	bt_func = (p_bt_fun_type) kallsyms_lookup_name(bt_func_name);
	if (bt_func)
		return TRUE;

	DBGLOG(INIT, ERROR, "[SER][L0] %s does not exist\n", bt_func_name);
	return FALSE;

}

static u_int8_t rst_L0_notify_step1(void)
{
	if (eResetReason != RST_BT_TRIGGER) {
		typedef int (*p_bt_fun_type) (int);
		p_bt_fun_type bt_func;
		char *bt_func_name = "WF_rst_L0_notify_BT_step1";

		DBGLOG(INIT, STATE, "[SER][L0] %s\n", bt_func_name);
		bt_func = (p_bt_fun_type) kallsyms_lookup_name(bt_func_name);
		if (bt_func) {
			bt_func(0);
		} else {
			DBGLOG(INIT, ERROR,
				"[SER][L0] %s does not exist\n", bt_func_name);
			return FALSE;
		}
	}

	return TRUE;
}

static void wait_core_dump_end(void)
{
#ifdef CFG_SUPPORT_CONNAC2X
	if (eResetReason == RST_OID_TIMEOUT)
		return;
	DBGLOG(INIT, WARN, "[SER][L0] not support..\n");
#endif
}

int32_t BT_rst_L0_notify_WF_step1(int32_t reserved)
{
	glSetRstReason(RST_BT_TRIGGER);
	GL_RESET_TRIGGER(NULL, RST_FLAG_CHIP_RESET);

	return 0;
}
EXPORT_SYMBOL(BT_rst_L0_notify_WF_step1);

int32_t BT_rst_L0_notify_WF_2(int32_t reserved)
{
	DBGLOG(INIT, WARN, "[SER][L0] not support...\n");

	return 0;
}
EXPORT_SYMBOL(BT_rst_L0_notify_WF_2);

#endif
#endif


