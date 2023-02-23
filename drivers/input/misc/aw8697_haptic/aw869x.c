/*
 * aw869x.c
 *
 *
 *
 * Copyright (c) 2021 AWINIC Technology CO., LTD
 *
 *  Author: <chelvming@awinic.com.cn>
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
#include <linux/platform_device.h>
#include <asm/uaccess.h>
#include <linux/syscalls.h>
#include <linux/power_supply.h>
#include <linux/pm_qos.h>
#include <linux/vmalloc.h>
#include "aw_haptic.h"
#include  "ringbuffer.h"
#include "aw_config.h"
#include "aw869x.h"

/******************************************************
 *
 * variable
 *
 ******************************************************/
static struct aw869x_container *aw869x_rtp;
static struct aw869x *g_aw869x;

static const unsigned char aw869x_reg_access[AW869X_REG_MAX] = {
	[AW869X_REG_ID] = REG_RD_ACCESS,
	[AW869X_REG_SYSST] = REG_RD_ACCESS,
	[AW869X_REG_SYSINT] = REG_RD_ACCESS,
	[AW869X_REG_SYSINTM] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_SYSCTRL] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_GO] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_RTP_DATA] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_WAVSEQ1] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_WAVSEQ2] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_WAVSEQ3] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_WAVSEQ4] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_WAVSEQ5] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_WAVSEQ6] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_WAVSEQ7] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_WAVSEQ8] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_WAVLOOP1] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_WAVLOOP2] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_WAVLOOP3] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_WAVLOOP4] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_MAIN_LOOP] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_TRG1_WAV_P] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_TRG2_WAV_P] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_TRG3_WAV_P] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_TRG1_WAV_N] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_TRG2_WAV_N] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_TRG3_WAV_N] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_TRG_PRIO] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_TRG_CFG1] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_TRG_CFG2] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_DBGCTRL] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_BASE_ADDRH] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_BASE_ADDRL] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_FIFO_AEH] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_FIFO_AEL] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_FIFO_AFH] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_FIFO_AFL] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_WAKE_DLY] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_START_DLY] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_END_DLY_H] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_END_DLY_L] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_DATCTRL] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_PWMDEL] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_PWMPRC] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_PWMDBG] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_LDOCTRL] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_DBGSTAT] = REG_RD_ACCESS,
	[AW869X_REG_BSTDBG1] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_BSTDBG2] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_BSTDBG3] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_BSTCFG] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_ANADBG] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_ANACTRL] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_CPDBG] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_GLBDBG] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_DATDBG] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_BSTDBG4] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_BSTDBG5] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_BSTDBG6] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_HDRVDBG] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_PRLVL] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_PRTIME] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_RAMADDRH] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_RAMADDRL] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_RAMDATA] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_GLB_STATE] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_BST_AUTO] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_CONT_CTRL] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_F_PRE_H] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_F_PRE_L] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_TD_H] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_TD_L] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_TSET] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_TRIM_LRA] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_R_SPARE] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_D2SCFG] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_DETCTRL] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_RLDET] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_OSDET] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_VBATDET] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_TESTDET] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_DETLO] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_BEMFDBG] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_ADCTEST] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_BEMFTEST] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_F_LRA_F0_H] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_F_LRA_F0_L] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_F_LRA_CONT_H] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_F_LRA_CONT_L] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_WAIT_VOL_MP] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_WAIT_VOL_MN] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_BEMF_VOL_H] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_BEMF_VOL_L] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_ZC_THRSH_H] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_ZC_THRSH_L] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_BEMF_VTHH_H] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_BEMF_VTHH_L] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_BEMF_VTHL_H] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_BEMF_VTHL_L] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_BEMF_NUM] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_DRV_TIME] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_TIME_NZC] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_DRV_LVL] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_DRV_LVL_OV] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_NUM_F0_1] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_NUM_F0_2] = REG_RD_ACCESS | REG_WR_ACCESS,
	[AW869X_REG_NUM_F0_3] = REG_RD_ACCESS | REG_WR_ACCESS,
};
/******************************************************
 *
 * functions
 *
 ******************************************************/
static void aw869x_interrupt_clear(struct aw869x *aw869x);
static int aw869x_haptic_trig_enable_config(struct aw869x *aw869x);
static int aw869x_haptic_get_vbat(struct aw869x *aw869x);
 /******************************************************
 *
 * aw869x i2c write/read
 *
 ******************************************************/
