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
 * MUST mapping to clock-names @ mt6779.dts
 **********************************************/
/* clock */
static struct clk *clk_top_dsp_sel;
static struct clk *clk_top_dsp1_sel;
static struct clk *clk_top_dsp2_sel;
static struct clk *clk_top_ipu_if_sel;
static struct clk *clk_apu_core0_jtag_cg;
static struct clk *clk_apu_core0_axi_m_cg;
static struct clk *clk_apu_core0_apu_cg;
static struct clk *clk_apu_core1_jtag_cg;
static struct clk *clk_apu_core1_axi_m_cg;
static struct clk *clk_apu_core1_apu_cg;
static struct clk *clk_apu_conn_apu_cg;
static struct clk *clk_apu_conn_ahb_cg;
static struct clk *clk_apu_conn_axi_cg;
static struct clk *clk_apu_conn_isp_cg;
static struct clk *clk_apu_conn_cam_adl_cg;
static struct clk *clk_apu_conn_img_adl_cg;
static struct clk *clk_apu_conn_emi_26m_cg;
static struct clk *clk_apu_conn_vpu_udi_cg;
static struct clk *clk_apu_vcore_ahb_cg;
static struct clk *clk_apu_vcore_axi_cg;
static struct clk *clk_apu_vcore_adl_cg;
static struct clk *clk_apu_vcore_qos_cg;
static struct clk *clk_top_clk26m;
static struct clk *clk_top_univpll_d3_d8;
static struct clk *clk_top_univpll_d3_d4;
static struct clk *clk_top_mainpll_d2_d4;
static struct clk *clk_top_univpll_d3_d2;
static struct clk *clk_top_mainpll_d2_d2;
static struct clk *clk_top_univpll_d2_d2;
static struct clk *clk_top_mainpll_d3;
static struct clk *clk_top_univpll_d3;
static struct clk *clk_top_mmpll_d7;
static struct clk *clk_top_mmpll_d6;
static struct clk *clk_top_adsppll_d5;
static struct clk *clk_top_tvdpll_ck;
static struct clk *clk_top_tvdpll_mainpll_d2_ck;
static struct clk *clk_top_univpll_d2;
static struct clk *clk_top_adsppll_d4;
static struct clk *clk_top_mainpll_d2;
static struct clk *clk_top_mmpll_d4;

/* mtcmos */
static struct clk *mtcmos_dis;
static struct clk *mtcmos_vpu_vcore_shutdown;
static struct clk *mtcmos_vpu_conn_shutdown;
static struct clk *mtcmos_vpu_core0_shutdown;
static struct clk *mtcmos_vpu_core1_shutdown;
static struct clk *mtcmos_vpu_core2_shutdown;

/* smi clock */
static struct clk *clk_mmsys_gals_ipu2mm;
static struct clk *clk_mmsys_gals_ipu12mm;
static struct clk *clk_mmsys_gals_comm0;
static struct clk *clk_mmsys_gals_comm1;
static struct clk *clk_mmsys_smi_common;


/**************************************************
 * The following functions are vpu dedicated usate
 **************************************************/

