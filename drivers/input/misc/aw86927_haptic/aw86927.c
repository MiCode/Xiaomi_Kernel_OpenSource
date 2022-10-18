/*
 * aw86927.c
 *
 *
 *
 * Copyright (c) 2021 AWINIC Technology CO., LTD
 *
 * Author: <chelvming@awinic.com.cn>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/debugfs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <linux/power_supply.h>
#include <linux/vmalloc.h>
#include <linux/pm_qos.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/control.h>
#include <sound/soc.h>
#include "aw_haptic.h"
#include  "ringbuffer.h"
#include "aw86927.h"
#include "aw_config.h"

#include <linux/thermal.h>

#define AW86927_BROADCAST_ADDR			(0x00)
#define AW86927_LEFT_CHIP_ADDR			(0x5A)
#define AW86927_RIGHT_CHIP_ADDR			(0x5B)


static int aw86927_ram_update(struct aw86927 *aw86927);
static int aw86927_haptic_ram_vbat_comp(struct aw86927 *aw86927, bool flag);
static int aw86927_haptic_play_mode(struct aw86927 *aw86927,
				    unsigned char play_mode);
static void aw86927_haptic_play_go(struct aw86927 *aw86927);
static int aw86927_set_base_addr(struct aw86927 *aw86927);
static int aw86927_haptic_stop(struct aw86927 *aw86927);
/******************************************************
 *
 * variable
 *
 ******************************************************/
static struct aw86927_container *aw86927_rtp;

/******************************************************
 *
 * i2c write/read
 *
 ******************************************************/
static int aw86927_i2c_write(struct aw86927 *aw86927,
			    unsigned char reg_addr, unsigned char reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < AW_I2C_RETRIES) {
		ret =
		    i2c_smbus_write_byte_data(aw86927->i2c, reg_addr, reg_data);
		if (ret < 0) {
			aw_err("%s: i2c_write cnt=%d error=%d\n", __func__, cnt,
			       ret);
		} else {
			break;
		}
		cnt++;
		msleep(AW_I2C_RETRY_DELAY);
	}

	return ret;
}

static int aw86927_i2c_read(struct aw86927 *aw86927,
			   unsigned char reg_addr, unsigned char *reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < AW_I2C_RETRIES) {
		ret = i2c_smbus_read_byte_data(aw86927->i2c, reg_addr);
		if (ret < 0) {
			aw_err("%s: i2c_read cnt=%d error=%d\n", __func__, cnt,
			       ret);
		} else {
			*reg_data = ret;
			break;
		}
		cnt++;
		msleep(AW_I2C_RETRY_DELAY);
	}

	return ret;
}

static int aw86927_i2c_write_bits(struct aw86927 *aw86927,
				 unsigned char reg_addr, unsigned int mask,
				 unsigned char reg_data)
{
	unsigned char reg_val = 0;

	aw86927_i2c_read(aw86927, reg_addr, &reg_val);
	reg_val &= mask;
	reg_val |= reg_data;
	aw86927_i2c_write(aw86927, reg_addr, reg_val);

	return 0;
}

static int aw86927_i2c_writes(struct aw86927 *aw86927,
			      unsigned char reg_addr, unsigned char *buf,
			      unsigned int len)
{
	int ret = -1;
	unsigned char *data;

	data = kmalloc(len + 1, GFP_KERNEL);

	data[0] = reg_addr;
	memcpy(&data[1], buf, len);

	ret = i2c_master_send(aw86927->i2c, data, len + 1);
	if (ret < 0)
		aw_err("%s: i2c master send error\n", __func__);

	kfree(data);

	return ret;
}
int aw86927_i2c_reads(struct aw86927 *aw86927, unsigned char reg_addr,
		      unsigned char *buf, unsigned int len)
{
	int ret;
	struct i2c_msg msg[] = {
		[0] = {
			.addr = aw86927->i2c->addr,
			.flags = 0,
			.len = sizeof(uint8_t),
			.buf = &reg_addr,
			},
		[1] = {
			.addr = aw86927->i2c->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = buf,
			},
	};

	ret = i2c_transfer(aw86927->i2c->adapter, msg, ARRAY_SIZE(msg));
	if (ret < 0) {
		aw_err("%s: transfer failed.", __func__);
		return ret;
	} else if (ret != 2) {
		aw_err("%s: transfer failed(size error).", __func__);
		return -ENXIO;
	}

	return ret;
}

static void aw86927_select_edge_int_mode(struct aw86927 *aw86927)
{
	aw_info("%s enter!\n", __func__);
	/* edge int mode */
	aw86927_i2c_write_bits(aw86927, AW86927_REG_SYSCTRL4,
			       AW86927_BIT_SYSCTRL4_INT_MODE_MASK,
			       AW86927_BIT_SYSCTRL4_INT_MODE_EDGE);
	aw86927_i2c_write_bits(aw86927, AW86927_REG_SYSCTRL4,
			       AW86927_BIT_SYSCTRL4_INT_EDGE_MODE_MASK,
			       AW86927_BIT_SYSCTRL4_INT_EDGE_MODE_POS);
}
static int aw86927_set_cont_wait_num(struct aw86927 *aw86927, unsigned char val)
{
	aw_info("%s enter!\n", __func__);
	aw86927_i2c_write(aw86927, AW86927_REG_CONTCFG4, val);
	return 0;
}

static int aw86927_set_cont_drv_lvl(struct aw86927 *aw86927,
				    unsigned char drv1_lvl,
				    unsigned char drv2_lvl)
{
	aw_info("%s enter!\n", __func__);
	aw86927_i2c_write_bits(aw86927, AW86927_REG_CONTCFG6,
				AW86927_BIT_CONTCFG6_DRV1_LVL_MASK,
				drv1_lvl);
	aw86927_i2c_write(aw86927, AW86927_REG_CONTCFG7, drv2_lvl);
	return 0;
}

static int aw86927_set_cont_drv_time(struct aw86927 *aw86927,
				     unsigned char drv1_time,
				     unsigned char drv2_time)
{
	aw_info("%s enter!\n", __func__);
	aw86927_i2c_write(aw86927, AW86927_REG_CONTCFG8, drv1_time);
	aw86927_i2c_write(aw86927, AW86927_REG_CONTCFG9, drv2_time);
	return 0;
}

static int aw86927_set_cont_brk_time(struct aw86927 *aw86927, unsigned char val)
{
	aw_info("%s enter!\n", __func__);
	aw86927_i2c_write(aw86927, AW86927_REG_CONTCFG10, val);
	return 0;
}

static int aw86927_is_enter_standby(struct aw86927 *aw86927)
{
	int ret = -1;
	unsigned char reg_val = 0;

	aw_dbg("%s enter!\n", __func__);
	ret = aw86927_i2c_read(aw86927, AW86927_REG_GLBRD5, &reg_val);
	aw_dbg("%s glb_state = 0x%02x!\n", __func__, reg_val);
	if (ret < 0)
		return ret;
	if (reg_val == AW86927_BIT_GLBRD5_STATE_STANDBY)
		ret = 0;
	return ret;
}

static void aw86927_force_enter_standby(struct aw86927 *aw86927)
{
	aw_info("%s enter!\n", __func__);
	aw86927_i2c_write_bits(aw86927, AW86927_REG_SYSCTRL3,
		AW86927_BIT_SYSCTRL3_STANDBY_MASK,
		AW86927_BIT_SYSCTRL3_STANDBY_ON);
	aw86927_i2c_write_bits(aw86927, AW86927_REG_SYSCTRL3,
		AW86927_BIT_SYSCTRL3_STANDBY_MASK,
		AW86927_BIT_SYSCTRL3_STANDBY_OFF);

}

static int aw86927_haptic_wait_enter_standby(struct aw86927 *aw86927,
					     unsigned int cnt)
{
	int ret = 0;

	aw_dbg("%s enter!\n", __func__);
	while (cnt) {
		ret = aw86927_is_enter_standby(aw86927);
		if (!ret) {
			aw_info("%s: entered standby!\n", __func__);
			break;
		}
		cnt--;
		aw_info("%s: wait for standby\n", __func__);

		usleep_range(2000, 2500);
	}
	if (!cnt)
		ret = -1;
	return ret;
}

static void aw86927_haptic_auto_break_mode(struct aw86927 *aw86927, bool flag)
{
	aw_info("%s enter!\n", __func__);
	if (flag) {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_PLAYCFG3,
			       AW86927_BIT_PLAYCFG3_BRK_EN_MASK,
			       AW86927_BIT_PLAYCFG3_BRK_ENABLE);
	} else {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_PLAYCFG3,
			       AW86927_BIT_PLAYCFG3_BRK_EN_MASK,
			       AW86927_BIT_PLAYCFG3_BRK_DISABLE);
	}
}

static int aw86927_haptic_read_lra_f0(struct aw86927 *aw86927)
{
	int ret = 0;
	unsigned char reg_val = 0;
	unsigned int f0_reg = 0;
	unsigned long f0_tmp = 0;

	aw_info("%s enter\n", __func__);
	/* F_LRA_F0_H */
	ret = aw86927_i2c_read(aw86927, AW86927_REG_CONTCFG14, &reg_val);
	f0_reg = (f0_reg | reg_val) << 8;
	/* F_LRA_F0_L */
	ret = aw86927_i2c_read(aw86927, AW86927_REG_CONTCFG15, &reg_val);
	f0_reg |= (reg_val << 0);
	if (!f0_reg) {
		aw_err("%s didn't get lra f0 because f0_reg value is 0!\n",
			__func__);
		aw86927->f0_cali_status = false;
		return ret;
	}
	aw86927->f0_cali_status = true;
	f0_tmp = 384000 * 10 / f0_reg;
	aw86927->f0 = (unsigned int)f0_tmp;
	aw_info("%s lra_f0=%d\n", __func__, aw86927->f0);

	return ret;
}

static void aw86927_haptic_f0_detect(struct aw86927 *aw86927, bool flag)
{
	aw_info("%s enter!\n", __func__);
	if (flag) {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_CONTCFG1,
			       AW86927_BIT_CONTCFG1_EN_F0_DET_MASK,
			       AW86927_BIT_CONTCFG1_F0_DET_ENABLE);
	} else {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_CONTCFG1,
			       AW86927_BIT_CONTCFG1_EN_F0_DET_MASK,
			       AW86927_BIT_CONTCFG1_F0_DET_DISABLE);
	}
}

static void aw86927_haptic_trig1_param_init(struct aw86927 *aw86927)
{
	aw_info("%s enter\n", __func__);

	aw86927->trig[0].trig_level = aw86927->info.trig_config[0];
	aw86927->trig[0].trig_polar = aw86927->info.trig_config[1];
	aw86927->trig[0].pos_enable = aw86927->info.trig_config[2];
	aw86927->trig[0].pos_sequence = aw86927->info.trig_config[3];
	aw86927->trig[0].neg_enable = aw86927->info.trig_config[4];
	aw86927->trig[0].neg_sequence = aw86927->info.trig_config[5];
	aw86927->trig[0].trig_brk = aw86927->info.trig_config[6];
	aw86927->trig[0].trig_bst = aw86927->info.trig_config[7];
}

static void aw86927_haptic_trig2_param_init(struct aw86927 *aw86927)
{
	aw_info("%s enter\n", __func__);

	aw86927->trig[1].trig_level = aw86927->info.trig_config[8 + 0];
	aw86927->trig[1].trig_polar = aw86927->info.trig_config[8 + 1];
	aw86927->trig[1].pos_enable = aw86927->info.trig_config[8 + 2];
	aw86927->trig[1].pos_sequence = aw86927->info.trig_config[8 + 3];
	aw86927->trig[1].neg_enable = aw86927->info.trig_config[8 + 4];
	aw86927->trig[1].neg_sequence = aw86927->info.trig_config[8 + 5];
	aw86927->trig[1].trig_brk = aw86927->info.trig_config[8 + 6];
	aw86927->trig[1].trig_bst = aw86927->info.trig_config[8 + 7];
}

static void aw86927_haptic_trig3_param_init(struct aw86927 *aw86927)
{
	aw_info("%s enter\n", __func__);
	aw86927->trig[2].trig_level = aw86927->info.trig_config[16 + 0];
	aw86927->trig[2].trig_polar = aw86927->info.trig_config[16 + 1];
	aw86927->trig[2].pos_enable = aw86927->info.trig_config[16 + 2];
	aw86927->trig[2].pos_sequence = aw86927->info.trig_config[16 + 3];
	aw86927->trig[2].neg_enable = aw86927->info.trig_config[16 + 4];
	aw86927->trig[2].neg_sequence = aw86927->info.trig_config[16 + 5];
	aw86927->trig[2].trig_brk = aw86927->info.trig_config[16 + 6];
	aw86927->trig[2].trig_bst = aw86927->info.trig_config[16 + 7];
}

static void aw86927_haptic_trig1_param_config(struct aw86927 *aw86927)
{
	aw_info("%s enter\n", __func__);
	if (aw86927->trig[0].trig_level) {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_TRGCFG7,
				       AW86927_BIT_TRGCFG7_TRG1_MODE_MASK,
				       AW86927_BIT_TRGCFG7_TRG1_MODE_LEVEL);
	} else {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_TRGCFG7,
				       AW86927_BIT_TRGCFG7_TRG1_MODE_MASK,
				       AW86927_BIT_TRGCFG7_TRG1_MODE_EDGE);
	}
	if (aw86927->trig[0].trig_polar) {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_TRGCFG7,
				       AW86927_BIT_TRGCFG7_TRG1_POLAR_MASK,
				       AW86927_BIT_TRGCFG7_TRG1_POLAR_NEG);
	} else {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_TRGCFG7,
				       AW86927_BIT_TRGCFG7_TRG1_POLAR_MASK,
				       AW86927_BIT_TRGCFG7_TRG1_POLAR_POS);
	}
	if (aw86927->trig[0].pos_enable) {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_TRGCFG1,
				       AW86927_BIT_TRG_ENABLE_MASK,
				       AW86927_BIT_TRG_ENABLE);
	} else {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_TRGCFG1,
				       AW86927_BIT_TRG_ENABLE_MASK,
				       AW86927_BIT_TRG_DISABLE);
	}
	if (aw86927->trig[0].neg_enable) {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_TRGCFG4,
				       AW86927_BIT_TRG_ENABLE_MASK,
				       AW86927_BIT_TRG_ENABLE);
	} else {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_TRGCFG4,
				       AW86927_BIT_TRG_ENABLE_MASK,
				       AW86927_BIT_TRG_DISABLE);
	}
	if (aw86927->trig[0].pos_sequence) {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_TRGCFG1,
				       AW86927_BIT_TRG_SEQ_MASK,
				       aw86927->trig[0].pos_sequence);
	}
	if (aw86927->trig[0].neg_sequence) {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_TRGCFG4,
				       AW86927_BIT_TRG_SEQ_MASK,
				       aw86927->trig[0].neg_sequence);
	}
	if (aw86927->trig[0].trig_brk) {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_TRGCFG7,
				AW86927_BIT_TRGCFG7_TRG1_AUTO_BRK_MASK,
				AW86927_BIT_TRGCFG7_TRG1_AUTO_BRK_ENABLE);
	} else {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_TRGCFG7,
				AW86927_BIT_TRGCFG7_TRG1_AUTO_BRK_MASK,
				AW86927_BIT_TRGCFG7_TRG1_AUTO_BRK_DISABLE);
	}
	if (aw86927->trig[0].trig_bst) {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_TRGCFG7,
				       AW86927_BIT_TRGCFG7_TRG1_BST_MASK,
				       AW86927_BIT_TRGCFG7_TRG1_BST_ENABLE);
	} else {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_TRGCFG7,
				       AW86927_BIT_TRGCFG7_TRG1_BST_MASK,
				       AW86927_BIT_TRGCFG7_TRG1_BST_DISABLE);
	}
}

static void aw86927_haptic_trig2_param_config(struct aw86927 *aw86927)
{
	aw_info("%s enter\n", __func__);
	if (aw86927->trig[1].trig_level) {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_TRGCFG7,
				       AW86927_BIT_TRGCFG7_TRG2_MODE_MASK,
				       AW86927_BIT_TRGCFG7_TRG2_MODE_LEVEL);
	} else {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_TRGCFG7,
				       AW86927_BIT_TRGCFG7_TRG2_MODE_MASK,
				       AW86927_BIT_TRGCFG7_TRG2_MODE_EDGE);
	}
	if (aw86927->trig[1].trig_polar) {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_TRGCFG7,
				       AW86927_BIT_TRGCFG7_TRG2_POLAR_MASK,
				       AW86927_BIT_TRGCFG7_TRG2_POLAR_NEG);
	} else {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_TRGCFG7,
				       AW86927_BIT_TRGCFG7_TRG2_POLAR_MASK,
				       AW86927_BIT_TRGCFG7_TRG2_POLAR_POS);
	}
	if (aw86927->trig[1].pos_enable) {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_TRGCFG2,
				       AW86927_BIT_TRG_ENABLE_MASK,
				       AW86927_BIT_TRG_ENABLE);
	} else {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_TRGCFG2,
				       AW86927_BIT_TRG_ENABLE_MASK,
				       AW86927_BIT_TRG_DISABLE);
	}
	if (aw86927->trig[1].neg_enable) {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_TRGCFG5,
				       AW86927_BIT_TRG_ENABLE_MASK,
				       AW86927_BIT_TRG_ENABLE);
	} else {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_TRGCFG5,
				       AW86927_BIT_TRG_ENABLE_MASK,
				       AW86927_BIT_TRG_DISABLE);
	}
	if (aw86927->trig[1].pos_sequence) {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_TRGCFG2,
				       AW86927_BIT_TRG_SEQ_MASK,
				       aw86927->trig[1].pos_sequence);
	}
	if (aw86927->trig[1].neg_sequence) {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_TRGCFG5,
				       AW86927_BIT_TRG_SEQ_MASK,
				       aw86927->trig[1].neg_sequence);
	}
	if (aw86927->trig[1].trig_brk) {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_TRGCFG7,
				AW86927_BIT_TRGCFG7_TRG2_AUTO_BRK_MASK,
				AW86927_BIT_TRGCFG7_TRG2_AUTO_BRK_ENABLE);
	} else {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_TRGCFG7,
				AW86927_BIT_TRGCFG7_TRG2_AUTO_BRK_MASK,
				AW86927_BIT_TRGCFG7_TRG2_AUTO_BRK_DISABLE);
	}
	if (aw86927->trig[1].trig_bst) {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_TRGCFG7,
				       AW86927_BIT_TRGCFG7_TRG2_BST_MASK,
				       AW86927_BIT_TRGCFG7_TRG2_BST_ENABLE);
	} else {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_TRGCFG7,
				       AW86927_BIT_TRGCFG7_TRG2_BST_MASK,
				       AW86927_BIT_TRGCFG7_TRG2_BST_DISABLE);
	}
}

static void aw86927_haptic_trig3_param_config(struct aw86927 *aw86927)
{
	aw_info("%s enter\n", __func__);
	if (aw86927->trig[2].trig_level) {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_TRGCFG8,
				       AW86927_BIT_TRGCFG8_TRG3_MODE_MASK,
				       AW86927_BIT_TRGCFG8_TRG3_MODE_LEVEL);
	} else {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_TRGCFG8,
				       AW86927_BIT_TRGCFG8_TRG3_MODE_MASK,
				       AW86927_BIT_TRGCFG8_TRG3_MODE_EDGE);
	}
	if (aw86927->trig[2].trig_polar) {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_TRGCFG8,
				       AW86927_BIT_TRGCFG8_TRG3_POLAR_MASK,
				       AW86927_BIT_TRGCFG8_TRG3_POLAR_NEG);
	} else {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_TRGCFG8,
				       AW86927_BIT_TRGCFG8_TRG3_POLAR_MASK,
				       AW86927_BIT_TRGCFG8_TRG3_POLAR_POS);
	}
	if (aw86927->trig[2].pos_enable) {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_TRGCFG3,
				       AW86927_BIT_TRG_ENABLE_MASK,
				       AW86927_BIT_TRG_ENABLE);
	} else {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_TRGCFG3,
				       AW86927_BIT_TRG_ENABLE_MASK,
				       AW86927_BIT_TRG_DISABLE);
	}
	if (aw86927->trig[2].neg_enable) {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_TRGCFG6,
				       AW86927_BIT_TRG_ENABLE_MASK,
				       AW86927_BIT_TRG_ENABLE);
	} else {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_TRGCFG6,
				       AW86927_BIT_TRG_ENABLE_MASK,
				       AW86927_BIT_TRG_DISABLE);
	}
	if (aw86927->trig[2].pos_sequence) {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_TRGCFG3,
				       AW86927_BIT_TRG_SEQ_MASK,
				       aw86927->trig[2].pos_sequence);
	}
	if (aw86927->trig[2].neg_sequence) {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_TRGCFG6,
				       AW86927_BIT_TRG_SEQ_MASK,
				       aw86927->trig[2].neg_sequence);
	}
	if (aw86927->trig[2].trig_brk) {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_TRGCFG8,
				AW86927_BIT_TRGCFG8_TRG3_AUTO_BRK_MASK,
				AW86927_BIT_TRGCFG8_TRG3_AUTO_BRK_ENABLE);
	} else {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_TRGCFG8,
				AW86927_BIT_TRGCFG8_TRG3_AUTO_BRK_MASK,
				AW86927_BIT_TRGCFG8_TRG3_AUTO_BRK_DISABLE);
	}
	if (aw86927->trig[2].trig_bst) {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_TRGCFG8,
				       AW86927_BIT_TRGCFG8_TRG3_BST_MASK,
				       AW86927_BIT_TRGCFG8_TRG3_BST_ENABLE);
	} else {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_TRGCFG8,
				       AW86927_BIT_TRGCFG8_TRG3_BST_MASK,
				       AW86927_BIT_TRGCFG8_TRG3_BST_DISABLE);
	}
}

