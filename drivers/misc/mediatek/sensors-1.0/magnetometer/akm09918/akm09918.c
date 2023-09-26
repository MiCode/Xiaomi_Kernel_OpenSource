/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/* akm09918.c - akm09918 compass driver
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
#define pr_fmt(fmt) "[AKM09918] " fmt

#include "cust_mag.h"
#include "akm09918.h"
#include "mag.h"

#define DEBUG 0
#define AKM09918_DEV_NAME	"akm09918"
#define DRIVER_VERSION	 "1.0.1"
#define AKM09918_RETRY_COUNT	10
#define AKM09918_DEFAULT_DELAY	100

static DECLARE_WAIT_QUEUE_HEAD(open_wq);

#define AKM_CONTINUOUS 1

#if AKM_CONTINUOUS
#define AKM_CONTINUOUS_MODE
#endif

static short akmd_delay = AKM09918_DEFAULT_DELAY;
static int factory_mode;
static int akm09918_init_flag;
static struct i2c_client *this_client;
static int8_t akm_device;

static uint8_t akm_fuse[3] = {0};
/*----------------------------------------------------------------------------*/
static const struct i2c_device_id akm09918_i2c_id[] = {
					{AKM09918_DEV_NAME, 0}, {} };

/* Maintain  cust info here */
struct mag_hw mag_cust;
static struct mag_hw *hw = &mag_cust;

/* For  driver get cust info */
struct mag_hw *get_cust_mag(void)
{
	return &mag_cust;
}

/*----------------------------------------------------------------------------*/
static int akm09918_i2c_probe(struct i2c_client *client,
					const struct i2c_device_id *id);
static int akm09918_i2c_remove(struct i2c_client *client);
static int akm09918_i2c_detect(struct i2c_client *client,
					struct i2c_board_info *info);
static int akm09918_suspend(struct device *dev);
static int akm09918_resume(struct device *dev);
static int akm09918_local_init(void);
static int akm09918_remove(void);
static int akm09918_flush(void);

static struct mag_init_info akm09918_init_info = {
	.name = "akm09918",
	.init = akm09918_local_init,
	.uninit = akm09918_remove,
};


/*----------------------------------------------------------------------------*/
enum {
	AMK_FUN_DEBUG = 0x01,
	AMK_DATA_DEBUG = 0X02,
	AMK_HWM_DEBUG = 0X04,
	AMK_CTR_DEBUG = 0X08,
	AMK_I2C_DEBUG = 0x10,
} AMK_TRC;


/*----------------------------------------------------------------------------*/
struct akm09918_i2c_data {
	struct i2c_client *client;
	struct mag_hw *hw;
	atomic_t layout;
	atomic_t trace;
	struct hwmsen_convert cvt;
	bool flush;
	bool enable;
};
/*----------------------------------------------------------------------------*/
#ifdef CONFIG_OF
static const struct of_device_id mag_of_match[] = {
	{.compatible = "mediatek,msensor"},
	{},
};
#endif

#ifdef CONFIG_PM_SLEEP
static const struct dev_pm_ops akm09918_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(akm09918_suspend, akm09918_resume)
};
#endif

static struct i2c_driver akm09918_i2c_driver = {
	.driver = {
		   .name = AKM09918_DEV_NAME,
#ifdef CONFIG_PM_SLEEP
		.pm    = &akm09918_pm_ops,
#endif
#ifdef CONFIG_OF
		   .of_match_table = mag_of_match,
#endif
		   },
	.probe = akm09918_i2c_probe,
	.remove = akm09918_i2c_remove,
	.detect = akm09918_i2c_detect,
	.id_table = akm09918_i2c_id,
};

/* akm_map value rang is 1-8, so akm_map[0] is invalid*/
static struct hwmsen_convert akm_map[] = {
	{ { 0, 0, 0}, {0, 0, 0} },
	{ { 1, 1, 1}, {0, 1, 2} },
	{ { 1, -1, 1}, {1, 0, 2} },
	{ {-1, -1, 1}, {0, 1, 2} },
	{ {-1, 1, 1}, {1, 0, 2} },

	{ {-1, 1, -1}, {0, 1, 2} },
	{ { 1, 1, -1}, {1, 0, 2} },
	{ { 1, -1, -1}, {0, 1, 2} },
	{ {-1, -1, -1}, {1, 0, 2} },

};

/*----------------------------------------------------------------------------*/
static atomic_t dev_open_count;
/*----------------------------------------------------------------------------*/

static DEFINE_MUTEX(akm09918_i2c_mutex);
#ifndef CONFIG_MTK_I2C_EXTENSION
static int mag_i2c_read_block(struct i2c_client *client,
					u8 addr, u8 *data, u8 len)
{
	int err = 0;
	u8 beg = addr;
	struct i2c_msg msgs[2] = { {0}, {0} };

	if (!client) {
		return -EINVAL;
	} else if (len > C_I2C_FIFO_SIZE) {
		pr_err(" length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		return -EINVAL;
	}
	mutex_lock(&akm09918_i2c_mutex);

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
		pr_err_ratelimited("i2c_transfer error: (%d %p %d) %d\n",
			addr, data, len, err);
		err = -EIO;
	} else {
		err = 0;
	}
	mutex_unlock(&akm09918_i2c_mutex);
	return err;

}

