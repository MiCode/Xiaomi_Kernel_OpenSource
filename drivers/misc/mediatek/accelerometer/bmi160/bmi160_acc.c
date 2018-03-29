/*!
 * @section LICENSE
 * (C) Copyright 2011~2016 Bosch Sensortec GmbH All Rights Reserved
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 * This software program is licensed subject to the GNU General
 * Public License (GPL).Version 2,June 1991,
 * available at http://www.fsf.org/copyleft/gpl.html
 * VERSION: V2.0
 * Date: 2016/12/08
 * HISTORY: V1.0 --- Driver creation
 *V1.1Add share I2C address function
 *V1.2Fix the bug that sometimes sensor is stuck after system resume.
 *V1.3Add FIFO interfaces.
 *V1.4Use basic i2c function to read fifo data instead of i2c DMA mode.
 *V1.5Add compensated value performed by MTK acceleration calibration process.
 *V2.0change the driver node and change the driver node
 */

#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/platform_device.h>
#include <linux/atomic.h>
#include <linux/module.h>
#include <linux/gpio.h>

#include <hwmsensor.h>
#include <hwmsen_dev.h>
#include <sensors_io.h>
#include <hwmsen_helper.h>
#include <accel.h>
#include <cust_acc.h>
#include "bmi160_acc.h"
#include <step_counter.h>

#define SW_CALIBRATION
/*#define FIFO_READ_USE_DMA_MODE_I2C*/
#define BYTES_PER_LINE						(16)
#define ACCEL_DMA_MAX_TRANSACTION_LENGTH	1003
#define BMI160_SIGNIFICATION 1
/*#define BMI160_STEPDETECTOR 1*/
static const struct i2c_device_id bmi160_acc_i2c_id[] = {
	{BMI160_DEV_NAME, 0}, {}
};

static int bmi160_set_command_register(u8 cmd_reg);
static int bmi160_acc_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id);
static int bmi160_acc_i2c_remove(struct i2c_client *client);
static int bmi160_acc_local_init(void);
static int bmi160_acc_remove(void);

struct scale_factor {
	u8 whole;
	u8 fraction;
};

struct data_resolution {
	struct scale_factor scalefactor;
	int sensitivity;
};
/*! bmi160 sensor error status */
struct err_status {
	u8 fatal_err;
	u8 err_code;
	u8 i2c_fail;
	u8 drop_cmd;
	u8 mag_drdy_err;
	u8 err_st_all;
};
struct bmi160_acc_i2c_data {
	struct i2c_client *client;
	struct acc_hw *hw;
	struct hwmsen_convert cvt;
	struct bmi160_t device;
	/*misc */
	struct data_resolution *reso;
	atomic_t trace;
	atomic_t suspend;
	atomic_t filter;
	s16 cali_sw[BMI160_ACC_AXES_NUM + 1];
	struct mutex lock;
	/* +1: for 4-byte alignment */
	s8 offset[BMI160_ACC_AXES_NUM + 1];
	s16 data[BMI160_ACC_AXES_NUM + 1];
	u8 fifo_count;
	u8 fifo_head_en;
	u8 fifo_data_sel;
	u16 fifo_bytecount;
	struct odr_t odr;
	u64 fifo_time;
	atomic_t layout;
	struct work_struct irq_work;
	int IRQ;
	int IRQ_GPIO_NUM;
	struct delayed_work work;
	/*Pin Ctl functions*/
	struct pinctrl *bmi160_pinctrl;
	struct pinctrl_state *eint_as_int;
	struct pinctrl_state *gyro_eint_as_int;
	struct input_dev *input;
	atomic_t delay;
	int reg_sel;
	int reg_len;
	struct err_status err_st;
	u8 selftest;
	atomic_t selftest_result;
	/*early suspend */
#if defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_drv;
#endif
};

struct bmi160_axis_data_t {
	s16 x;
	s16 y;
	s16 z;
};
/*!bmi sensor generic power mode enum */
enum BMI_AXIS_TYPE {
	X_AXIS = 0,
	Y_AXIS,
	Z_AXIS,
	AXIS_MAX
};
static const char *bmi_axis_name[AXIS_MAX] = { "x", "y", "z" };

struct i2c_client *bmi160_acc_i2c_client;
static struct acc_init_info bmi160_acc_init_info;
static struct bmi160_acc_i2c_data *obj_i2c_data;
static bool sensor_power = true;
static struct GSENSOR_VECTOR3D gsensor_gain;

/* 0=ok, -1=fail */
static int bmi160_acc_init_flag = -1;
struct bmi160_t *p_bmi160;

struct acc_hw accel_cust;
static struct acc_hw *hw = &accel_cust;

static struct data_resolution bmi160_acc_data_resolution[] = {
	/*8 combination by {FULL_RES,RANGE} */
	{{0, 6}, 16384},	/*+/-2g  in 16-bit resolution:  0.06 mg/LSB */
	{{0, 12}, 8192},	/*+/-4g  in 16-bit resolution:  0.12 mg/LSB */
	{{0, 24}, 4096},	/*+/-8g  in 16-bit resolution:  0.24 mg/LSB */
	{{0, 5}, 2048},		/*+/-16g in 16-bit resolution:  0.49 mg/LSB */
};

static struct data_resolution bmi160_acc_offset_resolution = { {0, 12}, 8192 };

#ifdef FIFO_READ_USE_DMA_MODE_I2C
#include <linux/dma-mapping.h>
#ifndef I2C_MASK_FLAG
#define I2C_MASK_FLAG   (0x00ff)
#define I2C_DMA_FLAG    (0x2000)
#define I2C_ENEXT_FLAG  (0x0200)
#endif

static u8 *gpDMABuf_va;
static dma_addr_t gpDMABuf_pa;

static s32 i2c_dma_read(struct i2c_client *client, uint8_t addr,
			uint8_t *readbuf, int32_t readlen)
{
	int ret = 0;
	s32 retry = 0;
	u8 buffer[2];
	struct i2c_msg msg[2] = {
		{
		 .addr = (client->addr & I2C_MASK_FLAG),
		 .flags = 0,
		 .buf = buffer,
		 .len = 2,
		 /* .timing = I2C_MASTER_CLOCK */
		 },
		{
		 .addr = (client->addr & I2C_MASK_FLAG),
		 .ext_flag = (client->ext_flag | I2C_ENEXT_FLAG | I2C_DMA_FLAG),
		 .flags = I2C_M_RD,
		 .buf = (u8 *) gpDMABuf_pa,
		 .len = readlen,
		 /* .timing = I2C_MASTER_CLOCK */
		 },
	};
	buffer[0] = (addr >> 8) & 0xFF;
	buffer[1] = addr & 0xFF;
	if (NULL == readbuf) {
		ret = -1;
		return ret;
	}
	for (retry = 0; retry < 5; ++retry) {
		ret = i2c_transfer(client->adapter, &msg[0], 2);
		if (ret < 0)
			continue;
		memcpy(readbuf, gpDMABuf_va, readlen);
		return 0;
	}
	GSE_ERR("DMA I2C read error: 0x%04X, %d byte(s), err-code: %d", addr,
		readlen, ret);
	return ret;
}
#endif

/* I2C operation functions */
static int bma_i2c_read_block(struct i2c_client *client, u8 addr, u8 *data,
			      u8 len)
{
	int err;
	u8 beg = addr;
	struct i2c_msg msgs[2] = {
		{
		 .addr = client->addr,
		 .flags = 0,
		 .len = 1,
		 .buf = &beg},
		{
		 .addr = client->addr,
		 .flags = I2C_M_RD,
		 .len = len,
		 .buf = data,
		 }
	};
	if (!client)
		return -EINVAL;
	err =
	    i2c_transfer(client->adapter, msgs, sizeof(msgs) / sizeof(msgs[0]));
	if (err != 2) {
		GSE_ERR("i2c_transfer error: (%d %p %d) %d\n", addr, data, len,
			err);
		err = -EIO;
	} else {
		err = 0;	/*no error */
	}
	return err;
}

static int bma_i2c_write_block(struct i2c_client *client, u8 addr, u8 *data,
			       u8 len)
{
	/*
	 *because address also occupies one byte,
	 *the maximum length for write is 7 bytes
	 */
	int err, idx = 0, num = 0;
	char buf[32];
	if (!client)
		return -EINVAL;
	else if (len > C_I2C_FIFO_SIZE) {
		GSE_ERR(" length %d exceeds %d\n", len, C_I2C_FIFO_SIZE);
		return -EINVAL;
	}
	buf[num++] = addr;
	for (idx = 0; idx < len; idx++)
		buf[num++] = data[idx];
	err = i2c_master_send(client, buf, num);
	if (err < 0) {
		GSE_ERR("send command error!!\n");
		return -EFAULT;
	} else {
		err = 0;
	}
	return err;
}

s8 bmi_i2c_read_wrapper(u8 dev_addr, u8 reg_addr, u8 *data, u8 len)
{
	int err = 0;
	err = bma_i2c_read_block(bmi160_acc_i2c_client, reg_addr, data, len);
	if (err < 0)
		GSE_ERR("read bmi160 i2c failed.\n");
	return err;
}

s8 bmi_i2c_write_wrapper(u8 dev_addr, u8 reg_addr, u8 *data, u8 len)
{
	int err = 0;
	err = bma_i2c_write_block(bmi160_acc_i2c_client, reg_addr, data, len);
	if (err < 0)
		GSE_ERR("read bmi160 i2c failed.\n");
	return err;
}

static void bmi_delay(u32 msec)
{
	if (msec <= 20)
		usleep_range(msec * 1000, msec * 1000);
	else
		msleep(msec);
}

/*!
 * @brief Set data resolution
 *
 * @param[in] client the pointer of bmi160_acc_i2c_data
 *
 * @return zero success, non-zero failed
 */
static int bmi160_acc_set_resolution(struct bmi160_acc_i2c_data *obj)
{
	int res = 0;
	u8 dat, reso;

	mutex_lock(&obj->lock);
	res =
	    bma_i2c_read_block(obj->client, BMI160_USER_ACC_RANGE__REG, &dat,
			       1);
	mutex_unlock(&obj->lock);
	if (res) {
		GSE_ERR("read data format fail!!\n");
		return res;
	}
	dat = dat & BMI160_USER_ACC_RANGE__MSK;
	reso = 0xFF;
	switch (dat) {
	case BMI160_ACCEL_RANGE_2G:
		reso = 0;
		break;
	case BMI160_ACCEL_RANGE_4G:
		reso = 1;
		break;
	case BMI160_ACCEL_RANGE_8G:
		reso = 2;
		break;
	case BMI160_ACCEL_RANGE_16G:
		reso = 3;
		break;
	default:
		break;
	}
	if (reso <
	    sizeof(bmi160_acc_data_resolution) /
	    sizeof(bmi160_acc_data_resolution[0])) {
		obj->reso = &bmi160_acc_data_resolution[reso];
		GSE_LOG("bmi160_acc_set_resolution: %d\n",
			obj->reso->sensitivity);
		return 0;
	} else {
		return -EINVAL;
	}
}

static int bmi160_acc_read_data(struct i2c_client *client,
				s16 data[BMI160_ACC_AXES_NUM])
{
	int err = 0;
	u8 addr = BMI160_USER_DATA_14_ACC_X_LSB__REG;
	u8 buf[BMI160_ACC_DATA_LEN] = { 0 };
	err = bma_i2c_read_block(client, addr, buf, BMI160_ACC_DATA_LEN);
	if (err)
		GSE_ERR("read data failed.\n");
	else {
		/* Convert sensor raw data to 16-bit integer */
		data[BMI160_ACC_AXIS_X] = (s16) ((((s32) ((s8) buf[1]))
						  << BMI160_SHIFT_8_POSITION) |
						 (buf[0]));
		data[BMI160_ACC_AXIS_Y] = (s16) ((((s32) ((s8) buf[3]))
						  << BMI160_SHIFT_8_POSITION) |
						 (buf[2]));
		data[BMI160_ACC_AXIS_Z] = (s16) ((((s32) ((s8) buf[5]))
						  << BMI160_SHIFT_8_POSITION) |
						 (buf[4]));
	}
	return err;
}

static int bmi160_acc_read_offset(struct i2c_client *client,
				  s8 ofs[BMI160_ACC_AXES_NUM])
{
	int err = 0;
#ifdef SW_CALIBRATION
	ofs[0] = ofs[1] = ofs[2] = 0x0;
#else
	err =
	    bma_i2c_read_block(client, BMI160_USER_OFFSET_0_ADDR, ofs,
			       BMI160_ACC_AXES_NUM);
	if (err)
		GSE_ERR("error: %d\n", err);
#endif
	return err;
}

/*!
 * @brief Reset calibration for acc
 *
 * @param[in] client the pointer of i2c_client
 *
 * @return zero success, non-zero failed
 */
static int bmi160_acc_reset_calib(struct i2c_client *client)
{
	int err = 0;
	struct bmi160_acc_i2c_data *obj = obj_i2c_data;
#ifdef SW_CALIBRATION

#else
	u8 ofs[4] = { 0, 0, 0, 0 };
	err = bma_i2c_write_block(client, BMI160_ACC_REG_OFSX, ofs, 4);
	if (err)
		GSE_ERR("write offset failed.\n");
#endif
	memset(obj->cali_sw, 0x00, sizeof(obj->cali_sw));
	memset(obj->offset, 0x00, sizeof(obj->offset));
	return err;
}

static int bmi160_acc_read_calibration(struct i2c_client *client,
				       int dat[BMI160_ACC_AXES_NUM])
{
	struct bmi160_acc_i2c_data *obj = obj_i2c_data;
	int err = 0;
	int mul;
#ifdef SW_CALIBRATION
	mul = 0;		/*only SW Calibration, disable HW Calibration */
#else
	err = bmi160_acc_read_offset(client, obj->offset);
	if (err) {
		GSE_ERR("read offset fail, %d\n", err);
		return err;
	}
	mul = obj->reso->sensitivity / bmi160_acc_offset_resolution.sensitivity;
#endif

	dat[obj->cvt.map[BMI160_ACC_AXIS_X]] = obj->cvt.sign[BMI160_ACC_AXIS_X]
	    * (obj->offset[BMI160_ACC_AXIS_X] * mul +
	       obj->cali_sw[BMI160_ACC_AXIS_X]);
	dat[obj->cvt.map[BMI160_ACC_AXIS_Y]] = obj->cvt.sign[BMI160_ACC_AXIS_Y]
	    * (obj->offset[BMI160_ACC_AXIS_Y] * mul +
	       obj->cali_sw[BMI160_ACC_AXIS_Y]);
	dat[obj->cvt.map[BMI160_ACC_AXIS_Z]] = obj->cvt.sign[BMI160_ACC_AXIS_Z]
	    * (obj->offset[BMI160_ACC_AXIS_Z] * mul +
	       obj->cali_sw[BMI160_ACC_AXIS_Z]);

	return err;
}

