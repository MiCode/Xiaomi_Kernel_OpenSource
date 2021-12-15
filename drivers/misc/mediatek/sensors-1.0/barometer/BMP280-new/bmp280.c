// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#define pr_fmt(fmt) "<BMP280> " fmt

#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kobject.h>
#include <linux/math64.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>

#include "barometer.h"
#include "bmp280.h"
#include <cust_baro.h>
/* #include <linux/hwmsen_helper.h> */

/* #define POWER_NONE_MACRO MT65XX_POWER_NONE */

#define DRIVER_ATTR(_name, _mode, _show, _store) \
        struct driver_attribute driver_attr_##_name = \
        __ATTR(_name, _mode, _show, _store)

/* sensor type */
enum SENSOR_TYPE_ENUM {
	BMP280_TYPE = 0x0,

	INVALID_TYPE = 0xff
};

/* power mode */
enum BMP_POWERMODE_ENUM {
	BMP_SUSPEND_MODE = 0x0,
	BMP_NORMAL_MODE,

	BMP_UNDEFINED_POWERMODE = 0xff
};

/* filter */
enum BMP_FILTER_ENUM {
	BMP_FILTER_OFF = 0x0,
	BMP_FILTER_2,
	BMP_FILTER_4,
	BMP_FILTER_8,
	BMP_FILTER_16,

	BMP_UNDEFINED_FILTER = 0xff
};

/* oversampling */
enum BMP_OVERSAMPLING_ENUM {
	BMP_OVERSAMPLING_SKIPPED = 0x0,
	BMP_OVERSAMPLING_1X,
	BMP_OVERSAMPLING_2X,
	BMP_OVERSAMPLING_4X,
	BMP_OVERSAMPLING_8X,
	BMP_OVERSAMPLING_16X,

	BMP_UNDEFINED_OVERSAMPLING = 0xff
};

/* trace */
enum BAR_TRC {
	BAR_TRC_READ = 0x01,
	BAR_TRC_RAWDATA = 0x02,
	BAR_TRC_IOCTL = 0x04,
	BAR_TRC_FILTER = 0x08,
	BAR_TRC_INFO = 0x10,
};

/* s/w filter */
struct data_filter {
	u32 raw[C_MAX_FIR_LENGTH][BMP_DATA_NUM];
	int sum[BMP_DATA_NUM];
	int num;
	int idx;
};

/* bmp280 calibration */
struct bmp280_calibration_data {
	BMP280_U16_t dig_T1;
	BMP280_S16_t dig_T2;
	BMP280_S16_t dig_T3;
	BMP280_U16_t dig_P1;
	BMP280_S16_t dig_P2;
	BMP280_S16_t dig_P3;
	BMP280_S16_t dig_P4;
	BMP280_S16_t dig_P5;
	BMP280_S16_t dig_P6;
	BMP280_S16_t dig_P7;
	BMP280_S16_t dig_P8;
	BMP280_S16_t dig_P9;
};

/* bmp i2c client data */
struct bmp_i2c_data {
	struct i2c_client *client;
	struct baro_hw hw;

	/* sensor info */
	u8 sensor_name[MAX_SENSOR_NAME];
	enum SENSOR_TYPE_ENUM sensor_type;
	enum BMP_POWERMODE_ENUM power_mode;
	u8 hw_filter;
	u8 oversampling_p;
	u8 oversampling_t;
	unsigned long last_temp_measurement;
	unsigned long temp_measurement_period;
	struct bmp280_calibration_data bmp280_cali;

	/* calculated temperature correction coefficient */
	s32 t_fine;

	/*misc */
	struct mutex lock;
	atomic_t trace;
	atomic_t suspend;
	atomic_t filter;

#if defined(CONFIG_BMP_LOWPASS)
	atomic_t firlen;
	atomic_t fir_en;
	struct data_filter fir;
#endif
};

static struct i2c_driver bmp_i2c_driver;
static struct bmp_i2c_data *obj_i2c_data;
static const struct i2c_device_id bmp_i2c_id[] = {
	{BMP_DEV_NAME, 0},
	{}
};

#ifdef CONFIG_MTK_LEGACY
static struct i2c_board_info bmp_i2c_info __initdata = {
	I2C_BOARD_INFO(BMP_DEV_NAME, BMP280_I2C_ADDRESS)
};
#endif
static int bmp_local_init(void);
static int bmp_remove(void);
static int bmp_init_flag = -1;
static struct baro_init_info bmp_init_info = {
	.name = "bmp",
	.init = bmp_local_init,
	.uninit = bmp_remove,
};

/* I2C operation functions */
static int bmp_i2c_read_block(struct i2c_client *client, u8 addr, u8 *data,
			      u8 len)
{
	u8 reg_addr = addr;
	u8 *rxbuf = data;
	u8 left = len;
	u8 retry;
	u8 offset = 0;

	struct i2c_msg msg[2] = {
		{
			.addr = client->addr,
			.flags = 0,
			.buf = &reg_addr,
			.len = 1,
		},
		{
		 .addr = client->addr,
		 .flags = I2C_M_RD,
		},
	};

	if (rxbuf == NULL)
		return -1;

	while (left > 0) {
		retry = 0;
		reg_addr = addr + offset;
		msg[1].buf = &rxbuf[offset];

		if (left > C_I2C_FIFO_SIZE) {
			msg[1].len = C_I2C_FIFO_SIZE;
			left -= C_I2C_FIFO_SIZE;
			offset += C_I2C_FIFO_SIZE;
		} else {
			msg[1].len = left;
			left = 0;
		}

		while (i2c_transfer(client->adapter, &msg[0], 2) != 2) {
			retry++;

			if (retry == 20) {
				pr_err("i2c read reg=%#x length=%d failed\n",
					addr + offset, len);
				return -EIO;
			}
		}
	}

	return 0;
}

static int bmp_i2c_write_block(struct i2c_client *client, u8 addr, u8 *data,
			       u8 len)
{
	u8 buffer[C_I2C_FIFO_SIZE];
	u8 *txbuf = data;
	u8 left = len;
	u8 offset = 0;
	u8 retry = 0;

	struct i2c_msg msg = {
		.addr = client->addr, .flags = 0, .buf = buffer,
	};

	if (txbuf == NULL)
		return -1;

	while (left > 0) {
		retry = 0;
		/* register address */
		buffer[0] = addr + offset;

		if (left >= C_I2C_FIFO_SIZE) {
			memcpy(&buffer[1], &txbuf[offset], C_I2C_FIFO_SIZE - 1);
			msg.len = C_I2C_FIFO_SIZE;
			left -= C_I2C_FIFO_SIZE - 1;
			offset += C_I2C_FIFO_SIZE - 1;
		} else {
			memcpy(&buffer[1], &txbuf[offset], left);
			msg.len = left + 1;
			left = 0;
		}

		while (i2c_transfer(client->adapter, &msg, 1) != 1) {
			retry++;

			if (retry == 20) {
				pr_err("i2c write reg=%#x length=%d failed\n",
					buffer[0], len);
				return -EIO;
			}

			pr_debug("i2c write addr %#x, retry %d\n", buffer[0],
				retry);
		}
	}

	return 0;
}

