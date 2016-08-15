/* Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
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


#define BMP_VERSION			(23)
#define BMP180_RANGE_DFLT		(0)
/* until OSS is supported in the pressure calculation, this defaults to 5 */
#define BMP280_RANGE_DFLT		(5)

#define BMPX80_NAME			"bmpX80"
#define BMP180_NAME			"bmp180"
#define BMP280_NAME			"bmp280"
#define BMP280_I2C_ADDR0		(0x76)
#define BMPX80_I2C_ADDR1		(0x77)
#define BMPX80_HW_DELAY_POR_MS		(10)
#define BMPX80_POLL_DELAY_MS_DFLT	(200)
#define BMPX80_MPU_RETRY_COUNT		(50)
#define BMPX80_MPU_RETRY_DELAY_MS	(20)
/* sampling delays */
#define BMP180_DELAY_MS_ULP		(5)
#define BMP180_DELAY_MS_ST		(8)
#define BMP180_DELAY_MS_HIGH_RES	(14)
#define BMP180_DELAY_MS_UHIGH_RES	(26)
#define BMP280_DELAY_MS_ULP		(9)
#define BMP280_DELAY_MS_LP		(12)
#define BMP280_DELAY_MS_ST		(18)
#define BMP280_DELAY_MS_HIGH_RES	(30)
#define BMP280_DELAY_MS_UHIGH_RES	(57)
/* input poll values*/
#define BMP180_INPUT_RESOLUTION		(1)
#define BMP180_INPUT_DIVISOR		(100)
#define BMP280_INPUT_DIVISOR		(100)
#define BMP180_INPUT_DELAY_MS_MIN	(BMP180_DELAY_MS_UHIGH_RES)
#define BMP180_INPUT_POWER_UA		(12)
#define BMP180_PRESSURE_MIN		(30000)
#define BMP180_PRESSURE_MAX		(110000)
#define BMP180_PRESSURE_FUZZ		(5)
#define BMP180_PRESSURE_FLAT		(5)
/* BMPX80 registers */
#define BMPX80_REG_ID			(0xD0)
#define BMPX80_REG_ID_BMP180		(0x55)
#define BMPX80_REG_ID_BMP280		(0x56)
#define BMPX80_REG_RESET		(0xE0)
#define BMPX80_REG_RESET_VAL		(0xB6)
#define BMPX80_REG_CTRL			(0xF4)
#define BMP180_REG_CTRL_MODE_MASK	(0x1F)
#define BMP180_REG_CTRL_MODE_PRES	(0x34)
#define BMP180_REG_CTRL_MODE_TEMP	(0x2E)
#define BMP180_REG_CTRL_SCO		(5)
#define BMP180_REG_CTRL_OSS		(6)
#define BMP280_REG_CTRL_MODE_MASK	(0x03)
#define BMP280_REG_CTRL_MODE_SLEEP	(0)
#define BMP280_REG_CTRL_MODE_FORCED1	(1)
#define BMP280_REG_CTRL_MODE_FORCED2	(2)
#define BMP280_REG_CTRL_MODE_NORMAL	(3)
#define BMP280_REG_CTRL_OSRS_P		(2)
#define BMP280_REG_CTRL_OSRS_P_MASK	(0x1C)
#define BMP280_REG_CTRL_OSRS_T		(5)
#define BMP280_REG_CTRL_OSRS_T_MASK	(0xE0)
#define BMP180_REG_OUT_MSB		(0xF6)
#define BMP180_REG_OUT_LSB		(0xF7)
#define BMP180_REG_OUT_XLSB		(0xF8)
#define BMP280_REG_STATUS		(0xF3)
#define BMP280_REG_STATUS_MEASURING	(3)
#define BMP280_REG_STATUS_IM_UPDATE	(0)
#define BMP280_REG_CONFIG		(0xF5)
#define BMP280_REG_PRESS_MSB		(0xF7)
#define BMP280_REG_PRESS_LSB		(0xF8)
#define BMP280_REG_PRESS_XLSB		(0xF9)
#define BMP280_REG_TEMP_MSB		(0xFA)
#define BMP280_REG_TEMP_LSB		(0xFB)
#define BMP280_REG_TEMP_XLSB		(0xFC)

/* ROM registers */
#define BMP180_REG_AC1			(0xAA)
#define BMP180_REG_AC2			(0xAC)
#define BMP180_REG_AC3			(0xAE)
#define BMP180_REG_AC4			(0xB0)
#define BMP180_REG_AC5			(0xB2)
#define BMP180_REG_AC6			(0xB4)
#define BMP180_REG_B1			(0xB6)
#define BMP180_REG_B2			(0xB8)
#define BMP180_REG_MB			(0xBA)
#define BMP180_REG_MC			(0xBC)
#define BMP180_REG_MD			(0xBE)
#define BMP280_REG_CWORD00		(0x88)
#define BMP280_REG_CWORD01		(0x8A)
#define BMP280_REG_CWORD02		(0x8C)
#define BMP280_REG_CWORD03		(0x8E)
#define BMP280_REG_CWORD04		(0x90)
#define BMP280_REG_CWORD05		(0x92)
#define BMP280_REG_CWORD06		(0x94)
#define BMP280_REG_CWORD07		(0x96)
#define BMP280_REG_CWORD08		(0x98)
#define BMP280_REG_CWORD09		(0x9A)
#define BMP280_REG_CWORD10		(0x9C)
#define BMP280_REG_CWORD11		(0x9E)
#define BMP280_REG_CWORD12		(0xA0)

#define WR				(0)
#define RD				(1)

#define BMP_DBG_SPEW_MSG		(1 << 0)
#define BMP_DBG_SPEW_PRESSURE		(1 << 1)
#define BMP_DBG_SPEW_TEMPERATURE	(1 << 2)
#define BMP_DBG_SPEW_PRESSURE_RAW	(1 << 3)
#define BMP_DBG_SPEW_TEMPERATURE_RAW	(1 << 4)

enum BMP_DATA_INFO {
	BMP_DATA_INFO_DATA = 0,
	BMP_DATA_INFO_VER,
	BMP_DATA_INFO_RESET,
	BMP_DATA_INFO_REGS,
	BMP_DATA_INFO_DBG,
	BMP_DATA_INFO_PRESSURE_SPEW,
	BMP_DATA_INFO_TEMPERATURE_SPEW,
	BMP_DATA_INFO_PRESSURE_RAW_SPEW,
	BMP_DATA_INFO_TEMPERATURE_RAW_SPEW,
	BMP_DATA_INFO_LIMIT_MAX,
};

static u8 bmp_ids[] = {
	BMPX80_REG_ID_BMP180,
	BMPX80_REG_ID_BMP280,
	0x57,
	0x58,
};

/* regulator names in order of powering on */
static char *bmp_vregs[] = {
	"vdd",
	"vddio",
};

static char *bmp_configs[] = {
	"auto",
	"mpu",
	"host",
};

