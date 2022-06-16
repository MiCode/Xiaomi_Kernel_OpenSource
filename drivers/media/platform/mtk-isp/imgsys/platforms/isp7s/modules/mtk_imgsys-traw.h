/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 *
 * Author: Shih-fang Chuang <shih-fang.chuang@mediatek.com>
 *
 */

#ifndef _MTK_IMGSYS_TRAW_H_
#define _MTK_IMGSYS_TRAW_H_

// Standard C header file

// kernel header file

// mtk imgsys local header file
#include <mtk_imgsys-dev.h>
#include <mtk_imgsys-cmdq.h>

// Local header file
#include "./../mtk_imgsys-engine.h"

/********************************************************************
 * Global Define
 ********************************************************************/
#define TRAW_A_BASE_ADDR	0x15700000
#define TRAW_B_BASE_ADDR	0x15040000

#define IF_0_DEFINE 1
#define IOMMU_TF_CB_SUPPORT (1)

#define TRAW_DMA_NAME_MAX_SIZE	20

#define TRAW_DMA_DBG_SEL	 (0x4088)
#define TRAW_DMA_DBG_PORT	 (0x408C)
#define TRAW_CTL_DBG_SEL	 (0x0280)
#define TRAW_CTL_DBG_PORT	 (0x0284)
#define TRAW_DIPCQ_CQ_EN	 (0x0400)

#if IF_0_DEFINE //YWTBD K DBG
#define TRAW_IMGI_STATE_CHECKSUM		(0x00100)
#define TRAW_IMGI_LINE_PIX_CNT_TMP		(0x00200)
#define TRAW_IMGI_LINE_PIX_CNT			(0x00300)
#define TRAW_IMGI_IMPORTANT_STATUS		(0x00400)
#define TRAW_IMGI_SMI_DEBUG_DATA_CASE0		(0x00500)
#define TRAW_IMGI_TILEX_BYTE_CNT		(0x00600)
#define TRAW_IMGI_TILEY_CNT			(0x00700)
#define TRAW_IMGI_BURST_LINE_CNT		(0x00800)
#define TRAW_IMGI_XFER_Y_CNT			(0x00900)
#define TRAW_IMGI_FIFO_DEBUG_DATA_CASE1		(0x10600)
#define TRAW_IMGI_FIFO_DEBUG_DATA_CASE3		(0x30600)
#define TRAW_YUVO_T1_FIFO_DEBUG_DATA_CASE1	(0x10700)
#define TRAW_YUVO_T1_FIFO_DEBUG_DATA_CASE3	(0x30700)

#endif

#define	TRAW_DMA_IMGI_ADDR		(0x4100)
#define	TRAW_DMA_IMGBI_ADDR		(0x4170)
#define	TRAW_DMA_IMGCI_ADDR		(0x41E0)
#define	TRAW_DMA_UFDI_ADDR		(0x4250)
#define	TRAW_DMA_CACI_ADDR		(0x4480)

#define	TRAW_DMA_TIMGO_T1_ADDR		(0x47E0)
#define	TRAW_DMA_YUVO_T1_ADDR		(0x4890)
#define	TRAW_DMA_YUVBO_T1_ADDR		(0x4940)
#define	TRAW_DMA_YUVCO_T1_ADDR		(0x49F0)
#define	TRAW_DMA_YUVDO_T1_ADDR		(0x4AA0)
#define	TRAW_DMA_YUVO_T2_ADDR		(0x5000)
#define	TRAW_DMA_YUVBO_T2_ADDR		(0x5040)
#define	TRAW_DMA_YUVO_T3_ADDR		(0x5080)
#define	TRAW_DMA_YUVBO_T3_ADDR		(0x50C0)
#define	TRAW_DMA_YUVO_T4_ADDR		(0x5100)
#define	TRAW_DMA_YUVBO_T4_ADDR		(0x5140)
#define	TRAW_DMA_YUVO_T5_ADDR		(0x5180)
#define	TRAW_DMA_TNCSO_T1_ADDR		(0x51C0)
#define	TRAW_DMA_DRZS4NO_T1_ADDR	(0x5540)
#define	TRAW_DMA_LTMSO_T1_ADDR		(0x5600)

/********************************************************************
 * Enum Define
 ********************************************************************/
enum TRAWDmaDebugType {
	TRAW_ORI_RDMA_DEBUG,
	TRAW_ORI_RDMA_UFD_DEBUG,
	TRAW_ORI_WDMA_DEBUG,
	TRAW_ULC_RDMA_DEBUG,
	TRAW_ULC_WDMA_DEBUG,
};

/********************************************************************
 * Structure Define
 ********************************************************************/
struct TRAWDmaDebugInfo {
	char DMAName[TRAW_DMA_NAME_MAX_SIZE];
	enum TRAWDmaDebugType DMADebugType;
	char even_odd_mode;
};

#define EVEN_ODD_MODE(x) (x)
#define EVEN_ODD_SEL (0x1000)

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Public Functions
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void imgsys_traw_set_initial_value(struct mtk_imgsys_dev *imgsys_dev);
void imgsys_traw_set_initial_value_hw(struct mtk_imgsys_dev *imgsys_dev);
void imgsys_traw_debug_dump(struct mtk_imgsys_dev *imgsys_dev,
							unsigned int engine);
void imgsys_traw_uninit(struct mtk_imgsys_dev *imgsys_dev);

#endif /* _MTK_IMGSYS_TRAW_H_ */
