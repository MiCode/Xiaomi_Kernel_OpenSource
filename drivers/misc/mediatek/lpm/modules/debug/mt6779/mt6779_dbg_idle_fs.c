// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/console.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>

#include <mtk_dbg_common_v1.h>
#include <mtk_lpm_module.h>
#include <mtk_resource_constraint_v1.h>
#include <mtk_lpm_sysfs.h>
#include <mtk_suspend_sysfs.h>
#include <mtk_spm_sysfs.h>

#include <mt6779_pwr_ctrl.h>
#include <mt6779_dbg_fs_common.h>
#include <mt6779_cond.h>
#include <mt6779_spm_comm.h>


#define MT_LP_BUS26M_NODE "/sys/kernel/debug/cpuidle/IdleBus26m_state"
#define MT_LP_SYSPLL_NODE "/sys/kernel/debug/cpuidle/IdleSyspll_state"
#define MT_LP_DRAM_NODE "/sys/kernel/debug/cpuidle/IdleDram_state"


#undef mtk_idle_log
#define mtk_idle_log(fmt, args...) \
	do { \
		int l = scnprintf(p, sz, fmt, ##args); \
		p += l; \
		sz -= l; \
	} while (0)


static char *mt6779_spm_cond_cg_str[PLAT_SPM_COND_MAX] = {
	[PLAT_SPM_COND_MTCMOS_0]	= "MTCMOS_0",
	[PLAT_SPM_COND_MTCMOS_1]	= "MTCMOS_1",
	[PLAT_SPM_COND_CG_INFRA_0]	= "INFRA_0",
	[PLAT_SPM_COND_CG_INFRA_1]	= "INFRA_1",
	[PLAT_SPM_COND_CG_INFRA_2]	= "INFRA_2",
	[PLAT_SPM_COND_CG_INFRA_3]	= "INFRA_3",
	[PLAT_SPM_COND_CG_MMSYS_0]	= "MMSYS_0",
	[PLAT_SPM_COND_CG_MMSYS_1]	= "MMSYS_1",
};

static char *mt6779_spm_cond_pll_str[PLAT_SPM_COND_PLL_MAX] = {
	[PLAT_SPM_COND_UNIVPLL]	= "UNIVPLL",
	[PLAT_SPM_COND_MFGPLL]	= "MFGPLL",
	[PLAT_SPM_COND_MSDCPLL]	= "MSDCPLL",
	[PLAT_SPM_COND_TVPLL]	= "TVPLL",
	[PLAT_SPM_COND_MMPLL]	= "MMPLL",
};

static ssize_t mt6779_idle_dram_read(char *ToUserBuf,
					size_t sz, void *priv)
{
	char *p = ToUserBuf;
	uint32_t block, b;
	int i;

	block = mtk_lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_COND_BLOCK,
				    MT_LPM_SMC_ACT_GET,
				    MT_RM_CONSTRAINT_ID_DRAM, 0);
	mtk_idle_log("enable=%lu. count=%lu, is_cond_check = %lu\n\n",
		mtk_lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RC_SWITCH,
				    MT_LPM_SMC_ACT_GET,
				    MT_RM_CONSTRAINT_ID_DRAM, 0)
		& MT_SPM_RC_VALID_SW,
		mtk_lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RC_CNT,
				    MT_LPM_SMC_ACT_GET,
				    MT_RM_CONSTRAINT_ID_DRAM, 0),
		mtk_lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_COND_CHECK,
				    MT_LPM_SMC_ACT_GET,
				    MT_RM_CONSTRAINT_ID_DRAM, 0));

	mtk_idle_log("is_cond_block = 0x%08x\n", block);
	for (i = 0, b = block >> SPM_COND_BLOCKED_CG_IDX;
	     i < PLAT_SPM_COND_MAX; i++)
		mtk_idle_log("%8s = 0x%08lx\n", mt6779_spm_cond_cg_str[i],
				((b >> i) & 0x1) ?
		    mtk_lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_BLOCK_DETAIL,
					MT_LPM_SMC_ACT_GET,
					MT_RM_CONSTRAINT_ID_DRAM, i) : 0);
	for (i = 0, b = block >> SPM_COND_BLOCKED_PLL_IDX;
	     i < PLAT_SPM_COND_PLL_MAX; i++)
		mtk_idle_log("%8s = %d\n", mt6779_spm_cond_pll_str[i],
				((b >> i) & 0x1));


	mtk_idle_log("*************** idle command help ****************\n");
	mtk_idle_log("echo cond_check 0/1 > %s\n", MT_LP_DRAM_NODE);

	return p - ToUserBuf;
}

