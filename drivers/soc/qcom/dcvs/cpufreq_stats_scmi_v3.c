// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/debugfs.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/scmi_protocol.h>
#include <linux/slab.h>

#define PERF_STR 0x50455246 /* 'PERF' */
#define NAME_LEN 20
#define SCALING_FACTOR 4

struct scmi_perf_info {
	u32 version;
	u16 num_domains;
	enum scmi_power_scale power_scale;
	u64 stats_addr;
	u32 stats_size;
	struct perf_dom_info *dom_info;
};

struct scmi_protocol_handle {
	struct device *dev;
	const struct scmi_xfer_ops *xops;
	const struct scmi_proto_helpers_ops *hops;
	int (*set_priv)(const struct scmi_protocol_handle *ph, void *priv);
	void *(*get_priv)(const struct scmi_protocol_handle *ph);
};

enum entry_type {
	ENTRY_USAGE = 0,
	ENTRY_RESIDENCY,
	ENTRY_MAX,
};

/* structure to uniquely identify a fs entry */
struct clkdom_entry {
	enum entry_type entry;
	u16 clkdom;
};

struct scmi_stats {
	u32 signature;
	u16 revision;
	u16 attributes;
	u16 num_domains;
	u16 reserved;
	u32 match_sequence;
	u32 perf_dom_entry_off_arr[];
} __packed;

