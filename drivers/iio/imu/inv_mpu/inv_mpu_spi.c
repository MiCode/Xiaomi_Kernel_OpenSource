/*
* Copyright (C) 2012-2018 InvenSense, Inc.
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/
#define pr_fmt(fmt) "inv_mpu: " fmt

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/jiffies.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kfifo.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>

#include "inv_mpu_iio.h"
#include "inv_mpu_dts.h"

#define INV_SPI_READ 0x80

static int inv_spi_single_write(struct inv_mpu_state *st, u8 reg, u8 data)
{
	struct spi_message msg;
	int res;
	u8 d[2];
	struct spi_transfer xfers = {
		.tx_buf = d,
		.bits_per_word = 8,
		.len = 2,
	};

	pr_debug("reg_write: reg=0x%x data=0x%x\n", reg, data);
	d[0] = reg;
	d[1] = data;
	spi_message_init(&msg);
	spi_message_add_tail(&xfers, &msg);
	res = spi_sync(to_spi_device(st->dev), &msg);

	return res;
}

static int inv_spi_read(struct inv_mpu_state *st, u8 reg, int len, u8 *data)
{
	struct spi_message msg;
	int res;
	u8 d[1];
	struct spi_transfer xfers[] = {
		{
		 .tx_buf = d,
		 .bits_per_word = 8,
		 .len = 1,
		 },
		{
		 .rx_buf = data,
		 .bits_per_word = 8,
		 .len = len,
		 }
	};

	if (!data)
		return -EINVAL;

	d[0] = (reg | INV_SPI_READ);

	spi_message_init(&msg);
	spi_message_add_tail(&xfers[0], &msg);
	spi_message_add_tail(&xfers[1], &msg);
	res = spi_sync(to_spi_device(st->dev), &msg);

	if (len ==1)
		pr_debug("reg_read: reg=0x%x length=%d data=0x%x\n",
							reg, len, data[0]);
	else
		pr_debug("reg_read: reg=0x%x length=%d d0=0x%x d1=0x%x\n",
					reg, len, data[0], data[1]);

	return res;

}

static int inv_spi_mem_write(struct inv_mpu_state *st, u8 mpu_addr, u16 mem_addr,
		     u32 len, u8 const *data)
{
	struct spi_message msg;
	u8 buf[258];
	int res;

	struct spi_transfer xfers = {
		.tx_buf = buf,
		.bits_per_word = 8,
		.len = len + 1,
	};

	if (!data || !st)
		return -EINVAL;

	if (len > (sizeof(buf) - 1))
		return -ENOMEM;

	inv_plat_single_write(st, REG_MEM_BANK_SEL, mem_addr >> 8);
	inv_plat_single_write(st, REG_MEM_START_ADDR, mem_addr & 0xFF);

	buf[0] = REG_MEM_R_W;
	memcpy(buf + 1, data, len);
	spi_message_init(&msg);
	spi_message_add_tail(&xfers, &msg);
	res = spi_sync(to_spi_device(st->dev), &msg);

	return res;
}

static int inv_spi_mem_read(struct inv_mpu_state *st, u8 mpu_addr, u16 mem_addr,
		    u32 len, u8 *data)
{
	int res;

	if (!data || !st)
		return -EINVAL;

	if (len > 256)
		return -EINVAL;

	res = inv_plat_single_write(st, REG_MEM_BANK_SEL, mem_addr >> 8);
	res = inv_plat_single_write(st, REG_MEM_START_ADDR, mem_addr & 0xFF);
	res = inv_plat_read(st, REG_MEM_R_W, len, data);

	return res;
}

/*
 *  inv_mpu_probe() - probe function.
 */
