/*
 * Copyright (C) 2018 MediaTek Inc.
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
#include <mtk_mcdi_governor.h>
#include <mtk_mcdi_profile.h>
#include <mtk_mcdi_util.h>

#include <mtk_mcdi_plat.h>
#include <mtk_mcdi_state.h>
#include <mtk_mcdi_reg.h>

/* default profile MCDI_CPU_OFF */
static int profile_state = -1;

static DEFINE_SPINLOCK(mcdi_prof_spin_lock);

static struct mcdi_prof_lat mcdi_lat = {
	.enable = false,
	.pause  = false,
	.section[MCDI_PROFILE_CPU_DORMANT_ENTER] = {
		.name	= "mcdi enter",
		.start	= MCDI_PROFILE_ENTER,
		.end	= MCDI_PROFILE_CPU_DORMANT_ENTER,
	},
	.section[MCDI_PROFILE_LEAVE] = {
		.name	= "mcdi leave",
		.start	= MCDI_PROFILE_CPU_DORMANT_LEAVE,
		.end	= MCDI_PROFILE_LEAVE,
	},
#if 0
	/**
	 * Profiling specific section:
	 *    1. Add MCDI_PROFILE_XXX in mtk_mcdi_profile.h
	 *    2. Need to set member information:
	 *           'name', 'start' and 'end' are necessary.
	 *    3. Put mcdi_profile_ts(cpu, MCDI_PROFILE_XXX)
	 */
	.section[MCDI_PROFILE_RSV1] = {
		.name	= "Test section",
		.start	= MCDI_PROFILE_RSV0,
		.end	= MCDI_PROFILE_RSV1,
	},
#endif
};

static struct mcdi_prof_usage mcdi_usage;

#ifdef MCDI_PWR_SEQ_PROF_BREAKDOWN
const char *prof_pwr_seq_item[MCDI_PROF_BK_NUM] = {
	"cluster",
	"cpu",
	"armpll",
	"buck",
};
#endif

void mcdi_prof_set_idle_state(int cpu, int state)
{
	if ((cpu >= 0) && (cpu < NF_CPU))
		mcdi_usage.dev[cpu].actual_state = state;
}

static void set_mcdi_profile_sampling(int en)
{
	struct mcdi_prof_lat_raw *curr;
	struct mcdi_prof_lat_raw *res;
	unsigned long flags;
	int i, j;

	if (mcdi_lat.enable == en)
		return;

	mcdi_lat.pause = true;
	mcdi_lat.enable = en;

	spin_lock_irqsave(&mcdi_prof_spin_lock, flags);

	if (!en) {
		for (i = 0; i < NF_MCDI_PROFILE; i++) {
			for (j = 0; j < NF_CPU_TYPE; j++) {

				curr = &mcdi_lat.section[i].curr[j];
				res = &mcdi_lat.section[i].result[j];

				res->avg = curr->cnt ?
					div64_u64(curr->sum, curr->cnt) : 0;
				res->max = curr->max;
				res->cnt = curr->cnt;

				memset(curr, 0,
					sizeof(struct mcdi_prof_lat_raw));
			}

			for (j = 0; j < NF_CPU; j++)
				mcdi_lat.section[i].ts[j] = 0;
		}
	}

	spin_unlock_irqrestore(&mcdi_prof_spin_lock, flags);

	mcdi_lat.pause = false;
}

void mcdi_prof_core_cluster_off_token(int cpu)
{
	unsigned long flags;

	if (cpu_is_invalid(cpu))
		return;

	spin_lock_irqsave(&mcdi_prof_spin_lock, flags);

	mcdi_usage.last_id[cluster_idx_get(cpu)] = cpu;

	spin_unlock_irqrestore(&mcdi_prof_spin_lock, flags);
}

