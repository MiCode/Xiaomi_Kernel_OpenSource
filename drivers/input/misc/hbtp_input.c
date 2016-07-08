
/* Copyright (c) 2014-2016, The Linux Foundation. All rights reserved.
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

#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/input/mt.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <uapi/linux/hbtp_input.h>
#include "../input-compat.h"
#if defined(CONFIG_HBTP_INPUT_SECURE_TOUCH)
#include <linux/gpio.h>
#include <linux/interrupt.h>
#endif
#include <linux/of_gpio.h>

#if defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#endif

#define HBTP_INPUT_NAME			"hbtp_input"
#define DISP_COORDS_SIZE		2

struct hbtp_data {
	struct platform_device *pdev;
	struct input_dev *input_dev;
	s32 count;
	struct mutex mutex;
	bool touch_status[HBTP_MAX_FINGER];
#if defined(CONFIG_FB)
	struct notifier_block fb_notif;
#endif
	struct regulator *vcc_ana;
	struct regulator *vcc_dig;
	int afe_load_ua;
	int afe_vtg_min_uv;
	int afe_vtg_max_uv;
	int dig_load_ua;
	int dig_vtg_min_uv;
	int dig_vtg_max_uv;
	int disp_maxx;		/* Display Max X */
	int disp_maxy;		/* Display Max Y */
	int def_maxx;		/* Default Max X */
	int def_maxy;		/* Default Max Y */
	int des_maxx;		/* Desired Max X */
	int des_maxy;		/* Desired Max Y */
	int gpio_irq;
	u32 irq_gpio_flags;
#if defined(CONFIG_HBTP_INPUT_SECURE_TOUCH)
	int irq_num;
	atomic_t st_enabled;
	atomic_t st_pending_irqs;
	struct completion st_powerdown;
	struct completion st_irq_processed;
	struct completion st_userspace_task;
	bool st_initialized;
#endif
	bool use_scaling;
	bool override_disp_coords;
	bool manage_afe_power_ana;
	bool manage_power_dig;
};

static struct hbtp_data *hbtp;

#if defined(CONFIG_HBTP_INPUT_SECURE_TOUCH)
static irqreturn_t hbtp_filter_interrupt(int irq, void *context)
{
	if (atomic_read(&hbtp->st_enabled)) {
		if (atomic_cmpxchg(&hbtp->st_pending_irqs, 0, 1) == 0) {
			reinit_completion(&hbtp->st_irq_processed);
			sysfs_notify(&hbtp->pdev->dev.kobj, NULL,
							"secure_touch");
			wait_for_completion_interruptible(
					&hbtp->st_irq_processed);
		}
		return IRQ_HANDLED;
	}
	return IRQ_NONE;
}

static void hbtp_secure_touch_init(struct hbtp_data *hbtp)
{
	hbtp->st_initialized = 0;
	init_completion(&hbtp->st_powerdown);
	init_completion(&hbtp->st_irq_processed);
	init_completion(&hbtp->st_userspace_task);
	hbtp->st_initialized = 1;
}

/*
 * 'blocking' variable will have value 'true' when we want to prevent the driver
 * from accessing the xPU/SMMU protected HW resources while the session is
 * active.
 */
static void hbtp_secure_touch_stop(struct hbtp_data *hbtp, bool blocking)
{
	if (atomic_read(&hbtp->st_enabled)) {
		atomic_set(&hbtp->st_pending_irqs, -1);
		sysfs_notify(&hbtp->pdev->dev.kobj, NULL, "secure_touch");
		if (blocking)
			wait_for_completion_interruptible(
					&hbtp->st_powerdown);
	}
	dev_dbg(hbtp->pdev->dev.parent, "Secure Touch session stopped\n");
}

