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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/mutex.h>

#include <linux/fb.h>
#include <linux/notifier.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#endif

#include <mach/mtk_pmic_wrap.h>

#include "spm_v2/mtk_spm_pmic_wrap.h"
#include "spm_v2/mtk_spm_internal.h"
#include "mtk_vcorefs_manager.h"
#include "mtk_spm_vcore_dvfs.h"
#include "mtk_dramc.h"
#include "mtk_eem.h"
#include "mtk_ts_setting.h"

/*
 * AutoK options
 */
#define AUTOK_SD	1
#define AUTOK_EMMC	1
#define	AUTOK_SDIO	1

/*
 * __nosavedata will not be restored after IPO-H boot
 */
static int vcorefs_curr_opp __nosavedata = OPPI_PERF;

__weak int emmc_autok(void)
{
	return 0;
}

__weak int sd_autok(void)
{
	return 0;
}

__weak int sdio_autok(void)
{
	return 0;
}

/*
 * PMIC
 */
#if defined(CONFIG_MTK_PMIC_CHIP_MT6355)
#define PMIC_VCORE_ADDR PMIC_RG_BUCK_VCORE_VOSEL_ADDR
#define VCORE_BASE_UV		406250
#else
#define PMIC_VCORE_ADDR MT6351_PMIC_BUCK_VCORE_VOSEL_ON_ADDR
#define VCORE_BASE_UV		600000
#endif

#define VCORE_STEP_UV		6250

#define VCORE_PRCISE_ADJUST_UV (VCORE_STEP_UV * 3)

#define VCORE_INVALID		0x80

#define vcore_uv_to_pmic(uv)	/* pmic >= uv */	\
	((((uv) - VCORE_BASE_UV) + (VCORE_STEP_UV - 1)) / VCORE_STEP_UV)

#define vcore_pmic_to_uv(pmic)	\
	(((pmic) * VCORE_STEP_UV) + VCORE_BASE_UV)

/*
 *  sram debug info
 */
void __iomem *vcorefs_sram_base;
/*
 * struct define
 */
static DEFINE_MUTEX(governor_mutex);

struct governor_profile {
	bool plat_feature_en;
	bool vcore_dvs;
	bool ddr_dfs;
	bool sdio_dvfs;
	int  screen_off_ulpm;

	int late_init_opp;
	bool plat_init_done;

	int curr_vcore_uv;
	int curr_ddr_khz;

	u32 active_autok_kir;
	u32 autok_kir_group;
	u32 cpu_dvfs_req;
	u32 ddr_type;
	u32 segment_code;
	bool precise_lpm_vcore;
	bool emi_grouping;
	bool dfd_wakeup_src;

	bool perform_bw_enable;
	u32 perform_bw_ulpm_threshold;
	u32 perform_bw_lpm_threshold;
	u32 perform_bw_hpm_threshold;

	bool total_bw_enable;
	u32 total_bw_ulpm_threshold;
	u32 total_bw_lpm_threshold;
	u32 total_bw_hpm_threshold;

	bool ulpm_perform_bw_enable;
	bool ulpm_total_bw_enable;
};

static struct governor_profile governor_ctrl = {
	.plat_feature_en = 1, /* vcore dvfs enable */
	.vcore_dvs = 1,
	.ddr_dfs = 1,
	.screen_off_ulpm = 1,

	.late_init_opp = OPPI_LOW_PWR,
	.plat_init_done = 0,

	.active_autok_kir = 0,
	.autok_kir_group = ((1 << KIR_AUTOK_SDIO) | (1 << KIR_AUTOK_EMMC) | (1 << KIR_AUTOK_SD)),
	.cpu_dvfs_req = 0,
	.ddr_type = 0,

	.segment_code = 0,
	.precise_lpm_vcore = 0,
	.emi_grouping = 0,
	.dfd_wakeup_src = 0,

	.curr_vcore_uv = VCORE_0_P_80_UV,
	.curr_ddr_khz = FDDR_S0_KHZ,

	.perform_bw_enable = 0,
	.perform_bw_ulpm_threshold = 0,
	.perform_bw_lpm_threshold = 0,
	.perform_bw_hpm_threshold = 0,

	.total_bw_enable = 0,
	.total_bw_ulpm_threshold = 0,
	.total_bw_lpm_threshold = 0,
	.total_bw_hpm_threshold = 0,
	.ulpm_perform_bw_enable = 0,
	.ulpm_total_bw_enable	= 0,
};

int kicker_table[LAST_KICKER] __nosavedata;

static struct opp_profile opp_table[] __nosavedata = {
	/* performance mode */
	[OPP_0] = {
		.vcore_uv	= VCORE_0_P_80_UV,
		.ddr_khz	= FDDR_S0_KHZ,
	},
	/* low power mode */
	[OPP_1] = {
		.vcore_uv	= VCORE_0_P_70_UV,
		.ddr_khz	= FDDR_S1_KHZ,
	},
	/* ultra low power mode */
	[OPP_2] = {
		.vcore_uv	= VCORE_0_P_70_UV,
		.ddr_khz	= FDDR_S2_KHZ,
	}
};

static char *kicker_name[] = {
	"KIR_MM",
	"KIR_MM_NON_FORCE",
	"KIR_AUDIO",
	"KIR_PERF",
	"KIR_SYSFS",
	"KIR_SYSFS_N",
	"KIR_GPU",
	"KIR_CPU",
	"KIR_THERMAL",
	"KIR_FB",
	"KIR_FBT",
	"KIR_BOOTUP",
	"NUM_KICKER",

	"KIR_LATE_INIT",
	"KIR_SYSFSX",
	"KIR_AUTOK_EMMC",
	"KIR_AUTOK_SDIO",
	"KIR_AUTOK_SD",
	"LAST_KICKER",
};

int bootup_kicker_init_opp = OPPI_ULTRA_LOW_PWR;

int vcorefs_bootup_get_init_opp(void)
{
	return bootup_kicker_init_opp;
}

