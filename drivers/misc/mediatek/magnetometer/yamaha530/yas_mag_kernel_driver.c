/*
 * Copyright (c) 2010 Yamaha Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#include <asm/atomic.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>

#define __LINUX_KERNEL_DRIVER__
#include "yas.h"
//#include "yas_mag_driver.c"

#ifdef MEDIATEK_CODE
#include <linux/miscdevice.h>
#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include <cust_mag.h>
#include <linux/hwmsen_helper.h>
#endif

#define DEBUG_RW 0
/*----------------------------------------------------------------------------*/
#define YAMAHA530_DEV_NAME         "yamaha530"
#define DRIVER_VERSION          "1.0.1"
/*----------------------------------------------------------------------------*/
#define YAMAHA530_AXIS_X            0
#define YAMAHA530_AXIS_Y            1
#define YAMAHA530_AXIS_Z            2
#define YAMAHA530_AXES_NUM          3

#define DRIVER_DEBUG	1

/*----------------------------------------------------------------------------*/
#define MSE_TAG                  "MSENSOR"

#if DRIVER_DEBUG
#define MSE_FUN(f)               printk(KERN_INFO MSE_TAG" %s\r\n", __FUNCTION__)
#define MSE_ERR(fmt, args...)    printk(KERN_ERR MSE_TAG" %s %d : \r\n"fmt, __FUNCTION__, __LINE__, ##args)
#define MSE_LOG(fmt, args...)    printk(KERN_INFO MSE_TAG fmt, ##args)
#define MSE_VER(fmt, args...)   ((void)0)
#else
#define MSE_FUN(f)             
#define MSE_ERR(fmt, args...)
#define MSE_LOG(fmt, args...) 
#define MSE_VER(fmt, args...) 

#endif

/*----------------------------------------------------------------------------*/
static const struct i2c_device_id yamaha530_i2c_id[] = {{YAMAHA530_DEV_NAME,0},{}};
static struct i2c_board_info __initdata i2c_YAMAHA530={ I2C_BOARD_INFO("yamaha530", (0x5c>>1))};
/*the adapter id will be available in customization*/
//static unsigned short yamaha530_force[] = {0x00, YAS_MAG_I2C_SLAVEADDR, I2C_CLIENT_END, I2C_CLIENT_END};
//static const unsigned short *const yamaha530_forces[] = { yamaha530_force, NULL };
//static struct i2c_client_address_data yamaha530_addr_data = { .forces = yamaha530_forces,};
/*----------------------------------------------------------------------------*/
static int yamaha530_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id); 
static int yamaha530_i2c_remove(struct i2c_client *client);
static int yamaha530_i2c_detect(struct i2c_client *client, struct i2c_board_info *info);

static struct platform_driver yamaha_sensor_driver;


#define GEOMAGNETIC_I2C_DEVICE_NAME         "geomagnetic"
#define GEOMAGNETIC_INPUT_NAME              "geomagnetic"
#define GEOMAGNETIC_INPUT_RAW_NAME          "geomagnetic_raw"
#undef GEOMAGNETIC_PLATFORM_API

#define ABS_STATUS                          (ABS_BRAKE)
#define ABS_WAKE                            (ABS_MISC)

#define ABS_RAW_DISTORTION                  (ABS_THROTTLE)
#define ABS_RAW_THRESHOLD                   (ABS_RUDDER)
#define ABS_RAW_SHAPE                       (ABS_WHEEL)
#define ABS_RAW_REPORT                      (ABS_GAS)

struct geomagnetic_data {
    struct input_dev *input_data;
    struct input_dev *input_raw;
    struct delayed_work work;
    struct semaphore driver_lock;//need to repaired
    struct semaphore multi_lock;
	struct mag_hw *hw;
    atomic_t last_data[3];
    atomic_t last_status;
    atomic_t enable;
    int filter_enable;
    int filter_len;
    int32_t filter_noise[3];
    int32_t filter_threshold;
    int delay;
    int32_t threshold;
    int32_t distortion[3];
    int32_t shape;
    struct yas_mag_offset driver_offset;
	struct i2c_client *client;
	atomic_t layout;   
	atomic_t trace;
#if DEBUG
    int suspend;
#endif
};


static struct i2c_client *this_client = NULL;

static int
geomagnetic_i2c_open(void)
{
    return 0;
}

static int
geomagnetic_i2c_close(void)
{
    return 0;
}

#if YAS_MAG_DRIVER == YAS_MAG_DRIVER_YAS529
static int
geomagnetic_i2c_write(uint8_t slave, const uint8_t *buf, int len)
{
    if (i2c_master_send(this_client, buf, len) < 0) {
        return -1;
    }
#if DEBUG_RW
    YLOGD(("[W] [%02x]\n", buf[0]));
#endif

    return 0;
}

static int
geomagnetic_i2c_read(uint8_t slave, uint8_t *buf, int len)
{
    if (i2c_master_recv(this_client, buf, len) < 0) {
        return -1;
    }

#if DEBUG_RW
    if (len == 1) {
        YLOGD(("[R] [%02x]\n", buf[0]));
    }
    else if (len == 6) {
        YLOGD(("[R] "
        "[%02x%02x%02x%02x%02x%02x]\n",
        buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]));
    }
    else if (len == 8) {
        YLOGD(("[R] "
        "[%02x%02x%02x%02x%02x%02x%02x%02x]\n",
        buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]));
    }
    else if (len == 9) {
        YLOGD(("[R] "
        "[%02x%02x%02x%02x%02x%02x%02x%02x%02x]\n",
        buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8]));
    }
    else if (len == 16) {
        YLOGD(("[R] "
        "[%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x]\n",
        buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7],
        buf[8], buf[9], buf[10], buf[11], buf[12], buf[13], buf[14], buf[15]));
    }
#endif

    return 0;
}

#else

static int
geomagnetic_i2c_write(uint8_t slave, uint8_t addr, const uint8_t *buf, int len)
{
    uint8_t tmp[16];

    if (sizeof(tmp) -1 < len) {
        return -1;
    }

    tmp[0] = addr;
    memcpy(&tmp[1], buf, len);

    if (i2c_master_send(this_client, tmp, len + 1) < 0) {
        return -1;
    }
#if DEBUG
    YLOGD(("530 [W] addr[%02x] [%02x]\n", addr, buf[0]));
#endif

    return 0;
}

static int
geomagnetic_i2c_read(uint8_t slave, uint8_t addr, uint8_t *buf, int len)
{
    struct i2c_msg msg[2];
    int err;

    msg[0].addr = slave;
    msg[0].flags = 0;
    msg[0].len = 1;
    msg[0].buf = &addr;
	msg[0].timing = 200;
    msg[1].addr = slave;
    msg[1].flags = I2C_M_RD;
    msg[1].len = len;
    msg[1].buf = buf;
	msg[0].timing = 200;

/*
    err = i2c_transfer(this_client->adapter, msg, 2);
    if (err != 2) {
        dev_err(&this_client->dev,
                "i2c_transfer() read error: slave_addr=%02x, reg_addr=%02x, err=%d\n", slave, addr, err);
        return err;
    }
*/
	this_client->addr = this_client->addr & I2C_MASK_FLAG;
		//MSE_ERR("Sensor non-dma read timing is %x!\r\n", this_client->timing);
//	buf[0]= addr;	
//	err = i2c_master_recv(this_client, buf, len);
	err = hwmsen_read_block(this_client, addr, buf, len);


#if DEBUG
	YLOGD(("530 read "));

    if (len == 1) {
        YLOGD(("[R] addr[%02x] [%02x]\n", addr, buf[0]));
    }
    else if (len == 6) {
        YLOGD(("[R] addr[%02x] "
        "[%02x%02x%02x%02x%02x%02x]\n",
        addr, buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]));
    }
    else if (len == 8) {
        YLOGD(("[R] addr[%02x] "
        "[%02x%02x%02x%02x%02x%02x%02x%02x]\n",
        addr, buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]));
    }
    else if (len == 9) {
        YLOGD(("[R] addr[%02x] "
        "[%02x%02x%02x%02x%02x%02x%02x%02x%02x]\n",
        addr, buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8]));
    }
    else if (len == 16) {
        YLOGD(("[R] addr[%02x] "
        "[%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x]\n",
        addr,
        buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7],
        buf[8], buf[9], buf[10], buf[11], buf[12], buf[13], buf[14], buf[15]));
    }
