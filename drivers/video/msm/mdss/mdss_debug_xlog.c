/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/ktime.h>
#include <linux/debugfs.h>

#include "mdss.h"
#include "mdss_mdp.h"
#include "mdss_debug.h"

#define MDSS_XLOG_ENTRY	256
#define MDSS_XLOG_MAX_DATA 6
#define MDSS_XLOG_BUF_MAX 512

struct tlog {
	u64 tick;
	const char *name;
	u32 data[MDSS_XLOG_MAX_DATA];
	u32 data_cnt;
};

struct mdss_dbg_xlog {
	struct tlog logs[MDSS_XLOG_ENTRY];
	int first;
	int last;
	spinlock_t xlock;
} mdss_dbg_xlog;

static int mdss_xlog_dump_open(struct inode *inode, struct file *file)
{
	/* non-seekable */
	file->f_mode &= ~(FMODE_LSEEK | FMODE_PREAD | FMODE_PWRITE);
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t mdss_xlog_dump_read(struct file *file, char __user *buff,
		size_t count, loff_t *ppos)
{
	MDSS_XLOG_TOUT_HANDLER("mdp", "dsi0", "dsi1", "edp", "hdmi", "panic");
	return 0;
}

static const struct file_operations mdss_xlog_fops = {
	.open = mdss_xlog_dump_open,
	.read = mdss_xlog_dump_read,
};

int mdss_create_xlog_debug(struct mdss_debug_data *mdd)
{
	spin_lock_init(&mdss_dbg_xlog.xlock);

	mdd->logd.xlog = debugfs_create_dir("xlog", mdd->root);
	if (IS_ERR_OR_NULL(mdd->logd.xlog)) {
		pr_err("debugfs_create_dir fail, error %ld\n",
		       PTR_ERR(mdd->logd.xlog));
		mdd->logd.xlog = NULL;
		return -ENODEV;
	}
	debugfs_create_file("dump", 0644, mdd->logd.xlog, NULL,
						&mdss_xlog_fops);
	debugfs_create_bool("enable", 0644, mdd->logd.xlog,
			    &mdd->logd.xlog_enable);
	debugfs_create_bool("panic", 0644, mdd->logd.xlog,
			    &mdd->logd.panic_on_err);
	debugfs_create_bool("reg_dump", 0644, mdd->logd.xlog,
			    &mdd->logd.enable_reg_dump);
	return 0;
}

void mdss_xlog(const char *name, ...)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdss_debug_data *mdd = mdata->debug_inf.debug_data;
	unsigned long flags;
	int i, val = 0;
	va_list args;
	struct tlog *log;
	ktime_t time;

	if (!mdd->logd.xlog_enable)
		return;

	spin_lock_irqsave(&mdss_dbg_xlog.xlock, flags);

	time = ktime_get();

	log = &mdss_dbg_xlog.logs[mdss_dbg_xlog.first];
	log->tick = local_clock();
	log->name = name;
	log->data_cnt = 0;

	va_start(args, name);
	for (i = 0; i < MDSS_XLOG_MAX_DATA; i++) {

		val = va_arg(args, int);
		if (val == DATA_LIMITER)
			break;

		log->data[i] = val;
	}
	va_end(args);

	log->data_cnt = i;

	mdss_dbg_xlog.last = mdss_dbg_xlog.first;
	mdss_dbg_xlog.first++;
	mdss_dbg_xlog.first %= MDSS_XLOG_ENTRY;

	spin_unlock_irqrestore(&mdss_dbg_xlog.xlock, flags);
}

void mdss_xlog_dump(void)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdss_debug_data *mdd = mdata->debug_inf.debug_data;
	int i, n, d_cnt, off;
	unsigned long flags;
	unsigned long rem_nsec;
	struct tlog *log;
	char xlog_buf[MDSS_XLOG_BUF_MAX];

	if (!mdd->logd.xlog_enable)
		return;

	spin_lock_irqsave(&mdss_dbg_xlog.xlock, flags);
	i = mdss_dbg_xlog.first;
	for (n = 0; n < MDSS_XLOG_ENTRY; n++) {
		log = &mdss_dbg_xlog.logs[i];
		rem_nsec = do_div(log->tick, 1000000000);
		off = snprintf(xlog_buf, MDSS_XLOG_BUF_MAX,
				"%-32s => [%5llu.%06lu]: ", log->name,
					log->tick, rem_nsec / 1000);
		for (d_cnt = 0; d_cnt < log->data_cnt;) {
			off += snprintf((xlog_buf + off),
					(MDSS_XLOG_BUF_MAX - off),
					"%x ", log->data[d_cnt]);
			d_cnt++;
		}
		pr_err("%s\n", xlog_buf);

		i = (i + 1) % MDSS_XLOG_ENTRY;
	}
	spin_unlock_irqrestore(&mdss_dbg_xlog.xlock, flags);
}

void mdss_xlog_tout_handler(const char *name, ...)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdss_debug_data *mdd = mdata->debug_inf.debug_data;
	struct mdss_debug_base *blk_base, *tmp;
	int i, dead = 0;
	va_list args;
	char *blk_name = NULL;

	if (!mdd->logd.xlog_enable)
		return;

	va_start(args, name);
	for (i = 0; i < MDSS_XLOG_MAX_DATA; i++) {

		blk_name = va_arg(args, char*);
		if (IS_ERR_OR_NULL(blk_name))
			break;

		list_for_each_entry_safe(blk_base, tmp, &mdd->base_list, head) {

			if (blk_base->name &&
				!strcmp(blk_base->name, blk_name) &&
				mdd->logd.enable_reg_dump) {
				pr_info("\n%s  :   =========%s DUMP=========\n",
						__func__, blk_base->name);
				mdss_dump_reg(blk_base->base,
						blk_base->max_offset);
			}
		}
		if (!strcmp(blk_name, "panic"))
			dead = 1;
	}
	va_end(args);

	MDSS_XLOG(0xffff, 0xffff, 0xffff, 0xffff, 0xffff);
	mdss_xlog_dump();

	if (dead && mdd->logd.panic_on_err)
		panic(name);
}
