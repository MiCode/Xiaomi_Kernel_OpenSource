/*
* Copyright (C) 2012 Invensense, Inc.
* Copyright (c) 2013 NVIDIA CORPORATION.  All rights reserved.
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

/**
 *  @addtogroup  DRIVERS
 *  @brief       Hardware drivers.
 *
 *  @{
 *      @file    inv_gyro.c
 *      @brief   A sysfs device driver for Invensense devices
 *      @details This driver currently works for the ITG3500, MPU6050, MPU9150
 *               MPU3050
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/jiffies.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kfifo.h>
#include <linux/regulator/consumer.h>
#include <linux/notifier.h>
#include <linux/byteorder/generic.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/mpu.h>

#include "inv_gyro.h"


#define NVI_VERSION			(40)

/* regulator names in order of powering on */
static char *nvi_vregs[] = {
	"vdd",
	"vlogic",
};

static int nvi_nb_vreg(struct inv_gyro_state_s *st,
		       unsigned long event, unsigned int i);

static int nvi_nb_vreg_vdd(struct notifier_block *nb,
			   unsigned long event, void *ignored)
{
	struct inv_gyro_state_s *st = container_of(nb, struct inv_gyro_state_s,
						   nb_vreg[0]);
	return nvi_nb_vreg(st, event, 0);
}

static int nvi_nb_vreg_vlogic(struct notifier_block *nb,
			      unsigned long event, void *ignored)
{
	struct inv_gyro_state_s *st = container_of(nb, struct inv_gyro_state_s,
						   nb_vreg[1]);
	return nvi_nb_vreg(st, event, 1);
}

static int (* const nvi_nb_vreg_pf[])(struct notifier_block *nb,
				      unsigned long event, void *ignored) = {
	nvi_nb_vreg_vdd,
	nvi_nb_vreg_vlogic,
};


static struct inv_reg_map_s chip_reg = {
	.who_am_i		= 0x75,
	.sample_rate_div	= 0x19,
	.lpf			= 0x1A,
	.product_id		= 0x0C,
	.bank_sel		= 0x6D,
	.user_ctrl		= 0x6A,
	.fifo_en		= 0x23,
	.gyro_config		= 0x1B,
	.accl_config		= 0x1C,
	.fifo_count_h		= 0x72,
	.fifo_r_w		= 0x74,
	.raw_gyro		= 0x43,
	.raw_accl		= 0x3B,
	.temperature		= 0x41,
	.int_enable		= 0x38,
	.int_status		= 0x3A,
	.pwr_mgmt_1		= 0x6B,
	.pwr_mgmt_2		= 0x6C,
	.mem_start_addr		= 0x6E,
	.mem_r_w		= 0x6F,
	.prgm_strt_addrh	= 0x70,

	.accl_fifo_en		= BIT_ACCEL_OUT,
	.fifo_reset		= BIT_FIFO_RST,
	.i2c_mst_reset		= BIT_I2C_MST_RST,
	.cycle			= BIT_CYCLE,
	.temp_dis		= BIT_TEMP_DIS
};

static const struct inv_hw_s hw_info[INV_NUM_PARTS] = {
	{119, "ITG3500"},
	{ 63, "MPU3050"},
	{117, "MPU6050"},
	{118, "MPU9150"},
	{128, "MPU6500"},
	{128, "MPU9250"},
	{128, "MPU9350"},
	{128, "MPU6515"},
};

static unsigned long nvi_lpf_us_tbl[] = {
	0, /* WAR: disabled 3906, 256Hz */
	5319,	/* 188Hz */
	10204,	/* 98Hz */
	23810,	/* 42Hz */
	50000,	/* 20Hz */
	100000,	/* 10Hz */
	/* 200000, 5Hz */
};

static unsigned long nvi_lpa_delay_us_tbl_6050[] = {
	800000,	/* 800ms */
	200000,	/* 200ms */
	50000,	/* 50ms */
	/* 25000, 25ms */
};

static unsigned long nvi_lpa_delay_us_tbl_6500[] = {
	4096000,/* 4096ms */
	2048000,/* 2048ms */
	1024000,/* 1024ms */
	512000,	/* 512ms */
	256000,	/* 256ms */
	128000,	/* 128ms */
	64000,	/* 64ms */
	32000,	/* 32ms */
	16000,	/* 16ms */
	8000,	/* 8ms */
	4000,	/* 4ms */
	/* 2000, 2ms */
};

static struct inv_gyro_state_s *inf_local;

s64 nvi_ts_ns(void)
{
	struct timespec ts;

	ktime_get_ts(&ts);
	return timespec_to_ns(&ts);
}

/**
 *  inv_i2c_read_base() - Read one or more bytes from the device registers.
 *  @st:	Device driver instance.
 *  @reg:	First device register to be read from.
 *  @length:	Number of bytes to read.
 *  @data:	Data read from device.
 *  NOTE: The slave register will not increment when reading from the FIFO.
 */
int inv_i2c_read_base(struct inv_gyro_state_s *st, unsigned short i2c_addr,
		      unsigned char reg, unsigned short length,
		      unsigned char *data)
{
	struct i2c_msg msgs[2];
	int res;

	msgs[0].addr = i2c_addr;
	msgs[0].flags = 0;	/* write */
	msgs[0].buf = &reg;
	msgs[0].len = 1;

	msgs[1].addr = i2c_addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].buf = data;
	msgs[1].len = length;

	res = i2c_transfer(st->sl_handle, msgs, 2);
	pr_debug("%s RD%02X%02X%02X res=%d\n",
		 st->hw_s->name, i2c_addr, reg, length, res);
	if (res < 2) {
		if (res >= 0)
			res = -EIO;
		return res;
	}

	return 0;
}

/**
 *  inv_i2c_single_write_base() - Write a byte to a device register.
 *  @st:	Device driver instance.
 *  @reg:	Device register to be written to.
 *  @data:	Byte to write to device.
 */
int inv_i2c_single_write_base(struct inv_gyro_state_s *st,
			      unsigned short i2c_addr, unsigned char reg,
			      unsigned char data)
{
	unsigned char tmp[2];
	struct i2c_msg msg;
	int res;

	tmp[0] = reg;
	tmp[1] = data;

	msg.addr = i2c_addr;
	msg.flags = 0;	/* write */
	msg.buf = tmp;
	msg.len = 2;

	res = i2c_transfer(st->sl_handle, &msg, 1);
	pr_debug("%s WR%02X%02X%02X res=%d\n",
		 st->hw_s->name, i2c_addr, reg, data, res);
	if (res < 1) {
		if (res == 0)
			res = -EIO;
		return res;
	}

	return 0;
}


/* Register SMPLRT_DIV (0x19) */
int nvi_smplrt_div_wr(struct inv_gyro_state_s *inf, u8 smplrt_div)
{
	int err = 0;

	if (smplrt_div != inf->hw.smplrt_div) {
		err = inv_i2c_single_write(inf, inf->reg->sample_rate_div,
					   smplrt_div);
		if (!err)
			inf->hw.smplrt_div = smplrt_div;
	}
	return err;
}

/* Register CONFIG (0x1A) */
int nvi_config_wr(struct inv_gyro_state_s *inf, u8 val)
{
	int err = 0;

	if (val != inf->hw.config) {
		err = inv_i2c_single_write(inf, inf->reg->lpf, val);
		if (!err) {
			inf->hw.config = val;
			err = 1; /* flag change made */
		}
	}
	return err;
}

/* Register GYRO_CONFIG (0x1B) */
int nvi_gyro_config_wr(struct inv_gyro_state_s *inf, u8 test, u8 fsr)
{
	u8 val;
	int err = 0;

	if (inf->chip_type == INV_MPU3050) {
		val = inf->hw.config;
		val &= 0xE7;
		val |= fsr << 3;
		return nvi_config_wr(inf, val);
	}

	val = (test << 5) | (fsr << 3);
	if (val != inf->hw.gyro_config) {
		err = inv_i2c_single_write(inf, inf->reg->gyro_config, val);
		if (!err) {
			inf->hw.gyro_config = val;
			err = 1; /* flag change made */
		}
	}
	return err;
}

/* Register ACCEL_CONFIG2 (0x1D) */
static int nvi_accel_config2_wr(struct inv_gyro_state_s *inf, u8 val)
{
	int err = 0;

	if (val != inf->hw.accl_config2) {
		err = inv_i2c_single_write(inf, 0x1D, val);
		if (!err) {
			inf->hw.accl_config2 = val;
			err = 1; /* flag change made */
		}
	}
	return err;
}

/* Register ACCEL_CONFIG (0x1C) */
int nvi_accel_config_wr(struct inv_gyro_state_s *inf, u8 test, u8 fsr, u8 hpf)
{
	u8 val;
	int err;
	int err_t = 0;

	if (inf->chip_type == INV_MPU3050) {
		if (inf->mpu_slave != NULL)
			err_t = inf->mpu_slave->set_fs(inf, fsr);
		return err_t;
	}

	val = (test << 5) | (fsr << 3);
	if (inf->chip_type == INV_MPU6500)
		err_t = nvi_accel_config2_wr(inf, hpf);
	else
		val |= hpf;
	if (val != inf->hw.accl_config) {
		err = inv_i2c_single_write(inf, inf->reg->accl_config, val);
		if (!err) {
			inf->hw.accl_config = val;
			err_t |= 1; /* flag change made */
		} else {
			err_t |= err;
		}
	}
	return err_t;
}

/* Register LP_ACCEL_ODR (0x1E) */
static int nvi_lp_accel_odr_wr(struct inv_gyro_state_s *inf, u8 lposc_clksel)
{
	int err = 0;

	if (lposc_clksel != inf->hw.lposc_clksel) {
		err = inv_i2c_single_write(inf, REG_6500_LP_ACCEL_ODR,
					   lposc_clksel);
		if (!err)
			inf->hw.lposc_clksel = lposc_clksel;
	}
	return err;
}

/* Register MOT_THR (0x1F) */
static int nvi_mot_thr_wr(struct inv_gyro_state_s *inf, u8 mot_thr)
{
	int err = 0;

	if (mot_thr != inf->hw.mot_thr) {
		err = inv_i2c_single_write(inf, REG_MOT_THR, mot_thr);
		if (!err)
			inf->hw.mot_thr = mot_thr;
	}
	return err;
}

/* Register MOT_DUR (0x20) */
static int nvi_mot_dur_wr(struct inv_gyro_state_s *inf, u8 mot_dur)
{
	int err = 0;

	if (mot_dur != inf->hw.mot_dur) {
		err = inv_i2c_single_write(inf, REG_MOT_DUR, mot_dur);
		if (!err)
			inf->hw.mot_dur = mot_dur;
	}
	return err;
}

/* Register FIFO_EN (0x23) */
int nvi_fifo_en_wr(struct inv_gyro_state_s *inf, u8 fifo_en)
{
	int err = 0;

	if (fifo_en != inf->hw.fifo_en) {
		err = inv_i2c_single_write(inf, inf->reg->fifo_en, fifo_en);
		if (!err)
			inf->hw.fifo_en = fifo_en;
	}
	return err;
}

/* Register I2C_MST_CTRL (0x24) */
static int nvi_i2c_mst_ctrl_wr(struct inv_gyro_state_s *inf,
			       bool port3_fifo_en)
{
	u8 val;
	int err = 0;

	if (inf->chip_type == INV_MPU3050)
		return 0;

	val = inf->aux.clock_i2c;
	val |= BIT_WAIT_FOR_ES;
	if (port3_fifo_en)
		val |= BIT_SLV3_FIFO_EN;
	if (val != inf->hw.i2c_mst_ctrl) {
		err = inv_i2c_single_write(inf, REG_I2C_MST_CTRL, val);
		if (!err)
			inf->hw.i2c_mst_ctrl = val;
	}
	return err;
}

/* Register I2C_SLV0_CTRL (0x25) */
/* Register I2C_SLV1_CTRL (0x28) */
/* Register I2C_SLV2_CTRL (0x2B) */
/* Register I2C_SLV3_CTRL (0x2E) */
/* Register I2C_SLV4_CTRL (0x31) */
static int nvi_i2c_slv_addr_wr(struct inv_gyro_state_s *inf, int port, u8 addr)
{
	u8 reg;
	int err = 0;

	reg = (REG_I2C_SLV0_ADDR + (port * 3));
	if (addr != inf->hw.i2c_slv_addr[port]) {
		err = inv_i2c_single_write(inf, reg, addr);
		if (!err)
			inf->hw.i2c_slv_addr[port] = addr;
	}
	return err;
}

/* Register I2C_SLV0_CTRL (0x26) */
/* Register I2C_SLV1_CTRL (0x29) */
/* Register I2C_SLV2_CTRL (0x2C) */
/* Register I2C_SLV3_CTRL (0x2F) */
/* Register I2C_SLV4_CTRL (0x32) */
static int nvi_i2c_slv_reg_wr(struct inv_gyro_state_s *inf, int port, u8 val)
{
	u8 reg;
	int err = 0;

	reg = (REG_I2C_SLV0_REG + (port * 3));
	if (val != inf->hw.i2c_slv_reg[port]) {
		err = inv_i2c_single_write(inf, reg, val);
		if (!err)
			inf->hw.i2c_slv_reg[port] = val;
	}
	return err;
}

/* Register I2C_SLV0_CTRL (0x27) */
/* Register I2C_SLV1_CTRL (0x2A) */
/* Register I2C_SLV2_CTRL (0x2D) */
/* Register I2C_SLV3_CTRL (0x30) */
static int nvi_i2c_slv_ctrl_wr(struct inv_gyro_state_s *inf, int port, u8 val)
{
	u8 reg;
	int err = 0;

	reg = (REG_I2C_SLV0_CTRL + (port * 3));
	if (val != inf->hw.i2c_slv_ctrl[port]) {
		err = inv_i2c_single_write(inf, reg, val);
		if (!err) {
			inf->hw.i2c_slv_ctrl[port] = val;
			err = 1; /* flag change made */
		}
	}
	return err;
}

/* Register I2C_SLV4_CTRL (0x34) */
static int nvi_i2c_slv4_ctrl_wr(struct inv_gyro_state_s *inf, bool slv4_en)
{
	u8 val;
	int err = 0;

	val = inf->aux.delay_hw;
	val |= (inf->aux.port[AUX_PORT_SPECIAL].nmp.ctrl &
							 BITS_I2C_SLV_REG_DIS);
	if (slv4_en)
		val |= BIT_SLV_EN;
	if (val != inf->hw.i2c_slv4_ctrl) {
		err = inv_i2c_single_write(inf, REG_I2C_SLV4_CTRL, val);
		if (!err) {
			inf->hw.i2c_slv4_ctrl = val;
			err = 1; /* flag change made */
		}
	}
	return err;
}

/* Register INT_PIN_CFG (0x37) */
static int nvi_int_pin_cfg_wr(struct inv_gyro_state_s *inf, u8 val)
{
	int err = 0;

	if (val != inf->hw.int_pin_cfg) {
		err = inv_i2c_single_write(inf, REG_INT_PIN_CFG, val);
		if (!err)
			inf->hw.int_pin_cfg = val;
	}
	return err;
}

/* Register INT_ENABLE (0x38) */
int nvi_int_enable_wr(struct inv_gyro_state_s *inf, bool enable)
{
	u8 int_enable = 0;
	int err = 0;

	if (enable && (!(inf->hw.pwr_mgmt_1 & BIT_SLEEP))) {
		if ((inf->hw.user_ctrl & BIT_I2C_MST_EN) ||
				 ((~inf->hw.pwr_mgmt_2) & BIT_PWR_GYRO_STBY)) {
			int_enable = BIT_DATA_RDY_EN;
		} else if (inf->chip_type > INV_MPU3050) {
			if ((~inf->hw.pwr_mgmt_2) & BIT_PWR_ACCL_STBY) {
				if (inf->mot_det_en) {
					int_enable = BIT_MOT_EN;
					if (inf->chip_config.mot_enable ==
								   NVI_MOT_DBG)
						pr_info("%s HW motion on\n",
							__func__);
				} else {
					int_enable = BIT_DATA_RDY_EN;
				}
			} else if (!(inf->hw.pwr_mgmt_1 & BIT_TEMP_DIS)) {
				int_enable = BIT_DATA_RDY_EN;
			}
		}
		if ((inf->hw.user_ctrl & BIT_FIFO_EN) &&
						  (!inf->chip_config.fifo_thr))
			int_enable = BIT_FIFO_OVERFLOW;
	}
	if ((int_enable != inf->hw.int_enable) && (inf->pm > NVI_PM_OFF)) {
		err = inv_i2c_single_write(inf,
					   inf->reg->int_enable, int_enable);
		if (!err) {
			inf->hw.int_enable = int_enable;
			if (inf->dbg & NVI_DBG_SPEW_MSG)
				dev_info(&inf->i2c->dev, "%s: %x\n",
					 __func__, int_enable);
		}
	}
	if (!enable)
		synchronize_irq(inf->i2c->irq);
	return err;
}

/* Register I2C_SLV0_CTRL (0x63) */
/* Register I2C_SLV1_CTRL (0x64) */
/* Register I2C_SLV2_CTRL (0x65) */
/* Register I2C_SLV3_CTRL (0x66) */
/* Register I2C_SLV4_CTRL (0x33) */
static int nvi_i2c_slv_do_wr(struct inv_gyro_state_s *inf,
			     int port, u8 data_out)
{
	u8 *hw;
	u8 reg;
	int err = 0;

	if (port == AUX_PORT_SPECIAL) {
		hw = &inf->hw.i2c_slv4_do;
		reg = REG_I2C_SLV4_DO;
	} else {
		hw = &inf->hw.i2c_slv_do[port];
		reg = REG_I2C_SLV0_DO + port;
	}
	if (data_out != *hw) {
		err = inv_i2c_single_write(inf, reg, data_out);
		if (!err)
			*hw = data_out;
	}
	return err;
}

/* Register I2C_MST_DELAY_CTRL (0x67) */
static int nvi_i2c_mst_delay_ctrl_wr(struct inv_gyro_state_s *inf,
				     u8 i2c_mst_delay_ctrl)
{
	int err = 0;

	if (i2c_mst_delay_ctrl != inf->hw.i2c_mst_delay_ctrl) {
		err = inv_i2c_single_write(inf, REG_I2C_MST_DELAY_CTRL,
					   i2c_mst_delay_ctrl);
		if (!err)
			inf->hw.i2c_mst_delay_ctrl = i2c_mst_delay_ctrl;
	}
	return err;
}

/* Register MOT_DETECT_CTRL (0x69) */
static int nvi_mot_detect_ctrl_wr(struct inv_gyro_state_s *inf, u8 val)
{
	int err = 0;

	if (val != inf->hw.mot_detect_ctrl) {
		err = inv_i2c_single_write(inf, REG_MOT_DETECT_CTRL, val);
		if (!err)
			inf->hw.mot_detect_ctrl = val;
	}
	return err;
}

/* Register USER_CTRL (0x6A) */
int nvi_user_ctrl_reset_wr(struct inv_gyro_state_s *inf, u8 val)
{
	int i;
	int err;
	int err_t;

	err_t =  inv_i2c_single_write(inf, inf->reg->user_ctrl, val);
	for (i = 0; i < POWER_UP_TIME; i++) {
		val = -1;
		err = inv_i2c_read(inf, inf->reg->user_ctrl, 1, &val);
		if (!(val & (inf->reg->fifo_reset | inf->reg->i2c_mst_reset)))
			break;

		mdelay(1);
	}
	err_t |= err;
	inf->hw.user_ctrl = val;
	return err_t;
}

/* Register USER_CTRL (0x6A) */
int nvi_user_ctrl_en_wr(struct inv_gyro_state_s *inf, u8 val)
{
	int err = 0;

	if (val != inf->hw.user_ctrl) {
		err = inv_i2c_single_write(inf, inf->reg->user_ctrl, val);
		if (!err) {
			inf->hw.user_ctrl = val;
			dev_dbg(&inf->i2c->dev, "%s: %x\n", __func__, val);
		} else {
			dev_err(&inf->i2c->dev, "%s: %x=>%x ERR\n",
				__func__, inf->hw.user_ctrl, val);
		}
	}
	return err;
}

