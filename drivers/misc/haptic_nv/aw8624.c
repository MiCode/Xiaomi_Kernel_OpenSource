/*
 * aw8624.c
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
#include <linux/interrupt.h>
#include <linux/debugfs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <linux/power_supply.h>
#include <linux/pm_qos.h>
#include <linux/jiffies.h>
#include <linux/vmalloc.h>

#include "aw8624.h"
#include "haptic_nv.h"

/******************************************************
 *
 * Register Access
 *
 ******************************************************/
static const unsigned char aw8624_reg_list[] = {
	AW8624_REG_ID,
	AW8624_REG_SYSST,
	AW8624_REG_SYSINT,
	AW8624_REG_SYSINTM,
	AW8624_REG_SYSCTRL,
	AW8624_REG_GO,
	AW8624_REG_WAVSEQ1,
	AW8624_REG_WAVSEQ2,
	AW8624_REG_WAVSEQ3,
	AW8624_REG_WAVSEQ4,
	AW8624_REG_WAVSEQ5,
	AW8624_REG_WAVSEQ6,
	AW8624_REG_WAVSEQ7,
	AW8624_REG_WAVSEQ8,
	AW8624_REG_WAVLOOP1,
	AW8624_REG_WAVLOOP2,
	AW8624_REG_WAVLOOP3,
	AW8624_REG_WAVLOOP4,
	AW8624_REG_MANLOOP,
	AW8624_REG_TRG1_SEQP,
	AW8624_REG_TRG1_SEQN,
	AW8624_REG_TRG_CFG1,
	AW8624_REG_TRG_CFG2,
	AW8624_REG_DBGCTRL,
	AW8624_REG_BASE_ADDRH,
	AW8624_REG_BASE_ADDRL,
	AW8624_REG_FIFO_AEH,
	AW8624_REG_FIFO_AEL,
	AW8624_REG_FIFO_AFH,
	AW8624_REG_FIFO_AFL,
	AW8624_REG_DATCTRL,
	AW8624_REG_PWMPRC,
	AW8624_REG_PWMDBG,
	AW8624_REG_DBGSTAT,
	AW8624_REG_WAVECTRL,
	AW8624_REG_BRAKE0_CTRL,
	AW8624_REG_BRAKE1_CTRL,
	AW8624_REG_BRAKE2_CTRL,
	AW8624_REG_BRAKE_NUM,
	AW8624_REG_ANACTRL,
	AW8624_REG_SW_BRAKE,
	AW8624_REG_DATDBG,
	AW8624_REG_PRLVL,
	AW8624_REG_PRTIME,
	AW8624_REG_RAMADDRH,
	AW8624_REG_RAMADDRL,
	AW8624_REG_BRA_MAX_NUM,
	AW8624_REG_GLB_STATE,
	AW8624_REG_CONT_CTRL,
	AW8624_REG_F_PRE_H,
	AW8624_REG_F_PRE_L,
	AW8624_REG_TD_H,
	AW8624_REG_TD_L,
	AW8624_REG_TSET,
	AW8624_REG_THRS_BRA_END,
	AW8624_REG_EF_RDATAH,
	AW8624_REG_TRIM_LRA,
	AW8624_REG_R_SPARE,
	AW8624_REG_D2SCFG,
	AW8624_REG_DETCTRL,
	AW8624_REG_RLDET,
	AW8624_REG_OSDET,
	AW8624_REG_VBATDET,
	AW8624_REG_ADCTEST,
	AW8624_REG_F_LRA_F0_H,
	AW8624_REG_F_LRA_F0_L,
	AW8624_REG_F_LRA_CONT_H,
	AW8624_REG_F_LRA_CONT_L,
	AW8624_REG_WAIT_VOL_MP,
	AW8624_REG_WAIT_VOL_MN,
	AW8624_REG_ZC_THRSH_H,
	AW8624_REG_ZC_THRSH_L,
	AW8624_REG_BEMF_VTHH_H,
	AW8624_REG_BEMF_VTHH_L,
	AW8624_REG_BEMF_VTHL_H,
	AW8624_REG_BEMF_VTHL_L,
	AW8624_REG_BEMF_NUM,
	AW8624_REG_DRV_TIME,
	AW8624_REG_TIME_NZC,
	AW8624_REG_DRV_LVL,
	AW8624_REG_DRV_LVL_OV,
	AW8624_REG_NUM_F0_1,
	AW8624_REG_NUM_F0_2,
	AW8624_REG_NUM_F0_3,
};

/******************************************************
 *
 * value
 *
 ******************************************************/
static char *aw8624_ram_name = "aw8624_haptic.bin";
static char aw8624_rtp_name[][AW8624_RTP_NAME_MAX] = {
	{"aw8624l_osc_rtp_24K_5s.bin"},
	{"aw8624l_rtp.bin"},
	{"aw8624l_rtp_lighthouse.bin"},
	{"aw8624l_rtp_silk.bin"},
};
static struct aw8624_dts_info aw8624_dts_data;
static struct pm_qos_request aw8624_pm_qos_req_vb;

/******************************************************
 *
 * function declaration
 *
 ******************************************************/
static void aw8624_interrupt_clear(struct aw8624 *aw8624);
static void aw8624_haptic_upload_lra(struct aw8624 *aw8624, unsigned int flag);
int aw8624_haptic_stop_l(struct aw8624 *aw8624);
static int aw8624_analyse_duration_range(struct aw8624 *aw8624);

/******************************************************
 *
 * aw8624 i2c write/read
 *
 ******************************************************/
static int aw8624_i2c_read(struct aw8624 *aw8624, unsigned char reg_addr,
			   unsigned char *buf, unsigned int len)
{
	int ret = 0;

	struct i2c_msg msg[2] = {
		[0] = {
				.addr = aw8624->i2c->addr,
				.flags = 0,
				.len = sizeof(uint8_t),
				.buf = &reg_addr,
				},
		[1] = {
				.addr = aw8624->i2c->addr,
				.flags = I2C_M_RD,
				.len = len,
				.buf = buf,
				},
	};

	ret = i2c_transfer(aw8624->i2c->adapter, msg, ARRAY_SIZE(msg));
	if (ret < 0) {
		aw_dev_err("%s: i2c_transfer failed\n", __func__);
		return ret;
	} else if (ret != AW_I2C_READ_MSG_NUM) {
		aw_dev_err("%s: transfer failed(size error)\n", __func__);
		return -ENXIO;
	}
	return ret;
}

static int aw8624_i2c_write(struct aw8624 *aw8624, unsigned char reg_addr,
			    unsigned char *buf, unsigned int len)
{
	unsigned char *data = NULL;
	int ret = -1;

	data = kmalloc(len + 1, GFP_KERNEL);
	data[0] = reg_addr;
	memcpy(&data[1], buf, len);
	ret = i2c_master_send(aw8624->i2c, data, len + 1);
	if (ret < 0)
		aw_dev_err("%s: i2c master send 0x%02x error\n",
			   __func__, reg_addr);
	kfree(data);
	return ret;
}

static int aw8624_i2c_write_bits(struct aw8624 *aw8624, unsigned char reg_addr,
				 unsigned int mask,
				 unsigned char reg_data)
{
	unsigned char reg_val = 0;
	int ret = -1;

	ret = aw8624_i2c_read(aw8624, reg_addr, &reg_val, AW_I2C_BYTE_ONE);
	if (ret < 0) {
		aw_dev_err("%s: i2c read error, ret=%d\n", __func__, ret);
		return ret;
	}
	reg_val &= mask;
	reg_val |= reg_data;
	ret = aw8624_i2c_write(aw8624, reg_addr, &reg_val, AW_I2C_BYTE_ONE);
	if (ret < 0) {
		aw_dev_err("%s: i2c write error, ret=%d\n", __func__, ret);
		return ret;
	}
	return 0;
}

static int aw8624_parse_data(struct aw8624 *aw8624, const char *buf)
{
	unsigned char reg_num = aw8624->aw_i2c_package.reg_num;
	unsigned char i = 0;
	const char *temp_buf = NULL;
	char data_buf[AWRW_CMD_UNIT] = { 0 };
	unsigned int value = 0;
	int ret = 0;
	int len = strlen(buf) - (AWRW_CMD_UNIT * 3);

	if (len < 0) {
		aw_dev_err("%s: parse data error\n", __func__);
		return -ERANGE;
	}
	temp_buf = &buf[AWRW_CMD_UNIT * 3];
	for (i = 0; i < reg_num; i++) {
		if (((i + 1) * AWRW_CMD_UNIT) > len) {
			aw_dev_err("%s: parse data error\n", __func__);
			return -ERANGE;
		}
		memcpy(data_buf, &temp_buf[i * AWRW_CMD_UNIT], 4);
		data_buf[4] = '\0';
		ret = kstrtouint(data_buf, 0, &value);
		if (ret < 0) {
			aw_dev_err("%s: kstrtouint error\n", __func__);
			return ret;
		}
		aw8624->aw_i2c_package.reg_data[i] = (unsigned char)value;
	}
	return 0;
}

static void aw8624_interrupt_clear(struct aw8624 *aw8624)
{
	unsigned char reg_val = 0;

	aw8624_i2c_read(aw8624, AW8624_REG_SYSINT, &reg_val, AW_I2C_BYTE_ONE);
	aw_dev_info("%s: reg SYSINT=0x%x\n", __func__, reg_val);
}

/*****************************************************
 *
 * ram update
 *
 *****************************************************/
static int aw8624_haptic_juge_RTP_is_going_on(struct aw8624 *aw8624)
{
	unsigned char rtp_state = 0;
	unsigned char mode = 0;
	unsigned char glb_st = 0;

	aw8624_i2c_read(aw8624, AW8624_REG_SYSCTRL, &mode, AW_I2C_BYTE_ONE);
	aw8624_i2c_read(aw8624, AW8624_REG_GLB_STATE, &glb_st, AW_I2C_BYTE_ONE);
	if ((mode & AW8624_BIT_SYSCTRL_PLAY_MODE_RTP) &&
		(glb_st == AW8624_BIT_GLBRD5_STATE_RTP_GO)) {
		rtp_state = 1;
	}
	return rtp_state;
}

static void aw8624_haptic_raminit(struct aw8624 *aw8624, bool flag)
{
	if (flag) {
		aw8624_i2c_write_bits(aw8624, AW8624_REG_SYSCTRL,
				       AW8624_BIT_SYSCTRL_RAMINIT_MASK,
				       AW8624_BIT_SYSCTRL_RAMINIT_EN);
	} else {
		aw8624_i2c_write_bits(aw8624, AW8624_REG_SYSCTRL,
				       AW8624_BIT_SYSCTRL_RAMINIT_MASK,
				       AW8624_BIT_SYSCTRL_RAMINIT_OFF);
	}
}

#ifdef AW_CHECK_RAM_DATA
static int aw8624_check_ram_data(struct aw8624 *aw8624,
				 unsigned char *cont_data,
				 unsigned char *ram_data, unsigned int len)
{
	int i = 0;

	for (i = 0; i < len; i++) {
		if (ram_data[i] != cont_data[i]) {
			aw_dev_err("%s: check ramdata error, addr=0x%04x, ram_data=0x%02x, file_data=0x%02x\n",
				   __func__, i, ram_data[i], cont_data[i]);
			return -ERANGE;
		}
	}
	return 0;
}
#endif

static void aw8624_set_ram_addr(struct aw8624 *aw8624)
{
	unsigned char ram_addr[2] = {0};
	unsigned int base_addr = aw8624->ram.base_addr;

	ram_addr[0] = (unsigned char)AW_SET_RAMADDR_H(base_addr);
	ram_addr[1] = (unsigned char)AW_SET_RAMADDR_L(base_addr);

	aw8624_i2c_write(aw8624, AW8624_REG_RAMADDRH, ram_addr,
			 AW_I2C_BYTE_TWO);
}

static int aw8624_container_update(struct aw8624 *aw8624,
		struct aw8624_container *aw8624_cont)
{
	unsigned char reg_array[4] = {0};
	unsigned int shift = 0;
	unsigned int base_addr = 0;
	int i = 0;
	int ret = 0;
	int len = 0;
#ifdef AW_CHECK_RAM_DATA
	unsigned char ram_data[AW_RAMDATA_RD_BUFFER_SIZE] = {0};
#endif
	mutex_lock(&aw8624->lock);

	aw8624->ram.baseaddr_shift = 2;
	aw8624->ram.ram_shift = 4;

	/* RAMINIT Enable */
	aw8624_haptic_raminit(aw8624, true);

	/* set base addr */
	shift = aw8624->ram.baseaddr_shift;
	aw8624->ram.base_addr = (unsigned int)((aw8624_cont->data[0+shift]<<8) |
		(aw8624_cont->data[1+shift]));
	base_addr = aw8624->ram.base_addr;
	reg_array[0] = (unsigned char)AW_SET_BASEADDR_H(base_addr);
	reg_array[1] = (unsigned char)AW_SET_BASEADDR_L(base_addr);
	aw8624_i2c_write(aw8624, AW8624_REG_BASE_ADDRH, reg_array,
			 AW_I2C_BYTE_TWO);
	aw_dev_info("%s: base_addr=0x%04x\n", __func__,
		    aw8624->ram.base_addr);

	/* set FIFO_AE and FIFO_AF addr */
	reg_array[0] = (unsigned char)AW8624_SET_AEADDR_H(base_addr);
	reg_array[1] = (unsigned char)AW8624_SET_AEADDR_L(base_addr);
	reg_array[2] = (unsigned char)AW8624_SET_AFADDR_H(base_addr);
	reg_array[3] = (unsigned char)AW8624_SET_AFADDR_L(base_addr);
	aw8624_i2c_write(aw8624, AW8624_REG_FIFO_AEH, reg_array,
			 AW_I2C_BYTE_FOUR);
	/* get FIFO_AE and FIFO_AF addr */
	aw8624_i2c_read(aw8624, AW8624_REG_FIFO_AEH, reg_array,
			AW_I2C_BYTE_FOUR);
	aw_dev_info("%s: almost_empty_threshold = %d\n", __func__,
		    (unsigned short)((reg_array[0] << 8) | reg_array[1]));
	aw_dev_info("%s: almost_empty_threshold = %d\n", __func__,
		    (unsigned short)((reg_array[2] << 8) | reg_array[3]));

	/* write ram data */
	aw8624_set_ram_addr(aw8624);
	i = aw8624->ram.ram_shift;
	while (i < aw8624_cont->len) {
		if ((aw8624_cont->len - i) < AW_RAMDATA_WR_BUFFER_SIZE)
			len = aw8624_cont->len - i;
		else
			len = AW_RAMDATA_WR_BUFFER_SIZE;

		aw8624_i2c_write(aw8624, AW8624_REG_RAMDATA,
			    &aw8624_cont->data[i], len);
		i += len;
	}


#ifdef AW_CHECK_RAM_DATA
	aw8624_set_ram_addr(aw8624);
	i = aw8624->ram.ram_shift;
	while (i < aw8624_cont->len) {
		if ((aw8624_cont->len - i) < AW_RAMDATA_RD_BUFFER_SIZE)
			len = aw8624_cont->len - i;
		else
			len = AW_RAMDATA_RD_BUFFER_SIZE;

		aw8624_i2c_read(aw8624, AW8624_REG_RAMDATA, ram_data, len);
		ret = aw8624_check_ram_data(aw8624, &aw8624_cont->data[i],
					     ram_data, len);
		if (ret < 0)
			break;
		i += len;
	}
	if (ret)
		aw_dev_err("%s: ram data check sum error\n", __func__);
	else
		aw_dev_info("%s: ram data check sum pass\n",  __func__);


#endif
	/* RAMINIT Disable */
	aw8624_haptic_raminit(aw8624, false);

	mutex_unlock(&aw8624->lock);
	aw_dev_info("%s exit\n", __func__);
	return ret;
}

