// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2021 Unisoc, Inc.
#include "sprd-cpufreq-v2.h"

#define MAX_SIZE		(512)

struct debug_node {
	const char *name;
	const struct file_operations *fops;
	umode_t mode;
};

struct chip_des {
	struct device *dev;
	struct dentry *dir;
};

struct cluster_den {
	struct dentry *dir;
	struct dentry **nodes;
};

#if defined(CONFIG_DEBUG_FS)
static struct chip_des chip;
static struct cluster_den *pcluster_dens;

static size_t table_cat(struct cpufreq_policy *policy, char *buf, size_t len)
{
	struct cluster_info *cluster_info = policy->driver_data;
	unsigned long rate, volt, freq;
	struct dev_pm_opp *dev_opp;
	struct device *cpu_dev;
	int opp_num, size, i;

	mutex_lock(&cluster_info->mutex);

	cpu_dev = get_cpu_device(cluster_info->cpu);
	if (!cpu_dev) {
		dev_err(chip.dev, "%s: get cpu device failed\n", __func__);
		goto out_error;
	}

	opp_num = dev_pm_opp_get_opp_count(cpu_dev);
	if (opp_num < 0) {
		dev_err(chip.dev, "%s: get opp entry num failed(%d)\n", __func__, opp_num);
		goto out_error;
	}

	size = scnprintf(buf, len, "     DVFS Table(%d)\nFreq(Hz)\tVolt(uV)\n", cluster_info->temp_level_node->temp);
	if (size >= len) {
		dev_err(chip.dev, "%s: buf len is error\n", __func__);
		goto out_error;
	}

	for (i = 0, rate = 0; i < opp_num; i++, rate++) {
		dev_opp = dev_pm_opp_find_freq_ceil(cpu_dev, &rate);
		if (IS_ERR(dev_opp)) {
			dev_err(chip.dev, "%s: get dev opp error\n", __func__);
			goto out_error;
		}

		freq = dev_pm_opp_get_freq(dev_opp);		/* in Hz */
		volt = dev_pm_opp_get_voltage(dev_opp);		/* in uV */

		dev_pm_opp_put(dev_opp);

		size += scnprintf(buf + size, len - size, "%lu\t%lu\n", freq, volt);
		if (size >= len) {
			dev_err(chip.dev, "%s: buf len is error\n", __func__);
			goto out_error;
		}
	}

	mutex_unlock(&cluster_info->mutex);

	return size;

out_error:
	mutex_unlock(&cluster_info->mutex);

	return 0;
}

static ssize_t sprd_debug_table_read(struct file *file, char __user *buf, size_t len, loff_t *ppos)
{
	struct cpufreq_policy *policy = file->private_data;
	char *str;
	size_t size;
	ssize_t ret = 0;

	str = kcalloc(MAX_SIZE, sizeof(char), GFP_KERNEL);
	if (!str)
		return -ENOMEM;

	size = table_cat(policy, str, MAX_SIZE);
	if (size)
		ret = simple_read_from_buffer(buf, len, ppos, str, size);

	kfree(str);

	return ret;
}

static int sprd_debug_table_release(struct inode *inode, struct file *file)
{
	return 0;
}

static int sprd_debug_table_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;

	return nonseekable_open(inode, file);
}

static const struct file_operations sprd_debug_table_fops = {
	.owner	 = THIS_MODULE,
	.open	 = sprd_debug_table_open,
	.release = sprd_debug_table_release,
	.read	 = sprd_debug_table_read,
	.llseek	 = generic_file_llseek,
};

static const struct debug_node debug_nodes[] = {
	{
		.name = "dvfs_table",
		.fops = &sprd_debug_table_fops,
		.mode = 0444,
	}
};