static int aw869x_i2c_write(struct aw869x *aw869x,
			    unsigned char reg_addr, unsigned char reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < AW_I2C_RETRIES) {
		ret =
		    i2c_smbus_write_byte_data(aw869x->i2c, reg_addr, reg_data);
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

static int aw869x_i2c_read(struct aw869x *aw869x,
			   unsigned char reg_addr, unsigned char *reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < AW_I2C_RETRIES) {
		ret = i2c_smbus_read_byte_data(aw869x->i2c, reg_addr);
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

static int aw869x_i2c_write_bits(struct aw869x *aw869x,
				 unsigned char reg_addr, unsigned int mask,
				 unsigned char reg_data)
{
	unsigned char reg_val = 0;

	aw869x_i2c_read(aw869x, reg_addr, &reg_val);
	reg_val &= mask;
	reg_val |= (reg_data & (~mask));
	aw869x_i2c_write(aw869x, reg_addr, reg_val);

	return 0;
}

static int aw869x_i2c_writes(struct aw869x *aw869x,
			     unsigned char reg_addr, unsigned char *buf,
			     unsigned int len)
{
	int ret = -1;
	unsigned char *data;

	data = kmalloc(len + 1, GFP_KERNEL);

	data[0] = reg_addr;
	memcpy(&data[1], buf, len);

	ret = i2c_master_send(aw869x->i2c, data, len + 1);
	if (ret < 0)
		aw_err("%s: i2c master send error\n", __func__);

	kfree(data);

	return ret;
}

int aw869x_i2c_reads(struct aw869x *aw869x, unsigned char reg_addr,
		      unsigned char *buf, unsigned int len)
{
	int ret;
	struct i2c_msg msg[] = {
		[0] = {
			.addr = aw869x->i2c->addr,
			.flags = 0,
			.len = sizeof(uint8_t),
			.buf = &reg_addr,
			},
		[1] = {
			.addr = aw869x->i2c->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = buf,
			},
	};

	ret = i2c_transfer(aw869x->i2c->adapter, msg, ARRAY_SIZE(msg));
	if (ret < 0) {
		aw_err("%s: transfer failed.", __func__);
		return ret;
	} else if (ret != 2) {
		aw_err("%s: transfer failed(size error).", __func__);
		return -ENXIO;
	}

	return ret;
}


/*****************************************************
 *
 * ram update
 *
 *****************************************************/
static void aw869x_rtp_loaded(const struct firmware *cont, void *context)
{
	struct aw869x *aw869x = context;

	aw_info("%s enter\n", __func__);

	if (!cont) {
		aw_err("%s: failed to read %s\n", __func__,
		       awinic_rtp_name[aw869x->rtp_file_num]);
		release_firmware(cont);
		return;
	}

	aw_info("%s: loaded %s - size: %zu\n", __func__,
		awinic_rtp_name[aw869x->rtp_file_num], cont ? cont->size : 0);

	/* aw869x rtp update */
	aw869x_rtp = vmalloc(cont->size + sizeof(int));
	if (!aw869x_rtp) {
		release_firmware(cont);
		aw_err("%s: Error allocating memory\n", __func__);
		return;
	}
	aw869x_rtp->len = cont->size;
	aw_info("%s: rtp size = %d\n", __func__, aw869x_rtp->len);
	memcpy(aw869x_rtp->data, cont->data, cont->size);
	release_firmware(cont);

	aw869x->rtp_init = 1;
	aw_info("%s: rtp update complete\n", __func__);
}

static int aw869x_rtp_update(struct aw869x *aw869x)
{
	aw_info("%s enter\n", __func__);

	return request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
				       awinic_rtp_name[aw869x->rtp_file_num],
				       aw869x->dev, GFP_KERNEL, aw869x,
				       aw869x_rtp_loaded);
}

#ifdef AW_CHECK_RAM_DATA
static int aw869x_check_ram_data(struct aw869x *aw869x,
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

static void aw869x_container_update(struct aw869x *aw869x,
				    struct aw869x_container *aw869x_cont)
{
	int i = 0;
	int len = 0;
	unsigned int shift = 0;

#ifdef AW_CHECK_RAM_DATA
	int ret = 0;
	unsigned char ram_data[AW_RAMDATA_RD_BUFFER_SIZE] = {0};
#endif

	aw_info("%s enter\n", __func__);

	mutex_lock(&aw869x->lock);

	aw869x->ram.baseaddr_shift = 2;
	aw869x->ram.ram_shift = 4;

	/* RAMINIT Enable */
	aw869x_i2c_write_bits(aw869x, AW869X_REG_SYSCTRL,
			      AW869X_BIT_SYSCTRL_RAMINIT_MASK,
			      AW869X_BIT_SYSCTRL_RAMINIT_EN);

	/* base addr */
	shift = aw869x->ram.baseaddr_shift;
	aw869x->ram.base_addr =
	    (unsigned int)((aw869x_cont->data[0 + shift] << 8) |
			   (aw869x_cont->data[1 + shift]));
	aw_info("%s: base_addr=0x%4x\n", __func__, aw869x->ram.base_addr);

	aw869x_i2c_write(aw869x, AW869X_REG_BASE_ADDRH,
			 aw869x_cont->data[0 + shift]);
	aw869x_i2c_write(aw869x, AW869X_REG_BASE_ADDRL,
			 aw869x_cont->data[1 + shift]);

	aw869x_i2c_write(aw869x, AW869X_REG_FIFO_AEH,
			 (unsigned char)((aw869x->ram.base_addr >> 2) >> 8));
	aw869x_i2c_write(aw869x, AW869X_REG_FIFO_AEL,
			 (unsigned char)((aw869x->ram.base_addr >> 2) &
					 0x00FF));
	aw869x_i2c_write(aw869x, AW869X_REG_FIFO_AFH,
			 (unsigned
			  char)((aw869x->ram.base_addr -
				 (aw869x->ram.base_addr >> 2)) >> 8));
	aw869x_i2c_write(aw869x, AW869X_REG_FIFO_AFL,
			 (unsigned
			  char)((aw869x->ram.base_addr -
				 (aw869x->ram.base_addr >> 2)) & 0x00FF));

	/* ram */
	shift = aw869x->ram.baseaddr_shift;
	aw869x_i2c_write(aw869x, AW869X_REG_RAMADDRH,
			 aw869x_cont->data[0 + shift]);
	aw869x_i2c_write(aw869x, AW869X_REG_RAMADDRL,
			 aw869x_cont->data[1 + shift]);
	i = aw869x->ram.ram_shift;
	while(i < aw869x_cont->len) {
		if((aw869x_cont->len - i) < AW_RAMDATA_WR_BUFFER_SIZE)
			len = aw869x_cont->len - i;
		else
			len = AW_RAMDATA_WR_BUFFER_SIZE;
		aw869x_i2c_writes(aw869x, AW869X_REG_RAMDATA,
				   &aw869x_cont->data[i], len);
		i += len;
	}

#ifdef AW_CHECK_RAM_DATA
	shift = aw869x->ram.baseaddr_shift;
	aw869x_i2c_write(aw869x, AW869X_REG_RAMADDRH,
			 aw869x_cont->data[0 + shift]);
	aw869x_i2c_write(aw869x, AW869X_REG_RAMADDRL,
			 aw869x_cont->data[1 + shift]);
	i = aw869x->ram.ram_shift;
	while (i < aw869x_cont->len) {
		if ((aw869x_cont->len - i) < AW_RAMDATA_RD_BUFFER_SIZE)
			len = aw869x_cont->len - i;
		else
			len = AW_RAMDATA_RD_BUFFER_SIZE;

		aw869x_i2c_reads(aw869x, AW869X_REG_RAMDATA, ram_data, len);
		ret = aw869x_check_ram_data(aw869x, &aw869x_cont->data[i],
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
	aw869x_i2c_write_bits(aw869x, AW869X_REG_SYSCTRL,
			      AW869X_BIT_SYSCTRL_RAMINIT_MASK,
			      AW869X_BIT_SYSCTRL_RAMINIT_OFF);

	mutex_unlock(&aw869x->lock);

	aw_info("%s exit\n", __func__);
}

static void aw869x_ram_loaded(const struct firmware *cont, void *context)
{
	struct aw869x *aw869x = context;
	struct aw869x_container *aw869x_fw;
	int i = 0;
	unsigned short check_sum = 0;

	aw_info("%s enter\n", __func__);

	if (!cont) {
		aw_err("%s: failed to read %s\n", __func__, awinic_ram_name);
		release_firmware(cont);
		return;
	}

	aw_info("%s: loaded %s - size: %zu\n", __func__, awinic_ram_name,
		cont ? cont->size : 0);
	/*
	 * for (i=0; i<cont->size; i++) {
	 *	aw_info("%s: addr:0x%04x, data:0x%02x\n", __func__,
	 *	i, *(cont->data+i));
	 * }
	 */

	/* check sum */
	for (i = 2; i < cont->size; i++)
		check_sum += cont->data[i];

	if (check_sum ==
	    (unsigned short)((cont->data[0] << 8) | (cont->data[1]))) {
		aw_info("%s: check sum pass : 0x%04x\n", __func__, check_sum);
		aw869x->ram.check_sum = check_sum;
	} else {
		aw_err("%s: check sum err: check_sum=0x%04x\n", __func__,
		       check_sum);
		return;
	}

	/* aw869x ram update */
	aw869x_fw = kzalloc(cont->size + sizeof(int), GFP_KERNEL);
	if (!aw869x_fw) {
		release_firmware(cont);
		aw_err("%s: Error allocating memory\n", __func__);
		return;
	}
	aw869x_fw->len = cont->size;
	memcpy(aw869x_fw->data, cont->data, cont->size);
	release_firmware(cont);

	aw869x_container_update(aw869x, aw869x_fw);

	aw869x->ram.len = aw869x_fw->len;

	kfree(aw869x_fw);

	aw869x->ram_init = 1;
	aw_info("%s: fw update complete\n", __func__);

	aw869x_haptic_trig_enable_config(aw869x);

	aw869x_rtp_update(aw869x);
}

static int aw869x_ram_update(struct aw869x *aw869x)
{
	aw869x->ram_init = 0;
	aw869x->rtp_init = 0;
	aw_info("%s enter\n", __func__);

	return request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
				       awinic_ram_name, aw869x->dev, GFP_KERNEL,
				       aw869x, aw869x_ram_loaded);
}

#ifdef AW_RAM_UPDATE_DELAY
static void aw869x_ram_work_routine(struct work_struct *work)
{
	struct aw869x *aw869x =
	    container_of(work, struct aw869x, ram_work.work);

	aw_info("%s enter\n", __func__);

	aw869x_ram_update(aw869x);

}
#endif
/*****************************************************
 *
 * haptic control
 *
 *****************************************************/
static int aw869x_haptic_active(struct aw869x *aw869x)
{
	aw_dbg("%s enter\n", __func__);

	aw869x_i2c_write_bits(aw869x, AW869X_REG_SYSCTRL,
			      AW869X_BIT_SYSCTRL_WORK_MODE_MASK,
			      AW869X_BIT_SYSCTRL_ACTIVE);
	aw869x_interrupt_clear(aw869x);
	aw869x_i2c_write_bits(aw869x, AW869X_REG_SYSINTM,
			      AW869X_BIT_SYSINTM_UVLO_MASK,
			      AW869X_BIT_SYSINTM_UVLO_EN);
	return 0;
}

static int aw869x_haptic_play_mode(struct aw869x *aw869x,
				   unsigned char play_mode)
{
	aw_dbg("%s enter\n", __func__);

	switch (play_mode) {
	case AW869X_HAPTIC_STANDBY_MODE:
		aw869x->play_mode = AW869X_HAPTIC_STANDBY_MODE;
		aw869x_i2c_write_bits(aw869x, AW869X_REG_SYSINTM,
				      AW869X_BIT_SYSINTM_UVLO_MASK,
				      AW869X_BIT_SYSINTM_UVLO_OFF);
		aw869x_i2c_write_bits(aw869x, AW869X_REG_SYSCTRL,
				      AW869X_BIT_SYSCTRL_WORK_MODE_MASK,
				      AW869X_BIT_SYSCTRL_STANDBY);
		break;
	case AW869X_HAPTIC_RAM_MODE:
		aw869x->play_mode = AW869X_HAPTIC_RAM_MODE;
		aw869x_i2c_write_bits(aw869x, AW869X_REG_SYSCTRL,
				      AW869X_BIT_SYSCTRL_PLAY_MODE_MASK,
				      AW869X_BIT_SYSCTRL_PLAY_MODE_RAM);
		aw869x_haptic_active(aw869x);
		aw869x_i2c_write_bits(aw869x, AW869X_REG_SYSCTRL,
				      AW869X_BIT_SYSCTRL_BST_MODE_MASK,
				      AW869X_BIT_SYSCTRL_BST_MODE_BOOST);
		break;
	case AW869X_HAPTIC_RAM_LOOP_MODE:
		aw869x->play_mode = AW869X_HAPTIC_RAM_LOOP_MODE;
		aw869x_i2c_write_bits(aw869x, AW869X_REG_SYSCTRL,
				      AW869X_BIT_SYSCTRL_PLAY_MODE_MASK,
				      AW869X_BIT_SYSCTRL_PLAY_MODE_RAM);
		aw869x_haptic_active(aw869x);
		aw869x_i2c_write_bits(aw869x, AW869X_REG_SYSCTRL,
				      AW869X_BIT_SYSCTRL_BST_MODE_MASK,
				      AW869X_BIT_SYSCTRL_BST_MODE_BYPASS);
		break;
	case AW869X_HAPTIC_RTP_MODE:
		aw869x->play_mode = AW869X_HAPTIC_RTP_MODE;
		aw869x_i2c_write_bits(aw869x, AW869X_REG_SYSCTRL,
				      AW869X_BIT_SYSCTRL_PLAY_MODE_MASK,
				      AW869X_BIT_SYSCTRL_PLAY_MODE_RTP);
		aw869x_haptic_active(aw869x);
		aw869x_i2c_write_bits(aw869x, AW869X_REG_SYSCTRL,
				      AW869X_BIT_SYSCTRL_BST_MODE_MASK,
				      AW869X_BIT_SYSCTRL_BST_MODE_BOOST);
		break;
	case AW869X_HAPTIC_TRIG_MODE:
		aw869x->play_mode = AW869X_HAPTIC_TRIG_MODE;
		aw869x_i2c_write_bits(aw869x, AW869X_REG_SYSCTRL,
				      AW869X_BIT_SYSCTRL_PLAY_MODE_MASK,
				      AW869X_BIT_SYSCTRL_PLAY_MODE_RAM);
		aw869x_haptic_active(aw869x);
		break;
	case AW869X_HAPTIC_CONT_MODE:
		aw869x->play_mode = AW869X_HAPTIC_CONT_MODE;
		aw869x_i2c_write_bits(aw869x, AW869X_REG_SYSCTRL,
				      AW869X_BIT_SYSCTRL_PLAY_MODE_MASK,
				      AW869X_BIT_SYSCTRL_PLAY_MODE_CONT);
		aw869x_i2c_write_bits(aw869x, AW869X_REG_SYSCTRL,
				      AW869X_BIT_SYSCTRL_BST_MODE_MASK,
				      AW869X_BIT_SYSCTRL_BST_MODE_BYPASS);
		aw869x_haptic_active(aw869x);
		break;
	default:
		aw_err("%s: play mode %d err", __func__, play_mode);
		break;
	}
	return 0;
}

static int aw869x_haptic_play_go(struct aw869x *aw869x, bool flag)
{
	if (flag == true) {
		aw869x_i2c_write_bits(aw869x, AW869X_REG_GO,
				      AW869X_BIT_GO_MASK, AW869X_BIT_GO_ENABLE);
	} else {
		aw869x_i2c_write_bits(aw869x, AW869X_REG_GO,
				      AW869X_BIT_GO_MASK,
				      AW869X_BIT_GO_DISABLE);
	}
	return 0;
}

static int aw869x_haptic_stop_delay(struct aw869x *aw869x)
{
	unsigned char reg_val = 0;
	unsigned int cnt = 100;

	while (cnt--) {
		aw869x_i2c_read(aw869x, AW869X_REG_GLB_STATE, &reg_val);
		if ((reg_val & 0x0f) == 0x00)
			return 0;

		usleep_range(2000, 2500);
		aw_dbg("%s wait for standby, reg glb_state=0x%02x\n",
			 __func__, reg_val);
	}
	aw_err("%s do not enter standby automatically\n", __func__);

	return 0;
}

static int aw869x_haptic_stop(struct aw869x *aw869x)
{
	aw_dbg("%s enter\n", __func__);

	aw869x_haptic_play_go(aw869x, false);
	aw869x_haptic_stop_delay(aw869x);
	aw869x_haptic_play_mode(aw869x, AW869X_HAPTIC_STANDBY_MODE);

	return 0;
}

static int aw869x_haptic_start(struct aw869x *aw869x)
{
	aw_dbg("%s enter\n", __func__);

	aw869x_haptic_play_go(aw869x, true);

	return 0;
}

static int aw869x_haptic_set_wav_seq(struct aw869x *aw869x,
				     unsigned char wav, unsigned char seq)
{
	aw869x_i2c_write(aw869x, AW869X_REG_WAVSEQ1 + wav, seq);
	return 0;
}

static int aw869x_haptic_set_wav_loop(struct aw869x *aw869x,
				      unsigned char wav, unsigned char loop)
{
	unsigned char tmp = 0;

	if (wav % 2) {
		tmp = loop << 0;
		aw869x_i2c_write_bits(aw869x, AW869X_REG_WAVLOOP1 + (wav / 2),
				      AW869X_BIT_WAVLOOP_SEQNP1_MASK, tmp);
	} else {
		tmp = loop << 4;
		aw869x_i2c_write_bits(aw869x, AW869X_REG_WAVLOOP1 + (wav / 2),
				      AW869X_BIT_WAVLOOP_SEQN_MASK, tmp);
	}

	return 0;
}

static int aw869x_haptic_set_repeat_wav_seq(struct aw869x *aw869x,
					    unsigned char seq)
{
	aw869x_haptic_set_wav_seq(aw869x, 0x00, seq);
	aw869x_haptic_set_wav_loop(aw869x, 0x00,
				   AW869X_BIT_WAVLOOP_INIFINITELY);

	return 0;
}

static int aw869x_haptic_set_bst_vol(struct aw869x *aw869x,
				     unsigned char bst_vol)
{
	if (bst_vol & 0xe0)
		bst_vol = 0x1f;

	/* aw_info("%s %d --\n", __func__, __LINE__); */
	aw869x_i2c_write_bits(aw869x, AW869X_REG_BSTDBG4,
			      AW869X_BIT_BSTDBG4_BSTVOL_MASK, (bst_vol << 1));
	return 0;
}

static int aw869x_haptic_set_bst_peak_cur(struct aw869x *aw869x,
					  unsigned char peak_cur)
{
	peak_cur &= AW869X_BSTCFG_PEAKCUR_LIMIT;
	aw_info("%s  %d enter\n", __func__, __LINE__);
	aw869x_i2c_write_bits(aw869x, AW869X_REG_BSTCFG,
			      AW869X_BIT_BSTCFG_PEAKCUR_MASK, peak_cur);
	return 0;
}

static int aw869x_haptic_set_gain(struct aw869x *aw869x, unsigned char gain)
{
	unsigned char comp_gain = 0;

	if (aw869x->ram_vbat_comp == AW869X_HAPTIC_RAM_VBAT_COMP_ENABLE) {
		aw869x_haptic_get_vbat(aw869x);
		aw_dbg("%s: ref %d vbat %d ", __func__, AW_VBAT_REFER,
				aw869x->vbat);
		comp_gain = gain * AW_VBAT_REFER / aw869x->vbat;
		if (comp_gain > (128 * AW_VBAT_REFER / AW_VBAT_MIN)) {
			comp_gain = 128 * AW_VBAT_REFER / AW_VBAT_MIN;
			aw_dbg("%s: comp gain limit is %d ", __func__,
				 comp_gain);
		}
		aw_info("%s: enable vbat comp, level = %x comp level = %x",
			__func__, gain, comp_gain);
		aw869x_i2c_write(aw869x, AW869X_REG_DATDBG, comp_gain);
	} else {
		aw_dbg("%s: disable compsensation, vbat=%d, vbat_min=%d, vbat_ref=%d",
			__func__, aw869x->vbat,
			AW_VBAT_MIN, AW_VBAT_REFER);
		aw869x_i2c_write(aw869x, AW869X_REG_DATDBG, gain);
	}
	return 0;
}

static int aw869x_haptic_set_pwm(struct aw869x *aw869x, unsigned char mode)
{
	switch (mode) {
	case AW869X_PWM_48K:
		aw869x_i2c_write_bits(aw869x, AW869X_REG_PWMDBG,
				      AW869X_BIT_PWMDBG_PWM_MODE_MASK,
				      AW869X_BIT_PWMDBG_PWM_48K);
		break;
	case AW869X_PWM_24K:
		aw869x_i2c_write_bits(aw869x, AW869X_REG_PWMDBG,
				      AW869X_BIT_PWMDBG_PWM_MODE_MASK,
				      AW869X_BIT_PWMDBG_PWM_24K);
		break;
	case AW869X_PWM_12K:
		aw869x_i2c_write_bits(aw869x, AW869X_REG_PWMDBG,
				      AW869X_BIT_PWMDBG_PWM_MODE_MASK,
				      AW869X_BIT_PWMDBG_PWM_12K);
		break;
	default:
		break;
	}
	return 0;
}

static int aw869x_haptic_play_repeat_seq(struct aw869x *aw869x,
					 unsigned char flag)
{
	aw_dbg("%s enter\n", __func__);

	if (flag) {
		aw869x_haptic_play_mode(aw869x, AW869X_HAPTIC_RAM_LOOP_MODE);
		aw869x_haptic_start(aw869x);
	}

	return 0;
}

/*****************************************************
 *
 * motor protect
 *
 *****************************************************/
static int aw869x_haptic_swicth_motorprotect_config(struct aw869x *aw869x,
						    unsigned char addr,
						    unsigned char val)
{
	aw_info("%s enter\n", __func__);

	if (addr == 1) {
		aw869x_i2c_write_bits(aw869x, AW869X_REG_DETCTRL,
				      AW869X_BIT_DETCTRL_PROTECT_MASK,
				      AW869X_BIT_DETCTRL_PROTECT_SHUTDOWN);
		aw869x_i2c_write_bits(aw869x, AW869X_REG_PWMPRC,
				      AW869X_BIT_PWMPRC_PRC_MASK,
				      AW869X_BIT_PWMPRC_PRC_ENABLE);
		aw869x_i2c_write_bits(aw869x, AW869X_REG_PRLVL,
				      AW869X_BIT_PRLVL_PR_MASK,
				      AW869X_BIT_PRLVL_PR_ENABLE);
	} else if (addr == 0) {
		aw869x_i2c_write_bits(aw869x, AW869X_REG_DETCTRL,
				      AW869X_BIT_DETCTRL_PROTECT_MASK,
				      AW869X_BIT_DETCTRL_PROTECT_NO_ACTION);
		aw869x_i2c_write_bits(aw869x, AW869X_REG_PWMPRC,
				      AW869X_BIT_PWMPRC_PRC_MASK,
				      AW869X_BIT_PWMPRC_PRC_DISABLE);
		aw869x_i2c_write_bits(aw869x, AW869X_REG_PRLVL,
				      AW869X_BIT_PRLVL_PR_MASK,
				      AW869X_BIT_PRLVL_PR_DISABLE);
	} else if (addr == 0x2d) {
		aw869x_i2c_write_bits(aw869x, AW869X_REG_PWMPRC,
				      AW869X_BIT_PWMPRC_PRCTIME_MASK, val);
	} else if (addr == 0x3e) {
		aw869x_i2c_write_bits(aw869x, AW869X_REG_PRLVL,
				      AW869X_BIT_PRLVL_PRLVL_MASK, val);
	} else if (addr == 0x3f) {
		aw869x_i2c_write_bits(aw869x, AW869X_REG_PRTIME,
				      AW869X_BIT_PRTIME_PRTIME_MASK, val);
	} else {
		/*nothing to do; */
	}
	return 0;
}

/*****************************************************
 *
 * offset calibration
 *
 *****************************************************/
static int aw869x_haptic_offset_calibration(struct aw869x *aw869x)
{
	unsigned int cont = 2000;
	unsigned char reg_val = 0;

	aw_info("%s enter\n", __func__);

	aw869x_i2c_write_bits(aw869x, AW869X_REG_SYSCTRL,
			      AW869X_BIT_SYSCTRL_RAMINIT_MASK,
			      AW869X_BIT_SYSCTRL_RAMINIT_EN);
	aw869x_i2c_write_bits(aw869x, AW869X_REG_DETCTRL,
			      AW869X_BIT_DETCTRL_DIAG_GO_MASK,
			      AW869X_BIT_DETCTRL_DIAG_GO_ENABLE);
	while (1) {
		aw869x_i2c_read(aw869x, AW869X_REG_DETCTRL, &reg_val);
		if ((reg_val & 0x01) == 0 || cont == 0)
			break;
		cont--;
	}
	aw869x_i2c_write_bits(aw869x, AW869X_REG_SYSCTRL,
			      AW869X_BIT_SYSCTRL_RAMINIT_MASK,
			      AW869X_BIT_SYSCTRL_RAMINIT_OFF);

	return 0;
}

/*****************************************************
 *
 * trig config
 *
 *****************************************************/

static int aw869x_haptic_trig_param_init(struct aw869x *aw869x)
{
	aw_info("%s enter\n", __func__);

	aw869x->trig[0].enable = aw869x->info.trig_config[0][0];
	aw869x->trig[0].default_level = aw869x->info.trig_config[0][1];
	aw869x->trig[0].dual_edge = aw869x->info.trig_config[0][2];
	aw869x->trig[0].frist_seq = aw869x->info.trig_config[0][3];
	aw869x->trig[0].second_seq = aw869x->info.trig_config[0][4];

	aw869x->trig[1].enable = aw869x->info.trig_config[1][0];
	aw869x->trig[1].default_level = aw869x->info.trig_config[1][1];
	aw869x->trig[1].dual_edge = aw869x->info.trig_config[1][2];
	aw869x->trig[1].frist_seq = aw869x->info.trig_config[1][3];
	aw869x->trig[1].second_seq = aw869x->info.trig_config[1][4];

	aw869x->trig[2].enable = aw869x->info.trig_config[2][0];
	aw869x->trig[2].default_level = aw869x->info.trig_config[2][1];
	aw869x->trig[2].dual_edge = aw869x->info.trig_config[2][2];
	aw869x->trig[2].frist_seq = aw869x->info.trig_config[2][3];
	aw869x->trig[2].second_seq = aw869x->info.trig_config[2][4];

	return 0;
}

static int aw869x_haptic_trig_param_config(struct aw869x *aw869x)
{
	aw_info("%s enter\n", __func__);

	if (aw869x->trig[0].default_level) {
		aw869x_i2c_write_bits(aw869x, AW869X_REG_TRG_CFG1,
				      AW869X_BIT_TRGCFG1_TRG1_POLAR_MASK,
				      AW869X_BIT_TRGCFG1_TRG1_POLAR_NEG);
	} else {
		aw869x_i2c_write_bits(aw869x, AW869X_REG_TRG_CFG1,
				      AW869X_BIT_TRGCFG1_TRG1_POLAR_MASK,
				      AW869X_BIT_TRGCFG1_TRG1_POLAR_POS);
	}
	if (aw869x->trig[1].default_level) {
		aw869x_i2c_write_bits(aw869x, AW869X_REG_TRG_CFG1,
				      AW869X_BIT_TRGCFG1_TRG2_POLAR_MASK,
				      AW869X_BIT_TRGCFG1_TRG2_POLAR_NEG);
	} else {
		aw869x_i2c_write_bits(aw869x, AW869X_REG_TRG_CFG1,
				      AW869X_BIT_TRGCFG1_TRG2_POLAR_MASK,
				      AW869X_BIT_TRGCFG1_TRG2_POLAR_POS);
	}
	if (aw869x->trig[2].default_level) {
		aw869x_i2c_write_bits(aw869x, AW869X_REG_TRG_CFG1,
				      AW869X_BIT_TRGCFG1_TRG3_POLAR_MASK,
				      AW869X_BIT_TRGCFG1_TRG3_POLAR_NEG);
	} else {
		aw869x_i2c_write_bits(aw869x, AW869X_REG_TRG_CFG1,
				      AW869X_BIT_TRGCFG1_TRG3_POLAR_MASK,
				      AW869X_BIT_TRGCFG1_TRG3_POLAR_POS);
	}

	if (aw869x->trig[0].dual_edge) {
		aw869x_i2c_write_bits(aw869x, AW869X_REG_TRG_CFG1,
				      AW869X_BIT_TRGCFG1_TRG1_EDGE_MASK,
				      AW869X_BIT_TRGCFG1_TRG1_EDGE_POS_NEG);
	} else {
		aw869x_i2c_write_bits(aw869x, AW869X_REG_TRG_CFG1,
				      AW869X_BIT_TRGCFG1_TRG1_EDGE_MASK,
				      AW869X_BIT_TRGCFG1_TRG1_EDGE_POS);
	}
	if (aw869x->trig[1].dual_edge) {
		aw869x_i2c_write_bits(aw869x, AW869X_REG_TRG_CFG1,
				      AW869X_BIT_TRGCFG1_TRG2_EDGE_MASK,
				      AW869X_BIT_TRGCFG1_TRG2_EDGE_POS_NEG);
	} else {
		aw869x_i2c_write_bits(aw869x, AW869X_REG_TRG_CFG1,
				      AW869X_BIT_TRGCFG1_TRG2_EDGE_MASK,
				      AW869X_BIT_TRGCFG1_TRG2_EDGE_POS);
	}
	if (aw869x->trig[2].dual_edge) {
		aw869x_i2c_write_bits(aw869x, AW869X_REG_TRG_CFG1,
				      AW869X_BIT_TRGCFG1_TRG3_EDGE_MASK,
				      AW869X_BIT_TRGCFG1_TRG3_EDGE_POS_NEG);
	} else {
		aw869x_i2c_write_bits(aw869x, AW869X_REG_TRG_CFG1,
				      AW869X_BIT_TRGCFG1_TRG3_EDGE_MASK,
				      AW869X_BIT_TRGCFG1_TRG3_EDGE_POS);
	}

	if (aw869x->trig[0].frist_seq) {
		aw869x_i2c_write(aw869x, AW869X_REG_TRG1_WAV_P,
				 aw869x->trig[0].frist_seq);
	}
	if (aw869x->trig[0].second_seq && aw869x->trig[0].dual_edge) {
		aw869x_i2c_write(aw869x, AW869X_REG_TRG1_WAV_N,
				 aw869x->trig[0].second_seq);
	}
	if (aw869x->trig[1].frist_seq) {
		aw869x_i2c_write(aw869x, AW869X_REG_TRG2_WAV_P,
				 aw869x->trig[1].frist_seq);
	}
	if (aw869x->trig[1].second_seq && aw869x->trig[1].dual_edge) {
		aw869x_i2c_write(aw869x, AW869X_REG_TRG2_WAV_N,
				 aw869x->trig[1].second_seq);
	}
	if (aw869x->trig[2].frist_seq) {
		aw869x_i2c_write(aw869x, AW869X_REG_TRG3_WAV_P,
				 aw869x->trig[2].frist_seq);
	}
	if (aw869x->trig[2].second_seq && aw869x->trig[2].dual_edge) {
		aw869x_i2c_write(aw869x, AW869X_REG_TRG3_WAV_N,
				 aw869x->trig[2].second_seq);
	}

	return 0;
}

static int aw869x_haptic_trig_enable_config(struct aw869x *aw869x)
{
	aw_info("%s enter\n", __func__);

	aw869x_i2c_write_bits(aw869x, AW869X_REG_TRG_CFG2,
			      AW869X_BIT_TRGCFG2_TRG1_ENABLE_MASK,
			      aw869x->trig[0].enable);
	aw869x_i2c_write_bits(aw869x, AW869X_REG_TRG_CFG2,
			      AW869X_BIT_TRGCFG2_TRG2_ENABLE_MASK,
			      aw869x->trig[1].enable << 1);
	aw869x_i2c_write_bits(aw869x, AW869X_REG_TRG_CFG2,
			      AW869X_BIT_TRGCFG2_TRG3_ENABLE_MASK,
			      aw869x->trig[2].enable << 2);
	return 0;
}

static int aw869x_haptic_auto_boost_config(struct aw869x *aw869x,
					   unsigned char flag)
{
	aw869x->auto_boost = flag;
	if (flag) {
		aw869x_i2c_write_bits(aw869x, AW869X_REG_BST_AUTO,
				      AW869X_BIT_BST_AUTO_BST_AUTOSW_MASK,
				      AW869X_BIT_BST_AUTO_BST_AUTOMATIC_BOOST);
	} else {
		aw869x_i2c_write_bits(aw869x, AW869X_REG_BST_AUTO,
				      AW869X_BIT_BST_AUTO_BST_AUTOSW_MASK,
				      AW869X_BIT_BST_AUTO_BST_MANUAL_BOOST);
	}
	return 0;
}

/*****************************************************
 *
 * vbat mode
 *
 *****************************************************/
static int aw869x_haptic_cont_vbat_mode(struct aw869x *aw869x,
					unsigned char flag)
{
	if (flag == AW869X_HAPTIC_CONT_VBAT_HW_COMP_MODE) {
		aw869x_i2c_write_bits(aw869x, AW869X_REG_ADCTEST,
				      AW869X_BIT_ADCTEST_VBAT_MODE_MASK,
				      AW869X_BIT_ADCTEST_VBAT_HW_COMP);
	} else {
		aw869x_i2c_write_bits(aw869x, AW869X_REG_ADCTEST,
				      AW869X_BIT_ADCTEST_VBAT_MODE_MASK,
				      AW869X_BIT_ADCTEST_VBAT_SW_COMP);
	}
	return 0;
}

static int aw869x_haptic_get_vbat(struct aw869x *aw869x)
{
	unsigned char reg_val = 0;
	unsigned char reg_val_sysctrl = 0;
	unsigned char reg_val_detctrl = 0;

	aw869x_haptic_stop(aw869x);
	aw869x_i2c_read(aw869x, AW869X_REG_SYSCTRL, &reg_val_sysctrl);
	aw869x_i2c_read(aw869x, AW869X_REG_DETCTRL, &reg_val_detctrl);
	/*step 1:EN_RAMINIT*/
	aw869x_i2c_write_bits(aw869x,
				AW869X_REG_SYSCTRL,
				AW869X_BIT_SYSCTRL_RAMINIT_MASK,
				AW869X_BIT_SYSCTRL_RAMINIT_EN);

	/*step 2 :launch offset cali */
	aw869x_i2c_write_bits(aw869x,
				AW869X_REG_DETCTRL,
				AW869X_BIT_DETCTRL_DIAG_GO_MASK,
				AW869X_BIT_DETCTRL_DIAG_GO_ENABLE);
	/*step 3 :delay */
	usleep_range(2000, 2500);

	/*step 4 :launch power supply testing */
	aw869x_i2c_write_bits(aw869x,
				AW869X_REG_DETCTRL,
				AW869X_BIT_DETCTRL_VBAT_GO_MASK,
				AW869X_BIT_DETCTRL_VABT_GO_ENABLE);
	usleep_range(2000, 2500);

	aw869x_i2c_read(aw869x, AW869X_REG_VBATDET, &reg_val);
	aw869x->vbat = 6100 * reg_val / 256;
	if (aw869x->vbat > AW_VBAT_MAX) {
		aw869x->vbat = AW_VBAT_MAX;
		aw_info("%s:vbat max limit = %dmV\n", __func__, aw869x->vbat);
	}
	if (aw869x->vbat < AW_VBAT_MIN) {
		aw869x->vbat = AW_VBAT_MIN;
		aw_info("%s:vbat min limit = %dmV\n", __func__, aw869x->vbat);
	}
	aw_info("%s:aw869x->vbat=%dmV\n", __func__, aw869x->vbat);
	/*step 5: return val*/
	aw869x_i2c_write(aw869x, AW869X_REG_SYSCTRL, reg_val_sysctrl);

	return 0;
}

static int aw869x_haptic_ram_vbat_comp(struct aw869x *aw869x, bool flag)
{
	aw_dbg("%s: enter\n", __func__);
	if (flag)
		aw869x->ram_vbat_comp = AW869X_HAPTIC_RAM_VBAT_COMP_ENABLE;
	else
		aw869x->ram_vbat_comp = AW869X_HAPTIC_RAM_VBAT_COMP_DISABLE;
	return 0;
}

/*****************************************************
 *
 * f0
 *
 *****************************************************/
static int aw869x_haptic_set_f0_preset(struct aw869x *aw869x)
{
	unsigned int f0_reg = 0;

	aw_info("%s enter\n", __func__);

	f0_reg = 1000000000 / (aw869x->info.f0_pre * aw869x->info.f0_coeff);
	aw869x_i2c_write(aw869x, AW869X_REG_F_PRE_H,
			 (unsigned char)((f0_reg >> 8) & 0xff));
	aw869x_i2c_write(aw869x, AW869X_REG_F_PRE_L,
			 (unsigned char)((f0_reg >> 0) & 0xff));

	return 0;
}

#ifndef USE_CONT_F0_CALI
static int aw869x_haptic_read_f0(struct aw869x *aw869x)
{
	int ret = 0;
	unsigned char reg_val = 0;
	unsigned int f0_reg = 0;
	unsigned long f0_tmp = 0;

	aw_info("%s enter\n", __func__);

	ret = aw869x_i2c_read(aw869x, AW869X_REG_F_LRA_F0_H, &reg_val);
	f0_reg = (reg_val << 8);
	ret = aw869x_i2c_read(aw869x, AW869X_REG_F_LRA_F0_L, &reg_val);
	f0_reg |= (reg_val << 0);
	if (!f0_reg) {
		aw_info("%s: not get f0_reg value is 0!\n", __func__);
		aw869x->f0_cali_status = false;
		return 0;
	}
	aw869x->f0_cali_status = true;
	f0_tmp = 1000000000 / (f0_reg * aw869x->info.f0_coeff);
	aw869x->f0 = (unsigned int)f0_tmp;
	aw_info("%s f0=%d\n", __func__, aw869x->f0);

	return 0;
}
#endif

#ifndef USE_CONT_F0_CALI
static int aw869x_haptic_read_cont_f0(struct aw869x *aw869x)
{
	int ret = 0;
	unsigned char reg_val = 0;
	unsigned int f0_reg = 0;
	unsigned long f0_tmp = 0;

	aw_dbg("%s enter\n", __func__);

	ret = aw869x_i2c_read(aw869x, AW869X_REG_F_LRA_CONT_H, &reg_val);
	f0_reg = (reg_val << 8);
	ret = aw869x_i2c_read(aw869x, AW869X_REG_F_LRA_CONT_L, &reg_val);
	f0_reg |= (reg_val << 0);
	if (!f0_reg) {
		aw_info("%s: not get f0_reg value is 0!\n", __func__);
		return 0;
	}
	f0_tmp = 1000000000 / (f0_reg * aw869x->info.f0_coeff);
	aw869x->cont_f0 = (unsigned int)f0_tmp;
	aw_info("%s f0=%d\n", __func__, aw869x->cont_f0);

	return 0;
}
#else
static int aw869x_haptic_read_cont_f0(struct aw869x *aw869x)
{
	int ret = 0;
	unsigned char reg_val = 0;
	unsigned int f0_reg = 0;
	unsigned long f0_tmp = 0;

	aw_dbg("%s enter\n", __func__);

	ret = aw869x_i2c_read(aw869x, AW869X_REG_F_LRA_CONT_H, &reg_val);
	f0_reg = (reg_val << 8);
	ret = aw869x_i2c_read(aw869x, AW869X_REG_F_LRA_CONT_L, &reg_val);
	f0_reg |= (reg_val << 0);
	if (!f0_reg) {
		aw_info("%s: not get f0_reg value is 0!\n", __func__);
		return 0;
	}
	f0_tmp = 1000000000 / (f0_reg * aw869x->info.f0_coeff);
	aw869x->cont_f0 = (unsigned int)f0_tmp;
	aw869x->cont_f0 -= 12;
	aw869x->f0 = aw869x->cont_f0;
	aw_info("%s f0=%d\n", __func__, aw869x->cont_f0);

	return 0;
}
#endif

#ifndef USE_CONT_F0_CALI
static int aw869x_haptic_read_beme(struct aw869x *aw869x)
{
	int ret = 0;
	unsigned char reg_val = 0;

	aw_info("%s  %d enter\n", __func__, __LINE__);
	ret = aw869x_i2c_read(aw869x, AW869X_REG_WAIT_VOL_MP, &reg_val);
	aw869x->max_pos_beme = (reg_val << 0);
	ret = aw869x_i2c_read(aw869x, AW869X_REG_WAIT_VOL_MN, &reg_val);
	aw869x->max_neg_beme = (reg_val << 0);

	aw_info("%s max_pos_beme=%d\n", __func__, aw869x->max_pos_beme);
	aw_info("%s max_neg_beme=%d\n", __func__, aw869x->max_neg_beme);

	return 0;
}
#else
static int aw869x_haptic_read_cont_bemf(struct aw869x *aw869x)
{
	int ret = 0;
	unsigned char reg_val = 0;
	unsigned int bemf = 0;

	ret = aw869x_i2c_read(aw869x, AW869X_REG_BEMF_VOL_H, &reg_val);
	bemf |= (reg_val<<8);
	ret = aw869x_i2c_read(aw869x, AW869X_REG_BEMF_VOL_L, &reg_val);
	bemf |= (reg_val<<0);

	aw_info("%s bemf=%d\n", __func__, bemf);

	return 0;
}
#endif

/*****************************************************
 *
 * rtp
 *
 *****************************************************/
static void aw869x_haptic_set_rtp_aei(struct aw869x *aw869x, bool flag)
{
	if (flag) {
		aw869x_i2c_write_bits(aw869x, AW869X_REG_SYSINTM,
				      AW869X_BIT_SYSINTM_FF_AE_MASK,
				      AW869X_BIT_SYSINTM_FF_AE_EN);
	} else {
		aw869x_i2c_write_bits(aw869x, AW869X_REG_SYSINTM,
				      AW869X_BIT_SYSINTM_FF_AE_MASK,
				      AW869X_BIT_SYSINTM_FF_AE_OFF);
	}
}

static unsigned char aw869x_haptic_rtp_get_fifo_afi(struct aw869x *aw869x)
{
	unsigned char ret = 0;
	unsigned char reg_val = 0;

	if (aw869x->osc_cali_flag == 1) {
		aw869x_i2c_read(aw869x, AW869X_REG_SYSST, &reg_val);
		reg_val &= AW869X_BIT_SYSST_FF_AFS;
		ret = reg_val >> 3;
	} else {
		aw869x_i2c_read(aw869x, AW869X_REG_SYSINT, &reg_val);
		reg_val &= AW869X_BIT_SYSINT_FF_AFI;
		ret = reg_val >> 3;
	}

	return ret;
}

/*****************************************************
 *
 * rtp
 *
 *****************************************************/
static int aw869x_haptic_rtp_init(struct aw869x *aw869x)
{
	unsigned int buf_len = 0;
	unsigned int period_size = aw869x->ram.base_addr >> 2;

	pm_qos_enable(true);
	aw869x->rtp_cnt = 0;
	disable_irq(gpio_to_irq(aw869x->irq_gpio));
	while ((!aw869x_haptic_rtp_get_fifo_afi(aw869x)) &&
	       (aw869x->play_mode == AW869X_HAPTIC_RTP_MODE) &&
	       !atomic_read(&aw869x->exit_in_rtp_loop)) {
		if (aw869x->is_custom_wave == 0) {
			if ((aw869x_rtp->len - aw869x->rtp_cnt) <
			(aw869x->ram.base_addr >> 2)) {
				buf_len = aw869x_rtp->len - aw869x->rtp_cnt;
			} else {
				buf_len = (aw869x->ram.base_addr >> 2);
			}
			aw869x_i2c_writes(aw869x, AW869X_REG_RTP_DATA,
					  &aw869x_rtp->data[aw869x->rtp_cnt],
					  buf_len);
			aw869x->rtp_cnt += buf_len;
			if (aw869x->rtp_cnt == aw869x_rtp->len) {
				aw869x->rtp_cnt = 0;
				aw869x_haptic_set_rtp_aei(aw869x, false);
				break;
			}
		} else {
			buf_len = read_rb(aw869x_rtp->data,  period_size);
			aw869x_i2c_writes(aw869x, AW869X_REG_RTP_DATA,
					  aw869x_rtp->data, buf_len);
			aw869x->rtp_cnt += buf_len;
			if (buf_len < period_size) {
				aw_info("%s: custom rtp update complete\n", __func__);
				aw869x->rtp_cnt = 0;
				aw869x_haptic_set_rtp_aei(aw869x, false);
				break;
			}
		}
	}
	enable_irq(gpio_to_irq(aw869x->irq_gpio));
	if (aw869x->play_mode == AW869X_HAPTIC_RTP_MODE &&
	    !atomic_read(&aw869x->exit_in_rtp_loop) && aw869x->rtp_cnt != 0) {
		aw869x_haptic_set_rtp_aei(aw869x, true);
	}
	aw_info("%s: exit\n", __func__);
	pm_qos_enable(false);
	return 0;
}

static int16_t aw869x_haptic_effect_strength(struct aw869x *aw869x)
{
	aw_dbg("%s enter\n", __func__);
	aw_dbg("%s: aw869x->play.vmax_mv =0x%x\n", __func__,
		 aw869x->play.vmax_mv);
#if 0
	switch (aw869x->play.vmax_mv) {
	case AW869X_LIGHT_MAGNITUDE:
		aw869x->level = 0x80;
		break;
	case AW869X_MEDIUM_MAGNITUDE:
		aw869x->level = 0x50;
		break;
	case AW869X_STRONG_MAGNITUDE:
		aw869x->level = 0x30;
		break;
	default:
		break;
	}
#else
	if (aw869x->play.vmax_mv >= 0x7FFF)
		aw869x->level = 0x80; /*128*/
	else if (aw869x->play.vmax_mv <= 0x3FFF)
		aw869x->level = 0x1E; /*30*/
	else
		aw869x->level = (aw869x->play.vmax_mv - 16383) / 128;
	if (aw869x->level < 0x1E)
		aw869x->level = 0x1E; /*30*/
#endif

	aw_info("%s: aw869x->level =0x%x\n", __func__, aw869x->level);
	return 0;
}

static int aw869x_haptic_play_effect_seq(struct aw869x *aw869x,
					 unsigned char flag)
{
	if (aw869x->effect_id > aw869x->info.effect_id_boundary)
		return 0;
	/* aw_info("%s:aw869x->effect_id =%d\n", __func__, aw869x->effect_id);
	 * aw_info("%s:aw869x->activate_mode =%d\n", __func__,
	 *	  aw869x->activate_mode);
	 */

	if (flag) {
		if (aw869x->activate_mode == AW869X_HAPTIC_ACTIVATE_RAM_MODE) {
			aw869x_haptic_set_wav_seq(aw869x, 0x00,
						(char)aw869x->effect_id + 1);
			aw869x_haptic_set_wav_seq(aw869x, 0x01, 0x00);
			aw869x_haptic_set_wav_loop(aw869x, 0x00, 0x00);
			aw869x_haptic_play_mode(aw869x, AW869X_HAPTIC_RAM_MODE);
			if (aw869x->info.bst_vol_ram <= AW869X_MAX_BST_VO)
				aw869x_haptic_set_bst_vol(aw869x,
						aw869x->info.bst_vol_ram);
			else
				aw869x_haptic_set_bst_vol(aw869x, aw869x->vmax);
			aw869x_haptic_effect_strength(aw869x);
			aw869x_haptic_set_gain(aw869x, aw869x->level);
			aw869x_haptic_start(aw869x);
		}
		if (aw869x->activate_mode == AW869X_HAPTIC_ACTIVATE_RAM_LOOP_MODE) {
			aw869x_haptic_set_repeat_wav_seq(aw869x,
					(aw869x->info.effect_id_boundary + 1));
			aw869x_haptic_set_gain(aw869x, aw869x->level);
			aw869x_haptic_play_repeat_seq(aw869x, true);
		}
	}

	return 0;
}

static void aw869x_haptic_upload_lra(struct aw869x *aw869x, unsigned char flag)
{
	switch (flag) {
	case AW869X_WRITE_ZERO:
		aw_info("%s: write zero to trim_lra!\n", __func__);
		aw869x_i2c_write(aw869x, AW869X_REG_TRIM_LRA, 0x00);
		break;
	case AW869X_F0_CALI_LRA:
		aw_info("%s: f0_cali_lra=%d\n", __func__,
			aw869x->f0_calib_data);
		aw869x_i2c_write(aw869x, AW869X_REG_TRIM_LRA,
				 (char)aw869x->f0_calib_data);
		break;
	case AW869X_OSC_CALI_LRA:
		aw_info("%s: rtp_cali_lra=%d\n", __func__,
			aw869x->lra_calib_data);
		aw869x_i2c_write(aw869x, AW869X_REG_TRIM_LRA,
				 (char)aw869x->lra_calib_data);
		break;
	default:
		break;
	}

}

static int aw869x_clock_OSC_trim_calibration(unsigned long int theory_time,
					     unsigned long int real_time)
{
	unsigned int real_code = 0;
	unsigned int LRA_TRIM_CODE = 0;
	unsigned int DFT_LRA_TRIM_CODE = 0;
	unsigned int Not_need_cali_threshold = 10;/*0.1 percent not need calibrate*/

	if (theory_time == real_time) {
		aw_info("aw_osctheory_time == real_time:%ld  theory_time = %ld not need to cali\n",
			real_time, theory_time);
		return 0;
	} else if (theory_time < real_time) {
		if ((real_time - theory_time) > (theory_time / 50)) {
			aw_info("aw_osc(real_time - theory_time) > (theory_time/50) not to cali\n");
			return DFT_LRA_TRIM_CODE;
		}

		if ((real_time - theory_time) < (Not_need_cali_threshold*theory_time/10000)) {
			aw_info("aw_oscmicrosecond:%ld  theory_time = %ld not need to cali\n",
				real_time, theory_time);
			return DFT_LRA_TRIM_CODE;
		}

		real_code = ((real_time - theory_time) * 4000) / theory_time;
		real_code = ((real_code%10 < 5) ? 0 : 1) + real_code/10;
		real_code = 32 + real_code;
	} else if (theory_time > real_time) {
		if ((theory_time - real_time) > (theory_time / 50)) {
			aw_info("aw_osc((theory_time - real_time) > (theory_time / 50)) not to cali\n");
			return DFT_LRA_TRIM_CODE;
		}
		if ((theory_time - real_time) < (Not_need_cali_threshold * theory_time/10000)) {
			aw_info("aw_oscmicrosecond:%ld  theory_time = %ld not need to cali\n",
				real_time, theory_time);
			return DFT_LRA_TRIM_CODE;
		}
		real_code = ((theory_time - real_time) * 4000) / theory_time;
		real_code = ((real_code%10 < 5) ? 0 : 1) + real_code/10;
		real_code = 32 - real_code;
	}
	if (real_code > 31)
		LRA_TRIM_CODE = real_code - 32;
	else
		LRA_TRIM_CODE = real_code + 32;
	aw_info("aw_oscmicrosecond:%ld  theory_time = %ld real_code =0X%02X LRA_TRIM_CODE 0X%02X\n",
		real_time, theory_time, real_code, LRA_TRIM_CODE);

	return LRA_TRIM_CODE;
}

static int aw869x_rtp_trim_lra_calibration(struct aw869x *aw869x)
{
	unsigned char reg_val = 0;
	unsigned int fre_val = 0;
	unsigned int theory_time = 0;
	unsigned int lra_rtim_code = 0;

	aw869x_i2c_read(aw869x, AW869X_REG_PWMDBG, &reg_val);
	fre_val = (reg_val & 0x006f) >> 5;

	if (fre_val == 3)
		theory_time = (aw869x->rtp_len / 12000) * 1000000; /*12K */
	if (fre_val == 2)
		theory_time = (aw869x->rtp_len / 24000) * 1000000; /*24K */
	if (fre_val == 1 || fre_val == 0)
		theory_time = (aw869x->rtp_len / 48000) * 1000000; /*48K */

	aw_info("microsecond:%ld  theory_time = %d\n",
	       aw869x->microsecond, theory_time);

	lra_rtim_code = aw869x_clock_OSC_trim_calibration(theory_time,
							  aw869x->microsecond);
	if (lra_rtim_code > 0) {
		aw869x->lra_calib_data = lra_rtim_code;
		aw869x_i2c_write(aw869x, AW869X_REG_TRIM_LRA,
				 (char)lra_rtim_code);
	}
	return 0;
}
static unsigned char aw869x_haptic_osc_read_int(struct aw869x *aw869x)
{
	unsigned char reg_val = 0;

	aw869x_i2c_read(aw869x, AW869X_REG_DBGSTAT, &reg_val);
	return reg_val;
}

static int aw869x_rtp_osc_calibration(struct aw869x *aw869x)
{
	const struct firmware *rtp_file;
	int ret = -1;
	unsigned int buf_len = 0;
	unsigned char osc_int_state = 0;

	aw869x->rtp_cnt = 0;
	aw869x->timeval_flags = 1;
	aw869x->osc_cali_flag = 1;

	aw_info("%s enter\n", __func__);
	/* fw loaded */
	ret = request_firmware(&rtp_file,
			       awinic_rtp_name[/*aw869x->rtp_file_num*/ 0],
			       aw869x->dev);
	if (ret < 0) {
		aw_err("%s: failed to read %s\n", __func__,
			awinic_rtp_name[/*aw869x->rtp_file_num*/ 0]);
		return ret;
	}
	/*awinic add stop,for irq interrupt during calibrate*/
	aw869x_haptic_stop(aw869x);
	aw869x->rtp_init = 0;
	mutex_lock(&aw869x->rtp_lock);
	vfree(aw869x_rtp);
	aw869x_rtp = vmalloc(rtp_file->size+sizeof(int));
	if (!aw869x_rtp) {
		release_firmware(rtp_file);
		mutex_unlock(&aw869x->rtp_lock);
		aw_err("%s: error allocating memory\n", __func__);
		return -ENOMEM;
	}
	aw869x_rtp->len = rtp_file->size;
	aw869x->rtp_len = rtp_file->size;
	aw_info("%s: rtp file [%s] size = %d\n", __func__,
		awinic_rtp_name[/*aw869x->rtp_file_num*/ 0], aw869x_rtp->len);
	memcpy(aw869x_rtp->data, rtp_file->data, rtp_file->size);
	release_firmware(rtp_file);
	mutex_unlock(&aw869x->rtp_lock);

	/* gain */
	aw869x_haptic_ram_vbat_comp(aw869x, false);

	/* rtp mode config */
	aw869x_haptic_play_mode(aw869x, AW869X_HAPTIC_RTP_MODE);

	aw869x_i2c_write_bits(aw869x, AW869X_REG_DBGCTRL,
			      AW869X_BIT_DBGCTRL_INT_MODE_MASK,
			      AW869X_BIT_DBGCTRL_INT_MODE_EDGE);
	disable_irq(gpio_to_irq(aw869x->irq_gpio));
	/* haptic start */
	aw869x_haptic_start(aw869x);
	pm_qos_enable(true);
	while (1) {
		if (!aw869x_haptic_rtp_get_fifo_afi(aw869x)) {
			mutex_lock(&aw869x->rtp_lock);
			if ((aw869x_rtp->len - aw869x->rtp_cnt) < (aw869x->ram.base_addr>>2))
				buf_len = aw869x_rtp->len-aw869x->rtp_cnt;
			else
				buf_len = (aw869x->ram.base_addr >> 2);
			if (aw869x->rtp_cnt != aw869x_rtp->len) {
				if (aw869x->timeval_flags == 1) {
					ktime_get_real_ts64(&aw869x->start);
					aw869x->timeval_flags = 0;
				}
				aw869x_i2c_writes(aw869x, AW869X_REG_RTP_DATA, &aw869x_rtp->data[aw869x->rtp_cnt], buf_len);
				aw869x->rtp_cnt += buf_len;
			}
			mutex_unlock(&aw869x->rtp_lock);
		}
		osc_int_state = aw869x_haptic_osc_read_int(aw869x);
		if (osc_int_state&AW869X_BIT_SYSINT_DONEI) {
			ktime_get_real_ts64(&aw869x->end);
			aw_info("%s vincent playback done aw869x->rtp_cnt= %d\n",
				__func__, aw869x->rtp_cnt);
			break;
		}

		ktime_get_real_ts64(&aw869x->end);
		aw869x->microsecond = (aw869x->end.tv_sec - aw869x->start.tv_sec)*1000000 +
					(aw869x->end.tv_nsec - aw869x->start.tv_nsec) / 1000000;
		if (aw869x->microsecond > OSC_CALIBRATION_T_LENGTH) {
			aw_info("%s vincent time out aw869x->rtp_cnt %d osc_int_state %02x\n",
				__func__, aw869x->rtp_cnt, osc_int_state);
			break;
		}
	}
	pm_qos_enable(false);
	enable_irq(gpio_to_irq(aw869x->irq_gpio));

	aw869x->osc_cali_flag = 0;
	aw869x->microsecond = (aw869x->end.tv_sec - aw869x->start.tv_sec)*1000000 +
				(aw869x->end.tv_nsec - aw869x->start.tv_nsec) / 1000000;
	/*calibration osc*/
	aw_info("%s 2018_microsecond:%ld\n", __func__, aw869x->microsecond);
	aw_info("%s exit\n", __func__);
	return 0;
}


static void aw869x_rtp_work_routine(struct work_struct *work)
{
	const struct firmware *rtp_file;
	int ret = -1;
	struct aw869x *aw869x = container_of(work, struct aw869x, rtp_work);

	if ((aw869x->effect_id < aw869x->info.effect_id_boundary) &&
	    (aw869x->effect_id > aw869x->info.effect_max))
		return;

	aw_info("%s: effect_id = %d state=%d activate_mode = %d\n", __func__,
		aw869x->effect_id, aw869x->state, aw869x->activate_mode);
	mutex_lock(&aw869x->lock);
	aw869x_haptic_upload_lra(aw869x, AW869X_OSC_CALI_LRA);
	aw869x_haptic_set_rtp_aei(aw869x, false);
	aw869x_interrupt_clear(aw869x);
	/* wait for irq to exit */
	atomic_set(&aw869x->exit_in_rtp_loop, 1);
	while (atomic_read(&aw869x->is_in_rtp_loop)) {
		aw_info("%s  goint to waiting irq exit\n", __func__);
		mutex_unlock(&aw869x->lock);
		ret = wait_event_interruptible(aw869x->wait_q,
				atomic_read(&aw869x->is_in_rtp_loop) == 0);
		aw_info("%s  wakeup\n", __func__);
		mutex_lock(&aw869x->lock);
		if (ret == -ERESTARTSYS) {
			atomic_set(&aw869x->exit_in_rtp_loop, 0);
			wake_up_interruptible(&aw869x->stop_wait_q);
			mutex_unlock(&aw869x->lock);
			aw_err("%s wake up by signal return erro\n", __func__);
			return;
		}
	}

	atomic_set(&aw869x->exit_in_rtp_loop, 0);
	wake_up_interruptible(&aw869x->stop_wait_q);

	/* how to force exit this call */
	if (aw869x->is_custom_wave == 1 && aw869x->state) {
		aw_err("%s buffer size %d, availbe size %d\n", __func__,
		       aw869x->ram.base_addr >> 2, get_rb_avalible_size());
		while (get_rb_avalible_size() < aw869x->ram.base_addr &&
		       !rb_shoule_exit()) {
			mutex_unlock(&aw869x->lock);
			ret = wait_event_interruptible(aw869x->stop_wait_q, (get_rb_avalible_size() >= aw869x->ram.base_addr) || rb_shoule_exit());
			aw_info("%s  wakeup\n", __func__);
			aw_err("%s after wakeup sbuffer size %d, availbe size %d\n",
				__func__, aw869x->ram.base_addr >> 2,
				get_rb_avalible_size());
			if (ret == -ERESTARTSYS) {
				aw_err("%s wake up by signal return erro\n",
				       __func__);
				return;
			}
			mutex_lock(&aw869x->lock);

		}
	}

	aw869x_haptic_stop(aw869x);

	if (aw869x->state) {
		pm_stay_awake(aw869x->dev);
		if (aw869x->info.bst_vol_ram <= AW869X_MAX_BST_VO)
			aw869x_haptic_set_bst_vol(aw869x,
						  aw869x->info.bst_vol_rtp);
		else
			aw869x_haptic_set_bst_vol(aw869x, aw869x->vmax);
		aw869x_haptic_ram_vbat_comp(aw869x, false);
		aw869x_haptic_effect_strength(aw869x);
		aw869x_haptic_set_gain(aw869x, aw869x->level);
		aw869x->rtp_init = 0;
		if (aw869x->is_custom_wave == 0) {
			aw869x->rtp_file_num =
			    aw869x->effect_id - aw869x->info.effect_id_boundary;
			aw_info("%s: aw869x->rtp_file_num =%d\n", __func__,
				aw869x->rtp_file_num);
			if (aw869x->rtp_file_num < 0)
				aw869x->rtp_file_num = 0;
			if (aw869x->rtp_file_num > ((awinic_rtp_name_len) - 1))
				aw869x->rtp_file_num = (awinic_rtp_name_len) - 1;

			/* fw loaded */
			ret = request_firmware(&rtp_file,
					       awinic_rtp_name[aw869x->rtp_file_num],
					       aw869x->dev);
			if (ret < 0) {
				aw_err("%s: failed to read %s\n", __func__,
				       awinic_rtp_name[aw869x->rtp_file_num]);
				pm_relax(aw869x->dev);
				mutex_unlock(&aw869x->lock);
				return;
			}

			vfree(aw869x_rtp);
			aw869x_rtp = vmalloc(rtp_file->size + sizeof(int));
			if (!aw869x_rtp) {
				release_firmware(rtp_file);
				aw_err("%s: error allocating memory\n",
				       __func__);
				pm_relax(aw869x->dev);
				mutex_unlock(&aw869x->lock);
				return;
			}
			aw869x_rtp->len = rtp_file->size;
			aw_info("%s: rtp file [%s] size = %d\n", __func__,
				awinic_rtp_name[aw869x->rtp_file_num],
				aw869x_rtp->len);
			memcpy(aw869x_rtp->data, rtp_file->data,
			       rtp_file->size);
			release_firmware(rtp_file);
		} else  {
			vfree(aw869x_rtp);
			aw869x_rtp = vmalloc(aw869x->ram.base_addr >> 2);
			if (!aw869x_rtp) {
				aw_err("%s: error allocating memory\n",
				       __func__);
				pm_relax(aw869x->dev);
				mutex_unlock(&aw869x->lock);
				return;
			}
		}
		aw869x->rtp_init = 1;

		/* rtp mode config */
		aw869x_haptic_play_mode(aw869x, AW869X_HAPTIC_RTP_MODE);

		/* haptic start */
		aw869x_haptic_start(aw869x);

		aw869x_haptic_rtp_init(aw869x);
	} else {
		aw869x->rtp_cnt = 0;
		aw869x->rtp_init = 0;
		pm_relax(aw869x->dev);
	}
	mutex_unlock(&aw869x->lock);
}

static enum
hrtimer_restart aw869x_haptic_audio_timer_func(struct hrtimer *timer)
{
	struct aw869x *aw869x =
	    container_of(timer, struct aw869x, haptic_audio.timer);

	aw_dbg("%s enter\n", __func__);
	/* schedule_work(&aw869x->haptic_audio.work); */
	queue_work(aw869x->work_queue, &aw869x->haptic_audio.work);

	hrtimer_start(&aw869x->haptic_audio.timer,
		      ktime_set(aw869x->haptic_audio.timer_val / 1000000,
				(aw869x->haptic_audio.timer_val % 1000000) *
				1000), HRTIMER_MODE_REL);
	return HRTIMER_NORESTART;
}

static void aw869x_haptic_audio_work_routine(struct work_struct *work)
{
	struct aw869x *aw869x =
	    container_of(work, struct aw869x, haptic_audio.work);

	aw_info("%s enter\n", __func__);

	mutex_lock(&aw869x->haptic_audio.lock);
	memcpy(&aw869x->haptic_audio.ctr,
	       &aw869x->haptic_audio.data[aw869x->haptic_audio.cnt],
	       sizeof(struct haptic_ctr));
	aw_dbg("%s: cnt=%d, cmd=%d, play=%d, wavseq=%d, loop=%d, gain=%d\n",
		 __func__,
		 aw869x->haptic_audio.cnt,
		 aw869x->haptic_audio.ctr.cmd,
		 aw869x->haptic_audio.ctr.play,
		 aw869x->haptic_audio.ctr.wavseq,
		 aw869x->haptic_audio.ctr.loop, aw869x->haptic_audio.ctr.gain);
	mutex_unlock(&aw869x->haptic_audio.lock);
	if (aw869x->haptic_audio.ctr.cmd == AW869X_HAPTIC_CMD_ENABLE) {
		if (AW869X_HAPTIC_PLAY_ENABLE ==
		    aw869x->haptic_audio.ctr.play) {
			aw_info("%s: haptic_audio_play_start\n", __func__);
			mutex_lock(&aw869x->lock);
			aw869x_haptic_stop(aw869x);
			aw869x_haptic_play_mode(aw869x, AW869X_HAPTIC_RAM_MODE);

			aw869x_haptic_set_wav_seq(aw869x, 0x00,
						  aw869x->haptic_audio.
						  ctr.wavseq);
			aw869x_haptic_set_wav_seq(aw869x, 0x01, 0x00);

			aw869x_haptic_set_wav_loop(aw869x, 0x00,
						   aw869x->haptic_audio.
						   ctr.loop);

			aw869x_haptic_set_gain(aw869x,
					       aw869x->haptic_audio.ctr.gain);

			aw869x_haptic_start(aw869x);
			mutex_unlock(&aw869x->lock);
		} else if (AW869X_HAPTIC_PLAY_STOP ==
			   aw869x->haptic_audio.ctr.play) {
			mutex_lock(&aw869x->lock);
			aw869x_haptic_stop(aw869x);
			mutex_unlock(&aw869x->lock);
		} else if (AW869X_HAPTIC_PLAY_GAIN ==
			   aw869x->haptic_audio.ctr.play) {
			mutex_lock(&aw869x->lock);
			aw869x_haptic_set_gain(aw869x,
					       aw869x->haptic_audio.ctr.gain);
			mutex_unlock(&aw869x->lock);
		}
		mutex_lock(&aw869x->haptic_audio.lock);
		memset(&aw869x->haptic_audio.data[aw869x->haptic_audio.cnt],
		       0, sizeof(struct haptic_ctr));
		mutex_unlock(&aw869x->haptic_audio.lock);
	}

	mutex_lock(&aw869x->haptic_audio.lock);
	aw869x->haptic_audio.cnt++;
	if (aw869x->haptic_audio.data[aw869x->haptic_audio.cnt].cmd == 0) {
		aw869x->haptic_audio.cnt = 0;
		aw_dbg("%s: haptic play buffer restart\n", __func__);
	}
	mutex_unlock(&aw869x->haptic_audio.lock);

}

/*****************************************************
 *
 * haptic cont
 *
 *****************************************************/
static int aw869x_haptic_cont(struct aw869x *aw869x)
{
	aw_info("%s enter\n", __func__);

	/* work mode */
	aw869x_haptic_play_mode(aw869x, AW869X_HAPTIC_CONT_MODE);

	/* preset f0 */
	aw869x_haptic_set_f0_preset(aw869x);

	/* lpf */
	aw869x_i2c_write_bits(aw869x, AW869X_REG_DATCTRL,
			      AW869X_BIT_DATCTRL_FC_MASK,
			      AW869X_BIT_DATCTRL_FC_1000HZ);
	aw869x_i2c_write_bits(aw869x, AW869X_REG_DATCTRL,
			      AW869X_BIT_DATCTRL_LPF_ENABLE_MASK,
			      AW869X_BIT_DATCTRL_LPF_ENABLE);

	/* cont config */
	aw869x_i2c_write_bits(aw869x, AW869X_REG_CONT_CTRL,
			      AW869X_BIT_CONT_CTRL_ZC_DETEC_MASK,
			      AW869X_BIT_CONT_CTRL_ZC_DETEC_ENABLE);
	aw869x_i2c_write_bits(aw869x, AW869X_REG_CONT_CTRL,
			      AW869X_BIT_CONT_CTRL_WAIT_PERIOD_MASK,
			      AW869X_BIT_CONT_CTRL_WAIT_1PERIOD);
	aw869x_i2c_write_bits(aw869x, AW869X_REG_CONT_CTRL,
			      AW869X_BIT_CONT_CTRL_MODE_MASK,
			      AW869X_BIT_CONT_CTRL_BY_GO_SIGNAL);
	aw869x_i2c_write_bits(aw869x, AW869X_REG_CONT_CTRL,
			      AW869X_BIT_CONT_CTRL_EN_CLOSE_MASK,
			      AW869X_CONT_PLAYBACK_MODE);
	aw869x_i2c_write_bits(aw869x, AW869X_REG_CONT_CTRL,
			      AW869X_BIT_CONT_CTRL_F0_DETECT_MASK,
			      AW869X_BIT_CONT_CTRL_F0_DETECT_DISABLE);
	aw869x_i2c_write_bits(aw869x, AW869X_REG_CONT_CTRL,
			      AW869X_BIT_CONT_CTRL_O2C_MASK,
			      AW869X_BIT_CONT_CTRL_O2C_DISABLE);
	aw869x_i2c_write_bits(aw869x, AW869X_REG_CONT_CTRL,
			      AW869X_BIT_CONT_CTRL_AUTO_BRK_MASK,
			      AW869X_BIT_CONT_CTRL_AUTO_BRK_ENABLE);

	/* TD time */
	aw869x_i2c_write(aw869x, AW869X_REG_TD_H,
			 (unsigned char)(aw869x->info.cont_td >> 8));
	aw869x_i2c_write(aw869x, AW869X_REG_TD_L,
			 (unsigned char)(aw869x->info.cont_td >> 0));
	aw869x_i2c_write(aw869x, AW869X_REG_TSET, aw869x->info.tset);

	/* zero cross */
	aw869x_i2c_write(aw869x, AW869X_REG_ZC_THRSH_H,
			 (unsigned char)(aw869x->info.cont_zc_thr >> 8));
	aw869x_i2c_write(aw869x, AW869X_REG_ZC_THRSH_L,
			 (unsigned char)(aw869x->info.cont_zc_thr >> 0));

	aw869x_i2c_write_bits(aw869x, AW869X_REG_BEMF_NUM,
			      AW869X_BIT_BEMF_NUM_BRK_MASK,
			      aw869x->info.cont_num_brk);
	/* 35*171us=5.985ms */
	aw869x_i2c_write(aw869x, AW869X_REG_TIME_NZC, 0x23);

	/* f0 driver level */
	aw869x_i2c_write(aw869x, AW869X_REG_DRV_LVL, aw869x->info.cont_drv_lvl);
	aw869x_i2c_write(aw869x, AW869X_REG_DRV_LVL_OV,
			 aw869x->info.cont_drv_lvl_ov);

	/* cont play go */
	aw869x_haptic_play_go(aw869x, true);

	return 0;
}
static int aw869x_get_glb_state(struct aw869x *aw869x)
{
	unsigned char glb_state_val = 0;

	aw869x_i2c_read(aw869x, AW869X_REG_GLB_STATE, &glb_state_val);
	return glb_state_val;
}

#ifndef USE_CONT_F0_CALI
/*****************************************************
 *
 * haptic f0 cali
 *
 *****************************************************/
static int aw869x_haptic_get_f0(struct aw869x *aw869x)
{
	int ret = 0;
	unsigned char i = 0;
	unsigned char reg_val = 0;
	unsigned char f0_pre_num = 0;
	unsigned char f0_wait_num = 0;
	unsigned char f0_repeat_num = 0;
	unsigned char f0_trace_num = 0;
	unsigned int t_f0_ms = 0;
	unsigned int t_f0_trace_ms = 0;
	unsigned int f0_cali_cnt = 50;

	aw_info("%s enter\n", __func__);

	aw869x->f0 = aw869x->info.f0_pre;

	/* f0 calibrate work mode */
	aw869x_haptic_stop(aw869x);
	aw869x_haptic_play_mode(aw869x, AW869X_HAPTIC_CONT_MODE);

	aw869x_i2c_write_bits(aw869x, AW869X_REG_CONT_CTRL,
			      AW869X_BIT_CONT_CTRL_EN_CLOSE_MASK,
			      AW869X_BIT_CONT_CTRL_OPEN_PLAYBACK);
	aw869x_i2c_write_bits(aw869x, AW869X_REG_CONT_CTRL,
			      AW869X_BIT_CONT_CTRL_F0_DETECT_MASK,
			      AW869X_BIT_CONT_CTRL_F0_DETECT_ENABLE);

	/* LPF */
	aw869x_i2c_write_bits(aw869x, AW869X_REG_DATCTRL,
			      AW869X_BIT_DATCTRL_FC_MASK,
			      AW869X_BIT_DATCTRL_FC_1000HZ);
	aw869x_i2c_write_bits(aw869x, AW869X_REG_DATCTRL,
			      AW869X_BIT_DATCTRL_LPF_ENABLE_MASK,
			      AW869X_BIT_DATCTRL_LPF_ENABLE);
	/* preset f0 */
	aw869x_haptic_set_f0_preset(aw869x);

	/* f0 driver level */
	aw869x_i2c_write(aw869x, AW869X_REG_DRV_LVL, aw869x->info.cont_drv_lvl);

	/* f0 trace parameter */
	f0_pre_num = aw869x->info.f0_trace_parameter[0];
	f0_wait_num = aw869x->info.f0_trace_parameter[1];
	f0_repeat_num = aw869x->info.f0_trace_parameter[2];
	f0_trace_num = aw869x->info.f0_trace_parameter[3];
	aw869x_i2c_write(aw869x, AW869X_REG_NUM_F0_1,
			 (f0_pre_num << 4) | (f0_wait_num << 0));
	aw869x_i2c_write(aw869x, AW869X_REG_NUM_F0_2, (f0_repeat_num << 0));
	aw869x_i2c_write(aw869x, AW869X_REG_NUM_F0_3, (f0_trace_num << 0));

	/* clear aw869x interrupt */
	ret = aw869x_i2c_read(aw869x, AW869X_REG_SYSINT, &reg_val);

	/* play go and start f0 calibration */
	aw869x_haptic_play_go(aw869x, true);

	/* f0 trace time */
	t_f0_ms = 1000 * 10 / aw869x->info.f0_pre;
	t_f0_trace_ms =
	    t_f0_ms * (f0_pre_num + f0_wait_num +
		       (f0_trace_num + f0_wait_num) * (f0_repeat_num - 1)) + 50;
	usleep_range(t_f0_trace_ms * 1000, t_f0_trace_ms * 1000 + 500);

	for (i = 0; i < f0_cali_cnt; i++) {
		reg_val = aw869x_get_glb_state(aw869x);
		/* f0 calibrate done */
		if ((reg_val & 0x0f) == 0x00) {
			aw869x_haptic_read_f0(aw869x);
			aw869x_haptic_read_beme(aw869x);
			break;
		}
		usleep_range(10000, 10500);
		aw_info("%s: f0 cali sleep 10ms\n", __func__);
	}
	if (i == f0_cali_cnt)
		ret = -1;
	else
		ret = 0;
	/* restore default config */
	aw869x_i2c_write_bits(aw869x, AW869X_REG_CONT_CTRL,
			      AW869X_BIT_CONT_CTRL_EN_CLOSE_MASK,
			      AW869X_CONT_PLAYBACK_MODE);
	aw869x_i2c_write_bits(aw869x, AW869X_REG_CONT_CTRL,
			      AW869X_BIT_CONT_CTRL_F0_DETECT_MASK,
			      AW869X_BIT_CONT_CTRL_F0_DETECT_DISABLE);

	return ret;
}
#else
/*****************************************************
 *
 * haptic cont mode f0 cali
 *
 *****************************************************/
static int aw869x_haptic_get_f0(struct aw869x *aw869x)
{
	int ret = 0;
	unsigned char i = 0;
	unsigned char reg_val = 0;
	unsigned int t_f0_trace_ms = 0;
	unsigned int f0_cali_cnt = 50;

	aw_info("%s enter\n", __func__);

	aw869x->f0 = aw869x->info.f0_pre;

	/* f0 calibrate work mode */
	aw869x_haptic_stop(aw869x);
	aw869x_haptic_play_mode(aw869x, AW869X_HAPTIC_CONT_MODE);

	aw869x_i2c_write_bits(aw869x, AW869X_REG_CONT_CTRL,
			      AW869X_BIT_CONT_CTRL_MODE_MASK,
			      AW869X_BIT_CONT_CTRL_BY_DRV_TIME);
	aw869x_i2c_write_bits(aw869x, AW869X_REG_CONT_CTRL,
			      AW869X_BIT_CONT_CTRL_EN_CLOSE_MASK,
			      AW869X_BIT_CONT_CTRL_OPEN_PLAYBACK);
	aw869x_i2c_write_bits(aw869x, AW869X_REG_CONT_CTRL,
			      AW869X_BIT_CONT_CTRL_EN_CLOSE_MASK,
			      AW869X_BIT_CONT_CTRL_CLOSE_PLAYBACK);
	aw869x_i2c_write_bits(aw869x, AW869X_REG_CONT_CTRL,
			      AW869X_BIT_CONT_CTRL_F0_DETECT_MASK,
			      AW869X_BIT_CONT_CTRL_F0_DETECT_DISABLE);
	aw869x_i2c_write_bits(aw869x, AW869X_REG_CONT_CTRL,
			      AW869X_BIT_CONT_CTRL_AUTO_BRK_MASK,
			      AW869X_BIT_CONT_CTRL_AUTO_BRK_DISABLE);

	/* LPF */
	/*aw869x_i2c_write_bits(aw869x, AW869X_REG_DATCTRL,
	 *        AW869X_BIT_DATCTRL_FC_MASK, AW869X_BIT_DATCTRL_FC_1000HZ);
	 *aw869x_i2c_write_bits(aw869x, AW869X_REG_DATCTRL,
	 *			AW869X_BIT_DATCTRL_LPF_ENABLE_MASK,
	 *			AW869X_BIT_DATCTRL_LPF_ENABLE);
	 */

	/* f0 driver level */
	aw869x_i2c_write(aw869x, AW869X_REG_DRV_LVL, aw869x->info.cont_drv_lvl);
	aw869x_i2c_write(aw869x, AW869X_REG_DRV_LVL_OV,
			 aw869x->info.cont_drv_lvl_ov);

	/* TD time */
	aw869x_i2c_write(aw869x, AW869X_REG_TD_H, aw869x->info.cont_td >> 8);
	aw869x_i2c_write(aw869x, AW869X_REG_TD_L, aw869x->info.cont_td);
	aw869x_i2c_write(aw869x, AW869X_REG_TSET, aw869x->info.tset);

	/* drive time  */
	aw869x_i2c_write(aw869x, AW869X_REG_DRV_TIME, 0x75);

	/* preset f0 */
	aw869x_haptic_set_f0_preset(aw869x);

	/* clear aw869x interrupt */
	ret = aw869x_i2c_read(aw869x, AW869X_REG_SYSINT, &reg_val);

	/* play go and start f0 calibration */
	aw869x_haptic_play_go(aw869x, true);

	/* f0 trace time */
	t_f0_trace_ms = 0xfe * 684 / 1000;
	msleep(t_f0_trace_ms);

	for (i = 0; i < f0_cali_cnt; i++) {
		ret = aw869x_i2c_read(aw869x, AW869X_REG_SYSINT, &reg_val);
		/* f0 calibrate done */
		if (reg_val & 0x01) {
			aw869x_haptic_read_cont_f0(aw869x);
			aw869x_haptic_read_cont_bemf(aw869x);
			break;
		}
		msleep(10);
		aw_info("%s f0 cali sleep 10ms\n", __func__);
	}

	if (i == f0_cali_cnt)
		ret = -1;
	else
		ret = 0;


	/* restore default config */
	aw869x_i2c_write_bits(aw869x, AW869X_REG_CONT_CTRL,
			      AW869X_BIT_CONT_CTRL_EN_CLOSE_MASK,
			      AW869X_CONT_PLAYBACK_MODE);
	aw869x_i2c_write_bits(aw869x, AW869X_REG_CONT_CTRL,
			      AW869X_BIT_CONT_CTRL_F0_DETECT_MASK,
			      AW869X_BIT_CONT_CTRL_F0_DETECT_DISABLE);

	return ret;
}
#endif

static int aw869x_haptic_f0_calibration(struct aw869x *aw869x)
{
	int ret = 0;
	unsigned char reg_val = 0;
	unsigned int f0_limit = 0;
	char f0_cali_lra = 0;
	int f0_cali_step = 0;

	aw_info("%s enter\n", __func__);

	if (aw869x_haptic_get_f0(aw869x)) {
		aw_err("%s get f0 error, user defafult f0\n", __func__);
	} else {
		/* max and min limit */
		f0_limit = aw869x->f0;
		if (aw869x->f0 * 100 <
		    aw869x->info.f0_pre * (100 - aw869x->info.f0_cali_percen)) {
			f0_limit = aw869x->info.f0_pre;
		}
		if (aw869x->f0 * 100 >
		    aw869x->info.f0_pre * (100 + aw869x->info.f0_cali_percen)) {
			f0_limit = aw869x->info.f0_pre;
		}

		/* calculate cali step */
		f0_cali_step =
		    100000 * ((int)f0_limit -
			      (int)aw869x->info.f0_pre) / ((int)f0_limit * 25);
		aw_info("%s  line=%d f0_cali_step=%d\n", __func__, __LINE__,
		       f0_cali_step);
		aw_info("%s line=%d  f0_limit=%d\n", __func__, __LINE__,
		       (int)f0_limit);
		aw_info("%s line=%d  aw869x->info.f0_pre=%d\n", __func__,
		       __LINE__, (int)aw869x->info.f0_pre);

		if (f0_cali_step >= 0) {	/*f0_cali_step >= 0 */
			if (f0_cali_step % 10 >= 5)
				f0_cali_step = f0_cali_step / 10 + 1 + 32;
			else
				f0_cali_step = f0_cali_step / 10 + 32;

		} else {	/*f0_cali_step < 0 */
			if (f0_cali_step % 10 <= -5)
				f0_cali_step = 32 + (f0_cali_step / 10 - 1);
			else
				f0_cali_step = 32 + f0_cali_step / 10;

		}

		if (f0_cali_step > 31)
			f0_cali_lra = (char)f0_cali_step - 32;
		else
			f0_cali_lra = (char)f0_cali_step + 32;

		aw869x->f0_calib_data = (int)f0_cali_lra;
		aw_info("%s f0_cali_lra=%d\n", __func__, (int)f0_cali_lra);

		/* update cali step */
		aw869x_i2c_write(aw869x, AW869X_REG_TRIM_LRA,
				 (char)f0_cali_lra);
		aw869x_i2c_read(aw869x, AW869X_REG_TRIM_LRA, &reg_val);
		aw_info("%s final trim_lra=0x%02x\n", __func__, reg_val);
	}

	/* restore default work mode */
	aw869x_haptic_play_mode(aw869x, AW869X_HAPTIC_STANDBY_MODE);
	aw869x->play_mode = AW869X_HAPTIC_RAM_MODE;
	aw869x_i2c_write_bits(aw869x, AW869X_REG_SYSCTRL,
			      AW869X_BIT_SYSCTRL_PLAY_MODE_MASK,
			      AW869X_BIT_SYSCTRL_PLAY_MODE_RAM);
	aw869x_haptic_stop(aw869x);

	return ret;
}

/*****************************************************
 *
 * haptic fops
 *
 *****************************************************/
static int aw869x_file_open(struct inode *inode, struct file *file)
{
	if (!try_module_get(THIS_MODULE))
		return -ENODEV;
	aw_info("%s enter\n", __func__);
	file->private_data = (void *)g_aw869x;

	return 0;
}

static int aw869x_file_release(struct inode *inode, struct file *file)
{
	file->private_data = (void *)NULL;
	aw_info("%s enter\n", __func__);
	module_put(THIS_MODULE);

	return 0;
}

static long aw869x_file_unlocked_ioctl(struct file *file, unsigned int cmd,
				       unsigned long arg)
{
	struct aw869x *aw869x = (struct aw869x *)file->private_data;
	int ret = 0;

	aw_info("%s enter\n", __func__);
	aw_info("%s: cmd=0x%x, arg=0x%lx\n", __func__, cmd, arg);

	mutex_lock(&aw869x->lock);

	if (_IOC_TYPE(cmd) != AW869X_HAPTIC_IOCTL_MAGIC) {
		aw_err("%s: cmd magic err\n", __func__);
		return -EINVAL;
	}

	switch (cmd) {
	default:
		aw_err("%s, unknown cmd\n", __func__);
		break;
	}

	mutex_unlock(&aw869x->lock);

	return ret;
}

static ssize_t aw869x_file_read(struct file *filp, char *buff, size_t len,
				loff_t *offset)
{
	struct aw869x *aw869x = (struct aw869x *)filp->private_data;
	int ret = 0;
	int i = 0;
	unsigned char reg_val = 0;
	unsigned char *pbuff = NULL;

	aw_info("%s enter\n", __func__);
	mutex_lock(&aw869x->lock);

	aw_info("%s: len=%zu\n", __func__, len);

	switch (aw869x->fileops.cmd) {
	case AW869X_HAPTIC_CMD_READ_REG:
		pbuff = (unsigned char *)kzalloc(len, GFP_KERNEL);
		if (pbuff != NULL) {
			for (i = 0; i < len; i++) {
				aw869x_i2c_read(aw869x, aw869x->fileops.reg + i,
						&reg_val);
				pbuff[i] = reg_val;
			}
			for (i = 0; i < len; i++) {
				aw_info("%s: pbuff[%d]=0x%02x\n", __func__,
					 i, pbuff[i]);
			}
			ret = copy_to_user(buff, pbuff, len);
			if (ret) {
				aw_err("%s: copy to user fail\n", __func__);
			}
			kfree(pbuff);
		} else {
			aw_err("%s: alloc memory fail\n", __func__);
		}
		break;
	default:
		aw_err("%s, unknown cmd %d\n", __func__, aw869x->fileops.cmd);
		break;
	}

	mutex_unlock(&aw869x->lock);

	return len;
}

static ssize_t aw869x_file_write(struct file *filp, const char *buff,
				 size_t len, loff_t *off)
{
	struct aw869x *aw869x = (struct aw869x *)filp->private_data;
	int i = 0;
	int ret = 0;
	unsigned char *pbuff = NULL;

	aw_info("%s enter\n", __func__);
	pbuff = (unsigned char *)kzalloc(len, GFP_KERNEL);
	if (pbuff == NULL) {
		aw_err("%s: alloc memory fail\n", __func__);
		return len;
	}
	ret = copy_from_user(pbuff, buff, len);
	if (ret) {
		aw_err("%s: copy from user fail\n", __func__);
		return len;
	}

	for (i = 0; i < len; i++) {
		aw_info("%s: pbuff[%d]=0x%02x\n", __func__, i, pbuff[i]);
	}

	mutex_lock(&aw869x->lock);

	aw869x->fileops.cmd = pbuff[0];

	switch (aw869x->fileops.cmd) {
	case AW869X_HAPTIC_CMD_READ_REG:
		if (len == 2) {
			aw869x->fileops.reg = pbuff[1];
		} else {
			aw_err("%s: read cmd len %zu err\n",
				__func__, len);
		}
		break;
	case AW869X_HAPTIC_CMD_WRITE_REG:
		if (len > 2) {
			for (i = 0; i < len - 2; i++) {
				aw_info("%s: write reg0x%02x=0x%02x\n",
					 __func__, pbuff[1] + i, pbuff[i + 2]);
				aw869x_i2c_write(aw869x, pbuff[1] + i,
						 pbuff[2 + i]);
			}
		} else {
			aw_err("%s: write cmd len %zu err\n", __func__, len);
		}
		break;
	default:
		aw_err("%s, unknown cmd %d\n", __func__, aw869x->fileops.cmd);
		break;
	}

	mutex_unlock(&aw869x->lock);

	if (pbuff != NULL)
		kfree(pbuff);

	return len;
}

static const struct file_operations fops = {
	.owner = THIS_MODULE,
	.read = aw869x_file_read,
	.write = aw869x_file_write,
	.unlocked_ioctl = aw869x_file_unlocked_ioctl,
	.open = aw869x_file_open,
	.release = aw869x_file_release,
};

struct miscdevice aw869x_haptic_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = AW_HAPTIC_NAME,
	.fops = &fops,
};


/*****************************************************
 *
 * vibrator
 *
 *****************************************************/
static enum hrtimer_restart qti_hap_stop_timer(struct hrtimer *timer)
{
	struct aw869x *aw869x = container_of(timer, struct aw869x,
					     stop_timer);
	int rc;

	aw_info("%s enter\n", __func__);
	aw869x->play.length_us = 0;
	/* qti_haptics_play(aw869x, false); */
	rc = aw869x_haptic_play_go(aw869x, false);
	if (rc < 0)
		aw_err("Stop playing failed, rc=%d\n", rc);

	return HRTIMER_NORESTART;
}

static enum hrtimer_restart qti_hap_disable_timer(struct hrtimer *timer)
{
	struct aw869x *aw869x = container_of(timer, struct aw869x,
					     hap_disable_timer);
	int rc;

	aw_info("%s enter\n", __func__);
	/* qti_haptics_module_en(aw869x, false); */
	rc = aw869x_haptic_play_go(aw869x, false);
	if (rc < 0)
		aw_err("Disable haptics module failed, rc=%d\n", rc);

	return HRTIMER_NORESTART;
}

static enum hrtimer_restart aw869x_vibrator_timer_func(struct hrtimer *timer)
{
	struct aw869x *aw869x = container_of(timer, struct aw869x, timer);

	aw_info("%s enter\n", __func__);

	aw869x->state = 0;
	/* schedule_work(&aw869x->vibrator_work); */
	queue_work(aw869x->work_queue, &aw869x->vibrator_work);

	return HRTIMER_NORESTART;
}

static void aw869x_vibrator_work_routine(struct work_struct *work)
{
	struct aw869x *aw869x =
	    container_of(work, struct aw869x, vibrator_work);

	aw_dbg("%s enter\n", __func__);
	aw_info("%s: effect_id=%d state=%d activate_mode=%d duration=%d\n",
		__func__, aw869x->effect_id, aw869x->state,
		aw869x->activate_mode, aw869x->duration);
	mutex_lock(&aw869x->lock);
	aw869x_haptic_upload_lra(aw869x, AW869X_F0_CALI_LRA);
	aw869x_haptic_stop(aw869x);
	if (aw869x->state) {
		if (aw869x->activate_mode == AW869X_HAPTIC_ACTIVATE_RAM_MODE) {
			aw869x_haptic_ram_vbat_comp(aw869x, false);
			aw869x_haptic_play_effect_seq(aw869x, true);
		} else if (aw869x->activate_mode ==
			   AW869X_HAPTIC_ACTIVATE_RAM_LOOP_MODE) {
			aw869x->level = 0x80;
			aw869x_haptic_ram_vbat_comp(aw869x, true);
			aw869x_haptic_play_effect_seq(aw869x, true);
			hrtimer_start(&aw869x->timer,
				      ktime_set(aw869x->duration / 1000,
						(aw869x->duration % 1000) *
						1000000), HRTIMER_MODE_REL);
		} else if (aw869x->activate_mode ==
			   AW869X_HAPTIC_ACTIVATE_CONT_MODE) {
			aw869x_haptic_cont(aw869x);
			hrtimer_start(&aw869x->timer,
				      ktime_set(aw869x->duration / 1000,
						(aw869x->duration % 1000) *
						1000000), HRTIMER_MODE_REL);
		} else {
			/*other mode */
		}
	}
	mutex_unlock(&aw869x->lock);
}

/******************************************************
 *
 * irq
 *
 ******************************************************/
static void aw869x_interrupt_clear(struct aw869x *aw869x)
{
	unsigned char reg_val = 0;

	aw869x_i2c_read(aw869x, AW869X_REG_SYSINT, &reg_val);
}

void aw869x_interrupt_setup(struct aw869x *aw869x)
{
	unsigned char reg_val = 0;


	aw869x_i2c_read(aw869x, AW869X_REG_SYSINT, &reg_val);

	/* edge int mode */
	aw869x_i2c_write_bits(aw869x, AW869X_REG_DBGCTRL,
			      AW869X_BIT_DBGCTRL_INT_MODE_MASK,
			      AW869X_BIT_DBGCTRL_INT_MODE_EDGE);

	/* int enable */
	aw869x_i2c_write_bits(aw869x, AW869X_REG_SYSINTM,
			      AW869X_BIT_SYSINTM_BSTERR_MASK,
			      AW869X_BIT_SYSINTM_BSTERR_OFF);
	aw869x_i2c_write_bits(aw869x, AW869X_REG_SYSINTM,
			      AW869X_BIT_SYSINTM_OV_MASK,
			      AW869X_BIT_SYSINTM_OV_EN);
	aw869x_i2c_write_bits(aw869x, AW869X_REG_SYSINTM,
			      AW869X_BIT_SYSINTM_UVLO_MASK,
			      AW869X_BIT_SYSINTM_UVLO_EN);
	aw869x_i2c_write_bits(aw869x, AW869X_REG_SYSINTM,
			      AW869X_BIT_SYSINTM_OCD_MASK,
			      AW869X_BIT_SYSINTM_OCD_EN);
	aw869x_i2c_write_bits(aw869x, AW869X_REG_SYSINTM,
			      AW869X_BIT_SYSINTM_OT_MASK,
			      AW869X_BIT_SYSINTM_OT_EN);
}

irqreturn_t aw869x_irq(int irq, void *data)
{
	struct aw869x *aw869x = data;
	unsigned char reg_val = 0;
	unsigned char dbg_val = 0;
	unsigned int buf_len = 0;
	unsigned int period_size =  aw869x->ram.base_addr >> 2;


	atomic_set(&aw869x->is_in_rtp_loop, 1);
	aw869x_i2c_read(aw869x, AW869X_REG_SYSINT, &reg_val);
	/* aw_info("%s: reg SYSINT=0x%x\n", __func__, reg_val); */
	aw869x_i2c_read(aw869x, AW869X_REG_DBGSTAT, &dbg_val);
	/* aw_info("%s: reg DBGSTAT=0x%x\n", __func__, dbg_val); */

	if (reg_val & AW869X_BIT_SYSINT_OVI)
		aw_err("%s chip ov int error\n", __func__);

	if (reg_val & AW869X_BIT_SYSINT_UVLI)
		aw_err("%s chip uvlo int error\n", __func__);

	if (reg_val & AW869X_BIT_SYSINT_OCDI)
		aw_err("%s chip over current int error\n", __func__);

	if (reg_val & AW869X_BIT_SYSINT_OTI)
		aw_err("%s chip over temperature int error\n", __func__);

	if (reg_val & AW869X_BIT_SYSINT_DONEI)
		aw_info("%s chip playback done\n", __func__);


	if (reg_val & AW869X_BIT_SYSINT_FF_AEI) {
		aw_dbg("%s: aw869x rtp fifo almost empty int\n", __func__);
		if (aw869x->rtp_init) {
			while ((!aw869x_haptic_rtp_get_fifo_afi(aw869x)) &&
			       (aw869x->play_mode == AW869X_HAPTIC_RTP_MODE) &&
			       !atomic_read(&aw869x->exit_in_rtp_loop)) {
				mutex_lock(&aw869x->rtp_lock);
				if (!aw869x_rtp) {
					aw_info("%s:aw869x_rtp is null break\n",
					__func__);
					mutex_unlock(&aw869x->rtp_lock);
					break;
				}

				if (aw869x->is_custom_wave == 1) {
					buf_len = read_rb(aw869x_rtp->data,
							  period_size);
					aw869x_i2c_writes(aw869x,
							  AW869X_REG_RTP_DATA,
							  aw869x_rtp->data,
							  buf_len);
					aw869x->rtp_cnt += buf_len;
					if (buf_len < period_size) {
						aw_info("%s: rtp update complete\n",
							__func__);
						aw869x_haptic_set_rtp_aei(aw869x,
									  false);
						aw869x->rtp_cnt = 0;
						aw869x->rtp_init = 0;
						mutex_unlock(&aw869x->rtp_lock);
						break;
					}
				} else {
					if ((aw869x_rtp->len - aw869x->rtp_cnt) < period_size) {
						buf_len = aw869x_rtp->len -
								aw869x->rtp_cnt;
					} else {
						buf_len = period_size;
					}
					aw869x_i2c_writes(aw869x,
							  AW869X_REG_RTP_DATA,
							  &aw869x_rtp->
							  data[aw869x->rtp_cnt],
							  buf_len);
					aw869x->rtp_cnt += buf_len;
					if (aw869x->rtp_cnt == aw869x_rtp->len) {
						aw_info("%s: rtp update complete\n",
							__func__);
						aw869x_haptic_set_rtp_aei(aw869x, false);
						aw869x->rtp_cnt = 0;
						aw869x->rtp_init = 0;
						mutex_unlock(&aw869x->rtp_lock);
						break;
					}
				}
				mutex_unlock(&aw869x->rtp_lock);
			}
		} else {
			aw_err("%s: aw869x rtp init = %d, init error\n",
				__func__, aw869x->rtp_init);
		}
	}

	if (reg_val & AW869X_BIT_SYSINT_FF_AFI)
		aw_dbg("%s: aw869x rtp mode fifo full empty\n", __func__);


	if (aw869x->play_mode != AW869X_HAPTIC_RTP_MODE ||
	    atomic_read(&aw869x->exit_in_rtp_loop))
		aw869x_haptic_set_rtp_aei(aw869x, false);

	aw869x_i2c_read(aw869x, AW869X_REG_SYSINT, &reg_val);
	aw_dbg("%s: reg SYSINT=0x%x\n", __func__, reg_val);
	aw869x_i2c_read(aw869x, AW869X_REG_SYSST, &reg_val);
	aw_dbg("%s: reg SYSST=0x%x\n", __func__, reg_val);
	atomic_set(&aw869x->is_in_rtp_loop, 0);
	wake_up_interruptible(&aw869x->wait_q);
	aw_dbg("%s exit\n", __func__);
	return IRQ_HANDLED;
}

/*****************************************************
 *
 * device tree
 *
 *****************************************************/
int aw869x_parse_dt(struct device *dev, struct aw869x *aw869x,
			   struct device_node *np)
{
	unsigned int val = 0;
	unsigned int bstdbg[6];
	unsigned int f0_trace_parameter[4];
	unsigned int bemf_config[4];
	unsigned int rtp_time[175];
	unsigned int trig_config[15];
	struct qti_hap_config *config = &aw869x->config;
	struct device_node *child_node;
	struct qti_hap_effect *effect;
	int rc = 0, tmp, i = 0, j;

	val = of_property_read_u32(np, "aw869x_vib_mode", &aw869x->info.mode);
	if (val != 0)
		aw_info("aw869x_vib_mode not found\n");
	val = of_property_read_u32(np, "aw869x_vib_f0_pre",
				   &aw869x->info.f0_pre);
	if (val != 0)
		aw_info("aw869x_vib_f0_pre not found\n");
	val =
	    of_property_read_u32(np, "aw869x_vib_f0_cali_percen",
				 &aw869x->info.f0_cali_percen);
	if (val != 0)
		aw_info("aw869x_vib_f0_cali_percen not found\n");
	val =
	    of_property_read_u32(np, "aw869x_vib_cont_drv_lev",
				 &aw869x->info.cont_drv_lvl);
	if (val != 0)
		aw_info("aw869x_vib_cont_drv_lev not found\n");
	val =
	    of_property_read_u32(np, "aw869x_vib_cont_drv_lvl_ov",
				 &aw869x->info.cont_drv_lvl_ov);
	if (val != 0)
		aw_info("aw869x_vib_cont_drv_lvl_ov not found\n");
	val = of_property_read_u32(np, "aw869x_vib_cont_td",
				   &aw869x->info.cont_td);
	if (val != 0)
		aw_info("aw869x_vib_cont_td not found\n");
	val =
	    of_property_read_u32(np, "aw869x_vib_cont_zc_thr",
				 &aw869x->info.cont_zc_thr);
	if (val != 0)
		aw_info("aw869x_vib_cont_zc_thr not found\n");
	val =
	    of_property_read_u32(np, "aw869x_vib_cont_num_brk",
				 &aw869x->info.cont_num_brk);
	if (val != 0)
		aw_info("aw869x_vib_cont_num_brk not found\n");
	val = of_property_read_u32(np, "aw869x_vib_f0_coeff",
				   &aw869x->info.f0_coeff);
	if (val != 0)
		aw_info("aw869x_vib_f0_coeff not found\n");

	val = of_property_read_u32(np, "aw869x_vib_tset", &aw869x->info.tset);
	if (val != 0)
		aw_info("%s vib_tset not found\n", __func__);
	val = of_property_read_u32(np, "aw869x_vib_r_spare",
				   &aw869x->info.r_spare);
	if (val != 0)
		aw_info("%s vib_r_spare not found\n", __func__);
	val = of_property_read_u32_array(np, "aw869x_vib_bstdbg",
					 bstdbg, ARRAY_SIZE(bstdbg));
	if (val != 0)
		aw_info("%s vib_bstdbg not found\n", __func__);
	memcpy(aw869x->info.bstdbg, bstdbg, sizeof(bstdbg));

	val = of_property_read_u32_array(np, "aw869x_vib_f0_trace_parameter",
					 f0_trace_parameter,
					 ARRAY_SIZE(f0_trace_parameter));
	if (val != 0)
		aw_info("%s vib_f0_trace_parameter not found\n", __func__);
	memcpy(aw869x->info.f0_trace_parameter, f0_trace_parameter,
	       sizeof(f0_trace_parameter));
	val =
	    of_property_read_u32_array(np, "aw869x_vib_bemf_config",
				       bemf_config,
				       ARRAY_SIZE(bemf_config));
	if (val != 0)
		aw_info("%s vib_bemf_config not found\n", __func__);
	memcpy(aw869x->info.bemf_config, bemf_config, sizeof(bemf_config));

	val =
	    of_property_read_u32_array(np, "aw869x_vib_trig_config",
				       trig_config,
				       ARRAY_SIZE(trig_config));
	if (val != 0)
		aw_info("%s vib_trig_config not found\n", __func__);
	memcpy(aw869x->info.trig_config, trig_config, sizeof(trig_config));
	val =
	    of_property_read_u32(np, "aw869x_vib_bst_vol_default",
				 &aw869x->info.bst_vol_default);
	if (val != 0)
		aw_info("%s vib_bst_vol_default not found\n", __func__);
	val =
	    of_property_read_u32(np, "aw869x_vib_bst_vol_ram",
				 &aw869x->info.bst_vol_ram);
	if (val != 0)
		aw_info("%s vib_bst_vol_ram not found\n", __func__);
	val =
	    of_property_read_u32(np, "aw869x_vib_bst_vol_rtp",
				 &aw869x->info.bst_vol_rtp);
	if (val != 0)
		aw_info("%s vib_bst_vol_rtp not found\n", __func__);

	val = of_property_read_u32(np, "vib_effect_id_boundary",
				 &aw869x->info.effect_id_boundary);
	if (val != 0)
		aw_info("%s vib_effect_id_boundary not found\n", __func__);
	val = of_property_read_u32(np, "vib_effect_max",
				 &aw869x->info.effect_max);
	if (val != 0)
		aw_info("%s vib_effect_max not found\n", __func__);
	val = of_property_read_u32_array(np, "vib_rtp_time", rtp_time,
				       ARRAY_SIZE(rtp_time));
	if (val != 0)
		aw_info("%s vib_rtp_time not found\n", __func__);
	memcpy(aw869x->info.rtp_time, rtp_time, sizeof(rtp_time));
	config->play_rate_us = HAP_PLAY_RATE_US_DEFAULT;
	rc = of_property_read_u32(np, "play-rate-us", &tmp);
	if (!rc)
	config->play_rate_us = (tmp >= HAP_PLAY_RATE_US_MAX) ?
		HAP_PLAY_RATE_US_MAX : tmp;

	aw869x->constant.pattern = devm_kcalloc(aw869x->dev,
						HAP_WAVEFORM_BUFFER_MAX,
						sizeof(u8), GFP_KERNEL);
	if (!aw869x->constant.pattern)
		return -ENOMEM;

	tmp = of_get_available_child_count(np);
	aw869x->predefined = devm_kcalloc(aw869x->dev, tmp,
					  sizeof(*aw869x->predefined),
					  GFP_KERNEL);
	if (!aw869x->predefined)
		return -ENOMEM;

	aw869x->effects_count = tmp;
	for_each_available_child_of_node(np, child_node) {
		effect = &aw869x->predefined[i++];
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

		aw_info("%s ---%d effect->vmax_mv =%d\n",
			__func__, __LINE__, effect->vmax_mv);
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
		effect->pattern = devm_kcalloc(aw869x->dev,
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

	aw_info("bst_vol_default: 0x%x\n",
	       aw869x->info.bst_vol_default);
	aw_info("bst_vol_ram: 0x%x\n",
	       aw869x->info.bst_vol_ram);
	aw_info("bst_vol_rtp: 0x%x\n",
	       aw869x->info.bst_vol_rtp);
	return 0;
}

static inline void get_play_length(struct qti_hap_play_info *play,
				   int *length_us)
{
	struct qti_hap_effect *effect = play->effect;
	int tmp;

	/* aw_info("%s  %d enter\n", __func__, __LINE__); */

	tmp = effect->pattern_length * effect->play_rate_us;
	tmp *= wf_s_repeat[effect->wf_s_repeat_n];
	tmp *= wf_repeat[effect->wf_repeat_n];
	if (effect->brake_en)
		tmp += effect->play_rate_us * effect->brake_pattern_length;

	*length_us = tmp;
}

int aw869x_haptics_upload_effect(struct input_dev *dev,
					struct ff_effect *effect,
					struct ff_effect *old)
{
	struct aw869x *aw869x = input_get_drvdata(dev);
	struct qti_hap_play_info *play = &aw869x->play;
	s16 data[CUSTOM_DATA_LEN];
	ktime_t rem;
	s64 time_us;
	int ret;

	/*for osc calibration*/
	if (aw869x->osc_cali_run != 0)
		return 0;

	if (hrtimer_active(&aw869x->timer)) {
		rem = hrtimer_get_remaining(&aw869x->timer);
		time_us = ktime_to_us(rem);
		aw_info("waiting for playing clear sequence: %lld us\n",
			time_us);
		usleep_range(time_us, time_us + 100);
	}
	aw_dbg("%s: effect->type=0x%x,FF_CONSTANT=0x%x,FF_PERIODIC=0x%x\n",
		__func__, effect->type, FF_CONSTANT, FF_PERIODIC);
	aw869x->effect_type = effect->type;
	 mutex_lock(&aw869x->lock);
	 while (atomic_read(&aw869x->exit_in_rtp_loop)) {
		aw_info("%s  goint to waiting rtp  exit\n", __func__);
		mutex_unlock(&aw869x->lock);
		ret = wait_event_interruptible(aw869x->stop_wait_q, atomic_read(&aw869x->exit_in_rtp_loop) == 0);
		aw_info("%s  wakeup\n", __func__);
		if (ret == -ERESTARTSYS) {
			mutex_unlock(&aw869x->lock);
			aw_err("%s wake up by signal return erro\n", __func__);
			return ret;
		 }
		 mutex_lock(&aw869x->lock);
	 }

	if (aw869x->effect_type == FF_CONSTANT) {
		aw_dbg("%s: effect_type is  FF_CONSTANT!\n", __func__);
		/*cont mode set duration */
		aw869x->duration = effect->replay.length;
		aw869x->activate_mode = AW869X_HAPTIC_ACTIVATE_RAM_LOOP_MODE;
		aw869x->effect_id = aw869x->info.effect_id_boundary;

	} else if (aw869x->effect_type == FF_PERIODIC) {
		if (aw869x->effects_count == 0) {
			mutex_unlock(&aw869x->lock);
			return -EINVAL;
		}

		aw_dbg("%s: effect_type is  FF_PERIODIC!\n", __func__);
		if (copy_from_user(data, effect->u.periodic.custom_data,
				   sizeof(s16) * CUSTOM_DATA_LEN)) {
			mutex_unlock(&aw869x->lock);
			return -EFAULT;
		}

		aw869x->effect_id = data[0];
		aw_dbg("%s: aw869x->effect_id =%d\n", __func__,
			 aw869x->effect_id);
		play->vmax_mv = effect->u.periodic.magnitude; /*vmax level*/

		if (aw869x->effect_id < 0 ||
			aw869x->effect_id > aw869x->info.effect_max) {
			mutex_unlock(&aw869x->lock);
			return 0;
		}
		aw869x->is_custom_wave = 0;

		if (aw869x->effect_id < aw869x->info.effect_id_boundary) {
			aw869x->activate_mode = AW869X_HAPTIC_ACTIVATE_RAM_MODE;
			aw_dbg("%s: aw869x->effect_id=%d, aw869x->activate_mode = %d\n",
				 __func__, aw869x->effect_id,
				 aw869x->activate_mode);
			/*second data*/
			data[1] = aw869x->predefined[aw869x->effect_id].play_rate_us/1000000;
			/*millisecond data*/
			data[2] = aw869x->predefined[aw869x->effect_id].play_rate_us/1000;
		}
		if (aw869x->effect_id >= aw869x->info.effect_id_boundary) {
			aw869x->activate_mode = AW869X_HAPTIC_ACTIVATE_RTP_MODE;
			aw_dbg("%s: aw869x->effect_id=%d , aw869x->activate_mode = %d\n",
				 __func__, aw869x->effect_id,
				 aw869x->activate_mode);
			/*second data*/
			data[1] = aw869x->info.rtp_time[aw869x->effect_id]/1000;
			/*millisecond data*/
			data[2] = aw869x->info.rtp_time[aw869x->effect_id];
		}
		if (aw869x->effect_id == CUSTOME_WAVE_ID) {
			aw869x->activate_mode = AW869X_HAPTIC_ACTIVATE_RTP_MODE;
			aw_dbg("%s: aw869x->effect_id=%d , aw869x->activate_mode = %d\n",
				 __func__, aw869x->effect_id,
				 aw869x->activate_mode);
			/*second data*/
			data[1] = aw869x->info.rtp_time[aw869x->effect_id]/1000;
			/*millisecond data*/
			data[2] = aw869x->info.rtp_time[aw869x->effect_id];
			aw869x->is_custom_wave = 1;
			rb_init();
		}


		if (copy_to_user(effect->u.periodic.custom_data, data,
			sizeof(s16) * CUSTOM_DATA_LEN)) {
			mutex_unlock(&aw869x->lock);
			return -EFAULT;
		}

	} else {
		aw_err("%s Unsupported effect type: %d\n", __func__,
		       effect->type);
	}
	mutex_unlock(&aw869x->lock);
	return 0;
}

int aw869x_haptics_playback(struct input_dev *dev, int effect_id,
				   int val)
{
	struct aw869x *aw869x = input_get_drvdata(dev);
	int rc = 0;

	/* aw_info("%s effect_id=%d , val = %d\n", __func__, effect_id, val);
	 * aw_info("%s aw869x->effect_id=%d , aw869x->activate_mode = %d\n",
	 *   __func__, aw869x->effect_id, aw869x->activate_mode);
	 */

	aw_dbg("%s: effect_id=%d , activate_mode = %d val = %d\n",
		__func__, aw869x->effect_id, aw869x->activate_mode, val);
	/*for osc calibration*/
	if (aw869x->osc_cali_run != 0)
		return 0;

	if (val > 0)
		aw869x->state = 1;
	if (val <= 0)
		aw869x->state = 0;
	hrtimer_cancel(&aw869x->timer);

	if (aw869x->effect_type == FF_CONSTANT &&
		aw869x->activate_mode == AW869X_HAPTIC_ACTIVATE_RAM_LOOP_MODE) {
		aw_dbg("%s: enter cont_mode\n", __func__);
		/* schedule_work(&aw869x->vibrator_work); */
		queue_work(aw869x->work_queue, &aw869x->vibrator_work);
	} else if (aw869x->effect_type == FF_PERIODIC &&
		aw869x->activate_mode == AW869X_HAPTIC_ACTIVATE_RAM_MODE) {
		aw_dbg("%s: enter  ram_mode\n", __func__);
		/* schedule_work(&aw869x->vibrator_work) */
		queue_work(aw869x->work_queue, &aw869x->vibrator_work);
	} else if ((aw869x->effect_type == FF_PERIODIC) &&
		aw869x->activate_mode == AW869X_HAPTIC_ACTIVATE_RTP_MODE) {
		aw_dbg("%s: enter  rtp_mode\n", __func__);
		/* schedule_work(&aw869x->rtp_work); */
		queue_work(aw869x->work_queue, &aw869x->rtp_work);
		/* if we are in the play mode, force to exit */
		if (val == 0) {
			atomic_set(&aw869x->exit_in_rtp_loop, 1);
			rb_force_exit();
			wake_up_interruptible(&aw869x->stop_wait_q);
		}
	} else {
		/*other mode */
	}

	return rc;
}

int aw869x_haptics_erase(struct input_dev *dev, int effect_id)
{
	struct aw869x *aw869x = input_get_drvdata(dev);
	int rc = 0;

	/*for osc calibration*/
	if (aw869x->osc_cali_run != 0)
		return 0;

	aw_dbg("%s: enter\n", __func__);
	aw869x->effect_type = 0;
	aw869x->is_custom_wave = 0;
	aw869x->duration = 0;
	return rc;
}

void aw869x_haptics_set_gain_work_routine(struct work_struct *work)
{
	unsigned char comp_level = 0;
	struct aw869x *aw869x =
	    container_of(work, struct aw869x, set_gain_work);

	if (aw869x->new_gain >= 0x7FFF)
		aw869x->level = 0x80;	/*128 */
	else if (aw869x->new_gain <= 0x3FFF)
		aw869x->level = 0x1E;	/*30 */
	else
		aw869x->level = (aw869x->new_gain - 16383) / 128;

	if (aw869x->level < 0x1E)
		aw869x->level = 0x1E;	/*30 */
	aw_info("%s: set_gain queue work, new_gain = %x level = %x\n",
		__func__, aw869x->new_gain, aw869x->level);

	if (aw869x->ram_vbat_comp == AW869X_HAPTIC_RAM_VBAT_COMP_ENABLE
		&& aw869x->vbat) {
		aw_dbg("%s: ref %d vbat %d ", __func__, AW_VBAT_REFER,
				aw869x->vbat);
		comp_level = aw869x->level * AW_VBAT_REFER / aw869x->vbat;
		if (comp_level > (128 * AW_VBAT_REFER / AW_VBAT_MIN)) {
			comp_level = 128 * AW_VBAT_REFER / AW_VBAT_MIN;
			aw_dbg("%s: comp level limit is %d ", __func__,
				 comp_level);
		}
		aw_info("%s: enable vbat comp, level = %x comp level = %x",
			__func__, aw869x->level, comp_level);
		aw869x_i2c_write(aw869x, AW869X_REG_DATDBG, comp_level);
	} else {
		aw_dbg("%s: disable compsensation, vbat=%d, vbat_min=%d, vbat_ref=%d",
			 __func__, aw869x->vbat, AW_VBAT_MIN,
			 AW_VBAT_REFER);
		aw869x_i2c_write(aw869x, AW869X_REG_DATDBG, aw869x->level);
	}
}

void aw869x_haptics_set_gain(struct input_dev *dev, u16 gain)
{
	struct aw869x *aw869x = input_get_drvdata(dev);

	aw_dbg("%s enter\n", __func__);
	aw869x->new_gain = gain;
	queue_work(aw869x->work_queue, &aw869x->set_gain_work);
}

static ssize_t aw869x_activate_test_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;

	/* For now nothing to show */
	return snprintf(buf, PAGE_SIZE, "%d\n", aw869x->test_val);
}

static ssize_t aw869x_activate_test_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;

	aw869x->test_val = val;
	aw_dbg("%s: aw869x->test_val=%d\n", __func__, aw869x->test_val);

	if (aw869x->test_val == 1) {
		aw_info("%s  %d\n", __func__, __LINE__);
		aw869x->duration = 3000;

		aw869x->state = 1;
		aw869x->activate_mode = AW869X_HAPTIC_ACTIVATE_CONT_MODE;
		hrtimer_cancel(&aw869x->timer);
		/* schedule_work(&aw869x->vibrator_work); */
		queue_work(aw869x->work_queue, &aw869x->vibrator_work);
	}
	if (aw869x->test_val == 2) {
		aw_info("%s  %d\n", __func__, __LINE__);
		mutex_lock(&aw869x->lock);
		aw869x_haptic_set_wav_seq(aw869x, 0x00, 0x01);
		aw869x_haptic_set_wav_seq(aw869x, 0x01, 0x01);

		/*step 1:  choose  loop */
		aw869x_haptic_set_wav_loop(aw869x, 0x01, 0x01);
		mutex_unlock(&aw869x->lock);

		aw869x->state = 1;
		aw869x->activate_mode = AW869X_HAPTIC_ACTIVATE_RAM_MODE;
		hrtimer_cancel(&aw869x->timer);
		/* schedule_work(&aw869x->vibrator_work); */
		queue_work(aw869x->work_queue, &aw869x->vibrator_work);
	}

	if (aw869x->test_val == 3) {   /*Ram instead of Cont */
		aw869x->duration = 10000;

		aw869x->state = 1;
		aw869x->activate_mode = AW869X_HAPTIC_ACTIVATE_CONT_MODE;
		hrtimer_cancel(&aw869x->timer);
		/* schedule_work(&aw869x->vibrator_work); */
		queue_work(aw869x->work_queue, &aw869x->vibrator_work);
	}

	if (aw869x->test_val == 4) {
		mutex_lock(&aw869x->lock);
		aw869x_haptic_stop(aw869x);
		aw869x_haptic_play_mode(aw869x, AW869X_HAPTIC_RAM_MODE);

		aw869x_haptic_set_wav_seq(aw869x, 0x00, 0x01);
		aw869x_haptic_set_wav_seq(aw869x, 0x01, 0x00);

		aw869x_haptic_set_wav_loop(aw869x, 0x01, 0x01);

		if (aw869x->info.bst_vol_ram <= AW869X_MAX_BST_VO)
			aw869x_haptic_set_bst_vol(aw869x,
						  aw869x->info.bst_vol_ram);
		else
			aw869x_haptic_set_bst_vol(aw869x, aw869x->vmax);

		aw869x->activate_mode = AW869X_HAPTIC_ACTIVATE_RAM_MODE;
		aw869x->state = 1;
		mutex_unlock(&aw869x->lock);
		hrtimer_cancel(&aw869x->timer);
		/* schedule_work(&aw869x->vibrator_work); */
		queue_work(aw869x->work_queue, &aw869x->vibrator_work);
	}

	return count;
}
/******************************************************
 *
 * sys group attribute: reg
 *
 ******************************************************/
static ssize_t aw869x_i2c_reg_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	unsigned int databuf[2] = { 0, 0 };

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		aw869x_i2c_write(aw869x, (unsigned char)databuf[0],
				 (unsigned char)databuf[1]);
	}

	return count;
}

