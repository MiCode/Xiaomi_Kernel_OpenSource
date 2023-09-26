/*
 * Copyright(C)2014 MediaTek Inc.
 * Modification based on code covered by the below mentioned copyright
 * and/or permission notice(S).
 */

/* ITG1010 motion sensor driver
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
 */

#include "ITG1010.h"
#include "cust_gyro.h"
#include "gyroscope.h"

#define DEBUG 0
/*----------------------------------------------------------------------------*/
#define INV_GYRO_AUTO_CALI 1
#define ITG1010_DEFAULT_FS ITG1010_FS_1000
#define ITG1010_DEFAULT_LSB ITG1010_FS_1000_LSB
#define CONFIG_ITG1010_LOWPASS /*apply low pass filter on output*/

#define ITG1010_AXIS_X 0
#define ITG1010_AXIS_Y 1
#define ITG1010_AXIS_Z 2
#define ITG1010_AXES_NUM 3
#define ITG1010_DATA_LEN 6
#define ITG1010_DEV_NAME "ITG-1010A"

int packet_thresh = 75; /* 600 ms / 8ms/sample */
/*----------------------------------------------------------------------------*/
static int ITG1010_init_flag = -1;

struct platform_device *gyroPltFmDev;

static const struct i2c_device_id ITG1010_i2c_id[] = { { ITG1010_DEV_NAME, 0 },
						       {} };
/* static struct i2c_board_info __initdata i2c_ITG1010={
 *I2C_BOARD_INFO(ITG1010_DEV_NAME,
 *(ITG1010_I2C_SLAVE_ADDR>>1))};
 */

static int ITG1010_i2c_probe(struct i2c_client *client,
			     const struct i2c_device_id *id);
static int ITG1010_i2c_remove(struct i2c_client *client);
static int ITG1010_i2c_detect(struct i2c_client *client,
			      struct i2c_board_info *info);
static int ITG1010_suspend(struct device *dev);
static int ITG1010_resume(struct device *dev);
static int ITG1010_local_init(struct platform_device *pdev);
static int ITG1010_remove(void);
static int ITG1010_flush(void);

static struct gyro_init_info ITG1010_init_info = {
	.name = "ITG1010GY",
	.init = ITG1010_local_init,
	.uninit = ITG1010_remove,
};
/*----------------------------------------------------------------------------*/
enum GYRO_TRC {
	GYRO_TRC_FILTER = 0x01,
	GYRO_TRC_RAWDATA = 0x02,
	GYRO_TRC_IOCTL = 0x04,
	GYRO_TRC_CALI = 0X08,
	GYRO_TRC_INFO = 0X10,
	GYRO_TRC_DATA = 0X20,
};
/*----------------------------------------------------------------------------*/
struct scale_factor {
	u8 whole;
	u8 fraction;
};
/*----------------------------------------------------------------------------*/
struct data_resolution {
	struct scale_factor scalefactor;
	int sensitivity;
};
/*----------------------------------------------------------------------------*/
#define C_MAX_FIR_LENGTH (32)
/*----------------------------------------------------------------------------*/
struct data_filter {
	s16 raw[C_MAX_FIR_LENGTH][ITG1010_AXES_NUM];
	int sum[ITG1010_AXES_NUM];
	int num;
	int idx;
};
/*----------------------------------------------------------------------------*/
struct ITG1010_i2c_data {
	struct i2c_client *client;
	struct gyro_hw hw;
	struct hwmsen_convert cvt;

	/*misc*/
	struct data_resolution *reso;
	atomic_t trace;
	atomic_t suspend;
	atomic_t selftest;
	atomic_t filter;
	s16 cali_sw[ITG1010_AXES_NUM + 1];

	/*data*/
	s8 offset[ITG1010_AXES_NUM + 1]; /*+1: for 4-byte alignment*/
	s16 data[ITG1010_AXES_NUM + 1];

	atomic_t first_enable;
#if defined(CONFIG_ITG1010_LOWPASS)
	atomic_t firlen;
	atomic_t fir_en;
	struct data_filter fir;
#endif

#if INV_GYRO_AUTO_CALI == 1
	s16 inv_cali_raw[ITG1010_AXES_NUM + 1];
	s16 temperature;
	struct mutex temperature_mutex; /* for temperature protection */
	struct mutex raw_data_mutex;    /* for inv_cali_raw[] protection */
#endif
	bool flush;
};
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_OF
static const struct of_device_id gyro_of_match[] = {
	{.compatible = "mediatek,gyro" }, {},
};
#endif

#ifdef CONFIG_PM_SLEEP
static const struct dev_pm_ops ITG1010_i2c_pm_ops = { SET_SYSTEM_SLEEP_PM_OPS(
	ITG1010_suspend, ITG1010_resume) };
#endif

static struct i2c_driver ITG1010_i2c_driver = {
	.driver = {
		.name = ITG1010_DEV_NAME,
#ifdef CONFIG_PM_SLEEP
		.pm = &ITG1010_i2c_pm_ops,
#endif
#ifdef CONFIG_OF
		.of_match_table = gyro_of_match,
#endif
	    },
	.probe = ITG1010_i2c_probe,
	.remove = ITG1010_i2c_remove,
	.detect = ITG1010_i2c_detect,
	.id_table = ITG1010_i2c_id,
};

/*----------------------------------------------------------------------------*/
static struct i2c_client *ITG1010_i2c_client;
static struct ITG1010_i2c_data *obj_i2c_data;
static bool sensor_power;
static DEFINE_MUTEX(ITG1010_i2c_mutex);
#define C_I2C_FIFO_SIZE 8

/**************I2C operate API*****************************/
static int ITG1010_i2c_read_block(struct i2c_client *client, u8 addr, u8 *data,
				  u8 len)
{
	u8 beg = addr;
	int err;
	struct i2c_msg msgs[2] = { { 0 }, { 0 } };

	if (!client)
		return -EINVAL;
	else if (len > C_I2C_FIFO_SIZE) {
		pr_err_ratelimited("[Gyro]%s:length %d exceeds %d\n",
			__func__, len, C_I2C_FIFO_SIZE);
		mutex_unlock(&ITG1010_i2c_mutex);
		return -EINVAL;
	}

	mutex_lock(&ITG1010_i2c_mutex);

	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = &beg;

	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = data;

	err = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (err != 2) {
		pr_err_ratelimited("[Gyro]%s:i2c_transfer error: (%d %p %d) %d\n",
			__func__, addr, data,
			len, err);
		err = -EIO;
	} else
		err = 0;

	mutex_unlock(&ITG1010_i2c_mutex);
	return err;
}

