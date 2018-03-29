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

#include "cust_gyro.h"
#include "ITG1010.h"
#include "gyroscope.h"

#define DEBUG 0
/*----------------------------------------------------------------------------*/
#define INV_GYRO_AUTO_CALI  1
#define ITG1010_DEFAULT_FS		ITG1010_FS_1000
#define ITG1010_DEFAULT_LSB		ITG1010_FS_1000_LSB
#define CONFIG_ITG1010_LOWPASS   /*apply low pass filter on output*/

#define ITG1010_AXIS_X		  0
#define ITG1010_AXIS_Y		  1
#define ITG1010_AXIS_Z		  2
#define ITG1010_AXES_NUM		3
#define ITG1010_DATA_LEN		6
#define ITG1010_DEV_NAME		"ITG-1010A"

int packet_thresh = 75; /* 600 ms / 8ms/sample */
/*----------------------------------------------------------------------------*/
static int ITG1010_init_flag =  -1;

/* Maintain  cust info here */
struct gyro_hw gyro_cust;
static struct gyro_hw *hw = &gyro_cust;
struct platform_device *gyroPltFmDev;
/* For  driver get cust info */
struct gyro_hw *get_cust_gyro(void)
{
	return &gyro_cust;
}

static const struct i2c_device_id ITG1010_i2c_id[] = {{ITG1010_DEV_NAME, 0}, {} };
/* static struct i2c_board_info __initdata i2c_ITG1010={ I2C_BOARD_INFO(ITG1010_DEV_NAME,
*(ITG1010_I2C_SLAVE_ADDR>>1))}; */

static int ITG1010_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int ITG1010_i2c_remove(struct i2c_client *client);
static int ITG1010_i2c_detect(struct i2c_client *client, struct i2c_board_info *info);
static int ITG1010_suspend(struct i2c_client *client, pm_message_t msg);
static int ITG1010_resume(struct i2c_client *client);
static int ITG1010_local_init(struct platform_device *pdev);
static int  ITG1010_remove(void);


static struct gyro_init_info ITG1010_init_info = {
	.name = "ITG1010GY",
	.init = ITG1010_local_init,
	.uninit = ITG1010_remove,
};
/*----------------------------------------------------------------------------*/
enum GYRO_TRC {
	GYRO_TRC_FILTER  = 0x01,
	GYRO_TRC_RAWDATA = 0x02,
	GYRO_TRC_IOCTL   = 0x04,
	GYRO_TRC_CALI   = 0X08,
	GYRO_TRC_INFO   = 0X10,
	GYRO_TRC_DATA   = 0X20,
};
/*----------------------------------------------------------------------------*/
struct scale_factor {
	u8  whole;
	u8  fraction;
};
/*----------------------------------------------------------------------------*/
struct data_resolution {
	struct scale_factor scalefactor;
	int				 sensitivity;
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
	struct gyro_hw *hw;
	struct hwmsen_convert   cvt;

	/*misc*/
	struct data_resolution *reso;
	atomic_t				trace;
	atomic_t				suspend;
	atomic_t				selftest;
	atomic_t				filter;
	s16					 cali_sw[ITG1010_AXES_NUM+1];

	/*data*/
	s8					  offset[ITG1010_AXES_NUM+1];  /*+1: for 4-byte alignment*/
	s16					 data[ITG1010_AXES_NUM+1];

#if defined(CONFIG_ITG1010_LOWPASS)
	atomic_t				firlen;
	atomic_t				fir_en;
	struct data_filter	  fir;
#endif

#if INV_GYRO_AUTO_CALI == 1
	s16					 inv_cali_raw[ITG1010_AXES_NUM+1];
	s16					 temperature;
	struct mutex			temperature_mutex;/* for temperature protection */
	struct mutex			raw_data_mutex;/* for inv_cali_raw[] protection */
#endif
};
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_OF
static const struct of_device_id gyro_of_match[] = {
	{.compatible = "mediatek,gyro"},
	{},
};
#endif
static struct i2c_driver ITG1010_i2c_driver = {
	.driver = {
	.name		   = ITG1010_DEV_NAME,
#ifdef CONFIG_OF
	.of_match_table = gyro_of_match,
#endif
	},
	.probe			  = ITG1010_i2c_probe,
	.remove			 = ITG1010_i2c_remove,
	.detect			 = ITG1010_i2c_detect,
	.suspend			= ITG1010_suspend,
	.resume			 = ITG1010_resume,
	.id_table = ITG1010_i2c_id,
};

/*----------------------------------------------------------------------------*/
static struct i2c_client *ITG1010_i2c_client;
static struct ITG1010_i2c_data *obj_i2c_data;
static bool sensor_power;
static DEFINE_MUTEX(ITG1010_i2c_mutex);
#define C_I2C_FIFO_SIZE		8

/**************I2C operate API*****************************/
static int ITG1010_i2c_read_block(struct i2c_client *client, u8 addr, u8 *data, u8 len)
{
	u8 beg = addr;
	int err;
	struct i2c_msg msgs[2] = {{0}, {0} };

	if (!client)
		return -EINVAL;

	mutex_lock(&ITG1010_i2c_mutex);

	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = &beg;

	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = data;

	if (len > C_I2C_FIFO_SIZE) {
		GYRO_ERR(" length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		mutex_unlock(&ITG1010_i2c_mutex);
		return -EINVAL;
	}
	err = i2c_transfer(client->adapter, msgs, sizeof(msgs)/sizeof(msgs[0]));
	if (err != 2) {
		GYRO_ERR("i2c_transfer error: (%d %p %d) %d\n", addr, data, len, err);
		err = -EIO;
	} else
		err = 0;

	mutex_unlock(&ITG1010_i2c_mutex);
	return err;
}

static int ITG1010_i2c_write_block(struct i2c_client *client, u8 addr, u8 *data, u8 len)
{   /*because address also occupies one byte, the maximum length for write is 7 bytes*/
	int err, idx, num;
	char buf[C_I2C_FIFO_SIZE];

	err = 0;
	mutex_lock(&ITG1010_i2c_mutex);
	if (!client) {
		mutex_unlock(&ITG1010_i2c_mutex);
		return -EINVAL;
	} else if (len >= C_I2C_FIFO_SIZE) {
		GYRO_ERR(" length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		mutex_unlock(&ITG1010_i2c_mutex);
		return -EINVAL;
	}

	num = 0;
	buf[num++] = addr;
	for (idx = 0; idx < len; idx++)
		buf[num++] = data[idx];

	err = i2c_master_send(client, buf, num);
	if (err < 0) {
		GYRO_ERR("send command error!!\n");
		mutex_unlock(&ITG1010_i2c_mutex);
		return -EFAULT;
	}
	mutex_unlock(&ITG1010_i2c_mutex);
	return err;
}


static unsigned int power_on;
#if INV_GYRO_AUTO_CALI == 1

#define INV_DAEMON_CLASS_NAME  "invensense_daemon_class"
#define INV_DAEMON_DEVICE_NAME  "invensense_daemon_device"

static struct class *inv_daemon_class;
static struct device *inv_daemon_device;
static int inv_mpl_motion_state;
static int inv_gyro_power_state;
static ssize_t inv_mpl_motion_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int result;
	unsigned long data;
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
	struct device_attribute *attr, const char *buf, size_t count)
{
	sysfs_notify(&dev->kobj, NULL, "inv_gyro_data_ready");
	return count;
}
static ssize_t inv_gyro_data_ready_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "1\n");
}

