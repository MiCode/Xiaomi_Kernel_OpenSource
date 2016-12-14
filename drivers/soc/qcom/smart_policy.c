/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
/*************************************************************************
    > File Name: smart_policy.c
    > Author: aiquny
    > Mail: aiquny@qti.qualcomm.com
    > Created Time: Tue 10 Nov 2015 02:26:15 PM CST
 ************************************************************************/
#define DEBUG
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/fb.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

/* sm_state: showed the device smart state.
 *	  0 means normal state, device busy
 *	  1 means power save state, device suspended before and not display on yet
 * you can check this via /proc/sm_state
 */
static int sm_state;

#if defined(CONFIG_FB) || (defined(CONFIG_FB_MODULE))
/* This callback gets called when something important happens inside a
 * framebuffer driver. We're looking if that important event is blanking,
 * or suspend
 */
static int fb_notifier_callback(struct notifier_block *self,
				unsigned long action, void *data)
{
	struct fb_event *event = data;
	struct fb_info *info = event->info;

	dev_dbg(info->dev, "%s action:%lu\n", __func__, action);

	/* If we aren't interested in this action, skip it immediately ... */
	switch (action) {
	case FB_EVENT_BLANK:
	case FB_EARLY_EVENT_BLANK:
		sm_state = 0;
		break;
	default:
		return 0;
	}

	return 0;
}

static struct notifier_block fbcon_event_notifier = {
	.notifier_call = fb_notifier_callback,
};

static int sm_register_fb(void)
{
	return fb_register_client(&fbcon_event_notifier);
}

static void sm_unregister_fb(void)
{
	fb_unregister_client(&fbcon_event_notifier);
}
#else
static int sm_register_fb(void)
{
	return 0;
}

static inline void sm_unregister_fb(void)
{
}
#endif /* CONFIG_FB */

static int sm_policy_suspend(struct device *dev)
{
	dev_dbg(dev, "%s\n", __func__);
	dev_err(dev, "%s\n", __func__);
	sm_state = 1;
	return 0;
}

static int sm_policy_resume(struct device *dev)
{
	dev_dbg(dev, "%s\n", __func__);
	return 0;
}

static int sm_policy_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "%s\n", __func__);
	return 0;
}

static int sm_policy_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "%s\n", __func__);
	return 0;
}

static const struct dev_pm_ops sm_policy_dev_pm_ops = {
	.suspend = sm_policy_suspend,
	.resume = sm_policy_resume,
	.runtime_suspend = sm_policy_runtime_suspend,
	.runtime_resume = sm_policy_runtime_resume,
};

static int sm_policy_probe(struct platform_device *pdev)
{
	dev_err(&pdev->dev, "%s\n", __func__);
	sm_register_fb();
	return 0;
}

static int sm_policy_remove(struct platform_device *pdev)
{
	dev_err(&pdev->dev, "%s\n", __func__);
	sm_unregister_fb();
	return 0;
}

static struct platform_driver sm_policy_driver = {
	.driver = {
		   .name = "sm_policy",
		   .owner = THIS_MODULE,
		   .pm = &sm_policy_dev_pm_ops,
		   },
	.probe = sm_policy_probe,
	.remove = sm_policy_remove,
};

static void sm_policy_release(struct device *dev)
{
	return;
}

static struct platform_device sm_policy_device = {
	.name = "sm_policy",
	.id = -1,
	.dev = {
		.release = sm_policy_release,
		}
};

static int sm_stat_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", sm_state);
	return 0;
}

static int sm_stat_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, sm_stat_proc_show, NULL);
}

static const struct file_operations sm_state_proc_fops = {
	.open = sm_stat_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init proc_sm_state_init(void)
{
	proc_create("sm_state", 0, NULL, &sm_state_proc_fops);
	return 0;
}

static int __init sm_policy_init(void)
{
	proc_sm_state_init();
	platform_device_register(&sm_policy_device);
	return platform_driver_register(&sm_policy_driver);
}

module_init(sm_policy_init);

static void __exit sm_policy_exit(void)
{
	platform_driver_unregister(&sm_policy_driver);
	platform_device_unregister(&sm_policy_device);
}

module_exit(sm_policy_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Yu Aiqun <aiquny@qti.qualcomm.com>");
MODULE_DESCRIPTION("Msm Smart Policy Driver");
