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

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

#include <linux/version.h>	/* constant of kernel version */

#include <linux/kernel.h>	/* bitops.h */

#include <linux/timer.h>	/* struct timer_list */
#include <linux/jiffies.h>	/* jiffies */
#include <linux/delay.h>	/* udelay and mdelay macro */

#if CONFIG_ANDROID
#ifdef CONFIG_WAKELOCK
#include <linux/wakelock.h>
#else
#include <linux/device.h>
#endif
#endif

#include <linux/irq.h>		/* IRQT_FALLING */

#include <linux/netdevice.h>	/* struct net_device, struct net_device_stats */
#include <linux/etherdevice.h>	/* for eth_type_trans() function */
#include <linux/wireless.h>	/* struct iw_statistics */
#include <linux/if_arp.h>
#include <linux/inetdevice.h>	/* struct in_device */

#include <linux/ip.h>		/* struct iphdr */

#include <linux/string.h>	/* for memcpy()/memset() function */
#include <linux/stddef.h>	/* for offsetof() macro */

#include <linux/proc_fs.h>	/* The proc filesystem constants/structures */

#include <linux/rtnetlink.h>	/* for rtnl_lock() and rtnl_unlock() */
#include <linux/kthread.h>	/* kthread_should_stop(), kthread_run() */
#include <linux/uaccess.h>	/* for copy_from_user() */
#include <linux/fs.h>		/* for firmware download */
#include <linux/vmalloc.h>

#include <linux/kfifo.h>	/* for kfifo interface */
#include <linux/cdev.h>		/* for cdev interface */

#include <linux/firmware.h>	/* for firmware download */

#include <linux/random.h>

#include <linux/io.h>		/* readw and writew */

#include <linux/module.h>
#include <linux/errno.h>

#if defined(CONFIG_MTK_CLKMGR)
#include <mach/mt_clkmgr.h>
#else
#include <linux/clk.h>
#endif /* defined(CONFIG_MTK_CLKMGR) */

#ifdef CONFIG_OF
#include <linux/of_address.h>
#endif

#include "gl_os.h"
#include "hif.h"
#include "hif_pdma.h"


/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
/* #define DMA_DEBUG_SUP */
#define DMA_TAG             "PDMA> "

#ifdef DMA_DEBUG_SUP
#define DMA_DBG(_fmt, ...)  pr_info(DMA_TAG _fmt, ##__VA_ARGS__)
#else
#define DMA_DBG(_fmt, ...)
#endif /* DMA_DEBUG_SUP */

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
static VOID HifPdmaConfig(IN GL_HIF_INFO_T *HifInfo, IN MTK_WCN_HIF_DMA_CONF *Conf);

static VOID HifPdmaStart(IN GL_HIF_INFO_T *HifInfo);

static VOID HifPdmaStop(IN GL_HIF_INFO_T *HifInfo);

static BOOL HifPdmaPollStart(IN GL_HIF_INFO_T *HifInfo);

static BOOL HifPdmaPollIntr(IN GL_HIF_INFO_T *HifInfo);

static VOID HifPdmaAckIntr(IN GL_HIF_INFO_T *HifInfo);

static VOID HifPdmaClockCtrl(IN GL_HIF_INFO_T *HifInfo, IN BOOL fgEnable);

static VOID HifPdmaRegDump(IN GL_HIF_INFO_T *HifInfo);

static VOID HifPdmaReset(IN GL_HIF_INFO_T *HifInfo);

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/
GL_HIF_DMA_OPS_T HifPdmaOps = {
	.DmaConfig = HifPdmaConfig,
	.DmaStart = HifPdmaStart,
	.DmaStop = HifPdmaStop,
	.DmaPollStart = HifPdmaPollStart,
	.DmaPollIntr = HifPdmaPollIntr,
	.DmaAckIntr = HifPdmaAckIntr,
	.DmaClockCtrl = HifPdmaClockCtrl,
	.DmaRegDump = HifPdmaRegDump,
	.DmaReset = HifPdmaReset
};

/*******************************************************************************
*                        P U B L I C   F U N C T I O N S
********************************************************************************
*/

