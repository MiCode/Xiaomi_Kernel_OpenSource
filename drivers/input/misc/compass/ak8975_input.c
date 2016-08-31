/* Copyright (C) 2012 Invensense, Inc.
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/regulator/consumer.h>
#include <linux/mpu.h>


#define AKM_VERSION			(23)
#define AKM_NAME			"akm89xx"
#define AKM_HW_DELAY_POR_MS		(50)
#define AKM_HW_DELAY_TSM_MS		(10)	/* Time Single Measurement */
#define AKM_HW_DELAY_US			(100)
#define AKM_HW_DELAY_ROM_ACCESS_US	(200)
#define AKM_POLL_DELAY_MS_DFLT		(200)
#define AKM_MPU_RETRY_COUNT		(50)
#define AKM_MPU_RETRY_DELAY_MS		(20)
#define AKM_ERR_CNT_MAX			(20)

#define AKM_INPUT_RESOLUTION		(1)
#define AKM_INPUT_DIVISOR		(1)
#define AKM_INPUT_DELAY_MS_MIN		(AKM_HW_DELAY_TSM_MS)
#define AKM_INPUT_POWER_UA		(10000)
#define AKM_INPUT_RANGE			(4912)

#define AKM_REG_WIA			(0x00)
#define AKM_WIA_ID			(0x48)
#define AKM_REG_INFO			(0x01)
#define AKM_REG_ST1			(0x02)
#define AKM_ST1_DRDY			(0x01)
#define AKM_ST1_DOR			(0x02)
#define AKM_REG_HXL			(0x03)
#define AKM_REG_HXH			(0x04)
#define AKM_REG_HYL			(0x05)
#define AKM_REG_HYH			(0x06)
#define AKM_REG_HZL			(0x07)
#define AKM_REG_HZH			(0x08)
#define AKM_REG_ST2			(0x09)
#define AKM_ST2_DERR			(0x04)
#define AKM_ST2_HOFL			(0x08)
#define AKM_ST2_BITM			(0x10)
#define AKM_REG_CNTL1			(0x0A)
#define AKM_CNTL1_MODE_MASK		(0x0F)
#define AKM_CNTL1_MODE_POWERDOWN	(0x00)
#define AKM_CNTL1_MODE_SINGLE		(0x01)
#define AKM_CNTL1_MODE_CONT1		(0x02)
#define AKM_CNTL1_MODE_CONT2		(0x06)
#define AKM_CNTL1_MODE_SELFTEST		(0x08)
#define AKM_CNTL1_MODE_ROM_ACCESS	(0x0F)
#define AKM_CNTL1_OUTPUT_BIT16		(0x10)
#define AKM_REG_CNTL2			(0x0B)
#define AKM_CNTL2_SRST			(0x01)
#define AKM_REG_ASTC			(0x0C)
#define AKM_REG_ASTC_SELF		(0x40)
#define AKM_REG_TS1			(0x0D)
#define AKM_REG_TS2			(0x0E)
#define AKM_REG_I2CDIS			(0x0F)
#define AKM_REG_ASAX			(0x10)
#define AKM_REG_ASAY			(0x11)
#define AKM_REG_ASAZ			(0x12)

#define AKM8963_SCALE14			(19661)
#define AKM8963_SCALE16			(4915)
#define AKM8972_SCALE			(19661)
#define AKM8975_SCALE			(9830)
#define AKM8975_RANGE_X_LO		(-100)
#define AKM8975_RANGE_X_HI		(100)
#define AKM8975_RANGE_Y_LO		(-100)
#define AKM8975_RANGE_Y_HI		(100)
#define AKM8975_RANGE_Z_LO		(-1000)
#define AKM8975_RANGE_Z_HI		(-300)
#define AKM8972_RANGE_X_LO		(-50)
#define AKM8972_RANGE_X_HI		(50)
#define AKM8972_RANGE_Y_LO		(-50)
#define AKM8972_RANGE_Y_HI		(50)
#define AKM8972_RANGE_Z_LO		(-500)
#define AKM8972_RANGE_Z_HI		(-100)
#define AKM8963_RANGE14_X_LO		(-50)
#define AKM8963_RANGE14_X_HI		(50)
#define AKM8963_RANGE14_Y_LO		(-50)
#define AKM8963_RANGE14_Y_HI		(50)
#define AKM8963_RANGE14_Z_LO		(-800)
#define AKM8963_RANGE14_Z_HI		(-200)
#define AKM8963_RANGE16_X_LO		(-200)
#define AKM8963_RANGE16_X_HI		(200)
#define AKM8963_RANGE16_Y_LO		(-200)
#define AKM8963_RANGE16_Y_HI		(200)
#define AKM8963_RANGE16_Z_LO		(-3200)
#define AKM8963_RANGE16_Z_HI		(-800)

#define AXIS_X				(0)
#define AXIS_Y				(1)
#define AXIS_Z				(2)
#define WR				(0)
#define RD				(1)

#define AKM_DBG_SPEW_MSG		(1 << 0)
#define AKM_DBG_SPEW_MAGNETIC_FIELD	(1 << 1)
#define AKM_DBG_SPEW_MAGNETIC_FIELD_RAW	(1 << 2)

enum AKM_DATA_INFO {
	AKM_DATA_INFO_DATA = 0,
	AKM_DATA_INFO_VER,
	AKM_DATA_INFO_RESET,
	AKM_DATA_INFO_REGS,
	AKM_DATA_INFO_DBG,
	AKM_DATA_INFO_MAGNETIC_FIELD_SPEW,
	AKM_DATA_INFO_MAGNETIC_FIELD_RAW_SPEW,
	AKM_DATA_INFO_LIMIT_MAX,
};

/* regulator names in order of powering on */
static char *akm_vregs[] = {
	"vdd",
	"vid",
};

static char *akm_configs[] = {
	"auto",
	"mpu",
	"host",
};

struct akm_asa {
	u8 asa[3];			/* axis sensitivity adjustment */
	s16 range_lo[3];
	s16 range_hi[3];
};

