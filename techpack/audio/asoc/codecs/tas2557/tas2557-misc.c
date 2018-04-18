/*
** =============================================================================
** Copyright (c) 2016  Texas Instruments Inc.
** Copyright (C) 2018 XiaoMi, Inc.
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
**     tas2557-misc.c
**
** Description:
**     misc driver for Texas Instruments TAS2557 High Performance 4W Smart Amplifier
**
** =============================================================================
*/

#ifdef CONFIG_TAS2557_MISC

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

#include "tas2557.h"
#include "tas2557-core.h"
#include "tas2557-misc.h"
#include <linux/dma-mapping.h>

static int g_logEnable = 1;
static struct tas2557_priv *g_tas2557;

static int tas2557_file_open(struct inode *inode, struct file *file)
{
	struct tas2557_priv *pTAS2557 = g_tas2557;

	if (!try_module_get(THIS_MODULE))
		return -ENODEV;

	file->private_data = (void *)pTAS2557;
	if (g_logEnable)
		dev_info(pTAS2557->dev,	"%s\n", __func__);
	return 0;
}

static int tas2557_file_release(struct inode *inode, struct file *file)
{
	struct tas2557_priv *pTAS2557 = (struct tas2557_priv *)file->private_data;

	if (g_logEnable)
		dev_info(pTAS2557->dev,	"%s\n", __func__);
	file->private_data = (void *)NULL;
	module_put(THIS_MODULE);

	return 0;
}

