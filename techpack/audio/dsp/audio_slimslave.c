/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/miscdevice.h>
#include <sound/audio_slimslave.h>
#include <linux/slimbus/slimbus.h>
#include <linux/pm_runtime.h>

static struct slim_device *slim;
static int vote_count;
struct mutex suspend_lock;
bool suspend;

static int audio_slim_open(struct inode *inode, struct file *file)
{
	pr_debug("%s:\n", __func__);

	if (vote_count) {
		pr_debug("%s: unvote: vote_count=%d\n", __func__, vote_count);
		pm_runtime_mark_last_busy(slim->dev.parent);
		pm_runtime_put(slim->dev.parent);
		vote_count--;
	}
	return 0;
};

static int audio_slim_release(struct inode *inode, struct file *file)
{
	pr_debug("%s:\n", __func__);

	if (vote_count) {
		pr_debug("%s: unvote: vote_count=%d\n", __func__, vote_count);
		pm_runtime_mark_last_busy(slim->dev.parent);
		pm_runtime_put(slim->dev.parent);
		vote_count--;
	} else {
		pr_debug("%s: vote: vote_count=%d\n", __func__, vote_count);
		pm_runtime_get_sync(slim->dev.parent);
		vote_count++;
	}
	return 0;
};

static long audio_slim_ioctl(struct file *file, unsigned int cmd,
			     unsigned long u_arg)
{
	switch (cmd) {
	case AUDIO_SLIMSLAVE_VOTE:
		mutex_lock(&suspend_lock);
		if (!vote_count && !suspend) {
			pr_debug("%s:AUDIO_SLIMSLAVE_VOTE\n", __func__);
			pm_runtime_get_sync(slim->dev.parent);
			vote_count++;
		} else {
			pr_err("%s:Invalid vote: vote_count=%d suspend=%d\n",
				 __func__, vote_count, suspend);
		}
		mutex_unlock(&suspend_lock);
		break;
	case AUDIO_SLIMSLAVE_UNVOTE:
		mutex_lock(&suspend_lock);
		if (vote_count && !suspend) {
			pr_debug("%s:AUDIO_SLIMSLAVE_UNVOTE\n", __func__);
			pm_runtime_mark_last_busy(slim->dev.parent);
			pm_runtime_put(slim->dev.parent);
			vote_count--;
		} else {
			pr_err("%s:Invalid unvote: vote_count=%d suspend=%d\n",
				 __func__, vote_count, suspend);
		}
		mutex_unlock(&suspend_lock);
		break;
	default:
		pr_debug("%s: Invalid ioctl cmd: %d\n", __func__, cmd);
		break;
	}
	return 0;
}

static const struct file_operations audio_slimslave_fops = {
	.open =                 audio_slim_open,
	.unlocked_ioctl =       audio_slim_ioctl,
	.release =              audio_slim_release,
};

struct miscdevice audio_slimslave_misc = {
	.minor  =       MISC_DYNAMIC_MINOR,
	.name   =       AUDIO_SLIMSLAVE_IOCTL_NAME,
	.fops   =       &audio_slimslave_fops,
};

static int audio_slimslave_probe(struct slim_device *audio_slim)
{
	pr_debug("%s:\n", __func__);

	mutex_init(&suspend_lock);
	suspend = false;
	slim = audio_slim;
	misc_register(&audio_slimslave_misc);
	return 0;
}

static int audio_slimslave_remove(struct slim_device *audio_slim)
{
	pr_debug("%s:\n", __func__);

	misc_deregister(&audio_slimslave_misc);
	return 0;
}

static int audio_slimslave_resume(struct slim_device *audio_slim)
{
	pr_debug("%s:\n", __func__);

	mutex_lock(&suspend_lock);
	suspend = false;
	mutex_unlock(&suspend_lock);
	return 0;
}

static int audio_slimslave_suspend(struct slim_device *audio_slim,
				   pm_message_t pmesg)
{
	pr_debug("%s:\n", __func__);

	mutex_lock(&suspend_lock);
	suspend = true;
	mutex_unlock(&suspend_lock);
	return 0;
}

static const struct slim_device_id audio_slimslave_dt_match[] = {
	{"audio-slimslave", 0},
	{}
};

static struct slim_driver audio_slimslave_driver = {
	.driver = {
		.name = "audio-slimslave",
		.owner = THIS_MODULE,
	},
	.probe = audio_slimslave_probe,
	.remove = audio_slimslave_remove,
	.id_table = audio_slimslave_dt_match,
	.resume = audio_slimslave_resume,
	.suspend = audio_slimslave_suspend,
};

static int __init audio_slimslave_init(void)
{
	return slim_driver_register(&audio_slimslave_driver);
}
module_init(audio_slimslave_init);

static void __exit audio_slimslave_exit(void)
{

}
module_exit(audio_slimslave_exit);

/* Module information */
MODULE_DESCRIPTION("Audio side Slimbus slave driver");
MODULE_LICENSE("GPL v2");