struct akm_inf {
	struct i2c_client *i2c;
	struct mutex mutex_data;
	struct input_dev *idev;
	struct workqueue_struct *wq;
	struct delayed_work dw;
	struct regulator_bulk_data vreg[ARRAY_SIZE(akm_vregs)];
	struct mpu_platform_data pdata;
	struct akm_asa asa;		/* data for calibration */
	unsigned int poll_delay_us;	/* requested sampling delay (us) */
	unsigned int range_i;		/* max_range index */
	unsigned int resolution;	/* report when new data outside this */
	unsigned int data_info;		/* data info to return */
	unsigned int dbg;		/* device id */
	unsigned int dev_id;		/* device id */
	bool use_mpu;			/* if device behind MPU */
	bool initd;			/* set if initialized */
	bool enable;			/* enable status */
	bool fifo_enable;		/* MPU FIFO enable */
	bool port_en[2];		/* enable status of MPU write port */
	int port_id[2];			/* MPU port ID */
	u8 data_out;			/* write value to trigger a sample */
	s16 xyz_raw[3];			/* raw sample data */
	s16 xyz[3];			/* sample data after calibration */
};


static int akm_i2c_rd(struct akm_inf *inf, u8 reg, u16 len, u8 *val)
{
	struct i2c_msg msg[2];

	msg[0].addr = inf->i2c->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &reg;
	msg[1].addr = inf->i2c->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = len;
	msg[1].buf = val;
	if (i2c_transfer(inf->i2c->adapter, msg, 2) != 2)
		return -EIO;

	return 0;
}

static int akm_i2c_wr(struct akm_inf *inf, u8 reg, u8 val)
{
	struct i2c_msg msg;
	u8 buf[2];

	buf[0] = reg;
	buf[1] = val;
	msg.addr = inf->i2c->addr;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = buf;
	if (i2c_transfer(inf->i2c->adapter, &msg, 1) != 1)
		return -EIO;

	return 0;
}

static int akm_vreg_dis(struct akm_inf *inf, int i)
{
	int err = 0;

	if (inf->vreg[i].ret && (inf->vreg[i].consumer != NULL)) {
		err = regulator_disable(inf->vreg[i].consumer);
		if (err)
			dev_err(&inf->i2c->dev, "%s %s ERR\n",
				__func__, inf->vreg[i].supply);
		else
			dev_dbg(&inf->i2c->dev, "%s %s\n",
				__func__, inf->vreg[i].supply);
	}
	inf->vreg[i].ret = 0;
	return err;
}

static int akm_vreg_dis_all(struct akm_inf *inf)
{
	unsigned int i;
	int err = 0;

	for (i = ARRAY_SIZE(akm_vregs); i > 0; i--)
		err |= akm_vreg_dis(inf, (i - 1));
	return err;
}

static int akm_vreg_en(struct akm_inf *inf, int i)
{
	int err = 0;

	if ((!inf->vreg[i].ret) && (inf->vreg[i].consumer != NULL)) {
		err = regulator_enable(inf->vreg[i].consumer);
		if (!err) {
			inf->vreg[i].ret = 1;
			dev_dbg(&inf->i2c->dev, "%s %s\n",
				__func__, inf->vreg[i].supply);
			err = 1; /* flag regulator state change */
		} else {
			dev_err(&inf->i2c->dev, "%s %s ERR\n",
				__func__, inf->vreg[i].supply);
		}
	}
	return err;
}

static int akm_vreg_en_all(struct akm_inf *inf)
{
	int i;
	int err = 0;

	for (i = 0; i < ARRAY_SIZE(akm_vregs); i++)
		err |= akm_vreg_en(inf, i);
	return err;
}

static void akm_vreg_exit(struct akm_inf *inf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(akm_vregs); i++) {
		if (inf->vreg[i].consumer != NULL) {
			devm_regulator_put(inf->vreg[i].consumer);
			inf->vreg[i].consumer = NULL;
			dev_dbg(&inf->i2c->dev, "%s %s\n",
				__func__, inf->vreg[i].supply);
		}
	}
}

static int akm_vreg_init(struct akm_inf *inf)
{
	unsigned int i;
	int err = 0;

	for (i = 0; i < ARRAY_SIZE(akm_vregs); i++) {
		inf->vreg[i].supply = akm_vregs[i];
		inf->vreg[i].ret = 0;
		inf->vreg[i].consumer = devm_regulator_get(&inf->i2c->dev,
							  inf->vreg[i].supply);
		if (IS_ERR(inf->vreg[i].consumer)) {
			err |= PTR_ERR(inf->vreg[i].consumer);
			dev_err(&inf->i2c->dev, "%s err %d for %s\n",
				__func__, err, inf->vreg[i].supply);
			inf->vreg[i].consumer = NULL;
		} else {
			dev_dbg(&inf->i2c->dev, "%s %s\n",
				__func__, inf->vreg[i].supply);
		}
	}
	return err;
}

static int akm_pm(struct akm_inf *inf, bool enable)
{
	int err;

	if (enable) {
		err = akm_vreg_en_all(inf);
		if (err)
			mdelay(AKM_HW_DELAY_POR_MS);
	} else {
		err = akm_vreg_dis_all(inf);
	}
	if (err > 0)
		err = 0;
	if (err)
		dev_err(&inf->i2c->dev, "%s pwr=%x ERR=%d\n",
			__func__, enable, err);
	else
		dev_dbg(&inf->i2c->dev, "%s pwr=%x\n",
			__func__, enable);
	return err;
}

static int akm_port_free(struct akm_inf *inf, int port)
{
	int err = 0;

	if ((inf->use_mpu) && (inf->port_id[port] >= 0)) {
		err = nvi_mpu_port_free(inf->port_id[port]);
		if (!err)
			inf->port_id[port] = -1;
	}
	return err;
}

static int akm_ports_free(struct akm_inf *inf)
{
	int err;

	err = akm_port_free(inf, WR);
	err |= akm_port_free(inf, RD);
	return err;
}

static void akm_pm_exit(struct akm_inf *inf)
{
	akm_ports_free(inf);
	akm_pm(inf, false);
	akm_vreg_exit(inf);
}

static int akm_pm_init(struct akm_inf *inf)
{
	int err;

	inf->initd = false;
	inf->enable = false;
	inf->fifo_enable = false; /* DON'T ENABLE: MPU FIFO HW BROKEN */
	inf->port_en[WR] = false;
	inf->port_en[RD] = false;
	inf->port_id[WR] = -1;
	inf->port_id[RD] = -1;
	inf->poll_delay_us = (AKM_POLL_DELAY_MS_DFLT * 1000);
	akm_vreg_init(inf);
	err = akm_pm(inf, true);
	return err;
}

static int akm_nvi_mpu_bypass_request(struct akm_inf *inf)
{
	int i;
	int err = 0;

	if (inf->use_mpu) {
		for (i = 0; i < AKM_MPU_RETRY_COUNT; i++) {
			err = nvi_mpu_bypass_request(true);
			if ((!err) || (err == -EPERM))
				break;

			msleep(AKM_MPU_RETRY_DELAY_MS);
		}
		if (err == -EPERM)
			err = 0;
	}
	return err;
}

