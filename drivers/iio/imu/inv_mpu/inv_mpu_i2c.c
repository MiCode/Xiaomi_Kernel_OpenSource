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
#include <linux/i2c.h>
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

#define CONFIG_DYNAMIC_DEBUG_I2C 0

/**
 *  inv_i2c_read_base() - Read one or more bytes from the device registers.
 *  @st:	Device driver instance.
 *  @i2c_addr:  i2c address of device.
 *  @reg:	First device register to be read from.
 *  @length:	Number of bytes to read.
 *  @data:	Data read from device.
 *  NOTE:This is not re-implementation of i2c_smbus_read because i2c
 *       address could be specified in this case. We could have two different
 *       i2c address due to secondary i2c interface.
 */
int inv_i2c_read_base(struct inv_mpu_state *st, u16 i2c_addr,
						u8 reg, u16 length, u8 *data)
{
	struct i2c_msg msgs[2];
	int res;

	if (!data)
		return -EINVAL;

	msgs[0].addr = i2c_addr;
	msgs[0].flags = 0;	/* write */
	msgs[0].buf = &reg;
	msgs[0].len = 1;

	msgs[1].addr = i2c_addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].buf = data;
	msgs[1].len = length;

	res = i2c_transfer(st->sl_handle, msgs, 2);

	if (res < 2) {
		if (res >= 0)
			res = -EIO;
	} else
		res = 0;
	INV_I2C_INC_MPUWRITE(3);
	INV_I2C_INC_MPUREAD(length);

	return res;
}

/**
 *  inv_i2c_single_write_base() - Write a byte to a device register.
 *  @st:	Device driver instance.
 *  @i2c_addr:  I2C address of the device.
 *  @reg:	Device register to be written to.
 *  @data:	Byte to write to device.
 *  NOTE:This is not re-implementation of i2c_smbus_write because i2c
 *       address could be specified in this case. We could have two different
 *       i2c address due to secondary i2c interface.
 */
int inv_i2c_single_write_base(struct inv_mpu_state *st,
						u16 i2c_addr, u8 reg, u8 data)
{
	u8 tmp[2];
	struct i2c_msg msg;
	int res;

	tmp[0] = reg;
	tmp[1] = data;

	msg.addr = i2c_addr;
	msg.flags = 0;		/* write */
	msg.buf = tmp;
	msg.len = 2;

	INV_I2C_INC_MPUWRITE(3);

	res = i2c_transfer(st->sl_handle, &msg, 1);
	if (res < 1) {
		if (res == 0)
			res = -EIO;
		return res;
	} else
		return 0;
}

static int inv_i2c_single_write(struct inv_mpu_state *st, u8 reg, u8 data)
{
	return inv_i2c_single_write_base(st, st->i2c_addr, reg, data);
}

static int inv_i2c_read(struct inv_mpu_state *st, u8 reg, int len, u8 *data)
{
	return inv_i2c_read_base(st, st->i2c_addr, reg, len, data);
}

static int _memory_write(struct inv_mpu_state *st, u8 mpu_addr, u16 mem_addr,
						u32 len, u8 const *data)
{
	u8 bank[2];
	u8 addr[2];
	u8 buf[513];

	struct i2c_msg msgs[3];
	int res;

	if (!data || !st)
		return -EINVAL;

	if (len >= (sizeof(buf) - 1))
		return -ENOMEM;

	bank[0] = REG_MEM_BANK_SEL;
	bank[1] = mem_addr >> 8;

	addr[0] = REG_MEM_START_ADDR;
	addr[1] = mem_addr & 0xFF;

	buf[0] = REG_MEM_R_W;
	memcpy(buf + 1, data, len);

	/* write message */
	msgs[0].addr = mpu_addr;
	msgs[0].flags = 0;
	msgs[0].buf = bank;
	msgs[0].len = sizeof(bank);

	msgs[1].addr = mpu_addr;
	msgs[1].flags = 0;
	msgs[1].buf = addr;
	msgs[1].len = sizeof(addr);

	msgs[2].addr = mpu_addr;
	msgs[2].flags = 0;
	msgs[2].buf = (u8 *) buf;
	msgs[2].len = len + 1;

	INV_I2C_INC_MPUWRITE(3 + 3 + (2 + len));

#if CONFIG_DYNAMIC_DEBUG_I2C
	{
		char *write = 0;
		pr_debug("%s WM%02X%02X%02X%s%s - %d\n", st->hw->name,
			mpu_addr, bank[1], addr[1],
			wr_pr_debug_begin(data, len, write),
			wr_pr_debug_end(write), len);
	}
#endif

	res = i2c_transfer(st->sl_handle, msgs, 3);
	if (res != 3) {
		if (res >= 0)
			res = -EIO;
		return res;
	} else {
		return 0;
	}
}