void vcorefs_bootup_set_init_opp(int opp)
{
	vcorefs_info("bootup_set_init_opp(%d)\n", opp);
	bootup_kicker_init_opp = opp;
}
bool vcorefs_request_init_opp(int kicker, int opp)
{
	bool accept = false;

	mutex_lock(&governor_mutex);
	if (kicker == KIR_BOOTUP) {
		bootup_kicker_init_opp = opp;
		vcorefs_crit_mask(log_mask(), kicker, "init_opp request(kr:%d, opp:%d)\n", kicker, opp);
		accept = true;
	}
	mutex_unlock(&governor_mutex);
	return accept;
}

/*
 * set vcore cmd
 */
static void update_vcore_pwrap_cmd(struct opp_profile *opp_ctrl_table)
{
	u32 hpm, lpm, ulpm;

	hpm		= vcore_uv_to_pmic(opp_ctrl_table[OPP_0].vcore_uv);
	lpm		= vcore_uv_to_pmic(opp_ctrl_table[OPP_1].vcore_uv);
	ulpm		= vcore_uv_to_pmic(opp_ctrl_table[OPP_2].vcore_uv);

	/* PMIC_WRAP_PHASE_NORMAL */
	mt_spm_pmic_wrap_set_cmd(PMIC_WRAP_PHASE_NORMAL, IDX_NM_VCORE_HPM, hpm);
	mt_spm_pmic_wrap_set_cmd(PMIC_WRAP_PHASE_NORMAL, IDX_NM_VCORE_LPM, lpm);
	mt_spm_pmic_wrap_set_cmd(PMIC_WRAP_PHASE_NORMAL, IDX_NM_VCORE_ULPM, ulpm);

	/* PMIC_WRAP_PHASE_SUSPEND */
	mt_spm_pmic_wrap_set_cmd(PMIC_WRAP_PHASE_SUSPEND, IDX_SP_VCORE_HPM, hpm);
	mt_spm_pmic_wrap_set_cmd(PMIC_WRAP_PHASE_SUSPEND, IDX_SP_VCORE_LPM, lpm);
	mt_spm_pmic_wrap_set_cmd(PMIC_WRAP_PHASE_NORMAL, IDX_NM_VCORE_ULPM, ulpm);

	/* PMIC_WRAP_PHASE_DEEPIDLE */
	mt_spm_pmic_wrap_set_cmd(PMIC_WRAP_PHASE_DEEPIDLE, IDX_DI_VCORE_HPM, hpm);
	mt_spm_pmic_wrap_set_cmd(PMIC_WRAP_PHASE_DEEPIDLE, IDX_DI_VCORE_LPM, lpm);
	mt_spm_pmic_wrap_set_cmd(PMIC_WRAP_PHASE_NORMAL, IDX_NM_VCORE_ULPM, ulpm);

	vcorefs_crit("HPM   : %u (0x%x)\n", opp_ctrl_table[OPP_0].vcore_uv, hpm);
	vcorefs_crit("LPM   : %u (0x%x)\n", opp_ctrl_table[OPP_1].vcore_uv, lpm);
	vcorefs_crit("ULPM  : %u (0x%x)\n", opp_ctrl_table[OPP_2].vcore_uv, ulpm);
}

void vcorefs_update_opp_table(char *cmd, int val)
{
	struct opp_profile *opp_ctrl_table = opp_table;
	int uv = vcore_pmic_to_uv(val);

	if (!strcmp(cmd, "HPM") && val < VCORE_INVALID) {
		if (uv >= opp_ctrl_table[OPPI_LOW_PWR].vcore_uv) {
			opp_ctrl_table[OPPI_PERF].vcore_uv = uv;
			update_vcore_pwrap_cmd(opp_ctrl_table);
		}
	} else if (!strcmp(cmd, "LPM") && val < VCORE_INVALID) {
		if (uv <= opp_ctrl_table[OPPI_PERF].vcore_uv && uv >= opp_ctrl_table[OPPI_ULTRA_LOW_PWR].vcore_uv) {
			opp_ctrl_table[OPPI_LOW_PWR].vcore_uv = uv;
			update_vcore_pwrap_cmd(opp_ctrl_table);
		}
	} else if (!strcmp(cmd, "ULPM") && val < VCORE_INVALID) {
		if (uv <= opp_ctrl_table[OPPI_LOW_PWR].vcore_uv) {
			opp_ctrl_table[OPPI_ULTRA_LOW_PWR].vcore_uv = uv;
			update_vcore_pwrap_cmd(opp_ctrl_table);
		}
	}
}

/*
 * Governor extern API
 */
bool is_vcorefs_feature_enable(void)
{
	struct governor_profile *gvrctrl = &governor_ctrl;

	if (!dram_can_support_fh()) {
		vcorefs_err("DISABLE DVFS DUE TO NOT SUPPORT DRAM FH\n");
		return false;
	}
	if (!gvrctrl->vcore_dvs && !gvrctrl->ddr_dfs) {
		vcorefs_err("DISABLE DVFS DUE TO BOTH DVS & DFS disabled\n");
		return false;
	}

	return gvrctrl->plat_feature_en;
}

int vcorefs_get_num_opp(void)
{
	return NUM_OPP;
}

int vcorefs_get_hw_opp(void)
{
	return spm_vcorefs_get_opp();
}

int vcorefs_get_sw_opp(void)
{
	return vcorefs_curr_opp;
}

int vcorefs_get_curr_vcore(void)
{
	int vcore = VCORE_INVALID;

	pwrap_read(PMIC_VCORE_ADDR, &vcore);
	if (vcore >= VCORE_INVALID)
		pwrap_read(PMIC_VCORE_ADDR, &vcore);

	return vcore < VCORE_INVALID ? vcore_pmic_to_uv(vcore) : 0;
}
EXPORT_SYMBOL(vcorefs_get_curr_vcore);

int vcorefs_get_curr_ddr(void)
{
	int ddr_khz;

	ddr_khz = get_dram_data_rate() * 1000;

	return ddr_khz;
}
int vcorefs_get_vcore_by_steps(u32 steps)
{
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	switch (steps) {
	case OPP_0:
		return vcore_pmic_to_uv(get_vcore_ptp_volt(OPP_0));
	break;
	case OPP_1:
		return vcore_pmic_to_uv(get_vcore_ptp_volt(OPP_1));
	break;
	case OPP_2:
		return vcore_pmic_to_uv(get_vcore_ptp_volt(OPP_2));
	break;
	default:
		break;
	}
#endif
	return 0;
}

