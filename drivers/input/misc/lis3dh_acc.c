/******************** (C) COPYRIGHT 2010 STMicroelectronics ********************
 *
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * File Name          : lis3dh_acc.c
 * Authors            : MSH - Motion Mems BU - Application Team
 *		      : Matteo Dameno (matteo.dameno@st.com)
 *		      : Carmine Iascone (carmine.iascone@st.com)
 *                    : Samuel Huo (samuel.huo@st.com)
 * Version            : V.1.1.0
 * Date               : 07/10/2012
 * Description        : LIS3DH accelerometer sensor driver
 *
 *******************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THE PRESENT SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES
 * OR CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, FOR THE SOLE
 * PURPOSE TO SUPPORT YOUR APPLICATION DEVELOPMENT.
 * AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY DIRECT,
 * INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING FROM THE
 * CONTENT OF SUCH SOFTWARE AND/OR THE USE MADE BY CUSTOMERS OF THE CODING
 * INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
 *
 * THIS SOFTWARE IS SPECIFICALLY DESIGNED FOR EXCLUSIVE USE WITH ST PARTS.
 *
 ******************************************************************************
 Revision 1.0.0 05/11/09
 First Release;
 Revision 1.0.3 22/01/2010
  Linux K&R Compliant Release;
 Revision 1.0.5 16/08/2010
  modified _get_acceleration_data function;
  modified _update_odr function;
  manages 2 interrupts;
 Revision 1.0.6 15/11/2010
  supports sysfs;
  no more support for ioctl;
 Revision 1.0.7 26/11/2010
  checks for availability of interrupts pins
  correction on FUZZ and FLAT values;
 Revision 1.0.8 2010/Apr/01
  corrects a bug in interrupt pin management in 1.0.7
 Revision 1.0.9 07/25/2011
  Romove several unused functions,add 5ms delay in init,change sysfs attributes.
 Revision 1.1.0 07/10/2012
  To replace some deprecated functions for 3.4 kernel; to pass the checkpatch's formatting requirement;
  To add regulator request;

 ******************************************************************************/

#include	<linux/err.h>
#include	<linux/errno.h>
#include	<linux/delay.h>
#include	<linux/fs.h>
#include	<linux/i2c.h>
#include	<linux/input.h>
#include	<linux/uaccess.h>
#include	<linux/workqueue.h>
#include	<linux/irq.h>
#include	<linux/gpio.h>
#include	<linux/interrupt.h>
#include	<linux/slab.h>
#include	<linux/pm.h>
#include	<linux/input/lis3dh.h>
#include	<linux/module.h>
#include	<linux/regulator/consumer.h>
#include	<linux/of_gpio.h>
#include	<linux/sensors.h>

#define	DEBUG	1

#define	G_MAX		16000
#define LIS3DH_FIFO_SIZE	32
#define LIS3DH_TIME_MS_TO_NS	1000000L

#define SENSITIVITY_2G		1	/**	mg/LSB	*/
#define SENSITIVITY_4G		2	/**	mg/LSB	*/
#define SENSITIVITY_8G		4	/**	mg/LSB	*/
#define SENSITIVITY_16G		12	/**	mg/LSB	*/

/* Accelerometer Sensor Operating Mode */
#define LIS3DH_ACC_ENABLE	0x01
#define LIS3DH_ACC_DISABLE	0x00

#define	HIGH_RESOLUTION		0x08

#define	AXISDATA_REG		0x28
#define WHOAMI_LIS3DH_ACC	0x33	/*	Expected content for WAI */

/*	CONTROL REGISTERS	*/
#define WHO_AM_I		0x0F	/*	WhoAmI register		*/
#define	TEMP_CFG_REG		0x1F	/*	temper sens control reg	*/
/* ctrl 1: ODR3 ODR2 ODR ODR0 LPen Zenable Yenable Zenable */
#define	CTRL_REG1		0x20	/*	control reg 1		*/
#define	CTRL_REG2		0x21	/*	control reg 2		*/
#define	CTRL_REG3		0x22	/*	control reg 3		*/
#define	CTRL_REG4		0x23	/*	control reg 4		*/
#define	CTRL_REG5		0x24	/*	control reg 5		*/
#define	CTRL_REG6		0x25	/*	control reg 6		*/

#define	FIFO_CTRL_REG		0x2E	/*	FiFo control reg	*/
#define	FIFO_SRC_REG		0x2F	/*	FiFo source reg	*/

#define	INT_CFG1		0x30	/*	interrupt 1 config	*/
#define	INT_SRC1		0x31	/*	interrupt 1 source	*/
#define	INT_THS1		0x32	/*	interrupt 1 threshold	*/
#define	INT_DUR1		0x33	/*	interrupt 1 duration	*/


#define	TT_CFG			0x38	/*	tap config		*/
#define	TT_SRC			0x39	/*	tap source		*/
#define	TT_THS			0x3A	/*	tap threshold		*/
#define	TT_LIM			0x3B	/*	tap time limit		*/
#define	TT_TLAT			0x3C	/*	tap time latency	*/
#define	TT_TW			0x3D	/*	tap time window		*/
/*	end CONTROL REGISTRES	*/


#define ENABLE_HIGH_RESOLUTION	1

#define LIS3DH_ACC_PM_OFF		0x00
#define LIS3DH_ACC_ENABLE_ALL_AXES	0x07

/* FIFO REG BITS*/
#define PMODE_MASK			0x08
#define REG1_ODR_MASK			0XF0
#define REG1_ODR_SHIFT			4
#define FIFO_MODE_SHIFT			6
#define FIFO_MODE_MASK			0xC0
#define FIFO_WM_CFG_MASK		0x1F
#define FIFO_STATE_WATER_MARK		0x80
#define FIFO_STATE_FULL			0x40
#define FIFO_SRC_DATA_CNT_MASK		0x1F
#define FIFO_MAX_CNT			0x1F
#define FIFO_SRC_OVRN_MASK		0x40
#define REG5_FIFO_EN			0x40

/* interrupt configure REG BITS */
#define INT_GEN_I1_CLICK		0x80
#define INT_GEN_I1_GEN1			0x40
#define INT_GEN_I1_DRDY1		0x10
#define INT_GEN_I1_WTM			0x04
#define INT_GEN_I1_OVRN			0x02

#define IS_FIFO_FULL(status)		(status & FIFO_STATE_FULL)
#define IS_WATER_MARK_REACHED(status)	(status & FIFO_STATE_WATER_MARK)


#define ODR1		0x10  /* 1Hz output data rate */
#define ODR10		0x20  /* 10Hz output data rate */
#define ODR25		0x30  /* 25Hz output data rate */
#define ODR50		0x40  /* 50Hz output data rate */
#define ODR100		0x50  /* 100Hz output data rate */
#define ODR200		0x60  /* 200Hz output data rate */
#define ODR400		0x70  /* 400Hz output data rate */
#define ODR1250		0x90  /* 1250Hz output data rate */



#define	IA			0x40
#define	ZH			0x20
#define	ZL			0x10
#define	YH			0x08
#define	YL			0x04
#define	XH			0x02
#define	XL			0x01
/* */
/* CTRL REG BITS*/
#define	CTRL_REG3_I1_AOI1	0x40
#define	CTRL_REG6_I2_TAPEN	0x80
#define	CTRL_REG6_HLACTIVE	0x02
/* */
#define NO_MASK			0xFF
#define INT1_DURATION_MASK	0x7F
#define	INT1_THRESHOLD_MASK	0x7F
#define TAP_CFG_MASK		0x3F
#define	TAP_THS_MASK		0x7F
#define	TAP_TLIM_MASK		0x7F
#define	TAP_TLAT_MASK		NO_MASK
#define	TAP_TW_MASK		NO_MASK

/*soc irq set*/
#define CONFIG_IRQ_DRDY1	0x10
#define CONFIG_BLOCK_READ	0x80

/* TAP_SOURCE_REG BIT */
#define	DTAP			0x20
#define	STAP			0x10
#define	SIGNTAP			0x08
#define	ZTAP			0x04
#define	YTAP			0x02
#define	XTAZ			0x01


#define	FUZZ			0
#define	FLAT			0
#define	I2C_RETRY_DELAY		5
#define	I2C_RETRIES		5
#define	I2C_AUTO_INCREMENT	0x80

/* RESUME STATE INDICES */
#define	RES_CTRL_REG1		0
#define	RES_CTRL_REG2		1
#define	RES_CTRL_REG3		2
#define	RES_CTRL_REG4		3
#define	RES_CTRL_REG5		4
#define	RES_CTRL_REG6		5

#define	RES_INT_CFG1		6
#define	RES_INT_THS1		7
#define	RES_INT_DUR1		8

#define	RES_TT_CFG		9
#define	RES_TT_THS		10
#define	RES_TT_LIM		11
#define	RES_TT_TLAT		12
#define	RES_TT_TW		13

#define	RES_TEMP_CFG_REG	14
#define	RES_REFERENCE_REG	15
#define	RES_FIFO_CTRL_REG	16

#define	RESUME_ENTRIES		17
/* end RESUME STATE INDICES */

#define BATCH_MODE_NORMAL		0
#define BATCH_MODE_WAKE_UPON_FIFO_FULL	2

enum {
	LIS3DH_BYPASS_MODE = 0,
	LIS3DH_FIFO_MODE,
	LIS3DH_STREAM_MODE,
	LIS3DH_STREAM2FIFO_MODE,
	LIS3DH_FIFO_MODE_NUM
};

struct odr_config_table {
	unsigned int cutoff_ms;
	unsigned int mask;
};

struct odr_config_table lis3dh_acc_odr_table[] = {
		{    1, ODR1250 },
		{    3, ODR400  },  /* round to 3 */
		{    5, ODR200  },
		{   10, ODR100  },
		{   20, ODR50   },
		{   40, ODR25   },
		{  100, ODR10   },
		{ 1000, ODR1    },
};

