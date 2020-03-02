/*
 * Copyright (C) 2019 MediaTek Inc.
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

#include <linux/clk.h>
#include <linux/delay.h>

#include "apu_power_api.h"
#include "power_clock.h"
#include "apu_log.h"


/************** IMPORTANT !! *******************
 * The following name of each clock struct
 * MUST mapping to clock-names @ mt6885.dts
 **********************************************/

/* for dvfs clock source */
static struct clk *clk_top_dsp_sel;	/* CONN */
static struct clk *clk_top_dsp1_sel;	/* VPU_CORE0 */
static struct clk *clk_top_dsp2_sel;	/* VPU_CORE1 */
static struct clk *clk_top_dsp3_sel;	/* VPU_CORE2 */
static struct clk *clk_top_dsp6_sel;	/* MDLA0 & MDLA1 */
static struct clk *clk_top_dsp7_sel;	/* IOMMU */
static struct clk *clk_top_ipu_if_sel;	/* VCORE */

/* for vpu core 0 */
static struct clk *clk_apu_core0_jtag_cg;
static struct clk *clk_apu_core0_axi_m_cg;
static struct clk *clk_apu_core0_apu_cg;

/* for vpu core 1 */
static struct clk *clk_apu_core1_jtag_cg;
static struct clk *clk_apu_core1_axi_m_cg;
static struct clk *clk_apu_core1_apu_cg;

/* for vpu core 2 */
static struct clk *clk_apu_core2_jtag_cg;
static struct clk *clk_apu_core2_axi_m_cg;
static struct clk *clk_apu_core2_apu_cg;

/* for mdla core 0 */
static struct clk *clk_apu_mdla0_cg_b0;
static struct clk *clk_apu_mdla0_cg_b1;
static struct clk *clk_apu_mdla0_cg_b2;
static struct clk *clk_apu_mdla0_cg_b3;
static struct clk *clk_apu_mdla0_cg_b4;
static struct clk *clk_apu_mdla0_cg_b5;
static struct clk *clk_apu_mdla0_cg_b6;
static struct clk *clk_apu_mdla0_cg_b7;
static struct clk *clk_apu_mdla0_cg_b8;
static struct clk *clk_apu_mdla0_cg_b9;
static struct clk *clk_apu_mdla0_cg_b10;
static struct clk *clk_apu_mdla0_cg_b11;
static struct clk *clk_apu_mdla0_cg_b12;
static struct clk *clk_apu_mdla0_apb_cg;

/* for mdla core 1 */
static struct clk *clk_apu_mdla1_cg_b0;
static struct clk *clk_apu_mdla1_cg_b1;
static struct clk *clk_apu_mdla1_cg_b2;
static struct clk *clk_apu_mdla1_cg_b3;
static struct clk *clk_apu_mdla1_cg_b4;
static struct clk *clk_apu_mdla1_cg_b5;
static struct clk *clk_apu_mdla1_cg_b6;
static struct clk *clk_apu_mdla1_cg_b7;
static struct clk *clk_apu_mdla1_cg_b8;
static struct clk *clk_apu_mdla1_cg_b9;
static struct clk *clk_apu_mdla1_cg_b10;
static struct clk *clk_apu_mdla1_cg_b11;
static struct clk *clk_apu_mdla1_cg_b12;
static struct clk *clk_apu_mdla1_apb_cg;

/* for apu conn */
static struct clk *clk_apu_conn_ahb_cg;
static struct clk *clk_apu_conn_axi_cg;
static struct clk *clk_apu_conn_isp_cg;
static struct clk *clk_apu_conn_cam_adl_cg;
static struct clk *clk_apu_conn_img_adl_cg;
static struct clk *clk_apu_conn_emi_26m_cg;
static struct clk *clk_apu_conn_vpu_udi_cg;
static struct clk *clk_apu_conn_edma_0_cg;
static struct clk *clk_apu_conn_edma_1_cg;
static struct clk *clk_apu_conn_edmal_0_cg;
static struct clk *clk_apu_conn_edmal_1_cg;
static struct clk *clk_apu_conn_mnoc_cg;
static struct clk *clk_apu_conn_tcm_cg;
static struct clk *clk_apu_conn_md32_cg;
static struct clk *clk_apu_conn_iommu_0_cg;
static struct clk *clk_apu_conn_iommu_1_cg;
static struct clk *clk_apu_conn_md32_32k_cg;

