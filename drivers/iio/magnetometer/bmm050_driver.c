/*!
 * @section LICENSE
 * (C) Copyright 2011~2014 Bosch Sensortec GmbH All Rights Reserved
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 *
 * @filename    bmm050_driver.c
 * @date        "Thu Apr 24 10:40:36 2014 +0800"
 * @id          "6d0d027"
 * @version     v1.0
 *
 * @brief       BMM050 Linux IIO Driver
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/acpi.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#ifdef __KERNEL__
#include <linux/kernel.h>
#include <linux/unistd.h>
#include <linux/types.h>
#include <linux/string.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#endif

#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <asm/unaligned.h>
#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/kfifo_buf.h>
#include <linux/iio/trigger.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/iio/trigger_consumer.h>



#include "bmm050_iio.h"

/* sensor specific */
#define SENSOR_NAME "bmm050"

#define SENSOR_CHIP_ID_BMM (0x32)

#define BMM_REG_NAME(name) BMM050_##name
#define BMM_VAL_NAME(name) BMM050_##name
#define BMM_CALL_API(name) bmm050_##name

#define BMM_I2C_WRITE_DELAY_TIME 1

#define BMM_DEFAULT_REPETITION_XY BMM_VAL_NAME(REGULAR_REPXY)
#define BMM_DEFAULT_REPETITION_Z BMM_VAL_NAME(HIGHACCURACY_REPZ)
#define BMM_DEFAULT_ODR BMM_VAL_NAME(REGULAR_DR)
/* generic */
#define BMM_MAX_RETRY_I2C_XFER (100)
#define BMM_MAX_RETRY_WAKEUP (5)
#define BMM_MAX_RETRY_WAIT_DRDY (100)

#define BMM_DELAY_MIN (1)
#define BMM_DELAY_DEFAULT (200)

#define MAG_VALUE_MAX (32767)
#define MAG_VALUE_MIN (-32768)

#define BYTES_PER_LINE (16)

#define BMM_SELF_TEST 1
#define BMM_ADV_TEST 2

#define BMM_OP_MODE_UNKNOWN (-1)


#define BMM_SENSORS_12_BITS		12
#define BMM_SENSORS_13_BITS		13
#define BMM_SENSORS_14_BITS		14
#define BMM_SENSORS_15_BITS		15
#define BMM_SENSORS_16_BITS		16

#define BMM_DATA_SHIFT_RIGHT_2_BIT                2

/*! Bosch sensor unknown place*/
#define BOSCH_SENSOR_PLACE_UNKNOWN (-1)
/*! Bosch sensor remapping table size P0~P7*/
#define MAX_AXIS_REMAP_TAB_SZ 8

#ifdef CONFIG_BMM_USE_PLATFORM_DATA
struct bosch_sensor_specific {
	char *name;
	/* 0 to 7 */
	unsigned int place;
	int irq;
};
#endif
/*!
 * we use a typedef to hide the detail,
 * because this type might be changed
 */
struct bosch_sensor_axis_remap {
	/* src means which source will be mapped to target x, y, z axis */
	/* if an target OS axis is remapped from (-)x,
	 * src is 0, sign_* is (-)1 */
	/* if an target OS axis is remapped from (-)y,
	 * src is 1, sign_* is (-)1 */
	/* if an target OS axis is remapped from (-)z,
	 * src is 2, sign_* is (-)1 */
	int src_x:3;
	int src_y:3;
	int src_z:3;

	int sign_x:2;
	int sign_y:2;
	int sign_z:2;
};

struct bosch_sensor_data {
	union {
		int16_t v[3];
		struct {
			int16_t x;
			int16_t y;
			int16_t z;
		};
	};
};

static const u8 odr_map[] = {10, 2, 6, 8, 15, 20, 25, 30};
static const long op_mode_maps[] = {
	BMM_VAL_NAME(NORMAL_MODE),
	BMM_VAL_NAME(FORCED_MODE),
	BMM_VAL_NAME(SUSPEND_MODE),
	BMM_VAL_NAME(SLEEP_MODE)
};



#define BMM_MAG_CHANNELS_CONFIG(device_type, si, mod, \
							endian, bits, addr) \
	{ \
		.type = device_type, \
		.modified = 1, \
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW), \
		.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE) | \
						BIT(IIO_CHAN_INFO_SAMP_FREQ), \
		.scan_index = si, \
		.channel2 = mod, \
		.address = addr, \
		.scan_type = { \
			.sign = 's', \
			.realbits = bits, \
			.shift = 16 - bits, \
			.storagebits = 16, \
			.endianness = endian, \
		}, \
	}

#define BMM_BYTE_FOR_PER_AXIS_CHANNEL		2
#define BMM_BYTE_FOR_PER_RES_CHANNEL		2
#define VALUE_VALID                                                           1
#define VALUE_INVALID                                                       0

/*iio chan spec for BMM050 mag sensor*/
static const struct iio_chan_spec bmm050_raw_channels[] = {
	BMM_MAG_CHANNELS_CONFIG(IIO_MAGN, BMM_SCAN_MAG_X,
	IIO_MOD_X, IIO_LE, BMM_SENSORS_13_BITS, BMM050_DATAX_LSB),
	BMM_MAG_CHANNELS_CONFIG(IIO_MAGN, BMM_SCAN_MAG_Y,
	IIO_MOD_Y, IIO_LE, BMM_SENSORS_13_BITS, BMM050_DATAY_LSB),
	BMM_MAG_CHANNELS_CONFIG(IIO_MAGN, BMM_SCAN_MAG_Z,
	IIO_MOD_Z, IIO_LE, BMM_SENSORS_15_BITS, BMM050_DATAZ_LSB),
	IIO_CHAN_SOFT_TIMESTAMP(BMM_SCAN_TIMESTAMP),

};

static struct i2c_client *bmm_client;
/* i2c operation for API */
static void bmm_delay(u32 msec);
static char bmm_i2c_read(struct i2c_client *client, u8 reg_addr,
		u8 *data, u8 len);
static char bmm_i2c_write(struct i2c_client *client, u8 reg_addr,
		u8 *data, u8 len);

static void bmm_dump_reg(struct i2c_client *client);
static int bmm_wakeup(struct i2c_client *client);
static int bmm_check_chip_id(struct i2c_client *client);

static int bmm_pre_suspend(struct i2c_client *client);
static int bmm_post_resume(struct i2c_client *client);