int nvi_user_ctrl_en(struct inv_gyro_state_s *inf,
		     bool fifo_enable, bool i2c_enable)
{
	u8 val;
	u16 fifo_sample_size;
	bool en;
	int i;
	int err;

	dev_dbg(&inf->i2c->dev, "%s: FIFO=%x I2C=%x\n",
		__func__, fifo_enable, i2c_enable);
	val = 0;
	fifo_sample_size = 0;
	inf->fifo_sample_size = 0;
	en = false;
	if (fifo_enable) {
		if (inf->chip_type == INV_MPU3050) {
			val |= BIT_3050_FIFO_FOOTER;
			fifo_sample_size += 2;
		}
		if (inf->chip_config.accl_fifo_enable) {
			val |= inf->reg->accl_fifo_en;
			fifo_sample_size += 6;
		}
		if (inf->chip_config.temp_enable &&
					   inf->chip_config.temp_fifo_enable) {
			val |= BIT_TEMP_FIFO_EN;
			fifo_sample_size += 2;
		}
		if (inf->chip_config.gyro_fifo_enable) {
			val |= (inf->chip_config.gyro_fifo_enable << 4);
			if (val & BIT_GYRO_XOUT)
				fifo_sample_size += 2;
			if (val & BIT_GYRO_YOUT)
				fifo_sample_size += 2;
			if (val & BIT_GYRO_ZOUT)
				fifo_sample_size += 2;
		}
		for (i = 0; i < AUX_PORT_SPECIAL; i++) {
			if (inf->aux.port[i].fifo_en &&
				  (inf->aux.port[i].nmp.addr & BIT_I2C_READ) &&
				      (inf->hw.i2c_slv_ctrl[i] & BIT_SLV_EN)) {
				if (i == 3)
					en = true;
				else
					val |= (1 << i);
				fifo_sample_size += inf->aux.port[i].nmp.ctrl &
						    BITS_I2C_SLV_CTRL_LEN;
			}
		}
		err = nvi_i2c_mst_ctrl_wr(inf, en);
		if (val || en)
			en = true;
		else
			en = false;
		inf->fifo_sample_size = fifo_sample_size;
	} else {
		err = nvi_i2c_mst_ctrl_wr(inf, false);
	}
	err |= nvi_fifo_en_wr(inf, val);
	val = 0;
	if (fifo_enable && en)
		val |= BIT_FIFO_EN;
	if (i2c_enable && (inf->aux.enable || inf->aux.en3050))
		val |= BIT_I2C_MST_EN;
	err |= nvi_user_ctrl_en_wr(inf, val);
	return err;
}

/* Register PWR_MGMT_1 (0x6B) */
static int nvi_pwr_mgmt_1_war(struct inv_gyro_state_s *inf)
{
	u8 val;
	int i;
	int err;

	for (i = 0; i < (POWER_UP_TIME / REG_UP_TIME); i++) {
		inv_i2c_single_write(inf, inf->reg->pwr_mgmt_1, 0);
		mdelay(REG_UP_TIME);
		val = -1;
		err = inv_i2c_read(inf, inf->reg->pwr_mgmt_1, 1, &val);
		if ((!err) && (!val))
			break;
	}
	inf->hw.pwr_mgmt_1 = val;
	return err;
}

/* Register PWR_MGMT_1 (0x6B) */
static int nvi_pwr_mgmt_1_wr(struct inv_gyro_state_s *inf, u8 pwr_mgmt_1)
{
	unsigned int i;
	int err = 0;

	if (pwr_mgmt_1 != inf->hw.pwr_mgmt_1) {
		err = inv_i2c_single_write(inf, inf->reg->pwr_mgmt_1,
					   pwr_mgmt_1);
		if (!err) {
			if (pwr_mgmt_1 & BIT_H_RESET) {
				memset(&inf->hw, 0, sizeof(struct nvi_hw));
				inf->sample_delay_us = 0;
				for (i = 0; i < (POWER_UP_TIME / REG_UP_TIME);
									 i++) {
					mdelay(REG_UP_TIME);
					pwr_mgmt_1 = -1;
					err = inv_i2c_read(inf,
							  inf->reg->pwr_mgmt_1,
							   1, &pwr_mgmt_1);
					if ((!err) &&
						 (!(pwr_mgmt_1 & BIT_H_RESET)))
						break;
				}
			}
			inf->hw.pwr_mgmt_1 = pwr_mgmt_1;
		}
	}
	return err;
}

/* Register PWR_MGMT_2 (0x6C) */
static int nvi_pwr_mgmt_2_wr(struct inv_gyro_state_s *inf, u8 pwr_mgmt_2)
{
	int err = 0;

	if (pwr_mgmt_2 != inf->hw.pwr_mgmt_2) {
		err = inv_i2c_single_write(inf, inf->reg->pwr_mgmt_2,
					   pwr_mgmt_2);
		if (!err)
			inf->hw.pwr_mgmt_2 = pwr_mgmt_2;
	}
	return err;
}

static int nvi_motion_detect_enable(struct inv_gyro_state_s *inf, u8 mot_thr)
{
	int err;
	int err_t = 0;

	if (inf->chip_type != INV_MPU6050)
		return 0;

	if (mot_thr) {
		err = nvi_accel_config_wr(inf, 0,
					  inf->chip_config.accl_fsr, 0);
		if (err < 0)
			err_t |= err;
		err = nvi_config_wr(inf, 0);
		if (err < 0)
			err_t |= err;
		err_t |= nvi_mot_dur_wr(inf, inf->chip_config.mot_dur);
		err_t |= nvi_mot_detect_ctrl_wr(inf,
						inf->chip_config.mot_ctrl);
		err_t |= nvi_mot_thr_wr(inf, mot_thr);
		mdelay(5);
		err = nvi_accel_config_wr(inf, 0,
					  inf->chip_config.accl_fsr, 7);
		if (err < 0)
			err_t |= err;
		if (err_t)
			nvi_accel_config_wr(inf, 0,
					    inf->chip_config.accl_fsr, 0);
	} else {
		err = nvi_accel_config_wr(inf, 0,
					  inf->chip_config.accl_fsr, 0);
		if (err < 0)
			err_t |= err;
	}
	return err_t;
}

static int nvi_vreg_dis(struct inv_gyro_state_s *st, unsigned int i)
{
	int ret = 0;

	if (st->vreg[i].ret && (st->vreg[i].consumer != NULL)) {
		ret = regulator_disable(st->vreg[i].consumer);
		if (!ret) {
			st->vreg[i].ret = 0;
			dev_dbg(&st->i2c->dev, "%s %s\n",
				__func__, st->vreg[i].supply);
		} else {
			dev_err(&st->i2c->dev, "%s %s ERR\n",
				__func__, st->vreg[i].supply);
		}
	}
	return ret;
}

static int nvi_vreg_dis_all(struct inv_gyro_state_s *st)
{
	unsigned int i;
	int ret = 0;

	for (i = ARRAY_SIZE(nvi_vregs); i > 0; i--)
		ret |= nvi_vreg_dis(st, (i - 1));
	return ret;
}

static int nvi_vreg_en(struct inv_gyro_state_s *st, unsigned int i)
{
	int ret = 0;

	if ((!st->vreg[i].ret) && (st->vreg[i].consumer != NULL)) {
		ret = regulator_enable(st->vreg[i].consumer);
		if (!ret) {
			st->vreg[i].ret = 1;
			dev_dbg(&st->i2c->dev, "%s %s\n",
				__func__, st->vreg[i].supply);
			ret = 1; /* flag regulator state change */
		} else {
			dev_err(&st->i2c->dev, "%s %s ERR\n",
				__func__, st->vreg[i].supply);
		}
	}
	return ret;
}

static int nvi_vreg_en_all(struct inv_gyro_state_s *st)
{
	unsigned i;
	int ret = 0;

	for (i = 0; i < ARRAY_SIZE(nvi_vregs); i++)
		ret |= nvi_vreg_en(st, i);
	return ret;
}

static void nvi_vreg_exit(struct inv_gyro_state_s *st)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(nvi_vregs); i++) {
		if (st->vreg[i].consumer != NULL) {
			regulator_unregister_notifier(st->vreg[i].consumer,
						      &st->nb_vreg[i]);
			devm_regulator_put(st->vreg[i].consumer);
			st->vreg[i].consumer = NULL;
			dev_dbg(&st->i2c->dev, "%s %s\n",
				__func__, st->vreg[i].supply);
		}
	}
}

static int nvi_vreg_init(struct inv_gyro_state_s *st)
{
	unsigned int i;
	int ret = 0;

	for (i = 0; i < ARRAY_SIZE(nvi_vregs); i++) {
		st->vreg[i].supply = nvi_vregs[i];
		st->vreg[i].ret = 0;
		st->vreg[i].consumer = devm_regulator_get(&st->i2c->dev,
							  st->vreg[i].supply);
		if (IS_ERR(st->vreg[i].consumer)) {
			ret = PTR_ERR(st->vreg[i].consumer);
			dev_err(&st->i2c->dev, "%s ERR %d for %s\n",
				__func__, ret, st->vreg[i].supply);
			st->vreg_en_ts[i] = nvi_ts_ns();
			st->vreg[i].consumer = NULL;
		} else {
			ret = regulator_is_enabled(st->vreg[i].consumer);
			if (ret > 0)
				st->vreg_en_ts[i] = nvi_ts_ns();
			else
				st->vreg_en_ts[i] = 0;
			st->nb_vreg[i].notifier_call = nvi_nb_vreg_pf[i];
			ret = regulator_register_notifier(st->vreg[i].consumer,
							  &st->nb_vreg[i]);
			dev_dbg(&st->i2c->dev, "%s %s enable_ts=%lld\n",
				__func__, st->vreg[i].supply,
				st->vreg_en_ts[i]);
		}
	}
	return ret;
}

static int nvi_nb_vreg(struct inv_gyro_state_s *st,
		       unsigned long event, unsigned int i)
{
	if (event & REGULATOR_EVENT_POST_ENABLE) {
		st->vreg_en_ts[i] = nvi_ts_ns();
	} else if (event & (REGULATOR_EVENT_DISABLE |
			    REGULATOR_EVENT_FORCE_DISABLE)) {
		st->vreg_en_ts[i] = 0;
	}
	if (st->dbg & NVI_DBG_SPEW_MSG)
		dev_info(&st->i2c->dev, "%s %s event=0x%x ts=%lld\n",
			 __func__, st->vreg[i].supply, (unsigned int)event,
			 st->vreg_en_ts[i]);
	return NOTIFY_OK;
}

int nvi_pm_wr(struct inv_gyro_state_s *st,
	      u8 pwr_mgmt_1, u8 pwr_mgmt_2, u8 lpa)
{
	s64 por_ns;
	unsigned int delay_ms;
	unsigned int i;
	int ret;
	int ret_t = 0;

	ret = nvi_vreg_en_all(st);
	if (ret) {
		delay_ms = 0;
		for (i = 0; i < ARRAY_SIZE(nvi_vregs); i++) {
			por_ns = nvi_ts_ns() - st->vreg_en_ts[i];
			if ((por_ns < 0) || (!st->vreg_en_ts[i])) {
				delay_ms = (POR_MS * 1000000);
				break;
			}

			if (por_ns < (POR_MS * 1000000)) {
				por_ns = (POR_MS * 1000000) - por_ns;
				if (por_ns > delay_ms)
					delay_ms = (unsigned int)por_ns;
			}
		}
		delay_ms /= 1000000;
		if (st->dbg & NVI_DBG_SPEW_MSG)
			dev_info(&st->i2c->dev, "%s %ums delay\n",
				 __func__, delay_ms);
		if (delay_ms)
			msleep(delay_ms);
		ret_t |= nvi_pwr_mgmt_1_war(st);
		ret_t |= nvi_pwr_mgmt_1_wr(st, BIT_H_RESET);
		ret_t |= nvi_pwr_mgmt_1_war(st);
		ret_t |= nvi_user_ctrl_reset_wr(st, (st->reg->fifo_reset |
						     st->reg->i2c_mst_reset));
	} else {
		ret_t |= nvi_pwr_mgmt_1_war(st);
	}
	switch (st->chip_type) {
	case INV_MPU3050:
		pwr_mgmt_1 &= (BIT_SLEEP | INV_CLK_PLL);
		if (pwr_mgmt_1 & INV_CLK_PLL) {
			ret = nvi_pwr_mgmt_1_wr(st, (BITS_3050_POWER1 |
						      INV_CLK_PLL));
			ret |= nvi_pwr_mgmt_1_wr(st, (BITS_3050_POWER2 |
						       INV_CLK_PLL));
			ret |= nvi_pwr_mgmt_1_wr(st, INV_CLK_PLL);
		} else {
			pwr_mgmt_1 |= (pwr_mgmt_2 & 0x07) << 3;
			ret = nvi_pwr_mgmt_1_wr(st, pwr_mgmt_1);
		}
		if (ret)
			ret_t |= ret;
		else
			st->hw.pwr_mgmt_2 = pwr_mgmt_2;
		break;

	case INV_MPU6050:
		pwr_mgmt_2 |= lpa << 6;
		ret = nvi_pwr_mgmt_2_wr(st, pwr_mgmt_2);
		if (ret)
			ret_t |= ret;
		else
			st->hw.lposc_clksel = lpa;
		ret_t |= nvi_pwr_mgmt_1_wr(st, pwr_mgmt_1);
		break;

	default: /* INV_MPU65XX */
		ret_t |= nvi_lp_accel_odr_wr(st, lpa);
		ret_t |= nvi_pwr_mgmt_2_wr(st, pwr_mgmt_2);
		ret_t |= nvi_pwr_mgmt_1_wr(st, pwr_mgmt_1);
		break;
	}

	return ret_t;
}

static int nvi_reset(struct inv_gyro_state_s *inf,
		     bool reset_fifo, bool reset_i2c);

/**
 * @param inf
 * @param pm_req: call with one of the following:
 *      NVI_PM_OFF_FORCE = force off state
 *      NVI_PM_ON = minimum power for device access
 *      NVI_PM_ON_FULL = power for gyro
 *      NVI_PM_AUTO = automatically sets power for configuration
 *      Typical use is to set needed power for configuration and
 *      then call with NVI_PM_AUTO when done.
 *      All other NVI_PM_ levels are handled automatically and
 *      are for internal use.
 * @return int: returns 0 for success or error code
 */
static int nvi_pm(struct inv_gyro_state_s *st, int pm_req)
{
	bool irq;
	u8 pwr_mgmt_1;
	u8 pwr_mgmt_2;
	u8 lpa;
	int pm;
	int ret = 0;
	BUG_ON(!mutex_is_locked(&st->mutex));

	lpa = st->hw.lposc_clksel;
	if ((pm_req == NVI_PM_OFF_FORCE) || st->suspend) {
		pwr_mgmt_1 = BIT_SLEEP | st->reg->temp_dis;
		pwr_mgmt_2 = (BIT_PWR_ACCL_STBY | BIT_PWR_GYRO_STBY);
		pm = NVI_PM_OFF;
	} else {
		pwr_mgmt_2 = (((~st->chip_config.accl_enable) << 3) &
			      BIT_PWR_ACCL_STBY);
		pwr_mgmt_2 |= ((~st->chip_config.gyro_enable) &
			       BIT_PWR_GYRO_STBY);
		if (st->chip_config.gyro_enable ||
				 (st->chip_config.temp_enable & NVI_TEMP_EN) ||
					 (st->hw.user_ctrl & BIT_I2C_MST_EN)) {
			if (st->chip_config.gyro_enable)
				pm = NVI_PM_ON_FULL;
			else
				pm = NVI_PM_ON;
		} else if (st->chip_config.accl_enable) {
			if (st->mot_det_en ||
					      (st->chip_config.lpa_delay_us &&
					       st->hal.lpa_tbl_n &&
					      (st->chip_config.accl_delay_us >=
					      st->chip_config.lpa_delay_us))) {
				for (lpa = 0; lpa < st->hal.lpa_tbl_n; lpa++) {
					if (st->chip_config.accl_delay_us >=
							  st->hal.lpa_tbl[lpa])
						break;
				}
				pm = NVI_PM_ON_CYCLE;
			} else {
				pm = NVI_PM_ON;
			}
		} else if (st->chip_config.enable || st->aux.bypass_lock) {
			pm = NVI_PM_STDBY;
		} else {
			pm = NVI_PM_OFF;
		}
		if (pm_req > pm)
			pm = pm_req;
		switch (pm) {
		case NVI_PM_OFF:
		case NVI_PM_STDBY:
			pwr_mgmt_1 = BIT_SLEEP;
			break;

		case NVI_PM_ON_CYCLE:
			pwr_mgmt_1 = st->reg->cycle;
			break;

		case NVI_PM_ON:
			pwr_mgmt_1 = INV_CLK_INTERNAL;
			break;

		case NVI_PM_ON_FULL:
			pwr_mgmt_1 = INV_CLK_PLL;
			/* gyro must be turned on before going to PLL clock */
			pwr_mgmt_2 &= ~BIT_PWR_GYRO_STBY;
			break;

		default:
			dev_err(&st->i2c->dev, "%s %d=>%d ERR=EINVAL\n",
				__func__, st->pm, pm);
			return -EINVAL;
		}

		if (!st->chip_config.temp_enable)
			pwr_mgmt_1 |= st->reg->temp_dis;
	}

	if ((pm != st->pm) || (lpa != st->hw.lposc_clksel) ||
		   (pwr_mgmt_1 != st->hw.pwr_mgmt_1) ||
		   (pwr_mgmt_2 != (st->hw.pwr_mgmt_2 &
				   (BIT_PWR_ACCL_STBY | BIT_PWR_GYRO_STBY)))) {
		nvi_int_enable_wr(st, false);
		st->gyro_start_ts = 0;
		if (pm == NVI_PM_OFF) {
			switch (st->pm) {
			case NVI_PM_STDBY:
			case NVI_PM_ERR:
			case NVI_PM_OFF_FORCE:
				break;
			default:
				nvi_reset(st, true, false);
				break;
			}
		}
		if ((!(st->hw.pwr_mgmt_1 & (BIT_SLEEP | st->reg->cycle))) &&
			     (pm < NVI_PM_ON) && (st->pm > NVI_PM_ON_CYCLE)) {
			/* tasks that need access before low power state */
			if (pm_req == NVI_PM_AUTO)
				/* turn off FIFO and I2C */
				nvi_user_ctrl_en(st, false, false);
		}
		if (pm == NVI_PM_OFF) {
			if (st->pm > NVI_PM_OFF) {
				ret |= nvi_pwr_mgmt_1_war(st);
				ret |= nvi_pwr_mgmt_1_wr(st, BIT_H_RESET);
			}
			ret |= nvi_pm_wr(st, pwr_mgmt_1, pwr_mgmt_2, lpa);
			ret |= nvi_vreg_dis_all(st);
		} else {
			ret |= nvi_pm_wr(st, pwr_mgmt_1, pwr_mgmt_2, lpa);
			if (pm > NVI_PM_STDBY)
				mdelay(REG_UP_TIME);
		}
		if (ret < 0) {
			dev_err(&st->i2c->dev, "%s %d=>%d ERR=%d\n",
				__func__, st->pm, pm, ret);
			pm = NVI_PM_ERR;
		}
		if (st->dbg & NVI_DBG_SPEW_MSG)
			dev_info(&st->i2c->dev, "%s %d=>%d PM2=%x LPA=%x\n",
				 __func__, st->pm, pm, pwr_mgmt_2, lpa);
		st->pm = pm;
		if (ret > 0)
			ret = 0;
	}
	if (pm_req == NVI_PM_AUTO) {
		if (pm > NVI_PM_STDBY)
			irq = true;
		if (pm > NVI_PM_ON_CYCLE)
			nvi_user_ctrl_en(st, true, true);
		if ((pm == NVI_PM_ON_FULL) && (!st->gyro_start_ts))
			st->gyro_start_ts = nvi_ts_ns() +
					   st->chip_config.gyro_start_delay_ns;
	} else {
		irq = false;
	}
	nvi_int_enable_wr(st, irq);
	return ret;
}

static void nvi_pm_exit(struct inv_gyro_state_s *st)
{
	mutex_lock(&st->mutex);
	nvi_pm(st, NVI_PM_OFF_FORCE);
	mutex_unlock(&st->mutex);
	nvi_vreg_exit(st);
}