static void mcdi_usage_save(struct mcdi_prof_dev *dev, int entered_state,
		unsigned long long enter_ts, unsigned long long leave_ts)
{
	int cpu_idx, cluster;
	unsigned long flags;
	s64 diff;

	diff = (s64)div64_u64(leave_ts - enter_ts, 1000);

	if (diff > INT_MAX)
		diff = INT_MAX;

	dev->last_residency = (int) diff;

	if (entered_state > MCDI_STATE_CPU_OFF)
		dev->state[MCDI_STATE_CPU_OFF].dur += dev->last_residency;
	if (entered_state > MCDI_STATE_CLUSTER_OFF)
		dev->state[MCDI_STATE_CLUSTER_OFF].dur += dev->last_residency;

	if (entered_state == MCDI_STATE_CLUSTER_OFF) {

		spin_lock_irqsave(&mcdi_prof_spin_lock, flags);

		cluster = cluster_idx_get(dev->cpu);
		cpu_idx = mcdi_usage.last_id[cluster];

		if (!cpu_is_invalid(cpu_idx)) {

			enter_ts = mcdi_usage.dev[cpu_idx].enter;

			if (enter_ts > mcdi_usage.start)
				diff = (s64)div64_u64(
						leave_ts - enter_ts, 1000);
			else
				diff = 0;

			dev->state[MCDI_STATE_CLUSTER_OFF].dur += (int) diff;

			mcdi_usage.last_id[cluster] = -1;
		}

		spin_unlock_irqrestore(&mcdi_prof_spin_lock, flags);

	} else {
		if ((entered_state >= 0) && (entered_state < NF_MCDI_STATE))
			dev->state[entered_state].dur += dev->last_residency;
	}
}

static void mcdi_usage_enable(int en)
{
	struct mcdi_prof_dev *dev;
	int i, j;

	if (mcdi_usage.enable == en)
		return;

	mcdi_usage.enable = en;

	if (en) {

		for (i = 0; i < NF_CPU; i++) {

			dev = &mcdi_usage.dev[i];

			dev->enter = 0;
			dev->leave = 0;

			for (j = 0; j < NF_MCDI_STATE; j++)
				dev->state[j].dur = 0;
		}

		mcdi_usage.start = sched_clock();

	} else {

		mcdi_usage.prof_dur = div64_u64(
				sched_clock() - mcdi_usage.start, 1000);

		for (i = 0; i < NF_CPU; i++) {

			dev = &mcdi_usage.dev[i];

			if (dev->actual_state < 0
				|| dev->actual_state > MCDI_STATE_CPU_OFF)
				continue;

			if (dev->enter > dev->leave)
				mcdi_usage_save(dev, dev->actual_state,
					dev->enter, sched_clock());
		}

	}
}

static inline bool mcdi_usage_may_never_wakeup(int cpu)
{
	return is_mcdi_working() && mcdi_usage_cpu_valid(cpu);
}

static unsigned long long mcdi_usage_get_time(int cpu, int state_idx)
{
	struct mcdi_prof_dev *dev = &mcdi_usage.dev[cpu];
	unsigned long long dur = 0;

	if ((state_idx >= 0) && (state_idx < NF_MCDI_STATE))
		dur = dev->state[state_idx].dur;

	if (state_idx == MCDI_STATE_CPU_OFF && dur == 0) {
		if (mcdi_usage_may_never_wakeup(cpu))
			dur = mcdi_usage.prof_dur;
	}

	return dur;
}

unsigned int mcdi_usage_get_cnt(int cpu, int state_idx)
{
	if (cpu_is_invalid(cpu) || state_idx < 0)
		return 0;

	return mcdi_usage.dev[cpu].state[state_idx].cnt;
}

void mcdi_usage_time_start(int cpu)
{
	if (!mcdi_usage.enable)
		return;

	if ((cpu >= 0) && (cpu < NF_CPU))
		mcdi_usage.dev[cpu].enter = sched_clock();
}

void mcdi_usage_time_stop(int cpu)
{
	if (!mcdi_usage.enable)
		return;

	if ((cpu >= 0) && (cpu < NF_CPU))
		mcdi_usage.dev[cpu].leave = sched_clock();
}

