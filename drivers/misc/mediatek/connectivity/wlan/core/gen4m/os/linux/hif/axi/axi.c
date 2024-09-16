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
 *[File]             axi.c
 *[Version]          v1.0
 *[Revision Date]    2010-03-01
 *[Author]
 *[Description]
 *    The program provides AXI HIF driver
 *[Copyright]
 *    Copyright (C) 2010 MediaTek Incorporation. All Rights Reserved.
 ******************************************************************************/


/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */

#include "gl_os.h"

#include "hif_pdma.h"

#include "precomp.h"

#include <linux/mm.h>
#include <linux/platform_device.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#ifndef CONFIG_X86
#include <asm/memory.h>
#endif
#include <linux/of_device.h>
#include <linux/of_reserved_mem.h>

#include "mt66xx_reg.h"

#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of.h>

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */

static const struct platform_device_id mtk_axi_ids[] = {
	{	.name = "CONNAC",
#ifdef CONNAC
		.driver_data = (kernel_ulong_t)&mt66xx_driver_data_connac},
#endif /* CONNAC */
#ifdef CONNAC2X2
		.driver_data = (kernel_ulong_t)&mt66xx_driver_data_connac2x2},
#endif /* CONNAC2X2 */
#ifdef SOC3_0
		.driver_data = (kernel_ulong_t)&mt66xx_driver_data_soc3_0},
#endif /* SOC3_0 */

	{ /* end: all zeroes */ },
};

MODULE_DEVICE_TABLE(axi, mtk_axi_ids);

#ifdef CONFIG_OF
const struct of_device_id mtk_axi_of_ids[] = {
	{.compatible = "mediatek,wifi",},
	{}
};
#endif

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */
static probe_card pfWlanProbe;
static remove_card pfWlanRemove;

static struct platform_driver mtk_axi_driver = {
	.driver = {
		.name = "wlan",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = mtk_axi_of_ids,
#endif
	},
	.id_table = mtk_axi_ids,
	.probe = NULL,
	.remove = NULL,
};

struct platform_device *g_prPlatDev;

static struct GLUE_INFO *g_prGlueInfo;
static void *CSRBaseAddress;
static u64 g_u8CsrOffset;
static u32 g_u4CsrSize;
static u_int8_t g_fgDriverProbed = FALSE;

#if AXI_CFG_PREALLOC_MEMORY_BUFFER
struct HIF_PREALLOC_MEM grMem;
unsigned long long gWifiRsvMemSize;

struct wifi_rsrv_mem {
	phys_addr_t phy_base;
	void *vir_base;
	unsigned long long size;
};

/* Assume reserved memory size < BIT(32) */
static struct wifi_rsrv_mem wifi_rsrv_mems[32];
#endif

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */
#if AXI_CFG_PREALLOC_MEMORY_BUFFER
static int _init_resv_mem(struct platform_device *pdev);

static void axiAllocTxDesc(struct GL_HIF_INFO *prHifInfo,
			   struct RTMP_DMABUF *prDescRing,
			   uint32_t u4Num);
static void axiAllocRxDesc(struct GL_HIF_INFO *prHifInfo,
			   struct RTMP_DMABUF *prDescRing,
			   uint32_t u4Num);
static bool axiAllocTxCmdBuf(struct RTMP_DMABUF *prDmaBuf,
			     uint32_t u4Num, uint32_t u4Idx);
#else
static void axiAllocDesc(struct GL_HIF_INFO *prHifInfo,
			 struct RTMP_DMABUF *prDescRing,
			 uint32_t u4Num);
static void *axiAllocRuntimeMem(uint32_t u4SrcLen);
static phys_addr_t axiMapTxBuf(struct GL_HIF_INFO *prHifInfo,
			       void *pucBuf, uint32_t u4Offset, uint32_t u4Len);
static phys_addr_t axiMapRxBuf(struct GL_HIF_INFO *prHifInfo,
			       void *pucBuf, uint32_t u4Offset, uint32_t u4Len);
static void axiUnmapTxBuf(struct GL_HIF_INFO *prHifInfo,
			  phys_addr_t rDmaAddr, uint32_t u4Len);
static void axiUnmapRxBuf(struct GL_HIF_INFO *prHifInfo,
			  phys_addr_t rDmaAddr, uint32_t u4Len);
static void axiFreeDesc(struct GL_HIF_INFO *prHifInfo,
			struct RTMP_DMABUF *prDescRing);
static void axiFreeBuf(void *pucSrc, uint32_t u4Len);
static void axiFreePacket(void *pvPacket);
#endif /* AXI_CFG_PREALLOC_MEMORY_BUFFER */

static void axiAllocTxDataBuf(struct MSDU_TOKEN_ENTRY *prToken, uint32_t u4Idx);
static void *axiAllocRxBuf(struct GL_HIF_INFO *prHifInfo,
			   struct RTMP_DMABUF *prDmaBuf,
			   uint32_t u4Num, uint32_t u4Idx);
static bool axiCopyCmd(struct GL_HIF_INFO *prHifInfo,
		       struct RTMP_DMACB *prTxCell, void *pucBuf,
		       void *pucSrc1, uint32_t u4SrcLen1,
		       void *pucSrc2, uint32_t u4SrcLen2);
static bool axiCopyEvent(struct GL_HIF_INFO *prHifInfo,
			 struct RTMP_DMACB *pRxCell,
			 struct RXD_STRUCT *pRxD,
			 struct RTMP_DMABUF *prDmaBuf,
			 uint8_t *pucDst, uint32_t u4Len);
static bool axiCopyTxData(struct MSDU_TOKEN_ENTRY *prToken,
			  void *pucSrc, uint32_t u4Len);
static bool axiCopyRxData(struct GL_HIF_INFO *prHifInfo,
			  struct RTMP_DMACB *pRxCell,
			  struct RTMP_DMABUF *prDmaBuf,
			  struct SW_RFB *prSwRfb);
static void axiDumpTx(struct GL_HIF_INFO *prHifInfo,
		      struct RTMP_TX_RING *prTxRing,
		      uint32_t u4Idx, uint32_t u4DumpLen);
static void axiDumpRx(struct GL_HIF_INFO *prHifInfo,
		      struct RTMP_RX_RING *prRxRing,
		      uint32_t u4Idx, uint32_t u4DumpLen);

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */

static struct mt66xx_hif_driver_data *get_platform_driver_data(void)
{
	ASSERT(g_prPlatDev);
	if (!g_prPlatDev)
		return NULL;

	return (struct mt66xx_hif_driver_data *) platform_get_drvdata(
			g_prPlatDev);
}