static int ITG1010_i2c_write_block(struct i2c_client *client, u8 addr, u8 *data,
				   u8 len)
{
	/*because address also occupies one byte,
	 *the maximum length for write is 7 bytes
	 */
	int err;
	unsigned int idx, num;
	char buf[C_I2C_FIFO_SIZE];

	err = 0;
	mutex_lock(&ITG1010_i2c_mutex);
	if (!client) {
		mutex_unlock(&ITG1010_i2c_mutex);
		return -EINVAL;
	} else if (len >= C_I2C_FIFO_SIZE) {
		pr_err_ratelimited("[Gyro]%s:length %d exceeds %d\n",
			__func__, len, C_I2C_FIFO_SIZE);
		mutex_unlock(&ITG1010_i2c_mutex);
		return -EINVAL;
	}

	num = 0;
	buf[num++] = addr;
	for (idx = 0; idx < len; idx++)
		buf[num++] = data[idx];

	err = i2c_master_send(client, buf, num);
	if (err < 0) {
		pr_err_ratelimited("[Gyro]%s:send command error!!\n", __func__);
		mutex_unlock(&ITG1010_i2c_mutex);
		return -EFAULT;
	}
	mutex_unlock(&ITG1010_i2c_mutex);
	return err;
}

static unsigned int power_on;
#if INV_GYRO_AUTO_CALI == 1

#define INV_DAEMON_CLASS_NAME "invensense_daemon_class"
#define INV_DAEMON_DEVICE_NAME "invensense_daemon_device"

static struct class *inv_daemon_class;
static struct device *inv_daemon_device;
static int inv_mpl_motion_state;
static int inv_gyro_power_state;
static ssize_t inv_mpl_motion_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	unsigned int result = 0;
	unsigned long data = 0;
	char *envp[2];

	result = kstrtoul(buf, 10, &data);
	if (result)
		return result;

	if (data)
		envp[0] = "STATUS=MOTION";
	else
		envp[0] = "STATUS=NOMOTION";
	envp[1] = NULL;
	result = kobject_uevent_env(&dev->kobj, KOBJ_CHANGE, envp);

	inv_mpl_motion_state = data;

	return count;
}
static ssize_t inv_mpl_motion_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", inv_mpl_motion_state);
}

static ssize_t inv_gyro_data_ready_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	sysfs_notify(&dev->kobj, NULL, "inv_gyro_data_ready");
	return count;
}
static ssize_t inv_gyro_data_ready_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return sprintf(buf, "1\n");
}

static ssize_t inv_gyro_power_state_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	unsigned int result = 0;
	unsigned long data = 0;

	result = kstrtoul(buf, 10, &data);
	if (result)
		return result;

	inv_gyro_power_state = data;

	sysfs_notify(&dev->kobj, NULL, "inv_gyro_power_state");
	return count;
}
static ssize_t inv_gyro_power_state_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	return sprintf(buf, "%d\n", inv_gyro_power_state);
}

static DEVICE_ATTR(inv_mpl_motion, 0644, inv_mpl_motion_show,
		   inv_mpl_motion_store);
static DEVICE_ATTR(inv_gyro_data_ready, 0644,
		   inv_gyro_data_ready_show, inv_gyro_data_ready_store);
static DEVICE_ATTR(inv_gyro_power_state, 0644,
		   inv_gyro_power_state_show, inv_gyro_power_state_store);

static struct device_attribute *inv_daemon_dev_attributes[] = {
	&dev_attr_inv_mpl_motion, &dev_attr_inv_gyro_data_ready,
	&dev_attr_inv_gyro_power_state,
};
#endif /* #if INV_GYRO_AUTO_CALI == 1 */

int ITG1010_gyro_power(void) { return power_on; }
EXPORT_SYMBOL(ITG1010_gyro_power);

int ITG1010_gyro_mode(void) { return sensor_power; }
EXPORT_SYMBOL(ITG1010_gyro_mode);
/* ----------------------------------------------------------
 */
static int ITG1010_SetPowerMode(struct i2c_client *client, bool enable)
{
	u8 databuf[2] = { 0 };
	int res = 0;

	if (enable == sensor_power) {
		pr_debug("Gyro:Sensor power status is newest!\n");
		return ITG1010_SUCCESS;
	}

	if (ITG1010_i2c_read_block(client, ITG1010_REG_PWR_CTL, databuf, 1)) {
		pr_err_ratelimited("[Gyro]%s:read power ctl register err!\n",
			__func__);
		return ITG1010_ERR_I2C;
	}

	databuf[0] &= ~ITG1010_SLEEP;
	if (enable == FALSE)
		databuf[0] |= ITG1010_SLEEP;

	res = ITG1010_i2c_write_block(client, ITG1010_REG_PWR_CTL, databuf, 1);
	if (res <= 0) {
		pr_debug("Gyro:set power mode failed!\n");
		return ITG1010_ERR_I2C;
	}
	/* pr_debug("Gyro:set power mode ok %d!\n", enable); */

	sensor_power = enable;
	if (obj_i2c_data->flush) {
		if (sensor_power) {
			pr_debug("Gyro:is not flush, will call ITG1010_flush in setPowerMode\n");
			ITG1010_flush();
		} else
			obj_i2c_data->flush = false;
	}
	return ITG1010_SUCCESS;
}

/*----------------------------------------------------------------------------*/
static int ITG1010_SetDataFormat(struct i2c_client *client, u8 dataformat)
{
	u8 databuf[2] = { 0 };
	int res = 0;

	/*pr_info("%s\n", __func__);*/

	databuf[0] = dataformat;
	res = ITG1010_i2c_write_block(client, ITG1010_REG_CFG, databuf, 1);
	if (res <= 0)
		return ITG1010_ERR_I2C;

	/* read sample rate after written for test */
	udelay(500);

	res = ITG1010_i2c_read_block(client, ITG1010_REG_CFG, databuf, 1);
	if (res != 0) {
		pr_err_ratelimited("[Gyro]%s:read data format register err!\n",
			__func__);
		return ITG1010_ERR_I2C;
	}
	/* pr_debug("Gyro:read  data format: 0x%x\n", databuf[0]); */

	return ITG1010_SUCCESS;
}

static int ITG1010_SetFullScale(struct i2c_client *client, u8 dataformat)
{
	u8 databuf[2] = { 0 };
	int res = 0;

	/*pr_info("%s\n", __func__);*/

	databuf[0] = dataformat;
	res = ITG1010_i2c_write_block(client, ITG1010_REG_GYRO_CFG, databuf, 1);
	if (res <= 0)
		return ITG1010_ERR_I2C;

	/* read sample rate after written for test */
	udelay(500);
	res = ITG1010_i2c_read_block(client, ITG1010_REG_GYRO_CFG, databuf, 1);
	if (res != 0) {
		pr_err_ratelimited("[Gyro]%s:read data format register err!\n",
			__func__);
		return ITG1010_ERR_I2C;
	}
	pr_debug("Gyro:read  data format: 0x%x\n", databuf[0]);

	return ITG1010_SUCCESS;
}

