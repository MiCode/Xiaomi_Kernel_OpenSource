// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/bitops.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/acpi.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_runtime.h>

#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/buffer.h>
#include <linux/iio/trigger.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>

#include <linux/init.h>
#include <linux/time.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/cpumask.h>
#include <linux/iio/consumer.h>

// REGISTER MAP
#define IPD_REG_WIA                 0x00
#define ipd_DEVICE_ID               0x48C1

#define IPD_REG_WORD_ST             0x10
#define IPD_REG_WORD_ST_X           0x11
#define IPD_REG_WORD_ST_Y           0x12
#define IPD_REG_WORD_ST_Y_X         0x13
#define IPD_REG_WORD_ST_Z           0x14
#define IPD_REG_WORD_ST_Z_X         0x15
#define IPD_REG_WORD_ST_Z_Y         0x16
#define IPD_REG_WORD_ST_Z_Y_X       0x17
#define IPD_REG_BYTE_ST_V           0x18
#define IPD_REG_BYTE_ST_X           0x19
#define IPD_REG_BYTE_ST_Y           0x1A
#define IPD_REG_BYTE_ST_Y_X         0x1B
#define IPD_REG_BYTE_ST_Z           0x1C
#define IPD_REG_BYTE_ST_Z_X         0x1D
#define IPD_REG_BYTE_ST_Z_Y         0x1E
#define IPD_REG_BYTE_ST_Z_Y_X       0x1F
#define IPD_REG_CNTL1               0x20
#define IPD_REG_CNTL2               0x21
#define IPD_REG_THX                 0x22
#define IPD_REG_THY                 0x23
#define IPD_REG_THZ                 0x24
#define IPD_REG_THV                 0x25
#define IPD_REG_SRST                0x30

#define ipd_MAX_REGS                IPD_REG_SRST

#define IPD_MEASUREMENT_WAIT_TIME   2
#define IPD_WORD_LEN                2
#define SECOND_VAL_ENABLED          2
#define IPD_STEP_BASE               2
#define IPD_MAX_REG_LEN             7
#define MAX_IPD_RETRIES             1
#define MIN_SLEEP_TIME              100
#define IPD_MIN_STEP                0
#define IPD_MAX_STEP                6
#define MIN_READ_VALUE              32768
#define MAX_READ_VALUE              65536




static int BOPX[] = {400, 400, 400, 400, 400, 400, 0, 0};
static int BRPX[] = {250, 250, 250, 250, 250, 250, 0, 0};
static u16 BOPZ[]  = {0, 65286, 65286, 65286, 150, 400, 400, 400};
static u16 BRPZ[]  = {0, 65136, 65136, 65136, 50, 250, 250, 250};
static u16 CTRL1[] = {0x02, 0x40A, 0x40A, 0x40A, 0x0A, 0x0A, 0x8, 0x8};
static int modeBitTable[] = {0, 0x2, 0x4, 0x6, 0x8, 0xA, 0xC, 0xE, 0x10};

enum {
	ipd_MSRNO_WORD_ST = 0,
	ipd_MSRNO_WORD_ST_X,     // 1
	ipd_MSRNO_WORD_ST_Y,     // 2
	ipd_MSRNO_WORD_ST_Y_X,   // 3
	ipd_MSRNO_WORD_ST_Z,     // 4
	ipd_MSRNO_WORD_ST_Z_X,   // 5
	ipd_MSRNO_WORD_ST_Z_Y,   // 6
	ipd_MSRNO_WORD_ST_Z_Y_X, // 7
	ipd_MSRNO_WORD_ST_V,     // 8
	ipd_MSRNO_BYTE_ST_X,     // 9
	ipd_MSRNO_BYTE_ST_Y,     // 10
	ipd_MSRNO_BYTE_ST_Y_X,   // 11
	ipd_MSRNO_BYTE_ST_Z,     // 12
	ipd_MSRNO_BYTE_ST_Z_X,   // 13
	ipd_MSRNO_BYTE_ST_Z_Y,   // 14
	ipd_MSRNO_BYTE_ST_Z_Y_X, // 15
};

static struct mutex ipd_mutex;

struct ipd_data {
	struct i2c_client   *client;
	struct iio_channel  *adc;
	struct task_struct  *distance_thread;
	struct work_struct  update_registers;
	struct workqueue_struct *ipd_wq;

	int int_gpio;
	int irq;

	int enable;
	int resistance;
	int ipd_distance;
	int near_res;
	int far_res;
	int near_ipd;
	int far_ipd;
	int sleep_time;
	int enable_print;
	int ipd_retry;
	int enable_interrupt;

