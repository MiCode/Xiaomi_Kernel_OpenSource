/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/slab.h>

#include <media/rc-core.h>
#include <media/user-rc-input.h>

#define MAX_RC_DEVICES		1
#define USER_RC_INPUT_DEV_NAME	"user-rc-input"
#define USER_RC_INPUT_DRV_NAME	"rc-user-input"

struct user_rc_input_dev {
	struct cdev rc_input_cdev;
	struct class *rc_input_class;
	struct device *rc_input_dev;
	struct rc_dev *rcdev;
	dev_t rc_input_base_dev;
	struct device *dev;
	int in_use;
};

static int user_rc_input_open(struct inode *inode, struct file *file)
{
	struct cdev *input_cdev	= inode->i_cdev;
	struct user_rc_input_dev *input_dev =
	container_of(input_cdev, struct user_rc_input_dev, rc_input_cdev);

	if (input_dev->in_use) {
		dev_err(input_dev->dev,
		"Device is already open..only one instance is allowed\n");
		return -EBUSY;
	}
	input_dev->in_use++;
	file->private_data = input_dev;

	return 0;
}

static int user_rc_input_release(struct inode *inode, struct file *file)
{
	struct user_rc_input_dev *input_dev = file->private_data;

	input_dev->in_use--;

	return 0;
}

static ssize_t user_rc_input_write(struct file *file, const char __user *buffer,
						size_t count, loff_t *ppos)
{
	int ret;
	struct user_rc_input_dev *input_dev = file->private_data;
	__u8 *buf;

	buf = kmalloc(count * sizeof(__u8), GFP_KERNEL);
	if (!buf) {
		dev_err(input_dev->dev,
			"kmalloc failed...Insufficient memory\n");
		ret = -ENOMEM;
		goto out;
	}

	if (copy_from_user(buf, buffer, count)) {
		dev_err(input_dev->dev, "Copy from user failed\n");
		ret = -EFAULT;
		goto out_free;
	}

	switch (buf[0])	{
	case USER_CONTROL_PRESSED:
		dev_dbg(input_dev->dev, "user controlled"
					" pressed 0x%x\n", buf[1]);
		rc_keydown(input_dev->rcdev, buf[1], 0);
		break;
	case USER_CONTROL_REPEATED:
		dev_dbg(input_dev->dev, "user controlled"
					" repeated 0x%x\n", buf[1]);
		rc_repeat(input_dev->rcdev);
		break;
	case USER_CONTROL_RELEASED:
		dev_dbg(input_dev->dev, "user controlled"
					" released 0x%x\n", buf[1]);
		rc_keyup(input_dev->rcdev);
		break;
	}

out_free:
	kfree(buf);
out:
	return ret;
}

const struct file_operations fops = {
	.owner  = THIS_MODULE,
	.open   = user_rc_input_open,
	.write  = user_rc_input_write,
	.release = user_rc_input_release,
};