#ifdef CONFIG_HAS_EARLYSUSPEND
static void bmm_early_suspend(struct early_suspend *handler);
static void bmm_late_resume(struct early_suspend *handler);
#endif

static int bmm_restore_hw_cfg(struct i2c_client *client);

static const struct bosch_sensor_axis_remap
bst_axis_remap_tab_dft[MAX_AXIS_REMAP_TAB_SZ] = {
	/* src_x src_y src_z  sign_x  sign_y  sign_z */
	{  0,    1,    2,     1,      1,      1 }, /* P0 */
	{  1,    0,    2,     1,     -1,      1 }, /* P1 */
	{  0,    1,    2,    -1,     -1,      1 }, /* P2 */
	{  1,    0,    2,    -1,      1,      1 }, /* P3 */

	{  0,    1,    2,    -1,      1,     -1 }, /* P4 */
	{  1,    0,    2,    -1,     -1,     -1 }, /* P5 */
	{  0,    1,    2,     1,     -1,     -1 }, /* P6 */
	{  1,    0,    2,     1,      1,     -1 }, /* P7 */
};

#ifdef CONFIG_BMM_USE_PLATFORM_DATA
static void bst_remap_sensor_data(struct bosch_sensor_data *data,
		const struct bosch_sensor_axis_remap *remap)
{
	struct bosch_sensor_data tmp;

	tmp.x = data->v[remap->src_x] * remap->sign_x;
	tmp.y = data->v[remap->src_y] * remap->sign_y;
	tmp.z = data->v[remap->src_z] * remap->sign_z;

	memcpy(data, &tmp, sizeof(*data));
}

static void bst_remap_sensor_data_dft_tab(struct bosch_sensor_data *data,
		int place)
{
	/* sensor with place 0 needs not to be remapped */
	if ((place <= 0) || (place >= MAX_AXIS_REMAP_TAB_SZ))
		return;

	bst_remap_sensor_data(data, &bst_axis_remap_tab_dft[place]);
}
#endif

static void bmm_remap_sensor_data(struct bmm050_mdata_s32 *val,
		struct bmm_client_data *client_data)
{
#ifdef CONFIG_BMM_USE_PLATFORM_DATA
	struct bosch_sensor_data bsd;

	if (NULL == client_data->bst_pd)
		return;

	bsd.x = val->datax;
	bsd.y = val->datay;
	bsd.z = val->dataz;

	bst_remap_sensor_data_dft_tab(&bsd,
			client_data->bst_pd->place);

	val->datax = bsd.x;
	val->datay = bsd.y;
	val->dataz = bsd.z;
#else
	(void)val;
	(void)client_data;
#endif
}

static int bmm_check_chip_id(struct i2c_client *client)
{
	int err = 0;
	u8 chip_id = 0;

	bmm_i2c_read(client, BMM_REG_NAME(CHIP_ID), &chip_id, 1);
	dev_info(&client->dev, "read chip id result: %#x", chip_id);

	if ((chip_id & 0xff) != SENSOR_CHIP_ID_BMM)
		err = -1;

	return err;
}

static void bmm_delay(u32 msec)
{
	mdelay(msec);
}

static inline int bmm_get_forced_drdy_time(int rept_xy, int rept_z)
{
	return  (145 * rept_xy + 500 * rept_z + 980 + (1000 - 1)) / 1000;
}


static void bmm_dump_reg(struct i2c_client *client)
{
#ifdef DEBUG
	int i;
	u8 dbg_buf[64];
	u8 dbg_buf_str[64 * 3 + 1] = "";

	for (i = 0; i < BYTES_PER_LINE; i++) {
		dbg_buf[i] = i;
		sprintf(dbg_buf_str + i * 3, "%02x%c",
				dbg_buf[i],
				(((i + 1) % BYTES_PER_LINE == 0) ? '\n' : ' '));
	}

	dev_dbg(&client->dev, "%s\n", dbg_buf_str);

	bmm_i2c_read(client, BMM_REG_NAME(CHIP_ID), dbg_buf, 64);
	for (i = 0; i < 64; i++) {
		sprintf(dbg_buf_str + i * 3, "%02x%c",
				dbg_buf[i],
				(((i + 1) % BYTES_PER_LINE == 0) ? '\n' : ' '));
	}
	dev_dbg(&client->dev, "%s\n", dbg_buf_str);
#endif
}

static int bmm_wakeup(struct i2c_client *client)
{
	int err = 0;
	int try_times = BMM_MAX_RETRY_WAKEUP;
	const u8 value = 0x01;
	u8 dummy;

	dev_dbg(&client->dev, "waking up the chip...");

	while (try_times) {
		err = bmm_i2c_write(client,
				BMM_REG_NAME(POWER_CNTL), (u8 *)&value, 1);
		mdelay(BMM_I2C_WRITE_DELAY_TIME);
		dummy = 0;
		err = bmm_i2c_read(client, BMM_REG_NAME(POWER_CNTL), &dummy, 1);
		if (value == dummy)
			break;

		try_times--;
	}

	dev_info(&client->dev, "wake up result: %s, tried times: %d",
			(try_times > 0) ? "succeed" : "fail",
			BMM_MAX_RETRY_WAKEUP - try_times + 1);

	err = (try_times > 0) ? 0 : -1;

	return err;
}

/*i2c read routine for API*/
static char bmm_i2c_read(struct i2c_client *client, u8 reg_addr,
		u8 *data, u8 len)
{
#if !defined BMM_USE_BASIC_I2C_FUNC
	s32 dummy;
	if (NULL == client)
		return -1;

	while (0 != len--) {
#ifdef BMM_SMBUS
		dummy = i2c_smbus_read_byte_data(client, reg_addr);
		if (dummy < 0) {
			dev_err(&client->dev, "i2c bus read error");
			return -1;
		}
		*data = (u8)(dummy & 0xff);
#else
		dummy = i2c_master_send(client, (char *)&reg_addr, 1);
		if (dummy < 0)
			return -1;

		dummy = i2c_master_recv(client, (char *)data, 1);
		if (dummy < 0)
			return -1;
#endif
		reg_addr++;
		data++;
	}
	return 0;
#else
	int retry;

	struct i2c_msg msg[] = {
		{
		 .addr = client->addr,
		 .flags = 0,
		 .len = 1,
		 .buf = &reg_addr,
		},

		{
		 .addr = client->addr,
		 .flags = I2C_M_RD,
		 .len = len,
		 .buf = data,
		 },
	};

	for (retry = 0; retry < BMM_MAX_RETRY_I2C_XFER; retry++) {
		if (i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg)) > 0)
			break;
		else
			mdelay(BMM_I2C_WRITE_DELAY_TIME);
	}

	if (BMM_MAX_RETRY_I2C_XFER <= retry) {
		dev_err(&client->dev, "I2C xfer error");
		return -EIO;
	}

	return 0;
