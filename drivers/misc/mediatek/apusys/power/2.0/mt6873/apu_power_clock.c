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
#include <linux/clk-provider.h>
#include <linux/delay.h>

#include "apusys_power_reg.h"
#include "apu_power_api.h"
#include "power_clock.h"
#include "apu_log.h"
#include "apusys_power_ctl.h"
#ifdef CONFIG_MTK_FREQ_HOPPING
#include "mtk_freqhopping_drv.h"
#endif

/************** IMPORTANT !! *******************
 * The following name of each clock struct
 * MUST mapping to clock-names @ mt6873.dts
 **********************************************/

/* for dvfs clock source */
static struct clk *clk_top_dsp_sel;		/* CONN */
static struct clk *clk_top_dsp1_sel;
static struct clk *clk_top_dsp1_npupll_sel;	/* VPU_CORE0 */
static struct clk *clk_top_dsp2_sel;
static struct clk *clk_top_dsp2_npupll_sel;	/* VPU_CORE1 */
static struct clk *clk_top_dsp5_sel;
static struct clk *clk_top_dsp5_apupll_sel;	/* MDLA0 */
static struct clk *clk_top_ipu_if_sel;		/* VCORE */

/* for vpu core 0 */
static struct clk *clk_apu_core0_jtag_cg;
static struct clk *clk_apu_core0_axi_m_cg;
static struct clk *clk_apu_core0_apu_cg;

/* for vpu core 1 */
static struct clk *clk_apu_core1_jtag_cg;
static struct clk *clk_apu_core1_axi_m_cg;
static struct clk *clk_apu_core1_apu_cg;

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
static struct clk *clk_apu_mdla0_axi_m_cg;

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
static struct clk *clk_apu_conn_md32_32k_cg;

/* for apusys vcore */
static struct clk *clk_apusys_vcore_ahb_cg;
static struct clk *clk_apusys_vcore_axi_cg;
static struct clk *clk_apusys_vcore_adl_cg;
static struct clk *clk_apusys_vcore_qos_cg;

/* for dvfs clock parent */
static struct clk *clk_top_clk26m;		//  26
static struct clk *clk_top_mainpll_d4_d2;	// 273
static struct clk *clk_top_univpll_d4_d2;	// 312
static struct clk *clk_top_univpll_d6_d2;	// 208
//static struct clk *clk_top_mmpll_d6;		// 458 for dsp7_sel (no use)
//static struct clk *clk_top_mmpll_d5;		// 550 for dsp7_sel (no use)
static struct clk *clk_top_mmpll_d4;		// 687
static struct clk *clk_top_univpll_d5;		// 499
static struct clk *clk_top_univpll_d4;		// 624
static struct clk *clk_top_univpll_d3;		// 832
//static struct clk *clk_top_mainpll_d6;	// 364 for dsp7_sel (no use)
static struct clk *clk_top_mainpll_d4;		// 546
static struct clk *clk_top_mainpll_d3;		// 728
static struct clk *clk_top_tvdpll_ck;		// 594
static struct clk *clk_top_apupll_ck;
static struct clk *clk_top_npupll_ck;

static struct clk *clk_apmixed_apupll_rate;	// apmixed apupll for set rate
static struct clk *clk_apmixed_npupll_rate;	// apmixed npupll for set rate
static struct clk *mtcmos_scp_sys_vpu;		// mtcmos for apu conn/vcore


/**************************************************
 * The following functions are apu dedicated usate
 **************************************************/

int enable_apu_mtcmos(int enable)
{
	int ret = 0;
	int ret_all = 0;

	if (enable)
		ENABLE_CLK(mtcmos_scp_sys_vpu);
	else
		DISABLE_CLK(mtcmos_scp_sys_vpu);


	LOG_DBG("%s enable var = %d, ret = %d\n", __func__, enable, ret_all);
	return ret_all;
}