int vcorefs_get_ddr_by_steps(u32 steps)
{
	int ddr_khz;

	ddr_khz = dram_steps_freq(steps) * 1000;

	WARN_ON(ddr_khz < 0);

	return ddr_khz;
}
char *governor_get_kicker_name(int id)
{
	return kicker_name[id];
}

char *vcorefs_get_opp_table_info(char *p)
{
	struct opp_profile *opp_ctrl_table = opp_table;
	int i;
	char *buff_end = p + PAGE_SIZE;

	for (i = 0; i < NUM_OPP; i++) {
		p += snprintf(p, buff_end - p,
				"[OPP_%d] vcore_uv: %d (0x%x)\n", i, opp_ctrl_table[i].vcore_uv,
				vcore_uv_to_pmic(opp_ctrl_table[i].vcore_uv));
		p += snprintf(p, buff_end - p,
				"[OPP_%d] ddr_khz : %d\n", i, opp_ctrl_table[i].ddr_khz);
		p += snprintf(p, buff_end - p, "\n");
	}

	p += snprintf(p, buff_end - p, "HPM   : %u\n", opp_ctrl_table[OPP_0].vcore_uv);
	p += snprintf(p, buff_end - p, "LPM   : %u\n", opp_ctrl_table[OPP_1].vcore_uv);
	p += snprintf(p, buff_end - p, "ULPM  : %u\n", opp_ctrl_table[OPP_2].vcore_uv);

	return p;
}

int vcorefs_output_kicker_id(char *name)
{
	int i;

	for (i = 0; i < NUM_KICKER; i++) {
		if (!strcmp(kicker_name[i], name))
			return i;
	}

	return -1;
}

int vcorefs_get_dvfs_kicker_group(int kicker)
{
	int group = KIR_GROUP_HPM;

	switch (kicker) {
	case KIR_MM_NON_FORCE:
	case KIR_SYSFS_N:
		group = KIR_GROUP_HPM_NON_FORCE;
	break;
	case KIR_SYSFSX:
	case KIR_AUTOK_SDIO:
	case KIR_AUTOK_EMMC:
	case KIR_AUTOK_SD:
		group = KIR_GROUP_FIX;
	break;
	case KIR_MM:
	case KIR_PERF:
	case KIR_AUDIO:
	case KIR_SYSFS:
	case KIR_LATE_INIT:
	case KIR_GPU:
	case KIR_CPU:
	case KIR_THERMAL:
	case KIR_FB:
	case KIR_FBT:
	case KIR_BOOTUP:
	default:
		group = KIR_GROUP_HPM;
	break;
	}
	return group;
}

int vcorefs_get_kicker_opp(int id)
{
	int opp = -1;

	if (id < LAST_KICKER)
		opp = kicker_table[id];

	return opp;
}

/*
 * init boot-up OPP from late init
 */
static int set_init_opp_index(void)
{
	/* add condition here for diff late_init_opp */
	return OPPI_LOW_PWR;
}

/*
 *  sram debug info
 */
#define SRAM_DEBUG_COUNT 9
static u32 sram_debug_info[SRAM_DEBUG_COUNT];
static char *vcorefs_get_sram_debug_info(char *p)
{
	char *buff_end = p + PAGE_SIZE;

	if (p) {
		p += snprintf(p, buff_end - p,
				"dvfs  up/down count: %u / %u\n",
				spm_read(VCOREFS_SRAM_DVFS_UP_COUNT),
				spm_read(VCOREFS_SRAM_DVFS_DOWN_COUNT));
		p += snprintf(p, buff_end - p,
				"dvfs2 up/down count: %u / %u\n",
				spm_read(VCOREFS_SRAM_DVFS2_UP_COUNT),
				spm_read(VCOREFS_SRAM_DVFS2_DOWN_COUNT));
		p += snprintf(p, buff_end - p,
				"dvfs  up/down time: %u / %u\n",
				spm_read(VCOREFS_SRAM_DVFS_UP_TIME),
				spm_read(VCOREFS_SRAM_DVFS_DOWN_TIME));
		p += snprintf(p, buff_end - p,
				"dvfs2 up/down time: %u / %u\n",
				spm_read(VCOREFS_SRAM_DVFS2_UP_TIME),
				spm_read(VCOREFS_SRAM_DVFS2_DOWN_TIME));
		p += snprintf(p, buff_end - p,
				"emi block time: %u\n",
				spm_read(VCOREFS_SRAM_EMI_BLOCK_TIME));

	} else {
		sram_debug_info[0] = spm_read(VCOREFS_SRAM_DVFS_UP_COUNT);
		sram_debug_info[1] = spm_read(VCOREFS_SRAM_DVFS_DOWN_COUNT);
		sram_debug_info[2] = spm_read(VCOREFS_SRAM_DVFS2_UP_COUNT);
		sram_debug_info[3] = spm_read(VCOREFS_SRAM_DVFS2_DOWN_COUNT);
		sram_debug_info[4] = spm_read(VCOREFS_SRAM_DVFS_UP_TIME);
		sram_debug_info[5] = spm_read(VCOREFS_SRAM_DVFS_DOWN_TIME);
		sram_debug_info[6] = spm_read(VCOREFS_SRAM_DVFS2_UP_TIME);
		sram_debug_info[7] = spm_read(VCOREFS_SRAM_DVFS2_DOWN_TIME);
		sram_debug_info[8] = spm_read(VCOREFS_SRAM_EMI_BLOCK_TIME);
		vcorefs_crit("dvfs count: %d %d %d %d\n",
				sram_debug_info[0], sram_debug_info[1], sram_debug_info[2], sram_debug_info[3]);
		vcorefs_crit("dvfs time: %d %d %d %d %d\n",
				sram_debug_info[4], sram_debug_info[5], sram_debug_info[6], sram_debug_info[7],
				sram_debug_info[8]);
	}
	return p;
}

