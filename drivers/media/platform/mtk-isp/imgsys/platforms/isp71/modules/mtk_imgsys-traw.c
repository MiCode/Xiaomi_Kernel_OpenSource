// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 *
 * Author: Shih-Fang Chuang <shih-fang.chuang@mediatek.com>
 *
 */

 // Standard C header file

// kernel header file
#include <linux/device.h>
#include <linux/of_address.h>
#include <linux/dma-iommu.h>
#include <linux/pm_runtime.h>
#include <linux/remoteproc.h>
#include <dt-bindings/memory/mtk-memory-port.h>

// drivers/misc/mediatek/iommu/
#include "iommu_debug.h"


// mtk imgsys local header file

// Local header file
#include "mtk_imgsys-traw.h"

/********************************************************************
 * Global Define
 ********************************************************************/
#define TRAW_INIT_ARRAY_COUNT	1

#define TRAW_CTL_ADDR_OFST		0x330
#define TRAW_DMA_ADDR_OFST		0x4000
#define TRAW_DMA_ADDR_END		0x5300
#define TRAW_DATA_ADDR_OFST		0x8000
#define TRAW_MAX_ADDR_OFST		0xBD00


#define TRAW_HW_SET		3
#define WPE_HW_SET		3

#define TRAW_L9_PORT_CNT	25
#define TRAW_L11_PORT_CNT	16

/********************************************************************
 * Global Variable
 ********************************************************************/
const struct mtk_imgsys_init_array
			mtk_imgsys_traw_init_ary[TRAW_INIT_ARRAY_COUNT] = {
	{0x00A0, 0x80000000}, /* TRAWCTL_INT1_EN */
};

static struct TRAWDmaDebugInfo g_DMADbgIfo[] = {
	{"IMGI", TRAW_ORI_RDMA_DEBUG},
	{"IMGI_UFD", TRAW_ORI_RDMA_UFD_DEBUG},
	{"UFDI", TRAW_ORI_RDMA_DEBUG},
	{"IMGBI", TRAW_ORI_RDMA_DEBUG},
	{"IMGBI_UFD", TRAW_ORI_RDMA_UFD_DEBUG},
	{"IMGCI", TRAW_ORI_RDMA_DEBUG},
	{"IMGCI_UFD", TRAW_ORI_RDMA_UFD_DEBUG},
	{"SMTI_T1", TRAW_ULC_RDMA_DEBUG},
	{"SMTI_T2", TRAW_ULC_RDMA_DEBUG},
	{"SMTI_T3", TRAW_ULC_RDMA_DEBUG},
	{"SMTI_T4", TRAW_ULC_RDMA_DEBUG},
	{"SMTI_T5", TRAW_ULC_RDMA_DEBUG},
	{"CACI", TRAW_ULC_RDMA_DEBUG},
	{"TNCSTI_T1", TRAW_ULC_RDMA_DEBUG},
	{"TNCSTI_T2", TRAW_ULC_RDMA_DEBUG},
	{"TNCSTI_T3", TRAW_ULC_RDMA_DEBUG},
	{"TNCSTI_T4", TRAW_ULC_RDMA_DEBUG},
	{"TNCSTI_T5", TRAW_ULC_RDMA_DEBUG},
	{"YUVO_T1", TRAW_ORI_WDMA_DEBUG},
	{"YUVBO_T1", TRAW_ORI_WDMA_DEBUG},
	{"TIMGO", TRAW_ORI_WDMA_DEBUG},
	{"YUVCO", TRAW_ORI_WDMA_DEBUG},
	{"YUVDO", TRAW_ORI_WDMA_DEBUG},
	{"YUVO_T2", TRAW_ULC_WDMA_DEBUG},
	{"YUVBO_T2", TRAW_ULC_WDMA_DEBUG},
	{"YUVO_T3", TRAW_ULC_WDMA_DEBUG},
	{"YUVBO_T3", TRAW_ULC_WDMA_DEBUG},
	{"YUVO_T4", TRAW_ULC_WDMA_DEBUG},
	{"YUVBO_T4", TRAW_ULC_WDMA_DEBUG},
	{"YUVO_T5", TRAW_ORI_WDMA_DEBUG},
	{"TNCSO", TRAW_ULC_WDMA_DEBUG},
	{"TNCSBO", TRAW_ULC_WDMA_DEBUG},
	{"TNCSHO", TRAW_ULC_WDMA_DEBUG},
	{"TNCSYO", TRAW_ULC_WDMA_DEBUG},
	{"SMTO_T1", TRAW_ULC_WDMA_DEBUG},
	{"SMTO_T2", TRAW_ULC_WDMA_DEBUG},
	{"SMTO_T3", TRAW_ULC_WDMA_DEBUG},
	{"SMTO_T4", TRAW_ULC_WDMA_DEBUG},
	{"SMTO_T5", TRAW_ULC_WDMA_DEBUG},
	{"TNCSTO_T1", TRAW_ULC_WDMA_DEBUG},
	{"TNCSTO_T2", TRAW_ULC_WDMA_DEBUG},
	{"TNCSTO_T3", TRAW_ULC_WDMA_DEBUG},
	{"SMTCI_T1", TRAW_ULC_RDMA_DEBUG},
	{"SMTCI_T4", TRAW_ULC_RDMA_DEBUG},
	{"SMTCO_T1", TRAW_ULC_WDMA_DEBUG},
	{"SMTCO_T4", TRAW_ULC_WDMA_DEBUG},
	{"SMTI_T6", TRAW_ULC_RDMA_DEBUG},
	{"SMTI_T7", TRAW_ULC_RDMA_DEBUG},
	{"SMTO_T6", TRAW_ULC_WDMA_DEBUG},
	{"SMTO_T7", TRAW_ULC_WDMA_DEBUG},
	{"RZH1N2TO_T1", TRAW_ULC_WDMA_DEBUG},
	{"RZH1N2TBO_T1", TRAW_ULC_WDMA_DEBUG},
	{"DRZS4NO_T1", TRAW_ULC_WDMA_DEBUG},
	{"DBGO_T1", TRAW_ORI_WDMA_DEBUG},
	{"DBGBO_T1", TRAW_ORI_WDMA_DEBUG},
};

