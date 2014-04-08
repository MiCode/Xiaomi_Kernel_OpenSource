/* drivers/input/misc/cm36283.c - cm36283 optical sensors driver
 *
 * Copyright (C) 2012 Capella Microsystems Inc.
 * Author: Frank Hsieh <pengyueh@gmail.com>
 *
 * Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/sensors.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/wakelock.h>
#include <linux/jiffies.h>
#include <linux/cm36283.h>
#include <linux/of_gpio.h>

#include <asm/uaccess.h>
#include <asm/setup.h>

#define I2C_RETRY_COUNT 10

#define NEAR_DELAY_TIME ((100 * HZ) / 1000)

#define CONTROL_INT_ISR_REPORT        0x00
#define CONTROL_ALS                   0x01
#define CONTROL_PS                    0x02

/* POWER SUPPLY VOLTAGE RANGE */
#define CM36283_VDD_MIN_UV	2700000
#define CM36283_VDD_MAX_UV	3300000
#define CM36283_VI2C_MIN_UV	1750000
#define CM36283_VI2C_MAX_UV	1950000

/* cm36283 polling rate in ms */
#define CM36283_LS_MIN_POLL_DELAY	1
#define CM36283_LS_MAX_POLL_DELAY	1000
#define CM36283_LS_DEFAULT_POLL_DELAY	100

#define CM36283_PS_MIN_POLL_DELAY	1
#define CM36283_PS_MAX_POLL_DELAY	1000
#define CM36283_PS_DEFAULT_POLL_DELAY	100

static struct sensors_classdev sensors_light_cdev = {
	.name = "cm36283-light",
	.vendor = "Capella",
	.version = 1,
	.handle = SENSORS_LIGHT_HANDLE,
	.type = SENSOR_TYPE_LIGHT,
	.max_range = "6553",
	.resolution = "0.0125",
	.sensor_power = "0.15",
	.min_delay = 0,
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.enabled = 0,
	.delay_msec = CM36283_LS_DEFAULT_POLL_DELAY,
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
};

static struct sensors_classdev sensors_proximity_cdev = {
	.name = "cm36283-proximity",
	.vendor = "Capella",
	.version = 1,
	.handle = SENSORS_PROXIMITY_HANDLE,
	.type = SENSOR_TYPE_PROXIMITY,
	.max_range = "5.0",
	.resolution = "5.0",
	.sensor_power = "0.18",
	.min_delay = 0,
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.enabled = 0,
	.delay_msec = CM36283_PS_DEFAULT_POLL_DELAY,
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
};


static const int als_range[] = {
	[CM36283_ALS_IT0] = 6554,
	[CM36283_ALS_IT1] = 3277,
	[CM36283_ALS_IT2] = 1638,
	[CM36283_ALS_IT3] = 819,
};

static const int als_sense[] = {
	[CM36283_ALS_IT0] = 10,
	[CM36283_ALS_IT1] = 20,
	[CM36283_ALS_IT2] = 40,
	[CM36283_ALS_IT3] = 80,
};

static void sensor_irq_do_work(struct work_struct *work);
static DECLARE_WORK(sensor_irq_work, sensor_irq_do_work);

struct cm36283_info {
	struct class *cm36283_class;
	struct device *ls_dev;
	struct device *ps_dev;

	struct input_dev *ls_input_dev;
	struct input_dev *ps_input_dev;

	struct i2c_client *i2c_client;
	struct workqueue_struct *lp_wq;

	int intr_pin;
	int als_enable;
	int ps_enable;
	int ps_irq_flag;

	uint16_t *adc_table;
	uint16_t cali_table[10];
	int irq;

	int ls_calibrate;
	
	int (*power)(int, uint8_t); /* power to the chip */

	uint32_t als_kadc;
	uint32_t als_gadc;
	uint16_t golden_adc;

	struct wake_lock ps_wake_lock;
	int psensor_opened;
	int lightsensor_opened;
	uint8_t slave_addr;

	uint8_t ps_close_thd_set;
	uint8_t ps_away_thd_set;	
	int current_level;
	uint16_t current_adc;

	uint16_t ps_conf1_val;
	uint16_t ps_conf3_val;

	uint16_t ls_cmd;
	uint8_t record_clear_int_fail;
	bool polling;
	atomic_t ls_poll_delay;
	atomic_t ps_poll_delay;
	struct regulator *vdd;
	struct regulator *vio;
	struct delayed_work ldwork;
	struct delayed_work pdwork;
	struct sensors_classdev als_cdev;
	struct sensors_classdev ps_cdev;
};
struct cm36283_info *lp_info;
int fLevel=-1;
static struct mutex als_enable_mutex, als_disable_mutex, als_get_adc_mutex;
static struct mutex ps_enable_mutex, ps_disable_mutex, ps_get_adc_mutex;
static struct mutex CM36283_control_mutex;
static struct mutex wq_lock;
static int lightsensor_enable(struct cm36283_info *lpi);
static int lightsensor_disable(struct cm36283_info *lpi);
static int initial_cm36283(struct cm36283_info *lpi);
static void psensor_initial_cmd(struct cm36283_info *lpi);
static int cm36283_power_set(struct cm36283_info *info, bool on);

int32_t als_kadc;

static int control_and_report(struct cm36283_info *lpi, uint8_t mode,
		uint16_t param, int report);

static int I2C_RxData(uint16_t slaveAddr, uint8_t cmd, uint8_t *rxData, int length)
{
	uint8_t loop_i;
	struct cm36283_info *lpi = lp_info;
	uint8_t subaddr[1];

	struct i2c_msg msgs[] = {
		{
		 .addr = slaveAddr,
		 .flags = 0,
		 .len = 1,
		 .buf = subaddr,
		 },
		{
		 .addr = slaveAddr,
		 .flags = I2C_M_RD,
		 .len = length,
		 .buf = rxData,
		 },		 
	};

	subaddr[0] = cmd;

	for (loop_i = 0; loop_i < I2C_RETRY_COUNT; loop_i++) {

		if (i2c_transfer(lp_info->i2c_client->adapter, msgs, 2) > 0)
			break;

		dev_err(&lpi->i2c_client->dev, "%s: I2C error(%d). Retrying.\n",
				__func__, cmd);
		msleep(10);
	}
	if (loop_i >= I2C_RETRY_COUNT) {
		dev_err(&lpi->i2c_client->dev, "%s: Retry count exceeds %d.",
				__func__, I2C_RETRY_COUNT);
		return -EIO;
	}

	return 0;
}

static int I2C_TxData(uint16_t slaveAddr, uint8_t *txData, int length)
{
	uint8_t loop_i;
	struct cm36283_info *lpi = lp_info;

	struct i2c_msg msg[] = {
		{
		 .addr = slaveAddr,
		 .flags = 0,
		 .len = length,
		 .buf = txData,
		 },
	};

	for (loop_i = 0; loop_i < I2C_RETRY_COUNT; loop_i++) {
		if (i2c_transfer(lp_info->i2c_client->adapter, msg, 1) > 0)
			break;

		pr_err("%s: I2C error. Retrying...\n", __func__);
		msleep(10);
	}

	if (loop_i >= I2C_RETRY_COUNT) {
		dev_err(&lpi->i2c_client->dev, "%s: Retry count exceeds %d.",
				__func__, I2C_RETRY_COUNT);
		return -EIO;
	}

	return 0;
}

static int _cm36283_I2C_Read_Word(uint16_t slaveAddr, uint8_t cmd, uint16_t *pdata)
{
	uint8_t buffer[2];
	int ret = 0;

	if (pdata == NULL)
		return -EFAULT;

	ret = I2C_RxData(slaveAddr, cmd, buffer, 2);
	if (ret < 0) {
		pr_err("%s: I2C RxData fail(%d).\n", __func__, cmd);
		return ret;
	}

	*pdata = (buffer[1]<<8)|buffer[0];

	return ret;
}