/* for apusys vcore */
static struct clk *clk_apusys_vcore_ahb_cg;
static struct clk *clk_apusys_vcore_axi_cg;
static struct clk *clk_apusys_vcore_adl_cg;
static struct clk *clk_apusys_vcore_qos_cg;

/* for dvfs clock parent */
static struct clk *clk_top_clk26m;		//  26
static struct clk *clk_top_mainpll_d4_d2;	// 273
static struct clk *clk_top_mainpll_d4_d4;	// 136
static struct clk *clk_top_univpll_d4_d2;	// 312
static struct clk *clk_top_univpll_d6_d2;	// 208
static struct clk *clk_top_univpll_d6_d4;	// 104
static struct clk *clk_top_mmpll_d7;		// 392
static struct clk *clk_top_mmpll_d6;		// 458
static struct clk *clk_top_mmpll_d5;		// 550
static struct clk *clk_top_mmpll_d4;		// 687
static struct clk *clk_top_univpll_d6;		// 416
static struct clk *clk_top_univpll_d5;		// 499
static struct clk *clk_top_univpll_d4;		// 624
static struct clk *clk_top_univpll_d3;		// 832
static struct clk *clk_top_mainpll_d6;		// 364
static struct clk *clk_top_mainpll_d4;		// 546
static struct clk *clk_top_mainpll_d3;		// 728
static struct clk *clk_top_tvdpll_ck;		// 594
static struct clk *clk_top_tvdpll_mainpll_d2_ck;// 594
static struct clk *clk_top_apupll_ck;		// 850

// FIXME: check this is required or not
/* display mtcmos */
static struct clk *mtcmos_dis;

/**************************************************
 * The following functions are vpu dedicated usate
 **************************************************/