static void vcorefs_set_sram_data(int index, u32 data)
{
	spm_write(VCOREFS_SRAM_BASE + index * 4, data);
}

static void vcorefs_init_sram_debug(void)
{
	int i;

	if (!vcorefs_sram_base) {
		vcorefs_err("vcorefs_sram_base is not valid\n");
		return;
	}

	vcorefs_get_sram_debug_info(NULL);

	vcorefs_crit("clean debug sram info\n");
	for (i = 0; i < 32; i++)
		vcorefs_set_sram_data(i, 0);
}

u32 get_vcore_dvfs_sram_debug_regs(uint32_t index)
{
	u32 value = 0;

	if (index == 0)
		value = 0; /* SRAM_DEBUG_COUNT */
	else if (index < SRAM_DEBUG_COUNT)
		value = sram_debug_info[index-1];
	else
		vcorefs_err("out of sram debug count(%d)\n", index);

	return value;
}

/*
 *  governor debug sysfs
 */
static void vcorefs_set_cpu_dvfs_req(u32 value)
{

	struct governor_profile *gvrctrl = &governor_ctrl;

	u32 mask = 0xFFFF;

	mutex_lock(&governor_mutex);
	gvrctrl->cpu_dvfs_req = (value & mask);

	spm_vcorefs_set_cpu_dvfs_req(value, mask);

	mutex_unlock(&governor_mutex);
}

static int vcorefs_check_feature_enable(void)
{
	struct governor_profile *gvrctrl = &governor_ctrl;
	int flag;

	flag = SPM_FLAG_RUN_COMMON_SCENARIO;

	if (!gvrctrl->plat_feature_en) {
		flag |= (SPM_FLAG_DIS_VCORE_DVS | SPM_FLAG_DIS_VCORE_DFS);
	} else {
		if (!gvrctrl->vcore_dvs)
			flag |= SPM_FLAG_DIS_VCORE_DVS;
		if (!gvrctrl->ddr_dfs)
			flag |= SPM_FLAG_DIS_VCORE_DFS;
	}

	return flag;
}

static int vcorefs_enable_vcore(bool enable)
{
	struct governor_profile *gvrctrl = &governor_ctrl;
	u32 flag = SPM_FLAG_RUN_COMMON_SCENARIO;

	mutex_lock(&governor_mutex);

	gvrctrl->vcore_dvs = enable;

	if (!gvrctrl->vcore_dvs)
		flag |= SPM_FLAG_DIS_VCORE_DVS;

	if (!gvrctrl->ddr_dfs)
		flag |= SPM_FLAG_DIS_VCORE_DFS;

	spm_go_to_vcore_dvfs(flag, 0);

	mutex_unlock(&governor_mutex);
	return 0;
}

static int vcorefs_enable_ddr(bool enable)
{
	struct governor_profile *gvrctrl = &governor_ctrl;
	u32 flag = SPM_FLAG_RUN_COMMON_SCENARIO;

	mutex_lock(&governor_mutex);

	gvrctrl->ddr_dfs = enable;

	if (!gvrctrl->vcore_dvs)
		flag |= SPM_FLAG_DIS_VCORE_DVS;

	if (!gvrctrl->ddr_dfs)
		flag |= SPM_FLAG_DIS_VCORE_DFS;

	spm_go_to_vcore_dvfs(flag, 0);

	mutex_unlock(&governor_mutex);
	return 0;
}

static int init_vcorefs_cmd_table(void)
{
	struct governor_profile *gvrctrl = &governor_ctrl;
	struct opp_profile *opp_ctrl_table = opp_table;
	int opp;

	mutex_lock(&governor_mutex);
	gvrctrl->curr_vcore_uv = vcorefs_get_curr_vcore();
	/* BUG_ON(gvrctrl->curr_vcore_uv == 0); */

	gvrctrl->curr_ddr_khz = vcorefs_get_curr_ddr();

	for (opp = 0; opp < NUM_OPP; opp++) {
		switch (opp) {
		case OPPI_PERF:
			opp_ctrl_table[opp].vcore_uv = vcorefs_get_vcore_by_steps(opp);
			opp_ctrl_table[opp].ddr_khz = vcorefs_get_ddr_by_steps(opp);
			break;
		case OPPI_LOW_PWR:
			opp_ctrl_table[opp].vcore_uv = vcorefs_get_vcore_by_steps(opp);
			if (gvrctrl->precise_lpm_vcore)
				opp_ctrl_table[opp].vcore_uv -= VCORE_PRCISE_ADJUST_UV;
			opp_ctrl_table[opp].ddr_khz = vcorefs_get_ddr_by_steps(opp);
			break;
		case OPPI_ULTRA_LOW_PWR:
			opp_ctrl_table[opp].vcore_uv = vcorefs_get_vcore_by_steps(opp);
			if (gvrctrl->precise_lpm_vcore)
				opp_ctrl_table[opp].vcore_uv -= VCORE_PRCISE_ADJUST_UV;
			opp_ctrl_table[opp].ddr_khz = vcorefs_get_ddr_by_steps(opp);
			break;
		default:
			break;
		}
		vcorefs_crit("opp %u: vcore_uv: %u, ddr_khz: %u\n",
								opp,
								opp_ctrl_table[opp].vcore_uv,
								opp_ctrl_table[opp].ddr_khz);
	}

	vcorefs_crit("curr_vcore_uv: %u, curr_ddr_khz: %u\n",
							gvrctrl->curr_vcore_uv,
							gvrctrl->curr_ddr_khz);

	update_vcore_pwrap_cmd(opp_ctrl_table);
	mutex_unlock(&governor_mutex);

	return 0;
}