static int akm_nvi_mpu_bypass_release(struct akm_inf *inf)
{
	int err = 0;

	if (inf->use_mpu)
		err = nvi_mpu_bypass_release();
	return err;
}

static int akm_wr(struct akm_inf *inf, u8 reg, u8 val)
{
	int err = 0;

	err = akm_nvi_mpu_bypass_request(inf);
	if (!err) {
		err = akm_i2c_wr(inf, AKM_REG_CNTL1, AKM_CNTL1_MODE_POWERDOWN);
		udelay(AKM_HW_DELAY_US);
		err |= akm_i2c_wr(inf, reg, val);
		akm_nvi_mpu_bypass_release(inf);
	}
	return err;
}

static int akm_port_enable(struct akm_inf *inf, int port, bool enable)
{
	int err = 0;

	if (enable != inf->port_en[port]) {
		err = nvi_mpu_enable(inf->port_id[port],
				     enable, inf->fifo_enable);
		if (!err)
			inf->port_en[port] = enable;
	}
	return err;
}

static int akm_ports_enable(struct akm_inf *inf, bool enable)
{
	int err;

	err = akm_port_enable(inf, RD, enable);
	err |= akm_port_enable(inf, WR, enable);
	return err;
}

static int akm_mode_wr(struct akm_inf *inf, bool reset,
		       unsigned int range_i, u8 mode)
{
	u8 mode_old;
	u8 mode_new;
	u8 val;
	int i;
	int err = 0;

	mode_new = mode;
	if (range_i)
		mode |= AKM_CNTL1_OUTPUT_BIT16;
	if ((mode == inf->data_out) && (!reset))
		return err;

	mode_old = inf->data_out & AKM_CNTL1_MODE_MASK;
	if (inf->use_mpu)
		err = akm_ports_enable(inf, false);
	else
		cancel_delayed_work_sync(&inf->dw);
	if (err)
		return err;

	if (reset) {
		if (inf->dev_id == COMPASS_ID_AK8963) {
			err = akm_nvi_mpu_bypass_request(inf);
			if (!err) {
				err = akm_wr(inf, AKM_REG_CNTL2,
					     AKM_CNTL2_SRST);
				for (i = 0; i < AKM_HW_DELAY_POR_MS; i++) {
					mdelay(1);
					err = akm_i2c_rd(inf, AKM_REG_CNTL2,
							 1, &val);
					if (err)
						continue;

					if (!(val & AKM_CNTL2_SRST))
						break;
				}
				akm_nvi_mpu_bypass_release(inf);
			}
		}
	}
	inf->range_i = range_i;
	inf->data_out = mode;
	if (inf->use_mpu) {
		if ((mode_old > AKM_CNTL1_MODE_SINGLE) ||
					    (mode_new > AKM_CNTL1_MODE_SINGLE))
			err = akm_wr(inf, AKM_REG_CNTL1, mode);
		if (mode_new <= AKM_CNTL1_MODE_SINGLE) {
			err |= nvi_mpu_data_out(inf->port_id[WR], mode);
			if (mode_new)
				err |= akm_ports_enable(inf, true);
		} else {
			err |= akm_port_enable(inf, RD, true);
		}
	} else {
		err = akm_wr(inf, AKM_REG_CNTL1, mode);
		if (mode_new)
			queue_delayed_work(inf->wq, &inf->dw,
					 usecs_to_jiffies(inf->poll_delay_us));
	}
	return err;
}

static int akm_delay(struct akm_inf *inf, unsigned int delay_us)
{
	u8 mode;
	int err = 0;

	if (inf->use_mpu)
		err |= nvi_mpu_delay_us(inf->port_id[RD],
					(unsigned int)delay_us);
	if (!err) {
		if (inf->dev_id == COMPASS_ID_AK8963) {
			if (delay_us == (AKM_INPUT_DELAY_MS_MIN * 1000))
				mode = AKM_CNTL1_MODE_CONT2;
			else
				mode = AKM_CNTL1_MODE_SINGLE;
			err = akm_mode_wr(inf, false, inf->range_i, mode);
		}
	}
	return err;
}

static void akm_calc(struct akm_inf *inf, u8 *data)
{
	s16 x;
	s16 y;
	s16 z;

	/* data[1] = AKM_REG_HXL
	 * data[2] = AKM_REG_HXH
	 * data[3] = AKM_REG_HYL
	 * data[4] = AKM_REG_HYH
	 * data[5] = AKM_REG_HZL
	 * data[6] = AKM_REG_HZH
	 */
	mutex_lock(&inf->mutex_data);
	x = (s16)le16_to_cpup((__le16 *)(&data[1]));
	y = (s16)le16_to_cpup((__le16 *)(&data[3]));
	z = (s16)le16_to_cpup((__le16 *)(&data[5]));
	inf->xyz_raw[AXIS_X] = x;
	inf->xyz_raw[AXIS_Y] = y;
	inf->xyz_raw[AXIS_Z] = z;
	x = (((int)x * (inf->asa.asa[AXIS_X] + 128)) >> 8);
	y = (((int)y * (inf->asa.asa[AXIS_Y] + 128)) >> 8);
	z = (((int)z * (inf->asa.asa[AXIS_Z] + 128)) >> 8);
	inf->xyz[AXIS_X] = x;
	inf->xyz[AXIS_Y] = y;
	inf->xyz[AXIS_Z] = z;
	mutex_unlock(&inf->mutex_data);
}

static s64 akm_timestamp_ns(void)
{
	struct timespec ts;
	s64 ns;

	ktime_get_ts(&ts);
	ns = timespec_to_ns(&ts);
	return ns;
}

static void akm_report(struct akm_inf *inf, u8 *data, s64 ts)
{
	akm_calc(inf, data);
	mutex_lock(&inf->mutex_data);
	if (inf->dbg & AKM_DBG_SPEW_MAGNETIC_FIELD_RAW)
		dev_info(&inf->i2c->dev, "r %d %d %d %lld\n",
			 inf->xyz_raw[AXIS_X], inf->xyz_raw[AXIS_Y],
			 inf->xyz_raw[AXIS_Z], ts);
	if (inf->dbg & AKM_DBG_SPEW_MAGNETIC_FIELD)
		dev_info(&inf->i2c->dev, "%d %d %d %lld\n",
			 inf->xyz[AXIS_X], inf->xyz[AXIS_Y],
			 inf->xyz[AXIS_Z], ts);
	input_report_rel(inf->idev, REL_X, inf->xyz[AXIS_X]);
	input_report_rel(inf->idev, REL_Y, inf->xyz[AXIS_Y]);
	input_report_rel(inf->idev, REL_Z, inf->xyz[AXIS_Z]);
	mutex_unlock(&inf->mutex_data);
	input_report_rel(inf->idev, REL_MISC, (unsigned int)(ts >> 32));
	input_report_rel(inf->idev, REL_WHEEL,
			 (unsigned int)(ts & 0xffffffff));
	input_sync(inf->idev);
}

