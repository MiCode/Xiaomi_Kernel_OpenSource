/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2019 MediaTek Inc. */

#ifndef __ADAPTOR_I2C_H__
#define __ADAPTOR_I2C_H__

int adaptor_i2c_rd_u8(struct i2c_client *i2c_client,
		u16 addr, u16 reg, u8 *val);

int adaptor_i2c_rd_u16(struct i2c_client *i2c_client,
		u16 addr, u16 reg, u16 *val);

int adaptor_i2c_rd_p8(struct i2c_client *i2c_client,
		u16 addr, u16 reg, u8 *p_vals, u32 n_vals);

int adaptor_i2c_wr_u8(struct i2c_client *i2c_client,
		u16 addr, u16 reg, u8 val);

int adaptor_i2c_wr_u16(struct i2c_client *i2c_client,
		u16 addr, u16 reg, u16 val);

int adaptor_i2c_wr_p8(struct i2c_client *i2c_client,
		u16 addr, u16 reg, u8 *p_vals, u32 n_vals);

int adaptor_i2c_wr_p16(struct i2c_client *i2c_client,
		u16 addr, u16 reg, u16 *p_vals, u32 n_vals);

int adaptor_i2c_wr_regs_u8(struct i2c_client *i2c_client,
		u16 addr, u16 *list, u32 len);

int adaptor_i2c_wr_regs_u16(struct i2c_client *i2c_client,
		u16 addr, u16 *list, u32 len);

#endif
