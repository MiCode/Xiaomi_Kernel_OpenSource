/*
** =============================================================================
** Copyright (c) 2016  Texas Instruments Inc.
** Copyright (C) 2019 XiaoMi, Inc.
**
** This program is free software; you can redistribute it and/or modify it under
** the terms of the GNU General Public License as published by the Free Software
** Foundation; version 2.
**
** This program is distributed in the hope that it will be useful, but WITHOUT
** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
** FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
**
** File:
**     tas2563-misc.c
**
** Description:
**     misc driver for Texas Instruments TAS2563 High Performance 4W Smart Amplifier
**
** =============================================================================
*/

#ifdef CONFIG_TAS2563_MISC

#define DEBUG
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/firmware.h>
#include <linux/regmap.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>

#include "tas2563.h"
#include "tas2563-misc.h"
#include <linux/dma-mapping.h>

static int g_logEnable = 1;
static struct tas2563_priv *g_tas2563;

static int tas2563_file_open(struct inode *inode, struct file *file)
{
	struct tas2563_priv *pTAS2563 = g_tas2563;

	if (!try_module_get(THIS_MODULE))
		return -ENODEV;

	file->private_data = (void *)pTAS2563;
	if (g_logEnable)
		dev_info(pTAS2563->dev,	"%s\n", __func__);
	return 0;
}

static int tas2563_file_release(struct inode *inode, struct file *file)
{
	struct tas2563_priv *pTAS2563 = (struct tas2563_priv *)file->private_data;

	if (g_logEnable)
		dev_info(pTAS2563->dev,	"%s\n", __func__);
	file->private_data = (void *)NULL;
	module_put(THIS_MODULE);

	return 0;
}

static ssize_t tas2563_file_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	struct tas2563_priv *pTAS2563 = (struct tas2563_priv *)file->private_data;
	int ret = 0;
	unsigned int nValue = 0;
	unsigned char value = 0;
	unsigned char *p_kBuf = NULL;

	mutex_lock(&pTAS2563->file_lock);

	switch (pTAS2563->mnDBGCmd) {
	case TIAUDIO_CMD_REG_READ: {
		if (g_logEnable)
			dev_info(pTAS2563->dev, "TIAUDIO_CMD_REG_READ: current_reg = 0x%x, count=%d\n",
				pTAS2563->mnCurrentReg, (int)count);
		if (count == 1) {
			ret = pTAS2563->read(pTAS2563, pTAS2563->mnCurrentReg, &nValue);
			if (ret < 0)
				break;

			value = (u8)nValue;
			if (g_logEnable)
				dev_info(pTAS2563->dev, "TIAUDIO_CMD_REG_READ: nValue=0x%x, value=0x%x\n", nValue, value);
			ret = copy_to_user(buf, &value, 1);
			if (ret != 0) {
				/* Failed to copy all the data, exit */
				dev_err(pTAS2563->dev, "copy to user fail %d\n", ret);
			}
		} else if (count > 1) {
			p_kBuf = kzalloc(count, GFP_KERNEL);
			if (p_kBuf != NULL) {
				ret = pTAS2563->bulk_read(pTAS2563, pTAS2563->mnCurrentReg, p_kBuf, count);
				if (ret < 0)
					break;
				ret = copy_to_user(buf, p_kBuf, count);
				if (ret != 0) {
					/* Failed to copy all the data, exit */
					dev_err(pTAS2563->dev, "copy to user fail %d\n", ret);
				}
				kfree(p_kBuf);
			} else
				dev_err(pTAS2563->dev, "read no mem\n");
		}
	}
	break;

	}
	pTAS2563->mnDBGCmd = 0;

	mutex_unlock(&pTAS2563->file_lock);
	return count;
}

static ssize_t tas2563_file_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	struct tas2563_priv *pTAS2563 = (struct tas2563_priv *)file->private_data;
	int ret = 0;
	unsigned char *p_kBuf = NULL;
	unsigned int reg = 0;
	unsigned int len = 0;

	mutex_lock(&pTAS2563->file_lock);

	p_kBuf = kzalloc(count, GFP_KERNEL);
	if (p_kBuf == NULL) {
		dev_err(pTAS2563->dev, "write no mem\n");
		goto err;
	}

	ret = copy_from_user(p_kBuf, buf, count);
	if (ret != 0) {
		dev_err(pTAS2563->dev, "copy_from_user failed.\n");
		goto err;
	}

	pTAS2563->mnDBGCmd = p_kBuf[0];
	switch (pTAS2563->mnDBGCmd) {
	case TIAUDIO_CMD_REG_WITE:
		if (count > 5) {
			reg = ((unsigned int)p_kBuf[1] << 24)
				+ ((unsigned int)p_kBuf[2] << 16)
				+ ((unsigned int)p_kBuf[3] << 8)
				+ (unsigned int)p_kBuf[4];
			len = count - 5;
			if (len == 1) {
				ret = pTAS2563->write(pTAS2563, reg, p_kBuf[5]);
				if (g_logEnable)
					dev_info(pTAS2563->dev, "TIAUDIO_CMD_REG_WITE, Reg=0x%x, Val=0x%x\n", reg, p_kBuf[5]);
			} else
				ret = pTAS2563->bulk_write(pTAS2563, reg, &p_kBuf[5], len);
		} else
			dev_err(pTAS2563->dev, "%s, write len fail, count=%d.\n", __func__, (int)count);
		pTAS2563->mnDBGCmd = 0;
	break;

	case TIAUDIO_CMD_REG_READ:
		if (count == 5) {
			pTAS2563->mnCurrentReg = ((unsigned int)p_kBuf[1] << 24)
				+ ((unsigned int)p_kBuf[2] << 16)
				+ ((unsigned int)p_kBuf[3] << 8)
				+ (unsigned int)p_kBuf[4];
			if (g_logEnable)
				dev_info(pTAS2563->dev, "TIAUDIO_CMD_REG_READ whole=0x%x\n", pTAS2563->mnCurrentReg);
		} else
			dev_err(pTAS2563->dev, "read len fail.\n");
	break;
	}

err:
	if (p_kBuf != NULL)
		kfree(p_kBuf);

	mutex_unlock(&pTAS2563->file_lock);

	return count;
}

static const struct file_operations fops = {
	.owner = THIS_MODULE,
	.read = tas2563_file_read,
	.write = tas2563_file_write,
	.unlocked_ioctl = NULL,
	.open = tas2563_file_open,
	.release = tas2563_file_release,
};

#define MODULE_NAME	"tas2563"
static struct miscdevice tas2563_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = MODULE_NAME,
	.fops = &fops,
};

int tas2563_register_misc(struct tas2563_priv *pTAS2563)
{
	int ret = 0;

	g_tas2563 = pTAS2563;

	ret = misc_register(&tas2563_misc);
	if (ret)
		dev_err(pTAS2563->dev, "TAS2563 misc fail: %d\n", ret);

	dev_info(pTAS2563->dev, "%s, leave\n", __func__);

	return ret;
}

int tas2563_deregister_misc(struct tas2563_priv *pTAS2563)
{
	misc_deregister(&tas2563_misc);
	return 0;
}

MODULE_AUTHOR("Texas Instruments Inc.");
MODULE_DESCRIPTION("TAS2563 Misc Smart Amplifier driver");
MODULE_LICENSE("GPL v2");
#endif
