/*
 * ov5640.c - ov5640 sensor driver
 *
 * Copyright (c) 2011-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * Contributors:
 *      Abhinav Sinha <absinha@nvidia.com>
 *
 * Leverage soc380.c
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
#include <linux/tegra-pm.h>
#include <media/ov5640.h>

#include "ov5640_tables.h"

/* Focuser single step & full scale transition time truth table
 * in the format of:
 *    index	mode		single step transition	full scale transition
 *	0	0			0			0
 *	1	1			50uS			51.2mS
 *	2	1			100uS			102.3mS
 *	3	1			200uS			204.6mS
 *	4	1			400uS			409.2mS
 *	5	1			800uS			818.4mS
 *	6	1			1600uS			1637.0mS
 *	7	1			3200uS			3274.0mS
 *	8	0			0			0
 *	9	2			50uS			1.1mS
 *	A	2			100uS			2.2mS
 *	B	2			200uS			4.4mS
 *	C	2			400uS			8.8mS
 *	D	2			800uS			17.6mS
 *	E	2			1600uS			35.2mS
 *	F	2			3200uS			70.4mS
 */

/* pick up the mode index setting and its settle time from the above table */
#define OV5640_VCM_DACMODE 0x3602
#define OV5640_TRANSITION_MODE 0x0B
#define SETTLETIME_MS 5

#define POS_LOW (0)
#define POS_HIGH (1023)
#define FPOS_COUNT 1024
#define FOCAL_LENGTH (100)
#define FNUMBER (28)

#define SIZEOF_I2C_TRANSBUF 64

struct ov5640_info {
	int mode;
	struct miscdevice miscdev_info;
	struct i2c_client *i2c_client;
	struct clk *mclk;
	struct ov5640_platform_data *pdata;
	struct ov5640_config focuser;
	int af_fw_loaded;
	struct kobject *kobj;
	struct device *dev;
	u8 i2c_trans_buf[SIZEOF_I2C_TRANSBUF];
};

static int ov5640_read_reg(struct i2c_client *client, u16 addr, u8 *val)
{
	int err;
	struct i2c_msg msg[2];
	unsigned char data[3];

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
	msg[1].len = 1;
	msg[1].buf = data + 2;

	err = i2c_transfer(client->adapter, msg, 2);

	if (err != 2)
		return -EINVAL;

	*val = data[2];

	return 0;
}

#ifdef KERNEL_WARNING
static int ov5640_write_reg(struct i2c_client *client, u8 addr, u8 value)
{
	int count;
	struct i2c_msg msg[1];
	unsigned char data[4];

	if (!client->adapter)
		return -ENODEV;

	data[0] = addr;
	data[1] = (u8) (addr & 0xff);
	data[2] = value;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 3;
	msg[0].buf = data;

	count = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
	if (count == ARRAY_SIZE(msg))
		return 0;
	dev_err(&client->dev,
		"ov5840: i2c transfer failed, addr: %x, value: %02x\n",
	       addr, (u32)value);
	return -EIO;
}
#endif

static int ov5640_write_bulk_reg(struct i2c_client *client, u8 *data, int len)
{
	int err;
	struct i2c_msg msg;

	if (!client->adapter)
		return -ENODEV;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = len;
	msg.buf = data;

	err = i2c_transfer(client->adapter, &msg, 1);
	if (err == 1)
		return 0;

	dev_err(&client->dev, "ov5640: i2c transfer failed at %x\n",
		(int)data[0] << 8 | data[1]);

	return err;
}