static int aw8624_haptic_get_ram_number(struct aw8624 *aw8624)
{
	unsigned char ram_data[3];
	unsigned int first_wave_addr = 0;

	aw_dev_info("%s enter!\n", __func__);
	if (!aw8624->ram_init) {
		aw_dev_err("%s: ram init faild, ram_num = 0!\n", __func__);
		return -EPERM;
	}

	mutex_lock(&aw8624->lock);
	/* RAMINIT Enable */
	aw8624_haptic_raminit(aw8624, true);
	aw8624_haptic_stop_l(aw8624);
	aw8624_set_ram_addr(aw8624);

	aw8624_i2c_read(aw8624, AW8624_REG_RAMDATA, ram_data,
			AW_I2C_BYTE_THREE);
	first_wave_addr = (ram_data[1] << 8 | ram_data[2]);
	aw8624->ram.ram_num =
			(first_wave_addr - aw8624->ram.base_addr - 1) / 4;
	aw_dev_info("%s: ram_version = 0x%02x\n", __func__, ram_data[0]);
	aw_dev_info("%s: first waveform addr = 0x%04x\n", __func__,
		    first_wave_addr);
	aw_dev_info("%s: ram_num = %d\n", __func__, aw8624->ram.ram_num);
	/* RAMINIT Disable */
	aw8624_haptic_raminit(aw8624, false);
	mutex_unlock(&aw8624->lock);

	return 0;
}

static void aw8624_ram_check(const struct firmware *cont, void *context)
{
	struct aw8624 *aw8624 = context;
	struct aw8624_container *aw8624_fw;
	unsigned short check_sum = 0;
	int i = 0;
	int ret = 0;
#ifdef AW_READ_BIN_FLEXBALLY
	static unsigned char load_cont;
	int ram_timer_val = 1000;

	load_cont++;
#endif
	aw_dev_dbg("%s enter\n", __func__);
	if (!cont) {
		aw_dev_err("%s: failed to read %s\n", __func__,
			   aw8624_ram_name);
		release_firmware(cont);
#ifdef AW_READ_BIN_FLEXBALLY
		if (load_cont <= 20) {
			schedule_delayed_work(&aw8624->ram_work,
					msecs_to_jiffies(ram_timer_val));
			aw_dev_info("%s:start hrtimer: load_cont=%d\n",
				    __func__, load_cont);
		}
#endif
		return;
	}

	aw_dev_info("%s: loaded %s - size: %zu\n", __func__, aw8624_ram_name,
		cont ? cont->size : 0);

	/* check sum */
	for (i = 2; i < cont->size; i++)
		check_sum += cont->data[i];

	if (check_sum == (unsigned short)((cont->data[0]<<8)|(cont->data[1]))) {
		aw_dev_info("%s: check sum pass : 0x%04x\n",
			    __func__, check_sum);
		aw8624->ram.check_sum = check_sum;
	} else {
		aw_dev_err("%s: check sum err: check_sum=0x%04x\n",
			   __func__, check_sum);
		return;
	}

	/* aw8624 ram update */
	aw8624_fw = kzalloc(cont->size+sizeof(int), GFP_KERNEL);
	if (!aw8624_fw) {
		release_firmware(cont);
		aw_dev_err("%s: Error allocating memory\n", __func__);
		return;
	}
	aw8624_fw->len = cont->size;
	memcpy(aw8624_fw->data, cont->data, cont->size);
	release_firmware(cont);

	ret = aw8624_container_update(aw8624, aw8624_fw);
	if (ret) {
		kfree(aw8624_fw);
		aw8624->ram.len = 0;
		aw_dev_err("%s: ram firmware update failed!\n", __func__);
	} else {
		aw8624->ram_init = 1;
		aw8624->ram.len = aw8624_fw->len;
		kfree(aw8624_fw);
		aw_dev_info("%s: ram firmware update complete\n", __func__);
	}
	aw8624_haptic_get_ram_number(aw8624);

}

static int aw8624_ram_update(struct aw8624 *aw8624)
{
	aw8624->ram_init = 0;
	aw8624->rtp_init = 0;
	return request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
				aw8624_ram_name, aw8624->dev, GFP_KERNEL,
				aw8624, aw8624_ram_check);
}

static void aw8624_ram_work_routine(struct work_struct *work)
{
	struct aw8624 *aw8624 =
		container_of(work, struct aw8624, ram_work.work);

	aw8624_ram_update(aw8624);

}

int aw8624_ram_init_l(struct aw8624 *aw8624)
{
	int ram_timer_val = 8000;

	aw_dev_dbg("%s enter\n", __func__);
	INIT_DELAYED_WORK(&aw8624->ram_work, aw8624_ram_work_routine);
	schedule_delayed_work(&aw8624->ram_work,
				msecs_to_jiffies(ram_timer_val));

	return 0;
}

/*****************************************************
 *
 * haptic control
 *
 *****************************************************/

static int aw8624_haptic_play_init(struct aw8624 *aw8624)
{
	unsigned char reg_val = 0;

	if (aw8624->play_mode == AW8624_HAPTIC_CONT_MODE)
		reg_val = (unsigned char)(aw8624_dts_data.aw8624_sw_brake[0]);
	else
		reg_val = (unsigned char)(aw8624_dts_data.aw8624_sw_brake[1]);

	aw8624_i2c_write(aw8624,
			AW8624_REG_SW_BRAKE, &reg_val, AW_I2C_BYTE_ONE);
	return 0;
}

