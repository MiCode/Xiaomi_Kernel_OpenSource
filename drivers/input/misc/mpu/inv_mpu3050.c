/*
* Copyright (C) 2012 Invensense, Inc.
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
*
*/

/**
 *  @addtogroup  DRIVERS
 *  @brief       Hardware drivers.
 *
 *  @{
 *      @file    inv_mpu3050.c
 *      @brief   A sysfs device driver for Invensense devices
 *      @details This file is part of inv_gyro driver code
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


void inv_setup_reg_mpu3050(struct inv_reg_map_s *reg)
{
	reg->who_am_i		= 0x00;
	reg->fifo_en		= 0x12;
	reg->sample_rate_div	= 0x15;
	reg->lpf		= 0x16;
	reg->fifo_count_h	= 0x3A;
	reg->fifo_r_w		= 0x3C;
	reg->user_ctrl		= 0x3D;
	reg->pwr_mgmt_1		= 0x3E;
	reg->raw_gyro		= 0x1D;
	reg->raw_accl		= 0x23;
	reg->temperature	= 0x1B;
	reg->int_enable		= 0x17;
	reg->int_status		= 0x1A;

	reg->accl_fifo_en	= BITS_3050_ACCL_OUT;
	reg->fifo_reset		= BIT_3050_FIFO_RST;
	reg->i2c_mst_reset	= BIT_3050_AUX_IF_RST;
	reg->cycle		= 0;
	reg->temp_dis		= 0;
}

/**
 *  inv_init_config_mpu3050() - Initialize hardware, disable FIFO.
 *  @st:	Device driver instance.
 */
int inv_init_config_mpu3050(struct inv_gyro_state_s *st)
{
	u8 data;
	int result;

	st->chip_config.fifo_thr = FIFO_THRESHOLD;

	/*reading AUX VDDIO register */
	result = inv_i2c_read(st, REG_3050_AUX_VDDIO, 1, &data);
	if (result)
		return result;

	data &= ~BIT_3050_VDDIO;
	data |= (st->plat_data.level_shifter << 2);
	result = inv_i2c_single_write(st, REG_3050_AUX_VDDIO, data);
	if (result)
		return result;

	if (SECONDARY_SLAVE_TYPE_ACCEL == st->plat_data.sec_slave_type) {
		if (st->plat_data.sec_slave_id == ACCEL_ID_KXTF9)
			inv_register_kxtf9_slave(st);
		if (st->mpu_slave != NULL) {
			result = st->mpu_slave->setup(st);
			if (result)
				return result;
		}
	}
	return 0;
}

int set_3050_bypass(struct inv_gyro_state_s *inf, int enable)
{
	struct inv_reg_map_s *reg;
	u8 user_ctrl;
	int err;
	int err_t = 0;

	reg = inf->reg;
	if (((inf->hw.user_ctrl & BIT_3050_AUX_IF_EN) == 0) && enable)
		return 0;

	if ((inf->hw.user_ctrl & BIT_3050_AUX_IF_EN) && (enable == 0))
		return 0;

	user_ctrl = inf->hw.user_ctrl;
	user_ctrl &= ~BIT_3050_AUX_IF_EN;
	if (!enable) {
		user_ctrl |= BIT_3050_AUX_IF_EN;
		err_t = inv_i2c_single_write(inf, reg->user_ctrl, user_ctrl);
		if (!err_t) {
			inf->hw.user_ctrl = user_ctrl;
			inf->aux.en3050 = true;
		}
	} else {
		inf->aux.en3050 = false;
		/* Coming out of I2C is tricky due to several erratta.
		* Do not modify this algorithm
		* 1) wait for the right time and send the command to change
		* the aux i2c slave address to an invalid address that will
		* get nack'ed
		*
		* 0x00 is broadcast.  0x7F is unlikely to be used by any aux.
		*/
		err_t = inv_i2c_single_write(inf, REG_3050_SLAVE_ADDR, 0x7F);
		/*
		* 2) wait enough time for a nack to occur, then go into
		*    bypass mode:
		*/
		mdelay(2);
		err = inv_i2c_single_write(inf, reg->user_ctrl, user_ctrl);
		if (!err)
			inf->hw.user_ctrl = user_ctrl;
		else
			err_t |= err;
		/*
		* 3) wait for up to one MPU cycle then restore the slave
		*    address
		*/
		mdelay(20);
		err_t |= inv_i2c_single_write(inf, REG_3050_SLAVE_REG,
					    inf->plat_data.secondary_read_reg);
		err_t |= inv_i2c_single_write(inf, REG_3050_SLAVE_ADDR,
					    inf->plat_data.secondary_i2c_addr);
		err = inv_i2c_single_write(inf, reg->user_ctrl,
					   (user_ctrl | BIT_3050_AUX_IF_RST));
		if (!err)
			inf->hw.user_ctrl = user_ctrl;
		else
			err_t |= err;
		mdelay(2);
	}
	return err_t;
}
/**
 *  @}
 */

