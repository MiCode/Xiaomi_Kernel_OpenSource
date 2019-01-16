
/* mmc3524x.c - mmc3524x compass driver
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 * GNU General Public License for more details.
 *
 */

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/earlysuspend.h>
#include <linux/time.h>
#include <linux/hrtimer.h>

#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>


#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>



#include <mag.h>
/* #include <linux/batch.h> */



/*-------------------------MT6516&MT6573 define-------------------------------*/

#define POWER_NONE_MACRO MT65XX_POWER_NONE

#include <cust_mag.h>
#include "mmc3524x.h"
#include <linux/hwmsen_helper.h>
/*----------------------------------------------------------------------------*/
#define DEBUG 1
#define MMC3524X_DEV_NAME		"mmc3524x"
#define DRIVER_VERSION			"1.0.0"
/*----------------------------------------------------------------------------*/
#define MMC3524X_DEBUG		1
#define MMC3524X_DEBUG_MSG	1
#define MMC3524X_DEBUG_FUNC	1
#define MMC3524X_DEBUG_DATA	1
#define MAX_FAILURE_COUNT	3
#define MMC3524X_RETRY_COUNT	3
#define MMC3524X_DEFAULT_DELAY	100
#define MMC3524X_BUFSIZE	0x20

#define MSE_TAG					"MSENSOR"

#if MMC3524X_DEBUG_MSG
#define MMCDBG(format, ...)	pr_info("mmc3524x " format "\n", ## __VA_ARGS__)

#define MSE_FUN(f)				pr_info(MSE_TAG" %s\r\n", __func__)
#define MSE_ERR(fmt, args...)	pr_err(MSE_TAG" %s %d : \r\n"fmt, __func__, __LINE__, ##args)
#define MSE_LOG(fmt, args...)	pr_info(MSE_TAG fmt, ##args)

#else
#define MMCDBG(format, ...)
#endif

#if MMC3524X_DEBUG_FUNC
#define MMCFUNC(func) pr_info("mmc3524x " func " is called\n")
#else
#define MMCFUNC(func)
#endif

#ifndef MMC3524X_IOC_READ_REG
#define MMC3524X_IOC_READ_REG			_IOWR(MSENSOR, 0x23, unsigned char)
#endif

#ifndef MMC3524X_IOC_WRITE_REG
#define MMC3524X_IOC_WRITE_REG			_IOW(MSENSOR,  0x24, unsigned char[2])
#endif

#ifndef MMC3524X_IOC_READ_REGS
#define MMC3524X_IOC_READ_REGS			_IOWR(MSENSOR, 0x25, unsigned char[10])
#endif

#ifdef CONFIG_COMPAT
#ifndef COMPAT_MMC3524X_IOC_READ_REG
#define COMPAT_MMC3524X_IOC_READ_REG			_IOWR(MSENSOR, 0x23, unsigned char)
#endif

#ifndef COMPAT_MMC3524X_IOC_WRITE_REG
#define COMPAT_MMC3524X_IOC_WRITE_REG			_IOW(MSENSOR,  0x24, unsigned char[2])
#endif

#ifndef COMPAT_MMC3524X_IOC_READ_REGS
#define COMPAT_MMC3524X_IOC_READ_REGS			_IOWR(MSENSOR, 0x25, unsigned char[10])
#endif
#endif

static struct i2c_client *this_client;


/* calibration msensor and orientation data */
static int sensor_data[CALIBRATION_DATA_SIZE];
static struct mutex sensor_data_mutex;
static struct mutex read_i2c_xyz;
static struct mutex i2c_mutex; /* add by wxj */

static DECLARE_WAIT_QUEUE_HEAD(data_ready_wq);
static DECLARE_WAIT_QUEUE_HEAD(open_wq);

static int mmcd_delay = MMC3524X_DEFAULT_DELAY;

static atomic_t open_flag = ATOMIC_INIT(0);
static atomic_t m_flag = ATOMIC_INIT(0);
static atomic_t o_flag = ATOMIC_INIT(0);
static atomic_t suspend = ATOMIC_INIT(0);

/*----------------------------------------------------------------------------*/
static const struct i2c_device_id mmc3524x_i2c_id[] = {{MMC3524X_DEV_NAME, 0}, {} };
struct i2c_board_info __initdata i2c_mmc3524x = { I2C_BOARD_INFO("mmc3524x", (0x60>>1))};

/*the adapter id will be available in customization*/
/* static unsigned short mmc3524x_force[] = {0x00, MMC3524x_I2C_ADDR, I2C_CLIENT_END, I2C_CLIENT_END}; */
/* static const unsigned short *const mmc3524x_forces[] = { mmc3524x_force, NULL }; */
/* static struct i2c_client_address_data mmc3524x_addr_data = { .forces = mmc3524x_forces,}; */
/*----------------------------------------------------------------------------*/
static int mmc3524x_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int mmc3524x_i2c_remove(struct i2c_client *client);



static int mmc3524x_init_flag = -1;	/* 0 = OK, -1 = fail */

static int mmc3524x_remove(void);
static int mmc3524x_local_init(void);

static int mmc3524x_m_open_report_data(int en);
static int mmc3524x_m_set_delay(u64 delay);
static int mmc3524x_m_enable(int en);
static int mmc3524x_o_open_report_data(int en);
static int mmc3524x_o_set_delay(u64 delay);
static int mmc3524x_o_enable(int en);
static int mmc3524x_get_data_m(int *x, int *y, int *z, int *status);
static int mmc3524x_get_data_o(int *x, int *y, int *z, int *status);

/*----------------------------------------------------------------------------*/
enum {
	MMC_FUN_DEBUG	= 0x01,
	MMC_DATA_DEBUG = 0x02,
	MMC_HWM_DEBUG	= 0x04,
	MMC_CTR_DEBUG	= 0x08,
	MMC_I2C_DEBUG	= 0x10,
};

/* Define Delay time */
#define MMC3524X_DELAY_TM		10	/* ms */
#define MMC3524X_DELAY_SET		50	/* ms */
#define MMC3524X_DELAY_RESET	50	/* ms */
#define MMC3524X_DELAY_STDN		1	/* ms */

#define MMC3524X_RESET_INTV		250
static u32 read_idx;
#define READMD	0

/*----------------------------------------------------------------------------*/
struct mmc3524x_i2c_data {
	struct i2c_client *client;
	struct mag_hw *hw;
	atomic_t layout;
	atomic_t trace;
	/* atomic_t suspend; */
	struct hwmsen_convert	cvt;
#if defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend	early_drv;
#endif
};

/*----------------------------------------------------------------------------*/
static struct i2c_driver mmc3524x_i2c_driver = {
	.driver = {
	/* .owner = THIS_MODULE, */
		.name	= MMC3524X_DEV_NAME,
	},
	.probe		= mmc3524x_i2c_probe,
	.remove	= mmc3524x_i2c_remove,

#if !defined(CONFIG_HAS_EARLYSUSPEND)
	.suspend	= mmc3524x_suspend,
	.resume	= mmc3524x_resume,
#endif
	.id_table = mmc3524x_i2c_id,
};