static int akm_read_sts(struct akm_inf *inf, u8 *data)
{
	int err = 0; /* assume still processing */
	/* data[0] = AKM_REG_ST1
	 * data[7] = AKM_REG_ST2
	 * data[8] = AKM_REG_CNTL1
	 */
	if ((data[0] == AKM_ST1_DRDY) && (!(data[7] &
					   (AKM_ST2_HOFL | AKM_ST2_DERR))))
		err = 1; /* data ready to be reported */
	else if (!(data[8] & AKM_CNTL1_MODE_MASK))
		err = -1; /* something wrong */
	return err;
}

static int akm_read(struct akm_inf *inf)
{
	long long timestamp1;
	long long timestamp2;
	u8 data[9];
	int err;

	timestamp1 = akm_timestamp_ns();
	err = akm_i2c_rd(inf, AKM_REG_ST1, 9, data);
	timestamp2 = akm_timestamp_ns();
	if (err)
		return err;

	err = akm_read_sts(inf, data);
	if (err > 0) {
		timestamp2 = (timestamp2 - timestamp1) / 2;
		timestamp1 += timestamp2;
		akm_report(inf, data, timestamp1);
		if ((inf->data_out & AKM_CNTL1_MODE_MASK) ==
							 AKM_CNTL1_MODE_SINGLE)
			akm_i2c_wr(inf, AKM_REG_CNTL1, inf->data_out);
	} else if (err < 0) {
			dev_err(&inf->i2c->dev, "%s ERR\n", __func__);
			akm_mode_wr(inf, true, inf->range_i,
				    inf->data_out & AKM_CNTL1_MODE_MASK);
	}
	return err;
}

static void akm_mpu_handler(u8 *data, unsigned int len, s64 ts, void *p_val)
{
	struct akm_inf *inf;
	int err;

	inf = (struct akm_inf *)p_val;
	if (inf->enable) {
		err = akm_read_sts(inf, data);
		if (err > 0)
			akm_report(inf, data, ts);
	}
}

static void akm_work(struct work_struct *ws)
{
	struct akm_inf *inf;

	inf = container_of(ws, struct akm_inf, dw.work);
	akm_read(inf);
	queue_delayed_work(inf->wq, &inf->dw,
			   usecs_to_jiffies(inf->poll_delay_us));
}

static void akm_range_limits(struct akm_inf *inf)
{
	if (inf->dev_id == COMPASS_ID_AK8963) {
		if (inf->range_i) {
			inf->asa.range_lo[AXIS_X] = AKM8963_RANGE16_X_LO;
			inf->asa.range_hi[AXIS_X] = AKM8963_RANGE16_X_HI;
			inf->asa.range_lo[AXIS_Y] = AKM8963_RANGE16_Y_LO;
			inf->asa.range_hi[AXIS_Y] = AKM8963_RANGE16_Y_HI;
			inf->asa.range_lo[AXIS_Z] = AKM8963_RANGE16_Z_LO;
			inf->asa.range_hi[AXIS_Z] = AKM8963_RANGE16_Z_HI;
		} else {
			inf->asa.range_lo[AXIS_X] = AKM8963_RANGE14_X_LO;
			inf->asa.range_hi[AXIS_X] = AKM8963_RANGE14_X_HI;
			inf->asa.range_lo[AXIS_Y] = AKM8963_RANGE14_Y_LO;
			inf->asa.range_hi[AXIS_Y] = AKM8963_RANGE14_Y_HI;
			inf->asa.range_lo[AXIS_Z] = AKM8963_RANGE14_Z_LO;
			inf->asa.range_hi[AXIS_Z] = AKM8963_RANGE14_Z_HI;
		}
	} else if (inf->dev_id == COMPASS_ID_AK8972) {
		inf->asa.range_lo[AXIS_X] = AKM8972_RANGE_X_LO;
		inf->asa.range_hi[AXIS_X] = AKM8972_RANGE_X_HI;
		inf->asa.range_lo[AXIS_Y] = AKM8972_RANGE_Y_LO;
		inf->asa.range_hi[AXIS_Y] = AKM8972_RANGE_Y_HI;
		inf->asa.range_lo[AXIS_Z] = AKM8972_RANGE_Z_LO;
		inf->asa.range_hi[AXIS_Z] = AKM8972_RANGE_Z_HI;
	} else { /* default COMPASS_ID_AK8975 */
		inf->dev_id = COMPASS_ID_AK8975;
		inf->asa.range_lo[AXIS_X] = AKM8975_RANGE_X_LO;
		inf->asa.range_hi[AXIS_X] = AKM8975_RANGE_X_HI;
		inf->asa.range_lo[AXIS_Y] = AKM8975_RANGE_Y_LO;
		inf->asa.range_hi[AXIS_Y] = AKM8975_RANGE_Y_HI;
		inf->asa.range_lo[AXIS_Z] = AKM8975_RANGE_Z_LO;
		inf->asa.range_hi[AXIS_Z] = AKM8975_RANGE_Z_HI;
	}
}

