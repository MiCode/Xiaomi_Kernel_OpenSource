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

#include <trace/events/mtk_idle_event.h> /* trace header */

#include <mtk_mcdi_governor.h> /* idle_refcnt_inc/dec */

/* add/remove_cpu_to/from_perfer_schedule_domain */
#include <linux/irqchip/mtk-gic-extend.h>

#include <mtk_idle.h>
#include <mtk_idle_internal.h>
#include <mtk_spm_internal.h>
#include <mtk_spm_resource_req_internal.h>
#include "mtk_idle_sysfs.h"

#include "mtk_lp_dts.h"
/* Change sodi3 */
bool mtk_idle_screen_off_sodi3 = MTK_IDLE_ADJUST_CHECK_ORDER ? 1 : 0;

/* [ByChip] Internal weak functions: implemented in mtk_idle_cond_check.c */
void __attribute__((weak)) mtk_idle_cg_monitor(int sel) {}

/* External weak functions: implemented in mcdi driver */
void __attribute__((weak)) idle_refcnt_inc(void) {}
void __attribute__((weak)) idle_refcnt_dec(void) {}

bool __attribute__((weak)) mtk_spm_arch_type_get(void) { return false; }
void __attribute__((weak)) mtk_spm_arch_type_set(bool type) {}

/* mtk_dpidle_is_active() for pmic_throttling_dlpt
 *   return 0 : entering dpidle recently ( > 1s)
 *                      => normal mode(dlpt 10s)
 *   return 1 : entering dpidle recently (<= 1s)
 *                      => light-loading mode(dlpt 20s)
 */
#define DPIDLE_ACTIVE_TIME		(1)
struct timeval pre_dpidle_time;
bool mtk_dpidle_is_active(void)
{
	struct timeval current_time;
	long int diff;

	do_gettimeofday(&current_time);
	diff = current_time.tv_sec - pre_dpidle_time.tv_sec;

	if (diff > DPIDLE_ACTIVE_TIME)
		return false;
	else if ((diff == DPIDLE_ACTIVE_TIME) &&
		(current_time.tv_usec > pre_dpidle_time.tv_usec))
		return false;
	else
		return true;
}
EXPORT_SYMBOL(mtk_dpidle_is_active);