static int hifAxiProbe(void)
{
	int ret = 0;
	struct mt66xx_hif_driver_data *prDriverData;
	struct mt66xx_chip_info *prChipInfo;

	ASSERT(g_prPlatDev);

	prDriverData = get_platform_driver_data();
	prChipInfo = prDriverData->chip_info;

#if CFG_MTK_ANDROID_WMT
#if (CFG_SUPPORT_CONNINFRA == 0)
	mtk_wcn_consys_hw_wifi_paldo_ctrl(1);
#endif
#endif
	if (prChipInfo->wmmcupwron)
		ret = prChipInfo->wmmcupwron();
	if (ret != 0) {
		DBGLOG(INIT, INFO, "wmmcu pwr on fail!\n");
		if (prChipInfo->wmmcupwroff)
			prChipInfo->wmmcupwroff();
		goto out;
	}
	if (pfWlanProbe((void *) g_prPlatDev, (void *) prDriverData) !=
			WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, INFO, "pfWlanProbe fail!\n");
		ret = -1;
		if (prChipInfo->wmmcupwroff)
			prChipInfo->wmmcupwroff();
		goto out;
	}
	g_fgDriverProbed = TRUE;
out:
	DBGLOG(INIT, TRACE, "hifAxiProbe() done(%d)\n", ret);

	return ret;
}

int hifAxiRemove(void)
{
	struct mt66xx_hif_driver_data *prDriverData;
	struct mt66xx_chip_info *prChipInfo;

	prDriverData = get_platform_driver_data();
	prChipInfo = prDriverData->chip_info;

	if (g_fgDriverProbed) {
		pfWlanRemove();
		DBGLOG(INIT, TRACE, "pfWlanRemove done\n");
	}

#if (CFG_SUPPORT_CONNINFRA == 1)
	if (prChipInfo->coexpccifoff) {
		prChipInfo->coexpccifoff();
		DBGLOG(INIT, TRACE, "pccif off\n");
	}
#endif

	if (prChipInfo->coantVFE28Dis)
		prChipInfo->coantVFE28Dis();

	if (prChipInfo->wmmcupwroff)
		prChipInfo->wmmcupwroff();
#if CFG_MTK_ANDROID_WMT
#if (CFG_SUPPORT_CONNINFRA == 0)
	mtk_wcn_consys_hw_wifi_paldo_ctrl(0);
#endif
#endif
	g_fgDriverProbed = FALSE;

	DBGLOG(INIT, TRACE, "hifAxiRemove() done\n");
	return 0;
}

#if CFG_MTK_ANDROID_WMT
#if (CFG_SUPPORT_CONNINFRA == 0)
static int hifAxiGetBusCnt(void)
{
	if (!g_prGlueInfo)
		return 0;

	return g_prGlueInfo->rHifInfo.u4HifCnt;
}

static int hifAxiClrBusCnt(void)
{
	if (g_prGlueInfo)
		g_prGlueInfo->rHifInfo.u4HifCnt = 0;

	return 0;
}

static int hifAxiSetMpuProtect(bool enable)
{
#if CFG_MTK_ANDROID_EMI
	kalSetEmiMpuProtection(gConEmiPhyBase, enable);
#endif
	return 0;
}


static int hifAxiIsWifiDrvOwn(void)
{
	if (!g_prGlueInfo || !g_prGlueInfo->prAdapter)
		return 0;

	return (g_prGlueInfo->prAdapter->fgIsFwOwn == FALSE) ? 1 : 0;
}
#endif
#endif /* CFG_MTK_ANDROID_WMT */

static int axiDmaSetup(struct platform_device *pdev,
		struct mt66xx_hif_driver_data *prDriverData)
{
	struct mt66xx_chip_info *prChipInfo;
	u64 dma_mask;
	int ret = 0;

	prChipInfo = prDriverData->chip_info;

#if AXI_CFG_PREALLOC_MEMORY_BUFFER
	ret = _init_resv_mem(pdev);
	if (ret)
		goto exit;
	ret = of_reserved_mem_device_init(&pdev->dev);
	if (ret) {
		DBGLOG(INIT, ERROR, "of_reserved_mem_device_init failed(%d).\n",
				ret);
		goto exit;
	}
#else
	ret = of_dma_configure(&pdev->dev, pdev->dev.of_node, true);
	if (ret) {
		DBGLOG(INIT, ERROR, "of_dma_configure failed(%d).\n",
				ret);
		goto exit;
	}
#endif

	dma_mask = DMA_BIT_MASK(prChipInfo->bus_info->u4DmaMask);
	ret = dma_set_mask_and_coherent(&pdev->dev, dma_mask);
	if (ret) {
		DBGLOG(INIT, ERROR, "dma_set_mask_and_coherent failed(%d)\n",
			ret);
		goto exit;
	}

exit:
	return ret;
}


static bool axiCsrIoremap(struct platform_device *pdev)
{
#ifdef CONFIG_OF
	struct device_node *node = NULL;
	struct resource res;

	node = of_find_compatible_node(NULL, NULL, "mediatek,wifi");
	if (!node) {
		DBGLOG(INIT, ERROR, "WIFI-OF: get wifi device node fail\n");
		return false;
	}

	if (of_address_to_resource(node, 0, &res)) {
		DBGLOG(INIT, ERROR, "WIFI-OF: of_address_to_resource fail\n");
		of_node_put(node);
		return false;
	}
	of_node_put(node);

	g_u8CsrOffset = (u64)res.start;
	g_u4CsrSize = resource_size(&res);
#else
	g_u8CsrOffset = axi_resource_start(pdev, 0);
	g_u4CsrSize = axi_resource_len(pdev, 0);
#endif
	if (CSRBaseAddress) {
		DBGLOG(INIT, ERROR, "CSRBaseAddress not iounmap!\n");
		return false;
	}

	request_mem_region(g_u8CsrOffset, g_u4CsrSize, axi_name(pdev));

	/* map physical address to virtual address for accessing register */
#ifdef CONFIG_OF
	CSRBaseAddress = of_iomap(node, 0);
#else
	CSRBaseAddress = ioremap(g_u8CsrOffset, g_u4CsrSize);
#endif

	if (!CSRBaseAddress) {
		DBGLOG(INIT, INFO,
			"ioremap failed for device %s, region 0x%X @ 0x%lX\n",
			axi_name(pdev), g_u4CsrSize, g_u8CsrOffset);
		release_mem_region(g_u8CsrOffset, g_u4CsrSize);
		return false;
	}

	DBGLOG(INIT, INFO, "CSRBaseAddress:0x%lX ioremap region 0x%X @ 0x%lX\n",
	       CSRBaseAddress, g_u4CsrSize, g_u8CsrOffset);

	return true;
}

static void axiCsrIounmap(struct platform_device *pdev)
{
	if (!CSRBaseAddress)
		return;

	/* Unmap CSR base address */
	iounmap(CSRBaseAddress);
	release_mem_region(g_u8CsrOffset, g_u4CsrSize);

	CSRBaseAddress = NULL;
	g_u8CsrOffset = 0;
	g_u4CsrSize = 0;
}

#if AXI_CFG_PREALLOC_MEMORY_BUFFER
static bool axiAllocRsvMem(uint32_t u4Size, struct HIF_MEM *prMem)
{
	/* 8 bytes alignment */
	if (u4Size & 7)
		u4Size += 8 - (u4Size & 7);

	if ((grMem.u4Offset + u4Size) >= gWifiRsvMemSize)
		return false;

	prMem->pa = grMem.pucRsvMemBase + grMem.u4Offset;
	prMem->va = grMem.pucRsvMemVirBase + grMem.u4Offset;
	grMem.u4Offset += u4Size;

	return prMem->va != NULL;
}

