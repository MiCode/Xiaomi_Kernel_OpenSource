// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (c) 2011-2021, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include <clocksource/arm_arch_timer.h>

#ifdef CONFIG_ARM
#ifndef readq_relaxed
#define readq_relaxed(a) ({			\
	u64 val = readl_relaxed((a) + 4);	\
	val <<= 32;				\
	val |=  readl_relaxed((a));		\
	val;					\
})
#endif
#endif

struct stats_config {
	u32 offset_addr;
	u32 num_records;
	bool appended_stats_avail;
};

struct soc_sleep_stats_data {
	phys_addr_t stats_base;
	resource_size_t stats_size;
	const struct stats_config *config;
	struct kobject *kobj;
	struct kobj_attribute ka;
	void __iomem *reg;
};

struct entry {
	__le32 stat_type;
	__le32 count;
	__le64 last_entered_at;
	__le64 last_exited_at;
	__le64 accumulated;
};

struct appended_entry {
	__le32 client_votes;
	__le32 reserved[3];
};

struct stats_entry {
	struct entry entry;
	struct appended_entry appended_entry;
};

static inline u64 get_time_in_sec(u64 counter)
{
	do_div(counter, arch_timer_get_rate());

	return counter;
}

static inline ssize_t append_data_to_buf(char *buf, int length,
					 struct stats_entry *data)
{
	char stat_type[5] = {0};

	memcpy(stat_type, &data->entry.stat_type, sizeof(u32));

	return scnprintf(buf, length,
			 "%s\n"
			 "\tCount                    :%u\n"
			 "\tLast Entered At(sec)     :%llu\n"
			 "\tLast Exited At(sec)      :%llu\n"
			 "\tAccumulated Duration(sec):%llu\n"
			 "\tClient Votes             :0x%x\n\n",
			 stat_type, data->entry.count,
			 data->entry.last_entered_at,
			 data->entry.last_exited_at,
			 data->entry.accumulated,
			 data->appended_entry.client_votes);
}

static ssize_t stats_show(struct kobject *obj, struct kobj_attribute *attr,
			  char *buf)
{
	int i;
	uint32_t offset;
	ssize_t length = 0, op_length;
	struct stats_entry data;
	struct entry *e = &data.entry;
	struct appended_entry *ae = &data.appended_entry;
	struct soc_sleep_stats_data *drv = container_of(attr,
					   struct soc_sleep_stats_data, ka);
	void __iomem *reg = drv->reg;

	for (i = 0; i < drv->config->num_records; i++) {
		offset = offsetof(struct entry, stat_type);
		e->stat_type = le32_to_cpu(readl_relaxed(reg + offset));

		offset = offsetof(struct entry, count);
		e->count = le32_to_cpu(readl_relaxed(reg + offset));

		offset = offsetof(struct entry, last_entered_at);
		e->last_entered_at = le64_to_cpu(readq_relaxed(reg + offset));

		offset = offsetof(struct entry, last_exited_at);
		e->last_exited_at = le64_to_cpu(readq_relaxed(reg + offset));

		offset = offsetof(struct entry, accumulated);
		e->accumulated = le64_to_cpu(readq_relaxed(reg + offset));

		e->last_entered_at = get_time_in_sec(e->last_entered_at);
		e->last_exited_at = get_time_in_sec(e->last_exited_at);
		e->accumulated = get_time_in_sec(e->accumulated);

		reg += sizeof(struct entry);

		if (drv->config->appended_stats_avail) {
			offset = offsetof(struct appended_entry, client_votes);
			ae->client_votes = le32_to_cpu(readl_relaxed(reg +
								     offset));

			reg += sizeof(struct appended_entry);
		} else {
			ae->client_votes = 0;
		}

		op_length = append_data_to_buf(buf + length, PAGE_SIZE - length,
					       &data);
		if (op_length >= PAGE_SIZE - length)
			goto exit;

		length += op_length;
	}
exit:
	return length;
}

static int soc_sleep_stats_create_sysfs(struct platform_device *pdev,
					struct soc_sleep_stats_data *drv)
{
	drv->kobj = kobject_create_and_add("soc_sleep", power_kobj);
	if (!drv->kobj)
		return -ENOMEM;

	sysfs_attr_init(&drv->ka.attr);
	drv->ka.attr.mode = 0444;
	drv->ka.attr.name = "stats";
	drv->ka.show = stats_show;

	return sysfs_create_file(drv->kobj, &drv->ka.attr);
}

static const struct stats_config legacy_rpm_data = {
	.num_records = 2,
	.appended_stats_avail = true,
};

static const struct stats_config rpm_data = {
	.offset_addr = 0x14,
	.num_records = 2,
	.appended_stats_avail = true,
};

static const struct stats_config rpmh_data = {
	.offset_addr = 0x4,
	.num_records = 3,
	.appended_stats_avail = false,
};

static const struct of_device_id soc_sleep_stats_table[] = {
	{ .compatible = "qcom,rpm-sleep-stats", .data = &rpm_data},
	{ .compatible = "qcom,rpmh-sleep-stats", .data = &rpmh_data},
	{ .compatible = "qcom,legacy-rpm-sleep-stats", .data = &legacy_rpm_data},
	{ },
};

static int soc_sleep_stats_probe(struct platform_device *pdev)
{
	struct soc_sleep_stats_data *drv;
	struct resource *res;
	void __iomem *offset_addr;
	uint32_t offset = 0;
	int ret;

	drv = devm_kzalloc(&pdev->dev, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;

	drv->config = of_device_get_match_data(&pdev->dev);
	if (!drv->config)
		return -ENODEV;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return PTR_ERR(res);

	if (drv->config->offset_addr) {
		offset_addr = devm_ioremap_nocache(&pdev->dev, res->start +
						   drv->config->offset_addr,
						   sizeof(u32));
		if (!offset_addr)
			return -ENOMEM;

		offset = readl_relaxed(offset_addr);
	}

	drv->stats_base = res->start | offset;
	drv->stats_size = resource_size(res);

	ret = soc_sleep_stats_create_sysfs(pdev, drv);
	if (ret) {
		pr_err("Failed to create sysfs interface\n");
		return ret;
	}

	drv->reg = devm_ioremap(&pdev->dev, drv->stats_base, drv->stats_size);
	if (!drv->reg) {
		pr_err("ioremap failed\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, drv);
	return 0;
}

static int soc_sleep_stats_remove(struct platform_device *pdev)
{
	struct soc_sleep_stats_data *drv = platform_get_drvdata(pdev);

	sysfs_remove_file(drv->kobj, &drv->ka.attr);
	kobject_put(drv->kobj);

	return 0;
}

static struct platform_driver soc_sleep_stats_driver = {
	.probe = soc_sleep_stats_probe,
	.remove = soc_sleep_stats_remove,
	.driver = {
		.name = "soc_sleep_stats",
		.of_match_table = soc_sleep_stats_table,
	},
};
module_platform_driver(soc_sleep_stats_driver);

MODULE_DESCRIPTION("Qualcomm Technologies, Inc. SoC sleep stats driver");
MODULE_LICENSE("GPL v2");