/* get chip type */
static int bmp_get_chip_type(struct i2c_client *client)
{
	int err = 0;
	u8 chip_id = 0;
	struct bmp_i2c_data *obj = i2c_get_clientdata(client);

	/* pr_debug("%s\n", __func__);*/

	err = bmp_i2c_read_block(client, BMP_CHIP_ID_REG, &chip_id, 0x01);
	if (err != 0)
		return err;

	switch (chip_id) {
	case BMP280_CHIP_ID1:
	case BMP280_CHIP_ID2:
	case BMP280_CHIP_ID3:
		obj->sensor_type = BMP280_TYPE;
		strlcpy(obj->sensor_name, "bmp280", sizeof(obj->sensor_name));
		break;
	default:
		obj->sensor_type = INVALID_TYPE;
		strlcpy(obj->sensor_name, "unknown sensor",
			sizeof(obj->sensor_name));
		break;
	}

	if (atomic_read(&obj->trace) & BAR_TRC_INFO)
		pr_debug("[%s]chip id = %#x, sensor name = %s\n", __func__,
			chip_id, obj->sensor_name);

	if (obj->sensor_type == INVALID_TYPE) {
		pr_err("unknown pressure sensor\n");
		return -1;
	}
	return 0;
}

static int bmp_get_calibration_data(struct i2c_client *client)
{
	struct bmp_i2c_data *obj =
		(struct bmp_i2c_data *)i2c_get_clientdata(client);
	int status = 0;

	if (obj->sensor_type == BMP280_TYPE) {
		u8 a_data_u8r[BMP280_CALIBRATION_DATA_LENGTH] = {0};

		status = bmp_i2c_read_block(
			client, BMP280_CALIBRATION_DATA_START, a_data_u8r,
			BMP280_CALIBRATION_DATA_LENGTH);
		if (status < 0)
			return status;

		obj->bmp280_cali.dig_T1 = (BMP280_U16_t)(
			(((BMP280_U16_t)((unsigned char)a_data_u8r[1]))
			 << SHIFT_LEFT_8_POSITION) |
			a_data_u8r[0]);
		obj->bmp280_cali.dig_T2 = (BMP280_S16_t)(
			(((BMP280_S16_t)((signed char)a_data_u8r[3]))
			 << SHIFT_LEFT_8_POSITION) |
			a_data_u8r[2]);
		obj->bmp280_cali.dig_T3 = (BMP280_S16_t)(
			(((BMP280_S16_t)((signed char)a_data_u8r[5]))
			 << SHIFT_LEFT_8_POSITION) |
			a_data_u8r[4]);
		obj->bmp280_cali.dig_P1 = (BMP280_U16_t)(
			(((BMP280_U16_t)((unsigned char)a_data_u8r[7]))
			 << SHIFT_LEFT_8_POSITION) |
			a_data_u8r[6]);
		obj->bmp280_cali.dig_P2 = (BMP280_S16_t)(
			(((BMP280_S16_t)((signed char)a_data_u8r[9]))
			 << SHIFT_LEFT_8_POSITION) |
			a_data_u8r[8]);
		obj->bmp280_cali.dig_P3 = (BMP280_S16_t)(
			(((BMP280_S16_t)((signed char)a_data_u8r[11]))
			 << SHIFT_LEFT_8_POSITION) |
			a_data_u8r[10]);
		obj->bmp280_cali.dig_P4 = (BMP280_S16_t)(
			(((BMP280_S16_t)((signed char)a_data_u8r[13]))
			 << SHIFT_LEFT_8_POSITION) |
			a_data_u8r[12]);
		obj->bmp280_cali.dig_P5 = (BMP280_S16_t)(
			(((BMP280_S16_t)((signed char)a_data_u8r[15]))
			 << SHIFT_LEFT_8_POSITION) |
			a_data_u8r[14]);
		obj->bmp280_cali.dig_P6 = (BMP280_S16_t)(
			(((BMP280_S16_t)((signed char)a_data_u8r[17]))
			 << SHIFT_LEFT_8_POSITION) |
			a_data_u8r[16]);
		obj->bmp280_cali.dig_P7 = (BMP280_S16_t)(
			(((BMP280_S16_t)((signed char)a_data_u8r[19]))
			 << SHIFT_LEFT_8_POSITION) |
			a_data_u8r[18]);
		obj->bmp280_cali.dig_P8 = (BMP280_S16_t)(
			(((BMP280_S16_t)((signed char)a_data_u8r[21]))
			 << SHIFT_LEFT_8_POSITION) |
			a_data_u8r[20]);
		obj->bmp280_cali.dig_P9 = (BMP280_S16_t)(
			(((BMP280_S16_t)((signed char)a_data_u8r[23]))
			 << SHIFT_LEFT_8_POSITION) |
			a_data_u8r[22]);
	}
	return 0;
}

static int bmp_set_powermode(struct i2c_client *client,
			     enum BMP_POWERMODE_ENUM power_mode)
{
	struct bmp_i2c_data *obj = i2c_get_clientdata(client);
	u8 err = 0, data = 0, actual_power_mode = 0;

	if (atomic_read(&obj->trace) & BAR_TRC_INFO)
		pr_debug("[%s] p_m = %d, old p_m = %d\n", __func__,
			power_mode, obj->power_mode);

	if (power_mode == obj->power_mode)
		return 0;

	mutex_lock(&obj->lock);

	if (obj->sensor_type == BMP280_TYPE) { /* BMP280 */
		if (power_mode == BMP_SUSPEND_MODE) {
			actual_power_mode = BMP280_SLEEP_MODE;
		} else if (power_mode == BMP_NORMAL_MODE) {
			actual_power_mode = BMP280_NORMAL_MODE;
		} else {
			err = -EINVAL;
			pr_err("invalid power mode = %d\n", power_mode);
			mutex_unlock(&obj->lock);
			return err;
		}
		err = bmp_i2c_read_block(client, BMP280_CTRLMEAS_REG_MODE__REG,
					 &data, 1);
		data = BMP_SET_BITSLICE(data, BMP280_CTRLMEAS_REG_MODE,
					actual_power_mode);
		err += bmp_i2c_write_block(
			client, BMP280_CTRLMEAS_REG_MODE__REG, &data, 1);
	}

	if (err < 0)
		pr_err("set power mode failed, err = %d, sensor name = %s\n",
			err, obj->sensor_name);
	else
		obj->power_mode = power_mode;

	mutex_unlock(&obj->lock);
	return err;
}

