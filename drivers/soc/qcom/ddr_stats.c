// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mailbox_client.h>
#include <linux/mailbox/qmp.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/soc/qcom/ddr_stats.h>
#include <linux/uaccess.h>
#include <asm/arch_timer.h>

#define MAGIC_KEY1		0xA1157A75
#define MAX_NUM_MODES		0x14
#define MSM_ARCH_TIMER_FREQ	19200000
#define MAX_DRV			18
#define MAX_MSG_LEN		35
#define DRV_ABSENT		0xdeaddead
#define DRV_INVALID		0xffffdead
#define VOTE_MASK		0x3fff
#define VOTE_X_SHIFT		14

#define GET_PDATA_OF_ATTR(attr) \
	(container_of(attr, struct ddr_stats_kobj_attr, ka)->pd)

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

struct ddr_stats_platform_data {
	void __iomem *reg;
	int freq_count;
	int entry_count;
	bool read_ddr_vote;
	struct mutex ddr_stats_lock;
	struct mbox_client stats_mbox_cl;
	struct mbox_chan *stats_mbox_ch;
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

static struct ddr_stats_platform_data *ddr_pdata;

static inline u64 get_time_in_msec(u64 counter)
{
	do_div(counter, MSM_ARCH_TIMER_FREQ);
	counter *= MSEC_PER_SEC;
	return counter;
}

static ssize_t ddr_stats_append_data_to_buf(char *buf, int length, int *count,
		struct stats_entry *data, u64 accumulated_duration)
{
	u32 cp_idx = 0;
	u32 name;
	u64 duration = 0;

	if (accumulated_duration) {
		duration = data->duration * 100;
		do_div(duration, accumulated_duration);
	}

	name = (data->name >> 8) & 0xFF;

	if (name == 0x0) {
		name = (data->name) & 0xFF;
		*count = *count + 1;
		return snprintf(buf, length,
				"LPM %d:\tName:0x%x\tcount:%u\tTime(msec):%llu (~%llu%%)\n",
				*count, name, data->count,
				data->duration, duration);
	} else if (name == 0x1) {
		cp_idx = data->name & 0x1F;
		name = data->name >> 16;

		if (!name || !data->count)
			return 0;

		return snprintf(buf, length,
				"Freq %dMhz:\tCP IDX:%u\tcount:%u\tTime(msec):%llu (~%llu%%)\n",
				name, cp_idx, data->count,
				data->duration, duration);
	}

	return 0;
}

static void ddr_stats_fill_data(void __iomem *reg, u32 entry_count,
			 struct stats_entry *data, u64 *accumulated_duration)
{
	int i;