/* set the sample rate */
static int ITG1010_SetSampleRate(struct i2c_client *client, int sample_rate)
{
	u8 databuf[2] = { 0 };
	int rate_div = 0;
	int res = 0;

	/*pr_info("%s\n", __func__);*/

	res = ITG1010_i2c_read_block(client, ITG1010_REG_CFG, databuf, 1);
	if (res != 0) {
		pr_err_ratelimited("[Gyro]%s:read gyro data format register err!\n",
			__func__);
		return ITG1010_ERR_I2C;
	}


	if ((databuf[0] & 0x07) == 0) /* Analog sample rate is 8KHz */
		rate_div = 8 * 1024 / sample_rate - 1;
	else /* 1kHz */
		rate_div = 1024 / sample_rate - 1;

	if (rate_div > 255) /* rate_div: 0 to 255; */
		rate_div = 255;
	else if (rate_div < 0)
		rate_div = 0;

	databuf[0] = rate_div;
	res =
	    ITG1010_i2c_write_block(client, ITG1010_REG_SAMRT_DIV, databuf, 1);
	if (res <= 0) {
		pr_err_ratelimited("[Gyro]%s:write sample rate register err!\n",
			__func__);
		return ITG1010_ERR_I2C;
	}

	/* read sample div after written for test */
	udelay(500);
	res = ITG1010_i2c_read_block(client, ITG1010_REG_SAMRT_DIV, databuf, 1);
	if (res != 0) {
		pr_err_ratelimited("[Gyro]%s:read gyro sample rate register err!\n",
			__func__);
		return ITG1010_ERR_I2C;
	}
	/* pr_debug("Gyro:read  gyro sample rate: 0x%x\n", databuf[0]); */

	return ITG1010_SUCCESS;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static int ITG1010_ReadGyroData(struct i2c_client *client, char *buf,
				int bufsize)
{
	char databuf[6];
	int data[3];
	int ret = 0;
	struct ITG1010_i2c_data *obj = i2c_get_clientdata(client);

	if (sensor_power == false) {
		ITG1010_SetPowerMode(client, true);
		mdelay(50);
	}

#if INV_GYRO_AUTO_CALI == 1
	ret = ITG1010_i2c_read_block(client, ITG1010_REG_TEMPH, databuf, 2);
	if (ret != 0) {
		pr_err_ratelimited("[Gyro]%s:ITG1010 read temperature data  error\n",
			__func__);
		return -2;
	}
	mutex_lock(&obj->temperature_mutex);
	obj->temperature = ((s16)((databuf[1]) | (databuf[0] << 8)));
	mutex_unlock(&obj->temperature_mutex);
#endif

	ret = ITG1010_i2c_read_block(client, ITG1010_REG_GYRO_XH, databuf, 6);
	if (ret != 0) {
		pr_err_ratelimited("[Gyro]%s:ITG1010 read gyroscope data  error\n",
			__func__);
		return -2;
	}

	obj->data[ITG1010_AXIS_X] = ((s16)((databuf[ITG1010_AXIS_X * 2 + 1]) |
					   (databuf[ITG1010_AXIS_X * 2] << 8)));
	obj->data[ITG1010_AXIS_Y] = ((s16)((databuf[ITG1010_AXIS_Y * 2 + 1]) |
					   (databuf[ITG1010_AXIS_Y * 2] << 8)));
	obj->data[ITG1010_AXIS_Z] = ((s16)((databuf[ITG1010_AXIS_Z * 2 + 1]) |
					   (databuf[ITG1010_AXIS_Z * 2] << 8)));
#if DEBUG
	if (atomic_read(&obj->trace) & GYRO_TRC_RAWDATA) {
		pr_debug("Gyro:read gyro register: %d, %d, %d, %d, %d, %d",
			 databuf[0], databuf[1], databuf[2], databuf[3],
			 databuf[4], databuf[5]);
		pr_debug("Gyro:get gyro raw data (0x%08X, 0x%08X, 0x%08X) -> (%5d, %5d, %5d)\n",
			 obj->data[ITG1010_AXIS_X], obj->data[ITG1010_AXIS_Y],
			 obj->data[ITG1010_AXIS_Z], obj->data[ITG1010_AXIS_X],
			 obj->data[ITG1010_AXIS_Y], obj->data[ITG1010_AXIS_Z]);
	}
#endif
#if INV_GYRO_AUTO_CALI == 1
	mutex_lock(&obj->raw_data_mutex);
	/*remap coordinate*/
	obj->inv_cali_raw[obj->cvt.map[ITG1010_AXIS_X]] =
	    obj->cvt.sign[ITG1010_AXIS_X] * obj->data[ITG1010_AXIS_X];
	obj->inv_cali_raw[obj->cvt.map[ITG1010_AXIS_Y]] =
	    obj->cvt.sign[ITG1010_AXIS_Y] * obj->data[ITG1010_AXIS_Y];
	obj->inv_cali_raw[obj->cvt.map[ITG1010_AXIS_Z]] =
	    obj->cvt.sign[ITG1010_AXIS_Z] * obj->data[ITG1010_AXIS_Z];
	mutex_unlock(&obj->raw_data_mutex);
#endif
	obj->data[ITG1010_AXIS_X] =
	    obj->data[ITG1010_AXIS_X] + obj->cali_sw[ITG1010_AXIS_X];
	obj->data[ITG1010_AXIS_Y] =
	    obj->data[ITG1010_AXIS_Y] + obj->cali_sw[ITG1010_AXIS_Y];
	obj->data[ITG1010_AXIS_Z] =
	    obj->data[ITG1010_AXIS_Z] + obj->cali_sw[ITG1010_AXIS_Z];

	/*remap coordinate*/
	data[obj->cvt.map[ITG1010_AXIS_X]] =
	    obj->cvt.sign[ITG1010_AXIS_X] * obj->data[ITG1010_AXIS_X];
	data[obj->cvt.map[ITG1010_AXIS_Y]] =
	    obj->cvt.sign[ITG1010_AXIS_Y] * obj->data[ITG1010_AXIS_Y];
	data[obj->cvt.map[ITG1010_AXIS_Z]] =
	    obj->cvt.sign[ITG1010_AXIS_Z] * obj->data[ITG1010_AXIS_Z];

	/* Out put the degree/second(o/s) */
	data[ITG1010_AXIS_X] =
	    data[ITG1010_AXIS_X] * ITG1010_FS_MAX_LSB / ITG1010_DEFAULT_LSB;
	data[ITG1010_AXIS_Y] =
	    data[ITG1010_AXIS_Y] * ITG1010_FS_MAX_LSB / ITG1010_DEFAULT_LSB;
	data[ITG1010_AXIS_Z] =
	    data[ITG1010_AXIS_Z] * ITG1010_FS_MAX_LSB / ITG1010_DEFAULT_LSB;

	sprintf(buf, "%04x %04x %04x", data[ITG1010_AXIS_X],
		data[ITG1010_AXIS_Y], data[ITG1010_AXIS_Z]);

#if DEBUG
	if (atomic_read(&obj->trace) & GYRO_TRC_DATA)
		pr_debug("Gyro:get gyro data packet:[%d %d %d]\n",
			data[0], data[1], data[2]);
#endif

	return 0;
}

static int ITG1010_ReadChipInfo(struct i2c_client *client, char *buf,
				int bufsize)
{
	u8 databuf[10];
	int ret;
	memset(databuf, 0, sizeof(u8) * 10);

	if ((buf == NULL) || (bufsize <= 30))
		return -1;

	if (client == NULL) {
		*buf = 0;
		return -2;
	}

	ret = sprintf(buf, "ITG1010 Chip");
	if (ret < 0)
		pr_debug("%s:sprintf buf failed:%d\n", __func__, ret);
	return 0;
}
/*----------------------------------------------------------------------------*/
static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = ITG1010_i2c_client;
	char strbuf[ITG1010_BUFSIZE];

	if (client == NULL) {
		pr_err_ratelimited("[Gyro]%s:i2c client is null!!\n", __func__);
		return 0;
	}

	ITG1010_ReadChipInfo(client, strbuf, ITG1010_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}
/*----------------------------------------------------------------------------*/
static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = ITG1010_i2c_client;
	char strbuf[ITG1010_BUFSIZE];

	if (client == NULL) {
		pr_err_ratelimited("[Gyro]%s:i2c client is null!!\n", __func__);
		return 0;
	}

	ITG1010_ReadGyroData(client, strbuf, ITG1010_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}

/*----------------------------------------------------------------------------*/
static ssize_t show_trace_value(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	struct ITG1010_i2c_data *obj = obj_i2c_data;

	if (obj == NULL) {
		pr_err_ratelimited("[Gyro]%s:i2c_data obj is null!!\n",
			__func__);
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&obj->trace));
	return res;
}
/*----------------------------------------------------------------------------*/
static ssize_t store_trace_value(struct device_driver *ddri, const char *buf,
				 size_t count)
{
	struct ITG1010_i2c_data *obj = obj_i2c_data;
	int trace;

	if (obj == NULL) {
		pr_err_ratelimited("[Gyro]%s:i2c_data obj is null!!\n",
			__func__);
		return 0;
	}

	if (sscanf(buf, "0x%x", &trace) == 1)
		atomic_set(&obj->trace, trace);
	else
		pr_err_ratelimited("[Gyro]%s:invalid content: '%s', length = %zu\n",
			__func__, buf, count);

	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t show_status_value(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	struct ITG1010_i2c_data *obj = obj_i2c_data;

	if (obj == NULL) {
		pr_err_ratelimited("[Gyro]%s:i2c_data obj is null!!\n",
			__func__);
		return 0;
	}

	len += snprintf(buf + len, PAGE_SIZE - len, "CUST: %d %d (%d %d)\n",
			obj->hw.i2c_num, obj->hw.direction, obj->hw.power_id,
			obj->hw.power_vol);

	return len;
}

static ssize_t show_chip_orientation(struct device_driver *ddri, char *buf)
{
	ssize_t _tLength = 0;
	struct ITG1010_i2c_data *obj = obj_i2c_data;

	if (obj == NULL) {
		pr_err_ratelimited("[Gyro]%s:i2c_data obj is null!!\n",
			__func__);
		return 0;
	}

	pr_debug("Gyro:[%s] default direction: %d\n",
			__func__, obj->hw.direction);

	_tLength = snprintf(buf, PAGE_SIZE, "default direction = %d\n",
			    obj->hw.direction);

	return _tLength;
}

static ssize_t store_chip_orientation(struct device_driver *ddri,
				      const char *buf, size_t tCount)
{
	int _nDirection = 0;
	int ret = 0;
	struct ITG1010_i2c_data *_pt_i2c_obj = obj_i2c_data;

	if (_pt_i2c_obj == NULL)
		return 0;

	ret = kstrtoint(buf, 10, &_nDirection);
	if (ret == 0) {
		if (hwmsen_get_convert(_nDirection, &_pt_i2c_obj->cvt)) {
			pr_err_ratelimited("[Gyro]%s:ERR: fail to set direction\n",
				__func__);
		}
	}

	pr_debug("Gyro:[%s] set direction: %d\n", __func__, _nDirection);

	return tCount;
}

static ssize_t show_power_status(struct device_driver *ddri, char *buf)
{
	ssize_t res = 0;
	u8 uData = 0;
	struct ITG1010_i2c_data *obj = obj_i2c_data;

	if (obj == NULL) {
		pr_err_ratelimited("[Gyro]%s:i2c_data obj is null!!\n",
			__func__);
		return 0;
	}
	ITG1010_i2c_read_block(obj->client, ITG1010_REG_PWR_CTL, &uData, 1);

	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", uData);
	if (res < 0)
		pr_debug("%s:PAGE_SIZE snprintf fail:%d\n", __func__, res);
	return res;
}

static ssize_t show_regiter_map(struct device_driver *ddri, char *buf)
{
	u8 _bIndex = 0;
	u8 _baRegMap[34] = { 0x04, 0x05, 0x07, 0x08, 0xA,  0xB,  0x13,
			     0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A,
			     0x1B, 0x23, 0x37, 0x38, 0x3A, 0x41, 0x42,
			     0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x6A,
			     0x6B, 0x6C, 0x72, 0x73, 0x74, 0x75 };
	u8 _baRegValue[2] = { 0 };
	ssize_t _tLength = 0;
	struct ITG1010_i2c_data *obj = obj_i2c_data;

	for (_bIndex = 0; _bIndex < 34; _bIndex++) {
		ITG1010_i2c_read_block(obj->client, _baRegMap[_bIndex],
				       &_baRegValue[0], 1);
		_tLength += snprintf((buf + _tLength), (PAGE_SIZE - _tLength),
				     "Reg[0x%02X]: 0x%02X\n",
				     _baRegMap[_bIndex], _baRegValue[0]);
	}

	return _tLength;
}

/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(chipinfo, 0444, show_chipinfo_value, NULL);
static DRIVER_ATTR(sensordata, 0444, show_sensordata_value, NULL);
static DRIVER_ATTR(trace, 0644, show_trace_value,
		   store_trace_value);
static DRIVER_ATTR(status, 0444, show_status_value, NULL);
static DRIVER_ATTR(orientation, 0644, show_chip_orientation,
		   store_chip_orientation);
static DRIVER_ATTR(power, 0444, show_power_status, NULL);
static DRIVER_ATTR(regmap, 0444, show_regiter_map, NULL);

/*----------------------------------------------------------------------------*/
static struct driver_attribute *ITG1010_attr_list[] = {
	&driver_attr_chipinfo,   /*chip information*/
	&driver_attr_sensordata, /*dump sensor data*/
	&driver_attr_trace,      /*trace log*/
	&driver_attr_status,     &driver_attr_orientation,
	&driver_attr_power,      &driver_attr_regmap,
};
/*----------------------------------------------------------------------------*/
static int ITG1010_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(ARRAY_SIZE(ITG1010_attr_list));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, ITG1010_attr_list[idx]);
		if (err != 0) {
			pr_err_ratelimited("[Gyro]%s:driver_create_file (%s) = %d\n",
				__func__, ITG1010_attr_list[idx]->attr.name,
				err);
			break;
		}
	}
	return err;
}
/*----------------------------------------------------------------------------*/
static int ITG1010_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(ARRAY_SIZE(ITG1010_attr_list));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, ITG1010_attr_list[idx]);

	return err;
}

