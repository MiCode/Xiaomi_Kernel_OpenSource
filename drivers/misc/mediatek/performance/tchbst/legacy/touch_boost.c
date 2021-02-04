/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include "../perf_ioctl/perf_ioctl.h"
#include "perfmgr.h"

#define TAG "TOUCH_BOOST"

static struct mutex notify_lock;
static void notify_ui_update_timeout(void);
static void notify_render_aware_timeout(void);
static DECLARE_WORK(mt_ui_update_timeout_work,
	(void *) notify_ui_update_timeout);
static DECLARE_WORK(mt_render_aware_timeout_work,
	(void *) notify_render_aware_timeout);
static struct workqueue_struct *wq;
static struct hrtimer hrt, hrt1;

static int touch_boost_debug;
static int render_aware_valid;
static int is_touch_boost;
static int is_render_aware_boost;
static int tboost_core;
static int tboost_freq;

#define UI_UPDATE_DURATION_MS 300UL
#define RENDER_AWARE_DURATION_MS 3000UL

/*--------------------TIMER------------------------*/
static void enable_ui_update_timer(void)
{
	ktime_t ktime;

	ktime = ktime_set(0,
		(unsigned long)(NSEC_PER_MSEC * UI_UPDATE_DURATION_MS));
	hrtimer_start(&hrt, ktime, HRTIMER_MODE_REL);
}

static enum hrtimer_restart mt_ui_update_timeout(struct hrtimer *timer)
{
	if (wq)
		queue_work(wq, &mt_ui_update_timeout_work);

	return HRTIMER_NORESTART;
}

static void enable_render_aware_timer(void)
{
	ktime_t ktime;

	ktime = ktime_set(0,
		(unsigned long)(NSEC_PER_MSEC * RENDER_AWARE_DURATION_MS));
	hrtimer_start(&hrt1, ktime, HRTIMER_MODE_REL);
}

static void disable_render_aware_timer(void)
{
	hrtimer_cancel(&hrt1);
}

static enum hrtimer_restart mt_render_aware_timeout(struct hrtimer *timer)
{
	if (wq)
		queue_work(wq, &mt_render_aware_timeout_work);

	return HRTIMER_NORESTART;
}

/*--------------------FRAME HINT OP------------------------*/
static void notify_touch(int action)
{
	/* lock is mandatory*/
	WARN_ON(!mutex_is_locked(&notify_lock));

	/*action 1: touch down 0: touch up*/
	if (action == 1) {
		render_aware_valid = 1;
		is_touch_boost = 1;
		disable_render_aware_timer();
		pr_debug(TAG"enable UI boost, touch down, is_touch_boost:%d\n",
			is_touch_boost);
		perfmgr_boost(is_render_aware_boost | is_touch_boost,
			tboost_core, tboost_freq);
	} else if (action == 0) {
		enable_render_aware_timer();
		is_touch_boost = 0;
		pr_debug(TAG"enable UI boost, touch up, is_touch_boost:%d\n",
			is_touch_boost);
		perfmgr_boost(is_render_aware_boost | is_touch_boost,
			tboost_core, tboost_freq);
	}
}

static void notify_ui_update_timeout(void)
{
	mutex_lock(&notify_lock);
	if (!is_touch_boost)
		render_aware_valid = 0;
	is_render_aware_boost = 0;
	pr_debug(TAG"enable UI boost, frame noupdate, is_render_aware:%d\n",
		is_render_aware_boost);
	perfmgr_boost(is_render_aware_boost | is_touch_boost,
		tboost_core, tboost_freq);

	mutex_unlock(&notify_lock);

}

static void notify_render_aware_timeout(void)
{
	mutex_lock(&notify_lock);
	render_aware_valid = 0;
	is_render_aware_boost = 0;
	pr_debug(TAG"enable UI boost, render aware timeout, is_render_aware:%d\n",
		is_render_aware_boost);
	perfmgr_boost(is_render_aware_boost | is_touch_boost,
		tboost_core, tboost_freq);

	mutex_unlock(&notify_lock);
}

void notify_frame_complete(long frame_time)
{
	/* lock is mandatory*/
	WARN_ON(!mutex_is_locked(&notify_lock));

	if (!render_aware_valid)
		return;

	enable_ui_update_timer();
	is_render_aware_boost = 1;
	perfmgr_boost(is_render_aware_boost | is_touch_boost,
		tboost_core, tboost_freq);
}

/*--------------------DEV OP------------------------*/
static ssize_t device_write(struct file *filp, const char *ubuf,
		size_t cnt, loff_t *data)
{
	char buf[32], cmd[32];
	int arg;

	arg = 0;

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, ubuf, cnt))
		return -EFAULT;
	buf[cnt] = '\0';

	if (sscanf(buf, "%31s %d", cmd, &arg) != 2)
		return -EFAULT;

	if (strncmp(cmd, "debug", 5) == 0) {
		mutex_lock(&notify_lock);
		touch_boost_debug = arg;
		mutex_unlock(&notify_lock);
	} else if (strncmp(cmd, "core", 4) == 0) {
		tboost_core = arg;
	} else if (strncmp(cmd, "freq", 4) == 0) {
		tboost_freq = arg;
	}
	return cnt;
}

static int device_show(struct seq_file *m, void *v)
{
	seq_printf(m, "debug:\t%d\n", touch_boost_debug);
	seq_printf(m, "touch:\t%d\n", is_touch_boost);
	seq_printf(m, "render:\t%d\n", is_render_aware_boost);
	seq_printf(m, "core:\t%d\n", tboost_core);
	seq_printf(m, "freq:\t%d\n", tboost_freq);
	return 0;
}

static int device_open(struct inode *inode, struct file *file)
{
	return single_open(file, device_show, inode->i_private);
}

long touch_boost_ioctl(unsigned int cmd, unsigned long arg)
{
	ssize_t ret = 0;

	mutex_lock(&notify_lock);
	if (touch_boost_debug)
		goto ret_ioctl;

	switch (cmd) {
	/*receive touch info*/
	case FPSGO_TOUCH:
		notify_touch(arg);
		break;

	/*receive frame_time info*/
	case FPSGO_QUEUE:
		notify_frame_complete(arg);
		break;

	default:
		pr_debug(TAG "unknown cmd %u\n", cmd);
		ret = -1;
		goto ret_ioctl;
	}

ret_ioctl:
	mutex_unlock(&notify_lock);
	return ret;
}

static const struct file_operations Fops = {
	.open = device_open,
	.write = device_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/*--------------------INIT------------------------*/
int init_touch_boost(void)
{
	struct proc_dir_entry *pe;
	int ret_val = 0;


	pr_debug(TAG"Start to init touch_boost driver\n");

	mutex_init(&notify_lock);

	tboost_core = perfmgr_get_target_core();
	tboost_freq = perfmgr_get_target_freq();

	wq = create_singlethread_workqueue("mt_touch_boost_work");
	if (!wq) {
		pr_debug(TAG"work create fail\n");
		return -ENOMEM;
	}

	hrtimer_init(&hrt, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hrt.function = &mt_ui_update_timeout;
	hrtimer_init(&hrt1, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hrt1.function = &mt_render_aware_timeout;

	pe = proc_create("perfmgr/touch_boost", 0664, NULL, &Fops);
	if (!pe) {
		pr_debug(TAG"%s failed with %d\n",
				"Creating file node ",
				ret_val);
		ret_val = -ENOMEM;
		goto out_chrdev;
	}

	pr_debug(TAG"init touch_boost driver done\n");

	return 0;

out_chrdev:
	destroy_workqueue(wq);
	return ret_val;
}

