// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Christopher Chen <christopher.chen@mediatek.com>
 *
 */

#include <linux/device.h>
#include <linux/of_address.h>
#include <linux/dma-iommu.h>
#include <linux/pm_runtime.h>
#include <linux/remoteproc.h>
#include "mtk_imgsys-engine.h"
#include "mtk_imgsys-debug.h"
/* TODO */
#include "smi.h"

#define DL_CHECK_ENG_NUM 11
#define WPE_HW_SET    3
#define ADL_HW_SET    2
#define SW_RST   (0x000C)

struct imgsys_dbg_engine_t dbg_engine_name_list[DL_CHECK_ENG_NUM] = {
	{IMGSYS_ENG_WPE_EIS, "WPE_EIS"},
	{IMGSYS_ENG_WPE_TNR, "WPE_TNR"},
	{IMGSYS_ENG_WPE_LITE, "WPE_LITE"},
	{IMGSYS_ENG_TRAW, "TRAW"},
	{IMGSYS_ENG_LTR, "LTRAW"},
	{IMGSYS_ENG_XTR, "XTRAW"},
	{IMGSYS_ENG_DIP, "DIP"},
	{IMGSYS_ENG_PQDIP_A, "PQDIPA"},
	{IMGSYS_ENG_PQDIP_B, "PQDIPB"},
	{IMGSYS_ENG_ADL_A, "ADLA"},
	{IMGSYS_ENG_ADL_B, "ADLB"},
};

void __iomem *imgsysmainRegBA;
void __iomem *wpedip1RegBA;
void __iomem *wpedip2RegBA;
void __iomem *wpedip3RegBA;
void __iomem *dipRegBA;
void __iomem *dip1RegBA;
void __iomem *adlARegBA;
void __iomem *adlBRegBA;

void imgsys_main_init(struct mtk_imgsys_dev *imgsys_dev)
{
	struct resource adl;
	pr_info("%s: +.\n", __func__);

	imgsysmainRegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_TOP);
	if (!imgsysmainRegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap imgsys_top registers\n",
								__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
				__func__, imgsys_dev->dev->of_node->name);
		return;
	}

	wpedip1RegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_WPE1_DIP1);
	if (!wpedip1RegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap wpe_dip1 registers\n",
								__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
				__func__, imgsys_dev->dev->of_node->name);
		return;
	}

	wpedip2RegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_WPE2_DIP1);
	if (!wpedip2RegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap wpe_dip2 registers\n",
								__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
				__func__, imgsys_dev->dev->of_node->name);
		return;
	}

	wpedip3RegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_WPE3_DIP1);
	if (!wpedip3RegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap wpe_dip3 registers\n",
								__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
				__func__, imgsys_dev->dev->of_node->name);
		return;
	}

	dipRegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_DIP_TOP);
	if (!dipRegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap dip_top registers\n",
								__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
				__func__, imgsys_dev->dev->of_node->name);
		return;
	}

	dip1RegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_DIP_TOP_NR);
	if (!dip1RegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap dip_top_nr registers\n",
								__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
				__func__, imgsys_dev->dev->of_node->name);
		return;
	}

	of_address_to_resource(imgsys_dev->dev->of_node, REG_MAP_E_ADL_A, &adl);
	if (adl.start) {
		adlARegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_ADL_A);
		if (!adlARegBA) {
			dev_info(imgsys_dev->dev, "%s Unable to ioremap adl a registers\n",
									__func__);
			dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
					__func__, imgsys_dev->dev->of_node->name);
			return;
		}

		adlBRegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_ADL_B);
		if (!adlBRegBA) {
			dev_info(imgsys_dev->dev, "%s Unable to ioremap adl b registers\n",
									__func__);
			dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
					__func__, imgsys_dev->dev->of_node->name);
			return;
		}
	} else {
		adlARegBA = NULL;
		adlBRegBA = NULL;
		dev_info(imgsys_dev->dev, "%s Do not have ADL hardware.\n", __func__);
	}

	pr_info("%s: -.\n", __func__);
}