static int aw8624_haptic_active(struct aw8624 *aw8624)
{
	aw8624_haptic_play_init(aw8624);
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

static int aw8624_haptic_play_mode(struct aw8624 *aw8624,
							unsigned char play_mode)
{
	switch (play_mode) {
	case AW8624_HAPTIC_STANDBY_MODE:
		aw_dev_info("%s: enter standby mode\n", __func__);
		aw8624->play_mode = AW8624_HAPTIC_STANDBY_MODE;
		aw8624_i2c_write_bits(aw8624, AW8624_REG_SYSINTM,
				AW8624_BIT_SYSINTM_UVLO_MASK,
				AW8624_BIT_SYSINTM_UVLO_OFF);
		aw8624_i2c_write_bits(aw8624,
				AW8624_REG_SYSCTRL,
				AW8624_BIT_SYSCTRL_WORK_MODE_MASK,
				AW8624_BIT_SYSCTRL_STANDBY);
		break;
	case AW8624_HAPTIC_RAM_MODE:
		aw_dev_info("%s: enter ram mode\n", __func__);
		aw8624->play_mode = AW8624_HAPTIC_RAM_MODE;
		aw8624_i2c_write_bits(aw8624,
				AW8624_REG_SYSCTRL,
				AW8624_BIT_SYSCTRL_PLAY_MODE_MASK,
				AW8624_BIT_SYSCTRL_PLAY_MODE_RAM);
		aw8624_haptic_active(aw8624);
		break;
	case AW8624_HAPTIC_RAM_LOOP_MODE:
		aw_dev_info("%s: enter ram loop mode\n", __func__);
		aw8624->play_mode = AW8624_HAPTIC_RAM_LOOP_MODE;
		aw8624_i2c_write_bits(aw8624,
				AW8624_REG_SYSCTRL,
				AW8624_BIT_SYSCTRL_PLAY_MODE_MASK,
				AW8624_BIT_SYSCTRL_PLAY_MODE_RAM);
		aw8624_haptic_active(aw8624);
		break;
	case AW8624_HAPTIC_RTP_MODE:
		aw_dev_info("%s: enter rtp mode\n", __func__);
		aw8624->play_mode = AW8624_HAPTIC_RTP_MODE;
		aw8624_i2c_write_bits(aw8624,
				AW8624_REG_SYSCTRL,
				AW8624_BIT_SYSCTRL_PLAY_MODE_MASK,
				AW8624_BIT_SYSCTRL_PLAY_MODE_RTP);
		aw8624_haptic_active(aw8624);
		break;
	case AW8624_HAPTIC_TRIG_MODE:
		aw_dev_info("%s: enter trig mode\n", __func__);
		aw8624->play_mode = AW8624_HAPTIC_TRIG_MODE;
		aw8624_i2c_write_bits(aw8624,
				AW8624_REG_SYSCTRL,
				AW8624_BIT_SYSCTRL_PLAY_MODE_MASK,
				AW8624_BIT_SYSCTRL_PLAY_MODE_RAM);
		aw8624_haptic_active(aw8624);
		break;
	case AW8624_HAPTIC_CONT_MODE:
		aw_dev_info("%s: enter cont mode\n", __func__);
		aw8624->play_mode = AW8624_HAPTIC_CONT_MODE;
		aw8624_i2c_write_bits(aw8624,
				AW8624_REG_SYSCTRL,
				AW8624_BIT_SYSCTRL_PLAY_MODE_MASK,
				AW8624_BIT_SYSCTRL_PLAY_MODE_CONT);
		aw8624_haptic_active(aw8624);
		break;
	default:
		aw_dev_err("%s: play mode %d err", __func__, play_mode);
		break;
	}
	return 0;
}

static int aw8624_haptic_play_go(struct aw8624 *aw8624, bool flag)
{
	aw_dev_dbg("%s enter, flag = %d\n", __func__, flag);
	if (!flag) {
		aw8624->current_t = ktime_get();
		aw8624->interval_us = ktime_to_us(ktime_sub(aw8624->current_t,
						aw8624->pre_enter_t));
		if (aw8624->interval_us < 2000) {
			aw_dev_info("%s:aw8624->interval_us=%d\n",
					__func__, aw8624->interval_us);
			usleep_range(2000, 2500);
		}
	}
	if (flag == true) {
		aw8624_i2c_write_bits(aw8624, AW8624_REG_GO,
			AW8624_BIT_GO_MASK, AW8624_BIT_GO_ENABLE);
		aw8624->pre_enter_t = ktime_get();
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
		aw8624_i2c_read(aw8624, AW8624_REG_GLB_STATE, &reg_val,
				AW_I2C_BYTE_ONE);
		if ((reg_val&0x0f) == AW8624_BIT_GLBRD5_STATE_STANDBY) {
			aw_dev_info("%s enter standby, reg glb_state=0x%02x\n",
				__func__, reg_val);
			return 0;
		}
		usleep_range(2000, 2500);

		aw_dev_dbg("%s wait for standby, reg glb_state=0x%02x\n",
			__func__, reg_val);
	}
	aw_dev_err("%s do not enter standby automatically\n", __func__);
	return 0;
}

int aw8624_haptic_stop_l(struct aw8624 *aw8624)
{
	aw_dev_dbg("%s enter\n", __func__);
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
	aw8624_i2c_write(aw8624, AW8624_REG_WAVSEQ1+wav, &seq, AW_I2C_BYTE_ONE);
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
	aw8624_haptic_set_wav_seq(aw8624, 0x01, 0x00);
	aw8624_haptic_set_wav_loop(aw8624, 0x00,
				   AW8624_BIT_WAVLOOP_INIFINITELY);

	return 0;
}

static int aw8624_haptic_set_gain(struct aw8624 *aw8624, unsigned char gain)
{
	aw8624_i2c_write(aw8624, AW8624_REG_DATDBG, &gain, AW_I2C_BYTE_ONE);
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

static int aw8624_haptic_play_wav_seq(struct aw8624 *aw8624,
				       unsigned char flag)
{
	aw_dev_dbg("%s enter\n", __func__);
	if (flag) {
		aw8624_haptic_play_mode(aw8624, AW8624_HAPTIC_RAM_MODE);
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

static int aw8624_haptic_ram_config(struct aw8624 *aw8624, int duration)
{
	int ret = 0;

	if (aw8624->duration_time_flag < 0) {
		aw_dev_err("%s: duration time error, array size = %d\n",
			   __func__, aw8624->duration_time_size);
		return -ERANGE;
	}
	ret = aw8624_analyse_duration_range(aw8624);
	if (ret < 0)
		return ret;
	if ((duration > 0) && (duration <
				aw8624_dts_data.aw8624_duration_time[0])) {
		aw8624->index = 3;	/*3*/
		aw8624->activate_mode = AW8624_HAPTIC_RAM_MODE;
	} else if ((duration >= aw8624_dts_data.aw8624_duration_time[0]) &&
		(duration < aw8624_dts_data.aw8624_duration_time[1])) {
		aw8624->index = 2;	/*2*/
		aw8624->activate_mode = AW8624_HAPTIC_RAM_MODE;
	} else if ((duration >= aw8624_dts_data.aw8624_duration_time[1]) &&
		(duration < aw8624_dts_data.aw8624_duration_time[2])) {
		aw8624->index = 1;	/*1*/
		aw8624->activate_mode = AW8624_HAPTIC_RAM_MODE;
	} else if (duration >= aw8624_dts_data.aw8624_duration_time[2]) {
		aw8624->index = 4;	/*4*/
		aw8624->activate_mode = AW8624_HAPTIC_RAM_LOOP_MODE;
	} else {
		aw_dev_err("%s: duration time error, duration= %d\n",
			   __func__, duration);
		aw8624->index = 0;
		aw8624->activate_mode = AW8624_HAPTIC_NULL;
		ret = -ERANGE;
	}

	return ret;
}

static int aw8624_haptic_select_pin(struct aw8624 *aw8624, unsigned char pin)
{
	if (pin == AW8624_TRIG1) {
		aw8624_i2c_write_bits(aw8624,
				AW8624_REG_DBGCTRL,
				AW8624_BIT_DBGCTRL_INTN_TRG_SEL_MASK,
				AW8624_BIT_DBGCTRL_TRG_SEL_ENABLE);
		aw8624_i2c_write_bits(aw8624,
				AW8624_REG_TRG_CFG2,
				AW8624_BIT_TRGCFG2_TRG1_ENABLE_MASK,
				AW8624_BIT_TRGCFG2_TRG1_ENABLE);
		aw_dev_info("%s: select TRIG1 pin\n", __func__);
	} else if (pin == AW8624_IRQ) {
		aw8624_i2c_write_bits(aw8624,
				AW8624_REG_DBGCTRL,
				AW8624_BIT_DBGCTRL_INTN_TRG_SEL_MASK,
				AW8624_BIT_DBGCTRL_INTN_SEL_ENABLE);
		aw8624_i2c_write_bits(aw8624,
				AW8624_REG_TRG_CFG2,
				AW8624_BIT_TRGCFG2_TRG1_ENABLE_MASK,
				AW8624_BIT_TRGCFG2_TRG1_DISABLE);
		aw_dev_info("%s: select INIT pin\n", __func__);
	} else
		aw_dev_err("%s: There is no such option\n", __func__);
	return 0;
}
static int aw8624_haptic_trig1_param_init(struct aw8624 *aw8624)
{
	if (aw8624->IsUsedIRQ) {
		aw8624_haptic_select_pin(aw8624, AW8624_IRQ);
		return 0;
	}
	aw8624->trig.trig_enable = aw8624_dts_data.trig_config[0];
	aw8624->trig.trig_edge = aw8624_dts_data.trig_config[1];
	aw8624->trig.trig_polar = aw8624_dts_data.trig_config[2];
	aw8624->trig.pos_sequence = aw8624_dts_data.trig_config[3];
	aw8624->trig.neg_sequence = aw8624_dts_data.trig_config[4];
	aw_dev_info("%s: trig1 date init ok!\n", __func__);
	return 0;
}
static int aw8624_haptic_tirg1_param_config(struct aw8624 *aw8624)
{
	if (aw8624->IsUsedIRQ) {
		aw8624_haptic_select_pin(aw8624, AW8624_IRQ);
		return 0;
	}
	if (aw8624->trig.trig_enable)
		aw8624_haptic_select_pin(aw8624, AW8624_TRIG1);
	else
		aw8624_haptic_select_pin(aw8624, AW8624_IRQ);

	aw8624_i2c_write_bits(aw8624, AW8624_REG_TRG_CFG1,
			AW8624_BIT_TRGCFG1_TRG1_EDGE_MASK,
			aw8624->trig.trig_edge);
	aw8624_i2c_write_bits(aw8624, AW8624_REG_TRG_CFG1,
			AW8624_BIT_TRGCFG1_TRG1_POLAR_MASK,
			aw8624->trig.trig_polar << 1);
	aw8624_i2c_write(aw8624, AW8624_REG_TRG1_SEQP,
			 &aw8624->trig.pos_sequence, AW_I2C_BYTE_ONE);
	aw8624_i2c_write(aw8624, AW8624_REG_TRG1_SEQN,
			 &aw8624->trig.neg_sequence, AW_I2C_BYTE_ONE);
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

static int aw8624_haptic_set_f0_preset(struct aw8624 *aw8624, unsigned int f0_pre)
{
	unsigned int f0_reg = 0;
	unsigned char reg_array[2] = {0};

	f0_reg = 1000000000 / (f0_pre * aw8624_dts_data.aw8624_f0_coeff);
	reg_array[0] = (unsigned char)((f0_reg >> 8) & 0xff);
	reg_array[1] = (unsigned char)((f0_reg >> 0) & 0xff);

	aw8624_i2c_write(aw8624, AW8624_REG_F_PRE_H, reg_array,
			 AW_I2C_BYTE_TWO);
	return 0;
}

static int aw8624_haptic_read_f0(struct aw8624 *aw8624)
{
	unsigned char reg_val[2] = {0};
	unsigned int f0_reg = 0;
	unsigned long f0_tmp = 0;

	aw8624_i2c_read(aw8624, AW8624_REG_F_LRA_F0_H, reg_val,
			AW_I2C_BYTE_TWO);
	f0_reg = (reg_val[0] << 8) | reg_val[1];

	if (!f0_reg || !aw8624_dts_data.aw8624_f0_coeff) {
		aw8624->f0 = 0;
		aw_dev_info("%s : get f0 failed with the value becoming 0!\n",
			    __func__);
		return -EPERM;
	}

	f0_tmp = AW8624_F0_FORMULA(f0_reg, aw8624_dts_data.aw8624_f0_coeff);
	aw8624->f0 = (unsigned int)f0_tmp;
	aw_dev_info("%s f0=%d\n", __func__, aw8624->f0);
	return 0;
}

static int aw8624_haptic_read_cont_f0(struct aw8624 *aw8624)
{
	unsigned char reg_val[2] = {0};
	unsigned int f0_reg = 0;
	unsigned long f0_tmp = 0;

	aw8624_i2c_read(aw8624, AW8624_REG_F_LRA_CONT_H, reg_val,
			AW_I2C_BYTE_TWO);
	f0_reg = (reg_val[0] << 8) | reg_val[1];

	if (!f0_reg) {
		aw8624->cont_f0 = 0;
		aw_dev_info("%s: failed to reading cont f0 with 0\n", __func__);
		return 0;
	}

	f0_tmp = AW8624_F0_FORMULA(f0_reg, aw8624_dts_data.aw8624_f0_coeff);
	aw8624->cont_f0 = (unsigned int)f0_tmp;
	aw_dev_info("%s cont_f0=%d\n", __func__, aw8624->cont_f0);
	return 0;
}

static int aw8624_haptic_read_beme(struct aw8624 *aw8624)
{
	unsigned char reg_val[2] = {0};

	aw8624_i2c_read(aw8624, AW8624_REG_WAIT_VOL_MP, reg_val,
			AW_I2C_BYTE_TWO);
	aw8624->max_pos_beme = reg_val[0];
	aw8624->max_neg_beme = reg_val[1];

	aw_dev_info("%s max_pos_beme=%d\n", __func__, aw8624->max_pos_beme);
	aw_dev_info("%s max_neg_beme=%d\n", __func__, aw8624->max_neg_beme);

	return 0;
}

static int aw8624_vbat_monitor_detector(struct aw8624 *aw8624)
{
	unsigned char reg_val = 0;
	unsigned int vbat = 0;

	aw8624_haptic_stop_l(aw8624);
	/*step 1:EN_RAMINIT*/
	aw8624_haptic_raminit(aw8624, true);

	/*step 2 :launch power supply testing */
	aw8624_i2c_write_bits(aw8624,
				AW8624_REG_DETCTRL,
				AW8624_BIT_DETCTRL_VBAT_GO_MASK,
				AW8624_BIT_DETCTRL_VABT_GO_ENABLE);
	usleep_range(AW8624_VBAT_DELAY_MIN, AW8624_VBAT_DELAY_MAX);

	aw8624_i2c_read(aw8624, AW8624_REG_VBATDET, &reg_val,
			AW_I2C_BYTE_ONE);
	vbat = AW8624_VBAT_FORMULA(reg_val);
	aw_dev_info("%s get_vbat=%dmV\n", __func__, vbat);
	/*step 3: return val*/
	aw8624_haptic_raminit(aw8624, false);

	return vbat;
}

static int aw8624_lra_resistance_detector(struct aw8624 *aw8624)
{
	unsigned char reg_val = 0;
	unsigned char reg_val_anactrl = 0;
	unsigned char reg_val_d2scfg = 0;
	unsigned int r_lra = 0;

	mutex_lock(&aw8624->lock);
	aw8624_i2c_read(aw8624, AW8624_REG_ANACTRL, &reg_val_anactrl,
			AW_I2C_BYTE_ONE);
	aw8624_i2c_read(aw8624, AW8624_REG_D2SCFG, &reg_val_d2scfg,
			AW_I2C_BYTE_ONE);
	aw8624_haptic_stop_l(aw8624);
	aw8624_haptic_raminit(aw8624, true);


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
				(AW8624_BIT_DETCTRL_RL_OS_MASK &
				AW8624_BIT_DETCTRL_DIAG_GO_MASK),
				(AW8624_BIT_DETCTRL_RL_DETECT |
				AW8624_BIT_DETCTRL_DIAG_GO_ENABLE));
	usleep_range(AW8624_LRA_DELAY_MIN, AW8624_LRA_DELAY_MAX);
	aw8624_i2c_read(aw8624, AW8624_REG_RLDET, &reg_val,
			AW_I2C_BYTE_ONE);
	r_lra =  AW8624_LRA_FORMULA(reg_val);

	aw8624_i2c_write(aw8624, AW8624_REG_D2SCFG, &reg_val_d2scfg,
			 AW_I2C_BYTE_ONE);
	aw8624_i2c_write(aw8624, AW8624_REG_ANACTRL, &reg_val_anactrl,
			 AW_I2C_BYTE_ONE);
	aw8624_haptic_raminit(aw8624, false);
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
				aw_dev_dbg("%s gain limit=%d\n", __func__,
					   temp_gain);
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

	if (aw8624->osc_cali_flag == 1) {
		aw8624_i2c_read(aw8624, AW8624_REG_SYSST, &reg_val,
				AW_I2C_BYTE_ONE);
		reg_val &= AW8624_BIT_SYSST_FF_AFS;
		ret = reg_val >> 3;
	} else {
		aw8624_i2c_read(aw8624, AW8624_REG_SYSINT, &reg_val,
				AW_I2C_BYTE_ONE);
		reg_val &= AW8624_BIT_SYSINT_FF_AFI;
		ret = reg_val >> 3;
	}
	return ret;
}

static unsigned char aw8624_haptic_rtp_get_fifo_afs(struct aw8624 *aw8624)
{
	unsigned char ret = 0;
	unsigned char reg_val = 0;

	aw8624_i2c_read(aw8624, AW8624_REG_SYSST, &reg_val,
			AW_I2C_BYTE_ONE);
	reg_val &= AW8624_BIT_SYSST_FF_AFS;
	ret = reg_val >> 3;

	return ret;
}

static int aw8624_haptic_rtp_init(struct aw8624 *aw8624)
{
	unsigned int buf_len = 0;
	unsigned char glb_st = 0;
	struct aw8624_container *rtp_container = aw8624->rtp_container;

	aw_dev_dbg("%s enter\n", __func__);
	aw8624->rtp_cnt = 0;
	mutex_lock(&aw8624->rtp_lock);
	while ((!aw8624_haptic_rtp_get_fifo_afs(aw8624)) &&
	       (aw8624->play_mode == AW8624_HAPTIC_RTP_MODE)) {
#ifdef AW_ENABLE_RTP_PRINT_LOG
		aw_dev_info("%s rtp cnt = %d\n", __func__, aw8624->rtp_cnt);
#endif
		if (!rtp_container) {
			aw_dev_info("%s:aw8624_rtp is null break\n", __func__);
			break;
		}

		if (aw8624->rtp_cnt < aw8624->ram.base_addr) {
			if ((rtp_container->len-aw8624->rtp_cnt) <
						(aw8624->ram.base_addr)) {
				buf_len = rtp_container->len - aw8624->rtp_cnt;
			} else {
				buf_len = (aw8624->ram.base_addr);
			}
		} else if ((rtp_container->len - aw8624->rtp_cnt) <
						(aw8624->ram.base_addr >> 2)) {
			buf_len = rtp_container->len - aw8624->rtp_cnt;
		} else {
			buf_len = (aw8624->ram.base_addr >> 2);
		}
#ifdef AW_ENABLE_RTP_PRINT_LOG
		aw_dev_info("%s buf_len = %d\n", __func__, buf_len);
#endif
		aw8624_i2c_write(aw8624, AW8624_REG_RTP_DATA,
				&rtp_container->data[aw8624->rtp_cnt],
				buf_len);

		aw8624->rtp_cnt += buf_len;
		aw8624_i2c_read(aw8624, AW8624_REG_GLB_STATE, &glb_st,
				AW_I2C_BYTE_ONE);
		if (aw8624->rtp_cnt == rtp_container->len ||
		    (glb_st == AW8624_BIT_GLBRD5_STATE_STANDBY)) {
			if (aw8624->rtp_cnt == rtp_container->len)
				aw_dev_info("%s: rtp load completely!\n",
					    __func__);
			else
				aw_dev_err("%s rtp load failed!!\n", __func__);
			aw8624->rtp_cnt = 0;
			mutex_unlock(&aw8624->rtp_lock);
			return 0;
		}
	}

	if (aw8624->play_mode == AW8624_HAPTIC_RTP_MODE)
		aw8624_haptic_set_rtp_aei(aw8624, true);

	aw_dev_dbg("%s exit\n", __func__);
	mutex_unlock(&aw8624->rtp_lock);
	return 0;
}

static void aw8624_rtp_work_routine(struct work_struct *work)
{
	const struct firmware *rtp_file;
	int ret = -1;
	unsigned char reg_val = 0;
	bool rtp_work_flag = false;
	unsigned int cnt = 200;

	struct aw8624 *aw8624 = container_of(work, struct aw8624, rtp_work);

	/* fw loaded */
	aw_dev_dbg("%s enter\n", __func__);
	mutex_lock(&aw8624->rtp_lock);
	ret = request_firmware(&rtp_file,
	aw8624_rtp_name[aw8624->rtp_file_num], aw8624->dev);
	if (ret < 0) {
		aw_dev_err("%s: failed to read %s\n", __func__,
			   aw8624_rtp_name[aw8624->rtp_file_num]);
		mutex_unlock(&aw8624->rtp_lock);
		return;
	}
	aw8624->rtp_init = 0;
	vfree(aw8624->rtp_container);
	aw8624->rtp_container = vmalloc(rtp_file->size+sizeof(int));
	if (!aw8624->rtp_container) {
		release_firmware(rtp_file);
		aw_dev_err("%s: error allocating memory\n", __func__);
		mutex_unlock(&aw8624->rtp_lock);
		return;
	}
	aw8624->rtp_container->len = rtp_file->size;
	aw_dev_info("%s: rtp file [%s] size = %d\n", __func__,
		    aw8624_rtp_name[aw8624->rtp_file_num],
		    aw8624->rtp_container->len);
	memcpy(aw8624->rtp_container->data, rtp_file->data, rtp_file->size);
	mutex_unlock(&aw8624->rtp_lock);
	release_firmware(rtp_file);
	mutex_lock(&aw8624->lock);
	aw8624->rtp_init = 1;
	if (aw8624->IsUsedIRQ)
		aw8624_haptic_select_pin(aw8624, AW8624_IRQ);
	aw8624_haptic_upload_lra(aw8624, AW8624_HAPTIC_RTP_CALI_LRA);
	/* rtp mode config */
	aw8624_haptic_play_mode(aw8624, AW8624_HAPTIC_RTP_MODE);

	/* haptic start */
	aw8624_haptic_start(aw8624);
	mutex_unlock(&aw8624->lock);
	usleep_range(AW8624_STOP_DELAY_MIN, AW8624_STOP_DELAY_MAX);
	while (cnt) {
		aw8624_i2c_read(aw8624, AW8624_REG_GLB_STATE, &reg_val,
				AW_I2C_BYTE_ONE);
		if (reg_val == AW8624_BIT_GLBRD5_STATE_RTP_GO) {
			cnt = 0;
			rtp_work_flag = true;
			aw_dev_info("%s RTP_GO! glb_state=0x08\n", __func__);
		} else {
			cnt--;
			aw_dev_dbg("%s wait for RTP_GO, glb_state=0x%02X\n",
				   __func__, reg_val);
		}
		usleep_range(AW8624_STOP_DELAY_MIN, AW8624_STOP_DELAY_MAX);
	}
	if (rtp_work_flag) {
		aw8624_haptic_rtp_init(aw8624);
	} else {
		/* enter standby mode */
		aw8624_haptic_stop_l(aw8624);
		aw_dev_err("%s failed to enter RTP_GO status!\n",
			   __func__);
	}

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

static void aw8624_ctr_list_config(struct aw8624 *aw8624,
				  struct haptic_ctr *p_ctr,
				  struct haptic_ctr *p_ctr_bak,
				  struct haptic_audio *haptic_audio)
{
	unsigned int list_input_cnt = 0;
	unsigned int list_output_cnt = 0;
	unsigned int list_diff_cnt = 0;
	unsigned int list_del_cnt = 0;

	list_for_each_entry_safe(p_ctr, p_ctr_bak,
			&(haptic_audio->ctr_list), list) {
		list_input_cnt =  p_ctr->cnt;
		break;
	}
	list_for_each_entry_safe_reverse(p_ctr, p_ctr_bak,
			&(haptic_audio->ctr_list), list) {
		list_output_cnt =  p_ctr->cnt;
		break;
	}
	if (list_input_cnt > list_output_cnt)
		list_diff_cnt = list_input_cnt - list_output_cnt;

	if (list_input_cnt < list_output_cnt)
		list_diff_cnt = 32 + list_input_cnt - list_output_cnt;

	if (list_diff_cnt > 2) {
		list_for_each_entry_safe_reverse(p_ctr, p_ctr_bak,
				&(haptic_audio->ctr_list), list) {
			if ((p_ctr->play == 0) &&
			(AW8624_HAPTIC_CMD_ENABLE ==
				(AW8624_HAPTIC_CMD_HAPTIC & p_ctr->cmd))) {
				list_del(&p_ctr->list);
				kfree(p_ctr);
				list_del_cnt++;
			}
			if (list_del_cnt == list_diff_cnt)
				break;
		}
	}
}

static void aw8624_haptic_audio_work_routine(struct work_struct *work)
{
	struct aw8624 *aw8624 =
	container_of(work, struct aw8624, haptic_audio.work);
	struct haptic_audio *haptic_audio = NULL;
	struct haptic_ctr *p_ctr = NULL;
	struct haptic_ctr *p_ctr_bak = NULL;
	unsigned int ctr_list_flag = 0;

	int rtp_is_going_on = 0;

	aw_dev_dbg("%s enter\n", __func__);
	haptic_audio = &(aw8624->haptic_audio);
	mutex_lock(&aw8624->haptic_audio.lock);
	memset(&aw8624->haptic_audio.ctr, 0, sizeof(struct haptic_ctr));
	ctr_list_flag = 0;
		list_for_each_entry_safe_reverse(p_ctr, p_ctr_bak,
				&(haptic_audio->ctr_list), list) {
		ctr_list_flag = 1;
		break;
	}
	if (ctr_list_flag == 0)
		aw_dev_info("%s: ctr list empty\n", __func__);
	if (ctr_list_flag == 1)
		aw8624_ctr_list_config(aw8624, p_ctr, p_ctr_bak, haptic_audio);

	/* get the last data from list */
	list_for_each_entry_safe_reverse(p_ctr, p_ctr_bak,
		&(haptic_audio->ctr_list), list) {
		aw8624->haptic_audio.ctr.cnt = p_ctr->cnt;
		aw8624->haptic_audio.ctr.cmd = p_ctr->cmd;
		aw8624->haptic_audio.ctr.play = p_ctr->play;
		aw8624->haptic_audio.ctr.wavseq = p_ctr->wavseq;
		aw8624->haptic_audio.ctr.loop = p_ctr->loop;
		aw8624->haptic_audio.ctr.gain = p_ctr->gain;
		list_del(&p_ctr->list);
		kfree(p_ctr);
		break;
	}

	if (aw8624->haptic_audio.ctr.play) {
		aw_dev_info("%s: cnt=%d, cmd=%d, play=%d, wavseq=%d, loop=%d, gain=%d\n",
			    __func__,
			    aw8624->haptic_audio.ctr.cnt,
			    aw8624->haptic_audio.ctr.cmd,
			    aw8624->haptic_audio.ctr.play,
			    aw8624->haptic_audio.ctr.wavseq,
			    aw8624->haptic_audio.ctr.loop,
			    aw8624->haptic_audio.ctr.gain);
	}

	/* rtp mode jump */
	rtp_is_going_on = aw8624_haptic_juge_RTP_is_going_on(aw8624);
	if (rtp_is_going_on) {
		mutex_unlock(&aw8624->haptic_audio.lock);
		return;
	}
	mutex_unlock(&aw8624->haptic_audio.lock);

	if ((AW8624_HAPTIC_CMD_HAPTIC & aw8624->haptic_audio.ctr.cmd) ==
		AW8624_HAPTIC_CMD_ENABLE) {
		if (aw8624->haptic_audio.ctr.play ==
			AW8624_HAPTIC_PLAY_ENABLE) {
			aw_dev_info("%s: haptic audio play start\n", __func__);
			mutex_lock(&aw8624->lock);
			aw8624_haptic_stop_l(aw8624);

			aw8624_haptic_play_mode(aw8624, AW8624_HAPTIC_RAM_MODE);

			aw8624_haptic_set_wav_seq(aw8624, 0x00,
					aw8624->haptic_audio.ctr.wavseq);

			aw8624_haptic_set_wav_loop(aw8624, 0x00,
					aw8624->haptic_audio.ctr.loop);

			aw8624_haptic_set_gain(aw8624,
					aw8624->haptic_audio.ctr.gain);

			aw8624_haptic_start(aw8624);
			mutex_unlock(&aw8624->lock);
		} else if (AW8624_HAPTIC_PLAY_STOP ==
			   aw8624->haptic_audio.ctr.play) {
			mutex_lock(&aw8624->lock);
			aw8624_haptic_stop_l(aw8624);
			mutex_unlock(&aw8624->lock);

		} else if (AW8624_HAPTIC_PLAY_GAIN ==
			   aw8624->haptic_audio.ctr.play) {
			mutex_lock(&aw8624->lock);
			aw8624_haptic_set_gain(aw8624,
					       aw8624->haptic_audio.ctr.gain);
			mutex_unlock(&aw8624->lock);
		}
	}
}

/*****************************************************
 *
 * haptic cont
 *
 *****************************************************/
static int aw8624_haptic_cont(struct aw8624 *aw8624)
{
	unsigned char brake0_level = 0;
	unsigned char time_nzc = 0;
	unsigned char en_brake1 = 0;
	unsigned char brake1_level = 0;
	unsigned char en_brake2 = 0;
	unsigned char brake2_level = 0;
	unsigned char brake2_p_num = 0;
	unsigned char brake1_p_num = 0;
	unsigned char brake0_p_num = 0;
	unsigned char reg_array[4] = {0};

	aw_dev_dbg("%s enter\n", __func__);
	/* work mode */
	aw8624_haptic_active(aw8624);
	aw8624_haptic_play_mode(aw8624, AW8624_HAPTIC_CONT_MODE);

	/* preset f0 */
	if (aw8624->f0 <= 0)
		aw8624_haptic_set_f0_preset(aw8624, aw8624_dts_data.aw8624_f0_pre);
	else
		aw8624_haptic_set_f0_preset(aw8624, aw8624->f0);

	/* lpf */
	aw8624_i2c_write_bits(aw8624, AW8624_REG_DATCTRL,
			      (AW8624_BIT_DATCTRL_FC_MASK &
			      AW8624_BIT_DATCTRL_LPF_ENABLE_MASK),
			      (AW8624_BIT_DATCTRL_FC_1000HZ |
			      AW8624_BIT_DATCTRL_LPF_ENABLE));

	/* brake */
	en_brake1 = aw8624_dts_data.aw8624_cont_brake[0][0];
	en_brake2 = aw8624_dts_data.aw8624_cont_brake[0][1];
	brake0_level = aw8624_dts_data.aw8624_cont_brake[0][2];
	brake1_level = aw8624_dts_data.aw8624_cont_brake[0][3];
	brake2_level = aw8624_dts_data.aw8624_cont_brake[0][4];
	brake0_p_num = aw8624_dts_data.aw8624_cont_brake[0][5];
	brake1_p_num = aw8624_dts_data.aw8624_cont_brake[0][6];
	brake2_p_num = aw8624_dts_data.aw8624_cont_brake[0][7];

	reg_array[0] = brake0_level << 0;
	reg_array[1] = (en_brake1 << 7)|(brake1_level << 0);
	reg_array[2] = (en_brake2 << 7)|(brake2_level << 0);
	reg_array[3] = (brake2_p_num << 6) | (brake1_p_num << 3) |
			(brake0_p_num << 0);
	aw8624_i2c_write(aw8624, AW8624_REG_BRAKE0_CTRL, reg_array,
			 AW_I2C_BYTE_FOUR);

	/* cont config */
	reg_array[0] = AW8624_BIT_CONT_CTRL_ZC_DETEC_ENABLE |
		       AW8624_BIT_CONT_CTRL_WAIT_1PERIOD |
		       AW8624_BIT_CONT_CTRL_BY_GO_SIGNAL |
		       AW8624_BIT_CONT_CTRL_CLOSE_PLAYBACK |
		       AW8624_BIT_CONT_CTRL_F0_DETECT_DISABLE |
		       AW8624_BIT_CONT_CTRL_O2C_DISABLE;
	aw8624_i2c_write(aw8624, AW8624_REG_CONT_CTRL, reg_array,
			 AW_I2C_BYTE_ONE);
	/* TD time */
	reg_array[0] = (unsigned char)(aw8624->cont_td>>8);
	reg_array[1] = (unsigned char)(aw8624->cont_td>>0);
	aw8624_i2c_write(aw8624, AW8624_REG_TD_H, reg_array, AW_I2C_BYTE_TWO);


	aw8624_i2c_write_bits(aw8624, AW8624_REG_BEMF_NUM,
			      AW8624_BIT_BEMF_NUM_BRK_MASK,
			      aw8624->cont_num_brk);
	time_nzc = AW8624_BIT_TIME_NZC_DEF_VAL;
	aw8624_i2c_write(aw8624, AW8624_REG_TIME_NZC, &time_nzc,
			 AW_I2C_BYTE_ONE);

	/* f0 driver level */
	reg_array[0] = aw8624->cont_drv_lvl;
	reg_array[1] = aw8624->cont_drv_lvl_ov;
	aw8624_i2c_write(aw8624, AW8624_REG_DRV_LVL, reg_array,
			 AW_I2C_BYTE_TWO);
	/* cont play go */
	aw8624_haptic_play_go(aw8624, true);

	return 0;
}

/*****************************************************
 *
 * haptic f0 cali
 *
 *****************************************************/
static void aw8624_haptic_upload_lra(struct aw8624 *aw8624, unsigned int flag)
{
	unsigned char reg_val = 0x00;

	switch (flag) {
	case AW8624_HAPTIC_ZERO:
		aw_dev_info("%s write zero to trim_lra!\n", __func__);
		reg_val = 0x00;
		break;
	case AW8624_HAPTIC_F0_CALI_LRA:
		aw_dev_info("%s f0_cali_lra=%d\n", __func__,
			    aw8624->f0_calib_data);
		reg_val = (unsigned char)aw8624->f0_calib_data;
		break;
	case AW8624_HAPTIC_RTP_CALI_LRA:
		aw_dev_info("%s rtp_cali_lra=%d\n", __func__,
			    aw8624->lra_calib_data);
		reg_val = (unsigned char)aw8624->lra_calib_data;
		break;
	default:
		break;
	}
	aw8624_i2c_write(aw8624, AW8624_REG_TRIM_LRA, &reg_val,
			 AW_I2C_BYTE_ONE);
}

static int aw8624_haptic_get_f0(struct aw8624 *aw8624)
{
	int ret = 0;
	unsigned char reg_val = 0;
	unsigned char reg_array[3] = {0};
	unsigned char f0_pre_num = 0;
	unsigned char f0_wait_num = 0;
	unsigned char f0_repeat_num = 0;
	unsigned char f0_trace_num = 0;
	unsigned int t_f0_ms = 0;
	unsigned int t_f0_trace_ms = 0;
	unsigned char i = 0;
	unsigned int f0_cali_cnt = 50;


	aw_dev_dbg("%s enter\n", __func__);

	/* f0 calibrate work mode */
	aw8624_haptic_stop_l(aw8624);
	aw8624_haptic_play_mode(aw8624, AW8624_HAPTIC_CONT_MODE);


	aw8624_i2c_write_bits(aw8624,
			AW8624_REG_CONT_CTRL,
			(AW8624_BIT_CONT_CTRL_EN_CLOSE_MASK &
			AW8624_BIT_CONT_CTRL_F0_DETECT_MASK),
			(AW8624_BIT_CONT_CTRL_OPEN_PLAYBACK |
			AW8624_BIT_CONT_CTRL_F0_DETECT_ENABLE));

	/* LPF */
	aw8624_i2c_write_bits(aw8624,
			AW8624_REG_DATCTRL,
			(AW8624_BIT_DATCTRL_FC_MASK &
			AW8624_BIT_DATCTRL_LPF_ENABLE_MASK),
			(AW8624_BIT_DATCTRL_FC_1000HZ |
			AW8624_BIT_DATCTRL_LPF_ENABLE));

	/* preset f0 */
	aw8624_haptic_set_f0_preset(aw8624, aw8624->f0_pre);
	/* f0 driver level */
	aw8624_i2c_write(aw8624, AW8624_REG_DRV_LVL, &aw8624->cont_drv_lvl,
			 AW_I2C_BYTE_ONE);
	/* f0 trace parameter */
	if (!aw8624->f0_pre) {
		aw_dev_info("%s:fail to get t_f0_ms\n", __func__);
		return 0;
	}

	f0_pre_num = aw8624_dts_data.aw8624_f0_trace_parameter[0];
	f0_wait_num = aw8624_dts_data.aw8624_f0_trace_parameter[1];
	f0_repeat_num = aw8624_dts_data.aw8624_f0_trace_parameter[2];
	f0_trace_num = aw8624_dts_data.aw8624_f0_trace_parameter[3];
	reg_array[0] = (f0_pre_num << 4)|(f0_wait_num << 0);
	reg_array[1] = f0_repeat_num << 0;
	reg_array[2] = f0_trace_num << 0;

	aw8624_i2c_write(aw8624, AW8624_REG_NUM_F0_1, reg_array,
			 AW_I2C_BYTE_THREE);

	/* clear aw8624 interrupt */
	ret = aw8624_i2c_read(aw8624, AW8624_REG_SYSINT, &reg_val,
			      AW_I2C_BYTE_ONE);

	/* play go and start f0 calibration */
	aw8624_haptic_play_go(aw8624, true);

	/* f0 trace time */
	t_f0_ms = 1000*10 / aw8624->f0_pre;
	t_f0_trace_ms =
	    t_f0_ms * (f0_pre_num + f0_wait_num +
		       (f0_trace_num + f0_wait_num) * (f0_repeat_num - 1));
	aw_dev_info("%s: t_f0_trace_ms = %dms\n", __func__, t_f0_trace_ms);
	usleep_range(t_f0_trace_ms * 1000, t_f0_trace_ms * 1000 + 500);

	for (i = 0; i < f0_cali_cnt; i++) {
		ret = aw8624_i2c_read(aw8624, AW8624_REG_GLB_STATE, &reg_val,
				      AW_I2C_BYTE_ONE);
		/* f0 calibrate done */
		if (reg_val == AW8624_BIT_GLBRD5_STATE_STANDBY) {
			aw8624_haptic_read_f0(aw8624);
			aw8624_haptic_read_beme(aw8624);
			break;
		}
		usleep_range(AW8624_F0_DELAY_MIN, AW8624_F0_DELAY_MAX);
		aw_dev_info("%s f0 cali sleep 10ms,glb_state=0x%x\n",
			    __func__, reg_val);
	}

	if (i == f0_cali_cnt)
		ret = -ERANGE;
	else
		ret = 0;

	aw8624_i2c_write_bits(aw8624, AW8624_REG_CONT_CTRL,
			      (AW8624_BIT_CONT_CTRL_EN_CLOSE_MASK &
			      AW8624_BIT_CONT_CTRL_F0_DETECT_MASK),
			      (AW8624_BIT_CONT_CTRL_CLOSE_PLAYBACK |
			      AW8624_BIT_CONT_CTRL_F0_DETECT_DISABLE));

	return ret;
}

#ifdef AW8624_MUL_GET_F0
static int aw8624_multiple_get_f0(struct aw8624 *aw8624)
{
	int f0_max = aw8624_dts_data.aw8624_f0_pre + AW8624_MUL_GET_F0_RANGE;
	int f0_min = aw8624_dts_data.aw8624_f0_pre - AW8624_MUL_GET_F0_RANGE;
	int i = 0;
	int ret = 0;

	aw8624->f0_pre = aw8624_dts_data.aw8624_f0_pre;
	for (i = 0; i < AW8624_MUL_GET_F0_NUM; i++) {
		aw_dev_info("%s aw8624->f0_pre=%d", __func__, aw8624->f0_pre);
		ret = aw8624_haptic_get_f0(aw8624);
		if (ret)
			return ret;
		if (aw8624->f0 >= f0_max || aw8624->f0 <= f0_min)
			break;
		aw8624->f0_pre = aw8624->f0;
		usleep_range(4000, 4500);
	}
	return 0;
}
#endif

static int aw8624_haptic_f0_calibration(struct aw8624 *aw8624)
{
	int ret = 0;
	unsigned char reg_val = 0;
	unsigned int f0_limit = 0;
	char f0_cali_lra = 0;
	int f0_cali_step = 0;
#ifdef AW8624_MUL_GET_F0
	ret = aw8624_multiple_get_f0(aw8624);
#else
	ret = aw8624_haptic_get_f0(aw8624);
#endif

	if (ret) {
		aw_dev_err("%s get f0 error, user defafult f0\n", __func__);
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
		f0_cali_step = 100000*((int)f0_limit-aw8624_dts_data.aw8624_f0_pre)/((int)f0_limit*25);

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

		aw8624->f0_calib_data = (int)f0_cali_lra;
		/* update cali step */
		aw8624_haptic_upload_lra(aw8624,
					AW8624_HAPTIC_F0_CALI_LRA);
		aw8624_i2c_read(aw8624,
				AW8624_REG_TRIM_LRA,
				&reg_val, AW_I2C_BYTE_ONE);
		aw_dev_info("%s final trim_lra=0x%02x\n", __func__, reg_val);
	}

	aw8624_haptic_play_mode(aw8624, AW8624_HAPTIC_STANDBY_MODE);
	aw8624_haptic_stop_l(aw8624);

	return ret;
}

static int
aw8624_haptic_audio_ctr_list_insert(struct haptic_audio *haptic_audio,
				struct haptic_ctr *haptic_ctr,
				struct device *dev)
{
	struct haptic_ctr *p_new = NULL;

	p_new = kzalloc(sizeof(struct haptic_ctr), GFP_KERNEL);
	if (p_new == NULL)
		return -ERANGE;
	/* update new list info */
	p_new->cnt = haptic_ctr->cnt;
	p_new->cmd = haptic_ctr->cmd;
	p_new->play = haptic_ctr->play;
	p_new->wavseq = haptic_ctr->wavseq;
	p_new->loop = haptic_ctr->loop;
	p_new->gain = haptic_ctr->gain;

	INIT_LIST_HEAD(&(p_new->list));
	list_add(&(p_new->list), &(haptic_audio->ctr_list));
	return 0;
}

static int aw8624_haptic_audio_ctr_list_clear(struct haptic_audio *haptic_audio)
{
	struct haptic_ctr *p_ctr = NULL;
	struct haptic_ctr *p_ctr_bak = NULL;

	list_for_each_entry_safe_reverse(p_ctr, p_ctr_bak,
				&(haptic_audio->ctr_list), list) {
		list_del(&p_ctr->list);
		kfree(p_ctr);
	}

	return 0;
}

static int aw8624_haptic_audio_off(struct aw8624 *aw8624)
{
	aw_dev_info("%s: enter\n", __func__);
	mutex_lock(&aw8624->lock);
	aw8624_haptic_set_gain(aw8624, 0x80);
	aw8624_haptic_stop_l(aw8624);
	aw8624->gun_type = AW_GUN_TYPE_DEF_VAL;
	aw8624->bullet_nr =AW_BULLET_NR_DEF_VAL;
	aw8624_haptic_audio_ctr_list_clear(&aw8624->haptic_audio);
	mutex_unlock(&aw8624->lock);
	return 0;
}

static int aw8624_haptic_audio_init(struct aw8624 *aw8624)
{

	aw_dev_info("%s: enter\n", __func__);
	aw8624_haptic_set_wav_seq(aw8624, 0x01, 0x00);

	return 0;
}

static int aw8624_haptic_offset_calibration(struct aw8624 *aw8624)
{
	unsigned int cont = 2000;
	unsigned char reg_val = 0;
	unsigned char reg_val_sysctrl = 0;

	aw_dev_dbg("%s enter\n", __func__);
	aw8624_i2c_read(aw8624, AW8624_REG_SYSCTRL, &reg_val_sysctrl,
			AW_I2C_BYTE_ONE);
	aw8624_i2c_write_bits(aw8624,
				AW8624_REG_SYSCTRL,
				AW8624_BIT_SYSCTRL_RAMINIT_MASK,
				AW8624_BIT_SYSCTRL_RAMINIT_EN);
	aw8624_i2c_write_bits(aw8624,
				AW8624_REG_DETCTRL,
				AW8624_BIT_DETCTRL_DIAG_GO_MASK,
				AW8624_BIT_DETCTRL_DIAG_GO_ENABLE);
	while (1) {
		aw8624_i2c_read(aw8624, AW8624_REG_DETCTRL, &reg_val,
				AW_I2C_BYTE_ONE);
		if ((reg_val & 0x01) == 0 || cont == 0)
			break;
		cont--;
	}
	if (cont == 0)
		aw_dev_err("%s calibration offset failed!\n",
			   __func__);

	aw8624_i2c_write(aw8624, AW8624_REG_SYSCTRL, &reg_val_sysctrl,
			 AW_I2C_BYTE_ONE);
	return 0;

}

int aw8624_haptic_init_l(struct aw8624 *aw8624)
{
	int ret = 0;
	unsigned char reg_val = 0, reg_flag = 0;
	unsigned char reg_array[8] = {0};
	/* haptic audio */
	aw8624->haptic_audio.delay_val = 23;
	aw8624->haptic_audio.timer_val = 23;
	INIT_LIST_HEAD(&(aw8624->haptic_audio.ctr_list));
	hrtimer_init(&aw8624->haptic_audio.timer,
			CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	aw8624->haptic_audio.timer.function = aw8624_haptic_audio_timer_func;
	INIT_WORK(&aw8624->haptic_audio.work, aw8624_haptic_audio_work_routine);
	aw8624->gun_type = AW_GUN_TYPE_DEF_VAL;
	aw8624->bullet_nr = AW_BULLET_NR_DEF_VAL;
	mutex_init(&aw8624->haptic_audio.lock);
	/* haptic init */
	mutex_lock(&aw8624->lock);
	ret = aw8624_i2c_read(aw8624, AW8624_REG_EF_RDATAH, &reg_flag,
			      AW_I2C_BYTE_ONE);
	if ((ret >= 0) && ((reg_flag & 0x1) == 1)) {
		aw8624->chipid_flag = 1;
	} else {
		aw_dev_err("%s: to read register AW8624_REG_EF_RDATAH: %d\n",
			   __func__, ret);
	}

	aw8624->activate_mode = aw8624_dts_data.aw8624_mode;
	ret = aw8624_i2c_read(aw8624, AW8624_REG_WAVSEQ1, &reg_val,
			      AW_I2C_BYTE_ONE);
	aw8624->index = reg_val & 0x7F;
	ret = aw8624_i2c_read(aw8624, AW8624_REG_DATDBG, &reg_val,
			      AW_I2C_BYTE_ONE);
	aw8624->gain = reg_val & 0xFF;
	aw8624_i2c_read(aw8624, AW8624_REG_WAVSEQ1, reg_array,
			AW8624_SEQUENCER_SIZE);
	memcpy(aw8624->seq, reg_array, AW8624_SEQUENCER_SIZE);

	aw8624_haptic_play_mode(aw8624, AW8624_HAPTIC_STANDBY_MODE);
	aw8624_haptic_set_pwm(aw8624, AW8624_PWM_24K);

	aw8624_haptic_swicth_motorprotect_config(aw8624, 0x0, 0x0);
	/*trig config*/
	aw8624_haptic_trig1_param_init(aw8624);
	aw8624_haptic_tirg1_param_config(aw8624);
	aw8624_haptic_offset_calibration(aw8624);
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
	/*LRA trim source select register*/
	aw8624_i2c_write_bits(aw8624,
				AW8624_REG_ANACTRL,
				AW8624_BIT_ANACTRL_LRA_SRC_MASK,
				AW8624_BIT_ANACTRL_LRA_SRC_REG);
	if (aw8624_dts_data.aw8624_lk_f0_cali) {
		aw8624->f0_calib_data = aw8624_dts_data.aw8624_lk_f0_cali;
		aw8624_haptic_upload_lra(aw8624, AW8624_HAPTIC_F0_CALI_LRA);
	} else {
		aw8624_haptic_upload_lra(aw8624, AW8624_HAPTIC_ZERO);
		aw8624_haptic_f0_calibration(aw8624);
	}
	mutex_unlock(&aw8624->lock);

	/*brake*/
	mutex_lock(&aw8624->lock);
	reg_val = (unsigned char)(aw8624_dts_data.aw8624_sw_brake[0]);
	aw8624_i2c_write(aw8624, AW8624_REG_SW_BRAKE, &reg_val,
			 AW_I2C_BYTE_ONE);
	reg_val = 0x00;
	aw8624_i2c_write(aw8624, AW8624_REG_THRS_BRA_END, &reg_val,
			 AW_I2C_BYTE_ONE);
	aw8624_i2c_write_bits(aw8624,
			AW8624_REG_WAVECTRL,
			AW8624_BIT_WAVECTRL_NUM_OV_DRIVER_MASK,
			AW8624_BIT_WAVECTRL_NUM_OV_DRIVER);
	/* zero cross */
	reg_array[0] = (unsigned char)(aw8624->cont_zc_thr>>8);
	reg_array[1] = (unsigned char)(aw8624->cont_zc_thr>>0);
	aw8624_i2c_write(aw8624,
			AW8624_REG_ZC_THRSH_H, reg_array,
			AW_I2C_BYTE_TWO);
	reg_val = (unsigned char)aw8624_dts_data.aw8624_tset;
	aw8624_i2c_write(aw8624, AW8624_REG_TSET,
			&reg_val, AW_I2C_BYTE_ONE);

	/* bemf */
	reg_array[0] = (unsigned char)aw8624_dts_data.aw8624_bemf_config[0];
	reg_array[1] = (unsigned char)aw8624_dts_data.aw8624_bemf_config[1];
	reg_array[2] = (unsigned char)aw8624_dts_data.aw8624_bemf_config[2];
	reg_array[3] = (unsigned char)aw8624_dts_data.aw8624_bemf_config[3];
	aw8624_i2c_write(aw8624, AW8624_REG_BEMF_VTHH_H, reg_array,
			 AW_I2C_BYTE_FOUR);
	mutex_unlock(&aw8624->lock);

	return ret;
}

/*****************************************************
 *
 * vibrator
 *
 *****************************************************/
#ifdef TIMED_OUTPUT
static int aw8624_vibrator_get_time(struct timed_output_dev *vib_dev)
{
	struct aw8624 *aw8624 = container_of(vib_dev, struct aw8624, vib_dev);

	if (hrtimer_active(&aw8624->timer)) {
		ktime_t r = hrtimer_get_remaining(&aw8624->timer);

		return ktime_to_ms(r);
	}

	return 0;
}

static void aw8624_vibrator_enable(struct timed_output_dev *vib_dev, int value)
{
	struct aw8624 *aw8624 = container_of(vib_dev, struct aw8624, vib_dev);

	aw_dev_dbg("%s enter\n", __func__);
	if (!aw8624->ram_init) {
		aw_dev_err("%s: ram init failed, not allow to play!\n",
		       __func__);
		return;
	}
	mutex_lock(&aw8624->lock);
	aw8624_haptic_stop_l(aw8624);
	if (value > 0) {
		aw8624_haptic_upload_lra(aw8624, AW8624_HAPTIC_F0_CALI_LRA);
		aw8624_haptic_ram_vbat_comp(aw8624, false);
		aw8624_haptic_play_wav_seq(aw8624, value);
	}

	mutex_unlock(&aw8624->lock);
}

#else
static void
aw8624_vibrator_enable(struct led_classdev *dev, enum led_brightness value)
{
	struct aw8624 *aw8624 = container_of(dev, struct aw8624, vib_dev);

	aw_dev_dbg("%s enter\n", __func__);
	if (!aw8624->ram_init) {
		aw_dev_err("%s: ram init failed, not allow to play!\n",
		       __func__);
		return;
	}
	mutex_lock(&aw8624->lock);
	aw8624_haptic_stop_l(aw8624);
	if (value > 0) {
		aw8624_haptic_upload_lra(aw8624, AW8624_HAPTIC_F0_CALI_LRA);
		aw8624_haptic_ram_vbat_comp(aw8624, false);
		aw8624_haptic_play_wav_seq(aw8624, value);
	}

	mutex_unlock(&aw8624->lock);


}

#endif

static ssize_t aw8624_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);


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
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);
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
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;

	/* setting 0 on duration is NOP for now */
	if (val <= 0)
		return count;
	rc = aw8624_haptic_ram_config(aw8624, val);
	if (rc < 0)
		return count;
	aw8624->duration = val;
	return count;
}

static ssize_t aw8624_activate_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);

	/* For now nothing to show */
	return snprintf(buf, PAGE_SIZE, "%d\n", aw8624->state);
}

static ssize_t aw8624_activate_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);
	unsigned int val = 0;
	int rc = 0;

	if (!aw8624->ram_init) {
		aw_dev_err("%s: ram init failed, not allow to play!\n",
			   __func__);
		return count;
	}
	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;

	if (val != 0 && val != 1)
		return count;

	aw_dev_info("%s: value=%d\n", __func__, val);

	mutex_lock(&aw8624->lock);
	hrtimer_cancel(&aw8624->timer);

	aw8624->state = val;

	/*aw8624_haptic_stop_l(aw8624);*/

	mutex_unlock(&aw8624->lock);
	schedule_work(&aw8624->vibrator_work);

	return count;
}