static unsigned long bmp180_delay_ms_tbl[] = {
	BMP180_DELAY_MS_ULP,
	BMP180_DELAY_MS_ST,
	BMP180_DELAY_MS_HIGH_RES,
	BMP180_DELAY_MS_UHIGH_RES,
};

static unsigned long bmp280_delay_ms_tbl[] = {
	BMP280_DELAY_MS_ULP,
	BMP280_DELAY_MS_LP,
	BMP280_DELAY_MS_ST,
	BMP280_DELAY_MS_HIGH_RES,
	BMP280_DELAY_MS_UHIGH_RES,
};

union bmp_rom {
	struct bmp180_rom {
		s16 ac1;
		s16 ac2;
		s16 ac3;
		u16 ac4;
		u16 ac5;
		u16 ac6;
		s16 b1;
		s16 b2;
		s16 mb;
		s16 mc;
		s16 md;
	} bmp180;
	struct bmp280_rom {
		u16 dig_T1;
		s16 dig_T2;
		s16 dig_T3;
		u16 dig_P1;
		s16 dig_P2;
		s16 dig_P3;
		s16 dig_P4;
		s16 dig_P5;
		s16 dig_P6;
		s16 dig_P7;
		s16 dig_P8;
		s16 dig_P9;
		s16 reserved;
	} bmp280;
} rom;

struct bmp_inf {
	struct i2c_client *i2c;
	struct input_dev *idev;
	struct workqueue_struct *wq;
	struct delayed_work dw;
	struct regulator_bulk_data vreg[ARRAY_SIZE(bmp_vregs)];
	struct mpu_platform_data pdata;
	struct bmp_hal *hal;		/* Hardware Abstaction Layer */
	union bmp_rom rom;		/* calibration data */
	unsigned int poll_delay_us;	/* requested sampling delay (us) */
	unsigned int range_user;	/* user oversampling value */
	unsigned int range_i;		/* oversampling value */
	unsigned int resolution;	/* report when new data outside this */
	unsigned int data_info;		/* data info to return */
	unsigned int dbg;		/* debug switches */
	unsigned int dev_id;		/* device ID */
	bool use_mpu;			/* if device behind MPU */
	bool initd;			/* set if initialized */
	bool enable;			/* requested enable value */
	bool fifo_enable;		/* MPU FIFO enable */
	bool report;			/* used to report first valid sample */
	bool port_en[2];		/* enable status of MPU write port */
	int port_id[2];			/* MPU port ID */
	u8 data_out;			/* write value to mode register */
	s32 UT;				/* uncompensated temperature */
	s32 UP;				/* uncompensated pressure */
	s32 t_fine;			/* temperature used in pressure calc */
	s32 temperature;		/* true temperature */
	int pressure;			/* true pressure hPa/100 Pa/1 mBar */
};

struct bmp_hal {
	u8 rom_addr_start;
	u8 rom_size;
	unsigned long *bmp_delay_ms_tbl;
	unsigned int divisor;
	unsigned int range_limit;
	unsigned int range_dflt;
	int (*bmp_read)(struct bmp_inf *inf);
	int (*bmp_mode_wr_mpu)(struct bmp_inf *inf, u8 mode);
};


static int bmp_i2c_rd(struct bmp_inf *inf, u8 reg, u16 len, u8 *val)
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

static int bmp_i2c_wr(struct bmp_inf *inf, u8 reg, u8 val)
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

static int bmp_vreg_dis(struct bmp_inf *inf, int i)
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

static int bmp_vreg_dis_all(struct bmp_inf *inf)
{
	unsigned int i;
	int err = 0;

	for (i = ARRAY_SIZE(bmp_vregs); i > 0; i--)
		err |= bmp_vreg_dis(inf, (i - 1));
	return err;
}

static int bmp_vreg_en(struct bmp_inf *inf, int i)
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

static int bmp_vreg_en_all(struct bmp_inf *inf)
{
	int i;
	int err = 0;

	for (i = 0; i < ARRAY_SIZE(bmp_vregs); i++)
		err |= bmp_vreg_en(inf, i);
	return err;
}

static void bmp_vreg_exit(struct bmp_inf *inf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(bmp_vregs); i++) {
		if (inf->vreg[i].consumer != NULL) {
			devm_regulator_put(inf->vreg[i].consumer);
			inf->vreg[i].consumer = NULL;
			dev_dbg(&inf->i2c->dev, "%s %s\n",
				__func__, inf->vreg[i].supply);
		}
	}
}