static struct mag_init_info mmc3524x_init_info = {
	.name	= MMC3524X_DEV_NAME,
	.init	= mmc3524x_local_init,
	.uninit = mmc3524x_remove,
};

/*----------------------------------------------------------------------------*/
static atomic_t dev_open_count;

/*----------------------------------------------------------------------------*/
static void mmc3524x_power(struct mag_hw *hw, unsigned int on)
{
	static unsigned int power_on;
#ifdef __USE_LINUX_REGULATOR_FRAMEWORK__

#else
	if (hw->power_id != MT65XX_POWER_NONE) {
		MSE_LOG("power %s\n", on ? "on" : "off");
		if (power_on == on)
			MSE_LOG("ignore power control: %d\n", on);
		else if (on) {
			if (!hwPowerOn(hw->power_id, hw->power_vol, "mmc3524x"))
				MSE_ERR("power on fails!!\n");
		} else{
			if (!hwPowerDown(hw->power_id, "mmc3524x"))
				MSE_ERR("power off fail!!\n");
		}
	}
#endif
	power_on = on;
}

static int I2C_RxData(char *rxData, int length)
{
	uint8_t loop_i;

#if DEBUG
	int i;
	struct i2c_client *client = this_client;
	struct mmc3524x_i2c_data *data = i2c_get_clientdata(client);
	char addr = rxData[0];
#endif

	/* Caller should check parameter validity.*/
	if ((rxData == NULL) || (length < 1))
		return -EINVAL;

	mutex_lock(&i2c_mutex);
	for (loop_i = 0; loop_i < MMC3524X_RETRY_COUNT; loop_i++) {
		this_client->addr = (this_client->addr & I2C_MASK_FLAG) | I2C_WR_FLAG;
		if (i2c_master_send(this_client, (const char *)rxData, ((length<<0X08) | 0X01)))
			break;
		MSE_LOG("I2C_RxData delay!\n");
		mdelay(10);
	}
	this_client->addr = this_client->addr & I2C_MASK_FLAG;
	mutex_unlock(&i2c_mutex);

	if (loop_i >= MMC3524X_RETRY_COUNT) {
		MSE_ERR("%s retry over %d\n", __func__, MMC3524X_RETRY_COUNT);
		return -EIO;
	}
#if DEBUG
	if (atomic_read(&data->trace) & MMC_I2C_DEBUG) {
		MSE_LOG("RxData: len=%02x, addr=%02x\n	data=", length, addr);
		for (i = 0; i < length; i++)
			MSE_LOG(" %02x", rxData[i]);

		MSE_LOG("\n");
	}
#endif
	return 0;
}

static int I2C_TxData(char *txData, int length)
{
	uint8_t loop_i;

#if DEBUG
	int i;
	struct i2c_client *client = this_client;
	struct mmc3524x_i2c_data *data = i2c_get_clientdata(client);
#endif

	/* Caller should check parameter validity.*/
	if ((txData == NULL) || (length < 2))
		return -EINVAL;

	mutex_lock(&i2c_mutex);
	this_client->addr = this_client->addr & I2C_MASK_FLAG;
	for (loop_i = 0; loop_i < MMC3524X_RETRY_COUNT; loop_i++) {
		if (i2c_master_send(this_client, (const char *)txData, length) > 0)
			break;

		MSE_LOG("I2C_TxData delay!\n");
		mdelay(10);
	}
	mutex_unlock(&i2c_mutex);

	if (loop_i >= MMC3524X_RETRY_COUNT) {
		MSE_ERR("%s retry over %d\n", __func__, MMC3524X_RETRY_COUNT);
		return -EIO;
	}
#if DEBUG
	if (atomic_read(&data->trace) & MMC_I2C_DEBUG) {
		MSE_LOG("TxData: len=%02x, addr=%02x\n	data=", length, txData[0]);
		for (i = 0; i < (length-1); i++)
			MSE_LOG(" %02x", txData[i + 1]);

		MSE_LOG("\n");
	}
#endif
	return 0;
}

/* Daemon application save the data */
static int ECS_SaveData(int buf[12])
{
#if DEBUG
	struct i2c_client *client = this_client;
	struct mmc3524x_i2c_data *data = i2c_get_clientdata(client);
#endif

	mutex_lock(&sensor_data_mutex);
	memcpy(sensor_data, buf, sizeof(sensor_data));
	mutex_unlock(&sensor_data_mutex);

#if DEBUG
	if (atomic_read(&data->trace) & MMC_HWM_DEBUG) {
		MSE_LOG("Get daemon data: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d!\n",
			sensor_data[0], sensor_data[1], sensor_data[2], sensor_data[3],
			sensor_data[4], sensor_data[5], sensor_data[6], sensor_data[7],
			sensor_data[8], sensor_data[9], sensor_data[10], sensor_data[11]);
	}
#endif

	return 0;
}

static int ECS_ReadXYZData(int *vec, int size)
{
	unsigned char data[6] = {0, 0, 0, 0, 0, 0};

#if READMD
	int MD_times = 0;
#endif
	static int last_data[3];
	/* struct timespec time1, time2, time3,time4,delay,aa; */
	struct timespec time1, time2, time3;
#if DEBUG
	struct i2c_client *client = this_client;
	struct mmc3524x_i2c_data *clientdata = i2c_get_clientdata(client);
#endif
/* set_current_state(TASK_INTERRUPTIBLE); */
time1 = current_kernel_time();

	if (size < 3)
		return -1;

	mutex_lock(&read_i2c_xyz);
	time2 = current_kernel_time();

	if (!(read_idx % MMC3524X_RESET_INTV)) {
		/* Reset Sensor Periodly */
		/* SET */
		data[0] = MMC3524X_REG_CTRL;
		data[1] = MMC3524X_CTRL_REFILL;
			/* not check return value here, assume it always OK */
		I2C_TxData(data, 2);
			/* wait external capacitor charging done for next RM */
		msleep(MMC3524X_DELAY_SET);
		data[0] = MMC3524X_REG_CTRL;
		data[1] = MMC3524X_CTRL_SET;
			/* not check return value here, assume it always OK */
		I2C_TxData(data, 2);
			/* wait external capacitor charging done for next RM */
		usleep_range(1000, 1500);
		data[0] = MMC3524X_REG_CTRL;
		data[1] = 0;
			/* not check return value here, assume it always OK */
		I2C_TxData(data, 2);
			/* wait external capacitor charging done for next RM */
		usleep_range(1000, 1500);
	}

	time3 = current_kernel_time();
	/* send TM cmd before read */
	data[0] = MMC3524X_REG_CTRL;
	data[1] = MMC3524X_CTRL_TM;
	/* not check return value here, assume it always OK */
	I2C_TxData(data, 2);
	msleep(MMC3524X_DELAY_TM);

#if READMD
	/* Read MD */
	data[0] = MMC3524X_REG_DS;
	I2C_RxData(data, 1);
	while (!(data[0] & 0x01)) {
		usleep_range(1000, 1500);
			/* Read MD again*/
		data[0] = MMC3524X_REG_DS;
		I2C_RxData(data, 1);
		if (data[0] & 0x01)
			break;
		MD_times++;
		if (MD_times > 3) {
			MSE_LOG("TM not work!!");
			mutex_unlock(&read_i2c_xyz);
			return -EFAULT;
		}
	}
#endif
	read_idx++;
	data[0] = MMC3524X_REG_DATA;
	if (I2C_RxData(data, 6) < 0) {
		mutex_unlock(&read_i2c_xyz);
		return -EFAULT;
	}
	vec[0] = data[1] << 8 | data[0];
	vec[1] = data[3] << 8 | data[2];
	vec[2] = data[5] << 8 | data[4];
	vec[2] = 65536 - vec[2];
#if DEBUG
	if (atomic_read(&clientdata->trace) & MMC_DATA_DEBUG)
		MSE_LOG("[X - %04x] [Y - %04x] [Z - %04x]\n", vec[0], vec[1], vec[2]);

#endif
	mutex_unlock(&read_i2c_xyz);
	last_data[0] = vec[0];
	last_data[1] = vec[1];
	last_data[2] = vec[2];
	return 0;
}