static ssize_t aw8624_activate_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);

	return snprintf(buf, PAGE_SIZE, "%d\n",
			aw8624->activate_mode);
}

static ssize_t aw8624_activate_mode_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);

	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;

	mutex_lock(&aw8624->lock);
	aw8624->activate_mode = val;
	mutex_unlock(&aw8624->lock);
	return count;
}


static ssize_t aw8624_index_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);

	unsigned char reg_val = 0;

	aw8624_i2c_read(aw8624, AW8624_REG_WAVSEQ1, &reg_val,
			AW_I2C_BYTE_ONE);
	aw8624->index = reg_val;

	return snprintf(buf, PAGE_SIZE, "%d\n", aw8624->index);
}

static ssize_t aw8624_index_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;
	if (val > aw8624->ram.ram_num) {
		aw_dev_err("%s: input value out of range!\n", __func__);
		return count;
	}
	aw_dev_info("%s: value=%d\n", __func__, val);

	mutex_lock(&aw8624->lock);
	aw8624->index = val;
	aw8624_haptic_set_repeat_wav_seq(aw8624, aw8624->index);
	mutex_unlock(&aw8624->lock);
	return count;
}

static ssize_t aw8624_gain_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
		cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);

	return snprintf(buf, PAGE_SIZE, "0x%02x\n", aw8624->gain);
}

