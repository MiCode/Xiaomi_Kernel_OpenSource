
#include <linux/device.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/module.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/kfifo_buf.h>
#include <linux/iio/buffer.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include "bmp280_core.h"

#define BMP_IIO_NAME		"pressure"

#define BMP_SENSOR_CHIP_ID_1	0x56
#define BMP_SENSOR_CHIP_ID_2	0x57
#define BMP_SENSOR_CHIP_ID_3	0x58
#define BMP_I2C_ADDRESS		BMP280_I2C_ADDRESS
#define ABS_MIN_PRESSURE	30000
#define ABS_MAX_PRESSURE	110000
#define ABS_PRESSURE_FUZZ	5
#define ABS_PRESSURE_FLAT	5

#define BMP_DELAY_DEFAULT	200
#define BMP_OVERSAMPLING_T_MAX	BMP_VAL_NAME(OVERSAMPLING_16X)
#define BMP_OVERSAMPLING_P_MAX	BMP_VAL_NAME(OVERSAMPLING_16X)
#define BMP_FILTER_DEFAULT	BMP_VAL_NAME(FILTERCOEFF_8)
#define BMP_FILTER_MAX		BMP_VAL_NAME(FILTERCOEFF_16)
#define BMP_WORKMODE_DEFAULT		BMP_VAL_NAME(STANDARDRESOLUTION_MODE)
#define BMP_STANDBYDUR_DEFAULT	1
#define BMP280_I2C_DISABLE_SWITCH	0x87
#define BMP_SELFTEST_NO_ACTION	-1
#define BMP_SELFTEST_FAILED	0
#define BMP_SELFTEST_SUCCESS	1

#define CM_SENSORS_CHANNELS(device_type, mask, index, mod, \
					ch2, s, endian, rbits, sbits, addr) \
{ \
	.type = device_type, \
	.modified = mod, \
	.info_mask_separate = mask, \
	.scan_index = index, \
	.channel2 = ch2, \
	.address = addr, \
	.scan_type = { \
		.sign = s, \
		.realbits = rbits, \
		.shift = sbits - rbits, \
		.storagebits = sbits, \
		.endianness = endian, \
	}, \
}

struct bmp_client_data {
	struct bmp_data_bus data_bus;
	struct bmp280_t device;
	struct device *dev;
	struct mutex lock;

	u8 oversampling_t;
	u8 oversampling_p;
	u8 op_mode;
	u8 filter;
	u32 standbydur;
	u8 workmode;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
	struct iio_dev *idev;
	struct delayed_work work;
	u32 delay;
	u32 pressure;
	bool enable;
	s8 selftest;
};