static int bmp_vreg_init(struct bmp_inf *inf)
{
	unsigned int i;
	int err = 0;

	for (i = 0; i < ARRAY_SIZE(bmp_vregs); i++) {
		inf->vreg[i].supply = bmp_vregs[i];
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

static int bmp_pm(struct bmp_inf *inf, bool enable)
{
	int err;

	if (enable) {
		err = bmp_vreg_en_all(inf);
		if (err)
			mdelay(BMPX80_HW_DELAY_POR_MS);
	} else {
		err = bmp_vreg_dis_all(inf);
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

static int bmp_port_free(struct bmp_inf *inf, int port)
{
	int err = 0;

	if ((inf->use_mpu) && (inf->port_id[port] >= 0)) {
		err = nvi_mpu_port_free(inf->port_id[port]);
		if (!err)
			inf->port_id[port] = -1;
	}
	return err;
}

static int bmp_ports_free(struct bmp_inf *inf)
{
	int err;

	err = bmp_port_free(inf, WR);
	err |= bmp_port_free(inf, RD);
	return err;
}

static void bmp_pm_exit(struct bmp_inf *inf)
{
	bmp_ports_free(inf);
	bmp_pm(inf, false);
	bmp_vreg_exit(inf);
}

static int bmp_pm_init(struct bmp_inf *inf)
{
	int err;

	inf->initd = false;
	inf->enable = false;
	inf->fifo_enable = false; /* DON'T ENABLE: MPU FIFO HW BROKEN */
	inf->port_en[WR] = false;
	inf->port_en[RD] = false;
	inf->port_id[WR] = -1;
	inf->port_id[RD] = -1;
	inf->resolution = 0;
	inf->poll_delay_us = (BMPX80_POLL_DELAY_MS_DFLT * 1000);
	bmp_vreg_init(inf);
	err = bmp_pm(inf, true);
	return err;
}

static int bmp_nvi_mpu_bypass_request(struct bmp_inf *inf)
{
	int i;
	int err = 0;

	if (inf->use_mpu) {
		for (i = 0; i < BMPX80_MPU_RETRY_COUNT; i++) {
			err = nvi_mpu_bypass_request(true);
			if ((!err) || (err == -EPERM))
				break;

			msleep(BMPX80_MPU_RETRY_DELAY_MS);
		}
		if (err == -EPERM)
			err = 0;
	}
	return err;
}

static int bmp_nvi_mpu_bypass_release(struct bmp_inf *inf)
{
	int err = 0;

	if (inf->use_mpu)
		err = nvi_mpu_bypass_release();
	return err;
}

static int bmp_wr(struct bmp_inf *inf, u8 reg, u8 val)
{
	int err = 0;

	err = bmp_nvi_mpu_bypass_request(inf);
	if (!err) {
		err = bmp_i2c_wr(inf, reg, val);
		bmp_nvi_mpu_bypass_release(inf);
	}
	return err;
}

static int bmp_port_enable(struct bmp_inf *inf, int port, bool enable)
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

static int bmp_ports_enable(struct bmp_inf *inf, bool enable)
{
	int err;

	err = bmp_port_enable(inf, WR, enable);
	err |= bmp_port_enable(inf, RD, enable);
	return err;
}

static int bmp180_mode_wr_mpu(struct bmp_inf *inf, u8 mode)
{
	int err = 0;

	if (mode) {
		err = nvi_mpu_data_out(inf->port_id[WR], mode);
		err |= bmp_ports_enable(inf, true);
	}
	return err;
}

static int bmp280_mode_wr_mpu(struct bmp_inf *inf, u8 mode)
{
	u8 mode_old;
	u8 mode_new;
	int err = 0;

	mode_old = inf->data_out & BMP280_REG_CTRL_MODE_MASK;
	mode_new = mode & BMP280_REG_CTRL_MODE_MASK;
	switch (mode_new) {
	case BMP280_REG_CTRL_MODE_SLEEP:
		if (mode_old == BMP280_REG_CTRL_MODE_NORMAL)
			err = bmp_wr(inf, BMPX80_REG_CTRL, mode_new);
		break;

	case BMP280_REG_CTRL_MODE_FORCED1:
		if (mode_old == BMP280_REG_CTRL_MODE_NORMAL)
			err = bmp_wr(inf, BMPX80_REG_CTRL,
				     BMP280_REG_CTRL_MODE_SLEEP);
		err |= nvi_mpu_data_out(inf->port_id[WR], mode);
		err |= bmp_ports_enable(inf, true);
		break;

	case BMP280_REG_CTRL_MODE_FORCED2:
		if (mode_old == BMP280_REG_CTRL_MODE_NORMAL)
			err = bmp_wr(inf, BMPX80_REG_CTRL,
				     BMP280_REG_CTRL_MODE_SLEEP);
		err |= nvi_mpu_data_out(inf->port_id[WR], mode);
		err |= bmp_ports_enable(inf, true);
		break;

	case BMP280_REG_CTRL_MODE_NORMAL:
		err = bmp_wr(inf, BMPX80_REG_CTRL, mode);
		err |= bmp_port_enable(inf, RD, true);
		break;

	default:
		err = -EINVAL;
		break;
	}

	return err;
}

static int bmp_mode_wr(struct bmp_inf *inf, bool reset, bool enable)
{
	u8 mode_new;
	int err = 0;

	if (inf->dev_id == BMPX80_REG_ID_BMP180) {
		if (enable) {
			mode_new = inf->range_i << BMP180_REG_CTRL_OSS;
			mode_new |= BMP180_REG_CTRL_MODE_TEMP;
		} else {
			mode_new = 0;
		}
	} else {
		if (enable) {
			mode_new = inf->range_i + 1;
			mode_new = (mode_new << BMP280_REG_CTRL_OSRS_T) |
				   (mode_new << BMP280_REG_CTRL_OSRS_P);
			mode_new |= BMP280_REG_CTRL_MODE_FORCED1;
		} else {
			mode_new = BMP280_REG_CTRL_MODE_SLEEP;
		}
	}
	if ((mode_new == inf->data_out) && (!reset))
		return err;

	if (inf->use_mpu)
		err = bmp_ports_enable(inf, false);
	else
		cancel_delayed_work_sync(&inf->dw);
	if (err)
		return err;

	if (reset) {
		err = bmp_wr(inf, BMPX80_REG_RESET, BMPX80_REG_RESET_VAL);
		if (!err)
			mdelay(BMPX80_HW_DELAY_POR_MS);
	}
	if (inf->use_mpu) {
		inf->hal->bmp_mode_wr_mpu(inf, mode_new);
	} else {
		err = bmp_i2c_wr(inf, BMPX80_REG_CTRL, mode_new);
		if (enable)
			queue_delayed_work(inf->wq, &inf->dw,
					 usecs_to_jiffies(inf->poll_delay_us));
	}
	if (!err)
		inf->data_out = mode_new;
	return err;
}

static int bmp_delay(struct bmp_inf *inf,
		     unsigned int delay_us, unsigned int range_user)
{
	unsigned int i;
	int err;
	int err_t = 0;

	if (!range_user) {
		for (i = (inf->hal->range_limit - 1); i > 0; i--) {
			if (delay_us >= (inf->hal->bmp_delay_ms_tbl[i] * 1000))
				break;
		}
	} else {
		i = (range_user - 1);
	}
	if (i != inf->range_i) {
		err = 0;
		if (inf->use_mpu)
			err = nvi_mpu_delay_ms(inf->port_id[WR],
					       inf->hal->bmp_delay_ms_tbl[i]);
		if (err < 0)
			err_t |= err;
		else
			inf->range_i = i;
	}
	if (delay_us < (inf->hal->bmp_delay_ms_tbl[inf->range_i] * 1000))
		delay_us = (inf->hal->bmp_delay_ms_tbl[inf->range_i] * 1000);
	if (delay_us != inf->poll_delay_us) {
		if (inf->dbg & BMP_DBG_SPEW_MSG)
			dev_info(&inf->i2c->dev, "%s: %u\n",
				 __func__, delay_us);
		err = 0;
		if (inf->use_mpu)
			err = nvi_mpu_delay_us(inf->port_id[RD],
					       (unsigned long)delay_us);
		if (err)
			err_t |= err;
		else
			inf->poll_delay_us = delay_us;
	}
	return err_t;
}

static s64 bmp_timestamp_ns(void)
{
	struct timespec ts;
	s64 ns;

	ktime_get_ts(&ts);
	ns = timespec_to_ns(&ts);
	return ns;
}

static void bmp_report(struct bmp_inf *inf, s64 ts)
{
	if (inf->dbg & BMP_DBG_SPEW_PRESSURE_RAW)
		dev_info(&inf->i2c->dev, "pr %d %lld\n", inf->UP, ts);
	if (inf->dbg & BMP_DBG_SPEW_PRESSURE)
		dev_info(&inf->i2c->dev, "p %d %lld\n", inf->pressure, ts);
	if (inf->dbg & BMP_DBG_SPEW_TEMPERATURE_RAW)
		dev_info(&inf->i2c->dev, "tr %d %lld\n", inf->UT, ts);
	if (inf->dbg & BMP_DBG_SPEW_TEMPERATURE)
		dev_info(&inf->i2c->dev, "t %d %lld\n", inf->temperature, ts);
	input_report_abs(inf->idev, ABS_PRESSURE, inf->pressure);
	input_sync(inf->idev);
}

static void bmp180_calc(struct bmp_inf *inf)
{
	long X1, X2, X3, B3, B5, B6, p;
	unsigned long B4, B7;
	long pressure;

	X1 = ((inf->UT - inf->rom.bmp180.ac6) * inf->rom.bmp180.ac5) >> 15;
	X2 = inf->rom.bmp180.mc * (1 << 11) / (X1 + inf->rom.bmp180.md);
	B5 = X1 + X2;
	inf->temperature = (B5 + 8) >> 4;
	B6 = B5 - 4000;
	X1 = (inf->rom.bmp180.b2 * ((B6 * B6) >> 12)) >> 11;
	X2 = (inf->rom.bmp180.ac2 * B6) >> 11;
	X3 = X1 + X2;
	B3 = ((((inf->rom.bmp180.ac1 << 2) + X3) << inf->range_i) + 2) >> 2;
	X1 = (inf->rom.bmp180.ac3 * B6) >> 13;
	X2 = (inf->rom.bmp180.b1 * ((B6 * B6) >> 12)) >> 16;
	X3 = ((X1 + X2) + 2) >> 2;
	B4 = (inf->rom.bmp180.ac4 * (unsigned long)(X3 + 32768)) >> 15;
	B7 = ((unsigned long)inf->UP - B3) * (50000 >> inf->range_i);
	if (B7 < 0x80000000)
		p = (B7 << 1) / B4;
	else
		p = (B7 / B4) << 1;
	X1 = (p >> 8) * (p >> 8);
	X1 = (X1 * 3038) >> 16;
	X2 = (-7357 * p) >> 16;
	pressure = p + ((X1 + X2 + 3791) >> 4);
	inf->pressure = (int)pressure;
}

static int bmp180_read_sts(struct bmp_inf *inf, u8 *data)
{
	s32 val;
	int limit_lo;
	int limit_hi;
	int pres;
	bool report;
	int err = 0;

	/* BMP180_REG_CTRL_SCO is 0 when data is ready */
	if (!(data[0] & (1 << BMP180_REG_CTRL_SCO))) {
		err = -1;
		if (data[0] == 0x0A) { /* temperature */
			inf->UT = ((data[2] << 8) + data[3]);
			inf->data_out = BMP180_REG_CTRL_MODE_PRES |
					(inf->range_i << BMP180_REG_CTRL_OSS);
		} else { /* pressure */
			val = ((data[2] << 16) + (data[3] << 8) +
						data[4]) >> (8 - inf->range_i);
			inf->data_out = BMP180_REG_CTRL_MODE_TEMP;
			if (inf->resolution && (!inf->report)) {
				if (inf->UP == val)
					return err;
			}

			inf->UP = val;
			bmp180_calc(inf);
			pres = inf->pressure;
			if (inf->resolution) {
				limit_lo = pres;
				limit_hi = pres;
				limit_lo -= (inf->resolution >> 1);
				limit_hi += (inf->resolution >> 1);
				if (limit_lo < 0)
					limit_lo = 0;
				if ((pres < limit_lo) || (pres > limit_hi))
					report = true;
				else
					report = false;
			} else {
				report = true;
			}
			if (report || inf->report) {
				inf->report = false;
				err = 1;
			}
		}
	}
	return err;
}

static int bmp180_read(struct bmp_inf *inf)
{
	long long timestamp1;
	long long timestamp2;
	u8 data[5];
	int err;

	timestamp1 = bmp_timestamp_ns();
	err = bmp_i2c_rd(inf, BMPX80_REG_CTRL, 5, data);
	timestamp2 = bmp_timestamp_ns();
	if (err)
		return err;

	err = bmp180_read_sts(inf, data);
	if (err > 0) {
		timestamp2 = (timestamp2 - timestamp1) >> 1;
		timestamp1 += timestamp2;
		bmp_report(inf, timestamp1);
		bmp_i2c_wr(inf, BMPX80_REG_CTRL, inf->data_out);
	} else if (err < 0) {
		bmp_i2c_wr(inf, BMPX80_REG_CTRL, inf->data_out);
	}
	return 0;
}

static void bmp180_mpu_handler(u8 *data, unsigned int len, s64 ts, void *p_val)
{
	struct bmp_inf *inf;
	int err;

	inf = (struct bmp_inf *)p_val;
	if (inf->enable) {
		err = bmp180_read_sts(inf, data);
		if (err > 0) {
			bmp_report(inf, ts);
			nvi_mpu_data_out(inf->port_id[WR], inf->data_out);
		} else if (err < 0) {
			nvi_mpu_data_out(inf->port_id[WR], inf->data_out);
		}
	}
}

static void bmp280_calc_temp(struct bmp_inf *inf)
{
	s32 adc_T;
	s32 var1;
	s32 var2;

	adc_T = inf->UT;
	adc_T >>= (4 - inf->range_i);
	var1 = adc_T >> 3;
	var1 -= ((s32)inf->rom.bmp280.dig_T1 << 1);
	var1 *= (s32)inf->rom.bmp280.dig_T2;
	var1 >>= 11;
	var2 = adc_T >> 4;
	var2 -= (s32)inf->rom.bmp280.dig_T1;
	var2 *= var2;
	var2 >>= 12;
	var2 *= (s32)inf->rom.bmp280.dig_T3;
	var2 >>= 14;
	inf->t_fine = var1 + var2;
	inf->temperature = (inf->t_fine * 5 + 128) >> 8;
}

static int bmp280_calc_pres(struct bmp_inf *inf)
{
	s32 adc_P;
	s32 var1;
	s32 var2;
	s32 var3;
	u32 p;

	adc_P = inf->UP;
	var1 = inf->t_fine >> 1;
	var1 -= 64000;
	var2 = var1 >> 2;
	var2 *= var2;
	var3 = var2;
	var2 >>= 11;
	var2 *= inf->rom.bmp280.dig_P6;
	var2 += ((var1 * inf->rom.bmp280.dig_P5) << 1);
	var2 >>= 2;
	var2 += (inf->rom.bmp280.dig_P4 << 16);
	var3 >>= 13;
	var3 *= inf->rom.bmp280.dig_P3;
	var3 >>= 3;
	var1 *= inf->rom.bmp280.dig_P2;
	var1 >>= 1;
	var1 += var3;
	var1 >>= 18;
	var1 += 32768;
	var1 *= inf->rom.bmp280.dig_P1;
	var1 >>= 15;
	if (!var1)
		return -1;

	p = ((u32)(((s32)1048576) - adc_P) - (var2 >> 12)) * 3125;
	if (p < 0x80000000)
		p = (p << 1) / ((u32)var1);
	else
		p = (p / (u32)var1) << 1;
	var3 = p >> 3;
	var3 *= var3;
	var3 >>= 13;
	var1 = (s32)inf->rom.bmp280.dig_P9 * var3;
	var1 >>= 12;
	var2 = (s32)(p >> 2);
	var2 *= (s32)inf->rom.bmp280.dig_P8;
	var2 >>= 13;
	var3 = var1 + var2 + inf->rom.bmp280.dig_P7;
	var3 >>= 4;
	p = (u32)((s32)p + var3);
	inf->pressure = (int)p;
	return 1;
}

static int bmp280_read_sts(struct bmp_inf *inf, u8 *data)
{
	u8 sts;
	s32 val;
	int err;

	sts = data[1] & BMP280_REG_CTRL_MODE_MASK;
	if ((sts == BMP280_REG_CTRL_MODE_FORCED1) ||
					 (sts == BMP280_REG_CTRL_MODE_FORCED2))
		return 0;

	val = (data[4] << 16) | (data[5] << 8) | data[6];
	val = le32_to_cpup(&val);
	val >>= 4;
	inf->UP = val;
	val = (data[7] << 16) | (data[8] << 8) | data[9];
	val = le32_to_cpup(&val);
	val >>= 4;
	inf->UT = val;
	bmp280_calc_temp(inf);
	err = bmp280_calc_pres(inf);
	return err;
}

static int bmp280_read(struct bmp_inf *inf)
{
	long long timestamp1;
	long long timestamp2;
	u8 data[10];
	int err;

	timestamp1 = bmp_timestamp_ns();
	err = bmp_i2c_rd(inf, BMP280_REG_STATUS, 10, data);
	timestamp2 = bmp_timestamp_ns();
	if (err)
		return err;

	err = bmp280_read_sts(inf, data);
	if (err > 0) {
		timestamp2 = (timestamp2 - timestamp1) >> 1;
		timestamp1 += timestamp2;
		bmp_report(inf, timestamp1);
		bmp_i2c_wr(inf, BMPX80_REG_CTRL, inf->data_out);
	}
	return 0;
}

static void bmp280_mpu_handler(u8 *data, unsigned int len, s64 ts, void *p_val)
{
	struct bmp_inf *inf;
	int err;

	inf = (struct bmp_inf *)p_val;
	if (inf->enable) {
		err = bmp280_read_sts(inf, data);
		if (err > 0)
			bmp_report(inf, ts);
	}
}

static void bmp_work(struct work_struct *ws)
{
	struct bmp_inf *inf;

	inf = container_of(ws, struct bmp_inf, dw.work);
	inf->hal->bmp_read(inf);
	queue_delayed_work(inf->wq, &inf->dw,
			   usecs_to_jiffies(inf->poll_delay_us));
}

static int bmp_init_hw(struct bmp_inf *inf)
{
	u8 *p_rom8;
	u16 *p_rom16;
	int i;
	int err = 0;

	inf->UT = 0;
	inf->UP = 0;
	inf->temperature = 0;
	inf->pressure = 0;
	p_rom8 = (u8 *)&inf->rom;
	err = bmp_nvi_mpu_bypass_request(inf);
	if (!err) {
		err = bmp_i2c_rd(inf, inf->hal->rom_addr_start,
				 inf->hal->rom_size, p_rom8);
		bmp_nvi_mpu_bypass_release(inf);
	}
	if (err)
		return err;

	p_rom16 = (u16 *)&inf->rom;
	for (i = 0; i < (inf->hal->rom_size >> 1); i++) {
		*p_rom16 = be16_to_cpup(p_rom16);
		p_rom16++;
	}
	inf->initd = true;
	return err;
}

static int bmp_enable(struct bmp_inf *inf, bool enable)
{
	int err = 0;

	if (enable) {
		bmp_pm(inf, true);
		if (!inf->initd)
			err = bmp_init_hw(inf);
		if (!err) {
			inf->report = true;
			err |= bmp_delay(inf, inf->poll_delay_us,
					 inf->range_user);
			err |= bmp_mode_wr(inf, true, true);
			if (!err)
				inf->enable = true;
		}
	} else if (inf->enable) {
		err = bmp_mode_wr(inf, false, false);
		if (!err) {
			bmp_pm(inf, false);
			inf->enable = false;
		}
	}
	return err;
}

static ssize_t bmp_enable_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct bmp_inf *inf;
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
	dev_dbg(&inf->i2c->dev, "%s: %x\n", __func__, en);
	err = bmp_enable(inf, en);
	if (err) {
		dev_err(&inf->i2c->dev, "%s: %x ERR=%d\n", __func__, en, err);
		return err;
	}

	return count;
}

static ssize_t bmp_enable_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct bmp_inf *inf;
	unsigned int enable;

	inf = dev_get_drvdata(dev);
	if (inf->enable)
		enable = 1;
	else
		enable = 0;
	return sprintf(buf, "%u\n", enable);
}

static ssize_t bmp_delay_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct bmp_inf *inf;
	unsigned int delay_us;
	int err;

	inf = dev_get_drvdata(dev);
	err = kstrtouint(buf, 10, &delay_us);
	if (err)
		return -EINVAL;

	if (inf->dev_id == BMPX80_REG_ID_BMP180)
		/* since we rotate between acquiring data for pressure and
		 * temperature on the BMP180 we need to go twice as fast.
		 */
		delay_us >>= 1;
	err = bmp_delay(inf, delay_us, inf->range_user);
	if (err) {
		dev_err(&inf->i2c->dev, "%s: %u ERR=%d\n",
			__func__, delay_us, err);
		return err;
	}

	return count;
}

static ssize_t bmp_delay_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct bmp_inf *inf;
	unsigned int delay_us;

	inf = dev_get_drvdata(dev);
	if (inf->enable)
		delay_us = inf->poll_delay_us;
	else
		delay_us = inf->hal->bmp_delay_ms_tbl[inf->range_i] * 1000;
	return sprintf(buf, "%u\n", delay_us);
}

