/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/of_address.h>
#include <linux/of.h>
#include <linux/delay.h>


#include "ddp_misc.h"
#include "ddp_reg.h"
#include "ddp_log.h"
#include "ddp_rdma_ex.h"
#include "ddp_wdma_ex.h"
#include "ddp_ovl.h"
#include "m4u.h"
#include "ddp_info.h"
#include "mtk_disp_mgr.h"
#include "disp_helper.h"
#include "mmpath.h"

unsigned long get_smi_larb_va(unsigned int larb)
{
	struct device_node *node = NULL;
	char smi_larb_dt_name[25];
	unsigned long va = 0;

	snprintf(smi_larb_dt_name, sizeof(smi_larb_dt_name),
		"mediatek,smi_larb%u", larb);
	node = of_find_compatible_node(NULL, NULL, smi_larb_dt_name);

	if (node != NULL)
		va = (unsigned long)of_iomap(node, 0);

	if (va == 0)
		DDP_PR_ERR("Cannot get smi larb va, smi DT name: %s\n",
			   smi_larb_dt_name);

	return va;
}

void enable_smi_ultra(unsigned int larb, unsigned int value)
{
	unsigned long va = get_smi_larb_va(larb);
	unsigned long offset = SMI_LARB_FORCE_ULTRA;
	unsigned int prev_value, cur_value;

	if (va == 0)
		return;

	prev_value = DISP_REG_GET(va + offset);

	DISP_REG_SET(NULL, va + offset, value);

	cur_value = DISP_REG_GET(va + offset);

	DDPMSG("enable smi ultra, larb:%u, prev:0x%x, cur:0x%x\n",
		larb, prev_value, cur_value);

}

void enable_smi_preultra(unsigned int larb, unsigned int value)
{
	unsigned long va = get_smi_larb_va(larb);
	unsigned long offset = SMI_LARB_FORCE_PREULTRA;
	unsigned int prev_value, cur_value;

	if (va == 0)
		return;

	prev_value = DISP_REG_GET(va + offset);

	DISP_REG_SET(NULL, va + offset, value);

	cur_value = DISP_REG_GET(va + offset);

	DDPMSG("enable smi pre-ultra, larb:%u, prev:0x%x, cur:0x%x\n",
		larb, prev_value, cur_value);
}


void disable_smi_ultra(unsigned int larb, unsigned int value)
{
	unsigned long va = get_smi_larb_va(larb);
	unsigned long offset = SMI_LARB_ULTRA_DIS;
	unsigned int prev_value, cur_value;

	if (va == 0)
		return;

	prev_value = DISP_REG_GET(va + offset);

	DISP_REG_SET(NULL, va + offset, value);

	cur_value = DISP_REG_GET(va + offset);

	DDPMSG("disable smi ultra, larb:%u, prev:0x%x, cur:0x%x\n",
		larb, prev_value, cur_value);
}

void disable_smi_preultra(unsigned int larb, unsigned int value)
{
	unsigned long va = get_smi_larb_va(larb);
	unsigned long offset = SMI_LARB_PREULTRA_DIS;
	unsigned int prev_value, cur_value;

	if (va == 0)
		return;

	prev_value = DISP_REG_GET(va + offset);

	DISP_REG_SET(NULL, va + offset, value);

	cur_value = DISP_REG_GET(va + offset);

	DDPMSG("disable smi pre-ultra, larb:%u, prev:0x%x, cur:0x%x\n",
		larb, prev_value, cur_value);
}

void rdma_golden_setting_test(unsigned int idx, unsigned int bpp,
	unsigned int w, unsigned int h, unsigned int is_wrot_sram,
	bool is_dc, unsigned int is_vdo)
{
	struct golden_setting_context gsc;
	unsigned int gs[GS_RDMA_FLD_NUM] = {-1};
	unsigned int i;

	gsc.dst_width = w;
	gsc.dst_height = h;
	gsc.is_dc = is_dc;
	gsc.is_wrot_sram = is_wrot_sram;
	rdma_cal_golden_setting(idx, bpp, &gsc, gs, is_vdo);

	DDPMSG("== RDMA_GOLDEN_SETTING_TEST ==\n");
	DDPMSG("Input: idx:%u, bpp:%u, w:%u, h:%u, is_dc:%d, is_vdo:%d\n",
		idx, bpp, w, h, is_dc, is_vdo);
	DDPMSG("Result:\n");
	for (i = 0; i < GS_RDMA_FLD_NUM; i++)
		DDPMSG("%u\n", gs[i]);
}


