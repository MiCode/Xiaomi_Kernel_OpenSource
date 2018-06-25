/*
 * Copyright (c) 2018 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/jiffies.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kfifo.h>
#include <linux/poll.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include "inv_icm20602_bsp.h"
#include "inv_icm20602_iio.h"

#define icm20602_init_reg_addr(head, tail, reg_map) { \
			enum inv_icm20602_reg_addr i;\
			for (i = head; i <= tail; i++) {\
				reg_map->address = i;\
				reg_map->reg_u.reg = 0x0;\
				reg_map++;\
			} \
}

#define icm20602_write_reg_simple(st, register) \
		icm20602_write_reg(st, \
		register.address, \
		register.reg_u.reg)

static struct inv_icm20602_reg_map reg_set_20602;

int icm20602_init_reg_map(void)
{
	struct struct_XG_OFFS_TC_H *reg_map = &(reg_set_20602.XG_OFFS_TC_H);

	icm20602_init_reg_addr(ADDR_XG_OFFS_TC_H,
			ADDR_XG_OFFS_TC_L, reg_map);

	icm20602_init_reg_addr(ADDR_YG_OFFS_TC_H,
			ADDR_YG_OFFS_TC_L, reg_map);

	icm20602_init_reg_addr(ADDR_ZG_OFFS_TC_H,
			ADDR_ZG_OFFS_TC_L, reg_map);

	icm20602_init_reg_addr(ADDR_SELF_TEST_X_ACCEL,
			ADDR_SELF_TEST_Z_ACCEL, reg_map);

	icm20602_init_reg_addr(ADDR_XG_OFFS_USRH,
			ADDR_LP_MODE_CFG, reg_map);

	icm20602_init_reg_addr(ADDR_ACCEL_WOM_X_THR,
			ADDR_FIFO_EN, reg_map);

	icm20602_init_reg_addr(ADDR_FSYNC_INT,
			ADDR_GYRO_ZOUT_L, reg_map);

	icm20602_init_reg_addr(ADDR_SELF_TEST_X_GYRO,
			ADDR_SELF_TEST_Z_GYRO, reg_map);

	icm20602_init_reg_addr(ADDR_FIFO_WM_TH1,
			ADDR_FIFO_WM_TH2, reg_map);

	icm20602_init_reg_addr(ADDR_SIGNAL_PATH_RESET,
			ADDR_PWR_MGMT_2, reg_map);

	icm20602_init_reg_addr(ADDR_I2C_IF,
			ADDR_I2C_IF, reg_map);

	icm20602_init_reg_addr(ADDR_FIFO_COUNTH,
			ADDR_XA_OFFSET_L, reg_map);

	icm20602_init_reg_addr(ADDR_YA_OFFSET_H,
			ADDR_YA_OFFSET_L, reg_map);

	icm20602_init_reg_addr(ADDR_ZA_OFFSET_H,
			ADDR_ZA_OFFSET_L, reg_map);

	return MPU_SUCCESS;
}

#define W_FLG	0
#define R_FLG	1
int icm20602_bulk_read(struct inv_icm20602_state *st,
			int reg, u8 *buf, int size)
{
	int result = MPU_SUCCESS;
	char tx_buf[2] = {0x0, 0x0};
	u8 *tmp_buf = buf;
	struct i2c_msg msg[2];

	if (!st || !buf)
		return -MPU_FAIL;

	if (st->interface == ICM20602_SPI) {
		tx_buf[0] = ICM20602_READ_REG(reg);
		result = spi_write_then_read(st->spi, &tx_buf[0],
		1, tmp_buf, size);
		if (result) {
			pr_err("mpu read reg %u failed, rc %d\n",
			reg, result);
			result = -MPU_READ_FAIL;
		}
	} else {
		result = size;
#ifdef ICM20602_I2C_SMBUS
		result += i2c_smbus_read_i2c_block_data(st->client,
		reg, size, tmp_buf);
#else
		tx_buf[0] = reg;
		msg[0].addr = st->client->addr;
		msg[0].flags = W_FLG;
		msg[0].len = 1;
		msg[0].buf = tx_buf;

		msg[1].addr = st->client->addr;
		msg[1].flags = I2C_M_RD;
		msg[1].len = size;
		msg[1].buf = tmp_buf;
		i2c_transfer(st->client->adapter, msg, ARRAY_SIZE(msg));
#endif
	}
	return result;
}

static int icm20602_write_reg(struct inv_icm20602_state *st,
			 uint8_t reg, uint8_t val)
{
	int result = MPU_SUCCESS;
	char txbuf[2] = {0x0, 0x0};
	struct i2c_msg msg[1];

	if (st->interface == ICM20602_SPI) {
		txbuf[0] = ICM20602_WRITE_REG(reg);
		txbuf[1] = val;
		result = spi_write_then_read(st->spi, &txbuf[0], 2, NULL, 0);
		if (result) {
			pr_err("mpu write reg %u failed, rc %d\n",
						reg, val);
			result = -MPU_READ_FAIL;
		}
	} else if (st->interface == ICM20602_I2C) {
#ifdef ICM20602_I2C_SMBUS
		result = i2c_smbus_write_i2c_block_data(st->client,
						reg, 1, &val);
#else
		txbuf[0] = reg;
		txbuf[1] = val;
		msg[0].addr = st->client->addr;
		msg[0].flags = I2C_M_IGNORE_NAK;
		msg[0].len = 2;
		msg[0].buf = txbuf;

		i2c_transfer(st->client->adapter, msg, ARRAY_SIZE(msg));
#endif
	}

	return result;
}

static int icm20602_read_reg(struct inv_icm20602_state *st,
				uint8_t reg, uint8_t *val)
{
	int result = MPU_SUCCESS;
	char txbuf[1] = {0x0};
	char rxbuf[1] = {0x0};
	struct i2c_msg msg[2];

	if (st->interface == ICM20602_SPI) {
		txbuf[0] = ICM20602_READ_REG(reg);
		result = spi_write_then_read(st->spi,
						&txbuf[0], 1, rxbuf, 1);
		if (result) {
			pr_err("mpu read reg %u failed, rc %d\n",
						reg, result);
			result = -MPU_READ_FAIL;
		}
	} else if (st->interface == ICM20602_I2C) {
#ifdef ICM20602_I2C_SMBUS
		result = i2c_smbus_read_i2c_block_data(st->client,
						reg, 1, rxbuf);
		if (result != 1) {
			pr_err("mpu read reg %u failed, rc %d\n",
						reg, result);
			result = -MPU_READ_FAIL;
		}
#else
		txbuf[0] = reg;
		msg[0].addr = st->client->addr;
		msg[0].flags = W_FLG;
		msg[0].len = 1;
		msg[0].buf = txbuf;

		msg[1].addr = st->client->addr;
		msg[1].flags = I2C_M_RD;
		msg[1].len = 1;
		msg[1].buf = rxbuf;

		i2c_transfer(st->client->adapter, msg, ARRAY_SIZE(msg));
#endif
	}
	*val = rxbuf[0];

	return result;
}

#define combine_8_to_16(upper, lower) ((upper << 8) | lower)

int icm20602_read_raw(struct inv_icm20602_state *st,
		struct struct_icm20602_real_data *real_data, uint32_t type)
{
	struct struct_icm20602_raw_data raw_data;

	if ((type & ACCEL) != 0) {
		icm20602_read_reg(st,
			reg_set_20602.ACCEL_XOUT_H.address,
			&raw_data.ACCEL_XOUT_H);
		icm20602_read_reg(st,
			reg_set_20602.ACCEL_XOUT_L.address,
			&raw_data.ACCEL_XOUT_L);
		real_data->ACCEL_XOUT =
			combine_8_to_16(raw_data.ACCEL_XOUT_H,
			raw_data.ACCEL_XOUT_L);

		icm20602_read_reg(st,
			reg_set_20602.ACCEL_YOUT_H.address,
			&raw_data.ACCEL_YOUT_H);
		icm20602_read_reg(st,
			reg_set_20602.ACCEL_YOUT_L.address,
			&raw_data.ACCEL_YOUT_L);
		real_data->ACCEL_YOUT =
			combine_8_to_16(raw_data.ACCEL_YOUT_H,
			raw_data.ACCEL_YOUT_L);

		icm20602_read_reg(st,
			reg_set_20602.ACCEL_ZOUT_H.address,
			&raw_data.ACCEL_ZOUT_H);
		icm20602_read_reg(st,
			reg_set_20602.ACCEL_ZOUT_L.address,
			&raw_data.ACCEL_ZOUT_L);
		real_data->ACCEL_ZOUT =
			combine_8_to_16(raw_data.ACCEL_ZOUT_H,
			raw_data.ACCEL_ZOUT_L);
	}

	if ((type & GYRO) != 0) {
		icm20602_read_reg(st,
			reg_set_20602.GYRO_XOUT_H.address,
			&raw_data.GYRO_XOUT_H);
		icm20602_read_reg(st,
			reg_set_20602.GYRO_XOUT_L.address,
			&raw_data.GYRO_XOUT_L);
		real_data->GYRO_XOUT =
			combine_8_to_16(raw_data.GYRO_XOUT_H,
			raw_data.GYRO_XOUT_L);

		icm20602_read_reg(st,
			reg_set_20602.GYRO_YOUT_H.address,
			&raw_data.GYRO_YOUT_H);
		icm20602_read_reg(st,
			reg_set_20602.GYRO_YOUT_L.address,
			&raw_data.GYRO_YOUT_L);
		real_data->GYRO_YOUT =
			combine_8_to_16(raw_data.GYRO_YOUT_H,
			raw_data.GYRO_YOUT_L);

		icm20602_read_reg(st,
			reg_set_20602.GYRO_ZOUT_H.address,
			&raw_data.GYRO_ZOUT_H);
		icm20602_read_reg(st,
			reg_set_20602.GYRO_ZOUT_L.address,
			&raw_data.GYRO_ZOUT_L);
		real_data->GYRO_ZOUT =
			combine_8_to_16(raw_data.GYRO_ZOUT_H,
			raw_data.GYRO_ZOUT_L);
	}

	return MPU_SUCCESS;
}

int icm20602_read_fifo(struct inv_icm20602_state *st,
					void *buf, const int size)
{
	return icm20602_bulk_read(st,
		reg_set_20602.FIFO_R_W.address, buf, size);
}

int icm20602_start_fifo(struct inv_icm20602_state *st)
{
	struct icm20602_user_config *config = NULL;

	config = st->config;

	/* enable fifo */
	if (config->fifo_enabled) {
		reg_set_20602.USER_CTRL.reg_u.REG.FIFO_EN = 0x1;
		if (icm20602_write_reg_simple(st,
						reg_set_20602.USER_CTRL)) {
			pr_err("icm20602 start fifo failed\n");
			return -MPU_FAIL;
		}

		/* enable interrupt, need to test */
		reg_set_20602.INT_ENABLE.reg_u.REG.FIFO_OFLOW_EN = 0x1;
		reg_set_20602.INT_ENABLE.reg_u.REG.DATA_RDY_INT_EN = 0x0;
		if (icm20602_write_reg_simple(st,
						reg_set_20602.INT_ENABLE)) {
			pr_err("icm20602 set FIFO_OFLOW_EN failed\n");
			return -MPU_FAIL;
		}
	}

	return MPU_SUCCESS;
}

