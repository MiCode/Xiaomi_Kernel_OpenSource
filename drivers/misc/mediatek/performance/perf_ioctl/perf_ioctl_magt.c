// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include "perf_ioctl_magt.h"
#define TAG "PERF_IOCTL_MAGT"
#define cap_scale(v, s) ((v)*(s) >> SCHED_CAPACITY_SHIFT)

struct proc_dir_entry *perfmgr_root;
static DEFINE_MUTEX(cpu_lock);

static int thermal_aware_threshold = -1;
static int fpsdrop_aware_threshold = -1;
static int advice_bat_avg_current = -1;
static int advice_bat_max_current = -1;

module_param(thermal_aware_threshold, int, 0644);
module_param(fpsdrop_aware_threshold, int, 0644);
module_param(advice_bat_avg_current, int, 0644);
module_param(advice_bat_max_current, int, 0644);

static unsigned long perfctl_copy_to_user(void __user *pvTo,
		const void *pvFrom, unsigned long ulBytes)
{
	if (access_ok(pvTo, ulBytes))
		return __copy_to_user(pvTo, pvFrom, ulBytes);
	return ulBytes;
}

/*--------------------GET CPU INFO------------------------*/
static struct cpu_time *cur_wall_time, *cur_idle_time,
						*prev_wall_time, *prev_idle_time;
static struct cpu_info ci;
static int *num_cpus;

unsigned long capacity_curr_of(int cpu)
{
	unsigned long max_cap = cpu_rq(cpu)->cpu_capacity_orig;
	unsigned long scale_freq = arch_scale_freq_capacity(cpu);

	return cap_scale(max_cap, scale_freq);
}

static int arch_get_nr_clusters(void)
{
	int __arch_nr_clusters = -1;
	int max_id = 0;
	unsigned int cpu;

	/* assume socket id is monotonic increasing without gap. */
	for_each_possible_cpu(cpu) {
		struct cpu_topology *cpu_topo = &cpu_topology[cpu];

		if (cpu_topo->package_id > max_id)
			max_id = cpu_topo->package_id;
	}
	__arch_nr_clusters = max_id + 1;
	return __arch_nr_clusters;
}

void arch_get_cluster_cpus(struct cpumask *cpus, int package_id)
{
	unsigned int cpu;

	cpumask_clear(cpus);
	for_each_possible_cpu(cpu) {
		struct cpu_topology *cpu_topo = &cpu_topology[cpu];

		if (cpu_topo->package_id == package_id)
			cpumask_set_cpu(cpu, cpus);
	}
}

int init_cpu_time(void)
{
	int i;
	int cpu_num = num_possible_cpus();

	mutex_lock(&cpu_lock);
	cur_wall_time = kcalloc(cpu_num, sizeof(struct cpu_time), GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(cur_wall_time))
		goto err_cur_wall_time;

	cur_idle_time = kcalloc(cpu_num, sizeof(struct cpu_time), GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(cur_idle_time))
		goto err_cur_idle_time;

	prev_wall_time = kcalloc(cpu_num, sizeof(struct cpu_time), GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(prev_wall_time))
		goto err_prev_wall_time;

	prev_idle_time = kcalloc(cpu_num, sizeof(struct cpu_time), GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(prev_idle_time))
		goto err_prev_idle_time;

	// _ci = kcalloc(1, sizeof(struct cpu_info), GFP_KERNEL);
	for_each_possible_cpu(i) {
		prev_wall_time[i].time = cur_wall_time[i].time = 0;
		prev_idle_time[i].time = cur_idle_time[i].time = 0;
	}
	mutex_unlock(&cpu_lock);
	return 0;

err_prev_idle_time:
	kfree(prev_wall_time);
err_prev_wall_time:
	kfree(cur_idle_time);
err_cur_idle_time:
	kfree(cur_wall_time);
err_cur_wall_time:
	pr_debug(TAG "%s failed to alloc cpu time", __func__);
	mutex_unlock(&cpu_lock);
	return -ENOMEM;
}

int init_num_cpus(void)
{
	struct cpumask cluster_cpus;
	int i, cluster_nr;

	cluster_nr = arch_get_nr_clusters();
	// Get first cpu id of clusters.
	mutex_lock(&cpu_lock);
	num_cpus = kcalloc(cluster_nr + 1, sizeof(int), GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(num_cpus)) {
		pr_debug(TAG "%s failed to alloc num cpus", __func__);
		mutex_unlock(&cpu_lock);
		return -ENOMEM;
	}
	num_cpus[0] = 0;
	for (i = 0; i < cluster_nr; i++) {
		arch_get_cluster_cpus(&cluster_cpus, i);
		num_cpus[i + 1] = num_cpus[i] + cpumask_weight(&cluster_cpus);
		// pr_info("perf_index num_cpus = %d\n", num_cpus[i]);
	}
	mutex_unlock(&cpu_lock);
	return 0;
}