static int user_rc_input_probe(struct platform_device *pdev)
{
	struct user_rc_input_dev *user_rc_dev;
	struct rc_dev *rcdev;
	int retval;

	user_rc_dev = kzalloc(sizeof(struct user_rc_input_dev), GFP_KERNEL);
	if (!user_rc_dev)
		return -ENOMEM;

	user_rc_dev->rc_input_class = class_create(THIS_MODULE,
						"user-rc-input-loopback");

	if (IS_ERR(user_rc_dev->rc_input_class)) {
		retval = PTR_ERR(user_rc_dev->rc_input_class);
		goto err;
	}

	retval = alloc_chrdev_region(&user_rc_dev->rc_input_base_dev, 0,
				MAX_RC_DEVICES,	USER_RC_INPUT_DEV_NAME);

	if (retval) {
		dev_err(&pdev->dev,
			"alloc_chrdev_region failed\n");
		goto alloc_chrdev_err;
	}

	dev_info(&pdev->dev, "User space report key event input "
					"loopback driver registered, "
		"major %d\n", MAJOR(user_rc_dev->rc_input_base_dev));

	cdev_init(&user_rc_dev->rc_input_cdev, &fops);
	retval = cdev_add(&user_rc_dev->rc_input_cdev,
				user_rc_dev->rc_input_base_dev,
							MAX_RC_DEVICES);
	if (retval) {
		dev_err(&pdev->dev, "cdev_add failed\n");
		goto cdev_add_err;
	}
	user_rc_dev->rc_input_dev =
				device_create(user_rc_dev->rc_input_class,
									NULL,
				MKDEV(MAJOR(user_rc_dev->rc_input_base_dev),
				0), NULL, "user-rc-input-dev%d", 0);

	if (IS_ERR(user_rc_dev->rc_input_dev)) {
		retval = PTR_ERR(user_rc_dev->rc_input_dev);
		dev_err(&pdev->dev, "device_create failed\n");
		goto device_create_err;
	}

	rcdev = rc_allocate_device();
	if (!rcdev) {
		dev_err(&pdev->dev, "failed to allocate rc device");
		retval = -ENOMEM;
		goto err_allocate_device;
	}

	rcdev->driver_type = RC_DRIVER_SCANCODE;
	rcdev->allowed_protos = RC_TYPE_OTHER;
	rcdev->input_name = USER_RC_INPUT_DEV_NAME;
	rcdev->input_id.bustype = BUS_HOST;
	rcdev->driver_name = USER_RC_INPUT_DRV_NAME;
	rcdev->map_name = RC_MAP_UE_RF4CE;

	retval = rc_register_device(rcdev);
	if (retval < 0) {
		dev_err(&pdev->dev, "failed to register rc device\n");
		goto rc_register_err;
	}
	user_rc_dev->rcdev = rcdev;
	user_rc_dev->dev = &pdev->dev;
	platform_set_drvdata(pdev, user_rc_dev);
	user_rc_dev->in_use = 0;

	return 0;

rc_register_err:
	rc_free_device(rcdev);
err_allocate_device:
	device_destroy(user_rc_dev->rc_input_class,
			MKDEV(MAJOR(user_rc_dev->rc_input_base_dev), 0));
cdev_add_err:
	unregister_chrdev_region(user_rc_dev->rc_input_base_dev,
							MAX_RC_DEVICES);
device_create_err:
	cdev_del(&user_rc_dev->rc_input_cdev);
alloc_chrdev_err:
	class_destroy(user_rc_dev->rc_input_class);
err:
	kfree(user_rc_dev);
	return retval;
}

static int user_rc_input_remove(struct platform_device *pdev)
{
	struct user_rc_input_dev *user_rc_dev = platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, NULL);
	rc_free_device(user_rc_dev->rcdev);
	device_destroy(user_rc_dev->rc_input_class,
			MKDEV(MAJOR(user_rc_dev->rc_input_base_dev), 0));
	unregister_chrdev_region(user_rc_dev->rc_input_base_dev,
							MAX_RC_DEVICES);
	cdev_del(&user_rc_dev->rc_input_cdev);
	class_destroy(user_rc_dev->rc_input_class);
	kfree(user_rc_dev);

	return 0;
}

static struct platform_driver user_rc_input_driver = {
	.probe  = user_rc_input_probe,
	.remove = user_rc_input_remove,
	.driver = {
		.name   = USER_RC_INPUT_DRV_NAME,
		.owner  = THIS_MODULE,
	},
};

static int __init user_rc_input_init(void)
{
	return platform_driver_register(&user_rc_input_driver);
}
module_init(user_rc_input_init);

static void __exit user_rc_input_exit(void)
{
	platform_driver_unregister(&user_rc_input_driver);
}
module_exit(user_rc_input_exit);

MODULE_DESCRIPTION("User RC Input driver");
MODULE_LICENSE("GPL v2");
