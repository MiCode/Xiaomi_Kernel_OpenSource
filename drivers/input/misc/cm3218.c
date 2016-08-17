/* drivers/input/misc/cm3218.c - cm3218 Ambient Light Sensor driver
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION. All Rights Reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/lightsensor.h>
#include <linux/workqueue.h>
#include <linux/miscdevice.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/wakelock.h>
#include <linux/jiffies.h>
#include <asm/mach-types.h>
#include <asm/setup.h>
#include <linux/regulator/consumer.h>

#define ALS_CMD_SET	0x00
#define ALS_CMD_DATA	0x04
#define ALS_CMD_INIT_VALUE 0x0005;
#define ALS_POWER_MASK 0x0001
#define ALS_POWER_ON_VAL 0x0000
#define IS_ALS_POWER_ON \
	(((chip_info->als_set_reg) & ALS_POWER_MASK) == ALS_POWER_ON_VAL)
#define ALS_POWER_ENABLE ((chip_info->als_set_reg) &= 0xFFFE)
#define ALS_POWER_DISABLE ((chip_info->als_set_reg) |= 0x0001)

#define LS_POLLING_DELAY	1000 /* mSec */
#define REGULATOR_LATENCY	1

static void report_do_work(struct work_struct *w);
static DECLARE_DELAYED_WORK(report_work, report_do_work);

struct cm3218_info {
	struct class *cls;
	struct i2c_client *client;
	struct input_dev *inp_dev;
	struct workqueue_struct *wq;
	struct regulator *vdd_sensor;
	int polling_delay;
	u16 als_set_reg;
	struct mutex lock;
	bool is_als_on_before_suspend;
	int shutdown_complete;
};

static struct cm3218_info *chip_info;

static inline int cm3218_read(void)
{
	int ret = 0;
	mutex_lock(&chip_info->lock);
	if (chip_info && chip_info->shutdown_complete) {
		mutex_unlock(&chip_info->lock);
		return -ENODEV;
	}

	ret = i2c_smbus_read_word_data(chip_info->client, ALS_CMD_DATA);
	mutex_unlock(&chip_info->lock);
	return ret;
}

static inline int cm3218_write(void)
{
	int ret = 0;
	mutex_lock(&chip_info->lock);
	if (chip_info && chip_info->shutdown_complete) {
		mutex_unlock(&chip_info->lock);
		return -ENODEV;
	}
	ret = i2c_smbus_write_word_data(chip_info->client,
			ALS_CMD_SET, chip_info->als_set_reg);
	mutex_unlock(&chip_info->lock);
	return ret;
}


static void report_alsensor_input_event(void)
{
	int ret = 0;

	ret = cm3218_read();
	if (ret < 0) {
		pr_err("[CM3218] %s: Error read value=%d\n", __func__, ret);
		return;
	}

	mutex_lock(&chip_info->lock);
	input_report_abs(chip_info->inp_dev, ABS_MISC, ret);
	input_sync(chip_info->inp_dev);
	mutex_unlock(&chip_info->lock);
}

static void report_do_work(struct work_struct *work)
{
	report_alsensor_input_event();
	queue_delayed_work(chip_info->wq,
				&report_work,
				chip_info->polling_delay);
}

static int cm3218_power(int on)
{
	int ret = 0;
	if (on && !IS_ALS_POWER_ON) {
		regulator_enable(chip_info->vdd_sensor);
		msleep(REGULATOR_LATENCY);
		ALS_POWER_ENABLE;
		ret = cm3218_write();
		if (ret < 0)
			return ret;
		chip_info->inp_dev->enabled = true;
		queue_delayed_work(chip_info->wq, &report_work, 0);
	} else if (!on && IS_ALS_POWER_ON) {
		ALS_POWER_DISABLE;
		ret = cm3218_write();
		if (ret < 0)
			return ret;
		chip_info->inp_dev->enabled = false;
		cancel_delayed_work(&report_work);
		regulator_disable(chip_info->vdd_sensor);
	}
	return 0;
}

static int cm3218_enable(struct input_dev *dev)
{
	pr_debug("[LS][CM3218] %s\n", __func__);
	if (chip_info->is_als_on_before_suspend) {
		cm3218_power(true);
	} else {
		regulator_enable(chip_info->vdd_sensor);
		msleep(REGULATOR_LATENCY);
		cm3218_write();
		regulator_disable(chip_info->vdd_sensor);
	}
	return 0;
}

static int cm3218_disable(struct input_dev *dev)
{
	pr_debug("[LS][CM3218] %s\n", __func__);
	chip_info->is_als_on_before_suspend = IS_ALS_POWER_ON;
	if (IS_ALS_POWER_ON)
		cm3218_power(false);
	return 0;
}

static int lightsensor_open(struct inode *inode, struct file *file)
{
	pr_debug("[CM3218] %s\n", __func__);
	return 0;
}

static int lightsensor_release(struct inode *inode, struct file *file)
{
	pr_debug("[CM3218] %s\n", __func__);
	return 0;
}

