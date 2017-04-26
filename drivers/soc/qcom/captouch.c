/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
 * Copyright (C) 2016 XiaoMi, Inc.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/mutex.h>
#include <linux/atomic.h>
#include <linux/of.h>
#include <linux/input.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/jiffies.h>
#include <linux/timer.h>
#include <linux/wakelock.h>

#define CAPTOUCH_DEV "captouch"
#define CAPTOUCH_TTW_HOLD_TIME 1000


extern void (*captouch_key_report_ptr)(int key_status);
extern bool (*captouch_get_status_ptr)(void);

/*
* enum captouch_commands -
*      enumeration of command options
* @CAPTOUCH_ENABLE_HOMEKEY - cmd enables home-key
* @CAPTOUCH_DISABLE_HOMEKEY - cmd disabled home-key
*/
enum captouch_commands {
	CAPTOUCH_ENABLE_HOMEKEY = 100,
	CAPTOUCH_DISABLE_HOMEKEY = 101
};

enum captouch_type {
	CAPTOUCH_TYPE_NONE = 0,
	CAPTOUCH_TYPE_FINGER_DOWN = 1,
	CAPTOUCH_TYPE_FINGER_UP = 2
};

struct captouch_drvdata {
	struct class	*captouch_class;
	struct cdev	captouch_cdev;
	struct device	*dev;
	char		*captouch_node;
	wait_queue_head_t eventq;
	unsigned int	event;
	struct mutex	mutex;
	bool            homekey_enabled;
	struct timer_list timer;
	bool		timer_scheduled;
	struct wake_lock		ttw_wl;
};

struct captouch_drvdata *g_drvdata = NULL;
/**
 * captouch_open() - Function called when user space opens device.
 * Successful if driver not currently open and clocks turned on.
 * @inode:	ptr to inode object
 * @file:	ptr to file object
 *
 * Return: 0 on success. Error code on failure.
 */
static int captouch_open(struct inode *inode, struct file *file)
{
	int rc = 0;

	struct captouch_drvdata *drvdata = container_of(inode->i_cdev,
						   struct captouch_drvdata,
						   captouch_cdev);
	file->private_data = drvdata;

	return rc;
}

/**
 * captouch_release() - Function called when user space closes device.
 *                     SPI Clocks turn off.
 * @inode:	ptr to inode object
 * @file:	ptr to file object
 *
 * Return: 0 on success. Error code on failure.
 */
static int captouch_release(struct inode *inode, struct file *file)
{
	return 0;
}

/**
 * captouch_ioctl() - Function called when user space calls ioctl.
 * @file:	struct file - not used
 * @cmd:	cmd identifier:CAPTOUCH_ENABLE,CAPTOUCH_DISABLE
 * @arg:	null
 *
 * Return: 0 on success. Error code on failure.
 */
static long captouch_ioctl(struct file *file, unsigned cmd, unsigned long arg)
{
	int rc = 0;
	void __user *priv_arg = (void __user *)arg;
	struct captouch_drvdata *drvdata;

	if (IS_ERR(priv_arg)) {
		dev_err(drvdata->dev, "%s: invalid user space pointer %lu\n",
			__func__, arg);
		return -EINVAL;
	}

	drvdata = file->private_data;

	switch (cmd) {
	case CAPTOUCH_ENABLE_HOMEKEY:
	{
		drvdata->homekey_enabled = true;
		/* send home-key event to Android framework */
		dev_info(drvdata->dev, "%s: Home-key enabled\n", __func__);
		break;
	}
	case CAPTOUCH_DISABLE_HOMEKEY:
	{
		drvdata->homekey_enabled = false;
		/* don't send home-key event to Android framework */
		dev_info(drvdata->dev, "%s: Home-key disabled\n", __func__);
		break;
	}
	default:
		dev_err(drvdata->dev, "%s: Invalid cmd %d\n", __func__, cmd);
		rc = -EINVAL;
		goto end;
	}

end:
	return rc;
}

static ssize_t captouch_read(struct file *filp, char __user *ubuf,
		size_t cnt, loff_t *ppos)
{
	unsigned int retval = 0;
	struct captouch_drvdata *drvdata = filp->private_data;

	mutex_lock(&drvdata->mutex);
	if (drvdata->event == CAPTOUCH_TYPE_NONE) {
		if (filp->f_flags & O_NONBLOCK) {
			mutex_unlock(&drvdata->mutex);
			return -EAGAIN;
		}
		dev_info(drvdata->dev, "%s: gonig to sleep\n", __func__);
	}
	mutex_unlock(&drvdata->mutex);

	wait_event_interruptible(drvdata->eventq,
				(drvdata->event > CAPTOUCH_TYPE_NONE));

	mutex_lock(&drvdata->mutex);
	retval = drvdata->event;
	drvdata->event = CAPTOUCH_TYPE_NONE;
	mutex_unlock(&drvdata->mutex);
	dev_info(drvdata->dev, "%s: returning event %d\n", __func__, retval);

	if (cnt < sizeof(retval))
		return -EINVAL;
	return copy_to_user(ubuf, &retval, sizeof(retval));
}