static int akm_self_test(struct akm_inf *inf)
{
	u8 data[9];
	u8 mode;
	int i;
	int err;
	int err_t;

	err_t = akm_i2c_wr(inf, AKM_REG_CNTL1, AKM_CNTL1_MODE_POWERDOWN);
	udelay(AKM_HW_DELAY_US);
	err_t |= akm_i2c_wr(inf, AKM_REG_ASTC, AKM_REG_ASTC_SELF);
	udelay(AKM_HW_DELAY_US);
	mode = AKM_CNTL1_MODE_SELFTEST;
	if (inf->range_i)
		mode |= AKM_CNTL1_OUTPUT_BIT16;
	err_t |= akm_i2c_wr(inf, AKM_REG_CNTL1, mode);
	for (i = 0; i < AKM_HW_DELAY_TSM_MS; i++) {
		mdelay(AKM_HW_DELAY_TSM_MS);
		err = akm_i2c_rd(inf, AKM_REG_ST1, 9, data);
		if (!err) {
			err = akm_read_sts(inf, data);
			if (err > 0) {
				akm_calc(inf, data);
				err = 0;
				break;
			}
			err = -EBUSY;
		}
	}
	err_t |= err;
	akm_i2c_wr(inf, AKM_REG_ASTC, 0);
	if (!err_t) {
		akm_range_limits(inf);
		if ((inf->xyz[AXIS_X] < inf->asa.range_lo[AXIS_X]) ||
				(inf->xyz[AXIS_X] > inf->asa.range_hi[AXIS_X]))
			err_t |= 1 << AXIS_X;
		if ((inf->xyz[AXIS_Y] < inf->asa.range_lo[AXIS_Y]) ||
				(inf->xyz[AXIS_Y] > inf->asa.range_hi[AXIS_Y]))
			err_t |= 1 << AXIS_Y;
		if ((inf->xyz[AXIS_Z] < inf->asa.range_lo[AXIS_Z]) ||
				(inf->xyz[AXIS_Z] > inf->asa.range_hi[AXIS_Z]))
			err_t |= 1 << AXIS_Z;
		if (err_t) {
			dev_err(&inf->i2c->dev, "%s ERR: out_of_range %x\n",
				__func__, err_t);

		}
	}
	return err_t;
}

static int akm_init_hw(struct akm_inf *inf)
{
	u8 val[8];
	int err;
	int err_t;

	err_t = akm_nvi_mpu_bypass_request(inf);
	if (!err_t) {
		err_t = akm_wr(inf, AKM_REG_CNTL1, AKM_CNTL1_MODE_ROM_ACCESS);
		udelay(AKM_HW_DELAY_ROM_ACCESS_US);
		err_t |= akm_i2c_rd(inf, AKM_REG_ASAX, 3, inf->asa.asa);
		/* we can autodetect AK8963 with BITM */
		inf->dev_id = 0;
		err = akm_wr(inf, AKM_REG_CNTL1, (AKM_CNTL1_MODE_SINGLE |
						  AKM_CNTL1_OUTPUT_BIT16));
		if (!err) {
			mdelay(AKM_HW_DELAY_TSM_MS);
			err = akm_i2c_rd(inf, AKM_REG_ST2, 1, val);
			if ((!err) && (val[0] & AKM_CNTL1_OUTPUT_BIT16)) {
				inf->dev_id = COMPASS_ID_AK8963;
				inf->range_i = 1;
			}
		}
		if (!inf->dev_id)
			inf->dev_id = inf->pdata.sec_slave_id;
		err = akm_self_test(inf);
		if (err < 0)
			err_t |= err;
		akm_nvi_mpu_bypass_release(inf);
	}
	if (!err_t)
		inf->initd = true;
	else
		dev_err(&inf->i2c->dev, "%s ERR %d\n", __func__, err_t);
	return err_t;
}

static int akm_enable(struct akm_inf *inf, bool enable)
{
	int err = 0;

	if (enable) {
		akm_pm(inf, true);
		if (!inf->initd)
			err = akm_init_hw(inf);
		if (!err) {
			inf->data_out = AKM_CNTL1_MODE_SINGLE;
			err |= akm_delay(inf, inf->poll_delay_us);
			err |= akm_mode_wr(inf, true, inf->range_i,
					  inf->data_out & AKM_CNTL1_MODE_MASK);
			if (!err)
				inf->enable = true;
		}
	} else if (inf->enable) {
		err = akm_mode_wr(inf, false, inf->range_i,
				  AKM_CNTL1_MODE_POWERDOWN);
		if (!err) {
			akm_pm(inf, false);
			inf->enable = false;
		}
	}
	return err;
}

static ssize_t akm_enable_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct akm_inf *inf;
	unsigned int enable;
	bool en;
	int err;

	inf = dev_get_drvdata(dev);
	err = kstrtouint(buf, 10, &enable);
	if (err)
		return -EINVAL;

	if (enable)
		en = true;
	else
		en = false;
	if (inf->dbg & AKM_DBG_SPEW_MSG)
		dev_info(&inf->i2c->dev, "%s: %x\n", __func__, en);
	err = akm_enable(inf, en);
	if (err) {
		dev_err(&inf->i2c->dev, "%s: %x ERR=%d\n", __func__, en, err);
		return err;
	}

	return count;
}

static ssize_t akm_enable_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct akm_inf *inf;
	unsigned int enable;

	inf = dev_get_drvdata(dev);
	if (inf->enable)
		enable = 1;
	else
		enable = 0;
	return sprintf(buf, "%u\n", enable);
}

static ssize_t akm_delay_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct akm_inf *inf;
	unsigned int delay_us;
	int err;

	inf = dev_get_drvdata(dev);
	err = kstrtouint(buf, 10, &delay_us);
	if (err)
		return -EINVAL;

	if (delay_us < (AKM_INPUT_DELAY_MS_MIN * 1000))
		delay_us = (AKM_INPUT_DELAY_MS_MIN * 1000);
	if ((inf->enable) && (delay_us != inf->poll_delay_us))
		err = akm_delay(inf, delay_us);
	if (!err) {
		if (inf->dbg & AKM_DBG_SPEW_MSG)
			dev_info(&inf->i2c->dev, "%s: %u\n",
				 __func__, delay_us);
		inf->poll_delay_us = delay_us;
	} else {
		dev_err(&inf->i2c->dev, "%s: %u ERR=%d\n",
			__func__, delay_us, err);
		return err;
	}

	return count;
}

static ssize_t akm_delay_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct akm_inf *inf;

	inf = dev_get_drvdata(dev);
	if (inf->enable)
		return sprintf(buf, "%u\n", inf->poll_delay_us);

	return sprintf(buf, "%u\n", (AKM_INPUT_DELAY_MS_MIN * 1000));
}

static ssize_t akm_resolution_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct akm_inf *inf;
	unsigned int resolution;

	inf = dev_get_drvdata(dev);
	if (kstrtouint(buf, 10, &resolution))
		return -EINVAL;

	dev_dbg(&inf->i2c->dev, "%s %u\n", __func__, resolution);
	inf->resolution = resolution;
	return count;
}

static ssize_t akm_resolution_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct akm_inf *inf;
	unsigned int resolution;

	inf = dev_get_drvdata(dev);
	if (inf->enable)
		resolution = inf->resolution;
	else
		resolution = AKM_INPUT_RESOLUTION;
	return sprintf(buf, "%u\n", resolution);
}

