// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/seq_file.h>
#include <linux/cpuidle.h>

#include <mtk_lp_plat_reg.h>
#include <mtk_lpm.h>
#include <mt6833_spm_comm.h>
#include "mtk_cpupm_dbg.h"
#include "mtk_cpuidle_status.h"
#include "mtk_idle_procfs.h"

#define MTK_IDLE_PROC_FS_NAME	"cpuidle"

static int idle_proc_info_show(struct seq_file *m, void *v)
{
	int cpu;
	unsigned long long ts, rem;

	ts = mtk_cpupm_syssram_read(SYSRAM_RECENT_CNT_TS_H);
	ts = (ts << 32) | mtk_cpupm_syssram_read(SYSRAM_RECENT_CNT_TS_L);
	ts = div64_u64_rem(ts, 1000 * 1000 * 1000, &rem);

	seq_puts(m, "\n========= Power off count =========\n");

	seq_puts(m, "\n-- Within 5 seconds before dumped --\n");
	seq_printf(m, "\nLast dump timestamp = %llu.%09llu\n",
			ts, rem);

	seq_printf(m, "%8s", "cpu:");
	for_each_possible_cpu(cpu)
		seq_printf(m, " %d", mtk_cpupm_syssram_read(
					SYSRAM_RECENT_CPU_CNT(cpu)));
	seq_puts(m, "\n");

	seq_printf(m, "%8s %d\n",
			"cluster:",
			mtk_cpupm_syssram_read(SYSRAM_RECENT_CPUSYS_CNT));
	seq_printf(m, "%8s %d\n",
			"mcusys:",
			mtk_cpupm_syssram_read(SYSRAM_RECENT_MCUSYS_CNT));

	seq_printf(m, "%8s 0x%x\n",
			"online:",
			mtk_cpupm_syssram_read(SYSRAM_CPU_ONLINE));

	seq_puts(m, "\n---- Total ----\n");
	seq_printf(m, "%8s %d\n",
			"cluster:",
			mtk_cpupm_syssram_read(SYSRAM_CPUSYS_CNT)
			+ mtk_cpupm_syssram_read(SYSRAM_RECENT_CPUSYS_CNT));
	seq_printf(m, "%8s %d\n",
			"mcusys:",
			mtk_cpupm_syssram_read(SYSRAM_MCUSYS_CNT)
			+ mtk_cpupm_syssram_read(SYSRAM_RECENT_MCUSYS_CNT));
	return 0;
}

static ssize_t idle_proc_info_write(struct file *filp,
		const char __user *userbuf, size_t count, loff_t *f_pos)
{
	return count;
}

static int idle_proc_enable_show(struct seq_file *m, void *v)
{
	struct cpuidle_driver *drv;
	int i, cpu, en = 0;
	int suspend_type = mtk_lpm_suspend_type_get();

	for_each_possible_cpu(cpu) {

		drv = cpuidle_get_cpu_driver(per_cpu(cpuidle_devices, cpu));
		if (!drv)
			continue;

		for (i = 1; i < drv->state_count; i++) {
			if ((suspend_type == MTK_LPM_SUSPEND_S2IDLE) &&
			    !strcmp(drv->states[i].name, S2IDLE_STATE_NAME))
				continue;
			en += mtk_cpuidle_get_param(drv, i, IDLE_PARAM_EN);
		}
	}

	if (en == 0) {
		seq_printf(m, "MCDI: Disable, %llu ms\n",
			   mtk_cpuidle_state_last_dis_ms());
	} else {
		seq_puts(m, "MCDI: Enable\n");
		seq_puts(m,
		"(cat /proc/cpuidle/state/enabled for more detail)\n");
	}

	return 0;
}

static ssize_t idle_proc_enable_write(struct file *filp,
		const char __user *userbuf, size_t count, loff_t *f_pos)
{
	ssize_t ret = count;
	unsigned int enabled = 0;
	char *buf;

	mtk_idle_procfs_alloc_from_user(buf, userbuf, count);

	if (!buf)
		return -EINVAL;

	if (kstrtouint(buf, 10, &enabled) != 0) {
		ret = -EINVAL;
		goto free;
	}

	mtk_cpuidle_state_enable(!!enabled);

free:
	mtk_idle_procfs_free(buf);

	return ret;
}

PROC_FOPS(info);
PROC_FOPS(enable);
int __init mtk_idle_procfs_init(void)
{
	int i;
	struct proc_dir_entry *dir = NULL;

	const struct idle_proc_entry entries[] = {
		PROC_ENTRY(info),
		PROC_ENTRY(enable)
	};

	dir = proc_mkdir(MTK_IDLE_PROC_FS_NAME, NULL);

	if (!dir) {
		pr_notice("fail to create procfs @ %s()\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++)
		PROC_CREATE_NODE(dir, entries[i]);

	mtk_idle_procfs_state_dir_init(dir);
	mtk_idle_procfs_cpc_dir_init(dir);
	mtk_idle_procfs_profile_dir_init(dir);
	mtk_idle_procfs_control_dir_init(dir);

	return 0;
}

void __exit mtk_idle_procfs_exit(void)
{
	remove_proc_subtree(MTK_IDLE_PROC_FS_NAME, NULL);
}