static int hbtp_gpio_configure(struct hbtp_data *hbtp, bool on)
{
	int retval = 0;

	if (on) {
		if (gpio_is_valid(hbtp->gpio_irq)) {
			retval = gpio_request(hbtp->gpio_irq, "hbtp_irq");
			if (retval) {
				dev_err(hbtp->pdev->dev.parent,
					"unable to request GPIO [%d]\n",
					hbtp->gpio_irq);
				goto err_gpio_irq_req;
			}

			retval = gpio_direction_input(hbtp->gpio_irq);
			if (retval) {
				dev_err(hbtp->pdev->dev.parent,
					"unable to set dir for GPIO[%d]\n",
					hbtp->gpio_irq);
				goto err_gpio_irq_dir;
			}
		} else {
			dev_err(hbtp->pdev->dev.parent,
					"GPIO [%d] is not valid\n",
					hbtp->gpio_irq);
		}
	} else {
		if (gpio_is_valid(hbtp->gpio_irq))
			gpio_free(hbtp->gpio_irq);
	}

	return 0;

err_gpio_irq_dir:
	if (gpio_is_valid(hbtp->gpio_irq))
		gpio_free(hbtp->gpio_irq);
err_gpio_irq_req:
	return retval;
}

static ssize_t hbtp_secure_touch_enable_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d", atomic_read(&hbtp->st_enabled));
}

/*
 * Accept only "0" and "1" valid values.
 * "0" will reset the st_enabled flag, then wake up the reading process and
 * the interrupt handler.
 * The bus driver is notified via pm_runtime that it is not required to stay
 * awake anymore.
 * It will also make sure the queue of events is emptied in the controller,
 * in case a touch happened in between the secure touch being disabled and
 * the local ISR being ungated.
 * "1" will set the st_enabled flag and clear the st_pending_irqs flag.
 * The bus driver is requested via pm_runtime to stay awake.
 */
static ssize_t hbtp_secure_touch_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u8 value;
	int err = 0, retval;

	if (!hbtp->input_dev || !hbtp->st_initialized)
		return -EIO;

	if (count > 2)
		return -EINVAL;
	err = kstrtou8(buf, 10, &value);
	if (err != 0)
		return err;

	err = count;

	switch (value) {
	case 0:
		if (atomic_read(&hbtp->st_enabled) == 0)
			break;
		atomic_set(&hbtp->st_enabled, 0);
		complete(&hbtp->st_irq_processed);
		complete(&hbtp->st_powerdown);
		disable_irq(hbtp->irq_num);
		free_irq(hbtp->irq_num, hbtp);
		hbtp_gpio_configure(hbtp, false);
		reinit_completion(&hbtp->st_userspace_task);
		sysfs_notify(&hbtp->pdev->dev.kobj, NULL,
							"secure_touch_enable");
		wait_for_completion_interruptible(&hbtp->st_userspace_task);
		sysfs_notify(&hbtp->pdev->dev.kobj, NULL, "secure_touch");
		break;
	case 1:
		if (atomic_read(&hbtp->st_enabled)) {
			err = -EBUSY;
			break;
		}
		reinit_completion(&hbtp->st_powerdown);
		reinit_completion(&hbtp->st_irq_processed);
		reinit_completion(&hbtp->st_userspace_task);
		atomic_set(&hbtp->st_enabled, 1);
		sysfs_notify(&hbtp->pdev->dev.kobj, NULL,
							"secure_touch_enable");
		atomic_set(&hbtp->st_pending_irqs,  0);
		wait_for_completion_interruptible(&hbtp->st_userspace_task);
		retval = hbtp_gpio_configure(hbtp, true);
		if (retval)
			return retval;

		hbtp->irq_num = gpio_to_irq(hbtp->gpio_irq);
		retval = request_threaded_irq(hbtp->irq_num, NULL,
			hbtp_filter_interrupt,
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
			"hbtp", hbtp);
		if (retval < 0) {
			dev_err(hbtp->pdev->dev.parent,
				"%s: Failed to create irq thread ", __func__);
			return retval;
		}
		break;
	default:
		dev_err(hbtp->pdev->dev.parent,
			"unsupported value: %d\n", value);
		err = -EINVAL;
		break;
	}
	return err;
}

/*
 * Accept only "0" and "1" valid values.
 * "0" will reset the st_enabled flag, then wake up the reading process and
 * the interrupt handler.
 * The bus driver is notified via pm_runtime that it is not required to stay
 * awake anymore.
 * It will also make sure the queue of events is emptied in the controller,
 * in case a touch happened in between the secure touch being disabled and
 * the local ISR being ungated.
 * "1" will set the st_enabled flag and clear the st_pending_irqs flag.
 * The bus driver is requested via pm_runtime to stay awake.
 */
