
/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
#include <linux/hbtp_input.h>
#include <linux/input/mt.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include "../input-compat.h"

#if defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#endif

#define HBTP_INPUT_NAME			"hbtp_input"
#define HBTP_AFE_LOAD_UA		150000
#define HBTP_AFE_VTG_MIN_UV		2700000
#define HBTP_AFE_VTG_MAX_UV		3300000

struct hbtp_data {
	struct platform_device *pdev;
	struct input_dev *input_dev;
	s32 count;
	bool touch_status[HBTP_MAX_FINGER];
#if defined(CONFIG_FB)
	struct notifier_block fb_notif;
#endif
	struct regulator *vcc_ana;
};

static struct hbtp_data *hbtp;

#if defined(CONFIG_FB)
static int fb_notifier_callback(struct notifier_block *self,
				 unsigned long event, void *data)
{
	int blank;
	struct fb_event *evdata = data;
	struct hbtp_data *hbtp_data =
		container_of(self, struct hbtp_data, fb_notif);

	if (evdata && evdata->data && event == FB_EVENT_BLANK &&
		hbtp_data && hbtp_data->input_dev) {
		blank = *(int *)(evdata->data);
		if (blank == FB_BLANK_UNBLANK)
			kobject_uevent(&hbtp_data->input_dev->dev.kobj,
					KOBJ_ONLINE);
		else if (blank == FB_BLANK_POWERDOWN)
			kobject_uevent(&hbtp_data->input_dev->dev.kobj,
					KOBJ_OFFLINE);
	}

	return 0;
}
#endif

static int hbtp_input_open(struct inode *inode, struct file *file)
{
	if (hbtp->count) {
		pr_err("%s is busy\n", HBTP_INPUT_NAME);
		return -EBUSY;
	}
	hbtp->count++;

	return 0;
}

static int hbtp_input_release(struct inode *inode, struct file *file)
{
	if (!hbtp->count) {
		pr_err("%s wasn't opened\n", HBTP_INPUT_NAME);
		return -ENOTTY;
	}
	hbtp->count--;

	return 0;
}

static int hbtp_input_create_input_dev(struct hbtp_input_absinfo *absinfo)
{
	struct input_dev *input_dev;
	struct hbtp_input_absinfo *abs;
	int error;
	int i;

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

	/* For multi touch */
	input_mt_init_slots(input_dev, HBTP_MAX_FINGER);
	for (i = 0; i <= ABS_MT_LAST - ABS_MT_FIRST; i++) {
		abs = absinfo + i;
		if (abs->active)
			input_set_abs_params(input_dev, abs->code,
					abs->minimum, abs->maximum, 0, 0);
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
				input_report_abs(hbtp_data->input_dev,
						ABS_MT_POSITION_X,
						tch->x);
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
	int ret, error;

	if (!hbtp->vcc_ana) {
		pr_err("%s: regulator is not available\n", __func__);
		return -EINVAL;
	}

	if (!on)
		goto reg_off;

	ret = reg_set_optimum_mode_check(hbtp->vcc_ana, HBTP_AFE_LOAD_UA);
	if (ret < 0) {
		pr_err("%s: Regulator vcc_ana set_opt failed rc=%d\n",
			__func__, ret);
		return -EINVAL;
	}

	ret = regulator_enable(hbtp->vcc_ana);
	if (ret) {
		pr_err("%s: Regulator vcc_ana enable failed rc=%d\n",
			__func__, ret);
		error = -EINVAL;
		goto error_reg_en_vcc_ana;
	}

	return 0;

error_reg_en_vcc_ana:
	reg_set_optimum_mode_check(hbtp->vcc_ana, 0);
	return error;

reg_off:
	reg_set_optimum_mode_check(hbtp->vcc_ana, 0);
	regulator_disable(hbtp->vcc_ana);
	return 0;
}

static long hbtp_input_ioctl_handler(struct file *file, unsigned int cmd,
				 unsigned long arg, void __user *p)
{
	int error;
	struct hbtp_input_mt mt_data;
	struct hbtp_input_absinfo absinfo[ABS_MT_LAST - ABS_MT_FIRST + 1];
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

static int __devinit hbtp_pdev_probe(struct platform_device *pdev)
{
	int ret, error;
	struct regulator *vcc_ana;

	vcc_ana = regulator_get(&pdev->dev, "vcc_ana");
	if (IS_ERR(vcc_ana)) {
		ret = PTR_ERR(vcc_ana);
		pr_err("%s: Regulator get failed vcc_ana rc=%d\n",
			__func__, ret);
		return -EINVAL;
	}

	if (regulator_count_voltages(vcc_ana) > 0) {
		ret = regulator_set_voltage(vcc_ana,
				HBTP_AFE_VTG_MIN_UV, HBTP_AFE_VTG_MAX_UV);
		if (ret) {
			pr_err("%s: regulator set_vtg failed rc=%d\n",
				__func__, ret);
			error = -EINVAL;
			goto error_set_vtg_vcc_ana;
		}
	}

	hbtp->vcc_ana = vcc_ana;
	hbtp->pdev = pdev;
	return 0;

error_set_vtg_vcc_ana:
	regulator_put(vcc_ana);

	return error;
};

static int __devexit hbtp_pdev_remove(struct platform_device *pdev)
{
	if (hbtp->vcc_ana) {
		hbtp_pdev_power_on(hbtp, false);
		regulator_put(hbtp->vcc_ana);
	}

	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id hbtp_match_table[] = {
	{ .compatible = "qcom,hbtp",},
	{ },
};
#else
#define hbtp_match_table NULL
#endif

static struct platform_driver hbtp_pdev_driver = {
	.probe		= hbtp_pdev_probe,
	.remove		= __devexit_p(hbtp_pdev_remove),
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
