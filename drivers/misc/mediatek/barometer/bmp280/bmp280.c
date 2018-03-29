/*
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * History: V1.0 --- [2013.03.14]Driver creation
 *          V1.1 --- [2013.07.03]Re-write I2C function to fix the bug that
 *                               i2c access error on MT6589 platform.
 *          V1.2 --- [2013.07.04]Add self test function.
 *          V1.3 --- [2013.07.04]Support new chip id 0x57 and 0x58.
 */
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/gpio.h>
#include "cust_baro.h"
#include "bmp280.h"
#include "barometer.h"

#define POWER_NONE_MACRO MT65XX_POWER_NONE


static DEFINE_MUTEX(bmp280_i2c_mutex);

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
	struct baro_hw *hw;

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

#define BAR_TAG                  "[barometer] "
#define BAR_FUN(f)               pr_err(BAR_TAG"%s\n", __func__)
#define BAR_ERR(fmt, args...) \
	pr_err(BAR_TAG"%s %d : "fmt, __func__, __LINE__, ##args)

#define BAR_LOGLEVEL 0

#if ((BAR_LOGLEVEL) >= 1)
#define BAR_LOG(fmt, args...)    pr_debug(BAR_TAG fmt, ##args)
#else
#define BAR_LOG(fmt, args...)
#endif


static struct i2c_driver bmp_i2c_driver;
static struct bmp_i2c_data *obj_i2c_data;
static const struct i2c_device_id bmp_i2c_id[] = {
	{BMP_DEV_NAME, 0},
	{}
};
struct baro_hw baro_cust;
static struct baro_hw *hw = &baro_cust;
/* For alsp driver get cust info */
struct baro_hw *get_cust_baro(void)
{
	return &baro_cust;
}

static int bmp280_local_init(void);
static int bmp280_remove(void);
static int bmp280_init_flag = -1;
static struct baro_init_info bmp280_init_info = {
	.name = "bmp280",
	.init = bmp280_local_init,
	.uninit = bmp280_remove,
};

/* I2C operation functions */
static int bmp_i2c_read_block(struct i2c_client *client, u8 addr, u8 *data, u8 len)
{
	u8 beg = addr;
	int err;
	struct i2c_msg msgs[2] = { {0}, {0} };

	mutex_lock(&bmp280_i2c_mutex);

	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = &beg;

	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = data;

	if (!client) {
		mutex_unlock(&bmp280_i2c_mutex);
		return -EINVAL;
	} else if (len > C_I2C_FIFO_SIZE) {
		BAR_ERR(" length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		mutex_unlock(&bmp280_i2c_mutex);
		return -EINVAL;
	}
	err = i2c_transfer(client->adapter, msgs, sizeof(msgs) / sizeof(msgs[0]));
	if (err != 2) {
		BAR_ERR("i2c_transfer error: (%d %p %d) %d\n", addr, data, len, err);
		err = -EIO;
	} else {
		err = 0;
	}
	mutex_unlock(&bmp280_i2c_mutex);
	return err;

}

static int bmp_i2c_write_block(struct i2c_client *client, u8 addr, u8 *data, u8 len)
{				/*because address also occupies one byte, the maximum length for write is 7 bytes */
	int err = 0, idx = 0, num = 0;
	char buf[C_I2C_FIFO_SIZE];

	mutex_lock(&bmp280_i2c_mutex);

	if (!client) {
		mutex_unlock(&bmp280_i2c_mutex);
		return -EINVAL;
	} else if (len >= C_I2C_FIFO_SIZE) {
		BAR_ERR(" length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		mutex_unlock(&bmp280_i2c_mutex);
		return -EINVAL;
	}

	num = 0;
	buf[num++] = addr;
	for (idx = 0; idx < len; idx++)
		buf[num++] = data[idx];

	err = i2c_master_send(client, buf, num);
	if (err < 0) {
		BAR_ERR("send command error!!\n");
		mutex_unlock(&bmp280_i2c_mutex);
		return -EFAULT;
	}
	mutex_unlock(&bmp280_i2c_mutex);
	return err;
}

static void bmp_power(struct baro_hw *hw, unsigned int on)
{

}

/* get chip type */
static int bmp_get_chip_type(struct i2c_client *client)
{
	int err = 0;
	u8 chip_id = 0;
	struct bmp_i2c_data *obj = i2c_get_clientdata(client);

	BAR_FUN(f);

	err = bmp_i2c_read_block(client, BMP_CHIP_ID_REG, &chip_id, 0x01);
	if (err != 0)
		return err;

	switch (chip_id) {
	case BMP280_CHIP_ID1:
	case BMP280_CHIP_ID2:
	case BMP280_CHIP_ID3:
		obj->sensor_type = BMP280_TYPE;
		strcpy(obj->sensor_name, "bmp280");
		break;
	default:
		obj->sensor_type = INVALID_TYPE;
		strcpy(obj->sensor_name, "unknown sensor");
		break;
	}

	BAR_LOG("[%s]chip id = %#x, sensor name = %s\n", __func__, chip_id, obj->sensor_name);

	if (obj->sensor_type == INVALID_TYPE) {
		BAR_ERR("unknown pressure sensor\n");
		return -1;
	}
	return 0;
}

static int bmp_get_calibration_data(struct i2c_client *client)
{
	struct bmp_i2c_data *obj = (struct bmp_i2c_data *)i2c_get_clientdata(client);
	int status = 0;

	if (obj->sensor_type == BMP280_TYPE) {
		unsigned char a_data_u8r[8];

		status = bmp_i2c_read_block(client, BMP280_CALIBRATION_DATA_START,
				       a_data_u8r, 8);
		if (status < 0)
			return status;
		obj->bmp280_cali.dig_T1 = (BMP280_U16_t)(((
			(BMP280_U16_t)((unsigned char)a_data_u8r[1])) << SHIFT_LEFT_8_POSITION) | a_data_u8r[0]);
		obj->bmp280_cali.dig_T2 = (BMP280_S16_t)(((
			(BMP280_S16_t)((signed char)a_data_u8r[3])) << SHIFT_LEFT_8_POSITION) | a_data_u8r[2]);
		obj->bmp280_cali.dig_T3 = (BMP280_S16_t)(((
			(BMP280_S16_t)((signed char)a_data_u8r[5])) << SHIFT_LEFT_8_POSITION) | a_data_u8r[4]);
		obj->bmp280_cali.dig_P1 = (BMP280_U16_t)(((
			(BMP280_U16_t)((unsigned char)a_data_u8r[7])) << SHIFT_LEFT_8_POSITION) | a_data_u8r[6]);
		status = bmp_i2c_read_block(client, BMP280_CALIBRATION_DATA_START + 8,
				       a_data_u8r, 8);
		if (status < 0)
			return status;
		obj->bmp280_cali.dig_P2 = (BMP280_S16_t)(((
			(BMP280_S16_t)((signed char)a_data_u8r[1])) << SHIFT_LEFT_8_POSITION) | a_data_u8r[0]);
		obj->bmp280_cali.dig_P3 = (BMP280_S16_t)(((
			(BMP280_S16_t)((signed char)a_data_u8r[3])) << SHIFT_LEFT_8_POSITION) | a_data_u8r[2]);
		obj->bmp280_cali.dig_P4 = (BMP280_S16_t)(((
			(BMP280_S16_t)((signed char)a_data_u8r[5])) << SHIFT_LEFT_8_POSITION) | a_data_u8r[4]);
		obj->bmp280_cali.dig_P5 = (BMP280_S16_t)(((
			(BMP280_S16_t)((signed char)a_data_u8r[7])) << SHIFT_LEFT_8_POSITION) | a_data_u8r[6]);
		status = bmp_i2c_read_block(client, BMP280_CALIBRATION_DATA_START + 16,
				       a_data_u8r, 8);
		obj->bmp280_cali.dig_P6 = (BMP280_S16_t)(((
			(BMP280_S16_t)((signed char)a_data_u8r[1])) << SHIFT_LEFT_8_POSITION) | a_data_u8r[0]);
		obj->bmp280_cali.dig_P7 = (BMP280_S16_t)(((
			(BMP280_S16_t)((signed char)a_data_u8r[3])) << SHIFT_LEFT_8_POSITION) | a_data_u8r[2]);
		obj->bmp280_cali.dig_P8 = (BMP280_S16_t)(((
			(BMP280_S16_t)((signed char)a_data_u8r[5])) << SHIFT_LEFT_8_POSITION) | a_data_u8r[4]);
		obj->bmp280_cali.dig_P9 = (BMP280_S16_t)(((
			(BMP280_S16_t)((signed char)a_data_u8r[7])) << SHIFT_LEFT_8_POSITION) | a_data_u8r[6]);
	}
	return 0;
}

static int bmp_set_powermode(struct i2c_client *client, enum BMP_POWERMODE_ENUM power_mode)
{
	struct bmp_i2c_data *obj = i2c_get_clientdata(client);
	u8 err = 0, data = 0, actual_power_mode = 0;

	BAR_LOG("[%s] power_mode = %d, old power_mode = %d\n", __func__, power_mode, obj->power_mode);

	if (power_mode == obj->power_mode)
		return 0;

	mutex_lock(&obj->lock);

	if (obj->sensor_type == BMP280_TYPE) {	/* BMP280 */
		if (power_mode == BMP_SUSPEND_MODE) {
			actual_power_mode = BMP280_SLEEP_MODE;
		} else if (power_mode == BMP_NORMAL_MODE) {
			actual_power_mode = BMP280_NORMAL_MODE;
		} else {
			err = -EINVAL;
			BAR_ERR("invalid power mode = %d\n", power_mode);
			mutex_unlock(&obj->lock);
			return err;
		}
		err = bmp_i2c_read_block(client, BMP280_CTRLMEAS_REG_MODE__REG, &data, 1);
		data = BMP_SET_BITSLICE(data, BMP280_CTRLMEAS_REG_MODE, actual_power_mode);
		err += bmp_i2c_write_block(client, BMP280_CTRLMEAS_REG_MODE__REG, &data, 1);
	}

	if (err < 0)
		BAR_ERR("set power mode failed, err = %d, sensor name = %s\n", err, obj->sensor_name);
	else
		obj->power_mode = power_mode;

	mutex_unlock(&obj->lock);
	return 0;
}

static int bmp_set_filter(struct i2c_client *client, enum BMP_FILTER_ENUM filter)
{
	struct bmp_i2c_data *obj = i2c_get_clientdata(client);
	u8 err = 0, data = 0, actual_filter = 0;

	BAR_LOG("[%s] hw filter = %d, old hw filter = %d\n", __func__, filter, obj->hw_filter);

	if (filter == obj->hw_filter)
		return 0;

	mutex_lock(&obj->lock);

	if (obj->sensor_type == BMP280_TYPE) {	/* BMP280 */
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
			BAR_ERR("invalid hw filter = %d\n", filter);
			mutex_unlock(&obj->lock);
			return err;
		}
		err = bmp_i2c_read_block(client, BMP280_CONFIG_REG_FILTER__REG, &data, 1);
		data = BMP_SET_BITSLICE(data, BMP280_CONFIG_REG_FILTER, actual_filter);
		err += bmp_i2c_write_block(client, BMP280_CONFIG_REG_FILTER__REG, &data, 1);
	}

	if (err < 0)
		BAR_ERR("set hw filter failed, err = %d, sensor name = %s\n", err, obj->sensor_name);
	else
		obj->hw_filter = filter;

	mutex_unlock(&obj->lock);
	return err;
}

static int bmp_set_oversampling_p(struct i2c_client *client, enum BMP_OVERSAMPLING_ENUM oversampling_p)
{
	struct bmp_i2c_data *obj = i2c_get_clientdata(client);
	u8 err = 0, data = 0, actual_oversampling_p = 0;

	BAR_LOG("[%s] oversampling_p = %d, old oversampling_p = %d\n", __func__, oversampling_p, obj->oversampling_p);

	if (oversampling_p == obj->oversampling_p)
		return 0;

	mutex_lock(&obj->lock);

	if (obj->sensor_type == BMP280_TYPE) {	/* BMP280 */
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
			BAR_ERR("invalid oversampling_p = %d\n", oversampling_p);
			mutex_unlock(&obj->lock);
			return err;
		}
		err = bmp_i2c_read_block(client, BMP280_CTRLMEAS_REG_OSRSP__REG, &data, 1);
		data = BMP_SET_BITSLICE(data, BMP280_CTRLMEAS_REG_OSRSP, actual_oversampling_p);
		err += bmp_i2c_write_block(client, BMP280_CTRLMEAS_REG_OSRSP__REG, &data, 1);
	}

	if (err < 0)
		BAR_ERR("set pressure oversampling failed, err = %d," "sensor name = %s\n", err, obj->sensor_name);
	else
		obj->oversampling_p = oversampling_p;

	mutex_unlock(&obj->lock);
	return err;
}

static int bmp_set_oversampling_t(struct i2c_client *client, enum BMP_OVERSAMPLING_ENUM oversampling_t)
{
	struct bmp_i2c_data *obj = i2c_get_clientdata(client);
	u8 err = 0, data = 0, actual_oversampling_t = 0;

	BAR_LOG("[%s] oversampling_t = %d, old oversampling_t = %d\n", __func__, oversampling_t, obj->oversampling_t);

	if (oversampling_t == obj->oversampling_t)
		return 0;

	mutex_lock(&obj->lock);

	if (obj->sensor_type == BMP280_TYPE) {	/* BMP280 */
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
			BAR_ERR("invalid oversampling_t = %d\n", oversampling_t);
			mutex_unlock(&obj->lock);
			return err;
		}
		err = bmp_i2c_read_block(client, BMP280_CTRLMEAS_REG_OSRST__REG, &data, 1);
		data = BMP_SET_BITSLICE(data, BMP280_CTRLMEAS_REG_OSRST, actual_oversampling_t);
		err += bmp_i2c_write_block(client, BMP280_CTRLMEAS_REG_OSRST__REG, &data, 1);
	}

	if (err < 0)
		BAR_ERR("set temperature oversampling failed, err = %d," "sensor name = %s\n", err, obj->sensor_name);
	else
		obj->oversampling_t = oversampling_t;

	mutex_unlock(&obj->lock);
	return err;
}