static int axiAllocHifMem(struct platform_device *pdev,
		struct mt66xx_hif_driver_data *prDriverData)
{
	struct mt66xx_chip_info *prChipInfo;
	uint32_t u4Idx;
	uint32_t i = sizeof(wifi_rsrv_mems) / sizeof(struct wifi_rsrv_mem);

	prChipInfo = prDriverData->chip_info;

	/* Allocation size should be a power of two */
	while (i > 0) {
		i--;
		if (!(gWifiRsvMemSize & BIT(i)))
			continue;

		wifi_rsrv_mems[i].size = BIT(i);
		wifi_rsrv_mems[i].vir_base = KAL_DMA_ALLOC_COHERENT(&pdev->dev,
				wifi_rsrv_mems[i].size,
				&wifi_rsrv_mems[i].phy_base);
		if (!wifi_rsrv_mems[i].vir_base) {
			DBGLOG(INIT, ERROR,
				"[%d] DMA_ALLOC_COHERENT failed, size: 0x%x\n",
				i, wifi_rsrv_mems[i].size);
			return -1;
		}
		if (!grMem.pucRsvMemBase) {
			grMem.pucRsvMemBase = wifi_rsrv_mems[i].phy_base;
			grMem.pucRsvMemVirBase = wifi_rsrv_mems[i].vir_base;
			grMem.u4RsvMemSize = (uint64_t) gWifiRsvMemSize;
		}
	}
	if (!grMem.pucRsvMemBase)
		return -1;
	DBGLOG(INIT, INFO, "pucRsvMemBase[%pa], pucRsvMemVirBase[%pa]\n",
	       &grMem.pucRsvMemBase,
	       &grMem.pucRsvMemVirBase);

	kalSetDrvEmiMpuProtection(grMem.pucRsvMemBase, 0, grMem.u4RsvMemSize);

	for (u4Idx = 0; u4Idx < NUM_OF_TX_RING; u4Idx++) {
		if (!axiAllocRsvMem(TX_RING_SIZE * TXD_SIZE,
				    &grMem.rTxDesc[u4Idx]))
			DBGLOG(INIT, ERROR, "TxDesc[%u] alloc fail\n", u4Idx);
	}

	if (!axiAllocRsvMem(RX_RING0_SIZE * RXD_SIZE, &grMem.rRxDesc[0]))
		DBGLOG(INIT, ERROR, "RxDesc[0] alloc fail\n");

#if (CFG_SUPPORT_CONNAC2X == 1)

	if (!axiAllocRsvMem(RX_RING0_SIZE * RXD_SIZE, &grMem.rRxDesc[1]))
		DBGLOG(INIT, ERROR, "RxDesc[1] alloc fail\n");

	if (!axiAllocRsvMem(RX_RING1_SIZE * RXD_SIZE, &grMem.rRxDesc[2]))
		DBGLOG(INIT, ERROR, "RxDesc[2] alloc fail\n");

	if (!axiAllocRsvMem(RX_RING1_SIZE * RXD_SIZE, &grMem.rRxDesc[3]))
		DBGLOG(INIT, ERROR, "RxDesc[3] alloc fail\n");

	if (!axiAllocRsvMem(RX_RING1_SIZE * RXD_SIZE, &grMem.rRxDesc[4]))
		DBGLOG(INIT, ERROR, "RxDesc[4] alloc fail\n");

#else

	if (!axiAllocRsvMem(RX_RING1_SIZE * RXD_SIZE, &grMem.rRxDesc[1]))
		DBGLOG(INIT, ERROR, "RxDesc[1] alloc fail\n");

#endif

	for (u4Idx = 0; u4Idx < TX_RING_SIZE; u4Idx++) {
		if (!axiAllocRsvMem(AXI_TX_CMD_BUFF_SIZE,
				    &grMem.rTxCmdBuf[u4Idx]))
			DBGLOG(INIT, ERROR, "TxCmdBuf[%u] alloc fail\n", u4Idx);
	}

	for (u4Idx = 0; u4Idx < RX_RING0_SIZE; u4Idx++) {
		if (!axiAllocRsvMem(CFG_RX_MAX_PKT_SIZE,
				    &grMem.rRxDataBuf[u4Idx]))
			DBGLOG(INIT, ERROR,
			       "RxDataBuf[%u] alloc fail\n", u4Idx);
	}

#if (CFG_SUPPORT_CONNAC2X == 1)

	for (u4Idx = 0; u4Idx < RX_RING0_SIZE; u4Idx++) {
		if (!axiAllocRsvMem(RX_BUFFER_AGGRESIZE,
				    &grMem.rRxEventBuf[u4Idx]))
			DBGLOG(INIT, ERROR,
			       "RxDataBuf[%u] alloc fail\n", u4Idx);
	}

	for (u4Idx = 0; u4Idx < RX_RING1_SIZE; u4Idx++) {
		if (!axiAllocRsvMem(RX_BUFFER_AGGRESIZE,
				    &grMem.wfdma0_rx_ring_idx2[u4Idx]))
			DBGLOG(INIT, ERROR,
			       "RxEventBuf[%u] alloc fail\n", u4Idx);
	}

	for (u4Idx = 0; u4Idx < RX_RING1_SIZE; u4Idx++) {
		if (!axiAllocRsvMem(RX_BUFFER_AGGRESIZE,
				    &grMem.wfdma0_rx_ring_idx3[u4Idx]))
			DBGLOG(INIT, ERROR,
			       "RxEventBuf[%u] alloc fail\n", u4Idx);
	}

	for (u4Idx = 0; u4Idx < RX_RING1_SIZE; u4Idx++) {
		if (!axiAllocRsvMem(RX_BUFFER_AGGRESIZE,
				    &grMem.wfdma1_rx_ring_idx0[u4Idx]))
			DBGLOG(INIT, ERROR,
			       "RxEventBuf[%u] alloc fail\n", u4Idx);
	}
#else
	for (u4Idx = 0; u4Idx < RX_RING1_SIZE; u4Idx++) {
		if (!axiAllocRsvMem(RX_BUFFER_AGGRESIZE,
				    &grMem.rRxEventBuf[u4Idx]))
			DBGLOG(INIT, ERROR,
			       "RxEventBuf[%u] alloc fail\n", u4Idx);
	}

#endif

#if HIF_TX_PREALLOC_DATA_BUFFER
	for (u4Idx = 0; u4Idx < HIF_TX_MSDU_TOKEN_NUM; u4Idx++) {
		if (!axiAllocRsvMem(AXI_TX_MAX_SIZE_PER_FRAME +
				    prChipInfo->txd_append_size,
				    &grMem.rMsduBuf[u4Idx]))
			DBGLOG(INIT, ERROR, "MsduBuf[%u] alloc fail\n", u4Idx);
	}
#endif

	DBGLOG(INIT, INFO, "grMem.u4Offset[0x%x]\n", grMem.u4Offset);

	return 0;
}

