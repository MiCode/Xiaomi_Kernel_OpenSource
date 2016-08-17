/*
* Copyright (C) 2012 nVidia Corp.
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
 *      @file    inv_slave_kxtf9.c
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
#define KXTF9_CHIP_ID			1
#define KXTF9_RANGE_SET		0
#define KXTF9_BW_SET			4

/* range and bandwidth */
#define KXTF9_RANGE_2G                 0
#define KXTF9_RANGE_4G                 1
#define KXTF9_RANGE_8G                 2
#define KXTF9_RANGE_16G                3

/*      register definitions */
#define KXTF9_XOUT_HPF_L                (0x00)
#define KXTF9_XOUT_HPF_H                (0x01)
#define KXTF9_YOUT_HPF_L                (0x02)
#define KXTF9_YOUT_HPF_H                (0x03)
#define KXTF9_ZOUT_HPF_L                (0x04)
#define KXTF9_ZOUT_HPF_H                (0x05)
#define KXTF9_XOUT_L                    (0x06)
#define KXTF9_XOUT_H                    (0x07)
#define KXTF9_YOUT_L                    (0x08)
#define KXTF9_YOUT_H                    (0x09)
#define KXTF9_ZOUT_L                    (0x0A)
#define KXTF9_ZOUT_H                    (0x0B)
#define KXTF9_ST_RESP                   (0x0C)
#define KXTF9_WHO_AM_I                  (0x0F)
#define KXTF9_TILT_POS_CUR              (0x10)
#define KXTF9_TILT_POS_PRE              (0x11)
#define KXTF9_INT_SRC_REG1              (0x15)
#define KXTF9_INT_SRC_REG2              (0x16)
#define KXTF9_STATUS_REG                (0x18)
#define KXTF9_INT_REL                   (0x1A)
#define KXTF9_CTRL_REG1                 (0x1B)
#define KXTF9_CTRL_REG2                 (0x1C)
#define KXTF9_CTRL_REG3                 (0x1D)
#define KXTF9_INT_CTRL_REG1             (0x1E)
#define KXTF9_INT_CTRL_REG2             (0x1F)
#define KXTF9_INT_CTRL_REG3             (0x20)
#define KXTF9_DATA_CTRL_REG             (0x21)
#define KXTF9_TILT_TIMER                (0x28)
#define KXTF9_WUF_TIMER                 (0x29)
#define KXTF9_TDT_TIMER                 (0x2B)
#define KXTF9_TDT_H_THRESH              (0x2C)
#define KXTF9_TDT_L_THRESH              (0x2D)
#define KXTF9_TDT_TAP_TIMER             (0x2E)
#define KXTF9_TDT_TOTAL_TIMER           (0x2F)
#define KXTF9_TDT_LATENCY_TIMER         (0x30)
#define KXTF9_TDT_WINDOW_TIMER          (0x31)
#define KXTF9_WUF_THRESH                (0x5A)
#define KXTF9_TILT_ANGLE                (0x5C)
#define KXTF9_HYST_SET                  (0x5F)

/* mode settings */
#define KXTF9_MODE_SUSPEND     0
#define KXTF9_MODE_NORMAL      1

#define KXTF9_MAX_DUR (0xFF)
#define KXTF9_MAX_THS (0xFF)
#define KXTF9_THS_COUNTS_P_G (32)

#define KXTF9_TABLE_END	0xFF
#define KXTF9_TABLE_WAIT_MS	0xFE

struct kxtf9_reg {
	u8 reg;
	u8 val;
};

struct kxtf9_compare {
	int data;
	u8 val;
};

static struct kxtf9_compare kxtf9_compare_fs[] = {
	{2, 0x00},
	{4, 0x08},
	{8, 0x10},
};

static struct kxtf9_compare kxtf9_compare_rate[] = {
	{25, 0x01},
	{50, 0x02},
	{100, 0x03},
	{200, 0x04},
	{400, 0x05},
	{800, 0x06},
};

static struct kxtf9_reg kxtf9_table_init[] = {
	{KXTF9_CTRL_REG1, 0x40},
	{KXTF9_DATA_CTRL_REG, 0x36},
	{KXTF9_CTRL_REG3, 0xCD},
	{KXTF9_TABLE_WAIT_MS, 2},
	{KXTF9_TABLE_END, 0},
};

static struct kxtf9_reg kxtf9_table_suspend[] = {
	{KXTF9_CTRL_REG1, 0x40},
	{KXTF9_INT_CTRL_REG1, 0x00},
	{KXTF9_WUF_THRESH, 0x02},
	{KXTF9_DATA_CTRL_REG, 0x00},
	{KXTF9_WUF_TIMER, 0x00},
	{KXTF9_CTRL_REG1, 0x40},
	{KXTF9_TABLE_END, 0},
};

enum kxtf9_seq_en {
	KXTF9_SEQ_EN_CTRL_REG1_START = 0,
	KXTF9_SEQ_EN_INT_CTRL_REG1,
	KXTF9_SEQ_EN_WUF_THRESH,
	KXTF9_SEQ_EN_DATA_CTRL_REG,
	KXTF9_SEQ_EN_WUF_TIMER,
	KXTF9_SEQ_EN_CTRL_REG1,
	KXTF9_SEQ_EN_TABLE_END,
	KXTF9_SEQ_EN_TABLE_SIZE,
};

