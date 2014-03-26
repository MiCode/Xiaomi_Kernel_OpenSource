/*
 * Copyright (C) 2010 MEMSIC, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/sysctl.h>
#include <linux/input-polldev.h>
#include <asm/uaccess.h>

//#include <linux/i2c/mmc3416x.h>
#include "mmc3416x.h"

#define DEBUG			0
#define MAX_FAILURE_COUNT	3
#define READMD			0

#define MMC3416X_DELAY_TM	10	/* ms */
#define MMC3416X_DELAY_SET	75	/* ms */
#define MMC3416X_DELAY_RESET     75     /* ms */

#define MMC3416X_RETRY_COUNT	3
#define MMC3416X_SET_INTV	250

#define MMC3416X_DEV_NAME	"mmc3416x"

static u32 read_idx = 0;
struct class *mag_class;

static struct i2c_client *this_client;

static struct input_polled_dev *ipdev;
static struct mutex lock;

static short ecompass_delay = 0;

static DEFINE_MUTEX(ecompass_lock);

static int mmc3xxx_i2c_rx_data(char *buf, int len)
{
	uint8_t i;
	struct i2c_msg msgs[] = {
		{
			.addr	= this_client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= buf,
		},
		{
			.addr	= this_client->addr,
			.flags	= I2C_M_RD,
			.len	= len,
			.buf	= buf,
		}
	};

	for (i = 0; i < MMC3416X_RETRY_COUNT; i++) {
		if (i2c_transfer(this_client->adapter, msgs, 2) >= 0) {
			break;
		}
		mdelay(10);
	}

	if (i >= MMC3416X_RETRY_COUNT) {
		pr_err("%s: retry over %d\n", __FUNCTION__, MMC3416X_RETRY_COUNT);
		return -EIO;
	}

	return 0;
}

static int mmc3xxx_i2c_tx_data(char *buf, int len)
{
	uint8_t i;
	struct i2c_msg msg[] = {
		{
			.addr	= this_client->addr,
			.flags	= 0,
			.len	= len,
			.buf	= buf,
		}
	};
	
	for (i = 0; i < MMC3416X_RETRY_COUNT; i++) {
		if (i2c_transfer(this_client->adapter, msg, 1) >= 0) {
			break;
		}
		mdelay(10);
	}

	if (i >= MMC3416X_RETRY_COUNT) {
		pr_err("%s: retry over %d\n", __FUNCTION__, MMC3416X_RETRY_COUNT);
		return -EIO;
	}
	return 0;
}

static int mmc3416x_open(struct inode *inode, struct file *file)
{
	return nonseekable_open(inode, file);
}

static int mmc3416x_release(struct inode *inode, struct file *file)
{
	return 0;
}

