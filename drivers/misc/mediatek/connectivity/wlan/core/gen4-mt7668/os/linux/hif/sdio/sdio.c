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
#include <linux/mmc/sdio_ids.h>
#endif

#include <linux/mm.h>
#ifndef CONFIG_X86
#include <asm/memory.h>
#endif

#include "mt66xx_reg.h"

#if (CFG_SDIO_1BIT_DATA_MODE == 1)
#include "test_driver_sdio_ops.h"
#endif

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

#define HIF_SDIO_ERR_TITLE_STR              "["CHIP_NAME"] SDIO Access Error!"
#define HIF_SDIO_ERR_DESC_STR               "**SDIO Access Error**\n"

#define HIF_SDIO_ACCESS_RETRY_LIMIT         3
#define HIF_SDIO_INTERRUPT_RESPONSE_TIMEOUT (15000)

#if MTK_WCN_HIF_SDIO

/*
 * function prototypes
 *
 */

static INT_32 mtk_sdio_probe(MTK_WCN_HIF_SDIO_CLTCTX, const MTK_WCN_HIF_SDIO_FUNCINFO *);

static INT_32 mtk_sdio_remove(MTK_WCN_HIF_SDIO_CLTCTX);
static INT_32 mtk_sdio_interrupt(MTK_WCN_HIF_SDIO_CLTCTX);

/*
 * sdio function info table
 */

static MTK_WCN_HIF_SDIO_FUNCINFO funcInfo[] = {
	{MTK_WCN_HIF_SDIO_FUNC(0x037a, 0x6602, 0x1, 512)},
};

static MTK_WCN_SDIO_DRIVER_DATA_MAPPING sdio_driver_data_mapping[] = {
	{0x6602, &mt66xx_driver_data_mt6632},
};

