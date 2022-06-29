/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Frederic Chen <frederic.chen@mediatek.com>
 *
 */

#ifndef _MTK_DIP_DIP_H_
#define _MTK_DIP_DIP_H_

// Standard C header file

// kernel header file

// mtk imgsys local header file
#include "mtk_imgsys-dev.h"
#include "mtk_imgsys-cmdq.h"

// Local header file
#include "./../mtk_imgsys-engine.h"


/********************************************************************
 * Global Define
 ********************************************************************/
/* DIP */
#define DIP_TOP_ADDR	0x15100000
#define TOP_CTL_OFT	0x0000
#define TOP_CTL_SZ	0x0334
#define DMATOP_OFT	0x1200
#define DMATOP_SZ	0x2540
#define NR3D_CTL_OFT	0x5000
#define NR3D_CTL_SZ	0x1AEC
#define SNRS_CTL_OFT	0x7400
#define SNRS_CTL_SZ	0xE0
#define UNP_D1_CTL_OFT	0x8000
#define UNP_D1_CTL_SZ	0x20C
#define SMT_D1_CTL_OFT	0x83C0
#define SMT_D1_CTL_SZ	0x3C4

#define DIP_DBG_SEL		0x1D8
#define DIP_DBG_OUT		0x1DC
#define DIP_DMA_DBG_SEL		0x10C0
#define DIP_DMA_DBG_PORT	0x10C4
#define DIP_NR3D_DBG_SEL	0x501C
#define DIP_NR3D_DBG_CNT	0x5020
#define DIP_NR3D_DBG_ST		0x5024
#define DIP_NR3D_DBG_POINTS	72

/* DIP NR1 */
#define DIP_NR1_ADDR		0x15150000
#define SNR_D1_CTL_OFT		0x4400
#define SNR_D1_CTL_SZ		0x348
#define EE_D1_CTL_OFT		0x4C40
#define EE_D1_CTL_SZ		0x138
#define TNC_BCE_CTL_OFT		0x7000
#define TNC_BCE_CTL_SZ		0x10
#define TNC_TILE_CTL_OFT	0x7D90
#define TNC_TILE_CTL_SZ		0xC
#define TNC_C2G_CTL_OFT		0x8004
#define TNC_C2G_CTL_SZ		0x124
#define TNC_C3D_CTL_OFT		0xA000
#define TNC_C3D_CTL_SZ		0x84

/* DIP NR2 */
#define DIP_NR2_ADDR		0x15160000
#define VIPI_D1_CTL_OFT		0x1440
#define VIPI_D1_CTL_SZ		0x800
#define SNRCSI_D1_CTL_OFT	0x2240
#define SNRCSI_D1_CTL_SZ	0x1140
#define SMTCO_D4_CTL_OFT	0x3640
#define SMTCO_D4_CTL_SZ		0x1FE4
#define DRZH2N_D2_CTL_OFT	0x6D00
#define DRZH2N_D2_CTL_SZ	0x54

#define DIP_DMA_NAME_MAX_SIZE	20

#define DIP_IMGI_STATE_CHECKSUM			(0x00100)
#define DIP_IMGI_LINE_PIX_CNT_TMP		(0x00200)
#define DIP_IMGI_LINE_PIX_CNT			(0x00300)
#define DIP_IMGI_IMPORTANT_STATUS		(0x00400)
#define DIP_IMGI_SMI_DEBUG_DATA_CASE0		(0x00500)
#define DIP_IMGI_TILEX_BYTE_CNT			(0x00600)
#define DIP_IMGI_TILEY_CNT			(0x00700)
#define DIP_IMGI_BURST_LINE_CNT			(0x00800)
#define DIP_IMGI_XFER_Y_CNT			(0x00900)
#define DIP_IMGI_FIFO_DEBUG_DATA_CASE1		(0x10600)
#define DIP_IMGI_FIFO_DEBUG_DATA_CASE3		(0x30600)
#define DIP_YUVO_T1_FIFO_DEBUG_DATA_CASE1	(0x10700)
#define DIP_YUVO_T1_FIFO_DEBUG_DATA_CASE3	(0x30700)

/********************************************************************
 * Enum Define
 ********************************************************************/
enum DIPDmaDebugType {
	DIP_ORI_RDMA_DEBUG,
	DIP_ORI_RDMA_UFD_DEBUG,
	DIP_ORI_WDMA_DEBUG,
	DIP_ULC_RDMA_DEBUG,
	DIP_ULC_WDMA_DEBUG,
};

/********************************************************************
 * Structure Define
 ********************************************************************/
struct DIPDmaDebugInfo {
	char DMAName[DIP_DMA_NAME_MAX_SIZE];
	enum DIPDmaDebugType DMADebugType;
	unsigned int DMAIdx;
};



//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Public Functions
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void imgsys_dip_set_initial_value(struct mtk_imgsys_dev *imgsys_dev);
void imgsys_dip_set_hw_initial_value(struct mtk_imgsys_dev *imgsys_dev);
void imgsys_dip_debug_dump(struct mtk_imgsys_dev *imgsys_dev,
							unsigned int engine);

void imgsys_dip_uninit(struct mtk_imgsys_dev *imgsys_dev);

#endif /* _MTK_DIP_DIP_H_ */

