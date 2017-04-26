/*
** =============================================================================
** Copyright (c) 2016  Texas Instruments Inc.
** Copyright (C) 2016 XiaoMi, Inc.
**
** This program is free software; you can redistribute it and/or modify it under
** the terms of the GNU General Public License as published by the Free Software
** Foundation; version 2.
**
** This program is distributed in the hope that it will be useful, but WITHOUT
** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
** FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License along with
** this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
** Street, Fifth Floor, Boston, MA 02110-1301, USA.
**
** File:
**     tas2555-misc.c
**
** Description:
**     misc driver for Texas Instruments TAS2555 High Performance 4W Smart Amplifier
**
** =============================================================================
*/

#ifdef CONFIG_TAS2555_MISC

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
#include <asm/uaccess.h>


#include "tas2555.h"
#include "tas2555-core.h"
#include "tas2555-misc.h"
#include <linux/dma-mapping.h>

#define TAS2555_FW_FULL_NAME     "/etc/firmware/tas2555_uCDSP.bin"

static int g_logEnable = 1;
static struct tas2555_priv *g_tas2555;

static int tas2555_set_bit_rate(struct tas2555_priv *pTAS2555, unsigned int nBitRate)
{
	int ret = 0, n = -1;

	dev_dbg(pTAS2555->dev, "tas2555_set_bit_rate: nBitRate = %d \n",
		nBitRate);
	switch (nBitRate) {
	case 16:
		n = 0;
	break;
	case 20:
		n = 1;
	break;
	case 24:
		n = 2;
	break;
	case 32:
		n = 3;
	break;
	}

	if (n >= 0)
		ret = pTAS2555->update_bits(pTAS2555,
			TAS2555_ASI1_DAC_FORMAT_REG, 0x18, n<<3);

	return ret;
}

static int tas2555_file_open(struct inode *inode, struct file *file)
{
	struct tas2555_priv *pTAS2555 = g_tas2555;

	if (!try_module_get(THIS_MODULE))
		return -ENODEV;

	file->private_data = (void *)pTAS2555;

	if (g_logEnable)
		dev_info(pTAS2555->dev,
			"%s\n", __FUNCTION__);
	return 0;
}

static int tas2555_file_release(struct inode *inode, struct file *file)
{
	struct tas2555_priv *pTAS2555 = (struct tas2555_priv *)file->private_data;

	if (g_logEnable)
		dev_info(pTAS2555->dev,
			"%s\n", __FUNCTION__);

	file->private_data = (void *)NULL;
	module_put(THIS_MODULE);

	return 0;
}

