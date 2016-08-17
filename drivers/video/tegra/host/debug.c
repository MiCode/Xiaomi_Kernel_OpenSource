/*
 * drivers/video/tegra/host/debug.c
 *
 * Copyright (C) 2010 Google, Inc.
 * Author: Erik Gilling <konkers@android.com>
 *
 * Copyright (C) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
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

static int show_channels(struct platform_device *pdev, void *data)
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
		int locked = mutex_trylock(&ch->reflock);
		if (ch->refcount) {
			mutex_lock(&ch->cdma.lock);
			nvhost_get_chip_ops()->debug.show_channel_fifo(
				m, ch, o, pdata->index);
			nvhost_get_chip_ops()->debug.show_channel_cdma(
				m, ch, o, pdata->index);
			mutex_unlock(&ch->cdma.lock);
		}
		if (locked)
			mutex_unlock(&ch->reflock);
	}

	return 0;
}

static void show_syncpts(struct nvhost_master *m, struct output *o)
{
	int i;
	BUG_ON(!nvhost_get_chip_ops()->syncpt.name);
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

static void show_all(struct nvhost_master *m, struct output *o)
{
	nvhost_module_busy(m->dev);

	nvhost_get_chip_ops()->debug.show_mlocks(m, o);
	show_syncpts(m, o);
	nvhost_debug_output(o, "---- channels ----\n");
	nvhost_device_list_for_all(o, show_channels);

	nvhost_module_idle(m->dev);
}

#ifdef CONFIG_DEBUG_FS
static int show_channels_no_fifo(struct platform_device *pdev, void *data)
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
			mutex_lock(&ch->cdma.lock);
			nvhost_get_chip_ops()->debug.show_channel_cdma(m,
					ch, o, pdata->index);
			mutex_unlock(&ch->cdma.lock);
		}
		mutex_unlock(&ch->reflock);
	}

	return 0;
}

static void show_all_no_fifo(struct nvhost_master *m, struct output *o)
{
	nvhost_module_busy(m->dev);

	nvhost_get_chip_ops()->debug.show_mlocks(m, o);
	show_syncpts(m, o);
	nvhost_debug_output(o, "---- channels ----\n");
	nvhost_device_list_for_all(o, show_channels_no_fifo);

	nvhost_module_idle(m->dev);
}

static int nvhost_debug_show_all(struct seq_file *s, void *unused)
{
	struct output o = {
		.fn = write_to_seqfile,
		.ctx = s
	};
	show_all(s->private, &o);
	return 0;
}

static int nvhost_debug_show(struct seq_file *s, void *unused)
{
	struct output o = {
		.fn = write_to_seqfile,
		.ctx = s
	};
	show_all_no_fifo(s->private, &o);
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

static int actmon_below_wmark_show(struct seq_file *s, void *unused)
{
	struct nvhost_master *host = s->private;
	seq_printf(s, "%d\n", actmon_op().below_wmark_count(host));
	return 0;
}

static int actmon_below_wmark_open(struct inode *inode, struct file *file)
{
	return single_open(file, actmon_below_wmark_show, inode->i_private);
}

static const struct file_operations actmon_below_wmark_fops = {
	.open		= actmon_below_wmark_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int actmon_above_wmark_show(struct seq_file *s, void *unused)
{
	struct nvhost_master *host = s->private;
	seq_printf(s, "%d\n", actmon_op().above_wmark_count(host));
	return 0;
}

static int actmon_above_wmark_open(struct inode *inode, struct file *file)
{
	return single_open(file, actmon_above_wmark_show, inode->i_private);
}

static const struct file_operations actmon_above_wmark_fops = {
	.open		= actmon_above_wmark_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int actmon_avg_show(struct seq_file *s, void *unused)
{
	struct nvhost_master *host = s->private;
	u32 avg;
	int err;

	err = actmon_op().read_avg(host, &avg);
	if (!err)
		seq_printf(s, "%d\n", avg);
	return err;
}

static int actmon_avg_open(struct inode *inode, struct file *file)
{
	return single_open(file, actmon_avg_show, inode->i_private);
}

static const struct file_operations actmon_avg_fops = {
	.open		= actmon_avg_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int actmon_avg_norm_show(struct seq_file *s, void *unused)
{
	struct nvhost_master *host = s->private;
	u32 avg;
	int err;

	err = actmon_op().read_avg_norm(host, &avg);
	if (!err)
		seq_printf(s, "%d\n", avg);
	return err;
}

static int actmon_avg_norm_open(struct inode *inode, struct file *file)
{
	return single_open(file, actmon_avg_norm_show, inode->i_private);
}

static const struct file_operations actmon_avg_norm_fops = {
	.open		= actmon_avg_norm_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int actmon_sample_period_show(struct seq_file *s, void *unused)
{
	struct nvhost_master *host = s->private;
	long period = actmon_op().get_sample_period(host);
	seq_printf(s, "%ld\n", period);
	return 0;
}

static int actmon_sample_period_open(struct inode *inode, struct file *file)
{
	return single_open(file, actmon_sample_period_show, inode->i_private);
}

static const struct file_operations actmon_sample_period_fops = {
	.open		= actmon_sample_period_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int actmon_sample_period_norm_show(struct seq_file *s, void *unused)
{
	struct nvhost_master *host = s->private;
	long period = actmon_op().get_sample_period_norm(host);
	seq_printf(s, "%ld\n", period);
	return 0;
}

static int actmon_sample_period_norm_open(struct inode *inode,
						struct file *file)
{
	return single_open(file, actmon_sample_period_norm_show,
		inode->i_private);
}

static int actmon_sample_period_norm_write(struct file *file,
				const char __user *user_buf,
				size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct nvhost_master *host = s->private;
	char buffer[40];
	int buf_size;
	unsigned long period;

	memset(buffer, 0, sizeof(buffer));
	buf_size = min(count, (sizeof(buffer)-1));

	if (copy_from_user(buffer, user_buf, buf_size))
		return -EFAULT;

	if (kstrtoul(buffer, 10, &period))
		return -EINVAL;

	actmon_op().set_sample_period_norm(host, period);

	return count;
}

static const struct file_operations actmon_sample_period_norm_fops = {
	.open		= actmon_sample_period_norm_open,
	.read		= seq_read,
	.write          = actmon_sample_period_norm_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};



static int actmon_k_show(struct seq_file *s, void *unused)
{
	struct nvhost_master *host = s->private;
	long period = actmon_op().get_k(host);
	seq_printf(s, "%ld\n", period);
	return 0;
}

static int actmon_k_open(struct inode *inode, struct file *file)
{
	return single_open(file, actmon_k_show, inode->i_private);
}

static int actmon_k_write(struct file *file,
				const char __user *user_buf,
				size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct nvhost_master *host = s->private;
	char buffer[40];
	int buf_size;
	unsigned long k;

	memset(buffer, 0, sizeof(buffer));
	buf_size = min(count, (sizeof(buffer)-1));

	if (copy_from_user(buffer, user_buf, buf_size))
		return -EFAULT;

	if (kstrtoul(buffer, 10, &k))
		return -EINVAL;

	actmon_op().set_k(host, k);

	return count;
}

static const struct file_operations actmon_k_fops = {
	.open		= actmon_k_open,
	.read		= seq_read,
	.write          = actmon_k_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};


static int tickcount_show(struct seq_file *s, void *unused)
{
	struct platform_device *dev = s->private;
	u64 cnt;
	int err;

	err = tickctrl_op().tickcount(dev, &cnt);
	if (!err)
		seq_printf(s, "%lld\n", cnt);
	return err;
}

static int tickcount_open(struct inode *inode, struct file *file)
{
	if (!tickctrl_op().tickcount)
		return -ENODEV;

	return single_open(file, tickcount_show, inode->i_private);
}

static const struct file_operations tickcount_fops = {
	.open		= tickcount_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int stallcount_show(struct seq_file *s, void *unused)
{
	struct platform_device *dev = s->private;
	u64 cnt;
	int err;

	err = tickctrl_op().stallcount(dev, &cnt);
	if (!err)
		seq_printf(s, "%lld\n", cnt);
	return err;
}

static int stallcount_open(struct inode *inode, struct file *file)
{
	if (!tickctrl_op().stallcount)
		return -ENODEV;

	return single_open(file, stallcount_show, inode->i_private);
}

static const struct file_operations stallcount_fops = {
	.open		= stallcount_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int xfercount_show(struct seq_file *s, void *unused)
{
	struct platform_device *dev = s->private;
	u64 cnt;
	int err;

	err = tickctrl_op().xfercount(dev, &cnt);
	if (!err)
		seq_printf(s, "%lld\n", cnt);
	return err;
}

static int xfercount_open(struct inode *inode, struct file *file)
{
	if (!tickctrl_op().xfercount)
		return -ENODEV;

	return single_open(file, xfercount_show, inode->i_private);
}

static const struct file_operations xfercount_fops = {
	.open		= xfercount_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

void nvhost_device_debug_init(struct platform_device *dev)
{
	struct dentry *de = NULL;
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);

	de = debugfs_create_dir(dev->name, de);
	debugfs_create_file("stallcount", S_IRUGO, de, dev, &stallcount_fops);
	debugfs_create_file("xfercount", S_IRUGO, de, dev, &xfercount_fops);
	debugfs_create_file("tickcount", S_IRUGO, de, dev, &tickcount_fops);

	pdata->debugfs = de;
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

	debugfs_create_file("3d_actmon_k", S_IRUGO, de,
			master, &actmon_k_fops);
	debugfs_create_file("3d_actmon_sample_period", S_IRUGO, de,
			master, &actmon_sample_period_fops);
	debugfs_create_file("3d_actmon_sample_period_norm", S_IRUGO, de,
			master, &actmon_sample_period_norm_fops);
	debugfs_create_file("3d_actmon_avg_norm", S_IRUGO, de,
			master, &actmon_avg_norm_fops);
	debugfs_create_file("3d_actmon_avg", S_IRUGO, de,
			master, &actmon_avg_fops);
	debugfs_create_file("3d_actmon_above_wmark", S_IRUGO, de,
			master, &actmon_above_wmark_fops);
	debugfs_create_file("3d_actmon_below_wmark", S_IRUGO, de,
			master, &actmon_below_wmark_fops);
}
#else
void nvhost_debug_init(struct nvhost_master *master)
{
}
#endif

void nvhost_debug_dump(struct nvhost_master *master)
{
	struct output o = {
		.fn = write_to_printk
	};
	show_all(master, &o);
}
