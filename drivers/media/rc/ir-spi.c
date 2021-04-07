 /*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 * Author: Andi Shyti <andi.shyti@samsung.it>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * SPI driven IR LED device driver
 */
#include <linux/init.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/compat.h>
#include <linux/delay.h>
#include <linux/kthread.h>

#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/types.h>

#ifdef CONFIG_OF
#include <linux/of_device.h>
#include <linux/of.h>
#endif
#include <linux/gpio.h>

#include <asm/delay.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/spi/spi.h>
#include <linux/miscdevice.h>
#include <uapi/linux/lirc.h>
#include <linux/uaccess.h>

#define IR_SPI_DRIVER_NAME		"ir-spi"

#define IR_SPI_DEFAULT_FREQUENCY	1920000
#define IR_SPI_BIT_PER_WORD		    32
#define IR_SPI_DATA_BUFFER		    150000

struct ir_spi_data *ir_spi_data_g;

struct ir_spi_data {
	u16 nusers;
	int power_gpio;
	int buffer_size;

	u8 *buffer;

	dev_t devt;
	struct spi_device *spi;
	struct spi_transfer xfer;
	struct mutex mutex;
	struct regulator *regulator;
};

static ssize_t ir_spi_chardev_write(struct file *file,
					const char __user *buffer,
					size_t length, loff_t *offset)
{
	struct ir_spi_data *idata = file->private_data;
	bool please_free = false;
	int ret = 0;

	if (idata->xfer.len && (idata->xfer.len != length))
		return -EINVAL;

	mutex_lock(&idata->mutex);

	if (!idata->xfer.len) {
		idata->buffer = kmalloc(length, GFP_DMA);

		if (!idata->buffer) {
			ret = -ENOMEM;
			goto out_unlock;
		}

		idata->xfer.len = length;
		please_free = true;
	}

	if (copy_from_user(idata->buffer, buffer, length)) {
		ret = -EFAULT;
		goto out_free;
	}
	idata->xfer.tx_buf = idata->buffer;
	dev_warn(&idata->spi->dev, "xfer.len%d buffer_size %d\n",(int)idata->xfer.len,idata->buffer_size);
	ret = spi_sync_transfer(idata->spi, &idata->xfer, 1);
	if (ret)
		dev_err(&idata->spi->dev, "unable to deliver the signal\n");
out_free:
	if (please_free) {
		kfree(idata->buffer);
		idata->xfer.len = 0;
		idata->buffer = NULL;
	}

out_unlock:
	mutex_unlock(&idata->mutex);

	return ret ? ret : length;
}

static int ir_spi_chardev_open(struct inode *inode, struct file *file)
{
	struct ir_spi_data *idata;
	idata = ir_spi_data_g;

	if (unlikely(idata->nusers >= SHRT_MAX)) {
		dev_err(&idata->spi->dev, "device busy\n");
		return -EBUSY;
	}

	file->private_data = idata;

	mutex_lock(&idata->mutex);
	idata->nusers++;
	mutex_unlock(&idata->mutex);

	return 0;
}

static int ir_spi_chardev_close(struct inode *inode, struct file *file)
{
	struct ir_spi_data *idata;
	idata = ir_spi_data_g;

	mutex_lock(&idata->mutex);
	idata->nusers--;

	/*
	 * check if someone else is using the driver,
	 * if not, then:
	 *
	 *  - reset length and frequency values to default
	 *  - shut down the LED
	 *  - free the buffer (NULL or ZERO_SIZE_PTR are noop)
	 */
	if (!idata->nusers) {
		idata->xfer.len = 0;
		idata->xfer.speed_hz = IR_SPI_DEFAULT_FREQUENCY;
	}

	mutex_unlock(&idata->mutex);

	return 0;
}