void mcdi_usage_calc(int cpu)
{
	int entered_state;
	struct mcdi_prof_dev *dev = &mcdi_usage.dev[cpu];
	unsigned long long leave_ts, enter_ts;

	entered_state = dev->actual_state;

	if (!((entered_state >= 0) && (entered_state < NF_MCDI_STATE)))
		return;

	dev->state[entered_state].cnt++;
	dev->last_state_idx = entered_state;
	dev->actual_state = -1;

	if (!mcdi_usage.enable)
		return;

	leave_ts = dev->leave;
	enter_ts = dev->enter;

	if (dev->leave && !dev->enter)
		enter_ts = mcdi_usage.start;
	else if (!dev->leave && dev->enter)
		leave_ts = sched_clock();

	mcdi_usage_save(dev, entered_state, enter_ts, leave_ts);
}

static bool mcdi_profile_matched_state(int cpu)
{
	if (profile_state < 0)
		return true;

	/* Idle state was saved to last_state_idx in mcdi_usage_calc() */
	if ((cpu >= 0) && (cpu < NF_CPU))
		return profile_state == mcdi_usage.dev[cpu].last_state_idx;
	else
		return false;
}

void mcdi_profile_ts(int cpu_idx, unsigned int prof_idx)
{
	if (!mcdi_lat.enable)
		return;

	if (unlikely(prof_idx >= NF_MCDI_PROFILE))
		return;

	mcdi_lat.section[prof_idx].ts[cpu_idx] = sched_clock();
}

void mcdi_profile_ts_clr(int cpu_idx, unsigned int prof_idx)
{
	if (!mcdi_lat.enable)
		return;

	if (unlikely(prof_idx >= NF_MCDI_PROFILE))
		return;

	mcdi_lat.section[prof_idx].ts[cpu_idx] = 0;
}

void mcdi_profile_calc(int cpu)
{
	int cpu_type;
	struct mcdi_prof_lat_data *data;
	struct mcdi_prof_lat_raw *raw;
	unsigned long long start;
	unsigned long long end;
	unsigned int dur;
	unsigned long flags;
	int i = 0;

	if (!mcdi_lat.enable || mcdi_lat.pause)
		return;

	if (unlikely(cpu_is_invalid(cpu)))
		return;

	if (!mcdi_profile_matched_state(cpu))
		return;

	cpu_type = cpu_type_idx_get(cpu);

	spin_lock_irqsave(&mcdi_prof_spin_lock, flags);

	for (i = 0; i < NF_MCDI_PROFILE; i++) {

		data = &mcdi_lat.section[i];

		if (data->valid == false)
			continue;

		raw = &data->curr[cpu_type];

		start = data->start_ts[cpu];
		end = data->end_ts[cpu];

		if (unlikely(start == 0) || end == 0)
			continue;

		dur = (unsigned int)((end - start) & 0xFFFFFFFF);

		if ((dur & BIT(31)))
			continue;

		if (raw->max < dur)
			raw->max = dur;

		raw->sum += dur;
		raw->cnt++;
	}
	for (i = 0; i < NF_MCDI_PROFILE; i++)
		mcdi_profile_ts_clr(cpu, i);

	spin_unlock_irqrestore(&mcdi_prof_spin_lock, flags);
}

/* debugfs */
static char dbg_buf[4096] = { 0 };
static char cmd_buf[512] = { 0 };

static ssize_t mcdi_profile_read(struct file *filp,
		char __user *userbuf, size_t count, loff_t *f_pos)
{
	int i, j, len = 0;
	char *p = dbg_buf;
	unsigned int data_raw = 0;
	unsigned int data_int = 0;
	unsigned int data_frac = 0;
	unsigned int ratio_dur = 0;
	struct {
		unsigned int sec[DISTRIBUTE_NUM];
		unsigned int total;
	} cnt[2] = { { { 0 }, 0 }, { { 0 }, 0 } };

#ifdef MCDI_PWR_SEQ_PROF_BREAKDOWN
	struct mcdi_prof_breakdown *p_prof =
			(struct mcdi_prof_breakdown *)SYSRAM_PROF_PWR_SEQ_BK;
	unsigned int j;
#endif

	if (mcdi_is_cpc_mode())
		goto skip_sram_data;

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
		data_raw = 100 * mcdi_read(PROF_CPU_RATIO_REG(i));
		data_int = data_raw / ratio_dur;
		data_frac = (1000 * (data_raw % ratio_dur)) / ratio_dur;

		mcdi_log("%d: %3u.%03u%% (%u)\n",
			i, data_int, data_frac, data_raw/100);
	}

	mcdi_log("\nOFF %% (cluster):\n");

	for (i = 0; i < NF_CLUSTER; i++) {
		data_raw  = 100 * mcdi_read(PROF_CLUSTER_RATIO_REG(i));
		data_int  = data_raw / ratio_dur;
		data_frac = (1000 * (data_raw % ratio_dur)) / ratio_dur;

		mcdi_log("%d: %3u.%03u%% (%u)\n",
			i, data_int, data_frac, data_raw/100);
	}

