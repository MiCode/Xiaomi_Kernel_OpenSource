/*
* aw8624.c  aw8624 haptic module
*
* Version: v1.1.1
*
* Copyright (c) 2018 AWINIC Technology CO., LTD
* Copyright (C) 2020 XiaoMi, Inc.
*
* Author: Joseph <zhangzetao@awinic.com.cn>
*
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the
* Free Software Foundation; either version 2 of the License, or (at your
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
#include <linux/interrupt.h>
#include <linux/debugfs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <linux/power_supply.h>
//#include <linux/wakelock.h>
#include <linux/jiffies.h>
#include <aw8624.h>
#include <aw8624_reg.h>

/******************************************************
 *
 * Marco
 *
 ******************************************************/
#define AW8624_I2C_NAME "aw8624_haptic"
#define AW8624_HAPTIC_NAME "aw8624_haptic"
#define AW8624_VERSION "v1.1.1"
#define AWINIC_RAM_UPDATE_DELAY
#define AW_I2C_RETRIES 2
#define AW_I2C_RETRY_DELAY 2
#define AW_READ_CHIPID_RETRIES 5
#define AW_READ_CHIPID_RETRY_DELAY 2
#define AW8624_MAX_DSP_START_TRY_COUNT	10
#define AW8624_MAX_FIRMWARE_LOAD_CNT 20

/******************************************************
 *
 * variable
 *
 ******************************************************/
static char *aw8624_ram_name = "aw8624_haptic.bin";

#define AW8624_RTP_NAME_MAX		64
static char aw8624_rtp_name[][AW8624_RTP_NAME_MAX] = {
	{"aw8624_rtp.bin"},
	{"aw8624_rtp_lighthouse.bin"},
	{"aw8624_rtp_silk.bin"},
};
struct aw8624_container *aw8624_rtp;
struct aw8624 *g_aw8624;
struct aw8624_dts_info aw8624_dts_data;
/******************************************************
*
* functions
*
******************************************************/
static void aw8624_interrupt_clear(struct aw8624 *aw8624);
static int
aw8624_haptic_trig1_config(struct aw8624 *aw8624, unsigned char flag);
/******************************************************
*
* aw8624 i2c write/read
*
******************************************************/
static int aw8624_i2c_write(struct aw8624 *aw8624,
		 unsigned char reg_addr, unsigned char reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < AW_I2C_RETRIES) {
		ret =
		i2c_smbus_write_byte_data(aw8624->i2c, reg_addr, reg_data);
		if (ret < 0) {
			pr_err("%s: i2c_write cnt=%d error=%d\n",
				__func__, cnt, ret);
		} else {
			break;
		}
		cnt++;
		usleep_range(2000, 3000);
	}

	return ret;
}

static int aw8624_i2c_read(struct aw8624 *aw8624,
		unsigned char reg_addr, unsigned char *reg_data)
{
	int ret = -1;
	unsigned char cnt = 0;

	while (cnt < AW_I2C_RETRIES) {
		ret = i2c_smbus_read_byte_data(aw8624->i2c, reg_addr);
		if (ret < 0) {
			pr_err("%s: i2c_read cnt=%d error=%d\n",
				__func__, cnt, ret);
		} else {
			*reg_data = ret;
			break;
		}
		cnt++;
		usleep_range(2000, 3000);
	}

	return ret;
}

static int aw8624_i2c_write_bits(struct aw8624 *aw8624,
	unsigned char reg_addr, unsigned int mask, unsigned char reg_data)
{
	unsigned char reg_val = 0;

	aw8624_i2c_read(aw8624, reg_addr, &reg_val);
	reg_val &= mask;
	reg_val |= reg_data;
	aw8624_i2c_write(aw8624, reg_addr, reg_val);

	return 0;
}


static int aw8624_i2c_writes(struct aw8624 *aw8624,
		unsigned char reg_addr, unsigned char *buf, unsigned int len)
{
	int ret = -1;
	unsigned char *data;

	data = kmalloc(len+1, GFP_KERNEL);
	if (data == NULL)
		return -ENOMEM;

	data[0] = reg_addr;
	memcpy(&data[1], buf, len);

	ret = i2c_master_send(aw8624->i2c, data, len+1);
	if (ret < 0)
		pr_err("%s: i2c master send error\n", __func__);

	kfree(data);

	return ret;
}

static void aw8624_interrupt_clear(struct aw8624 *aw8624)
{
	unsigned char reg_val = 0;

	aw8624_i2c_read(aw8624, AW8624_REG_SYSINT, &reg_val);
	pr_info("%s: reg SYSINT=0x%x\n", __func__, reg_val);
}
static void aw8624_init_extra(struct aw8624 *aw8624)
{
	if (aw8624->IsUsedIRQ == true) {
		aw8624_haptic_trig1_config(aw8624, true);
		pr_info("enter %s\n", __func__);
	}
}
/*****************************************************
 *
 * ram update
 *
 *****************************************************/

static void aw8624_rtp_loaded(const struct firmware *cont, void *context)
{
	struct aw8624 *aw8624 = context;

	if (!cont) {
		pr_err("%s: failed to read %s\n",
			__func__, aw8624_rtp_name[aw8624->rtp_file_num]);
		release_firmware(cont);
		return;
	}

	pr_info("%s: loaded %s - size: %zu\n", __func__,
		aw8624_rtp_name[aw8624->rtp_file_num], cont ? cont->size : 0);

	/* aw8624 rtp update */
	aw8624_rtp = kzalloc(cont->size+sizeof(int), GFP_KERNEL);
	if (!aw8624_rtp) {
		release_firmware(cont);
		pr_err("%s: Error allocating memory\n", __func__);
		return;
	}
	aw8624_rtp->len = cont->size;
	pr_info("%s: rtp size = %d\n", __func__, aw8624_rtp->len);
	memcpy(aw8624_rtp->data, cont->data, cont->size);
	release_firmware(cont);

	aw8624->rtp_init = 1;
	aw8624_init_extra(aw8624);
	pr_info("%s: rtp update complete\n", __func__);
}

static int aw8624_rtp_update(struct aw8624 *aw8624)
{
	pr_info("%s enter\n", __func__);

	return request_firmware_nowait(THIS_MODULE,
					FW_ACTION_HOTPLUG,
					aw8624_rtp_name[aw8624->rtp_file_num],
					aw8624->dev,
					GFP_KERNEL,
					aw8624,
					aw8624_rtp_loaded);
}


static void aw8624_container_update(struct aw8624 *aw8624,
		struct aw8624_container *aw8624_cont)
{
	int i = 0;
	unsigned int shift = 0;

	mutex_lock(&aw8624->lock);

	aw8624->ram.baseaddr_shift = 2;
	aw8624->ram.ram_shift = 4;

	/* RAMINIT Enable */
	aw8624_i2c_write_bits(aw8624, AW8624_REG_SYSCTRL,
		AW8624_BIT_SYSCTRL_RAMINIT_MASK, AW8624_BIT_SYSCTRL_RAMINIT_EN);

	/* base addr */
	shift = aw8624->ram.baseaddr_shift;
	aw8624->ram.base_addr = (unsigned int)((aw8624_cont->data[0+shift]<<8) |
		(aw8624_cont->data[1+shift]));
	pr_info("%s: base_addr=0x%4x\n", __func__, aw8624->ram.base_addr);

	aw8624_i2c_write(aw8624,
			AW8624_REG_BASE_ADDRH,
			aw8624_cont->data[0+shift]);
	aw8624_i2c_write(aw8624,
			AW8624_REG_BASE_ADDRL,
			aw8624_cont->data[1+shift]);

	aw8624_i2c_write(aw8624,
			AW8624_REG_FIFO_AEH,
			(unsigned char)((aw8624->ram.base_addr>>2)>>8));
	aw8624_i2c_write(aw8624,
			AW8624_REG_FIFO_AEL,
			(unsigned char)((aw8624->ram.base_addr>>2)&0x00FF));
	aw8624_i2c_write(aw8624,
			AW8624_REG_FIFO_AFH,
			(unsigned char)((aw8624->ram.base_addr
			- (aw8624->ram.base_addr>>2))>>8));
	aw8624_i2c_write(aw8624,
			AW8624_REG_FIFO_AFL,
			(unsigned char)((aw8624->ram.base_addr
			-(aw8624->ram.base_addr>>2))&0x00FF));

	/* ram */
	shift = aw8624->ram.baseaddr_shift;
	aw8624_i2c_write(aw8624,
			AW8624_REG_RAMADDRH, aw8624_cont->data[0+shift]);
	aw8624_i2c_write(aw8624,
			AW8624_REG_RAMADDRL, aw8624_cont->data[1+shift]);
	shift = aw8624->ram.ram_shift;
	for (i = shift; i < aw8624_cont->len; i++) {
		aw8624_i2c_write(aw8624,
				AW8624_REG_RAMDATA, aw8624_cont->data[i]);
	}

	/* RAMINIT Disable */
	aw8624_i2c_write_bits(aw8624,
				AW8624_REG_SYSCTRL,
				AW8624_BIT_SYSCTRL_RAMINIT_MASK,
				AW8624_BIT_SYSCTRL_RAMINIT_OFF);

	mutex_unlock(&aw8624->lock);

	pr_info("%s exit\n", __func__);
}


static void aw8624_ram_loaded(const struct firmware *cont, void *context)
{
	struct aw8624 *aw8624 = context;
	struct aw8624_container *aw8624_fw;
	int i = 0;
	unsigned short check_sum = 0;

	if (!cont) {
		pr_err("%s: failed to read %s\n", __func__, aw8624_ram_name);
		release_firmware(cont);
		return;
	}

	pr_info("%s: loaded %s - size: %zu\n", __func__, aw8624_ram_name,
		cont ? cont->size : 0);

	/* check sum */
	for (i = 2; i < cont->size; i++)
		check_sum += cont->data[i];

	if (check_sum == (unsigned short)((cont->data[0]<<8)|(cont->data[1]))) {
		pr_info("%s: check sum pass : 0x%04x\n", __func__, check_sum);
		aw8624->ram.check_sum = check_sum;
	} else {
		pr_err("%s: check sum err: check_sum=0x%04x\n",
		__func__, check_sum);
		return;
	}

	/* aw8624 ram update */
	aw8624_fw = kzalloc(cont->size+sizeof(int), GFP_KERNEL);
	if (!aw8624_fw) {
		release_firmware(cont);
		pr_err("%s: Error allocating memory\n", __func__);
		return;
	}
	aw8624_fw->len = cont->size;
	memcpy(aw8624_fw->data, cont->data, cont->size);
	release_firmware(cont);

	aw8624_container_update(aw8624, aw8624_fw);

	aw8624->ram.len = aw8624_fw->len;

	kfree(aw8624_fw);

	aw8624->ram_init = 1;
	pr_info("%s: fw update complete\n", __func__);

	if (aw8624->IsUsedIRQ)
		aw8624_rtp_update(aw8624);

}

static int aw8624_ram_update(struct aw8624 *aw8624)
{
	aw8624->ram_init = 0;
	aw8624->rtp_init = 0;
	return request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
				aw8624_ram_name, aw8624->dev, GFP_KERNEL,
				aw8624, aw8624_ram_loaded);
}

static void aw8624_ram_work_routine(struct work_struct *work)
{
	struct aw8624 *aw8624 =
		container_of(work, struct aw8624, ram_work.work);

	aw8624_ram_update(aw8624);

}

static int aw8624_ram_init(struct aw8624 *aw8624)
{
	if (aw8624->ram_update_delay) {
		int ram_timer_val = 5000;

		INIT_DELAYED_WORK(&aw8624->ram_work, aw8624_ram_work_routine);
		schedule_delayed_work(&aw8624->ram_work,
					msecs_to_jiffies(ram_timer_val));
	} else {
		aw8624_ram_update(aw8624);
	}
	return 0;
}

static void aw8624_stop_work_routine(struct work_struct *work)
{
	struct aw8624 *aw8624 =
		container_of(work, struct aw8624, stop_work.work);
	/* restore default work mode */
	aw8624_i2c_write_bits(aw8624,
				AW8624_REG_CONT_CTRL,
				AW8624_BIT_CONT_CTRL_EN_CLOSE_MASK,
				AW8624_BIT_CONT_CTRL_CLOSE_PLAYBACK);
	aw8624_i2c_write_bits(aw8624,
				AW8624_REG_CONT_CTRL,
				AW8624_BIT_CONT_CTRL_F0_DETECT_MASK,
				AW8624_BIT_CONT_CTRL_F0_DETECT_DISABLE);

}
/*****************************************************
 *
 * haptic control
 *
 *****************************************************/
static int aw8624_haptic_softreset(struct aw8624 *aw8624)
{
	aw8624_i2c_write(aw8624, AW8624_REG_ID, 0xAA);
	usleep_range(2000, 2500);
	return 0;
}

static int aw8624_haptic_active(struct aw8624 *aw8624)
{
	aw8624_i2c_write_bits(aw8624,
				AW8624_REG_SYSCTRL,
				AW8624_BIT_SYSCTRL_WORK_MODE_MASK,
				AW8624_BIT_SYSCTRL_ACTIVE);
	aw8624_interrupt_clear(aw8624);
	aw8624_i2c_write_bits(aw8624,
				AW8624_REG_SYSINTM,
				AW8624_BIT_SYSINTM_UVLO_MASK,
				AW8624_BIT_SYSINTM_UVLO_EN);
	return 0;
}

