/*
 * aw86907.c
 *
 * Copyright (c) 2021 AWINIC Technology CO., LTD
 *
 * Author: <chelvming@awinic.com>
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
#include "aw_config.h"
#include "aw86907.h"


static struct aw86907_container *aw86907_rtp;
static struct pm_qos_request pm_qos_req_vb;
/******************************************************
 *
 * functions
 *
 ******************************************************/
static void aw86907_interrupt_clear(struct aw86907 *aw86907);
static void aw86907_haptic_bst_mode_config(struct aw86907 *aw86907,
					   unsigned char boost_mode);
static int aw86907_haptic_get_vbat(struct aw86907 *aw86907);

const unsigned char aw86907_reg_access[AW86907_REG_MAX] = {
	[AW86907_REG_ID] = REG_RD_ACCESS,
	[AW86907_REG_SYSST] = REG_RD_ACCESS,
	[AW86907_REG_SYSINT] = REG_RD_ACCESS,
	[AW86907_REG_SYSINTM] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_SYSST2] = REG_RD_ACCESS,
	[AW86907_REG_SYSER] = REG_RD_ACCESS,
	[AW86907_REG_PLAYCFG1] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_PLAYCFG2] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_PLAYCFG3] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_PLAYCFG4] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_WAVCFG1] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_WAVCFG2] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_WAVCFG3] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_WAVCFG4] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_WAVCFG5] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_WAVCFG6] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_WAVCFG7] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_WAVCFG8] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_WAVCFG9] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_WAVCFG10] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_WAVCFG11] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_WAVCFG12] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_WAVCFG13] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_WAVCFG14] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_CONTCFG1] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_CONTCFG2] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_CONTCFG3] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_CONTCFG4] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_CONTCFG5] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_CONTCFG6] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_CONTCFG7] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_CONTCFG8] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_CONTCFG9] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_CONTCFG10] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_CONTCFG11] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_CONTCFG12] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_CONTCFG13] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_CONTRD14] = REG_RD_ACCESS,
	[AW86907_REG_CONTRD15] = REG_RD_ACCESS,
	[AW86907_REG_CONTRD16] = REG_RD_ACCESS,
	[AW86907_REG_CONTRD17] = REG_RD_ACCESS,
	[AW86907_REG_CONTRD18] = REG_RD_ACCESS,
	[AW86907_REG_CONTRD19] = REG_RD_ACCESS,
	[AW86907_REG_CONTRD20] = REG_RD_ACCESS,
	[AW86907_REG_CONTRD21] = REG_RD_ACCESS,
	[AW86907_REG_RTPCFG1] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_RTPCFG2] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_RTPCFG3] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_RTPCFG4] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_RTPCFG5] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_RTPDATA] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_TRGCFG1] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_TRGCFG2] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_TRGCFG3] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_TRGCFG4] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_TRGCFG5] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_TRGCFG6] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_TRGCFG7] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_TRGCFG8] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_GLBCFG1] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_GLBCFG2] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_GLBCFG3] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_GLBCFG4] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_GLBRD5] = REG_RD_ACCESS,
	[AW86907_REG_RAMADDRH] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_RAMADDRL] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_RAMDATA] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_SYSCTRL1] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_SYSCTRL2] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_SYSCTRL3] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_SYSCTRL4] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_SYSCTRL5] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_SYSCTRL6] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_SYSCTRL7] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_I2SCFG1] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_I2SCFG2] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_PWMCFG1] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_PWMCFG2] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_PWMCFG3] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_PWMCFG4] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_DETCFG1] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_DETCFG2] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_DET_RL] = REG_RD_ACCESS,
	[AW86907_REG_DET_OS] = REG_RD_ACCESS,
	[AW86907_REG_DET_VBAT] = REG_RD_ACCESS,
	[AW86907_REG_DET_TEST] = REG_RD_ACCESS,
	[AW86907_REG_DET_LO] = REG_RD_ACCESS,
	[AW86907_REG_TRIMCFG1] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_TRIMCFG2] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_TRIMCFG3] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_TRIMCFG4] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_PLLCFG1] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_PLLCFG2] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_HDRVCFG1] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_IOCFG1] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_BEMFCFG1] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_BSTCFG1] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_BSTCFG2] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_BSTCFG3] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_BSTCFG4] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_BSTCFG5] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_CPCFG1] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_LDOCFG1] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_OCCFG1] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_ADCCFG1] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW86907_REG_D2SCFG1] = REG_RD_ACCESS | REG_WR_ACCESS,

};

 /******************************************************
 *
 * aw86907 i2c write/read
 *
 ******************************************************/
static int aw86907_i2c_write(struct aw86907 *aw86907,
			     unsigned char reg_addr, unsigned char reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < AW_I2C_RETRIES) {
		ret =
		    i2c_smbus_write_byte_data(aw86907->i2c, reg_addr, reg_data);
		if (ret < 0) {
			aw_err("%s: addr=0x%02X, data=0x%02X, cnt=%d, error=%d\n",
				   __func__, reg_addr, reg_data, cnt, ret);
		} else {
			break;
		}
		cnt++;
		usleep_range(AW_I2C_RETRY_DELAY * 1000,
			     AW_I2C_RETRY_DELAY * 1000 + 500);
	}
	return ret;
}

static int aw86907_i2c_read(struct aw86907 *aw86907,
			    unsigned char reg_addr, unsigned char *reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < AW_I2C_RETRIES) {
		ret = i2c_smbus_read_byte_data(aw86907->i2c, reg_addr);
		if (ret < 0) {
			aw_err("%s: addr=0x%02X, cnt=%d, error=%d\n",
				   __func__, reg_addr, cnt, ret);
		} else {
			*reg_data = ret;
			break;
		}
		cnt++;
		usleep_range(AW_I2C_RETRY_DELAY * 1000,
			     AW_I2C_RETRY_DELAY * 1000 + 500);
	}
	return ret;
}

static int aw86907_i2c_write_bits(struct aw86907 *aw86907,
				  unsigned char reg_addr, unsigned int mask,
				  unsigned char reg_data)
{
	int ret = -1;
	unsigned char reg_val = 0;

	ret = aw86907_i2c_read(aw86907, reg_addr, &reg_val);
	if (ret < 0) {
		aw_err("%s: i2c read error, ret=%d\n",
			   __func__, ret);
		return ret;
	}
	reg_val &= mask;
	reg_val |= (reg_data & (~mask));
	ret = aw86907_i2c_write(aw86907, reg_addr, reg_val);
	if (ret < 0) {
		aw_err("%s: i2c write error, ret=%d\n", __func__, ret);
		return ret;
	}
	return 0;
}

static int aw86907_i2c_writes(struct aw86907 *aw86907,
			      unsigned char reg_addr, unsigned char *buf,
			      unsigned int len)
{
	int ret = -1;
	unsigned char *data = NULL;

	data = kmalloc(len + 1, GFP_KERNEL);
	if (data == NULL) {
		aw_err("%s: can not allocate memory\n", __func__);
		return -ENOMEM;
	}
	data[0] = reg_addr;
	memcpy(&data[1], buf, len);
	ret = i2c_master_send(aw86907->i2c, data, len + 1);
	if (ret < 0)
		aw_err("%s: i2c master send error\n",
			   __func__);
	kfree(data);
	return ret;
}

int aw86907_i2c_reads(struct aw86907 *aw86907, unsigned char reg_addr,
		      unsigned char *buf, unsigned int len)
{
	int ret;
	struct i2c_msg msg[] = {
		[0] = {
			.addr = aw86907->i2c->addr,
			.flags = 0,
			.len = sizeof(uint8_t),
			.buf = &reg_addr,
			},
		[1] = {
			.addr = aw86907->i2c->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = buf,
			},
	};

	ret = i2c_transfer(aw86907->i2c->adapter, msg, ARRAY_SIZE(msg));
	if (ret < 0) {
		aw_err("%s: transfer failed.", __func__);
		return ret;
	} else if (ret != 2) {
		aw_err("%s: transfer failed(size error).", __func__);
		return -ENXIO;
	}

	return ret;
}


static void aw86907_haptic_raminit(struct aw86907 *aw86907, bool flag)
{
	if (flag) {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_SYSCTRL1,
				       AW86907_BIT_SYSCTRL1_RAMINIT_MASK,
				       AW86907_BIT_SYSCTRL1_RAMINIT_ON);
	} else {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_SYSCTRL1,
				       AW86907_BIT_SYSCTRL1_RAMINIT_MASK,
				       AW86907_BIT_SYSCTRL1_RAMINIT_OFF);
	}
}

static void aw86907_haptic_play_go(struct aw86907 *aw86907)
{
	if (aw86907->info.is_enabled_one_wire) {
		aw86907_i2c_write(aw86907, AW86907_REG_GLBCFG2,
				  AW86907_BIT_START_DLY_20US);
		aw86907_i2c_write(aw86907, AW86907_REG_PLAYCFG4,
				  AW86907_BIT_PLAYCFG4_GO_ON);
		mdelay(1);
		aw86907_i2c_write(aw86907, AW86907_REG_GLBCFG2,
				  AW86907_BIT_START_DLY_2P5MS);
	} else {
		aw86907_i2c_write(aw86907, AW86907_REG_PLAYCFG4,
				  AW86907_BIT_PLAYCFG4_GO_ON);
	}
}

static int aw86907_haptic_stop(struct aw86907 *aw86907)
{
	unsigned char cnt = 40;
	unsigned char reg_val = 0;
	bool force_flag = true;

	aw_info("%s enter\n", __func__);
	aw86907->play_mode = AW86907_HAPTIC_STANDBY_MODE;

	aw86907_i2c_write(aw86907, AW86907_REG_PLAYCFG4,
			  AW86907_BIT_PLAYCFG4_STOP_ON);
	while (cnt) {
		aw86907_i2c_read(aw86907, AW86907_REG_GLBRD5, &reg_val);
		if ((reg_val & 0x0f) == AW86907_BIT_GLBRD5_STATE_STANDBY
		    || (reg_val & 0x0f) ==
		    AW86907_BIT_GLBRD5_STATE_I2S_GO) {
			cnt = 0;
			force_flag = false;
			aw_info("%s entered standby! glb_state=0x%02X\n",
				    __func__, reg_val);
		} else {
			cnt--;
			aw_dbg
			    ("%s wait for standby, glb_state=0x%02X\n",
			     __func__, reg_val);
		}
		usleep_range(2000, 2500);
	}

	if (force_flag) {
		aw_err("%s force to enter standby mode!\n", __func__);
		aw86907_i2c_write_bits(aw86907, AW86907_REG_SYSCTRL2,
				       AW86907_BIT_SYSCTRL2_STANDBY_MASK,
				       AW86907_BIT_SYSCTRL2_STANDBY_ON);
		aw86907_i2c_write_bits(aw86907, AW86907_REG_SYSCTRL2,
				       AW86907_BIT_SYSCTRL2_STANDBY_MASK,
				       AW86907_BIT_SYSCTRL2_STANDBY_OFF);
	}
	return 0;
}

static int aw86907_haptic_get_ram_number(struct aw86907 *aw86907)
{
	unsigned char i = 0;
	unsigned char reg_val = 0;
	unsigned char ram_data[3];
	unsigned int first_wave_addr = 0;

	aw_info("%s enter!\n", __func__);
	if (!aw86907->ram_init) {
		aw_err("%s: ram init faild, ram_num = 0!\n",
			   __func__);
		return -EPERM;
	}

	mutex_lock(&aw86907->lock);
	/* RAMINIT Enable */
	aw86907_haptic_raminit(aw86907, true);
	aw86907_haptic_stop(aw86907);
	aw86907_i2c_write(aw86907, AW86907_REG_RAMADDRH,
			  (unsigned char)(aw86907->ram.base_addr >> 8));
	aw86907_i2c_write(aw86907, AW86907_REG_RAMADDRL,
			  (unsigned char)(aw86907->ram.base_addr & 0x00ff));
	for (i = 0; i < 3; i++) {
		aw86907_i2c_read(aw86907, AW86907_REG_RAMDATA, &reg_val);
		ram_data[i] = reg_val;
	}
	first_wave_addr = (ram_data[1] << 8 | ram_data[2]);
	aw86907->ram.ram_num =
			(first_wave_addr - aw86907->ram.base_addr - 1) / 4;
	aw_info("%s: ram_version = 0x%02x\n", __func__, ram_data[0]);
	aw_info("%s: first waveform addr = 0x%04x\n",
		    __func__, first_wave_addr);
	aw_info("%s: ram_num = %d\n", __func__, aw86907->ram.ram_num);
	/* RAMINIT Disable */
	aw86907_haptic_raminit(aw86907, false);
	mutex_unlock(&aw86907->lock);

	return 0;
}

#ifdef AW_CHECK_RAM_DATA
static int aw86907_check_ram_data(struct aw86907 *aw86907,
				  unsigned char *cont_data,
				  unsigned char *ram_data,
				  unsigned int len)
{
	int i = 0;

	for (i = 0; i < len; i++) {
		if (ram_data[i] != cont_data[i]) {
			aw_err("%s: check ramdata error, addr=0x%04x, ram_data=0x%02x, file_data=0x%02x\n",
			       __func__, i, ram_data[i], cont_data[i]);
			return -ERANGE;
		}
	}
	return 0;
}
#endif

static int aw86907_container_update(struct aw86907 *aw86907,
				     struct aw86907_container *aw86907_cont)
{
	int i = 0;
	unsigned int shift = 0;
	unsigned char reg_val = 0;
	unsigned int temp = 0;
	int ret = 0;
#ifdef AW_CHECK_RAM_DATA
	int len = 0;
	unsigned char ram_data[AW_RAMDATA_RD_BUFFER_SIZE] = {0};
#endif
	aw_info("%s enter\n", __func__);
	mutex_lock(&aw86907->lock);
	aw86907->ram.baseaddr_shift = 2;
	aw86907->ram.ram_shift = 4;
	/* RAMINIT Enable */
	aw86907_haptic_raminit(aw86907, true);
	/* Enter standby mode */
	aw86907_haptic_stop(aw86907);
	/* base addr */
	shift = aw86907->ram.baseaddr_shift;
	aw86907->ram.base_addr =
	    (unsigned int)((aw86907_cont->data[0 + shift] << 8) |
			   (aw86907_cont->data[1 + shift]));
	aw_info("%s: base_addr = %d\n", __func__, aw86907->ram.base_addr);

	aw86907_i2c_write(aw86907, AW86907_REG_RTPCFG1, /*ADDRH*/
			  aw86907_cont->data[0 + shift]);
	aw86907_i2c_write(aw86907, AW86907_REG_RTPCFG2, /*ADDRL*/
			  aw86907_cont->data[1 + shift]);
	/* FIFO_AEH */
	aw86907_i2c_write_bits(aw86907, AW86907_REG_RTPCFG3,
			       AW86907_BIT_RTPCFG3_FIFO_AEH_MASK,
			       (unsigned
				char)(((aw86907->
					ram.base_addr >> 1) >> 4) & 0xF0));
	/* FIFO AEL */
	aw86907_i2c_write(aw86907, AW86907_REG_RTPCFG4,
			  (unsigned
			   char)(((aw86907->ram.base_addr >> 1) & 0x00FF)));
	/* FIFO_AFH */
	aw86907_i2c_write_bits(aw86907, AW86907_REG_RTPCFG3,
			       AW86907_BIT_RTPCFG3_FIFO_AFH_MASK,
			       (unsigned char)(((aw86907->ram.base_addr -
						 (aw86907->
						  ram.base_addr >> 2)) >> 8) &
					       0x0F));
	/* FIFO_AFL */
	aw86907_i2c_write(aw86907, AW86907_REG_RTPCFG5,
			  (unsigned char)(((aw86907->ram.base_addr -
					    (aw86907->
					     ram.base_addr >> 2)) & 0x00FF)));
/*
*	unsigned int temp
*	HIGH<byte4 byte3 byte2 byte1>LOW
*	|_ _ _ _AF-12BIT_ _ _ _AE-12BIT|
*/
	aw86907_i2c_read(aw86907, AW86907_REG_RTPCFG3, &reg_val);
	temp = ((reg_val & 0x0f) << 24) | ((reg_val & 0xf0) << 4);
	aw86907_i2c_read(aw86907, AW86907_REG_RTPCFG4, &reg_val);
	temp = temp | reg_val;
	aw_info("%s: almost_empty_threshold = %d\n", __func__,
		    (unsigned short)temp);
	aw86907_i2c_read(aw86907, AW86907_REG_RTPCFG5, &reg_val);
	temp = temp | (reg_val << 16);
	aw_info("%s: almost_full_threshold = %d\n", __func__, temp >> 16);
	/* ram */
	shift = aw86907->ram.baseaddr_shift;
	aw86907_i2c_write_bits(aw86907, AW86907_REG_RAMADDRH,
			       AW86907_BIT_RAMADDRH_MASK,
			       aw86907_cont->data[0 + shift]);
	aw86907_i2c_write(aw86907, AW86907_REG_RAMADDRL,
			  aw86907_cont->data[1 + shift]);
	i = aw86907->ram.ram_shift;
	while(i < aw86907_cont->len) {
		if((aw86907_cont->len - i) < AW_RAMDATA_WR_BUFFER_SIZE)
			len = aw86907_cont->len - i;
		else
			len = AW_RAMDATA_WR_BUFFER_SIZE;
		aw86907_i2c_writes(aw86907, AW86907_REG_RAMDATA,
				   &aw86907_cont->data[i], len);
		i += len;
	}

#ifdef AW_CHECK_RAM_DATA
	shift = aw86907->ram.baseaddr_shift;
	aw86907_i2c_write_bits(aw86907, AW86907_REG_RAMADDRH,
			       AW86907_BIT_RAMADDRH_MASK,
			       aw86907_cont->data[0 + shift]);
	aw86907_i2c_write(aw86907, AW86907_REG_RAMADDRL,
			  aw86907_cont->data[1 + shift]);