static ssize_t hbtp_secure_touch_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int val = 0;

	if (atomic_read(&hbtp->st_enabled) == 0)
		return -EBADF;
	if (atomic_cmpxchg(&hbtp->st_pending_irqs, -1, 0) == -1)
		return -EINVAL;
	if (atomic_cmpxchg(&hbtp->st_pending_irqs, 1, 0) == 1)
		val = 1;
	else
		complete(&hbtp->st_irq_processed);
	return scnprintf(buf, PAGE_SIZE, "%u", val);
}

static ssize_t hbtp_secure_touch_userspace_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u8 value;
	int err = 0;

	if (!hbtp->input_dev)
		return -EIO;

	if (count > 2)
		return -EINVAL;
	err = kstrtou8(buf, 10, &value);
	if (err != 0)
		return err;

	if (value == 0 || value == 1) {
		dev_dbg(hbtp->pdev->dev.parent,
			"%s: Userspace is %s\n", __func__,
			(value == 0) ? "started" : "stopped");
		complete(&hbtp->st_userspace_task);
	} else {
		dev_err(hbtp->pdev->dev.parent,
			"%s: Wrong value sent\n", __func__);
	}

	return count;
}

static DEVICE_ATTR(secure_touch_enable, S_IRUGO | S_IWUSR | S_IWGRP,
		hbtp_secure_touch_enable_show, hbtp_secure_touch_enable_store);
static DEVICE_ATTR(secure_touch, S_IRUGO, hbtp_secure_touch_show, NULL);
static DEVICE_ATTR(secure_touch_userspace, S_IRUGO | S_IWUSR | S_IWGRP,
		hbtp_secure_touch_enable_show,
		hbtp_secure_touch_userspace_store);
#else
static void hbtp_secure_touch_init(struct hbtp_data *hbtp)
{
}
static void hbtp_secure_touch_stop(struct hbtp_data *data, bool blocking)
{
}
#endif

static struct attribute *secure_touch_attrs[] = {
#if defined(CONFIG_HBTP_INPUT_SECURE_TOUCH)
	&dev_attr_secure_touch_enable.attr,
	&dev_attr_secure_touch.attr,
	&dev_attr_secure_touch_userspace.attr,
	NULL
#endif
};

static const struct attribute_group secure_touch_attr_group = {
	.attrs = secure_touch_attrs,
};

#if defined(CONFIG_FB)
static int fb_notifier_callback(struct notifier_block *self,
				 unsigned long event, void *data)
{
	int blank;
	struct fb_event *evdata = data;
	struct hbtp_data *hbtp_data =
		container_of(self, struct hbtp_data, fb_notif);
	char *envp[2] = {HBTP_EVENT_TYPE_DISPLAY, NULL};

	if (evdata && evdata->data && event == FB_EVENT_BLANK &&
		hbtp_data && hbtp_data->input_dev) {
		blank = *(int *)(evdata->data);
		if (blank == FB_BLANK_UNBLANK)
			kobject_uevent_env(&hbtp_data->input_dev->dev.kobj,
					KOBJ_ONLINE, envp);
		else if (blank == FB_BLANK_POWERDOWN) {
			hbtp_secure_touch_stop(hbtp, true);
			kobject_uevent_env(&hbtp_data->input_dev->dev.kobj,
					KOBJ_OFFLINE, envp);
		}
	}

	return 0;
}
#endif

static int hbtp_input_open(struct inode *inode, struct file *file)
{
	mutex_lock(&hbtp->mutex);
	if (hbtp->count) {
		pr_err("%s is busy\n", HBTP_INPUT_NAME);
		mutex_unlock(&hbtp->mutex);
		return -EBUSY;
	}
	hbtp->count++;
	mutex_unlock(&hbtp->mutex);

	return 0;
}

static int hbtp_input_release(struct inode *inode, struct file *file)
{
	mutex_lock(&hbtp->mutex);
	if (!hbtp->count) {
		pr_err("%s wasn't opened\n", HBTP_INPUT_NAME);
		mutex_unlock(&hbtp->mutex);
		return -ENOTTY;
	}
	hbtp->count--;
	mutex_unlock(&hbtp->mutex);

	return 0;
}