static int inv_mpu_probe(struct spi_device *spi)
{
	const struct spi_device_id *id = spi_get_device_id(spi);
	struct inv_mpu_state *st;
	struct iio_dev *indio_dev;
	int result;

#ifdef KERNEL_VERSION_4_X
	indio_dev = devm_iio_device_alloc(&spi->dev, sizeof(*st));
	if (indio_dev == NULL) {
		pr_err("memory allocation failed\n");
		result = -ENOMEM;
		goto out_no_free;
	}
#else
	indio_dev = iio_device_alloc(sizeof(*st));
	if (indio_dev == NULL) {
		pr_err("memory allocation failed\n");
		result = -ENOMEM;
		goto out_no_free;
	}
#endif
	st = iio_priv(indio_dev);
	st->write = inv_spi_single_write;
	st->read = inv_spi_read;
	st->mem_write = inv_spi_mem_write;
	st->mem_read = inv_spi_mem_read;
	st->dev = &spi->dev;
	st->irq = spi->irq;
#if !defined(CONFIG_INV_MPU_IIO_ICM20602) \
	&& !defined(CONFIG_INV_MPU_IIO_IAM20680)
	st->i2c_dis = BIT_I2C_IF_DIS;
#endif
	st->bus_type = BUS_IIO_SPI;
	spi_set_drvdata(spi, indio_dev);
	indio_dev->dev.parent = &spi->dev;
	indio_dev->name = id->name;

#ifdef CONFIG_OF
	result = invensense_mpu_parse_dt(st->dev, &st->plat_data);
	if (result)
#  ifdef KERNEL_VERSION_4_X
		return -ENODEV;
#  else
		goto out_free;
#  endif
	/* Power on device */
	if (st->plat_data.power_on) {
		result = st->plat_data.power_on(&st->plat_data);
		if (result < 0) {
			dev_err(st->dev, "power_on failed: %d\n", result);
#  ifdef KERNEL_VERSION_4_X
			return -ENODEV;
#  else
			goto out_free;
#  endif
		}
		pr_info("%s: power on here.\n", __func__);
	}
	pr_info("%s: power on.\n", __func__);

	msleep(100);
#else
	if (dev_get_platdata(st->dev) == NULL)
#  ifdef KERNEL_VERSION_4_X
		return -ENODEV;
#  else
		goto out_free;
#  endif
	st->plat_data = *(struct mpu_platform_data *)dev_get_platdata(st->dev);
#endif

	/* power is turned on inside check chip type */
	result = inv_check_chip_type(indio_dev, id->name);
	if (result)
#ifdef KERNEL_VERSION_4_X
		return -ENODEV;
#else
		goto out_free;
#endif

	result = inv_mpu_configure_ring(indio_dev);
	if (result) {
		pr_err("configure ring buffer fail\n");
		goto out_free;
	}
#ifdef KERNEL_VERSION_4_X
	result = devm_iio_device_register(st->dev, indio_dev);
	if (result) {
		pr_err("IIO device register fail\n");
		goto out_unreg_ring;
	}
#else
	result = iio_buffer_register(indio_dev, indio_dev->channels,
				     indio_dev->num_channels);
	if (result) {
		pr_err("ring buffer register fail\n");
		goto out_unreg_ring;
	}

	result = iio_device_register(indio_dev);
	if (result) {
		pr_err("IIO device register fail\n");
		goto out_remove_ring;
	}
#endif

	result = inv_create_dmp_sysfs(indio_dev);
	if (result) {
		pr_err("create dmp sysfs failed\n");
		goto out_unreg_iio;
	}
	init_waitqueue_head(&st->wait_queue);
	st->resume_state = true;
#ifdef CONFIG_HAS_WAKELOCK
	wake_lock_init(&st->wake_lock, WAKE_LOCK_SUSPEND, "inv_mpu");
#else
	wakeup_source_init(&st->wake_lock, "inv_mpu");
#endif
	dev_info(st->dev, "%s ma-kernel-%s is ready to go!\n",
	         indio_dev->name, INVENSENSE_DRIVER_VERSION);

#ifdef SENSOR_DATA_FROM_REGISTERS
	pr_info("Data read from registers\n");
#else
	pr_info("Data read from FIFO\n");
#endif
#ifdef TIMER_BASED_BATCHING
	pr_info("Timer based batching\n");
#endif

	return 0;
#ifdef KERNEL_VERSION_4_X
out_unreg_iio:
	devm_iio_device_unregister(st->dev, indio_dev);
out_unreg_ring:
	inv_mpu_unconfigure_ring(indio_dev);
out_free:
	devm_iio_device_free(st->dev, indio_dev);
out_no_free:
#else
out_unreg_iio:
	iio_device_unregister(indio_dev);
out_remove_ring:
	iio_buffer_unregister(indio_dev);
out_unreg_ring:
	inv_mpu_unconfigure_ring(indio_dev);
out_free:
	iio_device_free(indio_dev);
out_no_free:
#endif
	dev_err(st->dev, "%s failed %d\n", __func__, result);

	return -EIO;
}

