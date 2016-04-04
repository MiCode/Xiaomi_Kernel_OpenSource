/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>

#include "sde_rotator_debug.h"
#include "sde_rotator_base.h"
#include "sde_rotator_core.h"
#include "sde_rotator_dev.h"

/*
 * sde_rotator_stat_show - Show statistics on read to this debugfs file
 * @s: Pointer to sequence file structure
 * @data: Pointer to private data structure
 */
static int sde_rotator_stat_show(struct seq_file *s, void *data)
{
	int i, offset;
	struct sde_rotator_device *rot_dev = s->private;
	struct sde_rotator_statistics *stats = &rot_dev->stats;
	u64 count = stats->count;
	int num_events;
	s64 proc_max, proc_min, proc_avg;

	proc_max = 0;
	proc_min = S64_MAX;
	proc_avg = 0;

	if (count > SDE_ROTATOR_NUM_EVENTS) {
		num_events = SDE_ROTATOR_NUM_EVENTS;
		offset = count % SDE_ROTATOR_NUM_EVENTS;
	} else {
		num_events = count;
		offset = 0;
	}

	for (i = 0; i < num_events; i++) {
		int k = (offset + i) % SDE_ROTATOR_NUM_EVENTS;
		ktime_t *ts = stats->ts[k];
		ktime_t start_time =
			ktime_before(ts[SDE_ROTATOR_TS_SRCQB],
					ts[SDE_ROTATOR_TS_DSTQB]) ?
					ts[SDE_ROTATOR_TS_SRCQB] :
					ts[SDE_ROTATOR_TS_DSTQB];
		s64 proc_time =
			ktime_to_us(ktime_sub(ts[SDE_ROTATOR_TS_RETIRE],
					start_time));

		seq_printf(s,
			"s:%d sq:%lld dq:%lld fe:%lld q:%lld c:%lld fl:%lld d:%lld sdq:%lld ddq:%lld t:%lld\n",
			i,
			ktime_to_us(ktime_sub(ts[SDE_ROTATOR_TS_FENCE],
					ts[SDE_ROTATOR_TS_SRCQB])),
			ktime_to_us(ktime_sub(ts[SDE_ROTATOR_TS_FENCE],
					ts[SDE_ROTATOR_TS_DSTQB])),
			ktime_to_us(ktime_sub(ts[SDE_ROTATOR_TS_QUEUE],
					ts[SDE_ROTATOR_TS_FENCE])),
			ktime_to_us(ktime_sub(ts[SDE_ROTATOR_TS_COMMIT],
					ts[SDE_ROTATOR_TS_QUEUE])),
			ktime_to_us(ktime_sub(ts[SDE_ROTATOR_TS_FLUSH],
					ts[SDE_ROTATOR_TS_COMMIT])),
			ktime_to_us(ktime_sub(ts[SDE_ROTATOR_TS_DONE],
					ts[SDE_ROTATOR_TS_FLUSH])),
			ktime_to_us(ktime_sub(ts[SDE_ROTATOR_TS_RETIRE],
					ts[SDE_ROTATOR_TS_DONE])),
			ktime_to_us(ktime_sub(ts[SDE_ROTATOR_TS_SRCDQB],
					ts[SDE_ROTATOR_TS_RETIRE])),
			ktime_to_us(ktime_sub(ts[SDE_ROTATOR_TS_DSTDQB],
					ts[SDE_ROTATOR_TS_RETIRE])),
			proc_time);

		proc_max = max(proc_max, proc_time);
		proc_min = min(proc_min, proc_time);
		proc_avg += proc_time;
	}

	proc_avg = (num_events) ?
			DIV_ROUND_CLOSEST_ULL(proc_avg, num_events) : 0;

	seq_printf(s, "count:%llu\n", count);
	seq_printf(s, "fai1:%llu\n", stats->fail_count);
	seq_printf(s, "t_max:%lld\n", proc_max);
	seq_printf(s, "t_min:%lld\n", proc_min);
	seq_printf(s, "t_avg:%lld\n", proc_avg);

	return 0;
}

