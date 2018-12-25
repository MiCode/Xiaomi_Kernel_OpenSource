/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */


/* akm09915.c - akm09915 compass driver
 *
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

#define pr_fmt(fmt) "<AKM09915> " fmt

#include "akm09915.h"
#include "cust_mag.h"
#include "mag.h"

#define DEBUG 0
#define AKM09915_DEV_NAME "akm09915"
#define DRIVER_VERSION "1.0.1"
#define AKM09915_DEBUG 1
#define AKM09915_RETRY_COUNT 10
#define AKM09915_DEFAULT_DELAY 100

#if AKM09915_DEBUG
#define MAGN_TAG "[AKM09915] "
#else
#define MAGN_TAG
#define pr_err(fmt, args...)                                                 \
	do {                                                                   \
	} while (0)
#define pr_debug(fmt, args...)                                                 \
	do {                                                                   \
	} while (0)
#endif

/* Addresses to scan -- protected by sense_data_mutex */
static char sense_data[SENSOR_DATA_SIZE];
static struct mutex sense_data_mutex;
/* calibration msensor and orientation data */
static int sensor_data[CALIBRATION_DATA_SIZE];
static struct mutex sensor_data_mutex;
/* static DECLARE_WAIT_QUEUE_HEAD(data_ready_wq); */
static DECLARE_WAIT_QUEUE_HEAD(open_wq);

static short akmd_delay = AKM09915_DEFAULT_DELAY;

static atomic_t open_flag = ATOMIC_INIT(0);
static atomic_t m_flag = ATOMIC_INIT(0);
static atomic_t o_flag = ATOMIC_INIT(0);

static int factory_mode;
static int mEnabled;
static int akm09915_init_flag;
static struct i2c_client *this_client;

/*----------------------------------------------------------------------------*/
static const struct i2c_device_id akm09915_i2c_id[] = {{AKM09915_DEV_NAME, 0},
						       {} };

/*----------------------------------------------------------------------------*/
static int akm09915_i2c_probe(struct i2c_client *client,
			      const struct i2c_device_id *id);
static int akm09915_i2c_remove(struct i2c_client *client);
static int akm09915_i2c_detect(struct i2c_client *client,
			       struct i2c_board_info *info);
static int akm09915_suspend(struct device *dev);
static int akm09915_resume(struct device *dev);
static int akm09915_local_init(void);
static int akm09915_remove(void);

static struct mag_init_info akm09915_init_info = {
	.name = "akm09915",
	.init = akm09915_local_init,
	.uninit = akm09915_remove,
};

enum { AMK_FUN_DEBUG = 0x01,
AMK_DATA_DEBUG = 0X02,
AMK_HWM_DEBUG = 0X04,
AMK_CTR_DEBUG = 0X08,
AMK_I2C_DEBUG = 0x10,
} AMK_TRC;


struct akm09915_i2c_data {
	struct i2c_client *client;
	struct mag_hw hw;
	atomic_t layout;
	atomic_t trace;
	struct hwmsen_convert cvt;
};

#ifdef CONFIG_OF
static const struct of_device_id mag_of_match[] = {
	{.compatible = "mediatek,msensor"}, {},
};
#endif

#ifdef CONFIG_PM_SLEEP
static const struct dev_pm_ops akm09915_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(akm09915_suspend, akm09915_resume)};
#endif

static struct i2c_driver akm09915_i2c_driver = {
	.driver = {

			.name = AKM09915_DEV_NAME,
#ifdef CONFIG_PM_SLEEP
			.pm = &akm09915_pm_ops,
#endif
#ifdef CONFIG_OF
			.of_match_table = mag_of_match,
#endif
		},
	.probe = akm09915_i2c_probe,
	.remove = akm09915_i2c_remove,
	.detect = akm09915_i2c_detect,
	.id_table = akm09915_i2c_id,
};

/*----------------------------------------------------------------------------*/
static atomic_t dev_open_count;
/*----------------------------------------------------------------------------*/