static int ECS_GetRawData(int data[3])
{
	int err = 0;
	err = ECS_ReadXYZData(data, 3);
	if (err != 0) {
		MSE_ERR("MMC3524X_IOC_TM failed\n");
		return -1;
	}

	/* sensitivity 2048 count = 1 Guass = 100uT */
	data[0] = (data[0] - MMC3524X_OFFSET_X) * 100 / MMC3524X_SENSITIVITY_X;
	data[1] = (data[1] - MMC3524X_OFFSET_X) * 100 / MMC3524X_SENSITIVITY_X;
	data[2] = (data[2] - MMC3524X_OFFSET_X) * 100 / MMC3524X_SENSITIVITY_X;

	return err;
}
static int ECS_GetOpenStatus(void)
{
	wait_event_interruptible(open_wq, (atomic_read(&open_flag) != 0) && (atomic_read(&suspend) == 0));
	return atomic_read(&open_flag);
}


/*----------------------------------------------------------------------------*/
static int mmc3524x_ReadChipInfo(char *buf, int bufsize)
{
	if ((!buf) || (bufsize <= MMC3524X_BUFSIZE - 1))
		return -1;

	if (!this_client) {
		*buf = 0;
		return -2;
	}

	sprintf(buf, "mmc3524x Chip");
	return 0;
}

/*----------------------------------------------------------------------------*/
static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	char strbuf[MMC3524X_BUFSIZE] = {0};
	mmc3524x_ReadChipInfo(strbuf, MMC3524X_BUFSIZE);
	return sprintf(buf, "%s\n", strbuf);
}
/*----------------------------------------------------------------------------*/
static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{

	int sensordata[3] = {0};
	char strbuf[MMC3524X_BUFSIZE] = {0};

	ECS_GetRawData(sensordata);

	sprintf(strbuf, "%d %d %d\n", sensordata[0], sensordata[1], sensordata[2]);

	return sprintf(buf, "%s\n", strbuf);
}
/*----------------------------------------------------------------------------*/
static ssize_t show_posturedata_value(struct device_driver *ddri, char *buf)
{
	int tmp[3] = {0};
	char strbuf[MMC3524X_BUFSIZE] = {0};
	tmp[0] = sensor_data[8] * CONVERT_O / CONVERT_O_DIV;
	tmp[1] = sensor_data[9] * CONVERT_O / CONVERT_O_DIV;
	tmp[2] = sensor_data[10] * CONVERT_O / CONVERT_O_DIV;
	sprintf(strbuf, "%d, %d, %d\n", tmp[0], tmp[1], tmp[2]);

	return sprintf(buf, "%s\n", strbuf);
}

/*----------------------------------------------------------------------------*/
static ssize_t show_layout_value(struct device_driver *ddri, char *buf)
{
	if (NULL == this_client) {
		MSE_ERR("this_client IS NULL !\n");
		return -1;
	}

	struct mmc3524x_i2c_data *data = i2c_get_clientdata(this_client);
	if (NULL == data) {
		MSE_ERR("mmc3524x_i2c_data is null!!\n");
		return -1;
	}


	return sprintf(buf, "(%d, %d)\n[%+2d %+2d %+2d]\n[%+2d %+2d %+2d]\n",
		data->hw->direction, atomic_read(&data->layout),	data->cvt.sign[0], data->cvt.sign[1],
		data->cvt.sign[2], data->cvt.map[0], data->cvt.map[1], data->cvt.map[2]);
}
/*----------------------------------------------------------------------------*/
static ssize_t store_layout_value(struct device_driver *ddri, const char *buf, size_t count)
{
	if (NULL == this_client) {
		MSE_ERR("this_client IS NULL !\n");
		return -1;
	}

	struct mmc3524x_i2c_data *data = i2c_get_clientdata(this_client);

	if (NULL == data) {
		MSE_ERR("mmc3524x_i2c_data is null!!\n");
		return -1;
	}

	int layout = 0;

	if (1 == sscanf(buf, "%d", &layout)) {
		atomic_set(&data->layout, layout);
		if (!hwmsen_get_convert(layout, &data->cvt))
			MSE_ERR("HWMSEN_GET_CONVERT function error!\r\n");
		else if (!hwmsen_get_convert(data->hw->direction, &data->cvt))
			MSE_ERR("invalid layout: %d, restore to %d\n", layout, data->hw->direction);
		else{
			MSE_ERR("invalid layout: (%d, %d)\n", layout, data->hw->direction);
			hwmsen_get_convert(0, &data->cvt);
		}
	} else
		MSE_ERR("invalid format = '%s'\n", buf);

	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t show_status_value(struct device_driver *ddri, char *buf)
{
	if (NULL == this_client) {
		MSE_ERR("this_client IS NULL !\n");
		return -1;
	}

	struct mmc3524x_i2c_data *data = i2c_get_clientdata(this_client);

	if (NULL == data) {
		MSE_ERR("mmc3524x_i2c_data is null!!\n");
		return -1;
	}

	ssize_t len = 0;

	if (data->hw) {
		len += snprintf(buf+len, PAGE_SIZE-len, "CUST: %d %d (%d %d)\n",
			data->hw->i2c_num, data->hw->direction, data->hw->power_id, data->hw->power_vol);
	} else
		len += snprintf(buf+len, PAGE_SIZE-len, "CUST: NULL\n");

	len += snprintf(buf+len, PAGE_SIZE-len, "OPEN: %d\n", atomic_read(&dev_open_count));
	return len;
}
/*----------------------------------------------------------------------------*/
static ssize_t show_trace_value(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	if (NULL == this_client) {
		MSE_ERR("this_client IS NULL !\n");
		return -1;
	}

	struct mmc3524x_i2c_data *data = i2c_get_clientdata(this_client);

	if (NULL == data) {
		MSE_ERR("mmc3524x_i2c_data is null!!\n");
		return -1;
	}


	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&data->trace));
	return res;
}
/*----------------------------------------------------------------------------*/
static ssize_t store_trace_value(struct device_driver *ddri, const char *buf, size_t count)
{
	int trace;
	if (NULL == this_client) {
		MSE_ERR("this_client IS NULL !\n");
		return -1;
	}

	struct mmc3524x_i2c_data *obj = i2c_get_clientdata(this_client);

	if (NULL == obj) {
		MSE_ERR("mmc3524x_i2c_data is null!!\n");
		return -1;
	}

	if (1 == sscanf(buf, "0x%x", &trace))
		atomic_set(&obj->trace, trace);
	else
		MSE_ERR("invalid content: '%s', length = %zu\n", buf, count);

	return count;
}
static ssize_t show_daemon_name(struct device_driver *ddri, char *buf)
{
	char strbuf[MMC3524X_BUFSIZE];
	memset(strbuf, 0 , sizeof(strbuf));
	/* sprintf(strbuf, "memsicd"); */
	sprintf(strbuf, "mmc3524xd");
	return sprintf(buf, "%s", strbuf);
}

