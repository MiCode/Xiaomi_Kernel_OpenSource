// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <mtk_cpuidle_sysfs.h>
#include <lpm_dbg_fs_common.h>

#if IS_ENABLED(CONFIG_MTK_LPM_MT6983)
#include <lpm_dbg_cpc_v5.h>
#else
#include <lpm_dbg_cpc_v3.h>
#endif

#include <lpm_dbg_syssram_v1.h>

#include "mtk_cpupm_dbg.h"
#include "mtk_cpuidle_status.h"
#include "mtk_cpuidle_cpc.h"

static ssize_t cpuidle_info_read(char *ToUserBuf, size_t sz, void *priv)
{
	char *p = ToUserBuf;
	int cpu;
	unsigned long long ts, rem;

	if (!p)
		return -EINVAL;

	ts = mtk_cpupm_syssram_read(SYSRAM_RECENT_CNT_TS_H);
	ts = (ts << 32) | mtk_cpupm_syssram_read(SYSRAM_RECENT_CNT_TS_L);
	ts = div64_u64_rem(ts, 1000 * 1000 * 1000, &rem);

	mtk_dbg_cpuidle_log("\n========= Power off count =========\n");

	mtk_dbg_cpuidle_log("\n-- Within 5 seconds before dumped --\n");
	mtk_dbg_cpuidle_log("\nLast dump timestamp = %llu.%09llu\n",
			ts, rem);

	mtk_dbg_cpuidle_log("%8s", "cpu:");
	for_each_possible_cpu(cpu)
		mtk_dbg_cpuidle_log(" %d", mtk_cpupm_syssram_read(
					SYSRAM_RECENT_CPU_CNT(cpu)));
	mtk_dbg_cpuidle_log("\n");

	mtk_dbg_cpuidle_log("%8s %d\n",
			"cluster:",
			mtk_cpupm_syssram_read(SYSRAM_RECENT_CPUSYS_CNT));
	mtk_dbg_cpuidle_log("%8s %d\n",
			"mcusys:",
			mtk_cpupm_syssram_read(SYSRAM_RECENT_MCUSYS_CNT));

	mtk_dbg_cpuidle_log("%8s 0x%x\n",
			"online:",
			mtk_cpupm_syssram_read(SYSRAM_CPU_ONLINE));

	mtk_dbg_cpuidle_log("\n---- Total ----\n");
	mtk_dbg_cpuidle_log("%8s %d\n",
			"cluster:",
			mtk_cpupm_syssram_read(SYSRAM_CPUSYS_CNT)
			+ mtk_cpupm_syssram_read(SYSRAM_RECENT_CPUSYS_CNT));
	mtk_dbg_cpuidle_log("%8s %d\n",
			"mcusys:",
			mtk_cpupm_syssram_read(SYSRAM_MCUSYS_CNT)
			+ mtk_cpupm_syssram_read(SYSRAM_RECENT_MCUSYS_CNT));

	return p - ToUserBuf;
}

static ssize_t cpuidle_enable_read(char *ToUserBuf, size_t sz, void *priv)
{
	char *p = ToUserBuf;
	long en = mtk_cpuidle_state_enabled();

	if (!p)
		return -EINVAL;

	if (en == 0) {
		mtk_dbg_cpuidle_log("MCDI: Disable, %llu ms\n",
			   mtk_cpuidle_state_last_dis_ms());
	} else {
		mtk_dbg_cpuidle_log("MCDI: Enable\n");
		mtk_dbg_cpuidle_log(
		"(cat /proc/mtk_lpm/cpuidle/state/enabled for more detail)\n");
	}

	return p - ToUserBuf;
}

static ssize_t cpuidle_enable_write(char *FromUserBuf, size_t sz, void *priv)
{
	unsigned int enabled = 0;

	if (!FromUserBuf)
		return -EINVAL;

	if (!kstrtouint(FromUserBuf, 10, &enabled)) {
		mtk_cpuidle_state_enable(!!enabled);
		return sz;
	}

	return -EINVAL;
}

static const struct mtk_lp_sysfs_op cpuidle_info_fops = {
	.fs_read = cpuidle_info_read,
};

static const struct mtk_lp_sysfs_op cpuidle_enable_fops = {
	.fs_read = cpuidle_enable_read,
	.fs_write = cpuidle_enable_write,
};

int lpm_cpuidle_fs_init(void)
{
	mtk_cpuidle_sysfs_root_entry_create();

	mtk_cpuidle_sysfs_entry_node_add("info", MTK_CPUIDLE_SYS_FS_MODE
			, &cpuidle_info_fops, NULL);

	mtk_cpuidle_sysfs_entry_node_add("enable", MTK_CPUIDLE_SYS_FS_MODE
			, &cpuidle_enable_fops, NULL);

	lpm_cpuidle_control_init();

	mtk_cpuidle_cpc_init();

	lpm_cpuidle_profile_init();

	lpm_cpuidle_state_init();

	return 0;
}
EXPORT_SYMBOL(lpm_cpuidle_fs_init);

int lpm_cpuidle_fs_deinit(void)
{
	return 0;
}
EXPORT_SYMBOL(lpm_cpuidle_fs_deinit);