static int vcorefs_precise_lpm_vcore_enable(bool enable)
{
	struct kicker_config krconf;
	struct governor_profile *gvrctrl = &governor_ctrl;

	vcorefs_crit("precise_lpm_vcore_enable(%d) feature=%d\n", enable, is_vcorefs_feature_enable());
	if (!is_vcorefs_feature_enable())
		return -1;

	mutex_lock(&governor_mutex);

	kicker_table[KIR_SYSFS] = OPPI_PERF;
	krconf.kicker = KIR_SYSFS;
	krconf.opp = OPPI_PERF;
	krconf.dvfs_opp = OPPI_PERF;
	kick_dvfs_by_opp_index(&krconf);

	gvrctrl->precise_lpm_vcore = enable;

	mutex_unlock(&governor_mutex);

	init_vcorefs_cmd_table();

	mutex_lock(&governor_mutex);

	kicker_table[KIR_SYSFS] = OPPI_UNREQ;
	krconf.kicker = KIR_SYSFS;
	krconf.opp = OPPI_UNREQ;
	krconf.dvfs_opp = _get_dvfs_opp(KIR_SYSFS, krconf.opp);
	kick_dvfs_by_opp_index(&krconf);

	mutex_unlock(&governor_mutex);

	return 0;
}

static int vcorefs_emi_grouping_enable(bool enable)
{
	struct kicker_config krconf;
	struct governor_profile *gvrctrl = &governor_ctrl;

	vcorefs_crit("emi_grouping_enable(%d) feature=%d\n", enable, is_vcorefs_feature_enable());
	if (!is_vcorefs_feature_enable())
		return -1;

	mutex_lock(&governor_mutex);

	kicker_table[KIR_SYSFS] = OPPI_PERF;
	krconf.kicker = KIR_SYSFS;
	krconf.opp = OPPI_PERF;
	krconf.dvfs_opp = OPPI_PERF;
	kick_dvfs_by_opp_index(&krconf);

	gvrctrl->emi_grouping = enable;
	spm_vcorefs_emi_grouping_req(gvrctrl->emi_grouping);

	kicker_table[KIR_SYSFS] = OPPI_UNREQ;
	krconf.kicker = KIR_SYSFS;
	krconf.opp = OPPI_UNREQ;
	krconf.dvfs_opp = _get_dvfs_opp(KIR_SYSFS, krconf.opp);
	kick_dvfs_by_opp_index(&krconf);

	mutex_unlock(&governor_mutex);

	return 0;
}

int governor_debug_store(const char *buf)
{
	struct governor_profile *gvrctrl = &governor_ctrl;
	int val, r = 0;
	char cmd[32];

	if (sscanf(buf, "%31s 0x%x", cmd, &val) == 2 ||
		   sscanf(buf, "%31s %d", cmd, &val) == 2) {

		if (!strcmp(cmd, "plat_feature_en")) {
			gvrctrl->plat_feature_en = val;
			vcorefs_late_init_dvfs();
		} else if (!strcmp(cmd, "cpu_dvfs_req"))
			vcorefs_set_cpu_dvfs_req(val);
		else if (!strcmp(cmd, "vcore_dvs"))
			vcorefs_enable_vcore(val);
		else if (!strcmp(cmd, "ddr_dfs"))
			vcorefs_enable_ddr(val);
		else if (!strcmp(cmd, "reset_emi_time"))
			spm_write(VCOREFS_SRAM_EMI_BLOCK_TIME, 0);
		else if (!strcmp(cmd, "screen_off_ulpm"))
			gvrctrl->screen_off_ulpm = !!val;
		else if (!strcmp(cmd, "ulpm_perform_bw_en"))
			vcorefs_enable_ulpm_perform_bw(!!val);
		else if (!strcmp(cmd, "ulpm_total_bw_en"))
			vcorefs_enable_ulpm_total_bw(!!val);
		else if (!strcmp(cmd, "precise_lpm_vcore"))
			r = vcorefs_precise_lpm_vcore_enable(!!val);
		else if (!strcmp(cmd, "emi_grouping"))
			r = vcorefs_emi_grouping_enable(!!val);
		else if (!strcmp(cmd, "dfd_wakeup_src")) {
			gvrctrl->dfd_wakeup_src = !!val;
			spm_set_dfd_wakeup_src(!!val);
		} else
			r = -EPERM;
	} else {
		r = -EPERM;
	}

	return r;
}

char *governor_get_dvfs_info(char *p)
{
	struct governor_profile *gvrctrl = &governor_ctrl;
	int uv = vcorefs_get_curr_vcore();
	char *buff_end = p + PAGE_SIZE;

	p += snprintf(p, buff_end - p, "sw_opp: %d\n", vcorefs_get_sw_opp());
	p += snprintf(p, buff_end - p, "hw_opp: %d\n", vcorefs_get_hw_opp());
	p += snprintf(p, buff_end - p, "\n");

	p += snprintf(p, buff_end - p, "[plat_feature_en]: %d\n", gvrctrl->plat_feature_en);
	p += snprintf(p, buff_end - p, "[vcore_dvs   ]: %d\n", gvrctrl->vcore_dvs);
	p += snprintf(p, buff_end - p, "[ddr_dfs     ]: %d\n", gvrctrl->ddr_dfs);
	p += snprintf(p, buff_end - p, "[cpu_dvfs_req ]: 0x%x\n", gvrctrl->cpu_dvfs_req);
	p += snprintf(p, buff_end - p, "[ddr_type ]: 0x%x\n", gvrctrl->ddr_type);
	p += snprintf(p, buff_end - p, "[screen_off_ulpm]: %d\n", gvrctrl->screen_off_ulpm);
	p += snprintf(p, buff_end - p, "[segment_code]: %d\n", gvrctrl->segment_code);
	p += snprintf(p, buff_end - p, "[precise_lpm_vcore]: %d\n", gvrctrl->precise_lpm_vcore);
	p += snprintf(p, buff_end - p, "[emi_grouping]: %d\n", gvrctrl->emi_grouping);
	p += snprintf(p, buff_end - p, "[dfd_wakeup_src]: %d\n", gvrctrl->dfd_wakeup_src);
	p += snprintf(p, buff_end - p, "\n");

	p += snprintf(p, buff_end - p, "[vcore] uv : %u (0x%x)\n", uv, vcore_uv_to_pmic(uv));
	p += snprintf(p, buff_end - p, "[ddr  ] khz: %u\n", vcorefs_get_curr_ddr());
	p += snprintf(p, buff_end - p, "\n");

	p += snprintf(p, buff_end - p,
			"[perform_bw]: en=%d ulpm_thres=0x%x, lpm_thres=0x%x, hpm_thres =0x%x\n",
			gvrctrl->perform_bw_enable,
			gvrctrl->perform_bw_ulpm_threshold,
			gvrctrl->perform_bw_lpm_threshold,
			gvrctrl->perform_bw_hpm_threshold);
	p += snprintf(p, buff_end - p,
			"[total_bw]: en=%d ulpm_thres=0x%x, lpm_thres=0x%x, hpm_thres =0x%x\n",
			gvrctrl->total_bw_enable,
			gvrctrl->total_bw_ulpm_threshold,
			gvrctrl->total_bw_lpm_threshold,
			gvrctrl->total_bw_hpm_threshold);
	p += sprintf(p, "[ulpm perform_bw]: en=%d\n", gvrctrl->ulpm_perform_bw_enable);
	p += sprintf(p, "[ulpm total_bw]  : en=%d\n", gvrctrl->ulpm_total_bw_enable);
	p += snprintf(p, buff_end - p, "\n");

	p = vcorefs_get_sram_debug_info(p);
	p += snprintf(p, buff_end - p, "\n");

	return p;
}