#endif

    return 0;
}

#endif

static int
geomagnetic_lock(void)
{
    struct geomagnetic_data *data = NULL;
  
//lock_count++;
//printk("geomagnetic_lock yucong debug: lock_count = %d\n",lock_count);
    if (this_client == NULL) {
        return -1;
    }

    data = i2c_get_clientdata(this_client);
//spin_lock_irqsave(&data->driver_lock,1);
//mutex_lock(&data->driver_lock);
    #if 0
    int rt;
    rt = down_interruptible(&data->driver_lock);
    if (rt < 0) {
        up(&data->driver_lock);
    }
    return rt;
	#endif
	return 0;
}

static int
geomagnetic_unlock(void)
{
    struct geomagnetic_data *data = NULL;
	//lock_count--;
	//printk("geomagnetic_unlock yucong debug: lock_count = %d\n",lock_count);
    if (this_client == NULL) {
        return -1;
    }

    data = i2c_get_clientdata(this_client);
	#if 0
    	up(&data->driver_lock);
	#endif
	//spin_unlock_irqrestore(&data->driver_lock,1);
	//mutex_unlock(&data->driver_lock);
	return 0;
}

static void
geomagnetic_msleep(int ms)
{
    msleep(ms);
}

static void
geomagnetic_current_time(int32_t *sec, int32_t *msec)
{
    struct timeval tv;

    do_gettimeofday(&tv);

    *sec = tv.tv_sec;
    *msec = tv.tv_usec / 1000;
}

static struct yas_mag_driver hwdep_driver = {
    .callback = {
        .lock           = geomagnetic_lock,
        .unlock         = geomagnetic_unlock,
        .i2c_open       = geomagnetic_i2c_open,
        .i2c_close      = geomagnetic_i2c_close,
        .i2c_read       = geomagnetic_i2c_read,
        .i2c_write      = geomagnetic_i2c_write,
        .msleep         = geomagnetic_msleep,
        .current_time   = geomagnetic_current_time,
    },
};

static int
geomagnetic_multi_lock(void)
{
    struct geomagnetic_data *data = NULL;
    //multi_lock_count++;
    //printk("geomagnetic_multi_lock yucong debug: multi_lock_count = %d\n",multi_lock_count);
	
    if (this_client == NULL) {
        return -1;
    }

    data = i2c_get_clientdata(this_client);
    #if 0
    int rt;
    rt = down_interruptible(&data->multi_lock);
    if (rt < 0) {
        up(&data->multi_lock);
    }
	#endif
	//spin_lock_irqsave(&data->multi_lock,1);
	//mutex_lock(&data->multi_lock);
    return 0;
}

static int
geomagnetic_multi_unlock(void)
{
    struct geomagnetic_data *data = NULL;
	
	//multi_lock_count--;
	//printk("geomagnetic_multi_unlock yucong debug: multi_lock_count = %d\n",multi_lock_count);
	
    if (this_client == NULL) {
        return -1;
    }

    data = i2c_get_clientdata(this_client);
	#if 0
    up(&data->multi_lock);
	#endif
	//spin_unlock_irqrestore(&data->multi_lock,1);
	//mutex_unlock(&data->multi_lock);
	return 0;
}

static int
geomagnetic_enable(struct geomagnetic_data *data)
{
    if (!atomic_cmpxchg(&data->enable, 0, 1)) {
        schedule_delayed_work(&data->work, 0);
    }

    return 0;
}

static int
geomagnetic_disable(struct geomagnetic_data *data)
{
    if (atomic_cmpxchg(&data->enable, 1, 0)) {
        cancel_delayed_work_sync(&data->work);
    }

    return 0;
}

/* Sysfs interface */
static ssize_t
geomagnetic_delay_show(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
    struct input_dev *input_data = to_input_dev(dev);
    struct geomagnetic_data *data = input_get_drvdata(input_data);
    int delay;

    geomagnetic_multi_lock();

    delay = data->delay;

    geomagnetic_multi_unlock();

    return sprintf(buf, "%d\n", delay);
}

static ssize_t
geomagnetic_delay_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf,
        size_t count)
{
    struct input_dev *input_data = to_input_dev(dev);
    struct geomagnetic_data *data = input_get_drvdata(input_data);
    int value = simple_strtol(buf, NULL, 10);

    if (hwdep_driver.set_delay == NULL) {
        return -ENOTTY;
    }

    geomagnetic_multi_lock();

    value = simple_strtol(buf, NULL, 10);
    if (hwdep_driver.set_delay(value) == 0) {
        data->delay = value;
    }

    geomagnetic_multi_unlock();

    return count;
}

static ssize_t
geomagnetic_enable_show(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
    struct input_dev *input_data = to_input_dev(dev);
    struct geomagnetic_data *data = input_get_drvdata(input_data);

    return sprintf(buf, "%d\n", atomic_read(&data->enable));
}

static ssize_t
geomagnetic_enable_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf,
        size_t count)
{
    struct input_dev *input_data = to_input_dev(dev);
    struct geomagnetic_data *data = input_get_drvdata(input_data);
    int value;
MSE_FUN(f);
    value = !!simple_strtol(buf, NULL, 10);
    if (hwdep_driver.set_enable == NULL) {
        return -ENOTTY;
    }

    if (geomagnetic_multi_lock() < 0) {
        return count;
    }

    if (hwdep_driver.set_enable(value) == 0) {
        if (value) {
            geomagnetic_enable(data);
        }
        else {
            geomagnetic_disable(data);
        }
    }

    geomagnetic_multi_unlock();

    return count;
}

static ssize_t
geomagnetic_filter_enable_show(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
    struct input_dev *input_data = to_input_dev(dev);
    struct geomagnetic_data *data = input_get_drvdata(input_data);
    int filter_enable;

    geomagnetic_multi_lock();

    filter_enable = data->filter_enable;

    geomagnetic_multi_unlock();

    return sprintf(buf, "%d\n", filter_enable);
}

static ssize_t
geomagnetic_filter_enable_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf,
        size_t count)
{
    struct input_dev *input_data = to_input_dev(dev);
    struct geomagnetic_data *data = input_get_drvdata(input_data);
    int value;

    if (hwdep_driver.set_filter_enable == NULL) {
        return -ENOTTY;
    }

    value = simple_strtol(buf, NULL, 10);
    if (geomagnetic_multi_lock() < 0) {
        return count;
    }

    if (hwdep_driver.set_filter_enable(value) == 0) {
        data->filter_enable = !!value;
    }

    geomagnetic_multi_unlock();

    return count;
}

static ssize_t
geomagnetic_filter_len_show(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
    struct input_dev *input_data = to_input_dev(dev);
    struct geomagnetic_data *data = input_get_drvdata(input_data);
    int filter_len;

    geomagnetic_multi_lock();

    filter_len = data->filter_len;

    geomagnetic_multi_unlock();

    return sprintf(buf, "%d\n", filter_len);
}