/*----------------------------------------------------------------------------*/
static int ITG1010_gpio_config(void)
{
	int ret;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_default;
	struct pinctrl_state *pins_cfg;

	pinctrl = devm_pinctrl_get(&gyroPltFmDev->dev);
	if (IS_ERR(pinctrl)) {
		ret = PTR_ERR(pinctrl);
		pr_err_ratelimited("[Gyro]%s:Cannot find gyro pinctrl!\n",
			__func__);
		return ret;
	}
	pins_default = pinctrl_lookup_state(pinctrl, "pin_default");
	if (IS_ERR(pins_default)) {
		ret = PTR_ERR(pins_default);
		pr_err_ratelimited("[Gyro]%s:Cannot find gyro pinctrl default!\n",
			__func__);
		return ret;
	}

	pins_cfg = pinctrl_lookup_state(pinctrl, "pin_cfg");
	if (IS_ERR(pins_cfg)) {
		ret = PTR_ERR(pins_cfg);
		pr_err_ratelimited("[Gyro]%s:Cannot find gyro pinctrl pin_cfg!\n",
			__func__);
		return ret;
	}
	pinctrl_select_state(pinctrl, pins_cfg);

	return 0;
}
static int ITG1010_init_client(struct i2c_client *client, bool enable)
{
	struct ITG1010_i2c_data *obj = i2c_get_clientdata(client);
	int res = 0;

	ITG1010_gpio_config();

	res = ITG1010_SetPowerMode(client, true);
	if (res != ITG1010_SUCCESS)
		return res;

	/* The range should at least be 17.45 rad/s (ie: ~1000 deg/s). */
	res = ITG1010_SetDataFormat(client,
				    (ITG1010_SYNC_GYROX << ITG1010_EXT_SYNC) |
					ITG1010_RATE_1K_LPFB_188HZ);
	res = ITG1010_SetFullScale(client,
				   (ITG1010_DEFAULT_FS << ITG1010_FS_RANGE));
	if (res != ITG1010_SUCCESS)
		return res;

	/* Set 125HZ sample rate */
	res = ITG1010_SetSampleRate(client, 125);
	if (res != ITG1010_SUCCESS)
		return res;

	res = ITG1010_SetPowerMode(client, enable);
	if (res != ITG1010_SUCCESS)
		return res;

/* pr_debug("Gyro:ITG1010_init_client OK!\n"); */

#ifdef CONFIG_ITG1010_LOWPASS
	memset(&obj->fir, 0x00, sizeof(obj->fir));
#endif

	return ITG1010_SUCCESS;
}