int get_cpu_loading(struct cpu_info *_ci)
{
	int i, cpu_loading = 0;
	u64 wall_time = 0, idle_time = 0;

	mutex_lock(&cpu_lock);
	if (ZERO_OR_NULL_PTR(cur_wall_time)) {
		mutex_unlock(&cpu_lock);
		return -ENOMEM;
	}

	for (i = 0; i < max_cpus; i++)
		_ci->cpu_loading[i] = 0;

	for_each_possible_cpu(i) {

		if (i >= max_cpus)
			break;

		cpu_loading = 0;
		wall_time = 0;
		idle_time = 0;
		prev_wall_time[i].time = cur_wall_time[i].time;
		prev_idle_time[i].time = cur_idle_time[i].time;
		/*idle time include iowait time*/
		cur_idle_time[i].time = get_cpu_idle_time(i,
				&cur_wall_time[i].time, 1);
		if (cpu_active(i)) {
			wall_time = cur_wall_time[i].time - prev_wall_time[i].time;
			idle_time = cur_idle_time[i].time - prev_idle_time[i].time;
		}
		if (wall_time > 0 && wall_time > idle_time)
			cpu_loading = div_u64((100 * (wall_time - idle_time)),
			wall_time);
		_ci->cpu_loading[i] = cpu_loading;
		// pr_info("CPU %d loading is %d%%\n", i, _ci->cpu_loading[i]);
	}
	mutex_unlock(&cpu_lock);
	return 0;
}
EXPORT_SYMBOL(get_cpu_loading);

int get_perf_index(struct cpu_info *_ci)
{
	int i, cluster_nr, perf_index = 0, cluster_idx = 0;

	cluster_nr = arch_get_nr_clusters();

	mutex_lock(&cpu_lock);
	if (ZERO_OR_NULL_PTR(num_cpus)) {
		mutex_unlock(&cpu_lock);
		return -ENOMEM;
	}

	for (i = 0; i < cluster_nr; i++)
		_ci->perf_index[i] = 0;
	for_each_possible_cpu(i) {
		if (i == num_cpus[cluster_idx]) {
			perf_index = div_u64(100 * capacity_curr_of(i), 1024);
			_ci->perf_index[cluster_idx] = perf_index;
			// pr_info("cluster %d perf_index = %d\n",
			// cluster_idx, _ci->perf_index[cluster_idx]);
			cluster_idx++;
		}
	}
	mutex_unlock(&cpu_lock);
	return 0;
}
EXPORT_SYMBOL(get_perf_index);

/*--------------------MAGT IOCTL------------------------*/
static long magt_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	ssize_t ret = 0;
	struct cpu_info *ciUM = (struct cpu_info *)arg, *ciKM = &ci;

	switch (cmd) {
	case GET_CPU_LOADING:
		ret = get_cpu_loading(ciKM);
		if (ret < 0)
			goto ret_ioctl;
		perfctl_copy_to_user(ciUM, ciKM,
			sizeof(struct cpu_info));
		break;
	case GET_PERF_INDEX:
		ret = get_perf_index(ciKM);
		if (ret < 0)
			goto ret_ioctl;
		perfctl_copy_to_user(ciUM, ciKM,
			sizeof(struct cpu_info));
		break;
	default:
		pr_debug(TAG "%s %d: unknown cmd %x\n",
			__FILE__, __LINE__, cmd);
		ret =  -EINVAL;
		goto ret_ioctl;
	}
ret_ioctl:
	return ret;
}

static int magt_show(struct seq_file *m, void *v)
{
	return 0;
}
static int magt_open(struct inode *inode, struct file *file)
{
	return single_open(file, magt_show, inode->i_private);
}
static const struct proc_ops Fops = {
	.proc_compat_ioctl = magt_ioctl,
	.proc_ioctl = magt_ioctl,
	.proc_open = magt_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

/*--------------------INIT------------------------*/
static void __exit exit_magt_perfctl(void)
{
	mutex_lock(&cpu_lock);
	kfree(cur_wall_time);
	kfree(cur_idle_time);
	kfree(prev_wall_time);
	kfree(prev_idle_time);
	kfree(num_cpus);
	mutex_unlock(&cpu_lock);
}

static int __init init_magt_perfctl(void)
{
	struct proc_dir_entry *pe, *parent;
	int ret_val = 0;

	pr_debug(TAG"Start to init MAGT perf_ioctl driver\n");
	parent = proc_mkdir("perfmgr_magt", NULL);
	if (!parent) {
		ret_val = -ENOMEM;
		pr_debug(TAG"%s failed with %d\n",
				"Creating dir ",
				ret_val);
		goto out_wq;
	}
	perfmgr_root = parent;
	pe = proc_create("magt_ioctl", 0660, parent, &Fops);
	if (!pe) {
		ret_val = -ENOMEM;
		pr_debug(TAG"%s failed with %d\n",
				"Creating file node ",
				ret_val);
		goto out_wq;
	}
	pr_debug(TAG"init magt_ioctl driver done\n");

	ret_val |= init_cpu_time();
	ret_val |= init_num_cpus();

	if (ret_val < 0)
		goto out_wq;

	return 0;
out_wq:
	return ret_val;
}
module_init(init_magt_perfctl);
module_exit(exit_magt_perfctl);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek MAGT ioctl");
MODULE_AUTHOR("MediaTek Inc.");

