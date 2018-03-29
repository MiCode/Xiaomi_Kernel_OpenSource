/*
 * drivers/dynamic_boost/dynamic_boost.c
 *
 * Copyright (C) 2010 MediaTek <www.MediaTek.com>
 * Copyright (C) 2018 XiaoMi, Inc.
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
 *
 *
 */

#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/rwsem.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/sysfs.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <mt_hotplug_strategy.h>
#include <mt_hotplug_strategy_internal.h>
#include "mt_cpufreq.h"
#include <linux/input.h>
#include <linux/workqueue.h>
#include "dynamic_boost.h"
#include <linux/notifier.h>
#include <linux/fb.h>

struct boost_state {
	int active;
	struct delayed_work work;
};

struct dynamic_boost {
	spinlock_t boost_lock;
	int last_req_mode;
	wait_queue_head_t wq;
	struct task_struct *thread;
	struct boost_state state[PRIO_DEFAULT];
	atomic_t event;
};

static struct dynamic_boost dboost;

struct dboost_input_handle {
	struct input_handle handle;
	int duration;
	int prio_mode;
};

#define MAX_CORES_NUMBER nr_cpu_ids
#define MAX_FREQUENCY 1
#define MAX_DURATION 10000

static ssize_t dynamic_boost_show(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t dynamic_boost_store(struct device *dev, struct device_attribute *attr, const char *buf,
	size_t n);
static struct device_attribute dynamic_boost_attr = __ATTR(dynamic_boost, 0750,
	dynamic_boost_show, dynamic_boost_store);

static void dboost_disable_work(struct work_struct *work)
{
	unsigned long flags;
	struct boost_state *state = container_of(work, struct boost_state, work.work);

	spin_lock_irqsave(&dboost.boost_lock, flags);
	if (state->active > 0)
		state->active--;
	spin_unlock_irqrestore(&dboost.boost_lock, flags);

	atomic_inc(&dboost.event);
	wake_up(&dboost.wq);
}

/*
 * Set up performance boost mode with requested duration
 * @duration: How long the user want this mode to keep. Specify with ms.
 * @mode: Request mode.
*/
int set_dynamic_boost(int duration, int prio_mode)
{
	unsigned long flags;
	struct boost_state *state;

	if (duration > MAX_DURATION ||
	    prio_mode < 0 || prio_mode > PRIO_DEFAULT)
		return -EINVAL;

	if (prio_mode == PRIO_DEFAULT || !duration)
		return 0;

	spin_lock_irqsave(&dboost.boost_lock, flags);

	state = &dboost.state[prio_mode];
	if (duration == ON)
		state->active++;
	else if (duration == OFF)
		state->active--;
	else if (!mod_delayed_work(system_wq, &state->work, msecs_to_jiffies(duration)))
		state->active++;
	spin_unlock_irqrestore(&dboost.boost_lock, flags);

	atomic_inc(&dboost.event);
	wake_up(&dboost.wq);
	return 0;
}
EXPORT_SYMBOL(set_dynamic_boost);

static int dboost_dvfs_hotplug_thread(void *ptr)
{
	int max_freq, cores_to_set_b, cores_to_set_l;
	unsigned long flags;

	set_user_nice(current, -10);

	while (!kthread_should_stop()) {
		int i, set_mode = PRIO_DEFAULT;

		spin_lock_irqsave(&dboost.boost_lock, flags);
		for (i = PRIO_DEFAULT - 1; i >= 0; i--) {
			if (dboost.state[i].active) {
				set_mode = i;
				break;
			}
		}
		spin_unlock_irqrestore(&dboost.boost_lock, flags);

		switch (set_mode) {
		case PRIO_MAX_CORES_MAX_FREQ:
			cores_to_set_b = num_possible_big_cpus();
			cores_to_set_l = num_possible_little_cpus();
			max_freq = MAX_FREQUENCY;
			break;
		case PRIO_MAX_CORES:
			cores_to_set_b = num_possible_big_cpus();
			cores_to_set_l = num_possible_little_cpus();
			max_freq = 0;
			break;
		case PRIO_FOUR_BIGS_MAX_FREQ:
			cores_to_set_b = 4;
			cores_to_set_l = 0;
			max_freq = MAX_FREQUENCY;
			break;
		case PRIO_FOUR_BIGS:
			cores_to_set_b = 4;
			cores_to_set_l = 0;
			max_freq = 0;
			break;
		case PRIO_TWO_BIGS_TWO_LITTLES_MAX_FREQ:
			cores_to_set_b = 2;
			cores_to_set_l = 2;
			max_freq = MAX_FREQUENCY;
			break;
		case PRIO_TWO_BIGS_TWO_LITTLES:
			cores_to_set_b = 2;
			cores_to_set_l = 2;
			max_freq = 0;
			break;
		case PRIO_FOUR_LITTLES_MAX_FREQ:
			cores_to_set_b = 0;
			cores_to_set_l = 4;
			max_freq = MAX_FREQUENCY;
			break;
		case PRIO_FOUR_LITTLES:
			cores_to_set_b = 0;
			cores_to_set_l = 4;
			max_freq = 0;
			break;
		case PRIO_TWO_BIGS_MAX_FREQ:
			cores_to_set_b = 2;
			cores_to_set_l = 0;
			max_freq = MAX_FREQUENCY;
			break;
		case PRIO_TWO_BIGS:
			cores_to_set_b = 2;
			cores_to_set_l = 0;
			max_freq = 0;
			break;
		case PRIO_ONE_BIG_ONE_LITTLE_MAX_FREQ:
			cores_to_set_b = 1;
			cores_to_set_l = 1;
			max_freq = MAX_FREQUENCY;
			break;
		case PRIO_ONE_BIG_ONE_LITTLE:
			cores_to_set_b = 1;
			cores_to_set_l = 1;
			max_freq = 0;
			break;
		case PRIO_ONE_BIG_MAX_FREQ:
			cores_to_set_b = 1;
			cores_to_set_l = 0;
			max_freq = MAX_FREQUENCY;
			break;
		case PRIO_ONE_BIG:
			cores_to_set_b = 1;
			cores_to_set_l = 0;
			max_freq = 0;
			break;
		case PRIO_TWO_LITTLES_MAX_FREQ:
			cores_to_set_b = 0;
			cores_to_set_l = 2;
			max_freq = MAX_FREQUENCY;
			break;
		case PRIO_TWO_LITTLES:
			cores_to_set_b = 0;
			cores_to_set_l = 2;
			max_freq = 0;
			break;
		case PRIO_RESET:
			for (i = PRIO_DEFAULT - 1; i >= 0; i--)
				dboost.state[i].active = 0;
		default:
			cores_to_set_b = 0;
			cores_to_set_l = 0;
			max_freq = 0;
			break;
		}

		interactive_boost_cpu(max_freq);
		if (!cores_to_set_l)
			cores_to_set_l = 1;
		hps_set_cpu_num_base(BASE_PERF_SERV, cores_to_set_l, cores_to_set_b);
		/* printk("dynamic boost: Mode=%d cpu_min_num_big=%d cpu_min_num_little=%d\n",
					set_mode, cores_to_set_b, cores_to_set_l);*/

		dboost.last_req_mode = set_mode;

		while (!atomic_read(&dboost.event))
			wait_event(dboost.wq, atomic_read(&dboost.event));

		atomic_dec(&dboost.event);
	}
	return 0;
}

static ssize_t dynamic_boost_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int i;

	i = snprintf(buf, PAGE_SIZE, "Mode: %d\n", dboost.last_req_mode);
	return i;
}