int prepare_apu_clock(struct device *dev)
{
	int ret = 0;

	PREPARE_MTCMOS(mtcmos_dis);

	PREPARE_CLK(clk_apusys_vcore_ahb_cg);
	PREPARE_CLK(clk_apusys_vcore_axi_cg);
	PREPARE_CLK(clk_apusys_vcore_adl_cg);
	PREPARE_CLK(clk_apusys_vcore_qos_cg);

	PREPARE_CLK(clk_apu_conn_ahb_cg);
	PREPARE_CLK(clk_apu_conn_axi_cg);
	PREPARE_CLK(clk_apu_conn_isp_cg);
	PREPARE_CLK(clk_apu_conn_cam_adl_cg);
	PREPARE_CLK(clk_apu_conn_img_adl_cg);
	PREPARE_CLK(clk_apu_conn_emi_26m_cg);
	PREPARE_CLK(clk_apu_conn_vpu_udi_cg);
	PREPARE_CLK(clk_apu_conn_edma_0_cg);
	PREPARE_CLK(clk_apu_conn_edma_1_cg);
	PREPARE_CLK(clk_apu_conn_edmal_0_cg);
	PREPARE_CLK(clk_apu_conn_edmal_1_cg);
	PREPARE_CLK(clk_apu_conn_mnoc_cg);
	PREPARE_CLK(clk_apu_conn_tcm_cg);
	PREPARE_CLK(clk_apu_conn_md32_cg);
	PREPARE_CLK(clk_apu_conn_iommu_0_cg);
	PREPARE_CLK(clk_apu_conn_iommu_1_cg);
	PREPARE_CLK(clk_apu_conn_md32_32k_cg);

	PREPARE_CLK(clk_apu_core0_jtag_cg);
	PREPARE_CLK(clk_apu_core0_axi_m_cg);
	PREPARE_CLK(clk_apu_core0_apu_cg);

	PREPARE_CLK(clk_apu_core1_jtag_cg);
	PREPARE_CLK(clk_apu_core1_axi_m_cg);
	PREPARE_CLK(clk_apu_core1_apu_cg);

	PREPARE_CLK(clk_apu_core2_jtag_cg);
	PREPARE_CLK(clk_apu_core2_axi_m_cg);
	PREPARE_CLK(clk_apu_core2_apu_cg);

	PREPARE_CLK(clk_apu_mdla0_cg_b0);
	PREPARE_CLK(clk_apu_mdla0_cg_b1);
	PREPARE_CLK(clk_apu_mdla0_cg_b2);
	PREPARE_CLK(clk_apu_mdla0_cg_b3);
	PREPARE_CLK(clk_apu_mdla0_cg_b4);
	PREPARE_CLK(clk_apu_mdla0_cg_b5);
	PREPARE_CLK(clk_apu_mdla0_cg_b6);
	PREPARE_CLK(clk_apu_mdla0_cg_b7);
	PREPARE_CLK(clk_apu_mdla0_cg_b8);
	PREPARE_CLK(clk_apu_mdla0_cg_b9);
	PREPARE_CLK(clk_apu_mdla0_cg_b10);
	PREPARE_CLK(clk_apu_mdla0_cg_b11);
	PREPARE_CLK(clk_apu_mdla0_cg_b12);
	PREPARE_CLK(clk_apu_mdla0_apb_cg);

	PREPARE_CLK(clk_apu_mdla1_cg_b0);
	PREPARE_CLK(clk_apu_mdla1_cg_b1);
	PREPARE_CLK(clk_apu_mdla1_cg_b2);
	PREPARE_CLK(clk_apu_mdla1_cg_b3);
	PREPARE_CLK(clk_apu_mdla1_cg_b4);
	PREPARE_CLK(clk_apu_mdla1_cg_b5);
	PREPARE_CLK(clk_apu_mdla1_cg_b6);
	PREPARE_CLK(clk_apu_mdla1_cg_b7);
	PREPARE_CLK(clk_apu_mdla1_cg_b8);
	PREPARE_CLK(clk_apu_mdla1_cg_b9);
	PREPARE_CLK(clk_apu_mdla1_cg_b10);
	PREPARE_CLK(clk_apu_mdla1_cg_b11);
	PREPARE_CLK(clk_apu_mdla1_cg_b12);
	PREPARE_CLK(clk_apu_mdla1_apb_cg);

	PREPARE_CLK(clk_top_dsp_sel);
	PREPARE_CLK(clk_top_dsp1_sel);
	PREPARE_CLK(clk_top_dsp2_sel);
	PREPARE_CLK(clk_top_dsp3_sel);
	PREPARE_CLK(clk_top_dsp6_sel);
	PREPARE_CLK(clk_top_dsp7_sel);
	PREPARE_CLK(clk_top_ipu_if_sel);

	PREPARE_CLK(clk_top_clk26m);
	PREPARE_CLK(clk_top_mainpll_d4_d2);
	PREPARE_CLK(clk_top_mainpll_d4_d4);
	PREPARE_CLK(clk_top_univpll_d4_d2);
	PREPARE_CLK(clk_top_univpll_d6_d2);
	PREPARE_CLK(clk_top_univpll_d6_d4);
	PREPARE_CLK(clk_top_mmpll_d7);
	PREPARE_CLK(clk_top_mmpll_d6);
	PREPARE_CLK(clk_top_mmpll_d5);
	PREPARE_CLK(clk_top_mmpll_d4);
	PREPARE_CLK(clk_top_univpll_d6);
	PREPARE_CLK(clk_top_univpll_d5);
	PREPARE_CLK(clk_top_univpll_d4);
	PREPARE_CLK(clk_top_univpll_d3);
	PREPARE_CLK(clk_top_mainpll_d6);
	PREPARE_CLK(clk_top_mainpll_d4);
	PREPARE_CLK(clk_top_mainpll_d3);
	PREPARE_CLK(clk_top_tvdpll_ck);
	PREPARE_CLK(clk_top_tvdpll_mainpll_d2_ck);
	PREPARE_CLK(clk_top_apupll_ck);

	return ret;
}

