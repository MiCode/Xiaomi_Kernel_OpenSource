/*
 * Copyright(C)2014 MediaTek Inc.
 * Copyright (C) 2018 XiaoMi, Inc.
 * Modification based on code covered by the below mentioned copyright
 * and/or permission notice(S).
 */

/* yas_mag_kernel_driver.c - YAS537 compass driver
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

#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/ioctl.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/sysfs.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#if 0
/* FIXME */
#include "stub.h"
#else
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include <linux/proc_fs.h>
#include <linux/hwmsen_helper.h>
#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>
#include <cust_mag.h>
#include "mag.h"
#endif

#include "yas.h"
#include <mach/i2c.h>		//for mtk i2c
#include <linux/dma-mapping.h>

#define USE_BOSCH_DAEMON 
//#define ST_USE_BOSCH_DAEMON

#ifdef USE_BOSCH_DAEMON
   // #ifdef ST_USE_BOSCH_DAEMON
    extern int lsm6ds3_m_open_report_data(int open);
    extern int lsm6ds3_m_enable(int en);
    extern int lsm6ds3_m_set_delay(u64 ns);
    extern int lsm6ds3_m_get_data(int* x ,int* y,int* z, int* status);
	extern int lsm6ds3_o_enable(int en);
	extern int lsm6ds3_o_set_delay(u64 ns);
	extern int lsm6ds3_o_open_report_data(int open);
	extern int lsm6ds3_o_get_data(int* x ,int* y,int* z, int* status);
  //  #else

	extern int bmi160_m_open_report_data(int open);
	extern int bmi160_m_enable(int en);
	extern int bmi160_m_set_delay(u64 ns);
	extern int bmi160_m_get_data(int* x ,int* y,int* z, int* status);
	extern int bmi160_o_enable(int en);
	extern int bmi160_o_set_delay(u64 ns);
	extern int bmi160_o_open_report_data(int open);
	extern int bmi160_o_get_data(int* x ,int* y,int* z, int* status);
  //  #endif
#endif

#define YAS_MTK_NAME			"yas537"
#define YAS_RAW_NAME			"yas537_raw"
#define YAS_CAL_NAME			"yas537_cal"
#define YAS_EULER_NAME			"yas537_euler"

#define YAS_ADDRESS			(0x2e)

#define CONVERT_M_DIV			(1000)	/* 1/1000 = CONVERT_M */
#define CONVERT_O_DIV			(1000)	/* 1/1000 = CONVERT_O */

#define POWER_NONE_MACRO		(MT65XX_POWER_NONE)
#define YAS537_I2C_USE_DMA			//use dma transfer

#define ABS_STATUS			(ABS_BRAKE)
#define ABS_DUMMY			(ABS_RUDDER)
#define MIN(a, b)			((a) < (b) ? (a) : (b))