/*----------------------------------------------------------------------------*/
int ITG1010_operate(void *self, uint32_t command, void *buff_in, int size_in,
		    void *buff_out, int size_out, int *actualout)
{
	int err = 0;
	int ret = 0;
	int value;
	struct ITG1010_i2c_data *priv = (struct ITG1010_i2c_data *)self;
	struct hwm_sensor_data *gyro_data;
	char buff[ITG1010_BUFSIZE];

	switch (command) {
	case SENSOR_DELAY:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			pr_err_ratelimited("[Gyro]%s:Set delay parameter error!\n",
				__func__);
			err = -EINVAL;
		}
		break;

	case SENSOR_ENABLE:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			pr_err_ratelimited("[Gyro]%s:Enable gyroscope parameter error!\n",
				__func__);
			err = -EINVAL;
		} else {
			value = *(int *)buff_in;
			if (((value == 0) && (sensor_power == false)) ||
			    ((value == 1) && (sensor_power == true)))
				pr_debug("Gyro:gyroscope device have updated!\n");
			else
				err = ITG1010_SetPowerMode(priv->client,
							   !sensor_power);
#if INV_GYRO_AUTO_CALI == 1
			inv_gyro_power_state = sensor_power;
			/* put this in where gyro power is changed, waking up
			 * mpu daemon
			 */
			sysfs_notify(&inv_daemon_device->kobj, NULL,
				     "inv_gyro_power_state");
#endif
		}
		break;

	case SENSOR_GET_DATA:
		if ((buff_out == NULL) ||
		    (size_out < sizeof(struct hwm_sensor_data))) {
			pr_err_ratelimited("[Gyro]%s:get gyroscope data parameter error!\n",
				__func__);
			err = -EINVAL;
		} else {
			gyro_data = (struct hwm_sensor_data *)buff_out;
			err = ITG1010_ReadGyroData(priv->client, buff,
						   ITG1010_BUFSIZE);
			if (!err) {
				ret = sscanf(buff, "%x %x %x",
					     &gyro_data->values[0],
					     &gyro_data->values[1],
					     &gyro_data->values[2]);
				gyro_data->status =
				    SENSOR_STATUS_ACCURACY_MEDIUM;
				gyro_data->value_divide = DEGREE_TO_RAD;
#if INV_GYRO_AUTO_CALI == 1
				/* put this in where gyro data is ready to
				 * report to hal, waking up mpu daemon
				 */
				sysfs_notify(&inv_daemon_device->kobj, NULL,
					     "inv_gyro_data_ready");
#endif
			}
		}
		break;

	default:
		pr_err_ratelimited("[Gyro]%s:gyroscope operate function no this parameter %d!\n",
			__func__, command);
		err = -1;
	}

	return err;
}
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_PM_SLEEP
static int ITG1010_suspend(struct device *dev)
{
	int err = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct ITG1010_i2c_data *obj = i2c_get_clientdata(client);

	/*pr_info("%s\n", __func__);*/

	if (obj == NULL) {
		pr_err_ratelimited("[Gyro]%s:null pointer!!\n", __func__);
		return -EINVAL;
	}
	atomic_set(&obj->suspend, 1);

	err = ITG1010_SetPowerMode(client, false);
	if (err <= 0)
		return err;
#if INV_GYRO_AUTO_CALI == 1
	inv_gyro_power_state = sensor_power;
	/* inv_gyro_power_state = 0; */
	/* put this in where gyro power is changed, waking up mpu daemon */
	sysfs_notify(&inv_daemon_device->kobj, NULL, "inv_gyro_power_state");
#endif
	return err;
}
/*----------------------------------------------------------------------------*/
static int ITG1010_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ITG1010_i2c_data *obj = i2c_get_clientdata(client);
	int err;

	pr_info("%s\n", __func__);

	if (obj == NULL) {
		pr_err_ratelimited("[Gyro]%s:null pointer!!\n", __func__);
		return -EINVAL;
	}

	err = ITG1010_init_client(client, false);
	if (err) {
		pr_err_ratelimited("[Gyro]%s:initialize client fail!!\n",
			__func__);
		return err;
	}
	atomic_set(&obj->suspend, 0);

	return 0;
}
#endif
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static int ITG1010_i2c_detect(struct i2c_client *client,
			      struct i2c_board_info *info)
{
	strlcpy(info->type, ITG1010_DEV_NAME, sizeof(info->type));
	return 0;
}