static ssize_t inv_gyro_power_state_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int result;
	unsigned long data;

	result = kstrtoul(buf, 10, &data);
	if (result)
		return result;

	inv_gyro_power_state = data;

	sysfs_notify(&dev->kobj, NULL, "inv_gyro_power_state");
	return count;
}
static ssize_t inv_gyro_power_state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", inv_gyro_power_state);
}

static DEVICE_ATTR(inv_mpl_motion, S_IRUGO | S_IWUSR, inv_mpl_motion_show, inv_mpl_motion_store);
static DEVICE_ATTR(inv_gyro_data_ready, S_IRUGO | S_IWUSR, inv_gyro_data_ready_show, inv_gyro_data_ready_store);
static DEVICE_ATTR(inv_gyro_power_state, S_IRUGO | S_IWUSR, inv_gyro_power_state_show, inv_gyro_power_state_store);

static struct device_attribute *inv_daemon_dev_attributes[] = {
	&dev_attr_inv_mpl_motion,
	&dev_attr_inv_gyro_data_ready,
	&dev_attr_inv_gyro_power_state,
};
#endif/* #if INV_GYRO_AUTO_CALI == 1 */


int ITG1010_gyro_power(void)
{
	return power_on;
}
EXPORT_SYMBOL(ITG1010_gyro_power);

int ITG1010_gyro_mode(void)
{
	return sensor_power;
}
EXPORT_SYMBOL(ITG1010_gyro_mode);