static DEFINE_MUTEX(akm09915_i2c_mutex);
#ifndef CONFIG_MTK_I2C_EXTENSION
static int mag_i2c_read_block(struct i2c_client *client, u8 addr, u8 *data,
			      u8 len)
{
	int err = 0;
	u8 beg = addr;
	struct i2c_msg msgs[2] = {{0}, {0} };

	mutex_lock(&akm09915_i2c_mutex);
	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = &beg;

	msgs[1].addr = client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = data;

	if (!client) {
		mutex_unlock(&akm09915_i2c_mutex);
		return -EINVAL;
	} else if (len > C_I2C_FIFO_SIZE) {
		mutex_unlock(&akm09915_i2c_mutex);
		pr_err(" length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		return -EINVAL;
	}

	err = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (err != 2) {
		pr_err("i2c_transfer error: (%d %p %d) %d\n", addr, data, len,
			 err);
		err = -EIO;
	} else {
		err = 0;
	}
	mutex_unlock(&akm09915_i2c_mutex);
	return err;
}

static int mag_i2c_write_block(struct i2c_client *client, u8 addr, u8 *data,
			       u8 len)
{
	int err = 0, idx = 0, num = 0;
	char buf[C_I2C_FIFO_SIZE];

	err = 0;
	mutex_lock(&akm09915_i2c_mutex);
	if (!client) {
		mutex_unlock(&akm09915_i2c_mutex);
		return -EINVAL;
	} else if (len >= C_I2C_FIFO_SIZE) {
		mutex_unlock(&akm09915_i2c_mutex);
		pr_err(" length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		return -EINVAL;
	}

	num = 0;
	buf[num++] = addr;
	for (idx = 0; idx < len; idx++)
		buf[num++] = data[idx];

	err = i2c_master_send(client, buf, num);
	if (err < 0) {
		mutex_unlock(&akm09915_i2c_mutex);
		pr_err("send command error!!\n");
		return -EFAULT;
	}
	mutex_unlock(&akm09915_i2c_mutex);
	return err;
}
#endif

static long AKI2C_RxData(char *rxData, int length)
{
#ifndef CONFIG_MTK_I2C_EXTENSION
	struct i2c_client *client = this_client;
	int res = 0;
	char addr;

	if ((rxData == NULL) || (length < 1))
		return -EINVAL;
	addr = rxData[0];

	res = mag_i2c_read_block(client, addr, rxData, length);
	if (res < 0)
		return -1;
	return 0;
#else
	uint8_t loop_i = 0;
#if DEBUG
	int i = 0;
	struct i2c_client *client = this_client;
	struct akm09915_i2c_data *data = i2c_get_clientdata(client);
	char addr = rxData[0];
#endif

	/* Caller should check parameter validity. */
	if ((rxData == NULL) || (length < 1))
		return -EINVAL;

	mutex_lock(&akm09915_i2c_mutex);
	for (loop_i = 0; loop_i < AKM09915_RETRY_COUNT; loop_i++) {
		this_client->addr = this_client->addr & I2C_MASK_FLAG;
		this_client->addr = this_client->addr | I2C_WR_FLAG;
		if (i2c_master_send(this_client, (const char *)rxData,
				    ((length << 0X08) | 0X01)))
			break;
		mdelay(10);
	}

	if (loop_i >= AKM09915_RETRY_COUNT) {
		mutex_unlock(&akm09915_i2c_mutex);
		pr_err("%s retry over %d\n", __func__,
			   AKM09915_RETRY_COUNT);
		return -EIO;
	}
	mutex_unlock(&akm09915_i2c_mutex);
#if DEBUG
	if (atomic_read(&data->trace) & AMK_I2C_DEBUG) {
		pr_debug("RxData: len=%02x, addr=%02x\n  data=", length, addr);
		for (i = 0; i < length; i++)
			pr_debug(" %02x", rxData[i]);

		pr_debug("\n");
	}
#endif

	return 0;
#endif
}

static long AKI2C_TxData(char *txData, int length)
{
#ifndef CONFIG_MTK_I2C_EXTENSION
	struct i2c_client *client = this_client;
	int res = 0;
	char addr;
	u8 *buff;

	if ((txData == NULL) || (length < 2))
		return -EINVAL;
	addr = txData[0];
	buff = &txData[1];

	res = mag_i2c_write_block(client, addr, buff, (length - 1));
	if (res < 0)
		return -1;
	return 0;
#else
	uint8_t loop_i = 0;
#if DEBUG
	int i = 0;
	struct i2c_client *client = this_client;
	struct akm09915_i2c_data *data = i2c_get_clientdata(client);
#endif

	/* Caller should check parameter validity. */
	if ((txData == NULL) || (length < 2))
		return -EINVAL;
	mutex_lock(&akm09915_i2c_mutex);
	this_client->addr = this_client->addr & I2C_MASK_FLAG;
	for (loop_i = 0; loop_i < AKM09915_RETRY_COUNT; loop_i++) {
		if (i2c_master_send(this_client, (const char *)txData, length) >
		    0)
			break;
		mdelay(10);
	}

	if (loop_i >= AKM09915_RETRY_COUNT) {
		mutex_unlock(&akm09915_i2c_mutex);
		pr_err("%s retry over %d\n", __func__,
			   AKM09915_RETRY_COUNT);
		return -EIO;
	}
	mutex_unlock(&akm09915_i2c_mutex);
#if DEBUG
	if (atomic_read(&data->trace) & AMK_I2C_DEBUG) {
		pr_debug("TxData: len=%02x, addr=%02x\n  data=", length,
			 txData[0]);
		for (i = 0; i < (length - 1); i++)
			pr_debug(" %02x", txData[i + 1]);

		pr_debug("\n");
	}
#endif

	return 0;
#endif
}

static long AKECS_SetMode_SngMeasure(void)
{
	char buffer[2];
/* Set measure mode */
#ifdef AKM_Device_AK8963
	buffer[0] = AK8963_REG_CNTL1;
	buffer[1] = AK8963_MODE_SNG_MEASURE;
#else
	buffer[0] = AK09915_REG_CNTL2;
	buffer[1] = AK09915_MODE_SNG_MEASURE;
#endif

	/* Set data */
	return AKI2C_TxData(buffer, 2);
}

static long AKECS_SetMode_SelfTest(void)
{
	char buffer[2];
/* Set self test mode */
#ifdef AKM_Device_AK8963
	buffer[0] = AK8963_REG_CNTL1;
	buffer[1] = AK8963_MODE_SELF_TEST;
#else
	buffer[0] = AK09915_REG_CNTL2;
	buffer[1] = AK09915_MODE_SELF_TEST;
#endif
	/* Set data */
	return AKI2C_TxData(buffer, 2);
}

static int AKECS_SetMode_PowerDown(void)
{
	char buffer[2];
/* Set powerdown mode */
#ifdef AKM_Device_AK8963
	buffer[0] = AK8963_REG_CNTL1;
	buffer[1] = AK8963_MODE_POWERDOWN;
#else
	buffer[0] = AK09915_REG_CNTL2;
	buffer[1] = AK09915_MODE_POWERDOWN;
#endif
	/* Set data */
	return AKI2C_TxData(buffer, 2);
}

static long AKECS_Reset(int hard)
{
	unsigned char buffer[2];
	long err = 0;

	if (hard != 0) {
		/*TODO change to board setting */
		/* gpio_set_value(akm->rstn, 0); */
		udelay(5);
		/* gpio_set_value(akm->rstn, 1); */
	} else {
/* Set measure mode */
#ifdef AKM_Device_AK8963
		buffer[0] = AK8963_REG_CNTL2;
		buffer[1] = 0x01;
#else
		buffer[0] = AK09915_REG_CNTL3;
		buffer[1] = 0x01;
#endif
		err = AKI2C_TxData(buffer, 2);
		if (err < 0)
			pr_debug("%s: Can not set SRST bit.", __func__);
		else
			pr_debug("Soft reset is done.");
	}

	/* Device will be accessible 300 us after */
	udelay(300); /* 100 */

	return err;
}

static long AKECS_SetMode(char mode)
{
	long ret;

	switch (mode & 0x1F) {
	case AK09915_MODE_SNG_MEASURE:
		ret = AKECS_SetMode_SngMeasure();
		break;

	case AK09915_MODE_SELF_TEST:
	case AK8963_MODE_SELF_TEST:
		ret = AKECS_SetMode_SelfTest();
		break;

#if defined(AKM_Device_AK8963) || defined(AKM_Device_AK09911)
	case AK09911_MODE_FUSE_ACCESS:
	case AK8963_MODE_FUSE_ACCESS:
		ret = AKECS_SetMode_FUSEAccess();
		break;
#endif

	case AK09915_MODE_POWERDOWN:
		ret = AKECS_SetMode_PowerDown();
		break;

	default:
		pr_debug("%s: Unknown mode(%d)", __func__, mode);
		return -EINVAL;
	}

	/* wait at least 100us after changing mode */
	udelay(100);

	return ret;
}

static int AKECS_CheckDevice(void)
{
	char buffer[2];
	int ret;

	pr_debug(" AKM check device id");
/* Set measure mode */
#ifdef AKM_Device_AK8963
	buffer[0] = AK8963_REG_WIA;
#else
	buffer[0] = AK09915_REG_WIA1;
#endif

	/* Read data */
	ret = AKI2C_RxData(buffer, 1);
	pr_debug(" AKM check device id = %x", buffer[0]);
	pr_debug("ret = %d", ret);
	if (ret < 0)
		return ret;

	/* Check read data */
	if (buffer[0] != 0x48)
		return -ENXIO;

	return 0;
}

/* Daemon application save the data */
static void AKECS_SaveData(int *buf)
{
#if DEBUG
	struct i2c_client *client = this_client;
	struct akm09915_i2c_data *data = i2c_get_clientdata(client);
#endif

	mutex_lock(&sensor_data_mutex);
	memcpy(sensor_data, buf, sizeof(sensor_data));
	mutex_unlock(&sensor_data_mutex);

#if DEBUG
	if (atomic_read(&data->trace) & AMK_HWM_DEBUG) {
		pr_debug(
			"Get daemon data[0-11]: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d\n",
			sensor_data[0], sensor_data[1], sensor_data[2],
			sensor_data[3], sensor_data[4], sensor_data[5],
			sensor_data[6], sensor_data[7], sensor_data[8],
			sensor_data[9], sensor_data[10], sensor_data[11]);

		pr_debug(
			"Get daemon data[12-25]: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d!\n",
			sensor_data[12], sensor_data[13], sensor_data[14],
			sensor_data[15], sensor_data[16], sensor_data[17],
			sensor_data[18], sensor_data[19], sensor_data[20],
			sensor_data[21], sensor_data[22], sensor_data[23],
			sensor_data[24], sensor_data[25]);
	}
#endif
}

/* M-sensor daemon application have set the sng mode */
static long AKECS_GetData(char *rbuf, int size)
{
	char temp;
	int loop_i, ret;
#if DEBUG
	struct i2c_client *client = this_client;
	struct akm09915_i2c_data *data = i2c_get_clientdata(client);
#endif

	if (size < SENSOR_DATA_SIZE) {
		pr_err("buff size is too small %d!\n", size);
		return -1;
	}

	memset(rbuf, 0, SENSOR_DATA_SIZE);
#ifdef AKM_Device_AK8963
	rbuf[0] = AK8963_REG_ST1;
#else
	rbuf[0] = AK09915_REG_ST1;
#endif

	for (loop_i = 0; loop_i < AKM09915_RETRY_COUNT; loop_i++) {
		ret = AKI2C_RxData(rbuf, 1);
		if (ret) {
			pr_err("read ST1 resigster failed!\n");
			return -1;
		}

		if ((rbuf[0] & 0x01) == 0x01)
			break;

		mdelay(2);
#ifdef AKM_Device_AK8963
		rbuf[0] = AK8963_REG_ST1;
#else
		rbuf[0] = AK09915_REG_ST1;
#endif
	}

	if (loop_i >= AKM09915_RETRY_COUNT) {
		pr_debug("Data read retry larger the max count!\n");
		if (factory_mode == 0)
			/* if return we can not get data at factory mode */
			return -1;
	}

	temp = rbuf[0];
#ifdef AKM_Device_AK8963
	rbuf[1] = AK8963_REG_HXL;
	ret = AKI2C_RxData(&rbuf[1], SENSOR_DATA_SIZE - 2);
#else
	rbuf[1] = AK09915_REG_HXL;
	ret = AKI2C_RxData(&rbuf[1], SENSOR_DATA_SIZE - 1);
#endif
	if (ret < 0) {
		pr_err("AKM8975 akm8975_work_func: I2C failed\n");
		return -1;
	}
	rbuf[0] = temp;
#ifdef AKM_Device_AK8963
	rbuf[8] = rbuf[7];
	rbuf[7] = 0;
#endif
	mutex_lock(&sense_data_mutex);
	memcpy(sense_data, rbuf, sizeof(sense_data));
	mutex_unlock(&sense_data_mutex);

#if DEBUG
	if (atomic_read(&data->trace) & AMK_DATA_DEBUG) {
		pr_debug("Get device data: %d, %d, %d, %d , %d, %d, %d, %d!\n",
			 sense_data[0], sense_data[1], sense_data[2],
			 sense_data[3], sense_data[4], sense_data[5],
			 sense_data[6], sense_data[7]);
	}
#endif

	return 0;
}

/* Get Msensor Raw data */
static int AKECS_GetRawData(char *rbuf, int size)
{
	char strbuf[SENSOR_DATA_SIZE];
	s16 data[3];

	if ((atomic_read(&open_flag) == 0) || (factory_mode == 1)) {
		AKECS_SetMode_SngMeasure();
		mdelay(10);
	}

	AKECS_GetData(strbuf, SENSOR_DATA_SIZE);
	data[0] = (s16)(strbuf[1] | (strbuf[2] << 8));
	data[1] = (s16)(strbuf[3] | (strbuf[4] << 8));
	data[2] = (s16)(strbuf[5] | (strbuf[6] << 8));

	sprintf(rbuf, "%x %x %x", data[0], data[1], data[2]);

	return 0;
}

static int AKECS_GetOpenStatus(void)
{
	wait_event_interruptible(open_wq, (atomic_read(&open_flag) != 0));
	return atomic_read(&open_flag);
}

static int AKECS_GetCloseStatus(void)
{
	wait_event_interruptible(open_wq, (atomic_read(&open_flag) <= 0));
	return atomic_read(&open_flag);
}

/*----------------------------------------------------------------------------*/
static int akm09915_ReadChipInfo(char *buf, int bufsize)
{
	if ((!buf) || (bufsize <= AKM09915_BUFSIZE - 1))
		return -1;

	if (!this_client) {
		*buf = 0;
		return -2;
	}

	sprintf(buf, "akm09915 Chip");
	return 0;
}

/*!
 * @return If @a testdata is in the range of between @a lolimit and @a hilimit,
 * the return value is 1, otherwise -1.
 * @param[in] testno   A pointer to a text string.
 * @param[in] testname A pointer to a text string.
 * @param[in] testdata A data to be tested.
 * @param[in] lolimit  The maximum allowable value of @a testdata.
 * @param[in] hilimit  The minimum allowable value of @a testdata.
 * @param[in,out] pf_total
 */
int TEST_DATA(const char testno[], const char testname[], const int testdata,
	      const int lolimit, const int hilimit, int *pf_total)
{
	int pf; /* Pass;1, Fail;-1 */

	if ((testno == NULL) && (strncmp(testname, "START", 5) == 0)) {
		pr_debug(
			" Test No. Test Name	Fail	Test Data	[	 Low	High]\n");
		pf = 1;
	} else if ((testno == NULL) && (strncmp(testname, "END", 3) == 0)) {
		if (*pf_total == 1)
			pr_debug("Factory shipment test was passed.\n\n");
		else
			pr_debug("Factory shipment test was failed.\n\n");

		pf = 1;
	} else {
		if ((lolimit <= testdata) && (testdata <= hilimit))
			pf = 1;
		else
			pf = -1;

		/* display result */
		pr_debug(" %7s  %-10s	 %c	%9d	[%9d	%9d]\n", testno,
			 testname, ((pf == 1) ? ('.') : ('F')), testdata,
			 lolimit, hilimit);
	}

	/* Pass/Fail check */
	if (*pf_total != 0) {
		if ((*pf_total == 1) && (pf == 1))
			*pf_total = 1; /* Pass */
		else
			*pf_total = -1; /* Fail */
	}
	return pf;
}

int FST_AK8963(void)
{
	int pf_total; /* p/f flag for this subtest */
	char i2cData[16];
	int hdata[3];
	int asax;
	int asay;
	int asaz;

	/* *********************************************** */
	/* Reset Test Result */
	/* *********************************************** */
	pf_total = 1;

	/* *********************************************** */
	/* Step1 */
	/* *********************************************** */

	/* Set to PowerDown mode */
	/* if (AKECS_SetMode(AK8963_MODE_POWERDOWN) < 0) { */
	/* pr_debug("%s:%d Error.\n", __FUNCTION__, __LINE__); */
	/* return 0; */
	/* } */
	AKECS_Reset(0);
	mdelay(1);

	/* When the serial interface is SPI, */
	/* write "00011011" to I2CDIS register(to disable I2C,). */
	if (CSPEC_SPI_USE == 1) {
		i2cData[0] = AK8963_REG_I2CDIS;
		i2cData[1] = 0x1B;
		if (AKI2C_TxData(i2cData, 2) < 0) {
			pr_debug("%s:%d Error.\n", __func__, __LINE__);
			return 0;
		}
	}

	/* Read values from WIA to ASTC. */
	i2cData[0] = AK8963_REG_WIA;
	if (AKI2C_RxData(i2cData, 7) < 0) {
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
		return 0;
	}

	/* TEST */
	TEST_DATA(TLIMIT_NO_RST_WIA, TLIMIT_TN_RST_WIA, (int)i2cData[0],
		  TLIMIT_LO_RST_WIA, TLIMIT_HI_RST_WIA, &pf_total);
	TEST_DATA(TLIMIT_NO_RST_INFO, TLIMIT_TN_RST_INFO, (int)i2cData[1],
		  TLIMIT_LO_RST_INFO, TLIMIT_HI_RST_INFO, &pf_total);
	TEST_DATA(TLIMIT_NO_RST_ST1, TLIMIT_TN_RST_ST1, (int)i2cData[2],
		  TLIMIT_LO_RST_ST1, TLIMIT_HI_RST_ST1, &pf_total);
	TEST_DATA(TLIMIT_NO_RST_HXL, TLIMIT_TN_RST_HXL, (int)i2cData[3],
		  TLIMIT_LO_RST_HXL, TLIMIT_HI_RST_HXL, &pf_total);
	TEST_DATA(TLIMIT_NO_RST_HXH, TLIMIT_TN_RST_HXH, (int)i2cData[4],
		  TLIMIT_LO_RST_HXH, TLIMIT_HI_RST_HXH, &pf_total);
	TEST_DATA(TLIMIT_NO_RST_HYL, TLIMIT_TN_RST_HYL, (int)i2cData[5],
		  TLIMIT_LO_RST_HYL, TLIMIT_HI_RST_HYL, &pf_total);
	TEST_DATA(TLIMIT_NO_RST_HYH, TLIMIT_TN_RST_HYH, (int)i2cData[6],
		  TLIMIT_LO_RST_HYH, TLIMIT_HI_RST_HYH, &pf_total);
	/* our i2c only most can read 8 byte  at one time , */
	i2cData[7] = AK8963_REG_HZL;
	if (AKI2C_RxData((i2cData + 7), 6) < 0) {
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
		return 0;
	}
	TEST_DATA(TLIMIT_NO_RST_HZL, TLIMIT_TN_RST_HZL, (int)i2cData[7],
		  TLIMIT_LO_RST_HZL, TLIMIT_HI_RST_HZL, &pf_total);
	TEST_DATA(TLIMIT_NO_RST_HZH, TLIMIT_TN_RST_HZH, (int)i2cData[8],
		  TLIMIT_LO_RST_HZH, TLIMIT_HI_RST_HZH, &pf_total);
	TEST_DATA(TLIMIT_NO_RST_ST2, TLIMIT_TN_RST_ST2, (int)i2cData[9],
		  TLIMIT_LO_RST_ST2, TLIMIT_HI_RST_ST2, &pf_total);
	TEST_DATA(TLIMIT_NO_RST_CNTL, TLIMIT_TN_RST_CNTL, (int)i2cData[10],
		  TLIMIT_LO_RST_CNTL, TLIMIT_HI_RST_CNTL, &pf_total);
	/* i2cData[11] is BLANK. */
	TEST_DATA(TLIMIT_NO_RST_ASTC, TLIMIT_TN_RST_ASTC, (int)i2cData[12],
		  TLIMIT_LO_RST_ASTC, TLIMIT_HI_RST_ASTC, &pf_total);

	/* Read values from I2CDIS. */
	i2cData[0] = AK8963_REG_I2CDIS;
	if (AKI2C_RxData(i2cData, 1) < 0) {
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
		return 0;
	}
	if (CSPEC_SPI_USE == 1)
		TEST_DATA(TLIMIT_NO_RST_I2CDIS, TLIMIT_TN_RST_I2CDIS,
			  (int)i2cData[0], TLIMIT_LO_RST_I2CDIS_USESPI,
			  TLIMIT_HI_RST_I2CDIS_USESPI, &pf_total);
	else
		TEST_DATA(TLIMIT_NO_RST_I2CDIS, TLIMIT_TN_RST_I2CDIS,
			  (int)i2cData[0], TLIMIT_LO_RST_I2CDIS_USEI2C,
			  TLIMIT_HI_RST_I2CDIS_USEI2C, &pf_total);

	/* Set to FUSE ROM access mode */
	if (AKECS_SetMode(AK8963_MODE_FUSE_ACCESS) < 0) {
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
		return 0;
	}

	/* Read values from ASAX to ASAZ */
	i2cData[0] = AK8963_FUSE_ASAX;
	if (AKI2C_RxData(i2cData, 3) < 0) {
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
		return 0;
	}
	asax = (int)i2cData[0];
	asay = (int)i2cData[1];
	asaz = (int)i2cData[2];

	/* TEST */
	TEST_DATA(TLIMIT_NO_ASAX, TLIMIT_TN_ASAX, asax, TLIMIT_LO_ASAX,
		  TLIMIT_HI_ASAX, &pf_total);
	TEST_DATA(TLIMIT_NO_ASAY, TLIMIT_TN_ASAY, asay, TLIMIT_LO_ASAY,
		  TLIMIT_HI_ASAY, &pf_total);
	TEST_DATA(TLIMIT_NO_ASAZ, TLIMIT_TN_ASAZ, asaz, TLIMIT_LO_ASAZ,
		  TLIMIT_HI_ASAZ, &pf_total);

	/* Read values. CNTL */
	i2cData[0] = AK8963_REG_CNTL1;
	if (AKI2C_RxData(i2cData, 1) < 0) {
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
		return 0;
	}

	/* Set to PowerDown mode */
	if (AKECS_SetMode(AK8963_MODE_POWERDOWN) < 0) {
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
		return 0;
	}

	/* TEST */
	TEST_DATA(TLIMIT_NO_WR_CNTL, TLIMIT_TN_WR_CNTL, (int)i2cData[0],
		  TLIMIT_LO_WR_CNTL, TLIMIT_HI_WR_CNTL, &pf_total);

	/* *********************************************** */
	/* Step2 */
	/* *********************************************** */

	/* Set to SNG measurement pattern (Set CNTL register) */
	if (AKECS_SetMode(AK8963_MODE_SNG_MEASURE) < 0) {
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
		return 0;
	}

	/* Wait for DRDY pin changes to HIGH. */
	mdelay(10);
	/* Get measurement data from AK8963 */
	/* ST1 + (HXL + HXH) + (HYL + HYH) + (HZL + HZH) + ST2 */
	/* = 1 + (1 + 1) + (1 + 1) + (1 + 1) + 1 = 8 bytes */
	if (AKECS_GetData(i2cData, SENSOR_DATA_SIZE) < 0) {
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
		return 0;
	}

	hdata[0] = (s16)(i2cData[1] | (i2cData[2] << 8));
	hdata[1] = (s16)(i2cData[3] | (i2cData[4] << 8));
	hdata[2] = (s16)(i2cData[5] | (i2cData[6] << 8));
	/* AK8963 @ 14 BIT */
	hdata[0] <<= 2;
	hdata[1] <<= 2;
	hdata[2] <<= 2;

	/* TEST */
	TEST_DATA(TLIMIT_NO_SNG_ST1, TLIMIT_TN_SNG_ST1, (int)i2cData[0],
		  TLIMIT_LO_SNG_ST1, TLIMIT_HI_SNG_ST1, &pf_total);
	TEST_DATA(TLIMIT_NO_SNG_HX, TLIMIT_TN_SNG_HX, hdata[0],
		  TLIMIT_LO_SNG_HX, TLIMIT_HI_SNG_HX, &pf_total);
	TEST_DATA(TLIMIT_NO_SNG_HY, TLIMIT_TN_SNG_HY, hdata[1],
		  TLIMIT_LO_SNG_HY, TLIMIT_HI_SNG_HY, &pf_total);
	TEST_DATA(TLIMIT_NO_SNG_HZ, TLIMIT_TN_SNG_HZ, hdata[2],
		  TLIMIT_LO_SNG_HZ, TLIMIT_HI_SNG_HZ, &pf_total);
	TEST_DATA(TLIMIT_NO_SNG_ST2, TLIMIT_TN_SNG_ST2, (int)i2cData[8],
		  TLIMIT_LO_SNG_ST2, TLIMIT_HI_SNG_ST2, &pf_total);

	/* Generate magnetic field for self-test (Set ASTC register) */
	i2cData[0] = AK8963_REG_ASTC;
	i2cData[1] = 0x40;
	if (AKI2C_TxData(i2cData, 2) < 0) {
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
		return 0;
	}

	/* Set to Self-test mode (Set CNTL register) */
	if (AKECS_SetMode(AK8963_MODE_SELF_TEST) < 0) {
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
		return 0;
	}

	/* Wait for DRDY pin changes to HIGH. */
	mdelay(10);
	/* Get measurement data from AK8963 */
	/* ST1 + (HXL + HXH) + (HYL + HYH) + (HZL + HZH) + ST2 */
	/* = 1 + (1 + 1) + (1 + 1) + (1 + 1) + 1 = 8Byte */
	if (AKECS_GetData(i2cData, SENSOR_DATA_SIZE) < 0) {
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
		return 0;
	}

	/* TEST */
	TEST_DATA(TLIMIT_NO_SLF_ST1, TLIMIT_TN_SLF_ST1, (int)i2cData[0],
		  TLIMIT_LO_SLF_ST1, TLIMIT_HI_SLF_ST1, &pf_total);

	hdata[0] = (s16)(i2cData[1] | (i2cData[2] << 8));
	hdata[1] = (s16)(i2cData[3] | (i2cData[4] << 8));
	hdata[2] = (s16)(i2cData[5] | (i2cData[6] << 8));

	/* AK8963 @ 14 BIT */
	hdata[0] <<= 2;
	hdata[1] <<= 2;
	hdata[2] <<= 2;

	pr_debug("hdata[0] = %d\n", hdata[0]);
	pr_debug("asax = %d\n", asax);
	TEST_DATA(TLIMIT_NO_SLF_RVHX, TLIMIT_TN_SLF_RVHX,
		  (hdata[0]) * ((asax - 128) / 2 / 128 + 1), TLIMIT_LO_SLF_RVHX,
		  TLIMIT_HI_SLF_RVHX, &pf_total);

	TEST_DATA(TLIMIT_NO_SLF_RVHY, TLIMIT_TN_SLF_RVHY,
		  (hdata[1]) * ((asay - 128) / 2 / 128 + 1), TLIMIT_LO_SLF_RVHY,
		  TLIMIT_HI_SLF_RVHY, &pf_total);

	TEST_DATA(TLIMIT_NO_SLF_RVHZ, TLIMIT_TN_SLF_RVHZ,
		  (hdata[2]) * ((asaz - 128) / 2 / 128 + 1), TLIMIT_LO_SLF_RVHZ,
		  TLIMIT_HI_SLF_RVHZ, &pf_total);
	/* TEST */
	TEST_DATA(TLIMIT_NO_SLF_ST2, TLIMIT_TN_SLF_ST2, (int)i2cData[8],
		  TLIMIT_LO_SLF_ST2, TLIMIT_HI_SLF_ST2, &pf_total);

	/* Set to Normal mode for self-test. */
	i2cData[0] = AK8963_REG_ASTC;
	i2cData[1] = 0x00;
	if (AKI2C_TxData(i2cData, 2) < 0) {
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
		return 0;
	}
	pr_debug("pf_total = %d\n", pf_total);
	return pf_total;
}

/*!
 *Execute "Onboard Function Test" (NOT includes "START" and "END" command).
 *@retval 1 The test is passed successfully.
 *@retval -1 The test is failed.
 *@retval 0 The test is aborted by kind of system error.
 */
int FST_AK09911(void)
{
	int pf_total; /* p/f flag for this subtest */
	char i2cData[16];
	int hdata[3];
	int asax;
	int asay;
	int asaz;

	/* *********************************************** */
	/* Reset Test Result */
	/* *********************************************** */
	pf_total = 1;

	/* *********************************************** */
	/* Step1 */
	/* *********************************************** */

	/* Reset device. */
	if (AKECS_Reset(0) < 0) {
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
		return 0;
	}

	/* Read values from WIA. */
	i2cData[0] = AK09911_REG_WIA1;
	if (AKI2C_RxData(i2cData, 2) < 0) {
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
		return 0;
	}

	/* TEST */
	TEST_DATA(TLIMIT_NO_RST_WIA1_09911, TLIMIT_TN_RST_WIA1_09911,
		  (int)i2cData[0], TLIMIT_LO_RST_WIA1_09911,
		  TLIMIT_HI_RST_WIA1_09911, &pf_total);
	TEST_DATA(TLIMIT_NO_RST_WIA2_09911, TLIMIT_TN_RST_WIA2_09911,
		  (int)i2cData[1], TLIMIT_LO_RST_WIA2_09911,
		  TLIMIT_HI_RST_WIA2_09911, &pf_total);

	/* Set to FUSE ROM access mode */
	if (AKECS_SetMode(AK09911_MODE_FUSE_ACCESS) < 0) {
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
		return 0;
	}

	/* Read values from ASAX to ASAZ */
	i2cData[0] = AK09911_FUSE_ASAX;
	if (AKI2C_RxData(i2cData, 3) < 0) {
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
		return 0;
	}
	asax = (int)i2cData[0];
	asay = (int)i2cData[1];
	asaz = (int)i2cData[2];

	/* TEST */
	TEST_DATA(TLIMIT_NO_ASAX_09911, TLIMIT_TN_ASAX_09911, asax,
		  TLIMIT_LO_ASAX_09911, TLIMIT_HI_ASAX_09911, &pf_total);
	TEST_DATA(TLIMIT_NO_ASAY_09911, TLIMIT_TN_ASAY_09911, asay,
		  TLIMIT_LO_ASAY_09911, TLIMIT_HI_ASAY_09911, &pf_total);
	TEST_DATA(TLIMIT_NO_ASAZ_09911, TLIMIT_TN_ASAZ_09911, asaz,
		  TLIMIT_LO_ASAZ_09911, TLIMIT_HI_ASAZ_09911, &pf_total);

	/* Set to PowerDown mode */
	if (AKECS_SetMode(AK09911_MODE_POWERDOWN) < 0) {
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
		return 0;
	}

	/* *********************************************** */
	/* Step2 */
	/* *********************************************** */

	/* Set to SNG measurement pattern (Set CNTL register) */
	if (AKECS_SetMode(AK09911_MODE_SNG_MEASURE) < 0) {
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
		return 0;
	}

	/* Wait for DRDY pin changes to HIGH. */
	/* usleep(AKM_MEASURE_TIME_US); */
	/* Get measurement data from AK09911 */
	/* ST1 + (HXL + HXH) + (HYL + HYH) + (HZL + HZH) + TEMP + ST2 */
	/* = 1 + (1 + 1) + (1 + 1) + (1 + 1) + 1 + 1 = 9yte */
	/* if (AKD_GetMagneticData(i2cData) != AKD_SUCCESS) { */
	if (AKECS_GetData(i2cData, SENSOR_DATA_SIZE) < 0) {
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
		return 0;
	}

	/* hdata[0] = (int)((((uint)(i2cData[2]))<<8)+(uint)(i2cData[1])); */
	/* hdata[1] = (int)((((uint)(i2cData[4]))<<8)+(uint)(i2cData[3])); */
	/* hdata[2] = (int)((((uint)(i2cData[6]))<<8)+(uint)(i2cData[5])); */

	hdata[0] = (s16)(i2cData[1] | (i2cData[2] << 8));
	hdata[1] = (s16)(i2cData[3] | (i2cData[4] << 8));
	hdata[2] = (s16)(i2cData[5] | (i2cData[6] << 8));

	/* TEST */
	i2cData[0] &= 0x7F;
	TEST_DATA(TLIMIT_NO_SNG_ST1_09911, TLIMIT_TN_SNG_ST1_09911,
		  (int)i2cData[0], TLIMIT_LO_SNG_ST1_09911,
		  TLIMIT_HI_SNG_ST1_09911, &pf_total);

	/* TEST */
	TEST_DATA(TLIMIT_NO_SNG_HX_09911, TLIMIT_TN_SNG_HX_09911, hdata[0],
		  TLIMIT_LO_SNG_HX_09911, TLIMIT_HI_SNG_HX_09911, &pf_total);
	TEST_DATA(TLIMIT_NO_SNG_HY_09911, TLIMIT_TN_SNG_HY_09911, hdata[1],
		  TLIMIT_LO_SNG_HY_09911, TLIMIT_HI_SNG_HY_09911, &pf_total);
	TEST_DATA(TLIMIT_NO_SNG_HZ_09911, TLIMIT_TN_SNG_HZ_09911, hdata[2],
		  TLIMIT_LO_SNG_HZ_09911, TLIMIT_HI_SNG_HZ_09911, &pf_total);
	TEST_DATA(TLIMIT_NO_SNG_ST2_09911, TLIMIT_TN_SNG_ST2_09911,
		  (int)i2cData[8], TLIMIT_LO_SNG_ST2_09911,
		  TLIMIT_HI_SNG_ST2_09911, &pf_total);

	/* Set to Self-test mode (Set CNTL register) */
	if (AKECS_SetMode(AK09911_MODE_SELF_TEST) < 0) {
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
		return 0;
	}

	/* Wait for DRDY pin changes to HIGH. */
	/* usleep(AKM_MEASURE_TIME_US); */
	/* Get measurement data from AK09911 */
	/* ST1 + (HXL + HXH) + (HYL + HYH) + (HZL + HZH) + TEMP + ST2 */
	/* = 1 + (1 + 1) + (1 + 1) + (1 + 1) + 1 + 1 = 9byte */
	/* if (AKD_GetMagneticData(i2cData) != AKD_SUCCESS) { */
	if (AKECS_GetData(i2cData, SENSOR_DATA_SIZE) < 0) {
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
		return 0;
	}

	/* TEST */
	i2cData[0] &= 0x7F;
	TEST_DATA(TLIMIT_NO_SLF_ST1_09911, TLIMIT_TN_SLF_ST1_09911,
		  (int)i2cData[0], TLIMIT_LO_SLF_ST1_09911,
		  TLIMIT_HI_SLF_ST1_09911, &pf_total);

	/* hdata[0] = (int)((((uint)(i2cData[2]))<<8)+(uint)(i2cData[1])); */
	/* hdata[1] = (int)((((uint)(i2cData[4]))<<8)+(uint)(i2cData[3])); */
	/* hdata[2] = (int)((((uint)(i2cData[6]))<<8)+(uint)(i2cData[5])); */

	hdata[0] = (s16)(i2cData[1] | (i2cData[2] << 8));
	hdata[1] = (s16)(i2cData[3] | (i2cData[4] << 8));
	hdata[2] = (s16)(i2cData[5] | (i2cData[6] << 8));

	/* TEST */
	TEST_DATA(TLIMIT_NO_SLF_RVHX_09911, TLIMIT_TN_SLF_RVHX_09911,
		  (hdata[0]) * (asax / 128 + 1), TLIMIT_LO_SLF_RVHX_09911,
		  TLIMIT_HI_SLF_RVHX_09911, &pf_total);

	TEST_DATA(TLIMIT_NO_SLF_RVHY_09911, TLIMIT_TN_SLF_RVHY_09911,
		  (hdata[1]) * (asay / 128 + 1), TLIMIT_LO_SLF_RVHY_09911,
		  TLIMIT_HI_SLF_RVHY_09911, &pf_total);

	TEST_DATA(TLIMIT_NO_SLF_RVHZ_09911, TLIMIT_TN_SLF_RVHZ_09911,
		  (hdata[2]) * (asaz / 128 + 1), TLIMIT_LO_SLF_RVHZ_09911,
		  TLIMIT_HI_SLF_RVHZ_09911, &pf_total);

	TEST_DATA(TLIMIT_NO_SLF_ST2_09911, TLIMIT_TN_SLF_ST2_09911,
		  (int)i2cData[8], TLIMIT_LO_SLF_ST2_09911,
		  TLIMIT_HI_SLF_ST2_09911, &pf_total);

	return pf_total;
}

int FST_AK09915(void)
{
	int pf_total; /* p/f flag for this subtest */
	char i2cData[16];
	int hdata[3];

	/* *********************************************** */
	/* Reset Test Result */
	/* *********************************************** */
	pf_total = 1;

	/* *********************************************** */
	/* Step1 */
	/* *********************************************** */

	/* Reset device. */
	if (AKECS_Reset(0) < 0) {
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
		return 0;
	}
	/* Read values from WIA. */
	i2cData[0] = AK09915_REG_WIA1;
	if (AKI2C_RxData(i2cData, 2) < 0) {
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
		return 0;
	}
	/* TEST */
	TEST_DATA(TLIMIT_NO_RST_WIA1_09915, TLIMIT_TN_RST_WIA1_09915,
		  (int)i2cData[0], TLIMIT_LO_RST_WIA1_09915,
		  TLIMIT_HI_RST_WIA1_09915, &pf_total);
	TEST_DATA(TLIMIT_NO_RST_WIA2_09915, TLIMIT_TN_RST_WIA2_09915,
		  (int)i2cData[1], TLIMIT_LO_RST_WIA2_09915,
		  TLIMIT_HI_RST_WIA2_09915, &pf_total);

	/* Set to PowerDown mode */
	if (AKECS_SetMode(AK09915_MODE_POWERDOWN) < 0) {
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
		return 0;
	}
	/* *********************************************** */
	/* Step2 */
	/* *********************************************** */

	/* Set to SNG measurement pattern (Set CNTL register) */
	if (AKECS_SetMode(AK09915_MODE_SNG_MEASURE) < 0) {
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
		return 0;
	}
	/* Wait for DRDY pin changes to HIGH. */
	/* usleep(AKM_MEASURE_TIME_US); */
	/* Get measurement data from AK09915 */
	/* ST1 + (HXL + HXH) + (HYL + HYH) + (HZL + HZH) + TEMP + ST2 */
	/* = 1 + (1 + 1) + (1 + 1) + (1 + 1) + 1 + 1 = 9yte */
	/* if (AKD_GetMagneticData(i2cData) != AKD_SUCCESS) { */
	if (AKECS_GetData(i2cData, SENSOR_DATA_SIZE) < 0) {
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
		return 0;
	}
	/* hdata[0] = (int)((((uint)(i2cData[2]))<<8)+(uint)(i2cData[1])); */
	/* hdata[1] = (int)((((uint)(i2cData[4]))<<8)+(uint)(i2cData[3])); */
	/* hdata[2] = (int)((((uint)(i2cData[6]))<<8)+(uint)(i2cData[5])); */

	hdata[0] = (s16)(i2cData[1] | (i2cData[2] << 8));
	hdata[1] = (s16)(i2cData[3] | (i2cData[4] << 8));
	hdata[2] = (s16)(i2cData[5] | (i2cData[6] << 8));

	/* TEST */
	/* i2cData[0] &= 0x7F; */
	TEST_DATA(TLIMIT_NO_SNG_ST1_09915, TLIMIT_TN_SNG_ST1_09915,
		  (int)i2cData[0], TLIMIT_LO_SNG_ST1_09915,
		  TLIMIT_HI_SNG_ST1_09915, &pf_total);

	/* TEST */
	TEST_DATA(TLIMIT_NO_SNG_HX_09915, TLIMIT_TN_SNG_HX_09915, hdata[0],
		  TLIMIT_LO_SNG_HX_09915, TLIMIT_HI_SNG_HX_09915, &pf_total);
	TEST_DATA(TLIMIT_NO_SNG_HY_09915, TLIMIT_TN_SNG_HY_09915, hdata[1],
		  TLIMIT_LO_SNG_HY_09915, TLIMIT_HI_SNG_HY_09915, &pf_total);
	TEST_DATA(TLIMIT_NO_SNG_HZ_09915, TLIMIT_TN_SNG_HZ_09915, hdata[2],
		  TLIMIT_LO_SNG_HZ_09915, TLIMIT_HI_SNG_HZ_09915, &pf_total);
	TEST_DATA(TLIMIT_NO_SNG_ST2_09915, TLIMIT_TN_SNG_ST2_09915,
		  (int)i2cData[8], TLIMIT_LO_SNG_ST2_09915,
		  TLIMIT_HI_SNG_ST2_09915, &pf_total);

	/* Set to Self-test mode (Set CNTL register) */
	if (AKECS_SetMode(AK09915_MODE_SELF_TEST) < 0) {
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
		return 0;
	}
	/* Wait for DRDY pin changes to HIGH. */
	/* usleep(AKM_MEASURE_TIME_US); */
	/* Get measurement data from AK09915 */
	/* ST1 + (HXL + HXH) + (HYL + HYH) + (HZL + HZH) + TEMP + ST2 */
	/* = 1 + (1 + 1) + (1 + 1) + (1 + 1) + 1 + 1 = 9byte */
	/* if (AKD_GetMagneticData(i2cData) != AKD_SUCCESS) { */
	if (AKECS_GetData(i2cData, SENSOR_DATA_SIZE) < 0) {
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
		return 0;
	}
	/* TEST */
	i2cData[0] &= 0x7F;
	TEST_DATA(TLIMIT_NO_SLF_ST1_09915, TLIMIT_TN_SLF_ST1_09915,
		  (int)i2cData[0], TLIMIT_LO_SLF_ST1_09915,
		  TLIMIT_HI_SLF_ST1_09915, &pf_total);

	/* hdata[0] = (int)((((uint)(i2cData[2]))<<8)+(uint)(i2cData[1])); */
	/* hdata[1] = (int)((((uint)(i2cData[4]))<<8)+(uint)(i2cData[3])); */
	/* hdata[2] = (int)((((uint)(i2cData[6]))<<8)+(uint)(i2cData[5])); */

	hdata[0] = (s16)(i2cData[1] | (i2cData[2] << 8));
	hdata[1] = (s16)(i2cData[3] | (i2cData[4] << 8));
	hdata[2] = (s16)(i2cData[5] | (i2cData[6] << 8));

	/* TEST */
	TEST_DATA(TLIMIT_NO_SLF_RVHX_09915, TLIMIT_TN_SLF_RVHX_09915, hdata[0],
		  TLIMIT_LO_SLF_RVHX_09915, TLIMIT_HI_SLF_RVHX_09915,
		  &pf_total);

	TEST_DATA(TLIMIT_NO_SLF_RVHY_09915, TLIMIT_TN_SLF_RVHY_09915, hdata[1],
		  TLIMIT_LO_SLF_RVHY_09915, TLIMIT_HI_SLF_RVHY_09915,
		  &pf_total);

	TEST_DATA(TLIMIT_NO_SLF_RVHZ_09915, TLIMIT_TN_SLF_RVHZ_09915, hdata[2],
		  TLIMIT_LO_SLF_RVHZ_09915, TLIMIT_HI_SLF_RVHZ_09915,
		  &pf_total);

	TEST_DATA(TLIMIT_NO_SLF_ST2_09915, TLIMIT_TN_SLF_ST2_09915,
		  (int)i2cData[8], TLIMIT_LO_SLF_ST2_09915,
		  TLIMIT_HI_SLF_ST2_09915, &pf_total);

	return pf_total;
}

/*!
 *Execute "Onboard Function Test" (includes "START" and "END" command).
 *@retval 1 The test is passed successfully.
 *@retval -1 The test is failed.
 *@retval 0 The test is aborted by kind of system error.
 */
int FctShipmntTestProcess_Body(void)
{
	int pf_total = 1;

	/* *********************************************** */
	/* Reset Test Result */
	/* *********************************************** */
	TEST_DATA(NULL, "START", 0, 0, 0, &pf_total);

/* *********************************************** */
/* Step 1 to 2 */
/* *********************************************** */
#ifdef AKM_Device_AK8963
	pf_total = FST_AK8963();
#else
#ifdef AKM_Device_AK09911
	pf_total = FST_AK09911();
#else
	pf_total = FST_AK09915();
#endif
#endif

	/* *********************************************** */
	/* Judge Test Result */
	/* *********************************************** */
	TEST_DATA(NULL, "END", 0, 0, 0, &pf_total);

	return pf_total;
}

static ssize_t store_shipment_test(struct device_driver *ddri, const char *buf,
				   size_t count)
{
	/* struct i2c_client *client = this_client; */
	/* struct akm09911_i2c_data *data = i2c_get_clientdata(client); */
	/* int layout = 0; */

	return count;
}

static ssize_t show_shipment_test(struct device_driver *ddri, char *buf)
{
	char result[10];
	int res = 0;

	res = FctShipmntTestProcess_Body();
	if (res == 1) {
		pr_debug("shipment_test pass\n");
		strcpy(result, "y");
	} else if (-1 == res) {
		pr_debug("shipment_test fail\n");
		strcpy(result, "n");
	} else {
		pr_debug("shipment_test NaN\n");
		strcpy(result, "NaN");
	}

	return sprintf(buf, "%s\n", result);
}

static ssize_t show_daemon_name(struct device_driver *ddri, char *buf)
{
	char strbuf[AKM09915_BUFSIZE];

#ifdef AKM_Device_AK8963
	sprintf(strbuf, "akmd8963");
#else
#ifdef AKM_Device_AK09911
	sprintf(strbuf, "akmd09911");
#else
	sprintf(strbuf, "akmd09915");
#endif
#endif
	return sprintf(buf, "%s", strbuf);
}

static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	char strbuf[AKM09915_BUFSIZE];

	akm09915_ReadChipInfo(strbuf, AKM09915_BUFSIZE);
	return sprintf(buf, "%s\n", strbuf);
}

/*----------------------------------------------------------------------------*/
static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{

	char sensordata[SENSOR_DATA_SIZE];
	char strbuf[AKM09915_BUFSIZE];

	if (atomic_read(&open_flag) == 0) {
		AKECS_SetMode_SngMeasure();
		mdelay(10);
		AKECS_GetData(sensordata, SENSOR_DATA_SIZE);
	} else {
		mutex_lock(&sense_data_mutex);
		memcpy(sensordata, sense_data, sizeof(sensordata));
		mutex_unlock(&sense_data_mutex);
	}

	sprintf(strbuf, "%d %d %d %d %d %d %d %d %d\n", sensordata[0],
		sensordata[1], sensordata[2], sensordata[3], sensordata[4],
		sensordata[5], sensordata[6], sensordata[7], sensordata[8]);

	return sprintf(buf, "%s\n", strbuf);
}

/*----------------------------------------------------------------------------*/
static ssize_t show_posturedata_value(struct device_driver *ddri, char *buf)
{
	short tmp[3];
	char strbuf[AKM09915_BUFSIZE];

	tmp[0] = sensor_data[13] * CONVERT_O / CONVERT_O_DIV;
	tmp[1] = sensor_data[14] * CONVERT_O / CONVERT_O_DIV;
	tmp[2] = sensor_data[15] * CONVERT_O / CONVERT_O_DIV;
	sprintf(strbuf, "%d, %d, %d\n", tmp[0], tmp[1], tmp[2]);

	return sprintf(buf, "%s\n", strbuf);
}

/*----------------------------------------------------------------------------*/
static ssize_t show_layout_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = this_client;
	struct akm09915_i2c_data *data = i2c_get_clientdata(client);

	return sprintf(buf, "(%d, %d)\n[%+2d %+2d %+2d]\n[%+2d %+2d %+2d]\n",
		       data->hw.direction, atomic_read(&data->layout),
		       data->cvt.sign[0], data->cvt.sign[1], data->cvt.sign[2],
		       data->cvt.map[0], data->cvt.map[1], data->cvt.map[2]);
}