/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(daemon,		S_IRUGO, show_daemon_name, NULL);
static DRIVER_ATTR(chipinfo,	S_IRUGO, show_chipinfo_value, NULL);
static DRIVER_ATTR(sensordata,	S_IRUGO, show_sensordata_value, NULL);
static DRIVER_ATTR(posturedata, S_IRUGO, show_posturedata_value, NULL);
static DRIVER_ATTR(layout,		S_IRUGO | S_IWUSR, show_layout_value, store_layout_value);
static DRIVER_ATTR(status,		S_IRUGO, show_status_value, NULL);
static DRIVER_ATTR(trace,		S_IRUGO | S_IWUSR, show_trace_value, store_trace_value);

/*----------------------------------------------------------------------------*/
static struct driver_attribute *mmc3524x_attr_list[] = {
	&driver_attr_daemon,
	&driver_attr_chipinfo,
	&driver_attr_sensordata,
	&driver_attr_posturedata,
	&driver_attr_layout,
	&driver_attr_status,
	&driver_attr_trace,
};

/*----------------------------------------------------------------------------*/
static int mmc3524x_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)ARRAY_SIZE(mmc3524x_attr_list);/* (sizeof(mmc3524x_attr_list)/sizeof(mmc3524x_attr_list[0])); */
	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, mmc3524x_attr_list[idx]);
		if (err != 0) {
			MSE_ERR("driver_create_file (%s) = %d\n", mmc3524x_attr_list[idx]->attr.name, err);
			break;
		}
	}

	return err;
}

/*----------------------------------------------------------------------------*/
static int mmc3524x_delete_attr(struct device_driver *driver)
{
	int idx;
	int num = (int)ARRAY_SIZE(mmc3524x_attr_list);/* (sizeof(mmc3524x_attr_list)/sizeof(mmc3524x_attr_list[0])); */

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, mmc3524x_attr_list[idx]);

	return 0;
}


/*----------------------------------------------------------------------------*/
static int mmc3524x_open(struct inode *inode, struct file *file)
{
	struct mmc3524x_i2c_data *obj = i2c_get_clientdata(this_client);
	int ret = -1;

	if (atomic_read(&obj->trace) & MMC_CTR_DEBUG)
		MSE_LOG("Open device node:mmc3524x\n");

	ret = nonseekable_open(inode, file);

	return ret;
}
/*----------------------------------------------------------------------------*/
static int mmc3524x_release(struct inode *inode, struct file *file)
{
	struct mmc3524x_i2c_data *obj = i2c_get_clientdata(this_client);
	atomic_dec(&dev_open_count);
	if (atomic_read(&obj->trace) & MMC_CTR_DEBUG)
		MSE_LOG("Release device node:mmc3524x\n");

	return 0;
}
/*----------------------------------------------------------------------------*/
/* static int mmc3524x_ioctl(struct inode *inode, struct file *file, unsigned int cmd,unsigned long arg) */
static long mmc3524x_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)