int aw86927_check_qualify(struct aw86927 *aw86927)
{
	int ret = -1;
	unsigned char reg_val = 0;

	ret = aw86927_i2c_read(aw86927, AW86927_REG_EFCFG6, &reg_val);
	if (ret < 0)
		return ret;
	if (!(reg_val & AW86927_BIT_EFCFG6_MASK)) {
		aw_err("unqualified chip!");
		return -ERANGE;
	}
	return 0;
}

static void aw86927_haptic_set_rtp_aei(struct aw86927 *aw86927, bool flag)
{
	aw_dbg("%s enter!\n", __func__);
	if (flag) {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_SYSINTM,
				       AW86927_BIT_SYSINTM_FF_AEM_MASK,
				       AW86927_BIT_SYSINTM_FF_AEM_ON);
	} else {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_SYSINTM,
				       AW86927_BIT_SYSINTM_FF_AEM_MASK,
				       AW86927_BIT_SYSINTM_FF_AEM_OFF);
	}
}

static unsigned char aw86927_haptic_rtp_get_fifo_afs(struct aw86927 *aw86927)
{
	unsigned char ret = 0;
	unsigned char reg_val = 0;

	aw_dbg("%s enter!\n", __func__);
	aw86927->i2c->addr = (u16)AW86927_LEFT_CHIP_ADDR;
	aw86927_i2c_read(aw86927, AW86927_REG_SYSST, &reg_val);
	reg_val &= AW86927_BIT_SYSST_FF_AFS;
	ret = reg_val >> 3;
	return ret;
}

static int aw86927_write_rtp_data(struct aw86927 *aw86927,
				  unsigned char *val, unsigned size)
{
	aw_dbg("%s enter!\n", __func__);
	aw86927_i2c_writes(aw86927, AW86927_REG_RTPDATA, val, size);
	return 0;
}

static int aw86927_set_fifo_addr(struct aw86927 *aw86927)
{
	unsigned int base_addr = 0;
	unsigned int ae_addr_h = 0;
	unsigned int ae_addr_l = 0;
	unsigned int af_addr_h = 0;
	unsigned int af_addr_l = 0;

	aw_info("%s enter!\n", __func__);
	base_addr = aw86927->ram.base_addr;
	ae_addr_h = ((base_addr >> 1) >> 4) & 0xF0;
	ae_addr_l = (base_addr >> 1) & 0x00FF;

	af_addr_h = ((base_addr - (base_addr >> 2)) >> 8) & 0x0F;
	af_addr_l = (base_addr - (base_addr >> 2)) & 0x00FF;
	/* FIFO_AEH */
	aw86927_i2c_write_bits(aw86927, AW86927_REG_RTPCFG3,
			AW86927_BIT_RTPCFG3_FIFO_AEH_MASK,
			(unsigned char)ae_addr_h);
	/* FIFO AEL */
	aw86927_i2c_write(aw86927, AW86927_REG_RTPCFG4,
			(unsigned char)ae_addr_l);
	/* FIFO_AFH */
	aw86927_i2c_write_bits(aw86927, AW86927_REG_RTPCFG3,
			AW86927_BIT_RTPCFG3_FIFO_AFH_MASK,
			(unsigned char)af_addr_h);
	/* FIFO_AFL */
	aw86927_i2c_write(aw86927, AW86927_REG_RTPCFG5,
			(unsigned char)af_addr_l);
	return 0;
}

static int aw86927_get_fifo_addr(struct aw86927 *aw86927)
{
	unsigned int temp = 0;
	unsigned char reg_val = 0;

/*
*	unsigned int temp
*	HIGH<byte4 byte3 byte2 byte1>LOW
*	|_ _ _ _AF-12BIT_ _ _ _AE-12BIT|
*/

	aw_info("%s enter!\n", __func__);
	aw86927_i2c_read(aw86927, AW86927_REG_RTPCFG3, &reg_val);
	temp = ((reg_val & 0x0f) << 24) | ((reg_val & 0xf0) << 4);
	aw86927_i2c_read(aw86927, AW86927_REG_RTPCFG4, &reg_val);
	temp = temp | reg_val;
	aw_info("%s: almost_empty_threshold = %d\n", __func__,
		    (unsigned short)temp);
	aw86927_i2c_read(aw86927, AW86927_REG_RTPCFG5, &reg_val);
	temp = temp | (reg_val << 16);
	aw_info("%s: almost_full_threshold = %d\n", __func__,
		    temp >> 16);
	return 0;
}

static int aw86927_write_ram_data(struct aw86927 *aw86927,
				  struct aw86927_container *aw86927_cont)
{
	int i = 0;
	int len = 0;

	aw_info("%s enter!\n", __func__);
	i = aw86927->ram.ram_shift;
	aw86927_set_base_addr(aw86927);
	while(i < aw86927_cont->len) {
		if((aw86927_cont->len - i) < AW_RAMDATA_WR_BUFFER_SIZE) 
			len = aw86927_cont->len - i;
		else
			len = AW_RAMDATA_WR_BUFFER_SIZE;
		aw86927_i2c_writes(aw86927, AW86927_REG_RAMDATA,
				   &aw86927_cont->data[i], len);
		i += len;
	}
	return 0;
}

static int aw86927_read_ram_data(struct aw86927 *aw86927,
				 unsigned char *reg_val)
{
	aw_dbg("%s enter!\n", __func__);
	aw86927_i2c_read(aw86927, AW86927_REG_RAMDATA, reg_val);
	return 0;
}

static int aw86927_set_base_addr(struct aw86927 *aw86927)
{
	int ret = -1;

	aw_info("%s enter!\n", __func__);
	if (!aw86927->ram.base_addr) {
		aw_err("%s:aw86927 ram base addr is error\n", __func__);
		return ret;
	}
	/* rtp */
	aw86927_i2c_write_bits(aw86927, AW86927_REG_RTPCFG1, /*ADDRH*/
			AW86927_BIT_RTPCFG1_BASE_ADDR_H_MASK,
			(unsigned char)(aw86927->ram.base_addr >> 8));
	aw86927_i2c_write(aw86927, AW86927_REG_RTPCFG2, /*ADDRL*/
			(unsigned char)(aw86927->ram.base_addr & 0x00ff));
	/* ram */
	aw86927_i2c_write_bits(aw86927, AW86927_REG_RAMADDRH,
			AW86927_BIT_RAMADDRH_MASK,
			(unsigned char)(aw86927->ram.base_addr >> 8));
	aw86927_i2c_write(aw86927, AW86927_REG_RAMADDRL,
			(unsigned char)(aw86927->ram.base_addr & 0x00ff));
	return 0;
}

static void aw86927_haptic_raminit(struct aw86927 *aw86927, bool flag)
{
	aw_info("%s enter!\n", __func__);
	if (flag) {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_SYSCTRL3,
				       AW86927_BIT_SYSCTRL3_EN_RAMINIT_MASK,
				       AW86927_BIT_SYSCTRL3_EN_RAMINIT_ON);
	} else {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_SYSCTRL3,
				       AW86927_BIT_SYSCTRL3_EN_RAMINIT_MASK,
				       AW86927_BIT_SYSCTRL3_EN_RAMINIT_OFF);
	}
}

#ifdef AW_CHECK_RAM_DATA
static int aw86927_check_ram_data(struct aw86927 *aw86927,
				  unsigned char *cont_data,
				  unsigned char *ram_data,
				  unsigned int len)
{
	int i = 0;
	
	for (i = 0; i < len; i++) {
		
		if (ram_data[i]  != cont_data[i]) {
			aw_err("%s: check ramdata error, addr=0x%04x, ram_data=0x%02x, file_data=0x%02x\n",		
				__func__, i, ram_data[i], cont_data[i]);
			return -ERANGE;
		}
	}
	return 0;

}
#endif


static int aw86927_haptic_cont_play(struct aw86927 *aw86927)
{
	aw_info("%s enter\n", __func__);

	/* work mode */
	aw86927_haptic_play_mode(aw86927, AW86927_CONT_MODE);

	aw86927_i2c_write_bits(aw86927, AW86927_REG_CONTCFG6,
			       AW86927_BIT_CONTCFG6_TRACK_EN_MASK,
			       AW86927_BIT_CONTCFG6_TRACK_ENABLE);
	/* f0 driver level */
	aw86927_i2c_write_bits(aw86927, AW86927_REG_CONTCFG6,
			       AW86927_BIT_CONTCFG6_DRV1_LVL_MASK,
			       aw86927->info.cont_drv1_lvl);
	aw86927_i2c_write(aw86927, AW86927_REG_CONTCFG7,
			  aw86927->info.cont_drv2_lvl);
	/* DRV1_TIME */
	/* aw86927_i2c_write(aw86927, AW86927_REG_CONTCFG8, 0xFF); */
	/* DRV2_TIME */
	aw86927_i2c_write(aw86927, AW86927_REG_CONTCFG9, 0xFF);
	/* cont play go */
	aw86927_haptic_play_go(aw86927);
	return 0;
}

void aw86927_interrupt_setup(struct aw86927 *aw86927)
{
	unsigned char reg_val = 0;

	aw_info("%s enter\n", __func__);

	aw86927_i2c_read(aw86927, AW86927_REG_SYSINT, &reg_val);

	aw_info("%s: reg SYSINT=0x%02X\n", __func__, reg_val);
	/* edge mode */
	aw86927_select_edge_int_mode(aw86927);
	/* int enable */
	aw86927_i2c_write_bits(aw86927, AW86927_REG_SYSINTM,
				AW86927_BIT_SYSINTM_BST_SCPM_MASK,
				AW86927_BIT_SYSINTM_BST_SCPM_ON);
	aw86927_i2c_write_bits(aw86927, AW86927_REG_SYSINTM,
			       AW86927_BIT_SYSINTM_BST_OVPM_MASK,
			       AW86927_BIT_SYSINTM_BST_OVPM_OFF);
	aw86927_i2c_write_bits(aw86927, AW86927_REG_SYSINTM,
			       AW86927_BIT_SYSINTM_UVLM_MASK,
			       AW86927_BIT_SYSINTM_UVLM_ON);
	aw86927_i2c_write_bits(aw86927, AW86927_REG_SYSINTM,
			       AW86927_BIT_SYSINTM_OCDM_MASK,
			       AW86927_BIT_SYSINTM_OCDM_ON);
	aw86927_i2c_write_bits(aw86927, AW86927_REG_SYSINTM,
			       AW86927_BIT_SYSINTM_OTM_MASK,
			       AW86927_BIT_SYSINTM_OTM_ON);
}

static void aw86927_interrupt_clear(struct aw86927 *aw86927)
{
	unsigned char reg_val = 0;

	aw_dbg("%s enter\n", __func__);
	aw86927_i2c_read(aw86927, AW86927_REG_SYSINT, &reg_val);
	aw_dbg("%s: reg SYSINT=0x%02X\n", __func__, reg_val);
}

static int aw86927_get_irq_state(struct aw86927 *aw86927)
{
	unsigned char reg_val = 0;
	int ret = 0;

	aw_dbg("%s enter!\n", __func__);
	ret = aw86927_i2c_read(aw86927, AW86927_REG_SYSINT, &reg_val);
	aw_dbg("%s: reg SYSINT=0x%02X\n", __func__, reg_val);
	if (ret < 0)
		return ret;
	if (reg_val & AW86927_BIT_SYSINT_BST_OVPI) {
		aw_err("%s chip ov int error\n", __func__);
		ret = AW86927_SYSINT_ERROR;
	}
	if (reg_val & AW86927_BIT_SYSINT_UVLI) {
		aw_err("%s chip uvlo int error\n", __func__);
		ret = AW86927_SYSINT_ERROR;
	}
	if (reg_val & AW86927_BIT_SYSINT_OCDI) {
		aw_err("%s chip over current int error\n",
			   __func__);
		ret = AW86927_SYSINT_ERROR;
	}
	if (reg_val & AW86927_BIT_SYSINT_OTI) {
		aw_err("%s chip over temperature int error\n",
			   __func__);
		ret = AW86927_SYSINT_ERROR;
	}
	if (reg_val & AW86927_BIT_SYSINT_DONEI) {
		aw_info("%s chip playback done\n", __func__);
		ret = AW86927_SYSINT_ERROR;
	}
	if (reg_val & AW86927_BIT_SYSINT_FF_AEI) {
		aw_info("%s aw86927 rtp fifo almost empty\n", __func__);
		ret |= AW86927_SYSINT_FF_AEI;
	}
	if (reg_val & AW86927_BIT_SYSINT_FF_AFI) {
		aw_info("%s aw86927 rtp fifo almost full\n", __func__);
		ret |= AW86927_SYSINT_FF_AFI;
	}
	return ret;
}

static int aw86927_haptic_read_cont_f0(struct aw86927 *aw86927)
{
	int ret = 0;
	unsigned char reg_val = 0;
	unsigned int f0_reg = 0;
	unsigned long f0_tmp = 0;

	aw_info("%s enter\n", __func__);
	ret = aw86927_i2c_read(aw86927, AW86927_REG_CONTCFG16, &reg_val);
	f0_reg = (f0_reg | reg_val) << 8;
	ret = aw86927_i2c_read(aw86927, AW86927_REG_CONTCFG17, &reg_val);
	f0_reg |= (reg_val << 0);
	if (!f0_reg) {
		aw_err("%s didn't get cont f0 because f0_reg value is 0!\n",
			__func__);
		aw86927->cont_f0 = aw86927->info.f0_pre;
		return ret;
	}
	f0_tmp = 384000 * 10 / f0_reg;
	aw86927->cont_f0 = (unsigned int)f0_tmp;
	aw_info("%s cont_f0=%d\n", __func__,
		    aw86927->cont_f0);
	return ret;
}

static int aw86927_haptic_vbat_mode_config(struct aw86927 *aw86927,
					   unsigned char flag)
{
	aw_info("%s enter!\n", __func__);
	if (flag == AW86927_VBAT_HW_ADJUST_MODE) {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_VBATCTRL,
				       AW86927_BIT_VBATCTRL_VBAT_MODE_MASK,
				       AW86927_BIT_VBATCTRL_VBAT_MODE_HW);
	} else {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_VBATCTRL,
				       AW86927_BIT_VBATCTRL_VBAT_MODE_MASK,
				       AW86927_BIT_VBATCTRL_VBAT_MODE_SW);
	}
	return 0;
}

static int aw86927_haptic_cont_get_f0(struct aw86927 *aw86927)
{
	int ret = 0;
	unsigned char reg_val = 0;
	unsigned char brk_en_default = 0;

	aw_info("%s enter\n", __func__);
	aw86927->f0 = aw86927->info.f0_pre;
	/* enter standby mode */
	aw86927_haptic_stop(aw86927);
	/* f0 calibrate work mode */
	aw86927_haptic_play_mode(aw86927, AW86927_CONT_MODE);
	/* enable f0 detect */
	aw86927_haptic_f0_detect(aw86927, true);
	/* cont config */
	aw86927_i2c_write_bits(aw86927, AW86927_REG_CONTCFG6,
			       AW86927_BIT_CONTCFG6_TRACK_EN_MASK,
			       AW86927_BIT_CONTCFG6_TRACK_ENABLE);
	/* enable auto break */
	aw86927_i2c_read(aw86927, AW86927_REG_PLAYCFG3, &reg_val);
	brk_en_default = 0x04 & reg_val;
	aw86927_haptic_auto_break_mode(aw86927, true);
	aw86927_haptic_vbat_mode_config(aw86927, AW86927_VBAT_HW_ADJUST_MODE);
	/* f0 driver level */
	aw86927_i2c_write_bits(aw86927, AW86927_REG_CONTCFG6,
			       AW86927_BIT_CONTCFG6_DRV1_LVL_MASK,
			       aw86927->info.cont_drv1_lvl);
	aw86927_i2c_write(aw86927, AW86927_REG_CONTCFG7,
			  aw86927->info.cont_drv2_lvl);
	/* DRV1_TIME */
	aw86927_i2c_write(aw86927, AW86927_REG_CONTCFG8,
			  aw86927->info.cont_drv1_time);
	/* DRV2_TIME */
	aw86927_i2c_write(aw86927, AW86927_REG_CONTCFG9,
			  aw86927->info.cont_drv2_time);
	/* TRACK_MARGIN */
	if (!aw86927->info.cont_track_margin) {
		aw_err("%s aw86927->info.cont_track_margin = 0!\n",
			   __func__);
	} else {
		aw86927_i2c_write(aw86927, AW86927_REG_CONTCFG11,
				  (unsigned char)aw86927->
				  info.cont_track_margin);
	}
	/* DRV_WIDTH */
	/*aw86927_i2c_write(aw86927, AW86927_REG_CONTCFG3,
	*		aw86927->info.cont_drv_width);
	*/

	/* cont play go */
	aw86927_haptic_play_go(aw86927);
	/* 300ms */
	ret = aw86927_haptic_wait_enter_standby(aw86927, 1000);
	if (!ret) {
		aw86927_haptic_read_lra_f0(aw86927);
		aw86927_haptic_read_cont_f0(aw86927);
	} else {
		aw_err("%s enter standby mode failed, stop reading f0!\n",
			__func__);
	}
	aw86927_haptic_vbat_mode_config(aw86927, AW86927_VBAT_SW_ADJUST_MODE);
	/* restore default config */
	aw86927_haptic_f0_detect(aw86927, false);
	/* recover auto break config */
	if (brk_en_default)
		aw86927_haptic_auto_break_mode(aw86927, true);
	else
		aw86927_haptic_auto_break_mode(aw86927, false);

	return ret;
}

static void aw86927_haptic_upload_lra(struct aw86927 *aw86927,
				      unsigned int flag)
{
	aw_dbg("%s enter!\n", __func__);
	/* Unlock register */
	aw86927_i2c_write(aw86927, AW86927_REG_TMCFG,
			  AW86927_BIT_TMCFG_TM_UNLOCK);
	switch (flag) {
	case AW86927_WRITE_ZERO:
		aw_info("%s write zero to trim_lra!\n",
			    __func__);
		aw86927_i2c_write(aw86927, AW86927_REG_ANACFG20,0x00);
		break;
	case AW86927_F0_CALI:
		aw_info("%s write f0_calib_data to trim_lra = 0x%02X\n",
			__func__, aw86927->f0_calib_data);
		aw86927_i2c_write(aw86927, AW86927_REG_ANACFG20,
				       (char)aw86927->f0_calib_data);
		break;
	case AW86927_OSC_CALI:
		aw_info("%s write lra_calib_data to trim_lra = 0x%02X\n",
			__func__, aw86927->lra_calib_data);
		aw86927_i2c_write(aw86927, AW86927_REG_ANACFG20,
				       (char)aw86927->lra_calib_data);
		break;
	default:
		break;
	}
	/* Lock register */
	aw86927_i2c_write(aw86927, AW86927_REG_TMCFG,
			  AW86927_BIT_TMCFG_TM_LOCK);
}

static unsigned char aw86927_haptic_osc_read_status(struct aw86927 *aw86927)
{
	unsigned char reg_val = 0;

	aw_dbg("%s enter!\n", __func__);
	aw86927_i2c_read(aw86927, AW86927_REG_SYSST2, &reg_val);
	return reg_val;
}

static unsigned int aw86927_haptic_get_theory_time(struct aw86927 *aw86927)
{
	unsigned char reg_val = 0;
	unsigned int fre_val = 0;
	unsigned int theory_time = 0;

	aw_info("%s enter!\n", __func__);
	aw86927_i2c_read(aw86927, AW86927_REG_SYSCTRL4, &reg_val);
	fre_val = (reg_val & 0x03) >> 5;

	if (fre_val == 2 || fre_val == 3)
		theory_time = (aw86927->rtp_len / 12000) * 1000000;	/*12K */
	if (fre_val == 0)
		theory_time = (aw86927->rtp_len / 24000) * 1000000;	/*24K */
	if (fre_val == 1)
		theory_time = (aw86927->rtp_len / 48000) * 1000000;	/*48K */
	return theory_time;
}