/*----------------------------------------------------------------------------*/
static ssize_t store_layout_value(struct device_driver *ddri, const char *buf,
				  size_t count)
{
	struct i2c_client *client = this_client;
	struct akm09915_i2c_data *data = i2c_get_clientdata(client);
	int layout = 0;
	int ret = 0;

	ret = kstrtoint(buf, 10, &layout);
	if (ret != 0) {
		atomic_set(&data->layout, layout);
		if (!hwmsen_get_convert(layout, &data->cvt))
			pr_err("HWMSEN_GET_CONVERT function error!\r\n");
		else if (!hwmsen_get_convert(data->hw.direction, &data->cvt))
			pr_err("invalid layout: %d, restore to %d\n",
				   layout, data->hw.direction);
		else {
			pr_err("invalid layout: (%d, %d)\n", layout,
				   data->hw.direction);
			hwmsen_get_convert(0, &data->cvt);
		}
	} else
		pr_err("invalid format = '%s'\n", buf);

	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t show_status_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = this_client;
	struct akm09915_i2c_data *data = i2c_get_clientdata(client);
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "CUST: %d %d (%d %d)\n",
			data->hw.i2c_num, data->hw.direction, data->hw.power_id,
			data->hw.power_vol);

	len += snprintf(buf + len, PAGE_SIZE - len, "OPEN: %d\n",
			atomic_read(&dev_open_count));
	return len;
}

