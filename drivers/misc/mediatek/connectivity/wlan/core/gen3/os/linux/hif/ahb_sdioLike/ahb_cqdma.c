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
#include "hif_cqdma.h"


/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
/* #define DMA_DEBUG_SUP */
#define DMA_TAG             "CQDMA> "

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
static VOID HifCqdmaConfig(IN GL_HIF_INFO_T *HifInfo, IN MTK_WCN_HIF_DMA_CONF *Conf);

static VOID HifCqdmaStart(IN GL_HIF_INFO_T *HifInfo);

static VOID HifCqdmaStop(IN GL_HIF_INFO_T *HifInfo);

static BOOL HifCqdmaPollStart(IN GL_HIF_INFO_T *HifInfo);

static BOOL HifCqdmaPollIntr(IN GL_HIF_INFO_T *HifInfo);

static VOID HifCqdmaAckIntr(IN GL_HIF_INFO_T *HifInfo);

static VOID HifCqdmaClockCtrl(IN GL_HIF_INFO_T *HifInfo, IN BOOL fgEnable);

static VOID HifCqdmaRegDump(IN GL_HIF_INFO_T *HifInfo);

static VOID HifCqdmaReset(IN GL_HIF_INFO_T *HifInfo);

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/
GL_HIF_DMA_OPS_T HifCqdmaOps = {
	.DmaConfig = HifCqdmaConfig,
	.DmaStart = HifCqdmaStart,
	.DmaStop = HifCqdmaStop,
	.DmaPollStart = HifCqdmaPollStart,
	.DmaPollIntr = HifCqdmaPollIntr,
	.DmaAckIntr = HifCqdmaAckIntr,
	.DmaClockCtrl = HifCqdmaClockCtrl,
	.DmaRegDump = HifCqdmaRegDump,
	.DmaReset = HifCqdmaReset
};

/*******************************************************************************
*                        P U B L I C   F U N C T I O N S
********************************************************************************
*/