/*
 * sde_rotator_stat_write - Clear statistics on write to this debugfs file.
 * @t_file:
 * @t_char:
 * @t_size_t:
 * @t_lof_t:
 */
static ssize_t sde_rotator_stat_write(struct file *t_file,
	const char *t_char, size_t t_size_t, loff_t *t_loff_t)
{
	struct seq_file *s = t_file->private_data;
	struct sde_rotator_device *rot_dev = s->private;
	struct sde_rotator_statistics *stats = &rot_dev->stats;
	char buf[128];

	mutex_lock(&rot_dev->lock);
	sde_rot_mgr_lock(rot_dev->mgr);
	memset(stats, 0, sizeof(struct sde_rotator_statistics));
	sde_rot_mgr_unlock(rot_dev->mgr);
	mutex_unlock(&rot_dev->lock);
	return simple_write_to_buffer(buf, sizeof(buf),
			t_loff_t, t_char, t_size_t);
}

/*
 * sde_rotator_raw_show - Show raw statistics on read from this debugfs file
 * @s: Pointer to sequence file structure
 * @data: Pointer to private data structure
 */
static int sde_rotator_raw_show(struct seq_file *s, void *data)
{
	int i, j, offset;
	struct sde_rotator_device *rot_dev = s->private;
	struct sde_rotator_statistics *stats = &rot_dev->stats;
	u64 count = stats->count;
	int num_events;

	if (count > SDE_ROTATOR_NUM_EVENTS) {
		num_events = SDE_ROTATOR_NUM_EVENTS;
		offset = count % SDE_ROTATOR_NUM_EVENTS;
	} else {
		num_events = count;
		offset = 0;
	}

	for (i = 0; i < num_events; i++) {
		int k = (offset + i) % SDE_ROTATOR_NUM_EVENTS;
		ktime_t *ts = stats->ts[k];

		seq_printf(s, "%d ", i);
		for (j = 0; j < SDE_ROTATOR_NUM_TIMESTAMPS; j++)
			seq_printf(s, "%lld ", ktime_to_us(ts[j]));
		seq_puts(s, "\n");
	}

	return 0;
}

/*
 * sde_rotator_dbg_open - Processed statistics debugfs file open function
 * @inode:
 * @file:
 */
static int sde_rotator_stat_open(struct inode *inode, struct file *file)
{
	return single_open(file, sde_rotator_stat_show, inode->i_private);
}

/*
 * sde_rotator_dbg_open - Raw statistics debugfs file open function
 * @inode:
 * @file:
 */
static int sde_rotator_raw_open(struct inode *inode, struct file *file)
{
	return single_open(file, sde_rotator_raw_show, inode->i_private);
}

/*
 * sde_rotator_dbg_open - Raw statistics debugfs file open function
 * @mdata: Pointer to rotator global data
 * @debugfs_root: Pointer to parent debugfs node
 */
static int sde_rotator_base_create_debugfs(
		struct sde_rot_data_type *mdata,
		struct dentry *debugfs_root)
{
	if (!debugfs_create_u32("iommu_ref_cnt", S_IRUGO,
			debugfs_root, &mdata->iommu_ref_cnt)) {
		SDEROT_WARN("failed to create debugfs iommu ref cnt\n");
		return -EINVAL;
	}

	return 0;
}

/*
 * sde_rotator_dbg_open - Raw statistics debugfs file open function
 * @mgr: Pointer to rotator manager structure
 * @debugfs_root: Pointer to parent debugfs node
 */
static int sde_rotator_core_create_debugfs(
		struct sde_rot_mgr *mgr,
		struct dentry *debugfs_root)
{
	int ret;

	if (!debugfs_create_u32("hwacquire_timeout", S_IRUGO | S_IWUSR,
			debugfs_root, &mgr->hwacquire_timeout)) {
		SDEROT_WARN("failed to create debugfs hw acquire timeout\n");
		return -EINVAL;
	}