/*----------------------------------------------------------------------------*/
static ssize_t show_trace_value(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	struct akm09915_i2c_data *obj = i2c_get_clientdata(this_client);

	if (obj == NULL) {
		pr_err("akm09915_i2c_data is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&obj->trace));
	return res;
}

/*----------------------------------------------------------------------------*/
static ssize_t store_trace_value(struct device_driver *ddri, const char *buf,
				 size_t count)
{
	struct akm09915_i2c_data *obj = i2c_get_clientdata(this_client);
	int trace;

	if (obj == NULL) {
		pr_err("akm09915_i2c_data is null!!\n");
		return 0;
	}

	if (sscanf(buf, "0x%x", &trace) == 1)
		atomic_set(&obj->trace, trace);
	else
		pr_err("invalid content: '%s', length = %zu\n", buf, count);

	return count;
}

static ssize_t show_chip_orientation(struct device_driver *ddri, char *buf)
{
	ssize_t _tLength = 0;
	struct akm09915_i2c_data *_pt_i2c_obj = i2c_get_clientdata(this_client);

	if (_pt_i2c_obj == NULL)
		return 0;

	pr_debug("[%s] default direction: %d\n", __func__,
		 _pt_i2c_obj->hw.direction);

	_tLength = snprintf(buf, PAGE_SIZE, "default direction = %d\n",
			    _pt_i2c_obj->hw.direction);

	return _tLength;
}

static ssize_t store_chip_orientation(struct device_driver *ddri,
				      const char *buf, size_t tCount)
{
	int _nDirection = 0;
	int ret = 0;
	struct akm09915_i2c_data *_pt_i2c_obj = i2c_get_clientdata(this_client);

	if (_pt_i2c_obj == NULL)
		return 0;

	ret = kstrtoint(buf, 10, &_nDirection);
	if (ret != 0) {
		if (hwmsen_get_convert(_nDirection, &_pt_i2c_obj->cvt))
			pr_err("ERR: fail to set direction\n");
	}

	pr_debug("[%s] set direction: %d\n", __func__, _nDirection);

	return tCount;
}

static ssize_t show_power_status(struct device_driver *ddri, char *buf)
{
	ssize_t res = 0;
	u8 uData = AK09915_REG_CNTL2;
	ssize_t ret = 0;
	struct akm09915_i2c_data *obj = i2c_get_clientdata(this_client);

	if (obj == NULL) {
		pr_err("i2c_data obj is null!!\n");
		return 0;
	}
	ret = AKI2C_RxData(&uData, 1);
	if (ret < 0)
		return ret;
	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", uData);
	return res;
}

static ssize_t show_regiter_map(struct device_driver *ddri, char *buf)
{
	u8 _bIndex = 0;
	u8 _baRegMap[] = {0x00, 0x01, 0x02, 0x03, 0x10, 0x11, 0x12, 0x13, 0x14,
			  0x15, 0x16, 0x17, 0x18, 0x30, 0x31, 0x32, 0x33};
	/* u8  _baRegValue[20]; */
	ssize_t _tLength = 0;
	char tmp[2] = {0};
	ssize_t ret = 0;

	for (_bIndex = 0; _bIndex < 17; _bIndex++) {
		tmp[0] = _baRegMap[_bIndex];
		ret = AKI2C_RxData(tmp, 1);
		if (ret < 0)
			return ret;
		_tLength += snprintf((buf + _tLength), (PAGE_SIZE - _tLength),
				     "Reg[0x%02X]: 0x%02X\n",
				     _baRegMap[_bIndex], tmp[0]);
	}

	return _tLength;
}

/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(daemon, 0444, show_daemon_name, NULL);
static DRIVER_ATTR(shipmenttest, 0644, show_shipment_test,
		   store_shipment_test);
static DRIVER_ATTR(chipinfo, 0444, show_chipinfo_value, NULL);
static DRIVER_ATTR(sensordata, 0444, show_sensordata_value, NULL);
static DRIVER_ATTR(posturedata, 0444, show_posturedata_value, NULL);
static DRIVER_ATTR(layout, 0644, show_layout_value,
		   store_layout_value);
static DRIVER_ATTR(status, 0444, show_status_value, NULL);
static DRIVER_ATTR(trace, 0644, show_trace_value,
		   store_trace_value);
static DRIVER_ATTR(orientation, 0644, show_chip_orientation,
		   store_chip_orientation);
static DRIVER_ATTR(power, 0444, show_power_status, NULL);
static DRIVER_ATTR(regmap, 0444, show_regiter_map, NULL);

/*----------------------------------------------------------------------------*/
static struct driver_attribute *akm09915_attr_list[] = {
	&driver_attr_daemon,
	&driver_attr_shipmenttest,
	&driver_attr_chipinfo,
	&driver_attr_sensordata,
	&driver_attr_posturedata,
	&driver_attr_layout,
	&driver_attr_status,
	&driver_attr_trace,
	&driver_attr_orientation,
	&driver_attr_power,
	&driver_attr_regmap,
};

/*----------------------------------------------------------------------------*/
static int akm09915_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)ARRAY_SIZE(akm09915_attr_list);

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, akm09915_attr_list[idx]);
		if (err) {
			pr_err("driver_create_file (%s) = %d\n",
				   akm09915_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

/*----------------------------------------------------------------------------*/
static int akm09915_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)ARRAY_SIZE(akm09915_attr_list);

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, akm09915_attr_list[idx]);

	return err;
}