static ssize_t mt6779_idle_dram_write(char *FromUserBuf,
				   size_t sz, void *priv)
{
	char cmd[128];
	int parm;

	if (sscanf(FromUserBuf, "%127s %x", cmd, &parm) == 2) {
		if (!strcmp(cmd, "cond_check")) {
			mtk_lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_COND_CHECK,
					    MT_LPM_SMC_ACT_SET,
					    MT_RM_CONSTRAINT_ID_DRAM, !!parm);
		}
		return sz;
	} else if ((!kstrtoint(FromUserBuf, 10, &parm)) == 1) {
		mtk_lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RC_SWITCH,
				    MT_LPM_SMC_ACT_SET,
				    MT_RM_CONSTRAINT_ID_DRAM, !!parm);
		return sz;
	}

	return -EINVAL;
}

static const struct mtk_lp_sysfs_op mt6779_idle_dram_fops = {
	.fs_read = mt6779_idle_dram_read,
	.fs_write = mt6779_idle_dram_write,
};

static ssize_t mt6779_idle_syspll_read(char *ToUserBuf,
					size_t sz, void *priv)
{
	char *p = ToUserBuf;
	uint32_t block, b;
	int i;

	block = mtk_lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_COND_BLOCK,
				    MT_LPM_SMC_ACT_GET,
				    MT_RM_CONSTRAINT_ID_SYSPLL, 0);
	mtk_idle_log("enable=%lu. count=%lu, is_cond_check = %lu\n\n",
		mtk_lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RC_SWITCH,
				    MT_LPM_SMC_ACT_GET,
				    MT_RM_CONSTRAINT_ID_SYSPLL, 0)
		& MT_SPM_RC_VALID_SW,
		mtk_lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RC_CNT,
				    MT_LPM_SMC_ACT_GET,
				    MT_RM_CONSTRAINT_ID_SYSPLL, 0),
		mtk_lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_COND_CHECK,
				    MT_LPM_SMC_ACT_GET,
				    MT_RM_CONSTRAINT_ID_SYSPLL, 0));

	mtk_idle_log("is_cond_block = 0x%08x\n", block);
	for (i = 0, b = block >> SPM_COND_BLOCKED_CG_IDX;
	     i < PLAT_SPM_COND_MAX; i++)
		mtk_idle_log("%8s = 0x%08lx\n", mt6779_spm_cond_cg_str[i],
				((b >> i) & 0x1) ?
		    mtk_lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_BLOCK_DETAIL,
					MT_LPM_SMC_ACT_GET,
					MT_RM_CONSTRAINT_ID_SYSPLL, i) : 0);
	for (i = 0, b = block >> SPM_COND_BLOCKED_PLL_IDX;
	     i < PLAT_SPM_COND_PLL_MAX; i++)
		mtk_idle_log("%8s = %d\n", mt6779_spm_cond_pll_str[i],
				((b >> i) & 0x1));


	mtk_idle_log("*************** idle command help ****************\n");
	mtk_idle_log("echo cond_check 0/1 > %s\n", MT_LP_SYSPLL_NODE);

	return p - ToUserBuf;
}

static ssize_t mt6779_idle_syspll_write(char *FromUserBuf,
				   size_t sz, void *priv)
{
	char cmd[128];
	int parm;

	if (sscanf(FromUserBuf, "%127s %x", cmd, &parm) == 2) {
		if (!strcmp(cmd, "cond_check")) {
			mtk_lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_COND_CHECK,
					    MT_LPM_SMC_ACT_SET,
					    MT_RM_CONSTRAINT_ID_SYSPLL, !!parm);
		}
		return sz;
	} else if ((!kstrtoint(FromUserBuf, 10, &parm)) == 1) {
		mtk_lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RC_SWITCH,
				    MT_LPM_SMC_ACT_SET,
				    MT_RM_CONSTRAINT_ID_SYSPLL, !!parm);
		return sz;
	}

	return -EINVAL;
}