struct bmp_el_data {
	uint32_t data;
	uint32_t pad1;
	int64_t timestamp;
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void bmp_early_suspend(struct early_suspend *h);
static void bmp_late_resume(struct early_suspend *h);
#endif

static const struct iio_chan_spec bmp_iio_channels[] = {
	CM_SENSORS_CHANNELS(IIO_PRESSURE,
		BIT(IIO_CHAN_INFO_RAW) | BIT(IIO_CHAN_INFO_SCALE) | BIT(IIO_CHAN_INFO_SAMP_FREQ),
		0, 0, IIO_NO_MOD, 'u', IIO_LE, 32, 32, 0),
	IIO_CHAN_SOFT_TIMESTAMP(1)
};

static const u32 standbydur_list[] = {
	1, 63, 125, 250, 500, 1000, 2000, 4000};
static const u8 op_mode_list[] = {
	BMP_VAL_NAME(SLEEP_MODE),
	BMP_VAL_NAME(FORCED_MODE),
	BMP_VAL_NAME(NORMAL_MODE)
};
static const u8 workmode_list[] = {
	BMP_VAL_NAME(ULTRALOWPOWER_MODE),
	BMP_VAL_NAME(LOWPOWER_MODE),
	BMP_VAL_NAME(STANDARDRESOLUTION_MODE),
	BMP_VAL_NAME(HIGHRESOLUTION_MODE),
	BMP_VAL_NAME(ULTRAHIGHRESOLUTION_MODE)
};

static void bmp_delay(u16 msec)
{
	mdelay(msec);
}

static int bmp_check_chip_id(struct bmp_data_bus *data_bus)
{
	int err = 0;
	u8 chip_id = 0;

	err = data_bus->bops->bus_read(BMP_I2C_ADDRESS, \
		BMP_REG_NAME(CHIPID_REG), &chip_id, 1);
	if (err < 0) {
		err = -EIO;
		PERR("bus read failed\n");
		return err;
	}

	if (BMP_SENSOR_CHIP_ID_1 != chip_id &&
		BMP_SENSOR_CHIP_ID_2 != chip_id &&
		BMP_SENSOR_CHIP_ID_3 != chip_id) {
		err = -ENODEV;
		PERR("read %s chip id failed, read value = %d\n", \
				BMP_NAME, chip_id);
		return err;
	}
	PINFO("read %s chip id successfully", BMP_NAME);

	return err;
}

static int bmp_get_temperature(struct bmp_client_data *data, s32 *temperature)
{
	s32 utemperature;
	int err = 0;

	err = BMP_CALL_API(read_ut)(&utemperature);
	if (err)
		return err;

	*temperature = BMP_CALL_API(compensate_T_int32)(utemperature);
	return err;
}

static int bmp_get_pressure(struct bmp_client_data *data, u32 *pressure)
{
	s32 temperature;
	s32 upressure;
	int err = 0;

	err = bmp_get_temperature(data, &temperature);
	if (err)
		return err;

	err = BMP_CALL_API(read_up)(&upressure);
	if (err)
		return err;

	*pressure = (BMP_CALL_API(compensate_P_int64)(upressure))>>8;
	return err;
}

static u32 bmp_get_oversampling_t(struct bmp_client_data *data)
{
	int err = 0;

	err = BMP_CALL_API(get_osrs_t)(&data->oversampling_t);
	if (err)
		return err;

	return data->oversampling_t;
}

static int bmp_set_oversampling_t(struct bmp_client_data *data, u8 oversampling)
{
	int err = 0;

	if (oversampling > BMP_OVERSAMPLING_T_MAX)
		oversampling = BMP_OVERSAMPLING_T_MAX;

	err = BMP_CALL_API(set_osrs_t)(oversampling);
	if (err)
		return	err;

	data->oversampling_t = oversampling;
	return err;
}

static u32 bmp_get_oversampling_p(struct bmp_client_data *data)
{
	int err = 0;

	err = BMP_CALL_API(get_osrs_p)(&data->oversampling_p);
	if (err)
		return err;

	return data->oversampling_p;
}

static int bmp_set_oversampling_p(struct bmp_client_data *data, u8 oversampling)
{
	int err = 0;

	if (oversampling > BMP_OVERSAMPLING_P_MAX)
		oversampling = BMP_OVERSAMPLING_P_MAX;

	err = BMP_CALL_API(set_osrs_p)(oversampling);
	if (err)
		return err;

	data->oversampling_p = oversampling;
	return err;
}

static u32 bmp_get_op_mode(struct bmp_client_data *data)
{
	int err = 0;

	err = BMP_CALL_API(get_mode)(&data->op_mode);
	if (err)
		return err;

	if (data->op_mode == 0x01 || data->op_mode == 0x02)
		data->op_mode = BMP_VAL_NAME(FORCED_MODE);

	return data->op_mode;
}

static int bmp_set_op_mode(struct bmp_client_data *data, u8 op_mode)
{
	int err = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(op_mode_list); i++) {
		if (op_mode_list[i] == op_mode)
			break;
	}

	if (ARRAY_SIZE(op_mode_list) <= i)
		return -1;

	err = BMP_CALL_API(set_mode)(op_mode);
	if (err)
		return err;

	data->op_mode = op_mode;
	return err;
}

static u32 bmp_get_filter(struct bmp_client_data *data)
{
	int err = 0;

	err = BMP_CALL_API(get_filter)(&data->filter);
	if (err)
		return err;

	return data->filter;
}