static ssize_t aw869x_i2c_reg_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	ssize_t len = 0;
	unsigned char i = 0;
	unsigned char reg_val = 0;

	for (i = 0; i < AW869X_REG_MAX; i++) {
		if (!(aw869x_reg_access[i] & REG_RD_ACCESS))
			continue;
		aw869x_i2c_read(aw869x, i, &reg_val);
		len +=
		    snprintf(buf + len, PAGE_SIZE - len, "reg:0x%02x=0x%02x\n",
			     i, reg_val);
	}
	return len;
}

static ssize_t aw869x_i2c_ram_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	unsigned int val = 0;
	int rc = 0;


	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;
	if (val == 1)
		aw869x_ram_update(aw869x);
	return count;
}

static ssize_t aw869x_i2c_ram_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	ssize_t len = 0;
	int size = 0;
	int i = 0;
	int j = 0;
	unsigned char ram_data[AW_RAMDATA_RD_BUFFER_SIZE] = {0};

	aw869x_haptic_stop(aw869x);
	/* RAMINIT Enable */
	aw869x_i2c_write_bits(aw869x, AW869X_REG_SYSCTRL,
			      AW869X_BIT_SYSCTRL_RAMINIT_MASK,
			      AW869X_BIT_SYSCTRL_RAMINIT_EN);

	aw869x_i2c_write(aw869x, AW869X_REG_RAMADDRH,
			 (unsigned char)(aw869x->ram.base_addr >> 8));
	aw869x_i2c_write(aw869x, AW869X_REG_RAMADDRL,
			 (unsigned char)(aw869x->ram.base_addr & 0x00ff));
	len += snprintf(buf + len, PAGE_SIZE - len, "aw869x_haptic_ram:\n");
	while (i < aw869x->ram.len) {
		if ((aw869x->ram.len - i) < AW_RAMDATA_RD_BUFFER_SIZE)
			size = aw869x->ram.len - i;
		else
			size = AW_RAMDATA_RD_BUFFER_SIZE;

		aw869x_i2c_reads(aw869x, AW869X_REG_RAMDATA, ram_data, size);
		for (j = 0; j < size; j++) {
			len += snprintf(buf + len, PAGE_SIZE - len,
					"0x%02X,", ram_data[j]);
		}
		i += size;
	}
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	/* RAMINIT Disable */
	aw869x_i2c_write_bits(aw869x, AW869X_REG_SYSCTRL,
			      AW869X_BIT_SYSCTRL_RAMINIT_MASK,
			      AW869X_BIT_SYSCTRL_RAMINIT_OFF);

	return len;
}