static unsigned int g_RegBaseAddr = TRAW_A_BASE_ADDR;

static void __iomem *g_trawRegBA, *g_ltrawRegBA, *g_xtrawRegBA;

static unsigned int g_IOMMUDumpPort;

static unsigned int g_IOMMUL9Def[TRAW_L9_PORT_CNT] = {
	MTK_M4U_PORT_ID(0, 0, 9, 0),	/* IMGI_T1_A */
	MTK_M4U_PORT_ID(0, 0, 9, 1),	/* UFDI_T1_A */
	MTK_M4U_PORT_ID(0, 0, 9, 2),	/* IMGBI_T1_A */
	MTK_M4U_PORT_ID(0, 0, 9, 3),	/* IMGCI_T1_A */
	MTK_M4U_PORT_ID(0, 0, 9, 4),	/* SMTI_T1_A */
	MTK_M4U_PORT_ID(0, 0, 9, 5),	/* SMTI_T4_A */
	MTK_M4U_PORT_ID(0, 0, 9, 6),	/* TNCSTI_T1_A */
	MTK_M4U_PORT_ID(0, 0, 9, 7),	/* TNCSTI_T4_A */
	MTK_M4U_PORT_ID(0, 0, 9, 8),	/* YUVO_T1_A */
	MTK_M4U_PORT_ID(0, 0, 9, 9),	/* YUVBO_T1_A */
	MTK_M4U_PORT_ID(0, 0, 9, 10),	/* YUVCO_T1_A */
	MTK_M4U_PORT_ID(0, 0, 9, 11),	/* TIMGO_T1_A */
	MTK_M4U_PORT_ID(0, 0, 9, 12),	/* YUVO_T2_A */
	MTK_M4U_PORT_ID(0, 0, 9, 13),	/* YUVO_T5_A */
	MTK_M4U_PORT_ID(0, 0, 9, 14),	/* IMGI_T1_B */
	MTK_M4U_PORT_ID(0, 0, 9, 15),	/* IMGBI_T1_B */
	MTK_M4U_PORT_ID(0, 0, 9, 16),	/* IMGCI_T1_B */
	MTK_M4U_PORT_ID(0, 0, 9, 17),	/* SMTI_T4_B */
	MTK_M4U_PORT_ID(0, 0, 9, 18),	/* TNCSO_T1_A */
	MTK_M4U_PORT_ID(0, 0, 9, 19),	/* SMTO_T1_A */
	MTK_M4U_PORT_ID(0, 0, 9, 20),	/* SMTO_T4_A */
	MTK_M4U_PORT_ID(0, 0, 9, 21),	/* TNCSTO_T1_A */
	MTK_M4U_PORT_ID(0, 0, 9, 22),	/* YUVO_T2_B */
	MTK_M4U_PORT_ID(0, 0, 9, 23),	/* YUVO_T5_B */
	MTK_M4U_PORT_ID(0, 0, 9, 24)	/* SMTO_T4_B */
};

static unsigned int g_IOMMUL11Def[TRAW_L11_PORT_CNT] = {
	MTK_M4U_PORT_ID(0, 0, 11, 9),	/* IMGI_T1_C */
	MTK_M4U_PORT_ID(0, 0, 11, 10),	/* IMGBI_T1_C */
	MTK_M4U_PORT_ID(0, 0, 11, 11),	/* IMGCI_T1_C */
	MTK_M4U_PORT_ID(0, 0, 11, 12),	/* SMTI_T1_C */
	MTK_M4U_PORT_ID(0, 0, 11, 13),	/* SMTI_T4_C */
	MTK_M4U_PORT_ID(0, 0, 11, 14),	/* SMTI_T6_C */
	MTK_M4U_PORT_ID(0, 0, 11, 15),	/* YUVO_T1_C */
	MTK_M4U_PORT_ID(0, 0, 11, 16),	/* YUVBO_T1_C */
	MTK_M4U_PORT_ID(0, 0, 11, 17),	/* YUVCO_T1_C */
	MTK_M4U_PORT_ID(0, 0, 11, 23),	/* TIMGO_T1_C */
	MTK_M4U_PORT_ID(0, 0, 11, 24),	/* YUVO_T2_C */
	MTK_M4U_PORT_ID(0, 0, 11, 25),	/* YUVO_T5_C */
	MTK_M4U_PORT_ID(0, 0, 11, 26),	/* SMTO_T1_C */
	MTK_M4U_PORT_ID(0, 0, 11, 27),	/* SMTO_T4_C */
	MTK_M4U_PORT_ID(0, 0, 11, 28),	/* SMTO_T6_C */
	MTK_M4U_PORT_ID(0, 0, 11, 29)	/* DBGO_T1_C */
};

