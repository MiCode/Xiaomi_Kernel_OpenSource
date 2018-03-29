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

/*
1. vcore_ao, vcore_pdn, merge to vcore
	 => struct dvfs_opp {,  struct pwr_ctrl {
	 => static struct dvfs_opp opp_table1[] __nosavedata = {,    static struct pwr_ctrl vcorefs_ctrl = {
	 => get_vcore_ao(), get_vcore_pdn()
	 => __update_vcore_ao_pdn()
	 => set_vcore_ao_pdn()
	=> pwr_ctrl_show(), opp_table_show(), opp_table_store(), vcore_debug_show()
2. vcore regulator framework
	=>
*/

#define pr_fmt(fmt)		"[VcoreFS] " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kobject.h>
#include <linux/wakelock.h>
#include <linux/kthread.h>
#include <linux/atomic.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#if 0				/* L318_Need_Related_File */
#include <linux/earlysuspend.h>
#endif				/* L318_Need_Related_File */
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/sched/rt.h>
#include <linux/clk.h>

#include "mt_vcore_dvfs.h"
#include <mt_chip.h>
#if 0				/* L318_Need_Related_File */
#include <mach/mt_pmic_wrap.h>
#endif				/* L318_Need_Related_File */
#include "mt_cpufreq.h"

#include <linux/platform_device.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/regulator/consumer.h>
#endif
#if 0				/* L318_Need_Related_File */
#include <mach/mt_dramc.h>
#endif				/* L318_Need_Related_File */
#include "mt_spm.h"
#if 0				/* L318_Need_Related_File */
#include <mach/board.h>
#include <mach/mtk_wcn_cmb_stub.h>
#endif				/* L318_Need_Related_File */
#if 0				/* VCORE-DVFS-TBD */
#include <mt_sd_func.h>
#endif				/* VCORE-DVFS-TBD */

#include <linux/regulator/consumer.h>

/**************************************
 * Config and Parameter
 **************************************/
#define VCORE_SET_CHECK		0
#define FDDR_SET_CHECK		0

#define FDDR_S0_KHZ		1792000
#define FDDR_S1_KHZ		1600000
#define FDDR_S2_KHZ		1333000

#define FAXI_S1_KHZ		273000	/* MUX_1 */
#define FAXI_S2_KHZ		218400	/* MUX_2 */

#define FMM_S1_KHZ		400000	/* FH */
#define FMM_S2_KHZ		317000	/* FH */

#define FVENC_S1_KHZ		494000	/* FH */
#define FVENC_S2_KHZ		384000	/* FH */

#define FVDEC_S1_KHZ		494000	/* FH */
#define FVDEC_S2_KHZ		384000	/* FH */

#define DVFS_CMD_SETTLE_US	5	/* (PWRAP->1.5us->PMIC->10mv/1us) * 2 */
#define DRAM_WINDOW_SHIFT_MAX	128

#define OPPI_SCREEN_OFF_LP	1
#define OPPI_LATE_INIT_LP	1


/**************************************
 * Macro and Inline
 **************************************/
#define DEFINE_ATTR_RO(_name)			\
static struct kobj_attribute _name##_attr = {	\
	.attr	= {				\
		.name = #_name,			\
		.mode = 0444,			\
	},					\
	.show	= _name##_show,			\
}

#define DEFINE_ATTR_RW(_name)			\
static struct kobj_attribute _name##_attr = {	\
	.attr	= {				\
		.name = #_name,			\
		.mode = 0644,			\
	},					\
	.show	= _name##_show,			\
	.store	= _name##_store,		\
}

#define __ATTR_OF(_name)	(&_name##_attr.attr)

#define vcorefs_emerg(fmt, args...)	pr_emerg(fmt, ##args)
#define vcorefs_alert(fmt, args...)	pr_alert(fmt, ##args)
#define vcorefs_crit(fmt, args...)	pr_crit(fmt, ##args)
#define vcorefs_err(fmt, args...)	pr_err(fmt, ##args)
#define vcorefs_warn(fmt, args...)	pr_warn(fmt, ##args)
#define vcorefs_notice(fmt, args...)	pr_warn(fmt, ##args)
#define vcorefs_info(fmt, args...)	pr_warn(fmt, ##args)
#define vcorefs_debug(fmt, args...)	pr_warn(fmt, ##args)	/* pr_debug show nothing */

/**************************************
 * Define and Declare
 **************************************/
struct dvfs_opp {
	u32 vcore;		/* after vcore ptpod */
	u32 vcore_nml;		/* before vcore ptpod,1.125V or 1.1V */
	u32 ddr_khz;
	u32 axi_khz;
	u32 mm_khz;
	u32 venc_khz;
	u32 vdec_khz;
};

struct pwr_ctrl {
	/* for framework */
	u8 feature_en;
	u8 sonoff_dvfs_only;	/* mt8173 will change freq, always 1 */
	u8 stay_lv_en;
	u8 vcore_dvs;
	u8 ddr_dfs;
	u8 axi_dfs;
	u8 screen_off;
	u8 mm_off;
	u8 sdio_lock;
	u8 sdio_lv_check;
	u8 lv_autok_trig;	/* META-FT check this to enable LV AutoK */
	u8 lv_autok_abort;
	u8 test_sim_ignore;
	u32 test_sim_prot;	/* RILD pass info to avoid pausing LTE */
	u32 son_opp_index;
	u32 son_dvfs_try;

	/* for OPP table */
	struct dvfs_opp *opp_table;
	u32 num_opps;

	/* for Vcore control */
	u32 curr_vcore;
	u8 sdio_trans_pause;
	u8 dma_dummy_read;

	/* for Freq control */
	u32 curr_ddr_khz;
	u32 curr_axi_khz;
	u32 curr_mm_khz;
	u32 curr_venc_khz;
	u32 curr_vdec_khz;
};

/* NOTE: __nosavedata will not be restored after IPO-H boot */

static struct dvfs_opp opp_table1[] __nosavedata = {
	{			/* OPP 0: performance mode */
	 .vcore = VCORE_1_P_125,
	 .vcore_nml = VCORE_1_P_125,
	 .ddr_khz = FDDR_S1_KHZ,
	 .axi_khz = FAXI_S1_KHZ,
	 .mm_khz = FMM_S1_KHZ,
	 .venc_khz = FVENC_S1_KHZ,
	 .vdec_khz = FVDEC_S1_KHZ,
	 },
	{			/* OPP 1: low power mode */
	 .vcore = VCORE_1_P_0,
	 .vcore_nml = VCORE_1_P_0,
	 .ddr_khz = FDDR_S2_KHZ,
	 .axi_khz = FAXI_S2_KHZ,
	 .mm_khz = FMM_S2_KHZ,
	 .venc_khz = FVENC_S2_KHZ,
	 .vdec_khz = FVDEC_S2_KHZ,
	 }
};