static void inv_mpu_shutdown(struct spi_device *spi)
{
	struct iio_dev *indio_dev = spi_get_drvdata(spi);
	struct inv_mpu_state *st = iio_priv(indio_dev);
	int result;

	mutex_lock(&indio_dev->mlock);
	inv_switch_power_in_lp(st, true);
	dev_dbg(st->dev, "Shutting down %s...\n", st->hw->name);

	/* reset to make sure previous state are not there */
	result = inv_plat_single_write(st, REG_PWR_MGMT_1, BIT_H_RESET);
	if (result)
		dev_err(st->dev, "Failed to reset %s\n",
			st->hw->name);
	msleep(POWER_UP_TIME);
	/* turn off power to ensure gyro engine is off */
	result = inv_set_power(st, false);
	if (result)
		dev_err(st->dev, "Failed to turn off %s\n",
			st->hw->name);
	inv_switch_power_in_lp(st, false);
	mutex_unlock(&indio_dev->mlock);
}

/*
 *  inv_mpu_remove() - remove function.
 */
static int inv_mpu_remove(struct spi_device *spi)
{
	struct iio_dev *indio_dev = spi_get_drvdata(spi);
	struct inv_mpu_state *st = iio_priv(indio_dev);

#ifdef KERNEL_VERSION_4_X
	devm_iio_device_unregister(st->dev, indio_dev);
#else
	iio_device_unregister(indio_dev);
	iio_buffer_unregister(indio_dev);
#endif
	inv_mpu_unconfigure_ring(indio_dev);
#ifdef KERNEL_VERSION_4_X
	devm_iio_device_free(st->dev, indio_dev);
#else
	iio_device_free(indio_dev);
#endif
	dev_info(st->dev, "inv-mpu-iio module removed.\n");

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int inv_mpu_spi_suspend(struct device *dev)
{
	struct iio_dev *indio_dev = spi_get_drvdata(to_spi_device(dev));

	return inv_mpu_suspend(indio_dev);
}

static void inv_mpu_spi_complete(struct device *dev)
{
	struct iio_dev *indio_dev = spi_get_drvdata(to_spi_device(dev));

	inv_mpu_complete(indio_dev);
}
#endif

static const struct dev_pm_ops inv_mpu_spi_pmops = {
#ifdef CONFIG_PM_SLEEP
	.suspend = inv_mpu_spi_suspend,
	.complete = inv_mpu_spi_complete,
#endif
};

/* device id table is used to identify what device can be
 * supported by this driver
 */
static const struct spi_device_id inv_mpu_id[] = {
#ifdef CONFIG_INV_MPU_IIO_ICM20648
	{"icm20645", ICM20645},
	{"icm10340", ICM10340},
	{"icm20648", ICM20648},
#else
	{"icm20608d", ICM20608D},
	{"icm20690", ICM20690},
	{"icm20602", ICM20602},
	{"iam20680", IAM20680},
#endif
	{}
};

MODULE_DEVICE_TABLE(spi, inv_mpu_id);

static struct spi_driver inv_mpu_driver = {
	.probe = inv_mpu_probe,
	.remove = inv_mpu_remove,
	.shutdown = inv_mpu_shutdown,
	.id_table = inv_mpu_id,
	.driver = {
		.owner = THIS_MODULE,
		.name = "inv-mpu-iio-spi",
		.pm = &inv_mpu_spi_pmops,
	},
};
module_spi_driver(inv_mpu_driver);

MODULE_AUTHOR("Invensense Corporation");
MODULE_DESCRIPTION("Invensense SPI device driver");
MODULE_LICENSE("GPL");
