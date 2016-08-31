/*
 * AD5820 focuser driver.
 *
 * Copyright (c) 2011-2014, NVIDIA Corporation. All Rights Reserved.
 *
 * Contributors:
 *      Sachin Nikam <snikam@nvidia.com>
 *
 * Based on ov5650.c.
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/module.h>

#include <media/ad5820.h>

/* Focuser single step & full scale transition time truth table
 * in the format of:
 *   index	mode		single step transition	full scale transition
 *	0	0			0			0
 *	1	1			50uS			51.2mS
 *	2	1			100uS			102.3mS
 *	3	1			200uS			204.6mS
 *	4	1			400uS			409.2mS
 *	5	1			800uS			818.4mS
 *	6	1			1600uS			1636.8mS
 *	7	1			3200uS			3273.6mS
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
#define AD5820_TRANSITION_MODE 0x0B
#define SETTLETIME_MS 5

#define POS_LOW (0)
#define POS_HIGH (1023)
#define FOCAL_LENGTH (4.507f)
#define FNUMBER (2.8f)
#define FPOS_COUNT 1024

struct ad5820_info {
	struct i2c_client *i2c_client;
	struct regulator *regulator;
	struct ad5820_config config;
};

static int ad5820_write(struct i2c_client *client, u32 value)
{
	int count;
	struct i2c_msg msg[1];
	unsigned char data[2];

	if (!client->adapter)
		return -ENODEV;

	data[0] = (u8) ((value >> 4) & 0x3F);
	data[1] = (u8) ((value & 0xF) << 4) | AD5820_TRANSITION_MODE;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = ARRAY_SIZE(data);
	msg[0].buf = data;

	count = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
	if (count == ARRAY_SIZE(msg))
		return 0;

	return -EIO;
}

static int ad5820_set_position(struct ad5820_info *info, u32 position)
{
	if (position < info->config.pos_low ||
	    position > info->config.pos_high)
		return -EINVAL;

	return ad5820_write(info->i2c_client, position);
}

static long ad5820_ioctl(struct file *file,
			unsigned int cmd, unsigned long arg)
{
	struct ad5820_info *info = file->private_data;

	switch (_IOC_NR(cmd)) {
	case _IOC_NR(AD5820_IOCTL_GET_CONFIG):
	{
		if (copy_to_user((void __user *) arg,
				 &info->config,
				 sizeof(info->config))) {
			pr_err("%s: 0x%x\n", __func__, __LINE__);
			return -EFAULT;
		}

		break;
	}
	case _IOC_NR(AD5820_IOCTL_SET_POSITION):
		return ad5820_set_position(info, (u32) arg);
	default:
		return -EINVAL;
	}

	return 0;
}

struct ad5820_info *info;

static int ad5820_open(struct inode *inode, struct file *file)
{
	file->private_data = info;
	if (info->regulator)
		regulator_enable(info->regulator);
	return 0;
}

int ad5820_release(struct inode *inode, struct file *file)
{
	if (info->regulator)
		regulator_disable(info->regulator);
	file->private_data = NULL;
	return 0;
}


static const struct file_operations ad5820_fileops = {
	.owner = THIS_MODULE,
	.open = ad5820_open,
	.unlocked_ioctl = ad5820_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = ad5820_ioctl,
#endif
	.release = ad5820_release,
};

static struct miscdevice ad5820_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ad5820",
	.fops = &ad5820_fileops,
};

static int ad5820_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int err;

	pr_info("ad5820: probing sensor.\n");

	info = kzalloc(sizeof(struct ad5820_info), GFP_KERNEL);
	if (!info) {
		pr_err("ad5820: Unable to allocate memory!\n");
		return -ENOMEM;
	}

	err = misc_register(&ad5820_device);
	if (err) {
		pr_err("ad5820: Unable to register misc device!\n");
		kfree(info);
		return err;
	}

	info->regulator = regulator_get(&client->dev, "vdd_vcore_af");
	if (IS_ERR(info->regulator)) {
		dev_err(&client->dev, "unable to get regulator %s\n",
			dev_name(&client->dev));
		info->regulator = NULL;
	} else {
		regulator_enable(info->regulator);
	}

	info->i2c_client = client;
	info->config.settle_time = SETTLETIME_MS;
	info->config.focal_length = FOCAL_LENGTH;
	info->config.fnumber = FNUMBER;
	info->config.pos_low = POS_LOW;
	info->config.pos_high = POS_HIGH;
	i2c_set_clientdata(client, info);
	return 0;
}

static int ad5820_remove(struct i2c_client *client)
{
	struct ad5820_info *info;
	info = i2c_get_clientdata(client);
	misc_deregister(&ad5820_device);
	kfree(info);
	return 0;
}

static const struct i2c_device_id ad5820_id[] = {
	{ "ad5820", 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, ad5820_id);

static struct i2c_driver ad5820_i2c_driver = {
	.driver = {
		.name = "ad5820",
		.owner = THIS_MODULE,
	},
	.probe = ad5820_probe,
	.remove = ad5820_remove,
	.id_table = ad5820_id,
};

static int __init ad5820_init(void)
{
	pr_info("ad5820 sensor driver loading\n");
	return i2c_add_driver(&ad5820_i2c_driver);
}

static void __exit ad5820_exit(void)
{
	i2c_del_driver(&ad5820_i2c_driver);
}

module_init(ad5820_init);
module_exit(ad5820_exit);
MODULE_LICENSE("GPL v2");
