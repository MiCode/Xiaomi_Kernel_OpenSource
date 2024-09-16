/******************************************************************************
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
 *****************************************************************************/
/******************************************************************************
*[File]             sdio.c
*[Version]          v1.0
*[Revision Date]    2010-03-01
*[Author]
*[Description]
*    The program provides SDIO HIF driver
*[Copyright]
*    Copyright (C) 2010 MediaTek Incorporation. All Rights Reserved.
******************************************************************************/


/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

#include "gl_os.h"
#include "precomp.h"

#if MTK_WCN_HIF_SDIO
#include "hif_sdio.h"
#else
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sdio.h>
#include <linux/mmc/sdio_func.h>	/* sdio_readl(), etc */
#include <linux/mmc/host.h>		/* mmc_add_host(), etc */
#include <linux/mmc/sdio_ids.h>
#endif

#include <linux/mm.h>
#ifndef CONFIG_X86
#include <asm/memory.h>
#endif

#include "mt66xx_reg.h"

#if CFG_SUPPORT_WOW_EINT
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif

#if (CFG_SDIO_1BIT_DATA_MODE == 1)
#include "test_driver_sdio_ops.h"
#endif

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

#define HIF_SDIO_ERR_TITLE_STR              "["CHIP_NAME"] SDIO Access Error!"
#define HIF_SDIO_ERR_DESC_STR               "**SDIO Access Error**\n"

#define HIF_SDIO_ACCESS_RETRY_LIMIT         250
#define HIF_SDIO_INTERRUPT_RESPONSE_TIMEOUT (15000)

#if CFG_SUPPORT_WOW_EINT
#define WIFI_COMPATIBLE_NODE_NAME	"mediatek,mt7663_wifi_ctrl"
#define WIFI_INTERRUPT_NAME		"mt7663_wifi_ctrl-eint"
#endif

#if MTK_WCN_HIF_SDIO

/*
 * function prototypes
 *
 */

static int32_t mtk_sdio_probe(unsigned long, const struct MTK_WCN_HIF_SDIO_FUNCINFO *);

static int32_t mtk_sdio_remove(unsigned long);
static int32_t mtk_sdio_interrupt(unsigned long);

/*
 * sdio function info table
 */

static struct MTK_WCN_HIF_SDIO_FUNCINFO funcInfo[] = {
	{MTK_WCN_HIF_SDIO_FUNC(0x037a, 0x6602, 0x1, 512)},
};

static struct MTK_WCN_SDIO_DRIVER_DATA_MAPPING sdio_driver_data_mapping[] = {
	{0x6602, &mt66xx_driver_data_mt6632},
};

static struct MTK_WCN_HIF_SDIO_CLTINFO cltInfo = {
	.func_tbl = funcInfo,
	.func_tbl_size = sizeof(funcInfo) / sizeof(struct MTK_WCN_HIF_SDIO_FUNCINFO),
	.hif_clt_probe = mtk_sdio_probe,
	.hif_clt_remove = mtk_sdio_remove,
	.hif_clt_irq = mtk_sdio_interrupt,
};

#else
/*
 * function prototypes
 *
 */
static int mtk_sdio_pm_suspend(struct device *pDev);
static int mtk_sdio_pm_resume(struct device *pDev);

static const struct sdio_device_id mtk_sdio_ids[] = {
#ifdef MT6632
	{	SDIO_DEVICE(0x037a, 0x6602),
		.driver_data = (kernel_ulong_t)&mt66xx_driver_data_mt6632},/* Not an SDIO standard class device */
#endif /* MT6632 */
#ifdef MT7668
	{	SDIO_DEVICE(0x037a, 0x7608),
		.driver_data = (kernel_ulong_t)&mt66xx_driver_data_mt7668},/* Not an SDIO standard class device */
#endif /* MT7668 */
#ifdef MT7663
	{	SDIO_DEVICE(0x037a, 0x7603),
		.driver_data = (kernel_ulong_t)&mt66xx_driver_data_mt7663},
#endif /* MT7663 */
	{ /* end: all zeroes */ },
};

MODULE_DEVICE_TABLE(sdio, mtk_sdio_ids);

#endif

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/
static probe_card pfWlanProbe;
static remove_card pfWlanRemove;

#if (MTK_WCN_HIF_SDIO == 0)
static const struct dev_pm_ops mtk_sdio_pm_ops = {
	.suspend = mtk_sdio_pm_suspend,
	.resume = mtk_sdio_pm_resume,
};

static struct sdio_driver mtk_sdio_driver = {
	.name = "wlan",		/* "MTK SDIO WLAN Driver" */
	.id_table = mtk_sdio_ids,
	.probe = NULL,
	.remove = NULL,
	.drv = {
		.owner = THIS_MODULE,
		.pm = &mtk_sdio_pm_ops,
	}
};
#endif

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/
#define dev_to_sdio_func(d)	container_of(d, struct sdio_func, dev)


/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
/*----------------------------------------------------------------------------*/
/*!
* \brief This function is a SDIO interrupt callback function
*
* \param[in] func  pointer to SDIO handle
*
* \return void
*/
/*----------------------------------------------------------------------------*/
#if CFG_SUPPORT_WOW_EINT
static irqreturn_t wifi_wow_isr(int irq, void *dev)
{
	return IRQ_HANDLED;
}

static void wlan_register_irq(struct WOWLAN_DEV_NODE *node)
{
	struct device_node *eint_node = NULL;
	int interrupts[2];

	eint_node = of_find_compatible_node(NULL,
			NULL, WIFI_COMPATIBLE_NODE_NAME);
	if (eint_node) {
		node->wowlan_irq = irq_of_parse_and_map(eint_node, 0);
		DBGLOG(INIT, INFO, "%s, WOWLAN irq_number = %d\n", __func__,
			node->wowlan_irq);
		if (node->wowlan_irq) {
			of_property_read_u32_array(eint_node,
				"interrupts",
				interrupts,
				ARRAY_SIZE(interrupts));
			node->wowlan_irqlevel = interrupts[1];
			if (request_irq(node->wowlan_irq,
				wifi_wow_isr,
				node->wowlan_irqlevel,
				WIFI_INTERRUPT_NAME,
				node)) {
				DBGLOG(INIT, ERROR,
					"%s, WOWLAN irq NOT AVAILABLE!\n",
					__func__);
			} else {
				disable_irq_nosync(node->wowlan_irq);
			}
		} else {
			DBGLOG(INIT, ERROR,
				"%s, can't find wifi_ctrl irq\n",
				__func__);
		}

	} else {
		node->wowlan_irq = 0;
		DBGLOG(INIT, ERROR,
			"%s, can't find wifi_ctrl compatible node\n",
			__func__);
	}
}

static void mtk_sdio_eint_interrupt(struct sdio_func *func)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	prGlueInfo = sdio_get_drvdata(func);
	if (!prGlueInfo)
		return;

	prGlueInfo->prAdapter->rWowlanDevNode.func = func;
	wlan_register_irq(&(prGlueInfo->prAdapter->rWowlanDevNode));
}

static void mtk_sdio_eint_free_irq(struct sdio_func *func)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	uint32_t u4Irq = 0;

	prGlueInfo = sdio_get_drvdata(func);
	if (!prGlueInfo)
		return;

	u4Irq = prGlueInfo->prAdapter->rWowlanDevNode.wowlan_irq;
	if (u4Irq) {
		disable_irq_nosync(u4Irq);
		free_irq(u4Irq,
			&prGlueInfo->prAdapter->rWowlanDevNode);
	}
}

#endif

#if MTK_WCN_HIF_SDIO

static int32_t mtk_sdio_interrupt(unsigned long cltCtx)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	int32_t ret = 0;

	prGlueInfo = mtk_wcn_hif_sdio_get_drvdata(cltCtx);

	/* ASSERT(prGlueInfo); */

	if (!prGlueInfo) {
		return -HIF_SDIO_ERR_FAIL;
	}

	prGlueInfo->u8HifIntTime = sched_clock();

	prGlueInfo->IsrCnt++;

	if (prGlueInfo->ulFlag & GLUE_FLAG_HALT) {
		ret = mtk_wcn_hif_sdio_writeb(cltCtx, MCR_WHLPCR, WHLPCR_INT_EN_CLR);
		return ret;
	}

	ret = mtk_wcn_hif_sdio_writeb(cltCtx, MCR_WHLPCR, WHLPCR_INT_EN_CLR);

	prGlueInfo->rHifInfo.fgIsPendingInt = FALSE;

	kalSetIntEvent(prGlueInfo);
	prGlueInfo->IsrPassCnt++;

	return ret;
}