static int mag_i2c_write_block(struct i2c_client *client,
					u8 addr, u8 *data, u8 len)
{
/*address also occupies one byte, the maximum length for write is 7 bytes */
	int err = 0, num = 0;
	char buf[C_I2C_FIFO_SIZE];
	unsigned int idx = 0;

	err = 0;
	mutex_lock(&akm09918_i2c_mutex);
	if (!client) {
		mutex_unlock(&akm09918_i2c_mutex);
		return -EINVAL;
	} else if (len >= C_I2C_FIFO_SIZE) {
		mutex_unlock(&akm09918_i2c_mutex);
		pr_err(" length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		return -EINVAL;
	}

	num = 0;
	buf[num++] = addr;
	for (idx = 0; idx < len; idx++)
		buf[num++] = data[idx];

	err = i2c_master_send(client, buf, num);
	if (err < 0) {
		mutex_unlock(&akm09918_i2c_mutex);
		pr_err_ratelimited("send command error!!\n");
		return -EFAULT;
	}
	mutex_unlock(&akm09918_i2c_mutex);
	return err;
}
#endif
static void akm09918_power(struct mag_hw *hw, unsigned int on)
{
}

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
	struct akm09918_i2c_data *data = i2c_get_clientdata(client);
	char addr = rxData[0];
#endif

	/* Caller should check parameter validity. */
	if ((rxData == NULL) || (length < 1))
		return -EINVAL;

	mutex_lock(&akm09918_i2c_mutex);
	for (loop_i = 0; loop_i < AKM09918_RETRY_COUNT; loop_i++) {
		this_client->addr = this_client->addr & I2C_MASK_FLAG;
		this_client->addr = this_client->addr | I2C_WR_FLAG;
		if (i2c_master_send(this_client,
			(const char *)rxData, ((length << 0X08) | 0X01)))
			break;
		mdelay(10);
	}

	if (loop_i >= AKM09918_RETRY_COUNT) {
		mutex_unlock(&akm09918_i2c_mutex);
		pr_err("%s retry over %d\n", __func__, AKM09918_RETRY_COUNT);
		return -EIO;
	}
	mutex_unlock(&akm09918_i2c_mutex);
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
	struct akm09918_i2c_data *data = i2c_get_clientdata(client);
#endif

	/* Caller should check parameter validity. */
	if ((txData == NULL) || (length < 2))
		return -EINVAL;
	mutex_lock(&akm09918_i2c_mutex);
	this_client->addr = this_client->addr & I2C_MASK_FLAG;
	for (loop_i = 0; loop_i < AKM09918_RETRY_COUNT; loop_i++) {
		if (i2c_master_send(this_client,
			(const char *)txData, length) > 0)
			break;
		mdelay(10);
	}

	if (loop_i >= AKM09918_RETRY_COUNT) {
		mutex_unlock(&akm09918_i2c_mutex);
		pr_err("%s retry over %d\n", __func__, AKM09918_RETRY_COUNT);
		return -EIO;
	}
	mutex_unlock(&akm09918_i2c_mutex);
#if DEBUG
	if (atomic_read(&data->trace) & AMK_I2C_DEBUG) {
		pr_debug("TxData: len=%02x, addr=%02x\n  data=",
			length, txData[0]);
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
	buffer[0] = AKM_REG_MODE;
	buffer[1] = AKM_MODE_SNG_MEASURE;

	/* Set data */
	return AKI2C_TxData(buffer, 2);
}

#ifdef AKM_CONTINUOUS_MODE
static long AKECS_SetMode_CntMeasure(char mode)
{
	char buffer[2];
	/* Set measure mode */
	buffer[0] = AKM_REG_MODE;
	buffer[1] = mode;
	/* Set data */
	return AKI2C_TxData(buffer, 2);
}
#endif

static long AKECS_SetMode_SelfTest(void)
{
	char buffer[2];
	/* Set selftest mode */
	buffer[0] = AKM_REG_MODE;
	buffer[1] = AKM_MODE_SELF_TEST;
	/* Set data */
	return AKI2C_TxData(buffer, 2);
}

static long AKECS_SetMode_FUSEAccess(void)
{
	char buffer[2];
	/* Set FuseROM Access mode */
	buffer[0] = AKM_REG_MODE;
	buffer[1] = AKM_MODE_FUSE_ACCESS;
	/* Set data */
	return AKI2C_TxData(buffer, 2);
}

static int AKECS_SetMode_PowerDown(void)
{
	char buffer[2];
	/* Set powerdown mode */
	buffer[0] = AKM_REG_MODE;
	buffer[1] = AKM_MODE_POWERDOWN;
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
		/* Set reset mode */
		buffer[0] = AKM_REG_RESET;
		buffer[1] = 0x01;
		err = AKI2C_TxData(buffer, 2);
		if (err < 0)
			pr_debug("%s: Can not set SRST bit.\n", __func__);
		else
			pr_debug("Soft reset is done.\n");
	}

	/* Device will be accessible 300 us after */
	udelay(300);		/* 100 */

	return err;
}

static long AKECS_SetMode(char mode)
{
	long ret;

	switch (mode & 0x1F) {
	case AKM_MODE_SNG_MEASURE:
		ret = AKECS_SetMode_SngMeasure();
		break;

	case AKM_MODE_SELF_TEST:
		ret = AKECS_SetMode_SelfTest();
		break;

	case AKM_MODE_FUSE_ACCESS:
		ret = AKECS_SetMode_FUSEAccess();
		break;

	case AKM_MODE_POWERDOWN:
		ret = AKECS_SetMode_PowerDown();
		break;

	default:
		pr_debug("%s: Unknown mode(%d)\n", __func__, mode);
		return -EINVAL;
	}

	/* wait at least 100us after changing mode */
	udelay(100);

	return ret;
}

static int AKECS_ReadFuse(void)
{
	int ret = 0;

	ret = AKECS_SetMode_FUSEAccess();
	if (ret < 0) {
		pr_debug("AKM set read fuse mode fail ret:%d\n", ret);
		return ret;
	}

	akm_fuse[0] = AK0991X_FUSE_ASAX;
	ret = AKI2C_RxData(akm_fuse, 3);
	if (ret < 0) {
		pr_debug("AKM read fuse fail ret:%d\n", ret);
		return ret;
	}
	ret = AKECS_SetMode_PowerDown();
	return ret;
}

static int AKECS_CheckDevice(void)
{
	char buffer[2];
	int ret;

	/* Read WIA1 */
	buffer[0] = AK0991X_REG_WIA1;

	/* Read data */
	ret = AKI2C_RxData(buffer, 2);
	if (ret < 0)
		return ret;

	/* Check read data */
	if (buffer[0] != 0x48)
		return -ENXIO;

	akm_device = buffer[1];
	if ((akm_device == 0x05) || (akm_device == 0x04)) {
		ret = AKECS_ReadFuse();
		if (ret < 0) {
			pr_err("%s AKM09918: read fuse fail\n", __func__);
			return -ENXIO;
		}
	} else if ((akm_device == 0x10) || (akm_device == 0x09) ||
		(akm_device == 0x0b) || (akm_device == 0x0c)) {
		akm_fuse[0] = 0x80;
		akm_fuse[1] = 0x80;
		akm_fuse[2] = 0x80;
	}

	return 0;
}

static int AKECS_AxisInfoToPat(
	const uint8_t axis_order[3],
	const uint8_t axis_sign[3],
	int16_t *pat)
{
	/* check invalid input */
	if ((axis_order[0] < 0) || (axis_order[0] > 2) ||
	   (axis_order[1] < 0) || (axis_order[1] > 2) ||
	   (axis_order[2] < 0) || (axis_order[2] > 2) ||
	   (axis_sign[0] < 0) || (axis_sign[0] > 1) ||
	   (axis_sign[1] < 0) || (axis_sign[1] > 1) ||
	   (axis_sign[2] < 0) || (axis_sign[2] > 1) ||
	  ((axis_order[0] * axis_order[1] * axis_order[2]) != 0) ||
	  ((axis_order[0] + axis_order[1] + axis_order[2]) != 3)) {
		*pat = 0;
		return -1;
	}
	/* calculate pat
	 * BIT MAP
	 * [8] = sign_x
	 * [7] = sign_y
	 * [6] = sign_z
	 * [5:4] = order_x
	 * [3:2] = order_y
	 * [1:0] = order_z
	 */
	*pat = ((int16_t)axis_sign[0] << 8);
	*pat += ((int16_t)axis_sign[1] << 7);
	*pat += ((int16_t)axis_sign[2] << 6);
	*pat += ((int16_t)axis_order[0] << 4);
	*pat += ((int16_t)axis_order[1] << 2);
	*pat += ((int16_t)axis_order[2] << 0);
	return 0;
}

