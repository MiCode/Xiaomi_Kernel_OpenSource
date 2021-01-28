// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include "mdla_debug.h"
#include "mdla.h"
#include "mdla_pmu.h"
#include "mdla_hw_reg.h"
#include "mdla_ioctl.h"
#include "mdla_trace.h"
#include "mdla_dvfs.h"

#include <linux/types.h>
#include <linux/uaccess.h>

#ifndef MTK_MDLA_FPGA_PORTING
#define ENABLE_PMQOS

#include <linux/clk.h>
#include <linux/soc/mediatek/mtk-pm-qos.h>
#include "apu_dvfs.h"
#include <linux/regulator/consumer.h>
#include "vpu_dvfs.h"
#include <mtk_devinfo.h>
#include "mtk_qos_bound.h"
#include <linux/pm_qos.h>

#ifdef MTK_PERF_OBSERVER
#include <mt-plat/mtk_perfobserver.h>
#endif

/* opp, mW */
struct MDLA_OPP_INFO mdla_power_table[MDLA_OPP_NUM] = {
	{MDLA_OPP_0, 688},
	{MDLA_OPP_1, 611},
	{MDLA_OPP_2, 544},
	{MDLA_OPP_3, 400},
	{MDLA_OPP_4, 392},
	{MDLA_OPP_5, 360},
	{MDLA_OPP_6, 346},
	{MDLA_OPP_7, 297},
	{MDLA_OPP_8, 274},
	{MDLA_OPP_9, 192},
	{MDLA_OPP_10, 164},
	{MDLA_OPP_11, 144},
	{MDLA_OPP_12, 109},

};
#define CMD_WAIT_TIME_MS    (3 * 1000)
#define OPP_WAIT_TIME_MS    (300)
#define PWR_KEEP_TIME_MS    (500)
#define OPP_KEEP_TIME_MS    (500)
#define POWER_ON_MAGIC		(2)
#define OPPTYPE_VCORE		(0)
#define OPPTYPE_DSPFREQ		(1)
#define OPPTYPE_VVPU		(2)
#define OPPTYPE_VMDLA		(3)

/* clock */
static struct clk *clk_top_dsp_sel;
static struct clk *clk_top_dsp3_sel;
static struct clk *clk_top_ipu_if_sel;
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
//static struct clk *mtcmos_vpu_core2_dormant;
static struct clk *mtcmos_vpu_core2_shutdown;

/* smi */
static struct clk *clk_mmsys_gals_ipu2mm;
static struct clk *clk_mmsys_gals_ipu12mm;
static struct clk *clk_mmsys_gals_comm0;
static struct clk *clk_mmsys_gals_comm1;
static struct clk *clk_mmsys_smi_common;
/* workqueue */
struct my_struct_t {
	int core;
	struct delayed_work my_work;
};
static struct workqueue_struct *wq;
static void mdla_power_counter_routine(struct work_struct *);
static struct my_struct_t power_counter_work[MTK_MDLA_USER];

/* static struct workqueue_struct *opp_wq; */
static void mdla_opp_keep_routine(struct work_struct *);
static DECLARE_DELAYED_WORK(opp_keep_work, mdla_opp_keep_routine);


/* power */
static struct mutex power_mutex[MTK_MDLA_USER];
static bool is_power_on[MTK_MDLA_USER];
static bool is_power_debug_lock;
static struct mutex power_counter_mutex[MTK_MDLA_USER];
static int power_counter[MTK_MDLA_USER];
static struct mutex opp_mutex;
static bool force_change_vcore_opp[MTK_MDLA_USER];
static bool force_change_vvpu_opp[MTK_MDLA_USER];
static bool force_change_vmdla_opp[MTK_MDLA_USER];
static bool force_change_dsp_freq[MTK_MDLA_USER];
static bool change_freq_first[MTK_MDLA_USER];
static bool opp_keep_flag;
static uint8_t max_vcore_opp;
//static uint8_t max_vvpu_opp;
static uint8_t max_vmdla_opp;
static uint8_t max_dsp_freq;
static struct mdla_lock_power lock_power[MDLA_OPP_PRIORIYY_NUM][MTK_MDLA_USER];
static uint8_t max_opp[MTK_MDLA_USER];
static uint8_t min_opp[MTK_MDLA_USER];
static uint8_t maxboost[MTK_MDLA_USER];
static uint8_t minboost[MTK_MDLA_USER];
static struct mutex power_lock_mutex;

/* dvfs */
static struct mdla_dvfs_opps opps;
#ifdef ENABLE_PMQOS
static struct mtk_pm_qos_request mdla_qos_bw_request[MTK_MDLA_USER];
static struct mtk_pm_qos_request mdla_qos_vcore_request[MTK_MDLA_USER];
static struct mtk_pm_qos_request mdla_qos_vvpu_request[MTK_MDLA_USER];
static struct mtk_pm_qos_request mdla_qos_vmdla_request[MTK_MDLA_USER];
#endif

/*regulator id*/
static struct regulator *vvpu_reg_id;
static struct regulator *vmdla_reg_id;

static int mdla_init_done;
static uint8_t segment_max_opp;
static uint8_t segment_index;

/* static function prototypes */
static int mdla_boot_up(int core);
static int mdla_shut_down(int core);
static bool mdla_update_lock_power_parameter
	(struct mdla_lock_power *mdla_lock_power);
static bool mdla_update_unlock_power_parameter
	(struct mdla_lock_power *mdla_lock_power);
static uint8_t mdla_boost_value_to_opp(uint8_t boost_value);
static int mdla_lock_set_power(struct mdla_lock_power *mdla_lock_power);

static inline int atf_vcore_cg_ctl(int state)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_KERNEL_APU_VCORE_CG_CTL
			, state, 0, 0, 0, 0, 0, 0, &res);

	return 0;
}

static inline int Map_MDLA_Freq_Table(int freq_opp)
{
	int freq_value = 0;

	switch (freq_opp) {
	case 0:
	default:
		freq_value = 788;
		break;
	case 1:
		freq_value = 700;
		break;
	case 2:
		freq_value = 624;
		break;
	case 3:
		freq_value = 606;
		break;
	case 4:
		freq_value = 594;
		break;
	case 5:
		freq_value = 546;
		break;
	case 6:
		freq_value = 525;
		break;
	case 7:
		freq_value = 450;
		break;
	case 8:
		freq_value = 416;
		break;
	case 9:
		freq_value = 364;
		break;
	case 10:
		freq_value = 312;
		break;
	case 11:
		freq_value = 273;
		break;
	case 12:
		freq_value = 208;
		break;
	case 13:
		freq_value = 137;
		break;
	case 14:
		freq_value = 52;
		break;
	case 15:
		freq_value = 26;
		break;
	}

	return freq_value;
}
#if defined(MDLA_MET_READY)
void MDLA_MET_Events_Trace(bool enter, int core)
{
	int vmdla_opp = 0;
	int dsp_freq = 0, ipu_if_freq = 0, mdla_freq = 0;

	if (enter) {
		/* only read for debug purpose*/
		/*mutex_lock(&opp_mutex);*/
		vmdla_opp = opps.vmdla.index;
		dsp_freq = Map_MDLA_Freq_Table(opps.dsp.index);
		ipu_if_freq = Map_MDLA_Freq_Table(opps.ipu_if.index);
		mdla_freq = Map_MDLA_Freq_Table(opps.mdlacore.index);
		/*mutex_unlock(&opp_mutex);*/
		mdla_met_event_enter(core, vmdla_opp, dsp_freq,
					ipu_if_freq, mdla_freq);
	} else {
		mdla_met_event_leave(core);
	}
}
#endif

static int mdla_set_clock_source(struct clk *clk, uint8_t step)
{
	struct clk *clk_src;

	LOG_DBG("mdla scc(%d)", step);
	/* set dsp frequency - 0:788 MHz, 1:700 MHz, 2:606 MHz, 3:594 MHz*/
	/* set dsp frequency - 4:560 MHz, 5:525 MHz, 6:450 MHz, 7:416 MHz*/
	/* set dsp frequency - 8:364 MHz, 9:312 MHz, 10:273 MH, 11:208 MH*/
	/* set dsp frequency - 12:137 MHz, 13:104 MHz, 14:52 MHz, 15:26 MHz*/

	switch (step) {
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
		LOG_ERR("wrong freq step(%d)", step);
		return -EINVAL;
	}

	return clk_set_parent(clk, clk_src);
}
/*set CONN hf_fdsp_ck, VCORE hf_fipu_if_ck*/
static int mdla_if_set_clock_source(struct clk *clk, uint8_t step)
{
	struct clk *clk_src;

	LOG_DBG("mdla scc(%d)", step);
	/* set dsp frequency - 0:624 MHz, 1:624 MHz, 2:606 MHz, 3:594 MHz*/
	/* set dsp frequency - 4:560 MHz, 5:525 MHz, 6:450 MHz, 7:416 MHz*/
	/* set dsp frequency - 8:364 MHz, 9:312 MHz, 10:273 MH, 11:208 MH*/
	/* set dsp frequency - 12:137 MHz, 13:104 MHz, 14:52 MHz, 15:26 MHz*/
	switch (step) {
	case 0:
		clk_src = clk_top_univpll_d2;/*624MHz*/
		break;
	case 1:
		clk_src = clk_top_univpll_d2;/*624MHz*/
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
		LOG_ERR("wrong freq step(%d)", step);
		return -EINVAL;
	}

	return clk_set_parent(clk, clk_src);
}


int mdla_get_bw(void)
{
	struct qos_bound *bound = get_qos_bound();
	int bw = 0;

	bw = bound->stats[bound->idx].smibw_mon[QOS_SMIBM_MDLA];

	mdla_dvfs_debug("[mdla] cmd bw=%d\n", bw);

	return bw;

}

int mdla_get_lat(void)
{
	struct qos_bound *bound = get_qos_bound();
	int lat = 0;

	lat = bound->stats[bound->idx].lat_mon[QOS_LAT_MDLA];
	mdla_dvfs_debug("[mdla] cmd latency=%d\n", lat);
	return lat;
}

int mdla_get_opp(void)
{
	LOG_DBG("[mdla] mdlacore.index:%d\n", opps.mdlacore.index);
	LOG_DBG("[mdla] opps.dsp.index:%d\n", opps.dsp.index);
	return opps.dsp.index;
}
EXPORT_SYMBOL(mdla_get_opp);

int get_mdlacore_opp(void)
{
	LOG_DBG("[mdla] get opp:%d\n", opps.mdlacore.index);
	return opps.mdlacore.index;
}
EXPORT_SYMBOL(get_mdlacore_opp);

int get_mdla_platform_floor_opp(void)
{
	return (MDLA_MAX_NUM_OPPS - 1);
}
EXPORT_SYMBOL(get_mdla_platform_floor_opp);

int get_mdla_ceiling_opp(void)
{
	return max_opp[0];
}
EXPORT_SYMBOL(get_mdla_ceiling_opp);

int get_mdla_opp_to_freq(uint8_t step)
{
	int freq = 0;
	/* set dsp frequency - 0:788 MHz, 1:700 MHz, 2:606 MHz, 3:594 MHz*/
	/* set dsp frequency - 4:560 MHz, 5:525 MHz, 6:450 MHz, 7:416 MHz*/
	/* set dsp frequency - 8:364 MHz, 9:312 MHz, 10:273 MH, 11:208 MH*/
	/* set dsp frequency - 12:137 MHz, 13:104 MHz, 14:52 MHz, 15:26 MHz*/

	switch (step) {
	case 0:
		freq = 788;
		break;
	case 1:
		freq = 700;
		break;
	case 2:
		freq = 606;
		break;
	case 3:
		freq = 594;
		break;
	case 4:
		freq = 560;
		break;
	case 5:
		freq = 525;
		break;
	case 6:
		freq = 450;
		break;
	case 7:
		freq = 416;
		break;
	case 8:
		freq = 364;
		break;
	case 9:
		freq = 312;
		break;
	case 10:
		freq = 273;
		break;
	case 11:
		freq = 208;
		break;
	case 12:
		freq = 137;
		break;
	case 13:
		freq = 104;
		break;
	case 14:
		freq = 52;
		break;
	case 15:
		freq = 26;
		break;

	default:
		LOG_ERR("wrong freq step(%d)", step);
		return -EINVAL;
	}
	return freq;
}
EXPORT_SYMBOL(get_mdla_opp_to_freq);