	i = aw86907->ram.ram_shift;
	while (i < aw86907_cont->len) {
		if ((aw86907_cont->len - i) < AW_RAMDATA_RD_BUFFER_SIZE)
			len = aw86907_cont->len - i;
		else
			len = AW_RAMDATA_RD_BUFFER_SIZE;

		aw86907_i2c_reads(aw86907, AW86907_REG_RAMDATA, ram_data, len);
		ret = aw86907_check_ram_data(aw86907, &aw86907_cont->data[i],
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
	aw86907_haptic_raminit(aw86907, false);
	mutex_unlock(&aw86907->lock);
	aw_info("%s exit\n", __func__);

	return ret;
}

static void aw86907_ram_loaded(const struct firmware *cont, void *context)
{
	struct aw86907 *aw86907 = context;
	struct aw86907_container *aw86907_fw;
	int i = 0;
	int ret = 0;
	unsigned short check_sum = 0;

	aw_info("%s enter\n", __func__);
	if (!cont) {
		aw_err("%s: failed to read %s\n", __func__,
			   awinic_ram_name);
		release_firmware(cont);
		return;
	}
	aw_info("%s: loaded %s - size: %zu bytes\n", __func__,
		    awinic_ram_name, cont ? cont->size : 0);

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
	aw_info("%s: check sum pass: 0x%04x\n", __func__, check_sum);
	aw86907->ram.check_sum = check_sum;

	/* aw86907 ram update less then 128kB */
	aw86907_fw = kzalloc(cont->size + sizeof(int), GFP_KERNEL);
	if (!aw86907_fw) {
		release_firmware(cont);
		aw_err("%s: Error allocating memory\n", __func__);
		return;
	}
	aw86907_fw->len = cont->size;
	memcpy(aw86907_fw->data, cont->data, cont->size);
	release_firmware(cont);
	ret = aw86907_container_update(aw86907, aw86907_fw);
	if (ret) {
		aw_err("%s: ram firmware update failed!\n",  __func__);
	} else {
		aw86907->ram_init = 1;
		aw_info("%s: ram firmware update complete!\n", __func__);
	}
	aw86907_haptic_get_ram_number(aw86907);
	aw86907->ram.len = aw86907_fw->len - aw86907->ram.ram_shift;
	kfree(aw86907_fw);
}

static int aw86907_ram_update(struct aw86907 *aw86907)
{
	aw86907->ram_init = 0;
	aw86907->rtp_init = 0;

	return request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
				       awinic_ram_name, aw86907->dev,
				       GFP_KERNEL, aw86907, aw86907_ram_loaded);
}

static void aw86907_ram_work_routine(struct work_struct *work)
{
	struct aw86907 *aw86907 = container_of(work, struct aw86907,
					       ram_work.work);

	aw_info("%s enter\n", __func__);
	aw86907_ram_update(aw86907);
}

int aw86907_ram_init(struct aw86907 *aw86907)
{
#ifdef AW_RAM_UPDATE_DELAY
	int ram_timer_val = 5000;

	aw_info("%s enter\n", __func__);
	INIT_DELAYED_WORK(&aw86907->ram_work, aw86907_ram_work_routine);
	queue_delayed_work(aw86907->work_queue, &aw86907->ram_work,
			   msecs_to_jiffies(ram_timer_val));
#else
	aw86907_ram_update(aw86907);
#endif
	return 0;
}

/*****************************************************
 *
 * haptic control
 *
 *****************************************************/

static int aw86907_haptic_play_mode(struct aw86907 *aw86907,
				    unsigned char play_mode)
{
	aw_info("%s enter\n", __func__);

	switch (play_mode) {
	case AW86907_HAPTIC_STANDBY_MODE:
		aw_info("%s: enter standby mode\n", __func__);
		aw86907->play_mode = AW86907_HAPTIC_STANDBY_MODE;
		aw86907_haptic_stop(aw86907);
		break;
	case AW86907_HAPTIC_RAM_MODE:
		aw_info("%s: enter ram mode\n", __func__);
		aw86907->play_mode = AW86907_HAPTIC_RAM_MODE;
		aw86907_i2c_write_bits(aw86907, AW86907_REG_PLAYCFG3,
				       AW86907_BIT_PLAYCFG3_PLAY_MODE_MASK,
				       AW86907_BIT_PLAYCFG3_PLAY_MODE_RAM);
		/* bst mode */
		aw86907_haptic_bst_mode_config(aw86907,
					       AW86907_HAPTIC_BST_MODE_BOOST);
		break;
	case AW86907_HAPTIC_RAM_LOOP_MODE:
		aw_info("%s: enter ram loop mode\n",
			    __func__);
		aw86907->play_mode = AW86907_HAPTIC_RAM_LOOP_MODE;
		aw86907_i2c_write_bits(aw86907, AW86907_REG_PLAYCFG3,
				       AW86907_BIT_PLAYCFG3_PLAY_MODE_MASK,
				       AW86907_BIT_PLAYCFG3_PLAY_MODE_RAM);
		/* bst mode */
		aw86907_haptic_bst_mode_config(aw86907,
					       AW86907_HAPTIC_BST_MODE_BYPASS);
		break;
	case AW86907_HAPTIC_RTP_MODE:
		aw_info("%s: enter rtp mode\n", __func__);
		aw86907->play_mode = AW86907_HAPTIC_RTP_MODE;
		aw86907_i2c_write_bits(aw86907, AW86907_REG_PLAYCFG3,
				       AW86907_BIT_PLAYCFG3_PLAY_MODE_MASK,
				       AW86907_BIT_PLAYCFG3_PLAY_MODE_RTP);
		/* bst mode */
		aw86907_haptic_bst_mode_config(aw86907,
					       AW86907_HAPTIC_BST_MODE_BOOST);
		break;
	case AW86907_HAPTIC_TRIG_MODE:
		aw_info("%s: enter trig mode\n", __func__);
		aw86907->play_mode = AW86907_HAPTIC_TRIG_MODE;
		aw86907_i2c_write_bits(aw86907, AW86907_REG_PLAYCFG3,
				       AW86907_BIT_PLAYCFG3_PLAY_MODE_MASK,
				       AW86907_BIT_PLAYCFG3_PLAY_MODE_RAM);
		break;
	case AW86907_HAPTIC_CONT_MODE:
		aw_info("%s: enter cont mode\n", __func__);
		aw86907->play_mode = AW86907_HAPTIC_CONT_MODE;
		aw86907_i2c_write_bits(aw86907, AW86907_REG_PLAYCFG3,
				       AW86907_BIT_PLAYCFG3_PLAY_MODE_MASK,
				       AW86907_BIT_PLAYCFG3_PLAY_MODE_CONT);
		/* bst mode */
		aw86907_haptic_bst_mode_config(aw86907,
					       AW86907_HAPTIC_BST_MODE_BYPASS);
		break;
	default:
		aw_err("%s: play mode %d error", __func__, play_mode);
		break;
	}
	return 0;
}

static int aw86907_haptic_set_wav_seq(struct aw86907 *aw86907,
				      unsigned char wav, unsigned char seq)
{
	aw86907_i2c_write(aw86907, AW86907_REG_WAVCFG1 + wav, seq);
	return 0;
}

static int aw86907_haptic_set_wav_loop(struct aw86907 *aw86907,
				       unsigned char wav, unsigned char loop)
{
	unsigned char tmp = 0;

	if (wav % 2) {
		tmp = loop << 0;
		aw86907_i2c_write_bits(aw86907, AW86907_REG_WAVCFG9 + (wav / 2),
				       AW86907_BIT_WAVLOOP_SEQ_EVEN_MASK, tmp);
	} else {
		tmp = loop << 4;
		aw86907_i2c_write_bits(aw86907, AW86907_REG_WAVCFG9 + (wav / 2),
				       AW86907_BIT_WAVLOOP_SEQ_ODD_MASK, tmp);
	}
	return 0;
}

static void aw86907_haptic_set_repeat_wav_seq(struct aw86907 *aw86907,
					      unsigned char seq)
{
	aw86907_haptic_set_wav_seq(aw86907, 0x00, seq);
	aw86907_haptic_set_wav_loop(aw86907, 0x00,
				    AW86907_BIT_WAVLOOP_INIFINITELY);
}

static int aw86907_haptic_set_bst_vol(struct aw86907 *aw86907,
				      unsigned char bst_vol)
{
	if (bst_vol & 0xc0)
		bst_vol = 0x3f;
	aw86907_i2c_write_bits(aw86907, AW86907_REG_PLAYCFG1,
			       AW86907_BIT_PLAYCFG1_BST_VOUT_RDA_MASK, bst_vol);
	return 0;
}

static int aw86907_haptic_set_bst_peak_cur(struct aw86907 *aw86907,
					   unsigned char peak_cur)
{
	aw_info("%s enter!\n", __func__);
	if (peak_cur > AW86907_BSTCFG_PEAKCUR_LIMIT)
		peak_cur = AW86907_BSTCFG_PEAKCUR_LIMIT;
	aw86907_i2c_write_bits(aw86907, AW86907_REG_BSTCFG1,
			      AW86907_BIT_BSTCFG1_BST_PC_MASK, peak_cur);

	return 0;
}

static int aw86907_haptic_set_gain(struct aw86907 *aw86907, unsigned char gain)
{
	unsigned char temp_gain = 0;

	if (aw86907->ram_vbat_comp == AW86907_HAPTIC_RAM_VBAT_COMP_ENABLE) {
		aw86907_haptic_get_vbat(aw86907);
		temp_gain = aw86907->gain * AW_VBAT_REFER / aw86907->vbat;
		if (temp_gain >
		    (128 * AW_VBAT_REFER / AW_VBAT_MIN)) {
			temp_gain =
			    128 * AW_VBAT_REFER / AW_VBAT_MIN;
			aw_info("%s gain limit=%d\n", __func__, temp_gain);
		}
		aw86907_i2c_write(aw86907, AW86907_REG_PLAYCFG2, temp_gain);
	} else {
		aw86907_i2c_write(aw86907, AW86907_REG_PLAYCFG2, gain);
	}
	return 0;
}

static int aw86907_haptic_set_pwm(struct aw86907 *aw86907, unsigned char mode)
{
	switch (mode) {
	case AW86907_PWM_48K:
		aw86907_i2c_write_bits(aw86907, AW86907_REG_SYSCTRL2,
				       AW86907_BIT_SYSCTRL2_WAVDAT_MODE_MASK,
				       AW86907_BIT_SYSCTRL2_RATE_48K);
		break;
	case AW86907_PWM_24K:
		aw86907_i2c_write_bits(aw86907, AW86907_REG_SYSCTRL2,
				       AW86907_BIT_SYSCTRL2_WAVDAT_MODE_MASK,
				       AW86907_BIT_SYSCTRL2_RATE_24K);
		break;
	case AW86907_PWM_12K:
		aw86907_i2c_write_bits(aw86907, AW86907_REG_SYSCTRL2,
				       AW86907_BIT_SYSCTRL2_WAVDAT_MODE_MASK,
				       AW86907_BIT_SYSCTRL2_RATE_12K);
		break;
	default:
		break;
	}
	return 0;
}

/*****************************************************
 *
 * motor protect
 *
 *****************************************************/
static int aw86907_haptic_swicth_motor_protect_config(struct aw86907 *aw86907,
						      unsigned char addr,
						      unsigned char val)
{
	aw_info("%s enter\n", __func__);
	if (addr == 1) {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_DETCFG1,
				       AW86907_BIT_DETCFG1_PRCT_MODE_MASK,
				       AW86907_BIT_DETCFG1_PRCT_MODE_VALID);
		aw86907_i2c_write_bits(aw86907, AW86907_REG_PWMCFG1,
				       AW86907_BIT_PWMCFG1_PRC_EN_MASK,
				       AW86907_BIT_PWMCFG1_PRC_ENABLE);
		aw86907_i2c_write_bits(aw86907, AW86907_REG_PWMCFG3,
				       AW86907_BIT_PWMCFG3_PR_EN_MASK,
				       AW86907_BIT_PWMCFG3_PR_ENABLE);
	} else if (addr == 0) {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_DETCFG1,
				       AW86907_BIT_DETCFG1_PRCT_MODE_MASK,
				       AW86907_BIT_DETCFG1_PRCT_MODE_INVALID);
		aw86907_i2c_write_bits(aw86907, AW86907_REG_PWMCFG1,
				       AW86907_BIT_PWMCFG1_PRC_EN_MASK,
				       AW86907_BIT_PWMCFG1_PRC_DISABLE);
		aw86907_i2c_write_bits(aw86907, AW86907_REG_PWMCFG3,
				       AW86907_BIT_PWMCFG3_PR_EN_MASK,
				       AW86907_BIT_PWMCFG3_PR_DISABLE);
	} else if (addr == 0x2d) {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_PWMCFG1,
				       AW86907_BIT_PWMCFG1_PRCTIME_MASK, val);
	} else if (addr == 0x3e) {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_PWMCFG3,
				       AW86907_BIT_PWMCFG3_PRLVL_MASK, val);
	} else if (addr == 0x3f) {
		aw86907_i2c_write(aw86907, AW86907_REG_PWMCFG4, val);
	}
	return 0;
}

/*****************************************************
 *
 * offset calibration
 *
 *****************************************************/
static int aw86907_haptic_offset_calibration(struct aw86907 *aw86907)
{
	unsigned int cont = 2000;
	unsigned char reg_val = 0;

	aw_info("%s enter\n", __func__);

	aw86907_haptic_raminit(aw86907, true);

	aw86907_i2c_write_bits(aw86907, AW86907_REG_DETCFG2,
			       AW86907_BIT_DETCFG2_DIAG_GO_MASK,
			       AW86907_BIT_DETCFG2_DIAG_GO_ON);
	while (1) {
		aw86907_i2c_read(aw86907, AW86907_REG_DETCFG2, &reg_val);
		if ((reg_val & 0x01) == 0 || cont == 0)
			break;
		cont--;
	}
	if (cont == 0)
		aw_err("%s calibration offset failed!\n",  __func__);
	aw86907_haptic_raminit(aw86907, false);
	return 0;
}

/*****************************************************
 *
 * trig config
 *
 *****************************************************/
static void aw86907_haptic_trig1_param_init(struct aw86907 *aw86907)
{
	aw_info("%s enter\n", __func__);

	aw86907->trig[0].trig_level = aw86907->info.trig_config[0];
	aw86907->trig[0].trig_polar = aw86907->info.trig_config[1];
	aw86907->trig[0].pos_enable = aw86907->info.trig_config[2];
	aw86907->trig[0].pos_sequence = aw86907->info.trig_config[3];
	aw86907->trig[0].neg_enable = aw86907->info.trig_config[4];
	aw86907->trig[0].neg_sequence = aw86907->info.trig_config[5];
	aw86907->trig[0].trig_brk = aw86907->info.trig_config[6];
	aw86907->trig[0].trig_bst = aw86907->info.trig_config[7];
}

static void aw86907_haptic_trig2_param_init(struct aw86907 *aw86907)
{
	aw_info("%s enter\n", __func__);

	aw86907->trig[1].trig_level = aw86907->info.trig_config[8 + 0];
	aw86907->trig[1].trig_polar = aw86907->info.trig_config[8 + 1];
	aw86907->trig[1].pos_enable = aw86907->info.trig_config[8 + 2];
	aw86907->trig[1].pos_sequence = aw86907->info.trig_config[8 + 3];
	aw86907->trig[1].neg_enable = aw86907->info.trig_config[8 + 4];
	aw86907->trig[1].neg_sequence = aw86907->info.trig_config[8 + 5];
	aw86907->trig[1].trig_brk = aw86907->info.trig_config[8 + 6];
	aw86907->trig[1].trig_bst = aw86907->info.trig_config[8 + 7];
}

static void aw86907_haptic_trig3_param_init(struct aw86907 *aw86907)
{
	aw_info("%s enter\n", __func__);
	aw86907->trig[2].trig_level = aw86907->info.trig_config[16 + 0];
	aw86907->trig[2].trig_polar = aw86907->info.trig_config[16 + 1];
	aw86907->trig[2].pos_enable = aw86907->info.trig_config[16 + 2];
	aw86907->trig[2].pos_sequence = aw86907->info.trig_config[16 + 3];
	aw86907->trig[2].neg_enable = aw86907->info.trig_config[16 + 4];
	aw86907->trig[2].neg_sequence = aw86907->info.trig_config[16 + 5];
	aw86907->trig[2].trig_brk = aw86907->info.trig_config[16 + 6];
	aw86907->trig[2].trig_bst = aw86907->info.trig_config[16 + 7];
}

static void aw86907_haptic_trig1_param_config(struct aw86907 *aw86907)
{
	aw_info("%s enter\n", __func__);
	if (aw86907->trig[0].trig_level) {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_TRGCFG7,
				       AW86907_BIT_TRGCFG7_TRG1_MODE_MASK,
				       AW86907_BIT_TRGCFG7_TRG1_MODE_LEVEL);
	} else {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_TRGCFG7,
				       AW86907_BIT_TRGCFG7_TRG1_MODE_MASK,
				       AW86907_BIT_TRGCFG7_TRG1_MODE_EDGE);
	}
	if (aw86907->trig[0].trig_polar) {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_TRGCFG7,
				       AW86907_BIT_TRGCFG7_TRG1_POLAR_MASK,
				       AW86907_BIT_TRGCFG7_TRG1_POLAR_NEG);
	} else {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_TRGCFG7,
				       AW86907_BIT_TRGCFG7_TRG1_POLAR_MASK,
				       AW86907_BIT_TRGCFG7_TRG1_POLAR_POS);
	}
	if (aw86907->trig[0].pos_enable) {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_TRGCFG1,
				       AW86907_BIT_TRG_ENABLE_MASK,
				       AW86907_BIT_TRG_ENABLE);
	} else {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_TRGCFG1,
				       AW86907_BIT_TRG_ENABLE_MASK,
				       AW86907_BIT_TRG_DISABLE);
	}
	if (aw86907->trig[0].neg_enable) {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_TRGCFG4,
				       AW86907_BIT_TRG_ENABLE_MASK,
				       AW86907_BIT_TRG_ENABLE);
	} else {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_TRGCFG4,
				       AW86907_BIT_TRG_ENABLE_MASK,
				       AW86907_BIT_TRG_DISABLE);
	}
	if (aw86907->trig[0].pos_sequence) {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_TRGCFG1,
				       AW86907_BIT_TRG_SEQ_MASK,
				       aw86907->trig[0].pos_sequence);
	}
	if (aw86907->trig[0].neg_sequence) {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_TRGCFG4,
				       AW86907_BIT_TRG_SEQ_MASK,
				       aw86907->trig[0].neg_sequence);
	}
	if (aw86907->trig[0].trig_brk) {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_TRGCFG7,
				AW86907_BIT_TRGCFG7_TRG1_AUTO_BRK_MASK,
				AW86907_BIT_TRGCFG7_TRG1_AUTO_BRK_ENABLE);
	} else {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_TRGCFG7,
				AW86907_BIT_TRGCFG7_TRG1_AUTO_BRK_MASK,
				AW86907_BIT_TRGCFG7_TRG1_AUTO_BRK_DISABLE);
	}
	if (aw86907->trig[0].trig_bst) {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_TRGCFG7,
				       AW86907_BIT_TRGCFG7_TRG1_BST_MASK,
				       AW86907_BIT_TRGCFG7_TRG1_BST_ENABLE);
	} else {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_TRGCFG7,
				       AW86907_BIT_TRGCFG7_TRG1_BST_MASK,
				       AW86907_BIT_TRGCFG7_TRG1_BST_DISABLE);
	}
}

static void aw86907_haptic_trig2_param_config(struct aw86907 *aw86907)
{
	aw_info("%s enter\n", __func__);
	if (aw86907->trig[1].trig_level) {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_TRGCFG7,
				       AW86907_BIT_TRGCFG7_TRG2_MODE_MASK,
				       AW86907_BIT_TRGCFG7_TRG2_MODE_LEVEL);
	} else {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_TRGCFG7,
				       AW86907_BIT_TRGCFG7_TRG2_MODE_MASK,
				       AW86907_BIT_TRGCFG7_TRG2_MODE_EDGE);
	}
	if (aw86907->trig[1].trig_polar) {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_TRGCFG7,
				       AW86907_BIT_TRGCFG7_TRG2_POLAR_MASK,
				       AW86907_BIT_TRGCFG7_TRG2_POLAR_NEG);
	} else {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_TRGCFG7,
				       AW86907_BIT_TRGCFG7_TRG2_POLAR_MASK,
				       AW86907_BIT_TRGCFG7_TRG2_POLAR_POS);
	}
	if (aw86907->trig[1].pos_enable) {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_TRGCFG2,
				       AW86907_BIT_TRG_ENABLE_MASK,
				       AW86907_BIT_TRG_ENABLE);
	} else {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_TRGCFG2,
				       AW86907_BIT_TRG_ENABLE_MASK,
				       AW86907_BIT_TRG_DISABLE);
	}
	if (aw86907->trig[1].neg_enable) {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_TRGCFG5,
				       AW86907_BIT_TRG_ENABLE_MASK,
				       AW86907_BIT_TRG_ENABLE);
	} else {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_TRGCFG5,
				       AW86907_BIT_TRG_ENABLE_MASK,
				       AW86907_BIT_TRG_DISABLE);
	}
	if (aw86907->trig[1].pos_sequence) {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_TRGCFG2,
				       AW86907_BIT_TRG_SEQ_MASK,
				       aw86907->trig[1].pos_sequence);
	}
	if (aw86907->trig[1].neg_sequence) {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_TRGCFG5,
				       AW86907_BIT_TRG_SEQ_MASK,
				       aw86907->trig[1].neg_sequence);
	}
	if (aw86907->trig[1].trig_brk) {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_TRGCFG7,
				AW86907_BIT_TRGCFG7_TRG2_AUTO_BRK_MASK,
				AW86907_BIT_TRGCFG7_TRG2_AUTO_BRK_ENABLE);
	} else {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_TRGCFG7,
				AW86907_BIT_TRGCFG7_TRG2_AUTO_BRK_MASK,
				AW86907_BIT_TRGCFG7_TRG2_AUTO_BRK_DISABLE);
	}
	if (aw86907->trig[1].trig_bst) {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_TRGCFG7,
				       AW86907_BIT_TRGCFG7_TRG2_BST_MASK,
				       AW86907_BIT_TRGCFG7_TRG2_BST_ENABLE);
	} else {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_TRGCFG7,
				       AW86907_BIT_TRGCFG7_TRG2_BST_MASK,
				       AW86907_BIT_TRGCFG7_TRG2_BST_DISABLE);
	}
}