static ssize_t akm_max_range_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct akm_inf *inf;
	unsigned int range_i;
	int err;

	inf = dev_get_drvdata(dev);
	err = kstrtouint(buf, 10, &range_i);
	if (err)
		return -EINVAL;

	dev_dbg(&inf->i2c->dev, "%s %u\n", __func__, range_i);
	if (inf->dev_id == COMPASS_ID_AK8963) {
		if (range_i > 1)
			return -EINVAL;

		if (inf->enable) {
			err = akm_mode_wr(inf, false, range_i,
					  inf->data_out & AKM_CNTL1_MODE_MASK);
			if (err)
				return err;
		} else {
			inf->range_i = range_i;
		}
	} else {
		return -EINVAL;
	}

	return count;
}

static ssize_t akm_max_range_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct akm_inf *inf;
	unsigned int range;

	inf = dev_get_drvdata(dev);
	if (inf->enable) {
		range = inf->range_i;
	} else {
		if (inf->dev_id == COMPASS_ID_AK8963) {
			if (inf->range_i)
				range = AKM8963_SCALE16;
			else
				range = AKM8963_SCALE14;
		} else if (inf->dev_id == COMPASS_ID_AK8972) {
			range = AKM8972_SCALE;
		} else {
			range = AKM8975_SCALE;
		}
	}
	return sprintf(buf, "%u\n", range);
}

static ssize_t akm_data_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct akm_inf *inf;
	unsigned int data_info;
	int err;

	inf = dev_get_drvdata(dev);
	err = kstrtouint(buf, 10, &data_info);
	if (err)
		return -EINVAL;

	if (data_info >= AKM_DATA_INFO_LIMIT_MAX)
		return -EINVAL;

	dev_dbg(&inf->i2c->dev, "%s %u\n", __func__, data_info);
	inf->data_info = data_info;
	switch (data_info) {
	case AKM_DATA_INFO_DATA:
		inf->dbg = 0;
		break;

	case AKM_DATA_INFO_DBG:
		inf->dbg ^= AKM_DBG_SPEW_MSG;
		break;

	case AKM_DATA_INFO_MAGNETIC_FIELD_SPEW:
		inf->dbg ^= AKM_DBG_SPEW_MAGNETIC_FIELD;
		break;

	case AKM_DATA_INFO_MAGNETIC_FIELD_RAW_SPEW:
		inf->dbg ^= AKM_DBG_SPEW_MAGNETIC_FIELD_RAW;
		break;

	default:
		break;
	}

	return count;
}

static ssize_t akm_data_show(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	struct akm_inf *inf;
	ssize_t t;
	u8 data[16];
	enum AKM_DATA_INFO data_info;
	bool enable;
	unsigned int i;
	int err = 0;

	inf = dev_get_drvdata(dev);
	data_info = inf->data_info;
	inf->data_info = AKM_DATA_INFO_DATA;
	enable = inf->enable;
	switch (data_info) {
	case AKM_DATA_INFO_DATA:
		mutex_lock(&inf->mutex_data);
		t = sprintf(buf, "magnetic_field: %hd, %hd, %hd   ",
			    inf->xyz[AXIS_X],
			    inf->xyz[AXIS_Y],
			    inf->xyz[AXIS_Z]);
		t += sprintf(buf + t, "raw: %hd, %hd, %hd\n",
			     inf->xyz_raw[AXIS_X],
			     inf->xyz_raw[AXIS_Y],
			     inf->xyz_raw[AXIS_Z]);
		mutex_unlock(&inf->mutex_data);
		return t;

	case AKM_DATA_INFO_VER:
		return sprintf(buf, "version=%u\n", AKM_VERSION);

	case AKM_DATA_INFO_RESET:
		akm_pm(inf, true);
		err = akm_mode_wr(inf, true, inf->range_i,
				  inf->data_out & AKM_CNTL1_MODE_MASK);
		akm_enable(inf, enable);
		if (err)
			return sprintf(buf, "reset ERR %d\n", err);
		else
			return sprintf(buf, "reset done\n");

	case AKM_DATA_INFO_REGS:
		if (!inf->initd)
			t = sprintf(buf, "calibration: NEED ENABLE\n");
		else
			t = sprintf(buf, "calibration: x=%#2x y=%#2x z=%#2x\n",
				    inf->asa.asa[AXIS_X],
				    inf->asa.asa[AXIS_Y],
				    inf->asa.asa[AXIS_Z]);
		err = akm_nvi_mpu_bypass_request(inf);
		if (!err) {
			err = akm_i2c_rd(inf, AKM_REG_WIA, AKM_REG_ASAX, data);
			akm_nvi_mpu_bypass_release(inf);
		}
		if (err) {
			t += sprintf(buf + t, "registers: ERR %d\n", err);
		} else {
			t += sprintf(buf + t, "registers:\n");
			for (i = 0; i < AKM_REG_ASAX; i++)
				t += sprintf(buf + t, "%#2x=%#2x\n",
					     AKM_REG_WIA + i, data[i]);
		}
		return t;

	case AKM_DATA_INFO_DBG:
		return sprintf(buf, "debug spew=%x\n",
			       inf->dbg & AKM_DBG_SPEW_MSG);

	case AKM_DATA_INFO_MAGNETIC_FIELD_SPEW:
		return sprintf(buf, "xyz_ts spew=%x\n",
			       !!(inf->dbg & AKM_DBG_SPEW_MAGNETIC_FIELD));

	case AKM_DATA_INFO_MAGNETIC_FIELD_RAW_SPEW:
		return sprintf(buf, "xyz_raw_ts spew=%x\n",
			       !!(inf->dbg & AKM_DBG_SPEW_MAGNETIC_FIELD_RAW));

	default:
		break;
	}

	return -EINVAL;
}

static ssize_t akm_mpu_fifo_enable_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	struct akm_inf *inf;
	unsigned int fifo_enable;
	int err;

	inf = dev_get_drvdata(dev);
	err = kstrtouint(buf, 10, &fifo_enable);
	if (err)
		return -EINVAL;

	if (!inf->use_mpu)
		return -EINVAL;

	if (fifo_enable)
		inf->fifo_enable = true;
	else
		inf->fifo_enable = false;
	dev_dbg(&inf->i2c->dev, "%s %x\n", __func__, inf->fifo_enable);
	return count;
}

static ssize_t akm_mpu_fifo_enable_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct akm_inf *inf = dev_get_drvdata(dev);

	return sprintf(buf, "%x\n", inf->fifo_enable & inf->use_mpu);
}

static ssize_t akm_divisor_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct akm_inf *inf;

	inf = dev_get_drvdata(dev);
	return sprintf(buf, "%u\n", AKM_INPUT_DIVISOR);
}