int icm20602_stop_fifo(struct inv_icm20602_state *st)
{
	struct icm20602_user_config *config = NULL;

	config = st->config;
	/* disable fifo */
	if (config->fifo_enabled) {
		reg_set_20602.USER_CTRL.reg_u.REG.FIFO_EN = 0x0;
		reg_set_20602.USER_CTRL.reg_u.REG.FIFO_RST = 0x1;
		if (icm20602_write_reg_simple(st,
				reg_set_20602.USER_CTRL)) {
			reg_set_20602.USER_CTRL.reg_u.REG.FIFO_RST = 0x0;
			pr_err("icm20602 stop fifo failed\n");
			return -MPU_FAIL;
		}
		reg_set_20602.USER_CTRL.reg_u.REG.FIFO_RST = 0x0;
	}

	return MPU_SUCCESS;
}

int icm20602_int_status(struct inv_icm20602_state *st,
	u8 *int_status)
{
	return icm20602_read_reg(st,
		reg_set_20602.INT_STATUS.address, int_status);
}

int icm20602_int_wm_status(struct inv_icm20602_state *st,
	u8 *int_status)
{
	return icm20602_read_reg(st,
		reg_set_20602.FIFO_WM_INT_STATUS.address, int_status);
}

int icm20602_fifo_count(struct inv_icm20602_state *st,
	u16 *fifo_count)
{
	u8 count_h, count_l;

	*fifo_count = 0;
	icm20602_read_reg(st, reg_set_20602.FIFO_COUNTH.address, &count_h);
	icm20602_read_reg(st, reg_set_20602.FIFO_COUNTL.address, &count_l);
	*fifo_count |= (count_h << 8);
	*fifo_count |= count_l;
	return MPU_SUCCESS;
}