static int aw86927_haptic_get_vbat(struct aw86927 *aw86927)
{
	unsigned char reg_val = 0;
	unsigned int vbat_code = 0;

	aw_info("%s enter!\n", __func__);
	aw86927_haptic_stop(aw86927);
	aw86927_haptic_raminit(aw86927, true);
	aw86927_i2c_write_bits(aw86927, AW86927_REG_DETCFG2,
			       AW86927_BIT_DETCFG2_DET_SEQ0_MASK,
			       AW86927_BIT_DETCFG2_DET_SEQ0_VBAT);
	aw86927_i2c_write_bits(aw86927, AW86927_REG_DETCFG1,
			       AW86927_BIT_DETCFG1_DET_GO_MASK,
			       AW86927_BIT_DETCFG1_DET_GO_DET_SEQ0);
	usleep_range(3000, 3500);
	aw86927_i2c_write_bits(aw86927, AW86927_REG_DETCFG1,
			       AW86927_BIT_DETCFG1_DET_GO_MASK,
			       AW86927_BIT_DETCFG1_DET_GO_NA);

	aw86927_i2c_read(aw86927, AW86927_REG_DETRD1, &reg_val);
	vbat_code = ((reg_val & 0x03) * 256);
	aw86927_i2c_read(aw86927, AW86927_REG_DETRD2, &reg_val);
	vbat_code = vbat_code + reg_val;
	aw86927->vbat = 5 * 1215 * vbat_code / 1024;
	if (aw86927->vbat > AW86927_VBAT_MAX) {
		aw86927->vbat = AW86927_VBAT_MAX;
		aw_info("%s vbat max limit = %dmV\n",
			    __func__, aw86927->vbat);
	}
	if (aw86927->vbat < AW86927_VBAT_MIN) {
		aw86927->vbat = AW86927_VBAT_MIN;
		aw_info("%s vbat min limit = %dmV\n",
			    __func__, aw86927->vbat);
	}
	aw_info("%s aw86927->vbat=%dmV, vbat_code=0x%02X\n",
		    __func__, aw86927->vbat, vbat_code);
	aw86927_haptic_raminit(aw86927, false);
	return 0;
}

static int aw86927_select_d2s_gain(struct aw86927 *aw86927, unsigned char reg)
{
	int d2s_gain = 0;

	switch (reg) {
	case AW86927_BIT_DETCFG2_D2S_GAIN_1:
		d2s_gain = 1;
		break;
	case AW86927_BIT_DETCFG2_D2S_GAIN_2:
		d2s_gain = 2;
		break;
	case AW86927_BIT_DETCFG2_D2S_GAIN_4:
		d2s_gain = 4;
		break;
	case AW86927_BIT_DETCFG2_D2S_GAIN_8:
		d2s_gain = 8;
		break;
	case AW86927_BIT_DETCFG2_D2S_GAIN_10:
		d2s_gain = 10;
		break;
	case AW86927_BIT_DETCFG2_D2S_GAIN_16:
		d2s_gain = 16;
		break;
	case AW86927_BIT_DETCFG2_D2S_GAIN_20:
		d2s_gain = 20;
		break;
	case AW86927_BIT_DETCFG2_D2S_GAIN_40:
		d2s_gain = 40;
		break;
	default:
		d2s_gain = -1;
		break;
	}
	return d2s_gain;
}

static int aw86927_haptic_get_lra_resistance(struct aw86927 *aw86927)
{
	unsigned char reg_val = 0;
	unsigned char adc_fs_default = 0;
	unsigned int lra_code = 0;
	unsigned char d2s_gain = 0;

	aw_info("%s enter!\n", __func__);
	aw86927_haptic_raminit(aw86927, true);
	aw86927_haptic_stop(aw86927);
	aw86927_i2c_write_bits(aw86927, AW86927_REG_DETCFG2,
			       AW86927_BIT_DETCFG2_DET_SEQ0_MASK,
			       AW86927_BIT_DETCFG2_DET_SEQ0_RL);
	aw86927_i2c_read(aw86927,  AW86927_REG_DETCFG1, &reg_val);
	adc_fs_default = reg_val & 0x0C;
	aw86927_i2c_write_bits(aw86927, AW86927_REG_DETCFG1,
			       AW86927_BIT_DETCFG1_ADC_FS_MASK,
			       AW86927_BIT_DETCFG1_ADC_FS_96KHZ);
	aw86927_i2c_write_bits(aw86927, AW86927_REG_DETCFG1,
			       AW86927_BIT_DETCFG1_DET_GO_MASK,
			       AW86927_BIT_DETCFG1_DET_GO_DET_SEQ0);
	usleep_range(3000, 3500);
	aw86927_i2c_write_bits(aw86927, AW86927_REG_DETCFG1,
			       AW86927_BIT_DETCFG1_DET_GO_MASK,
			       AW86927_BIT_DETCFG1_DET_GO_NA);
	/* restore default config*/
	aw86927_haptic_raminit(aw86927, false);
	aw86927_i2c_write_bits(aw86927, AW86927_REG_DETCFG1,
			       AW86927_BIT_DETCFG1_ADC_FS_MASK,
			       adc_fs_default);
	aw86927_i2c_read(aw86927, AW86927_REG_DETCFG2, &reg_val);
	reg_val &= 0x07;
	d2s_gain = aw86927_select_d2s_gain(aw86927, reg_val);
	if (d2s_gain < 0) {
		aw_err("%s d2s_gain is error\n", __func__);
		return -ERANGE;
	}
	aw86927_i2c_read(aw86927, AW86927_REG_DETRD1, &reg_val);
	lra_code = ((reg_val & 0x03) * 256);
	aw86927_i2c_read(aw86927, AW86927_REG_DETRD2, &reg_val);
	lra_code = lra_code + reg_val;
	aw86927->lra = (6075 * 100 * lra_code) / (1024*d2s_gain);
	return 0;
}

void aw86927_vibrate_params_init(struct aw86927 *aw86927)
{
	unsigned char i = 0;
	unsigned char reg_val = 0;

	aw_info("%s enter!\n", __func__);
	aw86927->activate_mode = aw86927->info.mode;
	aw86927->ram_vbat_comp = AW86927_RAM_VBAT_COMP_ENABLE;
	aw86927_i2c_read(aw86927, AW86927_REG_WAVCFG1, &reg_val);
	aw86927->index = reg_val & 0x7F;
	aw86927_i2c_read(aw86927, AW86927_REG_PLAYCFG2, &reg_val);
	aw86927->gain = reg_val & 0xFF;
	aw_info("%s aw86927->gain =0x%02X\n",
		    __func__, aw86927->gain);
	aw86927_i2c_read(aw86927, AW86927_REG_PLAYCFG1, &reg_val);
	if (aw86927->info.bst_vol_default > 0)
		aw86927->vmax = aw86927->info.bst_vol_default;
	else
		aw86927->vmax = reg_val &
				  AW86927_BIT_PLAYCFG1_BST_VOUT_VREFSET;
	for (i = 0; i < AW86927_SEQUENCER_SIZE; i++) {
		aw86927_i2c_read(aw86927, AW86927_REG_WAVCFG1 + i,
				       &reg_val);
		aw86927->seq[i] = reg_val;
	}

}

static enum hrtimer_restart qti_hap_stop_timer(struct hrtimer *timer)
{
	struct aw86927 *aw86927 = container_of(timer, struct aw86927,
					     stop_timer);
	int rc;

	aw_info("%s enter\n", __func__);
	aw86927->play.length_us = 0;
	rc = aw86927_i2c_write(aw86927, AW86927_REG_PLAYCFG4,
			  AW86927_BIT_PLAYCFG4_STOP_ON);
	if (rc < 0)
		aw_err("Stop playing failed, rc=%d\n", rc);

	return HRTIMER_NORESTART;
}

static enum hrtimer_restart qti_hap_disable_timer(struct hrtimer *timer)
{
	struct aw86927 *aw86927 = container_of(timer, struct aw86927,
					     hap_disable_timer);
	int rc;

	aw_info("%s enter\n", __func__);
	rc = aw86927_i2c_write(aw86927, AW86927_REG_PLAYCFG4,
			  AW86927_BIT_PLAYCFG4_STOP_ON);
	if (rc < 0)
		aw_err("Disable haptics module failed, rc=%d\n",
			rc);

	return HRTIMER_NORESTART;
}


static void aw86927_haptic_misc_para_init(struct aw86927 *aw86927)
{
	aw_info("%s enter!\n", __func__);

	aw86927->f0_cali_status = true;
	aw86927->rtp_routine_on = 0;
	aw86927->rtp_num_max = awinic_rtp_name_len;
	hrtimer_init(&aw86927->stop_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	aw86927->stop_timer.function = qti_hap_stop_timer;
	hrtimer_init(&aw86927->hap_disable_timer, CLOCK_MONOTONIC,
		     HRTIMER_MODE_REL);
	aw86927->hap_disable_timer.function = qti_hap_disable_timer;
	/* Unlock register */
	aw86927_i2c_write(aw86927, AW86927_REG_TMCFG,
			  AW86927_BIT_TMCFG_TM_UNLOCK);

	aw86927_i2c_write(aw86927, AW86927_REG_SYSCTRL5,
			  AW86927_BIR_SYSCTRL5_INIT_VAL);

	/* Close boost skip */
	aw86927_i2c_write_bits(aw86927, AW86927_REG_ANACFG12,
			       AW86927_BIT_ANACFG12_BST_SKIP_MASK,
			       AW86927_BIT_ANACFG12_BST_SKIP_SHUTDOWN);

	/* Open adaptive ipeak current limiting */
	aw86927_i2c_write_bits(aw86927, AW86927_REG_ANACFG15,
			       AW86927_BIT_ANACFG15_BST_PEAK_MODE_MASK,
			       AW86927_BIT_ANACFG15_BST_PEAK_BACK);

	aw86927_i2c_write_bits(aw86927, AW86927_REG_ANACFG16,
			       AW86927_BIT_ANACFG16_BST_SRC_MASK,
			       AW86927_BIT_ANACFG16_BST_SRC_3NS);

	aw86927_i2c_write(aw86927, AW86927_REG_PWMCFG1,
			  AW86927_BIT_PWMCFG1_INIT_VAL);


	/* brk_bst_md */
	if (!aw86927->info.brk_bst_md)
		aw_err("%s aw86927->info.brk_bst_md = 0!\n", __func__);
	aw86927_i2c_write_bits(aw86927, AW86927_REG_CONTCFG1,
			       AW86927_BIT_CONTCFG1_BRK_BST_MD_MASK,
			       (aw86927->info.brk_bst_md<< 6));
	/* cont_brk_time */
	if (!aw86927->info.cont_brk_time)
		aw_err("%s aw86927->info.cont_brk_time = 0!\n", __func__);
	aw86927_i2c_write(aw86927, AW86927_REG_CONTCFG10,
			  aw86927->info.cont_brk_time);
	/* cont_tset */
	if (!aw86927->info.cont_tset)
		aw_err("%s aw86927->info.cont_tset = 0!\n", __func__);
	aw86927_i2c_write_bits(aw86927, AW86927_REG_CONTCFG13,
			       AW86927_BIT_CONTCFG13_TSET_MASK,
			       (aw86927->info.cont_tset << 4));
	/* cont_bemf_set */
	if (!aw86927->info.cont_bemf_set)
		aw_err("%s aw86927->info.cont_bemf_set = 0!\n", __func__);
	aw86927_i2c_write_bits(aw86927, AW86927_REG_CONTCFG13,
			       AW86927_BIT_CONTCFG13_BEME_SET_MASK,
			       aw86927->info.cont_bemf_set);
	/* cont_bst_brk_gain */
	if (!aw86927->info.cont_bst_brk_gain)
		aw_err("%s aw86927->info.cont_bst_brk_gain = 0!\n", __func__);
	aw86927_i2c_write_bits(aw86927, AW86927_REG_CONTCFG5,
			       AW86927_BIT_CONTCFG5_BST_BRK_GAIN_MASK,
			       aw86927->info.cont_bst_brk_gain << 4);
	/* cont_brk_gain */
	if (!aw86927->info.cont_brk_gain)
		aw_err("%s aw86927->info.cont_brk_gain = 0!\n", __func__);
	aw86927_i2c_write_bits(aw86927, AW86927_REG_CONTCFG5,
			       AW86927_BIT_CONTCFG5_BRK_GAIN_MASK,
			       aw86927->info.cont_brk_gain);
	/* d2s_gain */
	if (!aw86927->info.d2s_gain)
		aw_err("%s aw86927->info.d2s_gain = 0!\n", __func__);
	aw86927_i2c_write_bits(aw86927, AW86927_REG_DETCFG2,
			       AW86927_BIT_DETCFG2_D2S_GAIN_MASK,
			       aw86927->info.d2s_gain);
	/* Lock register */
	aw86927_i2c_write(aw86927, AW86927_REG_TMCFG,
			  AW86927_BIT_TMCFG_TM_LOCK);
	/* GAIN_BYPASS DISABLE */
	aw86927_i2c_write_bits(aw86927, AW86927_REG_SYSCTRL4,
			  AW86927_BIT_SYSCTRL4_GAIN_BYPASS_MASK,
			  AW86927_BIT_SYSCTRL4_GAIN_BYPASS_DISABLE);
}

static int aw86927_haptic_set_bst_peak_cur(struct aw86927 *aw86927,
					   unsigned char peak_cur)
{
	aw_info("%s enter!\n", __func__);
	/* Unlock register */
	aw86927_i2c_write(aw86927, AW86927_REG_TMCFG,
			  AW86927_BIT_TMCFG_TM_UNLOCK);
	if (peak_cur > AW86927_BSTCFG_PEAKCUR_LIMIT)
		peak_cur = AW86927_BSTCFG_PEAKCUR_LIMIT;

	aw86927_i2c_write_bits(aw86927, AW86927_REG_ANACFG13,
			      AW86927_BIT_ANACFG13_PEAKCUR_MASK, peak_cur);
	/* Lock register */
	aw86927_i2c_write(aw86927, AW86927_REG_TMCFG,
			  AW86927_BIT_TMCFG_TM_LOCK);
	return 0;
}

static int aw86927_trig_config(struct aw86927 *aw86927)
{

	aw86927_haptic_trig1_param_init(aw86927);
	aw86927_haptic_trig1_param_config(aw86927);
	aw86927_haptic_trig2_param_init(aw86927);
	aw86927_haptic_trig3_param_init(aw86927);
	aw86927_haptic_trig2_param_config(aw86927);
	aw86927_haptic_trig3_param_config(aw86927);
	return 0;
}

static void aw86927_haptic_bst_mode_config(struct aw86927 *aw86927,
					   unsigned char mode)
{
	aw_info("%s enter!\n", __func__);
	aw86927->bst_mode = mode;
	switch (mode) {
	case AW86927_BST_MODE:
		aw_info("%s haptic bst mode = bst\n",
			    __func__);
		aw86927_i2c_write_bits(aw86927, AW86927_REG_PLAYCFG1,
				       AW86927_BIT_PLAYCFG1_BST_MODE_MASK,
				       AW86927_BIT_PLAYCFG1_BST_MODE);
		break;
	case AW86927_BST_MODE_BYPASS:
		aw_info("%s haptic bst mode = bypass\n",
			    __func__);
		aw86927_i2c_write_bits(aw86927, AW86927_REG_PLAYCFG1,
				       AW86927_BIT_PLAYCFG1_BST_MODE_MASK,
				       AW86927_BIT_PLAYCFG1_BST_MODE_BYPASS);
		break;
	default:
		aw_err("%s: bst = %d error",
			   __func__, mode);
		break;
	}
}

static int aw86927_haptic_set_bst_vol(struct aw86927 *aw86927,
				      unsigned char bst_vol)
{
	aw_dbg("%s enter!\n", __func__);
	if (bst_vol > AW86927_BIT_PLAYCFG1_BST_VOUT_10P5V)
		bst_vol = AW86927_BIT_PLAYCFG1_BST_VOUT_10P5V;
	if (bst_vol < AW86927_BIT_PLAYCFG1_BST_VOUT_6V)
		bst_vol = AW86927_BIT_PLAYCFG1_BST_VOUT_6V;
	aw86927_i2c_write_bits(aw86927, AW86927_REG_PLAYCFG1,
			AW86927_BIT_PLAYCFG1_BST_VOUT_VREFSET_MASK,
			bst_vol);
	return 0;
}

static int aw86927_haptic_set_pwm(struct aw86927 *aw86927, unsigned char mode)
{
	aw_info("%s enter!\n", __func__);
	switch (mode) {
	case AW86927_PWM_48K:
		aw86927_i2c_write_bits(aw86927, AW86927_REG_SYSCTRL4,
				       AW86927_BIT_SYSCTRL4_WAVDAT_MODE_MASK,
				       AW86927_BIT_SYSCTRL4_WAVDAT_48K);
		break;
	case AW86927_PWM_24K:
		aw86927_i2c_write_bits(aw86927, AW86927_REG_SYSCTRL4,
				       AW86927_BIT_SYSCTRL4_WAVDAT_MODE_MASK,
				       AW86927_BIT_SYSCTRL4_WAVDAT_24K);
		break;
	case AW86927_PWM_12K:
		aw86927_i2c_write_bits(aw86927, AW86927_REG_SYSCTRL4,
				       AW86927_BIT_SYSCTRL4_WAVDAT_MODE_MASK,
				       AW86927_BIT_SYSCTRL4_WAVDAT_12K);
		break;
	default:
		break;
	}
	return 0;
}

static int aw86927_haptic_swicth_motor_protect_config(struct aw86927 *aw86927,
						      unsigned char addr,
						      unsigned char val)
{
	aw_info("%s enter\n", __func__);
	if (addr == 1) {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_PWMCFG2,
				       AW86927_BIT_PWMCFG2_PRCT_MODE_MASK,
				       AW86927_BIT_PWMCFG2_PRCT_MODE_VALID);
		aw86927_i2c_write_bits(aw86927, AW86927_REG_PWMCFG1,
				       AW86927_BIT_PWMCFG1_PRC_EN_MASK,
				       AW86927_BIT_PWMCFG1_PRC_ENABLE);
		aw86927_i2c_write_bits(aw86927, AW86927_REG_PWMCFG3,
				       AW86927_BIT_PWMCFG3_PR_EN_MASK,
				       AW86927_BIT_PWMCFG3_PR_ENABLE);
	} else if (addr == 0) {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_PWMCFG2,
				       AW86927_BIT_PWMCFG2_PRCT_MODE_MASK,
				       AW86927_BIT_PWMCFG2_PRCT_MODE_INVALID);
		aw86927_i2c_write_bits(aw86927, AW86927_REG_PWMCFG1,
				       AW86927_BIT_PWMCFG1_PRC_EN_MASK,
				       AW86927_BIT_PWMCFG1_PRC_DISABLE);
		aw86927_i2c_write_bits(aw86927, AW86927_REG_PWMCFG3,
				       AW86927_BIT_PWMCFG3_PR_EN_MASK,
				       AW86927_BIT_PWMCFG3_PR_DISABLE);
	} else if (addr == AW86927_REG_PWMCFG1) {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_PWMCFG1,
				       AW86927_BIT_PWMCFG1_PRCTIME_MASK, val);
	} else if (addr == AW86927_REG_PWMCFG3) {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_PWMCFG3,
				       AW86927_BIT_PWMCFG3_PRLVL_MASK, val);
	} else if (addr == AW86927_REG_PWMCFG4) {
		aw86927_i2c_write(aw86927, AW86927_REG_PWMCFG4, val);
	}
	return 0;
}

static int aw86927_get_prctmode(struct aw86927 *aw86927)
{
	unsigned char reg_val = 0;
	int prctmode = 0;

	aw_info("%s enter!\n", __func__);
	aw86927_i2c_read(aw86927, AW86927_REG_PWMCFG2, &reg_val);
	prctmode = (int)(reg_val & 0x08);

	return prctmode;
}

static int aw86927_haptic_auto_bst_enable(struct aw86927 *aw86927,
					  unsigned char flag)
{

	aw86927->auto_boost = flag;

	aw_info("%s enter\n", __func__);
	if (flag) {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_PLAYCFG3,
				       AW86927_BIT_PLAYCFG3_AUTO_BST_MASK,
				       AW86927_BIT_PLAYCFG3_AUTO_BST_ENABLE);
	} else {
		aw86927_i2c_write_bits(aw86927, AW86927_REG_PLAYCFG3,
				       AW86927_BIT_PLAYCFG3_AUTO_BST_MASK,
				       AW86927_BIT_PLAYCFG3_AUTO_BST_DISABLE);
	}
	return 0;
}

static void aw86927_haptic_set_gain(struct aw86927 *aw86927, unsigned char gain)
{
	unsigned char comp_gain = 0;
	int ret = -EINVAL;
	char type[] = "charger_therm0";
	struct thermal_zone_device *tzd;
	unsigned char tep = 0;
	int temp = 0;

	aw_dbg("%s enter!\n", __func__);
	tzd = thermal_zone_get_zone_by_name(type);
	ret = thermal_zone_get_temp(tzd, &temp);
	aw_info("The temperature:%d,return value:%d\n",temp,ret);
	if (aw86927->ram_vbat_comp == AW86927_RAM_VBAT_COMP_ENABLE){
		aw86927_haptic_get_vbat(aw86927);
		aw_dbg("%s: aw86927->vbat(%dmV) VBAT_REFER(%dmv)\n",
				__func__, aw86927->vbat, AW86927_VBAT_REFER);
		comp_gain = gain * AW86927_VBAT_REFER / aw86927->vbat;
		if (comp_gain >
		    (128 * AW86927_VBAT_REFER / AW86927_VBAT_MIN)) {
			comp_gain = 128 * AW86927_VBAT_REFER / AW86927_VBAT_MIN;
			aw_dbg("%s comp gain limit is %d\n", __func__,
				comp_gain);
		}
		aw_info("%s: enable vbat comp, level = %x comp level = %x", __func__,
			   gain, comp_gain);
		if (aw86927->effect_id == 10 && ret == 0 && temp < 0){
			comp_gain = 2*(int)comp_gain/3;
			aw_info("The comp_gain is:%d\n",comp_gain);
			aw86927_i2c_write(aw86927, AW86927_REG_PLAYCFG2,(unsigned char)(comp_gain));
			aw86927_i2c_read(aw86927,AW86927_REG_PLAYCFG2,&tep);
			aw_info("The AW86927_REG_PLAYCFG2 is:%d\n",tep);
		}else{
			aw86927_i2c_write(aw86927, AW86927_REG_PLAYCFG2,comp_gain);
		}
	}else{
		aw_dbg("%s: disable compsensation, vbat=%d, vbat_min=%d, vbat_ref=%d",
				__func__, aw86927->vbat, AW86927_VBAT_MIN,AW86927_VBAT_REFER);
		aw86927_i2c_write(aw86927, AW86927_REG_PLAYCFG2, gain);
	}
}