static void axiFreeHifMem(struct platform_device *pdev)
{
	uint32_t i = 0;
	uint32_t count = sizeof(wifi_rsrv_mems) / sizeof(struct wifi_rsrv_mem);

	for (i = 0; i < count; i++) {
		if (!wifi_rsrv_mems[i].vir_base)
			continue;
		KAL_DMA_FREE_COHERENT(&pdev->dev,
			wifi_rsrv_mems[i].size,
			wifi_rsrv_mems[i].vir_base,
			(dma_addr_t) wifi_rsrv_mems[i].phy_base);
	}
}

static int _init_resv_mem(struct platform_device *pdev)
{
#ifdef CONFIG_OF
	int ret = 0;
	struct device_node *np;

	np = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
	if (!np) {
		DBGLOG(INIT, ERROR, "can NOT find memory-region.\n");
		return -1;
	}

	ret = of_property_read_u64_array(np, "size", &gWifiRsvMemSize, 1);
	if (ret != 0)
		DBGLOG(INIT, ERROR, "get rsrv mem size failed(%d).\n", ret);
	else
		DBGLOG(INIT, INFO, "gWifiRsvMemSize: 0x%x\n", gWifiRsvMemSize);

	of_node_put(np);

	return ret;
#else
	DBGLOG(INIT, ERROR, "kernel option CONFIG_OF not enabled.\n");
	return -1;
#endif
}

#endif /* AXI_CFG_PREALLOC_MEMORY_BUFFER */

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function is a AXI interrupt callback function
 *
 * \param[in] func  pointer to AXI handle
 *
 * \return void
 */
/*----------------------------------------------------------------------------*/
static irqreturn_t mtk_axi_interrupt(int irq, void *dev_instance)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	static DEFINE_RATELIMIT_STATE(_rs, 2 * HZ, 1);

	prGlueInfo = (struct GLUE_INFO *)dev_instance;
	if (!prGlueInfo) {
		DBGLOG(HAL, INFO, "No glue info in mtk_axi_interrupt()\n");
		return IRQ_NONE;
	}

	GLUE_INC_REF_CNT(prGlueInfo->prAdapter->rHifStats.u4HwIsrCount);
	halDisableInterrupt(prGlueInfo->prAdapter);

	if (prGlueInfo->ulFlag & GLUE_FLAG_HALT) {
		DBGLOG(HAL, INFO, "GLUE_FLAG_HALT skip INT\n");
		return IRQ_NONE;
	}

	kalSetIntEvent(prGlueInfo);
	if (__ratelimit(&_rs))
		LOG_FUNC("In HIF ISR.\n");

	return IRQ_HANDLED;
}
#if (CFG_SUPPORT_CONNINFRA == 1)
void kalSetRstEvent(void)
{
	KAL_WAKE_LOCK(NULL, g_IntrWakeLock);

	set_bit(GLUE_FLAG_RST_START_BIT, &g_ulFlag);

	/* when we got interrupt, we wake up servie thread */
	wake_up_interruptible(&g_waitq_rst);

}

static irqreturn_t mtk_sw_interrupt(int irq, void *dev_instance)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct mt66xx_chip_info *prChipInfo;

	prGlueInfo = (struct GLUE_INFO *)dev_instance;
	GLUE_INC_REF_CNT(prGlueInfo->prAdapter->rHifStats.u4SwIsrCount);
	prChipInfo = prGlueInfo->prAdapter->chip_info;
	if (prChipInfo->sw_interrupt_handler)
		prChipInfo->sw_interrupt_handler(prGlueInfo->prAdapter);

	return IRQ_HANDLED;
}
#endif

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function is a AXI probe function
 *
 * \param[in] func   pointer to AXI handle
 * \param[in] id     pointer to AXI device id table
 *
 * \return void
 */
/*----------------------------------------------------------------------------*/
static int mtk_axi_probe(IN struct platform_device *pdev)
{
	struct mt66xx_hif_driver_data *prDriverData;
	struct mt66xx_chip_info *prChipInfo;
	int ret = 0;
#if CFG_MTK_ANDROID_WMT
#if (CFG_SUPPORT_CONNINFRA == 0)
	struct _MTK_WCN_WMT_WLAN_CB_INFO rWmtCb;
#else
	struct MTK_WCN_WLAN_CB_INFO rWlanCb;
#endif
#endif

	g_prPlatDev = pdev;
	prDriverData = (struct mt66xx_hif_driver_data *)
			mtk_axi_ids[0].driver_data;
	prChipInfo = prDriverData->chip_info;
	prChipInfo->pdev = (void *) pdev;

	platform_set_drvdata(pdev, (void *) prDriverData);

	if (!axiCsrIoremap(pdev))
		goto exit;

	ret = axiDmaSetup(pdev, prDriverData);
	if (ret)
		goto exit;

#if AXI_CFG_PREALLOC_MEMORY_BUFFER
	ret = axiAllocHifMem(pdev, prDriverData);
	if (ret)
		goto exit;
#endif

#if CFG_MTK_ANDROID_WMT
#if (CFG_SUPPORT_CONNINFRA == 0)
	memset(&rWmtCb, 0, sizeof(struct _MTK_WCN_WMT_WLAN_CB_INFO));
	rWmtCb.wlan_probe_cb = hifAxiProbe;
	rWmtCb.wlan_remove_cb = hifAxiRemove;
	rWmtCb.wlan_bus_cnt_get_cb = hifAxiGetBusCnt;
	rWmtCb.wlan_bus_cnt_clr_cb = hifAxiClrBusCnt;
	rWmtCb.wlan_emi_mpu_set_protection_cb = hifAxiSetMpuProtect;
	rWmtCb.wlan_is_wifi_drv_own_cb = hifAxiIsWifiDrvOwn;
	mtk_wcn_wmt_wlan_reg(&rWmtCb);
#else

	rWlanCb.wlan_probe_cb = hifAxiProbe;
	rWlanCb.wlan_remove_cb = hifAxiRemove;
	mtk_wcn_wlan_reg(&rWlanCb);

	if (prChipInfo->conninra_cb_register)
		prChipInfo->conninra_cb_register();

#endif
#else
	hifAxiProbe();
#endif

exit:
	DBGLOG(INIT, INFO, "mtk_axi_probe() done, ret: %d\n", ret);
	return ret;
}