static int bmp_set_filter(struct i2c_client *client,
			  enum BMP_FILTER_ENUM filter)
{
	struct bmp_i2c_data *obj = i2c_get_clientdata(client);
	u8 err = 0, data = 0, actual_filter = 0;

	if (atomic_read(&obj->trace) & BAR_TRC_INFO)
		pr_debug("[%s] hw filter = %d, old hw filter = %d\n", __func__,
			filter, obj->hw_filter);

	if (filter == obj->hw_filter)
		return 0;

	mutex_lock(&obj->lock);

	if (obj->sensor_type == BMP280_TYPE) { /* BMP280 */
		if (filter == BMP_FILTER_OFF)
			actual_filter = BMP280_FILTERCOEFF_OFF;
		else if (filter == BMP_FILTER_2)
			actual_filter = BMP280_FILTERCOEFF_2;
		else if (filter == BMP_FILTER_4)
			actual_filter = BMP280_FILTERCOEFF_4;
		else if (filter == BMP_FILTER_8)
			actual_filter = BMP280_FILTERCOEFF_8;
		else if (filter == BMP_FILTER_16)
			actual_filter = BMP280_FILTERCOEFF_16;
		else {
			err = -EINVAL;
			pr_err("invalid hw filter = %d\n", filter);
			mutex_unlock(&obj->lock);
			return err;
		}
		err = bmp_i2c_read_block(client, BMP280_CONFIG_REG_FILTER__REG,
					 &data, 1);
		data = BMP_SET_BITSLICE(data, BMP280_CONFIG_REG_FILTER,
					actual_filter);
		err += bmp_i2c_write_block(
			client, BMP280_CONFIG_REG_FILTER__REG, &data, 1);
	}

	if (err < 0)
		pr_err("set hw filter failed, err = %d, sensor name = %s\n",
			err, obj->sensor_name);
	else
		obj->hw_filter = filter;

	mutex_unlock(&obj->lock);
	return err;
}

static int bmp_set_oversampling_p(struct i2c_client *client,
				  enum BMP_OVERSAMPLING_ENUM oversampling_p)
{
	struct bmp_i2c_data *obj = i2c_get_clientdata(client);
	u8 err = 0, data = 0, actual_oversampling_p = 0;

	if (atomic_read(&obj->trace) & BAR_TRC_INFO)
		pr_debug("[%s] oversampling_p = %d, old oversampling_p = %d\n",
			__func__, oversampling_p, obj->oversampling_p);

	if (oversampling_p == obj->oversampling_p)
		return 0;

	mutex_lock(&obj->lock);

	if (obj->sensor_type == BMP280_TYPE) { /* BMP280 */
		if (oversampling_p == BMP_OVERSAMPLING_SKIPPED)
			actual_oversampling_p = BMP280_OVERSAMPLING_SKIPPED;
		else if (oversampling_p == BMP_OVERSAMPLING_1X)
			actual_oversampling_p = BMP280_OVERSAMPLING_1X;
		else if (oversampling_p == BMP_OVERSAMPLING_2X)
			actual_oversampling_p = BMP280_OVERSAMPLING_2X;
		else if (oversampling_p == BMP_OVERSAMPLING_4X)
			actual_oversampling_p = BMP280_OVERSAMPLING_4X;
		else if (oversampling_p == BMP_OVERSAMPLING_8X)
			actual_oversampling_p = BMP280_OVERSAMPLING_8X;
		else if (oversampling_p == BMP_OVERSAMPLING_16X)
			actual_oversampling_p = BMP280_OVERSAMPLING_16X;
		else {
			err = -EINVAL;
			pr_err("invalid oversampling_p = %d\n",
				oversampling_p);
			mutex_unlock(&obj->lock);
			return err;
		}
		err = bmp_i2c_read_block(client, BMP280_CTRLMEAS_REG_OSRSP__REG,
					 &data, 1);
		data = BMP_SET_BITSLICE(data, BMP280_CTRLMEAS_REG_OSRSP,
					actual_oversampling_p);
		err += bmp_i2c_write_block(
			client, BMP280_CTRLMEAS_REG_OSRSP__REG, &data, 1);
	}

	if (err < 0)
		pr_err("set pressure oversampling failed, err = %d,sensor name = %s\n",
			err, obj->sensor_name);
	else
		obj->oversampling_p = oversampling_p;

	mutex_unlock(&obj->lock);
	return err;
}

static int bmp_set_oversampling_t(struct i2c_client *client,
				  enum BMP_OVERSAMPLING_ENUM oversampling_t)
{
	struct bmp_i2c_data *obj = i2c_get_clientdata(client);
	u8 err = 0, data = 0, actual_oversampling_t = 0;

	if (atomic_read(&obj->trace) & BAR_TRC_INFO)
		pr_debug("[%s] oversampling_t = %d, old oversampling_t = %d\n",
			__func__, oversampling_t, obj->oversampling_t);

	if (oversampling_t == obj->oversampling_t)
		return 0;

	mutex_lock(&obj->lock);

	if (obj->sensor_type == BMP280_TYPE) { /* BMP280 */
		if (oversampling_t == BMP_OVERSAMPLING_SKIPPED)
			actual_oversampling_t = BMP280_OVERSAMPLING_SKIPPED;
		else if (oversampling_t == BMP_OVERSAMPLING_1X)
			actual_oversampling_t = BMP280_OVERSAMPLING_1X;
		else if (oversampling_t == BMP_OVERSAMPLING_2X)
			actual_oversampling_t = BMP280_OVERSAMPLING_2X;
		else if (oversampling_t == BMP_OVERSAMPLING_4X)
			actual_oversampling_t = BMP280_OVERSAMPLING_4X;
		else if (oversampling_t == BMP_OVERSAMPLING_8X)
			actual_oversampling_t = BMP280_OVERSAMPLING_8X;
		else if (oversampling_t == BMP_OVERSAMPLING_16X)
			actual_oversampling_t = BMP280_OVERSAMPLING_16X;
		else {
			err = -EINVAL;
			pr_err("invalid oversampling_t = %d\n",
				oversampling_t);
			mutex_unlock(&obj->lock);
			return err;
		}
		err = bmp_i2c_read_block(client, BMP280_CTRLMEAS_REG_OSRST__REG,
					 &data, 1);
		data = BMP_SET_BITSLICE(data, BMP280_CTRLMEAS_REG_OSRST,
					actual_oversampling_t);
		err += bmp_i2c_write_block(
			client, BMP280_CTRLMEAS_REG_OSRST__REG, &data, 1);
	}

	if (err < 0)
		pr_err("set temperature oversampling failed, err = %d, sensor name = %s\n",
			err, obj->sensor_name);
	else
		obj->oversampling_t = oversampling_t;

	mutex_unlock(&obj->lock);
	return err;
}