static int
aw8624_haptic_play_mode(struct aw8624 *aw8624, unsigned char play_mode)
{
	switch (play_mode) {
	case AW8624_HAPTIC_STANDBY_MODE:
		aw8624->play_mode = AW8624_HAPTIC_STANDBY_MODE;
		aw8624_i2c_write_bits(aw8624,
				AW8624_REG_SYSCTRL,
				AW8624_BIT_SYSCTRL_WORK_MODE_MASK,
				AW8624_BIT_SYSCTRL_STANDBY);
		break;
	case AW8624_HAPTIC_RAM_MODE:
		aw8624->play_mode = AW8624_HAPTIC_RAM_MODE;
		aw8624_i2c_write_bits(aw8624,
				AW8624_REG_SYSCTRL,
				AW8624_BIT_SYSCTRL_PLAY_MODE_MASK,
				AW8624_BIT_SYSCTRL_PLAY_MODE_RAM);
		aw8624_haptic_active(aw8624);
		break;
	case AW8624_HAPTIC_RAM_LOOP_MODE:
		aw8624->play_mode = AW8624_HAPTIC_RAM_LOOP_MODE;
		aw8624_i2c_write_bits(aw8624,
				AW8624_REG_SYSCTRL,
				AW8624_BIT_SYSCTRL_PLAY_MODE_MASK,
				AW8624_BIT_SYSCTRL_PLAY_MODE_RAM);
		aw8624_haptic_active(aw8624);
		break;
	case AW8624_HAPTIC_RTP_MODE:
		aw8624->play_mode = AW8624_HAPTIC_RTP_MODE;
		aw8624_i2c_write_bits(aw8624,
				AW8624_REG_SYSCTRL,
				AW8624_BIT_SYSCTRL_PLAY_MODE_MASK,
				AW8624_BIT_SYSCTRL_PLAY_MODE_RTP);
		aw8624_haptic_active(aw8624);
		break;
	case AW8624_HAPTIC_TRIG_MODE:
		aw8624->play_mode = AW8624_HAPTIC_TRIG_MODE;
		aw8624_i2c_write_bits(aw8624,
				AW8624_REG_SYSCTRL,
				AW8624_BIT_SYSCTRL_PLAY_MODE_MASK,
				AW8624_BIT_SYSCTRL_PLAY_MODE_RAM);
		aw8624_haptic_active(aw8624);
		break;
	case AW8624_HAPTIC_CONT_MODE:
		aw8624->play_mode = AW8624_HAPTIC_CONT_MODE;
		aw8624_i2c_write_bits(aw8624,
				AW8624_REG_SYSCTRL,
				AW8624_BIT_SYSCTRL_PLAY_MODE_MASK,
				AW8624_BIT_SYSCTRL_PLAY_MODE_CONT);
		aw8624_haptic_active(aw8624);
		break;
	default:
		dev_err(aw8624->dev, "%s: play mode %d err",
				__func__, play_mode);
		break;
	}
	return 0;
}

static int aw8624_haptic_play_go(struct aw8624 *aw8624, bool flag)
{
	pr_debug("%s enter\n", __func__);
	if (!flag) {
		do_gettimeofday(&aw8624->current_time);
		aw8624->interval_us = (aw8624->current_time.tv_sec - aw8624->pre_enter_time.tv_sec) * 1000000
		+ (aw8624->current_time.tv_usec-aw8624->pre_enter_time.tv_usec);
		if (aw8624->interval_us < 2000) {
			pr_info("aw8624->interval_us t=%d\n", aw8624->interval_us);
			mdelay(2);
		}
	}
	if (flag == true) {
		aw8624_i2c_write_bits(aw8624, AW8624_REG_GO,
			AW8624_BIT_GO_MASK, AW8624_BIT_GO_ENABLE);
		do_gettimeofday(&aw8624->pre_enter_time);
	} else {
		aw8624_i2c_write_bits(aw8624, AW8624_REG_GO,
			AW8624_BIT_GO_MASK, AW8624_BIT_GO_DISABLE);
	}
	return 0;
}

static int aw8624_haptic_stop_delay(struct aw8624 *aw8624)
{
	unsigned char reg_val = 0;
	unsigned int cnt = 100;

	while (cnt--) {
		aw8624_i2c_read(aw8624, AW8624_REG_GLB_STATE, &reg_val);
		if ((reg_val&0x0f) == 0x00) {
			return 0;
		mdelay(2);
		}

		pr_info("%s wait for standby, reg glb_state=0x%02x\n",
		__func__, reg_val);
	}
	pr_err("%s do not enter standby automatically\n", __func__);
	return 0;
}

static int aw8624_haptic_stop(struct aw8624 *aw8624)
{
	aw8624_haptic_play_go(aw8624, false);
	aw8624_haptic_stop_delay(aw8624);
	aw8624_haptic_play_mode(aw8624, AW8624_HAPTIC_STANDBY_MODE);

	return 0;
}

static int aw8624_haptic_start(struct aw8624 *aw8624)
{
	aw8624_haptic_active(aw8624);
	aw8624_haptic_play_go(aw8624, true);

	return 0;
}

static int aw8624_haptic_set_wav_seq(struct aw8624 *aw8624,
		unsigned char wav, unsigned char seq)
{
	aw8624_i2c_write(aw8624, AW8624_REG_WAVSEQ1+wav, seq);
	return 0;
}

static int aw8624_haptic_set_wav_loop(struct aw8624 *aw8624,
		unsigned char wav, unsigned char loop)
{
	unsigned char tmp = 0;

	if (wav%2) {
		tmp = loop<<0;
		aw8624_i2c_write_bits(aw8624, AW8624_REG_WAVLOOP1+(wav/2),
			AW8624_BIT_WAVLOOP_SEQNP1_MASK, tmp);
	} else {
		tmp = loop<<4;
		aw8624_i2c_write_bits(aw8624, AW8624_REG_WAVLOOP1+(wav/2),
			AW8624_BIT_WAVLOOP_SEQN_MASK, tmp);
	}

	return 0;
}

static int
aw8624_haptic_set_repeat_wav_seq(struct aw8624 *aw8624, unsigned char seq)
{
	aw8624_haptic_set_wav_seq(aw8624, 0x00, seq);
	aw8624_haptic_set_wav_loop(aw8624,
				0x00,
				AW8624_BIT_WAVLOOP_INIFINITELY);

	return 0;
}

static int aw8624_haptic_set_gain(struct aw8624 *aw8624, unsigned char gain)
{
	aw8624_i2c_write(aw8624, AW8624_REG_DATDBG, gain);
	return 0;
}

static int aw8624_haptic_set_pwm(struct aw8624 *aw8624, unsigned char mode)
{
	switch (mode) {
	case AW8624_PWM_48K:
		aw8624_i2c_write_bits(aw8624,
				AW8624_REG_PWMDBG,
				AW8624_BIT_PWMDBG_PWM_MODE_MASK,
				AW8624_BIT_PWMDBG_PWM_48K);
		break;
	case AW8624_PWM_24K:
		aw8624_i2c_write_bits(aw8624,
				AW8624_REG_PWMDBG,
				AW8624_BIT_PWMDBG_PWM_MODE_MASK,
				AW8624_BIT_PWMDBG_PWM_24K);
		break;
	case AW8624_PWM_12K:
		aw8624_i2c_write_bits(aw8624,
				AW8624_REG_PWMDBG,
				AW8624_BIT_PWMDBG_PWM_MODE_MASK,
				AW8624_BIT_PWMDBG_PWM_12K);
		break;
	default:
		break;
	}
	return 0;
}

static int
aw8624_haptic_play_repeat_seq(struct aw8624 *aw8624, unsigned char flag)
{
	if (flag) {
		aw8624_haptic_play_mode(aw8624, AW8624_HAPTIC_RAM_LOOP_MODE);
		aw8624_haptic_start(aw8624);
	}

	return 0;
}

static int aw8624_haptic_swicth_motorprotect_config(struct aw8624 *aw8624,
		unsigned char addr, unsigned char val)
{
	if (addr == 1) {
		aw8624_i2c_write_bits(aw8624,
				AW8624_REG_DETCTRL,
				AW8624_BIT_DETCTRL_PROTECT_MASK,
				AW8624_BIT_DETCTRL_PROTECT_SHUTDOWN);
		aw8624_i2c_write_bits(aw8624,
				AW8624_REG_PWMPRC,
				AW8624_BIT_PWMPRC_PRC_EN_MASK,
				AW8624_BIT_PWMPRC_PRC_ENABLE);
		aw8624_i2c_write_bits(aw8624,
				AW8624_REG_PRLVL,
				AW8624_BIT_PRLVL_PR_EN_MASK,
				AW8624_BIT_PRLVL_PR_ENABLE);
	} else if (addr == 0) {
		aw8624_i2c_write_bits(aw8624,
				AW8624_REG_DETCTRL,
				AW8624_BIT_DETCTRL_PROTECT_MASK,
				AW8624_BIT_DETCTRL_PROTECT_NO_ACTION);
		aw8624_i2c_write_bits(aw8624,
				AW8624_REG_PWMPRC,
				AW8624_BIT_PWMPRC_PRC_EN_MASK,
				AW8624_BIT_PWMPRC_PRC_DISABLE);
		aw8624_i2c_write_bits(aw8624,
				AW8624_REG_PRLVL,
				AW8624_BIT_PRLVL_PR_EN_MASK,
				AW8624_BIT_PRLVL_PR_DISABLE);
	} else if (addr == 0x2d) {
		aw8624_i2c_write_bits(aw8624, AW8624_REG_PWMPRC,
			AW8624_BIT_PWMPRC_PRCTIME_MASK, val);
	} else if (addr == 0x3e) {
		aw8624_i2c_write_bits(aw8624, AW8624_REG_PRLVL,
			AW8624_BIT_PRLVL_PRLVL_MASK, val);
	} else if (addr == 0x3f) {
		aw8624_i2c_write_bits(aw8624, AW8624_REG_PRTIME,
			AW8624_BIT_PRTIME_PRTIME_MASK, val);
	} else {
		 /*nothing to do;*/
	}

	 return 0;
}

static int aw8624_haptic_trig1_config(struct aw8624 *aw8624, unsigned char flag)
{
	if (flag) {
		aw8624_i2c_write_bits(aw8624,
				AW8624_REG_DBGCTRL,
				AW8624_BIT_DBGCTRL_INTN_TRG_SEL_MASK,
				AW8624_BIT_DBGCTRL_TRG_SEL_ENABLE);
		aw8624_i2c_write_bits(aw8624,
				AW8624_REG_TRG_CFG2,
				AW8624_BIT_TRGCFG2_TRG1_ENABLE_MASK,
				AW8624_BIT_TRGCFG2_TRG1_ENABLE);
	} else {
		aw8624_i2c_write_bits(aw8624,
				AW8624_REG_DBGCTRL,
				AW8624_BIT_DBGCTRL_INTN_TRG_SEL_MASK,
				AW8624_BIT_DBGCTRL_TRG_SEL_ENABLE);
		aw8624_i2c_write_bits(aw8624,
				AW8624_REG_TRG_CFG2,
				AW8624_BIT_TRGCFG2_TRG1_ENABLE_MASK,
				AW8624_BIT_TRGCFG2_TRG1_DISABLE);
	}
	return 0;
}

static int aw8624_haptic_vbat_mode(struct aw8624 *aw8624, unsigned char flag)
{
	if (flag == AW8624_HAPTIC_VBAT_HW_COMP_MODE) {
		aw8624_i2c_write_bits(aw8624,
				AW8624_REG_ADCTEST,
				AW8624_BIT_DETCTRL_VBAT_MODE_MASK,
				AW8624_BIT_DETCTRL_VBAT_HW_COMP);
	} else {
		aw8624_i2c_write_bits(aw8624,
				AW8624_REG_ADCTEST,
				AW8624_BIT_DETCTRL_VBAT_MODE_MASK,
				AW8624_BIT_DETCTRL_VBAT_SW_COMP);
	}
	return 0;
}

static int aw8624_haptic_set_f0_preset(struct aw8624 *aw8624)
{
	unsigned int f0_reg = 0;

	f0_reg = 1000000000/(aw8624->f0_pre*aw8624_dts_data.aw8624_f0_coeff);
	aw8624_i2c_write(aw8624,
			AW8624_REG_F_PRE_H,
			(unsigned char)((f0_reg>>8)&0xff));
	aw8624_i2c_write(aw8624,
			AW8624_REG_F_PRE_L,
			(unsigned char)((f0_reg>>0)&0xff));

	return 0;
}

static int aw8624_haptic_read_f0(struct aw8624 *aw8624)
{
	int ret = 0;
	unsigned char reg_val = 0;
	unsigned int f0_reg = 0;
	unsigned long f0_tmp = 0;

	ret = aw8624_i2c_read(aw8624, AW8624_REG_F_LRA_F0_H, &reg_val);
	f0_reg = (reg_val<<8);
	ret = aw8624_i2c_read(aw8624, AW8624_REG_F_LRA_F0_L, &reg_val);
	f0_reg |= (reg_val<<0);
	f0_tmp = 1000000000/(f0_reg*aw8624_dts_data.aw8624_f0_coeff);
	aw8624->f0 = (unsigned int)f0_tmp;
	pr_info("%s f0=%d\n", __func__, aw8624->f0);
	return 0;
}

static int aw8624_haptic_read_cont_f0(struct aw8624 *aw8624)
{
	int ret = 0;
	unsigned char reg_val = 0;
	unsigned int f0_reg = 0;
	unsigned long f0_tmp = 0;

	ret = aw8624_i2c_read(aw8624, AW8624_REG_F_LRA_CONT_H, &reg_val);
	f0_reg = (reg_val<<8);
	ret = aw8624_i2c_read(aw8624, AW8624_REG_F_LRA_CONT_L, &reg_val);
	f0_reg |= (reg_val<<0);
	f0_tmp = 1000000000/(f0_reg*aw8624_dts_data.aw8624_f0_coeff);
	aw8624->cont_f0 = (unsigned int)f0_tmp;
	pr_info("%s cont_f0=%d\n", __func__, aw8624->cont_f0);
	return 0;
}

static int aw8624_haptic_read_beme(struct aw8624 *aw8624)
{
	int ret = 0;
	unsigned char reg_val = 0;

	ret = aw8624_i2c_read(aw8624, AW8624_REG_WAIT_VOL_MP, &reg_val);
	aw8624->max_pos_beme = (reg_val<<0);
	ret = aw8624_i2c_read(aw8624, AW8624_REG_WAIT_VOL_MN, &reg_val);
	aw8624->max_neg_beme = (reg_val<<0);

	pr_info("%s max_pos_beme=%d\n", __func__, aw8624->max_pos_beme);
	pr_info("%s max_neg_beme=%d\n", __func__, aw8624->max_neg_beme);

	return 0;
}

