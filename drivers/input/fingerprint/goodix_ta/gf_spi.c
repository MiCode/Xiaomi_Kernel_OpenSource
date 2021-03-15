/*
 * TEE driver for goodix fingerprint sensor
 * Copyright (C) 2016 Goodix
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#define DEBUG
#define pr_fmt(fmt)     KBUILD_MODNAME ": " fmt

#define GOODIX_DRM_INTERFACE_WA

#include <linux/init.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/input.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/compat.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/ktime.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include <linux/timer.h>
#include <linux/notifier.h>
#include <linux/fb.h>
#include <linux/pm_qos.h>
#include <linux/cpufreq.h>
#include <linux/pm_wakeup.h>
#include <drm/drm_bridge.h>
#ifndef GOODIX_DRM_INTERFACE_WA
#include <drm/drm_notifier_mi.h>
#endif

#include "gf_spi.h"

#if defined(USE_SPI_BUS)
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#elif defined(USE_PLATFORM_BUS)
#include <linux/platform_device.h>
#endif

#define VER_MAJOR   1
#define VER_MINOR   2
#define PATCH_LEVEL 1

#define WAKELOCK_HOLD_TIME 2000 /* in ms */
#define FP_UNLOCK_REJECTION_TIMEOUT (WAKELOCK_HOLD_TIME - 500)

#define GF_SPIDEV_NAME			"goodix,fingerprint"
/*device name after register in charater*/
#define GF_DEV_NAME				"goodix_fp"
#define GF_INPUT_NAME			"uinput-goodix" /*"goodix_fp" */

#define CHRD_DRIVER_NAME		"goodix_fp_spi"
#define CLASS_NAME				"goodix_fp"

#define N_SPI_MINORS			32  /* ... up to 256 */


static int SPIDEV_MAJOR;

static DECLARE_BITMAP(minors, N_SPI_MINORS);
static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);
static struct wakeup_source *fp_wakelock;
static struct gf_dev gf;

struct gf_key_map maps[] = {
	{ EV_KEY, GF_KEY_INPUT_HOME },
	{ EV_KEY, GF_KEY_INPUT_MENU },
	{ EV_KEY, GF_KEY_INPUT_BACK },
	{ EV_KEY, GF_KEY_INPUT_POWER },
#if defined(SUPPORT_NAV_EVENT)
	{ EV_KEY, GF_NAV_INPUT_UP },
	{ EV_KEY, GF_NAV_INPUT_DOWN },
	{ EV_KEY, GF_NAV_INPUT_RIGHT },
	{ EV_KEY, GF_NAV_INPUT_LEFT },
	{ EV_KEY, GF_KEY_INPUT_CAMERA },
	{ EV_KEY, GF_NAV_INPUT_CLICK },
	{ EV_KEY, GF_NAV_INPUT_DOUBLE_CLICK },
	{ EV_KEY, GF_NAV_INPUT_LONG_PRESS },
	{ EV_KEY, GF_NAV_INPUT_HEAVY },
#endif
};

static void gf_enable_irq(struct gf_dev *gf_dev)
{
	if (gf_dev->irq_enabled) {
		pr_warn("IRQ has been enabled.\n");
	} else {
		enable_irq(gf_dev->irq);
		gf_dev->irq_enabled = 1;
	}
}

static void gf_disable_irq(struct gf_dev *gf_dev)
{
	if (gf_dev->irq_enabled) {
		gf_dev->irq_enabled = 0;
		disable_irq(gf_dev->irq);
	} else {
		pr_warn("IRQ has been disabled.\n");
	}
}

#ifdef AP_CONTROL_CLK
static long spi_clk_max_rate(struct clk *clk, unsigned long rate)
{
	long lowest_available, nearest_low, step_size, cur;
	long step_direction = -1;
	long guess = rate;
	int max_steps = 10;
	cur = clk_round_rate(clk, rate);

	if (cur == rate) {
		return rate;
	}

	/* if we got here then: cur > rate */
	lowest_available = clk_round_rate(clk, 0);

	if (lowest_available > rate) {
		return -EINVAL;
	}

	step_size = (rate - lowest_available) >> 1;
	nearest_low = lowest_available;

	while (max_steps-- && step_size) {
		guess += step_size * step_direction;
		cur = clk_round_rate(clk, guess);

		if ((cur < rate) && (cur > nearest_low)) {
			nearest_low = cur;
		}

		/*
		 * if we stepped too far, then start stepping in the other
		 * direction with half the step size
		 */
		if (((cur > rate) && (step_direction > 0))
			|| ((cur < rate) && (step_direction < 0))) {
			step_direction = -step_direction;
			step_size >>= 1;
		}
	}

	return nearest_low;
}