static int16_t AKECS_SetCert(void)
{
	struct i2c_client *client = this_client;
	struct akm09918_i2c_data *data = i2c_get_clientdata(client);
	uint8_t axis_sign[3] = {0};
	uint8_t axis_order[3] = {0};
	int16_t ret = 0;
	int i = 0;
	int16_t cert = 0x06;

	for (i = 0; i < 3; i++)
		axis_order[i] = (uint8_t)data->cvt.map[i];

	for (i = 0; i < 3; i++) {
		if (data->cvt.sign[i] > 0)
			axis_sign[i] = 0;
		else if (data->cvt.sign[i] < 0)
			axis_sign[i] = 1;
	}
#if 0
	axis_order[0] = 0;
	axis_order[1] = 1;
	axis_order[2] = 2;
	axis_sign[0] = 0;
	axis_sign[1] = 0;
	axis_sign[2] = 0;
#endif
	ret = AKECS_AxisInfoToPat(axis_order, axis_sign, &cert);
	if (ret != 0)
		return 0;
	return cert;
}
/* M-sensor daemon application have set the sng mode */
static long AKECS_GetData(char *rbuf, int size)
{
	char temp;
	int loop_i, ret;
#if DEBUG
	struct i2c_client *client = this_client;
	struct akm09918_i2c_data *data = i2c_get_clientdata(client);
#endif

	if (size < SENSOR_DATA_SIZE) {
		pr_err("buff size is too small %d!\n", size);
		return -1;
	}

	memset(rbuf, 0, SENSOR_DATA_SIZE);
	rbuf[0] = AK0991X_REG_ST1;

	for (loop_i = 0; loop_i < AKM09918_RETRY_COUNT; loop_i++) {
		ret = AKI2C_RxData(rbuf, 1);
		if (ret) {
			pr_err_ratelimited("read ST1 resigster failed!\n");
			return -1;
		}

		if ((rbuf[0] & 0x01) == 0x01)
			break;

		mdelay(2);
		rbuf[0] = AK0991X_REG_ST1;
	}

	if (loop_i >= AKM09918_RETRY_COUNT) {
		pr_err("Data read retry larger the max count!\n");
		if (factory_mode == 0)
			/* if return we can not get data at factory mode */
			return -1;
	}

	temp = rbuf[0];

	rbuf[1] = AK0991X_REG_HXL;
	ret = AKI2C_RxData(&rbuf[1], SENSOR_DATA_SIZE - 1);

	if (ret < 0) {
		pr_err_ratelimited("AKM8975 akm8975_work_func: I2C failed\n");
		return -1;
	}
	rbuf[0] = temp;

	return 0;
}

static int AKECS_GetConvert(int direction, struct hwmsen_convert *cvt)
{
	if (!cvt)
		return -EINVAL;
	else if ((direction > 8) || (direction <= 0))
		return -EINVAL;

	*cvt = akm_map[direction];
	return 0;
}

/*----------------------------------------------------------------------------*/
static int akm09918_ReadChipInfo(char *buf, int bufsize)
{
	if ((!buf) || (bufsize <= AKM09918_BUFSIZE - 1))
		return -1;

	if (!this_client) {
		*buf = 0;
		return -2;
	}

	sprintf(buf, "akm09918 Chip");
	return 0;
}

/*----------------------------shipment test----------------------*/
/*!
 *@return If @a testdata is in the range of between @a lolimit and @a hilimit,
 *the return value is 1, otherwise -1.
 *@param[in] testno   A pointer to a text string.
 *@param[in] testname A pointer to a text string.
 *@param[in] testdata A data to be tested.
 *@param[in] lolimit  The maximum allowable value of @a testdata.
 *@param[in] hilimit  The minimum allowable value of @a testdata.
 *@param[in,out] pf_total
 */
int TEST_DATA(const char testno[], const char testname[], const int testdata,
	      const int lolimit, const int hilimit, int *pf_total)
{
	int pf;			/* Pass;1, Fail;-1 */

	if ((testno == NULL) && (strncmp(testname, "START", 5) == 0)) {
		pr_debug(" Test Name Fail Test Data [Low High]\n");
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
		pr_debug(" %7s  %-10s	 %c	%9d	[%9d	%9d]\n",
			 testno, testname, ((pf == 1) ? ('.') : ('F')),
			 testdata, lolimit, hilimit);
	}

	/* Pass/Fail check */
	if (*pf_total != 0) {
		if ((*pf_total == 1) && (pf == 1))
			*pf_total = 1;	/* Pass */
		else
			*pf_total = -1;	/* Fail */
	}
	return pf;
}

/*!
 *Execute "Onboard Function Test" (NOT includes "START" and "END" command).
 *@retval 1 The test is passed successfully.
 *@retval -1 The test is failed.
 *@retval 0 The test is aborted by kind of system error.
 */
