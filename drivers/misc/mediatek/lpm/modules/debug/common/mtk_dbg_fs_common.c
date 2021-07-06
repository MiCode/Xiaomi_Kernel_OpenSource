// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/console.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/syscore_ops.h>

#include <mtk_dbg_common_v1.h>
#include <mtk_lpm_module.h>
#include <mtk_resource_constraint_v1.h>
#include <mtk_idle_sysfs.h>
#include <mtk_suspend_sysfs.h>
#include <mtk_spm_sysfs.h>
#include <mtk_power_gs_api.h>
#include <mt-plat/mtk_ccci_common.h>

#define MTK_DGB_SUSP_NODE	"/sys/kernel/debug/suspend/suspend_state"

#undef mtk_dbg_log
#define mtk_dbg_log(fmt, args...) \
	do { \
		int l = scnprintf(p, sz, fmt, ##args); \
		p += l; \
		sz -= l; \
	} while (0)


static struct wakeup_source *mtk_suspend_lock;

/* debugfs for blocking syscore callback */
static int spm_syscore_block_suspend(void) { return -EINVAL; }
static void spm_syscore_block_resume(void) {}

static struct syscore_ops spm_block_syscore_ops = {
	.suspend = spm_syscore_block_suspend,
	.resume = spm_syscore_block_resume,
};

static unsigned int mtk_suspend_debug_flag;
static unsigned int power_golden_dump_type = GS_ALL;

/* debugfs for debug in syscore callback */
static int spm_syscore_dbg_suspend(void)
{
	if (mtk_suspend_debug_flag & MTK_DUMP_GPIO)
		mtk_suspend_gpio_dbg();
#if !defined(CONFIG_FPGA_EARLY_PORTING)
#ifdef CONFIG_MTK_BASE_POWER
	if (mtk_suspend_debug_flag & MTK_DUMP_LP_GOLDEN)
		mt_power_gs_dump_suspend(power_golden_dump_type);
#endif
#endif
	mtk_suspend_clk_dbg();
	return 0;
}
static void spm_syscore_dbg_resume(void) {}

static struct syscore_ops spm_dbg_syscore_ops = {
	.suspend = spm_syscore_dbg_suspend,
	.resume = spm_syscore_dbg_resume,
};

static ssize_t mtk_dbg_suspend_state_read(char *ToUser, size_t sz, void *priv)
{
	char *p = ToUser;

	if (!ToUser)
		return -EINVAL;

	mtk_dbg_log("mtk_suspend command help\n");
	mtk_dbg_log("kernel suspend disable/enable:\n");
	mtk_dbg_log("echo kernel_suspend 0/1 > %s\n", MTK_DGB_SUSP_NODE);
	mtk_dbg_log("mtk_suspend disable/enable:\n");
	mtk_dbg_log("echo mtk_suspend 0/1 > %s\n", MTK_DGB_SUSP_NODE);
	mtk_dbg_log("gpio dump disable/enable:\n");
	mtk_dbg_log("echo gpio_dump 0/1 > %s\n", MTK_DGB_SUSP_NODE);
	mtk_dbg_log("golden dump disable/enable:\n");
	mtk_dbg_log("echo golden_dump 0/1 > %s\n", MTK_DGB_SUSP_NODE);
	mtk_dbg_log("golden type setting (default)PMIC[0], CG[1], DCM[2]:\n");
	mtk_dbg_log("echo golden_type 1/3/7 > %s\n", MTK_DGB_SUSP_NODE);

	return p - ToUser;
}

static ssize_t mtk_dbg_suspend_state_write(char *FromUser,
						   size_t sz, void *priv)
{
	char cmd[128];
	int param;

	if (!FromUser)
		return -EINVAL;

	if (sscanf(FromUser, "%127s %d", cmd, &param) == 2) {
		if (!strcmp(cmd, "kernel_suspend")) {
			/* 0:require wakelock */
			if (!param)
				__pm_stay_awake(mtk_suspend_lock);
			/* 1:release wakelock */
			else
				__pm_relax(mtk_suspend_lock);
		} else if (!strcmp(cmd, "mtk_suspend")) {
			/* 0:block in syscore */
			if (!param)
				register_syscore_ops(&spm_block_syscore_ops);
			/* 1:unblock in syscore */
			else
				unregister_syscore_ops(&spm_block_syscore_ops);
		} else if (!strcmp(cmd, "gpio_dump")) {
			if (param)
				mtk_suspend_debug_flag |= MTK_DUMP_GPIO;
			else
				mtk_suspend_debug_flag &= ~(MTK_DUMP_GPIO);
		} else if (!strcmp(cmd, "golden_dump")) {
			if (param)
				mtk_suspend_debug_flag |= MTK_DUMP_LP_GOLDEN;
			else
				mtk_suspend_debug_flag &= ~(MTK_DUMP_LP_GOLDEN);
		} else if (!strcmp(cmd, "golden_type"))
			power_golden_dump_type = (param & 0xf);

		return sz;
	}
	return -EINVAL;
}

static const struct mtk_lp_sysfs_op mtk_dbg_suspend_state_fops = {
	.fs_read = mtk_dbg_suspend_state_read,
	.fs_write = mtk_dbg_suspend_state_write,
};

static ssize_t mtk_dbg_get_spm_sleep_count(char *ToUserBuf,
			    size_t sz, void *priv)
{
	int bLen;

	bLen = snprintf(ToUserBuf, sz, "%lu\n",
			mtk_lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_SUSPEND_DBG_CTRL,
					    MT_LPM_SMC_ACT_GET,
					    DBG_CTRL_COUNT, 0));

	return (bLen > sz) ? sz : bLen;
}