skip_sram_data:

	mcdi_log("\ncurrent profiling state = %d, profile target state=%d\n",
				mcdi_mbox_read(MCDI_MBOX_PROF_CMD),
				profile_state);

	for (i = 0; i < NF_MCDI_PROFILE; i++) {
		struct mcdi_prof_lat_data *data;

		data = &mcdi_lat.section[i];

		if (data->valid == false)
			continue;

		mcdi_log("\n%s:\n", data->name);

		for (j = 0; j < NF_CPU_TYPE; j++) {
			data_raw  = (unsigned int)(data->result[j].avg);
			data_int  = data_raw / 1000;
			data_frac = data_raw % 1000;

			mcdi_log("\tCPU_TYPE %d, ", j);
			mcdi_log("avg = %u.%03uus, max = %u.%03uus, cnt = %d\n",
				data_int, data_frac,
				data->result[j].max / 1000,
				data->result[j].max % 1000,
				data->result[j].cnt);
		}
	}

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

bool mcdi_usage_cpu_valid(int cpu)
{
	return (mcdi_usage.cpu_valid & (1 << cpu));
}

static void mcdi_usage_cpu_mask(unsigned int cpu_mask)
{
	mcdi_usage.cpu_valid = cpu_mask & ((1 << NF_CPU) - 1);
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
		if (param >= MCDI_SYSRAM_SIZE || (param % 4) != 0)
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
	} else if (!strncmp(cmd_str, "mcdi_state", sizeof("mcdi_state"))) {
		profile_state = param;
	} else if (!strncmp(cmd_str, "cluster", sizeof("cluster"))) {
		mcdi_mbox_write(MCDI_MBOX_PROF_CLUSTER, param);
	} else {
		return -EINVAL;
	}

	return count;
}