static int inv_i2c_mem_write(struct inv_mpu_state *st, u8 mpu_addr, u16 mem_addr,
						u32 len, u8 const *data)
{
	int r, i, j;
#define DMP_MEM_CMP_SIZE 16
	u8 w[DMP_MEM_CMP_SIZE];
	bool retry;

	j = 0;
	retry = true;
	while ((j < 3) && retry) {
		retry = false;
		r = _memory_write(st, mpu_addr, mem_addr, len, data);
		if (len < DMP_MEM_CMP_SIZE) {
			r = mem_r(mem_addr, len, w);
			for (i = 0; i < len; i++) {
				if (data[i] != w[i]) {
					pr_debug
				("error write=%x, len=%d,data=%x, w=%x, i=%d\n",
					mem_addr, len, data[i], w[i], i);
					retry = true;
				}
			}
		}
		j++;
	}

	return r;
}

static int inv_i2c_mem_read(struct inv_mpu_state *st, u8 mpu_addr, u16 mem_addr,
						u32 len, u8 *data)
{
	u8 bank[2];
	u8 addr[2];
	u8 buf;

	struct i2c_msg msgs[4];
	int res;

	if (!data || !st)
		return -EINVAL;

	bank[0] = REG_MEM_BANK_SEL;
	bank[1] = mem_addr >> 8;

	addr[0] = REG_MEM_START_ADDR;
	addr[1] = mem_addr & 0xFF;

	buf = REG_MEM_R_W;

	/* write message */
	msgs[0].addr = mpu_addr;
	msgs[0].flags = 0;
	msgs[0].buf = bank;
	msgs[0].len = sizeof(bank);

	msgs[1].addr = mpu_addr;
	msgs[1].flags = 0;
	msgs[1].buf = addr;
	msgs[1].len = sizeof(addr);

	msgs[2].addr = mpu_addr;
	msgs[2].flags = 0;
	msgs[2].buf = &buf;
	msgs[2].len = 1;

	msgs[3].addr = mpu_addr;
	msgs[3].flags = I2C_M_RD;
	msgs[3].buf = data;
	msgs[3].len = len;

	res = i2c_transfer(st->sl_handle, msgs, 4);
	if (res != 4) {
		if (res >= 0)
			res = -EIO;
	} else
		res = 0;
	INV_I2C_INC_MPUWRITE(3 + 3 + 3);
	INV_I2C_INC_MPUREAD(len);

#if CONFIG_DYNAMIC_DEBUG_I2C
	{
		char *read = 0;
		pr_debug("%s RM%02X%02X%02X%02X - %s%s\n", st->hw->name,
			mpu_addr, bank[1], addr[1], len,
			wr_pr_debug_begin(data, len, read),
			wr_pr_debug_end(read));
	}
#endif

	return res;
}