static ssize_t tas2557_file_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	struct tas2557_priv *pTAS2557 = (struct tas2557_priv *)file->private_data;
	int ret = 0;
	unsigned int nValue = 0;
	unsigned char value = 0;
	unsigned char *p_kBuf = NULL;

	mutex_lock(&pTAS2557->file_lock);

	switch (pTAS2557->mnDBGCmd) {
	case TIAUDIO_CMD_REG_READ: {
		if (g_logEnable)
			dev_info(pTAS2557->dev, "TIAUDIO_CMD_REG_READ: current_reg = 0x%x, count=%d\n",
				pTAS2557->mnCurrentReg, (int)count);
		if (count == 1) {
			ret = pTAS2557->read(pTAS2557, pTAS2557->mnCurrentReg, &nValue);
			if (ret < 0)
				break;

			value = (u8)nValue;
			if (g_logEnable)
				dev_info(pTAS2557->dev, "TIAUDIO_CMD_REG_READ: nValue=0x%x, value=0x%x\n", nValue, value);
			ret = copy_to_user(buf, &value, 1);
			if (ret != 0) {
				/* Failed to copy all the data, exit */
				dev_err(pTAS2557->dev, "copy to user fail %d\n", ret);
			}
		} else if (count > 1) {
			p_kBuf = kzalloc(count, GFP_KERNEL);
			if (p_kBuf != NULL) {
				ret = pTAS2557->bulk_read(pTAS2557, pTAS2557->mnCurrentReg, p_kBuf, count);
				if (ret < 0)
					break;
				ret = copy_to_user(buf, p_kBuf, count);
				if (ret != 0) {
					/* Failed to copy all the data, exit */
					dev_err(pTAS2557->dev, "copy to user fail %d\n", ret);
				}
				kfree(p_kBuf);
			} else
				dev_err(pTAS2557->dev, "read no mem\n");
		}
	}
	break;

	case TIAUDIO_CMD_PROGRAM: {
		if ((pTAS2557->mpFirmware->mnConfigurations > 0)
			&& (pTAS2557->mpFirmware->mnPrograms > 0)) {
			if (g_logEnable)
				dev_info(pTAS2557->dev,	"TIAUDIO_CMD_PROGRAM: count = %d\n", (int)count);

			if (count == PROGRAM_BUF_SIZE) {
				p_kBuf = kzalloc(count, GFP_KERNEL);
				if (p_kBuf != NULL) {
					struct TProgram *pProgram =
						&(pTAS2557->mpFirmware->mpPrograms[pTAS2557->mnCurrentProgram]);
					p_kBuf[0] = pTAS2557->mpFirmware->mnPrograms;
					p_kBuf[1] = pTAS2557->mnCurrentProgram;
					p_kBuf[2] = pProgram->mnAppMode;
					p_kBuf[3] = (pProgram->mnBoost&0xff00)>>8;
					p_kBuf[4] = (pProgram->mnBoost&0x00ff);
					memcpy(&p_kBuf[5], pProgram->mpName, FW_NAME_SIZE);
					strlcpy(&p_kBuf[5+FW_NAME_SIZE], pProgram->mpDescription, strlen(pProgram->mpDescription) + 1);
					ret = copy_to_user(buf, p_kBuf, count);
					if (ret != 0) {
						/* Failed to copy all the data, exit */
						dev_err(pTAS2557->dev, "copy to user fail %d\n", ret);
					}
					kfree(p_kBuf);
				} else
					dev_err(pTAS2557->dev, "read no mem\n");
			} else
				dev_err(pTAS2557->dev, "read buffer not sufficient\n");
		} else
			dev_err(pTAS2557->dev, "%s, firmware not loaded\n", __func__);
	}
	break;

	case TIAUDIO_CMD_CONFIGURATION: {
		if ((pTAS2557->mpFirmware->mnConfigurations > 0)
		&& (pTAS2557->mpFirmware->mnPrograms > 0)) {
			if (g_logEnable)
				dev_info(pTAS2557->dev, "TIAUDIO_CMD_CONFIGURATION: count = %d\n", (int)count);
			if (count == CONFIGURATION_BUF_SIZE) {
				p_kBuf = kzalloc(count, GFP_KERNEL);
				if (p_kBuf != NULL) {
					struct TConfiguration *pConfiguration = &(pTAS2557->mpFirmware->mpConfigurations[pTAS2557->mnCurrentConfiguration]);

					p_kBuf[0] = pTAS2557->mpFirmware->mnConfigurations;
					p_kBuf[1] = pTAS2557->mnCurrentConfiguration;
					memcpy(&p_kBuf[2], pConfiguration->mpName, FW_NAME_SIZE);
					p_kBuf[2+FW_NAME_SIZE] = pConfiguration->mnProgram;
					p_kBuf[3+FW_NAME_SIZE] = pConfiguration->mnPLL;
					p_kBuf[4+FW_NAME_SIZE] = (pConfiguration->mnSamplingRate&0x000000ff);
					p_kBuf[5+FW_NAME_SIZE] = ((pConfiguration->mnSamplingRate&0x0000ff00)>>8);
					p_kBuf[6+FW_NAME_SIZE] = ((pConfiguration->mnSamplingRate&0x00ff0000)>>16);
					p_kBuf[7+FW_NAME_SIZE] = ((pConfiguration->mnSamplingRate&0xff000000)>>24);
					strlcpy(&p_kBuf[8+FW_NAME_SIZE], pConfiguration->mpDescription, strlen(pConfiguration->mpDescription)+1);
					ret = copy_to_user(buf, p_kBuf, count);
					if (ret != 0) {
						/* Failed to copy all the data, exit */
						dev_err(pTAS2557->dev, "copy to user fail %d\n", ret);
					}
					kfree(p_kBuf);
				} else
					dev_err(pTAS2557->dev, "read no mem\n");
			} else
				dev_err(pTAS2557->dev, "read buffer not sufficient\n");
		} else
			dev_err(pTAS2557->dev, "%s, firmware not loaded\n", __func__);
	}
	break;

	case TIAUDIO_CMD_FW_TIMESTAMP: {
		if (g_logEnable)
			dev_info(pTAS2557->dev, "TIAUDIO_CMD_FW_TIMESTAMP: count = %d\n", (int)count);

		if (count == 4) {
			p_kBuf = kzalloc(count, GFP_KERNEL);
			if (p_kBuf != NULL) {
				p_kBuf[0] = (pTAS2557->mpFirmware->mnTimeStamp&0x000000ff);
				p_kBuf[1] = ((pTAS2557->mpFirmware->mnTimeStamp&0x0000ff00)>>8);
				p_kBuf[2] = ((pTAS2557->mpFirmware->mnTimeStamp&0x00ff0000)>>16);
				p_kBuf[3] = ((pTAS2557->mpFirmware->mnTimeStamp&0xff000000)>>24);
				ret = copy_to_user(buf, p_kBuf, count);
				if (ret != 0) {
					/* Failed to copy all the data, exit */
					dev_err(pTAS2557->dev, "copy to user fail %d\n", ret);
				}
				kfree(p_kBuf);
			} else
				dev_err(pTAS2557->dev, "read no mem\n");
		}
	}
	break;

	case TIAUDIO_CMD_CALIBRATION: {
		if (g_logEnable)
			dev_info(pTAS2557->dev, "TIAUDIO_CMD_CALIBRATION: count = %d\n", (int)count);

		if (count == 1) {
			unsigned char curCal = pTAS2557->mnCurrentCalibration;

			ret = copy_to_user(buf, &curCal, 1);
			if (ret != 0) {
				/* Failed to copy all the data, exit */
				dev_err(pTAS2557->dev, "copy to user fail %d\n", ret);
			}
		}
	}
	break;

	case TIAUDIO_CMD_SAMPLERATE: {
		if (g_logEnable)
			dev_info(pTAS2557->dev, "TIAUDIO_CMD_SAMPLERATE: count = %d\n", (int)count);
		if (count == 4) {
			p_kBuf = kzalloc(count, GFP_KERNEL);
			if (p_kBuf != NULL) {
				struct TConfiguration *pConfiguration =
					&(pTAS2557->mpFirmware->mpConfigurations[pTAS2557->mnCurrentConfiguration]);

				p_kBuf[0] = (pConfiguration->mnSamplingRate&0x000000ff);
				p_kBuf[1] = ((pConfiguration->mnSamplingRate&0x0000ff00)>>8);
				p_kBuf[2] = ((pConfiguration->mnSamplingRate&0x00ff0000)>>16);
				p_kBuf[3] = ((pConfiguration->mnSamplingRate&0xff000000)>>24);

				ret = copy_to_user(buf, p_kBuf, count);
				if (ret != 0) {
					/* Failed to copy all the data, exit */
					dev_err(pTAS2557->dev, "copy to user fail %d\n", ret);
				}

				kfree(p_kBuf);
			} else
				dev_err(pTAS2557->dev, "read no mem\n");
		}
	}
	break;

	case TIAUDIO_CMD_BITRATE: {
		if (g_logEnable)
			dev_info(pTAS2557->dev,
					"TIAUDIO_CMD_BITRATE: count = %d\n", (int)count);

		if (count == 1) {
			unsigned char bitRate = 0;
			ret = tas2557_get_bit_rate(pTAS2557, &bitRate);
			if (ret >= 0) {
				ret = copy_to_user(buf, &bitRate, 1);
				if (ret != 0) {
					/* Failed to copy all the data, exit */
					dev_err(pTAS2557->dev, "copy to user fail %d\n", ret);
				}
			}
		}
	}
	break;

	case TIAUDIO_CMD_DACVOLUME: {
		if (g_logEnable)
			dev_info(pTAS2557->dev, "TIAUDIO_CMD_DACVOLUME: count = %d\n", (int)count);

		if (count == 1) {
			unsigned char volume = 0;

			ret = tas2557_get_DAC_gain(pTAS2557, &volume);
			if (ret >= 0) {
				ret = copy_to_user(buf, &volume, 1);
				if (ret != 0) {
				/* Failed to copy all the data, exit */
					dev_err(pTAS2557->dev, "copy to user fail %d\n", ret);
				}
			}
		}
	}
	break;
	}
	pTAS2557->mnDBGCmd = 0;

	mutex_unlock(&pTAS2557->file_lock);
	return count;
}

