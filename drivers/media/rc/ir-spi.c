 /*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 * Copyright (C) 2019 XiaoMi, Inc.
 * Author: Andi Shyti <andi.shyti@samsung.it>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * SPI driven IR LED device driver
 */

#include <linux/fs.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/spi/spi.h>
#include "media/lirc_dev.h"

#define IR_SPI_DRIVER_NAME		"ir-spi"

#define IR_SPI_DEFAULT_FREQUENCY	1920000
#define IR_SPI_BIT_PER_WORD		    32
#define IR_SPI_DATA_BUFFER		    150000

struct ir_spi_data {
	u16 nusers;
	int power_gpio;
	int buffer_size;

	u8 *buffer;

	struct lirc_driver lirc_driver;
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
#if 0
	ret = regulator_enable(idata->regulator);
	if (ret) {
		dev_err(&idata->spi->dev, "failed to power on the LED\n");
		goto out_free;
	}
#endif
	idata->xfer.tx_buf = idata->buffer;
	dev_warn(&idata->spi->dev, "xfer.len%d buffer_size %d\n",
		(int)idata->xfer.len, idata->buffer_size);
	ret = spi_sync_transfer(idata->spi, &idata->xfer, 1);
	if (ret)
		dev_err(&idata->spi->dev, "unable to deliver the signal\n");
#if 0
	regulator_disable(idata->regulator);
#endif
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
	struct ir_spi_data *idata = lirc_get_pdata(file);

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
	struct ir_spi_data *idata = lirc_get_pdata(file);

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
	case LIRC_GET_FEATURES:
		return put_user(idata->lirc_driver.features,
					(__u32 __user *) arg);

	case LIRC_GET_LENGTH:
		return put_user(idata->xfer.len, (__u32 __user *) arg);

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
		if (idata->xfer.len) {
			new = krealloc(idata->buffer, p, GFP_DMA);
			}
		else{
			if ((p > idata->buffer_size) || (idata->buffer == NULL)) {
				printk ("IR new malloc %d", (int)idata->xfer.len);
				if (idata->buffer != NULL) {
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

	case LIRC_SET_SEND_CARRIER:
		return put_user(idata->xfer.speed_hz, (__u32 __user *) arg);

	case LIRC_SET_REC_CARRIER:
		ret = get_user(p, (__u32 __user *) arg);
		if (ret)
			return ret;

		/*
		 * The frequency cannot be obviously set to '0',
		 * while, as in the case of the data length,
		 * multiple users should use the driver with the same
		 * frequency value, otherwise return EPERM
		 */
		if (!p || ((idata->nusers > 1) && p != idata->xfer.speed_hz))
			return -EPERM;

		mutex_lock(&idata->mutex);
		idata->xfer.speed_hz = p;
		mutex_unlock(&idata->mutex);
		return 0;
	}

	return -EINVAL;
}

static const struct file_operations ir_spi_fops = {
	.owner   = THIS_MODULE,
	.read    = lirc_dev_fop_read,
	.write   = ir_spi_chardev_write,
	.poll    = lirc_dev_fop_poll,
	.open    = ir_spi_chardev_open,
	.release = ir_spi_chardev_close,
	.llseek  = noop_llseek,
	.unlocked_ioctl = ir_spi_chardev_ioctl,
	.compat_ioctl   = ir_spi_chardev_ioctl,
};

static int ir_spi_probe(struct spi_device *spi)
{
	struct ir_spi_data *idata;
	u8 *buffer = NULL;
	idata = devm_kzalloc(&spi->dev, sizeof(*idata), GFP_KERNEL);
	if (!idata)
		return -ENOMEM;
#if 0
	idata->regulator = devm_regulator_get(&spi->dev, "irda_regulator");
	if (IS_ERR(idata->regulator))
		return PTR_ERR(idata->regulator);
#endif
	snprintf(idata->lirc_driver.name, sizeof(idata->lirc_driver.name),
							IR_SPI_DRIVER_NAME);
	idata->lirc_driver.features    = LIRC_CAN_SEND_RAW;
	idata->lirc_driver.code_length = 1;
	idata->lirc_driver.fops        = &ir_spi_fops;
	idata->lirc_driver.dev         = &spi->dev;
	idata->lirc_driver.data        = idata;
	idata->lirc_driver.owner       = THIS_MODULE;
	idata->lirc_driver.minor       = -1;

	idata->lirc_driver.minor = lirc_register_driver(&idata->lirc_driver);
	if (idata->lirc_driver.minor < 0) {
		dev_err(&spi->dev, "unable to generate character device\n");
		return idata->lirc_driver.minor;
	}

	mutex_init(&idata->mutex);

	idata->spi = spi;

	idata->xfer.bits_per_word = IR_SPI_BIT_PER_WORD;
	idata->xfer.speed_hz = IR_SPI_DEFAULT_FREQUENCY;
	buffer = kmalloc(IR_SPI_DATA_BUFFER, GFP_DMA);
	if (!buffer) {
		return -ENOMEM;
	}
	idata->buffer = buffer;
	idata->buffer_size = IR_SPI_DATA_BUFFER;
	return 0;
}

static int ir_spi_remove(struct spi_device *spi)
{
	struct ir_spi_data *idata = spi_get_drvdata(spi);
	if (idata->buffer != NULL) {
		kfree(idata->buffer);
		idata->buffer = NULL;
	}
	lirc_unregister_driver(idata->lirc_driver.minor);

	return 0;
}

static const struct of_device_id ir_spi_of_match[] = {
	{ .compatible = "ir-spi" },
	{},
};

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