static void spi_clock_set(struct gf_dev *gf_dev, int speed)
{
	long rate;
	int rc;
	rate = spi_clk_max_rate(gf_dev->core_clk, speed);

	if (rate < 0) {
		pr_debug("%s: no match found for requested clock frequency:%d",
			 __func__, speed);
		return;
	}

	rc = clk_set_rate(gf_dev->core_clk, rate);
}

static int gfspi_ioctl_clk_init(struct gf_dev *data)
{
	pr_debug("%s: enter\n", __func__);
	data->clk_enabled = 0;
	data->core_clk = clk_get(&data->spi->dev, "core_clk");

	if (IS_ERR_OR_NULL(data->core_clk)) {
		pr_err("%s: fail to get core_clk\n", __func__);
		return -EPERM;
	}

	data->iface_clk = clk_get(&data->spi->dev, "iface_clk");

	if (IS_ERR_OR_NULL(data->iface_clk)) {
		pr_err("%s: fail to get iface_clk\n", __func__);
		clk_put(data->core_clk);
		data->core_clk = NULL;
		return -ENOENT;
	}

	return 0;
}

static int gfspi_ioctl_clk_enable(struct gf_dev *data)
{
	int err;
	pr_debug("%s: enter\n", __func__);

	if (data->clk_enabled) {
		return 0;
	}

	err = clk_prepare_enable(data->core_clk);

	if (err) {
		pr_err("%s: fail to enable core_clk\n", __func__);
		return -EPERM;
	}

	err = clk_prepare_enable(data->iface_clk);

	if (err) {
		pr_err("%s: fail to enable iface_clk\n", __func__);
		clk_disable_unprepare(data->core_clk);
		return -ENOENT;
	}

	data->clk_enabled = 1;
	return 0;
}

static int gfspi_ioctl_clk_disable(struct gf_dev *data)
{
	pr_debug("%s: enter\n", __func__);

	if (!data->clk_enabled) {
		return 0;
	}

	clk_disable_unprepare(data->core_clk);
	clk_disable_unprepare(data->iface_clk);
	data->clk_enabled = 0;
	return 0;
}

static int gfspi_ioctl_clk_uninit(struct gf_dev *data)
{
	pr_debug("%s: enter\n", __func__);

	if (data->clk_enabled) {
		gfspi_ioctl_clk_disable(data);
	}

	if (!IS_ERR_OR_NULL(data->core_clk)) {
		clk_put(data->core_clk);
		data->core_clk = NULL;
	}

	if (!IS_ERR_OR_NULL(data->iface_clk)) {
		clk_put(data->iface_clk);
		data->iface_clk = NULL;
	}

	return 0;
}
#endif