/* if use  this typ of enable , Gsensor should report inputEvent(x, y, z ,stats,
 * div) to HAL
 */
static int ITG1010_open_report_data(int open)
{
	/* should queuq work to report event if  is_report_input_direct=true */
	return 0;
}

/* if use  this typ of enable , Gsensor only enabled but not report inputEvent
 * to HAL
 */

static int ITG1010_enable_nodata(int en)
{
	int res = 0;
	int retry = 0;
	bool power = false;

	if (en == 1) {
		power = true;
		atomic_set(&obj_i2c_data->first_enable, true);
	} else if (en == 0) {
		power = false;
		atomic_set(&obj_i2c_data->first_enable, false);
	}

	for (retry = 0; retry < 3; retry++) {
		res = ITG1010_SetPowerMode(obj_i2c_data->client, power);
		if (res == 0) {
			pr_debug("Gyro:ITG1010_SetPowerMode done\n");
			break;
		}
		pr_debug("Gyro:ITG1010_SetPowerMode fail\n");
	}

	if (res != ITG1010_SUCCESS) {
		pr_debug("Gyro:ITG1010_SetPowerMode fail!\n");
		return -1;
	}
	pr_debug("Gyro:%s OK!\n", __func__);

	return 0;
}

static int ITG1010_set_delay(u64 ns) { return 0; }

static int ITG1010_get_data(int *x, int *y, int *z, int *status)
{
	char buff[ITG1010_BUFSIZE] = {0};
	int ret = 0;

	if (atomic_xchg(&obj_i2c_data->first_enable, false))
		msleep(50);
	ITG1010_ReadGyroData(obj_i2c_data->client, buff, ITG1010_BUFSIZE);

	ret = sscanf(buff, "%x %x %x", x, y, z);
	if (ret < 0)
		pr_info("%s:ITG1010_ReadGyroData sscanf err:%d\n", __func__, ret);
	*status = SENSOR_STATUS_ACCURACY_MEDIUM;

	return 0;
}

static int ITG1010_batch(int flag, int64_t samplingPeriodNs,
			 int64_t maxBatchReportLatencyNs)
{
	return 0;
}

static int ITG1010_flush(void)
{
	int err = 0;
	/*Only flush after sensor was enabled*/
	if (!sensor_power) {
		obj_i2c_data->flush = true;
		return 0;
	}
	err = gyro_flush_report();
	if (err >= 0)
		obj_i2c_data->flush = false;
	return err;
}

/*----------------------------------------------------------------------------*/
static int ITG1010_write_rel_calibration(struct ITG1010_i2c_data *obj,
					 int dat[ITG1010_AXES_NUM])
{
	obj->cali_sw[ITG1010_AXIS_X] =
	    obj->cvt.sign[ITG1010_AXIS_X] * dat[obj->cvt.map[ITG1010_AXIS_X]];
	obj->cali_sw[ITG1010_AXIS_Y] =
	    obj->cvt.sign[ITG1010_AXIS_Y] * dat[obj->cvt.map[ITG1010_AXIS_Y]];
	obj->cali_sw[ITG1010_AXIS_Z] =
	    obj->cvt.sign[ITG1010_AXIS_Z] * dat[obj->cvt.map[ITG1010_AXIS_Z]];
#if DEBUG
	if (atomic_read(&obj->trace) & GYRO_TRC_CALI) {
		pr_debug("Gyro:test  (%5d, %5d, %5d) ->(%5d, %5d, %5d)->(%5d, %5d, %5d))\n",
			 obj->cvt.sign[ITG1010_AXIS_X],
			 obj->cvt.sign[ITG1010_AXIS_Y],
			 obj->cvt.sign[ITG1010_AXIS_Z], dat[ITG1010_AXIS_X],
			 dat[ITG1010_AXIS_Y], dat[ITG1010_AXIS_Z],
			 obj->cvt.map[ITG1010_AXIS_X],
			 obj->cvt.map[ITG1010_AXIS_Y],
			 obj->cvt.map[ITG1010_AXIS_Z]);
		pr_debug("Gyro:write gyro calibration data  (%5d, %5d, %5d)\n",
			 obj->cali_sw[ITG1010_AXIS_X],
			 obj->cali_sw[ITG1010_AXIS_Y],
			 obj->cali_sw[ITG1010_AXIS_Z]);
	}
#endif
	return 0;
}

/*----------------------------------------------------------------------------*/
static int ITG1010_ResetCalibration(struct i2c_client *client)
{
	struct ITG1010_i2c_data *obj = i2c_get_clientdata(client);

	memset(obj->cali_sw, 0x00, sizeof(obj->cali_sw));
	return 0;
}
/*----------------------------------------------------------------------------*/
static int ITG1010_ReadCalibration(struct i2c_client *client,
				   int dat[ITG1010_AXES_NUM])
{
	struct ITG1010_i2c_data *obj = i2c_get_clientdata(client);

	dat[obj->cvt.map[ITG1010_AXIS_X]] =
	    obj->cvt.sign[ITG1010_AXIS_X] * obj->cali_sw[ITG1010_AXIS_X];
	dat[obj->cvt.map[ITG1010_AXIS_Y]] =
	    obj->cvt.sign[ITG1010_AXIS_Y] * obj->cali_sw[ITG1010_AXIS_Y];
	dat[obj->cvt.map[ITG1010_AXIS_Z]] =
	    obj->cvt.sign[ITG1010_AXIS_Z] * obj->cali_sw[ITG1010_AXIS_Z];

#if DEBUG
	if (atomic_read(&obj->trace) & GYRO_TRC_CALI) {
		pr_debug("Gyro:Read gyro calibration data  (%5d, %5d, %5d)\n",
			 dat[ITG1010_AXIS_X], dat[ITG1010_AXIS_Y],
			 dat[ITG1010_AXIS_Z]);
	}
#endif

	return 0;
}

