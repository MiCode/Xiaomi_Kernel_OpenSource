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

// Local header file
#include "mtk_imgsys-engine.h"

/********************************************************************
 * Global Define
 ********************************************************************/
#define TRAW_A_BASE_ADDR	0x15020000
#define TRAW_B_BASE_ADDR	0x15040000
#define TRAW_C_BASE_ADDR	0x15640000

#define TRAW_DMA_NAME_MAX_SIZE	20

#define TRAW_DMA_DBG_SEL	 (0x4070)
#define TRAW_DMA_DBG_PORT	 (0x3074)
#define TRAW_CTL_DBG_SEL	 (0x0190)
#define TRAW_CTL_DBG_PORT	 (0x0194)
#define TRAW_DIPCQ_CQ_EN	 (0x0200)
#define WPE_MACRO_SW_RST	 (0x000C)
#define WPE_MACRO_WPE_RST	 (0x0004)
#define WPE_MACRO_LARB11_RST	 (0x0001)


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
};

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Public Functions
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void imgsys_traw_set_initial_value(struct mtk_imgsys_dev *imgsys_dev);
void imgsys_traw_debug_dump(struct mtk_imgsys_dev *imgsys_dev,
							unsigned int engine);
void imgsys_traw_uninit(struct mtk_imgsys_dev *imgsys_dev);

#endif /* _MTK_IMGSYS_TRAW_H_ */