static int bmi160_acc_write_calibration(struct i2c_client *client,
					int dat[BMI160_ACC_AXES_NUM])
{
	int err = 0;
	int cali[BMI160_ACC_AXES_NUM] = { 0 };
	int raw[BMI160_ACC_AXES_NUM] = { 0 };
	struct bmi160_acc_i2c_data *obj = obj_i2c_data;
#ifndef SW_CALIBRATION
	int lsb = bmi160_acc_offset_resolution.sensitivity;
	int divisor = obj->reso->sensitivity / lsb;
#endif
	GSE_LOG("OLDOFF:(%+3d%+3d%+3d):(%+3d%+3d %+3d)/(%+3d%+3d%+3d)\n",
	raw[BMI160_ACC_AXIS_X], raw[BMI160_ACC_AXIS_Y], raw[BMI160_ACC_AXIS_Z],
	obj->offset[BMI160_ACC_AXIS_X], obj->offset[BMI160_ACC_AXIS_Y],
	obj->offset[BMI160_ACC_AXIS_Z],
	obj->cali_sw[BMI160_ACC_AXIS_X], obj->cali_sw[BMI160_ACC_AXIS_Y],
	obj->cali_sw[BMI160_ACC_AXIS_Z]);
	/* calculate the real offset expected by caller */
	cali[BMI160_ACC_AXIS_X] += dat[BMI160_ACC_AXIS_X];
	cali[BMI160_ACC_AXIS_Y] += dat[BMI160_ACC_AXIS_Y];
	cali[BMI160_ACC_AXIS_Z] += dat[BMI160_ACC_AXIS_Z];
	GSE_LOG("UPDATE: (%+3d %+3d %+3d)\n",
	dat[BMI160_ACC_AXIS_X], dat[BMI160_ACC_AXIS_Y], dat[BMI160_ACC_AXIS_Z]);
#ifdef SW_CALIBRATION
	obj->cali_sw[BMI160_ACC_AXIS_X] = obj->cvt.sign[BMI160_ACC_AXIS_X]
	    * (cali[obj->cvt.map[BMI160_ACC_AXIS_X]]);
	obj->cali_sw[BMI160_ACC_AXIS_Y] = obj->cvt.sign[BMI160_ACC_AXIS_Y]
	    * (cali[obj->cvt.map[BMI160_ACC_AXIS_Y]]);
	obj->cali_sw[BMI160_ACC_AXIS_Z] = obj->cvt.sign[BMI160_ACC_AXIS_Z]
	    * (cali[obj->cvt.map[BMI160_ACC_AXIS_Z]]);
#else
	obj->offset[BMI160_ACC_AXIS_X] = (s8)(obj->cvt.sign[BMI160_ACC_AXIS_X]
	* (cali[obj->cvt.map[BMI160_ACC_AXIS_X]])/(divisor));
	obj->offset[BMI160_ACC_AXIS_Y] = (s8)(obj->cvt.sign[BMI160_ACC_AXIS_Y]
	* (cali[obj->cvt.map[BMI160_ACC_AXIS_Y]])/(divisor));
	obj->offset[BMI160_ACC_AXIS_Z] = (s8)(obj->cvt.sign[BMI160_ACC_AXIS_Z]
	* (cali[obj->cvt.map[BMI160_ACC_AXIS_Z]])/(divisor));

	/*convert software calibration using standard calibration */
	obj->cali_sw[BMI160_ACC_AXIS_X] = obj->cvt.sign[BMI160_ACC_AXIS_X]
	    * (cali[obj->cvt.map[BMI160_ACC_AXIS_X]]) % (divisor);
	obj->cali_sw[BMI160_ACC_AXIS_Y] = obj->cvt.sign[BMI160_ACC_AXIS_Y]
	    * (cali[obj->cvt.map[BMI160_ACC_AXIS_Y]]) % (divisor);
	obj->cali_sw[BMI160_ACC_AXIS_Z] = obj->cvt.sign[BMI160_ACC_AXIS_Z]
	    * (cali[obj->cvt.map[BMI160_ACC_AXIS_Z]]) % (divisor);

	GSE_LOG("NEWOFF:(%+3d %+3d %+3d):(%+3d %+3d %+3d)/(%+3d %+3d %+3d)\n",
	obj->offset[BMI160_ACC_AXIS_X]*divisor +
	obj->cali_sw[BMI160_ACC_AXIS_X],
	obj->offset[BMI160_ACC_AXIS_Y]*divisor +
	obj->cali_sw[BMI160_ACC_AXIS_Y],
	obj->offset[BMI160_ACC_AXIS_Z]*divisor +
	obj->cali_sw[BMI160_ACC_AXIS_Z],
	obj->offset[BMI160_ACC_AXIS_X], obj->offset[BMI160_ACC_AXIS_Y],
	obj->offset[BMI160_ACC_AXIS_Z],
	obj->cali_sw[BMI160_ACC_AXIS_X], obj->cali_sw[BMI160_ACC_AXIS_Y],
	obj->cali_sw[BMI160_ACC_AXIS_Z]);

	err =
	    bma_i2c_write_block(obj->client, BMI160_USER_OFFSET_0_ADDR,
				obj->offset, BMI160_ACC_AXES_NUM);
	if (err) {
		GSE_ERR("write offset fail: %d\n", err);
		return err;
	}
#endif

	return err;
}

/*!
 * @brief check chip id
 *
 * @param[in] client the pointer of i2c_client
 *
 * @return zero success, non-zero failed
 */
static int bmi160_acc_check_chip_id(struct i2c_client *client)
{
	int err = 0;
	u8 databuf[2] = { 0 };
	err = bma_i2c_read_block(client, BMI160_USER_CHIP_ID__REG, databuf, 1);
	err = bma_i2c_read_block(client, BMI160_USER_CHIP_ID__REG, databuf, 1);
	if (err < 0) {
		GSE_ERR("read chip id failed.\n");
		return BMI160_ACC_ERR_I2C;
	}
	switch (databuf[0]) {
	case SENSOR_CHIP_ID_BMI:
	case SENSOR_CHIP_ID_BMI_C2:
	case SENSOR_CHIP_ID_BMI_C3:
		GSE_LOG("check chip id %x successfully.\n", databuf[0]);
		break;
	default:
		GSE_LOG("check chip id %d failed.\n", databuf[0]);
		break;
	}
	return err;
}

s8 bmi160_get_significant_motion_en(struct i2c_client *client,
				    u8 *v_intr_significant_motion_select_u8)
{
	/* variable used for return the status of communication result */
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL */
	/* read the significant or any motion interrupt */
	com_rslt = bma_i2c_read_block(client,
				      BMI160_USER_INTR_SIGNIFICATION_MOTION_SELECT__REG,
				      &v_data_u8,
				      BMI160_GEN_READ_WRITE_DATA_LENGTH);
	*v_intr_significant_motion_select_u8 =
	    BMI160_GET_BITSLICE(v_data_u8,
				BMI160_USER_INTR_SIGNIFICATION_MOTION_SELECT);
	return com_rslt;
}

s8 bmi160_get_step_detector_en(struct i2c_client *client, u8 *v_step_intr_u8)
{
	/* variable used for return the status of communication result */
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;

	/* read the step detector interrupt */
	com_rslt = bma_i2c_read_block(client,
				      BMI160_USER_INTR_ENABLE_2_STEP_DETECTOR_ENABLE__REG,
				      &v_data_u8,
				      BMI160_GEN_READ_WRITE_DATA_LENGTH);
	*v_step_intr_u8 =
	    BMI160_GET_BITSLICE(v_data_u8,
				BMI160_USER_INTR_ENABLE_2_STEP_DETECTOR_ENABLE);
	return com_rslt;
}

s8 bmi160_get_step_counter_en(struct i2c_client *client,
			      u8 *v_step_counter_u8)
{
	/* variable used for return the status of communication result */
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL */
	/* read the step counter */
	com_rslt = bma_i2c_read_block(client,
				      BMI160_USER_STEP_CONFIG_1_STEP_COUNT_ENABLE__REG,
				      &v_data_u8,
				      BMI160_GEN_READ_WRITE_DATA_LENGTH);
	*v_step_counter_u8 =
	    BMI160_GET_BITSLICE(v_data_u8,
				BMI160_USER_STEP_CONFIG_1_STEP_COUNT_ENABLE);
	return com_rslt;
}

/*!
 * @brief Set power mode for acc
 *
 * @param[in] client the pointer of i2c_client
 * @param[in] enable
 *
 * @return zero success, non-zero failed
 */
static int bmi160_acc_set_power_mode(struct i2c_client *client, bool enable)
{
	int err = 0;
	u8 databuf[2] = { 0 };
	struct bmi160_acc_i2c_data *obj = obj_i2c_data;
	uint8_t sig_enable = 0;
	uint8_t step_det_enable = 0;
	uint8_t step_counter_enable = 0;

	mutex_lock(&obj->lock);
	if (enable == true)
		databuf[0] = CMD_PMU_ACC_NORMAL;
	else {
		err =
		    bmi160_get_significant_motion_en(obj_i2c_data->client,
						     &sig_enable);
		err +=
		    bmi160_get_step_detector_en(obj_i2c_data->client,
						&step_det_enable);
		err +=
		    bmi160_get_step_counter_en(obj_i2c_data->client,
					       &step_counter_enable);
		if (err < 0) {
			GSE_ERR("write power mode failed.\n");
			mutex_unlock(&obj->lock);
			return BMI160_ACC_ERR_I2C;
		} else if ((sig_enable == 0) && (step_det_enable == 0)
			   && (step_counter_enable == 0))
			databuf[0] = CMD_PMU_ACC_SUSPEND;
		else {
			mutex_unlock(&obj->lock);
			return err;
		}
	}
	err =
	    bma_i2c_write_block(client, BMI160_CMD_COMMANDS__REG, &databuf[0],
				1);
	if (err < 0) {
		GSE_ERR("write power mode value to register failed.\n");
		mutex_unlock(&obj->lock);
		return BMI160_ACC_ERR_I2C;
	}
	sensor_power = enable;
	bmi_delay(1);
	mutex_unlock(&obj->lock);
	GSE_LOG("set power mode enable = %d ok!\n", enable);
	return 0;
}

/*!
 * @brief Set range value for acc
 *
 * @param[in] client the pointer of i2c_client
 * @param[in] range value
 *
 * @return zero success, non-zero failed
 */
static int bmi160_acc_set_data_range(struct i2c_client *client, u8 dataformat)
{
	int res = 0;
	u8 databuf[2] = { 0 };
	struct bmi160_acc_i2c_data *obj = obj_i2c_data;

	mutex_lock(&obj->lock);
	res =
	    bma_i2c_read_block(client, BMI160_USER_ACC_RANGE__REG, &databuf[0],
			       1);
	databuf[0] =
	    BMI160_SET_BITSLICE(databuf[0], BMI160_USER_ACC_RANGE, dataformat);
	res +=
	    bma_i2c_write_block(client, BMI160_USER_ACC_RANGE__REG, &databuf[0],
				1);
	bmi_delay(1);
	if (res < 0) {
		GSE_ERR("set range failed.\n");
		mutex_unlock(&obj->lock);
		return BMI160_ACC_ERR_I2C;
	}
	mutex_unlock(&obj->lock);
	GSE_LOG("bmi160_acc_set_data_range(0x%x)\n", dataformat);
	return bmi160_acc_set_resolution(obj);
}

/*!
 * @brief Set bandwidth for acc
 *
 * @param[in] client the pointer of i2c_client
 * @param[in] bandwidth value
 *
 * @return zero success, non-zero failed
 */
static int bmi160_acc_set_odr(struct i2c_client *client, u8 bwrate)
{
	int err = 0;
	u8 databuf[2] = { 0 };
	struct bmi160_acc_i2c_data *obj = obj_i2c_data;

	mutex_lock(&obj->lock);
	err =
	    bma_i2c_read_block(client, BMI160_USER_ACC_CONF_ODR__REG,
			       &databuf[0], 1);
	databuf[0] =
	    BMI160_SET_BITSLICE(databuf[0], BMI160_USER_ACC_CONF_ODR, bwrate);
	err +=
	    bma_i2c_write_block(client, BMI160_USER_ACC_CONF_ODR__REG,
				&databuf[0], 1);
	bmi_delay(20);
	if (err < 0) {
		GSE_ERR("set bandwidth failed, res = %d\n", err);
		mutex_unlock(&obj->lock);
		return BMI160_ACC_ERR_I2C;
	}
	GSE_LOG("set bandwidth = %d ok.\n", bwrate);
	mutex_unlock(&obj->lock);
	return err;
}

/*!
 * @brief Set OSR for acc
 *
 * @param[in] client the pointer of i2c_client
 *
 * @return zero success, non-zero failed
 */
static int bmi160_acc_set_osr4(struct i2c_client *client)
{
	int err = 0;
	uint8_t databuf[2] = { 0 };
	struct bmi160_acc_i2c_data *obj = obj_i2c_data;
	uint8_t bandwidth = BMI160_ACCEL_OSR4_AVG1;
	uint8_t accel_undersampling_parameter = 0;
	mutex_lock(&obj->lock);
	err =
	    bma_i2c_read_block(client, BMI160_USER_ACC_CONF_ODR__REG,
			       &databuf[0], 1);
	databuf[0] =
	    BMI160_SET_BITSLICE(databuf[0], BMI160_USER_ACC_CONF_ACC_BWP,
				bandwidth);
	databuf[0] =
	    BMI160_SET_BITSLICE(databuf[0],
				BMI160_USER_ACC_CONF_ACC_UNDER_SAMPLING,
				accel_undersampling_parameter);
	err +=
	    bma_i2c_write_block(client, BMI160_USER_ACC_CONF_ODR__REG,
				&databuf[0], 1);
	bmi_delay(10);
	if (err < 0) {
		GSE_ERR("set OSR failed.\n");
		mutex_unlock(&obj->lock);
		return BMI160_ACC_ERR_I2C;
	}
	GSE_LOG("[%s] acc_bmp = %d, acc_us = %d ok.\n", __func__, bandwidth,
		accel_undersampling_parameter);
	mutex_unlock(&obj->lock);
	return err;
}

/*!
 * @brief Set interrupt enable
 *
 * @param[in] client the pointer of i2c_client
 * @param[in] enable for interrupt
 *
 * @return zero success, non-zero failed
 */
static int bmi160_acc_set_int_enable(struct i2c_client *client, u8 enable)
{
	int err = 0;
	struct bmi160_acc_i2c_data *obj = obj_i2c_data;

	mutex_lock(&obj->lock);
	err =
	    bma_i2c_write_block(client, BMI160_USER_INT_EN_0_ADDR, &enable,
				0x01);
	bmi_delay(1);
	if (err < 0) {
		mutex_unlock(&obj->lock);
		return err;
	}
	err =
	    bma_i2c_write_block(client, BMI160_USER_INT_EN_1_ADDR, &enable,
				0x01);
	bmi_delay(1);
	if (err < 0) {
		mutex_unlock(&obj->lock);
		return err;
	}
	err =
	    bma_i2c_write_block(client, BMI160_USER_INT_EN_2_ADDR, &enable,
				0x01);
	bmi_delay(1);
	if (err < 0) {
		mutex_unlock(&obj->lock);
		return err;
	}
	mutex_unlock(&obj->lock);
	GSE_LOG("bmi160 set interrupt enable = %d ok.\n", enable);
	return 0;
}

 /*!
  *     @brief This API Reads
  *     step counter configuration
  *     from the register 0x7A bit 0 to 7
  *     and from the register 0x7B bit 0 to 2 and 4 to 7
  *
  *
  *  @param v_step_config_u16 : The value of step configuration
  *
  *     @return results of bus communication function
  *     @retval 0 -> Success
  *     @retval -1 -> Error
  *
  *
  */
BMI160_RETURN_FUNCTION_TYPE bmi160_get_step_config(u16 *v_step_config_u16)
{
	/* variable used for return the status of communication result */
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data1_u8r = BMI160_INIT_VALUE;
	u8 v_data2_u8r = BMI160_INIT_VALUE;
	u16 v_data3_u8r = BMI160_INIT_VALUE;
	/* Read the 0 to 7 bit */
	com_rslt =
	    p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
					   BMI160_USER_STEP_CONFIG_ZERO__REG,
					   &v_data1_u8r,
					   BMI160_GEN_READ_WRITE_DATA_LENGTH);
	/* Read the 8 to 10 bit */
	com_rslt +=
	    p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
					   BMI160_USER_STEP_CONFIG_ONE_CNF1__REG,
					   &v_data2_u8r,
					   BMI160_GEN_READ_WRITE_DATA_LENGTH);
	v_data2_u8r =
	    BMI160_GET_BITSLICE(v_data2_u8r, BMI160_USER_STEP_CONFIG_ONE_CNF1);
	v_data3_u8r = ((u16) ((((u32)
				((u8) v_data2_u8r))
			       << BMI160_SHIFT_BIT_POSITION_BY_08_BITS) |
			      (v_data1_u8r)));
	/* Read the 11 to 14 bit */
	com_rslt +=
	    p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
					   BMI160_USER_STEP_CONFIG_ONE_CNF2__REG,
					   &v_data1_u8r,
					   BMI160_GEN_READ_WRITE_DATA_LENGTH);
	v_data1_u8r =
	    BMI160_GET_BITSLICE(v_data1_u8r, BMI160_USER_STEP_CONFIG_ONE_CNF2);
	*v_step_config_u16 = ((u16) ((((u32)
				       ((u8) v_data1_u8r))
				      << BMI160_SHIFT_BIT_POSITION_BY_08_BITS) |
				     (v_data3_u8r)));

	return com_rslt;
}

 /*!
  *     @brief This API write
  *     step counter configuration
  *     from the register 0x7A bit 0 to 7
  *     and from the register 0x7B bit 0 to 2 and 4 to 7
  *
  *
  *  @param v_step_config_u16   :
  *     the value of  Enable step configuration
  *
  *     @return results of bus communication function
  *     @retval 0 -> Success
  *     @retval -1 -> Error
  *
  *
  */