void unprepare_apu_clock(void)
{
	UNPREPARE_CLK(clk_apu_core0_jtag_cg);
	UNPREPARE_CLK(clk_apu_core0_axi_m_cg);
	UNPREPARE_CLK(clk_apu_core0_apu_cg);

	UNPREPARE_CLK(clk_apu_core1_jtag_cg);
	UNPREPARE_CLK(clk_apu_core1_axi_m_cg);
	UNPREPARE_CLK(clk_apu_core1_apu_cg);

	UNPREPARE_CLK(clk_apu_core2_jtag_cg);
	UNPREPARE_CLK(clk_apu_core2_axi_m_cg);
	UNPREPARE_CLK(clk_apu_core2_apu_cg);

	UNPREPARE_CLK(clk_apu_mdla0_cg_b0);
	UNPREPARE_CLK(clk_apu_mdla0_cg_b1);
	UNPREPARE_CLK(clk_apu_mdla0_cg_b2);
	UNPREPARE_CLK(clk_apu_mdla0_cg_b3);
	UNPREPARE_CLK(clk_apu_mdla0_cg_b4);
	UNPREPARE_CLK(clk_apu_mdla0_cg_b5);
	UNPREPARE_CLK(clk_apu_mdla0_cg_b6);
	UNPREPARE_CLK(clk_apu_mdla0_cg_b7);
	UNPREPARE_CLK(clk_apu_mdla0_cg_b8);
	UNPREPARE_CLK(clk_apu_mdla0_cg_b9);
	UNPREPARE_CLK(clk_apu_mdla0_cg_b10);
	UNPREPARE_CLK(clk_apu_mdla0_cg_b11);
	UNPREPARE_CLK(clk_apu_mdla0_cg_b12);
	UNPREPARE_CLK(clk_apu_mdla0_apb_cg);

	UNPREPARE_CLK(clk_apu_mdla1_cg_b0);
	UNPREPARE_CLK(clk_apu_mdla1_cg_b1);
	UNPREPARE_CLK(clk_apu_mdla1_cg_b2);
	UNPREPARE_CLK(clk_apu_mdla1_cg_b3);
	UNPREPARE_CLK(clk_apu_mdla1_cg_b4);
	UNPREPARE_CLK(clk_apu_mdla1_cg_b5);
	UNPREPARE_CLK(clk_apu_mdla1_cg_b6);
	UNPREPARE_CLK(clk_apu_mdla1_cg_b7);
	UNPREPARE_CLK(clk_apu_mdla1_cg_b8);
	UNPREPARE_CLK(clk_apu_mdla1_cg_b9);
	UNPREPARE_CLK(clk_apu_mdla1_cg_b10);
	UNPREPARE_CLK(clk_apu_mdla1_cg_b11);
	UNPREPARE_CLK(clk_apu_mdla1_cg_b12);
	UNPREPARE_CLK(clk_apu_mdla1_apb_cg);

	UNPREPARE_CLK(clk_apu_conn_ahb_cg);
	UNPREPARE_CLK(clk_apu_conn_axi_cg);
	UNPREPARE_CLK(clk_apu_conn_isp_cg);
	UNPREPARE_CLK(clk_apu_conn_cam_adl_cg);
	UNPREPARE_CLK(clk_apu_conn_img_adl_cg);
	UNPREPARE_CLK(clk_apu_conn_emi_26m_cg);
	UNPREPARE_CLK(clk_apu_conn_vpu_udi_cg);
	UNPREPARE_CLK(clk_apu_conn_edma_0_cg);
	UNPREPARE_CLK(clk_apu_conn_edma_1_cg);
	UNPREPARE_CLK(clk_apu_conn_edmal_0_cg);
	UNPREPARE_CLK(clk_apu_conn_edmal_1_cg);
	UNPREPARE_CLK(clk_apu_conn_mnoc_cg);
	UNPREPARE_CLK(clk_apu_conn_tcm_cg);
	UNPREPARE_CLK(clk_apu_conn_md32_cg);
	UNPREPARE_CLK(clk_apu_conn_iommu_0_cg);
	UNPREPARE_CLK(clk_apu_conn_iommu_1_cg);
	UNPREPARE_CLK(clk_apu_conn_md32_32k_cg);

	UNPREPARE_CLK(clk_apusys_vcore_ahb_cg);
	UNPREPARE_CLK(clk_apusys_vcore_axi_cg);
	UNPREPARE_CLK(clk_apusys_vcore_adl_cg);
	UNPREPARE_CLK(clk_apusys_vcore_qos_cg);

	UNPREPARE_CLK(clk_top_dsp_sel);
	UNPREPARE_CLK(clk_top_dsp1_sel);
	UNPREPARE_CLK(clk_top_dsp2_sel);
	UNPREPARE_CLK(clk_top_dsp3_sel);
	UNPREPARE_CLK(clk_top_dsp6_sel);
	UNPREPARE_CLK(clk_top_dsp7_sel);
	UNPREPARE_CLK(clk_top_ipu_if_sel);

	UNPREPARE_CLK(clk_top_clk26m);
	UNPREPARE_CLK(clk_top_mainpll_d4_d2);
	UNPREPARE_CLK(clk_top_mainpll_d4_d4);
	UNPREPARE_CLK(clk_top_univpll_d4_d2);
	UNPREPARE_CLK(clk_top_univpll_d6_d2);
	UNPREPARE_CLK(clk_top_univpll_d6_d4);
	UNPREPARE_CLK(clk_top_mmpll_d7);
	UNPREPARE_CLK(clk_top_mmpll_d6);
	UNPREPARE_CLK(clk_top_mmpll_d5);
	UNPREPARE_CLK(clk_top_mmpll_d4);
	UNPREPARE_CLK(clk_top_univpll_d6);
	UNPREPARE_CLK(clk_top_univpll_d5);
	UNPREPARE_CLK(clk_top_univpll_d4);
	UNPREPARE_CLK(clk_top_univpll_d3);
	UNPREPARE_CLK(clk_top_mainpll_d6);
	UNPREPARE_CLK(clk_top_mainpll_d4);
	UNPREPARE_CLK(clk_top_mainpll_d3);
	UNPREPARE_CLK(clk_top_tvdpll_ck);
	UNPREPARE_CLK(clk_top_tvdpll_mainpll_d2_ck);
	UNPREPARE_CLK(clk_top_apupll_ck);
}

