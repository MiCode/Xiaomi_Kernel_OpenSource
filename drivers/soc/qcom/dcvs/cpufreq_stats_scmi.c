// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/scmi_protocol.h>
#include <linux/scmi_cpufreq_stats.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/cpu.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/debugfs.h>

#define MAX_CLK_DOMAIN 8
#define SCMI_CPUFREQ_STATS_MSG_ID_PROTOCOL_ATTRIBUTES (16)
#define SCMI_CPUFREQ_STATS_DIR_STRING "cpufreq_stats"
#define CPUFREQ_STATS_USAGE_FILENAME "usage"
#define CPUFREQ_STATS_RESIDENCY_FILENAME "time_in_state"

const static struct scmi_cpufreq_stats_vendor_ops *ops;
static struct scmi_protocol_handle *ph;

enum entry_type {
	usage = 0,
	residency,
	ENTRY_MAX,
};

// strcuture to uniquely identify a fs entry
struct clkdom_entry {
	enum entry_type entry;
	u16 clkdom;
};

static struct dentry *dir;

struct scmi_stats {
	u32 signature;
	u16 revision;
	u16 attributes;
	u16 num_domains;
	u16 reserved0;
	u32 match_sequence;
	u32 perf_dom_entry_off_arr[];
} __packed;

struct perf_lvl_entry {
	u32 perf_lvl;
	u32 reserved0;
	u64 usage;
	u64 residency;
} __packed;

struct perf_dom_entry {
	u16 num_perf_levels;
	u16 curr_perf_idx;
	u32 ext_tbl_off;
	u64 ts_last_change;
	struct perf_lvl_entry perf_lvl_arr[];
} __packed;

struct stats_info {
	u32 stats_size;
	void __iomem *stats_iomem;
	u16 num_clkdom;
	struct clkdom_entry *entries;
	u32 *freq_info;
};

struct stats_info *pinfo;

static u32 get_num_opps_for_clkdom(u32 clkdom)
{
	u32 dom_data_off;
	void __iomem *dom_data;

	dom_data_off = 4 * readl_relaxed(pinfo->stats_iomem +
					 offsetof(struct scmi_stats,
						  perf_dom_entry_off_arr) +
					 4 * clkdom);
	dom_data = pinfo->stats_iomem + dom_data_off;
	return readl_relaxed(dom_data) & 0xFF;
}

static u32 get_freq_at_idx_for_clkdom(u32 clkdom, u32 idx)
{
	u32 dom_data_off;
	void __iomem *dom_data;

	dom_data_off = 4 * readl_relaxed(pinfo->stats_iomem +
					 offsetof(struct scmi_stats,
						  perf_dom_entry_off_arr) +
					 4 * clkdom);
	dom_data = pinfo->stats_iomem + dom_data_off +
		   offsetof(struct perf_dom_entry, perf_lvl_arr) +
		   idx * sizeof(struct perf_lvl_entry) +
		   offsetof(struct perf_lvl_entry, perf_lvl);
	return readl_relaxed(dom_data);
}

