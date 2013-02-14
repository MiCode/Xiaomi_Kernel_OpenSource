/* Copyright (c) 2010-2012, The Linux Foundation. All rights reserved.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <mach/msm_iomap.h>

#include <linux/fsm_rfic_ftr.h>

/*
 * FTR8700 RFIC
 */

#define RFIC_FTR_DEVICE_NUM		2
#define RFIC_GRFC_REG_NUM		6

#define ANY_BUS				0x0
#define TX1_BUS				0x0
#define TX2_BUS				0x1
#define MISC_BUS			0x2
#define RX_BUS				0x3
#define BUS_BITS			0x3

/*
 * Device private information per device node
 */

static struct ftr_dev_node_info {
	void *grfcCtrlAddr;
	void *grfcMaskAddr;
	unsigned int busSelect[4];
	struct i2c_adapter *ssbi_adap;

	/* lock */
	struct mutex lock;
} ftr_dev_info[RFIC_FTR_DEVICE_NUM];

/*
 * Device private information per file
 */

struct ftr_dev_file_info {
	int ftrId;
};

/*
 * File interface
 */

static int ftr_find_id(int minor);

static int ftr_open(struct inode *inode, struct file *file)
{
	struct ftr_dev_file_info *pdfi;

	/* private data allocation */
	pdfi = kmalloc(sizeof(*pdfi), GFP_KERNEL);
	if (pdfi == NULL)
		return -ENOMEM;
	file->private_data = pdfi;

	/* FTR ID */
	pdfi->ftrId = ftr_find_id(MINOR(inode->i_rdev));

	return 0;
}

static int ftr_release(struct inode *inode, struct file *file)
{
	struct ftr_dev_file_info *pdfi;

	pdfi = (struct ftr_dev_file_info *) file->private_data;

	kfree(file->private_data);
	file->private_data = NULL;

	return 0;
}

static ssize_t ftr_read(struct file *filp, char __user *buf, size_t count,
	loff_t *f_pos)
{
	return 0;
}

static ssize_t ftr_write(struct file *file, const char __user *buffer,
	size_t count, loff_t *ppos)
{
	return 0;
}

static int ftr_ssbi_read(
	struct ftr_dev_node_info *pdev,
	unsigned int addr,
	u8 *buf,
	size_t len)
{
	int ret;
	struct i2c_msg msg = {
		.addr = addr,
		.flags = I2C_M_RD,
		.buf = buf,
		.len = len,
	};

	ret = i2c_transfer(pdev->ssbi_adap, &msg, 1);

	return (ret == 1) ? 0 : ret;
}

static int ftr_ssbi_write(
	struct ftr_dev_node_info *pdev,
	unsigned int addr,
	u8 *buf,
	size_t len)
{
	int ret;
	struct i2c_msg msg = {
		.addr = addr,
		.flags = 0x0,
		.buf = (u8 *) buf,
		.len = len,
	};

	ret = i2c_transfer(pdev->ssbi_adap, &msg, 1);

	return (ret == 1) ? 0 : ret;
}