static int icm20602_config_waterlevel(struct inv_icm20602_state *st)
{
	struct icm20602_user_config *config = NULL;
	uint8_t val = 0;

	config = st->config;
	if (config->fifo_enabled != true)
		return MPU_SUCCESS;
	/* config waterlevel as the fps need */
	config->fifo_waterlevel = (config->user_fps_in_ms /
				(1000 / config->gyro_accel_sample_rate))
				*ICM20602_PACKAGE_SIZE;

	if (config->fifo_waterlevel > 1023 ||
		config->fifo_waterlevel/50 >
		(1023-config->fifo_waterlevel)/ICM20602_PACKAGE_SIZE) {
		pr_err("set fifo_waterlevel failed %d\n",
					config->fifo_waterlevel);
		return MPU_FAIL;
	}
	reg_set_20602.FIFO_WM_TH1.reg_u.reg =
				(config->fifo_waterlevel & 0xff00) >> 8;
	reg_set_20602.FIFO_WM_TH2.reg_u.reg =
				(config->fifo_waterlevel & 0x00ff);

	icm20602_write_reg_simple(st, reg_set_20602.FIFO_WM_TH1);
	icm20602_write_reg_simple(st, reg_set_20602.FIFO_WM_TH2);
	icm20602_read_reg(st, reg_set_20602.FIFO_WM_TH1.address, &val);
	icm20602_read_reg(st, reg_set_20602.FIFO_WM_TH2.address, &val);

	return MPU_SUCCESS;
}