int prepare_apu_clock(struct device *dev)
{
	int ret = 0;

	PREPARE_CLK(mtcmos_scp_sys_vpu);

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
	PREPARE_CLK(clk_apu_conn_md32_32k_cg);

	PREPARE_CLK(clk_apu_core0_jtag_cg);
	PREPARE_CLK(clk_apu_core0_axi_m_cg);
	PREPARE_CLK(clk_apu_core0_apu_cg);

	PREPARE_CLK(clk_apu_core1_jtag_cg);
	PREPARE_CLK(clk_apu_core1_axi_m_cg);
	PREPARE_CLK(clk_apu_core1_apu_cg);

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
	PREPARE_CLK(clk_apu_mdla0_axi_m_cg);

	PREPARE_CLK(clk_top_dsp_sel);
	PREPARE_CLK(clk_top_dsp1_sel);
	PREPARE_CLK(clk_top_dsp1_npupll_sel);
	PREPARE_CLK(clk_top_dsp2_sel);
	PREPARE_CLK(clk_top_dsp2_npupll_sel);
	PREPARE_CLK(clk_top_dsp5_sel);
	PREPARE_CLK(clk_top_dsp5_apupll_sel);
	PREPARE_CLK(clk_top_ipu_if_sel);

	PREPARE_CLK(clk_top_clk26m);
	PREPARE_CLK(clk_top_mainpll_d4_d2);
	PREPARE_CLK(clk_top_univpll_d4_d2);
	PREPARE_CLK(clk_top_univpll_d6_d2);
	PREPARE_CLK(clk_top_mmpll_d4);
	PREPARE_CLK(clk_top_univpll_d5);
	PREPARE_CLK(clk_top_univpll_d4);
	PREPARE_CLK(clk_top_univpll_d3);
	PREPARE_CLK(clk_top_mainpll_d4);
	PREPARE_CLK(clk_top_mainpll_d3);
	PREPARE_CLK(clk_top_tvdpll_ck);
	PREPARE_CLK(clk_top_apupll_ck);
	PREPARE_CLK(clk_top_npupll_ck);

	PREPARE_CLK(clk_apmixed_apupll_rate);
	PREPARE_CLK(clk_apmixed_npupll_rate);
	return ret;
}

void unprepare_apu_clock(void)
{
	LOG_DBG("%s bypass\n", __func__);
}

#if 0
static void enable_pll(void)
{
	ENABLE_CLK(clk_top_clk26m);
	ENABLE_CLK(clk_top_mainpll_d4_d2);
	ENABLE_CLK(clk_top_univpll_d4_d2);
	ENABLE_CLK(clk_top_univpll_d6_d2);
	ENABLE_CLK(clk_top_mmpll_d4);
	ENABLE_CLK(clk_top_univpll_d5);
	ENABLE_CLK(clk_top_univpll_d4);
	ENABLE_CLK(clk_top_univpll_d3);
	ENABLE_CLK(clk_top_mainpll_d4);
	ENABLE_CLK(clk_top_mainpll_d3);
	ENABLE_CLK(clk_top_tvdpll_ck);
	ENABLE_CLK(clk_top_apupll_ck);
	ENABLE_CLK(clk_top_npupll_ck);
	ENABLE_CLK(clk_apmixed_apupll_rate);
	ENABLE_CLK(clk_apmixed_npupll_rate);
}

static void disable_pll(void)
{
	DISABLE_CLK(clk_top_clk26m);
	DISABLE_CLK(clk_top_mainpll_d4_d2);
	DISABLE_CLK(clk_top_univpll_d4_d2);
	DISABLE_CLK(clk_top_univpll_d6_d2);
	DISABLE_CLK(clk_top_mmpll_d4);
	DISABLE_CLK(clk_top_univpll_d5);
	DISABLE_CLK(clk_top_univpll_d4);
	DISABLE_CLK(clk_top_univpll_d3);
	DISABLE_CLK(clk_top_mainpll_d4);
	DISABLE_CLK(clk_top_mainpll_d3);
	DISABLE_CLK(clk_top_tvdpll_ck);
	DISABLE_CLK(clk_top_apupll_ck);
	DISABLE_CLK(clk_top_npupll_ck);
	DISABLE_CLK(clk_apmixed_apupll_rate);
	DISABLE_CLK(clk_apmixed_npupll_rate);
}
#endif