void ovl_golden_setting_test(enum dst_module_type dst_mode_type)
{
	unsigned int gs[GS_OVL_FLD_NUM] = {-1};
	unsigned int i;

	ovl_cal_golden_setting(dst_mode_type, gs);

	DDPMSG("== OVL_GOLDEN_SETTING_TEST ==\n");
	DDPMSG("dst_module_type%u\n", dst_mode_type);
	DDPMSG("Result:\n");
	for (i = 0; i < GS_OVL_FLD_NUM; i++)
		DDPMSG("%u\n", gs[i]);
}

void wdma_golden_setting_test(unsigned int w, unsigned int h,
	unsigned int is_dc, enum UNIFIED_COLOR_FMT format)
{
	struct golden_setting_context gsc;
	unsigned int i;
	unsigned int gs[GS_WDMA_FLD_NUM] = {-1};

	gsc.dst_width = w;
	gsc.dst_height = h;
	gsc.is_dc = is_dc;

	wdma_calc_golden_setting(&gsc, 1, gs, format);

	DDPMSG("== WDMA_GOLDEN_SETTING_TEST ==\n");
	DDPMSG("width%u, height:%u, is_dc:%u\n", w, h, is_dc);
	DDPMSG("Result:\n");
	for (i = 0; i < GS_WDMA_FLD_NUM; i++)
		DDPMSG("%u\n", gs[i]);
}

void golden_setting_test(void)
{
	/* RDMA golden setting */
	rdma_golden_setting_test(0, 24, 1080, 1920, 0, 0, 0);
	rdma_golden_setting_test(0, 24, 1080, 1920, 0, 1, 0);
	rdma_golden_setting_test(0, 24, 1080, 1920, 0, 0, 1);
	rdma_golden_setting_test(0, 24, 1080, 1920, 0, 1, 1);
	rdma_golden_setting_test(0, 24, 1080, 2520, 0, 0, 0);
	rdma_golden_setting_test(0, 24, 1080, 2520, 0, 1, 0);
	rdma_golden_setting_test(0, 24, 1080, 2520, 0, 0, 1);
	rdma_golden_setting_test(0, 24, 1080, 2520, 0, 1, 1);

	/* OVL golden setting */
	ovl_golden_setting_test(DST_MOD_REAL_TIME);
	ovl_golden_setting_test(DST_MOD_WDMA);

	/* WDMA golden setting */
	wdma_golden_setting_test(1080, 1920, 0, UFMT_RGBA8888);
	wdma_golden_setting_test(1080, 1920, 1, UFMT_I420);
	wdma_golden_setting_test(1080, 2520, 0, UFMT_NV12);
	wdma_golden_setting_test(1080, 2520, 1, UFMT_YV12);
}