static long mmc3416x_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *pa = (void __user *)arg;
	int __user *pa_i = (void __user *)arg;
	unsigned char data[16] = {0};
	int vec[3] = {0};
	int reg;
	short flag;
	short delay;

	mutex_lock(&ecompass_lock);
	switch (cmd) {
	case MMC3416X_IOC_DIAG:
		if (get_user(reg, pa_i))
			return -EFAULT;
		data[0] = (unsigned char)((0xff)&reg);
		if (mmc3xxx_i2c_rx_data(data, 1) < 0) {
			return -EFAULT;
		}
		if (put_user(data[0], pa_i))
			return -EFAULT;
		break;
	case MMC3416X_IOC_TM:
		data[0] = MMC3416X_REG_CTRL;
		data[1] = MMC3416X_CTRL_TM;
		if (mmc3xxx_i2c_tx_data(data, 2) < 0) {
	                mutex_unlock(&ecompass_lock);
			return -EFAULT;
		}
		/* wait TM done for coming data read */
		msleep(MMC3416X_DELAY_TM);
		break;
	case MMC3416X_IOC_SET:
		data[0] = MMC3416X_REG_CTRL;
		data[1] = MMC3416X_CTRL_REFILL;
		if (mmc3xxx_i2c_tx_data(data, 2) < 0) {
	                mutex_unlock(&ecompass_lock);
			return -EFAULT;
		}
		msleep(MMC3416X_DELAY_SET);
		data[0] = MMC3416X_REG_CTRL;
		data[1] = MMC3416X_CTRL_SET;
		if (mmc3xxx_i2c_tx_data(data, 2) < 0) {
	                mutex_unlock(&ecompass_lock);
			return -EFAULT;
		}
		msleep(1);
		data[0] = MMC3416X_REG_CTRL;
		data[1] = 0;
		if (mmc3xxx_i2c_tx_data(data, 2) < 0) {
	                mutex_unlock(&ecompass_lock);
			return -EFAULT;
		}
		msleep(MMC3416X_DELAY_SET);
		break;
	case MMC3416X_IOC_RESET:
		data[0] = MMC3416X_REG_CTRL;
		data[1] = MMC3416X_CTRL_REFILL;
		if (mmc3xxx_i2c_tx_data(data, 2) < 0) {
	                mutex_unlock(&ecompass_lock);
			return -EFAULT;
		}
		msleep(MMC3416X_DELAY_RESET);
		data[0] = MMC3416X_REG_CTRL;
		data[1] = MMC3416X_CTRL_RESET;
		if (mmc3xxx_i2c_tx_data(data, 2) < 0) {
	                mutex_unlock(&ecompass_lock);
			return -EFAULT;
		}
		msleep(1);
		data[0] = MMC3416X_REG_CTRL;
		data[1] = 0;
		if (mmc3xxx_i2c_tx_data(data, 2) < 0) {
	                mutex_unlock(&ecompass_lock);
			return -EFAULT;
		}
		msleep(1);
		break;
	case MMC3416X_IOC_READ:
		data[0] = MMC3416X_REG_DATA;
		if (mmc3xxx_i2c_rx_data(data, 6) < 0) {
	                mutex_unlock(&ecompass_lock);
			return -EFAULT;
		}
		vec[0] = data[1] << 8 | data[0];
		vec[1] = data[3] << 8 | data[2];
		vec[2] = data[5] << 8 | data[4];
		vec[2] = 65536 - vec[2];	
	#if DEBUG
		printk("[X - %04x] [Y - %04x] [Z - %04x]\n", 
			vec[0], vec[1], vec[2]);
	#endif
		if (copy_to_user(pa, vec, sizeof(vec))) {
	                mutex_unlock(&ecompass_lock);
			return -EFAULT;
		}
		break;
	case MMC3416X_IOC_READXYZ:
		if (!(read_idx % MMC3416X_SET_INTV)) {
		    data[0] = MMC3416X_REG_CTRL;
		    data[1] = MMC3416X_CTRL_REFILL;
		    mmc3xxx_i2c_tx_data(data, 2);
		    msleep(MMC3416X_DELAY_RESET);
		    data[0] = MMC3416X_REG_CTRL;
		    data[1] = MMC3416X_CTRL_RESET;
		    mmc3xxx_i2c_tx_data(data, 2);
		    msleep(1);
		    data[0] = MMC3416X_REG_CTRL;
		    data[1] = 0;
		    mmc3xxx_i2c_tx_data(data, 2);
		    msleep(1);

	        data[0] = MMC3416X_REG_CTRL;
	        data[1] = MMC3416X_CTRL_REFILL;
	        mmc3xxx_i2c_tx_data(data, 2);
	        msleep(MMC3416X_DELAY_SET);
	        data[0] = MMC3416X_REG_CTRL;
	        data[1] = MMC3416X_CTRL_SET;
	        mmc3xxx_i2c_tx_data(data, 2);
	        msleep(1);
	        data[0] = MMC3416X_REG_CTRL;
	        data[1] = 0;
	        mmc3xxx_i2c_tx_data(data, 2);
	        msleep(1);
		}
		/* send TM cmd before read */
		data[0] = MMC3416X_REG_CTRL;
		data[1] = MMC3416X_CTRL_TM;
		/* not check return value here, assume it always OK */
		mmc3xxx_i2c_tx_data(data, 2);
		/* wait TM done for coming data read */
		msleep(MMC3416X_DELAY_TM);
#if READMD
		/* Read MD */
		data[0] = MMC3416X_REG_DS;
		if (mmc3xxx_i2c_rx_data(data, 1) < 0) {
	                mutex_unlock(&ecompass_lock);
			return -EFAULT;
		}
		while (!(data[0] & 0x01)) {
			msleep(1);
			/* Read MD again*/
			data[0] = MMC3416X_REG_DS;
			if (mmc3xxx_i2c_rx_data(data, 1) < 0) {
	                        mutex_unlock(&ecompass_lock);
				return -EFAULT;
                        }
			
			if (data[0] & 0x01) break;
			MD_times++;
			if (MD_times > 2) {
	                        mutex_unlock(&ecompass_lock);
		#if DEBUG
				printk("TM not work!!");
		#endif
				return -EFAULT;
			}
		}
#endif		
		/* read xyz raw data */
		read_idx++;
		data[0] = MMC3416X_REG_DATA;
		if (mmc3xxx_i2c_rx_data(data, 6) < 0) {
	                mutex_unlock(&ecompass_lock);
			return -EFAULT;
		}
		vec[0] = data[1] << 8 | data[0];
		vec[1] = data[3] << 8 | data[2];
		vec[2] = data[5] << 8 | data[4];
		vec[2] = 65536 - vec[2];	
	#if DEBUG
		printk("[X - %04x] [Y - %04x] [Z - %04x]\n", 
			vec[0], vec[1], vec[2]);
	#endif
		if (copy_to_user(pa, vec, sizeof(vec))) {
	                mutex_unlock(&ecompass_lock);
			return -EFAULT;
		}

		break;
	case MMC3416X_IOC_ID:
		data[0] = MMC3416X_REG_PRODUCTID_0;
		if (mmc3xxx_i2c_rx_data(data, 1) < 0) {
	                mutex_unlock(&ecompass_lock);
			return -EFAULT;
		}
                data[14] = data[0];
		data[0] = MMC3416X_REG_PRODUCTID_1;
		if (mmc3xxx_i2c_rx_data(data, 1) < 0) {
	                mutex_unlock(&ecompass_lock);
			return -EFAULT;
		}
                data[15] = data[0];
                flag = data[15] << 8 | data[14];
		if (copy_to_user(pa, &flag, sizeof(flag))) {
	                mutex_unlock(&ecompass_lock);
			return -EFAULT;
                }
                break;
	default:
		break;
	}
	mutex_unlock(&ecompass_lock);

	return 0;
}