VOID HifDmaInit(GL_HIF_INFO_T *HifInfo)
{
	/* IO remap DMA register memory */
#ifdef CONFIG_OF
	HifInfo->DmaRegBaseAddr = (PUINT_8)of_iomap(HifInfo->Dev->of_node, 1);
#else
	HifInfo->DmaRegBaseAddr = ioremap(AP_DMA_HIF_BASE, AP_DMA_HIF_0_LENGTH);
#endif

	/* Assign DMA operators */
	HifInfo->DmaOps = &HifPdmaOps;

	/* Enable DMA mode */
	HifInfo->fgDmaEnable = TRUE;

#if !defined(CONFIG_MTK_CLKMGR)
#ifdef CONFIG_OF
	HifInfo->clk_wifi_dma = devm_clk_get(HifInfo->Dev, "wifi-dma");
	if (IS_ERR(HifInfo->clk_wifi_dma)) {
		DBGLOG(INIT, ERROR, "[CCF]Cannot get HIF DMA clock\n");
		/* return PTR_ERR(HifInfo->clk_wifi_dma); */
	}
	DBGLOG(INIT, INFO, "[CCF]HIF DMA clock = %p\n", HifInfo->clk_wifi_dma);
#endif
#endif

	DMA_DBG("HIF DMA init ok!\n");
}

VOID HifDmaUnInit(GL_HIF_INFO_T *HifInfo)
{
	iounmap(HifInfo->DmaRegBaseAddr);
}

/*******************************************************************************
*                       P R I V A T E   F U N C T I O N S
********************************************************************************
*/