	if (!debugfs_create_u32("ppc_numer", S_IRUGO | S_IWUSR,
			debugfs_root, &mgr->pixel_per_clk.numer)) {
		SDEROT_WARN("failed to create debugfs ppc numerator\n");
		return -EINVAL;
	}

	if (!debugfs_create_u32("ppc_denom", S_IRUGO | S_IWUSR,
			debugfs_root, &mgr->pixel_per_clk.denom)) {
		SDEROT_WARN("failed to create debugfs ppc denominator\n");
		return -EINVAL;
	}

	if (mgr->ops_hw_create_debugfs) {
		ret = mgr->ops_hw_create_debugfs(mgr, debugfs_root);
		if (ret)
			return ret;
	}
	return 0;
}

/*
 * struct sde_rotator_stat_ops - processed statistics file operations
 */
static const struct file_operations sde_rotator_stat_ops = {
	.open		= sde_rotator_stat_open,
	.read		= seq_read,
	.write		= sde_rotator_stat_write,
	.llseek		= seq_lseek,
	.release	= single_release
};

/*
 * struct sde_rotator_raw_ops - raw statistics file operations
 */
static const struct file_operations sde_rotator_raw_ops = {
	.open		= sde_rotator_raw_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release
};

/*
 * sde_rotator_create_debugfs - Setup rotator debugfs directory structure.
 * @rot_dev: Pointer to rotator device
 */
struct dentry *sde_rotator_create_debugfs(
		struct sde_rotator_device *rot_dev)
{
	struct dentry *debugfs_root;
	char dirname[32] = {0};

	snprintf(dirname, sizeof(dirname), "%s%d",
			SDE_ROTATOR_DRV_NAME, rot_dev->dev->id);
	debugfs_root = debugfs_create_dir(dirname, NULL);
	if (!debugfs_root) {
		SDEROT_ERR("fail create debugfs root\n");
		return NULL;
	}

	if (!debugfs_create_file("stats", S_IRUGO | S_IWUSR,
		debugfs_root, rot_dev, &sde_rotator_stat_ops)) {
		SDEROT_ERR("fail create debugfs stats\n");
		debugfs_remove_recursive(debugfs_root);
		return NULL;
	}

	if (!debugfs_create_file("raw", S_IRUGO | S_IWUSR,
		debugfs_root, rot_dev, &sde_rotator_raw_ops)) {
		SDEROT_ERR("fail create debugfs raw\n");
		debugfs_remove_recursive(debugfs_root);
		return NULL;
	}

	if (!debugfs_create_u32("fence_timeout", S_IRUGO | S_IWUSR,
			debugfs_root, &rot_dev->fence_timeout)) {
		SDEROT_ERR("fail create fence_timeout\n");
		debugfs_remove_recursive(debugfs_root);
		return NULL;
	}

	if (!debugfs_create_u32("streamoff_timeout", S_IRUGO | S_IWUSR,
			debugfs_root, &rot_dev->streamoff_timeout)) {
		SDEROT_ERR("fail create streamoff_timeout\n");
		debugfs_remove_recursive(debugfs_root);
		return NULL;
	}

	if (!debugfs_create_u32("early_submit", S_IRUGO | S_IWUSR,
			debugfs_root, &rot_dev->early_submit)) {
		SDEROT_ERR("fail create early_submit\n");
		debugfs_remove_recursive(debugfs_root);
		return NULL;
	}

	if (sde_rotator_base_create_debugfs(rot_dev->mdata, debugfs_root)) {
		SDEROT_ERR("fail create base debugfs\n");
		debugfs_remove_recursive(debugfs_root);
		return NULL;
	}

	if (sde_rotator_core_create_debugfs(rot_dev->mgr, debugfs_root)) {
		SDEROT_ERR("fail create core debugfs\n");
		debugfs_remove_recursive(debugfs_root);
		return NULL;
	}

	return debugfs_root;
}

/*
 * sde_rotator_destroy_debugfs - Destroy rotator debugfs directory structure.
 * @rot_dev: Pointer to rotator debugfs
 */
void sde_rotator_destroy_debugfs(struct dentry *debugfs)
{
	debugfs_remove_recursive(debugfs);
}