int FST_AK09911(void)
{
	int pf_total;		/* p/f flag for this subtest */
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
	i2cData[0] = AK0991X_REG_WIA1;
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
	if (AKECS_SetMode(AKM_MODE_FUSE_ACCESS) < 0) {
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
		return 0;
	}

	/* Read values from ASAX to ASAZ */
	i2cData[0] = AK0991X_FUSE_ASAX;
	if (AKI2C_RxData(i2cData, 3) < 0) {
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
		return 0;
	}
	asax = (int)i2cData[0];
	asay = (int)i2cData[1];
	asaz = (int)i2cData[2];

	/* TEST */
	TEST_DATA(TLIMIT_NO_ASAX_09911, TLIMIT_TN_ASAX_09911,
		asax, TLIMIT_LO_ASAX_09911,
		TLIMIT_HI_ASAX_09911, &pf_total);
	TEST_DATA(TLIMIT_NO_ASAY_09911, TLIMIT_TN_ASAY_09911,
		asay, TLIMIT_LO_ASAY_09911,
		TLIMIT_HI_ASAY_09911, &pf_total);
	TEST_DATA(TLIMIT_NO_ASAZ_09911, TLIMIT_TN_ASAZ_09911,
		asaz, TLIMIT_LO_ASAZ_09911,
		TLIMIT_HI_ASAZ_09911, &pf_total);

	/* Set to PowerDown mode */
	if (AKECS_SetMode(AKM_MODE_POWERDOWN) < 0) {
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
		return 0;
	}

	/* *********************************************** */
	/* Step2 */
	/* *********************************************** */

	/* Set to SNG measurement pattern (Set CNTL register) */
	if (AKECS_SetMode(AKM_MODE_SNG_MEASURE) < 0) {
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

	hdata[0] = (int16_t) (i2cData[1] | (i2cData[2] << 8));
	hdata[1] = (int16_t) (i2cData[3] | (i2cData[4] << 8));
	hdata[2] = (int16_t) (i2cData[5] | (i2cData[6] << 8));

	/* TEST */
	i2cData[0] &= 0x7F;
	TEST_DATA(TLIMIT_NO_SNG_ST1_09911, TLIMIT_TN_SNG_ST1_09911,
		(int)i2cData[0], TLIMIT_LO_SNG_ST1_09911,
		TLIMIT_HI_SNG_ST1_09911, &pf_total);

	/* TEST */
	TEST_DATA(TLIMIT_NO_SNG_HX_09911, TLIMIT_TN_SNG_HX_09911,
		hdata[0], TLIMIT_LO_SNG_HX_09911,
		TLIMIT_HI_SNG_HX_09911, &pf_total);
	TEST_DATA(TLIMIT_NO_SNG_HY_09911, TLIMIT_TN_SNG_HY_09911,
		hdata[1], TLIMIT_LO_SNG_HY_09911,
		TLIMIT_HI_SNG_HY_09911, &pf_total);
	TEST_DATA(TLIMIT_NO_SNG_HZ_09911, TLIMIT_TN_SNG_HZ_09911,
		hdata[2], TLIMIT_LO_SNG_HZ_09911,
		TLIMIT_HI_SNG_HZ_09911, &pf_total);
	TEST_DATA(TLIMIT_NO_SNG_ST2_09911, TLIMIT_TN_SNG_ST2_09911,
		(int)i2cData[8], TLIMIT_LO_SNG_ST2_09911,
		TLIMIT_HI_SNG_ST2_09911, &pf_total);

	/* Set to Self-test mode (Set CNTL register) */
	if (AKECS_SetMode(AKM_MODE_SELF_TEST) < 0) {
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

	hdata[0] = (int16_t) (i2cData[1] | (i2cData[2] << 8));
	hdata[1] = (int16_t) (i2cData[3] | (i2cData[4] << 8));
	hdata[2] = (int16_t) (i2cData[5] | (i2cData[6] << 8));

	/* TEST */
	TEST_DATA(TLIMIT_NO_SLF_RVHX_09911,
		  TLIMIT_TN_SLF_RVHX_09911,
		  (hdata[0]) * (asax / 128 + 1),
		  TLIMIT_LO_SLF_RVHX_09911,
		  TLIMIT_HI_SLF_RVHX_09911, &pf_total);

	TEST_DATA(TLIMIT_NO_SLF_RVHY_09911,
		  TLIMIT_TN_SLF_RVHY_09911,
		  (hdata[1]) * (asay / 128 + 1),
		  TLIMIT_LO_SLF_RVHY_09911,
		  TLIMIT_HI_SLF_RVHY_09911, &pf_total);

	TEST_DATA(TLIMIT_NO_SLF_RVHZ_09911,
		  TLIMIT_TN_SLF_RVHZ_09911,
		  (hdata[2]) * (asaz / 128 + 1),
		  TLIMIT_LO_SLF_RVHZ_09911,
		  TLIMIT_HI_SLF_RVHZ_09911, &pf_total);

	TEST_DATA(TLIMIT_NO_SLF_ST2_09911,
		  TLIMIT_TN_SLF_ST2_09911,
		  (int)i2cData[8], TLIMIT_LO_SLF_ST2_09911,
		  TLIMIT_HI_SLF_ST2_09911, &pf_total);

	return pf_total;
}

int FST_AK09916(void)
{
	int pf_total;  //p/f flag for this subtest
	char i2cData[16];
	int hdata[3];

	//  Reset Test Result
	pf_total = 1;

	//  Step1
	// Reset device.
	if (AKECS_Reset(0) < 0) {
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
		return 0;
	}

	// Read values from WIA.
	i2cData[0] = AK0991X_REG_WIA1;
	if (AKI2C_RxData(i2cData, 2) < 0) {
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
		return 0;
	}

	/* TEST */
	TEST_DATA(TLIMIT_NO_RST_WIA1_09916, TLIMIT_TN_RST_WIA1_09916,
		(int)i2cData[0], TLIMIT_LO_RST_WIA1_09916,
		TLIMIT_HI_RST_WIA1_09916, &pf_total);
	TEST_DATA(TLIMIT_NO_RST_WIA2_09916, TLIMIT_TN_RST_WIA2_09916,
		(int)i2cData[1], TLIMIT_LO_RST_WIA2_09916,
		TLIMIT_HI_RST_WIA2_09916, &pf_total);

	/* Set to PowerDown mode */
	if (AKECS_SetMode(AKM_MODE_POWERDOWN) < 0) {
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
		return 0;
	}

	/* *********************************************** */
	/* Step2 */
	/* *********************************************** */

	/* Set to SNG measurement pattern (Set CNTL register) */
	if (AKECS_SetMode(AKM_MODE_SNG_MEASURE) < 0) {
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
		return 0;
	}

	/* Wait for DRDY pin changes to HIGH. */
	/* usleep(AKM_MEASURE_TIME_US); */
	/* Get measurement data from AK09916 */
	/* ST1 + (HXL + HXH) + (HYL + HYH) + (HZL + HZH) + TEMP + ST2 */
	/* = 1 + (1 + 1) + (1 + 1) + (1 + 1) + 1 + 1 = 9yte */
	/* if (AKD_GetMagneticData(i2cData) != AKD_SUCCESS) { */
	if (AKECS_GetData(i2cData, SENSOR_DATA_SIZE) < 0) {
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
		return 0;
	}

	hdata[0] = (int16_t) (i2cData[1] | (i2cData[2] << 8));
	hdata[1] = (int16_t) (i2cData[3] | (i2cData[4] << 8));
	hdata[2] = (int16_t) (i2cData[5] | (i2cData[6] << 8));

	/* TEST */
	TEST_DATA(TLIMIT_NO_SNG_ST1_09916, TLIMIT_TN_SNG_ST1_09916,
		(int)i2cData[0], TLIMIT_LO_SNG_ST1_09916,
		TLIMIT_HI_SNG_ST1_09916, &pf_total);

	/* TEST */
	TEST_DATA(TLIMIT_NO_SNG_HX_09916, TLIMIT_TN_SNG_HX_09916,
		hdata[0], TLIMIT_LO_SNG_HX_09916,
		TLIMIT_HI_SNG_HX_09916, &pf_total);
	TEST_DATA(TLIMIT_NO_SNG_HY_09916, TLIMIT_TN_SNG_HY_09916,
		hdata[1], TLIMIT_LO_SNG_HY_09916,
		TLIMIT_HI_SNG_HY_09916, &pf_total);
	TEST_DATA(TLIMIT_NO_SNG_HZ_09916, TLIMIT_TN_SNG_HZ_09916,
		hdata[2], TLIMIT_LO_SNG_HZ_09916,
		TLIMIT_HI_SNG_HZ_09916, &pf_total);
	TEST_DATA(TLIMIT_NO_SNG_ST2_09916, TLIMIT_TN_SNG_ST2_09916,
		(int)i2cData[8] & TLIMIT_ST2_MASK_09916,
		TLIMIT_LO_SNG_ST2_09916, TLIMIT_HI_SNG_ST2_09916, &pf_total);

	/* Set to Self-test mode (Set CNTL register) */
	if (AKECS_SetMode(AKM_MODE_SELF_TEST) < 0) {
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
		return 0;
	}

	/* Wait for DRDY pin changes to HIGH. */
	/* usleep(AKM_MEASURE_TIME_US); */
	/* Get measurement data from AK09916 */
	/* ST1 + (HXL + HXH) + (HYL + HYH) + (HZL + HZH) + TEMP + ST2 */
	/* = 1 + (1 + 1) + (1 + 1) + (1 + 1) + 1 + 1 = 9byte */
	/* if (AKD_GetMagneticData(i2cData) != AKD_SUCCESS) { */
	if (AKECS_GetData(i2cData, SENSOR_DATA_SIZE) < 0) {
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
		return 0;
	}

	/* TEST */
	TEST_DATA(TLIMIT_NO_SLF_ST1_09916, TLIMIT_TN_SLF_ST1_09916,
		(int)i2cData[0], TLIMIT_LO_SLF_ST1_09916,
		TLIMIT_HI_SLF_ST1_09916, &pf_total);

	hdata[0] = (int16_t) (i2cData[1] | (i2cData[2] << 8));
	hdata[1] = (int16_t) (i2cData[3] | (i2cData[4] << 8));
	hdata[2] = (int16_t) (i2cData[5] | (i2cData[6] << 8));

	/* TEST */
	TEST_DATA(TLIMIT_NO_SLF_RVHX_09916, TLIMIT_TN_SLF_RVHX_09916,
		hdata[0], TLIMIT_LO_SLF_RVHX_09916,
		TLIMIT_HI_SLF_RVHX_09916, &pf_total);

	TEST_DATA(TLIMIT_NO_SLF_RVHY_09916, TLIMIT_TN_SLF_RVHY_09916,
		hdata[1], TLIMIT_LO_SLF_RVHY_09916,
		TLIMIT_HI_SLF_RVHY_09916, &pf_total);

	TEST_DATA(TLIMIT_NO_SLF_RVHZ_09916, TLIMIT_TN_SLF_RVHZ_09916,
		hdata[2], TLIMIT_LO_SLF_RVHZ_09916,
		TLIMIT_HI_SLF_RVHZ_09916, &pf_total);

	TEST_DATA(TLIMIT_NO_SLF_ST2_09916, TLIMIT_TN_SLF_ST2_09916,
		(int)i2cData[8] & TLIMIT_ST2_MASK_09916,
		TLIMIT_LO_SLF_ST2_09916, TLIMIT_HI_SLF_ST2_09916, &pf_total);

	return pf_total;
}

int FST_AK09918(void)
{
	int pf_total;  //p/f flag for this subtest
	char i2cData[16];
	int hdata[3];

	//Reset Test Result
	pf_total = 1;

	//Step1
	// Reset device.
	if (AKECS_Reset(0) < 0) {
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
		return 0;
	}

	// Read values from WIA.
	i2cData[0] = AK0991X_REG_WIA1;
	if (AKI2C_RxData(i2cData, 2) < 0) {
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
		return 0;
	}

	/* TEST */
	TEST_DATA(TLIMIT_NO_RST_WIA1_09918, TLIMIT_TN_RST_WIA1_09918,
		(int)i2cData[0],
		TLIMIT_LO_RST_WIA1_09918, TLIMIT_HI_RST_WIA1_09918,
		&pf_total);
	TEST_DATA(TLIMIT_NO_RST_WIA2_09918, TLIMIT_TN_RST_WIA2_09918,
		(int)i2cData[1],
		TLIMIT_LO_RST_WIA2_09918, TLIMIT_HI_RST_WIA2_09918,
		&pf_total);

	/* Set to PowerDown mode */
	if (AKECS_SetMode(AKM_MODE_POWERDOWN) < 0) {
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
		return 0;
	}

	/* *********************************************** */
	/* Step2 */
	/* *********************************************** */

	/* Set to SNG measurement pattern (Set CNTL register) */
	if (AKECS_SetMode(AKM_MODE_SNG_MEASURE) < 0) {
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
		return 0;
	}

	/* Wait for DRDY pin changes to HIGH. */
	/* usleep(AKM_MEASURE_TIME_US); */
	/* Get measurement data from AK09918 */
	/* ST1 + (HXL + HXH) + (HYL + HYH) + (HZL + HZH) + TEMP + ST2 */
	/* = 1 + (1 + 1) + (1 + 1) + (1 + 1) + 1 + 1 = 9yte */
	/* if (AKD_GetMagneticData(i2cData) != AKD_SUCCESS) { */
	if (AKECS_GetData(i2cData, SENSOR_DATA_SIZE) < 0) {
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
		return 0;
	}

	hdata[0] = (int16_t) (i2cData[1] | (i2cData[2] << 8));
	hdata[1] = (int16_t) (i2cData[3] | (i2cData[4] << 8));
	hdata[2] = (int16_t) (i2cData[5] | (i2cData[6] << 8));

	/* TEST */
	TEST_DATA(TLIMIT_NO_SNG_ST1_09918, TLIMIT_TN_SNG_ST1_09918,
		(int)i2cData[0], TLIMIT_LO_SNG_ST1_09918,
		TLIMIT_HI_SNG_ST1_09918, &pf_total);

	/* TEST */
	TEST_DATA(TLIMIT_NO_SNG_HX_09918, TLIMIT_TN_SNG_HX_09918,
		hdata[0], TLIMIT_LO_SNG_HX_09918,
		TLIMIT_HI_SNG_HX_09918, &pf_total);
	TEST_DATA(TLIMIT_NO_SNG_HY_09918, TLIMIT_TN_SNG_HY_09918,
		hdata[1], TLIMIT_LO_SNG_HY_09918,
		TLIMIT_HI_SNG_HY_09918, &pf_total);
	TEST_DATA(TLIMIT_NO_SNG_HZ_09918, TLIMIT_TN_SNG_HZ_09918,
		hdata[2], TLIMIT_LO_SNG_HZ_09918,
		TLIMIT_HI_SNG_HZ_09918, &pf_total);
	TEST_DATA(TLIMIT_NO_SNG_ST2_09918, TLIMIT_TN_SNG_ST2_09918,
		(int)i2cData[8] & TLIMIT_ST2_MASK_09918,
		TLIMIT_LO_SNG_ST2_09918, TLIMIT_HI_SNG_ST2_09918, &pf_total);

	/* Set to Self-test mode (Set CNTL register) */
	if (AKECS_SetMode(AKM_MODE_SELF_TEST) < 0) {
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
		return 0;
	}

	/* Wait for DRDY pin changes to HIGH. */
	/* usleep(AKM_MEASURE_TIME_US); */
	/* Get measurement data from AK09918 */
	/* ST1 + (HXL + HXH) + (HYL + HYH) + (HZL + HZH) + TEMP + ST2 */
	/* = 1 + (1 + 1) + (1 + 1) + (1 + 1) + 1 + 1 = 9byte */
	/* if (AKD_GetMagneticData(i2cData) != AKD_SUCCESS) { */
	if (AKECS_GetData(i2cData, SENSOR_DATA_SIZE) < 0) {
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
		return 0;
	}

	/* TEST */
	TEST_DATA(TLIMIT_NO_SLF_ST1_09918,
		TLIMIT_TN_SLF_ST1_09918, (int)i2cData[0],
		TLIMIT_LO_SLF_ST1_09918, TLIMIT_HI_SLF_ST1_09918, &pf_total);

	hdata[0] = (int16_t) (i2cData[1] | (i2cData[2] << 8));
	hdata[1] = (int16_t) (i2cData[3] | (i2cData[4] << 8));
	hdata[2] = (int16_t) (i2cData[5] | (i2cData[6] << 8));

	/* TEST */
	TEST_DATA(TLIMIT_NO_SLF_RVHX_09918,
		TLIMIT_TN_SLF_RVHX_09918, hdata[0], TLIMIT_LO_SLF_RVHX_09918,
		TLIMIT_HI_SLF_RVHX_09918, &pf_total);

	TEST_DATA(TLIMIT_NO_SLF_RVHY_09918,
		TLIMIT_TN_SLF_RVHY_09918, hdata[1], TLIMIT_LO_SLF_RVHY_09918,
		TLIMIT_HI_SLF_RVHY_09918, &pf_total);

	TEST_DATA(TLIMIT_NO_SLF_RVHZ_09918, TLIMIT_TN_SLF_RVHZ_09918,
		hdata[2], TLIMIT_LO_SLF_RVHZ_09918,
		TLIMIT_HI_SLF_RVHZ_09918, &pf_total);

	TEST_DATA(TLIMIT_NO_SLF_ST2_09918, TLIMIT_TN_SLF_ST2_09918,
		(int)i2cData[8] & TLIMIT_ST2_MASK_09918,
		TLIMIT_LO_SLF_ST2_09918, TLIMIT_HI_SLF_ST2_09918, &pf_total);

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
#if defined(AKM_Device_AK09911)
	pf_total = FST_AK09911();
#elif defined(AKM_Device_AK09916)
	pf_total = FST_AK09916();
#else
	pf_total = FST_AK09918();
#endif

	/* *********************************************** */
	/* Judge Test Result */
	/* *********************************************** */
	TEST_DATA(NULL, "END", 0, 0, 0, &pf_total);

	return pf_total;
}

static ssize_t store_shipment_test(struct device_driver *ddri,
					const char *buf, size_t count)
{
	/* struct i2c_client *client = this_client; */
	/* struct akm09918_i2c_data *data = i2c_get_clientdata(client); */
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
		strlcpy(result, "y", sizeof(result));
	} else if (res == -1) {
		pr_debug("shipment_test fail\n");
		strlcpy(result, "n", sizeof(result));
	} else {
		pr_debug("shipment_test NaN\n");
		strlcpy(result, "NaN", sizeof(result));
	}

	return sprintf(buf, "%s\n", result);
}

static ssize_t show_daemon_name(struct device_driver *ddri, char *buf)
{
	char strbuf[AKM09918_BUFSIZE];
	int ret;

	ret = sprintf(strbuf, "akmd09918");
	if (ret < 0)
		pr_debug("%s:strbuf sprintf Error:%d\n", __func__, ret);
	ret = sprintf(buf, "%s", strbuf);
	if (ret < 0)
		pr_debug("%s:strbuf to buf sprintf Error:%d\n", __func__, ret);

	return ret;
}

static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	char strbuf[AKM09918_BUFSIZE];

	akm09918_ReadChipInfo(strbuf, AKM09918_BUFSIZE);
	return sprintf(buf, "%s\n", strbuf);
}