static void nav_event_input(struct gf_dev *gf_dev, gf_nav_event_t nav_event)
{
	uint32_t nav_input = 0;

	switch (nav_event) {
		case GF_NAV_FINGER_DOWN:
			pr_debug("%s nav finger down\n", __func__);
			break;

		case GF_NAV_FINGER_UP:
			pr_debug("%s nav finger up\n", __func__);
			break;

		case GF_NAV_DOWN:
			nav_input = GF_NAV_INPUT_DOWN;
			pr_debug("%s nav down\n", __func__);
			break;

		case GF_NAV_UP:
			nav_input = GF_NAV_INPUT_UP;
			pr_debug("%s nav up\n", __func__);
			break;

		case GF_NAV_LEFT:
			nav_input = GF_NAV_INPUT_LEFT;
			pr_debug("%s nav left\n", __func__);
			break;

		case GF_NAV_RIGHT:
			nav_input = GF_NAV_INPUT_RIGHT;
			pr_debug("%s nav right\n", __func__);
			break;

		case GF_NAV_CLICK:
			nav_input = GF_NAV_INPUT_CLICK;
			pr_debug("%s nav click\n", __func__);
			break;

		case GF_NAV_HEAVY:
			nav_input = GF_NAV_INPUT_HEAVY;
			pr_debug("%s nav heavy\n", __func__);
			break;

		case GF_NAV_LONG_PRESS:
			nav_input = GF_NAV_INPUT_LONG_PRESS;
			pr_debug("%s nav long press\n", __func__);
			break;

		case GF_NAV_DOUBLE_CLICK:
			nav_input = GF_NAV_INPUT_DOUBLE_CLICK;
			pr_debug("%s nav double click\n", __func__);
			break;

		default:
			pr_warn("%s unknown nav event: %d\n", __func__, nav_event);
			break;
	}

	if ((nav_event != GF_NAV_FINGER_DOWN) && (nav_event != GF_NAV_FINGER_UP)) {
		input_report_key(gf_dev->input, nav_input, 1);
		input_sync(gf_dev->input);
		input_report_key(gf_dev->input, nav_input, 0);
		input_sync(gf_dev->input);
	}
}


static void gf_kernel_key_input(struct gf_dev *gf_dev, struct gf_key *gf_key)
{
	uint32_t key_input = 0;

	if (GF_KEY_HOME == gf_key->key) {
		key_input = GF_KEY_INPUT_HOME;
	} else if (GF_KEY_POWER == gf_key->key) {
		key_input = GF_KEY_INPUT_POWER;
	} else if (GF_KEY_CAMERA == gf_key->key) {
		key_input = GF_KEY_INPUT_CAMERA;
	} else {
		/* add special key define */
		key_input = gf_key->key;
	}

	pr_debug("%s: received key event[%d], key=%d, value=%d\n",
		 __func__, key_input, gf_key->key, gf_key->value);

	if ((GF_KEY_POWER == gf_key->key || GF_KEY_CAMERA == gf_key->key)
		&& (gf_key->value == 1)) {
		input_report_key(gf_dev->input, key_input, 1);
		input_sync(gf_dev->input);
		input_report_key(gf_dev->input, key_input, 0);
		input_sync(gf_dev->input);
	}

	if (GF_KEY_HOME == gf_key->key) {
		input_report_key(gf_dev->input, key_input, gf_key->value);
		input_sync(gf_dev->input);
	}
}

static long gf_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct gf_dev *gf_dev = &gf;
	struct gf_key gf_key;
#if defined(SUPPORT_NAV_EVENT)
	gf_nav_event_t nav_event = GF_NAV_NONE;