static int _cm36283_I2C_Write_Word(uint16_t SlaveAddress, uint8_t cmd, uint16_t data)
{
	char buffer[3];
	int ret = 0;

	buffer[0] = cmd;
	buffer[1] = (uint8_t)(data&0xff);
	buffer[2] = (uint8_t)((data&0xff00)>>8);	
	
	ret = I2C_TxData(SlaveAddress, buffer, 3);
	if (ret < 0) {
		pr_err("%s: I2C_TxData failed.\n", __func__);
		return -EIO;
	}

	return ret;
}

static int get_ls_adc_value(uint16_t *als_step, bool resume)
{
	struct cm36283_info *lpi = lp_info;
	uint32_t tmp;
	int ret = 0;

	if (als_step == NULL)
		return -EFAULT;

	/* Read ALS data: */
	ret = _cm36283_I2C_Read_Word(lpi->slave_addr, ALS_DATA, als_step);
	if (ret < 0) {
		dev_err(&lpi->i2c_client->dev, "%s: I2C read word failed.\n",
				__func__);
		return -EIO;
	}

	if (!lpi->ls_calibrate) {
		tmp = (uint32_t)(*als_step) * lpi->als_gadc / lpi->als_kadc;
		if (tmp > 0xFFFF)
			*als_step = 0xFFFF;
		else
			*als_step = tmp;
	}

	dev_dbg(&lpi->i2c_client->dev, "raw adc = 0x%x\n", *als_step);

	return ret;
}

static int set_lsensor_range(uint16_t low_thd, uint16_t high_thd)
{
	int ret = 0;
	struct cm36283_info *lpi = lp_info;

	_cm36283_I2C_Write_Word(lpi->slave_addr, ALS_THDH, high_thd);
	_cm36283_I2C_Write_Word(lpi->slave_addr, ALS_THDL, low_thd);

	return ret;
}

static int get_ps_adc_value(uint16_t *data)
{
	int ret = 0;
	struct cm36283_info *lpi = lp_info;

	if (data == NULL)
		return -EFAULT;	

	ret = _cm36283_I2C_Read_Word(lpi->slave_addr, PS_DATA, data);
	
	if (ret < 0)
		return ret;

	(*data) &= 0xFF;

	return ret;
}

static uint16_t mid_value(uint16_t value[], uint8_t size)
{
	int i = 0, j = 0;
	uint16_t temp = 0;

	if (size < 3)
		return 0;

	for (i = 0; i < (size - 1); i++)
		for (j = (i + 1); j < size; j++)
			if (value[i] > value[j]) {
				temp = value[i];
				value[i] = value[j];
				value[j] = temp;
			}
	return value[((size - 1) / 2)];
}

static int get_stable_ps_adc_value(uint16_t *ps_adc)
{
	uint16_t value[3] = {0, 0, 0}, mid_val = 0;
	int ret = 0;
	int i = 0;
	int wait_count = 0;
	struct cm36283_info *lpi = lp_info;

	for (i = 0; i < 3; i++) {
		/*wait interrupt GPIO high*/
		while (gpio_get_value(lpi->intr_pin) == 0) {
			msleep(10);
			wait_count++;
			if (wait_count > 12) {
				dev_err(&lpi->i2c_client->dev, "%s: interrupt GPIO low\n",
					       __func__);
				return -EIO;
			}
		}

		ret = get_ps_adc_value(&value[i]);
		if (ret < 0) {
			dev_err(&lpi->i2c_client->dev,
					"%s: error get ps value\n", __func__);
			return -EIO;
		}

		if (wait_count < 60/10) {/*wait gpio less than 60ms*/
			msleep(60 - (10*wait_count));
		}
		wait_count = 0;
	}

	mid_val = mid_value(value, 3);
	dev_dbg(&lpi->i2c_client->dev, "Sta_ps: After sort, value[0, 1, 2] = [0x%x, 0x%x, 0x%x]",
		value[0], value[1], value[2]);
	*ps_adc = (mid_val & 0xFF);

	return 0;
}

static void sensor_irq_do_work(struct work_struct *work)
{
	struct cm36283_info *lpi = lp_info;
	uint16_t intFlag;
	_cm36283_I2C_Read_Word(lpi->slave_addr, INT_FLAG, &intFlag);
	control_and_report(lpi, CONTROL_INT_ISR_REPORT, intFlag, 1);

	enable_irq(lpi->irq);
}

static int get_als_range(void)
{
	uint16_t ls_conf;
	int ret = 0;
	int index = 0;
	struct cm36283_info *lpi = lp_info;

	ret = _cm36283_I2C_Read_Word(lpi->slave_addr, ALS_CONF, &ls_conf);
	if (ret) {
		dev_err(&lpi->i2c_client->dev, "read ALS_CONF from i2c error. %d\n",
				ret);
		return -EIO;
	}

	index = (ls_conf & 0xC0) >> 0x06;
	return  als_range[index];
}

static int get_als_sense(void)
{
	uint16_t ls_conf;
	int ret = 0;
	int index = 0;
	struct cm36283_info *lpi = lp_info;

	ret = _cm36283_I2C_Read_Word(lpi->slave_addr, ALS_CONF, &ls_conf);
	if (ret) {
		dev_err(&lpi->i2c_client->dev, "read ALS_CONF from i2c error. %d\n",
				ret);
		return -EIO;
	}

	index = (ls_conf & 0xC0) >> 0x06;
	return  als_sense[index];
}

static void psensor_delay_work_handler(struct work_struct *work)
{
	struct cm36283_info *lpi = lp_info;
	uint16_t adc_value = 0;
	int ret;

	mutex_lock(&wq_lock);

	ret = get_ps_adc_value(&adc_value);

	mutex_unlock(&wq_lock);

	if (ret >= 0) {
		input_report_abs(lpi->ps_input_dev, ABS_DISTANCE,
				adc_value > lpi->ps_close_thd_set ? 0 : 1);
		input_sync(lpi->ps_input_dev);
	}
	schedule_delayed_work(&lpi->pdwork,
			msecs_to_jiffies(atomic_read(&lpi->ps_poll_delay)));
}

static void lsensor_delay_work_handler(struct work_struct *work)
{
	struct cm36283_info *lpi = lp_info;
	uint16_t adc_value = 0;
	int sense;

	mutex_lock(&wq_lock);

	get_ls_adc_value(&adc_value, 0);
	sense = get_als_sense();

	mutex_unlock(&wq_lock);

	if (sense > 0) {
		lpi->current_adc = adc_value;
		input_report_abs(lpi->ls_input_dev, ABS_MISC, adc_value/sense);
		input_sync(lpi->ls_input_dev);
	}
	schedule_delayed_work(&lpi->ldwork,
			msecs_to_jiffies(atomic_read(&lpi->ls_poll_delay)));
}

static irqreturn_t cm36283_irq_handler(int irq, void *data)
{
	struct cm36283_info *lpi = data;

	disable_irq_nosync(lpi->irq);
	queue_work(lpi->lp_wq, &sensor_irq_work);

	return IRQ_HANDLED;
}

static int als_power(int enable)
{
	struct cm36283_info *lpi = lp_info;

	if (lpi->power)
		lpi->power(LS_PWR_ON, 1);

	return 0;
}

static void ls_initial_cmd(struct cm36283_info *lpi)
{	
	/*must disable l-sensor interrupt befrore IST create*//*disable ALS func*/
	lpi->ls_cmd &= CM36283_ALS_INT_MASK;
  lpi->ls_cmd |= CM36283_ALS_SD;
  _cm36283_I2C_Write_Word(lpi->slave_addr, ALS_CONF, lpi->ls_cmd);  
}

static void psensor_initial_cmd(struct cm36283_info *lpi)
{
	/*must disable p-sensor interrupt befrore IST create*/
	lpi->ps_conf1_val |= CM36283_PS_SD;
	lpi->ps_conf1_val &= CM36283_PS_INT_MASK;
	_cm36283_I2C_Write_Word(lpi->slave_addr, PS_CONF1, lpi->ps_conf1_val);
	_cm36283_I2C_Write_Word(lpi->slave_addr, PS_CONF3, lpi->ps_conf3_val);
	_cm36283_I2C_Write_Word(lpi->slave_addr, PS_THD,
			(lpi->ps_close_thd_set << 8) | lpi->ps_away_thd_set);

	dev_dbg(&lpi->i2c_client->dev,
			"%s:send psensor initial command finished\n", __func__);
}

