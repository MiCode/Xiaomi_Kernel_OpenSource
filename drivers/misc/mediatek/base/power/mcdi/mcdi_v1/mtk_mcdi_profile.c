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

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/tick.h>
#include <linux/uaccess.h>

#include <mtk_mcdi.h>
#include <mtk_mcdi_profile.h>
#include <mtk_mcdi_util.h>

#include <mtk_mcdi_plat.h>
#include <mtk_mcdi_reg.h>

/* default profile MCDI_CPU_OFF */
static unsigned int profile_state = 1;
static bool mcdi_prof_en;
static int mcdi_prof_target_cpu;
static unsigned int mcdi_profile_sum_us[NF_MCDI_PROFILE - 1];
static unsigned long long mcdi_profile_ns[NF_MCDI_PROFILE];
static unsigned int mcdi_profile_count;

static DEFINE_SPINLOCK(mcdi_prof_spin_lock);

#ifdef MCDI_PROFILE_BREAKDOWN
const char *prof_item[NF_MCDI_PROFILE - 1] = {
	"gov sel enter",
	"gov sel get lock",
	"gov sel upt res",
	"gov sel any core",
	"gov sel cluster",
	"gov sel leave",
	"dormant enter",
	"dormant leave",
	"gov reflect",
};
#else
const char *prof_item[NF_MCDI_PROFILE - 1] = {
	"gov select",
	"dormant enter",
	"dormant leave",
	"gov reflect",
};
#endif

#ifdef MCDI_PWR_SEQ_PROF_BREAKDOWN
const char *prof_pwr_seq_item[MCDI_PROF_BK_NUM] = {
	"cluster",
	"cpu",
	"armpll",
	"buck",
};
#endif
unsigned int get_mcdi_profile_state(void)
{
	return profile_state;
}

void set_mcdi_profile_sampling(int en)
{
	int i = 0;
	unsigned long flags;

	spin_lock_irqsave(&mcdi_prof_spin_lock, flags);

	mcdi_prof_en = en;

	if (en) {

		for (i = 0; i < NF_MCDI_PROFILE; i++)
			mcdi_profile_ns[i] = 0;

		for (i = 0; i < (NF_MCDI_PROFILE - 1); i++)
			mcdi_profile_sum_us[i] = 0;

		mcdi_profile_count = 0;
	}

	spin_unlock_irqrestore(&mcdi_prof_spin_lock, flags);

	if (!en) {
		pr_info("cpu = %d, sample cnt = %u\n",
			mcdi_prof_target_cpu, mcdi_profile_count);

		for (i = 0; i < (NF_MCDI_PROFILE - 1); i++)
			pr_info("%d: %u\n", i, mcdi_profile_sum_us[i]);
	}
}

void set_mcdi_profile_target_cpu(int cpu)
{
	unsigned long flags;

	if (!(cpu >= 0 && cpu < NF_CPU))
		return;

	spin_lock_irqsave(&mcdi_prof_spin_lock, flags);

	mcdi_prof_target_cpu = cpu;

	spin_unlock_irqrestore(&mcdi_prof_spin_lock, flags);
}

void mcdi_profile_ts(unsigned int idx)
{
	unsigned long flags;
	bool en = false;

	if (!(smp_processor_id() == mcdi_prof_target_cpu))
		return;

	if (idx >= NF_MCDI_PROFILE)
		return;

	spin_lock_irqsave(&mcdi_prof_spin_lock, flags);

	en = mcdi_prof_en;

	spin_unlock_irqrestore(&mcdi_prof_spin_lock, flags);

	if (!en)
		return;

	mcdi_profile_ns[idx] = sched_clock();
}

void mcdi_profile_calc(void)
{
	unsigned long flags;
	unsigned int dur;
	bool en = false;
	int i = 0;

	if (!(smp_processor_id() == mcdi_prof_target_cpu))
		return;

	spin_lock_irqsave(&mcdi_prof_spin_lock, flags);

	en = mcdi_prof_en;

	spin_unlock_irqrestore(&mcdi_prof_spin_lock, flags);

	if (!en)
		return;

	for (i = 0; i < (NF_MCDI_PROFILE - 1); i++) {
		dur = (unsigned int)(mcdi_profile_ns[i + 1]
				- mcdi_profile_ns[i]);
		mcdi_profile_sum_us[i] += (dur / 1000);
	}

	mcdi_profile_count++;
}

int get_mcdi_profile_cpu(void)
{
	unsigned long flags;
	int cpu = 0;

	spin_lock_irqsave(&mcdi_prof_spin_lock, flags);

	cpu = mcdi_prof_target_cpu;

	spin_unlock_irqrestore(&mcdi_prof_spin_lock, flags);

	return cpu;
}

unsigned int get_mcdi_profile_cnt(void)
{
	unsigned long flags;
	unsigned int cnt;

	spin_lock_irqsave(&mcdi_prof_spin_lock, flags);

	cnt = mcdi_profile_count;

	spin_unlock_irqrestore(&mcdi_prof_spin_lock, flags);

	return cnt;
}