static ssize_t stats_get(struct file *file, char __user *user_buf, size_t count,
			 loff_t *ppos)
{
	u16 clkdom, num_lvl, i;
	u32 match_old = 0, match_new = 0;
	ssize_t r, bytes = 0;
	u64 *vals;
	void __iomem *dom_data;
	struct clkdom_entry *entry = (struct clkdom_entry *)file->private_data;
	struct dentry *dentry = file->f_path.dentry;
	ssize_t off = 0, perf_lvl_off = 0;
	char *str;

	r = debugfs_file_get(dentry);
	if (unlikely(r))
		return r;
	if (!entry)
		return -ENOENT;
	clkdom = entry->clkdom;
	dom_data = pinfo->stats_iomem +
		   4 * readl_relaxed(pinfo->stats_iomem +
				     offsetof(struct scmi_stats,
					      perf_dom_entry_off_arr) +
				     4 * clkdom);
	num_lvl = get_num_opps_for_clkdom(clkdom);
	if (!num_lvl)
		return 0;

	// allocate temporary variables
	vals = kcalloc(num_lvl, sizeof(u64), GFP_KERNEL);
	if (!vals)
		return -ENOMEM;
	str = kcalloc(1, 4096, GFP_KERNEL);
	if (!str)
		return -ENOMEM;

	// which offset within each perf_lvl entry
	if (entry->entry == usage)
		off = offsetof(struct perf_lvl_entry, usage);
	else if (entry->entry == residency)
		off = offsetof(struct perf_lvl_entry, residency);

	// read the iomem data for clkdom
	do {
		match_old = readl_relaxed(
			pinfo->stats_iomem +
			offsetof(struct scmi_stats, match_sequence));
		if (match_old % 2)
			continue;
		for (i = 0; i < num_lvl; i++) {
			perf_lvl_off =
				i * sizeof(struct perf_lvl_entry) +
				offsetof(struct perf_dom_entry, perf_lvl_arr);
			vals[i] = readl_relaxed(dom_data + perf_lvl_off + off) |
				  (u64)readl_relaxed(dom_data + perf_lvl_off +
						     off + 4)
					  << 32;
		}
		match_new = readl_relaxed(
			pinfo->stats_iomem +
			offsetof(struct scmi_stats, match_sequence));
	} while (match_old != match_new);

	for (i = 0; i < num_lvl; i++) {
		bytes += scnprintf(str + bytes, 4096 - bytes, "%u %llu\n",
				 pinfo->freq_info[pinfo->freq_info[clkdom] + i],
				 vals[i]);
	}

	r += simple_read_from_buffer(user_buf, count, ppos, str, bytes);
	debugfs_file_put(dentry);
	kfree(vals);
	kfree(str);
	return r;
}

static const struct file_operations stats_ops = {
	.read = stats_get,
	.open = simple_open,
	.llseek = default_llseek,
};

static int scmi_cpufreq_stats_create_fs_entries(struct device *dev)
{
	int i;
	struct dentry *ret;
	struct dentry *clkdom_dir = NULL;
	char clkdom_name[MAX_CLK_DOMAIN];

	// create the debugfs directory
	dir = debugfs_create_dir(SCMI_CPUFREQ_STATS_DIR_STRING, 0);
	if (!dir) {
		pr_err("Debugfs directory creation failed\n");
		return -ENOENT;
	}

	for (i = 0; i < pinfo->num_clkdom; i++) {
		snprintf(clkdom_name, MAX_CLK_DOMAIN, "clkdom%d", i);

		// create per-core dirs
		clkdom_dir = debugfs_create_dir(clkdom_name, dir);
		if (!clkdom_dir) {
			pr_err("Debugfs directory creation for %s failed\n",
				clkdom_name);
			return -ENOENT;
		}

		ret = debugfs_create_file(
			CPUFREQ_STATS_USAGE_FILENAME, 0400, clkdom_dir,
			pinfo->entries + i * ENTRY_MAX + usage, &stats_ops);
		ret = debugfs_create_file(
			CPUFREQ_STATS_RESIDENCY_FILENAME, 0400, clkdom_dir,
			pinfo->entries + i * ENTRY_MAX + residency, &stats_ops);
	}

	return 0;
}