static long ir_spi_chardev_ioctl(struct file *file, unsigned int cmd,
						unsigned long arg)
{
	__u32 p;
	s32 ret;
	struct ir_spi_data *idata = file->private_data;

	switch (cmd) {
		case LIRC_SET_SEND_MODE: {
			void *new;

			ret = get_user(p, (__u32 __user *) arg);
			if (ret)
				return ret;

			/*
			 * the user is trying to set the same
			 * length of the current value
			 */
			if (idata->xfer.len == p)
				return 0;

			/*
			 * multiple users should use the driver with the
			 * length, otherwise return EPERM same data
			 */
			if (idata->nusers > 1)
				return -EPERM;

			/*
			 * if the buffer is already allocated, reallocate it with the
			 * desired value. If the desired value is 0, then the buffer is
			 * freed from krealloc()
			 */
			if (idata->xfer.len){
				new = krealloc(idata->buffer, p, GFP_DMA);
				}
			else{
				if ((p>idata->buffer_size) || (idata->buffer == NULL)){
					printk ("IR new malloc %d",(int)idata->xfer.len);
					if (idata->buffer != NULL)
					{
						kfree (idata->buffer);
						idata->buffer = NULL;
					}
					new = kmalloc(p, GFP_DMA);
					if (!new)
						return -ENOMEM;
					idata->buffer = new;
					idata->buffer_size = p;
				}
			}

			mutex_lock(&idata->mutex);
			idata->xfer.len = p;
			mutex_unlock(&idata->mutex);

			return 0;
		}
	}

	return -EINVAL;
}

static const struct file_operations ir_spi_fops = {
	.owner   = THIS_MODULE,
	.write   = ir_spi_chardev_write,
	.open    = ir_spi_chardev_open,
	.release = ir_spi_chardev_close,
	.llseek  = noop_llseek,
	.unlocked_ioctl = ir_spi_chardev_ioctl,
	.compat_ioctl   = ir_spi_chardev_ioctl,
};

static struct miscdevice ir_spi_dev_drv = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ir_spi",
	.fops = &ir_spi_fops,
	.nodename = "ir_spi",
	.mode = 0666,
};

static int ir_spi_probe(struct spi_device *spi)
{
	struct ir_spi_data *idata;
	u8 *buffer = NULL;
	idata = devm_kzalloc(&spi->dev, sizeof(*idata), GFP_KERNEL);
	if (!idata)
		return -ENOMEM;

	mutex_init(&idata->mutex);

	idata->spi = spi;
	spi_set_drvdata(spi, idata);
	ir_spi_data_g = idata;

	idata->xfer.bits_per_word = IR_SPI_BIT_PER_WORD;
	idata->xfer.speed_hz = IR_SPI_DEFAULT_FREQUENCY;
	buffer = kmalloc(IR_SPI_DATA_BUFFER, GFP_DMA);
	if (!buffer)
	{
		return -ENOMEM;
	}
	idata->buffer = buffer;
	idata->buffer_size = IR_SPI_DATA_BUFFER;
	misc_register(&ir_spi_dev_drv);
	return 0;
}

static int ir_spi_remove(struct spi_device *spi)
{
	struct ir_spi_data *idata = spi_get_drvdata(spi);
	if (idata->buffer != NULL){
		kfree(idata->buffer);
		idata->buffer = NULL;
	}
	misc_deregister(&ir_spi_dev_drv);

	return 0;
}

static const struct of_device_id ir_spi_of_match[] = {
	{ .compatible = "ir-spi" },
	{},
};
MODULE_DEVICE_TABLE(of, ir_spi_of_match);

static struct spi_driver ir_spi_driver = {
	.probe = ir_spi_probe,
	.remove = ir_spi_remove,
	.driver = {
		.name = IR_SPI_DRIVER_NAME,
		.of_match_table = ir_spi_of_match,
	},
};

module_spi_driver(ir_spi_driver);

MODULE_AUTHOR("Andi Shyti <andi.shyti@samsung.com>");
MODULE_DESCRIPTION("SPI IR LED");
MODULE_LICENSE("GPL v2");