static int aw8624_haptic_ram_config(struct aw8624 *aw8624)
{
	unsigned char wavseq = 0;
	unsigned char wavloop = 0;

	if ((aw8624->duration > 0) &&
	(aw8624->duration < aw8624_dts_data.aw8624_duration_time[0])) {
		wavseq = aw8624_dts_data.aw8624_wavseq[3];	/*2*/
		wavloop = 0;
	} else if ((aw8624->duration >= aw8624_dts_data.aw8624_duration_time[0]) &&
	(aw8624->duration < aw8624_dts_data.aw8624_duration_time[1])) {
		wavseq = aw8624_dts_data.aw8624_wavseq[5];	/*3*/
		wavloop = 0;
	} else if ((aw8624->duration >= aw8624_dts_data.aw8624_duration_time[1]) &&
	(aw8624->duration < aw8624_dts_data.aw8624_duration_time[2])) {
		wavseq = aw8624_dts_data.aw8624_wavseq[7];	/*4*/
		wavloop = 0;
	} else if (aw8624->duration >= aw8624_dts_data.aw8624_duration_time[2]) {
		wavseq = aw8624_dts_data.aw8624_wavseq[1];	/*1*/
		wavloop = 15;	/*long vibration*/
	} else {
		wavseq = 0;
		wavloop = 0;
	}

	aw8624_haptic_set_wav_seq(aw8624, 0, wavseq);
	aw8624_haptic_set_wav_loop(aw8624, 0, wavloop);
	aw8624_haptic_set_wav_seq(aw8624, 1, 0);
	aw8624_haptic_set_wav_loop(aw8624, 1, 0);

	return 0;
}

static int aw8624_vbat_monitor_detector(struct aw8624 *aw8624)
{
	unsigned char reg_val = 0;
	unsigned char reg_val_sysctrl = 0;
	unsigned char reg_val_detctrl = 0;
	unsigned int vbat = 0;

	aw8624_haptic_stop(aw8624);
	aw8624_i2c_read(aw8624, AW8624_REG_SYSCTRL, &reg_val_sysctrl);
	aw8624_i2c_read(aw8624, AW8624_REG_DETCTRL, &reg_val_detctrl);
	/*step 1:EN_RAMINIT*/
	aw8624_i2c_write_bits(aw8624,
				AW8624_REG_SYSCTRL,
				AW8624_BIT_SYSCTRL_RAMINIT_MASK,
				AW8624_BIT_SYSCTRL_RAMINIT_EN);

	/*step 2 :launch offset cali */
	aw8624_i2c_write_bits(aw8624,
				AW8624_REG_DETCTRL,
				AW8624_BIT_DETCTRL_DIAG_GO_MASK,
				AW8624_BIT_DETCTRL_DIAG_GO_ENABLE);
	/*step 3 :delay */
	usleep_range(2000, 2500);

	/*step 4 :launch power supply testing */
	aw8624_i2c_write_bits(aw8624,
				AW8624_REG_DETCTRL,
				AW8624_BIT_DETCTRL_VBAT_GO_MASK,
				AW8624_BIT_DETCTRL_VABT_GO_ENABLE);
	usleep_range(2000, 2500);

	aw8624_i2c_read(aw8624, AW8624_REG_VBATDET, &reg_val);
	vbat = 6100 * reg_val / 256;

	/*step 5: return val*/
	aw8624_i2c_write(aw8624, AW8624_REG_SYSCTRL, reg_val_sysctrl);

	return vbat;
}

static int aw8624_lra_resistance_detector(struct aw8624 *aw8624)
{
	unsigned char reg_val = 0;
	unsigned char reg_val_sysctrl = 0;
	unsigned char reg_val_anactrl = 0;
	unsigned char reg_val_d2scfg = 0;
	unsigned int r_lra = 0;

	mutex_lock(&aw8624->lock);
	aw8624_i2c_read(aw8624, AW8624_REG_SYSCTRL, &reg_val_anactrl);
	aw8624_i2c_read(aw8624, AW8624_REG_ANACTRL, &reg_val_sysctrl);
	aw8624_i2c_read(aw8624, AW8624_REG_D2SCFG, &reg_val_d2scfg);
	aw8624_haptic_stop(aw8624);
	aw8624_i2c_write_bits(aw8624,
				AW8624_REG_SYSCTRL,
				AW8624_BIT_SYSCTRL_RAMINIT_MASK,
				AW8624_BIT_SYSCTRL_RAMINIT_EN);


	aw8624_i2c_write_bits(aw8624,
				AW8624_REG_ANACTRL,
				AW8624_BIT_ANACTRL_EN_IO_PD1_MASK,
				AW8624_BIT_ANACTRL_EN_IO_PD1_HIGH);

	aw8624_i2c_write_bits(aw8624,
				AW8624_REG_D2SCFG,
				AW8624_BIT_D2SCFG_CLK_ADC_MASK,
				AW8624_BIT_D2SCFG_CLK_ASC_1P5MHZ);

	aw8624_i2c_write_bits(aw8624,
				AW8624_REG_DETCTRL,
				AW8624_BIT_DETCTRL_RL_OS_MASK,
				AW8624_BIT_DETCTRL_RL_DETECT);
	aw8624_i2c_write_bits(aw8624,
				AW8624_REG_DETCTRL,
				AW8624_BIT_DETCTRL_DIAG_GO_MASK,
				AW8624_BIT_DETCTRL_DIAG_GO_ENABLE);
	usleep_range(3000, 3500);
	aw8624_i2c_read(aw8624, AW8624_REG_RLDET, &reg_val);
	r_lra = 298 * reg_val;
	/*len += snprintf(buf+len, PAGE_SIZE-len, "r_lra=%dmohm\n", r_lra);*/

	aw8624_i2c_write(aw8624, AW8624_REG_D2SCFG, reg_val_d2scfg);
	aw8624_i2c_write(aw8624, AW8624_REG_ANACTRL, reg_val_anactrl);
	aw8624_i2c_write(aw8624, AW8624_REG_SYSCTRL, reg_val_sysctrl);

	mutex_unlock(&aw8624->lock);

	return r_lra;
}

static int aw8624_haptic_ram_vbat_comp(struct aw8624 *aw8624, bool flag)
{
	int temp_gain = 0;
	int vbat = 0;

	if (flag) {
		if (aw8624->ram_vbat_comp ==
		AW8624_HAPTIC_RAM_VBAT_COMP_ENABLE) {
			vbat = aw8624_vbat_monitor_detector(aw8624);
			temp_gain = aw8624->gain * AW8624_VBAT_REFER / vbat;
			if
			(temp_gain > (128*AW8624_VBAT_REFER/AW8624_VBAT_MIN)) {
				temp_gain =
					128*AW8624_VBAT_REFER/AW8624_VBAT_MIN;
				pr_debug("%s gain limit=%d\n",
					__func__, temp_gain);
			}
			aw8624_haptic_set_gain(aw8624, temp_gain);
		} else {
			aw8624_haptic_set_gain(aw8624, aw8624->gain);
		}
	} else {
		aw8624_haptic_set_gain(aw8624, aw8624->gain);
	}

	return 0;
}

static void aw8624_haptic_set_rtp_aei(struct aw8624 *aw8624, bool flag)
{
	if (flag) {
		aw8624_i2c_write_bits(aw8624,
					AW8624_REG_SYSINTM,
					AW8624_BIT_SYSINTM_FF_AE_MASK,
					AW8624_BIT_SYSINTM_FF_AE_EN);
	} else {
		aw8624_i2c_write_bits(aw8624,
					AW8624_REG_SYSINTM,
					AW8624_BIT_SYSINTM_FF_AE_MASK,
					AW8624_BIT_SYSINTM_FF_AE_OFF);
	}
}

static unsigned char aw8624_haptic_rtp_get_fifo_afi(struct aw8624 *aw8624)
{
	unsigned char ret = 0;
	unsigned char reg_val = 0;

	aw8624_i2c_read(aw8624, AW8624_REG_SYSINT, &reg_val);
	reg_val &= AW8624_BIT_SYSINT_FF_AFI;
	ret = reg_val>>3;

	return ret;
}

static int aw8624_haptic_rtp_init(struct aw8624 *aw8624)
{
	unsigned int buf_len = 0;

	aw8624->rtp_cnt = 0;

	while ((!aw8624_haptic_rtp_get_fifo_afi(aw8624)) &&
			(aw8624->play_mode == AW8624_HAPTIC_RTP_MODE)) {
		pr_info("%s rtp cnt = %d\n", __func__, aw8624->rtp_cnt);
		if ((aw8624_rtp->len-aw8624->rtp_cnt) <
		(aw8624->ram.base_addr>>3)) {
			buf_len = aw8624_rtp->len-aw8624->rtp_cnt;
		} else {
			buf_len = (aw8624->ram.base_addr>>3);
		}
		aw8624_i2c_writes(aw8624, AW8624_REG_RTP_DATA,
			&aw8624_rtp->data[aw8624->rtp_cnt], buf_len);
		aw8624->rtp_cnt += buf_len;
		if (aw8624->rtp_cnt == aw8624_rtp->len) {
			pr_info("%s: rtp update complete\n", __func__);
			aw8624->rtp_cnt = 0;
			return 0;
		}
	}

	if (aw8624->play_mode == AW8624_HAPTIC_RTP_MODE)
		aw8624_haptic_set_rtp_aei(aw8624, true);

	pr_info("%s exit\n", __func__);

	return 0;
}

static void aw8624_rtp_work_routine(struct work_struct *work)
{
	const struct firmware *rtp_file;
	int ret = -1;
	struct aw8624 *aw8624 = container_of(work, struct aw8624, rtp_work);

	/* fw loaded */
	ret = request_firmware(&rtp_file,
	aw8624_rtp_name[aw8624->rtp_file_num], aw8624->dev);
	if (ret < 0) {
		pr_err("%s: failed to read %s\n", __func__,
		aw8624_rtp_name[aw8624->rtp_file_num]);
		return;
	}
	aw8624->rtp_init = 0;
	kfree(aw8624_rtp);
	aw8624_rtp = kzalloc(rtp_file->size+sizeof(int), GFP_KERNEL);
	if (!aw8624_rtp) {
		release_firmware(rtp_file);
		pr_err("%s: error allocating memory\n", __func__);
		return;
	}
	aw8624_rtp->len = rtp_file->size;
	pr_info("%s: rtp file [%s] size = %d\n", __func__,
	aw8624_rtp_name[aw8624->rtp_file_num], aw8624_rtp->len);
	memcpy(aw8624_rtp->data, rtp_file->data, rtp_file->size);
	release_firmware(rtp_file);

	aw8624->rtp_init = 1;

	aw8624_i2c_write_bits(aw8624,
				AW8624_REG_DBGCTRL,
				AW8624_BIT_DBGCTRL_INTN_TRG_SEL_MASK,
				AW8624_BIT_DBGCTRL_INTN_SEL_ENABLE);

	/* rtp mode config */
	aw8624_haptic_play_mode(aw8624, AW8624_HAPTIC_RTP_MODE);

	/* haptic start */
	aw8624_haptic_start(aw8624);
	aw8624_haptic_rtp_init(aw8624);
}

/*****************************************************
 *
 * haptic - audio
 *
 *****************************************************/
static enum hrtimer_restart
aw8624_haptic_audio_timer_func(struct hrtimer *timer)
{
	struct aw8624 *aw8624 =
	container_of(timer, struct aw8624, haptic_audio.timer);

	schedule_work(&aw8624->haptic_audio.work);
	hrtimer_start(&aw8624->haptic_audio.timer,
			ktime_set(aw8624->haptic_audio.timer_val/1000,
				(aw8624->haptic_audio.timer_val%1000)*1000000),
			HRTIMER_MODE_REL);
	return HRTIMER_NORESTART;
}

static void aw8624_haptic_audio_work_routine(struct work_struct *work)
{
	struct aw8624 *aw8624 =
	container_of(work, struct aw8624, haptic_audio.work);

	mutex_lock(&aw8624->haptic_audio.lock);
	if (aw8624->haptic_audio.ctr[aw8624->haptic_audio.cnt].cmd == 0x01) {
		if (aw8624->haptic_audio.ctr[aw8624->haptic_audio.cnt].play ==
		0x01) {
			pr_info("%s: haptic audio play start\n", __func__);
			mutex_lock(&aw8624->lock);
			aw8624_haptic_stop(aw8624);

			aw8624_haptic_play_mode(aw8624, AW8624_HAPTIC_RAM_MODE);

			aw8624_haptic_set_wav_seq(aw8624, 0x00,
		aw8624->haptic_audio.ctr[aw8624->haptic_audio.cnt].wavseq);
			aw8624_haptic_set_wav_seq(aw8624, 0x01, 0x00);

			aw8624_haptic_set_wav_loop(aw8624, 0x00,
		aw8624->haptic_audio.ctr[aw8624->haptic_audio.cnt].loop);

			aw8624_haptic_set_gain(aw8624,
		aw8624->haptic_audio.ctr[aw8624->haptic_audio.cnt].gain);

			aw8624_haptic_start(aw8624);
			mutex_unlock(&aw8624->lock);
		}
		aw8624->haptic_audio.ctr[aw8624->haptic_audio.cnt].cmd = 0;
	}

	aw8624->haptic_audio.cnt++;
	if (aw8624->haptic_audio.ctr[aw8624->haptic_audio.cnt].cmd == 0) {
		aw8624->haptic_audio.cnt = 0;
		pr_debug("%s: haptic play buffer restart\n", __func__);
	}
	mutex_unlock(&aw8624->haptic_audio.lock);

}



/*****************************************************
 *
 * haptic cont
 *
 *****************************************************/