/*--------------------gyroscopy power control function----------------------------------*/
static void ITG1010_power(struct gyro_hw *hw, unsigned int on)
{
}
/*----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------*/
static int ITG1010_write_rel_calibration(struct ITG1010_i2c_data *obj, int dat[ITG1010_AXES_NUM])
{
	obj->cali_sw[ITG1010_AXIS_X] = obj->cvt.sign[ITG1010_AXIS_X]*dat[obj->cvt.map[ITG1010_AXIS_X]];
	obj->cali_sw[ITG1010_AXIS_Y] = obj->cvt.sign[ITG1010_AXIS_Y]*dat[obj->cvt.map[ITG1010_AXIS_Y]];
	obj->cali_sw[ITG1010_AXIS_Z] = obj->cvt.sign[ITG1010_AXIS_Z]*dat[obj->cvt.map[ITG1010_AXIS_Z]];
#if DEBUG
	if (atomic_read(&obj->trace) & GYRO_TRC_CALI) {
		GYRO_LOG("test  (%5d, %5d, %5d) ->(%5d, %5d, %5d)->(%5d, %5d, %5d))\n",
		obj->cvt.sign[ITG1010_AXIS_X], obj->cvt.sign[ITG1010_AXIS_Y], obj->cvt.sign[ITG1010_AXIS_Z],
		dat[ITG1010_AXIS_X], dat[ITG1010_AXIS_Y], dat[ITG1010_AXIS_Z],
		obj->cvt.map[ITG1010_AXIS_X], obj->cvt.map[ITG1010_AXIS_Y], obj->cvt.map[ITG1010_AXIS_Z]);
		GYRO_LOG("write gyro calibration data  (%5d, %5d, %5d)\n",
		obj->cali_sw[ITG1010_AXIS_X], obj->cali_sw[ITG1010_AXIS_Y], obj->cali_sw[ITG1010_AXIS_Z]);
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
static int ITG1010_ReadCalibration(struct i2c_client *client, int dat[ITG1010_AXES_NUM])
{
	struct ITG1010_i2c_data *obj = i2c_get_clientdata(client);

	dat[obj->cvt.map[ITG1010_AXIS_X]] = obj->cvt.sign[ITG1010_AXIS_X]*obj->cali_sw[ITG1010_AXIS_X];
	dat[obj->cvt.map[ITG1010_AXIS_Y]] = obj->cvt.sign[ITG1010_AXIS_Y]*obj->cali_sw[ITG1010_AXIS_Y];
	dat[obj->cvt.map[ITG1010_AXIS_Z]] = obj->cvt.sign[ITG1010_AXIS_Z]*obj->cali_sw[ITG1010_AXIS_Z];

#if DEBUG
	if (atomic_read(&obj->trace) & GYRO_TRC_CALI) {
		GYRO_LOG("Read gyro calibration data  (%5d, %5d, %5d)\n",
		dat[ITG1010_AXIS_X], dat[ITG1010_AXIS_Y], dat[ITG1010_AXIS_Z]);
	}
#endif

	return 0;
}
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
static int ITG1010_WriteCalibration(struct i2c_client *client, int dat[ITG1010_AXES_NUM])
{
	struct ITG1010_i2c_data *obj = i2c_get_clientdata(client);
	int cali[ITG1010_AXES_NUM];

	/*GYRO_LOG();*/
	if (!obj || !dat) {
		GYRO_ERR("null ptr!!\n");
		return -EINVAL;
	}
	cali[obj->cvt.map[ITG1010_AXIS_X]] = obj->cvt.sign[ITG1010_AXIS_X]*obj->cali_sw[ITG1010_AXIS_X];
	cali[obj->cvt.map[ITG1010_AXIS_Y]] = obj->cvt.sign[ITG1010_AXIS_Y]*obj->cali_sw[ITG1010_AXIS_Y];
	cali[obj->cvt.map[ITG1010_AXIS_Z]] = obj->cvt.sign[ITG1010_AXIS_Z]*obj->cali_sw[ITG1010_AXIS_Z];
	cali[ITG1010_AXIS_X] += dat[ITG1010_AXIS_X];
	cali[ITG1010_AXIS_Y] += dat[ITG1010_AXIS_Y];
	cali[ITG1010_AXIS_Z] += dat[ITG1010_AXIS_Z];
#if DEBUG
	if (atomic_read(&obj->trace) & GYRO_TRC_CALI) {
		GYRO_LOG("write gyro calibration data  (%5d, %5d, %5d)-->(%5d, %5d, %5d)\n",
			dat[ITG1010_AXIS_X], dat[ITG1010_AXIS_Y], dat[ITG1010_AXIS_Z],
				cali[ITG1010_AXIS_X], cali[ITG1010_AXIS_Y], cali[ITG1010_AXIS_Z]);
	}
#endif
	return ITG1010_write_rel_calibration(obj, cali);
}
/*----------------------------------------------------------------------------*/


/* ----------------------------------------------------------------------------// */
static int ITG1010_SetPowerMode(struct i2c_client *client, bool enable)
{
	u8 databuf[2] = {0};
	int res = 0;

	if (enable == sensor_power) {
		GYRO_LOG("Sensor power status is newest!\n");
		return ITG1010_SUCCESS;
	}

	if (ITG1010_i2c_read_block(client, ITG1010_REG_PWR_CTL, databuf, 1)) {
		GYRO_ERR("read power ctl register err!\n");
		return ITG1010_ERR_I2C;
	}

	databuf[0] &= ~ITG1010_SLEEP;
	if (enable == FALSE)
		databuf[0] |= ITG1010_SLEEP;

	res = ITG1010_i2c_write_block(client, ITG1010_REG_PWR_CTL, databuf, 1);
	if (res <= 0) {
		GYRO_LOG("set power mode failed!\n");
		return ITG1010_ERR_I2C;
	}
	/* GYRO_LOG("set power mode ok %d!\n", enable); */

	sensor_power = enable;

	return ITG1010_SUCCESS;
}

/*----------------------------------------------------------------------------*/
static int ITG1010_SetDataFormat(struct i2c_client *client, u8 dataformat)
{
	u8 databuf[2] = {0};
	int res = 0;

	/*GYRO_LOG();*/

	databuf[0] = dataformat;
	res = ITG1010_i2c_write_block(client, ITG1010_REG_CFG, databuf, 1);
	if (res <= 0)
		return ITG1010_ERR_I2C;

	/* read sample rate after written for test */
	udelay(500);

	res = ITG1010_i2c_read_block(client, ITG1010_REG_CFG, databuf, 1);
	if (res != 0) {
		GYRO_ERR("read data format register err!\n");
		return ITG1010_ERR_I2C;
	}
	/* GYRO_LOG("read  data format: 0x%x\n", databuf[0]); */

	return ITG1010_SUCCESS;
}

static int ITG1010_SetFullScale(struct i2c_client *client, u8 dataformat)
{
	u8 databuf[2] = {0};
	int res = 0;

	/*GYRO_LOG();*/

	databuf[0] = dataformat;
	res = ITG1010_i2c_write_block(client, ITG1010_REG_GYRO_CFG, databuf, 1);
	if (res <= 0)
		return ITG1010_ERR_I2C;

	/* read sample rate after written for test */
	udelay(500);
	res = ITG1010_i2c_read_block(client, ITG1010_REG_GYRO_CFG, databuf, 1);
	if (res != 0) {
		GYRO_ERR("read data format register err!\n");
		return ITG1010_ERR_I2C;
	}
	GYRO_LOG("read  data format: 0x%x\n", databuf[0]);

	return ITG1010_SUCCESS;
}


/* set the sample rate */
static int ITG1010_SetSampleRate(struct i2c_client *client, int sample_rate)
{
	u8 databuf[2] = {0};
	int rate_div = 0;
	int res = 0;

	/*GYRO_LOG();*/

	res = ITG1010_i2c_read_block(client, ITG1010_REG_CFG, databuf, 1);
	if (res != 0) {
		GYRO_ERR("read gyro data format register err!\n");
		return ITG1010_ERR_I2C;
	}
	/* GYRO_LOG("read  gyro data format register: 0x%x\n", databuf[0]); */

	if ((databuf[0] & 0x07) == 0)	/* Analog sample rate is 8KHz */
		rate_div = 8 * 1024 / sample_rate - 1;
	else	/* 1kHz */
		rate_div = 1024 / sample_rate - 1;

	if (rate_div > 255)  /* rate_div: 0 to 255; */
		rate_div = 255;
	else if (rate_div < 0)
		rate_div = 0;

	databuf[0] = rate_div;
	res = ITG1010_i2c_write_block(client, ITG1010_REG_SAMRT_DIV, databuf, 1);
	if (res <= 0) {
		GYRO_ERR("write sample rate register err!\n");
		return ITG1010_ERR_I2C;
	}

	/* read sample div after written for test */
	udelay(500);
	res = ITG1010_i2c_read_block(client, ITG1010_REG_SAMRT_DIV, databuf, 1);
	if (res != 0) {
		GYRO_ERR("read gyro sample rate register err!\n");
		return ITG1010_ERR_I2C;
	}
	/* GYRO_LOG("read  gyro sample rate: 0x%x\n", databuf[0]); */

	return ITG1010_SUCCESS;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static int ITG1010_ReadGyroData(struct i2c_client *client, char *buf, int bufsize)
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
		GYRO_ERR("ITG1010 read temperature data  error\n");
		return -2;
	}
	mutex_lock(&obj->temperature_mutex);
	obj->temperature = ((s16)((databuf[1]) | (databuf[0] << 8)));
	mutex_unlock(&obj->temperature_mutex);
#endif

	ret = ITG1010_i2c_read_block(client, ITG1010_REG_GYRO_XH, databuf, 6);
	if (ret != 0) {
		GYRO_ERR("ITG1010 read gyroscope data  error\n");
		return -2;
	}

	obj->data[ITG1010_AXIS_X] = ((s16)((databuf[ITG1010_AXIS_X*2+1]) | (databuf[ITG1010_AXIS_X*2] << 8)));
	obj->data[ITG1010_AXIS_Y] = ((s16)((databuf[ITG1010_AXIS_Y*2+1]) | (databuf[ITG1010_AXIS_Y*2] << 8)));
	obj->data[ITG1010_AXIS_Z] = ((s16)((databuf[ITG1010_AXIS_Z*2+1]) | (databuf[ITG1010_AXIS_Z*2] << 8)));
#if DEBUG
	if (atomic_read(&obj->trace) & GYRO_TRC_RAWDATA) {
		GYRO_LOG("read gyro register: %d, %d, %d, %d, %d, %d",
			 databuf[0], databuf[1], databuf[2], databuf[3], databuf[4], databuf[5]);
		GYRO_LOG("get gyro raw data (0x%08X, 0x%08X, 0x%08X) -> (%5d, %5d, %5d)\n",
			 obj->data[ITG1010_AXIS_X], obj->data[ITG1010_AXIS_Y], obj->data[ITG1010_AXIS_Z],
		 obj->data[ITG1010_AXIS_X], obj->data[ITG1010_AXIS_Y], obj->data[ITG1010_AXIS_Z]);
	}
#endif
#if INV_GYRO_AUTO_CALI == 1
	mutex_lock(&obj->raw_data_mutex);
	/*remap coordinate*/
	obj->inv_cali_raw[obj->cvt.map[ITG1010_AXIS_X]] = obj->cvt.sign[ITG1010_AXIS_X]*obj->data[ITG1010_AXIS_X];
	obj->inv_cali_raw[obj->cvt.map[ITG1010_AXIS_Y]] = obj->cvt.sign[ITG1010_AXIS_Y]*obj->data[ITG1010_AXIS_Y];
	obj->inv_cali_raw[obj->cvt.map[ITG1010_AXIS_Z]] = obj->cvt.sign[ITG1010_AXIS_Z]*obj->data[ITG1010_AXIS_Z];
	mutex_unlock(&obj->raw_data_mutex);
#endif
	obj->data[ITG1010_AXIS_X] = obj->data[ITG1010_AXIS_X] + obj->cali_sw[ITG1010_AXIS_X];
	obj->data[ITG1010_AXIS_Y] = obj->data[ITG1010_AXIS_Y] + obj->cali_sw[ITG1010_AXIS_Y];
	obj->data[ITG1010_AXIS_Z] = obj->data[ITG1010_AXIS_Z] + obj->cali_sw[ITG1010_AXIS_Z];

	/*remap coordinate*/
	data[obj->cvt.map[ITG1010_AXIS_X]] = obj->cvt.sign[ITG1010_AXIS_X]*obj->data[ITG1010_AXIS_X];
	data[obj->cvt.map[ITG1010_AXIS_Y]] = obj->cvt.sign[ITG1010_AXIS_Y]*obj->data[ITG1010_AXIS_Y];
	data[obj->cvt.map[ITG1010_AXIS_Z]] = obj->cvt.sign[ITG1010_AXIS_Z]*obj->data[ITG1010_AXIS_Z];

	/* Out put the degree/second(o/s) */
	data[ITG1010_AXIS_X] = data[ITG1010_AXIS_X] * ITG1010_FS_MAX_LSB / ITG1010_DEFAULT_LSB;
	data[ITG1010_AXIS_Y] = data[ITG1010_AXIS_Y] * ITG1010_FS_MAX_LSB / ITG1010_DEFAULT_LSB;
	data[ITG1010_AXIS_Z] = data[ITG1010_AXIS_Z] * ITG1010_FS_MAX_LSB / ITG1010_DEFAULT_LSB;

	sprintf(buf, "%04x %04x %04x", data[ITG1010_AXIS_X], data[ITG1010_AXIS_Y], data[ITG1010_AXIS_Z]);

#if DEBUG
	if (atomic_read(&obj->trace) & GYRO_TRC_DATA)
		GYRO_LOG("get gyro data packet:[%d %d %d]\n", data[0], data[1], data[2]);
#endif

	return 0;
}

static int ITG1010_ReadChipInfo(struct i2c_client *client, char *buf, int bufsize)
{
	u8 databuf[10];

	memset(databuf, 0, sizeof(u8)*10);

	if ((NULL == buf) || (bufsize <= 30))
		return -1;

	if (NULL == client) {
		*buf = 0;
		return -2;
	}

	sprintf(buf, "ITG1010 Chip");
	return 0;
}

#if INV_GYRO_AUTO_CALI == 1
/*----------------------------------------------------------------------------*/
static int ITG1010_ReadGyroDataRaw(struct i2c_client *client, char *buf, int bufsize)
{
	struct ITG1010_i2c_data *obj = i2c_get_clientdata(client);

	mutex_lock(&obj->raw_data_mutex);
	/* return gyro raw LSB in device orientation */
	sprintf(buf, "%x %x %x", obj->inv_cali_raw[ITG1010_AXIS_X], obj->inv_cali_raw[ITG1010_AXIS_Y],
		obj->inv_cali_raw[ITG1010_AXIS_Z]);

#if DEBUG
	if (atomic_read(&obj->trace) & GYRO_TRC_DATA)
		GYRO_LOG("get gyro raw data packet:[%d %d %d]\n", obj->inv_cali_raw[0], obj->inv_cali_raw[1],
			obj->inv_cali_raw[2]);
#endif
	mutex_unlock(&obj->raw_data_mutex);

	return 0;
}

/*----------------------------------------------------------------------------*/
static int ITG1010_ReadTemperature(struct i2c_client *client, char *buf, int bufsize)
{
	struct ITG1010_i2c_data *obj = i2c_get_clientdata(client);

	mutex_lock(&obj->temperature_mutex);
	sprintf(buf, "%x", obj->temperature);

#if DEBUG
	if (atomic_read(&obj->trace) & GYRO_TRC_DATA)
		GYRO_LOG("get gyro temperature:[%d]\n", obj->temperature);
#endif
	mutex_unlock(&obj->temperature_mutex);

	return 0;
}

/*----------------------------------------------------------------------------*/
static int ITG1010_ReadPowerStatus(struct i2c_client *client, char *buf, int bufsize)
{
#if DEBUG
	GYRO_LOG("get gyro PowerStatus:[%d]\n", sensor_power);
#endif

	sprintf(buf, "%x", sensor_power);

	return 0;
}
#endif

/*----------------------------------------------------------------------------*/
static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = ITG1010_i2c_client;
	char strbuf[ITG1010_BUFSIZE];

	if (NULL == client) {
		GYRO_ERR("i2c client is null!!\n");
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

	if (NULL == client) {
		GYRO_ERR("i2c client is null!!\n");
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
		GYRO_ERR("i2c_data obj is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&obj->trace));
	return res;
}
/*----------------------------------------------------------------------------*/
static ssize_t store_trace_value(struct device_driver *ddri, const char *buf, size_t count)
{
	struct ITG1010_i2c_data *obj = obj_i2c_data;
	int trace;

	if (obj == NULL) {
		GYRO_ERR("i2c_data obj is null!!\n");
		return 0;
	}

	if (1 == sscanf(buf, "0x%x", &trace))
		atomic_set(&obj->trace, trace);
	else
		GYRO_ERR("invalid content: '%s', length = %zu\n", buf, count);

	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t show_status_value(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	struct ITG1010_i2c_data *obj = obj_i2c_data;

	if (obj == NULL) {
		GYRO_ERR("i2c_data obj is null!!\n");
		return 0;
	}

	if (obj->hw)
		len += snprintf(buf+len, PAGE_SIZE-len, "CUST: %d %d (%d %d)\n",
			obj->hw->i2c_num, obj->hw->direction, obj->hw->power_id, obj->hw->power_vol);
	else
		len += snprintf(buf+len, PAGE_SIZE-len, "CUST: NULL\n");

	return len;
}

static ssize_t show_chip_orientation(struct device_driver *ddri, char *buf)
{
	ssize_t		  _tLength = 0;
	struct gyro_hw   *_ptAccelHw = hw;

	GYRO_LOG("[%s] default direction: %d\n", __func__, _ptAccelHw->direction);

	_tLength = snprintf(buf, PAGE_SIZE, "default direction = %d\n", _ptAccelHw->direction);

	return _tLength;
}

static ssize_t store_chip_orientation(struct device_driver *ddri, const char *buf, size_t tCount)
{
	int _nDirection = 0;
	int ret = 0;
	struct ITG1010_i2c_data   *_pt_i2c_obj = obj_i2c_data;

	if (NULL == _pt_i2c_obj)
		return 0;

	ret = kstrtoint(buf, 10, &_nDirection);
	if (ret != 0) {
		if (hwmsen_get_convert(_nDirection, &_pt_i2c_obj->cvt))
			GYRO_ERR("ERR: fail to set direction\n");
	}

	GYRO_LOG("[%s] set direction: %d\n", __func__, _nDirection);

	return tCount;
}

static ssize_t show_power_status(struct device_driver *ddri, char *buf)
{
	ssize_t res = 0;
	u8 uData = 0;
	struct ITG1010_i2c_data *obj = obj_i2c_data;

	if (obj == NULL) {
		GYRO_ERR("i2c_data obj is null!!\n");
		return 0;
	}
	ITG1010_i2c_read_block(obj->client, ITG1010_REG_PWR_CTL, &uData, 1);

	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", uData);
	return res;
}

static ssize_t show_regiter_map(struct device_driver *ddri, char *buf)
{
	u8  _bIndex	   = 0;
	u8  _baRegMap[34] = {0x04, 0x05, 0x07, 0x08, 0xA, 0xB, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B
			, 0x23, 0x37, 0x38, 0x3A, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x6A, 0x6B, 0x6C
			, 0x72, 0x73, 0x74, 0x75};
	u8	 _baRegValue[2] = {0};
	ssize_t	_tLength	  = 0;
	struct ITG1010_i2c_data *obj = obj_i2c_data;

	for (_bIndex = 0; _bIndex < 34; _bIndex++) {
		ITG1010_i2c_read_block(obj->client, _baRegMap[_bIndex], &_baRegValue[0], 1);
		_tLength += snprintf((buf + _tLength), (PAGE_SIZE - _tLength), "Reg[0x%02X]: 0x%02X\n"
		, _baRegMap[_bIndex], _baRegValue[0]);
	}

	return _tLength;
}


/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(chipinfo, S_IRUGO, show_chipinfo_value,  NULL);
static DRIVER_ATTR(sensordata,  S_IRUGO, show_sensordata_value,  NULL);
static DRIVER_ATTR(trace,  S_IWUSR | S_IRUGO, show_trace_value,   store_trace_value);
static DRIVER_ATTR(status,  S_IRUGO, show_status_value,   NULL);
static DRIVER_ATTR(orientation, S_IWUSR | S_IRUGO, show_chip_orientation, store_chip_orientation);
static DRIVER_ATTR(power, S_IRUGO, show_power_status, NULL);
static DRIVER_ATTR(regmap, S_IRUGO, show_regiter_map, NULL);

/*----------------------------------------------------------------------------*/
static struct driver_attribute *ITG1010_attr_list[] = {
	&driver_attr_chipinfo,	 /*chip information*/
	&driver_attr_sensordata,   /*dump sensor data*/
	&driver_attr_trace,		/*trace log*/
	&driver_attr_status,
	&driver_attr_orientation,
	&driver_attr_power,
	&driver_attr_regmap,
};
/*----------------------------------------------------------------------------*/
static int ITG1010_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(sizeof(ITG1010_attr_list)/sizeof(ITG1010_attr_list[0]));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, ITG1010_attr_list[idx]);
		if (0 != err) {
			GYRO_ERR("driver_create_file (%s) = %d\n", ITG1010_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}
/*----------------------------------------------------------------------------*/
static int ITG1010_delete_attr(struct device_driver *driver)
{
	int idx , err = 0;
	int num = (int)(sizeof(ITG1010_attr_list)/sizeof(ITG1010_attr_list[0]));

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
		GYRO_ERR("Cannot find gyro pinctrl!\n");
	}
	pins_default = pinctrl_lookup_state(pinctrl, "pin_default");
	if (IS_ERR(pins_default)) {
		ret = PTR_ERR(pins_default);
		GYRO_ERR("Cannot find gyro pinctrl default!\n");

	}

	pins_cfg = pinctrl_lookup_state(pinctrl, "pin_cfg");
	if (IS_ERR(pins_cfg)) {
		ret = PTR_ERR(pins_cfg);
		GYRO_ERR("Cannot find gyro pinctrl pin_cfg!\n");

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
	res = ITG1010_SetDataFormat(client, (ITG1010_SYNC_GYROX << ITG1010_EXT_SYNC)|
				ITG1010_RATE_1K_LPFB_188HZ);
	res = ITG1010_SetFullScale(client, (ITG1010_DEFAULT_FS << ITG1010_FS_RANGE));
	if (res != ITG1010_SUCCESS)
		return res;

	/* Set 125HZ sample rate */
	res = ITG1010_SetSampleRate(client, 125);
	if (res != ITG1010_SUCCESS)
		return res;

	res = ITG1010_SetPowerMode(client, enable);
	if (res != ITG1010_SUCCESS)
		return res;

	/* GYRO_LOG("ITG1010_init_client OK!\n"); */

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
			GYRO_ERR("Set delay parameter error!\n");
			err = -EINVAL;
		}
		break;

	case SENSOR_ENABLE:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			GYRO_ERR("Enable gyroscope parameter error!\n");
			err = -EINVAL;
		} else {
			value = *(int *)buff_in;
			if (((value == 0) && (sensor_power == false)) || ((value == 1) && (sensor_power == true)))
				GYRO_LOG("gyroscope device have updated!\n");
			else
				err = ITG1010_SetPowerMode(priv->client, !sensor_power);
#if INV_GYRO_AUTO_CALI == 1
			inv_gyro_power_state = sensor_power;
			/* put this in where gyro power is changed, waking up mpu daemon */
			sysfs_notify(&inv_daemon_device->kobj, NULL, "inv_gyro_power_state");
#endif
		}
		break;

	case SENSOR_GET_DATA:
		if ((buff_out == NULL) || (size_out < sizeof(struct hwm_sensor_data))) {
			GYRO_ERR("get gyroscope data parameter error!\n");
			err = -EINVAL;
		} else {
			gyro_data = (struct hwm_sensor_data *)buff_out;
			err = ITG1010_ReadGyroData(priv->client, buff, ITG1010_BUFSIZE);
			if (!err) {
				ret = sscanf(buff, "%x %x %x", &gyro_data->values[0], &gyro_data->values[1]
					, &gyro_data->values[2]);
				gyro_data->status = SENSOR_STATUS_ACCURACY_MEDIUM;
				gyro_data->value_divide = DEGREE_TO_RAD;
#if INV_GYRO_AUTO_CALI == 1
				/* put this in where gyro data is ready to report to hal, waking up mpu daemon */
				sysfs_notify(&inv_daemon_device->kobj, NULL, "inv_gyro_data_ready");
#endif
			}
		}
		break;

	default:
	GYRO_ERR("gyroscope operate function no this parameter %d!\n", command);
	err = -1;
	}

	return err;
}

/******************************************************************************
 * Function Configuration
******************************************************************************/
static int ITG1010_open(struct inode *inode, struct file *file)
{
	file->private_data = ITG1010_i2c_client;

	if (file->private_data == NULL) {
		GYRO_ERR("null pointer!!\n");
		return -EINVAL;
	}
	return nonseekable_open(inode, file);
}
/*----------------------------------------------------------------------------*/
static int ITG1010_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}
/*----------------------------------------------------------------------------*/
static long ITG1010_unlocked_ioctl(struct file *file, unsigned int cmd,
	   unsigned long arg)
{
	struct i2c_client *client = (struct i2c_client *)file->private_data;
	char strbuf[ITG1010_BUFSIZE] = {0};
	void __user *data;
	long err = 0;
	int copy_cnt = 0;
	struct SENSOR_DATA sensor_data;
	int cali[3];
	int smtRes = 0;

	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));

	if (err) {
		GYRO_ERR("access error: %08X, (%2d, %2d)\n", cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
		return -EFAULT;
	}

	switch (cmd) {
	case GYROSCOPE_IOCTL_INIT:
		ITG1010_init_client(client, false);
		break;

	case GYROSCOPE_IOCTL_SMT_DATA:
		data = (void __user *) arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}

		GYRO_LOG("ioctl smtRes: %d!\n", smtRes);
		copy_cnt = copy_to_user(data, &smtRes,  sizeof(smtRes));

		if (copy_cnt) {
			err = -EFAULT;
			GYRO_ERR("copy gyro data to user failed!\n");
		}
		GYRO_LOG("copy gyro data to user OK: %d!\n", copy_cnt);
		break;

	case GYROSCOPE_IOCTL_READ_SENSORDATA:
		data = (void __user *) arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}

		ITG1010_ReadGyroData(client, strbuf, ITG1010_BUFSIZE);
		if (copy_to_user(data, strbuf, sizeof(strbuf))) {
			err = -EFAULT;
			break;
		}
		break;

	case GYROSCOPE_IOCTL_SET_CALI:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		if (copy_from_user(&sensor_data, data, sizeof(sensor_data)))
			err = -EFAULT;
		else {
			cali[ITG1010_AXIS_X] = sensor_data.x * ITG1010_DEFAULT_LSB / ITG1010_FS_MAX_LSB;
			cali[ITG1010_AXIS_Y] = sensor_data.y * ITG1010_DEFAULT_LSB / ITG1010_FS_MAX_LSB;
			cali[ITG1010_AXIS_Z] = sensor_data.z * ITG1010_DEFAULT_LSB / ITG1010_FS_MAX_LSB;
			GYRO_LOG("gyro set cali:[%5d %5d %5d]\n",
			cali[ITG1010_AXIS_X], cali[ITG1010_AXIS_Y], cali[ITG1010_AXIS_Z]);
			err = ITG1010_WriteCalibration(client, cali);
		}
		break;

	case GYROSCOPE_IOCTL_CLR_CALI:
		err = ITG1010_ResetCalibration(client);
		break;

	case GYROSCOPE_IOCTL_GET_CALI:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		err = ITG1010_ReadCalibration(client, cali);
		if (err)
			break;

		sensor_data.x = cali[ITG1010_AXIS_X] * ITG1010_FS_MAX_LSB / ITG1010_DEFAULT_LSB;
		sensor_data.y = cali[ITG1010_AXIS_Y] * ITG1010_FS_MAX_LSB / ITG1010_DEFAULT_LSB;
		sensor_data.z = cali[ITG1010_AXIS_Z] * ITG1010_FS_MAX_LSB / ITG1010_DEFAULT_LSB;
		if (copy_to_user(data, &sensor_data, sizeof(sensor_data))) {
			err = -EFAULT;
			break;
		}
		break;