static ssize_t tas2557_file_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	struct tas2557_priv *pTAS2557 = (struct tas2557_priv *)file->private_data;
	int ret = 0;
	unsigned char *p_kBuf = NULL;
	unsigned int reg = 0;
	unsigned int len = 0;

	mutex_lock(&pTAS2557->file_lock);

	p_kBuf = kzalloc(count, GFP_KERNEL);
	if (p_kBuf == NULL) {
		dev_err(pTAS2557->dev, "write no mem\n");
		goto err;
	}

	ret = copy_from_user(p_kBuf, buf, count);
	if (ret != 0) {
		dev_err(pTAS2557->dev, "copy_from_user failed.\n");
		goto err;
	}

	pTAS2557->mnDBGCmd = p_kBuf[0];
	switch (pTAS2557->mnDBGCmd) {
	case TIAUDIO_CMD_REG_WITE:
		if (count > 5) {
			reg = ((unsigned int)p_kBuf[1] << 24)
				+ ((unsigned int)p_kBuf[2] << 16)
				+ ((unsigned int)p_kBuf[3] << 8)
				+ (unsigned int)p_kBuf[4];
			len = count - 5;
			if (len == 1) {
				ret = pTAS2557->write(pTAS2557, reg, p_kBuf[5]);
				if (g_logEnable)
					dev_info(pTAS2557->dev, "TIAUDIO_CMD_REG_WITE, Reg=0x%x, Val=0x%x\n", reg, p_kBuf[5]);
			} else
				ret = pTAS2557->bulk_write(pTAS2557, reg, &p_kBuf[5], len);
		} else
			dev_err(pTAS2557->dev, "%s, write len fail, count=%d.\n", __func__, (int)count);
		pTAS2557->mnDBGCmd = 0;
	break;

	case TIAUDIO_CMD_REG_READ:
		if (count == 5) {
			pTAS2557->mnCurrentReg = ((unsigned int)p_kBuf[1] << 24)
				+ ((unsigned int)p_kBuf[2] << 16)
				+ ((unsigned int)p_kBuf[3] << 8)
				+ (unsigned int)p_kBuf[4];
			if (g_logEnable)
				dev_info(pTAS2557->dev, "TIAUDIO_CMD_REG_READ whole=0x%x\n", pTAS2557->mnCurrentReg);
		} else
			dev_err(pTAS2557->dev, "read len fail.\n");
	break;

	case TIAUDIO_CMD_DEBUG_ON:
		if (count == 2)
			g_logEnable = p_kBuf[1];

		pTAS2557->mnDBGCmd = 0;
	break;

	case TIAUDIO_CMD_PROGRAM:
	{
		if (count == 2) {
			if ((pTAS2557->mpFirmware->mnConfigurations > 0)
				&& (pTAS2557->mpFirmware->mnPrograms > 0)) {
				int config = -1;

				if (p_kBuf[1] == pTAS2557->mnCurrentProgram)
					config = pTAS2557->mnCurrentConfiguration;
				if (g_logEnable)
					dev_info(pTAS2557->dev, "TIAUDIO_CMD_PROGRAM, set to %d, cfg=%d\n", p_kBuf[1], config);
				tas2557_set_program(pTAS2557, p_kBuf[1], config);
				pTAS2557->mnDBGCmd = 0;
			} else
				dev_err(pTAS2557->dev, "%s, firmware not loaded\n", __func__);
		}
	}
	break;

	case TIAUDIO_CMD_CONFIGURATION:
	{
		if (count == 2) {
			if ((pTAS2557->mpFirmware->mnConfigurations > 0)
			&& (pTAS2557->mpFirmware->mnPrograms > 0)) {
				if (g_logEnable)
					dev_info(pTAS2557->dev, "TIAUDIO_CMD_CONFIGURATION, set to %d\n", p_kBuf[1]);
				tas2557_set_config(pTAS2557, p_kBuf[1]);
				pTAS2557->mnDBGCmd = 0;
			} else
				dev_err(pTAS2557->dev, "%s, firmware not loaded\n", __func__);
		}
	}
	break;

	case TIAUDIO_CMD_FW_TIMESTAMP:
	/*let go*/
	break;

	case TIAUDIO_CMD_CALIBRATION:
	{
		if (count == 2) {
			if ((pTAS2557->mpFirmware->mnConfigurations > 0)
			&& (pTAS2557->mpFirmware->mnPrograms > 0)) {
				if (g_logEnable)
					dev_info(pTAS2557->dev, "TIAUDIO_CMD_CALIBRATION, set to %d\n", p_kBuf[1]);
				tas2557_set_calibration(pTAS2557, p_kBuf[1]);
				pTAS2557->mnDBGCmd = 0;
			}
		}
	}
	break;

	case TIAUDIO_CMD_SAMPLERATE:
		if (count == 5) {
			unsigned int nSampleRate = ((unsigned int)p_kBuf[1] << 24) +
				((unsigned int)p_kBuf[2] << 16) +
				((unsigned int)p_kBuf[3] << 8) +
				(unsigned int)p_kBuf[4];
			if (g_logEnable)
				dev_info(pTAS2557->dev, "TIAUDIO_CMD_SAMPLERATE, set to %d\n", nSampleRate);

			tas2557_set_sampling_rate(pTAS2557, nSampleRate);
		}
	break;

	case TIAUDIO_CMD_BITRATE:
		if (count == 2) {
			if (g_logEnable)
				dev_info(pTAS2557->dev, "TIAUDIO_CMD_BITRATE, set to %d\n", p_kBuf[1]);

			tas2557_set_bit_rate(pTAS2557, p_kBuf[1]);
		}
	break;

	case TIAUDIO_CMD_DACVOLUME:
		if (count == 2) {
			unsigned char volume;

			volume = (p_kBuf[1] & 0x0f);
			if (g_logEnable)
				dev_info(pTAS2557->dev, "TIAUDIO_CMD_DACVOLUME, set to %d\n", volume);

			tas2557_set_DAC_gain(pTAS2557, volume);
		}
	break;

	case TIAUDIO_CMD_SPEAKER:
		if (count == 2) {
			if (g_logEnable)
				dev_info(pTAS2557->dev, "TIAUDIO_CMD_SPEAKER, set to %d\n", p_kBuf[1]);
			tas2557_enable(pTAS2557, (p_kBuf[1] > 0));
		}
	break;

	case TIAUDIO_CMD_FW_RELOAD:
		if (count == 1) {
			const char *pFWName;
			if (pTAS2557->mnPGID == TAS2557_PG_VERSION_2P1)
				pFWName = TAS2557_FW_NAME;
			else if (pTAS2557->mnPGID == TAS2557_PG_VERSION_1P0)
				pFWName = TAS2557_PG1P0_FW_NAME;
			else
				break;

			ret = request_firmware_nowait(THIS_MODULE, 1, pFWName,
				pTAS2557->dev, GFP_KERNEL, pTAS2557, tas2557_fw_ready);

			if (g_logEnable)
				dev_info(pTAS2557->dev, "TIAUDIO_CMD_FW_RELOAD: ret = %d\n", ret);
		}
	break;

	default:
		pTAS2557->mnDBGCmd = 0;
	break;
	}