	u8  mode;
	u8  device_type;
	s16 numMode;

	u8  msrNo;

	u8  DRDYENbit;
	u8  SWXENbit;
	u8  SWYENbit;
	u8  SWZENbit;
	u8  SWVENbit;
	u8  ERRENbit;

	u8  POLXbit;
	u8  POLYbit;
	u8  POLZbit;
	u8  POLVbit;

	u8  SDRbit;
	u8  SMRbit;

	u16 BOPXbits;
	u16 BRPXbits;

	u16 BOPYbits;
	u16 BRPYbits;

	u16 BOPZbits;
	u16 BRPZbits;

	u16 BOPVbits;
	u16 BRPVbits;
};
static int ipd_read_axis(struct ipd_data *ipd, int address, int *val);
static int ipd_read_axis_u(struct ipd_data *ipd, int address, u16 *val);
static void ipd_compute_distance(struct ipd_data *ipd);
static void ipd_read_control_status_register(struct ipd_data *ipd);

static int ipd_i2c_reads(struct i2c_client *client, u8 *reg, int reglen, u8 *rdata, int datalen)
{
	struct i2c_msg xfer[2];
	int ret;

	/* Write register */
	xfer[0].addr = client->addr;
	xfer[0].flags = 0;
	xfer[0].len = reglen;
	xfer[0].buf = reg;

	/* Read data */
	xfer[1].addr = client->addr;
	xfer[1].flags = I2C_M_RD;
	xfer[1].len = datalen;
	xfer[1].buf = rdata;

	ret = i2c_transfer(client->adapter, xfer, 2);

	if (ret == 2)
		return 0;
	else if (ret < 0)
		return ret;
	else
		return -EIO;
}

static int ipd_i2c_read8_16(struct i2c_client *client,
				u8 address, int wordLen, u16 *rdata)
{
	u8  tx[1];
	u8  rx[8];
	int i, ret;

	if ((wordLen < 1) || (wordLen > 4)) {
		dev_err(&client->dev, "[ipd] %s Read Word Length Error %d\n", __func__, wordLen);
		return -EINVAL;
	}

	tx[0] = address;

	ret = ipd_i2c_reads(client, tx, 1, rx, ((2 * wordLen) - 1));
	if (ret < 0) {
		dev_err(&client->dev, "[ipd] I2C Read Error\n");
		for (i = 0 ; i < wordLen ; i++)
			rdata[i] = 0;
	} else {
		rdata[0] = (u16)rx[0];
		for (i = 1 ; i < wordLen ; i++)
			rdata[i] = ((u16)rx[2*i-1] << 8) + (u16)rx[2*i];
	}

	pr_debug("[ipd] %s, addr = %x\n", __func__, address);
	for (i = 0 ; i < ((2 * wordLen) - 1) ; i++)
		pr_debug("%x\n", (int)rx[i]);
	pr_debug("\n");

	return ret;

}

static int ipd_i2c_write(struct i2c_client *client, const u8 *tx, size_t wlen)
{
	int ret;

	ret = i2c_master_send(client, tx, wlen);

	if (ret != wlen)
		pr_err("%s: comm error, ret %d, wlen %d\n", __func__, ret, (int)wlen);

	return ret;
}

static s32 ipd_i2c_write16(struct i2c_client *client,
				u8 address, int valueNum, u16 value1, u16 value2)
{
	u8  tx[5];
	s32 ret;
	int n;

	if ((valueNum != 1) &&  (valueNum != 2)) {
		pr_err("%s: valueNum error, valueNum= %d\n", __func__, valueNum);
		return -EINVAL;
	}

	n = 0;

	tx[n++] = address;
	tx[n++] = (u8)((0xFF00 & value1) >> 8);
	tx[n++] = (u8)(0xFF & value1);

	pr_debug("[ipd]%02XH,%02XH,%02XH\n", (int)tx[0], (int)tx[1], (int)tx[2]);

	if (valueNum == 2) {
		tx[n++] = (u8)((0xFF00 & value2) >> 8);
		tx[n++] = (u8)(0xFF & value2);
	}

	ret = ipd_i2c_write(client, tx, n);
	return ret;
}