static ssize_t
geomagnetic_filter_len_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf,
        size_t count)
{
    struct input_dev *input_data = to_input_dev(dev);
    struct geomagnetic_data *data = input_get_drvdata(input_data);
    struct yas_mag_filter filter;
    int value;


    if (hwdep_driver.get_filter == NULL || hwdep_driver.set_filter == NULL) {
        return -ENOTTY;
    }

    value = simple_strtol(buf, NULL, 10);

    if (geomagnetic_multi_lock() < 0) {
        return count;
    }

    hwdep_driver.get_filter(&filter);
    filter.len = value;
    if (hwdep_driver.set_filter(&filter) == 0) {
        data->filter_len = value;
    }

    geomagnetic_multi_unlock();

    return count;
}

static ssize_t
geomagnetic_filter_noise_show(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
    struct input_dev *input_raw = to_input_dev(dev);
    struct geomagnetic_data *data = input_get_drvdata(input_raw);
    int rt;

    geomagnetic_multi_lock();

    rt = sprintf(buf, "%d %d %d\n",
            data->filter_noise[0],
            data->filter_noise[1],
            data->filter_noise[2]);

    geomagnetic_multi_unlock();

    return rt;
}

static ssize_t
geomagnetic_filter_noise_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf,
        size_t count)
{
    struct input_dev *input_raw = to_input_dev(dev);
    struct yas_mag_filter filter;
    struct geomagnetic_data *data = input_get_drvdata(input_raw);
    int32_t filter_noise[3];

    geomagnetic_multi_lock();

    sscanf(buf, "%d %d %d",
            &filter_noise[0],
            &filter_noise[1],
            &filter_noise[2]);
    hwdep_driver.get_filter(&filter);
    memcpy(filter.noise, filter_noise, sizeof(filter.noise));
    if (hwdep_driver.set_filter(&filter) == 0) {
        memcpy(data->filter_noise, filter_noise, sizeof(data->filter_noise));
    }

    geomagnetic_multi_unlock();

    return count;
}

static ssize_t
geomagnetic_filter_threshold_show(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
    struct input_dev *input_data = to_input_dev(dev);
    struct geomagnetic_data *data = input_get_drvdata(input_data);
    int32_t filter_threshold;

    geomagnetic_multi_lock();

    filter_threshold = data->filter_threshold;

    geomagnetic_multi_unlock();

    return sprintf(buf, "%d\n", filter_threshold);
}

static ssize_t
geomagnetic_filter_threshold_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf,
        size_t count)
{
    struct input_dev *input_data = to_input_dev(dev);
    struct geomagnetic_data *data = input_get_drvdata(input_data);
    struct yas_mag_filter filter;
    int value;


    if (hwdep_driver.get_filter == NULL || hwdep_driver.set_filter == NULL) {
        return -ENOTTY;
    }

    value = simple_strtol(buf, NULL, 10);

    if (geomagnetic_multi_lock() < 0) {
        return count;
    }

    hwdep_driver.get_filter(&filter);
    filter.threshold = value;
    if (hwdep_driver.set_filter(&filter) == 0) {
        data->filter_threshold = value;
    }

    geomagnetic_multi_unlock();

    return count;
}

static ssize_t
geomagnetic_position_show(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
    if (hwdep_driver.get_position == NULL) {
        return -ENOTTY;
    }
    return sprintf(buf, "%d\n", hwdep_driver.get_position());
}

static ssize_t
geomagnetic_position_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf,
        size_t count)
{
    int value = simple_strtol(buf, NULL, 10);

    if (hwdep_driver.set_position == NULL) {
        return -ENOTTY;
    }
    hwdep_driver.set_position(value);

    return count;
}

static ssize_t
geomagnetic_data_show(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
    struct input_dev *input_data = to_input_dev(dev);
    struct geomagnetic_data *data = input_get_drvdata(input_data);
    int rt;

    rt = sprintf(buf, "%d %d %d\n",
            atomic_read(&data->last_data[0]),
            atomic_read(&data->last_data[1]),
            atomic_read(&data->last_data[2]));

    return rt;
}

static ssize_t
geomagnetic_status_show(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
    struct input_dev *input_data = to_input_dev(dev);
    struct geomagnetic_data *data = input_get_drvdata(input_data);
    int rt;

    rt = sprintf(buf, "%d\n", atomic_read(&data->last_status));

    return rt;
}

static ssize_t
geomagnetic_wake_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf,
        size_t count)
{
    struct input_dev *input_data = to_input_dev(dev);
    struct geomagnetic_data *data = input_get_drvdata(input_data);
    static int16_t cnt = 1;

    input_report_abs(data->input_data, ABS_WAKE, cnt++);
	input_sync(data->input_data);

    return count;
}

static ssize_t show_daemon_name(struct device_driver *ddri, char *buf)
{
	char strbuf[24];
	sprintf(strbuf, "orientationd");
	return sprintf(buf, "%s", strbuf);		
}

static ssize_t show_daemon2_name(struct device_driver *ddri, char *buf)
{
	char strbuf[24];
	sprintf(strbuf, "geomagneticd");
	return sprintf(buf, "%s", strbuf);		
}


#if DEBUG

static int geomagnetic_suspend(struct i2c_client *client, pm_message_t mesg);
static int geomagnetic_resume(struct i2c_client *client);

static ssize_t
geomagnetic_debug_suspend_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct input_dev *input = to_input_dev(dev);
    struct geomagnetic_data *data = input_get_drvdata(input);

    return sprintf(buf, "%d\n", data->suspend);
}

static ssize_t
geomagnetic_debug_suspend_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf, size_t count)
{
    unsigned long suspend = simple_strtol(buf, NULL, 10);

    if (suspend) {
        pm_message_t msg;
        memset(&msg, 0, sizeof(msg));
        geomagnetic_suspend(this_client, msg);
    } else {
        geomagnetic_resume(this_client);
    }

    return count;
}

#endif /* DEBUG */

static DEVICE_ATTR(delay, S_IRUGO|S_IWUSR|S_IWGRP,
        geomagnetic_delay_show, geomagnetic_delay_store);
static DEVICE_ATTR(enable, S_IRUGO|S_IWUSR|S_IWGRP,
        geomagnetic_enable_show, geomagnetic_enable_store);
static DEVICE_ATTR(filter_enable, S_IRUGO|S_IWUSR|S_IWGRP,
        geomagnetic_filter_enable_show, geomagnetic_filter_enable_store);
static DEVICE_ATTR(filter_len, S_IRUGO|S_IWUSR|S_IWGRP,
        geomagnetic_filter_len_show, geomagnetic_filter_len_store);
static DEVICE_ATTR(filter_threshold, S_IRUGO|S_IWUSR|S_IWGRP,
        geomagnetic_filter_threshold_show, geomagnetic_filter_threshold_store);
static DEVICE_ATTR(filter_noise, S_IRUGO|S_IWUSR|S_IWGRP,
        geomagnetic_filter_noise_show, geomagnetic_filter_noise_store);
static DEVICE_ATTR(data, S_IRUGO, geomagnetic_data_show, NULL);
static DEVICE_ATTR(status, S_IRUGO, geomagnetic_status_show, NULL);
static DEVICE_ATTR(wake, S_IWUSR|S_IWGRP, NULL, geomagnetic_wake_store);
static DEVICE_ATTR(position, S_IRUGO|S_IWUSR,
        geomagnetic_position_show, geomagnetic_position_store);
#if DEBUG
static DEVICE_ATTR(debug_suspend, S_IRUGO|S_IWUSR,
        geomagnetic_debug_suspend_show, geomagnetic_debug_suspend_store);
#endif /* DEBUG */

static struct attribute *geomagnetic_attributes[] = {
    &dev_attr_delay.attr,
    &dev_attr_enable.attr,
    &dev_attr_filter_enable.attr,
    &dev_attr_filter_len.attr,
    &dev_attr_filter_threshold.attr,
    &dev_attr_filter_noise.attr,
    &dev_attr_data.attr,
    &dev_attr_status.attr,
    &dev_attr_wake.attr,
    &dev_attr_position.attr,
#if DEBUG
    &dev_attr_debug_suspend.attr,
#endif /* DEBUG */
    NULL
};