static int nvi_pm_init(struct inv_gyro_state_s *st)
{
	int ret = 0;

	nvi_vreg_init(st);
	st->pm = NVI_PM_ERR;
	mutex_lock(&st->mutex);
	ret = nvi_pm(st, NVI_PM_ON);
	mutex_unlock(&st->mutex);
	return ret;
}

static int nvi_aux_delay(struct inv_gyro_state_s *inf,
			 int port, unsigned int delay_ms)
{
	struct aux_port *ap;
	u8 val;
	u8 i;
	unsigned int delay_new;
	int delay_rtn;

	if (port != AUX_PORT_BYPASS)
		inf->aux.port[port].nmp.delay_ms = delay_ms;
	/* determine valid delays by ports enabled */
	delay_new = 0;
	delay_rtn = 0;
	for (i = 0; i < AUX_PORT_SPECIAL; i++) {
		ap = &inf->aux.port[i];
		if (delay_rtn < ap->nmp.delay_ms)
			delay_rtn = ap->nmp.delay_ms;
		if (inf->hw.i2c_slv_ctrl[i] & BIT_SLV_EN) {
			if (delay_new < ap->nmp.delay_ms)
				delay_new = ap->nmp.delay_ms;
		}
	}
	ap = &inf->aux.port[AUX_PORT_SPECIAL];
	if (delay_rtn < ap->nmp.delay_ms)
		delay_rtn = ap->nmp.delay_ms;
	if (inf->hw.i2c_slv4_ctrl & BIT_SLV_EN) {
		if (delay_new < ap->nmp.delay_ms)
			delay_new = ap->nmp.delay_ms;
	}
	if (!(inf->hw.user_ctrl & BIT_I2C_MST_EN)) {
		/* delay will execute when re-enabled */
		if (delay_ms)
			return delay_rtn;
		else
			return 0;
	}

	/* HW global delay */
	delay_new *= 1000;
	delay_new /= inf->sample_delay_us;
	delay_new++;
	inf->aux.delay_hw = (u8)delay_new;
	nvi_i2c_slv4_ctrl_wr(inf, (bool)(inf->hw.i2c_slv4_ctrl & BIT_SLV_EN));
	/* HW port delay enable */
	val = BIT_DELAY_ES_SHADOW;
	for (i = 0; i < AUX_PORT_MAX; i++) {
		ap = &inf->aux.port[i];
		if (ap->nmp.delay_ms)
			val |= (1 << i);
	}
	nvi_i2c_mst_delay_ctrl_wr(inf, val);
	if (delay_ms)
		return delay_rtn;
	else
		return 0;
}

static int nvi_global_delay(struct inv_gyro_state_s *inf)
{
	unsigned long delay_us;
	unsigned long delay_us_old;
	unsigned long fs_hz;
	u8 dlpf;
	u8 smplrt_div;
	int i;
	int err;
	int err_t = 0;

	/* find the fastest polling of all the devices */
	delay_us = -1;
	for (i = 0; i < AUX_PORT_MAX; i++) {
		if (inf->aux.port[i].enable && inf->aux.port[i].nmp.delay_us) {
			if (inf->aux.port[i].nmp.delay_us < delay_us)
				delay_us = inf->aux.port[i].nmp.delay_us;
		}
	}
	if (inf->chip_config.gyro_enable && inf->chip_config.gyro_delay_us) {
		if (inf->chip_config.gyro_delay_us < delay_us)
			delay_us = inf->chip_config.gyro_delay_us;
	}
	if (inf->chip_config.accl_enable && inf->chip_config.accl_delay_us) {
		if (inf->chip_config.accl_delay_us < delay_us)
			delay_us = inf->chip_config.accl_delay_us;
	}
	if (delay_us == -1)
		delay_us = NVI_DELAY_DEFAULT; /* default if nothing found */
	/* set the limits */
	if (delay_us < inf->chip_config.min_delay_us)
		delay_us = inf->chip_config.min_delay_us;
	if (delay_us > NVI_DELAY_US_MAX)
		delay_us = NVI_DELAY_US_MAX;
	delay_us_old = inf->sample_delay_us;
	inf->sample_delay_us = delay_us;
	delay_us <<= 1;
	for (dlpf = 0; dlpf < ARRAY_SIZE(nvi_lpf_us_tbl); dlpf++) {
		if (delay_us < nvi_lpf_us_tbl[dlpf])
			break;
	}
	if (dlpf)
		fs_hz = 1000;
	else
		fs_hz = 8000;
	smplrt_div = inf->sample_delay_us / fs_hz - 1;
	dlpf |= (inf->hw.config & 0xF8);
	fs_hz = 1000000 / inf->sample_delay_us;
	if ((smplrt_div != inf->hw.smplrt_div) || (dlpf != inf->hw.config)) {
		if (inf->dbg)
			dev_info(&inf->i2c->dev, "%s %lu\n",
				 __func__, delay_us);
		if (inf->sample_delay_us < delay_us_old) {
			/* go faster */
			if (inf->chip_type == INV_MPU3050) {
				if (inf->mpu_slave != NULL)
					inf->mpu_slave->set_lpf(inf, fs_hz);
				dlpf |= (inf->hw.config & 0xE7);
			} else {
				nvi_aux_delay(inf, AUX_PORT_BYPASS, 0);
			}
			err = nvi_config_wr(inf, dlpf);
			if (err < 0)
				err_t |= err;
			err_t |= nvi_smplrt_div_wr(inf, smplrt_div);
		} else {
			/* go slower */
			err_t |= nvi_smplrt_div_wr(inf, smplrt_div);
			if (inf->chip_type == INV_MPU3050) {
				if (inf->mpu_slave != NULL)
					inf->mpu_slave->set_lpf(inf, fs_hz);
				dlpf |= (inf->hw.config & 0xE7);
				err = nvi_config_wr(inf, dlpf);
			} else {
				err = nvi_config_wr(inf, dlpf);
				nvi_aux_delay(inf, AUX_PORT_BYPASS, 0);
			}
			if (err < 0)
				err_t |= err;
		}
	}
	return err_t;
}

static void nvi_aux_dbg(struct inv_gyro_state_s *inf, char *tag, int val)
{
	struct nvi_mpu_port *n;
	struct aux_port *p;
	struct aux_ports *a;
	u8 data[4];
	int i;

	if (!(inf->dbg & NVI_DBG_SPEW_AUX))
		return;

	dev_info(&inf->i2c->dev, "%s %s %d\n", __func__, tag, val);
	for (i = 0; i < AUX_PORT_MAX; i++) {
		inv_i2c_read(inf, (REG_I2C_SLV0_ADDR + (i * 3)), 3, data);
		inv_i2c_read(inf, (REG_I2C_SLV0_DO + i), 1, &data[3]);
		/* HW = hardware */
		pr_info("HW: P%d AD=%x RG=%x CL=%x DO=%x\n",
			i, data[0], data[1], data[2], data[3]);
		n = &inf->aux.port[i].nmp;
		/* NS = nmp structure */
		pr_info("NS: P%d AD=%x RG=%x CL=%x DO=%x MS=%u US=%lu SB=%x\n",
			i, n->addr, n->reg, n->ctrl, n->data_out, n->delay_ms,
			n->delay_us, n->shutdown_bypass);
		p = &inf->aux.port[i];
		/* PS = port structure */
		pr_info("PS: P%d OF=%u EN=%x FE=%x HD=%x\n", i,
			p->ext_data_offset, p->enable, p->fifo_en, p->hw_do);
	}
	a = &inf->aux;
	pr_info("AUX: EN=%x GE=%x MD=%x GD=%lu DN=%u BE=%x BL=%d MX=%d\n",
		a->enable, (bool)(inf->hw.user_ctrl & BIT_I2C_MST_EN),
		(inf->hw.i2c_slv4_ctrl & BITS_I2C_MST_DLY),
		inf->sample_delay_us, a->ext_data_n,
		(inf->hw.int_pin_cfg & BIT_BYPASS_EN), a->bypass_lock,
		atomic_read(&inf->mutex.count));
}

static void nvi_aux_read(struct inv_gyro_state_s *inf)
{
	struct aux_port *ap;
	s64 timestamp1;
	s64 timestamp2;
	unsigned int i;
	unsigned int len;
	u8 *p;
	int err;

	if ((inf->aux.ext_data_n == 0) ||
				       (!(inf->hw.user_ctrl & BIT_I2C_MST_EN)))
		return;

	timestamp1 = nvi_ts_ns();
	err = inv_i2c_read(inf, REG_EXT_SENS_DATA_00,
			   inf->aux.ext_data_n,
			   (unsigned char *)&inf->aux.ext_data);
	if (err)
		return;

	timestamp2 = nvi_ts_ns();
	timestamp1 = timestamp1 + ((timestamp2 - timestamp1) >> 1);
	for (i = 0; i < AUX_PORT_SPECIAL; i++) {
		ap = &inf->aux.port[i];
		if ((inf->hw.i2c_slv_ctrl[i] & BIT_SLV_EN) && (!ap->fifo_en) &&
					       (ap->nmp.addr & BIT_I2C_READ) &&
						   (ap->nmp.handler != NULL)) {
			p = &inf->aux.ext_data[ap->ext_data_offset];
			len = ap->nmp.ctrl & BITS_I2C_SLV_CTRL_LEN;
			ap->nmp.handler(p, len, timestamp1,
					ap->nmp.ext_driver);
		}
	}
}

static void nvi_aux_ext_data_offset(struct inv_gyro_state_s *inf)
{
	int i;
	unsigned short offset;

	offset = 0;
	for (i = 0; i < AUX_PORT_SPECIAL; i++) {
		if ((inf->hw.i2c_slv_ctrl[i] & BIT_SLV_EN) &&
				  (inf->aux.port[i].nmp.addr & BIT_I2C_READ)) {
			inf->aux.port[i].ext_data_offset = offset;
			offset += (inf->aux.port[i].nmp.ctrl &
				   BITS_I2C_SLV_CTRL_LEN);
		}
	}
	if (offset > AUX_EXT_DATA_REG_MAX) {
		offset = AUX_EXT_DATA_REG_MAX;
		dev_err(&inf->i2c->dev,
			"%s ERR MPU slaves exceed data storage\n", __func__);
	}
	inf->aux.ext_data_n = offset;
	return;
}

static int nvi_aux_port_data_out(struct inv_gyro_state_s *inf,
				 int port, u8 data_out)
{
	int err;

	err = nvi_i2c_slv_do_wr(inf, port, data_out);
	if (!err) {
		inf->aux.port[port].nmp.data_out = data_out;
		inf->aux.port[port].hw_do = true;
	} else {
		inf->aux.port[port].hw_do = false;
	}
	return err;
}

static int nvi_aux_port_wr(struct inv_gyro_state_s *inf, int port)
{
	struct aux_port *ap;
	int err;

	ap = &inf->aux.port[port];
	err = nvi_i2c_slv_addr_wr(inf, port, ap->nmp.addr);
	err |= nvi_i2c_slv_reg_wr(inf, port, ap->nmp.reg);
	err |= nvi_i2c_slv_do_wr(inf, port, ap->nmp.data_out);
	return err;
}

static int nvi_aux_port_en(struct inv_gyro_state_s *inf,
			   int port, bool en)
{
	struct aux_port *ap;
	u8 val;
	int err = 0;

	inf->aux.ext_data_n = 0;
	ap = &inf->aux.port[port];
	if ((!(inf->hw.i2c_slv_addr[port])) && en) {
		err = nvi_aux_port_wr(inf, port);
		if (!err)
			ap->hw_do = true;
	}
	if ((!ap->hw_do) && en)
		nvi_aux_port_data_out(inf, port, ap->nmp.data_out);
	if (port == AUX_PORT_SPECIAL) {
		err = nvi_i2c_slv4_ctrl_wr(inf, en);
	} else {
		if (en)
			val = (ap->nmp.ctrl | BIT_SLV_EN);
		else
			val = 0;
		err = nvi_i2c_slv_ctrl_wr(inf, port, val);
	}
	if (err > 0) {
		nvi_aux_ext_data_offset(inf);
		err = 0;
	}
	return err;
}

static int nvi_aux_enable(struct inv_gyro_state_s *inf, bool enable)
{
	bool en;
	unsigned int i;
	int err = 0;

	if (inf->hw.int_pin_cfg & BIT_BYPASS_EN)
		enable = false;
	en = false;
	if (enable) {
		/* global enable is honored only if a port is enabled */
		for (i = 0; i < AUX_PORT_MAX; i++) {
			if (inf->aux.port[i].enable) {
				en = true;
				break;
			}
		}
		if (en == (bool)(inf->hw.user_ctrl & BIT_I2C_MST_EN))
			/* if already on then just update delays */
			nvi_global_delay(inf);
	}
	inf->aux.enable = en;
	if ((bool)(inf->hw.user_ctrl & BIT_I2C_MST_EN) == en) {
		if (inf->aux.reset_fifo)
			nvi_reset(inf, true, false);
		return 0;
	}

	if (en) {
		for (i = 0; i < AUX_PORT_MAX; i++) {
			if (inf->aux.port[i].enable)
				err |= nvi_aux_port_en(inf, i, true);
		}
	} else {
		for (i = 0; i < AUX_PORT_MAX; i++) {
			if (inf->hw.i2c_slv_addr[i])
				nvi_aux_port_en(inf, i, false);
		}
	}
	err |= nvi_global_delay(inf);
	if (inf->aux.reset_fifo)
		err |= nvi_reset(inf, true, false);
	else
		err |= nvi_user_ctrl_en(inf, true, en);
	return err;
}

static int nvi_aux_port_enable(struct inv_gyro_state_s *inf,
			       int port, bool enable, bool fifo_enable)
{
	struct aux_port *ap;
	int err;

	ap = &inf->aux.port[port];
	ap->enable = enable;
	if ((!enable) || (!(ap->nmp.addr & BIT_I2C_READ)))
		fifo_enable = false;
	if (ap->fifo_en != fifo_enable)
		inf->aux.reset_fifo = true;
	ap->fifo_en = fifo_enable;
	if (enable && (inf->hw.int_pin_cfg & BIT_BYPASS_EN))
		return 0;

	err = nvi_aux_port_en(inf, port, enable);
	err |= nvi_aux_enable(inf, true);
	return err;
}

static int nvi_reset(struct inv_gyro_state_s *inf,
		     bool reset_fifo, bool reset_i2c)
{
	u8 val;
	unsigned long flags;
	int err;

	dev_dbg(&inf->i2c->dev, "%s FIFO=%x I2C=%x\n",
		__func__, reset_fifo, reset_i2c);
	err = nvi_int_enable_wr(inf, false);
	val = 0;
	if (reset_i2c) {
		inf->aux.reset_i2c = false;
		/* nvi_aux_bypass_enable(inf, false)? */
		err |= nvi_aux_enable(inf, false);
		val |= inf->reg->i2c_mst_reset;
	}
	if (reset_fifo)
		val |= inf->reg->fifo_reset;
	err |= nvi_user_ctrl_en(inf, !reset_fifo, !reset_i2c);
	val |= inf->hw.user_ctrl;
	err |= nvi_user_ctrl_reset_wr(inf, val);
	if (reset_i2c)
		err |= nvi_aux_enable(inf, true);
	err |= nvi_user_ctrl_en(inf, true, true);
	if (reset_fifo && (inf->hw.user_ctrl & BIT_FIFO_EN)) {
		spin_lock_irqsave(&inf->time_stamp_lock, flags);
		kfifo_reset(&inf->trigger.timestamps);
		spin_unlock_irqrestore(&inf->time_stamp_lock, flags);
		inf->fifo_ts = nvi_ts_ns();
		inf->fifo_reset_3050 = true;
	}
	err |= nvi_int_enable_wr(inf, true);
	return err;
}

static int nvi_aux_port_free(struct inv_gyro_state_s *inf, int port)
{
	memset(&inf->aux.port[port], 0, sizeof(struct aux_port));
	if (inf->hw.i2c_slv_addr[port]) {
		nvi_aux_port_wr(inf, port);
		nvi_aux_port_en(inf, port, false);
		nvi_aux_enable(inf, false);
		nvi_aux_enable(inf, true);
		if (port != AUX_PORT_SPECIAL)
			inf->aux.reset_i2c = true;
	}
	return 0;
}

static int nvi_aux_port_alloc(struct inv_gyro_state_s *inf,
			      struct nvi_mpu_port *nmp, int port)
{
	int i;

	if (inf->aux.reset_i2c)
		nvi_reset(inf, false, true);
	if (port < 0) {
		for (i = 0; i < AUX_PORT_SPECIAL; i++) {
			if (inf->aux.port[i].nmp.addr == 0)
				break;
		}
		if (i == AUX_PORT_SPECIAL)
			return -ENODEV;
	} else {
		if (inf->aux.port[port].nmp.addr == 0)
			i = port;
		else
			return -ENODEV;
	}

	memset(&inf->aux.port[i], 0, sizeof(struct aux_port));
	memcpy(&inf->aux.port[i].nmp, nmp, sizeof(struct nvi_mpu_port));
	return i;
}

static int nvi_aux_bypass_enable(struct inv_gyro_state_s *inf, bool enable)
{
	u8 val;
	int err;

	if ((bool)(inf->hw.int_pin_cfg & BIT_BYPASS_EN) == enable)
		return 0;

	val = inf->hw.int_pin_cfg;
	if (enable) {
		err = nvi_aux_enable(inf, false);
		if (!err) {
			val |= BIT_BYPASS_EN;
			err = nvi_int_pin_cfg_wr(inf, val);
		}
	} else {
		val &= ~BIT_BYPASS_EN;
		err = nvi_int_pin_cfg_wr(inf, val);
		if (!err)
			nvi_aux_enable(inf, true);
	}
	return err;
}

static int nvi_aux_bypass_request(struct inv_gyro_state_s *inf, bool enable)
{
	s64 ns;
	s64 to;
	int err = 0;

	if ((bool)(inf->hw.int_pin_cfg & BIT_BYPASS_EN) == enable) {
		inf->aux.bypass_timeout_ns = nvi_ts_ns();
		inf->aux.bypass_lock++;
		if (!inf->aux.bypass_lock)
			dev_err(&inf->i2c->dev, "%s rollover ERR\n", __func__);
	} else {
		if (inf->aux.bypass_lock) {
			ns = nvi_ts_ns() - inf->aux.bypass_timeout_ns;
			to = inf->chip_config.bypass_timeout_ms * 1000000;
			if (ns > to)
				inf->aux.bypass_lock = 0;
			else
				err = -EBUSY;
		}
		if (!inf->aux.bypass_lock) {
			err = nvi_aux_bypass_enable(inf, enable);
			if (err)
				dev_err(&inf->i2c->dev, "%s ERR=%d\n",
					__func__, err);
			else
				inf->aux.bypass_lock++;
		}
	}
	return err;
}

static int nvi_aux_bypass_release(struct inv_gyro_state_s *inf)
{
	int err = 0;

	if (inf->aux.bypass_lock)
		inf->aux.bypass_lock--;
	if (!inf->aux.bypass_lock) {
		err = nvi_aux_bypass_enable(inf, false);
		if (err)
			dev_err(&inf->i2c->dev, "%s ERR=%d\n", __func__, err);
	}
	return err;
}

static int nvi_aux_dev_valid(struct inv_gyro_state_s *inf,
			     struct nvi_mpu_port *nmp, u8 *data)
{
	unsigned char val;
	int i;
	int err;

	/* turn off bypass */
	err = nvi_aux_bypass_request(inf, false);
	if (err)
		return -EBUSY;

	/* grab the special port */
	err = nvi_aux_port_alloc(inf, nmp, AUX_PORT_SPECIAL);
	if (err != AUX_PORT_SPECIAL) {
		nvi_aux_bypass_release(inf);
		return -EBUSY;
	}

	/* enable it */
	inf->aux.port[AUX_PORT_SPECIAL].nmp.delay_ms = 0;
	inf->aux.port[AUX_PORT_SPECIAL].nmp.delay_us = NVI_DELAY_US_MIN;
	err = nvi_aux_port_enable(inf, AUX_PORT_SPECIAL, true, false);
	if (err) {
		nvi_aux_port_free(inf, AUX_PORT_SPECIAL);
		nvi_aux_bypass_release(inf);
		return -EBUSY;
	}