static int aw8624_haptic_cont(struct aw8624 *aw8624)
{
	unsigned char brake0_level = 0;
	unsigned char en_brake1 = 0;
	unsigned char brake1_level = 0;
	unsigned char en_brake2 = 0;
	unsigned char brake2_level = 0;
	unsigned char brake2_p_num = 0;
	unsigned char brake1_p_num = 0;
	unsigned char brake0_p_num = 0;

	pr_info("%s enter\n.", __func__);
	/* work mode */
	aw8624_haptic_active(aw8624);
	aw8624_haptic_play_mode(aw8624, AW8624_HAPTIC_CONT_MODE);

	/* preset f0 */
	aw8624->f0_pre = aw8624->f0;
	aw8624_haptic_set_f0_preset(aw8624);

	/* lpf */
	aw8624_i2c_write_bits(aw8624,
				AW8624_REG_DATCTRL,
				AW8624_BIT_DATCTRL_FC_MASK,
				AW8624_BIT_DATCTRL_FC_1000HZ);
	aw8624_i2c_write_bits(aw8624,
				AW8624_REG_DATCTRL,
				AW8624_BIT_DATCTRL_LPF_ENABLE_MASK,
				AW8624_BIT_DATCTRL_LPF_ENABLE);

	/* brake */
	en_brake1 = aw8624_dts_data.aw8624_cont_brake[0][0];
	en_brake2 = aw8624_dts_data.aw8624_cont_brake[0][1];
	brake0_level = aw8624_dts_data.aw8624_cont_brake[0][2];
	brake1_level = aw8624_dts_data.aw8624_cont_brake[0][3];
	brake2_level = aw8624_dts_data.aw8624_cont_brake[0][4];
	brake0_p_num = aw8624_dts_data.aw8624_cont_brake[0][5];
	brake1_p_num = aw8624_dts_data.aw8624_cont_brake[0][6];
	brake2_p_num = aw8624_dts_data.aw8624_cont_brake[0][7];

	aw8624_i2c_write(aw8624,
			AW8624_REG_BRAKE0_CTRL,
			(brake0_level << 0));
	aw8624_i2c_write(aw8624,
			AW8624_REG_BRAKE1_CTRL,
			(en_brake1 << 7)|(brake1_level << 0));
	aw8624_i2c_write(aw8624,
			AW8624_REG_BRAKE2_CTRL,
			(en_brake2 << 7)|(brake2_level << 0));
	aw8624_i2c_write(aw8624,
			AW8624_REG_BRAKE_NUM,
			((brake2_p_num << 6)|(brake1_p_num << 3) |
			(brake0_p_num << 0)));

	/* cont config */
	aw8624_i2c_write_bits(aw8624,
				AW8624_REG_CONT_CTRL,
				AW8624_BIT_CONT_CTRL_ZC_DETEC_MASK,
				AW8624_BIT_CONT_CTRL_ZC_DETEC_ENABLE);
	aw8624_i2c_write_bits(aw8624,
				AW8624_REG_CONT_CTRL,
				AW8624_BIT_CONT_CTRL_WAIT_PERIOD_MASK,
				AW8624_BIT_CONT_CTRL_WAIT_1PERIOD);

	aw8624_i2c_write_bits(aw8624,
				AW8624_REG_CONT_CTRL,
				AW8624_BIT_CONT_CTRL_MODE_MASK,
				AW8624_BIT_CONT_CTRL_BY_GO_SIGNAL);

	aw8624_i2c_write_bits(aw8624,
				AW8624_REG_CONT_CTRL,
				AW8624_BIT_CONT_CTRL_EN_CLOSE_MASK,
				AW8624_BIT_CONT_CTRL_CLOSE_PLAYBACK);
	aw8624_i2c_write_bits(aw8624,
				AW8624_REG_CONT_CTRL,
				AW8624_BIT_CONT_CTRL_F0_DETECT_MASK,
				AW8624_BIT_CONT_CTRL_F0_DETECT_DISABLE);
	aw8624_i2c_write_bits(aw8624,
				AW8624_REG_CONT_CTRL,
				AW8624_BIT_CONT_CTRL_O2C_MASK,
				AW8624_BIT_CONT_CTRL_O2C_DISABLE);

	/* TD time */
	aw8624_i2c_write(aw8624,
			AW8624_REG_TD_H,
			(unsigned char)(aw8624->cont_td>>8));
	aw8624_i2c_write(aw8624,
			AW8624_REG_TD_L,
			(unsigned char)(aw8624->cont_td>>0));


	aw8624_i2c_write_bits(aw8624,
			AW8624_REG_BEMF_NUM,
			AW8624_BIT_BEMF_NUM_BRK_MASK,
			aw8624->cont_num_brk);
	aw8624_i2c_write(aw8624,
			AW8624_REG_TIME_NZC,
			0x1f);

	/* f0 driver level */
	aw8624_i2c_write(aw8624,
			AW8624_REG_DRV_LVL,
			aw8624->cont_drv_lvl);
	aw8624_i2c_write(aw8624,
			AW8624_REG_DRV_LVL_OV,
			aw8624->cont_drv_lvl_ov);

	/* cont play go */
	aw8624_haptic_play_go(aw8624, true);

	return 0;
}

/*****************************************************
 *
 * haptic f0 cali
 *
 *****************************************************/
static int aw8624_haptic_get_f0(struct aw8624 *aw8624)
{
	int ret = 0;
	/* unsigned char i = 0; */
	unsigned char reg_val = 0;
	unsigned char f0_pre_num = 0;
	unsigned char f0_wait_num = 0;
	unsigned int f0_repeat_num = 0;
	unsigned char f0_trace_num = 0;
	unsigned int t_f0_ms = 0;
	unsigned int t_f0_trace_ms = 0;
	unsigned int timer_val;
	/* unsigned int f0_cali_cnt = 50; */

	pr_info("%s enter\n", __func__);

	aw8624->f0 = aw8624->f0_pre;
	/* f0 calibrate work mode */
	aw8624_haptic_stop(aw8624);
	aw8624_haptic_play_mode(aw8624, AW8624_HAPTIC_CONT_MODE);


	aw8624_i2c_write_bits(aw8624,
			AW8624_REG_CONT_CTRL,
			AW8624_BIT_CONT_CTRL_EN_CLOSE_MASK,
			AW8624_BIT_CONT_CTRL_OPEN_PLAYBACK);
	aw8624_i2c_write_bits(aw8624,
			AW8624_REG_CONT_CTRL,
			AW8624_BIT_CONT_CTRL_F0_DETECT_MASK,
			AW8624_BIT_CONT_CTRL_F0_DETECT_ENABLE);

	/* LPF */
	aw8624_i2c_write_bits(aw8624,
			AW8624_REG_DATCTRL,
			AW8624_BIT_DATCTRL_FC_MASK,
			AW8624_BIT_DATCTRL_FC_1000HZ);
	aw8624_i2c_write_bits(aw8624,
			AW8624_REG_DATCTRL,
			AW8624_BIT_DATCTRL_LPF_ENABLE_MASK,
			AW8624_BIT_DATCTRL_LPF_ENABLE);

	/* LRA OSC Source */
	if (aw8624->f0_cali_flag == AW8624_HAPTIC_CALI_F0) {
		aw8624_i2c_write_bits(aw8624,
			AW8624_REG_ANACTRL,
			AW8624_BIT_ANACTRL_LRA_SRC_MASK,
			AW8624_BIT_ANACTRL_LRA_SRC_REG);
	} else {
		aw8624_i2c_write_bits(aw8624,
			AW8624_REG_ANACTRL,
			AW8624_BIT_ANACTRL_LRA_SRC_MASK,
			AW8624_BIT_ANACTRL_LRA_SRC_EFUSE);
	}

	/* preset f0 */
	aw8624_haptic_set_f0_preset(aw8624);
	/* f0 driver level */
	aw8624_i2c_write(aw8624, AW8624_REG_DRV_LVL, aw8624->cont_drv_lvl);
	/* f0 trace parameter */
	t_f0_ms = (1000*10 / 2) / aw8624->f0_pre;
	f0_pre_num = aw8624_dts_data.aw8624_f0_trace_parameter[0];
	f0_wait_num = aw8624_dts_data.aw8624_f0_trace_parameter[1];
	f0_trace_num = aw8624_dts_data.aw8624_f0_trace_parameter[3];
	f0_repeat_num =
	((aw8624_dts_data.aw8624_parameter1 * aw8624->f0_pre * 100) / 5000 -
	100 * (f0_pre_num + f0_wait_num + 2)) / (f0_trace_num+f0_wait_num + 2);

	if (f0_repeat_num % 100 >= 50)
		f0_repeat_num = f0_repeat_num / 100 + 1;
	else
		f0_repeat_num = f0_repeat_num / 100;

	aw8624_i2c_write(aw8624,
			AW8624_REG_NUM_F0_1,
			(f0_pre_num<<4)|(f0_wait_num<<0));
	aw8624_i2c_write(aw8624,
			AW8624_REG_NUM_F0_2,
			(char)(f0_repeat_num<<0));
	aw8624_i2c_write(aw8624,
			AW8624_REG_NUM_F0_3,
			(f0_trace_num<<0));

	/* clear aw8624 interrupt */
	ret = aw8624_i2c_read(aw8624, AW8624_REG_SYSINT, &reg_val);

	/* play go and start f0 calibration */
	aw8624_haptic_play_go(aw8624, true);

	/* f0 trace time */
	t_f0_trace_ms = t_f0_ms *
	(f0_pre_num + f0_wait_num + 2 + (f0_trace_num+f0_wait_num + 2)) + 50;
	mdelay(t_f0_trace_ms + 10);

	aw8624_haptic_read_f0(aw8624);
	aw8624_haptic_read_beme(aw8624);
	timer_val = aw8624_dts_data.aw8624_parameter1;
	INIT_DELAYED_WORK(&aw8624->stop_work, aw8624_stop_work_routine);
	schedule_delayed_work(&aw8624->stop_work, msecs_to_jiffies(timer_val));

	return ret;
}

static int aw8624_haptic_f0_calibration(struct aw8624 *aw8624)
{
	int ret = 0;
	unsigned char reg_val = 0;
	unsigned int f0_limit = 0;
	char f0_cali_lra = 0;
	int f0_cali_step = 0;
	/*int f0_dft_step = 0;*/

	aw8624->f0_cali_flag = AW8624_HAPTIC_CALI_F0;

	aw8624_i2c_write(aw8624, AW8624_REG_TRIM_LRA, 0x00);
	if (aw8624_haptic_get_f0(aw8624)) {
		pr_err("%s get f0 error, user defafult f0\n", __func__);
	} else {
		 /* max and min limit */
		f0_limit = aw8624->f0;
		if (aw8624->f0*100 < aw8624_dts_data.aw8624_f0_pre *
		(100-aw8624_dts_data.aw8624_f0_cali_percen)) {
			f0_limit = aw8624_dts_data.aw8624_f0_pre;
		}
		if (aw8624->f0*100 > aw8624_dts_data.aw8624_f0_pre *
		(100+aw8624_dts_data.aw8624_f0_cali_percen)) {
			f0_limit = aw8624_dts_data.aw8624_f0_pre;
		}
		/* calculate cali step */
		f0_cali_step =
		100000*((int)f0_limit-(int)aw8624->f0_pre)/((int)f0_limit*25);

		if (f0_cali_step >= 0) {  /*f0_cali_step >= 0*/
			if (f0_cali_step % 10 >= 5) {
				f0_cali_step = f0_cali_step/10 + 1 +
					(aw8624->chipid_flag == 1 ? 32 : 16);
			} else {
				f0_cali_step = f0_cali_step/10 +
					(aw8624->chipid_flag == 1 ? 32 : 16);
			}
		} else { /*f0_cali_step < 0*/
			if (f0_cali_step % 10 <= -5) {
				f0_cali_step =
					(aw8624->chipid_flag == 1 ? 32 : 16) +
					(f0_cali_step/10 - 1);
			} else {
				f0_cali_step =
					(aw8624->chipid_flag == 1 ? 32 : 16) +
					f0_cali_step/10;
			}
		}

		if (aw8624->chipid_flag == 1) {
			if (f0_cali_step > 31)
				f0_cali_lra = (char)f0_cali_step - 32;
			else
				f0_cali_lra = (char)f0_cali_step + 32;
		} else {
			if (f0_cali_step < 16 ||
			(f0_cali_step > 31 && f0_cali_step < 48)) {
				f0_cali_lra = (char)f0_cali_step + 16;
			} else {
				f0_cali_lra = (char)f0_cali_step - 16;
			}
		}
		/* update cali step */
		aw8624_i2c_write(aw8624,
				AW8624_REG_TRIM_LRA,
				(char)f0_cali_lra);
		aw8624_i2c_read(aw8624,
				AW8624_REG_TRIM_LRA,
				&reg_val);
		pr_info("%s final trim_lra=0x%02x\n", __func__, reg_val);
	}

	/* if (aw8624_haptic_get_f0(aw8624)) { */
	/* pr_err("%s get f0 error, user defafult f0\n", __func__); */
	/* } */

	aw8624_haptic_play_mode(aw8624, AW8624_HAPTIC_STANDBY_MODE);
	aw8624_haptic_play_mode(aw8624, AW8624_HAPTIC_RAM_MODE);
	aw8624_haptic_stop(aw8624);

	return ret;
}

/*****************************************************
 *
 * haptic fops
 *
 *****************************************************/
static int aw8624_file_open(struct inode *inode, struct file *file)
{
	if (!try_module_get(THIS_MODULE))
		return -ENODEV;

	file->private_data = (void *)g_aw8624;

	return 0;
}

static int aw8624_file_release(struct inode *inode, struct file *file)
{
	file->private_data = (void *)NULL;

	module_put(THIS_MODULE);

	return 0;
}

static long aw8624_file_unlocked_ioctl(struct file *file,
					unsigned int cmd, unsigned long arg)
{
	struct aw8624 *aw8624 = (struct aw8624 *)file->private_data;
	int ret = 0;

	dev_info(aw8624->dev, "%s: cmd=0x%x, arg=0x%lx\n",
		__func__, cmd, arg);

	mutex_lock(&aw8624->lock);

	if (_IOC_TYPE(cmd) != AW8624_HAPTIC_IOCTL_MAGIC) {
		dev_err(aw8624->dev, "%s: cmd magic err\n",
				__func__);
		return -EINVAL;
	}

	switch (cmd) {
	default:
		dev_err(aw8624->dev, "%s, unknown cmd\n", __func__);
		break;
	}

	mutex_unlock(&aw8624->lock);

	return ret;
}

static ssize_t aw8624_file_read(struct file *filp,
				char *buff, size_t len, loff_t *offset)
{
	struct aw8624 *aw8624 = (struct aw8624 *)filp->private_data;
	int ret = 0;
	int i = 0;
	unsigned char reg_val = 0;
	unsigned char *pbuff = NULL;

	mutex_lock(&aw8624->lock);

	dev_info(aw8624->dev, "%s: len=%zu\n", __func__, len);

	switch (aw8624->fileops.cmd) {
	case AW8624_HAPTIC_CMD_READ_REG:
		pbuff = kzalloc(len, GFP_KERNEL);
		if (pbuff != NULL) {
			for (i = 0; i < len; i++) {
				aw8624_i2c_read(aw8624,
						aw8624->fileops.reg+i,
						&reg_val);
				pbuff[i] = reg_val;
			}
			ret = copy_to_user(buff, pbuff, len);
			if (ret) {
				dev_err
				(aw8624->dev, "%s: copy to user fail\n",
				__func__);
			}
			kfree(pbuff);
		} else {
			dev_err(aw8624->dev, "%s: alloc memory fail\n",
				__func__);
		}
		break;
	default:
		dev_err(aw8624->dev, "%s, unknown cmd %d\n",
			__func__, aw8624->fileops.cmd);
		break;
	}

	mutex_unlock(&aw8624->lock);

	for (i = 0; i < len; i++) {
		dev_info(aw8624->dev, "%s: buff[%d]=0x%02x\n",
				__func__, i, buff[i]);
	}

	return len;
}

