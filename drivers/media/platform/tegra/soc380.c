/*
 * soc380.c - soc380 sensor driver
 *
 * Copyright (c) 2011-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * Contributors:
 *      Abhinav Sinha <absinha@nvidia.com>
 *
 * Leverage OV2710.c
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

/**
 * SetMode Sequence for 640x480. Phase 0. Sensor Dependent.
 * This sequence should put sensor in streaming mode for 640x480
 * This is usually given by the FAE or the sensor vendor.
 */

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/module.h>

#include <media/soc380.h>

struct soc380_reg {
	u16 addr;
	u16 val;
};

struct soc380_info {
	int mode;
	struct i2c_client *i2c_client;
	struct soc380_platform_data *pdata;
	struct clk *mclk;
};

#define SOC380_TABLE_WAIT_MS 0
#define SOC380_TABLE_END 1
#define SOC380_MAX_RETRIES 3

static struct soc380_reg mode_640x480[] = {
	{0x001A, 0x0011},

	{SOC380_TABLE_WAIT_MS, 1},

	{0x001A, 0x0010},

	{SOC380_TABLE_WAIT_MS, 1},

	{0x0018, 0x4028},
	{0x001A, 0x0210},
	{0x0010, 0x021c},
	{0x0012, 0x0000},
	{0x0014, 0x244B},

	{SOC380_TABLE_WAIT_MS, 10},

	{0x0014, 0x304B},

	{SOC380_TABLE_WAIT_MS, 50},

	{0x0014, 0xB04A},

	{0x098C, 0x2703},
	{0x0990, 0x0280},
	{0x098C, 0x2705},
	{0x0990, 0x01E0},
	{0x098C, 0x2707},
	{0x0990, 0x0280},
	{0x098C, 0x2709},
	{0x0990, 0x01E0},
	{0x098C, 0x270D},
	{0x0990, 0x0000},
	{0x098C, 0x270F},
	{0x0990, 0x0000},
	{0x098C, 0x2711},
	{0x0990, 0x01E7},
	{0x098C, 0x2713},
	{0x0990, 0x0287},
	{0x098C, 0x2715},
	{0x0990, 0x0001},
	{0x098C, 0x2717},
	{0x0990, 0x0026},
	{0x098C, 0x2719},
	{0x0990, 0x001A},
	{0x098C, 0x271B},
	{0x0990, 0x006B},
	{0x098C, 0x271D},
	{0x0990, 0x006B},
	{0x098C, 0x271F},
	{0x0990, 0x022A},
	{0x098C, 0x2721},
	{0x0990, 0x034A},
	{0x098C, 0x2723},
	{0x0990, 0x0000},
	{0x098C, 0x2725},
	{0x0990, 0x0000},
	{0x098C, 0x2727},
	{0x0990, 0x01E7},
	{0x098C, 0x2729},
	{0x0990, 0x0287},
	{0x098C, 0x272B},
	{0x0990, 0x0001},
	{0x098C, 0x272D},
	{0x0990, 0x0026},
	{0x098C, 0x272F},
	{0x0990, 0x001A},
	{0x098C, 0x2731},
	{0x0990, 0x006B},
	{0x098C, 0x2733},
	{0x0990, 0x006B},
	{0x098C, 0x2735},
	{0x0990, 0x022A},
	{0x098C, 0x2737},
	{0x0990, 0x034A},
	{0x098C, 0x2739},
	{0x0990, 0x0000},
	{0x098C, 0x273B},
	{0x0990, 0x027F},
	{0x098C, 0x273D},
	{0x0990, 0x0000},
	{0x098C, 0x273F},
	{0x0990, 0x01DF},
	{0x098C, 0x2747},
	{0x0990, 0x0000},
	{0x098C, 0x2749},
	{0x0990, 0x027F},
	{0x098C, 0x274B},
	{0x0990, 0x0000},
	{0x098C, 0x274D},
	{0x0990, 0x01DF},
	{0x098C, 0x222D},
	{0x0990, 0x008B},
	{0x098C, 0xA408},
	{0x0990, 0x0021},
	{0x098C, 0xA409},
	{0x0990, 0x0023},
	{0x098C, 0xA40A},
	{0x0990, 0x0028},
	{0x098C, 0xA40B},
	{0x0990, 0x002A},
	{0x098C, 0x2411},
	{0x0990, 0x008B},
	{0x098C, 0x2413},
	{0x0990, 0x00A6},
	{0x098C, 0x2415},
	{0x0990, 0x008B},
	{0x098C, 0x2417},
	{0x0990, 0x00A6},
	{0x098C, 0xA404},
	{0x0990, 0x0010},
	{0x098C, 0xA40D},
	{0x0990, 0x0002},
	{0x098C, 0xA40E},
	{0x0990, 0x0003},
	{0x098C, 0xA410},
	{0x0990, 0x000A},
	{0x098C, 0xA215},
	{0x0990, 0x0003},
	{0x098C, 0xA20C},
	{0x0990, 0x0003},