	/* now turn off all the other ports for fastest response */
	for (i = 0; i < AUX_PORT_SPECIAL; i++) {
		if (inf->hw.i2c_slv_addr[i])
			nvi_aux_port_en(inf, i, false);
	}
	/* start reading the results */
	for (i = 0; i < AUX_DEV_VALID_READ_LOOP_MAX; i++) {
		mdelay(AUX_DEV_VALID_READ_DELAY_MS);
		val = 0;
		err = inv_i2c_read(inf, REG_I2C_MST_STATUS, 1, &val);
		if (err)
			continue;

		if (val & 0x50)
			break;
	}
	/* these will restore all previously disabled ports */
	nvi_aux_bypass_release(inf);
	nvi_aux_port_free(inf, AUX_PORT_SPECIAL);
	if (i == AUX_DEV_VALID_READ_LOOP_MAX)
		return -ENODEV;

	if (val & 0x10) /* NACK */
		return -EIO;

	if (nmp->addr & BIT_I2C_READ) {
		err = inv_i2c_read(inf, REG_I2C_SLV4_DI, 1, &val);
		if (err)
			return -EBUSY;

		*data = (u8)val;
		dev_info(&inf->i2c->dev, "%s MPU read 0x%x from device 0x%x\n",
			__func__, val, (nmp->addr & ~BIT_I2C_READ));
	} else {
		dev_info(&inf->i2c->dev, "%s MPU found device 0x%x\n",
			__func__, (nmp->addr & ~BIT_I2C_READ));
	}
	return 0;
}

static int nvi_aux_mpu_call_pre(struct inv_gyro_state_s *inf, int port)
{
	if ((port < 0) || (port >= AUX_PORT_SPECIAL))
		return -EINVAL;

	BUG_ON(!mutex_is_locked(&inf->mutex));
	if (inf->shutdown || inf->suspend)
		return -EPERM;

	if (!inf->aux.port[port].nmp.addr)
		return -EINVAL;

	return 0;
}

static int nvi_aux_mpu_call_post(struct inv_gyro_state_s *inf,
				 char *tag, int err)
{
	BUG_ON(!mutex_is_locked(&inf->mutex));
	if (err < 0)
		err = -EBUSY;
	nvi_aux_dbg(inf, tag, err);
	return err;
}

/* See the mpu.h file for details on the nvi_mpu_ calls.
 */
int nvi_mpu_dev_valid(struct nvi_mpu_port *nmp, u8 *data)
{
	struct inv_gyro_state_s *inf;
	int err;

	inf = inf_local;
	if (inf != NULL) {
		if (inf->dbg & NVI_DBG_SPEW_AUX)
			pr_info("%s\n", __func__);
	} else {
		pr_debug("%s ERR -EAGAIN\n", __func__);
		return -EAGAIN;
	}

	if (nmp == NULL)
		return -EINVAL;

	if ((nmp->addr & BIT_I2C_READ) && (data == NULL))
		return -EINVAL;

	mutex_lock(&inf->mutex);
	if (inf->shutdown || inf->suspend) {
		mutex_unlock(&inf->mutex);
		return -EPERM;
	}

	nvi_pm(inf, NVI_PM_ON);
	err = nvi_aux_dev_valid(inf, nmp, data);
	nvi_pm(inf, NVI_PM_AUTO);
	mutex_unlock(&inf->mutex);
	nvi_aux_dbg(inf, "nvi_mpu_dev_valid err: ", err);
	return err;
}
EXPORT_SYMBOL(nvi_mpu_dev_valid);

int nvi_mpu_port_alloc(struct nvi_mpu_port *nmp)
{
	struct inv_gyro_state_s *inf;
	int err;

	inf = inf_local;
	if (inf != NULL) {
		if (inf->dbg & NVI_DBG_SPEW_AUX)
			pr_info("%s\n", __func__);
	} else {
		pr_debug("%s ERR -EAGAIN\n", __func__);
		return -EAGAIN;
	}

	if (nmp == NULL)
		return -EINVAL;

	if (!(nmp->ctrl & BITS_I2C_SLV_CTRL_LEN))
		return -EINVAL;

	mutex_lock(&inf->mutex);
	if (inf->shutdown || inf->suspend) {
		mutex_unlock(&inf->mutex);
		return -EPERM;
	}

	nvi_pm(inf, NVI_PM_ON);
	err = nvi_aux_port_alloc(inf, nmp, -1);
	nvi_pm(inf, NVI_PM_AUTO);
	err = nvi_aux_mpu_call_post(inf, "nvi_mpu_port_alloc err/port: ", err);
	mutex_unlock(&inf->mutex);
	return err;
}
EXPORT_SYMBOL(nvi_mpu_port_alloc);

int nvi_mpu_port_free(int port)
{
	struct inv_gyro_state_s *inf;
	int err;

	inf = inf_local;
	if (inf != NULL) {
		if (inf->dbg & NVI_DBG_SPEW_AUX)
			pr_info("%s port %d\n", __func__, port);
	} else {
		pr_debug("%s port %d ERR -EAGAIN\n", __func__, port);
		return -EAGAIN;
	}

	mutex_lock(&inf->mutex);
	err = nvi_aux_mpu_call_pre(inf, port);
	if (err) {
		mutex_unlock(&inf->mutex);
		return err;
	}

	nvi_pm(inf, NVI_PM_ON);
	err = nvi_aux_port_free(inf, port);
	nvi_pm(inf, NVI_PM_AUTO);
	err = nvi_aux_mpu_call_post(inf, "nvi_mpu_port_free err: ", err);
	mutex_unlock(&inf->mutex);
	return err;
}
EXPORT_SYMBOL(nvi_mpu_port_free);

int nvi_mpu_enable(int port, bool enable, bool fifo_enable)
{
	struct inv_gyro_state_s *inf;
	int err;

	inf = inf_local;
	if (inf != NULL) {
		if (inf->dbg & NVI_DBG_SPEW_AUX)
			pr_info("%s port %d: %x\n", __func__, port, enable);
	} else {
		pr_debug("%s port %d: %x ERR -EAGAIN\n",
			 __func__, port, enable);
		return -EAGAIN;
	}

	mutex_lock(&inf->mutex);
	err = nvi_aux_mpu_call_pre(inf, port);
	if (err) {
		mutex_unlock(&inf->mutex);
		return err;
	}

	nvi_pm(inf, NVI_PM_ON);
	err = nvi_aux_port_enable(inf, port, enable, fifo_enable);
	nvi_pm(inf, NVI_PM_AUTO);
	err = nvi_aux_mpu_call_post(inf, "nvi_mpu_enable err: ", err);
	mutex_unlock(&inf->mutex);
	return err;
}
EXPORT_SYMBOL(nvi_mpu_enable);

int nvi_mpu_delay_ms(int port, u8 delay_ms)
{
	struct inv_gyro_state_s *inf;
	int err;

	inf = inf_local;
	if (inf != NULL) {
		if (inf->dbg & NVI_DBG_SPEW_AUX)
			pr_info("%s port %d: %u\n", __func__, port, delay_ms);
	} else {
		pr_debug("%s port %d: %u ERR -EAGAIN\n",
			 __func__, port, delay_ms);
		return -EAGAIN;
	}

	mutex_lock(&inf->mutex);
	err = nvi_aux_mpu_call_pre(inf, port);
	if (err) {
		mutex_unlock(&inf->mutex);
		return err;
	}

	if (inf->hw.i2c_slv_ctrl[port] & BIT_SLV_EN) {
		err = nvi_aux_delay(inf, port, delay_ms);
		nvi_global_delay(inf);
	} else {
		inf->aux.port[port].nmp.delay_ms = delay_ms;
	}
	err = nvi_aux_mpu_call_post(inf, "nvi_mpu_delay_ms err: ", err);
	mutex_unlock(&inf->mutex);
	return err;
}
EXPORT_SYMBOL(nvi_mpu_delay_ms);

int nvi_mpu_delay_us(int port, unsigned long delay_us)
{
	struct inv_gyro_state_s *inf;
	int err;

	inf = inf_local;
	if (inf != NULL) {
		if (inf->dbg & NVI_DBG_SPEW_AUX)
			pr_info("%s port %d: %lu\n", __func__, port, delay_us);
	} else {
		pr_debug("%s port %d: %lu ERR -EAGAIN\n",
			__func__, port, delay_us);
		return -EAGAIN;
	}

	mutex_lock(&inf->mutex);
	err = nvi_aux_mpu_call_pre(inf, port);
	if (err) {
		mutex_unlock(&inf->mutex);
		return err;
	}

	inf->aux.port[port].nmp.delay_us = delay_us;
	if (inf->hw.i2c_slv_ctrl[port] & BIT_SLV_EN)
		err = nvi_global_delay(inf);
	err = nvi_aux_mpu_call_post(inf, "nvi_mpu_delay_us err: ", err);
	mutex_unlock(&inf->mutex);
	return err;
}
EXPORT_SYMBOL(nvi_mpu_delay_us);

int nvi_mpu_data_out(int port, u8 data_out)
{
	struct inv_gyro_state_s *inf;
	int err;

	inf = inf_local;
	if (inf == NULL)
		return -EAGAIN;

	mutex_lock(&inf->mutex);
	err = nvi_aux_mpu_call_pre(inf, port);
	if (err) {
		mutex_unlock(&inf->mutex);
		return err;
	}

	if (inf->hw.i2c_slv_ctrl[port] & BIT_SLV_EN) {
		err = nvi_aux_port_data_out(inf, port, data_out);
	} else {
		inf->aux.port[port].nmp.data_out = data_out;
		inf->aux.port[port].hw_do = false;
	}
	if (err < 0)
		err = -EBUSY;
	mutex_unlock(&inf->mutex);
	return err;
}
EXPORT_SYMBOL(nvi_mpu_data_out);

int nvi_mpu_bypass_request(bool enable)
{
	struct inv_gyro_state_s *inf;
	int err;

	inf = inf_local;
	if (inf != NULL) {
		if (inf->dbg & NVI_DBG_SPEW_AUX)
			pr_info("%s\n", __func__);
	} else {
		pr_debug("%s ERR -EAGAIN\n", __func__);
		return -EAGAIN;
	}

	mutex_lock(&inf->mutex);
	if (inf->shutdown || inf->suspend) {
		mutex_unlock(&inf->mutex);
		return -EPERM;
	}

	nvi_pm(inf, NVI_PM_ON);
	err = nvi_aux_bypass_request(inf, enable);
	nvi_pm(inf, NVI_PM_AUTO);
	err = nvi_aux_mpu_call_post(inf, "nvi_mpu_bypass_request err: ", err);
	mutex_unlock(&inf->mutex);
	return err;
}
EXPORT_SYMBOL(nvi_mpu_bypass_request);

int nvi_mpu_bypass_release(void)
{
	struct inv_gyro_state_s *inf;

	inf = inf_local;
	if (inf != NULL) {
		if (inf->dbg & NVI_DBG_SPEW_AUX)
			pr_info("%s\n", __func__);
	} else {
		pr_debug("%s\n", __func__);
		return 0;
	}

	mutex_lock(&inf->mutex);
	if (inf->shutdown || inf->suspend) {
		mutex_unlock(&inf->mutex);
		return 0;
	}

	nvi_pm(inf, NVI_PM_ON);
	nvi_aux_bypass_release(inf);
	nvi_pm(inf, NVI_PM_AUTO);
	nvi_aux_mpu_call_post(inf, "nvi_mpu_bypass_release", 0);
	mutex_unlock(&inf->mutex);
	return 0;
}
EXPORT_SYMBOL(nvi_mpu_bypass_release);

int nvi_mpu_sysfs_register(struct kobject *target, char *name)
{
	int err;
	struct inv_gyro_state_s *inf = inf_local;
	err = sysfs_create_link(&inf->inv_dev->kobj, target, name);
	if (err)
		dev_err(&inf->i2c->dev, "%s: ERR=%d\n",
			__func__, err);
	return err;
}
EXPORT_SYMBOL(nvi_mpu_sysfs_register);

int nvi_gyro_enable(struct inv_gyro_state_s *inf,
		    unsigned char enable, unsigned char fifo_enable)
{
	unsigned char enable_old;
	unsigned char fifo_enable_old;
	int err;
	int err_t;

	enable_old = inf->chip_config.gyro_enable;
	fifo_enable_old = inf->chip_config.gyro_fifo_enable;
	inf->chip_config.gyro_fifo_enable = fifo_enable;
	inf->chip_config.gyro_enable = enable;
	err_t = nvi_pm(inf, NVI_PM_ON_FULL);
	if (enable != enable_old) {
		if (enable) {
			err = nvi_gyro_config_wr(inf, 0,
						 inf->chip_config.gyro_fsr);
			if (err < 0)
				err_t |= err;
		}
		nvi_global_delay(inf);
	}
	if (fifo_enable_old != fifo_enable)
		err_t = nvi_reset(inf, true, false);
	if (err_t) {
		inf->chip_config.gyro_enable = enable_old;
		inf->chip_config.gyro_fifo_enable = fifo_enable_old;
	}
	if (inf->chip_config.gyro_enable)
		inf->chip_config.temp_enable |= NVI_TEMP_GYRO;
	else
		inf->chip_config.temp_enable &= ~NVI_TEMP_GYRO;
	err_t |= nvi_pm(inf, NVI_PM_AUTO);
	return err_t;
}

int nvi_accl_enable(struct inv_gyro_state_s *inf,
		    unsigned char enable, unsigned char fifo_enable)
{
	unsigned char enable_old;
	unsigned char fifo_enable_old;
	int err;
	int err_t;

	enable_old = inf->chip_config.accl_enable;
	fifo_enable_old = inf->chip_config.accl_fifo_enable;
	inf->chip_config.accl_fifo_enable = fifo_enable;
	inf->chip_config.accl_enable = enable;
	err_t = nvi_pm(inf, NVI_PM_ON);
	if (enable != enable_old) {
		if (inf->chip_type == INV_MPU3050) {
			if (inf->mpu_slave != NULL) {
				if (enable) {
					inf->mpu_slave->resume(inf);
					err_t |= nvi_accel_config_wr(inf, 0,
						 inf->chip_config.accl_fsr, 0);
				} else {
					inf->mpu_slave->suspend(inf);
				}
			}
		} else {
			if (enable) {
				err = nvi_accel_config_wr(inf, 0,
						 inf->chip_config.accl_fsr, 0);
				if (err < 0)
					err_t |= err;
			}
		}
		nvi_global_delay(inf);
	}
	if (fifo_enable_old != fifo_enable)
		err_t |= nvi_reset(inf, true, false);
	if (err_t) {
		inf->chip_config.accl_enable = enable_old;
		inf->chip_config.accl_fifo_enable = fifo_enable_old;
	}
	if (inf->chip_config.accl_enable)
		inf->chip_config.temp_enable |= NVI_TEMP_ACCL;
	else
		inf->chip_config.temp_enable &= ~NVI_TEMP_ACCL;
	err_t |= nvi_pm(inf, NVI_PM_AUTO);
	return err_t;
}


static ssize_t nvi_gyro_enable_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct inv_gyro_state_s *inf;
	unsigned char enable;
	unsigned char fifo_enable;
	int err;

	inf = dev_get_drvdata(dev);
	err = kstrtou8(buf, 10, &enable);
	if (err)
		return -EINVAL;

	if (enable > 7)
		enable = 7;
	mutex_lock(&inf->mutex);
	if (enable != inf->chip_config.gyro_enable) {
		dev_dbg(&inf->i2c->dev, "%s: %x\n", __func__, enable);
		fifo_enable = inf->chip_config.gyro_fifo_enable & enable;
		err = nvi_gyro_enable(inf, enable, fifo_enable);
	}
	mutex_unlock(&inf->mutex);
	if (err) {
		dev_err(&inf->i2c->dev, "%s: %x ERR=%d\n",
			__func__, enable, err);
		return err;
	}

	return count;
}

static ssize_t nvi_gyro_enable_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct inv_gyro_state_s *inf = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", inf->chip_config.gyro_enable);
}

static ssize_t nvi_gyro_fifo_enable_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct inv_gyro_state_s *inf;
	unsigned char fifo_enable;
	unsigned char enable;
	int err;

	inf = dev_get_drvdata(dev);
	err = kstrtou8(buf, 10, &fifo_enable);
	if (err)
		return -EINVAL;

	if (fifo_enable > 7)
		fifo_enable = 7;
	mutex_lock(&inf->mutex);
	if (fifo_enable != inf->chip_config.gyro_fifo_enable) {
		dev_dbg(&inf->i2c->dev, "%s: %x\n", __func__, fifo_enable);
		enable = inf->chip_config.gyro_enable | fifo_enable;
		err = nvi_gyro_enable(inf, enable, fifo_enable);
	}
	mutex_unlock(&inf->mutex);
	if (err) {
		dev_err(&inf->i2c->dev, "%s: %x ERR=%d\n",
			__func__, fifo_enable, err);
		return err;
	}

	return count;
}

static ssize_t nvi_gyro_fifo_enable_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct inv_gyro_state_s *inf = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", inf->chip_config.gyro_fifo_enable);
}

static ssize_t inv_gyro_delay_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct inv_gyro_state_s *inf;
	unsigned long gyro_delay_us;
	unsigned long gyro_delay_us_old;
	int err;

	inf = dev_get_drvdata(dev);
	err = kstrtoul(buf, 10, &gyro_delay_us);
	if (err)
		return err;

	if (gyro_delay_us < NVI_INPUT_GYRO_DELAY_US_MIN)
		gyro_delay_us = NVI_INPUT_GYRO_DELAY_US_MIN;
	mutex_lock(&inf->mutex);
	if (gyro_delay_us != inf->chip_config.gyro_delay_us) {
		dev_dbg(&inf->i2c->dev, "%s: %lu\n", __func__, gyro_delay_us);
		gyro_delay_us_old = inf->chip_config.gyro_delay_us;
		inf->chip_config.gyro_delay_us = gyro_delay_us;
		if (inf->chip_config.gyro_enable) {
			err = nvi_global_delay(inf);
			if (err)
				inf->chip_config.gyro_delay_us =
							     gyro_delay_us_old;
		}
	}
	mutex_unlock(&inf->mutex);
	if (err) {
		dev_err(&inf->i2c->dev, "%s: %lu ERR=%d\n",
			__func__, gyro_delay_us, err);
		return err;
	}

	return count;
}

static ssize_t nvi_gyro_delay_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct inv_gyro_state_s *inf;

	inf = dev_get_drvdata(dev);
	if (inf->chip_config.gyro_enable)
		return sprintf(buf, "%lu\n", inf->sample_delay_us);

	return sprintf(buf, "%d\n", NVI_INPUT_GYRO_DELAY_US_MIN);
}

static ssize_t nvi_gyro_resolution_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	struct inv_gyro_state_s *inf;
	unsigned int resolution;

	inf = dev_get_drvdata(dev);
	if (kstrtouint(buf, 10, &resolution))
		return -EINVAL;

	dev_dbg(&inf->i2c->dev, "%s %u\n", __func__, resolution);
	inf->chip_config.gyro_resolution = resolution;
	return count;
}

static ssize_t nvi_gyro_resolution_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct inv_gyro_state_s *inf;
	unsigned int resolution;

	inf = dev_get_drvdata(dev);
	if (inf->chip_config.gyro_enable)
		resolution = inf->chip_config.gyro_resolution;
	else
		resolution = GYRO_INPUT_RESOLUTION;
	return sprintf(buf, "%u\n", resolution);
}

static ssize_t nvi_gyro_max_range_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct inv_gyro_state_s *inf;
	unsigned char fsr;
	int err;

	inf = dev_get_drvdata(dev);
	err = kstrtou8(buf, 10, &fsr);
	if (err)
		return -EINVAL;

	if (fsr > 3)
		return -EINVAL;

	mutex_lock(&inf->mutex);
	if (fsr != inf->chip_config.gyro_fsr) {
		dev_dbg(&inf->i2c->dev, "%s: %x\n", __func__, fsr);
		if (inf->chip_config.gyro_enable) {
			err = nvi_gyro_config_wr(inf, 0, fsr);
			if ((err > 0) && (inf->hw.fifo_en & BITS_GYRO_OUT))
				nvi_reset(inf, true, false);
		}
		if (err >= 0)
			inf->chip_config.gyro_fsr = fsr;
	}
	mutex_unlock(&inf->mutex);
	if (err < 0) {
		dev_err(&inf->i2c->dev, "%s: %x ERR=%d\n", __func__, fsr, err);
		return err;
	}

	return count;
}

