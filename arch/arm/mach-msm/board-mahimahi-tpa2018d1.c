/* drivers/i2c/chips/tpa2018d1.c
 *
 * TI TPA2018D1 Speaker Amplifier
 *
 * Copyright (C) 2009 HTC Corporation
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/* TODO: content validation in TPA2018_SET_CONFIG */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/tpa2018d1.h>

#include "board-mahimahi-tpa2018d1.h"

static struct i2c_client *this_client;
static struct tpa2018d1_platform_data *pdata;
static int is_on;
static char spk_amp_cfg[8];
static const char spk_amp_on[8] = { /* same length as spk_amp_cfg */
	0x01, 0xc3, 0x20, 0x01, 0x00, 0x08, 0x1a, 0x21
};
static const char spk_amp_off[] = {0x01, 0xa2};

static DEFINE_MUTEX(spk_amp_lock);
static int tpa2018d1_opened;
static char *config_data;
static int tpa2018d1_num_modes;

#define DEBUG 0

static int tpa2018_i2c_write(const char *txData, int length)
{
	struct i2c_msg msg[] = {
		{
			.addr = this_client->addr,
			.flags = 0,
			.len = length,
			.buf = txData,
		},
	};

	if (i2c_transfer(this_client->adapter, msg, 1) < 0) {
		pr_err("%s: I2C transfer error\n", __func__);
		return -EIO;
	} else
		return 0;
}

static int tpa2018_i2c_read(char *rxData, int length)
{
	struct i2c_msg msgs[] = {
		{
			.addr = this_client->addr,
			.flags = I2C_M_RD,
			.len = length,
			.buf = rxData,
		},
	};

	if (i2c_transfer(this_client->adapter, msgs, 1) < 0) {
		pr_err("%s: I2C transfer error\n", __func__);
		return -EIO;
	}

#if DEBUG
	do {
		int i = 0;
		for (i = 0; i < length; i++)
			pr_info("%s: rx[%d] = %2x\n",
				__func__, i, rxData[i]);
	} while(0);
#endif

	return 0;
}

static int tpa2018d1_open(struct inode *inode, struct file *file)
{
	int rc = 0;

	mutex_lock(&spk_amp_lock);

	if (tpa2018d1_opened) {
		pr_err("%s: busy\n", __func__);
		rc = -EBUSY;
		goto done;
	}

	tpa2018d1_opened = 1;
done:
	mutex_unlock(&spk_amp_lock);
	return rc;
}

static int tpa2018d1_release(struct inode *inode, struct file *file)
{
	mutex_lock(&spk_amp_lock);
	tpa2018d1_opened = 0;
	mutex_unlock(&spk_amp_lock);

	return 0;
}

static int tpa2018d1_read_config(void __user *argp)
{
	int rc = 0;
	unsigned char reg_idx = 0x01;
	unsigned char tmp[7];

	if (!is_on) {
		gpio_set_value(pdata->gpio_tpa2018_spk_en, 1);
		msleep(5); /* According to TPA2018D1 Spec */
	}

	rc = tpa2018_i2c_write(&reg_idx, sizeof(reg_idx));
	if (rc < 0)
		goto err;

	rc = tpa2018_i2c_read(tmp, sizeof(tmp));
	if (rc < 0)
		goto err;

	if (copy_to_user(argp, &tmp, sizeof(tmp)))
		rc = -EFAULT;

err:
	if (!is_on)
		gpio_set_value(pdata->gpio_tpa2018_spk_en, 0);
	return rc;
}