int enable_apu_vcore_clksrc(void)
{
	int ret = 0;
	int ret_all = 0;

	ENABLE_CLK(clk_top_ipu_if_sel);
	LOG_WRN("%s, ret = %d\n", __func__, ret_all);
	return ret_all;
}

int enable_apu_conn_clksrc(void)
{
	int ret = 0;
	int ret_all = 0;

	ENABLE_CLK(clk_top_dsp_sel);
	//ENABLE_CLK(clk_top_ipu_if_sel);

	//enable_pll();

	if (ret_all)
		LOG_ERR("%s, ret = %d\n", __func__, ret_all);
	else
		LOG_DBG("%s, ret = %d\n", __func__, ret_all);

	return ret_all;
}

int enable_apu_device_clksrc(enum DVFS_USER user)
{
	int ret = 0;
	int ret_all = 0;

	switch (user) {
	case VPU0:
	case VPU1:
		ENABLE_CLK(clk_top_dsp1_sel);
		ENABLE_CLK(clk_top_dsp2_sel);
		ENABLE_CLK(clk_top_npupll_ck);
		ENABLE_CLK(clk_top_dsp1_npupll_sel);
		ENABLE_CLK(clk_top_dsp2_npupll_sel);
		break;
	case MDLA0:
		ENABLE_CLK(clk_top_dsp5_sel);
		ENABLE_CLK(clk_top_apupll_ck);
		ENABLE_CLK(clk_top_dsp5_apupll_sel);
		break;
	default:
		LOG_ERR("%s illegal DVFS_USER: %d\n", __func__, user);
		ret_all = -1;
	}

	if (ret_all)
		LOG_ERR("%s for DVFS_USER: %d, ret = %d\n",
						__func__, user, ret_all);
	else
		LOG_DBG("%s for DVFS_USER: %d, ret = %d\n",
						__func__, user, ret_all);

	return ret_all;
}

int enable_apu_conn_vcore_clock(void)
{
	int ret = 0;
	int ret_all = 0;

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
	ENABLE_CLK(clk_apu_conn_md32_32k_cg);

	if (ret_all)
		LOG_ERR("%s, ret = %d\n", __func__, ret_all);
	else
		LOG_DBG("%s, ret = %d\n", __func__, ret_all);

	return ret_all;
}

//per user
int enable_apu_device_clock(enum DVFS_USER user)
{
	int ret = 0;
	int ret_all = 0;

	switch (user) {
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
		ENABLE_CLK(clk_apu_mdla0_axi_m_cg);
		break;
	default:
		LOG_ERR("%s illegal DVFS_USER: %d\n", __func__, user);
		ret_all = -1;
	}

	if (ret_all)
		LOG_ERR("%s for DVFS_USER: %d, ret = %d\n",
						__func__, user, ret_all);
	else
		LOG_DBG("%s for DVFS_USER: %d, ret = %d\n",
						__func__, user, ret_all);
	return ret_all;
}

void disable_apu_conn_vcore_clock(void)
{
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
	DISABLE_CLK(clk_apu_conn_md32_32k_cg);

	DISABLE_CLK(clk_apusys_vcore_ahb_cg);
	DISABLE_CLK(clk_apusys_vcore_axi_cg);
	DISABLE_CLK(clk_apusys_vcore_adl_cg);
	DISABLE_CLK(clk_apusys_vcore_qos_cg);
	LOG_DBG("%s\n", __func__);
}

//per user
void disable_apu_device_clock(enum DVFS_USER user)
{
	switch (user) {
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
		DISABLE_CLK(clk_apu_mdla0_axi_m_cg);
		break;
	default:
		LOG_ERR("%s illegal DVFS_USER: %d\n", __func__, user);
	}
	LOG_DBG("%s for DVFS_USER: %d\n", __func__, user);
}