{
	void __user *argp = (void __user *)arg;

	/* NOTE: In this function the size of "char" should be 1-byte. */
	char buff[MMC3524X_BUFSIZE];				/* for chip information */

	int value[12] = {0};			/* for SET_YPR */
	int delay;				/* for GET_DELAY */
	int status;				/* for OPEN/CLOSE_STATUS */
	short sensor_status;		/* for Orientation and Msensor status */
	unsigned char data[16] = {0};
	int vec[3] = {0};
	struct i2c_client *client = this_client;
	struct mmc3524x_i2c_data *clientdata = i2c_get_clientdata(client);
	hwm_sensor_data *osensor_data;
	uint32_t enable;
	unsigned char reg_addr;
	unsigned char reg_value;
	/* read_reg_str reg_str; */
	/* int data_reg[3] = {0}; */

	memset(buff, 0, sizeof(buff));

	switch (cmd) {
	case MMC31XX_IOC_TM:
		data[0] = MMC3524X_REG_CTRL;
		data[1] = MMC3524X_CTRL_TM;
		if (I2C_TxData(data, 2) < 0) {
			MSE_ERR("MMC3524x_IOC_TM failed\n");
			return -EFAULT;
		}
			/* wait TM done for coming data read */
			/* msleep(MMC3524X_DELAY_TM); */
		break;

	case MMC31XX_IOC_SET:
	case MMC31XX_IOC_RM:
		data[0] = MMC3524X_REG_CTRL;
		data[1] = MMC3524X_CTRL_REFILL;
		if (I2C_TxData(data, 2) < 0) {
			MSE_ERR("MMC3524x_IOC_SET failed\n");
			return -EFAULT;
		}
			/* wait external capacitor charging done for next SET/RESET */
		msleep(MMC3524X_DELAY_SET);
		data[0] = MMC3524X_REG_CTRL;
		data[1] = MMC3524X_CTRL_SET;
		if (I2C_TxData(data, 2) < 0) {
			MSE_ERR("MMC3524x_IOC_SET failed\n");
			return -EFAULT;
		}
			/* wait external capacitor charging done for next SET/RESET */
		usleep_range(1000, 1500);
		data[0] = MMC3524X_REG_CTRL;
		data[1] = 0;
		if (I2C_TxData(data, 2) < 0) {
			MSE_ERR("MMC3524x_IOC_SET failed\n");
			return -EFAULT;
		}
			/* wait external capacitor charging done for next SET/RESET */
		usleep_range(1000, 1500);
		break;

	case MMC31XX_IOC_RESET:
	case MMC31XX_IOC_RRM:
		data[0] = MMC3524X_REG_CTRL;
		data[1] = MMC3524X_CTRL_REFILL;
		if (I2C_TxData(data, 2) < 0) {
			MSE_ERR("MMC3524x_IOC_SET failed\n");
			return -EFAULT;
		}
			/* wait external capacitor charging done for next SET/RESET */
		msleep(MMC3524X_DELAY_RESET);
		data[0] = MMC3524X_REG_CTRL;
		data[1] = MMC3524X_CTRL_RESET;
		if (I2C_TxData(data, 2) < 0) {
			MSE_ERR("MMC3524x_IOC_SET failed\n");
			return -EFAULT;
		}
			/* wait external capacitor charging done for next SET/RESET */
		usleep_range(1000, 1500);
		data[0] = MMC3524X_REG_CTRL;
		data[1] = 0;
		if (I2C_TxData(data, 2) < 0) {
			MSE_ERR("MMC3524x_IOC_SET failed\n");
			return -EFAULT;
		}
			/* wait external capacitor charging done for next SET/RESET */
		usleep_range(1000, 1500);
		break;

	case MMC31XX_IOC_READ:
		data[0] = MMC3524X_REG_DATA;
		if (I2C_RxData(data, 6) < 0) {
			MSE_ERR("MMC3524x_IOC_READ failed\n");
			return -EFAULT;
		}
		/* vec[0] = data[0] << 8 | data[1]; */
		/* vec[1] = data[2] << 8 | data[3]; */
		/* vec[2] = data[4] << 8 | data[5]; */
		vec[0] = data[1] << 8 | data[0];
		vec[1] = data[3] << 8 | data[2];
		vec[2] = data[5] << 8 | data[4];
#if DEBUG
		if (atomic_read(&clientdata->trace) & MMC_DATA_DEBUG)
			MSE_LOG("[X - %04x] [Y - %04x] [Z - %04x]\n", vec[0], vec[1], vec[2]);

#endif
		if (copy_to_user(argp, vec, sizeof(vec))) {
			MSE_ERR("MMC3524x_IOC_READ: copy to user failed\n");
			return -EFAULT;
		}
		break;

	case MMC31XX_IOC_READXYZ:
		ECS_ReadXYZData(vec, 3);
		if (copy_to_user(argp, vec, sizeof(vec))) {
			MSE_ERR("MMC3524x_IOC_READXYZ: copy to user failed\n");
			return -EFAULT;
		}
		break;

	case MMC3524X_IOC_READ_REG:
		MSE_LOG("MMC3524X_IOC_READ_REG\n");
		if (copy_from_user(&reg_addr, argp, sizeof(reg_addr)))
			return -EFAULT;
		data[0] = reg_addr;
		if (I2C_RxData(data, 1) < 0)
				/* mutex_unlock(&ecompass_lock); */
				return -EFAULT;

		MSE_LOG("<7>planar Read register No. 0x%0x\n", data[0]);
		reg_value = data[0];
		if (copy_to_user(argp, &reg_value, sizeof(reg_value)))
				/* mutex_unlock(&ecompass_lock); */
			return -EFAULT;

		break;

	case MMC3524X_IOC_WRITE_REG:
		MSE_LOG(" MMC3524X_IOC_WRITE_REG\n");
		if (copy_from_user(&data, argp, sizeof(data)))
			return -EFAULT;
		if (I2C_TxData(data, 2) < 0)
			/* mutex_unlock(&ecompass_lock); */
			return -EFAULT;

		MSE_LOG("<7>planar Write '0x%0x' to	register No. 0x%0x\n", data[0], data[1]);

		break;

	case MMC3524X_IOC_READ_REGS:
		MSE_LOG(" MMC3524X_IOC_READ_REGS\n");
		if (copy_from_user(&data, argp, sizeof(data)))
			return -EFAULT;
		MSE_LOG("<7> planar Read %d registers from 0x%0x\n", data[1], data[0]);
		if (I2C_RxData(data, data[1]) < 0)
				/* mutex_unlock(&ecompass_lock); */
			return -EFAULT;

		MSE_LOG("<7> data: %x %x %x\n%x %x %x\n", data[0], data[1], data[2], data[3], data[4], data[5]);
		if (copy_to_user(argp, data, sizeof(data)))
				/* mutex_unlock(&ecompass_lock); */
			return -EFAULT;

		break;

	case ECOMPASS_IOC_GET_DELAY:
		delay = mmcd_delay;
		if (copy_to_user(argp, &delay, sizeof(delay))) {
			MSE_ERR("copy_to_user failed.");
			return -EFAULT;
		}
		break;

	case ECOMPASS_IOC_SET_YPR:
		if (argp == NULL) {
			MSE_ERR("invalid argument.");
			return -EINVAL;
		}
		if (copy_from_user(value, argp, sizeof(value)))	{
			MSE_ERR("copy_from_user failed.");
			return -EFAULT;
		}
		ECS_SaveData(value);
		break;

	case ECOMPASS_IOC_GET_OPEN_STATUS:
		status = ECS_GetOpenStatus();
		if (copy_to_user(argp, &status, sizeof(status))) {
			MSE_ERR("copy_to_user failed.");
			return -EFAULT;
		}
		break;

	case ECOMPASS_IOC_GET_MFLAG:
		if (atomic_read(&suspend) == 0)
			sensor_status = atomic_read(&m_flag);
		else
			sensor_status = 0;

		if (copy_to_user(argp, &sensor_status, sizeof(sensor_status))) {
			MSE_ERR("copy_to_user failed.");
			return -EFAULT;
		}
		break;

	case ECOMPASS_IOC_GET_OFLAG:

		if (atomic_read(&suspend) == 0)
			sensor_status = atomic_read(&o_flag);
		else
			sensor_status = 0;


		if (copy_to_user(argp, &sensor_status, sizeof(sensor_status))) {
			MSE_ERR("copy_to_user failed.");
			return -EFAULT;
		}
		break;


	case MSENSOR_IOCTL_READ_CHIPINFO:
		if (argp == NULL) {
			MSE_ERR("IO parameter pointer is NULL!\r\n");
			break;
		}

		mmc3524x_ReadChipInfo(buff, MMC3524X_BUFSIZE);
		if (copy_to_user(argp, buff, strlen(buff)+1))
			return -EFAULT;

		break;

	case MSENSOR_IOCTL_READ_SENSORDATA:
		if (argp == NULL) {
			MSE_ERR("IO parameter pointer is NULL!\r\n");
			break;
		}
		ECS_GetRawData(vec);
		sprintf(buff, "%x %x %x", vec[0], vec[1], vec[2]);
		if (copy_to_user(argp, buff, strlen(buff)+1))
			return -EFAULT;

		break;

	case ECOMPASS_IOC_GET_LAYOUT:
		status = atomic_read(&clientdata->layout);
		if (copy_to_user(argp, &status, sizeof(status))) {
			MSE_ERR("copy_to_user failed.");
			return -EFAULT;
		}
		break;

	case MSENSOR_IOCTL_SENSOR_ENABLE:

		if (argp == NULL) {
			MSE_ERR("IO parameter pointer is NULL!\r\n");
			break;
		}
		if (copy_from_user(&enable, argp, sizeof(enable))) {
			MSE_ERR("copy_from_user failed.");
			return -EFAULT;
		} else{
			MSE_LOG("MSENSOR_IOCTL_SENSOR_ENABLE enable=%d!\r\n", enable);
			if (1 == enable) {
				atomic_set(&o_flag, 1);
				atomic_set(&open_flag, 1);
			} else{
				atomic_set(&o_flag, 0);
				if (atomic_read(&m_flag) == 0)
					atomic_set(&open_flag, 0);
			}
			wake_up(&open_wq);

		}

		break;

	case MSENSOR_IOCTL_READ_FACTORY_SENSORDATA:
		if (argp == NULL) {
			MSE_ERR("IO parameter pointer is NULL!\r\n");
			break;
		}
		osensor_data = (hwm_sensor_data *)buff;
		mutex_lock(&sensor_data_mutex);

		osensor_data->values[0] = sensor_data[8] * CONVERT_O;
		osensor_data->values[1] = sensor_data[9] * CONVERT_O;
		osensor_data->values[2] = sensor_data[10] * CONVERT_O;
		osensor_data->status = sensor_data[11];
		osensor_data->value_divide = CONVERT_O_DIV;

		mutex_unlock(&sensor_data_mutex);

		sprintf(buff, "%x %x %x %x %x", osensor_data->values[0], osensor_data->values[1],
			osensor_data->values[2], osensor_data->status, osensor_data->value_divide);
		if (copy_to_user(argp, buff, strlen(buff)+1))
			return -EFAULT;

		break;

	default:
		MSE_ERR("%s not supported = 0x%04x", __func__, cmd);
		return -ENOIOCTLCMD;
		break;
	}

	return 0;
}
#ifdef CONFIG_COMPAT
long mmc3524x_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = 0;

	void __user *arg64 = compat_ptr(arg);

	if (!file->f_op || !file->f_op->unlocked_ioctl) {
		MSE_ERR("file->f_op OR file->f_op->unlocked_ioctl is null!\n");
		return -ENOTTY;
	}

	switch (cmd) {
	case COMPAT_MMC31XX_IOC_TM:
		ret = file->f_op->unlocked_ioctl(file, MMC31XX_IOC_TM, (unsigned long)arg64);
		if (ret < 0)
			MSE_ERR("COMPAT_MMC31XX_IOC_TM is failed!\n");

		break;

	case COMPAT_MMC31XX_IOC_SET:
		ret = file->f_op->unlocked_ioctl(file, MMC31XX_IOC_SET, (unsigned long)arg64);
		if (ret < 0)
			MSE_ERR("COMPAT_MMC31XX_IOC_SET is failed!\n");

		break;

	case COMPAT_MMC31XX_IOC_RM:
		ret = file->f_op->unlocked_ioctl(file, MMC31XX_IOC_RM, (unsigned long)arg64);
		if (ret < 0)
			MSE_ERR("COMPAT_MMC31XX_IOC_RM is failed!\n");

		break;

	case COMPAT_MMC31XX_IOC_RESET:
		ret = file->f_op->unlocked_ioctl(file, MMC31XX_IOC_RESET, (unsigned long)arg64);
		if (ret < 0)
			MSE_ERR("COMPAT_MMC31XX_IOC_RESET is failed!\n");


	case COMPAT_MMC31XX_IOC_RRM:
		ret = file->f_op->unlocked_ioctl(file, MMC31XX_IOC_RRM, (unsigned long)arg64);
		if (ret < 0)
			MSE_ERR("COMPAT_MMC31XX_IOC_RRM is failed!\n");

		break;

	case COMPAT_MMC31XX_IOC_READ:
		ret = file->f_op->unlocked_ioctl(file, MMC31XX_IOC_READ, (unsigned long)arg64);
		if (ret < 0)
			MSE_ERR("COMPAT_MMC31XX_IOC_READ is failed!\n");

		break;

	case COMPAT_MMC31XX_IOC_READXYZ:
		ret = file->f_op->unlocked_ioctl(file, MMC31XX_IOC_READXYZ, (unsigned long)arg64);
		if (ret < 0)
			MSE_ERR("COMPAT_MMC31XX_IOC_READXYZ is failed!\n");

		break;

	case COMPAT_MMC3524X_IOC_READ_REG:
		ret = file->f_op->unlocked_ioctl(file, MMC3524X_IOC_READ_REG, (unsigned long)arg64);
		if (ret < 0)
			MSE_ERR("COMPAT_MMC3524X_IOC_READ_REG is failed!\n");
		break;

	case COMPAT_MMC3524X_IOC_WRITE_REG:
		ret = file->f_op->unlocked_ioctl(file, MMC3524X_IOC_WRITE_REG, (unsigned long)arg64);
		if (ret < 0)
			MSE_ERR("COMPAT_MMC3524X_IOC_WRITE_REG is failed!\n");

			break;

	case COMPAT_MMC3524X_IOC_READ_REGS:
		ret = file->f_op->unlocked_ioctl(file, MMC3524X_IOC_READ_REGS, (unsigned long)arg64);
		if (ret < 0)
			MSE_ERR("COMPAT_MMC3524X_IOC_READ_REGS is failed!\n");

		break;

	case COMPAT_ECOMPASS_IOC_GET_DELAY:
		ret = file->f_op->unlocked_ioctl(file, ECOMPASS_IOC_GET_DELAY, (unsigned long)arg64);
		if (ret < 0)
			MSE_ERR("COMPAT_ECOMPASS_IOC_GET_DELAY is failed!\n");

		break;

	case COMPAT_ECOMPASS_IOC_SET_YPR:
		ret = file->f_op->unlocked_ioctl(file, ECOMPASS_IOC_SET_YPR, (unsigned long)arg64);
		if (ret < 0)
			MSE_ERR("COMPAT_ECOMPASS_IOC_SET_YPR is failed!\n");

		break;

	case COMPAT_ECOMPASS_IOC_GET_OPEN_STATUS:
		ret = file->f_op->unlocked_ioctl(file, ECOMPASS_IOC_GET_OPEN_STATUS, (unsigned long)arg64);
		if (ret < 0)
			MSE_ERR("COMPAT_ECOMPASS_IOC_GET_OPEN_STATUS is failed!\n");

		break;

	case COMPAT_ECOMPASS_IOC_GET_MFLAG:
		ret = file->f_op->unlocked_ioctl(file, ECOMPASS_IOC_GET_MFLAG, (unsigned long)arg64);
		if (ret < 0)
			MSE_ERR("COMPAT_ECOMPASS_IOC_GET_MFLAG is failed!\n");

		break;

	case COMPAT_ECOMPASS_IOC_GET_OFLAG:
		ret = file->f_op->unlocked_ioctl(file, ECOMPASS_IOC_GET_OFLAG, (unsigned long)arg64);
		if (ret < 0)
			MSE_ERR("COMPAT_ECOMPASS_IOC_GET_OFLAG is failed!\n");

		break;


	case COMPAT_MSENSOR_IOCTL_READ_CHIPINFO:
		if (arg64 == NULL) {
				MSE_ERR("IO parameter pointer is NULL!\r\n");
				break;
		}

		ret = file->f_op->unlocked_ioctl(file, MSENSOR_IOCTL_READ_CHIPINFO, (unsigned long)arg64);
		if (ret < 0)
			MSE_ERR("COMPAT_MSENSOR_IOCTL_READ_CHIPINFO is failed!\n");
		break;

	case COMPAT_MSENSOR_IOCTL_READ_SENSORDATA:
		if (arg64 == NULL) {
			MSE_ERR("IO parameter pointer is NULL!\r\n");
			break;
		}

		ret = file->f_op->unlocked_ioctl(file, MSENSOR_IOCTL_READ_SENSORDATA, (unsigned long)arg64);
		if (ret < 0)
			MSE_ERR("COMPAT_MSENSOR_IOCTL_READ_SENSORDATA is failed!\n");

		break;

	case COMPAT_ECOMPASS_IOC_GET_LAYOUT:
		ret = file->f_op->unlocked_ioctl(file, ECOMPASS_IOC_GET_LAYOUT, (unsigned long)arg64);
		if (ret < 0)
			MSE_ERR("COMPAT_ECOMPASS_IOC_GET_LAYOUT is failed!\n");

		break;

	case COMPAT_MSENSOR_IOCTL_SENSOR_ENABLE:
		if (arg64 == NULL) {
			MSE_ERR("IO parameter pointer is NULL!\r\n");
			break;
		}

		ret = file->f_op->unlocked_ioctl(file, MSENSOR_IOCTL_SENSOR_ENABLE, (unsigned long)arg64);
		if (ret < 0)
			MSE_ERR("COMPAT_MSENSOR_IOCTL_SENSOR_ENABLE is failed!\n");

		break;

	case COMPAT_MSENSOR_IOCTL_READ_FACTORY_SENSORDATA:
		if (arg64 == NULL) {
			MSE_ERR("IO parameter pointer is NULL!\r\n");
			break;
		}

		ret = file->f_op->unlocked_ioctl(file, MSENSOR_IOCTL_READ_FACTORY_SENSORDATA, (unsigned long)arg64);
		if (ret < 0)
			MSE_ERR("COMPAT_MSENSOR_IOCTL_READ_FACTORY_SENSORDATA is failed!\n");

		break;

	default:
		MSE_ERR("%s not supported = 0x%04x", __func__, cmd);
		return -ENOIOCTLCMD;
		break;
	}
	return ret;
}
#endif
/*----------------------------------------------------------------------------*/
static const struct file_operations mmc3524x_fops = {
	/* .owner = THIS_MODULE, */
	.open = mmc3524x_open,
	.release = mmc3524x_release,
	.unlocked_ioctl = mmc3524x_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = mmc3524x_compat_ioctl,
#endif
};
/*----------------------------------------------------------------------------*/
static struct miscdevice mmc3524x_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "msensor",
	.fops = &mmc3524x_fops,
};
/*----------------------------------------------------------------------------*/

