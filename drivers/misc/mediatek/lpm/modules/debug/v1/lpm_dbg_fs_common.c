// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/console.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/proc_fs.h>
//#include <linux/spinlock.h>
//#include <linux/syscore_ops.h>

#include <lpm_dbg_common_v1.h>
#include <lpm_module.h>
#include <lpm_resource_constraint_v1.h>
#include <mtk_suspend_sysfs.h>
#include <mtk_spm_sysfs.h>

#define LPM_DGB_SUSP_NODE	"/proc/mtk_lpm/suspend/suspend_state"

#undef lpm_dbg_log
#define lpm_dbg_log(fmt, args...) \
	do { \
		int l = scnprintf(p, sz, fmt, ##args); \
		p += l; \
		sz -= l; \
	} while (0)


static struct wakeup_source *mtk_suspend_lock;

static ssize_t suspend_state_read(char *ToUser, size_t sz, void *priv)
{
	char *p = ToUser;

	if (!ToUser)
		return -EINVAL;

	lpm_dbg_log("mtk_suspend command help\n");
	lpm_dbg_log("kernel suspend disable/enable:\n");
	lpm_dbg_log("echo kernel_suspend 0/1 > %s\n", LPM_DGB_SUSP_NODE);
	lpm_dbg_log("mtk_suspend disable/enable:\n");
	lpm_dbg_log("echo mtk_suspend 0/1 > %s\n", LPM_DGB_SUSP_NODE);

	return p - ToUser;
}

static ssize_t suspend_state_write(char *FromUser,
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
			/* add debug if necessary*/
		}

		return sz;
	}
	return -EINVAL;
}

static const struct mtk_lp_sysfs_op lpm_dbg_suspend_state_fops = {
	.fs_read = suspend_state_read,
	.fs_write = suspend_state_write,
};

static ssize_t get_spm_sleep_count(char *ToUserBuf,
			    size_t sz, void *priv)
{
	int bLen;

	bLen = snprintf(ToUserBuf, sz, "%lu\n",
			lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_SUSPEND_DBG_CTRL,
					    MT_LPM_SMC_ACT_GET,
					    DBG_CTRL_COUNT, 0));

	return (bLen > sz) ? sz : bLen;
}

static const struct mtk_lp_sysfs_op lpm_dbg_spm_sleep_count_fops = {
	.fs_read = get_spm_sleep_count,
};

static void suspend_dbg_fs_init(void)
{
	mtk_suspend_sysfs_root_entry_create();
	mtk_suspend_sysfs_entry_node_add("suspend_state"
			, 0444, &lpm_dbg_suspend_state_fops, NULL);
	mtk_suspend_sysfs_entry_node_add("spm_sleep_count"
			, 0444, &lpm_dbg_spm_sleep_count_fops, NULL);
}

static ssize_t get_spm_last_wakeup_src(char *ToUserBuf,
				size_t sz, void *priv)
{
	int bLen = snprintf(ToUserBuf, sz, "0x%lx\n",
		lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_FS,
				    MT_LPM_SMC_ACT_GET,
				    WAKE_STA_R12, 0));
	return (bLen > sz) ? sz : bLen;
}

static const struct mtk_lp_sysfs_op lpm_dbg_spm_last_wakesrc_fops = {
	.fs_read = get_spm_last_wakeup_src,
};

static ssize_t get_spm_last_debug_flag(char *ToUserBuf,
				size_t sz, void *priv)
{
	int bLen = snprintf(ToUserBuf, sz, "0x%lx\n",
		lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_FS,
				    MT_LPM_SMC_ACT_GET,
				    WAKE_STA_DEBUG_FLAG, 0));
	return (bLen > sz) ? sz : bLen;
}

static const struct mtk_lp_sysfs_op lpm_dbg_spm_last_debugflag_fops = {
	.fs_read = get_spm_last_debug_flag,
};

static ssize_t get_spmfw_version(char *ToUserBuf,
			  size_t sz, void *priv)
{
	int index = 0;
	const char *version;
	char *p = ToUserBuf;

	struct device_node *node =
		of_find_compatible_node(NULL, NULL, "mediatek,sleep");

	if (node == NULL) {
		lpm_dbg_log("No Found mediatek,mediatek,sleep\n");
		goto return_size;
	}

	while (!of_property_read_string_index(node,
		"spmfw_version", index, &version)) {
		lpm_dbg_log("%d: %s\n", index, version);
		index++;
	}

	lpm_dbg_log("spmfw index: %lu\n",
		lpm_smc_spm(MT_SPM_SMC_UID_FW_TYPE,
				MT_LPM_SMC_ACT_GET, 0, 0));
	lpm_dbg_log("spmfw ready: %d\n",
		(lpm_smc_spm_dbg(MT_SPM_DBG_SMC_UID_RC_SWITCH,
				    MT_LPM_SMC_ACT_GET,
				    MT_RM_CONSTRAINT_ID_DRAM, -1)
		& MT_SPM_RC_VALID_FW) ? 1 : 0);

	if (node)
		of_node_put(node);
return_size:
	return p - ToUserBuf;
}

static const struct mtk_lp_sysfs_op lpm_dbg_spm_spmfw_ver_fops = {
	.fs_read = get_spmfw_version,
};

static void spm_dbg_fs_init(void)
{
	mtk_spm_sysfs_root_entry_create();

	mtk_spm_sysfs_entry_node_add("spm_last_wakeup_src", 0444
			, &lpm_dbg_spm_last_wakesrc_fops, NULL);
	mtk_spm_sysfs_entry_node_add("spm_last_debug_flag", 0444
			, &lpm_dbg_spm_last_debugflag_fops, NULL);
	mtk_spm_sysfs_entry_node_add("spmfw_version", 0444
			, &lpm_dbg_spm_spmfw_ver_fops, NULL);
}

static bool lpm_system_console_suspend;

void __exit lpm_dbg_common_fs_exit(void)
{
	/* restore suspend console */
	console_suspend_enabled = lpm_system_console_suspend;

	/* wakeup source deinit */
	wakeup_source_unregister(mtk_suspend_lock);
}

int __init lpm_dbg_common_fs_init(void)
{
	/* wakeup source init for suspend enable and disable */
	mtk_suspend_lock = wakeup_source_register(NULL, "mtk_suspend_wakelock");
	if (!mtk_suspend_lock) {
		pr_info("%s %d: init wakeup source fail!", __func__, __LINE__);
		return -1;
	}
	/* backup and disable suspend console (enable log print) */
	lpm_system_console_suspend = console_suspend_enabled;
	console_suspend_enabled = false;

	suspend_dbg_fs_init();
	spm_dbg_fs_init();

	pr_info("%s %d: finish", __func__, __LINE__);
	return 0;
}