#endif
	int retval = 0;
	u8 netlink_route = NETLINK_TEST;
	struct gf_ioc_chip_info info;

	if (_IOC_TYPE(cmd) != GF_IOC_MAGIC) {
		return -ENODEV;
	}

	if (_IOC_DIR(cmd) & _IOC_READ) {
		retval = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	} else if (_IOC_DIR(cmd) & _IOC_WRITE) {
		retval = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	}

	if (retval) {
		return -EFAULT;
	}

	if (gf_dev->device_available == 0) {
		if ((cmd == GF_IOC_ENABLE_POWER) || (cmd == GF_IOC_DISABLE_POWER)) {
			pr_debug("power cmd\n");
		} else {
			pr_debug("get cmd %d, but sensor is power off currently.\n", _IOC_NR(cmd));
			return -ENODEV;
		}
	}

	switch (cmd) {
		case GF_IOC_INIT:
			pr_debug("%s GF_IOC_INIT\n", __func__);

			if (copy_to_user((void __user *)arg, (void *)&netlink_route, sizeof(u8))) {
				retval = -EFAULT;
				break;
			}

			break;

		case GF_IOC_EXIT:
			pr_debug("%s GF_IOC_EXIT\n", __func__);
			break;

		case GF_IOC_DISABLE_IRQ:
			pr_debug("%s GF_IOC_DISABEL_IRQ\n", __func__);
			gf_disable_irq(gf_dev);
			break;

		case GF_IOC_ENABLE_IRQ:
			pr_debug("%s GF_IOC_ENABLE_IRQ\n", __func__);
			gf_enable_irq(gf_dev);
			break;

		case GF_IOC_RESET:
			pr_debug("%s GF_IOC_RESET.\n", __func__);
			gf_hw_reset(gf_dev, 3);
			break;

		case GF_IOC_INPUT_KEY_EVENT:
			if (copy_from_user(&gf_key, (struct gf_key *)arg, sizeof(struct gf_key))) {
				pr_debug("Failed to copy input key event from user to kernel\n");
				retval = -EFAULT;
				break;
			}

			gf_kernel_key_input(gf_dev, &gf_key);
			break;
#if defined(SUPPORT_NAV_EVENT)

		case GF_IOC_NAV_EVENT:
			pr_debug("%s GF_IOC_NAV_EVENT\n", __func__);

			if (copy_from_user(&nav_event, (gf_nav_event_t *)arg, sizeof(gf_nav_event_t))) {
				pr_debug("Failed to copy nav event from user to kernel\n");
				retval = -EFAULT;
				break;
			}

			nav_event_input(gf_dev, nav_event);
			break;
#endif

		case GF_IOC_ENABLE_SPI_CLK:
#ifdef AP_CONTROL_CLK
			gfspi_ioctl_clk_enable(gf_dev);
#endif
			break;

		case GF_IOC_DISABLE_SPI_CLK:
#ifdef AP_CONTROL_CLK
			gfspi_ioctl_clk_disable(gf_dev);
#endif
			break;

		case GF_IOC_ENABLE_POWER:
			pr_debug("%s GF_IOC_ENABLE_POWER\n", __func__);

			if (gf_dev->device_available == 1) {
				pr_debug("Sensor has already powered-on.\n");
			} else {
				gf_power_on(gf_dev);
			}

			gf_dev->device_available = 1;
			break;

		case GF_IOC_DISABLE_POWER:
			pr_debug("%s GF_IOC_DISABLE_POWER\n", __func__);

			if (gf_dev->device_available == 0) {
				pr_debug("Sensor has already powered-off.\n");
			} else {
				gf_power_off(gf_dev);
			}

			gf_dev->device_available = 0;
			break;

		case GF_IOC_ENTER_SLEEP_MODE:
			pr_debug("%s GF_IOC_ENTER_SLEEP_MODE\n", __func__);
			break;

		case GF_IOC_GET_FW_INFO:
			pr_debug("%s GF_IOC_GET_FW_INFO\n", __func__);
			break;

		case GF_IOC_REMOVE:
			pr_debug("%s GF_IOC_REMOVE\n", __func__);
			break;

		case GF_IOC_CHIP_INFO:
			pr_debug("%s GF_IOC_CHIP_INFO\n", __func__);

			if (copy_from_user(&info, (struct gf_ioc_chip_info *)arg,
						sizeof(struct gf_ioc_chip_info))) {
				retval = -EFAULT;
				break;
			}

			pr_debug("vendor_id : 0x%x\n", info.vendor_id);
			pr_debug("mode : 0x%x\n", info.mode);
			pr_debug("operation: 0x%x\n", info.operation);
			break;

		default:
			pr_warn("unsupport cmd:0x%x\n", cmd);
			break;
	}

	return retval;
}