void captouch_timer_fn(unsigned long arg)
{
	struct captouch_drvdata *drvdata = (struct captouch_drvdata *)arg;

	dev_info(drvdata->dev, "%s: touch event timer fired. awakening the readers.\n",
		__func__);
	drvdata->event = CAPTOUCH_TYPE_FINGER_DOWN;
	drvdata->timer_scheduled = false;
	wake_up_interruptible(&drvdata->eventq);
}

static int captouch_schedule_touch_event(struct captouch_drvdata *drvdata, int seconds)
{
	unsigned long j = jiffies;

	/* register the timer */
	init_timer(&drvdata->timer);
	drvdata->timer.data = (unsigned long)drvdata;
	drvdata->timer.function = captouch_timer_fn;
	drvdata->timer.expires = j + (HZ*seconds);
	add_timer(&drvdata->timer);
	drvdata->timer_scheduled = true;
	return 0;
}

static ssize_t captouch_write(struct file *filp,
		const char __user *ubuf, size_t cnt, loff_t *ppos)
{
	struct captouch_drvdata *drvdata = filp->private_data;
	char buf[16];
	char *cmp, *str, *str_running;
	long timeout_secs;
	static unsigned int event = CAPTOUCH_TYPE_FINGER_DOWN;

	cnt = min(cnt, sizeof(buf) - 1);
	if (copy_from_user(&buf, ubuf, cnt))
		return -EFAULT;
	buf[cnt] = '\0';
	cmp = strstrip(buf);

	mutex_lock(&drvdata->mutex);
	if (drvdata->timer_scheduled) {
		dev_info(drvdata->dev, "%s: touch event already scheduled\n", __func__);
	} else {
		str_running = (char *)&buf;
		str = strsep(&str_running, " ");
		if (!strcmp(str, "wake") && str_running) {
			if (kstrtol(str_running, 0, &timeout_secs)) {
				dev_err(drvdata->dev, "%s: error parsing %s",
					__func__, str_running);
				/* use 30 seconds default timeout */
				timeout_secs = 30;
			}
			dev_info(drvdata->dev, "%s: scheduling touch event in %ld seconds\n",
				__func__, timeout_secs);
			captouch_schedule_touch_event(drvdata, timeout_secs);
		}
	}
	drvdata->event = event;
	/* set next event value */
	if (event == CAPTOUCH_TYPE_FINGER_DOWN)
		event = CAPTOUCH_TYPE_FINGER_UP;
	else
		event = CAPTOUCH_TYPE_FINGER_DOWN;
	mutex_unlock(&drvdata->mutex);

	dev_info(drvdata->dev, "%s: awakening the readers\n", __func__);
	wake_up_interruptible(&drvdata->eventq);

	return cnt;
}

static unsigned int captouch_poll(struct file *filp,
		struct poll_table_struct *wait)
{
	struct captouch_drvdata *drvdata = filp->private_data;
	unsigned int mask = 0;

	poll_wait(filp, &drvdata->eventq, wait);

	if (drvdata->event > CAPTOUCH_TYPE_NONE)
		mask |= (POLLIN | POLLRDNORM);

	dev_dbg(drvdata->dev, "%s: return!\n", __func__);
	return mask;
}

static const struct file_operations captouch_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = captouch_ioctl,
	.open = captouch_open,
	.release = captouch_release,
	.read = captouch_read,
	.write = captouch_write,
	.poll = captouch_poll
};