#ifdef CONFIG_ENABLE_IAM_ACC_GYRO_BUFFERING
static void inv_enable_acc_gyro(struct inv_mpu_state *st)
{
	struct iio_dev *indio_dev = iio_priv_to_dev(st);
	int accel_hz = 100;
	int gyro_hz = 100;

	/**Enable the ACCEL**/
	st->sensor_l[SENSOR_L_ACCEL].on = 0;
	st->trigger_state = RATE_TRIGGER;
	inv_check_sensor_on(st);
	set_inv_enable(indio_dev);

	inv_switch_power_in_lp(st, true);
	st->chip_config.accel_fs = ACCEL_FSR_2G;
	inv_set_accel_sf(st);
	st->trigger_state = MISC_TRIGGER;
	set_inv_enable(indio_dev);

	st->sensor_l[SENSOR_L_ACCEL].rate = accel_hz;
	st->trigger_state = DATA_TRIGGER;
	inv_check_sensor_on(st);
	set_inv_enable(indio_dev);

	st->sensor_l[SENSOR_L_ACCEL].on = 1;
	st->trigger_state = RATE_TRIGGER;
	inv_check_sensor_on(st);
	set_inv_enable(indio_dev);

	/**Enable the GYRO**/
	st->sensor_l[SENSOR_L_GYRO].on = 0;
	st->trigger_state = RATE_TRIGGER;
	inv_check_sensor_on(st);
	set_inv_enable(indio_dev);

	inv_switch_power_in_lp(st, true);
	st->chip_config.fsr = GYRO_FSR_250DPS;
	inv_set_gyro_sf(st);
	st->trigger_state = MISC_TRIGGER;
	set_inv_enable(indio_dev);

	st->sensor_l[SENSOR_L_GYRO].rate = gyro_hz;
	st->trigger_state = DATA_TRIGGER;
	inv_check_sensor_on(st);
	set_inv_enable(indio_dev);

	st->sensor_l[SENSOR_L_GYRO].on = 1;
	st->trigger_state = RATE_TRIGGER;
	inv_check_sensor_on(st);
	set_inv_enable(indio_dev);
}