void imgsys_main_set_init(struct mtk_imgsys_dev *imgsys_dev)
{
	void __iomem *WpeRegBA = 0L;
	void __iomem *ADLRegBA = 0L;
	void __iomem *pWpeCtrl = 0L;
	unsigned int HwIdx = 0;
	uint32_t count;
	uint32_t value;
	int i, num;

	pr_debug("%s: +.\n", __func__);

	num = imgsys_dev->larbs_num - 1;
	for (i = 0; i < num; i++)
		mtk_smi_larb_clamp(imgsys_dev->larbs[i], 1);

	iowrite32(0xFFFFFFFF, (void *)(dipRegBA + SW_RST));
	iowrite32(0xFFFFFFFF, (void *)(dip1RegBA + SW_RST));

	for (HwIdx = 0; HwIdx < WPE_HW_SET; HwIdx++) {
		if (HwIdx == 0)
			WpeRegBA = wpedip1RegBA;
		else if (HwIdx == 1)
			WpeRegBA = wpedip2RegBA;
		else
			WpeRegBA = wpedip3RegBA;

		/* Wpe Macro HW Reset */
		pWpeCtrl = (void *)(WpeRegBA + SW_RST);
		iowrite32(0xFFFFFFFF, pWpeCtrl);
		/* Clear HW Reset */
		iowrite32(0x0, pWpeCtrl);
	}

	if (adlARegBA || adlBRegBA) {
		/* Reset ADL A */
		for (HwIdx = 0; HwIdx < ADL_HW_SET; HwIdx++) {
			if (HwIdx == 0)
				ADLRegBA = adlARegBA;
			else if (HwIdx == 1)
				ADLRegBA = adlBRegBA;

			if (!ADLRegBA)
				continue;

			value = ioread32((void *)(ADLRegBA + 0x300));
			value |= ((0x1 << 8) | (0x1 << 9));
			iowrite32(value, (ADLRegBA + 0x300));

			count = 0;
			while (count < 1000000) {
				value = ioread32((void *)(ADLRegBA + 0x300));
				if ((value & 0x3) == 0x3)
					break;
				count++;
			}

			value = ioread32((void *)(ADLRegBA + 0x300));
			value &= ~((0x1 << 8) | (0x1 << 9));
			iowrite32(value, (ADLRegBA + 0x300));
		}
	}

	iowrite32(0x00CF00FF, (void *)(imgsysmainRegBA + SW_RST));
	iowrite32(0x0, (void *)(imgsysmainRegBA + SW_RST));

	iowrite32(0x0, (void *)(dipRegBA + SW_RST));
	iowrite32(0x0, (void *)(dip1RegBA + SW_RST));

	for (HwIdx = 0; HwIdx < WPE_HW_SET; HwIdx++) {
		if (HwIdx == 0)
			WpeRegBA = wpedip1RegBA;
		else if (HwIdx == 1)
			WpeRegBA = wpedip2RegBA;
		else
			WpeRegBA = wpedip3RegBA;

		/* Wpe Macro HW Reset */
		pWpeCtrl = (void *)(WpeRegBA + SW_RST);
		iowrite32(0xFFFFFFFF, pWpeCtrl);
		/* Clear HW Reset */
		iowrite32(0x0, pWpeCtrl);
	}

	iowrite32(0x00CF00FF, (void *)(imgsysmainRegBA + SW_RST));
	iowrite32(0x0, (void *)(imgsysmainRegBA + SW_RST));

	for (i = 0; i < num; i++)
		mtk_smi_larb_clamp(imgsys_dev->larbs[i], 0);

	pr_debug("%s: -.\n", __func__);
}

void imgsys_main_uninit(struct mtk_imgsys_dev *imgsys_dev)
{
	pr_debug("%s: +.\n", __func__);

	if (imgsysmainRegBA) {
		iounmap(imgsysmainRegBA);
		imgsysmainRegBA = 0L;
	}

	if (wpedip1RegBA) {
		iounmap(wpedip1RegBA);
		wpedip1RegBA = 0L;
	}

	if (wpedip2RegBA) {
		iounmap(wpedip2RegBA);
		wpedip2RegBA = 0L;
	}

	if (wpedip3RegBA) {
		iounmap(wpedip3RegBA);
		wpedip3RegBA = 0L;
	}

	if (dipRegBA) {
		iounmap(dipRegBA);
		dipRegBA = 0L;
	}

	if (dip1RegBA) {
		iounmap(dip1RegBA);
		dip1RegBA = 0L;
	}

	if (adlARegBA) {
		iounmap(adlARegBA);
		adlARegBA = 0L;
	}

	if (adlBRegBA) {
		iounmap(adlBRegBA);
		adlBRegBA = 0L;
	}

	pr_debug("%s: -.\n", __func__);
}

void imgsys_debug_dump_routine(struct mtk_imgsys_dev *imgsys_dev,
	const struct module_ops *imgsys_modules,
	int imgsys_module_num, unsigned int hw_comb)
{
	bool module_on[IMGSYS_MOD_MAX] = {
		false, false, false, false, false, false, false};
	int i = 0;

	dev_info(imgsys_dev->dev,
			"%s: hw comb set: 0x%lx\n",
			__func__, hw_comb);

	imgsys_dl_debug_dump(imgsys_dev, hw_comb);

	if ((hw_comb & IMGSYS_ENG_WPE_EIS) || (hw_comb & IMGSYS_ENG_WPE_TNR)
		 || (hw_comb & IMGSYS_ENG_WPE_LITE))
		module_on[IMGSYS_MOD_WPE] = true;
	if ((hw_comb & IMGSYS_ENG_TRAW) || (hw_comb & IMGSYS_ENG_LTR)
		 || (hw_comb & IMGSYS_ENG_XTR))
		module_on[IMGSYS_MOD_TRAW] = true;
	if ((hw_comb & IMGSYS_ENG_DIP))
		module_on[IMGSYS_MOD_DIP] = true;
	if ((hw_comb & IMGSYS_ENG_PQDIP_A) || (hw_comb & IMGSYS_ENG_PQDIP_B))
		module_on[IMGSYS_MOD_PQDIP] = true;
	if ((hw_comb & IMGSYS_ENG_ME))
		module_on[IMGSYS_MOD_ME] = true;
	if ((hw_comb & IMGSYS_ENG_ADL_A) || (hw_comb & IMGSYS_ENG_ADL_B))
		module_on[IMGSYS_MOD_ADL] = true;

	/* in case module driver did not set imgsys_modules in module order */
	dev_info(imgsys_dev->dev,
			"%s: imgsys_module_num: %d\n",
			__func__, imgsys_module_num);
	for (i = 0 ; i < imgsys_module_num ; i++) {
		if (module_on[imgsys_modules[i].module_id])
			imgsys_modules[i].dump(imgsys_dev, hw_comb);
	}
}
EXPORT_SYMBOL(imgsys_debug_dump_routine);