static u32 curr_opp_index __nosavedata;
static u32 prev_opp_index __nosavedata;
static u32 curr_vcore_nml __nosavedata = VCORE_1_P_125;

static struct pwr_ctrl vcorefs_ctrl = {
	.feature_en = 0,
	.sonoff_dvfs_only = 1,
	.vcore_dvs = 1,
	.ddr_dfs = 1,
	.axi_dfs = 1,
	.sdio_lv_check = 1,
	.lv_autok_trig = 1,
	.lv_autok_abort = 1,
	.son_opp_index = 0,
	.son_dvfs_try = 3,
	.opp_table = opp_table1,
	.num_opps = ARRAY_SIZE(opp_table1),
	.curr_vcore = VCORE_1_P_125,
	.sdio_trans_pause = 1,
	.dma_dummy_read = 1,
	.curr_ddr_khz = FDDR_S1_KHZ,
	.curr_axi_khz = FAXI_S1_KHZ,
	.curr_mm_khz = FMM_S1_KHZ,
	.curr_venc_khz = FVENC_S1_KHZ,
	.curr_vdec_khz = FVDEC_S1_KHZ,
};

static struct wake_lock vcorefs_wakelock;
static struct task_struct *vcorefs_ktask;
static atomic_t kthread_nreq = ATOMIC_INIT(0);
static DEFINE_MUTEX(vcorefs_mutex);

/**************************************
 * Vcore Control Function
 **************************************/
struct regulator *reg_vcore_dvfs;	/* 0x027A */
static u32 get_vcore(void)
{				/* mv */
	u32 vcore_volt = 0;
	u32 vcore_pmic = 0;

	if (regulator_is_enabled(reg_vcore_dvfs))
		vcore_volt = regulator_get_voltage(reg_vcore_dvfs);
	else
		vcore_volt = 0;
	pr_err("%s[%d], ANDREW, vcore_volt=%d\n", __func__, __LINE__, vcore_volt);
	vcore_pmic = vcore_uv_to_pmic(vcore_volt);

	return vcore_pmic;
}

static void __update_vcore(struct pwr_ctrl *pwrctrl, int steps)
{
	regulator_set_voltage(reg_vcore_dvfs, pwrctrl->curr_vcore, pwrctrl->curr_vcore + 6250 - 1);

	/* also need to update deep idle table for Vcore restore */
	mt_cpufreq_set_pmic_cmd(PMIC_WRAP_PHASE_DEEPIDLE, IDX_DI_VSRAM_CA7_FAST_TRSN_EN,
				pwrctrl->curr_vcore);
	mt_cpufreq_set_pmic_cmd(PMIC_WRAP_PHASE_DEEPIDLE, IDX_DI_VCORE_PDN_NORMAL,
				pwrctrl->curr_vcore);

	if (pwrctrl->dma_dummy_read) {
		int loops = (DRAM_WINDOW_SHIFT_MAX + (steps - 1)) / steps;
		/* 7 loop * 16 uS, DRAM decide. */
		dma_dummy_read_for_vcorefs(loops);	/* for DQS gating window tracking */
	} else {
		udelay(DVFS_CMD_SETTLE_US);
	}
}

/* mt8173, @ 95, vcore_ao / vcore_pdn cannot diff too much */
static void set_vcore(struct pwr_ctrl *pwrctrl, u32 vcore)
{
	int vcore_step, steps, i;
	int vcore_step_uv;
	int vcore_curr_uv;
	int vcore_uv;

	/* curr_vcore = 0x44, vcore = 0x30, vcore_step = 0x14 */
	vcore_step = abs(vcore - pwrctrl->curr_vcore);
	steps = vcore_step;	/* steps = 0x14 */

#if 1
	vcore_step_uv = vcore_step * VCORE_STEP_UV / steps;	/* vcore_step_uv = 6250 = 6.25mV */

	vcore_step = (vcore >= pwrctrl->curr_vcore ? 1 : -1);	/* +-1 */

	vcore_curr_uv = vcore_pmic_to_uv(pwrctrl->curr_vcore) * vcore_step;	/* (+-)curr_vcore (uv) */

	vcore_uv = vcore_curr_uv;	/* (+-)curr_vcore (uv) */
#endif

	for (i = 0; i < steps; i++) {
		vcore_uv += vcore_step_uv;
		if (vcore_uv > vcore_curr_uv) {
			pwrctrl->curr_vcore += vcore_step;
			vcore_curr_uv += VCORE_STEP_UV;
		}
		pr_err("%s[%d], ANDREW, steps=%d\n", __func__, __LINE__, steps);
		__update_vcore(pwrctrl, steps);
	}

	vcorefs_crit("curr_vcore = 0x%x\n", pwrctrl->curr_vcore);

	BUG_ON(pwrctrl->curr_vcore != vcore);

#if VCORE_SET_CHECK
	vcore = get_vcore();
	vcorefs_debug("vcore = 0x%x\n", vcore);
	BUG_ON(vcore != pwrctrl->curr_vcore_ao);
#endif
}

static int set_vcore_with_opp(struct pwr_ctrl *pwrctrl, struct dvfs_opp *opp)
{
	int sdio_ret = -1;

	pwrctrl->curr_vcore = get_vcore();

	vcorefs_crit("%s[%d], VCoreDVFS, vcore = 0x%x, curr_vcore = 0x%x %s\n", __func__, __LINE__,
		     opp->vcore, pwrctrl->curr_vcore, pwrctrl->vcore_dvs ? "" : "[X]");

	if (pwrctrl->curr_vcore >= VCORE_INVALID)
		return -EBUSY;

	if (opp->vcore == pwrctrl->curr_vcore) {
		curr_vcore_nml = opp->vcore_nml;
		return 0;
	}

	if (!pwrctrl->vcore_dvs)
		return 0;

	if (pwrctrl->sdio_trans_pause)
#if 0				/* VCORE-DVFS-TBD */
		sdio_ret = sdio_stop_transfer();
#else
		pr_err("%s[%d], VCORE-DVFS-TBD, sdio_stop_transfer() @ sd.c\n", __func__, __LINE__);
#endif				/* VCORE-DVFS-TBD */

	set_vcore(pwrctrl, opp->vcore);

	curr_vcore_nml = opp->vcore_nml;

	if (!sdio_ret)
#if 0				/* VCORE-DVFS-TBD */
		sdio_start_ot_transfer();
#else
		pr_err("%s[%d], VCORE-DVFS-TBD, sdio_start_ot_transfer() @ sd.c\n", __func__,
		       __LINE__);
#endif				/* VCORE-DVFS-TBD */

	return 0;
}