static ssize_t nvi_gyro_max_range_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct inv_gyro_state_s *inf = dev_get_drvdata(dev);
	unsigned int range;

	if (inf->chip_config.gyro_enable)
		range = inf->chip_config.gyro_fsr;
	else
		range = (1 << inf->chip_config.gyro_fsr) * 250;
	return sprintf(buf, "%u\n", range);
}

static ssize_t nvi_accl_enable_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct inv_gyro_state_s *inf;
	unsigned char enable;
	unsigned char fifo_enable;
	int err;

	inf = dev_get_drvdata(dev);
	err = kstrtou8(buf, 10, &enable);
	if (err)
		return -EINVAL;

	if (enable > 7)
		enable = 7;
	mutex_lock(&inf->mutex);
	if (enable != inf->chip_config.accl_enable) {
		dev_dbg(&inf->i2c->dev, "%s: %x\n", __func__, enable);
		fifo_enable = inf->chip_config.accl_fifo_enable & enable;
		err = nvi_accl_enable(inf, enable, fifo_enable);
	}
	mutex_unlock(&inf->mutex);
	if (err) {
		dev_err(&inf->i2c->dev, "%s: %x ERR=%d\n",
			__func__, enable, err);
		return err;
	}

	return count;
}

static ssize_t nvi_accl_enable_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct inv_gyro_state_s *inf = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", inf->chip_config.accl_enable);
}

static ssize_t nvi_accl_fifo_enable_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct inv_gyro_state_s *inf;
	unsigned char fifo_enable;
	unsigned char enable;
	int err;

	inf = dev_get_drvdata(dev);
	err = kstrtou8(buf, 10, &fifo_enable);
	if (err)
		return -EINVAL;

	if (fifo_enable > 7)
		fifo_enable = 7;
	mutex_lock(&inf->mutex);
	if (fifo_enable != inf->chip_config.accl_fifo_enable) {
		dev_dbg(&inf->i2c->dev, "%s: %x\n", __func__, fifo_enable);
		enable = inf->chip_config.accl_enable | fifo_enable;
		err = nvi_accl_enable(inf, enable, fifo_enable);
	}
	mutex_unlock(&inf->mutex);
	if (err) {
		dev_err(&inf->i2c->dev, "%s: %x ERR=%d\n",
			__func__, fifo_enable, err);
		return err;
	}

	return count;
}

static ssize_t nvi_accl_fifo_enable_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct inv_gyro_state_s *inf = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", inf->chip_config.accl_fifo_enable);
}

static ssize_t nvi_accl_delay_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct inv_gyro_state_s *inf;
	unsigned long accl_delay_us;
	unsigned long accl_delay_us_old;
	int err;

	inf = dev_get_drvdata(dev);
	err = kstrtoul(buf, 10, &accl_delay_us);
	if (err)
		return err;

	if (accl_delay_us < NVI_INPUT_ACCL_DELAY_US_MIN)
		accl_delay_us = NVI_INPUT_ACCL_DELAY_US_MIN;
	mutex_lock(&inf->mutex);
	if (accl_delay_us != inf->chip_config.accl_delay_us) {
		dev_dbg(&inf->i2c->dev, "%s: %lu\n", __func__, accl_delay_us);
		accl_delay_us_old = inf->chip_config.accl_delay_us;
		inf->chip_config.accl_delay_us = accl_delay_us;
		if (inf->chip_config.accl_enable) {
			if (inf->hw.pwr_mgmt_1 & inf->reg->cycle)
				nvi_pm(inf, NVI_PM_ON);
			err = nvi_global_delay(inf);
			if (err)
				inf->chip_config.accl_delay_us =
							     accl_delay_us_old;
			else
				nvi_pm(inf, NVI_PM_AUTO);
		}
	}
	mutex_unlock(&inf->mutex);
	if (err) {
		dev_err(&inf->i2c->dev, "%s: %lu ERR=%d\n",
			__func__, accl_delay_us, err);
		return err;
	}

	return count;
}

static ssize_t nvi_accl_delay_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct inv_gyro_state_s *inf;

	inf = dev_get_drvdata(dev);
	if (inf->chip_config.accl_enable)
		return sprintf(buf, "%lu\n", inf->sample_delay_us);

	return sprintf(buf, "%d\n", NVI_INPUT_ACCL_DELAY_US_MIN);
}

static ssize_t nvi_accl_resolution_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	struct inv_gyro_state_s *inf;
	unsigned int resolution;

	inf = dev_get_drvdata(dev);
	if (kstrtouint(buf, 10, &resolution))
		return -EINVAL;

	dev_dbg(&inf->i2c->dev, "%s %u\n", __func__, resolution);
	inf->chip_config.accl_resolution = resolution;
	return count;
}

static ssize_t nvi_accl_resolution_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct inv_gyro_state_s *inf;
	unsigned int resolution;

	inf = dev_get_drvdata(dev);
	if (inf->chip_config.accl_enable)
		resolution = inf->chip_config.accl_resolution;
	else
		resolution = ACCL_INPUT_RESOLUTION;
	return sprintf(buf, "%u\n", resolution);
}

static ssize_t nvi_accl_max_range_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct inv_gyro_state_s *inf;
	unsigned char fsr;
	int err;

	inf = dev_get_drvdata(dev);
	err = kstrtou8(buf, 10, &fsr);
	if (err)
		return -EINVAL;

	if (fsr > 3)
		return -EINVAL;

	mutex_lock(&inf->mutex);
	if (fsr != inf->chip_config.accl_fsr) {
		dev_dbg(&inf->i2c->dev, "%s: %x\n", __func__, fsr);
		if (inf->chip_config.accl_enable) {
			if (inf->hw.pwr_mgmt_1 & inf->reg->cycle)
				nvi_pm(inf, NVI_PM_ON);
			if (inf->chip_type == INV_MPU3050) {
				if (inf->mpu_slave != NULL) {
					inf->mpu_slave->set_fs(inf, fsr);
					err = 1;
				}
			} else {
				err = nvi_accel_config_wr(inf, 0, fsr, 0);
			}
			if ((err > 0) && (inf->hw.fifo_en &
					  inf->reg->accl_fifo_en))
				nvi_reset(inf, true, false);
			nvi_pm(inf, NVI_PM_AUTO);
		}
		if (err >= 0)
			inf->chip_config.accl_fsr = fsr;
	}
	mutex_unlock(&inf->mutex);
	if (err < 0) {
		dev_err(&inf->i2c->dev, "%s: %x ERR=%d\n", __func__, fsr, err);
		return err;
	}

	return count;
}

static ssize_t nvi_accl_max_range_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct inv_gyro_state_s *inf = dev_get_drvdata(dev);
	unsigned int range;

	if (inf->chip_config.accl_enable)
		range = inf->chip_config.accl_fsr;
	else
		range = 0x4000 >> inf->chip_config.accl_fsr;
	return sprintf(buf, "%u\n", range);
}

static ssize_t nvi_lpa_delay_enable_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct inv_gyro_state_s *inf;
	unsigned long lpa_delay_us;
	int err;

	inf = dev_get_drvdata(dev);
	err = kstrtoul(buf, 10, &lpa_delay_us);
	if (err)
		return err;

	if (lpa_delay_us > NVI_DELAY_US_MAX)
		lpa_delay_us = NVI_DELAY_US_MAX;
	dev_dbg(&inf->i2c->dev, "%s: %lu\n", __func__, lpa_delay_us);
	mutex_lock(&inf->mutex);
	inf->chip_config.lpa_delay_us = lpa_delay_us;
	err = nvi_pm(inf, NVI_PM_AUTO);
	mutex_unlock(&inf->mutex);
	if (err)
		dev_err(&inf->i2c->dev, "%s: %lu ERR=%d\n",
			__func__, lpa_delay_us, err);
	return count;
}

static ssize_t nvi_lpa_delay_enable_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct inv_gyro_state_s *inf = dev_get_drvdata(dev);

	return sprintf(buf, "%lu\n", inf->chip_config.lpa_delay_us);
}

static ssize_t nvi_mot_enable_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct inv_gyro_state_s *inf;
	unsigned char mot_enable;
	int err;

	inf = dev_get_drvdata(dev);
	err = kstrtou8(buf, 10, &mot_enable);
	if (err)
		return -EINVAL;

	if (mot_enable > NVI_MOT_DBG)
		return -EINVAL;

	dev_dbg(&inf->i2c->dev, "%s: %u\n", __func__, mot_enable);
	inf->chip_config.mot_enable = mot_enable;
	if (!mot_enable) {
		mutex_lock(&inf->mutex);
		nvi_pm(inf, NVI_PM_ON);
		err = nvi_motion_detect_enable(inf, 0);
		err |= nvi_pm(inf, NVI_PM_AUTO);
		mutex_unlock(&inf->mutex);
		if (err) {
			dev_err(&inf->i2c->dev, "%s: %u ERR=%d\n",
				__func__, mot_enable, err);
			return err;
		}
	}

	return count;
}

static ssize_t nvi_mot_enable_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct inv_gyro_state_s *inf = dev_get_drvdata(dev);

	return sprintf(buf, "%x (0=dis 1=en 2=dbg)\n",
		       inf->chip_config.mot_enable);
}

static ssize_t nvi_motion_thr_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct inv_gyro_state_s *inf;
	unsigned char mot_thr;
	int err;

	inf = dev_get_drvdata(dev);
	err = kstrtou8(buf, 10, &mot_thr);
	if (err)
		return -EINVAL;

	mutex_lock(&inf->mutex);
	if (((!inf->chip_config.gyro_enable) && (!inf->aux.enable) &&
						inf->chip_config.mot_enable)) {
		dev_dbg(&inf->i2c->dev, "%s: %u\n", __func__, mot_thr);
		if (inf->chip_config.mot_enable == NVI_MOT_DBG)
			pr_info("%s: %u\n", __func__, mot_thr);
		nvi_pm(inf, NVI_PM_ON);
		err = nvi_motion_detect_enable(inf, mot_thr);
		err |= nvi_pm(inf, NVI_PM_AUTO);
	}
	mutex_unlock(&inf->mutex);
	if (err) {
		dev_err(&inf->i2c->dev, "%s: %u ERR=%d\n",
			__func__, mot_thr, err);
		return err;
	}

	return count;
}

static ssize_t nvi_motion_thr_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct inv_gyro_state_s *inf;
	unsigned char mot_thr;

	inf = dev_get_drvdata(dev);
	if ((inf->hw.accl_config & 0x07) == 0x07)
		mot_thr = inf->hw.mot_thr;
	else
		mot_thr = 0;
	return sprintf(buf, "%u\n", mot_thr);
}

static ssize_t nvi_motion_dur_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct inv_gyro_state_s *inf;
	unsigned char mot_dur;
	int err;

	inf = dev_get_drvdata(dev);
	err = kstrtou8(buf, 10, &mot_dur);
	if (err)
		return -EINVAL;

	dev_dbg(&inf->i2c->dev, "%s: %u\n", __func__, mot_dur);
	inf->chip_config.mot_dur = mot_dur;
	return count;
}

static ssize_t nvi_motion_dur_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct inv_gyro_state_s *inf = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", inf->chip_config.mot_dur);
}

static ssize_t nvi_motion_ctrl_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct inv_gyro_state_s *inf;
	unsigned char mot_ctrl;
	int err;

	inf = dev_get_drvdata(dev);
	err = kstrtou8(buf, 10, &mot_ctrl);
	if (err)
		return -EINVAL;

	dev_dbg(&inf->i2c->dev, "%s: %u\n", __func__, mot_ctrl);
	inf->chip_config.mot_ctrl = mot_ctrl;
	return count;
}

static ssize_t nvi_motion_ctrl_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct inv_gyro_state_s *inf = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", inf->chip_config.mot_ctrl);
}

static ssize_t nvi_motion_count_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct inv_gyro_state_s *inf;
	unsigned int mot_cnt;
	int err;

	inf = dev_get_drvdata(dev);
	err = kstrtouint(buf, 10, &mot_cnt);
	if (err)
		return -EINVAL;

	dev_dbg(&inf->i2c->dev, "%s: %u\n", __func__, mot_cnt);
	inf->chip_config.mot_cnt = mot_cnt;
	return count;
}

static ssize_t nvi_motion_count_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct inv_gyro_state_s *inf = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", inf->chip_config.mot_cnt);
}

static ssize_t nvi_bypass_timeout_ms_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	struct inv_gyro_state_s *inf;
	unsigned int bypass_timeout_ms;
	int err;

	inf = dev_get_drvdata(dev);
	err = kstrtouint(buf, 10, &bypass_timeout_ms);
	if (err)
		return -EINVAL;

	dev_dbg(&inf->i2c->dev, "%s: %u\n", __func__, bypass_timeout_ms);
	inf->chip_config.bypass_timeout_ms = bypass_timeout_ms;
	return count;
}

static ssize_t nvi_bypass_timeout_ms_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct inv_gyro_state_s *inf = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", inf->chip_config.bypass_timeout_ms);
}

static ssize_t nvi_min_delay_us_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct inv_gyro_state_s *inf;
	unsigned long min_delay_us;
	int err;

	inf = dev_get_drvdata(dev);
	err = kstrtoul(buf, 10, &min_delay_us);
	if (err)
		return -EINVAL;

	dev_dbg(&inf->i2c->dev, "%s: %lu\n", __func__, min_delay_us);
	inf->chip_config.min_delay_us = min_delay_us;
	return count;
}

static ssize_t nvi_min_delay_us_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct inv_gyro_state_s *inf = dev_get_drvdata(dev);

	return sprintf(buf, "%lu\n", inf->chip_config.min_delay_us);
}

static ssize_t nvi_fifo_thr_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct inv_gyro_state_s *inf;
	unsigned int fifo_thr;
	int err;

	inf = dev_get_drvdata(dev);
	err = kstrtouint(buf, 10, &fifo_thr);
	if (err)
		return -EINVAL;

	dev_dbg(&inf->i2c->dev, "%s: %u\n", __func__, fifo_thr);
	mutex_lock(&inf->mutex);
	inf->chip_config.fifo_thr = fifo_thr;
	err = nvi_int_enable_wr(inf, true);
	mutex_unlock(&inf->mutex);
	if (err) {
		dev_err(&inf->i2c->dev, "%s: %u ERR=%d\n",
			__func__, fifo_thr, err);
		return err;
	}

	return count;
}

static ssize_t nvi_fifo_thr_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct inv_gyro_state_s *inf = dev_get_drvdata(dev);

	return sprintf(buf, "%u  0=batch_mode 1=disable %u=limit\n",
		       inf->chip_config.fifo_thr, inf->hal.fifo_size);
}

static ssize_t nvi_enable_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct inv_gyro_state_s *inf;
	unsigned char enable;
	int err;

	inf = dev_get_drvdata(dev);
	err = kstrtou8(buf, 10, &enable);
	if (err)
		return -EINVAL;

	mutex_lock(&inf->mutex);
	if (enable)
		enable = 1;
	if (enable != inf->chip_config.enable) {
		dev_dbg(&inf->i2c->dev, "%s: %u\n", __func__, enable);
		inf->chip_config.enable = enable;
		err = nvi_pm(inf, NVI_PM_AUTO);
	}
	mutex_unlock(&inf->mutex);
	if (err)
		dev_err(&inf->i2c->dev, "%s: %u ERR=%d\n",
			__func__, enable, err);
	return count;
}

static ssize_t nvi_enable_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct inv_gyro_state_s *inf = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", inf->chip_config.enable);
}

/**
 *  inv_raw_gyro_show() - Read gyro data directly from registers.
 */
static ssize_t inv_raw_gyro_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct inv_gyro_state_s *st;
	struct inv_reg_map_s *reg;
	int result;
	unsigned char data[6];

	st = dev_get_drvdata(dev);
	reg = st->reg;
	if (0 == st->chip_config.gyro_enable)
		return -EPERM;

	result = inv_i2c_read(st, reg->raw_gyro, 6, data);
	if (result) {
		printk(KERN_ERR "Could not read raw registers.\n");
		return result;
	}

	return sprintf(buf, "%d %d %d %lld\n",
		(signed short)(be16_to_cpup((short *)&data[0])),
		(signed short)(be16_to_cpup((short *)&data[2])),
		(signed short)(be16_to_cpup((short *)&data[4])),
		nvi_ts_ns());
}

/**
 *  inv_raw_accl_show() - Read accel data directly from registers.
 */
static ssize_t inv_raw_accl_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct inv_gyro_state_s *st;
	struct inv_reg_map_s *reg;
	u8 data[6];
	s16 out[3];
	int result;

	st = dev_get_drvdata(dev);
	reg = st->reg;
	if (0 == st->chip_config.accl_enable)
		return -EPERM;

	result = inv_i2c_read(st, reg->raw_accl, 6, data);
	if (result)
		return result;

	if (st->chip_type == INV_MPU3050) {
		if (st->mpu_slave != NULL) {
			if (0 == st->mpu_slave->get_mode(st))
				return -EINVAL;

			st->mpu_slave->combine_data(data, out);
		} else {
			memcpy(out, data, sizeof(data));
		}
		return sprintf(buf, "%d %d %d %lld\n",
			       out[0], out[1], out[2], nvi_ts_ns());
	}

	return sprintf(buf, "%d %d %d %lld\n",
		       ((signed short)(be16_to_cpup((short *)&data[0])) *
			st->chip_info.multi),
		       ((signed short)(be16_to_cpup((short *)&data[2])) *
			st->chip_info.multi),
		       ((signed short)(be16_to_cpup((short *)&data[4])) *
			st->chip_info.multi),
		       nvi_ts_ns());
}

static ssize_t nvi_temp_enable_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct inv_gyro_state_s *inf;
	unsigned char enable;
	int err;

	inf = dev_get_drvdata(dev);
	err = kstrtou8(buf, 10, &enable);
	if (err)
		return -EINVAL;

	if (enable)
		enable = NVI_TEMP_EN;
	mutex_lock(&inf->mutex);
	if (enable != (inf->chip_config.temp_enable & NVI_TEMP_EN)) {
		dev_dbg(&inf->i2c->dev, "%s: %x\n", __func__, enable);
		if (enable)
			inf->chip_config.temp_enable |= NVI_TEMP_EN;
		else
			inf->chip_config.temp_enable &= ~NVI_TEMP_EN;
		err = nvi_pm(inf, NVI_PM_AUTO);
	}
	mutex_unlock(&inf->mutex);
	if (err) {
		dev_err(&inf->i2c->dev, "%s: %x ERR=%d\n",
			__func__, enable, err);
		return err;
	}

	return count;
}

static ssize_t nvi_temp_enable_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct inv_gyro_state_s *inf = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", inf->chip_config.temp_enable);
}

static ssize_t nvi_temp_fifo_enable_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct inv_gyro_state_s *inf;
	unsigned char fifo_enable;
	unsigned char fifo_enable_old;
	int err;

	inf = dev_get_drvdata(dev);
	err = kstrtou8(buf, 10, &fifo_enable);
	if (err)
		return -EINVAL;

	if (fifo_enable)
		fifo_enable = 1;
	dev_dbg(&inf->i2c->dev, "%s: %x\n", __func__, fifo_enable);
	mutex_lock(&inf->mutex);
	if (fifo_enable != inf->chip_config.temp_fifo_enable) {
		fifo_enable_old = inf->chip_config.temp_fifo_enable;
		inf->chip_config.temp_fifo_enable = fifo_enable;
		err = nvi_pm(inf, NVI_PM_ON);
		err |= nvi_reset(inf, true, false);
		err |= nvi_pm(inf, NVI_PM_AUTO);
		if (err)
			inf->chip_config.temp_fifo_enable = fifo_enable_old;
	}
	mutex_unlock(&inf->mutex);
	if (err) {
		dev_err(&inf->i2c->dev, "%s: %x ERR=%d\n",
			__func__, fifo_enable, err);
		return err;
	}

	return count;
}

static ssize_t nvi_temp_fifo_enable_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct inv_gyro_state_s *inf = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", inf->chip_config.temp_fifo_enable);
}

