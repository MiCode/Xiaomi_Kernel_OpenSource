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
/* clock */
static struct clk *clk_top_dsp_sel;
#if 0
static struct clk *clk_top_dsp1_sel;
static struct clk *clk_top_dsp2_sel;
#endif
static struct clk *clk_top_dsp3_sel;
static struct clk *clk_top_ipu_if_sel;
#if 0
static struct clk *clk_apu_core0_jtag_cg;
static struct clk *clk_apu_core0_axi_m_cg;
static struct clk *clk_apu_core0_apu_cg;
static struct clk *clk_apu_core1_jtag_cg;
static struct clk *clk_apu_core1_axi_m_cg;
static struct clk *clk_apu_core1_apu_cg;
#endif
static struct clk *clk_apu_mdla_apb_cg;
static struct clk *clk_apu_mdla_cg_b0;
static struct clk *clk_apu_mdla_cg_b1;
static struct clk *clk_apu_mdla_cg_b2;
static struct clk *clk_apu_mdla_cg_b3;
static struct clk *clk_apu_mdla_cg_b4;
static struct clk *clk_apu_mdla_cg_b5;
static struct clk *clk_apu_mdla_cg_b6;
static struct clk *clk_apu_mdla_cg_b7;
static struct clk *clk_apu_mdla_cg_b8;
static struct clk *clk_apu_mdla_cg_b9;
static struct clk *clk_apu_mdla_cg_b10;
static struct clk *clk_apu_mdla_cg_b11;
static struct clk *clk_apu_mdla_cg_b12;
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

//static struct clk *mtcmos_vpu_vcore_dormant;
static struct clk *mtcmos_vpu_vcore_shutdown;
//static struct clk *mtcmos_vpu_conn_dormant;
static struct clk *mtcmos_vpu_conn_shutdown;
#if 0
static struct clk *mtcmos_vpu_core0_dormant;
static struct clk *mtcmos_vpu_core0_shutdown;
static struct clk *mtcmos_vpu_core1_dormant;
static struct clk *mtcmos_vpu_core1_shutdown;
#endif
//static struct clk *mtcmos_vpu_core2_dormant;
static struct clk *mtcmos_vpu_core2_shutdown;

/* smi */
static struct clk *clk_mmsys_gals_ipu2mm;
static struct clk *clk_mmsys_gals_ipu12mm;
static struct clk *clk_mmsys_gals_comm0;
static struct clk *clk_mmsys_gals_comm1;
static struct clk *clk_mmsys_smi_common;


/**************************************************
 * The following functions are vpu dedicated usate
 **************************************************/