static ssize_t bmp_resolution_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct bmp_inf *inf;
	unsigned int resolution;

	inf = dev_get_drvdata(dev);
	if (kstrtouint(buf, 10, &resolution))
		return -EINVAL;

	inf->resolution = resolution;
	dev_dbg(&inf->i2c->dev, "%s %u", __func__, resolution);
	return count;
}

static ssize_t bmp_resolution_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct bmp_inf *inf;
	unsigned int resolution;

	inf = dev_get_drvdata(dev);
	if (inf->enable)
		resolution = inf->resolution;
	else
		resolution = BMP180_INPUT_RESOLUTION;
	return sprintf(buf, "%u\n", resolution);
}

static ssize_t bmp_max_range_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct bmp_inf *inf;
	unsigned int range_user;
	int err;

	inf = dev_get_drvdata(dev);
	err = kstrtouint(buf, 10, &range_user);
	if (err)
		return -EINVAL;

	if (range_user > inf->hal->range_limit)
		return -EINVAL;

	if (range_user != inf->range_user) {
		dev_dbg(&inf->i2c->dev, "%s %u\n", __func__, range_user);
		err = bmp_delay(inf, inf->poll_delay_us, range_user);
		if (!err) {
			inf->range_user = range_user;
		} else {
			dev_err(&inf->i2c->dev, "%s ERR %u\n",
				__func__, range_user);
			return err;
		}
	}

	return count;
}

