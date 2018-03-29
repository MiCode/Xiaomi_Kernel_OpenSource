/* BOSCH Pressure Sensor Driver
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * History: V1.0 --- [2013.02.18]Driver creation
 *          V1.1 --- [2013.03.14]Instead late_resume, use resume to make sure
 *                               driver resume is ealier than processes resume.
 *          V1.2 --- [2013.03.26]Re-write i2c function to fix the bug that
 *                               i2c access error on MT6589 platform.
 */

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>
#include <linux/module.h>
#include <linux/math64.h>

#include <cust_baro.h>
#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include "bmp180.h"
#include <linux/hwmsen_helper.h>

#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>

#define POWER_NONE_MACRO MT65XX_POWER_NONE
#define CONFIG_ID_TEMPERATURE 1

static DEFINE_MUTEX(bmp180_i2c_mutex);
static DEFINE_MUTEX(bmp180_op_mutex);

/* sensor type */
enum SENSOR_TYPE_ENUM {
	BMP180_TYPE = 0x0,

	INVALID_TYPE = 0xff
};

/* power mode */
enum BMP_POWERMODE_ENUM {
	BMP_SUSPEND_MODE = 0x0,
	BMP_NORMAL_MODE,

	BMP_UNDEFINED_POWERMODE = 0xff
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

/* bmp180 calibration */
struct bmp180_calibration_data {
	s16 AC1, AC2, AC3;
	u16 AC4, AC5, AC6;
	s16 B1, B2;
	s16 MB, MC, MD;
};

/* bmp i2c client data */
struct bmp_i2c_data {
	struct i2c_client *client;
	struct baro_hw *hw;

	/* sensor info */
	u8 sensor_name[MAX_SENSOR_NAME];
	enum SENSOR_TYPE_ENUM sensor_type;
	enum BMP_POWERMODE_ENUM power_mode;
	u8 oversampling_p;
	u32 last_temp_measurement;
	u32 temp_measurement_period;
	union {
		struct bmp180_calibration_data bmp180_cali;
	};
	/* calculated temperature correction coefficient */
	s32 t_fine;