static int mmc3524x_m_open_report_data(int en)
{
	return 0;
}
static int mmc3524x_m_set_delay(u64 delay)
{
	int value = 0;
	struct i2c_client *client = this_client;

	if (NULL == client) {
		MSE_ERR("this_client IS NULL !\n");
		return -1;
	}

	struct mmc3524x_i2c_data *data = i2c_get_clientdata(client);


	if (NULL == data) {
		MSE_ERR("data IS NULL !\n");
		return -1;
	}

	value = (int)delay;
	if (value <= 20)
		mmcd_delay = 20;

	mmcd_delay = value;


	return 0;
}
static int mmc3524x_m_enable(int en)
{
	int value = 0;
	struct i2c_client *client = this_client;
	if (NULL == client) {
		MSE_ERR("this_client IS NULL !\n");
		return -1;
	}

	struct mmc3524x_i2c_data *data = i2c_get_clientdata(client);


	if (NULL == data) {
		MSE_ERR("data IS NULL !\n");
		return -1;
	}

	value = en;

	if (value == 1) {
		atomic_set(&m_flag, 1);
		atomic_set(&open_flag, 1);
	} else{
		atomic_set(&m_flag, 0);
		if (atomic_read(&o_flag) == 0)
			atomic_set(&open_flag, 0);

	}
	wake_up(&open_wq);

	return 0;
}
static int mmc3524x_o_open_report_data(int en)
{
	return 0;
}
static int mmc3524x_o_set_delay(u64 delay)
{
	return mmc3524x_m_set_delay(delay);/* the same is msensor set_delay */
}
static int mmc3524x_o_enable(int en)
{
	int value = 0;
	struct i2c_client *client = this_client;
	if (NULL == client) {
		MSE_ERR("this_client IS NULL !\n");
		return -1;
	}

	struct mmc3524x_i2c_data *data = i2c_get_clientdata(client);


	if (NULL == data) {
		MSE_ERR("data IS NULL !\n");
		return -1;
	}

	value = en;

	if (value == 1) {
		atomic_set(&o_flag, 1);
		atomic_set(&open_flag, 1);
	} else{
		atomic_set(&o_flag, 0);
		if (atomic_read(&m_flag) == 0)
			atomic_set(&open_flag, 0);

	}
	wake_up(&open_wq);


	return 0;
}
static int mmc3524x_get_data_m(int *x, int *y, int *z, int *status)
{
	struct i2c_client *client = this_client;
	if (NULL == client) {
		MSE_ERR("this_client IS NULL !\n");
		return -1;
	}
	struct mmc3524x_i2c_data *data = i2c_get_clientdata(client);


	if (NULL == data) {
		MSE_ERR("data IS NULL !\n");
		return -1;
	}

	mutex_lock(&sensor_data_mutex);

	*x = sensor_data[4] * CONVERT_M;
	*y = sensor_data[5] * CONVERT_M;
	*z = sensor_data[6] * CONVERT_M;
	*status = sensor_data[7];

	mutex_unlock(&sensor_data_mutex);
#if DEBUG
	if (atomic_read(&data->trace) & MMC_HWM_DEBUG) {
		MSE_LOG("Hwm get m-sensor data: %d, %d, %d. divide %d, status %d!\n",
			*x, *y, *z, CONVERT_M_DIV, *status);
	}
#endif


	return 0;
}
static int mmc3524x_get_data_o(int *x, int *y, int *z, int *status)
{
	struct i2c_client *client = this_client;
	if (NULL == client) {
		MSE_ERR("this_client IS NULL !\n");
		return -1;
	}
	struct mmc3524x_i2c_data *data = i2c_get_clientdata(client);


	if (NULL == data) {
		MSE_ERR("data IS NULL !\n");
		return -1;
	}

	mutex_lock(&sensor_data_mutex);

	*x = sensor_data[8] * CONVERT_O;
	*y = sensor_data[9] * CONVERT_O;
	*z = sensor_data[10] * CONVERT_O;
	*status = sensor_data[11];

	mutex_unlock(&sensor_data_mutex);
#if DEBUG
	if (atomic_read(&data->trace) & MMC_HWM_DEBUG) {
		MSE_LOG("Hwm get o-sensor data: %d, %d, %d. divide %d, status %d!\n",
						*x, *y, *z, CONVERT_O_DIV, *status);
	}
#endif

	return 0;
}