static struct attribute_group geomagnetic_attribute_group = {
    .attrs = geomagnetic_attributes
};

static ssize_t
geomagnetic_raw_threshold_show(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
    struct input_dev *input_raw = to_input_dev(dev);
    struct geomagnetic_data *data = input_get_drvdata(input_raw);
    int threshold;

    geomagnetic_multi_lock();

    threshold = data->threshold;

    geomagnetic_multi_unlock();

    return sprintf(buf, "%d\n", threshold);
}

static ssize_t
geomagnetic_raw_threshold_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf,
        size_t count)
{
    struct input_dev *input_raw = to_input_dev(dev);
    struct geomagnetic_data *data = input_get_drvdata(input_raw);
    int value = simple_strtol(buf, NULL, 10);

    geomagnetic_multi_lock();

    if (0 <= value && value <= 2) {
        data->threshold = value;
        input_report_abs(data->input_raw, ABS_RAW_THRESHOLD, value);
		input_sync(data->input_raw);
    }

    geomagnetic_multi_unlock();

    return count;
}

static ssize_t
geomagnetic_raw_distortion_show(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
    struct input_dev *input_raw = to_input_dev(dev);
    struct geomagnetic_data *data = input_get_drvdata(input_raw);
    int rt;

    geomagnetic_multi_lock();

    rt = sprintf(buf, "%d %d %d\n",
            data->distortion[0],
            data->distortion[1],
            data->distortion[2]);

    geomagnetic_multi_unlock();

    return rt;
}

static ssize_t
geomagnetic_raw_distortion_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf,
        size_t count)
{
    struct input_dev *input_raw = to_input_dev(dev);
    struct geomagnetic_data *data = input_get_drvdata(input_raw);
    int32_t distortion[3];
    static int32_t val = 1;
    int i;

    geomagnetic_multi_lock();

    sscanf(buf, "%d %d %d",
            &distortion[0],
            &distortion[1],
            &distortion[2]);
    if (distortion[0] > 0 && distortion[1] > 0 && distortion[2] > 0) {
        for (i = 0; i < 3; i++) {
            data->distortion[i] = distortion[i];
        }
        input_report_abs(data->input_raw, ABS_RAW_DISTORTION, val++);
		input_sync(data->input_raw);
		
    }

    geomagnetic_multi_unlock();

    return count;
}

static ssize_t
geomagnetic_raw_shape_show(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
    struct input_dev *input_raw = to_input_dev(dev);
    struct geomagnetic_data *data = input_get_drvdata(input_raw);
    int shape;

    geomagnetic_multi_lock();

    shape = data->shape;

    geomagnetic_multi_unlock();

    return sprintf(buf, "%d\n", shape);
}

static ssize_t
geomagnetic_raw_shape_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf,
        size_t count)
{
    struct input_dev *input_raw = to_input_dev(dev);
    struct geomagnetic_data *data = input_get_drvdata(input_raw);
    int value = simple_strtol(buf, NULL, 10);

    geomagnetic_multi_lock();

    if (0 <= value && value <= 1) {
        data->shape = value;
        input_report_abs(data->input_raw, ABS_RAW_SHAPE, value);
		input_sync(data->input_raw);
    }

    geomagnetic_multi_unlock();

    return count;
}

static ssize_t
geomagnetic_raw_offsets_show(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
    struct input_dev *input_raw = to_input_dev(dev);
    struct geomagnetic_data *data = input_get_drvdata(input_raw);
    struct yas_mag_offset offset;
    int accuracy;

    geomagnetic_multi_lock();

    offset = data->driver_offset;
    accuracy = atomic_read(&data->last_status);

    geomagnetic_multi_unlock();

    return sprintf(buf, "%d %d %d %d %d %d %d\n",
            offset.hard_offset[0],
            offset.hard_offset[1],
            offset.hard_offset[2],
            offset.calib_offset.v[0],
            offset.calib_offset.v[1],
            offset.calib_offset.v[2],
            accuracy);
}

static ssize_t
geomagnetic_raw_offsets_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf,
        size_t count)
{
    struct input_dev *input_raw = to_input_dev(dev);
    struct geomagnetic_data *data = input_get_drvdata(input_raw);
    struct yas_mag_offset offset;
    int32_t hard_offset[3];
    int i, accuracy;

    geomagnetic_multi_lock();

    sscanf(buf, "%d %d %d %d %d %d %d",
            &hard_offset[0],
            &hard_offset[1],
            &hard_offset[2],
            &offset.calib_offset.v[0],
            &offset.calib_offset.v[1],
            &offset.calib_offset.v[2],
            &accuracy);
    if (0 <= accuracy && accuracy <= 3) {
        for (i = 0; i < 3; i++) {
            offset.hard_offset[i] = (int8_t)hard_offset[i];
        }
        if (hwdep_driver.set_offset(&offset) == 0) {
            atomic_set(&data->last_status, accuracy);
            data->driver_offset = offset;
        }
    }

    geomagnetic_multi_unlock();

    return count;
}

static DEVICE_ATTR(threshold, S_IRUGO|S_IWUSR,
        geomagnetic_raw_threshold_show, geomagnetic_raw_threshold_store);
static DEVICE_ATTR(distortion, S_IRUGO|S_IWUSR,
        geomagnetic_raw_distortion_show, geomagnetic_raw_distortion_store);
static DEVICE_ATTR(shape, S_IRUGO|S_IWUSR,
        geomagnetic_raw_shape_show, geomagnetic_raw_shape_store);
static DEVICE_ATTR(offsets, S_IRUGO|S_IWUSR,
        geomagnetic_raw_offsets_show, geomagnetic_raw_offsets_store);

static struct attribute *geomagnetic_raw_attributes[] = {
    &dev_attr_threshold.attr,
    &dev_attr_distortion.attr,
    &dev_attr_shape.attr,
    &dev_attr_offsets.attr,
    NULL
};

static struct attribute_group geomagnetic_raw_attribute_group = {
    .attrs = geomagnetic_raw_attributes
};

/* Interface Functions for Lower Layer */

static int
geomagnetic_work(struct yas_mag_data *magdata)
{
    struct geomagnetic_data *data = i2c_get_clientdata(this_client);
    uint32_t time_delay_ms = 100;
    static int cnt = 0;
    int rt, i, accuracy;

    if (hwdep_driver.measure == NULL || hwdep_driver.get_offset == NULL) {
        return time_delay_ms;
    }

    rt = hwdep_driver.measure(magdata, &time_delay_ms);
    if (rt < 0) {
        YLOGE(("measure failed[%d]\n", rt));
    }
    YLOGD(("xy1y2 [%d][%d][%d] raw[%d][%d][%d]\n",
            magdata->xy1y2.v[0], magdata->xy1y2.v[1], magdata->xy1y2.v[2],
            magdata->xyz.v[0], magdata->xyz.v[1], magdata->xyz.v[2]));

    if (rt >= 0) {
        accuracy = atomic_read(&data->last_status);

        if ((rt & YAS_REPORT_OVERFLOW_OCCURED)
                || (rt & YAS_REPORT_HARD_OFFSET_CHANGED)
                || (rt & YAS_REPORT_CALIB_OFFSET_CHANGED)) {
            static uint16_t count = 1;
            int code = 0;
            int value = 0;

            hwdep_driver.get_offset(&data->driver_offset);
            if (rt & YAS_REPORT_OVERFLOW_OCCURED) {
                atomic_set(&data->last_status, 0);
                accuracy = 0;
            }

            /* report event */
            code |= (rt & YAS_REPORT_OVERFLOW_OCCURED);
            code |= (rt & YAS_REPORT_HARD_OFFSET_CHANGED);
            code |= (rt & YAS_REPORT_CALIB_OFFSET_CHANGED);
            value = (count++ << 16) | (code);
            input_report_abs(data->input_raw, ABS_RAW_REPORT, value);
			input_sync(data->input_raw);
        }

        if (rt & YAS_REPORT_DATA) {
            /* report magnetic data in [nT] */
            input_report_abs(data->input_data, ABS_X, magdata->xyz.v[0]);
            input_report_abs(data->input_data, ABS_Y, magdata->xyz.v[1]);
            input_report_abs(data->input_data, ABS_Z, magdata->xyz.v[2]);

            if (atomic_read(&data->last_data[0]) == magdata->xyz.v[0]
                    && atomic_read(&data->last_data[1]) == magdata->xyz.v[1]
                    && atomic_read(&data->last_data[2]) == magdata->xyz.v[2]) {
                input_report_abs(data->input_data, ABS_RUDDER, cnt++);
				input_sync(data->input_data);
            }
            input_report_abs(data->input_data, ABS_STATUS, accuracy);
            input_sync(data->input_data);

            for (i = 0; i < 3; i++) {
                atomic_set(&data->last_data[i], magdata->xyz.v[i]);
            }
        }

        if (rt & YAS_REPORT_CALIB) {
            /* report raw magnetic data */
            input_report_abs(data->input_raw, ABS_X, magdata->raw.v[0]);
            input_report_abs(data->input_raw, ABS_Y, magdata->raw.v[1]);
            input_report_abs(data->input_raw, ABS_Z, magdata->raw.v[2]);
            input_sync(data->input_raw);
        }
    }
    else {
        time_delay_ms = 100;
    }

    return time_delay_ms;

}