static const struct mtk_lp_sysfs_op mt6779_idle_syspll_fops = {
	.fs_read = mt6779_idle_syspll_read,
	.fs_write = mt6779_idle_syspll_write,
};

static ssize_t mt6779_idle_bus26m_read(char *ToUserBuf,
					size_t sz, void *priv)
{
	char *p = ToUserBuf;
	uint32_t block, b;
	int i;

	block = mtk_lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_COND_BLOCK,
				    MT_LPM_SMC_ACT_GET,
				    MT_RM_CONSTRAINT_ID_BUS26M, 0);
	mtk_idle_log("enable=%lu. count=%lu, is_cond_check = %lu\n\n",
		mtk_lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RC_SWITCH,
				    MT_LPM_SMC_ACT_GET,
				    MT_RM_CONSTRAINT_ID_BUS26M, 0)
		& MT_SPM_RC_VALID_SW,
		mtk_lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RC_CNT,
				    MT_LPM_SMC_ACT_GET,
				    MT_RM_CONSTRAINT_ID_BUS26M, 0),
		mtk_lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_COND_CHECK,
				    MT_LPM_SMC_ACT_GET,
				    MT_RM_CONSTRAINT_ID_BUS26M, 0));

	mtk_idle_log("is_cond_block = 0x%08X\n", block);
	for (i = 0, b = block >> SPM_COND_BLOCKED_CG_IDX;
	     i < PLAT_SPM_COND_MAX; i++)
		mtk_idle_log("%8s = 0x%08lX\n", mt6779_spm_cond_cg_str[i],
				((b >> i) & 0x1) ?
		    mtk_lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_BLOCK_DETAIL,
					MT_LPM_SMC_ACT_GET,
					MT_RM_CONSTRAINT_ID_BUS26M, i) : 0);
	for (i = 0, b = block >> SPM_COND_BLOCKED_PLL_IDX;
	     i < PLAT_SPM_COND_PLL_MAX; i++)
		mtk_idle_log("%8s = %d\n", mt6779_spm_cond_pll_str[i],
				((b >> i) & 0x1));

	mtk_idle_log("*************** idle command help ****************\n");
	mtk_idle_log("echo cond_check 0/1 > %s\n", MT_LP_BUS26M_NODE);

	return p - ToUserBuf;
}

static ssize_t mt6779_idle_bus26m_write(char *FromUserBuf,
				   size_t sz, void *priv)
{
	char cmd[128];
	int parm;

	if (sscanf(FromUserBuf, "%127s %x", cmd, &parm) == 2) {
		if (!strcmp(cmd, "cond_check")) {
			mtk_lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_COND_CHECK,
					    MT_LPM_SMC_ACT_SET,
					    MT_RM_CONSTRAINT_ID_BUS26M, !!parm);
		}
		return sz;
	} else if ((!kstrtoint(FromUserBuf, 10, &parm)) == 1) {
		mtk_lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RC_SWITCH,
				    MT_LPM_SMC_ACT_SET,
				    MT_RM_CONSTRAINT_ID_BUS26M, !!parm);
		return sz;
	}

	return -EINVAL;
}

static const struct mtk_lp_sysfs_op mt6779_idle_bus26m_fops = {
	.fs_read = mt6779_idle_bus26m_read,
	.fs_write = mt6779_idle_bus26m_write,
};

int mt6779_dbg_idle_fs_init(void)
{
	/* enable resource constraint condition block latch */
	mtk_lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_BLOCK_LATCH,
				    MT_LPM_SMC_ACT_SET, 0, 0);
	mtk_lpm_sysfs_root_entry_create();
	mtk_lpm_sysfs_entry_node_add("IdleDram_state"
		      , 0644, &mt6779_idle_dram_fops, NULL);
	mtk_lpm_sysfs_entry_node_add("IdleSyspll_state"
		      , 0644, &mt6779_idle_syspll_fops, NULL);
	mtk_lpm_sysfs_entry_node_add("IdleBus26m_state"
		      , 0644, &mt6779_idle_bus26m_fops, NULL);
	return 0;
}

int mt6779_dbg_idle_fs_deinit(void)
{
	/* disable resource contraint condition block latch */
	mtk_lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_BLOCK_LATCH,
			    MT_LPM_SMC_ACT_CLR, 0, 0);
	return 0;
}