	/*misc */
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
#define BAR_FUN(f)               pr_debug(BAR_TAG"%s\n", __func__)
#define BAR_ERR(fmt, args...) \
	pr_err(BAR_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define BAR_LOG(fmt, args...)    pr_debug(BAR_TAG fmt, ##args)

static struct platform_driver bmp_barometer_driver;
static struct i2c_driver bmp_i2c_driver;
static struct bmp_i2c_data *obj_i2c_data;
static const struct i2c_device_id bmp_i2c_id[] = {
	{BMP_DEV_NAME, 0},
	{}
};

static struct i2c_board_info bmp_i2c_info __initdata = {
	I2C_BOARD_INFO(BMP_DEV_NAME, BMP180_I2C_ADDRESS)
};

/* I2C operation functions */
static int bmp_i2c_read_block(struct i2c_client *client, u8 addr, u8 *data, u8 len)
{
	u8 beg = addr;
	int err;
	struct i2c_msg msgs[2] = { {0}, {0} };

	mutex_lock(&bmp180_i2c_mutex);

	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = &beg;

	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = data;

	if (!client) {
		mutex_unlock(&bmp180_i2c_mutex);
		return -EINVAL;
	} else if (len > C_I2C_FIFO_SIZE) {
		BAR_ERR(" length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		mutex_unlock(&bmp180_i2c_mutex);
		return -EINVAL;
	}
	err = i2c_transfer(client->adapter, msgs, sizeof(msgs) / sizeof(msgs[0]));
	if (err != 2) {
		BAR_ERR("i2c_transfer error: (%d %p %d) %d\n", addr, data, len, err);
		err = -EIO;
	} else {
		err = 0;
	}
	mutex_unlock(&bmp180_i2c_mutex);
	return err;

}

static int bmp_i2c_write_block(struct i2c_client *client, u8 addr, u8 *data, u8 len)
{
/*because address also occupies one byte, the maximum length for write is 7 bytes */
	int err, idx, num;
	char buf[C_I2C_FIFO_SIZE];

	err = 0;
	mutex_lock(&bmp180_i2c_mutex);

	if (!client) {
		mutex_unlock(&bmp180_i2c_mutex);
		return -EINVAL;
	} else if (len >= C_I2C_FIFO_SIZE) {
		BAR_ERR(" length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		mutex_unlock(&bmp180_i2c_mutex);
		return -EINVAL;
	}

	num = 0;
	buf[num++] = addr;
	for (idx = 0; idx < len; idx++)
		buf[num++] = data[idx];

	err = i2c_master_send(client, buf, num);
	if (err < 0) {
		BAR_ERR("send command error!!\n");
		mutex_unlock(&bmp180_i2c_mutex);
		return -EFAULT;
	}
	mutex_unlock(&bmp180_i2c_mutex);
	return err;
}

static void bmp_power(struct baro_hw *hw, unsigned int on)
{
	static unsigned int power_on;

	if (hw->power_id != POWER_NONE_MACRO) {	/* have externel LDO */
		BAR_LOG("power %s\n", on ? "on" : "off");
		if (power_on == on) {	/* power status not change */
			BAR_LOG("ignore power control: %d\n", on);
		} else if (on) {	/* power on */
			if (!hwPowerOn(hw->power_id, hw->power_vol, BMP_DEV_NAME))
				BAR_ERR("power on failed\n");
		} else {	/* power off */
			if (!hwPowerDown(hw->power_id, BMP_DEV_NAME))
				BAR_ERR("power off failed\n");
		}
	}
	power_on = on;
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
	case BMP180_CHIP_ID:
		obj->sensor_type = BMP180_TYPE;
		strcpy(obj->sensor_name, "bmp180");
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
	int i = 0;

	if (obj->sensor_type == BMP180_TYPE) {
		u8 tmp[2] = { 0 };
		u16 cali_data[BMP180_CALIBRATION_DATA_LENGTH] = { 0 };

		for (i = 0; i < 11; i++) {
			status =
			    bmp_i2c_read_block(client, (BMP180_CALIBRATION_DATA_START + (i * 2)),
					       tmp, 0x02);
			if (status < 0)
				return status;
			cali_data[i] = tmp[0] | (tmp[1] >> 8);
			BAR_LOG("[%s] read data = 0x%x, 0x%x, i = %d\n", __func__, tmp[0], tmp[1],
				i);
			BAR_LOG("[%s] read address = 0x%x\n", __func__,
				(BMP180_CALIBRATION_DATA_START + (i * 2)));
		}

		obj->bmp180_cali.AC1 = be16_to_cpu(cali_data[0]);
		obj->bmp180_cali.AC2 = be16_to_cpu(cali_data[1]);
		obj->bmp180_cali.AC3 = be16_to_cpu(cali_data[2]);
		obj->bmp180_cali.AC4 = be16_to_cpu(cali_data[3]);
		obj->bmp180_cali.AC5 = be16_to_cpu(cali_data[4]);
		obj->bmp180_cali.AC6 = be16_to_cpu(cali_data[5]);
		obj->bmp180_cali.B1 = be16_to_cpu(cali_data[6]);
		obj->bmp180_cali.B2 = be16_to_cpu(cali_data[7]);
		obj->bmp180_cali.MB = be16_to_cpu(cali_data[8]);
		obj->bmp180_cali.MC = be16_to_cpu(cali_data[9]);
		obj->bmp180_cali.MD = be16_to_cpu(cali_data[10]);
	}

	return 0;
}

static int bmp_set_powermode(struct i2c_client *client, enum BMP_POWERMODE_ENUM power_mode)
{
	struct bmp_i2c_data *obj = i2c_get_clientdata(client);
	u8 err = 0;

	BAR_LOG("[%s] power_mode = %d, old power_mode = %d\n", __func__,
		power_mode, obj->power_mode);

	if (power_mode == obj->power_mode)
		return 0;

	if (obj->sensor_type == BMP180_TYPE) {	/* BMP180 */
		/* BMP180 only support forced mode */
		BAR_LOG("%s doesn't support hw power mode setting,only has forced mode\n", obj->sensor_name);
	}

	if (err < 0)
		BAR_ERR("set power mode failed, err = %d, sensor name = %s\n",
			err, obj->sensor_name);
	else
		obj->power_mode = power_mode;

	return err;
}

static int bmp_set_oversampling_p(struct i2c_client *client,
				  enum BMP_OVERSAMPLING_ENUM oversampling_p)
{
	struct bmp_i2c_data *obj = i2c_get_clientdata(client);
	u8 err = 0, data = 0, actual_oversampling_p = 0;

	BAR_LOG("[%s] oversampling_p = %d, old oversampling_p = %d\n", __func__, oversampling_p,
		obj->oversampling_p);

	if (oversampling_p == obj->oversampling_p)
		return 0;

	if (obj->sensor_type == BMP180_TYPE) {	/* BMP180 */
		if (oversampling_p == BMP_OVERSAMPLING_1X)
			actual_oversampling_p = BMP180_OVERSAMPLING_1X;
		else if (oversampling_p == BMP_OVERSAMPLING_2X)
			actual_oversampling_p = BMP180_OVERSAMPLING_2X;
		else if (oversampling_p == BMP_OVERSAMPLING_4X)
			actual_oversampling_p = BMP180_OVERSAMPLING_4X;
		else if (oversampling_p == BMP_OVERSAMPLING_8X)
			actual_oversampling_p = BMP180_OVERSAMPLING_8X;
		else {
			err = -EINVAL;
			BAR_ERR("invalid oversampling_p = %d\n", oversampling_p);
			return err;
		}
		err = bmp_i2c_read_block(client, BMP180_CTRLMEAS_REG_OSRSP__REG, &data, 1);
		data = BMP_SET_BITSLICE(data, BMP180_CTRLMEAS_REG_OSRSP, actual_oversampling_p);
		err += bmp_i2c_write_block(client, BMP180_CTRLMEAS_REG_OSRSP__REG, &data, 1);
	}

	if (err < 0)
		BAR_ERR("set pressure oversampling failed, err = %d, sensor name = %s\n", err,
			obj->sensor_name);
	else
		obj->oversampling_p = oversampling_p;

	return err;
}

static int bmp_read_raw_temperature(struct i2c_client *client, s32 *temperature)
{
	struct bmp_i2c_data *obj = i2c_get_clientdata(client);
	s32 err = 0;
	u16 tmp;
	u8 data;

	if (NULL == client) {
		err = -EINVAL;
		return err;
	}

	if (obj->sensor_type == BMP180_TYPE) {	/* BMP180 */
		err = bmp_i2c_read_block(client, BMP180_CTRLMEAS_REG_MC__REG, &data, 1);
		data = BMP_SET_BITSLICE(data, BMP180_CTRLMEAS_REG_MC, BMP180_TEMP_MEASUREMENT);
		err += bmp_i2c_write_block(client, BMP180_CTRLMEAS_REG_MC__REG, &data, 1);
		if (err < 0) {
			BAR_ERR("start measure temperature failed, err = %d\n", err);
			return err;
		}
		/* wait for the end of conversion */
		msleep(BMP180_TEMP_CONVERSION_TIME);

		err =
		    bmp_i2c_read_block(client, BMP180_CONVERSION_REGISTER_MSB, (u8 *) &tmp,
				       sizeof(tmp));
		if (err < 0) {
			BAR_ERR("read raw temperature failed, err = %d\n", err);
			return err;
		}
		*temperature = be16_to_cpu(tmp);
	}

	obj->last_temp_measurement = jiffies;

	return err;
}

static int bmp_read_raw_pressure(struct i2c_client *client, s32 *pressure)
{
	struct bmp_i2c_data *priv = i2c_get_clientdata(client);
	s32 err = 0;
	u32 tmp = 0;
	u8 data;

	if (NULL == client) {
		err = -EINVAL;
		return err;
	}

	if (priv->sensor_type == BMP180_TYPE) {	/* BMP180 */
		err = bmp_i2c_read_block(client, BMP180_CTRLMEAS_REG_MC__REG, &data, 1);
		data = BMP_SET_BITSLICE(data, BMP180_CTRLMEAS_REG_MC, BMP180_PRESSURE_MEASUREMENT);
		err += bmp_i2c_write_block(client, BMP180_CTRLMEAS_REG_MC__REG, &data, 1);
		if (err < 0) {
			BAR_ERR("start measure pressure failed, err = %d\n", err);
			return err;
		}
		/* wait for the end of conversion */
		msleep(2 + (3 << (priv->oversampling_p - 1)) + 10);

		/* copy data into a u32 (4 bytes), but skip the first byte. */
		err = bmp_i2c_read_block(client,
					 BMP180_CONVERSION_REGISTER_MSB, ((u8 *) &tmp) + 1, 3);
		if (err < 0) {
			BAR_ERR("read raw pressure failed, err = %d\n", err);
			return err;
		}
		*pressure = be32_to_cpu(tmp);
		*pressure >>= (8 - (priv->oversampling_p - 1));
	}
#ifdef CONFIG_BMP_LOWPASS
/*
*Example: firlen = 16, filter buffer = [0] ... [15],
*when 17th data come, replace [0] with this new data.
*Then, average this filter buffer and report average value to upper layer.
*/
	if (atomic_read(&priv->filter)) {
		if (atomic_read(&priv->fir_en) && !atomic_read(&priv->suspend)) {
			int idx, firlen = atomic_read(&priv->firlen);

			if (priv->fir.num < firlen) {
				priv->fir.raw[priv->fir.num][BMP_PRESSURE] = *pressure;
				priv->fir.sum[BMP_PRESSURE] += *pressure;
				if (atomic_read(&priv->trace) & BAR_TRC_FILTER) {
					BAR_LOG("add [%2d] [%5d] => [%5d]\n",
						priv->fir.num,
						priv->fir.raw
						[priv->fir.num][BMP_PRESSURE],
						priv->fir.sum[BMP_PRESSURE]);
				}
				priv->fir.num++;
				priv->fir.idx++;
			} else {
				idx = priv->fir.idx % firlen;
				priv->fir.sum[BMP_PRESSURE] -= priv->fir.raw[idx][BMP_PRESSURE];
				priv->fir.raw[idx][BMP_PRESSURE] = *pressure;
				priv->fir.sum[BMP_PRESSURE] += *pressure;
				priv->fir.idx++;
				*pressure = priv->fir.sum[BMP_PRESSURE] / firlen;
				if (atomic_read(&priv->trace) & BAR_TRC_FILTER) {
					BAR_LOG("add [%2d][%5d]=>[%5d]:[%5d]\n", idx,
						priv->fir.raw[idx][BMP_PRESSURE],
						priv->fir.sum[BMP_PRESSURE], *pressure);
				}
			}
		}
	}
#endif

	return err;
}

/*
*get compensated temperature
*unit:10 degrees centigrade
*/
static int bmp_get_temperature(struct i2c_client *client, char *buf, int bufsize)
{
	struct bmp_i2c_data *obj = i2c_get_clientdata(client);
	long x1, x2;
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

	if (obj->sensor_type == BMP180_TYPE) {	/* BMP180 */
		BAR_LOG("pressure sensor type is right\n");
		x1 = ((utemp - obj->bmp180_cali.AC6) * obj->bmp180_cali.AC5) >> 15;
		x2 = (obj->bmp180_cali.MC << 11) / (x1 + obj->bmp180_cali.MD);
		obj->t_fine = x1 + x2 - 4000;
		temperature = (x1 + x2 + 8) >> 4;
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
	int status;
	int err;
	s32 temperature = 0, upressure = 0, pressure = 0;
	char temp_buf[BMP_BUFSIZE];

	if (NULL == buf)
		return -1;

	if (NULL == client) {
		*buf = 0;
		return -2;
	}

	/* update the ambient temperature according to the given meas. period */
	if (time_before(obj->last_temp_measurement + obj->temp_measurement_period, jiffies)) {
		status = bmp_get_temperature(client, temp_buf, BMP_BUFSIZE);	/* update t_fine */
		if (status != 0)
			goto exit;
		err = kstrtoint(temp_buf, 10, &temperature);
		if (err)
			goto exit;
	}

	status = bmp_read_raw_pressure(client, &upressure);
	if (status != 0)
		goto exit;

	if (obj->sensor_type == BMP180_TYPE) {	/* BMP180 */
		s32 x1, x2, x3, b3;
		u32 b4, b7;
		s32 p;

		x1 = (obj->t_fine * obj->t_fine) >> 12;
		x1 *= obj->bmp180_cali.B2;
		x1 >>= 11;

		x2 = obj->bmp180_cali.AC2 * obj->t_fine;
		x2 >>= 11;

		x3 = x1 + x2;

		b3 = (((((s32) obj->bmp180_cali.AC1) * 4 + x3)
		       << (obj->oversampling_p - 1)) + 2);
		b3 >>= 2;

		x1 = (obj->bmp180_cali.AC3 * obj->t_fine) >> 13;
		x2 = (obj->bmp180_cali.B1 * ((obj->t_fine * obj->t_fine) >> 12))
		    >> 16;
		x3 = (x1 + x2 + 2) >> 2;
		b4 = (obj->bmp180_cali.AC4 * (u32) (x3 + 32768)) >> 15;

		b7 = ((u32) upressure - b3) * (50000 >> (obj->oversampling_p - 1));
		p = ((b7 < 0x80000000) ? ((b7 << 1) / b4) : ((b7 / b4) * 2));

		x1 = p >> 8;
		x1 *= x1;
		x1 = (x1 * 3038) >> 16;
		x2 = (-7357 * p) >> 16;
		p += (x1 + x2 + 3791) >> 4;

		pressure = p;
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

	err = bmp_set_oversampling_p(client, BMP_OVERSAMPLING_8X);
	if (err < 0) {
		BAR_ERR("set pressure oversampling failed, err = %d\n", err);
		return err;
	}

	return 0;
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
	ssize_t res;
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
	int trace;

	if (obj == NULL) {
		BAR_ERR("i2c_data obj is null\n");
		return 0;
	}

	if (1 == sscanf(buf, "0x%x", &trace))
		atomic_set(&obj->trace, trace);
	else
		BAR_ERR("invalid content: '%s', length = %d\n", buf, count);

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
				obj->hw->i2c_num,
				obj->hw->direction, obj->hw->power_id, obj->hw->power_vol);
	else
		len += snprintf(buf + len, PAGE_SIZE - len, "CUST: NULL\n");

	len += snprintf(buf + len, PAGE_SIZE - len, "i2c addr:%#x,ver:%s\n",
			obj->client->addr, BMP_DRIVER_VERSION);

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
	unsigned long power_mode;
	int err;

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

static DRIVER_ATTR(chipinfo, S_IRUGO, show_chipinfo_value, NULL);
static DRIVER_ATTR(sensordata, S_IRUGO, show_sensordata_value, NULL);
static DRIVER_ATTR(trace, S_IWUSR | S_IRUGO, show_trace_value, store_trace_value);
static DRIVER_ATTR(status, S_IRUGO, show_status_value, NULL);
static DRIVER_ATTR(powermode, S_IWUSR | S_IRUGO, show_power_mode_value, store_power_mode_value);

static struct driver_attribute *bmp_attr_list[] = {
	&driver_attr_chipinfo,	/* chip information */
	&driver_attr_sensordata,	/* dump sensor data */
	&driver_attr_trace,	/* trace log */
	&driver_attr_status,	/* cust setting */
	&driver_attr_powermode,	/* power mode */
};

static int bmp_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(bmp_attr_list) / sizeof(bmp_attr_list[0]));

	if (NULL == driver)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, bmp_attr_list[idx]);
		if (err) {
			BAR_ERR("driver_create_file (%s) = %d\n",
				bmp_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

static int bmp_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(bmp_attr_list) / sizeof(bmp_attr_list[0]));

	if (NULL == driver)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, bmp_attr_list[idx]);

	return err;
}

int barometer_operate(void *self, uint32_t command, void *buff_in, int size_in,
		      void *buff_out, int size_out, int *actualout)
{
	int err = 0;
	int value;
	struct bmp_i2c_data *priv = (struct bmp_i2c_data *)self;
	hwm_sensor_data *barometer_data;
	char buff[BMP_BUFSIZE];

	switch (command) {
	case SENSOR_DELAY:
		/* under construction */
		break;

	case SENSOR_ENABLE:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			BAR_ERR("enable sensor parameter error\n");
			err = -EINVAL;
		} else {
			mutex_lock(&bmp180_op_mutex);
			/* value:[0--->suspend, 1--->normal] */
			value = *(int *)buff_in;
			BAR_LOG("sensor enable/disable command: %s\n",
				value ? "enable" : "disable");

			err = bmp_set_powermode(priv->client, (enum BMP_POWERMODE_ENUM)(!!value));
			if (err)
				BAR_ERR("set power mode failed, err = %d\n", err);
#ifdef CONFIG_BMP_LOWPASS
			/* clear filter buffer */
			if (value == 0)
				memset(&(priv->fir), 0, sizeof(struct data_filter));
#endif
			mutex_unlock(&bmp180_op_mutex);
		}
		break;

	case SENSOR_GET_DATA:
		if ((buff_out == NULL) || (size_out < sizeof(hwm_sensor_data))) {
			BAR_ERR("get sensor data parameter error\n");
			err = -EINVAL;
		} else {
			mutex_lock(&bmp180_op_mutex);
			barometer_data = (hwm_sensor_data *) buff_out;
			err = bmp_get_pressure(priv->client, buff, BMP_BUFSIZE);
			if (err) {
				BAR_ERR("get compensated pressure value failed," "err = %d\n", err);
				return -1;
			}
			err = kstrtoint(buff, 10, &barometer_data->values[0]);
			if (err)
				break;
			barometer_data->values[1] = barometer_data->values[2] = 0;
			barometer_data->status = SENSOR_STATUS_ACCURACY_HIGH;
			barometer_data->value_divide = 100;
			mutex_unlock(&bmp180_op_mutex);
		}
		break;

	default:
		BAR_ERR("barometer operate function no this parameter %d\n", command);
		err = -1;
		break;
	}

	return err;
}

#ifdef CONFIG_ID_TEMPERATURE
int temperature_operate(void *self, uint32_t command, void *buff_in,
			int size_in, void *buff_out, int size_out, int *actualout)
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
			BAR_ERR("enable sensor parameter error\n");
			err = -EINVAL;
		} else {
			mutex_lock(&bmp180_op_mutex);
			/* value:[0--->suspend, 1--->normal] */
			value = *(int *)buff_in;
			BAR_LOG("sensor enable/disable command: %s\n",
				value ? "enable" : "disable");

			err = bmp_set_powermode(priv->client, (enum BMP_POWERMODE_ENUM)(!!value));
			if (err)
				BAR_ERR("set power mode failed, err = %d\n", err);

			mutex_unlock(&bmp180_op_mutex);
		}
		break;

	case SENSOR_GET_DATA:
		if ((buff_out == NULL) || (size_out < sizeof(hwm_sensor_data))) {
			BAR_ERR("get sensor data parameter error\n");
			err = -EINVAL;
		} else {
			mutex_lock(&bmp180_op_mutex);
			temperature_data = (hwm_sensor_data *) buff_out;
			err = bmp_get_temperature(priv->client, buff, BMP_BUFSIZE);
			if (err) {
				BAR_ERR("get compensated temperature value failed,err = %d\n", err);
				return -1;
			}
			err = kstrtoint(buff, 10, &temperature_data->values[0]);
			if (err)
				break;
			temperature_data->values[1] = temperature_data->values[2] = 0;
			temperature_data->status = SENSOR_STATUS_ACCURACY_HIGH;
			temperature_data->value_divide = 100;
			mutex_unlock(&bmp180_op_mutex);
		}
		break;

	default:
		BAR_ERR("temperature operate function no this parameter %d\n", command);
		err = -1;
		break;
	}