static long ftr_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg)
{
	unsigned int __user *argp = (unsigned int __user *) arg;
	struct ftr_dev_file_info *pdfi =
		(struct ftr_dev_file_info *) file->private_data;
	struct ftr_dev_node_info *pdev;

	if (pdfi->ftrId < 0 || pdfi->ftrId >= RFIC_FTR_DEVICE_NUM)
		return -EINVAL;

	pdev = ftr_dev_info + pdfi->ftrId;

	switch (cmd) {
	case RFIC_IOCTL_READ_REGISTER:
		{
			int ret;
			unsigned int rficAddr;
			u8 value;

			if (get_user(rficAddr, argp))
				return -EFAULT;

			mutex_lock(&pdev->lock);
			mb();
			/* Need to write twice due to bug in hardware */
			__raw_writel(
				pdev->busSelect[RFIC_FTR_GET_BUS(rficAddr)],
				pdev->grfcCtrlAddr);
			__raw_writel(
				pdev->busSelect[RFIC_FTR_GET_BUS(rficAddr)],
				pdev->grfcCtrlAddr);
			mb();
			ret = ftr_ssbi_read(pdev, RFIC_FTR_GET_ADDR(rficAddr),
				&value, 1);
			mutex_unlock(&pdev->lock);

			if (ret)
				return ret;

			if (put_user(value, argp))
				return -EFAULT;
		}
		break;

	case RFIC_IOCTL_WRITE_REGISTER:
		{
			int ret;
			struct rfic_write_register_param param;
			unsigned int rficAddr;
			u8 value;

			if (copy_from_user(&param, argp, sizeof param))
				return -EFAULT;
			rficAddr = param.rficAddr;
			value = (u8) param.value;

			mutex_lock(&pdev->lock);
			mb();
			/* Need to write twice due to bug in hardware */
			__raw_writel(
				pdev->busSelect[RFIC_FTR_GET_BUS(rficAddr)],
				pdev->grfcCtrlAddr);
			__raw_writel(
				pdev->busSelect[RFIC_FTR_GET_BUS(rficAddr)],
				pdev->grfcCtrlAddr);
			mb();
			ret = ftr_ssbi_write(pdev, RFIC_FTR_GET_ADDR(rficAddr),
				&value, 1);
			mutex_unlock(&pdev->lock);

			if (ret)
				return ret;
		}
		break;

	case RFIC_IOCTL_WRITE_REGISTER_WITH_MASK:
		{
			int ret;
			struct rfic_write_register_mask_param param;
			unsigned int rficAddr;
			u8 value;

			if (copy_from_user(&param, argp, sizeof param))
				return -EFAULT;
			rficAddr = param.rficAddr;

			mutex_lock(&pdev->lock);
			mb();
			/* Need to write twice due to bug in hardware */
			__raw_writel(
				pdev->busSelect[RFIC_FTR_GET_BUS(rficAddr)],
				pdev->grfcCtrlAddr);
			__raw_writel(
				pdev->busSelect[RFIC_FTR_GET_BUS(rficAddr)],
				pdev->grfcCtrlAddr);
			mb();
			ret = ftr_ssbi_read(pdev, RFIC_FTR_GET_ADDR(rficAddr),
				&value, 1);
			value &= (u8) ~param.mask;
			value |= (u8) (param.value & param.mask);
			ret = ftr_ssbi_write(pdev, RFIC_FTR_GET_ADDR(rficAddr),
				&value, 1);
			mutex_unlock(&pdev->lock);

			if (ret)
				return ret;
		}
		break;

	case RFIC_IOCTL_GET_GRFC:
		{
			struct rfic_grfc_param param;

			if (copy_from_user(&param, argp, sizeof param))
				return -EFAULT;

			if (param.grfcId >= RFIC_GRFC_REG_NUM)
				return -EINVAL;

			param.maskValue = __raw_readl(
				MSM_GRFC_BASE + 0x18 + param.grfcId * 4);
			param.ctrlValue = __raw_readl(
				MSM_GRFC_BASE + 0x00 + param.grfcId * 4);

			if (copy_to_user(argp, &param, sizeof param))
				return -EFAULT;
		}
		break;

	case RFIC_IOCTL_SET_GRFC:
		{
			struct rfic_grfc_param param;

			if (copy_from_user(&param, argp, sizeof param))
				return -EFAULT;

			if (param.grfcId >= RFIC_GRFC_REG_NUM)
				return -EINVAL;

			__raw_writel(param.maskValue,
				MSM_GRFC_BASE + 0x18 + param.grfcId * 4);
			/* Need to write twice due to bug in hardware */
			__raw_writel(param.ctrlValue,
				MSM_GRFC_BASE + 0x00 + param.grfcId * 4);
			__raw_writel(param.ctrlValue,
				MSM_GRFC_BASE + 0x00 + param.grfcId * 4);
			mb();
		}
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static const struct file_operations ftr_fops = {
	.owner = THIS_MODULE,
	.open = ftr_open,
	.release = ftr_release,
	.read = ftr_read,
	.write = ftr_write,
	.unlocked_ioctl = ftr_ioctl,
};

/*
 * Driver initialization & cleanup
 */

struct miscdevice ftr_misc_dev[RFIC_FTR_DEVICE_NUM] = {
	{
		.minor = MISC_DYNAMIC_MINOR,
		.name = RFIC_FTR_DEVICE_NAME "0",
		.fops = &ftr_fops,
	},
	{
		.minor = MISC_DYNAMIC_MINOR,
		.name = RFIC_FTR_DEVICE_NAME "1",
		.fops = &ftr_fops,
	},
};

int ftr_find_id(int minor)
{
	int i;

	for (i = 0; i < RFIC_FTR_DEVICE_NUM; ++i)
		if (ftr_misc_dev[i].minor == minor)
			break;

	return i;
}

static int __init ftr_init(void)
{
	int i, ret;
	struct ftr_dev_node_info *pdev;

	for (i = 0; i < RFIC_FTR_DEVICE_NUM; ++i) {
		pdev = ftr_dev_info + i;

		if (i == 0) {
			pdev->grfcCtrlAddr = MSM_GRFC_BASE + 0x00;
			pdev->grfcMaskAddr = MSM_GRFC_BASE + 0x18;
			__raw_writel(0x300000, pdev->grfcMaskAddr);
			pdev->busSelect[TX1_BUS] = 0x000000;
			pdev->busSelect[TX2_BUS] = 0x100000;
			pdev->busSelect[MISC_BUS] = 0x200000;
			pdev->busSelect[RX_BUS] = 0x300000;
			pdev->ssbi_adap = i2c_get_adapter(1);
		} else {
			pdev->grfcCtrlAddr = MSM_GRFC_BASE + 0x04;
			pdev->grfcMaskAddr = MSM_GRFC_BASE + 0x1c;
			__raw_writel(0x480000, pdev->grfcMaskAddr);
			pdev->busSelect[TX1_BUS] = 0x000000;
			pdev->busSelect[TX2_BUS] = 0x400000;
			pdev->busSelect[MISC_BUS] = 0x080000;
			pdev->busSelect[RX_BUS] = 0x480000;
			pdev->ssbi_adap = i2c_get_adapter(2);
		}

		mutex_init(&pdev->lock);
		ret = misc_register(ftr_misc_dev + i);

		if (ret < 0) {
			while (--i >= 0)
				misc_deregister(ftr_misc_dev + i);
			return ret;
		}
	}

	return 0;
}

static void __exit ftr_exit(void)
{
	int i;

	for (i = 0; i < RFIC_FTR_DEVICE_NUM; ++i)
		misc_deregister(ftr_misc_dev + i);
}

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Rohit Vaswani <rvaswani@codeaurora.org>");
MODULE_DESCRIPTION("Qualcomm FSM RFIC driver");
MODULE_VERSION("1.0");

module_init(ftr_init);
module_exit(ftr_exit);