BMI160_RETURN_FUNCTION_TYPE bmi160_set_step_config(u16 v_step_config_u16)
{
	/* variable used for return the status of communication result */
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data1_u8r = BMI160_INIT_VALUE;
	u8 v_data2_u8r = BMI160_INIT_VALUE;
	u16 v_data3_u16 = BMI160_INIT_VALUE;

	/* write the 0 to 7 bit */
	v_data1_u8r = (u8) (v_step_config_u16 & BMI160_STEP_CONFIG_0_7);
	p_bmi160->BMI160_BUS_WRITE_FUNC
	    (p_bmi160->dev_addr, BMI160_USER_STEP_CONFIG_ZERO__REG,
	     &v_data1_u8r, BMI160_GEN_READ_WRITE_DATA_LENGTH);
	/* write the 8 to 10 bit */
	com_rslt = p_bmi160->BMI160_BUS_READ_FUNC
	    (p_bmi160->dev_addr,
	     BMI160_USER_STEP_CONFIG_ONE_CNF1__REG, &v_data2_u8r,
	     BMI160_GEN_READ_WRITE_DATA_LENGTH);
	if (com_rslt == SUCCESS) {
		v_data3_u16 =
		    (u16) (v_step_config_u16 & BMI160_STEP_CONFIG_8_10);
		v_data1_u8r =
		    (u8) (v_data3_u16 >> BMI160_SHIFT_BIT_POSITION_BY_08_BITS);
		v_data2_u8r =
		    BMI160_SET_BITSLICE(v_data2_u8r,
					BMI160_USER_STEP_CONFIG_ONE_CNF1,
					v_data1_u8r);
		p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->dev_addr,
						BMI160_USER_STEP_CONFIG_ONE_CNF1__REG,
						&v_data2_u8r,
						BMI160_GEN_READ_WRITE_DATA_LENGTH);
	}
	/* write the 11 to 14 bit */
	com_rslt += p_bmi160->BMI160_BUS_READ_FUNC
	    (p_bmi160->dev_addr,
	     BMI160_USER_STEP_CONFIG_ONE_CNF2__REG, &v_data2_u8r,
	     BMI160_GEN_READ_WRITE_DATA_LENGTH);
	if (com_rslt == SUCCESS) {
		v_data3_u16 =
		    (u16) (v_step_config_u16 & BMI160_STEP_CONFIG_11_14);
		v_data1_u8r =
		    (u8) (v_data3_u16 >> BMI160_SHIFT_BIT_POSITION_BY_12_BITS);
		v_data2_u8r =
		    BMI160_SET_BITSLICE(v_data2_u8r,
					BMI160_USER_STEP_CONFIG_ONE_CNF2,
					v_data1_u8r);
		p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->dev_addr,
						BMI160_USER_STEP_CONFIG_ONE_CNF2__REG,
						&v_data2_u8r,
						BMI160_GEN_READ_WRITE_DATA_LENGTH);
	}

	return com_rslt;
}

 /*!
  *     @brief This API set Step counter modes
  *
  *
  *  @param  v_step_mode_u8 : The value of step counter mode
  *  value    |   mode
  * ----------|-----------
  *   0       | BMI160_STEP_NORMAL_MODE
  *   1       | BMI160_STEP_SENSITIVE_MODE
  *   2       | BMI160_STEP_ROBUST_MODE
  *
  *     @return results of bus communication function
  *     @retval 0 -> Success
  *     @retval -1 -> Error
  *
  *
  */
BMI160_RETURN_FUNCTION_TYPE bmi160_set_step_mode(u8 v_step_mode_u8)
{
	/* variable used for return the status of communication result */
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;

	switch (v_step_mode_u8) {
	case BMI160_STEP_NORMAL_MODE:
		com_rslt = bmi160_set_step_config(STEP_CONFIG_NORMAL);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		break;
	case BMI160_STEP_SENSITIVE_MODE:
		com_rslt = bmi160_set_step_config(STEP_CONFIG_SENSITIVE);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		break;
	case BMI160_STEP_ROBUST_MODE:
		com_rslt = bmi160_set_step_config(STEP_CONFIG_ROBUST);
		p_bmi160->delay_msec(BMI160_GEN_READ_WRITE_DELAY);
		break;
	default:
		com_rslt = E_BMI160_OUT_OF_RANGE;
		break;
	}

	return com_rslt;
}

/*!
 *	@brief This API is used to set the accel
 *	under sampling parameter form the register 0x40 bit 7
 *
 *
 *
 *
 *	@param  v_accel_under_sampling_u8 : The value of accel under sampling
 *	value    | under_sampling
 * ----------|---------------
 *  0x01     |  BMI160_ENABLE
 *  0x00     |  BMI160_DISABLE
 *
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_accel_under_sampling_parameter(u8
								      v_accel_under_sampling_u8)
{
/* variable used for return the status of communication result*/
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
/* check the p_bmi160 structure as NULL*/
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
	} else {
		if (v_accel_under_sampling_u8 <= BMI160_MAX_UNDER_SAMPLING) {
			com_rslt =
			    p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
							   BMI160_USER_ACCEL_CONFIG_ACCEL_UNDER_SAMPLING__REG,
							   &v_data_u8,
							   BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				/* write the accel under sampling parameter */
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
								BMI160_USER_ACCEL_CONFIG_ACCEL_UNDER_SAMPLING,
								v_accel_under_sampling_u8);
				com_rslt +=
				    p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
								    dev_addr,
								    BMI160_USER_ACCEL_CONFIG_ACCEL_UNDER_SAMPLING__REG,
								    &v_data_u8,
								    BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
			com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}

/*!
 *	@brief This API is used to set the
 *	accel output date rate form the register 0x40 bit 0 to 3
 *
 *
 *  @param  v_output_data_rate_u8 :The value of accel output date rate
 *  value |  output data rate
 * -------|--------------------------
 *	 0    |	BMI160_ACCEL_OUTPUT_DATA_RATE_RESERVED
 *	 1	  |	BMI160_ACCEL_OUTPUT_DATA_RATE_0_78HZ
 *	 2	  |	BMI160_ACCEL_OUTPUT_DATA_RATE_1_56HZ
 *	 3    |	BMI160_ACCEL_OUTPUT_DATA_RATE_3_12HZ
 *	 4    | BMI160_ACCEL_OUTPUT_DATA_RATE_6_25HZ
 *	 5	  |	BMI160_ACCEL_OUTPUT_DATA_RATE_12_5HZ
 *	 6	  |	BMI160_ACCEL_OUTPUT_DATA_RATE_25HZ
 *	 7	  |	BMI160_ACCEL_OUTPUT_DATA_RATE_50HZ
 *	 8	  |	BMI160_ACCEL_OUTPUT_DATA_RATE_100HZ
 *	 9	  |	BMI160_ACCEL_OUTPUT_DATA_RATE_200HZ
 *	 10	  |	BMI160_ACCEL_OUTPUT_DATA_RATE_400HZ
 *	 11	  |	BMI160_ACCEL_OUTPUT_DATA_RATE_800HZ
 *	 12	  |	BMI160_ACCEL_OUTPUT_DATA_RATE_1600HZ
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_accel_output_data_rate(u8
							      v_output_data_rate_u8)
{
	/* variable used for return the status of communication result */
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL */
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
	} else {
		/* accel output data rate selection */
		if ((v_output_data_rate_u8 != BMI160_INIT_VALUE) &&
		    (v_output_data_rate_u8 <=
		     BMI160_MAX_ACCEL_OUTPUT_DATA_RATE)) {
			/* write accel output data rate */
			com_rslt =
			    p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
							   BMI160_USER_ACCEL_CONFIG_OUTPUT_DATA_RATE__REG,
							   &v_data_u8,
							   BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
								BMI160_USER_ACCEL_CONFIG_OUTPUT_DATA_RATE,
								v_output_data_rate_u8);
				com_rslt +=
				    p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
								    dev_addr,
								    BMI160_USER_ACCEL_CONFIG_OUTPUT_DATA_RATE__REG,
								    &v_data_u8,
								    BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
			com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}

/*!
 *	@brief This API is used to set the ranges
 *	(g values) of the accel from the register 0x41 bit 0 to 3
 *  @param v_range_u8 : The value of accel g range
 *	value    | g_range
 * ----------|-----------
 *   0x03    | BMI160_ACCEL_RANGE_2G
 *   0x05    | BMI160_ACCEL_RANGE_4G
 *   0x08    | BMI160_ACCEL_RANGE_8G
 *   0x0C    | BMI160_ACCEL_RANGE_16G
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_accel_range(u8 v_range_u8)
{
	/* variable used for return the status of communication result */
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL */
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
	} else {
		if ((v_range_u8 == BMI160_ACCEL_RANGE0) ||
		    (v_range_u8 == BMI160_ACCEL_RANGE1) ||
		    (v_range_u8 == BMI160_ACCEL_RANGE3)
		    || (v_range_u8 == BMI160_ACCEL_RANGE4)) {
			com_rslt =
			    p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
							   BMI160_USER_ACCEL_RANGE__REG,
							   &v_data_u8,
							   BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 =
				    BMI160_SET_BITSLICE(v_data_u8,
							BMI160_USER_ACCEL_RANGE,
							v_range_u8);
				/* write the accel range */
				com_rslt +=
				    p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
								    dev_addr,
								    BMI160_USER_ACCEL_RANGE__REG,
								    &v_data_u8,
								    BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
			com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}

/*!
 *	@brief This API write accel self test amplitude
 *	from the register 0x6D bit 3
 *        select amplitude of the selftest deflection:
 *
 *  @param v_accel_selftest_amp_u8 : The value of accel self test amplitude
 *  Value  |  Description
 * --------|-------------
 *   0x00  | LOW
 *   0x01  | HIGH
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_accel_selftest_amp(u8
							  v_accel_selftest_amp_u8)
{
	/* variable used for return the status of communication result */
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL */
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
	} else {
		if (v_accel_selftest_amp_u8 <= BMI160_MAX_VALUE_SELFTEST_AMP) {
			/* write  self test amplitude */
			com_rslt =
			    p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
							   BMI160_USER_SELFTEST_AMP__REG,
							   &v_data_u8,
							   BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
								BMI160_USER_SELFTEST_AMP,
								v_accel_selftest_amp_u8);
				com_rslt +=
				    p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
								    dev_addr,
								    BMI160_USER_SELFTEST_AMP__REG,
								    &v_data_u8,
								    BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
			com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}

/*!
 *	@brief This API is used to get the
 *	accel output date rate form the register 0x40 bit 0 to 3
 *  @param  v_output_data_rate_u8 :The value of accel output date rate
 *  value |  output data rate
 * -------|--------------------------
 *	 0    |	BMI160_ACCEL_OUTPUT_DATA_RATE_RESERVED
 *	 1	  |	BMI160_ACCEL_OUTPUT_DATA_RATE_0_78HZ
 *	 2	  |	BMI160_ACCEL_OUTPUT_DATA_RATE_1_56HZ
 *	 3    |	BMI160_ACCEL_OUTPUT_DATA_RATE_3_12HZ
 *	 4    | BMI160_ACCEL_OUTPUT_DATA_RATE_6_25HZ
 *	 5	  |	BMI160_ACCEL_OUTPUT_DATA_RATE_12_5HZ
 *	 6	  |	BMI160_ACCEL_OUTPUT_DATA_RATE_25HZ
 *	 7	  |	BMI160_ACCEL_OUTPUT_DATA_RATE_50HZ
 *	 8	  |	BMI160_ACCEL_OUTPUT_DATA_RATE_100HZ
 *	 9	  |	BMI160_ACCEL_OUTPUT_DATA_RATE_200HZ
 *	 10	  |	BMI160_ACCEL_OUTPUT_DATA_RATE_400HZ
 *	 11	  |	BMI160_ACCEL_OUTPUT_DATA_RATE_800HZ
 *	 12	  |	BMI160_ACCEL_OUTPUT_DATA_RATE_1600HZ
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_accel_output_data_rate(u8 *
							      v_output_data_rate_u8)
{
	/* variable used for return the status of communication result */
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL */
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
	} else {
		/* read the accel output data rate */
		com_rslt =
		    p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
						   BMI160_USER_ACCEL_CONFIG_OUTPUT_DATA_RATE__REG,
						   &v_data_u8,
						   BMI160_GEN_READ_WRITE_DATA_LENGTH);
		*v_output_data_rate_u8 =
		    BMI160_GET_BITSLICE(v_data_u8,
					BMI160_USER_ACCEL_CONFIG_OUTPUT_DATA_RATE);
	}
	return com_rslt;
}

/*!
 *	@brief This API is used to get the ranges
 *	(g values) of the accel from the register 0x41 bit 0 to 3
 *  @param v_range_u8 : The value of accel g range
 *	value    | g_range
 * ----------|-----------
 *   0x03    | BMI160_ACCEL_RANGE_2G
 *   0x05    | BMI160_ACCEL_RANGE_4G
 *   0x08    | BMI160_ACCEL_RANGE_8G
 *   0x0C    | BMI160_ACCEL_RANGE_16G
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_accel_range(u8 *v_range_u8)
{
	/* variable used for return the status of communication result */
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL */
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
	} else {
		/* read the accel range */
		com_rslt =
		    p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
						   BMI160_USER_ACCEL_RANGE__REG,
						   &v_data_u8,
						   BMI160_GEN_READ_WRITE_DATA_LENGTH);
		*v_range_u8 =
		    BMI160_GET_BITSLICE(v_data_u8, BMI160_USER_ACCEL_RANGE);
	}
	return com_rslt;
}

/*!
 *	@brief This API read accel self test amplitude
 *	from the register 0x6D bit 3
 *        select amplitude of the selftest deflection:
 *  @param v_accel_selftest_amp_u8 : The value of accel self test amplitude
 *  Value  |  Description
 * --------|-------------
 *   0x00  | LOW
 *   0x01  | HIGH
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_accel_selftest_amp(u8 *
							  v_accel_selftest_amp_u8)
{
	/* variable used for return the status of communication result */
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL */
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
	} else {
		/* read  self test amplitude */
		com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
							  BMI160_USER_SELFTEST_AMP__REG,
							  &v_data_u8,
							  BMI160_GEN_READ_WRITE_DATA_LENGTH);
		*v_accel_selftest_amp_u8 =
		    BMI160_GET_BITSLICE(v_data_u8, BMI160_USER_SELFTEST_AMP);
	}
	return com_rslt;
}

/*!
 *	@brief This API reads accelerometer data X values
 *	form the register 0x12 and 0x13
 *  @param v_accel_x_s16 : The value of accel x
 *	@note For accel configuration use the following functions
 *	@note bmi160_set_accel_output_data_rate()
 *	@note bmi160_set_accel_bw()
 *	@note bmi160_set_accel_under_sampling_parameter()
 *	@note bmi160_set_accel_range()
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_read_accel_x(s16 *v_accel_x_s16)
{
	/* variable used for return the status of communication result */
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	/* Array contains the accel X lSB and MSB data
	   v_data_u8[0] - LSB
	   v_data_u8[1] - MSB */
	u8 v_data_u8[BMI160_ACCEL_X_DATA_SIZE] = { BMI160_INIT_VALUE,
		BMI160_INIT_VALUE
	};
	/* check the p_bmi160 structure as NULL */
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
	} else {
		com_rslt =
		    p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
						   BMI160_USER_DATA_14_ACCEL_X_LSB__REG,
						   v_data_u8,
						   BMI160_ACCEL_DATA_LENGTH);

		*v_accel_x_s16 = (s16)
		    ((((s32) ((s8) v_data_u8[BMI160_ACCEL_X_MSB_BYTE]))
		      << BMI160_SHIFT_BIT_POSITION_BY_08_BITS)
		     | (v_data_u8[BMI160_ACCEL_X_LSB_BYTE]));
	}
	return com_rslt;
}

/*!
 *	@brief This API reads accelerometer data Y values
 *	form the register 0x14 and 0x15
 *  @param v_accel_y_s16 : The value of accel y
 *	@note For accel configuration use the following functions
 *	@note bmi160_set_accel_output_data_rate()
 *	@note bmi160_set_accel_bw()
 *	@note bmi160_set_accel_under_sampling_parameter()
 *	@note bmi160_set_accel_range()
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_read_accel_y(s16 *v_accel_y_s16)
{
	/* variable used for return the status of communication result */
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	/* Array contains the accel Y lSB and MSB data
	   v_data_u8[0] - LSB
	   v_data_u8[1] - MSB */
	u8 v_data_u8[BMI160_ACCEL_Y_DATA_SIZE] = { BMI160_INIT_VALUE,
		BMI160_INIT_VALUE
	};
	/* check the p_bmi160 structure as NULL */
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
	} else {
		com_rslt =
		    p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
						   BMI160_USER_DATA_16_ACCEL_Y_LSB__REG,
						   v_data_u8,
						   BMI160_ACCEL_DATA_LENGTH);

		*v_accel_y_s16 = (s16)
		    ((((s32) ((s8) v_data_u8[BMI160_ACCEL_Y_MSB_BYTE]))
		      << BMI160_SHIFT_BIT_POSITION_BY_08_BITS)
		     | (v_data_u8[BMI160_ACCEL_Y_LSB_BYTE]));
	}
	return com_rslt;
}