//per user
void enable_apu_clock(enum DVFS_USER user)
{
	ENABLE_CLK(clk_top_dsp_sel);
	ENABLE_CLK(clk_top_dsp7_sel);
	ENABLE_CLK(clk_top_ipu_if_sel);

	switch (user) {
	default:
	case VPU0:
		ENABLE_CLK(clk_top_dsp1_sel);
		break;
	case VPU1:
		ENABLE_CLK(clk_top_dsp2_sel);
		break;
	case VPU2:
		ENABLE_CLK(clk_top_dsp3_sel);
		break;
	case MDLA0:
	case MDLA1:
		ENABLE_CLK(clk_top_dsp6_sel);
		break;
	}

	ENABLE_MTCMOS(mtcmos_dis);

	udelay(100);

	ENABLE_CLK(clk_apusys_vcore_ahb_cg);
	ENABLE_CLK(clk_apusys_vcore_axi_cg);
	ENABLE_CLK(clk_apusys_vcore_adl_cg);
	ENABLE_CLK(clk_apusys_vcore_qos_cg);

	ENABLE_CLK(clk_apu_conn_ahb_cg);
	ENABLE_CLK(clk_apu_conn_axi_cg);
	ENABLE_CLK(clk_apu_conn_isp_cg);
	ENABLE_CLK(clk_apu_conn_cam_adl_cg);
	ENABLE_CLK(clk_apu_conn_img_adl_cg);
	ENABLE_CLK(clk_apu_conn_emi_26m_cg);
	ENABLE_CLK(clk_apu_conn_vpu_udi_cg);
	ENABLE_CLK(clk_apu_conn_edma_0_cg);
	ENABLE_CLK(clk_apu_conn_edma_1_cg);
	ENABLE_CLK(clk_apu_conn_edmal_0_cg);
	ENABLE_CLK(clk_apu_conn_edmal_1_cg);
	ENABLE_CLK(clk_apu_conn_mnoc_cg);
	ENABLE_CLK(clk_apu_conn_tcm_cg);
	ENABLE_CLK(clk_apu_conn_md32_cg);
	ENABLE_CLK(clk_apu_conn_iommu_0_cg);
	ENABLE_CLK(clk_apu_conn_iommu_1_cg);
	ENABLE_CLK(clk_apu_conn_md32_32k_cg);

	switch (user) {
	default:
	case VPU0:
		ENABLE_CLK(clk_apu_core0_jtag_cg);
		ENABLE_CLK(clk_apu_core0_axi_m_cg);
		ENABLE_CLK(clk_apu_core0_apu_cg);
		break;
	case VPU1:
		ENABLE_CLK(clk_apu_core1_jtag_cg);
		ENABLE_CLK(clk_apu_core1_axi_m_cg);
		ENABLE_CLK(clk_apu_core1_apu_cg);
		break;
	case VPU2:
		ENABLE_CLK(clk_apu_core2_jtag_cg);
		ENABLE_CLK(clk_apu_core2_axi_m_cg);
		ENABLE_CLK(clk_apu_core2_apu_cg);
		break;
	case MDLA0:
		ENABLE_CLK(clk_apu_mdla0_cg_b0);
		ENABLE_CLK(clk_apu_mdla0_cg_b1);
		ENABLE_CLK(clk_apu_mdla0_cg_b2);
		ENABLE_CLK(clk_apu_mdla0_cg_b3);
		ENABLE_CLK(clk_apu_mdla0_cg_b4);
		ENABLE_CLK(clk_apu_mdla0_cg_b5);
		ENABLE_CLK(clk_apu_mdla0_cg_b6);
		ENABLE_CLK(clk_apu_mdla0_cg_b7);
		ENABLE_CLK(clk_apu_mdla0_cg_b8);
		ENABLE_CLK(clk_apu_mdla0_cg_b9);
		ENABLE_CLK(clk_apu_mdla0_cg_b10);
		ENABLE_CLK(clk_apu_mdla0_cg_b11);
		ENABLE_CLK(clk_apu_mdla0_cg_b12);
		ENABLE_CLK(clk_apu_mdla0_apb_cg);
		break;
	case MDLA1:
		ENABLE_CLK(clk_apu_mdla1_cg_b0);
		ENABLE_CLK(clk_apu_mdla1_cg_b1);
		ENABLE_CLK(clk_apu_mdla1_cg_b2);
		ENABLE_CLK(clk_apu_mdla1_cg_b3);
		ENABLE_CLK(clk_apu_mdla1_cg_b4);
		ENABLE_CLK(clk_apu_mdla1_cg_b5);
		ENABLE_CLK(clk_apu_mdla1_cg_b6);
		ENABLE_CLK(clk_apu_mdla1_cg_b7);
		ENABLE_CLK(clk_apu_mdla1_cg_b8);
		ENABLE_CLK(clk_apu_mdla1_cg_b9);
		ENABLE_CLK(clk_apu_mdla1_cg_b10);
		ENABLE_CLK(clk_apu_mdla1_cg_b11);
		ENABLE_CLK(clk_apu_mdla1_cg_b12);
		ENABLE_CLK(clk_apu_mdla1_apb_cg);
		break;
	}

	LOG_DBG("%s enable clk for DVFS_USER: %d", __func__, user);
}

