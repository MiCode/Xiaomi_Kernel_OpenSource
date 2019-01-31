/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>

#include <mtk_idle.h>
#include <mtk_idle_internal.h>
#include <mtk_spm_internal.h> /* mtk_idle_cond_check */

#include <mtk_idle_sysfs.h>

static bool sodi_feature_enable = MTK_IDLE_FEATURE_ENABLE_SODI;
static bool sodi_bypass_idle_cond;
static bool sodi_bypass_dis_check;
static bool mtk_disp_is_ready;
static unsigned int sodi_flag = MTK_IDLE_LOG_REDUCE;

unsigned long so_cnt[NR_CPUS] = {0};
static unsigned long so_block_cnt[NR_REASONS] = {0};

/*SODI3 section*/
static bool sodi3_feature_enable = MTK_IDLE_FEATURE_ENABLE_SODI3;
static bool sodi3_bypass_idle_cond;
static bool sodi3_bypass_dis_check;
static bool sodi3_bypass_pwm_check;
static bool sodi3_force_vcore_lp_mode;
static unsigned int sodi3_flag = MTK_IDLE_LOG_REDUCE;

unsigned long so3_cnt[NR_CPUS] = {0};
static unsigned long so3_block_cnt[NR_REASONS] = {0};
/* [ByChip] Internal weak functions: implemented in mtk_idle_cond_check.c */
void __attribute__((weak)) mtk_idle_cg_monitor(int sel) {}
bool __attribute__((weak)) mtk_idle_cond_check(int idle_type) {return false; }
void __attribute__((weak)) mtk_idle_cond_update_mask(
	int idle_type, unsigned int reg, unsigned int mask) {}
int __attribute__((weak)) mtk_idle_cond_append_info(
	bool short_log, int idle_type, char *logptr, unsigned int logsize);

bool mtk_sodi_enabled(void)
{
	return sodi_feature_enable;
}

void mtk_sodi_disable(void)
{
	sodi_feature_enable = 0;
}

static unsigned long get_uptime(void)
{
	struct timespec uptime;

	get_monotonic_boottime(&uptime);
	return (unsigned long)uptime.tv_sec;
}

/* for display use, abandoned 'spm_enable_sodi' */
void mtk_idle_disp_is_ready(bool enable)
{
	mtk_disp_is_ready = enable;
}

static int sodi_uptime_block_count;
/* External weak function: implemented in disp driver */
bool __attribute__((weak)) disp_pwm_is_osc(void) {return false; }


bool mtk_sodi3_enabled(void)
{
	return sodi3_feature_enable;
}

void mtk_sodi3_disable(void)
{
	sodi3_feature_enable = 0;
}

static int sodi3_uptime_block_count;
bool sodi_can_enter(int reason)
{
	/* sodi is disabled */
	if (!sodi_feature_enable)
		return false;

	if (reason < NR_REASONS)
		goto out;

	/* check cg condition for sodi */
	if (!sodi_bypass_idle_cond && !mtk_idle_cond_check(IDLE_TYPE_SO)) {
		reason = BY_CLK;
		goto out;
	}

	/* display driver is ready ? */
	if (!sodi_bypass_dis_check && !mtk_disp_is_ready) {
		reason = BY_DIS;
		goto out;
	}

	/* check current uptime > 20 ? */
	if (sodi_uptime_block_count != -1) {
		if (get_uptime() <= 20) {
			sodi_uptime_block_count++;
			reason = BY_BOOT;
			goto out;
		} else {
			pr_notice("Power/swap SODI: blocked by uptime, count = %d\n",
				sodi_uptime_block_count);
			sodi_uptime_block_count = -1;
		}
	}

	reason = NR_REASONS;

out:
	return mtk_idle_select_state(IDLE_TYPE_SO, reason);
}

int soidle_enter(int cpu)
{
	unsigned int op_cond = 0;

	lockdep_off();
	mtk_idle_ratio_calc_start(IDLE_TYPE_SO, cpu);
	mtk_idle_enter(IDLE_TYPE_SO, cpu, op_cond, sodi_flag);
	mtk_idle_ratio_calc_stop(IDLE_TYPE_SO, cpu);
	lockdep_on();

	so_cnt[cpu]++;

	return CPUIDLE_STATE_SO;
}
EXPORT_SYMBOL(soidle_enter);