static int bmp_read_raw_temperature(struct i2c_client *client, s32 *temperature)
{
	struct bmp_i2c_data *obj;
	s32 err = 0;

	if (client == NULL) {
		err = -EINVAL;
		return err;
	}

	obj = i2c_get_clientdata(client);

	mutex_lock(&obj->lock);

	if (obj->sensor_type == BMP280_TYPE) { /* BMP280 */
		unsigned char a_data_u8r[3] = {0};

		err = bmp_i2c_read_block(client, BMP280_TEMPERATURE_MSB_REG,
					 a_data_u8r, 3);
		if (err < 0) {
			pr_err("read raw temperature failed, err = %d\n", err);
			mutex_unlock(&obj->lock);
			return err;
		}
		*temperature = (BMP280_S32_t)((((BMP280_U32_t)(a_data_u8r[0]))
					       << SHIFT_LEFT_12_POSITION) |
					      (((BMP280_U32_t)(a_data_u8r[1]))
					       << SHIFT_LEFT_4_POSITION) |
					      ((BMP280_U32_t)a_data_u8r[2] >>
					       SHIFT_RIGHT_4_POSITION));
	}
	obj->last_temp_measurement = jiffies;
	mutex_unlock(&obj->lock);

	return err;
}

static int bmp_read_raw_pressure(struct i2c_client *client, s32 *pressure)
{
	struct bmp_i2c_data *priv;
	s32 err = 0;

	if (client == NULL) {
		err = -EINVAL;
		return err;
	}

	priv = i2c_get_clientdata(client);

	mutex_lock(&priv->lock);

	if (priv->sensor_type == BMP280_TYPE) { /* BMP280 */
		unsigned char a_data_u8r[3] = {0};

		err = bmp_i2c_read_block(client, BMP280_PRESSURE_MSB_REG,
					 a_data_u8r, 3);
		if (err < 0) {
			pr_err("read raw pressure failed, err = %d\n", err);
			mutex_unlock(&priv->lock);
			return err;
		}
		*pressure = (BMP280_S32_t)((((BMP280_U32_t)(a_data_u8r[0]))
					    << SHIFT_LEFT_12_POSITION) |
					   (((BMP280_U32_t)(a_data_u8r[1]))
					    << SHIFT_LEFT_4_POSITION) |
					   ((BMP280_U32_t)a_data_u8r[2] >>
					    SHIFT_RIGHT_4_POSITION));
	}
#ifdef CONFIG_BMP_LOWPASS
	/*
	 *Example: firlen = 16, filter buffer = [0] ... [15],
	 *when 17th data come, replace [0] with this new data.
	 *Then, average this filter buffer and report average value to upper
	 *layer.
	 */
	if (atomic_read(&priv->filter)) {
		if (atomic_read(&priv->fir_en) &&
		    !atomic_read(&priv->suspend)) {
			int idx, firlen = atomic_read(&priv->firlen);

			if (priv->fir.num < firlen) {
				priv->fir.raw[priv->fir.num][BMP_PRESSURE] =
					*pressure;
				priv->fir.sum[BMP_PRESSURE] += *pressure;
				if (atomic_read(&priv->trace) &
				    BAR_TRC_FILTER) {
					pr_debug("add [%2d] [%5d] => [%5d]\n",
						priv->fir.num,
						priv->fir.raw[priv->fir.num]
							     [BMP_PRESSURE],
						priv->fir.sum[BMP_PRESSURE]);
				}
				priv->fir.num++;
				priv->fir.idx++;
			} else {
				idx = priv->fir.idx % firlen;
				priv->fir.sum[BMP_PRESSURE] -=
					priv->fir.raw[idx][BMP_PRESSURE];
				priv->fir.raw[idx][BMP_PRESSURE] = *pressure;
				priv->fir.sum[BMP_PRESSURE] += *pressure;
				priv->fir.idx++;
				*pressure =
					priv->fir.sum[BMP_PRESSURE] / firlen;
				if (atomic_read(&priv->trace) &
				    BAR_TRC_FILTER) {
					pr_debug("add [%2d][%5d]=>[%5d]:[%5d]\n",
						idx,
						priv->fir
							.raw[idx][BMP_PRESSURE],
						priv->fir.sum[BMP_PRESSURE],
						*pressure);
				}
			}
		}
	}
#endif

	mutex_unlock(&priv->lock);
	return err;
}

/*
 *get compensated temperature
 *unit:10 degrees centigrade
 */
static int bmp_get_temperature(struct i2c_client *client, char *buf,
			       int bufsize)
{
	struct bmp_i2c_data *obj;
	int status;
	s32 utemp = 0; /* uncompensated temperature */
	s32 temperature = 0;

	if (buf == NULL)
		return -1;

	if (client == NULL) {
		*buf = 0;
		return -2;
	}

	obj = i2c_get_clientdata(client);

	status = bmp_read_raw_temperature(client, &utemp);
	if (status != 0)
		return status;

	if (obj->sensor_type == BMP280_TYPE) { /* BMP280 */
		BMP280_S32_t v_x1_u32r = 0;
		BMP280_S32_t v_x2_u32r = 0;

		v_x1_u32r = ((((utemp >> 3) -
			       ((BMP280_S32_t)obj->bmp280_cali.dig_T1 << 1))) *
			     ((BMP280_S32_t)obj->bmp280_cali.dig_T2)) >>
			    11;
		v_x2_u32r = (((((utemp >> 4) -
				((BMP280_S32_t)obj->bmp280_cali.dig_T1)) *
			       ((utemp >> 4) -
				((BMP280_S32_t)obj->bmp280_cali.dig_T1))) >>
			      12) *
			     ((BMP280_S32_t)obj->bmp280_cali.dig_T3)) >>
			    14;

		mutex_lock(&obj->lock);
		obj->t_fine = v_x1_u32r + v_x2_u32r;
		mutex_unlock(&obj->lock);
		temperature = (obj->t_fine * 5 + 128) >> 8;
	}

	sprintf(buf, "%08x", temperature);
	if (atomic_read(&obj->trace) & BAR_TRC_IOCTL) {
		pr_debug("temperature: %d\n", temperature);
		pr_debug("temperature/100: %d\n", temperature / 100);
		pr_debug("compensated temperature value: %s\n", buf);
	}

	return status;
}

/*
 *get compensated pressure
 *unit: hectopascal(hPa)
 */