/*
 * EMIBW
 */
int vcorefs_enable_perform_bw(bool enable)
{
	struct governor_profile *gvrctrl = &governor_ctrl;

	mutex_lock(&governor_mutex);
	gvrctrl->perform_bw_enable = enable;
	spm_vcorefs_enable_perform_bw(enable, true);
	mutex_unlock(&governor_mutex);

	return 0;
}

int vcorefs_set_perform_bw_threshold(u32 ulpm_threshold, u32 lpm_threshold, u32 hpm_threshold)
{
	struct governor_profile *gvrctrl = &governor_ctrl;

	if (((hpm_threshold & (~0x7F)) != 0) || ((lpm_threshold & (~0x7F)) != 0) || ((ulpm_threshold & (~0x7F)) != 0)) {
		vcorefs_err("perform BW threshold out-of-range, ULPM: %u, LPM: %u, HPM: %u\n",
				ulpm_threshold, lpm_threshold, hpm_threshold);
		return -1;
	}
	gvrctrl->perform_bw_ulpm_threshold = ulpm_threshold;
	gvrctrl->perform_bw_lpm_threshold = lpm_threshold;
	gvrctrl->perform_bw_hpm_threshold = hpm_threshold;

	mutex_lock(&governor_mutex);
	spm_vcorefs_set_perform_bw_threshold(ulpm_threshold, lpm_threshold, hpm_threshold);
	mutex_unlock(&governor_mutex);

	return 0;
}

int vcorefs_enable_total_bw(bool enable)
{
	struct governor_profile *gvrctrl = &governor_ctrl;

	mutex_lock(&governor_mutex);
	gvrctrl->total_bw_enable = enable;
	spm_vcorefs_enable_total_bw(enable, true);
	mutex_unlock(&governor_mutex);

	return 0;
}

int vcorefs_set_total_bw_threshold(u32 ulpm_threshold, u32 lpm_threshold, u32 hpm_threshold)
{
	struct governor_profile *gvrctrl = &governor_ctrl;

	if (((hpm_threshold & (~0x7F)) != 0) || ((lpm_threshold & (~0x7F)) != 0) || ((ulpm_threshold & (~0x7F)) != 0)) {
		vcorefs_err("perform BW threshold out-of-range, ULPM: %u, LPM: %u, HPM: %u\n",
				ulpm_threshold, lpm_threshold, hpm_threshold);
		return -1;
	}
	gvrctrl->total_bw_ulpm_threshold = ulpm_threshold;
	gvrctrl->total_bw_lpm_threshold = lpm_threshold;
	gvrctrl->total_bw_hpm_threshold = hpm_threshold;

	mutex_lock(&governor_mutex);
	spm_vcorefs_set_total_bw_threshold(ulpm_threshold, lpm_threshold, hpm_threshold);
	mutex_unlock(&governor_mutex);

	return 0;
}

int vcorefs_enable_ulpm_perform_bw(bool enable)
{
	struct governor_profile *gvrctrl = &governor_ctrl;

	mutex_lock(&governor_mutex);
	gvrctrl->ulpm_perform_bw_enable = enable;
	spm_vcorefs_enable_ulpm_perform_bw(enable, true);
	mutex_unlock(&governor_mutex);

	return 0;
}

int vcorefs_enable_ulpm_total_bw(bool enable)
{
	struct governor_profile *gvrctrl = &governor_ctrl;

	mutex_lock(&governor_mutex);
	gvrctrl->ulpm_total_bw_enable = enable;
	spm_vcorefs_enable_ulpm_total_bw(enable, true);
	mutex_unlock(&governor_mutex);

	return 0;
}

/*
 * Sub-main function
 */
