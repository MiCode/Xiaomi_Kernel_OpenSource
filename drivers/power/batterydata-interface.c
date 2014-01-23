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

#define pr_fmt(fmt)	"BATTERY: %s: " fmt, __func__
#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/sched.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/uaccess.h>
#include <linux/batterydata-lib.h>
#include <linux/batterydata-interface.h>

struct battery_data {
	dev_t				dev_no;
	struct class			*battery_class;
	struct device			*battery_device;
	struct cdev			battery_cdev;
	struct bms_battery_data		*profile;
};
static struct battery_data *the_battery;

static int battery_data_open(struct inode *inode, struct file *file)
{
	struct battery_data *battery = container_of(inode->i_cdev,
				struct battery_data, battery_cdev);

	pr_debug("battery_data device opened\n");

	file->private_data = battery;

	return 0;
}

static long battery_data_ioctl(struct file *file, unsigned int cmd,
						unsigned long arg)
{
	struct battery_data *battery = file->private_data;
	struct battery_params __user *bp_user =
				(struct battery_params __user *)arg;
	struct battery_params bp;
	int soc, rbatt_sf, slope, fcc_mah;
	int rc = 0;

	if (!battery->profile) {
		pr_err("Battery data not set!\n");
		return -EINVAL;
	}
	if (copy_from_user(&bp, bp_user, sizeof(bp))) {
		pr_err("copy_from_user failed\n");
		return -EFAULT;
	}

	switch (cmd) {
	case BPIOCXSOC:
		soc = interpolate_pc(battery->profile->pc_temp_ocv_lut,
					bp.batt_temp, bp.ocv_uv / 1000);
		rc = put_user(soc, &bp_user->soc);
		if (rc) {
			pr_err("BPIOCXSOC: Failed to 'put_user' rc=%d\n", rc);
			goto ret_err;
		}
		pr_debug("BPIOCXSOC: ocv=%d batt_temp=%d soc=%d\n",
				bp.ocv_uv / 1000, bp.batt_temp, soc);
		break;
	case BPIOCXRBATT:
		rbatt_sf = interpolate_scalingfactor(
				battery->profile->rbatt_sf_lut,
				bp.batt_temp, bp.soc);
		rc = put_user(rbatt_sf, &bp_user->rbatt_sf);
		if (rc) {
			pr_err("BPIOCXRBATT: Failed to 'put_user' rc=%d\n", rc);
			goto ret_err;
		}
		pr_debug("BPIOCXRBATT: soc=%d batt_temp=%d rbatt_sf=%d\n",
					bp.soc, bp.batt_temp, rbatt_sf);
		break;
	case BPIOCXSLOPE:
		slope = interpolate_slope(battery->profile->pc_temp_ocv_lut,
							bp.batt_temp, bp.soc);
		rc = put_user(slope, &bp_user->slope);
		if (rc) {
			pr_err("BPIOCXSLOPE: Failed to 'put_user' rc=%d\n", rc);
			goto ret_err;
		}
		pr_debug("BPIOCXSLOPE: soc=%d batt_temp=%d slope=%d\n",
					bp.soc, bp.batt_temp, slope);
		break;
	case BPIOCXFCC:
		fcc_mah = interpolate_fcc(battery->profile->fcc_temp_lut,
							bp.batt_temp);
		rc = put_user(fcc_mah, &bp_user->fcc_mah);
		if (rc) {
			pr_err("BPIOCXFCC: Failed to 'put_user' rc=%d\n", rc);
			goto ret_err;
		}
		pr_debug("BPIOCXFCC: batt_temp=%d fcc_mah=%d\n",
					bp.batt_temp, fcc_mah);
		break;
	default:
		pr_err("IOCTL %d not supported\n", cmd);
		rc = -EINVAL;

	}
ret_err:
	return rc;
}

static int battery_data_release(struct inode *inode, struct file *file)
{
	pr_debug("battery_data device closed\n");

	return 0;
}

static const struct file_operations battery_data_fops = {
	.owner = THIS_MODULE,
	.open = battery_data_open,
	.unlocked_ioctl	= battery_data_ioctl,
	.release = battery_data_release,
};

int config_battery_data(struct bms_battery_data *profile)
{
	if (!the_battery) {
		pr_err("Battery data not intialized\n");
		return -ENODEV;
	}

	the_battery->profile = profile;

	pr_debug("Battery profile set - %s\n",
			the_battery->profile->battery_type);

	return 0;
}

int batterydata_init(void)
{
	int rc;
	struct battery_data *battery;

	battery = kzalloc(sizeof(*battery), GFP_KERNEL);
	if (!battery) {
		pr_err("Unable to allocate memory\n");
		return -ENOMEM;
	}

	/* character device to access the battery-data from userspace */
	rc = alloc_chrdev_region(&battery->dev_no, 0, 1, "battery_data");
	if (rc) {
		pr_err("Unable to allocate chrdev rc=%d\n", rc);
		return rc;
	}
	cdev_init(&battery->battery_cdev, &battery_data_fops);
	rc = cdev_add(&battery->battery_cdev, battery->dev_no, 1);
	if (rc) {
		pr_err("Unable to add battery_cdev rc=%d\n", rc);
		goto unregister_chrdev;
	}

	battery->battery_class = class_create(THIS_MODULE, "battery_data");
	if (IS_ERR_OR_NULL(battery->battery_class)) {
		pr_err("Fail to create battery class\n");
		rc = -ENODEV;
		goto delete_cdev;
	}

	battery->battery_device = device_create(battery->battery_class,
					NULL, battery->dev_no,
					NULL, "battery_data");
	if (IS_ERR(battery->battery_device)) {
		pr_err("Fail to create battery_device device\n");
		rc = -ENODEV;
		goto delete_cdev;
	}

	the_battery = battery;

	pr_info("Battery-data device created!\n");

	return 0;

delete_cdev:
	cdev_del(&battery->battery_cdev);
unregister_chrdev:
	unregister_chrdev_region(battery->dev_no, 1);
	the_battery = NULL;
	return rc;
}
subsys_initcall(batterydata_init);

static void batterydata_exit(void)
{
	if (the_battery) {
		device_destroy(the_battery->battery_class, the_battery->dev_no);
		cdev_del(&the_battery->battery_cdev);
		unregister_chrdev_region(the_battery->dev_no, 1);
	}
	kfree(the_battery);
	the_battery = NULL;
}
module_exit(batterydata_exit);

MODULE_DESCRIPTION("Battery-data Interface driver");
MODULE_LICENSE("GPL v2");