#if INV_GYRO_AUTO_CALI == 1
	case GYROSCOPE_IOCTL_READ_SENSORDATA_RAW:
		data = (void __user *) arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}

		ITG1010_ReadGyroDataRaw(client, strbuf, ITG1010_BUFSIZE);
		if (copy_to_user(data, strbuf, sizeof(strbuf))) {
			err = -EFAULT;
			break;
		}
		break;

	case GYROSCOPE_IOCTL_READ_TEMPERATURE:
		data = (void __user *) arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}

		ITG1010_ReadTemperature(client, strbuf, ITG1010_BUFSIZE);
		if (copy_to_user(data, strbuf, sizeof(strbuf))) {
			err = -EFAULT;
			break;
		}
		break;

	case GYROSCOPE_IOCTL_GET_POWER_STATUS:
		data = (void __user *) arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}

		ITG1010_ReadPowerStatus(client, strbuf, ITG1010_BUFSIZE);
		if (copy_to_user(data, strbuf, sizeof(strbuf))) {
			err = -EFAULT;
			break;
		}
		break;
#endif

	default:
		GYRO_ERR("unknown IOCTL: 0x%08x\n", cmd);
		err = -ENOIOCTLCMD;
	}

	return err;
}

#ifdef CONFIG_COMPAT
static long ITG1010_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = 0;

	void __user *arg32 = compat_ptr(arg);

	if (!file->f_op || !file->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {
	case COMPAT_GYROSCOPE_IOCTL_INIT:
		if (arg32 == NULL) {
			GYRO_ERR("invalid argument.");
			return -EINVAL;
		 }

		ret = file->f_op->unlocked_ioctl(file, GYROSCOPE_IOCTL_INIT, (unsigned long)arg32);
		if (ret) {
			GYRO_ERR("GYROSCOPE_IOCTL_INIT unlocked_ioctl failed.");
			return ret;
		}

		break;

	case COMPAT_GYROSCOPE_IOCTL_SET_CALI:
		if (arg32 == NULL) {
			GYRO_ERR("invalid argument.");
			return -EINVAL;
		}

		ret = file->f_op->unlocked_ioctl(file, GYROSCOPE_IOCTL_SET_CALI, (unsigned long)arg32);
		if (ret) {
			GYRO_ERR("GYROSCOPE_IOCTL_SET_CALI unlocked_ioctl failed.");
			return ret;
		}

		break;

	case COMPAT_GYROSCOPE_IOCTL_CLR_CALI:
		if (arg32 == NULL) {
			GYRO_ERR("invalid argument.");
			return -EINVAL;
		}

		ret = file->f_op->unlocked_ioctl(file, GYROSCOPE_IOCTL_CLR_CALI,
							(unsigned long)arg32);
		if (ret) {
			GYRO_ERR("GYROSCOPE_IOCTL_CLR_CALI unlocked_ioctl failed.");
			return ret;
		}

		break;

	case COMPAT_GYROSCOPE_IOCTL_GET_CALI:
		if (arg32 == NULL) {
			GYRO_ERR("invalid argument.");
			return -EINVAL;
		}

		ret = file->f_op->unlocked_ioctl(file, GYROSCOPE_IOCTL_GET_CALI,
							(unsigned long)arg32);
		if (ret) {
			GYRO_ERR("GYROSCOPE_IOCTL_GET_CALI unlocked_ioctl failed.");
			return ret;
		}

		break;

	case COMPAT_GYROSCOPE_IOCTL_READ_SENSORDATA:
		if (arg32 == NULL) {
			GYRO_ERR("invalid argument.");
			return -EINVAL;
		}

		ret = file->f_op->unlocked_ioctl(file, GYROSCOPE_IOCTL_READ_SENSORDATA,
							(unsigned long)arg32);
		if (ret) {
			GYRO_ERR("GYROSCOPE_IOCTL_READ_SENSORDATA unlocked_ioctl failed.");
			return ret;
		}

		break;

	default:
		GYRO_ERR("%s not supported = 0x%04x", __func__, cmd);
		return -ENOIOCTLCMD;
	}

	return ret;
}
#endif
/*----------------------------------------------------------------------------*/
static const struct file_operations ITG1010_fops = {
	.open = ITG1010_open,
	.release = ITG1010_release,
	.unlocked_ioctl = ITG1010_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = ITG1010_compat_ioctl,
#endif
};
/*----------------------------------------------------------------------------*/
static struct miscdevice ITG1010_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gyroscope",
	.fops = &ITG1010_fops,
};
/*----------------------------------------------------------------------------*/