/*----------------------------------------------------------------------------*/
#ifndef	CONFIG_HAS_EARLYSUSPEND
/*----------------------------------------------------------------------------*/
static int mmc3524x_suspend(struct i2c_client *client, pm_message_t msg)
{
	struct mmc3524x_i2c_data *obj = i2c_get_clientdata(client)

	MSE_FUN(f);

	if (msg.event == PM_EVENT_SUSPEND)
		mmc3524x_power(obj->hw, 0);

	return 0;
}
/*----------------------------------------------------------------------------*/
static int mmc3524x_resume(struct i2c_client *client)
{
	struct mmc3524x_i2c_data *obj = i2c_get_clientdata(client)

	MSE_FUN(f);

	mmc3524x_power(obj->hw, 1);

	return 0;
}
/*----------------------------------------------------------------------------*/
#else /*CONFIG_HAS_EARLY_SUSPEND is defined*/
/*----------------------------------------------------------------------------*/
static void mmc3524x_early_suspend(struct early_suspend *h)
{
	struct mmc3524x_i2c_data *obj = container_of(h, struct mmc3524x_i2c_data, early_drv);
	MSE_FUN(f);
	if (NULL == obj) {
		MSE_ERR("null pointer!!\n");
		return;
	}
	atomic_set(&suspend, 1);
	mmc3524x_power(obj->hw, 0);


}
/*----------------------------------------------------------------------------*/
static void mmc3524x_late_resume(struct early_suspend *h)
{
	struct mmc3524x_i2c_data *obj = container_of(h, struct mmc3524x_i2c_data, early_drv);
	MSE_FUN(f);


	if (NULL == obj) {
		MSE_ERR("null pointer!!\n");
		return;
	}

	mmc3524x_power(obj->hw, 1);
	atomic_set(&suspend, 0);
	if (atomic_read(&open_flag) != 0)
		wake_up(&open_wq);

}
/*----------------------------------------------------------------------------*/
#endif /*CONFIG_HAS_EARLYSUSPEND*/