/*----------------------------------------------------------------------------*/
static int akm09915_open(struct inode *inode, struct file *file)
{
	struct akm09915_i2c_data *obj = i2c_get_clientdata(this_client);
	int ret = -1;

	if (atomic_read(&obj->trace) & AMK_CTR_DEBUG)
		pr_debug("Open device node:akm09915\n");

	ret = nonseekable_open(inode, file);

	return ret;
}

/*----------------------------------------------------------------------------*/
static int akm09915_release(struct inode *inode, struct file *file)
{
	struct akm09915_i2c_data *obj = i2c_get_clientdata(this_client);

	atomic_dec(&dev_open_count);
	if (atomic_read(&obj->trace) & AMK_CTR_DEBUG)
		pr_debug("Release device node:akm09915\n");

	return 0;
}

static long akm09915_unlocked_ioctl(struct file *file, unsigned int cmd,
				    unsigned long arg)
{
	void __user *argp = (void __user *)arg;

	/* NOTE: In this function the size of "char" should be 1-byte. */
	char sData[SENSOR_DATA_SIZE]; /* for GETDATA */
	char rwbuf[RWBUF_SIZE];       /* for READ/WRITE */
	char buff[AKM09915_BUFSIZE];  /* for chip information */
	char mode;		      /* for SET_MODE */
	int value[26];		      /* for SET_YPR */
	int64_t delay[3];	     /* for GET_DELAY */
	int status;		      /* for OPEN/CLOSE_STATUS */
	long ret = -1;		      /* Return value. */
	int layout;
	struct i2c_client *client = this_client;
	struct akm09915_i2c_data *data = i2c_get_clientdata(client);
	struct hwm_sensor_data *osensor_data;
	uint32_t enable;
	/* These two buffers are initialized at start up.*/
	/*After that, the value is not changed */
	unsigned char sense_info[AKM_SENSOR_INFO_SIZE];
	unsigned char sense_conf[AKM_SENSOR_CONF_SIZE];

	/* pr_err("akm09915 cmd:0x%x\n", cmd); */
	switch (cmd) {
	case ECS_IOCTL_WRITE:
		/* AKMFUNC("ECS_IOCTL_WRITE"); */
		if (argp == NULL) {
			pr_debug("invalid argument.");
			return -EINVAL;
		}
		if (copy_from_user(rwbuf, argp, sizeof(rwbuf))) {
			pr_debug("copy_from_user failed.");
			return -EFAULT;
		}

		if ((rwbuf[0] < 2) || (rwbuf[0] > (RWBUF_SIZE - 1))) {
			pr_debug("invalid argument.");
			return -EINVAL;
		}
		ret = AKI2C_TxData(&rwbuf[1], rwbuf[0]);
		if (ret < 0)

			return ret;

		break;

	case ECS_IOCTL_RESET:
		ret = AKECS_Reset(0); /* sw: 0, hw: 1 */
		if (ret < 0)
			return ret;
		break;

	case ECS_IOCTL_READ:
		if (argp == NULL) {
			pr_debug("invalid argument.");
			return -EINVAL;
		}

		if (copy_from_user(rwbuf, argp, sizeof(rwbuf))) {
			pr_debug("copy_from_user failed.");
			return -EFAULT;
		}

		if ((rwbuf[0] < 1) || (rwbuf[0] > (RWBUF_SIZE - 1))) {
			pr_debug("invalid argument.");
			return -EINVAL;
		}
		ret = AKI2C_RxData(&rwbuf[1], rwbuf[0]);
		if (ret < 0)
			return ret;

		if (copy_to_user(argp, rwbuf, rwbuf[0] + 1)) {
			pr_debug("copy_to_user failed.");
			return -EFAULT;
		}
		break;

	case ECS_IOCTL_GET_INFO:
#ifdef AKM_Device_AK8963
		sense_info[0] = AK8963_REG_WIA;
#else
		sense_info[0] = AK09915_REG_WIA1;
#endif

		ret = AKI2C_RxData(sense_info, AKM_SENSOR_INFO_SIZE);
		if (ret < 0)
			return ret;

		if (copy_to_user(argp, sense_info, AKM_SENSOR_INFO_SIZE)) {
			pr_debug("copy_to_user failed.");
			return -EFAULT;
		}
		break;

	case ECS_IOCTL_GET_CONF:
/* Set FUSE access mode */
#if defined(AKM_Device_AK8963) || defined(AKM_Device_AK09911)
#ifdef AKM_Device_AK8963
		ret = AKECS_SetMode(AK8963_MODE_FUSE_ACCESS);
#else
		ret = AKECS_SetMode(AK09911_MODE_FUSE_ACCESS);
#endif
		if (ret < 0)
			return ret;
#ifdef AKM_Device_AK8963
		sense_conf[0] = AK8963_FUSE_ASAX;
#else
		sense_conf[0] = AK09911_FUSE_ASAX;
#endif

		ret = AKI2C_RxData(sense_conf, AKM_SENSOR_CONF_SIZE);
		if (ret < 0)
			return ret;
#else
		sense_conf[0] = sense_conf[1] = sense_conf[2] = 128;
#endif
		if (copy_to_user(argp, sense_conf, AKM_SENSOR_CONF_SIZE)) {
			pr_debug("copy_to_user failed.");
			return -EFAULT;
		}
#if defined(AKM_Device_AK8963) || defined(AKM_Device_AK09911)
#ifdef AKM_Device_AK8963
		ret = AKECS_SetMode(AK8963_MODE_POWERDOWN);
#else
		ret = AKECS_SetMode(AK09911_MODE_POWERDOWN);
#endif
		if (ret < 0)
			return ret;
#endif

		break;

	case ECS_IOCTL_SET_MODE:
		/* AKMFUNC("ECS_IOCTL_SET_MODE"); */
		if (argp == NULL) {
			pr_debug("invalid argument.");
			return -EINVAL;
		}
		if (copy_from_user(&mode, argp, sizeof(mode))) {
			pr_debug("copy_from_user failed.");
			return -EFAULT;
		}
		ret = AKECS_SetMode(mode); /* MATCH command from AKMD PART */
		if (ret < 0)
			return ret;

		break;

	case ECS_IOCTL_GETDATA:
		/* AKMFUNC("ECS_IOCTL_GETDATA"); */
		ret = AKECS_GetData(sData, SENSOR_DATA_SIZE);
		if (ret < 0)
			return ret;

		if (copy_to_user(argp, sData, sizeof(sData))) {
			pr_debug("copy_to_user failed.");
			return -EFAULT;
		}
		break;

	case ECS_IOCTL_SET_YPR_09915:
		/* AKMFUNC("ECS_IOCTL_SET_YPR"); */
		if (argp == NULL) {
			pr_debug("invalid argument.");
			return -EINVAL;
		}
		if (copy_from_user(value, argp, sizeof(value))) {
			pr_debug("copy_from_user failed.");
			return -EFAULT;
		}
		AKECS_SaveData(value);
		break;

	case ECS_IOCTL_GET_OPEN_STATUS:
		/* AKMFUNC("IOCTL_GET_OPEN_STATUS"); */
		status = AKECS_GetOpenStatus();
		/* pr_debug("AKECS_GetOpenStatus returned (%d)", status); */
		if (copy_to_user(argp, &status, sizeof(status))) {
			pr_debug("copy_to_user failed.");
			return -EFAULT;
		}
		break;

	case ECS_IOCTL_GET_CLOSE_STATUS:
		/* AKMFUNC("IOCTL_GET_CLOSE_STATUS"); */
		status = AKECS_GetCloseStatus();
		/* pr_debug("AKECS_GetCloseStatus returned (%d)", status); */
		if (copy_to_user(argp, &status, sizeof(status))) {
			pr_debug("copy_to_user failed.");
			return -EFAULT;
		}
		break;

	case ECS_IOCTL_GET_OSENSOR_STATUS:
		/* AKMFUNC("ECS_IOCTL_GET_OSENSOR_STATUS"); */
		status = atomic_read(&o_flag);
		if (copy_to_user(argp, &status, sizeof(status))) {
			pr_debug("copy_to_user failed.");
			return -EFAULT;
		}
		break;

	case ECS_IOCTL_GET_DELAY_09915:
		/* AKMFUNC("IOCTL_GET_DELAY"); */
		delay[0] = (int)akmd_delay * 1000000;
		delay[1] = (int)akmd_delay * 1000000;
		delay[2] = (int)akmd_delay * 1000000;
		if (copy_to_user(argp, delay, sizeof(delay))) {
			pr_debug("copy_to_user failed.");
			return -EFAULT;
		}
		break;

	case ECS_IOCTL_GET_LAYOUT_09915:
		layout = atomic_read(&data->layout);
		pr_debug("layout=%d\r\n", layout);
		if (copy_to_user(argp, &layout, sizeof(char))) {
			pr_debug("copy_to_user failed.");
			return -EFAULT;
		}
		break;

	case MSENSOR_IOCTL_READ_CHIPINFO:
		if (argp == NULL) {
			pr_err("IO parameter pointer is NULL!\r\n");
			break;
		}

		akm09915_ReadChipInfo(buff, AKM09915_BUFSIZE);
		if (copy_to_user(argp, buff, strlen(buff) + 1))
			return -EFAULT;

		break;

	case MSENSOR_IOCTL_READ_SENSORDATA:
		if (argp == NULL) {
			pr_err("IO parameter pointer is NULL!\r\n");
			break;
		}

		AKECS_GetRawData(buff, AKM09915_BUFSIZE);

		if (copy_to_user(argp, buff, strlen(buff) + 1))
			return -EFAULT;

		break;

	case MSENSOR_IOCTL_SENSOR_ENABLE:
		if (argp == NULL) {
			pr_err("IO parameter pointer is NULL!\r\n");
			break;
		}
		if (copy_from_user(&enable, argp, sizeof(enable))) {
			pr_debug("copy_from_user failed.");
			return -EFAULT;
		}
		pr_debug("MSENSOR_IOCTL_SENSOR_ENABLE enable=%d!\r\n", enable);
		factory_mode = 1;
		if (enable == 1) {
			atomic_set(&o_flag, 1);
			atomic_set(&open_flag, 1);
		} else {
			atomic_set(&o_flag, 0);
			if (atomic_read(&m_flag) == 0)
				atomic_set(&open_flag, 0);
		}
		wake_up(&open_wq);

		break;

	case MSENSOR_IOCTL_READ_FACTORY_SENSORDATA:
		if (argp == NULL) {
			pr_err("IO parameter pointer is NULL!\r\n");
			break;
		}

		/* AKECS_GetRawData(buff, AKM09911_BUFSIZE); */
		osensor_data = (struct hwm_sensor_data *)buff;
		mutex_lock(&sensor_data_mutex);

		osensor_data->values[0] = sensor_data[13] * CONVERT_O;
		osensor_data->values[1] = sensor_data[14] * CONVERT_O;
		osensor_data->values[2] = sensor_data[15] * CONVERT_O;
		osensor_data->status = sensor_data[8];
		osensor_data->value_divide = CONVERT_O_DIV;

		mutex_unlock(&sensor_data_mutex);

		sprintf(buff, "%x %x %x %x %x", osensor_data->values[0],
			osensor_data->values[1], osensor_data->values[2],
			osensor_data->status, osensor_data->value_divide);
		if (copy_to_user(argp, buff, strlen(buff) + 1))
			return -EFAULT;

		break;

	default:
		pr_err("%s not supported = 0x%04x", __func__, cmd);
		return -ENOIOCTLCMD;
	}

	return 0;
}