static ssize_t aw8624_gain_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;

	aw_dev_info("%s: value=%d\n", __func__, val);

	mutex_lock(&aw8624->lock);
	aw8624->gain = val;
	aw8624_haptic_set_gain(aw8624, aw8624->gain);
	mutex_unlock(&aw8624->lock);
	return count;
}

static ssize_t aw8624_seq_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);
	size_t count = 0;
	unsigned char i = 0;
	unsigned char reg_val[AW8624_SEQUENCER_SIZE] = {0};

	aw8624_i2c_read(aw8624, AW8624_REG_WAVSEQ1, reg_val,
			AW8624_SEQUENCER_SIZE);

	for (i = 0; i < AW8624_SEQUENCER_SIZE; i++) {
		count += snprintf(buf+count, PAGE_SIZE-count,
			"seq%d: 0x%02x\n", i+1, reg_val[i]);
		aw8624->seq[i] = reg_val[i];
	}
	return count;
}

static ssize_t aw8624_seq_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);
	unsigned int databuf[2] = {0, 0};

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		if (databuf[0] > AW8624_SEQUENCER_SIZE ||
		    databuf[1] > aw8624->ram.ram_num) {
			aw_dev_err("%s input value out of range\n",
				   __func__);
			return count;
		}
		aw_dev_info("%s: seq%d=0x%x\n", __func__,
			    databuf[0], databuf[1]);
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
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);

	size_t count = 0;
	unsigned char i = 0;
	unsigned char reg_val[AW8624_SEQUENCER_LOOP_SIZE] = {0};

	aw8624_i2c_read(aw8624, AW8624_REG_WAVLOOP1, reg_val,
			AW8624_SEQUENCER_LOOP_SIZE);

	for (i = 0; i < AW8624_SEQUENCER_LOOP_SIZE; i++) {
		aw8624->loop[i*2+0] = (reg_val[i] >> 4) & 0x0F;
		aw8624->loop[i*2+1] = (reg_val[i] >> 0) & 0x0F;

		count += snprintf(buf+count, PAGE_SIZE-count,
			"seq%d_loop: 0x%02x\n", i*2+1, aw8624->loop[i*2+0]);
		count += snprintf(buf+count, PAGE_SIZE-count,
			"seq%d_loop: 0x%02x\n", i*2+2, aw8624->loop[i*2+1]);
	}
	return count;
}

static ssize_t aw8624_loop_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);
	unsigned int databuf[2] = {0, 0};

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		aw_dev_info("%s: seq%d loop=0x%x\n", __func__,
			    databuf[0], databuf[1]);
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
	unsigned char i = 0;
	unsigned char size = 0;
	unsigned char cnt = 0;
	unsigned char reg_array[AW_REG_MAX] = {0};
	ssize_t len = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);

	aw_dev_dbg("%s: enter!\n", __func__);
	for (i = 0; i <= (AW8624_REG_NUM_F0_3 + 1); i++) {

		if (i == aw8624_reg_list[cnt] &&
		    (cnt < sizeof(aw8624_reg_list))) {
			size++;
			cnt++;
			continue;
		} else {
			if (size != 0) {
				aw8624_i2c_read(aw8624,
						aw8624_reg_list[cnt-size],
						&reg_array[cnt-size], size);
				size = 0;

			}
		}
	}

	for (i = 0; i < sizeof(aw8624_reg_list); i++) {
		len += snprintf(buf + len, PAGE_SIZE - len,
				"reg:0x%02X=0x%02X\n", aw8624_reg_list[i],
				reg_array[i]);
	}

	return len;
}

static ssize_t aw8624_reg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned char val = 0;
	unsigned int databuf[2] = {0, 0};
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);



	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		val = (unsigned char)databuf[1];
		aw8624_i2c_write(aw8624, (unsigned char)databuf[0], &val,
				 AW_I2C_BYTE_ONE);
	}

	return count;
}

static ssize_t aw8624_rtp_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);

	len += snprintf(buf+len, PAGE_SIZE-len, "rtp_cnt = %d\n",
			aw8624->rtp_cnt);

	return len;
}