/**************************************
 * Freq Control Function
 **************************************/
struct freq_od {
	struct clk *clk;
	struct clk *od_parent;
	struct clk *ptpod_parent;
	unsigned long od_rate;
	unsigned long ptpod_rate;
};

#define PLL_OD(_clk, _od_rate, _ptpod_rate) {		\
	.clk = _clk,					\
	.od_rate = _od_rate,				\
	.ptpod_rate = _ptpod_rate,			\
}

#define MUX_OD(_clk, _od_parent, _ptpod_parent) {	\
	.clk = _clk,					\
	.od_parent = _od_parent,			\
	.ptpod_parent = _ptpod_parent,			\
}

static struct clk *vencpll;
static struct clk *vcodecpll;
static struct clk *mmpll;
static struct clk *axi_sel;
static struct clk *venclt_sel;
static struct clk *cci400_sel;
static struct clk *univpll_d2;
static struct clk *univpll1_d2;
static struct clk *univpll2_d2;
static struct clk *syspll_d2;
static struct clk *syspll1_d2;
static struct clk *vcodecpll_370p5;
static struct clk *ddrphycfg_sel;
static struct clk *syspll1_d8;
static struct clk *clk26m;
static struct clk *scp_sel;

static u32 get_ddr_khz(void)
{
	u32 dram_data_rate = 0;

	dram_data_rate = get_dram_data_rate() * 1000;
	vcorefs_crit("%s[%d], VCoreDVFS, dram_data_rate = %d\n", __func__, __LINE__,
		     dram_data_rate);
	return dram_data_rate;
}

u32 get_axi_khz(void)
{
	return (u32) (clk_get_rate(axi_sel) / 1000);
}

void clkmux_sel_for_vcorefs(bool mode)
{
	struct freq_od ode1[] = {
		PLL_OD(vencpll, 660000000, 800000000),
		PLL_OD(vcodecpll, 1152000000, 1482000000),
		PLL_OD(mmpll, 455000000, 600000000),
		MUX_OD(axi_sel, univpll2_d2, syspll1_d2),
		MUX_OD(venclt_sel, univpll1_d2, vcodecpll_370p5),
		MUX_OD(cci400_sel, syspll_d2, univpll_d2),
	};

	struct freq_od ode2[] = {
		PLL_OD(vencpll, 676000000, 800000000),
		PLL_OD(vcodecpll, 1152000000, 1521000000),
		PLL_OD(mmpll, 455000000, 600000000),
		MUX_OD(axi_sel, univpll2_d2, syspll1_d2),
		MUX_OD(venclt_sel, univpll1_d2, vcodecpll_370p5),
		MUX_OD(cci400_sel, univpll_d2, univpll_d2),
	};

	size_t i, len;
	struct freq_od *od;
	struct freq_od *p;

	od = (mt_get_chip_sw_ver() == CHIP_SW_VER_01) ? ode1 : ode2;
	len = (mt_get_chip_sw_ver() == CHIP_SW_VER_01) ? ARRAY_SIZE(ode1) : ARRAY_SIZE(ode2);

	for (i = 0; i < len; i++) {
		p = &od[i];

		if (IS_ERR_OR_NULL(p->clk))
			continue;

		if (mode) {
			if (p->ptpod_rate)
				clk_set_rate(p->clk, p->ptpod_rate);

			if (!IS_ERR_OR_NULL(p->ptpod_parent))
				clk_set_parent(p->clk, p->ptpod_parent);
		} else {
			if (p->od_rate)
				clk_set_rate(p->clk, p->od_rate);

			if (!IS_ERR_OR_NULL(p->od_parent))
				clk_set_parent(p->clk, p->od_parent);
		}
	}
}

static int set_fddr_with_opp(struct pwr_ctrl *pwrctrl, struct dvfs_opp *opp)
{
	int r;
	u32 spm_flags;

	pwrctrl->curr_ddr_khz = get_ddr_khz();

	vcorefs_crit("%s[%d], VCoreDVFS, ddr = %u, curr_ddr = %u %s\n", __func__, __LINE__,
		     opp->ddr_khz, pwrctrl->curr_ddr_khz, pwrctrl->ddr_dfs ? "" : "[X]");

	if (opp->ddr_khz == pwrctrl->curr_ddr_khz || !pwrctrl->ddr_dfs)
		return 0;

	spm_flags = (pwrctrl->screen_off ? SPM_SCREEN_OFF : 0);
	spm_flags |= (opp->ddr_khz > FDDR_S2_KHZ ? SPM_DDR_HIGH_SPEED : 0);

	r = spm_go_to_ddrdfs(spm_flags, 0);
	if (r)
		return r;

	pwrctrl->curr_ddr_khz = opp->ddr_khz;

#if FDDR_SET_CHECK
	spm_flags = get_ddr_khz();
	vcorefs_debug("hw_ddr = %u\n", spm_flags);
	BUG_ON(spm_flags != pwrctrl->curr_ddr_khz);
#endif

	return 0;
}

static int set_faxi_fxxx_with_opp(struct pwr_ctrl *pwrctrl, struct dvfs_opp *opp)
{
	pwrctrl->curr_axi_khz = get_axi_khz();

	vcorefs_crit("%s[%d], VCoreDVFS, axi = %u, curr_axi = %u %s\n", __func__, __LINE__,
		     opp->axi_khz, pwrctrl->curr_axi_khz, pwrctrl->axi_dfs ? "" : "[X]");

	if (opp->axi_khz == pwrctrl->curr_axi_khz || !pwrctrl->axi_dfs)
		return 0;

	/* change Faxi, Fmm, Fvenc, Fvdec between high/low speed */
	clkmux_sel_for_vcorefs(opp->axi_khz > FAXI_S2_KHZ ? true : false);

	pwrctrl->curr_axi_khz = opp->axi_khz;
	pwrctrl->curr_mm_khz = opp->mm_khz;
	pwrctrl->curr_venc_khz = opp->venc_khz;
	pwrctrl->curr_vdec_khz = opp->vdec_khz;

	return 0;
}