static unsigned int ExeDbgCmd(struct mtk_imgsys_dev *a_pDev,
			void __iomem *a_pRegBA,
			unsigned int a_DdbSel,
			unsigned int a_DbgOut,
			unsigned int a_DbgCmd)
{
	unsigned int DbgData = 0;
	unsigned int DbgOutReg = g_RegBaseAddr + a_DbgOut;
	void __iomem *pDbgSel = (void *)(a_pRegBA + a_DdbSel);
	void __iomem *pDbgPort = (void *)(a_pRegBA + a_DbgOut);

	iowrite32(a_DbgCmd, pDbgSel);
	DbgData = (unsigned int)ioread32(pDbgPort);
	pr_info("[0x%08X](0x%08X,0x%08X)\n",
		a_DbgCmd, DbgOutReg, DbgData);

	return DbgData;
}

static void imgsys_traw_dump_dma(struct mtk_imgsys_dev *a_pDev,
				void __iomem *a_pRegBA,
				unsigned int a_DdbSel,
				unsigned int a_DbgOut)
{
	unsigned int Idx = 0;
	unsigned int DbgCmd = 0;
	unsigned int DmaDegInfoSize = sizeof(struct TRAWDmaDebugInfo);
	unsigned int DebugCnt = sizeof(g_DMADbgIfo)/DmaDegInfoSize;
	enum TRAWDmaDebugType DbgTy = TRAW_ORI_RDMA_DEBUG;

	/* Dump DMA Debug Info */
	for (Idx = 0; Idx < DebugCnt; Idx++) {
		/* state_checksum */
		DbgCmd = TRAW_IMGI_STATE_CHECKSUM + Idx;
		ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
		/* line_pix_cnt_tmp */
		DbgCmd = TRAW_IMGI_LINE_PIX_CNT_TMP + Idx;
		ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
		/* line_pix_cnt */
		DbgCmd = TRAW_IMGI_LINE_PIX_CNT + Idx;
		ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);

		DbgTy = g_DMADbgIfo[Idx].DMADebugType;

		/* important_status */
		if (DbgTy == TRAW_ULC_RDMA_DEBUG ||
			DbgTy == TRAW_ULC_WDMA_DEBUG) {
			DbgCmd = TRAW_IMGI_IMPORTANT_STATUS + Idx;
			ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut,
			DbgCmd);
		}

		/* smi_debug_data (case 0) or cmd_data_cnt */
		if (DbgTy == TRAW_ORI_RDMA_DEBUG ||
			DbgTy == TRAW_ULC_RDMA_DEBUG ||
			DbgTy == TRAW_ULC_WDMA_DEBUG) {
			DbgCmd = TRAW_IMGI_SMI_DEBUG_DATA_CASE0 + Idx;
			ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut,
			DbgCmd);
		}

		/* ULC_RDMA or ULC_WDMA */
		if (DbgTy == TRAW_ULC_RDMA_DEBUG ||
			DbgTy == TRAW_ULC_WDMA_DEBUG) {
			DbgCmd = TRAW_IMGI_TILEX_BYTE_CNT + Idx;
			ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut,
				DbgCmd);
			DbgCmd = TRAW_IMGI_TILEY_CNT + Idx;
			ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut,
				DbgCmd);
		}

		/* smi_dbg_data(case 0) or burst_line_cnt or input_v_cnt */
		if (DbgTy == TRAW_ORI_WDMA_DEBUG ||
			DbgTy == TRAW_ULC_RDMA_DEBUG ||
			DbgTy == TRAW_ULC_WDMA_DEBUG) {
			DbgCmd = TRAW_IMGI_BURST_LINE_CNT + Idx;
			ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut,
				DbgCmd);
		}

		/* ORI_RDMA */
		if (DbgTy == TRAW_ORI_RDMA_DEBUG) {
			DbgCmd = TRAW_IMGI_FIFO_DEBUG_DATA_CASE1 + Idx;
			ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut,
				DbgCmd);
			DbgCmd = TRAW_IMGI_FIFO_DEBUG_DATA_CASE3 + Idx;
			ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut,
				DbgCmd);
		}

		/* ORI_WDMA */
		if (DbgTy == TRAW_ORI_WDMA_DEBUG) {
			DbgCmd = TRAW_YUVO_T1_FIFO_DEBUG_DATA_CASE1 + Idx;
			ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut,
				DbgCmd);
			DbgCmd = TRAW_YUVO_T1_FIFO_DEBUG_DATA_CASE3 + Idx;
			ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut,
				DbgCmd);
		}

		/* xfer_y_cnt */
		if (DbgTy == TRAW_ULC_WDMA_DEBUG) {
			DbgCmd = TRAW_IMGI_XFER_Y_CNT + Idx;
			ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut,
				DbgCmd);
		}

	}

}

static void imgsys_traw_dump_cq(struct mtk_imgsys_dev *a_pDev,
				void __iomem *a_pRegBA,
				unsigned int a_DdbSel,
				unsigned int a_DbgOut)
{
	unsigned int DbgCmd = 0;
	void __iomem *pCQEn = (void *)(a_pRegBA + TRAW_DIPCQ_CQ_EN);


	/* arx/atx/drx/dtx_state */
	DbgCmd = 0x00000005;
	ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	/* Thr(0~3)_state */
	DbgCmd = 0x00010005;
	ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);

	/* Set DIPCQ_CQ_EN[28] to 1 */
	iowrite32(0x10000000, pCQEn);
	/* cqd0_checksum0 */
	DbgCmd = 0x00000005;
	ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	/* cqd0_checksum1 */
	DbgCmd = 0x00010005;
	ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	/* cqd0_checksum2 */
	DbgCmd = 0x00020005;
	ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	/* cqd1_checksum0 */
	DbgCmd = 0x00040005;
	ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	/* cqd1_checksum1 */
	DbgCmd = 0x00050005;
	ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	/* cqd1_checksum2 */
	DbgCmd = 0x00060005;
	ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	/* cqa0_checksum0 */
	DbgCmd = 0x00080005;
	ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	/* cqa0_checksum1 */
	DbgCmd = 0x00090005;
	ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	/* cqa0_checksum2 */
	DbgCmd = 0x000A0005;
	ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	/* cqa1_checksum0 */
	DbgCmd = 0x000C0005;
	ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	/* cqa1_checksum1 */
	DbgCmd = 0x000D0005;
	ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	/* cqa1_checksum2 */
	DbgCmd = 0x000E0005;
	ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);

}