static int bmp_read_raw_temperature(struct i2c_client *client, s32 *temperature)
{
	struct bmp_i2c_data *obj = i2c_get_clientdata(client);
	s32 err = 0;

	if (NULL == client) {
		err = -EINVAL;
		return err;
	}

	mutex_lock(&obj->lock);

	if (obj->sensor_type == BMP280_TYPE) {	/* BMP280 */
		unsigned char a_data_u8r[3] = { 0 };

		err = bmp_i2c_read_block(client, BMP280_TEMPERATURE_MSB_REG, a_data_u8r, 3);
		if (err < 0) {
			BAR_ERR("read raw temperature failed, err = %d\n", err);
			mutex_unlock(&obj->lock);
			return err;
		}
		*temperature = (BMP280_S32_t) ((((BMP280_U32_t) (a_data_u8r[0])) << SHIFT_LEFT_12_POSITION) |
					       (((BMP280_U32_t) (a_data_u8r[1])) << SHIFT_LEFT_4_POSITION) |
					       ((BMP280_U32_t) a_data_u8r[2] >> SHIFT_RIGHT_4_POSITION)
		    );
	}
	obj->last_temp_measurement = jiffies;
	mutex_unlock(&obj->lock);

	return err;
}

static int bmp_read_raw_pressure(struct i2c_client *client, s32 *pressure)
{
	struct bmp_i2c_data *priv = i2c_get_clientdata(client);
	s32 err = 0;

	if (NULL == client) {
		err = -EINVAL;
		return err;
	}

	mutex_lock(&priv->lock);

	if (priv->sensor_type == BMP280_TYPE) {	/* BMP280 */
		unsigned char a_data_u8r[3] = { 0 };

		err = bmp_i2c_read_block(client, BMP280_PRESSURE_MSB_REG, a_data_u8r, 3);
		if (err < 0) {
			BAR_ERR("read raw pressure failed, err = %d\n", err);
			mutex_unlock(&priv->lock);
			return err;
		}
		*pressure = (BMP280_S32_t) ((((BMP280_U32_t) (a_data_u8r[0])) << SHIFT_LEFT_12_POSITION) |
					    (((BMP280_U32_t) (a_data_u8r[1])) << SHIFT_LEFT_4_POSITION) |
					    ((BMP280_U32_t) a_data_u8r[2] >> SHIFT_RIGHT_4_POSITION)
		    );
	}
#ifdef CONFIG_BMP_LOWPASS
/*
*Example: firlen = 16, filter buffer = [0] ... [15],
*when 17th data come, replace [0] with this new data.
*Then, average this filter buffer and report average value to upper layer.
*/
	if (atomic_read(&priv->filter)) {
		if (atomic_read(&priv->fir_en) && !atomic_read(&priv->suspend)) {
			int idx = 0, firlen = atomic_read(&priv->firlen);

			if (priv->fir.num < firlen) {
				priv->fir.raw[priv->fir.num][BMP_PRESSURE] = *pressure;
				priv->fir.sum[BMP_PRESSURE] += *pressure;
				if (atomic_read(&priv->trace) & BAR_TRC_FILTER)
					BAR_LOG("add [%2d] [%5d] => [%5d]\n",
						priv->fir.num,
						priv->fir.raw
						[priv->fir.num][BMP_PRESSURE], priv->fir.sum[BMP_PRESSURE]);
				priv->fir.num++;
				priv->fir.idx++;
			} else {
				idx = priv->fir.idx % firlen;
				priv->fir.sum[BMP_PRESSURE] -= priv->fir.raw[idx][BMP_PRESSURE];
				priv->fir.raw[idx][BMP_PRESSURE] = *pressure;
				priv->fir.sum[BMP_PRESSURE] += *pressure;
				priv->fir.idx++;
				*pressure = priv->fir.sum[BMP_PRESSURE] / firlen;
				if (atomic_read(&priv->trace) & BAR_TRC_FILTER)
					BAR_LOG("add [%2d][%5d]=>[%5d]:[%5d]\n",
						idx,
						priv->fir.raw[idx][BMP_PRESSURE],
						priv->fir.sum[BMP_PRESSURE], *pressure);
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
static int bmp_get_temperature(struct i2c_client *client, char *buf, int bufsize)
{
	struct bmp_i2c_data *obj = i2c_get_clientdata(client);
	int status;
	s32 utemp = 0;		/* uncompensated temperature */
	s32 temperature = 0;

	if (NULL == buf)
		return -1;

	if (NULL == client) {
		*buf = 0;
		return -2;
	}

	status = bmp_read_raw_temperature(client, &utemp);
	if (status != 0)
		return status;

	if (obj->sensor_type == BMP280_TYPE) {	/* BMP280 */
		BMP280_S32_t v_x1_u32r = 0;
		BMP280_S32_t v_x2_u32r = 0;

		v_x1_u32r = ((((utemp >> 3) - ((BMP280_S32_t)
					       obj->bmp280_cali.dig_T1 << 1))) *
			     ((BMP280_S32_t) obj->bmp280_cali.dig_T2)) >> 11;
		v_x2_u32r = (((((utemp >> 4) - ((BMP280_S32_t) obj->bmp280_cali.dig_T1))
			       * ((utemp >> 4) - ((BMP280_S32_t) obj->bmp280_cali.dig_T1))
			      ) >> 12) * ((BMP280_S32_t) obj->bmp280_cali.dig_T3)) >> 14;

		mutex_lock(&obj->lock);
		obj->t_fine = v_x1_u32r + v_x2_u32r;
		mutex_unlock(&obj->lock);
		temperature = (obj->t_fine * 5 + 128) >> 8;
	}

	sprintf(buf, "%08x", temperature);
	if (atomic_read(&obj->trace) & BAR_TRC_IOCTL)
		BAR_LOG("compensated temperature value: %s\n", buf);

	return status;
}

/*
*get compensated pressure
*unit: hectopascal(hPa)
*/
static int bmp_get_pressure(struct i2c_client *client, char *buf, int bufsize)
{
	struct bmp_i2c_data *obj = i2c_get_clientdata(client);
	int status = 0, err = 0;
	s32 temperature = 0, upressure = 0, pressure = 0;
	char temp_buf[BMP_BUFSIZE];

	if (NULL == buf)
		return -1;

	if (NULL == client) {
		*buf = 0;
		return -2;
	}

	/* update the ambient temperature according to the given meas. period */
	if (time_after(jiffies, obj->last_temp_measurement + obj->temp_measurement_period)) {
		status = bmp_get_temperature(client, temp_buf, BMP_BUFSIZE);	/* update t_fine */
		if (status != 0)
			goto exit;
		err = kstrtoint(temp_buf, 16, &temperature);
		if (err)
			goto exit;
	}

	status = bmp_read_raw_pressure(client, &upressure);
	if (status != 0)
		goto exit;

	if (obj->sensor_type == BMP280_TYPE) {	/* BMP280 */
		BMP280_S64_t v_x1_u32r = 0;
		BMP280_S64_t v_x2_u32r = 0;
		BMP280_S64_t p = 0;

		v_x1_u32r = ((BMP280_S64_t) obj->t_fine) - 128000;
		v_x2_u32r = v_x1_u32r * v_x1_u32r * (BMP280_S64_t) obj->bmp280_cali.dig_P6;
		v_x2_u32r = v_x2_u32r + ((v_x1_u32r * (BMP280_S64_t) obj->bmp280_cali.dig_P5) << 17);
		v_x2_u32r = v_x2_u32r + (((BMP280_S64_t) obj->bmp280_cali.dig_P4) << 35);
		v_x1_u32r = ((v_x1_u32r * v_x1_u32r *
			      (BMP280_S64_t) obj->bmp280_cali.dig_P3) >> 8) +
		    ((v_x1_u32r * (BMP280_S64_t) obj->bmp280_cali.dig_P2)
		     << 12);
		v_x1_u32r = (((((BMP280_S64_t) 1) << 47) + v_x1_u32r)) * ((BMP280_S64_t) obj->bmp280_cali.dig_P1) >> 33;
		if (v_x1_u32r == 0)
			/* Avoid exception caused by division by zero */
			return -1;
		p = 1048576 - upressure;
		p = div64_s64(((p << 31) - v_x2_u32r) * 3125, v_x1_u32r);
		v_x1_u32r = (((BMP280_S64_t) obj->bmp280_cali.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
		v_x2_u32r = (((BMP280_S64_t) obj->bmp280_cali.dig_P8) * p) >> 19;
		p = ((p + v_x1_u32r + v_x2_u32r) >> 8) + (((BMP280_S64_t) obj->bmp280_cali.dig_P7) << 4);
		pressure = (BMP280_U32_t) p / 256;
	}

	sprintf(buf, "%08x", pressure);
	if (atomic_read(&obj->trace) & BAR_TRC_IOCTL)
		BAR_LOG("compensated pressure value: %s\n", buf);
 exit:
	return status;
}

/* bmp setting initialization */
static int bmp_init_client(struct i2c_client *client)
{
	int err = 0;

	BAR_FUN();

	err = bmp_get_chip_type(client);
	if (err < 0) {
		BAR_ERR("get chip type failed, err = %d\n", err);
		return err;
	}

	err = bmp_get_calibration_data(client);
	if (err < 0) {
		BAR_ERR("get calibration data failed, err = %d\n", err);
		return err;
	}

	err = bmp_set_powermode(client, BMP_SUSPEND_MODE);
	if (err < 0) {
		BAR_ERR("set power mode failed, err = %d\n", err);
		return err;
	}

	err = bmp_set_filter(client, BMP_FILTER_8);
	if (err < 0) {
		BAR_ERR("set hw filter failed, err = %d\n", err);
		return err;
	}

	err = bmp_set_oversampling_p(client, BMP_OVERSAMPLING_8X);
	if (err < 0) {
		BAR_ERR("set pressure oversampling failed, err = %d\n", err);
		return err;
	}

	err = bmp_set_oversampling_t(client, BMP_OVERSAMPLING_1X);
	if (err < 0) {
		BAR_ERR("set temperature oversampling failed, err = %d\n", err);
		return err;
	}

	return 0;
}

static int bmp280_verify_i2c_disable_switch(struct bmp_i2c_data *obj)
{
	int err = 0;
	u8 reg_val = 0xFF;

	err = bmp_i2c_read_block(obj->client, BMP280_I2C_DISABLE_SWITCH, &reg_val, 1);
	if (err < 0) {
		err = -EIO;
		BAR_ERR("bus read failed\n");
		return err;
	}

	if (reg_val == 0x00) {
		BAR_LOG("bmp280 i2c interface is available\n");
		return 0;	/* OK */
	}

	BAR_ERR("verification of i2c interface is failure\n");
	return -1;		/* Failure */
}

static int bmp_check_calib_param(struct bmp_i2c_data *obj)
{
	struct bmp280_calibration_data *cali = &(obj->bmp280_cali);

	/* verify that not all calibration parameters are 0 */
	if (cali->dig_T1 == 0 && cali->dig_T2 == 0 && cali->dig_T3 == 0
	    && cali->dig_P1 == 0 && cali->dig_P2 == 0
	    && cali->dig_P3 == 0 && cali->dig_P4 == 0
	    && cali->dig_P5 == 0 && cali->dig_P6 == 0 && cali->dig_P7 == 0 && cali->dig_P8 == 0 && cali->dig_P9 == 0) {
		BAR_ERR("all calibration parameters are zero\n");
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

	BAR_LOG("calibration parameters are OK\n");
	return 0;
}

static int bmp_check_pt(struct bmp_i2c_data *obj)
{
	int err = 0;
	int temperature;
	int pressure;
	char t[BMP_BUFSIZE] = "", p[BMP_BUFSIZE] = "";

	err = bmp_set_powermode(obj->client, BMP_NORMAL_MODE);
	if (err < 0) {
		BAR_ERR("set power mode failed, err = %d\n", err);
		return -15;
	}

	mdelay(50);

	/* check ut and t */
	bmp_get_temperature(obj->client, t, BMP_BUFSIZE);
	err = kstrtoint(t, 16, &temperature);
	if (err == 0)
		if (temperature <= 0 || temperature >= 40 * 100) {
			BAR_ERR("temperature value is out of range:%d*0.01degree\n", temperature);
			return -16;
		}

	/* check up and p */
	bmp_get_pressure(obj->client, p, BMP_BUFSIZE);
	err = kstrtoint(p, 16, &pressure);
	if (err == 0)
		if (pressure <= 900 * 100 || pressure >= 1100 * 100) {
			BAR_ERR("pressure value is out of range:%d Pa\n", pressure);
			return -17;
		}

	BAR_LOG("bmp280 temperature and pressure values are OK\n");
	return 0;
}

static int bmp_do_selftest(struct bmp_i2c_data *obj)
{
	int err = 0;
	/* 0: failed, 1: success */
	u8 selftest = 0;

	err = bmp280_verify_i2c_disable_switch(obj);
	if (err) {
		selftest = 0;
		BAR_ERR("bmp280_verify_i2c_disable_switch:err=%d\n", err);
		goto exit;
	}

	err = bmp_check_calib_param(obj);
	if (err) {
		selftest = 0;
		BAR_ERR("bmp_check_calib_param:err=%d\n", err);
		goto exit;
	}

	err = bmp_check_pt(obj);
	if (err) {
		selftest = 0;
		BAR_ERR("bmp_check_pt:err=%d\n", err);
		goto exit;
	}

	/* selftest is OK */
	selftest = 1;
	BAR_LOG("bmp280 self test is OK\n");
 exit:
	return selftest;
}

static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	struct bmp_i2c_data *obj = obj_i2c_data;

	if (NULL == obj) {
		BAR_ERR("bmp i2c data pointer is null\n");
		return 0;
	}

	return snprintf(buf, PAGE_SIZE, "%s\n", obj->sensor_name);
}

static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{
	struct bmp_i2c_data *obj = obj_i2c_data;
	char strbuf[BMP_BUFSIZE] = "";

	if (NULL == obj) {
		BAR_ERR("bmp i2c data pointer is null\n");
		return 0;
	}

	bmp_get_pressure(obj->client, strbuf, BMP_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}

static ssize_t show_trace_value(struct device_driver *ddri, char *buf)
{
	ssize_t res = 0;
	struct bmp_i2c_data *obj = obj_i2c_data;

	if (obj == NULL) {
		BAR_ERR("bmp i2c data pointer is null\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&obj->trace));
	return res;
}

static ssize_t store_trace_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct bmp_i2c_data *obj = obj_i2c_data;
	int trace = 0, err = 0;

	if (obj == NULL) {
		BAR_ERR("i2c_data obj is null\n");
		return 0;
	}
	err = kstrtoint(buf, 10, &trace);
	if (err == 0)
		atomic_set(&obj->trace, trace);
	else
		BAR_ERR("invalid content: '%s', length = %d\n", buf, (int)count);

	return count;
}

static ssize_t show_status_value(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	struct bmp_i2c_data *obj = obj_i2c_data;

	if (obj == NULL) {
		BAR_ERR("bmp i2c data pointer is null\n");
		return 0;
	}

	if (obj->hw)
		len += snprintf(buf + len, PAGE_SIZE - len, "CUST: %d %d (%d %d)\n",
				obj->hw->i2c_num, obj->hw->direction, obj->hw->power_id, obj->hw->power_vol);
	else
		len += snprintf(buf + len, PAGE_SIZE - len, "CUST: NULL\n");

	len += snprintf(buf + len, PAGE_SIZE - len, "i2c addr:%#x,ver:%s\n", obj->client->addr, BMP_DRIVER_VERSION);

	return len;
}

static ssize_t show_power_mode_value(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	struct bmp_i2c_data *obj = obj_i2c_data;

	if (obj == NULL) {
		BAR_ERR("bmp i2c data pointer is null\n");
		return 0;
	}

	len += snprintf(buf + len, PAGE_SIZE - len, "%s mode\n",
			obj->power_mode == BMP_NORMAL_MODE ? "normal" : "suspend");

	return len;
}

static ssize_t store_power_mode_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct bmp_i2c_data *obj = obj_i2c_data;
	unsigned long power_mode = 0;
	int err = 0;

	if (obj == NULL) {
		BAR_ERR("bmp i2c data pointer is null\n");
		return 0;
	}

	err = kstrtoul(buf, 10, &power_mode);
	if (err == 0) {
		err = bmp_set_powermode(obj->client, (enum BMP_POWERMODE_ENUM)(!!(power_mode)));
		if (err)
			return err;
		return count;
	}
	return err;
}

static ssize_t show_selftest_value(struct device_driver *ddri, char *buf)
{
	struct bmp_i2c_data *obj = obj_i2c_data;

	if (NULL == obj) {
		BAR_ERR("bmp i2c data pointer is null\n");
		return 0;
	}

	return snprintf(buf, PAGE_SIZE, "%d\n", bmp_do_selftest(obj));
}

static DRIVER_ATTR(chipinfo, S_IRUGO, show_chipinfo_value, NULL);
static DRIVER_ATTR(sensordata, S_IRUGO, show_sensordata_value, NULL);
static DRIVER_ATTR(trace, S_IWUSR | S_IRUGO, show_trace_value, store_trace_value);
static DRIVER_ATTR(status, S_IRUGO, show_status_value, NULL);
static DRIVER_ATTR(powermode, S_IWUSR | S_IRUGO, show_power_mode_value, store_power_mode_value);
static DRIVER_ATTR(selftest, S_IRUGO, show_selftest_value, NULL);

static struct driver_attribute *bmp_attr_list[] = {
	&driver_attr_chipinfo,	/* chip information */
	&driver_attr_sensordata,	/* dump sensor data */
	&driver_attr_trace,	/* trace log */
	&driver_attr_status,	/* cust setting */
	&driver_attr_powermode,	/* power mode */
	&driver_attr_selftest,	/* self test */
};

static int bmp_create_attr(struct device_driver *driver)
{
	int idx = 0, err = 0;
	int num = (int)(sizeof(bmp_attr_list) / sizeof(bmp_attr_list[0]));

	if (NULL == driver)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, bmp_attr_list[idx]);
		if (err) {
			BAR_ERR("driver_create_file (%s) = %d\n", bmp_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

static int bmp_delete_attr(struct device_driver *driver)
{
	int idx = 0, err = 0;
	int num = (int)(sizeof(bmp_attr_list) / sizeof(bmp_attr_list[0]));

	if (NULL == driver)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, bmp_attr_list[idx]);

	return err;
}

static int bmp_open(struct inode *inode, struct file *file)
{
	file->private_data = obj_i2c_data;

	if (file->private_data == NULL) {
		BAR_ERR("null pointer\n");
		return -EINVAL;
	}
	return nonseekable_open(inode, file);
}

static int bmp_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static long bmp_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct bmp_i2c_data *obj = (struct bmp_i2c_data *)file->private_data;
	struct i2c_client *client = obj->client;
	char strbuf[BMP_BUFSIZE];
	u32 dat = 0;
	void __user *data;
	int err = 0;

	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));

	if (err) {
		BAR_ERR("access error: %08X, (%2d, %2d)\n", cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
		return -EFAULT;
	}

	switch (cmd) {
	case BAROMETER_IOCTL_INIT:
		bmp_init_client(client);
		err = bmp_set_powermode(client, BMP_NORMAL_MODE);
		if (err < 0) {
			err = -EFAULT;
			break;
		}
		break;

	case BAROMETER_IOCTL_READ_CHIPINFO:
		data = (void __user *)arg;
		if (NULL == data) {
			err = -EINVAL;
			break;
		}
		strcpy(strbuf, obj->sensor_name);
		if (copy_to_user(data, strbuf, strlen(strbuf) + 1)) {
			err = -EFAULT;
			break;
		}
		break;

	case BAROMETER_GET_PRESS_DATA:
		data = (void __user *)arg;
		if (NULL == data) {
			err = -EINVAL;
			break;
		}

		bmp_get_pressure(client, strbuf, BMP_BUFSIZE);
		err = kstrtoint(strbuf, 16, &dat);
		if (err == 0)
			if (copy_to_user(data, &dat, sizeof(dat))) {
				err = -EFAULT;
				break;
			}
		break;

	case BAROMETER_GET_TEMP_DATA:
		data = (void __user *)arg;
		if (NULL == data) {
			err = -EINVAL;
			break;
		}
		bmp_get_temperature(client, strbuf, BMP_BUFSIZE);
		err = kstrtoint(strbuf, 16, &dat);
		if (err == 0)
			if (copy_to_user(data, &dat, sizeof(dat))) {
				err = -EFAULT;
				break;
			}
		break;

	default:
		BAR_ERR("unknown IOCTL: 0x%08x\n", cmd);
		err = -ENOIOCTLCMD;
		break;
	}

	return err;
}

static const struct file_operations bmp_fops = {
	.owner = THIS_MODULE,
	.open = bmp_open,
	.release = bmp_release,
	.unlocked_ioctl = bmp_unlocked_ioctl,
};

static struct miscdevice bmp_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "barometer",
	.fops = &bmp_fops,
};

static int bmp_suspend(struct i2c_client *client, pm_message_t msg)
{
	struct bmp_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;

	BAR_FUN();

	if (msg.event == PM_EVENT_SUSPEND) {
		if (NULL == obj) {
			BAR_ERR("null pointer\n");
			return -EINVAL;
		}

		atomic_set(&obj->suspend, 1);
		err = bmp_set_powermode(obj->client, BMP_SUSPEND_MODE);
		if (err) {
			BAR_ERR("bmp set suspend mode failed, err = %d\n", err);
			return err;
		}
	}
	return err;
}

static int bmp_resume(struct i2c_client *client)
{
	struct bmp_i2c_data *obj = i2c_get_clientdata(client);
	int err = 0;

	BAR_FUN();

	if (NULL == obj) {
		BAR_ERR("null pointer\n");
		return -EINVAL;
	}

	err = bmp_init_client(obj->client);
	if (err) {
		BAR_ERR("initialize client fail\n");
		return err;
	}

#ifdef CONFIG_BMP_LOWPASS
	memset(&obj->fir, 0x00, sizeof(obj->fir));
#endif

	atomic_set(&obj->suspend, 0);

	return 0;
}

static int bmp_i2c_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	strcpy(info->type, BMP_DEV_NAME);
	return 0;
}

static int bmp280_open_report_data(int open)
{
	return 0;
}

static int bmp280_enable_nodata(int en)
{
	int res = 0;
	int retry = 0;
	bool power = false;

	if (1 == en)
		power = true;
	if (0 == en)
		power = false;

	for (retry = 0; retry < 3; retry++) {
		res = bmp_set_powermode(obj_i2c_data->client, (enum BMP_POWERMODE_ENUM)(!!power));
		if (res == 0) {
			BAR_LOG("bmp_set_powermode done\n");
			break;
		}
		BAR_ERR("bmp_set_powermode fail\n");
	}

	if (res != 0) {
		BAR_ERR("bmp_set_powermode fail!\n");
		return -1;
	}
	BAR_LOG("bmp_set_powermode OK!\n");
	return 0;
}

static int bmp280_set_delay(u64 ns)
{
	return 0;
}

static int bmp280_get_data(int *value, int *status)
{
	char buff[BMP_BUFSIZE];
	int err = 0;

	err = bmp_get_pressure(obj_i2c_data->client, buff, BMP_BUFSIZE);
	if (err) {
		BAR_ERR("get compensated pressure value failed," "err = %d\n", err);
		return -1;
	}
	err = kstrtoint(buff, 16, value);
	if (err == 0)
		*status = SENSOR_STATUS_ACCURACY_MEDIUM;

	return 0;
}

static int bmp_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct bmp_i2c_data *obj;
	struct baro_control_path ctl = { 0 };
	struct baro_data_path data = { 0 };
	int err = 0;

	BAR_FUN();

	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj) {
		err = -ENOMEM;
		goto exit;
	}

	obj->hw = hw;
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
	obj->temp_measurement_period = 1 * HZ;	/* temperature update period:1s */
	mutex_init(&obj->lock);

#ifdef CONFIG_BMP_LOWPASS
	if (obj->hw->firlen > C_MAX_FIR_LENGTH)
		atomic_set(&obj->firlen, C_MAX_FIR_LENGTH);
	else
		atomic_set(&obj->firlen, obj->hw->firlen);

	if (atomic_read(&obj->firlen) > 0)
		atomic_set(&obj->fir_en, 1);
#endif

	err = bmp_init_client(client);
	if (err)
		goto exit_init_client_failed;

	err = misc_register(&bmp_device);
	if (err) {
		BAR_ERR("misc device register failed, err = %d\n", err);
		goto exit_misc_device_register_failed;
	}

	ctl.is_use_common_factory = false;
	err = bmp_create_attr(&(bmp280_init_info.platform_diver_addr->driver));
	if (err) {
		BAR_ERR("create attribute failed, err = %d\n", err);
		goto exit_create_attr_failed;
	}

	ctl.open_report_data = bmp280_open_report_data;
	ctl.enable_nodata = bmp280_enable_nodata;
	ctl.set_delay = bmp280_set_delay;
	ctl.is_report_input_direct = false;
	ctl.is_support_batch = obj->hw->is_batch_supported;

	err = baro_register_control_path(&ctl);
	if (err) {
		BAR_ERR("register baro control path err\n");
		goto exit_hwmsen_attach_pressure_failed;
	}

	data.get_data = bmp280_get_data;
	data.vender_div = 100;
	err = baro_register_data_path(&data);
	if (err) {
		BAR_ERR("baro_register_data_path failed, err = %d\n", err);
		goto exit_hwmsen_attach_pressure_failed;
	}
	err = batch_register_support_info(ID_PRESSURE, obj->hw->is_batch_supported, data.vender_div, 0);
	if (err) {
		BAR_ERR("register baro batch support err = %d\n", err);
		goto exit_hwmsen_attach_pressure_failed;
	}

	bmp280_init_flag = 0;
	BAR_LOG("%s: OK\n", __func__);
	return 0;

exit_hwmsen_attach_pressure_failed:
	bmp_delete_attr(&(bmp280_init_info.platform_diver_addr->driver));
exit_create_attr_failed:
	misc_deregister(&bmp_device);
exit_misc_device_register_failed:
exit_init_client_failed:
	kfree(obj);
exit:
	BAR_ERR("err = %d\n", err);
	bmp280_init_flag = -1;
	return err;
}

static int bmp_i2c_remove(struct i2c_client *client)
{
	int err = 0;

	err = bmp_delete_attr(&(bmp280_init_info.platform_diver_addr->driver));
	if (err)
		BAR_ERR("bmp_delete_attr failed, err = %d\n", err);

	err = misc_deregister(&bmp_device);
	if (err)
		BAR_ERR("misc_deregister failed, err = %d\n", err);

	obj_i2c_data = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));

	return 0;
}
#ifdef CONFIG_OF
static const struct of_device_id baro_of_match[] = {
	{.compatible = "mediatek,barometer"},
	{},
};
#endif

