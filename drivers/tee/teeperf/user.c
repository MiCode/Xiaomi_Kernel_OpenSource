// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#include <linux/cdev.h>
#include <linux/cpufreq.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#include "user.h"

static void teeperf_set_cpu_to_high_freq(int target_cpu, u32 high_freq)
{
	struct cpufreq_policy *policy;
	unsigned int index, max_index, min_index;

	if (target_cpu >= 8) {
		pr_info(PFX "invalid target cpu (%d)\n", target_cpu);
		return;
	}

	policy = cpufreq_cpu_get(target_cpu);
	if (policy == NULL) {
		pr_info(PFX "invalid policy, target cpu (%d)\n", target_cpu);
		return;
	}

	down_write(&policy->rwsem);
	max_index = 0;
	min_index = cpufreq_table_find_index_dl(policy, 0);
	if (high_freq) {
		/* set min_freq to selected freq */
		index = max_index + FREQ_LEVEL_INDEX;
		if (index > min_index)
			index = min_index;
	} else {
		/* set min_freq to min freq */
		index = min_index;
	}
	policy->cpuinfo.min_freq = policy->freq_table[index].frequency;
	up_write(&policy->rwsem);
	cpufreq_cpu_put(policy);
	cpufreq_update_limits(target_cpu);
}

static int teeperf_user_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int teeperf_user_release(struct inode *inode, struct file *file)
{
	return 0;
}

static inline int teeperf_ioctl_check_pointer(unsigned int cmd, int __user *uarg)
{
	int err = 0;

	err = !access_ok(uarg, _IOC_SIZE(cmd));
	if (err)
		return -EFAULT;

	return 0;
}

static long teeperf_user_ioctl(struct file *file, unsigned int id, unsigned long arg)
{
	int __user *uarg = (int __user *)arg;
	int ret = -EINVAL;

	pr_info(PFX "%u from %s\n", _IOC_NR(id), current->comm);

	if (teeperf_ioctl_check_pointer(id, uarg))
		return -EFAULT;

	switch (id) {
	case TEEPERF_IO_HIGH_FREQ: {
		enum cpu_map_type map_type = cpu_map;
		u32 high_freq;
		int cpu;

		if (copy_from_user(&high_freq, uarg, sizeof(high_freq))) {
			ret = -EFAULT;
			break;
		}

		if (map_type == CPU_4_3_1_MAP) {
			teeperf_set_cpu_to_high_freq(4, high_freq);
			teeperf_set_cpu_to_high_freq(5, high_freq);
			teeperf_set_cpu_to_high_freq(6, high_freq);
		} else if (map_type == CPU_6_2_MAP) {
			teeperf_set_cpu_to_high_freq(6, high_freq);
			teeperf_set_cpu_to_high_freq(7, high_freq);
		} else {
			for (cpu = 0; cpu < 8; cpu++)
				teeperf_set_cpu_to_high_freq(cpu, high_freq);
		}
		ret = 0;
		break;
	}
	default:
		ret = -ENOIOCTLCMD;
		pr_info(PFX "unsupported command, id %d\n", id);
	}

	return ret;
}

static const struct file_operations teeperf_user_fops = {
	.owner = THIS_MODULE,
	.open = teeperf_user_open,
	.release = teeperf_user_release,
	.unlocked_ioctl = teeperf_user_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = teeperf_user_ioctl,
#endif
};

int teeperf_user_init(struct cdev *cdev)
{
	cdev_init(cdev, &teeperf_user_fops);
	return 0;
}