static int bmp_set_filter(struct bmp_client_data *data, u8 filter)
{
	int err = 0;

	if (filter > BMP_FILTER_MAX)
		filter = BMP_FILTER_MAX;

	err = BMP_CALL_API(set_filter)(filter);
	if (err)
		return err;

	data->filter = filter;
	return err;
}

static u32 bmp_get_standbydur(struct bmp_client_data *data)
{
	int err = 0;
	u8 standbydur;

	err = BMP_CALL_API(get_standbydur)(&standbydur);
	if (err)
		return err;

	data->standbydur = standbydur_list[standbydur];
	return data->standbydur;
}

static int bmp_set_standbydur(struct bmp_client_data *data, u32 standbydur)
{
	int err = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(standbydur_list); i++) {
		if (standbydur_list[i] == standbydur)
			break;
	}

	if (ARRAY_SIZE(standbydur_list) <= i)
		return -1;

	err = BMP_CALL_API(set_standbydur)(i);
	if (err)
		return err;

	data->standbydur = standbydur;
	return err;
}

static unsigned char bmp_get_workmode(struct bmp_client_data *data)
{
	return data->workmode;
}

static int bmp_set_workmode(struct bmp_client_data *data, u8 workmode)
{
	int err = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(workmode_list); i++) {
		if (workmode_list[i] == workmode)
			break;
	}

	if (ARRAY_SIZE(workmode_list) <= i)
		return -1;

	err = BMP_CALL_API(set_workmode)(workmode);
	if (err)
		return err;
	else
		data->workmode = workmode;

	bmp_get_oversampling_t(data);
	bmp_get_oversampling_p(data);

	return err;
}

static int bmp280_verify_i2c_disable_switch(struct bmp_client_data *data)
{
	int err = 0;
	u8 reg_val = 0xFF;

	err = BMP_CALL_API(read_register)
		(BMP280_I2C_DISABLE_SWITCH, &reg_val, 1);
	if (err < 0) {
		err = -EIO;
		PERR("bus read failed\n");
		return err;
	}

	if (reg_val == 0x00) {
		PINFO("bmp280 i2c interface is available\n");
		return 0;
	}

	PERR("verification of i2c interface is failure\n");
	return -1;
}

static int bmp280_verify_calib_param(struct bmp_client_data *data)
{
	struct bmp280_calibration_param_t *cali = &(data->device.cal_param);

	if (cali->dig_T1 == 0 && cali->dig_T2 == 0 && cali->dig_T3 == 0
		&& cali->dig_P1 == 0 &&	cali->dig_P2 == 0
		&& cali->dig_P3 == 0 && cali->dig_P4 == 0
		&& cali->dig_P5 == 0 && cali->dig_P6 == 0
		&& cali->dig_P7 == 0 && cali->dig_P8 == 0
		&& cali->dig_P9 == 0) {
		PERR("all calibration parameters are zero\n");
		return -1;
	}

	if (cali->dig_T1 < 19000 || cali->dig_T1 > 35000)
		return -1;
	else if (cali->dig_T2 < 22000 || cali->dig_T2 > 30000)
		return -1;
	else if (cali->dig_T3 < -3000 || cali->dig_T3 > -1000)
		return -1;
	else if (cali->dig_P1 < 30000 || cali->dig_P1 > 42000)
		return -1;
	else if (cali->dig_P2 < -12970 || cali->dig_P2 > -8000)
		return -1;
	else if (cali->dig_P3 < -5000 || cali->dig_P3 > 8000)
		return -1;
	else if (cali->dig_P4 < -10000 || cali->dig_P4 > 18000)
		return -1;
	else if (cali->dig_P5 < -500 || cali->dig_P5 > 1100)
		return -1;
	else if (cali->dig_P6 < -1000 || cali->dig_P6 > 1000)
		return -1;
	else if (cali->dig_P7 < -32768 || cali->dig_P7 > 32767)
		return -1;
	else if (cali->dig_P8 < -30000 || cali->dig_P8 > 10000)
		return -1;
	else if (cali->dig_P9 < -10000 || cali->dig_P9 > 30000)
		return -1;

	PINFO("calibration parameters are OK\n");
	return 0;
}