static ssize_t akm_microamp_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct akm_inf *inf;

	inf = dev_get_drvdata(dev);
	return sprintf(buf, "%u\n", AKM_INPUT_POWER_UA);
}

static ssize_t akm_self_test_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct akm_inf *inf;
	ssize_t t;
	bool enable;
	int err;

	inf = dev_get_drvdata(dev);
	enable = inf->enable;
	akm_enable(inf, false);
	akm_pm(inf, true);
	if (!inf->initd) {
		err = akm_init_hw(inf);
	} else {
		err = akm_nvi_mpu_bypass_request(inf);
		if (!err) {
			err = akm_self_test(inf);
			akm_nvi_mpu_bypass_release(inf);
		}
	}
	if (err < 0) {
		t = sprintf(buf, "ERR: %d\n", err);
	} else {
		t = sprintf(buf, "%d   xyz: %hd %hd %hd   raw: %hd %hd %hd   ",
			    err,
			    inf->xyz[AXIS_X],
			    inf->xyz[AXIS_Y],
			    inf->xyz[AXIS_Z],
			    inf->xyz_raw[AXIS_X],
			    inf->xyz_raw[AXIS_Y],
			    inf->xyz_raw[AXIS_Z]);
		if (err > 0) {
			if (err & (1 << AXIS_X))
				t += sprintf(buf + t, "X ");
			if (err & (1 << AXIS_Y))
				t += sprintf(buf + t, "Y ");
			if (err & (1 << AXIS_Z))
				t += sprintf(buf + t, "Z ");
			t += sprintf(buf + t, "FAILED\n");
		} else {
			t += sprintf(buf + t, "PASS\n");
		}
	}
	akm_enable(inf, enable);
	return t;
}

static ssize_t akm_orientation_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	struct akm_inf *inf;
	signed char *m;

	inf = dev_get_drvdata(dev);
	m = inf->pdata.orientation;
	return sprintf(buf, "%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
		       m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8]);
}

static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
		   akm_enable_show, akm_enable_store);
static DEVICE_ATTR(delay, S_IRUGO | S_IWUSR | S_IWGRP,
		   akm_delay_show, akm_delay_store);
static DEVICE_ATTR(resolution, S_IRUGO | S_IWUSR | S_IWGRP,
		   akm_resolution_show, akm_resolution_store);
static DEVICE_ATTR(max_range, S_IRUGO | S_IWUSR | S_IWGRP,
		   akm_max_range_show, akm_max_range_store);
static DEVICE_ATTR(data, S_IRUGO | S_IWUSR | S_IWGRP,
		   akm_data_show, akm_data_store);
static DEVICE_ATTR(mpu_fifo_en, S_IRUGO | S_IWUSR | S_IWGRP,
		   akm_mpu_fifo_enable_show, akm_mpu_fifo_enable_store);
static DEVICE_ATTR(divisor, S_IRUGO,
		   akm_divisor_show, NULL);
static DEVICE_ATTR(microamp, S_IRUGO,
		   akm_microamp_show, NULL);
static DEVICE_ATTR(self_test, S_IRUGO,
		   akm_self_test_show, NULL);
static DEVICE_ATTR(orientation, S_IRUGO,
		   akm_orientation_show, NULL);

static struct attribute *akm_attrs[] = {
	&dev_attr_enable.attr,
	&dev_attr_delay.attr,
	&dev_attr_resolution.attr,
	&dev_attr_max_range.attr,
	&dev_attr_divisor.attr,
	&dev_attr_microamp.attr,
	&dev_attr_data.attr,
	&dev_attr_self_test.attr,
	&dev_attr_orientation.attr,
	&dev_attr_mpu_fifo_en.attr,
	NULL
};

static struct attribute_group akm_attr_group = {
	.name = AKM_NAME,
	.attrs = akm_attrs
};

static int akm_sysfs_create(struct akm_inf *inf)
{
	int err;

	err = sysfs_create_group(&inf->idev->dev.kobj, &akm_attr_group);
	if (err) {
		dev_err(&inf->i2c->dev, "%s ERR %d\n", __func__, err);
		return err;
	}
	err = nvi_mpu_sysfs_register(&inf->idev->dev.kobj, AKM_NAME);
	if (err)
		dev_err(&inf->i2c->dev, "%s ERR %d\n", __func__, err);
	return err;
}

static void akm_input_close(struct input_dev *idev)
{
	struct akm_inf *inf;

	inf = input_get_drvdata(idev);
	if (inf != NULL)
		akm_enable(inf, false);
}

static int akm_input_create(struct akm_inf *inf)
{
	int err;

	inf->idev = input_allocate_device();
	if (!inf->idev) {
		err = -ENOMEM;
		dev_err(&inf->i2c->dev, "%s ERR %d\n", __func__, err);
		return err;
	}

	inf->idev->name = AKM_NAME;
	inf->idev->dev.parent = &inf->i2c->dev;
	inf->idev->close = akm_input_close;
	input_set_drvdata(inf->idev, inf);
	input_set_capability(inf->idev, EV_REL, REL_X);
	input_set_capability(inf->idev, EV_REL, REL_Y);
	input_set_capability(inf->idev, EV_REL, REL_Z);
	input_set_capability(inf->idev, EV_REL, REL_MISC);
	input_set_capability(inf->idev, EV_REL, REL_WHEEL);
	err = input_register_device(inf->idev);
	if (err) {
		input_free_device(inf->idev);
		inf->idev = NULL;
	}
	return err;
}