static void imgsys_traw_dump_drzh2n(struct mtk_imgsys_dev *a_pDev,
				void __iomem *a_pRegBA,
				unsigned int a_DdbSel,
				unsigned int a_DbgOut)
{
	unsigned int DbgCmd = 0;


	/* drzh2n_t1 line_pix_cnt_tmp */
	DbgCmd = 0x0002C001;
	ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	/* drzh2n_t1 line_pix_cnt */
	DbgCmd = 0x0003C001;
	ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	/* drzh2n_t1 handshake signal */
	DbgCmd = 0x0004C001;
	ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	/* drzh2n_t2 line_pix_cnt_tmp */
	DbgCmd = 0x0002C101;
	ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	/* drzh2n_t2 line_pix_cnt */
	DbgCmd = 0x0003C101;
	ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	/* drzh2n_t2 handshake signal */
	DbgCmd = 0x0004C101;
	ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	/* drzh2n_t3 line_pix_cnt_tmp */
	DbgCmd = 0x0002C501;
	ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	/* drzh2n_t3 line_pix_cnt */
	DbgCmd = 0x0003C501;
	ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	/* drzh2n_t3 handshake signal */
	DbgCmd = 0x0004C501;
	ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	/* drzh2n_t4 line_pix_cnt_tmp */
	DbgCmd = 0x0002C601;
	ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	/* drzh2n_t4 line_pix_cnt */
	DbgCmd = 0x0003C601;
	ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	/* drzh2n_t4 handshake signal */
	DbgCmd = 0x0004C601;
	ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	/* drzh2n_t5 line_pix_cnt_tmp */
	DbgCmd = 0x0002C701;
	ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	/* drzh2n_t5 line_pix_cnt */
	DbgCmd = 0x0003C701;
	ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	/* drzh2n_t5 handshake signal */
	DbgCmd = 0x0004C701;
	ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	/* drzh2n_t6 line_pix_cnt_tmp */
	DbgCmd = 0x0002C801;
	ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	/* drzh2n_t6 line_pix_cnt */
	DbgCmd = 0x0003C801;
	ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	/* drzh2n_t6 handshake signal */
	DbgCmd = 0x0004C801;
	ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	/* rzh1n2t_t1 checksum */
	DbgCmd = 0x0001C201;
	ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	/* rzh1n2t_t1 tile line_pix_cnt */
	DbgCmd = 0x0003C201;
	ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	/* rzh1n2t_t1 tile protocal */
	DbgCmd = 0x0008C201;
	ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);

}

static void imgsys_traw_dump_smto(struct mtk_imgsys_dev *a_pDev,
				void __iomem *a_pRegBA,
				unsigned int a_DdbSel,
				unsigned int a_DbgOut)
{
	unsigned int DbgCmd = 0;


	/* smto_t3 line_pix_cnt_tmp */
	DbgCmd = 0x0002C401;
	ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	/* smto_t3 line_pix_cnt */
	DbgCmd = 0x0003C401;
	ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	/* smto_t3 handshake signal */
	DbgCmd = 0x0004C401;
	ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);

}

