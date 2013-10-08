/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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
#include <linux/types.h>
#include <linux/of.h>
#include <asm/uaccess.h>
#include <asm/arch_timer.h>
#include <mach/msm_iomap.h>
#include "rpm_stats.h"


enum {
	ID_COUNTER,
	ID_ACCUM_TIME_SCLK,
	ID_MAX,
};

static char *msm_rpmstats_id_labels[ID_MAX] = {
	[ID_COUNTER] = "Count",
	[ID_ACCUM_TIME_SCLK] = "Total time(uSec)",
};

#define SCLK_HZ 32768
#define MSM_ARCH_TIMER_FREQ 19200000

struct msm_rpmstats_record{
	char		name[32];
	uint32_t	id;
	uint32_t	val;
};
struct msm_rpmstats_private_data{
	void __iomem *reg_base;
	u32 num_records;
	u32 read_idx;
	u32 len;
	char buf[256];
	struct msm_rpmstats_platform_data *platform_data;
};

struct msm_rpm_stats_data_v2 {
	u32 stat_type;
	u32 count;
	u64 last_entered_at;
	u64 last_exited_at;
	u64 accumulated;
	u32 reserved[4];
};

static inline u64 get_time_in_sec(u64 counter)
{
	do_div(counter, MSM_ARCH_TIMER_FREQ);
	return counter;
}

static inline u64 get_time_in_msec(u64 counter)
{
	do_div(counter, MSM_ARCH_TIMER_FREQ);
	counter *= MSEC_PER_SEC;
	return counter;
}

static inline int msm_rpmstats_append_data_to_buf(char *buf,
		struct msm_rpm_stats_data_v2 *data, int buflength)
{
	char stat_type[5];
	u64 time_in_last_mode;
	u64 time_since_last_mode;
	u64 actual_last_sleep;

	stat_type[4] = 0;
	memcpy(stat_type, &data->stat_type, sizeof(u32));

	time_in_last_mode = data->last_exited_at - data->last_entered_at;
	time_in_last_mode = get_time_in_msec(time_in_last_mode);
	time_since_last_mode = arch_counter_get_cntpct() - data->last_exited_at;
	time_since_last_mode = get_time_in_sec(time_since_last_mode);
	actual_last_sleep = get_time_in_msec(data->accumulated);

	return  snprintf(buf , buflength,
		"RPM Mode:%s\n\t count:%d\n time in last mode(msec):%llu\n"
		"time since last mode(sec):%llu\n actual last sleep(msec):%llu\n",
		stat_type, data->count, time_in_last_mode,
		time_since_last_mode, actual_last_sleep);
}

static inline u32 msm_rpmstats_read_long_register_v2(void __iomem *regbase,
		int index, int offset)
{
	return readl_relaxed(regbase + offset +
			index * sizeof(struct msm_rpm_stats_data_v2));
}

static inline u64 msm_rpmstats_read_quad_register_v2(void __iomem *regbase,
		int index, int offset)
{
	u64 dst;
	memcpy_fromio(&dst,
		regbase + offset + index * sizeof(struct msm_rpm_stats_data_v2),
		8);
	return dst;
}

static inline int msm_rpmstats_copy_stats_v2(
			struct msm_rpmstats_private_data *prvdata)
{
	void __iomem *reg;
	struct msm_rpm_stats_data_v2 data;
	int i, length;

	reg = prvdata->reg_base;

	for (i = 0, length = 0; i < prvdata->num_records; i++) {

		data.stat_type = msm_rpmstats_read_long_register_v2(reg, i,
				offsetof(struct msm_rpm_stats_data_v2,
					stat_type));
		data.count = msm_rpmstats_read_long_register_v2(reg, i,
				offsetof(struct msm_rpm_stats_data_v2, count));
		data.last_entered_at = msm_rpmstats_read_quad_register_v2(reg,
				i, offsetof(struct msm_rpm_stats_data_v2,
					last_entered_at));
		data.last_exited_at = msm_rpmstats_read_quad_register_v2(reg,
				i, offsetof(struct msm_rpm_stats_data_v2,
					last_exited_at));

		data.accumulated = msm_rpmstats_read_quad_register_v2(reg,
				i, offsetof(struct msm_rpm_stats_data_v2,
					accumulated));
		length += msm_rpmstats_append_data_to_buf(prvdata->buf + length,
				&data, sizeof(prvdata->buf) - length);
		prvdata->read_idx++;
	}
	return length;
}

static inline unsigned long  msm_rpmstats_read_register(void __iomem *regbase,
		int index, int offset)
{
	return  readl_relaxed(regbase + index * 12 + (offset + 1) * 4);
}
static void msm_rpmstats_strcpy(char *dest, char  *src)
{
	union {
		char ch[4];
		unsigned long word;
	} string;
	int index = 0;

	do  {
		int i;
		string.word = readl_relaxed(src + 4 * index);
		for (i = 0; i < 4; i++) {
			*dest++ = string.ch[i];
			if (!string.ch[i])
				break;
		}
		index++;
	} while (*(dest-1));

}
static int msm_rpmstats_copy_stats(struct msm_rpmstats_private_data *pdata)
{

	struct msm_rpmstats_record record;
	unsigned long ptr;
	unsigned long offset;
	char *str;
	uint64_t usec;

	ptr = msm_rpmstats_read_register(pdata->reg_base, pdata->read_idx, 0);
	offset = (ptr - (unsigned long)pdata->platform_data->phys_addr_base);

	if (offset > pdata->platform_data->phys_size)
		str = (char *)ioremap(ptr, SZ_256);
	else
		str = (char *) pdata->reg_base + offset;

	msm_rpmstats_strcpy(record.name, str);

	if (offset > pdata->platform_data->phys_size)
		iounmap(str);

	record.id = msm_rpmstats_read_register(pdata->reg_base,
						pdata->read_idx, 1);
	record.val = msm_rpmstats_read_register(pdata->reg_base,
						pdata->read_idx, 2);

	if (record.id == ID_ACCUM_TIME_SCLK) {
		usec = record.val * USEC_PER_SEC;
		do_div(usec, SCLK_HZ);
	}  else
		usec = (unsigned long)record.val;

	pdata->read_idx++;

	return snprintf(pdata->buf, sizeof(pdata->buf),
			"RPM Mode:%s\n\t%s:%llu\n",
			record.name,
			msm_rpmstats_id_labels[record.id],
			usec);
}