/**************************************
 * Framework Function/API
 **************************************/
static bool is_sdio_lv_ready(struct pwr_ctrl *pwrctrl)
{
	if (!pwrctrl->sdio_lv_check)
		return true;

#ifdef CONFIG_MTK_WCN_CMB_SDIO_SLOT
	return !!autok_is_vol_done(VCORE_1_P_0_UV, CONFIG_MTK_WCN_CMB_SDIO_SLOT);
#else
	return true;
#endif
}

static void trigger_sdio_lv_autok(struct pwr_ctrl *pwrctrl)
{
	if (is_sdio_lv_ready(pwrctrl) || (!pwrctrl->test_sim_ignore && pwrctrl->test_sim_prot))
		return;

#ifdef CONFIG_MTK_WCN_CMB_SDIO_SLOT
	{
		int r = mtk_wcn_cmb_stub_1vautok_for_dvfs();

		if (r)
			vcorefs_err("FAILED TO TRIGGER LV AUTOK (%d)\n", r);
	}
#endif
}

static void abort_sdio_lv_autok(struct pwr_ctrl *pwrctrl)
{
	if (is_sdio_lv_ready(pwrctrl))
		return;

#if 0 /* VCORE-DVFS-TBD */	/* SDIO need provide this API */
	autok_abort_action();
#else
	pr_err("%s[%d], VCORE-DVFS-TBD, autok_abort_action() @ sdio_auto_proc.c\n", __func__,
	       __LINE__);
#endif				/* VCORE-DVFS-TBD */
}

static bool can_dvfs_to_lowpwr_opp(struct pwr_ctrl *pwrctrl, bool in_autok, u32 index)
{
	if (index == 0)		/* skip performance OPP */
		return true;

	if (pwrctrl->sonoff_dvfs_only) {
		if (!pwrctrl->screen_off || !pwrctrl->mm_off) {
			vcorefs_err("ONLY SCREEN-ON/OFF DVFS\n");
			return false;
		}

		if (!pwrctrl->test_sim_ignore && pwrctrl->test_sim_prot) {
			vcorefs_err("TEST SIM PROTECT ENABLED\n");
			return false;
		}
	}

	if (!in_autok && !is_sdio_lv_ready(pwrctrl)) {
		vcorefs_err("SDIO LV IS NOT READY\n");
		return false;
	}

	return true;
}

static int do_dvfs_for_performance(struct pwr_ctrl *pwrctrl, struct dvfs_opp *opp)
{
	int r;

	/* for performance: scale UP voltage -> frequency */

	r = set_vcore_with_opp(pwrctrl, opp);
	if (r)
		return -EBUSY;

	set_faxi_fxxx_with_opp(pwrctrl, opp);

	r = set_fddr_with_opp(pwrctrl, opp);
	if (r)
		return ERR_DDR_DFS;

	return 0;
}

static int do_dvfs_for_low_power(struct pwr_ctrl *pwrctrl, struct dvfs_opp *opp)
{
	int r;

	/* for low power: scale DOWN frequency -> voltage */

	r = set_fddr_with_opp(pwrctrl, opp);
	if (r)
		return -EBUSY;

	set_faxi_fxxx_with_opp(pwrctrl, opp);

	r = set_vcore_with_opp(pwrctrl, opp);
	if (r)
		return ERR_VCORE_DVS;

	return 0;
}

/*
 * return value:
 *   > 0: partial success
 *   = 0: full success
 *   < 0: failure
 */
static int __kick_dvfs_by_index(struct pwr_ctrl *pwrctrl, u32 index)
{
	int err;

	if (index == curr_opp_index) {
		if (index == prev_opp_index)
			return 0;

		/* try again since previous change is partial success */
		curr_opp_index = prev_opp_index;
	}

	/* 0 (performance) <= index <= X (low power) */
	if (index < curr_opp_index)
		err = do_dvfs_for_performance(pwrctrl, &pwrctrl->opp_table[index]);
	else
		err = do_dvfs_for_low_power(pwrctrl, &pwrctrl->opp_table[index]);

	if (err == 0) {
		prev_opp_index = index;
		curr_opp_index = index;
	} else if (err > 0) {
		prev_opp_index = curr_opp_index;
		curr_opp_index = index;
	}

	if (err >= 0 && !pwrctrl->screen_off)
		pwrctrl->son_opp_index = curr_opp_index;

	vcorefs_crit("%s[%d], VCoreDVFS, done: curr_index = %u (%u), err = %d\n", __func__,
		     __LINE__, curr_opp_index, prev_opp_index, err);

	return err;
}

static int kick_dvfs_by_index(struct pwr_ctrl *pwrctrl, enum dvfs_kicker kicker, u32 index)
{
	int err = 0;
	bool in_autok = false;

	vcorefs_crit
	    ("%s[%d], VCoreDVFS, %u kick: sdio_lock = %u, index = %u, curr_index = %u (%u)\n",
	     __func__, __LINE__, kicker, pwrctrl->sdio_lock, index, curr_opp_index, prev_opp_index);

	if (pwrctrl->sdio_lock) {
		/* need performance */
		if (index <= curr_opp_index &&kicker == KR_SCREEN_ON) {
			if (pwrctrl->lv_autok_abort)
				abort_sdio_lv_autok(pwrctrl);
			pwrctrl->sdio_lock = 0;
			goto KICK;
		}

		if (kicker == KR_SDIO_AUTOK) {
			in_autok = true;
			goto KICK;
		}

		if (kicker == KR_SCREEN_ON || kicker == KR_SYSFS)
			return 0;	/* fake success due to BUG check */
		else
			return -EAGAIN;
	}

KICK:
	if (!can_dvfs_to_lowpwr_opp(pwrctrl, in_autok, index))
		return -EPERM;

	err = __kick_dvfs_by_index(pwrctrl, index);

	return err;
}

/**************************************
 * Stay-LV DVFS Function/API
 **************************************/
bool vcorefs_is_stay_lv_enabled(void)
{
	struct pwr_ctrl *pwrctrl = &vcorefs_ctrl;

	if (!pwrctrl->stay_lv_en)
		return false;

	return vcorefs_is_95m_segment();
}

static void __late_init_to_lowpwr_opp(struct pwr_ctrl *pwrctrl)
{
	vcorefs_crit("stay_lv = %u, 95m_seg = %u\n", pwrctrl->stay_lv_en, vcorefs_is_95m_segment());

	if (!pwrctrl->feature_en || !vcorefs_is_stay_lv_enabled())
		return;

	kick_dvfs_by_index(pwrctrl, KR_LATE_INIT, OPPI_LATE_INIT_LP);
}