unsigned int get_mcdi_profile_sum_us(int idx)
{
	unsigned long flags;
	unsigned int sum_us;

	if (!(idx >= 0 && idx < NF_MCDI_PROFILE - 1))
		return 0;

	spin_lock_irqsave(&mcdi_prof_spin_lock, flags);

	sum_us = mcdi_profile_sum_us[idx];

	spin_unlock_irqrestore(&mcdi_prof_spin_lock, flags);

	return sum_us;
}

/* debugfs */
static char dbg_buf[4096] = { 0 };
static char cmd_buf[512] = { 0 };

static int _mcdi_profile_open(struct seq_file *s, void *data)
{
	return 0;
}

static int mcdi_profile_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, _mcdi_profile_open, inode->i_private);
}

static ssize_t mcdi_profile_read(struct file *filp,
		char __user *userbuf, size_t count, loff_t *f_pos)
{
	int i, len = 0;
	char *p = dbg_buf;
	unsigned int ratio_raw = 0;
	unsigned int ratio_int = 0;
	unsigned int ratio_fraction = 0;
	unsigned int ratio_dur = 0;
	struct {
		unsigned int sec[DISTRIBUTE_NUM];
		unsigned int total;
	} cnt[2] = {0};

#ifdef MCDI_PWR_SEQ_PROF_BREAKDOWN
	struct mcdi_prof_breakdown *p_prof =
			(struct mcdi_prof_breakdown *)SYSRAM_PROF_PWR_SEQ_BK;
	unsigned int j;
#endif

	mcdi_log(
		"mcdi cpu off    : max_id = 0x%x, avg = %4dus, max = %5dus, cnt = %d\n",
		mcdi_read(CPU_OFF_LATENCY_REG(ID_OFS)),
		mcdi_read(CPU_OFF_LATENCY_REG(AVG_OFS)),
		mcdi_read(CPU_OFF_LATENCY_REG(MAX_OFS)),
		mcdi_read(CPU_OFF_LATENCY_REG(CNT_OFS)));
	mcdi_log(
		"mcdi cpu on     : max_id = 0x%x, avg = %4dus, max = %5dus, cnt = %d\n",
		mcdi_read(CPU_ON_LATENCY_REG(ID_OFS)),
		mcdi_read(CPU_ON_LATENCY_REG(AVG_OFS)),
		mcdi_read(CPU_ON_LATENCY_REG(MAX_OFS)),
		mcdi_read(CPU_ON_LATENCY_REG(CNT_OFS)));
	mcdi_log(
		"mcdi cluster off: max_id = 0x%x, avg = %4dus, max = %5dus, cnt = %d\n",
		mcdi_read(Cluster_OFF_LATENCY_REG(ID_OFS)),
		mcdi_read(Cluster_OFF_LATENCY_REG(AVG_OFS)),
		mcdi_read(Cluster_OFF_LATENCY_REG(MAX_OFS)),
		mcdi_read(Cluster_OFF_LATENCY_REG(CNT_OFS)));
	mcdi_log(
		"mcdi cluster on : max_id = 0x%x, avg = %4dus, max = %5dus, cnt = %d\n",
		mcdi_read(Cluster_ON_LATENCY_REG(ID_OFS)),
		mcdi_read(Cluster_ON_LATENCY_REG(AVG_OFS)),
		mcdi_read(Cluster_ON_LATENCY_REG(MAX_OFS)),
		mcdi_read(Cluster_ON_LATENCY_REG(CNT_OFS)));

	mcdi_log("\n");

	for (i = 0; i < DISTRIBUTE_NUM; i++) {
		cnt[0].sec[i] = mcdi_read(PROF_OFF_CNT_REG(i));
		cnt[1].sec[i] = mcdi_read(PROF_ON_CNT_REG(i));
		cnt[0].total += cnt[0].sec[i];
		cnt[1].total += cnt[1].sec[i];
	}

	cnt[0].total = cnt[0].total ? : 1;
	cnt[1].total = cnt[1].total ? : 1;

	for (i = 0; i < DISTRIBUTE_NUM; i++)
		mcdi_log("pwr off latency (section%d) : %2d%% (%d)\n",
				i,
				(100 * cnt[0].sec[i]) / cnt[0].total,
				cnt[0].sec[i]);

	for (i = 0; i < DISTRIBUTE_NUM; i++)
		mcdi_log("pwr on latency  (section%d) : %2d%% (%d)\n",
				i,
				(100 * cnt[1].sec[i]) / cnt[1].total,
				cnt[1].sec[i]);

	ratio_dur = mcdi_read(SYSRAM_PROF_RARIO_DUR);

	if (ratio_dur == 0)
		ratio_dur = ~0;

	mcdi_log("\nOFF %% (cpu):\n");

	for (i = 0; i < NF_CPU; i++) {
		ratio_raw = 100 * mcdi_read(PROF_CPU_RATIO_REG(i));
		ratio_int = ratio_raw / ratio_dur;
		ratio_fraction = (1000 * (ratio_raw % ratio_dur)) / ratio_dur;

		mcdi_log("%d: %3u.%03u%% (%u)\n",
			i, ratio_int, ratio_fraction, ratio_raw/100);
	}

	mcdi_log("\nOFF %% (cluster):\n");

	for (i = 0; i < NF_CLUSTER; i++) {
		ratio_raw      = 100 * mcdi_read(PROF_CLUSTER_RATIO_REG(i));
		ratio_int      = ratio_raw / ratio_dur;
		ratio_fraction = (1000 * (ratio_raw % ratio_dur)) / ratio_dur;

		mcdi_log("%d: %3u.%03u%% (%u)\n",
			i, ratio_int, ratio_fraction, ratio_raw/100);
	}

	mcdi_log("\nprof cpu = %d, count = %d, state = %d, profile_state=%d\n",
				get_mcdi_profile_cpu(),
				get_mcdi_profile_cnt(),
				mcdi_mbox_read(MCDI_MBOX_PROF_CMD),
				profile_state);
	for (i = 0; i < (NF_MCDI_PROFILE - 1); i++)
		mcdi_log("%s: %u\n", prof_item[i], get_mcdi_profile_sum_us(i));

#ifdef MCDI_PWR_SEQ_PROF_BREAKDOWN
	mcdi_log("\npwr seq (us)\n");
	for (j = 0; j < NF_CLUSTER; j++) {
		mcdi_log("\n---cluster %d---\n", j);
		for (i = 0; i < (MCDI_PROF_BK_NUM); i++) {
			mcdi_log("%s on, cnt : %u, %u\n",
				prof_pwr_seq_item[i],
				p_prof->onoff[0].item[i][j],
				p_prof->onoff[0].count[i][j]);
		}
		mcdi_log("\n");
		for (i = 0; i < (MCDI_PROF_BK_NUM); i++) {
			mcdi_log("%s off, cnt : %u, %u\n",
				prof_pwr_seq_item[i],
				p_prof->onoff[1].item[i][j],
				p_prof->onoff[1].count[i][j]);
		}
	}
#endif
	len = p - dbg_buf;

	return simple_read_from_buffer(userbuf, count, f_pos, dbg_buf, len);
}