static int hbtp_input_create_input_dev(struct hbtp_input_absinfo *absinfo)
{
	struct input_dev *input_dev;
	struct hbtp_input_absinfo *abs;
	int i, error;

	input_dev = input_allocate_device();
	if (!input_dev) {
		pr_err("%s: input_allocate_device failed\n", __func__);
		return -ENOMEM;
	}

	kfree(input_dev->name);
	input_dev->name = kstrndup(HBTP_INPUT_NAME, sizeof(HBTP_INPUT_NAME),
					GFP_KERNEL);
	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(BTN_TOUCH, input_dev->keybit);
	__set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

	for (i = KEY_HOME; i <= KEY_MICMUTE; i++)
		__set_bit(i, input_dev->keybit);

	/* For multi touch */
	input_mt_init_slots(input_dev, HBTP_MAX_FINGER, 0);
	for (i = 0; i <= ABS_MT_LAST - ABS_MT_FIRST; i++) {
		abs = absinfo + i;
		if (abs->active)
			input_set_abs_params(input_dev, abs->code,
					abs->minimum, abs->maximum, 0, 0);
	}

	if (hbtp->override_disp_coords) {
		input_set_abs_params(input_dev, ABS_MT_POSITION_X,
					0, hbtp->disp_maxx, 0, 0);
		input_set_abs_params(input_dev, ABS_MT_POSITION_Y,
					0, hbtp->disp_maxy, 0, 0);
	}

	error = input_register_device(input_dev);
	if (error) {
		pr_err("%s: input_register_device failed\n", __func__);
		goto err_input_reg_dev;
	}

	hbtp->input_dev = input_dev;
	return 0;

err_input_reg_dev:
	input_free_device(input_dev);

	return error;
}

static int hbtp_input_report_events(struct hbtp_data *hbtp_data,
				struct hbtp_input_mt *mt_data)
{
	int i;
	struct hbtp_input_touch *tch;

	for (i = 0; i < HBTP_MAX_FINGER; i++) {
		tch = &(mt_data->touches[i]);
		if (tch->active || hbtp_data->touch_status[i]) {
			input_mt_slot(hbtp_data->input_dev, i);
			input_mt_report_slot_state(hbtp_data->input_dev,
					MT_TOOL_FINGER, tch->active);

			if (tch->active) {
				input_report_abs(hbtp_data->input_dev,
						ABS_MT_TOOL_TYPE,
						tch->tool);
				input_report_abs(hbtp_data->input_dev,
						ABS_MT_TOUCH_MAJOR,
						tch->major);
				input_report_abs(hbtp_data->input_dev,
						ABS_MT_TOUCH_MINOR,
						tch->minor);
				input_report_abs(hbtp_data->input_dev,
						ABS_MT_ORIENTATION,
						tch->orientation);
				input_report_abs(hbtp_data->input_dev,
						ABS_MT_PRESSURE,
						tch->pressure);
				/*
				 * Scale up/down the X-coordinate as per
				 * DT property
				 */
				if (hbtp_data->use_scaling &&
						hbtp_data->def_maxx > 0 &&
						hbtp_data->des_maxx > 0)
					tch->x = (tch->x * hbtp_data->des_maxx)
							/ hbtp_data->def_maxx;

				input_report_abs(hbtp_data->input_dev,
						ABS_MT_POSITION_X,
						tch->x);
				/*
				 * Scale up/down the Y-coordinate as per
				 * DT property
				 */
				if (hbtp_data->use_scaling &&
						hbtp_data->def_maxy > 0 &&
						hbtp_data->des_maxy > 0)
					tch->y = (tch->y * hbtp_data->des_maxy)
							/ hbtp_data->def_maxy;

				input_report_abs(hbtp_data->input_dev,
						ABS_MT_POSITION_Y,
						tch->y);
			}
			hbtp_data->touch_status[i] = tch->active;
		}
	}

	input_report_key(hbtp->input_dev, BTN_TOUCH, mt_data->num_touches > 0);
	input_sync(hbtp->input_dev);

	return 0;
}

static int reg_set_optimum_mode_check(struct regulator *reg, int load_uA)
{
	return (regulator_count_voltages(reg) > 0) ?
		regulator_set_optimum_mode(reg, load_uA) : 0;
}