static int qcom_cpufreq_stats_init(struct scmi_handle *handle)
{
	u32 stats_signature;
	u16 num_clkdom = 0, revision, num_lvl = 0;
	int i, j, ret;
	struct cpufreq_stats_prot_attr prot_attr;

	ret = ops->cpufreq_stats_info_get(ph, &prot_attr);
	if (ret) {
		pr_err("SCMI CPUFREQ Stats CPUFREQSTATS_GET_MEM_INFO error: %d\n", ret);
		return ret;
	}

	if (prot_attr.statistics_len) {
		pinfo = kcalloc(1, sizeof(struct stats_info), GFP_KERNEL);
		if (!pinfo)
			return -ENOMEM;
		pinfo->stats_iomem = ioremap(prot_attr.statistics_address_low |
						(u64)prot_attr.statistics_address_high << 32,
						 prot_attr.statistics_len);
		if (!pinfo->stats_iomem) {
			kfree(pinfo);
			return -ENOMEM;
		}
		stats_signature = readl_relaxed(
			pinfo->stats_iomem +
			offsetof(struct scmi_stats, signature));
		revision = readl_relaxed(pinfo->stats_iomem +
					 offsetof(struct scmi_stats,
						  revision)) & 0xFF;
		num_clkdom = readl_relaxed(pinfo->stats_iomem +
					   offsetof(struct scmi_stats,
						    num_domains)) & 0xFF;
		if (stats_signature != 0x50455246) {
			pr_err("SCMI stats mem signature check failed\n");
			iounmap(pinfo->stats_iomem);
			kfree(pinfo);
			return -EPERM;
		}
		if (revision != 1) {
			pr_err("SCMI stats revision not supported\n");
			iounmap(pinfo->stats_iomem);
			kfree(pinfo);
			return -EPERM;
		}
		if (!num_clkdom) {
			pr_err("SCMI cpufreq stats number of clock domains are zero\n");
			iounmap(pinfo->stats_iomem);
			kfree(pinfo);
			return -EPERM;
		}
		pinfo->num_clkdom = num_clkdom;
	} else {
		pr_err("SCMI cpufreq stats length is zero\n");
		return -EPERM;
	}
	// allocate structures for each clkdom/entry pair
	pinfo->entries = kcalloc(num_clkdom * ENTRY_MAX,
				 sizeof(struct clkdom_entry), GFP_KERNEL);
	if (!pinfo->entries) {
		iounmap(pinfo->stats_iomem);
		kfree(pinfo);
		return -ENOMEM;
	}

	// initialize structures for each clkdom/entry pair
	for (i = 0; i < num_clkdom; i++) {
		for (j = 0; j < ENTRY_MAX; j++) {
			(pinfo->entries + (i * ENTRY_MAX) + j)->entry = j;
			(pinfo->entries + (i * ENTRY_MAX) + j)->clkdom = i;
		}
	}

	// Create the sysfs/debugfs entries
	if (scmi_cpufreq_stats_create_fs_entries(handle->dev)) {
		pr_err("Failed to create debugfs entries\n");
		kfree(pinfo->entries);
		iounmap(pinfo->stats_iomem);
		kfree(pinfo);
		return -ENOENT;
	}

	// find the number of frequencies in platform and allocate memory for
	// storing them
	for (i = 0; i < num_clkdom; i++)
		num_lvl += get_num_opps_for_clkdom(i);
	pinfo->freq_info =
		kcalloc(num_lvl + num_clkdom, sizeof(u32), GFP_KERNEL);
	if (!pinfo->freq_info) {
		pr_err("Failed to allocate memory for freq entries\n");
		kfree(pinfo->entries);
		iounmap(pinfo->stats_iomem);
		kfree(pinfo);
		return -ENOMEM;
	}

	// Cache the cpufreq values
	for (i = 0; i < num_clkdom; i++) {
		// find the no. of freq lvls of all preceding clkdoms
		pinfo->freq_info[i] = num_clkdom;
		for (j = 0; j < i; j++)
			pinfo->freq_info[i] += get_num_opps_for_clkdom(j);

		num_lvl = get_num_opps_for_clkdom(i);
		if (!num_lvl)
			continue; // skip this clkdom

		for (j = 0; j < num_lvl; j++) {
			pinfo->freq_info[pinfo->freq_info[i] + j] =
				get_freq_at_idx_for_clkdom(i, j);
		}
	}

	return 0;
}

static int scmi_cpufreq_stats_probe(struct scmi_device *sdev)
{

	if (!sdev)
		return -ENODEV;

	ops = sdev->handle->devm_get_protocol(sdev, SCMI_CPUFREQ_STATS_PROTOCOL, &ph);
	if (IS_ERR(ops))
		return PTR_ERR(ops);
	return qcom_cpufreq_stats_init(sdev->handle);
}

static const struct scmi_device_id scmi_id_table[] = {
	{ .protocol_id = SCMI_CPUFREQ_STATS_PROTOCOL, .name = "scmi_cpufreq_stats_protocol" },
	{ },
};
MODULE_DEVICE_TABLE(scmi, scmi_id_table);

static struct scmi_driver scmi_cpufreq_stats_drv = {
	.name		= "scmi-cpufreq-stats-driver",
	.probe		= scmi_cpufreq_stats_probe,
	.id_table	= scmi_id_table,
};
module_scmi_driver(scmi_cpufreq_stats_drv);

MODULE_SOFTDEP("pre: cpufreq_stats_vendor");
MODULE_DESCRIPTION("ARM SCMI CPUFREQ STATS driver");
MODULE_LICENSE("GPL v2");