static ssize_t aw869x_duration_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	ktime_t time_rem;
	s64 time_ms = 0;

	if (hrtimer_active(&aw869x->timer)) {
		time_rem = hrtimer_get_remaining(&aw869x->timer);
		time_ms = ktime_to_ms(time_rem);
	}

	return snprintf(buf, PAGE_SIZE, "%lld\n", time_ms);
}

static ssize_t aw869x_duration_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;

	/* setting 0 on duration is NOP for now */
	if (val <= 0)
		return count;

	aw869x->duration = val;

	return count;
}

static ssize_t aw869x_activate_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;

	/* For now nothing to show */
	return snprintf(buf, PAGE_SIZE, "%d\n", aw869x->state);
}

static ssize_t aw869x_activate_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;

	if (val != 0 && val != 1)
		return count;

	aw_dbg("%s: value=%d\n", __func__, val);

	mutex_lock(&aw869x->lock);
	hrtimer_cancel(&aw869x->timer);

	aw869x->state = val;

	mutex_unlock(&aw869x->lock);
	/* schedule_work(&aw869x->vibrator_work); */
	queue_work(aw869x->work_queue, &aw869x->vibrator_work);

	return count;
}

static ssize_t aw869x_activate_mode_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;

	return snprintf(buf, PAGE_SIZE, "activate_mode=%d\n",
			aw869x->activate_mode);
}