static int icm20602_read_ST_code(struct inv_icm20602_state *st)
{
	struct icm20602_user_config *config = NULL;
	int result = 0;

	config = st->config;
	result |= icm20602_read_reg(st, reg_set_20602.SELF_TEST_X_ACCEL.address,
		&(config->acc_self_test.X));
	result |= icm20602_read_reg(st, reg_set_20602.SELF_TEST_Y_ACCEL.address,
		&(config->acc_self_test.Y));
	result |= icm20602_read_reg(st, reg_set_20602.SELF_TEST_Z_ACCEL.address,
		&(config->acc_self_test.Z));

	result |= icm20602_read_reg(st, reg_set_20602.SELF_TEST_X_GYRO.address,
		&(config->gyro_self_test.X));
	result |= icm20602_read_reg(st, reg_set_20602.SELF_TEST_Y_GYRO.address,
		&(config->gyro_self_test.Y));
	result |= icm20602_read_reg(st, reg_set_20602.SELF_TEST_Z_GYRO.address,
		&(config->gyro_self_test.Z));

	return result;
}

static int icm20602_set_self_test(struct inv_icm20602_state *st)
{
	int result = 0;

	reg_set_20602.SMPLRT_DIV.reg_u.REG.SMPLRT_DIV = 0;
	result |= icm20602_write_reg_simple(st, reg_set_20602.SMPLRT_DIV);

	reg_set_20602.CONFIG.reg_u.REG.DLFP_CFG = INV_ICM20602_GYRO_LFP_92HZ;
	result |= icm20602_write_reg_simple(st, reg_set_20602.CONFIG);

	reg_set_20602.GYRO_CONFIG.reg_u.REG.FCHOICE_B = 0x0;
	reg_set_20602.GYRO_CONFIG.reg_u.REG.FS_SEL = ICM20602_GYRO_FSR_250DPS;
	result |= icm20602_write_reg_simple(st, reg_set_20602.GYRO_CONFIG);

	reg_set_20602.ACCEL_CONFIG2.reg_u.REG.A_DLPF_CFG = ICM20602_ACCLFP_99;
	reg_set_20602.ACCEL_CONFIG2.reg_u.REG.ACCEL_FCHOICE_B = 0X0;
	result |= icm20602_write_reg_simple(st, reg_set_20602.ACCEL_CONFIG2);

	reg_set_20602.ACCEL_CONFIG.reg_u.REG.ACCEL_FS_SEL = ICM20602_ACC_FSR_2G;
	result |= icm20602_write_reg_simple(st, reg_set_20602.ACCEL_CONFIG);

	icm20602_read_ST_code(st);

	return 0;
}

static int icm20602_do_test_acc(struct inv_icm20602_state *st,
	struct X_Y_Z *acc, struct X_Y_Z *acc_st)
{
	struct struct_icm20602_real_data *real_data =
		kmalloc(sizeof(struct inv_icm20602_state), GFP_ATOMIC);
	int i;

	for (i = 0; i < SELFTEST_COUNT; i++) {
		icm20602_read_raw(st, real_data, ACCEL);
		acc->X += real_data->ACCEL_XOUT;
		acc->Y += real_data->ACCEL_YOUT;
		acc->Z += real_data->ACCEL_ZOUT;
		usleep_range(1000, 1001);
	}
	acc->X /= SELFTEST_COUNT;
	acc->X *= ST_PRECISION;

	acc->Y /= SELFTEST_COUNT;
	acc->Y *= ST_PRECISION;

	acc->Z /= SELFTEST_COUNT;
	acc->Z *= ST_PRECISION;

	reg_set_20602.ACCEL_CONFIG.reg_u.REG.XG_ST = 0x1;
	reg_set_20602.ACCEL_CONFIG.reg_u.REG.YG_ST = 0x1;
	reg_set_20602.ACCEL_CONFIG.reg_u.REG.ZG_ST = 0x1;
	icm20602_write_reg_simple(st, reg_set_20602.ACCEL_CONFIG);

	for (i = 0; i < SELFTEST_COUNT; i++) {
		icm20602_read_raw(st, real_data, ACCEL);
		acc_st->X += real_data->ACCEL_XOUT;
		acc_st->Y += real_data->ACCEL_YOUT;
		acc_st->Z += real_data->ACCEL_ZOUT;
		usleep_range(1000, 1001);
	}
	acc_st->X /= SELFTEST_COUNT;
	acc_st->X *= ST_PRECISION;

	acc_st->Y /= SELFTEST_COUNT;
	acc_st->Y *= ST_PRECISION;

	acc_st->Z /= SELFTEST_COUNT;
	acc_st->Z *= ST_PRECISION;