static ssize_t bmp_max_range_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct bmp_inf *inf;
	unsigned int range;

	inf = dev_get_drvdata(dev);
	if (inf->enable)
		range = inf->range_i;
	else
		range = (BMP180_PRESSURE_MAX * inf->hal->divisor);
	return sprintf(buf, "%u\n", range);
}

static ssize_t bmp_data_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct bmp_inf *inf;
	unsigned int data_info;
	int err;

	inf = dev_get_drvdata(dev);
	err = kstrtouint(buf, 10, &data_info);
	if (err)
		return -EINVAL;

	if (data_info >= BMP_DATA_INFO_LIMIT_MAX)
		return -EINVAL;

	dev_dbg(&inf->i2c->dev, "%s %u\n", __func__, data_info);
	inf->data_info = data_info;
	switch (data_info) {
	case BMP_DATA_INFO_DATA:
		inf->dbg = 0;
		break;

	case BMP_DATA_INFO_DBG:
		inf->dbg ^= BMP_DBG_SPEW_MSG;
		break;

	case BMP_DATA_INFO_PRESSURE_SPEW:
		inf->dbg ^= BMP_DBG_SPEW_PRESSURE;
		break;

	case BMP_DATA_INFO_TEMPERATURE_SPEW:
		inf->dbg ^= BMP_DBG_SPEW_TEMPERATURE;
		break;

	case BMP_DATA_INFO_PRESSURE_RAW_SPEW:
		inf->dbg ^= BMP_DBG_SPEW_PRESSURE_RAW;
		break;

	case BMP_DATA_INFO_TEMPERATURE_RAW_SPEW:
		inf->dbg ^= BMP_DBG_SPEW_TEMPERATURE_RAW;
		break;

	default:
		break;
	}

	return count;
}