static int psensor_enable(struct cm36283_info *lpi)
{
	int ret = -EIO;
	unsigned int delay;
	
	mutex_lock(&ps_enable_mutex);
	dev_dbg(&lpi->i2c_client->dev, "psensor enable!\n");

	if (lpi->ps_enable) {
		dev_err(&lpi->i2c_client->dev, "already enabled\n");
		ret = 0;
	} else {
		ret = control_and_report(lpi, CONTROL_PS, 1, 0);
	}

	mutex_unlock(&ps_enable_mutex);

	delay = atomic_read(&lpi->ps_poll_delay);
	if (lpi->polling)
		schedule_delayed_work(&lpi->pdwork, msecs_to_jiffies(delay));

	return ret;
}

static int psensor_disable(struct cm36283_info *lpi)
{
	int ret = -EIO;

	if (lpi->polling)
		cancel_delayed_work_sync(&lpi->pdwork);

	mutex_lock(&ps_disable_mutex);
	dev_dbg(&lpi->i2c_client->dev, "psensor disable!\n");

	if (lpi->ps_enable == 0) {
		dev_err(&lpi->i2c_client->dev, "already disabled\n");
		ret = 0;
	} else {
		ret = control_and_report(lpi, CONTROL_PS, 0, 0);
	}

	mutex_unlock(&ps_disable_mutex);
	return ret;
}

static int psensor_open(struct inode *inode, struct file *file)
{
	struct cm36283_info *lpi = lp_info;

	dev_dbg(&lpi->i2c_client->dev, "psensor open!");

	if (lpi->psensor_opened)
		return -EBUSY;

	lpi->psensor_opened = 1;

	return 0;
}

static int psensor_release(struct inode *inode, struct file *file)
{
	struct cm36283_info *lpi = lp_info;

	dev_dbg(&lpi->i2c_client->dev, "psensor release!");

	lpi->psensor_opened = 0;

	return psensor_disable(lpi);
	//return 0;
}

static long psensor_ioctl(struct file *file, unsigned int cmd,
			unsigned long arg)
{
	int val;
	struct cm36283_info *lpi = lp_info;

	dev_dbg(&lpi->i2c_client->dev, "%s cmd %d\n", __func__, _IOC_NR(cmd));

	switch (cmd) {
	case CAPELLA_CM3602_IOCTL_ENABLE:
		if (get_user(val, (unsigned long __user *)arg))
			return -EFAULT;
		if (val)
			return psensor_enable(lpi);
		else
			return psensor_disable(lpi);
		break;
	case CAPELLA_CM3602_IOCTL_GET_ENABLED:
		return put_user(lpi->ps_enable, (unsigned long __user *)arg);
		break;
	default:
		dev_err(&lpi->i2c_client->dev, "%s: invalid cmd %d\n",
			__func__, _IOC_NR(cmd));
		return -EINVAL;
	}
}

static const struct file_operations psensor_fops = {
	.owner = THIS_MODULE,
	.open = psensor_open,
	.release = psensor_release,
	.unlocked_ioctl = psensor_ioctl
};

void lightsensor_set_kvalue(struct cm36283_info *lpi)
{
	if (!lpi) {
		pr_err("%s: ls_info is empty\n", __func__);
		return;
	}

	dev_dbg(&lpi->i2c_client->dev, "%s: ALS calibrated als_kadc=0x%x\n",
			__func__, als_kadc);

	if (als_kadc >> 16 == ALS_CALIBRATED)
		lpi->als_kadc = als_kadc & 0xFFFF;
	else {
		lpi->als_kadc = 0;
		dev_dbg(&lpi->i2c_client->dev, "%s: no ALS calibrated\n",
				__func__);
	}

	if (lpi->als_kadc && lpi->golden_adc > 0) {
		lpi->als_kadc = (lpi->als_kadc > 0 && lpi->als_kadc < 0x1000) ?
				lpi->als_kadc : lpi->golden_adc;
		lpi->als_gadc = lpi->golden_adc;
	} else {
		lpi->als_kadc = 1;
		lpi->als_gadc = 1;
	}
	dev_dbg(&lpi->i2c_client->dev, "%s: als_kadc=0x%x, als_gadc=0x%x\n",
		__func__, lpi->als_kadc, lpi->als_gadc);
}


static int lightsensor_update_table(struct cm36283_info *lpi)
{
	uint32_t tmp_data[10];
	int i;
	for (i = 0; i < 10; i++) {
		tmp_data[i] = (uint32_t)(*(lpi->adc_table + i))
			* lpi->als_kadc / lpi->als_gadc;

		if (tmp_data[i] <= 0xFFFF)
			lpi->cali_table[i] = (uint16_t) tmp_data[i];
		else
			lpi->cali_table[i] = 0xFFFF;

		dev_dbg(&lpi->i2c_client->dev, "%s: Calibrated adc_table: data[%d], %x\n",
				__func__, i, lpi->cali_table[i]);
	}

	return 0;
}


static int lightsensor_enable(struct cm36283_info *lpi)
{
	int ret = -EIO;
	unsigned int delay;

	mutex_lock(&als_enable_mutex);
	ret = control_and_report(lpi, CONTROL_ALS, 1, 0);
	mutex_unlock(&als_enable_mutex);

	delay = atomic_read(&lpi->ls_poll_delay);
	if (lpi->polling)
		schedule_delayed_work(&lpi->ldwork,
				msecs_to_jiffies(delay));

	return ret;
}

static int lightsensor_disable(struct cm36283_info *lpi)
{
	int ret = -EIO;
	mutex_lock(&als_disable_mutex);
	dev_dbg(&lpi->i2c_client->dev, "disable lightsensor\n");

	if (lpi->polling)
		cancel_delayed_work_sync(&lpi->ldwork);

	if ( lpi->als_enable == 0 ) {
		dev_err(&lpi->i2c_client->dev, "already disabled\n");
		ret = 0;
	} else {
		ret = control_and_report(lpi, CONTROL_ALS, 0, 0);
	}
	
	mutex_unlock(&als_disable_mutex);
	return ret;
}

static int lightsensor_open(struct inode *inode, struct file *file)
{
	struct cm36283_info *lpi = lp_info;
	int rc = 0;

	dev_dbg(&lpi->i2c_client->dev, "%s\n", __func__);
	if (lpi->lightsensor_opened) {
		dev_err(&lpi->i2c_client->dev, "%s: already opened\n",
				__func__);
		rc = -EBUSY;
	}
	lpi->lightsensor_opened = 1;
	return rc;
}

static int lightsensor_release(struct inode *inode, struct file *file)
{
	struct cm36283_info *lpi = lp_info;

	dev_dbg(&lpi->i2c_client->dev, "%s\n", __func__);
	lpi->lightsensor_opened = 0;
	return 0;
}

static long lightsensor_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	int rc, val;
	struct cm36283_info *lpi = lp_info;

	switch (cmd) {
	case LIGHTSENSOR_IOCTL_ENABLE:
		if (get_user(val, (unsigned long __user *)arg)) {
			rc = -EFAULT;
			break;
		}
		rc = val ? lightsensor_enable(lpi) : lightsensor_disable(lpi);
		break;
	case LIGHTSENSOR_IOCTL_GET_ENABLED:
		val = lpi->als_enable;
		rc = put_user(val, (unsigned long __user *)arg);
		break;
	default:
		pr_err("[LS][CM36283 error]%s: invalid cmd %d\n",
			__func__, _IOC_NR(cmd));
		rc = -EINVAL;
	}

	return rc;
}

static const struct file_operations lightsensor_fops = {
	.owner = THIS_MODULE,
	.open = lightsensor_open,
	.release = lightsensor_release,
	.unlocked_ioctl = lightsensor_ioctl
};