	for (i = 0; i < entry_count; i++) {
		data[i].count = readl_relaxed(reg + offsetof(
					      struct stats_entry, count));

		data[i].name = readl_relaxed(reg + offsetof(
					     struct stats_entry, name));

		data[i].duration = readq_relaxed(reg + offsetof(
						 struct stats_entry, duration));

		data[i].duration = get_time_in_msec(data[i].duration);
		*accumulated_duration += data[i].duration;
		reg += sizeof(struct stats_entry);
	}
}

static ssize_t ddr_stats_copy_stats(char *buf, int size, void __iomem *reg, u32 entry_count)
{
	struct stats_entry data[MAX_NUM_MODES];
	u64 accumulated_duration = 0;
	int lpm_count = 0, i;
	ssize_t length, op_length;

	reg += offsetofend(struct ddr_stats_data, entry_count);

	ddr_stats_fill_data(reg, entry_count, data, &accumulated_duration);

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

int ddr_stats_get_freq_count(void)
{
	if (!ddr_pdata)
		return -ENODEV;

	return ddr_pdata->freq_count;
}
EXPORT_SYMBOL(ddr_stats_get_freq_count);

int ddr_stats_get_residency(int freq_count, struct ddr_freq_residency *data)
{
	void __iomem *reg;
	u32 name;
	int i, j, num;
	uint64_t duration = 0;
	struct stats_entry stats_data[MAX_NUM_MODES];

	if (freq_count < 0 || !data || !ddr_pdata || !ddr_pdata->reg)
		return -EINVAL;

	if (!ddr_pdata->entry_count)
		return -EINVAL;

	mutex_lock(&ddr_pdata->ddr_stats_lock);
	num = freq_count > ddr_pdata->freq_count ? ddr_pdata->freq_count : freq_count;
	reg = ddr_pdata->reg + offsetofend(struct ddr_stats_data, entry_count);

	ddr_stats_fill_data(reg, ddr_pdata->entry_count, stats_data, &duration);

	for (i = 0, j = 0; i < ddr_pdata->entry_count; i++) {
		name = stats_data[i].name;
		if (((name >> 8) & 0xFF) == 0x1) {
			data[j].freq = name >> 16;
			data[j].residency = stats_data[i].duration;
			if (++j > num)
				break;
		}
		reg += sizeof(struct stats_entry);
	}

	mutex_unlock(&ddr_pdata->ddr_stats_lock);

	return j;
}
EXPORT_SYMBOL(ddr_stats_get_residency);

int ddr_stats_get_ss_count(void)
{
	return ddr_pdata->read_ddr_vote ? MAX_DRV : -EOPNOTSUPP;
}
EXPORT_SYMBOL(ddr_stats_get_ss_count);

int ddr_stats_get_ss_vote_info(int ss_count,
			       struct ddr_stats_ss_vote_info *vote_info)
{
	char buf[MAX_MSG_LEN] = {};
	struct qmp_pkt pkt;
	void __iomem *reg;
	u32 vote_offset, val[MAX_DRV];
	int ret, i;

	if (!ddr_pdata->read_ddr_vote)
		return -EOPNOTSUPP;

	if (!vote_info || !(ss_count == MAX_DRV) || !ddr_pdata)
		return -ENODEV;

	mutex_lock(&ddr_pdata->ddr_stats_lock);
	ret = scnprintf(buf, MAX_MSG_LEN, "{class: ddr, res: drvs_ddr_votes}");
	pkt.size = (ret + 0x3) & ~0x3;
	pkt.data = buf;

	ret = mbox_send_message(ddr_pdata->stats_mbox_ch, &pkt);
	if (ret < 0) {
		pr_err("Error sending mbox message: %d\n", ret);
		mutex_unlock(&ddr_pdata->ddr_stats_lock);
		return ret;
	}

	vote_offset = sizeof(u32) + sizeof(u32) +
			(ddr_pdata->entry_count * sizeof(struct stats_entry));
	reg = ddr_pdata->reg;

	for (i = 0; i < ss_count; i++, reg += sizeof(u32)) {
		val[i] = readl_relaxed(reg + vote_offset);
		if (val[i] == DRV_ABSENT) {
			vote_info[i].ab = DRV_ABSENT;
			vote_info[i].ib = DRV_ABSENT;
			continue;
		} else if (val[i] == DRV_INVALID) {
			vote_info[i].ab = DRV_INVALID;
			vote_info[i].ib = DRV_INVALID;
			continue;
		}

		vote_info[i].ab = (val[i] >> VOTE_X_SHIFT) & VOTE_MASK;
		vote_info[i].ib = val[i] & VOTE_MASK;
	}

	mutex_unlock(&ddr_pdata->ddr_stats_lock);
	return 0;
}
EXPORT_SYMBOL(ddr_stats_get_ss_vote_info);

static ssize_t ddr_stats_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	struct ddr_stats_platform_data *pdata = NULL;
	ssize_t length;

	pdata = GET_PDATA_OF_ATTR(attr);
	length = ddr_stats_copy_stats(buf, PAGE_SIZE, pdata->reg, pdata->entry_count);

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
	struct resource *res = NULL, *offset = NULL;
	u32 offset_addr = 0, phys_size, key, name;
	void __iomem *phys_ptr = NULL, *reg;
	phys_addr_t phys_addr_base;
	int i;

	ddr_pdata = devm_kzalloc(&pdev->dev, sizeof(*ddr_pdata), GFP_KERNEL);
	if (!ddr_pdata)
		return -ENOMEM;

	mutex_init(&ddr_pdata->ddr_stats_lock);
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

	phys_addr_base = res->start + offset_addr;
	phys_size = resource_size(res);

	ddr_pdata->reg = devm_ioremap(&pdev->dev, phys_addr_base, phys_size);
	if (!ddr_pdata->reg) {
		pr_err("ERROR could not ioremap start=%pa, len=%u\n",
		       phys_addr_base, phys_size);
		return -ENODEV;
	}

	key = readl_relaxed(ddr_pdata->reg + offsetof(struct ddr_stats_data, key));
	if (key != MAGIC_KEY1) {
		pr_err("Invalid key\n");
		return -EINVAL;
	}

	ddr_pdata->entry_count = readl_relaxed(ddr_pdata->reg + offsetof(struct ddr_stats_data,
				    entry_count));
	if (ddr_pdata->entry_count > MAX_NUM_MODES) {
		pr_err("Invalid entry count\n");
		return 0;
	}

	reg = ddr_pdata->reg + offsetofend(struct ddr_stats_data, entry_count);

	for (i = 0; i < ddr_pdata->entry_count; i++) {
		name = readl_relaxed(reg + offsetof(struct stats_entry, name));
		name = (name >> 8) & 0xFF;
		if (name == 0x1)
			ddr_pdata->freq_count++;

		reg += sizeof(struct stats_entry);
	}

	ddr_pdata->stats_mbox_cl.dev = &pdev->dev;
	ddr_pdata->stats_mbox_cl.tx_block = true;
	ddr_pdata->stats_mbox_cl.tx_tout = 1000;
	ddr_pdata->stats_mbox_cl.knows_txdone = false;

	ddr_pdata->read_ddr_vote = true;
	ddr_pdata->stats_mbox_ch = mbox_request_channel(&ddr_pdata->stats_mbox_cl, 0);
	if (IS_ERR(ddr_pdata->stats_mbox_ch))
		ddr_pdata->read_ddr_vote = false;

	return ddr_stats_create_sysfs(pdev, ddr_pdata);
}

static int ddr_stats_remove(struct platform_device *pdev)
{
	struct ddr_stats_kobj_attr *ddr_stats_ka;

	ddr_stats_ka = (struct ddr_stats_kobj_attr *)
			platform_get_drvdata(pdev);

	if (ddr_pdata->read_ddr_vote)
		mbox_free_channel(ddr_stats_ka->pd->stats_mbox_ch);

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