/*----------------------------------------------------------------------------*/
static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{

	char sensordata[SENSOR_DATA_SIZE];
	char strbuf[AKM09918_BUFSIZE];
	char buffer[1];
	int ret;

	buffer[0] = AK0991X_REG_CNTL2;
	ret = AKI2C_RxData(buffer, 1);

	/* Check if e-compass is measuring by checking the CNTL2 register.
	 * If (buffer[0] & 0x0F) is 0, e-compass is not measuring.
	 * Set it to single measurement mode
	 */
	if (ret < 0) {
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
	} else {
		if (!(buffer[0] & 0x0F)) {
			AKECS_SetMode_SngMeasure();
			mdelay(10);
		}
	}

	AKECS_GetData(sensordata, SENSOR_DATA_SIZE);

	ret = sprintf(strbuf, "%d %d %d %d %d %d %d %d %d\n",
		sensordata[0], sensordata[1], sensordata[2],
		sensordata[3], sensordata[4], sensordata[5],
		sensordata[6], sensordata[7], sensordata[8]);
	if (ret < 0)
		pr_debug("%s:sensor_data sprintf Error:%d\n", __func__, ret);

	ret = sprintf(buf, "%s\n", strbuf);
	if (ret < 0)
		pr_debug("%s:sensor_data to buf sprintf Error:%d\n", __func__, ret);
	return ret;
}