static void mdla_dsp_if_freq_check(int core, uint8_t vmdla_index)
{
	uint8_t vpu0_opp;
	uint8_t vpu1_opp;
	uint8_t vvpu_opp;
#ifdef MTK_VPU_SUPPORT
	vpu0_opp = get_vpu_dspcore_opp(0);
	vpu1_opp = get_vpu_dspcore_opp(1);
	if (vpu1_opp < vpu0_opp)
		vvpu_opp = vpu1_opp;
	else
		vvpu_opp = vpu0_opp;
#else
	vpu0_opp = 15;
	vpu1_opp = 15;
	vvpu_opp = 15;
#endif
	mdla_dvfs_debug("[mdla] vpu0_opp %d, vpu1_opp %d, vvpu_opp %d\n",
	vpu0_opp, vpu1_opp, vvpu_opp);
	switch (vmdla_index) {
	case 0:
		opps.dsp.index = 0;
		opps.ipu_if.index = 5;
		break;
	case 1:
		opps.dsp.index = 0;
		opps.ipu_if.index = 5;
		break;
	case 2:
		opps.dsp.index = 0;
		opps.ipu_if.index = 5;
		break;
	case 3:
	case 4:
	case 5:
	case 6:
		opps.dsp.index = 5;
		opps.ipu_if.index = 9;
		if (vvpu_opp <= 5) {
			opps.dsp.index = vvpu_opp;
			//opps.ipu_if.index = vvpu_opp;
		}
		break;
	case 7:
		opps.dsp.index = 6;
		opps.ipu_if.index = 9;
		if (vvpu_opp <= 6) {
			opps.dsp.index = vvpu_opp;
			//opps.ipu_if.index = vvpu_opp;
		}
		break;
	case 8:
		opps.dsp.index = 7;
		opps.ipu_if.index = 9;
		if (vvpu_opp <= 7) {
			opps.dsp.index = vvpu_opp;
			//opps.ipu_if.index = vvpu_opp;
		}
		break;
	case 9:
		opps.dsp.index = 9;
		opps.ipu_if.index = 9;
		if (vvpu_opp <= 9) {
			opps.dsp.index = vvpu_opp;
			//opps.ipu_if.index = vvpu_opp;
		}
		break;
	case 10:
		opps.dsp.index = 10;
		opps.ipu_if.index = 9;
		if (vvpu_opp <= 10) {
			opps.dsp.index = vvpu_opp;
			//opps.ipu_if.index = vvpu_opp;
		}
		break;
	case 11:
		opps.dsp.index = 11;
		opps.ipu_if.index = 9;
		if (vvpu_opp <= 11) {
			opps.dsp.index = vvpu_opp;
			//opps.ipu_if.index = vvpu_opp;
		}
		break;
	case 12:
		opps.dsp.index = 12;
		opps.ipu_if.index = 9;
		if (vvpu_opp <= 12) {
			opps.dsp.index = vvpu_opp;
			//opps.ipu_if.index = vvpu_opp;
		}
		break;
	case 13:
		opps.dsp.index = 13;
		opps.ipu_if.index = 9;
		if (vvpu_opp <= 13) {
			opps.dsp.index = vvpu_opp;
			//opps.ipu_if.index = vvpu_opp;
		}
		break;
	case 14:
		opps.dsp.index = 14;
		opps.ipu_if.index = 9;
		if (vvpu_opp <= 14) {
			opps.dsp.index = vvpu_opp;
			//opps.ipu_if.index = vvpu_opp;
		}
		break;
	case 15:
		opps.dsp.index = 15;
		opps.ipu_if.index = 9;
		if (vvpu_opp <= 15) {
			opps.dsp.index = vvpu_opp;
			//opps.ipu_if.index = vvpu_opp;
		}
		break;
	default:
		LOG_ERR("wrong vmdla_index(%d)", vmdla_index);
	}


}

static void get_segment_from_efuse(void)
{
	int segment = 0;

	segment = get_devinfo_with_index(7) & 0xFF;
	switch (segment) {
	case 0x7://segment p90M 5mode
		segment_max_opp = 0;
		segment_index = SEGMENT_90M;
		break;
	case 0xE0://segment p90M 6mode 525M
		segment_max_opp = 0;
		segment_index = SEGMENT_90M;
		break;
	case 0x20://p95
	case 0x4:
	case 0x60:
	case 0x6:
	case 0x10:
	case 0x8:
	case 0x90:
	case 0x9:
		segment_max_opp = 0;
		segment_index = SEGMENT_95;
		break;
	default: //segment p90
		segment_max_opp = 0;
		segment_index = SEGMENT_90;
		break;
	}
	mdla_dvfs_debug("mdla segment_max_opp %d\n", segment_max_opp);
}

/* expected range, vmdla_index: 0~15 */
/* expected range, freq_index: 0~15 */
void mdla_opp_check(int core, uint8_t vmdla_index, uint8_t freq_index)
{
	int i = 0;
	bool freq_check = false;
	int log_freq = 0, log_max_freq = 0;
	//int get_vcore_opp = 0;
	//int get_vvpu_opp = 0;
	int get_vmdla_opp = 0;

	if (is_power_debug_lock) {
		force_change_vcore_opp[core] = false;
		force_change_vvpu_opp[core] = false;
		force_change_vmdla_opp[core] = false;
		force_change_dsp_freq[core] = false;
		goto out;
	}

	log_freq = Map_MDLA_Freq_Table(freq_index);

	mdla_dvfs_debug("opp_check + (%d/%d/%d), ori vmdla(%d)\n", core,
			vmdla_index, freq_index, opps.vmdla.index);

	mutex_lock(&opp_mutex);
	change_freq_first[core] = false;
	log_max_freq = Map_MDLA_Freq_Table(max_dsp_freq);
	/*segment limitation*/
	if (vmdla_index < opps.vmdla.opp_map[segment_max_opp])
		vmdla_index = opps.vmdla.opp_map[segment_max_opp];
	if (freq_index < segment_max_opp)
		freq_index = segment_max_opp;

	/* vmdla opp*/
	get_vmdla_opp = mdla_get_hw_vmdla_opp(core);

	if (vmdla_index < opps.vmdla.opp_map[max_opp[core]])
		vmdla_index = opps.vmdla.opp_map[max_opp[core]];
	if (vmdla_index > opps.vmdla.opp_map[min_opp[core]])
		vmdla_index = opps.vmdla.opp_map[min_opp[core]];
	if (freq_index < max_opp[core])
		freq_index = max_opp[core];
	if (freq_index > min_opp[core])
		freq_index = min_opp[core];
	mdla_dvfs_debug("opp_check + max_opp%d,min_opp%d,(%d/%d/%d),ori vmdla(%d)",
	max_opp[core], min_opp[core], core,
	vmdla_index, freq_index, opps.vmdla.index);


	if (vmdla_index == 0xFF) {

		mdla_dvfs_debug("no need, vmdla opp(%d), hw vore opp(%d)\n",
			vmdla_index, get_vmdla_opp);

		force_change_vmdla_opp[core] = false;
		opps.vmdla.index = vmdla_index;
	} else {
		/* opp down, need change freq first*/
		if (vmdla_index > get_vmdla_opp)
			change_freq_first[core] = true;

		if (vmdla_index < max_vmdla_opp) {
			mdla_dvfs_debug("mdla bound vmdla opp(%d) to %d",
					vmdla_index, max_vmdla_opp);

			vmdla_index = max_vmdla_opp;
		}

		if (vmdla_index >= opps.count) {
			LOG_ERR("wrong vmdla opp(%d), max(%d)",
					vmdla_index, opps.count - 1);

		} else if ((vmdla_index < opps.vmdla.index) ||
				((vmdla_index > opps.vmdla.index) &&
					(!opp_keep_flag))) {
			opps.vmdla.index = vmdla_index;
			force_change_vmdla_opp[core] = true;
			freq_check = true;
		}
	}

	/* dsp freq opp */
	if (freq_index == 0xFF) {
		mdla_dvfs_debug("no request, freq opp(%d)", freq_index);
		force_change_dsp_freq[core] = false;
	} else {
		if (freq_index < max_dsp_freq) {
			mdla_dvfs_debug("mdla bound dsp freq(%dMHz) to %dMHz",
					log_freq, log_max_freq);
			freq_index = max_dsp_freq;
		}

		if ((opps.mdlacore.index != freq_index) || (freq_check)) {
			/* freq_check for all vcore adjust related operation
			 * in acceptable region
			 */

			/* vcore not change and dsp change */
			//if ((force_change_vcore_opp[core] == false) &&
			if ((force_change_vmdla_opp[core] == false) &&
				(freq_index > opps.mdlacore.index) &&
				(opp_keep_flag)) {
				force_change_dsp_freq[core] = false;
					mdla_dvfs_debug("%s(%d) %s (%d/%d_%d/%d)\n",
						__func__,
						core,
						"dsp keep high",
						force_change_vmdla_opp[core],
						freq_index,
						opps.mdlacore.index,
						opp_keep_flag);

			} else {
				opps.mdlacore.index = freq_index;
					/*To FIX*/
				if (opps.vmdla.index == 1 &&
						opps.mdlacore.index < 3) {
					/* adjust 0~3 to 4~7 for real table
					 * if needed
					 */
					opps.mdlacore.index = 3;
				}
				if (opps.vmdla.index == 2 &&
						opps.mdlacore.index < 9) {
					/* adjust 0~3 to 4~7 for real table
					 * if needed
					 */
					opps.mdlacore.index = 9;
				}


				opps.dsp.index = 15;
				opps.ipu_if.index = 15;
				for (i = 0 ; i < MTK_MDLA_CORE ; i++) {
					mdla_dvfs_debug("%s %s[%d].%s(%d->%d)\n",
						__func__,
						"opps.mdlacore",
						core,
						"index",
						opps.mdlacore.index,
						opps.dsp.index);

					/* interface should be the max freq of
					 * mdla cores
					 */
					if ((opps.mdlacore.index <
						opps.dsp.index) &&
						(opps.mdlacore.index >=
						max_dsp_freq)) {
						mdla_dsp_if_freq_check(core,
							opps.mdlacore.index);

					}
				}
				force_change_dsp_freq[core] = true;

				opp_keep_flag = true;

				mod_delayed_work(wq, &opp_keep_work,
					msecs_to_jiffies(OPP_KEEP_TIME_MS));

			}
		} else {
			/* vcore not change & dsp not change */
			mdla_dvfs_debug("opp_check(%d) vcore/dsp no change\n",
						core);

			opp_keep_flag = true;


			mod_delayed_work(wq, &opp_keep_work,
					msecs_to_jiffies(OPP_KEEP_TIME_MS));
		}
	}
	mutex_unlock(&opp_mutex);
out:
	mdla_dvfs_debug("%s(%d)(%d/%d_%d)(%d/%d)(%d.%d.%d)(%d/%d)(%d/%d/%d/%d)%d\n",
		"opp_check",
		core,
		is_power_debug_lock,
		vmdla_index,
		freq_index,
		opps.vmdla.index,
		get_vmdla_opp,
		opps.dsp.index,
		opps.mdlacore.index,
		opps.ipu_if.index,
		max_vmdla_opp,
		max_dsp_freq,
		freq_check,
		force_change_vmdla_opp[core],
		force_change_dsp_freq[core],
		change_freq_first[core],
		opp_keep_flag);
}
EXPORT_SYMBOL(mdla_opp_check);