static int ov5640_write_table(struct ov5640_info *info,
			      struct ov5640_reg table[],
			      struct ov5640_reg override_list[],
			      int num_override_regs)
{
	int err;
	struct ov5640_reg *next, *n_next;
	u8 *b_ptr = info->i2c_trans_buf;
	unsigned int buf_filled = 0;
	int i;
	u16 val;

	for (next = table; next->addr != OV5640_TABLE_END; next++) {
		if (next->addr == OV5640_TABLE_WAIT_MS) {
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

		if (!buf_filled) {
			b_ptr = info->i2c_trans_buf;
			*b_ptr++ = next->addr >> 8;
			*b_ptr++ = next->addr & 0xff;
			buf_filled = 2;
		}
		*b_ptr++ = val;
		buf_filled++;

		n_next = next + 1;
		if (n_next->addr != OV5640_TABLE_END &&
			n_next->addr != OV5640_TABLE_WAIT_MS &&
			buf_filled < SIZEOF_I2C_TRANSBUF &&
			n_next->addr == next->addr + 1) {
			continue;
		}

		err = ov5640_write_bulk_reg(info->i2c_client,
			info->i2c_trans_buf, buf_filled);
		if (err)
			return err;

		buf_filled = 0;
	}
	return 0;
}

static int ov5640_set_mode(struct ov5640_info *info, struct ov5640_mode *mode)
{
	int sensor_mode;
	int err;

	dev_info(info->dev, "%s: xres %u yres %u\n",
			__func__, mode->xres, mode->yres);
	if (!info->af_fw_loaded) {
		err = ov5640_write_table(info, tbl_af_firmware, NULL, 0);
		if (err)
			return err;
		info->af_fw_loaded = 1;
	}

	if (mode->xres == 2592 && mode->yres == 1944)
		sensor_mode = OV5640_MODE_2592x1944;
	else if (mode->xres == 1920 && mode->yres == 1080)
		sensor_mode = OV5640_MODE_1920x1080;
	else if (mode->xres == 1296 && mode->yres == 964)
		sensor_mode = OV5640_MODE_1296x972;
	else {
		dev_info(info->dev, "%s: invalid resolution: %d %d\n",
				__func__, mode->xres, mode->yres);
		return -EINVAL;
	}

	err = ov5640_write_table(info, mode_table[sensor_mode],
		NULL, 0);
	if (err)
		return err;

	info->mode = sensor_mode;
	return 0;
}

static int ov5640_set_af_mode(struct ov5640_info *info, u8 mode)
{
	dev_info(info->dev, "%s: mode %d\n", __func__, mode);
	if (mode == OV5640_AF_INIFINITY)
		return ov5640_write_table(info, tbl_release_focus, NULL, 0);

	if (mode == OV5640_AF_TRIGGER)
		return ov5640_write_table(info, tbl_single_focus, NULL, 0);

	return -EINVAL;
}

static int ov5640_get_af_status(struct ov5640_info *info, u8 *val)
{
	int err;

	err = ov5640_read_reg(info->i2c_client, 0x3023, val);
	if (err)
		return -EINVAL;

	dev_info(info->dev, "%s: value %p\n", __func__, val);
	return 0;
}

static void ov5640_mclk_disable(struct ov5640_info *info)
{
	dev_dbg(&info->i2c_client->dev, "%s: disable MCLK\n", __func__);
	clk_disable_unprepare(info->mclk);
}

static int ov5640_mclk_enable(struct ov5640_info *info)
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

static int ov5640_set_position(struct ov5640_info *info, u32 position)
{
	u8 data[4];

	if (position < info->focuser.pos_low ||
	    position > info->focuser.pos_high)
		return -EINVAL;

	data[0] = (OV5640_VCM_DACMODE >> 8) & 0xff;
	data[1] = OV5640_VCM_DACMODE & 0xff;
	data[2] = ((position & 0xf) << 4) | OV5640_TRANSITION_MODE;
	data[3] = (position * 0x3f0) >> 4;
	return ov5640_write_bulk_reg(info->i2c_client, data, 4);
}

static int ov5640_set_power(struct ov5640_info *info, u32 level)
{
	int err = 0;
	switch (level) {
	case OV5640_POWER_LEVEL_OFF:
	case OV5640_POWER_LEVEL_SUS:
		if (info->pdata && info->pdata->power_off)
			info->pdata->power_off(&info->i2c_client->dev);
		ov5640_mclk_disable(info);
		info->af_fw_loaded = 0;
		info->mode = 0;
		break;
	case OV5640_POWER_LEVEL_ON:
		if (info->pdata && info->pdata->power_on) {
			err = ov5640_mclk_enable(info);
			if (!err) {
				err = info->
					pdata->power_on(&info->i2c_client->dev);
				if (err < 0)
					ov5640_mclk_disable(info);
			}
		}
		break;
	default:
		dev_err(info->dev, "unknown power level %d.\n", level);
		return -EINVAL;
	}

	return err;
}

static int ov5640_set_wb(struct ov5640_info *info, u8 val)
{
	int err = 0;

	switch (val) {
	case OV5640_WB_AUTO:
		err = ov5640_write_table(info, wb_table[OV5640_WB_AUTO], NULL, 0);
		break;
	case OV5640_WB_INCANDESCENT:
		err = ov5640_write_table(info, wb_table[OV5640_WB_INCANDESCENT], NULL, 0);
		break;
	case OV5640_WB_DAYLIGHT:
		err = ov5640_write_table(info, wb_table[OV5640_WB_DAYLIGHT], NULL, 0);
		break;
	case OV5640_WB_FLUORESCENT:
		err = ov5640_write_table(info, wb_table[OV5640_WB_FLUORESCENT], NULL, 0);
		break;
	case OV5640_WB_CLOUDY:
		err = ov5640_write_table(info, wb_table[OV5640_WB_CLOUDY], NULL, 0);
		break;
	default:
		dev_err(info->dev, "this wb setting not supported!\n");
		return -EINVAL;
	}

	return err;
}

static long ov5640_ioctl(struct file *file,
			 unsigned int cmd, unsigned long arg)
{
	struct ov5640_info *info = file->private_data;

	switch (_IOC_NR(cmd)) {
	case _IOC_NR(OV5640_IOCTL_SET_SENSOR_MODE):
	{
		struct ov5640_mode mode;
		if (copy_from_user(&mode,
				   (const void __user *)arg,
				   sizeof(struct ov5640_mode))) {
			return -EFAULT;
		}

		return ov5640_set_mode(info, &mode);
	}
	case _IOC_NR(OV5640_IOCTL_GET_CONFIG):
	{
		if (copy_to_user((void __user *) arg,
				 &info->focuser,
				 sizeof(info->focuser))) {
			dev_err(info->dev, "%s: 0x%x\n", __func__, __LINE__);
			return -EFAULT;
		}

		break;
	}
	case _IOC_NR(OV5640_IOCTL_GET_AF_STATUS):
	{
		int err;
		u8 val;

		if (!info->af_fw_loaded) {
			dev_err(info->dev, "OV5640 AF fw not loaded!\n");
			break;
		}

		err = ov5640_get_af_status(info, &val);
		if (err)
			return err;

		if (copy_to_user((void __user *) arg,
				 &val, sizeof(val))) {
			dev_err(info->dev, "%s: 0x%x\n", __func__, __LINE__);
			return -EFAULT;
		}
		break;
	}
	case _IOC_NR(OV5640_IOCTL_SET_AF_MODE):
		if (!info->af_fw_loaded) {
			dev_err(info->dev, "OV5640 AF fw not loaded!\n");
			break;
		}
		return ov5640_set_af_mode(info, (u8)arg);
	case _IOC_NR(OV5640_IOCTL_POWER_LEVEL):
		return ov5640_set_power(info, (u32)arg);
	case _IOC_NR(OV5640_IOCTL_SET_FPOSITION):
		return ov5640_set_position(info, (u32)arg);
	case _IOC_NR(OV5640_IOCTL_GET_SENSOR_STATUS):
	{
		u8 status = 0;
		if (copy_to_user((void __user *)arg, &status,
				 1)) {
			dev_info(info->dev, "%s %d\n", __func__, __LINE__);
			return -EFAULT;
		}
		return 0;
	}
	case _IOC_NR(OV5640_IOCTL_SET_WB):
		return ov5640_set_wb(info, (u8)arg);
	default:
		return -EINVAL;
	}
	return 0;
}

static int ov5640_open(struct inode *inode, struct file *file)
{
	struct miscdevice *miscdev = file->private_data;
	struct ov5640_info *info;

	pr_info("%s\n", __func__);
	if (!miscdev) {
		pr_err("miscdev == NULL\n");
		return -1;
	}
	info = container_of(miscdev, struct ov5640_info, miscdev_info);
	file->private_data = info;

	return 0;
}

int ov5640_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	pr_info("%s\n", __func__);
	return 0;
}