	return MPU_SUCCESS;
}

static int icm20602_do_test_gyro(struct inv_icm20602_state *st,
	struct X_Y_Z *gyro, struct X_Y_Z *gyro_st)
{
	struct struct_icm20602_real_data *real_data =
		kmalloc(sizeof(struct inv_icm20602_state), GFP_ATOMIC);
	int i;

	for (i = 0; i < SELFTEST_COUNT; i++) {
		icm20602_read_raw(st, real_data, GYRO);
		gyro->X += real_data->GYRO_XOUT;
		gyro->Y += real_data->GYRO_YOUT;
		gyro->Z += real_data->GYRO_ZOUT;
		usleep_range(1000, 1001);
	}
	gyro->X /= SELFTEST_COUNT;
	gyro->X *= ST_PRECISION;

	gyro->Y /= SELFTEST_COUNT;
	gyro->Y *= ST_PRECISION;

	gyro->Z /= SELFTEST_COUNT;
	gyro->Z *= ST_PRECISION;

	reg_set_20602.GYRO_CONFIG.reg_u.REG.XG_ST = 0x1;
	reg_set_20602.GYRO_CONFIG.reg_u.REG.YG_ST = 0x1;
	reg_set_20602.GYRO_CONFIG.reg_u.REG.ZG_ST = 0x1;
	icm20602_write_reg_simple(st, reg_set_20602.GYRO_CONFIG);

	for (i = 0; i < SELFTEST_COUNT; i++) {
		icm20602_read_raw(st, real_data, ACCEL);
		gyro_st->X += real_data->GYRO_XOUT;
		gyro_st->Y += real_data->GYRO_YOUT;
		gyro_st->Z += real_data->GYRO_ZOUT;
		usleep_range(1000, 1001);
	}
	gyro_st->X /= SELFTEST_COUNT;
	gyro_st->X *= ST_PRECISION;

	gyro_st->Y /= SELFTEST_COUNT;
	gyro_st->Y *= ST_PRECISION;

	gyro_st->Z /= SELFTEST_COUNT;
	gyro_st->Z *= ST_PRECISION;

	return MPU_SUCCESS;
}

static bool icm20602_check_acc_selftest(struct inv_icm20602_state *st,
	struct X_Y_Z *acc, struct X_Y_Z *acc_st)
{
	struct X_Y_Z acc_ST_code, st_otp, st_shift_cust;
	bool otp_value_zero = false, test_result = true;

	acc_ST_code.X = st->config->acc_self_test.X;
	acc_ST_code.Y = st->config->acc_self_test.Y;
	acc_ST_code.Z = st->config->acc_self_test.Z;

	st_otp.X = (st_otp.X != 0) ? mpu_st_tb[acc_ST_code.X - 1] : 0;
	st_otp.Y = (st_otp.Y != 0) ? mpu_st_tb[acc_ST_code.Y - 1] : 0;
	st_otp.Z = (st_otp.Z != 0) ? mpu_st_tb[acc_ST_code.Z - 1] : 0;

	if ((st_otp.X & st_otp.Y & st_otp.Z) == 0)
		otp_value_zero = true;

	st_shift_cust.X = acc_st->X - acc->X;
	st_shift_cust.Y = acc_st->X - acc->Y;
	st_shift_cust.Z = acc_st->X - acc->Z;
	if (!otp_value_zero) {
		if (
		st_shift_cust.X <
		(st_otp.X * ST_PRECISION * ACC_ST_SHIFT_MIN / 100) ||
		st_shift_cust.Y <
		(st_otp.Y * ST_PRECISION * ACC_ST_SHIFT_MIN / 100) ||
		st_shift_cust.Z <
		(st_otp.Z * ST_PRECISION * ACC_ST_SHIFT_MIN / 100) ||

		st_shift_cust.X >
		(st_otp.X * ST_PRECISION * ACC_ST_SHIFT_MAX / 100) ||
		st_shift_cust.Y >
		(st_otp.Y * ST_PRECISION * ACC_ST_SHIFT_MAX / 100) ||
		st_shift_cust.Z >
		(st_otp.Z * ST_PRECISION * ACC_ST_SHIFT_MAX / 100)
		) {
			test_result = false;
		}
	} else {
		if (
		abs(st_shift_cust.X) <
		(ACC_ST_AL_MIN * 16384 / 1000 * ST_PRECISION) ||
		abs(st_shift_cust.Y) <
		(ACC_ST_AL_MIN * 16384 / 1000 * ST_PRECISION) ||
		abs(st_shift_cust.Z) <
		(ACC_ST_AL_MIN * 16384 / 1000 * ST_PRECISION)  ||

		abs(st_shift_cust.X) >
		(ACC_ST_AL_MAX * 16384 / 1000 * ST_PRECISION) ||
		abs(st_shift_cust.Y) >
		(ACC_ST_AL_MAX * 16384 / 1000 * ST_PRECISION) ||
		abs(st_shift_cust.Z) >
		(ACC_ST_AL_MAX * 16384 / 1000 * ST_PRECISION)
		) {
			test_result = false;
		}
	}