/*!
 *	@brief This API reads accelerometer data Z values
 *	form the register 0x16 and 0x17
 *  @param v_accel_z_s16 : The value of accel z
 *	@note For accel configuration use the following functions
 *	@note bmi160_set_accel_output_data_rate()
 *	@note bmi160_set_accel_bw()
 *	@note bmi160_set_accel_under_sampling_parameter()
 *	@note bmi160_set_accel_range()
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_read_accel_z(s16 *v_accel_z_s16)
{
	/* variable used for return the status of communication result */
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	/* Array contains the accel Z lSB and MSB data
	   a_data_u8r[LSB_ZERO] - LSB
	   a_data_u8r[MSB_ONE] - MSB */
	u8 a_data_u8r[BMI160_ACCEL_Z_DATA_SIZE] = {
		BMI160_INIT_VALUE, BMI160_INIT_VALUE
	};
	/* check the p_bmi160 structure as NULL */
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
	} else {
		com_rslt =
		    p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
						   BMI160_USER_DATA_18_ACCEL_Z_LSB__REG,
						   a_data_u8r,
						   BMI160_ACCEL_DATA_LENGTH);

		*v_accel_z_s16 = (s16)
		    ((((s32) ((s8) a_data_u8r[BMI160_ACCEL_Z_MSB_BYTE]))
		      << BMI160_SHIFT_BIT_POSITION_BY_08_BITS)
		     | (a_data_u8r[BMI160_ACCEL_Z_LSB_BYTE]));
	}
	return com_rslt;
}

/*!
 *	@brief This API write gyro self test trigger
 *
 *	@param v_gyro_selftest_start_u8: The value of gyro self test start
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_gyro_selftest_start(u8
							   v_gyro_selftest_start_u8)
{
	/* variable used for return the status of communication result */
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL */
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
	} else {
		if (v_gyro_selftest_start_u8 <= BMI160_MAX_VALUE_SELFTEST_START) {
			/* write gyro self test start */
			com_rslt =
			    p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
							   BMI160_USER_GYRO_SELFTEST_START__REG,
							   &v_data_u8,
							   BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
								BMI160_USER_GYRO_SELFTEST_START,
								v_gyro_selftest_start_u8);
				com_rslt +=
				    p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
								    dev_addr,
								    BMI160_USER_GYRO_SELFTEST_START__REG,
								    &v_data_u8,
								    BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
			com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}

/*!
 *	@brief This API reads the Gyroscope self test
 *	status from the register 0x1B bit 1
 *  @param v_gyro_selftest_u8 : The value of gyro self test status
 *  value    |   status
 *  ---------|----------------
 *   0       | Gyroscope self test is running or failed
 *   1       | Gyroscope self test completed successfully
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_gyro_selftest(u8 *v_gyro_selftest_u8)
{
	/* variable used for return the status of communication result */
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL */
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
	} else {
		com_rslt =
		    p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
						   BMI160_USER_STAT_GYRO_SELFTEST_OK__REG,
						   &v_data_u8,
						   BMI160_GEN_READ_WRITE_DATA_LENGTH);
		*v_gyro_selftest_u8 =
		    BMI160_GET_BITSLICE(v_data_u8,
					BMI160_USER_STAT_GYRO_SELFTEST_OK);
	}
	return com_rslt;
}

/*!
 *	@brief This API reads the error status
 *	from the error register 0x02 bit 0 to 7
 *  @param v_mag_data_rdy_err_u8 : The status of mag data ready interrupt
 *  @param v_fatal_er_u8r : The status of fatal error
 *  @param v_err_code_u8 : The status of error code
 *  @param v_i2c_fail_err_u8 : The status of I2C fail error
 *  @param v_drop_cmd_err_u8 : The status of drop command error
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_error_status(u8 *v_fatal_er_u8r,
						    u8 *v_err_code_u8,
						    u8 *v_i2c_fail_err_u8,
						    u8 *v_drop_cmd_err_u8,
						    u8 *v_mag_data_rdy_err_u8)
{
	/* variable used for return the status of communication result */
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL */
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
	} else {
		/* read the error codes */
		com_rslt =
		    p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
						   BMI160_USER_ERR_STAT__REG,
						   &v_data_u8,
						   BMI160_GEN_READ_WRITE_DATA_LENGTH);
		/* fatal error */
		*v_fatal_er_u8r =
		    BMI160_GET_BITSLICE(v_data_u8, BMI160_USER_FATAL_ERR);
		/* user error */
		*v_err_code_u8 =
		    BMI160_GET_BITSLICE(v_data_u8, BMI160_USER_ERR_CODE);
		/* i2c fail error */
		*v_i2c_fail_err_u8 =
		    BMI160_GET_BITSLICE(v_data_u8, BMI160_USER_I2C_FAIL_ERR);
		/* drop command error */
		*v_drop_cmd_err_u8 =
		    BMI160_GET_BITSLICE(v_data_u8, BMI160_USER_DROP_CMD_ERR);
		/* mag data ready error */
		*v_mag_data_rdy_err_u8 =
		    BMI160_GET_BITSLICE(v_data_u8,
					BMI160_USER_MAG_DADA_RDY_ERR);
	}
	return com_rslt;
}

static int bmi_get_err_status(struct bmi160_acc_i2c_data *client_data)
{
	int err = 0;

	err = bmi160_get_error_status(&client_data->err_st.fatal_err,
				      &client_data->err_st.err_code,
				      &client_data->err_st.i2c_fail,
				      &client_data->err_st.drop_cmd,
				      &client_data->err_st.mag_drdy_err);
	return err;
}

/*!
 * @brief This API write accel select axis to be self-test
 *
 *  @param v_accel_selftest_axis_u8 :
 *	The value of accel self test axis selection
 *  Value  |  Description
 * --------|-------------
 *   0x00  | disabled
 *   0x01  | x-axis
 *   0x02  | y-axis
 *   0x03  | z-axis
 *
 *
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
 *
 *
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_accel_selftest_axis(u8
							   v_accel_selftest_axis_u8)
{
	/* variable used for return the status of communication result */
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL */
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
	} else {
		if (v_accel_selftest_axis_u8 <= BMI160_MAX_ACCEL_SELFTEST_AXIS) {
			/* write accel self test axis */
			com_rslt =
			    p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
							   BMI160_USER_ACCEL_SELFTEST_AXIS__REG,
							   &v_data_u8,
							   BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
								BMI160_USER_ACCEL_SELFTEST_AXIS,
								v_accel_selftest_axis_u8);
				com_rslt +=
				    p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
								    dev_addr,
								    BMI160_USER_ACCEL_SELFTEST_AXIS__REG,
								    &v_data_u8,
								    BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
			com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}

/*!
 *	@brief This API write accel self test axis sign
 *	from the register 0x6D bit 2
 *
 *  @param v_accel_selftest_sign_u8: The value of accel self test axis sign
 *  Value  |  Description
 * --------|-------------
 *   0x00  | negative
 *   0x01  | positive
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_set_accel_selftest_sign(u8
							   v_accel_selftest_sign_u8)
{
	/* variable used for return the status of communication result */
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL */
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
	} else {
		if (v_accel_selftest_sign_u8 <= BMI160_MAX_VALUE_SELFTEST_SIGN) {
			/* write accel self test axis sign */
			com_rslt =
			    p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
							   BMI160_USER_ACCEL_SELFTEST_SIGN__REG,
							   &v_data_u8,
							   BMI160_GEN_READ_WRITE_DATA_LENGTH);
			if (com_rslt == SUCCESS) {
				v_data_u8 = BMI160_SET_BITSLICE(v_data_u8,
								BMI160_USER_ACCEL_SELFTEST_SIGN,
								v_accel_selftest_sign_u8);
				com_rslt +=
				    p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->
								    dev_addr,
								    BMI160_USER_ACCEL_SELFTEST_SIGN__REG,
								    &v_data_u8,
								    BMI160_GEN_READ_WRITE_DATA_LENGTH);
			}
		} else {
			com_rslt = E_BMI160_OUT_OF_RANGE;
		}
	}
	return com_rslt;
}

/*!
 *	@brief This API read accel self test axis sign
 *	from the register 0x6D bit 2
 *
 *  @param v_accel_selftest_sign_u8: The value of accel self test axis sign
 *  Value  |  Description
 * --------|-------------
 *   0x00  | negative
 *   0x01  | positive
 *	@return results of bus communication function
 *	@retval 0 -> Success
 *	@retval -1 -> Error
*/
BMI160_RETURN_FUNCTION_TYPE bmi160_get_accel_selftest_sign(u8 *
							   v_accel_selftest_sign_u8)
{
	/* variable used for return the status of communication result */
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	/* check the p_bmi160 structure as NULL */
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
	} else {
		/* read accel self test axis sign */
		com_rslt = p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
							  BMI160_USER_ACCEL_SELFTEST_SIGN__REG,
							  &v_data_u8,
							  BMI160_GEN_READ_WRITE_DATA_LENGTH);
		*v_accel_selftest_sign_u8 =
		    BMI160_GET_BITSLICE(v_data_u8,
					BMI160_USER_ACCEL_SELFTEST_SIGN);
	}
	return com_rslt;
}

/*!
 * @brief bmi160 initialization
 *
 * @param[in] client the pointer of i2c_client
 * @param[in] int reset calibration value
 *
 * @return zero success, non-zero failed
 */
static int bmi160_acc_init_client(struct i2c_client *client, int reset_cali)
{
	int err = 0;
	struct bmi160_acc_i2c_data *obj = obj_i2c_data;

	err = bmi160_acc_check_chip_id(client);
	if (err < 0)
		return err;
	/* soft reset */
	err = bmi160_set_command_register(0xB6);
	if (err < 0)
		return err;
	bmi_delay(5);
	err = bmi160_acc_set_odr(client, BMI160_ACCEL_ODR_200HZ);
	if (err < 0)
		return err;
	err = bmi160_acc_set_osr4(client);
	if (err < 0)
		return err;
	err = bmi160_acc_set_data_range(client, BMI160_ACCEL_RANGE_4G);
	if (err < 0)
		return err;
	gsensor_gain.x = gsensor_gain.y = gsensor_gain.z =
	    obj->reso->sensitivity;
	err = bmi160_acc_set_int_enable(client, 0);
	if (err < 0)
		return err;
	err = bmi160_acc_set_power_mode(client, false);
	if (err < 0)
		return err;
	if (0 != reset_cali) {
		/* reset calibration only in power on */
		err = bmi160_acc_reset_calib(client);
		if (err < 0)
			return err;
	}
	GSE_LOG("bmi160 acc init OK.\n");
	return 0;
}

static int bmi160_acc_read_chipInfo(struct i2c_client *client, char *buf,
				    int bufsize)
{
	snprintf(buf, PAGE_SIZE, "bmi160_acc");
	return 0;
}

/*!
 * @brief get the compensated data for gsensor
 *
 * @param[in] i2c_client client
 * @param[out] char save value
 * @param[in] int bufsize
 *
 * @return zero success, non-zero failed
 */
static int bmi160_acc_get_cps_data(struct i2c_client *client, char *buf,
				   int bufsize)
{
	int res = 0;
	int acc[BMI160_ACC_AXES_NUM] = { 0 };
	s16 databuf[BMI160_ACC_AXES_NUM] = { 0 };
	struct bmi160_acc_i2c_data *obj = obj_i2c_data;
	res = bmi160_acc_read_data(client, databuf);
	if (res) {
		GSE_ERR("read acc data failed.\n");
		return res;
	} else {
		/* Add compensated value performed by MTK calibration process */
		databuf[BMI160_ACC_AXIS_X] += obj->cali_sw[BMI160_ACC_AXIS_X];
		databuf[BMI160_ACC_AXIS_Y] += obj->cali_sw[BMI160_ACC_AXIS_Y];
		databuf[BMI160_ACC_AXIS_Z] += obj->cali_sw[BMI160_ACC_AXIS_Z];
		/*remap coordinate */
		acc[obj->cvt.map[BMI160_ACC_AXIS_X]] =
		    obj->cvt.sign[BMI160_ACC_AXIS_X] *
		    databuf[BMI160_ACC_AXIS_X];
		acc[obj->cvt.map[BMI160_ACC_AXIS_Y]] =
		    obj->cvt.sign[BMI160_ACC_AXIS_Y] *
		    databuf[BMI160_ACC_AXIS_Y];
		acc[obj->cvt.map[BMI160_ACC_AXIS_Z]] =
		    obj->cvt.sign[BMI160_ACC_AXIS_Z] *
		    databuf[BMI160_ACC_AXIS_Z];
		snprintf(buf, 96, "%d %d %d", (s16) acc[BMI160_ACC_AXIS_X],
			 (s16) acc[BMI160_ACC_AXIS_Y],
			 (s16) acc[BMI160_ACC_AXIS_Z]);
	}
	return 0;
}

static int bmi160_acc_read_sensor_data(struct i2c_client *client, char *buf,
				       int bufsize)
{
	int err = 0;
	int acc[BMI160_ACC_AXES_NUM] = { 0 };
	s16 databuf[BMI160_ACC_AXES_NUM] = { 0 };
	struct bmi160_acc_i2c_data *obj = obj_i2c_data;
	if (sensor_power == false) {
		err = bmi160_acc_set_power_mode(client, true);
		if (err) {
			GSE_ERR("set power on acc failed.\n");
			return err;
		}
	}
	err = bmi160_acc_read_data(client, databuf);
	if (err) {
		GSE_ERR("read acc data failed.\n");
		return err;
	} else {
		databuf[BMI160_ACC_AXIS_X] += obj->cali_sw[BMI160_ACC_AXIS_X];
		databuf[BMI160_ACC_AXIS_Y] += obj->cali_sw[BMI160_ACC_AXIS_Y];
		databuf[BMI160_ACC_AXIS_Z] += obj->cali_sw[BMI160_ACC_AXIS_Z];
		/* remap coordinate */
		acc[obj->cvt.map[BMI160_ACC_AXIS_X]] =
		    obj->cvt.sign[BMI160_ACC_AXIS_X] *
		    databuf[BMI160_ACC_AXIS_X];
		acc[obj->cvt.map[BMI160_ACC_AXIS_Y]] =
		    obj->cvt.sign[BMI160_ACC_AXIS_Y] *
		    databuf[BMI160_ACC_AXIS_Y];
		acc[obj->cvt.map[BMI160_ACC_AXIS_Z]] =
		    obj->cvt.sign[BMI160_ACC_AXIS_Z] *
		    databuf[BMI160_ACC_AXIS_Z];
		/* Output the mg */
		acc[BMI160_ACC_AXIS_X] =
		    acc[BMI160_ACC_AXIS_X] * GRAVITY_EARTH_1000 /
		    obj->reso->sensitivity;
		acc[BMI160_ACC_AXIS_Y] =
		    acc[BMI160_ACC_AXIS_Y] * GRAVITY_EARTH_1000 /
		    obj->reso->sensitivity;
		acc[BMI160_ACC_AXIS_Z] =
		    acc[BMI160_ACC_AXIS_Z] * GRAVITY_EARTH_1000 /
		    obj->reso->sensitivity;
		GSE_ERR("acc final xyz data: %d,%d,%d, sens:%d\n", acc[0],
			acc[1], acc[2], obj->reso->sensitivity);
		snprintf(buf, 96, "%04x %04x %04x", acc[BMI160_ACC_AXIS_X],
			 acc[BMI160_ACC_AXIS_Y], acc[BMI160_ACC_AXIS_Z]);
	}
	return 0;
}

static int bmi160_acc_read_raw_data(struct i2c_client *client, char *buf)
{
	int err = 0;
	s16 databuf[BMI160_ACC_AXES_NUM] = { 0 };
	err = bmi160_acc_read_data(client, databuf);
	if (err) {
		GSE_ERR("read acc raw data failed.\n");
		return -EIO;
	} else
		snprintf(buf, PAGE_SIZE,
			 "bmi160_acc_read_raw_data %04x %04x %04x",
			 databuf[BMI160_ACC_AXIS_X], databuf[BMI160_ACC_AXIS_Y],
			 databuf[BMI160_ACC_AXIS_Z]);
	return 0;
}

int bmi160_acc_get_mode(struct i2c_client *client, unsigned char *mode)
{
	int comres = 0;
	u8 v_data_u8r = C_BMI160_ZERO_U8X;
	comres =
	    bma_i2c_read_block(client, BMI160_USER_ACC_PMU_STATUS__REG,
			       &v_data_u8r, 1);
	*mode = BMI160_GET_BITSLICE(v_data_u8r, BMI160_USER_ACC_PMU_STATUS);
	return comres;
}

static int bmi160_acc_set_range(struct i2c_client *client, unsigned char range)
{
	return bmi160_acc_set_data_range(client, (u8) range);
}

static int bmi160_acc_get_range(struct i2c_client *client, u8 *range)
{
	int comres = 0;
	u8 data;
	comres =
	    bma_i2c_read_block(client, BMI160_USER_ACC_RANGE__REG, &data, 1);
	*range = BMI160_GET_BITSLICE(data, BMI160_USER_ACC_RANGE);
	return comres;
}