#endif
}

/*i2c write routine for */
static char bmm_i2c_write(struct i2c_client *client, u8 reg_addr,
		u8 *data, u8 len)
{
#if !defined BMM_USE_BASIC_I2C_FUNC
	s32 dummy;

#ifndef BMM_SMBUS
	u8 buffer[2];
#endif

	if (NULL == client)
		return -1;

	while (0 != len--) {
#ifdef BMM_SMBUS
		dummy = i2c_smbus_write_byte_data(client, reg_addr, *data);
#else
		buffer[0] = reg_addr;
		buffer[1] = *data;
		dummy = i2c_master_send(client, (char *)buffer, 2);
#endif
		reg_addr++;
		data++;
		if (dummy < 0) {
			dev_err(&client->dev, "error writing i2c bus");
			return -1;
		}

	}
	return 0;
#else
	u8 buffer[2];
	int retry;
	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 2,
			.buf = buffer,
		},
	};

	while (0 != len--) {
		buffer[0] = reg_addr;
		buffer[1] = *data;
		for (retry = 0; retry < BMM_MAX_RETRY_I2C_XFER; retry++) {
			if (i2c_transfer(client->adapter, msg,
						ARRAY_SIZE(msg)) > 0) {
				break;
			} else {
				mdelay(BMM_I2C_WRITE_DELAY_TIME);
			}
		}
		if (BMM_MAX_RETRY_I2C_XFER <= retry) {
			dev_err(&client->dev, "I2C xfer error");
			return -EIO;
		}
		reg_addr++;
		data++;
	}

	return 0;
#endif
}

static char bmm_i2c_read_wrapper(u8 dev_addr, u8 reg_addr, u8 *data, u8 len)
{
	char err = 0;
	err = bmm_i2c_read(bmm_client, reg_addr, data, len);
	return err;
}

static char bmm_i2c_write_wrapper(u8 dev_addr, u8 reg_addr, u8 *data, u8 len)
{
	char err = 0;
	err = bmm_i2c_write(bmm_client, reg_addr, data, len);
	return err;
}

/* this function exists for optimization of speed,
 * because it is frequently called */
static inline int bmm_set_forced_mode(struct i2c_client *client)
{
	int err = 0;

	/* FORCED_MODE */
	const u8 value = 0x02;
	err = bmm_i2c_write(client, BMM_REG_NAME(CONTROL), (u8 *)&value, 1);

	return err;
}

#ifdef BMM055_USE_INPUT_DEVICE
static void bmm_work_func(struct work_struct *work)
{
	struct bmm_client_data *client_data =
		container_of((struct delayed_work *)work,
			struct bmm_client_data, work);
	struct i2c_client *client = client_data->client;
	unsigned long delay =
		msecs_to_jiffies(atomic_read(&client_data->delay));

	mutex_lock(&client_data->mutex_value);

	mutex_lock(&client_data->mutex_op_mode);
	if (BMM_VAL_NAME(NORMAL_MODE) != client_data->op_mode)
		bmm_set_forced_mode(client);

	mutex_unlock(&client_data->mutex_op_mode);

	BMM_CALL_API(read_mdataXYZ_s32)(&client_data->value);
	bmm_remap_sensor_data(&client_data->value, client_data);

	input_report_abs(client_data->input, ABS_X, client_data->value.datax);
	input_report_abs(client_data->input, ABS_Y, client_data->value.datay);
	input_report_abs(client_data->input, ABS_Z, client_data->value.dataz);
	mutex_unlock(&client_data->mutex_value);

	input_sync(client_data->input);

	schedule_delayed_work(&client_data->work, delay);
}
#endif

static int bmm_set_odr(u8 odr)
{
	int err = 0;

	err = BMM_CALL_API(set_datarate)(odr);
	mdelay(BMM_I2C_WRITE_DELAY_TIME);

	return err;
}

static int bmm_get_odr(u8 *podr)
{
	int err = 0;
	u8 value;

	err = BMM_CALL_API(get_datarate)(&value);
	if (!err)
		*podr = value;

	return err;
}

static ssize_t bmm_show_chip_id(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", SENSOR_CHIP_ID_BMM);
}

static ssize_t bmm_show_op_mode(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);

	u8 op_mode = 0xff;
	u8 power_mode;

	mutex_lock(&indio_dev->mlock);
	BMM_CALL_API(get_powermode)(&power_mode);
	if (power_mode)
		BMM_CALL_API(get_functional_state)(&op_mode);
	else
		op_mode = BMM_VAL_NAME(SUSPEND_MODE);

	mutex_unlock(&indio_dev->mlock);

	dev_dbg(dev, "op_mode: %d", op_mode);

	ret = sprintf(buf, "%d\n", op_mode);

	return ret;
}


static inline int bmm_get_op_mode_idx(u8 op_mode)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(op_mode_maps); i++) {
		if (op_mode_maps[i] == op_mode)
			break;
	}

	if (i < ARRAY_SIZE(op_mode_maps))
		return i;
	else
		return -1;
}


static int bmm_set_op_mode(struct bmm_client_data *client_data, int op_mode)
{
	int err = 0;

	err = BMM_CALL_API(set_functional_state)(
			op_mode);

	if (BMM_VAL_NAME(SUSPEND_MODE) == op_mode)
		atomic_set(&client_data->in_suspend, 1);
	else
		atomic_set(&client_data->in_suspend, 0);

	return err;
}

static ssize_t bmm_store_op_mode(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err = 0;
	int i;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmm_client_data *client_data = iio_priv(indio_dev);
	long op_mode;

	err = kstrtoul(buf, 10, &op_mode);
	if (err)
		return err;


	i = bmm_get_op_mode_idx(op_mode);

	if (i != -1) {
		mutex_lock(&indio_dev->mlock);
		if (op_mode != client_data->op_mode) {
			if (BMM_VAL_NAME(FORCED_MODE) == op_mode) {
				/* special treat of forced mode
				 * for optimization */
				err = bmm_set_forced_mode(client_data->client);
			} else {
				err = bmm_set_op_mode(client_data, op_mode);
			}

			if (!err) {
				if (BMM_VAL_NAME(FORCED_MODE) == op_mode)
					client_data->op_mode =
						BMM_OP_MODE_UNKNOWN;
				else
					client_data->op_mode = op_mode;
			}
		}
		mutex_unlock(&indio_dev->mlock);
	} else {
		err = -EINVAL;
	}


	if (err)
		return err;
	else
		return count;
}

