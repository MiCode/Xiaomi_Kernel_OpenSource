/*
 * drivers/video/tegra/host/debug.c
 *
 * Copyright (C) 2010 Google, Inc.
 * Author: Erik Gilling <konkers@android.com>
 *
 * Copyright (C) 2011-2014, NVIDIA Corporation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#include <linux/io.h>

#include "dev.h"
#include "debug.h"
#include "nvhost_acm.h"
#include "nvhost_channel.h"
#include "chip_support.h"

pid_t nvhost_debug_null_kickoff_pid;
unsigned int nvhost_debug_trace_cmdbuf;

pid_t nvhost_debug_force_timeout_pid;
u32 nvhost_debug_force_timeout_val;
u32 nvhost_debug_force_timeout_channel;
u32 nvhost_debug_force_timeout_dump;

void nvhost_debug_output(struct output *o, const char* fmt, ...)
{
	va_list args;
	int len;

	va_start(args, fmt);
	len = vsnprintf(o->buf, sizeof(o->buf), fmt, args);
	va_end(args);
	o->fn(o->ctx, o->buf, len);
}

static int show_channels(struct platform_device *pdev, void *data,
			 int locked_id)
{
	struct nvhost_channel *ch;
	struct output *o = data;
	struct nvhost_master *m;
	struct nvhost_device_data *pdata;

	if (pdev == NULL)
		return 0;

	pdata = platform_get_drvdata(pdev);
	m = nvhost_get_host(pdev);
	ch = nvhost_getchannel(pdata->channel, true);
	if (ch->chid != locked_id)
		mutex_lock(&ch->cdma.lock);
	nvhost_get_chip_ops()->debug.show_channel_fifo(
		m, ch, o, pdata->index);
	nvhost_get_chip_ops()->debug.show_channel_cdma(
		m, ch, o, pdata->index);
	if (ch->chid != locked_id)
		mutex_unlock(&ch->cdma.lock);
	nvhost_putchannel(ch);

	return 0;
}

static void show_syncpts(struct nvhost_master *m, struct output *o)
{
	int i;
	nvhost_debug_output(o, "---- syncpts ----\n");
	for (i = 0; i < nvhost_syncpt_nb_pts(&m->syncpt); i++) {
		u32 max = nvhost_syncpt_read_max(&m->syncpt, i);
		u32 min = nvhost_syncpt_update_min(&m->syncpt, i);
		if (!min && !max)
			continue;
		nvhost_debug_output(o, "id %d (%s) min %d max %d\n",
				i, nvhost_get_chip_ops()->syncpt.name(&m->syncpt, i),
				min, max);
	}

	for (i = 0; i < nvhost_syncpt_nb_bases(&m->syncpt); i++) {
		u32 base_val;
		base_val = nvhost_syncpt_read_wait_base(&m->syncpt, i);
		if (base_val)
			nvhost_debug_output(o, "waitbase id %d val %d\n",
					i, base_val);
	}

	nvhost_debug_output(o, "\n");
}

static void show_all(struct nvhost_master *m, struct output *o,
		     int locked_id)
{
	nvhost_module_busy(m->dev);

	nvhost_get_chip_ops()->debug.show_mlocks(m, o);
	show_syncpts(m, o);
	nvhost_debug_output(o, "---- channels ----\n");
	nvhost_device_list_for_all(o, show_channels, locked_id);

	nvhost_module_idle(m->dev);
}

#ifdef CONFIG_DEBUG_FS
static int show_channels_no_fifo(struct platform_device *pdev, void *data,
				 int locked_id)
{
	struct nvhost_channel *ch;
	struct output *o = data;
	struct nvhost_master *m;
	struct nvhost_device_data *pdata;

	if (pdev == NULL)
		return 0;

	pdata = platform_get_drvdata(pdev);
	m = nvhost_get_host(pdev);
	ch = pdata->channel;
	if (ch) {
		mutex_lock(&ch->reflock);
		if (ch->refcount) {
			if (locked_id != ch->chid)
				mutex_lock(&ch->cdma.lock);
			nvhost_get_chip_ops()->debug.show_channel_cdma(m,
					ch, o, pdata->index);
			if (locked_id != ch->chid)
				mutex_unlock(&ch->cdma.lock);
		}
		mutex_unlock(&ch->reflock);
	}

	return 0;
}

static void show_all_no_fifo(struct nvhost_master *m, struct output *o,
			     int locked_id)
{
	nvhost_module_busy(m->dev);

	nvhost_get_chip_ops()->debug.show_mlocks(m, o);
	show_syncpts(m, o);
	nvhost_debug_output(o, "---- channels ----\n");
	nvhost_device_list_for_all(o, show_channels_no_fifo, locked_id);

	nvhost_module_idle(m->dev);
}

static int nvhost_debug_show_all(struct seq_file *s, void *unused)
{
	struct output o = {
		.fn = write_to_seqfile,
		.ctx = s
	};
	show_all(s->private, &o, -1);
	return 0;
}

static int nvhost_debug_show(struct seq_file *s, void *unused)
{
	struct output o = {
		.fn = write_to_seqfile,
		.ctx = s
	};
	show_all_no_fifo(s->private, &o, -1);
	return 0;
}

static int nvhost_debug_open_all(struct inode *inode, struct file *file)
{
	return single_open(file, nvhost_debug_show_all, inode->i_private);
}

static const struct file_operations nvhost_debug_all_fops = {
	.open		= nvhost_debug_open_all,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int nvhost_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, nvhost_debug_show, inode->i_private);
}

static const struct file_operations nvhost_debug_fops = {
	.open		= nvhost_debug_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

void nvhost_device_debug_init(struct platform_device *dev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);
	pdata->debugfs = debugfs_create_dir(dev->name, pdata->debugfs);
}

void nvhost_device_debug_deinit(struct platform_device *dev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);
	if (!IS_ERR_OR_NULL(pdata->debugfs))
		debugfs_remove(pdata->debugfs);
	pdata->debugfs = NULL;
}

void nvhost_debug_init(struct nvhost_master *master)
{
	struct nvhost_device_data *pdata;
	struct dentry *de = debugfs_create_dir("tegra_host", NULL);

	if (!de)
		return;

	pdata = platform_get_drvdata(master->dev);

	/* Store the created entry */
	pdata->debugfs = de;

	debugfs_create_file("status", S_IRUGO, de,
			master, &nvhost_debug_fops);
	debugfs_create_file("status_all", S_IRUGO, de,
			master, &nvhost_debug_all_fops);

	debugfs_create_u32("null_kickoff_pid", S_IRUGO|S_IWUSR, de,
			&nvhost_debug_null_kickoff_pid);
	debugfs_create_u32("trace_cmdbuf", S_IRUGO|S_IWUSR, de,
			&nvhost_debug_trace_cmdbuf);

	if (nvhost_get_chip_ops()->debug.debug_init)
		nvhost_get_chip_ops()->debug.debug_init(de);

	debugfs_create_u32("force_timeout_pid", S_IRUGO|S_IWUSR, de,
			&nvhost_debug_force_timeout_pid);
	debugfs_create_u32("force_timeout_val", S_IRUGO|S_IWUSR, de,
			&nvhost_debug_force_timeout_val);
	debugfs_create_u32("force_timeout_channel", S_IRUGO|S_IWUSR, de,
			&nvhost_debug_force_timeout_channel);
	debugfs_create_u32("force_timeout_dump", S_IRUGO|S_IWUSR, de,
			&nvhost_debug_force_timeout_dump);
	nvhost_debug_force_timeout_dump = 0;

#if defined(NVHOST_DEBUG)
	debugfs_create_u32("dbg_mask", S_IRUGO|S_IWUSR, de,
			&nvhost_dbg_mask);
	debugfs_create_u32("dbg_ftrace", S_IRUGO|S_IWUSR, de,
			&nvhost_dbg_ftrace);
#endif
	debugfs_create_u32("timeout_default_ms", S_IRUGO|S_IWUSR, de,
			&pdata->nvhost_timeout_default);
}

void nvhost_debug_dump_locked(struct nvhost_master *master, int locked_id)
{
	struct output o = {
		.fn = write_to_printk
	};
	show_all(master, &o, locked_id);
}

void nvhost_debug_dump(struct nvhost_master *master)
{
	struct output o = {
		.fn = write_to_printk
	};
	show_all_no_fifo(master, &o, -1);
}
#endif