static ssize_t aw8624_rtp_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int val = 0;
	int rc = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);

	if (!(aw8624->IsUsedIRQ))
		return count;
	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;
	mutex_lock(&aw8624->lock);
	aw8624_haptic_stop_l(aw8624);
	aw8624_haptic_set_rtp_aei(aw8624, false);
	aw8624_interrupt_clear(aw8624);
	if (val < (sizeof(aw8624_rtp_name)/AW8624_RTP_NAME_MAX)) {
		aw8624->rtp_file_num = val;
		if (val) {
			aw_dev_info("%s: aw8624_rtp_name[%d]: %s\n", __func__,
				    val, aw8624_rtp_name[val]);
			schedule_work(&aw8624->rtp_work);
		} else
			aw_dev_err("%s: rtp_file_num 0x%02X over max value\n",
				   __func__, aw8624->rtp_file_num);
	} else {
		aw_dev_err("%s: rtp_file_num 0x%02x over max value\n",
			   __func__, aw8624->rtp_file_num);
	}
	mutex_unlock(&aw8624->lock);
	return count;
}


static ssize_t aw8624_ram_update_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	unsigned char reg_val_sysctrl = 0;
	int cnt = 0;
	int i = 0;
	int size = 0;
	ssize_t len = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);
	unsigned char ram_data[AW_RAMDATA_RD_BUFFER_SIZE] = {0};

	aw8624_i2c_read(aw8624, AW8624_REG_SYSCTRL, &reg_val_sysctrl,
			AW_I2C_BYTE_ONE);
	/* RAMINIT Enable */
	aw8624_i2c_write_bits(aw8624,
				AW8624_REG_SYSCTRL,
				AW8624_BIT_SYSCTRL_RAMINIT_MASK,
				AW8624_BIT_SYSCTRL_RAMINIT_EN);
	aw8624_set_ram_addr(aw8624);
	len += snprintf(buf + len, PAGE_SIZE - len,
			"haptic_ram len = %d\n", aw8624->ram.len);
	while (cnt < aw8624->ram.len) {
		if ((aw8624->ram.len - cnt) < AW_RAMDATA_RD_BUFFER_SIZE)
			size = aw8624->ram.len - cnt;
		else
			size = AW_RAMDATA_RD_BUFFER_SIZE;

		aw8624_i2c_read(aw8624, AW8624_REG_RAMDATA,
				ram_data, size);
		for (i = 0; i < size; i++) {
			len += snprintf(buf + len, PAGE_SIZE - len,
					"0x%02X,", ram_data[i]);
		}
		cnt += size;
	}
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");

	/* RAMINIT Disable */
	aw8624_i2c_write(aw8624, AW8624_REG_SYSCTRL, &reg_val_sysctrl,
			 AW_I2C_BYTE_ONE);
	return len;
}

static ssize_t aw8624_ram_update_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int val = 0;
	int rc = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;

	if (val)
		aw8624_ram_update(aw8624);

	return count;
}

static ssize_t aw8624_f0_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	unsigned char temp = 0;
	ssize_t len = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);

	mutex_lock(&aw8624->lock);
	aw8624_i2c_read(aw8624, AW8624_REG_TRIM_LRA, &temp,
			AW_I2C_BYTE_ONE);
	aw8624_haptic_upload_lra(aw8624, AW8624_HAPTIC_ZERO);
#ifdef AW8624_MUL_GET_F0
	aw8624_multiple_get_f0(aw8624);
#else
	aw8624_haptic_get_f0(aw8624);
#endif
	aw8624_i2c_write(aw8624, AW8624_REG_TRIM_LRA, &temp, AW_I2C_BYTE_ONE);
	mutex_unlock(&aw8624->lock);
	len += snprintf(buf+len, PAGE_SIZE-len, "%d\n", aw8624->f0);
	aw_dev_info("len = %zd, buf=%s", len, buf);
	return len;
}

static ssize_t aw8624_f0_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;

	return count;
}


static ssize_t aw8624_cali_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	unsigned char temp = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);

	mutex_lock(&aw8624->lock);
	aw8624_i2c_read(aw8624, AW8624_REG_TRIM_LRA, &temp,
			AW_I2C_BYTE_ONE);
	aw8624_haptic_upload_lra(aw8624, AW8624_HAPTIC_F0_CALI_LRA);
	aw8624_haptic_get_f0(aw8624);
	aw8624_i2c_write(aw8624, AW8624_REG_TRIM_LRA, &temp, AW_I2C_BYTE_ONE);
	mutex_unlock(&aw8624->lock);

	len += snprintf(buf+len, PAGE_SIZE-len, "%d\n", aw8624->f0);
	return len;
}

static ssize_t
aw8624_cali_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned int val = 0;
	int rc = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;

	if (val) {
		mutex_lock(&aw8624->lock);
		aw8624_haptic_upload_lra(aw8624, AW8624_HAPTIC_ZERO);
		aw8624_haptic_f0_calibration(aw8624);
		mutex_unlock(&aw8624->lock);
	}
	return count;
}

static ssize_t
aw8624_cont_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);

	aw8624_haptic_read_cont_f0(aw8624);
	len += snprintf(buf+len, PAGE_SIZE-len, "%d\n", aw8624->cont_f0);
	return len;
}

static ssize_t
aw8624_cont_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned int val = 0;
	int rc = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;

	aw8624_haptic_stop_l(aw8624);

	if (val)
		aw8624_haptic_cont(aw8624);
	return count;
}


static ssize_t
aw8624_cont_td_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	ssize_t len = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);

	len += snprintf(buf+len, PAGE_SIZE-len,
			"cont_delay_time = 0x%04x\n",
			aw8624->cont_td);
	return len;
}

static ssize_t
aw8624_cont_td_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);
	int err, val;
	unsigned char reg_array[2] = {0};

	err = kstrtoint(buf, 16, &val);
	if (err != 0) {
		aw_dev_err("%s format not match!", __func__);
		return count;
	}

	aw8624->cont_td = val;
	reg_array[0] = (unsigned char)(val >> 8);
	reg_array[1] = (unsigned char)(val >> 0);
	aw8624_i2c_write(aw8624, AW8624_REG_TD_H, reg_array, AW_I2C_BYTE_TWO);

	return count;
}

static ssize_t
aw8624_cont_drv_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	ssize_t len = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);

	len += snprintf(buf+len, PAGE_SIZE-len,
			"cont drv level = 0x%02x\n",
			aw8624->cont_drv_lvl);
	len += snprintf(buf+len, PAGE_SIZE-len,
			"cont drv level overdrive= 0x%02x\n",
			aw8624->cont_drv_lvl_ov);
	return len;
}

static ssize_t
aw8624_cont_drv_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned char reg_array[2] = {0};
	unsigned int databuf[2] = {0};
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		aw8624->cont_drv_lvl = databuf[0];
		aw8624->cont_drv_lvl_ov = databuf[1];
		reg_array[0] = aw8624->cont_drv_lvl;
		reg_array[1] = aw8624->cont_drv_lvl_ov;
		aw8624_i2c_write(aw8624, AW8624_REG_DRV_LVL, reg_array,
				 AW_I2C_BYTE_TWO);
	}
	return count;
}

static ssize_t
aw8624_cont_num_brk_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	ssize_t len = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);

	len += snprintf(buf+len, PAGE_SIZE-len,
			"cont_brk_num = 0x%02x\n",
			aw8624->cont_num_brk);
	return len;
}

static ssize_t
aw8624_cont_num_brk_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	int err, val;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);

	err = kstrtoint(buf, 16, &val);
	if (err != 0) {
		aw_dev_err("%s format not match!", __func__);
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
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);
	ssize_t len = 0;

	len += snprintf(buf+len, PAGE_SIZE-len,
			"cont_zero_cross_thr = 0x%04x\n",
			aw8624->cont_zc_thr);
	return len;
}

static ssize_t
aw8624_cont_zc_thr_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	unsigned char reg_array[2] = {0};
	int err, val;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);

	err = kstrtoint(buf, 0, &val);
	if (err != 0) {
		aw_dev_err("%s format not match!", __func__);
		return count;
	}
	aw8624->cont_num_brk = val;
	aw_dev_info("%s: val=%d\n", __func__, val);
	if (val > 0) {
		reg_array[0] = (unsigned char)(val >> 8);
		reg_array[1] = (unsigned char)(val >> 0);
		aw8624_i2c_write(aw8624, AW8624_REG_ZC_THRSH_H, reg_array,
				 AW_I2C_BYTE_TWO);
	}
	return count;
}

static ssize_t
aw8624_vbat_monitor_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	unsigned int vbat = 0;
	ssize_t len = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);

	mutex_lock(&aw8624->lock);
	vbat = aw8624_vbat_monitor_detector(aw8624);
	mutex_unlock(&aw8624->lock);
	len += snprintf(buf+len, PAGE_SIZE-len, "vbat_monitor = %d\n", vbat);

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
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);
	ssize_t len = 0;
	unsigned int r_lra = 0;

	r_lra = aw8624_lra_resistance_detector(aw8624);

	len += snprintf(buf+len, PAGE_SIZE-len, "lra_resistance = %d\n", r_lra);
	return len;
}


static ssize_t
aw8624_lra_resistance_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	return count;
}



static ssize_t
aw8624_prctmode_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	unsigned char reg_val = 0;
	ssize_t len = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);


	aw8624_i2c_read(aw8624, AW8624_REG_RLDET, &reg_val,
			AW_I2C_BYTE_ONE);

	len += snprintf(buf+len, PAGE_SIZE-len,
			"prctmode = %d\n", reg_val&0x20);
	return len;
}


static ssize_t
aw8624_prctmode_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	unsigned int databuf[2] = {0, 0};
	unsigned int addr = 0;
	unsigned int val = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);

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
	ssize_t len = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);

	len += snprintf(buf+len, PAGE_SIZE-len,
			"ram_vbat_comp = %d\n",
			aw8624->ram_vbat_comp);

	return len;
}


static ssize_t
aw8624_ram_vbat_comp_store(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	unsigned int val = 0;
	int rc = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;

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
	ssize_t len = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);

	len += snprintf(buf+len, PAGE_SIZE-len, "%d\n",
			aw8624->haptic_audio.ctr.cnt);
	return len;
}

static int aw8624_haptic_audio_config(struct aw8624 *aw8624,
				      unsigned int databuf[])
{
	struct haptic_ctr *hap_ctr = NULL;

	hap_ctr = kzalloc(sizeof(struct haptic_ctr), GFP_KERNEL);
	if (hap_ctr == NULL)
		return -ENOMEM;
	mutex_lock(&aw8624->haptic_audio.lock);
	hap_ctr->cnt = (unsigned char)databuf[0];
	hap_ctr->cmd = (unsigned char)databuf[1];
	hap_ctr->play = (unsigned char)databuf[2];
	hap_ctr->wavseq = (unsigned char)databuf[3];
	hap_ctr->loop = (unsigned char)databuf[4];
	hap_ctr->gain = (unsigned char)databuf[5];
	aw8624_haptic_audio_ctr_list_insert(&aw8624->haptic_audio, hap_ctr,
					    aw8624->dev);
	if (hap_ctr->cmd == AW8624_HAPTIC_CMD_STOP) {
		aw_dev_info("%s: haptic_audio stop\n", __func__);
		if (hrtimer_active(&aw8624->haptic_audio.timer)) {
			aw_dev_info("%s:cancel haptic_audio_timer\n", __func__);
			hrtimer_cancel(&aw8624->haptic_audio.timer);
			aw8624->haptic_audio.ctr.cnt = 0;
			aw8624_haptic_audio_off(aw8624);
		}
	} else {
		if (hrtimer_active(&aw8624->haptic_audio.timer)) {
		} else {
			aw_dev_info("%s:start haptic_audio_timer\n", __func__);
			aw8624_haptic_audio_init(aw8624);
			hrtimer_start(&aw8624->haptic_audio.timer,
			ktime_set(aw8624->haptic_audio.delay_val/1000,
			(aw8624->haptic_audio.delay_val%1000)*1000000),
			HRTIMER_MODE_REL);
		}
	}
	kfree(hap_ctr);
	mutex_unlock(&aw8624->haptic_audio.lock);
	return 0;
}
static ssize_t
aw8624_haptic_audio_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned int databuf[6] = {0};
	int rtp_is_going_on = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);

	rtp_is_going_on = aw8624_haptic_juge_RTP_is_going_on(aw8624);
	if (rtp_is_going_on) {
		aw_dev_info("%s: RTP is runing, stop audio haptic\n", __func__);
		return count;
	}
	if (!aw8624->ram_init) {
		aw_dev_err("%s: ram init failed, not allow to play!\n",
			__func__);
		return count;
	}

	if (sscanf(buf, "%d %d %d %d %d %d",
		&databuf[0], &databuf[1], &databuf[2],
		&databuf[3], &databuf[4], &databuf[5]) == 6) {
		if (databuf[2]) {
			aw_dev_dbg("%s: cnt=%d, cmd=%d, play=%d\n", __func__,
				   databuf[0], databuf[1], databuf[2]);
			aw_dev_dbg("%s: wavseq=%d, loop=%d, gain=%d\n",
				   __func__,
				   databuf[3], databuf[4], databuf[5]);
			aw8624_haptic_audio_config(aw8624, databuf);
		}

	}
	return count;
}

static ssize_t
aw8624_haptic_audio_time_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	ssize_t len = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);

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
	unsigned int databuf[2] = {0};
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);

	if (sscanf(buf, "%d %d", &databuf[0], &databuf[1]) == 2) {
		aw8624->haptic_audio.delay_val = databuf[0];
		aw8624->haptic_audio.timer_val = databuf[1];
	}
	return count;
}

static int aw8624_clock_OSC_trim_calibration(struct aw8624 *aw8624,
					     unsigned long int theory_time)
{
	unsigned int real_code = 0;
	unsigned int LRA_TRIM_CODE = 0;
	unsigned int DFT_LRA_TRIM_CODE = 0;
	unsigned int Not_need_cali_threshold = 10;/*0.1 percent not need cali*/
	unsigned long int real_time = aw8624->microsecond;

	aw_dev_dbg("%s enter\n", __func__);
	if (theory_time == real_time) {
		aw_dev_info("aw_osctheory_time == real_time:%ld,", real_time);
		aw_dev_info("theory_time = %ld not need to cali\n",
			    theory_time);
		return 0;
	} else if (theory_time < real_time) {
		if ((real_time - theory_time) > (theory_time / 25)) {
			aw_dev_info("%s: failed not to cali\n", __func__);
			return DFT_LRA_TRIM_CODE;
		}

		if ((real_time - theory_time) <
				(Not_need_cali_threshold*theory_time/10000)) {
			aw_dev_info("aw_oscmicrosecond:%ld,theory_time = %ld\n",
				    real_time, theory_time);
			return DFT_LRA_TRIM_CODE;
		}

		real_code = ((real_time - theory_time) * 4) / (theory_time / 1000);
		real_code = ((real_code%10 < 5) ? 0 : 1) + real_code/10;
		real_code = 32 + real_code;
	} else if (theory_time > real_time) {
		if ((theory_time - real_time) > (theory_time / 25)) {
			aw_dev_info("failed not to cali\n");
			return DFT_LRA_TRIM_CODE;
		}
		if ((theory_time - real_time) <
				(Not_need_cali_threshold * theory_time/10000)) {
			aw_dev_info("aw_oscmicrosecond:%ld,theory_time = %ld\n",
				    real_time, theory_time);
			return DFT_LRA_TRIM_CODE;
		}
		real_code = ((theory_time - real_time) * 4) / (theory_time / 1000);
		real_code = ((real_code%10 < 5) ? 0 : 1) + real_code/10;
		real_code = 32 - real_code;
	}
	if (real_code > 31)
		LRA_TRIM_CODE = real_code - 32;
	else
		LRA_TRIM_CODE = real_code + 32;

	aw_dev_info("aw_oscmicrosecond:%ld,theory_time = %ld,real_code =0X%02X,",
		    real_time, theory_time, real_code);
	aw_dev_info("LRA_TRIM_CODE 0X%02X\n", LRA_TRIM_CODE);

	return LRA_TRIM_CODE;
}