static ssize_t bmm_show_odr(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char data = 0;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmm_client_data *client_data = iio_priv(indio_dev);

	int err;
	u8 power_mode;

	mutex_lock(&client_data->mutex_power_mode);
	BMM_CALL_API(get_powermode)(&power_mode);
	if (power_mode) {
		mutex_lock(&indio_dev->mlock);
		err = bmm_get_odr(&data);
		mutex_unlock(&indio_dev->mlock);
	} else {
		err = -EIO;
	}
	mutex_unlock(&client_data->mutex_power_mode);

	if (!err) {
		if (data < ARRAY_SIZE(odr_map))
			err = sprintf(buf, "%d\n", odr_map[data]);
		else
			err = -EINVAL;
	}

	return err;
}

static int bmm_get_odr_raw(struct iio_dev *indio_dev, int *val)
{
	unsigned char data = 0;
	int err;
	u8 power_mode;
	struct bmm_client_data *client_data = iio_priv(indio_dev);

	mutex_lock(&client_data->mutex_power_mode);
	BMM_CALL_API(get_powermode)(&power_mode);
	if (power_mode) {
		mutex_lock(&indio_dev->mlock);
		err = bmm_get_odr(&data);
		mutex_unlock(&indio_dev->mlock);
	} else
		err = -EIO;
	mutex_unlock(&client_data->mutex_power_mode);
	printk(KERN_ERR "ODR %x\n", data);
	if (!err) {
		if (data < ARRAY_SIZE(odr_map)) {
			*val = odr_map[data];
			return IIO_VAL_INT;
		} else
			return -EINVAL;
	}

	return err;
}

static int bmm_set_odr_raw(struct iio_dev *indio_dev, int val)
{
	int err = 0;
	struct bmm_client_data *client_data = iio_priv(indio_dev);
	int i;

	for (i = 0; i < ARRAY_SIZE(odr_map); i++) {
		if (odr_map[i] == val)
			break;
	}
	if (i < ARRAY_SIZE(odr_map)) {
		mutex_lock(&indio_dev->mlock);
		err = bmm_set_odr(i);
		if (!err)
			client_data->odr = i;
		mutex_unlock(&indio_dev->mlock);
	}

	return err;
}

static ssize_t bmm_store_odr(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long tmp;
	unsigned char data;
	int err;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmm_client_data *client_data = iio_priv(indio_dev);

	u8 power_mode;
	int i;

	err = kstrtoul(buf, 10, &tmp);
	if (err)
		return err;

	if (tmp > 255)
		return -EINVAL;

	data = (unsigned char)tmp;

	mutex_lock(&client_data->mutex_power_mode);
	BMM_CALL_API(get_powermode)(&power_mode);
	if (power_mode) {
		for (i = 0; i < ARRAY_SIZE(odr_map); i++) {
			if (odr_map[i] == data)
				break;
		}

		if (i < ARRAY_SIZE(odr_map)) {
			mutex_lock(&indio_dev->mlock);
			err = bmm_set_odr(i);
			if (!err)
				client_data->odr = i;

			mutex_unlock(&indio_dev->mlock);
		} else {
			err = -EINVAL;
		}
	} else {
		err = -EIO;
	}

	mutex_unlock(&client_data->mutex_power_mode);
	if (err)
		return err;

	return count;
}

static ssize_t bmm_show_value(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmm_client_data *client_data = iio_priv(indio_dev);
	int count;
	struct bmm050_mdata_s32 value = {0, 0, 0, 0, 0};

	BMM_CALL_API(read_mdataXYZ_s32)(&value);
	if (value.drdy) {
		bmm_remap_sensor_data(&value, client_data);
		client_data->value = value;
	} else
		dev_err(dev, "data not ready");

	count = sprintf(buf, "%d %d %d\n",
			client_data->value.datax,
			client_data->value.datay,
			client_data->value.dataz);

	return count;
}


static ssize_t bmm_show_value_raw(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bmm050_mdata value;
	int count;

	BMM_CALL_API(get_raw_xyz)(&value);

	count = sprintf(buf, "%hd %hd %hd\n",
			value.datax,
			value.datay,
			value.dataz);

	return count;
}


static ssize_t bmm_show_enable(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmm_client_data *client_data = iio_priv(indio_dev);

	int err;

	mutex_lock(&client_data->mutex_enable);
	err = sprintf(buf, "%d\n", client_data->enable);
	mutex_unlock(&client_data->mutex_enable);
	return err;
}

static ssize_t bmm_store_enable(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmm_client_data *client_data = iio_priv(indio_dev);


	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	data = data ? 1 : 0;
	mutex_lock(&client_data->mutex_enable);
	if (data != client_data->enable) {
		if (data) {
			schedule_delayed_work(
					&client_data->work,
					msecs_to_jiffies(atomic_read(
							&client_data->delay)));
		} else {
			cancel_delayed_work_sync(&client_data->work);
		}

		client_data->enable = data;
	}
	mutex_unlock(&client_data->mutex_enable);

	return count;
}

static ssize_t bmm_show_delay(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmm_client_data *client_data = iio_priv(indio_dev);

	return sprintf(buf, "%d\n", atomic_read(&client_data->delay));

}

static ssize_t bmm_store_delay(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmm_client_data *client_data = iio_priv(indio_dev);

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	if (data < BMM_DELAY_MIN)
		data = BMM_DELAY_MIN;

	atomic_set(&client_data->delay, data);

	return count;
}

static ssize_t bmm_show_test(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmm_client_data *client_data = iio_priv(indio_dev);

	int err;

	err = sprintf(buf, "%d\n", client_data->result_test);
	return err;
}