static ssize_t idle_state_read(char *ToUserBuf, size_t sz_t, void *priv)
{
	int i;
	char *p = ToUserBuf;
	size_t sz = sz_t;

	#undef log
	#define log(fmt, args...) \
	do { \
		int l = scnprintf(p, sz, fmt, ##args); \
		p += l; \
		sz -= l; \
	} while (0)

	log("*************** idle state ***********************\n");
	for (i = 0; i < nr_cpu_ids; i++) {
		log("cpu%d: dp=%lu, so3=%lu, so=%lu\n"
			, i, dp_cnt[i], so3_cnt[i], so_cnt[i]);
	}
	log("\n");

	log("*************** variable dump ********************\n");
	log("feature enable: dp=%d, so3=%d, so=%d\n",
		mtk_dpidle_enabled() ? 1 : 0,
		mtk_sodi3_enabled() ? 1 : 0,
		mtk_sodi_enabled() ? 1 : 0);

	log("idle_ratio_profile=%d\n", mtk_idle_get_ratio_status() ? 1 : 0);
	log("idle_latency_profile=%d\n"
			, mtk_idle_latency_profile_is_on() ? 1 : 0);
	log("twam_handler:%s (clk:%s)\n",
		(mtk_idle_get_twam()->running) ? "on" : "off",
		(mtk_idle_get_twam()->speed_mode) ? "speed" : "normal");
	log("screen_off_sodi3=%d (%s)\n",
		mtk_idle_screen_off_sodi3 ? 1 : 0,
		mtk_idle_screen_off_sodi3 ? "so3->dp->so" : "dp->so3->so");

	log("\n");

	log("**************** mtk_spm_arch **********************\n");
	log("mtk_spm_arch: %s-oriented\n",
		mtk_spm_arch_type_get() ? "resource" : "scenario");
	log("\n");

	#define MTK_DEBUGFS_IDLE	"/d/cpuidle/idle_state"
	#define MTK_DEBUGFS_DPIDLE	"/d/cpuidle/dpidle_state"
	#define MTK_DEBUGFS_SODI3	"/d/cpuidle/soidle3_state"
	#define MTK_DEBUGFS_SODI	"/d/cpuidle/soidle_state"

	log("*************** idle command help ****************\n");
	log("status help:          cat %s\n", MTK_DEBUGFS_IDLE);
	log("dpidle help:          cat %s\n", MTK_DEBUGFS_DPIDLE);
	log("sodi help:            cat %s\n", MTK_DEBUGFS_SODI);
	log("sodi3 help:           cat %s\n", MTK_DEBUGFS_SODI3);
	log("idle ratio profile:   echo ratio 1/0 > %s\n", MTK_DEBUGFS_IDLE);
	log("idle latency profile: echo latency 1/0 > %s\n", MTK_DEBUGFS_IDLE);
	log("cgmon off/dp/so3/so:  echo cgmon 0/1/2/3 > %s\n",
		MTK_DEBUGFS_IDLE);
	log("mtk_spm_arch_type:    echo spm_arch_type 0/1 > %s\n",
		MTK_DEBUGFS_IDLE);
	log("\n");

	return p - ToUserBuf;
}

static ssize_t idle_state_write(char *FromUserBuf, size_t sz, void *priv)
{
	char cmd[128];
	int parm;

	if (sscanf(FromUserBuf, "%127s %x", cmd, &parm) == 2) {
		if (!strcmp(cmd, "ratio")) {
			if (parm == 1)
				mtk_idle_enable_ratio_calc();
			else
				mtk_idle_disable_ratio_calc();
		} else if (!strcmp(cmd, "latency")) {
			mtk_idle_latency_profile_enable(parm ? true : false);
		} else if (!strcmp(cmd, "spmtwam_clk")) {
			mtk_idle_get_twam()->speed_mode = parm;
		} else if (!strcmp(cmd, "spmtwam_sel")) {
			mtk_idle_get_twam()->sel = parm;
		} else if (!strcmp(cmd, "spmtwam")) {
			pr_info("Power/swap spmtwam_event = %d\n", parm);
			if (parm >= 0)
				mtk_idle_twam_enable(parm);
			else
				mtk_idle_twam_disable();
		} else if (!strcmp(cmd, "cgmon")) {
			mtk_idle_cg_monitor(parm == 1 ? IDLE_TYPE_DP :
				parm == 2 ? IDLE_TYPE_SO3 :
				parm == 3 ? IDLE_TYPE_SO : -1);
		} else if (!strcmp(cmd, "screen_off_sodi3")) {
			mtk_idle_screen_off_sodi3 = parm ? true : false;
		} else if (!strcmp(cmd, "spm_arch_type")) {
			mtk_spm_arch_type_set(parm ? true : false);
		}
		return sz;
	} else if ((!kstrtoint(FromUserBuf, 10, &parm)) == 1) {
		return sz;
	}

	return -EINVAL;
}

static const struct mtk_idle_sysfs_op idle_state_fops = {
	.fs_read = idle_state_read,
	.fs_write = idle_state_write,
};

static void mtk_idle_init(void)
{
	mtk_idle_sysfs_entry_node_add("idle_state"
			, 0644, &idle_state_fops, NULL);
}

void __init mtk_cpuidle_framework_init(void)
{
	struct mtk_idle_init_data pInitData = {0, 0};
	struct device_node *idle_node = NULL;

	/* Get dts of cpu's idle-state*/
	idle_node = GET_MTK_IDLE_STATES_DTS_NODE();

	if (idle_node) {
		int state = 0;

		/* Get dts of SODI*/
		state = GET_MTK_OF_PROPERTY_STATUS_SODI(idle_node);
		MTK_IDLE_FEATURE_DTS_STATE_CHECK(MTK_LP_FEATURE_DTS_SODI
						, state, pInitData);

		/* Get dts of SODI3*/
		state = GET_MTK_OF_PROPERTY_STATUS_SODI3(idle_node);
		MTK_IDLE_FEATURE_DTS_STATE_CHECK(MTK_LP_FEATURE_DTS_SODI3
						, state, pInitData);

		/* Get dts of DeepIdle*/
		state = GET_MTK_OF_PROPERTY_STATUS_DP(idle_node);
		MTK_IDLE_FEATURE_DTS_STATE_CHECK(MTK_LP_FEATURE_DTS_DP
						, state, pInitData);

		of_node_put(idle_node);
	}
	mtk_idle_sysfs_entry_create();

	mtk_idle_init();
	mtk_dpidle_init(&pInitData);
	mtk_sodi_init(&pInitData);
	mtk_sodi3_init(&pInitData);
	spm_resource_req_debugfs_init();

	spm_resource_req_init();
}
EXPORT_SYMBOL(mtk_cpuidle_framework_init);