/*----------------------------------------------------------------------------*/
static int mmc3524x_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_client *new_client;
	struct mmc3524x_i2c_data *data = NULL;
	char tmp[2];
	int err = 0;

	struct mag_control_path ctl_path = {0};
	struct mag_data_path dat_path = {0};

	MSE_LOG("%s: ++++\n", __func__);
	data = kmalloc(sizeof(struct mmc3524x_i2c_data), GFP_KERNEL);
	if (data == NULL) {
		err = -ENOMEM;
		goto exit;
	}
	memset(data, 0, sizeof(struct mmc3524x_i2c_data));

	data->hw = get_cust_mag_hw();

	atomic_set(&data->layout, data->hw->direction);
	atomic_set(&data->trace, 0);

	mutex_init(&sensor_data_mutex);
	mutex_init(&read_i2c_xyz);
	mutex_init(&i2c_mutex);

	init_waitqueue_head(&data_ready_wq);
	init_waitqueue_head(&open_wq);

	data->client = client;
	new_client = data->client;
	i2c_set_clientdata(new_client, data);

	this_client = new_client;

	usleep_range(10000, 10500);

	/* send TM cmd to mag sensor first of all */
	tmp[0] = MMC3524X_REG_CTRL;
	tmp[1] = MMC3524X_CTRL_TM;
	if (I2C_TxData(tmp, 2) < 0) {
		MSE_ERR("mmc3524x_device set TM cmd failed\n");
		goto exit_kfree;
	}

	err = mmc3524x_create_attr(&(mmc3524x_init_info.platform_diver_addr->driver));
	if (err != 0) {
		MSE_ERR("mmc3524x_create_attr create attribute err = %d\n", err);
		goto exit_sysfs_create_group_failed;
	}

	err = misc_register(&mmc3524x_device);
	if (err != 0) {
		MSE_ERR("mmc3524x_device register failed\n");
		goto exit_misc_device_register_failed;
	}


	ctl_path.m_open_report_data = mmc3524x_m_open_report_data;
	ctl_path.m_enable			= mmc3524x_m_enable;
	ctl_path.m_set_delay		= mmc3524x_m_set_delay;

	ctl_path.o_open_report_data = mmc3524x_o_open_report_data;
	ctl_path.o_enable			= mmc3524x_o_enable;
	ctl_path.o_set_delay		= mmc3524x_o_set_delay;

	ctl_path.is_report_input_direct = false;
	ctl_path.is_support_batch		= data->hw->is_batch_supported;

	err = mag_register_control_path(&ctl_path);

	if (err < 0) {
		MSE_ERR("mag_register_control_path failed!\n");
		goto exit_misc_device_register_failed;
	}

	dat_path.div_m = CONVERT_M_DIV;
	dat_path.div_o = CONVERT_O_DIV;

	dat_path.get_data_m = mmc3524x_get_data_m;
	dat_path.get_data_o = mmc3524x_get_data_o;

	err = mag_register_data_path(&dat_path);
	if (err < 0) {
		MSE_ERR("mag_register_control_path failed!\n");
		goto exit_misc_device_register_failed;
	}

#if CONFIG_HAS_EARLYSUSPEND
	data->early_drv.level	= EARLY_SUSPEND_LEVEL_DISABLE_FB - 1,
	data->early_drv.suspend	= mmc3524x_early_suspend,
	data->early_drv.resume	= mmc3524x_late_resume,
	register_early_suspend(&data->early_drv);
#endif

	mmc3524x_init_flag = 0;

	MSE_LOG("%s: OK\n", __func__);
	return 0;

exit_misc_device_register_failed:

exit_sysfs_create_group_failed:

exit_kfree:
	kfree(data);
exit:

	mmc3524x_init_flag = -1;

	MSE_ERR("%s: err = %d\n", __func__, err);
/* m_sensor_deteced = false; */
	return err;
}
/*----------------------------------------------------------------------------*/
static int mmc3524x_i2c_remove(struct i2c_client *client)
{
	int err;

	err = mmc3524x_delete_attr(&(mmc3524x_init_info.platform_diver_addr->driver));
	if (err != 0)
		MSE_ERR("mmc3524x_delete_attr fail: %d\n", err);

	this_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));
	misc_deregister(&mmc3524x_device);
	return 0;
}
/*----------------------------------------------------------------------------*/

static int mmc3524x_local_init(void)
{
	struct mag_hw *hw = get_cust_mag_hw();
	int err = 0;

	mmc3524x_power(hw, 1);
	atomic_set(&dev_open_count, 0);

	err = i2c_add_driver(&mmc3524x_i2c_driver);

	if (err < 0) {
		MSE_ERR("add mmc3524x_i2c_driver error\n");
		return -1;
	}


	if (mmc3524x_init_flag == -1)
		return -1;

	return err;
}

/*----------------------------------------------------------------------------*/
static int mmc3524x_remove(void)
{
	struct mag_hw *hw = get_cust_mag_hw();

	mmc3524x_power(hw, 0);
	atomic_set(&dev_open_count, 0);
	i2c_del_driver(&mmc3524x_i2c_driver);
	return 0;
}

/*----------------------------------------------------------------------------*/
static int __init mmc3524x_init(void)
{
	struct mag_hw *hw = get_cust_mag_hw();

	MSE_FUN();

	i2c_register_board_info(hw->i2c_num, &i2c_mmc3524x, 1);

	if (mag_driver_add(&mmc3524x_init_info) < 0)
		MSE_ERR("mag_driver_add failed!\n");

	return 0;
}
/*----------------------------------------------------------------------------*/
static void __exit mmc3524x_exit(void)
{
	MSE_FUN();
}
/*----------------------------------------------------------------------------*/
module_init(mmc3524x_init);
module_exit(mmc3524x_exit);

MODULE_AUTHOR("WD");
MODULE_DESCRIPTION("mmc3524x compass driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);
