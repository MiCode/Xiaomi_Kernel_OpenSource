// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Spreadtrum Communications Inc.
 *
 * Used to keep time in sync with AP and other systems.
 */

#include <clocksource/arm_arch_timer.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/timekeeping.h>
#include <linux/timekeeper_internal.h>
#include <linux/uaccess.h>

#include <linux/soc/sprd/sprd_systimer.h>
#include <linux/soc/sprd/sprd_time_sync.h>

#define SPRD_TIMESYNC_MSG_LENGTH	(100)

static struct class *sprd_time_sync_class;
static struct sprd_time_sync_device *tcd_dev;

enum SPRD_TIMESYNC_MSG_TYPE {
	SPRD_TIMESYNC_MSG_MONOTIME,
	SPRD_TIMESYNC_MSG_BOOTTIME,
	SPRD_TIMESYNC_MSG_REALTIME,
	SPRD_TIMESYNC_MSG_TS_CNT,
	SPRD_TIMESYNC_MSG_SYSFRT_CNT,
	SPRD_TIMESYNC_MSG_TZ_MINUTESWEST,
	SPRD_TIMESYNC_MSG_TZ_DSTTIME,
	SPRD_TIMESYNC_MSG_NUM,
};

static void sprd_send_time_event_msg(struct kobject *);

static int sprd_time_sync_open(struct inode *inode, struct file *filp)
{
	struct sprd_time_sync_device *sprd_time_sync;

	sprd_time_sync = container_of(inode->i_cdev,
				      struct sprd_time_sync_device, cdev);

	filp->private_data = sprd_time_sync;

	dev_dbg(sprd_time_sync->dev, "open\n");

	return 0;
}

static int sprd_time_sync_release(struct inode *inode, struct file *filp)
{
	struct sprd_time_sync_device *sprd_time_sync;

	sprd_time_sync = container_of(inode->i_cdev,
				      struct sprd_time_sync_device, cdev);

	dev_dbg(sprd_time_sync->dev, "release\n");

	return 0;
}

static ssize_t sprd_time_sync_read(struct file *filp,
				   char __user *buf, size_t count, loff_t *ppos)
{
	struct sprd_time_sync_init_data read_uevent_time;
	struct timezone tz;

	memset(&tz, 0, sizeof(struct timezone));
	memcpy(&tz, &sys_tz, sizeof(struct timezone));

	memset(&read_uevent_time, 0, sizeof(struct sprd_time_sync_init_data));

	if (!tcd_dev || !tcd_dev->init) {
		return 0;
	}

	read_uevent_time.t_monotime_ns = ktime_get_mono_fast_ns();
	read_uevent_time.t_boottime_ns = ktime_get_boot_fast_ns();
	read_uevent_time.t_realtime_s = ktime_get_real_seconds();
	read_uevent_time.t_ts_cnt = arch_timer_read_counter();
	read_uevent_time.t_sysfrt_cnt = sprd_systimer_read();
	read_uevent_time.tz_minuteswest = tz.tz_minuteswest;
	read_uevent_time.tz_dsttime = tz.tz_dsttime;

	if (copy_to_user((struct sprd_time_sync_init_data *)buf,
			 &read_uevent_time,
			 sizeof(struct sprd_time_sync_init_data))) {
		return -1;
	}

	return 0;
}

static const struct file_operations sprd_time_sync_fops = {
	.owner = THIS_MODULE,
	.open = sprd_time_sync_open,
	.read = sprd_time_sync_read,
	.release = sprd_time_sync_release,
	.llseek = default_llseek,
};

static int sprd_time_sync_parse_dt(struct sprd_time_sync_init_data **init,
		struct device *dev)
{
	struct sprd_time_sync_init_data *pdata = NULL;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	pdata->name = "sprd_time_sync";

	*init = pdata;

	return 0;
}

static inline void sprd_time_sync_destroy_pdata(struct sprd_time_sync_init_data **init)
{
	struct sprd_time_sync_init_data *pdata = *init;

	pdata = NULL;
}