/*----------------------------------------------------------------------------*/
/*!
* \brief Config PDMA TX/RX behavior.
*
* \param[in] HifInfo            Pointer to the GL_HIF_INFO_T structure.
* \param[in] Param              Pointer to the settings.
*
* \retval NONE
*/
/*----------------------------------------------------------------------------*/
static VOID HifPdmaConfig(IN GL_HIF_INFO_T *HifInfo, IN MTK_WCN_HIF_DMA_CONF *Conf)
{
	UINT_32 RegVal;

	/* Assign fixed value */
	Conf->Burst = HIF_PDMA_BURST_4_4;	/* vs. HIF_BURST_4DW */
	Conf->Fix_en = FALSE;

	/* AP_DMA_HIF_0_CON */
	DMA_DBG("Conf->Dir = %d\n", Conf->Dir);

	/* AP_DMA_HIF_0_CON */
	RegVal = HIF_DMAR_READL(HifInfo, AP_DMA_HIF_0_CON);
	RegVal &= ~(ADH_CR_BURST_INCR | ADH_CR_BURST_LEN | ADH_CR_FIX_EN | ADH_CR_DIR);
	RegVal |= (((Conf->Burst << ADH_CR_BURST_LEN_OFFSET) & ADH_CR_BURST_LEN) |
		   (Conf->Fix_en << ADH_CR_FIX_EN_OFFSET) | (Conf->Dir));
	RegVal |= ADH_CR_BURST_INCR;

	HIF_DMAR_WRITEL(HifInfo, AP_DMA_HIF_0_CON, RegVal);
	DMA_DBG("AP_DMA_HIF_0_CON = 0x%08x\n", RegVal);

	/* AP_DMA_HIF_0_SRC_ADDR */
	HIF_DMAR_WRITEL(HifInfo, AP_DMA_HIF_0_SRC_ADDR, Conf->Src);
	DMA_DBG("AP_DMA_HIF_0_SRC_ADDR = 0x%08lx\n", Conf->Src);

	/* AP_DMA_HIF_0_DST_ADDR */
	HIF_DMAR_WRITEL(HifInfo, AP_DMA_HIF_0_DST_ADDR, Conf->Dst);
	DMA_DBG("AP_DMA_HIF_0_DST_ADDR = 0x%08lx\n", Conf->Dst);

	/* AP_DMA_HIF_0_LEN */
	HIF_DMAR_WRITEL(HifInfo, AP_DMA_HIF_0_LEN, (Conf->Count & ADH_CR_LEN));
	DMA_DBG("AP_DMA_HIF_0_LEN = %u\n", Conf->Count & ADH_CR_LEN);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Start PDMA TX/RX.
*
* \param[in] HifInfo            Pointer to the GL_HIF_INFO_T structure.
*
* \retval NONE
*/
/*----------------------------------------------------------------------------*/
static VOID HifPdmaStart(IN GL_HIF_INFO_T *HifInfo)
{
	UINT_32 RegVal;

	RegVal = HIF_DMAR_READL(HifInfo, AP_DMA_HIF_0_SRC_ADDR2);
	HIF_DMAR_WRITEL(HifInfo, AP_DMA_HIF_0_SRC_ADDR2, (RegVal | ADH_CR_SRC_ADDR2));
	RegVal = HIF_DMAR_READL(HifInfo, AP_DMA_HIF_0_DST_ADDR2);
	HIF_DMAR_WRITEL(HifInfo, AP_DMA_HIF_0_DST_ADDR2, (RegVal | ADH_CR_DST_ADDR2));

	/* Enable interrupt */
	RegVal = HIF_DMAR_READL(HifInfo, AP_DMA_HIF_0_INT_EN);
	HIF_DMAR_WRITEL(HifInfo, AP_DMA_HIF_0_INT_EN, (RegVal | ADH_CR_INTEN_FLAG_0));

	/* Start DMA */
	RegVal = HIF_DMAR_READL(HifInfo, AP_DMA_HIF_0_EN);
	HIF_DMAR_WRITEL(HifInfo, AP_DMA_HIF_0_EN, (RegVal | ADH_CR_EN));

	DMA_DBG("HIF DMA start...\n");
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Stop PDMA TX/RX.
*
* \param[in] HifInfo            Pointer to the GL_HIF_INFO_T structure.
*
* \retval NONE
*/
/*----------------------------------------------------------------------------*/
static VOID HifPdmaStop(IN GL_HIF_INFO_T *HifInfo)
{
	UINT_32 RegVal, LoopCnt;

	/* Disable interrupt */
	RegVal = HIF_DMAR_READL(HifInfo, AP_DMA_HIF_0_INT_EN);
	HIF_DMAR_WRITEL(HifInfo, AP_DMA_HIF_0_INT_EN, (RegVal & ~(ADH_CR_INTEN_FLAG_0)));

	/* Confirm DMA is stopped */
	if (HifPdmaPollStart(HifInfo)) {
		RegVal = HIF_DMAR_READL(HifInfo, AP_DMA_HIF_0_STOP);
		HIF_DMAR_WRITEL(HifInfo, AP_DMA_HIF_0_STOP, (RegVal | ADH_CR_STOP));

		/* Polling START bit turn to 0 */
		LoopCnt = 0;
		do {
			if (LoopCnt++ > 10000) {
				/* Stop DMA failed, try to reset DMA */
				HifPdmaReset(HifInfo);
				break;
			}
		} while (HifPdmaPollStart(HifInfo));
	}
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Poll PDMA enable bit.
*
* \param[in] HifInfo            Pointer to the GL_HIF_INFO_T structure.
*
* \retval TRUE    DMA is running
*         FALSE   DMA is stopped
*/
/*----------------------------------------------------------------------------*/
static BOOL HifPdmaPollStart(IN GL_HIF_INFO_T *HifInfo)
{
	UINT_32 RegVal;

	RegVal = HIF_DMAR_READL(HifInfo, AP_DMA_HIF_0_EN);
	return ((RegVal & ADH_CR_EN) != 0) ? TRUE : FALSE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Poll PDMA interrupt flag.
*
* \param[in] HifInfo            Pointer to the GL_HIF_INFO_T structure.
*
* \retval TRUE    DMA finish
*         FALSE
*/
/*----------------------------------------------------------------------------*/
static BOOL HifPdmaPollIntr(IN GL_HIF_INFO_T *HifInfo)
{
	UINT_32 RegVal;

	RegVal = HIF_DMAR_READL(HifInfo, AP_DMA_HIF_0_INT_FLAG);
	return ((RegVal & ADH_CR_FLAG_0) != 0) ? TRUE : FALSE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Acknowledge PDMA interrupt flag.
*
* \param[in] HifInfo            Pointer to the GL_HIF_INFO_T structure.
*
* \retval NONE
*/
/*----------------------------------------------------------------------------*/
static VOID HifPdmaAckIntr(IN GL_HIF_INFO_T *HifInfo)
{
	UINT_32 RegVal;

	/* Write 0 to clear interrupt */
	RegVal = HIF_DMAR_READL(HifInfo, AP_DMA_HIF_0_INT_FLAG);
	HIF_DMAR_WRITEL(HifInfo, AP_DMA_HIF_0_INT_FLAG, (RegVal & ~ADH_CR_FLAG_0));
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Enable/disable PDMA clock.
*
* \param[in] HifInfo            Pointer to the GL_HIF_INFO_T structure.
* \param[in] fgEnable           TRUE: enable; FALSE: disable
*
* \retval NONE
*/
/*----------------------------------------------------------------------------*/
static VOID HifPdmaClockCtrl(IN GL_HIF_INFO_T *HifInfo, IN BOOL fgEnable)
{
#if defined(CONFIG_MTK_CLKMGR)
	if (fgEnable == TRUE)
		enable_clock(MT_CG_PERI_APDMA, "WLAN");
	else
		disable_clock(MT_CG_PERI_APDMA, "WLAN");
#else
#if CONFIG_OF
{
	int ret = 0;

	if (IS_ERR_OR_NULL(HifInfo->clk_wifi_dma))
		return;

	if (fgEnable == TRUE) {
		ret = clk_prepare_enable(HifInfo->clk_wifi_dma);
		if (ret)
			DBGLOG(HAL, WARN, "[CCF]clk_prepare_enable return %d\n", ret);
	} else
		clk_disable_unprepare(HifInfo->clk_wifi_dma);
}
#endif
#endif
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Dump PDMA related registers when abnormal, such as DMA timeout.
*
* \param[in] HifInfo            Pointer to the GL_HIF_INFO_T structure.
*
* \retval NONE
*/
/*----------------------------------------------------------------------------*/
static VOID HifPdmaRegDump(IN GL_HIF_INFO_T *HifInfo)
{
	UINT_32 RegVal[4], RegOffset, Length;
	UINT_32 idx = 0;

	Length = ((AP_DMA_HIF_0_LENGTH + 15) & ~15u); /* Dump 16 bytes alignment length */

	for (RegOffset = 0; RegOffset < Length; RegOffset += 4) {
		RegVal[idx] = HIF_DMAR_READL(HifInfo, RegOffset);

		if (idx++ >= 3) {
			DBGLOG(HAL, INFO, DMA_TAG "DUMP32 ADDRESS: 0x%08x\n", AP_DMA_HIF_BASE + RegOffset - 12);
			DBGLOG(HAL, INFO, "\t%08x %08x %08x %08x\n", RegVal[0], RegVal[1], RegVal[2], RegVal[3]);
			idx = 0;
		}
	}
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Reset PDMA.
*
* \param[in] HifInfo            Pointer to the GL_HIF_INFO_T structure.
*
* \retval NONE
*/
/*----------------------------------------------------------------------------*/
static VOID HifPdmaReset(IN GL_HIF_INFO_T *HifInfo)
{
	UINT_32 RegVal, LoopCnt;

	/* Do warm reset: DMA will wait for current transaction finished */
	DBGLOG(HAL, INFO, DMA_TAG "do warm reset...\n");

	/* Normally, we need to make sure that bit0 of AP_DMA_HIF_0_EN is 1 here */

	RegVal = HIF_DMAR_READL(HifInfo, AP_DMA_HIF_0_RST);
	HIF_DMAR_WRITEL(HifInfo, AP_DMA_HIF_0_RST, (RegVal | ADH_CR_WARM_RST));

	for (LoopCnt = 0; LoopCnt < 10000; LoopCnt++) {
		if (!HifPdmaPollStart(HifInfo))
			break;	/* reset ok */
	}

	if (HifPdmaPollStart(HifInfo)) {
		/* Do hard reset if warm reset fails */
		DBGLOG(HAL, INFO, DMA_TAG "do hard reset...\n");
		RegVal = HIF_DMAR_READL(HifInfo, AP_DMA_HIF_0_RST);
		HIF_DMAR_WRITEL(HifInfo, AP_DMA_HIF_0_RST, (RegVal | ADH_CR_HARD_RST));
		HIF_DMAR_WRITEL(HifInfo, AP_DMA_HIF_0_RST, (RegVal & ~ADH_CR_HARD_RST));
	}
}