#else
static void mtk_sdio_interrupt(struct sdio_func *func)
{
	struct GLUE_INFO *prGlueInfo = NULL;

	int ret = 0;

	prGlueInfo = sdio_get_drvdata(func);
	/* ASSERT(prGlueInfo); */

	if (!prGlueInfo) {
		return;
	}

	if (prGlueInfo->ulFlag & GLUE_FLAG_HALT) {
		sdio_writeb(prGlueInfo->rHifInfo.func, WHLPCR_INT_EN_CLR, MCR_WHLPCR, &ret);
		return;
	}

	sdio_writeb(prGlueInfo->rHifInfo.func, WHLPCR_INT_EN_CLR, MCR_WHLPCR, &ret);

	kalSetIntEvent(prGlueInfo);
}
#endif

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is a SDIO probe function
*
* \param[in] func   pointer to SDIO handle
* \param[in] id     pointer to SDIO device id table
*
* \return void
*/
/*----------------------------------------------------------------------------*/

#if MTK_WCN_HIF_SDIO

/* FIXME: global variable */
static const struct MTK_WCN_HIF_SDIO_FUNCINFO *prFunc;

static int32_t mtk_sdio_probe(unsigned long cltCtx, const struct MTK_WCN_HIF_SDIO_FUNCINFO *prFuncInfo)
{
	int32_t ret = HIF_SDIO_ERR_SUCCESS;
	int32_t i = 0;
	int32_t dd_table_len = sizeof(sdio_driver_data_mapping) / sizeof(struct MTK_WCN_SDIO_DRIVER_DATA_MAPPING);
	struct mt66xx_hif_driver_data *sdio_driver_data = NULL;

	prFunc = prFuncInfo;

	for (i = 0; i < dd_table_len; i++) {
		if (prFunc->card_id == sdio_driver_data_mapping[i].card_id) {
			sdio_driver_data = sdio_driver_data_mapping[i].mt66xx_driver_data;
			break;
		}
	}

	if (sdio_driver_data == NULL) {
		DBGLOG(HAL, ERROR, "sdio probe error: %x driver data not found!\n", prFunc->card_id);
		return HIF_SDIO_ERR_UNSUP_CARD_ID;
	}

	if (pfWlanProbe((void *)&cltCtx, (void *) sdio_driver_data) != WLAN_STATUS_SUCCESS) {
		pfWlanRemove();
		ret = -(HIF_SDIO_ERR_FAIL);
	} else {

	}
	return ret;
}
#else
static int mtk_sdio_probe(struct sdio_func *func, const struct sdio_device_id *id)
{
	int ret = 0;
	/* int i = 0; */



	ASSERT(func);
	ASSERT(id);

	sdio_claim_host(func);
	ret = sdio_enable_func(func);
	sdio_release_host(func);

	if (ret) {
		goto out;
	}

	if (pfWlanProbe((void *) func, (void *) id->driver_data) != WLAN_STATUS_SUCCESS) {
		pfWlanRemove();
		ret = -1;
	}

out:
	return ret;
}
#endif

#if MTK_WCN_HIF_SDIO
static int32_t mtk_sdio_remove(unsigned long cltCtx)
{
	int32_t ret = HIF_SDIO_ERR_SUCCESS;

	pfWlanRemove();

	return ret;
}
#else
static void mtk_sdio_remove(struct sdio_func *func)
{

	ASSERT(func);

	pfWlanRemove();

	sdio_claim_host(func);
	sdio_disable_func(func);
	sdio_release_host(func);

}
#endif

#if (MTK_WCN_HIF_SDIO == 0)
static int mtk_sdio_pm_suspend(struct device *pDev)
{
	int ret = 0, wait = 0;
	int pm_caps, set_flag;
	const char *func_id;
	struct sdio_func *func;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	uint8_t count = 0;
	uint8_t drv_own_fail = FALSE;

	DBGLOG(HAL, STATE, "mtk_sdio_pm_suspend\n");

	if (kalIsResetting()) {
		DBGLOG(HAL, WARN, "Chip resetting, skip\n");
		return -1;
	}

	func = dev_to_sdio_func(pDev);
	prGlueInfo = sdio_get_drvdata(func);
	prAdapter = prGlueInfo->prAdapter;

	/* Stop upper layers calling the device hard_start_xmit routine. */
	netif_tx_stop_all_queues(prGlueInfo->prDevHandler);
	wlanWaitCfg80211SuspendDone(prGlueInfo);

	/* change to non-READY state to block cfg80211 ops */
	glSdioSetState(&prGlueInfo->rHifInfo, SDIO_STATE_PRE_SUSPEND_START);

#if CFG_ENABLE_WAKE_LOCK
	/* AIS flow: disassociation if wow_en=0 */
	/* cancel scan report done event */
	aisPreSuspendFlow(prGlueInfo);

	/* In current design, only support AIS connection during suspend only.
	 * It need to add flow to deactive P2P (GC/GO) link during suspend flow.
	 * Otherwise, MT7668 would fail to enter deep sleep.
	 */
	p2pProcessPreSuspendFlow(prGlueInfo->prAdapter);
#endif
	wlan_fb_power_down = TRUE;
	if (!wlan_perf_monitor_force_enable)
		kalPerMonDisable(prGlueInfo);

	wlanSuspendPmHandle(prGlueInfo);

	/* send pre-suspend cmd to notify FW do not send pkt/event to host */
	halPreSuspendCmd(prAdapter);

	while (prGlueInfo->rHifInfo.state != SDIO_STATE_PRE_SUSPEND_DONE) {
		if (count > 500) {
			DBGLOG(HAL, ERROR, "pre_suspend timeout\n");
			break;
		}
		usleep_range(2000, 3000);
		count++;
	}

	halDisableInterrupt(prAdapter);

	prAdapter->fgForceFwOwn = TRUE;

#if CFG_SUPPORT_WOW_EINT
	if (prAdapter->rWowlanDevNode.wowlan_irq != 0 &&
		atomic_read(
		&(prAdapter->rWowlanDevNode.irq_enable_count)) == 0) {
		DBGLOG(HAL, ERROR, "%s:enable WIFI IRQ:%d\n", __func__,
			prAdapter->rWowlanDevNode.wowlan_irq);
		enable_irq(prAdapter->rWowlanDevNode.wowlan_irq);
		enable_irq_wake(prAdapter->rWowlanDevNode.wowlan_irq);
		atomic_inc(&(prAdapter->rWowlanDevNode.irq_enable_count));
	} else {
		DBGLOG(HAL, ERROR, "%s:irq_enable count:%d\n", __func__,
			atomic_read(
			&(prAdapter->rWowlanDevNode.irq_enable_count)));
	}
#endif

	/* Wait for
	*  1. The other unfinished ownership handshakes
	*  2. FW own back
	*/
	while (wait < 500) {
		if ((prAdapter->u4PwrCtrlBlockCnt == 0) &&
		    (prAdapter->fgIsFwOwn == TRUE) &&
		    (drv_own_fail == FALSE)) {
			DBGLOG(HAL, STATE, "************************\n");
			DBGLOG(HAL, STATE, "* Entered SDIO Suspend *\n");
			DBGLOG(HAL, STATE, "************************\n");
			DBGLOG(HAL, INFO, "wait = %d\n\n", wait);
			break;
		}

		ACQUIRE_POWER_CONTROL_FROM_PM(prAdapter);
		/* Prevent that suspend without FW Own:
		 * Set Drv own has failed, and then Set FW Own is skipped
		 */
		if (prAdapter->fgIsFwOwn == FALSE)
			drv_own_fail = FALSE;
		else
			drv_own_fail = TRUE;
		/* For single core CPU, let hif_thread can be completed */
		usleep_range(1000, 3000);
		RECLAIM_POWER_CONTROL_TO_PM(prAdapter, FALSE);

		wait++;
	}

	if (wait >= 500) {
		DBGLOG(HAL, ERROR, "Set FW Own Timeout !!\n");
		return -EAGAIN;
	}

	pm_caps = sdio_get_host_pm_caps(func);
	func_id = sdio_func_id(func);

	/* Ask kernel keeping SDIO bus power-on */
	set_flag = MMC_PM_KEEP_POWER;
	ret = sdio_set_host_pm_flags(func, set_flag);
	if (ret) {
		DBGLOG(HAL, ERROR, "set flag %d err %d\n", set_flag, ret);
		DBGLOG(HAL, ERROR,
			"%s: cannot remain alive(0x%X)\n", func_id, pm_caps);
		return -ENOSYS;
	}

	/* If wow enable, ask kernel accept SDIO IRQ in suspend mode */
	if (prGlueInfo->prAdapter->rWifiVar.ucWow &&
		prGlueInfo->prAdapter->rWowCtrl.fgWowEnable) {
		set_flag = MMC_PM_WAKE_SDIO_IRQ;
		ret = sdio_set_host_pm_flags(func, set_flag);
		if (ret) {
			DBGLOG(HAL, ERROR, "set flag %d err %d\n", set_flag, ret);
			DBGLOG(HAL, ERROR,
				"%s: cannot sdio wake-irq(0x%X)\n", func_id, pm_caps);
		}
	}

	glSdioSetState(&prGlueInfo->rHifInfo, SDIO_STATE_SUSPEND);
	wlanReleaseAllTxCmdQueue(prGlueInfo->prAdapter);

	DBGLOG(HAL, STATE, "<==\n");
	return 0;
}