	return test_result;
}

static int icm20602_check_gyro_selftest(struct inv_icm20602_state *st,
	struct X_Y_Z *gyro, struct X_Y_Z *gyro_st)
{
	struct X_Y_Z gyro_ST_code, st_otp, st_shift_cust;
	bool otp_value_zero = false, test_result = true;

	gyro_ST_code.X = st->config->gyro_self_test.X;
	gyro_ST_code.Y = st->config->gyro_self_test.Y;
	gyro_ST_code.Z = st->config->gyro_self_test.Z;

	st_otp.X = (gyro_ST_code.X != 0) ? mpu_st_tb[gyro_ST_code.X - 1] : 0;
	st_otp.Y = (gyro_ST_code.Y != 0) ? mpu_st_tb[gyro_ST_code.Y - 1] : 0;
	st_otp.Z = (gyro_ST_code.Z != 0) ? mpu_st_tb[gyro_ST_code.Z - 1] : 0;

	if ((st_otp.X & st_otp.Y & st_otp.Z) == 0)
		otp_value_zero = true;

	st_shift_cust.X = gyro_st->X - gyro->X;
	st_shift_cust.Y = gyro_st->X - gyro->Y;
	st_shift_cust.Z = gyro_st->X - gyro->Z;
	if (!otp_value_zero) {
		if (
		st_shift_cust.X <
		(st_otp.X * ST_PRECISION * GYRO_ST_SHIFT / 100) ||
		st_shift_cust.Y <
		(st_otp.Y * ST_PRECISION * GYRO_ST_SHIFT / 100) ||
		st_shift_cust.Z <
		(st_otp.Z * ST_PRECISION * GYRO_ST_SHIFT / 100)
		) {
			test_result = false;
		}
	} else {
		if (
		abs(st_shift_cust.X) <
		(GYRO_ST_AL * 32768 / 250 * ST_PRECISION) ||
		abs(st_shift_cust.Y) <
		(GYRO_ST_AL * 32768 / 250 * ST_PRECISION) ||
		abs(st_shift_cust.Z) <
		(GYRO_ST_AL * 32768 / 250 * ST_PRECISION)
		) {
			test_result = false;
		}
	}

	if (test_result == true) {
		/* Self Test Pass/Fail Criteria C */
		if (
		abs(st_shift_cust.X) >
		GYRO_OFFSET_MAX * 32768 / 250 * ST_PRECISION ||
		abs(st_shift_cust.Y) >
		GYRO_OFFSET_MAX * 32768 / 250 * ST_PRECISION ||
		abs(st_shift_cust.Z) >
		GYRO_OFFSET_MAX * 32768 / 250 * ST_PRECISION
		) {
			test_result = false;
		}
	}

	return test_result;
}

bool icm20602_self_test(struct inv_icm20602_state *st)
{
	struct X_Y_Z acc, acc_st;
	struct X_Y_Z gyro, gyro_st;
	bool test_result = true;

	icm20602_set_self_test(st);
	icm20602_do_test_acc(st, &acc, &acc_st);
	icm20602_do_test_gyro(st, &gyro, &gyro_st);
	test_result = icm20602_check_acc_selftest(st, &acc, &acc_st);
	test_result = icm20602_check_gyro_selftest(st, &gyro, &gyro_st);

	return test_result;
}

static int icm20602_config_fifo(struct inv_icm20602_state *st)
{
	struct icm20602_user_config *config = NULL;

	config = st->config;
	if (config->fifo_enabled != true)
		return MPU_SUCCESS;

	/*
	 * Set CONFIG.USER_SET_BIT = 0, No reason as datasheet said
	 */
	reg_set_20602.CONFIG.reg_u.REG.USER_SET_BIT = 0x0;
	/*
	 * Set CONFIG.FIFO_MODE = 1,
	 * i.e. when FIFO is full, additional writes will
	 * not be written to FIFO
	 */
	reg_set_20602.CONFIG.reg_u.REG.FIFO_MODE = 0x1;
	if (icm20602_write_reg_simple(st, reg_set_20602.CONFIG))
		return -MPU_FAIL;

	/* reset fifo */
	reg_set_20602.USER_CTRL.reg_u.REG.FIFO_RST = 0x1;
	if (icm20602_write_reg_simple(st, reg_set_20602.USER_CTRL)) {
		reg_set_20602.USER_CTRL.reg_u.REG.FIFO_RST = 0x0;
		return -MPU_FAIL;
	}
	reg_set_20602.USER_CTRL.reg_u.REG.FIFO_RST = 0x0;

	/* Enable FIFO on specified sensors */
	reg_set_20602.FIFO_EN.reg_u.REG.GYRO_FIFO_EN = 0x1;
	reg_set_20602.FIFO_EN.reg_u.REG.ACCEL_FIFO_EN = 0x1;
	if (icm20602_write_reg_simple(st, reg_set_20602.FIFO_EN))
		return -MPU_FAIL;

	if (icm20602_config_waterlevel(st))
		return -MPU_FAIL;

	if (icm20602_start_fifo(st))
		return -MPU_FAIL;

	return MPU_SUCCESS;
}

