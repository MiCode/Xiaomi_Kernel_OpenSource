/*
 * drivers/misc/throughput.c
 *
 * Copyright (C) 2012-2013, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/kthread.h>
#include <linux/ktime.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/throughput_ioctl.h>
#include <linux/module.h>
#include <linux/nvhost.h>
#include <mach/dc.h>

#define DEFAULT_SYNC_RATE 60000 /* 60 Hz */

static unsigned int target_frame_time;
static ktime_t last_flip;
static spinlock_t lock;

#define EMA_PERIOD  8

static int frame_time_sum_init = 1;
static long frame_time_sum; /* used for fps EMA */

static struct work_struct work;
static int throughput_hint;

static int sync_rate;
static int throughput_active_app_count;

static void set_throughput_hint(struct work_struct *work)
{
	/* notify throughput hint clients here */
	nvhost_scale3d_set_throughput_hint(throughput_hint);
}

static void throughput_flip_callback(void)
{
	long timediff;
	ktime_t now;

	now = ktime_get();

	if (last_flip.tv64 != 0) {
		timediff = (long) ktime_us_delta(now, last_flip);

		if (timediff <= 0) {
			pr_warn("%s: flips %lld nsec apart\n",
				__func__, now.tv64 - last_flip.tv64);
			last_flip = now;
			return;
		}

		throughput_hint =
			((int) target_frame_time * 1000) / timediff;

		/* only deliver throughput hints when a single app is active */
		if (throughput_active_app_count == 1 && !work_pending(&work))
			schedule_work(&work);

		if (frame_time_sum_init) {
			frame_time_sum = timediff * EMA_PERIOD;
			frame_time_sum_init = 0;
		} else {
			int t = (frame_time_sum / EMA_PERIOD) *
				(EMA_PERIOD - 1);
			frame_time_sum = t + timediff;
		}
	}

	last_flip = now;
}

static void reset_target_frame_time(void)
{
	if (sync_rate == 0) {
		sync_rate = tegra_dc_get_panel_sync_rate();

		if (sync_rate == 0)
			sync_rate = DEFAULT_SYNC_RATE;
	}

	target_frame_time = (unsigned int) (1000000000 / sync_rate);

	pr_debug("%s: panel sync rate %d, target frame time %u\n",
		__func__, sync_rate, target_frame_time);
}

static int throughput_open(struct inode *inode, struct file *file)
{
	spin_lock(&lock);

	throughput_active_app_count++;
	frame_time_sum_init = 1;

	spin_unlock(&lock);


	pr_debug("throughput_open node %p file %p\n", inode, file);

	return 0;
}

static int throughput_release(struct inode *inode, struct file *file)
{
	spin_lock(&lock);

	throughput_active_app_count--;
	frame_time_sum_init = 1;

	if (throughput_active_app_count == 1)
		reset_target_frame_time();

	spin_unlock(&lock);

	pr_debug("throughput_release node %p file %p\n", inode, file);

	return 0;
}

static int throughput_set_target_fps(unsigned long arg)
{
	pr_debug("%s: target fps %lu requested\n", __func__, arg);

	if (throughput_active_app_count != 1) {
		pr_debug("%s: %d active apps, disabling fps usage\n",
			__func__, throughput_active_app_count);
		return 0;
	}

	if (arg == 0)
		reset_target_frame_time();
	else
		target_frame_time = (unsigned int) (1000000 / arg);

	return 0;
}

static long
throughput_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int err = 0;

	if ((_IOC_TYPE(cmd) != TEGRA_THROUGHPUT_MAGIC) ||
		(_IOC_NR(cmd) == 0) ||
		(_IOC_NR(cmd) > TEGRA_THROUGHPUT_IOCTL_MAXNR))
		return -EFAULT;

	switch (cmd) {
	case TEGRA_THROUGHPUT_IOCTL_TARGET_FPS:
		pr_debug("%s: TEGRA_THROUGHPUT_IOCTL_TARGET_FPS %lu\n",
			__func__, arg);
		err = throughput_set_target_fps(arg);
		break;

	default:
		err = -ENOTTY;
	}

	return err;
}

static const struct file_operations throughput_user_fops = {
	.owner			= THIS_MODULE,
	.open			= throughput_open,
	.release		= throughput_release,
	.unlocked_ioctl		= throughput_ioctl,
};

#define TEGRA_THROUGHPUT_MINOR 1

static struct miscdevice throughput_miscdev = {
	.minor = TEGRA_THROUGHPUT_MINOR,
	.name  = "tegra-throughput",
	.fops  = &throughput_user_fops,
	.mode  = 0666,
};

static ssize_t show_fps(struct kobject *kobj,
	struct attribute *attr, char *buf)
{
	int frame_time_avg;
	ktime_t now;
	long timediff;
	int fps = 0;

	if (frame_time_sum_init)
		goto DONE;

	now = ktime_get();
	timediff = (long) ktime_us_delta(now, last_flip);
	if (timediff > 1000000)
		goto DONE;

	frame_time_avg = frame_time_sum / EMA_PERIOD;
	fps = frame_time_avg > 0 ? 1000000 / frame_time_avg : 0;

DONE:
	return sprintf(buf, "%d\n", fps);
}

static struct global_attr fps_attr = __ATTR(fps, 0444,
		show_fps, NULL);

int __init throughput_init_miscdev(void)
{
	int ret;

	pr_debug("%s: initializing\n", __func__);

	spin_lock_init(&lock);
	INIT_WORK(&work, set_throughput_hint);

	ret = misc_register(&throughput_miscdev);
	if (ret) {
		pr_err("can\'t reigster throughput miscdev"
		       " (minor %d err %d)\n", TEGRA_THROUGHPUT_MINOR, ret);
		return ret;
	}

	ret = sysfs_create_file(&throughput_miscdev.this_device->kobj,
		&fps_attr.attr);
	if (ret)
		pr_err("%s: error %d creating sysfs node\n", __func__, ret);

	tegra_dc_set_flip_callback(throughput_flip_callback);

	return 0;
}

module_init(throughput_init_miscdev);

void __exit throughput_exit_miscdev(void)
{
	pr_debug("%s: exiting\n", __func__);

	tegra_dc_unset_flip_callback();

	cancel_work_sync(&work);

	sysfs_remove_file(&throughput_miscdev.this_device->kobj, &fps_attr.attr);

	misc_deregister(&throughput_miscdev);
}

module_exit(throughput_exit_miscdev);