static ssize_t mcdi_profile_write(struct file *filp,
		const char __user *userbuf, size_t count, loff_t *f_pos)
{
	int ret = 0;
	unsigned long param = 0;
	char *cmd_ptr = cmd_buf;
	char *cmd_str = NULL;
	char *param_str = NULL;

	count = min(count, sizeof(cmd_buf) - 1);

	if (copy_from_user(cmd_buf, userbuf, count))
		return -EFAULT;

	cmd_buf[count] = '\0';

	cmd_str = strsep(&cmd_ptr, " ");

	if (cmd_str == NULL)
		return -EINVAL;

	param_str = strsep(&cmd_ptr, " ");

	if (param_str == NULL)
		return -EINVAL;

	ret = kstrtoul(param_str, 16, &param);

	if (ret < 0)
		return -EINVAL;

	if (!strncmp(cmd_str, "reg", sizeof("reg"))) {
		if (!(param >= 0
				&& param < MCDI_SYSRAM_SIZE
				&& (param % 4) == 0))
			return -EINVAL;

		pr_info("mcdi_reg: 0x%lx=0x%x(%d)\n",
			param, mcdi_read(mcdi_sysram_base + param),
			mcdi_read(mcdi_sysram_base + param));
	} else if (!strncmp(cmd_str, "enable", sizeof("enable"))) {
		if (param == MCDI_PROF_FLAG_STOP
				|| param == MCDI_PROF_FLAG_START)
			set_mcdi_profile_sampling(param);

		if (param == MCDI_PROF_FLAG_STOP
				|| param == MCDI_PROF_FLAG_START
				|| param == MCDI_PROF_FLAG_POLLING) {
			mcdi_mbox_write(MCDI_MBOX_PROF_CMD, param);
		}
	} else if (!strncmp(cmd_str, "cpu", sizeof("cpu"))) {
		set_mcdi_profile_target_cpu(param);
	} else if (!strncmp(cmd_str, "mcdi_state", sizeof("mcdi_state"))) {
		profile_state = param;
	} else if (!strncmp(cmd_str, "cluster", sizeof("cluster"))) {
		mcdi_mbox_write(MCDI_MBOX_PROF_CLUSTER, param);
	} else {
		return -EINVAL;
	}
	return count;
}

static const struct file_operations mcdi_profile_fops = {
	.open = mcdi_profile_open,
	.read = mcdi_profile_read,
	.write = mcdi_profile_write,
	.llseek = seq_lseek,
	.release = single_release,
};

void mcdi_procfs_profile_init(struct proc_dir_entry *mcdi_dir)
{
	if (!proc_create("profile", 0644, mcdi_dir, &mcdi_profile_fops))
		pr_notice("%s(), create /proc/mcdi/%s failed\n",
			__func__, "profile");
}