static int icm20602_initialize_gyro(struct inv_icm20602_state *st)
{
	struct icm20602_user_config *config = NULL;
	int result = MPU_SUCCESS;

	if (st == NULL)
		return -MPU_FAIL;

	/*
	 * ICM20602 supports gyro sampling rate up to 32KHz
	 * when fchoice_b != 0x00
	 * In our driver, we supports up to 8KHz
	 * thus always set fchoice_b to 0x00;
	 */
	config = st->config;
	/*
	 * SAPLRT_DIV in ICM20602_REG_SMPLRT_DIV is only used for 1kHz internal
	 * sampling, i.e. fchoice_b in ICM20602_REG_GYRO_CONFIG is 00
	 * and 0 < dlpf_cfg in ICM20602_REG_CONFIG < 7
	 * SAMPLE_RATE=Internal_Sample_Rate / (1 + SMPLRT_DIV)
	 */
	if (config->gyro_accel_sample_rate <= ICM20602_SAMPLE_RATE_1000HZ)
		reg_set_20602.SMPLRT_DIV.reg_u.reg =
		ICM20602_INTERNAL_SAMPLE_RATE_HZ /
		config->gyro_accel_sample_rate - 1;

	result = icm20602_write_reg_simple(st, reg_set_20602.SMPLRT_DIV);

	/* Set gyro&temperature(combine) LPF */
	reg_set_20602.CONFIG.reg_u.REG.DLFP_CFG = config->gyro_lpf;
	result |= icm20602_write_reg_simple(st, reg_set_20602.CONFIG);

	/* Set gyro full scale range */
	reg_set_20602.GYRO_CONFIG.reg_u.REG.FCHOICE_B = 0x0;
	reg_set_20602.GYRO_CONFIG.reg_u.REG.FS_SEL = config->gyro_fsr;
	result |= icm20602_write_reg_simple(st, reg_set_20602.GYRO_CONFIG);

	/* Set Accel full scale range */
	reg_set_20602.ACCEL_CONFIG.reg_u.REG.ACCEL_FS_SEL = config->acc_fsr;
	result |= icm20602_write_reg_simple(st, reg_set_20602.ACCEL_CONFIG);

	/*
	 * Set accel LPF
	 * Support accel sample rate up to 1KHz
	 * thus set accel_fchoice_b to 0x00
	 * The actual accel sample rate is 1KHz/(1+SMPLRT_DIV)
	 */
	reg_set_20602.ACCEL_CONFIG2.reg_u.REG.ACCEL_FCHOICE_B = 0x0;
	reg_set_20602.ACCEL_CONFIG2.reg_u.REG.A_DLPF_CFG = config->acc_lpf;
	result |= icm20602_write_reg_simple(st,
				reg_set_20602.ACCEL_CONFIG2);

	if (result) {
		pr_err("icm20602 init gyro and accel failed\n");
		return -MPU_FAIL;
	}

	return result;
}

int icm20602_set_power_itg(struct inv_icm20602_state *st, bool power_on)
{
	int result = MPU_SUCCESS;

	if (power_on) {
		reg_set_20602.PWR_MGMT_1.reg_u.reg = 0;
		result = icm20602_write_reg_simple(st,
					reg_set_20602.PWR_MGMT_1);
	} else {
		reg_set_20602.PWR_MGMT_1.reg_u.REG.SLEEP = 0x1;
		result = icm20602_write_reg_simple(st,
					reg_set_20602.PWR_MGMT_1);
	}
	if (result) {
		pr_err("set power failed power %d  err %d\n",
					power_on, result);
		return result;
	}

	if (power_on)
		msleep(30);

	return result;
}

