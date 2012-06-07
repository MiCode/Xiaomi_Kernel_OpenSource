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
#include <linux/qfp_fuse.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/regulator/consumer.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>

/*
 * Time QFPROM requires to reliably burn a fuse.
 */
#define QFPROM_BLOW_TIMEOUT_US      10
#define QFPROM_BLOW_TIMER_OFFSET    0x2038
/*
 * Denotes number of cycles required to blow the fuse.
 */
#define QFPROM_BLOW_TIMER_VALUE     (QFPROM_BLOW_TIMEOUT_US * 83)

#define QFPROM_BLOW_STATUS_OFFSET   0x204C
#define QFPROM_BLOW_STATUS_BUSY     0x01
#define QFPROM_BLOW_STATUS_ERROR    0x02

#define QFP_FUSE_READY              0x01
#define QFP_FUSE_OFF                0x00

struct qfp_priv_t {
	uint32_t base;
	uint32_t end;
	struct mutex lock;
	struct regulator *fuse_vdd;
	u8 state;
};

/* We need only one instance of this for the driver */
static struct qfp_priv_t *qfp_priv;


static int qfp_fuse_open(struct inode *inode, struct file *filp)
{
	if (qfp_priv == NULL)
		return -ENODEV;

	filp->private_data = qfp_priv;

	return 0;
}

static int qfp_fuse_release(struct inode *inode, struct file *filp)
{

	filp->private_data = NULL;

	return 0;
}

static inline int qfp_fuse_wait_for_fuse_blow(u32 *status)
{
	u32 timeout = QFPROM_BLOW_TIMEOUT_US;
	/* wait for 400us before checking for the first time */
	udelay(400);
	do {
		*status = readl_relaxed(
			qfp_priv->base + QFPROM_BLOW_STATUS_OFFSET);

		if (!(*status & QFPROM_BLOW_STATUS_BUSY))
			return 0;

		timeout--;
		udelay(1);
	} while (timeout);
	pr_err("Timeout waiting for FUSE blow, status = %x\n", *status);
	return -ETIMEDOUT;
}

static inline int qfp_fuse_enable_regulator(void)
{
	int err;
	err = regulator_enable(qfp_priv->fuse_vdd);
	if (err != 0)
		pr_err("Error (%d) enabling regulator\n", err);
	return err;
}

static inline int qfp_fuse_disable_regulator(void)
{
	int err;
	err = regulator_disable(qfp_priv->fuse_vdd);
	if (err != 0)
		pr_err("Error (%d) disabling regulator\n", err);
	return err;
}

static int qfp_fuse_write_word(u32 *addr, u32 data)
{
	u32 blow_status = 0;
	u32 read_data;
	int err;

	/* Set QFPROM  blow timer register */
	writel_relaxed(QFPROM_BLOW_TIMER_VALUE,
			qfp_priv->base + QFPROM_BLOW_TIMER_OFFSET);
	mb();

	/* Enable LVS0 regulator */
	err = qfp_fuse_enable_regulator();
	if (err != 0)
		return err;

	/*
	 * Wait for about 1ms. However msleep(1) can sleep for
	 * up to 20ms as per Documentation/timers/timers-howto.txt.
	 * Time is not a constraint here.
	 */

	msleep(20);

	/* Write data */
	__raw_writel(data, addr);
	mb();

	/* blow_status = QFPROM_BLOW_STATUS_BUSY; */
	err = qfp_fuse_wait_for_fuse_blow(&blow_status);
	if (err) {
		qfp_fuse_disable_regulator();
		return err;
	}

	/* Check error status */
	if (blow_status & QFPROM_BLOW_STATUS_ERROR) {
		pr_err("Fuse blow status error: %d\n", blow_status);
		qfp_fuse_disable_regulator();
		return -EFAULT;
	}

	/* Disable regulator */
	qfp_fuse_disable_regulator();
	/*
	 * Wait for about 1ms. However msleep(1) can sleep for
	 * up to 20ms as per Documentation/timers/timers-howto.txt.
	 * Time is not a constraint here.
	 */
	msleep(20);

	/* Verify written data */
	read_data = readl_relaxed(addr);
	if (read_data != data) {
		pr_err("Error: read/write data mismatch\n");
		pr_err("Address = %p written data = %x read data = %x\n",
			addr, data, read_data);
		return -EFAULT;
	}

	return 0;
}