	return err;
}
#endif				/* CONFIG_ID_TEMPERATURE */

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
		if (err) {
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
		err = kstrtoint(strbuf, 10, &dat);
		if (err)
			break;
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
		err = kstrtoint(strbuf, 10, &dat);
		if (err)
			break;
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
	mutex_lock(&bmp180_op_mutex);
	if (msg.event == PM_EVENT_SUSPEND) {
		if (NULL == obj) {
			BAR_ERR("null pointer\n");
			mutex_unlock(&bmp180_op_mutex);
			return -EINVAL;
		}

		atomic_set(&obj->suspend, 1);
		err = bmp_set_powermode(obj->client, BMP_SUSPEND_MODE);
		if (err) {
			BAR_ERR("bmp set suspend mode failed, err = %d\n", err);
			mutex_unlock(&bmp180_op_mutex);
			return err;
		}
		bmp_power(obj->hw, 0);
	}
	mutex_unlock(&bmp180_op_mutex);
	return err;
}

static int bmp_resume(struct i2c_client *client)
{
	struct bmp_i2c_data *obj = i2c_get_clientdata(client);
	int err;

	BAR_FUN();
	mutex_lock(&bmp180_op_mutex);
	if (NULL == obj) {
		BAR_ERR("null pointer\n");
		mutex_unlock(&bmp180_op_mutex);
		return -EINVAL;
	}

	bmp_power(obj->hw, 1);

	err = bmp_init_client(obj->client);
	if (err) {
		BAR_ERR("initialize client fail\n");
		mutex_unlock(&bmp180_op_mutex);
		return err;
	}
#ifdef CONFIG_BMP_LOWPASS
	memset(&obj->fir, 0x00, sizeof(obj->fir));
#endif

	atomic_set(&obj->suspend, 0);
	mutex_unlock(&bmp180_op_mutex);
	return 0;
}

static int bmp_i2c_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	strcpy(info->type, BMP_DEV_NAME);
	return 0;
}

