/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/kernel.h>
#include <linux/module.h>

#include <mtk_idle.h>
#include <mtk_idle_internal.h>
#include <mtk_spm_internal.h> /* mtk_idle_cond_check */

#include <mtk_idle_fs/mtk_idle_sysfs.h>
#include <mtk_idle_module.h>
#include <mtk_idle_module_plat.h>

#include "mtk_spm_resource_req.h"

static bool idle_dram_feature_enable = MTK_IDLE_FEATURE_ENABLE_IDLEDRAM;
static bool idle_dram_bypass_idle_cond;

static unsigned int idle_dram_flag = MTK_IDLE_LOG_REDUCE;

/* [ByChip] Internal weak functions: implemented in mtk_idle_cond_check.c */
void __attribute__((weak)) mtk_idle_cg_monitor(int sel) {}
bool __attribute__((weak)) mtk_idle_cond_check(unsigned int idle_type) {return false; }
void __attribute__((weak)) mtk_idle_cond_update_mask(
	unsigned int idle_type, unsigned int reg, unsigned int mask) {}
int __attribute__((weak)) mtk_idle_cond_append_info(
	bool short_log, unsigned int idle_type, char *logptr, unsigned int logsize);

bool mtk_idle_dram_enabled(void)
{
	return idle_dram_feature_enable;
}

void mtk_idle_dram_disable(void)
{
	idle_dram_feature_enable = 0;
}

bool mtk_idle_dram_can_enter(int reason, struct mtk_idle_info *info)
{
	if (!idle_dram_feature_enable)
		return false;

	if (reason < NR_REASONS)
		goto out;

	if (!idle_dram_bypass_idle_cond &&
		!mtk_idle_cond_check(IDLE_MODEL_DRAM)) {
		reason = BY_CLK;
		goto out;
	}

	reason = NR_REASONS;

out:
	return mtk_idle_select_state(IDLE_MODEL_DRAM, reason);
}

int mtk_idle_dram_enter(int cpu)
{
	unsigned int op_cond = 0;

	spm_resource_req(SPM_RESOURCE_USER_SPM, SPM_RESOURCE_MAINPLL);
	mtk_idle_enter(IDLE_MODEL_DRAM, cpu, op_cond, idle_dram_flag);
	spm_resource_req(SPM_RESOURCE_USER_SPM, SPM_RESOURCE_RELEASE);

	return MTK_IDLE_MOD_OK;
}

static ssize_t mtk_idle_dram_state_read(char *ToUserBuf, size_t sz, void *priv)
{
	int i;
	char *p = ToUserBuf;
	struct MTK_IDLE_MODEL_COUNTER cnt;

	#undef log
	#define log(fmt, args...) ({\
			p += scnprintf(p, sz - strlen(ToUserBuf)\
					, fmt, ##args); p; })

	log("*************** Idle dram state *********************\n");
	if (mtk_idle_model_count_get(IDLE_MODEL_DRAM, &cnt)
			== MTK_IDLE_MOD_OK) {
		for (i = 0; i < NR_REASONS; i++) {
			if (cnt.block[i] > 0)
				log("[%d] block_cnt[%s]=%lu\n"
					, i, mtk_idle_block_reason_name(i),
					cnt.block[i]);
		}
		log("\n");
	}

	p += mtk_idle_cond_append_info(
		false, IDLE_MODEL_DRAM, p, sz - strlen(ToUserBuf));
	log("\n");

	log("*************** variable dump ********************\n");
	log("enable=%d bypass=%d\n",
		idle_dram_feature_enable ? 1 : 0,
		idle_dram_bypass_idle_cond ? 1 : 0);
	log("log_flag=%x\n", idle_dram_flag);
	log("\n");

	log("*************** Idle dram command *******************\n");
	log("Dram idle state:  cat /d/cpuidle/IdleDram_state\n");
	log("Dram idle on/off: echo 1/0 > /d/cpuidle/IdleDram_state\n");
	log("bypass cg/pll: echo bypass 1/0 > /d/cpuidle/IdleDram_state\n");
	log("cg monitor:    echo cgmon 1/0 > /d/cpuidle/IdleDram_state\n");
	log("set log_flag:  echo log [hex] > /d/cpuidle/IdleDram_state\n");
	log("               [0] reduced [1] spm res usage [2] disable\n");
	log("\n");

	return p - ToUserBuf;
}

static ssize_t mtk_idle_dram_state_write(
			char *FromUserBuf, size_t sz, void *priv)
{
	char cmd[128];
	int parm, parm1;


	if (sscanf(FromUserBuf, "%127s %x %x", cmd, &parm, &parm1) == 3) {
		if (!strcmp(cmd, "mask"))
			mtk_idle_cond_update_mask(IDLE_MODEL_DRAM, parm, parm1);
	} else if (sscanf(FromUserBuf, "%127s %x", cmd, &parm) == 2) {
		if (!strcmp(cmd, "enable"))
			idle_dram_feature_enable = !!parm;
		else if (!strcmp(cmd, "bypass"))
			idle_dram_bypass_idle_cond = !!parm;
		else if (!strcmp(cmd, "cgmon"))
			mtk_idle_cg_monitor(parm ? IDLE_MODEL_DRAM : -1);
		else if (!strcmp(cmd, "log"))
			idle_dram_flag = parm;
		return sz;
	} else if ((!kstrtoint(FromUserBuf, 10, &parm)) == 1) {
		idle_dram_feature_enable = !!parm;
		return sz;
	}

	return -EINVAL;
}

static const struct mtk_idle_sysfs_op mtk_idle_dram_state_fops = {
	.fs_read = mtk_idle_dram_state_read,
	.fs_write = mtk_idle_dram_state_write,
};

struct mtk_lp_sysfs_handle lp_fs_dram = {
	._current = NULL,
};

void mtk_idle_dram_init(struct mtk_idle_init_data *pData)
{
	if (pData) {
		if (IS_MTK_LP_DTS_AVAILABLE_IDLEDRAM(pData)) {
			idle_dram_feature_enable =
				GET_MTK_LP_DTS_VALUE_IDLEDRAM(pData);
		}
	}
	idle_dram_bypass_idle_cond = false;
	mtk_idle_sysfs_entry_node_add("IdleDram_state"
			, 0644, &mtk_idle_dram_state_fops, &lp_fs_dram);
}

void mtk_idle_dram_deinit(void)
{
	mtk_idle_sysfs_entry_node_remove(&lp_fs_dram);
}

