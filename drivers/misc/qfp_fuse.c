/* Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
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
#include <linux/of.h>

/*
 * Time QFPROM requires to reliably burn a fuse.
 */
#define QFPROM_BLOW_TIMEOUT_US      20
#define QFPROM_BLOW_TIMER_OFFSET    0x2038
/*
 * Denotes number of cycles required to blow the fuse.
 */
#define QFPROM_BLOW_TIMER_VALUE     0xF0

#define QFPROM_BLOW_STATUS_OFFSET   0x204C
#define QFPROM_BLOW_STATUS_BUSY     0x01
#define QFPROM_BLOW_STATUS_ERROR    0x02

#define QFP_FUSE_READY              0x01
#define QFP_FUSE_OFF                0x00

#define QFP_FUSE_BUF_SIZE           64
#define UINT32_MAX                  (0xFFFFFFFFU)

static const char *blow_supply = "vdd-blow";

struct qfp_priv_t {
	uint32_t base;
	uint32_t end;
	uint32_t blow_status_offset;
	uint32_t blow_timer;
	struct mutex lock;
	struct regulator *fuse_vdd;
	u8 state;
};

struct qfp_resource {
	resource_size_t	start;
	resource_size_t	size;
	uint32_t	blow_status_offset;
	uint32_t	blow_timer;
	const char	*regulator_name;
};

/* We need only one instance of this for the driver */
static struct qfp_priv_t *qfp_priv;

static inline bool is_usr_req_valid(const struct qfp_fuse_req *req)
{
	uint32_t size = qfp_priv->end - qfp_priv->base;
	uint32_t req_size;

	if (req->size >= (UINT32_MAX / sizeof(uint32_t)))
		return false;
	req_size = req->size * sizeof(uint32_t);
	if ((req_size == 0) || (req_size > size))
		return false;
	if (req->offset >= size)
		return false;
	if ((req->offset + req_size) > size)
		return false;

	return true;
}

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
			qfp_priv->base + qfp_priv->blow_status_offset);

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
	writel_relaxed(qfp_priv->blow_timer,
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
	u32 fuse_buf[QFP_FUSE_BUF_SIZE];
	u32 *buf = fuse_buf;
	u32 *ptr = NULL;
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
		if (is_usr_req_valid(&req) == false) {
			pr_err("Invalid request\n");
			err = -EINVAL;
			break;
		}

		if (req.size > QFP_FUSE_BUF_SIZE) {
			/* Allocate memory for buffer */
			ptr = kzalloc(req.size * 4, GFP_KERNEL);
			if (ptr == NULL) {
				pr_alert("No memory for data\n");
				err = -ENOMEM;
				break;
			}
			buf = ptr;
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
		if (is_usr_req_valid(&req) == false) {
			pr_err("Invalid request\n");
			err = -EINVAL;
			break;
		}

		if (req.size > QFP_FUSE_BUF_SIZE) {
			/* Allocate memory for buffer */
			ptr = kzalloc(req.size * 4, GFP_KERNEL);
			if (ptr == NULL) {
				pr_alert("No memory for data\n");
				err = -ENOMEM;
				break;
			}
			buf = ptr;
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

	kfree(ptr);

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

static int qfp_get_resource(struct platform_device *pdev,
			    struct qfp_resource *qfp_res)
{
	struct resource *res;
	const char *regulator_name = NULL;
	uint32_t blow_status_offset = QFPROM_BLOW_STATUS_OFFSET;
	uint32_t blow_timer = QFPROM_BLOW_TIMER_VALUE;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;

	if (pdev->dev.of_node) {
		struct device_node *np = pdev->dev.of_node;

		if (of_property_read_u32(np, "qcom,blow-status-offset",
					 &blow_status_offset) == 0) {
			if ((res->start + blow_status_offset) > res->end) {
				pr_err("Invalid blow-status-offset\n");
				return -EINVAL;
			}
		}

		if (of_property_read_bool(np, "vdd-blow-supply")) {
			/* For backward compatibility, use the name
			 * from blow_supply */
			regulator_name = blow_supply;
		} else {
			pr_err("Failed to find regulator-name property\n");
			return -EINVAL;
		}

		of_property_read_u32(np, "qcom,blow-timer", &blow_timer);

	} else {
		regulator_name = pdev->dev.platform_data;
	}

	if (!regulator_name)
		return -EINVAL;

	qfp_res->start = res->start;
	qfp_res->size = resource_size(res);
	qfp_res->blow_status_offset = blow_status_offset;
	qfp_res->blow_timer = blow_timer;
	qfp_res->regulator_name = regulator_name;

	return 0;
}

static int qfp_fuse_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct qfp_resource res;

	ret = qfp_get_resource(pdev, &res);
	if (ret)
		return ret;

	/* Initialize */
	qfp_priv = kzalloc(sizeof(struct qfp_priv_t), GFP_KERNEL);

	if (qfp_priv == NULL) {
		pr_alert("Not enough memory to initialize device\n");
		return -ENOMEM;
	}

	qfp_priv->base = (uint32_t)ioremap(res.start, res.size);
	if (!qfp_priv->base) {
		pr_warn("ioremap failed\n");
		goto err;
	}
	qfp_priv->end = qfp_priv->base + res.size;
	qfp_priv->blow_status_offset = res.blow_status_offset;
	qfp_priv->blow_timer = res.blow_timer;

	/* Get regulator for QFPROM writes */
	qfp_priv->fuse_vdd = regulator_get(&pdev->dev, res.regulator_name);
	if (IS_ERR(qfp_priv->fuse_vdd)) {
		ret = PTR_ERR(qfp_priv->fuse_vdd);
		pr_err("Err (%d) getting %s\n", ret, res.regulator_name);
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

static int qfp_fuse_remove(struct platform_device *plat)
{
	if (qfp_priv && qfp_priv->fuse_vdd)
		regulator_put(qfp_priv->fuse_vdd);

	iounmap((void __iomem *)qfp_priv->base);
	kfree(qfp_priv);
	qfp_priv = NULL;

	misc_deregister(&qfp_fuse_dev);
	pr_info("Removing Fuse driver\n");
	return 0;
}

static struct of_device_id __attribute__ ((unused)) qfp_fuse_of_match[] = {
	{ .compatible = "qcom,qfp-fuse", },
	{}
};

static struct platform_driver qfp_fuse_driver = {
	.probe = qfp_fuse_probe,
	.remove = qfp_fuse_remove,
	.driver = {
		.name = "qfp_fuse_driver",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(qfp_fuse_of_match),
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