static struct kxtf9_reg kxtf9_table_resume[] = {
	[KXTF9_SEQ_EN_CTRL_REG1_START] = {KXTF9_CTRL_REG1, 0x40},
	[KXTF9_SEQ_EN_INT_CTRL_REG1] = {KXTF9_INT_CTRL_REG1, 0x00},
	[KXTF9_SEQ_EN_WUF_THRESH] = {KXTF9_WUF_THRESH, 0x01},
	[KXTF9_SEQ_EN_DATA_CTRL_REG] = {KXTF9_DATA_CTRL_REG, 0x04},
	[KXTF9_SEQ_EN_WUF_TIMER] = {KXTF9_WUF_TIMER, 0xFF},
	[KXTF9_SEQ_EN_CTRL_REG1] = {KXTF9_CTRL_REG1, 0xC0},
	[KXTF9_SEQ_EN_TABLE_END] = {KXTF9_TABLE_END, 0},
};

struct kxtf9_data {
	int mode;
};

static struct kxtf9_data kxtf9_info = {
	.mode = KXTF9_MODE_SUSPEND,
};

static int kxtf9_wr_table(struct inv_gyro_state_s *st, struct kxtf9_reg table[])
{
	int err;
	struct kxtf9_reg *next;

	err = set_3050_bypass(st, 1); /*set to bypass mode */
	if (err)
		return err;

	for (next = table; next->reg != KXTF9_TABLE_END; next++) {
		if (next->reg == KXTF9_TABLE_WAIT_MS) {
			msleep(next->val);
			continue;
		}

		err = inv_secondary_write(next->reg, next->val);
		if (err)
			return err;
	}

	return 0;
}

static int kxtf9_wr(struct inv_gyro_state_s *st, u8 reg, u8 val)
{
	int err;

	err = set_3050_bypass(st, 1);
	if (err)
		return err;

	err = inv_secondary_write(KXTF9_CTRL_REG1, 0x40);
	if (err)
		return err;

	if (reg)
		err = inv_secondary_write(reg, val);
	err |= inv_secondary_write(KXTF9_CTRL_REG1,
			       kxtf9_table_resume[KXTF9_SEQ_EN_CTRL_REG1].val);
	err |= set_3050_bypass(st, 0);
	return err;
}

static int kxtf9_find_least_match(struct kxtf9_compare *cmp, unsigned int n,
				  int data, u8 *val)
{
	unsigned int i;

	for (i = 0; i < n; i++) {
		if (data <= cmp[i].data)
			break;
	}

	if (i == n)
		return -EINVAL;

	*val = cmp[i].val;
	return 0;
}

static int kxtf9_setup(struct inv_gyro_state_s *st)
{
	int err;

	kxtf9_info.mode = KXTF9_MODE_SUSPEND;
	err = kxtf9_wr_table(st, kxtf9_table_init);
	return err;
}

static int kxtf9_suspend(struct inv_gyro_state_s *st)
{
	int err;
	unsigned char data;

	if (kxtf9_info.mode == KXTF9_MODE_SUSPEND)
		return 0;

	err = kxtf9_wr_table(st, kxtf9_table_suspend);
	err |= inv_secondary_read(KXTF9_INT_REL, 1, &data);
	if (err)
		return err;

	kxtf9_info.mode = KXTF9_MODE_SUSPEND;
	return 0;
}

static int kxtf9_resume(struct inv_gyro_state_s *st)
{
	int err;

	if (kxtf9_info.mode == KXTF9_MODE_NORMAL)
		return 0;

	err = kxtf9_wr_table(st, kxtf9_table_resume);
	err |= set_3050_bypass(st, 0); /* recover bypass mode */
	if (err)
		return err;

	kxtf9_info.mode = KXTF9_MODE_NORMAL;
	return 0;
}

static int kxtf9_combine_data(unsigned char *in, short *out)
{
	out[0] = (in[0] | (in[1]<<8));
	out[1] = (in[2] | (in[3]<<8));
	out[2] = (in[4] | (in[5]<<8));
	return 0;
}

static int kxtf9_get_mode(struct inv_gyro_state_s *st)
{
	return kxtf9_info.mode;
};

static int kxtf9_set_lpf(struct inv_gyro_state_s *st, int rate)
{
	int err;
	u8 val = 0;

	err = kxtf9_find_least_match(kxtf9_compare_rate,
				     ARRAY_SIZE(kxtf9_compare_rate),
				     rate, &val);
	if (err)
		return err;

	kxtf9_table_resume[KXTF9_SEQ_EN_DATA_CTRL_REG].val &= 0xF8;
	kxtf9_table_resume[KXTF9_SEQ_EN_DATA_CTRL_REG].val |= val;
	if (kxtf9_info.mode < KXTF9_MODE_NORMAL)
		return 0;

	err = kxtf9_wr(st, KXTF9_DATA_CTRL_REG, val);
	return err;
}

static int kxtf9_set_fs(struct inv_gyro_state_s *st, int fs)
{
	int err;
	u8 val = 0;

	err = kxtf9_find_least_match(kxtf9_compare_fs,
				     ARRAY_SIZE(kxtf9_compare_fs), fs, &val);
	if (err)
		return err;

	kxtf9_table_resume[KXTF9_SEQ_EN_CTRL_REG1].val &= 0xE7;
	kxtf9_table_resume[KXTF9_SEQ_EN_CTRL_REG1].val |= val;
	if (kxtf9_info.mode < KXTF9_MODE_NORMAL)
		return 0;

	err = kxtf9_wr(st, 0, 0); /* NULL reg to update CTRL_REG1 */
	return err;
}

static struct inv_mpu_slave slave_kxtf9 = {
	.suspend = kxtf9_suspend,
	.resume  = kxtf9_resume,
	.setup   = kxtf9_setup,
	.combine_data = kxtf9_combine_data,
	.get_mode = kxtf9_get_mode,
	.set_lpf = kxtf9_set_lpf,
	.set_fs  = kxtf9_set_fs
};

int inv_register_kxtf9_slave(struct inv_gyro_state_s *st)
{
	st->mpu_slave = &slave_kxtf9;
	return 0;
}

