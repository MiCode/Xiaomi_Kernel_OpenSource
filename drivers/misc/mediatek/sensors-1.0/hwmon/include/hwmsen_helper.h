/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

/*
 * Copyright(C)2014 MediaTek Inc.
 * Modification based on code covered by the below mentioned copyright
 * and/or permission notice(S).
 */

#include "hwmsensor.h"
#include <linux/i2c.h>
#include <linux/types.h>
#ifndef __HWMSEN_HELPER_H__
#define __HWMSEN_HELPER_H__

#define C_I2C_FIFO_SIZE 8 /*according i2c_mt6516.c */

struct hwmsen_reg {
	const char *name;
	u16 addr;
	u16 mode;
	u16 mask;
	u16 len;
};
/*----------------------------------------------------------------------------*/
#define HWMSEN_DUMMY_REG(X)                                                    \
{                                                                      \
	NULL, X, REG_NA, 0x00, 0                                       \
}
/*----------------------------------------------------------------------------*/
struct hwmsen_reg_test_multi {
	u8 addr;
	u8 len;
	u8 mode;
	u8 _align;
};
/*----------------------------------------------------------------------------*/
enum {
REG_NA = 0x0000,
REG_RO = 0x0001,
REG_WO = 0x0002,
REG_LK = 0x0004, /*lcoked, register test will by-pass this register */
REG_RW = REG_RO | REG_WO,
};
/*----------------------------------------------------------------------------*/
/*
 * @sign, map: only used in accelerometer/magnetic field
 *      sometimes, the sensor output need to be remapped before reporting to
 * framework.
 *      the 'sign' is only -1 or +1 to align the sign for framework's coordinate
 * system
 *      the 'map'  align the value for framework's coordinate system. Take
 * accelerometer
 *      as an example:
 *      assume HAL receives original acceleration: acc[] = {100, 0, 100}
 *      sign[] = {1, -1, 1, 0};
 *      map[]  = {HWM_CODE_ACC_Y, HWM_CODE_ACC_X, HWM_CODE_ACC_Z, 0};
 *      according to the above 'sign' & 'map', the sensor output need to remap
 * as {y, -x, z}:
 *      float resolution = unit_numerator*GRAVITY_EARTH/unit_denominator;
 *      acc_x = sign[0]*acc[map[0]]*resolution;
 *      acc_y = sign[1]*acc[map[1]]*resolution;
 *      acc_z = sign[2]*acc[map[2]]*resolution;
 */
struct hwmsen_convert {
	s8 sign[C_MAX_HWMSEN_EVENT_NUM];
	u8 map[C_MAX_HWMSEN_EVENT_NUM];
};
/*----------------------------------------------------------------------------*/
struct hwmsen_conf {
	/*output sensitivity of sensor data */
	s32 sensitivity[C_MAX_HWMSEN_EVENT_NUM];
	int num;
};
/*----------------------------------------------------------------------------*/
typedef struct hwmsen_reg *(*find_reg_t)(int reg_idx);
/*----------------------------------------------------------------------------*/
extern int hwmsen_set_bits(struct i2c_client *client, u8 addr, u8 bits);
extern int hwmsen_clr_bits(struct i2c_client *client, u8 addr, u8 bits);
extern int hwmsen_read_byte(struct i2c_client *client, u8 addr, u8 *data);
extern int hwmsen_write_byte(struct i2c_client *client, u8 addr, u8 data);
extern int hwmsen_read_block(struct i2c_client *client, u8 addr, u8 *data,
			     u8 len);
extern int hwmsen_write_block(struct i2c_client *client, u8 addr, u8 *data,
			      u8 len);
extern void hwmsen_single_rw(struct i2c_client *client, struct hwmsen_reg *regs,
			     int num);
extern void hwmsen_multi_rw(struct i2c_client *client, find_reg_t findreg,
			    struct hwmsen_reg_test_multi *items, int inum);
extern ssize_t hwmsen_show_dump(struct i2c_client *client, u8 startAddr,
				u8 *regtbl, u32 regnum, find_reg_t findreg,
				char *buf, u32 buflen);
extern ssize_t hwmsen_read_all_regs(struct i2c_client *client,
				    struct hwmsen_reg *regs, u32 num, char *buf,
				    u32 buflen);
extern ssize_t hwmsen_show_reg(struct i2c_client *client, u8 addr, char *buf,
			       u32 buflen);
extern ssize_t hwmsen_store_reg(struct i2c_client *client, u8 addr,
				const char *buf, size_t count);
extern ssize_t hwmsen_show_byte(struct device *dev,
				struct device_attribute *attr, char *buf,
				u32 buflen);
extern ssize_t hwmsen_store_byte(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count);
extern ssize_t hwmsen_show_word(struct device *dev,
				struct device_attribute *attr, char *buf,
				u32 buflen);
extern ssize_t hwmsen_store_word(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count);
extern int hwmsen_get_convert(int direction, struct hwmsen_convert *cvt);
/*----------------------------------------------------------------------------*/
#endif