void disable_apu_conn_clksrc(void)
{
	//disable_pll();

	DISABLE_CLK(clk_top_dsp_sel);
	//DISABLE_CLK(clk_top_ipu_if_sel);
	LOG_DBG("%s\n", __func__);
}

void disable_apu_device_clksrc(enum DVFS_USER user)
{
	switch (user) {
	case VPU0:
	case VPU1:
		DISABLE_CLK(clk_top_dsp1_npupll_sel);
		DISABLE_CLK(clk_top_dsp2_npupll_sel);
		DISABLE_CLK(clk_top_npupll_ck);
		DISABLE_CLK(clk_top_dsp1_sel);
		DISABLE_CLK(clk_top_dsp2_sel);
		break;
	case MDLA0:
		DISABLE_CLK(clk_top_dsp5_apupll_sel);
		DISABLE_CLK(clk_top_apupll_ck);
		DISABLE_CLK(clk_top_dsp5_sel);
		break;
	default:
		LOG_ERR("%s illegal DVFS_USER: %d\n", __func__, user);
	}
	LOG_DBG("%s for DVFS_USER: %d\n", __func__, user);
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

	case V_MDLA0:
		return clk_top_dsp5_sel;

	case V_VCORE:
		return clk_top_ipu_if_sel;
	default:
		LOG_ERR("%s fail to find clk !\n", __func__);
		return NULL;
	}
}

// set normal clock w/ ckmux
int set_apu_clock_source(enum DVFS_FREQ freq, enum DVFS_VOLTAGE_DOMAIN domain)
{
	int ret = 0;
	struct clk *clk_src = NULL;
	struct clk *clk_target = NULL;
#if APUSYS_SETTLE_TIME_TEST
	u8 buck_id;
#endif

	switch (freq) {
	case DVFS_FREQ_00_026000_F:
		clk_src = clk_top_clk26m;
		break;

	case DVFS_FREQ_00_208000_F:
		clk_src = clk_top_univpll_d6_d2;
		break;

	case DVFS_FREQ_00_273000_F:
		clk_src = clk_top_mainpll_d4_d2;
		break;

	case DVFS_FREQ_00_312000_F:
		clk_src = clk_top_univpll_d4_d2;
		break;

	case DVFS_FREQ_00_499200_F:
		clk_src = clk_top_univpll_d5;
		break;

	case DVFS_FREQ_00_546000_F:
		clk_src = clk_top_mainpll_d4;
		break;

	case DVFS_FREQ_00_624000_F:
		clk_src = clk_top_univpll_d4;
		break;

	case DVFS_FREQ_00_687500_F:
		clk_src = clk_top_mmpll_d4;
		break;

	case DVFS_FREQ_00_728000_F:
		clk_src = clk_top_mainpll_d3;
		break;

	case DVFS_FREQ_00_832000_F:
		clk_src = clk_top_univpll_d3;
		break;

	case DVFS_FREQ_NOT_SUPPORT:
	default:
		clk_src = clk_top_clk26m;
		LOG_ERR("%s wrong freq : %d, force assign 26M\n",
							__func__, freq);
	}

	clk_target = find_clk_by_domain(domain);

	if (clk_target != NULL) {
#if APUSYS_SETTLE_TIME_TEST
		buck_id = apusys_buck_domain_to_buck[domain];
		apusys_opps.st[buck_id + 1].begin = sched_clock();
#endif
		LOG_DBG("%s config domain %s to freq %d\n",
			__func__, buck_domain_str[domain], freq);

		ret |= clk_set_parent(clk_target, clk_src);

		if (domain == V_VPU0)
			ret |= clk_set_parent(clk_top_dsp1_npupll_sel,
				clk_top_dsp1_sel);
		else if (domain == V_VPU1)
			ret |= clk_set_parent(clk_top_dsp2_npupll_sel,
				clk_top_dsp2_sel);
		else if (domain == V_MDLA0)
			ret |= clk_set_parent(clk_top_dsp5_apupll_sel,
				clk_top_dsp5_sel);

		return ret;
	}
	LOG_ERR("%s config domain %s to freq %d failed\n",
		__func__, buck_domain_str[domain], freq);
	return -1;
}

