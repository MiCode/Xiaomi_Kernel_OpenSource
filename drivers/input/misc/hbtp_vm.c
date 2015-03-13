/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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
#include <uapi/linux/hbtp_input.h>
#include <uapi/linux/hbtp_vm.h>

#define hbtp_vm_name    "hbtp_vm"

struct hbtp_virtual_mouse {
	struct input_dev *input_dev;
	s32 open_count;
	struct mutex mutex;
	bool touch_status[HBTP_MAX_FINGER];

	int enabled;
	int last_x;
	int last_y;
	int parking_dist_x;
};

static struct hbtp_virtual_mouse *hbtp_vm;

static int hbtp_vm_open(struct inode *inode, struct file *file)
{
	mutex_lock(&hbtp_vm->mutex);
	if (hbtp_vm->open_count)
		pr_debug("hbtp_vm was already opened\n");
	hbtp_vm->open_count++;
	mutex_unlock(&hbtp_vm->mutex);

	return 0;
}

static int hbtp_vm_release(struct inode *inode, struct file *file)
{
	mutex_lock(&hbtp_vm->mutex);
	if (!hbtp_vm->open_count) {
		pr_err("%s wasn't opened\n", hbtp_vm_name);
		mutex_unlock(&hbtp_vm->mutex);
		return -ENOTTY;
	}
	hbtp_vm->open_count--;
	mutex_unlock(&hbtp_vm->mutex);
	return 0;
}

static int hbtp_vm_create_input_dev(void)
{
	struct input_dev *input_dev;
	int error;

	input_dev = input_allocate_device();
	if (!input_dev) {
		pr_err("%s: input_allocate_device failed\n", __func__);
		return -ENOMEM;
	}

	kfree(input_dev->name);
	input_dev->name = kstrndup(hbtp_vm_name, sizeof(hbtp_vm_name),
				GFP_KERNEL);

	__set_bit(INPUT_PROP_DIRECT, input_dev->propbit);
	__set_bit(EV_REL, input_dev->evbit);
	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(BTN_LEFT, input_dev->keybit);
	__set_bit(BTN_RIGHT, input_dev->keybit);
	__set_bit(REL_HWHEEL, input_dev->relbit);

	input_dev->relbit[0] = BIT_MASK(REL_X) | BIT_MASK(REL_Y);

	error = input_register_device(input_dev);
	if (error) {
		pr_err("%s: input_register_device failed\n", __func__);
		goto err_input_reg_dev;
	}

	hbtp_vm->input_dev = input_dev;
	return 0;

err_input_reg_dev:
	input_free_device(input_dev);
	return error;
}

static int hbtp_vm_report_events(struct hbtp_virtual_mouse *hbtp_data,
			struct hbtp_input_mt *mt_data)
{
	struct hbtp_input_touch *tch;
	int dx = 0;
	int dy = 0;

	if (mt_data->num_touches > 1) {
		pr_err("virtual mouse received multi touch reports\n");
		return 0;
	}

	if (mt_data->num_touches == 1) {
		tch = &(mt_data->touches[0]);
		if (hbtp_vm->last_x != -1 && hbtp_vm->last_x != -1) {
			dx = tch->x - hbtp_vm->last_x;
			dy = tch->y - hbtp_vm->last_y;
		}

		if (!(dx == 0 && dy == 0)) {
			input_report_rel(hbtp_vm->input_dev, REL_X, dx);
			input_report_rel(hbtp_vm->input_dev, REL_Y, dy);
			input_sync(hbtp_vm->input_dev);
		}

		hbtp_vm->last_x = tch->x;
		hbtp_vm->last_y = tch->y;
	} else {
		hbtp_vm->last_x = -1;
		hbtp_vm->last_y = -1;
	}

	return 0;
}

static int hbtp_vm_report_clicks(struct hbtp_virtual_mouse *hbtp_data,
			struct hbtp_vm_click *click_data)
{
	unsigned int code = BTN_LEFT;
	int value = 1;

	if (click_data->mask & HBTP_VM_BUTTON_RIGHT)
		code = BTN_RIGHT;

	if (click_data->mask & HBTP_VM_BUTTON_UP)
		value = 0;

	input_report_key(hbtp_vm->input_dev, code, value);
	input_sync(hbtp_vm->input_dev);

	return 0;
}

