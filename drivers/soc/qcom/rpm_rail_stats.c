/* Copyright (c) 2015, 2017, The Linux Foundation. All rights reserved.
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
#include <linux/of.h>
#include <linux/uaccess.h>

#include "rpm_stats.h"

#define RPM_RAIL_BUF_LEN 600

#define SNPRINTF(buf, size, format, ...) \
	do { \
		if (size > 0) { \
			int ret; \
			ret = snprintf(buf, size, format, ## __VA_ARGS__); \
			if (ret > size) { \
				buf += size; \
				size = 0; \
			} else { \
				buf += ret; \
				size -= ret; \
			} \
		} \
	} while (0)

#define NAMELEN (sizeof(uint32_t)+1)

static DEFINE_MUTEX(msm_rpm_rail_stats_mutex);

struct msm_rpm_rail_stats_platform_data {
	phys_addr_t phys_addr_base;
	u32 phys_size;
};

struct msm_rpm_rail_corner {
	uint64_t time;
	uint32_t corner;
	uint32_t reserved;
};

struct msm_rpm_rail_type {
	uint32_t rail;
	uint32_t num_corners;
	uint32_t current_corner;
	uint32_t last_entered;
};

struct msm_rpm_rail_stats {
	uint32_t num_rails;
	uint32_t reserved;
};

struct msm_rpm_rail_stats_private_data {
	void __iomem *reg_base;
	u32 len;
	char buf[RPM_RAIL_BUF_LEN];
	struct msm_rpm_rail_stats_platform_data *platform_data;
};

int msm_rpm_rail_stats_file_close(struct inode *inode, struct file *file)
{
	struct msm_rpm_rail_stats_private_data *private = file->private_data;

	mutex_lock(&msm_rpm_rail_stats_mutex);
	if (private->reg_base)
		iounmap(private->reg_base);
	kfree(file->private_data);
	mutex_unlock(&msm_rpm_rail_stats_mutex);

	return 0;
}

static int msm_rpm_rail_corner_copy(void __iomem **base, char **buf,
					int count)
{
	struct msm_rpm_rail_corner rc;
	char corner[NAMELEN];

	memset(&rc, 0, sizeof(rc));
	memcpy_fromio(&rc, *base, sizeof(rc));

	corner[NAMELEN - 1] = '\0';
	memcpy(corner, &rc.corner, NAMELEN - 1);
	SNPRINTF(*buf, count, "\t\tcorner:%-5s time:%-16llu\n",
		corner, rc.time);

	*base += sizeof(rc);

	return count;
}

static int msm_rpm_rail_type_copy(void __iomem **base, char **buf, int count)
{
	struct msm_rpm_rail_type rt;
	char rail[NAMELEN];
	int i;

	memset(&rt, 0, sizeof(rt));
	memcpy_fromio(&rt, *base, sizeof(rt));

	rail[NAMELEN - 1] = '\0';
	memcpy(rail, &rt.rail, NAMELEN - 1);
	SNPRINTF(*buf, count,
		"\trail:%-2s num_corners:%-2u current_corner:%-2u last_entered:%-8u\n",
		rail, rt.num_corners, rt.current_corner, rt.last_entered);

	*base += sizeof(rt);

	for (i = 0; i < rt.num_corners; i++)
		count = msm_rpm_rail_corner_copy(base, buf, count);

	return count;
}

static int msm_rpm_rail_stats_copy(
		struct msm_rpm_rail_stats_private_data *prvdata)
{
	struct msm_rpm_rail_stats rs;
	void __iomem *base = prvdata->reg_base;
	char *buf = prvdata->buf;
	int count = RPM_RAIL_BUF_LEN;
	int i;

	memset(&rs, 0, sizeof(rs));
	memcpy_fromio(&rs, base, sizeof(rs));

	SNPRINTF(buf, count, "Number of Rails:%u\n", rs.num_rails);

	base = prvdata->reg_base + sizeof(rs);

	for (i = 0; i < rs.num_rails; i++)
		count = msm_rpm_rail_type_copy(&base, &buf, count);

	return RPM_RAIL_BUF_LEN - count;
}

static ssize_t msm_rpm_rail_stats_file_read(struct file *file,
				char __user *bufu, size_t count, loff_t *ppos)
{
	struct msm_rpm_rail_stats_private_data *prvdata;
	struct msm_rpm_rail_stats_platform_data *pdata;
	ssize_t ret;

	mutex_lock(&msm_rpm_rail_stats_mutex);
	prvdata = file->private_data;
	if (!prvdata) {
		ret = -EINVAL;
		goto exit;
	}

	if (!prvdata->platform_data) {
		ret = -EINVAL;
		goto exit;
	}

	if (!bufu || count == 0) {
		ret = -EINVAL;
		goto exit;
	}

	pdata = prvdata->platform_data;

	if (*ppos <= pdata->phys_size) {
		prvdata->len = msm_rpm_rail_stats_copy(prvdata);
		*ppos = 0;
	}

	ret = simple_read_from_buffer(bufu, count, ppos,
			prvdata->buf, prvdata->len);
exit:
	mutex_unlock(&msm_rpm_rail_stats_mutex);
	return ret;
}

static int msm_rpm_rail_stats_file_open(struct inode *inode,
		struct file *file)
{
	struct msm_rpm_rail_stats_private_data *prvdata;
	struct msm_rpm_rail_stats_platform_data *pdata;
	int ret = 0;

	mutex_lock(&msm_rpm_rail_stats_mutex);
	pdata = inode->i_private;

	file->private_data =
		kzalloc(sizeof(struct msm_rpm_rail_stats_private_data),
			GFP_KERNEL);

	if (!file->private_data) {
		ret = -ENOMEM;
		goto exit;
	}

	prvdata = file->private_data;

	prvdata->reg_base = ioremap(pdata->phys_addr_base,
						pdata->phys_size);
	if (!prvdata->reg_base) {
		kfree(file->private_data);
		prvdata = NULL;
		pr_err("%s: ERROR could not ioremap start=%pa, len=%u\n",
			__func__, &pdata->phys_addr_base,
			pdata->phys_size);
		ret = -EBUSY;
		goto exit;
	}

	prvdata->len = 0;
	prvdata->platform_data = pdata;
exit:
	mutex_unlock(&msm_rpm_rail_stats_mutex);
	return ret;
}


static const struct file_operations msm_rpm_rail_stats_fops = {
	.owner	  = THIS_MODULE,
	.open	  = msm_rpm_rail_stats_file_open,
	.read	  = msm_rpm_rail_stats_file_read,
	.release  = msm_rpm_rail_stats_file_close,
	.llseek   = no_llseek,
};

static int msm_rpm_rail_stats_probe(struct platform_device *pdev)
{
	struct dentry *dent;
	struct msm_rpm_rail_stats_platform_data *pdata;
	struct resource *res;
	struct resource *offset;
	struct device_node *node;
	uint32_t offset_addr;
	void __iomem *phys_ptr;

	if (!pdev)
		return -EINVAL;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"phys_addr_base");
	if (!res)
		return -EINVAL;

	offset = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"offset_addr");
	if (!offset)
		return -EINVAL;

	phys_ptr = ioremap_nocache(offset->start, SZ_4);
	if (!phys_ptr) {
		pr_err("%s: Failed to ioremap address: %x\n",
				__func__, offset_addr);
		return -ENODEV;
	}
	offset_addr = readl_relaxed(phys_ptr);
	iounmap(phys_ptr);

	if (!offset_addr) {
		pr_err("%s: RPM Rail Stats not available: Exit\n", __func__);
		return 0;
	}

	node = pdev->dev.of_node;

	if (pdev->dev.platform_data)
		pdata = pdev->dev.platform_data;
	else if (node)
		pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);

	if (!pdata)
		return -ENOMEM;

	pdata->phys_addr_base = res->start + offset_addr;
	pdata->phys_size = resource_size(res);

	dent = debugfs_create_file("rpm_rail_stats", S_IRUGO, NULL,
					pdata, &msm_rpm_rail_stats_fops);

	if (!dent) {
		dev_err(&pdev->dev, "%s: ERROR debugfs_create_file failed\n",
								__func__);
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, dent);
	return 0;
}

static int msm_rpm_rail_stats_remove(struct platform_device *pdev)
{
	struct dentry *dent = platform_get_drvdata(pdev);

	debugfs_remove(dent);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static struct of_device_id rpm_rail_table[] = {
	{.compatible = "qcom,rpm-rail-stats"},
	{},
};

static struct platform_driver msm_rpm_rail_stats_driver = {
	.probe	= msm_rpm_rail_stats_probe,
	.remove = msm_rpm_rail_stats_remove,
	.driver = {
		.name = "msm_rpm_rail_stats",
		.owner = THIS_MODULE,
		.of_match_table = rpm_rail_table,
	},
};

static int __init msm_rpm_rail_stats_init(void)
{
	return platform_driver_register(&msm_rpm_rail_stats_driver);
}

static void __exit msm_rpm_rail_stats_exit(void)
{
	platform_driver_unregister(&msm_rpm_rail_stats_driver);
}

module_init(msm_rpm_rail_stats_init);
module_exit(msm_rpm_rail_stats_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MSM RPM rail Statistics driver");
MODULE_ALIAS("platform:msm_rail_stat_log");