static ssize_t aw8624_file_write(struct file *filp,
				const char *buff, size_t len, loff_t *off)
{
	struct aw8624 *aw8624 = (struct aw8624 *)filp->private_data;
	int i = 0;
	int ret = 0;
	unsigned char *pbuff = NULL;


	mutex_lock(&aw8624->lock);

	aw8624->fileops.cmd = buff[0];

	switch (aw8624->fileops.cmd) {
	case AW8624_HAPTIC_CMD_READ_REG:
		if (len == 2) {
			aw8624->fileops.reg = buff[1];
		} else {
			dev_err(aw8624->dev,
			"%s: read cmd len %zu err\n",
			__func__, len);
		}
		break;
	case AW8624_HAPTIC_CMD_WRITE_REG:
		if (len > 2) {
			pbuff = kzalloc(len, GFP_KERNEL);
			if (pbuff != NULL) {
				ret = copy_from_user(pbuff, buff, len);
				if (ret) {
					dev_err(aw8624->dev,
					"%s: copy from user fail\n",
					__func__);
				} else {
					for (i = 0; i < len - 2; i++) {
						dev_info
						(aw8624->dev,
						"%s: write reg0x%x = 0x%x\n",
						__func__,
						pbuff[1]+i, pbuff[i+2]);
						aw8624_i2c_write(aw8624,
							pbuff[1]+i,
							pbuff[2+i]);
					}
				}
				kfree(pbuff);
			} else {
				dev_err(aw8624->dev,
					"%s: alloc memory fail\n",
					__func__);
			}
		} else {
			dev_err(aw8624->dev,
				"%s: write cmd len %zu err\n",
				__func__, len);
		}
		break;
	default:
		dev_err(aw8624->dev,
			"%s, unknown cmd %d\n",
			__func__, aw8624->fileops.cmd);
		break;
	}

	mutex_unlock(&aw8624->lock);

	return len;
}

static const struct file_operations fops = {
	.owner = THIS_MODULE,
	.read = aw8624_file_read,
	.write = aw8624_file_write,
	.unlocked_ioctl = aw8624_file_unlocked_ioctl,
	.open = aw8624_file_open,
	.release = aw8624_file_release,
};

static struct miscdevice aw8624_haptic_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = AW8624_HAPTIC_NAME,
	.fops = &fops,
};

static int aw8624_haptic_init(struct aw8624 *aw8624)
{
	int ret = 0;
	unsigned char i = 0;
	unsigned char reg_val = 0, reg_flag = 0;
	unsigned char bemf_config = 0;

	ret = misc_register(&aw8624_haptic_misc);
	if (ret) {
		dev_err(aw8624->dev, "%s: misc fail: %d\n", __func__, ret);
		return ret;
	}

	/* haptic audio */
	aw8624->haptic_audio.delay_val = 23;
	aw8624->haptic_audio.timer_val = 23;

	hrtimer_init(&aw8624->haptic_audio.timer,
			CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	aw8624->haptic_audio.timer.function = aw8624_haptic_audio_timer_func;
	INIT_WORK(&aw8624->haptic_audio.work, aw8624_haptic_audio_work_routine);

	mutex_init(&aw8624->haptic_audio.lock);

	/* haptic init */
	mutex_lock(&aw8624->lock);
	ret = aw8624_i2c_read(aw8624, AW8624_REG_EF_RDATAH, &reg_flag);
	if ((ret >= 0) && ((reg_flag & 0x1) == 1)) {
		aw8624->chipid_flag = 1;
	} else {
		dev_err(aw8624->dev,
		"%s: to read register AW8624_REG_EF_RDATAH: %d\n",
		__func__, ret);
	}

	aw8624->activate_mode = aw8624_dts_data.aw8624_mode;
	ret = aw8624_i2c_read(aw8624, AW8624_REG_WAVSEQ1, &reg_val);
	aw8624->index = reg_val & 0x7F;
	ret = aw8624_i2c_read(aw8624, AW8624_REG_DATDBG, &reg_val);
	aw8624->gain = reg_val & 0xFF;
	for (i = 0; i < AW8624_SEQUENCER_SIZE; i++) {
		ret = aw8624_i2c_read(aw8624, AW8624_REG_WAVSEQ1+i, &reg_val);
		aw8624->seq[i] = reg_val;
	}

	aw8624_haptic_play_mode(aw8624, AW8624_HAPTIC_STANDBY_MODE);
	aw8624_haptic_set_pwm(aw8624, AW8624_PWM_24K);

	aw8624_haptic_swicth_motorprotect_config(aw8624, 0x0, 0x0);
	aw8624_haptic_vbat_mode(aw8624, AW8624_HAPTIC_VBAT_HW_COMP_MODE);
	mutex_unlock(&aw8624->lock);

	/* f0 calibration */
	aw8624->f0_pre = aw8624_dts_data.aw8624_f0_pre;
	aw8624->cont_drv_lvl = aw8624_dts_data.aw8624_cont_drv_lvl;
	aw8624->cont_drv_lvl_ov = aw8624_dts_data.aw8624_cont_drv_lvl_ov;
	aw8624->cont_td = aw8624_dts_data.aw8624_cont_td;
	aw8624->cont_zc_thr = aw8624_dts_data.aw8624_cont_zc_thr;
	aw8624->cont_num_brk = aw8624_dts_data.aw8624_cont_num_brk;
	aw8624->ram_vbat_comp = AW8624_HAPTIC_RAM_VBAT_COMP_ENABLE;
	mutex_lock(&aw8624->lock);
	aw8624_i2c_write_bits(aw8624, AW8624_REG_R_SPARE,
		AW8624_BIT_R_SPARE_MASK, AW8624_BIT_R_SPARE_ENABLE);
	aw8624_haptic_f0_calibration(aw8624);
	mutex_unlock(&aw8624->lock);

	/*brake*/
	mutex_lock(&aw8624->lock);
	aw8624_i2c_write(aw8624,
			AW8624_REG_SW_BRAKE,
			aw8624_dts_data.aw8624_sw_brake);
	aw8624_i2c_write(aw8624, AW8624_REG_THRS_BRA_END, 0x00);
	aw8624_i2c_write_bits(aw8624,
			AW8624_REG_WAVECTRL,
			AW8624_BIT_WAVECTRL_NUM_OV_DRIVER_MASK,
			AW8624_BIT_WAVECTRL_NUM_OV_DRIVER);
	aw8624->f0_value = 20000 / aw8624_dts_data.aw8624_f0_pre + 1;
	/* zero cross */
	aw8624_i2c_write(aw8624,
			AW8624_REG_ZC_THRSH_H,
			(unsigned char)(aw8624->cont_zc_thr>>8));
	aw8624_i2c_write(aw8624,
			AW8624_REG_ZC_THRSH_L,
			(unsigned char)(aw8624->cont_zc_thr>>0));
	aw8624_i2c_write(aw8624,
			AW8624_REG_TSET,
			aw8624_dts_data.aw8624_tset);

	/* bemf */
	bemf_config = aw8624_dts_data.aw8624_bemf_config[0];
	aw8624_i2c_write(aw8624, AW8624_REG_BEMF_VTHH_H, bemf_config);
	bemf_config = aw8624_dts_data.aw8624_bemf_config[1];
	aw8624_i2c_write(aw8624, AW8624_REG_BEMF_VTHH_L, bemf_config);
	bemf_config = aw8624_dts_data.aw8624_bemf_config[2];
	aw8624_i2c_write(aw8624, AW8624_REG_BEMF_VTHL_H, bemf_config);
	bemf_config = aw8624_dts_data.aw8624_bemf_config[3];
	aw8624_i2c_write(aw8624, AW8624_REG_BEMF_VTHL_L, bemf_config);
	mutex_unlock(&aw8624->lock);

	return ret;
}



/*****************************************************
 *
 * vibrator
 *
 *****************************************************/
static void aw8624_vibrator_enable(struct led_classdev *dev,
				enum led_brightness value)
{
	struct aw8624 *aw8624 = container_of(dev, struct aw8624, to_dev);

	pr_info("%s: time value = %d\n", __func__, value);

	mutex_lock(&aw8624->lock);
	hrtimer_cancel(&aw8624->timer);

	aw8624->state = value;
	aw8624->duration = value;

	/* aw8624_haptic_stop(aw8624); */

	mutex_unlock(&aw8624->lock);

	schedule_work(&aw8624->vibrator_work);
}

static ssize_t aw8624_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *to_dev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(to_dev, struct aw8624, to_dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", aw8624->state);
}

static ssize_t aw8624_state_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t aw8624_duration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *to_dev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(to_dev, struct aw8624, to_dev);

	ktime_t time_rem;
	s64 time_ms = 0;

	if (hrtimer_active(&aw8624->timer)) {
		time_rem = hrtimer_get_remaining(&aw8624->timer);
		time_ms = ktime_to_ms(time_rem);
	}

	return snprintf(buf, PAGE_SIZE, "%lld\n", time_ms);
}

static ssize_t aw8624_duration_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct led_classdev *to_dev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(to_dev, struct aw8624, to_dev);

	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	/* setting 0 on duration is NOP for now */
	if (val <= 0)
		return count;

	if (val <= aw8624->f0_value)
		val = aw8624->f0_value;

	aw8624->duration = val;
	return count;
}

static ssize_t aw8624_activate_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *to_dev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(to_dev, struct aw8624, to_dev);

	/* For now nothing to show */
	return snprintf(buf, PAGE_SIZE, "%d\n", aw8624->state);
}

static ssize_t aw8624_activate_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct led_classdev *to_dev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(to_dev, struct aw8624, to_dev);

	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	if (val != 0 && val != 1)
		return count;

	pr_info("%s: value=%d\n", __func__, val);

	mutex_lock(&aw8624->lock);
	hrtimer_cancel(&aw8624->timer);

	aw8624->state = val;

	/*aw8624_haptic_stop(aw8624);*/

	mutex_unlock(&aw8624->lock);
	schedule_work(&aw8624->vibrator_work);

	return count;
}

static ssize_t aw8624_activate_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{

	struct led_classdev *to_dev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(to_dev, struct aw8624, to_dev);


	return snprintf(buf, PAGE_SIZE, "activate_mode=%d\n",
			aw8624->activate_mode);
}

static ssize_t aw8624_activate_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct led_classdev *to_dev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(to_dev, struct aw8624, to_dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	mutex_lock(&aw8624->lock);
	aw8624->activate_mode = val;
	mutex_unlock(&aw8624->lock);
	return count;
}


static ssize_t aw8624_index_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *to_dev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(to_dev, struct aw8624, to_dev);
	unsigned char reg_val = 0;

	aw8624_i2c_read(aw8624, AW8624_REG_WAVSEQ1, &reg_val);
	aw8624->index = reg_val;

	return snprintf(buf, PAGE_SIZE, "%d\n", aw8624->index);
}

static ssize_t aw8624_index_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct led_classdev *to_dev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(to_dev, struct aw8624, to_dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	pr_info("%s: value=%d\n", __func__, val);

	mutex_lock(&aw8624->lock);
	aw8624->index = val;
	aw8624_haptic_set_repeat_wav_seq(aw8624, aw8624->index);
	mutex_unlock(&aw8624->lock);
	return count;
}


static ssize_t aw8624_gain_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *to_dev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(to_dev, struct aw8624, to_dev);

	return snprintf(buf, PAGE_SIZE, "0x%02x\n", aw8624->gain);
}

static ssize_t aw8624_gain_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct led_classdev *to_dev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(to_dev, struct aw8624, to_dev);

	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	pr_info("%s: value=%d\n", __func__, val);

	mutex_lock(&aw8624->lock);
	aw8624->gain = val;
	aw8624_haptic_set_gain(aw8624, aw8624->gain);
	mutex_unlock(&aw8624->lock);
	return count;
}

static ssize_t aw8624_seq_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *to_dev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(to_dev, struct aw8624, to_dev);

	size_t count = 0;
	unsigned char i = 0;
	unsigned char reg_val = 0;

	for (i = 0; i < AW8624_SEQUENCER_SIZE; i++) {
		aw8624_i2c_read(aw8624, AW8624_REG_WAVSEQ1+i, &reg_val);
		count += snprintf(buf+count, PAGE_SIZE-count,
			"seq%d: 0x%02x\n", i+1, reg_val);
		aw8624->seq[i] |= reg_val;
	}
	return count;
}

static ssize_t aw8624_seq_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct led_classdev *to_dev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(to_dev, struct aw8624, to_dev);
	unsigned int databuf[2] = {0, 0};

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		pr_info("%s: seq%d=0x%x\n",
			__func__, databuf[0], databuf[1]);
		mutex_lock(&aw8624->lock);
		aw8624->seq[databuf[0]] = (unsigned char)databuf[1];
		aw8624_haptic_set_wav_seq(aw8624, (unsigned char)databuf[0],
			aw8624->seq[databuf[0]]);
		mutex_unlock(&aw8624->lock);
	}
	return count;
}

static ssize_t aw8624_loop_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *to_dev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(to_dev, struct aw8624, to_dev);

	size_t count = 0;
	unsigned char i = 0;
	unsigned char reg_val = 0;

	for (i = 0; i < AW8624_SEQUENCER_LOOP_SIZE; i++) {
		aw8624_i2c_read(aw8624, AW8624_REG_WAVLOOP1+i, &reg_val);
		aw8624->loop[i*2+0] = (reg_val>>4)&0x0F;
		aw8624->loop[i*2+1] = (reg_val>>0)&0x0F;

		count += snprintf(buf+count, PAGE_SIZE-count,
			"seq%d loop: 0x%02x\n", i*2+1, aw8624->loop[i*2+0]);
		count += snprintf(buf+count, PAGE_SIZE-count,
			"seq%d loop: 0x%02x\n", i*2+2, aw8624->loop[i*2+1]);
	}
	return count;
}

static ssize_t aw8624_loop_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct led_classdev *to_dev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(to_dev, struct aw8624, to_dev);

	unsigned int databuf[2] = {0, 0};

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		pr_info("%s: seq%d loop=0x%x\n",
			__func__, databuf[0], databuf[1]);
		mutex_lock(&aw8624->lock);
		aw8624->loop[databuf[0]] = (unsigned char)databuf[1];
		aw8624_haptic_set_wav_loop(aw8624, (unsigned char)databuf[0],
			aw8624->loop[databuf[0]]);
		mutex_unlock(&aw8624->lock);
	}

	return count;
}