static void aw86907_haptic_trig3_param_config(struct aw86907 *aw86907)
{
	aw_info("%s enter\n", __func__);
	if (aw86907->trig[2].trig_level) {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_TRGCFG8,
				       AW86907_BIT_TRGCFG8_TRG3_MODE_MASK,
				       AW86907_BIT_TRGCFG8_TRG3_MODE_LEVEL);
	} else {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_TRGCFG8,
				       AW86907_BIT_TRGCFG8_TRG3_MODE_MASK,
				       AW86907_BIT_TRGCFG8_TRG3_MODE_EDGE);
	}
	if (aw86907->trig[2].trig_polar) {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_TRGCFG8,
				       AW86907_BIT_TRGCFG8_TRG3_POLAR_MASK,
				       AW86907_BIT_TRGCFG8_TRG3_POLAR_NEG);
	} else {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_TRGCFG8,
				       AW86907_BIT_TRGCFG8_TRG3_POLAR_MASK,
				       AW86907_BIT_TRGCFG8_TRG3_POLAR_POS);
	}
	if (aw86907->trig[2].pos_enable) {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_TRGCFG3,
				       AW86907_BIT_TRG_ENABLE_MASK,
				       AW86907_BIT_TRG_ENABLE);
	} else {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_TRGCFG3,
				       AW86907_BIT_TRG_ENABLE_MASK,
				       AW86907_BIT_TRG_DISABLE);
	}
	if (aw86907->trig[2].neg_enable) {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_TRGCFG6,
				       AW86907_BIT_TRG_ENABLE_MASK,
				       AW86907_BIT_TRG_ENABLE);
	} else {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_TRGCFG6,
				       AW86907_BIT_TRG_ENABLE_MASK,
				       AW86907_BIT_TRG_DISABLE);
	}
	if (aw86907->trig[2].pos_sequence) {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_TRGCFG3,
				       AW86907_BIT_TRG_SEQ_MASK,
				       aw86907->trig[2].pos_sequence);
	}
	if (aw86907->trig[2].neg_sequence) {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_TRGCFG6,
				       AW86907_BIT_TRG_SEQ_MASK,
				       aw86907->trig[2].neg_sequence);
	}
	if (aw86907->trig[2].trig_brk) {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_TRGCFG8,
				AW86907_BIT_TRGCFG8_TRG3_AUTO_BRK_MASK,
				AW86907_BIT_TRGCFG8_TRG3_AUTO_BRK_ENABLE);
	} else {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_TRGCFG8,
				AW86907_BIT_TRGCFG8_TRG3_AUTO_BRK_MASK,
				AW86907_BIT_TRGCFG8_TRG3_AUTO_BRK_DISABLE);
	}
	if (aw86907->trig[2].trig_bst) {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_TRGCFG8,
				       AW86907_BIT_TRGCFG8_TRG3_BST_MASK,
				       AW86907_BIT_TRGCFG8_TRG3_BST_ENABLE);
	} else {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_TRGCFG8,
				       AW86907_BIT_TRGCFG8_TRG3_BST_MASK,
				       AW86907_BIT_TRGCFG8_TRG3_BST_DISABLE);
	}
}

static void aw86907_haptic_bst_mode_config(struct aw86907 *aw86907,
					   unsigned char boost_mode)
{
	aw86907->boost_mode = boost_mode;

	switch (boost_mode) {
	case AW86907_HAPTIC_BST_MODE_BOOST:
		aw_info("%s haptic boost mode = boost\n", __func__);
		aw86907_i2c_write_bits(aw86907, AW86907_REG_PLAYCFG1,
				       AW86907_BIT_PLAYCFG1_BST_MODE_MASK,
				       AW86907_BIT_PLAYCFG1_BST_MODE_BOOST);
		break;
	case AW86907_HAPTIC_BST_MODE_BYPASS:
		aw_info("%s haptic boost mode = bypass\n", __func__);
		aw86907_i2c_write_bits(aw86907, AW86907_REG_PLAYCFG1,
				       AW86907_BIT_PLAYCFG1_BST_MODE_MASK,
				       AW86907_BIT_PLAYCFG1_BST_MODE_BYPASS);
		break;
	default:
		aw_err("%s: boost_mode = %d error", __func__, boost_mode);
		break;
	}
}

static int aw86907_haptic_auto_bst_enable(struct aw86907 *aw86907,
					  unsigned char flag)
{
	aw86907->auto_boost = flag;
	if (flag) {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_PLAYCFG3,
				       AW86907_BIT_PLAYCFG3_AUTO_BST_MASK,
				       AW86907_BIT_PLAYCFG3_AUTO_BST_ENABLE);
	} else {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_PLAYCFG3,
				       AW86907_BIT_PLAYCFG3_AUTO_BST_MASK,
				       AW86907_BIT_PLAYCFG3_AUTO_BST_DISABLE);
	}
	return 0;
}

/*****************************************************
 *
 * vbat mode
 *
 *****************************************************/
static int aw86907_haptic_vbat_mode_config(struct aw86907 *aw86907,
					   unsigned char flag)
{
	if (flag == AW86907_HAPTIC_CONT_VBAT_HW_ADJUST_MODE) {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_SYSCTRL1,
				       AW86907_BIT_SYSCTRL1_VBAT_MODE_MASK,
				       AW86907_BIT_SYSCTRL1_VBAT_MODE_HW);
	} else {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_SYSCTRL1,
				       AW86907_BIT_SYSCTRL1_VBAT_MODE_MASK,
				       AW86907_BIT_SYSCTRL1_VBAT_MODE_SW);
	}
	return 0;
}

static int aw86907_haptic_get_vbat(struct aw86907 *aw86907)
{
	unsigned char reg_val = 0;
	unsigned int vbat_code = 0;
	unsigned int cont = 2000;

	aw86907_haptic_stop(aw86907);
	aw86907_haptic_raminit(aw86907, true);
	aw86907_i2c_write_bits(aw86907, AW86907_REG_DETCFG2,
			       AW86907_BIT_DETCFG2_VBAT_GO_MASK,
			       AW86907_BIT_DETCFG2_VABT_GO_ON);

	while (1) {
		aw86907_i2c_read(aw86907, AW86907_REG_DETCFG2, &reg_val);
		if ((reg_val & 0x02) == 0 || cont == 0)
			break;
		cont--;
	}

	aw86907_i2c_read(aw86907, AW86907_REG_DET_VBAT, &reg_val);
	vbat_code = (vbat_code | reg_val) << 2;
	aw86907_i2c_read(aw86907, AW86907_REG_DET_LO, &reg_val);
	vbat_code = vbat_code | ((reg_val & 0x30) >> 4);
	aw86907->vbat = 6100 * vbat_code / 1024;
	if (aw86907->vbat > AW_VBAT_MAX) {
		aw86907->vbat = AW_VBAT_MAX;
		aw_info("%s vbat max limit = %dmV\n", __func__, aw86907->vbat);
	}
	if (aw86907->vbat < AW_VBAT_MIN) {
		aw86907->vbat = AW_VBAT_MIN;
		aw_info("%s vbat min limit = %dmV\n", __func__, aw86907->vbat);
	}
	aw_info("%s aw86907->vbat=%dmV, vbat_code=0x%02X\n",
		    __func__, aw86907->vbat, vbat_code);
	aw86907_haptic_raminit(aw86907, false);
	return 0;
}

static int aw86907_haptic_ram_vbat_comp(struct aw86907 *aw86907, bool flag)
{
	aw_dbg("%s: enter\n", __func__);
	if (flag)
		aw86907->ram_vbat_comp = AW86907_HAPTIC_RAM_VBAT_COMP_ENABLE;
	else
		aw86907->ram_vbat_comp = AW86907_HAPTIC_RAM_VBAT_COMP_DISABLE;
	return 0;
}

static int aw86907_haptic_get_lra_resistance(struct aw86907 *aw86907)
{
	unsigned char reg_val = 0;
	unsigned char d2s_gain_temp = 0;
	unsigned int lra_code = 0;

	mutex_lock(&aw86907->lock);
	aw86907_haptic_stop(aw86907);
	aw86907_i2c_read(aw86907, AW86907_REG_SYSCTRL7, &reg_val);
	d2s_gain_temp = 0x07 & reg_val;
	aw86907_i2c_write_bits(aw86907, AW86907_REG_SYSCTRL7,
			       AW86907_BIT_SYSCTRL7_D2S_GAIN_MASK,
			       aw86907->info.d2s_gain);
	aw86907_i2c_read(aw86907, AW86907_REG_SYSCTRL7, &reg_val);
	aw_info("%s: d2s_gain=%d\n", __func__, 0x07 & reg_val);

	aw86907_haptic_raminit(aw86907, true);
	/* enter standby mode */
	aw86907_haptic_stop(aw86907);
	usleep_range(2000, 2500);
	aw86907_i2c_write_bits(aw86907, AW86907_REG_SYSCTRL2,
			       AW86907_BIT_SYSCTRL2_STANDBY_MASK,
			       AW86907_BIT_SYSCTRL2_STANDBY_OFF);
	aw86907_i2c_write_bits(aw86907, AW86907_REG_DETCFG1,
			       AW86907_BIT_DETCFG1_RL_OS_MASK,
			       AW86907_BIT_DETCFG1_RL);
	aw86907_i2c_write_bits(aw86907, AW86907_REG_DETCFG2,
			       AW86907_BIT_DETCFG2_DIAG_GO_MASK,
			       AW86907_BIT_DETCFG2_DIAG_GO_ON);
	usleep_range(30000, 35000);
	aw86907_i2c_read(aw86907, AW86907_REG_DET_RL, &reg_val);
	lra_code = (lra_code | reg_val) << 2;
	aw86907_i2c_read(aw86907, AW86907_REG_DET_LO, &reg_val);
	lra_code = lra_code | (reg_val & 0x03);
	/* 2num */
	aw86907->lra = (lra_code * 678 * 1000) / (1024 * 10);
	aw86907_haptic_raminit(aw86907, false);
	aw86907_i2c_write_bits(aw86907, AW86907_REG_SYSCTRL7,
			       AW86907_BIT_SYSCTRL7_D2S_GAIN_MASK,
			       d2s_gain_temp);
	mutex_unlock(&aw86907->lock);
	return 0;
}


static enum hrtimer_restart qti_hap_stop_timer(struct hrtimer *timer)
{
	struct aw86907 *aw86907 = container_of(timer, struct aw86907,
					     stop_timer);
	int rc;

	aw_info("%s enter\n", __func__);
	aw86907->play.length_us = 0;
	rc = aw86907_i2c_write(aw86907, AW86907_REG_PLAYCFG4,
			  AW86907_BIT_PLAYCFG4_STOP_ON);
	if (rc < 0)
		aw_err("Stop playing failed, rc=%d\n", rc);

	return HRTIMER_NORESTART;
}

static enum hrtimer_restart qti_hap_disable_timer(struct hrtimer *timer)
{
	struct aw86907 *aw86907 = container_of(timer, struct aw86907,
					     hap_disable_timer);
	int rc;

	aw_info("%s enter\n", __func__);
	rc = aw86907_i2c_write(aw86907, AW86907_REG_PLAYCFG4,
			  AW86907_BIT_PLAYCFG4_STOP_ON);
	if (rc < 0)
		aw_err("Disable haptics module failed, rc=%d\n",
			rc);

	return HRTIMER_NORESTART;
}

static void aw86907_haptic_misc_para_init(struct aw86907 *aw86907)
{

	aw_info("%s enter\n", __func__);

	aw86907->f0_cali_status = true;
	aw86907->rtp_routine_on = 0;
	aw86907->rtp_num_max = awinic_rtp_name_len;
	hrtimer_init(&aw86907->stop_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	aw86907->stop_timer.function = qti_hap_stop_timer;
	hrtimer_init(&aw86907->hap_disable_timer, CLOCK_MONOTONIC,
		     HRTIMER_MODE_REL);
	aw86907->hap_disable_timer.function = qti_hap_disable_timer;

	/* GAIN_BYPASS config */
	aw86907_i2c_write_bits(aw86907, AW86907_REG_SYSCTRL7,
			  AW86907_BIT_SYSCTRL7_GAIN_BYPASS_MASK,
			  AW86907_BIT_SYSCTRL7_GAIN_CHANGEABLE);

	aw86907_i2c_write(aw86907, AW86907_REG_BSTCFG1,
			  aw86907->info.bstcfg[0]);
	aw86907_i2c_write(aw86907, AW86907_REG_BSTCFG2,
			  aw86907->info.bstcfg[1]);
	aw86907_i2c_write(aw86907, AW86907_REG_BSTCFG3,
			  aw86907->info.bstcfg[2]);
	aw86907_i2c_write(aw86907, AW86907_REG_BSTCFG4,
			  aw86907->info.bstcfg[3]);
	aw86907_i2c_write(aw86907, AW86907_REG_BSTCFG5,
			  aw86907->info.bstcfg[4]);
	aw86907_i2c_write(aw86907, AW86907_REG_SYSCTRL3,
			  aw86907->info.sine_array[0]);
	aw86907_i2c_write(aw86907, AW86907_REG_SYSCTRL4,
			  aw86907->info.sine_array[1]);
	aw86907_i2c_write(aw86907, AW86907_REG_SYSCTRL5,
			  aw86907->info.sine_array[2]);
	aw86907_i2c_write(aw86907, AW86907_REG_SYSCTRL6,
			  aw86907->info.sine_array[3]);

	/* brk_bst_md */
	if (!aw86907->info.brk_bst_md)
		aw_err("%s aw86907->info.brk_bst_md = 0!\n", __func__);
	aw86907_i2c_write_bits(aw86907, AW86907_REG_CONTCFG1,
			       AW86907_BIT_CONTCFG1_BRK_BST_MD_MASK,
			       aw86907->info.brk_bst_md << 1);

	/* d2s_gain */
	if (!aw86907->info.d2s_gain)
		aw_err("%s aw86907->info.d2s_gain = 0!\n", __func__);
	aw86907_i2c_write_bits(aw86907, AW86907_REG_SYSCTRL7,
			       AW86907_BIT_SYSCTRL7_D2S_GAIN_MASK,
			       aw86907->info.d2s_gain);

	/* cont_tset */
	if (!aw86907->info.cont_tset)
		aw_err("%s aw86907->info.cont_tset = 0!\n", __func__);
	aw86907_i2c_write_bits(aw86907, AW86907_REG_CONTCFG13,
			       AW86907_BIT_CONTCFG13_TSET_MASK,
			       aw86907->info.cont_tset << 4);

	/* cont_bemf_set */
	if (!aw86907->info.cont_bemf_set)
		aw_err("%s aw86907->info.cont_bemf_set = 0!\n", __func__);
	aw86907_i2c_write_bits(aw86907, AW86907_REG_CONTCFG13,
			       AW86907_BIT_CONTCFG13_BEME_SET_MASK,
			       aw86907->info.cont_bemf_set);

	/* cont_brk_time */
	if (!aw86907->info.cont_brk_time)
		aw_err("%s aw86907->info.cont_brk_time = 0!\n", __func__);
	aw86907_i2c_write(aw86907, AW86907_REG_CONTCFG10,
			  aw86907->info.cont_brk_time);

	/* cont_bst_brk_gain */
	if (!aw86907->info.cont_bst_brk_gain)
		aw_err("%s aw86907->info.cont_bst_brk_gain = 0!\n",
			   __func__);
	aw86907_i2c_write_bits(aw86907, AW86907_REG_CONTCFG5,
			       AW86907_BIT_CONTCFG5_BST_BRK_GAIN_MASK,
			       aw86907->info.cont_bst_brk_gain << 4);

	/* cont_brk_gain */
	if (!aw86907->info.cont_brk_gain)
		aw_err("%s aw86907->info.cont_brk_gain = 0!\n", __func__);
	aw86907_i2c_write_bits(aw86907, AW86907_REG_CONTCFG5,
			       AW86907_BIT_CONTCFG5_BRK_GAIN_MASK,
			       aw86907->info.cont_brk_gain);

	/* i2s enbale */
	if (aw86907->info.is_enabled_i2s) {
		aw_info("%s i2s enabled!\n", __func__);
		aw86907_i2c_write_bits(aw86907, AW86907_REG_SYSCTRL2,
				       AW86907_BIT_SYSCTRL2_I2S_PIN_MASK,
				       AW86907_BIT_SYSCTRL2_I2S_PIN_I2S);
	} else {
		aw_info("%s i2s disabled!\n", __func__);
		aw86907_i2c_write_bits(aw86907, AW86907_REG_SYSCTRL2,
				       AW86907_BIT_SYSCTRL2_I2S_PIN_MASK,
				       AW86907_BIT_SYSCTRL2_I2S_PIN_TRIG);
	}
}

/*****************************************************
 *
 * rtp
 *
 *****************************************************/
static void aw86907_haptic_set_rtp_aei(struct aw86907 *aw86907, bool flag)
{
	if (flag) {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_SYSINTM,
				       AW86907_BIT_SYSINTM_FF_AEM_MASK,
				       AW86907_BIT_SYSINTM_FF_AEM_ON);
	} else {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_SYSINTM,
				       AW86907_BIT_SYSINTM_FF_AEM_MASK,
				       AW86907_BIT_SYSINTM_FF_AEM_OFF);
	}
}

static unsigned char aw86907_haptic_rtp_get_fifo_afs(struct aw86907 *aw86907)
{
	unsigned char ret = 0;
	unsigned char reg_val = 0;

	aw86907_i2c_read(aw86907, AW86907_REG_SYSST, &reg_val);
	reg_val &= AW86907_BIT_SYSST_FF_AFS;
	ret = reg_val >> 3;
	return ret;
}

static int aw86907_haptic_rtp_play(struct aw86907 *aw86907)
{
	unsigned int buf_len = 0;
	unsigned int period_size = aw86907->ram.base_addr >> 2;

	aw_info("%s enter\n", __func__);
	pm_qos_add_request(&pm_qos_req_vb, PM_QOS_CPU_DMA_LATENCY,
			   PM_QOS_VALUE_VB);

	mutex_lock(&aw86907->rtp_lock);
	aw86907->rtp_cnt = 0;
	disable_irq(gpio_to_irq(aw86907->irq_gpio));
	while ((!aw86907_haptic_rtp_get_fifo_afs(aw86907)) &&
		(aw86907->play_mode == AW86907_HAPTIC_RTP_MODE) &&
		!atomic_read(&aw86907->exit_in_rtp_loop)) {
#ifdef AW_ENABLE_RTP_PRINT_LOG
		aw_info("%s rtp cnt = %d\n", __func__, aw86907->rtp_cnt);
#endif
		if (!aw86907_rtp) {
			aw_info("%s:aw86907_rtp is null, break!\n",
				    __func__);
			break;
		}

		if (aw86907->is_custom_wave == 0) {
			if ((aw86907_rtp->len - aw86907->rtp_cnt) <
			(aw86907->ram.base_addr >> 2)) {
				buf_len = aw86907_rtp->len - aw86907->rtp_cnt;
			} else {
				buf_len = (aw86907->ram.base_addr >> 2);
			}
			aw86907_i2c_writes(aw86907, AW86907_REG_RTPDATA,
					  &aw86907_rtp->data[aw86907->rtp_cnt],
					  buf_len);
			aw86907->rtp_cnt += buf_len;
			if (aw86907->rtp_cnt == aw86907_rtp->len) {
				aw86907->rtp_cnt = 0;
				aw86907_haptic_set_rtp_aei(aw86907, false);
				break;
			}
		} else {
			buf_len = read_rb(aw86907_rtp->data,  period_size);
			aw86907_i2c_writes(aw86907, AW86907_REG_RTPDATA,
					  aw86907_rtp->data, buf_len);
			aw86907->rtp_cnt += buf_len;
			if (buf_len < period_size) {
				aw_info("%s: custom rtp update complete\n", __func__);
				aw86907->rtp_cnt = 0;
				aw86907_haptic_set_rtp_aei(aw86907, false);
				break;
			}
		}
	}

	enable_irq(gpio_to_irq(aw86907->irq_gpio));
	if (aw86907->play_mode == AW86907_HAPTIC_RTP_MODE &&
	    !atomic_read(&aw86907->exit_in_rtp_loop) &&
	    aw86907->rtp_cnt != 0)
		aw86907_haptic_set_rtp_aei(aw86907, true);

	aw_info("%s exit\n", __func__);
	mutex_unlock(&aw86907->rtp_lock);
	pm_qos_remove_request(&pm_qos_req_vb);
	return 0;
}

static void aw86907_haptic_upload_lra(struct aw86907 *aw86907,
				      unsigned int flag)
{
	switch (flag) {
	case WRITE_ZERO:
		aw_info("%s write zero to trim_lra!\n", __func__);
		aw86907_i2c_write_bits(aw86907, AW86907_REG_TRIMCFG3,
				       AW86907_BIT_TRIMCFG3_TRIM_LRA_MASK,
				       0x00);
		break;
	case F0_CALI:
		aw_info("%s write f0_cali_data to trim_lra = 0x%02X\n",
			    __func__, aw86907->f0_cali_data);
		aw86907_i2c_write_bits(aw86907, AW86907_REG_TRIMCFG3,
				       AW86907_BIT_TRIMCFG3_TRIM_LRA_MASK,
				       (char)aw86907->f0_cali_data);
		break;
	case OSC_CALI:
		aw_info("%s write osc_cali_data to trim_lra = 0x%02X\n",
			    __func__, aw86907->osc_cali_data);
		aw86907_i2c_write_bits(aw86907, AW86907_REG_TRIMCFG3,
				       AW86907_BIT_TRIMCFG3_TRIM_LRA_MASK,
				       (char)aw86907->osc_cali_data);
		break;
	default:
		break;
	}
}