static ssize_t tas2555_file_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	struct tas2555_priv *pTAS2555 = (struct tas2555_priv *)file->private_data;
	int ret = 0;
	unsigned int nValue = 0;
	unsigned char value = 0;
	unsigned char *p_kBuf = NULL;

	mutex_lock(&pTAS2555->file_lock);

	switch (pTAS2555->mnDBGCmd) {
	case TIAUDIO_CMD_REG_READ:
		{
			if (g_logEnable)
				dev_info(pTAS2555->dev,
					"TIAUDIO_CMD_REG_READ: current_reg = 0x%x, count=%d\n", pTAS2555->mnCurrentReg, (int)count);
			if (count == 1) {
				ret = pTAS2555->read(pTAS2555, pTAS2555->mnCurrentReg, &nValue);
				if (0 > ret) {
					dev_err(pTAS2555->dev, "dev read fail %d\n", ret);
					break;
				}

				value = (u8)nValue;
				if (g_logEnable)
					dev_info(pTAS2555->dev,
						"TIAUDIO_CMD_REG_READ: nValue=0x%x, value=0x%x\n",
						nValue, value);
				ret = copy_to_user(buf, &value, 1);
				if (0 != ret) {
					/* Failed to copy all the data, exit */
					dev_err(pTAS2555->dev, "copy to user fail %d\n", ret);
				}
			} else if (count > 1) {
				p_kBuf = (unsigned char *)kzalloc(count, GFP_KERNEL);
				if (p_kBuf != NULL) {
					ret = pTAS2555->bulk_read(pTAS2555, pTAS2555->mnCurrentReg, p_kBuf, count);
					if (0 > ret) {
						dev_err(pTAS2555->dev, "dev bulk read fail %d\n", ret);
					} else {
						ret = copy_to_user(buf, p_kBuf, count);
						if (0 != ret) {
							/* Failed to copy all the data, exit */
							dev_err(pTAS2555->dev, "copy to user fail %d\n", ret);
						}
					}

					kfree(p_kBuf);
				} else {
					dev_err(pTAS2555->dev, "read no mem\n");
				}
			}
		}
		break;

	case TIAUDIO_CMD_PROGRAM:
		{
			if (g_logEnable)
				dev_info(pTAS2555->dev,
						"TIAUDIO_CMD_PROGRAM: count = %d\n",
						(int)count);

			if (count == PROGRAM_BUF_SIZE) {
				p_kBuf = (unsigned char *)kzalloc(count, GFP_KERNEL);
				if (p_kBuf != NULL) {
					TProgram *pProgram =
						&(pTAS2555->mpFirmware->mpPrograms[pTAS2555->mnCurrentProgram]);

					p_kBuf[0] = pTAS2555->mpFirmware->mnPrograms;
					p_kBuf[1] = pTAS2555->mnCurrentProgram;
					memcpy(&p_kBuf[2], pProgram->mpName, FW_NAME_SIZE);
					strcpy(&p_kBuf[2+FW_NAME_SIZE], pProgram->mpDescription);

					ret = copy_to_user(buf, p_kBuf, count);
					if (0 != ret) {
						/* Failed to copy all the data, exit */
						dev_err(pTAS2555->dev, "copy to user fail %d\n", ret);
					}

					kfree(p_kBuf);
				} else {
					dev_err(pTAS2555->dev, "read no mem\n");
				}
			} else {
				dev_err(pTAS2555->dev, "read buffer not sufficient\n");
			}
		}
		break;

	case TIAUDIO_CMD_CONFIGURATION:
		{
			if (g_logEnable)
				dev_info(pTAS2555->dev,
						"TIAUDIO_CMD_CONFIGURATION: count = %d\n",
						(int)count);

			if (count == CONFIGURATION_BUF_SIZE) {
				p_kBuf = (unsigned char *)kzalloc(count, GFP_KERNEL);
				if (p_kBuf != NULL) {
					TConfiguration *pConfiguration =
						&(pTAS2555->mpFirmware->mpConfigurations[pTAS2555->mnCurrentConfiguration]);

					p_kBuf[0] = pTAS2555->mpFirmware->mnConfigurations;
					p_kBuf[1] = pTAS2555->mnCurrentConfiguration;
					memcpy(&p_kBuf[2], pConfiguration->mpName, FW_NAME_SIZE);
					p_kBuf[2+FW_NAME_SIZE] = pConfiguration->mnProgram;
					p_kBuf[3+FW_NAME_SIZE] = pConfiguration->mnPLL;
					p_kBuf[4+FW_NAME_SIZE] = (pConfiguration->mnSamplingRate&0x000000ff);
					p_kBuf[5+FW_NAME_SIZE] = ((pConfiguration->mnSamplingRate&0x0000ff00)>>8);
					p_kBuf[6+FW_NAME_SIZE] = ((pConfiguration->mnSamplingRate&0x00ff0000)>>16);
					p_kBuf[7+FW_NAME_SIZE] = ((pConfiguration->mnSamplingRate&0xff000000)>>24);
					strcpy(&p_kBuf[8+FW_NAME_SIZE], pConfiguration->mpDescription);

					ret = copy_to_user(buf, p_kBuf, count);
					if (0 != ret) {
						/* Failed to copy all the data, exit */
						dev_err(pTAS2555->dev, "copy to user fail %d\n", ret);
					}

					kfree(p_kBuf);
				} else {
					dev_err(pTAS2555->dev, "read no mem\n");
				}
			} else {
				dev_err(pTAS2555->dev, "read buffer not sufficient\n");
			}
		}
		break;

	case TIAUDIO_CMD_FW_TIMESTAMP:
		{
			if (g_logEnable)
				dev_info(pTAS2555->dev,
						"TIAUDIO_CMD_FW_TIMESTAMP: count = %d\n",
						(int)count);

			if (count == 4) {
				p_kBuf = (unsigned char *)kzalloc(count, GFP_KERNEL);
				if (p_kBuf != NULL) {
					p_kBuf[0] = (pTAS2555->mpFirmware->mnTimeStamp&0x000000ff);
					p_kBuf[1] = ((pTAS2555->mpFirmware->mnTimeStamp&0x0000ff00)>>8);
					p_kBuf[2] = ((pTAS2555->mpFirmware->mnTimeStamp&0x00ff0000)>>16);
					p_kBuf[3] = ((pTAS2555->mpFirmware->mnTimeStamp&0xff000000)>>24);

					ret = copy_to_user(buf, p_kBuf, count);
					if (0 != ret) {
						/* Failed to copy all the data, exit */
						dev_err(pTAS2555->dev, "copy to user fail %d\n", ret);
					}

					kfree(p_kBuf);
				} else {
					dev_err(pTAS2555->dev, "read no mem\n");
				}
			}
		}
		break;

	case TIAUDIO_CMD_CALIBRATION:
		{
			if (g_logEnable)
				dev_info(pTAS2555->dev,
						"TIAUDIO_CMD_CALIBRATION: count = %d\n",
						(int)count);

			if (count == 1) {
				unsigned char curCal = pTAS2555->mnCurrentCalibration;
				ret = copy_to_user(buf, &curCal, 1);
				if (0 != ret) {
					/* Failed to copy all the data, exit */
					dev_err(pTAS2555->dev, "copy to user fail %d\n", ret);
				}
			}
		}
		break;

	case TIAUDIO_CMD_SAMPLERATE:
		{
			if (g_logEnable)
				dev_info(pTAS2555->dev,
						"TIAUDIO_CMD_SAMPLERATE: count = %d\n",
						(int)count);
			if (count == 4) {
				p_kBuf = (unsigned char *)kzalloc(count, GFP_KERNEL);
				if (p_kBuf != NULL) {
					TConfiguration *pConfiguration =
						&(pTAS2555->mpFirmware->mpConfigurations[pTAS2555->mnCurrentConfiguration]);

					p_kBuf[0] = (pConfiguration->mnSamplingRate&0x000000ff);
					p_kBuf[1] = ((pConfiguration->mnSamplingRate&0x0000ff00)>>8);
					p_kBuf[2] = ((pConfiguration->mnSamplingRate&0x00ff0000)>>16);
					p_kBuf[3] = ((pConfiguration->mnSamplingRate&0xff000000)>>24);

					ret = copy_to_user(buf, p_kBuf, count);
					if (0 != ret) {
						/* Failed to copy all the data, exit */
						dev_err(pTAS2555->dev, "copy to user fail %d\n", ret);
					}

					kfree(p_kBuf);
				} else {
					dev_err(pTAS2555->dev, "read no mem\n");
				}
			}
		}
		break;

	case TIAUDIO_CMD_BITRATE:
		{
			if (g_logEnable)
				dev_info(pTAS2555->dev,
						"TIAUDIO_CMD_BITRATE: count = %d\n",
						(int)count);

			if (count == 1) {
				unsigned int dac_format = 0;
				unsigned char bitRate = 0;
				ret = pTAS2555->read(pTAS2555,
					TAS2555_ASI1_DAC_FORMAT_REG, &dac_format);
				if (ret >= 0) {
					bitRate = (dac_format&0x18)>>3;
					if (bitRate == 0)
						bitRate = 16;
					else if (bitRate == 1)
						bitRate = 20;
					else if (bitRate == 2)
						bitRate = 24;
					else if (bitRate == 3)
						bitRate = 32;
					ret = copy_to_user(buf, &bitRate, 1);
					if (0 != ret) {
					/* Failed to copy all the data, exit */
						dev_err(pTAS2555->dev, "copy to user fail %d\n", ret);
					}
				}
			}
		}
		break;

	case TIAUDIO_CMD_DACVOLUME:
		{
			if (g_logEnable)
				dev_info(pTAS2555->dev,
						"TIAUDIO_CMD_DACVOLUME: count = %d\n",
						(int)count);

			if (count == 1) {
				unsigned int value = 0;
				unsigned char volume = 0;
				ret = pTAS2555->read(pTAS2555,
					TAS2555_SPK_CTRL_REG, &value);
				if (ret >= 0) {
					volume = (value&0x78)>>3;
					ret = copy_to_user(buf, &volume, 1);
					if (0 != ret) {
					/* Failed to copy all the data, exit */
						dev_err(pTAS2555->dev, "copy to user fail %d\n", ret);
					}
				}
			}
		}
		break;
	}
	 pTAS2555->mnDBGCmd = 0;

	mutex_unlock(&pTAS2555->file_lock);
	return count;
}