static int set_dvfs_with_opp(struct kicker_config *krconf)
{
	struct governor_profile *gvrctrl = &governor_ctrl;
	struct opp_profile *opp_ctrl_table = opp_table;
	int expect_vcore_uv, expect_ddr_khz, opp_idx, kir_group;
	int r = 0;

	gvrctrl->curr_vcore_uv = vcorefs_get_curr_vcore();
	gvrctrl->curr_ddr_khz = vcorefs_get_curr_ddr();

	opp_idx = krconf->dvfs_opp;
	if (opp_idx == OPPI_UNREQ)
		opp_idx = OPPI_LOW_PWR;

	expect_vcore_uv = (gvrctrl->vcore_dvs == 1) ? opp_ctrl_table[opp_idx].vcore_uv : gvrctrl->curr_vcore_uv;
	expect_ddr_khz = (gvrctrl->ddr_dfs == 1) ? opp_ctrl_table[opp_idx].ddr_khz : gvrctrl->curr_vcore_uv;

	vcorefs_crit_mask(log_mask(), krconf->kicker, "opp: %d, vcore: %u(%u), fddr: %u(%u) %s%s\n",
		     krconf->dvfs_opp,
		     opp_ctrl_table[opp_idx].vcore_uv, gvrctrl->curr_vcore_uv,
		     opp_ctrl_table[opp_idx].ddr_khz, gvrctrl->curr_ddr_khz,
		     (gvrctrl->vcore_dvs) ? "[O]" : "[X]",
		     (gvrctrl->ddr_dfs) ? "[O]" : "[X]");

	if (!gvrctrl->vcore_dvs && !gvrctrl->ddr_dfs)
		return 0;

	kir_group = vcorefs_get_dvfs_kicker_group(krconf->kicker);

	switch (kir_group) {
	case KIR_GROUP_HPM:
		r = spm_vcorefs_set_opp(krconf->dvfs_opp, expect_vcore_uv, expect_ddr_khz, 0);
	break;
	case KIR_GROUP_HPM_NON_FORCE:
		r = spm_vcorefs_set_opp(krconf->dvfs_opp, expect_vcore_uv, expect_ddr_khz, 1);
	break;
	case KIR_GROUP_FIX:
		r = spm_vcorefs_set_opp_fix(krconf->dvfs_opp, expect_vcore_uv, expect_ddr_khz);
	break;
	default:
		WARN_ON(1);
	break;
	}

	gvrctrl->curr_vcore_uv = opp_ctrl_table[krconf->dvfs_opp].vcore_uv;
	gvrctrl->curr_ddr_khz = opp_ctrl_table[krconf->dvfs_opp].ddr_khz;

	return r;
}

/*
 * Main function API
 */
int kick_dvfs_by_opp_index(struct kicker_config *krconf)
{
	int r = 0;

	r = set_dvfs_with_opp(krconf);

	if (r == 0) {
		vcorefs_curr_opp = krconf->dvfs_opp;
	} else if (r < 0) {
		vcorefs_err("Vcore DVFS Fail, r: %d\n", r);
	} else {
		vcorefs_err("unknown error handling, r: %d\n", r);
		WARN_ON(1);
	}

	return r;
}

static int vcorefs_fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	struct governor_profile *gvrctrl = &governor_ctrl;
	struct fb_event *evdata = data;
	int blank;

	if (!is_vcorefs_feature_enable() || !gvrctrl->plat_init_done)
		return 0;

	if (gvrctrl->cpu_dvfs_req & MD_DISABLE_SCREEN_CHANGE)
		return 0;

	if (event != FB_EVENT_BLANK)
		return 0;

	blank = *(int *)evdata->data;

	switch (blank) {
	case FB_BLANK_UNBLANK:
		vcorefs_crit("SCREEN ON\n");
		spm_vcorefs_set_cpu_dvfs_req(0xFFFF, gvrctrl->cpu_dvfs_req);
		if (gvrctrl->ddr_type != TYPE_LPDDR3 && kicker_table[KIR_SYSFSX] == OPPI_UNREQ) {
			spm_vcoefs_MD_LPM_req(true);
			vcorefs_request_dvfs_opp(KIR_FB, OPPI_UNREQ);
		}
		break;
	case FB_BLANK_POWERDOWN:
		vcorefs_crit("SCREEN OFF\n");
		spm_vcorefs_set_cpu_dvfs_req(0, gvrctrl->cpu_dvfs_req);
		if (gvrctrl->ddr_type != TYPE_LPDDR3 && kicker_table[KIR_SYSFSX] == OPPI_UNREQ) {
			if (gvrctrl->screen_off_ulpm) {
				spm_vcoefs_MD_LPM_req(false);
				vcorefs_request_dvfs_opp(KIR_FB, OPPI_ULTRA_LOW_PWR);
			} else {
				spm_vcoefs_MD_LPM_req(true);
				vcorefs_request_dvfs_opp(KIR_FB, OPPI_UNREQ);
			}
		}
		break;
	default:
		break;
	}

	return 0;
}

static struct notifier_block vcorefs_fb_notif = {
		.notifier_call = vcorefs_fb_notifier_callback,
};
/*
 * AutoK related API
 */
void governor_autok_manager(void)
{
	int r;

	#if (AUTOK_EMMC == 1)
	r = emmc_autok();
	vcorefs_crit("EMMC autok done: %s\n", (r == 0) ? "Yes" : "No");
	#endif

	#if (AUTOK_SD == 1)
	r = sd_autok();
	vcorefs_crit("SD autok done: %s\n", (r == 0) ? "Yes" : "No");
	#endif

	#if (AUTOK_SDIO == 1)
	r = sdio_autok();
	vcorefs_crit("SDIO autok done: %s\n", (r == 0) ? "Yes" : "No");
	#endif
}

bool governor_autok_check(int kicker)
{
	int is_autok = true;
	struct governor_profile *gvrctrl = &governor_ctrl;

	mutex_lock(&governor_mutex);
	if (!((1 << kicker) & gvrctrl->autok_kir_group)) {
		is_autok = false;
	} else if (gvrctrl->active_autok_kir != 0 && gvrctrl->active_autok_kir != kicker) {
		vcorefs_err("Not allow kir:%d autok ( other kir:%d on-going)\n", kicker, gvrctrl->active_autok_kir);
		is_autok = false;
	} else {
		is_autok = true;
	}
	mutex_unlock(&governor_mutex);

	return is_autok;
}

bool governor_autok_lock_check(int kicker, int opp)
{
	bool lock_r = true;
	struct governor_profile *gvrctrl = &governor_ctrl;

	mutex_lock(&governor_mutex);

	if (gvrctrl->active_autok_kir == 0) {
		gvrctrl->active_autok_kir = kicker;	/* start autok */
		lock_r = true;
	} else if (kicker == gvrctrl->active_autok_kir) {
		lock_r = true;	/* continue autok */
	} else {
		WARN_ON(1);
	}

	if (opp == OPPI_UNREQ) {
		gvrctrl->active_autok_kir = 0;
		lock_r = false;
	}

	mutex_unlock(&governor_mutex);
	return lock_r;
}

/*
 * late_init is called after dyna_load_fw ready
 */