static void imgsys_traw_dump_dl(struct mtk_imgsys_dev *a_pDev,
				void __iomem *a_pRegBA,
				unsigned int a_DdbSel,
				unsigned int a_DbgOut)
{
	unsigned int DbgCmd = 0;
	unsigned int DbgData = 0;
	unsigned int DbgLineCnt = 0, DbgRdy = 0, DbgReq = 0;
	unsigned int DbgLineCntReg = 0;


	/* wpe_wif_t1_debug */
	/* sot_st,eol_st,eot_st,sof,sot,eol,eot,req,rdy,7b0,checksum_out */
	DbgCmd = 0x00000006;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgRdy = ((DbgData & 0x800000) > 0) ? 1 : 0;
	DbgReq = ((DbgData & 0x1000000) > 0) ? 1 : 0;
	pr_info("[wpe_wif_t1_debug]checksum(0x%X),rdy(%d) req(%d)\n",
		DbgData & 0xFFFF, DbgRdy, DbgReq);
	/* line_cnt[15:0],  pix_cnt[15:0] */
	DbgCmd = 0x00000007;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgLineCnt = (DbgData & 0xFFFF0000) / 0xFFFF;
	pr_info("[wpe_wif_t1_debug]pix_cnt(0x%X),line_cnt(0x%X)\n",
		DbgData & 0xFFFF, DbgLineCnt);
	/* line_cnt_reg[15:0], pix_cnt_reg[15:0] */
	DbgCmd = 0x00000008;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgLineCntReg = (DbgData & 0xFFFF0000) / 0xFFFF;
	pr_info("[wpe_wif_t1_debug]pix_cnt_reg(0x%X),line_cnt_reg(0x%X)\n",
		DbgData & 0xFFFF, DbgLineCntReg);

	/* wpe_wif_t2_debug */
	/* sot_st,eol_st,eot_st,sof,sot,eol,eot,req,rdy,7b0,checksum_out */
	DbgCmd = 0x00000106;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgRdy = ((DbgData & 0x800000) > 0) ? 1 : 0;
	DbgReq = ((DbgData & 0x1000000) > 0) ? 1 : 0;
	pr_info("[wpe_wif_t2_debug]checksum(0x%X),rdy(%d) req(%d)\n",
		DbgData & 0xFFFF, DbgRdy, DbgReq);
	/* line_cnt[15:0],  pix_cnt[15:0] */
	DbgCmd = 0x00000107;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgLineCnt = (DbgData & 0xFFFF0000) / 0xFFFF;
	pr_info("[wpe_wif_t2_debug]pix_cnt(0x%X),line_cnt(0x%X)\n",
		DbgData & 0xFFFF, DbgLineCnt);
	/* line_cnt_reg[15:0], pix_cnt_reg[15:0] */
	DbgCmd = 0x00000108;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgLineCntReg = (DbgData & 0xFFFF0000) / 0xFFFF;
	pr_info("[wpe_wif_t2_debug]pix_cnt_reg(0x%X),line_cnt_reg(0x%X)\n",
		DbgData & 0xFFFF, DbgLineCntReg);

	/* traw_dip_d1_debug */
	/* sot_st,eol_st,eot_st,sof,sot,eol,eot,req,rdy,7b0,checksum_out */
	DbgCmd = 0x00000206;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgRdy = ((DbgData & 0x800000) > 0) ? 1 : 0;
	DbgReq = ((DbgData & 0x1000000) > 0) ? 1 : 0;
	pr_info("[traw_dip_d1_debug]checksum(0x%X),rdy(%d) req(%d)\n",
		DbgData & 0xFFFF, DbgRdy, DbgReq);
	/* line_cnt[15:0],  pix_cnt[15:0] */
	DbgCmd = 0x00000207;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgLineCnt = (DbgData & 0xFFFF0000) / 0xFFFF;
	pr_info("[traw_dip_d1_debug]pix_cnt(0x%X),line_cnt(0x%X)\n",
		DbgData & 0xFFFF, DbgLineCnt);
	/* line_cnt_reg[15:0], pix_cnt_reg[15:0] */
	DbgCmd = 0x00000208;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgLineCntReg = (DbgData & 0xFFFF0000) / 0xFFFF;
	pr_info("[traw_dip_d1_debug]pix_cnt_reg(0x%X),line_cnt_reg(0x%X)\n",
		DbgData & 0xFFFF, DbgLineCntReg);

	/* wpe_wif_t3_debug */
	/* sot_st,eol_st,eot_st,sof,sot,eol,eot,req,rdy,7b0,checksum_out */
	DbgCmd = 0x00000306;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgRdy = ((DbgData & 0x800000) > 0) ? 1 : 0;
	DbgReq = ((DbgData & 0x1000000) > 0) ? 1 : 0;
	pr_info("[wpe_wif_t3_debug]checksum(0x%X),rdy(%d) req(%d)\n",
		DbgData & 0xFFFF, DbgRdy, DbgReq);
	/* line_cnt[15:0],  pix_cnt[15:0] */
	DbgCmd = 0x00000307;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgLineCnt = (DbgData & 0xFFFF0000) / 0xFFFF;
	pr_info("[wpe_wif_t3_debug]pix_cnt(0x%X),line_cnt(0x%X)\n",
		DbgData & 0xFFFF, DbgLineCnt);
	/* line_cnt_reg[15:0], pix_cnt_reg[15:0] */
	DbgCmd = 0x00000308;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgLineCntReg = (DbgData & 0xFFFF0000) / 0xFFFF;
	pr_info("[wpe_wif_t3_debug]pix_cnt_reg(0x%X),line_cnt_reg(0x%X)\n",
		DbgData & 0xFFFF, DbgLineCntReg);

	/* adl_wif_t4_debug */
	/* sot_st,eol_st,eot_st,sof,sot,eol,eot,req,rdy,7b0,checksum_out */
	DbgCmd = 0x00000406;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgRdy = ((DbgData & 0x800000) > 0) ? 1 : 0;
	DbgReq = ((DbgData & 0x1000000) > 0) ? 1 : 0;
	pr_info("[adl_wif_t4_debug]checksum(0x%X),rdy(%d) req(%d)\n",
		DbgData & 0xFFFF, DbgRdy, DbgReq);
	/* line_cnt[15:0],  pix_cnt[15:0] */
	DbgCmd = 0x00000407;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgLineCnt = (DbgData & 0xFFFF0000) / 0xFFFF;
	pr_info("[adl_wif_t4_debug]pix_cnt(0x%X),line_cnt(0x%X)\n",
		DbgData & 0xFFFF, DbgLineCnt);
	/* line_cnt_reg[15:0], pix_cnt_reg[15:0] */
	DbgCmd = 0x00000408;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgLineCntReg = (DbgData & 0xFFFF0000) / 0xFFFF;
	pr_info("[adl_wif_t4_debug]pix_cnt_reg(0x%X),line_cnt_reg(0x%X)\n",
		DbgData & 0xFFFF, DbgLineCntReg);

	/* adl_wif_t4n_debug */
	/* sot_st,eol_st,eot_st,sof,sot,eol,eot,req,rdy,7b0,checksum_out */
	DbgCmd = 0x00000506;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgRdy = ((DbgData & 0x800000) > 0) ? 1 : 0;
	DbgReq = ((DbgData & 0x1000000) > 0) ? 1 : 0;
	pr_info("[adl_wif_t4n_debug]checksum(0x%X),rdy(%d) req(%d)\n",
		DbgData & 0xFFFF, DbgRdy, DbgReq);
	/* line_cnt[15:0],  pix_cnt[15:0] */
	DbgCmd = 0x00000507;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgLineCnt = (DbgData & 0xFFFF0000) / 0xFFFF;
	pr_info("[adl_wif_t4n_debug]pix_cnt(0x%X),line_cnt(0x%X)\n",
		DbgData & 0xFFFF, DbgLineCnt);
	/* line_cnt_reg[15:0], pix_cnt_reg[15:0] */
	DbgCmd = 0x00000508;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgLineCntReg = (DbgData & 0xFFFF0000) / 0xFFFF;
	pr_info("[adl_wif_t4n_debug]pix_cnt_reg(0x%X),line_cnt_reg(0x%X)\n",
		DbgData & 0xFFFF, DbgLineCntReg);

	/* adl_wif_t5_debug */
	/* sot_st,eol_st,eot_st,sof,sot,eol,eot,req,rdy,7b0,checksum_out */
	DbgCmd = 0x00000606;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgRdy = ((DbgData & 0x800000) > 0) ? 1 : 0;
	DbgReq = ((DbgData & 0x1000000) > 0) ? 1 : 0;
	pr_info("[adl_wif_t5_debug]checksum(0x%X),rdy(%d) req(%d)\n",
		DbgData & 0xFFFF, DbgRdy, DbgReq);
	/* line_cnt[15:0],  pix_cnt[15:0] */
	DbgCmd = 0x00000607;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgLineCnt = (DbgData & 0xFFFF0000) / 0xFFFF;
	pr_info("[adl_wif_t5_debug]pix_cnt(0x%X),line_cnt(0x%X)\n",
		DbgData & 0xFFFF, DbgLineCnt);
	/* line_cnt_reg[15:0], pix_cnt_reg[15:0] */
	DbgCmd = 0x00000608;
	DbgData = ExeDbgCmd(a_pDev, a_pRegBA, a_DdbSel, a_DbgOut, DbgCmd);
	DbgLineCntReg = (DbgData & 0xFFFF0000) / 0xFFFF;
	pr_info("[adl_wif_t5_debug]pix_cnt_reg(0x%X),line_cnt_reg(0x%X)\n",
		DbgData & 0xFFFF, DbgLineCntReg);

}