static const struct mtk_lp_sysfs_op mtk_dbg_spm_sleep_count_fops = {
	.fs_read = mtk_dbg_get_spm_sleep_count,
};

static void mtk_dbg_suspend_fs_init(void)
{
	mtk_suspend_sysfs_root_entry_create();
	mtk_suspend_sysfs_entry_node_add("suspend_state"
			, 0444, &mtk_dbg_suspend_state_fops, NULL);
	mtk_suspend_sysfs_entry_node_add("spm_sleep_count"
			, 0444, &mtk_dbg_spm_sleep_count_fops, NULL);
}

static ssize_t mtk_dbg_get_spm_last_wakeup_src(char *ToUserBuf,
				size_t sz, void *priv)
{
	int bLen = snprintf(ToUserBuf, sz, "0x%lx\n",
		mtk_lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_FS,
				    MT_LPM_SMC_ACT_GET,
				    WAKE_STA_R12, 0));
	return (bLen > sz) ? sz : bLen;
}

static const struct mtk_lp_sysfs_op mtk_dbg_spm_last_wakesrc_fops = {
	.fs_read = mtk_dbg_get_spm_last_wakeup_src,
};

static ssize_t mtk_dbg_get_spm_last_debug_flag(char *ToUserBuf,
				size_t sz, void *priv)
{
	int bLen = snprintf(ToUserBuf, sz, "0x%lx\n",
		mtk_lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_FS,
				    MT_LPM_SMC_ACT_GET,
				    WAKE_STA_DEBUG_FLAG, 0));
	return (bLen > sz) ? sz : bLen;
}

static const struct mtk_lp_sysfs_op mtk_dbg_spm_last_debugflag_fops = {
	.fs_read = mtk_dbg_get_spm_last_debug_flag,
};

static ssize_t mtk_dbg_get_spmfw_version(char *ToUserBuf,
			  size_t sz, void *priv)
{
	int index = 0;
	const char *version;
	char *p = ToUserBuf;

	struct device_node *node =
		of_find_compatible_node(NULL, NULL, "mediatek,sleep");

	if (node == NULL) {
		mtk_dbg_log("No Found mediatek,mediatek,sleep\n");
		goto return_size;
	}

	while (!of_property_read_string_index(node,
		"spmfw_version", index, &version)) {
		mtk_dbg_log("%d: %s\n", index, version);
		index++;
	}

	mtk_dbg_log("spmfw index: %lu\n",
		mtk_lpm_smc_spm(MT_SPM_SMC_UID_FW_TYPE,
				MT_LPM_SMC_ACT_GET, 0, 0));
	mtk_dbg_log("spmfw ready: %d\n",
		(mtk_lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RC_SWITCH,
				    MT_LPM_SMC_ACT_GET,
				    MT_RM_CONSTRAINT_ID_DRAM, -1)
		& MT_SPM_RC_VALID_FW) ? 1 : 0);

	if (node)
		of_node_put(node);