/*----------------------------------------------------------------------------*/
static int ITG1010_WriteCalibration(struct i2c_client *client,
				    int dat[ITG1010_AXES_NUM])
{
	struct ITG1010_i2c_data *obj = i2c_get_clientdata(client);
	int cali[ITG1010_AXES_NUM];

	/*pr_info("%s\n", __func__);*/
	if (!obj || !dat) {
		pr_err_ratelimited("[Gyro]%s:null ptr!!\n", __func__);
		return -EINVAL;
	}
	cali[obj->cvt.map[ITG1010_AXIS_X]] =
	    obj->cvt.sign[ITG1010_AXIS_X] * obj->cali_sw[ITG1010_AXIS_X];
	cali[obj->cvt.map[ITG1010_AXIS_Y]] =
	    obj->cvt.sign[ITG1010_AXIS_Y] * obj->cali_sw[ITG1010_AXIS_Y];
	cali[obj->cvt.map[ITG1010_AXIS_Z]] =
	    obj->cvt.sign[ITG1010_AXIS_Z] * obj->cali_sw[ITG1010_AXIS_Z];
	cali[ITG1010_AXIS_X] += dat[ITG1010_AXIS_X];
	cali[ITG1010_AXIS_Y] += dat[ITG1010_AXIS_Y];
	cali[ITG1010_AXIS_Z] += dat[ITG1010_AXIS_Z];
#if DEBUG
	if (atomic_read(&obj->trace) & GYRO_TRC_CALI) {
		pr_debug("Gyro:write gyro calibration data  (%5d, %5d, %5d)-->(%5d, %5d, %5d)\n",
			 dat[ITG1010_AXIS_X], dat[ITG1010_AXIS_Y],
			 dat[ITG1010_AXIS_Z], cali[ITG1010_AXIS_X],
			 cali[ITG1010_AXIS_Y], cali[ITG1010_AXIS_Z]);
	}
#endif
	return ITG1010_write_rel_calibration(obj, cali);
}
/*----------------------------------------------------------------------------*/

static int ITG1010_factory_enable_sensor(bool enabledisable,
					 int64_t sample_periods_ms)
{
	int err = 0;

	err = ITG1010_enable_nodata(enabledisable == true ? 1 : 0);
	if (err) {
		pr_err_ratelimited("[Gyro]%s:enable failed!\n", __func__);
		return -1;
	}
	err = ITG1010_batch(0, sample_periods_ms * 1000000, 0);
	if (err) {
		pr_err_ratelimited("[Gyro]%s:set batch failed!\n", __func__);
		return -1;
	}
	return 0;
}
static int ITG1010_factory_get_data(int32_t data[3], int *status)
{
	return ITG1010_get_data(&data[0], &data[1], &data[2], status);
}
static int ITG1010_factory_get_raw_data(int32_t data[3])
{
	pr_info("don't support %s!\n", __func__);
	return 0;
}
static int ITG1010_factory_enable_calibration(void) { return 0; }
static int ITG1010_factory_clear_cali(void)
{
	int err = 0;

	err = ITG1010_ResetCalibration(ITG1010_i2c_client);
	if (err) {
		pr_info("bmg_ResetCalibration failed!\n");
		return -1;
	}
	return 0;
}
static int ITG1010_factory_set_cali(int32_t data[3])
{
	int err = 0;
	int cali[3] = { 0 };

	cali[ITG1010_AXIS_X] =
	    data[0] * ITG1010_DEFAULT_LSB / ITG1010_FS_MAX_LSB;
	cali[ITG1010_AXIS_Y] =
	    data[1] * ITG1010_DEFAULT_LSB / ITG1010_FS_MAX_LSB;
	cali[ITG1010_AXIS_Z] =
	    data[2] * ITG1010_DEFAULT_LSB / ITG1010_FS_MAX_LSB;
	pr_debug("Gyro:gyro set cali:[%5d %5d %5d]\n", cali[ITG1010_AXIS_X],
		 cali[ITG1010_AXIS_Y], cali[ITG1010_AXIS_Z]);
	err = ITG1010_WriteCalibration(ITG1010_i2c_client, cali);
	return 0;
}
static int ITG1010_factory_get_cali(int32_t data[3])
{
	int err = 0;
	int cali[3] = { 0 };

	err = ITG1010_ReadCalibration(ITG1010_i2c_client, cali);
	if (err) {
		pr_info("bmg_ReadCalibration failed!\n");
		return -1;
	}
	data[0] =
	    cali[ITG1010_AXIS_X] * ITG1010_FS_MAX_LSB / ITG1010_DEFAULT_LSB;
	data[1] =
	    cali[ITG1010_AXIS_Y] * ITG1010_FS_MAX_LSB / ITG1010_DEFAULT_LSB;
	data[2] =
	    cali[ITG1010_AXIS_Z] * ITG1010_FS_MAX_LSB / ITG1010_DEFAULT_LSB;

	return 0;
}
static int ITG1010_factory_do_self_test(void) { return 0; }

static struct gyro_factory_fops ITG1010_factory_fops = {
	.enable_sensor = ITG1010_factory_enable_sensor,
	.get_data = ITG1010_factory_get_data,
	.get_raw_data = ITG1010_factory_get_raw_data,
	.enable_calibration = ITG1010_factory_enable_calibration,
	.clear_cali = ITG1010_factory_clear_cali,
	.set_cali = ITG1010_factory_set_cali,
	.get_cali = ITG1010_factory_get_cali,
	.do_self_test = ITG1010_factory_do_self_test,
};

static struct gyro_factory_public ITG1010_factory_device = {
	.gain = 1, .sensitivity = 1, .fops = &ITG1010_factory_fops,
};