/*----------------------------------------------------------------------------*/
static ssize_t show_layout_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = this_client;
	struct akm09918_i2c_data *data = i2c_get_clientdata(client);

	return sprintf(buf, "(%d, %d)\n[%+2d %+2d %+2d]\n[%+2d %+2d %+2d]\n",
		       data->hw->direction, atomic_read(&data->layout),
		       data->cvt.sign[0], data->cvt.sign[1],
		       data->cvt.sign[2], data->cvt.map[0],
		       data->cvt.map[1], data->cvt.map[2]);
}

/*----------------------------------------------------------------------------*/
static ssize_t store_layout_value(struct device_driver *ddri,
					const char *buf, size_t count)
{
	struct i2c_client *client = this_client;
	struct akm09918_i2c_data *data = i2c_get_clientdata(client);
	int layout = 0;
	int ret = 0;

	ret = kstrtoint(buf, 10, &layout);
	if (ret == 0) {
		atomic_set(&data->layout, layout);
		if (AKECS_GetConvert(layout, &data->cvt))
			pr_err("HWMSEN_GET_CONVERT function error!\r\n");
		else if (AKECS_GetConvert(data->hw->direction, &data->cvt))
			pr_err("invalid layout: %d, restore to %d\n", layout,
				 data->hw->direction);
		else {
			pr_err("invalid layout: (%d, %d)\n",
				layout, data->hw->direction);
			ret = AKECS_GetConvert(1, &data->cvt);
			if (ret)
				pr_err("HWMSEN_GET_CONVERT function error!\r\n");
		}
	} else
		pr_err("invalid format = '%s'\n", buf);

	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t show_status_value(struct device_driver *ddri, char *buf)
{
	struct i2c_client *client = this_client;
	struct akm09918_i2c_data *data = i2c_get_clientdata(client);
	ssize_t len = 0;

	if (data->hw)
		len += snprintf(buf + len, PAGE_SIZE - len,
				"CUST: %d %d (%d %d)\n",
				data->hw->i2c_num, data->hw->direction,
				data->hw->power_id, data->hw->power_vol);
	else
		len += snprintf(buf + len, PAGE_SIZE - len, "CUST: NULL\n");

	len += snprintf(buf + len,
		PAGE_SIZE - len, "OPEN: %d\n", atomic_read(&dev_open_count));
	return len;
}

/*----------------------------------------------------------------------------*/
static ssize_t show_trace_value(struct device_driver *ddri, char *buf)
{
	ssize_t res;
	struct akm09918_i2c_data *obj = i2c_get_clientdata(this_client);

	if (obj == NULL) {
		pr_err("akm09918_i2c_data is null!!\n");
		return 0;
	}

	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&obj->trace));
	return res;
}

/*----------------------------------------------------------------------------*/
static ssize_t store_trace_value(struct device_driver *ddri,
					const char *buf, size_t count)
{
	struct akm09918_i2c_data *obj = i2c_get_clientdata(this_client);
	int trace;

	if (obj == NULL) {
		pr_err("akm09918_i2c_data is null!!\n");
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
	struct mag_hw *_ptAccelHw = hw;

	pr_debug("[%s] default direction: %d\n",
		__func__, _ptAccelHw->direction);

	_tLength = snprintf(buf,
		PAGE_SIZE, "default direction = %d\n", _ptAccelHw->direction);

	return _tLength;
}

static ssize_t store_chip_orientation(struct device_driver *ddri,
						const char *buf, size_t tCount)
{
	int _nDirection = 0;
	int ret = 0;
	struct akm09918_i2c_data *_pt_i2c_obj = i2c_get_clientdata(this_client);

	if (_pt_i2c_obj == NULL)
		return 0;

	ret = kstrtoint(buf, 10, &_nDirection);
	if (ret == 0) {
		if (AKECS_GetConvert(_nDirection, &_pt_i2c_obj->cvt))
			pr_err("ERR: fail to set direction\n");
	}

	pr_debug("[%s] set direction: %d\n", __func__, _nDirection);

	return tCount;
}

static ssize_t show_power_status(struct device_driver *ddri, char *buf)
{
	int ret = 0;
	ssize_t res = 0;
	u8 uData = AK0991X_REG_CNTL2;
	struct akm09918_i2c_data *obj = i2c_get_clientdata(this_client);

	if (obj == NULL) {
		pr_err("i2c_data obj is null!!\n");
		return 0;
	}
	ret = AKI2C_RxData(&uData, 1);
	if (ret < 0)
		pr_debug("%s:%d Error.\n", __func__, __LINE__);
	res = snprintf(buf, PAGE_SIZE, "0x%04X\n", uData);
	if (res < 0)
		pr_debug("%s:PAGE_SIZE snprintf Error:%d\n", __func__, res);
	return res;
}

static ssize_t show_regiter_map(struct device_driver *ddri, char *buf)
{
	u8 _bIndex = 0;
	u8 _baRegMap[] = {
		0x00, 0x01, 0x02, 0x03, 0x10, 0x11,
		0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
		0x30, 0x31, 0x32, 0x33, 0x60, 0x61, 0x62
	};
	/* u8  _baRegValue[20]; */
	ssize_t _tLength = 0;
	char tmp[2] = { 0 };
	int ret = 0;

	for (_bIndex = 0; _bIndex < 20; _bIndex++) {
		tmp[0] = _baRegMap[_bIndex];
		ret = AKI2C_RxData(tmp, 1);
		if (ret < 0)
			pr_debug("%s:%d Error.\n", __func__, __LINE__);
		_tLength +=
		    snprintf((buf + _tLength),
			(PAGE_SIZE - _tLength), "Reg[0x%02X]: 0x%02X\n",
			_baRegMap[_bIndex], tmp[0]);
	}

	return _tLength;
}


/*----------------------------------------------------------------------------*/
static DRIVER_ATTR(daemon, 0444, show_daemon_name, NULL);
static DRIVER_ATTR(shipmenttest, 0644,
				show_shipment_test, store_shipment_test);
static DRIVER_ATTR(chipinfo, 0444, show_chipinfo_value, NULL);
static DRIVER_ATTR(sensordata, 0444, show_sensordata_value, NULL);
static DRIVER_ATTR(layout, 0644, show_layout_value, store_layout_value);
static DRIVER_ATTR(status, 0444, show_status_value, NULL);
static DRIVER_ATTR(trace, 0644, show_trace_value, store_trace_value);
static DRIVER_ATTR(orientation, 0644,
				show_chip_orientation, store_chip_orientation);
static DRIVER_ATTR(power, 0444, show_power_status, NULL);
static DRIVER_ATTR(regmap, 0444, show_regiter_map, NULL);

/*----------------------------------------------------------------------------*/
static struct driver_attribute *akm09918_attr_list[] = {
	&driver_attr_daemon,
	&driver_attr_shipmenttest,
	&driver_attr_chipinfo,
	&driver_attr_sensordata,
	&driver_attr_layout,
	&driver_attr_status,
	&driver_attr_trace,
	&driver_attr_orientation,
	&driver_attr_power,
	&driver_attr_regmap,
};

/*----------------------------------------------------------------------------*/
static int akm09918_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(ARRAY_SIZE(akm09918_attr_list));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, akm09918_attr_list[idx]);
		if (err) {
			pr_err("driver_create_file (%s) = %d\n",
				 akm09918_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

/*----------------------------------------------------------------------------*/
static int akm09918_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = (int)(ARRAY_SIZE(akm09918_attr_list));

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, akm09918_attr_list[idx]);

	return err;
}