static ssize_t tas2555_file_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	struct tas2555_priv *pTAS2555 = (struct tas2555_priv *)file->private_data;
	int ret = 0;

	unsigned char *p_kBuf = NULL;
	unsigned int reg = 0;
	unsigned int len = 0;

	mutex_lock(&pTAS2555->file_lock);

	p_kBuf = (unsigned char *)kzalloc(count, GFP_KERNEL);
	if (p_kBuf == NULL) {
		dev_err(pTAS2555->dev, "write no mem\n");
		goto err;
	}

	ret = copy_from_user(p_kBuf, buf, count);
	if (0 != ret) {
		dev_err(pTAS2555->dev, "copy_from_user failed.\n");
		goto err;
	}

	pTAS2555->mnDBGCmd = p_kBuf[0];
	switch (pTAS2555->mnDBGCmd) {
	case TIAUDIO_CMD_REG_WITE:
		if (count > 5) {
			reg = ((unsigned int)p_kBuf[1] << 24) +
				((unsigned int)p_kBuf[2] << 16) +
				((unsigned int)p_kBuf[3] << 8) +
				(unsigned int)p_kBuf[4];
			len = count - 5;
			if (len == 1) {
				ret = pTAS2555->write(pTAS2555, reg, p_kBuf[5]);
				if (g_logEnable)
					dev_info(pTAS2555->dev,
					"TIAUDIO_CMD_REG_WITE, Reg=0x%x, Val=0x%x\n",
					reg, p_kBuf[5]);
			} else {
				ret = pTAS2555->bulk_write(pTAS2555, reg, &p_kBuf[5], len);
			}
		} else {
			dev_err(pTAS2555->dev, "%s, write len fail, count=%d.\n",
				__FUNCTION__, (int)count);
		}
		pTAS2555->mnDBGCmd = 0;
		break;

	case TIAUDIO_CMD_REG_READ:
		if (count == 5) {
			pTAS2555->mnCurrentReg = ((unsigned int)p_kBuf[1] << 24) +
				((unsigned int)p_kBuf[2] << 16) +
				((unsigned int)p_kBuf[3] << 8) 	+
				(unsigned int)p_kBuf[4];
			if (g_logEnable) {
				dev_info(pTAS2555->dev,
					"TIAUDIO_CMD_REG_READ, whole=0x%x\n",
					pTAS2555->mnCurrentReg);
			}
		} else {
			dev_err(pTAS2555->dev, "read len fail.\n");
		}
		break;

	case TIAUDIO_CMD_DEBUG_ON:
		{
			if (count == 2) {
				g_logEnable = p_kBuf[1];
			}
			pTAS2555->mnDBGCmd = 0;
		}
		break;

	case TIAUDIO_CMD_PROGRAM:
		{
			if (count == 2) {
				if (g_logEnable)
					dev_info(pTAS2555->dev,
					"TIAUDIO_CMD_PROGRAM, set to %d\n",
					p_kBuf[1]);
				tas2555_set_program(pTAS2555, p_kBuf[1]);
				pTAS2555->mnDBGCmd = 0;
			}
		}
		break;

	case TIAUDIO_CMD_CONFIGURATION:
		{
			if (count == 2) {
				if (g_logEnable)
					dev_info(pTAS2555->dev,
					"TIAUDIO_CMD_CONFIGURATION, set to %d\n",
					p_kBuf[1]);
				tas2555_set_config(pTAS2555, p_kBuf[1]);
				pTAS2555->mnDBGCmd = 0;
			}
		}
		break;

	case TIAUDIO_CMD_FW_TIMESTAMP:
		/*let go*/
		break;

	case TIAUDIO_CMD_CALIBRATION:
		{
			if (count == 2) {
				if (g_logEnable)
					dev_info(pTAS2555->dev,
					"TIAUDIO_CMD_CALIBRATION, set to %d\n",
					p_kBuf[1]);
				tas2555_set_calibration(pTAS2555, p_kBuf[1]);
				pTAS2555->mnDBGCmd = 0;
			}
		}
		break;

	case TIAUDIO_CMD_SAMPLERATE:
		{
			if (count == 5) {
				unsigned int nSampleRate = ((unsigned int)p_kBuf[1] << 24) +
					((unsigned int)p_kBuf[2] << 16) +
					((unsigned int)p_kBuf[3] << 8) 	+
					(unsigned int)p_kBuf[4];
				if (g_logEnable)
					dev_info(pTAS2555->dev,
					"TIAUDIO_CMD_SAMPLERATE, set to %d\n",
					nSampleRate);

				tas2555_set_sampling_rate(pTAS2555, nSampleRate);
			}
		}
		break;

	case TIAUDIO_CMD_BITRATE:
		{
			if (count == 2) {
				if (g_logEnable)
					dev_info(pTAS2555->dev,
					"TIAUDIO_CMD_BITRATE, set to %d\n",
					p_kBuf[1]);

				tas2555_set_bit_rate(pTAS2555, p_kBuf[1]);
			}
		}
		break;

	case TIAUDIO_CMD_DACVOLUME:
		{
			if (count == 2) {
				unsigned char volume = (p_kBuf[1] & 0x0f);
				if (g_logEnable)
					dev_info(pTAS2555->dev,
					"TIAUDIO_CMD_DACVOLUME, set to %d\n",
					volume);

				pTAS2555->update_bits(pTAS2555,
					TAS2555_SPK_CTRL_REG, 0x78, volume<<3);
			}
		}
		break;

	case TIAUDIO_CMD_SPEAKER:
		{
			if (count == 2) {
				if (g_logEnable)
					dev_info(pTAS2555->dev,
					"TIAUDIO_CMD_SPEAKER, set to %d\n",
					p_kBuf[1]);
				tas2555_enable(pTAS2555, (p_kBuf[1] > 0));
			}
		}
		break;

	case TIAUDIO_CMD_FW_RELOAD:
		{
			if (count == 1) {
				ret = request_firmware_nowait(THIS_MODULE, 1, TAS2555_FW_NAME,
					pTAS2555->dev, GFP_KERNEL, pTAS2555, tas2555_fw_ready);

				if (g_logEnable)
					dev_info(pTAS2555->dev,
						"TIAUDIO_CMD_FW_RELOAD: ret = %d\n",
						ret);
			}

		}
		break;

	default:
		pTAS2555->mnDBGCmd = 0;
		break;
	}