/*----------------------------------------------------------------------------*/
static int ITG1010_i2c_probe(struct i2c_client *client,
			     const struct i2c_device_id *id)
{
	struct i2c_client *new_client = NULL;
	struct ITG1010_i2c_data *obj = NULL;
	struct gyro_control_path ctl = { 0 };
	struct gyro_data_path data = { 0 };
	int i;
	int err = 0;
	int result;

	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj) {
		err = -ENOMEM;
		goto exit;
	}

	err = get_gyro_dts_func(client->dev.of_node, &obj->hw);
	if (err < 0) {
		pr_err_ratelimited("[Gyro]%s:get dts info fail\n", __func__);
		goto exit_kfree;
	}

	err = hwmsen_get_convert(obj->hw.direction, &obj->cvt);
	if (err) {
		pr_err_ratelimited("[Gyro]%s:invalid direction: %d\n",
			__func__, obj->hw.direction);
		goto exit_kfree;
	}

	if (obj->hw.addr != 0) {
		client->addr = obj->hw.addr >> 1;
		pr_debug("Gyro:gyro_use_i2c_addr: %x\n", client->addr);
	}

	obj_i2c_data = obj;
	obj->client = client;
	new_client = obj->client;
	i2c_set_clientdata(new_client, obj);

	atomic_set(&obj->trace, 0);
	atomic_set(&obj->suspend, 0);

	ITG1010_i2c_client = new_client;
	err = ITG1010_init_client(new_client, false);
	if (err)
		goto exit_init_failed;

	err = gyro_factory_device_register(&ITG1010_factory_device);
	if (err) {
		pr_err_ratelimited("[Gyro]%s:misc device register failed, err = %d\n",
			__func__, err);
		goto exit_misc_device_register_failed;
	}

	ctl.is_use_common_factory = false;

	err = ITG1010_create_attr(
	    &(ITG1010_init_info.platform_diver_addr->driver));
	if (err) {
		pr_err_ratelimited("[Gyro]%s:ITG1010 create attribute err = %d\n",
			__func__, err);
		goto exit_create_attr_failed;
	}

	ctl.open_report_data = ITG1010_open_report_data;
	ctl.enable_nodata = ITG1010_enable_nodata;
	ctl.set_delay = ITG1010_set_delay;
	ctl.batch = ITG1010_batch;
	ctl.flush = ITG1010_flush;
	ctl.is_report_input_direct = false;
	ctl.is_support_batch = obj->hw.is_batch_supported;

	err = gyro_register_control_path(&ctl);
	if (err) {
		pr_err_ratelimited("[Gyro]%s:register gyro control path err\n",
			__func__);
		goto exit_kfree;
	}

	data.get_data = ITG1010_get_data;
	data.vender_div = DEGREE_TO_RAD;
	err = gyro_register_data_path(&data);
	if (err) {
		pr_err_ratelimited("[Gyro]%s:gyro_register_data_path fail = %d\n",
			__func__, err);
		goto exit_kfree;
	}

#if INV_GYRO_AUTO_CALI == 1
	mutex_init(&obj->temperature_mutex);
	mutex_init(&obj->raw_data_mutex);

	/* create a class to avoid event drop by uevent_ops->filter function
	 * (dev_uevent_filter())
	 */
	inv_daemon_class = class_create(THIS_MODULE, INV_DAEMON_CLASS_NAME);
	if (IS_ERR(inv_daemon_class)) {
		pr_err_ratelimited("[Gyro]%s:cannot create inv daemon class, %s\n",
			__func__, INV_DAEMON_CLASS_NAME);
		goto exit_class_create_failed;
	}

	inv_daemon_device = kzalloc(sizeof(struct device), GFP_KERNEL);
	if (!inv_daemon_device)
		goto exit_device_register_failed;

	inv_daemon_device->init_name = INV_DAEMON_DEVICE_NAME;
	inv_daemon_device->class = inv_daemon_class;
	inv_daemon_device->release = (void (*)(struct device *))kfree;
	result = device_register(inv_daemon_device);
	if (result) {
		pr_err_ratelimited("[Gyro]%s:cannot register inv daemon device, %s\n",
			__func__, INV_DAEMON_DEVICE_NAME);
		goto exit_device_register_failed;
	}

	result = 0;
	for (i = 0; i < ARRAY_SIZE(inv_daemon_dev_attributes); i++) {
		result = device_create_file(inv_daemon_device,
					    inv_daemon_dev_attributes[i]);
		if (result)
			break;
	}
	if (result) {
		while (--i >= 0)
			device_remove_file(inv_daemon_device,
					   inv_daemon_dev_attributes[i]);
		pr_err_ratelimited("[Gyro]%s:cannot create inv daemon dev attr.\n",
			__func__);
		goto exit_create_file_failed;
	}

#endif
	ITG1010_init_flag = 0;

	pr_debug("Gyro:%s: OK\n", __func__);
	return 0;

#if INV_GYRO_AUTO_CALI == 1
exit_create_file_failed:
	device_unregister(inv_daemon_device);
exit_device_register_failed:
	class_destroy(inv_daemon_class);
exit_class_create_failed:
#endif
exit_create_attr_failed:
exit_init_failed:
exit_misc_device_register_failed:
exit_kfree:
	kfree(obj);
exit:
	obj = NULL;
	new_client = NULL;
	ITG1010_i2c_client = NULL;
	obj_i2c_data = NULL;
	ITG1010_init_flag = -1;
	pr_err_ratelimited("[Gyro]%s:err = %d\n", __func__, err);
	return err;
}

/*----------------------------------------------------------------------------*/
static int ITG1010_i2c_remove(struct i2c_client *client)
{
	int err = 0;

#if INV_GYRO_AUTO_CALI == 1
	int i;

	for (i = 0; i < ARRAY_SIZE(inv_daemon_dev_attributes); i++)
		device_remove_file(inv_daemon_device,
				   inv_daemon_dev_attributes[i]);

	device_unregister(inv_daemon_device);
	class_destroy(inv_daemon_class);
#endif
	err = ITG1010_delete_attr(
	    &(ITG1010_init_info.platform_diver_addr->driver));
	if (err) {
		pr_err_ratelimited("[Gyro]%s:ITG1010_delete_attr fail: %d\n",
			__func__, err);
	}

	ITG1010_i2c_client = NULL;
	i2c_unregister_device(client);
	gyro_factory_device_deregister(&ITG1010_factory_device);
	kfree(i2c_get_clientdata(client));
	return 0;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static int ITG1010_remove(void)
{
	pr_info("%s\n", __func__);
	i2c_del_driver(&ITG1010_i2c_driver);
	return 0;
}
/*----------------------------------------------------------------------------*/
static int ITG1010_local_init(struct platform_device *pdev)
{
	gyroPltFmDev = pdev;

	if (i2c_add_driver(&ITG1010_i2c_driver)) {
		pr_err_ratelimited("[Gyro]%s:add driver error\n", __func__);
		return -1;
	}
	if (-1 == ITG1010_init_flag)
		return -1;

	return 0;
}

/*----------------------------------------------------------------------------*/
static int __init ITG1010_init(void)
{
	pr_info("%s\n", __func__);
	gyro_driver_add(&ITG1010_init_info);

	return 0;
}
/*----------------------------------------------------------------------------*/
static void __exit ITG1010_exit(void)
{
/*pr_info("%s\n", __func__);*/
#ifdef CONFIG_CUSTOM_KERNEL_GYROSCOPE_MODULE
	gyro_success_Flag = false;
#endif
}
/*----------------------------------------------------------------------------*/
module_init(ITG1010_init);
module_exit(ITG1010_exit);
/*----------------------------------------------------------------------------*/
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ITG1010 gyroscope driver");