static const struct file_operations ov5640_fileops = {
	.owner = THIS_MODULE,
	.open = ov5640_open,
	.unlocked_ioctl = ov5640_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = ov5640_ioctl,
#endif
	.release = ov5640_release,
};

static struct miscdevice ov5640_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ov5640",
	.fops = &ov5640_fileops,
};

static int ov5640_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct ov5640_info *info;
	int err;
	const char *mclk_name;

	dev_info(&client->dev, "ov5640: probing sensor.\n");

	info = devm_kzalloc(&client->dev,
			sizeof(struct ov5640_info), GFP_KERNEL);
	if (!info) {
		dev_err(&client->dev, "ov5640: Unable to allocate memory!\n");
		return -ENOMEM;
	}

	memcpy(&(info->miscdev_info),
		&ov5640_device,
		sizeof(struct miscdevice));

	err = misc_register(&(info->miscdev_info));
	if (err) {
		dev_err(&client->dev,
			"ov5640: Unable to register misc device!\n");
		devm_kfree(&client->dev, info);
		return err;
	}

	info->dev = &client->dev;
	info->pdata = client->dev.platform_data;
	info->i2c_client = client;
	info->focuser.settle_time = SETTLETIME_MS;
	info->focuser.focal_length = FOCAL_LENGTH;
	info->focuser.fnumber = FNUMBER;
	info->focuser.pos_low = POS_LOW;
	info->focuser.pos_high = POS_HIGH;

	i2c_set_clientdata(client, info);

	mclk_name = info->pdata->mclk_name ?
		    info->pdata->mclk_name : "default_mclk";
	info->mclk = devm_clk_get(&client->dev, mclk_name);
	if (IS_ERR(info->mclk)) {
		dev_err(&client->dev, "%s: unable to get clock %s\n",
			__func__, mclk_name);
		return PTR_ERR(info->mclk);
	}

	return 0;
}

static int ov5640_remove(struct i2c_client *client)
{
	struct ov5640_info *info;
	info = i2c_get_clientdata(client);
	misc_deregister(&(info->miscdev_info));
	return 0;
}

static const struct i2c_device_id ov5640_id[] = {
	{ "ov5640", 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, ov5640_id);

static struct i2c_driver ov5640_i2c_driver = {
	.driver = {
		.name = "ov5640",
		.owner = THIS_MODULE,
	},
	.probe = ov5640_probe,
	.remove = ov5640_remove,
	.id_table = ov5640_id,
};

module_i2c_driver(ov5640_i2c_driver);