static ssize_t dynamic_boost_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t n)
{
	int now_req_duration = 0;
	int now_req_mode = 0;

	if ((n == 0) || (buf == NULL))
		return -EINVAL;
	if (sscanf(buf, "%d %d", &now_req_duration, &now_req_mode) != 2)
		return -EINVAL;
	if (now_req_mode < 0)
		return -EINVAL;

	set_dynamic_boost(now_req_duration, now_req_mode);

	return n;
}

static int dynamic_boost_probe(struct platform_device *dev)
{
	int ret_device_file = 0;

	ret_device_file = device_create_file(&(dev->dev), &dynamic_boost_attr);

	return ret_device_file;
}

struct platform_device dynamic_boost_device = {
	.name   = "dynamic_boost",
	.id        = -1,
};

int dboost_suspend(struct device *dev)
{
	unsigned long flags;
	int i;

	/* cancel all boost jobs if system suspend is requested */
	for (i = 0; i < ARRAY_SIZE(dboost.state); ++i) {
		cancel_delayed_work_sync(&dboost.state[i].work);
		spin_lock_irqsave(&dboost.boost_lock, flags);
		dboost.state[i].active = 0;
		spin_unlock_irqrestore(&dboost.boost_lock, flags);
	}
	atomic_inc(&dboost.event);
	wake_up(&dboost.wq);
	return 0;
}

int dboost_resume(struct device *dev)
{
	return 0;
}

static struct platform_driver dynamic_boost_driver = {
	.probe      = dynamic_boost_probe,
	.driver     = {
		.name = "dynamic_boost",
		.pm = &(const struct dev_pm_ops){
			.suspend = dboost_suspend,
			.resume = dboost_resume,
		},
	},
};