#define YAS_MTK_DEBUG			(1)
#if YAS_MTK_DEBUG
#define MAGN_TAG		"[Msensor] "
#define MAGN_ERR(fmt, args...)	\
	printk(KERN_ERR  MAGN_TAG"%s %d : "fmt, __func__, __LINE__, ##args)
#define MAGN_LOG(fmt, args...)	printk(KERN_INFO MAGN_TAG fmt, ##args)
#else
#define MAGN_TAG
#define MAGN_ERR(fmt, args...)	do {} while (0)
#define MAGN_LOG(fmt, args...)	do {} while (0)
#endif

static struct i2c_client *this_client = NULL;
static struct mutex yas537_i2c_mutex;

#ifdef USE_BOSCH_DAEMON
extern atomic_t bosch_chip;
extern atomic_t st_chip;
#endif


struct yas_state {
	struct mag_hw *hw;
	struct mutex lock;
	struct yas_mag_driver mag;
	struct i2c_client *client;
	struct input_dev *euler;
	struct input_dev *cal;
	struct input_dev *raw;
	struct delayed_work work;
	char * dma_va;
	dma_addr_t dma_pa;
	int32_t mag_delay;
	int32_t euler_delay;
	atomic_t mag_enable;
	atomic_t euler_enable;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend sus;
#endif
};

static int yas_device_open(int32_t type)
{
	return 0;
}

static int yas_device_close(int32_t type)
{
	return 0;
}

static int yas_device_write(int32_t type, uint8_t addr, const uint8_t *buf,
		int len)
{

	uint8_t tmp[2];
	int ret = 0;
	struct yas_state *st = i2c_get_clientdata(this_client);
	
	if (sizeof(tmp) - 1 < len)
		return -1;
	tmp[0] = addr;
	memcpy(&tmp[1], buf, len);
	mutex_lock(&yas537_i2c_mutex);

	if((len+1) <= 8)
	{
		ret = i2c_master_send(this_client, tmp, len+1);
	}
	else
	{
#if 0
		if(unlikely(NULL == st->dma_va))
		{

			this_client->ext_flag &= I2C_MASK_FLAG; //CLEAR DMA FLAG
			for(i=0; i<=(len+1); i=i+8)
			{
				trans_len = ((i+8)<=(len+1)) ? 8 : (len+1-i);
				MSE_LOG("%s   trans_len = %d\n", __FUNCTION__,trans_len);
				ret = i2c_master_send(this_client, &tmp[i], trans_len);
				if(ret < 0)
					break;
			}
		}
		else
#endif
		{
			this_client->ext_flag = this_client->ext_flag | I2C_DMA_FLAG;	//ENABLE DMA FLAG
			memset(st->dma_va, 0, 1024);
			st->dma_va[0] = addr;
			memcpy(&(st->dma_va[1]), buf, len);
			ret = i2c_master_send(this_client, (char *)(st->dma_pa), len+1);
			if(ret < 0)
			{
				MAGN_ERR("%s i2c_master_send failed! ret = %d\n",__FUNCTION__, ret);
			}
		}
	}

	this_client->ext_flag &= I2C_MASK_FLAG; //CLEAR DMA FLAG
	mutex_unlock(&yas537_i2c_mutex);
	if(ret < 0)
		return ret;
    //MSE_LOG("%s   successful\n", __FUNCTION__);
	return 0;
}

static int yas_device_read(int32_t type, uint8_t addr, uint8_t *buf, int len)
{

	struct mt_i2c_msg msg[2];
	int err = 0;
	struct yas_state *st= i2c_get_clientdata(this_client);

	//memset(msg, 0, sizeof(msg));
	msg[0].addr = this_client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &addr;
	msg[0].timing = this_client->timing;	//add for mtk i2c
	msg[0].ext_flag = this_client->ext_flag & I2C_MASK_FLAG;//add for mtk i2c
	msg[1].addr = this_client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = len;
	msg[1].buf = buf;
	msg[1].timing = this_client->timing;	//add for mtk i2c
	msg[1].ext_flag = this_client->ext_flag & I2C_MASK_FLAG;//add for mtk i2c

	if((len > 8 ) && (st->dma_va != NULL))
	{
		msg[1].ext_flag = this_client->ext_flag | I2C_DMA_FLAG;//add for mtk i2c
	}
	mutex_lock(&yas537_i2c_mutex);
//#if 0
	if(len <= 8)
	{
		err = i2c_transfer(this_client->adapter, (struct i2c_msg *)msg, 2);
		if (err != 2) {
			dev_err(&this_client->dev,
					"i2c_transfer() read error: "
					"adapter num = %d,slave_addr=%02x, reg_addr=%02x, err=%d\n",
					this_client->adapter->nr, this_client->addr, addr, err);
			mutex_unlock(&yas537_i2c_mutex);
			return err;
		}
	}

//#else
	else
	{
	#if 0
		if(unlikely(NULL == st->dma_va))
		{
			this_client->ext_flag &= I2C_MASK_FLAG; //CLEAR DMA FLAG	
			memset(buf, 0,len);
			buf = &addr;
			err = i2c_master_send(this_client, buf,1);
			if(err < 0)
			{
				MSE_ERR("%s  i2c_master_send failed err = %d\n", __FUNCTION__, err);
				mutex_unlock(&yas537_i2c_mutex);
				return err;
			}
			
			for(i=0; i<=len; i=i+8)
			{
				trans_len = ((i+8)<=len) ? 8 : (len-i);
				MSE_LOG("%s   trans_len = %d\n", __FUNCTION__,trans_len);

				err = i2c_master_recv(this_client, &buf[i], trans_len);

				if(err < 0)
				{
					MSE_ERR("%s  i2c_master_recv failed err = %d\n", __FUNCTION__, err);
					mutex_unlock(&yas537_i2c_mutex);
					return err;
				}
			}
		}
		else
	#endif
		{
			memset(st->dma_va, 0, 1024);
			msg[1].buf = (char *)(st->dma_pa);
			err = i2c_transfer(this_client->adapter, (struct i2c_msg *)msg, 2);
			if (err != 2) {
				dev_err(&this_client->dev,
						"i2c_transfer() read error: "
						"adapter num = %d,slave_addr=%02x, reg_addr=%02x, err=%d\n",
						this_client->adapter->nr, this_client->addr, addr, err);

				mutex_unlock(&yas537_i2c_mutex);
				return err;
			}
			memcpy(buf, st->dma_va, len);
		}
	}
//#endif
//	MSE_LOG("%s   successful\n", __FUNCTION__);

	mutex_unlock(&yas537_i2c_mutex);
	return 0;
}

static void yas_usleep(int us)
{
	usleep_range(us, us + 1000);
}

static uint32_t yas_current_time(void)
{
	return jiffies_to_msecs(jiffies);
}

static void input_get_data(struct input_dev *input, int32_t *x, int32_t *y,
		int32_t *z, int32_t *status)
{
	*x = input_abs_get_val(input, ABS_X);
	*y = input_abs_get_val(input, ABS_Y);
	*z = input_abs_get_val(input, ABS_Z);
	*status = input_abs_get_val(input, ABS_STATUS);
}

static int start_mag(struct yas_state *st)
{
	mutex_lock(&st->lock);
	st->mag.set_enable(1);
	mutex_unlock(&st->lock);
	schedule_delayed_work(&st->work, 0);
	return 0;
}

static int stop_mag(struct yas_state *st)
{
	cancel_delayed_work_sync(&st->work);
	mutex_lock(&st->lock);
	st->mag.set_enable(0);
	mutex_unlock(&st->lock);
	return 0;
}

static int set_delay(struct yas_state *st, int delay)
{
	int rt;
	mutex_lock(&st->lock);
	rt = st->mag.set_delay(delay);
	mutex_unlock(&st->lock);
	MAGN_ERR("XINXIN_set_delay=%d\n",delay);
	return rt;
}

static ssize_t yas_mag_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct yas_state *st = i2c_get_clientdata(this_client);
	return sprintf(buf, "%d\n", atomic_read(&st->mag_enable));
}

static void set_mag_enable(struct yas_state *st, int enable)
{
	if (enable) {
		if (!atomic_cmpxchg(&st->mag_enable, 0, 1)
				&& !atomic_read(&st->euler_enable))
			start_mag(st);
	} else {
		if (atomic_cmpxchg(&st->mag_enable, 1, 0)
				&& !atomic_read(&st->euler_enable))
			stop_mag(st);
	}
}

static ssize_t yas_mag_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct yas_state *st = i2c_get_clientdata(this_client);
	int enable;
	if (kstrtoint(buf, 10, &enable) < 0)
		return -EINVAL;
	set_mag_enable(st, enable);
	return count;
}