void fake_engine(unsigned int idx, unsigned int en,
			unsigned int wr_en, unsigned int rd_en,
			unsigned int latency, unsigned int preultra_cnt,
			unsigned int ultra_cnt)
{
	int offset = idx * 0x20;
	struct M4U_PORT_STRUCT port;
	static void *va[2];
	static phys_addr_t pa[2];

	int wr_pat = 4;
	int burst = 7;
	int test_len = 255;
	int loop = 1;
	int preultra_en = 0;
	int ultra_en = 0;
	int dis_wr = !wr_en;
	int dis_rd = !rd_en;
	int delay_cnt = 0;

	if (idx > 1) {
		DDP_PR_ERR("%s idx only 0 and 1\n", __func__);
		return;
	}

	if (preultra_cnt > 0) {
		preultra_en = 1;
		preultra_cnt--;
	}

	if (ultra_cnt > 0) {
		ultra_en = 1;
		ultra_cnt--;
	}

	if (!va[idx]) {
		va[idx] = kzalloc(1024*1024, GFP_KERNEL | GFP_DMA);
		pa[idx] = virt_to_phys(va[idx]);
		DDPMSG("%s fake_engine_%d va=%p, pa=0x%pa\n",
			__func__, idx, va[idx], &pa[idx]);
	}

	if (en) {

		memset(&port, 0, sizeof(port));
		if (idx)
			port.ePortID = M4U_PORT_DISP_FAKE1;
		else
			port.ePortID = M4U_PORT_DISP_FAKE0;
		port.Security = 0;
		port.Virtuality = 0;
		m4u_config_port(&port);

		if (idx)
			DISP_REG_SET(NULL,
				DISP_REG_CONFIG_MMSYS_CG_CLR1, BIT(4));
		else
			DISP_REG_SET(NULL,
				DISP_REG_CONFIG_MMSYS_CG_CLR0, BIT(19));

		DISP_REG_SET(NULL,
			DISP_REG_CONFIG_DISP_FAKE_ENG_RD_ADDR + offset,
			(unsigned int)pa[idx]);
		DISP_REG_SET(NULL,
			DISP_REG_CONFIG_DISP_FAKE_ENG_WR_ADDR + offset,
			(unsigned int)pa[idx] + 4096);
		DISP_REG_SET(NULL,
			DISP_REG_CONFIG_DISP_FAKE_ENG_CON0 + offset,
			(wr_pat << 24) | (loop << 22) | test_len);
		DISP_REG_SET(NULL,
			DISP_REG_CONFIG_DISP_FAKE_ENG_CON1 + offset,
			(ultra_en << 23) | (ultra_cnt << 20) |
			(preultra_en << 19) | (preultra_cnt << 16) |
			(burst << 12) | (dis_wr << 11) | (dis_rd << 10) |
			latency);
		DISP_REG_SET(NULL,
			DISP_REG_CONFIG_DISP_FAKE_ENG_RST + offset, 1);
		DISP_REG_SET(NULL,
			DISP_REG_CONFIG_DISP_FAKE_ENG_RST + offset, 0);
		DISP_REG_SET(NULL,
			DISP_REG_CONFIG_DISP_FAKE_ENG_EN + offset, 0x3);

		DDPMSG("fake_engine_%d enable\n", idx);
	} else {
		DISP_REG_SET(NULL,
			DISP_REG_CONFIG_DISP_FAKE_ENG_EN + offset, 0x1);

		while ((DISP_REG_GET(DISP_REG_CONFIG_DISP_FAKE_ENG_STATE
					+ offset) & 0x1) == 0x1) {
			delay_cnt++;
			udelay(1);
			if (delay_cnt > 1000) {
				DDP_PR_ERR("Wait fake_engine_%d idle timeout\n",
					idx);
				break;
			}
		}

		DISP_REG_SET(NULL,
			DISP_REG_CONFIG_DISP_FAKE_ENG_EN + offset, 0x0);

		if (idx)
			DISP_REG_SET(NULL,
				DISP_REG_CONFIG_MMSYS_CG_SET1, BIT(4));
		else
			DISP_REG_SET(NULL,
				DISP_REG_CONFIG_MMSYS_CG_SET0, BIT(19));

		DDPMSG("fake_engine_%d disable\n", idx);
	}
}