/**
 *  inv_temp_scale_show() - Get the temperature scale factor in LSBs/degree C.
 */
static ssize_t inv_temp_scale_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct inv_gyro_state_s *st = dev_get_drvdata(dev);

	if (INV_MPU3050 == st->chip_type)
		return sprintf(buf, "280\n");
	else
		return sprintf(buf, "340\n");
}

/**
 *  inv_temp_offset_show() - Get the temperature offset in LSBs/degree C.
 */
static ssize_t inv_temp_offset_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct inv_gyro_state_s *st = dev_get_drvdata(dev);

	if (INV_MPU3050 == st->chip_type)
		return sprintf(buf, "-13200\n");
	else
		return sprintf(buf, "-521\n");
}

static ssize_t inv_temperature_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct inv_gyro_state_s *inf;
	ssize_t rtn;

	inf = dev_get_drvdata(dev);
	mutex_lock(&inf->mutex_temp);
	rtn = sprintf(buf, "%d %lld\n", inf->temp_val, inf->temp_ts);
	mutex_unlock(&inf->mutex_temp);
	return rtn;
}

/**
 * inv_key_store() -  calling this function will store authenticate key
 */
static ssize_t inv_key_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct inv_gyro_state_s *st = dev_get_drvdata(dev);
	unsigned int result, data, out;
	unsigned char *p, d[4];

	if (st->chip_config.enable)
		return -EPERM;

	result = kstrtoul(buf, 10, (long unsigned int *)&data);
	if (result)
		return result;

	out = cpu_to_be32p(&data);
	p = (unsigned char *)&out;
	result = mem_w(D_AUTH_IN, 4, p);
	if (result)
		return result;

	result = mpu_memory_read(st->sl_handle, st->i2c_addr,
		D_AUTH_IN, 4, d);
	return count;
}

/**
 *  inv_reg_dump_show() - Register dump for testing.
 *  TODO: Only for testing.
 */
static ssize_t inv_reg_dump_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int ii;
	char data;
	ssize_t bytes_printed = 0;
	struct inv_gyro_state_s *st = dev_get_drvdata(dev);

	for (ii = 0; ii < st->hw_s->num_reg; ii++) {
		/* don't read fifo r/w register */
		if (ii == st->reg->fifo_r_w)
			data = 0;
		else
			inv_i2c_read(st, ii, 1, &data);
		bytes_printed += sprintf(buf + bytes_printed, "%#2x: %#2x\n",
					 ii, data);
	}
	return bytes_printed;
}

/**
 * inv_gyro_orientation_show() - show orientation matrix
 */
static ssize_t inv_gyro_orientation_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct inv_gyro_state_s *st = dev_get_drvdata(dev);
	signed char *m;

	m = st->plat_data.orientation;
	return sprintf(buf, "%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
		       m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8]);
}

/**
 * inv_accl_matrix_show() - show orientation matrix
 */
static ssize_t inv_accl_matrix_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct inv_gyro_state_s *st = dev_get_drvdata(dev);
	signed char *m;

	if (st->plat_data.sec_slave_type == SECONDARY_SLAVE_TYPE_ACCEL)
		m = st->plat_data.secondary_orientation;
	else
		m = st->plat_data.orientation;
	return sprintf(buf, "%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
		       m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8]);
}

/**
 * inv_self_test_show() - self test result. 0 for fail; 1 for success.
 *                        calling this function will trigger self test
 *                        and return test result.
 */
static ssize_t inv_self_test_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	int result;
	int bias[3];
	struct inv_gyro_state_s *st = dev_get_drvdata(dev);

	if (INV_MPU3050 == st->chip_type) {
		bias[0] = bias[1] = bias[2] = 0;
		result = 0;
	} else {
		mutex_lock(&st->mutex);
		result = inv_hw_self_test(st, bias);
		mutex_unlock(&st->mutex);
	}
	return sprintf(buf, "%d, %d, %d, %d\n",
		bias[0], bias[1], bias[2], result);
}

/**
 * inv_get_accl_bias_show() - show accl bias value
 */
static ssize_t inv_get_accl_bias_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	int result;
	int bias[3];
	struct inv_gyro_state_s *st = dev_get_drvdata(dev);
	mutex_lock(&st->mutex);
	result = inv_get_accl_bias(st, bias);
	mutex_unlock(&st->mutex);
	if (result)
		return -EINVAL;

	return sprintf(buf, "%d, %d, %d\n", bias[0], bias[1], bias[2]);
}

/**
 * inv_key_show() -  calling this function will show the key
 *
 */
static ssize_t inv_key_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct inv_gyro_state_s *st = dev_get_drvdata(dev);
	unsigned char *key;
	key = st->plat_data.key;

	return sprintf(buf,
	"%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
		key[0], key[1], key[2], key[3], key[4], key[5], key[6],
		key[7], key[8], key[9], key[10], key[11], key[12],
		key[13], key[14], key[15]);
}

/**
 *  OBSOLETE
 */
static ssize_t inv_power_state_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct inv_gyro_state_s *st;
	unsigned long power_state;

	st = dev_get_drvdata(dev);
	if (kstrtoul(buf, 10, &power_state))
		return -EINVAL;

	if (power_state)
		st->chip_config.is_asleep = 0;
	else
		st->chip_config.is_asleep = 1;
	return count;
}

/**
 *  OBSOLETE
 */
static ssize_t inv_power_state_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct inv_gyro_state_s *st = dev_get_drvdata(dev);

	if (st->chip_config.is_asleep)
		return sprintf(buf, "0\n");

	else
		return sprintf(buf, "1\n");
}

static u16 nvi_report_accl(struct inv_gyro_state_s *inf, u8 *data, s64 ts)
{
	s16 val[3];
	s16 raw[3];
	u16 buf_i;
	unsigned int i;

	if (inf->chip_type == INV_MPU3050) {
		if (inf->mpu_slave != NULL) {
			inf->mpu_slave->combine_data(data, raw);
			for (i = 0; i < 3; i++)
				val[i] = raw[i];
		} else {
			return 0;
		}
	} else {
		raw[AXIS_X] = be16_to_cpup((__be16 *)&data[0]);
		raw[AXIS_Y] = be16_to_cpup((__be16 *)&data[2]);
		raw[AXIS_Z] = be16_to_cpup((__be16 *)&data[4]);
		val[AXIS_X] = raw[AXIS_X] * inf->chip_info.multi;
		val[AXIS_Y] = raw[AXIS_Y] * inf->chip_info.multi;
		val[AXIS_Z] = raw[AXIS_Z] * inf->chip_info.multi;
	}

	buf_i = 0;
	if (!(inf->hw.pwr_mgmt_2 & BIT_STBY_XA)) {
		input_report_rel(inf->idev, REL_RX, val[AXIS_X]);
		buf_i += 2;
	} else {
		raw[AXIS_X] = 0;
		val[AXIS_X] = 0;
	}
	if (!(inf->hw.pwr_mgmt_2 & BIT_STBY_YA)) {
		input_report_rel(inf->idev, REL_RY, val[AXIS_Y]);
		buf_i += 2;
	} else {
		raw[AXIS_Y] = 0;
		val[AXIS_Y] = 0;
	}
	if (!(inf->hw.pwr_mgmt_2 & BIT_STBY_ZA)) {
		input_report_rel(inf->idev, REL_RZ, val[AXIS_Z]);
		buf_i += 2;
	} else {
		raw[AXIS_Z] = 0;
		val[AXIS_Z] = 0;
	}
	for (i = 0; i < 3; i++) {
		inf->accl_raw[i] = raw[i];
		inf->accl[i] = val[i];
	}
	if (inf->dbg & NVI_DBG_SPEW_ACCL_RAW)
		dev_info(&inf->i2c->dev, "ACCL_RAW %d %d %d %lld\n",
			 raw[AXIS_X], raw[AXIS_Y], raw[AXIS_Z], ts);
	if (inf->dbg & NVI_DBG_SPEW_ACCL)
		dev_info(&inf->i2c->dev, "ACCL %d %d %d %lld\n",
			 val[AXIS_X], val[AXIS_Y], val[AXIS_Z], ts);
	return buf_i;
}

void nvi_report_temp(struct inv_gyro_state_s *inf, u8 *data, s64 ts)
{
	mutex_lock(&inf->mutex_temp);
	inf->temp_val = be16_to_cpup((__be16 *)data);
	inf->temp_ts = ts;
	if (inf->dbg & NVI_DBG_SPEW_TEMP)
		dev_info(&inf->i2c->dev, "TEMP %d %lld\n", inf->temp_val, ts);
	mutex_unlock(&inf->mutex_temp);
}

static u16 nvi_report_gyro(struct inv_gyro_state_s *inf,
			   u8 *data, u8 mask, s64 ts)
{
	s16 val[3];
	u16 buf_i;
	bool report;
	unsigned int i;

	if (ts < inf->gyro_start_ts)
		report = false;
	else
		report = true;
	for (i = 0; i < 3; i++)
		val[i] = 0;
	buf_i = 0;
	if (mask & 4) {
		if (report && (!(inf->hw.pwr_mgmt_2 & BIT_STBY_XG))) {
			val[AXIS_X] = be16_to_cpup((__be16 *)&data[buf_i]);
			input_report_rel(inf->idev, REL_X, val[AXIS_X]);
		}
		buf_i += 2;
	}
	if (mask & 2) {
		if (report && (!(inf->hw.pwr_mgmt_2 & BIT_STBY_YG))) {
			val[AXIS_Y] = be16_to_cpup((__be16 *)&data[buf_i]);
			input_report_rel(inf->idev, REL_Y, val[AXIS_Y]);
		}
		buf_i += 2;
	}
	if (mask & 1) {
		if (report && (!(inf->hw.pwr_mgmt_2 & BIT_STBY_ZG))) {
			val[AXIS_Z] = be16_to_cpup((__be16 *)&data[buf_i]);
			input_report_rel(inf->idev, REL_Z, val[AXIS_Z]);
		}
		buf_i += 2;
	}
	for (i = 0; i < 3; i++)
		inf->gyro[i] = val[i];
	if (inf->dbg & NVI_DBG_SPEW_GYRO)
		dev_info(&inf->i2c->dev, "GYRO %d %d %d %lld\n",
			 val[AXIS_X], val[AXIS_Y], val[AXIS_Z], ts);
	return buf_i;
}

static void nvi_sync(struct inv_gyro_state_s *inf, s64 ts)
{
	input_report_rel(inf->idev, REL_MISC, (unsigned int)(ts >> 32));
	input_report_rel(inf->idev, REL_WHEEL,
			 (unsigned int)(ts & 0xffffffff));
	input_sync(inf->idev);
}

static int nvi_accl_read(struct inv_gyro_state_s *inf, s64 ts)
{
	u8 data[6];
	int err;

	err = inv_i2c_read(inf, inf->reg->raw_accl, 6, data);
	if (!err)
		err = nvi_report_accl(inf, data, ts);
	return err;
}

static u16 nvi_fifo_read_accl(struct inv_gyro_state_s *inf,
			      u16 buf_index, s64 ts)
{
	if (inf->hw.fifo_en & inf->reg->accl_fifo_en) {
		nvi_report_accl(inf, &inf->buf[buf_index], ts);
		buf_index += 6;
	}
	return buf_index;
}

static u16 nvi_fifo_read_gyro(struct inv_gyro_state_s *inf,
			      u16 buf_index, s64 ts)
{
	u8 mask;

	if (inf->hw.fifo_en & BIT_TEMP_FIFO_EN) {
		nvi_report_temp(inf, &inf->buf[buf_index], ts);
		buf_index += 2;
	}
	mask = inf->hw.fifo_en;
	mask &= (BIT_GYRO_XOUT | BIT_GYRO_YOUT | BIT_GYRO_ZOUT);
	mask >>= 4;
	if (mask) {
		buf_index += nvi_report_gyro(inf, &inf->buf[buf_index],
					     mask, ts);
	}
	return buf_index;
}

static irqreturn_t nvi_irq_thread(int irq, void *dev_id)
{
	struct inv_gyro_state_s *inf;
	struct aux_port *ap;
	u8 mask;
	u16 fifo_count = 0;
	u16 fifo_sample_size;
	u16 fifo_rd_n;
	u16 fifo_align;
	u16 buf_index;
	s64 ts;
	s64 ts_irq;
	s64 delay;
	bool sync;
	unsigned int ts_len;
	unsigned int samples;
	unsigned int copied;
	unsigned int len;
	int i;
	int err;

	inf = (struct inv_gyro_state_s *)dev_id;
	/* if only accelermeter data */
	if ((inf->hw.pwr_mgmt_1 & inf->reg->cycle) || (inf->hw.int_enable &
						       BIT_MOT_EN)) {
		if (inf->hw.int_enable & BIT_MOT_EN) {
			mutex_lock(&inf->mutex);
			nvi_pm(inf, NVI_PM_ON);
			nvi_motion_detect_enable(inf, 0);
			nvi_int_enable_wr(inf, true);
			nvi_pm(inf, NVI_PM_AUTO);
			mutex_unlock(&inf->mutex);
			if (inf->chip_config.mot_enable == NVI_MOT_DBG)
				pr_info("%s motion detect off\n", __func__);
		}
		ts = nvi_ts_ns();
		err = nvi_accl_read(inf, ts);
		if (err < 0)
			goto nvi_irq_thread_exit_clear;

		nvi_sync(inf, ts);
		goto nvi_irq_thread_exit;
	}

	/* handle FIFO disabled data */
	sync = false;
	ts = nvi_ts_ns();
	if (((~inf->hw.pwr_mgmt_2) & BIT_PWR_ACCL_STBY) &&
			       (!(inf->hw.fifo_en & inf->reg->accl_fifo_en))) {
		err = nvi_accl_read(inf, ts);
		if (err > 0)
			sync = true;
	}
	if ((!(inf->hw.pwr_mgmt_1 & inf->reg->temp_dis)) &&
				     (!(inf->hw.fifo_en & BIT_TEMP_FIFO_EN))) {
		err = inv_i2c_read(inf, inf->reg->temperature, 2, inf->buf);
		if (!err)
			nvi_report_temp(inf, inf->buf, ts);
	}
	mask = (BIT_GYRO_XOUT | BIT_GYRO_YOUT | BIT_GYRO_ZOUT);
	mask &= ~inf->hw.fifo_en;
	mask >>= 4;
	if (inf->chip_config.gyro_enable && mask) {
		buf_index = 0;
		err = 0;
		if (mask & 4) {
			err = inv_i2c_read(inf, inf->reg->raw_gyro,
					   2, &inf->buf[buf_index]);
			buf_index = 2;
		}
		if (mask & 2) {
			err |= inv_i2c_read(inf, inf->reg->raw_gyro + 2,
					    2, &inf->buf[buf_index]);
			buf_index += 2;
		}
		if (mask & 1)
			err |= inv_i2c_read(inf, inf->reg->raw_gyro + 4,
					    2, &inf->buf[buf_index]);
		if (!err) {
			buf_index = nvi_report_gyro(inf, inf->buf, mask, ts);
			if (buf_index)
				sync = true;
		}
	}
	if (sync)
		nvi_sync(inf, ts);
	nvi_aux_read(inf);
	if (!(inf->hw.user_ctrl & BIT_FIFO_EN))
		goto nvi_irq_thread_exit;

	/* handle FIFO enabled data */
	fifo_sample_size = inf->fifo_sample_size;
	if (!fifo_sample_size)
		goto nvi_irq_thread_exit;

	/* must get IRQ timestamp len first for timestamp best-fit algorithm */
	ts_len = kfifo_len(&inf->trigger.timestamps);
	err = inv_i2c_read(inf, inf->reg->fifo_count_h, 2, inf->buf);
	if (err)
		goto nvi_irq_thread_exit;

	fifo_count = be16_to_cpup((__be16 *)(&inf->buf));
	/* FIFO threshold */
	if (inf->chip_config.fifo_thr > fifo_sample_size) {
		if (fifo_count > inf->chip_config.fifo_thr) {
			dev_dbg(&inf->i2c->dev, "FIFO threshold exceeded\n");
			goto nvi_irq_thread_exit_reset;
		}
	}

	fifo_align = fifo_count % fifo_sample_size;
	if (fifo_count < fifo_sample_size + fifo_align)
		goto nvi_irq_thread_exit;

	if (inf->chip_type == INV_MPU3050) {
		/* FIFO HW BUG WAR:
		 * The MPU3050 will fire an IRQ on incomplete sampling of data
		 * to the FIFO causing misalignment of data.  The WAR is to
		 * simply wait for the next IRQ.  This misalignment problem
		 * usually works itself out with the next data sample.
		 * The safety net is a FIFO reset, should the problem not work
		 * itself out by the time the FIFO threshold is reached.
		 */
		if (inf->fifo_reset_3050) {
			if (fifo_align)
				goto nvi_irq_thread_exit;
		} else {
			if (fifo_align != 2)
				goto nvi_irq_thread_exit;
		}
	}

	ts = inf->fifo_ts;
	delay = inf->sample_delay_us * 1000;
	samples = (fifo_count / fifo_sample_size);
	if (inf->dbg & NVI_DBG_SPEW_FIFO)
		dev_info(&inf->i2c->dev,
			 "fifo_count=%u sample_size=%u offset=%u samples=%u\n",
			 fifo_count, fifo_sample_size, fifo_align, samples);
	fifo_rd_n = 0;
	buf_index = 0;
	while (samples) {
		if (buf_index >= fifo_rd_n) {
			fifo_rd_n = sizeof(inf->buf);
			fifo_rd_n -= fifo_align;
			fifo_rd_n /= fifo_sample_size;
			if (samples < fifo_rd_n)
				fifo_rd_n = samples;
			fifo_rd_n *= fifo_sample_size;
			fifo_rd_n += fifo_align;
			if (inf->chip_type == INV_MPU3050)
				fifo_rd_n -= 2; /* FIFO_FOOTER */
			err = inv_i2c_read(inf, inf->reg->fifo_r_w,
					   fifo_rd_n, inf->buf);
			if (err)
				goto nvi_irq_thread_exit;

			buf_index = fifo_align;
			if (inf->chip_type == INV_MPU3050) {
				if (inf->fifo_reset_3050) {
					inf->fifo_reset_3050 = false;
					fifo_align += 2;
				}
			}
		}

		if (ts_len) {
			len = ts_len;
			for (i = 0; i < len; i++) {
				err = kfifo_out_peek(&inf->trigger.timestamps,
						     &ts_irq, 1);
				if (err != 1)
					goto nvi_irq_thread_exit_reset;

				if (ts < (ts_irq - delay))
					break;

				err = kfifo_to_user(&inf->trigger.timestamps,
						    &ts_irq, sizeof(ts_irq),
						    &copied);
				if (err)
					goto nvi_irq_thread_exit_reset;

				ts_len--;
				if (ts < (ts_irq + delay)) {
					ts = ts_irq;
					break;
				}
			}
			if ((inf->dbg & NVI_DBG_SPEW_FIFO) && (ts != ts_irq))
				dev_info(&inf->i2c->dev,
					 "%s TS=%lld != IRQ=%lld s=%u i=%u\n",
					__func__, ts, ts_irq, samples, ts_len);
		} else {
			if (inf->dbg & NVI_DBG_SPEW_FIFO)
				dev_info(&inf->i2c->dev,
					 "%s NO IRQ_TS TS=%lld s=%u\n",
					 __func__, ts, samples);
		}
		if (inf->chip_type == INV_MPU3050) {
			buf_index = nvi_fifo_read_gyro(inf, buf_index, ts);
			buf_index = nvi_fifo_read_accl(inf, buf_index, ts);
			buf_index += 2; /* FIFO_FOOTER */
		} else {
			buf_index = nvi_fifo_read_accl(inf, buf_index, ts);
			buf_index = nvi_fifo_read_gyro(inf, buf_index, ts);
		}
		nvi_sync(inf, ts);
		for (i = 0; i < AUX_PORT_SPECIAL; i++) {
			ap = &inf->aux.port[i];
			if (ap->fifo_en &&
				      (inf->hw.i2c_slv_ctrl[i] & BIT_SLV_EN)) {
				len = ap->nmp.ctrl & BITS_I2C_SLV_CTRL_LEN;
				if (ap->nmp.handler != NULL)
					ap->nmp.handler(&inf->buf[buf_index],
							len, ts,
							ap->nmp.ext_driver);
				buf_index += len;
			}
		}
		ts += delay;
		samples--;
	}
	if (ts_len) {
		if (inf->dbg & NVI_DBG_SPEW_FIFO)
			dev_info(&inf->i2c->dev, "%s SYNC TO IRQ_TS %lld\n",
				 __func__, ts);
		for (i = 0; i < ts_len; i++) {
			err = kfifo_to_user(&inf->trigger.timestamps,
					    &ts, sizeof(ts), &copied);
			if (err)
				goto nvi_irq_thread_exit_reset;
		}
	}

	inf->fifo_ts = ts;
nvi_irq_thread_exit:
	return IRQ_HANDLED;

nvi_irq_thread_exit_clear:
	/* Ensure HW touched to clear IRQ */
	inv_i2c_read(inf, inf->reg->int_status, 1, inf->buf);
	return IRQ_HANDLED;

nvi_irq_thread_exit_reset:
	if (inf->dbg & NVI_DBG_SPEW_FIFO)
		dev_info(&inf->i2c->dev,
			 "%s fifo_count=%u fifo_sample_size=%u\n",
			 __func__, fifo_count, fifo_sample_size);
	nvi_reset(inf, true, false);
	return IRQ_HANDLED;
}