static int late_init_to_lowpwr_opp(void)
{
	struct pwr_ctrl *pwrctrl = &vcorefs_ctrl;

	mutex_lock(&vcorefs_mutex);
	__late_init_to_lowpwr_opp(pwrctrl);
	mutex_unlock(&vcorefs_mutex);

	return 0;
}

/**************************************
 * SDIO AutoK related API
 **************************************/
/*
 * return value:
 *   = 1: lock at screen-off
 *   = 0: lock at screen-on
 */
int vcorefs_sdio_lock_dvfs(bool in_ot)
{
	int soff;
	struct pwr_ctrl *pwrctrl = &vcorefs_ctrl;

	if (in_ot) {		/* avoid OT thread sleeping in vcorefs_mutex */
		vcorefs_crit("sdio lock: in online-tuning\n");
		return 0;
	}

	mutex_lock(&vcorefs_mutex);
	vcorefs_crit("sdio lock: screen_off = %u, mm_off = %u\n",
		     pwrctrl->screen_off, pwrctrl->mm_off);

	pwrctrl->sdio_lock = 1;
	soff = (pwrctrl->screen_off && pwrctrl->mm_off ? 1 : 0);
	mutex_unlock(&vcorefs_mutex);

	return soff;
}

u32 vcorefs_sdio_get_vcore_nml(void)
{
	return vcore_pmic_to_uv(curr_vcore_nml);
}

int vcorefs_sdio_set_vcore_nml(u32 vcore_uv)
{
	int i, r;
	u32 vcore_nml = vcore_uv_to_pmic(vcore_uv);
	struct pwr_ctrl *pwrctrl = &vcorefs_ctrl;

	mutex_lock(&vcorefs_mutex);
	vcorefs_crit("sdio set: lock = %u, nml = 0x%x, curr_nml = 0x%x\n",
		     pwrctrl->sdio_lock, vcore_nml, curr_vcore_nml);

	if (vcore_nml == curr_vcore_nml) {
		mutex_unlock(&vcorefs_mutex);
		return 0;
	}

	if (!pwrctrl->feature_en || !pwrctrl->sdio_lock) {
		mutex_unlock(&vcorefs_mutex);
		return -EPERM;
	}

	for (i = 0; i < pwrctrl->num_opps; i++) {
		if (vcore_nml == pwrctrl->opp_table[i].vcore_nml)
			break;
	}
	if (i >= pwrctrl->num_opps) {
		mutex_unlock(&vcorefs_mutex);
		return -EINVAL;
	}

	r = kick_dvfs_by_index(pwrctrl, KR_SDIO_AUTOK, i);
	mutex_unlock(&vcorefs_mutex);

	return r;
}

int vcorefs_sdio_unlock_dvfs(bool in_ot)
{
	struct pwr_ctrl *pwrctrl = &vcorefs_ctrl;

	if (in_ot) {		/* avoid OT thread sleeping in vcorefs_mutex */
		vcorefs_crit("sdio unlock: in online-tuning\n");
		return 0;
	}

	mutex_lock(&vcorefs_mutex);
	vcorefs_crit("sdio unlock: lock = %u\n", pwrctrl->sdio_lock);

	if (!pwrctrl->sdio_lock) {
		mutex_unlock(&vcorefs_mutex);
		return -EPERM;
	}

	pwrctrl->sdio_lock = 0;
	__late_init_to_lowpwr_opp(pwrctrl);	/* try to save power */
	mutex_unlock(&vcorefs_mutex);

	return 0;
}

/**************************************
 * Screen-on/off DVFS Function/API
 **************************************/
static void kick_dvfs_after_screen_off(struct pwr_ctrl *pwrctrl)
{
	vcorefs_crit("%s[%d], VCoreDVFS, screen_off - %u, mm_off - %u\n", __func__, __LINE__,
		     pwrctrl->screen_off, pwrctrl->mm_off);

	if (!pwrctrl->feature_en || !pwrctrl->screen_off || !pwrctrl->mm_off)
		return;

	if (pwrctrl->lv_autok_trig)	/* for Vcore 1.0 */
		trigger_sdio_lv_autok(pwrctrl);

	kick_dvfs_by_index(pwrctrl, KR_SCREEN_OFF, OPPI_SCREEN_OFF_LP);
}

static void kick_dvfs_before_screen_on(struct pwr_ctrl *pwrctrl)
{
	int i, r;

	vcorefs_crit("%s[%d], VCoreDVFS, screen_off - %u, mm_off - %u\n", __func__, __LINE__,
		     pwrctrl->screen_off, pwrctrl->mm_off);

	if (!pwrctrl->feature_en || !pwrctrl->screen_off || !pwrctrl->mm_off)
		return;

	for (i = 1; i <= pwrctrl->son_dvfs_try; i++) {
		r = kick_dvfs_by_index(pwrctrl, KR_SCREEN_ON, pwrctrl->son_opp_index);
		if (!r)
			return;
	}

	BUG();			/* screen-on DVFS failed */
}

static int vcorefs_kthread(void *data)
{
	struct pwr_ctrl *pwrctrl = &vcorefs_ctrl;

	while (1) {
		set_current_state(TASK_INTERRUPTIBLE);

		if (kthread_should_stop())
			break;

		if (atomic_read(&kthread_nreq) <= 0) {
			schedule();
			continue;
		}

		wake_lock(&vcorefs_wakelock);

#if 0				/* mt8173 not LTE VCORE-DVFS-TBD */
		if (pwrctrl->screen_off && pwrctrl->mm_off)
			spm_ensure_lte_pause_interval();	/* may sleep */
#endif

		mutex_lock(&vcorefs_mutex);
		kick_dvfs_after_screen_off(pwrctrl);
		atomic_dec(&kthread_nreq);
		mutex_unlock(&vcorefs_mutex);

		wake_unlock(&vcorefs_wakelock);
	}

	__set_current_state(TASK_RUNNING);
	return 0;
}

/* this will be called at MM off */
void vcorefs_clkmgr_notify_mm_off(void)
{

	struct pwr_ctrl *pwrctrl = &vcorefs_ctrl;

	if (IS_ERR_OR_NULL(vcorefs_ktask))
		return;

	pwrctrl->mm_off = 1;
	atomic_inc(&kthread_nreq);
	smp_mb();		/* mm off */

	wake_up_process(vcorefs_ktask);
}