static ssize_t yas_mag_position_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct yas_state *st = i2c_get_clientdata(this_client);
	int ret;
	mutex_lock(&st->lock);
	ret = st->mag.get_position();
	mutex_unlock(&st->lock);
	if (ret < 0)
		return -EFAULT;
	return sprintf(buf, "%d\n", ret);
}

static ssize_t yas_mag_position_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct yas_state *st = i2c_get_clientdata(this_client);
	int ret, position;
	sscanf(buf, "%d\n", &position);
	mutex_lock(&st->lock);
	ret = st->mag.set_position(position);
	mutex_unlock(&st->lock);
	if (ret < 0)
		return -EFAULT;
	return count;
}

static ssize_t yas_mag_delay_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct yas_state *st = i2c_get_clientdata(this_client);
	int32_t mag_delay;
	mutex_lock(&st->lock);
	mag_delay = st->mag_delay;
	mutex_unlock(&st->lock);
	return sprintf(buf, "%d\n", mag_delay);
}

static int set_mag_delay(struct yas_state *st, int delay)
{
	if (atomic_read(&st->mag_enable) && atomic_read(&st->euler_enable)) {
		if (set_delay(st, MIN(st->euler_delay, delay)) < 0)
			return -EINVAL;
	} else if (atomic_read(&st->mag_enable)) {
		if (set_delay(st, delay) < 0)
			return -EINVAL;
	}
	st->mag_delay = delay;
	return 0;
}

static ssize_t yas_mag_delay_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct yas_state *st = i2c_get_clientdata(this_client);
	int delay;
	if (kstrtoint(buf, 10, &delay) < 0)
		return -EINVAL;
	if (delay <= 0)
		delay = 0;
	if (set_mag_delay(st, delay) < 0)
		return -EINVAL;
	return count;
}

static ssize_t yas_mag_hard_offset_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct yas_state *st = i2c_get_clientdata(this_client);
	int8_t hard_offset[3];
	int ret;
	mutex_lock(&st->lock);
	ret = st->mag.ext(YAS537_GET_HW_OFFSET, hard_offset);
	mutex_unlock(&st->lock);
	if (ret < 0)
		return -EFAULT;
	return sprintf(buf, "%d %d %d\n", hard_offset[0], hard_offset[1],
			hard_offset[2]);
}

static ssize_t yas_mag_static_matrix_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct yas_state *st = i2c_get_clientdata(this_client);
	int16_t m[9];
	int ret;
	mutex_lock(&st->lock);
	ret = st->mag.ext(YAS537_GET_STATIC_MATRIX, m);
	mutex_unlock(&st->lock);
	if (ret < 0)
		return -EFAULT;
	return sprintf(buf, "%d %d %d %d %d %d %d %d %d\n", m[0], m[1], m[2],
			m[3], m[4], m[5], m[6], m[7], m[8]);
}

static ssize_t yas_mag_static_matrix_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct yas_state *st = i2c_get_clientdata(this_client);
	int16_t m[9];
	int ret;
	sscanf(buf, "%hd %hd %hd %hd %hd %hd %hd %hd %hd\n", &m[0], &m[1],
			&m[2], &m[3], &m[4], &m[5], &m[6], &m[7], &m[8]);
	mutex_lock(&st->lock);
	ret = st->mag.ext(YAS537_SET_STATIC_MATRIX, m);
	mutex_unlock(&st->lock);
	if (ret < 0)
		return -EFAULT;
	return count;
}

static ssize_t yas_mag_self_test_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct yas_state *st = i2c_get_clientdata(this_client);
	struct yas537_self_test_result r;
	int ret;
	mutex_lock(&st->lock);
	ret = st->mag.ext(YAS537_SELF_TEST, &r);
	mutex_unlock(&st->lock);
	return sprintf(buf, "%d %d %d %d %d %d %d %d\n", ret, r.id, r.dir,
			r.sx, r.sy, r.xyz[0], r.xyz[1], r.xyz[2]);
}

static ssize_t yas_mag_self_test_noise_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct yas_state *st = i2c_get_clientdata(this_client);
	int32_t xyz_raw[3];
	int ret;
	mutex_lock(&st->lock);
	ret = st->mag.ext(YAS537_SELF_TEST_NOISE, xyz_raw);
	mutex_unlock(&st->lock);
	if (ret < 0)
		return -EFAULT;
	return sprintf(buf, "%d %d %d\n", xyz_raw[0], xyz_raw[1], xyz_raw[2]);
}

static ssize_t yas_mag_average_sample_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct yas_state *st = i2c_get_clientdata(this_client);
	int8_t mag_average_sample;
	int ret;
	mutex_lock(&st->lock);
	ret = st->mag.ext(YAS537_GET_AVERAGE_SAMPLE, &mag_average_sample);
	mutex_unlock(&st->lock);
	if (ret < 0)
		return -EFAULT;
	return sprintf(buf, "%d\n", mag_average_sample);
}

static ssize_t yas_mag_average_sample_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct yas_state *st = i2c_get_clientdata(this_client);
	int32_t tmp;
	int8_t mag_average_sample;
	int ret;
	sscanf(buf, "%d\n", &tmp);
	mag_average_sample = (int8_t)tmp;
	mutex_lock(&st->lock);
	ret = st->mag.ext(YAS537_SET_AVERAGE_SAMPLE, &mag_average_sample);
	mutex_unlock(&st->lock);
	if (ret < 0)
		return -EFAULT;
	return count;
}

