// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/jiffies.h>

#include <linux/time.h>		//do_gettimeofday()

#include "mtk-interconnect.h"

#include "camera_pda.h"

// --------- define region --------
// #define FOR_DEBUG
#define PDA_MMQOS
// --------------------------------

#define PDA_DEV_NAME "camera-pda"

#define LOG_INF(format, args...)                                               \
	pr_info(PDA_DEV_NAME " [%s] " format, __func__, ##args)

//define the write register function
#define mt_reg_sync_writel(v, a) \
	do {    \
		*(unsigned int *)(a) = (v);    \
		mb();  /*make sure register access in order */ \
	} while (0)
#define PDA_WR32(addr, data) mt_reg_sync_writel(data, addr)
#define PDA_RD32(addr) ioread32(addr)

static unsigned int g_Frame_Width, g_Frame_Height, g_B_N;

/*******************************************************************************
 *                               Porting Part
 ******************************************************************************/
#define CAMSYS_NODE_COMPATIBLE "mediatek,mt6985-camsys_mraw"
#define PDA_0_RESET_BITMASK (BIT(12) | BIT(13))
#define PDA_1_RESET_BITMASK (BIT(14) | BIT(15))

// clock relate
static const char * const clk_names[] = {
	"camsys_mraw_pda0",
	"camsys_mraw_pda1",
	"mraw_larbx",
	"cam_main_cam2mm0_gals_cg_con",
	"cam_main_cam2mm1_gals_cg_con",
	"cam_main_cam_cg_con",
};
#define PDA_CLK_NUM ARRAY_SIZE(clk_names)
struct PDA_CLK_STRUCT pda_clk[PDA_CLK_NUM];

#ifdef PDA_MMQOS
// mmqos relate
static const char * const mmqos_names_rdma[] = {
	"l25_pdai_a0",
	"l25_pdai_a1",
	"l26_pdai_b0",
	"l26_pdai_b1"
};
#define PDA_MMQOS_RDMA_NUM ARRAY_SIZE(mmqos_names_rdma)
struct icc_path *icc_path_pda_rdma[PDA_MMQOS_RDMA_NUM];

static const char * const mmqos_names_rdma_b[] = {
	"l25_pdai_a2",
	"l26_pdai_b2",
	"l25_pdai_a3",
	"l26_pdai_b3",
	"l25_pdai_a4",
	"l26_pdai_b4"
};
#define PDA_MMQOS_RDMA_B_NUM ARRAY_SIZE(mmqos_names_rdma_b)
struct icc_path *icc_path_pda_rdma_b[PDA_MMQOS_RDMA_B_NUM];

static const char * const mmqos_names_wdma[] = {
	"l25_pdao_a",
	"l26_pdao_b"
};
#define PDA_MMQOS_WDMA_NUM ARRAY_SIZE(mmqos_names_wdma)
struct icc_path *icc_path_pda_wdma[PDA_MMQOS_WDMA_NUM];
#endif

/*******************************************************************************
 *                               Internal function
 ******************************************************************************/
struct device *init_larb(struct platform_device *pdev, int idx)
{
	struct device_node *node;
	struct platform_device *larb_pdev;
	struct device_link *link;

	/* get larb node from dts */
	node = of_parse_phandle(pdev->dev.of_node, "mediatek,larbs", idx);
	if (!node) {
		LOG_INF("fail to parse mediatek,larb\n");
		return NULL;
	}

	larb_pdev = of_find_device_by_node(node);
	if (WARN_ON(!larb_pdev)) {
		of_node_put(node);
		LOG_INF("no larb for idx %d\n", idx);
		return NULL;
	}
	of_node_put(node);

	link = device_link_add(&pdev->dev, &larb_pdev->dev,
					DL_FLAG_PM_RUNTIME | DL_FLAG_STATELESS);
	if (!link)
		LOG_INF("unable to link smi larb%d\n", idx);

	LOG_INF("pdev %p idx %d\n", pdev, idx);

	return &larb_pdev->dev;
}

/*******************************************************************************
 *                                     API
 ******************************************************************************/
#ifdef PDA_MMQOS
void pda_mmqos_init(struct device *pdev)
{
	int i = 0;

	// get interconnect path for MMQOS
	for (i = 0; i < PDA_MMQOS_RDMA_NUM; ++i) {
		LOG_INF("rdma index: %d, mmqos name: %s\n", i, mmqos_names_rdma[i]);
		icc_path_pda_rdma[i] = of_mtk_icc_get(pdev, mmqos_names_rdma[i]);
	}

	// get interconnect path for MMQOS
	for (i = 0; i < PDA_MMQOS_RDMA_B_NUM; ++i) {
		LOG_INF("rdma b index: %d, mmqos name: %s\n", i, mmqos_names_rdma_b[i]);
		icc_path_pda_rdma_b[i] = of_mtk_icc_get(pdev, mmqos_names_rdma_b[i]);
	}

	// get interconnect path for MMQOS
	for (i = 0; i < PDA_MMQOS_WDMA_NUM; ++i) {
		LOG_INF("wdma index: %d, mmqos name: %s\n", i, mmqos_names_wdma[i]);
		icc_path_pda_wdma[i] = of_mtk_icc_get(pdev, mmqos_names_wdma[i]);
	}
}

void pda_mmqos_bw_set(struct _pda_a_reg_t_ *PDA_FrameSetting)
{
	int i = 0;
	unsigned int Inter_Frame_Size_Width = 0;
	unsigned int Inter_Frame_Size_Height = 0;
	unsigned int B_N = 0;

	unsigned int Mach_ROI_Max_Width = 2048;
	unsigned int Mach_ROI_Max_Height = 96;
	unsigned int Mach_Frame_Size_Width = 4096;
	unsigned int Mach_Frame_Size_Height = 192;

	unsigned int Freqency = 360;
	unsigned int FOV = 200;
	unsigned int ROI_Number = PDA_MAXROI_PER_ROUND;
	unsigned int Frame_Rate = 30;
	unsigned int Search_Range = 40;
	#define Operation_Margin (12 / 10)

	unsigned int Inter_Frame_Size = 0, Mach_Frame_Size = 0;
	unsigned int Inter_Frame_Size_FOV = 0, Mach_Frame_Size_FOV = 0;
	unsigned int Inter_Input_Total_pixel_Itar = 0;
	unsigned int Inter_Input_Total_pixel_Iref = 0;
	unsigned int Mach_Input_Total_pixel_Itar = 0;

	unsigned int Required_Operation_Cycle = 0;
	unsigned int WDMA_Data = 0, temp = 0, RDMA_Data = 0;
	unsigned int OperationTime = 0;

	unsigned int WDMA_PEAK_BW = 0, WDMA_AVG_BW = 0;
	unsigned int RDMA_PEAK_BW = 0, RDMA_AVG_BW = 0;
	unsigned int IMAGE_TABLE_RDMA_PEAK_BW = 0;
	unsigned int IMAGE_TABLE_RDMA_AVG_BW = 0;

	unsigned int IMAGE_IMAGE_RDMA_PEAK_BW = 0;
	unsigned int IMAGE_IMAGE_RDMA_AVG_BW = 0;

	// -------------------------- parameter estimate ------------------------
	Inter_Frame_Size_Width = PDA_FrameSetting->PDA_CFG_0.Bits.PDA_WIDTH;
	Inter_Frame_Size_Height = PDA_FrameSetting->PDA_CFG_0.Bits.PDA_HEIGHT;
	B_N = PDA_FrameSetting->PDA_CFG_254.Bits.PDA_B_N;

	if (g_Frame_Width == Inter_Frame_Size_Width &&
		g_Frame_Height == Inter_Frame_Size_Height &&
		g_B_N == B_N) {
#ifdef FOR_DEBUG
		LOG_INF("Frame WIDTH/HEIGHT/B_N no change, no need to set qos\n");
#endif
		return;
	}

	FOV = (B_N > 0) ? 100 : 200;

#ifdef FOR_DEBUG
	LOG_INF("Frame WIDTH/HEIGHT/B_N: %d/%d/%d\n",
		Inter_Frame_Size_Width,
		Inter_Frame_Size_Height,
		B_N);
#endif

	Inter_Frame_Size = Inter_Frame_Size_Width * Inter_Frame_Size_Height;
#ifdef FOR_DEBUG
	LOG_INF("E16 Inter_Frame_Size: %d\n", Inter_Frame_Size);
#endif
	Mach_Frame_Size = Mach_Frame_Size_Width * Mach_Frame_Size_Height;

	Inter_Frame_Size_FOV = Inter_Frame_Size * FOV / 100;
#ifdef FOR_DEBUG
	LOG_INF("E19 Inter_Frame_Size_FOV: %d\n", Inter_Frame_Size_FOV);
#endif
	Mach_Frame_Size_FOV = Mach_Frame_Size * FOV / 100;
	Inter_Input_Total_pixel_Itar = Inter_Frame_Size_FOV;
	Inter_Input_Total_pixel_Iref = Inter_Frame_Size_FOV;
	Mach_Input_Total_pixel_Itar =
		(ROI_Number*Search_Range*Mach_ROI_Max_Height) +
		Mach_Frame_Size_FOV +
		(1*Mach_ROI_Max_Width);

	Required_Operation_Cycle =
		(unsigned int)(Mach_Input_Total_pixel_Itar *
		Operation_Margin *
		(Search_Range+1)) /
		Search_Range + 1;
	WDMA_Data = OUT_BYTE_PER_ROI*ROI_Number;
	temp = Inter_Input_Total_pixel_Itar+Inter_Input_Total_pixel_Iref;
	RDMA_Data = temp*(16+2)/8;
#ifdef FOR_DEBUG
	LOG_INF("E27 RDMA_Data: %d\n", RDMA_Data);
#endif

	OperationTime = Required_Operation_Cycle / Freqency / 1000;

	// WDMA BW estimate
	WDMA_PEAK_BW = WDMA_Data / OperationTime;
	WDMA_AVG_BW = WDMA_Data * Frame_Rate * 133 / 100 / 1000;

	// RDMA BW estimate
	RDMA_PEAK_BW = RDMA_Data / OperationTime;
	RDMA_AVG_BW = RDMA_Data * Frame_Rate / 1000;

	// Left/Right RDMA BW
	IMAGE_TABLE_RDMA_PEAK_BW = RDMA_PEAK_BW / 2;
	IMAGE_TABLE_RDMA_AVG_BW = RDMA_AVG_BW * 133 / 100 / 2;
	// IMAGE IMAGE SMI port is equal to IMAGE TABLE SMI port * 16/9
	IMAGE_IMAGE_RDMA_AVG_BW = IMAGE_TABLE_RDMA_AVG_BW * 16 / 9;

	// pda is not HRT engine, no need to set HRT bw
	IMAGE_TABLE_RDMA_PEAK_BW = 0;
	IMAGE_IMAGE_RDMA_PEAK_BW = 0;
	WDMA_PEAK_BW = 0;

	if (B_N > 0) {
		LOG_INF("RDMA_BW IT,II AVG/PEAK: %d/%d, %d/%d, WDMA_BW AVG/PEAK: %d/%d\n",
			IMAGE_TABLE_RDMA_AVG_BW,
			IMAGE_TABLE_RDMA_PEAK_BW,
			IMAGE_IMAGE_RDMA_AVG_BW,
			IMAGE_IMAGE_RDMA_PEAK_BW,
			WDMA_AVG_BW,
			WDMA_PEAK_BW);
	} else {
		LOG_INF("RDMA_BW IT AVG/PEAK: %d/%d, WDMA_BW AVG/PEAK: %d/%d\n",
			IMAGE_TABLE_RDMA_AVG_BW,
			IMAGE_TABLE_RDMA_PEAK_BW,
			WDMA_AVG_BW,
			WDMA_PEAK_BW);
	}

	// MMQOS set bw
	for (i = 0; i < PDA_MMQOS_RDMA_NUM; ++i) {
		if (icc_path_pda_rdma[i]) {
			mtk_icc_set_bw(icc_path_pda_rdma[i],
				(int)(IMAGE_TABLE_RDMA_AVG_BW),
				(int)(IMAGE_TABLE_RDMA_PEAK_BW));
		}
	}

	if (B_N <= 3) {
		// MMQOS set bw
		for (i = 0; i < (B_N * 2); ++i) {
			if (icc_path_pda_rdma_b[i]) {
				mtk_icc_set_bw(icc_path_pda_rdma_b[i],
					(int)(IMAGE_IMAGE_RDMA_AVG_BW),
					(int)(IMAGE_IMAGE_RDMA_PEAK_BW));
			}
		}
	} else {
		LOG_INF("B_N out of range, B_N:%d\n", B_N);
	}

	for (i = 0; i < PDA_MMQOS_WDMA_NUM; ++i) {
		if (icc_path_pda_wdma[i]) {
			mtk_icc_set_bw(icc_path_pda_wdma[i],
				(int)(WDMA_AVG_BW),
				(int)(WDMA_PEAK_BW));
		}
	}

	g_Frame_Width = Inter_Frame_Size_Width;
	g_Frame_Height = Inter_Frame_Size_Height;
	g_B_N = B_N;
}

void pda_mmqos_bw_reset(void)
{
	int i = 0;

#ifdef FOR_DEBUG
	LOG_INF("mmqos reset\n");
#endif

	// MMQOS reset bw
	for (i = 0; i < PDA_MMQOS_RDMA_NUM; ++i) {
		if (icc_path_pda_rdma[i])
			mtk_icc_set_bw(icc_path_pda_rdma[i], 0, 0);
	}

	for (i = 0; i < PDA_MMQOS_RDMA_B_NUM; ++i) {
		if (icc_path_pda_rdma_b[i])
			mtk_icc_set_bw(icc_path_pda_rdma_b[i], 0, 0);
	}

	for (i = 0; i < PDA_MMQOS_WDMA_NUM; ++i) {
		if (icc_path_pda_wdma[i])
			mtk_icc_set_bw(icc_path_pda_wdma[i], 0, 0);
	}

	g_Frame_Width = 0;
	g_Frame_Height = 0;
	g_B_N = 0;
}
#endif

void pda_init_larb(struct platform_device *pdev)
{
	int larbs, i;
	struct device *larb;

	// must porting in dts
	larbs = of_count_phandle_with_args(
				pdev->dev.of_node, "mediatek,larbs", NULL);
	LOG_INF("larb_num:%d\n", larbs);
	for (i = 0; i < larbs; i++)
		larb = init_larb(pdev, i);
}

int pda_devm_clk_get(struct platform_device *pdev)
{
	int i = 0;

	for (i = 0; i < PDA_CLK_NUM; ++i) {
		// CCF: Grab clock pointer (struct clk*)
		LOG_INF("index: %d, clock name: %s\n", i, clk_names[i]);
		pda_clk[i].CG_PDA_TOP_MUX = devm_clk_get(&pdev->dev, clk_names[i]);
		if (IS_ERR(pda_clk[i].CG_PDA_TOP_MUX)) {
			LOG_INF("cannot get %s clock\n", clk_names[i]);
			return PTR_ERR(pda_clk[i].CG_PDA_TOP_MUX);
		}
	}
	return 0;
}

void pda_clk_prepare_enable(void)
{
	int ret, i;

	for (i = 0; i < PDA_CLK_NUM; i++) {
		ret = clk_prepare_enable(pda_clk[i].CG_PDA_TOP_MUX);
		if (ret)
			LOG_INF("cannot enable clock (%s)\n", clk_names[i]);
#ifdef FOR_DEBUG
		LOG_INF("clk_prepare_enable (%s) done", clk_names[i]);
#endif
	}
#ifdef FOR_DEBUG
	LOG_INF("clk_prepare_enable done");
#endif
}

void pda_clk_disable_unprepare(void)
{
	int i;

	for (i = 0; i < PDA_CLK_NUM; i++) {
		clk_disable_unprepare(pda_clk[i].CG_PDA_TOP_MUX);
#ifdef FOR_DEBUG
		LOG_INF("clk_disable_unprepare (%s) done\n", clk_names[i]);
#endif
	}
}

void __iomem *pda_get_camsys_address(void)
{
	struct device_node *camsys_node;

	// camsys node
	camsys_node = of_find_compatible_node(NULL, NULL, CAMSYS_NODE_COMPATIBLE);

	return of_iomap(camsys_node, 0);
}

unsigned int GetResetBitMask(int PDA_Index)
{
	unsigned int ret = 0;

	if (PDA_Index == 0)
		ret = PDA_0_RESET_BITMASK;
	else if (PDA_Index == 1)
		ret = PDA_1_RESET_BITMASK;
	return ret;
}