static void ipd_read_control_status_register(struct ipd_data *ipd)
{
	int rValue = 0;
	int ret;

	ret = ipd_read_axis(ipd, IPD_REG_WORD_ST_Z_X, &rValue);
	pr_info("[ipd]  IPD_REG_WORD_ST_X\n");
	ret = ipd_read_axis(ipd, IPD_REG_WORD_ST_X, &rValue);
	pr_info("[ipd]  IPD_REG_WORD_ST_Y\n");
	ret = ipd_read_axis(ipd, IPD_REG_WORD_ST_Y, &rValue);
	pr_info("[ipd]  IPD_REG_WORD_ST_Z\n");
	ret = ipd_read_axis(ipd, IPD_REG_WORD_ST_Z, &rValue);
}

static void ipd_compute_distance(struct ipd_data *ipd)
{
	int ret = 0;

	ret = iio_read_channel_processed(ipd->adc, &ipd->resistance);
	if (ret) {
		if (ipd->resistance < ipd->near_res) {
			ipd->ipd_distance = ipd->near_ipd - 1;
			if ((ipd->near_res -  ipd->resistance) >= 500)
				ipd->ipd_distance = ipd->near_ipd - IPD_STEP_BASE;
		} else if (ipd->resistance > ipd->far_res) {
			ipd->ipd_distance = ipd->far_ipd + IPD_STEP_BASE;
		} else {
			ipd->ipd_distance  = (int)(ipd->near_ipd +
			(((ipd->far_ipd - ipd->near_ipd) * (ipd->resistance - ipd->near_res))
					/ (ipd->far_res - ipd->near_res)));
		}
	} else
		pr_err("[ipd]  Failed to read Reistance value (%d)\n", ret);
}

int ipd_thread_function(void *ipd_)
{
	struct ipd_data *ipd = ipd_;

	while (!kthread_should_stop()) {
		ipd_compute_distance(ipd);
		if (ipd->enable_print) {
			/*READ Status  Control register */
			ipd_read_control_status_register(ipd);

			pr_info("In ipd Thread Function && resistance=%d && distance=%f\n",
					ipd->resistance, ipd->ipd_distance);
		}
		if (ipd->sleep_time)
			msleep(ipd->sleep_time);
	}
	return 0;
}


static ssize_t enable_thread_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ipd_data *ipd = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", ipd->enable);
}

static ssize_t enable_thread_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct ipd_data *ipd = dev_get_drvdata(dev);
	int ret;

	ret = sscanf(buf, "%d\n", &ipd->enable);

	if (ipd->enable) {
		if (ipd->distance_thread) {
			pr_debug("ipd resuming kthread\n");
			wake_up_process(ipd->distance_thread);
		} else {
			pr_debug("ipd creating kthread\n");
			ipd->distance_thread = kthread_create(
					ipd_thread_function, ipd,
					 "IPD Thread");
			if (ipd->distance_thread)
				wake_up_process(ipd->distance_thread);
			else
				pr_err("%s ERROR creating IPD Thread\n", __func__);
		}
	} else {
		if (ipd->distance_thread) {
			kthread_stop(ipd->distance_thread);
			ipd->distance_thread = NULL;
		}
	}

	return count;
}

static ssize_t ipd_distance_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ipd_data *ipd = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", ipd->ipd_distance);
}

static ssize_t ipd_distance_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	pr_info("cannot store ipd_distance\n");
	return count;
}

static ssize_t resistance_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ipd_data *ipd = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", ipd->resistance);
}

static ssize_t resistance_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	pr_info("cannot store resistance\n");
	return count;
}

static ssize_t near_res_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ipd_data *ipd = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", ipd->near_res);
}

static ssize_t near_res_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct ipd_data *ipd = dev_get_drvdata(dev);
	int ret;

	ret = sscanf(buf, "%d\n", &ipd->near_res);
	return count;
}

static ssize_t far_res_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ipd_data *ipd = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", ipd->far_res);
}

static ssize_t far_res_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct ipd_data *ipd = dev_get_drvdata(dev);
	int ret;

	ret = sscanf(buf, "%d\n", &ipd->far_res);
	return count;
}

static ssize_t ipd_retry_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ipd_data *ipd = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", ipd->ipd_retry);
}

static ssize_t ipd_retry_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct ipd_data *ipd = dev_get_drvdata(dev);
	int ret;

	ret = sscanf(buf, "%d\n", &ipd->ipd_retry);
	return count;
}

static ssize_t near_ipd_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ipd_data *ipd = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", ipd->near_ipd);
}

static ssize_t near_ipd_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct ipd_data *ipd = dev_get_drvdata(dev);
	int ret;

	ret = sscanf(buf, "%d\n", &ipd->near_ipd);
	return count;
}

static ssize_t far_ipd_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ipd_data *ipd = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", ipd->far_ipd);
}