static ssize_t bmm_store_test(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long data;
	int err;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmm_client_data *client_data = iio_priv(indio_dev);

	u8 dummy;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	/* the following code assumes the work thread is not running */
	if (BMM_SELF_TEST == data) {
		/* self test */
		err = bmm_set_op_mode(client_data, BMM_VAL_NAME(SLEEP_MODE));
		mdelay(3);
		err = BMM_CALL_API(set_selftest)(1);
		mdelay(3);
		err = BMM_CALL_API(get_self_test_XYZ)(&dummy);
		client_data->result_test = dummy;
	} else if (BMM_ADV_TEST == data) {
		/* advanced self test */
		err = BMM_CALL_API(perform_advanced_selftest)(
				&client_data->result_test);
	} else {
		err = -EINVAL;
	}

	if (!err) {
		BMM_CALL_API(soft_reset)();
		mdelay(BMM_I2C_WRITE_DELAY_TIME);
		bmm_restore_hw_cfg(client_data->client);
	}

	if (err)
		count = -1;

	return count;
}

static ssize_t bmm_show_place(struct device *dev,
		struct device_attribute *attr, char *buf)
{
#ifdef CONFIG_BMM_USE_PLATFORM_DATA
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmm_client_data *client_data = iio_priv(indio_dev);
#endif
	int place = BOSCH_SENSOR_PLACE_UNKNOWN;

#ifdef CONFIG_BMM_USE_PLATFORM_DATA
	if (NULL != client_data->bst_pd)
		place = client_data->bst_pd->place;
#endif
	return sprintf(buf, "%d\n", place);
}

static int bmm_read_axis_data(struct iio_dev *indio_dev,
	struct iio_chan_spec const *ch, struct bmm050_iio_mdata_s32 *raw_data)
{
	int ret = 0;
	unsigned char res_data_arr[BMM_BYTE_FOR_PER_RES_CHANNEL];
	struct bmm050_mdata_s32 value = {0, 0, 0, 0, 0};
	struct  bmm_client_data *client_data = iio_priv(indio_dev);
	struct  i2c_client *client = client_data->client;

	/*Reading data for Resistance*/
	ret = bmm_i2c_read(client_data->client, BMM050_R_LSB,
			res_data_arr, BMM_BYTE_FOR_PER_RES_CHANNEL);
	if (ret < 0)
		return ret;
	/*Get Data ready status bit*/
	raw_data->drdy = BMM050_GET_BITSLICE(res_data_arr[0],
					BMM050_DATA_RDYSTAT);
	if (raw_data->drdy) {
		BMM_CALL_API(read_mdataXYZ_s32)(&value);
#ifdef CONFIG_BMM_USE_PLATFORM_DATA
		bmm_remap_sensor_data(&value, client_data);
#endif
		client_data->value = value;
		client_data->iio_mdata.value_x_valid = VALUE_VALID;
		client_data->iio_mdata.value_y_valid = VALUE_VALID;
		client_data->iio_mdata.value_z_valid = VALUE_VALID;
	}

	switch (ch->scan_index) {
	case BMM_SCAN_MAG_X:
		/* x aixs data */
		if (!client_data->iio_mdata.value_x_valid)
			dev_dbg(&client->dev, "x data not updata!\n");
		raw_data->data = client_data->value.datax;
		client_data->iio_mdata.value_x_valid = VALUE_INVALID;
		break;
	case BMM_SCAN_MAG_Y:
		/* y aixs data */
		if (!client_data->iio_mdata.value_y_valid)
			dev_dbg(&client->dev, "y data not updata!\n");
		raw_data->data = client_data->value.datay;
		client_data->iio_mdata.value_y_valid = VALUE_INVALID;
		break;
	case BMM_SCAN_MAG_Z:
		/* z aixs data */
		if (!client_data->iio_mdata.value_z_valid)
			dev_dbg(&client->dev, "z data not updata!\n");
		raw_data->data = client_data->value.dataz;
		client_data->iio_mdata.value_z_valid = VALUE_INVALID;
		break;

	default:
		return -EINVAL;
	}

	return ret;
}

static int bmm_read_raw(struct iio_dev *indio_dev,
			struct iio_chan_spec const *ch, int *val,
							int *val2, long mask)
{
	int ret, result, err;
	struct bmm050_iio_mdata_s32 raw_data = {0, 0};
	struct  bmm_client_data *client_data = iio_priv(indio_dev);

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
	{
		result = 0;
		ret = IIO_VAL_INT;
		mutex_lock(&indio_dev->mlock);
		switch (ch->type) {
		case IIO_MAGN:
			err = pm_runtime_get_sync(&client_data->client->dev);
			if (err < 0) {
				mutex_unlock(&indio_dev->mlock);
				return err;
			}
			result = bmm_read_axis_data(indio_dev, ch, &raw_data);
			*val  = raw_data.data / 16;
			pm_runtime_mark_last_busy(&client_data->client->dev);
			pm_runtime_put_autosuspend(&client_data->client->dev);
			break;
		default:
			ret = -EINVAL;
			break;
		}
		mutex_unlock(&indio_dev->mlock);
	if (result < 0)
		return result;
	return ret;
	}
	case IIO_CHAN_INFO_SCALE:
		*val = 0;
		*val2 = 3000;
		return IIO_VAL_INT_PLUS_MICRO;
	case IIO_CHAN_INFO_SAMP_FREQ:
		return bmm_get_odr_raw(indio_dev, val);
	default:
		return -EINVAL;
	}

}

static int bmm_write_raw(struct iio_dev *indio_dev,
			 struct iio_chan_spec const *chan,
			 int val, int val2, long mask)
{
	int ret;

	switch (mask) {
	case IIO_CHAN_INFO_SAMP_FREQ:
		return bmm_set_odr_raw(indio_dev, val);
	default:
		ret = -EINVAL;
	}

	return ret;
}

static IIO_CONST_ATTR_SAMP_FREQ_AVAIL(
		"10 2 6 8 15 20 25 30");

static IIO_DEVICE_ATTR(chip_id, S_IRUGO,
		bmm_show_chip_id, NULL, 0);
static IIO_DEVICE_ATTR(op_mode, S_IRUGO|S_IWUSR,
		bmm_show_op_mode, bmm_store_op_mode, 0);
static IIO_DEVICE_ATTR(odr, S_IRUGO|S_IWUSR,
		bmm_show_odr, bmm_store_odr, 0);
static IIO_DEVICE_ATTR(value, S_IRUGO,
		bmm_show_value, NULL, 0);
static IIO_DEVICE_ATTR(value_raw, S_IRUGO,
		bmm_show_value_raw, NULL, 0);