static ssize_t aw869x_activate_mode_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;

	mutex_lock(&aw869x->lock);
	aw869x->activate_mode = val;
	mutex_unlock(&aw869x->lock);
	return count;
}

static ssize_t aw869x_index_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	unsigned char reg_val = 0;

	aw869x_i2c_read(aw869x, AW869X_REG_WAVSEQ1, &reg_val);
	aw869x->index = reg_val;

	return snprintf(buf, PAGE_SIZE, "%d\n", aw869x->index);
}

static ssize_t aw869x_index_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;

	aw_dbg("%s: value=%d\n", __func__, val);

	mutex_lock(&aw869x->lock);
	aw869x->index = val;
	aw869x_haptic_set_repeat_wav_seq(aw869x, aw869x->index);
	mutex_unlock(&aw869x->lock);
	return count;
}

static ssize_t aw869x_vmax_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;

	return snprintf(buf, PAGE_SIZE, "0x%02x\n", aw869x->vmax);
}

static ssize_t aw869x_vmax_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;

	aw_dbg("%s: value=%d\n", __func__, val);

	mutex_lock(&aw869x->lock);
	aw869x->vmax = val;
	aw869x_haptic_set_bst_vol(aw869x, aw869x->vmax);
	mutex_unlock(&aw869x->lock);
	return count;
}