static int bmp_get_pressure(struct i2c_client *client, char *buf, int bufsize)
{
	struct bmp_i2c_data *obj;
	int status;
	s32 temperature = 0, upressure = 0, pressure = 0;
	char temp_buf[BMP_BUFSIZE];

	if (buf == NULL)
		return -1;

	if (client == NULL) {
		*buf = 0;
		return -2;
	}

	obj = i2c_get_clientdata(client);

	/* update the ambient temperature according to the given meas. period */
	/* below method will have false problem when jiffies wrap around.
	 *so replace.
	 */

	if (time_before_eq((unsigned long)(obj->last_temp_measurement +
					   obj->temp_measurement_period),
			   jiffies)) {

		status = bmp_get_temperature(client, temp_buf,
					     BMP_BUFSIZE); /* update t_fine */
		if (status != 0)
			goto exit;
		if (kstrtos32(temp_buf, 16, &temperature) != 1)
			pr_err("sscanf parsing fail\n");
	}

	status = bmp_read_raw_pressure(client, &upressure);
	if (status != 0)
		goto exit;

	if (obj->sensor_type == BMP280_TYPE) { /* BMP280 */
		BMP280_S64_t v_x1_u32r = 0;
		BMP280_S64_t v_x2_u32r = 0;
		BMP280_S64_t p = 0;

		v_x1_u32r = ((BMP280_S64_t)obj->t_fine) - 128000;
		v_x2_u32r = v_x1_u32r * v_x1_u32r *
			    (BMP280_S64_t)obj->bmp280_cali.dig_P6;
		v_x2_u32r = v_x2_u32r +
			    ((v_x1_u32r * (BMP280_S64_t)obj->bmp280_cali.dig_P5)
			     << 17);
		v_x2_u32r = v_x2_u32r +
			    (((BMP280_S64_t)obj->bmp280_cali.dig_P4) << 35);
		v_x1_u32r = ((v_x1_u32r * v_x1_u32r *
			      (BMP280_S64_t)obj->bmp280_cali.dig_P3) >>
			     8) +
			    ((v_x1_u32r * (BMP280_S64_t)obj->bmp280_cali.dig_P2)
			     << 12);
		v_x1_u32r = (((((BMP280_S64_t)1) << 47) + v_x1_u32r)) *
				    ((BMP280_S64_t)obj->bmp280_cali.dig_P1) >>
			    33;
		if (v_x1_u32r == 0)
			/* Avoid exception caused by division by zero */
			return -1;
		p = 1048576 - upressure;
		p = div64_s64(((p << 31) - v_x2_u32r) * 3125, v_x1_u32r);
		v_x1_u32r = (((BMP280_S64_t)obj->bmp280_cali.dig_P9) *
			     (p >> 13) * (p >> 13)) >>
			    25;
		v_x2_u32r = (((BMP280_S64_t)obj->bmp280_cali.dig_P8) * p) >> 19;
		p = ((p + v_x1_u32r + v_x2_u32r) >> 8) +
		    (((BMP280_S64_t)obj->bmp280_cali.dig_P7) << 4);
		pressure = (BMP280_U32_t)p / 256;
	}

	sprintf(buf, "%08x", pressure);
	if (atomic_read(&obj->trace) & BAR_TRC_IOCTL) {
		pr_debug("pressure: %d\n", pressure);
		pr_debug("pressure/100: %d\n", pressure / 100);
		pr_debug("compensated pressure value: %s\n", buf);
	}
exit:
	return status;
}

/* bmp setting initialization */
static int bmp_init_client(struct i2c_client *client)
{
	int err = 0;

	/* pr_debug("%s\n", __func__); */

	err = bmp_get_chip_type(client);
	if (err < 0) {
		pr_err("get chip type failed, err = %d\n", err);
		return err;
	}

	err = bmp_get_calibration_data(client);
	if (err < 0) {
		pr_err("get calibration data failed, err = %d\n", err);
		return err;
	}

	err = bmp_set_powermode(client, BMP_SUSPEND_MODE);
	if (err < 0) {
		pr_err("set power mode failed, err = %d\n", err);
		return err;
	}

	err = bmp_set_filter(client, BMP_FILTER_8);
	if (err < 0) {
		pr_err("set hw filter failed, err = %d\n", err);
		return err;
	}

	err = bmp_set_oversampling_p(client, BMP_OVERSAMPLING_8X);
	if (err < 0) {
		pr_err("set pressure oversampling failed, err = %d\n", err);
		return err;
	}

	err = bmp_set_oversampling_t(client, BMP_OVERSAMPLING_1X);
	if (err < 0) {
		pr_err("set temperature oversampling failed, err = %d\n", err);
		return err;
	}

	return 0;
}

static int bmp280_verify_i2c_disable_switch(struct bmp_i2c_data *obj)
{
	int err = 0;
	u8 reg_val = 0xFF;

	err = bmp_i2c_read_block(obj->client, BMP280_I2C_DISABLE_SWITCH,
				 &reg_val, 1);
	if (err < 0) {
		err = -EIO;
		pr_err("bus read failed\n");
		return err;
	}

	if (reg_val == 0x00) {
		pr_debug("bmp280 i2c interface is available\n");
		return 0; /* OK */
	}

	pr_err("verification of i2c interface is failure\n");
	return -1; /* Failure */
}

static int bmp_check_calib_param(struct bmp_i2c_data *obj)
{
	struct bmp280_calibration_data *cali = &(obj->bmp280_cali);

	/* verify that not all calibration parameters are 0 */
	if (cali->dig_T1 == 0 && cali->dig_T2 == 0 && cali->dig_T3 == 0 &&
	    cali->dig_P1 == 0 && cali->dig_P2 == 0 && cali->dig_P3 == 0 &&
	    cali->dig_P4 == 0 && cali->dig_P5 == 0 && cali->dig_P6 == 0 &&
	    cali->dig_P7 == 0 && cali->dig_P8 == 0 && cali->dig_P9 == 0) {
		pr_err("all calibration parameters are zero\n");
		return -2;
	}

	/* verify whether all the calibration parameters are within range */
	if (cali->dig_T1 < 19000 || cali->dig_T1 > 35000)
		return -3;
	else if (cali->dig_T2 < 22000 || cali->dig_T2 > 30000)
		return -4;
	else if (cali->dig_T3 < -3000 || cali->dig_T3 > -1000)
		return -5;
	else if (cali->dig_P1 < 30000 || cali->dig_P1 > 42000)
		return -6;
	else if (cali->dig_P2 < -12970 || cali->dig_P2 > -8000)
		return -7;
	else if (cali->dig_P3 < -5000 || cali->dig_P3 > 8000)
		return -8;
	else if (cali->dig_P4 < -10000 || cali->dig_P4 > 18000)
		return -9;
	else if (cali->dig_P5 < -500 || cali->dig_P5 > 1100)
		return -10;
	else if (cali->dig_P6 < -1000 || cali->dig_P6 > 1000)
		return -11;
	else if (cali->dig_P7 < -32768 || cali->dig_P7 > 32767)
		return -12;
	else if (cali->dig_P8 < -30000 || cali->dig_P8 > 10000)
		return -13;
	else if (cali->dig_P9 < -10000 || cali->dig_P9 > 30000)
		return -14;

	pr_debug("calibration parameters are OK\n");
	return 0;
}