#ifdef CONFIG_COMPAT
static long akm09915_compat_ioctl(struct file *file, unsigned int cmd,
				  unsigned long arg)
{
	long ret;
	void __user *arg32 = compat_ptr(arg);

	if (!file->f_op || !file->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {
	case COMPAT_ECS_IOCTL_WRITE:
		if (arg32 == NULL) {
			pr_debug("invalid argument.");
			return -EINVAL;
		}

		ret = file->f_op->unlocked_ioctl(file, ECS_IOCTL_WRITE,
						 (unsigned long)arg32);
		if (ret) {
			pr_debug("ECS_IOCTL_WRITE unlocked_ioctl failed.");
			return ret;
		}

		break;

	case COMPAT_ECS_IOCTL_RESET:
		ret = file->f_op->unlocked_ioctl(file, ECS_IOCTL_RESET,
						 (unsigned long)arg32);
		if (ret) {
			pr_debug("ECS_IOCTL_RESET unlocked_ioctl failed.");
			return ret;
		}
		break;

	case COMPAT_ECS_IOCTL_READ:
		if (arg32 == NULL) {
			pr_debug("invalid argument.");
			return -EINVAL;
		}

		ret = file->f_op->unlocked_ioctl(file, ECS_IOCTL_READ,
						 (unsigned long)arg32);
		if (ret) {
			pr_debug("ECS_IOCTL_WRITE unlocked_ioctl failed.");
			return ret;
		}

		break;

	case COMPAT_ECS_IOCTL_GET_INFO:
		if (arg32 == NULL) {
			pr_debug("invalid argument.");
			return -EINVAL;
		}

		ret = file->f_op->unlocked_ioctl(file, ECS_IOCTL_GET_INFO,
						 (unsigned long)(arg32));
		if (ret) {
			pr_debug("ECS_IOCTL_GET_INFO unlocked_ioctl failed.");
			return ret;
		}
		break;

	case COMPAT_ECS_IOCTL_GET_CONF:
		if (arg32 == NULL) {
			pr_debug("invalid argument.");
			return -EINVAL;
		}

		ret = file->f_op->unlocked_ioctl(file, ECS_IOCTL_GET_CONF,
						 (unsigned long)(arg32));
		if (ret) {
			pr_debug("ECS_IOCTL_GET_CONF unlocked_ioctl failed.");
			return ret;
		}
		break;

	case COMPAT_ECS_IOCTL_SET_MODE:
		if (arg32 == NULL) {
			pr_debug("invalid argument.");
			return -EINVAL;
		}

		ret = file->f_op->unlocked_ioctl(file, ECS_IOCTL_SET_MODE,
						 (unsigned long)(arg32));
		if (ret) {
			pr_debug("ECS_IOCTL_SET_MODE unlocked_ioctl failed.");
			return ret;
		}
		break;

	case COMPAT_ECS_IOCTL_GETDATA:
		ret = file->f_op->unlocked_ioctl(file, ECS_IOCTL_GETDATA,
						 (unsigned long)(arg32));
		if (ret) {
			pr_debug("ECS_IOCTL_GETDATA unlocked_ioctl failed.");
			return ret;
		}

		break;

	case COMPAT_ECS_IOCTL_SET_YPR_09911:
		ret = file->f_op->unlocked_ioctl(file, ECS_IOCTL_SET_YPR_09915,
						 (unsigned long)(arg32));
		if (ret) {
			pr_debug(
				"ECS_IOCTL_SET_YPR_09915 unlocked_ioctl failed.");
			return ret;
		}

		break;

	case COMPAT_ECS_IOCTL_GET_OPEN_STATUS:
		ret = file->f_op->unlocked_ioctl(file,
						 ECS_IOCTL_GET_OPEN_STATUS,
						 (unsigned long)(arg32));
		if (ret) {
			pr_debug(
				"ECS_IOCTL_GET_OPEN_STATUS unlocked_ioctl failed.");
			return ret;
		}

		break;

	case COMPAT_ECS_IOCTL_GET_CLOSE_STATUS:
		ret = file->f_op->unlocked_ioctl(file,
						 ECS_IOCTL_GET_CLOSE_STATUS,
						 (unsigned long)(arg32));
		if (ret) {
			pr_debug(
				"ECS_IOCTL_GET_CLOSE_STATUS unlocked_ioctl failed.");
			return ret;
		}

		break;

	case COMPAT_ECS_IOCTL_GET_OSENSOR_STATUS:
		ret = file->f_op->unlocked_ioctl(file,
						 ECS_IOCTL_GET_OSENSOR_STATUS,
						 (unsigned long)(arg32));
		if (ret) {
			pr_debug(
				"ECS_IOCTL_GET_OSENSOR_STATUS unlocked_ioctl failed.");
			return ret;
		}

		break;

	case COMPAT_ECS_IOCTL_GET_DELAY_09911:
		ret = file->f_op->unlocked_ioctl(file,
						 ECS_IOCTL_GET_DELAY_09915,
						 (unsigned long)(arg32));
		if (ret) {
			pr_debug(
				"ECS_IOCTL_GET_DELAY_09915 unlocked_ioctl failed.");
			return ret;
		}

		break;

	case COMPAT_ECS_IOCTL_GET_LAYOUT_09911:
		ret = file->f_op->unlocked_ioctl(
			file, ECS_IOCTL_GET_LAYOUT_09915, (unsigned long)arg32);
		if (ret) {
			pr_debug(
				"ECS_IOCTL_GET_LAYOUT_09915 unlocked_ioctl failed.");
			return ret;
		}

		break;

	case COMPAT_MSENSOR_IOCTL_READ_CHIPINFO:
		ret = file->f_op->unlocked_ioctl(file,
						 MSENSOR_IOCTL_READ_CHIPINFO,
						 (unsigned long)arg32);
		if (ret) {
			pr_debug(
				"MSENSOR_IOCTL_READ_CHIPINFO unlocked_ioctl failed.");
			return ret;
		}

		break;

	case COMPAT_MSENSOR_IOCTL_READ_SENSORDATA:
		ret = file->f_op->unlocked_ioctl(file,
						 MSENSOR_IOCTL_READ_SENSORDATA,
						 (unsigned long)arg32);
		if (ret) {
			pr_debug(
				"MSENSOR_IOCTL_READ_SENSORDATA unlocked_ioctl failed.");
			return ret;
		}

		break;

	case COMPAT_MSENSOR_IOCTL_SENSOR_ENABLE:
		if (arg32 == NULL) {
			pr_debug("invalid argument.");
			return -EINVAL;
		}

		ret = file->f_op->unlocked_ioctl(file,
						 MSENSOR_IOCTL_SENSOR_ENABLE,
						 (unsigned long)(arg32));
		if (ret) {
			pr_debug(
				"MSENSOR_IOCTL_SENSOR_ENABLE unlocked_ioctl failed.");
			return ret;
		}

		break;

	case COMPAT_MSENSOR_IOCTL_READ_FACTORY_SENSORDATA:
		if (arg32 == NULL) {
			pr_debug("invalid argument.");
			return -EINVAL;
		}

		ret = file->f_op->unlocked_ioctl(
			file, MSENSOR_IOCTL_READ_FACTORY_SENSORDATA,
			(unsigned long)(arg32));
		if (ret) {
			pr_debug(
				"MSENSOR_IOCTL_READ_FACTORY_SENSORDATA unlocked_ioctl failed.");
			return ret;
		}
		break;

	default:
		pr_debug("%s not supported = 0x%04x", __func__, cmd);
		return -ENOIOCTLCMD;
	}
	return 0;
}
#endif

/*----------------------------------------------------------------------------*/
static const struct file_operations akm09915_fops = {
	.owner = THIS_MODULE,
	.open = akm09915_open,
	.release = akm09915_release,
	/* .unlocked_ioctl = akm09915_ioctl, */
	.unlocked_ioctl = akm09915_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = akm09915_compat_ioctl,
#endif
};

/*----------------------------------------------------------------------------*/
static struct miscdevice akm09915_device = {
	.minor = MISC_DYNAMIC_MINOR, .name = "msensor", .fops = &akm09915_fops,
};

/*----------------------------------------------------------------------------*/
int akm09915_operate(void *self, uint32_t command, void *buff_in, int size_in,
		     void *buff_out, int size_out, int *actualout)
{
	int err = 0;
	int value;
	struct hwm_sensor_data *msensor_data;

#if DEBUG
	struct i2c_client *client = this_client;
	struct akm09915_i2c_data *data = i2c_get_clientdata(client);
#endif

#if DEBUG
	if (atomic_read(&data->trace) & AMK_FUN_DEBUG)
		AKMFUNC("akm09915_operate");
#endif
	switch (command) {
	case SENSOR_DELAY:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			pr_err("Set delay parameter error!\n");
			err = -EINVAL;
		} else {
			value = *(int *)buff_in;
			if (value <= 10)
				value = 10;

			akmd_delay = value;
		}
		break;

	case SENSOR_ENABLE:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			pr_err("Enable sensor parameter error!\n");
			err = -EINVAL;
		} else {
			value = *(int *)buff_in;

			if (value == 1) {
				atomic_set(&m_flag, 1);
				atomic_set(&open_flag, 1);
			} else {
				atomic_set(&m_flag, 0);
				if ((atomic_read(&o_flag) == 0))
					atomic_set(&open_flag, 0);
			}
			wake_up(&open_wq);

			/* TODO: turn device into standby or normal mode */
		}
		break;

	case SENSOR_GET_DATA:
		if ((buff_out == NULL) ||
		    (size_out < sizeof(struct hwm_sensor_data))) {
			pr_err("get sensor data parameter error!\n");
			err = -EINVAL;
		} else {
			msensor_data = (struct hwm_sensor_data *)buff_out;
			mutex_lock(&sensor_data_mutex);

			msensor_data->values[0] = sensor_data[5] * CONVERT_M;
			msensor_data->values[1] = sensor_data[6] * CONVERT_M;
			msensor_data->values[2] = sensor_data[7] * CONVERT_M;
			msensor_data->status = sensor_data[8];
			msensor_data->value_divide = CONVERT_M_DIV;

			mutex_unlock(&sensor_data_mutex);
#if DEBUG
			if (atomic_read(&data->trace) & AMK_HWM_DEBUG) {
				pr_debug(
					"Hwm get m-sensor data: %d, %d, %d. divide %d, status %d!\n",
					msensor_data->values[0],
					msensor_data->values[1],
					msensor_data->values[2],
					msensor_data->value_divide,
					msensor_data->status);
			}
#endif
		}
		break;
	default:
		pr_err("msensor operate function no this parameter %d!\n",
			   command);
		err = -1;
		break;
	}

	return err;
}