void imgsys_cg_debug_dump(struct mtk_imgsys_dev *imgsys_dev)
{
	unsigned int i = 0;

	if (!imgsysmainRegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap imgsys_top registers\n",
								__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
				__func__, imgsys_dev->dev->of_node->name);
		return;
	}

	for (i = 0; i <= 0x500; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X]",
		__func__, (unsigned int)(0x15000000 + i),
		(unsigned int)ioread32((void *)(imgsysmainRegBA + i)),
		(unsigned int)(0x15000000 + i + 0x4),
		(unsigned int)ioread32((void *)(imgsysmainRegBA + i + 0x4)),
		(unsigned int)(0x15000000 + i + 0x8),
		(unsigned int)ioread32((void *)(imgsysmainRegBA + i + 0x8)),
		(unsigned int)(0x15000000 + i + 0xc),
		(unsigned int)ioread32((void *)(imgsysmainRegBA + i + 0xc)));
	}

	if (!dipRegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap dip registers\n",
								__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
				__func__, imgsys_dev->dev->of_node->name);
		return;
	}

	for (i = 0; i <= 0x100; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X]",
		__func__, (unsigned int)(0x15110000 + i),
		(unsigned int)ioread32((void *)(dipRegBA + i)),
		(unsigned int)(0x15110000 + i + 0x4),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
		(unsigned int)(0x15110000 + i + 0x8),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
		(unsigned int)(0x15110000 + i + 0xc),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}

	if (!dip1RegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap dip1 registers\n",
								__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
				__func__, imgsys_dev->dev->of_node->name);
		return;
	}

	for (i = 0; i <= 0x100; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X]",
		__func__, (unsigned int)(0x15130000 + i),
		(unsigned int)ioread32((void *)(dip1RegBA + i)),
		(unsigned int)(0x15130000 + i + 0x4),
		(unsigned int)ioread32((void *)(dip1RegBA + i + 0x4)),
		(unsigned int)(0x15130000 + i + 0x8),
		(unsigned int)ioread32((void *)(dip1RegBA + i + 0x8)),
		(unsigned int)(0x15130000 + i + 0xc),
		(unsigned int)ioread32((void *)(dip1RegBA + i + 0xc)));
	}

	if (!wpedip1RegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap wpe_dip1 registers\n",
								__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
				__func__, imgsys_dev->dev->of_node->name);
		return;
	}

	for (i = 0; i <= 0x100; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X]",
		__func__, (unsigned int)(0x15220000 + i),
		(unsigned int)ioread32((void *)(wpedip1RegBA + i)),
		(unsigned int)(0x15220000 + i + 0x4),
		(unsigned int)ioread32((void *)(wpedip1RegBA + i + 0x4)),
		(unsigned int)(0x15220000 + i + 0x8),
		(unsigned int)ioread32((void *)(wpedip1RegBA + i + 0x8)),
		(unsigned int)(0x15220000 + i + 0xc),
		(unsigned int)ioread32((void *)(wpedip1RegBA + i + 0xc)));
	}

	if (!wpedip2RegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap wpe_dip2 registers\n",
								__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
				__func__, imgsys_dev->dev->of_node->name);
		return;
	}

	for (i = 0; i <= 0x100; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X]",
		__func__, (unsigned int)(0x15520000 + i),
		(unsigned int)ioread32((void *)(wpedip2RegBA + i)),
		(unsigned int)(0x15520000 + i + 0x4),
		(unsigned int)ioread32((void *)(wpedip2RegBA + i + 0x4)),
		(unsigned int)(0x15520000 + i + 0x8),
		(unsigned int)ioread32((void *)(wpedip2RegBA + i + 0x8)),
		(unsigned int)(0x15520000 + i + 0xc),
		(unsigned int)ioread32((void *)(wpedip2RegBA + i + 0xc)));
	}

	if (!wpedip3RegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap wpe_dip3 registers\n",
								__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
				__func__, imgsys_dev->dev->of_node->name);
		return;
	}

	for (i = 0; i <= 0x100; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X]",
		__func__, (unsigned int)(0x15620000 + i),
		(unsigned int)ioread32((void *)(wpedip3RegBA + i)),
		(unsigned int)(0x15620000 + i + 0x4),
		(unsigned int)ioread32((void *)(wpedip3RegBA + i + 0x4)),
		(unsigned int)(0x15620000 + i + 0x8),
		(unsigned int)ioread32((void *)(wpedip3RegBA + i + 0x8)),
		(unsigned int)(0x15620000 + i + 0xc),
		(unsigned int)ioread32((void *)(wpedip3RegBA + i + 0xc)));
	}
}