static ssize_t ps_adc_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{

	uint16_t value;
	int ret;
	struct cm36283_info *lpi = lp_info;
	int intr_val = -1;

	get_ps_adc_value(&value);
	if (gpio_is_valid(lpi->intr_pin))
		intr_val = gpio_get_value(lpi->intr_pin);

	ret = snprintf(buf, PAGE_SIZE, "ADC[0x%04X], ENABLE=%d intr_pin=%d\n",
			value, lpi->ps_enable, intr_val);

	return ret;
}

static int ps_enable_set(struct sensors_classdev *sensors_cdev,
		unsigned int enable)
{
	struct cm36283_info *lpi = container_of(sensors_cdev,
			struct cm36283_info, ps_cdev);
	int ret;

	if (enable)
		ret = psensor_enable(lpi);
	else
		ret = psensor_disable(lpi);

	return ret;
}

static ssize_t ps_enable_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ps_en;
	struct cm36283_info *lpi = lp_info;

	ps_en = -1;
	sscanf(buf, "%d", &ps_en);

	if (ps_en != 0 && ps_en != 1
		&& ps_en != 10 && ps_en != 13 && ps_en != 16)
		return -EINVAL;

	dev_dbg(&lpi->i2c_client->dev, "%s: ps_en=%d\n",
			__func__, ps_en);

	if (ps_en)
		psensor_enable(lpi);
	else
		psensor_disable(lpi);

	return count;
}


static ssize_t ps_parameters_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int ret;
	struct cm36283_info *lpi = lp_info;

	ret = snprintf(buf, PAGE_SIZE,
			"PS_close_thd_set = 0x%x, PS_away_thd_set = 0x%x\n",
			lpi->ps_close_thd_set, lpi->ps_away_thd_set);

	return ret;
}

static ssize_t ps_parameters_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{

	struct cm36283_info *lpi = lp_info;
	char *token[10];
	int i;
	unsigned long tmp;

	for (i = 0; i < 3; i++)
		token[i] = strsep((char **)&buf, " ");

	if (kstrtoul(token[0], 16, &tmp))
		return -EINVAL;
	lpi->ps_close_thd_set = tmp;

	if (kstrtoul(token[1], 16, &tmp))
		return -EINVAL;
	lpi->ps_away_thd_set = tmp;

	dev_dbg(&lpi->i2c_client->dev, "ps_close_thd_set:0x%x\n",
			lpi->ps_close_thd_set);
	dev_dbg(&lpi->i2c_client->dev, "ps_away_thd_set:0x%x\n",
			lpi->ps_away_thd_set);

	return count;
}

static ssize_t ps_conf_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct cm36283_info *lpi = lp_info;
	return sprintf(buf, "PS_CONF1 = 0x%x, PS_CONF3 = 0x%x\n", lpi->ps_conf1_val, lpi->ps_conf3_val);
}
static ssize_t ps_conf_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int code1, code2;
	struct cm36283_info *lpi = lp_info;

	sscanf(buf, "0x%x 0x%x", &code1, &code2);
	dev_dbg(&lpi->i2c_client->dev, "PS_CONF1:0x%x PS_CONF3:0x%x\n",
			code1, code2);

	lpi->ps_conf1_val = code1;
	lpi->ps_conf3_val = code2;

	_cm36283_I2C_Write_Word(lpi->slave_addr, PS_CONF3, lpi->ps_conf3_val);
	_cm36283_I2C_Write_Word(lpi->slave_addr, PS_CONF1, lpi->ps_conf1_val);

	return count;
}

static ssize_t ps_thd_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int ret;
	struct cm36283_info *lpi = lp_info;
  ret = sprintf(buf, "%s ps_close_thd_set = 0x%x, ps_away_thd_set = 0x%x\n", __func__, lpi->ps_close_thd_set, lpi->ps_away_thd_set);
  return ret;	
}
static ssize_t ps_thd_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int code;
	struct cm36283_info *lpi = lp_info;

	sscanf(buf, "0x%x", &code);

	lpi->ps_away_thd_set = code &0xFF;
	lpi->ps_close_thd_set = (code & 0xFF00)>>8;

	dev_dbg(&lpi->i2c_client->dev, "ps_away_thd_set:0x%x\n",
			lpi->ps_away_thd_set);
	dev_dbg(&lpi->i2c_client->dev, "ps_close_thd_set:0x%x\n",
			lpi->ps_close_thd_set);

	return count;
}

static ssize_t ps_hw_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int ret = 0;
	struct cm36283_info *lpi = lp_info;

	ret = sprintf(buf, "PS1: reg = 0x%x, PS3: reg = 0x%x, ps_close_thd_set = 0x%x, ps_away_thd_set = 0x%x\n",
		lpi->ps_conf1_val, lpi->ps_conf3_val, lpi->ps_close_thd_set, lpi->ps_away_thd_set);

	return ret;
}
static ssize_t ps_hw_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int code;

	sscanf(buf, "0x%x", &code);

	return count;
}

static ssize_t ls_adc_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	int ret;
	struct cm36283_info *lpi = lp_info;

	ret = sprintf(buf, "ADC[0x%04X] => level %d\n",
		lpi->current_adc, lpi->current_level);

	return ret;
}

static int ls_enable_set(struct sensors_classdev *sensors_cdev,
		unsigned int enable)
{
	struct cm36283_info *lpi = container_of(sensors_cdev,
			struct cm36283_info, als_cdev);
	int ret;

	if (enable)
		ret = lightsensor_enable(lpi);
	else
		ret = lightsensor_disable(lpi);

	if (ret < 0) {
		dev_err(&lpi->i2c_client->dev, "%s: set auto light sensor fail\n",
				__func__);
		return -EIO;
	}

	return 0;
}

static ssize_t ls_enable_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{

	int ret = 0;
	struct cm36283_info *lpi = lp_info;

	ret = sprintf(buf, "Light sensor Auto Enable = %d\n",
			lpi->als_enable);

	return ret;
}

static ssize_t ls_enable_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	int ret = 0;
	int ls_auto;
	struct cm36283_info *lpi = lp_info;

	ls_auto = -1;
	sscanf(buf, "%d", &ls_auto);

	if (ls_auto != 0 && ls_auto != 1 && ls_auto != 147)
		return -EINVAL;

	if (ls_auto) {
		lpi->ls_calibrate = (ls_auto == 147) ? 1 : 0;
		ret = lightsensor_enable(lpi);
	} else {
		lpi->ls_calibrate = 0;
		ret = lightsensor_disable(lpi);
	}

	dev_dbg(&lpi->i2c_client->dev, "als_enable:0x%x\n",
			lpi->als_enable);
	dev_dbg(&lpi->i2c_client->dev, "ls_calibrate:0x%x\n",
			lpi->ls_calibrate);
	dev_dbg(&lpi->i2c_client->dev, "ls_auto:0x%x\n", ls_auto);

	if (ret < 0) {
		dev_err(&lpi->i2c_client->dev, "%s: set auto light sensor fail\n",
		__func__);
		return ret;
	}

	return count;
}


static ssize_t ls_kadc_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct cm36283_info *lpi = lp_info;
	int ret;

	ret = sprintf(buf, "kadc = 0x%x",
			lpi->als_kadc);

	return ret;
}

static ssize_t ls_kadc_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct cm36283_info *lpi = lp_info;
	int kadc_temp = 0;

	sscanf(buf, "%d", &kadc_temp);

	mutex_lock(&als_get_adc_mutex);
	if (kadc_temp != 0) {
		lpi->als_kadc = kadc_temp;
		if (lpi->als_gadc != 0) {
			if (lightsensor_update_table(lpi) < 0)
				dev_err(&lpi->i2c_client->dev, "%s: update ls table fail\n",
						__func__);
			else
				dev_dbg(&lpi->i2c_client->dev, "%s: als_gadc =0x%x wait to be set\n",
						__func__, lpi->als_gadc);
		}
	} else {
		dev_err(&lpi->i2c_client->dev, "%s: als_kadc can't be set to zero\n",
				__func__);
	}
				
	mutex_unlock(&als_get_adc_mutex);
	return count;
}