static int mtk_sdio_pm_resume(struct device *pDev)
{
	struct sdio_func *func;
	struct GLUE_INFO *prGlueInfo = NULL;
	uint8_t count = 0;
#if CFG_SUPPORT_WOW_EINT
	struct ADAPTER *prAdapter = NULL;
#endif

	DBGLOG(HAL, STATE, "==>\n");

	if (kalIsResetting()) {
		DBGLOG(HAL, WARN, "Chip resetting, skip\n");
		return -1;
	}

	func = dev_to_sdio_func(pDev);
	prGlueInfo = sdio_get_drvdata(func);

	halEnableInterrupt(prGlueInfo->prAdapter);

#if CFG_SUPPORT_WOW_EINT
	prAdapter = prGlueInfo->prAdapter;

	if (prAdapter->rWowlanDevNode.wowlan_irq != 0 &&
		atomic_read(
		&(prAdapter->rWowlanDevNode.irq_enable_count)) == 1) {
		DBGLOG(HAL, ERROR, "%s:disable WIFI IRQ:%d\n", __func__,
			prAdapter->rWowlanDevNode.wowlan_irq);
		atomic_dec(&(prAdapter->rWowlanDevNode.irq_enable_count));
		disable_irq_wake(prAdapter->rWowlanDevNode.wowlan_irq);
		disable_irq(prAdapter->rWowlanDevNode.wowlan_irq);
	} else {
		DBGLOG(HAL, ERROR, "%s:irq_enable count:%d\n", __func__,
			atomic_read(
			&(prAdapter->rWowlanDevNode.irq_enable_count)));
	}
#endif

	glSdioSetState(&prGlueInfo->rHifInfo, SDIO_STATE_PRE_RESUME);

	prGlueInfo->prAdapter->fgForceFwOwn = FALSE;

	halPreResumeCmd(prGlueInfo->prAdapter);
	kalPerMonEnable(prGlueInfo);
	wlan_fb_power_down = FALSE;

	while (prGlueInfo->rHifInfo.state != SDIO_STATE_LINK_UP) {
		if (count > 500) {
			DBGLOG(HAL, ERROR, "pre_resume timeout\n");
			break;
		}

		kalUdelay(2000);
		schedule();
		count++;
	}

	wlanResumePmHandle(prGlueInfo);

	/* change to READY state to allow cfg80211 ops */
	glSdioSetState(&prGlueInfo->rHifInfo, SDIO_STATE_READY);

	/* Allow upper layers to call the device hard_start_xmit routine. */
	netif_tx_start_all_queues(prGlueInfo->prDevHandler);

	DBGLOG(HAL, STATE, "<==\n");

	return 0;
}

#if (CFG_SDIO_ASYNC_IRQ_AUTO_ENABLE == 1)
int mtk_sdio_async_irq_enable(struct sdio_func *func)
{
#define SDIO_CCCR_IRQ_EXT	0x16
#define SDIO_IRQ_EXT_SAI	BIT(0)
#define SDIO_IRQ_EXT_EAI	BIT(1)
	unsigned char data = 0;
	unsigned int quirks_bak;
	int ret;

	/* Read CCCR 0x16 (interrupt extension)*/
	data = sdio_f0_readb(func, SDIO_CCCR_IRQ_EXT, &ret);
	if (ret) {
		DBGLOG(HAL, ERROR, "CCCR 0x%X read fail (%d).\n", SDIO_CCCR_IRQ_EXT, ret);
		return FALSE;
	}
	/* Check CCCR capability status */
	if (!(data & SDIO_IRQ_EXT_SAI)) {
		/* SAI = 0 */
		DBGLOG(HAL, ERROR, "No Async-IRQ capability.\n");
		return FALSE;
	} else if (data & SDIO_IRQ_EXT_EAI) {
		/* EAI = 1 */
		DBGLOG(INIT, INFO, "Async-IRQ enabled already.\n");
		return TRUE;
	}

	/* Set EAI bit */
	data |= SDIO_IRQ_EXT_EAI;

	/* Enable capability to write CCCR */
	quirks_bak = func->card->quirks;
	func->card->quirks |= MMC_QUIRK_LENIENT_FN0;
	/* Write CCCR into card */
	sdio_f0_writeb(func, data, SDIO_CCCR_IRQ_EXT, &ret);
	if (ret) {
		DBGLOG(HAL, ERROR, "CCCR 0x%X write fail (%d).\n", SDIO_CCCR_IRQ_EXT, ret);
		return FALSE;
	}
	func->card->quirks = quirks_bak;

	data = sdio_f0_readb(func, SDIO_CCCR_IRQ_EXT, &ret);
	if (ret || !(data & SDIO_IRQ_EXT_EAI)) {
		DBGLOG(HAL, ERROR, "CCCR 0x%X write fail (%d).\n", SDIO_CCCR_IRQ_EXT, ret);
		return FALSE;
	}
	return TRUE;
}
#endif
#endif

/*----------------------------------------------------------------------------*/
/*!
* \brief This function will register sdio bus to the os
*
* \param[in] pfProbe    Function pointer to detect card
* \param[in] pfRemove   Function pointer to remove card
*
* \return The result of registering sdio bus
*/
/*----------------------------------------------------------------------------*/
uint32_t glRegisterBus(probe_card pfProbe, remove_card pfRemove)
{
	int ret = 0;

	ASSERT(pfProbe);
	ASSERT(pfRemove);


	pfWlanProbe = pfProbe;
	pfWlanRemove = pfRemove;

#if MTK_WCN_HIF_SDIO
	/* register MTK sdio client */
	ret =
	    ((mtk_wcn_hif_sdio_client_reg(&cltInfo) ==
	      HIF_SDIO_ERR_SUCCESS) ? WLAN_STATUS_SUCCESS : WLAN_STATUS_FAILURE);
#else
	mtk_sdio_driver.probe = mtk_sdio_probe;
	mtk_sdio_driver.remove = mtk_sdio_remove;

	ret = (sdio_register_driver(&mtk_sdio_driver) == 0) ? WLAN_STATUS_SUCCESS : WLAN_STATUS_FAILURE;
#endif

	return ret;
}				/* end of glRegisterBus() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This function will unregister sdio bus to the os
*
* \param[in] pfRemove   Function pointer to remove card
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
void glUnregisterBus(remove_card pfRemove)
{
	ASSERT(pfRemove);
	pfRemove();

#if MTK_WCN_HIF_SDIO
	/* unregister MTK sdio client */
	mtk_wcn_hif_sdio_client_unreg(&cltInfo);
#else
	sdio_unregister_driver(&mtk_sdio_driver);
#endif
}				/* end of glUnregisterBus() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This function stores hif related info, which is initialized before.
*
* \param[in] prGlueInfo Pointer to glue info structure
* \param[in] u4Cookie   Pointer to UINT_32 memory base variable for _HIF_HPI
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
void glSetHifInfo(struct GLUE_INFO *prGlueInfo, unsigned long ulCookie)
{
	struct GL_HIF_INFO *prHif = NULL;
	uint8_t ucIdx;

	prHif = &prGlueInfo->rHifInfo;

	QUEUE_INITIALIZE(&prHif->rFreeQueue);
	QUEUE_INITIALIZE(&prHif->rRxDeAggQueue);
	QUEUE_INITIALIZE(&prHif->rRxFreeBufQueue);

#if MTK_WCN_HIF_SDIO
	/* prHif->prFuncInfo = ((MTK_WCN_HIF_SDIO_FUNCINFO *) u4Cookie); */
	prHif->prFuncInfo = prFunc;
	prHif->cltCtx = *((unsigned long *) ulCookie);
	mtk_wcn_hif_sdio_set_drvdata(prHif->cltCtx, prGlueInfo);

#else
	prHif->func = (struct sdio_func *)ulCookie;

	sdio_set_drvdata(prHif->func, prGlueInfo);

	SET_NETDEV_DEV(prGlueInfo->prDevHandler, &prHif->func->dev);
#endif

	/* Reset statistic counter */
	kalMemZero(&prHif->rStatCounter, sizeof(struct SDIO_STAT_COUNTER));

	for (ucIdx = HIF_TXC_IDX_0; ucIdx < HIF_TXC_IDX_NUM; ucIdx++)
		prHif->au4PendingTxDoneCount[ucIdx] = 0;

	spin_lock_init(&prHif->rStateLock);
	spin_lock_init(&prHif->rSuspendLock);

	mutex_init(&prHif->rRxFreeBufQueMutex);
	mutex_init(&prHif->rRxDeAggQueMutex);

	prHif->fgSkipRx = FALSE;
	glSdioSetState(prHif, SDIO_STATE_READY);
	prGlueInfo->u4InfType = MT_DEV_INF_SDIO;
}				/* end of glSetHifInfo() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This function clears hif related info.
*
* \param[in] prGlueInfo Pointer to glue info structure
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
void glClearHifInfo(struct GLUE_INFO *prGlueInfo)
{
	/* P_GL_HIF_INFO_T prHif = NULL; */
	/* ASSERT(prGlueInfo); */
	/* prHif = &prGlueInfo->rHifInfo; */
}				/* end of glClearHifInfo() */