static int aw86927_haptic_set_wav_seq(struct aw86927 *aw86927,
				      unsigned char wav, unsigned char seq)
{
	aw_info("%s enter!\n", __func__);
	aw86927_i2c_write(aw86927, AW86927_REG_WAVCFG1 + wav, seq);
	return 0;
}

static int aw86927_haptic_get_wav_seq(struct aw86927 *aw86927,
				      unsigned char wav, unsigned char *seq)
{
	aw_info("%s enter!\n", __func__);
	aw86927_i2c_read(aw86927, AW86927_REG_WAVCFG1 + wav, seq);
	return 0;
}

static int aw86927_haptic_set_wav_loop(struct aw86927 *aw86927,
					unsigned char wav, unsigned char loop)
{
	unsigned char tmp = 0;

	aw_info("%s enter!\n", __func__);
	if (wav % 2) {
		tmp = loop << 0;
		aw86927_i2c_write_bits(aw86927, AW86927_REG_WAVCFG9 + (wav / 2),
				       AW86927_BIT_WAVLOOP_SEQ_EVEN_MASK, tmp);
	} else {
		tmp = loop << 4;
		aw86927_i2c_write_bits(aw86927, AW86927_REG_WAVCFG9 + (wav / 2),
				       AW86927_BIT_WAVLOOP_SEQ_ODD_MASK, tmp);
	}
	return 0;
}

static int aw86927_haptic_get_wav_loop(struct aw86927 *aw86927,
					unsigned char wav, unsigned char *loop)
{
	unsigned char tmp = 0;

	aw_info("%s enter!\n", __func__);
	if (wav % 2) {
		aw86927_i2c_read(aw86927, AW86927_REG_WAVCFG9 + (wav / 2),
				 &tmp);
		*loop = (tmp >> 0) & 0x0F;
	} else {
		aw86927_i2c_read(aw86927, AW86927_REG_WAVCFG9 + (wav / 2),
				 &tmp);
		*loop = (tmp >> 4) & 0x0F;
	}

	return 0;
}

static int aw86927_haptic_get_glb_state(struct aw86927 *aw86927,
					unsigned char *state)
{
	aw_dbg("%s enter!\n", __func__);
	aw86927_i2c_read(aw86927, AW86927_REG_GLBRD5, state);
	return 0;
}

static void aw86927_haptic_play_go(struct aw86927 *aw86927)
{
	aw_dbg("%s enter!\n", __func__);
	aw86927_i2c_write(aw86927, AW86927_REG_PLAYCFG4,
				AW86927_BIT_PLAYCFG4_GO_ON);
}

static void aw86927_haptic_set_repeat_wav_seq(struct aw86927 *aw86927,
					      unsigned char seq)
{
	aw_info("%s enter!\n", __func__);
	aw86927_haptic_set_wav_seq(aw86927, 0x00, seq);
	aw86927_haptic_set_wav_loop(aw86927, 0x00,
				    AW86927_BIT_WAVLOOP_INIFINITELY);
}

static int aw86927_haptic_play_mode(struct aw86927 *aw86927,
				    unsigned char play_mode)
{
	aw_dbg("%s enter!\n", __func__);

	switch (play_mode) {
	case AW86927_STANDBY_MODE:
		aw_info("%s: enter standby mode\n", __func__);
		aw86927->play_mode = AW86927_STANDBY_MODE;
		aw86927_haptic_stop(aw86927);
		break;
	case AW86927_RAM_MODE:
		aw_info("%s: enter ram mode\n", __func__);
		aw86927->play_mode = AW86927_RAM_MODE;
		aw86927_i2c_write_bits(aw86927, AW86927_REG_PLAYCFG3,
				       AW86927_BIT_PLAYCFG3_PLAY_MODE_MASK,
				       AW86927_BIT_PLAYCFG3_PLAY_MODE_RAM);
		/* bst mode */
		aw86927_haptic_bst_mode_config(aw86927,
					      AW86927_BST_MODE);
		break;
	case AW86927_RAM_LOOP_MODE:
		aw_info("%s: enter ram loop mode\n",
			    __func__);
		aw86927->play_mode = AW86927_RAM_LOOP_MODE;
		aw86927_i2c_write_bits(aw86927, AW86927_REG_PLAYCFG3,
				       AW86927_BIT_PLAYCFG3_PLAY_MODE_MASK,
				       AW86927_BIT_PLAYCFG3_PLAY_MODE_RAM);
		/* bst mode */
		aw86927_haptic_bst_mode_config(aw86927,
					      AW86927_BST_MODE_BYPASS);
		break;
	case AW86927_RTP_MODE:
		aw_info("%s: enter rtp mode\n", __func__);
		aw86927->play_mode = AW86927_RTP_MODE;
		aw86927_i2c_write_bits(aw86927, AW86927_REG_PLAYCFG3,
				       AW86927_BIT_PLAYCFG3_PLAY_MODE_MASK,
				       AW86927_BIT_PLAYCFG3_PLAY_MODE_RTP);
		/* bst mode */
		aw86927_haptic_bst_mode_config(aw86927,
					       AW86927_BST_MODE);
		break;
	case AW86927_TRIG_MODE:
		aw_info("%s: enter trig mode\n", __func__);
		aw86927->play_mode = AW86927_TRIG_MODE;
		aw86927_i2c_write_bits(aw86927, AW86927_REG_PLAYCFG3,
				       AW86927_BIT_PLAYCFG3_PLAY_MODE_MASK,
				       AW86927_BIT_PLAYCFG3_PLAY_MODE_RAM);
		break;
	case AW86927_CONT_MODE:
		aw_info("%s: enter cont mode\n", __func__);
		aw86927->play_mode = AW86927_CONT_MODE;
		aw86927_i2c_write_bits(aw86927, AW86927_REG_PLAYCFG3,
				       AW86927_BIT_PLAYCFG3_PLAY_MODE_MASK,
				       AW86927_BIT_PLAYCFG3_PLAY_MODE_CONT);
		/* bst mode */
		aw86927_haptic_bst_mode_config(aw86927,
					       AW86927_BST_MODE_BYPASS);
		break;
	default:
		aw_err("%s: play mode %d error",
			   __func__, play_mode);
		break;
	}
	return 0;
}

static int aw86927_haptic_stop(struct aw86927 *aw86927)
{
	int ret = 0;

	aw_info("%s enter\n", __func__);
	aw86927->play_mode = AW86927_STANDBY_MODE;

	aw86927_i2c_write(aw86927, AW86927_REG_PLAYCFG4,
			  AW86927_BIT_PLAYCFG4_STOP_ON);
	ret = aw86927_haptic_wait_enter_standby(aw86927, 40);

	if (ret < 0) {
		aw_err("%s force to enter standby mode!\n",
			   __func__);
		aw86927_force_enter_standby(aw86927);
	}
	return 0;
}

static int aw86927_haptic_get_ram_number(struct aw86927 *aw86927)
{
	unsigned char i = 0;
	unsigned char reg_val = 0;
	unsigned char ram_data[3];
	unsigned int first_wave_addr = 0;

	aw_info("%s enter!\n", __func__);
	if (!aw86927->ram_init) {
		aw_err("%s: ram init faild, ram_num = 0!\n", __func__);
		return -EPERM;
	}

	mutex_lock(&aw86927->lock);
	/* RAMINIT Enable */
	aw86927_haptic_raminit(aw86927, true);
	aw86927_haptic_stop(aw86927);
	aw86927_set_base_addr(aw86927);
	for (i = 0; i < 3; i++) {
		aw86927_read_ram_data(aw86927, &reg_val);
		ram_data[i] = reg_val;
	}
	first_wave_addr = (ram_data[1] << 8 | ram_data[2]);
	aw86927->ram.ram_num =
			(first_wave_addr - aw86927->ram.base_addr - 1) / 4;
	aw_info("%s: ram_version = 0x%02x\n", __func__, ram_data[0]);
	aw_info("%s: first waveform addr = 0x%04x\n", __func__,
		first_wave_addr);
	aw_info("%s: ram_num = %d\n", __func__, aw86927->ram.ram_num);
	/* RAMINIT Disable */
	aw86927_haptic_raminit(aw86927, false);
	mutex_unlock(&aw86927->lock);

	return 0;
}

static int aw86927_haptic_get_rtp_data(struct aw86927 *aw86927)
{
	const struct firmware *rtp_file;
	int ret = 0;

	aw_info("%s enter!\n", __func__);
	ret = request_firmware(&rtp_file, awinic_rtp_name[0], aw86927->dev);
	if (ret < 0) {
		aw_err("%s: failed to read %s\n", __func__,
			   awinic_rtp_name[0]);
		return ret;
	}

	aw86927_haptic_stop(aw86927);

	mutex_lock(&aw86927->rtp_lock);
	vfree(aw86927_rtp);
	aw86927_rtp = vmalloc(rtp_file->size + sizeof(int));
	if (!aw86927_rtp) {
		release_firmware(rtp_file);
		mutex_unlock(&aw86927->rtp_lock);
		aw_err("%s: error allocating memory\n", __func__);
		return -ERANGE;
	}
	aw86927_rtp->len = rtp_file->size;
	aw86927->rtp_len = rtp_file->size;
	aw_info("%s: rtp file:[%s] size = %dbytes\n",
		    __func__, awinic_rtp_name[0], aw86927_rtp->len);

	memcpy(aw86927_rtp->data, rtp_file->data, rtp_file->size);
	release_firmware(rtp_file);
	mutex_unlock(&aw86927->rtp_lock);
	return 0;
}

static int aw86927_rtp_osc_calibration(struct aw86927 *aw86927)
{
	int ret = -1;
	unsigned char osc_int_state = 0;
	unsigned int buf_len = 0;

	aw86927->rtp_cnt = 0;
	aw86927->timeval_flags = 1;

	aw_info("%s enter\n", __func__);
	ret = aw86927_haptic_get_rtp_data(aw86927);
	if (ret < 0)
		return ret;
	/* gain */
	aw86927_haptic_ram_vbat_comp(aw86927, false);
	/* rtp mode config */
	aw86927_haptic_play_mode(aw86927, AW86927_RTP_MODE);
	/* bst mode */
	aw86927_haptic_bst_mode_config(aw86927, AW86927_BST_MODE_BYPASS);
	disable_irq(gpio_to_irq(aw86927->irq_gpio));

	/* haptic go */
	aw86927_haptic_play_go(aw86927);
	/* require latency of CPU & DMA not more then PM_QOS_VALUE_VB us */
	while (1) {
		if (!aw86927_haptic_rtp_get_fifo_afs(aw86927)) {
			mutex_lock(&aw86927->rtp_lock);
			if ((aw86927_rtp->len - aw86927->rtp_cnt) <
			    (aw86927->ram.base_addr >> 2))
				buf_len = aw86927_rtp->len - aw86927->rtp_cnt;
			else
				buf_len = (aw86927->ram.base_addr >> 2);
			if (aw86927->rtp_cnt != aw86927_rtp->len) {
				if (aw86927->timeval_flags == 1) {

					aw86927->kstart = ktime_get();
					aw86927->timeval_flags = 0;
				}
				aw86927_write_rtp_data(aw86927,
						       &aw86927_rtp->data
						       [aw86927->rtp_cnt],
						       buf_len);
				aw86927->rtp_cnt += buf_len;
			}
			mutex_unlock(&aw86927->rtp_lock);
		}
		osc_int_state = aw86927_haptic_osc_read_status(aw86927);
		if (osc_int_state & AW86927_BIT_SYSINT_DONEI) {

			aw86927->kend = ktime_get();
			aw_info("%s osc trim playback done aw86927->rtp_cnt= %d\n",
				__func__, aw86927->rtp_cnt);
			break;
		}

		aw86927->kend = ktime_get();
		aw86927->microsecond = ktime_to_us(ktime_sub(aw86927->kend,
							     aw86927->kstart));
		if (aw86927->microsecond > OSC_CALIBRATION_T_LENGTH) {
			aw_info("%s osc trim time out! aw86927->rtp_cnt %d osc_int_state %02x\n",
				__func__, aw86927->rtp_cnt, osc_int_state);
			break;
		}
	}

	enable_irq(gpio_to_irq(aw86927->irq_gpio));

	aw86927->microsecond = ktime_to_us(ktime_sub(aw86927->kend,
						aw86927->kstart));
	/*calibration osc */
	aw_info("%s aw86927_microsecond: %ld\n", __func__,
		aw86927->microsecond);
	aw_info("%s exit\n", __func__);
	return 0;
}

static int aw86927_osc_trim_calculation(struct aw86927 *aw86927,
					unsigned long int theory_time,
					unsigned long int real_time)
{
	unsigned int real_code = 0;
	unsigned int lra_code = 0;
	unsigned int DFT_LRA_TRIM_CODE = 0;
	/*0.1 percent below no need to calibrate */
	unsigned int osc_cali_threshold = 10;

	aw_info("%s enter\n", __func__);
	if (theory_time == real_time) {
		aw_info("%s theory_time == real_time: %ld, no need to calibrate!\n",
			__func__, real_time);
		return 0;
	} else if (theory_time < real_time) {
		if ((real_time - theory_time) > (theory_time / 50)) {
			aw_info("%s (real_time - theory_time) > (theory_time/50), can't calibrate!\n",
				__func__);
			return DFT_LRA_TRIM_CODE;
		}

		if ((real_time - theory_time) <
		    (osc_cali_threshold * theory_time / 10000)) {
			aw_info("%s real_time: %ld, theory_time: %ld, no need to calibrate!\n",
				__func__, real_time, theory_time);
			return DFT_LRA_TRIM_CODE;
		}

		real_code = ((real_time - theory_time) * 4000) / theory_time;
		real_code = ((real_code % 10 < 5) ? 0 : 1) + real_code / 10;
		real_code = 32 + real_code;
	} else if (theory_time > real_time) {
		if ((theory_time - real_time) > (theory_time / 50)) {
			aw_info("%s (theory_time - real_time) > (theory_time / 50), can't calibrate!\n",
				__func__);
			return DFT_LRA_TRIM_CODE;
		}
		if ((theory_time - real_time) <
		    (osc_cali_threshold * theory_time / 10000)) {
			aw_info("%s real_time: %ld, theory_time: %ld, no need to calibrate!\n",
				__func__, real_time, theory_time);
			return DFT_LRA_TRIM_CODE;
		}
		real_code = (theory_time - real_time) /
					(theory_time / 100000) / 24;
		real_code = ((real_code % 10 < 5) ? 0 : 1) + real_code / 10;
		real_code = 32 - real_code;
	}
	if (real_code > 31)
		lra_code = real_code - 32;
	else
		lra_code = real_code + 32;
	aw_info("%s real_time: %ld, theory_time: %ld\n",
		__func__, real_time, theory_time);
	aw_info("%s real_code: %02X, trim_lra: 0x%02X\n",
		    __func__, real_code, lra_code);
	return lra_code;
}

static void aw86927_calculate_cali_step(struct aw86927 *aw86927)
{
	char f0_cali_lra = 0;
	int f0_cali_step = 0;
	unsigned char reg_val = 0;

	aw_info("%s enter!\n", __func__);
	f0_cali_step = 100000 * ((int)aw86927->f0 -
				 (int)aw86927->info.f0_pre) /
				 ((int)aw86927->f0 * 24);
	aw_info("%s f0_cali_step = %d\n", __func__,
		    f0_cali_step);

	if (f0_cali_step >= 0) { /*f0_cali_step >= 0 */
		if (f0_cali_step % 10 >= 5)
			f0_cali_step = 32 + (f0_cali_step / 10 + 1);
		else
			f0_cali_step = 32 + f0_cali_step / 10;
	} else { /* f0_cali_step < 0 */
		if (f0_cali_step % 10 <= -5)
			f0_cali_step = 32 + (f0_cali_step / 10 - 1);
		else
			f0_cali_step = 32 + f0_cali_step / 10;
	}
	if (f0_cali_step > 31)
		f0_cali_lra = (char)f0_cali_step - 32;
	else
		f0_cali_lra = (char)f0_cali_step + 32;

	aw86927->f0_calib_data = (int)f0_cali_lra;
	aw_info("%s origin trim_lra = 0x%02X, f0_cali_lra = 0x%02X, final f0_calib_data = 0x%02X\n",
		__func__, (reg_val & 0x3f), f0_cali_lra,
		aw86927->f0_calib_data);

}

static int aw86927_rtp_trim_lra_calibration(struct aw86927 *aw86927)
{
	unsigned int theory_time = 0;
	unsigned int lra_trim_code = 0;

	aw_info("%s enter!\n", __func__);
	theory_time = aw86927_haptic_get_theory_time(aw86927);

	aw_info("%s microsecond:%ld  theory_time = %d\n",
		    __func__, aw86927->microsecond, theory_time);

	lra_trim_code = aw86927_osc_trim_calculation(aw86927, theory_time,
						     aw86927->microsecond);
	if (lra_trim_code >= 0) {
		aw86927->lra_calib_data = lra_trim_code;
		aw86927_haptic_upload_lra(aw86927, AW86927_OSC_CALI);
	}
	return 0;
}

static int aw86927_haptic_is_within_cali_range(struct aw86927 *aw86927)
{
	unsigned int f0_cali_min = 0;
	unsigned int f0_cali_max = 0;
	int ret = 0;

	aw_info("%s enter!\n", __func__);
	f0_cali_min = aw86927->info.f0_pre *
				(100 - aw86927->info.f0_cali_percen) / 100;
	f0_cali_max = aw86927->info.f0_pre *
				(100 + aw86927->info.f0_cali_percen) / 100;

	aw_info("%s f0_pre = %d, f0_cali_min = %d, f0_cali_max = %d, f0 = %d\n",
		__func__, aw86927->info.f0_pre,
		f0_cali_min, f0_cali_max, aw86927->f0);

	if ((aw86927->f0 < f0_cali_min) || aw86927->f0 > f0_cali_max) {
		aw_err("%s f0 calibration out of range = %d!\n",
			__func__, aw86927->f0);
		ret = -1;
	}
	return ret;
}

static int aw86927_haptic_f0_calibration(struct aw86927 *aw86927)
{
	int ret = 0;

	aw_info("%s enter\n", __func__);

	if (aw86927_haptic_cont_get_f0(aw86927)) {
		aw_err("%s get f0 error, user defafult f0\n", __func__);
	} else {
		/* max and min limit */
		ret = aw86927_haptic_is_within_cali_range(aw86927);
		if (ret < 0)
			return -ERANGE;
		/* calculate cali step */
		aw86927_calculate_cali_step(aw86927);

	}
	aw86927_haptic_upload_lra(aw86927, AW86927_F0_CALI);
	/* restore standby work mode */
	aw86927_haptic_stop(aw86927);
	return ret;
}

/******************************************************
 *
 * sysfs attr
 *
 ******************************************************/
static ssize_t aw86927_effect_id_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;

	return snprintf(buf, PAGE_SIZE, "effect_id =%d\n", aw86927->effect_id);
}

static ssize_t aw86927_effect_id_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	mutex_lock(&aw86927->lock);
	aw86927->effect_id = val;
	aw86927->play.vmax_mv = AW86927_MEDIUM_MAGNITUDE;
	mutex_unlock(&aw86927->lock);
	return count;
}

static ssize_t aw86927_f0_check_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	ssize_t len = 0;

	if (aw86927->f0_cali_status == true)
		len += snprintf(buf + len, PAGE_SIZE - len, "%d\n", 1);
	if (aw86927->f0_cali_status == false)
		len += snprintf(buf + len, PAGE_SIZE - len, "%d\n", 0);

	return len;
}

static ssize_t aw86927_f0_value_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;

	return snprintf(buf, PAGE_SIZE, "%d\n", aw86927->f0);
}

/* return buffer size and availbe size */
static ssize_t aw86927_custom_wave_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	ssize_t len = 0;

	len +=
		snprintf(buf + len, PAGE_SIZE - len, "period_size=%d;",
		aw86927->ram.base_addr >> 2);
	len +=
		snprintf(buf + len, PAGE_SIZE - len,
		"max_size=%d;free_size=%d;",
		get_rb_max_size(), get_rb_free_size());
	len +=
		snprintf(buf + len, PAGE_SIZE - len,
		"custom_wave_id=%d;", CUSTOME_WAVE_ID);
	return len;
}

static ssize_t aw86927_custom_wave_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	unsigned long  buf_len, period_size, offset;
	int ret;

	period_size = (aw86927->ram.base_addr >> 2);
	offset = 0;

	aw_dbg("%swrite szie %zd, period size %lu", __func__, count,
		 period_size);
	if (count % period_size || count < period_size)
		rb_end();
	atomic_set(&aw86927->is_in_write_loop, 1);

	while (count > 0) {
		buf_len = MIN(count, period_size);
		ret = write_rb(buf + offset,  buf_len);
		if (ret < 0)
			goto exit;
		count -= buf_len;
		offset += buf_len;
	}
	ret = offset;