static unsigned int apu_get_dds(enum DVFS_FREQ freq,
	enum DVFS_VOLTAGE_DOMAIN domain)
{
	int opp = 0;

	for (opp = 0; opp < APUSYS_MAX_NUM_OPPS; opp++) {
		if (apusys_opps.opps[opp][domain].freq == freq)
			return apusys_opps.opps[opp][domain].dds;
	}

	LOG_DBG("%s freq %d find no dds\n",
		__func__, freq);
	return 0;
}

static enum DVFS_FREQ_POSTDIV apu_get_posdiv_power(enum DVFS_FREQ freq,
	enum DVFS_VOLTAGE_DOMAIN domain)
{
	int opp = 0;

	for (opp = 0; opp < APUSYS_MAX_NUM_OPPS; opp++) {
		if (apusys_opps.opps[opp][domain].freq == freq)
			return apusys_opps.opps[opp][domain].post_divider;
	}

	LOG_DBG("%s freq %d find no post divider\n",
		__func__, freq);
	return POSDIV_4;
}

static enum DVFS_FREQ_POSTDIV apu_get_curr_posdiv_power(
	enum DVFS_VOLTAGE_DOMAIN domain)
{
	unsigned long pll  = 0;
	enum DVFS_FREQ_POSTDIV real_posdiv_power = 0;

	if (domain == V_VPU0 || domain == V_VPU1)
		pll = DRV_Reg32(NPUPLL_CON1);
	else if (domain == V_MDLA0)
		pll = DRV_Reg32(APUPLL_CON1);

	real_posdiv_power = (pll & (0x7 << POSDIV_SHIFT)) >> POSDIV_SHIFT;

	LOG_DBG("%s real_posdiv_power %d\n",
		__func__, real_posdiv_power);

	return real_posdiv_power;
}