/*----------------------------------------------------------------------------*/
int akm09915_orientation_operate(void *self, uint32_t command, void *buff_in,
				 int size_in, void *buff_out, int size_out,
				 int *actualout)
{
	int err = 0;
	int value;
	struct hwm_sensor_data *osensor_data;
#if DEBUG
	struct i2c_client *client = this_client;
	struct akm09915_i2c_data *data = i2c_get_clientdata(client);
#endif

#if DEBUG
	if (atomic_read(&data->trace) & AMK_FUN_DEBUG)
		AKMFUNC("akm09915_orientation_operate");
#endif

	switch (command) {
	case SENSOR_DELAY:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			pr_err("Set delay parameter error!\n");
			err = -EINVAL;
		} else {
			value = *(int *)buff_in;
			if (value <= 10)
				value = 10;

			akmd_delay = value;
		}
		break;

	case SENSOR_ENABLE:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			pr_err("Enable sensor parameter error!\n");
			err = -EINVAL;
		} else {
			value = *(int *)buff_in;
			if (mEnabled <= 0) {
				if (value == 1) {
					atomic_set(&o_flag, 1);
					atomic_set(&open_flag, 1);
				}
			} else if (mEnabled == 1) {
				if (!value) {
					atomic_set(&o_flag, 0);
					if (atomic_read(&m_flag) == 0)
						atomic_set(&open_flag, 0);
				}
			}

			if (value) {
				mEnabled++;
				if (mEnabled > 32767)
					mEnabled = 32767;
			} else {
				mEnabled--;
				if (mEnabled < 0)
					mEnabled = 0;
			}
			wake_up(&open_wq);
		}

		break;

	case SENSOR_GET_DATA:
		if ((buff_out == NULL) ||
		    (size_out < sizeof(struct hwm_sensor_data))) {
			pr_err("get sensor data parameter error!\n");
			err = -EINVAL;
		} else {
			osensor_data = (struct hwm_sensor_data *)buff_out;
			mutex_lock(&sensor_data_mutex);
			osensor_data->values[0] = sensor_data[13] * CONVERT_O;
			osensor_data->values[1] = sensor_data[14] * CONVERT_O;
			osensor_data->values[2] = sensor_data[15] * CONVERT_O;
			osensor_data->status = sensor_data[8];
			osensor_data->value_divide = CONVERT_O_DIV;

			mutex_unlock(&sensor_data_mutex);
#if DEBUG
			if (atomic_read(&data->trace) & AMK_HWM_DEBUG) {
				pr_debug(
					"Hwm get o-sensor data: %d, %d, %d. divide %d, status %d!\n",
					osensor_data->values[0],
					osensor_data->values[1],
					osensor_data->values[2],
					osensor_data->value_divide,
					osensor_data->status);
			}
#endif
		}
		break;
	default:
		pr_err("gsensor operate function no this parameter %d!\n",
			   command);
		err = -1;
		break;
	}

	return err;
}

#ifdef AKM_Pseudogyro
/*----------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------*/
int akm09915_gyroscope_operate(void *self, uint32_t command, void *buff_in,
			       int size_in, void *buff_out, int size_out,
			       int *actualout)
{
	int err = 0;
	int value;
	struct hwm_sensor_data *gyrosensor_data;
#if DEBUG
	struct i2c_client *client = this_client;
	struct akm09915_i2c_data *data = i2c_get_clientdata(client);
#endif

#if DEBUG
	if (atomic_read(&data->trace) & AMK_FUN_DEBUG)
		AKMFUNC("akm09915_gyroscope_operate");
#endif

	switch (command) {
	case SENSOR_DELAY:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			pr_err("Set delay parameter error!\n");
			err = -EINVAL;
		} else {
			value = *(int *)buff_in;
			akmd_delay = 10; /* fix to 100Hz */
		}
		break;

	case SENSOR_ENABLE:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			pr_err("Enable sensor parameter error!\n");
			err = -EINVAL;
		} else {
			value = *(int *)buff_in;
			if (mEnabled <= 0) {
				if (value == 1) {
					atomic_set(&o_flag, 1);
					atomic_set(&open_flag, 1);
				}
			} else if (mEnabled == 1) {
				if (!value) {
					atomic_set(&o_flag, 0);
					if (atomic_read(&m_flag) == 0)
						atomic_set(&open_flag, 0);
				}
			}

			if (value) {
				mEnabled++;
				if (mEnabled > 32767)
					mEnabled = 32767;
			} else {
				mEnabled--;
				if (mEnabled < 0)
					mEnabled = 0;
			}

			wake_up(&open_wq);
		}

		break;

	case SENSOR_GET_DATA:
		if ((buff_out == NULL) ||
		    (size_out < sizeof(struct hwm_sensor_data))) {
			pr_err("get sensor data parameter error!\n");
			err = -EINVAL;
		} else {
			gyrosensor_data = (struct hwm_sensor_data *)buff_out;
			mutex_lock(&sensor_data_mutex);

			gyrosensor_data->values[0] =
				sensor_data[9] * CONVERT_Q16;
			gyrosensor_data->values[1] =
				sensor_data[10] * CONVERT_Q16;
			gyrosensor_data->values[2] =
				sensor_data[11] * CONVERT_Q16;
			gyrosensor_data->status = sensor_data[12];
			gyrosensor_data->value_divide = CONVERT_Q16_DIV;

			mutex_unlock(&sensor_data_mutex);
#if DEBUG
			if (atomic_read(&data->trace) & AMK_HWM_DEBUG) {
				pr_debug(
					"Hwm get gyro-sensor data: %d, %d, %d. divide %d, status %d!\n",
					gyrosensor_data->values[0],
					gyrosensor_data->values[1],
					gyrosensor_data->values[2],
					gyrosensor_data->value_divide,
					gyrosensor_data->status);
			}
#endif
		}
		break;
	default:
		pr_err(
			"gyrosensor operate function no this parameter %d!\n",
			command);
		err = -1;
		break;
	}

	return err;
}

/*----------------------------------------------------------------------------*/
int akm09915_rotation_vector_operate(void *self, uint32_t command,
				     void *buff_in, int size_in, void *buff_out,
				     int size_out, int *actualout)
{
	int err = 0;
	int value;
	struct hwm_sensor_data *RV_data;
#if DEBUG
	struct i2c_client *client = this_client;
	struct akm09915_i2c_data *data = i2c_get_clientdata(client);
#endif

#if DEBUG
	if (atomic_read(&data->trace) & AMK_FUN_DEBUG)
		AKMFUNC("akm09915_rotation_vector_operate");
#endif

	switch (command) {
	case SENSOR_DELAY:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			pr_err("Set delay parameter error!\n");
			err = -EINVAL;
		} else {
			value = *(int *)buff_in;
			akmd_delay = 10; /* fix to 100Hz */
		}
		break;

	case SENSOR_ENABLE:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			pr_err("Enable sensor parameter error!\n");
			err = -EINVAL;
		} else {
			value = *(int *)buff_in;
			if (mEnabled <= 0) {
				if (value == 1) {
					atomic_set(&o_flag, 1);
					atomic_set(&open_flag, 1);
				}
			} else if (mEnabled == 1) {
				if (!value) {
					atomic_set(&o_flag, 0);
					if ((atomic_read(&m_flag) == 0))
						atomic_set(&open_flag, 0);
				}
			}

			if (value) {
				mEnabled++;
				if (mEnabled > 32767)
					mEnabled = 32767;
			} else {
				mEnabled--;
				if (mEnabled < 0)
					mEnabled = 0;
			}
			wake_up(&open_wq);
		}

		break;

	case SENSOR_GET_DATA:
		if ((buff_out == NULL) ||
		    (size_out < sizeof(struct hwm_sensor_data))) {
			pr_err("get sensor data parameter error!\n");
			err = -EINVAL;
		} else {
			RV_data = (struct hwm_sensor_data *)buff_out;
			mutex_lock(&sensor_data_mutex);

			RV_data->values[0] = sensor_data[22] * CONVERT_Q16;
			RV_data->values[1] = sensor_data[23] * CONVERT_Q16;
			RV_data->values[2] = sensor_data[24] * CONVERT_Q16;
			RV_data->status = 0; /* sensor_data[19];  fix w-> 0 w */
			RV_data->value_divide = CONVERT_Q16_DIV;

			mutex_unlock(&sensor_data_mutex);
#if DEBUG
			if (atomic_read(&data->trace) & AMK_HWM_DEBUG) {
				pr_debug(
					"Hwm get rv-sensor data: %d, %d, %d. divide %d, status %d!\n",
					RV_data->values[0], RV_data->values[1],
					RV_data->values[2],
					RV_data->value_divide, RV_data->status);
			}
#endif
		}
		break;
	default:
		pr_err("RV  operate function no this parameter %d!\n",
			   command);
		err = -1;
		break;
	}

	return err;
}