static ssize_t mmc3416x_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;

	sprintf(buf, "MMC3416X");
	ret = strlen(buf) + 1;

	return ret;
}

static DEVICE_ATTR(mmc3416x, S_IRUGO, mmc3416x_show, NULL);

static struct file_operations mmc3416x_fops = {
	.owner		= THIS_MODULE,
	.open		= mmc3416x_open,
	.release	= mmc3416x_release,
	.unlocked_ioctl = mmc3416x_ioctl,
};

static struct miscdevice mmc3416x_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = MMC3416X_DEV_NAME,
	.fops = &mmc3416x_fops,
};

static void mmc3416x_poll(struct input_polled_dev *ipdev)
{
	unsigned char data[16] = {0, 0, 0, 0, 0, 0, 0, 0,
				  0, 0, 0, 0, 0, 0, 0, 0};
	int vec[3] = {0, 0, 0};
	static int first = 1;
	mutex_lock(&lock);
	if (!first) {
		/* read xyz raw data */
		read_idx++;
		data[0] = MMC3416X_REG_DATA;
		if (mmc3xxx_i2c_rx_data(data, 6) < 0) {
			mutex_unlock(&ecompass_lock);
			return;
		}
		vec[0] = data[1] << 8 | data[0];
		vec[1] = data[3] << 8 | data[2];
		vec[2] = data[5] << 8 | data[4];
		vec[2] = 65536 - vec[2];	
#if DEBUG
		printk("[X - %04x] [Y - %04x] [Z - %04x]\n", 
			vec[0], vec[1], vec[2]);
#endif
		input_report_abs(ipdev->input, ABS_X, vec[0]);
		input_report_abs(ipdev->input, ABS_Y, vec[1]);
		input_report_abs(ipdev->input, ABS_Z, vec[2]);

		input_sync(ipdev->input);
	} else {
		first = 0;
	}

	/* send TM cmd before read */
	data[0] = MMC3416X_REG_CTRL;
	data[1] = MMC3416X_CTRL_TM;
	/* not check return value here, assume it always OK */
	mmc3xxx_i2c_tx_data(data, 2);
	msleep(MMC3416X_DELAY_TM);
	mutex_unlock(&lock);
}