static irqreturn_t nvi_irq_handler(int irq, void *dev_id)
{
	struct inv_gyro_state_s *inf;
	s64 timestamp;
	inf = (struct inv_gyro_state_s *)dev_id;
	spin_lock(&inf->time_stamp_lock);
	++(inf->num_int);
	if (inf->num_int >= 1000) {
		timestamp = nvi_ts_ns();
		/*
		 * If it takes less than half the time the shortest possible
		 * time to get 1000 interrupts, reset gyro
		 *
		 * minimum time in us to get 1000 interrupts =
		 * 1000 interrupts / 1 / NVI_DELAY_US_MIN
		 * = NVI_DELAY_US_MIN * 1000
		 */
		if ((timestamp >= inf->last_1000)
			&& (((timestamp - inf->last_1000) / ONE_K_HZ) <
				(NVI_DELAY_US_MIN * 500))
			) {
			dev_err(inf->inv_dev, "The number of interrupts from " \
				"gyro is excessive. Scheduling " \
				"disable/enable\n");
			schedule_work(&inf->work_struct);
			if (!inf->irq_disabled) {
				disable_irq_nosync(irq);
				inf->irq_disabled = true;
			}
		}
		inf->num_int = 0;
		inf->last_1000 = timestamp;
	}
	if (inf->hw.user_ctrl & BIT_FIFO_EN) {
		timestamp = nvi_ts_ns();
		kfifo_in(&inf->trigger.timestamps, &timestamp, 1);
	}
	spin_unlock(&inf->time_stamp_lock);
	return IRQ_WAKE_THREAD;
}

static ssize_t nvi_data_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct inv_gyro_state_s *inf;
	unsigned int data_info;
	int err;

	inf = dev_get_drvdata(dev);
	err = kstrtouint(buf, 10, &data_info);
	if (err)
		return -EINVAL;

	if (data_info >= NVI_DATA_INFO_LIMIT_MAX)
		return -EINVAL;

	dev_dbg(&inf->i2c->dev, "%s %u\n", __func__, data_info);
	inf->data_info = data_info;
	switch (data_info) {
	case NVI_DATA_INFO_DATA:
		inf->dbg = 0;
		break;

	case NVI_DATA_INFO_DBG:
		inf->dbg ^= NVI_DBG_SPEW_MSG;
		break;

	case NVI_DATA_INFO_AUX_SPEW:
		inf->dbg ^= NVI_DBG_SPEW_AUX;
		nvi_aux_dbg(inf, "SNAPSHOT", 0);
		break;

	case NVI_DATA_INFO_GYRO_SPEW:
		inf->dbg ^= NVI_DBG_SPEW_GYRO;
		break;

	case NVI_DATA_INFO_TEMP_SPEW:
		inf->dbg ^= NVI_DBG_SPEW_TEMP;
		break;

	case NVI_DATA_INFO_ACCL_SPEW:
		inf->dbg ^= NVI_DBG_SPEW_ACCL;
		break;

	case NVI_DATA_INFO_ACCL_RAW_SPEW:
		inf->dbg ^= NVI_DBG_SPEW_ACCL_RAW;
		break;

	case NVI_DATA_INFO_FIFO_SPEW:
		inf->dbg ^= NVI_DBG_SPEW_FIFO;
		break;

	default:
		break;
	}

	return count;
}

static ssize_t nvi_data_show(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	struct inv_gyro_state_s *inf;
	ssize_t t = 0;
	enum NVI_DATA_INFO data_info;
	char data;
	unsigned int i;
	int err;

	inf = dev_get_drvdata(dev);
	data_info = inf->data_info;
	inf->data_info = NVI_DATA_INFO_DATA;
	switch (data_info) {
	case NVI_DATA_INFO_DATA:
		t = sprintf(buf, "ACCL_RAW %hd %hd %hd\n",
			    inf->accl_raw[AXIS_X],
			    inf->accl_raw[AXIS_Y],
			    inf->accl_raw[AXIS_Z]);
		t += sprintf(buf + t, "ACCL %hd %hd %hd\n",
			     inf->accl[AXIS_X],
			     inf->accl[AXIS_Y],
			     inf->accl[AXIS_Z]);
		t += sprintf(buf + t, "GYRO %hd %hd %hd\n",
			     inf->gyro[AXIS_X],
			     inf->gyro[AXIS_Y],
			     inf->gyro[AXIS_Z]);
		mutex_lock(&inf->mutex_temp);
		t += sprintf(buf + t, "TEMP %hd %lld\n",
			     inf->temp_val,
			     inf->temp_ts);
		mutex_unlock(&inf->mutex_temp);
		return t;

	case NVI_DATA_INFO_VER:
		return sprintf(buf, "version=%u\n", NVI_VERSION);

	case NVI_DATA_INFO_RESET:
		mutex_lock(&inf->mutex);
		err = nvi_pm(inf, NVI_PM_OFF_FORCE);
		err |= nvi_pm(inf, NVI_PM_AUTO);
		mutex_unlock(&inf->mutex);
		if (err)
			return sprintf(buf, "reset ERR\n");
		else
			return sprintf(buf, "reset done\n");

	case NVI_DATA_INFO_REGS:
		for (i = 0; i < inf->hw_s->num_reg; i++) {
			if (i == inf->reg->fifo_r_w)
				data = 0;
			else
				inv_i2c_read(inf, i, 1, &data);
			t += sprintf(buf + t, "%#2x=%#2x\n", i, data);
		}
		return t;

	case NVI_DATA_INFO_DBG:
		return sprintf(buf, "DBG spew=%x\n",
			       !!(inf->dbg & NVI_DBG_SPEW_MSG));

	case NVI_DATA_INFO_AUX_SPEW:
		return sprintf(buf, "AUX spew=%x\n",
			       !!(inf->dbg & NVI_DBG_SPEW_AUX));

	case NVI_DATA_INFO_GYRO_SPEW:
		return sprintf(buf, "GYRO spew=%x\n",
			       !!(inf->dbg & NVI_DBG_SPEW_GYRO));

	case NVI_DATA_INFO_TEMP_SPEW:
		return sprintf(buf, "TEMP spew=%x\n",
			       !!(inf->dbg & NVI_DBG_SPEW_TEMP));

	case NVI_DATA_INFO_ACCL_SPEW:
		return sprintf(buf, "ACCL spew=%x\n",
			       !!(inf->dbg & NVI_DBG_SPEW_ACCL));

	case NVI_DATA_INFO_ACCL_RAW_SPEW:
		return sprintf(buf, "ACCL_RAW spew=%x\n",
			       !!(inf->dbg & NVI_DBG_SPEW_ACCL_RAW));

	case NVI_DATA_INFO_FIFO_SPEW:
		return sprintf(buf, "FIFO spew=%x\n",
			       !!(inf->dbg & NVI_DBG_SPEW_FIFO));

	default:
		break;
	}

	return -EINVAL;
}

#if DEBUG_SYSFS_INTERFACE
static ssize_t nvi_dbg_i2c_addr_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct inv_gyro_state_s *inf;
	u16 dbg_i2c_addr;
	int err;

	inf = dev_get_drvdata(dev);
	err = kstrtou16(buf, 16, &dbg_i2c_addr);
	if (err)
		return -EINVAL;

	inf->dbg_i2c_addr = dbg_i2c_addr;
	return count;
}

static ssize_t nvi_dbg_i2c_addr_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct inv_gyro_state_s *inf;
	ssize_t ret = 0;
	u16 dbg_i2c_addr;

	inf = dev_get_drvdata(dev);
	if (inf->dbg_i2c_addr)
		dbg_i2c_addr = inf->dbg_i2c_addr;
	else
		dbg_i2c_addr = inf->i2c->addr;
	ret += sprintf(buf + ret, "%#2x\n", dbg_i2c_addr);
	return ret;
}

static ssize_t nvi_dbg_reg_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct inv_gyro_state_s *inf;
	u8 dbg_reg;
	int err;

	inf = dev_get_drvdata(dev);
	err = kstrtou8(buf, 16, &dbg_reg);
	if (err)
		return -EINVAL;

	inf->dbg_reg = dbg_reg;
	return count;
}

static ssize_t nvi_dbg_reg_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct inv_gyro_state_s *inf;
	ssize_t ret = 0;

	inf = dev_get_drvdata(dev);
	ret += sprintf(buf + ret, "%#2x\n", inf->dbg_reg);
	return ret;
}

static ssize_t nvi_dbg_dat_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct inv_gyro_state_s *inf;
	u16 dbg_i2c_addr;
	u8 dbg_dat;
	int err;

	inf = dev_get_drvdata(dev);
	err = kstrtou8(buf, 16, &dbg_dat);
	if (err)
		return -EINVAL;

	if (inf->dbg_i2c_addr)
		dbg_i2c_addr = inf->dbg_i2c_addr;
	else
		dbg_i2c_addr = inf->i2c->addr;
	err = inv_i2c_single_write_base(inf, dbg_i2c_addr,
					inf->dbg_reg, dbg_dat);
	pr_info("%s dev=%x reg=%x data=%x err=%d\n",
		__func__, dbg_i2c_addr, inf->dbg_reg, dbg_dat, err);
	return count;
}

static ssize_t nvi_dbg_dat_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct inv_gyro_state_s *inf;
	ssize_t ret = 0;
	u16 dbg_i2c_addr;
	u8 dbg_dat = 0;
	int err;

	inf = dev_get_drvdata(dev);
	if (inf->dbg_i2c_addr)
		dbg_i2c_addr = inf->dbg_i2c_addr;
	else
		dbg_i2c_addr = inf->i2c->addr;
	err = inv_i2c_read_base(inf, dbg_i2c_addr, inf->dbg_reg, 1, &dbg_dat);
	ret += sprintf(buf + ret, "%s dev=%x reg=%x data=%x err=%d\n",
		       __func__, dbg_i2c_addr, inf->dbg_reg, dbg_dat, err);
	return ret;
}
#endif /* DEBUG_SYSFS_INTERFACE */

static DEVICE_ATTR(gyro_enable, S_IRUGO | S_IWUSR | S_IWGRP,
		   nvi_gyro_enable_show, nvi_gyro_enable_store);
static DEVICE_ATTR(gyro_fifo_enable, S_IRUGO | S_IWUSR | S_IWGRP,
		   nvi_gyro_fifo_enable_show, nvi_gyro_fifo_enable_store);
static DEVICE_ATTR(gyro_delay, S_IRUGO | S_IWUSR | S_IWGRP,
		   nvi_gyro_delay_show, inv_gyro_delay_store);
static DEVICE_ATTR(gyro_resolution, S_IRUGO | S_IWUSR | S_IWGRP,
		   nvi_gyro_resolution_show, nvi_gyro_resolution_store);
static DEVICE_ATTR(gyro_max_range, S_IRUGO | S_IWUSR | S_IWGRP,
		   nvi_gyro_max_range_show, nvi_gyro_max_range_store);
static DEVICE_ATTR(gyro_orientation, S_IRUGO,
		   inv_gyro_orientation_show, NULL);
static DEVICE_ATTR(raw_gyro, S_IRUGO,
		   inv_raw_gyro_show, NULL);
static DEVICE_ATTR(accl_enable, S_IRUGO | S_IWUSR | S_IWGRP,
		   nvi_accl_enable_show, nvi_accl_enable_store);
static DEVICE_ATTR(accl_fifo_enable, S_IRUGO | S_IWUSR | S_IWGRP,
		   nvi_accl_fifo_enable_show, nvi_accl_fifo_enable_store);
static DEVICE_ATTR(accl_delay, S_IRUGO | S_IWUSR | S_IWGRP,
		   nvi_accl_delay_show, nvi_accl_delay_store);
static DEVICE_ATTR(accl_resolution, S_IRUGO | S_IWUSR | S_IWGRP,
		   nvi_accl_resolution_show, nvi_accl_resolution_store);
static DEVICE_ATTR(accl_max_range, S_IRUGO | S_IWUSR | S_IWGRP,
		   nvi_accl_max_range_show, nvi_accl_max_range_store);
static DEVICE_ATTR(accl_orientation, S_IRUGO,
		   inv_accl_matrix_show, NULL);
static DEVICE_ATTR(raw_accl, S_IRUGO,
		   inv_raw_accl_show, NULL);
static DEVICE_ATTR(accl_bias, S_IRUGO,
		   inv_get_accl_bias_show, NULL);
static DEVICE_ATTR(lpa_delay, S_IRUGO | S_IWUSR | S_IWGRP,
		   nvi_lpa_delay_enable_show, nvi_lpa_delay_enable_store);
static DEVICE_ATTR(motion_enable, S_IRUGO | S_IWUSR | S_IWGRP,
		   nvi_mot_enable_show, nvi_mot_enable_store);
static DEVICE_ATTR(motion_threshold, S_IRUGO | S_IWUSR | S_IWGRP,
		   nvi_motion_thr_show, nvi_motion_thr_store);
static DEVICE_ATTR(motion_duration, S_IRUGO | S_IWUSR | S_IWGRP,
		   nvi_motion_dur_show, nvi_motion_dur_store);
static DEVICE_ATTR(motion_count, S_IRUGO | S_IWUSR | S_IWGRP,
		   nvi_motion_count_show, nvi_motion_count_store);
static DEVICE_ATTR(motion_control, S_IRUGO | S_IWUSR | S_IWGRP,
		   nvi_motion_ctrl_show, nvi_motion_ctrl_store);
static DEVICE_ATTR(temp_enable, S_IRUGO | S_IWUSR | S_IWGRP,
		   nvi_temp_enable_show, nvi_temp_enable_store);
static DEVICE_ATTR(temp_fifo_enable, S_IRUGO | S_IWUSR | S_IWGRP,
		   nvi_temp_fifo_enable_show, nvi_temp_fifo_enable_store);
static DEVICE_ATTR(temp_scale, S_IRUGO,
		   inv_temp_scale_show, NULL);
static DEVICE_ATTR(temp_offset, S_IRUGO,
		   inv_temp_offset_show, NULL);
static DEVICE_ATTR(temperature, S_IRUGO,
		   inv_temperature_show, NULL);
static DEVICE_ATTR(bypass_timeout_ms, S_IRUGO | S_IWUSR | S_IWGRP,
		   nvi_bypass_timeout_ms_show, nvi_bypass_timeout_ms_store);
static DEVICE_ATTR(min_delay_us, S_IRUGO | S_IWUSR | S_IWGRP,
		   nvi_min_delay_us_show, nvi_min_delay_us_store);
static DEVICE_ATTR(fifo_threshold, S_IRUGO | S_IWUSR | S_IWGRP,
		   nvi_fifo_thr_show, nvi_fifo_thr_store);
static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
		   nvi_enable_show, nvi_enable_store);
static DEVICE_ATTR(self_test, S_IRUGO,
		   inv_self_test_show, NULL);
static DEVICE_ATTR(data, S_IRUGO | S_IWUSR | S_IWGRP,
		   nvi_data_show, nvi_data_store);
static DEVICE_ATTR(dbg_reg, S_IRUGO | S_IWUSR | S_IWGRP,
		   nvi_dbg_reg_show, nvi_dbg_reg_store);
static DEVICE_ATTR(dbg_dat, S_IRUGO | S_IWUSR | S_IWGRP,
		   nvi_dbg_dat_show, nvi_dbg_dat_store);
static DEVICE_ATTR(dbg_i2c_addr, S_IRUGO | S_IWUSR | S_IWGRP,
		   nvi_dbg_i2c_addr_show, nvi_dbg_i2c_addr_store);
static DEVICE_ATTR(reg_dump, S_IRUGO,
		   inv_reg_dump_show, NULL);
static DEVICE_ATTR(power_state, S_IRUGO | S_IWUSR | S_IWGRP,
		   inv_power_state_show, inv_power_state_store);
static DEVICE_ATTR(key, S_IRUGO | S_IWUSR | S_IWGRP,
		   inv_key_show, inv_key_store);

static struct device_attribute *inv_attributes[] = {
	&dev_attr_accl_enable,
	&dev_attr_accl_fifo_enable,
	&dev_attr_accl_delay,
	&dev_attr_accl_max_range,
	&dev_attr_accl_orientation,
	&dev_attr_raw_accl,
	&dev_attr_gyro_enable,
	&dev_attr_gyro_fifo_enable,
	&dev_attr_gyro_max_range,
	&dev_attr_gyro_delay,
	&dev_attr_gyro_orientation,
	&dev_attr_raw_gyro,
	&dev_attr_temp_scale,
	&dev_attr_temp_offset,
	&dev_attr_temperature,
	&dev_attr_reg_dump,
	&dev_attr_self_test,
	&dev_attr_enable,
	&dev_attr_power_state,
	&dev_attr_key,
#if DEBUG_SYSFS_INTERFACE
	&dev_attr_dbg_reg,
	&dev_attr_dbg_dat,
	&dev_attr_dbg_i2c_addr,
#endif /* DEBUG_SYSFS_INTERFACE */
	NULL
};

static struct device_attribute *inv_mpu6050_attributes[] = {
	&dev_attr_gyro_resolution,
	&dev_attr_temp_enable,
	&dev_attr_temp_fifo_enable,
	&dev_attr_accl_resolution,
	&dev_attr_accl_bias,
	&dev_attr_lpa_delay,
	&dev_attr_motion_enable,
	&dev_attr_motion_threshold,
	&dev_attr_motion_duration,
	&dev_attr_motion_count,
	&dev_attr_motion_control,
	&dev_attr_bypass_timeout_ms,
	&dev_attr_min_delay_us,
	&dev_attr_fifo_threshold,
	&dev_attr_data,
	NULL
};

static void inv_input_close(struct input_dev *d)
{
	struct inv_gyro_state_s *inf;

	inf = input_get_drvdata(d);
	nvi_pm_exit(inf);
}

/**
 *  inv_setup_input() - internal setup input device.
 *  @st:	Device driver instance.
 *  @**idev_in  pointer to input device
 *  @*client    i2c client
 *  @*name      name of the input device.
 */
static int inv_setup_input(struct inv_gyro_state_s *st,
			   struct input_dev **idev_in,
			   struct i2c_client *client, unsigned char *name)
{
	int result;
	struct input_dev *idev;

	idev = input_allocate_device();
	if (!idev) {
		result = -ENOMEM;
		return result;
	}

	/* Setup input device. */
	idev->name = name;
	idev->id.bustype = BUS_I2C;
	idev->id.product = 'S';
	idev->id.vendor     = ('I'<<8) | 'S';
	idev->id.version    = 1;
	idev->dev.parent = &client->dev;
	/* Open and close method. */
	if (strcmp(name, "INV_DMP"))
		idev->close = inv_input_close;
	input_set_capability(idev, EV_REL, REL_X);
	input_set_capability(idev, EV_REL, REL_Y);
	input_set_capability(idev, EV_REL, REL_Z);
	input_set_capability(idev, EV_REL, REL_RX);
	input_set_capability(idev, EV_REL, REL_RY);
	input_set_capability(idev, EV_REL, REL_RZ);
	input_set_capability(idev, EV_REL, REL_MISC);
	input_set_capability(idev, EV_REL, REL_WHEEL);
	input_set_drvdata(idev, st);
	result = input_register_device(idev);
	if (result)
		input_free_device(idev);
	*idev_in = idev;
	return result;
}