static int bmi160_acc_set_bandwidth(struct i2c_client *client,
				    unsigned char bandwidth)
{
	int comres = 0;
	unsigned char data[2] = { BMI160_USER_ACC_CONF_ODR__REG };
	struct bmi160_acc_i2c_data *obj = obj_i2c_data;
	GSE_LOG("[%s] bandwidth = %d\n", __func__, bandwidth);
	mutex_lock(&obj->lock);
	comres =
	    bma_i2c_read_block(client, BMI160_USER_ACC_CONF_ODR__REG, data + 1,
			       1);
	data[1] =
	    BMI160_SET_BITSLICE(data[1], BMI160_USER_ACC_CONF_ODR, bandwidth);
	comres = i2c_master_send(client, data, 2);
	bmi_delay(1);
	mutex_unlock(&obj->lock);
	if (comres <= 0)
		return BMI160_ACC_ERR_I2C;
	else
		return comres;
}

static int bmi160_acc_get_bandwidth(struct i2c_client *client,
				    unsigned char *bandwidth)
{
	int comres = 0;
	comres =
	    bma_i2c_read_block(client, BMI160_USER_ACC_CONF_ODR__REG, bandwidth,
			       1);
	*bandwidth = BMI160_GET_BITSLICE(*bandwidth, BMI160_USER_ACC_CONF_ODR);
	return comres;
}

static ssize_t show_chipinfo_value(struct device_driver *ddri, char *buf)
{
	char strbuf[BMI160_BUFSIZE];
	struct i2c_client *client = bmi160_acc_i2c_client;
	bmi160_acc_read_chipInfo(client, strbuf, BMI160_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}

static ssize_t show_acc_op_mode_value(struct device_driver *ddri, char *buf)
{
	int err = 0;
	u8 data = 0;
	err = bmi160_acc_get_mode(bmi160_acc_i2c_client, &data);
	if (err < 0) {
		GSE_ERR("get acc op mode failed.\n");
		return err;
	}
	return snprintf(buf, 32, "%d\n", data);
}

static ssize_t store_acc_op_mode_value(struct device_driver *ddri,
				       const char *buf, size_t count)
{
	int err;
	unsigned long data;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;
	GSE_LOG("store_acc_op_mode_value = %d .\n", (int)data);
	if (data == BMI160_ACC_MODE_NORMAL)
		err = bmi160_acc_set_power_mode(bmi160_acc_i2c_client, true);
	else
		err = bmi160_acc_set_power_mode(bmi160_acc_i2c_client, false);
	if (err < 0) {
		GSE_ERR("set acc op mode = %d failed.\n", (int)data);
		return err;
	}
	bmi_delay(50);
	return count;
}

static ssize_t show_acc_range_value(struct device_driver *ddri, char *buf)
{
	int err = 0;
	u8 data;
	err = bmi160_acc_get_range(bmi160_acc_i2c_client, &data);
	if (err < 0) {
		GSE_ERR("get acc range failed.\n");
		return err;
	}
	return snprintf(buf, 32, "%d\n", data);
}

static ssize_t store_acc_range_value(struct device_driver *ddri,
				     const char *buf, size_t count)
{
	unsigned long data;
	int err;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;
	err = bmi160_acc_set_range(bmi160_acc_i2c_client, (u8) data);
	if (err < 0) {
		GSE_ERR("set acc range = %d failed.\n", (int)data);
		return err;
	}
	return count;
}

static ssize_t show_acc_odr_value(struct device_driver *ddri, char *buf)
{
	int err;
	u8 data;

	err = bmi160_acc_get_bandwidth(bmi160_acc_i2c_client, &data);
	if (err < 0) {
		GSE_ERR("get acc odr failed.\n");
		return err;
	}
	return snprintf(buf, 32, "%d\n", data);
}

static ssize_t store_acc_odr_value(struct device_driver *ddri, const char *buf,
				   size_t count)
{
	unsigned long data;
	int err;
	struct bmi160_acc_i2c_data *client_data = obj_i2c_data;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;
	err = bmi160_acc_set_bandwidth(bmi160_acc_i2c_client, (u8) data);
	if (err < 0) {
		GSE_ERR("set acc bandwidth failed.\n");
		return err;
	}
	client_data->odr.acc_odr = data;
	return count;
}

static ssize_t show_cpsdata_value(struct device_driver *ddri, char *buf)
{
	char strbuf[BMI160_BUFSIZE] = { 0 };
	struct i2c_client *client = bmi160_acc_i2c_client;
	bmi160_acc_get_cps_data(client, strbuf, BMI160_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}

static ssize_t show_sensordata_value(struct device_driver *ddri, char *buf)
{
	char strbuf[BMI160_BUFSIZE] = { 0 };
	struct i2c_client *client = bmi160_acc_i2c_client;
	bmi160_acc_read_sensor_data(client, strbuf, BMI160_BUFSIZE);
	return snprintf(buf, PAGE_SIZE, "%s\n", strbuf);
}

static ssize_t show_cali_value(struct device_driver *ddri, char *buf)
{
	int err = 0;
	int len = 0;
	int mul = 0;
	int tmp[BMI160_ACC_AXES_NUM] = { 0 };
	struct bmi160_acc_i2c_data *obj;
	struct i2c_client *client = bmi160_acc_i2c_client;

	len = mul;
	obj = obj_i2c_data;
	err = bmi160_acc_read_offset(client, obj->offset);
	if (err)
		return -EINVAL;
	err = bmi160_acc_read_calibration(client, tmp);
	if (err)
		return -EINVAL;
	else {
		mul =
		    obj->reso->sensitivity /
		    bmi160_acc_offset_resolution.sensitivity;
		len +=
		    snprintf(buf + len, PAGE_SIZE - len,
			     "[HW ][%d] (%+3d, %+3d, %+3d) : (0x%02X, 0x%02X, 0x%02X)\n",
			     mul, obj->offset[BMI160_ACC_AXIS_X],
			     obj->offset[BMI160_ACC_AXIS_Y],
			     obj->offset[BMI160_ACC_AXIS_Z],
			     obj->offset[BMI160_ACC_AXIS_X],
			     obj->offset[BMI160_ACC_AXIS_Y],
			     obj->offset[BMI160_ACC_AXIS_Z]);
		len +=
		    snprintf(buf + len, PAGE_SIZE - len,
			     "[SW ][%d] (%+3d, %+3d, %+3d)\n", 1,
			     obj->cali_sw[BMI160_ACC_AXIS_X],
			     obj->cali_sw[BMI160_ACC_AXIS_Y],
			     obj->cali_sw[BMI160_ACC_AXIS_Z]);

		len += snprintf(buf + len, PAGE_SIZE - len,
				"[ALL]    (%+3d, %+3d, %+3d) : (%+3d, %+3d, %+3d)\n",
				obj->offset[BMI160_ACC_AXIS_X] * mul +
				obj->cali_sw[BMI160_ACC_AXIS_X],
				obj->offset[BMI160_ACC_AXIS_Y] * mul +
				obj->cali_sw[BMI160_ACC_AXIS_Y],
				obj->offset[BMI160_ACC_AXIS_Z] * mul +
				obj->cali_sw[BMI160_ACC_AXIS_Z],
				tmp[BMI160_ACC_AXIS_X], tmp[BMI160_ACC_AXIS_Y],
				tmp[BMI160_ACC_AXIS_Z]);
		GSE_ERR("len = %d mul = %d\n", len, mul);
		return len;
	}
}

static ssize_t store_cali_value(struct device_driver *ddri, const char *buf,
				size_t count)
{
	int err, x, y, z;
	int dat[BMI160_ACC_AXES_NUM] = { 0 };
	struct i2c_client *client = bmi160_acc_i2c_client;
	if (!strncmp(buf, "rst", 3)) {
		err = bmi160_acc_reset_calib(client);
		if (err)
			GSE_ERR("reset offset err = %d\n", err);
	} else if (3 == sscanf(buf, "0x%02X 0x%02X 0x%02X", &x, &y, &z)) {
		dat[BMI160_ACC_AXIS_X] = x;
		dat[BMI160_ACC_AXIS_Y] = y;
		dat[BMI160_ACC_AXIS_Z] = z;
		err = bmi160_acc_write_calibration(client, dat);
		if (err)
			GSE_ERR("write calibration err = %d\n", err);
	} else
		GSE_ERR("set calibration value by invalid format.\n");
	return count;
}

static ssize_t show_firlen_value(struct device_driver *ddri, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "not support\n");
}

static ssize_t store_firlen_value(struct device_driver *ddri, const char *buf,
				  size_t count)
{
	return count;
}

static ssize_t show_trace_value(struct device_driver *ddri, char *buf)
{
	int err;
	struct bmi160_acc_i2c_data *obj = obj_i2c_data;
	err = snprintf(buf, PAGE_SIZE, "0x%04X\n", atomic_read(&obj->trace));
	return err;
}

static ssize_t store_trace_value(struct device_driver *ddri, const char *buf,
				 size_t count)
{
	int trace;
	struct bmi160_acc_i2c_data *obj = obj_i2c_data;

	if (1 == sscanf(buf, "0x%2x", &trace))
		atomic_set(&obj->trace, trace);
	else
		GSE_ERR("invalid content: '%s'\n", buf);
	return count;
}

static ssize_t show_status_value(struct device_driver *ddri, char *buf)
{
	ssize_t len = 0;
	struct bmi160_acc_i2c_data *obj = obj_i2c_data;

	if (obj->hw) {
		len +=
		    snprintf(buf + len, PAGE_SIZE - len,
			     "CUST: %d %d (%d %d)\n", obj->hw->i2c_num,
			     obj->hw->direction, obj->hw->power_id,
			     obj->hw->power_vol);
	} else {
		len += snprintf(buf + len, PAGE_SIZE - len, "CUST: NULL\n");
	}
	return len;
}

static ssize_t show_power_status_value(struct device_driver *ddri, char *buf)
{
	if (sensor_power)
		GSE_LOG("G sensor in work mode, sensor_power = %d\n",
			sensor_power);
	else
		GSE_LOG("G sensor in standby mode, sensor_power = %d\n",
			sensor_power);

	return 0;
}

static int bmi160_fifo_length(uint32_t *fifo_length)
{
	int comres = 0;
	struct i2c_client *client = bmi160_acc_i2c_client;
	uint8_t a_data_u8r[2] = { 0, 0 };
	comres +=
	    bma_i2c_read_block(client, BMI160_USER_FIFO_BYTE_COUNTER_LSB__REG,
			       a_data_u8r, 2);
	a_data_u8r[1] =
	    BMI160_GET_BITSLICE(a_data_u8r[1],
				BMI160_USER_FIFO_BYTE_COUNTER_MSB);
	*fifo_length = (uint32_t) (((uint32_t)
				    ((uint8_t) (a_data_u8r[1]) <<
				     BMI160_SHIFT_8_POSITION))
				   | a_data_u8r[0]);

	return comres;
}

int bmi160_set_fifo_time_enable(u8 v_fifo_time_enable_u8)
{
	/* variable used for return the status of communication result */
	int com_rslt = -1;
	u8 v_data_u8 = 0;
	struct i2c_client *client = bmi160_acc_i2c_client;

	if (v_fifo_time_enable_u8 <= 1) {
		/* write the fifo sensor time */
		com_rslt =
		    bma_i2c_read_block(client,
				       BMI160_USER_FIFO_TIME_ENABLE__REG,
				       &v_data_u8, 1);
		if (com_rslt == 0) {
			v_data_u8 =
			    BMI160_SET_BITSLICE(v_data_u8,
						BMI160_USER_FIFO_TIME_ENABLE,
						v_fifo_time_enable_u8);
			com_rslt +=
			    bma_i2c_write_block(client,
						BMI160_USER_FIFO_TIME_ENABLE__REG,
						&v_data_u8, 1);
		}
	} else {
		com_rslt = -2;
	}
	return com_rslt;
}

int bmi160_set_fifo_header_enable(u8 v_fifo_header_enable_u8)
{
	/* variable used for return the status of communication result */
	int com_rslt = -1;
	u8 v_data_u8 = 0;
	struct i2c_client *client = bmi160_acc_i2c_client;
	if (v_fifo_header_enable_u8 <= 1) {
		/* read the fifo sensor header enable */
		com_rslt =
		    bma_i2c_read_block(client, BMI160_USER_FIFO_HEADER_EN__REG,
				       &v_data_u8, 1);
		if (com_rslt == 0) {
			v_data_u8 =
			    BMI160_SET_BITSLICE(v_data_u8,
						BMI160_USER_FIFO_HEADER_EN,
						v_fifo_header_enable_u8);
			com_rslt +=
			    bma_i2c_write_block(client,
						BMI160_USER_FIFO_HEADER_EN__REG,
						&v_data_u8, 1);
		}
	} else {
		com_rslt = -2;
	}
	return com_rslt;
}

static int bmi160_get_fifo_header_enable(u8 *v_fifo_header_u8)
{
	/* variable used for return the status of communication result */
	int com_rslt = -1;
	u8 v_data_u8 = 0;
	struct i2c_client *client = bmi160_acc_i2c_client;
	com_rslt =
	    bma_i2c_read_block(client, BMI160_USER_FIFO_HEADER_EN__REG,
			       &v_data_u8, 1);
	*v_fifo_header_u8 =
	    BMI160_GET_BITSLICE(v_data_u8, BMI160_USER_FIFO_HEADER_EN);
	return com_rslt;
}

BMI160_RETURN_FUNCTION_TYPE bmi160_read_reg(u8 v_addr_u8, u8 *v_data_u8,
					    u8 v_len_u8)
{
	/* variable used for return the status of communication result */
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	/* check the p_bmi160 structure as NULL */
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
	} else {
		/* Read data from register */
		com_rslt =
		    p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
						   v_addr_u8, v_data_u8,
						   v_len_u8);
	}
	return com_rslt;
}

BMI160_RETURN_FUNCTION_TYPE bmi160_write_reg(u8 v_addr_u8, u8 *v_data_u8,
					     u8 v_len_u8)
{
	/* variable used for return the status of communication result */
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	/* check the p_bmi160 structure as NULL */
	if (p_bmi160 == BMI160_NULL) {
		return E_BMI160_NULL_PTR;
	} else {
		/* write data from register */
		com_rslt =
		    p_bmi160->BMI160_BUS_WRITE_FUNC(p_bmi160->dev_addr,
						    v_addr_u8, v_data_u8,
						    v_len_u8);
	}
	return com_rslt;
}

BMI160_RETURN_FUNCTION_TYPE bmi160_init(struct bmi160_t *bmi160)
{
	/* variable used for return the status of communication result */
	BMI160_RETURN_FUNCTION_TYPE com_rslt = E_BMI160_COMM_RES;
	u8 v_data_u8 = BMI160_INIT_VALUE;
	u8 v_pmu_data_u8 = BMI160_INIT_VALUE;
	/* assign bmi160 ptr */
	p_bmi160 = bmi160;
	com_rslt =
	    p_bmi160->BMI160_BUS_READ_FUNC(p_bmi160->dev_addr,
					   BMI160_USER_CHIP_ID__REG, &v_data_u8,
					   BMI160_GEN_READ_WRITE_DATA_LENGTH);
	/* read Chip Id */
	p_bmi160->chip_id = v_data_u8;
	/* To avoid gyro wakeup it is required to write 0x00 to 0x6C */
	com_rslt +=
	    bmi160_write_reg(BMI160_USER_PMU_TRIGGER_ADDR, &v_pmu_data_u8,
			     BMI160_GEN_READ_WRITE_DATA_LENGTH);
	return com_rslt;
}

static int bmi160_set_command_register(u8 cmd_reg)
{
	int comres = 0;
	struct i2c_client *client = bmi160_acc_i2c_client;
	comres +=
	    bma_i2c_write_block(client, BMI160_CMD_COMMANDS__REG, &cmd_reg, 1);
	return comres;
}

static ssize_t bmi160_fifo_bytecount_show(struct device_driver *ddri, char *buf)
{
	int comres = 0;
	uint32_t fifo_bytecount = 0;
	uint8_t a_data_u8r[2] = { 0, 0 };
	struct i2c_client *client = bmi160_acc_i2c_client;
	comres +=
	    bma_i2c_read_block(client, BMI160_USER_FIFO_BYTE_COUNTER_LSB__REG,
			       a_data_u8r, 2);
	a_data_u8r[1] =
	    BMI160_GET_BITSLICE(a_data_u8r[1],
				BMI160_USER_FIFO_BYTE_COUNTER_MSB);
	fifo_bytecount = (uint32_t) (((uint32_t)
				      ((uint8_t) (a_data_u8r[1]) <<
				       BMI160_SHIFT_8_POSITION))
				     | a_data_u8r[0]);
	comres = snprintf(buf, 32, "%u\n", fifo_bytecount);
	return comres;
}

static ssize_t bmi160_fifo_bytecount_store(struct device_driver *ddri,
					   const char *buf, size_t count)
{
	int err;
	unsigned long data;
	struct bmi160_acc_i2c_data *client_data = obj_i2c_data;
	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;
	client_data->fifo_bytecount = (u16) data;
	return count;
}