static long lightsensor_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg) {
	int rc = 0;
	int val;
	unsigned long delay;

	switch (cmd) {
	case LIGHTSENSOR_IOCTL_ENABLE:
		if (get_user(val, (unsigned long __user *)arg)) {
			rc = -EFAULT;
			break;
		}
		pr_debug("[CM3218] %s LIGHTSENSOR_IOCTL_ENABLE, value = %d\n",
		  __func__, val);
		rc = cm3218_power(val);
		break;

	case LIGHTSENSOR_IOCTL_GET_ENABLED:
		val = IS_ALS_POWER_ON;
		pr_debug("[CM3218] %s LIGHTSENSOR_IOCTL_GET_ENABLED, enabled %d\n",
					__func__, val);
		rc = put_user(val, (unsigned long __user *)arg);
		break;

	case LIGHTSENSOR_IOCTL_SET_DELAY:
		if (get_user(delay, (unsigned long __user *)arg)) {
			rc = -EFAULT;
			break;
		}
		pr_debug("[CM3218] %s LIGHTSENSOR_IOCTL_SET_DELAY, delay %ld\n",
			__func__, delay);
		delay = delay / 1000000;
		chip_info->polling_delay = msecs_to_jiffies(delay);
		break;

	default:
		pr_err("[CM3218 error]%s: invalid cmd %d\n",
				__func__, _IOC_NR(cmd));
		rc = -EINVAL;
	}

	return rc;
}

static const struct file_operations lightsensor_fops = {
	.owner = THIS_MODULE,
	.open = lightsensor_open,
	.release = lightsensor_release,
	.unlocked_ioctl = lightsensor_ioctl,
};

static struct miscdevice lightsensor_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "lightsensor",
	.fops = &lightsensor_fops,
};


static int cm3218_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int ret = 0;

	chip_info =
		kzalloc(sizeof(struct cm3218_info), GFP_KERNEL);

	if (!chip_info)
		return -ENOMEM;
	chip_info->vdd_sensor = regulator_get(NULL, "vdd_sensor_2v85");

	if (IS_ERR_OR_NULL(chip_info->vdd_sensor)) {
		pr_err("[CM3218 error]%s: regulator_get failed\n",
			__func__);
		return PTR_ERR(chip_info->vdd_sensor);
	}
	i2c_set_clientdata(client, chip_info);
	chip_info->client = client;

	mutex_init(&chip_info->lock);

	chip_info->inp_dev = input_allocate_device();
	if (!chip_info->inp_dev) {
		pr_err("[CM3218 error]%s: could not allocate input device\n",
			__func__);
		ret = -ENOMEM;
		goto err_input_allocate;
	}

	chip_info->inp_dev->name = "cm3218-ls";
	chip_info->inp_dev->enable = cm3218_enable;
	chip_info->inp_dev->disable = cm3218_disable;
	chip_info->inp_dev->enabled = false;

	set_bit(EV_ABS, chip_info->inp_dev->evbit);
	input_set_abs_params(chip_info->inp_dev, ABS_MISC, 0, 9, 0, 0);

	ret = input_register_device(chip_info->inp_dev);
	if (ret < 0) {
		pr_err("[CM3218 error]%s:can not register input device\n",
		__func__);
		goto err_input_register_device;
	}

	ret = misc_register(&lightsensor_misc);
	if (ret < 0) {
		pr_err("[CM3218 error]%s:can not register misc device\n",
		__func__);
		goto err_misc_register;
	}

	chip_info->wq = create_singlethread_workqueue("cm3218_wq");
	if (!chip_info->wq) {
		pr_err("[CM3218 error]%s: can't create workqueue\n", __func__);
		ret = -ENOMEM;
		goto err_create_singlethread_workqueue;
	}

	chip_info->cls = class_create(THIS_MODULE, "optical_sensors");
	if (IS_ERR(chip_info->cls)) {
		ret = PTR_ERR(chip_info->cls);
		chip_info->cls = NULL;
		goto err_class_create;
	}

	chip_info->polling_delay = msecs_to_jiffies(LS_POLLING_DELAY);
	chip_info->als_set_reg = ALS_CMD_INIT_VALUE;
	regulator_enable(chip_info->vdd_sensor);
	msleep(REGULATOR_LATENCY);
	cm3218_write();
	regulator_disable(chip_info->vdd_sensor);
	chip_info->shutdown_complete = 0;
	pr_info("[CM3218] probe success");
	return ret;


err_class_create:
err_create_singlethread_workqueue:
	misc_deregister(&lightsensor_misc);
err_misc_register:
	input_unregister_device(chip_info->inp_dev);
err_input_register_device:
	input_free_device(chip_info->inp_dev);
err_input_allocate:
	mutex_destroy(&chip_info->lock);
	kfree(chip_info);
	return ret;
}

static void cm3218_shutdown(struct i2c_client *client)
{
	mutex_lock(&chip_info->lock);
	if (IS_ALS_POWER_ON) {
		cancel_delayed_work_sync(&report_work);
	}
	chip_info->shutdown_complete = 1;
	mutex_unlock(&chip_info->lock);
}

static const struct i2c_device_id cm3218_id[] = {
	{"cm3218", 0},
	{}
};

static struct i2c_driver cm3218_driver = {
	.id_table = cm3218_id,
	.probe = cm3218_probe,
	.driver = {
		.name = "cm3218",
		.owner = THIS_MODULE,
	},
	.shutdown = cm3218_shutdown,
};

static int __init cm3218_init(void)
{
	return i2c_add_driver(&cm3218_driver);
}

static void __exit cm3218_exit(void)
{
	i2c_del_driver(&cm3218_driver);
}

module_init(cm3218_init);
module_exit(cm3218_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("CM3218 Driver");
MODULE_AUTHOR("Sri Krishna chowdary <schowdary@nvidia.com>");