static struct input_polled_dev * mmc3416x_input_init(struct i2c_client *client)
{
	struct input_polled_dev *ipdev;
	int status;

	ipdev = input_allocate_polled_device();
	if (!ipdev) {
		dev_dbg(&client->dev, "error creating input device\n");
		return NULL;
	}
	ipdev->poll = mmc3416x_poll;
	ipdev->poll_interval = 20;       /* 50Hz */
	ipdev->private = client;
       
	ipdev->input->name = "Mmagnetometer";
	ipdev->input->phys = "mmc3416x/input0";
	ipdev->input->id.bustype = BUS_HOST;

	set_bit(EV_ABS, ipdev->input->evbit);

	input_set_abs_params(ipdev->input, ABS_X, -2047, 2047, 0, 0);
	input_set_abs_params(ipdev->input, ABS_Y, -2047, 2047, 0, 0);
	input_set_abs_params(ipdev->input, ABS_Z, -2047, 2047, 0, 0);

	input_set_capability(ipdev->input, EV_REL, REL_X);
	input_set_capability(ipdev->input, EV_REL, REL_Y);
	input_set_capability(ipdev->input, EV_REL, REL_Z);

	status = input_register_polled_device(ipdev);
	if (status) {
		dev_dbg(&client->dev,
			"error registering input device\n");
		input_free_polled_device(ipdev);
		return NULL;
	}
	return ipdev;
}


static int mmc3416x_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	unsigned char data[16] = {0};
	int res = 0;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s: functionality check failed\n", __FUNCTION__);
		res = -ENODEV;
		goto out;
	}
	this_client = client;

	res = misc_register(&mmc3416x_device);
	if (res) {
		pr_err("%s: mmc3416x_device register failed\n", __FUNCTION__);
		goto out;
	}
	res = device_create_file(&client->dev, &dev_attr_mmc3416x);
	if (res) {
		pr_err("%s: device_create_file failed\n", __FUNCTION__);
		goto out_deregister;
	}

	data[0] = MMC3416X_REG_CTRL;
	data[1] = MMC3416X_CTRL_REFILL;
	if (mmc3xxx_i2c_tx_data(data, 2) < 0) {
	}
	msleep(MMC3416X_DELAY_SET);

	data[0] = MMC3416X_REG_CTRL;
	data[1] = MMC3416X_CTRL_RESET;
	if (mmc3xxx_i2c_tx_data(data, 2) < 0) {
	}
	msleep(1);
	data[0] = MMC3416X_REG_CTRL;
	data[1] = 0;
	if (mmc3xxx_i2c_tx_data(data, 2) < 0) {
	}
	msleep(MMC3416X_DELAY_SET);

	data[0] = MMC3416X_REG_CTRL;
	data[1] = MMC3416X_CTRL_REFILL;
	if (mmc3xxx_i2c_tx_data(data, 2) < 0) {
	}
	msleep(MMC3416X_DELAY_RESET);
	data[0] = MMC3416X_REG_CTRL;
	data[1] = MMC3416X_CTRL_SET;
	if (mmc3xxx_i2c_tx_data(data, 2) < 0) {
	}
	msleep(1);
	data[0] = MMC3416X_REG_CTRL;
	data[1] = 0;
	if (mmc3xxx_i2c_tx_data(data, 2) < 0) {
	}
	msleep(1);

	data[0] = MMC3416X_REG_BITS;
	data[1] = MMC3416X_BITS_SLOW_16;
	if (mmc3xxx_i2c_tx_data(data, 2) < 0) {
	}
	msleep(MMC3416X_DELAY_TM);

	data[0] = MMC3416X_REG_CTRL;
	data[1] = MMC3416X_CTRL_TM;
	if (mmc3xxx_i2c_tx_data(data, 2) < 0) {
	}
	msleep(MMC3416X_DELAY_TM);

