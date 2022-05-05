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

/*******************************************************************************
 *                               Porting Part
 ******************************************************************************/
#define CAMSYS_NODE_COMPATIBLE "mediatek,mt6855-camsys_main"
#define PDA_0_RESET_BITMASK (BIT(6) | BIT(7))
#define PDA_1_RESET_BITMASK (BIT(6) | BIT(7))

// clock relate
static const char * const clk_names[] = {
	"camsys_mraw_pda0",
	"mraw_larbx",
	"cam_main_cam2mm0_gals_cg_con",
	"cam_main_cam_cg_con",
};
#define PDA_CLK_NUM ARRAY_SIZE(clk_names)
struct PDA_CLK_STRUCT pda_clk[PDA_CLK_NUM];

#ifdef PDA_MMQOS
// mmqos relate
static const char * const mmqos_names_rdma[] = {
	"l13_pdai_a0",
	"l13_pdai_a1"
};
#define PDA_MMQOS_RDMA_NUM ARRAY_SIZE(mmqos_names_rdma)
struct icc_path *icc_path_pda_rdma[PDA_MMQOS_RDMA_NUM];

static const char * const mmqos_names_wdma[] = {
	"l13_pdao_a"
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
	for (i = 0; i < PDA_MMQOS_WDMA_NUM; ++i) {
		LOG_INF("wdma index: %d, mmqos name: %s\n", i, mmqos_names_wdma[i]);
		icc_path_pda_wdma[i] = of_mtk_icc_get(pdev, mmqos_names_wdma[i]);
	}
}

void pda_mmqos_bw_set(void)
{
	int i = 0;
	// int Inter_ROI_Max_Width = 1024;
	// int Inter_ROI_Max_Height = 96;
	int Inter_Frame_Size_Width = 2048;
	int Inter_Frame_Size_Height = 192;

	int Mach_ROI_Max_Width = 2048;
	int Mach_ROI_Max_Height = 96;
	int Mach_Frame_Size_Width = 4096;
	int Mach_Frame_Size_Height = 192;

	double Freqency = 360.0;
	int FOV = 100;
	int ROI_Number = 45;
	int Frame_Rate = 30;
	// int Expected_HW_Compute_time = 15;
	int Search_Range = 40;

	// Intermediate data
	double Operation_Margin = 1.2;
	int Inter_Frame_Size = Inter_Frame_Size_Width * Inter_Frame_Size_Height;
	int Mach_Frame_Size = Mach_Frame_Size_Width * Mach_Frame_Size_Height;
	// int Inter_Factor = 8;
	int Inter_Frame_Size_FOV = Inter_Frame_Size * FOV / 100;
	int Mach_Frame_Size_FOV = Mach_Frame_Size * FOV / 100;
	int Inter_Input_Total_pixel_Itar = Inter_Frame_Size_FOV;
	int Inter_Input_Total_pixel_Iref = Inter_Frame_Size_FOV;
	int Mach_Input_Total_pixel_Itar =
		(ROI_Number*Search_Range*Mach_ROI_Max_Height) +
		Mach_Frame_Size_FOV +
		(1*Mach_ROI_Max_Width);
	// int Mach_Input_Total_pixel_Iref = Mach_Frame_Size_FOV;
	int Required_Operation_Cycle =
		(int)(Mach_Input_Total_pixel_Itar *
		Operation_Margin *
		(Search_Range+1)) /
		Search_Range + 1;
	int WDMA_Data = OUT_BYTE_PER_ROI*ROI_Number;
	int temp = Inter_Input_Total_pixel_Itar+Inter_Input_Total_pixel_Iref;
	int RDMA_Data = temp*20/8;

	double OperationTime = (double)Required_Operation_Cycle / Freqency / 1000.0;

	// WDMA BW estimate
	double WDMA_PEAK_BW = (double)WDMA_Data / (double)OperationTime / 1000.0;
	double WDMA_AVG_BW = (double)WDMA_Data * Frame_Rate * (1.33) / 1000000.0;

	// RDMA BW estimate
	double RDMA_PEAK_BW = (double)RDMA_Data / (double)OperationTime / 1000.0;
	double RDMA_AVG_BW = (double)RDMA_Data / (1000000.0/(double)Frame_Rate);

	// Left/Right RDMA BW
	double IMAGE_TABLE_RDMA_PEAK_BW = RDMA_PEAK_BW / 2;
	double IMAGE_TABLE_RDMA_AVG_BW = RDMA_AVG_BW * (1.33) / 2;

	// pda is not HRT engine, no need to set HRT bw
	IMAGE_TABLE_RDMA_PEAK_BW = 0;
	WDMA_PEAK_BW = 0;

	LOG_INF("RDMA_BW AVG/PEAK: %d/%d, WDMA_BW AVG/PEAK: %d/%d\n",
		(int)MBps_to_icc(IMAGE_TABLE_RDMA_AVG_BW),
		(int)MBps_to_icc(IMAGE_TABLE_RDMA_PEAK_BW),
		(int)MBps_to_icc(WDMA_AVG_BW),
		(int)MBps_to_icc(WDMA_PEAK_BW));

	// MMQOS set bw
	for (i = 0; i < PDA_MMQOS_RDMA_NUM; ++i) {
		if (icc_path_pda_rdma[i]) {
			mtk_icc_set_bw(icc_path_pda_rdma[i],
				(int)MBps_to_icc(IMAGE_TABLE_RDMA_AVG_BW),
				(int)MBps_to_icc(IMAGE_TABLE_RDMA_PEAK_BW));
		}
	}

	for (i = 0; i < PDA_MMQOS_WDMA_NUM; ++i) {
		if (icc_path_pda_wdma[i]) {
			mtk_icc_set_bw(icc_path_pda_wdma[i],
				(int)MBps_to_icc(WDMA_AVG_BW),
				(int)MBps_to_icc(WDMA_PEAK_BW));
		}
	}
}

void pda_mmqos_bw_reset(void)
{
	int i = 0;

#ifdef FOR_DEBUG
	LOG_INF("mmqos reset\n");
#endif

	// MMQOS set bw
	for (i = 0; i < PDA_MMQOS_RDMA_NUM; ++i) {
		if (icc_path_pda_rdma[i])
			mtk_icc_set_bw(icc_path_pda_rdma[i], 0, 0);
	}
	for (i = 0; i < PDA_MMQOS_WDMA_NUM; ++i) {
		if (icc_path_pda_wdma[i])
			mtk_icc_set_bw(icc_path_pda_wdma[i], 0, 0);
	}
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

unsigned int GetResetBitMask(unsigned int PDA_Index)
{
	unsigned int ret = 0;

	if (PDA_Index == 0)
		ret = PDA_0_RESET_BITMASK;
	else if (PDA_Index == 1)
		ret = PDA_1_RESET_BITMASK;
	return ret;
}