static void
geomagnetic_input_work_func(struct work_struct *work)
{
    struct geomagnetic_data *data = container_of((struct delayed_work *)work,
            struct geomagnetic_data, work);
    uint32_t time_delay_ms;
    struct yas_mag_data magdata;

    time_delay_ms = geomagnetic_work(&magdata);

    if (time_delay_ms > 0) {
        schedule_delayed_work(&data->work, msecs_to_jiffies(time_delay_ms) + 1);
    }
    else {
        schedule_delayed_work(&data->work, 0);
    }
}

static int
geomagnetic_suspend(struct i2c_client *client, pm_message_t mesg)
{
    struct geomagnetic_data *data = i2c_get_clientdata(client);

    if (atomic_read(&data->enable)) {
        cancel_delayed_work_sync(&data->work);
    }
#if DEBUG
    data->suspend = 1;
#endif

    return 0;
}

static int
geomagnetic_resume(struct i2c_client *client)
{
    struct geomagnetic_data *data = i2c_get_clientdata(client);

    if (atomic_read(&data->enable)) {
        schedule_delayed_work(&data->work, 0);
    }

#if DEBUG
    data->suspend = 0;
#endif

    return 0;
}

#ifdef MEDIATEK_CODE

#define GEOMAGNETIC 0x84
#define GEOMAGNETIC_IOCTL_INIT              _IO(GEOMAGNETIC, 0x01)
#define GEOMAGNETIC_IOCTL_READ_CHIPINFO     _IO(GEOMAGNETIC, 0x02)
#define GEOMAGNETIC_IOCTL_READ_SENSORDATA   _IO(GEOMAGNETIC, 0x03)
#define GEOMAGNETIC_IOCTL_READ_POSTUREDATA  _IO(GEOMAGNETIC, 0x04)
#define GEOMAGNETIC_IOCTL_READ_CALIDATA     _IO(GEOMAGNETIC, 0x05)
#define GEOMAGNETIC_IOCTL_READ_CONTROL      _IO(GEOMAGNETIC, 0x06)
#define GEOMAGNETIC_IOCTL_SET_CONTROL       _IO(GEOMAGNETIC, 0x07)
#define GEOMAGNETIC_IOCTL_SET_MODE          _IO(GEOMAGNETIC, 0x08)
#define GEOMAGNETIC_IOCTL_SET_POSTURE       _IO(GEOMAGNETIC, 0x09)
#define GEOMAGNETIC_IOCTL_SET_CALIDATA      _IO(GEOMAGNETIC, 0x0a)

#define GEOMAGNETIC_CHRDEV_NAME "msensor"

static long
ioctl_init(void)
{
    /* nothing to do */
    return 0;
}

#if 0
static int
ioctl_read_chipinfo(unsigned long args)
{
    char *p = (char *) args;
    if (copy_to_user(p, GEOMAGNETIC_CHRDEV_NAME,
                sizeof(GEOMAGNETIC_CHRDEV_NAME))) {
        return -EFAULT;
    }

    return 0;
}
#endif

static int
ioctl_read_sensordata(unsigned long args)
{
    struct geomagnetic_data *data = i2c_get_clientdata(this_client);
    char buf[64], *p;

    p = (char *) args;
    sprintf(buf, "%x %x %x",
            atomic_read(&data->last_data[0]),
            atomic_read(&data->last_data[1]),
            atomic_read(&data->last_data[2]));
    if (copy_to_user(p, buf, strlen(buf)+1)) {
        return -EFAULT;
    }

    return 0;
}

static int
ioctl_read_calidata(unsigned long args)
{
    struct geomagnetic_data *data = i2c_get_clientdata(this_client);
    int buf[7], *p, i;

    p = (int *) args;
    if (is_valid_calib_offset(data->driver_offset.calib_offset.v)) {
        for (i = 0; i < 3; i++) {
            buf[i] = data->driver_offset.calib_offset.v[i];
        }
    }
    else {
        for (i = 0; i < 3; i++) {
            buf[i] = 0;
        }
    }
    buf[3] = 0;
    buf[4] = 0;
    buf[5] = 0;
    buf[6] = atomic_read(&data->last_status);
    if (copy_to_user(p, buf, sizeof(buf))) {
        return -EFAULT;
    }

    return 0;
}

#if 0
static long
geomagnetic_dev_ioctl(struct file *f, unsigned int cmd, unsigned long args)
{
    int result = 0;

    geomagnetic_multi_lock();

    switch (cmd) {
    case GEOMAGNETIC_IOCTL_INIT:
        result = ioctl_init();
        break;
    case GEOMAGNETIC_IOCTL_READ_CHIPINFO:
        result = ioctl_read_chipinfo(args);
        break;
    case GEOMAGNETIC_IOCTL_READ_SENSORDATA:
        result = ioctl_read_sensordata(args);
        break;
    case GEOMAGNETIC_IOCTL_READ_CALIDATA:
        result = ioctl_read_calidata(args);
        break;
    case GEOMAGNETIC_IOCTL_READ_CONTROL:
    case GEOMAGNETIC_IOCTL_READ_POSTUREDATA:
    case GEOMAGNETIC_IOCTL_SET_CONTROL:
    case GEOMAGNETIC_IOCTL_SET_CALIDATA:
    case GEOMAGNETIC_IOCTL_SET_MODE:
    case GEOMAGNETIC_IOCTL_SET_POSTURE:
    default:
        result = -ENOTTY;
        break;
    }

    geomagnetic_multi_unlock();

    return result;
}

static struct file_operations geomagnetic_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = geomagnetic_dev_ioctl,
};


static struct miscdevice geomagnetic_device = {
    .name = GEOMAGNETIC_CHRDEV_NAME,
    .fops = &geomagnetic_fops,
    .minor = MISC_DYNAMIC_MINOR,
};
#endif

int
geomagnetic_get_delay(void)
{
    struct geomagnetic_data *data = i2c_get_clientdata(this_client);
    int delay;

    if (geomagnetic_multi_lock() < 0) {
        return -1;
    }

    delay = data->delay;

    geomagnetic_multi_unlock();

    return delay;
}
EXPORT_SYMBOL(geomagnetic_get_delay);