/*----------------------------------------------------------------------------*/
static int akm09918_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct akm09918_i2c_data *obj = i2c_get_clientdata(client);

	akm09918_power(obj->hw, 0);

	return 0;
}

/*----------------------------------------------------------------------------*/
static int akm09918_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct akm09918_i2c_data *obj = i2c_get_clientdata(client);

	akm09918_power(obj->hw, 1);
	return 0;
}

/*----------------------------------------------------------------------------*/
static int akm09918_i2c_detect(struct i2c_client *client,
					struct i2c_board_info *info)
{
	strlcpy(info->type, AKM09918_DEV_NAME, sizeof(info->type));
	return 0;
}

static int akm09918_enable(int en)
{
	int value = 0;
	int err = 0;
	struct akm09918_i2c_data *f_obj = i2c_get_clientdata(this_client);

	if (f_obj == NULL)
		return -1;

	value = en;
	factory_mode = 1;
	if (value == 1) {
		f_obj->enable = true;

#ifdef AKM_CONTINUOUS_MODE
		err = AKECS_SetMode_CntMeasure(AKM_MODE_CNT_MEASURE_4);
#else

		err = AKECS_SetMode(AKM_MODE_SNG_MEASURE);
#endif
		if (err < 0) {
			pr_err("%s:AKECS_SetMode Error.\n", __func__);
			return err;
		}
	} else {
		f_obj->enable = false;
		err = AKECS_SetMode(AKM_MODE_POWERDOWN);
		if (err < 0) {
			pr_err("%s:AKECS_SetMode Error.\n", __func__);
			return err;
		}
	}
	if (f_obj->flush) {
		if (value == 1) {
			pr_debug("%s will call akm09918_flush\n", __func__);
			akm09918_flush();
		} else
			f_obj->flush = false;
	}
	wake_up(&open_wq);
	return err;
}

static int akm09918_set_delay(u64 ns)
{
	int value = 0;

	value = (int)ns / 1000 / 1000;

	if (value <= 10)
		akmd_delay = 10;
	else
		akmd_delay = value;

	return 0;
}

static int akm09918_open_report_data(int open)
{
	return 0;
}

static int akm09918_coordinate_convert(int16_t *mag_data)
{
	struct i2c_client *client = this_client;
	struct akm09918_i2c_data *data = i2c_get_clientdata(client);
	int16_t temp_data[3];
	int i = 0;

	for (i = 0; i < 3; i++)
		temp_data[i] = mag_data[i];
	/* remap coordinate */
	mag_data[0] = data->cvt.sign[0] * temp_data[data->cvt.map[0]];
	mag_data[1] = data->cvt.sign[1] * temp_data[data->cvt.map[1]];
	mag_data[2] = data->cvt.sign[2] * temp_data[data->cvt.map[2]];

	return 0;
}
static int akm09918_get_data(int *x, int *y, int *z, int *status)
{
	char strbuf[SENSOR_DATA_SIZE];
	int16_t data[3];

#ifdef AKM_CONTINUOUS_MODE
	//AKECS_SetMode_CntMeasure(AKM_MODE_CNT_MEASURE_4);
#else
	AKECS_SetMode_SngMeasure();
	mdelay(10);
#endif

	AKECS_GetData(strbuf, SENSOR_DATA_SIZE);
	data[0] = (int16_t)(strbuf[1] | (strbuf[2] << 8));
	data[1] = (int16_t)(strbuf[3] | (strbuf[4] << 8));
	data[2] = (int16_t)(strbuf[5] | (strbuf[6] << 8));

	akm09918_coordinate_convert(data);

	if (akm_device == 0x04) {/* ak09912 */
		*x = data[0] * CONVERT_M_DIV *
			AKECS_ASA_CACULATE_AK09912(akm_fuse[0]);
		*y = data[1] * CONVERT_M_DIV *
			AKECS_ASA_CACULATE_AK09912(akm_fuse[1]);
		*z = data[2] * CONVERT_M_DIV *
			AKECS_ASA_CACULATE_AK09912(akm_fuse[2]);
	} else if (akm_device == 0x05) {
		*x = data[0] * CONVERT_M_DIV *
			AKECS_ASA_CACULATE_AK09911(akm_fuse[0]);
		*y = data[1] * CONVERT_M_DIV *
			AKECS_ASA_CACULATE_AK09911(akm_fuse[1]);
		*z = data[2] * CONVERT_M_DIV *
			AKECS_ASA_CACULATE_AK09911(akm_fuse[2]);
	} else if ((akm_device == 0x10) || (akm_device == 0x09) ||
		(akm_device == 0x0b) || (akm_device == 0x0c)) {
		*x = data[0] * CONVERT_M_DIV;
		*y = data[1] * CONVERT_M_DIV;
		*z = data[2] * CONVERT_M_DIV;
	}
	*status = strbuf[8];
	return 0;
}

static int akm09918_batch(int flag, int64_t samplingPeriodNs,
					int64_t maxBatchReportLatencyNs)
{
	int value = 0;

	value = (int)samplingPeriodNs / 1000 / 1000;

	if (value <= 10)
		akmd_delay = 10;
	else
		akmd_delay = value;

	pr_debug("akm09918 mag set delay = (%d) ok.\n", value);
	return 0;
}