#define log_length (64)
void imgsys_dl_checksum_dump(struct mtk_imgsys_dev *imgsys_dev,
	unsigned int hw_comb, char *logBuf_path,
	char *logBuf_inport, char *logBuf_outport, int dl_path)
{
	/*void __iomem *imgsysmainRegBA = 0L;*/
	/*void __iomem *wpedip1RegBA = 0L;*/
	/*void __iomem *wpedip2RegBA = 0L;*/
	/*void __iomem *wpedip3RegBA = 0L;*/
	unsigned int checksum_dbg_sel = 0x0;
	unsigned int original_dbg_sel_value = 0x0;
	char logBuf_final[log_length * 4];
	int debug0_req[2] = {0, 0};
	int debug0_rdy[2] = {0, 0};
	int debug0_checksum[2] = {0, 0};
	int debug1_line_cnt[2] = {0, 0};
	int debug1_pix_cnt[2] = {0, 0};
	int debug2_line_cnt[2] = {0, 0};
	int debug2_pix_cnt[2] = {0, 0};
	unsigned int dbg_sel_value[2] = {0x0, 0x0};
	unsigned int debug0_value[2] = {0x0, 0x0};
	unsigned int debug1_value[2] = {0x0, 0x0};
	unsigned int debug2_value[2] = {0x0, 0x0};
	unsigned int wpe_pqdip_mux_v = 0x0;
	unsigned int wpe_pqdip_mux2_v = 0x0;
	unsigned int wpe_pqdip_mux3_v = 0x0;
	char logBuf_temp[log_length];
	int ret;

	memset((char *)logBuf_final, 0x0, log_length * 4);
	logBuf_final[strlen(logBuf_final)] = '\0';
	memset((char *)logBuf_temp, 0x0, log_length);
	logBuf_temp[strlen(logBuf_temp)] = '\0';

	dev_info(imgsys_dev->dev,
		"%s: + hw_comb/path(0x%x/%s) dl_path:%d, start dump\n",
		__func__, hw_comb, logBuf_path, dl_path);
	/* iomap registers */
	/*imgsysmainRegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_TOP);*/
	if (!imgsysmainRegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap imgsys_top registers\n",
								__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
				__func__, imgsys_dev->dev->of_node->name);
		return;
	}

	/*dump former engine in DL (imgsys main in port) status */
	checksum_dbg_sel = (unsigned int)((dl_path << 1) | (0 << 0));
	original_dbg_sel_value = (unsigned int)ioread32((void *)(imgsysmainRegBA + 0x4C));
	original_dbg_sel_value = original_dbg_sel_value & 0xff00ffff; /*clear last time data*/
	dbg_sel_value[0] = (original_dbg_sel_value | 0x1 |
		((checksum_dbg_sel << 16) & 0x00ff0000));
	writel(dbg_sel_value[0], (imgsysmainRegBA + 0x4C));
	dbg_sel_value[0] = (unsigned int)ioread32((void *)(imgsysmainRegBA + 0x4C));
	debug0_value[0] = (unsigned int)ioread32((void *)(imgsysmainRegBA + 0x200));
	debug0_checksum[0] = (debug0_value[0] & 0x0000ffff);
	debug0_rdy[0] = (debug0_value[0] & 0x00800000) >> 23;
	debug0_req[0] = (debug0_value[0] & 0x01000000) >> 24;
	debug1_value[0] = (unsigned int)ioread32((void *)(imgsysmainRegBA + 0x204));
	debug1_line_cnt[0] = ((debug1_value[0] & 0xffff0000) >> 16) & 0x0000ffff;
	debug1_pix_cnt[0] = (debug1_value[0] & 0x0000ffff);
	debug2_value[0] = (unsigned int)ioread32((void *)(imgsysmainRegBA + 0x208));
	debug2_line_cnt[0] = ((debug2_value[0] & 0xffff0000) >> 16) & 0x0000ffff;
	debug2_pix_cnt[0] = (debug2_value[0] & 0x0000ffff);

	/*dump later engine in DL (imgsys main out port) status */
	checksum_dbg_sel = (unsigned int)((dl_path << 1) | (1 << 0));
	dbg_sel_value[1] = (original_dbg_sel_value | 0x1 |
		((checksum_dbg_sel << 16) & 0x00ff0000));
	writel(dbg_sel_value[1], (imgsysmainRegBA + 0x4C));
	dbg_sel_value[1] = (unsigned int)ioread32((void *)(imgsysmainRegBA + 0x4C));
	debug0_value[1] = (unsigned int)ioread32((void *)(imgsysmainRegBA + 0x200));
	debug0_checksum[1] = (debug0_value[1] & 0x0000ffff);
	debug0_rdy[1] = (debug0_value[1] & 0x00800000) >> 23;
	debug0_req[1] = (debug0_value[1] & 0x01000000) >> 24;
	debug1_value[1] = (unsigned int)ioread32((void *)(imgsysmainRegBA + 0x204));
	debug1_line_cnt[1] = ((debug1_value[1] & 0xffff0000) >> 16) & 0x0000ffff;
	debug1_pix_cnt[1] = (debug1_value[1] & 0x0000ffff);
	debug2_value[1] = (unsigned int)ioread32((void *)(imgsysmainRegBA + 0x208));
	debug2_line_cnt[1] = ((debug2_value[1] & 0xffff0000) >> 16) & 0x0000ffff;
	debug2_pix_cnt[1] = (debug2_value[1] & 0x0000ffff);

	/* macro_comm status */
	/*if (dl_path == IMGSYS_DL_WPE_PQDIP) {*/
	/*wpedip1RegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_WPE1_DIP1);*/
	if (!wpedip1RegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap wpe_dip1 registers\n",
								__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
				__func__, imgsys_dev->dev->of_node->name);
		return;
	}
	wpe_pqdip_mux_v = (unsigned int)ioread32((void *)(wpedip1RegBA + 0xA8));
	/*iounmap(wpedip1RegBA);*/

	/*wpedip2RegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_WPE2_DIP1);*/
	if (!wpedip2RegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap wpe_dip2 registers\n",
								__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
				__func__, imgsys_dev->dev->of_node->name);
		return;
	}
	wpe_pqdip_mux2_v = (unsigned int)ioread32((void *)(wpedip2RegBA + 0xA8));
	/*iounmap(wpedip2RegBA);*/

	/*wpedip3RegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_WPE3_DIP1);*/
	if (!wpedip3RegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap wpe_dip3 registers\n",
								__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
				__func__, imgsys_dev->dev->of_node->name);
		return;
	}
	wpe_pqdip_mux3_v = (unsigned int)ioread32((void *)(wpedip3RegBA + 0xA8));
	/*iounmap(wpedip3RegBA);*/
	/*}*/

	/* dump information */
	if (dl_path == IMGSYS_DL_WPET_TRAW) {
	} else {
		if (debug0_req[0] == 1) {
			snprintf(logBuf_temp, log_length,
				"%s req to send data to %s/",
				logBuf_inport, logBuf_outport);
		} else {
			snprintf(logBuf_temp, log_length,
				"%s not send data to %s/",
				logBuf_inport, logBuf_outport);
		}
		strncat(logBuf_final, logBuf_temp, strlen(logBuf_temp));
		memset((char *)logBuf_temp, 0x0, log_length);
		logBuf_temp[strlen(logBuf_temp)] = '\0';
		if (debug0_rdy[0] == 1) {
			ret = snprintf(logBuf_temp, log_length,
				"%s rdy to receive data from %s",
				logBuf_outport, logBuf_inport);
			if (ret >= log_length)
				dev_dbg(imgsys_dev->dev, "%s: string truncated\n", __func__);

		} else {
			ret = snprintf(logBuf_temp, log_length,
				"%s not rdy to receive data from %s",
				logBuf_outport, logBuf_inport);
			if (ret >= log_length)
				dev_dbg(imgsys_dev->dev, "%s: string truncated\n", __func__);
		}
		strncat(logBuf_final, logBuf_temp, strlen(logBuf_temp));
		dev_info(imgsys_dev->dev,
			"%s: %s", __func__, logBuf_final);

		memset((char *)logBuf_final, 0x0, log_length * 4);
		logBuf_final[strlen(logBuf_final)] = '\0';
		memset((char *)logBuf_temp, 0x0, log_length);
		logBuf_temp[strlen(logBuf_temp)] = '\0';
		if (debug0_req[1] == 1) {
			ret = snprintf(logBuf_temp, log_length,
				"%s req to send data to %sPIPE/",
				logBuf_outport, logBuf_outport);
			if (ret >= log_length)
				dev_dbg(imgsys_dev->dev, "%s: string truncated\n", __func__);

		} else {
			ret = snprintf(logBuf_temp, log_length,
				"%s not send data to %sPIPE/",
				logBuf_outport, logBuf_outport);
			if (ret >= log_length)
				dev_dbg(imgsys_dev->dev, "%s: string truncated\n", __func__);
		}
		strncat(logBuf_final, logBuf_temp, strlen(logBuf_temp));
		memset((char *)logBuf_temp, 0x0, log_length);
		logBuf_temp[strlen(logBuf_temp)] = '\0';
		if (debug0_rdy[1] == 1) {
			ret = snprintf(logBuf_temp, log_length,
				"%sPIPE rdy to receive data from %s",
				logBuf_outport, logBuf_outport);
			if (ret >= log_length)
				dev_dbg(imgsys_dev->dev, "%s: string truncated\n", __func__);

		} else {
			ret = snprintf(logBuf_temp, log_length,
				"%sPIPE not rdy to receive data from %s",
				logBuf_outport, logBuf_outport);
			if (ret >= log_length)
				dev_dbg(imgsys_dev->dev, "%s: string truncated\n", __func__);
		}
		strncat(logBuf_final, logBuf_temp, strlen(logBuf_temp));
		dev_info(imgsys_dev->dev,
			"%s: %s", __func__, logBuf_final);
		dev_info(imgsys_dev->dev,
			"%s: in_req/in_rdy/out_req/out_rdy = %d/%d/%d/%d,(cheskcum: in/out) = (%d/%d)",
			__func__,
			debug0_req[0], debug0_rdy[0],
			debug0_req[1], debug0_rdy[1],
			debug0_checksum[0], debug0_checksum[1]);
		dev_info(imgsys_dev->dev,
			"%s: info01 in_line/in_pix/out_line/out_pix = %d/%d/%d/%d",
			__func__,
			debug1_line_cnt[0], debug1_pix_cnt[0], debug1_line_cnt[1],
			debug1_pix_cnt[1]);
		dev_info(imgsys_dev->dev,
			"%s: info02 in_line/in_pix/out_line/out_pix = %d/%d/%d/%d",
			__func__,
			debug2_line_cnt[0], debug2_pix_cnt[0], debug2_line_cnt[1],
			debug2_pix_cnt[1]);
	}
	dev_info(imgsys_dev->dev, "%s: ===(%s): %s DBG INFO===",
		__func__, logBuf_path, logBuf_inport);
	dev_info(imgsys_dev->dev, "%s:  0x%08X %08X", __func__,
		(unsigned int)(0x15000000 + 0x4C), dbg_sel_value[0]);
	dev_info(imgsys_dev->dev, "%s:  0x%08X %08X", __func__,
		(unsigned int)(0x15000000 + 0x200), debug0_value[0]);
	dev_info(imgsys_dev->dev, "%s:  0x%08X %08X", __func__,
		(unsigned int)(0x15000000 + 0x204), debug1_value[0]);
	dev_info(imgsys_dev->dev, "%s:  0x%08X %08X", __func__,
		(unsigned int)(0x15000000 + 0x208), debug2_value[0]);

	dev_info(imgsys_dev->dev, "%s: ===(%s): %s DBG INFO===",
		__func__, logBuf_path, logBuf_outport);
	dev_info(imgsys_dev->dev, "%s:  0x%08X %08X", __func__,
		(unsigned int)(0x15000000 + 0x4C), dbg_sel_value[1]);
	dev_info(imgsys_dev->dev, "%s:  0x%08X %08X", __func__,
		(unsigned int)(0x15000000 + 0x200), debug0_value[1]);
	dev_info(imgsys_dev->dev, "%s:  0x%08X %08X", __func__,
		(unsigned int)(0x15000000 + 0x204), debug1_value[1]);
	dev_info(imgsys_dev->dev, "%s:  0x%08X %08X", __func__,
		(unsigned int)(0x15000000 + 0x208), debug2_value[1]);

	dev_info(imgsys_dev->dev, "%s: ===(%s): IMGMAIN CG INFO===",
		__func__, logBuf_path);
	dev_info(imgsys_dev->dev, "%s: CG_CON  0x%08X %08X", __func__,
		(unsigned int)(0x15000000 + 0x0),
		(unsigned int)ioread32((void *)(imgsysmainRegBA + 0x0)));
	dev_info(imgsys_dev->dev, "%s: CG_SET  0x%08X %08X", __func__,
		(unsigned int)(0x15000000 + 0x4),
		(unsigned int)ioread32((void *)(imgsysmainRegBA + 0x4)));
	dev_info(imgsys_dev->dev, "%s: CG_CLR  0x%08X %08X", __func__,
		(unsigned int)(0x15000000 + 0x8),
		(unsigned int)ioread32((void *)(imgsysmainRegBA + 0x8)));

	/*if (dl_path == IMGSYS_DL_WPE_PQDIP) {*/
	dev_info(imgsys_dev->dev, "%s:  0x%08X %08X", __func__,
		(unsigned int)(0x15220000 + 0xA8), wpe_pqdip_mux_v);
	dev_info(imgsys_dev->dev, "%s:  0x%08X %08X", __func__,
		(unsigned int)(0x15520000 + 0xA8), wpe_pqdip_mux2_v);
	dev_info(imgsys_dev->dev, "%s:  0x%08X %08X", __func__,
		(unsigned int)(0x15620000 + 0xA8), wpe_pqdip_mux3_v);
	/*}*/
	/*iounmap(imgsysmainRegBA);*/
}