static int bmp_check_pt(struct bmp_i2c_data *obj)
{
	int err = 0;
	int temperature = -5000;
	int pressure = -1;
	char t[BMP_BUFSIZE] = "", p[BMP_BUFSIZE] = "";

	err = bmp_set_powermode(obj->client, BMP_NORMAL_MODE);
	if (err < 0) {
		pr_err("set power mode failed, err = %d\n", err);
		return -15;
	}

	mdelay(50);

	/* check ut and t */
	bmp_get_temperature(obj->client, t, BMP_BUFSIZE);
	if (kstrtoint(t, 16, &temperature) != 1)
		pr_err("sscanf parsing fail\n");
	if (temperature <= -40 * 100 || temperature >= 85 * 100) {
		pr_err("temperature value is out of range:%d*0.01degree\n",
			temperature);
		return -16;
	}

	/* check up and p */
	bmp_get_pressure(obj->client, p, BMP_BUFSIZE);
	if (kstrtoint(p, 16, &pressure) != 1)
		pr_err("sscanf parsing fail\n");
	if (pressure <= 800 * 100 || pressure >= 1100 * 100) {
		pr_err("pressure value is out of range:%d Pa\n", pressure);
		return -17;
	}

	pr_debug("bmp280 temperature and pressure values are OK\n");
	return 0;
}

static int bmp_do_selftest(struct bmp_i2c_data *obj)
{
	int err = 0;
	/* 0: failed, 1: success */
	u8 selftest;

	err = bmp280_verify_i2c_disable_switch(obj);
	if (err) {
		selftest = 0;
		pr_err("bmp280_verify_i2c_disable_switch:err=%d\n", err);
		goto exit;
	}

	err = bmp_check_calib_param(obj);
	if (err) {
		selftest = 0;
		pr_err("bmp_check_calib_param:err=%d\n", err);
		goto exit;
	}

	err = bmp_check_pt(obj);
	if (err) {
		selftest = 0;
		pr_err("bmp_check_pt:err=%d\n", err);
		goto exit;
	}

	/* selftest is OK */
	selftest = 1;
	pr_debug("bmp280 self test is OK\n");
exit:
	return selftest;
}

static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	struct bmp_i2c_data *obj = obj_i2c_data;

	if (obj == NULL) {
		pr_err("bmp i2c data pointer is null\n");
		return 0;
	}

	return snprintf(buf, PAGE_SIZE, "%s\n", obj->sensor_name);
}

static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{
	struct bmp_i2c_data *obj = obj_i2c_data;
	char strbuf[BMP_BUFSIZE] = "";

	if (obj == NULL) {
		pr_err("bmp i2c data pointer is null\n");
		return 0;
	}

	bmp_get_pressure(obj->client, strbuf, BMP_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}

static ssize_t show_trace_value(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	struct bmp_i2c_data *obj = obj_i2c_data;

	if (obj == NULL) {
		pr_err("bmp i2c data pointer is null\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&obj->trace));
	return res;
}

static ssize_t store_trace_value(struct device_driver *ddri, const char *buf,
				 size_t count)
{
	struct bmp_i2c_data *obj = obj_i2c_data;
	int trace;

	if (obj == NULL) {
		pr_err("i2c_data obj is null\n");
		return 0;
	}

	if (sscanf(buf, "0x%x", &trace) == 1)
		atomic_set(&obj->trace, trace);
	else
		pr_err("invalid content: '%s', length = %d\n", buf,
			(int)count);

	return count;
}

static ssize_t show_status_value(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	struct bmp_i2c_data *obj = obj_i2c_data;

	if (obj == NULL) {
		pr_err("bmp i2c data pointer is null\n");
		return 0;
	}

	len += snprintf(buf + len, PAGE_SIZE - len, "CUST: %d %d (%d %d)\n",
			obj->hw.i2c_num, obj->hw.direction, obj->hw.power_id,
			obj->hw.power_vol);

	len += snprintf(buf + len, PAGE_SIZE - len, "i2c addr:%#x,ver:%s\n",
			obj->client->addr, BMP_DRIVER_VERSION);

	return len;
}

static ssize_t show_power_mode_value(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	struct bmp_i2c_data *obj = obj_i2c_data;

	if (obj == NULL) {
		pr_err("bmp i2c data pointer is null\n");
		return 0;
	}

	len += snprintf(buf + len, PAGE_SIZE - len, "%s mode\n",
			obj->power_mode == BMP_NORMAL_MODE ? "normal"
							   : "suspend");

	return len;
}

static ssize_t store_power_mode_value(struct device_driver *ddri,
				      const char *buf, size_t count)
{
	struct bmp_i2c_data *obj = obj_i2c_data;
	unsigned long power_mode;
	int err;

	if (obj == NULL) {
		pr_err("bmp i2c data pointer is null\n");
		return 0;
	}

	err = kstrtoul(buf, 10, &power_mode);

	if (err == 0) {
		err = bmp_set_powermode(
			obj->client, (enum BMP_POWERMODE_ENUM)(!!(power_mode)));
		if (err)
			return err;
		return count;
	}
	return err;
}

static ssize_t show_selftest_value(struct device_driver *ddri, char *buf)
{
	struct bmp_i2c_data *obj = obj_i2c_data;

	if (obj == NULL) {
		pr_err("bmp i2c data pointer is null\n");
		return 0;
	}

	return snprintf(buf, PAGE_SIZE, "%d\n", bmp_do_selftest(obj));
}

static DRIVER_ATTR(chipinfo, 0444, show_chipinfo_value, NULL);
static DRIVER_ATTR(sensordata, 0444, show_sensordata_value, NULL);
static DRIVER_ATTR(trace, 0644, show_trace_value,
		   store_trace_value);
static DRIVER_ATTR(status, 0444, show_status_value, NULL);
static DRIVER_ATTR(powermode, 0644, show_power_mode_value,
		   store_power_mode_value);
static DRIVER_ATTR(selftest, 0444, show_selftest_value, NULL);

static struct driver_attribute *bmp_attr_list[] = {
	&driver_attr_chipinfo,   /* chip information */
	&driver_attr_sensordata, /* dump sensor data */
	&driver_attr_trace,      /* trace log */
	&driver_attr_status,     /* cust setting */
	&driver_attr_powermode,  /* power mode */
	&driver_attr_selftest,   /* self test */
};

static int bmp_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(ARRAY_SIZE(bmp_attr_list));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, bmp_attr_list[idx]);
		if (err) {
			pr_err("driver_create_file (%s) = %d\n",
				bmp_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

static int bmp_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(ARRAY_SIZE(bmp_attr_list));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, bmp_attr_list[idx]);

	return err;
}

#ifdef CONFIG_ID_TEMPERATURE
int temperature_operate(void *self, uint32_t command, void *buff_in,
			int size_in, void *buff_out, int size_out,
			int *actualout)
{
	int err = 0;
	int value;
	struct bmp_i2c_data *priv = (struct bmp_i2c_data *)self;
	hwm_sensor_data *temperature_data;
	char buff[BMP_BUFSIZE];

	switch (command) {
	case SENSOR_DELAY:
		/* under construction */
		break;

	case SENSOR_ENABLE:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			pr_err("enable sensor parameter error\n");
			err = -EINVAL;
		} else {
			/* value:[0--->suspend, 1--->normal] */
			value = *(int *)buff_in;
			pr_debug("sensor enable/disable command: %s\n",
				value ? "enable" : "disable");

			err = bmp_set_powermode(
				priv->client,
				(enum BMP_POWERMODE_ENUM)(!!value));
			if (err)
				pr_err("set power mode failed, err = %d\n",
					err);
		}
		break;

	case SENSOR_GET_DATA:
		if ((buff_out == NULL) ||
		    (size_out < sizeof(hwm_sensor_data))) {
			pr_err("get sensor data parameter error\n");
			err = -EINVAL;
		} else {
			temperature_data = (hwm_sensor_data *)buff_out;
			err = bmp_get_temperature(priv->client, buff,
						  BMP_BUFSIZE);
			if (err) {
				pr_err("get compensated temperature value failed,err = %d\n",
					err);
				return -1;
			}
			if (kstrtoint(buff, 16, &temperature_data->values[0]) !=
			    1)
				pr_err("sscanf parsing fail\n");
			temperature_data->values[1] =
				temperature_data->values[2] = 0;
			temperature_data->status = SENSOR_STATUS_ACCURACY_HIGH;
			temperature_data->value_divide = 100;
		}
		break;

	default:
		pr_err("temperature operate function no this parameter %d\n",
			command);
		err = -1;
		break;
	}

	return err;
}
#endif /* CONFIG_ID_TEMPERATURE */

#ifdef CONFIG_PM_SLEEP
static int bmp_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bmp_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;

	if (obj == NULL) {
		pr_err("null pointer\n");
		return -EINVAL;
	}

	if (atomic_read(&obj->trace) & BAR_TRC_INFO)
		pr_debug("%s\n", __func__);

	atomic_set(&obj->suspend, 1);
	err = bmp_set_powermode(obj->client, BMP_SUSPEND_MODE);
	if (err) {
		pr_err("bmp set suspend mode failed, err = %d\n", err);
		return err;
	}
	return err;
}