/* this will be called at CLKMGR late resume */
void vcorefs_clkmgr_notify_mm_on(void)
{
	struct pwr_ctrl *pwrctrl = &vcorefs_ctrl;

	pwrctrl->mm_off = 0;
	smp_mb();		/* mm on */
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void vcorefs_early_suspend(struct early_suspend *h)
{
	struct pwr_ctrl *pwrctrl = &vcorefs_ctrl;

	vcorefs_crit("%s[%d], VCoreDVFS", __func__, __LINE__);

	mutex_lock(&vcorefs_mutex);
	pwrctrl->screen_off = 1;
	atomic_inc(&kthread_nreq);
	mutex_unlock(&vcorefs_mutex);

	wake_up_process(vcorefs_ktask);

	vcorefs_crit("%s[%d], VCoreDVFS", __func__, __LINE__);
}

static void vcorefs_late_resume(struct early_suspend *h)
{
	struct pwr_ctrl *pwrctrl = &vcorefs_ctrl;

	vcorefs_crit("%s[%d], VCoreDVFS", __func__, __LINE__);

	mutex_lock(&vcorefs_mutex);
	kick_dvfs_before_screen_on(pwrctrl);
	pwrctrl->screen_off = 0;
	mutex_unlock(&vcorefs_mutex);

	vcorefs_crit("%s[%d], VCoreDVFS", __func__, __LINE__);
}

static struct early_suspend vcorefs_earlysuspend_desc = {
	.level = 9999,		/* should be last one */
	.suspend = vcorefs_early_suspend,
	.resume = vcorefs_late_resume,
};
#endif

/**************************************
 * pwr_ctrl_xxx Function
 **************************************/
static ssize_t pwr_ctrl_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	char *p = buf;

	struct pwr_ctrl *pwrctrl = &vcorefs_ctrl;

	p += sprintf(p, "feature_en = %u\n", pwrctrl->feature_en);
	p += sprintf(p, "sonoff_dvfs_only = %u\n", pwrctrl->sonoff_dvfs_only);
	p += sprintf(p, "stay_lv_en = %u\n", pwrctrl->stay_lv_en);
	p += sprintf(p, "vcore_dvs = %u\n", pwrctrl->vcore_dvs);
	p += sprintf(p, "ddr_dfs = %u\n", pwrctrl->ddr_dfs);
	p += sprintf(p, "axi_dfs = %u\n", pwrctrl->axi_dfs);
	p += sprintf(p, "screen_off = %u\n", pwrctrl->screen_off);
	p += sprintf(p, "mm_off = %u\n", pwrctrl->mm_off);
	p += sprintf(p, "sdio_lock = %u\n", pwrctrl->sdio_lock);
	p += sprintf(p, "sdio_lv_check = %u\n", pwrctrl->sdio_lv_check);
	p += sprintf(p, "lv_autok_trig = %u\n", pwrctrl->lv_autok_trig);
	p += sprintf(p, "lv_autok_abort = %u\n", pwrctrl->lv_autok_abort);
	p += sprintf(p, "test_sim_ignore = %u\n", pwrctrl->test_sim_ignore);
	p += sprintf(p, "test_sim_prot = 0x%x\n", pwrctrl->test_sim_prot);
	p += sprintf(p, "son_opp_index = %u\n", pwrctrl->son_opp_index);
	p += sprintf(p, "son_dvfs_try = %u\n", pwrctrl->son_dvfs_try);

	p += sprintf(p, "opp_table = 0x%p\n", pwrctrl->opp_table);
	p += sprintf(p, "num_opps = %u\n", pwrctrl->num_opps);
	p += sprintf(p, "curr_opp_index = %u\n", curr_opp_index);
	p += sprintf(p, "prev_opp_index = %u\n", prev_opp_index);

	p += sprintf(p, "curr_vcore = 0x%x\n", pwrctrl->curr_vcore);
	p += sprintf(p, "curr_vcore_nml = 0x%x\n", curr_vcore_nml);
	p += sprintf(p, "sdio_trans_pause = %u\n", pwrctrl->sdio_trans_pause);
	p += sprintf(p, "dma_dummy_read = %u\n", pwrctrl->dma_dummy_read);

	p += sprintf(p, "curr_ddr_khz = %u\n", pwrctrl->curr_ddr_khz);
	p += sprintf(p, "curr_axi_khz = %u\n", pwrctrl->curr_axi_khz);
	p += sprintf(p, "curr_mm_khz = %u\n", pwrctrl->curr_mm_khz);
	p += sprintf(p, "curr_venc_khz = %u\n", pwrctrl->curr_venc_khz);
	p += sprintf(p, "curr_vdec_khz = %u\n", pwrctrl->curr_vdec_khz);

	BUG_ON(p - buf >= PAGE_SIZE);

	return p - buf;
}

static ssize_t pwr_ctrl_store(struct kobject *kobj, struct kobj_attribute *attr,
			      const char *buf, size_t count)
{
	u32 val;
	char cmd[32];
	struct pwr_ctrl *pwrctrl = &vcorefs_ctrl;

	if (sscanf(buf, "%31s %x", cmd, &val) != 2)
		return -EPERM;

	vcorefs_crit("pwr_ctrl: cmd = %s, val = 0x%x\n", cmd, val);

	if (!strcmp(cmd, "feature_en")) {
		pwrctrl->feature_en = val;
		pwrctrl->lv_autok_trig = (val && pwrctrl->sdio_lv_check);
	} else if (!strcmp(cmd, "vcore_dvs")) {
		pwrctrl->vcore_dvs = val;
	} else if (!strcmp(cmd, "ddr_dfs")) {
		pwrctrl->ddr_dfs = val;
	} else if (!strcmp(cmd, "axi_dfs")) {
		pwrctrl->axi_dfs = val;
	} else if (!strcmp(cmd, "sdio_lv_check")) {
		pwrctrl->sdio_lv_check = val;
		pwrctrl->lv_autok_trig = (pwrctrl->feature_en && val);
	} else if (!strcmp(cmd, "lv_autok_trig")) {
		pwrctrl->lv_autok_trig = !!val;
	} else if (!strcmp(cmd, "lv_autok_abort")) {
		pwrctrl->lv_autok_abort = val;
	} else if (!strcmp(cmd, "test_sim_ignore")) {
		pwrctrl->test_sim_ignore = val;
	} else if (!strcmp(cmd, "test_sim_prot")) {
		pwrctrl->test_sim_prot = val;
	} else if (!strcmp(cmd, "son_dvfs_try")) {
		pwrctrl->son_dvfs_try = val;
	} else if (!strcmp(cmd, "sdio_trans_pause")) {
		pwrctrl->sdio_trans_pause = val;
	} else if (!strcmp(cmd, "dma_dummy_read")) {
		pwrctrl->dma_dummy_read = val;
	} else {
		return -EINVAL;
	}

	return count;
}