#ifdef CONFIG_COMPAT
static long gf_compat_ioctl(struct file *filp, unsigned int cmd,
				unsigned long arg)
{
	return gf_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#endif /*CONFIG_COMPAT*/

#ifndef GOODIX_DRM_INTERFACE_WA
static void notification_work(struct work_struct *work)
{
	pr_debug("%s unblank\n", __func__);
	dsi_bridge_interface_enable(FP_UNLOCK_REJECTION_TIMEOUT);
}
#endif

static irqreturn_t gf_irq(int irq, void *handle)
{
	struct gf_dev *gf_dev = &gf;
#if defined(GF_NETLINK_ENABLE)
	char temp[4] = { 0x0 };
	uint32_t key_input = 0;
	temp[0] = GF_NET_EVENT_IRQ;
	pr_debug("%s enter\n", __func__);
	__pm_wakeup_event(fp_wakelock, WAKELOCK_HOLD_TIME);
	sendnlmsg(temp);

	if ((gf_dev->wait_finger_down == true) && (gf_dev->device_available == 1) &&
		(gf_dev->fb_black == 1)) {
		key_input = KEY_RIGHT;
		input_report_key(gf_dev->input, key_input, 1);
		input_sync(gf_dev->input);
		input_report_key(gf_dev->input, key_input, 0);
		input_sync(gf_dev->input);
		gf_dev->wait_finger_down = false;
		schedule_work(&gf_dev->work);
	}

#elif defined (GF_FASYNC)
	struct gf_dev *gf_dev = &gf;

	if (gf_dev->async) {
		kill_fasync(&gf_dev->async, SIGIO, POLL_IN);
	}

#endif
	return IRQ_HANDLED;
}

static int gf_open(struct inode *inode, struct file *filp)
{
	struct gf_dev *gf_dev;
	int status = -ENXIO;
	int rc = 0;
	int err = 0;
	mutex_lock(&device_list_lock);
	list_for_each_entry(gf_dev, &device_list, device_entry) {
		if (gf_dev->devt == inode->i_rdev) {
			pr_debug("Found\n");
			status = 0;
			break;
		}
	}
#ifdef CONFIG_FINGERPRINT_FP_VREG_CONTROL
	pr_info("Try to enable fp_vdd_vreg\n");
	gf_dev->vreg = regulator_get(&gf_dev->spi->dev, "fp_vdd_vreg");

	if (gf_dev->vreg == NULL) {
		dev_err(&gf_dev->spi->dev, "fp_vdd_vreg regulator get failed!\n");
		mutex_unlock(&device_list_lock);
		return -EPERM;
	}

	if (regulator_is_enabled(gf_dev->vreg)) {
		pr_info("fp_vdd_vreg is already enabled!\n");
	} else {
		rc = regulator_enable(gf_dev->vreg);

		if (rc) {
			dev_err(&gf_dev->spi->dev, "error enabling fp_vdd_vreg!\n");
			regulator_put(gf_dev->vreg);
			gf_dev->vreg = NULL;
			mutex_unlock(&device_list_lock);
			return -EPERM;
		}
	}

	pr_info("fp_vdd_vreg is enabled!\n");
#endif

	if (status == 0) {
#ifdef GF_PW_CTL
		rc = gpio_request(gf_dev->pwr_gpio, "goodix_pwr");

		if (rc) {
			dev_err(&gf_dev->spi->dev, "Failed to request PWR GPIO. rc = %d\n", rc);
			mutex_unlock(&device_list_lock);
			err = -EPERM;
			goto open_error1;
		}
#endif

		rc = gpio_request(gf_dev->reset_gpio, "gpio-reset");

		if (rc) {
			dev_err(&gf_dev->spi->dev, "Failed to request RESET GPIO. rc = %d\n", rc);
			mutex_unlock(&device_list_lock);
			err = -EPERM;
			goto open_error1;
		}

		gpio_direction_output(gf_dev->reset_gpio, 0);
		rc = gpio_request(gf_dev->irq_gpio, "gpio-irq");

		if (rc) {
			dev_err(&gf_dev->spi->dev, "Failed to request IRQ GPIO. rc = %d\n", rc);
			mutex_unlock(&device_list_lock);
			err = -EPERM;
			goto open_error2;
		}

		gpio_direction_input(gf_dev->irq_gpio);
		rc = request_threaded_irq(gf_dev->irq, NULL, gf_irq,
						IRQF_TRIGGER_RISING | IRQF_ONESHOT,
						"gf", gf_dev);

		if (!rc) {
			enable_irq_wake(gf_dev->irq);
			gf_dev->irq_enabled = 1;
			gf_disable_irq(gf_dev);
		} else {
			err = -EPERM;
			goto open_error3;
		}

		gf_dev->users++;
		filp->private_data = gf_dev;
		nonseekable_open(inode, filp);
		pr_debug("Succeed to open device. irq = %d\n", gf_dev->irq);
	} else {
		pr_debug("No device for minor %d\n", iminor(inode));
	}

	mutex_unlock(&device_list_lock);
	return status;
open_error3:
	gpio_free(gf_dev->irq_gpio);
open_error2:
	gpio_free(gf_dev->reset_gpio);
open_error1:
	return err;
}

#ifdef GF_FASYNC
static int gf_fasync(int fd, struct file *filp, int mode)
{
	struct gf_dev *gf_dev = filp->private_data;
	int ret;
	ret = fasync_helper(fd, filp, mode, &gf_dev->async);
	pr_debug("ret = %d\n", ret);
	return ret;
}
#endif

static int gf_release(struct inode *inode, struct file *filp)
{
	struct gf_dev *gf_dev;
	int status = 0;
	pr_debug("%s\n", __func__);
	mutex_lock(&device_list_lock);
	gf_dev = filp->private_data;
	filp->private_data = NULL;
	/*
	 *Disable fp_vdd_vreg regulator
	 */
#ifdef CONFIG_FINGERPRINT_FP_VREG_CONTROL
	pr_info("disable fp_vdd_vreg!\n");

	if (regulator_is_enabled(gf_dev->vreg)) {
		regulator_disable(gf_dev->vreg);
		regulator_put(gf_dev->vreg);
		gf_dev->vreg = NULL;
	}

#endif
	gf_dev->users --;

	if (!gf_dev->users) {
		pr_debug("disble_irq. irq = %d\n", gf_dev->irq);
		gf_disable_irq(gf_dev);
		/*power off the sensor*/
		gf_dev->device_available = 0;
		free_irq(gf_dev->irq, gf_dev);
		gpio_free(gf_dev->irq_gpio);
		gpio_free(gf_dev->reset_gpio);
		gf_power_off(gf_dev);
#ifdef GF_PW_CTL
		gpio_free(gf_dev->pwr_gpio);
#endif
	}

	mutex_unlock(&device_list_lock);
	return status;
}

static const struct file_operations gf_fops = {
	.owner = THIS_MODULE,
	/* REVISIT switch to aio primitives, so that userspace
	 * gets more complete API coverage.  It'll simplify things
	 * too, except for the locking.
	 */
	.unlocked_ioctl = gf_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = gf_compat_ioctl,
#endif /*CONFIG_COMPAT*/
	.open = gf_open,
	.release = gf_release,
#ifdef GF_FASYNC
	.fasync = gf_fasync,
#endif
};


#ifndef GOODIX_DRM_INTERFACE_WA
static int goodix_fb_state_chg_callback(struct notifier_block *nb,
					unsigned long val, void *data)
{
	struct gf_dev *gf_dev;
	struct fb_event *evdata = data;
	unsigned int blank;
	char temp[4] = { 0x0 };

	if (val != MI_DRM_EVENT_BLANK) {
		return 0;
	}

	pr_debug("[info] %s go to the goodix_fb_state_chg_callback value = %d\n",
		 __func__, (int)val);
	gf_dev = container_of(nb, struct gf_dev, notifier);

	if (evdata && evdata->data && val == MI_DRM_EVENT_BLANK && gf_dev) {
		blank = *(int *)(evdata->data);

		switch (blank) {
			case MI_DRM_BLANK_POWERDOWN:
				if (gf_dev->device_available == 1) {
					gf_dev->fb_black = 1;
					gf_dev->wait_finger_down = true;
#if defined(GF_NETLINK_ENABLE)
					temp[0] = GF_NET_EVENT_FB_BLACK;
					sendnlmsg(temp);
#elif defined (GF_FASYNC)

					if (gf_dev->async) {
						kill_fasync(&gf_dev->async, SIGIO, POLL_IN);
					}

#endif
				}
				break;

			case MI_DRM_BLANK_UNBLANK:
				if (gf_dev->device_available == 1) {
					gf_dev->fb_black = 0;
#if defined(GF_NETLINK_ENABLE)
					temp[0] = GF_NET_EVENT_FB_UNBLACK;
					sendnlmsg(temp);
#elif defined (GF_FASYNC)

					if (gf_dev->async) {
						kill_fasync(&gf_dev->async, SIGIO, POLL_IN);
					}

#endif
				}

				break;

			default:
				pr_debug("%s defalut\n", __func__);
				break;
		}
	}

	return NOTIFY_OK;
}

static struct notifier_block goodix_noti_block = {
	.notifier_call = goodix_fb_state_chg_callback,
};
#endif

static struct class *gf_class;
#if defined(USE_SPI_BUS)
static int gf_probe(struct spi_device *spi)
#elif defined(USE_PLATFORM_BUS)
static int gf_probe(struct platform_device *pdev)
#endif
{
	struct gf_dev *gf_dev = &gf;
	int status = -EINVAL;
	unsigned long minor;
	int i;
	/* Initialize the driver data */
	INIT_LIST_HEAD(&gf_dev->device_entry);
#if defined(USE_SPI_BUS)
	gf_dev->spi = spi;
#elif defined(USE_PLATFORM_BUS)
	gf_dev->spi = pdev;
#endif
	gf_dev->irq_gpio = -EINVAL;
	gf_dev->reset_gpio = -EINVAL;
	gf_dev->pwr_gpio = -EINVAL;
	gf_dev->device_available = 0;
	gf_dev->fb_black = 0;
	gf_dev->wait_finger_down = false;
#ifndef GOODIX_DRM_INTERFACE_WA
	INIT_WORK(&gf_dev->work, notification_work);
#endif

	if (gf_parse_dts(gf_dev)) {
		goto error_hw;
	}

	/* If we can allocate a minor number, hook up this device.
	 * Reusing minors is fine so long as udev or mdev is working.
	 */
	mutex_lock(&device_list_lock);
	minor = find_first_zero_bit(minors, N_SPI_MINORS);

	if (minor < N_SPI_MINORS) {
		struct device *dev;
		gf_dev->devt = MKDEV(SPIDEV_MAJOR, minor);
		dev = device_create(gf_class, &gf_dev->spi->dev, gf_dev->devt,
					gf_dev, GF_DEV_NAME);
		status = IS_ERR(dev) ? PTR_ERR(dev) : 0;
	} else {
		dev_dbg(&gf_dev->spi->dev, "no minor number available!\n");
		status = -ENODEV;
		mutex_unlock(&device_list_lock);
		goto error_hw;
	}

	if (status == 0) {
		set_bit(minor, minors);
		list_add(&gf_dev->device_entry, &device_list);
	} else {
		gf_dev->devt = 0;
	}

	mutex_unlock(&device_list_lock);

	if (status == 0) {
		/*input device subsystem */
		gf_dev->input = input_allocate_device();

		if (gf_dev->input == NULL) {
			pr_err("%s, failed to allocate input device\n", __func__);
			status = -ENOMEM;
			goto error_dev;
		}

		for (i = 0; i < ARRAY_SIZE(maps); i++) {
			input_set_capability(gf_dev->input, maps[i].type, maps[i].code);
		}

		gf_dev->input->name = GF_INPUT_NAME;
		status = input_register_device(gf_dev->input);

		if (status) {
			pr_err("failed to register input device\n");
			goto error_input;
		}
	}

#ifdef AP_CONTROL_CLK
	pr_debug("Get the clk resource.\n");

	/* Enable spi clock */
	if (gfspi_ioctl_clk_init(gf_dev)) {
		goto gfspi_probe_clk_init_failed;
	}

	if (gfspi_ioctl_clk_enable(gf_dev)) {
		goto gfspi_probe_clk_enable_failed;
	}

	spi_clock_set(gf_dev, 1000000);
#endif
#ifndef GOODIX_DRM_INTERFACE_WA
	gf_dev->notifier = goodix_noti_block;
	mi_drm_register_client(&gf_dev->notifier);
#endif
	gf_dev->irq = gf_irq_num(gf_dev);
	fp_wakelock = wakeup_source_register(&gf_dev->spi->dev, "fp_wakelock");
	pr_debug("version V%d.%d.%02d\n", VER_MAJOR, VER_MINOR, PATCH_LEVEL);
	return status;
#ifdef AP_CONTROL_CLK
gfspi_probe_clk_enable_failed:
	gfspi_ioctl_clk_uninit(gf_dev);
gfspi_probe_clk_init_failed:
#endif
	input_unregister_device(gf_dev->input);
error_input:

	if (gf_dev->input != NULL) {
		input_free_device(gf_dev->input);
	}

error_dev:

	if (gf_dev->devt != 0) {
		pr_debug("Err: status = %d\n", status);
		mutex_lock(&device_list_lock);
		list_del(&gf_dev->device_entry);
		device_destroy(gf_class, gf_dev->devt);
		clear_bit(MINOR(gf_dev->devt), minors);
		mutex_unlock(&device_list_lock);
	}

error_hw:
	gf_cleanup(gf_dev);
	gf_dev->device_available = 0;
	return status;
}

#if defined(USE_SPI_BUS)
static int gf_remove(struct spi_device *spi)
#elif defined(USE_PLATFORM_BUS)
static int gf_remove(struct platform_device *pdev)
#endif
{
	struct gf_dev *gf_dev = &gf;
	wakeup_source_unregister(fp_wakelock);

	/* make sure ops on existing fds can abort cleanly */
	if (gf_dev->irq) {
		free_irq(gf_dev->irq, gf_dev);
	}

	if (gf_dev->input != NULL) {
		input_unregister_device(gf_dev->input);
	}

	input_free_device(gf_dev->input);
	/* prevent new opens */
	mutex_lock(&device_list_lock);
	list_del(&gf_dev->device_entry);
	device_destroy(gf_class, gf_dev->devt);
	clear_bit(MINOR(gf_dev->devt), minors);

	if (gf_dev->users == 0) {
		gf_cleanup(gf_dev);
	}

#ifndef GOODIX_DRM_INTERFACE_WA
	mi_drm_unregister_client(&gf_dev->notifier);
#endif
	mutex_unlock(&device_list_lock);
	return 0;
}

static struct of_device_id gx_match_table[] = {
	{ .compatible = GF_SPIDEV_NAME },
	{},
};

#if defined(USE_SPI_BUS)
static struct spi_driver gf_driver = {
#elif defined(USE_PLATFORM_BUS)
static struct platform_driver gf_driver = {
#endif
	.driver = {
		.name = GF_DEV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = gx_match_table,
	},
	.probe = gf_probe,
	.remove = gf_remove,
};

static int __init gf_init(void)
{
	int status;
	/* Claim our 256 reserved device numbers.  Then register a class
	 * that will key udev/mdev to add/remove /dev nodes.  Last, register
	 * the driver which manages those device numbers.
	 */
	BUILD_BUG_ON(N_SPI_MINORS > 256);
	status = register_chrdev(SPIDEV_MAJOR, CHRD_DRIVER_NAME, &gf_fops);

	if (status < 0) {
		pr_warn("Failed to register char device!\n");
		return status;
	}

	SPIDEV_MAJOR = status;
	gf_class = class_create(THIS_MODULE, CLASS_NAME);

	if (IS_ERR(gf_class)) {
		unregister_chrdev(SPIDEV_MAJOR, gf_driver.driver.name);
		pr_warn("Failed to create class.\n");
		return PTR_ERR(gf_class);
	}

#if defined(USE_PLATFORM_BUS)
	status = platform_driver_register(&gf_driver);
#elif defined(USE_SPI_BUS)
	status = spi_register_driver(&gf_driver);
#endif

	if (status < 0) {
		class_destroy(gf_class);
		unregister_chrdev(SPIDEV_MAJOR, gf_driver.driver.name);
		pr_warn("Failed to register SPI driver.\n");
	}

#ifdef GF_NETLINK_ENABLE
	netlink_init();
#endif
	pr_debug("status = 0x%x\n", status);
	return 0;
}
module_init(gf_init);

static void __exit gf_exit(void)
{
#ifdef GF_NETLINK_ENABLE
	netlink_exit();
#endif
#if defined(USE_PLATFORM_BUS)
	platform_driver_unregister(&gf_driver);
#elif defined(USE_SPI_BUS)
	spi_unregister_driver(&gf_driver);
#endif
	class_destroy(gf_class);
	unregister_chrdev(SPIDEV_MAJOR, gf_driver.driver.name);
}
module_exit(gf_exit);

MODULE_AUTHOR("Jiangtao Yi, <yijiangtao@goodix.com>");
MODULE_AUTHOR("Jandy Gou, <gouqingsong@goodix.com>");
MODULE_DESCRIPTION("goodix fingerprint sensor device driver");
MODULE_LICENSE("GPL");