err:
	if (p_kBuf != NULL)
		kfree(p_kBuf);

	mutex_unlock(&pTAS2557->file_lock);

	return count;
}

static long tas2557_file_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct tas2557_priv *pTAS2557 = file->private_data;
	int ret = 0;

	mutex_lock(&pTAS2557->file_lock);

	switch (cmd) {
	case SMARTPA_SPK_DAC_VOLUME:
	{
	}
	break;

	case SMARTPA_SPK_POWER_ON:
	{
		tas2557_enable(pTAS2557, true);
	}
	break;

	case SMARTPA_SPK_POWER_OFF:
	{
		tas2557_enable(pTAS2557, false);
	}
	break;

	case SMARTPA_SPK_SWITCH_PROGRAM:
	{
		if ((pTAS2557->mpFirmware->mnConfigurations > 0)
			&& (pTAS2557->mpFirmware->mnPrograms > 0))
			tas2557_set_program(pTAS2557, arg, -1);
	}
	break;

	case SMARTPA_SPK_SWITCH_CONFIGURATION:
	{
		if ((pTAS2557->mpFirmware->mnConfigurations > 0)
			&& (pTAS2557->mpFirmware->mnPrograms > 0))
			tas2557_set_config(pTAS2557, arg);
	}
	break;

	case SMARTPA_SPK_SWITCH_CALIBRATION:
	{
		if ((pTAS2557->mpFirmware->mnConfigurations > 0)
			&& (pTAS2557->mpFirmware->mnPrograms > 0))
			tas2557_set_calibration(pTAS2557, arg);
	}
	break;

	case SMARTPA_SPK_SET_SAMPLERATE:
	{
		tas2557_set_sampling_rate(pTAS2557, arg);
	}
	break;

	case SMARTPA_SPK_SET_BITRATE:
	{
		tas2557_set_bit_rate(pTAS2557, arg);
	}
	break;
	}

	mutex_unlock(&pTAS2557->file_lock);
	return ret;
}

static const struct file_operations fops = {
	.owner = THIS_MODULE,
	.read = tas2557_file_read,
	.write = tas2557_file_write,
	.unlocked_ioctl = tas2557_file_unlocked_ioctl,
	.open = tas2557_file_open,
	.release = tas2557_file_release,
};

#define MODULE_NAME	"tas2557"
static struct miscdevice tas2557_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = MODULE_NAME,
	.fops = &fops,
};

int tas2557_register_misc(struct tas2557_priv *pTAS2557)
{
	int ret = 0;

	g_tas2557 = pTAS2557;

	ret = misc_register(&tas2557_misc);
	if (ret)
		dev_err(pTAS2557->dev, "TAS2557 misc fail: %d\n", ret);

	dev_info(pTAS2557->dev, "%s, leave\n", __func__);

	return ret;
}

int tas2557_deregister_misc(struct tas2557_priv *pTAS2557)
{
	misc_deregister(&tas2557_misc);
	return 0;
}

MODULE_AUTHOR("Texas Instruments Inc.");
MODULE_DESCRIPTION("TAS2557 Misc Smart Amplifier driver");
MODULE_LICENSE("GPL v2");
#endif