static int bmi160_fifo_data_sel_get(struct bmi160_acc_i2c_data *client_data)
{
	int err = 0;
	struct i2c_client *client = bmi160_acc_i2c_client;
	unsigned char data;
	unsigned char fifo_acc_en, fifo_gyro_en, fifo_mag_en;
	unsigned char fifo_datasel;

	err =
	    bma_i2c_read_block(client, BMI160_USER_FIFO_ACC_EN__REG, &data, 1);
	fifo_acc_en = BMI160_GET_BITSLICE(data, BMI160_USER_FIFO_ACC_EN);

	err +=
	    bma_i2c_read_block(client, BMI160_USER_FIFO_GYRO_EN__REG, &data, 1);
	fifo_gyro_en = BMI160_GET_BITSLICE(data, BMI160_USER_FIFO_GYRO_EN);

	err +=
	    bma_i2c_read_block(client, BMI160_USER_FIFO_MAG_EN__REG, &data, 1);
	fifo_mag_en = BMI160_GET_BITSLICE(data, BMI160_USER_FIFO_MAG_EN);

	if (err)
		return err;

	fifo_datasel = (fifo_acc_en << BMI_ACC_SENSOR) |
	    (fifo_gyro_en << BMI_GYRO_SENSOR) | (fifo_mag_en << BMI_MAG_SENSOR);

	client_data->fifo_data_sel = fifo_datasel;

	return err;
}

static ssize_t bmi160_fifo_data_sel_show(struct device_driver *ddri, char *buf)
{
	int err = 0;
	struct bmi160_acc_i2c_data *client_data = obj_i2c_data;

	err = bmi160_fifo_data_sel_get(client_data);
	if (err)
		return -EINVAL;
	return snprintf(buf, 32, "%d\n", client_data->fifo_data_sel);
}

static ssize_t bmi160_fifo_data_sel_store(struct device_driver *ddri,
					  const char *buf, size_t count)
{
	struct bmi160_acc_i2c_data *client_data = obj_i2c_data;
	struct i2c_client *client = bmi160_acc_i2c_client;
	int err;
	unsigned long data;
	unsigned char fifo_datasel;
	unsigned char fifo_acc_en, fifo_gyro_en, fifo_mag_en;

	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;
	/* data format: aimed 0b0000 0x(m)x(g)x(a), x:1 enable, 0:disable */
	if (data > 7)
		return -EINVAL;

	fifo_datasel = (unsigned char)data;
	fifo_acc_en = fifo_datasel & (1 << BMI_ACC_SENSOR) ? 1 : 0;
	fifo_gyro_en = fifo_datasel & (1 << BMI_GYRO_SENSOR) ? 1 : 0;
	fifo_mag_en = fifo_datasel & (1 << BMI_MAG_SENSOR) ? 1 : 0;
	err +=
	    bma_i2c_read_block(client, BMI160_USER_FIFO_ACC_EN__REG,
			       &fifo_datasel, 1);
	fifo_datasel =
	    BMI160_SET_BITSLICE(fifo_datasel, BMI160_USER_FIFO_ACC_EN,
				fifo_acc_en);
	err +=
	    bma_i2c_write_block(client, BMI160_USER_FIFO_ACC_EN__REG,
				&fifo_datasel, 1);
	udelay(500);
	err +=
	    bma_i2c_read_block(client, BMI160_USER_FIFO_GYRO_EN__REG,
			       &fifo_datasel, 1);
	fifo_datasel =
	    BMI160_SET_BITSLICE(fifo_datasel, BMI160_USER_FIFO_GYRO_EN,
				fifo_gyro_en);
	err +=
	    bma_i2c_write_block(client, BMI160_USER_FIFO_GYRO_EN__REG,
				&fifo_datasel, 1);
	udelay(500);
	err +=
	    bma_i2c_read_block(client, BMI160_USER_FIFO_MAG_EN__REG,
			       &fifo_datasel, 1);
	fifo_datasel =
	    BMI160_SET_BITSLICE(fifo_datasel, BMI160_USER_FIFO_MAG_EN,
				fifo_mag_en);
	err +=
	    bma_i2c_write_block(client, BMI160_USER_FIFO_MAG_EN__REG,
				&fifo_datasel, 1);
	udelay(500);
	if (err)
		return -EIO;

	client_data->fifo_data_sel = (u8) data;
	GSE_LOG("fifo_data_sel %d, A_en:%d, G_en:%d, M_en:%d\n",
		client_data->fifo_data_sel, fifo_acc_en, fifo_gyro_en,
		fifo_mag_en);
	return count;
}

static ssize_t bmi160_fifo_data_out_frame_show(struct device_driver *ddri,
					       char *buf)
{
	int err = 0;
	uint32_t fifo_bytecount = 0;
	struct i2c_client *client = bmi160_acc_i2c_client;
	struct bmi160_acc_i2c_data *client_data = obj_i2c_data;

	err = bmi160_fifo_length(&fifo_bytecount);
	if (err < 0) {
		GSE_ERR("read fifo length error.\n");
		return -EINVAL;
	}
	if (0 == fifo_bytecount)
		return 0;
	client_data->fifo_bytecount = fifo_bytecount;
#ifdef FIFO_READ_USE_DMA_MODE_I2C
	err =
	    i2c_dma_read(client, BMI160_USER_FIFO_DATA__REG, buf,
			 client_data->fifo_bytecount);
#else
	err =
	    bma_i2c_read_block(client, BMI160_USER_FIFO_DATA__REG, buf,
			       fifo_bytecount);
#endif
	if (err < 0) {
		GSE_ERR("read fifo data error.\n");
		return snprintf(buf, PAGE_SIZE, "Read byte block error.");
	}
	return client_data->fifo_bytecount;
}

static ssize_t show_layout_value(struct device_driver *ddri, char *buf)
{
	struct bmi160_acc_i2c_data *data = obj_i2c_data;
	return snprintf(buf, PAGE_SIZE,
			"(%d, %d)\n[%+2d %+2d %+2d]\n[%+2d %+2d %+2d]\n",
			data->hw->direction, atomic_read(&data->layout),
			data->cvt.sign[0], data->cvt.sign[1],
			data->cvt.sign[2], data->cvt.map[0], data->cvt.map[1],
			data->cvt.map[2]);
}

static ssize_t store_layout_value(struct device_driver *ddri, const char *buf,
				  size_t count)
{
	struct bmi160_acc_i2c_data *data = obj_i2c_data;
	int layout = 0;

	if (1 == sscanf(buf, "%11d", &layout)) {
		atomic_set(&data->layout, layout);
		if (!hwmsen_get_convert(layout, &data->cvt)) {
			GSE_ERR("HWMSEN_GET_CONVERT error!\r\n");
		} else if (!hwmsen_get_convert(data->hw->direction, &data->cvt)) {
			GSE_ERR("invalid layout: %d, restore to %d\n", layout,
				data->hw->direction);
		} else {
			GSE_ERR("invalid layout: (%d, %d)\n", layout,
				data->hw->direction);
			hwmsen_get_convert(0, &data->cvt);
		}
	} else
		GSE_ERR("invalid format = '%s'\n", buf);

	return count;
}

static void bmi_dump_reg(struct bmi160_acc_i2c_data *client_data)
{
	int i;
	u8 dbg_buf0[REG_MAX0];
	u8 dbg_buf1[REG_MAX1];
	u8 dbg_buf_str0[REG_MAX0 * 3 + 1] = "";
	u8 dbg_buf_str1[REG_MAX1 * 3 + 1] = "";
	struct i2c_client *client = bmi160_acc_i2c_client;

	GSE_LOG("\nFrom 0x00:\n");
	bma_i2c_read_block(client, BMI160_USER_CHIP_ID__REG, dbg_buf0,
			   REG_MAX0);
	for (i = 0; i < REG_MAX0; i++) {
		snprintf(dbg_buf_str0 + i * 3, 48, "%02x%c", dbg_buf0[i],
			 (((i + 1) % BYTES_PER_LINE == 0) ? '\n' : ' '));
	}
	GSE_LOG("%s\n", dbg_buf_str0);

	bma_i2c_read_block(client, BMI160_USER_ACCEL_CONFIG_ADDR, dbg_buf1,
			   REG_MAX1);
	GSE_LOG("\nFrom 0x40:\n");
	for (i = 0; i < REG_MAX1; i++) {
		snprintf(dbg_buf_str1 + i * 3, 48, "%02x%c", dbg_buf1[i],
			 (((i + 1) % BYTES_PER_LINE == 0) ? '\n' : ' '));
	}
	GSE_LOG("\n%s\n", dbg_buf_str1);
}

static ssize_t bmi_register_show(struct device_driver *ddri, char *buf)
{
	struct bmi160_acc_i2c_data *client_data = obj_i2c_data;
	bmi_dump_reg(client_data);
	return snprintf(buf, 64, "Dump OK\n");
}

static ssize_t bmi_register_store(struct device_driver *ddri, const char *buf,
				  size_t count)
{
	int err;
	int reg_addr = 0;
	int data;
	u8 write_reg_add = 0;
	u8 write_data = 0;
	struct i2c_client *client = bmi160_acc_i2c_client;

	err = sscanf(buf, "%3d %3d", &reg_addr, &data);
	if (err < 2)
		return err;

	if (data > 0xff)
		return -EINVAL;

	write_reg_add = (u8) reg_addr;
	write_data = (u8) data;
	err += bma_i2c_write_block(client, write_reg_add, &write_data, 1);

	if (!err) {
		GSE_ERR("write reg 0x%2x, value= 0x%2x\n", reg_addr, data);
	} else {
		GSE_ERR("write reg fail\n");
		return err;
	}
	return count;
}

static ssize_t bmi160_bmi_value_show(struct device_driver *ddri, char *buf)
{
	int err;
	u8 raw_data[12] = { 0 };
	struct i2c_client *client = bmi160_acc_i2c_client;
	memset(raw_data, 0, sizeof(raw_data));
	err =
	    bma_i2c_read_block(client, BMI160_USER_DATA_8_GYRO_X_LSB__REG,
			       raw_data, 12);
	if (err)
		return err;
	/* output:gyro x y z acc x y z */
	return snprintf(buf, PAGE_SIZE, "%hd %d %hd %hd %hd %hd\n",
			(s16) (raw_data[1] << 8 | raw_data[0]),
			(s16) (raw_data[3] << 8 | raw_data[2]),
			(s16) (raw_data[5] << 8 | raw_data[4]),
			(s16) (raw_data[7] << 8 | raw_data[6]),
			(s16) (raw_data[9] << 8 | raw_data[8]),
			(s16) (raw_data[11] << 8 | raw_data[10]));
}

static int bmi160_get_fifo_wm(u8 *v_fifo_wm_u8)
{
	/* variable used for return the status of communication result */
	int com_rslt = -1;
	u8 v_data_u8 = 0;
	struct i2c_client *client = bmi160_acc_i2c_client;
	/* check the p_bmi160 structure as NULL */
	/* read the fifo water mark level */
	com_rslt =
	    bma_i2c_read_block(client, BMI160_USER_FIFO_WM__REG, &v_data_u8, 1);
	*v_fifo_wm_u8 = BMI160_GET_BITSLICE(v_data_u8, BMI160_USER_FIFO_WM);
	return com_rslt;
}

static int bmi160_set_fifo_wm(u8 v_fifo_wm_u8)
{
	/* variable used for return the status of communication result */
	int com_rslt = -1;
	struct i2c_client *client = bmi160_acc_i2c_client;
	/* write the fifo water mark level */
	com_rslt =
	    bma_i2c_write_block(client, BMI160_USER_FIFO_WM__REG, &v_fifo_wm_u8,
				1);
	return com_rslt;
}

static ssize_t bmi160_fifo_watermark_show(struct device_driver *ddri, char *buf)
{
	int err;
	u8 data = 0xff;
	err = bmi160_get_fifo_wm(&data);
	if (err)
		return err;
	return snprintf(buf, 32, "%d\n", data);
}

static ssize_t bmi160_fifo_watermark_store(struct device_driver *ddri,
					   const char *buf, size_t count)
{
	int err;
	unsigned long data;
	u8 fifo_watermark;
	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;

	fifo_watermark = (u8) data;
	err = bmi160_set_fifo_wm(fifo_watermark);
	if (err)
		return -EIO;

	GSE_LOG("set fifo watermark = %d ok.", (int)fifo_watermark);
	return count;
}

static ssize_t bmi160_delay_show(struct device_driver *ddri, char *buf)
{
	struct bmi160_acc_i2c_data *client_data = obj_i2c_data;
	return snprintf(buf, 32, "%d\n", atomic_read(&client_data->delay));
}

static ssize_t bmi160_delay_store(struct device_driver *ddri, const char *buf,
				  size_t count)
{
	int err;
	unsigned long data;
	struct bmi160_acc_i2c_data *client_data = obj_i2c_data;
	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;
	if (data < BMI_DELAY_MIN)
		data = BMI_DELAY_MIN;

	atomic_set(&client_data->delay, (unsigned int)data);
	return count;
}

static ssize_t bmi160_fifo_flush_store(struct device_driver *ddri,
				       const char *buf, size_t count)
{
	int err;
	unsigned long enable;
	err = kstrtoul(buf, 10, &enable);
	if (err)
		return err;
	if (enable)
		err = bmi160_set_command_register(CMD_CLR_FIFO_DATA);
	if (err)
		GSE_ERR("fifo flush failed!\n");
	return count;
}

static ssize_t bmi160_step_counter_mode_store(struct device_driver *ddri,
					      const char *buf, size_t count)
{
	int err;
	unsigned long mode;
	err = kstrtoul(buf, 10, &mode);
	if (err)
		return err;
	err = bmi160_set_step_mode((unsigned char)mode);
	if (err)
		GSE_ERR("set step_counter mode failed!\n");
	return count;
}

static ssize_t bmi160_fifo_header_en_show(struct device_driver *ddri, char *buf)
{
	int err;
	u8 data = 0xff;
	err = bmi160_get_fifo_header_enable(&data);
	if (err)
		return err;
	return snprintf(buf, 32, "%d\n", data);
}

static ssize_t bmi160_show_reg_sel(struct device_driver *ddri, char *buf)
{
	struct bmi160_acc_i2c_data *client_data = obj_i2c_data;

	if (client_data == NULL) {
		printk(KERN_ERR "Invalid client_data pointer");
		return -ENODEV;
	}

	return snprintf(buf, 64, "reg=0X%02X, len=%d\n", client_data->reg_sel,
			client_data->reg_len);
}

static ssize_t bmi160_store_reg_sel(struct device_driver *ddri, const char *buf,
				    size_t count)
{
	struct bmi160_acc_i2c_data *client_data = obj_i2c_data;
	ssize_t ret;

	if (client_data == NULL) {
		printk(KERN_ERR "Invalid client_data pointer");
		return -ENODEV;
	}
	ret =
	    sscanf(buf, "%11X %11d", &client_data->reg_sel,
		   &client_data->reg_len);
	if (ret != 2) {
		printk(KERN_ERR "Invalid argument");
		return -EINVAL;
	}

	return count;
}

static ssize_t bmi160_show_reg_val(struct device_driver *ddri, char *buf)
{
	struct bmi160_acc_i2c_data *client_data = obj_i2c_data;

	ssize_t ret;
	u8 reg_data[128], i;
	int pos;

	if (client_data == NULL) {
		printk(KERN_ERR "Invalid client_data pointer");
		return -ENODEV;
	}

	ret =
	    bmi160_read_reg(client_data->reg_sel, reg_data,
			    client_data->reg_len);
	if (ret < 0) {
		printk(KERN_ERR "Reg op failed");
		return ret;
	}

	pos = 0;
	for (i = 0; i < client_data->reg_len; ++i) {
		pos += snprintf(buf + pos, 16, "%02X", reg_data[i]);
		buf[pos++] = (i + 1) % 16 == 0 ? '\n' : ' ';
	}
	if (buf[pos - 1] == ' ')
		buf[pos - 1] = '\n';

	return pos;
}

static ssize_t bmi160_store_reg_val(struct device_driver *ddri, const char *buf,
				    size_t count)
{
	struct bmi160_acc_i2c_data *client_data = obj_i2c_data;
	ssize_t ret;
	u8 reg_data[32];
	int i, j, status, digit;

	if (client_data == NULL) {
		printk(KERN_ERR "Invalid client_data pointer");
		return -ENODEV;
	}
	status = 0;
	for (i = j = 0; i < count && j < client_data->reg_len; ++i) {
		if (buf[i] == ' ' || buf[i] == '\n' || buf[i] == '\t'
		    || buf[i] == '\r') {
			status = 0;
			++j;
			continue;
		}
		digit = buf[i] & 0x10 ? (buf[i] & 0xF) : ((buf[i] & 0xF) + 9);
		printk(KERN_INFO "digit is %d", digit);
		switch (status) {
		case 2:
			++j;	/* Fall thru */
		case 0:
			reg_data[j] = digit;
			status = 1;
			break;
		case 1:
			reg_data[j] = reg_data[j] * 16 + digit;
			status = 2;
			break;
		}
	}
	if (status > 0)
		++j;
	if (j > client_data->reg_len)
		j = client_data->reg_len;
	else if (j < client_data->reg_len) {
		printk(KERN_ERR "Invalid argument");
		return -EINVAL;
	}
	printk(KERN_INFO "Reg data read as");
	for (i = 0; i < j; ++i)
		printk(KERN_INFO "%d", reg_data[i]);

	ret =
	    bmi160_write_reg(client_data->reg_sel, reg_data,
			     client_data->reg_len);
	if (ret < 0) {
		printk(KERN_ERR "Reg op failed");
		return ret;
	}

	return count;
}