struct lis3dh_acc_data {
	struct i2c_client *client;
	struct lis3dh_acc_platform_data *pdata;
	struct sensors_classdev cdev;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pin_default;
	struct pinctrl_state *pin_sleep;

	struct mutex lock;
	struct delayed_work input_work;

	struct input_dev *input_dev;

	int hw_initialized;
	/* hw_working=-1 means not tested yet */
	int hw_working;
	/* flag sensor is enabled (batch/poll) */
	atomic_t enabled;
	int on_before_suspend;

	u8 sensitivity;

	u8 resume_state[RESUME_ENTRIES];

	/* batch mode is configured */
	bool use_batch;
	unsigned int delay_ms;
	unsigned int batch_mode;
	unsigned int fifo_timeout_ms;
	unsigned int flush_count;
	int irq1;
	int irq2;

#ifdef DEBUG
	u8 reg_addr;
#endif
};

static struct sensors_classdev lis3dh_acc_cdev = {
	.name = "lis3dh-accel",
	.vendor = "STMicroelectronics",
	.version = 1,
	.handle = SENSORS_ACCELERATION_HANDLE,
	.type = SENSOR_TYPE_ACCELEROMETER,
	.max_range = "156.8",
	.resolution = "0.01",
	.sensor_power = "0.01",
	.min_delay = 10000,
	.max_delay = 6400,
	.delay_msec = 200,
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.enabled = 0,
	.max_latency = 0,
	.flags = 0,
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
	.sensors_set_latency = NULL,
	.sensors_flush = NULL,
};

struct sensor_regulator {
	struct regulator *vreg;
	const char *name;
	u32	min_uV;
	u32	max_uV;
};

struct sensor_regulator lis3dh_acc_vreg[] = {
	{NULL, "vdd", 1700000, 3600000},
	{NULL, "vddio", 1700000, 3600000},
};

static inline s64 lis3dh_acc_get_time_ns(void)
{
	struct timespec ts;

	ktime_get_ts(&ts);
	return timespec_to_ns(&ts);
}

static unsigned int lis3dh_acc_odr_to_interval(struct lis3dh_acc_data *acc,
				unsigned int odr)
{
	unsigned int odr_mask;
	unsigned int i;

	odr_mask = (odr << REG1_ODR_SHIFT) & REG1_ODR_MASK;

	for (i = ARRAY_SIZE(lis3dh_acc_odr_table) - 1; i > 0; i--) {
		if (lis3dh_acc_odr_table[i].mask == odr_mask)
			break;
	}

	return lis3dh_acc_odr_table[i].cutoff_ms;
}

static int lis3dh_acc_config_regulator(struct lis3dh_acc_data *acc, bool on)
{
	int rc = 0, i;
	int num_reg = sizeof(lis3dh_acc_vreg) / sizeof(struct sensor_regulator);

	if (on) {
		for (i = 0; i < num_reg; i++) {
			lis3dh_acc_vreg[i].vreg =
				regulator_get(&acc->client->dev,
				lis3dh_acc_vreg[i].name);
			if (IS_ERR(lis3dh_acc_vreg[i].vreg)) {
				rc = PTR_ERR(lis3dh_acc_vreg[i].vreg);
				dev_err(&acc->client->dev,
					"Regulator(%s) get failed rc=%d\n",
					lis3dh_acc_vreg[i].name, rc);
				lis3dh_acc_vreg[i].vreg = NULL;
				goto deinit_vregs;
			}

			if (regulator_count_voltages(
				lis3dh_acc_vreg[i].vreg) > 0) {
				rc = regulator_set_voltage(
					lis3dh_acc_vreg[i].vreg,
					lis3dh_acc_vreg[i].min_uV,
					lis3dh_acc_vreg[i].max_uV);
				if (rc) {
					dev_err(&acc->client->dev,
					"Regulator(%s)Set voltage failed rc=%d\n",
					lis3dh_acc_vreg[i].name, rc);
					regulator_put(lis3dh_acc_vreg[i].vreg);
					lis3dh_acc_vreg[i].vreg = NULL;
					goto deinit_vregs;
				}
			}
		}
		return rc;
	} else {
		i = num_reg;
		goto deinit_vregs;
	}

deinit_vregs:
	while (--i >= 0) {
		if (!IS_ERR_OR_NULL(lis3dh_acc_vreg[i].vreg)) {
			regulator_put(lis3dh_acc_vreg[i].vreg);
			lis3dh_acc_vreg[i].vreg = NULL;
		}
	}
	return rc;
}

static int lis3dh_acc_set_regulator(struct lis3dh_acc_data *acc, bool on)
{
	int rc = 0, i;
	int num_reg = sizeof(lis3dh_acc_vreg) / sizeof(struct sensor_regulator);

	if (on) {
		for (i = 0; i < num_reg; i++) {
			if (!IS_ERR_OR_NULL(lis3dh_acc_vreg[i].vreg)) {
				rc = regulator_enable(lis3dh_acc_vreg[i].vreg);
				if (rc) {
					dev_err(&acc->client->dev,
					"Enable regulator(%s) failed rc=%d\n",
					lis3dh_acc_vreg[i].name, rc);
					goto disable_regulator;
				}
			}
		}
		return rc;
	} else {
		for (i = (num_reg - 1); i >= 0; i--) {
			if (!IS_ERR_OR_NULL(lis3dh_acc_vreg[i].vreg)) {
				rc = regulator_disable(lis3dh_acc_vreg[i].vreg);
				if (rc)
					dev_err(&acc->client->dev,
					"Disable regulator(%s) failed rc=%d\n",
					lis3dh_acc_vreg[i].name, rc);
			}
		}
		return 0;
	}

disable_regulator:
	while (--i >= 0) {
		if (!IS_ERR_OR_NULL(lis3dh_acc_vreg[i].vreg))
			regulator_disable(lis3dh_acc_vreg[i].vreg);
	}
	return rc;
}