int sprd_send_ap_time(void)
{
	struct timezone tz;

	memset(&tz, 0, sizeof(struct timezone));
	memcpy(&tz, &sys_tz, sizeof(struct timezone));

	if (!tcd_dev || !tcd_dev->init) {
		pr_err("device is not exist \n");
		return -1;
	}

	tcd_dev->init->t_monotime_ns = ktime_get_mono_fast_ns();
	tcd_dev->init->t_boottime_ns = ktime_get_boot_fast_ns();
	tcd_dev->init->t_realtime_s = ktime_get_real_seconds();
	tcd_dev->init->t_ts_cnt = arch_timer_read_counter();
	tcd_dev->init->t_sysfrt_cnt = sprd_systimer_read();
	tcd_dev->init->tz_minuteswest = tz.tz_minuteswest;
	tcd_dev->init->tz_dsttime = tz.tz_dsttime;
	sprd_send_time_event_msg(&tcd_dev->dev->kobj);

	return 0;
}
EXPORT_SYMBOL(sprd_send_ap_time);

static void sprd_send_time_event_msg(struct kobject *kobj)
{
	char *msg[SPRD_TIMESYNC_MSG_NUM + 1];

	msg[0] = kzalloc((SPRD_TIMESYNC_MSG_NUM + 1) * SPRD_TIMESYNC_MSG_LENGTH, GFP_KERNEL);

	if (!*msg)
		return;

	if (!tcd_dev || !tcd_dev->init || !kobj)
		return;

	msg[SPRD_TIMESYNC_MSG_MONOTIME] =
		*msg + SPRD_TIMESYNC_MSG_MONOTIME * SPRD_TIMESYNC_MSG_LENGTH;
	msg[SPRD_TIMESYNC_MSG_BOOTTIME] =
		*msg + SPRD_TIMESYNC_MSG_BOOTTIME * SPRD_TIMESYNC_MSG_LENGTH;
	msg[SPRD_TIMESYNC_MSG_REALTIME] =
		*msg + SPRD_TIMESYNC_MSG_REALTIME * SPRD_TIMESYNC_MSG_LENGTH;
	msg[SPRD_TIMESYNC_MSG_TS_CNT] =
		*msg + SPRD_TIMESYNC_MSG_TS_CNT * SPRD_TIMESYNC_MSG_LENGTH;
	msg[SPRD_TIMESYNC_MSG_SYSFRT_CNT] =
		*msg + SPRD_TIMESYNC_MSG_SYSFRT_CNT * SPRD_TIMESYNC_MSG_LENGTH;
	msg[SPRD_TIMESYNC_MSG_TZ_MINUTESWEST] =
		*msg + SPRD_TIMESYNC_MSG_TZ_MINUTESWEST * SPRD_TIMESYNC_MSG_LENGTH;
	msg[SPRD_TIMESYNC_MSG_TZ_DSTTIME] =
		*msg + SPRD_TIMESYNC_MSG_TZ_DSTTIME * SPRD_TIMESYNC_MSG_LENGTH;
	msg[SPRD_TIMESYNC_MSG_NUM] = NULL;

	snprintf(msg[SPRD_TIMESYNC_MSG_MONOTIME],
		 SPRD_TIMESYNC_MSG_LENGTH, "monotime_ns=%lld", tcd_dev->init->t_monotime_ns);
	snprintf(msg[SPRD_TIMESYNC_MSG_BOOTTIME],
		 SPRD_TIMESYNC_MSG_LENGTH, "boottime_ns=%lld", tcd_dev->init->t_boottime_ns);
	snprintf(msg[SPRD_TIMESYNC_MSG_REALTIME],
		 SPRD_TIMESYNC_MSG_LENGTH, "realtime_s=%lld", tcd_dev->init->t_realtime_s);
	snprintf(msg[SPRD_TIMESYNC_MSG_TS_CNT],
		 SPRD_TIMESYNC_MSG_LENGTH, "ts_cnt=%lld", tcd_dev->init->t_ts_cnt);
	snprintf(msg[SPRD_TIMESYNC_MSG_SYSFRT_CNT],
		 SPRD_TIMESYNC_MSG_LENGTH, "sysfrt_cnt=%lld", tcd_dev->init->t_sysfrt_cnt);
	snprintf(msg[SPRD_TIMESYNC_MSG_TZ_MINUTESWEST],
		 SPRD_TIMESYNC_MSG_LENGTH, "minuteswest=%d", tcd_dev->init->tz_minuteswest);
	snprintf(msg[SPRD_TIMESYNC_MSG_TZ_DSTTIME],
		 SPRD_TIMESYNC_MSG_LENGTH, "dsttime=%d", tcd_dev->init->tz_dsttime);

	kobject_uevent_env(kobj, KOBJ_CHANGE, msg);

	dev_info(tcd_dev->dev, "send %s %s %s %s %s %s %s to userspace\n",
		 msg[SPRD_TIMESYNC_MSG_MONOTIME],
		 msg[SPRD_TIMESYNC_MSG_BOOTTIME],
		 msg[SPRD_TIMESYNC_MSG_REALTIME],
		 msg[SPRD_TIMESYNC_MSG_TS_CNT],
		 msg[SPRD_TIMESYNC_MSG_SYSFRT_CNT],
		 msg[SPRD_TIMESYNC_MSG_TZ_MINUTESWEST],
		 msg[SPRD_TIMESYNC_MSG_TZ_DSTTIME]);

	kfree(*msg);
}