static ssize_t far_ipd_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct ipd_data *ipd = dev_get_drvdata(dev);
	int ret;

	ret = sscanf(buf, "%d\n", &ipd->far_ipd);
	return count;
}

static ssize_t sleep_time_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ipd_data *ipd = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", ipd->sleep_time);
}

static ssize_t sleep_time_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct ipd_data *ipd = dev_get_drvdata(dev);
	int ret;

	ret = sscanf(buf, "%d\n", &ipd->sleep_time);
	return count;
}

static ssize_t enable_print_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ipd_data *ipd = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", ipd->enable_print);
}

static ssize_t enable_print_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct ipd_data *ipd = dev_get_drvdata(dev);
	int ret;

	ret = sscanf(buf, "%d\n", &ipd->enable_print);
	return count;
}

static ssize_t enable_interrupt_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ipd_data *ipd = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", ipd->enable_interrupt);
}

static ssize_t enable_interrupt_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct ipd_data *ipd = dev_get_drvdata(dev);
	int ret;

	ret = sscanf(buf, "%d\n", &ipd->enable_interrupt);
	/*Enable disable interrupt on sys node*/
	if (ipd->enable_interrupt)
		enable_irq(ipd->irq);
	else
		disable_irq(ipd->irq);
	return count;
}

static DEVICE_ATTR_RW(enable_thread);
static DEVICE_ATTR_RW(ipd_distance);
static DEVICE_ATTR_RW(resistance);
static DEVICE_ATTR_RW(near_ipd);
static DEVICE_ATTR_RW(far_ipd);
static DEVICE_ATTR_RW(near_res);
static DEVICE_ATTR_RW(far_res);
static DEVICE_ATTR_RW(sleep_time);
static DEVICE_ATTR_RW(enable_print);
static DEVICE_ATTR_RW(enable_interrupt);
static DEVICE_ATTR_RW(ipd_retry);

static struct attribute *ipd_i2c_sysfs_attrs[] = {
	&dev_attr_enable_thread.attr,
	&dev_attr_ipd_distance.attr,
	&dev_attr_resistance.attr,
	&dev_attr_far_res.attr,
	&dev_attr_near_res.attr,
	&dev_attr_far_ipd.attr,
	&dev_attr_near_ipd.attr,
	&dev_attr_sleep_time.attr,
	&dev_attr_enable_print.attr,
	&dev_attr_enable_interrupt.attr,
	&dev_attr_ipd_retry.attr,
	NULL,
};

static struct attribute_group ipd_i2c_attribute_group = {
	.attrs = ipd_i2c_sysfs_attrs,
};

static int ipd_compute_step(struct ipd_data *ipd)
{
	int step_mode = 0;

	if (ipd->resistance < ipd->near_res) {
		step_mode = IPD_MIN_STEP + 1;
		if ((ipd->near_res -  ipd->resistance) >= 500)
			step_mode = IPD_MIN_STEP;
	} else if (ipd->resistance > ipd->far_res) {
		step_mode = IPD_MAX_STEP;
	} else {
		step_mode = (int)((((ipd->far_ipd - ipd->near_ipd)
				* (ipd->resistance - ipd->near_res))
				/ (ipd->far_res - ipd->near_res)));
		step_mode += IPD_STEP_BASE;
		if (step_mode != 1)
			step_mode = (step_mode / 2);
	}
	return step_mode;
}

static void ipd_update_x_z_axis(struct ipd_data *ipd, int step)
{
	u16 readValue;
	int status;

	/*Read X axis */
	status = ipd_read_axis_u(ipd, IPD_REG_WORD_ST_X, &readValue);
	if (ipd->enable_print)
		pr_info("[ipd] %s : X raw readValue=%d\n",
				__func__, readValue >= MIN_READ_VALUE ?
				readValue - MAX_READ_VALUE : readValue);
	ipd->BOPXbits = readValue + BOPX[step];
	ipd->BRPXbits = readValue + BRPX[step];
	status = ipd_i2c_write16(ipd->client, IPD_REG_THX,
			SECOND_VAL_ENABLED, ipd->BOPXbits, ipd->BRPXbits);

	/*Read Z axis */
	status = ipd_read_axis_u(ipd, IPD_REG_WORD_ST_Z, &readValue);
	if (ipd->enable_print)
		pr_info("[ipd] Z raw readValue=%d\n", readValue >= MIN_READ_VALUE ?
				readValue - MAX_READ_VALUE : readValue);
	ipd->BOPZbits = readValue + BOPZ[step];
	ipd->BRPZbits = readValue + BRPZ[step];
	status = ipd_i2c_write16(ipd->client, IPD_REG_THZ,
			SECOND_VAL_ENABLED, ipd->BOPZbits, ipd->BRPZbits);
}