static int aw86907_osc_trim_calculation(struct aw86907 *aw86907,
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
		real_code = ((theory_time - real_time) * 4000) / theory_time;
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

static int aw86907_rtp_trim_lra_calibration(struct aw86907 *aw86907)
{
	unsigned char reg_val = 0;
	unsigned int fre_val = 0;
	unsigned int theory_time = 0;
	unsigned int lra_trim_code = 0;

	aw86907_i2c_read(aw86907, AW86907_REG_SYSCTRL2, &reg_val);
	fre_val = (reg_val & 0x03) >> 0;

	if (fre_val == 2 || fre_val == 3)
		theory_time = (aw86907->rtp_len / 12000) * 1000000;	/*12K */
	if (fre_val == 0)
		theory_time = (aw86907->rtp_len / 24000) * 1000000;	/*24K */
	if (fre_val == 1)
		theory_time = (aw86907->rtp_len / 48000) * 1000000;	/*48K */

	aw_info("%s microsecond:%ld  theory_time = %d\n",
		    __func__, aw86907->microsecond, theory_time);

	lra_trim_code = aw86907_osc_trim_calculation(aw86907, theory_time,
						     aw86907->microsecond);
	if (lra_trim_code >= 0) {
		aw86907->osc_cali_data = lra_trim_code;
		aw86907_haptic_upload_lra(aw86907, OSC_CALI);
	}
	return 0;
}

static unsigned char aw86907_haptic_osc_read_status(struct aw86907 *aw86907)
{
	unsigned char reg_val = 0;

	aw86907_i2c_read(aw86907, AW86907_REG_SYSST2, &reg_val);
	return reg_val;
}

static int aw86907_rtp_osc_calibration(struct aw86907 *aw86907)
{
	const struct firmware *rtp_file;
	int ret = -1;
	unsigned int buf_len = 0;
	unsigned char osc_int_state = 0;

	aw86907->rtp_cnt = 0;
	aw86907->timeval_flags = 1;

	aw_info("%s enter\n", __func__);
	/* fw loaded */
	ret = request_firmware(&rtp_file, awinic_rtp_name[0], aw86907->dev);
	if (ret < 0) {
		aw_err("%s: failed to read %s\n", __func__,
			   awinic_rtp_name[0]);
		return ret;
	}
	/*awinic add stop,for irq interrupt during calibrate */
	aw86907_haptic_stop(aw86907);
	aw86907->rtp_init = 0;
	mutex_lock(&aw86907->rtp_lock);
	vfree(aw86907_rtp);
	aw86907_rtp = vmalloc(rtp_file->size + sizeof(int));
	if (!aw86907_rtp) {
		release_firmware(rtp_file);
		mutex_unlock(&aw86907->rtp_lock);
		aw_err("%s: error allocating memory\n",
			   __func__);
		return -ENOMEM;
	}
	aw86907_rtp->len = rtp_file->size;
	aw86907->rtp_len = rtp_file->size;
	aw_info("%s: rtp file:[%s] size = %dbytes\n",
		    __func__, awinic_rtp_name[0], aw86907_rtp->len);

	memcpy(aw86907_rtp->data, rtp_file->data, rtp_file->size);
	release_firmware(rtp_file);
	mutex_unlock(&aw86907->rtp_lock);
	/* gain */
	aw86907_haptic_ram_vbat_comp(aw86907, false);
	/* rtp mode config */
	aw86907_haptic_play_mode(aw86907, AW86907_HAPTIC_RTP_MODE);
	/* bst mode */
	aw86907_haptic_bst_mode_config(aw86907, AW86907_HAPTIC_BST_MODE_BYPASS);
	disable_irq(gpio_to_irq(aw86907->irq_gpio));

	/* haptic go */
	aw86907_haptic_play_go(aw86907);
	/* require latency of CPU & DMA not more then PM_QOS_VALUE_VB us */
	pm_qos_add_request(&pm_qos_req_vb, PM_QOS_CPU_DMA_LATENCY,
			   PM_QOS_VALUE_VB);
	while (1) {
		if (!aw86907_haptic_rtp_get_fifo_afs(aw86907)) {
#ifdef AW_ENABLE_RTP_PRINT_LOG
			aw_info("%s not almost_full, aw86907->rtp_cnt=%d\n",
				    __func__, aw86907->rtp_cnt);
#endif
			mutex_lock(&aw86907->rtp_lock);
			if ((aw86907_rtp->len - aw86907->rtp_cnt) <
			    (aw86907->ram.base_addr >> 2))
				buf_len = aw86907_rtp->len - aw86907->rtp_cnt;
			else
				buf_len = (aw86907->ram.base_addr >> 2);

			if (aw86907->rtp_cnt != aw86907_rtp->len) {
				if (aw86907->timeval_flags == 1) {
					do_gettimeofday(&aw86907->start);
					aw86907->timeval_flags = 0;
				}
				aw86907->rtp_update_flag =
				    aw86907_i2c_writes(aw86907,
						       AW86907_REG_RTPDATA,
						       &aw86907_rtp->data
						       [aw86907->rtp_cnt],
						       buf_len);
				aw86907->rtp_cnt += buf_len;
			}
			mutex_unlock(&aw86907->rtp_lock);
		}
		osc_int_state = aw86907_haptic_osc_read_status(aw86907);
		if (osc_int_state & AW86907_BIT_SYSST2_FF_EMPTY) {
			do_gettimeofday(&aw86907->end);
			aw_info
			    ("%s osc trim playback done aw86907->rtp_cnt= %d\n",
			     __func__, aw86907->rtp_cnt);
			break;
		}
		do_gettimeofday(&aw86907->end);
		aw86907->microsecond =
		    (aw86907->end.tv_sec - aw86907->start.tv_sec) * 1000000 +
		    (aw86907->end.tv_usec - aw86907->start.tv_usec);
		if (aw86907->microsecond > OSC_CALI_MAX_LENGTH) {
			aw_info("%s osc trim time out! aw86907->rtp_cnt %d osc_int_state %02x\n",
				    __func__, aw86907->rtp_cnt, osc_int_state);
			break;
		}
	}
	pm_qos_remove_request(&pm_qos_req_vb);
	enable_irq(gpio_to_irq(aw86907->irq_gpio));

	aw86907->microsecond =
	    (aw86907->end.tv_sec - aw86907->start.tv_sec) * 1000000 +
	    (aw86907->end.tv_usec - aw86907->start.tv_usec);
	/*calibration osc */
	aw_info("%s awinic_microsecond: %ld\n", __func__,
		    aw86907->microsecond);
	aw_info("%s exit\n", __func__);
	return 0;
}

static int aw86907_haptic_effect_strength(struct aw86907 *aw86907)
{
	aw_dbg("%s enter\n", __func__);
	aw_dbg("%s: aw86907->play.vmax_mv =0x%x\n", __func__,
		 aw86907->play.vmax_mv);
#if 0
	switch (aw86907->play.vmax_mv) {
	case AW86907_LIGHT_MAGNITUDE:
		aw86907->level = 0x80;
		break;
	case AW86907_MEDIUM_MAGNITUDE:
		aw86907->level = 0x50;
		break;
	case AW86907_STRONG_MAGNITUDE:
		aw86907->level = 0x30;
		break;
	default:
		break;
	}
#else
	if (aw86907->play.vmax_mv >= 0x7FFF)
		aw86907->level = 0x80; /*128*/
	else if (aw86907->play.vmax_mv <= 0x3FFF)
		aw86907->level = 0x1E; /*30*/
	else
		aw86907->level = (aw86907->play.vmax_mv - 16383) / 128;
	if (aw86907->level < 0x1E)
		aw86907->level = 0x1E; /*30*/
#endif

	aw_info("%s: aw86907->level =0x%x\n", __func__, aw86907->level);
	return 0;
}

static void aw86907_rtp_work_routine(struct work_struct *work)
{
	const struct firmware *rtp_file;
	int ret = -1;
	unsigned int cnt = 200;
	unsigned char reg_val = 0;
	bool rtp_work_flag = false;
	struct aw86907 *aw86907 = container_of(work, struct aw86907, rtp_work);

	aw_info("%s enter\n", __func__);

	aw_info("%s: effect_id = %d state=%d activate_mode = %d\n", __func__,
		aw86907->effect_id, aw86907->state, aw86907->activate_mode);

	if ((aw86907->effect_id < aw86907->info.effect_id_boundary) &&
	    (aw86907->effect_id > aw86907->info.effect_max))
		return;

	mutex_lock(&aw86907->lock);

	aw86907->rtp_routine_on = 1;
	aw86907_haptic_upload_lra(aw86907, OSC_CALI);
	aw86907_haptic_set_rtp_aei(aw86907, false);
	aw86907_interrupt_clear(aw86907);

	/* wait for irq to exit */
	atomic_set(&aw86907->exit_in_rtp_loop, 1);
	while (atomic_read(&aw86907->is_in_rtp_loop)) {
		aw_info("%s  goint to waiting irq exit\n", __func__);
		mutex_unlock(&aw86907->lock);
		ret = wait_event_interruptible(aw86907->wait_q,
				atomic_read(&aw86907->is_in_rtp_loop) == 0);
		aw_info("%s  wakeup\n", __func__);
		mutex_lock(&aw86907->lock);
		if (ret == -ERESTARTSYS) {
			atomic_set(&aw86907->exit_in_rtp_loop, 0);
			wake_up_interruptible(&aw86907->stop_wait_q);
			mutex_unlock(&aw86907->lock);
			aw_err("%s wake up by signal return erro\n", __func__);
			return;
		}
	}

	atomic_set(&aw86907->exit_in_rtp_loop, 0);
	wake_up_interruptible(&aw86907->stop_wait_q);

	/* how to force exit this call */
	if (aw86907->is_custom_wave == 1 && aw86907->state) {
		aw_err("%s buffer size %d, availbe size %d\n",
		       __func__, aw86907->ram.base_addr >> 2,
		       get_rb_avalible_size());
		while (get_rb_avalible_size() < aw86907->ram.base_addr &&
		       !rb_shoule_exit()) {
			mutex_unlock(&aw86907->lock);
			ret = wait_event_interruptible(aw86907->stop_wait_q,
							(get_rb_avalible_size() >= aw86907->ram.base_addr) ||
							rb_shoule_exit());
			aw_info("%s  wakeup\n", __func__);
			aw_err("%s after wakeup sbuffer size %d, availbe size %d\n",
			       __func__, aw86907->ram.base_addr >> 2,
			       get_rb_avalible_size());
			if (ret == -ERESTARTSYS) {
				aw_err("%s wake up by signal return erro\n",
				       __func__);
					return;
			}
			mutex_lock(&aw86907->lock);

		}
	}

	aw86907_haptic_stop(aw86907);

	if (aw86907->state) {
		pm_stay_awake(aw86907->dev);
		/* boost voltage */
		if (aw86907->info.bst_vol_rtp <= AW86907_MAX_BST_VOL)
			aw86907_haptic_set_bst_vol(aw86907, aw86907->info.bst_vol_rtp);
		else
			aw86907_haptic_set_bst_vol(aw86907, aw86907->vmax);
		/* gain */
		aw86907_haptic_ram_vbat_comp(aw86907, false);
		aw86907_haptic_effect_strength(aw86907);
		aw86907_haptic_set_gain(aw86907, aw86907->level);
		aw86907->rtp_init = 0;
		if (aw86907->is_custom_wave == 0) {
			aw86907->rtp_file_num = aw86907->effect_id -
					aw86907->info.effect_id_boundary;
			aw_info("%s: aw86907->rtp_file_num =%d\n", __func__,
			       aw86907->rtp_file_num);
			if (aw86907->rtp_file_num < 0)
				aw86907->rtp_file_num = 0;
			if (aw86907->rtp_file_num > (awinic_rtp_name_len - 1))
				aw86907->rtp_file_num = awinic_rtp_name_len - 1;
			aw86907->rtp_routine_on = 1;
			/* fw loaded */
			ret = request_firmware(&rtp_file,
					       awinic_rtp_name[aw86907->rtp_file_num],
					       aw86907->dev);
			if (ret < 0) {
				aw_err("%s: failed to read %s\n",
					   __func__,
					   awinic_rtp_name[aw86907->rtp_file_num]);
				aw86907->rtp_routine_on = 0;
				pm_relax(aw86907->dev);
				mutex_unlock(&aw86907->lock);
				return;
			}
			vfree(aw86907_rtp);
			aw86907_rtp = vmalloc(rtp_file->size + sizeof(int));
			if (!aw86907_rtp) {
				release_firmware(rtp_file);
				aw_err("%s: error allocating memory\n",
				       __func__);
				aw86907->rtp_routine_on = 0;
				pm_relax(aw86907->dev);
				mutex_unlock(&aw86907->lock);
				return;
			}
			aw86907_rtp->len = rtp_file->size;
			aw_info("%s: rtp file:[%s] size = %dbytes\n",
				__func__,
				awinic_rtp_name[aw86907->rtp_file_num],
				aw86907_rtp->len);
			memcpy(aw86907_rtp->data, rtp_file->data,
			       rtp_file->size);
			release_firmware(rtp_file);
		} else {
			vfree(aw86907_rtp);
			aw86907_rtp = vmalloc(aw86907->ram.base_addr >> 2);
			if (!aw86907_rtp) {
				aw_err("%s: error allocating memory\n",
				       __func__);
				pm_relax(aw86907->dev);
				mutex_unlock(&aw86907->lock);
				return;
			}
		}
		aw86907->rtp_init = 1;
		aw86907_haptic_play_mode(aw86907, AW86907_HAPTIC_RTP_MODE);
		/* haptic go */
		aw86907_haptic_play_go(aw86907);
		usleep_range(2000, 2500);
		while (cnt) {
			aw86907_i2c_read(aw86907, AW86907_REG_GLBRD5, &reg_val);
			if ((reg_val & 0x0f) == 0x08) {
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
			aw86907_haptic_rtp_play(aw86907);
		} else {
			/* enter standby mode */
			aw86907_haptic_stop(aw86907);
			aw_err("%s failed to enter RTP_GO status!\n",
				   __func__);
		}
		aw86907->rtp_routine_on = 0;

	} else {
		aw86907->rtp_cnt = 0;
		aw86907->rtp_init = 0;
		pm_relax(aw86907->dev);
	}
	mutex_unlock(&aw86907->lock);

}

/*****************************************************
 *
 * haptic - audio
 *
 *****************************************************/
static int aw86907_haptic_start(struct aw86907 *aw86907)
{
	aw_dbg("%s enter\n", __func__);

	aw86907_haptic_play_go(aw86907);

	return 0;
}

static void aw86907_clean_status(struct aw86907 *aw86907)
{
	aw86907->audio_ready = false;
	aw86907->haptic_ready = false;
	aw86907->rtp_routine_on = 0;
	aw_info("%s enter\n", __func__);
}

static int aw86907_haptic_juge_RTP_is_going_on(struct aw86907 *aw86907)
{
	unsigned char glb_state = 0;
	unsigned char rtp_state = 0;

	aw86907_i2c_read(aw86907, AW86907_REG_GLBRD5, &glb_state);
	if (aw86907->rtp_routine_on
	    || (glb_state == AW86907_BIT_GLBRD5_STATE_RTP_GO)) {
		rtp_state = 1;	/*is going on */
		aw_info("%s: rtp_routine_on\n", __func__);
	}
	return rtp_state;
}

static enum hrtimer_restart aw86907_haptic_audio_timer_func(struct hrtimer
							    *timer)
{
	struct aw86907 *aw86907 =
	    container_of(timer, struct aw86907, haptic_audio.timer);

	aw_dbg("%s enter\n", __func__);
	schedule_work(&aw86907->haptic_audio.work);

	hrtimer_start(&aw86907->haptic_audio.timer,
		      ktime_set(aw86907->haptic_audio.timer_val / 1000000,
				(aw86907->haptic_audio.timer_val % 1000000) *
				1000), HRTIMER_MODE_REL);
	return HRTIMER_NORESTART;
}

static void aw86907_haptic_audio_work_routine(struct work_struct *work)
{
	struct aw86907 *aw86907 =
	    container_of(work, struct aw86907, haptic_audio.work);
	int rtp_is_going_on = 0;

	aw_info("%s enter\n", __func__);
	mutex_lock(&aw86907->haptic_audio.lock);
	/* rtp mode jump */
	rtp_is_going_on = aw86907_haptic_juge_RTP_is_going_on(aw86907);
	if (rtp_is_going_on) {
		mutex_unlock(&aw86907->haptic_audio.lock);
		return;
	}
	memcpy(&aw86907->haptic_audio.ctr,
	       &aw86907->haptic_audio.data[aw86907->haptic_audio.cnt],
	       sizeof(struct haptic_ctr));
	aw_dbg("%s: cnt=%d, cmd=%d, play=%d, wavseq=%d, loop=%d, gain=%d\n",
		 __func__,
		 aw86907->haptic_audio.cnt,
		 aw86907->haptic_audio.ctr.cmd,
		 aw86907->haptic_audio.ctr.play,
		 aw86907->haptic_audio.ctr.wavseq,
		 aw86907->haptic_audio.ctr.loop,
		 aw86907->haptic_audio.ctr.gain);
	mutex_unlock(&aw86907->haptic_audio.lock);
	if (aw86907->haptic_audio.ctr.cmd == AW86907_HAPTIC_CMD_ENABLE) {
		if (aw86907->haptic_audio.ctr.play ==
		    AW86907_HAPTIC_PLAY_ENABLE) {
			aw_info("%s: haptic_audio_play_start\n", __func__);
			mutex_lock(&aw86907->lock);
			aw86907_haptic_stop(aw86907);
			aw86907_haptic_play_mode(aw86907,
						 AW86907_HAPTIC_RAM_MODE);

			aw86907_haptic_set_wav_seq(aw86907, 0x00,
						   aw86907->haptic_audio.ctr.
						   wavseq);
			aw86907_haptic_set_wav_seq(aw86907, 0x01, 0x00);

			aw86907_haptic_set_wav_loop(aw86907, 0x00,
						    aw86907->haptic_audio.ctr.
						    loop);

			aw86907_haptic_set_gain(aw86907,
						aw86907->haptic_audio.ctr.gain);

			aw86907_haptic_start(aw86907);
			mutex_unlock(&aw86907->lock);
		} else if (AW86907_HAPTIC_PLAY_STOP ==
			   aw86907->haptic_audio.ctr.play) {
			mutex_lock(&aw86907->lock);
			aw86907_haptic_stop(aw86907);
			mutex_unlock(&aw86907->lock);
		} else if (AW86907_HAPTIC_PLAY_GAIN ==
			   aw86907->haptic_audio.ctr.play) {
			mutex_lock(&aw86907->lock);
			aw86907_haptic_set_gain(aw86907,
						aw86907->haptic_audio.ctr.gain);
			mutex_unlock(&aw86907->lock);
		}
		mutex_lock(&aw86907->haptic_audio.lock);
		memset(&aw86907->haptic_audio.data[aw86907->haptic_audio.cnt],
		       0, sizeof(struct haptic_ctr));
		mutex_unlock(&aw86907->haptic_audio.lock);
	}
	mutex_lock(&aw86907->haptic_audio.lock);
	aw86907->haptic_audio.cnt++;
	if (aw86907->haptic_audio.data[aw86907->haptic_audio.cnt].cmd == 0) {
		aw86907->haptic_audio.cnt = 0;
		aw_dbg("%s: haptic play buffer restart\n", __func__);
	}
	mutex_unlock(&aw86907->haptic_audio.lock);
}

/*****************************************************
 *
 * haptic cont
 *
 *****************************************************/
static int aw86907_haptic_cont_config(struct aw86907 *aw86907)
{
	aw_info("%s enter\n", __func__);

	/* work mode */
	aw86907_haptic_play_mode(aw86907, AW86907_HAPTIC_CONT_MODE);
	/* cont config */
	/* aw86907_i2c_write_bits(aw86907, AW86907_REG_CONTCFG1,
	 **                     AW86907_BIT_CONTCFG1_EN_F0_DET_MASK,
	 **                     AW86907_BIT_CONTCFG1_F0_DET_ENABLE);
	 */
	aw86907_i2c_write_bits(aw86907, AW86907_REG_CONTCFG6,
			       AW86907_BIT_CONTCFG6_TRACK_EN_MASK,
			       AW86907_BIT_CONTCFG6_TRACK_ENABLE);
	/* f0 driver level */
	aw86907_i2c_write_bits(aw86907, AW86907_REG_CONTCFG6,
			       AW86907_BIT_CONTCFG6_DRV1_LVL_MASK,
			       aw86907->info.cont_drv1_lvl);
	aw86907_i2c_write(aw86907, AW86907_REG_CONTCFG7,
			  aw86907->info.cont_drv2_lvl);
	/* DRV1_TIME */
	/* aw86907_i2c_write(aw86907, AW86907_REG_CONTCFG8, 0xFF); */
	/* DRV2_TIME */
	aw86907_i2c_write(aw86907, AW86907_REG_CONTCFG9, 0xFF);
	/* cont play go */
	aw86907_haptic_play_go(aw86907);
	return 0;
}

/*****************************************************
 *
 * haptic f0 cali
 *
 *****************************************************/
static int aw86907_haptic_read_lra_f0(struct aw86907 *aw86907)
{
	int ret = 0;
	unsigned char reg_val = 0;
	unsigned int f0_reg = 0;
	unsigned long f0_tmp = 0;

	aw_info("%s enter\n", __func__);
	/* F_LRA_F0_H */
	ret = aw86907_i2c_read(aw86907, AW86907_REG_CONTRD14, &reg_val);
	f0_reg = (f0_reg | reg_val) << 8;
	/* F_LRA_F0_L */
	ret = aw86907_i2c_read(aw86907, AW86907_REG_CONTRD15, &reg_val);
	f0_reg |= (reg_val << 0);
	if (!f0_reg) {
		aw_err("%s didn't get lra f0 because f0_reg value is 0!\n",
			   __func__);
		aw86907->f0_cali_status = false;
		return ret;
	}
	aw86907->f0_cali_status = true;
	f0_tmp = 384000 * 10 / f0_reg;
	aw86907->f0 = (unsigned int)f0_tmp;
	aw_info("%s lra_f0=%d\n", __func__, aw86907->f0);

	return ret;
}

static int aw86907_haptic_read_cont_f0(struct aw86907 *aw86907)
{
	int ret = 0;
	unsigned char reg_val = 0;
	unsigned int f0_reg = 0;
	unsigned long f0_tmp = 0;

	aw_info("%s enter\n", __func__);
	ret = aw86907_i2c_read(aw86907, AW86907_REG_CONTRD16, &reg_val);
	f0_reg = (f0_reg | reg_val) << 8;
	ret = aw86907_i2c_read(aw86907, AW86907_REG_CONTRD17, &reg_val);
	f0_reg |= (reg_val << 0);
	if (!f0_reg) {
		aw_err("%s didn't get cont f0 because f0_reg value is 0!\n",
			   __func__);
		aw86907->cont_f0 = aw86907->info.f0_ref;
		return ret;
	}
	f0_tmp = 384000 * 10 / f0_reg;
	aw86907->cont_f0 = (unsigned int)f0_tmp;
	aw_info("%s cont_f0=%d\n", __func__, aw86907->cont_f0);
	return ret;
}

static void aw86907_haptic_auto_break_mode(struct aw86907 *aw86907, bool flag)
{
	if (flag) {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_PLAYCFG3,
			       AW86907_BIT_PLAYCFG3_BRK_EN_MASK,
			       AW86907_BIT_PLAYCFG3_BRK_ENABLE);
	} else {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_PLAYCFG3,
			       AW86907_BIT_PLAYCFG3_BRK_EN_MASK,
			       AW86907_BIT_PLAYCFG3_BRK_DISABLE);
	}
}