static long hbtp_vm_ioctl(struct file *file, unsigned int cmd,
			unsigned long arg)
{
	int error = 0;
	struct hbtp_input_mt mt_data;
	struct hbtp_vm_click clik_data;

	if (!hbtp_vm) {
		pr_err("%s: virtual mouse driver not initialized\n",
					__func__);
		return -ENOMEM;
	}

	switch (cmd) {
	case HBTP_VM_ENABLE:
		if (hbtp_vm->enabled) {
			pr_err("virtual mouse device is already enabled\n");
			return 0;
		}
		hbtp_vm->enabled = true;
		input_report_rel(hbtp_vm->input_dev, REL_X, -2000);
		input_report_rel(hbtp_vm->input_dev, REL_Y, -2000);
		input_sync(hbtp_vm->input_dev);
		input_report_rel(hbtp_vm->input_dev, REL_X, 100);
		input_report_rel(hbtp_vm->input_dev, REL_Y, 100);
		input_sync(hbtp_vm->input_dev);

		break;

	case HBTP_VM_DISABLE:
		if (!hbtp_vm->enabled) {
			pr_err("virtual mouse device is already disabled\n");
			return 0;
		}
		hbtp_vm->enabled = false;
		input_report_rel(hbtp_vm->input_dev, REL_X, 2000);
		input_report_rel(hbtp_vm->input_dev, REL_Y, 2000);
		input_sync(hbtp_vm->input_dev);
		break;

	case HBTP_VM_SET_TOUCHDATA:
		if (!hbtp_vm->input_dev) {
			pr_err("%s: virtual mouse input device hasn't been created\n",
					__func__);
			return -EFAULT;
		}

		if (copy_from_user(&mt_data, (void *)arg,
		    sizeof(struct hbtp_input_mt))) {
			pr_err("%s: Error copying data\n", __func__);
			return -EFAULT;
		}

		hbtp_vm_report_events(hbtp_vm, &mt_data);
		error = 0;
		break;

	case HBTP_VM_SEND_CLICK:
		if (!hbtp_vm->input_dev) {
			pr_err("%s: virtual mouse input device hasn't been created\n",
						__func__);
			return -EFAULT;
		}

		if (copy_from_user(&clik_data, (void *)arg,
		    sizeof(struct hbtp_vm_click))) {
			pr_err("%s: Error copying data\n", __func__);
			return -EFAULT;
		}

		hbtp_vm_report_clicks(hbtp_vm, &clik_data);
		error = 0;
		break;
	default:
		pr_err("%s: Unsupported ioctl command %u\n", __func__, cmd);
		error = -EINVAL;
		break;
	}

	return error;
}

static const struct file_operations hbtp_vm_fops = {
	.owner           = THIS_MODULE,
	.open            = hbtp_vm_open,
	.release         = hbtp_vm_release,
	.unlocked_ioctl  = hbtp_vm_ioctl,
	.compat_ioctl    = hbtp_vm_ioctl,
};

static struct miscdevice hbtp_vm_misc = {
	.fops        = &hbtp_vm_fops,
	.minor       = MISC_DYNAMIC_MINOR,
	.name        = hbtp_vm_name,
};

static int __init hbtp_vm_init(void)
{
	int error;

	hbtp_vm = kzalloc(sizeof(struct hbtp_virtual_mouse), GFP_KERNEL);

	if (!hbtp_vm)
		return -ENOMEM;

	hbtp_vm->last_x = -1;
	hbtp_vm->last_y = -1;

	mutex_init(&hbtp_vm->mutex);
	error = misc_register(&hbtp_vm_misc);
	if (error) {
		pr_err("%s: misc_register failed\n", hbtp_vm_name);
		goto err_misc_reg;
	}

	hbtp_vm_create_input_dev();
	return 0;

err_misc_reg:
	kfree(hbtp_vm);
	return error;
}

static void __exit hbtp_vm_exit(void)
{
	misc_deregister(&hbtp_vm_misc);
	if (hbtp_vm->input_dev) {
		input_unregister_device(hbtp_vm->input_dev);
		hbtp_vm->input_dev = NULL;
	}
	kfree(hbtp_vm);
}

MODULE_ALIAS_MISCDEV(MISC_DYNAMIC_MINOR);
MODULE_ALIAS("devname:" hbtp_vm_name);
MODULE_DESCRIPTION("kernel module to support virtual mouse");
MODULE_LICENSE("GPL v2");
module_init(hbtp_vm_init);
module_exit(hbtp_vm_exit);