static bool mdla_change_opp(int core, int type)
{
#ifdef MTK_MDLA_FPGA_PORTING
	mdla_dvfs_debug("[mdla_%d] %d Skip at FPGA", core, type);

	return true;
#else
	int ret = 0;


	switch (type) {
	/* vcore opp */
	case OPPTYPE_VCORE:
		mdla_dvfs_debug("[mdla_%d] wait for changing vcore opp", core);
		break;
	/* dsp freq opp */
	case OPPTYPE_DSPFREQ:
		mutex_lock(&opp_mutex);
		mdla_dvfs_debug("[mdla] %s setclksrc(%d/%d/%d)\n",
				__func__,
				opps.dsp.index,
				opps.mdlacore.index,
				opps.ipu_if.index);

		ret = mdla_if_set_clock_source(clk_top_dsp_sel, opps.dsp.index);
		if (ret) {
			LOG_ERR("[mdla]fail to set dsp freq, %s=%d, %s=%d\n",
				"step", opps.dsp.index,
				"ret", ret);
			goto out;
		}
		ret = mdla_if_set_clock_source(clk_top_ipu_if_sel,
							opps.ipu_if.index);
		if (ret) {
			LOG_ERR("[mdla]%s, %s=%d, %s=%d\n",
					"fail to set ipu_if freq",
					"step", opps.ipu_if.index,
					"ret", ret);
			goto out;
		}
			ret = mdla_set_clock_source(clk_top_dsp3_sel,
						opps.mdlacore.index);
			if (ret) {
				LOG_ERR("[mdla]%s%d freq, %s=%d, %s=%d\n",
					"fail to set dsp_", opps.mdlacore.index,
					"step", opps.dsp.index,
					"ret", ret);
				goto out;
			}



		force_change_dsp_freq[core] = false;
		mutex_unlock(&opp_mutex);

#ifdef MTK_PERF_OBSERVER
		{
			struct pob_xpufreq_info pxi;

			pxi.id = core;
			pxi.opp = opps.mdlacore.index;

			pob_xpufreq_update(POB_XPUFREQ_MDLA, &pxi);
		}
#endif

		break;
	/* vmdla opp */
	case OPPTYPE_VMDLA:
		mdla_dvfs_debug("[mdla_%d] wait for changing vmdla opp", core);
		LOG_DBG("[mdla_%d] to do vmdla opp change", core);
		mutex_lock(&opp_mutex);
		mdla_trace_tag_begin("vcore:request");
		#ifdef ENABLE_PMQOS
		switch (opps.vmdla.index) {
		case 0:
			mtk_pm_qos_update_request(&mdla_qos_vcore_request[core],
								VCORE_OPP_1);
			mtk_pm_qos_update_request(&mdla_qos_vvpu_request[core],
								VVPU_OPP_0);
			mtk_pm_qos_update_request(&mdla_qos_vmdla_request[core],
								VMDLA_OPP_0);
			break;
		case 1:
			mtk_pm_qos_update_request(&mdla_qos_vvpu_request[core],
								VVPU_OPP_1);
			mtk_pm_qos_update_request(&mdla_qos_vmdla_request[core],
								VMDLA_OPP_1);
			mtk_pm_qos_update_request(&mdla_qos_vcore_request[core],
								VCORE_OPP_2);
			break;
		case 2:
		default:
			mtk_pm_qos_update_request(&mdla_qos_vmdla_request[core],
								VMDLA_OPP_2);
			mtk_pm_qos_update_request(&mdla_qos_vvpu_request[core],
								VVPU_OPP_2);
			mtk_pm_qos_update_request(&mdla_qos_vcore_request[core],
								VCORE_OPP_2);
			break;
		}
		#else
		ret = mmdvfs_set_fine_step(MMDVFS_SCEN_VPU_KERNEL,
							opps.vcore.index);
		#endif
		mdla_trace_tag_end();
		if (ret) {
			LOG_ERR("[mdla_%d]fail to request vcore, step=%d\n",
					core, opps.vcore.index);
			goto out;
		}

		mdla_dvfs_debug("[mdla_%d] cgopp vmdla=%d\n",
				core,
				regulator_get_voltage(vmdla_reg_id));

		force_change_vcore_opp[core] = false;
		force_change_vvpu_opp[core] = false;
		force_change_vmdla_opp[core] = false;
		mutex_unlock(&opp_mutex);
		//wake_up_interruptible(&waitq_do_core_executing);
		break;
	default:
		mdla_dvfs_debug("unexpected type(%d)", type);
		break;
	}

out:
	return true;
#endif
}

int32_t mdla_thermal_en_throttle_cb(uint8_t vcore_opp, uint8_t mdla_opp)
{
	int i = 0;
	int ret = 0;
	int vmdla_opp_index = 0;
	int mdla_freq_index = 0;

	if (mdla_init_done != 1)
		return ret;

	if (mdla_opp < MDLA_MAX_NUM_OPPS) {
		vmdla_opp_index = opps.vmdla.opp_map[mdla_opp];
		mdla_freq_index = opps.dsp.opp_map[mdla_opp];
	} else {
		LOG_ERR("mdla_thermal_en wrong opp(%d)\n", mdla_opp);
		return -1;
	}

	mdla_dvfs_debug("%s, opp(%d)->(%d/%d)\n",
	__func__, mdla_opp, vmdla_opp_index, mdla_freq_index);

	mutex_lock(&opp_mutex);
	max_dsp_freq = mdla_freq_index;
	max_vmdla_opp = vmdla_opp_index;
	mutex_unlock(&opp_mutex);
	for (i = 0 ; i < MTK_MDLA_USER ; i++) {
		mutex_lock(&opp_mutex);

		/* force change for all core under thermal request */
		opp_keep_flag = false;

		mutex_unlock(&opp_mutex);
		mdla_opp_check(i, vmdla_opp_index, mdla_freq_index);
	}

	for (i = 0 ; i < MTK_MDLA_USER ; i++) {
		if (force_change_dsp_freq[i]) {
			/* force change freq while running */
			switch (mdla_freq_index) {
			case 0:
			default:
				LOG_INF("thermal force bound freq @788MHz\n");
				break;
			case 1:
				LOG_INF("thermal force bound freq @ 700MHz\n");
				break;
			case 2:
				LOG_INF("thermal force bound freq @624MHz\n");
				break;
			case 3:
				LOG_INF("thermal force bound freq @606MHz\n");
				break;
			case 4:
				LOG_INF("thermal force bound freq @594MHz\n");
				break;
			case 5:
				LOG_INF("thermal force bound freq @546MHz\n");
				break;
			case 6:
				LOG_INF("thermal force bound freq @525MHz\n");
				break;
			case 7:
				LOG_INF("thermal force bound freq @450MHz\n");
				break;
			case 8:
				LOG_INF("thermal force bound freq @416MHz\n");
				break;
			case 9:
				LOG_INF("thermal force bound freq @364MHz\n");
				break;
			case 10:
				LOG_INF("thermal force bound freq @312MHz\n");
				break;
			case 11:
				LOG_INF("thermal force bound freq @273MHz\n");
				break;
			case 12:
				LOG_INF("thermal force bound freq @208MHz\n");
				break;
			case 13:
				LOG_INF("thermal force bound freq @137MHz\n");
				break;
			case 14:
				LOG_INF("thermal force bound freq @52MHz\n");
				break;
			case 15:
				LOG_INF("thermal force bound freq @26MHz\n");
				break;
			}
			/*mdla_change_opp(i, OPPTYPE_DSPFREQ);*/
		}
		if (force_change_vmdla_opp[i]) {
			/* vcore change should wait */
			LOG_INF("thermal force bound vcore opp to %d\n",
					vmdla_opp_index);
			/* vcore only need to change one time from
			 * thermal request
			 */
			/*if (i == 0)*/
			/*	mdla_change_opp(i, OPPTYPE_VCORE);*/
		}
	}

	return ret;
}

int32_t mdla_thermal_dis_throttle_cb(void)
{
	int ret = 0;

	if (mdla_init_done != 1)
		return ret;

	LOG_INF("%s +\n", __func__);
	mutex_lock(&opp_mutex);
	max_vcore_opp = 0;
	max_vmdla_opp = 0;
	max_dsp_freq = 0;
	mutex_unlock(&opp_mutex);
	LOG_INF("%s -\n", __func__);

	return ret;
}