static ssize_t ls_gadc_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct cm36283_info *lpi = lp_info;
	int ret;

	ret = sprintf(buf, "gadc = 0x%x\n", lpi->als_gadc);

	return ret;
}

static ssize_t ls_gadc_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct cm36283_info *lpi = lp_info;
	int gadc_temp = 0;

	sscanf(buf, "%d", &gadc_temp);
	
	mutex_lock(&als_get_adc_mutex);
	if (gadc_temp != 0) {
		lpi->als_gadc = gadc_temp;
		if (lpi->als_kadc != 0) {
			if (lightsensor_update_table(lpi) < 0)
				dev_err(&lpi->i2c_client->dev, "%s: update ls table fail\n",
						__func__);
		} else {
			dev_dbg(&lpi->i2c_client->dev, "als_kadc =0x%x wait to be set\n",
					lpi->als_kadc);
		}
	} else {
		dev_err(&lpi->i2c_client->dev, "als_gadc can't be set to zero\n");
	}
	mutex_unlock(&als_get_adc_mutex);
	return count;
}


static ssize_t ls_adc_table_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	unsigned length = 0;
	int i;

	for (i = 0; i < 10; i++) {
		length += sprintf(buf + length,
			"[CM36283]Get adc_table[%d] =  0x%x ; %d, Get cali_table[%d] =  0x%x ; %d, \n",
			i, *(lp_info->adc_table + i),
			*(lp_info->adc_table + i),
			i, *(lp_info->cali_table + i),
			*(lp_info->cali_table + i));
	}
	return length;
}

static ssize_t ls_adc_table_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{

	struct cm36283_info *lpi = lp_info;
	char *token[10];
	uint16_t tempdata[10];
	int i;

	for (i = 0; i < 10; i++) {
		token[i] = strsep((char **)&buf, " ");
		tempdata[i] = simple_strtoul(token[i], NULL, 16);
		if (tempdata[i] < 1 || tempdata[i] > 0xffff) {
			dev_err(&lpi->i2c_client->dev,
			"adc_table[%d] =  0x%x error\n",
			i, tempdata[i]);
			return count;
		}
	}
	mutex_lock(&als_get_adc_mutex);
	for (i = 0; i < 10; i++)
		lpi->adc_table[i] = tempdata[i];

	if (lightsensor_update_table(lpi) < 0)
		dev_err(&lpi->i2c_client->dev, "%s: update ls table fail\n",
		__func__);
	mutex_unlock(&als_get_adc_mutex);

	return count;
}

static ssize_t ls_conf_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct cm36283_info *lpi = lp_info;
	return sprintf(buf, "ALS_CONF = %x\n", lpi->ls_cmd);
}
static ssize_t ls_conf_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct cm36283_info *lpi = lp_info;
	int value = 0;
	sscanf(buf, "0x%x", &value);

	lpi->ls_cmd = value;

	dev_dbg(&lpi->i2c_client->dev, "ALS_CONF:0x%x\n", lpi->ls_cmd);

	_cm36283_I2C_Write_Word(lpi->slave_addr, ALS_CONF, lpi->ls_cmd);
	return count;
}

static ssize_t ls_poll_delay_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cm36283_info *lpi = lp_info;
	return snprintf(buf, PAGE_SIZE, "%d\n",
			atomic_read(&lpi->ls_poll_delay));
}

static ssize_t ls_poll_delay_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct cm36283_info *lpi = lp_info;
	unsigned long interval_ms;

	if (kstrtoul(buf, 10, &interval_ms))
		return -EINVAL;

	if ((interval_ms < CM36283_LS_MIN_POLL_DELAY) ||
			(interval_ms > CM36283_LS_MAX_POLL_DELAY))
		return -EINVAL;

	atomic_set(&lpi->ls_poll_delay, (unsigned int) interval_ms);
	return count;
}

static int ls_poll_delay_set(struct sensors_classdev *sensors_cdev,
		unsigned int delay_msec)
{
	struct cm36283_info *lpi = container_of(sensors_cdev,
			struct cm36283_info, als_cdev);

	if ((delay_msec < CM36283_LS_MIN_POLL_DELAY) ||
			(delay_msec > CM36283_LS_MAX_POLL_DELAY))
		return -EINVAL;

	atomic_set(&lpi->ls_poll_delay, delay_msec);

	return 0;
}

static ssize_t ps_poll_delay_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct cm36283_info *lpi = lp_info;
	return snprintf(buf, PAGE_SIZE, "%d\n",
			atomic_read(&lpi->ps_poll_delay));
}

static ssize_t ps_poll_delay_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct cm36283_info *lpi = lp_info;
	unsigned long interval_ms;

	if (kstrtoul(buf, 10, &interval_ms))
		return -EINVAL;

	if ((interval_ms < CM36283_PS_MIN_POLL_DELAY) ||
			(interval_ms > CM36283_PS_MAX_POLL_DELAY))
		return -EINVAL;

	atomic_set(&lpi->ps_poll_delay, (unsigned int) interval_ms);
	return count;
}

static int ps_poll_delay_set(struct sensors_classdev *sensors_cdev,
		unsigned int delay_msec)
{
	struct cm36283_info *lpi = container_of(sensors_cdev,
			struct cm36283_info, als_cdev);

	if ((delay_msec < CM36283_PS_MIN_POLL_DELAY) ||
			(delay_msec > CM36283_PS_MAX_POLL_DELAY))
		return -EINVAL;

	atomic_set(&lpi->ps_poll_delay, delay_msec);
	return 0;
}

static ssize_t ls_fLevel_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "fLevel = %d\n", fLevel);
}
static ssize_t ls_fLevel_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct cm36283_info *lpi = lp_info;
	int value=0;
	sscanf(buf, "%d", &value);
	(value>=0)?(value=min(value,10)):(value=max(value,-1));
	fLevel=value;
	input_report_abs(lpi->ls_input_dev, ABS_MISC, fLevel);
	input_sync(lpi->ls_input_dev);

	msleep(1000);
	fLevel=-1;
	return count;
}

static int lightsensor_setup(struct cm36283_info *lpi)
{
	int ret;
	int range;

	lpi->ls_input_dev = input_allocate_device();
	if (!lpi->ls_input_dev) {
		pr_err(
			"[LS][CM36283 error]%s: could not allocate ls input device\n",
			__func__);
		return -ENOMEM;
	}
	lpi->ls_input_dev->name = "cm36283-ls";
	lpi->ls_input_dev->id.bustype = BUS_I2C;
	set_bit(EV_ABS, lpi->ls_input_dev->evbit);

	range = get_als_range();
	input_set_abs_params(lpi->ls_input_dev, ABS_MISC, 0, range, 0, 0);

	ret = input_register_device(lpi->ls_input_dev);
	if (ret < 0) {
		pr_err("[LS][CM36283 error]%s: can not register ls input device\n",
				__func__);
		goto err_free_ls_input_device;
	}

	return ret;

err_free_ls_input_device:
	input_free_device(lpi->ls_input_dev);
	return ret;
}

static int psensor_setup(struct cm36283_info *lpi)
{
	int ret;

	lpi->ps_input_dev = input_allocate_device();
	if (!lpi->ps_input_dev) {
		pr_err(
			"[PS][CM36283 error]%s: could not allocate ps input device\n",
			__func__);
		return -ENOMEM;
	}
	lpi->ps_input_dev->name = "cm36283-ps";
	lpi->ps_input_dev->id.bustype = BUS_I2C;
	set_bit(EV_ABS, lpi->ps_input_dev->evbit);
	input_set_abs_params(lpi->ps_input_dev, ABS_DISTANCE, 0, 1, 0, 0);

	ret = input_register_device(lpi->ps_input_dev);
	if (ret < 0) {
		pr_err(
			"[PS][CM36283 error]%s: could not register ps input device\n",
			__func__);
		goto err_free_ps_input_device;
	}

	return ret;

err_free_ps_input_device:
	input_free_device(lpi->ps_input_dev);
	return ret;
}