exit:
	atomic_set(&aw86927->is_in_write_loop, 0);
	wake_up_interruptible(&aw86927->stop_wait_q);
	aw_dbg(" return size %d", ret);
	return ret;
}

static ssize_t aw86927_bst_vol_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len,
			"bst_vol_ram=%d, bst_vol_rtp=%d\n",
			aw86927->info.bst_vol_ram, aw86927->info.bst_vol_rtp);
	return len;
}

static ssize_t aw86927_bst_vol_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	unsigned int databuf[2] = { 0, 0 };

	if (sscanf(buf, "%d %d", &databuf[0], &databuf[1]) == 2) {
		if (databuf[0] < AW86927_BIT_PLAYCFG1_BST_VOUT_6V)
			databuf[0] = AW86927_BIT_PLAYCFG1_BST_VOUT_6V;
		if (databuf[1] < AW86927_BIT_PLAYCFG1_BST_VOUT_6V)
			databuf[1] = AW86927_BIT_PLAYCFG1_BST_VOUT_6V;
		if (databuf[0] > AW86927_BIT_PLAYCFG1_BST_VOUT_10P5V)
			databuf[0] = AW86927_BIT_PLAYCFG1_BST_VOUT_10P5V;
		if (databuf[1] > AW86927_BIT_PLAYCFG1_BST_VOUT_10P5V)
			databuf[1] = AW86927_BIT_PLAYCFG1_BST_VOUT_10P5V;
		aw86927->info.bst_vol_ram = databuf[0];
		aw86927->info.bst_vol_rtp = databuf[1];
	}
	return count;
}

static ssize_t aw86927_cont_wait_num_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len,
			"cont_wait_num = 0x%02X\n",
			aw86927->info.cont_wait_num);
	return len;
}

static ssize_t aw86927_cont_wait_num_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	int rc = 0;
	unsigned int val = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	aw86927->info.cont_wait_num = val;
	aw86927_set_cont_wait_num(aw86927, val);

	return count;
}

static ssize_t aw86927_cont_drv_lvl_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len,
			"cont_drv1_lvl = 0x%02X, cont_drv2_lvl = 0x%02X\n",
			aw86927->info.cont_drv1_lvl,
			aw86927->info.cont_drv2_lvl);
	return len;
}

static ssize_t aw86927_cont_drv_lvl_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	unsigned int databuf[2] = { 0, 0 };

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		aw86927->info.cont_drv1_lvl = databuf[0];
		aw86927->info.cont_drv2_lvl = databuf[1];
		aw86927_set_cont_drv_lvl(aw86927, databuf[0], databuf[1]);
	}
	return count;
}

static ssize_t aw86927_cont_drv_time_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len,
			"cont_drv1_time = 0x%02X, cont_drv2_time = 0x%02X\n",
			aw86927->info.cont_drv1_time,
			aw86927->info.cont_drv2_time);
	return len;
}

static ssize_t aw86927_cont_drv_time_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	unsigned int databuf[2] = { 0, 0 };

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		aw86927->info.cont_drv1_time = databuf[0];
		aw86927->info.cont_drv2_time = databuf[1];
		aw86927_set_cont_drv_time(aw86927, databuf[0], databuf[1]);
	}
	return count;
}

static ssize_t aw86927_cont_brk_time_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "cont_brk_time = 0x%02X\n",
			aw86927->info.cont_brk_time);
	return len;
}

static ssize_t aw86927_cont_brk_time_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	int rc = 0;
	unsigned int val = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	aw86927->info.cont_brk_time = val;
	aw86927_set_cont_brk_time(aw86927, val);
	return count;
}


static ssize_t aw86927_trig_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	ssize_t len = 0;
	unsigned char i = 0;

	for (i = 0; i < AW86927_TRIG_NUM; i++) {
		len += snprintf(buf + len, PAGE_SIZE - len,
				"trig%d: trig_level=%d, trig_polar=%d, pos_enable=%d, pos_sequence=%d, neg_enable=%d, neg_sequence=%d trig_brk=%d, trig_bst=%d\n",
				i + 1,
				aw86927->trig[i].trig_level,
				aw86927->trig[i].trig_polar,
				aw86927->trig[i].pos_enable,
				aw86927->trig[i].pos_sequence,
				aw86927->trig[i].neg_enable,
				aw86927->trig[i].neg_sequence,
				aw86927->trig[i].trig_brk,
				aw86927->trig[i].trig_bst);
	}

	return len;
}

static ssize_t aw86927_trig_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	unsigned int databuf[9] = { 0 };

	if (sscanf(buf, "%d %d %d %d %d %d %d %d %d", &databuf[0], &databuf[1],
		   &databuf[2], &databuf[3], &databuf[4], &databuf[5],
		   &databuf[6], &databuf[7], &databuf[8]) == 9) {
		aw_info("%s: %d, %d, %d, %d, %d, %d, %d, %d, %d\n",
			__func__, databuf[0], databuf[1], databuf[2],
			databuf[3], databuf[4], databuf[5], databuf[6],
			databuf[7], databuf[8]);
		if (databuf[0] < 1 || databuf[0] > 3) {
			aw_info("%s: input trig_num out of range!\n",
				    __func__);
			return count;
		}
		if (!aw86927->ram_init) {
			aw_err("%s: ram init failed, not allow to play!\n",
				__func__);
			return count;
		}
		if (databuf[4] > aw86927->ram.ram_num ||
		    databuf[6] > aw86927->ram.ram_num) {
			aw_err("%s: input seq value out of range!\n",
				__func__);
			return count;
		}
		databuf[0] -= 1;

		aw86927->trig[databuf[0]].trig_level = databuf[1];
		aw86927->trig[databuf[0]].trig_polar = databuf[2];
		aw86927->trig[databuf[0]].pos_enable = databuf[3];
		aw86927->trig[databuf[0]].pos_sequence = databuf[4];
		aw86927->trig[databuf[0]].neg_enable = databuf[5];
		aw86927->trig[databuf[0]].neg_sequence = databuf[6];
		aw86927->trig[databuf[0]].trig_brk = databuf[7];
		aw86927->trig[databuf[0]].trig_bst = databuf[8];
		mutex_lock(&aw86927->lock);
		switch (databuf[0]) {
		case 0:
			aw86927_haptic_trig1_param_config(aw86927);
			break;
		case 1:
			aw86927_haptic_trig2_param_config(aw86927);
			break;
		case 2:
			aw86927_haptic_trig3_param_config(aw86927);
			break;
		}
		mutex_unlock(&aw86927->lock);
	}
	return count;
}

static ssize_t aw86927_state_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;

	return snprintf(buf, PAGE_SIZE, "state = %d\n", aw86927->state);
}

static ssize_t aw86927_state_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	return count;
}

static ssize_t aw86927_duration_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	ktime_t time_rem;
	s64 time_ms = 0;

	if (hrtimer_active(&aw86927->timer)) {
		time_rem = hrtimer_get_remaining(&aw86927->timer);
		time_ms = ktime_to_ms(time_rem);
	}
	return snprintf(buf, PAGE_SIZE, "%lld\n", time_ms);
}

static ssize_t aw86927_duration_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	/* setting 0 on duration is NOP for now */
	if (val <= 0)
		return count;
	aw86927->duration = val;
	return count;
}

static ssize_t aw86927_activate_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;

	/* For now nothing to show */
	return snprintf(buf, PAGE_SIZE, "%d\n", aw86927->state);
}

static ssize_t aw86927_activate_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	aw_info("%s: value = %d\n", __func__, val);
	if (!aw86927->ram_init) {
		aw_err("%s: ram init failed, not allow to play!\n", __func__);
		return count;
	}
	mutex_lock(&aw86927->lock);
	hrtimer_cancel(&aw86927->timer);
	aw86927->state = val;
	mutex_unlock(&aw86927->lock);
	queue_work(aw86927->work_queue, &aw86927->vibrator_work);
	return count;
}


static ssize_t aw86927_activate_mode_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;

	return snprintf(buf, PAGE_SIZE, "activate_mode = %d\n",
			aw86927->activate_mode);
}

static ssize_t aw86927_activate_mode_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	mutex_lock(&aw86927->lock);
	aw86927->activate_mode = val;
	mutex_unlock(&aw86927->lock);
	return count;
}

static ssize_t aw86927_index_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	unsigned char seq = 0;

	aw86927_haptic_get_wav_seq(aw86927, 0x00, &seq);
	aw86927->index = seq;
	return snprintf(buf, PAGE_SIZE, "index = %d\n", aw86927->index);
}

static ssize_t aw86927_index_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	if (val > aw86927->ram.ram_num) {
		aw_err("%s: input value out of range!\n", __func__);
		return count;
	}
	aw_info("%s: value=%d\n", __func__, val);
	mutex_lock(&aw86927->lock);
	aw86927->index = val;
	aw86927_haptic_set_repeat_wav_seq(aw86927, aw86927->index);
	mutex_unlock(&aw86927->lock);
	return count;
}

static ssize_t aw86927_vmax_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;

	return snprintf(buf, PAGE_SIZE, "0x%02X\n", aw86927->vmax);
}

static ssize_t aw86927_vmax_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	aw_info("%s: value=%d\n", __func__, val);

	mutex_lock(&aw86927->lock);
	aw86927->vmax = val;
	aw86927_haptic_set_bst_vol(aw86927, aw86927->vmax);
	mutex_unlock(&aw86927->lock);
	return count;
}

static ssize_t aw86927_gain_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;

	return snprintf(buf, PAGE_SIZE, "0x%02X\n", aw86927->gain);
}

static ssize_t aw86927_gain_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	aw_info("%s: value=%d\n", __func__, val);

	mutex_lock(&aw86927->lock);
	aw86927->gain = val;
	aw86927_haptic_set_gain(aw86927, aw86927->gain);
	mutex_unlock(&aw86927->lock);
	return count;
}

static ssize_t aw86927_seq_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	size_t count = 0;
	unsigned char i = 0;
	unsigned char reg_val = 0;

	for (i = 0; i < AW86927_SEQUENCER_SIZE; i++) {
		aw86927_haptic_get_wav_seq(aw86927, i, &reg_val);
		count += snprintf(buf + count, PAGE_SIZE - count,
				  "seq%d = 0x%02x\n", i + 1, reg_val);
		aw86927->seq[i] |= reg_val;
	}
	return count;
}

static ssize_t aw86927_seq_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	unsigned int databuf[2] = { 0, 0 };

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		if (databuf[0] >= AW86927_SEQUENCER_SIZE ||
		    databuf[1] > aw86927->ram.ram_num) {
			aw_err("%s: input value out of range!\n", __func__);
			return count;
		}
		aw_info("%s: seq%d=0x%02X\n", __func__,
			    databuf[0], databuf[1]);
		mutex_lock(&aw86927->lock);
		aw86927->seq[databuf[0]] = (unsigned char)databuf[1];
		aw86927_haptic_set_wav_seq(aw86927, (unsigned char)databuf[0],
					   aw86927->seq[databuf[0]]);
		mutex_unlock(&aw86927->lock);
	}
	return count;
}


static ssize_t aw86927_loop_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	size_t count = 0;
	unsigned char i = 0;
	unsigned char reg_val = 0;

	for (i = 0; i < AW86927_SEQUENCER_LOOP_SIZE; i++) {
		aw86927_haptic_get_wav_loop(aw86927, i, &reg_val);
		aw86927->loop[i] = reg_val;
		count += snprintf(buf + count, PAGE_SIZE - count,
				  "seq%d_loop = %d\n", i,
				  aw86927->loop[i]);
	}
	return count;
}

static ssize_t aw86927_loop_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	unsigned int databuf[2] = { 0, 0 };

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		aw_info("%s: seq%d loop=0x%02X\n", __func__,
			    databuf[0], databuf[1]);
		mutex_lock(&aw86927->lock);
		aw86927->loop[databuf[0]] = (unsigned char)databuf[1];
		aw86927_haptic_set_wav_loop(aw86927, (unsigned char)databuf[0],
					    aw86927->loop[databuf[0]]);
		mutex_unlock(&aw86927->lock);
	}

	return count;
}

static ssize_t aw86927_read_reg(struct aw86927 *aw86927, char *buf, ssize_t len,
				unsigned char head_reg_addr,
				unsigned char tail_reg_addr)
{
	int reg_num = 0;
	int i = 0;
	unsigned char reg_array[AW86927_REG_MAX] = {0};

	reg_num = tail_reg_addr - head_reg_addr + 1;
	aw86927_i2c_reads(aw86927, head_reg_addr, reg_array, reg_num);
	for (i = 0 ; i < reg_num; i++) {
		len += snprintf(buf + len, PAGE_SIZE - len,
				"reg:0x%02X=0x%02X\n",
				head_reg_addr + i, reg_array[i]);
	}
	return len;
}
static ssize_t aw86927_reg_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	ssize_t len = 0;

	len = aw86927_read_reg(aw86927, buf, len, AW86927_REG_RSTCFG,
			 AW86927_REG_RTPDATA - 1);
	if (!len)
		return len;
	len = aw86927_read_reg(aw86927, buf, len, AW86927_REG_RTPDATA + 1,
			 AW86927_REG_RAMDATA - 1);
	if (!len)
		return len;
	len = aw86927_read_reg(aw86927, buf, len, AW86927_REG_RAMDATA + 1,
			 AW86927_REG_ANACFG22);

	return len;
}

static ssize_t aw86927_reg_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	unsigned int databuf[2] = { 0, 0 };

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		if (databuf[0] == AW86927_REG_PLAYCFG1 &&
		    databuf[1] < AW86927_BIT_PLAYCFG1_BST_VOUT_6V)
		    databuf[1] = AW86927_BIT_PLAYCFG1_BST_VOUT_6V;
		if (databuf[0] == AW86927_REG_PLAYCFG1 &&
		    databuf[1] > AW86927_BIT_PLAYCFG1_BST_VOUT_10P5V)
		    databuf[1] = AW86927_BIT_PLAYCFG1_BST_VOUT_10P5V;
			aw86927_i2c_write(aw86927, (unsigned char)databuf[0],
					  (unsigned char)databuf[1]);
	}

	return count;
}

static ssize_t aw86927_rtp_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "rtp_cnt = %d\n",
			aw86927->rtp_cnt);
	return len;
}

static ssize_t aw86927_rtp_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0) {
		aw_err("%s kstrtouint fail\n", __func__);
		return rc;
	}
	mutex_lock(&aw86927->lock);
	aw86927_haptic_stop(aw86927);
	aw86927_haptic_set_rtp_aei(aw86927, false);
	aw86927_interrupt_clear(aw86927);
	if (val > 0) {
		queue_work(aw86927->work_queue, &aw86927->rtp_work);
	} else {
		aw_err("%s input number error:%d\n",
			   __func__, val);
	}
	mutex_unlock(&aw86927->lock);
	return count;
}


static ssize_t aw86927_ram_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	ssize_t len = 0;
	unsigned int i = 0;
	unsigned char reg_val = 0;

	/* RAMINIT Enable */
	aw86927_haptic_raminit(aw86927, true);
	aw86927_haptic_stop(aw86927);
	aw86927_set_base_addr(aw86927);
	len += snprintf(buf + len, PAGE_SIZE - len, "aw86927_haptic_ram:\n");
	for (i = 0; i < aw86927->ram.len; i++) {
		aw86927_read_ram_data(aw86927, &reg_val);
		len += snprintf(buf + len, PAGE_SIZE - len, "%02X,", reg_val);
	}
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	/* RAMINIT Disable */
	aw86927_haptic_raminit(aw86927, false);
	return len;
}

static ssize_t aw86927_ram_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	if (val)
		aw86927_ram_update(aw86927);
	return count;
}


static ssize_t aw86927_ram_update_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	ssize_t len = 0;
	int size = 0;
	int i = 0;
	int j = 0;
	unsigned char ram_data[AW_RAMDATA_RD_BUFFER_SIZE] = {0};

	/* RAMINIT Enable */
	aw86927_haptic_raminit(aw86927, true);
	aw86927_haptic_stop(aw86927);
	aw86927_set_base_addr(aw86927);
	len += snprintf(buf + len, PAGE_SIZE - len, "aw86927_haptic_ram:\n");
	while (i < aw86927->ram.len) {
		if ((aw86927->ram.len - i) < AW_RAMDATA_RD_BUFFER_SIZE)
			size = aw86927->ram.len - i;
		else
			size = AW_RAMDATA_RD_BUFFER_SIZE;

		aw86927_i2c_reads(aw86927, AW86927_REG_RAMDATA, ram_data, size);
		for (j = 0; j < size; j++) {
			len += snprintf(buf + len, PAGE_SIZE - len,
					"0x%02X,", ram_data[j]);
		}
		i += size;
	}
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	/* RAMINIT Disable */
	aw86927_haptic_raminit(aw86927, false);
	return len;
}

static ssize_t aw86927_ram_update_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	if (val)
		aw86927_ram_update(aw86927);
	return count;
}


static ssize_t aw86927_ram_num_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	ssize_t len = 0;

	aw86927_haptic_get_ram_number(aw86927);

	len += snprintf(buf + len, PAGE_SIZE - len,
			"ram_num = %d\n", aw86927->ram.ram_num);
	return len;
}


static ssize_t aw86927_f0_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	ssize_t len = 0;

	mutex_lock(&aw86927->lock);
	aw86927_haptic_upload_lra(aw86927, AW86927_WRITE_ZERO);
	aw86927_haptic_cont_get_f0(aw86927);
	mutex_unlock(&aw86927->lock);
	len += snprintf(buf + len, PAGE_SIZE - len, "%d\n", aw86927->f0);
	return len;
}

static ssize_t aw86927_f0_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	return count;
}



static ssize_t aw86927_cali_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	ssize_t len = 0;

	mutex_lock(&aw86927->lock);
	aw86927_haptic_upload_lra(aw86927, AW86927_F0_CALI);
	aw86927_haptic_cont_get_f0(aw86927);
	mutex_unlock(&aw86927->lock);
	len += snprintf(buf + len, PAGE_SIZE - len,
			"aw86927 cali f0 = %d\n", aw86927->f0);
	return len;
}

static ssize_t aw86927_cali_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	if (val) {
		mutex_lock(&aw86927->lock);
		aw86927_haptic_upload_lra(aw86927, AW86927_WRITE_ZERO);
		aw86927_haptic_f0_calibration(aw86927);
		mutex_unlock(&aw86927->lock);
	}
	return count;
}
static ssize_t aw86927_f0_save_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "%d\n",
			aw86927->f0_calib_data);

	return len;
}

static ssize_t aw86927_f0_save_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	aw86927->f0_calib_data = val;
	return count;
}


static ssize_t aw86927_osc_cali_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "%d\n",
			aw86927->lra_calib_data);

	return len;
}

static ssize_t aw86927_osc_cali_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	mutex_lock(&aw86927->lock);
	/*osc calibration flag start,Other behaviors are forbidden*/
	aw86927->osc_cali_run = 1;
	if (val == 3) {
		aw86927_haptic_set_pwm(aw86927, AW86927_PWM_24K);
		aw86927_haptic_upload_lra(aw86927, AW86927_WRITE_ZERO);
		aw86927_rtp_osc_calibration(aw86927);
		aw86927_rtp_trim_lra_calibration(aw86927);
	} else if (val == 1) {
		aw86927_haptic_set_pwm(aw86927, AW86927_PWM_24K);
		aw86927_haptic_upload_lra(aw86927, AW86927_OSC_CALI);
		aw86927_rtp_osc_calibration(aw86927);
	} else {
		aw_err("%s input value out of range\n", __func__);
	}
	aw86927->osc_cali_run = 0;
	/* osc calibration flag end, other behaviors are permitted */
	mutex_unlock(&aw86927->lock);

	return count;
}

static ssize_t aw86927_osc_save_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "%d\n",
			aw86927->lra_calib_data);

	return len;
}

static ssize_t aw86927_osc_save_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	aw86927->lra_calib_data = val;
	aw_info("%s load osa cal: %d\n", __func__, val);
	return count;
}

static ssize_t aw86927_cont_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	ssize_t len = 0;

	mutex_lock(&aw86927->lock);
	aw86927_haptic_read_cont_f0(aw86927);
	len += snprintf(buf + len, PAGE_SIZE - len,
			"aw86927 cont f0 = %d\n", aw86927->cont_f0);
	mutex_unlock(&aw86927->lock);
	return len;
}

static ssize_t aw86927_cont_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	mutex_lock(&aw86927->lock);
	aw86927_haptic_stop(aw86927);
	if (val) {
		aw86927_haptic_upload_lra(aw86927, AW86927_F0_CALI);
		aw86927_haptic_cont_play(aw86927);
	}
	mutex_unlock(&aw86927->lock);
	return count;
}

static ssize_t aw86927_vbat_monitor_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	ssize_t len = 0;

	mutex_lock(&aw86927->lock);
	aw86927_haptic_get_vbat(aw86927);
	len += snprintf(buf + len, PAGE_SIZE - len, "vbat=%dmv\n",
			aw86927->vbat);
	mutex_unlock(&aw86927->lock);

	return len;
}