int vpu_prepare_clock(struct device *dev)
{
	int ret = 0;

	PREPARE_MTCMOS(mtcmos_dis);
	PREPARE_MTCMOS(mtcmos_vpu_vcore_shutdown);
	PREPARE_MTCMOS(mtcmos_vpu_conn_shutdown);
	PREPARE_MTCMOS(mtcmos_vpu_core0_shutdown);
	PREPARE_MTCMOS(mtcmos_vpu_core1_shutdown);
	PREPARE_MTCMOS(mtcmos_vpu_core2_shutdown);

	PREPARE_CLK(clk_mmsys_gals_ipu2mm);
	PREPARE_CLK(clk_mmsys_gals_ipu12mm);
	PREPARE_CLK(clk_mmsys_gals_comm0);
	PREPARE_CLK(clk_mmsys_gals_comm1);
	PREPARE_CLK(clk_mmsys_smi_common);
	PREPARE_CLK(clk_apu_vcore_ahb_cg);
	PREPARE_CLK(clk_apu_vcore_axi_cg);
	PREPARE_CLK(clk_apu_vcore_adl_cg);
	PREPARE_CLK(clk_apu_vcore_qos_cg);
	PREPARE_CLK(clk_apu_conn_apu_cg);
	PREPARE_CLK(clk_apu_conn_ahb_cg);
	PREPARE_CLK(clk_apu_conn_axi_cg);
	PREPARE_CLK(clk_apu_conn_isp_cg);
	PREPARE_CLK(clk_apu_conn_cam_adl_cg);
	PREPARE_CLK(clk_apu_conn_img_adl_cg);
	PREPARE_CLK(clk_apu_conn_emi_26m_cg);
	PREPARE_CLK(clk_apu_conn_vpu_udi_cg);
	PREPARE_CLK(clk_apu_core0_jtag_cg);
	PREPARE_CLK(clk_apu_core0_axi_m_cg);
	PREPARE_CLK(clk_apu_core0_apu_cg);
	PREPARE_CLK(clk_apu_core1_jtag_cg);
	PREPARE_CLK(clk_apu_core1_axi_m_cg);
	PREPARE_CLK(clk_apu_core1_apu_cg);
	PREPARE_CLK(clk_top_dsp_sel);
	PREPARE_CLK(clk_top_dsp1_sel);
	PREPARE_CLK(clk_top_dsp2_sel);
	PREPARE_CLK(clk_top_ipu_if_sel);
	PREPARE_CLK(clk_top_clk26m);
	PREPARE_CLK(clk_top_univpll_d3_d8);
	PREPARE_CLK(clk_top_univpll_d3_d4);
	PREPARE_CLK(clk_top_mainpll_d2_d4);
	PREPARE_CLK(clk_top_univpll_d3_d2);
	PREPARE_CLK(clk_top_mainpll_d2_d2);
	PREPARE_CLK(clk_top_univpll_d2_d2);
	PREPARE_CLK(clk_top_mainpll_d3);
	PREPARE_CLK(clk_top_univpll_d3);
	PREPARE_CLK(clk_top_mmpll_d7);
	PREPARE_CLK(clk_top_mmpll_d6);
	PREPARE_CLK(clk_top_adsppll_d5);
	PREPARE_CLK(clk_top_tvdpll_ck);
	PREPARE_CLK(clk_top_tvdpll_mainpll_d2_ck);
	PREPARE_CLK(clk_top_univpll_d2);
	PREPARE_CLK(clk_top_adsppll_d4);
	PREPARE_CLK(clk_top_mainpll_d2);
	PREPARE_CLK(clk_top_mmpll_d4);

	return ret;
}