static void ipd_update_y_v_axis(struct ipd_data *ipd, int step)
{
	int status;

	/*Read Z axis */
	status = ipd_i2c_write16(ipd->client, IPD_REG_THY,
			SECOND_VAL_ENABLED, ipd->BOPYbits, ipd->BRPYbits);
	/*Read V axis */
	status = ipd_i2c_write16(ipd->client, IPD_REG_THV,
			SECOND_VAL_ENABLED, ipd->BOPVbits, ipd->BRPVbits);
}

static int ipd_write_mode(struct ipd_data *ipd, int modeBit)
{
	int mode;
	int ret;

	mode = (ipd->SMRbit << 6) + (ipd->SDRbit << 5) + modeBit;

	if (ipd->enable_print)
		pr_info("[ipd] %s (%d, %d)\n", __func__, IPD_REG_CNTL2, mode);

	ret = i2c_smbus_write_byte_data(ipd->client, (u8)IPD_REG_CNTL2, (u8)mode);
	if (ret < 0)
		pr_err("%s: comm error, ret= %d\n", __func__, ret);

	return ret;
}

static int ipd_read_axis(struct ipd_data *akm, int address, int *val)
{
	u16  rdata[4] = {0,};
	u16  wordLen  = IPD_WORD_LEN;

	if (akm->mode == 0)
		ipd_write_mode(akm, 1);

	*val = 0;
	if ((address == IPD_REG_WORD_ST || address == IPD_REG_CNTL2))
		wordLen =  1;

	if ((address == IPD_REG_WORD_ST_Z_X) || (address == IPD_REG_WORD_ST_Y_X) ||
			(address == IPD_REG_WORD_ST_Z_Y))
		wordLen = 3;

	if ((address == IPD_REG_WORD_ST_Z) || (address == IPD_REG_WORD_ST_Y) ||
			(address == IPD_REG_WORD_ST_X))
		wordLen = 2;

	ipd_i2c_read8_16(akm->client, address, wordLen, rdata);
	pr_info("[ipd] %s address:%x (%x)(%x)\n",
			__func__, address, (int)rdata[0], (int)rdata[1]);

	*val = rdata[wordLen - 1];
	if (address == IPD_REG_WORD_ST || address == IPD_REG_CNTL2)
		*val &= 0x3FF;

	return 0;
}

static int ipd_read_axis_u(struct ipd_data *ipd, int address, u16 *val)
{
	u16  rdata[IPD_WORD_LEN] = {0,};

	if (ipd->mode == 0)
		ipd_write_mode(ipd, 0x8);

	*val = 0;
	ipd_i2c_read8_16(ipd->client, address, IPD_WORD_LEN, rdata);
	*val = rdata[IPD_WORD_LEN - 1];
	if (ipd->enable_print)
		pr_debug("[ipd] %s address :%x (%x)(%x)\n",
				__func__, address, rdata[0], rdata[1]);

	if (*val >= MIN_READ_VALUE)
		*val -= MAX_READ_VALUE;
	return 0;
}

static int ipd_setup(struct i2c_client *client)
{
	struct ipd_data *ipd = i2c_get_clientdata(client);
	u8   mod_value = 0;
	int status;
	int step = 0;

	pr_info("[ipd] %s(%d)\n", __func__, __LINE__);

	if (ipd->mode < ARRAY_SIZE(modeBitTable))
		mod_value = modeBitTable[ipd->mode];

	status = ipd_write_mode(ipd, mod_value);

	ipd_compute_distance(ipd);

	/*Based in device type,
	 * We need update or skip updating BOPX/BRPX/BOPZ/BRPZ
	 */

	if (ipd->device_type) {
		ipd->client = client;
		step = ipd_compute_step(ipd);
		status = ipd_i2c_write16(ipd->client, IPD_REG_CNTL1, 1, CTRL1[step], 0);

		/*Update X and Z axis*/
		ipd_update_x_z_axis(ipd, step);

		/*Update Y and V axis*/
		ipd_update_y_v_axis(ipd, step);
	}

	pr_info("***********************IPD-SETUP**************************************************\n");
	if (ipd->device_type) {
		pr_info("[ipd] BOPX : (%d)  BRPX : (%d)\n", ipd->BOPXbits, ipd->BRPXbits);
		pr_info("[ipd] BOPY : (%d)  BRPY : (%d)\n", ipd->BOPYbits, ipd->BRPYbits);
		pr_info("[ipd] BOPZ : (%d)  BRPZ : (%d)\n", ipd->BOPZbits, ipd->BRPZbits);
		pr_info("[ipd] BOPV : (%d)  BRPV : (%d)\n", ipd->BOPVbits, ipd->BRPVbits);
		pr_info("[ipd] STEP : (%d)  IPD_REG_CNTL1: (%d)\n", step, CTRL1[step]);
	}
	pr_info("[ipd] MODE : (%d)  Device_type : (%d)\n", ipd->mode, ipd->device_type);
	pr_info("[ipd] MIN_IPD : (%d)  MAX_IPD : (%d)\n", ipd->near_ipd, ipd->far_ipd);
	pr_info("[ipd] MIN_RES : (%d)  MAX_RES : (%d)\n", ipd->near_res, ipd->far_res);
	pr_info("[ipd] Reistance (%d) Distance (%d)\n", ipd->resistance, ipd->ipd_distance);
	pr_info("****************************IPD-SETUP-END*****************************************\n");

	return 0;
}