int config_apupll(enum DVFS_FREQ freq, enum DVFS_VOLTAGE_DOMAIN domain)
{
	int ret = 0;
	struct clk *clk_target = NULL;
#if CCF_SET_RATE
	int scaled_freq = freq * 1000;
#endif
	enum DVFS_FREQ ckmux_freq = 0;
	enum DVFS_FREQ_POSTDIV posdiv_power = 0;
	enum DVFS_FREQ_POSTDIV real_posdiv_power = 0;
	unsigned int dds, pll = 0;
	bool parking = false;
#if APUSYS_SETTLE_TIME_TEST
	u8 buck_id;
#endif
	real_posdiv_power = apu_get_curr_posdiv_power(domain);
	posdiv_power = apu_get_posdiv_power(freq, domain);
	dds = apu_get_dds(freq, domain);
	pll = (0x80000000) | (posdiv_power << POSDIV_SHIFT) | dds;

#ifndef CONFIG_MTK_FREQ_HOPPING
	/* force parking if FHCTL not ready */
	parking = true;
#else
	if (posdiv_power != real_posdiv_power)
		parking = true;
	else
		parking = false;
#endif
#if CCF_SET_RATE
	parking = true;
#endif

	LOG_DBG(
		"posdiv: %d, real_posdiv: %d, dds: 0x%x, pll: 0x%08x, parking: %d\n",
		(1 << posdiv_power), (1 << real_posdiv_power),
		dds, pll, parking);

	if (parking) {
		clk_target = find_clk_by_domain(domain);

		if (clk_target != NULL) {
			ckmux_freq = DVFS_FREQ_00_312000_F;
			ret |= set_apu_clock_source(ckmux_freq, domain);
		} else {
			LOG_ERR("%s config domain %s to freq %d failed\n",
				__func__, buck_domain_str[domain], freq);
			return -1;
		}
#if CCF_SET_RATE
		ret |= clk_set_rate(clk_top_apupll_ck, scaled_freq);
#else
		DRV_WriteReg32(APUPLL_CON1, pll);
#endif
		/* PLL spec */
		udelay(20);

	} else {
#ifdef CONFIG_MTK_FREQ_HOPPING
		mt_dfs_general_pll(APUPLL_FH_PLL, dds);
#endif
	}

	/* If need, switch dsp5_apupll_sel's parent to APUPLL */
	clk_target = clk_get_parent(clk_top_dsp5_apupll_sel);
	if (!clk_is_match(clk_target, clk_top_apupll_ck)) {
		LOG_DBG("%s change %s's parent from %s to %s\n", __func__,
			__clk_get_name(clk_top_dsp5_apupll_sel),
			__clk_get_name(clk_target),
			__clk_get_name(clk_top_apupll_ck));
		ret |= clk_set_parent(clk_top_dsp5_apupll_sel,
				      clk_top_apupll_ck);
	}

	if (ret)
		LOG_ERR("%s fail, ret = %d\n", __func__, ret);
	else
		LOG_DBG("%s pass, ret = %d\n", __func__, ret);

#if APUSYS_SETTLE_TIME_TEST
	buck_id = apusys_buck_domain_to_buck[domain];
	apusys_opps.st[buck_id + 1].begin = sched_clock();
#endif
	LOG_DBG("%s config domain %s to freq %d,  APUPLL_CON1 0x%x\n",
		__func__, buck_domain_str[domain],
		freq, DRV_Reg32(APUPLL_CON1));


	return ret;
}

int config_npupll(enum DVFS_FREQ freq, enum DVFS_VOLTAGE_DOMAIN domain)
{
	int ret = 0;
	struct clk *clk_target = NULL;
#if CCF_SET_RATE
	int scaled_freq = freq * 1000;
#endif
	enum DVFS_FREQ ckmux_freq = 0;
	enum DVFS_FREQ_POSTDIV posdiv_power = 0;
	enum DVFS_FREQ_POSTDIV real_posdiv_power = 0;
	enum DVFS_VOLTAGE_DOMAIN domain_idx = 0;
	unsigned int dds, pll = 0;
	bool parking = false;
#if APUSYS_SETTLE_TIME_TEST
	u8 buck_id;
#endif

	real_posdiv_power = apu_get_curr_posdiv_power(domain);
	posdiv_power = apu_get_posdiv_power(freq, domain);
	dds = apu_get_dds(freq, domain);
	pll = (0x80000000) | (posdiv_power << POSDIV_SHIFT) | dds;

#ifndef CONFIG_MTK_FREQ_HOPPING
	parking = true;
#else
	if (posdiv_power != real_posdiv_power)
		parking = true;
	else
		parking = false;
#endif
#if CCF_SET_RATE
	parking = true;
#endif

	LOG_DBG(
		"%s posdiv: %d, real_posdiv: %d, dds: 0x%x, pll: 0x%08x, parking: %d\n",
		__func__, (1 << posdiv_power), (1 << real_posdiv_power),
		dds, pll, parking);

	if (parking) {
		for (domain_idx = V_VPU0; domain_idx < V_VPU0 + APUSYS_VPU_NUM;
			domain_idx++) {
			clk_target = find_clk_by_domain(domain_idx);

			if (clk_target != NULL) {
				ckmux_freq = DVFS_FREQ_00_273000_F;
				ret |= set_apu_clock_source(ckmux_freq,
					domain_idx);
			} else {
				LOG_ERR("%s config domain %s to freq %d fail\n",
					__func__,
					buck_domain_str[domain_idx],
					freq);
				return -1;
			}
		}
#if CCF_SET_RATE
		ret |= clk_set_rate(clk_apmixed_npupll_rate, scaled_freq);
#else
		DRV_WriteReg32(NPUPLL_CON1, pll);
#endif
		/* PLL spec */
		udelay(20);

	} else {
#ifdef CONFIG_MTK_FREQ_HOPPING
		mt_dfs_general_pll(NPUPLL_FH_PLL, dds);
#endif
	}

	/* If need, switch dsp1_npupll_sel's parent to NPUPLL */
	clk_target = clk_get_parent(clk_top_dsp1_npupll_sel);
	if (domain == V_VPU0 &&
	    !clk_is_match(clk_target, clk_top_npupll_ck)) {
		LOG_DBG("%s modify %s's parent from %s to %s\n", __func__,
			__clk_get_name(clk_top_dsp1_npupll_sel),
			__clk_get_name(clk_target),
			__clk_get_name(clk_top_npupll_ck));
		ret |= clk_set_parent(clk_top_dsp1_npupll_sel,
				      clk_top_npupll_ck);
	}

	/* If need, switch dsp2_npupll_sel's parent to NPUPLL */
	clk_target = clk_get_parent(clk_top_dsp2_npupll_sel);
	if (domain == V_VPU1 &&
	    !clk_is_match(clk_target, clk_top_npupll_ck)) {
		LOG_DBG("%s modify %s's parent from %s to %s\n", __func__,
			__clk_get_name(clk_top_dsp2_npupll_sel),
			__clk_get_name(clk_target),
			__clk_get_name(clk_top_npupll_ck));
		ret |= clk_set_parent(clk_top_dsp2_npupll_sel,
				      clk_top_npupll_ck);
	}

	if (ret)
		LOG_ERR("%s fail, ret = %d\n", __func__, ret);
	else
		LOG_DBG("%s pass, ret = %d\n", __func__, ret);

#if APUSYS_SETTLE_TIME_TEST
	buck_id = apusys_buck_domain_to_buck[domain];
	apusys_opps.st[buck_id + 1].begin = sched_clock();
#endif
	LOG_DBG("%s config domain %s to freq %d, NPUPLL_CON1 0x%x\n",
		__func__, buck_domain_str[domain],
		freq, DRV_Reg32(NPUPLL_CON1));

	return ret;
}