static int initial_cm36283(struct cm36283_info *lpi)
{
	int val, ret;
	uint16_t idReg;

	val = gpio_get_value(lpi->intr_pin);
	dev_dbg(&lpi->i2c_client->dev, "%s, INTERRUPT GPIO val = %d\n",
		       __func__, val);

	ret = _cm36283_I2C_Read_Word(lpi->slave_addr, ID_REG, &idReg);

	return ret;
}

static int cm36283_setup(struct cm36283_info *lpi)
{
	int ret = 0;

	als_power(1);
	msleep(5);
	ret = gpio_request(lpi->intr_pin, "gpio_cm36283_intr");
	if (ret < 0) {
		pr_err("[PS][CM36283 error]%s: gpio %d request failed (%d)\n",
			__func__, lpi->intr_pin, ret);
		return ret;
	}

	ret = gpio_direction_input(lpi->intr_pin);
	if (ret < 0) {
		pr_err(
			"[PS][CM36283 error]%s: fail to set gpio %d as input (%d)\n",
			__func__, lpi->intr_pin, ret);
		goto fail_free_intr_pin;
	}


	ret = initial_cm36283(lpi);
	if (ret < 0) {
		pr_err(
			"[PS_ERR][CM36283 error]%s: fail to initial cm36283 (%d)\n",
			__func__, ret);
		goto fail_free_intr_pin;
	}
	
	/*Default disable P sensor and L sensor*/
	ls_initial_cmd(lpi);
	psensor_initial_cmd(lpi);

	if (!lpi->polling)
		ret = request_any_context_irq(lpi->irq,
				cm36283_irq_handler,
				IRQF_TRIGGER_LOW,
				"cm36283",
				lpi);
	if (ret < 0) {
		pr_err(
			"[PS][CM36283 error]%s: req_irq(%d) fail for gpio %d (%d)\n",
			__func__, lpi->irq,
			lpi->intr_pin, ret);
		goto fail_free_intr_pin;
	}

	return ret;

fail_free_intr_pin:
	gpio_free(lpi->intr_pin);
	return ret;
}

static int cm36283_parse_dt(struct device *dev,
				struct cm36283_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	u32	levels[CM36283_LEVELS_SIZE], i;
	u32 temp_val;
	int rc;

	rc = of_get_named_gpio_flags(np, "capella,interrupt-gpio",
			0, NULL);
	if (rc < 0) {
		dev_err(dev, "Unable to read interrupt pin number\n");
		return rc;
	} else {
		pdata->intr = rc;
	}

	rc = of_property_read_u32_array(np, "capella,levels", levels,
			CM36283_LEVELS_SIZE);
	if (rc) {
		dev_err(dev, "Unable to read levels data\n");
		return rc;
	} else {
		for (i = 0; i < CM36283_LEVELS_SIZE; i++)
			pdata->levels[i] = levels[i];
	}

	rc = of_property_read_u32(np, "capella,ps_close_thd_set", &temp_val);
	if (rc) {
		dev_err(dev, "Unable to read ps_close_thd_set\n");
		return rc;
	} else {
		pdata->ps_close_thd_set = (u8)temp_val;
	}

	rc = of_property_read_u32(np, "capella,ps_away_thd_set", &temp_val);
	if (rc) {
		dev_err(dev, "Unable to read ps_away_thd_set\n");
		return rc;
	} else {
		pdata->ps_away_thd_set = (u8)temp_val;
	}

	rc = of_property_read_u32(np, "capella,ls_cmd", &temp_val);
	if (rc) {
		dev_err(dev, "Unable to read ls_cmd\n");
		return rc;
	} else {
		pdata->ls_cmd = (u16)temp_val;
	}

	rc = of_property_read_u32(np, "capella,ps_conf1_val", &temp_val);
	if (rc) {
		dev_err(dev, "Unable to read ps_conf1_val\n");
		return rc;
	} else {
		pdata->ps_conf1_val = (u16)temp_val;
	}

	rc = of_property_read_u32(np, "capella,ps_conf3_val", &temp_val);
	if (rc) {
		dev_err(dev, "Unable to read ps_conf3_val\n");
		return rc;
	} else {
		pdata->ps_conf3_val = (u16)temp_val;
	}

	pdata->polling = of_property_read_bool(np, "capella,use-polling");

	return 0;
}

static int create_sysfs_interfaces(struct device *dev,
		struct device_attribute *attributes, int len)
{
	int i;
	int err;
	for (i = 0; i < len; i++) {
		err = device_create_file(dev, attributes + i);
		if (err)
			goto error;
	}
	return 0;

error:
	for (; i >= 0; i--)
		device_remove_file(dev, attributes + i);
	dev_err(dev, "%s:Unable to create interface\n", __func__);
	return err;
}

static int remove_sysfs_interfaces(struct device *dev,
		struct device_attribute *attributes, int len)
{
	int i;
	for (i = 0; i < len; i++)
		device_remove_file(dev, attributes + i);
	return 0;
}

static struct device_attribute light_attr[] = {
	__ATTR(ls_adc, 0444, ls_adc_show, NULL),
	__ATTR(ls_kadc, 0664, ls_kadc_show, ls_kadc_store),
	__ATTR(ls_gadc, 0664, ls_gadc_show, ls_gadc_store),
	__ATTR(ls_conf, 0664, ls_conf_show, ls_conf_store),
	__ATTR(ls_adc_table, 0664,
			ls_adc_table_show, ls_adc_table_store),
	__ATTR(poll_delay, 0664, ls_poll_delay_show,
			ls_poll_delay_store),
	__ATTR(enable, 0664,
			ls_enable_show, ls_enable_store),
};

static struct device_attribute proximity_attr[] = {
	__ATTR(enable, 0664, ps_adc_show, ps_enable_store),
	__ATTR(ps_parameters, 0664,
			ps_parameters_show, ps_parameters_store),
	__ATTR(ps_conf, 0664, ps_conf_show, ps_conf_store),
	__ATTR(ps_hw, 0664, ps_hw_show, ps_hw_store),
	__ATTR(ps_thd, 0664, ps_thd_show, ps_thd_store),
	__ATTR(poll_delay, 0664, ps_poll_delay_show,
			ps_poll_delay_store),
	__ATTR(ls_flevel, 0664, ls_fLevel_show, ls_fLevel_store),
};