static int aw86907_haptic_cont_get_f0(struct aw86907 *aw86907)
{
	int ret = 0;
	unsigned int cnt = 200;
	unsigned char reg_val = 0;
	unsigned char brk_en_default = 0;
	unsigned char d2s_gain_default = 0;
	bool get_f0_flag = false;

	aw_info("%s enter\n", __func__);
	aw86907->f0 = aw86907->info.f0_ref;
	/* enter standby mode */
	aw86907_haptic_stop(aw86907);
	/* config max d2s_gain */
	aw86907_i2c_read(aw86907, AW86907_REG_SYSCTRL7, &reg_val);
	d2s_gain_default = reg_val & AW86907_BIT_SYSCTRL7_D2S_GAIN;
	aw86907_i2c_write_bits(aw86907, AW86907_REG_SYSCTRL7,
			       AW86907_BIT_SYSCTRL7_D2S_GAIN_MASK,
			       AW86907_BIT_SYSCTRL7_D2S_GAIN_26);
	/* f0 calibrate work mode */
	aw86907_haptic_play_mode(aw86907, AW86907_HAPTIC_CONT_MODE);
	/* enable f0 detect */
	aw86907_i2c_write_bits(aw86907, AW86907_REG_CONTCFG1,
			       AW86907_BIT_CONTCFG1_EN_F0_DET_MASK,
			       AW86907_BIT_CONTCFG1_F0_DET_ENABLE);
	/* cont config */
	aw86907_i2c_write_bits(aw86907, AW86907_REG_CONTCFG6,
			       AW86907_BIT_CONTCFG6_TRACK_EN_MASK,
			       AW86907_BIT_CONTCFG6_TRACK_ENABLE);
	/* enable auto break */
	aw86907_i2c_read(aw86907, AW86907_REG_PLAYCFG3, &reg_val);
	brk_en_default = 0x04 & reg_val;
	aw86907_haptic_auto_break_mode(aw86907, true);

	/* f0 driver level */
	aw86907_i2c_write_bits(aw86907, AW86907_REG_CONTCFG6,
			       AW86907_BIT_CONTCFG6_DRV1_LVL_MASK,
			       aw86907->info.cont_drv1_lvl);
	aw86907_i2c_write(aw86907, AW86907_REG_CONTCFG7,
			  aw86907->info.cont_drv2_lvl);
	/* DRV1_TIME */
	aw86907_i2c_write(aw86907, AW86907_REG_CONTCFG8,
			  aw86907->info.cont_drv1_time);
	/* DRV2_TIME */
	aw86907_i2c_write(aw86907, AW86907_REG_CONTCFG9,
			  aw86907->info.cont_drv2_time);
	/* TRACK_MARGIN */
	if (!aw86907->info.cont_track_margin) {
		aw_err("%s aw86907->info.cont_track_margin = 0!\n",
			   __func__);
	} else {
		aw86907_i2c_write(aw86907, AW86907_REG_CONTCFG11,
				  (unsigned char)aw86907->
				  info.cont_track_margin);
	}
	/* DRV_WIDTH */
	aw86907_i2c_write(aw86907, AW86907_REG_CONTCFG3,
			  aw86907->info.cont_drv_width);

	/* cont play go */
	aw86907_haptic_play_go(aw86907);
	usleep_range(20000, 20500);
	/* 300ms */
	while (cnt) {
		aw86907_i2c_read(aw86907, AW86907_REG_GLBRD5, &reg_val);
		if ((reg_val & 0x0f) == 0x00) {
			cnt = 0;
			get_f0_flag = true;
			aw_info("%s: entered standby mode! glb_state=0x%02X\n",
				    __func__, reg_val);
		} else {
			cnt--;
			aw_dbg("%s: waitting for standby, glb_state=0x%02X\n",
				   __func__, reg_val);
		}
		usleep_range(10000, 10500);
	}
	if (get_f0_flag) {
		aw86907_haptic_read_lra_f0(aw86907);
		aw86907_haptic_read_cont_f0(aw86907);
	} else {
		aw_err("%s enter standby mode failed, stop reading f0!\n",
			   __func__);
	}
	/* restore d2s_gain config */
	aw86907_i2c_write_bits(aw86907, AW86907_REG_SYSCTRL7,
			       AW86907_BIT_SYSCTRL7_D2S_GAIN_MASK,
			       d2s_gain_default);
	/* restore default config */
	aw86907_i2c_write_bits(aw86907, AW86907_REG_CONTCFG1,
			       AW86907_BIT_CONTCFG1_EN_F0_DET_MASK,
			       AW86907_BIT_CONTCFG1_F0_DET_DISABLE);
	/* recover auto break config */
	if (brk_en_default)
		aw86907_haptic_auto_break_mode(aw86907, true);
	else
		aw86907_haptic_auto_break_mode(aw86907, false);

	return ret;
}

static int aw86907_haptic_f0_calibration(struct aw86907 *aw86907)
{
	int ret = 0;
	unsigned char reg_val = 0;
	unsigned int f0_limit = 0;
	char f0_cali_lra = 0;
	int f0_cali_step = 0;
	unsigned int f0_cali_min =
	    aw86907->info.f0_ref * (100 - aw86907->info.f0_cali_percent) / 100;
	unsigned int f0_cali_max =
	    aw86907->info.f0_ref * (100 + aw86907->info.f0_cali_percent) / 100;

	aw_info("%s enter\n", __func__);

#ifndef AW_ENABLE_MULTI_CALI
	aw86907_haptic_upload_lra(aw86907, WRITE_ZERO);
#endif

	if (aw86907_haptic_cont_get_f0(aw86907)) {
		aw_err("%s get f0 error, user defafult f0\n", __func__);
	} else {
		/* max and min limit */
		f0_limit = aw86907->f0;
		aw_info("%s f0_ref = %d, f0_cali_min = %d, f0_cali_max = %d, f0 = %d\n",
			    __func__, aw86907->info.f0_ref,
			    f0_cali_min, f0_cali_max, aw86907->f0);

		if ((aw86907->f0 < f0_cali_min) || aw86907->f0 > f0_cali_max) {
			aw_err("%s f0 calibration out of range = %d!\n",
				   __func__, aw86907->f0);
			f0_limit = aw86907->info.f0_ref;
			return -ERANGE;
		}
		aw_info("%s f0_limit = %d\n", __func__, (int)f0_limit);
		/* calculate cali step */
		f0_cali_step = 100000 * ((int)f0_limit -
					 (int)aw86907->info.f0_ref) /
		    ((int)f0_limit * 24);
		aw_info("%s f0_cali_step = %d\n", __func__, f0_cali_step);
		if (f0_cali_step >= 0) {	/*f0_cali_step >= 0 */
			if (f0_cali_step % 10 >= 5)
				f0_cali_step = 32 + (f0_cali_step / 10 + 1);
			else
				f0_cali_step = 32 + f0_cali_step / 10;
		} else {	/* f0_cali_step < 0 */
			if (f0_cali_step % 10 <= -5)
				f0_cali_step = 32 + (f0_cali_step / 10 - 1);
			else
				f0_cali_step = 32 + f0_cali_step / 10;
		}
		if (f0_cali_step > 31)
			f0_cali_lra = (char)f0_cali_step - 32;
		else
			f0_cali_lra = (char)f0_cali_step + 32;
		/* update cali step */
		aw86907_i2c_read(aw86907, AW86907_REG_TRIMCFG3, &reg_val);
		aw86907->f0_cali_data =
		    ((int)f0_cali_lra + (int)(reg_val & 0x3f)) & 0x3f;

		aw_info("%s origin trim_lra = 0x%02X, f0_cali_lra = 0x%02X, final f0_cali_data = 0x%02X\n",
			    __func__, (reg_val & 0x3f), f0_cali_lra,
			    aw86907->f0_cali_data);
	}
	aw86907_haptic_upload_lra(aw86907, F0_CALI);
	/* restore standby work mode */
	aw86907_haptic_stop(aw86907);
	return ret;
}

static int aw86907_haptic_i2s_init(struct aw86907 *aw86907)
{
	aw_info("%s: enter\n", __func__);

	aw86907_i2c_write_bits(aw86907, AW86907_REG_SYSCTRL2,
			       AW86907_BIT_SYSCTRL2_I2S_PIN_MASK,
			       AW86907_BIT_SYSCTRL2_I2S_PIN_I2S);
	aw86907_i2c_write_bits(aw86907, AW86907_REG_IOCFG1,
			       AW86907_BIT_IOCFG1_IO_FAST_MASK,
			       AW86907_BIT_IOCFG1_IIS_IO_FAST_ENABLE);
	return 0;
}

static int aw86907_haptic_one_wire_init(struct aw86907 *aw86907)
{
	aw_info("%s: enter\n", __func__);

	/*if enable one-wire, trig1 priority must be less than trig2 and trig3*/
	aw86907_i2c_write(aw86907, AW86907_REG_GLBCFG4, 0x6c);
	aw86907_i2c_write(aw86907, AW86907_REG_GLBCFG2,
			  AW86907_BIT_START_DLY_2P5MS);
	aw86907_i2c_write_bits(aw86907,
			       AW86907_REG_TRGCFG8,
			       AW86907_BIT_TRGCFG8_TRG_ONEWIRE_MASK,
			       AW86907_BIT_TRGCFG8_TRG_ONEWIRE_ENABLE);
	return 0;
}

int aw86907_haptic_init(struct aw86907 *aw86907)
{
	int ret = 0;
	unsigned char i = 0;
	unsigned char reg_val = 0;

	aw_info("%s enter\n", __func__);
	/* haptic audio */
	aw86907->haptic_audio.delay_val = 1;
	aw86907->haptic_audio.timer_val = 21318;
	hrtimer_init(&aw86907->haptic_audio.timer, CLOCK_MONOTONIC,
		     HRTIMER_MODE_REL);
	aw86907->haptic_audio.timer.function = aw86907_haptic_audio_timer_func;
	INIT_WORK(&aw86907->haptic_audio.work,
		  aw86907_haptic_audio_work_routine);
	mutex_init(&aw86907->haptic_audio.lock);
	aw86907_clean_status(aw86907);
	/* haptic init */
	mutex_lock(&aw86907->lock);
	aw86907->activate_mode = aw86907->info.mode;
	ret = aw86907_i2c_read(aw86907, AW86907_REG_WAVCFG1, &reg_val);
	aw86907->index = reg_val & 0x7F;
	ret = aw86907_i2c_read(aw86907, AW86907_REG_PLAYCFG2, &reg_val);
	aw86907->gain = reg_val & 0xFF;
	aw_info("%s aw86907->gain =0x%02X\n", __func__, aw86907->gain);
	ret = aw86907_i2c_read(aw86907, AW86907_REG_PLAYCFG1, &reg_val);
	aw86907->vmax = reg_val & 0x3F;
	for (i = 0; i < AW86907_SEQUENCER_SIZE; i++) {
		ret = aw86907_i2c_read(aw86907, AW86907_REG_WAVCFG1 + i,
				       &reg_val);
		aw86907->seq[i] = reg_val;
	}
	aw86907_haptic_play_mode(aw86907, AW86907_HAPTIC_STANDBY_MODE);
	aw86907_haptic_set_pwm(aw86907, AW86907_PWM_24K);
	/* misc value init */
	aw86907_haptic_misc_para_init(aw86907);
	/* set BST_ADJ */
	aw86907_i2c_write_bits(aw86907, AW86907_REG_BSTCFG5,
			       AW86907_BIT_BSTCFG5_BST_ADJ_MASK,
			       AW86907_BIT_BSTCFG5_BST_ADJ_LOW);
	aw86907_haptic_set_bst_peak_cur(aw86907, AW86907_DEFAULT_PEAKCUR);
	aw86907_haptic_swicth_motor_protect_config(aw86907, AW_PROTECT_EN, AW_PROTECT_VAL);
	aw86907_haptic_auto_bst_enable(aw86907, false);
	aw86907_haptic_offset_calibration(aw86907);
	/* vbat compensation */
	aw86907_haptic_vbat_mode_config(aw86907,
				AW86907_HAPTIC_CONT_VBAT_HW_ADJUST_MODE);
	aw86907->ram_vbat_comp = AW86907_HAPTIC_RAM_VBAT_COMP_ENABLE;
	/* i2s config */
	if (aw86907->info.is_enabled_i2s) {
		aw_info("%s i2s is enabled!\n", __func__);
		aw86907_haptic_i2s_init(aw86907);
	} else {
		aw86907_haptic_trig2_param_init(aw86907);
		aw86907_haptic_trig3_param_init(aw86907);
		aw86907_haptic_trig2_param_config(aw86907);
		aw86907_haptic_trig3_param_config(aw86907);
	}
	/* one wire config */
	if (aw86907->info.is_enabled_one_wire) {
		aw_info("%s one wire is enabled!\n", __func__);
		aw86907_haptic_one_wire_init(aw86907);
	} else {
		aw86907_haptic_trig1_param_init(aw86907);
		aw86907_haptic_trig1_param_config(aw86907);
	}
	mutex_unlock(&aw86907->lock);

	/* f0 calibration */
#ifndef USE_CONT_F0_CALI
	mutex_lock(&aw86907->lock);
	aw86907_haptic_upload_lra(aw86907, WRITE_ZERO);
	aw86907_haptic_f0_calibration(aw86907);
	mutex_unlock(&aw86907->lock);
#endif
	return ret;
}

/*****************************************************
 *
 * vibrator
 *
 *****************************************************/
static ssize_t aw86907_bst_vol_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len,
			"bst_vol_ram=%d, bst_vol_rtp=%d\n",
			aw86907->info.bst_vol_ram, aw86907->info.bst_vol_rtp);
	return len;
}

static ssize_t aw86907_bst_vol_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	unsigned int databuf[2] = { 0, 0 };

	if (sscanf(buf, "%d %d", &databuf[0], &databuf[1]) == 2) {
		aw86907->info.bst_vol_ram = databuf[0];
		aw86907->info.bst_vol_rtp = databuf[1];
	}
	return count;
}

static ssize_t aw86907_state_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;

	return snprintf(buf, PAGE_SIZE, "state = %d\n", aw86907->state);
}

static ssize_t aw86907_state_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	return count;
}

static ssize_t aw86907_duration_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	ktime_t time_rem;
	s64 time_ms = 0;

	if (hrtimer_active(&aw86907->timer)) {
		time_rem = hrtimer_get_remaining(&aw86907->timer);
		time_ms = ktime_to_ms(time_rem);
	}
	return snprintf(buf, PAGE_SIZE, "duration = %lldms\n", time_ms);
}

static ssize_t aw86907_duration_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	/* setting 0 on duration is NOP for now */
	if (val <= 0)
		return count;
	aw86907->duration = val;
	return count;
}

static ssize_t aw86907_activate_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;

	/* For now nothing to show */
	return snprintf(buf, PAGE_SIZE, "activate = %d\n", aw86907->state);
}

static ssize_t aw86907_activate_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	aw_info("%s: value=%d\n", __func__, val);
	if (!aw86907->ram_init) {
		aw_err("%s: ram init failed, not allow to play!\n",
			   __func__);
		return count;
	}
	mutex_lock(&aw86907->lock);
	hrtimer_cancel(&aw86907->timer);
	aw86907->state = val;
	mutex_unlock(&aw86907->lock);
	queue_work(aw86907->work_queue, &aw86907->vibrator_work);
	return count;
}

static ssize_t aw86907_activate_mode_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;

	return snprintf(buf, PAGE_SIZE, "activate_mode = %d\n",
			aw86907->activate_mode);
}

static ssize_t aw86907_activate_mode_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	mutex_lock(&aw86907->lock);
	aw86907->activate_mode = val;
	mutex_unlock(&aw86907->lock);
	return count;
}

static ssize_t aw86907_index_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	unsigned char reg_val = 0;

	aw86907_i2c_read(aw86907, AW86907_REG_WAVCFG1, &reg_val);
	aw86907->index = reg_val;
	return snprintf(buf, PAGE_SIZE, "index = %d\n", aw86907->index);
}

static ssize_t aw86907_index_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	if (val > aw86907->ram.ram_num) {
		aw_err("%s: input value out of range!\n", __func__);
		return count;
	}
	aw_info("%s: value=%d\n", __func__, val);
	mutex_lock(&aw86907->lock);
	aw86907->index = val;
	aw86907_haptic_set_repeat_wav_seq(aw86907, aw86907->index);
	mutex_unlock(&aw86907->lock);
	return count;
}

static ssize_t aw86907_vmax_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;

	return snprintf(buf, PAGE_SIZE, "vmax = 0x%02X\n", aw86907->vmax);
}

static ssize_t aw86907_vmax_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	aw_info("%s: value=%d\n", __func__, val);

	mutex_lock(&aw86907->lock);
	aw86907->vmax = val;
	aw86907_haptic_set_bst_vol(aw86907, aw86907->vmax);
	mutex_unlock(&aw86907->lock);
	return count;
}

static ssize_t aw86907_gain_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;

	return snprintf(buf, PAGE_SIZE, "gain = 0x%02X\n", aw86907->gain);
}