static ssize_t bmi160_fifo_header_en_store(struct device_driver *ddri,
					   const char *buf, size_t count)
{
	int err;
	unsigned long data;
	u8 fifo_header_en;
	struct bmi160_acc_i2c_data *client_data = obj_i2c_data;
	err = kstrtoul(buf, 10, &data);
	if (err)
		return err;
	if (data > 1)
		return -ENOENT;
	fifo_header_en = (u8) data;
	err = bmi160_set_fifo_header_enable(fifo_header_en);
	if (err)
		return -EIO;
	client_data->fifo_head_en = fifo_header_en;
	return count;
}

static ssize_t show_selftest(struct device_driver *ddri, char *buf)
{
	struct bmi160_acc_i2c_data *client_data = obj_i2c_data;
	return snprintf(buf, 16, "0x%x\n",
			atomic_read(&client_data->selftest_result));
}

/*!
 * @brief store selftest result which make up of acc and gyro
 * format: 0b 0000 xxxx  x:1 failed, 0 success
 * bit3:     gyro_self
 * bit2..0: acc_self z y x
 */
static ssize_t store_selftest(struct device_driver *ddri, const char *buf,
			      size_t count)
{
	int err = 0;
	int i = 0;
	u8 acc_selftest = 0;
	u8 gyro_selftest = 0;
	u8 bmi_selftest = 0;
	s16 axis_p_value, axis_n_value;
	u16 diff_axis[3] = { 0xff, 0xff, 0xff };
	u8 acc_odr, range, acc_selftest_amp, acc_selftest_sign;
	struct bmi160_acc_i2c_data *client_data = obj_i2c_data;

	GSE_LOG("Selftest for BMI16x starting.\n");

	client_data->selftest = 1;

	/*soft reset */
	err = bmi160_set_command_register(CMD_RESET_USER_REG);
	bmi_delay(70);
	err += bmi160_set_command_register(CMD_PMU_ACC_NORMAL);
	err += bmi160_set_command_register(CMD_PMU_GYRO_NORMAL);
	err += bmi160_set_accel_under_sampling_parameter(0);
	err +=
	    bmi160_set_accel_output_data_rate
	    (BMI160_ACCEL_OUTPUT_DATA_RATE_1600HZ);

	/* set to 8G range */
	err += bmi160_set_accel_range(BMI160_ACCEL_RANGE_8G);
	/* set to self amp high */
	err += bmi160_set_accel_selftest_amp(BMI_SELFTEST_AMP_HIGH);

	err += bmi160_get_accel_output_data_rate(&acc_odr);
	err += bmi160_get_accel_range(&range);
	err += bmi160_get_accel_selftest_amp(&acc_selftest_amp);
	err += bmi160_read_accel_x(&axis_n_value);

	GSE_LOG("acc_odr:%d, acc_range:%d, acc_selftest_amp:%d, acc_x:%d\n",
		acc_odr, range, acc_selftest_amp, axis_n_value);

	for (i = X_AXIS; i < AXIS_MAX; i++) {
		axis_n_value = 0;
		axis_p_value = 0;
		/* set every selftest axis */
		/*set_acc_selftest_axis(param),param x:1, y:2, z:3
		 * but X_AXIS:0, Y_AXIS:1, Z_AXIS:2
		 * so we need to +1*/
		err += bmi160_set_accel_selftest_axis(i + 1);
		bmi_delay(50);
		switch (i) {
		case X_AXIS:
			/* set negative sign */
			err += bmi160_set_accel_selftest_sign(0);
			err +=
			    bmi160_get_accel_selftest_sign(&acc_selftest_sign);

			bmi_delay(60);
			err += bmi160_read_accel_x(&axis_n_value);
			GSE_LOG("acc_x_selftest_sign:%d, axis_n_value:%d\n",
				acc_selftest_sign, axis_n_value);

			/* set postive sign */
			err += bmi160_set_accel_selftest_sign(1);
			err +=
			    bmi160_get_accel_selftest_sign(&acc_selftest_sign);

			bmi_delay(60);
			err += bmi160_read_accel_x(&axis_p_value);
			GSE_LOG("acc_x_selftest_sign:%d, axis_p_value:%d\n",
				acc_selftest_sign, axis_p_value);
			diff_axis[i] = abs(axis_p_value - axis_n_value);
			break;

		case Y_AXIS:
			/* set negative sign */
			err += bmi160_set_accel_selftest_sign(0);
			bmi_delay(60);
			err += bmi160_read_accel_y(&axis_n_value);
			/* set postive sign */
			err += bmi160_set_accel_selftest_sign(1);
			bmi_delay(60);
			err += bmi160_read_accel_y(&axis_p_value);
			diff_axis[i] = abs(axis_p_value - axis_n_value);
			break;

		case Z_AXIS:
			/* set negative sign */
			err += bmi160_set_accel_selftest_sign(0);
			bmi_delay(60);
			err += bmi160_read_accel_z(&axis_n_value);
			/* set postive sign */
			err += bmi160_set_accel_selftest_sign(1);
			bmi_delay(60);
			err += bmi160_read_accel_z(&axis_p_value);
			/* also start gyro self test */
			err += bmi160_set_gyro_selftest_start(1);
			bmi_delay(60);
			err += bmi160_get_gyro_selftest(&gyro_selftest);

			diff_axis[i] = abs(axis_p_value - axis_n_value);
			break;
		default:
			err += -EINVAL;
			break;
		}
		if (err) {
			GSE_ERR("Failed selftest axis:%s, p_val=%d, n_val=%d\n",
				bmi_axis_name[i], axis_p_value, axis_n_value);
			client_data->selftest = 0;
			return -EINVAL;
		}

		/*400mg for acc z axis */
		if (Z_AXIS == i) {
			if (diff_axis[i] < 1639) {
				acc_selftest |= 1 << i;
				GSE_ERR("Over selftest minimum for "
					"axis:%s,diff=%d,p_val=%d, n_val=%d\n",
					bmi_axis_name[i], diff_axis[i],
					axis_p_value, axis_n_value);
			}
		} else {
			/*800mg for x or y axis */
			if (diff_axis[i] < 3277) {
				acc_selftest |= 1 << i;

				if (bmi_get_err_status(client_data) < 0)
					return err;
				GSE_ERR("Over selftest minimum for "
					"axis:%s,diff=%d, p_val=%d, n_val=%d\n",
					bmi_axis_name[i], diff_axis[i],
					axis_p_value, axis_n_value);
				GSE_ERR("err_st:0x%x\n",
					client_data->err_st.err_st_all);

			}
		}

	}
	/* gyro_selftest==1,gyro selftest successfully,
	 * but bmi_result bit4 0 is successful, 1 is failed*/
	bmi_selftest = (acc_selftest & 0x0f) | ((!gyro_selftest) << AXIS_MAX);
	atomic_set(&client_data->selftest_result, bmi_selftest);
	/*soft reset */
	err = bmi160_set_command_register(CMD_RESET_USER_REG);
	if (err) {
		client_data->selftest = 0;
		return err;
	}
	bmi_delay(50);
	client_data->selftest = 0;
	GSE_LOG("Selftest for BMI16x finished\n");

	return count;
}

static DRIVER_ATTR(chipinfo, S_IWUSR | S_IRUGO, show_chipinfo_value, NULL);
static DRIVER_ATTR(cpsdata, S_IWUSR | S_IRUGO, show_cpsdata_value, NULL);
static DRIVER_ATTR(acc_op_mode, S_IWUSR | S_IRUGO, show_acc_op_mode_value,
		   store_acc_op_mode_value);
static DRIVER_ATTR(acc_range, S_IWUSR | S_IRUGO, show_acc_range_value,
		   store_acc_range_value);
static DRIVER_ATTR(acc_odr, S_IWUSR | S_IRUGO, show_acc_odr_value,
		   store_acc_odr_value);
static DRIVER_ATTR(sensordata, S_IWUSR | S_IRUGO, show_sensordata_value, NULL);
static DRIVER_ATTR(cali, S_IWUSR | S_IRUGO, show_cali_value, store_cali_value);
static DRIVER_ATTR(firlen, S_IWUSR | S_IRUGO, show_firlen_value,
		   store_firlen_value);
static DRIVER_ATTR(selftest, S_IWUSR | S_IRUGO, show_selftest, store_selftest);
static DRIVER_ATTR(trace, S_IWUSR | S_IRUGO, show_trace_value,
		   store_trace_value);
static DRIVER_ATTR(status, S_IRUGO, show_status_value, NULL);
static DRIVER_ATTR(powerstatus, S_IRUGO, show_power_status_value, NULL);
static DRIVER_ATTR(fifo_bytecount, S_IRUGO | S_IWUSR,
		   bmi160_fifo_bytecount_show, bmi160_fifo_bytecount_store);
static DRIVER_ATTR(fifo_data_sel, S_IRUGO | S_IWUSR, bmi160_fifo_data_sel_show,
		   bmi160_fifo_data_sel_store);
static DRIVER_ATTR(fifo_data_frame, S_IRUGO, bmi160_fifo_data_out_frame_show,
		   NULL);
static DRIVER_ATTR(layout, S_IRUGO | S_IWUSR, show_layout_value,
		   store_layout_value);
static DRIVER_ATTR(register, S_IRUGO | S_IWUSR, bmi_register_show,
		   bmi_register_store);
static DRIVER_ATTR(bmi_value, S_IRUGO, bmi160_bmi_value_show, NULL);
static DRIVER_ATTR(fifo_watermark, S_IRUGO | S_IWUSR,
		   bmi160_fifo_watermark_show, bmi160_fifo_watermark_store);
static DRIVER_ATTR(delay, S_IRUGO | S_IWUSR, bmi160_delay_show,
		   bmi160_delay_store);
static DRIVER_ATTR(fifo_flush, S_IWUSR | S_IRUGO, NULL,
		   bmi160_fifo_flush_store);
static DRIVER_ATTR(step_counter_mode, S_IWUSR | S_IRUGO, NULL,
		   bmi160_step_counter_mode_store);
static DRIVER_ATTR(fifo_header_en, S_IWUSR | S_IRUGO,
		   bmi160_fifo_header_en_show, bmi160_fifo_header_en_store);
static DRIVER_ATTR(reg_sel, S_IWUSR | S_IRUGO, bmi160_show_reg_sel,
		   bmi160_store_reg_sel);
static DRIVER_ATTR(reg_val, S_IWUSR | S_IRUGO, bmi160_show_reg_val,
		   bmi160_store_reg_val);

static struct driver_attribute *bmi160_acc_attr_list[] = {
/*chip information*/
	&driver_attr_chipinfo,
/*dump sensor data*/
	&driver_attr_sensordata,
/*show calibration data*/
	&driver_attr_cali,
/*filter length: 0: disable, others: enable*/
	&driver_attr_firlen,
/*trace log*/
	&driver_attr_trace,
	&driver_attr_status,
	&driver_attr_powerstatus,
/*g sensor data for compass tilt compensation*/
	&driver_attr_cpsdata,
/*g sensor opmode for compass tilt compensation*/
	&driver_attr_acc_op_mode,
/*g sensor range for compass tilt compensation*/
	&driver_attr_acc_range,
/*g sensor bandwidth for compass tilt compensation*/
	&driver_attr_acc_odr,
	&driver_attr_selftest,
	&driver_attr_step_counter_mode,
	&driver_attr_fifo_bytecount,
	&driver_attr_fifo_data_sel,
	&driver_attr_fifo_data_frame,
	&driver_attr_layout,
	&driver_attr_register,
	&driver_attr_bmi_value,
	&driver_attr_fifo_watermark,
	&driver_attr_delay,
	&driver_attr_fifo_flush,
	&driver_attr_fifo_header_en,
	&driver_attr_reg_sel,
	&driver_attr_reg_val,
};

static int bmi160_acc_create_attr(struct device_driver *driver)
{
	int err = 0;
	int idx = 0;
	int num =
	    (int)(sizeof(bmi160_acc_attr_list) /
		  sizeof(bmi160_acc_attr_list[0]));

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, bmi160_acc_attr_list[idx]);
		if (err) {
			GSE_ERR("create driver file (%s) failed.\n",
				bmi160_acc_attr_list[idx]->attr.name);
			break;
		}
	}
	return err;
}

static int bmi160_acc_delete_attr(struct device_driver *driver)
{
	int idx = 0;
	int err = 0;
	int num =
	    (int)(sizeof(bmi160_acc_attr_list) /
		  sizeof(bmi160_acc_attr_list[0]));
	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, bmi160_acc_attr_list[idx]);
	return err;
}

static int bmi160_acc_open(struct inode *inode, struct file *file)
{
	file->private_data = bmi160_acc_i2c_client;
	if (file->private_data == NULL) {
		GSE_ERR("file->private_data is null pointer.\n");
		return -EINVAL;
	}
	return nonseekable_open(inode, file);
}

static int bmi160_acc_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static long bmi160_acc_unlocked_ioctl(struct file *file, unsigned int cmd,
				      unsigned long arg)
{
	char strbuf[BMI160_BUFSIZE] = { 0 };
	void __user *data;
	struct SENSOR_DATA sensor_data;
	int err = 0;
	int cali[3] = { 0 };
	struct i2c_client *client = (struct i2c_client *)file->private_data;
	struct bmi160_acc_i2c_data *obj = obj_i2c_data;
	if (_IOC_DIR(cmd) & _IOC_READ) {
		err =
		    !access_ok(VERIFY_WRITE, (void __user *)arg,
			       _IOC_SIZE(cmd));
	} else if (_IOC_DIR(cmd) & _IOC_WRITE) {
		err =
		    !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	}
	if (err) {
		GSE_ERR("access error: %08X, (%2d, %2d)\n", cmd, _IOC_DIR(cmd),
			_IOC_SIZE(cmd));
		return -EFAULT;
	}
	GSE_LOG("bmi160_acc_unlocked_ioctl, cmd = 0x%x\n", cmd);
	switch (cmd) {
	case GSENSOR_IOCTL_INIT:
		bmi160_acc_init_client(client, 0);
		break;
	case GSENSOR_IOCTL_READ_CHIPINFO:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		bmi160_acc_read_chipInfo(client, strbuf, BMI160_BUFSIZE);
		if (copy_to_user(data, strbuf, strlen(strbuf) + 1))
			err = -EFAULT;
		break;
	case GSENSOR_IOCTL_READ_SENSORDATA:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		bmi160_acc_read_sensor_data(client, strbuf, BMI160_BUFSIZE);
		if (copy_to_user(data, strbuf, strlen(strbuf) + 1))
			err = -EFAULT;
		break;
	case GSENSOR_IOCTL_READ_GAIN:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		if (copy_to_user
		    (data, &gsensor_gain, sizeof(struct GSENSOR_VECTOR3D))) {
			err = -EFAULT;
		}
		break;
	case GSENSOR_IOCTL_READ_RAW_DATA:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		bmi160_acc_read_raw_data(client, strbuf);
		if (copy_to_user(data, &strbuf, strlen(strbuf) + 1))
			err = -EFAULT;
		break;
	case GSENSOR_IOCTL_SET_CALI:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		if (copy_from_user(&sensor_data, data, sizeof(sensor_data))) {
			err = -EFAULT;
			break;
		}
		if (atomic_read(&obj->suspend)) {
			GSE_ERR("can't calibration in suspend\n");
			err = -EINVAL;
		} else {
			cali[BMI160_ACC_AXIS_X] =
			    sensor_data.x * obj->reso->sensitivity /
			    GRAVITY_EARTH_1000;
			cali[BMI160_ACC_AXIS_Y] =
			    sensor_data.y * obj->reso->sensitivity /
			    GRAVITY_EARTH_1000;
			cali[BMI160_ACC_AXIS_Z] =
			    sensor_data.z * obj->reso->sensitivity /
			    GRAVITY_EARTH_1000;
			err = bmi160_acc_write_calibration(client, cali);
		}
		break;
	case GSENSOR_IOCTL_CLR_CALI:
		err = bmi160_acc_reset_calib(client);
		break;
	case GSENSOR_IOCTL_GET_CALI:
		data = (void __user *)arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		err = bmi160_acc_read_calibration(client, cali);
		if (err) {
			GSE_ERR("read calibration failed.\n");
			break;
		}
		sensor_data.x =
		    cali[BMI160_ACC_AXIS_X] * GRAVITY_EARTH_1000 /
		    obj->reso->sensitivity;
		sensor_data.y =
		    cali[BMI160_ACC_AXIS_Y] * GRAVITY_EARTH_1000 /
		    obj->reso->sensitivity;
		sensor_data.z =
		    cali[BMI160_ACC_AXIS_Z] * GRAVITY_EARTH_1000 /
		    obj->reso->sensitivity;
		if (copy_to_user(data, &sensor_data, sizeof(sensor_data)))
			err = -EFAULT;
		break;
	default:
		GSE_ERR("unknown IOCTL: 0x%08x\n", cmd);
		err = -ENOIOCTLCMD;
		break;
	}
	return err;
}