static int bmp_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bmp_i2c_data *obj = i2c_get_clientdata(client);
	int err;

	if (obj == NULL) {
		pr_err("null pointer\n");
		return -EINVAL;
	}

	if (atomic_read(&obj->trace) & BAR_TRC_INFO)
		pr_debug("%s\n", __func__);

	err = bmp_init_client(obj->client);
	if (err) {
		pr_err("initialize client fail\n");
		return err;
	}

	err = bmp_set_powermode(obj->client, BMP_NORMAL_MODE);
	if (err) {
		pr_err("bmp set normal mode failed, err = %d\n", err);
		return err;
	}
#ifdef CONFIG_BMP_LOWPASS
	memset(&obj->fir, 0x00, sizeof(obj->fir));
#endif

	atomic_set(&obj->suspend, 0);
	return 0;
}
#endif

static int bmp_i2c_detect(struct i2c_client *client,
			  struct i2c_board_info *info)
{
	strlcpy(info->type, BMP_DEV_NAME, sizeof(info->type));
	return 0;
}

static int bmp_open_report_data(int open)
{
	/* should queuq work to report event if  is_report_input_direct=true */
	return 0;
}

static int bmp_enable_nodata(int en)
{
	struct bmp_i2c_data *obj = i2c_get_clientdata(obj_i2c_data->client);
	int res = 0;
	int retry = 0;
	bool power = false;

	if (en == 1)
		power = true;

	if (en == 0)
		power = false;

	for (retry = 0; retry < 3; retry++) {
		res = bmp_set_powermode(obj_i2c_data->client,
					(enum BMP_POWERMODE_ENUM)(!!power));
		if (res == 0) {
			pr_debug("bmp_set_powermode done\n");
			break;
		}
		pr_err("bmp_set_powermode fail\n");
	}
	obj->last_temp_measurement = jiffies - obj->temp_measurement_period;

	if (res != 0) {
		pr_err("bmp_set_powermode fail!\n");
		return -1;
	}
	pr_debug("bmp_set_powermode OK!\n");
	return 0;
}

static int bmp_set_delay(u64 ns)
{
	return 0;
}

static int bmp_batch(int flag, int64_t samplingPeriodNs,
		     int64_t maxBatchReportLatencyNs)
{
	return bmp_set_delay(samplingPeriodNs);
}

static int bmp_flush(void)
{
	return baro_flush_report();
}

static int bmp_get_data(int *value, int *status)
{
	char buff[BMP_BUFSIZE];
	int err = 0;

	err = bmp_get_pressure(obj_i2c_data->client, buff, BMP_BUFSIZE);
	if (err) {
		pr_err("get compensated pressure value failed, err = %d\n",
			err);
		return -1;
	}
	if (kstrtoint(buff, 16, value) != 1)
		pr_err("sscanf parsing fail\n");
	*status = SENSOR_STATUS_ACCURACY_MEDIUM;

	return 0;
}

static int bmp_factory_enable_sensor(bool enabledisable,
				     int64_t sample_periods_ms)
{
	int err = 0;

	err = bmp_enable_nodata(enabledisable == true ? 1 : 0);
	if (err) {
		pr_err("%s enable sensor failed!\n", __func__);
		return -1;
	}
	err = bmp_batch(0, sample_periods_ms * 1000000, 0);
	if (err) {
		pr_err("%s enable set batch failed!\n", __func__);
		return -1;
	}
	return 0;
}
static int bmp_factory_get_data(int32_t *data)
{
	int err = 0, status = 0;

	err = bmp_get_data(data, &status);
	if (err < 0) {
		pr_err("%s get data fail\n", __func__);
		return -1;
	}
	return 0;
}
static int bmp_factory_get_raw_data(int32_t *data)
{
	return 0;
}
static int bmp_factory_enable_calibration(void)
{
	return 0;
}
static int bmp_factory_clear_cali(void)
{
	return 0;
}
static int bmp_factory_set_cali(int32_t offset)
{
	return 0;
}
static int bmp_factory_get_cali(int32_t *offset)
{
	return 0;
}
static int bmp_factory_do_self_test(void)
{
	return 0;
}

static struct baro_factory_fops bmp_factory_fops = {
	.enable_sensor = bmp_factory_enable_sensor,
	.get_data = bmp_factory_get_data,
	.get_raw_data = bmp_factory_get_raw_data,
	.enable_calibration = bmp_factory_enable_calibration,
	.clear_cali = bmp_factory_clear_cali,
	.set_cali = bmp_factory_set_cali,
	.get_cali = bmp_factory_get_cali,
	.do_self_test = bmp_factory_do_self_test,
};

static struct baro_factory_public bmp_factory_device = {
	.gain = 1, .sensitivity = 1, .fops = &bmp_factory_fops,
};