static int bmp280_verify_pt(struct bmp_client_data *data)
{
	u8 wait_time = 0;
	s32 temperature;
	u32 pressure;

	bmp_set_workmode(data, BMP_VAL_NAME(ULTRALOWPOWER_MODE));
	bmp_set_op_mode(data, BMP_VAL_NAME(FORCED_MODE));
	BMP_CALL_API(compute_wait_time)(&wait_time);
	bmp_delay(wait_time);
	bmp_get_temperature(data, &temperature);
	if (temperature <= 0 || temperature >= 40*100) {
		PERR("temperature value is out of range:%d*0.01degree\n",
			temperature);
		return -1;
	}
	bmp_get_pressure(data, &pressure);
	if (pressure <= 900*100 || pressure >= 1100*100) {
		PERR("pressure value is out of range:%d Pa\n", pressure);
		return -1;
	}

	PINFO("bmp280 temperature and pressure values are OK\n");
	return 0;
}

static int bmp_do_selftest(struct bmp_client_data *data)
{
	int err = 0;

	if (data == NULL)
		return -EINVAL;

	err = bmp280_verify_i2c_disable_switch(data);
	if (err) {
		data->selftest = 0;
		return BMP_SELFTEST_FAILED;
	}

	err = bmp280_verify_calib_param(data);
	if (err) {
		data->selftest = 0;
		return BMP_SELFTEST_FAILED;
	}

	err = bmp280_verify_pt(data);
	if (err) {
		data->selftest = 0;
		return BMP_SELFTEST_FAILED;
	}

	data->selftest = 1;
	return BMP_SELFTEST_SUCCESS;
}

static ssize_t show_delay(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmp_client_data *data = iio_priv(indio_dev);
	return sprintf(buf, "%d\n", data->delay);
}

static ssize_t store_delay(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmp_client_data *data = iio_priv(indio_dev);
	unsigned long delay;
	int status = kstrtoul(buf, 10, &delay);
	if (status == 0) {
		mutex_lock(&data->lock);
		data->delay = delay;
		mutex_unlock(&data->lock);
		return count;
	}
	return status;
}

static ssize_t show_temperature(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	s32 temperature;
	int status;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmp_client_data *data = iio_priv(indio_dev);

	mutex_lock(&data->lock);
	status = bmp_get_temperature(data, &temperature);
	mutex_unlock(&data->lock);
	if (status == 0)
		return sprintf(buf, "%d\n", temperature);

	return status;
}

static ssize_t show_pressure(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	u32 pressure;
	int status;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmp_client_data *data = iio_priv(indio_dev);

	mutex_lock(&data->lock);
	status = bmp_get_pressure(data, &pressure);
	mutex_unlock(&data->lock);
	if (status == 0)
		return sprintf(buf, "%d\n", pressure);

	return status;
}

static ssize_t show_oversampling_t(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmp_client_data *data = iio_priv(indio_dev);
	return sprintf(buf, "%d\n", bmp_get_oversampling_t(data));
}

static ssize_t store_oversampling_t(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmp_client_data *data = iio_priv(indio_dev);
	unsigned long oversampling;
	int status = kstrtoul(buf, 10, &oversampling);
	if (status == 0) {
		mutex_lock(&data->lock);
		bmp_set_oversampling_t(data, oversampling);
		mutex_unlock(&data->lock);
		return count;
	}
	return status;
}

static ssize_t show_oversampling_p(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmp_client_data *data = iio_priv(indio_dev);
	return sprintf(buf, "%d\n", bmp_get_oversampling_p(data));
}

static ssize_t store_oversampling_p(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmp_client_data *data = iio_priv(indio_dev);
	unsigned long oversampling;
	int status = kstrtoul(buf, 10, &oversampling);
	if (status == 0) {
		mutex_lock(&data->lock);
		bmp_set_oversampling_p(data, oversampling);
		mutex_unlock(&data->lock);
		return count;
	}
	return status;
}