static ssize_t aw86927_vbat_monitor_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	return count;
}



static ssize_t aw86927_lra_resistance_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	ssize_t len = 0;

	aw86927_haptic_get_lra_resistance(aw86927);
	len += snprintf(buf + len, PAGE_SIZE - len, "%d\n", aw86927->lra);
	return len;
}

static ssize_t aw86927_lra_resistance_store(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t count)
{
	return count;
}

static ssize_t aw86927_auto_boost_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "auto_boost=%d\n",
			aw86927->auto_boost);

	return len;
}

static ssize_t aw86927_auto_boost_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	mutex_lock(&aw86927->lock);
	aw86927_haptic_stop(aw86927);
	aw86927_haptic_auto_bst_enable(aw86927, val);
	mutex_unlock(&aw86927->lock);

	return count;
}

static ssize_t aw86927_prctmode_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	ssize_t len = 0;
	unsigned char reg_val = 0;

	aw86927_get_prctmode(aw86927);

	len += snprintf(buf + len, PAGE_SIZE - len, "prctmode=%d\n",
			reg_val & 0x08);
	return len;
}

static ssize_t aw86927_prctmode_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	unsigned int databuf[2] = { 0, 0 };
	unsigned int addr = 0;
	unsigned int val = 0;

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		addr = databuf[0];
		val = databuf[1];
		mutex_lock(&aw86927->lock);
		aw86927_haptic_swicth_motor_protect_config(aw86927, addr, val);
		mutex_unlock(&aw86927->lock);
	}
	return count;
}

static ssize_t aw86927_ram_vbat_comp_show(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len,
			"ram_vbat_comp=%d\n",
			aw86927->ram_vbat_comp);

	return len;
}

static ssize_t aw86927_ram_vbat_comp_store(struct device *dev,
						 struct device_attribute *attr,
						 const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	mutex_lock(&aw86927->lock);
	if (val)
		aw86927->ram_vbat_comp =
			AW86927_RAM_VBAT_COMP_ENABLE;
	else
		aw86927->ram_vbat_comp =
			AW86927_RAM_VBAT_COMP_DISABLE;
	mutex_unlock(&aw86927->lock);

	return count;
}

static ssize_t aw86927_nv_flag_show(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	ssize_t len = 0;

	aw_info("%s enter\n", __func__);
	len += snprintf(buf + len, PAGE_SIZE - len,
			"nv_flag=%d\n",
			aw86927->nv_flag);
	aw_info("The cat's nv_flag:%d\n",aw86927->nv_flag);
	return len;
}

static ssize_t aw86927_nv_flag_store(struct device *dev,
						 struct device_attribute *attr,
						 const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86927 *aw86927 = awinic->aw86927;
	unsigned int val = 0;
	int rc = 0;
	aw86927->nv_flag = 0;

	aw_info("%s enter\n", __func__);
	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	aw86927->nv_flag = val;
	aw_info("The echo's nv_flag:%d\n",aw86927->nv_flag);
	mutex_lock(&aw86927->lock);
	if(aw86927->nv_flag == 1)
		aw86927_ram_update(aw86927);
	mutex_unlock(&aw86927->lock);
	return count;
}


static DEVICE_ATTR(effect_id, S_IWUSR | S_IRUGO, aw86927_effect_id_show,
		   aw86927_effect_id_store);
static DEVICE_ATTR(f0_check, S_IRUGO, aw86927_f0_check_show, NULL);
static DEVICE_ATTR(f0_value, S_IRUGO, aw86927_f0_value_show, NULL);
static DEVICE_ATTR(custom_wave, S_IWUSR | S_IRUGO, aw86927_custom_wave_show,
		   aw86927_custom_wave_store);
static DEVICE_ATTR(bst_vol, S_IWUSR | S_IRUGO, aw86927_bst_vol_show,
		   aw86927_bst_vol_store);
static DEVICE_ATTR(cont_wait_num, S_IWUSR | S_IRUGO, aw86927_cont_wait_num_show,
		   aw86927_cont_wait_num_store);
static DEVICE_ATTR(cont_drv_lvl, S_IWUSR | S_IRUGO, aw86927_cont_drv_lvl_show,
		   aw86927_cont_drv_lvl_store);
static DEVICE_ATTR(cont_drv_time, S_IWUSR | S_IRUGO, aw86927_cont_drv_time_show,
		   aw86927_cont_drv_time_store);
static DEVICE_ATTR(cont_brk_time, S_IWUSR | S_IRUGO, aw86927_cont_brk_time_show,
		   aw86927_cont_brk_time_store);
static DEVICE_ATTR(trig, S_IWUSR | S_IRUGO, aw86927_trig_show,
		   aw86927_trig_store);
static DEVICE_ATTR(state, S_IWUSR | S_IRUGO, aw86927_state_show,
		   aw86927_state_store);
static DEVICE_ATTR(duration, S_IWUSR | S_IRUGO, aw86927_duration_show,
		   aw86927_duration_store);
static DEVICE_ATTR(activate, S_IWUSR | S_IRUGO, aw86927_activate_show,
		   aw86927_activate_store);
static DEVICE_ATTR(activate_mode, S_IWUSR | S_IRUGO, aw86927_activate_mode_show,
		   aw86927_activate_mode_store);
static DEVICE_ATTR(index, S_IWUSR | S_IRUGO, aw86927_index_show,
		   aw86927_index_store);
static DEVICE_ATTR(vmax, S_IWUSR | S_IRUGO, aw86927_vmax_show,
		   aw86927_vmax_store);
static DEVICE_ATTR(gain, S_IWUSR | S_IRUGO, aw86927_gain_show,
		   aw86927_gain_store);
static DEVICE_ATTR(seq, S_IWUSR | S_IRUGO, aw86927_seq_show, aw86927_seq_store);
static DEVICE_ATTR(loop, S_IWUSR | S_IRUGO, aw86927_loop_show,
		   aw86927_loop_store);
static DEVICE_ATTR(reg, S_IWUSR | S_IRUGO, aw86927_reg_show, aw86927_reg_store);

static DEVICE_ATTR(rtp, S_IWUSR | S_IRUGO, aw86927_rtp_show, aw86927_rtp_store);
static DEVICE_ATTR(ram, S_IWUSR | S_IRUGO, aw86927_ram_show, aw86927_ram_store);
static DEVICE_ATTR(ram_update, S_IWUSR | S_IRUGO, aw86927_ram_update_show,
		   aw86927_ram_update_store);
static DEVICE_ATTR(ram_num, S_IWUSR | S_IRUGO, aw86927_ram_num_show,
		   NULL);
static DEVICE_ATTR(f0, S_IWUSR | S_IRUGO, aw86927_f0_show, aw86927_f0_store);
static DEVICE_ATTR(cali, S_IWUSR | S_IRUGO, aw86927_cali_show,
		   aw86927_cali_store);
static DEVICE_ATTR(f0_save, S_IWUSR | S_IRUGO, aw86927_f0_save_show,
		   aw86927_f0_save_store);
static DEVICE_ATTR(osc_cali, S_IWUSR | S_IRUGO, aw86927_osc_cali_show,
		   aw86927_osc_cali_store);
static DEVICE_ATTR(osc_save, S_IWUSR | S_IRUGO, aw86927_osc_save_show,
		   aw86927_osc_save_store);
static DEVICE_ATTR(cont, S_IWUSR | S_IRUGO, aw86927_cont_show,
		   aw86927_cont_store);
static DEVICE_ATTR(vbat_monitor, S_IWUSR | S_IRUGO, aw86927_vbat_monitor_show,
		   aw86927_vbat_monitor_store);
static DEVICE_ATTR(lra_resistance, S_IWUSR | S_IRUGO,
		   aw86927_lra_resistance_show, aw86927_lra_resistance_store);
static DEVICE_ATTR(auto_boost, S_IWUSR | S_IRUGO, aw86927_auto_boost_show,
		   aw86927_auto_boost_store);
static DEVICE_ATTR(prctmode, S_IWUSR | S_IRUGO, aw86927_prctmode_show,
		   aw86927_prctmode_store);
static DEVICE_ATTR(ram_vbat_comp, S_IWUSR | S_IRUGO,
		   aw86927_ram_vbat_comp_show,
		   aw86927_ram_vbat_comp_store);
static DEVICE_ATTR(nv_flag, S_IWUSR | S_IRUGO,aw86927_nv_flag_show,
		   aw86927_nv_flag_store);

static struct attribute *aw86927_vibrator_attributes[] = {
	&dev_attr_effect_id.attr,
	&dev_attr_f0_check.attr,
	&dev_attr_f0_value.attr,
	&dev_attr_custom_wave.attr,
	&dev_attr_bst_vol.attr,
	&dev_attr_cont_wait_num.attr,
	&dev_attr_cont_drv_lvl.attr,
	&dev_attr_cont_drv_time.attr,
	&dev_attr_cont_brk_time.attr,
	&dev_attr_trig.attr,
	&dev_attr_state.attr,
	&dev_attr_duration.attr,
	&dev_attr_activate.attr,
	&dev_attr_activate_mode.attr,
	&dev_attr_index.attr,
	&dev_attr_vmax.attr,
	&dev_attr_gain.attr,
	&dev_attr_seq.attr,
	&dev_attr_loop.attr,
	&dev_attr_reg.attr,
	&dev_attr_rtp.attr,
	&dev_attr_ram.attr,
	&dev_attr_ram_update.attr,
	&dev_attr_ram_num.attr,
	&dev_attr_f0.attr,
	&dev_attr_cali.attr,
	&dev_attr_f0_save.attr,
	&dev_attr_osc_cali.attr,
	&dev_attr_osc_save.attr,
	&dev_attr_cont.attr,
	&dev_attr_vbat_monitor.attr,
	&dev_attr_lra_resistance.attr,
	&dev_attr_auto_boost.attr,
	&dev_attr_prctmode.attr,
	&dev_attr_ram_vbat_comp.attr,
	&dev_attr_nv_flag.attr,
	NULL
};

struct attribute_group aw86927_vibrator_attribute_group = {
	.attrs = aw86927_vibrator_attributes
};

/*****************************************************
 *
 * Extern function : parse dts
 *
 *****************************************************/
int aw86927_parse_dt(struct aw86927 *aw86927, struct device *dev,
		     struct device_node *np)
{
	unsigned int val = 0;
	unsigned int trig_config_temp[24] = {
		1, 0, 1, 1, 1, 2, 0, 0,
		1, 0, 0, 1, 0, 2, 0, 0,
		1, 0, 0, 1, 0, 2, 0, 0
	};
	struct qti_hap_config *config = &aw86927->config;
	struct device_node *child_node;
	struct qti_hap_effect *effect;
	int rc = 0, tmp, i = 0, j;
	unsigned int rtp_time[175];

	val = of_property_read_u32(np, "aw86927_vib_mode", &aw86927->info.mode);
	if (val != 0)
		aw_info("%s vib_mode not found\n", __func__);
	val = of_property_read_u32(np, "aw86927_vib_f0_pre",
				   &aw86927->info.f0_pre);
	if (val != 0)
		aw_info("%s vib_f0_pre not found\n", __func__);
	val = of_property_read_u32(np, "aw86927_vib_f0_cali_percen",
				   &aw86927->info.f0_cali_percen);
	if (val != 0)
		aw_info("%s vib_f0_cali_percen not found\n", __func__);

	val = of_property_read_u32(np, "aw86927_vib_cont_drv1_lvl",
				   &aw86927->info.cont_drv1_lvl);
	if (val != 0)
		aw_info("%s vib_cont_drv1_lvl not found\n", __func__);
	val = of_property_read_u32(np, "aw86927_vib_cont_drv2_lvl",
				   &aw86927->info.cont_drv2_lvl);
	if (val != 0)
		aw_info("%s vib_cont_drv2_lvl not found\n", __func__);
	val = of_property_read_u32(np, "aw86927_vib_cont_drv1_time",
				   &aw86927->info.cont_drv1_time);
	if (val != 0)
		aw_info("%s vib_cont_drv1_time not found\n", __func__);
	val = of_property_read_u32(np, "aw86927_vib_cont_drv2_time",
				   &aw86927->info.cont_drv2_time);
	if (val != 0)
		aw_info("%s vib_cont_drv2_time not found\n", __func__);
	val = of_property_read_u32(np, "aw86927_vib_cont_drv_width",
				   &aw86927->info.cont_drv_width);
	if (val != 0)
		aw_info("%s vib_cont_drv_width not found\n", __func__);
	val = of_property_read_u32(np, "aw86927_vib_cont_wait_num",
				   &aw86927->info.cont_wait_num);
	if (val != 0)
		aw_info("%s vib_cont_wait_num not found\n", __func__);
	val = of_property_read_u32(np, "aw86927_vib_cont_brk_time",
				   &aw86927->info.cont_brk_time);
	if (val != 0)
		aw_info("%s vib_cont_brk_time not found\n", __func__);
	val = of_property_read_u32(np, "aw86927_vib_cont_track_margin",
				   &aw86927->info.cont_track_margin);
	if (val != 0)
		aw_info("%s vib_cont_track_margin not found\n", __func__);
	val = of_property_read_u32(np, "aw86927_vib_brk_bst_md",
				   &aw86927->info.brk_bst_md);
	if (val != 0)
		aw_info("%s vib_brk_bst_md not found\n", __func__);
	val = of_property_read_u32(np, "aw86927_vib_cont_tset",
				   &aw86927->info.cont_tset);
	if (val != 0)
		aw_info("%s vib_cont_tset not found\n", __func__);
	val = of_property_read_u32(np, "aw86927_vib_cont_bemf_set",
				   &aw86927->info.cont_bemf_set);
	if (val != 0)
		aw_info("%s vib_cont_bemf_set not found\n",
			    __func__);
	val = of_property_read_u32(np, "aw86927_vib_cont_bst_brk_gain",
				   &aw86927->info.cont_bst_brk_gain);
	if (val != 0)
		aw_info("%s vib_cont_bst_brk_gain not found\n", __func__);
	val = of_property_read_u32(np, "aw86927_vib_cont_brk_gain",
				   &aw86927->info.cont_brk_gain);
	if (val != 0)
		aw_info("%s vib_cont_brk_gain not found\n", __func__);
	val = of_property_read_u32(np, "aw86927_vib_d2s_gain",
				   &aw86927->info.d2s_gain);
	if (val != 0)
		aw_info("%s vib_d2s_gain not found\n", __func__);
	val = of_property_read_u32_array(np, "aw86927_vib_trig_config",
					 trig_config_temp,
					 ARRAY_SIZE(trig_config_temp));
	if (val != 0)
		aw_info("%s vib_trig_config not found\n", __func__);
	memcpy(aw86927->info.trig_config, trig_config_temp,
	       sizeof(trig_config_temp));
	val = of_property_read_u32(np, "aw86927_vib_bst_vol_default",
				   &aw86927->info.bst_vol_default);
	if (val != 0)
		aw_info("%s vib_bst_vol_default not found\n", __func__);
	val = of_property_read_u32(np, "aw86927_vib_bst_vol_ram",
				   &aw86927->info.bst_vol_ram);
	if (val != 0)
		aw_info("%s vib_bst_vol_ram not found\n", __func__);
	val = of_property_read_u32(np, "aw86927_vib_bst_vol_rtp",
				   &aw86927->info.bst_vol_rtp);
	if (val != 0)
		aw_info("%s vib_bst_vol_rtp not found\n", __func__);

	val = of_property_read_u32(np, "vib_effect_id_boundary",
				 &aw86927->info.effect_id_boundary);
	if (val != 0)
		aw_info("%s vib_effect_id_boundary not found\n", __func__);
	val = of_property_read_u32(np, "vib_effect_max",
				 &aw86927->info.effect_max);
	if (val != 0)
		aw_info("%s vib_effect_max not found\n", __func__);
	val = of_property_read_u32_array(np, "vib_rtp_time", rtp_time,
				       ARRAY_SIZE(rtp_time));
	if (val != 0)
		aw_info("%s vib_rtp_time not found\n", __func__);
	memcpy(aw86927->info.rtp_time, rtp_time, sizeof(rtp_time));
	config->play_rate_us = HAP_PLAY_RATE_US_DEFAULT;
	rc = of_property_read_u32(np, "play-rate-us", &tmp);
	if (!rc)
	config->play_rate_us = (tmp >= HAP_PLAY_RATE_US_MAX) ?
		HAP_PLAY_RATE_US_MAX : tmp;

	aw86927->constant.pattern = devm_kcalloc(aw86927->dev,
						HAP_WAVEFORM_BUFFER_MAX,
						sizeof(u8), GFP_KERNEL);
	if (!aw86927->constant.pattern)
		return -ENOMEM;

	tmp = of_get_available_child_count(np);
	aw86927->predefined = devm_kcalloc(aw86927->dev, tmp,
					  sizeof(*aw86927->predefined),
					  GFP_KERNEL);
	if (!aw86927->predefined)
		return -ENOMEM;

	aw86927->effects_count = tmp;
	for_each_available_child_of_node(np, child_node) {
		effect = &aw86927->predefined[i++];
		rc = of_property_read_u32(child_node, "mtk,effect-id",
					  &effect->id);
		if (rc != 0)
			aw_info("%s Read mtk,effect-id failed\n", __func__);

		effect->vmax_mv = config->vmax_mv;
		rc = of_property_read_u32(child_node, "mtk,wf-vmax-mv", &tmp);
		if (rc != 0)
			aw_info("%s  Read mtk,wf-vmax-mv failed !\n", __func__);
		else
			effect->vmax_mv = tmp;

		aw_info("%s ---%d effect->vmax_mv =%d\n", __func__, __LINE__,
			effect->vmax_mv);
		rc = of_property_count_elems_of_size(child_node,
						     "mtk,wf-pattern",
						     sizeof(u8));
		if (rc < 0) {
			aw_info("%s Count mtk,wf-pattern property failed !\n",
			       __func__);
		} else if (rc == 0) {
			aw_info("%s mtk,wf-pattern has no data\n", __func__);
		}
		aw_info("%s ---%d\n", __func__, __LINE__);

		effect->pattern_length = rc;
		effect->pattern = devm_kcalloc(aw86927->dev,
					       effect->pattern_length,
					       sizeof(u8), GFP_KERNEL);

		rc = of_property_read_u8_array(child_node, "mtk,wf-pattern",
					       effect->pattern,
					       effect->pattern_length);
		if (rc < 0) {
			aw_info("%s Read mtk,wf-pattern property failed !\n",
			       __func__);
		}

		effect->play_rate_us = config->play_rate_us;
		rc = of_property_read_u32(child_node, "mtk,wf-play-rate-us",
					  &tmp);
		if (rc < 0)
			aw_info("%s Read mtk,wf-play-rate-us failed !\n",
			       __func__);
		else
			effect->play_rate_us = tmp;

		rc = of_property_read_u32(child_node, "mtk,wf-repeat-count",
					  &tmp);
		if (rc < 0) {
			aw_info("%s Read  mtk,wf-repeat-count failed !\n",
			       __func__);
		} else {
			for (j = 0; j < ARRAY_SIZE(wf_repeat); j++)
				if (tmp <= wf_repeat[j])
					break;

			effect->wf_repeat_n = j;
		}

		rc = of_property_read_u32(child_node, "mtk,wf-s-repeat-count",
					  &tmp);
		if (rc < 0) {
			aw_info("%s Read  mtk,wf-s-repeat-count failed !\n",
			       __func__);
		} else {
			for (j = 0; j < ARRAY_SIZE(wf_s_repeat); j++)
				if (tmp <= wf_s_repeat[j])
					break;

			effect->wf_s_repeat_n = j;
		}

		effect->lra_auto_res_disable =
			of_property_read_bool(child_node,
					      "mtk,lra-auto-resonance-disable");

		tmp = of_property_count_elems_of_size(child_node,
						      "mtk,wf-brake-pattern",
						      sizeof(u8));
		if (tmp <= 0)
			continue;

		if (tmp > HAP_BRAKE_PATTERN_MAX) {
			aw_info("%s wf-brake-pattern shouldn't be more than %d bytes\n",
			       __func__, HAP_BRAKE_PATTERN_MAX);
		}

		rc = of_property_read_u8_array(child_node,
					       "mtk,wf-brake-pattern",
					       effect->brake, tmp);
		if (rc < 0) {
			aw_info("%s Failed to get wf-brake-pattern !\n",
			       __func__);
		}

		effect->brake_pattern_length = tmp;
	}

	aw_info("%s aw86927->info.brk_bst_md: %d\n",
		    __func__, aw86927->info.brk_bst_md);
	aw_info("%s aw86927->info.bst_vol_default: %d\n",
		    __func__, aw86927->info.bst_vol_default);
	aw_info("%s aw86927->info.bst_vol_ram: %d\n",
		    __func__, aw86927->info.bst_vol_ram);
	aw_info("%s aw86927->info.bst_vol_rtp: %d\n",
		    __func__, aw86927->info.bst_vol_rtp);

	return 0;
}

static int aw86927_haptic_ram_vbat_comp(struct aw86927 *aw86927, bool flag)
{
	aw_dbg("%s enter!\n", __func__);
	if (flag)
		aw86927->ram_vbat_comp = AW86927_RAM_VBAT_COMP_ENABLE;
	else
		aw86927->ram_vbat_comp = AW86927_RAM_VBAT_COMP_DISABLE;
	return 0;
}