static int ipd_parse_dt(struct ipd_data  *ipd)
{
	u32 buf[8];
	struct device *dev;
	struct device_node *np;
	int ret;

	pr_info("[ipd] %s(%d)\n", __func__, __LINE__);

	dev = &(ipd->client->dev);
	np = dev->of_node;

	if (!np)
		return -EINVAL;

	ret = of_property_read_u32_array(np, "ipd,measurment_number", buf, 1);
	if (ret < 0)
		ipd->msrNo = ipd_MSRNO_WORD_ST_Z_Y_X;
	else {
		ipd->msrNo = buf[0];
		if (buf[0] > ipd_MSRNO_BYTE_ST_Z_Y_X)
			ipd->msrNo = ipd_MSRNO_WORD_ST_Z_Y_X;
	}

	ret = of_property_read_u32_array(np, "ipd,DRDY_event", buf, 1);
	if (ret < 0)
		ipd->DRDYENbit = 1;
	else
		ipd->DRDYENbit = buf[0];

	ret = of_property_read_u32_array(np, "ipd,ERR_event", buf, 1);
	if (ret < 0)
		ipd->ERRENbit = 1;
	else
		ipd->ERRENbit = buf[0];

	ret = of_property_read_u32_array(np, "ipd,POL_setting", buf, 4);
	if (ret < 0) {
		ipd->POLXbit = 0;
		ipd->POLYbit = 0;
		ipd->POLZbit = 0;
		ipd->POLVbit = 0;
	} else {
		ipd->POLXbit = buf[0];
		ipd->POLYbit = buf[1];
		ipd->POLZbit = buf[2];
		ipd->POLVbit = buf[3];
	}

	ret = of_property_read_u32_array(np, "ipd,SDR_setting", buf, 1);
	if (ret < 0)
		ipd->SDRbit = 0;
	else
		ipd->SDRbit = buf[0];

	ret = of_property_read_u32_array(np, "ipd,Mode", buf, 1);
	if (ret < 0)
		ipd->mode = 0;
	else
		ipd->mode = buf[0];

	ret = of_property_read_u32_array(np, "ipd,near_ipd", buf, 1);
	if (ret < 0)
		ipd->near_ipd = 0;
	else
		ipd->near_ipd = buf[0];

	ret = of_property_read_u32_array(np, "ipd,far_ipd", buf, 1);
	if (ret < 0)
		ipd->far_ipd = 0;
	else
		ipd->far_ipd = buf[0];

	ret = of_property_read_u32_array(np, "ipd,near_res", buf, 1);
	if (ret < 0)
		ipd->near_res = 0;
	else
		ipd->near_res = buf[0];

	ret = of_property_read_u32_array(np, "ipd,far_res", buf, 1);
	if (ret < 0)
		ipd->far_res = 0;
	else
		ipd->far_res = buf[0];

	ret = of_property_read_u32_array(np, "ipd,Device_type", buf, 1);
	if (ret < 0)
		ipd->device_type = 0;
	else
		ipd->device_type = buf[0];

	ret = of_property_read_u32_array(np, "ipd,SMR_setting", buf, 1);
	if (ret < 0)
		ipd->SMRbit = 0;
	else
		ipd->SMRbit = buf[0];

	ret = of_property_read_u32_array(np, "ipd,threshold_X", buf, 2);
	if (ret < 0) {
		ipd->BOPXbits = 0;
		ipd->BRPXbits = 0;
	} else {
		ipd->BOPXbits = buf[0];
		ipd->BRPXbits = buf[1];
	}

	ret = of_property_read_u32_array(np, "ipd,threshold_Y", buf, 2);
	if (ret < 0) {
		ipd->BOPYbits = 0;
		ipd->BRPYbits = 0;
	} else {
		ipd->BOPYbits = buf[0];
		ipd->BRPYbits = buf[1];
	}

	ret = of_property_read_u32_array(np, "ipd,threshold_Z", buf, 2);
	if (ret < 0) {
		ipd->BOPZbits = 0;
		ipd->BRPZbits = 0;
	} else {
		ipd->BOPZbits = buf[0];
		ipd->BRPZbits = buf[1];
	}

	ret = of_property_read_u32_array(np, "ipd,threshold_V", buf, 2);
	if (ret < 0) {
		ipd->BOPVbits = 0;
		ipd->BRPVbits = 0;
	} else {
		ipd->BOPVbits = buf[0];
		ipd->BRPVbits = buf[1];
	}

	ret = of_property_read_u32_array(np, "ipd,SW_event", buf, 4);
	if (ret < 0) {
		ipd->SWXENbit = 0;
		ipd->SWYENbit = 0;
		ipd->SWZENbit = 0;
		ipd->SWVENbit = 0;
	} else {
		ipd->SWXENbit = buf[0];
		ipd->SWYENbit = buf[1];
		ipd->SWZENbit = buf[2];
		ipd->SWVENbit = buf[3];
	}
	return 0;
}