static int lis3dh_acc_i2c_read(struct lis3dh_acc_data *acc,
				u8 *buf, int len)
{
	int err;
	int tries = 0;

	struct i2c_msg	msgs[] = {
		{
			.addr = acc->client->addr,
			.flags = acc->client->flags & I2C_M_TEN,
			.len = 1,
			.buf = buf,
		},
		{
			.addr = acc->client->addr,
			.flags = (acc->client->flags & I2C_M_TEN) | I2C_M_RD,
			.len = len,
			.buf = buf,
		},
	};

	do {
		err = i2c_transfer(acc->client->adapter, msgs, 2);
		if (err != 2)
			msleep_interruptible(I2C_RETRY_DELAY);
	} while ((err != 2) && (++tries < I2C_RETRIES));

	if (err != 2) {
		dev_err(&acc->client->dev, "read transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

static int lis3dh_acc_i2c_write(struct lis3dh_acc_data *acc, u8 *buf, int len)
{
	int err;
	int tries = 0;

	struct i2c_msg msgs[] = {
		{
		 .addr = acc->client->addr,
			.flags = acc->client->flags & I2C_M_TEN,
		 .len = len + 1,
		 .buf = buf,
		 },
	};

	do {
		err = i2c_transfer(acc->client->adapter, msgs, 1);
		if (err != 1)
			msleep_interruptible(I2C_RETRY_DELAY);
	} while ((err != 1) && (++tries < I2C_RETRIES));

	if (err != 1) {
		dev_err(&acc->client->dev, "write transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

static int lis3dh_acc_hw_init(struct lis3dh_acc_data *acc)
{
	int err = -1;
	u8 buf[7];

	buf[0] = WHO_AM_I;
	err = lis3dh_acc_i2c_read(acc, buf, 1);
	if (err < 0) {
		dev_warn(&acc->client->dev,
		"Error reading WHO_AM_I: is device available/working?\n");
		goto err_firstread;
	} else
		acc->hw_working = 1;
	if (buf[0] != WHOAMI_LIS3DH_ACC) {
		dev_err(&acc->client->dev,
		"device unknown. Expected: 0x%x, Replies: 0x%x\n",
		WHOAMI_LIS3DH_ACC, buf[0]);
		err = -1; /* choose the right coded error */
		goto err_unknown_device;
	}

	buf[0] = CTRL_REG1;
	buf[1] = acc->resume_state[RES_CTRL_REG1];
	err = lis3dh_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		goto err_resume_state;

	buf[0] = TEMP_CFG_REG;
	buf[1] = acc->resume_state[RES_TEMP_CFG_REG];
	err = lis3dh_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		goto err_resume_state;

	buf[0] = FIFO_CTRL_REG;
	buf[1] = acc->resume_state[RES_FIFO_CTRL_REG];
	err = lis3dh_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		goto err_resume_state;

	buf[0] = (I2C_AUTO_INCREMENT | TT_THS);
	buf[1] = acc->resume_state[RES_TT_THS];
	buf[2] = acc->resume_state[RES_TT_LIM];
	buf[3] = acc->resume_state[RES_TT_TLAT];
	buf[4] = acc->resume_state[RES_TT_TW];
	err = lis3dh_acc_i2c_write(acc, buf, 4);
	if (err < 0)
		goto err_resume_state;
	buf[0] = TT_CFG;
	buf[1] = acc->resume_state[RES_TT_CFG];
	err = lis3dh_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		goto err_resume_state;

	buf[0] = (I2C_AUTO_INCREMENT | INT_THS1);
	buf[1] = acc->resume_state[RES_INT_THS1];
	buf[2] = acc->resume_state[RES_INT_DUR1];
	err = lis3dh_acc_i2c_write(acc, buf, 2);
	if (err < 0)
		goto err_resume_state;
	buf[0] = INT_CFG1;
	buf[1] = acc->resume_state[RES_INT_CFG1];
	err = lis3dh_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		goto err_resume_state;


	buf[0] = (I2C_AUTO_INCREMENT | CTRL_REG2);
	buf[1] = acc->resume_state[RES_CTRL_REG2];
	buf[2] = acc->resume_state[RES_CTRL_REG3];
	buf[3] = acc->resume_state[RES_CTRL_REG4];
	buf[4] = acc->resume_state[RES_CTRL_REG5];
	buf[5] = acc->resume_state[RES_CTRL_REG6];
	err = lis3dh_acc_i2c_write(acc, buf, 5);
	if (err < 0)
		goto err_resume_state;

	acc->hw_initialized = 1;
	return 0;

err_firstread:
	acc->hw_working = 0;
err_unknown_device:
err_resume_state:
	acc->hw_initialized = 0;
	dev_err(&acc->client->dev, "hw init error 0x%x,0x%x: %d\n", buf[0],
			buf[1], err);
	return err;
}

static void lis3dh_acc_device_power_off(struct lis3dh_acc_data *acc)
{
	int err;
	u8 buf[2] = { CTRL_REG1, LIS3DH_ACC_PM_OFF };

	err = lis3dh_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		dev_err(&acc->client->dev, "soft power off failed: %d\n", err);

	lis3dh_acc_set_regulator(acc, false);

	if (acc->hw_initialized) {
		if (gpio_is_valid(acc->pdata->gpio_int1)
				&& acc->pdata->enable_int)
			disable_irq_nosync(acc->irq1);

		if (gpio_is_valid(acc->pdata->gpio_int2)
				&& acc->pdata->enable_int)
			disable_irq_nosync(acc->irq2);

		acc->hw_initialized = 0;
	}
}

static int lis3dh_acc_device_power_on(struct lis3dh_acc_data *acc)
{
	int err = -1;

	err = lis3dh_acc_set_regulator(acc, true);
	if (err < 0) {
		dev_err(&acc->client->dev,
				"power_on failed: %d\n", err);
		return err;
	}

	msleep(20);

	if (!acc->hw_initialized) {
		err = lis3dh_acc_hw_init(acc);
		if (acc->hw_working == 1 && err < 0) {
			lis3dh_acc_set_regulator(acc, false);
			return err;
		}
	}

	if (acc->hw_initialized) {
		if (gpio_is_valid(acc->pdata->gpio_int1)
			&& acc->pdata->enable_int)
			enable_irq(acc->irq1);
		if (gpio_is_valid(acc->pdata->gpio_int2)
			&& acc->pdata->enable_int)
			enable_irq(acc->irq2);
	}
	return 0;
}

int lis3dh_acc_update_g_range(struct lis3dh_acc_data *acc, u8 new_g_range)
{
	int err = -1;

	u8 sensitivity;
	u8 buf[2];
	u8 updated_val;
	u8 init_val;
	u8 new_val;
	u8 mask = LIS3DH_ACC_FS_MASK | HIGH_RESOLUTION;

	switch (new_g_range) {
	case LIS3DH_ACC_G_2G:

		sensitivity = SENSITIVITY_2G;
		break;
	case LIS3DH_ACC_G_4G:

		sensitivity = SENSITIVITY_4G;
		break;
	case LIS3DH_ACC_G_8G:

		sensitivity = SENSITIVITY_8G;
		break;
	case LIS3DH_ACC_G_16G:

		sensitivity = SENSITIVITY_16G;
		break;
	default:
		dev_err(&acc->client->dev, "invalid g range requested: %u\n",
				new_g_range);
		return -EINVAL;
	}

	if (atomic_read(&acc->enabled)) {
		/* Updates configuration register 4,
		* which contains g range setting */
		buf[0] = CTRL_REG4;
		err = lis3dh_acc_i2c_read(acc, buf, 1);
		if (err < 0)
			goto error;
		init_val = buf[0];
		acc->resume_state[RES_CTRL_REG4] = init_val;
		new_val = new_g_range | HIGH_RESOLUTION;
		updated_val = ((mask & new_val) | ((~mask) & init_val));
		buf[1] = updated_val;
		buf[0] = CTRL_REG4;
		err = lis3dh_acc_i2c_write(acc, buf, 1);
		if (err < 0)
			goto error;
		acc->resume_state[RES_CTRL_REG4] = updated_val;
		acc->sensitivity = sensitivity;
	}


	return err;
error:
	dev_err(&acc->client->dev, "update g range failed 0x%x,0x%x: %d\n",
			buf[0], buf[1], err);

	return err;
}

static unsigned int lis3dh_acc_delay_to_odr(struct lis3dh_acc_data *acc,
				unsigned int delay_ms)
{
	unsigned int i;
	/*
	 * Following, looks for the longest possible odr interval scrolling the
	 * odr_table vector from the end (shortest interval) backward (longest
	 * interval), to support the polling interval requested by the system.
	 * It must be the longest interval lower then the poll interval.
	 * polling interval should not less then 1.
	 */

	for (i = ARRAY_SIZE(lis3dh_acc_odr_table) - 1; i > 0; i--) {
		if (lis3dh_acc_odr_table[i].cutoff_ms <= delay_ms)
			break;
	}
	return lis3dh_acc_odr_table[i].mask;
}

static int lis3dh_acc_update_odr(struct lis3dh_acc_data *acc,
			unsigned int delay_ms)
{
	int err = -1;
	u8 config[2];

	config[1] = lis3dh_acc_delay_to_odr(acc, delay_ms);
	config[1] |= LIS3DH_ACC_ENABLE_ALL_AXES;

	config[0] = CTRL_REG1;
	err = lis3dh_acc_i2c_write(acc, config, 1);
	if (err < 0)
		goto error;
	acc->resume_state[RES_CTRL_REG1] = config[1];

	dev_dbg(&acc->client->dev,
		"update odr success delay=%u code=%#x\n", delay_ms, config[1]);
	return 0;

error:
	dev_err(&acc->client->dev, "update odr failed 0x%x,0x%x: %d\n",
			config[0], config[1], err);

	return err;
}

static int lis3dh_acc_calc_watermark(struct lis3dh_acc_data *acc,
				unsigned int timeout_ms)
{
	unsigned int num;
	unsigned int odr, delay_ms;

	/* Get actual odr and calculate watermark */
	odr = acc->resume_state[RES_CTRL_REG1] >> 4;
	delay_ms = lis3dh_acc_odr_to_interval(acc, odr);
	/* Ensure watermark number always > 1 and not divide by zero */
	if ((timeout_ms < delay_ms) || (delay_ms == 0)) {
		dev_err(&acc->client->dev,
			"Timeout(%u) is less than polling interval(%u)\n",
			timeout_ms, delay_ms);
		return -EINVAL;
	}

	num = timeout_ms / delay_ms;
	dev_dbg(&acc->client->dev,
		"timeout_ms=%d, delay=%d sample_num =%d\n",
		timeout_ms, delay_ms, num);
	return num;
}

static int lis3dh_acc_get_fifo_lvl(struct lis3dh_acc_data *acc)
{
	int error = 0;
	unsigned char buf[2];
	unsigned int fifo_lvl;

	buf[0] = FIFO_SRC_REG;
	error = lis3dh_acc_i2c_read(acc, buf, 1);
	if (error < 0) {
		dev_err(&acc->client->dev, "read fifo level error\n");
		return error;
	}
	fifo_lvl = buf[0] & FIFO_SRC_DATA_CNT_MASK;
	if ((fifo_lvl == FIFO_MAX_CNT)
		&& (buf[0] | FIFO_SRC_OVRN_MASK))
		fifo_lvl = FIFO_MAX_CNT + 1;

	return fifo_lvl;
}

static int lis3dh_acc_set_wtm_int(struct lis3dh_acc_data *acc, bool enable)
{
	unsigned char buf[2];
	int error = 0;

	buf[0] = CTRL_REG3;
	error = lis3dh_acc_i2c_read(acc, buf, 1);
	if (error < 0) {
		dev_err(&acc->client->dev, "read fifo control reg error\n");
		return error;
	}
	if (enable)
		buf[1] = buf[0] | INT_GEN_I1_WTM;
	else
		buf[1] = buf[0] & (~INT_GEN_I1_WTM);

	buf[0] = CTRL_REG3;
	error = lis3dh_acc_i2c_write(acc, buf, 1);
	if (error < 0) {
		dev_err(&acc->client->dev, "write fifo control reg error\n");
		return error;
	}
	acc->resume_state[RES_CTRL_REG3] = buf[1];
	return 0;
}

/*
 * Turn ON/OFF FIFO by setting the FIFO_En bit,
 * FIFO must be enabled before activate FIFO mode.
 */
static int lis3dh_enable_fifo(struct i2c_client *client, bool enable)
{
	unsigned char buf[2];
	int error;
	struct lis3dh_acc_data *acc = i2c_get_clientdata(client);

	buf[0] = CTRL_REG5;
	error = lis3dh_acc_i2c_read(acc, buf, 1);
	if (error < 0) {
		dev_err(&client->dev, "read fifo enable reg error\n");
		return error;
	}
	if (enable)
		buf[1] = buf[0] | REG5_FIFO_EN;
	else
		buf[1] = buf[0] & (~REG5_FIFO_EN);
	buf[0] = CTRL_REG5;
	error = lis3dh_acc_i2c_write(acc, buf, 1);
	if (error < 0) {
		dev_err(&client->dev, "write fifo enable reg error\n");
		return error;
	}
	acc->resume_state[RES_CTRL_REG5] = buf[1];
	dev_dbg(&client->dev, "lis3dh REG5 = %#x\n", buf[1]);
	buf[0] = CTRL_REG3;
	error = lis3dh_acc_i2c_read(acc, buf, 1);
	if (error < 0) {
		dev_err(&client->dev, "read fifo control reg error\n");
		return error;
	}
	if (enable) {
		buf[0] = buf[0] & (~INT_GEN_I1_DRDY1);
		buf[1] = buf[0] | INT_GEN_I1_OVRN;
	} else {
		/* disable I1_WTM and I1_OVERRUN */
		buf[1] = buf[0] & ~(INT_GEN_I1_OVRN | INT_GEN_I1_WTM);
		/* REenable I1_DRDY1 */
		if (acc->pdata->enable_int)
			buf[1] = buf[1] | INT_GEN_I1_DRDY1;
	}
	buf[0] = CTRL_REG3;
	error = lis3dh_acc_i2c_write(acc, buf, 1);
	if (error < 0) {
		dev_err(&client->dev, "write enable fifo reg error\n");
		return error;
	}
	acc->resume_state[RES_CTRL_REG3] = buf[1];
	dev_dbg(&client->dev,
		"EN FIFO; lis3dh REG3=%#x, err=%d\n", buf[1], error);
	return error;
}

/*
 * Activate FIFO mode by setting FIFO mode bits.
 */
static int lis3dh_set_fifo_mode(struct i2c_client *client,
				unsigned char fifo_mode)
{
	unsigned char buf[2];
	int error;
	struct lis3dh_acc_data *acc = i2c_get_clientdata(client);

	buf[0] = FIFO_CTRL_REG;
	error = lis3dh_acc_i2c_read(acc, buf, 1);
	if (error < 0) {
		dev_err(&client->dev, "read fifo reg error\n");
		return error;
	}
	buf[1] = buf[0] & (~FIFO_MODE_MASK);
	buf[1] |= (unsigned char)(fifo_mode << FIFO_MODE_SHIFT);

	buf[0] = FIFO_CTRL_REG;
	error = lis3dh_acc_i2c_write(acc, buf, 1);
	if (error < 0) {
		dev_err(&client->dev, "write fifo mode error\n");
		return error;
	}
	acc->resume_state[RES_FIFO_CTRL_REG] = buf[1];
	dev_dbg(&client->dev,
		"lis3dh_set_fifo_mode: lis3dh fifo_reg = %#x\n", buf[1]);
	return error;
}

static int lis3dh_interrupt_status(struct lis3dh_acc_data *acc, char *status)
{
	int error;
	char buf;

	buf = FIFO_SRC_REG;
	error =  lis3dh_acc_i2c_read(acc, &buf, 1);
	if (error < 0) {
		dev_err(&acc->client->dev, "read interrupt status error\n");
		return error;
	} else {
		*status = buf;
		dev_dbg(&acc->client->dev, "FIFO_SRC = %#x\n", (int) buf);
	}
	return 0;
}

static int lis3dh_enable_DRDY_int(struct lis3dh_acc_data *acc)
{
	int error;
	char buf[2];

	if (acc->pdata->enable_int) {
		buf[0] = CTRL_REG3;
		error = lis3dh_acc_i2c_read(acc, buf, 1);
		if (error < 0) {
			dev_err(&acc->client->dev,
				"read fifo control reg error\n");
			return error;
		}

		buf[1] = buf[0] | INT_GEN_I1_DRDY1;
		buf[0] = CTRL_REG3;
		error = lis3dh_acc_i2c_write(acc, buf, 1);
		if (error < 0) {
			dev_err(&acc->client->dev,
				"write enable fifo reg error\n");
			return error;
		}
		acc->resume_state[RES_CTRL_REG3] = buf[1];
	} else {
		dev_info(&acc->client->dev, "Interrupt not enabled\n");
	}
	return 0;
}

static int lis3dh_acc_set_waterwark(struct lis3dh_acc_data *acc,
				int watermark)
{
	unsigned char buf[2];
	int error = 0;

	if (watermark < LIS3DH_FIFO_SIZE - 1) {
		buf[0] = FIFO_CTRL_REG;
		error = lis3dh_acc_i2c_read(acc, buf, 1);
		if (error < 0) {
			dev_err(&acc->client->dev, "read fifo reg error\n");
			return error;
		}
		buf[1] = buf[0] & (~FIFO_WM_CFG_MASK);
		buf[1] = buf[1] | (watermark & FIFO_WM_CFG_MASK);
		buf[0] = FIFO_CTRL_REG;
		error = lis3dh_acc_i2c_write(acc, buf, 1);
		if (error < 0) {
			dev_err(&acc->client->dev, "write fifo mode error\n");
			return error;
		}
		acc->resume_state[RES_FIFO_CTRL_REG] = buf[1];
		error = lis3dh_acc_set_wtm_int(acc, true);
	} else {
		error = lis3dh_acc_set_wtm_int(acc, false);

		dev_dbg(&acc->client->dev,
			"Watermark (%d) >= FIFO level (%d), disable WTM int!\n",
			watermark, LIS3DH_FIFO_SIZE - 1);
	}
	return error;
}

static int lis3dh_acc_enable_batch(struct lis3dh_acc_data *acc, bool en)
{
	struct i2c_client *client = acc->client;
	int watermark;
	int err;

	mutex_lock(&acc->lock);
	if (en) {
		watermark = lis3dh_acc_calc_watermark(acc,
					acc->fifo_timeout_ms);
		if (watermark <= 0) {
			dev_err(&acc->client->dev,
				"enable batch: cannot calculate watermark, ret=%d\n",
				watermark);
			err = -EINVAL;
			goto exit;
		}
		err = lis3dh_acc_set_waterwark(acc, watermark);
		if (err < 0) {
			dev_err(&acc->client->dev,
				"enable batch: cannot set watermark, ret=%d\n",
				err);
			goto exit;
		}

		err = lis3dh_enable_fifo(client, true);
		if (err < 0) {
			dev_err(&acc->client->dev,
				"enable batch: cannot enable FIFO\n");
			goto exit;
		}
		err = lis3dh_set_fifo_mode(client, LIS3DH_FIFO_MODE);
		if (err < 0) {
			dev_err(&acc->client->dev,
				"enable batch: cannot set FIFO mode\n");
			goto exit;
		}
	} else {
		err = lis3dh_set_fifo_mode(client, LIS3DH_BYPASS_MODE);
		if (err < 0) {
			dev_err(&acc->client->dev,
				"enable batch: cannot set FIFO mode\n");
			goto exit;
		}
		err = lis3dh_enable_fifo(client, false);
		if (err < 0) {
			dev_err(&acc->client->dev,
				"enable batch: cannot enable FIFO\n");
			goto exit;
		}
	}

exit:
	mutex_unlock(&acc->lock);
	return err;
}

static int lis3dh_acc_register_write(struct lis3dh_acc_data *acc, u8 *buf,
		u8 reg_address, u8 new_value)
{
	int err = -1;

	/* Sets configuration register at reg_address
	 *  NOTE: this is a straight overwrite  */
		buf[0] = reg_address;
		buf[1] = new_value;
		err = lis3dh_acc_i2c_write(acc, buf, 1);
		if (err < 0)
			return err;
	return err;
}

static int lis3dh_acc_get_acceleration_data(struct lis3dh_acc_data *acc,
		int *xyz)
{
	int err = -1;
	/* Data bytes from hardware xL, xH, yL, yH, zL, zH */
	u8 acc_data[6];
	/* x,y,z hardware data */
	s16 hw_d[3] = { 0 };

	acc_data[0] = (I2C_AUTO_INCREMENT | AXISDATA_REG);
	err = lis3dh_acc_i2c_read(acc, acc_data, 6);
	if (err < 0)
		return err;

	hw_d[0] = (((s16) ((acc_data[1] << 8) | acc_data[0])) >> 4);
	hw_d[1] = (((s16) ((acc_data[3] << 8) | acc_data[2])) >> 4);
	hw_d[2] = (((s16) ((acc_data[5] << 8) | acc_data[4])) >> 4);

	hw_d[0] = hw_d[0] * acc->sensitivity;
	hw_d[1] = hw_d[1] * acc->sensitivity;
	hw_d[2] = hw_d[2] * acc->sensitivity;


	xyz[0] = ((acc->pdata->negate_x) ? (-hw_d[acc->pdata->axis_map_x])
		   : (hw_d[acc->pdata->axis_map_x]));
	xyz[1] = ((acc->pdata->negate_y) ? (-hw_d[acc->pdata->axis_map_y])
		   : (hw_d[acc->pdata->axis_map_y]));
	xyz[2] = ((acc->pdata->negate_z) ? (-hw_d[acc->pdata->axis_map_z])
		   : (hw_d[acc->pdata->axis_map_z]));

	dev_dbg(&acc->client->dev, "%s read x=%d, y=%d, z=%d\n",
				LIS3DH_ACC_DEV_NAME, xyz[0], xyz[1], xyz[2]);
	return err;
}

static void lis3dh_acc_report_values(struct lis3dh_acc_data *acc,
					int *xyz)
{
	input_report_abs(acc->input_dev, ABS_X, xyz[0]);
	input_report_abs(acc->input_dev, ABS_Y, xyz[1]);
	input_report_abs(acc->input_dev, ABS_Z, xyz[2]);
	input_sync(acc->input_dev);
}

static int lis3dh_acc_enable(struct lis3dh_acc_data *acc)
{
	int err;

	dev_dbg(&acc->client->dev, "enable acc: state =%d\n",
			atomic_read(&acc->enabled));
	if (!atomic_cmpxchg(&acc->enabled, 0, 1)) {
		if (pinctrl_select_state(acc->pinctrl, acc->pin_default))
			dev_err(&acc->client->dev,
				"can't select pinctrl default state\n");

		err = lis3dh_acc_device_power_on(acc);
		if (err < 0) {
			atomic_set(&acc->enabled, 0);
			return err;
		}
		err = lis3dh_acc_update_odr(acc, acc->delay_ms);
		if (err) {
			atomic_set(&acc->enabled, 0);
			dev_err(&acc->client->dev, "update_odr err=%d\n", err);
			return err;
		}
		if (!acc->pdata->enable_int && !acc->use_batch) {
			schedule_delayed_work(&acc->input_work,
				msecs_to_jiffies(acc->delay_ms));
			return 0;
		}
		if (acc->use_batch) {
			err = lis3dh_acc_enable_batch(acc, true);
			if (err < 0) {
				atomic_set(&acc->enabled, 0);
				dev_err(&acc->client->dev,
					"enable batch err=%d\n", err);
				return err;
			}
		} else {
			err = lis3dh_enable_DRDY_int(acc);
			if (err) {
				atomic_set(&acc->enabled, 0);
				dev_err(&acc->client->dev,
					"enable interrupt err=%d\n", err);
				return err;
			}
		}
	}

	return 0;
}

static int lis3dh_acc_disable(struct lis3dh_acc_data *acc)
{
	int err = 0;

	dev_dbg(&acc->client->dev, "disable state=%d\n",
		atomic_read(&acc->enabled));
	if (atomic_cmpxchg(&acc->enabled, 1, 0)) {
		if (acc->use_batch) {
			err = lis3dh_acc_enable_batch(acc, false);
			if (err < 0) {
				atomic_set(&acc->enabled, 1);
				return err;
			}
		}
		if (!acc->pdata->enable_int && !acc->use_batch)
			cancel_delayed_work_sync(&acc->input_work);
		lis3dh_acc_device_power_off(acc);
		if (pinctrl_select_state(acc->pinctrl, acc->pin_sleep))
			dev_err(&acc->client->dev,
				"can't select pinctrl sleep state\n");
	}

	return 0;
}

static irqreturn_t lis3dh_acc_isr1(int irq, void *dev)
{
	int err;
	char status;
	int i;
	int fifo_cnt;
	int xyz[3] = { 0 };
	u64 timestamp = 0;
	u32 time_h, time_l, time_ms;
	struct lis3dh_acc_data *acc = dev;

	err = lis3dh_interrupt_status(acc, &status);
	if (err) {
		dev_err(&acc->client->dev, "read status error\n");
		goto exit;
	}
	if (IS_FIFO_FULL(status) || IS_WATER_MARK_REACHED(status)) {
		timestamp = lis3dh_acc_get_time_ns();
		time_ms = lis3dh_acc_odr_to_interval(acc,
			(acc->resume_state[RES_CTRL_REG1] >> 4));
		fifo_cnt = lis3dh_acc_get_fifo_lvl(acc);
		dev_dbg(&acc->client->dev, "TS: base=%lld, interval=%d fifo_cnt=%d\n",
			timestamp, time_ms, fifo_cnt);
		timestamp = timestamp -
			((u64)time_ms * LIS3DH_TIME_MS_TO_NS * fifo_cnt);

		for (i = 0; i < fifo_cnt; i++) {
			err = lis3dh_acc_get_acceleration_data(acc, xyz);
			if (err < 0) {
				dev_err(&acc->client->dev,
					"get_acceleration_data failed\n");
				goto exit;
			} else {
				timestamp = timestamp +
				((u64)time_ms * LIS3DH_TIME_MS_TO_NS);
				time_h = (u32)((timestamp >> 32) & 0xFFFFFFFF);
				time_l = (u32)(timestamp & 0xFFFFFFFF);

				input_report_abs(acc->input_dev, ABS_RX,
						time_h);
				input_report_abs(acc->input_dev, ABS_RY,
						time_l);
				lis3dh_acc_report_values(acc, xyz);
			}
		}

		err = lis3dh_set_fifo_mode(acc->client, LIS3DH_BYPASS_MODE);
		if (err < 0) {
			dev_err(&acc->client->dev,
					"set fifo mode to bypass failed\n");
			goto exit;
		}

		err = lis3dh_set_fifo_mode(acc->client, LIS3DH_FIFO_MODE);
		if (err < 0) {
			dev_err(&acc->client->dev,
					"set fifo mode to bypass failed\n");
			goto exit;
		}
	} else {
		err = lis3dh_acc_get_acceleration_data(acc, xyz);
		if (err < 0) {
			dev_err(&acc->client->dev,
					"get_acceleration_data failed\n");
			goto exit;
		} else {
			lis3dh_acc_report_values(acc, xyz);
		}
	}
exit:
	return IRQ_HANDLED;
}

static irqreturn_t lis3dh_acc_isr2(int irq, void *dev)
{
	struct lis3dh_acc_data *acc = dev;
	int err;
	int xyz[3] = { 0 };
	err = lis3dh_acc_get_acceleration_data(acc, xyz);
	if (err < 0)
		dev_err(&acc->client->dev, "get_acceleration_data failed\n");
	else
		lis3dh_acc_report_values(acc, xyz);

	return IRQ_HANDLED;
}

static ssize_t read_single_reg(struct device *dev, char *buf, u8 reg)
{
	ssize_t ret;
	struct lis3dh_acc_data *acc = dev_get_drvdata(dev);
	int err;

	u8 data = reg;
	err = lis3dh_acc_i2c_read(acc, &data, 1);
	if (err < 0)
		return err;
	ret = snprintf(buf, 4, "0x%02x\n", data);
	return ret;

}

static int write_reg(struct device *dev, const char *buf, u8 reg,
		u8 mask, int resumeIndex)
{
	int err = -1;
	struct lis3dh_acc_data *acc = dev_get_drvdata(dev);
	u8 x[2];
	u8 new_val;
	unsigned long val;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;

	new_val = ((u8) val & mask);
	x[0] = reg;
	x[1] = new_val;
	err = lis3dh_acc_register_write(acc, x, reg, new_val);
	if (err < 0)
		return err;
	acc->resume_state[resumeIndex] = new_val;
	return err;
}

static ssize_t attr_get_polling_rate(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	int val;
	struct lis3dh_acc_data *acc = dev_get_drvdata(dev);
	mutex_lock(&acc->lock);
	val = acc->delay_ms;
	mutex_unlock(&acc->lock);
	return snprintf(buf, 8, "%d\n", val);
}

static ssize_t attr_set_polling_rate(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t size)
{
	struct lis3dh_acc_data *acc = dev_get_drvdata(dev);
	unsigned long interval_ms;

	if (kstrtoul(buf, 10, &interval_ms))
		return -EINVAL;
	if (!interval_ms)
		return -EINVAL;
	mutex_lock(&acc->lock);
	acc->delay_ms = interval_ms;
	lis3dh_acc_update_odr(acc, interval_ms);
	mutex_unlock(&acc->lock);
	return size;
}

static ssize_t attr_get_range(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	char val;
	struct lis3dh_acc_data *acc = dev_get_drvdata(dev);
	char range = 2;

	mutex_lock(&acc->lock);
	val = acc->pdata->g_range;
	switch (val) {
	case LIS3DH_ACC_G_2G:
		range = 2;
		break;
	case LIS3DH_ACC_G_4G:
		range = 4;
		break;
	case LIS3DH_ACC_G_8G:
		range = 8;
		break;
	case LIS3DH_ACC_G_16G:
		range = 16;
		break;
	}
	mutex_unlock(&acc->lock);
	return snprintf(buf, 4, "%d\n", range);
}

static ssize_t attr_set_range(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t size)
{
	struct lis3dh_acc_data *acc = dev_get_drvdata(dev);
	unsigned long val;
	if (kstrtoul(buf, 10, &val))
		return -EINVAL;
	mutex_lock(&acc->lock);
	acc->pdata->g_range = val;
	lis3dh_acc_update_g_range(acc, val);
	mutex_unlock(&acc->lock);
	return size;
}

static ssize_t attr_get_enable(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct lis3dh_acc_data *acc = dev_get_drvdata(dev);
	int val = atomic_read(&acc->enabled);
	return snprintf(buf, sizeof(val) + 2, "%d\n", val);
}

static ssize_t attr_set_enable(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t size)
{
	struct lis3dh_acc_data *acc = dev_get_drvdata(dev);
	unsigned long val;

	if (kstrtoul(buf, 10, &val))
		return -EINVAL;

	if (val)
		lis3dh_acc_enable(acc);
	else
		lis3dh_acc_disable(acc);

	return size;
}

static ssize_t attr_set_intconfig1(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev, buf, INT_CFG1, NO_MASK, RES_INT_CFG1);
}

static ssize_t attr_get_intconfig1(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev, buf, INT_CFG1);
}

static ssize_t attr_set_duration1(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev, buf, INT_DUR1, INT1_DURATION_MASK, RES_INT_DUR1);
}

static ssize_t attr_get_duration1(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev, buf, INT_DUR1);
}

static ssize_t attr_set_thresh1(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev, buf, INT_THS1, INT1_THRESHOLD_MASK, RES_INT_THS1);
}

static ssize_t attr_get_thresh1(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev, buf, INT_THS1);
}