static ssize_t aw86907_gain_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	aw_info("%s: value=%d\n", __func__, val);

	mutex_lock(&aw86907->lock);
	aw86907->gain = val;
	aw86907_haptic_set_gain(aw86907, aw86907->gain);
	mutex_unlock(&aw86907->lock);
	return count;
}

static ssize_t aw86907_seq_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	size_t count = 0;
	unsigned char i = 0;
	unsigned char reg_val = 0;

	for (i = 0; i < AW86907_SEQUENCER_SIZE; i++) {
		aw86907_i2c_read(aw86907, AW86907_REG_WAVCFG1 + i, &reg_val);
		count += snprintf(buf + count, PAGE_SIZE - count,
				  "seq%d = %d\n", i + 1, reg_val);
		aw86907->seq[i] |= reg_val;
	}
	return count;
}

static ssize_t aw86907_seq_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	unsigned int databuf[2] = { 0, 0 };

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		if (databuf[0] >= AW86907_SEQUENCER_SIZE ||
		    databuf[1] > aw86907->ram.ram_num) {
			aw_err("%s: input value out of range!\n", __func__);
			return count;
		}
		aw_info("%s: seq%d=0x%02X\n", __func__,
			    databuf[0], databuf[1]);
		mutex_lock(&aw86907->lock);
		aw86907->seq[databuf[0]] = (unsigned char)databuf[1];
		aw86907_haptic_set_wav_seq(aw86907, (unsigned char)databuf[0],
					   aw86907->seq[databuf[0]]);
		mutex_unlock(&aw86907->lock);
	}
	return count;
}

static ssize_t aw86907_loop_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	size_t count = 0;
	unsigned char i = 0;
	unsigned char reg_val = 0;

	for (i = 0; i < AW86907_SEQUENCER_LOOP_SIZE; i++) {
		aw86907_i2c_read(aw86907, AW86907_REG_WAVCFG9 + i, &reg_val);
		aw86907->loop[i * 2 + 0] = (reg_val >> 4) & 0x0F;
		aw86907->loop[i * 2 + 1] = (reg_val >> 0) & 0x0F;

		count += snprintf(buf + count, PAGE_SIZE - count,
				  "seq%d_loop = %d\n", i * 2 + 1,
				  aw86907->loop[i * 2 + 0]);
		count += snprintf(buf + count, PAGE_SIZE - count,
				  "seq%d_loop = %d\n", i * 2 + 2,
				  aw86907->loop[i * 2 + 1]);
	}
	return count;
}

static ssize_t aw86907_loop_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	unsigned int databuf[2] = { 0, 0 };

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		aw_info("%s: seq%d loop=0x%02X\n", __func__,
			    databuf[0], databuf[1]);
		mutex_lock(&aw86907->lock);
		aw86907->loop[databuf[0]] = (unsigned char)databuf[1];
		aw86907_haptic_set_wav_loop(aw86907, (unsigned char)databuf[0],
					    aw86907->loop[databuf[0]]);
		mutex_unlock(&aw86907->lock);
	}

	return count;
}

static ssize_t aw86907_reg_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	ssize_t len = 0;
	unsigned char i = 0;
	unsigned char reg_val = 0;

	for (i = 0; i < AW86907_REG_MAX; i++) {
		if (!(aw86907_reg_access[i] & REG_RD_ACCESS))
			continue;
		aw86907_i2c_read(aw86907, i, &reg_val);
		len += snprintf(buf + len, PAGE_SIZE - len,
				"reg:0x%02X=0x%02X\n", i, reg_val);
	}
	return len;
}

static ssize_t aw86907_reg_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	unsigned int databuf[2] = { 0, 0 };

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		aw86907_i2c_write(aw86907, (unsigned char)databuf[0],
				  (unsigned char)databuf[1]);
	}

	return count;
}

static ssize_t aw86907_rtp_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "rtp_cnt = %d\n",
			aw86907->rtp_cnt);
	return len;
}

static ssize_t aw86907_rtp_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0) {
		aw_err("%s kstrtouint fail\n", __func__);
		return rc;
	}
	mutex_lock(&aw86907->lock);
	aw86907_haptic_stop(aw86907);
	aw86907_haptic_set_rtp_aei(aw86907, false);
	aw86907_interrupt_clear(aw86907);
	if (val > 0) {
		queue_work(aw86907->work_queue, &aw86907->rtp_work);
	} else {
		aw_err("%s input number error:%d\n", __func__, val);
	}
	mutex_unlock(&aw86907->lock);
	return count;
}

static ssize_t aw86907_ram_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	ssize_t len = 0;
	unsigned int i = 0;
	unsigned char reg_val = 0;

	/* RAMINIT Enable */
	aw86907_haptic_raminit(aw86907, true);
	aw86907_haptic_stop(aw86907);
	aw86907_i2c_write(aw86907, AW86907_REG_RAMADDRH,
			  (unsigned char)(aw86907->ram.base_addr >> 8));
	aw86907_i2c_write(aw86907, AW86907_REG_RAMADDRL,
			  (unsigned char)(aw86907->ram.base_addr & 0x00ff));
	len += snprintf(buf + len, PAGE_SIZE - len, "aw86907_haptic_ram:\n");
	for (i = 0; i < aw86907->ram.len; i++) {
		aw86907_i2c_read(aw86907, AW86907_REG_RAMDATA, &reg_val);
		len += snprintf(buf + len, PAGE_SIZE - len, "%02X,", reg_val);
	}
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	/* RAMINIT Disable */
	aw86907_haptic_raminit(aw86907, false);
	return len;
}

static ssize_t aw86907_ram_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	if (val)
		aw86907_ram_update(aw86907);
	return count;
}

static ssize_t aw86907_ram_update_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	ssize_t len = 0;
	int size = 0;
	int i = 0;
	int j = 0;
	unsigned char ram_data[AW_RAMDATA_RD_BUFFER_SIZE] = {0};

	/* RAMINIT Enable */
	aw86907_haptic_raminit(aw86907, true);
	aw86907_haptic_stop(aw86907);
	aw86907_i2c_write(aw86907, AW86907_REG_RAMADDRH,
			  (unsigned char)(aw86907->ram.base_addr >> 8));
	aw86907_i2c_write(aw86907, AW86907_REG_RAMADDRL,
			  (unsigned char)(aw86907->ram.base_addr & 0x00ff));
	len += snprintf(buf + len, PAGE_SIZE - len, "aw86907_haptic_ram:\n");
	while (i < aw86907->ram.len) {
		if ((aw86907->ram.len - i) < AW_RAMDATA_RD_BUFFER_SIZE)
			size = aw86907->ram.len - i;
		else
			size = AW_RAMDATA_RD_BUFFER_SIZE;

		aw86907_i2c_reads(aw86907, AW86907_REG_RAMDATA, ram_data, size);
		for (j = 0; j < size; j++) {
			len += snprintf(buf + len, PAGE_SIZE - len,
					"0x%02X,", ram_data[j]);
		}
		i += size;
	}
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	/* RAMINIT Disable */
	aw86907_haptic_raminit(aw86907, false);
	return len;
}

static ssize_t aw86907_ram_update_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	if (val)
		aw86907_ram_update(aw86907);
	return count;
}

static ssize_t aw86907_ram_num_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	ssize_t len = 0;

	aw86907_haptic_get_ram_number(aw86907);

	len += snprintf(buf + len, PAGE_SIZE - len,
			"ram_num = %d\n", aw86907->ram.ram_num);
	return len;
}

static ssize_t aw86907_f0_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	ssize_t len = 0;

	mutex_lock(&aw86907->lock);
	aw86907_haptic_upload_lra(aw86907, WRITE_ZERO);
	aw86907_haptic_cont_get_f0(aw86907);
	aw86907_haptic_upload_lra(aw86907, F0_CALI);
	mutex_unlock(&aw86907->lock);
	len += snprintf(buf + len, PAGE_SIZE - len,
			"%d\n", aw86907->f0);
	return len;
}

static ssize_t aw86907_f0_store(struct device *dev,
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

static ssize_t aw86907_f0_value_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;

	return snprintf(buf, PAGE_SIZE, "%d\n", aw86907->f0);
}

static ssize_t aw86907_osc_save_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "osc_cali_data = 0x%02X\n",
			aw86907->osc_cali_data);

	return len;
}

static ssize_t aw86907_osc_save_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	aw86907->osc_cali_data = val;
	return count;
}

static ssize_t aw86907_f0_save_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "f0_cali_data = 0x%02X\n",
			aw86907->f0_cali_data);

	return len;
}

static ssize_t aw86907_f0_save_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	aw86907->f0_cali_data = val;
	return count;
}

static ssize_t aw86907_cali_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	ssize_t len = 0;

	mutex_lock(&aw86907->lock);
	aw86907_haptic_upload_lra(aw86907, F0_CALI);
	aw86907_haptic_cont_get_f0(aw86907);
	mutex_unlock(&aw86907->lock);
	len += snprintf(buf + len, PAGE_SIZE - len, "%d\n", aw86907->f0);
	return len;
}

static ssize_t aw86907_cali_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	if (val) {
		mutex_lock(&aw86907->lock);
		aw86907_haptic_f0_calibration(aw86907);
		mutex_unlock(&aw86907->lock);
	}
	return count;
}

static ssize_t aw86907_cont_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	ssize_t len = 0;

	aw86907_haptic_read_cont_f0(aw86907);
	len += snprintf(buf + len, PAGE_SIZE - len,
			"cont_f0 = %d\n", aw86907->cont_f0);
	return len;
}

static ssize_t aw86907_cont_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	aw86907_haptic_stop(aw86907);
	if (val) {
		aw86907_haptic_upload_lra(aw86907, F0_CALI);
		aw86907_haptic_cont_config(aw86907);
	}
	return count;
}

static ssize_t aw86907_cont_wait_num_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len,
			"cont_wait_num = 0x%02X\n",
			aw86907->info.cont_wait_num);
	return len;
}

static ssize_t aw86907_cont_wait_num_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	int rc = 0;
	unsigned int val = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	aw86907->info.cont_wait_num = val;
	aw86907_i2c_write(aw86907, AW86907_REG_CONTCFG4, val);

	return count;
}

static ssize_t aw86907_cont_drv_lvl_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len,
			"cont_drv1_lvl = 0x%02X, cont_drv2_lvl = 0x%02X\n",
			aw86907->info.cont_drv1_lvl,
			aw86907->info.cont_drv2_lvl);
	return len;
}

static ssize_t aw86907_cont_drv_lvl_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	unsigned int databuf[2] = { 0, 0 };

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		aw86907->info.cont_drv1_lvl = databuf[0];
		aw86907->info.cont_drv2_lvl = databuf[1];
		aw86907_i2c_write_bits(aw86907, AW86907_REG_CONTCFG6,
				       AW86907_BIT_CONTCFG6_DRV1_LVL_MASK,
				       aw86907->info.cont_drv1_lvl);
		aw86907_i2c_write(aw86907, AW86907_REG_CONTCFG7,
				  aw86907->info.cont_drv2_lvl);
	}
	return count;
}

static ssize_t aw86907_cont_drv_time_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len,
			"cont_drv1_time = 0x%02X, cont_drv2_time = 0x%02X\n",
			aw86907->info.cont_drv1_time,
			aw86907->info.cont_drv2_time);
	return len;
}

static ssize_t aw86907_cont_drv_time_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	unsigned int databuf[2] = { 0, 0 };

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		aw86907->info.cont_drv1_time = databuf[0];
		aw86907->info.cont_drv2_time = databuf[1];
		aw86907_i2c_write(aw86907, AW86907_REG_CONTCFG8,
				  aw86907->info.cont_drv1_time);
		aw86907_i2c_write(aw86907, AW86907_REG_CONTCFG9,
				  aw86907->info.cont_drv2_time);
	}
	return count;
}

static ssize_t aw86907_cont_brk_time_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "cont_brk_time = 0x%02X\n",
			aw86907->info.cont_brk_time);
	return len;
}

static ssize_t aw86907_cont_brk_time_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	int rc = 0;
	unsigned int val = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	aw86907->info.cont_brk_time = val;
	aw86907_i2c_write(aw86907, AW86907_REG_CONTCFG10,
			  aw86907->info.cont_brk_time);
	return count;
}

static ssize_t aw86907_vbat_monitor_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	ssize_t len = 0;

	mutex_lock(&aw86907->lock);
	aw86907_haptic_get_vbat(aw86907);
	len += snprintf(buf + len, PAGE_SIZE - len, "vbat_monitor = %d\n",
			aw86907->vbat);
	mutex_unlock(&aw86907->lock);

	return len;
}

static ssize_t aw86907_vbat_monitor_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	return count;
}

static ssize_t aw86907_lra_resistance_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	ssize_t len = 0;

	aw86907_haptic_get_lra_resistance(aw86907);
	len += snprintf(buf + len, PAGE_SIZE - len, "lra_resistance = %d\n",
			aw86907->lra);
	return len;
}

static ssize_t aw86907_lra_resistance_store(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t count)
{
	return count;
}

static ssize_t aw86907_auto_boost_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "auto_boost = %d\n",
			aw86907->auto_boost);

	return len;
}

static ssize_t aw86907_auto_boost_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	mutex_lock(&aw86907->lock);
	aw86907_haptic_stop(aw86907);
	aw86907_haptic_auto_bst_enable(aw86907, val);
	mutex_unlock(&aw86907->lock);

	return count;
}

static ssize_t aw86907_prctmode_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	ssize_t len = 0;
	unsigned char reg_val = 0;

	aw86907_i2c_read(aw86907, AW86907_REG_DETCFG1, &reg_val);

	len += snprintf(buf + len, PAGE_SIZE - len, "prctmode = %d\n",
			reg_val & 0x08);
	return len;
}

static ssize_t aw86907_prctmode_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	unsigned int databuf[2] = { 0, 0 };
	unsigned int addr = 0;
	unsigned int val = 0;

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		addr = databuf[0];
		val = databuf[1];
		mutex_lock(&aw86907->lock);
		aw86907_haptic_swicth_motor_protect_config(aw86907, addr, val);
		mutex_unlock(&aw86907->lock);
	}
	return count;
}

static ssize_t aw86907_trig_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	ssize_t len = 0;
	unsigned char i = 0;

	for (i = 0; i < AW86907_TRIG_NUM; i++) {
		len += snprintf(buf + len, PAGE_SIZE - len,
				"trig%d: trig_level=%d, trig_polar=%d, pos_enable=%d, pos_sequence=%d, neg_enable=%d, neg_sequence=%d trig_brk=%d, trig_bst=%d\n",
				i + 1,
				aw86907->trig[i].trig_level,
				aw86907->trig[i].trig_polar,
				aw86907->trig[i].pos_enable,
				aw86907->trig[i].pos_sequence,
				aw86907->trig[i].neg_enable,
				aw86907->trig[i].neg_sequence,
				aw86907->trig[i].trig_brk,
				aw86907->trig[i].trig_bst);
	}

	return len;
}

static ssize_t aw86907_trig_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
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
		if (databuf[0] == 1 && aw86907->info.is_enabled_one_wire) {
			aw_info("%s: trig1 pin used for one wire!\n",
				    __func__);
			return count;
		}
		if ((databuf[0] == 2 || databuf[0] == 3) &&
		     aw86907->info.is_enabled_i2s) {
			aw_info("%s: trig2 and trig3 pin used for i2s!\n",
				    __func__);
			return count;
		}
		if (!aw86907->ram_init) {
			aw_err("%s: ram init failed, not allow to play!\n",
				   __func__);
			return count;
		}
		if (databuf[4] > aw86907->ram.ram_num ||
		    databuf[6] > aw86907->ram.ram_num) {
			aw_err("%s: input seq value out of range!\n",
				   __func__);
			return count;
		}
		databuf[0] -= 1;

		aw86907->trig[databuf[0]].trig_level = databuf[1];
		aw86907->trig[databuf[0]].trig_polar = databuf[2];
		aw86907->trig[databuf[0]].pos_enable = databuf[3];
		aw86907->trig[databuf[0]].pos_sequence = databuf[4];
		aw86907->trig[databuf[0]].neg_enable = databuf[5];
		aw86907->trig[databuf[0]].neg_sequence = databuf[6];
		aw86907->trig[databuf[0]].trig_brk = databuf[7];
		aw86907->trig[databuf[0]].trig_bst = databuf[8];
		mutex_lock(&aw86907->lock);
		switch (databuf[0]) {
		case 0:
			aw86907_haptic_trig1_param_config(aw86907);
			break;
		case 1:
			aw86907_haptic_trig2_param_config(aw86907);
			break;
		case 2:
			aw86907_haptic_trig3_param_config(aw86907);
			break;
		}
		mutex_unlock(&aw86907->lock);
	}
	return count;
}

static ssize_t aw86907_ram_vbat_compensate_show(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len,
			"ram_vbat_compensate = %d\n",
			aw86907->ram_vbat_comp);

	return len;
}

static ssize_t aw86907_ram_vbat_compensate_store(struct device *dev,
						 struct device_attribute *attr,
						 const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	mutex_lock(&aw86907->lock);
	if (val)
		aw86907->ram_vbat_comp =
		    AW86907_HAPTIC_RAM_VBAT_COMP_ENABLE;
	else
		aw86907->ram_vbat_comp =
		    AW86907_HAPTIC_RAM_VBAT_COMP_DISABLE;
	mutex_unlock(&aw86907->lock);

	return count;
}

static ssize_t aw86907_osc_cali_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "osc_cali_data = 0x%02X\n",
			aw86907->osc_cali_data);

	return len;
}

static ssize_t aw86907_osc_cali_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;
	mutex_lock(&aw86907->lock);
	if (val == 3) {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_D2SCFG1,
				       AW86907_BIT_D2SCFG1_CLK_TRIM_MODE_MASK,
				       AW86907_BIT_D2SCFG1_CLK_TRIM_MODE_24K);
		aw86907_haptic_upload_lra(aw86907, WRITE_ZERO);
		aw86907_rtp_osc_calibration(aw86907);
		aw86907_rtp_trim_lra_calibration(aw86907);
	} else if (val == 1) {
		aw86907_i2c_write_bits(aw86907, AW86907_REG_D2SCFG1,
				       AW86907_BIT_D2SCFG1_CLK_TRIM_MODE_MASK,
				       AW86907_BIT_D2SCFG1_CLK_TRIM_MODE_24K);
		aw86907_haptic_upload_lra(aw86907, OSC_CALI);
		aw86907_rtp_osc_calibration(aw86907);
	}  else {
		aw_err("%s input value out of range\n", __func__);
	}
	/* osc calibration flag end,Other behaviors are permitted */
	mutex_unlock(&aw86907->lock);

	return count;
}

static ssize_t aw86907_haptic_audio_time_show(struct device *dev,
					      struct device_attribute *attr,
					      char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;

	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len,
			"haptic_audio.delay_val=%dus\n",
			aw86907->haptic_audio.delay_val);
	len += snprintf(buf + len, PAGE_SIZE - len,
			"haptic_audio.timer_val=%dus\n",
			aw86907->haptic_audio.timer_val);
	return len;
}

static ssize_t aw86907_haptic_audio_time_store(struct device *dev,
					       struct device_attribute *attr,
					       const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;

	unsigned int databuf[2] = { 0 };

	if (sscanf(buf, "%d %d", &databuf[0], &databuf[1]) == 2) {
		aw86907->haptic_audio.delay_val = databuf[0];
		aw86907->haptic_audio.timer_val = databuf[1];
	}

	return count;
}

static ssize_t aw86907_gun_type_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;

	return snprintf(buf, PAGE_SIZE, "0x%02x\n", aw86907->gun_type);
}

static ssize_t aw86907_gun_type_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;

	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	aw_dbg("%s: value=%d\n", __func__, val);

	mutex_lock(&aw86907->lock);
	aw86907->gun_type = val;
	mutex_unlock(&aw86907->lock);
	return count;
}

static ssize_t aw86907_bullet_nr_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;

	return snprintf(buf, PAGE_SIZE, "0x%02x\n", aw86907->bullet_nr);
}

static ssize_t aw86907_bullet_nr_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;

	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	aw_dbg("%s: value=%d\n", __func__, val);

	mutex_lock(&aw86907->lock);
	aw86907->bullet_nr = val;
	mutex_unlock(&aw86907->lock);
	return count;
}

static ssize_t aw86907_f0_check_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	ssize_t len = 0;

	if (aw86907->f0_cali_status == true)
		len += snprintf(buf + len, PAGE_SIZE - len, "%d\n", 1);
	if (aw86907->f0_cali_status == false)
		len += snprintf(buf + len, PAGE_SIZE - len, "%d\n", 0);

	return len;
}

static ssize_t aw86907_effect_id_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;

	return snprintf(buf, PAGE_SIZE, "effect_id =%d\n", aw86907->effect_id);
}