VOID HifDmaInit(GL_HIF_INFO_T *HifInfo)
{
	/*
	 * CQDMA H/W can access 36-bit physical address, set DMA_MASK to 36 bit
	 * to avoid bounce buffering when using DMA.
	 * (If the physical region of allocated DRAM memory is considered not
	 * reachable by the device, dma_map_single will return a bounce buffer
	 * from IOTLB, device can only perform DMA to the bounce buffer, kernel
	 * does the extra copy between bounce buffer and VA is needed.)
	 */
	dma_set_mask(HifInfo->Dev, DMA_BIT_MASK(36));

	/* IO remap DMA register memory */
#ifdef CONFIG_OF
	HifInfo->DmaRegBaseAddr = (PUINT_8)of_iomap(HifInfo->Dev->of_node, 1);
#else
	HifInfo->DmaRegBaseAddr = ioremap(CQ_DMA_HIF_BASE, CQ_DMA_HIF_LENGTH);
#endif
	/* Assign DMA operators */
	HifInfo->DmaOps = &HifCqdmaOps;

	/* Enable DMA mode */
	HifInfo->fgDmaEnable = TRUE;

#if !defined(CONFIG_MTK_CLKMGR)
#ifdef CONFIG_OF
	HifInfo->clk_wifi_dma = devm_clk_get(HifInfo->Dev, "wifi-dma");
	if (IS_ERR(HifInfo->clk_wifi_dma)) {
		DBGLOG(INIT, ERROR, "[CCF]Cannot get HIF DMA clock\n");
		/* return PTR_ERR(HifInfo->clk_wifi_dma); */
	}
	DBGLOG(INIT, TRACE, "[CCF]HIF DMA clock = %p\n", HifInfo->clk_wifi_dma);
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
* \brief Config CQDMA TX/RX behavior.
*
* \param[in] HifInfo            Pointer to the GL_HIF_INFO_T structure.
* \param[in] Param              Pointer to the settings.
*
* \retval NONE
*/
/*----------------------------------------------------------------------------*/
static VOID HifCqdmaConfig(IN GL_HIF_INFO_T *HifInfo, IN MTK_WCN_HIF_DMA_CONF *Conf)
{
	UINT_32 RegVal;
	ULONG addr_h;

	/* Assign fixed value */
	Conf->Burst = HIF_CQDMA_BURST_8_8;
	Conf->Fix_en = FALSE;
	Conf->Connect = HIF_CQDMA_CONNECT_SET1;

	/* CQ_DMA_HIF_CON */
	DMA_DBG("Conf->Dir = %d\n", Conf->Dir);

	RegVal = HIF_DMAR_READL(HifInfo, CQ_DMA_HIF_CON);
	RegVal &= ~(CDH_CR_FIX8 | CDH_CR_BURST_LEN | CDH_CR_WRAP_EN | CDH_CR_WADDR_FIX_EN |
		    CDH_CR_RADDR_FIX_EN | CDH_CR_FIX_EN | CDH_CR_RESIDUE);
	RegVal |= CDH_CR_FIX8;
	RegVal |= (((Conf->Burst << CDH_CR_BURST_LEN_OFFSET) & CDH_CR_BURST_LEN) |
		   ((Conf->Fix_en << CDH_CR_FIX_EN_OFFSET) & CDH_CR_FIX_EN));
	if (Conf->Dir == HIF_DMA_DIR_RX)
		RegVal |= CDH_CR_RESIDUE; /* CQDMA fix length 8 byte align to 4 byte align */

	HIF_DMAR_WRITEL(HifInfo, CQ_DMA_HIF_CON, RegVal);
	DMA_DBG("CQ_DMA_HIF_CON = 0x%08x\n", RegVal);

	/* CQ_DMA_HIF_CONNECT */
	RegVal = HIF_DMAR_READL(HifInfo, CQ_DMA_HIF_CONNECT);
	RegVal &= ~(CDH_CR_DIR | CDH_CR_CONNECT);
	RegVal |= (((Conf->Dir << CDH_CR_DIR_OFFSET) & CDH_CR_DIR) |
		   ((Conf->Connect) & CDH_CR_CONNECT));
	HIF_DMAR_WRITEL(HifInfo, CQ_DMA_HIF_CONNECT, RegVal);
	DMA_DBG("CQ_DMA_HIF_CONNECT = 0x%08x\n", RegVal);

	/* CQ_DMA_HIF_SRC_ADDR2 */
	/* ulong is 32 bit on ILP32, and 64 bit on LP64. So on 32-bit OS platform,
	 * right shift 32 to ulong is forbidden, we just right shift 16 twice to replace.
	 * For 32-bit address, it should be 0 to fill in ADDR2 register.
	 */
	addr_h = Conf->Src >> 16;
	addr_h = addr_h >> 16;
	HIF_DMAR_WRITEL(HifInfo, CQ_DMA_HIF_SRC_ADDR2, (UINT_32)addr_h);
	DMA_DBG("CQ_DMA_HIF_SRC_ADDR2 = 0x%08x\n", (UINT_32)addr_h);

	/* CQ_DMA_HIF_SRC_ADDR */
	HIF_DMAR_WRITEL(HifInfo, CQ_DMA_HIF_SRC_ADDR, (UINT_32)Conf->Src);
	DMA_DBG("CQ_DMA_HIF_SRC_ADDR = 0x%08x\n", (UINT_32)Conf->Src);

	/* CQ_DMA_HIF_DST_ADDR2 */
	/* ulong is 32 bit on ILP32, and 64 bit on LP64. So on 32-bit OS platform,
	 * right shift 32 to ulong is forbidden, we just right shift 16 twice to replace.
	 * For 32-bit address, it should be 0 to fill in ADDR2 register.
	 */
	addr_h = Conf->Dst >> 16;
	addr_h = addr_h >> 16;
	HIF_DMAR_WRITEL(HifInfo, CQ_DMA_HIF_DST_ADDR2, (UINT_32)addr_h);
	DMA_DBG("CQ_DMA_HIF_DST_ADDR2 = 0x%08x\n", (UINT_32)addr_h);

	/* CQ_DMA_HIF_DST_ADDR */
	HIF_DMAR_WRITEL(HifInfo, CQ_DMA_HIF_DST_ADDR, (UINT_32)Conf->Dst);
	DMA_DBG("CQ_DMA_HIF_DST_ADDR = 0x%08x\n", (UINT_32)Conf->Dst);

	/* CQ_DMA_HIF_LEN1 */
	HIF_DMAR_WRITEL(HifInfo, CQ_DMA_HIF_LEN1, (UINT_32)(Conf->Count & CDH_CR_LEN));
	DMA_DBG("CQ_DMA_HIF_LEN1 = %u\n", (UINT_32)(Conf->Count & CDH_CR_LEN));
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Start CQDMA TX/RX.
*
* \param[in] HifInfo            Pointer to the GL_HIF_INFO_T structure.
*
* \retval NONE
*/
/*----------------------------------------------------------------------------*/
static VOID HifCqdmaStart(IN GL_HIF_INFO_T *HifInfo)
{
	UINT_32 RegVal;

	/* Enable interrupt */
	RegVal = HIF_DMAR_READL(HifInfo, CQ_DMA_HIF_INT_EN);
	HIF_DMAR_WRITEL(HifInfo, CQ_DMA_HIF_INT_EN, (RegVal | CDH_CR_INTEN_FLAG));

	/* Start DMA */
	RegVal = HIF_DMAR_READL(HifInfo, CQ_DMA_HIF_EN);
	HIF_DMAR_WRITEL(HifInfo, CQ_DMA_HIF_EN, (RegVal | CDH_CR_EN));

	DMA_DBG("HIF DMA start...\n");
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Stop CQDMA TX/RX.
*
* \param[in] HifInfo            Pointer to the GL_HIF_INFO_T structure.
*
* \retval NONE
*/
/*----------------------------------------------------------------------------*/
static VOID HifCqdmaStop(IN GL_HIF_INFO_T *HifInfo)
{
	UINT_32 RegVal, LoopCnt;

	/* Disable interrupt */
	RegVal = HIF_DMAR_READL(HifInfo, CQ_DMA_HIF_INT_EN);
	HIF_DMAR_WRITEL(HifInfo, CQ_DMA_HIF_INT_EN, (RegVal & ~(CDH_CR_INTEN_FLAG)));

	/* Confirm DMA is stopped */
	if (HifCqdmaPollStart(HifInfo)) {
		RegVal = HIF_DMAR_READL(HifInfo, CQ_DMA_HIF_STOP);
		HIF_DMAR_WRITEL(HifInfo, CQ_DMA_HIF_STOP, (RegVal | CDH_CR_STOP));

		/* Polling START bit turn to 0 */
		LoopCnt = 0;
		do {
			if (LoopCnt++ > 10000) {
				/* Stop DMA failed, try to reset DMA */
				HifCqdmaReset(HifInfo);
				break;
			}
		} while (HifCqdmaPollStart(HifInfo));
	}
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Poll CQDMA enable bit.
*
* \param[in] HifInfo            Pointer to the GL_HIF_INFO_T structure.
*
* \retval TRUE    DMA is running
*         FALSE   DMA is stopped
*/
/*----------------------------------------------------------------------------*/
static BOOL HifCqdmaPollStart(IN GL_HIF_INFO_T *HifInfo)
{
	UINT_32 RegVal;

	RegVal = HIF_DMAR_READL(HifInfo, CQ_DMA_HIF_EN);
	return ((RegVal & CDH_CR_EN) != 0) ? TRUE : FALSE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Poll CQDMA interrupt flag.
*
* \param[in] HifInfo            Pointer to the GL_HIF_INFO_T structure.
*
* \retval TRUE    DMA finish
*         FALSE
*/
/*----------------------------------------------------------------------------*/
static BOOL HifCqdmaPollIntr(IN GL_HIF_INFO_T *HifInfo)
{
	UINT_32 RegVal;

	RegVal = HIF_DMAR_READL(HifInfo, CQ_DMA_HIF_INT_FLAG);
	return ((RegVal & CDH_CR_INT_FLAG) != 0) ? TRUE : FALSE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Acknowledge CQDMA interrupt flag.
*
* \param[in] HifInfo            Pointer to the GL_HIF_INFO_T structure.
*
* \retval NONE
*/
/*----------------------------------------------------------------------------*/
static VOID HifCqdmaAckIntr(IN GL_HIF_INFO_T *HifInfo)
{
	UINT_32 RegVal;

	/* Write 0 to clear interrupt */
	RegVal = HIF_DMAR_READL(HifInfo, CQ_DMA_HIF_INT_FLAG);
	HIF_DMAR_WRITEL(HifInfo, CQ_DMA_HIF_INT_FLAG, (RegVal & ~CDH_CR_INT_FLAG));
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Enable/disable CQDMA clock.
*
* \param[in] HifInfo            Pointer to the GL_HIF_INFO_T structure.
* \param[in] fgEnable           TRUE: enable; FALSE: disable
*
* \retval NONE
*/
/*----------------------------------------------------------------------------*/
static VOID HifCqdmaClockCtrl(IN GL_HIF_INFO_T *HifInfo, IN BOOL fgEnable)
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
* \brief Dump CQDMA related registers when abnormal, such as DMA timeout.
*
* \param[in] HifInfo            Pointer to the GL_HIF_INFO_T structure.
*
* \retval NONE
*/
/*----------------------------------------------------------------------------*/
static VOID HifCqdmaRegDump(IN GL_HIF_INFO_T *HifInfo)
{
	UINT_32 RegVal[4], RegOffset, Length;
	UINT_32 idx = 0;

	Length = ((CQ_DMA_HIF_LENGTH + 15) & ~15u); /* Dump 16 bytes alignment length */

	for (RegOffset = 0; RegOffset < Length; RegOffset += 4) {
		RegVal[idx] = HIF_DMAR_READL(HifInfo, RegOffset);

		if (idx++ >= 3) {
			DBGLOG(HAL, INFO, DMA_TAG "DUMP32 ADDRESS: 0x%08x\n", CQ_DMA_HIF_BASE + RegOffset - 12);
			DBGLOG(HAL, INFO, "\t%08x %08x %08x %08x\n", RegVal[0], RegVal[1], RegVal[2], RegVal[3]);
			idx = 0;
		}
	}
}

/*----------------------------------------------------------------------------*/
/*!
* \brief Reset CQDMA.
*
* \param[in] HifInfo            Pointer to the GL_HIF_INFO_T structure.
*
* \retval NONE
*/
/*----------------------------------------------------------------------------*/
static VOID HifCqdmaReset(IN GL_HIF_INFO_T *HifInfo)
{
	UINT_32 RegVal, LoopCnt;

	/* Do warm reset: DMA will wait for current transaction finished */
	DBGLOG(HAL, INFO, DMA_TAG "do warm reset...\n");

	/* Normally, we need to make sure that bit0 of CQ_DMA_HIF_EN is 1 here */

	RegVal = HIF_DMAR_READL(HifInfo, CQ_DMA_HIF_RST);
	HIF_DMAR_WRITEL(HifInfo, CQ_DMA_HIF_RST, (RegVal | CDH_CR_WARM_RST));

	for (LoopCnt = 0; LoopCnt < 10000; LoopCnt++) {
		if (!HifCqdmaPollStart(HifInfo))
			break;	/* reset ok */
	}

	if (HifCqdmaPollStart(HifInfo)) {
		/* Do hard reset if warm reset fails */
		DBGLOG(HAL, INFO, DMA_TAG "do hard reset...\n");
		RegVal = HIF_DMAR_READL(HifInfo, CQ_DMA_HIF_RST);
		HIF_DMAR_WRITEL(HifInfo, CQ_DMA_HIF_RST, (RegVal | CDH_CR_HARD_RST));
		HIF_DMAR_WRITEL(HifInfo, CQ_DMA_HIF_RST, (RegVal & ~CDH_CR_HARD_RST));
	}
}