static int mtk_axi_remove(IN struct platform_device *pdev)
{
	axiCsrIounmap(pdev);

#if AXI_CFG_PREALLOC_MEMORY_BUFFER
	axiFreeHifMem(pdev);
#endif

#if CFG_MTK_ANDROID_WMT
#if (CFG_SUPPORT_CONNINFRA == 0)
	mtk_wcn_wmt_wlan_unreg();
#else
	mtk_wcn_wlan_unreg();
#endif /*end of CFG_SUPPORT_CONNINFRA == 0*/
#else
	hifAxiRemove();
#endif
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static int mtk_axi_suspend(IN struct platform_device *pdev,
	IN pm_message_t state)
{
	return 0;
}

int mtk_axi_resume(IN struct platform_device *pdev)
{
	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function will register pci bus to the os
 *
 * \param[in] pfProbe    Function pointer to detect card
 * \param[in] pfRemove   Function pointer to remove card
 *
 * \return The result of registering pci bus
 */
/*----------------------------------------------------------------------------*/
uint32_t glRegisterBus(probe_card pfProbe, remove_card pfRemove)
{
	int ret = 0;

	ASSERT(pfProbe);
	ASSERT(pfRemove);

	pfWlanProbe = pfProbe;
	pfWlanRemove = pfRemove;

	mtk_axi_driver.probe = mtk_axi_probe;
	mtk_axi_driver.remove = mtk_axi_remove;

	mtk_axi_driver.suspend = mtk_axi_suspend;
	mtk_axi_driver.resume = mtk_axi_resume;

	ret = (platform_driver_register(&mtk_axi_driver) == 0) ?
			WLAN_STATUS_SUCCESS : WLAN_STATUS_FAILURE;
	return ret;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief This function will unregister pci bus to the os
 *
 * \param[in] pfRemove Function pointer to remove card
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
void glUnregisterBus(remove_card pfRemove)
{
	if (g_fgDriverProbed) {
		pfRemove();
		g_fgDriverProbed = FALSE;
	}
	platform_driver_unregister(&mtk_axi_driver);
}

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
	struct HIF_MEM_OPS *prMemOps;

	g_prGlueInfo = prGlueInfo;
	prHif = &prGlueInfo->rHifInfo;
	prMemOps = &prHif->rMemOps;

	prHif->pdev = (struct platform_device *)ulCookie;
	prHif->prDmaDev = &prHif->pdev->dev;

	prHif->CSRBaseAddress = CSRBaseAddress;

	SET_NETDEV_DEV(prGlueInfo->prDevHandler, &prHif->pdev->dev);

	prGlueInfo->u4InfType = MT_DEV_INF_AXI;

	prHif->fgIsPowerOff = true;
	prHif->fgIsDumpLog = false;

#if AXI_CFG_PREALLOC_MEMORY_BUFFER
	prMemOps->allocTxDesc = axiAllocTxDesc;
	prMemOps->allocRxDesc = axiAllocRxDesc;
	prMemOps->allocTxCmdBuf = axiAllocTxCmdBuf;
	prMemOps->allocTxDataBuf = axiAllocTxDataBuf;
	prMemOps->allocRxBuf = axiAllocRxBuf;
	prMemOps->allocRuntimeMem = NULL;
	prMemOps->copyCmd = axiCopyCmd;
	prMemOps->copyEvent = axiCopyEvent;
	prMemOps->copyTxData = axiCopyTxData;
	prMemOps->copyRxData = axiCopyRxData;
	prMemOps->mapTxBuf = NULL;
	prMemOps->mapRxBuf = NULL;
	prMemOps->unmapTxBuf = NULL;
	prMemOps->unmapRxBuf = NULL;
	prMemOps->freeDesc = NULL;
	prMemOps->freeBuf = NULL;
	prMemOps->freePacket = NULL;
	prMemOps->dumpTx = axiDumpTx;
	prMemOps->dumpRx = axiDumpRx;
#else
	prMemOps->allocTxDesc = axiAllocDesc;
	prMemOps->allocRxDesc = axiAllocDesc;
	prMemOps->allocTxCmdBuf = NULL;
	prMemOps->allocTxDataBuf = axiAllocTxDataBuf;
	prMemOps->allocRxBuf = axiAllocRxBuf;
	prMemOps->allocRuntimeMem = axiAllocRuntimeMem;
	prMemOps->copyCmd = axiCopyCmd;
	prMemOps->copyEvent = axiCopyEvent;
	prMemOps->copyTxData = axiCopyTxData;
	prMemOps->copyRxData = axiCopyRxData;
	prMemOps->mapTxBuf = axiMapTxBuf;
	prMemOps->mapRxBuf = axiMapRxBuf;
	prMemOps->unmapTxBuf = axiUnmapTxBuf;
	prMemOps->unmapRxBuf = axiUnmapRxBuf;
	prMemOps->freeDesc = axiFreeDesc;
	prMemOps->freeBuf = axiFreeBuf;
	prMemOps->freePacket = axiFreePacket;
	prMemOps->dumpTx = axiDumpTx;
	prMemOps->dumpRx = axiDumpRx;
#endif /* AXI_CFG_PREALLOC_MEMORY_BUFFER */
}

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
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Initialize bus operation and hif related information, request
 *        resources.
 *
 * \param[out] pvData    A pointer to HIF-specific data type buffer.
 *                       For eHPI, pvData is a pointer to UINT_32 type and
 *                       stores a mapped base address.
 *
 * \return (none)
 */
/*----------------------------------------------------------------------------*/
u_int8_t glBusInit(void *pvData)
{
	ASSERT(pvData);

	return TRUE;
}

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
}

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
	struct net_device *prNetDevice = NULL;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct GL_HIF_INFO *prHifInfo = NULL;
	struct platform_device *pdev = NULL;
#ifdef CONFIG_OF
	struct device_node *node = NULL;
#endif
	int ret = 0;

	ASSERT(pvData);
	if (!pvData)
		return -1;

	prNetDevice = (struct net_device *)pvData;
	prGlueInfo = (struct GLUE_INFO *)pvCookie;
	ASSERT(prGlueInfo);
	if (!prGlueInfo)
		return -1;

	prHifInfo = &prGlueInfo->rHifInfo;
	pdev = prHifInfo->pdev;

	prHifInfo->u4IrqId = AXI_WLAN_IRQ_NUMBER;
#ifdef CONFIG_OF
	node = of_find_compatible_node(NULL, NULL, "mediatek,wifi");
	if (node) {
		prHifInfo->u4IrqId = irq_of_parse_and_map(node, 0);
#if (CFG_SUPPORT_CONNINFRA == 1)
		prHifInfo->u4IrqId_1 = irq_of_parse_and_map(node, 1);
#endif
	}
	else
		DBGLOG(INIT, ERROR,
			"WIFI-OF: get wifi device node fail\n");
#endif
#if (CFG_SUPPORT_CONNINFRA == 1)
	DBGLOG(INIT, INFO, "glBusSetIrq: request_irq num(%d), num(%d)\n",
	       prHifInfo->u4IrqId, prHifInfo->u4IrqId_1);
#else
	DBGLOG(INIT, INFO, "glBusSetIrq: request_irq num(%d)\n",
	       prHifInfo->u4IrqId);
#endif /*end of CFG_SUPPORT_CONNINFRA == 1*/
	ret = request_irq(prHifInfo->u4IrqId, mtk_axi_interrupt, IRQF_SHARED,
			  prNetDevice->name, prGlueInfo);
	if (ret != 0) {
		DBGLOG(INIT, INFO, "request_irq(%u) ERROR(%d)\n",
				prHifInfo->u4IrqId, ret);
		goto exit;
	}
#if KERNEL_VERSION(5, 4, 0) <= CFG80211_VERSION_CODE
	ret = enable_irq_wake(prHifInfo->u4IrqId);
	if (ret) {
		DBGLOG(INIT, INFO, "enable_irq_wake(%u) ERROR(%d)\n",
				prHifInfo->u4IrqId, ret);
		goto exit;
	}
#endif
#if (CFG_SUPPORT_CONNINFRA == 1)
	ret = request_irq(prHifInfo->u4IrqId_1, mtk_sw_interrupt, IRQF_SHARED,
			  prNetDevice->name, prGlueInfo);
	if (ret != 0) {
		DBGLOG(INIT, INFO, "request_irq(%u) ERROR(%d)\n",
				prHifInfo->u4IrqId_1, ret);
		goto exit;
	}
#if KERNEL_VERSION(5, 4, 0) <= CFG80211_VERSION_CODE
	ret = enable_irq_wake(prHifInfo->u4IrqId_1);
	if (ret) {
		DBGLOG(INIT, INFO, "enable_irq_wake(%u) ERROR(%d)\n",
				prHifInfo->u4IrqId_1, ret);
		goto exit;
	}
#endif
#endif

exit:
	return ret;
}

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
	struct platform_device *pdev = NULL;

	ASSERT(pvData);
	if (!pvData) {
		DBGLOG(INIT, INFO, "%s null pvData\n", __func__);
		return;
	}
	prNetDevice = (struct net_device *)pvData;
	prGlueInfo = (struct GLUE_INFO *) pvCookie;
	ASSERT(prGlueInfo);
	if (!prGlueInfo) {
		DBGLOG(INIT, INFO, "%s no glue info\n", __func__);
		return;
	}

	prHifInfo = &prGlueInfo->rHifInfo;
	pdev = prHifInfo->pdev;

	synchronize_irq(prHifInfo->u4IrqId);
	free_irq(prHifInfo->u4IrqId, prGlueInfo);
