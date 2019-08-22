// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/uaccess.h>
#include <asm/arch_timer.h>

#include <clocksource/arm_arch_timer.h>

#define MAGIC_KEY1		0xA1157A75
#define MAX_NUM_MODES		0x14

#define GET_PDATA_OF_ATTR(attr) \
	(container_of(attr, struct ddr_stats_kobj_attr, ka)->pd)

struct ddr_stats_platform_data {
	phys_addr_t phys_addr_base;
	u32 phys_size;
};

struct stats_entry {
	uint32_t name;
	uint32_t count;
	uint64_t duration;
};

struct ddr_stats_data {
	uint32_t key;
	uint32_t entry_count;
	struct stats_entry entry[MAX_NUM_MODES];
};

struct ddr_stats_kobj_attr {
	struct kobject *kobj;
	struct kobj_attribute ka;
	struct ddr_stats_platform_data *pd;
};

static u64 get_time_in_msec(u64 counter)
{
	do_div(counter, (arch_timer_get_rate()/MSEC_PER_SEC));
	return counter;
}

static ssize_t ddr_stats_append_data_to_buf(char *buf, int length, int *count,
		struct stats_entry *data, u64 accumulated_duration)
{
	u32 cp_idx = 0, name, duration = 0;

	if (accumulated_duration)
		duration = (data->duration * 100) / accumulated_duration;

	name = (data->name >> 8) & 0xFF;

	if (name == 0x0) {
		name = (data->name) & 0xFF;
		*count = *count + 1;
		return snprintf(buf, length,
				"LPM %d:\tName:0x%x\tcount:%u\tTime(msec):%llu (~%d%%)\n",
				*count, name, data->count,
				data->duration, duration);
	} else if (name == 0x1) {
		cp_idx = data->name & 0x1F;
		name = data->name >> 16;

		if (!name || !data->count)
			return 0;

		return snprintf(buf, length,
				"Freq %dMhz:\tCP IDX:%u\tcount:%u\tTime(msec):%llu (~%d%%)\n",
				name, cp_idx, data->count,
				data->duration, duration);
	}

	return 0;
}

static ssize_t ddr_stats_copy_stats(char *buf, int size, void __iomem *reg,
							u32 entry_count)
{
	struct stats_entry data[MAX_NUM_MODES];
	u64 accumulated_duration = 0;
	int lpm_count = 0, i;
	ssize_t length, op_length;

	reg += offsetofend(struct ddr_stats_data, entry_count);

	for (i = 0; i < entry_count; i++) {
		data[i].count = readl_relaxed(reg + offsetof(
					      struct stats_entry, count));

		data[i].name = readl_relaxed(reg + offsetof(
					     struct stats_entry, name));

		data[i].duration = readq_relaxed(reg + offsetof(
						 struct stats_entry, duration));

		data[i].duration = get_time_in_msec(data[i].duration);
		accumulated_duration += data[i].duration;
		reg += sizeof(struct stats_entry);
	}

	for (i = 0, length = 0; i < entry_count; i++) {
		op_length = ddr_stats_append_data_to_buf(buf + length,
						size - length, &lpm_count,
						&data[i], accumulated_duration);
		if (op_length >= size - length)
			return length;

		length += op_length;
	}

	return length;
}

static ssize_t ddr_stats_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	struct ddr_stats_platform_data *pdata = NULL;
	void __iomem *reg;
	ssize_t length;
	u32 key, entry_count;

	pdata = GET_PDATA_OF_ATTR(attr);

	reg = ioremap_nocache(pdata->phys_addr_base, pdata->phys_size);
	if (!reg) {
		pr_err("ERROR could not ioremap start=%pa, len=%u\n",
		       &pdata->phys_addr_base, pdata->phys_size);
		return 0;
	}

	key = readl_relaxed(reg + offsetof(struct ddr_stats_data, key));
	if (key != MAGIC_KEY1) {
		pr_err("Invalid key\n");
		return 0;
	}

	entry_count = readl_relaxed(reg + offsetof(struct ddr_stats_data,
				    entry_count));
	if (entry_count > MAX_NUM_MODES) {
		pr_err("Invalid entry count\n");
		return 0;
	}

	length = ddr_stats_copy_stats(buf, PAGE_SIZE, reg, entry_count);
	iounmap(reg);

	return length;
}

static int ddr_stats_create_sysfs(struct platform_device *pdev,
				struct ddr_stats_platform_data *pd)
{
	struct kobject *ddr_stats_kobj = NULL;
	struct ddr_stats_kobj_attr *ddr_stats_ka = NULL;

	ddr_stats_kobj = kobject_create_and_add("ddr", power_kobj);
	if (!ddr_stats_kobj) {
		pr_err("Cannot create ddr stats kobject\n");
		return -ENODEV;
	}

	ddr_stats_ka = devm_kzalloc(&pdev->dev, sizeof(*ddr_stats_ka),
				    GFP_KERNEL);
	if (!ddr_stats_ka) {
		kobject_put(ddr_stats_kobj);
		return -ENOMEM;
	}

	ddr_stats_ka->kobj = ddr_stats_kobj;

	sysfs_attr_init(&ddr_stats_ka->ka.attr);
	ddr_stats_ka->pd = pd;
	ddr_stats_ka->ka.attr.mode = 0444;
	ddr_stats_ka->ka.attr.name = "residency";
	ddr_stats_ka->ka.show = ddr_stats_show;
	ddr_stats_ka->ka.store = NULL;

	platform_set_drvdata(pdev, ddr_stats_ka);

	return sysfs_create_file(ddr_stats_kobj, &ddr_stats_ka->ka.attr);
}

static int ddr_stats_probe(struct platform_device *pdev)
{
	struct ddr_stats_platform_data *pdata;
	struct resource *res = NULL, *offset = NULL;
	u32 offset_addr = 0;
	void __iomem *phys_ptr = NULL;

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					   "phys_addr_base");
	if (!res)
		return -ENODEV;

	offset = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					      "offset_addr");
	if (offset) {
		/* Remap the ddr stats pointer */
		phys_ptr = ioremap_nocache(offset->start, SZ_4);
		if (!phys_ptr) {
			pr_err("Failed to ioremap offset address\n");
			return -ENODEV;
		}
		offset_addr = readl_relaxed(phys_ptr);
		iounmap(phys_ptr);
	}

	if (!offset_addr)
		return -ENODEV;

	pdata->phys_addr_base  = res->start + offset_addr;
	pdata->phys_size = resource_size(res);

	return ddr_stats_create_sysfs(pdev, pdata);
}

static int ddr_stats_remove(struct platform_device *pdev)
{
	struct ddr_stats_kobj_attr *ddr_stats_ka;

	ddr_stats_ka = (struct ddr_stats_kobj_attr *)
			platform_get_drvdata(pdev);

	sysfs_remove_file(ddr_stats_ka->kobj, &ddr_stats_ka->ka.attr);
	kobject_put(ddr_stats_ka->kobj);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id ddr_stats_table[] = {
	{ .compatible = "qcom,ddr-stats" },
	{ },
};

static struct platform_driver ddr_stats_driver = {
	.probe = ddr_stats_probe,
	.remove = ddr_stats_remove,
	.driver = {
		.name = "ddr_stats",
		.of_match_table = ddr_stats_table,
	},
};
module_platform_driver(ddr_stats_driver);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MSM DDR Statistics driver");
MODULE_ALIAS("platform:msm_ddr_stats_log");