static int captouch_dev_register(struct captouch_drvdata *drvdata)
{
	dev_t dev_no;
	int ret = 0;
	size_t node_size;
	char *node_name = CAPTOUCH_DEV;
	struct device *dev = drvdata->dev;
	struct device *device;

	node_size = strlen(node_name) + 1;

	drvdata->captouch_node = devm_kzalloc(dev, node_size, GFP_KERNEL);
	if (!drvdata->captouch_node) {
		ret = -ENOMEM;
		goto err_alloc;
	}

	strlcpy(drvdata->captouch_node, node_name, node_size);

	ret = alloc_chrdev_region(&dev_no, 0, 1, drvdata->captouch_node);
	if (ret) {
		dev_err(drvdata->dev, "%s: alloc_chrdev_region failed %d\n",
			__func__, ret);
		goto err_alloc;
	}

	cdev_init(&drvdata->captouch_cdev, &captouch_fops);

	drvdata->captouch_cdev.owner = THIS_MODULE;
	ret = cdev_add(&drvdata->captouch_cdev, dev_no, 1);
	if (ret) {
		dev_err(drvdata->dev, "%s: cdev_add failed %d\n", __func__,
			ret);
		goto err_cdev_add;
	}

	drvdata->captouch_class = class_create(THIS_MODULE,
					   drvdata->captouch_node);
	if (IS_ERR(drvdata->captouch_class)) {
		ret = PTR_ERR(drvdata->captouch_class);
		dev_err(drvdata->dev, "%s: class_create failed %d\n", __func__,
			ret);
		goto err_class_create;
	}

	device = device_create(drvdata->captouch_class, NULL,
			       drvdata->captouch_cdev.dev, drvdata,
			       drvdata->captouch_node);
	if (IS_ERR(device)) {
		ret = PTR_ERR(device);
		dev_err(drvdata->dev, "%s: device_create failed %d\n",
			__func__, ret);
		goto err_dev_create;
	}

	return 0;

err_dev_create:
	class_destroy(drvdata->captouch_class);
err_class_create:
	cdev_del(&drvdata->captouch_cdev);
err_cdev_add:
	unregister_chrdev_region(drvdata->captouch_cdev.dev, 1);
err_alloc:
	return ret;
}

bool captouch_get_status(void)
{
	/*TODO: check SSC FD when resume*/

	dev_info(g_drvdata->dev, "%s: homekey_enabled = %s\n",
		__func__, g_drvdata->homekey_enabled ? "true":"false");

	return g_drvdata->homekey_enabled;
}

void captouch_key_report(int key_status)
{
	wake_lock_timeout(&g_drvdata->ttw_wl, msecs_to_jiffies(CAPTOUCH_TTW_HOLD_TIME));

	if (key_status == 1)
		g_drvdata->event = CAPTOUCH_TYPE_FINGER_DOWN;
	else if (key_status == 0)
		g_drvdata->event = CAPTOUCH_TYPE_FINGER_UP;
	else
		g_drvdata->event = CAPTOUCH_TYPE_NONE;

	dev_info(g_drvdata->dev, "%s: key_status = %d (1:down 0:up)\n", __func__, key_status);

	wake_up_interruptible(&g_drvdata->eventq);
}

/**
 * captouch_probe() - Function loads hardware config from device tree
 * @pdev:	ptr to platform device object
 *
 * Return: 0 on success. Error code on failure.
 */
static int captouch_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct captouch_drvdata *drvdata;
	int rc = 0;


	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	g_drvdata = drvdata;

	drvdata->dev = &pdev->dev;
	platform_set_drvdata(pdev, drvdata);

	dev_info(drvdata->dev, "%s: ENTRY\n", __func__);

	mutex_init(&drvdata->mutex);
	init_waitqueue_head(&drvdata->eventq);
	drvdata->event = CAPTOUCH_TYPE_NONE;
	drvdata->homekey_enabled = true;
	drvdata->timer_scheduled = false;

	rc = captouch_dev_register(drvdata);
	if (rc < 0)
		goto end;

	captouch_key_report_ptr = captouch_key_report;
	captouch_get_status_ptr = captouch_get_status;
	wake_lock_init(&drvdata->ttw_wl, WAKE_LOCK_SUSPEND, "captouch_ttw_wl");

end:
	return rc;
}

static int captouch_remove(struct platform_device *pdev)
{
	struct captouch_drvdata *drvdata = platform_get_drvdata(pdev);

	dev_info(drvdata->dev, "%s: ENTRY\n", __func__);

	mutex_destroy(&drvdata->mutex);
	device_destroy(drvdata->captouch_class, drvdata->captouch_cdev.dev);
	class_destroy(drvdata->captouch_class);
	cdev_del(&drvdata->captouch_cdev);
	unregister_chrdev_region(drvdata->captouch_cdev.dev, 1);
	return 0;
}

static int captouch_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static struct of_device_id captouch_match[] = {
	{ .compatible = "qcom,captouch" },
	{}
};

static struct platform_driver captouch_plat_driver = {
	.probe = captouch_probe,
	.remove = captouch_remove,
	.suspend = captouch_suspend,
	.driver = {
		.name = "captouch",
		.owner = THIS_MODULE,
		.of_match_table = captouch_match,
	},
};

static int captouch_init(void)
{
	return platform_driver_register(&captouch_plat_driver);
}
module_init(captouch_init);

static void captouch_exit(void)
{
	platform_driver_unregister(&captouch_plat_driver);
}
module_exit(captouch_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Qualcomm Technologies, Inc. CAPTOUCH driver");