//per user
void disable_apu_clock(enum DVFS_USER user)
{
	switch (user) {
	default:
	case VPU0:
		DISABLE_CLK(clk_apu_core0_jtag_cg);
		DISABLE_CLK(clk_apu_core0_axi_m_cg);
		DISABLE_CLK(clk_apu_core0_apu_cg);
		break;
	case VPU1:
		DISABLE_CLK(clk_apu_core1_jtag_cg);
		DISABLE_CLK(clk_apu_core1_axi_m_cg);
		DISABLE_CLK(clk_apu_core1_apu_cg);
		break;
	case VPU2:
		DISABLE_CLK(clk_apu_core2_jtag_cg);
		DISABLE_CLK(clk_apu_core2_axi_m_cg);
		DISABLE_CLK(clk_apu_core2_apu_cg);
		break;
	case MDLA0:
		DISABLE_CLK(clk_apu_mdla0_cg_b0);
		DISABLE_CLK(clk_apu_mdla0_cg_b1);
		DISABLE_CLK(clk_apu_mdla0_cg_b2);
		DISABLE_CLK(clk_apu_mdla0_cg_b3);
		DISABLE_CLK(clk_apu_mdla0_cg_b4);
		DISABLE_CLK(clk_apu_mdla0_cg_b5);
		DISABLE_CLK(clk_apu_mdla0_cg_b6);
		DISABLE_CLK(clk_apu_mdla0_cg_b7);
		DISABLE_CLK(clk_apu_mdla0_cg_b8);
		DISABLE_CLK(clk_apu_mdla0_cg_b9);
		DISABLE_CLK(clk_apu_mdla0_cg_b10);
		DISABLE_CLK(clk_apu_mdla0_cg_b11);
		DISABLE_CLK(clk_apu_mdla0_cg_b12);
		DISABLE_CLK(clk_apu_mdla0_apb_cg);
		break;
	case MDLA1:
		DISABLE_CLK(clk_apu_mdla1_cg_b0);
		DISABLE_CLK(clk_apu_mdla1_cg_b1);
		DISABLE_CLK(clk_apu_mdla1_cg_b2);
		DISABLE_CLK(clk_apu_mdla1_cg_b3);
		DISABLE_CLK(clk_apu_mdla1_cg_b4);
		DISABLE_CLK(clk_apu_mdla1_cg_b5);
		DISABLE_CLK(clk_apu_mdla1_cg_b6);
		DISABLE_CLK(clk_apu_mdla1_cg_b7);
		DISABLE_CLK(clk_apu_mdla1_cg_b8);
		DISABLE_CLK(clk_apu_mdla1_cg_b9);
		DISABLE_CLK(clk_apu_mdla1_cg_b10);
		DISABLE_CLK(clk_apu_mdla1_cg_b11);
		DISABLE_CLK(clk_apu_mdla1_cg_b12);
		DISABLE_CLK(clk_apu_mdla1_apb_cg);
		break;
	}

	DISABLE_CLK(clk_apu_conn_ahb_cg);
	DISABLE_CLK(clk_apu_conn_axi_cg);
	DISABLE_CLK(clk_apu_conn_isp_cg);
	DISABLE_CLK(clk_apu_conn_cam_adl_cg);
	DISABLE_CLK(clk_apu_conn_img_adl_cg);
	DISABLE_CLK(clk_apu_conn_emi_26m_cg);
	DISABLE_CLK(clk_apu_conn_vpu_udi_cg);
	DISABLE_CLK(clk_apu_conn_edma_0_cg);
	DISABLE_CLK(clk_apu_conn_edma_1_cg);
	DISABLE_CLK(clk_apu_conn_edmal_0_cg);
	DISABLE_CLK(clk_apu_conn_edmal_1_cg);
	DISABLE_CLK(clk_apu_conn_mnoc_cg);
	DISABLE_CLK(clk_apu_conn_tcm_cg);
	DISABLE_CLK(clk_apu_conn_md32_cg);
	DISABLE_CLK(clk_apu_conn_iommu_0_cg);
	DISABLE_CLK(clk_apu_conn_iommu_1_cg);
	DISABLE_CLK(clk_apu_conn_md32_32k_cg);

	DISABLE_CLK(clk_apusys_vcore_ahb_cg);
	DISABLE_CLK(clk_apusys_vcore_axi_cg);
	DISABLE_CLK(clk_apusys_vcore_adl_cg);
	DISABLE_CLK(clk_apusys_vcore_qos_cg);

	DISABLE_MTCMOS(mtcmos_dis);

	DISABLE_CLK(clk_top_dsp_sel);
	DISABLE_CLK(clk_top_ipu_if_sel);
	DISABLE_CLK(clk_top_dsp7_sel);

	switch (user) {
	default:
	case VPU0:
		DISABLE_CLK(clk_top_dsp1_sel);
		break;
	case VPU1:
		DISABLE_CLK(clk_top_dsp2_sel);
		break;
	case VPU2:
		DISABLE_CLK(clk_top_dsp3_sel);
		break;
	case MDLA0:
	case MDLA1:
		DISABLE_CLK(clk_top_dsp6_sel);
		break;
	}

	LOG_DBG("%s disable clk for DVFS_USER: %d", __func__, user);
}