static ssize_t aw86907_effect_id_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	mutex_lock(&aw86907->lock);
	aw86907->effect_id = val;
	aw86907->play.vmax_mv = AW86907_MEDIUM_MAGNITUDE;
	mutex_unlock(&aw86907->lock);
	return count;
}

/* return buffer size and availbe size */
static ssize_t aw86907_custom_wave_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	ssize_t len = 0;

	len +=
		snprintf(buf + len, PAGE_SIZE - len, "period_size=%d;",
		aw86907->ram.base_addr >> 2);
	len +=
		snprintf(buf + len, PAGE_SIZE - len,
		"max_size=%d;free_size=%d;",
		get_rb_max_size(), get_rb_free_size());
	len +=
		snprintf(buf + len, PAGE_SIZE - len,
		"custom_wave_id=%d;", CUSTOME_WAVE_ID);
	return len;
}

static ssize_t aw86907_custom_wave_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw86907 *aw86907 = awinic->aw86907;
	unsigned long  buf_len, period_size, offset;
	int ret;

	period_size = (aw86907->ram.base_addr >> 2);
	offset = 0;

	aw_dbg("%swrite szie %zd, period size %lu", __func__, count,
		 period_size);
	if (count % period_size || count < period_size)
		rb_end();
	atomic_set(&aw86907->is_in_write_loop, 1);

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
	atomic_set(&aw86907->is_in_write_loop, 0);
	wake_up_interruptible(&aw86907->stop_wait_q);
	aw_dbg(" return size %d", ret);
	return ret;
}

static DEVICE_ATTR(custom_wave, S_IWUSR | S_IRUGO, aw86907_custom_wave_show,
		   aw86907_custom_wave_store);
static DEVICE_ATTR(effect_id, S_IWUSR | S_IRUGO, aw86907_effect_id_show,
		   aw86907_effect_id_store);
static DEVICE_ATTR(f0_check, S_IRUGO, aw86907_f0_check_show, NULL);
static DEVICE_ATTR(bst_vol, S_IWUSR | S_IRUGO, aw86907_bst_vol_show,
		   aw86907_bst_vol_store);
static DEVICE_ATTR(state, S_IWUSR | S_IRUGO, aw86907_state_show,
		   aw86907_state_store);
static DEVICE_ATTR(duration, S_IWUSR | S_IRUGO, aw86907_duration_show,
		   aw86907_duration_store);
static DEVICE_ATTR(activate, S_IWUSR | S_IRUGO, aw86907_activate_show,
		   aw86907_activate_store);
static DEVICE_ATTR(activate_mode, S_IWUSR | S_IRUGO, aw86907_activate_mode_show,
		   aw86907_activate_mode_store);
static DEVICE_ATTR(index, S_IWUSR | S_IRUGO, aw86907_index_show,
		   aw86907_index_store);
static DEVICE_ATTR(vmax, S_IWUSR | S_IRUGO, aw86907_vmax_show,
		   aw86907_vmax_store);
static DEVICE_ATTR(gain, S_IWUSR | S_IRUGO, aw86907_gain_show,
		   aw86907_gain_store);
static DEVICE_ATTR(seq, S_IWUSR | S_IRUGO, aw86907_seq_show, aw86907_seq_store);
static DEVICE_ATTR(loop, S_IWUSR | S_IRUGO, aw86907_loop_show,
		   aw86907_loop_store);
static DEVICE_ATTR(reg, S_IWUSR | S_IRUGO, aw86907_reg_show, aw86907_reg_store);
static DEVICE_ATTR(rtp, S_IWUSR | S_IRUGO, aw86907_rtp_show, aw86907_rtp_store);
static DEVICE_ATTR(ram, S_IWUSR | S_IRUGO, aw86907_ram_show, aw86907_ram_store);
static DEVICE_ATTR(ram_update, S_IWUSR | S_IRUGO, aw86907_ram_update_show,
		   aw86907_ram_update_store);
static DEVICE_ATTR(ram_num, S_IWUSR | S_IRUGO, aw86907_ram_num_show,
		   NULL);
static DEVICE_ATTR(f0, S_IWUSR | S_IRUGO, aw86907_f0_show, aw86907_f0_store);
static DEVICE_ATTR(f0_value, S_IWUSR | S_IRUGO, aw86907_f0_value_show, NULL);
static DEVICE_ATTR(f0_save, S_IWUSR | S_IRUGO, aw86907_f0_save_show,
		   aw86907_f0_save_store);
static DEVICE_ATTR(osc_save, S_IWUSR | S_IRUGO, aw86907_osc_save_show,
		   aw86907_osc_save_store);
static DEVICE_ATTR(cali, S_IWUSR | S_IRUGO, aw86907_cali_show,
		   aw86907_cali_store);
static DEVICE_ATTR(cont, S_IWUSR | S_IRUGO, aw86907_cont_show,
		   aw86907_cont_store);
static DEVICE_ATTR(cont_wait_num, S_IWUSR | S_IRUGO, aw86907_cont_wait_num_show,
		   aw86907_cont_wait_num_store);
static DEVICE_ATTR(cont_drv_lvl, S_IWUSR | S_IRUGO, aw86907_cont_drv_lvl_show,
		   aw86907_cont_drv_lvl_store);
static DEVICE_ATTR(cont_drv_time, S_IWUSR | S_IRUGO, aw86907_cont_drv_time_show,
		   aw86907_cont_drv_time_store);
static DEVICE_ATTR(cont_brk_time, S_IWUSR | S_IRUGO, aw86907_cont_brk_time_show,
		   aw86907_cont_brk_time_store);
static DEVICE_ATTR(vbat_monitor, S_IWUSR | S_IRUGO, aw86907_vbat_monitor_show,
		   aw86907_vbat_monitor_store);
static DEVICE_ATTR(lra_resistance, S_IWUSR | S_IRUGO,
		   aw86907_lra_resistance_show, aw86907_lra_resistance_store);
static DEVICE_ATTR(auto_boost, S_IWUSR | S_IRUGO, aw86907_auto_boost_show,
		   aw86907_auto_boost_store);
static DEVICE_ATTR(prctmode, S_IWUSR | S_IRUGO, aw86907_prctmode_show,
		   aw86907_prctmode_store);
static DEVICE_ATTR(trig, S_IWUSR | S_IRUGO, aw86907_trig_show,
		   aw86907_trig_store);
static DEVICE_ATTR(ram_vbat_comp, S_IWUSR | S_IRUGO,
		   aw86907_ram_vbat_compensate_show,
		   aw86907_ram_vbat_compensate_store);
static DEVICE_ATTR(osc_cali, S_IWUSR | S_IRUGO, aw86907_osc_cali_show,
		   aw86907_osc_cali_store);
static DEVICE_ATTR(haptic_audio_time, S_IWUSR | S_IRUGO,
		   aw86907_haptic_audio_time_show,
		   aw86907_haptic_audio_time_store);
static DEVICE_ATTR(gun_type, S_IWUSR | S_IRUGO, aw86907_gun_type_show,
		   aw86907_gun_type_store);
static DEVICE_ATTR(bullet_nr, S_IWUSR | S_IRUGO, aw86907_bullet_nr_show,
		   aw86907_bullet_nr_store);
static struct attribute *aw86907_vibrator_attributes[] = {
	&dev_attr_bst_vol.attr,
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
	&dev_attr_f0_value.attr,
	&dev_attr_f0_save.attr,
	&dev_attr_osc_save.attr,
	&dev_attr_cali.attr,
	&dev_attr_cont.attr,
	&dev_attr_cont_wait_num.attr,
	&dev_attr_cont_drv_lvl.attr,
	&dev_attr_cont_drv_time.attr,
	&dev_attr_cont_brk_time.attr,
	&dev_attr_vbat_monitor.attr,
	&dev_attr_lra_resistance.attr,
	&dev_attr_auto_boost.attr,
	&dev_attr_prctmode.attr,
	&dev_attr_trig.attr,
	&dev_attr_ram_vbat_comp.attr,
	&dev_attr_osc_cali.attr,
	&dev_attr_haptic_audio_time.attr,
	&dev_attr_gun_type.attr,
	&dev_attr_bullet_nr.attr,
	&dev_attr_f0_check.attr,
	&dev_attr_effect_id.attr,
	&dev_attr_custom_wave.attr,
	NULL
};

struct attribute_group aw86907_vibrator_attribute_group = {
	.attrs = aw86907_vibrator_attributes
};

static enum hrtimer_restart aw86907_vibrator_timer_func(struct hrtimer *timer)
{
	struct aw86907 *aw86907 = container_of(timer, struct aw86907, timer);

	aw_info("%s enter\n", __func__);
	aw86907->state = 0;
	queue_work(aw86907->work_queue, &aw86907->vibrator_work);

	return HRTIMER_NORESTART;
}

static int aw86907_haptic_play_effect_seq(struct aw86907 *aw86907,
					 unsigned char flag)
{
	if (aw86907->effect_id > aw86907->info.effect_id_boundary)
		return 0;

	if (flag) {
		if (aw86907->activate_mode ==
		    AW86907_HAPTIC_ACTIVATE_RAM_MODE) {
			aw86907_haptic_set_wav_seq(aw86907, 0x00,
						(char)aw86907->effect_id + 1);
			aw86907_haptic_set_wav_seq(aw86907, 0x01, 0x00);
			aw86907_haptic_set_wav_loop(aw86907, 0x00, 0x00);
			aw86907_haptic_play_mode(aw86907,
						 AW86907_HAPTIC_RAM_MODE);
			if (aw86907->info.bst_vol_ram <= AW86907_MAX_BST_VOL)
				aw86907_haptic_set_bst_vol(aw86907,
						aw86907->info.bst_vol_ram);
			else
				aw86907_haptic_set_bst_vol(aw86907,
							   aw86907->vmax);
			aw86907_haptic_effect_strength(aw86907);
			aw86907_haptic_set_gain(aw86907, aw86907->level);
			aw86907_haptic_play_go(aw86907);
		}
		if (aw86907->activate_mode ==
		    AW86907_HAPTIC_ACTIVATE_RAM_LOOP_MODE) {
			aw86907_haptic_play_mode(aw86907,
						 AW86907_HAPTIC_RAM_LOOP_MODE);
			aw86907_haptic_set_repeat_wav_seq(aw86907,
					(aw86907->info.effect_id_boundary + 1));
			aw86907_haptic_set_gain(aw86907, aw86907->level);
			aw86907_haptic_play_go(aw86907);
		}
	}

	return 0;
}

static void aw86907_vibrator_work_routine(struct work_struct *work)
{
	struct aw86907 *aw86907 = container_of(work, struct aw86907,
					       vibrator_work);

	aw_info("%s enter\n", __func__);
	aw_info("%s: effect_id = %d state=%d activate_mode = %d duration = %d\n",
		__func__,
		aw86907->effect_id, aw86907->state, aw86907->activate_mode,
		aw86907->duration);

	mutex_lock(&aw86907->lock);
	/* Enter standby mode */
	aw86907_haptic_upload_lra(aw86907, F0_CALI);
	aw86907_haptic_stop(aw86907);
	if (aw86907->state) {
		if (aw86907->activate_mode ==
		    AW86907_HAPTIC_ACTIVATE_RAM_MODE) {
			aw86907_haptic_ram_vbat_comp(aw86907, false);
			aw86907_haptic_play_effect_seq(aw86907, true);
		} else if (aw86907->activate_mode ==
			   AW86907_HAPTIC_ACTIVATE_RAM_LOOP_MODE) {
			aw86907->level = 0x80;
			aw86907_haptic_ram_vbat_comp(aw86907, true);
			aw86907_haptic_play_effect_seq(aw86907, true);
			hrtimer_start(&aw86907->timer,
				      ktime_set(aw86907->duration / 1000,
						(aw86907->duration % 1000) *
						1000000), HRTIMER_MODE_REL);
		} else if (aw86907->activate_mode ==
			   AW86907_HAPTIC_ACTIVATE_CONT_MODE) {
			aw86907_haptic_cont_config(aw86907);
			/* run ms timer */
			hrtimer_start(&aw86907->timer,
				      ktime_set(aw86907->duration / 1000,
						(aw86907->duration % 1000) *
						1000000), HRTIMER_MODE_REL);
		} else {
			aw_err("%s: activate_mode error\n", __func__);
		}

	}
	mutex_unlock(&aw86907->lock);
}