static ssize_t bmp_data_show(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	struct bmp_inf *inf;
	ssize_t t;
	u8 data[11];
	u16 *cal;
	enum BMP_DATA_INFO data_info;
	bool enable;
	unsigned int i;
	int err = 0;

	inf = dev_get_drvdata(dev);
	data_info = inf->data_info;
	inf->data_info = BMP_DATA_INFO_DATA;
	enable = inf->enable;
	switch (data_info) {
	case BMP_DATA_INFO_DATA:
		t = sprintf(buf, "pressure: %d   raw: %d\n",
			    inf->pressure, inf->UP);
		t += sprintf(buf + t, "temperature: %d   raw: %d\n",
			     inf->temperature, inf->UT);
		if (inf->dev_id != BMPX80_REG_ID_BMP180)
			t += sprintf(buf + t, "temperature_fine: %d\n",
				     inf->t_fine);
		return t;

	case BMP_DATA_INFO_VER:
		return sprintf(buf, "version=%u\n", BMP_VERSION);

	case BMP_DATA_INFO_RESET:
		bmp_pm(inf, true);
		err = bmp_mode_wr(inf, true, enable);
		bmp_pm(inf, enable);
		if (err)
			return sprintf(buf, "reset ERR %d\n", err);
		else
			return sprintf(buf, "reset done\n");

	case BMP_DATA_INFO_REGS:
		if (!inf->initd) {
			t = sprintf(buf, "calibration: NEED ENABLE\n");
		} else {
			t = sprintf(buf, "calibration:\n");
			cal = &inf->rom.bmp280.dig_T1;
			for (i = 0; i < inf->hal->rom_size; i = i + 2)
				t += sprintf(buf + t, "%#2x=%#2x\n",
					     inf->hal->rom_addr_start + i,
					     *cal++);
		}
		err = bmp_nvi_mpu_bypass_request(inf);
		if (!err) {
			err = bmp_i2c_rd(inf, BMPX80_REG_ID, 1, data);
			err |= bmp_i2c_rd(inf, BMP280_REG_STATUS,
					  10, &data[1]);
			bmp_nvi_mpu_bypass_release(inf);
		}
		if (err) {
			t += sprintf(buf + t, "registers: ERR %d\n", err);
		} else {
			t += sprintf(buf + t, "registers:\n");
			t += sprintf(buf + t, "%#2x=%#2x\n",
				     BMPX80_REG_ID, data[0]);
			for (i = 0; i < 10; i++)
				t += sprintf(buf + t, "%#2x=%#2x\n",
					     BMP280_REG_STATUS + i,
					     data[i + 1]);
		}
		return t;

	case BMP_DATA_INFO_DBG:
		return sprintf(buf, "debug spew=%x\n",
			       !!(inf->dbg & BMP_DBG_SPEW_MSG));

	case BMP_DATA_INFO_PRESSURE_SPEW:
		return sprintf(buf, "pressure spew=%x\n",
			       !!(inf->dbg & BMP_DBG_SPEW_PRESSURE));

	case BMP_DATA_INFO_TEMPERATURE_SPEW:
		return sprintf(buf, "temperature spew=%x\n",
			       !!(inf->dbg & BMP_DBG_SPEW_TEMPERATURE));

	case BMP_DATA_INFO_PRESSURE_RAW_SPEW:
		return sprintf(buf, "pressure_raw spew=%x\n",
			       !!(inf->dbg & BMP_DBG_SPEW_PRESSURE_RAW));

	case BMP_DATA_INFO_TEMPERATURE_RAW_SPEW:
		return sprintf(buf, "temperature_raw spew=%x\n",
			       !!(inf->dbg & BMP_DBG_SPEW_TEMPERATURE_RAW));

	default:
		break;
	}

	return -EINVAL;
}

static ssize_t bmp_mpu_fifo_enable_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	struct bmp_inf *inf;
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

static ssize_t bmp_mpu_fifo_enable_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct bmp_inf *inf = dev_get_drvdata(dev);

	return sprintf(buf, "%x\n", inf->fifo_enable & inf->use_mpu);
}

static ssize_t bmp_divisor_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct bmp_inf *inf;

	inf = dev_get_drvdata(dev);
	return sprintf(buf, "%u\n", inf->hal->divisor);
}

static ssize_t bmp_microamp_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct bmp_inf *inf;

	inf = dev_get_drvdata(dev);
	return sprintf(buf, "%u\n", BMP180_INPUT_POWER_UA);
}


static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
		   bmp_enable_show, bmp_enable_store);
static DEVICE_ATTR(delay, S_IRUGO | S_IWUSR | S_IWGRP,
		   bmp_delay_show, bmp_delay_store);
static DEVICE_ATTR(resolution, S_IRUGO | S_IWUSR | S_IWGRP,
		   bmp_resolution_show, bmp_resolution_store);