struct perf_lvl_entry {
	u32 perf_lvl;
	u32 reserved;
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

static struct stats_info *pinfo;

static u32 get_num_opps_for_clkdom(u32 clkdom)
{
	__le32 dom_data_off;
	void __iomem *dom_data;

	dom_data_off = SCALING_FACTOR * readl_relaxed(pinfo->stats_iomem +
					 offsetof(struct scmi_stats,
						  perf_dom_entry_off_arr) +
					 SCALING_FACTOR * clkdom);
	dom_data = pinfo->stats_iomem + dom_data_off;
	return readl_relaxed(dom_data) & 0xFF;
}

static u32 get_freq_at_idx_for_clkdom(u32 clkdom, u32 idx)
{
	__le32 dom_data_off;
	void __iomem *dom_data;

	dom_data_off = SCALING_FACTOR * readl_relaxed(pinfo->stats_iomem +
					 offsetof(struct scmi_stats,
						  perf_dom_entry_off_arr) +
					 SCALING_FACTOR * clkdom);
	dom_data = pinfo->stats_iomem + dom_data_off +
		   offsetof(struct perf_dom_entry, perf_lvl_arr) +
		   idx * sizeof(struct perf_lvl_entry) +
		   offsetof(struct perf_lvl_entry, perf_lvl);
	return readl_relaxed(dom_data);
}

static ssize_t cpufreq_stats_get(struct file *file, char __user *user_buf, size_t count,
			 loff_t *ppos)
{
	struct clkdom_entry *entry = (struct clkdom_entry *)file->private_data;
	struct dentry *dentry = file->f_path.dentry;
	ssize_t off = 0, perf_lvl_off = 0;
	u32 match_old = 0, match_new = 0;
	u16 clkdom, num_lvl, i;
	void __iomem *dom_data_off;
	ssize_t r, bytes = 0;
	char *str;
	u64 *vals;

	r = debugfs_file_get(dentry);
	if (unlikely(r))
		return r;
	if (!entry) {
		debugfs_file_put(dentry);
		return -ENOENT;
	}
	clkdom = entry->clkdom;
	dom_data_off = pinfo->stats_iomem +
			SCALING_FACTOR * readl_relaxed(pinfo->stats_iomem +
			offsetof(struct scmi_stats, perf_dom_entry_off_arr) +
				     SCALING_FACTOR * clkdom);
	num_lvl = get_num_opps_for_clkdom(clkdom);
	if (!num_lvl) {
		debugfs_file_put(dentry);
		return 0;
	}

	vals = kcalloc(num_lvl, sizeof(u64), GFP_KERNEL);
	if (!vals) {
		debugfs_file_put(dentry);
		return -ENOMEM;
	}
	str = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (!str) {
		kfree(vals);
		debugfs_file_put(dentry);
		return -ENOMEM;
	}

	/* which offset within each perf_lvl entry */
	if (entry->entry == ENTRY_USAGE)
		off = offsetof(struct perf_lvl_entry, usage);
	else if (entry->entry == ENTRY_RESIDENCY)
		off = offsetof(struct perf_lvl_entry, residency);

	/* read the iomem data for clkdom */
	do {
		match_old = readl_relaxed(
			pinfo->stats_iomem +
			offsetof(struct scmi_stats, match_sequence));
		if (match_old & 1)
			continue;
		for (i = 0; i < num_lvl; i++) {
			perf_lvl_off =
				i * sizeof(struct perf_lvl_entry) +
				offsetof(struct perf_dom_entry, perf_lvl_arr);
			vals[i] = readl_relaxed(dom_data_off + perf_lvl_off + off) |
				  (u64)readl_relaxed(dom_data_off + perf_lvl_off +
						     off + 4) << 32;
		}
		match_new = readl_relaxed(
			pinfo->stats_iomem +
			offsetof(struct scmi_stats, match_sequence));
	} while (match_old != match_new);

	for (i = 0; i < num_lvl; i++) {
		bytes += scnprintf(str + bytes, PAGE_SIZE - bytes, "%u %llu\n",
				 pinfo->freq_info[pinfo->freq_info[clkdom] + i],
				 vals[i]);
	}

	r = simple_read_from_buffer(user_buf, count, ppos, str, bytes);
	kfree(vals);
	kfree(str);
	debugfs_file_put(dentry);
	return r;
}

static const struct file_operations stats_ops = {
	.read = cpufreq_stats_get,
	.open = simple_open,
	.llseek = default_llseek,
};

static int scmi_cpufreq_stats_create_fs_entries(struct device *dev)
{
	struct dentry *clkdom_dir = NULL;
	char clkdom_name[NAME_LEN];
	struct dentry *dir, *ret;
	int i;

	dir = debugfs_create_dir("cpufreq_stats", 0);
	if (IS_ERR(dir)) {
		pr_err("Failed to create cpufreq_stats debugfs %ld\n", PTR_ERR(dir));
		return PTR_ERR(dir);
	}
	for (i = 0; i < pinfo->num_clkdom; i++) {
		snprintf(clkdom_name, NAME_LEN, "clkdom%d", i);

		clkdom_dir = debugfs_create_dir(clkdom_name, dir);
		if (IS_ERR(clkdom_dir)) {
			dev_err(dev,
				"Debugfs directory creation for %s failed\n",
				clkdom_name);
			return PTR_ERR(clkdom_dir);
		}

		ret = debugfs_create_file(
			"usage", 0400, clkdom_dir,
			pinfo->entries + i * ENTRY_MAX + ENTRY_USAGE, &stats_ops);
		if (IS_ERR(ret)) {
			pr_err("Failed to create cpufreq_stats debugfs %ld\n", PTR_ERR(ret));
			return PTR_ERR(ret);
		}
		ret = debugfs_create_file(
			"time_in_state", 0400, clkdom_dir,
			pinfo->entries + i * ENTRY_MAX + ENTRY_RESIDENCY, &stats_ops);
		if (IS_ERR(ret)) {
			pr_err("Failed to create cpufreq_stats debugfs %ld\n", PTR_ERR(ret));
			return PTR_ERR(ret);
		}
	}
	return 0;
}

static int qcom_cpufreq_stats_init(struct scmi_perf_info *pi, struct scmi_protocol_handle *ph)
{
	u32 stats_signature;
	u16 num_clkdom = 0, revision, num_lvl = 0;
	int i, j;
	struct clkdom_entry *entry;

	if (pi->stats_size) {
		pinfo = devm_kzalloc(ph->dev, sizeof(struct stats_info), GFP_KERNEL);
		if (!pinfo)
			return -ENOMEM;
		pinfo->stats_iomem = devm_ioremap(
			ph->dev, pi->stats_addr, pi->stats_size);
		if (!pinfo->stats_iomem)
			return -ENOMEM;
		stats_signature = readl_relaxed(
			pinfo->stats_iomem +
			offsetof(struct scmi_stats, signature));
		revision = readl_relaxed(pinfo->stats_iomem +
					 offsetof(struct scmi_stats,
						  revision)) & 0xFF;
		num_clkdom = readl_relaxed(pinfo->stats_iomem +
					   offsetof(struct scmi_stats,
						    num_domains)) & 0xFF;
		if (stats_signature != PERF_STR) {
			dev_err(ph->dev, "SCMI stats mem signature check failed\n");
			return -EPERM;
		}
		if (revision != 1) {
			dev_err(ph->dev, "SCMI stats revision not supported\n");
			return -EPERM;
		}
		if (!num_clkdom) {
			dev_err(ph->dev, "SCMI cpufreq stats number of clock domains are zero\n");
			return -EPERM;
		}
		pinfo->num_clkdom = num_clkdom;
	} else {
		dev_err(ph->dev, "SCMI cpufreq stats length is zero\n");
		return -EPERM;
	}
	/* allocate structures for each clkdom/entry pair */
	pinfo->entries = devm_kcalloc(ph->dev, num_clkdom * ENTRY_MAX,
				 sizeof(struct clkdom_entry), GFP_KERNEL);
	if (!pinfo->entries)
		return -ENOMEM;
	/* initialize structures for each clkdom/entry pair */
	for (i = 0; i < num_clkdom; i++) {
		entry = pinfo->entries + (i * ENTRY_MAX);
		for (j = 0; j < ENTRY_MAX; j++) {
			entry[j].entry = j;
			entry[j].clkdom = i;
		}
	}
	if (scmi_cpufreq_stats_create_fs_entries(ph->dev)) {
		dev_err(ph->dev, "Failed to create debugfs entries\n");
		return -ENOENT;
	}
	/*find the number of frequencies in platform and allocate memory for storing them */
	for (i = 0; i < num_clkdom; i++)
		num_lvl += get_num_opps_for_clkdom(i);
	pinfo->freq_info =
		devm_kcalloc(ph->dev, num_lvl + num_clkdom, sizeof(u32), GFP_KERNEL);
	if (!pinfo->freq_info)
		return -ENOMEM;
	/* Cache the cpufreq values */
	for (i = 0; i < num_clkdom; i++) {
		/* find the no. of freq lvls of all preceding clkdoms */
		pinfo->freq_info[i] = num_clkdom;
		for (j = 0; j < i; j++)
			pinfo->freq_info[i] += get_num_opps_for_clkdom(j);

		num_lvl = get_num_opps_for_clkdom(i);
		if (!num_lvl)
			continue;
		for (j = 0; j < num_lvl; j++) {
			pinfo->freq_info[pinfo->freq_info[i] + j] =
				get_freq_at_idx_for_clkdom(i, j);
		}
	}
	return 0;
}

static int scmi_cpufreq_stats_probe(struct scmi_device *sdev)
{
	static const struct scmi_perf_proto_ops *perf_ops;
	const struct scmi_handle *handle = sdev->handle;
	struct scmi_protocol_handle *ph;
	struct scmi_perf_info *pi;

	if (!handle)
		return -ENODEV;
	perf_ops = handle->devm_protocol_get(sdev, SCMI_PROTOCOL_PERF, &ph);
	if (IS_ERR(perf_ops))
		return PTR_ERR(perf_ops);
	pi = ph->get_priv(ph);
	return qcom_cpufreq_stats_init(pi, ph);
}

static const struct scmi_device_id scmi_id_table[] = {
	{ SCMI_PROTOCOL_PERF, "cpufreqstats" },
	{ },
};
MODULE_DEVICE_TABLE(scmi, scmi_id_table);

static struct scmi_driver scmi_cpufreq_stats_driver = {
	.name = "scmi-cpufreq-stats",
	.probe = scmi_cpufreq_stats_probe,
	.id_table = scmi_id_table,
};

module_scmi_driver(scmi_cpufreq_stats_driver);
MODULE_DESCRIPTION("QTI SCMI CPUFREQ STATS driver");
MODULE_LICENSE("GPL");