static ssize_t attr_get_source1(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return read_single_reg(dev, buf, INT_SRC1);
}

static ssize_t attr_set_click_cfg(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev, buf, TT_CFG, TAP_CFG_MASK, RES_TT_CFG);
}

static ssize_t attr_get_click_cfg(struct device *dev,
		struct device_attribute *attr,	char *buf)
{

	return read_single_reg(dev, buf, TT_CFG);
}

static ssize_t attr_get_click_source(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev, buf, TT_SRC);
}

static ssize_t attr_set_click_ths(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev, buf, TT_THS, TAP_THS_MASK, RES_TT_THS);
}

static ssize_t attr_get_click_ths(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev, buf, TT_THS);
}

static ssize_t attr_set_click_tlim(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev, buf, TT_LIM, TAP_TLIM_MASK, RES_TT_LIM);
}

static ssize_t attr_get_click_tlim(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev, buf, TT_LIM);
}

static ssize_t attr_set_click_tlat(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev, buf, TT_TLAT, TAP_TLAT_MASK, RES_TT_TLAT);
}

static ssize_t attr_get_click_tlat(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev, buf, TT_TLAT);
}

static ssize_t attr_set_click_tw(struct device *dev,
		struct device_attribute *attr,	const char *buf, size_t size)
{
	return write_reg(dev, buf, TT_TLAT, TAP_TW_MASK, RES_TT_TLAT);
}