int sprd_debug_cluster_init(struct cpufreq_policy *policy)
{
	struct cluster_info *cluster_info;
	struct cluster_den *cluster_den;
	const struct debug_node *pn;
	struct dentry **ppe;
	int cluster_num, i;

	if (!chip.dev || !policy)
		return -EINVAL;

	cluster_info = (struct cluster_info *)policy->driver_data;
	if (!cluster_info) {
		dev_err(chip.dev, "%s: get cluster info from policy error\n", __func__);
		return -EINVAL;
	}

	cluster_den = pcluster_dens + cluster_info->id;
	cluster_num = ARRAY_SIZE(debug_nodes);

	cluster_den->nodes = devm_kzalloc(chip.dev, sizeof(struct dentry *) * cluster_num, GFP_KERNEL);
	if (!cluster_den->nodes) {
		dev_err(chip.dev, "%s: alloc mem for cluster %u nodes error\n", __func__, cluster_info->id);
		return -ENOMEM;
	}

	for (i = 0; i < cluster_num; ++i) {
		ppe = cluster_den->nodes + i;
		pn = debug_nodes + i;

		*ppe = debugfs_create_file(pn->name, pn->mode, cluster_den->dir, policy, pn->fops);
		if (!*ppe) {
			dev_err(chip.dev, "%s: create cluster %u %s node error\n", __func__, cluster_info->id, pn->name);
			return -EINVAL;
		}
	}

	return 0;
}

int sprd_debug_cluster_exit(struct cpufreq_policy *policy)
{
	struct cluster_info *cluster_info;
	struct cluster_den *cluster_den;
	struct dentry **ppe;
	int cluster_num, i;

	if (!chip.dev || !policy)
		return -EINVAL;

	cluster_info = (struct cluster_info *)policy->driver_data;
	if (!cluster_info) {
		dev_err(chip.dev, "%s: get cluster from policy error\n", __func__);
		return -EINVAL;
	}

	cluster_den = pcluster_dens + cluster_info->id;
	cluster_num = ARRAY_SIZE(debug_nodes);

	for (i = 0; i < cluster_num; ++i) {
		ppe = cluster_den->nodes + i;

		debugfs_remove(*ppe);
		*ppe = NULL;
	}

	devm_kfree(chip.dev, cluster_den->nodes);
	cluster_den->nodes = NULL;

	return 0;
}

int sprd_debug_init(struct device *dev)
{
	struct cluster_den *cluster_den;
	int cluster_num, i;
	char name[32];

	if (chip.dev || !dev)
		return -EINVAL;

	/* dev */
	chip.dev = dev;

	/* chip */
	chip.dir = debugfs_create_dir("sprd-cpufreq-v2", NULL);
	if (!chip.dir) {
		dev_err(dev, "%s: create chip dir error\n", __func__);
		return -EINVAL;
	}

	/* cluster */
	cluster_num = sprd_cluster_num();

	pcluster_dens = devm_kzalloc(dev, sizeof(struct cluster_den) * cluster_num, GFP_KERNEL);
	if (!pcluster_dens) {
		dev_err(dev, "%s: alloc mem for cluster error\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < cluster_num; ++i) {
		cluster_den = pcluster_dens + i;

		sprintf(name, "cluster%d", i);

		cluster_den->dir = debugfs_create_dir(name, chip.dir);
		if (!cluster_den->dir) {
			dev_err(dev, "%s: create cluster %d dir error\n", __func__, i);
			return -EINVAL;
		}
	}

	return 0;
}

int sprd_debug_exit(void)
{
	struct cluster_den *cluster_den;
	int cluster_num, i;

	if (!chip.dev)
		return -EINVAL;

	/* cluster */
	cluster_num = sprd_cluster_num();

	for (i = 0; i < cluster_num; ++i) {
		cluster_den = pcluster_dens + i;
		debugfs_remove(cluster_den->dir);
		cluster_den->dir = NULL;
	}

	devm_kfree(chip.dev, pcluster_dens);
	pcluster_dens = NULL;

	/* chip */
	debugfs_remove(chip.dir);
	chip.dir = NULL;

	/* dev */
	chip.dev = NULL;

	return 0;
}
#else
int sprd_debug_cluster_init(struct cpufreq_policy *policy)
{
	return 0;
}
int sprd_debug_cluster_exit(struct cpufreq_policy *policy)
{
	return 0;
}
int sprd_debug_init(struct device *pdev)
{
	return 0;
}
int sprd_debug_exit(void)
{
	return 0;
}
#endif /* CONFIG_DEBUG_FS */

MODULE_LICENSE("GPL v2");