int
geomagnetic_set_delay(int msec)
{
    struct geomagnetic_data *data = i2c_get_clientdata(this_client);

    if (hwdep_driver.set_delay == NULL) {
        return -1;
    }

    geomagnetic_multi_lock();

    if (hwdep_driver.set_delay(msec) == 0) {
        data->delay = msec;
    }

    geomagnetic_multi_unlock();

    return 0;
}
EXPORT_SYMBOL(geomagnetic_set_delay);

int
geomagnetic_get_enable(void)
{
    struct geomagnetic_data *data = i2c_get_clientdata(this_client);
    return atomic_read(&data->enable);
}
EXPORT_SYMBOL(geomagnetic_get_enable);

int
geomagnetic_set_enable(int enable)
{
    struct geomagnetic_data *data = i2c_get_clientdata(this_client);
	MSE_FUN(f);

    if (hwdep_driver.set_enable == NULL) {
        return -1;
    }

    geomagnetic_multi_lock();

    if (hwdep_driver.set_enable(enable) == 0) {
        if (enable) {
            geomagnetic_enable(data);
        }
        else {
            geomagnetic_disable(data);
        }
    }

    geomagnetic_multi_unlock();

    return 0;
}
EXPORT_SYMBOL(geomagnetic_set_enable);

#endif

#if 0
static int
geomagnetic_remove(struct i2c_client *client)
{
    struct geomagnetic_data *data = i2c_get_clientdata(client);

    if (data != NULL) {
#ifdef MEDIATEK_CODE
        misc_deregister(&geomagnetic_device);
#endif
        geomagnetic_disable(data);
        if (hwdep_driver.term != NULL) {
            hwdep_driver.term();
        }

        input_unregister_device(data->input_raw);
        sysfs_remove_group(&data->input_data->dev.kobj,
                &geomagnetic_attribute_group);
        sysfs_remove_group(&data->input_raw->dev.kobj,
                &geomagnetic_raw_attribute_group);
        input_unregister_device(data->input_data);
        kfree(data);
    }

    return 0;
}
#endif


#ifdef GEOMAGNETIC_PLATFORM_API
static int
geomagnetic_api_enable(int enable)
{
    struct geomagnetic_data *data = i2c_get_clientdata(this_client);
    int rt;

    if (geomagnetic_multi_lock() < 0) {
        return -1;
    }
    enable = !!enable;

    if ((rt = hwdep_driver.set_enable(enable)) == 0) {
        atomic_set(&data->enable, enable);
        if (enable) {
            rt = hwdep_driver.set_delay(20);
        }
    }

    geomagnetic_multi_unlock();

    return rt;
}

int
geomagnetic_api_resume(void)
{
    return geomagnetic_api_enable(1);
}
EXPORT_SYMBOL(geomagnetic_api_resume);

int
geomagnetic_api_suspend(void)
{
    return geomagnetic_api_enable(0);
}
EXPORT_SYMBOL(geomagnetic_api_suspend);

int
geomagnetic_api_read(int *xyz, int *raw, int *xy1y2, int *accuracy)
{
    struct geomagnetic_data *data = i2c_get_clientdata(this_client);
    struct yas_mag_data magdata;
    int i;

    geomagnetic_work(&magdata);
    if (xyz != NULL) {
        for (i = 0; i < 3; i++) {
            xyz[i] = magdata.xyz.v[i];
        }
    }
    if (raw != NULL) {
        for (i = 0; i < 3; i++) {
            raw[i] = magdata.raw.v[i];
        }
    }
    if (xy1y2 != NULL) {
        for (i = 0; i < 3; i++) {
            xy1y2[i] = magdata.xy1y2.v[i];
        }
    }
    if (accuracy != NULL) {
        *accuracy = atomic_read(&data->last_status);
    }

    return 0;
}
EXPORT_SYMBOL(geomagnetic_api_read);
#endif

/*----------------------------------------------------------------------------*/
static int yamaha530_open(struct inode *inode, struct file *file)
{    
	int ret = -1;
	
	geomagnetic_set_enable(1);	// start sample polling
	ret = nonseekable_open(inode, file);
	
	return ret;
}
/*----------------------------------------------------------------------------*/
static int yamaha530_release(struct inode *inode, struct file *file)
{	
	geomagnetic_set_enable(0); // stop sample polling
	return 0;
}