static int tpa2018d1_ioctl(struct inode *inode, struct file *file,
		unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int rc = 0;
	int mode = -1;
	int offset = 0;
	struct tpa2018d1_config_data cfg;

	mutex_lock(&spk_amp_lock);

	switch (cmd) {
	case TPA2018_SET_CONFIG:
		if (copy_from_user(spk_amp_cfg, argp, sizeof(spk_amp_cfg)))
			rc = -EFAULT;
		break;

	case TPA2018_READ_CONFIG:
		rc = tpa2018d1_read_config(argp);
		break;

	case TPA2018_SET_MODE:
		if (copy_from_user(&mode, argp, sizeof(mode))) {
			rc = -EFAULT;
			break;
		}
		if (mode >= tpa2018d1_num_modes || mode < 0) {
			pr_err("%s: unsupported tpa2018d1 mode %d\n",
				__func__, mode);
			rc = -EINVAL;
			break;
		}
		if (!config_data) {
			pr_err("%s: no config data!\n", __func__);
			rc = -EIO;
			break;
		}
		memcpy(spk_amp_cfg, config_data + mode * TPA2018D1_CMD_LEN,
			TPA2018D1_CMD_LEN);
		break;

	case TPA2018_SET_PARAM:
		if (copy_from_user(&cfg, argp, sizeof(cfg))) {
			pr_err("%s: copy from user failed.\n", __func__);
			rc = -EFAULT;
			break;
		}
		tpa2018d1_num_modes = cfg.mode_num;
		if (tpa2018d1_num_modes > TPA2018_NUM_MODES) {
			pr_err("%s: invalid number of modes %d\n", __func__,
				tpa2018d1_num_modes);
			rc = -EINVAL;
			break;
		}
		if (cfg.data_len != tpa2018d1_num_modes*TPA2018D1_CMD_LEN) {
			pr_err("%s: invalid data length %d, expecting %d\n",
				__func__, cfg.data_len,
				tpa2018d1_num_modes * TPA2018D1_CMD_LEN);
			rc = -EINVAL;
			break;
		}
		/* Free the old data */
		if (config_data)
			kfree(config_data);
		config_data = kmalloc(cfg.data_len, GFP_KERNEL);
		if (!config_data) {
			pr_err("%s: out of memory\n", __func__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(config_data, cfg.cmd_data, cfg.data_len)) {
			pr_err("%s: copy data from user failed.\n", __func__);
			kfree(config_data);
			config_data = NULL;
			rc = -EFAULT;
			break;
		}
		/* replace default setting with playback setting */
		if (tpa2018d1_num_modes >= TPA2018_MODE_PLAYBACK) {
			offset = TPA2018_MODE_PLAYBACK * TPA2018D1_CMD_LEN;
			memcpy(spk_amp_cfg, config_data + offset,
					TPA2018D1_CMD_LEN);
		}
		break;

	default:
		pr_err("%s: invalid command %d\n", __func__, _IOC_NR(cmd));
		rc = -EINVAL;
		break;
	}
	mutex_unlock(&spk_amp_lock);
	return rc;
}

static struct file_operations tpa2018d1_fops = {
	.owner = THIS_MODULE,
	.open = tpa2018d1_open,
	.release = tpa2018d1_release,
	.ioctl = tpa2018d1_ioctl,
};

static struct miscdevice tpa2018d1_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "tpa2018d1",
	.fops = &tpa2018d1_fops,
};

void tpa2018d1_set_speaker_amp(int on)
{
	if (!pdata) {
		pr_err("%s: no platform data!\n", __func__);
		return;
	}
	mutex_lock(&spk_amp_lock);
	if (on && !is_on) {
		gpio_set_value(pdata->gpio_tpa2018_spk_en, 1);
		msleep(5); /* According to TPA2018D1 Spec */

		if (tpa2018_i2c_write(spk_amp_cfg, sizeof(spk_amp_cfg)) == 0) {
			is_on = 1;
			pr_info("%s: ON\n", __func__);
		}
	} else if (!on && is_on) {
		if (tpa2018_i2c_write(spk_amp_off, sizeof(spk_amp_off)) == 0) {
			is_on = 0;
			msleep(2);
			gpio_set_value(pdata->gpio_tpa2018_spk_en, 0);
			pr_info("%s: OFF\n", __func__);
		}
	}
	mutex_unlock(&spk_amp_lock);
}

static int tpa2018d1_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;

	pdata = client->dev.platform_data;

	if (!pdata) {
		ret = -EINVAL;
		pr_err("%s: platform data is NULL\n", __func__);
		goto err_no_pdata;
	}

	this_client = client;

	ret = gpio_request(pdata->gpio_tpa2018_spk_en, "tpa2018");
	if (ret < 0) {
		pr_err("%s: gpio request aud_spk_en pin failed\n", __func__);
		goto err_free_gpio;
	}

	ret = gpio_direction_output(pdata->gpio_tpa2018_spk_en, 1);
	if (ret < 0) {
		pr_err("%s: request aud_spk_en gpio direction failed\n",
			__func__);
		goto err_free_gpio;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s: i2c check functionality error\n", __func__);
		ret = -ENODEV;
		goto err_free_gpio;
	}

	gpio_set_value(pdata->gpio_tpa2018_spk_en, 0); /* Default Low */

	ret = misc_register(&tpa2018d1_device);
	if (ret) {
		pr_err("%s: tpa2018d1_device register failed\n", __func__);
		goto err_free_gpio;
	}
	memcpy(spk_amp_cfg, spk_amp_on, sizeof(spk_amp_on));
	return 0;

err_free_gpio:
	gpio_free(pdata->gpio_tpa2018_spk_en);
err_no_pdata:
	return ret;
}

static int tpa2018d1_suspend(struct i2c_client *client, pm_message_t mesg)
{
	return 0;
}

static int tpa2018d1_resume(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id tpa2018d1_id[] = {
	{ TPA2018D1_I2C_NAME, 0 },
	{ }
};

static struct i2c_driver tpa2018d1_driver = {
	.probe = tpa2018d1_probe,
	.suspend = tpa2018d1_suspend,
	.resume = tpa2018d1_resume,
	.id_table = tpa2018d1_id,
	.driver = {
		.name = TPA2018D1_I2C_NAME,
	},
};

static int __init tpa2018d1_init(void)
{
	pr_info("%s\n", __func__);
	return i2c_add_driver(&tpa2018d1_driver);
}

module_init(tpa2018d1_init);

MODULE_DESCRIPTION("tpa2018d1 speaker amp driver");
MODULE_LICENSE("GPL");