static int hbtp_pdev_power_on(struct hbtp_data *hbtp, bool on)
{
	int ret;

	if (!hbtp->vcc_ana)
		pr_err("%s: analog regulator is not available\n", __func__);

	if (!hbtp->vcc_dig)
		pr_err("%s: digital regulator is not available\n", __func__);

	if (!hbtp->vcc_ana && !hbtp->vcc_dig) {
		pr_err("%s: no regulators available\n", __func__);
		return -EINVAL;
	}

	if (!on)
		goto reg_off;

	if (hbtp->vcc_ana) {
		ret = reg_set_optimum_mode_check(hbtp->vcc_ana,
			hbtp->afe_load_ua);
		if (ret < 0) {
			pr_err("%s: Regulator vcc_ana set_opt failed rc=%d\n",
				__func__, ret);
			return ret;
		}

		ret = regulator_enable(hbtp->vcc_ana);
		if (ret) {
			pr_err("%s: Regulator vcc_ana enable failed rc=%d\n",
				__func__, ret);
			reg_set_optimum_mode_check(hbtp->vcc_ana, 0);
			return ret;
		}
	}
	if (hbtp->vcc_dig) {
		ret = reg_set_optimum_mode_check(hbtp->vcc_dig,
			hbtp->dig_load_ua);
		if (ret < 0) {
			pr_err("%s: Regulator vcc_dig set_opt failed rc=%d\n",
				__func__, ret);
			return ret;
		}

		ret = regulator_enable(hbtp->vcc_dig);
		if (ret) {
			pr_err("%s: Regulator vcc_dig enable failed rc=%d\n",
				__func__, ret);
			reg_set_optimum_mode_check(hbtp->vcc_dig, 0);
			return ret;
		}
	}

	return 0;

reg_off:
	if (hbtp->vcc_ana) {
		reg_set_optimum_mode_check(hbtp->vcc_ana, 0);
		regulator_disable(hbtp->vcc_ana);
	}
	if (hbtp->vcc_dig) {
		reg_set_optimum_mode_check(hbtp->vcc_dig, 0);
		regulator_disable(hbtp->vcc_dig);
	}
	return 0;
}

static long hbtp_input_ioctl_handler(struct file *file, unsigned int cmd,
				 unsigned long arg, void __user *p)
{
	int error = 0;
	struct hbtp_input_mt mt_data;
	struct hbtp_input_absinfo absinfo[ABS_MT_LAST - ABS_MT_FIRST + 1];
	struct hbtp_input_key key_data;
	enum hbtp_afe_power_cmd power_cmd;

	switch (cmd) {
	case HBTP_SET_ABSPARAM:
		if (hbtp && hbtp->input_dev) {
			pr_err("%s: The input device is already created\n",
				__func__);
			return 0;
		}

		if (copy_from_user(absinfo, (void *)arg,
					sizeof(struct hbtp_input_absinfo) *
					(ABS_MT_LAST - ABS_MT_FIRST + 1))) {
			pr_err("%s: Error copying data for ABS param\n",
				__func__);
			return -EFAULT;
		}

		error = hbtp_input_create_input_dev(absinfo);
		if (error)
			pr_err("%s, hbtp_input_create_input_dev failed (%d)\n",
				__func__, error);
		break;

	case HBTP_SET_TOUCHDATA:
		if (!hbtp || !hbtp->input_dev) {
			pr_err("%s: The input device hasn't been created\n",
				__func__);
			return -EFAULT;
		}

		if (copy_from_user(&mt_data, (void *)arg,
					sizeof(struct hbtp_input_mt))) {
			pr_err("%s: Error copying data\n", __func__);
			return -EFAULT;
		}

		hbtp_input_report_events(hbtp, &mt_data);
		error = 0;
		break;

	case HBTP_SET_POWERSTATE:
		if (!hbtp || !hbtp->input_dev) {
			pr_err("%s: The input device hasn't been created\n",
				__func__);
			return -EFAULT;
		}

		if (copy_from_user(&power_cmd, (void *)arg,
					sizeof(enum hbtp_afe_power_cmd))) {
			pr_err("%s: Error copying data\n", __func__);
			return -EFAULT;
		}

		switch (power_cmd) {
		case HBTP_AFE_POWER_ON:
			error = hbtp_pdev_power_on(hbtp, true);
			if (error)
				pr_err("%s: failed to power on\n", __func__);
			break;
		case HBTP_AFE_POWER_OFF:
			error = hbtp_pdev_power_on(hbtp, false);
			if (error)
				pr_err("%s: failed to power off\n", __func__);
			break;
		default:
			pr_err("%s: Unsupported command for power state, %d\n",
				__func__, power_cmd);
			return -EINVAL;
		}
		break;

	case HBTP_SET_KEYDATA:
		if (!hbtp || !hbtp->input_dev) {
			pr_err("%s: The input device hasn't been created\n",
				__func__);
			return -EFAULT;
		}

		if (copy_from_user(&key_data, (void *)arg,
					sizeof(struct hbtp_input_key))) {
			pr_err("%s: Error copying data for key info\n",
				__func__);
			return -EFAULT;
		}

		input_report_key(hbtp->input_dev, key_data.code,
				key_data.value);
		input_sync(hbtp->input_dev);
		break;

	default:
		pr_err("%s: Unsupported ioctl command %u\n", __func__, cmd);
		error = -EINVAL;
		break;
	}

	return error;
}