	{0x098C, 0xA103},
	{0x0990, 0x0006},
	{SOC380_TABLE_WAIT_MS, 100},

	{0x098C, 0xA103},
	{0x0990, 0x0005},
	{SOC380_TABLE_WAIT_MS, 50},

	{SOC380_TABLE_END, 0x0000}
};

enum {
	SOC380_MODE_680x480,
};

static struct soc380_reg *mode_table[] = {
	[SOC380_MODE_680x480] = mode_640x480,
};

static int soc380_read_reg(struct i2c_client *client, u16 addr, u16 *val)
{
	int err;
	struct i2c_msg msg[2];
	unsigned char data[4];

	if (!client->adapter)
		return -ENODEV;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = data;

	/* high byte goes out first */
	data[0] = (u8) (addr >> 8);
	data[1] = (u8) (addr & 0xff);

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 2;
	msg[1].buf = data + 2;

	err = i2c_transfer(client->adapter, msg, 2);

	if (err != 2)
		return -EINVAL;

	*val = data[2] << 8 | data[3];

	return 0;
}

static int soc380_write_reg(struct i2c_client *client, u16 addr, u16 val)
{
	int err;
	struct i2c_msg msg;
	unsigned char data[4];
	int retry = 0;

	if (!client->adapter)
		return -ENODEV;

	data[0] = (u8) (addr >> 8);
	data[1] = (u8) (addr & 0xff);
	data[2] = (u8) (val >> 8);
	data[3] = (u8) (val & 0xff);

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 4;
	msg.buf = data;

	do {
		err = i2c_transfer(client->adapter, &msg, 1);
		if (err == 1)
			return 0;
		retry++;
		pr_err("soc380: i2c transfer failed, retrying %x %x\n",
		       addr, val);
		msleep(3);
	} while (retry <= SOC380_MAX_RETRIES);

	return err;
}

static int soc380_write_table(struct i2c_client *client,
			      const struct soc380_reg table[],
			      const struct soc380_reg override_list[],
			      int num_override_regs)
{
	int err;
	const struct soc380_reg *next;
	int i;
	u16 val;

	for (next = table; next->addr != SOC380_TABLE_END; next++) {
		if (next->addr == SOC380_TABLE_WAIT_MS) {
			msleep(next->val);
			continue;
		}

		val = next->val;

		/* When an override list is passed in, replace the reg */
		/* value to write if the reg is in the list            */
		if (override_list) {
			for (i = 0; i < num_override_regs; i++) {
				if (next->addr == override_list[i].addr) {
					val = override_list[i].val;
					break;
				}
			}
		}

		err = soc380_write_reg(client, next->addr, val);
		if (err)
			return err;
	}
	return 0;
}

static int soc380_set_mode(struct soc380_info *info, struct soc380_mode *mode)
{
	int sensor_mode;
	int err;

	pr_info("%s: xres %u yres %u\n", __func__, mode->xres, mode->yres);
	if (mode->xres == 640 && mode->yres == 480)
		sensor_mode = SOC380_MODE_680x480;
	else {
		pr_err("%s: invalid resolution supplied to set mode %d %d\n",
		       __func__, mode->xres, mode->yres);
		return -EINVAL;
	}

	err = soc380_write_table(info->i2c_client, mode_table[sensor_mode],
		NULL, 0);
	if (err)
		return err;

	info->mode = sensor_mode;
	return 0;
}

static int soc380_get_status(struct soc380_info *info,
		struct soc380_status *dev_status)
{
	int err;

	err = soc380_write_reg(info->i2c_client, 0x98C, dev_status->data);
	if (err)
		return err;

	err = soc380_read_reg(info->i2c_client, 0x0990,
		(u16 *) &dev_status->status);
	if (err)
		return err;

	return err;
}

