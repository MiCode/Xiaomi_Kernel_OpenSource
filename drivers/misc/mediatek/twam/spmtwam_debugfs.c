// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/* spmtwam test driver
 * 1. setup twam speed mode (optional, default high)
 *    echo [0|1] > /sys/kernel/debug/spmtwam/speed_mode
 * 2. setup signal [0-3], id [0-31], and monitor type [0-3] for each channel
 *    echo [0-3]  > /sys/kernel/debug/spmtwam/ch0/signal
 *    echo [0-31] > /sys/kernel/debug/spmtwam/ch0/id
 *    echo [0-3]  > /sys/kernel/debug/spmtwam/ch0/monitor_type
 * 3. start monitor (monitor up to 4 channels at the same time)
 *    echo 1 > /sys/kernel/debug/spmtwam/state
 * 4. stop monitor (will clear all configs)
 *    echo 0 > /sys/kernel/debug/spmtwam/state
 * 5. check current config state
 *    cat /sys/kernel/debug/spmtwam_state
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include "spmtwam.h"

struct spmtwam_local_cfg {
	bool enable;
	struct spmtwam_cfg cfg;
};

static struct spmtwam_local_cfg cur;

static void setup_default_cfg(struct spmtwam_local_cfg *c)
{
	int i;
	struct spmtwam_cfg *cfg = &c->cfg;

	c->enable = false;
	cfg->spmtwam_speed_mode = DEFAULT_SPEED_MODE;
	/* spmtwam_window_len will be updated according to speed mode */
	cfg->spmtwam_window_len = 0;
	for (i = 0 ; i < 4; i++) {
		cfg->ch[i].signal = 0;
		cfg->ch[i].id = 0;
		cfg->ch[i].montype = DEFAULT_MONTYPE;
	}
}

static void spmtwam_handler(struct spmtwam_result *r)
{
	int i;
	struct spmtwam_cfg *cfg = &r->cfg;

	for (i = 0; i < 4; i++) {
		pr_info("spmtwam (sel%d:%d) ratio: %u/1000 %s, %u\n",
			cfg->ch[i].signal, cfg->ch[i].id,
			cfg->spmtwam_speed_mode ?
				GET_EVENT_RATIO_SPEED(r->value[i]) :
				GET_EVENT_RATIO_NORMAL(r->value[i]),
			cfg->spmtwam_speed_mode ? "high" : "normal",
			r->value[i]);
	}
}

static void spmtwam_profile_enable(bool enable)
{
	int ret = 0;

	/* verify local spmtwam config */
	if (!enable)
		setup_default_cfg(&cur);

	ret = spmtwam_monitor(enable, &cur.cfg, spmtwam_handler);
	if (ret == 0)
		cur.enable = enable;
}


static char dbgbuf[1024] = {0};
#define log2buf(p, s, fmt, args...) \
	(p += scnprintf(p, sizeof(s) - strlen(s), fmt, ##args))
#undef log
#define log(fmt, args...)   log2buf(p, dbgbuf, fmt, ##args)

static ssize_t dbg_read(struct file *filp, char __user *userbuf,
	size_t count, loff_t *f_pos)
{
	int i, len = 0;
	char *p = dbgbuf;
	struct spmtwam_cfg *cfg = &cur.cfg;

	p[0] = '\0';

	log("spmtwam state:\n");
	log("enable %d\n", cur.enable ? 1 : 0);
	log("speed_mode %d (0: low, 1: high)\n",
		cfg->spmtwam_speed_mode ? 1 : 0);
	log("window_len %u (0x%x)\n",
		cfg->spmtwam_window_len, cfg->spmtwam_window_len);
	for (i = 0; i < 4; i++)
		log("ch%d: signal %u id %u montype %u (%s)\n",
			i, cfg->ch[i].signal, cfg->ch[i].id, cfg->ch[i].montype,
			cfg->ch[i].montype == 0 ? "rising" :
			cfg->ch[i].montype == 1 ? "falling" :
			cfg->ch[i].montype == 2 ? "high level" :
			cfg->ch[i].montype == 3 ? "high level" : "unknown");

	len = p - dbgbuf;

	return simple_read_from_buffer(userbuf, count, f_pos, dbgbuf, len);
}

static ssize_t dbg_write(struct file *fp, const char __user *userbuf,
	size_t count, loff_t *f_pos)
{
	char cmd[16];
	unsigned int en = 0;

	count = min(count, sizeof(cmd)-1);

	if (copy_from_user(cmd, userbuf, count))
		return -EFAULT;

	cmd[count] = '\0';

	if (kstrtou32(cmd, 0, &en) == 0)
		spmtwam_profile_enable(en ? true : false);

	return count;
}

const static struct file_operations dbg_fops = {
	.owner = THIS_MODULE,
	.read = dbg_read,
	.write = dbg_write,
};

static struct dentry *spmtwam_droot;

static int __init spmtwam_debugfs_init(void)
{
	int i;
	struct dentry *ch[4];
	struct spmtwam_cfg *cfg = &cur.cfg;

	/* setup local default spmtwam config*/
	setup_default_cfg(&cur);

	/* create debugfs for this test driver */
	spmtwam_droot = debugfs_create_dir("spmtwam", NULL);

	if (spmtwam_droot) {
		debugfs_create_file("state", 0644, spmtwam_droot,
			(void *) &(cur.enable), &dbg_fops);
		debugfs_create_bool("speed_mode", 0644, spmtwam_droot,
			(void *) &(cfg->spmtwam_speed_mode));
		debugfs_create_u32("window_len", 0644, spmtwam_droot,
			(void *) &(cfg->spmtwam_window_len));
		ch[0] =	debugfs_create_dir("ch0", spmtwam_droot);
		ch[1] =	debugfs_create_dir("ch1", spmtwam_droot);
		ch[2] =	debugfs_create_dir("ch2", spmtwam_droot);
		ch[3] =	debugfs_create_dir("ch3", spmtwam_droot);
		for (i = 0 ; i < 4; i++) {
			if (ch[i]) {
				debugfs_create_u32("signal", 0644, ch[i],
					(void *)&(cfg->ch[i].signal));
				debugfs_create_u32("id", 0644, ch[i],
					(void *)&(cfg->ch[i].id));
				debugfs_create_u32("montype", 0644, ch[i],
					(void *)&(cfg->ch[i].montype));
			}
		}
	}
	return 0;
}

module_init(spmtwam_debugfs_init);

static void __exit spmtwam_debugfs_exit(void)
{
	spmtwam_profile_enable(false);
	debugfs_remove(spmtwam_droot);
}

module_exit(spmtwam_debugfs_exit);

MODULE_DESCRIPTION("Mediatek MT67XX spmtwam debugfs driver");
MODULE_AUTHOR("JM Lai <jm.lai@mediatek.com>");
MODULE_LICENSE("GPL");