/*----------------------------------------------------------------------------*/
static long yamaha530_ioctl(struct file *file, unsigned int cmd,unsigned long arg)
{
//	int valuebuf[4];
//	int calidata[7];
//	int controlbuf[10];
//	char strbuf[64];
	void __user *data;
	long retval=0;
//	int mode=0;

//      void __user *argp = (void __user *)arg;
	geomagnetic_multi_lock();
//	MSE_FUN(f);

	switch (cmd)
	{
		case MSENSOR_IOCTL_INIT:
			data = (void __user *) arg;
			retval = ioctl_init();         
			break;

		case MSENSOR_IOCTL_SET_POSTURE:			
			break;        

		case MSENSOR_IOCTL_SET_CALIDATA:			
			break;                                

		case MSENSOR_IOCTL_READ_CHIPINFO:
			data = (void __user *) arg;
			if(data == NULL)
			{
				MSE_ERR("IO parameter pointer is NULL!\r\n");
				break;
			}		
			
			if(copy_to_user(data, "yas529", sizeof("yas529")))
			{
				retval = -EFAULT;
				goto err_out;
			}                
			break;

		case MSENSOR_IOCTL_READ_SENSORDATA:
			
			retval = ioctl_read_sensordata(arg);
			               
			break;                

		case MSENSOR_IOCTL_READ_POSTUREDATA:			           
			break;            

		case MSENSOR_IOCTL_READ_CALIDATA:
			
			retval = ioctl_read_calidata(arg);
			               
			break;

		case MSENSOR_IOCTL_READ_CONTROL:			                           
			break;

		case MSENSOR_IOCTL_SET_CONTROL:			  
			break;

		case MSENSOR_IOCTL_SET_MODE:			                
			break;

		case MSENSOR_IOCTL_SENSOR_ENABLE:
			break;
		case MSENSOR_IOCTL_READ_FACTORY_SENSORDATA:
			break;
			/*
		case MSENSOR_IOCTL_READ_FACTORY_SENSORDATA:			
			if(argp == NULL)
			{
				printk(KERN_ERR "IO parameter pointer is NULL!\r\n");
				break;    
			}
			
			//AKECS_GetRawData(buff, AKM8975_BUFSIZE);
			osensor_data = (hwm_sensor_data *)buff;
		    mutex_lock(&sensor_data_mutex);
				
			osensor_data->values[0] = sensor_data[0] * CONVERT_O;
			osensor_data->values[1] = sensor_data[1] * CONVERT_O;
			osensor_data->values[2] = sensor_data[2] * CONVERT_O;
			osensor_data->status = sensor_data[4];
			osensor_data->value_divide = CONVERT_O_DIV;
					
			mutex_unlock(&sensor_data_mutex);

            sprintf(buff, "%x %x %x %x %x", osensor_data->values[0], osensor_data->values[1],
				osensor_data->values[2],osensor_data->status,osensor_data->value_divide);
			if(copy_to_user(argp, buff, strlen(buff)+1))
			{
				return -EFAULT;
			} 
			
			break;
		    */
		default:
			MSE_ERR("%s not supported = 0x%04x", __FUNCTION__, cmd);
			retval = -ENOIOCTLCMD;
			break;
		}

	geomagnetic_multi_unlock();
	err_out:
	return retval;

}
/*----------------------------------------------------------------------------*/
static struct file_operations yamaha530_fops = {
	.owner = THIS_MODULE,
	.open = yamaha530_open,
	.release = yamaha530_release,
	.unlocked_ioctl = yamaha530_ioctl,
};
/*----------------------------------------------------------------------------*/
static struct miscdevice yamaha530_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "msensor",
    .fops = &yamaha530_fops,
};
/*----------------------------------------------------------------------------*/
int yamaha530_operate(void* self, uint32_t command, void* buff_in, int size_in,
		void* buff_out, int size_out, int* actualout)
{
	int err = 0;
	int value, sample_delay;
	hwm_sensor_data* msensor_data;
	struct geomagnetic_data *data = i2c_get_clientdata(this_client);

//	geomagnetic_multi_lock();
	MSE_FUN(f);
	switch (command)
	{
		case SENSOR_DELAY:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				MSE_ERR("Set delay parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				value = *(int *)buff_in;
				if(value <= 20)
				{
					sample_delay = 20;
				}				
				geomagnetic_set_delay(sample_delay);
			}	
			break;

		case SENSOR_ENABLE:
			if((buff_in == NULL) || (size_in < sizeof(int)))
			{
				MSE_ERR("Enable sensor parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				value = *(int *)buff_in;				
				geomagnetic_set_enable(value);
				// TODO: turn device into standby or normal mode
			}
			break;

		case SENSOR_GET_DATA:
			if((buff_out == NULL) || (size_out< sizeof(hwm_sensor_data)))
			{
				MSE_ERR("get sensor data parameter error!\n");
				err = -EINVAL;
			}
			else
			{
				

				msensor_data = (hwm_sensor_data *)buff_out;

				msensor_data->values[0] = atomic_read(&data->last_data[0]),
				msensor_data->values[1] = atomic_read(&data->last_data[1]),
				msensor_data->values[2] = atomic_read(&data->last_data[2]),
				msensor_data->status = atomic_read(&data->last_status);
								
				msensor_data->value_divide = 1000;				
			}
			break;
		default:
			MSE_ERR("msensor operate function no this parameter %d!\n", command);
			err = -1;
			break;
	}

//	geomagnetic_multi_unlock();
	
	return err;

}

static DRIVER_ATTR(daemon,      S_IRUGO, show_daemon_name, NULL);
static DRIVER_ATTR(daemon2,      S_IRUGO, show_daemon2_name, NULL);

static struct driver_attribute *yamaha530_attr_list[] = {
    &driver_attr_daemon,
    &driver_attr_daemon2,
	
};

static int yamaha530_create_attr(struct device_driver *driver) 
{
	int idx, err = 0;
	int num = (int)(sizeof(yamaha530_attr_list)/sizeof(yamaha530_attr_list[0]));
	if (driver == NULL)
	{
		return -EINVAL;
	}

	for(idx = 0; idx < num; idx++)
	{
		if((err = driver_create_file(driver, yamaha530_attr_list[idx])))
		{            
			MSE_ERR("driver_create_file (%s) = %d\n", yamaha530_attr_list[idx]->attr.name, err);
			break;
		}
	}    
	return err;
}

static int yamaha530_delete_attr(struct device_driver *driver)
{
	int idx ,err = 0;
	int num = (int)(sizeof(yamaha530_attr_list)/sizeof(yamaha530_attr_list[0]));

	if(driver == NULL)
	{
		return -EINVAL;
	}
	

	for(idx = 0; idx < num; idx++)
	{
		driver_remove_file(driver, yamaha530_attr_list[idx]);
	}
	

	return err;
}



static int yamaha530_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct geomagnetic_data *data = NULL;
    struct input_dev *input_data = NULL, *input_raw = NULL;
    int rt, sysfs_created = 0, sysfs_raw_created = 0;
    int data_registered = 0, raw_registered = 0, i;
	struct hwmsen_object sobj_m;
	 struct yas_mag_filter filter;

    i2c_set_clientdata(client, NULL);
    data = kzalloc(sizeof(struct geomagnetic_data), GFP_KERNEL);
    if (data == NULL) {
        rt = -ENOMEM;
        goto err;
    }

	data->hw = get_cust_mag_hw();
	
    data->threshold = YAS_DEFAULT_MAGCALIB_THRESHOLD;
    for (i = 0; i < 3; i++) {
        data->distortion[i] = YAS_DEFAULT_MAGCALIB_DISTORTION;
    }
    data->shape = 0;
    atomic_set(&data->enable, 0);
    for (i = 0; i < 3; i++) {
        atomic_set(&data->last_data[i], 0);
    }
    atomic_set(&data->last_status, 0);
    INIT_DELAYED_WORK(&data->work, geomagnetic_input_work_func);
	//data->driver_lock =__MUTEX_INITIALIZER(data->driver_lock);
	//data->multi_lock =__MUTEX_INITIALIZER(data->multi_lock);
    sema_init(&data->driver_lock,1);
    sema_init(&data->multi_lock,1);	

    input_data = input_allocate_device();
    if (input_data == NULL) {
        rt = -ENOMEM;
        printk(KERN_ERR
               "geomagnetic_probe: Failed to allocate input_data device\n");
        goto err;
    }

    input_data->name = GEOMAGNETIC_INPUT_NAME;
    input_data->id.bustype = BUS_I2C;
    set_bit(EV_ABS, input_data->evbit);
    input_set_abs_params(input_data, ABS_X, 0x80000000, 0x7fffffff, 0, 0);
    input_set_abs_params(input_data, ABS_Y, 0x80000000, 0x7fffffff, 0, 0);
    input_set_abs_params(input_data, ABS_Z, 0x80000000, 0x7fffffff, 0, 0);
    input_set_abs_params(input_data, ABS_RUDDER, 0x80000000, 0x7fffffff, 0, 0);
    input_set_abs_params(input_data, ABS_STATUS, 0, 3, 0, 0);
    input_set_abs_params(input_data, ABS_WAKE, 0x80000000, 0x7fffffff, 0, 0);
    input_data->dev.parent = &client->dev;

    rt = input_register_device(input_data);
    if (rt) {
        printk(KERN_ERR
               "geomagnetic_probe: Unable to register input_data device: %s\n",
               input_data->name);
        goto err;
    }
    data_registered = 1;

	if(yamaha530_create_attr(&yamaha_sensor_driver.driver))
	{
		printk("yamaha530 create attribute err\n");
		goto  err;
	}

    rt = sysfs_create_group(&input_data->dev.kobj,
            &geomagnetic_attribute_group);
    if (rt) {
        printk(KERN_ERR
               "geomagnetic_probe: sysfs_create_group failed[%s]\n",
               input_data->name);
        goto err;
    }
    sysfs_created = 1;

    input_raw = input_allocate_device();
    if (input_raw == NULL) {
        rt = -ENOMEM;
        printk(KERN_ERR
               "geomagnetic_probe: Failed to allocate input_raw device\n");
        goto err;
    }

    input_raw->name = GEOMAGNETIC_INPUT_RAW_NAME;
    input_raw->id.bustype = BUS_I2C;
    set_bit(EV_ABS, input_raw->evbit);
    input_set_abs_params(input_raw, ABS_X, 0x80000000, 0x7fffffff, 0, 0);
    input_set_abs_params(input_raw, ABS_Y, 0x80000000, 0x7fffffff, 0, 0);
    input_set_abs_params(input_raw, ABS_Z, 0x80000000, 0x7fffffff, 0, 0);
    input_set_abs_params(input_raw, ABS_RAW_DISTORTION, 0, 0x7fffffff, 0, 0);
    input_set_abs_params(input_raw, ABS_RAW_THRESHOLD, 0, 2, 0, 0);
    input_set_abs_params(input_raw, ABS_RAW_SHAPE, 0, 1, 0, 0);
    input_set_abs_params(input_raw, ABS_RAW_REPORT, 0x80000000, 0x7fffffff, 0, 0);
    input_raw->dev.parent = &client->dev;

    rt = input_register_device(input_raw);
    if (rt) {
        printk(KERN_ERR
               "geomagnetic_probe: Unable to register input_raw device: %s\n",
               input_raw->name);
        goto err;
    }
    raw_registered = 1;

    rt = sysfs_create_group(&input_raw->dev.kobj,
            &geomagnetic_raw_attribute_group);
    if (rt) {
        printk(KERN_ERR
               "geomagnetic_probe: sysfs_create_group failed[%s]\n",
               input_data->name);
        goto err;
    }
    sysfs_raw_created = 1;	

    this_client = client;
	//client->addr = client->addr & I2C_MASK_FLAG | I2C_DMA_FLAG;
	client->timing = 200;
    data->input_raw = input_raw;
    data->input_data = input_data;
	data->client = client;
    input_set_drvdata(input_data, data);
    input_set_drvdata(input_raw, data);
    i2c_set_clientdata(client, data);

    if ((rt = yas_mag_driver_init(&hwdep_driver)) < 0) {
        printk(KERN_ERR "geomagnetic_driver_init failed[%d]\n", rt);
        goto err;
    }
    if(hwdep_driver.init != NULL) {
        if ((rt = hwdep_driver.init()) < 0) {
            printk(KERN_ERR "hwdep_driver.init() failed[%d]\n", rt);
            goto err;
        }
    }
    if (hwdep_driver.set_position != NULL) {
        if (hwdep_driver.set_position(data->hw->direction) < 0) {
            printk(KERN_ERR "hwdep_driver.set_position() failed[%d]\n", rt);
            goto err;
        }
    }
    if (hwdep_driver.get_offset != NULL) {
        if (hwdep_driver.get_offset(&data->driver_offset) < 0) {
            printk(KERN_ERR "hwdep_driver get_driver_state failed\n");
            goto err;
        }
    }
    if (hwdep_driver.get_delay != NULL) {
        data->delay = hwdep_driver.get_delay();
    }
    if (hwdep_driver.set_filter_enable != NULL) {
        /* default to enable */
        if (hwdep_driver.set_filter_enable(1) == 0) {
            data->filter_enable = 1;
        }
    }
    if (hwdep_driver.get_filter != NULL) {
        if (hwdep_driver.get_filter(&filter) < 0) {
            YLOGE(("hwdep_driver get_filter failed\n"));
            goto err;
        }
        data->filter_len = filter.len;
        for (i = 0; i < 3; i++) {
            data->filter_noise[i] = filter.noise[i];
        }
        data->filter_threshold = filter.threshold;
    }

	if((rt = misc_register(&yamaha530_device)))
	{
		MSE_ERR("yamaha530_device register failed\n");
		goto err;
	}    

	sobj_m.self = data;
    sobj_m.polling = 1;
    sobj_m.sensor_operate = yamaha530_operate;
	if((rt = hwmsen_attach(ID_MAGNETIC, &sobj_m)))
	{
		MSE_ERR("attach fail = %d\n", rt);
		goto err;
	}
	printk("yamaha530 i2c probe ok!\n");
    return 0;

    err:
    if (data != NULL) {
        if (input_raw != NULL) {
            if (sysfs_raw_created) {
                sysfs_remove_group(&input_raw->dev.kobj,
                        &geomagnetic_raw_attribute_group);
            }
            if (raw_registered) {
                input_unregister_device(input_raw);
            }
            else {
                input_free_device(input_raw);
            }
        }
        if (input_data != NULL) {
            if (sysfs_created) {
                sysfs_remove_group(&input_data->dev.kobj,
                        &geomagnetic_attribute_group);
            }
            if (data_registered) {
                input_unregister_device(input_data);
            }
            else {
                input_free_device(input_data);
            }
        }
        kfree(data);
    }

    return rt;
}