static int inv_acc_gyro_early_buff_init(struct iio_dev *indio_dev)
{
	int i = 0, err = 0;
	struct inv_mpu_state *st;

	st = iio_priv(indio_dev);
	st->acc_bufsample_cnt = 0;
	st->gyro_bufsample_cnt = 0;
	st->report_evt_cnt = 5;
	st->max_buffer_time = 40;

	st->inv_acc_cachepool = kmem_cache_create("acc_sensor_sample",
			sizeof(struct inv_acc_sample),
			0,
			SLAB_HWCACHE_ALIGN, NULL);
	if (!st->inv_acc_cachepool) {
		pr_err("inv_acc_cachepool cache create failed\n");
		err = -ENOMEM;
		return 0;
	}

	for (i = 0; i < INV_ACC_MAXSAMPLE; i++) {
		st->inv_acc_samplist[i] =
			kmem_cache_alloc(st->inv_acc_cachepool,
					GFP_KERNEL);
		if (!st->inv_acc_samplist[i]) {
			err = -ENOMEM;
			goto clean_exit1;
		}
	}

	st->inv_gyro_cachepool = kmem_cache_create("gyro_sensor_sample"
			, sizeof(struct inv_gyro_sample), 0,
			SLAB_HWCACHE_ALIGN, NULL);
	if (!st->inv_gyro_cachepool) {
		pr_err("inv_gyro_cachepool cache create failed\n");
		err = -ENOMEM;
		goto clean_exit1;
	}

	for (i = 0; i < INV_GYRO_MAXSAMPLE; i++) {
		st->inv_gyro_samplist[i] =
			kmem_cache_alloc(st->inv_gyro_cachepool,
					GFP_KERNEL);
		if (!st->inv_gyro_samplist[i]) {
			err = -ENOMEM;
			goto clean_exit2;
		}
	}

	st->accbuf_dev = input_allocate_device();
	if (!st->accbuf_dev) {
		err = -ENOMEM;
		pr_err("input device allocation failed\n");
		goto clean_exit2;
	}
	st->accbuf_dev->name = "inv_accbuf";
	st->accbuf_dev->id.bustype = BUS_I2C;
	input_set_events_per_packet(st->accbuf_dev,
			st->report_evt_cnt * INV_ACC_MAXSAMPLE);
	set_bit(EV_ABS, st->accbuf_dev->evbit);
	input_set_abs_params(st->accbuf_dev, ABS_X,
			-G_MAX, G_MAX, 0, 0);
	input_set_abs_params(st->accbuf_dev, ABS_Y,
			-G_MAX, G_MAX, 0, 0);
	input_set_abs_params(st->accbuf_dev, ABS_Z,
			-G_MAX, G_MAX, 0, 0);
	input_set_abs_params(st->accbuf_dev, ABS_RX,
			-G_MAX, G_MAX, 0, 0);
	input_set_abs_params(st->accbuf_dev, ABS_RY,
			-G_MAX, G_MAX, 0, 0);
	err = input_register_device(st->accbuf_dev);
	if (err) {
		pr_err("unable to register input device %s\n",
				st->accbuf_dev->name);
		goto clean_exit3;
	}

	st->gyrobuf_dev = input_allocate_device();
	if (!st->gyrobuf_dev) {
		err = -ENOMEM;
		pr_err("input device allocation failed\n");
		goto clean_exit4;
	}
	st->gyrobuf_dev->name = "inv_gyrobuf";
	st->gyrobuf_dev->id.bustype = BUS_I2C;
	input_set_events_per_packet(st->gyrobuf_dev,
			st->report_evt_cnt * INV_GYRO_MAXSAMPLE);
	set_bit(EV_ABS, st->gyrobuf_dev->evbit);
	input_set_abs_params(st->gyrobuf_dev, ABS_X,
			-G_MAX, G_MAX, 0, 0);
	input_set_abs_params(st->gyrobuf_dev, ABS_Y,
			-G_MAX, G_MAX, 0, 0);
	input_set_abs_params(st->gyrobuf_dev, ABS_Z,
			-G_MAX, G_MAX, 0, 0);
	input_set_abs_params(st->gyrobuf_dev, ABS_RX,
			-G_MAX, G_MAX, 0, 0);
	input_set_abs_params(st->gyrobuf_dev, ABS_RY,
			-G_MAX, G_MAX, 0, 0);
	err = input_register_device(st->gyrobuf_dev);
	if (err) {
		pr_err("unable to register input device %s\n",
				st->gyrobuf_dev->name);
		goto clean_exit5;
	}

	st->acc_buffer_inv_samples = true;
	st->gyro_buffer_inv_samples = true;

	mutex_init(&st->acc_sensor_buff);
	mutex_init(&st->gyro_sensor_buff);

	inv_enable_acc_gyro(st);

	return 1;

clean_exit5:
	input_free_device(st->gyrobuf_dev);
clean_exit4:
	input_unregister_device(st->accbuf_dev);
clean_exit3:
	input_free_device(st->accbuf_dev);
clean_exit2:
	for (i = 0; i < INV_GYRO_MAXSAMPLE; i++)
		kmem_cache_free(st->inv_gyro_cachepool,
				st->inv_gyro_samplist[i]);
	kmem_cache_destroy(st->inv_gyro_cachepool);
clean_exit1:
	for (i = 0; i < INV_ACC_MAXSAMPLE; i++)
		kmem_cache_free(st->inv_acc_cachepool,
				st->inv_acc_samplist[i]);
	kmem_cache_destroy(st->inv_acc_cachepool);

	return 0;
}
static void inv_acc_gyro_input_cleanup(
		struct iio_dev *indio_dev)
{
	int i = 0;
	struct inv_mpu_state *st;