int icm20602_init_device(struct inv_icm20602_state *st)
{
	int result = MPU_SUCCESS;
	struct icm20602_user_config *config = NULL;
	int package_count;
	int i;

	config = st->config;
	if (st == NULL || st->config == NULL) {
		pr_err("icm20602 validate config failed\n");
		return -MPU_FAIL;
	}

	/* turn on gyro and accel */
	reg_set_20602.PWR_MGMT_2.reg_u.reg = 0x0;
	result |= icm20602_write_reg_simple(st, reg_set_20602.PWR_MGMT_2);
	msleep(30);

	/* disable INT */
	reg_set_20602.INT_ENABLE.reg_u.reg = 0x0;
	result |= icm20602_write_reg_simple(st, reg_set_20602.INT_ENABLE);

	/* disbale FIFO */
	reg_set_20602.FIFO_EN.reg_u.reg = 0x0;
	result |= icm20602_write_reg_simple(st, reg_set_20602.FIFO_EN);

	/* reset FIFO */
	reg_set_20602.USER_CTRL.reg_u.REG.FIFO_RST = 0x1;
	result |= icm20602_write_reg_simple(st, reg_set_20602.USER_CTRL);
	reg_set_20602.USER_CTRL.reg_u.REG.FIFO_RST = 0x0;
	msleep(30);

	/* init gyro and accel */
	if (icm20602_initialize_gyro(st)) {
		pr_err("icm20602 init device failed\n");
		return -MPU_FAIL;
	}

	/* if FIFO enable, config FIFO */
	if (config->fifo_enabled) {
		if (icm20602_config_fifo(st)) {
			pr_err("icm20602 init config fifo failed\n");
			return -MPU_FAIL;
		}
	} else {
		/* enable interrupt */
		reg_set_20602.INT_ENABLE.reg_u.REG.DATA_RDY_INT_EN = 0x0;
		if (icm20602_write_reg_simple(st,
				reg_set_20602.INT_ENABLE)) {
			pr_err("icm20602 set raw rdy failed\n");
			return -MPU_FAIL;
		}
	}

	/* buffer malloc */
	package_count = config->fifo_waterlevel / ICM20602_PACKAGE_SIZE;

	st->buf = kzalloc(config->fifo_waterlevel * 2, GFP_ATOMIC);
	if (!st->buf)
		return -ENOMEM;

	st->data_push = kcalloc(package_count,
		sizeof(struct struct_icm20602_data), GFP_ATOMIC);
	if (!st->data_push)
		return -ENOMEM;

	for (i = 0; i < package_count; i++) {
		st->data_push[i].raw_data =
			kzalloc(ICM20602_PACKAGE_SIZE, GFP_ATOMIC);
	}

	return result;
}

int icm20602_reset_fifo(struct inv_icm20602_state *st)
{
	reg_set_20602.USER_CTRL.reg_u.REG.FIFO_RST = 0x1;
	if (icm20602_write_reg_simple(st, reg_set_20602.USER_CTRL)) {
		reg_set_20602.USER_CTRL.reg_u.REG.FIFO_RST = 0x0;
		return -MPU_FAIL;
	}
	reg_set_20602.USER_CTRL.reg_u.REG.FIFO_RST = 0x0;
	return MPU_SUCCESS;
}


void icm20602_rw_test(struct inv_icm20602_state *st)
{
	uint8_t val = 0;

	reg_set_20602.PWR_MGMT_2.reg_u.REG.STBY_ZG = 0x1;
	icm20602_write_reg_simple(st, reg_set_20602.PWR_MGMT_2);
	reg_set_20602.CONFIG.reg_u.REG.FIFO_MODE = 0x1;
	icm20602_write_reg_simple(st, reg_set_20602.CONFIG);

	icm20602_read_reg(st, reg_set_20602.PWR_MGMT_2.address, &val);
	icm20602_read_reg(st, reg_set_20602.CONFIG.address, &val);

}

int icm20602_detect(struct inv_icm20602_state *st)
{
	int result = MPU_SUCCESS;
	uint8_t retry = 0, val = 0;

	pr_debug("icm20602_detect\n");
	/* reset to make sure previous state are not there */
	reg_set_20602.PWR_MGMT_1.reg_u.REG.DEVICE_RESET = 0x1;
	result = icm20602_write_reg_simple(st, reg_set_20602.PWR_MGMT_1);
	if (result) {
		pr_err("mpu write reg 0x%x value 0x%x failed\n",
		reg_set_20602.PWR_MGMT_1.reg_u.reg,
		reg_set_20602.PWR_MGMT_1.reg_u.REG.DEVICE_RESET);
		return result;
	}
	reg_set_20602.PWR_MGMT_1.reg_u.REG.DEVICE_RESET = 0x0;

	/* the power up delay */
	msleep(30);

	/* out of sleep */
	result = icm20602_set_power_itg(st, true);
	if (result)
		return result;
	/* get who am i register */
	while (retry < 10) {
		/* get version (expecting 0x12 for the icm20602) */
		icm20602_read_reg(st, reg_set_20602.WHO_AM_I.address, &val);
		if (val == ICM20602_WHO_AM_I)
			break;
		retry++;
	}

	if (val != ICM20602_WHO_AM_I) {
		pr_err("detect mpu failed,whoami reg 0x%x\n", val);
		result = -MPU_FAIL;
	} else {
		pr_debug("detect mpu ok,whoami reg 0x%x\n", val);
	}
	icm20602_rw_test(st);

	return result;
}