static struct clk *find_clk_by_domain(enum DVFS_VOLTAGE_DOMAIN domain)
{
	switch (domain) {
	case V_APU_CONN:
		return clk_top_dsp_sel;

	case V_VPU0:
		return clk_top_dsp1_sel;

	case V_VPU1:
		return clk_top_dsp2_sel;

	case V_VPU2:
		return clk_top_dsp3_sel;

	case V_MDLA0:
	case V_MDLA1:
		return clk_top_dsp6_sel;

	case V_TOP_IOMMU:
		return clk_top_dsp7_sel;

	default:
	case V_VCORE:
		return clk_top_ipu_if_sel;
	}
}

// FIXME: segmant code ?
// set normal clock
int set_apu_clock_source(enum DVFS_FREQ freq, enum DVFS_VOLTAGE_DOMAIN domain)
{
	struct clk *clk_src;

	switch (freq) {
	case DVFS_FREQ_00_026000_F:
		clk_src = clk_top_clk26m;
		break;

	case DVFS_FREQ_00_104000_F:
		clk_src = clk_top_univpll_d6_d4;
		break;

	case DVFS_FREQ_00_136500_F:
		clk_src = clk_top_mainpll_d4_d4;
		break;

	case DVFS_FREQ_00_208000_F:
		clk_src = clk_top_univpll_d6_d2;
		break;

	case DVFS_FREQ_00_273000_F:
		clk_src = clk_top_mainpll_d4_d2;
		break;
#if 0
	case DVFS_FREQ_00_280000_F:
		clk_src = clk_top_clk26m;
		break;

	case DVFS_FREQ_00_306000_F:
		clk_src = clk_top_clk26m;
		break;
#endif
	case DVFS_FREQ_00_312000_F:
		clk_src = clk_top_univpll_d4_d2;
		break;

	case DVFS_FREQ_00_364000_F:
		clk_src = clk_top_mainpll_d6;
		break;

	case DVFS_FREQ_00_392857_F:
		clk_src = clk_top_mmpll_d7;
		break;

	case DVFS_FREQ_00_416000_F:
		clk_src = clk_top_univpll_d6;
		break;
#if 0
	case DVFS_FREQ_00_457000_F:
		clk_src = clk_top_clk26m;
		break;
#endif
	case DVFS_FREQ_00_458333_F:
		clk_src = clk_top_mmpll_d6;
		break;

	case DVFS_FREQ_00_499200_F:
		clk_src = clk_top_univpll_d5;
		break;

	case DVFS_FREQ_00_546000_F:
		clk_src = clk_top_mainpll_d4;
		break;

	case DVFS_FREQ_00_550000_F:
		clk_src = clk_top_mmpll_d5;
		break;
#if 0
	case DVFS_FREQ_00_572000_F:
		clk_src = clk_top_clk26m;
		break;
#endif
	case DVFS_FREQ_00_594000_F:
		clk_src = clk_top_tvdpll_mainpll_d2_ck;
		break;

	case DVFS_FREQ_00_624000_F:
		clk_src = clk_top_univpll_d4;
		break;

	case DVFS_FREQ_00_687500_F:
		clk_src = clk_top_mmpll_d4;
		break;
#if 0
	case DVFS_FREQ_00_700000_F:
		clk_src = clk_top_clk26m;
		break;
#endif
	case DVFS_FREQ_00_728000_F:
		clk_src = clk_top_mainpll_d3;
		break;
#if 0
	case DVFS_FREQ_00_750000_F:
		clk_src = clk_top_clk26m;
		break;
#endif
	case DVFS_FREQ_00_832000_F:
		clk_src = clk_top_univpll_d3;
		break;

	case DVFS_FREQ_00_850000_F:
		clk_src = clk_top_apupll_ck;
		break;

	case DVFS_FREQ_NOT_SUPPORT:
	default:
		clk_src = clk_top_clk26m;
		LOG_ERR("%s wrong freq : %d, force assign 26M", __func__, freq);
	}

	LOG_DBG("%s config domain %d to opp %d", __func__, domain, freq);
	return clk_set_parent(find_clk_by_domain(domain), clk_src);
}