static ssize_t soidle_state_read(char *ToUserBuf, size_t sz, void *priv)
{
	int i;
	char *p = ToUserBuf;

	if (!ToUserBuf)
		return -EINVAL;
	#undef log
	#define log(fmt, args...) ({\
		p += scnprintf(p, sz - strlen(ToUserBuf), fmt, ##args); p; })

	log("*************** sodi state ***********************\n");
	for (i = 0; i < NR_REASONS; i++) {
		if (so_block_cnt[i] > 0)
			log("[%d] block_cnt[%s]=%lu\n"
				, i, mtk_idle_block_reason_name(i),
				so_block_cnt[i]);
	}
	log("\n");

	p += mtk_idle_cond_append_info(
		false, IDLE_TYPE_SO, p, sz - strlen(ToUserBuf));
	log("\n");

	log("*************** variable dump ********************\n");
	log("enable=%d bypass=%d bypass_dis=%d\n",
		sodi_feature_enable ? 1 : 0,
		sodi_bypass_idle_cond ? 1 : 0,
		sodi_bypass_dis_check ? 1 : 0);
	log("log_flag=0x%x\n", sodi_flag);
	log("\n");

	log("*************** sodi command *********************\n");
	log("sodi state:    cat /d/cpuidle/soidle_state\n");
	log("sodi on/off:   echo 1/0 > /d/cpuidle/soidle_state\n");
	log("bypass cg/pll: echo bypass 1/0 > /d/cpuidle/soidle_state\n");
	log("bypass dis:    echo bypass_dis 1/0 > /d/cpuidle/soidle_state\n");
	log("cg monitor:    echo cgmon 1/0 > /d/cpuidle/soidle_state\n");
	log("set log_flag:  echo log [hex] > /d/cpuidle/soidle_state\n");
	log("               [0] reduced [1] spm res usage [2] disable\n");
	log("\n");

	return p - ToUserBuf;
}

static ssize_t soidle_state_write(char *FromUserBuf, size_t sz, void *priv)
{
	char cmd[128];
	int parm, parm1;

	if (!FromUserBuf)
		return -EINVAL;


	if (sscanf(FromUserBuf, "%127s %x %x", cmd, &parm, &parm1) == 3) {
		if (!strcmp(cmd, "mask"))
			mtk_idle_cond_update_mask(IDLE_TYPE_SO, parm, parm1);
	} else if (sscanf(FromUserBuf, "%127s %x", cmd, &parm) == 2) {
		if (!strcmp(cmd, "soidle"))
			sodi_feature_enable = !!parm;
		else if (!strcmp(cmd, "bypass"))
			sodi_bypass_idle_cond = !!parm;
		else if (!strcmp(cmd, "bypass_dis"))
			sodi_bypass_dis_check = !!parm;
		else if (!strcmp(cmd, "cgmon"))
			mtk_idle_cg_monitor(parm ? IDLE_TYPE_SO : -1);
		else if (!strcmp(cmd, "log"))
			sodi_flag = parm;
		return sz;
	} else if ((!kstrtoint(FromUserBuf, 10, &parm)) == 1) {
		sodi_feature_enable = !!parm;
		return sz;
	}

	return -EINVAL;
}

static const struct mtk_idle_sysfs_op sodi_state_fops = {
	.fs_read = soidle_state_read,
	.fs_write = soidle_state_write,
};
bool sodi3_can_enter(int reason)
{
	/* sodi3 is disabled */
	if (!sodi3_feature_enable)
		return false;

	if (reason < NR_REASONS)
		goto out;

	/* check cg condition for sodi3 */
	if (!sodi3_bypass_idle_cond && !mtk_idle_cond_check(IDLE_TYPE_SO3)) {
		reason = BY_CLK;
		goto out;
	}

	/* display driver is ready ? */
	if (!sodi3_bypass_dis_check && !mtk_disp_is_ready) {
		reason = BY_DIS;
		goto out;
	}

	/* pwm clock uses ulposc ? */
	if (!disp_pwm_is_osc() && !sodi3_bypass_pwm_check) {
		reason = BY_PWM;
		goto out;
	}

	/* check current uptime > 30 ? */
	if (sodi3_uptime_block_count != -1) {
		if (get_uptime() <= 30) {
			sodi3_uptime_block_count++;
			reason = BY_BOOT;
			goto out;
		} else {
			pr_notice("Power/swap SODI3: blocked by uptime, count = %d\n",
				sodi3_uptime_block_count);
			sodi3_uptime_block_count = -1;
		}
	}

	reason = NR_REASONS;

out:
	return mtk_idle_select_state(IDLE_TYPE_SO3, reason);
}

int soidle3_enter(int cpu)
{
	unsigned int op_cond = 0;

	if (sodi3_force_vcore_lp_mode)
		op_cond |= MTK_IDLE_OPT_VCORE_LP_MODE;

	lockdep_off();
	mtk_idle_ratio_calc_start(IDLE_TYPE_SO3, cpu);
	mtk_idle_enter(IDLE_TYPE_SO3, cpu, op_cond, sodi3_flag);
	mtk_idle_ratio_calc_stop(IDLE_TYPE_SO3, cpu);
	lockdep_on();

	so3_cnt[cpu]++;

	return CPUIDLE_STATE_SO3;
}
EXPORT_SYMBOL(soidle3_enter);
static ssize_t soidle3_state_read(char *ToUserBuf, size_t sz, void *priv)
{
	int i;
	char *p = ToUserBuf;

	if (!ToUserBuf)
		return -EINVAL;

#undef log
#define log(fmt, args...) ({\
		p += scnprintf(p, sz - strlen(ToUserBuf), fmt, ##args); p; })

	log("*************** sodi3 state **********************\n");
	for (i = 0; i < NR_REASONS; i++) {
		if (so3_block_cnt[i] > 0)
			log("[%d] block_cnt[%s]=%lu\n", i
				, mtk_idle_block_reason_name(i)
				, so3_block_cnt[i]);
	}
	log("\n");

	p += mtk_idle_cond_append_info(
		false, IDLE_TYPE_SO3, p, sz - strlen(ToUserBuf));
	log("\n");


	log("*************** variable dump ********************\n");
	log("enable=%d bypass=%d bypass_dis=%d\n"
		, sodi3_feature_enable ? 1 : 0
		, sodi3_bypass_idle_cond ? 1 : 0
		, sodi3_bypass_dis_check ? 1 : 0);

	log("bypass_pwm=%d force_vcore_lp_mode=%d\n"
			, sodi3_bypass_pwm_check ? 1 : 0
			, sodi3_force_vcore_lp_mode ? 1 : 0);

	log("log_flag=0x%x\n", sodi3_flag);
	log("\n");

log("*************** sodi3 command ********************\n");
log("sodi3 state:	cat /d/cpuidle/soidle3_state\n");
log("sodi3 on/off:	echo 1/0 > /d/cpuidle/soidle3_state\n");
log("bypass cg/pll: echo bypass 1/0 > /d/cpuidle/soidle3_state\n");
log("bypass dis:	echo bypass_dis 1/0 > /d/cpuidle/soidle3_state\n");
log("bypass pwm:	echo bypass_pwm 1/0 > /d/cpuidle/soidle3_state\n");
log("cg monitor:	echo cgmon 1/0 > /d/cpuidle/soidle3_state\n");
log("set log_flag:	echo log [hex_value] > /d/cpuidle/soidle3_state\n");
log("				[0] reduced [1] spm res usage [2] disable\n");
log("\n");

	return p - ToUserBuf;
}

static ssize_t soidle3_state_write(char *FromUserBuf, size_t sz, void *priv)
{
	char cmd[128];
	int parm, parm1;

	if (!FromUserBuf)
		return -EINVAL;

	if (sscanf(FromUserBuf, "%127s %x %x", cmd, &parm, &parm1) == 3) {
		if (!strcmp(cmd, "mask"))
			mtk_idle_cond_update_mask(IDLE_TYPE_SO3, parm, parm1);
	} else if (sscanf(FromUserBuf, "%127s %x", cmd, &parm) == 2) {
		if (!strcmp(cmd, "soidle3"))
			sodi3_feature_enable = !!parm;
		else if (!strcmp(cmd, "bypass"))
			sodi3_bypass_idle_cond = !!parm;
		else if (!strcmp(cmd, "bypass_dis"))
			sodi3_bypass_dis_check = !!parm;
		else if (!strcmp(cmd, "bypass_pwm"))
			sodi3_bypass_pwm_check = !!parm;
		else if (!strcmp(cmd, "force_vcore_lp_mode"))
			sodi3_force_vcore_lp_mode = !!parm;
		else if (!strcmp(cmd, "cgmon"))
			mtk_idle_cg_monitor(parm ? IDLE_TYPE_SO3 : -1);
		else if (!strcmp(cmd, "log"))
			sodi3_flag = parm;
		return sz;
	} else if ((!kstrtoint(FromUserBuf, 10, &parm)) == 1) {
		sodi3_feature_enable = !!parm;
		return sz;
	}

	return -EINVAL;
}

static const struct mtk_idle_sysfs_op sodi3_state_fops = {
	.fs_read = soidle3_state_read,
	.fs_write = soidle3_state_write,
};

void mtk_sodi_init(void)
{
	sodi_uptime_block_count = 0;
	sodi_bypass_idle_cond = false;
	sodi_bypass_dis_check = false;
	mtk_disp_is_ready = false;

	mtk_idle_sysfs_entry_node_add("soidle_state"
			, 0644, &sodi_state_fops, NULL);

	mtk_idle_block_setting(IDLE_TYPE_SO, so_cnt, so_block_cnt);
}
void mtk_sodi3_init(void)
{
	sodi3_uptime_block_count = 0;

	sodi3_bypass_idle_cond = false;
	sodi3_bypass_dis_check = false;
	sodi3_bypass_pwm_check = false;
	sodi3_force_vcore_lp_mode = false;

	mtk_idle_sysfs_entry_node_add("soidle3_state"
				, 0644, &sodi3_state_fops, NULL);

	mtk_idle_block_setting(IDLE_TYPE_SO3, so3_cnt, so3_block_cnt);
}