#if (CFG_SUPPORT_CONNINFRA == 1)
	synchronize_irq(prHifInfo->u4IrqId_1);
	free_irq(prHifInfo->u4IrqId_1, prGlueInfo);
#endif
}

u_int8_t glIsReadClearReg(uint32_t u4Address)
{
	return TRUE;
}

void glSetPowerState(IN struct GLUE_INFO *prGlueInfo, IN uint32_t ePowerMode)
{
}

void glGetDev(void *ctx, struct device **dev)
{
	*dev = &((struct platform_device *)ctx)->dev;
}

void glGetHifDev(struct GL_HIF_INFO *prHif, struct device **dev)
{
	*dev = &(prHif->pdev->dev);
}

#if AXI_CFG_PREALLOC_MEMORY_BUFFER
static void axiAllocTxDesc(struct GL_HIF_INFO *prHifInfo,
			   struct RTMP_DMABUF *prDescRing,
			   uint32_t u4Num)
{
	prDescRing->AllocVa = grMem.rTxDesc[u4Num].va;
	prDescRing->AllocPa = grMem.rTxDesc[u4Num].pa;
	if (prDescRing->AllocVa == NULL)
		DBGLOG(HAL, ERROR, "prDescRing->AllocVa is NULL\n");
	else
		memset(prDescRing->AllocVa, 0, prDescRing->AllocSize);
}

static void axiAllocRxDesc(struct GL_HIF_INFO *prHifInfo,
			   struct RTMP_DMABUF *prDescRing,
			   uint32_t u4Num)
{
	prDescRing->AllocVa = grMem.rRxDesc[u4Num].va;
	prDescRing->AllocPa = grMem.rRxDesc[u4Num].pa;
	if (prDescRing->AllocVa == NULL)
		DBGLOG(HAL, ERROR, "prDescRing->AllocVa is NULL\n");
	else
		memset(prDescRing->AllocVa, 0, prDescRing->AllocSize);
}

static bool axiAllocTxCmdBuf(struct RTMP_DMABUF *prDmaBuf,
			     uint32_t u4Num, uint32_t u4Idx)
{
	/* only for cmd & fw download ring */
	if (u4Num == 2 || u4Num == 3) {
		prDmaBuf->AllocSize = AXI_TX_CMD_BUFF_SIZE;
		prDmaBuf->AllocPa = grMem.rTxCmdBuf[u4Idx].pa;
		prDmaBuf->AllocVa = grMem.rTxCmdBuf[u4Idx].va;
		if (prDmaBuf->AllocVa  == NULL) {
			DBGLOG(HAL, ERROR, "prDescRing->AllocVa is NULL\n");
			return false;
		}
		memset(prDmaBuf->AllocVa, 0, prDmaBuf->AllocSize);
	}
	return true;
}

static void axiAllocTxDataBuf(struct MSDU_TOKEN_ENTRY *prToken, uint32_t u4Idx)
{
	prToken->prPacket = grMem.rMsduBuf[u4Idx].va;
	prToken->rDmaAddr = grMem.rMsduBuf[u4Idx].pa;
}

static void *axiAllocRxBuf(struct GL_HIF_INFO *prHifInfo,
			   struct RTMP_DMABUF *prDmaBuf,
			   uint32_t u4Num, uint32_t u4Idx)
{
	/* ring 0 for data, ring 1 for event */
	if (u4Num == 0) {
		prDmaBuf->AllocPa = grMem.rRxDataBuf[u4Idx].pa;
		prDmaBuf->AllocVa = grMem.rRxDataBuf[u4Idx].va;
	} else if (u4Num == 1) {
		prDmaBuf->AllocPa = grMem.rRxEventBuf[u4Idx].pa;
		prDmaBuf->AllocVa = grMem.rRxEventBuf[u4Idx].va;
	}
	/* Add for connac2x RX ring */
#if (CFG_SUPPORT_CONNAC2X == 1)

	else if (u4Num == 2) {
		prDmaBuf->AllocPa = grMem.wfdma0_rx_ring_idx2[u4Idx].pa;
		prDmaBuf->AllocVa = grMem.wfdma0_rx_ring_idx2[u4Idx].va;
	} else if (u4Num == 3) {
		prDmaBuf->AllocPa = grMem.wfdma0_rx_ring_idx3[u4Idx].pa;
		prDmaBuf->AllocVa = grMem.wfdma0_rx_ring_idx3[u4Idx].va;
	} else if (u4Num == 4) {
		prDmaBuf->AllocPa = grMem.wfdma1_rx_ring_idx0[u4Idx].pa;
		prDmaBuf->AllocVa = grMem.wfdma1_rx_ring_idx0[u4Idx].va;
	}

#endif
	else {
		DBGLOG(RX, ERROR, "RX alloc fail error number=%d\n", u4Num);
		return prDmaBuf->AllocVa;
	}
	if (prDmaBuf->AllocVa == NULL)
		DBGLOG(HAL, ERROR, "prDmaBuf->AllocVa is NULL\n");
	else
		memset(prDmaBuf->AllocVa, 0, prDmaBuf->AllocSize);

	return prDmaBuf->AllocVa;
}

static bool axiCopyCmd(struct GL_HIF_INFO *prHifInfo,
		       struct RTMP_DMACB *prTxCell, void *pucBuf,
		       void *pucSrc1, uint32_t u4SrcLen1,
		       void *pucSrc2, uint32_t u4SrcLen2)
{
	struct RTMP_DMABUF *prDmaBuf = &prTxCell->DmaBuf;

	memcpy(prDmaBuf->AllocVa, pucSrc1, u4SrcLen1);
	if (pucSrc2 != NULL && u4SrcLen2 > 0)
		memcpy(prDmaBuf->AllocVa + u4SrcLen1, pucSrc2, u4SrcLen2);
	prTxCell->PacketPa = prDmaBuf->AllocPa;

	return true;
}