static ssize_t yas_mag_ouflow_thresh_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct yas_state *st = i2c_get_clientdata(this_client);
	int16_t thresh[6];
	int ret;
	mutex_lock(&st->lock);
	ret = st->mag.ext(YAS537_GET_OUFLOW_THRESH, thresh);
	mutex_unlock(&st->lock);
	if (ret < 0)
		return -EFAULT;
	return sprintf(buf, "%d %d %d %d %d %d\n", thresh[0], thresh[1],
			thresh[2], thresh[3], thresh[4], thresh[5]);
}

static ssize_t yas_mag_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct yas_state *st = i2c_get_clientdata(this_client);
	int32_t xyz[3], status;
	mutex_lock(&st->lock);
	input_get_data(st->cal, &xyz[0], &xyz[1], &xyz[2], &status);
	mutex_unlock(&st->lock);
	return sprintf(buf, "%d %d %d\n", xyz[0], xyz[1], xyz[2]);
}

static DEVICE_ATTR(mag_delay, S_IRUGO|S_IWUSR|S_IWGRP,
		yas_mag_delay_show,
		yas_mag_delay_store);
static DEVICE_ATTR(mag_enable, S_IRUGO|S_IWUSR|S_IWGRP, yas_mag_enable_show,
		yas_mag_enable_store);
static DEVICE_ATTR(mag_data, S_IRUGO, yas_mag_data_show, NULL);
static DEVICE_ATTR(mag_position, S_IRUSR|S_IWUSR, yas_mag_position_show,
		yas_mag_position_store);
static DEVICE_ATTR(mag_hard_offset, S_IRUSR, yas_mag_hard_offset_show, NULL);
static DEVICE_ATTR(mag_static_matrix, S_IRUSR|S_IWUSR,
		yas_mag_static_matrix_show, yas_mag_static_matrix_store);
static DEVICE_ATTR(mag_self_test, S_IRUSR|S_IRGRP|S_IROTH , yas_mag_self_test_show, NULL);
static DEVICE_ATTR(mag_self_test_noise, S_IRUSR, yas_mag_self_test_noise_show,
		NULL);
static DEVICE_ATTR(mag_average_sample, S_IRUSR|S_IWUSR,
		yas_mag_average_sample_show, yas_mag_average_sample_store);
static DEVICE_ATTR(mag_ouflow_thresh, S_IRUSR, yas_mag_ouflow_thresh_show, NULL);

static struct attribute *yas_mag_attributes[] = {
	&dev_attr_mag_delay.attr,
	&dev_attr_mag_enable.attr,
	&dev_attr_mag_data.attr,
	&dev_attr_mag_position.attr,
	&dev_attr_mag_hard_offset.attr,
	&dev_attr_mag_static_matrix.attr,
	&dev_attr_mag_self_test.attr,
	&dev_attr_mag_self_test_noise.attr,
	&dev_attr_mag_average_sample.attr,
	&dev_attr_mag_ouflow_thresh.attr,
	NULL
};
static struct attribute_group yas_mag_attribute_group = {
	.attrs = yas_mag_attributes
};

static ssize_t daemon_name_show(struct device_driver *ddri, char *buf)
{
	char strbuf[64];
	sprintf(strbuf, "yamaha537");
	printk("1111111111111 ");
	return sprintf(buf, "%s", strbuf);		
}

static ssize_t yas_input_show_value(struct device_driver *ddri, char *buf)
{
	struct yas_data data;
	struct yas_state *st = i2c_get_clientdata(this_client);
	int ret = st->mag.measure(&data, 1);
	ssize_t count;
	if(ret == YAS_ERROR_BUSY) {
		while(ret == YAS_ERROR_BUSY)
			ret = st->mag.measure(&data, 1);
	}
	//printk(KERN_INFO "ret is %d, xyz is %d %d %d, timestamp is %u\n", ret, data.xyz.v[0], data.xyz.v[1], data.xyz.v[2], data.timestamp);
	count = sprintf(buf, "%d %d %d\n",  data.xyz.v[0], data.xyz.v[1], data.xyz.v[2]);
	return count;
}

static ssize_t yas_input_show_op_mode(struct device_driver *ddri, char *buf)
{
	struct yas_state *st = i2c_get_clientdata(this_client);
	int ret = st->mag.get_enable();
	ssize_t count;
	printk(KERN_INFO "ret for get_enable is %d\n", ret);
	if(ret == 0)
		ret = 2;
	count = sprintf(buf, "%d", ret);
	return count;
}

static ssize_t yas_input_store_op_mode(struct device_driver *ddri,
		const char *buf, size_t count)
{
	struct yas_state *st = i2c_get_clientdata(this_client);
	int enable, ret;
	ret = kstrtoint(buf, 10, &enable);
	switch(enable) {
		case 0:
		case 1:
			ret = st->mag.set_enable(1);
			break;
		default:
			ret = st->mag.set_enable(0);
	}
	printk(KERN_INFO "ret for set_enable is %d\n", ret);
	return count;
}

static DRIVER_ATTR(daemon,      S_IRUGO, daemon_name_show, NULL);
static DRIVER_ATTR(magenable,      S_IRUGO|S_IWUSR, yas_mag_enable_show, yas_mag_enable_store);
static DRIVER_ATTR(magsensordata,      S_IRUGO, yas_mag_data_show, NULL);
static DRIVER_ATTR(rawdata, S_IRUGO, yas_input_show_value, NULL);
static DRIVER_ATTR(cpsopmode, S_IRUGO|S_IWUGO, yas_input_show_op_mode, yas_input_store_op_mode);


static struct driver_attribute *yas537_attr_list[] = 
{
	&driver_attr_daemon,
	&driver_attr_magenable,
	&driver_attr_magsensordata,
	&driver_attr_rawdata,
	&driver_attr_cpsopmode,
};