void dump_fake_engine(void)
{
	DDPMSG("=================Dump Fake_engine================\n");
	DDPMSG("CG_CON0 = 0x%x\n",
		DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON0));
	DDPMSG("CG_CON1 = 0x%x\n",
		DISP_REG_GET(DISP_REG_CONFIG_MMSYS_CG_CON1));

	DDPMSG("FAKE_ENG_RD_ADDR = 0x%x\n",
		DISP_REG_GET(DISP_REG_CONFIG_DISP_FAKE_ENG_RD_ADDR));
	DDPMSG("FAKE_ENG_WR_ADDR = 0x%x\n",
		DISP_REG_GET(DISP_REG_CONFIG_DISP_FAKE_ENG_WR_ADDR));
	DDPMSG("FAKE_ENG_CON0 = 0x%x\n",
		DISP_REG_GET(DISP_REG_CONFIG_DISP_FAKE_ENG_CON0));
	DDPMSG("FAKE_ENG_CON1 = 0x%x\n",
		DISP_REG_GET(DISP_REG_CONFIG_DISP_FAKE_ENG_CON1));
	DDPMSG("FAKE_ENG_EN = 0x%x\n",
		DISP_REG_GET(DISP_REG_CONFIG_DISP_FAKE_ENG_EN));
	DDPMSG("FAKE_ENG_STATE = 0x%x\n",
		DISP_REG_GET(DISP_REG_CONFIG_DISP_FAKE_ENG_STATE));

	DDPMSG("FAKE2_ENG_RD_ADDR = 0x%x\n",
		DISP_REG_GET(DISP_REG_CONFIG_DISP_FAKE2_ENG_RD_ADDR));
	DDPMSG("FAKE2_ENG_WR_ADDR = 0x%x\n",
		DISP_REG_GET(DISP_REG_CONFIG_DISP_FAKE2_ENG_WR_ADDR));
	DDPMSG("FAKE2_ENG_CON0 = 0x%x\n",
		DISP_REG_GET(DISP_REG_CONFIG_DISP_FAKE2_ENG_CON0));
	DDPMSG("FAKE2_ENG_CON1 = 0x%x\n",
		DISP_REG_GET(DISP_REG_CONFIG_DISP_FAKE2_ENG_CON1));
	DDPMSG("FAKE2_ENG_EN = 0x%x\n",
		DISP_REG_GET(DISP_REG_CONFIG_DISP_FAKE2_ENG_EN));
	DDPMSG("FAKE2_ENG_STATE = 0x%x\n",
		DISP_REG_GET(DISP_REG_CONFIG_DISP_FAKE2_ENG_STATE));
	DDPMSG("====================================================\n");

}

void MMPathTracePrimaryOvl2Dsi(void)
{
#ifndef CONFIG_FPGA_EARLY_PORTING

	char str[1300] = "";
	int strlen = sizeof(str), n = 0;
	int HWC_gpid = get_HWC_gpid();

	if (disp_helper_get_option(DISP_OPT_MMPATH) == 0)
		return;

	if (MMPathIsPrimaryDL()) {
		n += scnprintf(str + n, strlen - n, "hw=DISP_OVL0, pid=%d, ",
			HWC_gpid);
		n = MMPathTracePrimaryOVL(str, strlen, n);
	} else {
		n += scnprintf(str + n, strlen - n, "hw=DISP_RDMA0, pid=%d, ",
			HWC_gpid);
		n = MMPathTracePrimaryRDMA(str, strlen, n);
	}

	n += scnprintf(str + n, strlen - n, "OUT=DSI");

	trace_MMPath(str);
#endif
}

void MMPathTracePrimaryOvl2Mem(void)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	char str[1300] = "";
	int strlen = sizeof(str), n = 0;
	int HWC_gpid = get_HWC_gpid();
	unsigned int wdma_sel_in = 0;

	if (disp_helper_get_option(DISP_OPT_MMPATH) == 0)
		return;

	/* do not print if path is 1-to-2 */
	if (MMPathIsPrimaryDL())
		return;

	wdma_sel_in = DISP_REG_GET(DISP_REG_CONFIG_OVL_TO_WDMA_SEL_IN);

	/* check if ovl0 to wdma */
	if (wdma_sel_in != 0)
		return;

	n += scnprintf(str + n, strlen - n, "hw=DISP_OVL0, pid=%d, ",
		HWC_gpid);

	n = MMPathTracePrimaryOVL(str, strlen, n);

	n = MMPathTracePrimaryWDMA(str, strlen, n);

	trace_MMPath(str);
#endif
}

void MMPathTraceSecondOvl2Mem(void)
{
#ifndef CONFIG_FPGA_EARLY_PORTING

	char str[1300] = "";
	int strlen = sizeof(str), n = 0;
	int HWC_gpid = get_HWC_gpid();
	unsigned int wdma_sel_in = 0;

	if (disp_helper_get_option(DISP_OPT_MMPATH) == 0)
		return;

	wdma_sel_in = DISP_REG_GET(DISP_REG_CONFIG_OVL_TO_WDMA_SEL_IN);

	/* check if ovl1_2l to wdma */
	if (wdma_sel_in != 2)
		return;

	n += scnprintf(str + n, strlen - n, "hw=DISP_OVL1, pid=%d, ",
		HWC_gpid);

	n = MMPathTraceSecondOVL(str, strlen, n);

	n = MMPathTracePrimaryWDMA(str, strlen, n);

	trace_MMPath(str);
#endif
}
