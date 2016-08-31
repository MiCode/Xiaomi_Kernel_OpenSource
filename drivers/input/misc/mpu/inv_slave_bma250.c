/*
* Copyright (C) 2012 Invensense, Inc.
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
 *      @file    inv_slave_bma250.c
 *      @brief   A sysfs device driver for Invensense devices
 *      @details This file is part of inv_gyro driver code
 *
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
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>

#include "inv_gyro.h"
#define BMA250_CHIP_ID			3
#define BMA250_RANGE_SET		0
#define BMA250_BW_SET			4

/* range and bandwidth */

#define BMA250_RANGE_2G                 0
#define BMA250_RANGE_4G                 1
#define BMA250_RANGE_8G                 2
#define BMA250_RANGE_16G                3


#define BMA250_BW_7_81HZ        0x08
#define BMA250_BW_15_63HZ       0x09
#define BMA250_BW_31_25HZ       0x0A
#define BMA250_BW_62_50HZ       0x0B
#define BMA250_BW_125HZ         0x0C
#define BMA250_BW_250HZ         0x0D
#define BMA250_BW_500HZ         0x0E
#define BMA250_BW_1000HZ        0x0F

/*      register definitions */
#define BMA250_CHIP_ID_REG                      0x00
#define BMA250_VERSION_REG                      0x01
#define BMA250_X_AXIS_LSB_REG                   0x02
#define BMA250_X_AXIS_MSB_REG                   0x03
#define BMA250_Y_AXIS_LSB_REG                   0x04
#define BMA250_Y_AXIS_MSB_REG                   0x05
#define BMA250_Z_AXIS_LSB_REG                   0x06
#define BMA250_Z_AXIS_MSB_REG                   0x07
#define BMA250_TEMP_RD_REG                      0x08
#define BMA250_STATUS1_REG                      0x09
#define BMA250_STATUS2_REG                      0x0A
#define BMA250_STATUS_TAP_SLOPE_REG             0x0B
#define BMA250_STATUS_ORIENT_HIGH_REG           0x0C
#define BMA250_RANGE_SEL_REG                    0x0F
#define BMA250_BW_SEL_REG                       0x10
#define BMA250_MODE_CTRL_REG                    0x11
#define BMA250_LOW_NOISE_CTRL_REG               0x12
#define BMA250_DATA_CTRL_REG                    0x13
#define BMA250_RESET_REG                        0x14
#define BMA250_INT_ENABLE1_REG                  0x16
#define BMA250_INT_ENABLE2_REG                  0x17
#define BMA250_INT1_PAD_SEL_REG                 0x19
#define BMA250_INT_DATA_SEL_REG                 0x1A
#define BMA250_INT2_PAD_SEL_REG                 0x1B
#define BMA250_INT_SRC_REG                      0x1E
#define BMA250_INT_SET_REG                      0x20
#define BMA250_INT_CTRL_REG                     0x21
#define BMA250_LOW_DURN_REG                     0x22
#define BMA250_LOW_THRES_REG                    0x23
#define BMA250_LOW_HIGH_HYST_REG                0x24
#define BMA250_HIGH_DURN_REG                    0x25
#define BMA250_HIGH_THRES_REG                   0x26
#define BMA250_SLOPE_DURN_REG                   0x27
#define BMA250_SLOPE_THRES_REG                  0x28
#define BMA250_TAP_PARAM_REG                    0x2A
#define BMA250_TAP_THRES_REG                    0x2B
#define BMA250_ORIENT_PARAM_REG                 0x2C
#define BMA250_THETA_BLOCK_REG                  0x2D
#define BMA250_THETA_FLAT_REG                   0x2E
#define BMA250_FLAT_HOLD_TIME_REG               0x2F
#define BMA250_STATUS_LOW_POWER_REG             0x31
#define BMA250_SELF_TEST_REG                    0x32
#define BMA250_EEPROM_CTRL_REG                  0x33
#define BMA250_SERIAL_CTRL_REG                  0x34
#define BMA250_CTRL_UNLOCK_REG                  0x35
#define BMA250_OFFSET_CTRL_REG                  0x36
#define BMA250_OFFSET_PARAMS_REG                0x37
#define BMA250_OFFSET_FILT_X_REG                0x38
#define BMA250_OFFSET_FILT_Y_REG                0x39
#define BMA250_OFFSET_FILT_Z_REG                0x3A
#define BMA250_OFFSET_UNFILT_X_REG              0x3B
#define BMA250_OFFSET_UNFILT_Y_REG              0x3C
#define BMA250_OFFSET_UNFILT_Z_REG              0x3D
#define BMA250_SPARE_0_REG                      0x3E
#define BMA250_SPARE_1_REG                      0x3F

/* mode settings */

#define BMA250_MODE_NORMAL      0
#define BMA250_MODE_LOWPOWER    1
#define BMA250_MODE_SUSPEND     2

struct bma_property {
	int range;
	int bandwidth;
	int mode;
};

static struct bma_property bma_static_property = {
	.range = BMA250_RANGE_SET,
	.bandwidth = BMA250_BW_SET,
	.mode = BMA250_MODE_SUSPEND
};

static int bma250_set_bandwidth(struct inv_gyro_state_s *st, unsigned char BW)
{
	int res = 0;
	unsigned char data;
	int Bandwidth = 0;
	if (BW >= 8)
		return -1;
	switch (BW) {
	case 0:
		Bandwidth = BMA250_BW_7_81HZ;
		break;
	case 1:
		Bandwidth = BMA250_BW_15_63HZ;
		break;
	case 2:
		Bandwidth = BMA250_BW_31_25HZ;
		break;
	case 3:
		Bandwidth = BMA250_BW_62_50HZ;
		break;
	case 4:
		Bandwidth = BMA250_BW_125HZ;
		break;
	case 5:
		Bandwidth = BMA250_BW_250HZ;
		break;
	case 6:
		Bandwidth = BMA250_BW_500HZ;
		break;
	case 7:
		Bandwidth = BMA250_BW_1000HZ;
		break;
	default:
		break;
	}
	res = inv_secondary_read(BMA250_BW_SEL_REG, 1, &data);
	if (res)
		return res;
	data &= 0xe0;
	data |= Bandwidth;
	res = inv_secondary_write(BMA250_BW_SEL_REG, data);
	return res;
}