static IIO_DEVICE_ATTR(enable, S_IRUGO|S_IWUSR,
		bmm_show_enable, bmm_store_enable, 0);
static IIO_DEVICE_ATTR(delay, S_IRUGO|S_IWUSR,
		bmm_show_delay, bmm_store_delay, 0);
static IIO_DEVICE_ATTR(test, S_IRUGO|S_IWUSR,
		bmm_show_test, bmm_store_test, 0);
static IIO_DEVICE_ATTR(place, S_IRUGO,
		bmm_show_place, NULL, 0);

static struct attribute *bmm_attributes[] = {
	&iio_dev_attr_chip_id.dev_attr.attr,
	&iio_dev_attr_op_mode.dev_attr.attr,
	&iio_dev_attr_odr.dev_attr.attr,
	&iio_dev_attr_value.dev_attr.attr,
	&iio_dev_attr_value_raw.dev_attr.attr,
	&iio_dev_attr_enable.dev_attr.attr,
	&iio_dev_attr_delay.dev_attr.attr,
	&iio_dev_attr_test.dev_attr.attr,
	&iio_dev_attr_place.dev_attr.attr,
	&iio_const_attr_sampling_frequency_available.dev_attr.attr,
	NULL,
};


static struct attribute_group bmm_attribute_group = {
	.attrs = bmm_attributes
};

static const struct iio_info bmm_iio_info = {
	.driver_module = THIS_MODULE,
	.attrs = &bmm_attribute_group,
	.read_raw = &bmm_read_raw,
	.write_raw = &bmm_write_raw,
};

#ifdef BMM055_USE_INPUT_DEVICE
static int bmm_input_init(struct bmm_client_data *client_data)
{
	struct input_dev *dev;
	int err = 0;

	dev = input_allocate_device();
	if (NULL == dev)
		return -ENOMEM;

	dev->name = SENSOR_NAME;
	dev->id.bustype = BUS_I2C;

	input_set_capability(dev, EV_ABS, ABS_MISC);
	input_set_abs_params(dev, ABS_X, MAG_VALUE_MIN, MAG_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, ABS_Y, MAG_VALUE_MIN, MAG_VALUE_MAX, 0, 0);
	input_set_abs_params(dev, ABS_Z, MAG_VALUE_MIN, MAG_VALUE_MAX, 0, 0);
	input_set_drvdata(dev, client_data);

	err = input_register_device(dev);
	if (err < 0) {
		input_free_device(dev);
		return err;
	}
	client_data->input = dev;

	return 0;
}

static void bmm_input_destroy(struct bmm_client_data *client_data)
{
	struct input_dev *dev = client_data->input;

	input_unregister_device(dev);
	input_free_device(dev);
}
#endif

static int bmm_restore_hw_cfg(struct i2c_client *client)
{
	int err = 0;
	u8 value;
	struct bmm_client_data *client_data =
		(struct bmm_client_data *)i2c_get_clientdata(client);
	int op_mode;

	mutex_lock(&client_data->mutex_op_mode);
	err = bmm_set_op_mode(client_data, BMM_VAL_NAME(SLEEP_MODE));

	op_mode = client_data->op_mode;
	mutex_unlock(&client_data->mutex_op_mode);

	mutex_lock(&client_data->mutex_odr);
	BMM_CALL_API(set_datarate)(client_data->odr);
	mdelay(BMM_I2C_WRITE_DELAY_TIME);
	mutex_unlock(&client_data->mutex_odr);

	mutex_lock(&client_data->mutex_rept_xy);
	err = bmm_i2c_write(client, BMM_REG_NAME(NO_REPETITIONS_XY),
			&client_data->rept_xy, 1);
	mdelay(BMM_I2C_WRITE_DELAY_TIME);
	err = bmm_i2c_read(client, BMM_REG_NAME(NO_REPETITIONS_XY), &value, 1);
	dev_dbg(&client->dev, "BMM_NO_REPETITIONS_XY: %02x", value);
	mutex_unlock(&client_data->mutex_rept_xy);

	mutex_lock(&client_data->mutex_rept_z);
	err = bmm_i2c_write(client, BMM_REG_NAME(NO_REPETITIONS_Z),
			&client_data->rept_z, 1);
	mdelay(BMM_I2C_WRITE_DELAY_TIME);
	err = bmm_i2c_read(client, BMM_REG_NAME(NO_REPETITIONS_Z), &value, 1);
	dev_dbg(&client->dev, "BMM_NO_REPETITIONS_Z: %02x", value);
	mutex_unlock(&client_data->mutex_rept_z);

	mutex_lock(&client_data->mutex_op_mode);
	if (BMM_OP_MODE_UNKNOWN == client_data->op_mode) {
		bmm_set_forced_mode(client);
		dev_dbg(&client->dev, "set forced mode after hw_restore");
		mdelay(bmm_get_forced_drdy_time(client_data->rept_xy,
					client_data->rept_z));
	}
	mutex_unlock(&client_data->mutex_op_mode);

	dev_dbg(&client->dev, "register dump after init");
	bmm_dump_reg(client);

	return err;
}

static int bmm_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int err = 0;
	int dummy;
	struct iio_dev *indio_dev;
	struct bmm_client_data *client_data = NULL;
	#ifdef CONFIG_BMM_USE_PLATFORM_DATA
	struct device_node *np = client->dev.of_node;
	#endif
	dev_info(&client->dev, "bmm function entrance");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "i2c_check_functionality error!");
		return -EIO;
	}

	if (!bmm_client)
		bmm_client = client;
	else {
		dev_err(&client->dev,
			"this driver does not support multiple clients");
		return -EBUSY;
	}

	/* wake up the chip */
	dummy = bmm_wakeup(client);
	if (dummy < 0) {
		dev_err(&client->dev,
			"Cannot wake up %s, I2C xfer error", SENSOR_NAME);
		err = -EIO;
		goto exit_err_clean;
	}

	dev_info(&client->dev, "register dump after waking up");
	bmm_dump_reg(client);

	/* check chip id */
	err = bmm_check_chip_id(client);
	if (!err) {
		dev_info(&client->dev, "Bosch Sensortec Device %s detected: %#x",
				SENSOR_NAME, client->addr);
	} else {
		dev_err(&client->dev,
			"Bosch Sensortec Device not found, chip id mismatch");
		err = -ENXIO;
		goto exit_err_clean;
	}

	indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*client_data));
	if (!indio_dev)
		return -ENOMEM;

	client_data = iio_priv(indio_dev);
	client_data->client = client;
	i2c_set_clientdata(client, client_data);
	client_data->indio_dev = indio_dev;

	mutex_init(&client_data->mutex_power_mode);
	mutex_init(&client_data->mutex_op_mode);
	mutex_init(&client_data->mutex_enable);
	mutex_init(&client_data->mutex_odr);
	mutex_init(&client_data->mutex_rept_xy);
	mutex_init(&client_data->mutex_rept_z);
	mutex_init(&client_data->mutex_value);