static ssize_t aw869x_gain_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;

	return snprintf(buf, PAGE_SIZE, "0x%02x\n", aw869x->level);
}

static ssize_t aw869x_gain_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;

	aw_dbg("%s: value=%d\n", __func__, val);

	mutex_lock(&aw869x->lock);
	aw869x->level = val;
	aw869x_haptic_set_gain(aw869x, aw869x->level);
	mutex_unlock(&aw869x->lock);
	return count;
}

static ssize_t aw869x_seq_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	size_t count = 0;
	unsigned char i = 0;
	unsigned char reg_val = 0;

	for (i = 0; i < AW869X_SEQUENCER_SIZE; i++) {
		aw869x_i2c_read(aw869x, AW869X_REG_WAVSEQ1 + i, &reg_val);
		count += snprintf(buf + count, PAGE_SIZE - count,
				  "seq%d: 0x%02x\n", i + 1, reg_val);
		aw869x->seq[i] |= reg_val;
	}
	return count;
}

static ssize_t aw869x_seq_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	unsigned int databuf[2] = { 0, 0 };

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		aw_dbg("%s: seq%d=0x%x\n", __func__, databuf[0],
			 databuf[1]);
		mutex_lock(&aw869x->lock);
		aw869x->seq[databuf[0]] = (unsigned char)databuf[1];
		aw869x_haptic_set_wav_seq(aw869x, (unsigned char)databuf[0],
					  aw869x->seq[databuf[0]]);
		mutex_unlock(&aw869x->lock);
	}
	return count;
}