static struct i2c_driver bmp_i2c_driver = {
	.driver = {
		.name = BMP_DEV_NAME,
#ifdef CONFIG_OF
		.of_match_table = baro_of_match,
#endif
	},
	.probe = bmp_i2c_probe,
	.remove = bmp_i2c_remove,
	.detect = bmp_i2c_detect,
	.suspend = bmp_suspend,
	.resume = bmp_resume,
	.id_table = bmp_i2c_id,
};

static int bmp280_remove(void)
{
	struct baro_hw *hw = hw;

	BAR_FUN();
	bmp_power(hw, 0);
	i2c_del_driver(&bmp_i2c_driver);
	return 0;
}

static int bmp280_local_init(void)
{
	struct baro_hw *hw = hw;

	bmp_power(hw, 1);
	if (i2c_add_driver(&bmp_i2c_driver)) {
		BAR_ERR("add driver error\n");
		return -1;
	}
	if (-1 == bmp280_init_flag)
		return -1;
	return 0;
}

static int __init bmp_init(void)
{
	const char *name = "mediatek,bmp280";

	hw =   get_baro_dts_func(name, hw);
	if (!hw)
		BAR_ERR("get dts info fail\n");
	baro_driver_add(&bmp280_init_info);
	return 0;
}

static void __exit bmp_exit(void)
{
	BAR_FUN();

}

module_init(bmp_init);
module_exit(bmp_exit);

MODULE_LICENSE("GPLv2");
MODULE_DESCRIPTION("BMP280 I2C Driver");
MODULE_AUTHOR("hongxu.zhao@mediatek.com");
MODULE_VERSION(BMP_DRIVER_VERSION);