#ifdef BMM055_USE_INPUT_DEVICE
	/* input device init */
	err = bmm_input_init(client_data);
	if (err < 0)
		goto exit_err_clean;
#endif

	indio_dev->dev.parent = &client->dev;
	indio_dev->name = client->name;
	indio_dev->channels = bmm050_raw_channels;
	indio_dev->num_channels = ARRAY_SIZE(bmm050_raw_channels);
	indio_dev->info = &bmm_iio_info;
	indio_dev->modes = INDIO_DIRECT_MODE;


#ifdef CONFIG_BMM_USE_PLATFORM_DATA
	client_data->bst_pd = kzalloc(sizeof(*client_data->bst_pd),
					GFP_KERNEL);
	if (NULL != client_data->bst_pd) {
		/*read some parameter from DTS*/
		of_property_read_u32(np, "place",
					&(client_data->bst_pd->place));
		dev_dbg(&client->dev,
				"platform data of bmm %s: place: %d, irq: %d",
				client_data->bst_pd->name,
				client_data->bst_pd->place,
				client_data->bst_pd->irq);
	}
#endif
#ifdef BMM055_USE_INPUT_DEVICE
	/* workqueue init */
	INIT_DELAYED_WORK(&client_data->work, bmm_work_func);
	atomic_set(&client_data->delay, BMM_DELAY_DEFAULT);
#endif
	/* h/w init */
	client_data->device.bus_read = bmm_i2c_read_wrapper;
	client_data->device.bus_write = bmm_i2c_write_wrapper;
	client_data->device.delay_msec = bmm_delay;
	BMM_CALL_API(init)(&client_data->device);

	bmm_dump_reg(client);

	client_data->enable = 0;
	/* now it's power on which is considered as resuming from suspend */
	client_data->op_mode = BMM_VAL_NAME(SUSPEND_MODE);
	client_data->odr = BMM_DEFAULT_ODR;
	client_data->rept_xy = BMM_DEFAULT_REPETITION_XY;
	client_data->rept_z = BMM_DEFAULT_REPETITION_Z;

#ifdef BMM050_TRIGGER_ENABLE
	err = iio_triggered_buffer_setup(indio_dev,
					    &iio_pollfunc_store_time,
					    &bmm_buffer_handler,
					    NULL);
	if (err) {
		dev_err(indio_dev->dev.parent,
				"bmm configure buffer fail %d\n", err);
		return err;
	}
	err = bmm_probe_trigger(indio_dev);
	if (err < 0) {
		dev_err(indio_dev->dev.parent, "trigger probe fail %d\n", err);
		goto err_unreg_ring;
	}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	client_data->early_suspend_handler.level =
		EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	client_data->early_suspend_handler.suspend = bmm_early_suspend;
	client_data->early_suspend_handler.resume = bmm_late_resume;
	register_early_suspend(&client_data->early_suspend_handler);
#endif
	err = iio_device_register(indio_dev);
	if (err) {
		dev_err(indio_dev->dev.parent,
				"bmm IIO device register failed %d\n", err);
		goto exit_err_sysfs;
	}
	dev_info(&client->dev, "sensor %s probed successfully", SENSOR_NAME);

	dev_dbg(&client->dev,
		"i2c_client: %p client_data: %p i2c_device: %p input: %p",
		client, client_data, &client->dev, client_data->input);

	pm_runtime_set_active(&client->dev);
	pm_runtime_enable(&client->dev);
	pm_runtime_set_autosuspend_delay(&client->dev, 2000);
	pm_runtime_use_autosuspend(&client->dev);

	return 0;


exit_err_sysfs:
#ifdef BMM055_USE_INPUT_DEVICE
	if (err)
		bmm_input_destroy(client_data);
#endif

#ifdef BMM050_TRIGGER_ENABLE
err_unreg_ring:
	bmm_deallocate_ring(indio_dev);
#endif
exit_err_clean:
	if (err)
		bmm_client = NULL;

	return err;
}

static int bmm_pre_suspend(struct i2c_client *client)
{
	int err = 0;
	struct bmm_client_data *client_data =
		(struct bmm_client_data *)i2c_get_clientdata(client);
	dev_dbg(&client->dev, "function entrance");

	mutex_lock(&client_data->mutex_enable);
	if (client_data->enable) {
#ifdef BMM055_USE_INPUT_DEVICE
		cancel_delayed_work_sync(&client_data->work);
#endif
		dev_dbg(&client->dev, "cancel work");
	}
	mutex_unlock(&client_data->mutex_enable);

	return err;
}