static ssize_t mcdi_usage_read(struct file *filp,
		char __user *userbuf, size_t count, loff_t *f_pos)
{
	struct cpuidle_driver *drv;
	int i, cpu, len = 0;
	int state_idx;
	char *p = dbg_buf;
	unsigned long long ratio_raw = 0;
	unsigned long long ratio_dur = 0;
	unsigned long long ratio_rem = 0;
	unsigned int ratio_int = 0;
	unsigned int ratio_frac = 0;

	if (mcdi_usage.enable) {
		mcdi_log("Busy. need to stop profiling and get result\n");
		goto finish;
	}

	drv = mcdi_state_tbl_get(0);

	ratio_dur = mcdi_usage.prof_dur;

	mcdi_log("\nCPU valid mask = 0x%x, duration of profiling = %llu ms\n",
			mcdi_usage.cpu_valid,
			div64_u64(ratio_dur, 1000));

	mcdi_log("\ncpu");
	for (state_idx = 0; state_idx <= NF_MCDI_STATE; state_idx++)
		mcdi_log("%12s ", drv->states[state_idx].name);

	mcdi_log("\n");
	for (cpu = 0; cpu < NF_CPU; cpu++) {
		mcdi_log("%d: ", cpu);
		for (state_idx = 0; state_idx < NF_MCDI_STATE; state_idx++) {
			ratio_raw = 100 *
				mcdi_usage_get_time(cpu, state_idx);
			ratio_int = (unsigned int)div64_u64_rem(
					ratio_raw,
					ratio_dur,
					&ratio_rem);
			ratio_frac = (unsigned int)div64_u64(
					1000 * ratio_rem,
					ratio_dur);

			mcdi_log("%7u.%03u%% ",
				ratio_int,
				ratio_frac);
		}
		mcdi_log("\n");
	}

	mcdi_log("\n");
	mcdi_log("%s ratio:\n", drv->states[MCDI_STATE_CLUSTER_OFF].name);

	for (i = 0; i < NF_CLUSTER; i++) {

		ratio_raw = 0;

		for (cpu = 0; cpu < NF_CPU; cpu++) {

			if (i != cluster_idx_get(cpu))
				continue;

			ratio_raw += 100 * mcdi_usage_get_time(
						cpu, MCDI_STATE_CLUSTER_OFF);
		}

		ratio_int = (unsigned int)div64_u64_rem(ratio_raw,
				ratio_dur, &ratio_rem);
		ratio_frac = (unsigned int)div64_u64(
				1000 * ratio_rem, ratio_dur);

		mcdi_log("\t%d: %3u.%03u%% (%llu)\n",
			i, ratio_int,
			ratio_frac, div64_u64(ratio_raw, 100));
	}

	state_idx = MCDI_STATE_CLUSTER_OFF + 1;

	for ( ; state_idx < drv->state_count; state_idx++) {

		mcdi_log("\n");
		mcdi_log("%s ratio:\n", drv->states[state_idx].name);

		ratio_raw = 0;

		for (cpu = 0; cpu < NF_CPU; cpu++) {
			ratio_raw += 100 *
				mcdi_usage_get_time(cpu, state_idx);
		}

		ratio_int = (unsigned int)div64_u64_rem(ratio_raw,
				ratio_dur, &ratio_rem);
		ratio_frac = (unsigned int)div64_u64(
				1000 * ratio_rem, ratio_dur);

		mcdi_log("\t%3u.%03u%% (%llu)\n",
			ratio_int, ratio_frac, div64_u64(ratio_raw, 100));
	}
	mcdi_log("\n");

finish:
	len = p - dbg_buf;

	return simple_read_from_buffer(userbuf, count, f_pos, dbg_buf, len);
}

static ssize_t mcdi_usage_write(struct file *filp,
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

	if (!strncmp(cmd_str, "enable", sizeof("enable")))
		mcdi_usage_enable(param);
	else if (!strncmp(cmd_str, "set_cpu", sizeof("set_cpu")))
		mcdi_usage_cpu_mask(param);
	else if (!strncmp(cmd_str, "set_cpu_b", sizeof("set_cpu_b")))
		mcdi_usage_cpu_mask((mcdi_usage.cpu_valid | (1 << param)));
	else if (!strncmp(cmd_str, "clr_cpu_b", sizeof("clr_cpu_b")))
		mcdi_usage_cpu_mask((mcdi_usage.cpu_valid & ~(1 << param)));
	else
		return -EINVAL;

	return count;
}

PROC_FOPS_MCDI(profile);
PROC_FOPS_MCDI(usage);

void mcdi_procfs_profile_init(struct proc_dir_entry *mcdi_dir)
{
	PROC_CREATE_MCDI(mcdi_dir, profile);
	PROC_CREATE_MCDI(mcdi_dir, usage);
}

void mcdi_prof_init(void)
{
	struct mcdi_prof_lat_data *data;
	int i;

	for (i = 0; i < (NF_MCDI_PROFILE); i++) {

		data = &mcdi_lat.section[i];
		data->valid = false;

		if ((data->start == data->end) || !data->name)
			continue;

		if (data->start >= 0 && data->start < NF_MCDI_PROFILE
			&& data->end >= 0 && data->end < NF_MCDI_PROFILE) {

			data->valid = true;
			data->start_ts = mcdi_lat.section[data->start].ts;
			data->end_ts = mcdi_lat.section[data->end].ts;
		}
	}

	for (i = 0; i < NF_CLUSTER; i++)
		mcdi_usage.last_id[i] = -1;

	for (i = 0; i < NF_CPU; i++)
		mcdi_usage.dev[i].cpu = i;

	mcdi_usage.cpu_valid = (1 << NF_CPU) - 1;
}