static ssize_t aw8624_reg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *to_dev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(to_dev, struct aw8624, to_dev);
	ssize_t len = 0;
	unsigned char i = 0;
	unsigned char reg_val = 0;

	for (i = 0; i < AW8624_REG_MAX; i++) {
		if (!(aw8624_reg_access[i]&REG_RD_ACCESS))
			continue;
		aw8624_i2c_read(aw8624, i, &reg_val);
		len += snprintf(buf+len, PAGE_SIZE-len, "reg:0x%02x=0x%02x\n",
				i, reg_val);
	}
	return len;
}

static ssize_t aw8624_reg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct led_classdev *to_dev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(to_dev, struct aw8624, to_dev);

	unsigned int databuf[2] = {0, 0};

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		aw8624_i2c_write(aw8624,
				(unsigned char)databuf[0],
				(unsigned char)databuf[1]);
	}

	return count;
}

static ssize_t aw8624_rtp_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct led_classdev *to_dev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(to_dev, struct aw8624, to_dev);
	ssize_t len = 0;

	len += snprintf(buf+len, PAGE_SIZE-len, "rtp play: %d\n",
			aw8624->rtp_cnt);

	return len;
}

static ssize_t aw8624_rtp_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct led_classdev *to_dev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(to_dev, struct aw8624, to_dev);

	unsigned int val = 0;
	int rc = 0;

	if (!(aw8624->IsUsedIRQ))
		return rc;
	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	aw8624_haptic_stop(aw8624);
	aw8624_haptic_set_rtp_aei(aw8624, false);
	aw8624_interrupt_clear(aw8624);
	if (val < (sizeof(aw8624_rtp_name)/AW8624_RTP_NAME_MAX)) {
		aw8624->rtp_file_num = val;
		if (val)
			schedule_work(&aw8624->rtp_work);
	} else {
		pr_err("%s: rtp_file_num 0x%02x over max value\n",
			__func__, aw8624->rtp_file_num);
	}

	return count;
}


static ssize_t aw8624_ram_update_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;

	len += snprintf(buf+len, PAGE_SIZE-len, "sram update mode\n");
	return len;
}

static ssize_t aw8624_ram_update_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct led_classdev *to_dev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(to_dev, struct aw8624, to_dev);

	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	if (val)
		aw8624_ram_update(aw8624);

	return count;
}

static ssize_t aw8624_f0_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct led_classdev *to_dev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(to_dev, struct aw8624, to_dev);

	ssize_t len = 0;

	mutex_lock(&aw8624->lock);
	aw8624->f0_cali_flag = AW8624_HAPTIC_LRA_F0;
	aw8624_haptic_get_f0(aw8624);
	mutex_unlock(&aw8624->lock);
	len += snprintf(buf+len, PAGE_SIZE-len, "aw8624 lra f0 = %d\n",
			aw8624->f0);
	return len;
}

static ssize_t aw8624_f0_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	return count;
}


static ssize_t aw8624_cali_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct led_classdev *to_dev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(to_dev, struct aw8624, to_dev);
	ssize_t len = 0;

	mutex_lock(&aw8624->lock);
	aw8624->f0_cali_flag = AW8624_HAPTIC_CALI_F0;
	aw8624_haptic_get_f0(aw8624);
	mutex_unlock(&aw8624->lock);

	len += snprintf(buf+len, PAGE_SIZE-len, "aw8624 f0 = %d\n", aw8624->f0);
	return len;
}

static ssize_t
aw8624_cali_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct led_classdev *to_dev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(to_dev, struct aw8624, to_dev);

	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	if (val) {
		mutex_lock(&aw8624->lock);
		aw8624_haptic_f0_calibration(aw8624);
		mutex_unlock(&aw8624->lock);
	}
	return count;
}

static ssize_t
aw8624_cont_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct led_classdev *to_dev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(to_dev, struct aw8624, to_dev);
	ssize_t len = 0;

	aw8624_haptic_read_cont_f0(aw8624);
	len += snprintf(buf+len, PAGE_SIZE-len, "aw8624 f0 = %d\n", aw8624->f0);
	return len;
}

static ssize_t
aw8624_cont_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct led_classdev *to_dev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(to_dev, struct aw8624, to_dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	aw8624_haptic_stop(aw8624);

	if (val)
		aw8624_haptic_cont(aw8624);
	return count;
}


static ssize_t
aw8624_cont_td_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct led_classdev *to_dev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(to_dev, struct aw8624, to_dev);
	ssize_t len = 0;

	len += snprintf(buf+len, PAGE_SIZE-len,
			"aw8624 cont delay time = 0x%04x\n",
			aw8624->cont_td);
	return len;
}

static ssize_t
aw8624_cont_td_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct led_classdev *to_dev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(to_dev, struct aw8624, to_dev);
	int err, val;

	err = kstrtoint(buf, 16, &val);
	if (err != 0) {
		pr_err("%s format not match!", __func__);
		return count;
	}

	aw8624->cont_td = val;
	aw8624_i2c_write(aw8624,
			AW8624_REG_TD_H,
			(unsigned char)(val >> 8));
	aw8624_i2c_write(aw8624,
			AW8624_REG_TD_L,
			(unsigned char)(val >> 0));

	return count;
}

static ssize_t
aw8624_cont_drv_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct led_classdev *to_dev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(to_dev, struct aw8624, to_dev);
	ssize_t len = 0;

	len += snprintf(buf+len, PAGE_SIZE-len,
			"aw8624 cont drv level = 0x%02x\n",
			aw8624->cont_drv_lvl);
	len += snprintf(buf+len, PAGE_SIZE-len,
			"aw8624 cont drv level overdrive= 0x%02x\n",
			aw8624->cont_drv_lvl_ov);
	return len;
}

static ssize_t
aw8624_cont_drv_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct led_classdev *to_dev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(to_dev, struct aw8624, to_dev);
	unsigned int databuf[2] = {0, 0};

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		aw8624->cont_drv_lvl = databuf[0];
		aw8624_i2c_write(aw8624,
				AW8624_REG_DRV_LVL,
				aw8624->cont_drv_lvl);
		aw8624->cont_drv_lvl_ov = databuf[1];
		aw8624_i2c_write(aw8624,
				AW8624_REG_DRV_LVL_OV,
				aw8624->cont_drv_lvl_ov);
	}
	return count;
}

static ssize_t
aw8624_cont_num_brk_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct led_classdev *to_dev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(to_dev, struct aw8624, to_dev);
	ssize_t len = 0;

	len += snprintf(buf+len, PAGE_SIZE-len,
			"aw8624 cont break num = 0x%02x\n",
			aw8624->cont_num_brk);
	return len;
}

static ssize_t
aw8624_cont_num_brk_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct led_classdev *to_dev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(to_dev, struct aw8624, to_dev);
	int err, val;

	err = kstrtoint(buf, 16, &val);
	if (err != 0) {
		pr_err("%s format not match!", __func__);
		return count;
	}

	aw8624->cont_num_brk = val;
	if (aw8624->cont_num_brk > 7)
		aw8624->cont_num_brk = 7;

	aw8624_i2c_write_bits(aw8624, AW8624_REG_BEMF_NUM,
		AW8624_BIT_BEMF_NUM_BRK_MASK, aw8624->cont_num_brk);

	return count;
}

static ssize_t
aw8624_cont_zc_thr_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct led_classdev *to_dev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(to_dev, struct aw8624, to_dev);
	ssize_t len = 0;

	len += snprintf(buf+len, PAGE_SIZE-len,
			"aw8624 cont zero cross thr = 0x%04x\n",
			aw8624->cont_zc_thr);
	return len;
}

static ssize_t
aw8624_cont_zc_thr_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct led_classdev *to_dev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(to_dev, struct aw8624, to_dev);
	int err, val;

	err = kstrtoint(buf, 0, &val);
	if (err != 0) {
		pr_err("%s format not match!", __func__);
		return count;
	}
	pr_info("%s: val=%d\n", __func__, val);
	if (val == 1) {
		aw8624_i2c_write(aw8624,
				AW8624_REG_ZC_THRSH_H,
				(unsigned char)(val >> 8));
		aw8624_i2c_write(aw8624,
				AW8624_REG_ZC_THRSH_L,
				(unsigned char)(val >> 0));
	}
	return count;
}

static ssize_t
aw8624_vbat_monitor_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct led_classdev *to_dev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(to_dev, struct aw8624, to_dev);
	ssize_t len = 0;
	unsigned int vbat = 0;

	mutex_lock(&aw8624->lock);
	vbat = aw8624_vbat_monitor_detector(aw8624);
	mutex_unlock(&aw8624->lock);
	len += snprintf(buf+len, PAGE_SIZE-len, "vbat=%dmV\n", vbat);

	return len;
}

static ssize_t
aw8624_vbat_monitor_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	return count;
}

static ssize_t
aw8624_lra_resistance_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct led_classdev *to_dev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(to_dev, struct aw8624, to_dev);
	ssize_t len = 0;
	unsigned int r_lra = 0;

	r_lra = aw8624_lra_resistance_detector(aw8624);

	len += snprintf(buf+len, PAGE_SIZE-len, "r_lra=%dmohm\n", r_lra);
	return len;
}


static ssize_t
aw8624_lra_resistance_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	return count;
}

static ssize_t
aw8624_trig_boost_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	ssize_t len = 0;
	return len;
}
static ssize_t
aw8624_trig_boost_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{

	struct led_classdev *to_dev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(to_dev, struct aw8624, to_dev);
	unsigned int val = 0;
	int rc = 0;

	if (!(aw8624->IsUsedIRQ))
		return rc;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	mutex_lock(&aw8624->lock);
	aw8624_haptic_stop(aw8624);
	aw8624_haptic_trig1_config(aw8624, val);
	mutex_unlock(&aw8624->lock);

	return count;
}

static ssize_t
aw8624_prctmode_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{

	struct led_classdev *to_dev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(to_dev, struct aw8624, to_dev);
	ssize_t len = 0;
	unsigned char reg_val = 0;

	aw8624_i2c_read(aw8624, AW8624_REG_RLDET, &reg_val);

	len += snprintf(buf+len, PAGE_SIZE-len, "prctmode=%d\n", reg_val&0x20);
	return len;
}


static ssize_t
aw8624_prctmode_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{

	struct led_classdev *to_dev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(to_dev, struct aw8624, to_dev);
	unsigned int databuf[2] = {0, 0};
	unsigned int addr = 0;
	unsigned int val = 0;

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		addr = databuf[0];
		val = databuf[1];
		mutex_lock(&aw8624->lock);
		aw8624_haptic_swicth_motorprotect_config(aw8624, addr, val);
		mutex_unlock(&aw8624->lock);
	}
	return count;
}

static ssize_t
aw8624_ram_vbat_comp_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct led_classdev *to_dev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(to_dev, struct aw8624, to_dev);
	ssize_t len = 0;

	len += snprintf(buf+len, PAGE_SIZE-len,
			"ram_vbat_comp=%d\n",
			aw8624->ram_vbat_comp);

	return len;
}


static ssize_t
aw8624_ram_vbat_comp_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct led_classdev *to_dev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(to_dev, struct aw8624, to_dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return rc;

	mutex_lock(&aw8624->lock);
	if (val)
		aw8624->ram_vbat_comp = AW8624_HAPTIC_RAM_VBAT_COMP_ENABLE;
	else
		aw8624->ram_vbat_comp = AW8624_HAPTIC_RAM_VBAT_COMP_DISABLE;

	mutex_unlock(&aw8624->lock);

	return count;
}

static ssize_t
aw8624_haptic_audio_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct led_classdev *to_dev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(to_dev, struct aw8624, to_dev);

	ssize_t len = 0;

	len += snprintf(buf+len, PAGE_SIZE-len, "%d\n",
			aw8624->haptic_audio.cnt);
	return len;
}

static ssize_t
aw8624_haptic_audio_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct led_classdev *to_dev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(to_dev, struct aw8624, to_dev);

	unsigned int databuf[6] = {0};

	if (sscanf(buf, "%d %d %d %d %d %d", &databuf[0], &databuf[1],
	&databuf[2], &databuf[3], &databuf[4], &databuf[5]) == 6) {
		pr_debug
		("%s: cnt=%d, cmd=%d, play=%d, wavseq=%d, loop=%d, gain=%d\n",
			__func__,
			databuf[0], databuf[1], databuf[2],
			databuf[3], databuf[4], databuf[5]);
		mutex_lock(&aw8624->haptic_audio.lock);
		aw8624->haptic_audio.cnt = (unsigned char)databuf[0];
		aw8624->haptic_audio.ctr[aw8624->haptic_audio.cnt].cmd =
						(unsigned char)databuf[1];
		aw8624->haptic_audio.ctr[aw8624->haptic_audio.cnt].play =
						(unsigned char)databuf[2];
		aw8624->haptic_audio.ctr[aw8624->haptic_audio.cnt].wavseq =
						(unsigned char)databuf[3];
		aw8624->haptic_audio.ctr[aw8624->haptic_audio.cnt].loop =
						(unsigned char)databuf[4];
		aw8624->haptic_audio.ctr[aw8624->haptic_audio.cnt].gain =
						(unsigned char)databuf[5];
		mutex_unlock(&aw8624->haptic_audio.lock);

		if (aw8624->haptic_audio.ctr[aw8624->haptic_audio.cnt].cmd ==
		0xff) {
			pr_info("%s: haptic_audio stop\n", __func__);
			if (hrtimer_active(&aw8624->haptic_audio.timer)) {
				pr_info
				("%s: cancel haptic_audio_timer\n", __func__);
				hrtimer_cancel(&aw8624->haptic_audio.timer);
				aw8624->haptic_audio.cnt = 0;
			}
		} else {
			if (hrtimer_active(&aw8624->haptic_audio.timer)) {
			} else {
				pr_info
				("%s: start haptic_audio_timer\n", __func__);
				hrtimer_start(&aw8624->haptic_audio.timer,
				ktime_set
				(aw8624->haptic_audio.delay_val/1000,
				(aw8624->haptic_audio.delay_val%1000)*1000000),
				HRTIMER_MODE_REL);
			}
		}
	}
	return count;
}