/**************************************
 * opp_table_xxx Function
 **************************************/
static ssize_t opp_table_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int i;
	char *p = buf;

	struct pwr_ctrl *pwrctrl = &vcorefs_ctrl;
	struct dvfs_opp *opp;

	for (i = 0; i < pwrctrl->num_opps; i++) {
		opp = &pwrctrl->opp_table[i];

		p += sprintf(p, "[OPP %d]\n", i);
		p += sprintf(p, "vcore = 0x%x\n", opp->vcore);
		p += sprintf(p, "vcore_nml = 0x%x\n", opp->vcore_nml);
		p += sprintf(p, "ddr_khz = %u\n", opp->ddr_khz);
		p += sprintf(p, "axi_khz = %u\n", opp->axi_khz);
		p += sprintf(p, "mm_khz = %u\n", opp->mm_khz);
		p += sprintf(p, "venc_khz = %u\n", opp->venc_khz);
		p += sprintf(p, "vdec_khz = %u\n", opp->vdec_khz);
	}

	BUG_ON(p - buf >= PAGE_SIZE);

	return p - buf;
}

static ssize_t opp_table_store(struct kobject *kobj, struct kobj_attribute *attr,
			       const char *buf, size_t count)
{
	u32 val1, val2;
	char cmd[32];
	struct pwr_ctrl *pwrctrl = &vcorefs_ctrl;
	struct dvfs_opp *opp;

	if (sscanf(buf, "%31s %x %x", cmd, &val1, &val2) != 3)
		return -EPERM;

	vcorefs_debug("opp_table: cmd = %s, val1 = 0x%x, val2 = 0x%x\n", cmd, val1, val2);

	if (val1 >= pwrctrl->num_opps)
		return -EINVAL;

	opp = &pwrctrl->opp_table[val1];

	if (!strcmp(cmd, "vcore") && val2 < VCORE_INVALID)
		opp->vcore = val2;
	else if (!strcmp(cmd, "vcore_nml") && val2 < VCORE_INVALID)
		opp->vcore_nml = val2;
	else if (!strcmp(cmd, "ddr_khz") && val2 > FREQ_INVALID)
		opp->ddr_khz = val2;
	else if (!strcmp(cmd, "axi_khz") && val2 > FREQ_INVALID)
		opp->axi_khz = val2;
	else if (!strcmp(cmd, "mm_khz") && val2 > FREQ_INVALID)
		opp->mm_khz = val2;
	else if (!strcmp(cmd, "venc_khz") && val2 > FREQ_INVALID)
		opp->venc_khz = val2;
	else if (!strcmp(cmd, "vdec_khz") && val2 > FREQ_INVALID)
		opp->vdec_khz = val2;
	else
		return -EINVAL;

	return count;
}


/**************************************
 * vcore_debug_xxx Function
 **************************************/
static ssize_t vcore_debug_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	u32 vcore;
	char *p = buf;

	vcore = get_vcore();

	p += sprintf(p, "VCORE = %u (0x%x)\n", vcore_pmic_to_uv(vcore), vcore);
	p += sprintf(p, "FDDR = %u\n", get_ddr_khz());
	p += sprintf(p, "FAXI = %u\n", get_axi_khz());

	BUG_ON(p - buf >= PAGE_SIZE);

	return p - buf;
}

static ssize_t vcore_debug_store(struct kobject *kobj, struct kobj_attribute *attr,
				 const char *buf, size_t count)
{
	int r;
	u32 val;
	char cmd[32];
	struct pwr_ctrl *pwrctrl = &vcorefs_ctrl;

	if (sscanf(buf, "%31s %x", cmd, &val) != 2)
		return -EPERM;

	vcorefs_debug("vcore_debug: cmd = %s, val = 0x%x\n", cmd, val);

	if (!strcmp(cmd, "opp_vcore_set") && val < pwrctrl->num_opps) {
		mutex_lock(&vcorefs_mutex);
		if (pwrctrl->feature_en && pwrctrl->screen_off && pwrctrl->mm_off) {
			r = kick_dvfs_by_index(pwrctrl, KR_SYSFS, val);
			BUG_ON(r);
		}
		mutex_unlock(&vcorefs_mutex);
	} else if (!strcmp(cmd, "opp_vcore_setX") && val < pwrctrl->num_opps) {
		mutex_lock(&vcorefs_mutex);
		if (pwrctrl->feature_en) {
			u8 orig = pwrctrl->sonoff_dvfs_only;

			pwrctrl->sonoff_dvfs_only = 0;
			r = kick_dvfs_by_index(pwrctrl, KR_SYSFS, val);
			BUG_ON(r);
			pwrctrl->sonoff_dvfs_only = orig;
		}
		mutex_unlock(&vcorefs_mutex);
	} else {
		return -EINVAL;
	}

	return count;
}

/**************************************
 * Init Function
 **************************************/
DEFINE_ATTR_RW(pwr_ctrl);
DEFINE_ATTR_RW(opp_table);
DEFINE_ATTR_RW(vcore_debug);

static struct attribute *vcorefs_attrs[] = {
	__ATTR_OF(pwr_ctrl),
	__ATTR_OF(opp_table),
	__ATTR_OF(vcore_debug),
	NULL,
};

static struct attribute_group vcorefs_attr_group = {
	.name = "vcorefs",
	.attrs = vcorefs_attrs,
};

static int create_vcorefs_kthread(void)
{
	struct sched_param param = {.sched_priority = MAX_RT_PRIO - 1 };

	vcorefs_ktask = kthread_create(vcorefs_kthread, NULL, "vcorefs");
	if (IS_ERR(vcorefs_ktask))
		return PTR_ERR(vcorefs_ktask);

	sched_setscheduler_nocheck(vcorefs_ktask, SCHED_FIFO, &param);
	get_task_struct(vcorefs_ktask);
	wake_up_process(vcorefs_ktask);

	return 0;
}