void vpu_unprepare_clock(void)
{
	UNPREPARE_CLK(clk_apu_core0_jtag_cg);
	UNPREPARE_CLK(clk_apu_core0_axi_m_cg);
	UNPREPARE_CLK(clk_apu_core0_apu_cg);
	UNPREPARE_CLK(clk_apu_core1_jtag_cg);
	UNPREPARE_CLK(clk_apu_core1_axi_m_cg);
	UNPREPARE_CLK(clk_apu_core1_apu_cg);
	UNPREPARE_CLK(clk_apu_conn_apu_cg);
	UNPREPARE_CLK(clk_apu_conn_ahb_cg);
	UNPREPARE_CLK(clk_apu_conn_axi_cg);
	UNPREPARE_CLK(clk_apu_conn_isp_cg);
	UNPREPARE_CLK(clk_apu_conn_cam_adl_cg);
	UNPREPARE_CLK(clk_apu_conn_img_adl_cg);
	UNPREPARE_CLK(clk_apu_conn_emi_26m_cg);
	UNPREPARE_CLK(clk_apu_conn_vpu_udi_cg);
	UNPREPARE_CLK(clk_apu_vcore_ahb_cg);
	UNPREPARE_CLK(clk_apu_vcore_axi_cg);
	UNPREPARE_CLK(clk_apu_vcore_adl_cg);
	UNPREPARE_CLK(clk_apu_vcore_qos_cg);
	UNPREPARE_CLK(clk_mmsys_gals_ipu2mm);
	UNPREPARE_CLK(clk_mmsys_gals_ipu12mm);
	UNPREPARE_CLK(clk_mmsys_gals_comm0);
	UNPREPARE_CLK(clk_mmsys_gals_comm1);
	UNPREPARE_CLK(clk_mmsys_smi_common);
	UNPREPARE_CLK(clk_top_dsp_sel);
	UNPREPARE_CLK(clk_top_dsp1_sel);
	UNPREPARE_CLK(clk_top_dsp2_sel);
	UNPREPARE_CLK(clk_top_ipu_if_sel);
	UNPREPARE_CLK(clk_top_clk26m);
	UNPREPARE_CLK(clk_top_univpll_d3_d8);
	UNPREPARE_CLK(clk_top_univpll_d3_d4);
	UNPREPARE_CLK(clk_top_mainpll_d2_d4);
	UNPREPARE_CLK(clk_top_univpll_d3_d2);
	UNPREPARE_CLK(clk_top_mainpll_d2_d2);
	UNPREPARE_CLK(clk_top_univpll_d2_d2);
	UNPREPARE_CLK(clk_top_mainpll_d3);
	UNPREPARE_CLK(clk_top_univpll_d3);
	UNPREPARE_CLK(clk_top_mmpll_d7);
	UNPREPARE_CLK(clk_top_mmpll_d6);
	UNPREPARE_CLK(clk_top_adsppll_d5);
	UNPREPARE_CLK(clk_top_tvdpll_ck);
	UNPREPARE_CLK(clk_top_tvdpll_mainpll_d2_ck);
	UNPREPARE_CLK(clk_top_univpll_d2);
	UNPREPARE_CLK(clk_top_adsppll_d4);
	UNPREPARE_CLK(clk_top_mainpll_d2);
	UNPREPARE_CLK(clk_top_mmpll_d4);
}

//per core
void vpu_enable_clock(int core)
{

	ENABLE_CLK(clk_top_dsp_sel);
	ENABLE_CLK(clk_top_ipu_if_sel);
	ENABLE_CLK(clk_top_dsp1_sel);
	ENABLE_CLK(clk_top_dsp2_sel);

	ENABLE_MTCMOS(mtcmos_dis);
	ENABLE_MTCMOS(mtcmos_vpu_vcore_shutdown);
	ENABLE_MTCMOS(mtcmos_vpu_conn_shutdown);
	ENABLE_MTCMOS(mtcmos_vpu_core0_shutdown);
	ENABLE_MTCMOS(mtcmos_vpu_core1_shutdown);

	udelay(500);

	ENABLE_CLK(clk_mmsys_gals_ipu2mm);
	ENABLE_CLK(clk_mmsys_gals_ipu12mm);
	ENABLE_CLK(clk_mmsys_gals_comm0);
	ENABLE_CLK(clk_mmsys_gals_comm1);
	ENABLE_CLK(clk_mmsys_smi_common);

	ENABLE_CLK(clk_apu_vcore_ahb_cg);
	ENABLE_CLK(clk_apu_vcore_axi_cg);
	ENABLE_CLK(clk_apu_vcore_adl_cg);
	ENABLE_CLK(clk_apu_vcore_qos_cg);

// FIXME: check if mt6885 need this code
#if 0
	vcore_cg_ctl(1);
#endif

	ENABLE_CLK(clk_apu_conn_apu_cg);
	ENABLE_CLK(clk_apu_conn_ahb_cg);
	ENABLE_CLK(clk_apu_conn_axi_cg);
	ENABLE_CLK(clk_apu_conn_isp_cg);
	ENABLE_CLK(clk_apu_conn_cam_adl_cg);
	ENABLE_CLK(clk_apu_conn_img_adl_cg);
	ENABLE_CLK(clk_apu_conn_emi_26m_cg);
	ENABLE_CLK(clk_apu_conn_vpu_udi_cg);

	switch (core) {
	case 0:
	default:
		ENABLE_CLK(clk_apu_core0_jtag_cg);
		ENABLE_CLK(clk_apu_core0_axi_m_cg);
		ENABLE_CLK(clk_apu_core0_apu_cg);
		break;
	case 1:
		ENABLE_CLK(clk_apu_core1_jtag_cg);
		ENABLE_CLK(clk_apu_core1_axi_m_cg);
		ENABLE_CLK(clk_apu_core1_apu_cg);
		break;
	case 2:
		ENABLE_CLK(clk_apu_core1_jtag_cg);
		ENABLE_CLK(clk_apu_core1_axi_m_cg);
		ENABLE_CLK(clk_apu_core1_apu_cg);
		break;
	}

	LOG_DBG("%s enable clk for core%d", __func__, core);
}