void imgsys_dl_debug_dump(struct mtk_imgsys_dev *imgsys_dev, unsigned int hw_comb)
{
	int dl_path = 0;
	char logBuf_path[log_length];
	char logBuf_inport[log_length];
	char logBuf_outport[log_length];
	char logBuf_eng[log_length];
	int i = 0, get = false;

	memset((char *)logBuf_path, 0x0, log_length);
	logBuf_path[strlen(logBuf_path)] = '\0';
	memset((char *)logBuf_inport, 0x0, log_length);
	logBuf_inport[strlen(logBuf_inport)] = '\0';
	memset((char *)logBuf_outport, 0x0, log_length);
	logBuf_outport[strlen(logBuf_outport)] = '\0';

	for (i = 0 ; i < DL_CHECK_ENG_NUM ; i++) {
		memset((char *)logBuf_eng, 0x0, log_length);
		logBuf_eng[strlen(logBuf_eng)] = '\0';
		if (hw_comb & dbg_engine_name_list[i].eng_e) {
			if (get) {
				snprintf(logBuf_eng, log_length, "-%s",
					dbg_engine_name_list[i].eng_name);
			} else {
				snprintf(logBuf_eng, log_length, "%s",
					dbg_engine_name_list[i].eng_name);
			}
			get = true;
		}
		strncat(logBuf_path, logBuf_eng, strlen(logBuf_eng));
	}
	memset((char *)logBuf_eng, 0x0, log_length);
	logBuf_eng[strlen(logBuf_eng)] = '\0';
	snprintf(logBuf_eng, log_length, "%s", " FAIL");
	strncat(logBuf_path, logBuf_eng, strlen(logBuf_eng));

	dev_info(imgsys_dev->dev, "%s: %s\n",
			__func__, logBuf_path);
	switch (hw_comb) {
	/*DL checksum case*/
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_TRAW):
		dl_path = IMGSYS_DL_WPEE_TRAW;
		snprintf(logBuf_inport, log_length, "%s",
			"WPE_EIS");
		snprintf(logBuf_outport, log_length, "%s",
			"TRAW");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_LTR):
		dl_path = IMGSYS_DL_WPEE_TRAW;
		snprintf(logBuf_inport, log_length, "%s",
			"WPE_EIS");
		snprintf(logBuf_outport, log_length, "%s",
			"LTRAW");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_XTR):
		dl_path = IMGSYS_DL_WPEE_XTRAW;
		snprintf(logBuf_inport, log_length, "%s",
			"WPE_EIS");
		snprintf(logBuf_outport, log_length, "%s",
			"XTRAW");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_TRAW):
		dl_path = IMGSYS_DL_WPET_TRAW;
		snprintf(logBuf_inport, log_length, "%s",
			"WPE_TNR");
		snprintf(logBuf_outport, log_length, "%s",
			"TRAW");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_LTR):
		dl_path = IMGSYS_DL_WPET_TRAW;
		snprintf(logBuf_inport, log_length, "%s",
			"WPE_TNR");
		snprintf(logBuf_outport, log_length, "%s",
			"LTRAW");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_XTR):
		dl_path = IMGSYS_DL_WPET_XTRAW;
		snprintf(logBuf_inport, log_length, "%s",
			"WPE_TNR");
		snprintf(logBuf_outport, log_length, "%s",
			"XTRAW");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_ENG_WPE_LITE | IMGSYS_ENG_TRAW):
		dl_path = IMGSYS_DL_WPET_TRAW;
		snprintf(logBuf_inport, log_length, "%s",
			"WPE_LITE");
		snprintf(logBuf_outport, log_length, "%s",
			"TRAW");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		dev_info(imgsys_dev->dev,
			"%s: we dont have checksum for WPELITE DL TRAW\n",
			__func__);
		break;
	case (IMGSYS_ENG_WPE_LITE | IMGSYS_ENG_LTR):
		dl_path = IMGSYS_DL_WPET_TRAW;
		snprintf(logBuf_inport, log_length, "%s",
			"WPE_LITE");
		snprintf(logBuf_outport, log_length, "%s",
			"LTRAW");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		dev_info(imgsys_dev->dev,
			"%s: we dont have checksum for WPELITE DL LTRAW\n",
			__func__);
		break;
	case (IMGSYS_ENG_WPE_LITE | IMGSYS_ENG_XTR):
		dl_path = IMGSYS_DL_WPET_TRAW;
		snprintf(logBuf_inport, log_length, "%s",
			"WPE_LITE");
		snprintf(logBuf_outport, log_length, "%s",
			"XTRAW");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		dev_info(imgsys_dev->dev,
			"%s: we dont have checksum for WPELITE DL XTRAW\n",
			__func__);
		break;
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_DIP):
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_A):
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_B):
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_A |
		IMGSYS_ENG_PQDIP_B):
		dl_path = IMGSYS_DL_WPEE_DIP;
		snprintf(logBuf_inport, log_length, "%s",
			"WPE_EIS");
		snprintf(logBuf_outport, log_length, "%s",
			 "DIP");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, log_length);
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, log_length);
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_PQDIPA;
		snprintf(logBuf_inport, log_length, "%s",
			"DIP");
		snprintf(logBuf_outport, log_length, "%s",
			"PQDIPA");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, log_length);
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, log_length);
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_PQDIPB;
		snprintf(logBuf_inport, log_length, "%s",
			"DIP");
		snprintf(logBuf_outport, log_length, "%s",
			"PQDIPB");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_DIP):
	case (IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_A):
	case (IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_B):
	case (IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_A |
		IMGSYS_ENG_PQDIP_B):
		dl_path = IMGSYS_DL_WPET_DIP;
		snprintf(logBuf_inport, log_length, "%s",
			"WPE_TNR");
		snprintf(logBuf_outport, log_length, "%s",
			"DIP");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, log_length);
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, log_length);
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_PQDIPA;
		snprintf(logBuf_inport, log_length, "%s",
			"DIP");
		snprintf(logBuf_outport, log_length, "%s",
			"PQDIPA");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, log_length);
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, log_length);
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_PQDIPB;
		snprintf(logBuf_inport, log_length, "%s",
			"DIP");
		snprintf(logBuf_outport, log_length, "%s",
			"PQDIPB");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_PQDIP_A):
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_PQDIP_B):
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_PQDIP_A |
		IMGSYS_ENG_PQDIP_B):
		dl_path = IMGSYS_DL_WPE_PQDIP;
		snprintf(logBuf_inport, log_length, "%s",
			"WPE_EIS");
		snprintf(logBuf_outport, log_length, "%s",
			"PQDIP");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_PQDIP_A):
	case (IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_PQDIP_B):
	case (IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_PQDIP_A |
		IMGSYS_ENG_PQDIP_B):
		dl_path = IMGSYS_DL_WPE_PQDIP;
		snprintf(logBuf_inport, log_length, "%s",
			"WPE_TNR");
		snprintf(logBuf_outport, log_length, "%s",
			"PQDIP");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP):
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP |
		IMGSYS_ENG_PQDIP_A):
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP |
		IMGSYS_ENG_PQDIP_B):
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP |
		IMGSYS_ENG_PQDIP_A | IMGSYS_ENG_PQDIP_B):
		dl_path = IMGSYS_DL_WPEE_TRAW;
		snprintf(logBuf_inport, log_length, "%s",
			"WPE_EIS");
		snprintf(logBuf_outport, log_length, "%s",
			"TRAW");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, log_length);
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, log_length);
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_TRAW_DIP;
		snprintf(logBuf_inport, log_length, "%s",
			"TRAW");
		snprintf(logBuf_outport, log_length, "%s",
			"DIP");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, log_length);
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, log_length);
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_PQDIPA;
		snprintf(logBuf_inport, log_length, "%s",
			"DIP");
		snprintf(logBuf_outport, log_length, "%s",
			"PQDIPA");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, log_length);
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, log_length);
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_PQDIPB;
		snprintf(logBuf_inport, log_length, "%s",
			"DIP");
		snprintf(logBuf_outport, log_length, "%s",
			"PQDIPB");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP):
	case (IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP |
		IMGSYS_ENG_PQDIP_A):
	case (IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP |
		IMGSYS_ENG_PQDIP_B):
	case (IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP |
		IMGSYS_ENG_PQDIP_A | IMGSYS_ENG_PQDIP_B):
		dl_path = IMGSYS_DL_TRAW_DIP;
		snprintf(logBuf_inport, log_length, "%s",
			"TRAW");
		snprintf(logBuf_outport, log_length, "%s",
			"DIP");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, log_length);
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, log_length);
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_WPET_DIP;
		snprintf(logBuf_inport, log_length, "%s",
			"WPE_TNR");
		snprintf(logBuf_outport, log_length, "%s",
			"DIP");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, log_length);
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, log_length);
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_PQDIPA;
		snprintf(logBuf_inport, log_length, "%s",
			"DIP");
		snprintf(logBuf_outport, log_length, "%s",
			"PQDIPA");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, log_length);
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, log_length);
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_PQDIPB;
		snprintf(logBuf_inport, log_length, "%s",
			"DIP");
		snprintf(logBuf_outport, log_length, "%s",
			"PQDIPB");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_ENG_WPE_LITE | IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP):
	case (IMGSYS_ENG_WPE_LITE | IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP |
		IMGSYS_ENG_PQDIP_A):
	case (IMGSYS_ENG_WPE_LITE | IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP |
		IMGSYS_ENG_PQDIP_B):
	case (IMGSYS_ENG_WPE_LITE | IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP |
		IMGSYS_ENG_PQDIP_A | IMGSYS_ENG_PQDIP_B):
		dl_path = IMGSYS_DL_WPEL_TRAW;
		snprintf(logBuf_inport, log_length, "%s",
			"WPE_LITE");
		snprintf(logBuf_outport, log_length, "%s",
			"TRAW");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		dev_info(imgsys_dev->dev,
			"%s: we dont have checksum for WPELITE DL TRAW\n",
			__func__);
		/**/
		memset((char *)logBuf_inport, 0x0, log_length);
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, log_length);
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_TRAW_DIP;
		snprintf(logBuf_inport, log_length, "%s",
			"TRAW");
		snprintf(logBuf_outport, log_length, "%s",
			"DIP");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, log_length);
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, log_length);
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_PQDIPA;
		snprintf(logBuf_inport, log_length, "%s",
			"DIP");
		snprintf(logBuf_outport, log_length, "%s",
			"PQDIPA");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, log_length);
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, log_length);
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_PQDIPB;
		snprintf(logBuf_inport, log_length, "%s",
			"DIP");
		snprintf(logBuf_outport, log_length, "%s",
			"PQDIPB");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_DIP):
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_DIP |
		IMGSYS_ENG_PQDIP_A):
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_DIP |
		IMGSYS_ENG_PQDIP_B):
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_DIP |
		IMGSYS_ENG_PQDIP_A | IMGSYS_ENG_PQDIP_B):
		dl_path = IMGSYS_DL_WPEE_DIP;
		snprintf(logBuf_inport, log_length, "%s",
			"WPE_EIS");
		snprintf(logBuf_outport, log_length, "%s",
			"DIP");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, log_length);
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, log_length);
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_WPET_DIP;
		snprintf(logBuf_inport, log_length, "%s",
			"WPE_TNR");
		snprintf(logBuf_outport, log_length, "%s",
			"DIP");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, log_length);
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, log_length);
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_PQDIPA;
		snprintf(logBuf_inport, log_length, "%s",
			"DIP");
		snprintf(logBuf_outport, log_length, "%s",
			"PQDIPA");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, log_length);
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, log_length);
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_PQDIPB;
		snprintf(logBuf_inport, log_length, "%s",
			"DIP");
		snprintf(logBuf_outport, log_length, "%s",
			"PQDIPB");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_TRAW |
		IMGSYS_ENG_DIP):
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_TRAW |
		IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_A):
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_TRAW |
		IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_B):
	case (IMGSYS_ENG_WPE_EIS | IMGSYS_ENG_WPE_TNR | IMGSYS_ENG_TRAW |
		IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_A | IMGSYS_ENG_PQDIP_B):
		dev_info(imgsys_dev->dev,
			"%s: TOBE CHECKED SELECTION BASED ON FMT..\n",
			__func__);
		break;
	case (IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP):
	case (IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_A):
	case (IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_B):
	case (IMGSYS_ENG_TRAW | IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_A |
		IMGSYS_ENG_PQDIP_B):
		dl_path = IMGSYS_DL_TRAW_DIP;
		snprintf(logBuf_inport, log_length, "%s",
			"TRAW");
		snprintf(logBuf_outport, log_length, "%s",
			"DIP");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, log_length);
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, log_length);
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_PQDIPA;
		snprintf(logBuf_inport, log_length, "%s",
			"DIP");
		snprintf(logBuf_outport, log_length, "%s",
			"PQDIPA");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, log_length);
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, log_length);
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_PQDIPB;
		snprintf(logBuf_inport, log_length, "%s",
			"DIP");
		snprintf(logBuf_outport, log_length, "%s",
			"PQDIPB");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_A):
	case (IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_B):
	case (IMGSYS_ENG_DIP | IMGSYS_ENG_PQDIP_A | IMGSYS_ENG_PQDIP_B):
		dl_path = IMGSYS_DL_DIP_PQDIPA;
		snprintf(logBuf_inport, log_length, "%s",
			"DIP");
		snprintf(logBuf_outport, log_length, "%s",
			"PQDIPA");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		/**/
		memset((char *)logBuf_inport, 0x0, log_length);
		logBuf_inport[strlen(logBuf_inport)] = '\0';
		memset((char *)logBuf_outport, 0x0, log_length);
		logBuf_outport[strlen(logBuf_outport)] = '\0';
		dl_path = IMGSYS_DL_DIP_PQDIPB;
		snprintf(logBuf_inport, log_length, "%s",
			"DIP");
		snprintf(logBuf_outport, log_length, "%s",
			"PQDIPB");
		imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
			logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		break;
	case (IMGSYS_ENG_ADL_A | IMGSYS_ENG_XTR):
	case (IMGSYS_ENG_ADL_A | IMGSYS_ENG_ADL_B | IMGSYS_ENG_XTR):
		/**
		 * dl_path = IMGSYS_DL_ADLA_XTRAW;
		 * snprintf(logBuf_inport, log_length, "%s", "ADL");
		 * snprintf(logBuf_outport, log_length, "%s", "XTRAW");
		 * imgsys_dl_checksum_dump(imgsys_dev, hw_comb,
		 *  logBuf_path, logBuf_inport, logBuf_outport, dl_path);
		 */
		dev_info(imgsys_dev->dev,
			"%s: we dont have checksum for ADL DL XTRAW\n",
			__func__);
		break;
	case (IMGSYS_ENG_ME):
		imgsys_cg_debug_dump(imgsys_dev);
		break;
	default:
		break;
	}

	dev_info(imgsys_dev->dev, "%s: -\n", __func__);
}