/*
 * Handle data ready irq
 */
static void ipd_update_registers(struct work_struct *w)
{
	struct ipd_data *ipd = container_of(w, struct ipd_data, update_registers);
	unsigned int retry = ipd->ipd_retry;
	int  step = 0;
	int ret = 0;

	mutex_lock(&ipd_mutex);

	do {
		retry -= 1;
		ipd_compute_distance(ipd);

		if (ipd->enable_print)
			ipd_read_control_status_register(ipd);

		/*Based in device type
		 * We need update or skip updating BOPX/BRPX/BOPZ/BRPZ
		 */
		if (ipd->device_type) {
			step = ipd_compute_step(ipd);

			/*CNTL1 Registers*/
			ret = ipd_i2c_write16(ipd->client, IPD_REG_CNTL1, 1, CTRL1[step], 0);
			/*Update X and Z axis*/
			ipd_update_x_z_axis(ipd, step);
		}
		if (retry)
			msleep(ipd->sleep_time);

		if (ipd->enable_print) {
			if (ipd->device_type) {
				pr_info("[ipd] BOPX : (%d)  BRPX : (%d)\n",
						ipd->BOPXbits, ipd->BRPXbits);
				pr_info("[ipd] BOPZ : (%d)  BRPZ : (%d)\n",
						ipd->BOPZbits, ipd->BRPZbits);
			}
			pr_info("[ipd] MODE : (%d)  STEP : (%d)  IPD_REG_CNTL1: (%d)\n",
					modeBitTable[ipd->mode], step, CTRL1[step]);
			pr_info("[ipd] MIN_IPD : (%d)  MAX_IPD : (%d)\n",
					ipd->near_ipd, ipd->far_ipd);
			pr_info("[ipd] MIN_RES : (%d)  MAX_RES : (%d)\n",
					ipd->near_res, ipd->far_res);
			pr_info("[ipd] DISTANCE : (%d) RESISTANCE : (%d)\n",
					ipd->ipd_distance, ipd->resistance);
		}

	} while (retry);
	mutex_unlock(&ipd_mutex);

	/*enable IRQ's*/
	enable_irq(ipd->irq);
}


static irqreturn_t ipd_irq_handler(int irq, void *data)
{
	struct ipd_data *ipd = data;

	disable_irq_nosync(ipd->irq);

	if (ipd->enable_print)
		pr_info("In ipd INT Handler ENTER %d\n", ipd->irq);

	queue_work(ipd->ipd_wq, &ipd->update_registers);

	if (ipd->enable_print)
		pr_info("In ipd INT Handler EXIT %d\n", ipd->irq);

	return IRQ_HANDLED;
}