//per core
void vpu_disable_clock(int core)
{
	switch (core) {
	case 0:
	default:
		DISABLE_CLK(clk_apu_core0_jtag_cg);
		DISABLE_CLK(clk_apu_core0_axi_m_cg);
		DISABLE_CLK(clk_apu_core0_apu_cg);
		break;
	case 1:
		DISABLE_CLK(clk_apu_core1_jtag_cg);
		DISABLE_CLK(clk_apu_core1_axi_m_cg);
		DISABLE_CLK(clk_apu_core1_apu_cg);
		break;
	case 2:
		DISABLE_CLK(clk_apu_core1_jtag_cg);
		DISABLE_CLK(clk_apu_core1_axi_m_cg);
		DISABLE_CLK(clk_apu_core1_apu_cg);
		break;
	}

	LOG_DBG("%s disable clk for core%d", __func__, core);

	DISABLE_CLK(clk_apu_conn_apu_cg);
	DISABLE_CLK(clk_apu_conn_ahb_cg);
	DISABLE_CLK(clk_apu_conn_axi_cg);
	DISABLE_CLK(clk_apu_conn_isp_cg);
	DISABLE_CLK(clk_apu_conn_cam_adl_cg);
	DISABLE_CLK(clk_apu_conn_img_adl_cg);
	DISABLE_CLK(clk_apu_conn_emi_26m_cg);
	DISABLE_CLK(clk_apu_conn_vpu_udi_cg);
	DISABLE_CLK(clk_apu_vcore_ahb_cg);
	DISABLE_CLK(clk_apu_vcore_axi_cg);
	DISABLE_CLK(clk_apu_vcore_adl_cg);
	DISABLE_CLK(clk_apu_vcore_qos_cg);
	DISABLE_CLK(clk_mmsys_gals_ipu2mm);
	DISABLE_CLK(clk_mmsys_gals_ipu12mm);
	DISABLE_CLK(clk_mmsys_gals_comm0);
	DISABLE_CLK(clk_mmsys_gals_comm1);
	DISABLE_CLK(clk_mmsys_smi_common);

	DISABLE_MTCMOS(mtcmos_vpu_core1_shutdown);
	DISABLE_MTCMOS(mtcmos_vpu_core0_shutdown);
	DISABLE_MTCMOS(mtcmos_vpu_conn_shutdown);
	DISABLE_MTCMOS(mtcmos_vpu_vcore_shutdown);
	DISABLE_MTCMOS(mtcmos_dis);

	DISABLE_CLK(clk_top_dsp_sel);
	DISABLE_CLK(clk_top_ipu_if_sel);
	DISABLE_CLK(clk_top_dsp1_sel);
	DISABLE_CLK(clk_top_dsp2_sel);
}

static struct clk *find_clk_by_domain(enum DVFS_VOLTAGE_DOMAIN domain)
{
	switch (domain) {

	// FIXME: check clk table
	case V_VPU0:
		return clk_top_dsp1_sel;

	case V_VPU1:
		return clk_top_dsp2_sel;

	case V_VPU2:
		return clk_top_dsp2_sel;
/*
 *	case V_MDLA:
 *		return clk_top_dsp3_sel;
 */
	case V_APU_CONN:
		return clk_top_dsp_sel;

	default:
	case V_VCORE:
		return clk_top_ipu_if_sel;
	}
}