static long hbtp_input_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	return hbtp_input_ioctl_handler(file, cmd, arg, (void __user *)arg);
}

#ifdef CONFIG_COMPAT
static long hbtp_input_compat_ioctl(struct file *file, unsigned int cmd,
					unsigned long arg)
{
	return hbtp_input_ioctl_handler(file, cmd, arg, compat_ptr(arg));
}
#endif

static const struct file_operations hbtp_input_fops = {
	.owner		= THIS_MODULE,
	.open		= hbtp_input_open,
	.release	= hbtp_input_release,
	.unlocked_ioctl	= hbtp_input_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= hbtp_input_compat_ioctl,
#endif
};

static struct miscdevice hbtp_input_misc = {
	.fops		= &hbtp_input_fops,
	.minor		= MISC_DYNAMIC_MINOR,
	.name		= HBTP_INPUT_NAME,
};
MODULE_ALIAS_MISCDEV(MISC_DYNAMIC_MINOR);
MODULE_ALIAS("devname:" HBTP_INPUT_NAME);

#ifdef CONFIG_OF
static int hbtp_parse_dt(struct device *dev)
{
	int rc, size;
	struct device_node *np = dev->of_node;
	struct property *prop;
	u32 temp_val;
	u32 disp_reso[DISP_COORDS_SIZE];

	if (of_find_property(np, "vcc_ana-supply", NULL))
		hbtp->manage_afe_power_ana = true;
	if (of_find_property(np, "vcc_dig-supply", NULL))
		hbtp->manage_power_dig = true;

	if (hbtp->manage_afe_power_ana) {
		rc = of_property_read_u32(np, "qcom,afe-load", &temp_val);
		if (!rc) {
			hbtp->afe_load_ua = (int) temp_val;
		} else {
			dev_err(dev, "Unable to read AFE load\n");
			return rc;
		}

		rc = of_property_read_u32(np, "qcom,afe-vtg-min", &temp_val);
		if (!rc) {
			hbtp->afe_vtg_min_uv = (int) temp_val;
		} else {
			dev_err(dev, "Unable to read AFE min voltage\n");
			return rc;
		}

		rc = of_property_read_u32(np, "qcom,afe-vtg-max", &temp_val);
		if (!rc) {
			hbtp->afe_vtg_max_uv = (int) temp_val;
		} else {
			dev_err(dev, "Unable to read AFE max voltage\n");
			return rc;
		}
	}
	if (hbtp->manage_power_dig) {
		rc = of_property_read_u32(np, "qcom,dig-load", &temp_val);
		if (!rc) {
			hbtp->dig_load_ua = (int) temp_val;
		} else {
			dev_err(dev, "Unable to read digital load\n");
			return rc;
		}

		rc = of_property_read_u32(np, "qcom,dig-vtg-min", &temp_val);
		if (!rc) {
			hbtp->dig_vtg_min_uv = (int) temp_val;
		} else {
			dev_err(dev, "Unable to read digital min voltage\n");
			return rc;
		}

		rc = of_property_read_u32(np, "qcom,dig-vtg-max", &temp_val);
		if (!rc) {
			hbtp->dig_vtg_max_uv = (int) temp_val;
		} else {
			dev_err(dev, "Unable to read digital max voltage\n");
			return rc;
		}
	}

	prop = of_find_property(np, "qcom,display-resolution", NULL);
	if (prop != NULL) {
		if (!prop->value)
			return -ENODATA;

		size = prop->length / sizeof(u32);
		if (size != DISP_COORDS_SIZE) {
			dev_err(dev, "invalid qcom,display-resolution DT property\n");
			return -EINVAL;
		}

		rc = of_property_read_u32_array(np, "qcom,display-resolution",
							disp_reso, size);
		if (rc && (rc != -EINVAL)) {
			dev_err(dev, "Unable to read DT property qcom,display-resolution\n");
			return rc;
		}

		hbtp->disp_maxx = disp_reso[0];
		hbtp->disp_maxy = disp_reso[1];

		hbtp->override_disp_coords = true;
	}

	hbtp->gpio_irq = of_get_named_gpio_flags(np,
				"hbtp,irq-gpio", 0, &hbtp->irq_gpio_flags);

	hbtp->use_scaling = of_property_read_bool(np, "qcom,use-scale");
	if (hbtp->use_scaling) {
		rc = of_property_read_u32(np, "qcom,default-max-x", &temp_val);
		if (!rc) {
			hbtp->def_maxx = (int) temp_val;
		} else if (rc && (rc != -EINVAL)) {
			dev_err(dev, "Unable to read default max x\n");
			return rc;
		}

		rc = of_property_read_u32(np, "qcom,desired-max-x", &temp_val);
		if (!rc) {
			hbtp->des_maxx = (int) temp_val;
		} else if (rc && (rc != -EINVAL)) {
			dev_err(dev, "Unable to read desired max x\n");
			return rc;
		}

		/*
		 * Either both DT properties i.e. Default max X and
		 * Desired max X should be defined simultaneously, or none
		 * of them should be defined.
		 */
		if ((hbtp->def_maxx == 0 && hbtp->des_maxx != 0) ||
				(hbtp->def_maxx != 0 && hbtp->des_maxx == 0)) {
			dev_err(dev, "default or desired max-X properties are incorrect\n");
			return -EINVAL;
		}

		rc = of_property_read_u32(np, "qcom,default-max-y", &temp_val);
		if (!rc) {
			hbtp->def_maxy = (int) temp_val;
		} else if (rc && (rc != -EINVAL)) {
			dev_err(dev, "Unable to read default max y\n");
			return rc;
		}

		rc = of_property_read_u32(np, "qcom,desired-max-y", &temp_val);
		if (!rc) {
			hbtp->des_maxy = (int) temp_val;
		} else if (rc && (rc != -EINVAL)) {
			dev_err(dev, "Unable to read desired max y\n");
			return rc;
		}

		/*
		 * Either both DT properties i.e. Default max X and
		 * Desired max X should be defined simultaneously, or none
		 * of them should be defined.
		 */
		if ((hbtp->def_maxy == 0 && hbtp->des_maxy != 0) ||
				(hbtp->def_maxy != 0 && hbtp->des_maxy == 0)) {
			dev_err(dev, "default or desired max-Y properties are incorrect\n");
			return -EINVAL;
		}

	}

	return 0;
}
#else
static int hbtp_parse_dt(struct device *dev)
{
	return -ENODEV;
}
#endif