static int GetFaultDMAAddr(unsigned int port, unsigned int *pStartAddr, unsigned int *pEndAddr)
{
	int Result = 1;

	/* IMGI_T1 */
	if (port == MTK_M4U_PORT_ID(0, 0, 9, 0) ||
		port == MTK_M4U_PORT_ID(0, 0, 9, 14) ||
		port == MTK_M4U_PORT_ID(0, 0, 11, 9)) {
		*pStartAddr = TRAW_DMA_IMGI_ADDR;
		*pEndAddr = (*pStartAddr + 48);
	}
	/* UFDI_T1 */
	else if (port == MTK_M4U_PORT_ID(0, 0, 9, 1)) {
		*pStartAddr = TRAW_DMA_UFDI_ADDR;
		*pEndAddr = (*pStartAddr + 48);
	}
	/* IMGBI_T1 */
	else if (port == MTK_M4U_PORT_ID(0, 0, 9, 2) ||
		port == MTK_M4U_PORT_ID(0, 0, 9, 15) ||
		port == MTK_M4U_PORT_ID(0, 0, 11, 10)) {
		*pStartAddr = TRAW_DMA_IMGBI_ADDR;
		*pEndAddr = (*pStartAddr + 48);
	}
	/* IMGCI_T1 */
	else if (port == MTK_M4U_PORT_ID(0, 0, 9, 3) ||
		port == MTK_M4U_PORT_ID(0, 0, 9, 16) ||
		port == MTK_M4U_PORT_ID(0, 0, 11, 11)) {
		*pStartAddr = TRAW_DMA_IMGCI_ADDR;
		*pEndAddr = (*pStartAddr + 48);
	}
	/* YUVO_T1 */
	else if (port == MTK_M4U_PORT_ID(0, 0, 9, 8) ||
		port == MTK_M4U_PORT_ID(0, 0, 9, 9) ||
		port == MTK_M4U_PORT_ID(0, 0, 11, 15) ||
		port == MTK_M4U_PORT_ID(0, 0, 11, 16)) {
		*pStartAddr = TRAW_DMA_YUVO_T1_ADDR;
		*pEndAddr = (*pStartAddr + 224);
	}
	/* YUVCO_T1 */
	else if (port == MTK_M4U_PORT_ID(0, 0, 9, 10) ||
		port == MTK_M4U_PORT_ID(0, 0, 11, 17)) {
		*pStartAddr = TRAW_DMA_YUVCO_T1_ADDR;
		*pEndAddr = (*pStartAddr + 224);
	}
	/* TIMGO_T1 */
	else if (port == MTK_M4U_PORT_ID(0, 0, 9, 11) ||
		port == MTK_M4U_PORT_ID(0, 0, 11, 23)) {
		*pStartAddr = TRAW_DMA_TIMGO_T1_ADDR;
		*pEndAddr = (*pStartAddr + 48);
	}
	/* YUVO_T2 */
	else if (port == MTK_M4U_PORT_ID(0, 0, 9, 12) ||
		port == MTK_M4U_PORT_ID(0, 0, 9, 22) ||
		port == MTK_M4U_PORT_ID(0, 0, 11, 24)) {
		*pStartAddr = TRAW_DMA_YUVO_T2_ADDR;
		*pEndAddr = (*pStartAddr + 368);
	}
	/* YUVO_T5 */
	else if (port == MTK_M4U_PORT_ID(0, 0, 9, 13) ||
		port == MTK_M4U_PORT_ID(0, 0, 9, 23) ||
		port == MTK_M4U_PORT_ID(0, 0, 11, 25)) {
		*pStartAddr = TRAW_DMA_YUVO_T5_ADDR;
		*pEndAddr = (*pStartAddr + 48);
	}
	/* TNCSO_T1 */
	else if (port == MTK_M4U_PORT_ID(0, 0, 9, 18)) {
		*pStartAddr = TRAW_DMA_TNCSO_T1_ADDR;
		*pEndAddr = (*pStartAddr + 240);
	}
	/* SMTO_T6 */
	else if (port == MTK_M4U_PORT_ID(0, 0, 11, 28)) {
		*pStartAddr = TRAW_DMA_RZH1N2TO_T1_ADDR;
		*pEndAddr = (*pStartAddr + 176);
	}
	/* DBGO_T1 */
	else if (port == MTK_M4U_PORT_ID(0, 0, 11, 29)) {
		*pStartAddr = TRAW_DMA_DBGO_T1_ADDR;
		*pEndAddr = (*pStartAddr + 48);
	}
	/* Not Define Port */
	else
		Result = 0;



	return Result;


}