static ssize_t
aw8624_haptic_audio_time_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{

	struct led_classdev *to_dev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(to_dev, struct aw8624, to_dev);
	ssize_t len = 0;

	len += snprintf(buf+len, PAGE_SIZE-len,
			"haptic_audio.delay_val=%dus\n",
			aw8624->haptic_audio.delay_val);
	len += snprintf(buf+len, PAGE_SIZE-len,
			"haptic_audio.timer_val=%dus\n",
			aw8624->haptic_audio.timer_val);
	return len;
}

static ssize_t
aw8624_haptic_audio_time_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{

	struct led_classdev *to_dev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(to_dev, struct aw8624, to_dev);

	unsigned int databuf[2] = {0};

	if (sscanf(buf, "%d %d", &databuf[0], &databuf[1]) == 2) {
		aw8624->haptic_audio.delay_val = databuf[0];
		aw8624->haptic_audio.timer_val = databuf[1];
	}
	return count;
}

static DEVICE_ATTR(state, 0664, aw8624_state_show, aw8624_state_store);
static DEVICE_ATTR(duration, 0664, aw8624_duration_show, aw8624_duration_store);
static DEVICE_ATTR(activate, 0664, aw8624_activate_show, aw8624_activate_store);
static DEVICE_ATTR(activate_mode, 0664,
		aw8624_activate_mode_show, aw8624_activate_mode_store);
static DEVICE_ATTR(index, 0664, aw8624_index_show, aw8624_index_store);
static DEVICE_ATTR(gain, 0664, aw8624_gain_show, aw8624_gain_store);
static DEVICE_ATTR(seq, 0664, aw8624_seq_show, aw8624_seq_store);
static DEVICE_ATTR(loop, 0664, aw8624_loop_show, aw8624_loop_store);
static DEVICE_ATTR(register, 0664, aw8624_reg_show, aw8624_reg_store);
static DEVICE_ATTR(ram_update, 0664,
		aw8624_ram_update_show, aw8624_ram_update_store);
static DEVICE_ATTR(f0, 0664, aw8624_f0_show, aw8624_f0_store);
static DEVICE_ATTR(cali, 0664, aw8624_cali_show, aw8624_cali_store);
static DEVICE_ATTR(cont, 0664, aw8624_cont_show, aw8624_cont_store);
static DEVICE_ATTR(cont_td, 0664, aw8624_cont_td_show, aw8624_cont_td_store);
static DEVICE_ATTR(cont_drv, 0664, aw8624_cont_drv_show, aw8624_cont_drv_store);
static DEVICE_ATTR(cont_num_brk, 0664,
		aw8624_cont_num_brk_show, aw8624_cont_num_brk_store);
static DEVICE_ATTR(cont_zc_thr, 0664,
		aw8624_cont_zc_thr_show, aw8624_cont_zc_thr_store);
static DEVICE_ATTR(vbat_monitor, 0664,
		aw8624_vbat_monitor_show, aw8624_vbat_monitor_store);
static DEVICE_ATTR(lra_resistance, 0664,
		aw8624_lra_resistance_show, aw8624_lra_resistance_store);
static DEVICE_ATTR(prctmode, 0664, aw8624_prctmode_show, aw8624_prctmode_store);
static DEVICE_ATTR(haptic_audio, 0664,
		aw8624_haptic_audio_show, aw8624_haptic_audio_store);
static DEVICE_ATTR(haptic_audio_time, 0664,
		aw8624_haptic_audio_time_show, aw8624_haptic_audio_time_store);
static DEVICE_ATTR(trig_boost, 0664,
		aw8624_trig_boost_show, aw8624_trig_boost_store);
static DEVICE_ATTR(ram_vbat_comp, 0664,
		aw8624_ram_vbat_comp_show, aw8624_ram_vbat_comp_store);
static DEVICE_ATTR(rtp, 0664, aw8624_rtp_show, aw8624_rtp_store);
static struct attribute *aw8624_vibrator_attributes[] = {
	&dev_attr_state.attr,
	&dev_attr_duration.attr,
	&dev_attr_activate.attr,
	&dev_attr_activate_mode.attr,
	&dev_attr_index.attr,
	&dev_attr_gain.attr,
	&dev_attr_seq.attr,
	&dev_attr_loop.attr,
	&dev_attr_register.attr,
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
	&dev_attr_prctmode.attr,
	&dev_attr_haptic_audio.attr,
	&dev_attr_ram_vbat_comp.attr,
	&dev_attr_haptic_audio_time.attr,
	&dev_attr_trig_boost.attr,
	&dev_attr_rtp.attr,
	NULL
};

static struct attribute_group aw8624_vibrator_attribute_group = {
	.attrs = aw8624_vibrator_attributes
};

static enum hrtimer_restart aw8624_vibrator_timer_func(struct hrtimer *timer)
{
	struct aw8624 *aw8624 = container_of(timer, struct aw8624, timer);

	pr_info("%s enter\n", __func__);
	aw8624->state = 0;
	schedule_work(&aw8624->vibrator_work);

	return HRTIMER_NORESTART;
}

static irqreturn_t aw8624_irq(int irq, void *data)
{
	struct aw8624 *aw8624 = data;
	unsigned char reg_val = 0;
	unsigned int buf_len = 0;

	aw8624_i2c_read(aw8624, AW8624_REG_SYSINT, &reg_val);
	/*("%s: reg SYSINT=0x%x\n", __func__, reg_val); */
	/*if (reg_val & AW8624_BIT_SYSINT_UVLI) */
	/*	pr_err("%s chip uvlo int error\n", __func__); */
	/*if (reg_val & AW8624_BIT_SYSINT_OCDI) */
	/*	pr_err("%s chip over current int error\n", __func__); */
	/*if (reg_val & AW8624_BIT_SYSINT_OTI) */
	/*	pr_err("%s chip over temperature int error\n", __func__); */
	/*if (reg_val & AW8624_BIT_SYSINT_DONEI) */
	/*	pr_info("%s chip playback done\n", __func__);*/

	if (reg_val & AW8624_BIT_SYSINT_FF_AEI) {
		/*pr_info("%s: aw8624 rtp fifo almost empty int\n", __func__);*/
		if (aw8624->rtp_init) {
			while ((!aw8624_haptic_rtp_get_fifo_afi(aw8624)) &&
			(aw8624->play_mode == AW8624_HAPTIC_RTP_MODE)) {
				pr_info
				("%s: aw8624 rtp mode fifo update, cnt=%d\n",
				__func__, aw8624->rtp_cnt);
				if ((aw8624_rtp->len-aw8624->rtp_cnt) <
				(aw8624->ram.base_addr>>3)) {
					buf_len =
					aw8624_rtp->len-aw8624->rtp_cnt;
				} else {
					buf_len = (aw8624->ram.base_addr>>3);
				}
				aw8624_i2c_writes(aw8624,
					AW8624_REG_RTP_DATA,
					&aw8624_rtp->data[aw8624->rtp_cnt],
					buf_len);
				aw8624->rtp_cnt += buf_len;
				if (aw8624->rtp_cnt == aw8624_rtp->len) {
					aw8624_haptic_set_rtp_aei(aw8624,
								false);
					aw8624->rtp_cnt = 0;
					aw8624->rtp_init = 0;
					break;
				}
			}
		}
	}

	if (reg_val & AW8624_BIT_SYSINT_FF_AFI)
		pr_info("%s: aw8624 rtp mode fifo full empty\n", __func__);

	if (aw8624->play_mode != AW8624_HAPTIC_RTP_MODE)
		aw8624_haptic_set_rtp_aei(aw8624, false);

	aw8624_i2c_read(aw8624, AW8624_REG_SYSINT, &reg_val);
	aw8624_i2c_read(aw8624, AW8624_REG_SYSST, &reg_val);

	return IRQ_HANDLED;
}

static void aw8624_vibrator_work_routine(struct work_struct *work)
{
	struct aw8624 *aw8624 =
	container_of(work, struct aw8624, vibrator_work);

	pr_info("%s enter\n", __func__);

	mutex_lock(&aw8624->lock);

	aw8624_haptic_stop(aw8624);
	if (aw8624->state) {
		if (aw8624->activate_mode == AW8624_HAPTIC_ACTIVATE_RAM_MODE) {
			aw8624_haptic_ram_config(aw8624);
			aw8624_haptic_ram_vbat_comp(aw8624, true);
			aw8624_haptic_play_repeat_seq(aw8624, true);
	} else if (aw8624->activate_mode == AW8624_HAPTIC_ACTIVATE_CONT_MODE) {
		aw8624_haptic_cont(aw8624);
	} else {
		 /*other mode*/
	}
		/* run ms timer */
		hrtimer_start(&aw8624->timer,
			ktime_set(aw8624->duration / 1000,
			(aw8624->duration % 1000) * 1000000),
			HRTIMER_MODE_REL);
		//wake_lock(&aw8624->wk_lock);
		aw8624->wk_lock_flag = 1;
	} else {
		if (aw8624->wk_lock_flag == 1) {
			//wake_unlock(&aw8624->wk_lock);
			aw8624->wk_lock_flag = 0;
		}
		/*aw8624_haptic_stop(aw8624);*/
	}

	mutex_unlock(&aw8624->lock);
}

static int aw8624_vibrator_init(struct aw8624 *aw8624)
{
	int ret = 0;

	pr_info("%s enter\n", __func__);

	aw8624->to_dev.name = "vibrator";

	aw8624->to_dev.brightness_set = aw8624_vibrator_enable;

	ret = devm_led_classdev_register(&aw8624->i2c->dev, &(aw8624->to_dev));
	if (ret < 0) {
		dev_err(aw8624->dev, "%s: fail to create leds dev\n",
				__func__);
		return ret;
	}
	ret = sysfs_create_group(&aw8624->to_dev.dev->kobj,
				&aw8624_vibrator_attribute_group);
	if (ret < 0) {
		dev_err(aw8624->dev,
			"%s error creating sysfs attr files\n",
			__func__);
		return ret;
	}

	hrtimer_init(&aw8624->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	aw8624->timer.function = aw8624_vibrator_timer_func;
	INIT_WORK(&aw8624->vibrator_work, aw8624_vibrator_work_routine);

	if (aw8624->IsUsedIRQ)
		INIT_WORK(&aw8624->rtp_work, aw8624_rtp_work_routine);

	mutex_init(&aw8624->lock);

	return 0;
}




/******************************************************
 *
 * irq
 *
 ******************************************************/


static void aw8624_interrupt_setup(struct aw8624 *aw8624)
{
	unsigned char reg_val = 0;

	aw8624_i2c_read(aw8624, AW8624_REG_SYSINT, &reg_val);
	pr_info("%s: reg SYSINT=0x%x\n", __func__, reg_val);

	aw8624_i2c_write_bits(aw8624, AW8624_REG_SYSINTM,
		AW8624_BIT_SYSINTM_UVLO_MASK, AW8624_BIT_SYSINTM_UVLO_EN);
	aw8624_i2c_write_bits(aw8624, AW8624_REG_SYSINTM,
		AW8624_BIT_SYSINTM_OCD_MASK, AW8624_BIT_SYSINTM_OCD_EN);
	aw8624_i2c_write_bits(aw8624, AW8624_REG_SYSINTM,
		AW8624_BIT_SYSINTM_OT_MASK, AW8624_BIT_SYSINTM_OT_EN);
}

#if 0
/*
*   schedule_work is low priority.
*   aw8624 rtp mode can`t be interrupted.
*/
static irqreturn_t aw8624_irq(int irq, void *data)
{
	struct aw8624 *aw8624 = (struct aw8624 *)data;

	schedule_work(&aw8624->irq_work);
	return IRQ_HANDLED;
}

#endif
/*****************************************************
 *
 * device tree
 *
 *****************************************************/
static int aw8624_parse_dt(struct device *dev, struct aw8624 *aw8624,
		struct device_node *np) {
	unsigned int val = 0;
	//unsigned int brake_ram_config[24];
	unsigned int brake_cont_config[24];
	unsigned int f0_trace_parameter[4];
	unsigned int bemf_config[4];
	unsigned int duration_time[5];
	unsigned int wavseq_config[16];
	unsigned int wavloop_config[10];
	unsigned int td_brake_config[3];

	aw8624->reset_gpio = of_get_named_gpio(np, "reset-gpio", 0);
	if (aw8624->reset_gpio >= 0) {
		dev_info(dev, "%s: reset gpio provided ok\n", __func__);
	} else {
		aw8624->reset_gpio = -1;
		dev_err(dev,
		"%s: no reset gpio provided, will not HW reset device\n",
		__func__);
		return -1;
	}

	aw8624->irq_gpio = of_get_named_gpio(np, "irq-gpio", 0);
	if (aw8624->irq_gpio < 0) {
		dev_err(dev, "%s: no irq gpio provided.\n", __func__);
		aw8624->IsUsedIRQ = false;
	} else {
		dev_info(dev, "%s: irq gpio provided ok.\n", __func__);
		aw8624->IsUsedIRQ = true;
		aw8624->irq_gpio = -1;
	}
	aw8624->ram_update_delay =
	of_property_read_bool(np, "ram-update-delay");
	val = of_property_read_u32(np, "reg-real-addr", &aw8624->reg_real_addr);
	if (val) {
		dev_info(dev, "configure dts no the reg-real-addr\n");
		aw8624->dts_add_real = false;
	} else {
		aw8624->dts_add_real = true;
	}

	val =
	of_property_read_u32(np, "vib_mode", &aw8624_dts_data.aw8624_mode);
	if (val != 0)
		pr_info("vib_mode not found\n");
	val =
	of_property_read_u32(np, "vib_f0_pre", &aw8624_dts_data.aw8624_f0_pre);
	if (val != 0)
		pr_info("vib_f0_pre not found\n");
	val =
	of_property_read_u32(np, "vib_f0_cali_percen",
				&aw8624_dts_data.aw8624_f0_cali_percen);
	if (val != 0)
		pr_info("vib_f0_cali_percen not found\n");
	val =
	of_property_read_u32(np, "vib_cont_drv_lev",
				&aw8624_dts_data.aw8624_cont_drv_lvl);
	if (val != 0)
		pr_info("vib_cont_drv_lev not found\n");
	val =
	of_property_read_u32(np, "vib_cont_drv_lvl_ov",
				&aw8624_dts_data.aw8624_cont_drv_lvl_ov);
	if (val != 0)
		pr_info("vib_cont_drv_lvl_ov not found\n");
	val =
	of_property_read_u32(np, "vib_cont_td",
				&aw8624_dts_data.aw8624_cont_td);
	if (val != 0)
		pr_info("vib_cont_td not found\n");
	val =
	of_property_read_u32(np, "vib_cont_zc_thr",
				&aw8624_dts_data.aw8624_cont_zc_thr);
	if (val != 0)
		pr_info("vib_cont_zc_thr not found\n");
	val =
	of_property_read_u32(np, "vib_cont_num_brk",
				&aw8624_dts_data.aw8624_cont_num_brk);
	if (val != 0)
		pr_info("vib_cont_num_brk not found\n");
	val =
	of_property_read_u32(np, "vib_f0_coeff",
				&aw8624_dts_data.aw8624_f0_coeff);
	if (val != 0)
		pr_info("vib_f0_coeff not found\n");

	val = of_property_read_u32_array(np, "vib_duration_time",
		duration_time, ARRAY_SIZE(duration_time));
	if (val != 0)
		pr_info("%s vib_duration_time not found\n", __func__);
	memcpy(aw8624_dts_data.aw8624_duration_time,
		duration_time, sizeof(duration_time));

	val = of_property_read_u32_array(np, "vib_brake_cont_config",
		brake_cont_config, ARRAY_SIZE(brake_cont_config));
	if (val != 0)
		pr_info("%s vib_brake_cont_config not found\n", __func__);
	memcpy(aw8624_dts_data.aw8624_cont_brake,
		brake_cont_config, sizeof(brake_cont_config));

	val = of_property_read_u32_array(np, "vib_f0_trace_parameter",
		f0_trace_parameter, ARRAY_SIZE(f0_trace_parameter));
	if (val != 0)
		pr_info("%s vib_f0_trace_parameter not found\n", __func__);
	memcpy(aw8624_dts_data.aw8624_f0_trace_parameter,
		f0_trace_parameter, sizeof(f0_trace_parameter));

	val = of_property_read_u32_array(np, "vib_bemf_config",
		bemf_config, ARRAY_SIZE(bemf_config));
	if (val != 0)
		pr_info("%s vib_bemf_config not found\n", __func__);
	memcpy(aw8624_dts_data.aw8624_bemf_config,
		bemf_config, sizeof(bemf_config));

	val =
	of_property_read_u32(np, "vib_sw_brake",
		&aw8624_dts_data.aw8624_sw_brake);
	if (val != 0)
		pr_info("%s vib_sw_brake not found\n", __func__);

	val = of_property_read_u32_array(np, "vib_wavseq",
		wavseq_config, ARRAY_SIZE(wavseq_config));
	if (val != 0)
		pr_info("%s vib_wavseq not found\n", __func__);
	memcpy(aw8624_dts_data.aw8624_wavseq,
		wavseq_config, sizeof(wavseq_config));

	val = of_property_read_u32_array(np, "vib_wavloop",
		wavloop_config, ARRAY_SIZE(wavloop_config));
	if (val != 0)
		pr_info("%s vib_wavloop not found\n", __func__);
	memcpy(aw8624_dts_data.aw8624_wavloop,
		wavloop_config, sizeof(wavloop_config));


	val = of_property_read_u32_array(np, "vib_td_brake",
		td_brake_config, ARRAY_SIZE(td_brake_config));
	if (val != 0)
		pr_info("%s vib_td_brake not found\n", __func__);
	memcpy(aw8624_dts_data.aw8624_td_brake,
		td_brake_config, sizeof(td_brake_config));

	val = of_property_read_u32(np, "vib_tset",
		&aw8624_dts_data.aw8624_tset);
	if (val != 0)
		pr_info("%s vib_tset not found\n", __func__);
	val = of_property_read_u32(np, "vib_func_parameter1",
		&aw8624_dts_data.aw8624_parameter1);
	if (val != 0)
		pr_info("%s vib_func_parameter1 not found\n", __func__);



	return 0;
}

static int aw8624_hw_reset(struct aw8624 *aw8624)
{
	pr_info("%s enter\n", __func__);

	if (aw8624 && gpio_is_valid(aw8624->reset_gpio)) {
		gpio_set_value_cansleep(aw8624->reset_gpio, 0);
		usleep_range(1000, 2000);
		gpio_set_value_cansleep(aw8624->reset_gpio, 1);
		usleep_range(3500, 4000);
	} else {
		dev_err(aw8624->dev, "%s: failed\n", __func__);
	}
	return 0;
}


/*****************************************************
 *
 * check chip id
 *
 *****************************************************/
static int aw8624_read_chipid(struct aw8624 *aw8624)
{
	int ret = -1;
	unsigned char cnt = 0;
	unsigned char reg = 0;

	while (cnt < AW_READ_CHIPID_RETRIES) {
		/* hardware reset */
		aw8624_hw_reset(aw8624);

		ret = aw8624_i2c_read(aw8624, AW8624_REG_ID, &reg);
		if (ret < 0) {
			dev_err(aw8624->dev,
			"%s: failed to read register AW8624_REG_ID: %d\n",
			__func__, ret);
		}

		switch (reg) {
		case 0x24:
			pr_info("%s aw8624 detected\n", __func__);
			aw8624->chipid = AW8624_ID;
			aw8624_haptic_softreset(aw8624);
			return 0;
		default:
			pr_info("%s unsupported device revision (0x%x)\n",
			__func__, reg);
			break;
		}
		cnt++;

		usleep_range(2000, 3000);
	}

	return -EINVAL;
}


/******************************************************
 *
 * sys group attribute: reg
 *
 ******************************************************/
static ssize_t
aw8624_i2c_reg_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);

	unsigned int databuf[2] = {0, 0};

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2)
		aw8624_i2c_write(aw8624,
				(unsigned char)databuf[0],
				(unsigned char)databuf[1]);

	return count;
}

