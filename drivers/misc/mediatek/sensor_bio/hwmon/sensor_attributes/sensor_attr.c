/*
* Copyright (C) 2011-2014 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify it under the terms of the
* GNU General Public License version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/init.h>
#include <linux/device.h>
#include "sensor_attr.h"
#include "sensor_event.h"


static int sensor_attr_major = -1;
static struct class *sensor_attr_class;

static LIST_HEAD(sensor_attr_list);
static DEFINE_MUTEX(sensor_attr_mtx);

static int sensor_attr_open(struct inode *inode, struct file *file)
{
	int minor = iminor(inode);
	struct sensor_attr_t *c;
	int err = -ENODEV;
	const struct file_operations *new_fops = NULL;

	mutex_lock(&sensor_attr_mtx);

	list_for_each_entry(c, &sensor_attr_list, list) {
		if (c->minor == minor) {
			new_fops = fops_get(c->fops);
			break;
		}
	}

	if (!new_fops) {
		mutex_unlock(&sensor_attr_mtx);
		request_module("char-major-%d-%d", sensor_attr_major, minor);
		mutex_lock(&sensor_attr_mtx);

		list_for_each_entry(c, &sensor_attr_list, list) {
			if (c->minor == minor) {
				new_fops = fops_get(c->fops);
				break;
			}
		}
		if (!new_fops)
			goto fail;
	}

	err = 0;
	replace_fops(file, new_fops);
	if (file->f_op->open) {
		file->private_data = c;
		err = file->f_op->open(inode, file);
	}
fail:
	mutex_unlock(&sensor_attr_mtx);
	return err;
}

static const struct file_operations sensor_attr_fops = {
	.owner		= THIS_MODULE,
	.open		= sensor_attr_open,
};

int sensor_attr_register(struct sensor_attr_t *misc)
{
	dev_t dev;
	int err = 0;
	struct sensor_attr_t *c;

	mutex_lock(&sensor_attr_mtx);
	list_for_each_entry(c, &sensor_attr_list, list) {
		if (c->minor == misc->minor) {
			err = -EBUSY;
			goto out;
		}
	}
	dev = MKDEV(sensor_attr_major, misc->minor);
	misc->this_device = device_create(sensor_attr_class, misc->parent, dev,
					  misc, "%s", misc->name);
	if (IS_ERR(misc->this_device))
		goto out;
	list_add(&misc->list, &sensor_attr_list);
	mutex_unlock(&sensor_attr_mtx);
	err = sensor_event_register(misc->minor);
	return err;
 out:
	mutex_unlock(&sensor_attr_mtx);
	return err;
}
int sensor_attr_deregister(struct sensor_attr_t *misc)
{
	if (WARN_ON(list_empty(&misc->list)))
		return -EINVAL;

	mutex_lock(&sensor_attr_mtx);
	list_del(&misc->list);
	device_destroy(sensor_attr_class, MKDEV(sensor_attr_major, misc->minor));
	mutex_unlock(&sensor_attr_mtx);
	sensor_event_deregister(misc->minor);
	return 0;
}
#if 0
static char *sensor_attr_devnode(struct device *dev, umode_t *mode)
{
	pr_err("sensor_attr: name :%s\n", dev_name(dev));
	return kasprintf(GFP_KERNEL, "sensor/%s", dev_name(dev));
}
#endif
static int __init sensor_attr_init(void)
{
	int err;

	sensor_attr_class = class_create(THIS_MODULE, "sensor");
	err = PTR_ERR(sensor_attr_class);
	if (IS_ERR(sensor_attr_class)) {
		err = -EIO;
		return err;
	}
	sensor_attr_major = register_chrdev(0, "sensor", &sensor_attr_fops);
	if (sensor_attr_major < 0)
		goto fail_printk;
	/* sensor_attr_class->devnode = sensor_attr_devnode; */
	return 0;

fail_printk:
	pr_err("unable to get major %d for misc devices\n", sensor_attr_major);
	class_destroy(sensor_attr_class);
	return err;
}
subsys_initcall(sensor_attr_init);