static ssize_t show_op_mode(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmp_client_data *data = iio_priv(indio_dev);
	return sprintf(buf, "%d\n", bmp_get_op_mode(data));
}

static ssize_t store_op_mode(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmp_client_data *data = iio_priv(indio_dev);
	unsigned long op_mode;
	int status = kstrtoul(buf, 10, &op_mode);
	if (status == 0) {
		mutex_lock(&data->lock);
		bmp_set_op_mode(data, op_mode);
		mutex_unlock(&data->lock);
		return count;
	}
	return status;
}

static ssize_t show_filter(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmp_client_data *data = iio_priv(indio_dev);
	return sprintf(buf, "%d\n", bmp_get_filter(data));
}

static ssize_t store_filter(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmp_client_data *data = iio_priv(indio_dev);
	unsigned long filter;
	int status = kstrtoul(buf, 10, &filter);
	if (status == 0) {
		mutex_lock(&data->lock);
		bmp_set_filter(data, filter);
		mutex_unlock(&data->lock);
		return count;
	}
	return status;
}

static ssize_t show_standbydur(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmp_client_data *data = iio_priv(indio_dev);
	return sprintf(buf, "%d\n", bmp_get_standbydur(data));
}

static ssize_t store_standbydur(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmp_client_data *data = iio_priv(indio_dev);
	unsigned long standbydur;
	int status = kstrtoul(buf, 10, &standbydur);
	if (status == 0) {
		mutex_lock(&data->lock);
		bmp_set_standbydur(data, standbydur);
		mutex_unlock(&data->lock);
		return count;
	}
	return status;
}

static ssize_t show_workmode(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmp_client_data *data = iio_priv(indio_dev);
	return sprintf(buf, "%d\n", bmp_get_workmode(data));
}

static ssize_t store_workmode(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmp_client_data *data = iio_priv(indio_dev);
	unsigned long workmode;
	int status = kstrtoul(buf, 10, &workmode);
	if (status == 0) {
		mutex_lock(&data->lock);
		bmp_set_workmode(data, workmode);
		mutex_unlock(&data->lock);
		return count;
	}
	return status;
}

static ssize_t show_selftest(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmp_client_data *data = iio_priv(indio_dev);
	return sprintf(buf, "%d\n", data->selftest);
}

static ssize_t store_selftest(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmp_client_data *data = iio_priv(indio_dev);
	unsigned long action;
	int status = kstrtoul(buf, 10, &action);
	if (0 != status)
		return status;

	if (1 != action)
		return -EINVAL;

	mutex_lock(&data->lock);
	status = bmp_do_selftest(data);
	mutex_unlock(&data->lock);

	if (BMP_SELFTEST_SUCCESS == status)
		return count;
	else
		return status;
}

#ifdef DEBUG_BMP280
static ssize_t show_dump_reg(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	#define REG_CALI_NUM (0xA1 - 0x88 + 1)
	#define REG_CTRL_NUM (0xFC - 0xF3 + 1)
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct bmp_client_data *data = iio_priv(indio_dev);
	char regsbuf[REG_CALI_NUM + REG_CTRL_NUM];
	char strbuf[REG_CALI_NUM + REG_CTRL_NUM + 256] = {0};
	int err = 0, i;

	sprintf(strbuf + strlen(strbuf), \
			"-----calib regs[0x88 ~ 0xA1]-----\n");
	err = data->data_bus.bops->bus_read(BMP_I2C_ADDRESS, \
			0x88, regsbuf, REG_CALI_NUM);
	if (err)
		return err;

	for (i = 0; i < REG_CALI_NUM; i++)
		sprintf(strbuf + strlen(strbuf), "%02X%c", \
				regsbuf[i], ((i+1)%0x10 == 0) ? '\n' : ' ');

	sprintf(strbuf + strlen(strbuf), \
			"\n-----ctrl regs[0xF3 ~ 0xFC]-----\n");
	err = data->data_bus.bops->bus_read(BMP_I2C_ADDRESS, \
		0xF3, regsbuf, REG_CTRL_NUM);
	if (err)
		return err;

	for (i = 0; i < REG_CTRL_NUM; i++)
		sprintf(strbuf + strlen(strbuf), "%02X ", regsbuf[i]);

	return snprintf(buf, 4096, "%s\n", strbuf);
}
#endif