static ssize_t attr_get_click_tw(struct device *dev,
		struct device_attribute *attr,	char *buf)
{
	return read_single_reg(dev, buf, TT_TLAT);
}


#ifdef DEBUG
/* PAY ATTENTION: These DEBUG funtions don't manage resume_state */
static ssize_t attr_reg_set(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	int rc;
	struct lis3dh_acc_data *acc = dev_get_drvdata(dev);
	u8 x[2];
	unsigned long val;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	mutex_lock(&acc->lock);
	x[0] = acc->reg_addr;
	mutex_unlock(&acc->lock);
	x[1] = val;
	rc = lis3dh_acc_i2c_write(acc, x, 1);
	/*TODO: error need to be managed */
	return size;
}

static ssize_t attr_reg_get(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	ssize_t ret;
	struct lis3dh_acc_data *acc = dev_get_drvdata(dev);
	int rc;
	u8 data;

	mutex_lock(&acc->lock);
	data = acc->reg_addr;
	mutex_unlock(&acc->lock);
	rc = lis3dh_acc_i2c_read(acc, &data, 1);
	/* TODO: error need to be managed */
	ret = snprintf(buf, 8, "0x%02x\n", data);
	return ret;
}

static ssize_t attr_addr_set(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct lis3dh_acc_data *acc = dev_get_drvdata(dev);
	unsigned long val;
	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	mutex_lock(&acc->lock);
	acc->reg_addr = val;
	mutex_unlock(&acc->lock);
	return size;
}
#endif