static int yas537_create_attr(struct device_driver *driver)
{
	int ret = 0;
	int i = 0;
	int num = sizeof(yas537_attr_list)/sizeof(yas537_attr_list[0]);

	if(NULL == driver)
	{
		return -EINVAL;
	}
	for(i = 0; i < num; i++)
	{
		ret = driver_create_file(driver, yas537_attr_list[i]);
		if(ret < 0)
		{
			MAGN_ERR("driver_create_file(%s) error=%d!\n",yas537_attr_list[i]->attr.name, ret);
			break;
		}
	}
	
	return ret;
}

static int yas537_delete_attr(struct device_driver *driver)
{
	int ret = 0;
	int i = 0;
	int num = sizeof(yas537_attr_list)/sizeof(yas537_attr_list[0]);

	if(NULL == driver)
	{
		return -EINVAL;
	}
	for(i = 0; i < num; i++)
	{
		driver_remove_file(driver, yas537_attr_list[i]);
	}
	
	return ret;

}


static ssize_t yas_euler_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct yas_state *st = i2c_get_clientdata(this_client);
	return sprintf(buf, "%d\n", atomic_read(&st->euler_enable));
}

static void set_euler_enable(struct yas_state *st, int enable)
{
	if (enable) {
		if (!atomic_cmpxchg(&st->euler_enable, 0, 1)
				&& !atomic_read(&st->mag_enable))
			start_mag(st);
	} else {
		if (atomic_cmpxchg(&st->euler_enable, 1, 0)
				&& !atomic_read(&st->mag_enable))
			stop_mag(st);
	}
}

static ssize_t yas_euler_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct yas_state *st = i2c_get_clientdata(this_client);
	int enable;
	if (kstrtoint(buf, 10, &enable) < 0)
		return -EINVAL;
	set_euler_enable(st, enable);
	return count;
}

static ssize_t yas_euler_delay_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct yas_state *st = i2c_get_clientdata(this_client);
	int32_t euler_delay;
	mutex_lock(&st->lock);
	euler_delay = st->euler_delay;
	mutex_unlock(&st->lock);
	return sprintf(buf, "%d\n", euler_delay);
}

static int set_euler_delay(struct yas_state *st, int delay)
{
	if (atomic_read(&st->mag_enable) && atomic_read(&st->euler_enable)) {
		if (set_delay(st, MIN(st->mag_delay, delay)) < 0)
			return -EINVAL;
	} else if (atomic_read(&st->euler_enable)) {
		if (set_delay(st, delay) < 0)
			return -EINVAL;
	}
	st->euler_delay = delay;
	return 0;
}

static ssize_t yas_euler_delay_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct yas_state *st = i2c_get_clientdata(this_client);
	int delay;
	if (kstrtoint(buf, 10, &delay) < 0)
		return -EINVAL;
	if (delay <= 0)
		delay = 0;
	set_euler_delay(st, delay);
	return count;
}

static ssize_t yas_euler_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct yas_state *st = i2c_get_clientdata(this_client);
	int32_t xyz[3], status;
	mutex_lock(&st->lock);
	input_get_data(st->euler, &xyz[0], &xyz[1], &xyz[2], &status);
	mutex_unlock(&st->lock);
	return sprintf(buf, "%d %d %d\n", xyz[0], xyz[1], xyz[2]);
}

static DEVICE_ATTR(euler_delay, S_IRUGO|S_IWUSR|S_IWGRP,
		yas_euler_delay_show, yas_euler_delay_store);
static DEVICE_ATTR(euler_enable, S_IRUGO|S_IWUSR|S_IWGRP, yas_euler_enable_show,
		yas_euler_enable_store);
static DEVICE_ATTR(euler_data, S_IRUGO, yas_euler_data_show, NULL);

static struct attribute *yas_euler_attributes[] = {
	&dev_attr_euler_delay.attr,
	&dev_attr_euler_enable.attr,
	&dev_attr_euler_data.attr,
	NULL
};
static struct attribute_group yas_euler_attribute_group = {
	.attrs = yas_euler_attributes
};

static void yas_work_func(struct work_struct *work)
{
	static int32_t dummy_count;
	struct yas_state *st
		= container_of((struct delayed_work *)work,
			struct yas_state, work);
	struct yas_data mag[1];
	int32_t delay;
	uint32_t time_before, time_after;
	int ret;

	time_before = yas_current_time();
	mutex_lock(&st->lock);
	if (atomic_read(&st->mag_enable) && atomic_read(&st->euler_enable))
		delay = MIN(st->euler_delay, st->mag_delay);
	else if (atomic_read(&st->mag_enable))
		delay = st->mag_delay;
	else if (atomic_read(&st->euler_enable))
		delay = st->euler_delay;
	else
		delay = MIN(st->euler_delay, st->mag_delay);
	ret = st->mag.measure(mag, 1);
	mutex_unlock(&st->lock);
	if (ret == 1) {
		/* report magnetic data in [nT] */
		input_report_abs(st->raw, ABS_X, mag[0].xyz.v[0]);
		input_report_abs(st->raw, ABS_Y, mag[0].xyz.v[1]);
		input_report_abs(st->raw, ABS_Z, mag[0].xyz.v[2]);
		input_report_abs(st->raw, ABS_DUMMY, ++dummy_count);
		input_sync(st->raw);
	}
	time_after = yas_current_time();
	delay = delay - (time_after - time_before);
	if (delay <= 0)
		delay = 1;
	schedule_delayed_work(&st->work, msecs_to_jiffies(delay));
}

#ifndef USE_BOSCH_DAEMON
static int yas_m_enable(int en)
{
	struct yas_state *st = i2c_get_clientdata(this_client);
	set_mag_enable(st, en);
	return 0;
}

static int yas_m_set_delay(u64 ns)
{
	struct yas_state *st = i2c_get_clientdata(this_client);
	//MAGN_ERR("XINXIN0_set_delay=%d\n",ns);
	return set_mag_delay(st, (int32_t)ns /1000000);
}