static int cm36283_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret = 0;
	struct cm36283_info *lpi;
	struct cm36283_platform_data *pdata;

	lpi = kzalloc(sizeof(struct cm36283_info), GFP_KERNEL);
	if (!lpi)
		return -ENOMEM;

	lpi->i2c_client = client;

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allocate memory for pdata\n");
			ret = -ENOMEM;
			goto err_platform_data_null;
		}

		ret = cm36283_parse_dt(&client->dev, pdata);
		pdata->slave_addr = client->addr;
		if (ret) {
			dev_err(&client->dev, "Failed to get pdata from device tree\n");
			goto err_parse_dt;
		}
	} else {
		pdata = client->dev.platform_data;
		if (!pdata) {
			dev_err(&client->dev, "%s: Assign platform_data error!!\n",
					__func__);
			ret = -EBUSY;
			goto err_platform_data_null;
		}
	}

	lpi->irq = client->irq;

	i2c_set_clientdata(client, lpi);
	
  lpi->intr_pin = pdata->intr;
	lpi->adc_table = pdata->levels;
	lpi->power = pdata->power;
	
	lpi->slave_addr = pdata->slave_addr;
	
	lpi->ps_away_thd_set = pdata->ps_away_thd_set;
	lpi->ps_close_thd_set = pdata->ps_close_thd_set;	
	lpi->ps_conf1_val = pdata->ps_conf1_val;
	lpi->ps_conf3_val = pdata->ps_conf3_val;
	lpi->polling = pdata->polling;
	atomic_set(&lpi->ls_poll_delay,
			(unsigned int) CM36283_LS_DEFAULT_POLL_DELAY);
	atomic_set(&lpi->ps_poll_delay,
			(unsigned int) CM36283_PS_DEFAULT_POLL_DELAY);

	
	lpi->ls_cmd  = pdata->ls_cmd;
	
	lpi->record_clear_int_fail=0;
	
	dev_dbg(&lpi->i2c_client->dev, "[PS][CM36283] %s: ls_cmd 0x%x\n",
		__func__, lpi->ls_cmd);
	
	if (pdata->ls_cmd == 0) {
		lpi->ls_cmd  = CM36283_ALS_IT_80ms | CM36283_ALS_GAIN_2;
	}

	lp_info = lpi;

	mutex_init(&CM36283_control_mutex);

	mutex_init(&als_enable_mutex);
	mutex_init(&als_disable_mutex);
	mutex_init(&als_get_adc_mutex);


	mutex_init(&ps_enable_mutex);
	mutex_init(&ps_disable_mutex);
	mutex_init(&ps_get_adc_mutex);


	/*
	 * SET LUX STEP FACTOR HERE
	 * if adc raw value one step = 5/100 = 1/20 = 0.05 lux
	 * the following will set the factor 0.05 = 1/20
	 * and lpi->golden_adc = 1;
	 * set als_kadc = (ALS_CALIBRATED << 16) | 20;
	 */

	als_kadc = (ALS_CALIBRATED << 16) | 10;
	lpi->golden_adc = 100;
	lpi->ls_calibrate = 0;

	lightsensor_set_kvalue(lpi);
	ret = lightsensor_update_table(lpi);
	if (ret < 0) {
		pr_err("[LS][CM36283 error]%s: update ls table fail\n",
			__func__);
		goto err_lightsensor_update_table;
	}

	lpi->lp_wq = create_singlethread_workqueue("cm36283_wq");
	if (!lpi->lp_wq) {
		pr_err("[PS][CM36283 error]%s: can't create workqueue\n", __func__);
		ret = -ENOMEM;
		goto err_create_singlethread_workqueue;
	}
	wake_lock_init(&(lpi->ps_wake_lock), WAKE_LOCK_SUSPEND, "proximity");

	ret = cm36283_power_set(lpi, true);
	if (ret < 0) {
		dev_err(&client->dev, "%s:cm36283 power on error!\n", __func__);
		goto err_cm36283_power_on;
	}

	ret = cm36283_setup(lpi);
	if (ret < 0) {
		pr_err("[PS_ERR][CM36283 error]%s: cm36283_setup error!\n", __func__);
		goto err_cm36283_setup;
	}

	ret = lightsensor_setup(lpi);
	if (ret < 0) {
		pr_err("[LS][CM36283 error]%s: lightsensor_setup error!!\n",
			__func__);
		goto err_lightsensor_setup;
	}

	ret = psensor_setup(lpi);
	if (ret < 0) {
		pr_err("[PS][CM36283 error]%s: psensor_setup error!!\n",
			__func__);
		goto err_psensor_setup;
	}

	ret = create_sysfs_interfaces(&lpi->ls_input_dev->dev, light_attr,
			ARRAY_SIZE(light_attr));
	if (ret < 0) {
		dev_err(&client->dev, "failed to create sysfs\n");
		goto err_input_cleanup;
	}

	ret = create_sysfs_interfaces(&lpi->ps_input_dev->dev, proximity_attr,
			ARRAY_SIZE(proximity_attr));
	if (ret < 0) {
		dev_err(&client->dev, "failed to create sysfs\n");
		goto err_light_sysfs_cleanup;
	}

	lpi->als_cdev = sensors_light_cdev;
	lpi->als_cdev.sensors_enable = ls_enable_set;
	lpi->als_cdev.sensors_poll_delay = ls_poll_delay_set;
	lpi->als_cdev.min_delay = CM36283_LS_MIN_POLL_DELAY * 1000;

	lpi->ps_cdev = sensors_proximity_cdev;
	lpi->ps_cdev.sensors_enable = ps_enable_set;
	lpi->ps_cdev.sensors_poll_delay = ps_poll_delay_set;
	lpi->ps_cdev.min_delay = CM36283_PS_MIN_POLL_DELAY * 1000;

	ret = sensors_classdev_register(&client->dev, &lpi->als_cdev);
	if (ret)
		goto err_proximity_sysfs_cleanup;

	ret = sensors_classdev_register(&client->dev, &lpi->ps_cdev);
	if (ret)
		goto err_create_class_sysfs;

	mutex_init(&wq_lock);
	INIT_DELAYED_WORK(&lpi->ldwork, lsensor_delay_work_handler);
	INIT_DELAYED_WORK(&lpi->pdwork, psensor_delay_work_handler);
	dev_dbg(&lpi->i2c_client->dev, "%s: Probe success!\n", __func__);

	return ret;
err_create_class_sysfs:
	sensors_classdev_unregister(&lpi->als_cdev);
err_proximity_sysfs_cleanup:
	remove_sysfs_interfaces(&lpi->ps_input_dev->dev, proximity_attr,
			ARRAY_SIZE(proximity_attr));
err_light_sysfs_cleanup:
	remove_sysfs_interfaces(&lpi->ls_input_dev->dev, light_attr,
			ARRAY_SIZE(light_attr));
err_input_cleanup:
	input_unregister_device(lpi->ps_input_dev);
	input_free_device(lpi->ps_input_dev);
err_psensor_setup:
	input_unregister_device(lpi->ls_input_dev);
	input_free_device(lpi->ls_input_dev);
err_lightsensor_setup:
err_cm36283_setup:
	cm36283_power_set(lpi, false);
err_cm36283_power_on:
	wake_lock_destroy(&(lpi->ps_wake_lock));
	destroy_workqueue(lpi->lp_wq);
err_create_singlethread_workqueue:
err_lightsensor_update_table:
	mutex_destroy(&CM36283_control_mutex);
	mutex_destroy(&als_enable_mutex);
	mutex_destroy(&als_disable_mutex);
	mutex_destroy(&als_get_adc_mutex);
	mutex_destroy(&ps_enable_mutex);
	mutex_destroy(&ps_disable_mutex);
	mutex_destroy(&ps_get_adc_mutex);
err_parse_dt:
	if (client->dev.of_node && (pdata != NULL))
		devm_kfree(&client->dev, pdata);
err_platform_data_null:
	kfree(lpi);
	dev_err(&client->dev, "%s:error exit! ret = %d\n", __func__, ret);

	return ret;
}