static struct device_attribute attributes[] = {

	__ATTR(poll_delay, 0664, attr_get_polling_rate,
			attr_set_polling_rate),
	__ATTR(range, 0664, attr_get_range, attr_set_range),
	__ATTR(enable, 0664, attr_get_enable, attr_set_enable),
	__ATTR(int1_config, 0664, attr_get_intconfig1, attr_set_intconfig1),
	__ATTR(int1_duration, 0664, attr_get_duration1, attr_set_duration1),
	__ATTR(int1_threshold, 0664, attr_get_thresh1, attr_set_thresh1),
	__ATTR(int1_source, 0444, attr_get_source1, NULL),
	__ATTR(click_config, 0664, attr_get_click_cfg, attr_set_click_cfg),
	__ATTR(click_source, 0444, attr_get_click_source, NULL),
	__ATTR(click_threshold, 0664, attr_get_click_ths, attr_set_click_ths),
	__ATTR(click_timelimit, 0664, attr_get_click_tlim,
			attr_set_click_tlim),
	__ATTR(click_timelatency, 0664, attr_get_click_tlat,
							attr_set_click_tlat),
	__ATTR(click_timewindow, 0664, attr_get_click_tw, attr_set_click_tw),

#ifdef DEBUG
	__ATTR(reg_value, 0664, attr_reg_get, attr_reg_set),
	__ATTR(reg_addr, 0220, NULL, attr_addr_set),
#endif
};

static int create_sysfs_interfaces(struct device *dev)
{
	int i;
	int err;
	for (i = 0; i < ARRAY_SIZE(attributes); i++) {
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

static int remove_sysfs_interfaces(struct device *dev)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(attributes); i++)
		device_remove_file(dev, attributes + i);
	return 0;
}

static int lis3dh_acc_flush(struct sensors_classdev *sensors_cdev)
{
	struct lis3dh_acc_data *acc = container_of(sensors_cdev,
			struct lis3dh_acc_data, cdev);
	s64 timestamp;
	int err;
	int fifo_cnt;
	int i;
	u32 time_h, time_l, time_ms;
	int xyz[3] = {0};

	timestamp = lis3dh_acc_get_time_ns();
	time_ms = lis3dh_acc_odr_to_interval(acc,
			(acc->resume_state[RES_CTRL_REG1] >> 4));
	fifo_cnt = lis3dh_acc_get_fifo_lvl(acc);
	dev_dbg(&acc->client->dev, "TS: base=%lld, interval=%d fifo_cnt=%d\n",
			timestamp, time_ms, fifo_cnt);
	timestamp = timestamp -
		((s64)time_ms * LIS3DH_TIME_MS_TO_NS * fifo_cnt);

	for (i = 0; i < fifo_cnt; i++) {
		err = lis3dh_acc_get_acceleration_data(acc, xyz);
		if (err < 0) {
			dev_err(&acc->client->dev,
					"get_acceleration_data failed\n");
			goto exit;
		} else {
			timestamp = timestamp +
				(time_ms * LIS3DH_TIME_MS_TO_NS);
			time_h = (u32)(((u64)timestamp >> 32) & 0xFFFFFFFF);
			time_l = (u32)(timestamp & 0xFFFFFFFF);

			input_report_abs(acc->input_dev, ABS_RX,
					time_h);
			input_report_abs(acc->input_dev, ABS_RY,
					time_l);
			lis3dh_acc_report_values(acc, xyz);
		}
	}

	input_event(acc->input_dev, EV_SYN, SYN_CONFIG, acc->flush_count++);
	input_sync(acc->input_dev);

	return 0;
exit:
	return err;
}

static int lis3dh_acc_poll_delay_set(struct sensors_classdev *sensors_cdev,
	unsigned int delay_msec)
{
	struct lis3dh_acc_data *acc = container_of(sensors_cdev,
		struct lis3dh_acc_data, cdev);
	int err;
	int watermark;

	dev_dbg(&acc->client->dev, "set poll delay =%d\n", delay_msec);
	mutex_lock(&acc->lock);
	acc->delay_ms = delay_msec;
	/*
	 * Device may not power on,
	 * only set register when device is enabled.
	 */
	if (atomic_read(&acc->enabled)) {
		err = lis3dh_acc_update_odr(acc, delay_msec);
		if (err < 0) {
			dev_err(&acc->client->dev, "Cannot update ODR\n");
			err = -EBUSY;
			goto exit;
		}
		if (acc->use_batch) {
			watermark = lis3dh_acc_calc_watermark(acc,
					acc->fifo_timeout_ms);
			if (watermark <= 0) {
				dev_err(&acc->client->dev,
					"Cannot calculate watermark, ret=%d\n",
					watermark);
				err = -EINVAL;
				goto exit;
			} else {
				err = lis3dh_acc_set_waterwark(acc, watermark);
			}
		}
	}
exit:
	mutex_unlock(&acc->lock);
	return err;
}

static int lis3dh_acc_enable_set(struct sensors_classdev *sensors_cdev,
	unsigned int enable)
{
	struct lis3dh_acc_data *acc = container_of(sensors_cdev,
		struct lis3dh_acc_data, cdev);
	int err;

	if (enable) {
		err = lis3dh_acc_enable(acc);
		if (err < 0) {
			dev_err(&acc->client->dev, "enable error\n");
			return err;
		}
	} else {
		err = lis3dh_acc_disable(acc);
		if (err < 0) {
			dev_err(&acc->client->dev, "enable error\n");
			return err;
		}
	}
	return err;
}
static int lis3dh_latency_set(struct sensors_classdev *cdev,
					unsigned int max_latency)
{
	struct lis3dh_acc_data *acc = container_of(cdev,
		struct lis3dh_acc_data, cdev);
	struct i2c_client *client = acc->client;

	/* Does not support batch in while interrupt is not enabled */
	if (!acc->pdata->enable_int) {
		dev_err(&client->dev,
			"Cannot set batch mode, interrupt is not enabled!\n");
		return -EPERM;
	}
	acc->use_batch = max_latency ? true : false;
	acc->fifo_timeout_ms = max_latency;
	return 0;
}

/* Work function for polling mode */
static void lis3dh_acc_input_work_func(struct work_struct *work)
{
	struct lis3dh_acc_data *acc;

	int xyz[3] = { 0 };
	int err;

	acc = container_of((struct delayed_work *)work,
			struct lis3dh_acc_data,	input_work);

	mutex_lock(&acc->lock);
	err = lis3dh_acc_get_acceleration_data(acc, xyz);
	if (err < 0)
		dev_err(&acc->client->dev, "get_acceleration_data failed\n");
	else
		lis3dh_acc_report_values(acc, xyz);

	schedule_delayed_work(&acc->input_work, msecs_to_jiffies(
			acc->delay_ms));
	mutex_unlock(&acc->lock);
}

int lis3dh_acc_input_open(struct input_dev *input)
{
	struct lis3dh_acc_data *acc = input_get_drvdata(input);

	return lis3dh_acc_enable(acc);
}

void lis3dh_acc_input_close(struct input_dev *dev)
{
	struct lis3dh_acc_data *acc = input_get_drvdata(dev);

	lis3dh_acc_disable(acc);
}

static int lis3dh_acc_validate_pdata(struct lis3dh_acc_data *acc)
{
	acc->pdata->init_interval = max(acc->pdata->init_interval,
			acc->pdata->min_interval);

	if (acc->pdata->axis_map_x > 2 ||
		acc->pdata->axis_map_y > 2 ||
		 acc->pdata->axis_map_z > 2) {
		dev_err(&acc->client->dev,
			"invalid axis_map value x:%u y:%u z%u\n",
			acc->pdata->axis_map_x,
			acc->pdata->axis_map_y, acc->pdata->axis_map_z);
		return -EINVAL;
	}

	/* Only allow 0 and 1 for negation boolean flag */
	if (acc->pdata->negate_x > 1 || acc->pdata->negate_y > 1
			|| acc->pdata->negate_z > 1) {
		dev_err(&acc->client->dev,
			"invalid negate value x:%u y:%u z:%u\n",
			acc->pdata->negate_x,
			acc->pdata->negate_y, acc->pdata->negate_z);
		return -EINVAL;
	}

	/* Enforce minimum polling interval */
	if (acc->pdata->init_interval < acc->pdata->min_interval) {
		dev_err(&acc->client->dev, "minimum poll interval violated\n");
		return -EINVAL;
	}

	return 0;
}