/*----------------------------------------------------------------------------*/
/*!
* \brief Initialize bus operation and hif related information, request resources.
*
* \param[out] pvData    A pointer to HIF-specific data type buffer.
*                       For eHPI, pvData is a pointer to UINT_32 type and stores a
*                       mapped base address.
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
u_int8_t glBusInit(void *pvData)
{
#if (MTK_WCN_HIF_SDIO == 0)
	int ret = 0;
	struct sdio_func *func = NULL;

	ASSERT(pvData);

	func = (struct sdio_func *)pvData;

	sdio_claim_host(func);

#if (CFG_SDIO_1BIT_DATA_MODE == 1)
	ret = sdio_disable_wide(func->card);
	if (ret)
		DBGLOG(HAL, ERROR, "glBusInit() Error at enabling SDIO 1-BIT data mode.\n");
	else
		DBGLOG(HAL, INFO, "glBusInit() SDIO 1-BIT data mode is working.\n");
#endif

#if (CFG_SDIO_ASYNC_IRQ_AUTO_ENABLE == 1)
	ret = mtk_sdio_async_irq_enable(func);
	if (ret == FALSE)
		DBGLOG(HAL, ERROR, "Async-IRQ auto-enable fail.\n");
	else
		DBGLOG(INIT, INFO, "Async-IRQ is enabled.\n");
#endif

	ret = sdio_set_block_size(func, 512);
	sdio_release_host(func);

#endif
	return TRUE;
}				/* end of glBusInit() */

/*----------------------------------------------------------------------------*/
/*!
* \brief Stop bus operation and release resources.
*
* \param[in] pvData A pointer to struct net_device.
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
void glBusRelease(void *pvData)
{
}				/* end of glBusRelease() */

/*----------------------------------------------------------------------------*/
/*!
* \brief Setup bus interrupt operation and interrupt handler for os.
*
* \param[in] pvData     A pointer to struct net_device.
* \param[in] pfnIsr     A pointer to interrupt handler function.
* \param[in] pvCookie   Private data for pfnIsr function.
*
* \retval WLAN_STATUS_SUCCESS   if success
*         NEGATIVE_VALUE   if fail
*/
/*----------------------------------------------------------------------------*/
int32_t glBusSetIrq(void *pvData, void *pfnIsr, void *pvCookie)
{
	int ret = 0;

	struct net_device *prNetDevice = NULL;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct GL_HIF_INFO *prHifInfo = NULL;

	ASSERT(pvData);
	if (!pvData)
		return -1;

	prNetDevice = (struct net_device *)pvData;
	prGlueInfo = (struct GLUE_INFO *) pvCookie;
	ASSERT(prGlueInfo);
	if (!prGlueInfo)
		return -1;

	prHifInfo = &prGlueInfo->rHifInfo;

#if (MTK_WCN_HIF_SDIO == 0)
	sdio_claim_host(prHifInfo->func);
	ret = sdio_claim_irq(prHifInfo->func, mtk_sdio_interrupt);
	sdio_release_host(prHifInfo->func);
#else
	mtk_wcn_hif_sdio_enable_irq(prHifInfo->cltCtx, TRUE);
#endif

#if CFG_SUPPORT_WOW_EINT
	mtk_sdio_eint_interrupt(prHifInfo->func);
#endif

	prHifInfo->fgIsPendingInt = FALSE;

	prHifInfo->u4IntLogIdx = 0;
	prHifInfo->ucIntLogEntry = 0;

	return ret;
}				/* end of glBusSetIrq() */

/*----------------------------------------------------------------------------*/
/*!
* \brief Stop bus interrupt operation and disable interrupt handling for os.
*
* \param[in] pvData     A pointer to struct net_device.
* \param[in] pvCookie   Private data for pfnIsr function.
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
void glBusFreeIrq(void *pvData, void *pvCookie)
{
	struct net_device *prNetDevice = NULL;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct GL_HIF_INFO *prHifInfo = NULL;

	ASSERT(pvData);
	if (!pvData) {
		return;
	}
	prNetDevice = (struct net_device *)pvData;
	prGlueInfo = (struct GLUE_INFO *) pvCookie;
	ASSERT(prGlueInfo);
	if (!prGlueInfo) {
		return;
	}

	prHifInfo = &prGlueInfo->rHifInfo;
#if (MTK_WCN_HIF_SDIO == 0)
	sdio_claim_host(prHifInfo->func);
	sdio_release_irq(prHifInfo->func);
	sdio_release_host(prHifInfo->func);
#else
	mtk_wcn_hif_sdio_enable_irq(prHifInfo->cltCtx, FALSE);
#endif

#if CFG_SUPPORT_WOW_EINT
	mtk_sdio_eint_free_irq(prHifInfo->func);
#endif

}				/* end of glBusreeIrq() */