// dump related frequencies of APUsys
void dump_frequency(struct apu_power_info *info)
{
	int dsp_freq = 0;
	int dsp1_freq = 0;
	int dsp2_freq = 0;
	int dsp5_freq = 0;
	int apupll_freq = 0;
	int npupll_freq = 0;
	int ipuif_freq = 0;
	int dump_div = 1;
	int temp_id = 1;
	int temp_freq = 0;
	struct clk *tmpclk = NULL;

	temp_freq = mt_get_ckgen_freq(temp_id);
	dsp_freq = mt_get_ckgen_freq(13);
	if (dsp_freq == 0) {
		temp_freq = mt_get_ckgen_freq(temp_id);
		dsp_freq = mt_get_ckgen_freq(13);
	}

	/*
	 * Move reading (a/n)pupll ahead, since struct apu_power_info
	 * may use (a/n)pupll freq according to different clock parents
	 * of clk_top_dsp(1/2/5)_npupll_sel.
	 */
#if 1
	temp_freq = mt_get_abist_freq(temp_id);
	apupll_freq = mt_get_abist_freq(5);
	if (apupll_freq == 0) {
		temp_freq = mt_get_abist_freq(temp_id);
		apupll_freq = mt_get_abist_freq(5);
	}

	temp_freq = mt_get_abist_freq(temp_id);
	npupll_freq = mt_get_abist_freq(7);
	if (npupll_freq == 0) {
		temp_freq = mt_get_abist_freq(temp_id);
		npupll_freq = mt_get_abist_freq(7);
	}
#endif

#if CCF_SET_RATE // unit: HZ for clk_set_rate use case
	dsp1_freq = clk_get_rate(clk_top_dsp1_npupll_sel);
	dsp2_freq = clk_get_rate(clk_top_dsp2_npupll_sel);
	dsp5_freq = clk_get_rate(clk_top_dsp5_apupll_sel);
#else
	/*
	 * Below dsp1/2/5 are read from TOP_MUX_DSP1/2/5 directly
	 * with dummy flow if 1st time value read back is 0.
	 */
	temp_freq = mt_get_ckgen_freq(temp_id);
	dsp1_freq = mt_get_ckgen_freq(14);
	if (dsp1_freq == 0) {
		temp_freq = mt_get_ckgen_freq(temp_id);
		dsp1_freq = mt_get_ckgen_freq(14);
	}

	temp_freq = mt_get_ckgen_freq(temp_id);
	dsp2_freq = mt_get_ckgen_freq(15);
	if (dsp2_freq == 0) {
		temp_freq = mt_get_ckgen_freq(temp_id);
		dsp2_freq = mt_get_ckgen_freq(15);
	}

	temp_freq = mt_get_ckgen_freq(temp_id);
	dsp5_freq = mt_get_ckgen_freq(16);
	if (dsp5_freq == 0) {
		temp_freq = mt_get_ckgen_freq(temp_id);
		dsp5_freq = mt_get_ckgen_freq(16);
	}
#endif

	temp_freq = mt_get_ckgen_freq(temp_id);
	ipuif_freq = mt_get_ckgen_freq(18);
	if (ipuif_freq == 0) {
		temp_freq = mt_get_ckgen_freq(temp_id);
		ipuif_freq = mt_get_ckgen_freq(18);
	}

	check_vpu_clk_sts();

	if (info->dump_div > 0)
		dump_div = info->dump_div;

	info->dsp_freq = dsp_freq / dump_div;

#if CCF_SET_RATE // unit: HZ for clk_set_rate use case
	info->dsp1_freq = dsp1_freq / (dump_div * 1000);
	info->dsp2_freq = dsp2_freq / (dump_div * 1000);
	info->dsp5_freq = dsp5_freq / (dump_div * 1000);
#else //[Fix me] It's true after clk_set_parent to npupll/apupll

	/* fill info->dsp1 according to dsp1_pll_sel's parent */
	tmpclk = clk_get_parent(clk_top_dsp1_npupll_sel);
	if (!clk_is_match(tmpclk, clk_top_npupll_ck))
		info->dsp1_freq = dsp1_freq / dump_div;
	else
		info->dsp1_freq = npupll_freq / dump_div;

	/* fill info->dsp2 according to dsp2_pll_sel's parent */
	tmpclk = clk_get_parent(clk_top_dsp2_npupll_sel);
	if (!clk_is_match(tmpclk, clk_top_npupll_ck))
		info->dsp2_freq = dsp2_freq / dump_div;
	else
		info->dsp2_freq = npupll_freq / dump_div;

	/* fill info->dsp5 according to dsp5_pll_sel's parent */
	tmpclk = clk_get_parent(clk_top_dsp5_apupll_sel);
	if (!clk_is_match(tmpclk, clk_top_apupll_ck))
		info->dsp5_freq = dsp5_freq / dump_div;
	else
		info->dsp5_freq = apupll_freq / (dump_div * 2);
#endif

	info->apupll_freq = apupll_freq / dump_div;
	info->npupll_freq = npupll_freq / dump_div;
	info->ipuif_freq = ipuif_freq / dump_div;
#if 1
	LOG_DBG("dsp_freq = %d\n", dsp_freq);
	LOG_DBG("dsp1_freq = %d\n", dsp1_freq);
	LOG_DBG("dsp2_freq = %d\n", dsp2_freq);
	LOG_DBG("dsp5_freq = %d\n", dsp5_freq);
	LOG_DBG("apupll_freq = %d\n", apupll_freq);
	LOG_DBG("npupll_freq = %d\n", npupll_freq);
	LOG_DBG("ipuif_freq = %d\n", ipuif_freq);
#endif
}