int mdla_prepare_clock(struct device *dev)
{
	int ret = 0;

	PREPARE_MTCMOS(mtcmos_dis);
	PREPARE_MTCMOS(mtcmos_vpu_vcore_shutdown);
	PREPARE_MTCMOS(mtcmos_vpu_conn_shutdown);
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
#if 0
	PREPARE_VPU_CLK(clk_apu_core0_jtag_cg);
	PREPARE_VPU_CLK(clk_apu_core0_axi_m_cg);
	PREPARE_VPU_CLK(clk_apu_core0_apu_cg);
	PREPARE_VPU_CLK(clk_apu_core1_jtag_cg);
	PREPARE_VPU_CLK(clk_apu_core1_axi_m_cg);
	PREPARE_VPU_CLK(clk_apu_core1_apu_cg);
#endif
	PREPARE_CLK(clk_apu_mdla_apb_cg);
	PREPARE_CLK(clk_apu_mdla_cg_b0);
	PREPARE_CLK(clk_apu_mdla_cg_b1);
	PREPARE_CLK(clk_apu_mdla_cg_b2);
	PREPARE_CLK(clk_apu_mdla_cg_b3);
	PREPARE_CLK(clk_apu_mdla_cg_b4);
	PREPARE_CLK(clk_apu_mdla_cg_b5);
	PREPARE_CLK(clk_apu_mdla_cg_b6);
	PREPARE_CLK(clk_apu_mdla_cg_b7);
	PREPARE_CLK(clk_apu_mdla_cg_b8);
	PREPARE_CLK(clk_apu_mdla_cg_b9);
	PREPARE_CLK(clk_apu_mdla_cg_b10);
	PREPARE_CLK(clk_apu_mdla_cg_b11);
	PREPARE_CLK(clk_apu_mdla_cg_b12);
	PREPARE_CLK(clk_top_dsp_sel);
#if 0
	PREPARE_CLK(clk_top_dsp1_sel);
	PREPARE_CLK(clk_top_dsp2_sel);
#endif
	PREPARE_CLK(clk_top_dsp3_sel);
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

void mdla_unprepare_clock(void)
{
#if 0
		UNPREPARE_CLK(clk_apu_core0_jtag_cg);
		UNPREPARE_CLK(clk_apu_core0_axi_m_cg);
		UNPREPARE_CLK(clk_apu_core0_apu_cg);
		UNPREPARE_CLK(clk_apu_core1_jtag_cg);
		UNPREPARE_CLK(clk_apu_core1_axi_m_cg);
		UNPREPARE_CLK(clk_apu_core1_apu_cg);
#endif
		UNPREPARE_CLK(clk_apu_mdla_apb_cg);
		UNPREPARE_CLK(clk_apu_mdla_cg_b0);
		UNPREPARE_CLK(clk_apu_mdla_cg_b1);
		UNPREPARE_CLK(clk_apu_mdla_cg_b2);
		UNPREPARE_CLK(clk_apu_mdla_cg_b3);
		UNPREPARE_CLK(clk_apu_mdla_cg_b4);
		UNPREPARE_CLK(clk_apu_mdla_cg_b5);
		UNPREPARE_CLK(clk_apu_mdla_cg_b6);
		UNPREPARE_CLK(clk_apu_mdla_cg_b7);
		UNPREPARE_CLK(clk_apu_mdla_cg_b8);
		UNPREPARE_CLK(clk_apu_mdla_cg_b9);
		UNPREPARE_CLK(clk_apu_mdla_cg_b10);
		UNPREPARE_CLK(clk_apu_mdla_cg_b11);
		UNPREPARE_CLK(clk_apu_mdla_cg_b12);
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
#if 0
		UNPREPARE_CLK(clk_top_dsp1_sel);
		UNPREPARE_CLK(clk_top_dsp2_sel);
#endif
		UNPREPARE_CLK(clk_top_dsp3_sel);
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
void mdla_enable_clock(int core)
{
	// FIXME: should enable mdla clk by core

	ENABLE_CLK(clk_top_dsp_sel);
	ENABLE_CLK(clk_top_ipu_if_sel);
	ENABLE_CLK(clk_top_dsp3_sel);

	ENABLE_MTCMOS(mtcmos_dis);
	ENABLE_MTCMOS(mtcmos_vpu_vcore_shutdown);
	ENABLE_MTCMOS(mtcmos_vpu_conn_shutdown);
	ENABLE_MTCMOS(mtcmos_vpu_core2_shutdown);

	udelay(500);

	ENABLE_CLK(clk_mmsys_gals_ipu2mm);
	ENABLE_CLK(clk_mmsys_gals_ipu12mm);
	ENABLE_CLK(clk_mmsys_gals_comm0);
	ENABLE_CLK(clk_mmsys_gals_comm1);
	ENABLE_CLK(clk_mmsys_smi_common);
	#if 0
	/*ENABLE_VPU_CLK(clk_ipu_adl_cabgen);*/
	/*ENABLE_VPU_CLK(clk_ipu_conn_dap_rx_cg);*/
	/*ENABLE_VPU_CLK(clk_ipu_conn_apb2axi_cg);*/
	/*ENABLE_VPU_CLK(clk_ipu_conn_apb2ahb_cg);*/
	/*ENABLE_VPU_CLK(clk_ipu_conn_ipu_cab1to2);*/
	/*ENABLE_VPU_CLK(clk_ipu_conn_ipu1_cab1to2);*/
	/*ENABLE_VPU_CLK(clk_ipu_conn_ipu2_cab1to2);*/
	/*ENABLE_VPU_CLK(clk_ipu_conn_cab3to3);*/
	/*ENABLE_VPU_CLK(clk_ipu_conn_cab2to1);*/
	/*ENABLE_VPU_CLK(clk_ipu_conn_cab3to1_slice);*/
	#endif


	ENABLE_CLK(clk_apu_vcore_ahb_cg);
	ENABLE_CLK(clk_apu_vcore_axi_cg);
	ENABLE_CLK(clk_apu_vcore_adl_cg);
	ENABLE_CLK(clk_apu_vcore_qos_cg);

// FIXME: check if mt6885 need this code
#if 0
	/*move vcore cg ctl to atf*/
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

	ENABLE_CLK(clk_apu_mdla_apb_cg);
	ENABLE_CLK(clk_apu_mdla_cg_b0);
	ENABLE_CLK(clk_apu_mdla_cg_b1);
	ENABLE_CLK(clk_apu_mdla_cg_b2);
	ENABLE_CLK(clk_apu_mdla_cg_b3);
	ENABLE_CLK(clk_apu_mdla_cg_b4);
	ENABLE_CLK(clk_apu_mdla_cg_b5);
	ENABLE_CLK(clk_apu_mdla_cg_b6);
	ENABLE_CLK(clk_apu_mdla_cg_b7);
	ENABLE_CLK(clk_apu_mdla_cg_b8);
	ENABLE_CLK(clk_apu_mdla_cg_b9);
	ENABLE_CLK(clk_apu_mdla_cg_b10);
	ENABLE_CLK(clk_apu_mdla_cg_b11);
	ENABLE_CLK(clk_apu_mdla_cg_b12);

	switch (core) {
	case 0:
	default:
		//ENABLE_CLK(clk_apu_core0_jtag_cg);
		//ENABLE_CLK(clk_apu_core0_axi_m_cg);
		//ENABLE_CLK(clk_apu_core0_apu_cg);
		break;
	case 1:
		//ENABLE_CLK(clk_apu_core1_jtag_cg);
		//ENABLE_CLK(clk_apu_core1_axi_m_cg);
		//ENABLE_CLK(clk_apu_core1_apu_cg);
		break;
	case 2:
		//ENABLE_CLK(clk_apu_core1_jtag_cg);
		//ENABLE_CLK(clk_apu_core1_axi_m_cg);
		//ENABLE_CLK(clk_apu_core1_apu_cg);
		break;
	}

	LOG_DBG("%s enable clk for core%d", __func__, core);
}

//per core
void mdla_disable_clock(int core)
{
	// FIXME: check clk table
	switch (core) {
	case 0:
	default:
		//DISABLE_CLK(clk_apu_core0_jtag_cg);
		//DISABLE_CLK(clk_apu_core0_axi_m_cg);
		//DISABLE_CLK(clk_apu_core0_apu_cg);
		break;
	case 1:
		//DISABLE_CLK(clk_apu_core1_jtag_cg);
		//DISABLE_CLK(clk_apu_core1_axi_m_cg);
		//DISABLE_CLK(clk_apu_core1_apu_cg);
		break;
	case 2:
		//DISABLE_CLK(clk_apu_core1_jtag_cg);
		//DISABLE_CLK(clk_apu_core1_axi_m_cg);
		//DISABLE_CLK(clk_apu_core1_apu_cg);
		break;
	}

	LOG_DBG("%s disable clk for core%d", __func__, core);

	DISABLE_CLK(clk_apu_mdla_apb_cg);
	DISABLE_CLK(clk_apu_mdla_cg_b0);
	DISABLE_CLK(clk_apu_mdla_cg_b1);
	DISABLE_CLK(clk_apu_mdla_cg_b2);
	DISABLE_CLK(clk_apu_mdla_cg_b3);
	DISABLE_CLK(clk_apu_mdla_cg_b4);
	DISABLE_CLK(clk_apu_mdla_cg_b5);
	DISABLE_CLK(clk_apu_mdla_cg_b6);
	DISABLE_CLK(clk_apu_mdla_cg_b7);
	DISABLE_CLK(clk_apu_mdla_cg_b8);
	DISABLE_CLK(clk_apu_mdla_cg_b9);
	DISABLE_CLK(clk_apu_mdla_cg_b10);
	DISABLE_CLK(clk_apu_mdla_cg_b11);
	DISABLE_CLK(clk_apu_mdla_cg_b12);

	#if 0
	DISABLE_VPU_CLK(clk_ipu_adl_cabgen);
	DISABLE_VPU_CLK(clk_ipu_conn_dap_rx_cg);
	DISABLE_VPU_CLK(clk_ipu_conn_apb2axi_cg);
	DISABLE_VPU_CLK(clk_ipu_conn_apb2ahb_cg);
	DISABLE_VPU_CLK(clk_ipu_conn_ipu_cab1to2);
	DISABLE_VPU_CLK(clk_ipu_conn_ipu1_cab1to2);
	DISABLE_VPU_CLK(clk_ipu_conn_ipu2_cab1to2);
	DISABLE_VPU_CLK(clk_ipu_conn_cab3to3);
	DISABLE_VPU_CLK(clk_ipu_conn_cab2to1);
	DISABLE_VPU_CLK(clk_ipu_conn_cab3to1_slice);
	#endif
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

	DISABLE_MTCMOS(mtcmos_vpu_core2_shutdown);
	DISABLE_MTCMOS(mtcmos_vpu_conn_shutdown);
	DISABLE_MTCMOS(mtcmos_vpu_vcore_shutdown);
	DISABLE_MTCMOS(mtcmos_dis);

	DISABLE_CLK(clk_top_dsp_sel);
	DISABLE_CLK(clk_top_ipu_if_sel);
	DISABLE_CLK(clk_top_dsp3_sel);
}


static struct clk *find_clk_by_domain(enum DVFS_VOLTAGE_DOMAIN domain)
{
	switch (domain) {
#if 0
	case V_VPU0:
		return clk_top_dsp1_sel;

	case V_VPU1:
		return clk_top_dsp2_sel;
#endif

	// FIXME: check clk table
	case V_MDLA0:
		return clk_top_dsp3_sel;

	case V_MDLA1:
		return clk_top_dsp3_sel;

	case V_APU_CONN:
		return clk_top_dsp_sel;

	default:
	case V_VCORE:
		return clk_top_ipu_if_sel;
	}
}


// set normal clock
int mdla_set_clock_source(int target_opp, enum DVFS_VOLTAGE_DOMAIN domain)
{
	struct clk *clk_src;

	/* set dsp frequency - 0:788 MHz, 1:700 MHz, 2:606 MHz, 3:594 MHz*/
	/* set dsp frequency - 4:560 MHz, 5:525 MHz, 6:450 MHz, 7:416 MHz*/
	/* set dsp frequency - 8:364 MHz, 9:312 MHz, 10:273 MH, 11:208 MH*/
	/* set dsp frequency - 12:137 MHz, 13:104 MHz, 14:52 MHz, 15:26 MHz*/

	switch (target_opp) {
	case 0:
		clk_src = clk_top_mmpll_d4;
		break;
	case 1:
		clk_src = clk_top_adsppll_d4;
		break;
	case 2:
		clk_src = clk_top_univpll_d2;
		break;
	case 3:
		clk_src = clk_top_tvdpll_mainpll_d2_ck;
		break;
	case 4:
		clk_src = clk_top_tvdpll_ck;
		break;
	case 5:
		clk_src = clk_top_mainpll_d2;
		break;
	case 6:
		clk_src = clk_top_mmpll_d6;
		break;
	case 7:
		clk_src = clk_top_mmpll_d7;
		break;
	case 8:
		clk_src = clk_top_univpll_d3;
		break;
	case 9:
		clk_src = clk_top_mainpll_d3;
		break;
	case 10:
		clk_src = clk_top_univpll_d2_d2;
		break;
	case 11:
		clk_src = clk_top_mainpll_d2_d2;
		break;
	case 12:
		clk_src = clk_top_univpll_d3_d2;
		break;
	case 13:
		clk_src = clk_top_mainpll_d2_d4;
		break;
	case 14:
		clk_src = clk_top_univpll_d3_d8;
		break;
	case 15:
		clk_src = clk_top_clk26m;
		break;
	default:
		LOG_ERR("wrong freq step(%d)", target_opp);
		return -EINVAL;
	}

	return clk_set_parent(find_clk_by_domain(domain), clk_src);
}