static const struct file_operations bmi160_acc_fops = {
	/* .owner = THIS_MODULE, */
	.open = bmi160_acc_open,
	.release = bmi160_acc_release,
	.unlocked_ioctl = bmi160_acc_unlocked_ioctl,
};

static struct miscdevice bmi160_acc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gsensor",
	.fops = &bmi160_acc_fops,
};

#ifndef CONFIG_HAS_EARLYSUSPEND
static int bmi160_acc_suspend(struct i2c_client *client, pm_message_t msg)
{
	struct bmi160_acc_i2c_data *obj = obj_i2c_data;
	if (msg.event == PM_EVENT_SUSPEND)
		atomic_set(&obj->suspend, 1);
	return 0;
}

static int bmi160_acc_resume(struct i2c_client *client)
{
	struct bmi160_acc_i2c_data *obj = obj_i2c_data;
	atomic_set(&obj->suspend, 0);
	return 0;
}

#else /*CONFIG_HAS_EARLY_SUSPEND is defined */

static void bmi160_acc_early_suspend(struct early_suspend *h)
{
	struct bmi160_acc_i2c_data *obj =
	    container_of(h, struct bmi160_acc_i2c_data, early_drv);
	atomic_set(&obj->suspend, 1);
	return;
}

static void bmi160_acc_late_resume(struct early_suspend *h)
{
	struct bmi160_acc_i2c_data *obj =
	    container_of(h, struct bmi160_acc_i2c_data, early_drv);
	atomic_set(&obj->suspend, 0);
	return;
}

#endif /*CONFIG_HAS_EARLYSUSPEND */

#ifdef CONFIG_OF
static const struct of_device_id gsensor_of_match[] = {
	{.compatible = "mediatek,gsensor",},
	{},
};
#endif

static struct i2c_driver bmi160_acc_i2c_driver = {
	.driver = {
		   .name = BMI160_DEV_NAME,
#ifdef CONFIG_OF
		   .of_match_table = gsensor_of_match,
#endif
		   },
	.probe = bmi160_acc_i2c_probe,
	.remove = bmi160_acc_i2c_remove,
#if !defined(CONFIG_HAS_EARLYSUSPEND)
	.suspend = bmi160_acc_suspend,
	.resume = bmi160_acc_resume,
#endif
	.id_table = bmi160_acc_i2c_id,
};

/*!
 * @brief if use this type of enable,
 * Gsensor should report inputEvent(x, y, z, status, div) to HAL
 *
 * @param[in] int open true or false
 *
 * @return zero success, non-zero failed
 */
static int bmi160_acc_open_report_data(int open)
{
	return 0;
}

/*!
 * @brief If use this type of enable, Gsensor only enabled but not report inputEvent to HAL
 *
 * @param[in] int enable true or false
 *
 * @return zero success, non-zero failed
 */
static int bmi160_acc_enable_nodata(int en)
{
	int err = 0;
	bool power = false;

	if (1 == en)
		power = true;
	else
		power = false;
	err = bmi160_acc_set_power_mode(obj_i2c_data->client, power);
	if (err < 0) {
		GSE_ERR("bmi160_acc_set_power_mode failed.\n");
		return err;
	}
	GSE_LOG("bmi160_acc_enable_nodata ok!\n");
	return err;
}

/*!
 * @brief set the delay value for acc
 *
 * @param[in] u64 ns for dealy
 *
 * @return zero success, non-zero failed
 */
static int bmi160_acc_set_delay(u64 ns)
{
	int value = 0;
	int sample_delay = 0;
	int err = 0;

	value = (int)ns / 1000 / 1000;
	if (value <= 5)
		sample_delay = BMI160_ACCEL_ODR_400HZ;
	else if (value <= 10)
		sample_delay = BMI160_ACCEL_ODR_200HZ;
	else
		sample_delay = BMI160_ACCEL_ODR_100HZ;
	err = bmi160_acc_set_odr(obj_i2c_data->client, sample_delay);
	if (err < 0) {
		GSE_ERR("set delay parameter error!\n");
		return err;
	}
	GSE_LOG("bmi160 acc set delay = (%d) ok.\n", value);
	return 0;
}

/*!
 * @brief get the raw data for gsensor
 *
 * @param[in] int x axis value
 * @param[in] int y axis value
 * @param[in] int z axis value
 * @param[in] int status
 *
 * @return zero success, non-zero failed
 */
static int bmi160_acc_get_data(int *x, int *y, int *z, int *status)
{
	int err = 0;
	char buff[BMI160_BUFSIZE];
	err =
	    bmi160_acc_read_sensor_data(obj_i2c_data->client, buff,
					BMI160_BUFSIZE);
	if (err < 0) {
		GSE_ERR("bmi160_acc_get_data failed.\n");
		return err;
	}
	sscanf(buff, "%x %x %x", x, y, z);
	*status = SENSOR_STATUS_ACCURACY_MEDIUM;
	return 0;
}

static void bmi_signification_motion_interrupt_handle(struct bmi160_acc_i2c_data
						      *client_data)
{
#if defined(BMI160_SIGNIFICATION)
	step_notify(TYPE_SIGNIFICANT);
	bmi160_set_command_register(CMD_RESET_INT_ENGINE);
#endif
}

static void bmi_stepdetector_interrupt_handle(struct bmi160_acc_i2c_data
					      *client_data)
{
#if defined(BMI160_STEPDETECTOR)
	step_notify(TYPE_STEP_DETECTOR);
#endif
}

static void bmi_irq_work_func(struct work_struct *work)
{
	struct bmi160_acc_i2c_data *client_data =
	    container_of((struct work_struct *)work,
			 struct bmi160_acc_i2c_data, irq_work);
	unsigned char int_status[4] = { 0, 0, 0, 0 };

	client_data->device.bus_read(client_data->device.dev_addr,
				     BMI160_USER_INTR_STAT_0_ADDR, int_status,
				     4);
	ACC_LOG("%x %x %x %x\n", int_status[0], int_status[1], int_status[2],
		int_status[3]);
	if (BMI160_GET_BITSLICE
	    (int_status[0], BMI160_USER_INTR_STAT_0_STEP_INTR))
		bmi_stepdetector_interrupt_handle(client_data);
	if (BMI160_GET_BITSLICE
	    (int_status[0], BMI160_USER_INTR_STAT_0_SIGNIFICANT_INTR))
		bmi_signification_motion_interrupt_handle(client_data);
}

static irqreturn_t bmi_irq_handler(int irq, void *handle)
{
	struct bmi160_acc_i2c_data *client_data = handle;
	schedule_work(&client_data->irq_work);
	return IRQ_HANDLED;
}

static int bmi160_request_irq(struct bmi160_acc_i2c_data *client_data)
{
	int err = 0;
	struct device_node *node = NULL;
	node = of_find_compatible_node(NULL, NULL, "mediatek,gsensor");

	if (node) {
		client_data->IRQ = irq_of_parse_and_map(node, 0);
		GSE_LOG("bmi160 request_irq number: (0x%x)", client_data->IRQ);
		err =
		    request_irq(client_data->IRQ, bmi_irq_handler,
				IRQF_TRIGGER_RISING, "bmi160_accint",
				client_data);
		if (err > 0)
			GSE_ERR("bmi160 request_irq failed");
	} else
		GSE_ERR("[%s] can not find!", __func__);

	GSE_LOG("irq_num= %d IRQ_num=%d\n", client_data->IRQ_GPIO_NUM,
		client_data->IRQ);
	INIT_WORK(&client_data->irq_work, bmi_irq_work_func);
	enable_irq(client_data->IRQ);
	return err;
}

static int bmi160_acc_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	int err = 0;
	struct i2c_client *new_client;
	struct bmi160_acc_i2c_data *obj;
	struct acc_control_path ctl = { 0 };
	struct acc_data_path data = { 0 };

	ACC_LOG("%s: is begin.\n", __func__);
	obj = kzalloc(sizeof(*obj), GFP_KERNEL);
	if (!obj) {
		err = -ENOMEM;
		goto exit;
	}
	memset(obj, 0, sizeof(struct bmi160_acc_i2c_data));
	obj->hw = hw;
	err = hwmsen_get_convert(obj->hw->direction, &obj->cvt);
	if (err) {
		ACC_ERR("invalid direction: %d\n", obj->hw->direction);
		goto exit;
	}
	obj_i2c_data = obj;
	client->addr = *hw->i2c_addr;
	obj->client = client;
	bmi160_acc_i2c_client = new_client = obj->client;
	i2c_set_clientdata(new_client, obj);
	atomic_set(&obj->trace, 0);
	atomic_set(&obj->suspend, 0);
	mutex_init(&obj->lock);
	/* h/w init */
	/*get sensor interrupt GPIO number*/
	if (client->dev.of_node) {
		const struct of_device_id *match;
		match = of_match_device(of_match_ptr(gsensor_of_match), &client->dev);
		if (!match) {
			GSE_LOG("Error: No device match found\n");
			return -ENODEV;
		}
	}

	obj->IRQ_GPIO_NUM = of_get_named_gpio(client->dev.of_node, "int-gpio", 0);
	GSE_LOG("g_vproc_vsel_gpio_number %d\n", obj->IRQ_GPIO_NUM);
	err = gpio_request_one(obj->IRQ_GPIO_NUM, GPIOF_IN,
				 "accel_int");
	if (err < 0) {
		GSE_ERR("Unable to request gpio int_pin\n");
		gpio_free(obj->IRQ_GPIO_NUM);
		return -EFAULT;
	}

	err = gpio_direction_input(obj->IRQ_GPIO_NUM);
	if (err < 0) {
		GSE_ERR("set_direction for irq gpio failed\n");
		return -EFAULT;
	}

	/*get interrupr pin ctl functions*/
	obj->bmi160_pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR_OR_NULL(obj->bmi160_pinctrl)) {
		GSE_ERR("Target does not use pinctrl\n");
		err = PTR_ERR(obj->bmi160_pinctrl);
		obj->bmi160_pinctrl = NULL;
		return err;
	}

	obj->eint_as_int = pinctrl_lookup_state(obj->bmi160_pinctrl, "state_eint_as_int");
	if (IS_ERR_OR_NULL(obj->eint_as_int)) {
		GSE_ERR("Can not get bmi state_eint_as_int pinstate\n");
		err = PTR_ERR(obj->eint_as_int);
		obj->bmi160_pinctrl = NULL;
		return err;
	}

	obj->gyro_eint_as_int = pinctrl_lookup_state(obj->bmi160_pinctrl, "state_gyro_eint_as_int");
	if (IS_ERR_OR_NULL(obj->gyro_eint_as_int)) {
		GSE_ERR("Can not get bmi gyro_eint_as_int pinstate\n");
		err = PTR_ERR(obj->gyro_eint_as_int);
		obj->bmi160_pinctrl = NULL;
		return err;
	}

	err = pinctrl_select_state(obj->bmi160_pinctrl, obj->eint_as_int);
	if (err) {
		GSE_ERR("Can not set bmi160 state_eint_as_int pinstate\n");
		return err;
	}
	err = pinctrl_select_state(obj->bmi160_pinctrl, obj->gyro_eint_as_int);
	if (err) {
		GSE_ERR("Can not set bmi160 state_gyro_eint_as_int pinstate\n");
		return err;
	}

	obj->device.bus_read = bmi_i2c_read_wrapper;
	obj->device.bus_write = bmi_i2c_write_wrapper;
	obj->device.delay_msec = bmi_delay;
	bmi160_init(&obj->device);

	err = bmi160_acc_init_client(new_client, 1);
	if (err)
		goto exit_init_failed;
#ifdef FIFO_READ_USE_DMA_MODE_I2C
	client->dev.coherent_dma_mask = DMA_BIT_MASK(32);
	client->dev.dma_mask = &client->dev.coherent_dma_mask;
	/* DMA size for customer */
	gpDMABuf_va = (u8 *) dma_alloc_coherent(&client->dev,
						ACCEL_DMA_MAX_TRANSACTION_LENGTH,
						&gpDMABuf_pa,
						GFP_KERNEL | GFP_DMA);
	if (!gpDMABuf_va) {
		GSE_ERR("[DMA][Error] Allocate DMA I2C Buffer failed!\n");
		goto exit;
	}
	memset(gpDMABuf_va, 0, ACCEL_DMA_MAX_TRANSACTION_LENGTH);
#endif
	err = misc_register(&bmi160_acc_device);
	if (err) {
		ACC_ERR("bmi160_acc_device register failed.\n");
		goto exit_misc_device_register_failed;
	}
	err =
	    bmi160_acc_create_attr(&
				   (bmi160_acc_init_info.platform_diver_addr->
				    driver));
	if (err) {
		ACC_ERR("create attribute failed.\n");
		goto exit_create_attr_failed;
	}
	ctl.open_report_data = bmi160_acc_open_report_data;
	ctl.enable_nodata = bmi160_acc_enable_nodata;
	ctl.set_delay = bmi160_acc_set_delay;
	ctl.is_report_input_direct = false;
	err = acc_register_control_path(&ctl);
	if (err) {
		ACC_ERR("register acc control path error.\n");
		goto exit_kfree;
	}
	data.get_data = bmi160_acc_get_data;
	data.vender_div = 1000;
	err = acc_register_data_path(&data);
	if (err) {
		ACC_ERR("register acc data path error.\n");
		goto exit_kfree;
	}
#ifdef CONFIG_HAS_EARLYSUSPEND
	obj->early_drv.level = EARLY_SUSPEND_LEVEL_DISABLE_FB - 1,
	    obj->early_drv.suspend = bmi160_acc_early_suspend,
	    obj->early_drv.resume =
	    bmi160_acc_late_resume, register_early_suspend(&obj->early_drv);
#endif

	/* fifo setting */
	bmi160_set_fifo_header_enable(ENABLE);
	bmi160_set_fifo_time_enable(DISABLE);
	bmi160_request_irq(obj_i2c_data);
	bmi160_acc_init_flag = 0;
	ACC_LOG("%s: is ok.\n", __func__);
	return 0;

      exit_create_attr_failed:
	misc_deregister(&bmi160_acc_device);
      exit_misc_device_register_failed:
      exit_init_failed:
      exit_kfree:
	kfree(obj);
      exit:
	ACC_ERR("%s: err = %d\n", __func__, err);
	bmi160_acc_init_flag = -1;
	return err;
}

static int bmi160_acc_i2c_remove(struct i2c_client *client)
{
	int err = 0;
	err =
	    bmi160_acc_delete_attr(&
				   (bmi160_acc_init_info.platform_diver_addr->
				    driver));
	if (err)
		GSE_ERR("delete device attribute failed.\n");
	err = misc_deregister(&bmi160_acc_device);
	if (err)
		GSE_ERR("misc_deregister fail: %d\n", err);

	bmi160_acc_i2c_client = NULL;
	i2c_unregister_device(client);
	kfree(obj_i2c_data);
	return 0;
}

int bmi160_acc_local_init(void)
{
	int err = 0;
	if (i2c_add_driver(&bmi160_acc_i2c_driver)) {
		GSE_ERR("add driver error\n");
		err = -1;
		return err;
	}
	if (-1 == bmi160_acc_init_flag) {
		GSE_LOG("bmi160 acc local init failed.\n");
		err = -1;
		return err;
	}
	GSE_LOG("bmi160 acc local init.\n");
	return 0;
}

static int bmi160_acc_remove(void)
{
	GSE_FUN();
	i2c_del_driver(&bmi160_acc_i2c_driver);
	return 0;
}

static struct acc_init_info bmi160_acc_init_info = {
	.name = BMI160_DEV_NAME,
	.init = bmi160_acc_local_init,
	.uninit = bmi160_acc_remove,
};

static int __init bmi160_acc_init(void)
{
	hw = get_accel_dts_func(COMPATIABLE_NAME, hw);
	if (!hw) {
		GSE_ERR("device tree configuration error!\n");
		return 0;
	}
	GSE_FUN();
	acc_driver_add(&bmi160_acc_init_info);
	return 0;
}

static void __exit bmi160_acc_exit(void)
{
	GSE_FUN();
}

module_init(bmi160_acc_init);
module_exit(bmi160_acc_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("BMI160_ACC I2C driver");
MODULE_AUTHOR("contact@bosch-sensortec.com>");
