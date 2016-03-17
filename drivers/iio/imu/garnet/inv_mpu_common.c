/*
* Copyright (C) 2012 Invensense, Inc.
 * Copyright (C) 2016 XiaoMi, Inc.
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
#include <linux/types.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <linux/iio/imu/mpu.h>

#include "inv_mpu_iio.h"

static int inv_mpu_power_on(const struct inv_mpu_state *st)
{
	const struct mpu_platform_data *pdata = &st->plat_data;
	int err;

	if (!IS_ERR(pdata->vdd_ana)) {
		err = regulator_enable(pdata->vdd_ana);
		if (err) {
			dev_err(st->dev, "error enabling vdd_ana power: %d\n", err);
			return err;
		}
	}

	if (!IS_ERR(pdata->vdd_i2c)) {
		err = regulator_enable(pdata->vdd_i2c);
		if (err) {
			dev_err(st->dev, "error enabling vdd_i2c power: %d\n", err);
			return err;
		}
	}

	msleep(2000);

	return 0;
}

static int inv_mpu_power_off(const struct inv_mpu_state *st)
{
	const struct mpu_platform_data *pdata = &st->plat_data;
	int err1 = 0;
	int err2 = 0;

	if (!IS_ERR(pdata->vdd_ana)) {
		err1 = regulator_disable(pdata->vdd_ana);
		if (err1)
			dev_err(st->dev, "error disabling vdd_ana power: %d\n", err1);
	}

	if (!IS_ERR(pdata->vdd_i2c)) {
		err2 = regulator_disable(pdata->vdd_i2c);
		if (err2)
			dev_err(st->dev, "error disabling vdd_i2c power: %d\n", err2);
	}

	if (err1 || err2)
		return -EPERM;
	else
		return 0;
}

void inv_init_power(struct inv_mpu_state *st)
{
	const struct mpu_platform_data *pdata = &st->plat_data;

	if (!IS_ERR(pdata->vdd_ana) || !IS_ERR(pdata->vdd_i2c)) {
		st->power_on = inv_mpu_power_on;
		st->power_off = inv_mpu_power_off;
	}
}

int inv_proto_set_power(struct inv_mpu_state *st, bool power_on)
{
	u8 frame[] = { 0x00, 0x02, 0x00, 0x00 };

	if (power_on)
		frame[2] = 0x01;

	return inv_send_command_down(st, frame, sizeof(frame));
}

int inv_set_bank(struct inv_mpu_state *st, u8 bank)
{
	int ret;

	if (st->bank == bank)
		return 0;

	ret = inv_plat_single_write(st, REG_BANK_SEL, bank);
	if (ret)
		return ret;

	st->bank = bank;
	return 0;
}

int inv_set_fifo_index(struct inv_mpu_state *st, u8 fifo_index)
{
	int ret;

	if (st->fifo_index == fifo_index)
		return 0;

	ret = inv_set_bank(st, 0);
	if (ret)
		return ret;
	ret = inv_plat_single_write(st, REG_FIFO_INDEX, fifo_index);
	if (ret)
		return ret;

	st->fifo_index = fifo_index;
	return 0;
}

int inv_soft_reset(struct inv_mpu_state *st)
{
	int result;

	result = inv_set_bank(st, 0);
	if (result)
		return result;
	result = inv_plat_single_write(st, REG_PWR_MGMT_1, BIT_SOFT_RESET);
	if (result)
		return result;
	msleep(100);
	result = inv_plat_single_write(st, REG_PWR_MGMT_1, 0);
	if (result)
		return result;

	return 0;
}

int inv_fifo_config(struct inv_mpu_state *st)
{
	int result;
	u8 reg_mod;

	result = inv_set_bank(st, 0);
	if (result)
		return result;

	/* enable FIFOs and assert reset */
	result = inv_plat_single_read(st, REG_MOD_EN, &reg_mod);
	if (result)
		return result;
	reg_mod |= BIT_SERIF_FIFO_EN;
	result = inv_plat_single_write(st, REG_MOD_EN, reg_mod);
	if (result)
		return result;
	result = inv_plat_single_write(st, REG_FIFO_RST, INV_FIFO_IDS);
	if (result)
		return result;

	/* override FIFOs packet size */
	result = inv_plat_single_write(st, REG_PKT_SIZE_OVERRIDE, INV_FIFO_IDS);
	if (result)
		return result;
	result = inv_plat_single_write(st, INV_FIFO_CMD_REG_PACKET,
				       INV_FIFO_CMD_PACKET);
	if (result)
		return result;
	result = inv_plat_single_write(st, INV_FIFO_DATA_NORMAL_REG_PACKET,
				       INV_FIFO_DATA_PACKET);
	if (result)
		return result;
	result = inv_plat_single_write(st, INV_FIFO_DATA_WAKEUP_REG_PACKET,
				       INV_FIFO_DATA_PACKET);
	if (result)
		return result;

	/* set FIFOs size */
	result = inv_plat_single_write(st, INV_FIFO_CMD_REG_SIZE,
				       INV_FIFO_CMD_SIZE_VAL);
	if (result)
		return result;
	result = inv_plat_single_write(st, INV_FIFO_DATA_NORMAL_REG_SIZE,
				       INV_FIFO_DATA_SIZE_VAL);
	if (result)
		return result;
	result = inv_plat_single_write(st, INV_FIFO_DATA_WAKEUP_REG_SIZE,
				       INV_FIFO_DATA_SIZE_VAL);
	if (result)
		return result;

	/* configure FIFOs in data streaming rollover mode */
	result = inv_plat_single_write(st, REG_FIFO_MODE, INV_FIFO_IDS);
	if (result)
		return result;

	/* disable the empty indicator */
	result = inv_plat_single_write(st, REG_MOD_CTRL2,
					BIT_FIFO_EMPTY_IND_DIS);
	if (result)
		return result;

	/* deassert FIFOs reset */
	result = inv_plat_single_write(st, REG_FIFO_RST, 0x0);
	if (result)
		return result;

	/* enable MCU */
	reg_mod |= BIT_MCU_EN;
	result = inv_plat_single_write(st, REG_MOD_EN, reg_mod);
	if (result)
		return result;

	st->fifo_length = INV_FIFO_DATA_SIZE;

	return result;
}

void inv_wake_start(const struct inv_mpu_state *st)
{
	const struct mpu_platform_data *pdata = &st->plat_data;

	if (gpio_is_valid(pdata->wake_gpio)) {
		gpio_set_value(pdata->wake_gpio, 1);
		usleep_range(pdata->wake_delay_min,
				pdata->wake_delay_max);
	}
}

void inv_wake_stop(const struct inv_mpu_state *st)
{
	const struct mpu_platform_data *pdata = &st->plat_data;

	if (gpio_is_valid(pdata->wake_gpio))
		gpio_set_value(pdata->wake_gpio, 0);
}