static int bmp_i2c_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct bmp_i2c_data *obj = NULL;
	struct baro_control_path ctl = {0};
	struct baro_data_path data = {0};
#ifdef CONFIG_ID_TEMPERATURE
	struct hwmsen_object sobj_t;
#endif
	int err = 0;

	pr_debug("%s\n", __func__);

	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj) {
		err = -ENOMEM;
		goto exit;
	}

	err = get_baro_dts_func(client->dev.of_node, &obj->hw);
	if (err < 0) {
		pr_err("get cust_baro dts info fail\n");
		goto exit_init_client_failed;
	}

	obj_i2c_data = obj;
	obj->client = client;
	i2c_set_clientdata(client, obj);

	atomic_set(&obj->trace, 0);
	atomic_set(&obj->suspend, 0);
	obj->power_mode = BMP_UNDEFINED_POWERMODE;
	obj->hw_filter = BMP_UNDEFINED_FILTER;
	obj->oversampling_p = BMP_UNDEFINED_OVERSAMPLING;
	obj->oversampling_t = BMP_UNDEFINED_OVERSAMPLING;
	obj->last_temp_measurement = 0;
	obj->temp_measurement_period =
		1 * HZ; /* temperature update period:1s */
	mutex_init(&obj->lock);

#ifdef CONFIG_BMP_LOWPASS
	if (obj->hw.firlen > C_MAX_FIR_LENGTH)
		atomic_set(&obj->firlen, C_MAX_FIR_LENGTH);
	else
		atomic_set(&obj->firlen, obj->hw.firlen);

	if (atomic_read(&obj->firlen) > 0)
		atomic_set(&obj->fir_en, 1);
#endif

	err = bmp_init_client(client);
	if (err)
		goto exit_init_client_failed;

	/* err = misc_register(&bmp_device); */
	err = baro_factory_device_register(&bmp_factory_device);
	if (err) {
		pr_err("baro_factory device register failed, err = %d\n", err);
		goto exit_misc_device_register_failed;
	}

	err = bmp_create_attr(&(bmp_init_info.platform_diver_addr->driver));
	if (err) {
		pr_err("create attribute failed, err = %d\n", err);
		goto exit_create_attr_failed;
	}

	ctl.is_use_common_factory = false;
	ctl.open_report_data = bmp_open_report_data;
	ctl.enable_nodata = bmp_enable_nodata;
	ctl.set_delay = bmp_set_delay;
	ctl.batch = bmp_batch;
	ctl.flush = bmp_flush;

	ctl.is_report_input_direct = false;
	ctl.is_support_batch = obj->hw.is_batch_supported;

	err = baro_register_control_path(&ctl);
	if (err) {
		pr_err("register baro control path err\n");
		goto exit_hwmsen_attach_pressure_failed;
	}

	data.get_data = bmp_get_data;
	data.vender_div = 100;
	err = baro_register_data_path(&data);
	if (err) {
		pr_err("baro_register_data_path failed, err = %d\n", err);
		goto exit_hwmsen_attach_pressure_failed;
	}

#ifdef CONFIG_ID_TEMPERATURE
	sobj_t.self = obj;
	sobj_t.polling = 1;
	sobj_t.sensor_operate = temperature_operate;
	err = hwmsen_attach(ID_TEMPRERATURE, &sobj_t);
	if (err) {
		pr_err("hwmsen attach failed, err = %d\n", err);
		goto exit_hwmsen_attach_temperature_failed;
	}
#endif /* CONFIG_ID_TEMPERATURE */

	bmp_init_flag = 0;
	pr_debug("%s: OK\n", __func__);
	return 0;

#ifdef CONFIG_ID_TEMPERATURE
exit_hwmsen_attach_temperature_failed:
	hwmsen_detach(ID_PRESSURE);
#endif /* CONFIG_ID_TEMPERATURE */
exit_hwmsen_attach_pressure_failed:
	bmp_delete_attr(&(bmp_init_info.platform_diver_addr->driver));
exit_create_attr_failed:
/* misc_deregister(&bmp_device); */
exit_misc_device_register_failed:
exit_init_client_failed:
	kfree(obj);
exit:
	obj = NULL;
	obj_i2c_data = NULL;
	pr_err("err = %d\n", err);
	bmp_init_flag = -1;
	return err;
}

static int bmp_i2c_remove(struct i2c_client *client)
{
	int err = 0;

#ifdef CONFIG_ID_TEMPERATURE
	err = hwmsen_detach(ID_TEMPRERATURE);
	if (err)
		pr_err("hwmsen_detach ID_TEMPRERATURE failed, err = %d\n",
			err);
#endif

	err = bmp_delete_attr(&(bmp_init_info.platform_diver_addr->driver));
	if (err)
		pr_err("bmp_delete_attr failed, err = %d\n", err);

	/* misc_deregister(&bmp_device); */
	baro_factory_device_deregister(&bmp_factory_device);

	obj_i2c_data = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));

	return 0;
}

static int bmp_remove(void)
{
	/*struct baro_hw *hw = get_cust_baro(); */

	pr_debug("%s\n", __func__);
	i2c_del_driver(&bmp_i2c_driver);
	return 0;
}

static int bmp_local_init(void)
{
	if (i2c_add_driver(&bmp_i2c_driver)) {
		pr_err("add driver error\n");
		return -1;
	}
	if (-1 == bmp_init_flag)
		return -1;

	/* pr_debug("fwq loccal init---\n"); */
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id baro_of_match[] = {
	{.compatible = "mediatek,barometer"}, {},
};
#endif

#ifdef CONFIG_PM_SLEEP
static const struct dev_pm_ops bmp280_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(bmp_suspend, bmp_resume)};
#endif

static struct i2c_driver bmp_i2c_driver = {
	.driver = {

			.owner = THIS_MODULE,
			.name = BMP_DEV_NAME,
#ifdef CONFIG_PM_SLEEP
			.pm = &bmp280_pm_ops,
#endif
#ifdef CONFIG_OF
			.of_match_table = baro_of_match,
#endif
		},
	.probe = bmp_i2c_probe,
	.remove = bmp_i2c_remove,
	.detect = bmp_i2c_detect,
	.id_table = bmp_i2c_id,
};

static int __init bmp_init(void)
{
	pr_debug("%s\n", __func__);
#ifdef CONFIG_MTK_LEGACY
	i2c_register_board_info(hw.i2c_num, &bmp_i2c_info, 1);
#endif
	baro_driver_add(&bmp_init_info);

	return 0;
}

static void __exit bmp_exit(void)
{
	pr_debug("%s\n", __func__);
}
module_init(bmp_init);
module_exit(bmp_exit);

MODULE_DESCRIPTION("BMP280 I2C Driver");
MODULE_AUTHOR("deliang.tao@bosch-sensortec.com");
MODULE_VERSION(BMP_DRIVER_VERSION);