static ssize_t aw869x_loop_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	size_t count = 0;
	unsigned char i = 0;
	unsigned char reg_val = 0;

	for (i = 0; i < AW869X_SEQUENCER_LOOP_SIZE; i++) {
		aw869x_i2c_read(aw869x, AW869X_REG_WAVLOOP1 + i, &reg_val);
		aw869x->loop[i * 2 + 0] = (reg_val >> 4) & 0x0F;
		aw869x->loop[i * 2 + 1] = (reg_val >> 0) & 0x0F;

		count += snprintf(buf + count, PAGE_SIZE - count,
				  "seq%d loop: 0x%02x\n", i * 2 + 1,
				  aw869x->loop[i * 2 + 0]);
		count +=
		    snprintf(buf + count, PAGE_SIZE - count,
			     "seq%d loop: 0x%02x\n", i * 2 + 2,
			     aw869x->loop[i * 2 + 1]);
	}
	return count;
}

static ssize_t aw869x_loop_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	unsigned int databuf[2] = { 0, 0 };

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		aw_dbg("%s: seq%d loop=0x%x\n", __func__, databuf[0],
			 databuf[1]);
		mutex_lock(&aw869x->lock);
		aw869x->loop[databuf[0]] = (unsigned char)databuf[1];
		aw869x_haptic_set_wav_loop(aw869x, (unsigned char)databuf[0],
					   aw869x->loop[databuf[0]]);
		mutex_unlock(&aw869x->lock);
	}

	return count;
}

static ssize_t aw869x_rtp_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	ssize_t len = 0;

	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "rtp play: %d\n",
		     aw869x->rtp_cnt);

	return len;
}

static ssize_t aw869x_rtp_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;

	aw869x_haptic_stop(aw869x);
	aw869x_haptic_set_rtp_aei(aw869x, false);
	aw869x_interrupt_clear(aw869x);
	if (val < (awinic_rtp_name_len)) {
		aw869x->rtp_file_num = val;
		if (val) {
			/* schedule_work(&aw869x->rtp_work); */
			queue_work(aw869x->work_queue, &aw869x->rtp_work);
		}
	} else {
		aw_err("%s: rtp_file_num 0x%02x over max value\n", __func__,
		       aw869x->rtp_file_num);
	}

	return count;
}

static ssize_t aw869x_ram_update_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "sram update mode\n");
	return len;
}

static ssize_t aw869x_ram_update_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;

	if (val)
		aw869x_ram_update(aw869x);

	return count;
}

static ssize_t aw869x_f0_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	ssize_t len = 0;

	mutex_lock(&aw869x->lock);
	aw869x_haptic_upload_lra(aw869x, AW869X_WRITE_ZERO);
	aw869x_haptic_get_f0(aw869x);
	mutex_unlock(&aw869x->lock);
	len +=
	    /* snprintf(buf + len, PAGE_SIZE - len, "aw869x lra f0 = %d\n", */
	    snprintf(buf + len, PAGE_SIZE - len, "%d\n", aw869x->f0);
	return len;
}

static ssize_t aw869x_f0_store(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;

	return count;
}

static ssize_t aw869x_cali_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	ssize_t len = 0;

	mutex_lock(&aw869x->lock);
	aw869x_haptic_upload_lra(aw869x, AW869X_F0_CALI_LRA);
	aw869x_haptic_get_f0(aw869x);
	mutex_unlock(&aw869x->lock);
	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "aw869x cali f0 = %d\n",
		     aw869x->f0);
	return len;
}

static ssize_t aw869x_cali_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;

	if (val) {
		mutex_lock(&aw869x->lock);
		aw869x_haptic_upload_lra(aw869x, AW869X_WRITE_ZERO);
		aw869x_haptic_f0_calibration(aw869x);
		mutex_unlock(&aw869x->lock);
	}
	return count;
}

static ssize_t aw869x_cont_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	ssize_t len = 0;

	aw869x_haptic_read_cont_f0(aw869x);
	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "aw869x cont f0 = %d\n",
		     aw869x->cont_f0);
	return len;
}

static ssize_t aw869x_cont_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;

	aw869x_haptic_stop(aw869x);
	if (val)
		aw869x_haptic_cont(aw869x);

	return count;
}

static ssize_t aw869x_cont_td_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	ssize_t len = 0;

	len +=
	    snprintf(buf + len, PAGE_SIZE - len,
		     "aw869x cont delay time = 0x%04x\n", aw869x->info.cont_td);
	return len;
}

static ssize_t aw869x_cont_td_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;

	aw869x->info.cont_td = val;
	aw869x_i2c_write(aw869x, AW869X_REG_TD_H,
			 (unsigned char)(val >> 8));
	aw869x_i2c_write(aw869x, AW869X_REG_TD_L,
			 (unsigned char)(val >> 0));
	return count;
}

static ssize_t aw869x_cont_drv_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	ssize_t len = 0;

	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "aw869x cont drv level = %d\n",
		     aw869x->info.cont_drv_lvl);
	len +=
	    snprintf(buf + len, PAGE_SIZE - len,
		     "aw869x cont drv level overdrive= %d\n",
		     aw869x->info.cont_drv_lvl_ov);
	return len;
}

static ssize_t aw869x_cont_drv_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	unsigned int databuf[2] = { 0, 0 };

	if (sscanf(buf, "%d %d", &databuf[0], &databuf[1]) == 2) {
		aw869x->info.cont_drv_lvl = databuf[0];
		aw869x_i2c_write(aw869x, AW869X_REG_DRV_LVL,
				 aw869x->info.cont_drv_lvl);
		aw869x->info.cont_drv_lvl_ov = databuf[1];
		aw869x_i2c_write(aw869x, AW869X_REG_DRV_LVL_OV,
				 aw869x->info.cont_drv_lvl_ov);
	}
	return count;
}

/* return buffer size and availbe size */
static ssize_t aw869x_custom_wave_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	ssize_t len = 0;

	len +=
		snprintf(buf + len, PAGE_SIZE - len, "period_size=%d;",
		aw869x->ram.base_addr >> 2);
	len +=
		snprintf(buf + len, PAGE_SIZE - len,
		"max_size=%d;free_size=%d;",
		get_rb_max_size(), get_rb_free_size());
	len +=
		snprintf(buf + len, PAGE_SIZE - len,
		"custom_wave_id=%d;", CUSTOME_WAVE_ID);
	return len;
}

static ssize_t aw869x_custom_wave_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	unsigned long  buf_len, period_size, offset;
	int ret;

	period_size = (aw869x->ram.base_addr >> 2);
	offset = 0;

	aw_dbg("%swrite szie %zd, period size %lu", __func__, count,
		 period_size);
	if (count % period_size || count < period_size)
		rb_end();
	atomic_set(&aw869x->is_in_write_loop, 1);

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
	atomic_set(&aw869x->is_in_write_loop, 0);
	wake_up_interruptible(&aw869x->stop_wait_q);
	aw_dbg(" return size %d", ret);
	return ret;
}

static ssize_t aw869x_cont_num_brk_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	ssize_t len = 0;

	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "aw869x cont break num = %d\n",
		     aw869x->info.cont_num_brk);
	return len;
}

static ssize_t aw869x_cont_num_brk_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;

	aw869x->info.cont_num_brk = val;
	if (aw869x->info.cont_num_brk > 7)
		aw869x->info.cont_num_brk = 7;

	aw869x_i2c_write_bits(aw869x, AW869X_REG_BEMF_NUM,
			      AW869X_BIT_BEMF_NUM_BRK_MASK,
			      aw869x->info.cont_num_brk);
	return count;
}

static ssize_t aw869x_cont_zc_thr_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	ssize_t len = 0;

	len +=
	    snprintf(buf + len, PAGE_SIZE - len,
		     "aw869x cont zero cross thr = 0x%04x\n",
		     aw869x->info.cont_zc_thr);
	return len;
}

static ssize_t aw869x_cont_zc_thr_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;
	aw869x->info.cont_zc_thr = val;
		aw869x_i2c_write(aw869x, AW869X_REG_ZC_THRSH_H,
				 (unsigned char)(val >> 8));
		aw869x_i2c_write(aw869x, AW869X_REG_ZC_THRSH_L,
				 (unsigned char)(val >> 0));
	return count;
}

static ssize_t aw869x_vbat_monitor_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	ssize_t len = 0;

	mutex_lock(&aw869x->lock);
	aw869x_haptic_stop(aw869x);
	aw869x_haptic_get_vbat(aw869x);
	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "vbat=%dmV\n", aw869x->vbat);
	mutex_unlock(&aw869x->lock);

	return len;
}

static ssize_t aw869x_vbat_monitor_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	return count;
}

static ssize_t aw869x_lra_resistance_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	ssize_t len = 0;
	unsigned char reg_val = 0;

	mutex_lock(&aw869x->lock);
	aw869x_haptic_stop(aw869x);
	aw869x_i2c_write_bits(aw869x, AW869X_REG_SYSCTRL,
			      AW869X_BIT_SYSCTRL_RAMINIT_MASK,
			      AW869X_BIT_SYSCTRL_RAMINIT_EN);
	aw869x_i2c_write_bits(aw869x, AW869X_REG_SYSCTRL,
			      AW869X_BIT_SYSCTRL_BST_MODE_MASK,
			      AW869X_BIT_SYSCTRL_BST_MODE_BYPASS);

	aw869x_i2c_write_bits(aw869x, AW869X_REG_ANACTRL,
			      AW869X_BIT_ANACTRL_HD_PD_MASK,
			      AW869X_BIT_ANACTRL_HD_HZ_EN);
	aw869x_i2c_write_bits(aw869x, AW869X_REG_D2SCFG,
			      AW869X_BIT_D2SCFG_CLK_ADC_MASK,
			      AW869X_BIT_D2SCFG_CLK_ASC_1P5MHZ);

	aw869x_i2c_write_bits(aw869x, AW869X_REG_DETCTRL,
			      AW869X_BIT_DETCTRL_RL_OS_MASK,
			      AW869X_BIT_DETCTRL_RL_DETECT);
	aw869x_i2c_write_bits(aw869x, AW869X_REG_DETCTRL,
			      AW869X_BIT_DETCTRL_DIAG_GO_MASK,
			      AW869X_BIT_DETCTRL_DIAG_GO_ENABLE);
	usleep_range(3000, 3500);
	aw869x_i2c_read(aw869x, AW869X_REG_RLDET, &reg_val);
	aw869x->lra = 298 * reg_val;
	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "%d\n", aw869x->lra);
	    /* snprintf(buf + len, PAGE_SIZE - len, "r_lra=%dmohm\n",
	     * aw869x->lra);
	     */

	aw869x_i2c_write_bits(aw869x, AW869X_REG_ANACTRL,
			      AW869X_BIT_ANACTRL_HD_PD_MASK,
			      AW869X_BIT_ANACTRL_HD_PD_EN);
	aw869x_i2c_write_bits(aw869x, AW869X_REG_D2SCFG,
			      AW869X_BIT_D2SCFG_CLK_ADC_MASK,
			      AW869X_BIT_D2SCFG_CLK_ASC_6MHZ);

	aw869x_i2c_write_bits(aw869x, AW869X_REG_SYSCTRL,
			      AW869X_BIT_SYSCTRL_RAMINIT_MASK,
			      AW869X_BIT_SYSCTRL_RAMINIT_OFF);
	aw869x_i2c_write_bits(aw869x, AW869X_REG_SYSCTRL,
			      AW869X_BIT_SYSCTRL_BST_MODE_MASK,
			      AW869X_BIT_SYSCTRL_BST_MODE_BOOST);
	mutex_unlock(&aw869x->lock);

	return len;
}

