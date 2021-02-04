/*
** =============================================================================
** Copyright (c) 2016  Texas Instruments Inc.
** Copyright (C) 2021 XiaoMi, Inc.
**
** This program is free software; you can redistribute it and/or modify it under
** the terms of the GNU General Public License as published by the Free Software
** Foundation; version 2.
**
** This program is distributed in the hope that it will be useful, but WITHOUT
** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
** FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
** File:
**     tas2562-misc.c
**
** Description:
**     misc driver for Texas Instruments TAS2562 High Performance 4W Smart Amplifier
**
** =============================================================================
*/

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

#include "tas2562.h"
#include "tas2562-misc.h"
#include <linux/dma-mapping.h>

static int g_logEnable = 1;
static struct tas2562_priv *g_tas2562;

static int tas2562_file_open(struct inode *inode, struct file *file)
{
	struct tas2562_priv *pTAS2562 = g_tas2562;

	if (!try_module_get(THIS_MODULE))
		return -ENODEV;

	file->private_data = (void *)pTAS2562;

	if (g_logEnable)
		dev_info(pTAS2562->dev, "%s\n", __func__);
	return 0;
}

static int tas2562_file_release(struct inode *inode, struct file *file)
{
	struct tas2562_priv *pTAS2562 = (struct tas2562_priv *)file->private_data;

	if (g_logEnable)
		dev_info(pTAS2562->dev, "%s\n", __func__);

	file->private_data = (void *)NULL;
	module_put(THIS_MODULE);

	return 0;
}

static ssize_t tas2562_file_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	struct tas2562_priv *pTAS2562 = (struct tas2562_priv *)file->private_data;
	int ret = 0;
	unsigned int nValue = 0;
	unsigned char value = 0;
	unsigned char *p_kBuf = NULL;

	mutex_lock(&pTAS2562->file_lock);

	switch (pTAS2562->mnDBGCmd) {
	case TIAUDIO_CMD_REG_READ:
	{
		if (g_logEnable)
			dev_info(pTAS2562->dev,
				"TIAUDIO_CMD_REG_READ: current_reg = 0x%x, count=%d\n",
				pTAS2562->mnCurrentReg, (int)count);
		if (count == 1) {
			ret = pTAS2562->read(pTAS2562, pTAS2562->mnCurrentReg, &nValue);
			if (ret < 0) {
				dev_err(pTAS2562->dev, "dev read fail %d\n", ret);
				break;
			}

			value = (u8)nValue;
			if (g_logEnable)
				dev_info(pTAS2562->dev, "TIAUDIO_CMD_REG_READ: nValue=0x%x, value=0x%x\n",
					nValue, value);
			ret = copy_to_user(buf, &value, 1);
			if (ret != 0) {
				/* Failed to copy all the data, exit */
				dev_err(pTAS2562->dev, "copy to user fail %d\n", ret);
			}
		} else if (count > 1) {
			p_kBuf = kzalloc(count, GFP_KERNEL);
			if (p_kBuf != NULL) {
				ret = pTAS2562->bulk_read(pTAS2562, pTAS2562->mnCurrentReg, p_kBuf, count);
				if (ret < 0) {
					dev_err(pTAS2562->dev, "dev bulk read fail %d\n", ret);
				} else {
					ret = copy_to_user(buf, p_kBuf, count);
					if (ret != 0) {
						/* Failed to copy all the data, exit */
						dev_err(pTAS2562->dev, "copy to user fail %d\n", ret);
					}
				}

				kfree(p_kBuf);
			} else {
				dev_err(pTAS2562->dev, "read no mem\n");
			}
		}
	}
	break;
	}
	pTAS2562->mnDBGCmd = 0;

	mutex_unlock(&pTAS2562->file_lock);
	return count;
}

static ssize_t tas2562_file_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	struct tas2562_priv *pTAS2562 = (struct tas2562_priv *)file->private_data;
	int ret = 0;
	unsigned char *p_kBuf = NULL;
	unsigned int reg = 0;
	unsigned int len = 0;

	mutex_lock(&pTAS2562->file_lock);

	p_kBuf = kzalloc(count, GFP_KERNEL);
	if (p_kBuf == NULL) {
		dev_err(pTAS2562->dev, "write no mem\n");
		goto err;
	}

	ret = copy_from_user(p_kBuf, buf, count);
	if (ret != 0) {
		dev_err(pTAS2562->dev, "copy_from_user failed.\n");
		goto err;
	}

	pTAS2562->mnDBGCmd = p_kBuf[0];
	switch (pTAS2562->mnDBGCmd) {
	case TIAUDIO_CMD_REG_WITE:
	if (count > 5) {
		reg = ((unsigned int)p_kBuf[1] << 24) +
			((unsigned int)p_kBuf[2] << 16) +
			((unsigned int)p_kBuf[3] << 8) +
			(unsigned int)p_kBuf[4];
		len = count - 5;
		if (len == 1) {
			ret = pTAS2562->write(pTAS2562, reg, p_kBuf[5]);
			if (g_logEnable)
				dev_info(pTAS2562->dev,
					"TIAUDIO_CMD_REG_WITE, Reg=0x%x, Val=0x%x\n",
					reg, p_kBuf[5]);
		} else {
			ret = pTAS2562->bulk_write(pTAS2562, reg, &p_kBuf[5], len);
		}
	} else {
		dev_err(pTAS2562->dev, "%s, write len fail, count=%d.\n",
			__func__, (int)count);
	}
	pTAS2562->mnDBGCmd = 0;
	break;

	case TIAUDIO_CMD_REG_READ:
	if (count == 5) {
		pTAS2562->mnCurrentReg = ((unsigned int)p_kBuf[1] << 24) +
			((unsigned int)p_kBuf[2] << 16) +
			((unsigned int)p_kBuf[3] << 8) +
			(unsigned int)p_kBuf[4];
		if (g_logEnable) {
			dev_info(pTAS2562->dev,
				"TIAUDIO_CMD_REG_READ, whole=0x%x\n",
				pTAS2562->mnCurrentReg);
		}
	} else {
		dev_err(pTAS2562->dev, "read len fail.\n");
	}
	break;
	}
err:
	if (p_kBuf != NULL)
		kfree(p_kBuf);

	mutex_unlock(&pTAS2562->file_lock);

	return count;
}

static const struct file_operations fops = {
	.owner = THIS_MODULE,
	.read = tas2562_file_read,
	.write = tas2562_file_write,
	.unlocked_ioctl = NULL,
	.open = tas2562_file_open,
	.release = tas2562_file_release,
};

#define MODULE_NAME	"tas2562"
static struct miscdevice tas2562_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = MODULE_NAME,
	.fops = &fops,
};

int tas2562_register_misc(struct tas2562_priv *pTAS2562)
{
	int ret = 0;

	g_tas2562 = pTAS2562;
	ret = misc_register(&tas2562_misc);
	if (ret) {
		dev_err(pTAS2562->dev, "TAS2562 misc fail: %d\n", ret);
	}

	dev_info(pTAS2562->dev, "%s, leave\n", __func__);

	return ret;
}

int tas2562_deregister_misc(struct tas2562_priv *pTAS2562)
{
	misc_deregister(&tas2562_misc);
	return 0;
}

MODULE_AUTHOR("Texas Instruments Inc.");
MODULE_DESCRIPTION("TAS2562 Misc Smart Amplifier driver");
MODULE_LICENSE("GPL v2");