int vcorefs_late_init_dvfs(void)
{
	struct kicker_config krconf;
	struct governor_profile *gvrctrl = &governor_ctrl;
	int flag;

	if (!can_spm_pmic_set_vcore_voltage())
		gvrctrl->vcore_dvs = 0;

	if (gvrctrl->ddr_type == TYPE_LPDDR3)
		gvrctrl->cpu_dvfs_req = (1 << MD_CA_DATALINK | 1 << MD_POSITION);
	spm_vcorefs_set_cpu_dvfs_req(0xffff, gvrctrl->cpu_dvfs_req);

	if (1) {
		flag = vcorefs_check_feature_enable();
		vcorefs_crit("[%s] vcore_dvs: %d, ddr_dfs: %d, pcm_flag: 0x%x\n", __func__,
						gvrctrl->vcore_dvs, gvrctrl->ddr_dfs, flag);
		spm_go_to_vcore_dvfs(flag, 0);
	}

	mutex_lock(&governor_mutex);
	gvrctrl->late_init_opp = set_init_opp_index();

	if (is_vcorefs_feature_enable()) {
		int init_temp = get_immediate_ts4_wrap();
#if THERMAL_LT_SET_HPM
		if (init_temp < enter_hpm_temp)  {
			kicker_table[KIR_THERMAL] = OPPI_PERF;
			gvrctrl->late_init_opp = OPPI_PERF;
			vcorefs_crit("init with HPM for LT(temp=%d)\n", init_temp);
		}
#endif
		krconf.kicker = KIR_LATE_INIT;
		krconf.opp = gvrctrl->late_init_opp;
		krconf.dvfs_opp = gvrctrl->late_init_opp;

		kick_dvfs_by_opp_index(&krconf);
	}

	vcorefs_curr_opp = gvrctrl->late_init_opp;
	gvrctrl->plat_init_done = 1;
	mutex_unlock(&governor_mutex);

	vcorefs_crit("[%s] plat_init_done: %d, plat_feature_en: %d, late_init_opp: %d(%d)\n", __func__,
				gvrctrl->plat_init_done, is_vcorefs_feature_enable(),
				gvrctrl->late_init_opp, vcorefs_curr_opp);

	vcorefs_drv_init(gvrctrl->plat_init_done, is_vcorefs_feature_enable(), gvrctrl->late_init_opp);

	return 0;
}

#if 0
static int vcorefs_is_lp_flavor(void)
{
#if defined(CONFIG_ARM64)
	vcorefs_crit("flavor: %s\n", CONFIG_BUILD_ARM64_APPENDED_DTB_IMAGE_NAMES);

	if (strncmp(CONFIG_BUILD_ARM64_APPENDED_DTB_IMAGE_NAMES, "mediatek/k57pv1_dm_64_op01_lwctg_lp", 35) == 0)
		return 1;

	if (strncmp(CONFIG_BUILD_ARM64_APPENDED_DTB_IMAGE_NAMES, "mediatek/k57pv1_dm_64_om_lwctg_lp", 33) == 0)
		return 1;

	if (strncmp(CONFIG_BUILD_ARM64_APPENDED_DTB_IMAGE_NAMES, "mediatek/k57pv1_64_op01_lwctg_lp", 32) == 0)
		return 1;

	if (strncmp(CONFIG_BUILD_ARM64_APPENDED_DTB_IMAGE_NAMES, "mediatek/k57pv1_64_om_lwctg_lp", 30) == 0)
		return 1;

	if (strncmp(CONFIG_BUILD_ARM64_APPENDED_DTB_IMAGE_NAMES, "mediatek/k57pv1_dm_64_6m_cnop_lp", 32) == 0)
		return 1;
#endif

	return 0;
}
#endif

static int __init vcorefs_module_init(void)
{
	int r;
	struct device_node *node;
	int i;
	struct governor_profile *gvrctrl = &governor_ctrl;

#if defined(CONFIG_FPGA_EARLY_PORTING)
	return 0;
#endif
	gvrctrl->ddr_type = get_ddr_type();

	gvrctrl->segment_code = (get_devinfo_with_index(30) & 0x000000E0) >> 5;

	if (gvrctrl->segment_code == 3 || gvrctrl->segment_code == 7) {
		/* gvrctrl->precise_lpm_vcore = 1; */
		gvrctrl->dfd_wakeup_src = 1;
		/* gvrctrl->emi_grouping = 1; */
#if 0
		if (gvrctrl->ddr_type != TYPE_LPDDR3) {
			if (!vcorefs_is_lp_flavor()) {
				gvrctrl->plat_feature_en = 0;
				spm_vcorefs_off_load_lpm_req(0);
			}
		}
#endif
	}

	spm_set_dfd_wakeup_src(gvrctrl->dfd_wakeup_src);
	spm_vcorefs_emi_grouping_req(gvrctrl->emi_grouping);

	r = init_vcorefs_cmd_table();
	if (r) {
		vcorefs_err("FAILED TO INIT CONFIG (%d)\n", r);
		return r;
	}

	r = init_vcorefs_sysfs();
	if (r) {
		vcorefs_err("FAILED TO CREATE /sys/power/vcorefs (%d)\n", r);
		return r;
	}

	r = fb_register_client(&vcorefs_fb_notif);
	if (r) {
		vcorefs_err("FAILED TO REGISTER FB CLIENT (%d)\n", r);
		return r;
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,sleep");
	if (!node)
		vcorefs_err("find sleep node failed\n");

	vcorefs_sram_base = of_iomap(node, 1);
	if (!vcorefs_sram_base) {
		vcorefs_err("FAILED TO MAP SRAM MEMORY OF VCORE DVFS\n");
		return -ENOMEM;
	}
	vcorefs_init_sram_debug();

	for (i = 0; i < NUM_KICKER; i++)
		kicker_table[i] = OPPI_UNREQ;

	kicker_table[KIR_CPU] = OPPI_LOW_PWR;

	kicker_table[KIR_SYSFSX] = OPPI_UNREQ;


	return r;
}

module_init(vcorefs_module_init);