// set normal clock
int vpu_set_clock_source(int target_opp, enum DVFS_VOLTAGE_DOMAIN domain)
{
	struct clk *clk_src;

	switch (target_opp) {
	case 0:
//FIXME
/*
 *		if (segment_index == SEGMENT_95)
 *			clk_src = clk_top_adsppll_d5;
 *		else
 */
			clk_src = clk_top_adsppll_d4;
		break;
	case 1:
		clk_src = clk_top_univpll_d2;
		break;
	case 2:
		clk_src = clk_top_tvdpll_mainpll_d2_ck;
		break;
	case 3:
		clk_src = clk_top_tvdpll_ck;
		break;
	case 4:
//FIXME
/*
 *		if (segment_index == SEGMENT_95)
 *			clk_src = clk_top_tvdpll_ck;
 *		else
 */
			clk_src = clk_top_adsppll_d5;
		break;
	case 5:
		clk_src = clk_top_mmpll_d6;
		break;
	case 6:
		clk_src = clk_top_mmpll_d7;
		break;
	case 7:
		clk_src = clk_top_univpll_d3;
		break;
	case 8:
		clk_src = clk_top_mainpll_d3;
		break;
	case 9:
		clk_src = clk_top_univpll_d2_d2;
		break;
	case 10:
		clk_src = clk_top_mainpll_d2_d2;
		break;
	case 11:
		clk_src = clk_top_univpll_d3_d2;
		break;
	case 12:
		clk_src = clk_top_mainpll_d2_d4;
		break;
	case 13:
		clk_src = clk_top_univpll_d3_d4;
		break;
	case 14:
		clk_src = clk_top_univpll_d3_d8;
		break;
	case 15:
		clk_src = clk_top_clk26m;
		break;
	default:
		LOG_ERR("%s wrong freq opp(%d)", __func__, target_opp);
		return -EINVAL;
	}

	LOG_DBG("%s config domain %d to opp %d", __func__, domain, target_opp);
	return clk_set_parent(find_clk_by_domain(domain), clk_src);
}

// set vcore and conn clock
int set_if_clock_source(int target_opp, enum DVFS_VOLTAGE_DOMAIN domain)
{
	struct clk *clk_src;

	switch (target_opp) {
	case 0:
		clk_src = clk_top_univpll_d2;
		break;
	case 1:
		clk_src = clk_top_univpll_d2;
		break;
	case 2:
		clk_src = clk_top_tvdpll_mainpll_d2_ck;
		break;
	case 3:
		clk_src = clk_top_tvdpll_ck;
		break;
	case 4:
		clk_src = clk_top_adsppll_d5;
		break;
	case 5:
		clk_src = clk_top_mmpll_d6;
		break;
	case 6:
		clk_src = clk_top_mmpll_d7;
		break;
	case 7:
		clk_src = clk_top_univpll_d3;
		break;
	case 8:
		clk_src = clk_top_mainpll_d3;
		break;
	case 9:
		clk_src = clk_top_univpll_d2_d2;
		break;
	case 10:
		clk_src = clk_top_mainpll_d2_d2;
		break;
	case 11:
		clk_src = clk_top_univpll_d3_d2;
		break;
	case 12:
		clk_src = clk_top_mainpll_d2_d4;
		break;
	case 13:
		clk_src = clk_top_univpll_d3_d4;
		break;
	case 14:
		clk_src = clk_top_univpll_d3_d8;
		break;
	case 15:
		clk_src = clk_top_clk26m;
		break;
	default:
		LOG_ERR("%s wrong freq step(%d)", __func__, target_opp);
		return -EINVAL;

	}

	LOG_DBG("%s config domain %d to opp %d", __func__, domain, target_opp);
	return clk_set_parent(find_clk_by_domain(domain), clk_src);
}

// FIXME: check clk id to support 3 core VPU
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