static int imgsys_traw_iommu_cb(int port, dma_addr_t mva, void *cb_data)
{
	unsigned int engine = IMGSYS_ENG_TRAW;
	void __iomem *trawRegBA = 0L;
	unsigned int i = 0;
	unsigned int DMAStartAddr = 0, DMAEndAddr = 0;
	unsigned int RegMap = REG_MAP_E_TRAW;
	char DbgStr[128];


	if (g_IOMMUDumpPort != port)
		g_IOMMUDumpPort = port;
	else
		return 0;

	/* Set HW Engine */
	if (port >= MTK_M4U_PORT_ID(0, 0, 9, 0) &&
		port <= MTK_M4U_PORT_ID(0, 0, 9, 13))
		engine = IMGSYS_ENG_TRAW;
	else if (port >= MTK_M4U_PORT_ID(0, 0, 9, 14) &&
		port <= MTK_M4U_PORT_ID(0, 0, 9, 17))
		engine = IMGSYS_ENG_LTR;
	else if (port >= MTK_M4U_PORT_ID(0, 0, 9, 18) &&
		port <= MTK_M4U_PORT_ID(0, 0, 9, 21))
		engine = IMGSYS_ENG_TRAW;
	else if (port >= MTK_M4U_PORT_ID(0, 0, 9, 22) &&
		port <= MTK_M4U_PORT_ID(0, 0, 9, 24))
		engine = IMGSYS_ENG_LTR;
	else if (port >= MTK_M4U_PORT_ID(0, 0, 11, 9) &&
		port <= MTK_M4U_PORT_ID(0, 0, 11, 17))
		engine = IMGSYS_ENG_XTR;
	else if (port >= MTK_M4U_PORT_ID(0, 0, 11, 23) &&
		port <= MTK_M4U_PORT_ID(0, 0, 11, 29))
		engine = IMGSYS_ENG_XTR;
	else
		return 0;

	/* ltraw */
	if (engine & IMGSYS_ENG_LTR) {
		RegMap = REG_MAP_E_LTRAW;
		g_RegBaseAddr = TRAW_B_BASE_ADDR;
		trawRegBA = g_ltrawRegBA;
	}
	/* xltraw */
	else if (engine & IMGSYS_ENG_XTR) {
		RegMap = REG_MAP_E_XTRAW;
		g_RegBaseAddr = TRAW_C_BASE_ADDR;
		trawRegBA = g_xtrawRegBA;
	}
	/* traw */
	else {
		g_RegBaseAddr = TRAW_A_BASE_ADDR;
		trawRegBA = g_trawRegBA;
	}

	if (!trawRegBA)
		return 0;

	/* Dump Fault DMA registers */
	if (GetFaultDMAAddr(port, &DMAStartAddr, &DMAEndAddr) == 0)
		return 0;

	for (i = DMAStartAddr; i <= DMAEndAddr; i += 16) {
		if (sprintf(DbgStr, "[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
			(unsigned int)(g_RegBaseAddr + i),
			(unsigned int)ioread32((void *)(trawRegBA + i)),
			(unsigned int)ioread32((void *)(trawRegBA + i + 4)),
			(unsigned int)ioread32((void *)(trawRegBA + i + 8)),
			(unsigned int)ioread32((void *)(trawRegBA + i + 12))) > 0)
			pr_info("%s\n", DbgStr);
	}

	return 0;

}

static void imgsys_traw_reg_iommu_cb(void)
{
	unsigned int i = 0;

	/* Reg Traw/Ltraw L9 Port Callback */
	for (i = 0; i < TRAW_L9_PORT_CNT; i++) {
		mtk_iommu_register_fault_callback(
			g_IOMMUL9Def[i],
			(mtk_iommu_fault_callback_t)imgsys_traw_iommu_cb,
			NULL, false);
	}

	/* Reg Xtraw L11 Port Callback */
	for (i = 0; i < TRAW_L11_PORT_CNT; i++) {
		mtk_iommu_register_fault_callback(
			g_IOMMUL11Def[i],
			(mtk_iommu_fault_callback_t)imgsys_traw_iommu_cb,
			NULL, false);
	}

}

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Public Functions
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void imgsys_traw_set_initial_value(struct mtk_imgsys_dev *imgsys_dev)
{
	/* iomap reg base */
	g_trawRegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_TRAW);
	g_ltrawRegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_LTRAW);
	g_xtrawRegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_XTRAW);
	imgsys_traw_reg_iommu_cb();
	/* Register IOMMU Callback */
	g_IOMMUDumpPort = 0;

	pr_debug("%s\n", __func__);
}