// FIXME: check clk id to support 3 core VPU and 2 core MDLA
// dump related frequencies of APUsys
void dump_frequency(struct apu_power_info *info)
{
	int dsp_freq = 0;
	int dsp1_freq = 0;
	int dsp2_freq = 0;
	int dsp3_freq = 0;
	int ipuif_freq = 0;
	int dump_div = 1;
#if 0
	int temp_freq = 0;

	temp_freq = mt_get_ckgen_freq(1);

	dsp_freq = mt_get_ckgen_freq(10);
	if (dsp_freq == 0) {
		temp_freq = mt_get_ckgen_freq(1);
		dsp_freq = mt_get_ckgen_freq(10);
	}

	dsp1_freq = mt_get_ckgen_freq(11);
	if (dsp1_freq == 0) {
		temp_freq = mt_get_ckgen_freq(1);
		dsp1_freq = mt_get_ckgen_freq(11);
	}

	dsp2_freq = mt_get_ckgen_freq(12);
	if (dsp2_freq == 0) {
		temp_freq = mt_get_ckgen_freq(1);
		dsp2_freq = mt_get_ckgen_freq(12);
	}

	dsp3_freq = mt_get_ckgen_freq(13);
	if (dsp3_freq == 0) {
		temp_freq = mt_get_ckgen_freq(1);
		dsp3_freq = mt_get_ckgen_freq(13);
	}

	ipuif_freq = mt_get_ckgen_freq(14);
	if (ipuif_freq == 0) {
		temp_freq = mt_get_ckgen_freq(1);
		ipuif_freq = mt_get_ckgen_freq(14);
	}

	check_vpu_clk_sts();
#endif
	if (info->dump_div > 0)
		dump_div = info->dump_div;

	info->dsp_freq = dsp_freq / dump_div;
	info->dsp1_freq = dsp1_freq / dump_div;
	info->dsp2_freq = dsp2_freq / dump_div;
	info->dsp3_freq = dsp3_freq / dump_div;
	info->ipuif_freq = ipuif_freq / dump_div;

	LOG_DBG("dsp_freq = %d\n", dsp_freq);
	LOG_DBG("dsp1_freq = %d\n", dsp1_freq);
	LOG_DBG("dsp2_freq = %d\n", dsp2_freq);
	LOG_DBG("dsp3_freq = %d\n", dsp3_freq);
	LOG_DBG("ipuif_freq = %d\n", ipuif_freq);
}
