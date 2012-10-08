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
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/slab.h>

#include <media/rc-core.h>
#include <media/user-rc-input.h>

#define MAX_SP_DEVICES		1
#define USER_SP_INPUT_DEV_NAME	"user-sp-input"
#define USER_SP_INPUT_DRV_NAME	"sp-user-input"

struct user_sp_input_dev {
	struct cdev sp_input_cdev;
	struct class *sp_input_class;
	struct device *sp_input_dev;
	struct rc_dev *spdev;
	dev_t sp_input_base_dev;
	struct device *dev;
	int in_use;
};

static int user_sp_input_open(struct inode *inode, struct file *file)
{
	struct cdev *input_cdev = inode->i_cdev;
	struct user_sp_input_dev *input_dev =
	container_of(input_cdev, struct user_sp_input_dev, sp_input_cdev);

	if (input_dev->in_use) {
		dev_err(input_dev->dev,
		"Device is already open..only one instance is allowed\n");
		return -EBUSY;
	}
	input_dev->in_use++;
	file->private_data = input_dev;

	return 0;
}

static int user_sp_input_release(struct inode *inode, struct file *file)
{
	struct user_sp_input_dev *input_dev = file->private_data;

	input_dev->in_use--;

	return 0;
}

static ssize_t user_sp_input_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *ppos)
{
	int ret = count;
	struct user_sp_input_dev *input_dev = file->private_data;
	unsigned char cmd = 0;
	int scancode = 0;

	if (copy_from_user(&cmd, buffer, 1)) {
		dev_err(input_dev->dev, "Copy from user failed\n");
		ret = -EFAULT;
		goto out_free;
	}

	if (copy_from_user(&scancode, &buffer[1], 4)) {
		dev_err(input_dev->dev, "Copy from user failed\n");
		ret = -EFAULT;
		goto out_free;
	}

	switch (cmd) {
	case USER_CONTROL_PRESSED:
		dev_dbg(input_dev->dev, "user controlled pressed 0x%x\n",
			scancode);
		rc_keydown(input_dev->spdev, scancode, 0);
		break;
	case USER_CONTROL_REPEATED:
		dev_dbg(input_dev->dev, "user controlled repeated 0x%x\n",
			scancode);
		rc_repeat(input_dev->spdev);
		break;
	case USER_CONTROL_RELEASED:
		dev_dbg(input_dev->dev, "user controlled released 0x%x\n",
			scancode);
		rc_keyup(input_dev->spdev);
		break;
	}

out_free:
	return ret;
}

const struct file_operations sp_fops = {
	.owner  = THIS_MODULE,
	.open   = user_sp_input_open,
	.write  = user_sp_input_write,
	.release = user_sp_input_release,
};

static int user_sp_input_probe(struct platform_device *pdev)
{
	struct user_sp_input_dev *user_sp_dev;
	struct rc_dev *spdev;
	int retval;

	user_sp_dev = kzalloc(sizeof(struct user_sp_input_dev), GFP_KERNEL);
	if (!user_sp_dev)
		return -ENOMEM;

	user_sp_dev->sp_input_class = class_create(THIS_MODULE,
						"user-sp-input-loopback");

	if (IS_ERR(user_sp_dev->sp_input_class)) {
		retval = PTR_ERR(user_sp_dev->sp_input_class);
		goto err;
	}

	retval = alloc_chrdev_region(&user_sp_dev->sp_input_base_dev, 0,
				MAX_SP_DEVICES, USER_SP_INPUT_DEV_NAME);

	if (retval) {
		dev_err(&pdev->dev,
			"alloc_chrdev_region failed\n");
		goto alloc_chrdev_err;
	}

	dev_info(&pdev->dev, "User space report standby key event input" \
				" driver registered, major %d\n",
				MAJOR(user_sp_dev->sp_input_base_dev));

	cdev_init(&user_sp_dev->sp_input_cdev, &sp_fops);
	retval = cdev_add(&user_sp_dev->sp_input_cdev,
			user_sp_dev->sp_input_base_dev, MAX_SP_DEVICES);
	if (retval) {
		dev_err(&pdev->dev, "cdev_add failed\n");
		goto cdev_add_err;
	}
	user_sp_dev->sp_input_dev = device_create(user_sp_dev->sp_input_class,
		NULL, MKDEV(MAJOR(user_sp_dev->sp_input_base_dev), 0), NULL,
		"user-sp-input-dev%d", 0);

	if (IS_ERR(user_sp_dev->sp_input_dev)) {
		retval = PTR_ERR(user_sp_dev->sp_input_dev);
		dev_err(&pdev->dev, "device_create failed\n");
		goto device_create_err;
	}

	spdev = rc_allocate_device();
	if (!spdev) {
		dev_err(&pdev->dev, "failed to allocate rc device");
		retval = -ENOMEM;
		goto err_allocate_device;
	}

	spdev->driver_type = RC_DRIVER_SCANCODE;
	spdev->allowed_protos = RC_TYPE_OTHER;
	spdev->input_name = USER_SP_INPUT_DEV_NAME;
	spdev->input_id.bustype = BUS_HOST;
	spdev->driver_name = USER_SP_INPUT_DRV_NAME;
	spdev->map_name = RC_MAP_RC6_PHILIPS;

	retval = rc_register_device(spdev);
	if (retval < 0) {
		dev_err(&pdev->dev, "failed to register rc device\n");
		goto rc_register_err;
	}
	user_sp_dev->spdev = spdev;
	user_sp_dev->dev = &pdev->dev;
	platform_set_drvdata(pdev, user_sp_dev);
	user_sp_dev->in_use = 0;

	return 0;

rc_register_err:
	rc_free_device(spdev);
err_allocate_device:
	device_destroy(user_sp_dev->sp_input_class,
			MKDEV(MAJOR(user_sp_dev->sp_input_base_dev), 0));
cdev_add_err:
	unregister_chrdev_region(user_sp_dev->sp_input_base_dev,
				MAX_SP_DEVICES);
device_create_err:
	cdev_del(&user_sp_dev->sp_input_cdev);
alloc_chrdev_err:
	class_destroy(user_sp_dev->sp_input_class);
err:
	kfree(user_sp_dev);
	return retval;
}

static int user_sp_input_remove(struct platform_device *pdev)
{
	struct user_sp_input_dev *user_sp_dev = platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, NULL);
	rc_free_device(user_sp_dev->spdev);
	device_destroy(user_sp_dev->sp_input_class,
			MKDEV(MAJOR(user_sp_dev->sp_input_base_dev), 0));
	unregister_chrdev_region(user_sp_dev->sp_input_base_dev,
				MAX_SP_DEVICES);
	cdev_del(&user_sp_dev->sp_input_cdev);
	class_destroy(user_sp_dev->sp_input_class);
	kfree(user_sp_dev);

	return 0;
}

static struct platform_driver user_sp_input_driver = {
	.probe  = user_sp_input_probe,
	.remove = user_sp_input_remove,
	.driver = {
		.name   = USER_SP_INPUT_DRV_NAME,
		.owner  = THIS_MODULE,
	},
};

static int __init user_sp_input_init(void)
{
	return platform_driver_register(&user_sp_input_driver);
}
module_init(user_sp_input_init);

static void __exit user_sp_input_exit(void)
{
	platform_driver_unregister(&user_sp_input_driver);
}
module_exit(user_sp_input_exit);

MODULE_DESCRIPTION("User SP RC Input driver");
MODULE_LICENSE("GPL v2");