static ssize_t
aw8624_i2c_reg_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
	ssize_t len = 0;
	unsigned char i = 0;
	unsigned char reg_val = 0;

	for (i = 0; i < AW8624_REG_MAX; i++) {
		if (!(aw8624_reg_access[i]&REG_RD_ACCESS))
			continue;
		aw8624_i2c_read(aw8624, i, &reg_val);
		len += snprintf(buf+len, PAGE_SIZE-len,
		"reg:0x%02x=0x%02x\n", i, reg_val);
	}
	return len;
}
static ssize_t
aw8624_i2c_ram_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
	int err, val;

	err = kstrtoint(buf, 0, &val);
	if (err != 0) {
		pr_err("%s format not match!", __func__);
		return count;
	}
	pr_info("%s: value=%d\n", __func__, val);
	if (val == 1)
		aw8624_ram_update(aw8624);

	return count;
}

static ssize_t
aw8624_i2c_ram_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct aw8624 *aw8624 = dev_get_drvdata(dev);
	ssize_t len = 0;
	unsigned int i = 0;
	unsigned char reg_val = 0;

	aw8624_haptic_stop(aw8624);
	/* RAMINIT Enable */
	aw8624_i2c_write_bits(aw8624,
			AW8624_REG_SYSCTRL,
			AW8624_BIT_SYSCTRL_RAMINIT_MASK,
			AW8624_BIT_SYSCTRL_RAMINIT_EN);

	aw8624_i2c_write(aw8624,
			AW8624_REG_RAMADDRH,
			(unsigned char)(aw8624->ram.base_addr>>8));
	aw8624_i2c_write(aw8624,
			AW8624_REG_RAMADDRL,
			(unsigned char)(aw8624->ram.base_addr&0x00ff));
	len += snprintf(buf+len, PAGE_SIZE-len, "aw8624_haptic_ram:\n");
	for (i = 0; i < aw8624->ram.len; i++) {
		aw8624_i2c_read(aw8624, AW8624_REG_RAMDATA, &reg_val);
		len += snprintf(buf+len, PAGE_SIZE-len, "0x%02x,", reg_val);
	}
	len += snprintf(buf+len, PAGE_SIZE-len, "\n");
	/* RAMINIT Disable */
	aw8624_i2c_write_bits(aw8624,
			AW8624_REG_SYSCTRL,
			AW8624_BIT_SYSCTRL_RAMINIT_MASK,
			AW8624_BIT_SYSCTRL_RAMINIT_OFF);

	return len;
}

static DEVICE_ATTR(reg, 0664, aw8624_i2c_reg_show, aw8624_i2c_reg_store);
static DEVICE_ATTR(ram, 0664, aw8624_i2c_ram_show, aw8624_i2c_ram_store);

static struct attribute *aw8624_attributes[] = {
	&dev_attr_reg.attr,
	&dev_attr_ram.attr,
	NULL
};

static struct attribute_group aw8624_attribute_group = {
	.attrs = aw8624_attributes
};


/******************************************************
 *
 * i2c driver
 *
 ******************************************************/
static int
aw8624_i2c_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	struct aw8624 *aw8624;
	struct device_node *np = i2c->dev.of_node;
	int ret = -1;
	int irq_flags = 0;

	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		dev_err(&i2c->dev, "check_functionality failed\n");
		return -EIO;
	}

	aw8624 = devm_kzalloc(&i2c->dev, sizeof(struct aw8624), GFP_KERNEL);
	if (aw8624 == NULL)
		return -ENOMEM;

	aw8624->dev = &i2c->dev;
	aw8624->i2c = i2c;

	i2c_set_clientdata(i2c, aw8624);


	/* aw8624 rst & int */
	if (np) {
		ret = aw8624_parse_dt(&i2c->dev, aw8624, np);
		if (ret) {
			dev_err(&i2c->dev,
			"%s: failed to parse device tree node\n", __func__);
			goto err_parse_dt;
		}
	}
	if (aw8624->dts_add_real)
		i2c->addr =  (u16)aw8624->reg_real_addr;

	if (gpio_is_valid(aw8624->reset_gpio)) {
		aw8624->reset_gpio_ret =
		devm_gpio_request_one(&i2c->dev, aw8624->reset_gpio,
			GPIOF_OUT_INIT_LOW, "aw8624_rst");
		if (aw8624->reset_gpio_ret) {
			dev_err(&i2c->dev,
			"%s: rst request failed\n", __func__);
			return aw8624->reset_gpio_ret;
		}
	}

	if (gpio_is_valid(aw8624->irq_gpio)) {
		aw8624->irq_gpio_ret =
		devm_gpio_request_one(&i2c->dev, aw8624->irq_gpio,
			GPIOF_DIR_IN, "aw8624_int");
		if (aw8624->irq_gpio_ret) {
			dev_err(&i2c->dev,
			"%s: int request failed\n", __func__);
			return aw8624->irq_gpio_ret;
		}
	}

	/* aw8624 chip id */
	ret = aw8624_read_chipid(aw8624);
	if (ret < 0) {
		dev_err(&i2c->dev,
		"%s: aw8624_read_chipid failed ret=%d\n", __func__, ret);
		goto err_id;
	}


	/* aw8624 irq */
	if (gpio_is_valid(aw8624->irq_gpio) &&
		!(aw8624->flags & AW8624_FLAG_SKIP_INTERRUPTS)) {
		/* register irq handler */
		aw8624_interrupt_setup(aw8624);
		irq_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
		ret = devm_request_threaded_irq(&i2c->dev,
					gpio_to_irq(aw8624->irq_gpio),
					NULL, aw8624_irq, irq_flags,
					"aw8624", aw8624);
		if (ret != 0) {
			dev_err(&i2c->dev,
			"%s: failed to request IRQ %d: %d\n",
			__func__, gpio_to_irq(aw8624->irq_gpio), ret);
			goto err_irq;
		}
	}
	dev_set_drvdata(&i2c->dev, aw8624);

	ret = sysfs_create_group(&i2c->dev.kobj, &aw8624_attribute_group);
	if (ret < 0) {
		dev_info(&i2c->dev,
		"%s error creating sysfs attr files\n", __func__);
		goto err_sysfs;
	}

	g_aw8624 = aw8624;
	//wake_lock_init(&aw8624->wk_lock, WAKE_LOCK_SUSPEND, "aw8624_wakelock");

	aw8624_vibrator_init(aw8624);
	aw8624_haptic_init(aw8624);
	aw8624_ram_init(aw8624);
	usleep_range(100000, 150000);
	if (aw8624->IsUsedIRQ)
		aw8624_haptic_trig1_config(aw8624, true);
	pr_info("%s probe completed successfully!\n", __func__);

	return 0;

err_sysfs:
	sysfs_remove_group(&i2c->dev.kobj, &aw8624_attribute_group);
err_irq:
err_id:
err_parse_dt:
	return ret;
}

static int aw8624_i2c_remove(struct i2c_client *i2c)
{
	struct aw8624 *aw8624 = i2c_get_clientdata(i2c);

	cancel_delayed_work_sync(&aw8624->ram_work);
	misc_deregister(&aw8624_haptic_misc);

	cancel_work_sync(&aw8624->haptic_audio.work);
	hrtimer_cancel(&aw8624->haptic_audio.timer);
	if (aw8624->IsUsedIRQ)
		cancel_work_sync(&aw8624->rtp_work);
	cancel_work_sync(&aw8624->vibrator_work);
	hrtimer_cancel(&aw8624->timer);

	mutex_destroy(&aw8624->lock);
	mutex_destroy(&aw8624->haptic_audio.lock);

	sysfs_remove_group(&i2c->dev.kobj, &aw8624_attribute_group);
	sysfs_remove_group(&i2c->dev.kobj, &aw8624_vibrator_attribute_group);

	devm_free_irq(&i2c->dev, gpio_to_irq(aw8624->irq_gpio), aw8624);

	return 0;
}

static const struct i2c_device_id aw8624_i2c_id[] = {
	{ AW8624_I2C_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, aw8624_i2c_id);

static const struct of_device_id aw8624_dt_match[] = {
	{ .compatible = "awinic,aw8624_haptic" },
	{ },
};

static struct i2c_driver aw8624_i2c_driver = {
	.driver = {
		.name = AW8624_I2C_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(aw8624_dt_match),
	},
	.probe = aw8624_i2c_probe,
	.remove = aw8624_i2c_remove,
	.id_table = aw8624_i2c_id,
};

static int __init aw8624_i2c_init(void)
{
	int ret = 0;

	pr_info("aw8624 driver version %s\n", AW8624_VERSION);

	ret = i2c_add_driver(&aw8624_i2c_driver);
	if (ret) {
		pr_err("fail to add aw8624 device into i2c\n");
		return ret;
	}

	return 0;
}

late_initcall(aw8624_i2c_init);

static void __exit aw8624_i2c_exit(void)
{
	i2c_del_driver(&aw8624_i2c_driver);
}
module_exit(aw8624_i2c_exit);

MODULE_DESCRIPTION("AW8624 Haptic Driver");
MODULE_LICENSE("GPL v2");