static int lis3dh_acc_input_init(struct lis3dh_acc_data *acc)
{
	int err;

	if (!acc->pdata->enable_int)
		INIT_DELAYED_WORK(&acc->input_work, lis3dh_acc_input_work_func);
	acc->input_dev = input_allocate_device();
	if (!acc->input_dev) {
		err = -ENOMEM;
		dev_err(&acc->client->dev, "input device allocation failed\n");
		goto err0;
	}

	acc->input_dev->open = lis3dh_acc_input_open;
	acc->input_dev->close = lis3dh_acc_input_close;
	acc->input_dev->name = ACCEL_INPUT_DEV_NAME;
	acc->input_dev->id.bustype = BUS_I2C;
	acc->input_dev->dev.parent = &acc->client->dev;

	input_set_drvdata(acc->input_dev, acc);

	set_bit(EV_ABS, acc->input_dev->evbit);
	/*	next is used for interruptA sources data if the case */
	set_bit(ABS_MISC, acc->input_dev->absbit);
	/*	next is used for interruptB sources data if the case */
	set_bit(ABS_WHEEL, acc->input_dev->absbit);

	input_set_abs_params(acc->input_dev, ABS_RX, 0, (1UL << 31) - 1,
						FUZZ, FLAT);
	input_set_abs_params(acc->input_dev, ABS_RY, 0, (1UL << 31) - 1,
						FUZZ, FLAT);
	input_set_abs_params(acc->input_dev, ABS_X, -G_MAX, G_MAX, FUZZ, FLAT);
	input_set_abs_params(acc->input_dev, ABS_Y, -G_MAX, G_MAX, FUZZ, FLAT);
	input_set_abs_params(acc->input_dev, ABS_Z, -G_MAX, G_MAX, FUZZ, FLAT);
	/*	next is used for interruptA sources data if the case */
	input_set_abs_params(acc->input_dev, ABS_MISC, INT_MIN, INT_MAX, 0, 0);
	/*	next is used for interruptB sources data if the case */
	input_set_abs_params(acc->input_dev, ABS_WHEEL, INT_MIN, INT_MAX, 0, 0);


	err = input_register_device(acc->input_dev);
	if (err) {
		dev_err(&acc->client->dev,
				"unable to register input device %s\n",
				acc->input_dev->name);
		goto err1;
	}

	return 0;

err1:
	input_free_device(acc->input_dev);
err0:
	return err;
}

static void lis3dh_acc_input_cleanup(struct lis3dh_acc_data *acc)
{
	input_unregister_device(acc->input_dev);
	input_free_device(acc->input_dev);
}

static int lis3dh_pinctrl_init(struct lis3dh_acc_data *acc)
{
	struct i2c_client *client = acc->client;

	acc->pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR_OR_NULL(acc->pinctrl)) {
		dev_err(&client->dev, "Failed to get pinctrl\n");
		return PTR_ERR(acc->pinctrl);
	}

	acc->pin_default =
		pinctrl_lookup_state(acc->pinctrl, "lis3dh_default");
	if (IS_ERR_OR_NULL(acc->pin_default)) {
		dev_err(&client->dev, "Failed to look up default state\n");
		return PTR_ERR(acc->pin_default);
	}

	acc->pin_sleep =
		pinctrl_lookup_state(acc->pinctrl, "lis3dh_sleep");
	if (IS_ERR_OR_NULL(acc->pin_sleep)) {
		dev_err(&client->dev, "Failed to look up sleep state\n");
		return PTR_ERR(acc->pin_sleep);
	}

	return 0;
}

#ifdef CONFIG_OF
static int lis3dh_parse_dt(struct device *dev,
			struct lis3dh_acc_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	u32 temp_val;
	int rc;

	rc = of_property_read_u32(np, "st,min-interval", &temp_val);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read min-interval\n");
		return rc;
	} else {
		pdata->min_interval = temp_val;
	}

	rc = of_property_read_u32(np, "st,init-interval", &temp_val);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read init-interval\n");
		return rc;
	} else {
		pdata->init_interval = temp_val;
	}

	rc = of_property_read_u32(np, "st,axis-map-x", &temp_val);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read axis-map_x\n");
		return rc;
	} else {
		pdata->axis_map_x = (u8)temp_val;
	}

	rc = of_property_read_u32(np, "st,axis-map-y", &temp_val);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read axis_map_y\n");
		return rc;
	} else {
		pdata->axis_map_y = (u8)temp_val;
	}

	rc = of_property_read_u32(np, "st,axis-map-z", &temp_val);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read axis-map-z\n");
		return rc;
	} else {
		pdata->axis_map_z = (u8)temp_val;
	}

	rc = of_property_read_u32(np, "st,g-range", &temp_val);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read g-range\n");
		return rc;
	} else {
		switch (temp_val) {
		case 2:
			pdata->g_range = LIS3DH_ACC_G_2G;
			break;
		case 4:
			pdata->g_range = LIS3DH_ACC_G_4G;
			break;
		case 8:
			pdata->g_range = LIS3DH_ACC_G_8G;
			break;
		case 16:
			pdata->g_range = LIS3DH_ACC_G_16G;
			break;
		default:
			pdata->g_range = LIS3DH_ACC_G_2G;
			break;
		}
	}

	pdata->negate_x = of_property_read_bool(np, "st,negate-x");

	pdata->negate_y = of_property_read_bool(np, "st,negate-y");

	pdata->negate_z = of_property_read_bool(np, "st,negate-z");

	pdata->enable_int = of_property_read_bool(np, "st,enable-int");

	pdata->gpio_int1 = of_get_named_gpio_flags(dev->of_node,
				"st,gpio-int1", 0, NULL);

	pdata->gpio_int2 = of_get_named_gpio_flags(dev->of_node,
				"st,gpio-int2", 0, NULL);
	return 0;
}
#else
static int lis3dh_parse_dt(struct device *dev,
			struct lis3dh_acc_platform_data *pdata)
{
	return -EINVAL;
}
#endif