static int aw86927_haptic_effect_strength(struct aw86927 *aw86927)
{
	aw_dbg("%s enter\n", __func__);
	aw_dbg("%s: aw86927->play.vmax_mv =0x%x\n", __func__,
		 aw86927->play.vmax_mv);
#if 0
	switch (aw86927->play.vmax_mv) {
	case AW86927_LIGHT_MAGNITUDE:
		aw86927->level = 0x80;
		break;
	case AW86927_MEDIUM_MAGNITUDE:
		aw86927->level = 0x50;
		break;
	case AW86927_STRONG_MAGNITUDE:
		aw86927->level = 0x30;
		break;
	default:
		break;
	}
#else
	if (aw86927->play.vmax_mv >= 0x7FFF)
		aw86927->level = 0x80; /*128*/
	else if (aw86927->play.vmax_mv <= 0x3FFF)
		aw86927->level = 0x1E; /*30*/
	else
		aw86927->level = (aw86927->play.vmax_mv - 16383) / 128;
	if (aw86927->level < 0x1E)
		aw86927->level = 0x1E; /*30*/
#endif

	aw_info("%s: aw86927->level =0x%x\n", __func__, aw86927->level);
	return 0;
}

static int aw86927_haptic_play_effect_seq(struct aw86927 *aw86927,
					 unsigned char flag)
{
	if (aw86927->effect_id > aw86927->info.effect_id_boundary)
		return 0;

	if (flag) {
		if (aw86927->activate_mode == AW86927_ACTIVATE_RAM_MODE) {
			aw86927_haptic_set_wav_seq(aw86927, 0x00,
						(char)aw86927->effect_id + 1);
			aw86927_haptic_set_wav_seq(aw86927, 0x01, 0x00);
			aw86927_haptic_set_wav_loop(aw86927, 0x00, 0x00);
			aw86927_haptic_play_mode(aw86927, AW86927_RAM_MODE);
			if (aw86927->info.bst_vol_ram <= AW86927_MAX_BST_VO)
				aw86927_haptic_set_bst_vol(aw86927,
						aw86927->info.bst_vol_ram);
			else
				aw86927_haptic_set_bst_vol(aw86927,
							   aw86927->vmax);
			aw86927_haptic_effect_strength(aw86927);
			aw86927_haptic_set_gain(aw86927, aw86927->level);
			aw86927_haptic_play_go(aw86927);
		}
		if (aw86927->activate_mode == AW86927_ACTIVATE_RAM_LOOP_MODE) {
			aw86927_haptic_play_mode(aw86927,
						 AW86927_RAM_LOOP_MODE);
			aw86927_haptic_set_repeat_wav_seq(aw86927,
					(aw86927->info.effect_id_boundary + 1));
			aw86927_haptic_effect_strength(aw86927);
			aw86927_haptic_set_gain(aw86927, aw86927->level);
			aw86927_haptic_play_go(aw86927);
		}
	}

	return 0;
}

static enum
hrtimer_restart aw86927_haptic_audio_timer_func(struct hrtimer *timer)
{
	struct aw86927 *aw86927 =
	    container_of(timer, struct aw86927, haptic_audio.timer);

	aw_dbg("%s enter\n", __func__);
	queue_work(aw86927->work_queue, &aw86927->haptic_audio.work);

	hrtimer_start(&aw86927->haptic_audio.timer,
		      ktime_set(aw86927->haptic_audio.timer_val / 1000000,
				(aw86927->haptic_audio.timer_val % 1000000) *
				1000), HRTIMER_MODE_REL);
	return HRTIMER_NORESTART;
}

static int aw86927_haptic_juge_RTP_is_going_on(struct aw86927 *aw86927)
{
	unsigned char glb_state = 0;
	unsigned char rtp_state = 0;

	aw_dbg("%s enter\n", __func__);
	aw86927_haptic_get_glb_state(aw86927, &glb_state);
	if (aw86927->rtp_routine_on
	    || (glb_state == AW86927_BIT_GLBRD5_STATE_RTP_GO)) {
		rtp_state = 1;	/* is going on */
		aw_info("%s: rtp_routine_on\n", __func__);
	}
	return rtp_state;
}

static void aw86927_haptic_audio_work_routine(struct work_struct *work)
{
	struct aw86927 *aw86927 = container_of(work, struct aw86927,
					       haptic_audio.work);
	int rtp_is_going_on = 0;

	aw_info("%s enter\n", __func__);

	mutex_lock(&aw86927->haptic_audio.lock);
	/* rtp mode jump */
	rtp_is_going_on = aw86927_haptic_juge_RTP_is_going_on(aw86927);
	if (rtp_is_going_on) {
		mutex_unlock(&aw86927->haptic_audio.lock);
		return;
	}
	memcpy(&aw86927->haptic_audio.ctr,
	       &aw86927->haptic_audio.data[aw86927->haptic_audio.cnt],
	       sizeof(struct haptic_ctr));
	aw_dbg("%s: cnt=%d, cmd=%d, play=%d, wavseq=%d, loop=%d, gain=%d\n",
		 __func__,
		 aw86927->haptic_audio.cnt,
		 aw86927->haptic_audio.ctr.cmd,
		 aw86927->haptic_audio.ctr.play,
		 aw86927->haptic_audio.ctr.wavseq,
		 aw86927->haptic_audio.ctr.loop,
		 aw86927->haptic_audio.ctr.gain);
	mutex_unlock(&aw86927->haptic_audio.lock);
	if (aw86927->haptic_audio.ctr.cmd == AW86927_HAPTIC_CMD_ENABLE) {
		if (aw86927->haptic_audio.ctr.play ==
		    AW86927_HAPTIC_PLAY_ENABLE) {
			aw_info("%s: haptic_audio_play_start\n", __func__);
			mutex_lock(&aw86927->lock);
			aw86927_haptic_stop(aw86927);
			aw86927_haptic_play_mode(aw86927,
						 AW86927_RAM_MODE);

			aw86927_haptic_set_wav_seq(aw86927, 0x00,
						   aw86927->haptic_audio.ctr.
						   wavseq);
			aw86927_haptic_set_wav_seq(aw86927, 0x01, 0x00);

			aw86927_haptic_set_wav_loop(aw86927, 0x00,
						    aw86927->haptic_audio.ctr.
						    loop);

			aw86927_haptic_set_gain(aw86927,
						aw86927->haptic_audio.ctr.gain);

			aw86927_haptic_play_go(aw86927);
			mutex_unlock(&aw86927->lock);
		} else if (AW86927_HAPTIC_PLAY_STOP ==
			   aw86927->haptic_audio.ctr.play) {
			mutex_lock(&aw86927->lock);
			aw86927_haptic_stop(aw86927);
			mutex_unlock(&aw86927->lock);
		} else if (AW86927_HAPTIC_PLAY_GAIN ==
			   aw86927->haptic_audio.ctr.play) {
			mutex_lock(&aw86927->lock);
			aw86927_haptic_set_gain(aw86927,
						aw86927->haptic_audio.ctr.gain);
			mutex_unlock(&aw86927->lock);
		}
		mutex_lock(&aw86927->haptic_audio.lock);
		memset(&aw86927->haptic_audio.data[aw86927->haptic_audio.cnt],
		       0, sizeof(struct haptic_ctr));
		mutex_unlock(&aw86927->haptic_audio.lock);
	}
	mutex_lock(&aw86927->haptic_audio.lock);
	aw86927->haptic_audio.cnt++;
	if (aw86927->haptic_audio.data[aw86927->haptic_audio.cnt].cmd == 0) {
		aw86927->haptic_audio.cnt = 0;
		aw_dbg("%s: haptic play buffer restart\n", __func__);
	}
	mutex_unlock(&aw86927->haptic_audio.lock);
}

static void aw86927_haptic_audio_init(struct aw86927 *aw86927)
{
	aw_info("%s enter\n", __func__);

	aw86927->haptic_audio.delay_val = 1;
	aw86927->haptic_audio.timer_val = 21318;
	hrtimer_init(&aw86927->haptic_audio.timer, CLOCK_MONOTONIC,
		     HRTIMER_MODE_REL);
	aw86927->haptic_audio.timer.function = aw86927_haptic_audio_timer_func;
	INIT_WORK(&aw86927->haptic_audio.work,
		  aw86927_haptic_audio_work_routine);
	mutex_init(&aw86927->haptic_audio.lock);

}

static int aw86927_container_update(struct aw86927 *aw86927,
				     struct aw86927_container *aw86927_cont)
{
	unsigned int shift = 0;
	int ret = 0;
#ifdef AW_CHECK_RAM_DATA
	int i = 0;
	int len = 0;
	unsigned char ram_data[AW_RAMDATA_RD_BUFFER_SIZE] = {0};
#endif

	aw_info("%s enter\n", __func__);
	mutex_lock(&aw86927->lock);
	aw86927->ram.baseaddr_shift = 2;
	aw86927->ram.ram_shift = 4;
	/* RAMINIT Enable */
	aw86927_haptic_raminit(aw86927, true);
	/* Enter standby mode */
	aw86927_haptic_stop(aw86927);
	/* base addr */
	shift = aw86927->ram.baseaddr_shift;
	aw86927->ram.base_addr =
	    (unsigned int)((aw86927_cont->data[0 + shift] << 8) |
			   (aw86927_cont->data[1 + shift]));
	aw_info("%s: base_addr = %d\n", __func__,
		    aw86927->ram.base_addr);

	aw86927_set_base_addr(aw86927);
	aw86927_set_fifo_addr(aw86927);
	aw86927_get_fifo_addr(aw86927);
	aw86927_write_ram_data(aw86927, aw86927_cont);

#ifdef AW_CHECK_RAM_DATA
	aw86927_set_base_addr(aw86927);
	i = aw86927->ram.ram_shift;
	while (i < aw86927_cont->len) {
		if ((aw86927_cont->len - i) < AW_RAMDATA_RD_BUFFER_SIZE)
			len = aw86927_cont->len - i;
		else
			len = AW_RAMDATA_RD_BUFFER_SIZE;

		aw86927_i2c_reads(aw86927, AW86927_REG_RAMDATA, ram_data, len);
		ret = aw86927_check_ram_data(aw86927, &aw86927_cont->data[i],
					     ram_data, len);
		if (ret < 0)
			break;
		i += len;
	}
	if (ret)
		aw_err("%s: ram data check sum error\n", __func__);
	else
		aw_info("%s: ram data check sum pass\n", __func__);
#endif
	/* RAMINIT Disable */
	aw86927_haptic_raminit(aw86927, false);
	mutex_unlock(&aw86927->lock);
	aw_info("%s exit\n", __func__);

	return ret;
}

static void aw86927_ram_loaded(const struct firmware *cont, void *context)
{
	struct aw86927 *aw86927 = context;
	struct aw86927_container *aw86927_fw;
	int i = 0;
	int ret = 0;
	unsigned short check_sum = 0;

	aw_info("%s enter\n", __func__);
	if(aw86927->nv_flag == 1){
		if (!cont) {
			aw_err("%s: failed to read %s\n", __func__,
			   awinic_ram_name[1]);
			release_firmware(cont);
			return;
		}else{
			aw_info("%s: loaded %s - size: %zu bytes\n", __func__,
				awinic_ram_name[1], cont ? cont->size : 0);
		}
	}else{
		if (!cont) {
			aw_err("%s: failed to read %s\n", __func__,
			   awinic_ram_name[0]);
			release_firmware(cont);
			return;
		}else{
			aw_info("%s: loaded %s - size: %zu bytes\n", __func__,
				awinic_ram_name[0], cont ? cont->size : 0);
		}
	}
/*
 *	for(i=0; i < cont->size; i++) {
 *		aw_info("%s: addr: 0x%04x, data: 0x%02X\n",
 *			__func__, i, *(cont->data+i));
 *	}
 */
	/* check sum */
	for (i = 2; i < cont->size; i++)
		check_sum += cont->data[i];
	if (check_sum !=
	    (unsigned short)((cont->data[0] << 8) | (cont->data[1]))) {
		aw_err("%s: check sum err: check_sum=0x%04x\n", __func__,
			check_sum);
		release_firmware(cont);
		return;
	}
	aw_info("%s: check sum pass: 0x%04x\n",  __func__, check_sum);
	aw86927->ram.check_sum = check_sum;

	/* aw86927 ram update less then 128kB */
	aw86927_fw = kzalloc(cont->size + sizeof(int), GFP_KERNEL);
	if (!aw86927_fw) {
		release_firmware(cont);
		aw_err("%s: Error allocating memory\n",
			   __func__);
		return;
	}
	aw86927_fw->len = cont->size;
	memcpy(aw86927_fw->data, cont->data, cont->size);
	release_firmware(cont);
	ret = aw86927_container_update(aw86927, aw86927_fw);
	if (ret) {
		aw_err("%s: ram firmware update failed!\n",
			   __func__);
	} else {
		aw86927->ram_init = 1;
		aw86927_trig_config(aw86927);
		aw86927_haptic_get_ram_number(aw86927);
		aw_info("%s: ram firmware update complete!\n",
			    __func__);
	}
	aw86927->ram.len = aw86927_fw->len - aw86927->ram.ram_shift;
	kfree(aw86927_fw);
}

static int aw86927_ram_update(struct aw86927 *aw86927)
{
	aw86927->ram_init = 0;
	aw86927->rtp_init = 0;
	aw_info("%s enter\n", __func__);
	if(aw86927->nv_flag == 1){
		aw_info("Use the backup ram file!\n");
		return request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
				       awinic_ram_name[1], aw86927->dev,
				       GFP_KERNEL, aw86927, aw86927_ram_loaded);
	}else{
		aw_info("Use the default ram file!\n");
		return request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
				       awinic_ram_name[0], aw86927->dev,
				       GFP_KERNEL, aw86927, aw86927_ram_loaded);
	}
}

#ifdef AW_RAM_UPDATE_DELAY
static void aw86927_ram_work_routine(struct work_struct *work)
{
	struct aw86927 *aw86927 =
	    container_of(work, struct aw86927, ram_work.work);

	aw_info("%s enter\n", __func__);

	aw86927_ram_update(aw86927);

}
#endif

static int aw86927_haptic_rtp_play(struct aw86927 *aw86927)
{

	unsigned int buf_len = 0;
	unsigned int period_size = aw86927->ram.base_addr >> 2;

	aw_info("%s enter\n", __func__);
	aw86927->rtp_cnt = 0;
	disable_irq(gpio_to_irq(aw86927->irq_gpio));
	while ((!aw86927_haptic_rtp_get_fifo_afs(aw86927)) &&
		(aw86927->play_mode == AW86927_RTP_MODE) &&
		!atomic_read(&aw86927->exit_in_rtp_loop)) {
		if (aw86927->is_custom_wave == 0) {
			if ((aw86927_rtp->len - aw86927->rtp_cnt) <
			(aw86927->ram.base_addr >> 2)) {
				buf_len = aw86927_rtp->len - aw86927->rtp_cnt;
			} else {
				buf_len = (aw86927->ram.base_addr >> 2);
			}
			aw86927_i2c_writes(aw86927, AW86927_REG_RTPDATA,
					  &aw86927_rtp->data[aw86927->rtp_cnt],
					  buf_len);
			aw86927->rtp_cnt += buf_len;
			if (aw86927->rtp_cnt == aw86927_rtp->len) {
				aw86927->rtp_cnt = 0;
				aw86927_haptic_set_rtp_aei(aw86927, false);
				break;
			}
		} else {
			buf_len = read_rb(aw86927_rtp->data,  period_size);
			aw86927_i2c_writes(aw86927, AW86927_REG_RTPDATA,
					   aw86927_rtp->data, buf_len);
			if (buf_len < period_size) {
				aw_info("%s: custom rtp update complete\n",
					__func__);
				aw86927->rtp_cnt = 0;
				aw86927_haptic_set_rtp_aei(aw86927, false);
				break;
			}
		}
	}
	enable_irq(gpio_to_irq(aw86927->irq_gpio));
	if (aw86927->play_mode == AW86927_RTP_MODE &&
		!atomic_read(&aw86927->exit_in_rtp_loop)) {
		aw86927_haptic_set_rtp_aei(aw86927, true);
	}
	aw_info("%s: exit\n", __func__);

	return 0;
}

static void aw86927_rtp_work_routine(struct work_struct *work)
{
	bool rtp_work_flag = false;
	unsigned char reg_val = 0;
	unsigned int cnt = 200;
	const struct firmware *rtp_file;
	int ret = -1;

	struct aw86927 *aw86927 = container_of(work, struct aw86927, rtp_work);

	if ((aw86927->effect_id < aw86927->info.effect_id_boundary) &&
	    (aw86927->effect_id > aw86927->info.effect_max))
		return;

	aw_info("%s: effect_id = %d state=%d activate_mode = %d\n", __func__,
		aw86927->effect_id, aw86927->state, aw86927->activate_mode);
	mutex_lock(&aw86927->lock);
	aw86927_haptic_upload_lra(aw86927, AW86927_OSC_CALI);
	aw86927_haptic_set_rtp_aei(aw86927, false);
	aw86927_interrupt_clear(aw86927);
	/* wait for irq to exit */
	atomic_set(&aw86927->exit_in_rtp_loop, 1);
	while (atomic_read(&aw86927->is_in_rtp_loop)) {
		aw_info("%s  goint to waiting irq exit\n", __func__);
		mutex_unlock(&aw86927->lock);
		ret = wait_event_interruptible(aw86927->wait_q,
				atomic_read(&aw86927->is_in_rtp_loop) == 0);
		aw_info("%s  wakeup\n", __func__);
		mutex_lock(&aw86927->lock);
		if (ret == -ERESTARTSYS) {
			atomic_set(&aw86927->exit_in_rtp_loop, 0);
			wake_up_interruptible(&aw86927->stop_wait_q);
			mutex_unlock(&aw86927->lock);
			aw_err("%s wake up by signal return erro\n", __func__);
			return;
		}
	}

	atomic_set(&aw86927->exit_in_rtp_loop, 0);
	wake_up_interruptible(&aw86927->stop_wait_q);

	/* how to force exit this call */
	if (aw86927->is_custom_wave == 1 && aw86927->state) {
		aw_err("%s buffer size %d, availbe size %d\n",
		       __func__, aw86927->ram.base_addr >> 2,
		       get_rb_avalible_size());
		while (get_rb_avalible_size() < aw86927->ram.base_addr &&
		       !rb_shoule_exit()) {
			mutex_unlock(&aw86927->lock);
			ret = wait_event_interruptible(aw86927->stop_wait_q,
							(get_rb_avalible_size() >= aw86927->ram.base_addr) ||
							rb_shoule_exit());
			aw_info("%s  wakeup\n", __func__);
			aw_err("%s after wakeup sbuffer size %d, availbe size %d\n",
			       __func__, aw86927->ram.base_addr >> 2,
			       get_rb_avalible_size());
			if (ret == -ERESTARTSYS) {
				aw_err("%s wake up by signal return erro\n",
				       __func__);
					return;
			}
			mutex_lock(&aw86927->lock);

		}
	}

	aw86927_haptic_stop(aw86927);
	if (aw86927->state) {
		pm_stay_awake(aw86927->dev);
		/* boost voltage */
		if (aw86927->info.bst_vol_ram <= AW86927_MAX_BST_VO)
			aw86927_haptic_set_bst_vol(aw86927,
						aw86927->info.bst_vol_rtp);
		else
			aw86927_haptic_set_bst_vol(aw86927, aw86927->vmax);
		/* gain */
		aw86927_haptic_ram_vbat_comp(aw86927, false);
		aw86927_haptic_effect_strength(aw86927);
		aw86927_haptic_set_gain(aw86927, aw86927->level);
		aw86927->rtp_init = 0;
		if (aw86927->is_custom_wave == 0) {
			aw86927->rtp_file_num = aw86927->effect_id -
					aw86927->info.effect_id_boundary;
			aw_info("%s: aw86927->rtp_file_num =%d\n", __func__,
			       aw86927->rtp_file_num);
			if (aw86927->rtp_file_num < 0)
				aw86927->rtp_file_num = 0;
			if (aw86927->rtp_file_num > (awinic_rtp_name_len - 1))
				aw86927->rtp_file_num = awinic_rtp_name_len - 1;
			aw86927->rtp_routine_on = 1;
			/* fw loaded */
			ret = request_firmware(&rtp_file,
					awinic_rtp_name[aw86927->rtp_file_num],
					aw86927->dev);
			if (ret < 0) {
				aw_err("%s: failed to read %s\n", __func__,
					awinic_rtp_name[aw86927->rtp_file_num]);
				aw86927->rtp_routine_on = 0;
				pm_relax(aw86927->dev);
				mutex_unlock(&aw86927->lock);
				return;
			}
			vfree(aw86927_rtp);
			aw86927_rtp = vmalloc(rtp_file->size + sizeof(int));
			if (!aw86927_rtp) {
				release_firmware(rtp_file);
				aw_err("%s: error allocating memory\n",
				       __func__);
				aw86927->rtp_routine_on = 0;
				pm_relax(aw86927->dev);
				mutex_unlock(&aw86927->lock);
				return;
			}
			aw86927_rtp->len = rtp_file->size;
			aw_info("%s: rtp file:[%s] size = %dbytes\n",
				__func__,
				awinic_rtp_name[aw86927->rtp_file_num],
				aw86927_rtp->len);
			memcpy(aw86927_rtp->data, rtp_file->data,
			       rtp_file->size);
			release_firmware(rtp_file);
		} else {
			vfree(aw86927_rtp);
			aw86927_rtp = NULL;
			if(aw86927->ram.base_addr != 0) {
				aw86927_rtp = vmalloc((aw86927->ram.base_addr >> 2) + sizeof(int));
			} else {
				pr_err("ram update not done yet, return !");
			}
			if (!aw86927_rtp) {
				aw_err("%s: error allocating memory\n",
				       __func__);
				pm_relax(aw86927->dev);
				mutex_unlock(&aw86927->lock);
				return;
			}
		}
		aw86927->rtp_init = 1;
		/* rtp mode config */
		aw86927_haptic_play_mode(aw86927, AW86927_RTP_MODE);
		/* haptic go */
		aw86927_haptic_play_go(aw86927);
		usleep_range(2000, 2500);
		while (cnt) {
			aw86927_haptic_get_glb_state(aw86927, &reg_val);
			if ((reg_val & 0x0f) ==
			     AW86927_BIT_GLBRD5_STATE_RTP_GO) {
				cnt = 0;
				rtp_work_flag = true;
				aw_info("%s RTP_GO! glb_state=0x08\n",
					__func__);
			} else {
				cnt--;
				aw_dbg("%s wait for RTP_GO, glb_state=0x%02X\n",
					 __func__, reg_val);
			}
			usleep_range(2000, 2500);
		}
		if (rtp_work_flag) {
			aw86927_haptic_rtp_play(aw86927);
		} else {
			/* enter standby mode */
			aw86927_haptic_stop(aw86927);
			aw_err("%s failed to enter RTP_GO status!\n", __func__);
		}
		aw86927->rtp_routine_on = 0;
	} else {
		aw86927->rtp_cnt = 0;
		aw86927->rtp_init = 0;
		pm_relax(aw86927->dev);
	}
	mutex_unlock(&aw86927->lock);
}