/* 95m will fix ddr at 1333MHz, mt8173 is not */
bool vcorefs_is_95m_segment(void)
{
/* 95m will fix ddr at 1333MHz, mt8173 is not */
#if 1
	return false;
#else
#if defined(CONFIG_MTK_WQHD) || defined(MTK_DISABLE_EFUSE)
	return false;
#else
	u32 func;
	struct pwr_ctrl *pwrctrl = &vcorefs_ctrl;

	if (!pwrctrl->feature_en)
		return false;	/* make SDIO only do 1.125V AutoK */

	func = get_devinfo_with_index(24);
	func = (func >> 24) & 0xf;

	if (func >= 6 && func <= 10)	/* 95 segment */
		return false;

	return true;
#endif
#endif
}

static int init_vcorefs_pwrctrl(void)
{
	struct pwr_ctrl *pwrctrl = &vcorefs_ctrl;
	struct dvfs_opp *opp;

	mutex_lock(&vcorefs_mutex);
	pwrctrl->curr_vcore = get_vcore();
	BUG_ON(pwrctrl->curr_vcore >= VCORE_INVALID);

	pwrctrl->curr_ddr_khz = get_ddr_khz();
	pwrctrl->curr_axi_khz = get_axi_khz();

	opp = &pwrctrl->opp_table[0];
	opp->vcore = pwrctrl->curr_vcore;	/* by PTPOD */
	opp->ddr_khz = pwrctrl->curr_ddr_khz;	/* E1: 1600M, E2: 1792M, M: 1333M */
	opp->axi_khz = pwrctrl->curr_axi_khz;
	vcorefs_crit("OPP 0: vcore = 0x%x, ddr_khz = %u, axi_khz = %u\n",
		     opp->vcore, opp->ddr_khz, opp->axi_khz);

	if (vcorefs_is_95m_segment()) {
		BUG_ON(opp->ddr_khz != FDDR_S2_KHZ);	/* violate spec */
		pwrctrl->sonoff_dvfs_only = 0;
		pwrctrl->lv_autok_trig = 0;
		pwrctrl->stay_lv_en = 1;
	} else {
		/*pwrctrl->feature_en = 0; */
		pwrctrl->lv_autok_trig = (pwrctrl->feature_en && pwrctrl->sdio_lv_check);
	}

	mutex_unlock(&vcorefs_mutex);

	return 0;
}

#ifdef CONFIG_OF
int vcore_dvfs_config_speed(int hispeed)
{
	pr_err("%s[%d], hispeed = %d\n", __func__, __LINE__, hispeed);

	if (hispeed) {
		clk_prepare_enable(ddrphycfg_sel);
		clk_set_parent(ddrphycfg_sel, syspll1_d8);
		clk_disable_unprepare(ddrphycfg_sel);

		clk_prepare_enable(scp_sel);
		clk_set_parent(scp_sel, syspll1_d2);
		clk_disable_unprepare(scp_sel);
	} else {
		clk_prepare_enable(ddrphycfg_sel);
		clk_set_parent(ddrphycfg_sel, clk26m);
		clk_disable_unprepare(ddrphycfg_sel);
	}
	return 0;
}

#endif

static int mt_vcoredvfs_pdrv_probe(struct platform_device *pdev)
{
#ifdef CONFIG_OF
	struct clkname {
		struct clk **pclk;
		const char *name;
	} clks[] = {
		{
		&vencpll, "vencpll"}, {
		&vcodecpll, "vcodecpll"}, {
		&mmpll, "mmpll"}, {
		&axi_sel, "axi_sel"}, {
		&venclt_sel, "venclt_sel"}, {
		&cci400_sel, "cci400_sel"}, {
		&univpll_d2, "univpll_d2"}, {
		&univpll1_d2, "univpll1_d2"}, {
		&univpll2_d2, "univpll2_d2"}, {
		&syspll_d2, "syspll_d2"}, {
		&syspll1_d2, "syspll1_d2"}, {
		&vcodecpll_370p5, "vcodecpll_370p5"}, {
		&ddrphycfg_sel, "ddrphycfg_sel"}, {
		&syspll1_d8, "syspll1_d8"}, {
		&clk26m, "clk26m"}, {
	&scp_sel, "scp_sel"},};

	size_t i;
	int ret;

	reg_vcore_dvfs = devm_regulator_get(&pdev->dev, "reg-vcore");
	if (IS_ERR(reg_vcore_dvfs)) {
		ret = PTR_ERR(reg_vcore_dvfs);
		dev_err(&pdev->dev, "Failed to request reg-vcore: %d\n", ret);
		return ret;
	}
	ret = regulator_enable(reg_vcore_dvfs);

	for (i = 0; i < ARRAY_SIZE(clks); i++) {
		*clks[i].pclk = devm_clk_get(&pdev->dev, clks[i].name);
		if (IS_ERR(*clks[i].pclk))
			dev_warn(&pdev->dev, "lookup clk fail: %s\n", clks[i].name);
	}
#endif
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mtvcoredvfs_of_match[] = {
	{.compatible = "mediatek,MT_VCOREDVFS",},
	{},
};

MODULE_DEVICE_TABLE(of, mtvcoredvfs_of_match);
#endif

static struct platform_driver mt_vcoredvfs_pdrv = {
	.probe = mt_vcoredvfs_pdrv_probe,
	.driver = {
		   .name = "mt-vcoredvfs",
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(mtvcoredvfs_of_match),
#endif
		   .owner = THIS_MODULE,
		   },
};

static int __init vcorefs_module_init(void)
{
	int r = 0;

	r = platform_driver_register(&mt_vcoredvfs_pdrv);
	if (r)
		vcorefs_err("fail to register vcore-dvfs driver @ %s()\n", __func__);

	r = init_vcorefs_pwrctrl();
	if (r) {
		vcorefs_err("FAILED TO INIT PWRCTRL (%d)\n", r);
		return r;
	}

	wake_lock_init(&vcorefs_wakelock, WAKE_LOCK_SUSPEND, "vcorefs");

	r = create_vcorefs_kthread();	/* for screen-off DVFS */
	if (r) {
		vcorefs_err("FAILED TO CREATE KTHREAD (%d)\n", r);
		return r;
	}
#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&vcorefs_earlysuspend_desc);	/* for screen-on DVFS */
#endif

	r = sysfs_create_group(power_kobj, &vcorefs_attr_group);
	if (r)
		vcorefs_err("FAILED TO CREATE /sys/power/vcorefs (%d)\n", r);

	return r;
}

module_init(vcorefs_module_init);
late_initcall_sync(late_init_to_lowpwr_opp);

MODULE_DESCRIPTION("Vcore DVFS Driver v1.0");