static int bmp_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct bmp_i2c_data *obj;
	struct hwmsen_object sobj_p;
#ifdef CONFIG_ID_TEMPERATURE
	struct hwmsen_object sobj_t;
#endif
	int err = 0;

	BAR_FUN();

	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj) {
		err = -ENOMEM;
		goto exit;
	}

	obj->hw = get_cust_baro_hw();
	obj_i2c_data = obj;
	obj->client = client;
	i2c_set_clientdata(client, obj);

	atomic_set(&obj->trace, 0);
	atomic_set(&obj->suspend, 0);
	obj->power_mode = BMP_UNDEFINED_POWERMODE;
	obj->oversampling_p = BMP_UNDEFINED_OVERSAMPLING;
	obj->last_temp_measurement = 0;
	obj->temp_measurement_period = 1 * HZ;	/* temperature update period:1s */

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

	err = bmp_create_attr(&bmp_barometer_driver.driver);
	if (err) {
		BAR_ERR("create attribute failed, err = %d\n", err);
		goto exit_create_attr_failed;
	}

	sobj_p.self = obj;
	sobj_p.polling = 1;
	sobj_p.sensor_operate = barometer_operate;
	err = hwmsen_attach(ID_PRESSURE, &sobj_p);
	if (err) {
		BAR_ERR("hwmsen attach failed, err = %d\n", err);
		goto exit_hwmsen_attach_pressure_failed;
	}