static long
qfp_fuse_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	struct qfp_fuse_req req;
	u32 *buf = NULL;
	int i;

	/* Verify user arguments. */
	if (_IOC_TYPE(cmd) != QFP_FUSE_IOC_MAGIC)
		return -ENOTTY;

	switch (cmd) {
	case QFP_FUSE_IOC_READ:
		if (arg == 0) {
			pr_err("user space arg not supplied\n");
			err = -EFAULT;
			break;
		}

		if (copy_from_user(&req, (void __user *)arg, sizeof(req))) {
			pr_err("Error copying req from user space\n");
			err = -EFAULT;
			break;
		}

		/* Check for limits */
		if (!req.size) {
			pr_err("Request size zero.\n");
			err = -EFAULT;
			break;
		}

		if (qfp_priv->base + req.offset + (req.size - 1) * 4 >
				qfp_priv->end) {
			pr_err("Req size exceeds QFPROM addr space\n");
			err = -EFAULT;
			break;
		}

		/* Allocate memory for buffer */
		buf = kzalloc(req.size * 4, GFP_KERNEL);
		if (buf == NULL) {
			pr_alert("No memory for data\n");
			err = -ENOMEM;
			break;
		}

		if (mutex_lock_interruptible(&qfp_priv->lock)) {
			err = -ERESTARTSYS;
			break;
		}

		/* Read data */
		for (i = 0; i < req.size; i++)
			buf[i] = readl_relaxed(
				((u32 *) (qfp_priv->base + req.offset)) + i);

		if (copy_to_user((void __user *)req.data, buf, 4*(req.size))) {
			pr_err("Error copying to user space\n");
			err = -EFAULT;
		}

		mutex_unlock(&qfp_priv->lock);
		break;

	case QFP_FUSE_IOC_WRITE:
		if (arg == 0) {
			pr_err("user space arg not supplied\n");
			err = -EFAULT;
			break;
		}

		if (copy_from_user(&req, (void __user *)arg, sizeof(req))) {
			pr_err("Error copying req from user space\n");
			err = -EFAULT;
			break;
		}
		/* Check for limits */
		if (!req.size) {
			pr_err("Request size zero.\n");
			err = -EFAULT;
			break;
		}
		if (qfp_priv->base + req.offset + (req.size - 1) * 4 >
				qfp_priv->end) {
			pr_err("Req size exceeds QFPROM space\n");
			err = -EFAULT;
			break;
		}

		/* Allocate memory for buffer */
		buf = kzalloc(4 * (req.size), GFP_KERNEL);
		if (buf == NULL) {
			pr_alert("No memory for data\n");
			err = -ENOMEM;
			break;
		}

		/* Copy user data to local buffer */
		if (copy_from_user(buf, (void __user *)req.data,
				4 * (req.size))) {
			pr_err("Error copying data from user space\n");
			err = -EFAULT;
			break;
		}

		if (mutex_lock_interruptible(&qfp_priv->lock)) {
			err = -ERESTARTSYS;
			break;
		}

		/* Write data word at a time */
		for (i = 0; i < req.size && !err; i++) {
			err = qfp_fuse_write_word(((u32 *) (
				qfp_priv->base + req.offset) + i), buf[i]);
		}

		mutex_unlock(&qfp_priv->lock);
		break;
	default:
		pr_err("Invalid ioctl command.\n");
		return -ENOTTY;
	}
	kfree(buf);
	return err;
}

static const struct file_operations qfp_fuse_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = qfp_fuse_ioctl,
	.open = qfp_fuse_open,
	.release = qfp_fuse_release
};

static struct miscdevice qfp_fuse_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "qfpfuse",
	.fops = &qfp_fuse_fops
};


static int qfp_fuse_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct resource *res;
	const char *regulator_name = pdev->dev.platform_data;


	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;

	if (!regulator_name)
		return -EINVAL;

	/* Initialize */
	qfp_priv = kzalloc(sizeof(struct qfp_priv_t), GFP_KERNEL);

	if (qfp_priv == NULL) {
		pr_alert("Not enough memory to initialize device\n");
		return -ENOMEM;
	}

	/* The driver is passed ioremapped address */
	qfp_priv->base = res->start;
	qfp_priv->end = res->end;

	/* Get regulator for QFPROM writes */
	qfp_priv->fuse_vdd = regulator_get(NULL, regulator_name);
	if (IS_ERR(qfp_priv->fuse_vdd)) {
		ret = PTR_ERR(qfp_priv->fuse_vdd);
		pr_err("Err (%d) getting %s\n", ret, regulator_name);
		qfp_priv->fuse_vdd = NULL;
		goto err;
	}

	mutex_init(&qfp_priv->lock);

	ret = misc_register(&qfp_fuse_dev);
	if (ret < 0)
		goto err;

	pr_info("Fuse driver base:%x end:%x\n", qfp_priv->base, qfp_priv->end);
	return 0;

err:
	if (qfp_priv->fuse_vdd)
		regulator_put(qfp_priv->fuse_vdd);

	kfree(qfp_priv);
	qfp_priv = NULL;

	return ret;

}

static int __devexit qfp_fuse_remove(struct platform_device *plat)
{
	if (qfp_priv && qfp_priv->fuse_vdd)
		regulator_put(qfp_priv->fuse_vdd);

	kfree(qfp_priv);
	qfp_priv = NULL;

	misc_deregister(&qfp_fuse_dev);
	pr_info("Removing Fuse driver\n");
	return 0;
}

static struct platform_driver qfp_fuse_driver = {
	.probe = qfp_fuse_probe,
	.remove = qfp_fuse_remove,
	.driver = {
		.name = "qfp_fuse_driver",
		.owner = THIS_MODULE,
	},
};

static int __init qfp_fuse_init(void)
{
	return platform_driver_register(&qfp_fuse_driver);
}

static void __exit qfp_fuse_exit(void)
{
	platform_driver_unregister(&qfp_fuse_driver);
}

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Rohit Vaswani <rvaswani@codeaurora.org>");
MODULE_DESCRIPTION("Driver to read/write to QFPROM fuses.");
MODULE_VERSION("1.01");

module_init(qfp_fuse_init);
module_exit(qfp_fuse_exit);
