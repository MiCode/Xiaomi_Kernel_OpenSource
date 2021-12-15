/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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
#include <mtk_idle_fs/mtk_idle_sysfs.h>

#include <mtk_idle_module.h>

/* [ByChip] Internal weak functions: implemented in mtk_idle_cond_check.c */
void __attribute__((weak)) mtk_idle_cg_monitor(int sel) {}

/* External weak functions: implemented in mcdi driver */
void __attribute__((weak)) idle_refcnt_inc(void) {}
void __attribute__((weak)) idle_refcnt_dec(void) {}

bool __attribute__((weak)) mtk_spm_arch_type_get(void) { return false; }
void __attribute__((weak)) mtk_spm_arch_type_set(bool type) {}



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
	i = mtk_idle_module_info_dump_locked(
		MTK_IDLE_MODULE_INFO_IDLE_STATE, p, sz);
	p += i;
	sz -= i;
	log("\n");

	log("*************** variable dump ********************\n");
	log("feature enable: ");
	i = mtk_idle_module_info_dump_locked(
		MTK_IDLE_MODULE_INFO_IDLE_ENABLED, p, sz);
	p += i;
	sz -= i;
	log("\n");

	log("idle_ratio_profile=%d\n", mtk_idle_get_ratio_status() ? 1 : 0);
	log("idle_latency_profile=%d\n"
			, mtk_idle_latency_profile_is_on() ? 1 : 0);
	log("twam_handler:%s (clk:%s)\n",
		(mtk_idle_get_twam()->running) ? "on" : "off",
		(mtk_idle_get_twam()->speed_mode) ? "speed" : "normal");
	log("Idle switch type support:\n");
	i = mtk_idle_module_switch_support(p, sz);
	p += i;
	sz -= i;

	#define MTK_DEBUGFS_IDLE	"/d/cpuidle/idle_state"

	log("*************** idle command help ****************\n");
	log("status help:          cat %s\n", MTK_DEBUGFS_IDLE);
	log("idle ratio profile:   echo ratio 1/0 > %s\n", MTK_DEBUGFS_IDLE);
	log("idle latency profile: echo latency 1/0 > %s\n", MTK_DEBUGFS_IDLE);
	log("switch: echo switch [type number] > %s\n", MTK_DEBUGFS_IDLE);
	i = mtk_idle_module_get_helper(p, sz);
	p += i;
	sz -= i;
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
		} else if (!strcmp(cmd, "switch")) {
			mtk_idle_module_switch(parm);
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
	mtk_idle_sysfs_entry_create();

	mtk_idle_init();
	spm_resource_req_debugfs_init();

	spm_resource_req_init();
}
EXPORT_SYMBOL(mtk_cpuidle_framework_init);