static int bmp_iio_read_raw(struct iio_dev *indio_dev,
				struct iio_chan_spec const *chan,
				int *val, int *val2, long mask)
{
	int ret = 0;
	int scale = 0;
	struct bmp_client_data *data = iio_priv(indio_dev);

	mutex_lock(&data->lock);
	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		if (data->enable)
			*val = data->pressure;
		else
			*val = 0;
		ret = IIO_VAL_INT;
		break;
	case IIO_CHAN_INFO_SCALE:
		scale = 0.01*1000000;
		*val = scale / 1000000;
		*val2 = scale % 1000000;
		ret = IIO_VAL_INT_PLUS_MICRO;
		break;
	case IIO_CHAN_INFO_SAMP_FREQ:
		*val = 10;
		ret = IIO_VAL_INT;
		break;
	default:
		ret = -EINVAL;
	}
	mutex_unlock(&data->lock);

	return ret;
}

static int bmp_iio_write_raw(struct iio_dev *indio_dev,
				struct iio_chan_spec const *chan,
				int val, int val2, long mask)
{
	int ret = 0;

	switch (mask) {
	case IIO_CHAN_INFO_SAMP_FREQ:
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

static int bmp_iio_write_get_fmt(struct iio_dev *indio_dev,
				      struct iio_chan_spec const *chan,
				      long mask)
{
	return IIO_VAL_INT_PLUS_MICRO;
}



static DEVICE_ATTR(delay, S_IWUSR | S_IRUGO,
			show_delay, store_delay);
static DEVICE_ATTR(temperature, S_IRUGO,
			show_temperature, NULL);
static DEVICE_ATTR(pressure, S_IRUGO,
			show_pressure, NULL);
static DEVICE_ATTR(oversampling_t, S_IWUSR | S_IRUGO,
			show_oversampling_t, store_oversampling_t);
static DEVICE_ATTR(oversampling_p, S_IWUSR | S_IRUGO,
			show_oversampling_p, store_oversampling_p);
static DEVICE_ATTR(op_mode, S_IWUSR | S_IRUGO,
			show_op_mode, store_op_mode);
static DEVICE_ATTR(filter, S_IWUSR | S_IRUGO,
			show_filter, store_filter);
static DEVICE_ATTR(standbydur, S_IWUSR | S_IRUGO,
			show_standbydur, store_standbydur);
static DEVICE_ATTR(workmode, S_IWUSR | S_IRUGO,
			show_workmode, store_workmode);
static DEVICE_ATTR(selftest, S_IWUSR | S_IRUGO,
			show_selftest, store_selftest);
#ifdef DEBUG_BMP280
static DEVICE_ATTR(dump_reg, S_IRUGO,
			show_dump_reg, NULL);
#endif

static struct attribute *bmp_attributes[] = {
	&dev_attr_delay.attr,
	&dev_attr_temperature.attr,
	&dev_attr_pressure.attr,
	&dev_attr_oversampling_t.attr,
	&dev_attr_oversampling_p.attr,
	&dev_attr_op_mode.attr,
	&dev_attr_filter.attr,
	&dev_attr_standbydur.attr,
	&dev_attr_workmode.attr,
	&dev_attr_selftest.attr,
#ifdef DEBUG_BMP280
	&dev_attr_dump_reg.attr,
#endif
	NULL
};

static const struct attribute_group bmp_attr_group = {
	.attrs = bmp_attributes,
};


static const struct iio_info bmp_iio_info = {
	.driver_module = THIS_MODULE,
	.attrs = &bmp_attr_group,
	.read_raw = bmp_iio_read_raw,
	.write_raw = bmp_iio_write_raw,
	.write_raw_get_fmt = bmp_iio_write_get_fmt,
};

static int bmp_enable_disable(struct iio_dev *indio_dev, bool enable)
{
	struct bmp_client_data *data = iio_priv(indio_dev);

	mutex_lock(&data->lock);
	if (data->enable != enable) {
		if (enable) {
			#ifdef CONFIG_PM
			bmp_enable(data->dev);
			#endif
			bmp_set_op_mode(data, \
				BMP_VAL_NAME(NORMAL_MODE));
			schedule_delayed_work(&data->work,
				msecs_to_jiffies(data->delay));
		} else{
			cancel_delayed_work(&data->work);
			bmp_set_op_mode(data, \
				BMP_VAL_NAME(SLEEP_MODE));
			#ifdef CONFIG_PM
			bmp_disable(data->dev);
			#endif
		}
		data->enable = enable;
	}
	mutex_unlock(&data->lock);

	return 0;
}

static int bmp_buffer_postenable(struct iio_dev *indio_dev)
{
	return bmp_enable_disable(indio_dev, true);
}

static int bmp_buffer_predisable(struct iio_dev *indio_dev)
{
	return bmp_enable_disable(indio_dev, false);
}

static const struct iio_buffer_setup_ops bmp_buffer_setup_ops = {
	.preenable = iio_sw_buffer_preenable,
	.postenable = bmp_buffer_postenable,
	.predisable = bmp_buffer_predisable,
};

static void bmp_work_func(struct work_struct *work)
{
	struct bmp_client_data *client_data =
		container_of((struct delayed_work *)work,
		struct bmp_client_data, work);
	unsigned long delay = msecs_to_jiffies(client_data->delay);
	unsigned long j1 = jiffies;
	u32 pressure;
	int status;
	struct bmp_el_data el_data;
	struct timespec ts;

	get_monotonic_boottime(&ts);
	el_data.timestamp = timespec_to_ns(&ts);

	mutex_lock(&client_data->lock);
	status = bmp_get_pressure(client_data, &pressure);
	client_data->pressure = pressure;

	el_data.data = pressure;
	if (status == 0) {
		status = iio_push_to_buffers(client_data->idev, (unsigned char *)&el_data);
		if (status < 0)
			dev_err(client_data->dev, "failed to push pressure "
				"data to buffer, err=%d\n", status);
	}

	schedule_delayed_work(&client_data->work, delay-(jiffies-j1));

	mutex_unlock(&client_data->lock);
}

static int bmp_init_client(struct bmp_client_data *data)
{
	int status;

	data->device.bus_read = data->data_bus.bops->bus_read;
	data->device.bus_write = data->data_bus.bops->bus_write;
	data->device.delay_msec = bmp_delay;

	status = BMP_CALL_API(init)(&data->device);
	if (status)
		return status;

	mutex_init(&data->lock);

	data->delay  = BMP_DELAY_DEFAULT;
	data->enable = 0;
	data->selftest = BMP_SELFTEST_NO_ACTION;

	status = bmp_set_op_mode(data, BMP_VAL_NAME(SLEEP_MODE));
	if (status)
		return status;

	status = bmp_set_filter(data, BMP_FILTER_DEFAULT);
	if (status)
		return status;

	status = bmp_set_standbydur(data, BMP_STANDBYDUR_DEFAULT);
	if (status)
		return status;

	status = bmp_set_workmode(data, BMP_WORKMODE_DEFAULT);
	return status;
}

static int bmp_iio_buffer_setup(struct iio_dev *indio_dev,
			const struct iio_buffer_setup_ops *setup_ops)
{
	int ret;

	indio_dev->buffer = iio_kfifo_allocate(indio_dev);
	if (!indio_dev->buffer) {
		ret = -ENOMEM;
		goto error_ret;
	}

	if (setup_ops)
		indio_dev->setup_ops = setup_ops;
	else
		goto error_kfifo_free;

	ret = iio_buffer_register(indio_dev,
				  indio_dev->channels,
				  indio_dev->num_channels);
	if (ret)
		goto error_kfifo_free;

	return 0;

error_kfifo_free:
	iio_kfifo_free(indio_dev->buffer);
error_ret:
	return ret;
}

static int bmp_iio_buffer_clean(struct iio_dev *indio_dev)
{
	iio_buffer_unregister(indio_dev);
	iio_kfifo_free(indio_dev->buffer);

	return 0;
}

int bmp_probe(struct device *dev, struct bmp_data_bus *data_bus)
{
	struct bmp_client_data *data;
	struct iio_dev *indio_dev;
	int err = 0;

	if (!dev || !data_bus) {
		err = -EINVAL;
		goto exit;
	}

	err = bmp_check_chip_id(data_bus);
	if (err) {
		PERR("Bosch Sensortec Device not found, chip id mismatch!\n");
		goto exit;
	} else {
		PNOTICE("Bosch Sensortec Device %s detected.\n", BMP_NAME);
	}

	indio_dev = iio_device_alloc(sizeof(*data));
	if (!indio_dev) {
		err = -ENOMEM;
		goto exit;
	}

	indio_dev->channels = bmp_iio_channels;
	indio_dev->num_channels = ARRAY_SIZE(bmp_iio_channels);
	indio_dev->dev.parent = dev;
	indio_dev->info = &bmp_iio_info;
	indio_dev->name = BMP_IIO_NAME;
	indio_dev->modes = INDIO_BUFFER_HARDWARE;

	data = iio_priv(indio_dev);
	data->data_bus = *data_bus;
	data->dev = dev;
	data->idev = indio_dev;

	err = bmp_init_client(data);
	if (err != 0)
		goto exit_free_iio;

	err = bmp_iio_buffer_setup(indio_dev, &bmp_buffer_setup_ops);
	if (err)
		goto exit_free_iio;

	err = iio_device_register(indio_dev);
	if (err) {
		dev_err(dev, "BMP IIO register fail\n");
		goto exit_free_buffer;
	}

	INIT_DELAYED_WORK(&data->work, bmp_work_func);

#ifdef CONFIG_HAS_EARLYSUSPEND
	data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	data->early_suspend.suspend = bmp_early_suspend;
	data->early_suspend.resume = bmp_late_resume;
	register_early_suspend(&data->early_suspend);
#endif

	PINFO("Succesfully probe sensor %s\n", BMP_NAME);
	return 0;

exit_free_buffer:
	bmp_iio_buffer_clean(indio_dev);
exit_free_iio:
	data->idev = NULL;
	iio_device_free(indio_dev);
exit:
	return err;
}
EXPORT_SYMBOL(bmp_probe);

int bmp_remove(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&data->early_suspend);
#endif
	bmp_iio_buffer_clean(indio_dev);
	iio_device_free(indio_dev);

	return 0;
}
EXPORT_SYMBOL(bmp_remove);

#ifdef CONFIG_PM
int bmp_disable(struct device *dev)
{
	return 0;
}
EXPORT_SYMBOL(bmp_disable);

int bmp_enable(struct device *dev)
{
	return 0;
}
EXPORT_SYMBOL(bmp_enable);
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void bmp_early_suspend(struct early_suspend *h)
{
	struct bmp_client_data *data =
		container_of(h, struct bmp_client_data, early_suspend);

	mutex_lock(&data->lock);
	if (data->enable) {
		cancel_delayed_work_sync(&data->work);
		bmp_set_op_mode(data, BMP_VAL_NAME(SLEEP_MODE));
		#ifdef CONFIG_PM
		(void) bmp_disable(data->dev);
		#endif
	}
	mutex_unlock(&data->lock);
}

static void bmp_late_resume(struct early_suspend *h)
{
	struct bmp_client_data *data =
		container_of(h, struct bmp_client_data, early_suspend);

	mutex_lock(&data->lock);
	if (data->enable) {
		#ifdef CONFIG_PM
		(void) bmp_enable(data->dev);
		#endif
		bmp_set_op_mode(data, BMP_VAL_NAME(NORMAL_MODE));
		schedule_delayed_work(&data->work,
			msecs_to_jiffies(data->delay));
	}
	mutex_unlock(&data->lock);
}
#endif

MODULE_DESCRIPTION("BMP280 DRIVER CORE");
MODULE_LICENSE("GPLv2");