#ifdef CONFIG_ID_TEMPERATURE
	sobj_t.self = obj;
	sobj_t.polling = 1;
	sobj_t.sensor_operate = temperature_operate;
	err = hwmsen_attach(ID_TEMPRERATURE, &sobj_t);
	if (err) {
		BAR_ERR("hwmsen attach failed, err = %d\n", err);
		goto exit_hwmsen_attach_temperature_failed;
	}
#endif				/* CONFIG_ID_TEMPERATURE */

	BAR_LOG("%s: OK\n", __func__);
	return 0;

#ifdef CONFIG_ID_TEMPERATURE
exit_hwmsen_attach_temperature_failed:
	hwmsen_detach(ID_TEMPRERATURE);
#endif				/* CONFIG_ID_TEMPERATURE */
exit_hwmsen_attach_pressure_failed:
	bmp_delete_attr(&bmp_barometer_driver.driver);
exit_create_attr_failed:
	misc_deregister(&bmp_device);
exit_misc_device_register_failed:
exit_init_client_failed:
	kfree(obj);
exit:
	BAR_ERR("err = %d\n", err);
	return err;
}

static int bmp_i2c_remove(struct i2c_client *client)
{
	int err = 0;

	err = hwmsen_detach(ID_PRESSURE);
	if (err)
		BAR_ERR("hwmsen_detach ID_PRESSURE failed, err = %d\n", err);

#ifdef CONFIG_ID_TEMPERATURE
	err = hwmsen_detach(ID_TEMPRERATURE);
	if (err)
		BAR_ERR("hwmsen_detach ID_TEMPRERATURE failed, err = %d\n", err);
#endif

	err = bmp_delete_attr(&bmp_barometer_driver.driver);
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

static int bmp_probe(struct platform_device *pdev)
{
	struct baro_hw *hw = get_cust_baro_hw();

	BAR_FUN();

	bmp_power(hw, 1);
	if (i2c_add_driver(&bmp_i2c_driver)) {
		BAR_ERR("add i2c driver failed\n");
		return -1;
	}

	return 0;
}

static int bmp_remove(struct platform_device *pdev)
{
	struct baro_hw *hw = get_cust_baro_hw();

	BAR_FUN();

	bmp_power(hw, 0);
	i2c_del_driver(&bmp_i2c_driver);

	return 0;
}

static struct i2c_driver bmp_i2c_driver = {
	.driver = {
		   .owner = THIS_MODULE,
		   .name = BMP_DEV_NAME,
		   },
	.probe = bmp_i2c_probe,
	.remove = bmp_i2c_remove,
	.detect = bmp_i2c_detect,
	.suspend = bmp_suspend,
	.resume = bmp_resume,
	.id_table = bmp_i2c_id,
};

static struct platform_driver bmp_barometer_driver = {
	.probe = bmp_probe,
	.remove = bmp_remove,
	.driver = {
		   .name = "barometer",
		   .owner = THIS_MODULE,
		   }
};

static int __init bmp_init(void)
{
	struct baro_hw *hw = get_cust_baro_hw();

	BAR_FUN();
	i2c_register_board_info(hw->i2c_num, &bmp_i2c_info, 1);
	if (platform_driver_register(&bmp_barometer_driver)) {
		BAR_ERR("register bmp platform driver failed");
		return -ENODEV;
	}
	return 0;
}

static void __exit bmp_exit(void)
{
	BAR_FUN();
	platform_driver_unregister(&bmp_barometer_driver);
}

module_init(bmp_init);
module_exit(bmp_exit);

MODULE_LICENSE("GPLv2");
MODULE_DESCRIPTION("BMP180 I2C Driver");
MODULE_AUTHOR("deliang.tao@bosch-sensortec.com");
MODULE_VERSION(BMP_DRIVER_VERSION);