static int ITG1010_suspend(struct i2c_client *client, pm_message_t msg)
{
	int err = 0;
	struct ITG1010_i2c_data *obj = i2c_get_clientdata(client);

	/*GYRO_LOG();*/

	if (msg.event == PM_EVENT_SUSPEND) {
		if (obj == NULL) {
			GYRO_ERR("null pointer!!\n");
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
	}
	return err;
}
/*----------------------------------------------------------------------------*/
static int ITG1010_resume(struct i2c_client *client)
{
	struct ITG1010_i2c_data *obj = i2c_get_clientdata(client);
	int err;

	GYRO_LOG();

	if (obj == NULL) {
		GYRO_ERR("null pointer!!\n");
		return -EINVAL;
	}

	ITG1010_power(obj->hw, 1);
	err = ITG1010_init_client(client, false);
	if (err) {
		GYRO_ERR("initialize client fail!!\n");
		return err;
	}
	atomic_set(&obj->suspend, 0);

	return 0;
}
/*----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------*/
static int ITG1010_i2c_detect(struct i2c_client *client, struct i2c_board_info *info)
{
	strncpy(info->type, ITG1010_DEV_NAME, strlen(ITG1010_DEV_NAME));
	return 0;
}


/* if use  this typ of enable , Gsensor should report inputEvent(x, y, z ,stats, div) to HAL */
static int ITG1010_open_report_data(int open)
{
	/* should queuq work to report event if  is_report_input_direct=true */
	return 0;
}

/* if use  this typ of enable , Gsensor only enabled but not report inputEvent to HAL */

static int ITG1010_enable_nodata(int en)
{
	int res = 0;
	int retry = 0;
	bool power = false;

	if (1 == en)
		power = true;
	if (0 == en)
		power = false;

	for (retry = 0; retry < 3; retry++) {
		res = ITG1010_SetPowerMode(obj_i2c_data->client, power);
		if (res == 0) {
			GYRO_LOG("ITG1010_SetPowerMode done\n");
			break;
		}
		GYRO_LOG("ITG1010_SetPowerMode fail\n");
	}


	if (res != ITG1010_SUCCESS) {
		GYRO_LOG("ITG1010_SetPowerMode fail!\n");
		return -1;
	}
	GYRO_LOG("ITG1010_enable_nodata OK!\n");

	return 0;
}

static int ITG1010_set_delay(u64 ns)
{
	return 0;
}

static int ITG1010_get_data(int *x , int *y, int *z, int *status)
{
	char buff[ITG1010_BUFSIZE];
	int ret = 0;

	ITG1010_ReadGyroData(obj_i2c_data->client, buff, ITG1010_BUFSIZE);

	ret = sscanf(buff, "%x %x %x", x, y, z);
	*status = SENSOR_STATUS_ACCURACY_MEDIUM;

	return 0;
}


/*----------------------------------------------------------------------------*/
static int ITG1010_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_client *new_client;
	struct ITG1010_i2c_data *obj;
	struct gyro_control_path ctl = {0};
	struct gyro_data_path data = {0};
	int i;
	int err = 0;
	int result;

	/*GYRO_LOG();*/
	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj) {
		err = -ENOMEM;
		goto exit;
	}

	memset(obj, 0, sizeof(struct ITG1010_i2c_data));

	obj->hw = hw;
	err = hwmsen_get_convert(obj->hw->direction, &obj->cvt);
	if (err) {
		GYRO_ERR("invalid direction: %d\n", obj->hw->direction);
		goto exit;
	}

	if (0 != obj->hw->addr) {
		client->addr = obj->hw->addr >> 1;
		GYRO_LOG("gyro_use_i2c_addr: %x\n", client->addr);
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

	err = misc_register(&ITG1010_device);
	if (err) {
		GYRO_ERR("ITG1010_device misc register failed!\n");
		goto exit_misc_device_register_failed;
	}
	ctl.is_use_common_factory = false;

	err = ITG1010_create_attr(&(ITG1010_init_info.platform_diver_addr->driver));
	if (err) {
		GYRO_ERR("ITG1010 create attribute err = %d\n", err);
		goto exit_create_attr_failed;
	}


	ctl.open_report_data = ITG1010_open_report_data;
	ctl.enable_nodata = ITG1010_enable_nodata;
	ctl.set_delay  = ITG1010_set_delay;
	ctl.is_report_input_direct = false;
	ctl.is_support_batch = obj->hw->is_batch_supported;

	err = gyro_register_control_path(&ctl);
	if (err) {
		GYRO_ERR("register gyro control path err\n");
		goto exit_kfree;
	}

	data.get_data = ITG1010_get_data;
	data.vender_div = DEGREE_TO_RAD;
	err = gyro_register_data_path(&data);
	if (err) {
		GYRO_ERR("gyro_register_data_path fail = %d\n", err);
		goto exit_kfree;
	}

#if INV_GYRO_AUTO_CALI == 1
	mutex_init(&obj->temperature_mutex);
	mutex_init(&obj->raw_data_mutex);


	/* create a class to avoid event drop by uevent_ops->filter function (dev_uevent_filter()) */
	inv_daemon_class = class_create(THIS_MODULE, INV_DAEMON_CLASS_NAME);
	if (IS_ERR(inv_daemon_class)) {
		GYRO_ERR("cannot create inv daemon class, %s\n", INV_DAEMON_CLASS_NAME);
		goto exit_class_create_failed;
	}

	inv_daemon_device = kzalloc(sizeof(struct device), GFP_KERNEL);
	if (!inv_daemon_device) {
		GYRO_ERR("cannot allocate inv daemon device, %s\n", INV_DAEMON_DEVICE_NAME);
		goto exit_device_register_failed;
	}
	inv_daemon_device->init_name = INV_DAEMON_DEVICE_NAME;
	inv_daemon_device->class = inv_daemon_class;
	inv_daemon_device->release = (void (*)(struct device *))kfree;
	result = device_register(inv_daemon_device);
	if (result) {
		GYRO_ERR("cannot register inv daemon device, %s\n", INV_DAEMON_DEVICE_NAME);
		goto exit_device_register_failed;
	}

	result = 0;
	for (i = 0; i < ARRAY_SIZE(inv_daemon_dev_attributes); i++) {
		result = device_create_file(inv_daemon_device, inv_daemon_dev_attributes[i]);
		if (result)
			break;
	}
	if (result) {
		while (--i >= 0)
			device_remove_file(inv_daemon_device, inv_daemon_dev_attributes[i]);
		GYRO_ERR("cannot create inv daemon dev attr.\n");
		goto exit_create_file_failed;
	}

#endif
	ITG1010_init_flag = 0;

	GYRO_LOG("%s: OK\n", __func__);
	return 0;

#if INV_GYRO_AUTO_CALI == 1
exit_create_file_failed:
	device_unregister(inv_daemon_device);
exit_device_register_failed:
	class_destroy(inv_daemon_class);
exit_class_create_failed:
	hwmsen_detach(ID_GYROSCOPE);
#endif
exit_create_attr_failed:
	misc_deregister(&ITG1010_device);
exit_misc_device_register_failed:
exit_init_failed:
exit_kfree:
exit:
	kfree(obj);
	obj = NULL;
	ITG1010_init_flag =  -1;
	GYRO_ERR("%s: err = %d\n", __func__, err);
	return err;
}