static int bma250_set_range(struct inv_gyro_state_s *st, unsigned char Range)
{
	int res = 0;
	unsigned char orig, data = 0;
	if (Range >= 4)
		return -1;
	switch (Range) {
	case 0:
		data  = 3;
		break;
	case 1:
		data  = 5;
		break;
	case 2:
		data  = 8;
		break;
	case 3:
		data  = 12;
		break;
	default:
		break;
	}
	res = inv_secondary_read(BMA250_RANGE_SEL_REG, 1, &orig);
	if (res)
		return res;
	orig &= 0xf0;
	data |= orig;
	res = inv_secondary_write(BMA250_RANGE_SEL_REG, data);
	bma_static_property.range = Range;
	return res;
}

static int setup_slave_bma250(struct inv_gyro_state_s *st)
{
	int result;
	unsigned char data[4];
	result = set_3050_bypass(st, 1);
	if (result)
		return result;
	/*read secondary i2c ID register */
	result = inv_secondary_read(0, 2, data);
	if (result)
		return result;
	if (BMA250_CHIP_ID != data[0])
		return result;
	result = set_3050_bypass(st, 0);
	if (result)
		return result;
	/*AUX(accel), slave address is set inside set_3050_bypass*/
	/* bma250 x axis LSB register address is 2 */
	result = inv_i2c_single_write(st, 0x18, BMA250_X_AXIS_LSB_REG);
	return result;
}

static int bma250_set_mode(struct inv_gyro_state_s *st, unsigned char Mode)
{
	int res = 0;
	unsigned char data = 0;

	if (Mode >= 3)
		return -1;
	res = inv_secondary_read(BMA250_MODE_CTRL_REG, 1, &data);
	if (res)
		return res;
	data &= 0x3F;
	switch (Mode) {
	case BMA250_MODE_NORMAL:
		break;
	case BMA250_MODE_LOWPOWER:
		data |= 0x40;
		break;
	case BMA250_MODE_SUSPEND:
		data |= 0x80;
		break;
	default:
		break;
	}
	res = inv_secondary_write(BMA250_MODE_CTRL_REG, data);
	bma_static_property.mode = Mode;
	return res;
}
static int suspend_slave_bma250(struct inv_gyro_state_s *st)
{
	int result;
	if (bma_static_property.mode == BMA250_MODE_SUSPEND)
		return 0;
	/*set to bypass mode */
	result = set_3050_bypass(st, 1);
	if (result)
		return result;
	bma250_set_mode(st, BMA250_MODE_SUSPEND);
	/* no need to recover to non-bypass mode because we need it now */
	return result;
}
static int resume_slave_bma250(struct inv_gyro_state_s *st)
{
	int result;
	if (bma_static_property.mode == BMA250_MODE_NORMAL)
		return 0;
	/*set to bypass mode */
	result = set_3050_bypass(st, 1);
	if (result)
		return result;
	bma250_set_mode(st, BMA250_MODE_NORMAL);
	/* recover bypass mode */
	result = set_3050_bypass(st, 0);
	return result;
}
static int combine_data_slave_bma250(unsigned char *in, short *out)
{
	out[0] = (in[0] | (in[1]<<8));
	out[1] = (in[2] | (in[3]<<8));
	out[2] = (in[4] | (in[5]<<8));
	return 0;
}
static int get_mode_slave_bma250(struct inv_gyro_state_s *st)
{
	if (bma_static_property.mode == BMA250_MODE_SUSPEND)
		return 0;
	else if (bma_static_property.mode == BMA250_MODE_NORMAL)
		return 1;
	return -1;
};
/**
 *  set_lpf_bma250() - set lpf value
 */

static int set_lpf_bma250(struct inv_gyro_state_s *st, int rate)
{
	short hz[8] = {1000, 500, 250, 125, 62, 31, 15, 7};
	int   d[8] = {7, 6, 5, 4, 3, 2, 1, 0};
	int i, h, data, result;
	h = (rate >> 1);
	i = 0;
	while ((h < hz[i]) && (i < 8))
		i++;
	data = d[i];

	result = set_3050_bypass(st, 1);
	if (result)
		return result;
	result = bma250_set_bandwidth(st, (unsigned char) data);
	result |= set_3050_bypass(st, 0);

	return result;
}
/**
 *  set_fs_bma250() - set range value
 */

static int set_fs_bma250(struct inv_gyro_state_s *st, int fs)
{
	int result;
	result = set_3050_bypass(st, 1);
	if (result)
		return result;
	result = bma250_set_range(st, (unsigned char) fs);
	result |= set_3050_bypass(st, 0);
	if (result)
		return -EINVAL;
	return result;
}

static struct inv_mpu_slave slave_bma250 = {
	.suspend = suspend_slave_bma250,
	.resume  = resume_slave_bma250,
	.setup   = setup_slave_bma250,
	.combine_data = combine_data_slave_bma250,
	.get_mode = get_mode_slave_bma250,
	.set_lpf = set_lpf_bma250,
	.set_fs  = set_fs_bma250
};

int inv_register_bma250_slave(struct inv_gyro_state_s *st)
{
	st->mpu_slave = &slave_bma250;
	return 0;
}
/**
 *  @}
 */