static DEVICE_ATTR(max_range, S_IRUGO | S_IWUSR | S_IWGRP,
		   bmp_max_range_show, bmp_max_range_store);
static DEVICE_ATTR(mpu_fifo_en, S_IRUGO | S_IWUSR | S_IWGRP,
		   bmp_mpu_fifo_enable_show, bmp_mpu_fifo_enable_store);
static DEVICE_ATTR(data, S_IRUGO | S_IWUSR | S_IWGRP,
		   bmp_data_show, bmp_data_store);
static DEVICE_ATTR(divisor, S_IRUGO,
		   bmp_divisor_show, NULL);
static DEVICE_ATTR(microamp, S_IRUGO,
		   bmp_microamp_show, NULL);

static struct attribute *bmp_attrs[] = {
	&dev_attr_enable.attr,
	&dev_attr_delay.attr,
	&dev_attr_resolution.attr,
	&dev_attr_max_range.attr,
	&dev_attr_divisor.attr,
	&dev_attr_microamp.attr,
	&dev_attr_data.attr,
	&dev_attr_mpu_fifo_en.attr,
	NULL
};

static struct attribute_group bmp_attr_group = {
	.name = BMPX80_NAME,
	.attrs = bmp_attrs
};

static int bmp_sysfs_create(struct bmp_inf *inf)
{
	int err;

	err = sysfs_create_group(&inf->idev->dev.kobj, &bmp_attr_group);
	if (err) {
		dev_err(&inf->i2c->dev, "%s ERR %d\n", __func__, err);
		return err;
	}
	err = nvi_mpu_sysfs_register(&inf->idev->dev.kobj, BMPX80_NAME);
	if (err)
		dev_err(&inf->i2c->dev, "%s ERR %d\n", __func__, err);
	return err;
}

static void bmp_input_close(struct input_dev *idev)
{
	struct bmp_inf *inf;

	inf = input_get_drvdata(idev);
	if (inf != NULL)
		bmp_enable(inf, false);
}

static int bmp_input_create(struct bmp_inf *inf)
{
	int err;

	inf->idev = input_allocate_device();
	if (!inf->idev) {
		err = -ENOMEM;
		dev_err(&inf->i2c->dev, "%s ERR %d\n", __func__, err);
		return err;
	}

	inf->idev->name = BMPX80_NAME;
	inf->idev->dev.parent = &inf->i2c->dev;
	inf->idev->close = bmp_input_close;
	input_set_drvdata(inf->idev, inf);
	input_set_capability(inf->idev, EV_ABS, ABS_PRESSURE);
	input_set_abs_params(inf->idev, ABS_PRESSURE,
			     BMP180_PRESSURE_MIN, BMP180_PRESSURE_MAX,
			     BMP180_PRESSURE_FUZZ, BMP180_PRESSURE_FLAT);
	err = input_register_device(inf->idev);
	if (err) {
		input_free_device(inf->idev);
		inf->idev = NULL;
	}
	return err;
}

static int bmp_id_compare(struct bmp_inf *inf, u8 val,
			  const struct i2c_device_id *id)
{
	unsigned int i;
	int err = 0;

	for (i = 0; i < ARRAY_SIZE(bmp_ids); i++) {
		if (val == bmp_ids[i]) {
			if (inf->dev_id && (inf->dev_id != val))
				dev_err(&inf->i2c->dev, "%s ERR: %x != %s\n",
					__func__, inf->dev_id, id->name);
			if (val != BMPX80_REG_ID_BMP180)
				/* BMP280 may have more ID's than 0x56 */
				val = BMPX80_REG_ID_BMP280;
			inf->dev_id = val;
			break;
		}
	}
	if (!inf->dev_id) {
		err = -ENODEV;
		dev_err(&inf->i2c->dev, "%s ERR: ID %x != %s\n",
			__func__, val, id->name);
	} else {
		dev_dbg(&inf->i2c->dev, "%s using ID %x for %s\n",
			__func__, inf->dev_id, id->name);
	}
	return err;
}

static struct bmp_hal bmp180_hal = {
	.rom_addr_start		= BMP180_REG_AC1,
	.rom_size		= 22,
	.bmp_delay_ms_tbl	= bmp180_delay_ms_tbl,
	.divisor		= BMP180_INPUT_DIVISOR,
	.range_limit		= ARRAY_SIZE(bmp180_delay_ms_tbl),
	.range_dflt		= BMP180_RANGE_DFLT,
	.bmp_read		= &bmp180_read,
	.bmp_mode_wr_mpu	= &bmp180_mode_wr_mpu
};

static struct bmp_hal bmp280_hal = {
	.rom_addr_start		= BMP280_REG_CWORD00,
	.rom_size		= 26,
	.bmp_delay_ms_tbl	= bmp280_delay_ms_tbl,
	.divisor		= BMP280_INPUT_DIVISOR,
	.range_limit		= ARRAY_SIZE(bmp280_delay_ms_tbl),
	.range_dflt		= BMP280_RANGE_DFLT,
	.bmp_read		= &bmp280_read,
	.bmp_mode_wr_mpu	= &bmp280_mode_wr_mpu
};

static int bmp_hal(struct bmp_inf *inf)
{
	int err = 0;

	switch (inf->dev_id) {
	case BMPX80_REG_ID_BMP280:
		inf->hal = &bmp280_hal;
		break;

	case BMPX80_REG_ID_BMP180:
		inf->hal = &bmp180_hal;
		break;

	default:
		dev_err(&inf->i2c->dev, "%s ERR: Unknown device\n", __func__);
		inf->hal = &bmp180_hal; /* to prevent NULL pointers */
		err = -ENODEV;
		break;
	}

	inf->range_user = inf->hal->range_dflt;
	return err;
}