static int sprd_time_sync_remove(struct platform_device *pdev)
{
	struct sprd_time_sync_device *sprd_time_sync_dev = platform_get_drvdata(pdev);

	device_destroy(sprd_time_sync_class, MKDEV(sprd_time_sync_dev->major,
		       sprd_time_sync_dev->minor));

	cdev_del(&sprd_time_sync_dev->cdev);

	unregister_chrdev_region(MKDEV(sprd_time_sync_dev->major,
				 sprd_time_sync_dev->minor), 1);

	sprd_time_sync_destroy_pdata(&sprd_time_sync_dev->init);

	platform_set_drvdata(pdev, NULL);

	class_destroy(sprd_time_sync_class);

	return 0;
}

static int sprd_time_sync_probe(struct platform_device *pdev)
{
	struct sprd_time_sync_init_data *init = pdev->dev.platform_data;
	struct sprd_time_sync_device *sprd_time_sync_dev;
	dev_t devid;
	int rval;
	struct device *dev = &pdev->dev;

	dev_info(dev, "start parse sprd_time_sync\n");

	sprd_time_sync_class = class_create(THIS_MODULE, "sprd_time_sync");
	if (IS_ERR(sprd_time_sync_class))
		return PTR_ERR(sprd_time_sync_class);

	sprd_time_sync_dev = devm_kzalloc(dev, sizeof(*sprd_time_sync_dev), GFP_KERNEL);
	if (!sprd_time_sync_dev)
		return -ENOMEM;

	tcd_dev = sprd_time_sync_dev;

	rval = sprd_time_sync_parse_dt(&init, &pdev->dev);
	if (rval) {
		dev_err(dev, "Failed to parse sprd_time_sync device tree, ret=%d\n", rval);
		return rval;
	}

	dev_dbg(dev, "after parse device tree, name=%s\n",
			init->name);

	rval = alloc_chrdev_region(&devid, 0, 1, init->name);
	if (rval != 0) {
		dev_err(dev, "Failed to alloc sprd_time_sync chrdev\n");
		goto  err_alloc_region;
	}

	cdev_init(&sprd_time_sync_dev->cdev, &sprd_time_sync_fops);

	rval = cdev_add(&sprd_time_sync_dev->cdev, devid, 1);
	if (rval != 0) {
		dev_err(dev, "Failed to add sprd_time_sync cdev\n");
		goto err_add_cdev;
	}

	sprd_time_sync_dev->major = MAJOR(devid);
	sprd_time_sync_dev->minor = MINOR(devid);
	sprd_time_sync_dev->dev = device_create(sprd_time_sync_class, NULL,
						MKDEV(sprd_time_sync_dev->major,
						sprd_time_sync_dev->minor),
						NULL, "%s", init->name);

	if (!sprd_time_sync_dev->dev) {
		dev_err(dev, "create dev failed\n");
		rval = -ENODEV;
		goto err_no_ctrl_dev;
	}

	sprd_time_sync_dev->init = init;
	platform_set_drvdata(pdev, sprd_time_sync_dev);

	dev_info(dev, "success parse sprd_time_sync\n");

	return 0;

err_no_ctrl_dev:
	cdev_del(&sprd_time_sync_dev->cdev);

err_add_cdev:
	unregister_chrdev_region(devid, 1);

err_alloc_region:
	sprd_time_sync_destroy_pdata(&init);

	return rval;
}

static const struct of_device_id sprd_time_sync_match_table[] = {
	{.compatible = "sprd,time-sync", },
	{/* MUST end with empty struct */},
};

static struct platform_driver sprd_time_sync_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "sprd_time_sync",
		.of_match_table = sprd_time_sync_match_table,
	},
	.probe = sprd_time_sync_probe,
	.remove = sprd_time_sync_remove,
};

module_platform_driver(sprd_time_sync_driver);

MODULE_AUTHOR("Catdeo Zhang & Ruifeng Zhang");
MODULE_DESCRIPTION("sprd time sync driver");
MODULE_LICENSE("GPL v2");