static int akm_id(struct akm_inf *inf)
{
	struct nvi_mpu_port nmp;
	u8 config_boot;
	u8 val = 0;
	int err;

	config_boot = inf->pdata.config & NVI_CONFIG_BOOT_MASK;
	if (config_boot == NVI_CONFIG_BOOT_AUTO) {
		nmp.addr = inf->i2c->addr | 0x80;
		nmp.reg = AKM_REG_WIA;
		nmp.ctrl = 1;
		err = nvi_mpu_dev_valid(&nmp, &val);
		/* see mpu.h for possible return values */
		dev_dbg(&inf->i2c->dev, "%s AUTO ID=%x err=%d\n",
			__func__, val, err);
		if ((err == -EAGAIN) || (err == -EBUSY))
			return -EAGAIN;

		if (((!err) && (val == AKM_WIA_ID)) || (err == -EIO))
			config_boot = NVI_CONFIG_BOOT_MPU;
	}
	if (config_boot == NVI_CONFIG_BOOT_MPU) {
		inf->use_mpu = true;
		nmp.addr = inf->i2c->addr | 0x80;
		nmp.reg = AKM_REG_ST1;
		nmp.ctrl = 10; /* MPU FIFO can't handle odd size */
		nmp.data_out = 0;
		nmp.delay_ms = 0;
		nmp.delay_us = inf->poll_delay_us;
		nmp.shutdown_bypass = false;
		nmp.handler = &akm_mpu_handler;
		nmp.ext_driver = (void *)inf;
		err = nvi_mpu_port_alloc(&nmp);
		dev_dbg(&inf->i2c->dev, "%s MPU port/err=%d\n",
			__func__, err);
		if (err < 0)
			return err;

		inf->port_id[RD] = err;
		nmp.addr = inf->i2c->addr;
		nmp.reg = AKM_REG_CNTL1;
		nmp.ctrl = 1;
		nmp.data_out = inf->data_out;
		nmp.delay_ms = AKM_HW_DELAY_TSM_MS;
		nmp.delay_us = 0;
		nmp.shutdown_bypass = false;
		nmp.handler = NULL;
		nmp.ext_driver = NULL;
		err = nvi_mpu_port_alloc(&nmp);
		dev_dbg(&inf->i2c->dev, "%s MPU port/err=%d\n",
			__func__, err);
		if (err < 0) {
			akm_ports_free(inf);
		} else {
			inf->port_id[WR] = err;
			err = 0;
		}
		return err;
	}

	/* NVI_CONFIG_BOOT_HOST */
	inf->use_mpu = false;
	err = akm_i2c_rd(inf, AKM_REG_WIA, 1, &val);
	dev_dbg(&inf->i2c->dev, "%s Host read ID=%x err=%d\n",
		__func__, val, err);
	if ((!err) && (val == AKM_WIA_ID))
		return 0;

	return -ENODEV;
}

static int akm_remove(struct i2c_client *client)
{
	struct akm_inf *inf;

	inf = i2c_get_clientdata(client);
	if (inf != NULL) {
		if (inf->idev)
			input_unregister_device(inf->idev);
		if (inf->wq)
			destroy_workqueue(inf->wq);
		akm_pm_exit(inf);
		if (&inf->mutex_data)
			mutex_destroy(&inf->mutex_data);
	}
	dev_info(&client->dev, "%s\n", __func__);
	return 0;
}

static void akm_shutdown(struct i2c_client *client)
{
	akm_remove(client);
}

static struct mpu_platform_data *akm_parse_dt(struct i2c_client *client)
{
	struct mpu_platform_data *pdata;
	struct device_node *np = client->dev.of_node;
	char const *pchar;
	u8 config;
	int len;

	pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(&client->dev, "Can't allocate platform data\n");
		return ERR_PTR(-ENOMEM);
	}

	pchar = of_get_property(np, "orientation", &len);
	if (!pchar || len != sizeof(pdata->orientation)) {
		dev_err(&client->dev, "Cannot read orientation property\n");
		return ERR_PTR(-EINVAL);
	}

	memcpy(pdata->orientation, pchar, len);
	if (of_property_read_string(np, "config", &pchar)) {
		dev_err(&client->dev, "Cannot read config property\n");
		return ERR_PTR(-EINVAL);
	}

	for (config = 0; config < ARRAY_SIZE(akm_configs); config++) {
		if (!strcasecmp(pchar, akm_configs[config])) {
			pdata->config = config;
			break;
		}
	}
	if (config == ARRAY_SIZE(akm_configs)) {
		dev_err(&client->dev, "Invalid config value\n");
		return ERR_PTR(-EINVAL);
	}

	return pdata;
}

static int akm_probe(struct i2c_client *client,
		     const struct i2c_device_id *devid)
{
	struct akm_inf *inf;
	struct mpu_platform_data *pd;
	int err;

	dev_info(&client->dev, "%s\n", __func__);
	inf = devm_kzalloc(&client->dev, sizeof(*inf), GFP_KERNEL);
	if (!inf) {
		dev_err(&client->dev, "%s kzalloc ERR\n", __func__);
		return -ENOMEM;
	}

	inf->i2c = client;
	i2c_set_clientdata(client, inf);
	if (client->dev.of_node) {
		pd = akm_parse_dt(client);
		if (IS_ERR(pd))
			return -EINVAL;
	} else {
		pd = (struct mpu_platform_data *)dev_get_platdata(&client->dev);
		if (!pd)
			return -EINVAL;
	}

	inf->pdata = *pd;
	akm_pm_init(inf);
	err = akm_id(inf);
	akm_pm(inf, false);
	if (err == -EAGAIN)
		goto akm_probe_again;
	else if (err)
		goto akm_probe_err;

	mutex_init(&inf->mutex_data);
	err = akm_input_create(inf);
	if (err)
		goto akm_probe_err;

	inf->wq = create_singlethread_workqueue(AKM_NAME);
	if (!inf->wq) {
		dev_err(&client->dev, "%s workqueue ERR\n", __func__);
		err = -ENOMEM;
		goto akm_probe_err;
	}

	INIT_DELAYED_WORK(&inf->dw, akm_work);
	err = akm_sysfs_create(inf);
	if (err)
		goto akm_probe_err;

	return 0;

akm_probe_err:
	dev_err(&client->dev, "%s ERR %d\n", __func__, err);
akm_probe_again:
	akm_remove(client);
	return err;
}

static const struct i2c_device_id akm_i2c_device_id[] = {
	{AKM_NAME, 0},
	{"ak8963", 0},
	{"ak8972", 0},
	{"ak8975", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, akm_i2c_device_id);

static const struct of_device_id akm_of_match[] = {
	{ .compatible = "ak,ak89xx", },
	{ .compatible = "ak,ak8963", },
	{ .compatible = "ak,ak8972", },
	{ .compatible = "ak,ak8975", },
	{ },
};

MODULE_DEVICE_TABLE(of, akm_of_match);

static struct i2c_driver akm_driver = {
	.class		= I2C_CLASS_HWMON,
	.probe		= akm_probe,
	.remove		= akm_remove,
	.driver = {
		.name		= AKM_NAME,
		.owner		= THIS_MODULE,
		.of_match_table = of_match_ptr(akm_of_match),
	},
	.id_table	= akm_i2c_device_id,
	.shutdown	= akm_shutdown,
};

static int __init akm_init(void)
{
	return i2c_add_driver(&akm_driver);
}

static void __exit akm_exit(void)
{
	i2c_del_driver(&akm_driver);
}

module_init(akm_init);
module_exit(akm_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("AKM driver");