/*----------------------------------------------------------------------------*/
int akm09915_gravity_operate(void *self, uint32_t command, void *buff_in,
			     int size_in, void *buff_out, int size_out,
			     int *actualout)
{
	int err = 0;
	int value;
	struct hwm_sensor_data *gravity_data;
#if DEBUG
	struct i2c_client *client = this_client;
	struct akm09915_i2c_data *data = i2c_get_clientdata(client);
#endif

#if DEBUG
	if (atomic_read(&data->trace) & AMK_FUN_DEBUG)
		AKMFUNC("akm09915_gravity_operate");
#endif

	switch (command) {
	case SENSOR_DELAY:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			pr_err("Set delay parameter error!\n");
			err = -EINVAL;
		} else {
			value = *(int *)buff_in;
			if (value <= 10)
				value = 10;
			akmd_delay = value;
		}
		break;

	case SENSOR_ENABLE:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			pr_err("Enable sensor parameter error!\n");
			err = -EINVAL;
		} else {
			value = *(int *)buff_in;
			if (mEnabled <= 0) {
				if (value == 1) {
					atomic_set(&o_flag, 1);
					atomic_set(&open_flag, 1);
				}
			} else if (mEnabled == 1) {
				if (!value) {
					atomic_set(&o_flag, 0);
					if (atomic_read(&m_flag) == 0)
						atomic_set(&open_flag, 0);
				}
			}

			if (value) {
				mEnabled++;
				if (mEnabled > 32767)
					mEnabled = 32767;
			} else {
				mEnabled--;
				if (mEnabled < 0)
					mEnabled = 0;
			}
			wake_up(&open_wq);
		}

		break;

	case SENSOR_GET_DATA:
		if ((buff_out == NULL) ||
		    (size_out < sizeof(struct hwm_sensor_data))) {
			pr_err("get sensor data parameter error!\n");
			err = -EINVAL;
		} else {
			gravity_data = (struct hwm_sensor_data *)buff_out;
			mutex_lock(&sensor_data_mutex);

			gravity_data->values[0] = sensor_data[16] * CONVERT_Q16;
			gravity_data->values[1] = sensor_data[17] * CONVERT_Q16;
			gravity_data->values[2] = sensor_data[18] * CONVERT_Q16;
			gravity_data->status = sensor_data[4];
			gravity_data->value_divide = CONVERT_Q16_DIV;

			mutex_unlock(&sensor_data_mutex);
#if DEBUG
			if (atomic_read(&data->trace) & AMK_HWM_DEBUG) {
				pr_debug(
					"Hwm get gravity-sensor data: %d, %d, %d. divide %d, status %d!\n",
					gravity_data->values[0],
					gravity_data->values[1],
					gravity_data->values[2],
					gravity_data->value_divide,
					gravity_data->status);
			}
#endif
		}
		break;
	default:
		pr_err("gravity operate function no this parameter %d!\n",
			   command);
		err = -1;
		break;
	}

	return err;
}

/*----------------------------------------------------------------------------*/
int akm09915_linear_accelration_operate(void *self, uint32_t command,
					void *buff_in, int size_in,
					void *buff_out, int size_out,
					int *actualout)
{
	int err = 0;
	int value;
	struct hwm_sensor_data *LA_data;
#if DEBUG
	struct i2c_client *client = this_client;
	struct akm09915_i2c_data *data = i2c_get_clientdata(client);
#endif

#if DEBUG
	if (atomic_read(&data->trace) & AMK_FUN_DEBUG)
		AKMFUNC("akm09915_linear_accelration_operate");
#endif

	switch (command) {
	case SENSOR_DELAY:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			pr_err("Set delay parameter error!\n");
			err = -EINVAL;
		} else {
			value = *(int *)buff_in;
			if (value <= 10)
				value = 10;
			akmd_delay = value;
		}
		break;

	case SENSOR_ENABLE:
		if ((buff_in == NULL) || (size_in < sizeof(int))) {
			pr_err("Enable sensor parameter error!\n");
			err = -EINVAL;
		} else {
			value = *(int *)buff_in;
			if (mEnabled <= 0) {
				if (value == 1) {
					atomic_set(&o_flag, 1);
					atomic_set(&open_flag, 1);
				}
			} else if (mEnabled == 1) {
				if (!value) {
					atomic_set(&o_flag, 0);
					if ((atomic_read(&m_flag) == 0))
						atomic_set(&open_flag, 0);
				}
			}

			if (value) {
				mEnabled++;
				if (mEnabled > 32767)
					mEnabled = 32767;
			} else {
				mEnabled--;
				if (mEnabled < 0)
					mEnabled = 0;
			}
			wake_up(&open_wq);
		}

		break;

	case SENSOR_GET_DATA:
		if ((buff_out == NULL) ||
		    (size_out < sizeof(struct hwm_sensor_data))) {
			pr_err("get sensor data parameter error!\n");
			err = -EINVAL;
		} else {
			LA_data = (struct hwm_sensor_data *)buff_out;
			mutex_lock(&sensor_data_mutex);

			LA_data->values[0] = sensor_data[19] * CONVERT_Q16;
			LA_data->values[1] = sensor_data[20] * CONVERT_Q16;
			LA_data->values[2] = sensor_data[21] * CONVERT_Q16;
			LA_data->status = sensor_data[4];
			LA_data->value_divide = CONVERT_Q16_DIV;

			mutex_unlock(&sensor_data_mutex);
#if DEBUG
			if (atomic_read(&data->trace) & AMK_HWM_DEBUG) {
				pr_debug(
					"Hwm get LA-sensor data: %d, %d, %d. divide %d, status %d!\n",
					LA_data->values[0], LA_data->values[1],
					LA_data->values[2],
					LA_data->value_divide, LA_data->status);
			}
#endif
		}
		break;
	default:
		pr_err(
			"linear_accelration operate function no this parameter %d!\n",
			command);
		err = -1;
		break;
	}

	return err;
}

#endif

/*----------------------------------------------------------------------------*/
static int akm09915_suspend(struct device *dev)
{
	return 0;
}

/*----------------------------------------------------------------------------*/
static int akm09915_resume(struct device *dev)
{
	return 0;
}

/*----------------------------------------------------------------------------*/
static int akm09915_i2c_detect(struct i2c_client *client,
			       struct i2c_board_info *info)
{
	strcpy(info->type, AKM09915_DEV_NAME);
	return 0;
}

static int akm09915_enable(int en)
{
	int value = 0;
	int err = 0;

	value = en;
	factory_mode = 1;
	if (value == 1) {
		atomic_set(&m_flag, 1);
		atomic_set(&open_flag, 1);

		err = AKECS_SetMode(AK09915_MODE_SNG_MEASURE);
		if (err < 0) {
			pr_err("%s:AKECS_SetMode Error.\n", __func__);
			return err;
		}
	} else {
		atomic_set(&m_flag, 0);
		if (atomic_read(&o_flag) == 0) {
			atomic_set(&open_flag, 0);
			err = AKECS_SetMode(AK09915_MODE_POWERDOWN);
			if (err < 0) {
				pr_err("%s:AKECS_SetMode Error.\n",
					   __func__);
				return err;
			}
		}
	}
	wake_up(&open_wq);
	return err;
}

static int akm09915_set_delay(u64 ns)
{
	int value = 0;

	value = (int)ns / 1000 / 1000;

	if (value <= 10)
		akmd_delay = 10;
	else
		akmd_delay = value;

	return 0;
}

static int akm09915_batch(int flag, int64_t samplingPeriodNs,
			  int64_t maxBatchReportLatencyNs)
{
	return 0;
}

static int akm09915_flush(void)
{
	return mag_flush_report();
}

static int akm09915_open_report_data(int open)
{
	return 0;
}

static int akm09915_get_data(int *x, int *y, int *z, int *status)
{
	mutex_lock(&sensor_data_mutex);

	*x = sensor_data[5] * CONVERT_M;
	*y = sensor_data[6] * CONVERT_M;
	*z = sensor_data[7] * CONVERT_M;
	*status = sensor_data[8];

	mutex_unlock(&sensor_data_mutex);
	return 0;
}

/*----------------------------------------------------------------------------*/
static int akm09915_i2c_probe(struct i2c_client *client,
			      const struct i2c_device_id *id)
{
	int err = 0;
	struct i2c_client *new_client;
	struct akm09915_i2c_data *data;
	struct mag_control_path ctl = {0};
	struct mag_data_path mag_data = {0};

	pr_debug("akm09915_i2c_probe\n");

	data = kzalloc(sizeof(struct akm09915_i2c_data), GFP_KERNEL);
	if (!data) {
		err = -ENOMEM;
		goto exit;
	}

	/* get customization and power on */
	err = get_mag_dts_func(client->dev.of_node, &data->hw);
	if (err < 0) {
		pr_err("get dts info fail\n");
		return 0;
	}
	atomic_set(&data->layout, data->hw.direction);
	atomic_set(&data->trace, 0);
	mutex_init(&sense_data_mutex);
	mutex_init(&sensor_data_mutex);
	/* init_waitqueue_head(&data_ready_wq); */
	init_waitqueue_head(&open_wq);
	data->client = client;
	new_client = data->client;
	i2c_set_clientdata(new_client, data);
	this_client = new_client;

	/* Check connection */
	err = AKECS_CheckDevice();
	if (err < 0) {
		pr_err(
			"AKM09915 akm09915_probe: check device connect error\n");
		goto exit_init_failed;
	}

	/* Register sysfs attribute */
	err = akm09915_create_attr(
		&(akm09915_init_info.platform_diver_addr->driver));
	if (err) {
		pr_err("create attribute err = %d\n", err);
		goto exit_sysfs_create_group_failed;
	}

	err = misc_register(&akm09915_device);
	if (err) {
		pr_err("akm09915_device register failed\n");
		goto exit_misc_device_register_failed;
	}

	ctl.is_use_common_factory = false;
	ctl.enable = akm09915_enable;
	ctl.set_delay = akm09915_set_delay;
	ctl.open_report_data = akm09915_open_report_data;
	ctl.batch = akm09915_batch;
	ctl.flush = akm09915_flush;
	ctl.is_report_input_direct = false;
	ctl.is_support_batch = data->hw.is_batch_supported;

	err = mag_register_control_path(&ctl);
	if (err) {
		pr_err("register mag control path err\n");
		goto exit_kfree;
	}

	// mag_data.div = CONVERT_M_DIV;
	// mag_data.div_o = CONVERT_O_DIV;
	mag_data.get_data = akm09915_get_data;

	err = mag_register_data_path(&mag_data);
	if (err) {
		pr_err("register data control path err\n");
		goto exit_kfree;
	}

	pr_debug("%s: OK\n", __func__);
	akm09915_init_flag = 1;
	return 0;

exit_sysfs_create_group_failed:
exit_init_failed:
exit_misc_device_register_failed:
exit_kfree:
	kfree(data);
exit:
	pr_err("%s: err = %d\n", __func__, err);
	akm09915_init_flag = -1;
	return err;
}

/*----------------------------------------------------------------------------*/
static int akm09915_i2c_remove(struct i2c_client *client)
{
	int err;

	err = akm09915_delete_attr(
		&(akm09915_init_info.platform_diver_addr->driver));
	if (err)
		pr_err("akm09915_delete_attr fail: %d\n", err);

	this_client = NULL;
	i2c_unregister_device(client);
	kfree(i2c_get_clientdata(client));
	misc_deregister(&akm09915_device);
	return 0;
}

/*----------------------------------------------------------------------------*/
static int akm09915_remove(void)
{
	atomic_set(&dev_open_count, 0);
	i2c_del_driver(&akm09915_i2c_driver);
	return 0;
}

static int akm09915_local_init(void)
{
	if (i2c_add_driver(&akm09915_i2c_driver)) {
		pr_err("i2c_add_driver error\n");
		return -1;
	}
	if (-1 == akm09915_init_flag)
		return -1;
	return 0;
}

/*----------------------------------------------------------------------------*/
static int __init akm09915_init(void)
{
	mag_driver_add(&akm09915_init_info);
	return 0;
}

/*----------------------------------------------------------------------------*/
static void __exit akm09915_exit(void)
{
}

/*----------------------------------------------------------------------------*/
module_init(akm09915_init);
module_exit(akm09915_exit);

MODULE_AUTHOR("MTK");
MODULE_DESCRIPTION("AKM09915 compass driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);