static int control_and_report(struct cm36283_info *lpi, uint8_t mode,
	uint16_t param, int report)
{
	int ret = 0;
	uint16_t adc_value = 0;
	uint16_t ps_data = 0;
	int level = 0, i, val;
	
  mutex_lock(&CM36283_control_mutex);

  if( mode == CONTROL_ALS ){
    if(param){
      lpi->ls_cmd &= CM36283_ALS_SD_MASK;      
    } else {
      lpi->ls_cmd |= CM36283_ALS_SD;
    }
    _cm36283_I2C_Write_Word(lpi->slave_addr, ALS_CONF, lpi->ls_cmd);
    lpi->als_enable=param;
  } else if( mode == CONTROL_PS ){
    if(param){ 
      lpi->ps_conf1_val &= CM36283_PS_SD_MASK;
      lpi->ps_conf1_val |= CM36283_PS_INT_IN_AND_OUT;      
    } else {
      lpi->ps_conf1_val |= CM36283_PS_SD;
      lpi->ps_conf1_val &= CM36283_PS_INT_MASK;
    }
    _cm36283_I2C_Write_Word(lpi->slave_addr, PS_CONF1, lpi->ps_conf1_val);    
    lpi->ps_enable=param;  
  }
  if((mode == CONTROL_ALS)||(mode == CONTROL_PS)){  
    if( param==1 ){
		  msleep(100);  
    }
  }
     	
  if(lpi->als_enable){
    if( mode == CONTROL_ALS ||
      ( mode == CONTROL_INT_ISR_REPORT && 
      ((param&INT_FLAG_ALS_IF_L)||(param&INT_FLAG_ALS_IF_H)))){
    
    	  lpi->ls_cmd &= CM36283_ALS_INT_MASK;
    	  ret = _cm36283_I2C_Write_Word(lpi->slave_addr, ALS_CONF, lpi->ls_cmd);  
      
        get_ls_adc_value(&adc_value, 0);
          
        if( lpi->ls_calibrate ) {
        	for (i = 0; i < 10; i++) {
      	  	if (adc_value <= (*(lpi->cali_table + i))) {
      		  	level = i;
      			  if (*(lpi->cali_table + i))
      				  break;
      		  }
      		  if ( i == 9) {/*avoid  i = 10, because 'cali_table' of size is 10 */
      			  level = i;
      			  break;
      		  }
      	  }
        } else {
      	  for (i = 0; i < 10; i++) {
      		  if (adc_value <= (*(lpi->adc_table + i))) {
      			  level = i;
      			  if (*(lpi->adc_table + i))
      				  break;
      		  }
      		  if ( i == 9) {/*avoid  i = 10, because 'cali_table' of size is 10 */
      			  level = i;
      			  break;
      		  }
      	  }
    	  }
	if (!lpi->polling) {
		ret = set_lsensor_range(((i == 0) ||
					(adc_value == 0)) ? 0 :
				*(lpi->cali_table + (i - 1)) + 1,
				*(lpi->cali_table + i));

		lpi->ls_cmd |= CM36283_ALS_INT_EN;
	}

	ret = _cm36283_I2C_Write_Word(lpi->slave_addr, ALS_CONF,
			lpi->ls_cmd);

	if (report) {
		lpi->current_level = level;
		lpi->current_adc = adc_value;
		input_report_abs(lpi->ls_input_dev, ABS_MISC, level);
		input_sync(lpi->ls_input_dev);
	}
    }
  }

#define PS_CLOSE 1
#define PS_AWAY  (1<<1)
#define PS_CLOSE_AND_AWAY PS_CLOSE+PS_AWAY
	if (report && (lpi->ps_enable)) {
		int ps_status = 0;
		if (mode == CONTROL_PS)
			ps_status = PS_CLOSE_AND_AWAY;
		else if (mode == CONTROL_INT_ISR_REPORT) {
			if (param & INT_FLAG_PS_IF_CLOSE)
				ps_status |= PS_CLOSE;
			if (param & INT_FLAG_PS_IF_AWAY)
				ps_status |= PS_AWAY;
		}

		if (ps_status != 0) {
			switch (ps_status) {
			case PS_CLOSE_AND_AWAY:
				get_stable_ps_adc_value(&ps_data);
				val = (ps_data >= lpi->ps_close_thd_set)
					? 0 : 1;
				break;
			case PS_AWAY:
				val = 1;
				break;
			case PS_CLOSE:
				val = 0;
				break;
			};
			input_report_abs(lpi->ps_input_dev, ABS_DISTANCE, val);
			input_sync(lpi->ps_input_dev);
		}
	}

	mutex_unlock(&CM36283_control_mutex);
	return ret;
}

static int cm36283_power_set(struct cm36283_info *info, bool on)
{
	int rc;

	if (on) {
		info->vdd = regulator_get(&info->i2c_client->dev, "vdd");
		if (IS_ERR(info->vdd)) {
			rc = PTR_ERR(info->vdd);
			dev_err(&info->i2c_client->dev,
				"Regulator get failed vdd rc=%d\n", rc);
			goto err_vdd_get;
		}

		if (regulator_count_voltages(info->vdd) > 0) {
			rc = regulator_set_voltage(info->vdd,
					CM36283_VDD_MIN_UV, CM36283_VDD_MAX_UV);
			if (rc) {
				dev_err(&info->i2c_client->dev,
					"Regulator set failed vdd rc=%d\n", rc);
				goto err_vdd_set_vtg;
			}
		}

		info->vio = regulator_get(&info->i2c_client->dev, "vio");
		if (IS_ERR(info->vio)) {
			rc = PTR_ERR(info->vio);
			dev_err(&info->i2c_client->dev,
				"Regulator get failed vio rc=%d\n", rc);
			goto err_vio_get;
		}

		if (regulator_count_voltages(info->vio) > 0) {
			rc = regulator_set_voltage(info->vio,
				CM36283_VI2C_MIN_UV, CM36283_VI2C_MAX_UV);
			if (rc) {
				dev_err(&info->i2c_client->dev,
				"Regulator set failed vio rc=%d\n", rc);
				goto err_vio_set_vtg;
			}
		}

		rc = regulator_enable(info->vdd);
		if (rc) {
			dev_err(&info->i2c_client->dev,
				"Regulator vdd enable failed rc=%d\n", rc);
			goto err_vdd_ena;
		}

		rc = regulator_enable(info->vio);
		if (rc) {
			dev_err(&info->i2c_client->dev,
				"Regulator vio enable failed rc=%d\n", rc);
			goto err_vio_ena;
		}

	} else {
		rc = regulator_disable(info->vdd);
		if (rc) {
			dev_err(&info->i2c_client->dev,
				"Regulator vdd disable failed rc=%d\n", rc);
			return rc;
		}
		if (regulator_count_voltages(info->vdd) > 0)
			regulator_set_voltage(info->vdd, 0, CM36283_VDD_MAX_UV);

		regulator_put(info->vdd);

		rc = regulator_disable(info->vio);
		if (rc) {
			dev_err(&info->i2c_client->dev,
				"Regulator vio disable failed rc=%d\n", rc);
			return rc;
		}
		if (regulator_count_voltages(info->vio) > 0)
			regulator_set_voltage(info->vio, 0,
					CM36283_VI2C_MAX_UV);

		regulator_put(info->vio);
	}

	return 0;

err_vio_ena:
	regulator_disable(info->vdd);
err_vdd_ena:
	if (regulator_count_voltages(info->vio) > 0)
		regulator_set_voltage(info->vio, 0, CM36283_VI2C_MAX_UV);
err_vio_set_vtg:
	regulator_put(info->vio);
err_vio_get:
	if (regulator_count_voltages(info->vdd) > 0)
		regulator_set_voltage(info->vdd, 0, CM36283_VDD_MAX_UV);
err_vdd_set_vtg:
	regulator_put(info->vdd);
err_vdd_get:
	return rc;
}

#ifdef CONFIG_PM_SLEEP
static int cm36283_suspend(struct device *dev)
{
	struct cm36283_info *lpi = lp_info;

	if (lpi->als_enable) {
		if (lightsensor_disable(lpi))
			goto out;
		lpi->als_enable = 1;
	}
	if (cm36283_power_set(lpi, 0))
		goto out;

	return 0;

out:
	dev_err(&lpi->i2c_client->dev, "%s:failed during resume operation.\n",
			__func__);
	return -EIO;
}

static int cm36283_resume(struct device *dev)
{
	struct cm36283_info *lpi = lp_info;

	if (cm36283_power_set(lpi, 1))
		goto out;

	if (lpi->als_enable) {
		ls_initial_cmd(lpi);
		psensor_initial_cmd(lpi);
		if (lightsensor_enable(lpi))
			goto out;
	}
	return 0;

out:
	dev_err(&lpi->i2c_client->dev, "%s:failed during resume operation.\n",
			__func__);
	return -EIO;
}
#endif

static UNIVERSAL_DEV_PM_OPS(cm36283_pm, cm36283_suspend, cm36283_resume, NULL);

static const struct i2c_device_id cm36283_i2c_id[] = {
	{CM36283_I2C_NAME, 0},
	{}
};

static struct of_device_id cm36283_match_table[] = {
	{ .compatible = "capella,cm36283",},
	{ },
};

static struct i2c_driver cm36283_driver = {
	.id_table = cm36283_i2c_id,
	.probe = cm36283_probe,
	.driver = {
		.name = CM36283_I2C_NAME,
		.owner = THIS_MODULE,
		.pm = &cm36283_pm,
		.of_match_table = cm36283_match_table,
	},
};

static int __init cm36283_init(void)
{
	return i2c_add_driver(&cm36283_driver);
}

static void __exit cm36283_exit(void)
{
	i2c_del_driver(&cm36283_driver);
}

module_init(cm36283_init);
module_exit(cm36283_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("CM36283 Driver");
MODULE_AUTHOR("Frank Hsieh <pengyueh@gmail.com>");