	st = iio_priv(indio_dev);
	input_free_device(st->accbuf_dev);
	input_unregister_device(st->gyrobuf_dev);
	input_free_device(st->gyrobuf_dev);
	for (i = 0; i < INV_GYRO_MAXSAMPLE; i++)
		kmem_cache_free(st->inv_gyro_cachepool,
				st->inv_gyro_samplist[i]);
	kmem_cache_destroy(st->inv_gyro_cachepool);
	for (i = 0; i < INV_ACC_MAXSAMPLE; i++)
		kmem_cache_free(st->inv_acc_cachepool,
				st->inv_acc_samplist[i]);
	kmem_cache_destroy(st->inv_acc_cachepool);
}
#else
static int inv_acc_gyro_early_buff_init(struct iio_dev *indio_dev)
{
	return 1;
}
static void inv_acc_gyro_input_cleanup(
		struct iio_dev *indio_dev)
{
}
#endif

/*
 *  inv_mpu_probe() - probe function.
 */
static int inv_mpu_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct inv_mpu_state *st;
	struct iio_dev *indio_dev;
	int result;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		result = -ENOSYS;
		pr_err("I2c function error\n");
		goto out_no_free;
	}

#ifdef KERNEL_VERSION_4_X
	indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*st));
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
	st->client = client;
	st->sl_handle = client->adapter;
	st->i2c_addr = client->addr;
	st->write = inv_i2c_single_write;
	st->read = inv_i2c_read;
	st->mem_write = inv_i2c_mem_write;
	st->mem_read = inv_i2c_mem_read;
	st->dev = &client->dev;
	st->bus_type = BUS_IIO_I2C;
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

	/* Make state variables available to all _show and _store functions. */
	i2c_set_clientdata(client, indio_dev);
	indio_dev->dev.parent = st->dev;
	indio_dev->name = id->name;

	st->irq = client->irq;

	result = inv_mpu_configure_ring(indio_dev);
	if (result) {
		pr_err("configure ring buffer fail\n");
		goto out_free;
	}
#ifdef KERNEL_VERSION_4_X
	INV_I2C_SETIRQ(IRQ_MPU, st->irq);
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
	INV_I2C_SETIRQ(IRQ_MPU, client->irq);
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
	result = inv_acc_gyro_early_buff_init(indio_dev);
	if (!result)
		return -EIO;

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
	dev_err(st->dev, "%s failed %d\n", __func__, result);
	iio_device_free(indio_dev);
out_no_free:
#endif

	return -EIO;
}

static void inv_mpu_shutdown(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
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
static int inv_mpu_remove(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct inv_mpu_state *st = iio_priv(indio_dev);

	inv_acc_gyro_input_cleanup(indio_dev);
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
static int inv_mpu_i2c_suspend(struct device *dev)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(to_i2c_client(dev));

	return inv_mpu_suspend(indio_dev);
}

static void inv_mpu_i2c_complete(struct device *dev)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(to_i2c_client(dev));

	inv_mpu_complete(indio_dev);
}
#endif

static const struct dev_pm_ops inv_mpu_i2c_pmops = {
#ifdef CONFIG_PM_SLEEP
	.suspend = inv_mpu_i2c_suspend,
	.complete = inv_mpu_i2c_complete,
#endif
};

/* device id table is used to identify what device can be
 * supported by this driver
 */
static const struct i2c_device_id inv_mpu_id[] = {
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

MODULE_DEVICE_TABLE(i2c, inv_mpu_id);

static struct i2c_driver inv_mpu_driver = {
	.probe = inv_mpu_probe,
	.remove = inv_mpu_remove,
	.shutdown = inv_mpu_shutdown,
	.id_table = inv_mpu_id,
	.driver = {
		.owner = THIS_MODULE,
		.name = "inv-mpu-iio-i2c",
		.pm = &inv_mpu_i2c_pmops,
	},
};
module_i2c_driver(inv_mpu_driver);

MODULE_AUTHOR("Invensense Corporation");
MODULE_DESCRIPTION("Invensense I2C device driver");
MODULE_LICENSE("GPL");