static MTK_WCN_HIF_SDIO_CLTINFO cltInfo = {
	.func_tbl = funcInfo,
	.func_tbl_size = sizeof(funcInfo) / sizeof(MTK_WCN_HIF_SDIO_FUNCINFO),
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

const struct sdio_device_id mtk_sdio_ids[] = {
	{	SDIO_DEVICE(0x037a, 0x6602),
		.driver_data = (kernel_ulong_t)&mt66xx_driver_data_mt6632},/* Not an SDIO standard class device */
	{	SDIO_DEVICE(0x037a, 0x7608),
		.driver_data = (kernel_ulong_t)&mt66xx_driver_data_mt7668},/* Not an SDIO standard class device */
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
#ifdef CFG_SUPPORT_DUAL_CARD_DUAL_DRIVER_B
	.name = "wlanb",        /* "MTK SDIO WLAN Driver" */
#else
	.name = "wlan",     /* "MTK SDIO WLAN Driver" */
#endif
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

#if MTK_WCN_HIF_SDIO

static INT_32 mtk_sdio_interrupt(MTK_WCN_HIF_SDIO_CLTCTX cltCtx)
{
	P_GLUE_INFO_T prGlueInfo = NULL;
	INT_32 ret = 0;

	prGlueInfo = mtk_wcn_hif_sdio_get_drvdata(cltCtx);

	/* ASSERT(prGlueInfo); */

	if (!prGlueInfo) {
		/* printk(KERN_INFO DRV_NAME"No glue info in mtk_sdio_interrupt()\n"); */
		return -HIF_SDIO_ERR_FAIL;
	}

	if (prGlueInfo->ulFlag & GLUE_FLAG_HALT) {
		/* printk(KERN_INFO DRV_NAME"GLUE_FLAG_HALT skip INT\n"); */
		ret = mtk_wcn_hif_sdio_writeb(cltCtx, MCR_WHLPCR, WHLPCR_INT_EN_CLR);
		return ret;
	}

	ret = mtk_wcn_hif_sdio_writeb(cltCtx, MCR_WHLPCR, WHLPCR_INT_EN_CLR);

	prGlueInfo->rHifInfo.fgIsPendingInt = FALSE;

	kalSetIntEvent(prGlueInfo);

	return ret;
}

#else
static void mtk_sdio_interrupt(struct sdio_func *func)
{
	P_GLUE_INFO_T prGlueInfo = NULL;

	int ret = 0;

	prGlueInfo = sdio_get_drvdata(func);
	/* ASSERT(prGlueInfo); */

	if (!prGlueInfo) {
		/* printk(KERN_INFO DRV_NAME"No glue info in mtk_sdio_interrupt()\n"); */
		return;
	}

	if (prGlueInfo->ulFlag & GLUE_FLAG_HALT) {
		sdio_writeb(prGlueInfo->rHifInfo.func, WHLPCR_INT_EN_CLR, MCR_WHLPCR, &ret);
		/* printk(KERN_INFO DRV_NAME"GLUE_FLAG_HALT skip INT\n"); */
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
static const MTK_WCN_HIF_SDIO_FUNCINFO *prFunc;
INT_32 mtk_sdio_probe(
	MTK_WCN_HIF_SDIO_CLTCTX cltCtx,
	const MTK_WCN_HIF_SDIO_FUNCINFO *prFuncInfo)
{
	INT_32 ret = HIF_SDIO_ERR_SUCCESS;
	INT_32 i = 0;
	INT_32 dd_table_len = sizeof(sdio_driver_data_mapping) / sizeof(MTK_WCN_SDIO_DRIVER_DATA_MAPPING);
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

	if (pfWlanProbe((PVOID)&cltCtx, (PVOID) sdio_driver_data) != WLAN_STATUS_SUCCESS) {
		/* printk(KERN_WARNING DRV_NAME"pfWlanProbe fail!call pfWlanRemove()\n"); */
		pfWlanRemove();
		ret = -(HIF_SDIO_ERR_FAIL);
	} else {
		/* printk(KERN_INFO DRV_NAME"mtk_wifi_sdio_probe() done(%d)\n", ret); */
	}
	return ret;
}
#else
int mtk_sdio_probe(struct sdio_func *func, const struct sdio_device_id *id)
{
	int ret = 0;
#if defined(CFG_SUPPORT_DUAL_CARD_DUAL_DRIVER_A)
#define MMC_FILTER	"mmc1"
#elif defined(CFG_SUPPORT_DUAL_CARD_DUAL_DRIVER_B)
#define MMC_FILTER	"mmc0"
#endif

	/* int i = 0; */

	/* printk(KERN_INFO DRV_NAME "mtk_sdio_probe()\n"); */

	ASSERT(func);
	ASSERT(id);
	/*
	*printk(KERN_INFO DRV_NAME "Basic struct size checking...\n");
	*printk(KERN_INFO DRV_NAME "sizeof(struct device) = %d\n", sizeof(struct device));
	*printk(KERN_INFO DRV_NAME "sizeof(struct mmc_host) = %d\n", sizeof(struct mmc_host));
	*printk(KERN_INFO DRV_NAME "sizeof(struct mmc_card) = %d\n", sizeof(struct mmc_card));
	*printk(KERN_INFO DRV_NAME "sizeof(struct mmc_driver) = %d\n", sizeof(struct mmc_driver));
	*printk(KERN_INFO DRV_NAME "sizeof(struct mmc_data) = %d\n", sizeof(struct mmc_data));
	*printk(KERN_INFO DRV_NAME "sizeof(struct mmc_command) = %d\n", sizeof(struct mmc_command));
	*printk(KERN_INFO DRV_NAME "sizeof(struct mmc_request) = %d\n", sizeof(struct mmc_request));
	*printk(KERN_INFO DRV_NAME "sizeof(struct sdio_func) = %d\n", sizeof(struct sdio_func));
	*
	*printk(KERN_INFO DRV_NAME "Card information checking...\n");
	*printk(KERN_INFO DRV_NAME "func = 0x%p\n", func);
	*printk(KERN_INFO DRV_NAME "Number of info = %d:\n", func->card->num_info);
	*
	*
	*for (i = 0; i < func->card->num_info; i++)
	*	printk(KERN_INFO DRV_NAME "info[%d]: %s\n", i, func->card->info[i]);
	*
	*/
#ifdef MMC_FILTER
	if (strcmp(mmc_hostname(func->card->host), MMC_FILTER) == 0) {
		ret = -1;
		goto out;
	}
#endif

	

	sdio_claim_host(func);
	ret = sdio_enable_func(func);
	sdio_release_host(func);

	if (ret) {
		/* printk(KERN_INFO DRV_NAME"sdio_enable_func failed!\n"); */
		goto out;
	}
	/* printk(KERN_INFO DRV_NAME"sdio_enable_func done!\n"); */

	if (pfWlanProbe((PVOID) func, (PVOID) id->driver_data) != WLAN_STATUS_SUCCESS) {
		/* printk(KERN_WARNING DRV_NAME"pfWlanProbe fail!call pfWlanRemove()\n"); */
		pfWlanRemove();
		ret = -1;
	}

out:
	/* printk(KERN_INFO DRV_NAME"mtk_sdio_probe() done(%d)\n", ret); */
	return ret;
}
#endif

#if MTK_WCN_HIF_SDIO
INT_32 mtk_sdio_remove(MTK_WCN_HIF_SDIO_CLTCTX cltCtx)
{
	INT_32 ret = HIF_SDIO_ERR_SUCCESS;
	/* printk(KERN_INFO DRV_NAME"pfWlanRemove done\n"); */
	pfWlanRemove();

	return ret;
}
#else
void mtk_sdio_remove(struct sdio_func *func)
{
	/* printk(KERN_INFO DRV_NAME"mtk_sdio_remove()\n"); */

	ASSERT(func);
	/* printk(KERN_INFO DRV_NAME"pfWlanRemove done\n"); */
	pfWlanRemove();

	sdio_claim_host(func);
	sdio_disable_func(func);
	/* printk(KERN_INFO DRV_NAME"sdio_disable_func() done\n"); */
	sdio_release_host(func);

	/* printk(KERN_INFO DRV_NAME"mtk_sdio_remove() done\n"); */
}
#endif

#if (MTK_WCN_HIF_SDIO == 0)
static int mtk_sdio_pm_suspend(struct device *pDev)
{
	int ret = 0, wait = 0;
	int pm_caps, set_flag;
	const char *func_id;
	struct sdio_func *func;
	P_GLUE_INFO_T prGlueInfo = NULL;

	DBGLOG(HAL, STATE, "==>\n");

	func = dev_to_sdio_func(pDev);
	prGlueInfo = sdio_get_drvdata(func);

	/* This suspend API could be triggered multiple times.
	*  Make sure wlanSuspendPmHandle() called once only.
	*/
	if (prGlueInfo->prAdapter->fgForceFwOwn == FALSE)
		wlanSuspendPmHandle(prGlueInfo);

	prGlueInfo->prAdapter->fgForceFwOwn = TRUE;

	/* Wait for
	*  1. The other unfinished ownership handshakes
	*  2. FW own back
	*/
	wait = 0;
	while (1) {
		if (prGlueInfo->prAdapter->u4PwrCtrlBlockCnt == 0
				&& prGlueInfo->prAdapter->fgIsFwOwn == TRUE) {
			DBGLOG(HAL, STATE, "************************\n");
			DBGLOG(HAL, STATE, "* Entered SDIO Supsend *\n");
			DBGLOG(HAL, STATE, "************************\n");
			DBGLOG(HAL, INFO, "wait = %d\n\n", wait);
			break;
		}

		ACQUIRE_POWER_CONTROL_FROM_PM(prGlueInfo->prAdapter);
		kalMsleep(5);
		RECLAIM_POWER_CONTROL_TO_PM(prGlueInfo->prAdapter, FALSE);

		if (wait > 200) {
			DBGLOG(HAL, ERROR, "Timeout !!\n\n");
			return -EAGAIN;
		}
		wait++;
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

	DBGLOG(HAL, STATE, "<==\n");
	return 0;
}

static int mtk_sdio_pm_resume(struct device *pDev)
{
	struct sdio_func *func;
	P_GLUE_INFO_T prGlueInfo = NULL;

	DBGLOG(HAL, STATE, "==>\n");

	func = dev_to_sdio_func(pDev);
	prGlueInfo = sdio_get_drvdata(func);

	prGlueInfo->prAdapter->fgForceFwOwn = FALSE;

	wlanResumePmHandle(prGlueInfo);

	DBGLOG(HAL, STATE, "<==\n");
	return 0;
}

static int mtk_sdio_suspend(struct device *pDev, pm_message_t state)
{
	/* printk(KERN_INFO "mtk_sdio: mtk_sdio_suspend dev(0x%p)\n", pDev); */
	/* printk(KERN_INFO "mtk_sdio: MediaTek SDIO WLAN driver\n"); */

	return mtk_sdio_pm_suspend(pDev);
}

int mtk_sdio_resume(struct device *pDev)
{
	/* printk(KERN_INFO "mtk_sdio: mtk_sdio_resume dev(0x%p)\n", pDev); */

	return mtk_sdio_pm_resume(pDev);
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
WLAN_STATUS glRegisterBus(probe_card pfProbe, remove_card pfRemove)
{
	int ret = 0;

	ASSERT(pfProbe);
	ASSERT(pfRemove);

	/* printk(KERN_INFO "mtk_sdio: MediaTek SDIO WLAN driver\n"); */
	/* printk(KERN_INFO "mtk_sdio: Copyright MediaTek Inc.\n"); */

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

	mtk_sdio_driver.drv.suspend = mtk_sdio_suspend;
	mtk_sdio_driver.drv.resume = mtk_sdio_resume;

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
VOID glUnregisterBus(remove_card pfRemove)
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
VOID glSetHifInfo(P_GLUE_INFO_T prGlueInfo, ULONG ulCookie)
{
	P_GL_HIF_INFO_T prHif = NULL;
	UINT_8 ucIdx;

	prHif = &prGlueInfo->rHifInfo;

	QUEUE_INITIALIZE(&prHif->rFreeQueue);
	QUEUE_INITIALIZE(&prHif->rRxDeAggQueue);
	QUEUE_INITIALIZE(&prHif->rRxFreeBufQueue);

#if MTK_WCN_HIF_SDIO
	/* prHif->prFuncInfo = ((MTK_WCN_HIF_SDIO_FUNCINFO *) u4Cookie); */
	prHif->prFuncInfo = prFunc;
	prHif->cltCtx = *((MTK_WCN_HIF_SDIO_CLTCTX *) ulCookie);
	mtk_wcn_hif_sdio_set_drvdata(prHif->cltCtx, prGlueInfo);

#else
	prHif->func = (struct sdio_func *)ulCookie;

	/* printk(KERN_INFO DRV_NAME"prHif->func->dev = 0x%p\n", &prHif->func->dev); */
	/* printk(KERN_INFO DRV_NAME"prHif->func->vendor = 0x%04X\n", prHif->func->vendor); */
	/* printk(KERN_INFO DRV_NAME"prHif->func->device = 0x%04X\n", prHif->func->device); */
	/* printk(KERN_INFO DRV_NAME"prHif->func->func = 0x%04X\n", prHif->func->num); */

	sdio_set_drvdata(prHif->func, prGlueInfo);

	SET_NETDEV_DEV(prGlueInfo->prDevHandler, &prHif->func->dev);
#endif

	/* Reset statistic counter */
	kalMemZero(&prHif->rStatCounter, sizeof(SDIO_STAT_COUNTER_T));

	for (ucIdx = TC0_INDEX; ucIdx < TC_NUM; ucIdx++)
		prHif->au4PendingTxDoneCount[ucIdx] = 0;

	mutex_init(&prHif->rRxFreeBufQueMutex);
	mutex_init(&prHif->rRxDeAggQueMutex);
#if CFG_CHIP_RESET_SUPPORT
	rst_data.func = prGlueInfo->rHifInfo.func;
	mutex_init(&(rst_data.rst_mutex));
#endif

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
VOID glClearHifInfo(P_GLUE_INFO_T prGlueInfo)
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
BOOL glBusInit(PVOID pvData)
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

	if (ret) {
		/* printk(KERN_INFO
		 *  DRV_NAME"sdio_set_block_size 512 failed!\n");
		 */
	} else {
		/* printk(KERN_INFO
		 *  DRV_NAME"sdio_set_block_size 512 done!\n");
		 */
	}

	/* printk(KERN_INFO DRV_NAME"param: func->cur_blksize(%d)\n", func->cur_blksize); */
	/* printk(KERN_INFO DRV_NAME"param: func->max_blksize(%d)\n", func->max_blksize); */
	/* printk(KERN_INFO DRV_NAME"param: func->card->host->max_blk_size(%d)\n", func->card->host->max_blk_size); */
	/* printk(KERN_INFO DRV_NAME"param: func->card->host->max_blk_count(%d)\n", func->card->host->max_blk_count); */
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
VOID glBusRelease(PVOID pvData)
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
INT_32 glBusSetIrq(PVOID pvData, PVOID pfnIsr, PVOID pvCookie)
{
	int ret = 0;

	struct net_device *prNetDevice = NULL;
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_GL_HIF_INFO_T prHifInfo = NULL;

	ASSERT(pvData);
	if (!pvData)
		return -1;

	prNetDevice = (struct net_device *)pvData;
	prGlueInfo = (P_GLUE_INFO_T) pvCookie;
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

	prHifInfo->fgIsPendingInt = FALSE;

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
VOID glBusFreeIrq(PVOID pvData, PVOID pvCookie)
{
	struct net_device *prNetDevice = NULL;
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_GL_HIF_INFO_T prHifInfo = NULL;

	ASSERT(pvData);
	if (!pvData) {
		/* printk(KERN_INFO DRV_NAME"%s null pvData\n", __FUNCTION__); */
		return;
	}
	prNetDevice = (struct net_device *)pvData;
	prGlueInfo = (P_GLUE_INFO_T) pvCookie;
	ASSERT(prGlueInfo);
	if (!prGlueInfo) {
		/* printk(KERN_INFO DRV_NAME"%s no glue info\n", __FUNCTION__); */
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
}				/* end of glBusreeIrq() */

BOOLEAN glIsReadClearReg(UINT_32 u4Address)
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
BOOL kalDevRegRead(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Register, OUT PUINT_32 pu4Value)
{
	int ret = 0;
	UINT_8 ucRetryCount = 0;

	ASSERT(prGlueInfo);
	ASSERT(pu4Value);

	do {
#if MTK_WCN_HIF_SDIO
		ret = mtk_wcn_hif_sdio_readl(prGlueInfo->rHifInfo.cltCtx, u4Register, (PUINT_32) pu4Value);
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
#if CFG_CHIP_RESET_SUPPORT
		P_ADAPTER_T prAdapter = NULL;
#endif
		kalSendAeeWarning(HIF_SDIO_ERR_TITLE_STR,
				  HIF_SDIO_ERR_DESC_STR "sdio_readl() reports error: %x retry: %u", ret, ucRetryCount);
		DBGLOG(HAL, ERROR, "sdio_readl() reports error: %x retry: %u\n", ret, ucRetryCount);
#if CFG_CHIP_RESET_SUPPORT
		prAdapter = container_of(&prGlueInfo, ADAPTER_T, prGlueInfo);
		prAdapter->fgIsChipNoAck = TRUE;
		DBGLOG(HAL, ERROR, "fgIsChipNoAck = %d\n",
						prAdapter->fgIsChipNoAck);
		glResetTrigger(prAdapter);
#endif
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
BOOL kalDevRegRead_mac(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Register, OUT PUINT_32 pu4Value)
{
	UINT_32 value;
	UINT_32 u4Time, u4Current;
	BOOL ucRet;

    /* progrqm h2d mailbox0 as interested register address */
	kalDevRegWrite(prGlueInfo, MCR_H2DSM0R, u4Register);

    /* set h2d interrupt to notify firmware. bit16 */
	kalDevRegWrite(prGlueInfo, MCR_WSICR, SDIO_MAILBOX_FUNC_READ_REG_IDX);

	/* polling interrupt status asserted. bit16 */

	/* first, disable interrupt enable for SDIO_MAILBOX_FUNC_READ_REG_IDX */
	ucRet = kalDevRegRead(prGlueInfo, MCR_WHIER, &value);
	kalDevRegWrite(prGlueInfo, MCR_WHIER, (value & ~SDIO_MAILBOX_FUNC_READ_REG_IDX));

	u4Time = (UINT_32) kalGetTimeTick();

	do {
		/* check bit16 of WHISR assert for read register response */
		ucRet = kalDevRegRead(prGlueInfo, MCR_WHISR, &value);

		if (value & SDIO_MAILBOX_FUNC_READ_REG_IDX) {
			/* read d2h mailbox0 for interested register address */
			ucRet = kalDevRegRead(prGlueInfo, MCR_D2HRM0R, &value);

			if (value != u4Register) {
				DBGLOG(HAL, ERROR, "ERROR! kalDevRegRead_mac():register address mis-match");
				DBGLOG(HAL, ERROR, "(u4Register = 0x%08x, reported register = 0x%08x)\n",
				u4Register, value);
				return  FALSE;
			}

			/* read d2h mailbox1 for the value of the register */
			ucRet = kalDevRegRead(prGlueInfo, MCR_D2HRM1R, &value);
			*pu4Value = value;
			return	TRUE;
		}

		/* timeout exceeding check */
		u4Current = (UINT_32) kalGetTimeTick();

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
BOOL kalDevRegWrite(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Register, IN UINT_32 u4Value)
{
	int ret = 0;
	UINT_8 ucRetryCount = 0;

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
#if CFG_CHIP_RESET_SUPPORT
		P_ADAPTER_T prAdapter = NULL;
#endif
		kalSendAeeWarning(HIF_SDIO_ERR_TITLE_STR,
				  HIF_SDIO_ERR_DESC_STR "sdio_writel() reports error: %x retry: %u", ret, ucRetryCount);
		DBGLOG(HAL, ERROR, "sdio_writel() reports error: %x retry: %u\n", ret, ucRetryCount);
#if CFG_CHIP_RESET_SUPPORT
		prAdapter = container_of(&prGlueInfo, ADAPTER_T, prGlueInfo);
		prAdapter->fgIsChipNoAck = TRUE;
		DBGLOG(HAL, ERROR, "fgIsChipNoAck = %d\n",
					prAdapter->fgIsChipNoAck);
		glResetTrigger(prAdapter);
#endif
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
BOOL kalDevRegWrite_mac(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Register, IN UINT_32 u4Value)
{
	UINT_32 value;
	UINT_32 u4Time, u4Current;
	BOOL ucRet;

	/* progrqm h2d mailbox0 as interested register address */
	kalDevRegWrite(prGlueInfo, MCR_H2DSM0R, u4Register);

	/* progrqm h2d mailbox1 as the value to write */
	kalDevRegWrite(prGlueInfo, MCR_H2DSM1R, u4Value);

	/*  set h2d interrupt to notify firmware bit17 */
	kalDevRegWrite(prGlueInfo, MCR_WSICR, SDIO_MAILBOX_FUNC_WRITE_REG_IDX);

	/* polling interrupt status asserted. bit17 */

	/* first, disable interrupt enable for SDIO_MAILBOX_FUNC_WRITE_REG_IDX */
	ucRet = kalDevRegRead(prGlueInfo, MCR_WHIER, &value);
	kalDevRegWrite(prGlueInfo, MCR_WHIER, (value & ~SDIO_MAILBOX_FUNC_WRITE_REG_IDX));

	u4Time = (UINT_32) kalGetTimeTick();

	do {
		/* check bit17 of WHISR assert for response */
		ucRet = kalDevRegRead(prGlueInfo, MCR_WHISR, &value);

		if (value & SDIO_MAILBOX_FUNC_WRITE_REG_IDX) {
			/* read d2h mailbox0 for interested register address */
			ucRet = kalDevRegRead(prGlueInfo, MCR_D2HRM0R, &value);

			if (value != u4Register) {
				DBGLOG(HAL, ERROR, "ERROR! kalDevRegWrite_mac():register address mis-match");
				DBGLOG(HAL, ERROR, "(u4Register = 0x%08x, reported register = 0x%08x)\n",
				u4Register, value);
				return  FALSE;
			}
			return	TRUE;
		}

		/* timeout exceeding check */
		u4Current = (UINT_32) kalGetTimeTick();

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
BOOL
kalDevPortRead(IN P_GLUE_INFO_T prGlueInfo,
	       IN UINT_16 u2Port, IN UINT_32 u4Len, OUT PUINT_8 pucBuf, IN UINT_32 u4ValidOutBufSize)
{
	P_GL_HIF_INFO_T prHifInfo = NULL;
	PUINT_8 pucDst = NULL;
	int count = u4Len;
	int ret = 0;
	int bNum = 0;

#if (MTK_WCN_HIF_SDIO == 0)
	struct sdio_func *prSdioFunc = NULL;
#endif

#if DBG
	/* printk(KERN_INFO DRV_NAME"++kalDevPortRead++ buf:0x%p, port:0x%x, length:%d\n", pucBuf, u2Port, u4Len); */
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
		    mtk_wcn_hif_sdio_read_buf(prGlueInfo->rHifInfo.cltCtx, u2Port, (PUINT_32) pucDst,
					      ((prGlueInfo->rHifInfo).prFuncInfo->blk_sz) * bNum);
	} else {
		ret = mtk_wcn_hif_sdio_read_buf(prGlueInfo->rHifInfo.cltCtx, u2Port, (PUINT_32) pucDst, count);
	}
#endif

	if (ret) {
#if CFG_CHIP_RESET_SUPPORT
		P_ADAPTER_T prAdapter = NULL;
#endif
		kalSendAeeWarning(HIF_SDIO_ERR_TITLE_STR, HIF_SDIO_ERR_DESC_STR "sdio_readsb() reports error: %x", ret);
		DBGLOG(HAL, ERROR, "sdio_readsb() reports error: %x\n", ret);
#if CFG_CHIP_RESET_SUPPORT
		prAdapter = container_of(&prGlueInfo, ADAPTER_T, prGlueInfo);
		prAdapter->fgIsChipNoAck = TRUE;
		DBGLOG(HAL, ERROR, "fgIsChipNoAck = %d\n",
						prAdapter->fgIsChipNoAck);
		glResetTrigger(prAdapter);
#endif
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
BOOL
kalDevPortWrite(IN P_GLUE_INFO_T prGlueInfo,
		IN UINT_16 u2Port, IN UINT_32 u4Len, IN PUINT_8 pucBuf, IN UINT_32 u4ValidInBufSize)
{
	P_GL_HIF_INFO_T prHifInfo = NULL;
	PUINT_8 pucSrc = NULL;
	int count = u4Len;
	int ret = 0;
	int bNum = 0;

#if (MTK_WCN_HIF_SDIO == 0)
	struct sdio_func *prSdioFunc = NULL;
#endif

#if DBG
	/* printk(KERN_INFO DRV_NAME"++kalDevPortWrite++ buf:0x%p, port:0x%x, length:%d\n", pucBuf, u2Port, u2Len); */
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
					       (PUINT_32) pucSrc, ((prGlueInfo->rHifInfo).prFuncInfo->blk_sz) * bNum);
	} else {		/* byte mode */
		ret = mtk_wcn_hif_sdio_write_buf(prGlueInfo->rHifInfo.cltCtx, u2Port, (PUINT_32) pucSrc, count);
	}
#endif

	if (ret) {
#if CFG_CHIP_RESET_SUPPORT
		P_ADAPTER_T prAdapter = NULL;
#endif
		kalSendAeeWarning(HIF_SDIO_ERR_TITLE_STR,
				  HIF_SDIO_ERR_DESC_STR "sdio_writesb() reports error: %x", ret);
		DBGLOG(HAL, ERROR, "sdio_writesb() reports error: %x\n", ret);
#if CFG_CHIP_RESET_SUPPORT
		prAdapter = container_of(&prGlueInfo, ADAPTER_T, prGlueInfo);
		prAdapter->fgIsChipNoAck = TRUE;
		DBGLOG(HAL, ERROR, "fgIsChipNoAck = %d\n",
						prAdapter->fgIsChipNoAck);
		glResetTrigger(prAdapter);
#endif
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
VOID kalDevReadIntStatus(IN P_ADAPTER_T prAdapter, OUT PUINT_32 pu4IntStatus)
{
#if CFG_SDIO_INTR_ENHANCE
	P_SDIO_CTRL_T prSDIOCtrl;
	P_SDIO_STAT_COUNTER_T prStatCounter;

	SDIO_TIME_INTERVAL_DEC();

	DEBUGFUNC("nicSDIOReadIntStatus");

	ASSERT(prAdapter);
	ASSERT(pu4IntStatus);

	prSDIOCtrl = prAdapter->prGlueInfo->rHifInfo.prSDIOCtrl;
	ASSERT(prSDIOCtrl);

	prStatCounter = &prAdapter->prGlueInfo->rHifInfo.rStatCounter;

	/* There are pending interrupt to be handled */
	if (prAdapter->prGlueInfo->rHifInfo.fgIsPendingInt)
		prAdapter->prGlueInfo->rHifInfo.fgIsPendingInt = FALSE;
	else {
		SDIO_REC_TIME_START();
		HAL_PORT_RD(prAdapter, MCR_WHISR, sizeof(ENHANCE_MODE_DATA_STRUCT_T),
			(PUINT_8) prSDIOCtrl, sizeof(ENHANCE_MODE_DATA_STRUCT_T));
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

	*pu4IntStatus = prSDIOCtrl->u4WHISR;
#else
	HAL_MCR_RD(prAdapter, MCR_WHISR, pu4IntStatus);
#endif /* CFG_SDIO_INTR_ENHANCE */

	if (*pu4IntStatus & ~(WHIER_DEFAULT | WHIER_FW_OWN_BACK_INT_EN)) {
		DBGLOG(INTR, WARN, "Un-handled HISR %#lx, HISR = %#lx (HIER:0x%lx)\n",
		       (*pu4IntStatus & ~WHIER_DEFAULT), *pu4IntStatus, WHIER_DEFAULT);
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
BOOL kalDevWriteWithSdioCmd52(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Addr, IN UINT_8 ucData)
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
#if CFG_CHIP_RESET_SUPPORT
		P_ADAPTER_T prAdapter = container_of(&prGlueInfo, ADAPTER_T,
								prGlueInfo);
#endif
		kalSendAeeWarning(HIF_SDIO_ERR_TITLE_STR, HIF_SDIO_ERR_DESC_STR "sdio_writeb() reports error: %x", ret);
		DBGLOG(HAL, ERROR, "sdio_writeb() reports error: %x\n", ret);
#if CFG_CHIP_RESET_SUPPORT
		prAdapter->fgIsChipNoAck = TRUE;
		DBGLOG(HAL, ERROR, "fgIsChipNoAck = %d\n",
					prAdapter->fgIsChipNoAck);
		glResetTrigger(prAdapter);
#endif
	}

	return (ret) ? FALSE : TRUE;

}				/* end of kalDevWriteWithSdioCmd52() */

VOID glSetPowerState(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 ePowerMode)
{
}

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
BOOL kalDevWriteData(IN P_GLUE_INFO_T prGlueInfo, IN P_MSDU_INFO_T prMsduInfo)
{
	P_ADAPTER_T prAdapter = prGlueInfo->prAdapter;
	P_GL_HIF_INFO_T prHifInfo = &prGlueInfo->rHifInfo;
	P_TX_CTRL_T prTxCtrl;
	PUINT_8 pucOutputBuf = (PUINT_8) NULL;
	UINT_32 u4PaddingLength;
	struct sk_buff *skb;
	UINT_8 *pucBuf;
	UINT_32 u4Length;
	UINT_8 ucTC;

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
			*(PUINT_32) (&((pucOutputBuf)[ALIGN_4(prTxCtrl->u4WrIdx)])) = 0;
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
	prHifInfo->au4PendingTxDoneCount[ucTC]++;

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
BOOL kalDevKickData(IN P_GLUE_INFO_T prGlueInfo)
{
	P_ADAPTER_T prAdapter = prGlueInfo->prAdapter;
	P_GL_HIF_INFO_T prHifInfo = &prGlueInfo->rHifInfo;
	P_TX_CTRL_T prTxCtrl;
	PUINT_8 pucOutputBuf = (PUINT_8) NULL;

	prTxCtrl = &prAdapter->rTxCtrl;
	pucOutputBuf = prTxCtrl->pucTxCoalescingBufPtr;

	if (prTxCtrl->u4WrIdx == 0)
		return FALSE;

	if ((prAdapter->u4CoalescingBufCachedSize - ALIGN_4(prTxCtrl->u4WrIdx)) >= HIF_TX_TERMINATOR_LEN) {
		/* fill with single dword of zero as TX-aggregation termination */
		*(PUINT_32) (&((pucOutputBuf)[ALIGN_4(prTxCtrl->u4WrIdx)])) = 0;
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
BOOL kalDevWriteCmd(IN P_GLUE_INFO_T prGlueInfo, IN P_CMD_INFO_T prCmdInfo, IN UINT_8 ucTC)
{
	P_ADAPTER_T prAdapter = prGlueInfo->prAdapter;
/*	P_GL_HIF_INFO_T prHifInfo = &prGlueInfo->rHifInfo; */
	P_TX_CTRL_T prTxCtrl;
	PUINT_8 pucOutputBuf = (PUINT_8) NULL;
	UINT_16 u2OverallBufferLength = 0;
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
		*(PUINT_32) (&((pucOutputBuf)[ALIGN_4(u2OverallBufferLength)])) = 0;
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
	prGlueInfo->rHifInfo.au4PendingTxDoneCount[ucTC]++;

	prGlueInfo->rHifInfo.rStatCounter.u4CmdPktWriteCnt++;
	return TRUE;
}

void glGetDev(PVOID ctx, struct device **dev)
{
#if MTK_WCN_HIF_SDIO
	mtk_wcn_hif_sdio_get_dev(*((MTK_WCN_HIF_SDIO_CLTCTX *) ctx), dev);
#else
	*dev = &((struct sdio_func *)ctx)->dev;
#endif
}

void glGetHifDev(P_GL_HIF_INFO_T prHif, struct device **dev)
{
#if MTK_WCN_HIF_SDIO
	mtk_wcn_hif_sdio_get_dev(prHif->cltCtx, dev);
#else
	*dev = &(prHif->func->dev);
#endif
}

BOOLEAN glWakeupSdio(P_GLUE_INFO_T prGlueInfo)
{
	BOOLEAN fgSuccess = TRUE;

#if (HIF_SDIO_SUPPORT_GPIO_SLEEP_MODE && MTK_WCN_HIF_SDIO)
	if (mtk_wcn_hif_sdio_wake_up_ctrl(prGlueInfo->rHifInfo.cltCtx) != 0)
		fgSuccess = FALSE;
#endif

	return fgSuccess;
}