static int bmp_id(struct bmp_inf *inf, const struct i2c_device_id *id)
{
	struct nvi_mpu_port nmp;
	u8 config_boot;
	u8 val = 0;
	int err;

	config_boot = inf->pdata.config & NVI_CONFIG_BOOT_MASK;
	if ((inf->i2c->addr != BMP280_I2C_ADDR0) ||
					  (inf->i2c->addr != BMPX80_I2C_ADDR1))
		inf->i2c->addr = BMPX80_I2C_ADDR1;
	if (!strcmp(id->name, BMP180_NAME))
		inf->dev_id = BMPX80_REG_ID_BMP180;
	else if (!strcmp(id->name, BMP280_NAME))
		inf->dev_id = BMPX80_REG_ID_BMP280;
	if ((config_boot == NVI_CONFIG_BOOT_MPU) && (!inf->dev_id)) {
		dev_err(&inf->i2c->dev, "%s ERR: NVI_CONFIG_BOOT_MPU && %s\n",
			__func__, id->name);
		config_boot = NVI_CONFIG_BOOT_AUTO;
	}
	if (config_boot == NVI_CONFIG_BOOT_AUTO) {
		nmp.addr = inf->i2c->addr | 0x80;
		nmp.reg = BMPX80_REG_ID;
		nmp.ctrl = 1;
		err = nvi_mpu_dev_valid(&nmp, &val);
		dev_dbg(&inf->i2c->dev, "%s AUTO ID=%x err=%d\n",
			__func__, val, err);
		/* see mpu.h for possible return values */
		if ((err == -EAGAIN) || (err == -EBUSY))
			return -EAGAIN;

		if (!err)
			err = bmp_id_compare(inf, val, id);
		if ((!err) || ((err == -EIO) && (inf->dev_id)))
			config_boot = NVI_CONFIG_BOOT_MPU;
	}
	if (config_boot == NVI_CONFIG_BOOT_MPU) {
		bmp_hal(inf);
		inf->use_mpu = true;
		nmp.addr = inf->i2c->addr | 0x80;
		nmp.data_out = 0;
		nmp.delay_ms = 0;
		nmp.delay_us = inf->poll_delay_us;
		nmp.shutdown_bypass = true;
		nmp.ext_driver = (void *)inf;
		if (inf->dev_id == BMPX80_REG_ID_BMP180) {
			nmp.reg = BMPX80_REG_CTRL;
			nmp.ctrl = 6; /* MPU FIFO can't handle odd size */
			nmp.handler = &bmp180_mpu_handler;
		} else {
			nmp.reg = BMP280_REG_STATUS;
			nmp.ctrl = 10; /* MPU FIFO can't handle odd size */
			nmp.handler = &bmp280_mpu_handler;
		}
		err = nvi_mpu_port_alloc(&nmp);
		dev_dbg(&inf->i2c->dev, "%s MPU port/err=%d\n",
			__func__, err);
		if (err < 0)
			return err;

		inf->port_id[RD] = err;
		nmp.addr = inf->i2c->addr;
		nmp.reg = BMPX80_REG_CTRL;
		nmp.ctrl = 1;
		nmp.data_out = inf->data_out;
		nmp.delay_ms = inf->hal->bmp_delay_ms_tbl[inf->range_i];
		nmp.delay_us = 0;
		nmp.shutdown_bypass = false;
		nmp.handler = NULL;
		nmp.ext_driver = NULL;
		err = nvi_mpu_port_alloc(&nmp);
		dev_dbg(&inf->i2c->dev, "%s MPU port/err=%d\n",
			__func__, err);
		if (err < 0) {
			bmp_ports_free(inf);
			dev_err(&inf->i2c->dev, "%s ERR %d\n", __func__, err);
		} else {
			inf->port_id[WR] = err;
			err = 0;
		}
		return err;
	}

	/* NVI_CONFIG_BOOT_HOST */
	inf->use_mpu = false;
	err = bmp_i2c_rd(inf, BMPX80_REG_ID, 1, &val);
	dev_dbg(&inf->i2c->dev, "%s Host read ID=%x err=%d\n",
		__func__, val, err);
	if (!err) {
		err = bmp_id_compare(inf, val, id);
		if (!err)
			err = bmp_hal(inf);
	}
	return err;
}

static int bmp_remove(struct i2c_client *client)
{
	struct bmp_inf *inf;

	inf = i2c_get_clientdata(client);
	if (inf != NULL) {
		if (inf->idev) {
			input_unregister_device(inf->idev);
			input_free_device(inf->idev);
		}
		if (inf->wq)
			destroy_workqueue(inf->wq);
		bmp_pm_exit(inf);
	}
	dev_info(&client->dev, "%s\n", __func__);
	return 0;
}

static void bmp_shutdown(struct i2c_client *client)
{
	bmp_remove(client);
}

static struct mpu_platform_data *bmp_parse_dt(struct i2c_client *client)
{
	struct mpu_platform_data *pdata;
	struct device_node *np = client->dev.of_node;
	char const *pchar;
	u8 config;

	pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(&client->dev, "Can't allocate platform data\n");
		return ERR_PTR(-ENOMEM);
	}

	if (of_property_read_string(np, "config", &pchar)) {
		dev_err(&client->dev, "Cannot read config property\n");
		return ERR_PTR(-EINVAL);
	}

	for (config = 0; config < ARRAY_SIZE(bmp_configs); config++) {
		if (!strcasecmp(pchar, bmp_configs[config])) {
			pdata->config = config;
			break;
		}
	}

	if (config == ARRAY_SIZE(bmp_configs)) {
		dev_err(&client->dev, "Invalid config value\n");
		return ERR_PTR(-EINVAL);
	}

	return pdata;
}

static int bmp_probe(struct i2c_client *client,
		     const struct i2c_device_id *id)
{
	struct bmp_inf *inf;
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
		pd = bmp_parse_dt(client);
		if (IS_ERR(pd))
			return -EINVAL;
		else
			inf->pdata = *pd;
	} else {
		pd = (struct mpu_platform_data *)dev_get_platdata(&client->dev);
		if (pd != NULL)
			inf->pdata = *pd;
	}

	bmp_pm_init(inf);
	err = bmp_id(inf, id);
	bmp_pm(inf, false);
	if (err == -EAGAIN)
		goto bmp_probe_again;
	else if (err)
		goto bmp_probe_err;

	err = bmp_input_create(inf);
	if (err)
		goto bmp_probe_err;

	inf->wq = create_singlethread_workqueue(BMPX80_NAME);
	if (!inf->wq) {
		dev_err(&client->dev, "%s workqueue ERR\n", __func__);
		err = -ENOMEM;
		goto bmp_probe_err;
	}

	INIT_DELAYED_WORK(&inf->dw, bmp_work);
	err = bmp_sysfs_create(inf);
	if (err)
		goto bmp_probe_err;

	return 0;

bmp_probe_err:
	dev_err(&client->dev, "%s ERR %d\n", __func__, err);
bmp_probe_again:
	bmp_remove(client);
	return err;
}

static const struct i2c_device_id bmp_i2c_device_id[] = {
	{BMPX80_NAME, 0},
	{BMP180_NAME, 0},
	{BMP280_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, bmp_i2c_device_id);

static const struct of_device_id bmp_of_match[] = {
	{ .compatible = "bmp,bmpX80", },
	{ .compatible = "bmp,bmp180", },
	{ .compatible = "bmp,bmp280", },
	{ },
};

MODULE_DEVICE_TABLE(of, bmp_of_match);

static struct i2c_driver bmp_driver = {
	.class		= I2C_CLASS_HWMON,
	.probe		= bmp_probe,
	.remove		= bmp_remove,
	.driver = {
		.name	= BMPX80_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(bmp_of_match),
	},
	.id_table	= bmp_i2c_device_id,
	.shutdown	= bmp_shutdown,
};

static int __init bmp_init(void)
{
	return i2c_add_driver(&bmp_driver);
}

static void __exit bmp_exit(void)
{
	i2c_del_driver(&bmp_driver);
}

module_init(bmp_init);
module_exit(bmp_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("BMPX80 driver");
MODULE_AUTHOR("NVIDIA Corporation");