/*----------------------------------------------------------------------------*/
static int ITG1010_i2c_remove(struct i2c_client *client)
{
	int err = 0;

#if INV_GYRO_AUTO_CALI == 1
	int i;

	for (i = 0; i < ARRAY_SIZE(inv_daemon_dev_attributes); i++)
		device_remove_file(inv_daemon_device, inv_daemon_dev_attributes[i]);

	device_unregister(inv_daemon_device);
	class_destroy(inv_daemon_class);
#endif
	err = ITG1010_delete_attr(&(ITG1010_init_info.platform_diver_addr->driver));
	if (err)
		GYRO_ERR("ITG1010_delete_attr fail: %d\n", err);

	err = misc_deregister(&ITG1010_device);
	if (err)
		GYRO_ERR("misc_deregister fail: %d\n", err);

	ITG1010_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));
	return 0;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static int ITG1010_remove(void)
{
	GYRO_LOG();
	ITG1010_power(hw, 0);
	i2c_del_driver(&ITG1010_i2c_driver);
	return 0;
}
/*----------------------------------------------------------------------------*/
static int ITG1010_local_init(struct platform_device *pdev)
{
	/* printk("fwq loccal init+++\n"); */
	gyroPltFmDev = pdev;

	ITG1010_power(hw, 1);
	if (i2c_add_driver(&ITG1010_i2c_driver)) {
		GYRO_ERR("add driver error\n");
		return -1;
	}
	if (-1 == ITG1010_init_flag)
		return -1;

	return 0;
}

/*----------------------------------------------------------------------------*/
static int __init ITG1010_init(void)
{
	const char *name = "mediatek,itg1010";

	hw = get_gyro_dts_func(name, hw);
	if (!hw)
		GYRO_ERR("get dts info fail\n");

	gyro_driver_add(&ITG1010_init_info);

	return 0;
}
/*----------------------------------------------------------------------------*/
static void __exit ITG1010_exit(void)
{
	/*GYRO_LOG();*/
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