err:
	if (p_kBuf != NULL)
		kfree(p_kBuf);

	mutex_unlock(&pTAS2555->file_lock);

	return count;
}

static long tas2555_file_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct tas2555_priv *pTAS2555 = file->private_data;
	int ret = 0;

	mutex_lock(&pTAS2555->file_lock);

	switch (cmd) {
	case SMARTPA_SPK_DAC_VOLUME:
	{
		u8 volume = (arg & 0x0f);
		pTAS2555->update_bits(pTAS2555,
			TAS2555_SPK_CTRL_REG, 0x78, volume<<3);
	}
	break;

	case SMARTPA_SPK_POWER_ON:
	{
		tas2555_enable(pTAS2555, true);
	}
	break;

	case SMARTPA_SPK_POWER_OFF:
	{
		tas2555_enable(pTAS2555, false);
	}
	break;

	case SMARTPA_SPK_SWITCH_PROGRAM:
	{
		tas2555_set_program(pTAS2555, arg);
	}
	break;

	case SMARTPA_SPK_SWITCH_CONFIGURATION:
	{
		tas2555_set_config(pTAS2555, arg);
	}
	break;

	case SMARTPA_SPK_SWITCH_CALIBRATION:
	{
		tas2555_set_calibration(pTAS2555, arg);
	}
	break;

	case SMARTPA_SPK_SET_SAMPLERATE:
	{
		tas2555_set_sampling_rate(pTAS2555, arg);
	}
	break;

	case SMARTPA_SPK_SET_BITRATE:
	{
		tas2555_set_bit_rate(pTAS2555, arg);
	}
	break;
	}

	mutex_unlock(&pTAS2555->file_lock);
	return ret;
}

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.read = tas2555_file_read,
	.write = tas2555_file_write,
	.unlocked_ioctl = tas2555_file_unlocked_ioctl,
	.open = tas2555_file_open,
	.release = tas2555_file_release,
};

#define MODULE_NAME	"tas2555"
static struct miscdevice tas2555_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = MODULE_NAME,
	.fops = &fops,
};

int tas2555_register_misc(struct tas2555_priv *pTAS2555)
{
	int ret = 0;

	g_tas2555 = pTAS2555;

	ret = misc_register(&tas2555_misc);
	if (ret) {
		dev_err(pTAS2555->dev, "TAS2555 misc fail: %d\n", ret);
	}

	dev_info(pTAS2555->dev, "%s, leave\n", __FUNCTION__);

	return ret;
}

int tas2555_deregister_misc(struct tas2555_priv *pTAS2555)
{
	misc_deregister(&tas2555_misc);
	return 0;
}

MODULE_AUTHOR("Texas Instruments Inc.");
MODULE_DESCRIPTION("TAS2555 Misc Smart Amplifier driver");
MODULE_LICENSE("GPLv2");
#endif
