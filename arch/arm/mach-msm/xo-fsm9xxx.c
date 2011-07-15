/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/regulator/pm8058-xo.h>
#include <linux/platform_device.h>

#define FSM_XO_IOC_MAGIC	0x93
#define FSM_XO_IOC_CLKBUF	_IO(FSM_XO_IOC_MAGIC, 1)

#define FSM_XO_DEVICE_READY	0x01
#define FSM_XO_DEVICE_OFF	0x00

/* enum for TCXO clock output buffer definition */
enum clk_buffer_type {
	XO_BUFFER_A0 = 0,
	XO_BUFFER_A1 = 1,
	XO_BUFFER_LAST
};

/*
 * This user request structure is used to exchange the pmic device data
 * requested to user space applications.  The pointer to this structure is
 * passed to the the ioctl function.
*/
struct fsm_xo_req {
	enum clk_buffer_type   clkBuffer;
	u8                     clkBufEnable;
};

struct fsm_xo_priv_t {
	struct mutex lock;
	struct regulator *a0;
	struct regulator *a1;
	u8 a0_enabled;
	u8 a1_enabled;
};

static struct fsm_xo_priv_t *fsm_xo_priv;

static int fsm_xo_open(struct inode *inode, struct file *filp)
{
	if (fsm_xo_priv == NULL)
		return -ENODEV;

	filp->private_data = fsm_xo_priv;

	return 0;
}

static int fsm_xo_release(struct inode *inode, struct file *filp)
{
	filp->private_data = NULL;

	return 0;
}

static inline int fsm_xo_enable_a0(void)
{
	int err = 0;

	if (!fsm_xo_priv->a0_enabled) {
		err = regulator_enable(fsm_xo_priv->a0);
		if (err != 0)
			pr_err("Error = %d enabling xo buffer a0\n", err);
		else
			fsm_xo_priv->a0_enabled = 1;
	}
	return err;
}

static inline int fsm_xo_disable_a0(void)
{
	int err = 0;

	if (fsm_xo_priv->a0_enabled) {
		err = regulator_disable(fsm_xo_priv->a0);
		if (err != 0)
			pr_err("Error = %d disabling xo buffer a0\n", err);
		else
			fsm_xo_priv->a0_enabled = 0;
	}
	return err;
}

static inline int fsm_xo_enable_a1(void)
{
	int err = 0;

	if (!fsm_xo_priv->a1_enabled) {
		err = regulator_enable(fsm_xo_priv->a1);
		if (err != 0)
			pr_err("Error = %d enabling xo buffer a1\n", err);
		else
			fsm_xo_priv->a1_enabled = 1;
	}
	return err;
}

static inline int fsm_xo_disable_a1(void)
{
	int err = 0;

	if (fsm_xo_priv->a1_enabled) {
		err = regulator_disable(fsm_xo_priv->a1);
		if (err != 0)
			pr_err("Error = %d disabling xo buffer a1\n", err);
		else
			fsm_xo_priv->a1_enabled = 0;
	}
	return err;
}
static long
fsm_xo_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	struct fsm_xo_req req;

	/* Verify user arguments. */
	if (_IOC_TYPE(cmd) != FSM_XO_IOC_MAGIC)
		return -ENOTTY;

	/* Lock for access */
	if (mutex_lock_interruptible(&fsm_xo_priv->lock))
		return -ERESTARTSYS;

	switch (cmd) {
	case FSM_XO_IOC_CLKBUF:
		if (arg == 0) {
			pr_err("user space arg not supplied\n");
			err = -EFAULT;
			break;
		}

		if (copy_from_user(&req, (void __user *)arg,
			sizeof(req))) {
			pr_err("Error copying from user space\n");
			err = -EFAULT;
			break;
		}

		if (req.clkBuffer == XO_BUFFER_A0) {
			if (req.clkBufEnable)
				err = fsm_xo_enable_a0();
			else
				err = fsm_xo_disable_a0();
		} else if (req.clkBuffer == XO_BUFFER_A1) {
			if (req.clkBufEnable)
				err = fsm_xo_enable_a1();
			else
				err = fsm_xo_disable_a1();
		} else {
			pr_err("Invalid ioctl argument.\n");
			err = -ENOTTY;
		}
		break;
	default:
		pr_err("Invalid ioctl command.\n");
		err = -ENOTTY;
		break;
	}

	mutex_unlock(&fsm_xo_priv->lock);
	return err;
}

static const struct file_operations fsm_xo_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = fsm_xo_ioctl,
	.open = fsm_xo_open,
	.release = fsm_xo_release
};

static struct miscdevice fsm_xo_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "fsm_xo",
	.fops = &fsm_xo_fops
};

static int fsm_xo_probe(struct platform_device *pdev)
{
	int ret = 0;

	/* Initialize */
	fsm_xo_priv = kzalloc(sizeof(struct fsm_xo_priv_t), GFP_KERNEL);

	if (fsm_xo_priv == NULL) {
		pr_alert("Not enough memory to initialize device\n");
		return -ENOMEM;
	}

	fsm_xo_priv->a0 = regulator_get(&pdev->dev, "a0_clk_buffer");
	if (IS_ERR(fsm_xo_priv->a0)) {
		pr_err("Error getting a0_clk_buffer\n");
		ret = PTR_ERR(fsm_xo_priv->a0);
		fsm_xo_priv->a0 = NULL;
		goto err;
	}
	fsm_xo_priv->a1 = regulator_get(&pdev->dev, "a1_clk_buffer");
	if (IS_ERR(fsm_xo_priv->a1)) {
		pr_err("Error getting a1_clk_buffer\n");
		ret = PTR_ERR(fsm_xo_priv->a1);
		fsm_xo_priv->a1 = NULL;
		goto err;
	}

	fsm_xo_priv->a0_enabled = 0;
	fsm_xo_priv->a1_enabled = 0;

	mutex_init(&fsm_xo_priv->lock);

	ret = misc_register(&fsm_xo_dev);
	if (ret < 0)
		goto err;

	return 0;

err:
	if (fsm_xo_priv->a0)
		regulator_put(fsm_xo_priv->a0);
	if (fsm_xo_priv->a1)
		regulator_put(fsm_xo_priv->a1);

	kfree(fsm_xo_priv);
	fsm_xo_priv = NULL;

	return ret;
}

static int __devexit fsm_xo_remove(struct platform_device *pdev)
{
	if (fsm_xo_priv && fsm_xo_priv->a0)
		regulator_put(fsm_xo_priv->a0);
	if (fsm_xo_priv && fsm_xo_priv->a1)
		regulator_put(fsm_xo_priv->a1);

	kfree(fsm_xo_priv);
	fsm_xo_priv = NULL;

	misc_deregister(&fsm_xo_dev);
	return 0;
}

static struct platform_driver fsm_xo_driver = {
	.probe          = fsm_xo_probe,
	.remove         = fsm_xo_remove,
	.driver         = {
		.name = "fsm_xo_driver",
	}
};

static int __init fsm_xo_init(void)
{
	return platform_driver_register(&fsm_xo_driver);
}

static void __exit fsm_xo_exit(void)
{
	platform_driver_unregister(&fsm_xo_driver);
}

module_init(fsm_xo_init);
module_exit(fsm_xo_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Provide userspace access to XO buffers in PMIC8058.");
MODULE_VERSION("1.00");