static bool axiCopyEvent(struct GL_HIF_INFO *prHifInfo,
			 struct RTMP_DMACB *pRxCell,
			 struct RXD_STRUCT *pRxD,
			 struct RTMP_DMABUF *prDmaBuf,
			 uint8_t *pucDst, uint32_t u4Len)
{
	memcpy(pucDst, prDmaBuf->AllocVa, u4Len);

	return true;
}

static bool axiCopyTxData(struct MSDU_TOKEN_ENTRY *prToken,
			  void *pucSrc, uint32_t u4Len)
{
	memcpy(prToken->prPacket, pucSrc, u4Len);

	return true;
}

static bool axiCopyRxData(struct GL_HIF_INFO *prHifInfo,
			  struct RTMP_DMACB *pRxCell,
			  struct RTMP_DMABUF *prDmaBuf,
			  struct SW_RFB *prSwRfb)
{
	struct RXD_STRUCT *pRxD = (struct RXD_STRUCT *)pRxCell->AllocVa;
	struct sk_buff *prSkb = ((struct sk_buff *)prSwRfb->pvPacket);
	uint32_t u4Size = pRxD->SDLen0;

	if (u4Size > CFG_RX_MAX_PKT_SIZE) {
		DBGLOG(RX, ERROR, "Rx Data too large[%u]\n", u4Size);
		return false;
	}

	memcpy(prSkb->data, prDmaBuf->AllocVa, u4Size);

	return true;
}

static void axiDumpTx(struct GL_HIF_INFO *prHifInfo,
		      struct RTMP_TX_RING *prTxRing,
		      uint32_t u4Idx, uint32_t u4DumpLen)
{
	struct RTMP_DMACB *prTxCell;
	struct RTMP_DMABUF *prDmaBuf;
	void *prAddr = NULL;

	prTxCell = &prTxRing->Cell[u4Idx];
	prDmaBuf = &prTxCell->DmaBuf;

	if (prTxCell->prToken)
		prAddr = prTxCell->prToken->prPacket;
	else if (prDmaBuf->AllocVa)
		prAddr = prDmaBuf->AllocVa;

	if (prAddr)
		DBGLOG_MEM32(HAL, INFO, prAddr, u4DumpLen);
}

static void axiDumpRx(struct GL_HIF_INFO *prHifInfo,
		      struct RTMP_RX_RING *prRxRing,
		      uint32_t u4Idx, uint32_t u4DumpLen)
{
	struct RTMP_DMACB *prRxCell;
	struct RTMP_DMABUF *prDmaBuf;

	prRxCell = &prRxRing->Cell[u4Idx];
	prDmaBuf = &prRxCell->DmaBuf;

	if (prRxCell->pPacket)
		DBGLOG_MEM32(HAL, INFO, prRxCell->pPacket, u4DumpLen);
}
#else /* AXI_CFG_PREALLOC_MEMORY_BUFFER */
static void axiAllocDesc(struct GL_HIF_INFO *prHifInfo,
			 struct RTMP_DMABUF *prDescRing,
			 uint32_t u4Num)
{
	dma_addr_t rAddr;

	prDescRing->AllocVa = (void *)KAL_DMA_ALLOC_COHERENT(
		prHifInfo->prDmaDev, prDescRing->AllocSize, &rAddr);
	prDescRing->AllocPa = (phys_addr_t)rAddr;
	if (prDescRing->AllocVa)
		memset(prDescRing->AllocVa, 0, prDescRing->AllocSize);
}

static void *axiAllocRxBuf(struct GL_HIF_INFO *prHifInfo,
			   struct RTMP_DMABUF *prDmaBuf,
			   uint32_t u4Num, uint32_t u4Idx)
{
	struct sk_buff *pkt = dev_alloc_skb(prDmaBuf->AllocSize);
	dma_addr_t rAddr;

	if (!pkt) {
		DBGLOG(HAL, ERROR, "can't allocate rx %u size packet\n",
		       prDmaBuf->AllocSize);
		prDmaBuf->AllocPa = 0;
		prDmaBuf->AllocVa = NULL;
		return NULL;
	}

	prDmaBuf->AllocVa = (void *)pkt->data;
	memset(prDmaBuf->AllocVa, 0, prDmaBuf->AllocSize);

	rAddr = KAL_DMA_MAP_SINGLE(prHifInfo->prDmaDev, prDmaBuf->AllocVa,
				   prDmaBuf->AllocSize, KAL_DMA_FROM_DEVICE);
	if (KAL_DMA_MAPPING_ERROR(prHifInfo->prDmaDev, rAddr)) {
		DBGLOG(HAL, ERROR, "sk_buff dma mapping error!\n");
		dev_kfree_skb(pkt);
		return NULL;
	}
	prDmaBuf->AllocPa = (phys_addr_t)rAddr;
	return (void *)pkt;
}

static void axiAllocTxDataBuf(struct MSDU_TOKEN_ENTRY *prToken, uint32_t u4Idx)
{
	prToken->prPacket = kalMemAlloc(prToken->u4DmaLength, PHY_MEM_TYPE);
	prToken->rDmaAddr = 0;
}

static void *axiAllocRuntimeMem(uint32_t u4SrcLen)
{
	return kalMemAlloc(u4SrcLen, PHY_MEM_TYPE);
}

static bool axiCopyCmd(struct GL_HIF_INFO *prHifInfo,
		       struct RTMP_DMACB *prTxCell, void *pucBuf,
		       void *pucSrc1, uint32_t u4SrcLen1,
		       void *pucSrc2, uint32_t u4SrcLen2)
{
	dma_addr_t rAddr;
	uint32_t u4TotalLen = u4SrcLen1 + u4SrcLen2;

	prTxCell->pBuffer = pucBuf;

	memcpy(pucBuf, pucSrc1, u4SrcLen1);
	if (pucSrc2 != NULL && u4SrcLen2 > 0)
		memcpy(pucBuf + u4SrcLen1, pucSrc2, u4SrcLen2);
	rAddr = KAL_DMA_MAP_SINGLE(prHifInfo->prDmaDev, pucBuf,
				   u4TotalLen, KAL_DMA_TO_DEVICE);
	if (KAL_DMA_MAPPING_ERROR(prHifInfo->prDmaDev, rAddr)) {
		DBGLOG(HAL, ERROR, "KAL_DMA_MAP_SINGLE() error!\n");
		return false;
	}

	prTxCell->PacketPa = (phys_addr_t)rAddr;

	return true;
}