static ssize_t aw869x_lra_resistance_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	return count;
}

static ssize_t aw869x_auto_boost_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	ssize_t len = 0;

	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "auto_boost=%d\n",
		     aw869x->auto_boost);

	return len;
}

static ssize_t aw869x_auto_boost_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;

	mutex_lock(&aw869x->lock);
	aw869x_haptic_stop(aw869x);
	aw869x_haptic_auto_boost_config(aw869x, val);
	mutex_unlock(&aw869x->lock);

	return count;
}

static ssize_t aw869x_prctmode_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	ssize_t len = 0;
	unsigned char reg_val = 0;

	aw869x_i2c_read(aw869x, AW869X_REG_RLDET, &reg_val);

	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "prctmode=%d\n",
		     reg_val & 0x20);
	return len;
}

static ssize_t aw869x_prctmode_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	unsigned int databuf[2] = { 0, 0 };
	unsigned int addr = 0;
	unsigned int val = 0;

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		addr = databuf[0];
		val = databuf[1];
		mutex_lock(&aw869x->lock);
		aw869x_haptic_swicth_motorprotect_config(aw869x, addr, val);
		mutex_unlock(&aw869x->lock);
	}
	return count;
}

static ssize_t aw869x_trig_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	ssize_t len = 0;
	unsigned char i = 0;

	for (i = 0; i < AW869X_TRIG_NUM; i++) {
		len += snprintf(buf + len, PAGE_SIZE - len,
				"trig%d: enable=%d, default_level=%d, dual_edge=%d, frist_seq=%d, second_seq=%d\n",
				i + 1, aw869x->trig[i].enable,
				aw869x->trig[i].default_level,
				aw869x->trig[i].dual_edge,
				aw869x->trig[i].frist_seq,
				aw869x->trig[i].second_seq);
	}

	return len;
}

static ssize_t aw869x_trig_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	unsigned int databuf[6] = { 0 };

	if (sscanf(buf, "%d %d %d %d %d %d",
		   &databuf[0], &databuf[1], &databuf[2], &databuf[3],
		   &databuf[4], &databuf[5]) == 6) {
		aw_dbg("%s: %d, %d, %d, %d, %d, %d\n", __func__, databuf[0],
			 databuf[1], databuf[2], databuf[3], databuf[4],
			 databuf[5]);
		if (databuf[0] > 3)
			databuf[0] = 3;

		if (databuf[0] > 0)
			databuf[0] -= 1;

		aw869x->trig[databuf[0]].enable = databuf[1];
		aw869x->trig[databuf[0]].default_level = databuf[2];
		aw869x->trig[databuf[0]].dual_edge = databuf[3];
		aw869x->trig[databuf[0]].frist_seq = databuf[4];
		aw869x->trig[databuf[0]].second_seq = databuf[5];
		mutex_lock(&aw869x->lock);
		aw869x_haptic_trig_param_config(aw869x);
		aw869x_haptic_trig_enable_config(aw869x);
		mutex_unlock(&aw869x->lock);
	}
	return count;
}

static ssize_t aw869x_ram_vbat_comp_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	ssize_t len = 0;

	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "ram_vbat_comp=%d\n",
		     aw869x->ram_vbat_comp);

	return len;
}

static ssize_t aw869x_ram_vbat_comp_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;

	mutex_lock(&aw869x->lock);
	if (val)
		aw869x->ram_vbat_comp = AW869X_HAPTIC_RAM_VBAT_COMP_ENABLE;
	else
		aw869x->ram_vbat_comp = AW869X_HAPTIC_RAM_VBAT_COMP_DISABLE;
	mutex_unlock(&aw869x->lock);

	return count;
}

static ssize_t aw869x_osc_cali_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	ssize_t len = 0;

	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "%d\n",
		     aw869x->lra_calib_data);

	return len;
}

static ssize_t aw869x_osc_cali_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	unsigned int val = 0;

	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;

	mutex_lock(&aw869x->lock);
	/*osc calibration flag start,Other behaviors are forbidden*/
	aw869x->osc_cali_run = 1;
	aw869x_haptic_upload_lra(aw869x, AW869X_WRITE_ZERO);
	if (val == 3) {
		aw869x_rtp_osc_calibration(aw869x);
		aw869x_rtp_trim_lra_calibration(aw869x);
	}
	if (val == 1)
		aw869x_rtp_osc_calibration(aw869x);

	aw869x->osc_cali_run = 0;
	/*osc calibration flag end,Other behaviors are permitted*/
	mutex_unlock(&aw869x->lock);

	return count;
}

static ssize_t aw869x_osc_save_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	unsigned int val = 0;
	int rc = 0;

	aw_info("%s enter\n", __func__);
	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;
	aw869x->lra_calib_data = val;
	aw_info("%s load osa cal: %d\n", __func__, val);

	return count;
}

static ssize_t aw869x_f0_save_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	ssize_t len = 0;

	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "%d\n",
		     aw869x->f0_calib_data);

	return len;
}

static ssize_t aw869x_f0_save_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	unsigned int val = 0;
	int rc = 0;

	aw_info("%s enter\n", __func__);
	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;
	aw869x->f0_calib_data = val;
	aw_info("%s load f0 cal: %d\n", __func__, val);

	return count;
}

static ssize_t aw869x_f0_value_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;

	return snprintf(buf, PAGE_SIZE, "%d\n", aw869x->f0);
}

static ssize_t aw869x_f0_check_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	ssize_t len = 0;

	if (aw869x->f0_cali_status == true)
		len += snprintf(buf + len, PAGE_SIZE - len, "%d\n", 1);
	if (aw869x->f0_cali_status == false)
		len += snprintf(buf + len, PAGE_SIZE - len, "%d\n", 0);

	return len;
}
static ssize_t aw869x_effect_id_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;

	return snprintf(buf, PAGE_SIZE, "effect_id =%d\n", aw869x->effect_id);
}

static ssize_t aw869x_effect_id_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct awinic *awinic = dev_get_drvdata(dev);
	struct aw869x *aw869x = awinic->aw869x;
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;

	mutex_lock(&aw869x->lock);
	aw869x->effect_id = val;
	aw869x->play.vmax_mv = AW869X_MEDIUM_MAGNITUDE;
	mutex_unlock(&aw869x->lock);
	return count;
}

static DEVICE_ATTR(effect_id, S_IWUSR | S_IRUGO, aw869x_effect_id_show,
		   aw869x_effect_id_store);
static DEVICE_ATTR(activate_test, S_IWUSR | S_IRUGO, aw869x_activate_test_show,
		   aw869x_activate_test_store);

static DEVICE_ATTR(reg, S_IWUSR | S_IRUGO, aw869x_i2c_reg_show,
		   aw869x_i2c_reg_store);
static DEVICE_ATTR(ram, S_IWUSR | S_IRUGO, aw869x_i2c_ram_show,
		   aw869x_i2c_ram_store);
static DEVICE_ATTR(duration, S_IWUSR | S_IRUGO, aw869x_duration_show,
		   aw869x_duration_store);
static DEVICE_ATTR(activate, S_IWUSR | S_IRUGO, aw869x_activate_show,
		   aw869x_activate_store);
static DEVICE_ATTR(activate_mode, S_IWUSR | S_IRUGO, aw869x_activate_mode_show,
		   aw869x_activate_mode_store);
static DEVICE_ATTR(index, S_IWUSR | S_IRUGO, aw869x_index_show,
		   aw869x_index_store);
static DEVICE_ATTR(vmax, S_IWUSR | S_IRUGO, aw869x_vmax_show,
		   aw869x_vmax_store);
static DEVICE_ATTR(gain, S_IWUSR | S_IRUGO, aw869x_gain_show,
		   aw869x_gain_store);
static DEVICE_ATTR(seq, S_IWUSR | S_IRUGO, aw869x_seq_show, aw869x_seq_store);
static DEVICE_ATTR(loop, S_IWUSR | S_IRUGO, aw869x_loop_show,
		   aw869x_loop_store);
static DEVICE_ATTR(rtp, S_IWUSR | S_IRUGO, aw869x_rtp_show, aw869x_rtp_store);
static DEVICE_ATTR(ram_update, S_IWUSR | S_IRUGO, aw869x_ram_update_show,
		   aw869x_ram_update_store);
static DEVICE_ATTR(f0, S_IWUSR | S_IRUGO, aw869x_f0_show, aw869x_f0_store);
static DEVICE_ATTR(cali, S_IWUSR | S_IRUGO, aw869x_cali_show,
		   aw869x_cali_store);
static DEVICE_ATTR(cont, S_IWUSR | S_IRUGO, aw869x_cont_show,
		   aw869x_cont_store);
static DEVICE_ATTR(cont_td, S_IWUSR | S_IRUGO, aw869x_cont_td_show,
		   aw869x_cont_td_store);
static DEVICE_ATTR(cont_drv, S_IWUSR | S_IRUGO, aw869x_cont_drv_show,
		   aw869x_cont_drv_store);
static DEVICE_ATTR(cont_num_brk, S_IWUSR | S_IRUGO, aw869x_cont_num_brk_show,
		   aw869x_cont_num_brk_store);
static DEVICE_ATTR(cont_zc_thr, S_IWUSR | S_IRUGO, aw869x_cont_zc_thr_show,
		   aw869x_cont_zc_thr_store);
static DEVICE_ATTR(vbat_monitor, S_IWUSR | S_IRUGO, aw869x_vbat_monitor_show,
		   aw869x_vbat_monitor_store);
static DEVICE_ATTR(lra_resistance, S_IWUSR | S_IRUGO,
		   aw869x_lra_resistance_show, aw869x_lra_resistance_store);
static DEVICE_ATTR(auto_boost, S_IWUSR | S_IRUGO, aw869x_auto_boost_show,
		   aw869x_auto_boost_store);
static DEVICE_ATTR(prctmode, S_IWUSR | S_IRUGO, aw869x_prctmode_show,
		   aw869x_prctmode_store);
static DEVICE_ATTR(trig, S_IWUSR | S_IRUGO, aw869x_trig_show,
		   aw869x_trig_store);
static DEVICE_ATTR(ram_vbat_comp, S_IWUSR | S_IRUGO, aw869x_ram_vbat_comp_show,
		   aw869x_ram_vbat_comp_store);
static DEVICE_ATTR(osc_cali, S_IWUSR | S_IRUGO, aw869x_osc_cali_show,
		   aw869x_osc_cali_store);
static DEVICE_ATTR(f0_check, S_IWUSR | S_IRUGO, aw869x_f0_check_show, NULL);
static DEVICE_ATTR(f0_save, S_IWUSR | S_IRUGO, aw869x_f0_save_show,
		   aw869x_f0_save_store);
static DEVICE_ATTR(osc_save, S_IWUSR | S_IRUGO, aw869x_osc_cali_show,
		   aw869x_osc_save_store);
static DEVICE_ATTR(f0_value, S_IRUGO, aw869x_f0_value_show, NULL);
static DEVICE_ATTR(custom_wave, S_IWUSR | S_IRUGO, aw869x_custom_wave_show,
		   aw869x_custom_wave_store);
static struct attribute *aw869x_vibrator_attributes[] = {
	&dev_attr_effect_id.attr,
	&dev_attr_reg.attr,
	&dev_attr_ram.attr,
	&dev_attr_activate_test.attr,
	&dev_attr_duration.attr,
	&dev_attr_activate.attr,
	&dev_attr_activate_mode.attr,
	&dev_attr_index.attr,
	&dev_attr_vmax.attr,
	&dev_attr_gain.attr,
	&dev_attr_seq.attr,
	&dev_attr_loop.attr,
	&dev_attr_rtp.attr,
	&dev_attr_ram_update.attr,
	&dev_attr_f0.attr,
	&dev_attr_cali.attr,
	&dev_attr_cont.attr,
	&dev_attr_cont_td.attr,
	&dev_attr_cont_drv.attr,
	&dev_attr_cont_num_brk.attr,
	&dev_attr_cont_zc_thr.attr,
	&dev_attr_vbat_monitor.attr,
	&dev_attr_lra_resistance.attr,
	&dev_attr_auto_boost.attr,
	&dev_attr_prctmode.attr,
	&dev_attr_trig.attr,
	&dev_attr_ram_vbat_comp.attr,
	&dev_attr_osc_cali.attr,
	&dev_attr_f0_check.attr,
	&dev_attr_osc_save.attr,
	&dev_attr_f0_save.attr,
	&dev_attr_f0_value.attr,
	&dev_attr_custom_wave.attr,
	NULL
};

struct attribute_group aw869x_vibrator_attribute_group = {
	.attrs = aw869x_vibrator_attributes
};

int aw869x_vibrator_init(struct aw869x *aw869x)
{
	int ret = 0;

	aw_info("%s enter\n", __func__);

	ret = sysfs_create_group(&aw869x->i2c->dev.kobj,
			       &aw869x_vibrator_attribute_group);
	if (ret < 0) {
		aw_info("%s error creating sysfs attr files\n", __func__);
		return ret;
	}
	g_aw869x = aw869x;
	hrtimer_init(&aw869x->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	aw869x->timer.function = aw869x_vibrator_timer_func;
	INIT_WORK(&aw869x->vibrator_work, aw869x_vibrator_work_routine);
	INIT_WORK(&aw869x->rtp_work, aw869x_rtp_work_routine);

	mutex_init(&aw869x->lock);
	mutex_init(&aw869x->rtp_lock);
	atomic_set(&aw869x->is_in_rtp_loop, 0);
	atomic_set(&aw869x->exit_in_rtp_loop, 0);
	atomic_set(&aw869x->is_in_write_loop, 0);
	init_waitqueue_head(&aw869x->wait_q);
	init_waitqueue_head(&aw869x->stop_wait_q);

	return 0;
}

int aw869x_haptic_init(struct aw869x *aw869x, unsigned char chip_name)
{
	int ret = 0;
	unsigned char i = 0;
	unsigned char reg_val = 0;
	unsigned char bemf_config = 0;

	aw_info("%s enter\n", __func__);
	ret = misc_register(&aw869x_haptic_misc);
	if (ret) {
		aw_err("%s: misc fail: %d\n", __func__, ret);
		return ret;
	}

	/* haptic audio */
	aw869x->haptic_audio.delay_val = 1;
	aw869x->haptic_audio.timer_val = 21318;
	aw869x->f0_cali_status = true;

	hrtimer_init(&aw869x->haptic_audio.timer, CLOCK_MONOTONIC,
		     HRTIMER_MODE_REL);
	aw869x->haptic_audio.timer.function = aw869x_haptic_audio_timer_func;
	INIT_WORK(&aw869x->haptic_audio.work, aw869x_haptic_audio_work_routine);

	mutex_init(&aw869x->haptic_audio.lock);

	hrtimer_init(&aw869x->stop_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	aw869x->stop_timer.function = qti_hap_stop_timer;
	hrtimer_init(&aw869x->hap_disable_timer, CLOCK_MONOTONIC,
		     HRTIMER_MODE_REL);
	aw869x->hap_disable_timer.function = qti_hap_disable_timer;

	/* haptic init */
	mutex_lock(&aw869x->lock);

	aw869x->activate_mode = aw869x->info.mode;
	aw869x->osc_cali_run = 0;
	ret = aw869x_i2c_read(aw869x, AW869X_REG_WAVSEQ1, &reg_val);
	aw869x->index = reg_val & 0x7F;
	ret = aw869x_i2c_read(aw869x, AW869X_REG_DATDBG, &reg_val);
	aw869x->gain = reg_val & 0xFF;
	ret = aw869x_i2c_read(aw869x, AW869X_REG_BSTDBG4, &reg_val);
	aw869x->vmax = (reg_val >> 1) & 0x1F;
	for (i = 0; i < AW869X_SEQUENCER_SIZE; i++) {
		ret = aw869x_i2c_read(aw869x, AW869X_REG_WAVSEQ1 + i, &reg_val);
		aw869x->seq[i] = reg_val;
	}

	aw869x_haptic_play_mode(aw869x, AW869X_HAPTIC_STANDBY_MODE);

	aw869x_haptic_set_pwm(aw869x, AW869X_PWM_24K);
	/*LRA trim source select register*/
	aw869x_i2c_write_bits(aw869x, AW869X_REG_ANACTRL,
				      AW869X_BIT_ANACTRL_LRA_SRC_MASK,
				      AW869X_BIT_ANACTRL_LRA_SRC_REG);
	aw869x_i2c_write(aw869x, AW869X_REG_BSTDBG1, aw869x->info.bstdbg[0]);
	aw869x_i2c_write(aw869x, AW869X_REG_BSTDBG2, aw869x->info.bstdbg[1]);
	aw869x_i2c_write(aw869x, AW869X_REG_BSTDBG3, aw869x->info.bstdbg[2]);
	aw869x_i2c_write(aw869x, AW869X_REG_TSET, aw869x->info.tset);
	aw869x_i2c_write(aw869x, AW869X_REG_R_SPARE, aw869x->info.r_spare);

	aw869x_i2c_write_bits(aw869x, AW869X_REG_ANADBG,
			      AW869X_BIT_ANADBG_IOC_MASK,
			      AW869X_BIT_ANADBG_IOC_4P65A);

	if (chip_name == AW8697)
		aw869x_haptic_set_bst_peak_cur(aw869x, AW8697_DEFAULT_PEAKCUR);
	else if (chip_name == AW8695)
		aw869x_haptic_set_bst_peak_cur(aw869x, AW8695_DEFAULT_PEAKCUR);
	else
		aw_err("chip name is error");

	aw869x_haptic_swicth_motorprotect_config(aw869x, AW_PROTECT_EN, AW_PROTECT_VAL);

	aw869x_haptic_auto_boost_config(aw869x, false);

	if ((aw869x->info.trig_config[0][0] == 1) ||
		(aw869x->info.trig_config[1][0] == 1) ||
		(aw869x->info.trig_config[2][0] == 1)) {
		aw869x_haptic_trig_param_init(aw869x);
		aw869x_haptic_trig_param_config(aw869x);
	}

	aw869x_haptic_offset_calibration(aw869x);

	/* vbat compensation */
	aw869x_haptic_cont_vbat_mode(aw869x,
				     AW869X_HAPTIC_CONT_VBAT_HW_COMP_MODE);
	aw869x->ram_vbat_comp = AW869X_HAPTIC_RAM_VBAT_COMP_ENABLE;

	mutex_unlock(&aw869x->lock);

	/* f0 calibration */
	mutex_lock(&aw869x->lock);
#ifndef USE_CONT_F0_CALI
	aw869x_haptic_upload_lra(aw869x, AW869X_WRITE_ZERO);
	aw869x_haptic_f0_calibration(aw869x);
#endif
	mutex_unlock(&aw869x->lock);
	/* beme config */
	bemf_config = aw869x->info.bemf_config[0];
	aw869x_i2c_write(aw869x, AW869X_REG_BEMF_VTHH_H, bemf_config);
	bemf_config = aw869x->info.bemf_config[1];
	aw869x_i2c_write(aw869x, AW869X_REG_BEMF_VTHH_L, bemf_config);
	bemf_config = aw869x->info.bemf_config[2];
	aw869x_i2c_write(aw869x, AW869X_REG_BEMF_VTHL_H, bemf_config);
	bemf_config = aw869x->info.bemf_config[3];
	aw869x_i2c_write(aw869x, AW869X_REG_BEMF_VTHL_L, bemf_config);
	return ret;
}

int aw869x_ram_init(struct aw869x *aw869x)
{
#ifdef AW_RAM_UPDATE_DELAY
	int ram_timer_val = 5000;

	aw_info("%s enter\n", __func__);
	INIT_DELAYED_WORK(&aw869x->ram_work, aw869x_ram_work_routine);
	/*schedule_delayed_work(&aw869x->ram_work,
	 *			msecs_to_jiffies(ram_timer_val));
	 */
	queue_delayed_work(aw869x->work_queue, &aw869x->ram_work,
			   msecs_to_jiffies(ram_timer_val));
#else
	aw869x_ram_update(aw869x);
#endif
	return 0;
}