return_size:
	return p - ToUserBuf;
}

static const struct mtk_lp_sysfs_op mtk_dbg_spm_spmfw_ver_fops = {
	.fs_read = mtk_dbg_get_spmfw_version,
};

#define MD_SLEEP_INFO_SMEM_OFFEST (4)
static ssize_t mtk_dbg_get_system_stats(char *ToUserBuf,
			  size_t sz, void *priv)
{
	/* dump sleep info */
#if defined(CONFIG_MTK_ECCCI_DRIVER)
	u32 len = 0;
	u32 *share_mem = NULL;
	struct md_sleep_status md_data;

	share_mem = (u32 *)get_smem_start_addr(MD_SYS1,
		SMEM_USER_LOW_POWER, NULL);
	memset(&md_data, 0, sizeof(struct md_sleep_status));
	if (share_mem != NULL) {
		share_mem = share_mem + MD_SLEEP_INFO_SMEM_OFFEST;
		memcpy(&md_data, share_mem, sizeof(struct md_sleep_status));
	}
	len = snprintf(ToUserBuf, sz,
	"26M:%lld:%lld.%03lld\nAP:%lld:%lld.%03lld\nMD:%lld:%lld.%03lld\n",
	spm_26M_off_count,
	PCM_TICK_TO_SEC(spm_26M_off_duration),
	PCM_TICK_TO_SEC((spm_26M_off_duration % PCM_32K_TICKS_PER_SEC)
		* 1000),
	ap_pd_count,
	PCM_TICK_TO_SEC(ap_slp_duration),
	PCM_TICK_TO_SEC((ap_slp_duration % PCM_32K_TICKS_PER_SEC)
		* 1000),
	md_data.sleep_cnt,
	PCM_TICK_TO_SEC(md_data.sleep_time),
	PCM_TICK_TO_SEC((md_data.sleep_time % PCM_32K_TICKS_PER_SEC)
		* 1000));

	return (len > sz) ? sz : len;
#else
	return 0;
#endif
}

static const struct mtk_lp_sysfs_op mtk_dbg_spm_system_stats_fops = {
	.fs_read = mtk_dbg_get_system_stats,
};

static void mtk_dbg_spm_fs_init(void)
{
	mtk_spm_sysfs_root_entry_create();

	mtk_spm_sysfs_entry_node_add("spm_last_wakeup_src", 0444
			, &mtk_dbg_spm_last_wakesrc_fops, NULL);
	mtk_spm_sysfs_entry_node_add("spm_last_debug_flag", 0444
			, &mtk_dbg_spm_last_debugflag_fops, NULL);
	mtk_spm_sysfs_entry_node_add("spmfw_version", 0444
			, &mtk_dbg_spm_spmfw_ver_fops, NULL);
	mtk_spm_sysfs_entry_node_add("system_stats", 0444
			, &mtk_dbg_spm_system_stats_fops, NULL);
}

static bool mtk_system_console_suspend;

static void __exit mtk_dbg_common_fs_exit(void)
{
	/* restore suspend console */
	console_suspend_enabled = mtk_system_console_suspend;

	/* wakeup source deinit */
	wakeup_source_unregister(mtk_suspend_lock);
	/* remove syscore callback */
	unregister_syscore_ops(&spm_block_syscore_ops);
	unregister_syscore_ops(&spm_dbg_syscore_ops);
}

static int __init mtk_dbg_common_fs_init(void)
{
	/* wakeup source init for suspend enable and disable */
	mtk_suspend_lock = wakeup_source_register(NULL, "mtk_suspend_wakelock");
	if (!mtk_suspend_lock) {
		pr_info("%s %d: init wakeup source fail!", __func__, __LINE__);
		return -1;
	}
	/* backup and disable suspend console (enable log print) */
	mtk_system_console_suspend = console_suspend_enabled;
	console_suspend_enabled = false;

	mtk_dbg_suspend_fs_init();
	mtk_dbg_spm_fs_init();
	register_syscore_ops(&spm_dbg_syscore_ops);

	pr_info("%s %d: finish", __func__, __LINE__);
	return 0;
}

module_init(mtk_dbg_common_fs_init);
module_exit(mtk_dbg_common_fs_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek Low Power FileSystem");
MODULE_AUTHOR("MediaTek Inc.");
