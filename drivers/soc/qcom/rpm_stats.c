/* Copyright (c) 2011-2015, 2017, The Linux Foundation. All rights reserved.
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
#include <linux/uaccess.h>
#include <asm/arch_timer.h>
#include "rpm_stats.h"

#define GET_PDATA_OF_ATTR(attr) \
	(container_of(attr, struct msm_rpmstats_kobj_attr, ka)->pd)

static DEFINE_MUTEX(rpm_stats_mutex);

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

struct msm_rpmstats_record {
	char		name[32];
	uint32_t	id;
	uint32_t	val;
};

struct msm_rpmstats_private_data {
	void __iomem *reg_base;
	u32 num_records;
	u32 read_idx;
	u32 len;
	char buf[320];
	struct msm_rpmstats_platform_data *platform_data;
};

struct msm_rpm_stats_data_v2 {
	u32 stat_type;
	u32 count;
	u64 last_entered_at;
	u64 last_exited_at;
	u64 accumulated;
	u32 client_votes;
	u32 reserved[3];
};

struct msm_rpmstats_kobj_attr {
	struct kobj_attribute ka;
	struct msm_rpmstats_platform_data *pd;
};

static struct dentry *heap_dent;

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
		"RPM Mode:%s\n\t count:%d\ntime in last mode(msec):%llu\n"
		"time since last mode(sec):%llu\nactual last sleep(msec):%llu\n"
		"client votes: %#010x\n\n",
		stat_type, data->count, time_in_last_mode,
		time_since_last_mode, actual_last_sleep,
		data->client_votes);
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
		data.client_votes = msm_rpmstats_read_long_register_v2(reg,
				i, offsetof(struct msm_rpm_stats_data_v2,
					client_votes));
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
	if (record.id >= ID_MAX) {
		pr_err("%s: array out of bound error found.\n",
			__func__);
		return -EINVAL;
	}

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

static ssize_t msm_rpmstats_file_read(struct file *file, char __user *bufu,
				  size_t count, loff_t *ppos)
{
	struct msm_rpmstats_private_data *prvdata;
	ssize_t ret;

	mutex_lock(&rpm_stats_mutex);
	prvdata = file->private_data;

	if (!prvdata) {
		ret = -EINVAL;
		goto exit;
	}

	if (!bufu || count == 0) {
		ret = -EINVAL;
		goto exit;
	}

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

	ret = simple_read_from_buffer(bufu, count, ppos,
			prvdata->buf, prvdata->len);
exit:
	mutex_unlock(&rpm_stats_mutex);
	return ret;
}

static int msm_rpmstats_file_open(struct inode *inode, struct file *file)
{
	struct msm_rpmstats_private_data *prvdata;
	struct msm_rpmstats_platform_data *pdata;
	int ret = 0;

	mutex_lock(&rpm_stats_mutex);
	pdata = inode->i_private;

	file->private_data =
		kmalloc(sizeof(struct msm_rpmstats_private_data), GFP_KERNEL);

	if (!file->private_data) {
		ret = -ENOMEM;
		goto exit;
	}

	prvdata = file->private_data;

	prvdata->reg_base = ioremap_nocache(pdata->phys_addr_base,
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

	prvdata->read_idx = prvdata->num_records =  prvdata->len = 0;
	prvdata->platform_data = pdata;
	if (pdata->version == 2)
		prvdata->num_records = 2;
exit:
	mutex_unlock(&rpm_stats_mutex);
	return ret;
}

static int msm_rpmstats_file_close(struct inode *inode, struct file *file)
{
	struct msm_rpmstats_private_data *private = file->private_data;

	mutex_lock(&rpm_stats_mutex);
	if (private->reg_base)
		iounmap(private->reg_base);
	kfree(file->private_data);
	mutex_unlock(&rpm_stats_mutex);

	return 0;
}

static const struct file_operations msm_rpmstats_fops = {
	.owner	  = THIS_MODULE,
	.open	  = msm_rpmstats_file_open,
	.read	  = msm_rpmstats_file_read,
	.release  = msm_rpmstats_file_close,
	.llseek   = no_llseek,
};

static int msm_rpmheap_file_show(struct seq_file *m, void *v)
{
	struct msm_rpmstats_platform_data *pdata;
	void __iomem *reg_base;
	uint32_t rpmheap_free;

	if (!m->private)
		return -EINVAL;

	pdata = m->private;

	reg_base = ioremap_nocache(pdata->heap_phys_addrbase, SZ_4);
	if (!reg_base) {
		pr_err("%s: ERROR could not ioremap start=%p\n",
			__func__, &pdata->heap_phys_addrbase);
		return -EBUSY;
	}

	rpmheap_free = readl_relaxed(reg_base);
	iounmap(reg_base);

	seq_printf(m, "RPM FREE HEAP SPACE is 0x%x Bytes\n", rpmheap_free);
	return 0;
}

static int msm_rpmheap_file_open(struct inode *inode, struct file *file)
{
	return single_open(file, msm_rpmheap_file_show, inode->i_private);
}

static const struct file_operations msm_rpmheap_fops = {
	.owner	  = THIS_MODULE,
	.open	  = msm_rpmheap_file_open,
	.read	  = seq_read,
	.release  = single_release,
	.llseek   = no_llseek,
};

static ssize_t rpmstats_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	struct msm_rpmstats_private_data *prvdata = NULL;
	struct msm_rpmstats_platform_data *pdata = NULL;
	ssize_t ret;

	mutex_lock(&rpm_stats_mutex);
	pdata = GET_PDATA_OF_ATTR(attr);

	prvdata =
		kmalloc(sizeof(*prvdata), GFP_KERNEL);
	if (!prvdata) {
		ret = -ENOMEM;
		goto kmalloc_fail;
	}

	prvdata->reg_base = ioremap_nocache(pdata->phys_addr_base,
					pdata->phys_size);
	if (!prvdata->reg_base) {
		pr_err("%s: ERROR could not ioremap start=%pa, len=%u\n",
			__func__, &pdata->phys_addr_base,
			pdata->phys_size);
		ret = -EBUSY;
		goto ioremap_fail;
	}

	prvdata->read_idx = prvdata->num_records =  prvdata->len = 0;
	prvdata->platform_data = pdata;
	if (pdata->version == 2)
		prvdata->num_records = 2;

	if (prvdata->platform_data->version == 1) {
		if (!prvdata->num_records)
			prvdata->num_records =
				readl_relaxed(prvdata->reg_base);
	}

	if (prvdata->read_idx < prvdata->num_records) {
		if (prvdata->platform_data->version == 1)
			prvdata->len = msm_rpmstats_copy_stats(prvdata);
		else if (prvdata->platform_data->version == 2)
			prvdata->len = msm_rpmstats_copy_stats_v2(
					prvdata);
	}

	ret = snprintf(buf, prvdata->len, prvdata->buf);
	iounmap(prvdata->reg_base);
ioremap_fail:
	kfree(prvdata);
kmalloc_fail:
	mutex_unlock(&rpm_stats_mutex);
	return ret;
}

static int msm_rpmstats_create_sysfs(struct msm_rpmstats_platform_data *pd)
{
	struct kobject *rpmstats_kobj = NULL;
	struct msm_rpmstats_kobj_attr *rpms_ka = NULL;
	int ret = 0;

	rpmstats_kobj = kobject_create_and_add("system_sleep", power_kobj);
	if (!rpmstats_kobj) {
		pr_err("%s: Cannot create rpmstats kobject\n", __func__);
		ret = -ENOMEM;
		goto fail;
	}

	rpms_ka = kzalloc(sizeof(*rpms_ka), GFP_KERNEL);
	if (!rpms_ka) {
		pr_err("%s: Cannot allocate mem for rpmstats kobj attr\n",
			__func__);
		kobject_put(rpmstats_kobj);
		ret = -ENOMEM;
		goto fail;
	}

	sysfs_attr_init(&rpms_ka->ka.attr);
	rpms_ka->pd = pd;
	rpms_ka->ka.attr.mode = 0444;
	rpms_ka->ka.attr.name = "stats";
	rpms_ka->ka.show = rpmstats_show;
	rpms_ka->ka.store = NULL;

	ret = sysfs_create_file(rpmstats_kobj, &rpms_ka->ka.attr);

fail:
	return ret;
}

static int msm_rpmstats_probe(struct platform_device *pdev)
{
	struct dentry *dent = NULL;
	struct msm_rpmstats_platform_data *pdata;
	struct msm_rpmstats_platform_data *pd;
	struct resource *res = NULL, *offset = NULL;
	struct device_node *node = NULL;
	uint32_t offset_addr = 0;
	void __iomem *phys_ptr = NULL;
	int ret = 0;

	if (!pdev)
		return -EINVAL;

	pdata = kzalloc(sizeof(struct msm_rpmstats_platform_data), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
							"phys_addr_base");
	if (!res)
		return -EINVAL;

	offset = platform_get_resource_byname(pdev, IORESOURCE_MEM,
							"offset_addr");
	if (offset) {
		/* Remap the rpm-stats pointer */
		phys_ptr = ioremap_nocache(offset->start, SZ_4);
		if (!phys_ptr) {
			pr_err("%s: Failed to ioremap address: %x\n",
					__func__, offset_addr);
			return -ENODEV;
		}
		offset_addr = readl_relaxed(phys_ptr);
		iounmap(phys_ptr);
	}

	pdata->phys_addr_base  = res->start + offset_addr;

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
			pr_err("%s: ERROR rpm_stats debugfs_create_file	fail\n",
					__func__);
			kfree(pdata);
			return -ENOMEM;
		}

	} else {
		kfree(pdata);
		return -EINVAL;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"heap_phys_addrbase");
	if (res) {
		heap_dent = debugfs_create_file("rpm_heap", S_IRUGO, NULL,
				pdata, &msm_rpmheap_fops);

		if (!heap_dent) {
			pr_err("%s: ERROR rpm_heap debugfs_create_file fail\n",
					__func__);
			kfree(pdata);
			return -ENOMEM;
		}
		pdata->heap_phys_addrbase = res->start;
	}

	msm_rpmstats_create_sysfs(pdata);

	platform_set_drvdata(pdev, dent);
	return 0;
}

static int msm_rpmstats_remove(struct platform_device *pdev)
{
	struct dentry *dent;

	dent = platform_get_drvdata(pdev);
	debugfs_remove(dent);
	debugfs_remove(heap_dent);
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