static int hbtp_pdev_probe(struct platform_device *pdev)
{
	int error;
	struct regulator *vcc_ana, *vcc_dig;

	if (pdev->dev.of_node) {
		error = hbtp_parse_dt(&pdev->dev);
		if (error) {
			pr_err("%s: parse dt failed, rc=%d\n", __func__, error);
			return error;
		}
	}

	if (hbtp->manage_afe_power_ana) {
		vcc_ana = regulator_get(&pdev->dev, "vcc_ana");
		if (IS_ERR(vcc_ana)) {
			error = PTR_ERR(vcc_ana);
			pr_err("%s: regulator get failed vcc_ana rc=%d\n",
				__func__, error);
			return error;
		}

		if (regulator_count_voltages(vcc_ana) > 0) {
			error = regulator_set_voltage(vcc_ana,
				hbtp->afe_vtg_min_uv, hbtp->afe_vtg_max_uv);
			if (error) {
				pr_err("%s: regulator set vtg failed vcc_ana rc=%d\n",
					__func__, error);
				regulator_put(vcc_ana);
				return error;
			}
		}
		hbtp->vcc_ana = vcc_ana;
	}

	if (hbtp->manage_power_dig) {
		vcc_dig = regulator_get(&pdev->dev, "vcc_dig");
		if (IS_ERR(vcc_dig)) {
			error = PTR_ERR(vcc_dig);
			pr_err("%s: regulator get failed vcc_dig rc=%d\n",
				__func__, error);
			return error;
		}

		if (regulator_count_voltages(vcc_dig) > 0) {
			error = regulator_set_voltage(vcc_dig,
				hbtp->dig_vtg_min_uv, hbtp->dig_vtg_max_uv);
			if (error) {
				pr_err("%s: regulator set vtg failed vcc_dig rc=%d\n",
					__func__, error);
				regulator_put(vcc_dig);
				return error;
			}
		}
		hbtp->vcc_dig = vcc_dig;
	}

	hbtp->pdev = pdev;
	error = sysfs_create_group(&hbtp->pdev->dev.kobj,
					&secure_touch_attr_group);
	if (error) {
		dev_err(&pdev->dev, "Failed to create sysfs entries\n");
		goto err_sysfs_create_group;
	}

	hbtp_secure_touch_init(hbtp);
	hbtp_secure_touch_stop(hbtp, true);

	return 0;

err_sysfs_create_group:
	if (hbtp->manage_power_dig)
		regulator_put(vcc_dig);
	if (hbtp->manage_afe_power_ana)
		regulator_put(vcc_ana);
	return error;
}