static int ipd_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct ipd_data *ipd;
	int err;
	const char *name = NULL;

	ipd = devm_kzalloc(&client->dev, sizeof(*ipd), GFP_KERNEL);
	if (!ipd)
		return -ENOMEM;

	i2c_set_clientdata(client, ipd);
	dev_set_drvdata(&client->dev, ipd);

	ipd->client = client;
	dev_err(&client->dev, "Probe IPD Driver\n");

	INIT_WORK(&ipd->update_registers, ipd_update_registers);
	ipd->ipd_wq = alloc_ordered_workqueue("ipd_wq", 0);

	ipd->adc = devm_iio_channel_get(&(ipd->client->dev), "ipd");
	if (IS_ERR(ipd->adc)) {
		dev_err(&client->dev, "Not able to fetch ipd IIO channel\n");
		return PTR_ERR(ipd->adc);
	}

	ipd->int_gpio = of_get_gpio(client->dev.of_node, 0);

	if (gpio_is_valid(ipd->int_gpio)) {
		err = devm_gpio_request_one(&client->dev, ipd->int_gpio,
							GPIOF_IN, "ipd_int");
		if (err < 0) {
			dev_err(&client->dev, "[ipd] failed to request GPIO %d, error %d\n",
							ipd->int_gpio, err);
			return err;
		}
	}

	ipd->irq = gpio_to_irq(ipd->int_gpio);

	err = ipd_parse_dt(ipd);
	if (err < 0)
		dev_err(&client->dev, "[ipd] Device Tree Setting was not found!\n");
	if (id)
		name = id->name;

	if (ipd->irq) {
		err = devm_request_irq(&client->dev, ipd->irq,
				ipd_irq_handler,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"ipd_INT", ipd);
		if (err < 0) {
			dev_err(&client->dev, "%s dev_request_irq  fails\n", name);
			return err;
		}
		pr_info("[ipd] %s Register Trigger Interrupt :%d\n", __func__, err);
	}
	ipd->sleep_time = MIN_SLEEP_TIME;
	ipd->enable_interrupt = 1;
	ipd->ipd_retry = MAX_IPD_RETRIES;

	err = ipd_setup(client);
	if (err < 0) {
		dev_err(&client->dev, "%s initialization fails\n", name);
		return err;
	}
	mutex_init(&ipd_mutex);
	err = devm_device_add_group(&client->dev, &ipd_i2c_attribute_group);
	if (err < 0) {
		dev_err(&client->dev, "couldn't register sysfs group\n");
		return err;
	}

	pr_info("[ipd] %s(ipd_device_register=%d)\n", __func__, err);
	return err;
}

static int ipd_remove(struct i2c_client *client)
{
	struct ipd_data *ipd = i2c_get_clientdata(client);


	if (ipd->distance_thread) {
		kthread_stop(ipd->distance_thread);
		ipd->distance_thread = NULL;
	}

	i2c_unregister_device(client);
	return 0;
}

static int ipd_i2c_suspend(struct device *dev)
{
	struct ipd_data *ipd = dev_get_drvdata(dev);

	if (ipd->distance_thread) {
		kthread_stop(ipd->distance_thread);
		ipd->distance_thread = NULL;
	} else {
		pr_info("[ipd] %s\n", __func__);
		mutex_lock(&ipd_mutex);
		disable_irq_nosync(ipd->irq);
		mutex_unlock(&ipd_mutex);
	}

	return 0;
}

static int ipd_i2c_resume(struct device *dev)
{
	struct ipd_data *ipd = dev_get_drvdata(dev);

	if (ipd->enable) {
		ipd->distance_thread = kthread_create(ipd_thread_function, ipd, "IPD Thread");
		if (ipd->distance_thread)
			wake_up_process(ipd->distance_thread);
		else
			pr_err("%s ERROR creating IPD Thread\n", __func__);
	} else {
		pr_info("[ipd] %s\n", __func__);
		mutex_lock(&ipd_mutex);
		enable_irq(ipd->irq);
		mutex_unlock(&ipd_mutex);
	}

	return 0;
}

static const struct dev_pm_ops ipd_i2c_pops = {
	.suspend	= ipd_i2c_suspend,
	.resume		= ipd_i2c_resume,
};

static const struct i2c_device_id ipd_id[] = {
	{ "ipd", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, ipd_id);

static const struct of_device_id ipd_of_match[] = {
	{ .compatible = "qcom,ipd"},
	{}
};
MODULE_DEVICE_TABLE(of, ipd_of_match);

static struct i2c_driver ipd_driver = {
	.driver = {
		.name	= "ipd",
		.pm = &ipd_i2c_pops,
		.of_match_table = of_match_ptr(ipd_of_match),
	},
	.probe		= ipd_probe,
	.remove		= ipd_remove,
	.id_table	= ipd_id,
};
module_i2c_driver(ipd_driver);

MODULE_DESCRIPTION("Inter Pupillary Distance(IPD) Driver");
MODULE_LICENSE("GPL v2");