static int bmm_post_resume(struct i2c_client *client)
{
	int err = 0;
	struct bmm_client_data *client_data =
		(struct bmm_client_data *)i2c_get_clientdata(client);

	mutex_lock(&client_data->mutex_enable);
	if (client_data->enable) {
#ifdef BMM055_USE_INPUT_DEVICE
		schedule_delayed_work(&client_data->work,
				msecs_to_jiffies(
					atomic_read(&client_data->delay)));
#endif
	}
	mutex_unlock(&client_data->mutex_enable);

	return err;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
devm_static void bmm_early_suspend(struct early_suspend *handler)
{
	struct bmm_client_data *client_data =
		(struct bmm_client_data *)container_of(handler,
			struct bmm_client_data, early_suspend_handler);
	struct i2c_client *client = client_data->client;
	u8 power_mode;
	dev_dbg(&client->dev, "function entrance");

	mutex_lock(&client_data->mutex_power_mode);
	BMM_CALL_API(get_powermode)(&power_mode);
	if (power_mode) {
		bmm_pre_suspend(client);
		bmm_set_op_mode(client_data, BMM_VAL_NAME(SUSPEND_MODE));
	}
	mutex_unlock(&client_data->mutex_power_mode);

}

static void bmm_late_resume(struct early_suspend *handler)
{
	struct bmm_client_data *client_data =
		(struct bmm_client_data *)container_of(handler,
			struct bmm_client_data, early_suspend_handler);
	struct i2c_client *client = client_data->client;
	dev_dbg(&client->dev, "function entrance");

	mutex_lock(&client_data->mutex_power_mode);

	bmm_restore_hw_cfg(client);
	/* post resume operation */
	bmm_post_resume(client);

	mutex_unlock(&client_data->mutex_power_mode);
}
#else
#ifdef CONFIG_PM
static int bmm_suspend(struct device *dev)
{
	int err = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct bmm_client_data *client_data =
		(struct bmm_client_data *)i2c_get_clientdata(client);
	u8 power_mode;

	dev_dbg(&client->dev, "function entrance %s", __func__);

	mutex_lock(&client_data->mutex_power_mode);
	BMM_CALL_API(get_powermode)(&power_mode);
	if (power_mode) {
		err = bmm_pre_suspend(client);
		err = bmm_set_op_mode(client_data, BMM_VAL_NAME(SUSPEND_MODE));
	}
	mutex_unlock(&client_data->mutex_power_mode);

	return err;
}

static int bmm_resume(struct device *dev)
{
	int err = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct bmm_client_data *client_data =
		(struct bmm_client_data *)i2c_get_clientdata(client);

	dev_dbg(&client->dev, "function entrance %s", __func__);

	/* Soft reset the chipset */
	BMM_CALL_API(soft_reset)();
	mdelay(BMM_I2C_WRITE_DELAY_TIME);

	/* Wake it up */
	bmm_wakeup(client);

	/* And then restore the HW configuration */
	mutex_lock(&client_data->mutex_power_mode);
	err = bmm_restore_hw_cfg(client);
	err = bmm_set_op_mode(client_data, client_data->op_mode);
	if (err) {
		dev_err(&client->dev, "fail to set opmode %d",
						client_data->op_mode);
		mutex_unlock(&client_data->mutex_power_mode);
		return err;
	}
	/* post resume operation */
	bmm_post_resume(client);

	mutex_unlock(&client_data->mutex_power_mode);

	return err;
}
#endif /* CONFIG_PM */
#endif

#ifdef CONFIG_PM_RUNTIME
static int bmm_runtime_suspend(struct device *dev)
{
	int err;
	struct i2c_client *client = to_i2c_client(dev);
	struct bmm_client_data *client_data =
		(struct bmm_client_data *)i2c_get_clientdata(client);

	dev_dbg(&client->dev, "function entrance %s", __func__);
	err = bmm_set_op_mode(client_data, BMM_VAL_NAME(SUSPEND_MODE));
	if (err) {
		dev_err(&client->dev, "fail to set opmode %d",
					BMM_VAL_NAME(SUSPEND_MODE));
		return err;
	} else
		client_data->op_mode = BMM_VAL_NAME(SUSPEND_MODE);

	return 0;
}

static int bmm_runtime_resume(struct device *dev)
{
	int err;
	struct i2c_client *client = to_i2c_client(dev);
	struct bmm_client_data *client_data =
		(struct bmm_client_data *)i2c_get_clientdata(client);

	dev_dbg(&client->dev, "function entrance %s", __func__);
	err = bmm_restore_hw_cfg(client);
	if (err < 0) {
		dev_err(&client->dev, "fail to restore hw config %d",
					BMM_VAL_NAME(NORMAL_MODE));
		return err;
	}

	err = bmm_set_op_mode(client_data, BMM_VAL_NAME(NORMAL_MODE));
	if (err) {
		dev_err(&client->dev, "fail to set opmode %d",
					BMM_VAL_NAME(NORMAL_MODE));
		return err;
	} else
		client_data->op_mode = BMM_VAL_NAME(NORMAL_MODE);

	bmm_dump_reg(client);

	return 0;
}
#endif

static int bmm_remove(struct i2c_client *client)
{
	int err = 0;
	struct bmm_client_data *client_data =
		(struct bmm_client_data *)i2c_get_clientdata(client);

	struct  iio_dev *indio_dev = client_data->indio_dev;

	pm_runtime_disable(&client->dev);
	pm_runtime_set_suspended(&client->dev);
	pm_runtime_put_noidle(&client->dev);

	if (NULL != client_data) {
#ifdef CONFIG_HAS_EARLYSUSPEND
		unregister_early_suspend(&client_data->early_suspend_handler);
#endif
#ifdef BMM055_USE_INPUT_DEVICE
		mutex_lock(&client_data->mutex_op_mode);
		if (BMM_VAL_NAME(NORMAL_MODE) == client_data->op_mode) {
			cancel_delayed_work_sync(&client_data->work);
			dev_dbg(&client->dev, "cancel work");
		}
		mutex_unlock(&client_data->mutex_op_mode);
#endif

		err = bmm_set_op_mode(client_data, BMM_VAL_NAME(SUSPEND_MODE));
		mdelay(BMM_I2C_WRITE_DELAY_TIME);

#ifdef BMM055_USE_INPUT_DEVICE
		bmm_input_destroy(client_data);
#endif
#ifdef CONFIG_BMM_USE_PLATFORM_DATA
			if (NULL != client_data->bst_pd) {
				kfree(client_data->bst_pd);
				client_data->bst_pd = NULL;
			}
#endif
		iio_device_unregister(indio_dev);

		bmm_client = NULL;
	}

	return 0;
}

static const struct dev_pm_ops bmm_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(bmm_suspend, bmm_resume)
	SET_RUNTIME_PM_OPS(bmm_runtime_suspend,
			   bmm_runtime_resume, NULL)
};

static const struct i2c_device_id bmm_id[] = {
	{SENSOR_NAME, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, bmm_id);

static const struct acpi_device_id bmm050_acpi_match[] = {
	{ "BMM0050", 0 },
	{ },
};
MODULE_DEVICE_TABLE(acpi, bmm050_acpi_match);

static struct i2c_driver bmm_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = SENSOR_NAME,
		.acpi_match_table = ACPI_PTR(bmm050_acpi_match),
		.pm = &bmm_pm_ops,
	},
	.class = I2C_CLASS_HWMON,
	.id_table = bmm_id,
	.probe = bmm_probe,
	.remove = bmm_remove,
};
module_i2c_driver(bmm_driver);

MODULE_AUTHOR("contact@bosch.sensortec.com");
MODULE_DESCRIPTION("driver for " SENSOR_NAME);
MODULE_LICENSE("GPL");
