/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <asm/uaccess.h>

#include <mach/msm_iomap.h>
#include "rpm_stats.h"
#define MSG_RAM_SIZE_PER_MASTER	32

enum {
	NUMSHUTDOWNS,
	ACTIVECORES,
	MASTER_ID_MAX,
};

static char *msm_rpm_master_stats_id_labels[MASTER_ID_MAX] = {
	[NUMSHUTDOWNS] = "num_shutdowns",
	[ACTIVECORES] = "active_cores",
};


struct msm_rpm_master_stats {
	unsigned long numshutdowns;
	unsigned long active_cores;
};

struct msm_rpm_master_stats_private_data {
	void __iomem *reg_base;
	u32 len;
	char **master_names;
	u32 nomasters;
	char buf[256];
	struct msm_rpm_master_stats_platform_data *platform_data;
};

static int msm_rpm_master_stats_file_close(struct inode *inode,
		struct file *file)
{
	struct msm_rpm_master_stats_private_data *private = file->private_data;

	if (private->reg_base)
		iounmap(private->reg_base);
	kfree(file->private_data);

	return 0;
}

static int msm_rpm_master_copy_stats(
		struct msm_rpm_master_stats_private_data *pdata)
{
	struct msm_rpm_master_stats record;
	static int nomasters;
	int count;
	static DEFINE_MUTEX(msm_rpm_master_stats_mutex);
	int j = 0;

	mutex_lock(&msm_rpm_master_stats_mutex);
	/*
	 * iterrate possible nomasters times.
	 * 8960, 8064 have 5 masters.
	 * 8930 has 4 masters.
	 * 9x15 has 3 masters.
	 */
	if (nomasters > pdata->nomasters - 1) {
		nomasters = 0;
		mutex_unlock(&msm_rpm_master_stats_mutex);
		return 0;
	}

	record.numshutdowns = readl_relaxed(pdata->reg_base +
			(nomasters * MSG_RAM_SIZE_PER_MASTER));
	record.active_cores = readl_relaxed(pdata->reg_base +
				(nomasters * MSG_RAM_SIZE_PER_MASTER + 4));

	count = snprintf(pdata->buf, sizeof(pdata->buf),
		"%s\n\t%s:%lu\n\t%s:%lu\n",
		pdata->master_names[nomasters],
		msm_rpm_master_stats_id_labels[0],
		record.numshutdowns,
		msm_rpm_master_stats_id_labels[1],
		record.active_cores);

	j = find_first_bit(&record.active_cores, BITS_PER_LONG);
	while (j < BITS_PER_LONG) {
		count += snprintf(pdata->buf + count,
			sizeof(pdata->buf) - count,
			"\t\tcore%d\n", j);
		j = find_next_bit(&record.active_cores,
				BITS_PER_LONG, j + 1);
	}


	nomasters++;
	mutex_unlock(&msm_rpm_master_stats_mutex);
	return count;
}

static int msm_rpm_master_stats_file_read(struct file *file, char __user *bufu,
				  size_t count, loff_t *ppos)
{
	struct msm_rpm_master_stats_private_data *prvdata;
	struct msm_rpm_master_stats_platform_data *pdata;

	prvdata = file->private_data;
	if (!prvdata)
		return -EINVAL;

	pdata = prvdata->platform_data;
	if (!pdata)
		return -EINVAL;

	if (!bufu || count == 0)
		return -EINVAL;

	if ((*ppos <= pdata->phys_size)) {
		prvdata->len = msm_rpm_master_copy_stats(prvdata);
		*ppos = 0;
	}

	return simple_read_from_buffer(bufu, count, ppos,
			prvdata->buf, prvdata->len);
}

static int msm_rpm_master_stats_file_open(struct inode *inode,
		struct file *file)
{
	struct msm_rpm_master_stats_private_data *prvdata;
	struct msm_rpm_master_stats_platform_data *pdata;

	pdata = inode->i_private;

	file->private_data =
		kmalloc(sizeof(struct msm_rpm_master_stats_private_data),
			GFP_KERNEL);

	if (!file->private_data)
		return -ENOMEM;
	prvdata = file->private_data;

	prvdata->reg_base = ioremap(pdata->phys_addr_base,
		pdata->phys_size);
	if (!prvdata->reg_base) {
		kfree(file->private_data);
		prvdata = NULL;
		pr_err("%s: ERROR could not ioremap start=%p, len=%u\n",
			__func__, (void *)pdata->phys_addr_base,
			pdata->phys_size);
		return -EBUSY;
	}

	prvdata->len = 0;
	prvdata->nomasters = pdata->nomasters;
	prvdata->master_names = pdata->masters;
	prvdata->platform_data = pdata;
	return 0;
}

static const struct file_operations msm_rpm_master_stats_fops = {
	.owner	  = THIS_MODULE,
	.open	  = msm_rpm_master_stats_file_open,
	.read	  = msm_rpm_master_stats_file_read,
	.release  = msm_rpm_master_stats_file_close,
	.llseek   = no_llseek,
};

static  int __devinit msm_rpm_master_stats_probe(struct platform_device *pdev)
{
	struct dentry *dent;
	struct msm_rpm_master_stats_platform_data *pdata;
	struct resource *res;

	pdata = pdev->dev.platform_data;
	if (!pdata)
		return -EINVAL;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	pdata->phys_addr_base = res->start;
	pdata->phys_size = resource_size(res);

	dent = debugfs_create_file("rpm_master_stats", S_IRUGO, NULL,
			pdev->dev.platform_data, &msm_rpm_master_stats_fops);

	if (!dent) {
		pr_err("%s: ERROR debugfs_create_file failed\n", __func__);
		return -ENOMEM;
	}
	platform_set_drvdata(pdev, dent);
	return 0;
}

static int __devexit msm_rpm_master_stats_remove(struct platform_device *pdev)
{
	struct dentry *dent;

	dent = platform_get_drvdata(pdev);
	debugfs_remove(dent);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static struct platform_driver msm_rpm_master_stats_driver = {
	.probe	= msm_rpm_master_stats_probe,
	.remove = __devexit_p(msm_rpm_master_stats_remove),
	.driver = {
		.name = "msm_rpm_master_stat",
		.owner = THIS_MODULE,
	},
};

static int __init msm_rpm_master_stats_init(void)
{
	return platform_driver_register(&msm_rpm_master_stats_driver);
}

static void __exit msm_rpm_master_stats_exit(void)
{
	platform_driver_unregister(&msm_rpm_master_stats_driver);
}

module_init(msm_rpm_master_stats_init);
module_exit(msm_rpm_master_stats_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MSM RPM Master Statistics driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:msm_master_stat_log");