static int hbtp_pdev_remove(struct platform_device *pdev)
{
	sysfs_remove_group(&hbtp->pdev->dev.kobj, &secure_touch_attr_group);
	if (hbtp->vcc_ana || hbtp->vcc_dig) {
		hbtp_pdev_power_on(hbtp, false);
		if (hbtp->vcc_ana)
			regulator_put(hbtp->vcc_ana);
		if (hbtp->vcc_dig)
			regulator_put(hbtp->vcc_dig);
	}

	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id hbtp_match_table[] = {
	{ .compatible = "qcom,hbtp-input",},
	{ },
};
#else
#define hbtp_match_table NULL
#endif

static struct platform_driver hbtp_pdev_driver = {
	.probe		= hbtp_pdev_probe,
	.remove		= hbtp_pdev_remove,
	.driver		= {
		.name		= "hbtp",
		.owner		= THIS_MODULE,
		.of_match_table = hbtp_match_table,
	},
};

static int __init hbtp_init(void)
{
	int error;

	hbtp = kzalloc(sizeof(struct hbtp_data), GFP_KERNEL);
	if (!hbtp)
		return -ENOMEM;

	mutex_init(&hbtp->mutex);

	error = misc_register(&hbtp_input_misc);
	if (error) {
		pr_err("%s: misc_register failed\n", HBTP_INPUT_NAME);
		goto err_misc_reg;
	}

#if defined(CONFIG_FB)
	hbtp->fb_notif.notifier_call = fb_notifier_callback;
	error = fb_register_client(&hbtp->fb_notif);
	if (error) {
		pr_err("%s: Unable to register fb_notifier: %d\n",
			HBTP_INPUT_NAME, error);
		goto err_fb_reg;
	}
#endif

	error = platform_driver_register(&hbtp_pdev_driver);
	if (error) {
		pr_err("Failed to register platform driver: %d\n", error);
		goto err_platform_drv_reg;
	}

	return 0;

err_platform_drv_reg:
#if defined(CONFIG_FB)
	fb_unregister_client(&hbtp->fb_notif);
err_fb_reg:
#endif
	misc_deregister(&hbtp_input_misc);
err_misc_reg:
	kfree(hbtp);

	return error;
}

static void __exit hbtp_exit(void)
{
	misc_deregister(&hbtp_input_misc);
	if (hbtp->input_dev)
		input_unregister_device(hbtp->input_dev);

#if defined(CONFIG_FB)
	fb_unregister_client(&hbtp->fb_notif);
#endif

	platform_driver_unregister(&hbtp_pdev_driver);

	kfree(hbtp);
}

MODULE_DESCRIPTION("Kernel driver to support host based touch processing");
MODULE_LICENSE("GPLv2");

module_init(hbtp_init);
module_exit(hbtp_exit);