/*----------------------------------------------------------------------------*/
static struct i2c_driver yamaha530_i2c_driver = {
    .driver = {
//        .owner = THIS_MODULE, 
        .name  = YAMAHA530_DEV_NAME,
    },
	.probe      = yamaha530_i2c_probe,
	.remove     = yamaha530_i2c_remove,
	.detect     = yamaha530_i2c_detect,
	.suspend    = geomagnetic_suspend,
	.resume     = geomagnetic_resume,

	.id_table = yamaha530_i2c_id,
//	.address_data = &yamaha530_addr_data,
};
/*----------------------------------------------------------------------------*/
static atomic_t dev_open_count;
/*----------------------------------------------------------------------------*/


static int yamaha530_i2c_detect(struct i2c_client *client, struct i2c_board_info *info) 
{    
	strcpy(info->type, YAMAHA530_DEV_NAME);
	return 0;
}



/*----------------------------------------------------------------------------*/
static int yamaha530_i2c_remove(struct i2c_client *client)
{
	int err;
	struct geomagnetic_data *data = i2c_get_clientdata(client);

	if(data != NULL)
	{
		geomagnetic_disable(data);
		if(hwdep_driver.term != NULL)
		{
			hwdep_driver.term();
		}

		err = yamaha530_delete_attr(&yamaha_sensor_driver.driver);

		if(err)
	    {
		   printk("yamaha530_delete_attr fail: %d\n", err);
	    }

		input_unregister_device(data->input_raw);
        sysfs_remove_group(&data->input_data->dev.kobj,
                &geomagnetic_attribute_group);
        sysfs_remove_group(&data->input_raw->dev.kobj,
                &geomagnetic_raw_attribute_group);
        input_unregister_device(data->input_data);
        kfree(data);
		kfree(data);
    }	
	 
	this_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));	
	misc_deregister(&yamaha530_device);    
	return 0;
}
/*----------------------------------------------------------------------------*/
static int yamaha_probe(struct platform_device *pdev) 
{
	//struct mag_hw *hw = get_cust_mag_hw();

	//yamaha530_power(hw, 1);    
	//yamaha530_force[0] = hw->i2c_num;
	if(i2c_add_driver(&yamaha530_i2c_driver))
	{
		MSE_ERR("add driver error\n");
		return -1;
	} 
	return 0;
}
/*----------------------------------------------------------------------------*/
static int yamaha_remove(struct platform_device *pdev)
{
	//struct mag_hw *hw = get_cust_mag_hw();

	MSE_FUN();    
	//yamaha530_power(hw, 0);    
	atomic_set(&dev_open_count, 0);  
	i2c_del_driver(&yamaha530_i2c_driver);
	return 0;
}
/*----------------------------------------------------------------------------*/
#if 0
static struct platform_driver yamaha_sensor_driver = {
	.probe      = yamaha_probe,
	.remove     = yamaha_remove,    
	.driver     = {
		.name  = "msensor",
//		.owner = THIS_MODULE,
	}
};
#endif

#ifdef CONFIG_OF
static const struct of_device_id yamaha_of_match[] = {
	{ .compatible = "mediatek,msensor", },
	{},
};
#endif

static struct platform_driver yamaha_sensor_driver =
{
	.probe      = yamaha_probe,
	.remove     = yamaha_remove,    
	.driver     = 
	{
		.name = "msensor",
        #ifdef CONFIG_OF
		.of_match_table = yamaha_of_match,
		#endif
	}
};

/*----------------------------------------------------------------------------*/
static int __init yamaha530_init(void)
{
	struct mag_hw *hw = get_cust_mag_hw();
	MSE_FUN();
	MSE_LOG("%s: i2c_number=%d\n", __func__,hw->i2c_num); 
	i2c_register_board_info(hw->i2c_num, &i2c_YAMAHA530, 1);
	if(platform_driver_register(&yamaha_sensor_driver))
	{
		MSE_ERR("failed to register driver");
		return -ENODEV;
	}
	return 0;    
}
/*----------------------------------------------------------------------------*/
static void __exit yamaha530_exit(void)
{
	MSE_FUN();
	platform_driver_unregister(&yamaha_sensor_driver);
}
/*----------------------------------------------------------------------------*/


module_init(yamaha530_init);
module_exit(yamaha530_exit);

MODULE_AUTHOR("Yamaha Corporation");
MODULE_DESCRIPTION("YAS529 Geomagnetic Sensor Driver");
MODULE_LICENSE( "GPL" );
MODULE_VERSION("1.2.0");