static long soc380_ioctl(struct file *file,
			 unsigned int cmd, unsigned long arg)
{
	int err;
	struct soc380_info *info = file->private_data;

	switch (_IOC_NR(cmd)) {
	case _IOC_NR(SOC380_IOCTL_SET_MODE):
	{
		struct soc380_mode mode;
		if (copy_from_user(&mode,
				   (const void __user *)arg,
				   sizeof(struct soc380_mode))) {
			return -EFAULT;
		}

		return soc380_set_mode(info, &mode);
	}
	case _IOC_NR(SOC380_IOCTL_GET_STATUS):
	{
		struct soc380_status dev_status;
		if (copy_from_user(&dev_status,
				   (const void __user *)arg,
				   sizeof(struct soc380_status))) {
			return -EFAULT;
		}

		err = soc380_get_status(info, &dev_status);
		if (err)
			return err;
		if (copy_to_user((void __user *)arg, &dev_status,
				 sizeof(struct soc380_status))) {
			return -EFAULT;
		}
		return 0;
	}
	default:
		return -EINVAL;
	}
	return 0;
}

static struct soc380_info *info;

static void soc380_mclk_disable(struct soc380_info *info)
{
	dev_dbg(&info->i2c_client->dev, "%s: disable MCLK\n", __func__);
	clk_disable_unprepare(info->mclk);
}

static int soc380_mclk_enable(struct soc380_info *info)
{
	int err;
	unsigned long mclk_init_rate = 24000000;

	dev_dbg(&info->i2c_client->dev, "%s: enable MCLK with %lu Hz\n",
		__func__, mclk_init_rate);

	err = clk_set_rate(info->mclk, mclk_init_rate);
	if (!err)
		err = clk_prepare_enable(info->mclk);
	return err;
}

static int soc380_open(struct inode *inode, struct file *file)
{
	struct soc380_status dev_status;
	int err = soc380_mclk_enable(info);
	if (err < 0)
		goto fail_mclk;

	file->private_data = info;

	if (info->pdata && info->pdata->power_on) {
		err = info->pdata->power_on(&info->i2c_client->dev);
		if (err < 0)
			goto fail_power_on;
	}

	dev_status.data = 0;
	dev_status.status = 0;

	err = soc380_get_status(info, &dev_status);
	if (err < 0)
		goto fail_status;

	return 0;

fail_status:
	if (info->pdata && info->pdata->power_off)
		info->pdata->power_off(&info->i2c_client->dev);
fail_power_on:
	soc380_mclk_disable(info);
fail_mclk:
	file->private_data = NULL;
	return err;
}

static int soc380_release(struct inode *inode, struct file *file)
{
	if (info->pdata && info->pdata->power_off)
		info->pdata->power_off(&info->i2c_client->dev);
	soc380_mclk_disable(info);
	file->private_data = NULL;
	return 0;
}

static const struct file_operations soc380_fileops = {
	.owner = THIS_MODULE,
	.open = soc380_open,
	.unlocked_ioctl = soc380_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = soc380_ioctl,
#endif
	.release = soc380_release,
};

static struct miscdevice soc380_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "soc380",
	.fops = &soc380_fileops,
};

static int soc380_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int err;
	const char *mclk_name;

	pr_info("soc380: probing sensor.\n");

	info = kzalloc(sizeof(struct soc380_info), GFP_KERNEL);
	if (!info) {
		pr_err("soc380: Unable to allocate memory!\n");
		return -ENOMEM;
	}

	info->pdata = client->dev.platform_data;
	info->i2c_client = client;

	mclk_name = info->pdata && info->pdata->mclk_name ?
		    info->pdata->mclk_name : "default_mclk";
	info->mclk = devm_clk_get(&client->dev, mclk_name);
	if (IS_ERR(info->mclk)) {
		dev_err(&client->dev, "%s: unable to get clock %s\n",
			__func__, mclk_name);
		kfree(info);
		return PTR_ERR(info->mclk);
	}

	i2c_set_clientdata(client, info);

	err = misc_register(&soc380_device);
	if (err) {
		pr_err("soc380: Unable to register misc device!\n");
		kfree(info);
		return err;
	}

	return 0;
}

static int soc380_remove(struct i2c_client *client)
{
	struct soc380_info *info;
	info = i2c_get_clientdata(client);
	misc_deregister(&soc380_device);
	kfree(info);
	return 0;
}

static const struct i2c_device_id soc380_id[] = {
	{ "soc380", 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, soc380_id);

static struct i2c_driver soc380_i2c_driver = {
	.driver = {
		.name = "soc380",
		.owner = THIS_MODULE,
	},
	.probe = soc380_probe,
	.remove = soc380_remove,
	.id_table = soc380_id,
};

static int __init soc380_init(void)
{
	pr_info("soc380 sensor driver loading\n");
	return i2c_add_driver(&soc380_i2c_driver);
}

static void __exit soc380_exit(void)
{
	i2c_del_driver(&soc380_i2c_driver);
}

module_init(soc380_init);
module_exit(soc380_exit);
MODULE_LICENSE("GPL v2");