static int aw8624_rtp_trim_lra_calibration(struct aw8624 *aw8624)
{
	unsigned char reg_val = 0;
	unsigned int fre_val = 0;
	unsigned int theory_time = 0;
	unsigned int lra_rtim_code = 0;
	unsigned int rate = 0;

	aw_dev_dbg("%s enter\n", __func__);
	aw8624_i2c_read(aw8624, AW8624_REG_PWMDBG, &reg_val,
			AW_I2C_BYTE_ONE);
	fre_val = (reg_val & 0x006f) >> 5;

	if (fre_val == AW8624_CLK_12K)
		rate = 12000;
	else if (fre_val == AW8624_CLK_24K)
		rate = 24000;
	else
		rate = 48000;
	theory_time = (aw8624->rtp_len / rate) * 1000000;
	aw_dev_info("microsecond:%ld  theory_time = %d\n",
		    aw8624->microsecond, theory_time);

	lra_rtim_code = aw8624_clock_OSC_trim_calibration(aw8624, theory_time);
	if (lra_rtim_code >= 0) {
		aw8624->lra_calib_data = lra_rtim_code;
		reg_val = (unsigned char)lra_rtim_code;
		aw8624_i2c_write(aw8624, AW8624_REG_TRIM_LRA,
				 &reg_val, AW_I2C_BYTE_ONE);
	}
	return 0;
}

static unsigned char aw8624_haptic_osc_read_int(struct aw8624 *aw8624)
{
	unsigned char reg_val = 0;

	aw8624_i2c_read(aw8624, AW8624_REG_DBGSTAT, &reg_val,
			AW_I2C_BYTE_ONE);
	return reg_val;
}

static int aw8624_osc_calculation_time(struct aw8624 *aw8624)
{
	unsigned char osc_int_state = 0;
	unsigned int base_addr = aw8624->ram.base_addr;
	unsigned int buf_len = 0;
	int ret = -1;
	struct aw8624_container *rtp_container = aw8624->rtp_container;

	if (!aw8624_haptic_rtp_get_fifo_afi(aw8624)) {
		aw_dev_dbg("%s: haptic_rtp_get_fifo_afi, rtp_cnt= %d\n",
			    __func__, aw8624->rtp_cnt);

		mutex_lock(&aw8624->rtp_lock);
		if ((rtp_container->len - aw8624->rtp_cnt) < (base_addr >> 2))
			buf_len = rtp_container->len - aw8624->rtp_cnt;
		else
			buf_len = (base_addr >> 2);

		if (aw8624->rtp_cnt != rtp_container->len) {
			if (aw8624->timeval_flags == 1) {
			aw8624->kstart = ktime_get();
			aw8624->timeval_flags = 0;
			}
			aw8624_i2c_write(aw8624,
					AW8624_REG_RTP_DATA,
					&rtp_container->data[aw8624->rtp_cnt],
					buf_len);
			aw8624->rtp_cnt += buf_len;
		}
		mutex_unlock(&aw8624->rtp_lock);
	}

	osc_int_state = aw8624_haptic_osc_read_int(aw8624);
	if (osc_int_state&AW8624_BIT_SYSINT_DONEI) {
		aw8624->kend = ktime_get();
		aw_dev_info("%s vincent playback aw8624->rtp_cnt= %d\n",
			    __func__, aw8624->rtp_cnt);
		return ret;
	}

	aw8624->kend = ktime_get();
	aw8624->microsecond = ktime_to_us(ktime_sub(aw8624->kend,
						aw8624->kstart));
	if (aw8624->microsecond > AW8624_OSC_CALIBRATION_T_LENGTH) {
		aw_dev_info("%s:vincent time out aw8624->rtp_cnt %d,",
			    __func__, aw8624->rtp_cnt);
		aw_dev_info("%s: osc_int_state %02x\n", __func__, osc_int_state);
		return ret;
	}
	return 0;
}

static int aw8624_rtp_osc_calibration(struct aw8624 *aw8624)
{
	int ret = -1;
	const struct firmware *rtp_file;

	aw8624->rtp_cnt = 0;
	aw8624->timeval_flags = 1;
	aw8624->osc_cali_flag = 1;

	aw_dev_dbg("%s enter\n", __func__);
	/* fw loaded */
	ret = request_firmware(&rtp_file,
				aw8624_rtp_name[0],/*aw8624->rtp_file_num */
				aw8624->dev);
	if (ret < 0) {
		/*aw8624->rtp_file_num */
		aw_dev_err("%s: failed to read %s\n", __func__,
			   aw8624_rtp_name[0]);
		return ret;
	}

	/*awinic add stop,for irq interrupt during calibrate*/
	aw8624_haptic_stop_l(aw8624);
	aw8624->rtp_init = 0;
	mutex_lock(&aw8624->rtp_lock);
	vfree(aw8624->rtp_container);
	aw8624->rtp_container = vmalloc(rtp_file->size + sizeof(int));
	if (!aw8624->rtp_container) {
		release_firmware(rtp_file);
		mutex_unlock(&aw8624->rtp_lock);
		aw_dev_err("%s: error allocating memory\n", __func__);
		return -ENOMEM;
	}
	aw8624->rtp_container->len = rtp_file->size;
	aw8624->rtp_len = rtp_file->size;
	/*aw8624->rtp_file_num */
	aw_dev_info("%s: rtp file [%s] size = %d\n", __func__,
		aw8624_rtp_name[0], aw8624->rtp_container->len);
	memcpy(aw8624->rtp_container->data, rtp_file->data, rtp_file->size);
	release_firmware(rtp_file);
	mutex_unlock(&aw8624->rtp_lock);

	/* gain */
	aw8624_haptic_ram_vbat_comp(aw8624, false);

	/* rtp mode config */
	aw8624_haptic_play_mode(aw8624, AW8624_HAPTIC_RTP_MODE);

	aw8624_i2c_write_bits(aw8624, AW8624_REG_DBGCTRL,
			      AW8624_BIT_DBGCTRL_INT_MODE_MASK,
			      AW8624_BIT_DBGCTRL_INT_MODE_EDGE);
	disable_irq(gpio_to_irq(aw8624->irq_gpio));
	/* haptic start */
	aw8624_haptic_start(aw8624);
	pm_qos_add_request(&aw8624_pm_qos_req_vb, PM_QOS_CPU_DMA_LATENCY,
							AW8624_PM_QOS_VALUE_VB);
	while (1) {
		ret = aw8624_osc_calculation_time(aw8624);
		if (ret < 0)
			break;
	}
	pm_qos_remove_request(&aw8624_pm_qos_req_vb);
	enable_irq(gpio_to_irq(aw8624->irq_gpio));

	aw8624->osc_cali_flag = 0;
	aw8624->microsecond = ktime_to_us(ktime_sub(aw8624->kend,
							aw8624->kstart));
	/*calibration osc*/
	aw_dev_info("%s awinic_microsecond:%ld\n", __func__,
		    aw8624->microsecond);
	aw_dev_info("%s exit\n", __func__);
	return 0;
}

static ssize_t aw8624_osc_cali_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	ssize_t len = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);

	len +=
		snprintf(buf + len, PAGE_SIZE - len, "lra_calib_data=%d\n",
			aw8624->lra_calib_data);

	return len;
}

static ssize_t aw8624_osc_cali_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned int val = 0;
	int rc = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);


	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;
	mutex_lock(&aw8624->lock);
	/* osc calibration flag start,Other behaviors are forbidden */
	aw8624->osc_cali_run = 1;
	if (val == 1) {
		aw8624_haptic_upload_lra(aw8624, AW8624_HAPTIC_ZERO);
		aw8624_rtp_osc_calibration(aw8624);
		aw8624_rtp_trim_lra_calibration(aw8624);
	} else if (val == 2) {
		aw8624_haptic_upload_lra(aw8624, AW8624_HAPTIC_RTP_CALI_LRA);
		aw8624_rtp_osc_calibration(aw8624);
	} else {
		aw_dev_err("%s input value out of range\n", __func__);
	}
	aw8624->osc_cali_run = 0;
	/* osc calibration flag end,Other behaviors are permitted */
	mutex_unlock(&aw8624->lock);

	return count;
}




static enum hrtimer_restart aw8624_vibrator_timer_func(struct hrtimer *timer)
{
	struct aw8624 *aw8624 = container_of(timer, struct aw8624, timer);

	aw_dev_dbg("%s enter\n", __func__);
	aw8624->state = 0;
	schedule_work(&aw8624->vibrator_work);

	return HRTIMER_NORESTART;
}


static void aw8624_haptic_ram_play(struct aw8624 *aw8624, unsigned char type)
{
	aw_dev_info("%s index = %d\n", __func__, aw8624->index);

	mutex_lock(&aw8624->ram_lock);
	if (type == AW8624_HAPTIC_RAM_LOOP_MODE)
		aw8624_haptic_set_repeat_wav_seq(aw8624, aw8624->index);

	if (type == AW8624_HAPTIC_RAM_MODE) {
		aw8624_haptic_set_wav_seq(aw8624, 0x00, aw8624->index);
		aw8624_haptic_set_wav_seq(aw8624, 0x01, 0x00);
		aw8624_haptic_set_wav_loop(aw8624, 0x00, 0x00);
	}
	aw8624_haptic_play_mode(aw8624, type);
	aw8624_haptic_start(aw8624);
	mutex_unlock(&aw8624->ram_lock);
}
static void aw8624_vibrator_work_routine(struct work_struct *work)
{
	struct aw8624 *aw8624 = container_of(work, struct aw8624,
					     vibrator_work);

	aw_dev_dbg("%s enter\n", __func__);

	mutex_lock(&aw8624->lock);
	aw8624_haptic_stop_l(aw8624);
	aw8624_haptic_upload_lra(aw8624, AW8624_HAPTIC_F0_CALI_LRA);
	if (aw8624->state) {
		if (aw8624->activate_mode == AW8624_HAPTIC_RAM_MODE) {
			aw8624_haptic_ram_vbat_comp(aw8624, true);
			aw8624_haptic_ram_play(aw8624, AW8624_HAPTIC_RAM_MODE);
		} else if (aw8624->activate_mode ==
					AW8624_HAPTIC_RAM_LOOP_MODE) {
			aw8624_haptic_ram_vbat_comp(aw8624, true);
			aw8624_haptic_ram_play(aw8624,
					       AW8624_HAPTIC_RAM_LOOP_MODE);
			/* run ms timer */
			hrtimer_start(&aw8624->timer,
				      ktime_set(aw8624->duration / 1000,
				      (aw8624->duration % 1000) * 1000000),
				      HRTIMER_MODE_REL);
		} else if (aw8624->activate_mode ==
					AW8624_HAPTIC_CONT_MODE) {
			aw8624_haptic_cont(aw8624);
			/* run ms timer */
			hrtimer_start(&aw8624->timer,
				      ktime_set(aw8624->duration / 1000,
				      (aw8624->duration % 1000) * 1000000),
				      HRTIMER_MODE_REL);
		} else {
			 aw_dev_err("%s: activate_mode error\n",
				   __func__);
		}
	}
	mutex_unlock(&aw8624->lock);
}



/******************************************************
 *
 * irq
 *
 ******************************************************/
void aw8624_interrupt_setup_l(struct aw8624 *aw8624)
{
	unsigned char reg_val = 0;

	aw8624_i2c_read(aw8624, AW8624_REG_SYSINT, &reg_val,
			AW_I2C_BYTE_ONE);
	aw_dev_info("%s: reg SYSINT=0x%x\n", __func__, reg_val);

	aw8624_i2c_write_bits(aw8624, AW8624_REG_DBGCTRL,
		AW8624_BIT_DBGCTRL_INT_MODE_MASK,
		AW8624_BIT_DBGCTRL_INT_MODE_EDGE);

	aw8624_i2c_write_bits(aw8624, AW8624_REG_SYSINTM,
			      (AW8624_BIT_SYSINTM_UVLO_MASK &
			       AW8624_BIT_SYSINTM_OCD_MASK &
			       AW8624_BIT_SYSINTM_OT_MASK),
			      (AW8624_BIT_SYSINTM_UVLO_EN |
			       AW8624_BIT_SYSINTM_OCD_EN |
			       AW8624_BIT_SYSINTM_OT_EN));
}

static ssize_t aw8624_gun_type_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);

	return snprintf(buf, PAGE_SIZE, "0x%02x\n", aw8624->gun_type);
}

static ssize_t aw8624_gun_type_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	unsigned int val = 0;
	int rc = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;

	aw_dev_dbg("%s: value=%d\n", __func__, val);

	mutex_lock(&aw8624->lock);
	aw8624->gun_type = val;
	mutex_unlock(&aw8624->lock);
	return count;
}

static ssize_t aw8624_bullet_nr_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);

	return snprintf(buf, PAGE_SIZE, "0x%02x\n", aw8624->bullet_nr);
}

static ssize_t aw8624_bullet_nr_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	unsigned int val = 0;
	int rc = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;

	aw_dev_dbg("%s: value=%d\n", __func__, val);

	mutex_lock(&aw8624->lock);
	aw8624->bullet_nr = val;
	mutex_unlock(&aw8624->lock);
	return count;
}

static ssize_t aw8624_trig_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	ssize_t len = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);

	len += snprintf(buf + len, PAGE_SIZE - len,
			"trig: trig_enable=%d, trig_edge=%d",
			aw8624->trig.trig_enable, aw8624->trig.trig_edge);
	len += snprintf(buf + len, PAGE_SIZE - len,
			"trig_polar=%d, pos_sequence=%d, neg_sequence=%d\n",
			aw8624->trig.trig_polar, aw8624->trig.pos_sequence,
			aw8624->trig.neg_sequence);
	return len;

}

static ssize_t aw8624_trig_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	unsigned int databuf[5] = { 0 };
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);

	if (!aw8624->ram_init) {
		aw_dev_err("%s: ram init failed, not allow to play!\n",
			   __func__);
		return count;
	}
	if (sscanf(buf, "%d %d %d %d %d",
		&databuf[0], &databuf[1],
		&databuf[2], &databuf[3], &databuf[4]) == 5) {
		if (databuf[0] > 1)
			databuf[0] = 1;
		if (databuf[0] < 0)
			databuf[0] = 0;
		if (databuf[1] > 1)
			databuf[0] = 1;
		if (databuf[1] < 0)
			databuf[0] = 0;
		if (databuf[2] > 1)
			databuf[0] = 1;
		if (databuf[2] < 0)
			databuf[0] = 0;
		if (databuf[3] > aw8624->ram.ram_num ||
		    databuf[4] > aw8624->ram.ram_num) {
			aw_dev_err("%s: input seq value out of range!\n",
				   __func__);
			return count;
		}
		aw8624->trig.trig_enable = databuf[0];
		aw8624->trig.trig_edge = databuf[1];
		aw8624->trig.trig_polar = databuf[2];
		aw8624->trig.pos_sequence = databuf[3];
		aw8624->trig.neg_sequence = databuf[4];
		mutex_lock(&aw8624->lock);
		aw8624_haptic_tirg1_param_config(aw8624);
		mutex_unlock(&aw8624->lock);
	} else
		aw_dev_err("%s: please input five parameters\n",
				   __func__);
	return count;
}

static ssize_t aw8624_ram_num_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	ssize_t len = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);

	aw8624_haptic_get_ram_number(aw8624);

	len += snprintf(buf + len, PAGE_SIZE - len,
			"ram_num = %d\n", aw8624->ram.ram_num);
	return len;

}

static ssize_t aw8624_awrw_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);
	unsigned char reg_num = aw8624->aw_i2c_package.reg_num;
	unsigned char flag = aw8624->aw_i2c_package.flag;
	unsigned char *reg_data = aw8624->aw_i2c_package.reg_data;
	unsigned char i = 0;
	ssize_t len = 0;


	if (!reg_num) {
		aw_dev_err("%s: awrw parameter error\n",
			   __func__);
		return len;
	}
	if (flag == AW8624_READ) {
		for (i = 0; i < reg_num; i++) {
			len += snprintf(buf + len, PAGE_SIZE - len,
					"0x%02x,", reg_data[i]);
		}

		len += snprintf(buf + len - 1, PAGE_SIZE - len, "\n");
	}

	return len;
}