u_int8_t glIsReadClearReg(uint32_t u4Address)
{
	switch (u4Address) {
	case MCR_WHISR:
	case MCR_WASR:
	case MCR_D2HRM0R:
	case MCR_D2HRM1R:
	case MCR_WTQCR0:
	case MCR_WTQCR1:
	case MCR_WTQCR2:
	case MCR_WTQCR3:
	case MCR_WTQCR4:
	case MCR_WTQCR5:
	case MCR_WTQCR6:
	case MCR_WTQCR7:
		return TRUE;

	default:
		return FALSE;
	}
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Read a 32-bit device register of SDIO host driver domian
*
* \param[in] prGlueInfo Pointer to the GLUE_INFO_T structure.
* \param[in] u4Register Register offset
* \param[in] pu4Value   Pointer to variable used to store read value
*
* \retval TRUE          operation success
* \retval FALSE         operation fail
*/
/*----------------------------------------------------------------------------*/
u_int8_t kalDevRegRead(IN struct GLUE_INFO *prGlueInfo, IN uint32_t u4Register, OUT uint32_t *pu4Value)
{
	int ret = 0;
	uint8_t ucRetryCount = 0;

	ASSERT(prGlueInfo);
	ASSERT(pu4Value);

	do {
#if MTK_WCN_HIF_SDIO
		ret = mtk_wcn_hif_sdio_readl(prGlueInfo->rHifInfo.cltCtx, u4Register, (uint32_t *) pu4Value);
#else
		sdio_claim_host(prGlueInfo->rHifInfo.func);
		*pu4Value = sdio_readl(prGlueInfo->rHifInfo.func, u4Register, &ret);
		sdio_release_host(prGlueInfo->rHifInfo.func);
#endif

		if (ret || ucRetryCount) {
			/* DBGLOG(HAL, ERROR,
			 *  ("sdio_readl() addr: 0x%08x value: 0x%08x status: %x retry: %u\n",
			 *  u4Register, (unsigned int)*pu4Value, (unsigned int)ret, ucRetryCount));
			 */

			if (glIsReadClearReg(u4Register) && (ucRetryCount == 0)) {
				/* Read Snapshot CR instead */
				u4Register = MCR_WSR;
			}
		}

		ucRetryCount++;
		if (ucRetryCount > HIF_SDIO_ACCESS_RETRY_LIMIT)
			break;
	} while (ret);

	if (ret) {
		kalSendAeeWarning(HIF_SDIO_ERR_TITLE_STR,
				  HIF_SDIO_ERR_DESC_STR "sdio_readl() reports error: %x retry: %u", ret, ucRetryCount);
		DBGLOG(HAL, ERROR, "sdio_readl() reports error: %x retry: %u\n", ret, ucRetryCount);
	}

	return (ret) ? FALSE : TRUE;
}				/* end of kalDevRegRead() */

/*----------------------------------------------------------------------------*/
/*!
* \brief Read a 32-bit device register of chip firmware register domain
*
* \param[in] prGlueInfo Pointer to the GLUE_INFO_T structure.
* \param[in] u4Register Register offset
* \param[in] pu4Value   Pointer to variable used to store read value
*
* \retval TRUE          operation success
* \retval FALSE         operation fail
*/
/*----------------------------------------------------------------------------*/
u_int8_t kalDevRegRead_mac(IN struct GLUE_INFO *prGlueInfo, IN uint32_t u4Register, OUT uint32_t *pu4Value)
{
	uint32_t value;
	uint32_t u4Time, u4Current;
	uint8_t ucResult;/* For Unchecked return value*/

    /* progrqm h2d mailbox0 as interested register address */
	ucResult = kalDevRegWrite(prGlueInfo, MCR_H2DSM0R, u4Register);

    /* set h2d interrupt to notify firmware. bit16 */
	ucResult = kalDevRegWrite(prGlueInfo, MCR_WSICR,
			SDIO_MAILBOX_FUNC_READ_REG_IDX);

	/* polling interrupt status asserted. bit16 */

	/* first, disable interrupt enable for SDIO_MAILBOX_FUNC_READ_REG_IDX */
	ucResult = kalDevRegRead(prGlueInfo, MCR_WHIER, &value);
	ucResult = kalDevRegWrite(prGlueInfo, MCR_WHIER,
			(value & ~SDIO_MAILBOX_FUNC_READ_REG_IDX));

	u4Time = (uint32_t) kalGetTimeTick();

	do {
		/* check bit16 of WHISR assert for read register response */
		ucResult = kalDevRegRead(prGlueInfo, MCR_WHISR, &value);

		if (value & SDIO_MAILBOX_FUNC_READ_REG_IDX) {
			/* read d2h mailbox0 for interested register address */
			ucResult = kalDevRegRead(prGlueInfo,
						MCR_D2HRM0R, &value);

			if (value != u4Register) {
				DBGLOG(HAL, ERROR, "ERROR! kalDevRegRead_mac():register address mis-match");
				DBGLOG(HAL, ERROR, "(u4Register = 0x%08x, reported register = 0x%08x)\n",
				u4Register, value);
				return  FALSE;
			}

			/* read d2h mailbox1 for the value of the register */
			ucResult = kalDevRegRead(prGlueInfo,
						MCR_D2HRM1R, &value);
			*pu4Value = value;
			return	TRUE;
		}

		/* timeout exceeding check */
		u4Current = (uint32_t) kalGetTimeTick();

		if (((u4Current > u4Time) && ((u4Current - u4Time) > HIF_SDIO_INTERRUPT_RESPONSE_TIMEOUT))
			|| (u4Current < u4Time && ((u4Current + (0xFFFFFFFF - u4Time))
			> HIF_SDIO_INTERRUPT_RESPONSE_TIMEOUT))) {
			DBGLOG(HAL, ERROR, "ERROR: kalDevRegRead_mac(): response timeout\n");
			return	FALSE;
		}

		/* Response packet is not ready */
		kalUdelay(50);
	} while (1);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Write a 32-bit device register of SDIO driver domian
*
* \param[in] prGlueInfo Pointer to the GLUE_INFO_T structure.
* \param[in] u4Register Register offset
* \param[in] u4Value    Value to be written
*
* \retval TRUE          operation success
* \retval FALSE         operation fail
*/
/*----------------------------------------------------------------------------*/
u_int8_t kalDevRegWrite(IN struct GLUE_INFO *prGlueInfo, IN uint32_t u4Register, IN uint32_t u4Value)
{
	int ret = 0;
	uint8_t ucRetryCount = 0;

	ASSERT(prGlueInfo);

	do {
#if MTK_WCN_HIF_SDIO
		ret = mtk_wcn_hif_sdio_writel(prGlueInfo->rHifInfo.cltCtx, u4Register, u4Value);
#else
		sdio_claim_host(prGlueInfo->rHifInfo.func);
		sdio_writel(prGlueInfo->rHifInfo.func, u4Value, u4Register, &ret);
		sdio_release_host(prGlueInfo->rHifInfo.func);
#endif

		if (ret || ucRetryCount) {
			/* DBGLOG(HAL, ERROR,
			 *  ("sdio_writel() addr: 0x%x status: %x retry: %u\n", u4Register,
			 *  ret, ucRetryCount));
			 */
		}

		ucRetryCount++;
		if (ucRetryCount > HIF_SDIO_ACCESS_RETRY_LIMIT)
			break;

	} while (ret);

	if (ret) {
		kalSendAeeWarning(HIF_SDIO_ERR_TITLE_STR,
				  HIF_SDIO_ERR_DESC_STR "sdio_writel() reports error: %x retry: %u", ret, ucRetryCount);
		DBGLOG(HAL, ERROR, "sdio_writel() reports error: %x retry: %u\n", ret, ucRetryCount);
	}

	return (ret) ? FALSE : TRUE;
}				/* end of kalDevRegWrite() */

/*----------------------------------------------------------------------------*/
/*!
* \brief Write a 32-bit device register of chip firmware register domain
*
* \param[in] prGlueInfo Pointer to the GLUE_INFO_T structure.
* \param[in] u4Register Register offset
* \param[in] u4Value    Value to be written
*
* \retval TRUE          operation success
* \retval FALSE         operation fail
*/
/*----------------------------------------------------------------------------*/
u_int8_t kalDevRegWrite_mac(IN struct GLUE_INFO *prGlueInfo, IN uint32_t u4Register, IN uint32_t u4Value)
{
	uint32_t value;
	uint32_t u4Time, u4Current;
	uint8_t ucResult; /* For Unchecked return value*/

	/* progrqm h2d mailbox0 as interested register address */
	ucResult = kalDevRegWrite(prGlueInfo, MCR_H2DSM0R, u4Register);

	/* progrqm h2d mailbox1 as the value to write */
	ucResult = kalDevRegWrite(prGlueInfo, MCR_H2DSM1R, u4Value);

	/*  set h2d interrupt to notify firmware bit17 */
	ucResult = kalDevRegWrite(prGlueInfo, MCR_WSICR,
			SDIO_MAILBOX_FUNC_WRITE_REG_IDX);

	/* polling interrupt status asserted. bit17 */

	/* first, disable interrupt enable for SDIO_MAILBOX_FUNC_WRITE_REG_IDX */
	ucResult = kalDevRegRead(prGlueInfo, MCR_WHIER, &value);
	ucResult = kalDevRegWrite(prGlueInfo, MCR_WHIER,
			(value & ~SDIO_MAILBOX_FUNC_WRITE_REG_IDX));

	u4Time = (uint32_t) kalGetTimeTick();

	do {
		/* check bit17 of WHISR assert for response */
		ucResult = kalDevRegRead(prGlueInfo, MCR_WHISR, &value);

		if (value & SDIO_MAILBOX_FUNC_WRITE_REG_IDX) {
			/* read d2h mailbox0 for interested register address */
			ucResult = kalDevRegRead(prGlueInfo,
						MCR_D2HRM0R, &value);

			if (value != u4Register) {
				DBGLOG(HAL, ERROR, "ERROR! kalDevRegWrite_mac():register address mis-match");
				DBGLOG(HAL, ERROR, "(u4Register = 0x%08x, reported register = 0x%08x)\n",
				u4Register, value);
				return  FALSE;
			}
			return	TRUE;
		}

		/* timeout exceeding check */
		u4Current = (uint32_t) kalGetTimeTick();

		if (((u4Current > u4Time) && ((u4Current - u4Time) > HIF_SDIO_INTERRUPT_RESPONSE_TIMEOUT))
			|| (u4Current < u4Time && ((u4Current + (0xFFFFFFFF - u4Time))
			> HIF_SDIO_INTERRUPT_RESPONSE_TIMEOUT))) {
			DBGLOG(HAL, ERROR, "ERROR: kalDevRegWrite_mac(): response timeout\n");
			return	FALSE;
		}

		/* Response packet is not ready */
		kalUdelay(50);
	} while (1);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Read device I/O port
*
* \param[in] prGlueInfo         Pointer to the GLUE_INFO_T structure.
* \param[in] u2Port             I/O port offset
* \param[in] u2Len              Length to be read
* \param[out] pucBuf            Pointer to read buffer
* \param[in] u2ValidOutBufSize  Length of the buffer valid to be accessed
*
* \retval TRUE          operation success
* \retval FALSE         operation fail
*/
/*----------------------------------------------------------------------------*/
u_int8_t
kalDevPortRead(IN struct GLUE_INFO *prGlueInfo,
	       IN uint16_t u2Port, IN uint32_t u4Len, OUT uint8_t *pucBuf, IN uint32_t u4ValidOutBufSize)
{
	struct GL_HIF_INFO *prHifInfo = NULL;
	uint8_t *pucDst = NULL;
	int count = u4Len;
	int ret = 0;
	int bNum = 0;

#if (MTK_WCN_HIF_SDIO == 0)
	struct sdio_func *prSdioFunc = NULL;
#endif

	ASSERT(prGlueInfo);
	prHifInfo = &prGlueInfo->rHifInfo;

	ASSERT(pucBuf);
	pucDst = pucBuf;

	ASSERT(u4Len <= u4ValidOutBufSize);

#if (MTK_WCN_HIF_SDIO == 0)
	prSdioFunc = prHifInfo->func;

	ASSERT(prSdioFunc->cur_blksize > 0);

	sdio_claim_host(prSdioFunc);

	/* Split buffer into multiple single block to workaround hifsys */
	while (count >= prSdioFunc->cur_blksize) {
		count -= prSdioFunc->cur_blksize;
		bNum++;
	}
	if (count > 0 && bNum > 0)
		bNum++;

	if (bNum > 0) {
		ret = sdio_readsb(prSdioFunc, pucDst, u2Port, prSdioFunc->cur_blksize * bNum);

#ifdef CONFIG_X86
		/* ENE workaround */
		{
			int tmp;

			sdio_writel(prSdioFunc, 0x0, SDIO_X86_WORKAROUND_WRITE_MCR, &tmp);
		}
#endif

	} else {
		ret = sdio_readsb(prSdioFunc, pucDst, u2Port, count);
	}

	sdio_release_host(prSdioFunc);
#else

	/* Split buffer into multiple single block to workaround hifsys */
	while (count >= (prGlueInfo->rHifInfo).prFuncInfo->blk_sz) {
		count -= ((prGlueInfo->rHifInfo).prFuncInfo->blk_sz);
		bNum++;
	}
	if (count > 0 && bNum > 0)
		bNum++;

	if (bNum > 0) {
		ret =
		    mtk_wcn_hif_sdio_read_buf(prGlueInfo->rHifInfo.cltCtx, u2Port, (uint32_t *) pucDst,
					      ((prGlueInfo->rHifInfo).prFuncInfo->blk_sz) * bNum);
	} else {
		ret = mtk_wcn_hif_sdio_read_buf(prGlueInfo->rHifInfo.cltCtx, u2Port, (uint32_t *) pucDst, count);
	}
#endif

	if (ret) {
		kalSendAeeWarning(HIF_SDIO_ERR_TITLE_STR, HIF_SDIO_ERR_DESC_STR "sdio_readsb() reports error: %d", ret);

		DBGLOG(HAL, ERROR, "\n");
		DBGLOG(HAL, ERROR, "\nSDIO BUS ERROR: ");
		DBGLOG(HAL, ERROR, "sdio_readsb() reports error: %d\n", ret);
		DBGLOG(HAL, ERROR, "\n");
	}

	return (ret) ? FALSE : TRUE;
}				/* end of kalDevPortRead() */

/*----------------------------------------------------------------------------*/
/*!
* \brief Write device I/O port
*
* \param[in] prGlueInfo         Pointer to the GLUE_INFO_T structure.
* \param[in] u2Port             I/O port offset
* \param[in] u2Len              Length to be write
* \param[in] pucBuf             Pointer to write buffer
* \param[in] u2ValidInBufSize   Length of the buffer valid to be accessed
*
* \retval TRUE          operation success
* \retval FALSE         operation fail
*/
/*----------------------------------------------------------------------------*/
u_int8_t
kalDevPortWrite(IN struct GLUE_INFO *prGlueInfo,
		IN uint16_t u2Port, IN uint32_t u4Len, IN uint8_t *pucBuf, IN uint32_t u4ValidInBufSize)
{
	struct GL_HIF_INFO *prHifInfo = NULL;
	uint8_t *pucSrc = NULL;
	int count = u4Len;
	int ret = 0;
	int bNum = 0;

#if (MTK_WCN_HIF_SDIO == 0)
	struct sdio_func *prSdioFunc = NULL;
#endif


	ASSERT(prGlueInfo);
	prHifInfo = &prGlueInfo->rHifInfo;

	ASSERT(pucBuf);
	pucSrc = pucBuf;

	ASSERT(u4Len <= u4ValidInBufSize);

#if (MTK_WCN_HIF_SDIO == 0)
	prSdioFunc = prHifInfo->func;
	ASSERT(prSdioFunc->cur_blksize > 0);

	sdio_claim_host(prSdioFunc);

	/* Split buffer into multiple single block to workaround hifsys */
	while (count >= prSdioFunc->cur_blksize) {
		count -= prSdioFunc->cur_blksize;
		bNum++;
	}
	if (count > 0 && bNum > 0)
		bNum++;

	if (bNum > 0) {		/* block mode */
		ret = sdio_writesb(prSdioFunc, u2Port, pucSrc, prSdioFunc->cur_blksize * bNum);

#ifdef CONFIG_X86
		/* ENE workaround */
		{
			int tmp;

			sdio_writel(prSdioFunc, 0x0, SDIO_X86_WORKAROUND_WRITE_MCR, &tmp);
		}
#endif

	} else {		/* byte mode */

		ret = sdio_writesb(prSdioFunc, u2Port, pucSrc, count);
	}

	sdio_release_host(prSdioFunc);
#else
	/* Split buffer into multiple single block to workaround hifsys */
	while (count >= ((prGlueInfo->rHifInfo).prFuncInfo->blk_sz)) {
		count -= ((prGlueInfo->rHifInfo).prFuncInfo->blk_sz);
		bNum++;
	}
	if (count > 0 && bNum > 0)
		bNum++;

	if (bNum > 0) {		/* block mode */
		ret =
		    mtk_wcn_hif_sdio_write_buf(prGlueInfo->rHifInfo.cltCtx, u2Port,
					       (uint32_t *) pucSrc, ((prGlueInfo->rHifInfo).prFuncInfo->blk_sz) * bNum);
	} else {		/* byte mode */
		ret = mtk_wcn_hif_sdio_write_buf(prGlueInfo->rHifInfo.cltCtx, u2Port, (uint32_t *) pucSrc, count);
	}
#endif

	if (ret) {
		kalSendAeeWarning(HIF_SDIO_ERR_TITLE_STR,
				  HIF_SDIO_ERR_DESC_STR "sdio_writesb() reports error: %d", ret);

		DBGLOG(HAL, ERROR, "\n");
		DBGLOG(HAL, ERROR, "\nSDIO BUS ERROR: ");
		DBGLOG(HAL, ERROR, "sdio_writesb() reports error: %d\n", ret);
		DBGLOG(HAL, ERROR, "\n");
	}

	return (ret) ? FALSE : TRUE;
}				/* end of kalDevPortWrite() */

/*----------------------------------------------------------------------------*/
/*!
* @brief Read interrupt status from hardware
*
* @param prAdapter pointer to the Adapter handler
* @param the interrupts
*
* @return N/A
*
*/
/*----------------------------------------------------------------------------*/
void kalDevReadIntStatus(IN struct ADAPTER *prAdapter, OUT uint32_t *pu4IntStatus)
{
#if CFG_SDIO_INTR_ENHANCE
	struct ENHANCE_MODE_DATA_STRUCT *prSDIOCtrl;
	struct SDIO_STAT_COUNTER *prStatCounter;
	u_int8_t fgPendingInt = FALSE;

	SDIO_TIME_INTERVAL_DEC();

	DEBUGFUNC("nicSDIOReadIntStatus");

	ASSERT(prAdapter);
	ASSERT(pu4IntStatus);

	prSDIOCtrl = prAdapter->prGlueInfo->rHifInfo.prSDIOCtrl;
	ASSERT(prSDIOCtrl);

	prStatCounter = &prAdapter->prGlueInfo->rHifInfo.rStatCounter;

	/* There are pending interrupt to be handled */
	if (prAdapter->prGlueInfo->rHifInfo.fgIsPendingInt) {
		prAdapter->prGlueInfo->rHifInfo.fgIsPendingInt = FALSE;
		fgPendingInt = TRUE;
	}
	else {
		SDIO_REC_TIME_START();
		HAL_PORT_RD(prAdapter, MCR_WHISR, sizeof(struct ENHANCE_MODE_DATA_STRUCT),
			(uint8_t *) prSDIOCtrl, sizeof(struct ENHANCE_MODE_DATA_STRUCT));
		SDIO_REC_TIME_END();
		SDIO_ADD_TIME_INTERVAL(prStatCounter->u4IntReadTime);
		prStatCounter->u4IntReadCnt++;
	}

	prStatCounter->u4IntCnt++;

	if (kalIsCardRemoved(prAdapter->prGlueInfo) == TRUE || fgIsBusAccessFailed == TRUE) {
		*pu4IntStatus = 0;
		return;
	}

	halProcessEnhanceInterruptStatus(prAdapter);

	if (prSDIOCtrl->u4WHISR) {
		halRecIntLog(prAdapter, prSDIOCtrl);
		if (fgPendingInt)
			halTagIntLog(prAdapter, SDIO_INT_RX_ENHANCE);
	}

	*pu4IntStatus = prSDIOCtrl->u4WHISR;
#else
	HAL_MCR_RD(prAdapter, MCR_WHISR, pu4IntStatus);
#endif /* CFG_SDIO_INTR_ENHANCE */

	if (*pu4IntStatus & ~(WHIER_DEFAULT | WHIER_FW_OWN_BACK_INT_EN)) {
		DBGLOG(INTR, WARN,
			"Un-handled HISR %lx, HISR = %lx (HIER:0x%lx)\n",
			(*pu4IntStatus & ~WHIER_DEFAULT), *pu4IntStatus,
			WHIER_DEFAULT);
		*pu4IntStatus &= WHIER_DEFAULT;
	}
}				/* end of nicSDIOReadIntStatus() */

/*----------------------------------------------------------------------------*/
/*!
* \brief Write device I/O port in byte with CMD52
*
* \param[in] prGlueInfo         Pointer to the GLUE_INFO_T structure.
* \param[in] u4Addr             I/O port offset
* \param[in] ucData             Single byte of data to be written
*
* \retval TRUE          operation success
* \retval FALSE         operation fail
*/
/*----------------------------------------------------------------------------*/
u_int8_t kalDevWriteWithSdioCmd52(IN struct GLUE_INFO *prGlueInfo, IN uint32_t u4Addr, IN uint8_t ucData)
{
	int ret = 0;

#if (MTK_WCN_HIF_SDIO == 0)
	sdio_claim_host(prGlueInfo->rHifInfo.func);
	sdio_writeb(prGlueInfo->rHifInfo.func, ucData, u4Addr, &ret);
	sdio_release_host(prGlueInfo->rHifInfo.func);
#else
	ret = mtk_wcn_hif_sdio_writeb(prGlueInfo->rHifInfo.cltCtx, u4Addr, ucData);
#endif

	if (ret) {
		kalSendAeeWarning(HIF_SDIO_ERR_TITLE_STR, HIF_SDIO_ERR_DESC_STR "sdio_writeb() reports error: %x", ret);
		DBGLOG(HAL, ERROR, "sdio_writeb() reports error: %x\n", ret);
	}

	return (ret) ? FALSE : TRUE;

}				/* end of kalDevWriteWithSdioCmd52() */

void glSetPowerState(IN struct GLUE_INFO *prGlueInfo, IN uint32_t ePowerMode)
{
}
#ifdef _SDIO_RING
u_int8_t kalDevWriteData(IN struct GLUE_INFO *prGlueInfo,
IN struct MSDU_INFO *prMsduInfo)
{
	struct ADAPTER *prAdapter = prGlueInfo->prAdapter;
	struct GL_HIF_INFO *prHifInfo = &prGlueInfo->rHifInfo;
	uint8_t *pucOutputBuf = (uint8_t *) NULL;
	uint32_t u4PaddingLength;
	struct sk_buff *skb;
	uint8_t *pucBuf;
	uint32_t u4Length;
	uint8_t ucTC;
	uint32_t head;
	uint32_t data_len;

	SDIO_TIME_INTERVAL_DEC();

	skb = (struct sk_buff *)prMsduInfo->prPacket;
	pucBuf = skb->data;
	u4Length = skb->len;
	ucTC = prMsduInfo->ucTC;

	head = prHifInfo->ring_head;

	pucOutputBuf = prHifInfo->ring_buffer[head];
	data_len = prHifInfo->ring_len[head];

	if ((data_len + ALIGN_4(u4Length)) > SDIO_RING_BUF_LEN) {
		if (kalDevKickData(prGlueInfo) == FALSE)
			return FALSE;
		head = prHifInfo->ring_head;
		pucOutputBuf = prHifInfo->ring_buffer[head];
		data_len = prHifInfo->ring_len[head];
	}
	if (prHifInfo->ring_PLE[head] ==
		prAdapter->nicTxReousrce.u4DataTotalResourcePle) {
		if (kalDevKickData(prGlueInfo) == FALSE)
			return FALSE;
		head = prHifInfo->ring_head;
		pucOutputBuf = prHifInfo->ring_buffer[head];
		data_len = prHifInfo->ring_len[head];
	}

	SDIO_REC_TIME_START();
	memcpy(pucOutputBuf + data_len, pucBuf, u4Length);
	SDIO_REC_TIME_END();
	SDIO_ADD_TIME_INTERVAL(prHifInfo->rStatCounter.u4TxDataCpTime);
	data_len += u4Length;

	u4PaddingLength = (ALIGN_4(u4Length) - u4Length);
	if (u4PaddingLength) {
		memset(pucOutputBuf + data_len, 0, u4PaddingLength);
		data_len += u4PaddingLength;
	}

	prHifInfo->ring_len[head] = data_len;
	prHifInfo->ring_PSE[head] +=
		nicTxGetPageCount(prAdapter,
		prMsduInfo->u2FrameLength, FALSE);
	prHifInfo->ring_PLE[head]++;

	SDIO_REC_TIME_START();
	if (!prMsduInfo->pfTxDoneHandler)
		kalFreeTxMsdu(prAdapter, prMsduInfo);
	SDIO_REC_TIME_END();
	SDIO_ADD_TIME_INTERVAL
		(prHifInfo->rStatCounter.u4TxDataFreeTime);

	/* Update pending Tx done count */
	halUpdateTxDonePendingCount
	(prAdapter, TRUE, ucTC, prMsduInfo->u2FrameLength);

	prHifInfo->rStatCounter.u4DataPktWriteCnt++;

	return TRUE;
}
u_int8_t kalDevKickData(IN struct GLUE_INFO *prGlueInfo)
{
	struct GL_HIF_INFO *prHifInfo = &prGlueInfo->rHifInfo;
	uint32_t head;
	uint32_t *data_len;
	u_int8_t ret;


	head = prHifInfo->ring_head;
	data_len = &prHifInfo->ring_len[head];

	if (*data_len == 0) {
		kalSetTxEvent2Hif(prGlueInfo);
		kalSetEvent(prGlueInfo);

		return FALSE;
	}
	ret = sdio_ring_push(prGlueInfo);

	if (ret == TRUE)
		kalSetEvent(prGlueInfo);

	kalSetTxEvent2Hif(prGlueInfo);

	return ret;
}
#else
/*----------------------------------------------------------------------------*/
/*!
* \brief Write data to device
*
* \param[in] prGlueInfo         Pointer to the GLUE_INFO_T structure.
* \param[in] prMsduInfo         msdu info
*
* \retval TRUE          operation success
* \retval FALSE         operation fail
*/
/*----------------------------------------------------------------------------*/
u_int8_t kalDevWriteData(IN struct GLUE_INFO *prGlueInfo, IN struct MSDU_INFO *prMsduInfo)
{
	struct ADAPTER *prAdapter = prGlueInfo->prAdapter;
	struct GL_HIF_INFO *prHifInfo = &prGlueInfo->rHifInfo;
	struct TX_CTRL *prTxCtrl;
	uint8_t *pucOutputBuf = (uint8_t *) NULL;
	uint32_t u4PaddingLength;
	struct sk_buff *skb;
	uint8_t *pucBuf;
	uint32_t u4Length;
	uint8_t ucTC;

	SDIO_TIME_INTERVAL_DEC();

	skb = (struct sk_buff *)prMsduInfo->prPacket;
	pucBuf = skb->data;
	u4Length = skb->len;
	ucTC = prMsduInfo->ucTC;

	prTxCtrl = &prAdapter->rTxCtrl;
	pucOutputBuf = prTxCtrl->pucTxCoalescingBufPtr;

	if (prTxCtrl->u4WrIdx + ALIGN_4(u4Length) > prAdapter->u4CoalescingBufCachedSize) {
		if ((prAdapter->u4CoalescingBufCachedSize - ALIGN_4(prTxCtrl->u4WrIdx)) >= HIF_TX_TERMINATOR_LEN) {
			/* fill with single dword of zero as TX-aggregation termination */
			*(uint32_t *) (&((pucOutputBuf)[ALIGN_4(prTxCtrl->u4WrIdx)])) = 0;
		}

		if (HAL_TEST_FLAG(prAdapter, ADAPTER_FLAG_HW_ERR) == FALSE) {
			if (kalDevPortWrite(prGlueInfo, MCR_WTDR1, prTxCtrl->u4WrIdx,
					pucOutputBuf, prAdapter->u4CoalescingBufCachedSize) == FALSE) {
				HAL_SET_FLAG(prAdapter, ADAPTER_FLAG_HW_ERR);
				fgIsBusAccessFailed = TRUE;
			}
			prHifInfo->rStatCounter.u4DataPortWriteCnt++;
		}
		prTxCtrl->u4WrIdx = 0;
	}

	SDIO_REC_TIME_START();
	memcpy(pucOutputBuf + prTxCtrl->u4WrIdx, pucBuf, u4Length);
	SDIO_REC_TIME_END();
	SDIO_ADD_TIME_INTERVAL(prHifInfo->rStatCounter.u4TxDataCpTime);

	prTxCtrl->u4WrIdx += u4Length;

	u4PaddingLength = (ALIGN_4(u4Length) - u4Length);
	if (u4PaddingLength) {
		memset(pucOutputBuf + prTxCtrl->u4WrIdx, 0, u4PaddingLength);
		prTxCtrl->u4WrIdx += u4PaddingLength;
	}

	SDIO_REC_TIME_START();
	if (!prMsduInfo->pfTxDoneHandler)
		kalFreeTxMsdu(prAdapter, prMsduInfo);
	SDIO_REC_TIME_END();
	SDIO_ADD_TIME_INTERVAL(prHifInfo->rStatCounter.u4TxDataFreeTime);

	/* Update pending Tx done count */
	halUpdateTxDonePendingCount(prAdapter, TRUE, ucTC,
					prMsduInfo->u2FrameLength);

	prHifInfo->rStatCounter.u4DataPktWriteCnt++;

	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Kick Tx data to device
*
* \param[in] prGlueInfo         Pointer to the GLUE_INFO_T structure.
*
* \retval TRUE          operation success
* \retval FALSE         operation fail
*/
/*----------------------------------------------------------------------------*/
u_int8_t kalDevKickData(IN struct GLUE_INFO *prGlueInfo)
{
	struct ADAPTER *prAdapter = prGlueInfo->prAdapter;
	struct GL_HIF_INFO *prHifInfo = &prGlueInfo->rHifInfo;
	struct TX_CTRL *prTxCtrl;
	uint8_t *pucOutputBuf = (uint8_t *) NULL;

	prTxCtrl = &prAdapter->rTxCtrl;
	pucOutputBuf = prTxCtrl->pucTxCoalescingBufPtr;

	if (prTxCtrl->u4WrIdx == 0)
		return FALSE;

	if ((prAdapter->u4CoalescingBufCachedSize - ALIGN_4(prTxCtrl->u4WrIdx)) >= HIF_TX_TERMINATOR_LEN) {
		/* fill with single dword of zero as TX-aggregation termination */
		*(uint32_t *) (&((pucOutputBuf)[ALIGN_4(prTxCtrl->u4WrIdx)])) = 0;
	}

	if (HAL_TEST_FLAG(prAdapter, ADAPTER_FLAG_HW_ERR) == FALSE) {
		if (kalDevPortWrite(prGlueInfo, MCR_WTDR1, prTxCtrl->u4WrIdx,
				pucOutputBuf, prAdapter->u4CoalescingBufCachedSize) == FALSE) {
			HAL_SET_FLAG(prAdapter, ADAPTER_FLAG_HW_ERR);
			fgIsBusAccessFailed = TRUE;
		}
		prHifInfo->rStatCounter.u4DataPortWriteCnt++;
	}

	prTxCtrl->u4WrIdx = 0;

	prHifInfo->rStatCounter.u4DataPortKickCnt++;

	return TRUE;
}
#endif
/*----------------------------------------------------------------------------*/
/*!
* \brief Write command to device
*
* \param[in] prGlueInfo         Pointer to the GLUE_INFO_T structure.
* \param[in] u4Addr             I/O port offset
* \param[in] ucData             Single byte of data to be written
*
* \retval TRUE          operation success
* \retval FALSE         operation fail
*/
/*----------------------------------------------------------------------------*/
u_int8_t kalDevWriteCmd(IN struct GLUE_INFO *prGlueInfo, IN struct CMD_INFO *prCmdInfo, IN uint8_t ucTC)
{
	struct ADAPTER *prAdapter = prGlueInfo->prAdapter;
/*	P_GL_HIF_INFO_T prHifInfo = &prGlueInfo->rHifInfo; */
	struct TX_CTRL *prTxCtrl;
	uint8_t *pucOutputBuf = (uint8_t *) NULL;
	uint16_t u2OverallBufferLength = 0;
/*	WLAN_STATUS u4Status = WLAN_STATUS_SUCCESS; */

	prTxCtrl = &prAdapter->rTxCtrl;
	pucOutputBuf = prTxCtrl->pucTxCoalescingBufPtr;

	if (TFCB_FRAME_PAD_TO_DW(prCmdInfo->u4TxdLen + prCmdInfo->u4TxpLen) >
		prAdapter->u4CoalescingBufCachedSize) {
		DBGLOG(HAL, ERROR, "Command TX buffer underflow!\n");
		return FALSE;
	}

	if (prCmdInfo->u4TxdLen) {
		memcpy((pucOutputBuf + u2OverallBufferLength), prCmdInfo->pucTxd, prCmdInfo->u4TxdLen);
		u2OverallBufferLength += prCmdInfo->u4TxdLen;
	}

	if (prCmdInfo->u4TxpLen) {
		memcpy((pucOutputBuf + u2OverallBufferLength), prCmdInfo->pucTxp, prCmdInfo->u4TxpLen);
		u2OverallBufferLength += prCmdInfo->u4TxpLen;
	}

	memset(pucOutputBuf + u2OverallBufferLength, 0,
		(TFCB_FRAME_PAD_TO_DW(u2OverallBufferLength) - u2OverallBufferLength));

	if ((prAdapter->u4CoalescingBufCachedSize - ALIGN_4(u2OverallBufferLength)) >= HIF_TX_TERMINATOR_LEN) {
		/* fill with single dword of zero as TX-aggregation termination */
		*(uint32_t *) (&((pucOutputBuf)[ALIGN_4(u2OverallBufferLength)])) = 0;
	}
	if (HAL_TEST_FLAG(prAdapter, ADAPTER_FLAG_HW_ERR) == FALSE) {
		if (kalDevPortWrite(prGlueInfo, MCR_WTDR1, TFCB_FRAME_PAD_TO_DW(u2OverallBufferLength),
				pucOutputBuf, prAdapter->u4CoalescingBufCachedSize) == FALSE) {
			HAL_SET_FLAG(prAdapter, ADAPTER_FLAG_HW_ERR);
			fgIsBusAccessFailed = TRUE;
		}
		prGlueInfo->rHifInfo.rStatCounter.u4CmdPortWriteCnt++;
	}

	/* Update pending Tx done count */
	halUpdateTxDonePendingCount(prAdapter, TRUE, ucTC,
				   (prCmdInfo->u4TxdLen + prCmdInfo->u4TxpLen));

	prGlueInfo->rHifInfo.rStatCounter.u4CmdPktWriteCnt++;
	return TRUE;
}

void glGetDev(void *ctx, struct device **dev)
{
#if MTK_WCN_HIF_SDIO
	mtk_wcn_hif_sdio_get_dev(*((unsigned long *) ctx), dev);
#else
	*dev = &((struct sdio_func *)ctx)->dev;
#endif
}

void glGetHifDev(struct GL_HIF_INFO *prHif, struct device **dev)
{
#if MTK_WCN_HIF_SDIO
	mtk_wcn_hif_sdio_get_dev(prHif->cltCtx, dev);
#else
	*dev = &(prHif->func->dev);
#endif
}

u_int8_t glWakeupSdio(struct GLUE_INFO *prGlueInfo)
{
	u_int8_t fgSuccess = TRUE;

#if (HIF_SDIO_SUPPORT_GPIO_SLEEP_MODE && MTK_WCN_HIF_SDIO)
	#if KERNEL_VERSION(4, 4, 0) <= CFG80211_VERSION_CODE
		if (mtk_wcn_stp_sdio_wake_up_ctrl(prGlueInfo->rHifInfo.cltCtx) != 0)
	#else
		if (mtk_wcn_hif_sdio_wake_up_ctrl(prGlueInfo->rHifInfo.cltCtx) != 0)
	#endif
		fgSuccess = FALSE;
#endif

	return fgSuccess;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function set SDIO state
*
* \param[in] prHifInfo  Pointer to the GL_HIF_INFO_T structure
* \param[in] state      Specify TC index
*
* \retval TRUE          operation success
* \retval FALSE         operation fail
*/
/*----------------------------------------------------------------------------*/
void glSdioSetState(struct GL_HIF_INFO *prHifInfo, enum sdio_state state)
{
	unsigned long flags;

	spin_lock_irqsave(&prHifInfo->rStateLock, flags);
	prHifInfo->state = state;
	spin_unlock_irqrestore(&prHifInfo->rStateLock, flags);
	DBGLOG(HAL, STATE, "sdio state:%d\n", state);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function get SDIO state
*
* \param[in] prHifInfo  Pointer to the GL_HIF_INFO_T structure
* \param[in] state      Specify TC index
*
* \retval TRUE          operation success
* \retval FALSE         operation fail
*/
/*----------------------------------------------------------------------------*/
enum sdio_state glSdioGetState(struct GL_HIF_INFO *prHifInfo)
{
	return prHifInfo->state;
}

#if (CFG_CHIP_RESET_SUPPORT == 1) && (MTK_WCN_HIF_SDIO == 0)
void kalRemoveProbe(IN struct GLUE_INFO *prGlueInfo)
{
	struct mmc_host *host;

	ASSERT(prGlueInfo);

	host = prGlueInfo->rHifInfo.func->card->host;
	host->rescan_entered = 0;

	/* clear trx fifo */
	DBGLOG(INIT, STATE, "[SER][L0] mmc_remove_host\n");
	mmc_remove_host(prGlueInfo->rHifInfo.func->card->host);

	DBGLOG(INIT, STATE, "[SER][L0] mmc_add_host\n");
	mmc_add_host(host);

}
#endif