static enum hrtimer_restart aw86927_vibrator_timer_func(struct hrtimer *timer)
{
	struct aw86927 *aw86927 = container_of(timer, struct aw86927, timer);

	aw_info("%s enter\n", __func__);

	aw86927->state = 0;
	queue_work(aw86927->work_queue, &aw86927->vibrator_work);

	return HRTIMER_NORESTART;
}

static void aw86927_vibrator_work_routine(struct work_struct *work)
{
	struct aw86927 *aw86927 = container_of(work, struct aw86927,
					       vibrator_work);

	aw_dbg("%s enter\n", __func__);
	aw_info("%s: effect_id = %d state=%d activate_mode = %d duration = %d\n",
		__func__,
		aw86927->effect_id, aw86927->state, aw86927->activate_mode,
		aw86927->duration);
	mutex_lock(&aw86927->lock);
	if (aw86927->effect_id == 10) {
		aw86927_haptic_upload_lra(aw86927, AW86927_WRITE_ZERO);
	} else {
		aw86927_haptic_upload_lra(aw86927, AW86927_F0_CALI);
	}
	aw86927_haptic_stop(aw86927);
	if (aw86927->state) {
		if (aw86927->activate_mode ==
		    AW86927_ACTIVATE_RAM_MODE) {
			aw86927_haptic_ram_vbat_comp(aw86927, false);
			aw86927_haptic_play_effect_seq(aw86927, true);
		} else if (aw86927->activate_mode ==
			   AW86927_ACTIVATE_RAM_LOOP_MODE) {
			aw86927_haptic_ram_vbat_comp(aw86927, true);
			aw86927_haptic_play_effect_seq(aw86927, true);
			hrtimer_start(&aw86927->timer,
				      ktime_set(aw86927->duration / 1000,
						(aw86927->duration % 1000) *
						1000000), HRTIMER_MODE_REL);
		} else if (aw86927->activate_mode ==
			   AW86927_ACTIVATE_CONT_MODE) {
			aw86927_haptic_cont_play(aw86927);
			hrtimer_start(&aw86927->timer,
				      ktime_set(aw86927->duration / 1000,
						(aw86927->duration % 1000) *
						1000000), HRTIMER_MODE_REL);
		} else {
			/*other mode */
		}
	}
	mutex_unlock(&aw86927->lock);
}

irqreturn_t aw86927_irq(int irq, void *data)
{
	struct aw86927 *aw86927 = data;
	unsigned char glb_state_val = 0;
	unsigned char reg_val = 0;
	unsigned int buf_len = 0;
	unsigned int period_size = aw86927->ram.base_addr >> 2;
	int ret = 0;

	atomic_set(&aw86927->is_in_rtp_loop, 1);
	aw_info("%s enter\n", __func__);
	ret = aw86927_get_irq_state(aw86927);
	if (ret < 0)
		return IRQ_HANDLED;
	if (ret & AW86927_SYSINT_ERROR)
		aw86927->rtp_routine_on = 0;
	if ((ret & AW86927_SYSINT_FF_AEI) && aw86927->rtp_init) {
		aw_dbg("%s: aw86927 rtp fifo almost empty\n", __func__);
		while ((!aw86927_haptic_rtp_get_fifo_afs(aw86927)) &&
			(aw86927->play_mode == AW86927_RTP_MODE) &&
			!atomic_read(&aw86927->exit_in_rtp_loop)) {
			mutex_lock(&aw86927->rtp_lock);
			if (!aw86927_rtp) {
				aw_info("%s:aw86927_rtp is null, break!\n",
					__func__);
				mutex_unlock(&aw86927->rtp_lock);
				break;
			}
			if (aw86927->is_custom_wave == 1) {
				buf_len = read_rb(aw86927_rtp->data,
						  period_size);
				aw86927_i2c_writes(aw86927,
						AW86927_REG_RTPDATA,
						aw86927_rtp->data,
						buf_len);
				if (buf_len < period_size) {
					aw_info("%s: rtp update complete\n",
						__func__);
					aw86927_haptic_set_rtp_aei(aw86927,
								  false);
					aw86927->rtp_cnt = 0;
					aw86927->rtp_init = 0;
					mutex_unlock(&aw86927->rtp_lock);
					break;
				}
			} else {
				if ((aw86927_rtp->len - aw86927->rtp_cnt) <
				    period_size) {
					buf_len = aw86927_rtp->len -
							aw86927->rtp_cnt;
				} else {
					buf_len = period_size;
				}
				aw86927_i2c_writes(aw86927,
						AW86927_REG_RTPDATA,
						&aw86927_rtp->
						data[aw86927->rtp_cnt],
						buf_len);
				aw86927->rtp_cnt += buf_len;
				aw86927_haptic_get_glb_state(aw86927,
							     &glb_state_val);
				if ((glb_state_val & 0x0f) == 0) {
					if (aw86927->rtp_cnt !=
					    aw86927_rtp->len)
						aw_err("%s: rtp play suspend!\n",
							__func__);
					else
						aw_info("%s: rtp update complete!\n",
							__func__);
					aw86927->rtp_routine_on = 0;
					aw86927_haptic_set_rtp_aei(aw86927,
								   false);
					aw86927->rtp_cnt = 0;
					aw86927->rtp_init = 0;
					mutex_unlock(&aw86927->rtp_lock);
					break;
				}
			}
			mutex_unlock(&aw86927->rtp_lock);
		}

	}

	if (ret & AW86927_SYSINT_FF_AFI)
		aw_info("%s: aw86927 rtp mode fifo almost full!\n", __func__);

	if (aw86927->play_mode != AW86927_RTP_MODE ||
	    atomic_read(&aw86927->exit_in_rtp_loop))
		aw86927_haptic_set_rtp_aei(aw86927, false);

	aw86927_i2c_read(aw86927, AW86927_REG_SYSINT, &reg_val);
	aw_dbg("%s: reg SYSINT=0x%x\n", __func__, reg_val);
	aw86927_i2c_read(aw86927, AW86927_REG_SYSST, &reg_val);
	aw_dbg("%s: reg SYSST=0x%x\n", __func__, reg_val);
	atomic_set(&aw86927->is_in_rtp_loop, 0);
	wake_up_interruptible(&aw86927->wait_q);
	aw_info("%s exit\n", __func__);

	return IRQ_HANDLED;
}

int aw86927_vibrator_init(struct aw86927 *aw86927)
{
	int ret = 0;

	aw_info("%s enter\n", __func__);
	ret = sysfs_create_group(&aw86927->i2c->dev.kobj,
				 &aw86927_vibrator_attribute_group);
	if (ret < 0) {
		aw_info("%s error creating sysfs attr files\n", __func__);
		return ret;
	}

	hrtimer_init(&aw86927->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	aw86927->timer.function = aw86927_vibrator_timer_func;
	INIT_WORK(&aw86927->vibrator_work, aw86927_vibrator_work_routine);
	INIT_WORK(&aw86927->rtp_work, aw86927_rtp_work_routine);
	mutex_init(&aw86927->lock);
	mutex_init(&aw86927->rtp_lock);
	atomic_set(&aw86927->is_in_rtp_loop, 0);
	atomic_set(&aw86927->exit_in_rtp_loop, 0);
	atomic_set(&aw86927->is_in_write_loop, 0);
	init_waitqueue_head(&aw86927->wait_q);
	init_waitqueue_head(&aw86927->stop_wait_q);
	return 0;
}

int aw86927_haptic_init(struct aw86927 *aw86927)
{
	aw_info("%s enter\n", __func__);

	mutex_lock(&aw86927->lock);
	aw86927_haptic_audio_init(aw86927);
	aw86927_vibrate_params_init(aw86927);
	aw86927_haptic_play_mode(aw86927, AW86927_STANDBY_MODE);
	aw86927_haptic_set_pwm(aw86927, AW86927_PWM_24K);
	aw86927_haptic_misc_para_init(aw86927);
	aw86927_haptic_set_bst_vol(aw86927, aw86927->vmax);
	aw86927_haptic_set_bst_peak_cur(aw86927, AW86927_BIT_ANACFG13_PEAKCUR_3P75A);
	aw86927_haptic_swicth_motor_protect_config(aw86927, AW_PROTECT_EN, AW_PROTECT_VAL);
	aw86927_haptic_auto_bst_enable(aw86927, false);
	aw86927_haptic_auto_break_mode(aw86927, false);
	aw86927_haptic_vbat_mode_config(aw86927, AW86927_VBAT_SW_ADJUST_MODE);
	mutex_unlock(&aw86927->lock);
	/* f0 calibration */
#ifndef USE_CONT_F0_CALI
	mutex_lock(&aw86927->lock);
	aw86927_haptic_upload_lra(aw86927, AW86927_WRITE_ZERO);
	aw86927_haptic_f0_calibration(aw86927);
	mutex_unlock(&aw86927->lock);
#endif

	return 0;

}

int aw86927_ram_init(struct aw86927 *aw86927)
{
#ifdef AW_RAM_UPDATE_DELAY
	int ram_timer_val = 5000;

	aw_info("%s enter\n", __func__);
	INIT_DELAYED_WORK(&aw86927->ram_work, aw86927_ram_work_routine);
	queue_delayed_work(aw86927->work_queue, &aw86927->ram_work,
			   msecs_to_jiffies(ram_timer_val));
#else
	aw86927_ram_update(aw86927);
#endif
	return 0;
}

int aw86927_haptics_upload_effect(struct input_dev *dev,
					struct ff_effect *effect,
					struct ff_effect *old)
{
	struct aw86927 *aw86927 = input_get_drvdata(dev);
	struct qti_hap_play_info *play = &aw86927->play;
	s16 data[CUSTOM_DATA_LEN];
	ktime_t rem;
	s64 time_us;
	int ret;
	/*for osc calibration*/
	if (aw86927->osc_cali_run != 0)
		return 0;

	if (hrtimer_active(&aw86927->timer)) {
		rem = hrtimer_get_remaining(&aw86927->timer);
		time_us = ktime_to_us(rem);
		aw_info("waiting for playing clear sequence: %lld us\n",
			time_us);
		usleep_range(time_us, time_us + 100);
	}
	aw_dbg("%s: effect->type=0x%x,FF_CONSTANT=0x%x,FF_PERIODIC=0x%x\n",
		__func__, effect->type, FF_CONSTANT, FF_PERIODIC);
	aw86927->effect_type = effect->type;
	 mutex_lock(&aw86927->lock);
	 while (atomic_read(&aw86927->exit_in_rtp_loop)) {
		aw_info("%s  goint to waiting rtp  exit\n", __func__);
		mutex_unlock(&aw86927->lock);
		ret = wait_event_interruptible(aw86927->stop_wait_q,
				atomic_read(&aw86927->exit_in_rtp_loop) == 0);
		aw_info("%s wakeup\n", __func__);
		if (ret == -ERESTARTSYS) {
			mutex_unlock(&aw86927->lock);
			aw_err("%s wake up by signal return erro\n", __func__);
			return ret;
		}
		mutex_lock(&aw86927->lock);
	 }
	if (aw86927->effect_type == FF_CONSTANT) {
		aw_dbg("%s: effect_type is  FF_CONSTANT!\n", __func__);
		/*cont mode set duration */
		aw86927->duration = effect->replay.length;
		aw86927->activate_mode = AW86927_ACTIVATE_RAM_LOOP_MODE;
		aw86927->effect_id = aw86927->info.effect_id_boundary;

	} else if (aw86927->effect_type == FF_PERIODIC) {
		if (aw86927->effects_count == 0) {
			mutex_unlock(&aw86927->lock);
			return -EINVAL;
		}

		aw_dbg("%s: effect_type is  FF_PERIODIC!\n", __func__);
		if (copy_from_user(data, effect->u.periodic.custom_data,
				   sizeof(s16) * CUSTOM_DATA_LEN)) {
			mutex_unlock(&aw86927->lock);
			return -EFAULT;
		}
		aw86927->effect_id = data[0];
		aw_dbg("%s: aw86927->effect_id =%d\n",
			 __func__, aw86927->effect_id);
		play->vmax_mv = effect->u.periodic.magnitude; /*vmax level*/
		if (aw86927->effect_id < 0 ||
			aw86927->effect_id > aw86927->info.effect_max) {
			mutex_unlock(&aw86927->lock);
			return 0;
		}
		aw86927->is_custom_wave = 0;
		if (aw86927->effect_id < aw86927->info.effect_id_boundary) {
			aw86927->activate_mode = AW86927_ACTIVATE_RAM_MODE;
			aw_dbg("%s: aw86927->effect_id=%d , aw86927->activate_mode = %d\n",
				__func__, aw86927->effect_id,
				aw86927->activate_mode);
			/*second data*/
			data[1] = aw86927->predefined[aw86927->effect_id].play_rate_us/1000000;
			/*millisecond data*/
			data[2] = aw86927->predefined[aw86927->effect_id].play_rate_us/1000;
		}
		if (aw86927->effect_id >= aw86927->info.effect_id_boundary) {
			aw86927->activate_mode = AW86927_ACTIVATE_RTP_MODE;
			aw_dbg("%s: aw86927->effect_id=%d , aw86927->activate_mode = %d\n",
				__func__, aw86927->effect_id,
				aw86927->activate_mode);
			/*second data*/
			data[1] = 30;
			/*millisecond data*/
			data[2] = 0;
		}
		if (aw86927->effect_id == CUSTOME_WAVE_ID) {
			aw86927->activate_mode = AW86927_ACTIVATE_RTP_MODE;
			aw_dbg("%s: aw86927->effect_id=%d , aw86927->activate_mode = %d\n",
				__func__, aw86927->effect_id,
				aw86927->activate_mode);
			/*second data*/
			data[1] = 30;
			/*millisecond data*/
			data[2] = 0;
			aw86927->is_custom_wave = 1;
			rb_init();
		}
		if (copy_to_user(effect->u.periodic.custom_data, data,
			sizeof(s16) * CUSTOM_DATA_LEN)) {
			mutex_unlock(&aw86927->lock);
			return -EFAULT;
		}

	} else {
		aw_err("%s Unsupported effect type: %d\n", __func__,
		       effect->type);
	}
	mutex_unlock(&aw86927->lock);
	return 0;
}

int aw86927_haptics_playback(struct input_dev *dev, int effect_id,
				   int val)
{
	struct aw86927 *aw86927 = input_get_drvdata(dev);
	int rc = 0;

	aw_dbg("%s: effect_id=%d , activate_mode = %d val = %d\n",
		__func__, aw86927->effect_id, aw86927->activate_mode, val);
	/*for osc calibration*/
	if (aw86927->osc_cali_run != 0)
		return 0;

	if (val > 0)
		aw86927->state = 1;
	if (val <= 0)
		aw86927->state = 0;
	hrtimer_cancel(&aw86927->timer);

	if (aw86927->effect_type == FF_CONSTANT &&
		aw86927->activate_mode ==
			AW86927_ACTIVATE_RAM_LOOP_MODE) {
		aw_dbg("%s: enter cont_mode\n", __func__);
		queue_work(aw86927->work_queue, &aw86927->vibrator_work);
	} else if (aw86927->effect_type == FF_PERIODIC &&
		aw86927->activate_mode == AW86927_ACTIVATE_RAM_MODE) {
		aw_dbg("%s: enter  ram_mode\n", __func__);
		queue_work(aw86927->work_queue, &aw86927->vibrator_work);
	} else if ((aw86927->effect_type == FF_PERIODIC) &&
		aw86927->activate_mode == AW86927_ACTIVATE_RTP_MODE) {
		aw_dbg("%s: enter  rtp_mode\n", __func__);
		queue_work(aw86927->work_queue, &aw86927->rtp_work);
		/*if we are in the play mode, force to exit*/
		if (val == 0) {
			atomic_set(&aw86927->exit_in_rtp_loop, 1);
			rb_force_exit();
			wake_up_interruptible(&aw86927->stop_wait_q);
		}
	} else {
		/*other mode */
	}

	return rc;
}

int aw86927_haptics_erase(struct input_dev *dev, int effect_id)
{
	struct aw86927 *aw86927 = input_get_drvdata(dev);
	int rc = 0;

	/*for osc calibration*/
	if (aw86927->osc_cali_run != 0)
		return 0;

	aw_dbg("%s: enter\n", __func__);
	aw86927->effect_type = 0;
	aw86927->is_custom_wave = 0;
	aw86927->duration = 0;
	return rc;
}

void aw86927_haptics_set_gain_work_routine(struct work_struct *work)
{
	unsigned char comp_level = 0;
	int ret = -EINVAL;
	char type[] = "charger_therm0";
	unsigned char tep = 0;
	int temp = 0;
	struct aw86927 *aw86927 =
	    container_of(work, struct aw86927, set_gain_work);

	struct thermal_zone_device *tzd;
	aw_dbg("%s enter!\n", __func__);
	tzd = thermal_zone_get_zone_by_name(type);
	ret = thermal_zone_get_temp(tzd, &temp);
	aw_info("The temperature:%d,return value:%d\n",temp,ret);


	if (aw86927->new_gain >= 0x7FFF)
		aw86927->level = 0x80;	/*128 */
	else if (aw86927->new_gain <= 0x3FFF)
		aw86927->level = 0x1E;	/*30 */
	else
		aw86927->level = (aw86927->new_gain - 16383) / 128;

	if (aw86927->level < 0x1E)
		aw86927->level = 0x1E;	/*30 */
	aw_info("%s: set_gain queue work, new_gain = %x level = %x\n",
		__func__, aw86927->new_gain, aw86927->level);

	if (aw86927->ram_vbat_comp == AW86927_RAM_VBAT_COMP_ENABLE
		&& aw86927->vbat) {
		aw_dbg("%s: ref %d vbat %d ", __func__, AW86927_VBAT_REFER,
				aw86927->vbat);
		comp_level = aw86927->level * AW86927_VBAT_REFER / aw86927->vbat;
		if (comp_level > (128 * AW86927_VBAT_REFER / AW86927_VBAT_MIN)) {
			comp_level = 128 * AW86927_VBAT_REFER / AW86927_VBAT_MIN;
			aw_dbg("%s: comp level limit is %d ",
				 __func__, comp_level);
		}
		aw_info("%s: enable vbat comp, level = %x comp level = %x",
			__func__, aw86927->level, comp_level);
		if (aw86927->effect_id == 10 && ret == 0 && temp < 0){
			comp_level = 2*(int)comp_level/3;
			aw_info("The comp_level is:%d\n",comp_level);
			aw86927_i2c_write(aw86927, AW86927_REG_PLAYCFG2,(unsigned char)(comp_level));
			aw86927_i2c_read(aw86927,AW86927_REG_PLAYCFG2,&tep);
			aw_info("The AW86927_REG_PLAYCFG2 is:%d\n",tep);
		}else{
			aw86927_i2c_write(aw86927, AW86927_REG_PLAYCFG2, comp_level);
		}
	} else {
		aw_dbg("%s: disable compsensation, vbat=%d, vbat_min=%d, vbat_ref=%d",
			 __func__, aw86927->vbat,
			 AW86927_VBAT_MIN, AW86927_VBAT_REFER);
		aw86927_i2c_write(aw86927, AW86927_REG_PLAYCFG2,
				  aw86927->level);
	}
}

void aw86927_haptics_set_gain(struct input_dev *dev, u16 gain)
{
	struct aw86927 *aw86927 = input_get_drvdata(dev);

	aw_dbg("%s enter\n", __func__);
	aw86927->new_gain = gain;
	queue_work(aw86927->work_queue, &aw86927->set_gain_work);
}