void imgsys_traw_set_initial_value_hw(struct mtk_imgsys_dev *imgsys_dev)
{
	void __iomem *trawRegBA = 0L;
	void __iomem *ofset = NULL;
	unsigned int i = 0, HwIdx = 0;

	for (HwIdx = 0; HwIdx < TRAW_HW_SET; HwIdx++) {
		if (HwIdx == 0)
			trawRegBA = g_trawRegBA;
		else if (HwIdx == 1)
			trawRegBA = g_ltrawRegBA;
		else
			trawRegBA = g_xtrawRegBA;

		if (!trawRegBA) {
			pr_info("%s: hw(%d)null reg base\n", __func__, HwIdx);
			break;
		}

		for (i = 0 ; i < TRAW_INIT_ARRAY_COUNT ; i++) {
			ofset = trawRegBA + mtk_imgsys_traw_init_ary[i].ofset;
			writel(mtk_imgsys_traw_init_ary[i].val, ofset);
		}
	}

	pr_debug("%s\n", __func__);
}

void imgsys_traw_debug_dump(struct mtk_imgsys_dev *imgsys_dev,
							unsigned int engine)
{
	void __iomem *trawRegBA = 0L;
	unsigned int i;
	unsigned int DMADdbSel = TRAW_DMA_DBG_SEL;
	unsigned int DMADbgOut = TRAW_DMA_DBG_PORT;
	unsigned int CtlDdbSel = TRAW_CTL_DBG_SEL;
	unsigned int CtlDbgOut = TRAW_CTL_DBG_PORT;
	unsigned int RegMap = REG_MAP_E_TRAW;
	char DbgStr[128];

	pr_info("%s: +\n", __func__);

	/* ltraw */
	if (engine & IMGSYS_ENG_LTR) {
		RegMap = REG_MAP_E_LTRAW;
		g_RegBaseAddr = TRAW_B_BASE_ADDR;
		trawRegBA = g_ltrawRegBA;
	}
	/* xltraw */
	else if (engine & IMGSYS_ENG_XTR) {
		RegMap = REG_MAP_E_XTRAW;
		g_RegBaseAddr = TRAW_C_BASE_ADDR;
		trawRegBA = g_xtrawRegBA;
	}
	/* traw */
	else {
		g_RegBaseAddr = TRAW_A_BASE_ADDR;
		trawRegBA = g_trawRegBA;
	}

	if (!trawRegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap regmap(%d)\n",
			__func__, RegMap);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
			__func__, imgsys_dev->dev->of_node->name);
		goto err_debug_dump;
	}

	/* DL debug data */
	imgsys_traw_dump_dl(imgsys_dev, trawRegBA, CtlDdbSel, CtlDbgOut);

	/* Ctrl registers */
	for (i = 0x0; i <= TRAW_CTL_ADDR_OFST; i += 16) {
		if (sprintf(DbgStr, "[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
			(unsigned int)(g_RegBaseAddr + i),
			(unsigned int)ioread32((void *)(trawRegBA + i)),
			(unsigned int)ioread32((void *)(trawRegBA + i + 4)),
			(unsigned int)ioread32((void *)(trawRegBA + i + 8)),
			(unsigned int)ioread32((void *)(trawRegBA + i + 12))) > 0)
			pr_info("%s\n", DbgStr);
	}
	/* Dma registers */
	for (i = TRAW_DMA_ADDR_OFST; i <= TRAW_DMA_ADDR_END; i += 16) {
		if (sprintf(DbgStr, "[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
			(unsigned int)(g_RegBaseAddr + i),
			(unsigned int)ioread32((void *)(trawRegBA + i)),
			(unsigned int)ioread32((void *)(trawRegBA + i + 4)),
			(unsigned int)ioread32((void *)(trawRegBA + i + 8)),
			(unsigned int)ioread32((void *)(trawRegBA + i + 12))) > 0)
			pr_info("%s\n", DbgStr);
	}
	/* Data registers */
	for (i = TRAW_DATA_ADDR_OFST; i <= TRAW_MAX_ADDR_OFST; i += 16) {
		if (sprintf(DbgStr, "[0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
			(unsigned int)(g_RegBaseAddr + i),
			(unsigned int)ioread32((void *)(trawRegBA + i)),
			(unsigned int)ioread32((void *)(trawRegBA + i + 4)),
			(unsigned int)ioread32((void *)(trawRegBA + i + 8)),
			(unsigned int)ioread32((void *)(trawRegBA + i + 12))) > 0)
			pr_info("%s\n", DbgStr);
	}

	/* DMA debug data */
	imgsys_traw_dump_dma(imgsys_dev, trawRegBA, DMADdbSel, DMADbgOut);
	/* CQ debug data */
	imgsys_traw_dump_cq(imgsys_dev, trawRegBA, CtlDdbSel, CtlDbgOut);
	/* DRZH2N debug data */
	imgsys_traw_dump_drzh2n(imgsys_dev, trawRegBA, CtlDdbSel, CtlDbgOut);
	/* SMTO debug data */
	imgsys_traw_dump_smto(imgsys_dev, trawRegBA, CtlDdbSel, CtlDbgOut);

err_debug_dump:
	pr_info("%s: -\n", __func__);
}

void imgsys_traw_uninit(struct mtk_imgsys_dev *imgsys_dev)
{
	if (g_trawRegBA) {
		iounmap(g_trawRegBA);
		g_trawRegBA = 0L;
	}
	if (g_ltrawRegBA) {
		iounmap(g_ltrawRegBA);
		g_ltrawRegBA = 0L;
	}
	if (g_xtrawRegBA) {
		iounmap(g_xtrawRegBA);
		g_xtrawRegBA = 0L;
	}

	pr_debug("%s\n", __func__);
}