static bool axiCopyEvent(struct GL_HIF_INFO *prHifInfo,
			 struct RTMP_DMACB *pRxCell,
			 struct RXD_STRUCT *pRxD,
			 struct RTMP_DMABUF *prDmaBuf,
			 uint8_t *pucDst, uint32_t u4Len)
{
	struct sk_buff *prSkb = NULL;
	void *pRxPacket = NULL;
	dma_addr_t rAddr;

	KAL_DMA_UNMAP_SINGLE(prHifInfo->prDmaDev,
			     (dma_addr_t)prDmaBuf->AllocPa,
			     prDmaBuf->AllocSize, KAL_DMA_FROM_DEVICE);

	pRxPacket = pRxCell->pPacket;
	ASSERT(pRxPacket);

	prSkb = (struct sk_buff *)pRxPacket;
	memcpy(pucDst, (uint8_t *)prSkb->data, u4Len);

	prDmaBuf->AllocVa = ((struct sk_buff *)pRxCell->pPacket)->data;
	rAddr = KAL_DMA_MAP_SINGLE(prHifInfo->prDmaDev, prDmaBuf->AllocVa,
				   prDmaBuf->AllocSize, KAL_DMA_FROM_DEVICE);
	if (KAL_DMA_MAPPING_ERROR(prHifInfo->prDmaDev, rAddr)) {
		DBGLOG(HAL, ERROR, "KAL_DMA_MAP_SINGLE() error!\n");
		return false;
	}
	prDmaBuf->AllocPa = (phys_addr_t)rAddr;
	return true;
}

static bool axiCopyTxData(struct MSDU_TOKEN_ENTRY *prToken,
			  void *pucSrc, uint32_t u4Len)
{
	memcpy(prToken->prPacket, pucSrc, u4Len);
	return true;
}

static bool axiCopyRxData(struct GL_HIF_INFO *prHifInfo,
			  struct RTMP_DMACB *pRxCell,
			  struct RTMP_DMABUF *prDmaBuf,
			  struct SW_RFB *prSwRfb)
{
	void *pRxPacket = NULL;
	dma_addr_t rAddr;

	pRxPacket = pRxCell->pPacket;
	ASSERT(pRxPacket);

	pRxCell->pPacket = prSwRfb->pvPacket;

	KAL_DMA_UNMAP_SINGLE(prHifInfo->prDmaDev,
			     (dma_addr_t)prDmaBuf->AllocPa,
			     prDmaBuf->AllocSize, KAL_DMA_FROM_DEVICE);
	prSwRfb->pvPacket = pRxPacket;

	prDmaBuf->AllocVa = ((struct sk_buff *)pRxCell->pPacket)->data;
	rAddr = KAL_DMA_MAP_SINGLE(prHifInfo->prDmaDev,
		prDmaBuf->AllocVa, prDmaBuf->AllocSize, KAL_DMA_FROM_DEVICE);
	if (KAL_DMA_MAPPING_ERROR(prHifInfo->prDmaDev, rAddr)) {
		DBGLOG(HAL, ERROR, "KAL_DMA_MAP_SINGLE() error!\n");
		ASSERT(0);
		return false;
	}
	prDmaBuf->AllocPa = (phys_addr_t)rAddr;

	return true;
}

static phys_addr_t axiMapTxBuf(struct GL_HIF_INFO *prHifInfo,
			 void *pucBuf, uint32_t u4Offset, uint32_t u4Len)
{
	dma_addr_t rDmaAddr = 0;

	rDmaAddr = KAL_DMA_MAP_SINGLE(prHifInfo->prDmaDev, pucBuf + u4Offset,
				      u4Len, KAL_DMA_TO_DEVICE);
	if (KAL_DMA_MAPPING_ERROR(prHifInfo->prDmaDev, rDmaAddr)) {
		DBGLOG(HAL, ERROR, "KAL_DMA_MAP_SINGLE() error!\n");
		return 0;
	}

	return (phys_addr_t)rDmaAddr;
}

static phys_addr_t axiMapRxBuf(struct GL_HIF_INFO *prHifInfo,
			 void *pucBuf, uint32_t u4Offset, uint32_t u4Len)
{
	dma_addr_t rDmaAddr = 0;

	rDmaAddr = KAL_DMA_MAP_SINGLE(prHifInfo->prDmaDev, pucBuf + u4Offset,
				      u4Len, KAL_DMA_FROM_DEVICE);
	if (KAL_DMA_MAPPING_ERROR(prHifInfo->prDmaDev, rDmaAddr)) {
		DBGLOG(HAL, ERROR, "KAL_DMA_MAP_SINGLE() error!\n");
		return 0;
	}

	return (phys_addr_t)rDmaAddr;
}

static void axiUnmapTxBuf(struct GL_HIF_INFO *prHifInfo,
			  phys_addr_t rDmaAddr, uint32_t u4Len)
{
	KAL_DMA_UNMAP_SINGLE(prHifInfo->prDmaDev,
			     (dma_addr_t)rDmaAddr,
			     u4Len, KAL_DMA_TO_DEVICE);
}

static void axiUnmapRxBuf(struct GL_HIF_INFO *prHifInfo,
			  phys_addr_t rDmaAddr, uint32_t u4Len)
{
	KAL_DMA_UNMAP_SINGLE(prHifInfo->prDmaDev,
			     (dma_addr_t)rDmaAddr,
			     u4Len, KAL_DMA_FROM_DEVICE);
}

static void axiFreeDesc(struct GL_HIF_INFO *prHifInfo,
			struct RTMP_DMABUF *prDescRing)
{
	if (prDescRing->AllocVa == NULL)
		return;

	KAL_DMA_FREE_COHERENT(prHifInfo->prDmaDev,
			      prDescRing->AllocSize,
			      prDescRing->AllocVa,
			      (dma_addr_t)prDescRing->AllocPa);
	memset(prDescRing, 0, sizeof(struct RTMP_DMABUF));
}

static void axiFreeBuf(void *pucSrc, uint32_t u4Len)
{
	kalMemFree(pucSrc, PHY_MEM_TYPE, u4Len);
}

static void axiFreePacket(void *pvPacket)
{
	kalPacketFree(NULL, pvPacket);
}

static void axiDumpTx(struct GL_HIF_INFO *prHifInfo,
		      struct RTMP_TX_RING *prTxRing,
		      uint32_t u4Idx, uint32_t u4DumpLen)
{
	struct RTMP_DMACB *prTxCell;
	void *prAddr = NULL;

	prTxCell = &prTxRing->Cell[u4Idx];

	if (prTxCell->prToken)
		prAddr = prTxCell->prToken->prPacket;
	else
		prAddr = prTxCell->pBuffer;

	if (prAddr)
		DBGLOG_MEM32(HAL, INFO, prAddr, u4DumpLen);
}

static void axiDumpRx(struct GL_HIF_INFO *prHifInfo,
		      struct RTMP_RX_RING *prRxRing,
		      uint32_t u4Idx, uint32_t u4DumpLen)
{
	struct RTMP_DMACB *prRxCell;
	struct RTMP_DMABUF *prDmaBuf;

	prRxCell = &prRxRing->Cell[u4Idx];
	prDmaBuf = &prRxCell->DmaBuf;

	if (!prRxCell->pPacket)
		return;

	axiUnmapRxBuf(prHifInfo, prDmaBuf->AllocPa, prDmaBuf->AllocSize);

	DBGLOG_MEM32(HAL, INFO, ((struct sk_buff *)prRxCell->pPacket)->data,
		     u4DumpLen);

	prDmaBuf->AllocPa = axiMapRxBuf(prHifInfo, prDmaBuf->AllocVa,
					0, prDmaBuf->AllocSize);
}
#endif /* AXI_CFG_PREALLOC_MEMORY_BUFFER */