static int akm09918_flush(void)
{
	/*Only flush after sensor was enabled*/
	int err = 0;
	struct akm09918_i2c_data *f_obj = i2c_get_clientdata(this_client);

	if (f_obj == NULL)
		return -1;

	if (!f_obj->enable) {
		f_obj->flush = true;
		return 0;
	}
	err = mag_flush_report();
	if (err >= 0)
		f_obj->flush = false;
	return err;
}

static int akm09918_factory_enable_sensor(bool enabledisable,
						int64_t sample_periods_ms)
{
	int err;

	err = akm09918_enable(enabledisable == true ? 1 : 0);
	if (err) {
		pr_err("%s enable sensor failed!\n", __func__);
		return -1;
	}
	err = akm09918_batch(0, sample_periods_ms * 1000000, 0);
	if (err) {
		pr_err("%s enable set batch failed!\n", __func__);
		return -1;
	}
	return 0;
}
static int akm09918_factory_get_data(int32_t data[3], int *status)
{
	int ret = 0;

	/* get raw data */
	ret =  akm09918_get_data(&data[0], &data[1], &data[2], status);

	data[0] = data[0] / CONVERT_M_DIV;
	data[1] = data[1] / CONVERT_M_DIV;
	data[2] = data[2] / CONVERT_M_DIV;

	return 0;
}
static int akm09918_factory_get_raw_data(int32_t data[3])
{
	pr_debug("%s do not support!\n", __func__);
	return 0;
}
static int akm09918_factory_enable_calibration(void)
{
	return 0;
}
static int akm09918_factory_clear_cali(void)
{
	return 0;
}
static int akm09918_factory_set_cali(int32_t data[3])
{
	return 0;
}
static int akm09918_factory_get_cali(int32_t data[3])
{
	return 0;
}
static int akm09918_factory_do_self_test(void)
{
	return 0;
}

static struct mag_factory_fops akm09918_factory_fops = {
	.enable_sensor = akm09918_factory_enable_sensor,
	.get_data = akm09918_factory_get_data,
	.get_raw_data = akm09918_factory_get_raw_data,
	.enable_calibration = akm09918_factory_enable_calibration,
	.clear_cali = akm09918_factory_clear_cali,
	.set_cali = akm09918_factory_set_cali,
	.get_cali = akm09918_factory_get_cali,
	.do_self_test = akm09918_factory_do_self_test,
};

static struct mag_factory_public akm09918_factory_device = {
	.gain = 1,
	.sensitivity = 1,
	.fops = &akm09918_factory_fops,
};

/*----------------------------------------------------------------------------*/
static int akm09918_i2c_probe(struct i2c_client *client,
					const struct i2c_device_id *id)
{
	int err = 0;
	struct i2c_client *new_client;
	struct akm09918_i2c_data *data;
	struct mag_control_path ctl = { 0 };
	struct mag_data_path mag_data = { 0 };
	struct platform_driver *paddr =
					akm09918_init_info.platform_diver_addr;

	pr_debug("%s\n", __func__);
	err = get_mag_dts_func(client->dev.of_node, hw);
	if (err) {
		pr_err("get dts info fail\n");
		err = -EFAULT;
		goto exit;
	}

	data = kzalloc(sizeof(struct akm09918_i2c_data), GFP_KERNEL);
	if (!data) {
		err = -ENOMEM;
		goto exit;
	}

	data->hw = hw;
	/*akm_map is different from MTK, so do not use hwmsen_get_convert*/
	//err = hwmsen_get_convert(data->hw->direction, &data->cvt);
	err = AKECS_GetConvert(data->hw->direction, &data->cvt);
	if (err) {
		/*direction is 1 - 8*/
		pr_err("invalid direction: %d\n", data->hw->direction);
		goto exit_kfree;
	}
	atomic_set(&data->layout, data->hw->direction);
	atomic_set(&data->trace, 0);
	/* init_waitqueue_head(&data_ready_wq); */
	init_waitqueue_head(&open_wq);
	data->client = client;
	new_client = data->client;
	i2c_set_clientdata(new_client, data);
	this_client = new_client;

	/* Check connection */
	err = AKECS_CheckDevice();
	if (err < 0) {
		pr_err("AKM09918 akm09918_probe: check device connect error\n");
		goto exit_init_failed;
	}

	err = mag_factory_device_register(&akm09918_factory_device);
	if (err) {
		pr_err("misc device register failed, err = %d\n", err);
		goto exit_misc_device_register_failed;
	}

	/* Register sysfs attribute */
	err = akm09918_create_attr(&(paddr->driver));
	if (err) {
		pr_err("create attribute err = %d\n", err);
		goto exit_sysfs_create_group_failed;
	}

	ctl.is_use_common_factory = false;
	ctl.enable = akm09918_enable;
	ctl.set_delay = akm09918_set_delay;
	ctl.open_report_data = akm09918_open_report_data;
	ctl.batch = akm09918_batch;
	ctl.flush = akm09918_flush;
	ctl.is_report_input_direct = false;
	ctl.is_support_batch = data->hw->is_batch_supported;
	strlcpy(ctl.libinfo.libname, "akm", sizeof(ctl.libinfo.libname));
	ctl.libinfo.layout = AKECS_SetCert();
	ctl.libinfo.deviceid = akm_device;

	err = mag_register_control_path(&ctl);
	if (err) {
		pr_err("register mag control path err\n");
		goto exit_kfree;
	}

	mag_data.div = CONVERT_M_DIV;
	mag_data.get_data = akm09918_get_data;

	err = mag_register_data_path(&mag_data);
	if (err) {
		pr_err("register data control path err\n");
		goto exit_kfree;
	}

	pr_debug("%s: OK\n", __func__);
	akm09918_init_flag = 1;
	return 0;

exit_sysfs_create_group_failed:
exit_init_failed:
exit_misc_device_register_failed:
exit_kfree:
	kfree(data);
	data = NULL;
exit:
	pr_err("%s: err = %d\n", __func__, err);
	akm09918_init_flag = -1;
	return err;
}

/*----------------------------------------------------------------------------*/
static int akm09918_i2c_remove(struct i2c_client *client)
{
	int err;
	struct platform_driver *paddr =
					akm09918_init_info.platform_diver_addr;

	err = akm09918_delete_attr(&(paddr->driver));
	if (err)
		pr_err("akm09918_delete_attr fail: %d\n", err);

	this_client = NULL;
	i2c_unregister_device(client);
	mag_factory_device_deregister(&akm09918_factory_device);
	kfree(i2c_get_clientdata(client));
	return 0;
}

/*----------------------------------------------------------------------------*/
static int akm09918_remove(void)
{
	akm09918_power(hw, 0);
	atomic_set(&dev_open_count, 0);
	i2c_del_driver(&akm09918_i2c_driver);
	return 0;
}

static int akm09918_local_init(void)
{
	akm09918_power(hw, 1);
	if (i2c_add_driver(&akm09918_i2c_driver)) {
		pr_err("i2c_add_driver error\n");
		return -1;
	}
	if (-1 == akm09918_init_flag)
		return -1;
	return 0;
}

/*----------------------------------------------------------------------------*/
static int __init akm09918_init(void)
{
	mag_driver_add(&akm09918_init_info);
	return 0;
}

/*----------------------------------------------------------------------------*/
static void __exit akm09918_exit(void)
{
#ifdef CONFIG_CUSTOM_KERNEL_MAGNETOMETER_MODULE
	mag_success_Flag = false;
#endif
}

/*----------------------------------------------------------------------------*/
module_init(akm09918_init);
module_exit(akm09918_exit);

MODULE_AUTHOR("MTK");
MODULE_DESCRIPTION("AKM09918 compass driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRIVER_VERSION);