//	ipdev = mmc3416x_input_init(client);
	mutex_init(&lock);

	return 0;

out_deregister:
	misc_deregister(&mmc3416x_device);
out:
	return res;
}

static ssize_t mmc3416x_fs_read(struct device *dev, struct device_attribute *attr, char *buf)
{
	unsigned char data[6] = {0};
	int vec[3] = {0};
	int count;
	int res = 0;

	mutex_lock(&ecompass_lock);

        data[0] = MMC3416X_REG_CTRL;
        data[1] = MMC3416X_CTRL_TM;
        res = mmc3xxx_i2c_tx_data(data, 2);

        msleep(MMC3416X_DELAY_TM);

        data[0] = MMC3416X_REG_DATA;
	if (mmc3xxx_i2c_rx_data(data, 6) < 0) {
	    return 0;
	}
	vec[0] = data[1] << 8 | data[0];
	vec[1] = data[3] << 8 | data[2];
	vec[2] = data[5] << 8 | data[4];
	vec[2] = 65536 - vec[2];	
	count = sprintf(buf,"%d,%d,%d\n", vec[0], vec[1], vec[2]);
	mutex_unlock(&ecompass_lock);

	return count;
}

static ssize_t mmc3416x_fs_write(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned char data[16] = {0};

	data[0] = MMC3416X_REG_CTRL;
	data[1] = MMC3416X_CTRL_TM;
	if (mmc3xxx_i2c_tx_data(data, 2) < 0) {
	}
	msleep(MMC3416X_DELAY_TM);

	return size;
}

static int mmc3416x_remove(struct i2c_client *client)
{
	device_remove_file(&client->dev, &dev_attr_mmc3416x);
	misc_deregister(&mmc3416x_device);
	if (ipdev) input_unregister_polled_device(ipdev);

	return 0;
}

static DEVICE_ATTR(read_mag, S_IRUGO | S_IWUSR | S_IWGRP, mmc3416x_fs_read, mmc3416x_fs_write);

static const struct i2c_device_id mmc3416x_id[] = {
	{ MMC3416X_I2C_NAME, 0 },
	{ }
};

static struct i2c_driver mmc3416x_driver = {
	.probe 		= mmc3416x_probe,
	.remove 	= mmc3416x_remove,
	.id_table	= mmc3416x_id,
	.driver 	= {
		.owner	= THIS_MODULE,
		.name	= MMC3416X_I2C_NAME,
	},
};


static int __init mmc3416x_init(void)
{
	struct device *dev_t;

	mag_class = class_create(THIS_MODULE, "magnetic");

	if (IS_ERR(mag_class)) 
		return PTR_ERR( mag_class );

	dev_t = device_create( mag_class, NULL, 0, "%s", "magnetic");

	if (device_create_file(dev_t, &dev_attr_read_mag) < 0)
		printk("Failed to create device file(%s)!\n", dev_attr_read_mag.attr.name);

	if (IS_ERR(dev_t)) 
	{
		return PTR_ERR(dev_t);
	}
        printk("mmc3416x add driver\r\n");
	ipdev = NULL;
	return i2c_add_driver(&mmc3416x_driver);
}

static void __exit mmc3416x_exit(void)
{
        i2c_del_driver(&mmc3416x_driver);
}

module_init(mmc3416x_init);
module_exit(mmc3416x_exit);

MODULE_DESCRIPTION("MEMSIC MMC3416X Magnetic Sensor Driver");
MODULE_LICENSE("GPL");