int aw86907_vibrator_init(struct aw86907 *aw86907)
{
	int ret = 0;

	aw_info("%s enter\n", __func__);
	ret = sysfs_create_group(&aw86907->i2c->dev.kobj,
				 &aw86907_vibrator_attribute_group);
	if (ret < 0) {
		aw_err("%s error creating sysfs attr files\n",
			   __func__);
		return ret;
	}

	hrtimer_init(&aw86907->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	aw86907->timer.function = aw86907_vibrator_timer_func;
	INIT_WORK(&aw86907->vibrator_work, aw86907_vibrator_work_routine);
	INIT_WORK(&aw86907->rtp_work, aw86907_rtp_work_routine);
	mutex_init(&aw86907->lock);
	mutex_init(&aw86907->rtp_lock);
	atomic_set(&aw86907->is_in_rtp_loop, 0);
	atomic_set(&aw86907->exit_in_rtp_loop, 0);
	atomic_set(&aw86907->is_in_write_loop, 0);
	init_waitqueue_head(&aw86907->wait_q);
	init_waitqueue_head(&aw86907->stop_wait_q);
	return 0;
}

/******************************************************
 *
 * irq
 *
 ******************************************************/
static void aw86907_interrupt_clear(struct aw86907 *aw86907)
{
	unsigned char reg_val = 0;

	aw_info("%s enter\n", __func__);
	aw86907_i2c_read(aw86907, AW86907_REG_SYSINT, &reg_val);
	aw_dbg("%s: reg SYSINT=0x%02X\n", __func__, reg_val);
}

void aw86907_interrupt_setup(struct aw86907 *aw86907)
{
	unsigned char reg_val = 0;

	aw_info("%s enter\n", __func__);

	aw86907_i2c_read(aw86907, AW86907_REG_SYSINT, &reg_val);

	aw_info("%s: reg SYSINT=0x%02X\n", __func__, reg_val);

	/* edge int mode */
	aw86907_i2c_write_bits(aw86907, AW86907_REG_SYSCTRL7,
			       AW86907_BIT_SYSCTRL7_INT_MODE_MASK,
			       AW86907_BIT_SYSCTRL7_INT_MODE_EDGE);
	aw86907_i2c_write_bits(aw86907, AW86907_REG_SYSCTRL7,
			       AW86907_BIT_SYSCTRL7_INT_EDGE_MODE_MASK,
			       AW86907_BIT_SYSCTRL7_INT_EDGE_MODE_POS);
	/* int enable */
	aw86907_i2c_write_bits(aw86907, AW86907_REG_SYSINTM,
			       AW86907_BIT_SYSINTM_BST_SCPM_MASK,
			       AW86907_BIT_SYSINTM_BST_SCPM_ON);
	aw86907_i2c_write_bits(aw86907, AW86907_REG_SYSINTM,
			       AW86907_BIT_SYSINTM_BST_OVPM_MASK,
			       AW86907_BIT_SYSINTM_BST_OVPM_OFF);
	aw86907_i2c_write_bits(aw86907, AW86907_REG_SYSINTM,
			       AW86907_BIT_SYSINTM_UVLM_MASK,
			       AW86907_BIT_SYSINTM_UVLM_ON);
	aw86907_i2c_write_bits(aw86907, AW86907_REG_SYSINTM,
			       AW86907_BIT_SYSINTM_OCDM_MASK,
			       AW86907_BIT_SYSINTM_OCDM_ON);
	aw86907_i2c_write_bits(aw86907, AW86907_REG_SYSINTM,
			       AW86907_BIT_SYSINTM_OTM_MASK,
			       AW86907_BIT_SYSINTM_OTM_ON);
}

irqreturn_t aw86907_irq(int irq, void *data)
{
	struct aw86907 *aw86907 = data;
	unsigned char reg_val = 0;
	unsigned char glb_state_val = 0;
	unsigned int buf_len = 0;
	unsigned int period_size = aw86907->ram.base_addr >> 2;

	atomic_set(&aw86907->is_in_rtp_loop, 1);
	aw_info("%s enter\n", __func__);
	aw86907_i2c_read(aw86907, AW86907_REG_SYSINT, &reg_val);
	aw_info("%s: reg SYSINT=0x%02X\n", __func__, reg_val);
	if (reg_val & AW86907_BIT_SYSINT_BST_OVPI) {
		aw86907_clean_status(aw86907);
		aw_err("%s chip ov int error\n", __func__);
	}
	if (reg_val & AW86907_BIT_SYSINT_UVLI) {
		aw86907_clean_status(aw86907);
		aw_err("%s chip uvlo int error\n", __func__);
	}
	if (reg_val & AW86907_BIT_SYSINT_OCDI) {
		aw86907_clean_status(aw86907);
		aw_err("%s chip over current int error\n",
			   __func__);
	}
	if (reg_val & AW86907_BIT_SYSINT_OTI) {
		aw86907_clean_status(aw86907);
		aw_err("%s chip over temperature int error\n",
			   __func__);
	}
	if (reg_val & AW86907_BIT_SYSINT_DONEI) {
		aw86907_clean_status(aw86907);
		aw_info("%s chip playback done\n", __func__);
	}

	if ((reg_val & AW86907_BIT_SYSINT_FF_AEI) && (aw86907->rtp_init)) {
		aw_info("%s: aw86907 rtp fifo almost empty\n",
			    __func__);
		while ((!aw86907_haptic_rtp_get_fifo_afs(aw86907)) &&
		       (aw86907->play_mode == AW86907_HAPTIC_RTP_MODE) &&
		       !atomic_read(&aw86907->exit_in_rtp_loop)) {
			mutex_lock(&aw86907->rtp_lock);
			if (!aw86907->rtp_cnt) {
				aw_info("%s:aw86907->rtp_cnt is 0!\n",
					    __func__);
				mutex_unlock(&aw86907->rtp_lock);
				break;
			}
#ifdef AW_ENABLE_RTP_PRINT_LOG
			aw_info("%s:rtp mode fifo update, cnt=%d\n",
				    __func__, aw86907->rtp_cnt);
#endif
			if (!aw86907_rtp) {
				aw_info("%s:aw86907_rtp is null, break!\n",
					    __func__);
				mutex_unlock(&aw86907->rtp_lock);
				break;
			}

			if (aw86907->is_custom_wave == 1) {
				buf_len = read_rb(aw86907_rtp->data,
						  period_size);
				aw86907_i2c_writes(aw86907,
						AW86907_REG_RTPDATA,
						aw86907_rtp->data,
						buf_len);
				aw86907->rtp_cnt += buf_len;
				if (buf_len < period_size) {
					aw_info("%s: rtp update complete\n",
						__func__);
					aw86907_haptic_set_rtp_aei(aw86907,
								  false);
					aw86907->rtp_cnt = 0;
					aw86907->rtp_init = 0;
					mutex_unlock(&aw86907->rtp_lock);
					break;
				}
			} else {
				if ((aw86907_rtp->len - aw86907->rtp_cnt) <
				    period_size) {
					buf_len = aw86907_rtp->len -
							aw86907->rtp_cnt;
				} else {
					buf_len = period_size;
				}
				aw86907_i2c_writes(aw86907,
						AW86907_REG_RTPDATA,
						&aw86907_rtp->
						data[aw86907->rtp_cnt],
						buf_len);
				aw86907->rtp_cnt += buf_len;
				aw86907_i2c_read(aw86907, AW86907_REG_GLBRD5,
					 &glb_state_val);
				if ((glb_state_val & 0x0f) == 0) {
					if (aw86907->rtp_cnt !=
					    aw86907_rtp->len)
						aw_err("%s: rtp play suspend!\n",
							__func__);
					else
						aw_info("%s: rtp update complete!\n",
							__func__);
					aw86907_clean_status(aw86907);
					aw86907_haptic_set_rtp_aei(aw86907,
								   false);
					aw86907->rtp_cnt = 0;
					aw86907->rtp_init = 0;
					mutex_unlock(&aw86907->rtp_lock);
					break;
				}
			}
			mutex_unlock(&aw86907->rtp_lock);
		}

	}

	if (reg_val & AW86907_BIT_SYSINT_FF_AFI)
		aw_info("%s: aw86907 rtp mode fifo almost full!\n",
			    __func__);

	if (aw86907->play_mode != AW86907_HAPTIC_RTP_MODE ||
	    atomic_read(&aw86907->exit_in_rtp_loop))
		aw86907_haptic_set_rtp_aei(aw86907, false);

	aw86907_i2c_read(aw86907, AW86907_REG_SYSINT, &reg_val);
	aw_dbg("%s: reg SYSINT=0x%x\n", __func__, reg_val);
	aw86907_i2c_read(aw86907, AW86907_REG_SYSST, &reg_val);
	aw_dbg("%s: reg SYSST=0x%x\n", __func__, reg_val);
	atomic_set(&aw86907->is_in_rtp_loop, 0);
	wake_up_interruptible(&aw86907->wait_q);
	aw_info("%s exit\n", __func__);

	return IRQ_HANDLED;
}

/*****************************************************
 *
 * device tree
 *
 *****************************************************/
int aw86907_parse_dt( struct aw86907 *aw86907, struct device *dev,
		     struct device_node *np)
{
	unsigned int val = 0;
	struct qti_hap_config *config = &aw86907->config;
	struct device_node *child_node;
	struct qti_hap_effect *effect;
	int rc = 0, tmp, i = 0, j;
	unsigned int rtp_time[175];
	unsigned int bstcfg_temp[5] = { 0x2a, 0x24, 0x9a, 0x40, 0x91 };
	unsigned int prctmode_temp[3];
	unsigned int sine_array_temp[4] = { 0x05, 0xB2, 0xFF, 0xEF };
	unsigned int trig_config_temp[24] = { 1, 0, 1, 1, 1, 2, 0, 0,
		1, 0, 0, 1, 0, 2, 0, 0,
		1, 0, 0, 1, 0, 2, 0, 0
	};

	val = of_property_read_u32(np, "aw86907_vib_mode", &aw86907->info.mode);
	if (val != 0)
		aw_info("%s vib_mode not found\n", __func__);
	val = of_property_read_u32(np, "aw86907_vib_brk_bst_md",
				   &aw86907->info.brk_bst_md);
	if (val != 0)
		aw_info("%s vib_brk_bst_md not found\n", __func__);
	val = of_property_read_u32(np, "aw86907_vib_f0_ref", &aw86907->info.f0_ref);
	if (val != 0)
		aw_info("%s vib_f0_ref not found\n", __func__);
	val = of_property_read_u32(np, "aw86907_vib_f0_cali_percent",
				   &aw86907->info.f0_cali_percent);
	if (val != 0)
		aw_info("%s vib_f0_cali_percent not found\n", __func__);

	val = of_property_read_u32(np, "aw86907_vib_cont_drv1_lvl",
				   &aw86907->info.cont_drv1_lvl);
	if (val != 0)
		aw_info("%s vib_cont_drv1_lvl not found\n", __func__);
	val = of_property_read_u32(np, "aw86907_vib_cont_drv2_lvl",
				   &aw86907->info.cont_drv2_lvl);
	if (val != 0)
		aw_info("%s vib_cont_drv2_lvl not found\n", __func__);
	val = of_property_read_u32(np, "aw86907_vib_cont_drv1_time",
				   &aw86907->info.cont_drv1_time);
	if (val != 0)
		aw_info("%s vib_cont_drv1_time not found\n", __func__);
	val = of_property_read_u32(np, "aw86907_vib_cont_drv2_time",
				   &aw86907->info.cont_drv2_time);
	if (val != 0)
		aw_info("%s vib_cont_drv2_time not found\n", __func__);
	val = of_property_read_u32(np, "aw86907_vib_cont_drv_width",
				   &aw86907->info.cont_drv_width);
	if (val != 0)
		aw_info("%s vib_cont_drv_width not found\n", __func__);
	val = of_property_read_u32(np, "aw86907_vib_cont_wait_num",
				   &aw86907->info.cont_wait_num);
	if (val != 0)
		aw_info("%s vib_cont_wait_num not found\n", __func__);
	val = of_property_read_u32(np, "aw86907_vib_cont_bst_brk_gain",
				   &aw86907->info.cont_bst_brk_gain);
	if (val != 0)
		aw_info("%s vib_cont_bst_brk_gain not found\n", __func__);
	val = of_property_read_u32(np, "aw86907_vib_cont_brk_gain",
				   &aw86907->info.cont_brk_gain);
	if (val != 0)
		aw_info("%s vib_cont_brk_gain not found\n", __func__);
	val =
	    of_property_read_u32(np, "aw86907_vib_cont_tset", &aw86907->info.cont_tset);
	if (val != 0)
		aw_info("%s vib_cont_tset not found\n", __func__);
	val = of_property_read_u32(np, "aw86907_vib_cont_bemf_set",
				   &aw86907->info.cont_bemf_set);
	if (val != 0)
		aw_info("%s vib_cont_bemf_set not found\n", __func__);
	val = of_property_read_u32(np, "aw86907_vib_d2s_gain", &aw86907->info.d2s_gain);
	if (val != 0)
		aw_info("%s vib_d2s_gain not found\n", __func__);
	val = of_property_read_u32(np, "aw86907_vib_cont_brk_time",
				   &aw86907->info.cont_brk_time);
	if (val != 0)
		aw_info("%s vib_cont_brk_time not found\n", __func__);
	val = of_property_read_u32(np, "aw86907_vib_cont_track_margin",
				   &aw86907->info.cont_track_margin);
	if (val != 0)
		aw_info("%s vib_cont_track_margin not found\n", __func__);
	aw86907->info.is_enabled_auto_bst =
			of_property_read_bool(np, "aw86907_vib_is_enabled_auto_bst");
	aw_info("%s aw86907->info.is_enabled_auto_bst = %d\n", __func__,
		    aw86907->info.is_enabled_auto_bst);
	aw86907->info.is_enabled_i2s =
			of_property_read_bool(np, "aw86907_vib_is_enabled_i2s");
	aw_info("%s aw86907->info.is_enabled_i2s = %d\n",
		    __func__, aw86907->info.is_enabled_i2s);
	aw86907->info.is_enabled_one_wire =
			of_property_read_bool(np, "aw86907_vib_is_enabled_one_wire");
	aw_info("%s aw86907->info.is_enabled_one_wire = %d\n",
		    __func__, aw86907->info.is_enabled_one_wire);
	aw86907->info.powerup_f0_cali =
			of_property_read_bool(np, "aw86907_vib_powerup_f0_cali");
	aw_info("%s aw86907->info.vib_powerup_f0_cali = %d\n",
		    __func__, aw86907->info.powerup_f0_cali);
	val = of_property_read_u32_array(np, "aw86907_vib_bstcfg", bstcfg_temp,
					 ARRAY_SIZE(bstcfg_temp));
	if (val != 0)
		aw_info("%s vib_bstcfg not found\n", __func__);
	memcpy(aw86907->info.bstcfg, bstcfg_temp, sizeof(bstcfg_temp));

	val = of_property_read_u32(np, "aw86907_vib_bst_vol_default",
				   &aw86907->info.bst_vol_default);
	if (val != 0)
		aw_info("%s vib_bst_vol_default not found\n", __func__);
	val = of_property_read_u32(np, "aw86907_vib_bst_vol_ram",
				   &aw86907->info.bst_vol_ram);
	if (val != 0)
		aw_info("%s vib_bst_vol_ram not found\n", __func__);
	val = of_property_read_u32(np, "aw86907_vib_bst_vol_rtp",
				   &aw86907->info.bst_vol_rtp);
	if (val != 0)
		aw_info("%s vib_bst_vol_rtp not found\n", __func__);

	val = of_property_read_u32_array(np, "aw86907_vib_prctmode",
					 prctmode_temp,
					 ARRAY_SIZE(prctmode_temp));
	if (val != 0)
		aw_info("%s vib_prctmode not found\n", __func__);
	memcpy(aw86907->info.prctmode, prctmode_temp, sizeof(prctmode_temp));
	val = of_property_read_u32_array(np, "aw86907_vib_sine_array", sine_array_temp,
					 ARRAY_SIZE(sine_array_temp));
	if (val != 0)
		aw_info("%s vib_sine_array not found\n", __func__);
	memcpy(aw86907->info.sine_array, sine_array_temp,
	       sizeof(sine_array_temp));
	val =
	    of_property_read_u32_array(np, "aw86907_vib_trig_config", trig_config_temp,
				       ARRAY_SIZE(trig_config_temp));
	if (val != 0)
		aw_info("%s vib_trig_config not found\n",  __func__);
	memcpy(aw86907->info.trig_config, trig_config_temp,
	       sizeof(trig_config_temp));

	val = of_property_read_u32(np, "vib_effect_id_boundary",
				 &aw86907->info.effect_id_boundary);
	if (val != 0)
		aw_info("%s vib_effect_id_boundary not found\n", __func__);
	val = of_property_read_u32(np, "vib_effect_max",
				 &aw86907->info.effect_max);
	if (val != 0)
		aw_info("%s vib_effect_max not found\n", __func__);
	val = of_property_read_u32_array(np, "vib_rtp_time", rtp_time,
				       ARRAY_SIZE(rtp_time));
	if (val != 0)
		aw_info("%s vib_rtp_time not found\n", __func__);
	memcpy(aw86907->info.rtp_time, rtp_time, sizeof(rtp_time));
	config->play_rate_us = HAP_PLAY_RATE_US_DEFAULT;
	rc = of_property_read_u32(np, "play-rate-us", &tmp);
	if (!rc)
	config->play_rate_us = (tmp >= HAP_PLAY_RATE_US_MAX) ?
		HAP_PLAY_RATE_US_MAX : tmp;

	aw86907->constant.pattern = devm_kcalloc(aw86907->dev,
						HAP_WAVEFORM_BUFFER_MAX,
						sizeof(u8), GFP_KERNEL);
	if (!aw86907->constant.pattern)
		return -ENOMEM;

	tmp = of_get_available_child_count(np);
	aw86907->predefined = devm_kcalloc(aw86907->dev, tmp,
					   sizeof(*aw86907->predefined),
					   GFP_KERNEL);
	if (!aw86907->predefined)
		return -ENOMEM;

	aw86907->effects_count = tmp;
	for_each_available_child_of_node(np, child_node) {
		effect = &aw86907->predefined[i++];
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
		effect->pattern = devm_kcalloc(aw86907->dev,
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

	aw_info("%s aw86907->info.brk_bst_md: %d\n",
		    __func__, aw86907->info.brk_bst_md);
	aw_info("%s aw86907->info.bst_vol_default: %d\n",
		    __func__, aw86907->info.bst_vol_default);
	aw_info("%s aw86907->info.bst_vol_ram: %d\n",
		    __func__, aw86907->info.bst_vol_ram);
	aw_info("%s aw86907->info.bst_vol_rtp: %d\n",
		    __func__, aw86907->info.bst_vol_rtp);

	return 0;
}


int aw86907_check_qualify(struct aw86907 *aw86907)
{
	int ret = -1;
	unsigned char reg = 0;

	ret = aw86907_i2c_read(aw86907, 0x64, &reg);
	if (ret < 0) {
		aw_err("%s: failed to read register 0x64: %d\n",
			   __func__, ret);
		return ret;
	}
	if (!(reg & 0x80)) {
		aw_err("%s:unqualified chip!\n", __func__);
		return -ERANGE;
	}

	return 0;
}


int aw86907_haptics_upload_effect(struct input_dev *dev,
				struct ff_effect *effect,
				struct ff_effect *old)
{
	struct aw86907 *aw86907 = input_get_drvdata(dev);
	struct qti_hap_play_info *play = &aw86907->play;
	s16 data[CUSTOM_DATA_LEN];
	ktime_t rem;
	s64 time_us;
	int ret;

	/*for osc calibration*/
	if (aw86907->osc_cali_run != 0)
		return 0;

	if (hrtimer_active(&aw86907->timer)) {
		rem = hrtimer_get_remaining(&aw86907->timer);
		time_us = ktime_to_us(rem);
		aw_info("waiting for playing clear sequence: %lld us\n",
			time_us);
		usleep_range(time_us, time_us + 100);
	}
	aw_dbg("%s: effect->type=0x%x,FF_CONSTANT=0x%x,FF_PERIODIC=0x%x\n",
		__func__, effect->type, FF_CONSTANT, FF_PERIODIC);
	aw86907->effect_type = effect->type;
	 mutex_lock(&aw86907->lock);
	 while (atomic_read(&aw86907->exit_in_rtp_loop)) {
		aw_info("%s  goint to waiting rtp  exit\n", __func__);
		mutex_unlock(&aw86907->lock);
		ret = wait_event_interruptible(aw86907->stop_wait_q,
				atomic_read(&aw86907->exit_in_rtp_loop) == 0);
		aw_info("%s wakeup\n", __func__);
		if (ret == -ERESTARTSYS) {
			mutex_unlock(&aw86907->lock);
			aw_err("%s wake up by signal return erro\n", __func__);
			return ret;
		}
		mutex_lock(&aw86907->lock);
	 }

	if (aw86907->effect_type == FF_CONSTANT) {
		aw_dbg("%s: effect_type is  FF_CONSTANT!\n", __func__);
		/*cont mode set duration */
		aw86907->duration = effect->replay.length;
		aw86907->activate_mode = AW86907_HAPTIC_ACTIVATE_RAM_LOOP_MODE;
		aw86907->effect_id = aw86907->info.effect_id_boundary;

	} else if (aw86907->effect_type == FF_PERIODIC) {
		if (aw86907->effects_count == 0) {
			mutex_unlock(&aw86907->lock);
			return -EINVAL;
		}

		aw_dbg("%s: effect_type is  FF_PERIODIC!\n", __func__);
		if (copy_from_user(data, effect->u.periodic.custom_data,
				   sizeof(s16) * CUSTOM_DATA_LEN)) {
			mutex_unlock(&aw86907->lock);
			return -EFAULT;
		}

		aw86907->effect_id = data[0];
		aw_dbg("%s: aw86907->effect_id =%d\n",
			 __func__, aw86907->effect_id);
		play->vmax_mv = effect->u.periodic.magnitude; /*vmax level*/

		if (aw86907->effect_id < 0 ||
			aw86907->effect_id > aw86907->info.effect_max) {
			mutex_unlock(&aw86907->lock);
			return 0;
		}
		aw86907->is_custom_wave = 0;

		if (aw86907->effect_id < aw86907->info.effect_id_boundary) {
			aw86907->activate_mode = AW86907_HAPTIC_ACTIVATE_RAM_MODE;
			aw_dbg("%s: aw86907->effect_id=%d , aw86907->activate_mode = %d\n",
				__func__, aw86907->effect_id,
				aw86907->activate_mode);
			/*second data*/
			data[1] = aw86907->predefined[aw86907->effect_id].play_rate_us/1000000;
			/*millisecond data*/
			data[2] = aw86907->predefined[aw86907->effect_id].play_rate_us/1000;
		}
		if (aw86907->effect_id >= aw86907->info.effect_id_boundary) {
			aw86907->activate_mode = AW86907_HAPTIC_ACTIVATE_RTP_MODE;
			aw_dbg("%s: aw86907->effect_id=%d , aw86907->activate_mode = %d\n",
				__func__, aw86907->effect_id,
				aw86907->activate_mode);
			/*second data*/
			data[1] = aw86907->info.rtp_time[aw86907->effect_id]/1000;
			/*millisecond data*/
			data[2] = aw86907->info.rtp_time[aw86907->effect_id];
		}
		if (aw86907->effect_id == CUSTOME_WAVE_ID) {
			aw86907->activate_mode = AW86907_HAPTIC_ACTIVATE_RTP_MODE;
			aw_dbg("%s: aw86907->effect_id=%d , aw86907->activate_mode = %d\n",
				__func__, aw86907->effect_id,
				aw86907->activate_mode);
			/*second data*/
			data[1] = aw86907->info.rtp_time[aw86907->effect_id]/1000;
			/*millisecond data*/
			data[2] = aw86907->info.rtp_time[aw86907->effect_id];
			aw86907->is_custom_wave = 1;
			rb_init();
		}


		if (copy_to_user(effect->u.periodic.custom_data, data,
			sizeof(s16) * CUSTOM_DATA_LEN)) {
			mutex_unlock(&aw86907->lock);
			return -EFAULT;
		}

	} else {
		aw_err("%s Unsupported effect type: %d\n", __func__,
		       effect->type);
	}
	mutex_unlock(&aw86907->lock);
	return 0;
}


int aw86907_haptics_playback(struct input_dev *dev, int effect_id,
				   int val)
{
	struct aw86907 *aw86907 = input_get_drvdata(dev);
	int rc = 0;

	aw_dbg("%s: effect_id=%d , activate_mode = %d val = %d\n",
		__func__, aw86907->effect_id, aw86907->activate_mode, val);
	/*for osc calibration*/
	if (aw86907->osc_cali_run != 0)
		return 0;

	if (val > 0)
		aw86907->state = 1;
	if (val <= 0)
		aw86907->state = 0;
	hrtimer_cancel(&aw86907->timer);

	if (aw86907->effect_type == FF_CONSTANT &&
		aw86907->activate_mode ==
			AW86907_HAPTIC_ACTIVATE_RAM_LOOP_MODE) {
		aw_dbg("%s: enter cont_mode\n", __func__);
		queue_work(aw86907->work_queue, &aw86907->vibrator_work);
	} else if (aw86907->effect_type == FF_PERIODIC &&
		aw86907->activate_mode == AW86907_HAPTIC_ACTIVATE_RAM_MODE) {
		aw_dbg("%s: enter  ram_mode\n", __func__);
		queue_work(aw86907->work_queue, &aw86907->vibrator_work);
	} else if ((aw86907->effect_type == FF_PERIODIC) &&
		aw86907->activate_mode == AW86907_HAPTIC_ACTIVATE_RTP_MODE) {
		aw_dbg("%s: enter  rtp_mode\n", __func__);
		queue_work(aw86907->work_queue, &aw86907->rtp_work);
		/*if we are in the play mode, force to exit*/
		if (val == 0) {
			atomic_set(&aw86907->exit_in_rtp_loop, 1);
			rb_force_exit();
			wake_up_interruptible(&aw86907->stop_wait_q);
		}
	} else {
		/*other mode */
	}

	return rc;
}

int aw86907_haptics_erase(struct input_dev *dev, int effect_id)
{
	struct aw86907 *aw86907 = input_get_drvdata(dev);
	int rc = 0;

	/*for osc calibration*/
	if (aw86907->osc_cali_run != 0)
		return 0;

	aw_dbg("%s: enter\n", __func__);
	aw86907->effect_type = 0;
	aw86907->is_custom_wave = 0;
	aw86907->duration = 0;
	return rc;
}

void aw86907_haptics_set_gain_work_routine(struct work_struct *work)
{
	unsigned char comp_level = 0;
	struct aw86907 *aw86907 =
	    container_of(work, struct aw86907, set_gain_work);

	if (aw86907->new_gain >= 0x7FFF)
		aw86907->level = 0x80;	/*128 */
	else if (aw86907->new_gain <= 0x3FFF)
		aw86907->level = 0x1E;	/*30 */
	else
		aw86907->level = (aw86907->new_gain - 16383) / 128;

	if (aw86907->level < 0x1E)
		aw86907->level = 0x1E;	/*30 */
	aw_info("%s: set_gain queue work, new_gain = %x level = %x\n",
		__func__, aw86907->new_gain, aw86907->level);

	if (aw86907->ram_vbat_comp == AW86907_HAPTIC_RAM_VBAT_COMP_ENABLE
		&& aw86907->vbat) {
		aw_dbg("%s: ref %d vbat %d ", __func__, AW_VBAT_REFER,
				aw86907->vbat);
		comp_level = aw86907->level * AW_VBAT_REFER / aw86907->vbat;
		if (comp_level > (128 * AW_VBAT_REFER / AW_VBAT_MIN)) {
			comp_level = 128 * AW_VBAT_REFER / AW_VBAT_MIN;
			aw_dbg("%s: comp level limit is %d ",
				 __func__, comp_level);
		}
		aw_info("%s: enable vbat comp, level = %x comp level = %x",
			__func__, aw86907->level, comp_level);
		aw86907_i2c_write(aw86907, AW86907_REG_PLAYCFG2, comp_level);
	} else {
		aw_dbg("%s: disable compsensation, vbat=%d, vbat_min=%d, vbat_ref=%d",
			 __func__, aw86907->vbat, AW_VBAT_MIN, AW_VBAT_REFER);
		aw86907_i2c_write(aw86907, AW86907_REG_PLAYCFG2,
				  aw86907->level);
	}
}

void aw86907_haptics_set_gain(struct input_dev *dev, u16 gain)
{
	struct aw86907 *aw86907 = input_get_drvdata(dev);

	aw_dbg("%s enter\n", __func__);
	aw86907->new_gain = gain;
	queue_work(aw86907->work_queue, &aw86907->set_gain_work);
}