static int mdla_prepare_regulator_and_clock(struct device *pdev)
{
	int ret = 0;
/*enable Vmdla Vmdla*/
/*--Get regulator handle--*/
	vvpu_reg_id = regulator_get(pdev, "vpu");
	if (!vvpu_reg_id) {
		ret = -ENOENT;
	    LOG_ERR("regulator_get vvpu_reg_id failed\n");
	}
	vmdla_reg_id = regulator_get(pdev, "VMDLA");
	if (!vmdla_reg_id) {
		ret = -ENOENT;
		LOG_ERR("regulator_get vmdla_reg_id failed\n");
	}


#ifdef MTK_MDLA_FPGA_PORTING
	LOG_INF("%s skip at FPGA\n", __func__);
#else
#define PREPARE_MDLA_MTCMOS(clk) \
	{ \
		clk = devm_clk_get(pdev, #clk); \
		if (IS_ERR(clk)) { \
			ret = -ENOENT; \
			LOG_ERR("can not find mtcmos: %s\n", #clk); \
		} \
	}
	PREPARE_MDLA_MTCMOS(mtcmos_dis);
	PREPARE_MDLA_MTCMOS(mtcmos_vpu_vcore_shutdown);
	PREPARE_MDLA_MTCMOS(mtcmos_vpu_conn_shutdown);
	PREPARE_MDLA_MTCMOS(mtcmos_vpu_core2_shutdown);

#undef PREPARE_MDLA_MTCMOS

#define PREPARE_MDLA_CLK(clk) \
	{ \
		clk = devm_clk_get(pdev, #clk); \
		if (IS_ERR(clk)) { \
			ret = -ENOENT; \
			LOG_ERR("can not find clock: %s\n", #clk); \
		} else if (clk_prepare(clk)) { \
			ret = -EBADE; \
			LOG_ERR("fail to prepare clock: %s\n", #clk); \
		} \
	}

	PREPARE_MDLA_CLK(clk_mmsys_gals_ipu2mm);
	PREPARE_MDLA_CLK(clk_mmsys_gals_ipu12mm);
	PREPARE_MDLA_CLK(clk_mmsys_gals_comm0);
	PREPARE_MDLA_CLK(clk_mmsys_gals_comm1);
	PREPARE_MDLA_CLK(clk_mmsys_smi_common);
	PREPARE_MDLA_CLK(clk_apu_vcore_ahb_cg);
	PREPARE_MDLA_CLK(clk_apu_vcore_axi_cg);
	PREPARE_MDLA_CLK(clk_apu_vcore_adl_cg);
	PREPARE_MDLA_CLK(clk_apu_vcore_qos_cg);
	PREPARE_MDLA_CLK(clk_apu_conn_apu_cg);
	PREPARE_MDLA_CLK(clk_apu_conn_ahb_cg);
	PREPARE_MDLA_CLK(clk_apu_conn_axi_cg);
	PREPARE_MDLA_CLK(clk_apu_conn_isp_cg);
	PREPARE_MDLA_CLK(clk_apu_conn_cam_adl_cg);
	PREPARE_MDLA_CLK(clk_apu_conn_img_adl_cg);
	PREPARE_MDLA_CLK(clk_apu_conn_emi_26m_cg);
	PREPARE_MDLA_CLK(clk_apu_conn_vpu_udi_cg);
	PREPARE_MDLA_CLK(clk_apu_mdla_apb_cg);
	PREPARE_MDLA_CLK(clk_apu_mdla_cg_b0);
	PREPARE_MDLA_CLK(clk_apu_mdla_cg_b1);
	PREPARE_MDLA_CLK(clk_apu_mdla_cg_b2);
	PREPARE_MDLA_CLK(clk_apu_mdla_cg_b3);
	PREPARE_MDLA_CLK(clk_apu_mdla_cg_b4);
	PREPARE_MDLA_CLK(clk_apu_mdla_cg_b5);
	PREPARE_MDLA_CLK(clk_apu_mdla_cg_b6);
	PREPARE_MDLA_CLK(clk_apu_mdla_cg_b7);
	PREPARE_MDLA_CLK(clk_apu_mdla_cg_b8);
	PREPARE_MDLA_CLK(clk_apu_mdla_cg_b9);
	PREPARE_MDLA_CLK(clk_apu_mdla_cg_b10);
	PREPARE_MDLA_CLK(clk_apu_mdla_cg_b11);
	PREPARE_MDLA_CLK(clk_apu_mdla_cg_b12);
	PREPARE_MDLA_CLK(clk_top_dsp_sel);
	PREPARE_MDLA_CLK(clk_top_dsp3_sel);
	PREPARE_MDLA_CLK(clk_top_ipu_if_sel);
	PREPARE_MDLA_CLK(clk_top_clk26m);
	PREPARE_MDLA_CLK(clk_top_univpll_d3_d8);
	PREPARE_MDLA_CLK(clk_top_univpll_d3_d4);
	PREPARE_MDLA_CLK(clk_top_mainpll_d2_d4);
	PREPARE_MDLA_CLK(clk_top_univpll_d3_d2);
	PREPARE_MDLA_CLK(clk_top_mainpll_d2_d2);
	PREPARE_MDLA_CLK(clk_top_univpll_d2_d2);
	PREPARE_MDLA_CLK(clk_top_mainpll_d3);
	PREPARE_MDLA_CLK(clk_top_univpll_d3);
	PREPARE_MDLA_CLK(clk_top_mmpll_d7);
	PREPARE_MDLA_CLK(clk_top_mmpll_d6);
	PREPARE_MDLA_CLK(clk_top_adsppll_d5);
	PREPARE_MDLA_CLK(clk_top_tvdpll_ck);
	PREPARE_MDLA_CLK(clk_top_tvdpll_mainpll_d2_ck);
	PREPARE_MDLA_CLK(clk_top_univpll_d2);
	PREPARE_MDLA_CLK(clk_top_adsppll_d4);
	PREPARE_MDLA_CLK(clk_top_mainpll_d2);
	PREPARE_MDLA_CLK(clk_top_mmpll_d4);

#undef PREPARE_MDLA_CLK

#endif
	return ret;
}
static int mdla_enable_regulator_and_clock(int core)
{
#ifdef MTK_MDLA_FPGA_PORTING
	LOG_INF("%s skip at FPGA\n", __func__);

	is_power_on[core] = true;
	force_change_vcore_opp[core] = false;
	force_change_vmdla_opp[core] = false;
	force_change_dsp_freq[core] = false;
	return 0;
#else
	int ret = 0;
	int ret1 = 0;
	//int get_vcore_opp = 0;
	int get_vmdla_opp = 0;
	//bool adjust_vcore = false;
	bool adjust_vmdla = false;

	mdla_dvfs_debug("[mdla] bypass setvoltage\n");
	mdla_dvfs_debug("[mdla_%d] en_rc + (%d)\n", core, is_power_debug_lock);

	mdla_trace_tag_begin("%s");

	/*--enable regulator--*/
	ret1 = vvpu_regulator_set_mode(true);
	udelay(100);//slew rate:rising10mV/us
	mdla_dvfs_debug("enable vvpu ret:%d\n", ret1);
	ret1 = vmdla_regulator_set_mode(true);
	udelay(100);//slew rate:rising10mV/us
	mdla_dvfs_debug("enable vmdla ret:%d\n", ret1);
	vvpu_vmdla_vcore_checker();
	get_vmdla_opp = mdla_get_hw_vmdla_opp(core);
	adjust_vmdla = true;

	mdla_trace_tag_begin("vcore:request");
	//if (adjust_vcore) {
	if (adjust_vmdla) {
		mdla_dvfs_debug("[mdla_%d] adjust_vmdla", core);
		LOG_DBG("[mdla_%d] en_rc to do vmdla opp change", core);
#ifdef ENABLE_PMQOS
		switch (opps.vmdla.index) {
		case 0:
			mtk_pm_qos_update_request(&mdla_qos_vcore_request[core],
								VCORE_OPP_1);
			mtk_pm_qos_update_request(&mdla_qos_vvpu_request[core],
								VVPU_OPP_0);
			mtk_pm_qos_update_request(&mdla_qos_vmdla_request[core],
								VMDLA_OPP_0);
			break;
		case 1:
			mtk_pm_qos_update_request(&mdla_qos_vvpu_request[core],
								VVPU_OPP_1);
			mtk_pm_qos_update_request(&mdla_qos_vmdla_request[core],
								VMDLA_OPP_1);
			mtk_pm_qos_update_request(&mdla_qos_vcore_request[core],
								VCORE_OPP_2);
				break;
		case 2:
		default:
			mtk_pm_qos_update_request(&mdla_qos_vmdla_request[core],
								VMDLA_OPP_2);
			mtk_pm_qos_update_request(&mdla_qos_vvpu_request[core],
								VVPU_OPP_2);
			mtk_pm_qos_update_request(&mdla_qos_vcore_request[core],
								VCORE_OPP_2);
			break;
		}
#else
		ret = mmdvfs_set_fine_step(MMDVFS_SCEN_VPU_KERNEL,
							opps.vcore.index);
#endif
	}
	mdla_trace_tag_end();
	if (ret) {
		LOG_ERR("[mdla_%d]fail to request vcore, step=%d\n",
				core, opps.vcore.index);
		goto out;
	}
	mdla_dvfs_debug("[mdla_%d] adjust(%d,%d) result vmdla=%d\n",
			core,
			adjust_vmdla,
			opps.vmdla.index,
			regulator_get_voltage(vmdla_reg_id));

	LOG_DBG("[mdla_%d] en_rc setmmdvfs(%d) done\n", core, opps.vcore.index);


mdla_dvfs_debug("[mdla_%d] adjust(%d,%d) result vmdla=%d\n",
		core,
		adjust_vmdla,
		opps.vmdla.index,
		regulator_get_voltage(vmdla_reg_id));

#define ENABLE_MDLA_MTCMOS(clk) \
{ \
	if (clk != NULL) { \
		if (clk_prepare_enable(clk)) \
			LOG_ERR("fail to prepare&enable mtcmos:%s\n", #clk); \
	} else { \
		LOG_WRN("mtcmos not existed: %s\n", #clk); \
	} \
}

#define ENABLE_MDLA_CLK(clk) \
	{ \
		if (clk != NULL) { \
			if (clk_enable(clk)) \
				LOG_ERR("fail to enable clock: %s\n", #clk); \
		} else { \
			LOG_WRN("clk not existed: %s\n", #clk); \
		} \
	}
	/*move vcore cg ctl to atf*/
#define vcore_cg_ctl(poweron) \
		atf_vcore_cg_ctl(poweron)

	mdla_trace_tag_begin("clock:enable_source");
	ENABLE_MDLA_CLK(clk_top_dsp_sel);
	ENABLE_MDLA_CLK(clk_top_ipu_if_sel);
	ENABLE_MDLA_CLK(clk_top_dsp3_sel);
	mdla_trace_tag_end();

	mdla_trace_tag_begin("mtcmos:enable");
	ENABLE_MDLA_MTCMOS(mtcmos_dis);
	ENABLE_MDLA_MTCMOS(mtcmos_vpu_vcore_shutdown);
	ENABLE_MDLA_MTCMOS(mtcmos_vpu_conn_shutdown);
	ENABLE_MDLA_MTCMOS(mtcmos_vpu_core2_shutdown);

	mdla_trace_tag_end();
	udelay(500);

	mdla_trace_tag_begin("clock:enable");
	ENABLE_MDLA_CLK(clk_mmsys_gals_ipu2mm);
	ENABLE_MDLA_CLK(clk_mmsys_gals_ipu12mm);
	ENABLE_MDLA_CLK(clk_mmsys_gals_comm0);
	ENABLE_MDLA_CLK(clk_mmsys_gals_comm1);
	ENABLE_MDLA_CLK(clk_mmsys_smi_common);


	ENABLE_MDLA_CLK(clk_apu_vcore_ahb_cg);
	ENABLE_MDLA_CLK(clk_apu_vcore_axi_cg);
	ENABLE_MDLA_CLK(clk_apu_vcore_adl_cg);
	ENABLE_MDLA_CLK(clk_apu_vcore_qos_cg);
	/*move vcore cg ctl to atf*/
	vcore_cg_ctl(1);
	ENABLE_MDLA_CLK(clk_apu_conn_apu_cg);
	ENABLE_MDLA_CLK(clk_apu_conn_ahb_cg);
	ENABLE_MDLA_CLK(clk_apu_conn_axi_cg);
	ENABLE_MDLA_CLK(clk_apu_conn_isp_cg);
	ENABLE_MDLA_CLK(clk_apu_conn_cam_adl_cg);
	ENABLE_MDLA_CLK(clk_apu_conn_img_adl_cg);
	ENABLE_MDLA_CLK(clk_apu_conn_emi_26m_cg);
	ENABLE_MDLA_CLK(clk_apu_conn_vpu_udi_cg);
	switch (core) {
	case 0:
	default:
		ENABLE_MDLA_CLK(clk_apu_mdla_apb_cg);
		ENABLE_MDLA_CLK(clk_apu_mdla_cg_b0);
		ENABLE_MDLA_CLK(clk_apu_mdla_cg_b1);
		ENABLE_MDLA_CLK(clk_apu_mdla_cg_b2);
		ENABLE_MDLA_CLK(clk_apu_mdla_cg_b3);
		ENABLE_MDLA_CLK(clk_apu_mdla_cg_b4);
		ENABLE_MDLA_CLK(clk_apu_mdla_cg_b5);
		ENABLE_MDLA_CLK(clk_apu_mdla_cg_b6);
		ENABLE_MDLA_CLK(clk_apu_mdla_cg_b7);
		ENABLE_MDLA_CLK(clk_apu_mdla_cg_b8);
		ENABLE_MDLA_CLK(clk_apu_mdla_cg_b9);
		ENABLE_MDLA_CLK(clk_apu_mdla_cg_b10);
		ENABLE_MDLA_CLK(clk_apu_mdla_cg_b11);
		ENABLE_MDLA_CLK(clk_apu_mdla_cg_b12);
		break;
	}
	mdla_trace_tag_end();

#undef ENABLE_MDLA_MTCMOS
#undef ENABLE_MDLA_CLK

	mdla_dvfs_debug("[mdla_%d] en_rc setclksrc(%d/%d/%d)\n",
			core,
			opps.dsp.index,
			opps.mdlacore.index,
			opps.ipu_if.index);

	ret = mdla_if_set_clock_source(clk_top_dsp_sel, opps.dsp.index);
	if (ret) {
		LOG_ERR("[mdla_%d]fail to set dsp freq, step=%d, ret=%d\n",
				core, opps.dsp.index, ret);
		goto out;
	}

	ret = mdla_set_clock_source(clk_top_dsp3_sel, opps.mdlacore.index);
	if (ret) {
		LOG_ERR("[mdla_%d]fail to set mdla freq, step=%d, ret=%d\n",
				core, opps.mdlacore.index, ret);
		goto out;
	}

	ret = mdla_if_set_clock_source(clk_top_ipu_if_sel, opps.ipu_if.index);
	if (ret) {
		LOG_ERR("[mdla_%d]fail to set ipu_if freq, step=%d, ret=%d\n",
				core, opps.ipu_if.index, ret);
		goto out;
	}

out:
	mdla_trace_tag_end();
	if (mdla_klog & MDLA_DBG_DVFS)
		apu_get_power_info();
	is_power_on[core] = true;
	force_change_vcore_opp[core] = false;
	force_change_vmdla_opp[core] = false;
	force_change_dsp_freq[core] = false;
	LOG_DBG("[mdla_%d] en_rc -\n", core);
	return ret;
#endif
}

static int mdla_disable_regulator_and_clock(int core)
{
	int ret = 0;
	int ret1 = 0;

#ifdef MTK_MDLA_FPGA_PORTING
	mdla_dvfs_debug("%s skip at FPGA\n", __func__);

	is_power_on[core] = false;
	if (!is_power_debug_lock)
		opps.mdlacore.index = 7;

	return ret;
#else
	unsigned int smi_bus_vpu_value = 0x0;

	/* check there is un-finished transaction in bus before
	 * turning off vpu power
	 */
#ifdef MTK_VPU_SMI_DEBUG_ON
	smi_bus_vpu_value = vpu_read_smi_bus_debug(core);

	mdla_dvfs_debug("[vpu_%d] dis_rc 1 (0x%x)\n", core, smi_bus_vpu_value);

	if ((int)smi_bus_vpu_value != 0) {
		mdelay(1);
		smi_bus_vpu_value = vpu_read_smi_bus_debug(core);

		mdla_dvfs_debug("[vpu_%d] dis_rc again (0x%x)\n", core,
				smi_bus_vpu_value);

		if ((int)smi_bus_vpu_value != 0) {
			smi_debug_bus_hanging_detect_ext2(0x1ff, 1, 0, 1);
			vpu_aee_warn("VPU SMI CHECK",
				"core_%d fail to check smi, value=%d\n",
				core,
				smi_bus_vpu_value);
		}
	}
#else
	mdla_dvfs_debug("[mdla_%d] dis_rc + (0x%x)\n", core, smi_bus_vpu_value);
#endif

#define DISABLE_MDLA_CLK(clk) \
	{ \
		if (clk != NULL) { \
			clk_disable(clk); \
		} else { \
			LOG_WRN("clk not existed: %s\n", #clk); \
		} \
	}
	switch (core) {
	case 0:
	default:
		DISABLE_MDLA_CLK(clk_apu_mdla_apb_cg);
		DISABLE_MDLA_CLK(clk_apu_mdla_cg_b0);
		DISABLE_MDLA_CLK(clk_apu_mdla_cg_b1);
		DISABLE_MDLA_CLK(clk_apu_mdla_cg_b2);
		DISABLE_MDLA_CLK(clk_apu_mdla_cg_b3);
		DISABLE_MDLA_CLK(clk_apu_mdla_cg_b4);
		DISABLE_MDLA_CLK(clk_apu_mdla_cg_b5);
		DISABLE_MDLA_CLK(clk_apu_mdla_cg_b6);
		DISABLE_MDLA_CLK(clk_apu_mdla_cg_b7);
		DISABLE_MDLA_CLK(clk_apu_mdla_cg_b8);
		DISABLE_MDLA_CLK(clk_apu_mdla_cg_b9);
		DISABLE_MDLA_CLK(clk_apu_mdla_cg_b10);
		DISABLE_MDLA_CLK(clk_apu_mdla_cg_b11);
		DISABLE_MDLA_CLK(clk_apu_mdla_cg_b12);
		break;
	}
	DISABLE_MDLA_CLK(clk_apu_conn_apu_cg);
	DISABLE_MDLA_CLK(clk_apu_conn_ahb_cg);
	DISABLE_MDLA_CLK(clk_apu_conn_axi_cg);
	DISABLE_MDLA_CLK(clk_apu_conn_isp_cg);
	DISABLE_MDLA_CLK(clk_apu_conn_cam_adl_cg);
	DISABLE_MDLA_CLK(clk_apu_conn_img_adl_cg);
	DISABLE_MDLA_CLK(clk_apu_conn_emi_26m_cg);
	DISABLE_MDLA_CLK(clk_apu_conn_vpu_udi_cg);
	DISABLE_MDLA_CLK(clk_apu_vcore_ahb_cg);
	DISABLE_MDLA_CLK(clk_apu_vcore_axi_cg);
	DISABLE_MDLA_CLK(clk_apu_vcore_adl_cg);
	DISABLE_MDLA_CLK(clk_apu_vcore_qos_cg);
	DISABLE_MDLA_CLK(clk_mmsys_gals_ipu2mm);
	DISABLE_MDLA_CLK(clk_mmsys_gals_ipu12mm);
	DISABLE_MDLA_CLK(clk_mmsys_gals_comm0);
	DISABLE_MDLA_CLK(clk_mmsys_gals_comm1);
	DISABLE_MDLA_CLK(clk_mmsys_smi_common);
	mdla_dvfs_debug("[mdla_%d] dis_rc flag4\n", core);

#define DISABLE_MDLA_MTCMOS(clk) \
	{ \
		if (clk != NULL) { \
			clk_disable_unprepare(clk); \
		} else { \
			LOG_WRN("mtcmos not existed: %s\n", #clk); \
		} \
	}
	DISABLE_MDLA_MTCMOS(mtcmos_vpu_core2_shutdown);
	DISABLE_MDLA_MTCMOS(mtcmos_vpu_conn_shutdown);
	DISABLE_MDLA_MTCMOS(mtcmos_vpu_vcore_shutdown);
	DISABLE_MDLA_MTCMOS(mtcmos_dis);


	DISABLE_MDLA_CLK(clk_top_dsp_sel);
	DISABLE_MDLA_CLK(clk_top_ipu_if_sel);
	DISABLE_MDLA_CLK(clk_top_dsp3_sel);

#undef DISABLE_MDLA_MTCMOS
#undef DISABLE_MDLA_CLK
#ifdef ENABLE_PMQOS
	mtk_pm_qos_update_request(&mdla_qos_vmdla_request[core], VMDLA_OPP_2);
	mtk_pm_qos_update_request(&mdla_qos_vvpu_request[core], VVPU_OPP_2);
	mtk_pm_qos_update_request(&mdla_qos_vcore_request[core],
						VCORE_OPP_UNREQ);
	mdla_dvfs_debug("[mdla_%d]vvpu, vmdla unreq\n", core);
#else
	ret = mmdvfs_set_fine_step(MMDVFS_SCEN_VPU_KERNEL,
						MMDVFS_FINE_STEP_UNREQUEST);
#endif
	if (ret) {
		LOG_ERR("[mdla_%d]fail to unrequest vcore!\n", core);
		goto out;
	}
	LOG_DBG("[mdla_%d] disable result vmdla=%d\n",
		core, regulator_get_voltage(vmdla_reg_id));
out:

	/*--disable regulator--*/
	ret1 = vmdla_regulator_set_mode(false);
	udelay(100);//slew rate:rising10mV/us
	mdla_dvfs_debug("disable vmdla ret:%d\n", ret1);
	ret1 = vvpu_regulator_set_mode(false);
	udelay(100);//slew rate:rising10mV/us
	mdla_dvfs_debug("disable vvpu ret:%d\n", ret1);

	vvpu_vmdla_vcore_checker();

	is_power_on[core] = false;
	if (!is_power_debug_lock) {
		opps.mdlacore.index = 15;
		opps.dsp.index = 9;
		opps.ipu_if.index = 9;
		}
	mdla_dvfs_debug("[mdla_%d] dis_rc -\n", core);
	return ret;
#endif
}

static void mdla_unprepare_regulator_and_clock(void)
{

#ifdef MTK_MDLA_FPGA_PORTING
	mdla_dvfs_debug("%s skip at FPGA\n", __func__);
#else
#define UNPREPARE_MDLA_CLK(clk) \
	{ \
		if (clk != NULL) { \
			clk_unprepare(clk); \
			clk = NULL; \
		} \
	}
	UNPREPARE_MDLA_CLK(clk_apu_mdla_apb_cg);
	UNPREPARE_MDLA_CLK(clk_apu_mdla_cg_b0);
	UNPREPARE_MDLA_CLK(clk_apu_mdla_cg_b1);
	UNPREPARE_MDLA_CLK(clk_apu_mdla_cg_b2);
	UNPREPARE_MDLA_CLK(clk_apu_mdla_cg_b3);
	UNPREPARE_MDLA_CLK(clk_apu_mdla_cg_b4);
	UNPREPARE_MDLA_CLK(clk_apu_mdla_cg_b5);
	UNPREPARE_MDLA_CLK(clk_apu_mdla_cg_b6);
	UNPREPARE_MDLA_CLK(clk_apu_mdla_cg_b7);
	UNPREPARE_MDLA_CLK(clk_apu_mdla_cg_b8);
	UNPREPARE_MDLA_CLK(clk_apu_mdla_cg_b9);
	UNPREPARE_MDLA_CLK(clk_apu_mdla_cg_b10);
	UNPREPARE_MDLA_CLK(clk_apu_mdla_cg_b11);
	UNPREPARE_MDLA_CLK(clk_apu_mdla_cg_b12);
	UNPREPARE_MDLA_CLK(clk_apu_conn_apu_cg);
	UNPREPARE_MDLA_CLK(clk_apu_conn_ahb_cg);
	UNPREPARE_MDLA_CLK(clk_apu_conn_axi_cg);
	UNPREPARE_MDLA_CLK(clk_apu_conn_isp_cg);
	UNPREPARE_MDLA_CLK(clk_apu_conn_cam_adl_cg);
	UNPREPARE_MDLA_CLK(clk_apu_conn_img_adl_cg);
	UNPREPARE_MDLA_CLK(clk_apu_conn_emi_26m_cg);
	UNPREPARE_MDLA_CLK(clk_apu_conn_vpu_udi_cg);
	UNPREPARE_MDLA_CLK(clk_apu_vcore_ahb_cg);
	UNPREPARE_MDLA_CLK(clk_apu_vcore_axi_cg);
	UNPREPARE_MDLA_CLK(clk_apu_vcore_adl_cg);
	UNPREPARE_MDLA_CLK(clk_apu_vcore_qos_cg);
	UNPREPARE_MDLA_CLK(clk_mmsys_gals_ipu2mm);
	UNPREPARE_MDLA_CLK(clk_mmsys_gals_ipu12mm);
	UNPREPARE_MDLA_CLK(clk_mmsys_gals_comm0);
	UNPREPARE_MDLA_CLK(clk_mmsys_gals_comm1);
	UNPREPARE_MDLA_CLK(clk_mmsys_smi_common);
	UNPREPARE_MDLA_CLK(clk_top_dsp_sel);
	UNPREPARE_MDLA_CLK(clk_top_dsp3_sel);
	UNPREPARE_MDLA_CLK(clk_top_ipu_if_sel);
	UNPREPARE_MDLA_CLK(clk_top_clk26m);
	UNPREPARE_MDLA_CLK(clk_top_univpll_d3_d8);
	UNPREPARE_MDLA_CLK(clk_top_univpll_d3_d4);
	UNPREPARE_MDLA_CLK(clk_top_mainpll_d2_d4);
	UNPREPARE_MDLA_CLK(clk_top_univpll_d3_d2);
	UNPREPARE_MDLA_CLK(clk_top_mainpll_d2_d2);
	UNPREPARE_MDLA_CLK(clk_top_univpll_d2_d2);
	UNPREPARE_MDLA_CLK(clk_top_mainpll_d3);
	UNPREPARE_MDLA_CLK(clk_top_univpll_d3);
	UNPREPARE_MDLA_CLK(clk_top_mmpll_d7);
	UNPREPARE_MDLA_CLK(clk_top_mmpll_d6);
	UNPREPARE_MDLA_CLK(clk_top_adsppll_d5);
	UNPREPARE_MDLA_CLK(clk_top_tvdpll_ck);
	UNPREPARE_MDLA_CLK(clk_top_tvdpll_mainpll_d2_ck);
	UNPREPARE_MDLA_CLK(clk_top_univpll_d2);
	UNPREPARE_MDLA_CLK(clk_top_adsppll_d4);
	UNPREPARE_MDLA_CLK(clk_top_mainpll_d2);
	UNPREPARE_MDLA_CLK(clk_top_mmpll_d4);



#undef UNPREPARE_MDLA_CLK
#endif
}

int mdla_get_power(int core)
{
	int ret = 0;

	mdla_dvfs_debug("[mdla_%d/%d] gp +\n", core, power_counter[core]);
	mutex_lock(&power_counter_mutex[core]);
	power_counter[core]++;
	ret = mdla_boot_up(core);
	mdla_reset_lock(REASON_POWERON);
	mutex_unlock(&power_counter_mutex[core]);
	mdla_dvfs_debug("[mdla_%d/%d] gp + 2\n", core, power_counter[core]);

	if (ret == POWER_ON_MAGIC) {
		mutex_lock(&opp_mutex);
		if (change_freq_first[core]) {
			mdla_dvfs_debug("[mdla_%d] change freq first(%d)\n",
					core, change_freq_first[core]);
			/*mutex_unlock(&opp_mutex);*/
			/*mutex_lock(&opp_mutex);*/
			if (force_change_dsp_freq[core]) {
				mutex_unlock(&opp_mutex);
				/* force change freq while running */
		mdla_dvfs_debug("mdla_%d force change dsp freq", core);
				mdla_change_opp(core, OPPTYPE_DSPFREQ);
			} else {
				mutex_unlock(&opp_mutex);
			}

			mutex_lock(&opp_mutex);
			//if (force_change_vcore_opp[core]) {
			if (force_change_vmdla_opp[core]) {
				mutex_unlock(&opp_mutex);
				/* vcore change should wait */
		mdla_dvfs_debug("mdla_%d force change vmdla opp", core);
				//mdla_change_opp(core, OPPTYPE_VCORE);
				mdla_change_opp(core, OPPTYPE_VMDLA);
			} else {
				mutex_unlock(&opp_mutex);
			}
		} else {
			/*mutex_unlock(&opp_mutex);*/
			/*mutex_lock(&opp_mutex);*/
			//if (force_change_vcore_opp[core]) {
			if (force_change_vmdla_opp[core]) {
				mutex_unlock(&opp_mutex);
				/* vcore change should wait */
		mdla_dvfs_debug("mdla_%d force change vcore opp", core);
				//mdla_change_opp(core, OPPTYPE_VCORE);
				mdla_change_opp(core, OPPTYPE_VMDLA);
			} else {
				mutex_unlock(&opp_mutex);
			}

			mutex_lock(&opp_mutex);
			if (force_change_dsp_freq[core]) {
				mutex_unlock(&opp_mutex);
				/* force change freq while running */
		mdla_dvfs_debug("mdla_%d force change dsp freq", core);
				mdla_change_opp(core, OPPTYPE_DSPFREQ);
			} else {
				mutex_unlock(&opp_mutex);
			}
		}
	}
	LOG_DBG("[mdla_%d/%d] gp -\n", core, power_counter[core]);
	if (mdla_klog & MDLA_DBG_DVFS)
		apu_get_power_info();
	enable_apu_bw(0);
	enable_apu_bw(1);
	enable_apu_bw(2);
	enable_apu_latency(0);
	enable_apu_latency(1);
	enable_apu_latency(2);
	if (core == 0)
		apu_power_count_enable(true, USER_MDLA);
	else if (core == 1)
		apu_power_count_enable(true, USER_EDMA);
	if (ret == POWER_ON_MAGIC)
		return 0;
	else
		return ret;
}
EXPORT_SYMBOL(mdla_get_power);

void mdla_put_power(int core)
{
	mdla_dvfs_debug("[mdla_%d/%d] pp +\n", core, power_counter[core]);
	mutex_lock(&power_counter_mutex[core]);
	if (--power_counter[core] == 0)
		mdla_shut_down(core);

	mutex_unlock(&power_counter_mutex[core]);
	LOG_DBG("[mdla_%d/%d] pp -\n", core, power_counter[core]);
}
EXPORT_SYMBOL(mdla_put_power);

static int mdla_set_power(struct mdla_power *power)
{
	int ret = 0;
	uint8_t vmdla_opp_index = 0xFF;
	uint8_t dsp_freq_index = 0xFF;
	uint8_t opp_step = 0;

	opp_step = mdla_boost_value_to_opp(power->boost_value);
	if (opp_step < MDLA_MAX_NUM_OPPS && opp_step >= 0) {
		vmdla_opp_index = opps.vmdla.opp_map[opp_step];
		dsp_freq_index = opps.mdlacore.opp_map[opp_step];
	} else {
		LOG_ERR("wrong opp step (%d)", opp_step);
			ret = -1;
			return ret;
	}
	mdla_opp_check(0, vmdla_opp_index, dsp_freq_index);
	//user->power_opp = power->opp_step;

	ret = mdla_get_power(0);
	/* to avoid power leakage, power on/off need be paired */
	mdla_put_power(0);
	mdla_dvfs_debug("[mdla] %s -\n", __func__);
	return ret;
}

static void mdla_power_counter_routine(struct work_struct *work)
{
	int core = 0;
	struct my_struct_t *my_work_core =
			container_of(work, struct my_struct_t, my_work.work);

	core = my_work_core->core;
	mdla_dvfs_debug("mdla_%d counterR (%d)+\n", core, power_counter[core]);

	mutex_lock(&power_counter_mutex[core]);
	if (power_counter[core] == 0)
		mdla_shut_down(core);
	else
		LOG_DBG("mdla_%d no need this time.\n", core);
	mutex_unlock(&power_counter_mutex[core]);

	mdla_dvfs_debug("mdla_%d counterR -", core);
}
int mdla_quick_suspend(int core)
{
	mdla_dvfs_debug("[mdla_%d] q_suspend +\n", core);
	mutex_lock(&power_counter_mutex[core]);
	//mdla_dvfs_debug("[mdla_%d] q_suspend (%d/%d)\n", core,
		//power_counter[core], mdla_service_cores[core].state);

	if (power_counter[core] == 0) {
		mutex_unlock(&power_counter_mutex[core]);

			mod_delayed_work(wq,
				&(power_counter_work[core].my_work),
				msecs_to_jiffies(0));
	} else {
		mutex_unlock(&power_counter_mutex[core]);
	}

	return 0;
}


static void mdla_opp_keep_routine(struct work_struct *work)
{
	mdla_dvfs_debug("%s flag (%d) +\n", __func__, opp_keep_flag);
	mutex_lock(&opp_mutex);
	opp_keep_flag = false;
	mutex_unlock(&opp_mutex);
	mdla_dvfs_debug("%s flag (%d) -\n", __func__, opp_keep_flag);
}
int mdla_init_hw(int core, struct platform_device *pdev)
{
	int ret, i, j;
	//int param;

	if (core == 0) {
		//init_waitqueue_head(&cmd_wait);
		//mutex_init(&lock_mutex);
		//init_waitqueue_head(&lock_wait);
		mutex_init(&opp_mutex);
		//init_waitqueue_head(&waitq_change_vcore);
		//init_waitqueue_head(&waitq_do_core_executing);
		//is_locked = false;
		max_vcore_opp = 0;
		max_vmdla_opp = 0;
		max_dsp_freq = 0;
		opp_keep_flag = false;
		//mdla_dev = device;
		is_power_debug_lock = false;

		for (i = 0 ; i < MTK_MDLA_USER ; i++) {
			mutex_init(&(power_mutex[i]));
			mutex_init(&(power_counter_mutex[i]));
			mutex_init(&power_lock_mutex);
			power_counter[i] = 0;
			power_counter_work[i].core = i;
			is_power_on[i] = false;
			force_change_vcore_opp[i] = false;
			force_change_vmdla_opp[i] = false;
			force_change_dsp_freq[i] = false;
			change_freq_first[i] = false;
			//exception_isr_check[i] = false;

			INIT_DELAYED_WORK(&(power_counter_work[i].my_work),
						mdla_power_counter_routine);
			}
			wq = create_workqueue("mdla_wq");
#define DEFINE_APU_STEP(step, m, v0, v1, v2, v3, \
						v4, v5, v6, v7, v8, v9, \
						v10, v11, v12, v13, v14, v15) \
				{ \
					opps.step.index = m - 1; \
					opps.step.count = m; \
					opps.step.values[0] = v0; \
					opps.step.values[1] = v1; \
					opps.step.values[2] = v2; \
					opps.step.values[3] = v3; \
					opps.step.values[4] = v4; \
					opps.step.values[5] = v5; \
					opps.step.values[6] = v6; \
					opps.step.values[7] = v7; \
					opps.step.values[8] = v8; \
					opps.step.values[9] = v9; \
					opps.step.values[10] = v10; \
					opps.step.values[11] = v11; \
					opps.step.values[12] = v12; \
					opps.step.values[13] = v13; \
					opps.step.values[14] = v14; \
					opps.step.values[15] = v15; \
				}


#define DEFINE_APU_OPP(i, v0, v1, v2, v3, v4, v5, v6) \
			{ \
				opps.vvpu.opp_map[i]  = v0; \
				opps.vmdla.opp_map[i]  = v1; \
				opps.dsp.opp_map[i]    = v2; \
				opps.dspcore[0].opp_map[i]	 = v3; \
				opps.dspcore[1].opp_map[i]	 = v4; \
				opps.ipu_if.opp_map[i] = v5; \
				opps.mdlacore.opp_map[i]    = v6; \
			}

			DEFINE_APU_STEP(vcore, 3, 825000,
				725000, 650000, 0,
				0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0);
			DEFINE_APU_STEP(vvpu, 3, 825000,
				725000, 650000, 0,
				0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0);
			DEFINE_APU_STEP(vmdla, 3, 825000,
				725000, 650000, 0,
				0, 0, 0, 0, 0,
				0, 0, 0, 0, 0, 0, 0);
			DEFINE_APU_STEP(dsp, 16, 624000,
				624000, 606000, 594000,
				560000, 525000, 450000,
				416000, 364000, 312000,
				273000, 208000, 137000,
				104000, 52000, 26000);
			DEFINE_APU_STEP(dspcore[0], 16, 700000,
				624000, 606000, 594000,
				560000, 525000, 450000,
				416000, 364000, 312000,
				273000, 208000, 137000,
				104000, 52000, 26000);
			DEFINE_APU_STEP(dspcore[1], 16, 700000,
				624000, 606000, 594000,
				560000, 525000, 450000,
				416000, 364000, 312000,
				273000, 208000, 137000,
				104000, 52000, 26000);
			DEFINE_APU_STEP(mdlacore, 16, 788000,
				700000, 624000, 606000,
				594000, 546000, 525000,
				450000, 416000, 364000,
				312000, 273000, 208000,
				137000, 52000, 26000);
			DEFINE_APU_STEP(ipu_if, 16, 624000,
				624000, 606000, 594000,
				560000, 525000, 450000,
				416000, 364000, 312000,
				273000, 208000, 137000,
				104000, 52000, 26000);

			/* default freq */
			DEFINE_APU_OPP(0, 0, 0, 0, 0, 0, 0, 0);
			DEFINE_APU_OPP(1, 0, 0, 1, 1, 1, 1, 1);
			DEFINE_APU_OPP(2, 0, 0, 2, 2, 2, 2, 2);
			DEFINE_APU_OPP(3, 0, 1, 3, 3, 3, 3, 3);
			DEFINE_APU_OPP(4, 0, 1, 4, 4, 4, 4, 4);
			DEFINE_APU_OPP(5, 1, 1, 5, 5, 5, 5, 5);
			DEFINE_APU_OPP(6, 1, 1, 6, 6, 6, 6, 6);
			DEFINE_APU_OPP(7, 1, 1, 7, 7, 7, 7, 7);
			DEFINE_APU_OPP(8, 1, 1, 8, 8, 8, 8, 8);
			DEFINE_APU_OPP(9, 2, 2, 9, 9, 9, 9, 9);
			DEFINE_APU_OPP(10, 2, 2, 10, 10, 10, 10, 10);
			DEFINE_APU_OPP(11, 2, 2, 11, 11, 11, 11, 11);
			DEFINE_APU_OPP(12, 2, 2, 12, 12, 12, 12, 12);
			DEFINE_APU_OPP(13, 2, 2, 13, 13, 13, 13, 13);
			DEFINE_APU_OPP(14, 2, 2, 14, 14, 14, 14, 14);
			DEFINE_APU_OPP(15, 2, 2, 15, 15, 15, 15, 15);


			/* default low opp */
			opps.count = 16;
			opps.index = 5; /* user space usage*/
			opps.vcore.index = 1;
			opps.vmdla.index = 1;
			opps.dsp.index = 9;
			opps.ipu_if.index = 9;
			opps.mdlacore.index = 9;
#undef DEFINE_APU_OPP
#undef DEFINE_APU_STEP
		ret = mdla_prepare_regulator_and_clock(&pdev->dev);
		if (ret) {
			LOG_ERR("[mdla_%d]fail to prepare regulator or clk!\n",
					core);
			goto out;
		}

		/* pmqos  */
		#ifdef ENABLE_PMQOS
		for (i = 0 ; i < MTK_MDLA_USER ; i++) {
			mtk_pm_qos_add_request(&mdla_qos_bw_request[i],
						MTK_PM_QOS_MEMORY_EXT_BANDWIDTH,
						PM_QOS_DEFAULT_VALUE);

			mtk_pm_qos_add_request(&mdla_qos_vcore_request[i],
					MTK_PM_QOS_VCORE_OPP,
					MTK_PM_QOS_VCORE_OPP_DEFAULT_VALUE);

		    mtk_pm_qos_add_request(&mdla_qos_vmdla_request[i],
					    MTK_PM_QOS_VMDLA_OPP,
					    MTK_PM_QOS_VMDLA_OPP_DEFAULT_VALUE);
		    mtk_pm_qos_add_request(&mdla_qos_vvpu_request[i],
					    MTK_PM_QOS_VVPU_OPP,
					    MTK_PM_QOS_VVPU_OPP_DEFAULT_VALUE);
			mtk_pm_qos_update_request(&mdla_qos_vvpu_request[i],
								VVPU_OPP_2);
			mtk_pm_qos_update_request(&mdla_qos_vmdla_request[i],
								VMDLA_OPP_2);
		}
		mdla_dvfs_debug("[mdla]init vvpu, vmdla to opp2\n");
		ret = vmdla_regulator_set_mode(true);
		udelay(100);
		mdla_dvfs_debug("vvpu set sleep mode ret=%d\n", ret);
		ret = vmdla_regulator_set_mode(false);
		mdla_dvfs_debug("vvpu set sleep mode ret=%d\n", ret);
		udelay(100);
		#endif

	}
	/*init mdla lock power struct*/
	for (i = 0 ; i < MDLA_OPP_PRIORIYY_NUM ; i++) {
		lock_power[i][0].core = 0;
		lock_power[i][0].max_boost_value = 100;
		lock_power[i][0].min_boost_value = 0;
		lock_power[i][0].lock = false;
		lock_power[i][0].priority = MDLA_OPP_NORMAL;
	}
	for (j = 0 ; j < MTK_MDLA_USER ; j++) {
		min_opp[j] = MDLA_MAX_NUM_OPPS-1;
		max_opp[j] = 0;
		minboost[j] = 0;
		maxboost[j] = 100;
	}
	get_segment_from_efuse();
	mdla_init_done = 1;
	return 0;

out:
	return ret;
}

int mdla_uninit_hw(void)
{
	int i;

	for (i = 0 ; i < MTK_MDLA_USER ; i++) {
		cancel_delayed_work(&(power_counter_work[i].my_work));
	}
	cancel_delayed_work(&opp_keep_work);
	/* pmqos  */
	#ifdef ENABLE_PMQOS
	for (i = 0 ; i < MTK_MDLA_USER ; i++) {
		mtk_pm_qos_remove_request(&mdla_qos_bw_request[i]);
		mtk_pm_qos_remove_request(&mdla_qos_vcore_request[i]);
		mtk_pm_qos_remove_request(&mdla_qos_vmdla_request[i]);
		mtk_pm_qos_remove_request(&mdla_qos_vvpu_request[i]);
	}
	#endif

	mdla_unprepare_regulator_and_clock();
	if (wq) {
		flush_workqueue(wq);
		destroy_workqueue(wq);
		wq = NULL;
	}
	mdla_init_done = 0;
	return 0;
}

static int mdla_boot_up(int core)
{
	int ret = 0;

	mdla_dvfs_debug("[mdla_%d] boot_up +\n", core);
	mutex_lock(&power_mutex[core]);
	mdla_dvfs_debug("[mdla_%d] is_power_on(%d)\n", core, is_power_on[core]);
	if (is_power_on[core]) {
		mutex_unlock(&power_mutex[core]);
		//mutex_lock(&(mdla_service_cores[core].state_mutex));
		//mdla_service_cores[core].state = VCT_BOOTUP;
		//mutex_unlock(&(mdla_service_cores[core].state_mutex));
		//wake_up_interruptible(&waitq_change_vcore);
		mdla_dvfs_debug("[mdla_%d] already power on\n", core);
		return POWER_ON_MAGIC;
	}
	mdla_dvfs_debug("[mdla_%d] boot_up flag2\n", core);

	mdla_trace_tag_begin("%s", __func__);
	//mutex_lock(&(mdla_service_cores[core].state_mutex));
	//mdla_service_cores[core].state = VCT_BOOTUP;
	//mutex_unlock(&(mdla_service_cores[core].state_mutex));
	//wake_up_interruptible(&waitq_change_vcore);

	ret = mdla_enable_regulator_and_clock(core);
	if (ret) {
		LOG_ERR("[mdla_%d]fail to enable regulator or clock\n", core);
		goto out;
	}
#ifdef MET_POLLING_MODE
	ret = mdla_profile_state_set(core, 1);
	if (ret) {
		LOG_ERR("[mdla_%d] fail to mdla_profile_state_set 1\n", core);
		goto out;
	}
#endif

out:
	mdla_trace_tag_end();
	mutex_unlock(&power_mutex[core]);

#ifdef MTK_PERF_OBSERVER
	if (!ret) {
		struct pob_xpufreq_info pxi;

		pxi.id = core;
		pxi.opp = opps.mdlacore.index;

		pob_xpufreq_update(POB_XPUFREQ_MDLA, &pxi);
	}
#endif

	return ret;
}

static int mdla_shut_down(int core)
{
	int ret = 0;

	if (core == 0)
		apu_power_count_enable(false, USER_MDLA);
	else if (core == 1)
		apu_power_count_enable(false, USER_EDMA);
	apu_shut_down();


	mdla_dvfs_debug("[mdla_%d] shutdown +\n", core);
	mutex_lock(&power_mutex[core]);
	if (!is_power_on[core]) {
		mutex_unlock(&power_mutex[core]);
		return 0;
	}
#ifdef MET_POLLING_MODE
	ret = mdla_profile_state_set(core, 0);
	if (ret) {
		LOG_ERR("[mdla_%d] fail to mdla_profile_state_set 0\n", core);
		goto out;
	}
#endif

	mdla_trace_tag_begin("%s", __func__);
	mdla_dvfs_debug("[mdla_%d] mdla_disable_regulator_and_clock +\n", core);
	ret = mdla_disable_regulator_and_clock(core);
	if (ret) {
		LOG_ERR("[mdla_%d]fail to disable regulator and clock\n", core);
		goto out;
	}
	mdla_dvfs_debug("[mdla_%d] mdla_disable_regulator_and_clock -\n", core);

	//wake_up_interruptible(&waitq_change_vcore);
out:
	mdla_trace_tag_end();
	mutex_unlock(&power_mutex[core]);
	LOG_DBG("[mdla_%d] shutdown -\n", core);

#ifdef MTK_PERF_OBSERVER
	if (!ret) {
		struct pob_xpufreq_info pxi;

		pxi.id = core;
		pxi.opp = -1;

		pob_xpufreq_update(POB_XPUFREQ_MDLA, &pxi);
	}
#endif

	return ret;
}

int mdla_dump_opp_table(struct seq_file *s)
{
	int i;

#define LINE_BAR "  +-----+----------+----------+------------+-----------+-----------+\n"
	mdla_print_seq(s, LINE_BAR);
	mdla_print_seq(s, "  |%-5s|%-10s|%-10s|%-10s|%-10s|%-12s|\n",
				"OPP", "VVPU(uV)",
				"VMDLA(uV)", "MDLA(KHz)", "DSP(KHz)",
				"IPU_IF(KHz)");
	mdla_print_seq(s, LINE_BAR);

	for (i = 0; i < opps.count; i++) {
		mdla_print_seq(s,
"  |%-5d|[%d]%-7d|[%d]%-7d|[%d]%-7d|[%d]%-7d|[%d]%-9d\n",
			i,
			opps.vvpu.opp_map[i],
			opps.vvpu.values[opps.vvpu.opp_map[i]],
			opps.vmdla.opp_map[i],
			opps.vmdla.values[opps.vmdla.opp_map[i]],
			opps.mdlacore.opp_map[i],
			opps.mdlacore.values[opps.mdlacore.opp_map[i]],
			opps.dsp.opp_map[i],
			opps.dsp.values[opps.dsp.opp_map[i]],
			opps.ipu_if.opp_map[i],
			opps.ipu_if.values[opps.ipu_if.opp_map[i]]);
	}

	mdla_print_seq(s, LINE_BAR);
#undef LINE_BAR

	return 0;
}

int mdla_dump_power(struct seq_file *s)
{
	int vmdla_opp = 0;

	vmdla_opp = mdla_get_hw_vmdla_opp(0);


mdla_print_seq(s, "%s(rw): %s[%d/%d]\n",
			"dvfs_debug",
			"vmdla", opps.vmdla.index, vmdla_opp);
mdla_print_seq(s, "dvfs_debug(rw): min/max opp[0][%d/%d]\n",
	min_opp[0], max_opp[0]);
mdla_print_seq(s, "dvfs_debug(rw): min/max boost[0][%d/%d]\n",
	minboost[0], maxboost[0]);
mdla_print_seq(s, "%s[%d], %s[%d], %s[%d]\n",
			"dsp", opps.dsp.index,
			"ipu_if", opps.ipu_if.index,
			"mdla", opps.mdlacore.index);


	mdla_print_seq(s, "is_power_debug_lock(rw): %d\n", is_power_debug_lock);

	return 0;
}

int mdla_set_power_parameter(uint8_t param, int argc, int *args)
{
	int ret = 0;

	switch (param) {
	case MDLA_POWER_PARAM_FIX_OPP:
		ret = (argc == 1) ? 0 : -EINVAL;
		if (ret) {
			LOG_ERR("invalid argument, expected:1, received:%d\n",
					argc);
			goto out;
		}
		switch (args[0]) {
		case 0:
			is_power_debug_lock = false;
			break;
		case 1:
			is_power_debug_lock = true;
			break;
		default:

			if (ret) {
				LOG_ERR("invalid argument, received:%d\n",
						(int)(args[0]));
				goto out;
			}
			ret = -EINVAL;
			goto out;

		}
		break;
	case MDLA_POWER_PARAM_DVFS_DEBUG:
		ret = (argc == 1) ? 0 : -EINVAL;
		if (ret) {
			LOG_ERR("invalid argument, expected:1, received:%d\n",
					argc);
			goto out;
		}

		ret = args[0] >= opps.count;
		if (ret) {
			LOG_ERR("opp step(%d) is out-of-bound, count:%d\n",
					(int)(args[0]), opps.count);
			goto out;
		}
		LOG_ERR("@@test%d\n", argc);

		opps.vcore.index = opps.vcore.opp_map[args[0]];
		opps.vvpu.index = opps.vvpu.opp_map[args[0]];
		opps.vmdla.index = opps.vmdla.opp_map[args[0]];
		opps.dsp.index = opps.dsp.opp_map[args[0]];
		opps.ipu_if.index = opps.ipu_if.opp_map[args[0]];
		opps.mdlacore.index = opps.mdlacore.opp_map[args[0]];

		is_power_debug_lock = true;


		break;
	case MDLA_POWER_PARAM_JTAG:
		mdla_put_power(0);
		break;
	case MDLA_POWER_PARAM_LOCK:
		ret = (argc == 1) ? 0 : -EINVAL;
		if (ret) {
			LOG_ERR("invalid argument, expected:1, received:%d\n",
					argc);
			goto out;
		}

		ret = args[0] >= opps.count;
		if (ret) {
			LOG_ERR("opp step(%d) is out-of-bound, count:%d\n",
					(int)(args[0]), opps.count);
			goto out;
		}
		LOG_ERR("@@lock%d\n", argc);

		opps.vcore.index = opps.vcore.opp_map[args[0]];
		opps.vvpu.index = opps.vvpu.opp_map[args[0]];
		opps.vmdla.index = opps.vmdla.opp_map[args[0]];
		opps.dsp.index = opps.dsp.opp_map[args[0]];
		opps.ipu_if.index = opps.ipu_if.opp_map[args[0]];
		opps.mdlacore.index = opps.mdlacore.opp_map[args[0]];


		mdla_opp_check(0, opps.dsp.index, opps.dsp.index);
		//user->power_opp = power->opp_step;

		ret = mdla_get_power(0);





		break;
	case MDLA_POWER_HAL_CTL:
	{
		struct mdla_lock_power mdla_lock_power;

		ret = (argc == 2) ? 0 : -EINVAL;
		if (ret) {
			LOG_ERR("invalid argument, expected:2, received:%d\n",
				argc);
				goto out;
		}

		if (args[0] > 100 || args[0] < 0) {
			LOG_ERR("min boost(%d) is out-of-bound\n",
					(int)(args[0]));
			goto out;
		}
		if (args[1] > 100 || args[1] < 0) {
			LOG_ERR("max boost(%d) is out-of-bound\n",
					(int)(args[1]));
			goto out;
		}
		mdla_lock_power.core = 1;
		mdla_lock_power.lock = true;
		mdla_lock_power.priority = MDLA_OPP_POWER_HAL;
		mdla_lock_power.max_boost_value = args[1];
		mdla_lock_power.min_boost_value = args[0];
		mdla_dvfs_debug("[mdla]POWER_HAL_LOCK+core:%d, maxb:%d, minb:%d\n",
			mdla_lock_power.core, mdla_lock_power.max_boost_value,
				mdla_lock_power.min_boost_value);
		ret = mdla_lock_set_power(&mdla_lock_power);
		if (ret) {
			LOG_ERR("[POWER_HAL_LOCK]failed, ret=%d\n", ret);
			goto out;
		}
		break;
	}
	case MDLA_EARA_CTL:
	{
		struct mdla_lock_power mdla_lock_power;

		ret = (argc == 2) ? 0 : -EINVAL;
		if (ret) {
			LOG_ERR("invalid argument, expected:3, received:%d\n",
					argc);
			goto out;
		}
		if (args[0] > 100 || args[0] < 0) {
			LOG_ERR("min boost(%d) is out-of-bound\n",
					(int)(args[0]));
			goto out;
		}
		if (args[1] > 100 || args[1] < 0) {
			LOG_ERR("max boost(%d) is out-of-bound\n",
					(int)(args[1]));
			goto out;
		}
		mdla_lock_power.core = 1;
		mdla_lock_power.lock = true;
		mdla_lock_power.priority = MDLA_OPP_EARA_QOS;
		mdla_lock_power.max_boost_value = args[1];
		mdla_lock_power.min_boost_value = args[0];
		mdla_dvfs_debug("[mdla]EARA_LOCK+core:%d, maxb:%d, minb:%d\n",
			mdla_lock_power.core, mdla_lock_power.max_boost_value,
				mdla_lock_power.min_boost_value);
		ret = mdla_lock_set_power(&mdla_lock_power);
		if (ret) {
			LOG_ERR("[POWER_HAL_LOCK]failed, ret=%d\n", ret);
			goto out;
		}
		break;
		}

	default:
		LOG_ERR("unsupport the power parameter:%d\n", param);
		break;
	}

out:
	return ret;
}

static uint8_t mdla_boost_value_to_opp(uint8_t boost_value)
{
	int ret = 0;
/* set dsp frequency - 0:788 MHz, 1:700 MHz, 2:606 MHz, 3:594 MHz*/
/* set dsp frequency - 4:560 MHz, 5:525 MHz, 6:450 MHz, 7:416 MHz*/
/* set dsp frequency - 8:364 MHz, 9:312 MHz, 10:273 MH, 11:208 MH*/
/* set dsp frequency - 12:137 MHz, 13:104 MHz, 14:52 MHz, 15:26 MHz*/
	uint32_t freq = 0;
	uint32_t freq0 = opps.mdlacore.values[0];
	uint32_t freq1 = opps.mdlacore.values[1];
	uint32_t freq2 = opps.mdlacore.values[2];
	uint32_t freq3 = opps.mdlacore.values[3];
	uint32_t freq4 = opps.mdlacore.values[4];
	uint32_t freq5 = opps.mdlacore.values[5];
	uint32_t freq6 = opps.mdlacore.values[6];
	uint32_t freq7 = opps.mdlacore.values[7];
	uint32_t freq8 = opps.mdlacore.values[8];
	uint32_t freq9 = opps.mdlacore.values[9];
	uint32_t freq10 = opps.mdlacore.values[10];
	uint32_t freq11 = opps.mdlacore.values[11];
	uint32_t freq12 = opps.mdlacore.values[12];
	uint32_t freq13 = opps.mdlacore.values[13];
	uint32_t freq14 = opps.mdlacore.values[14];
	uint32_t freq15 = opps.mdlacore.values[15];

	if ((boost_value <= 100) && (boost_value >= 0))
		freq = boost_value * freq0 / 100;
	else
		freq = freq0;

	if (freq <= freq0 && freq > freq1)
		ret = 0;
	else if (freq <= freq1 && freq > freq2)
		ret = 1;
	else if (freq <= freq2 && freq > freq3)
		ret = 2;
	else if (freq <= freq3 && freq > freq4)
		ret = 3;
	else if (freq <= freq4 && freq > freq5)
		ret = 4;
	else if (freq <= freq5 && freq > freq6)
		ret = 5;
	else if (freq <= freq6 && freq > freq7)
		ret = 6;
	else if (freq <= freq7 && freq > freq8)
		ret = 7;
	else if (freq <= freq8 && freq > freq9)
		ret = 8;
	else if (freq <= freq9 && freq > freq10)
		ret = 9;
	else if (freq <= freq10 && freq > freq11)
		ret = 10;
	else if (freq <= freq11 && freq > freq12)
		ret = 11;
	else if (freq <= freq12 && freq > freq13)
		ret = 12;
	else if (freq <= freq13 && freq > freq14)
		ret = 13;
	else if (freq <= freq14 && freq > freq15)
		ret = 14;
	else
		ret = 15;

	mdla_dvfs_debug("%s opp %d\n", __func__, ret);
	return ret;
}

static bool mdla_update_lock_power_parameter
	(struct mdla_lock_power *mdla_lock_power)
{
	bool ret = true;
	int i, core = -1;
	unsigned int priority = mdla_lock_power->priority;

	for (i = 0 ; i < MTK_MDLA_USER ; i++) {
		if (mdla_lock_power->core == (0x1 << i)) {
			core = i;
			break;
		}
	}

	if (core >= MTK_MDLA_USER || core < 0) {
		LOG_ERR("wrong core index (0x%x/%d/%d)",
			mdla_lock_power->core, core, MTK_MDLA_USER);
		ret = false;
		return ret;
	}
	lock_power[priority][core].core = core;
	lock_power[priority][core].max_boost_value =
		mdla_lock_power->max_boost_value;
	lock_power[priority][core].min_boost_value =
		mdla_lock_power->min_boost_value;
	lock_power[priority][core].lock = true;
	lock_power[priority][core].priority =
		mdla_lock_power->priority;
mdla_dvfs_debug("power_para core %d, maxboost:%d, minboost:%d pri%d\n",
		lock_power[priority][core].core,
		lock_power[priority][core].max_boost_value,
		lock_power[priority][core].min_boost_value,
		lock_power[priority][core].priority);
	return ret;
}
static bool mdla_update_unlock_power_parameter
	(struct mdla_lock_power *mdla_lock_power)
{
	bool ret = true;
	int i, core = -1;
	unsigned int priority = mdla_lock_power->priority;

	for (i = 0 ; i < MTK_MDLA_USER ; i++) {
		if (mdla_lock_power->core == (0x1 << i)) {
			core = i;
			break;
		}
	}

	if (core >= MTK_MDLA_USER || core < 0) {
		LOG_ERR("wrong core index (0x%x/%d/%d)",
			mdla_lock_power->core, core, MTK_MDLA_USER);
		ret = false;
		return ret;
	}

	lock_power[priority][core].core =
		mdla_lock_power->core;
	lock_power[priority][core].max_boost_value =
		mdla_lock_power->max_boost_value;
	lock_power[priority][core].min_boost_value =
		mdla_lock_power->min_boost_value;
	lock_power[priority][core].lock = false;
	lock_power[priority][core].priority =
		mdla_lock_power->priority;
	mdla_dvfs_debug("%s\n", __func__);
	return ret;
}
uint8_t mdla_min_of(uint8_t value1, uint8_t value2)
{
	if (value1 <= value2)
		return value1;
	else
		return value2;
}
uint8_t mdla_max_of(uint8_t value1, uint8_t value2)
{
	if (value1 <= value2)
		return value2;
	else
		return value1;
}

bool mdla_update_max_opp(struct mdla_lock_power *mdla_lock_power)
{
	bool ret = true;
	int i, core = -1;
	uint8_t first_priority = MDLA_OPP_NORMAL;
	uint8_t first_priority_max_boost_value = 100;
	uint8_t first_priority_min_boost_value = 0;
	uint8_t temp_max_boost_value = 100;
	uint8_t temp_min_boost_value = 0;
	bool lock = false;
	uint8_t max_boost = 100;
	uint8_t min_boost = 0;
	uint8_t priority = MDLA_OPP_NORMAL;

	for (i = 0 ; i < MTK_MDLA_USER ; i++) {
		if (mdla_lock_power->core == (0x1 << i)) {
			core = i;
			break;
		}
	}

	if (core >= MTK_MDLA_USER || core < 0) {
		LOG_ERR("wrong core index (0x%x/%d/%d)",
			mdla_lock_power->core, core, MTK_MDLA_USER);
		ret = false;
		return ret;
	}

	for (i = 0 ; i < MDLA_OPP_PRIORIYY_NUM ; i++) {
		if (lock_power[i][core].lock == true
			&& lock_power[i][core].priority < MDLA_OPP_NORMAL) {
			first_priority = i;
			first_priority_max_boost_value =
				lock_power[i][core].max_boost_value;
			first_priority_min_boost_value =
				lock_power[i][core].min_boost_value;
			break;
			}
	}
	temp_max_boost_value = first_priority_max_boost_value;
	temp_min_boost_value = first_priority_min_boost_value;
/*find final_max_boost_value*/
	for (i = first_priority ; i < MDLA_OPP_PRIORIYY_NUM ; i++) {
		lock = lock_power[i][core].lock;
		max_boost = lock_power[i][core].max_boost_value;
		min_boost = lock_power[i][core].min_boost_value;
		priority = lock_power[i][core].priority;
		if (lock == true) {
			if (priority < MDLA_OPP_NORMAL &&
			(((max_boost <= temp_max_boost_value) &&
			(max_boost >= temp_min_boost_value)) ||
			((min_boost <= temp_max_boost_value) &&
			(min_boost >= temp_min_boost_value)) ||
			((max_boost >= temp_max_boost_value) &&
			(min_boost <= temp_min_boost_value)))) {

				temp_max_boost_value =
	mdla_min_of(temp_max_boost_value, max_boost);
				temp_min_boost_value =
	mdla_max_of(temp_min_boost_value, min_boost);

			}
	}
		}
	max_opp[core] = mdla_boost_value_to_opp(temp_max_boost_value);
	min_opp[core] = mdla_boost_value_to_opp(temp_min_boost_value);
	maxboost[core] = temp_max_boost_value;
	minboost[core] = temp_min_boost_value;
	mdla_dvfs_debug("final_min_boost_value:%d final_max_boost_value:%d\n",
		temp_min_boost_value, temp_max_boost_value);
	return ret;
}

static int mdla_lock_set_power(struct mdla_lock_power *mdla_lock_power)
{
	int ret = -1;
	int i, core = -1;

	mutex_lock(&power_lock_mutex);
	for (i = 0 ; i < MTK_MDLA_USER ; i++) {
		if (mdla_lock_power->core == (0x1 << i)) {
			core = i;
			break;
		}
	}

	if (core >= MTK_MDLA_USER || core < 0) {
		LOG_ERR("wrong core index (0x%x/%d/%d)",
			mdla_lock_power->core, core, MTK_MDLA_USER);
		ret = -1;
		mutex_unlock(&power_lock_mutex);
		return ret;
	}


	if (!mdla_update_lock_power_parameter(mdla_lock_power)) {
		mutex_unlock(&power_lock_mutex);
		return -1;
		}
	if (!mdla_update_max_opp(mdla_lock_power)) {
		mutex_unlock(&power_lock_mutex);
		return -1;
		}
	mutex_unlock(&power_lock_mutex);
	return 0;
}

static int mdla_unlock_set_power(struct mdla_lock_power *mdla_lock_power)
{
	int ret = -1;
	int i, core = -1;

	mutex_lock(&power_lock_mutex);
	for (i = 0 ; i < MTK_MDLA_USER ; i++) {
		if (mdla_lock_power->core == (0x1 << i)) {
			core = i;
			break;
		}
	}

	if (core >= MTK_MDLA_USER || core < 0) {
		LOG_ERR("wrong core index (0x%x/%d/%d)",
			mdla_lock_power->core, core, MTK_MDLA_USER);
		ret = false;
		return ret;
	}
	if (!mdla_update_unlock_power_parameter(mdla_lock_power)) {
		mutex_unlock(&power_lock_mutex);
		return -1;
		}
	if (!mdla_update_max_opp(mdla_lock_power)) {
		mutex_unlock(&power_lock_mutex);
		return -1;
		}
	mutex_unlock(&power_lock_mutex);
	return ret;

}
//static __u8 mdla_get_first_priority_cmd(struct list_head *cmd_list)
//{
//	struct command_entry *iter, *n;
//	__u8 first_priority = MDLA_REQ_MAX_NUM_PRIORITY-1;
//
//	if (!list_empty(cmd_list)) {
//		list_for_each_entry_safe(iter, n, cmd_list, list) {
//			if (iter->priority < first_priority)
//				first_priority = iter->priority;
//		}
//	} else
//	first_priority = 0;
//
//
//	LOG_INF("%s:first_priority=%d\n", __func__, first_priority);
//	return first_priority;
//}

long mdla_dvfs_ioctl(struct file *filp, unsigned int command,
		unsigned long arg)
{
	long retval = 0;

	switch (command) {
	case IOCTL_SET_POWER:
	{
		struct mdla_power power;

		if (copy_from_user(&power, (void *) arg,
			sizeof(struct mdla_power))) {
			retval = -EFAULT;
			return retval;
		}

		retval = mdla_set_power(&power);
		LOG_INF("[SET_POWER] ret=%ld\n", retval);
		return retval;

		break;
	}

	case IOCTL_EARA_LOCK_POWER:
	{
		struct mdla_lock_power mdla_lock_power;

		if (copy_from_user(&mdla_lock_power, (void *) arg,
			sizeof(struct mdla_lock_power))) {
			retval = -EFAULT;
			return retval;
		}

		mdla_lock_power.lock = true;
		mdla_lock_power.priority = MDLA_OPP_EARA_QOS;
LOG_INF("[mdla] IOCTL_EARA_LOCK_POWER +,maxboost:%d, minboost:%d\n",
	mdla_lock_power.max_boost_value, mdla_lock_power.min_boost_value);
		retval = mdla_lock_set_power(&mdla_lock_power);
		return retval;

		break;
	}
	case IOCTL_EARA_UNLOCK_POWER:
	{
		struct mdla_lock_power mdla_lock_power;

		if (copy_from_user(&mdla_lock_power, (void *) arg,
			sizeof(struct mdla_lock_power))) {
			retval = -EFAULT;
			return retval;
			}
		mdla_lock_power.lock = false;
		mdla_lock_power.priority = MDLA_OPP_EARA_QOS;
		LOG_INF("[mdla] IOCTL_EARA_UNLOCK_POWER +\n");
		retval = mdla_unlock_set_power(&mdla_lock_power);
		return retval;

		break;
	}
	case IOCTL_POWER_HAL_LOCK_POWER:
	{
		struct mdla_lock_power mdla_lock_power;

		if (copy_from_user(&mdla_lock_power, (void *) arg,
			sizeof(struct mdla_lock_power))) {
			retval = -EFAULT;
			return retval;
			}
		mdla_lock_power.lock = true;
		mdla_lock_power.priority = MDLA_OPP_POWER_HAL;
LOG_INF("[mdla] IOCTL_POWER_HAL_LOCK_POWER +, maxboost:%d, minboost:%d\n",
	mdla_lock_power.max_boost_value, mdla_lock_power.min_boost_value);
		retval = mdla_lock_set_power(&mdla_lock_power);
		return retval;

		break;
	}
	case IOCTL_POWER_HAL_UNLOCK_POWER:
	{
		struct mdla_lock_power mdla_lock_power;

		if (copy_from_user(&mdla_lock_power, (void *) arg,
			sizeof(struct mdla_lock_power))) {
			retval = -EFAULT;
			return retval;
			}
		mdla_lock_power.lock = false;
		mdla_lock_power.priority = MDLA_OPP_POWER_HAL;
		LOG_INF("[mdla] IOCTL_POWER_HAL_UNLOCK_POWER +\n");
		retval = mdla_unlock_set_power(&mdla_lock_power);
		return retval;

		break;
	}
	default:
		return -EINVAL;
	}

	return 0;
}

/* command start: enable power & clock */
int mdla_dvfs_cmd_start(struct command_entry *ce)
{
	int ret = 0;
	uint8_t opp_step = 0;
	uint8_t vmdla_opp_index = 0xFF;
	uint8_t dsp_freq_index = 0xFF;

	if (ce) {
		opp_step = mdla_boost_value_to_opp(ce->boost_value);
		vmdla_opp_index = opps.vmdla.opp_map[opp_step];
		dsp_freq_index = opps.mdlacore.opp_map[opp_step];

		mdla_opp_check(0, vmdla_opp_index, dsp_freq_index);
	}

	/* step1, enable clocks and boot-up if needed */

	ret = mdla_get_power(0);

	if (ret) {
		apu_get_power_info();
		LOG_ERR("[mdla] fail to get power!\n");
		return ret;
	}
#if defined(MDLA_MET_READY)
	MDLA_MET_Events_Trace(1, 0);
#endif
	return ret;
}

/* command end: save bandwidth info to command entry */
int mdla_dvfs_cmd_end_info(struct command_entry *ce)
{
	int i;	// TODO: remove me

	if (ce == NULL)
		return 0;

	for (i = 0; i < 16; i++)  // TODO: remove me
		ce->bandwidth = i;

	// TODO: Put bandwidth to ce->bandwidth.

	return 0;
}

/* command end: fifo empty, shutdown */
int mdla_dvfs_cmd_end_shutdown(void)
{

	mdla_put_power(0);
#if defined(MDLA_MET_READY)
		MDLA_MET_Events_Trace(0, 0);
#endif

	return 0;
}

#endif