static int msm_rpmstats_file_read(struct file *file, char __user *bufu,
				  size_t count, loff_t *ppos)
{
	struct msm_rpmstats_private_data *prvdata;
	prvdata = file->private_data;

	if (!prvdata)
		return -EINVAL;

	if (!bufu || count == 0)
		return -EINVAL;

	if (prvdata->platform_data->version == 1) {
		if (!prvdata->num_records)
			prvdata->num_records = readl_relaxed(prvdata->reg_base);
	}

	if ((*ppos >= prvdata->len)
		&& (prvdata->read_idx < prvdata->num_records)) {
			if (prvdata->platform_data->version == 1)
				prvdata->len = msm_rpmstats_copy_stats(prvdata);
			else if (prvdata->platform_data->version == 2)
				prvdata->len = msm_rpmstats_copy_stats_v2(
						prvdata);
			*ppos = 0;
	}

	return simple_read_from_buffer(bufu, count, ppos,
			prvdata->buf, prvdata->len);
}

static int msm_rpmstats_file_open(struct inode *inode, struct file *file)
{
	struct msm_rpmstats_private_data *prvdata;
	struct msm_rpmstats_platform_data *pdata;

	pdata = inode->i_private;

	file->private_data =
		kmalloc(sizeof(struct msm_rpmstats_private_data), GFP_KERNEL);

	if (!file->private_data)
		return -ENOMEM;
	prvdata = file->private_data;

	prvdata->reg_base = ioremap_nocache(pdata->phys_addr_base,
					pdata->phys_size);
	if (!prvdata->reg_base) {
		kfree(file->private_data);
		prvdata = NULL;
		pr_err("%s: ERROR could not ioremap start=%pa, len=%u\n",
			__func__, &pdata->phys_addr_base,
			pdata->phys_size);
		return -EBUSY;
	}

	prvdata->read_idx = prvdata->num_records =  prvdata->len = 0;
	prvdata->platform_data = pdata;
	if (pdata->version == 2)
		prvdata->num_records = 2;

	return 0;
}

static int msm_rpmstats_file_close(struct inode *inode, struct file *file)
{
	struct msm_rpmstats_private_data *private = file->private_data;

	if (private->reg_base)
		iounmap(private->reg_base);
	kfree(file->private_data);

	return 0;
}

static const struct file_operations msm_rpmstats_fops = {
	.owner	  = THIS_MODULE,
	.open	  = msm_rpmstats_file_open,
	.read	  = msm_rpmstats_file_read,
	.release  = msm_rpmstats_file_close,
	.llseek   = no_llseek,
};

static  int msm_rpmstats_probe(struct platform_device *pdev)
{
	struct dentry *dent = NULL;
	struct msm_rpmstats_platform_data *pdata;
	struct msm_rpmstats_platform_data *pd;
	struct resource *res = NULL;
	struct device_node *node = NULL;
	int ret = 0;

	if (!pdev)
		return -EINVAL;

	pdata = kzalloc(sizeof(struct msm_rpmstats_platform_data), GFP_KERNEL);

	if (!pdata)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	if (!res)
		return -EINVAL;

	pdata->phys_addr_base  = res->start;

	pdata->phys_size = resource_size(res);
	node = pdev->dev.of_node;
	if (pdev->dev.platform_data) {
		pd = pdev->dev.platform_data;
		pdata->version = pd->version;

	} else if (node)
		ret = of_property_read_u32(node,
			"qcom,sleep-stats-version", &pdata->version);

	if (!ret) {

		dent = debugfs_create_file("rpm_stats", S_IRUGO, NULL,
				pdata, &msm_rpmstats_fops);

		if (!dent) {
			pr_err("%s: ERROR debugfs_create_file failed\n",
					__func__);
			kfree(pdata);
			return -ENOMEM;
		}

	} else {
		kfree(pdata);
		return -EINVAL;
	}
	platform_set_drvdata(pdev, dent);
	return 0;
}

static int msm_rpmstats_remove(struct platform_device *pdev)
{
	struct dentry *dent;

	dent = platform_get_drvdata(pdev);
	debugfs_remove(dent);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static struct of_device_id rpm_stats_table[] = {
	       {.compatible = "qcom,rpm-stats"},
	       {},
};

static struct platform_driver msm_rpmstats_driver = {
	.probe	= msm_rpmstats_probe,
	.remove = msm_rpmstats_remove,
	.driver = {
		.name = "msm_rpm_stat",
		.owner = THIS_MODULE,
		.of_match_table = rpm_stats_table,
	},
};
static int __init msm_rpmstats_init(void)
{
	return platform_driver_register(&msm_rpmstats_driver);
}
static void __exit msm_rpmstats_exit(void)
{
	platform_driver_unregister(&msm_rpmstats_driver);
}
module_init(msm_rpmstats_init);
module_exit(msm_rpmstats_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MSM RPM Statistics driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:msm_stat_log");