static ssize_t aw8624_awrw_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	unsigned int datatype[3] = { 0 };
	int ret = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8624 *aw8624 = container_of(cdev, struct aw8624, vib_dev);

	if (sscanf(buf, "%x %x %x", &datatype[0], &datatype[1],
				    &datatype[2]) == 3) {
		if (!datatype[1]) {
			aw_dev_err("%s: awrw parameter error\n",
				   __func__);
			return count;
		}
		aw8624->aw_i2c_package.flag = (unsigned char)datatype[0];
		aw8624->aw_i2c_package.reg_num = (unsigned char)datatype[1];
		aw8624->aw_i2c_package.first_addr = (unsigned char)datatype[2];

		if (aw8624->aw_i2c_package.flag == AW8624_WRITE) {
			ret = aw8624_parse_data(aw8624, buf);
			if (ret < 0)
				return count;
			aw8624_i2c_write(aw8624,
					 aw8624->aw_i2c_package.first_addr,
					 aw8624->aw_i2c_package.reg_data,
					 aw8624->aw_i2c_package.reg_num);
		}
		if (aw8624->aw_i2c_package.flag == AW8624_READ)
			aw8624_i2c_read(aw8624,
					aw8624->aw_i2c_package.first_addr,
					aw8624->aw_i2c_package.reg_data,
					aw8624->aw_i2c_package.reg_num);
	} else
		aw_dev_err("%s: missing number of parameters\n",
			   __func__);

	return count;
}

#if 0
/*
*   schedule_work is low priority.
*   aw8624 rtp mode can`t be interrupted.
*/
static irqreturn_t aw8624_irq_l(int irq, void *data)
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
static DEVICE_ATTR(ram_vbat_comp, 0664,
		aw8624_ram_vbat_comp_show, aw8624_ram_vbat_comp_store);
static DEVICE_ATTR(rtp, 0664, aw8624_rtp_show, aw8624_rtp_store);
static DEVICE_ATTR(osc_cali, 0664, aw8624_osc_cali_show, aw8624_osc_cali_store);
static DEVICE_ATTR(gun_type, 0664, aw8624_gun_type_show, aw8624_gun_type_store);
static DEVICE_ATTR(bullet_nr, 0664,
		aw8624_bullet_nr_show, aw8624_bullet_nr_store);
static DEVICE_ATTR(trig, 0664,
		aw8624_trig_show, aw8624_trig_store);
static DEVICE_ATTR(ram_num, 0664, aw8624_ram_num_show, NULL);
static DEVICE_ATTR(awrw, 0644, aw8624_awrw_show, aw8624_awrw_store);
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
	&dev_attr_rtp.attr,
	&dev_attr_osc_cali.attr,
	&dev_attr_gun_type.attr,
	&dev_attr_bullet_nr.attr,
	&dev_attr_trig.attr,
	&dev_attr_ram_num.attr,
	&dev_attr_awrw.attr,
	NULL
};

struct attribute_group aw8624_vibrator_attribute_group_l = {
	.attrs = aw8624_vibrator_attributes
};
int aw8624_vibrator_init_l(struct aw8624 *aw8624)
{
	int ret = 0;

	aw_dev_dbg("%s enter\n", __func__);

#ifdef TIMED_OUTPUT
	aw8624->vib_dev.name = "awinic_vibrator_l";
	aw8624->vib_dev.get_time = aw8624_vibrator_get_time;
	aw8624->vib_dev.enable = aw8624_vibrator_enable;

	ret = timed_output_dev_register(&(aw8624->vib_dev));
	if (ret < 0) {
		dev_err(aw8624->dev, "%s: fail to create timed output dev\n",
			__func__);
		return ret;
	}
	ret = sysfs_create_group(&aw8624->vib_dev.dev->kobj,
				&aw8624_vibrator_attribute_group_l);
	if (ret < 0) {
		dev_err(aw8624->dev,
			"%s error creating sysfs attr files\n",
			__func__);
		return ret;
	}
#else
	aw8624->vib_dev.name = "awinic_vibrator_l";
	aw8624->vib_dev.brightness_set = aw8624_vibrator_enable;


	ret = devm_led_classdev_register(&aw8624->i2c->dev, &(aw8624->vib_dev));
	if (ret < 0) {
		dev_err(aw8624->dev, "%s: fail to create leds dev\n",
				__func__);
		return ret;
	}

	ret = sysfs_create_group(&aw8624->vib_dev.dev->kobj,
				&aw8624_vibrator_attribute_group_l);
	if (ret < 0) {
		dev_err(aw8624->dev, "%s error creating sysfs attr files\n",
			__func__);
		return ret;
	}
#endif
	hrtimer_init(&aw8624->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	aw8624->timer.function = aw8624_vibrator_timer_func;
	INIT_WORK(&aw8624->vibrator_work, aw8624_vibrator_work_routine);

	if (aw8624->IsUsedIRQ)
		INIT_WORK(&aw8624->rtp_work, aw8624_rtp_work_routine);

	mutex_init(&aw8624->lock);
	mutex_init(&aw8624->rtp_lock);
	mutex_init(&aw8624->ram_lock);


	return 0;
}


static int aw8624_is_rtp_load_end(struct aw8624 *aw8624)
{
	unsigned char glb_st = 0;
	int ret = -1;

	aw8624_i2c_read(aw8624, AW8624_REG_GLB_STATE, &glb_st,
			AW_I2C_BYTE_ONE);
	if ((aw8624->rtp_cnt == aw8624->rtp_container->len) ||
			((glb_st & 0x0f) == 0x00)) {
		if (aw8624->rtp_cnt ==
			aw8624->rtp_container->len)
			aw_dev_info("%s:rtp load completely.",
				    __func__);
		else
			aw_dev_err("%s rtp load failed!!\n",
				   __func__);
		aw8624_haptic_set_rtp_aei(aw8624,
					false);
		aw8624->rtp_cnt = 0;
		aw8624->rtp_init = 0;
		ret = 0;
	}
	return ret;
}

static int aw8624_write_rtp_data(struct aw8624 *aw8624)
{
	unsigned int buf_len = 0;
	int ret = -1;

	if (!aw8624->rtp_cnt) {
		aw_dev_info("%s:aw8624->rtp_cnt is 0!\n",
			__func__);
		return ret;
	}
	if (!aw8624->rtp_container) {
		aw_dev_info("%s:aw8624->rtp_container is null, break!\n",
			__func__);
		return ret;
	}
	if ((aw8624->rtp_container->len - aw8624->rtp_cnt) <
	(aw8624->ram.base_addr >> 2)) {
		buf_len =
		aw8624->rtp_container->len - aw8624->rtp_cnt;
	} else {
		buf_len = (aw8624->ram.base_addr >> 2);
	}
	aw8624_i2c_write(aw8624, AW8624_REG_RTP_DATA,
			 &aw8624->rtp_container->data[aw8624->rtp_cnt],
			 buf_len);
	aw8624->rtp_cnt += buf_len;

	return 0;
}


irqreturn_t aw8624_irq_l(int irq, void *data)
{
	unsigned char reg_val = 0;
	unsigned char glb_st = 0;
	int ret = 0;
	struct aw8624 *aw8624 = data;

	aw8624_i2c_read(aw8624, AW8624_REG_SYSINT, &reg_val,
			AW_I2C_BYTE_ONE);
	if (reg_val & AW8624_BIT_SYSINT_UVLI)
		aw_dev_err("%s chip uvlo int error\n", __func__);
	if (reg_val & AW8624_BIT_SYSINT_OCDI)
		aw_dev_err("%s chip over current int error\n",
			   __func__);
	if (reg_val & AW8624_BIT_SYSINT_OTI)
		aw_dev_err("%s chip over temperature int error\n",
			   __func__);
	if (reg_val & AW8624_BIT_SYSINT_DONEI)
		aw_dev_info("%s chip playback done\n", __func__);


	if (reg_val & AW8624_BIT_SYSINT_UVLI) {
		aw8624_i2c_read(aw8624, AW8624_REG_GLB_STATE, &glb_st,
				AW_I2C_BYTE_ONE);
		if (glb_st == AW8624_BIT_GLBRD5_STATE_STANDBY) {
			aw8624_i2c_write_bits(aw8624,
					      AW8624_REG_SYSINTM,
					      AW8624_BIT_SYSINTM_UVLO_MASK,
					      AW8624_BIT_SYSINTM_UVLO_OFF);
		}
	}

	if (reg_val & AW8624_BIT_SYSINT_FF_AEI) {
		aw_dev_dbg("%s: aw8624 rtp fifo almost empty\n",
			    __func__);
		if (aw8624->rtp_init) {
			while ((!aw8624_haptic_rtp_get_fifo_afs(aw8624)) &&
			(aw8624->play_mode == AW8624_HAPTIC_RTP_MODE)) {
				mutex_lock(&aw8624->rtp_lock);
#ifdef AW_ENABLE_RTP_PRINT_LOG
			aw_dev_info("%s: aw8624 rtp mode fifo update, cnt=%d\n",
				__func__, aw8624->rtp_cnt);
#endif
				ret = aw8624_write_rtp_data(aw8624);
				if (ret < 0) {
					mutex_unlock(&aw8624->rtp_lock);
					break;
				}
				ret = aw8624_is_rtp_load_end(aw8624);
				if (!ret) {
					mutex_unlock(&aw8624->rtp_lock);
					break;
				}
				mutex_unlock(&aw8624->rtp_lock);
			}
		} else {
			aw_dev_err( "%s: aw8624 rtp init = %d, init error\n",
				    __func__, aw8624->rtp_init);
		}
	}

	if (reg_val & AW8624_BIT_SYSINT_FF_AFI)
		aw_dev_dbg("%s: aw8624 rtp mode fifo full\n", __func__);

	if (aw8624->play_mode != AW8624_HAPTIC_RTP_MODE)
		aw8624_haptic_set_rtp_aei(aw8624, false);

	return IRQ_HANDLED;
}

static int aw8624_analyse_duration_range(struct aw8624 *aw8624)
{
	int i = 0;
	int ret = 0;
	int len = 0;
	int *duration_time = NULL;

	len = ARRAY_SIZE(aw8624_dts_data.aw8624_duration_time);
	duration_time = aw8624_dts_data.aw8624_duration_time;
	if (len < 2) {
		aw_dev_err("%s: duration time range error\n", __func__);
		return -ERANGE;
	}
	for (i = (len - 1); i > 0; i--) {
		if (duration_time[i] > duration_time[i-1])
			continue;
		else
			break;

	}
	if (i > 0) {
		aw_dev_err("%s: duration time range error\n", __func__);
		ret = -ERANGE;
	}
	return ret;
}

static int
aw8624_analyse_duration_array_size(struct aw8624 *aw8624,
				   struct device_node *np)
{
	int ret = 0;

	ret = of_property_count_elems_of_size(np, "aw8624_vib_duration_time",
					     4);
	if (ret < 0) {
		aw8624->duration_time_flag = -1;
		aw_dev_info("%s vib_duration_time not found\n", __func__);
		return ret;
	}
	aw8624->duration_time_size = ret;
	if (aw8624->duration_time_size > 3) {
		aw8624->duration_time_flag = -1;
		aw_dev_info("%s vib_duration_time error, array size = %d\n",
			    __func__, aw8624->duration_time_size);
		return -ERANGE;
	}
	return 0;
}

int aw8624_parse_dt_l(struct aw8624 *aw8624, struct device *dev,
		    struct device_node *np) {
	unsigned int val = 0;
	/*unsigned int brake_ram_config[24];*/
	unsigned int brake_cont_config[24];
	unsigned int f0_trace_parameter[4];
	unsigned int bemf_config[4];
	unsigned int duration_time[3];
	unsigned int sw_brake[2];
	unsigned int trig_config_temp[5];
	int ret = 0;

	val =
	of_property_read_u32(np, "aw8624_vib_lk_f0_cali",
			&aw8624_dts_data.aw8624_lk_f0_cali);
	if (val != 0)
		aw_dev_info("aw8624_vib_mode not found\n");
	aw_dev_info("%s: aw8624_vib_lk_f0_cali = 0x%02x\n",
		    __func__, aw8624_dts_data.aw8624_lk_f0_cali);
	val =
	of_property_read_u32(np, "aw8624_vib_mode",
			&aw8624_dts_data.aw8624_mode);
	if (val != 0)
		aw_dev_info("aw8624_vib_mode not found\n");
	val =
	of_property_read_u32(np, "aw8624_vib_f0_pre",
			&aw8624_dts_data.aw8624_f0_pre);
	if (val != 0)
		aw_dev_info("aw8624_vib_f0_pre not found\n");
	val =
	of_property_read_u32(np, "aw8624_vib_f0_cali_percen",
				&aw8624_dts_data.aw8624_f0_cali_percen);
	if (val != 0)
		aw_dev_info("aw8624_vib_f0_cali_percen not found\n");
	val =
	of_property_read_u32(np, "aw8624_vib_cont_drv_lev",
				&aw8624_dts_data.aw8624_cont_drv_lvl);
	if (val != 0)
		aw_dev_info("aw8624_vib_cont_drv_lev not found\n");
	val =
	of_property_read_u32(np, "aw8624_vib_cont_drv_lvl_ov",
				&aw8624_dts_data.aw8624_cont_drv_lvl_ov);
	if (val != 0)
		aw_dev_info("aw8624_vib_cont_drv_lvl_ov not found\n");
	val =
	of_property_read_u32(np, "aw8624_vib_cont_td",
				&aw8624_dts_data.aw8624_cont_td);
	if (val != 0)
		aw_dev_info("aw8624_vib_cont_td not found\n");
	val =
	of_property_read_u32(np, "aw8624_vib_cont_zc_thr",
				&aw8624_dts_data.aw8624_cont_zc_thr);
	if (val != 0)
		aw_dev_info("aw8624_vib_cont_zc_thr not found\n");
	val =
	of_property_read_u32(np, "aw8624_vib_cont_num_brk",
				&aw8624_dts_data.aw8624_cont_num_brk);
	if (val != 0)
		aw_dev_info("aw8624_vib_cont_num_brk not found\n");
	val =
	of_property_read_u32(np, "aw8624_vib_f0_coeff",
				&aw8624_dts_data.aw8624_f0_coeff);
	if (val != 0)
		aw_dev_info("aw8624_vib_f0_coeff not found\n");
	val = of_property_read_u32_array(np, "aw8624_vib_brake_cont_config",
		brake_cont_config, ARRAY_SIZE(brake_cont_config));
	if (val != 0)
		aw_dev_info("%s vib_brake_cont_config not found\n", __func__);
	memcpy(aw8624_dts_data.aw8624_cont_brake,
		brake_cont_config, sizeof(brake_cont_config));

	val = of_property_read_u32_array(np, "aw8624_vib_f0_trace_parameter",
		f0_trace_parameter, ARRAY_SIZE(f0_trace_parameter));
	if (val != 0)
		aw_dev_info("%s vib_f0_trace_parameter not found\n", __func__);
	memcpy(aw8624_dts_data.aw8624_f0_trace_parameter,
		f0_trace_parameter, sizeof(f0_trace_parameter));

	val = of_property_read_u32_array(np, "aw8624_vib_bemf_config",
		bemf_config, ARRAY_SIZE(bemf_config));
	if (val != 0)
		aw_dev_info("%s vib_bemf_config not found\n", __func__);
	memcpy(aw8624_dts_data.aw8624_bemf_config,
		bemf_config, sizeof(bemf_config));

	val =
	of_property_read_u32_array(np, "aw8624_vib_sw_brake",
		sw_brake, ARRAY_SIZE(sw_brake));
	if (val != 0)
		aw_dev_info("%s vib_wavseq not found\n", __func__);
	memcpy(aw8624_dts_data.aw8624_sw_brake,
		sw_brake, sizeof(sw_brake));

	val = of_property_read_u32(np, "aw8624_vib_tset",
		&aw8624_dts_data.aw8624_tset);
	if (val != 0)
		aw_dev_info("%s vib_tset not found\n", __func__);
	val = of_property_read_u32_array(np, "aw8624_vib_duration_time",
		duration_time, ARRAY_SIZE(duration_time));
	if (val != 0)
		aw_dev_info("%s vib_duration_time not found\n", __func__);
	ret = aw8624_analyse_duration_array_size(aw8624, np);
	if (!ret)
		memcpy(aw8624_dts_data.aw8624_duration_time,
				duration_time, sizeof(duration_time));
	val =
	    of_property_read_u32_array(np,
				"aw8624_vib_trig_config",
				trig_config_temp,
				ARRAY_SIZE(trig_config_temp));
	if (val != 0)
		aw_dev_info("%s vib_trig_config not found\n",
			    __func__);
	memcpy(aw8624_dts_data.trig_config, trig_config_temp,
	       sizeof(trig_config_temp));
	return 0;
}