static int lis3dh_acc_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{

	struct lis3dh_acc_data *acc;
	int err = -1;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "client not i2c capable\n");
		err = -ENODEV;
		goto exit_check_functionality_failed;
	}

	acc = kzalloc(sizeof(struct lis3dh_acc_data), GFP_KERNEL);
	if (acc == NULL) {
		err = -ENOMEM;
		dev_err(&client->dev,
				"failed to allocate memory for module data: "
					"%d\n", err);
		goto exit_check_functionality_failed;
	}


	mutex_init(&acc->lock);
	mutex_lock(&acc->lock);

	acc->client = client;
	i2c_set_clientdata(client, acc);

	acc->pdata = kmalloc(sizeof(*acc->pdata), GFP_KERNEL);
	if (acc->pdata == NULL) {
		err = -ENOMEM;
		dev_err(&client->dev,
				"failed to allocate memory for pdata: %d\n",
				err);
		goto err_mutexunlock;
	}
	memset(acc->pdata, 0 , sizeof(*acc->pdata));

	if (client->dev.of_node) {
		err = lis3dh_parse_dt(&client->dev, acc->pdata);
		if (err) {
			dev_err(&client->dev, "Failed to parse device tree\n");
			err = -EINVAL;
			goto exit_kfree_pdata;
		}
	} else if (client->dev.platform_data != NULL) {
		memcpy(acc->pdata, client->dev.platform_data,
			sizeof(*acc->pdata));
	} else {
		dev_err(&client->dev, "No valid platform data. exiting.\n");
		err = -ENODEV;
		goto exit_kfree_pdata;
	}

	err = lis3dh_acc_validate_pdata(acc);
	if (err < 0) {
		dev_err(&client->dev, "failed to validate platform data\n");
		goto exit_kfree_pdata;
	}

	/* initialize pinctrl */
	err = lis3dh_pinctrl_init(acc);
	if (err) {
		dev_err(&client->dev, "Can't initialize pinctrl\n");
			goto exit_kfree_pdata;
	}
	err = pinctrl_select_state(acc->pinctrl, acc->pin_default);
	if (err) {
		dev_err(&client->dev,
			"Can't select pinctrl default state\n");
		goto exit_kfree_pdata;
	}

	if (acc->pdata->init) {
		err = acc->pdata->init();
		if (err < 0) {
			dev_err(&client->dev, "init failed: %d\n", err);
			goto err_pdata_init;
		}
	}

	if (gpio_is_valid(acc->pdata->gpio_int1)
			&& acc->pdata->enable_int)
		acc->irq1 = gpio_to_irq(acc->pdata->gpio_int1);

	if (gpio_is_valid(acc->pdata->gpio_int2)
			&& acc->pdata->enable_int)
		acc->irq2 = gpio_to_irq(acc->pdata->gpio_int2);

	memset(acc->resume_state, 0, ARRAY_SIZE(acc->resume_state));

	acc->resume_state[RES_CTRL_REG1] = LIS3DH_ACC_ENABLE_ALL_AXES;
	acc->resume_state[RES_CTRL_REG2] = 0x00;
	acc->resume_state[RES_CTRL_REG3] = 0x00;
	acc->resume_state[RES_CTRL_REG4] = 0x00;
	acc->resume_state[RES_CTRL_REG5] = 0x00;
	acc->resume_state[RES_CTRL_REG6] = 0x00;

	acc->resume_state[RES_TEMP_CFG_REG] = 0x00;
	acc->resume_state[RES_FIFO_CTRL_REG] = 0x00;
	acc->resume_state[RES_INT_CFG1] = 0x00;
	acc->resume_state[RES_INT_THS1] = 0x00;
	acc->resume_state[RES_INT_DUR1] = 0x00;

	acc->resume_state[RES_TT_CFG] = 0x00;
	acc->resume_state[RES_TT_THS] = 0x00;
	acc->resume_state[RES_TT_LIM] = 0x00;
	acc->resume_state[RES_TT_TLAT] = 0x00;
	acc->resume_state[RES_TT_TW] = 0x00;

	if (acc->pdata->enable_int) {
		if (gpio_is_valid(acc->pdata->gpio_int1))
			acc->resume_state[RES_CTRL_REG3] = CONFIG_IRQ_DRDY1;
		acc->resume_state[RES_CTRL_REG4] = CONFIG_BLOCK_READ;
	}

	err = lis3dh_acc_config_regulator(acc, true);
	if (err < 0) {
		dev_err(&client->dev, "Configure power failed: %d\n", err);
		goto err_pdata_init;
	}

	err = lis3dh_acc_device_power_on(acc);
	if (err < 0) {
		dev_err(&client->dev, "power on failed: %d\n", err);
		goto err_regulator_init;
	}

	atomic_set(&acc->enabled, 1);

	err = lis3dh_acc_update_g_range(acc, acc->pdata->g_range);
	if (err < 0) {
		dev_err(&client->dev, "update_g_range failed\n");
		goto  err_power_off;
	}

	acc->use_batch = false;
	acc->batch_mode = BATCH_MODE_NORMAL;
	acc->fifo_timeout_ms = 0;
	acc->delay_ms = acc->pdata->init_interval;
	err = lis3dh_acc_update_odr(acc, acc->delay_ms);
	if (err < 0) {
		dev_err(&client->dev, "update_odr failed\n");
		goto  err_power_off;
	}

	err = lis3dh_acc_input_init(acc);
	if (err < 0) {
		dev_err(&client->dev, "input init failed\n");
		goto err_power_off;
	}


	err = create_sysfs_interfaces(&client->dev);
	if (err < 0) {
		dev_err(&client->dev,
		   "device LIS3DH_ACC_DEV_NAME sysfs register failed\n");
		goto err_input_cleanup;
	}

	acc->cdev = lis3dh_acc_cdev;
	acc->cdev.sensors_enable = lis3dh_acc_enable_set;
	acc->cdev.sensors_poll_delay = lis3dh_acc_poll_delay_set;
	/* batch is possiable only when interrupt is enabled */
	if (gpio_is_valid(acc->pdata->gpio_int1) && acc->pdata->enable_int) {
		acc->cdev.sensors_set_latency = lis3dh_latency_set;
		acc->cdev.sensors_flush = lis3dh_acc_flush;
		acc->cdev.fifo_reserved_event_count = LIS3DH_FIFO_SIZE;
		acc->cdev.fifo_max_event_count = LIS3DH_FIFO_SIZE;
	}
	err = sensors_classdev_register(&client->dev, &acc->cdev);
	if (err) {
		dev_err(&client->dev,
			"class device create failed: %d\n", err);
		goto err_remove_sysfs_int;
	}

	lis3dh_acc_device_power_off(acc);

	/* As default, do not report information */
	atomic_set(&acc->enabled, 0);

	if (gpio_is_valid(acc->pdata->gpio_int1) && acc->pdata->enable_int) {
		err = request_threaded_irq(acc->irq1, NULL,
				lis3dh_acc_isr1,
				IRQF_TRIGGER_RISING | IRQF_ONESHOT,
				"lis3dh_acc_irq1", acc);
		if (err < 0) {
			dev_err(&client->dev,
					"request irq1 failed: %d\n", err);
			goto err_unreg_sensor_class;
		}
		disable_irq_nosync(acc->irq1);
	}

	if (gpio_is_valid(acc->pdata->gpio_int2) && acc->pdata->enable_int) {
		err = request_threaded_irq(acc->irq2, NULL,
				lis3dh_acc_isr2,
				IRQF_TRIGGER_RISING | IRQF_ONESHOT,
				"lis3dh_acc_irq2", acc);
		if (err < 0) {
			dev_err(&client->dev,
					"request irq2 failed: %d\n", err);
			goto err_free_irq1;
		}
		disable_irq_nosync(acc->irq2);
	}

	if (acc->pdata->enable_int)
		device_init_wakeup(&client->dev, 1);

	if (pinctrl_select_state(acc->pinctrl, acc->pin_sleep))
		dev_err(&client->dev,
			"Can't select pinctrl sleep state\n");

	mutex_unlock(&acc->lock);

	dev_dbg(&client->dev, "%s: probed\n", LIS3DH_ACC_DEV_NAME);

	return 0;

err_free_irq1:
if (gpio_is_valid(acc->pdata->gpio_int1) && acc->pdata->enable_int)
	free_irq(acc->irq1, acc);
err_unreg_sensor_class:
	sensors_classdev_unregister(&acc->cdev);
err_remove_sysfs_int:
	remove_sysfs_interfaces(&client->dev);
err_input_cleanup:
	lis3dh_acc_input_cleanup(acc);
err_power_off:
	lis3dh_acc_device_power_off(acc);
err_regulator_init:
	lis3dh_acc_config_regulator(acc, false);
err_pdata_init:
	if (acc->pdata->exit)
		acc->pdata->exit();
exit_kfree_pdata:
	kfree(acc->pdata);
err_mutexunlock:
	mutex_unlock(&acc->lock);
	kfree(acc);
exit_check_functionality_failed:
	dev_err(&client->dev, "%s: Driver Init failed\n", LIS3DH_ACC_DEV_NAME);
	return err;
}

static int lis3dh_acc_remove(struct i2c_client *client)
{
	struct lis3dh_acc_data *acc = i2c_get_clientdata(client);

	lis3dh_acc_device_power_off(acc);
	if (gpio_is_valid(acc->pdata->gpio_int1) && acc->pdata->enable_int)
		free_irq(acc->irq1, acc);

	if (gpio_is_valid(acc->pdata->gpio_int2) && acc->pdata->enable_int)
		free_irq(acc->irq2, acc);

	sensors_classdev_unregister(&acc->cdev);
	lis3dh_acc_input_cleanup(acc);
	lis3dh_acc_config_regulator(acc, false);
	remove_sysfs_interfaces(&client->dev);

	if (acc->pdata->exit)
		acc->pdata->exit();
	kfree(acc->pdata);
	kfree(acc);

	return 0;
}

#ifdef CONFIG_PM
static int lis3dh_acc_resume(struct i2c_client *client)
{
	struct lis3dh_acc_data *acc = i2c_get_clientdata(client);
	int err;

	if (!acc->on_before_suspend)
		return 0;

	if (!acc->use_batch) {
		err = lis3dh_acc_enable(acc);
		if (err < 0)
			dev_err(&client->dev,
				"Resume: fail to enable sensor\n");
		return 0;
	}
	/* resume to FIFO mode */
	if (acc->batch_mode == BATCH_MODE_WAKE_UPON_FIFO_FULL) {
		irq_set_irq_wake(acc->irq1, 0);
		dev_dbg(&client->dev, "Resume: cancel irq wakeup\n");
	} else {
		err = lis3dh_set_fifo_mode(client, LIS3DH_FIFO_MODE);
		dev_dbg(&client->dev, "Resume: swich back to FIFO mode\n");
		if (err < 0)
			dev_err(&client->dev,
				"Resume: set fifo mode error\n");
	}
	return 0;
}

static int lis3dh_acc_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct lis3dh_acc_data *acc = i2c_get_clientdata(client);
	int err;

	acc->on_before_suspend = atomic_read(&acc->enabled);
	if (!acc->on_before_suspend)
		return 0;

	/* Power off the sensor if not in batch */
	if (!acc->use_batch) {
		err = lis3dh_acc_disable(acc);
		if (err < 0)
			dev_err(&client->dev,
				"Suspend: fail to disable sensor\n");
		return 0;
	}

	/*
	 * set IRQ wakeup if FIFO full wakeup is required,
	 * otherwise switch to steam mode and drop data.
	 */
	if (acc->batch_mode == BATCH_MODE_WAKE_UPON_FIFO_FULL) {
		irq_set_irq_wake(acc->irq1, 1);
		dev_dbg(&client->dev, "Suspend: Wakeup upon FIFO full\n");
	} else {
		err = lis3dh_set_fifo_mode(client, LIS3DH_STREAM_MODE);
		dev_dbg(&client->dev, "Suspend: swich to STREAM mode\n");
		if (err < 0)
			dev_err(&client->dev,
				"Suspend: set fifo mode error\n");
	}

	return 0;
}
#else
#define lis3dh_acc_suspend	NULL
#define lis3dh_acc_resume	NULL
#endif /* CONFIG_PM */

static const struct i2c_device_id lis3dh_acc_id[]
		= { { LIS3DH_ACC_DEV_NAME, 0 }, { }, };

static struct of_device_id lis3dh_acc_match_table[] = {
	{ .compatible = "st,lis3dh", },
	{ },
};

MODULE_DEVICE_TABLE(i2c, lis3dh_acc_id);

static struct i2c_driver lis3dh_acc_driver = {
	.driver = {
			.owner = THIS_MODULE,
			.name = LIS3DH_ACC_DEV_NAME,
			.of_match_table = lis3dh_acc_match_table,
		  },
	.probe = lis3dh_acc_probe,
	.remove = lis3dh_acc_remove,
	.suspend = lis3dh_acc_suspend,
	.resume = lis3dh_acc_resume,
	.id_table = lis3dh_acc_id,
};

static int __init lis3dh_acc_init(void)
{
	return i2c_add_driver(&lis3dh_acc_driver);
}

static void __exit lis3dh_acc_exit(void)
{
	i2c_del_driver(&lis3dh_acc_driver);
	return;
}

module_init(lis3dh_acc_init);
module_exit(lis3dh_acc_exit);

MODULE_DESCRIPTION("lis3dh digital accelerometer sysfs driver");
MODULE_AUTHOR("Matteo Dameno, Carmine Iascone, Samuel Huo, STMicroelectronics");
MODULE_LICENSE("GPL");