#ifdef CONFIG_TOUCH_BOOST
/******************************************************************************
 *                         Handle touch boost                                 *
 ******************************************************************************/
static void dboost_input_event(struct input_handle *handle, unsigned int type,
		unsigned int code, int value)
{
	struct dboost_input_handle *in = container_of(handle, struct dboost_input_handle, handle);

	if ((type == EV_KEY && code == BTN_TOUCH) ||
				(type == EV_ABS && code == ABS_MT_TRACKING_ID && value != 0))
		set_dynamic_boost(in->duration, in->prio_mode);
}

static int dboost_input_connect(struct input_handler *handler,
		struct input_dev *dev, const struct input_device_id *id)
{
	struct dboost_input_handle *in;
	int error;

	in = kzalloc(sizeof(struct dboost_input_handle), GFP_KERNEL);
	if (!in)
		return -ENOMEM;

	in->handle.dev = dev;
	in->handle.handler = handler;
	in->handle.name = "dynamic_boost";

	/* TODO: the following parameters should be configured through platform data */
	in->prio_mode = PRIO_TWO_BIGS_TWO_LITTLES_MAX_FREQ;
	in->duration = 150;

	error = input_register_handle(&in->handle);
	if (error)
		goto err2;

	error = input_open_device(&in->handle);
	if (error)
		goto err1;

	return 0;
err1:
	input_unregister_handle(&in->handle);
err2:
	kfree(&in->handle);
	return error;
}

static void dboost_input_disconnect(struct input_handle *handle)
{
	struct dboost_input_handle *in = container_of(handle, struct dboost_input_handle, handle);

	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(in);
}

static const struct input_device_id dboost_ids[] = {
	{ .driver_info = 1 },
	{ },
};

static struct input_handler dboost_input_handler = {
	.event		= dboost_input_event,
	.connect	= dboost_input_connect,
	.disconnect	= dboost_input_disconnect,
	.name		= "touch_boost_ond",
	.id_table	= dboost_ids,
};
#endif /* CONFIG_TOUCH_BOOST */

#ifdef CONFIG_SCREEN_ON_BOOST
static int dynamic_boost_lcm_fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int blank;

	blank = *(int *)evdata->data;

	/* skip if it's not a blank event */
	if (event != FB_EARLY_EVENT_BLANK)
		return 0;

	switch (blank) {
	/* LCM ON */
	case FB_BLANK_UNBLANK:
		set_dynamic_boost(500, PRIO_TWO_LITTLES);
		break;
	/* LCM OFF */
	case FB_BLANK_POWERDOWN:
	default:
		break;
	}

	return 0;
}

static struct notifier_block dynamic_boost_lcm_fb_notifier = {
	.notifier_call = dynamic_boost_lcm_fb_notifier_callback,
};
#endif /* CONFIG_SCREEN_ON_BOOST */

static int __init dynamic_boost_init(void)
{
	int ret = 0, i;

	ret = platform_device_register(&dynamic_boost_device);
	if (ret)
		return ret;
	ret = platform_driver_register(&dynamic_boost_driver);
	if (ret)
		return ret;

	spin_lock_init(&dboost.boost_lock);
	dboost.last_req_mode = PRIO_DEFAULT;
	atomic_set(&dboost.event, 0);

	for (i = 0; i < ARRAY_SIZE(dboost.state); ++i) {
		INIT_DELAYED_WORK(&dboost.state[i].work, dboost_disable_work);
#if 0 /* TODO: should 2 littles default always on? */
		if (i == PRIO_TWO_LITTLES)
			dboost.state[i].active = 1;
		else
#endif
			dboost.state[i].active = 0;
	}
	init_waitqueue_head(&dboost.wq);

	dboost.thread = kthread_run(dboost_dvfs_hotplug_thread, &dboost, "dynamic_boost");
	if (IS_ERR(dboost.thread))
		return -EINVAL;

#ifdef CONFIG_TOUCH_BOOST
	ret = input_register_handler(&dboost_input_handler);
	if (ret)
		return ret;
#endif
#ifdef CONFIG_SCREEN_ON_BOOST
	if (fb_register_client(&dynamic_boost_lcm_fb_notifier)) {
		pr_err("%s: register FB client failed!\n", __func__);
		return -EINVAL;
	}
#endif
	return 0;
}

late_initcall(dynamic_boost_init);

static void __exit dynamic_boost_exit(void)
{
#ifdef CONFIG_TOUCH_BOOST
	input_unregister_handler(&dboost_input_handler);
#endif
	kthread_stop(dboost.thread);
}

module_exit(dynamic_boost_exit);

MODULE_AUTHOR("MediaTek Inc.");
MODULE_DESCRIPTION("'dynamic boost' - provide performance boost");
MODULE_LICENSE("GPL");