static int inv_create_input(struct inv_gyro_state_s *st,
		struct i2c_client *client){
	struct input_dev *idev;
	int result;

	idev = NULL;
	result = inv_setup_input(st, &idev, client, st->hw_s->name);
	if (result)
		return result;

	st->idev = idev;
	if (INV_ITG3500 == st->chip_type)
		return 0;

	result = inv_setup_input(st, &idev, client, "INV_DMP");
	if (result)
		input_unregister_device(st->idev);
	else
		st->idev_dmp = idev;
	return 0;
}

int create_device_attributes(struct device *dev,
	struct device_attribute **attrs)
{
	int i;
	int err = 0;

	for (i = 0 ; NULL != attrs[i] ; ++i) {
		err = sysfs_create_file(&dev->kobj, &attrs[i]->attr);
		if (0 != err)
			break;
	}
	if (0 != err) {
		for (; i >= 0 ; --i)
			sysfs_remove_file(&dev->kobj, &attrs[i]->attr);
	}
	return err;
}

void remove_device_attributes(struct device *dev,
	struct device_attribute **attrs)
{
	int i;

	for (i = 0 ; NULL != attrs[i] ; ++i)
		device_remove_file(dev, attrs[i]);
}

static char const *const inv_class_name = "invensense";
static char const *const inv_device_name = "mpu";
static dev_t const inv_device_dev_t = MKDEV(MISC_MAJOR, MISC_DYNAMIC_MINOR);
static struct bin_attribute dmp_firmware = {
	.attr = {
		.name = "dmp_firmware",
		.mode = S_IRUGO | S_IWUSR
	},
	.size = 4096,
	.read = inv_dmp_firmware_read,
	.write = inv_dmp_firmware_write,
};

static int create_sysfs_interfaces(struct inv_gyro_state_s *st)
{
	int result;
	result = 0;

	st->inv_class = class_create(THIS_MODULE, inv_class_name);
	if (IS_ERR(st->inv_class)) {
		result = PTR_ERR(st->inv_class);
		goto exit_nullify_class;
	}

	st->inv_dev = device_create(st->inv_class, &st->i2c->dev,
			inv_device_dev_t, st, inv_device_name);
	if (IS_ERR(st->inv_dev)) {
		result = PTR_ERR(st->inv_dev);
		goto exit_destroy_class;
	}

	result = create_device_attributes(st->inv_dev, inv_attributes);
	if (result < 0)
		goto exit_destroy_device;

	if (INV_ITG3500 == st->chip_type)
		return 0;

	result = sysfs_create_bin_file(&st->inv_dev->kobj, &dmp_firmware);
	if (result < 0)
		goto exit_remove_device_attributes;

	if (INV_MPU3050 == st->chip_type)
		return 0;

	result = create_device_attributes(st->inv_dev, inv_mpu6050_attributes);
	if (result < 0)
		goto exit_remove_bin_file;

	return 0;

exit_remove_bin_file:
	sysfs_remove_bin_file(&st->inv_dev->kobj, &dmp_firmware);
exit_remove_device_attributes:
	remove_device_attributes(st->inv_dev, inv_attributes);
exit_destroy_device:
	device_destroy(st->inv_class, inv_device_dev_t);
exit_destroy_class:
	st->inv_dev = NULL;
	class_destroy(st->inv_class);
exit_nullify_class:
	st->inv_class = NULL;
	return result;
}

static void remove_sysfs_interfaces(struct inv_gyro_state_s *st)
{
	remove_device_attributes(st->inv_dev, inv_attributes);
	if (INV_ITG3500 != st->chip_type)
		sysfs_remove_bin_file(&st->inv_dev->kobj, &dmp_firmware);
	if ((INV_ITG3500 != st->chip_type) && (INV_MPU3050 != st->chip_type))
		remove_device_attributes(st->inv_dev, inv_mpu6050_attributes);
	device_destroy(st->inv_class, inv_device_dev_t);
	class_destroy(st->inv_class);
	st->inv_dev = NULL;
	st->inv_class = NULL;
}

static void nvi_init_config(struct inv_gyro_state_s *inf)
{
	inf->chip_config.bypass_timeout_ms = NVI_BYPASS_TIMEOUT_MS;
	inf->chip_config.min_delay_us = NVI_DELAY_US_MIN;
	inf->chip_config.fifo_thr = 1;
	inf->chip_config.temp_fifo_enable = 1;
	inf->chip_config.lpf = INV_FILTER_42HZ;
	inf->chip_config.gyro_enable = 0;
	inf->chip_config.gyro_fifo_enable = 0;
	inf->chip_config.gyro_fsr = INV_FSR_2000DPS;
	inf->chip_config.gyro_start_delay_ns = 100000000;
	inf->chip_config.accl_enable = 0;
	inf->chip_config.accl_fifo_enable = 0;
	inf->chip_config.accl_fsr = INV_FS_02G;
	inf->chip_config.mot_enable = NVI_MOT_DIS;
	inf->chip_config.mot_dur = 1;
	inf->chip_config.mot_ctrl = 1;
	inf->chip_config.mot_cnt = 10;
	inf->chip_config.enable = 0;
	inf->chip_config.prog_start_addr = DMP_START_ADDR;
}

static int nvi_dev_init(struct inv_gyro_state_s *inf,
			const struct i2c_device_id *id)
{
	u8 dev_id;
	u8 val;
	int err = 0;

	dev_id = 0;
	if (!strcmp(id->name, "itg3500")) {
		inf->chip_type = INV_ITG3500;
	} else if (!strcmp(id->name, "mpu3050")) {
		inf->chip_type = INV_MPU3050;
		inv_setup_reg_mpu3050(inf->reg);
	} else if (!strcmp(id->name, "mpu6050")) {
		inf->chip_type = INV_MPU6050;
		dev_id = MPU6050_ID;
	} else if (!strcmp(id->name, "mpu9150")) {
		inf->chip_type = INV_MPU6050;
		dev_id = MPU6050_ID;
	} else if (!strcmp(id->name, "mpu6500")) {
		inf->chip_type = INV_MPU6500;
		dev_id = MPU6500_ID;
	} else if (!strcmp(id->name, "mpu6515")) {
		inf->chip_type = INV_MPU6500;
		dev_id = MPU6515_ID;
	} else if (!strcmp(id->name, "mpu9250")) {
		inf->chip_type = INV_MPU6500;
		dev_id = MPU9250_ID;
	} else if (!strcmp(id->name, "mpu6xxx")) {
		inf->chip_type = INV_MPU6050;
		dev_id = 0xFF;
	} else {
		return -ENODEV;
	}

	nvi_pm_init(inf);
	if (dev_id) {
		err = inv_i2c_read(inf, inf->reg->who_am_i, 1, &val);
		if (err) {
			dev_err(&inf->i2c->dev, "%s I2C ID READ ERR\n",
				__func__);
			if (dev_id == 0xFF) {
				dev_err(&inf->i2c->dev, "%s AUTO ID FAILED\n",
					__func__);
				return -EPERM;
			}
		} else {
			if ((dev_id != 0xFF) && (dev_id != val))
				dev_err(&inf->i2c->dev, "%s %s_ID %x != %x\n",
					__func__, id->name, dev_id, val);
			switch (val) {
			case MPU6050_ID:
				inf->chip_type = INV_MPU6050;
				break;

			case MPU6500_ID:
				inf->chip_type = INV_MPU6500;
				break;

			case MPU9250_ID:
				inf->chip_type = INV_MPU6500;
				break;

			case MPU6515_ID:
				inf->chip_type = INV_MPU6500;
				break;

			default:
				dev_err(&inf->i2c->dev, "%s ERR: NO ID %x\n",
					__func__, val);
			}
		}
	}

	inf->hw_s = (struct inv_hw_s *)(hw_info + inf->chip_type);
	dev_dbg(&inf->i2c->dev, "%s: BRD_CFG=%s ID=%x USING: %s\n",
		__func__, id->name, val, inf->hw_s->name);
	nvi_init_config(inf);
	switch (inf->chip_type) {
	case INV_ITG3500:
		inf->hal.fifo_size = NVI_FIFO_SIZE_3050;
		break;

	case INV_MPU3050:
		inf->hal.fifo_size = NVI_FIFO_SIZE_3050;
		err = inv_init_config_mpu3050(inf);
		break;

	case INV_MPU6050:
		inf->hal.fifo_size = NVI_FIFO_SIZE_6050;
		inf->hal.lpa_tbl = &nvi_lpa_delay_us_tbl_6050[0];
		inf->hal.lpa_tbl_n = ARRAY_SIZE(nvi_lpa_delay_us_tbl_6050);
		err = inv_get_silicon_rev_mpu6050(inf);
		break;

	case INV_MPU6500:
		inf->hal.fifo_size = NVI_FIFO_SIZE_6500;
		inf->hal.lpa_tbl = &nvi_lpa_delay_us_tbl_6500[0];
		inf->hal.lpa_tbl_n = ARRAY_SIZE(nvi_lpa_delay_us_tbl_6500);
		err = inv_get_silicon_rev_mpu6500(inf);
		break;

	default:
		err = -ENODEV;
		break;
	}

	mutex_lock(&inf->mutex);
	nvi_pm(inf, NVI_PM_OFF);
	mutex_unlock(&inf->mutex);
	return err;
}

static int nvi_suspend(struct device *dev)
{
	struct inv_gyro_state_s *inf;
	int err;
	unsigned long flags;
	inf = dev_get_drvdata(dev);

	spin_lock_irqsave(&inf->time_stamp_lock, flags);
	if (!inf->irq_disabled)
		disable_irq_nosync(inf->i2c->irq);
	inf->irq_disabled = true;
	inf->stop_workqueue = true;
	spin_unlock_irqrestore(&inf->time_stamp_lock, flags);
	synchronize_irq(inf->i2c->irq);

	flush_work_sync(&inf->work_struct);

	mutex_lock(&inf->mutex);
	inf->suspend = true;
	err = nvi_pm(inf, NVI_PM_OFF);
	mutex_unlock(&inf->mutex);
	if (err)
		dev_err(dev, "%s ERR\n", __func__);
	if (inf->dbg & NVI_DBG_SPEW_MSG)
		dev_info(dev, "%s done\n", __func__);
	return 0;
}

static int nvi_resume(struct device *dev)
{
	struct inv_gyro_state_s *inf;
	unsigned long flags;
	inf = dev_get_drvdata(dev);
	spin_lock_irqsave(&inf->time_stamp_lock, flags);
	if (inf->irq_disabled)
		enable_irq(inf->i2c->irq);
	inf->irq_disabled = false;
	inf->stop_workqueue = false;
	spin_unlock_irqrestore(&inf->time_stamp_lock, flags);

	mutex_lock(&inf->mutex);
	BUG_ON(!inf->suspend);
	inf->suspend = false;
	mutex_unlock(&inf->mutex);
	if (inf->dbg & NVI_DBG_SPEW_MSG)
		dev_info(dev, "%s done\n", __func__);
	return 0;
}

static const struct dev_pm_ops nvi_pm_ops = {
	.suspend = nvi_suspend,
	.resume = nvi_resume,
};

static void nvi_shutdown(struct i2c_client *client)
{
	struct inv_gyro_state_s *inf;
	int i;
	unsigned long flags;
	inf = i2c_get_clientdata(client);
	if (inf == NULL)
		return;

	spin_lock_irqsave(&inf->time_stamp_lock, flags);
	if (!inf->irq_disabled)
		disable_irq_nosync(client->irq);
	inf->irq_disabled = true;
	inf->stop_workqueue = true;
	spin_unlock_irqrestore(&inf->time_stamp_lock, flags);
	synchronize_irq(inf->i2c->irq);

	flush_work_sync(&inf->work_struct);

	mutex_lock(&inf->mutex);
	for (i = 0; i < AUX_PORT_SPECIAL; i++) {
		if (inf->aux.port[i].nmp.shutdown_bypass) {
			nvi_aux_bypass_enable(inf, true);
			break;
		}
	}
	inf->shutdown = true;
	if (inf->inv_dev)
		remove_sysfs_interfaces(inf);
	free_irq(client->irq, inf);
	mutex_unlock(&inf->mutex);
	if (inf->idev)
		input_unregister_device(inf->idev);
	if ((INV_ITG3500 != inf->chip_type) && (inf->idev_dmp))
		input_unregister_device(inf->idev_dmp);
}

static int nvi_remove(struct i2c_client *client)
{
	struct inv_gyro_state_s *inf;

	nvi_shutdown(client);
	inf = i2c_get_clientdata(client);
	if (inf != NULL) {
		nvi_pm_exit(inf);
		kfifo_free(&inf->trigger.timestamps);
		kfree(inf);
	}
	dev_info(&client->dev, "%s\n", __func__);
	return 0;
}

static int of_nvi_parse_platform_data(struct i2c_client *client,
				struct mpu_platform_data *pdata)
{
	struct device_node *np = client->dev.of_node;
	char const *pchar;
	u32 tmp;
	int len;

	if (!pdata) {
		dev_err(&client->dev, "Platform data hasn't been allocated\n");
		return -EINVAL;
	}

	if (!of_property_read_u32_array(np, "invensense,int_config", &tmp, 1))
		pdata->int_config = (u8)tmp;
	else
		dev_err(&client->dev, "Can't read int_config property\n");

	if (!of_property_read_u32_array(np, "invensense,level_shifter",
							&tmp, 1))
		pdata->level_shifter = (u8)tmp;
	else
		dev_err(&client->dev, "Can't read level_shifter property\n");

	pchar = of_get_property(np, "invensense,orientation", &len);
	if (pchar && len == sizeof(pdata->orientation))
		memcpy(pdata->orientation, pchar, len);
	else
		dev_err(&client->dev, "Can't read orientation property\n");

	if (!of_property_read_u32_array(np, "invensense,sec_slave_type",
							&tmp, 1))
		pdata->sec_slave_type = (enum secondary_slave_type)tmp;
	else
		dev_err(&client->dev, "Can't read sec_slave_type property\n");

	pchar = of_get_property(np, "invensense,key", &len);
	if (pchar && len == sizeof(pdata->key))
		memcpy(pdata->key, pchar, len);
	else
		dev_err(&client->dev, "Can't read key property\n");

	return 0;
}

static void nvi_work_func(struct work_struct *work)
{
	unsigned long flags;
	int nvi_pm_current;
	struct inv_gyro_state_s *inf =
		container_of(work, struct inv_gyro_state_s, work_struct);
	mutex_lock(&inf->mutex);
	nvi_pm_current = inf->pm;
	nvi_pm(inf, NVI_PM_OFF_FORCE);
	/*
	 * If suspending, no need to revive the power state
	 */
	if (!(inf->suspend)) {
		nvi_pm(inf, nvi_pm_current);
		nvi_reset(inf, true, true);
		nvi_global_delay(inf);
	}

	spin_lock_irqsave(&inf->time_stamp_lock, flags);
	if (inf->irq_disabled && !inf->stop_workqueue
		&& !(inf->suspend || inf->shutdown)) {
		enable_irq(inf->i2c->irq);
		inf->irq_disabled = false;
	}
	spin_unlock_irqrestore(&inf->time_stamp_lock, flags);
	mutex_unlock(&inf->mutex);
	dev_err(inf->inv_dev, "Done resetting gyro\n");
}

static int nvi_probe(struct i2c_client *client,
		     const struct i2c_device_id *id)
{
	struct inv_gyro_state_s *st;
	int result;

	pr_info("%s: Probe name %s\n", __func__, id->name);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		result = -ENODEV;
		goto out_no_free;
	}

	st = kzalloc(sizeof(*st), GFP_KERNEL);
	if (!st) {
		result = -ENOMEM;
		goto out_no_free;
	}
	INIT_WORK(&st->work_struct, nvi_work_func);
	mutex_init(&st->mutex);
	mutex_init(&st->mutex_temp);

	/* Make state variables available to all _show and _store functions. */
	i2c_set_clientdata(client, st);
	st->i2c = client;
	st->sl_handle = client->adapter;
	st->reg = (struct inv_reg_map_s *)&chip_reg ;
	st->hw_s = (struct inv_hw_s *)hw_info;
	st->i2c_addr = client->addr;

	if (client->dev.of_node) {
		result = of_nvi_parse_platform_data(client, &st->plat_data);
		if (result)
			goto out_free;
	} else
		st->plat_data =
		   *(struct mpu_platform_data *)dev_get_platdata(&client->dev);

	result = nvi_dev_init(st, id);
	if (result)
		goto out_free;

	INIT_KFIFO(st->trigger.timestamps);
	result = create_sysfs_interfaces(st);
	if (result)
		goto out_free_kfifo;

	if (!client->irq) {
		dev_err(&client->adapter->dev, "IRQ not assigned.\n");
		result = -EPERM;
		goto out_close_sysfs;
	}

	st->trigger.irq = client->irq;
	result = request_threaded_irq(client->irq,
				      nvi_irq_handler, nvi_irq_thread,
				      IRQF_TRIGGER_RISING | IRQF_SHARED,
				      "inv_irq", st);
	if (result)
		goto out_close_sysfs;

	spin_lock_init(&st->time_stamp_lock);
	result = inv_create_input(st, client);
	if (result) {
		free_irq(client->irq, st);
		goto out_close_sysfs;
	}

	inf_local = st;
	dev_info(&client->adapter->dev, "%s is ready to go\n", st->hw_s->name);
	return 0;

out_close_sysfs:
	remove_sysfs_interfaces(st);
out_free_kfifo:
	kfifo_free(&st->trigger.timestamps);
out_free:
	nvi_pm_exit(st);
	kfree(st);
out_no_free:
	dev_err(&client->adapter->dev, "%s failed %d\n", __func__, result);
	return -EIO;
}

static unsigned short normal_i2c[] = { I2C_CLIENT_END };

/* device id table is used to identify what device can be
 * supported by this driver
 */
static struct i2c_device_id nvi_mpu_id[] = {
	{"itg3500", INV_ITG3500},
	{"mpu3050", INV_MPU3050},
	{"mpu6050", INV_MPU6050},
	{"mpu9150", INV_MPU9150},
	{"mpu6500", INV_MPU6500},
	{"mpu9250", INV_MPU9250},
	{"mpu6xxx", INV_MPU6XXX},
	{"mpu9350", INV_MPU9350},
	{"mpu6515", INV_MPU6515},
	{}
};

MODULE_DEVICE_TABLE(i2c, nvi_mpu_id);

#ifdef CONFIG_OF
static const struct of_device_id nvi_mpu_of_match[] = {
	{ .compatible = "invensense,itg3500", },
	{ .compatible = "invensense,mpu3050", },
	{ .compatible = "invensense,mpu6050", },
	{ .compatible = "invensense,mpu9150", },
	{ .compatible = "invensense,mpu6500", },
	{ .compatible = "invensense,mpu9250", },
	{ .compatible = "invensense,mpu6xxx", },
	{ .compatible = "invensense,mpu9350", },
	{ .compatible = "invensense,mpu6515", },
	{}
};

MODULE_DEVICE_TABLE(of, nvi_mpu_of_match);
#endif

static struct i2c_driver inv_mod_driver = {
	.class = I2C_CLASS_HWMON,
	.probe		=	nvi_probe,
	.remove		=	nvi_remove,
	.id_table	=	nvi_mpu_id,
	.driver = {
		.owner	=	THIS_MODULE,
		.name	=	"inv_dev",
		.of_match_table = of_match_ptr(nvi_mpu_of_match),
#ifdef CONFIG_PM
		.pm	=	&nvi_pm_ops,
#endif
	},
	.address_list = normal_i2c,
	.shutdown	=	nvi_shutdown,
};

static int __init inv_mod_init(void)
{
	int result = i2c_add_driver(&inv_mod_driver);

	if (result) {
		pr_err("%s failed\n", __func__);
		return result;
	}

	return 0;
}

static void __exit inv_mod_exit(void)
{
	i2c_del_driver(&inv_mod_driver);
}

module_init(inv_mod_init);
module_exit(inv_mod_exit);

MODULE_AUTHOR("NVIDIA Corporation");
MODULE_DESCRIPTION("Invensense device driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("inv_dev");