static int yas_m_open_report_data(int open)
{
	return 0;
}

static int yas_m_get_data(int *x, int *y, int *z, int *status)
{
	struct yas_state *st = i2c_get_clientdata(this_client);
	input_get_data(st->cal, x, y, z, status);
	return 0;
}

static int yas_o_enable(int en)
{
	struct yas_state *st = i2c_get_clientdata(this_client);
	set_euler_enable(st, en);
	return 0;
}

static int yas_o_set_delay(u64 ns)
{
	struct yas_state *st = i2c_get_clientdata(this_client);
	return set_euler_delay(st, (int32_t)ns /1000000);
}

static int yas_o_open_report_data(int open)
{
	return 0;
}

static int yas_o_get_data(int *x, int *y, int *z, int *status)
{
	struct yas_state *st = i2c_get_clientdata(this_client);
	input_get_data(st->euler, x, y, z, status);
	return 0;
}
#endif

static int yas_local_init(void);
static int yas_local_uninit(void);

static struct mag_init_info yas_init_info = {
	.name = "yas537",
	.init = yas_local_init,
	.uninit = yas_local_uninit,
};

static void yas_power(struct mag_hw *hw, unsigned int on)
{
	static unsigned int power_on;
	MAGN_LOG("[%s]\n", __func__);
	if (hw->power_id != POWER_NONE_MACRO) {
		MAGN_LOG("power %s\n", on ? "on" : "off");
		if (power_on == on) {
			MAGN_LOG("ignore power control: %d\n", on);
		} else if (on) {
			if (!hwPowerOn(hw->power_id, hw->power_vol, "yas537"))
				MAGN_ERR("power on fails!!\n");
		} else {
			if (!hwPowerDown(hw->power_id, "yas537"))
				MAGN_ERR("power off fail!!\n");
		}
	}
	power_on = on;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void yas_early_suspend(struct early_suspend *h)
{
	struct yas_state *st = container_of(h, struct yas_state,
			sus);
	int err = 0;
	MAGN_LOG("[%s]\n", __func__);
	if (atomic_read(&st->mag_enable) || atomic_read(&st->euler_enable))
		stop_mag(st);
	yas_power(st->hw, 0);
}

static void yas_late_resume(struct early_suspend *h)
{
	struct yas_state *st = container_of(h, struct yas_state, sus);
	int err;
	MAGN_LOG("[%s]\n", __func__);
	yas_power(st->hw, 1);
	if (atomic_read(&st->mag_enable) || atomic_read(&st->euler_enable))
		start_mag(st);
}
#endif

#include <linux/dev_info.h>
static int yas_probe(struct i2c_client *i2c,
		const struct i2c_device_id *id)
{
	struct yas_state *st;
	struct input_dev *raw, *cal, *euler;
	struct mag_control_path ctl = {0};
	struct mag_data_path mag_data = {0};
	int ret;

	MAGN_LOG("[%s]\n", __func__);
	this_client = i2c;

	st = kzalloc(sizeof(struct yas_state), GFP_KERNEL);
	if (!st) {
		ret = -ENOMEM;
		goto error_ret;
	}
	i2c_set_clientdata(i2c, st);

	raw = input_allocate_device();
	if (raw == NULL) {
		ret = -ENOMEM;
		goto error_free_device;
	}
	cal = input_allocate_device();
	if (cal == NULL) {
		ret = -ENOMEM;
		goto error_free_device;
	}
	euler = input_allocate_device();
	if (euler == NULL) {
		ret = -ENOMEM;
		goto error_free_device;
	}

	raw->name = YAS_RAW_NAME;
	raw->dev.parent = &i2c->dev;
	raw->id.bustype = BUS_I2C;
	set_bit(EV_ABS, raw->evbit);
	input_set_abs_params(raw, ABS_X, INT_MIN, INT_MAX, 0, 0);
	input_set_abs_params(raw, ABS_Y, INT_MIN, INT_MAX, 0, 0);
	input_set_abs_params(raw, ABS_Z, INT_MIN, INT_MAX, 0, 0);
	input_set_abs_params(raw, ABS_DUMMY, INT_MIN, INT_MAX, 0, 0);
	input_set_drvdata(raw, st);

	cal->name = YAS_CAL_NAME;
	cal->dev.parent = &i2c->dev;
	cal->id.bustype = BUS_I2C;
	set_bit(EV_ABS, cal->evbit);
	input_set_abs_params(cal, ABS_X, INT_MIN, INT_MAX, 0, 0);
	input_set_abs_params(cal, ABS_Y, INT_MIN, INT_MAX, 0, 0);
	input_set_abs_params(cal, ABS_Z, INT_MIN, INT_MAX, 0, 0);
	input_set_abs_params(cal, ABS_DUMMY, INT_MIN, INT_MAX, 0, 0);
	input_set_abs_params(cal, ABS_STATUS, 0, 3, 0, 0);
	input_set_drvdata(cal, st);

	euler->name = YAS_EULER_NAME;
	euler->dev.parent = &i2c->dev;
	euler->id.bustype = BUS_I2C;
	set_bit(EV_ABS, euler->evbit);
	input_set_abs_params(euler, ABS_X, INT_MIN, INT_MAX, 0, 0);
	input_set_abs_params(euler, ABS_Y, INT_MIN, INT_MAX, 0, 0);
	input_set_abs_params(euler, ABS_Z, INT_MIN, INT_MAX, 0, 0);
	input_set_abs_params(euler, ABS_DUMMY, INT_MIN, INT_MAX, 0, 0);
	input_set_abs_params(euler, ABS_STATUS, 0, 3, 0, 0);
	input_set_drvdata(euler, st);

	ret = input_register_device(raw);
	if (ret)
		goto error_free_device;
	ret = input_register_device(cal);
	if (ret)
		goto error_free_device;
	ret = input_register_device(euler);
	if (ret)
		goto error_free_device;

	ret = sysfs_create_group(&cal->dev.kobj, &yas_mag_attribute_group);
	if (ret)
		goto error_free_device;

	ret = sysfs_create_group(&euler->dev.kobj, &yas_euler_attribute_group);
	if (ret)
		goto error_free_device;

	atomic_set(&st->mag_enable, 0);
	st->hw = get_cust_mag_hw();
	st->raw = raw;
	st->cal = cal;
	st->euler = euler;

	st->mag_delay = YAS_DEFAULT_SENSOR_DELAY;
	st->euler_delay = YAS_DEFAULT_SENSOR_DELAY;
	st->mag.callback.device_open = yas_device_open;
	st->mag.callback.device_close = yas_device_close;
	st->mag.callback.device_write = yas_device_write;
	st->mag.callback.device_read = yas_device_read;
	st->mag.callback.usleep = yas_usleep;
	st->mag.callback.current_time = yas_current_time;
	INIT_DELAYED_WORK(&st->work, yas_work_func);
	mutex_init(&st->lock);
	mutex_init(&yas537_i2c_mutex);
	
	#ifdef YAS537_I2C_USE_DMA
/********try to alloc dma memory 3times************/
	st->dma_va = (char *)dma_alloc_coherent(&(this_client->dev), 1024, &(st->dma_pa), GFP_KERNEL);
	if(unlikely(NULL==st->dma_va))
	{
		st->dma_va = (char *)dma_alloc_coherent(&(this_client->dev), 1024, &(st->dma_pa), GFP_KERNEL);
		if(unlikely(NULL==st->dma_va))
		{
			st->dma_va = (char *)dma_alloc_coherent(&(this_client->dev), 1024, &(st->dma_pa), GFP_KERNEL);
			if(unlikely(NULL==st->dma_va))
			{
				MAGN_ERR("%s  dma_alloc_coherent failed!\n",__FUNCTION__);
			}
		}
	}
#endif
/*
#ifdef CONFIG_HAS_EARLYSUSPEND
	st->sus.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	st->sus.suspend = yas_early_suspend;
	st->sus.resume = yas_late_resume;
	register_early_suspend(&st->sus);
#endif*/

	ret = yas_mag_driver_init(&st->mag);
	if (ret < 0) {
		ret = -EFAULT;
		goto error_remove_sysfs;
	}
	ret = st->mag.init();
	if (ret < 0) {
		ret = -EFAULT;
		goto error_remove_sysfs;
	}

#ifdef CONFIG_CM865_MAINBOARD
	ret = st->mag.set_position(3);
#else
	ret = st->mag.set_position(0);
#endif
	if (ret < 0) {
		ret = -EFAULT;
		goto error_remove_sysfs;
	}

    ret = yas537_create_attr(&(yas_init_info.platform_diver_addr->driver));
	if(ret < 0)
	{
		MAGN_ERR("yas537_create_attr error! \n");
	}
#ifdef USE_BOSCH_DAEMON
   
    //#ifdef ST_USE_BOSCH_DAEMON
   if( (atomic_read(&bosch_chip) == 0) ||(atomic_read(&st_chip) == 1))
   	{
    	ctl.is_use_common_factory = false;
	ctl.m_enable = lsm6ds3_m_enable;
	ctl.m_set_delay  = lsm6ds3_m_set_delay;
	ctl.m_open_report_data = lsm6ds3_m_open_report_data;
	ctl.o_enable = lsm6ds3_o_enable;
	ctl.o_set_delay  = lsm6ds3_o_set_delay;
	ctl.o_open_report_data = lsm6ds3_o_open_report_data;
	ctl.is_report_input_direct = false;
	ctl.is_support_batch = st->hw->is_batch_supported;
	
	ret = mag_register_control_path(&ctl);
	if(ret)
	{
		MAG_ERR("register mag control path err\n");
		goto error_remove_sysfs;
	}

	mag_data.div_m = 4;
	mag_data.div_o = 71;
	mag_data.get_data_o = lsm6ds3_o_get_data;
	mag_data.get_data_m = lsm6ds3_m_get_data;

	ret = mag_register_data_path(&mag_data);
	if(ret)
	{
		MAG_ERR("register data control path err\n");
		goto error_remove_sysfs;
	}
  }
   else{
  //  #else
	ctl.is_use_common_factory = false;
	ctl.m_enable = bmi160_m_enable;
	ctl.m_set_delay  = bmi160_m_set_delay;
	ctl.m_open_report_data = bmi160_m_open_report_data;
	ctl.o_enable = bmi160_o_enable;
	ctl.o_set_delay  = bmi160_o_set_delay;
	ctl.o_open_report_data = bmi160_o_open_report_data;
	ctl.is_report_input_direct = false;
	ctl.is_support_batch = st->hw->is_batch_supported;
	
	ret = mag_register_control_path(&ctl);
	if(ret)
	{
		MAG_ERR("register mag control path err\n");
		goto error_remove_sysfs;
	}

	mag_data.div_m = 4;
	mag_data.div_o = 71;
	mag_data.get_data_o = bmi160_o_get_data;
	mag_data.get_data_m = bmi160_m_get_data;

	ret = mag_register_data_path(&mag_data);
	if(ret)
	{
		MAG_ERR("register data control path err\n");
		goto error_remove_sysfs;
	}
}
 //  #endif
#else
	ctl.is_use_common_factory = false;
	ctl.m_enable = yas_m_enable;
	ctl.m_set_delay  = yas_m_set_delay;
	ctl.m_open_report_data = yas_m_open_report_data;
	ctl.o_enable = yas_o_enable;
	ctl.o_set_delay  = yas_o_set_delay;
	ctl.o_open_report_data = yas_o_open_report_data;
	ctl.is_report_input_direct = false;
	ctl.is_support_batch = st->hw->is_batch_supported;

	ret = mag_register_control_path(&ctl);
	if (ret) {
		MAGN_ERR("register mag control path ret\n");
		goto error_remove_sysfs;
	}

	mag_data.div_m = CONVERT_M_DIV;
	mag_data.div_o = CONVERT_O_DIV;
	mag_data.get_data_o = yas_o_get_data;
	mag_data.get_data_m = yas_m_get_data;

	ret = mag_register_data_path(&mag_data);
	if (ret) {
		MAGN_ERR("register st control path ret\n");
		goto error_remove_sysfs;
	}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	st->sus.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
		st->sus.suspend  = yas_early_suspend,
		st->sus.resume   = yas_late_resume,
		register_early_suspend(&st->sus);
#endif
	MAGN_ERR("%s: OK\n", __func__);
		struct devinfo_struct *dev = (struct devinfo_struct*)kmalloc(sizeof(struct devinfo_struct), GFP_KERNEL);;
		dev->device_type = "MAG";
		dev->device_vendor = "YAMAHA"; 
		dev->device_ic = "yas537";
		dev->device_version = DEVINFO_NULL;
		dev->device_module = DEVINFO_NULL; 
		dev->device_info = DEVINFO_NULL;
		dev->device_used = DEVINFO_USED;	
		  DEVINFO_CHECK_ADD_DEVICE(dev);


	return 0;

error_remove_sysfs:
	sysfs_remove_group(&st->cal->dev.kobj, &yas_mag_attribute_group);
	sysfs_remove_group(&st->euler->dev.kobj, &yas_euler_attribute_group);
error_free_device:
	if (raw != NULL) {
		input_unregister_device(raw);
		input_free_device(raw);
	}
	if (cal != NULL) {
		input_unregister_device(cal);
		input_free_device(cal);
	}
	if (euler != NULL) {
		input_unregister_device(euler);
		input_free_device(euler);
	}
	kfree(st);
error_ret:
	i2c_set_clientdata(i2c, NULL);
	this_client = NULL;
	return ret;
}

static int yas_remove(struct i2c_client *i2c)
{
	struct yas_state *st = i2c_get_clientdata(i2c);
	MAGN_LOG("[%s]\n", __func__);
	if (st != NULL) {
#ifdef CONFIG_HAS_EARLYSUSPEND
		unregister_early_suspend(&st->sus);
#endif
		stop_mag(st);
		st->mag.term();
		sysfs_remove_group(&st->cal->dev.kobj, &yas_mag_attribute_group);
		sysfs_remove_group(&st->euler->dev.kobj, &yas_euler_attribute_group);
		input_unregister_device(st->raw);
		input_free_device(st->raw);
		input_unregister_device(st->cal);
		input_free_device(st->cal);
		input_unregister_device(st->euler);
		input_free_device(st->euler);
		kfree(st);
		this_client = NULL;
	}
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int yas_suspend(struct device *dev)
{
	struct yas_state *st = i2c_get_clientdata(this_client);
	MAGN_LOG("[%s]\n", __func__);
	if (atomic_read(&st->mag_enable) || atomic_read(&st->euler_enable))
		stop_mag(st);
	yas_power(st->hw, 0);
	return 0;
}

static int yas_resume(struct device *dev)
{
	struct yas_state *st = i2c_get_clientdata(this_client);
	MAGN_LOG("[%s]\n", __func__);
	yas_power(st->hw, 1);
	if (atomic_read(&st->mag_enable) || atomic_read(&st->euler_enable))
		start_mag(st);
	return 0;
}

static SIMPLE_DEV_PM_OPS(yas_pm_ops, yas_suspend, yas_resume);
#define YAS_PM_OPS (&yas_pm_ops)
#else
#define YAS_PM_OPS NULL
#endif

static const struct i2c_device_id yas_id[] = {
	{YAS_MTK_NAME, 0},
	{ }
};
MODULE_DEVICE_TABLE(i2c, yas_id);

static struct i2c_board_info __initdata i2c_yas53x = {
	I2C_BOARD_INFO(YAS_MTK_NAME, YAS_ADDRESS)
};

static struct i2c_driver yas_driver = {
	.driver = {
		.name	= YAS_MTK_NAME,
		.owner	= THIS_MODULE,
		.pm	= YAS_PM_OPS,
	},
	.probe		= yas_probe,
	.remove		= yas_remove,
	.id_table	= yas_id,
};

static int yas_local_init(void)
{
	struct mag_hw *hw = get_cust_mag_hw();
	MAGN_LOG("[%s]\n", __func__);
	yas_power(hw, 1);
	if (i2c_add_driver(&yas_driver)) {
		MAGN_ERR("i2c_add_driver error\n");
		return -1;
	}
	return 0;
}

static int yas_local_uninit(void)
{
	struct mag_hw *hw = get_cust_mag_hw();
	MAGN_LOG("[%s]\n", __func__);
	yas_power(hw, 0);
	i2c_del_driver(&yas_driver);
	return 0;
}

static int __init yas_init(void)
{
	struct mag_hw *hw = get_cust_mag_hw();
	MAGN_LOG("[%s]: i2c_number=%d\n", __func__, hw->i2c_num);
	i2c_register_board_info(hw->i2c_num, &i2c_yas53x, 1);
	mag_driver_add(&yas_init_info);
	return 0;
}

static void __exit yas_exit(void)
{
	MAGN_LOG("[%s]\n", __func__);
}
module_init(yas_init);
module_exit(yas_exit);

MODULE_DESCRIPTION("YAS537 compass driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("0.9.0.1025");
