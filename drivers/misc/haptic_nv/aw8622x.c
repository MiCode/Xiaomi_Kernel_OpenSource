/*
 * aw8622x.c
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
#include <asm/uaccess.h>
#include <linux/syscalls.h>
#include <linux/power_supply.h>
#include <linux/pm_qos.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/control.h>
#include <sound/soc.h>
#include <linux/vmalloc.h>

#include "aw8622x.h"
#include "haptic_nv.h"

/******************************************************
 *
 * Register Access
 *
 ******************************************************/
static const unsigned char aw8622x_reg_list[] = {
	AW8622X_REG_ID,
	AW8622X_REG_SYSST,
	AW8622X_REG_SYSINT,
	AW8622X_REG_SYSINTM,
	AW8622X_REG_SYSST2,
	AW8622X_REG_SYSER,
	AW8622X_REG_PLAYCFG2,
	AW8622X_REG_PLAYCFG3,
	AW8622X_REG_PLAYCFG4,
	AW8622X_REG_WAVCFG1,
	AW8622X_REG_WAVCFG2,
	AW8622X_REG_WAVCFG3,
	AW8622X_REG_WAVCFG4,
	AW8622X_REG_WAVCFG5,
	AW8622X_REG_WAVCFG6,
	AW8622X_REG_WAVCFG7,
	AW8622X_REG_WAVCFG8,
	AW8622X_REG_WAVCFG9,
	AW8622X_REG_WAVCFG10,
	AW8622X_REG_WAVCFG11,
	AW8622X_REG_WAVCFG12,
	AW8622X_REG_WAVCFG13,
	AW8622X_REG_CONTCFG1,
	AW8622X_REG_CONTCFG2,
	AW8622X_REG_CONTCFG3,
	AW8622X_REG_CONTCFG4,
	AW8622X_REG_CONTCFG5,
	AW8622X_REG_CONTCFG6,
	AW8622X_REG_CONTCFG7,
	AW8622X_REG_CONTCFG8,
	AW8622X_REG_CONTCFG9,
	AW8622X_REG_CONTCFG10,
	AW8622X_REG_CONTCFG11,
	AW8622X_REG_CONTCFG13,
	AW8622X_REG_CONTRD14,
	AW8622X_REG_CONTRD15,
	AW8622X_REG_CONTRD16,
	AW8622X_REG_CONTRD17,
	AW8622X_REG_RTPCFG1,
	AW8622X_REG_RTPCFG2,
	AW8622X_REG_RTPCFG3,
	AW8622X_REG_RTPCFG4,
	AW8622X_REG_RTPCFG5,
	AW8622X_REG_TRGCFG1,
	AW8622X_REG_TRGCFG2,
	AW8622X_REG_TRGCFG3,
	AW8622X_REG_TRGCFG4,
	AW8622X_REG_TRGCFG5,
	AW8622X_REG_TRGCFG6,
	AW8622X_REG_TRGCFG7,
	AW8622X_REG_TRGCFG8,
	AW8622X_REG_GLBCFG4,
	AW8622X_REG_GLBRD5,
	AW8622X_REG_RAMADDRH,
	AW8622X_REG_RAMADDRL,
	AW8622X_REG_SYSCTRL1,
	AW8622X_REG_SYSCTRL2,
	AW8622X_REG_SYSCTRL3,
	AW8622X_REG_SYSCTRL4,
	AW8622X_REG_SYSCTRL5,
	AW8622X_REG_SYSCTRL6,
	AW8622X_REG_SYSCTRL7,
	AW8622X_REG_PWMCFG1,
	AW8622X_REG_PWMCFG3,
	AW8622X_REG_PWMCFG4,
	AW8622X_REG_DETCFG1,
	AW8622X_REG_DETCFG2,
	AW8622X_REG_DET_RL,
	AW8622X_REG_DET_VBAT,
	AW8622X_REG_DET_LO,
	AW8622X_REG_TRIMCFG1,
	AW8622X_REG_TRIMCFG3,
};

/******************************************************
 *
 * value
 *
 ******************************************************/
static char *aw8622x_ram_name = "aw8622x_haptic.bin";
static char aw8622x_rtp_name[][AW8622X_RTP_NAME_MAX] = {
	{"aw8622xl_osc_rtp_12K_10s.bin"},
	{"aw8622xl_rtp.bin"},
	{"aw8622xl_rtp_lighthouse.bin"},
	{"aw8622xl_rtp_silk.bin"},
};

static struct pm_qos_request aw8622x_pm_qos_req_vb;

 /******************************************************
 *
 * aw8622x i2c write/read
 *
 ******************************************************/
static int aw8622x_i2c_read(struct aw8622x *aw8622x, unsigned char reg_addr,
			   unsigned char *buf, unsigned int len)
{
	int ret = 0;

	struct i2c_msg msg[2] = {
		[0] = {
				.addr = aw8622x->i2c->addr,
				.flags = 0,
				.len = sizeof(uint8_t),
				.buf = &reg_addr,
				},
		[1] = {
				.addr = aw8622x->i2c->addr,
				.flags = I2C_M_RD,
				.len = len,
				.buf = buf,
				},
	};

	ret = i2c_transfer(aw8622x->i2c->adapter, msg, ARRAY_SIZE(msg));
	if (ret < 0) {
		aw_dev_err("%s: i2c_transfer failed\n", __func__);
		return ret;
	} else if (ret != AW_I2C_READ_MSG_NUM) {
		aw_dev_err("%s: transfer failed(size error)\n", __func__);
		return -ENXIO;
	}
	return ret;
}

static int aw8622x_i2c_write(struct aw8622x *aw8622x, unsigned char reg_addr,
			    unsigned char *buf, unsigned int len)
{
	unsigned char *data = NULL;
	int ret = -1;

	data = kmalloc(len + 1, GFP_KERNEL);
	if (data == NULL)
		return -ENOMEM;
	data[0] = reg_addr;
	memcpy(&data[1], buf, len);
	ret = i2c_master_send(aw8622x->i2c, data, len + 1);
	if (ret < 0)
		aw_dev_err("%s: i2c master send 0x%02x error\n",
			   __func__, reg_addr);
	kfree(data);
	return ret;
}

static int
aw8622x_i2c_write_bits(struct aw8622x *aw8622x, unsigned char reg_addr,
		       unsigned int mask,
		       unsigned char reg_data)
{
	unsigned char reg_val = 0;
	int ret = -1;

	ret = aw8622x_i2c_read(aw8622x, reg_addr, &reg_val, AW_I2C_BYTE_ONE);
	if (ret < 0) {
		aw_dev_err("%s: i2c read error, ret=%d\n", __func__, ret);
		return ret;
	}
	reg_val &= mask;
	reg_val |= reg_data;
	ret = aw8622x_i2c_write(aw8622x, reg_addr, &reg_val, AW_I2C_BYTE_ONE);
	if (ret < 0) {
		aw_dev_err("%s: i2c write error, ret=%d\n", __func__, ret);
		return ret;
	}
	return 0;
}

static int aw8622x_parse_data(struct aw8622x *aw8622x, const char *buf)
{
	unsigned char reg_num = aw8622x->aw_i2c_package.reg_num;
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
			aw_dev_err("%s: kstrtouint error\n",
				   __func__);
			return ret;
		}
		aw8622x->aw_i2c_package.reg_data[i] = (unsigned char)value;
	}
	return 0;
}

static unsigned char aw8622x_haptic_rtp_get_fifo_afs(struct aw8622x *aw8622x)
{
	unsigned char ret = 0;
	unsigned char reg_val = 0;

	aw8622x_i2c_read(aw8622x, AW8622X_REG_SYSST, &reg_val, AW_I2C_BYTE_ONE);
	reg_val &= AW8622X_BIT_SYSST_FF_AFS;
	ret = reg_val >> 3;
	return ret;
}

/*****************************************************
 *
 * rtp
 *
 *****************************************************/
static void aw8622x_haptic_set_rtp_aei(struct aw8622x *aw8622x, bool flag)
{
	if (flag) {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSINTM,
				       AW8622X_BIT_SYSINTM_FF_AEM_MASK,
				       AW8622X_BIT_SYSINTM_FF_AEM_ON);
	} else {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSINTM,
				       AW8622X_BIT_SYSINTM_FF_AEM_MASK,
				       AW8622X_BIT_SYSINTM_FF_AEM_OFF);
	}
}

static int aw8622x_analyse_duration_range(struct aw8622x *aw8622x)
{
	int i = 0;
	int ret = 0;
	int len = 0;
	int *duration_time = NULL;

	len = ARRAY_SIZE(aw8622x->dts_info.duration_time);
	duration_time = aw8622x->dts_info.duration_time;
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

static int aw8622x_analyse_duration_array_size(struct aw8622x *aw8622x,
					       struct device_node *np)
{
	int ret = 0;

	ret = of_property_count_elems_of_size(np, "aw8622x_vib_duration_time",
					      4);
	if (ret < 0) {
		aw8622x->duration_time_flag = -1;
		aw_dev_info("%s vib_duration_time not found\n", __func__);
		return ret;
	}
	aw8622x->duration_time_size = ret;
	if (aw8622x->duration_time_size > 3) {
		aw8622x->duration_time_flag = -1;
		aw_dev_info("%s vib_duration_time error, array size = %d\n",
			    __func__, aw8622x->duration_time_size);
		return -ERANGE;
	}
	return 0;
}

/*****************************************************
 *
 * device tree
 *
 *****************************************************/
int aw8622x_parse_dt_l(struct aw8622x *aw8622x, struct device *dev,
			    struct device_node *np)
{
	unsigned int val = 0;
	unsigned int prctmode_temp[3];
	unsigned int sine_array_temp[4];
	unsigned int trig_config_temp[21];
	unsigned int duration_time[3];
	int ret = 0;

	val = of_property_read_u32(np, "aw8622x_vib_lk_f0_cali",
			&aw8622x->dts_info.lk_f0_cali);
	if (val != 0)
		aw_dev_info("aw8622x_vib_mode not found\n");
	aw_dev_info("%s: aw8622x_vib_lk_f0_cali = 0x%02x\n", __func__,
		    aw8622x->dts_info.lk_f0_cali);
	val = of_property_read_u32(np,
			"aw8622x_vib_mode",
			&aw8622x->dts_info.mode);
	if (val != 0)
		aw_dev_info("%s aw8622x_vib_mode not found\n", __func__);
	val = of_property_read_u32(np,
			"aw8622x_vib_f0_pre",
			&aw8622x->dts_info.f0_ref);
	if (val != 0)
		aw_dev_info("%s vib_f0_ref not found\n", __func__);
	val =
	    of_property_read_u32(np, "aw8622x_vib_f0_cali_percen",
				 &aw8622x->dts_info.f0_cali_percent);
	if (val != 0)
		aw_dev_info("%s vib_f0_cali_percent not found\n",  __func__);

	val = of_property_read_u32(np, "aw8622x_vib_cont_drv1_lvl",
				   &aw8622x->dts_info.cont_drv1_lvl_dt);
	if (val != 0)
		aw_dev_info("%s vib_cont_drv1_lvl not found\n", __func__);
	val =
	    of_property_read_u32(np, "aw8622x_vib_cont_drv2_lvl",
				 &aw8622x->dts_info.cont_drv2_lvl_dt);
	if (val != 0)
		aw_dev_info("%s vib_cont_drv2_lvl not found\n", __func__);
	val =
	    of_property_read_u32(np, "aw8622x_vib_cont_drv1_time",
				 &aw8622x->dts_info.cont_drv1_time_dt);
	if (val != 0)
		aw_dev_info("%s vib_cont_drv1_time not found\n", __func__);
	val =
	    of_property_read_u32(np, "aw8622x_vib_cont_drv2_time",
				 &aw8622x->dts_info.cont_drv2_time_dt);
	if (val != 0)
		aw_dev_info("%s vib_cont_drv2_time not found\n", __func__);
	val =
	    of_property_read_u32(np, "aw8622x_vib_cont_drv_width",
				 &aw8622x->dts_info.cont_drv_width);
	if (val != 0)
		aw_dev_info("%s vib_cont_drv_width not found\n", __func__);
	val =
	    of_property_read_u32(np, "aw8622x_vib_cont_wait_num",
				 &aw8622x->dts_info.cont_wait_num_dt);
	if (val != 0)
		aw_dev_info("%s vib_cont_wait_num not found\n", __func__);
	val =
	    of_property_read_u32(np, "aw8622x_vib_cont_brk_gain",
				 &aw8622x->dts_info.cont_brk_gain);
	if (val != 0)
		aw_dev_info("%s vib_cont_brk_gain not found\n",  __func__);
	val =
	    of_property_read_u32(np, "aw8622x_vib_cont_tset",
				 &aw8622x->dts_info.cont_tset);
	if (val != 0)
		aw_dev_info("%s vib_cont_tset not found\n", __func__);
	val =
	    of_property_read_u32(np, "aw8622x_vib_cont_bemf_set",
				 &aw8622x->dts_info.cont_bemf_set);
	if (val != 0)
		aw_dev_info("%s vib_cont_bemf_set not found\n", __func__);
	val =
	    of_property_read_u32(np, "aw8622x_vib_d2s_gain",
				 &aw8622x->dts_info.d2s_gain);
	if (val != 0)
		aw_dev_info("%s vib_d2s_gain not found\n", __func__);
	val =
	    of_property_read_u32(np, "aw8622x_vib_cont_brk_time",
				 &aw8622x->dts_info.cont_brk_time_dt);
	if (val != 0)
		aw_dev_info("%s vib_cont_brk_time not found\n", __func__);
	val =
	    of_property_read_u32(np, "aw8622x_vib_cont_track_margin",
				 &aw8622x->dts_info.cont_track_margin);
	if (val != 0)
		aw_dev_info("%s vib_cont_track_margin not found\n", __func__);

	val = of_property_read_u32_array(np, "aw8622x_vib_prctmode",
					 prctmode_temp,
					 ARRAY_SIZE(prctmode_temp));
	if (val != 0)
		aw_dev_info("%s vib_prctmode not found\n", __func__);
	memcpy(aw8622x->dts_info.prctmode, prctmode_temp,
					sizeof(prctmode_temp));
	val = of_property_read_u32_array(np,
				"aw8622x_vib_sine_array",
				sine_array_temp,
				ARRAY_SIZE(sine_array_temp));
	if (val != 0)
		aw_dev_info("%s vib_sine_array not found\n", __func__);
	memcpy(aw8622x->dts_info.sine_array, sine_array_temp,
		sizeof(sine_array_temp));
	val =
	    of_property_read_u32_array(np,
				"aw8622x_vib_trig_config",
				trig_config_temp,
				ARRAY_SIZE(trig_config_temp));
	if (val != 0)
		aw_dev_info("%s vib_trig_config not found\n", __func__);
	memcpy(aw8622x->dts_info.trig_config, trig_config_temp,
	       sizeof(trig_config_temp));
	val = of_property_read_u32_array(np, "aw8622x_vib_duration_time",
		duration_time, ARRAY_SIZE(duration_time));
	if (val != 0)
		aw_dev_info("%s vib_duration_time not found\n", __func__);
	ret = aw8622x_analyse_duration_array_size(aw8622x, np);
	if (!ret)
		memcpy(aw8622x->dts_info.duration_time,
			duration_time, sizeof(duration_time));
	aw8622x->dts_info.is_enabled_auto_brk =
			of_property_read_bool(np,
					"aw8622x_vib_is_enabled_auto_brk");
	aw_dev_info("%s aw8622x->info.is_enabled_auto_brk = %d\n", __func__,
		    aw8622x->dts_info.is_enabled_auto_brk);

	return 0;
}

static void aw8622x_haptic_upload_lra(struct aw8622x *aw8622x,
				      unsigned int flag)
{
	switch (flag) {
	case AW8622X_WRITE_ZERO:
		aw_dev_info("%s write zero to trim_lra!\n",  __func__);
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_TRIMCFG3,
				       AW8622X_BIT_TRIMCFG3_TRIM_LRA_MASK,
				       0x00);
		break;
	case AW8622X_F0_CALI:
		aw_dev_info("%s write f0_cali_data to trim_lra = 0x%02X\n",
			    __func__, aw8622x->f0_cali_data);
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_TRIMCFG3,
				       AW8622X_BIT_TRIMCFG3_TRIM_LRA_MASK,
				       (char)aw8622x->f0_cali_data);
		break;
	case AW8622X_OSC_CALI:
		aw_dev_info("%s write osc_cali_data to trim_lra = 0x%02X\n",
			    __func__, aw8622x->osc_cali_data);
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_TRIMCFG3,
				       AW8622X_BIT_TRIMCFG3_TRIM_LRA_MASK,
				       (char)aw8622x->osc_cali_data);
		break;
	default:
		break;
	}
}

/*****************************************************
 *
 * sram size, normally 3k(2k fifo, 1k ram)
 *
 *****************************************************/
static int aw8622x_sram_size(struct aw8622x *aw8622x, int size_flag)
{
	if (size_flag == AW8622X_HAPTIC_SRAM_2K) {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_RTPCFG1,
				       AW8622X_BIT_RTPCFG1_SRAM_SIZE_2K_MASK,
				       AW8622X_BIT_RTPCFG1_SRAM_SIZE_2K_EN);
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_RTPCFG1,
				       AW8622X_BIT_RTPCFG1_SRAM_SIZE_1K_MASK,
				       AW8622X_BIT_RTPCFG1_SRAM_SIZE_1K_DIS);
	} else if (size_flag == AW8622X_HAPTIC_SRAM_1K) {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_RTPCFG1,
				       AW8622X_BIT_RTPCFG1_SRAM_SIZE_2K_MASK,
				       AW8622X_BIT_RTPCFG1_SRAM_SIZE_2K_DIS);
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_RTPCFG1,
				       AW8622X_BIT_RTPCFG1_SRAM_SIZE_1K_MASK,
				       AW8622X_BIT_RTPCFG1_SRAM_SIZE_1K_EN);
	} else if (size_flag == AW8622X_HAPTIC_SRAM_3K) {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_RTPCFG1,
				       AW8622X_BIT_RTPCFG1_SRAM_SIZE_1K_MASK,
				       AW8622X_BIT_RTPCFG1_SRAM_SIZE_1K_EN);
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_RTPCFG1,
				       AW8622X_BIT_RTPCFG1_SRAM_SIZE_2K_MASK,
				       AW8622X_BIT_RTPCFG1_SRAM_SIZE_2K_EN);
	}
	return 0;
}

int aw8622x_haptic_stop_l(struct aw8622x *aw8622x)
{
	bool force_flag = true;
	unsigned char cnt = 40;
	unsigned char reg_val = 0;

	aw_dev_info("%s enter\n", __func__);
	aw8622x->play_mode = AW8622X_HAPTIC_STANDBY_MODE;
	reg_val = AW8622X_BIT_PLAYCFG4_STOP_ON;
	aw8622x_i2c_write(aw8622x, AW8622X_REG_PLAYCFG4, &reg_val,
			  AW_I2C_BYTE_ONE);
	while (cnt) {
		aw8622x_i2c_read(aw8622x, AW8622X_REG_GLBRD5, &reg_val,
				 AW_I2C_BYTE_ONE);
		if ((reg_val & AW8622X_BIT_GLBRD5_STATE) ==
		     AW8622X_BIT_GLBRD5_STATE_STANDBY) {
			cnt = 0;
			force_flag = false;
			aw_dev_info("%s entered standby! glb_state=0x%02X\n",
				    __func__, reg_val);
		} else {
			cnt--;
			aw_dev_dbg("%s wait for standby, glb_state=0x%02X\n",
				   __func__, reg_val);
		}
		usleep_range(AW8622X_STOP_DELAY_MIN, AW8622X_STOP_DELAY_MAX);
	}

	if (force_flag) {
		aw_dev_err("%s force to enter standby mode!\n", __func__);
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL2,
				       AW8622X_BIT_SYSCTRL2_STANDBY_MASK,
				       AW8622X_BIT_SYSCTRL2_STANDBY_ON);
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL2,
				       AW8622X_BIT_SYSCTRL2_STANDBY_MASK,
				       AW8622X_BIT_SYSCTRL2_STANDBY_OFF);
	}
	return 0;
}

static void aw8622x_haptic_raminit(struct aw8622x *aw8622x, bool flag)
{
	if (flag) {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL1,
				       AW8622X_BIT_SYSCTRL1_RAMINIT_MASK,
				       AW8622X_BIT_SYSCTRL1_RAMINIT_ON);
	} else {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL1,
				       AW8622X_BIT_SYSCTRL1_RAMINIT_MASK,
				       AW8622X_BIT_SYSCTRL1_RAMINIT_OFF);
	}
}

static int aw8622x_haptic_get_vbat(struct aw8622x *aw8622x)
{
	unsigned char reg_val = 0;
	unsigned int vbat_code = 0;
	/*unsigned int cont = 2000;*/

	aw8622x_haptic_stop_l(aw8622x);
	aw8622x_haptic_raminit(aw8622x, true);
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_DETCFG2,
			       AW8622X_BIT_DETCFG2_VBAT_GO_MASK,
			       AW8622X_BIT_DETCFG2_VABT_GO_ON);
	usleep_range(AW8622X_VBAT_DELAY_MIN, AW8622X_VBAT_DELAY_MAX);
	aw8622x_i2c_read(aw8622x, AW8622X_REG_DET_VBAT, &reg_val,
			 AW_I2C_BYTE_ONE);
	vbat_code = (vbat_code | reg_val) << 2;
	aw8622x_i2c_read(aw8622x, AW8622X_REG_DET_LO, &reg_val,
			 AW_I2C_BYTE_ONE);
	vbat_code = vbat_code | ((reg_val & AW8622X_BIT_DET_LO_VBAT) >> 4);
	aw8622x->vbat = AW8622X_VBAT_FORMULA(vbat_code);
	if (aw8622x->vbat > AW8622X_VBAT_MAX) {
		aw8622x->vbat = AW8622X_VBAT_MAX;
		aw_dev_info("%s vbat max limit = %dmV\n", __func__,
			    aw8622x->vbat);
	}
	if (aw8622x->vbat < AW8622X_VBAT_MIN) {
		aw8622x->vbat = AW8622X_VBAT_MIN;
		aw_dev_info("%s vbat min limit = %dmV\n", __func__,
			    aw8622x->vbat);
	}
	aw_dev_info("%s aw8622x->vbat=%dmV, vbat_code=0x%02X\n",
		    __func__, aw8622x->vbat, vbat_code);
	aw8622x_haptic_raminit(aw8622x, false);
	return 0;
}

static void aw8622x_interrupt_clear(struct aw8622x *aw8622x)
{
	unsigned char reg_val = 0;

	aw8622x_i2c_read(aw8622x, AW8622X_REG_SYSINT, &reg_val,
			 AW_I2C_BYTE_ONE);
	aw_dev_dbg("%s: SYSINT=0x%02X\n", __func__, reg_val);
}

static void aw8622x_haptic_set_gain(struct aw8622x *aw8622x, unsigned char gain)
{
	aw8622x_i2c_write(aw8622x, AW8622X_REG_PLAYCFG2, &gain,
			  AW_I2C_BYTE_ONE);
}

static int aw8622x_haptic_ram_vbat_comp(struct aw8622x *aw8622x,
					      bool flag)
{
	int temp_gain = 0;

	if (flag) {
		if (aw8622x->ram_vbat_compensate ==
		    AW8622X_HAPTIC_RAM_VBAT_COMP_ENABLE) {
			aw8622x_haptic_get_vbat(aw8622x);
			temp_gain =
			    aw8622x->gain * AW8622X_VBAT_REFER / aw8622x->vbat;
			if (temp_gain >
			    (128 * AW8622X_VBAT_REFER / AW8622X_VBAT_MIN)) {
				temp_gain =
				    128 * AW8622X_VBAT_REFER / AW8622X_VBAT_MIN;
				aw_dev_dbg("%s gain limit=%d\n", __func__,
					   temp_gain);
			}
			aw8622x_haptic_set_gain(aw8622x, temp_gain);
		} else {
			aw8622x_haptic_set_gain(aw8622x, aw8622x->gain);
		}
	} else {
		aw8622x_haptic_set_gain(aw8622x, aw8622x->gain);
	}
	return 0;
}

static int aw8622x_haptic_play_mode(struct aw8622x *aw8622x,
				    unsigned char play_mode)
{
	aw_dev_info("%s enter\n", __func__);

	switch (play_mode) {
	case AW8622X_HAPTIC_STANDBY_MODE:
		aw_dev_info("%s: enter standby mode\n", __func__);
		aw8622x->play_mode = AW8622X_HAPTIC_STANDBY_MODE;
		aw8622x_haptic_stop_l(aw8622x);
		break;
	case AW8622X_HAPTIC_RAM_MODE:
		aw_dev_info("%s: enter ram mode\n", __func__);
		aw8622x->play_mode = AW8622X_HAPTIC_RAM_MODE;
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_PLAYCFG3,
				       AW8622X_BIT_PLAYCFG3_PLAY_MODE_MASK,
				       AW8622X_BIT_PLAYCFG3_PLAY_MODE_RAM);
		break;
	case AW8622X_HAPTIC_RAM_LOOP_MODE:
		aw_dev_info("%s: enter ram loop mode\n",
			    __func__);
		aw8622x->play_mode = AW8622X_HAPTIC_RAM_LOOP_MODE;
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_PLAYCFG3,
				       AW8622X_BIT_PLAYCFG3_PLAY_MODE_MASK,
				       AW8622X_BIT_PLAYCFG3_PLAY_MODE_RAM);
		break;
	case AW8622X_HAPTIC_RTP_MODE:
		aw_dev_info("%s: enter rtp mode\n", __func__);
		aw8622x->play_mode = AW8622X_HAPTIC_RTP_MODE;
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_PLAYCFG3,
				       AW8622X_BIT_PLAYCFG3_PLAY_MODE_MASK,
				       AW8622X_BIT_PLAYCFG3_PLAY_MODE_RTP);
		break;
	case AW8622X_HAPTIC_TRIG_MODE:
		aw_dev_info("%s: enter trig mode\n", __func__);
		aw8622x->play_mode = AW8622X_HAPTIC_TRIG_MODE;
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_PLAYCFG3,
				       AW8622X_BIT_PLAYCFG3_PLAY_MODE_MASK,
				       AW8622X_BIT_PLAYCFG3_PLAY_MODE_RAM);
		break;
	case AW8622X_HAPTIC_CONT_MODE:
		aw_dev_info("%s: enter cont mode\n", __func__);
		aw8622x->play_mode = AW8622X_HAPTIC_CONT_MODE;
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_PLAYCFG3,
				       AW8622X_BIT_PLAYCFG3_PLAY_MODE_MASK,
				       AW8622X_BIT_PLAYCFG3_PLAY_MODE_CONT);
		break;
	default:
		aw_dev_err("%s: play mode %d error",
			   __func__, play_mode);
		break;
	}
	return 0;
}

static int aw8622x_haptic_play_go(struct aw8622x *aw8622x, bool flag)
{
	unsigned char reg_val = 0;

	aw_dev_dbg("%s enter\n", __func__);
	if (flag == true) {
		reg_val = AW8622X_BIT_PLAYCFG4_GO_ON;
		aw8622x_i2c_write(aw8622x, AW8622X_REG_PLAYCFG4, &reg_val,
				     AW_I2C_BYTE_ONE);
		usleep_range(2000, 2500);
	} else {
		reg_val = AW8622X_BIT_PLAYCFG4_STOP_ON;
		aw8622x_i2c_write(aw8622x, AW8622X_REG_PLAYCFG4, &reg_val,
				  AW_I2C_BYTE_ONE);
	}
	return 0;
}

static void aw8622x_haptic_set_wav_seq(struct aw8622x *aw8622x,
				      unsigned char wav, unsigned char seq)
{
	aw8622x_i2c_write(aw8622x, AW8622X_REG_WAVCFG1 + wav, &seq,
			  AW_I2C_BYTE_ONE);
}

static int aw8622x_haptic_set_wav_loop(struct aw8622x *aw8622x,
				       unsigned char wav, unsigned char loop)
{
	unsigned char tmp = 0;

	if (wav % 2) {
		tmp = loop << 0;
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_WAVCFG9 + (wav / 2),
				       AW8622X_BIT_WAVLOOP_SEQ_EVEN_MASK, tmp);
	} else {
		tmp = loop << 4;
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_WAVCFG9 + (wav / 2),
				       AW8622X_BIT_WAVLOOP_SEQ_ODD_MASK, tmp);
	}
	return 0;
}

/*****************************************************
 *
 * haptic f0 cali
 *
 *****************************************************/
static int aw8622x_haptic_read_lra_f0(struct aw8622x *aw8622x)
{
	unsigned char reg_val[2] = {0};
	unsigned int f0_reg = 0;
	unsigned long f0_tmp = 0;

	aw_dev_info("%s enter\n", __func__);
	/* F_LRA_F0 */
	aw8622x_i2c_read(aw8622x, AW8622X_REG_CONTRD14, reg_val,
			 AW_I2C_BYTE_TWO);
	f0_reg = (f0_reg | reg_val[0]) << 8;
	f0_reg |= (reg_val[1] << 0);


	if (!f0_reg) {
		aw_dev_err("%s didn't get lra f0 because f0_reg value is 0!\n",
			   __func__);
		aw8622x->f0 = aw8622x->dts_info.f0_ref;
		return -ERANGE;
	}
	f0_tmp = AW8622X_LRA_F0_FORMULA(f0_reg);
	aw8622x->f0 = (unsigned int)f0_tmp;
	aw_dev_info("%s lra_f0=%d\n", __func__,  aw8622x->f0);
	return 0;
}

static int aw8622x_haptic_read_cont_f0(struct aw8622x *aw8622x)
{
	unsigned char reg_val[2] = {0};
	unsigned int f0_reg = 0;
	unsigned long f0_tmp = 0;

	aw_dev_info("%s enter\n", __func__);
	aw8622x_i2c_read(aw8622x, AW8622X_REG_CONTRD16, reg_val,
			 AW_I2C_BYTE_TWO);
	f0_reg = (f0_reg | reg_val[0]) << 8;
	f0_reg |= (reg_val[1] << 0);

	if (!f0_reg) {
		aw_dev_err("%s didn't get cont f0 because f0_reg value is 0!\n",
			   __func__);
		aw8622x->cont_f0 = aw8622x->dts_info.f0_ref;
		return -ERANGE;
	}
	f0_tmp = AW8622X_CONT_F0_FORMULA(f0_reg);
	aw8622x->cont_f0 = (unsigned int)f0_tmp;
	aw_dev_info("%s cont_f0=%d\n",
		    __func__, aw8622x->cont_f0);
	return 0;
}

static int aw8622x_haptic_cont_get_f0(struct aw8622x *aw8622x)
{
	bool get_f0_flag = false;
	unsigned char reg_val = 0;
	unsigned char brk_en_temp = 0;
	unsigned char reg_array[3] = {0};
	unsigned int cnt = 200;
	int ret = 0;

	aw_dev_info("%s enter\n", __func__);
	aw8622x->f0 = aw8622x->dts_info.f0_ref;
	/* enter standby mode */
	aw8622x_haptic_stop_l(aw8622x);
	/* f0 calibrate work mode */
	aw8622x_haptic_play_mode(aw8622x, AW8622X_HAPTIC_CONT_MODE);
	/* enable f0 detect */
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_CONTCFG1,
			       AW8622X_BIT_CONTCFG1_EN_F0_DET_MASK,
			       AW8622X_BIT_CONTCFG1_F0_DET_ENABLE);
	/* cont config */
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_CONTCFG6,
			       AW8622X_BIT_CONTCFG6_TRACK_EN_MASK,
			       AW8622X_BIT_CONTCFG6_TRACK_ENABLE);
	/* enable auto brake */
	aw8622x_i2c_read(aw8622x, AW8622X_REG_PLAYCFG3, &reg_val,
			 AW_I2C_BYTE_ONE);
	brk_en_temp = AW8622X_BIT_PLAYCFG3_BRK & reg_val;
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_PLAYCFG3,
			       AW8622X_BIT_PLAYCFG3_BRK_EN_MASK,
			       AW8622X_BIT_PLAYCFG3_BRK_ENABLE);
	/* f0 driver level */
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_CONTCFG6,
			       AW8622X_BIT_CONTCFG6_DRV1_LVL_MASK,
			       aw8622x->dts_info.cont_drv1_lvl_dt);
	reg_array[0] = (unsigned char)aw8622x->dts_info.cont_drv2_lvl_dt;
	reg_array[1] = (unsigned char)aw8622x->dts_info.cont_drv1_time_dt;
	reg_array[2] = (unsigned char)aw8622x->dts_info.cont_drv2_time_dt;
	aw8622x_i2c_write(aw8622x, AW8622X_REG_CONTCFG7, reg_array,
			  AW_I2C_BYTE_THREE);
	/* TRACK_MARGIN */
	if (!aw8622x->dts_info.cont_track_margin) {
		aw_dev_err("%s aw8622x->dts_info.cont_track_margin = 0!\n",
			   __func__);
	} else {
		reg_val = (unsigned char)aw8622x->dts_info.cont_track_margin;
		aw8622x_i2c_write(aw8622x, AW8622X_REG_CONTCFG11, &reg_val,
				  AW_I2C_BYTE_ONE);
	}
	/* cont play go */
	aw8622x_haptic_play_go(aw8622x, true);
	/* 300ms */
	while (cnt) {
		aw8622x_i2c_read(aw8622x, AW8622X_REG_GLBRD5, &reg_val,
				 AW_I2C_BYTE_ONE);
		if ((reg_val & AW8622X_BIT_GLBRD5_STATE) ==
		    AW8622X_BIT_GLBRD5_STATE_STANDBY) {
			cnt = 0;
			get_f0_flag = true;
			aw_dev_info("%s entered standby mode! glb_state=0x%02X\n",
				    __func__, reg_val);
		} else {
			cnt--;
			aw_dev_dbg("%s waitting for standby, glb_state=0x%02X\n",
				    __func__, reg_val);
		}
		usleep_range(AW8622X_F0_DELAY_MIN, AW8622X_F0_DELAY_MAX);
	}
	if (get_f0_flag) {
		aw8622x_haptic_read_lra_f0(aw8622x);
		aw8622x_haptic_read_cont_f0(aw8622x);
	} else {
		aw_dev_err("%s enter standby mode failed, stop reading f0!\n",
			   __func__);
	}
	/* restore default config */
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_CONTCFG1,
			       AW8622X_BIT_CONTCFG1_EN_F0_DET_MASK,
			       AW8622X_BIT_CONTCFG1_F0_DET_DISABLE);
	/* recover auto break config */
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_PLAYCFG3,
			       AW8622X_BIT_PLAYCFG3_BRK_EN_MASK,
			       brk_en_temp);
	return ret;
}

static int aw8622x_haptic_rtp_init(struct aw8622x *aw8622x)
{
	unsigned int buf_len = 0;
	unsigned char glb_state_val = 0;
	struct aw8622x_container *rtp_container = aw8622x->rtp_container;

	aw_dev_dbg("%s enter\n", __func__);
	pm_qos_add_request(&aw8622x_pm_qos_req_vb, PM_QOS_CPU_DMA_LATENCY,
			   AW8622X_PM_QOS_VALUE_VB);
	aw8622x->rtp_cnt = 0;
	mutex_lock(&aw8622x->rtp_lock);
	while ((!aw8622x_haptic_rtp_get_fifo_afs(aw8622x))
	       && (aw8622x->play_mode == AW8622X_HAPTIC_RTP_MODE)) {
#ifdef AW_ENABLE_RTP_PRINT_LOG
		aw_dev_info("%s rtp cnt = %d\n", __func__, aw8622x->rtp_cnt);
#endif
		if (!rtp_container) {
			aw_dev_info("%s:rtp_container is null, break!\n",
				    __func__);
			break;
		}
		if (aw8622x->rtp_cnt < (aw8622x->ram.base_addr)) {
			if ((rtp_container->len - aw8622x->rtp_cnt) <
			    (aw8622x->ram.base_addr)) {
				buf_len = rtp_container->len - aw8622x->rtp_cnt;
			} else {
				buf_len = aw8622x->ram.base_addr;
			}
		} else if ((rtp_container->len - aw8622x->rtp_cnt) <
			   (aw8622x->ram.base_addr >> 2)) {
			buf_len = rtp_container->len - aw8622x->rtp_cnt;
		} else {
			buf_len = aw8622x->ram.base_addr >> 2;
		}
#ifdef AW_ENABLE_RTP_PRINT_LOG
		aw_dev_info("%s buf_len = %d\n", __func__,
			    buf_len);
#endif
		aw8622x_i2c_write(aw8622x, AW8622X_REG_RTPDATA,
				   &rtp_container->data[aw8622x->rtp_cnt],
				   buf_len);
		aw8622x->rtp_cnt += buf_len;
		aw8622x_i2c_read(aw8622x, AW8622X_REG_GLBRD5, &glb_state_val,
				 AW_I2C_BYTE_ONE);
		if ((aw8622x->rtp_cnt == rtp_container->len)
		    || ((glb_state_val & AW8622X_BIT_GLBRD5_STATE) ==
		    	 AW8622X_BIT_GLBRD5_STATE_STANDBY)) {
			if (aw8622x->rtp_cnt == rtp_container->len)
				aw_dev_info("%s: rtp load completely! glb_state_val=%02x aw8622x->rtp_cnt=%02x\n",
					    __func__, glb_state_val,
					    aw8622x->rtp_cnt);
			else
				aw_dev_err("%s rtp load failed!! glb_state_val=%02x aw8622x->rtp_cnt=%02x\n",
					   __func__, glb_state_val,
					   aw8622x->rtp_cnt);
			aw8622x->rtp_cnt = 0;
			pm_qos_remove_request(&aw8622x_pm_qos_req_vb);
			mutex_unlock(&aw8622x->rtp_lock);
			return 0;
		}
	}

	if (aw8622x->play_mode == AW8622X_HAPTIC_RTP_MODE)
		aw8622x_haptic_set_rtp_aei(aw8622x, true);

	aw_dev_dbg("%s exit\n", __func__);
	mutex_unlock(&aw8622x->rtp_lock);
	pm_qos_remove_request(&aw8622x_pm_qos_req_vb);
	return 0;
}

static int aw8622x_haptic_ram_config(struct aw8622x *aw8622x, int duration)
{
	int ret = 0;

	if (aw8622x->duration_time_flag < 0) {
		aw_dev_err("%s: duration time error, array size = %d\n",
			   __func__, aw8622x->duration_time_size);
		return -ERANGE;
	}
	ret = aw8622x_analyse_duration_range(aw8622x);
	if (ret < 0)
		return ret;

	if ((duration > 0) && (duration <
				aw8622x->dts_info.duration_time[0])) {
		aw8622x->index = 3;	/*3*/
		aw8622x->activate_mode = AW8622X_HAPTIC_RAM_MODE;
	} else if ((duration >= aw8622x->dts_info.duration_time[0]) &&
		(duration < aw8622x->dts_info.duration_time[1])) {
		aw8622x->index = 2;	/*2*/
		aw8622x->activate_mode = AW8622X_HAPTIC_RAM_MODE;
	} else if ((duration >= aw8622x->dts_info.duration_time[1]) &&
		(duration < aw8622x->dts_info.duration_time[2])) {
		aw8622x->index = 1;	/*1*/
		aw8622x->activate_mode = AW8622X_HAPTIC_RAM_MODE;
	} else if (duration >= aw8622x->dts_info.duration_time[2]) {
		aw8622x->index = 4;	/*4*/
		aw8622x->activate_mode = AW8622X_HAPTIC_RAM_LOOP_MODE;
	} else {
		aw_dev_err("%s: duration time error, duration= %d\n",
			   __func__, duration);
		aw8622x->index = 0;
		aw8622x->activate_mode = AW8622X_HAPTIC_NULL;
		ret = -ERANGE;
	}

	return ret;
}

static unsigned char aw8622x_haptic_osc_read_status(struct aw8622x *aw8622x)
{
	unsigned char reg_val = 0;

	aw8622x_i2c_read(aw8622x, AW8622X_REG_SYSST2, &reg_val,
			 AW_I2C_BYTE_ONE);
	return reg_val;
}

static int aw8622x_haptic_set_repeat_wav_seq(struct aw8622x *aw8622x,
					     unsigned char seq)
{
	aw8622x_haptic_set_wav_seq(aw8622x, 0x00, seq);
	aw8622x_haptic_set_wav_seq(aw8622x, 0x01, 0x00);
	aw8622x_haptic_set_wav_loop(aw8622x, 0x00,
				    AW8622X_BIT_WAVLOOP_INIFINITELY);
	return 0;
}

static void aw8622x_rtp_work_routine(struct work_struct *work)
{
	bool rtp_work_flag = false;
	unsigned char reg_val = 0;
	unsigned int cnt = 200;
	int ret = -1;
	const struct firmware *rtp_file;
	struct aw8622x *aw8622x = container_of(work, struct aw8622x, rtp_work);

	aw_dev_info("%s enter\n", __func__);
	mutex_lock(&aw8622x->rtp_lock);
	/* fw loaded */
	ret = request_firmware(&rtp_file,
			       aw8622x_rtp_name[aw8622x->rtp_file_num],
			       aw8622x->dev);
	if (ret < 0) {
		aw_dev_err("%s: failed to read %s\n", __func__,
			   aw8622x_rtp_name[aw8622x->rtp_file_num]);
		mutex_unlock(&aw8622x->rtp_lock);
		return;
	}
	aw8622x->rtp_init = 0;
	vfree(aw8622x->rtp_container);
	aw8622x->rtp_container = vmalloc(rtp_file->size + sizeof(int));
	if (!aw8622x->rtp_container) {
		release_firmware(rtp_file);
		aw_dev_err("%s: error allocating memory\n", __func__);
		mutex_unlock(&aw8622x->rtp_lock);
		return;
	}
	aw8622x->rtp_container->len = rtp_file->size;
	aw_dev_info("%s: rtp file:[%s] size = %dbytes\n",
		    __func__, aw8622x_rtp_name[aw8622x->rtp_file_num],
		    aw8622x->rtp_container->len);
	memcpy(aw8622x->rtp_container->data, rtp_file->data, rtp_file->size);
	mutex_unlock(&aw8622x->rtp_lock);
	release_firmware(rtp_file);
	mutex_lock(&aw8622x->lock);
	aw8622x->rtp_init = 1;
	aw8622x_haptic_upload_lra(aw8622x, AW8622X_OSC_CALI);
	/* gain */
	aw8622x_haptic_ram_vbat_comp(aw8622x, false);
	/* rtp mode config */
	aw8622x_haptic_play_mode(aw8622x, AW8622X_HAPTIC_RTP_MODE);
	/* haptic go */
	aw8622x_haptic_play_go(aw8622x, true);
	mutex_unlock(&aw8622x->lock);
	usleep_range(AW8622X_STOP_DELAY_MIN, AW8622X_STOP_DELAY_MAX);
	while (cnt) {
		aw8622x_i2c_read(aw8622x, AW8622X_REG_GLBRD5, &reg_val,
				 AW_I2C_BYTE_ONE);
		if ((reg_val & AW8622X_BIT_GLBRD5_STATE) ==
		     AW8622X_BIT_GLBRD5_STATE_RTP_GO) {
			cnt = 0;
			rtp_work_flag = true;
			aw_dev_info("%s RTP_GO! glb_state=0x08\n", __func__);
		} else {
			cnt--;
			aw_dev_dbg("%s wait for RTP_GO, glb_state=0x%02X\n",
				   __func__, reg_val);
		}
		usleep_range(AW8622X_STOP_DELAY_MIN, AW8622X_STOP_DELAY_MAX);
	}
	if (rtp_work_flag) {
		aw8622x_haptic_rtp_init(aw8622x);
	} else {
		/* enter standby mode */
		aw8622x_haptic_stop_l(aw8622x);
		aw_dev_err("%s failed to enter RTP_GO status!\n", __func__);
	}
}

static int aw8622x_osc_calculation_time(struct aw8622x *aw8622x)
{
	unsigned char osc_int_state = 0;
	unsigned int base_addr = aw8622x->ram.base_addr;
	unsigned int buf_len = 0;
	int ret = -1;
	struct aw8622x_container *rtp_container = aw8622x->rtp_container;

	if (!aw8622x_haptic_rtp_get_fifo_afs(aw8622x)) {
		aw_dev_dbg("%s: haptic_rtp_get_fifo_afi, rtp_cnt= %d\n",
			    __func__, aw8622x->rtp_cnt);

		mutex_lock(&aw8622x->rtp_lock);
		if ((rtp_container->len - aw8622x->rtp_cnt) < (base_addr >> 2))
			buf_len = rtp_container->len - aw8622x->rtp_cnt;
		else
			buf_len = (base_addr >> 2);

		if (aw8622x->rtp_cnt != rtp_container->len) {
			if (aw8622x->timeval_flags == 1) {
				aw8622x->kstart = ktime_get();
				aw8622x->timeval_flags = 0;
			}
			aw8622x_i2c_write(aw8622x, AW8622X_REG_RTPDATA,
					&rtp_container->data[aw8622x->rtp_cnt],
					buf_len);
			aw8622x->rtp_cnt += buf_len;
		}
		mutex_unlock(&aw8622x->rtp_lock);
	}

	osc_int_state = aw8622x_haptic_osc_read_status(aw8622x);
	if (osc_int_state & AW8622X_BIT_SYSST2_FF_EMPTY) {
		aw8622x->kend = ktime_get();
		aw_dev_info("%s osc trim playback done aw8622x->rtp_cnt= %d\n",
			    __func__, aw8622x->rtp_cnt);
		return ret;
	}
	aw8622x->kend = ktime_get();
	aw8622x->microsecond = ktime_to_us(ktime_sub(aw8622x->kend,
						aw8622x->kstart));

	if (aw8622x->microsecond > AW8622X_OSC_CALI_MAX_LENGTH) {
		aw_dev_info("%s osc trim time out! aw8622x->rtp_cnt %d\n",
			__func__, aw8622x->rtp_cnt);
		aw_dev_info("%s osc_int_state %02x\n", __func__, osc_int_state);
		return ret;
	}
	return 0;
}

static int aw8622x_rtp_osc_calibration(struct aw8622x *aw8622x)
{
	int ret = -1;
	const struct firmware *rtp_file;

	aw8622x->rtp_cnt = 0;
	aw8622x->timeval_flags = 1;

	aw_dev_info("%s enter\n", __func__);
	/* fw loaded */
	ret = request_firmware(&rtp_file, aw8622x_rtp_name[0], aw8622x->dev);
	if (ret < 0) {
		aw_dev_err("%s: failed to read %s\n", __func__,
			   aw8622x_rtp_name[0]);
		return ret;
	}
	/*awinic add stop,for irq interrupt during calibrate */
	aw8622x_haptic_stop_l(aw8622x);
	aw8622x->rtp_init = 0;
	mutex_lock(&aw8622x->rtp_lock);
	vfree(aw8622x->rtp_container);
	aw8622x->rtp_container = vmalloc(rtp_file->size + sizeof(int));
	if (!aw8622x->rtp_container) {
		release_firmware(rtp_file);
		mutex_unlock(&aw8622x->rtp_lock);
		aw_dev_err("%s: error allocating memory\n",
			   __func__);
		return -ENOMEM;
	}
	aw8622x->rtp_container->len = rtp_file->size;
	aw8622x->rtp_len = rtp_file->size;
	aw_dev_info("%s: rtp file:[%s] size = %dbytes\n",
		    __func__, aw8622x_rtp_name[0], aw8622x->rtp_container->len);

	memcpy(aw8622x->rtp_container->data, rtp_file->data, rtp_file->size);
	release_firmware(rtp_file);
	mutex_unlock(&aw8622x->rtp_lock);
	/* gain */
	aw8622x_haptic_ram_vbat_comp(aw8622x, false);
	/* rtp mode config */
	aw8622x_haptic_play_mode(aw8622x, AW8622X_HAPTIC_RTP_MODE);

	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL7,
			       AW8622X_BIT_SYSCTRL7_INT_MODE_MASK,
			       AW8622X_BIT_SYSCTRL7_INT_MODE_EDGE);
	disable_irq(gpio_to_irq(aw8622x->irq_gpio));
	/* haptic go */
	aw8622x_haptic_play_go(aw8622x, true);
	/* require latency of CPU & DMA not more then PM_QOS_VALUE_VB us */
	pm_qos_add_request(&aw8622x_pm_qos_req_vb, PM_QOS_CPU_DMA_LATENCY,
			   AW8622X_PM_QOS_VALUE_VB);
	while (1) {
		ret = aw8622x_osc_calculation_time(aw8622x);
		if (ret < 0)
			break;
	}
	pm_qos_remove_request(&aw8622x_pm_qos_req_vb);
	enable_irq(gpio_to_irq(aw8622x->irq_gpio));
	aw8622x->microsecond = ktime_to_us(ktime_sub(aw8622x->kend,
						aw8622x->kstart));
	/*calibration osc */
	aw_dev_info("%s awinic_microsecond: %ld\n", __func__,
		    aw8622x->microsecond);
	aw_dev_info("%s exit\n", __func__);
	return 0;
}

static int aw8622x_osc_trim_calculation(struct aw8622x *aw8622x,
					unsigned long int theory_time)
{
	unsigned int real_code = 0;
	unsigned int lra_code = 0;
	unsigned int DFT_LRA_TRIM_CODE = 0;
	/*0.1 percent below no need to calibrate */
	unsigned int osc_cali_threshold = 10;
	unsigned long int real_time = aw8622x->microsecond;


	aw_dev_info("%s enter\n", __func__);
	if (theory_time == real_time) {
		aw_dev_info("%s theory_time == real_time: %ld, no need to calibrate!\n",
			__func__, real_time);
		return 0;
	} else if (theory_time < real_time) {
		if ((real_time - theory_time) > (theory_time / 50)) {
			aw_dev_info("%s (real_time - theory_time) > (theory_time/50), can't calibrate!\n",
				    __func__);
			return DFT_LRA_TRIM_CODE;
		}

		if ((real_time - theory_time) <
		    (osc_cali_threshold * theory_time / 10000)) {
			aw_dev_info("%s real_time: %ld, theory_time: %ld, no need to calibrate!\n",
				    __func__, real_time, theory_time);
			return DFT_LRA_TRIM_CODE;
		}

		real_code = ((real_time - theory_time) * 4000) / theory_time;
		real_code = ((real_code % 10 < 5) ? 0 : 1) + real_code / 10;
		real_code = 32 + real_code;
	} else if (theory_time > real_time) {
		if ((theory_time - real_time) > (theory_time / 50)) {
			aw_dev_info("%s (theory_time - real_time) > (theory_time / 50), can't calibrate!\n",
				    __func__);
			return DFT_LRA_TRIM_CODE;
		}
		if ((theory_time - real_time) <
		    (osc_cali_threshold * theory_time / 10000)) {
			aw_dev_info("%s real_time: %ld, theory_time: %ld, no need to calibrate!\n",
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
	aw_dev_info("%s real_time: %ld, theory_time: %ld\n", __func__,
		    real_time, theory_time);
	aw_dev_info("%s real_code: %02X, trim_lra: 0x%02X\n", __func__,
		    real_code, lra_code);
	return lra_code;
}

static int aw8622x_haptic_get_lra_resistance(struct aw8622x *aw8622x)
{
	unsigned char reg_val = 0;
	unsigned char d2s_gain_temp = 0;
	unsigned int lra_code = 0;
	unsigned int lra = 0;

	mutex_lock(&aw8622x->lock);
	aw8622x_haptic_stop_l(aw8622x);
	aw8622x_i2c_read(aw8622x, AW8622X_REG_SYSCTRL7, &reg_val,
			 AW_I2C_BYTE_ONE);
	d2s_gain_temp = AW8622X_BIT_SYSCTRL7_GAIN & reg_val;
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL7,
			       AW8622X_BIT_SYSCTRL7_D2S_GAIN_MASK,
			       aw8622x->dts_info.d2s_gain);
	aw8622x_haptic_raminit(aw8622x, true);
	/* enter standby mode */
	aw8622x_haptic_stop_l(aw8622x);
	usleep_range(AW8622X_STOP_DELAY_MIN, AW8622X_STOP_DELAY_MAX);
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL2,
			       AW8622X_BIT_SYSCTRL2_STANDBY_MASK,
			       AW8622X_BIT_SYSCTRL2_STANDBY_OFF);
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_DETCFG1,
			       AW8622X_BIT_DETCFG1_RL_OS_MASK,
			       AW8622X_BIT_DETCFG1_RL);
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_DETCFG2,
			       AW8622X_BIT_DETCFG2_DIAG_GO_MASK,
			       AW8622X_BIT_DETCFG2_DIAG_GO_ON);
	usleep_range(AW8622X_LRA_DELAY_MIN, AW8622X_LRA_DELAY_MAX);
	aw8622x_i2c_read(aw8622x, AW8622X_REG_DET_RL, &reg_val,
			 AW_I2C_BYTE_ONE);
	lra_code = (lra_code | reg_val) << 2;
	aw8622x_i2c_read(aw8622x, AW8622X_REG_DET_LO, &reg_val,
			 AW_I2C_BYTE_ONE);
	lra_code = lra_code | (reg_val & AW8622X_BIT_DET_LO_RL);
	/* 2num */
	lra = AW8622X_LRA_FORMULA(lra_code);
	/* Keep up with aw8624 driver */
	aw8622x->lra = lra * 10;
	aw8622x_haptic_raminit(aw8622x, false);
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL7,
			       AW8622X_BIT_SYSCTRL7_D2S_GAIN_MASK,
			       d2s_gain_temp);
	mutex_unlock(&aw8622x->lock);
	return 0;
}

static int aw8622x_haptic_juge_RTP_is_going_on(struct aw8622x *aw8622x)
{
	unsigned char rtp_state = 0;
	unsigned char mode = 0;
	unsigned char glb_st = 0;

	aw8622x_i2c_read(aw8622x, AW8622X_REG_PLAYCFG3, &mode,
			 AW_I2C_BYTE_ONE);
	aw8622x_i2c_read(aw8622x, AW8622X_REG_GLBRD5, &glb_st,
			 AW_I2C_BYTE_ONE);
	if ((mode & AW8622X_BIT_PLAYCFG3_PLAY_MODE_RTP) &&
		(glb_st == AW8622X_BIT_GLBRD5_STATE_RTP_GO)) {
		rtp_state = 1;
	}
	return rtp_state;
}

static void aw8622x_set_ram_addr(struct aw8622x *aw8622x)
{
	unsigned char ram_addr[2] = {0};
	unsigned int base_addr = aw8622x->ram.base_addr;

	ram_addr[0] = (unsigned char)AW_SET_RAMADDR_H(base_addr);
	ram_addr[1] = (unsigned char)AW_SET_RAMADDR_L(base_addr);
	aw8622x_i2c_write(aw8622x, AW8622X_REG_RAMADDRH, ram_addr,
			 AW_I2C_BYTE_TWO);
}

#ifdef AW_CHECK_RAM_DATA
static int aw8622x_check_ram_data(struct aw8622x *aw8622x,
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

static int aw8622x_container_update(struct aw8622x *aw8622x,
				    struct aw8622x_container *aw8622x_cont)
{
	unsigned char ae_addr_h = 0;
	unsigned char af_addr_h = 0;
	unsigned char ae_addr_l = 0;
	unsigned char af_addr_l = 0;
	unsigned char reg_array[3] = {0};
	unsigned int shift = 0;
	unsigned int base_addr = 0;
	int i = 0;
	int ret = 0;
	int len = 0;
#ifdef AW_CHECK_RAM_DATA
	unsigned char ram_data[AW_RAMDATA_RD_BUFFER_SIZE] = {0};
#endif

	aw_dev_info("%s enter\n", __func__);
	mutex_lock(&aw8622x->lock);
	aw8622x->ram.baseaddr_shift = 2;
	aw8622x->ram.ram_shift = 4;
	/* RAMINIT Enable */
	aw8622x_haptic_raminit(aw8622x, true);
	/* Enter standby mode */
	aw8622x_haptic_stop_l(aw8622x);
	/* default 3k SRAM */
	aw8622x_sram_size(aw8622x, AW8622X_HAPTIC_SRAM_3K);
	/* set base addr */
	shift = aw8622x->ram.baseaddr_shift;
	aw8622x->ram.base_addr =
	    (unsigned int)((aw8622x_cont->data[0 + shift] << 8) |
			   (aw8622x_cont->data[1 + shift]));
	base_addr  = aw8622x->ram.base_addr;
	aw_dev_info("%s: base_addr=%d\n", __func__, base_addr);
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_RTPCFG1,
			       AW8622X_BIT_RTPCFG1_ADDRH_MASK,
			       (unsigned char)AW_SET_BASEADDR_H(base_addr));
	reg_array[0] = (unsigned char)AW_SET_BASEADDR_L(base_addr);
	aw8622x_i2c_write(aw8622x, AW8622X_REG_RTPCFG2,
			  &reg_array[0], AW_I2C_BYTE_ONE);
	/* set FIFO_AE and FIFO_AF addr */
	ae_addr_h = (unsigned char)AW8622X_SET_AEADDR_H(base_addr);
	af_addr_h = (unsigned char)AW8622X_SET_AFADDR_H(base_addr);
	reg_array[0] = ae_addr_h | af_addr_h;
	reg_array[1] = (unsigned char)AW8622X_SET_AEADDR_L(base_addr);
	reg_array[2] = (unsigned char)AW8622X_SET_AFADDR_L(base_addr);
	aw8622x_i2c_write(aw8622x, AW8622X_REG_RTPCFG3, reg_array,
			  AW_I2C_BYTE_THREE);
	/* get FIFO_AE and FIFO_AF addr */
	aw8622x_i2c_read(aw8622x, AW8622X_REG_RTPCFG3, reg_array,
			 AW_I2C_BYTE_THREE);
	ae_addr_h = ((reg_array[0]) & AW8622X_BIT_RTPCFG3_FIFO_AEH) >> 4;
	ae_addr_l = reg_array[1];
	aw_dev_info("%s: almost_empty_threshold = %d\n", __func__,
		    (unsigned short)((ae_addr_h << 8) | ae_addr_l));
	af_addr_h = ((reg_array[0]) & AW8622X_BIT_RTPCFG3_FIFO_AFH);
	af_addr_l = reg_array[2];
	aw_dev_info("%s: almost_full_threshold = %d\n", __func__,
		    (unsigned short)((af_addr_h << 8) | af_addr_l));

	/* ram */
	aw8622x_set_ram_addr(aw8622x);
	i = aw8622x->ram.ram_shift;
	while (i < aw8622x_cont->len) {
		if ((aw8622x_cont->len - i) < AW_RAMDATA_WR_BUFFER_SIZE)
			len = aw8622x_cont->len - i;
		else
			len = AW_RAMDATA_WR_BUFFER_SIZE;

		aw8622x_i2c_write(aw8622x, AW8622X_REG_RAMDATA,
				 &aw8622x_cont->data[i], len);
		i += len;
	}
#ifdef	AW_CHECK_RAM_DATA
	aw8622x_set_ram_addr(aw8622x);
	i = aw8622x->ram.ram_shift;
	while (i < aw8622x_cont->len) {
		if ((aw8622x_cont->len - i) < AW_RAMDATA_RD_BUFFER_SIZE)
			len = aw8622x_cont->len - i;
		else
			len = AW_RAMDATA_RD_BUFFER_SIZE;

		aw8622x_i2c_read(aw8622x, AW8622X_REG_RAMDATA, ram_data, len);
		ret = aw8622x_check_ram_data(aw8622x, &aw8622x_cont->data[i],
					     ram_data, len);
		if (ret < 0)
			break;
		i += len;
	}
	if (ret)
		aw_dev_err("%s: ram data check sum error\n", __func__);
	else
		aw_dev_info("%s: ram data check sum pass\n", __func__);


#endif
	/* RAMINIT Disable */
	aw8622x_haptic_raminit(aw8622x, false);
	mutex_unlock(&aw8622x->lock);
	aw_dev_info("%s exit\n", __func__);
	return ret;
}

static int aw8622x_haptic_get_ram_number(struct aw8622x *aw8622x)
{
	unsigned char ram_data[3];
	unsigned int first_wave_addr = 0;

	aw_dev_info("%s enter!\n", __func__);
	if (!aw8622x->ram_init) {
		aw_dev_err("%s: ram init faild, ram_num = 0!\n",
			   __func__);
		return -EPERM;
	}

	mutex_lock(&aw8622x->lock);
	/* RAMINIT Enable */
	aw8622x_haptic_raminit(aw8622x, true);
	aw8622x_haptic_stop_l(aw8622x);
	aw8622x_set_ram_addr(aw8622x);
	aw8622x_i2c_read(aw8622x, AW8622X_REG_RAMDATA, ram_data,
			 AW_I2C_BYTE_THREE);
	first_wave_addr = (ram_data[1] << 8 | ram_data[2]);
	aw8622x->ram.ram_num =
			(first_wave_addr - aw8622x->ram.base_addr - 1) / 4;
	aw_dev_info("%s: ram_version = 0x%02x\n", __func__, ram_data[0]);
	aw_dev_info("%s: first waveform addr = 0x%04x\n", __func__,
		    first_wave_addr);
	aw_dev_info("%s: ram_num = %d\n", __func__, aw8622x->ram.ram_num);
	/* RAMINIT Disable */
	aw8622x_haptic_raminit(aw8622x, false);
	mutex_unlock(&aw8622x->lock);

	return 0;
}

static void aw8622x_ram_check(const struct firmware *cont, void *context)
{
	unsigned short check_sum = 0;
	int i = 0;
	int ret = 0;
	struct aw8622x *aw8622x = context;
	struct aw8622x_container *aw8622x_fw;
#ifdef AW_READ_BIN_FLEXBALLY
	static unsigned char load_cont;
	int ram_timer_val = 1000;

	load_cont++;
#endif
	aw_dev_info("%s enter\n", __func__);
	if (!cont) {
		aw_dev_err("%s: failed to read %s\n", __func__,
			   aw8622x_ram_name);
		release_firmware(cont);
#ifdef AW_READ_BIN_FLEXBALLY
		if (load_cont <= 20) {
			schedule_delayed_work(&aw8622x->ram_work,
					msecs_to_jiffies(ram_timer_val));
			aw_dev_info("%s:start hrtimer: load_cont=%d\n",
					__func__, load_cont);
		}
#endif
		return;
	}
	aw_dev_info("%s: loaded %s - size: %zu bytes\n", __func__,
		    aw8622x_ram_name, cont ? cont->size : 0);
/*
*	for(i=0; i < cont->size; i++) {
*		aw_dev_info("%s: addr: 0x%04x, data: 0x%02X\n",
*			__func__, i, *(cont->data+i));
*	}
*/
	/* check sum */
	for (i = 2; i < cont->size; i++)
		check_sum += cont->data[i];
	if (check_sum == (unsigned short)((cont->data[0]<<8)|(cont->data[1]))) {

		aw_dev_info("%s: check sum pass: 0x%04x\n",
			    __func__, check_sum);
		aw8622x->ram.check_sum = check_sum;
	} else {
		aw_dev_err("%s: check sum err: check_sum=0x%04x\n", __func__,
			   check_sum);
		return;
	}

	/* aw8622x ram update less then 128kB */
	aw8622x_fw = kzalloc(cont->size + sizeof(int), GFP_KERNEL);
	if (!aw8622x_fw) {
		release_firmware(cont);
		aw_dev_err("%s: Error allocating memory\n",
			   __func__);
		return;
	}
	aw8622x_fw->len = cont->size;
	memcpy(aw8622x_fw->data, cont->data, cont->size);
	release_firmware(cont);
	ret = aw8622x_container_update(aw8622x, aw8622x_fw);
	if (ret) {
		kfree(aw8622x_fw);
		aw8622x->ram.len = 0;
		aw_dev_err("%s: ram firmware update failed!\n", __func__);
	} else {
		aw8622x->ram_init = 1;
		aw8622x->ram.len = aw8622x_fw->len;
		kfree(aw8622x_fw);
		aw_dev_info("%s: ram firmware update complete!\n",  __func__);
	}
	aw8622x_haptic_get_ram_number(aw8622x);

}

static int aw8622x_ram_update(struct aw8622x *aw8622x)
{
	aw8622x->ram_init = 0;
	aw8622x->rtp_init = 0;

	return request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
				       aw8622x_ram_name, aw8622x->dev,
				       GFP_KERNEL, aw8622x, aw8622x_ram_check);
}

static int aw8622x_rtp_trim_lra_calibration(struct aw8622x *aw8622x)
{
	unsigned char reg_val = 0;
	unsigned int fre_val = 0;
	unsigned int theory_time = 0;
	unsigned int lra_trim_code = 0;
	unsigned int rate = 0;

	aw8622x_i2c_read(aw8622x, AW8622X_REG_SYSCTRL2, &reg_val,
			 AW_I2C_BYTE_ONE);
	fre_val = (reg_val & AW8622X_BIT_SYSCTRL2_RATE) >> 0;

	if (fre_val == AW8622X_BIT_SYSCTRL2_RATE_48K)
		rate = 48000;
	else if (fre_val == AW8622X_BIT_SYSCTRL2_RATE_24K)
		rate = 24000;
	else
		rate = 12000;

	theory_time = (aw8622x->rtp_len / rate) * 1000000;

	aw_dev_info("%s microsecond:%ld  theory_time = %d\n", __func__,
		    aw8622x->microsecond, theory_time);

	lra_trim_code = aw8622x_osc_trim_calculation(aw8622x, theory_time);
	if (lra_trim_code >= 0) {
		aw8622x->osc_cali_data = lra_trim_code;
		aw8622x_haptic_upload_lra(aw8622x, AW8622X_OSC_CALI);
	}
	return 0;
}

static enum hrtimer_restart aw8622x_vibrator_timer_func(struct hrtimer *timer)
{
	struct aw8622x *aw8622x = container_of(timer, struct aw8622x, timer);

	aw_dev_info("%s enter\n", __func__);
	aw8622x->state = 0;
	schedule_work(&aw8622x->vibrator_work);

	return HRTIMER_NORESTART;
}

static int aw8622x_haptic_trig_param_init(struct aw8622x *aw8622x)
{
	aw_dev_info("%s enter\n", __func__);

	if ((aw8622x->name == AW86224_5) && (aw8622x->isUsedIntn))
		return 0;
	/* trig1 date */
	aw8622x->trig[0].trig_level = aw8622x->dts_info.trig_config[0];
	aw8622x->trig[0].trig_polar = aw8622x->dts_info.trig_config[1];
	aw8622x->trig[0].pos_enable = aw8622x->dts_info.trig_config[2];
	aw8622x->trig[0].pos_sequence = aw8622x->dts_info.trig_config[3];
	aw8622x->trig[0].neg_enable = aw8622x->dts_info.trig_config[4];
	aw8622x->trig[0].neg_sequence = aw8622x->dts_info.trig_config[5];
	aw8622x->trig[0].trig_brk = aw8622x->dts_info.trig_config[6];
	aw_dev_info("%s: trig1 date init ok!\n", __func__);
	if ((aw8622x->name == AW86224_5) && (!aw8622x->isUsedIntn)) {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL2,
				AW8622X_BIT_SYSCTRL2_INTN_PIN_MASK,
				AW8622X_BIT_SYSCTRL2_TRIG1);
		return 0;
	}

	/* trig2 date */
	aw8622x->trig[1].trig_level = aw8622x->dts_info.trig_config[7 + 0];
	aw8622x->trig[1].trig_polar = aw8622x->dts_info.trig_config[7 + 1];
	aw8622x->trig[1].pos_enable = aw8622x->dts_info.trig_config[7 + 2];
	aw8622x->trig[1].pos_sequence = aw8622x->dts_info.trig_config[7 + 3];
	aw8622x->trig[1].neg_enable = aw8622x->dts_info.trig_config[7 + 4];
	aw8622x->trig[1].neg_sequence = aw8622x->dts_info.trig_config[7 + 5];
	aw8622x->trig[1].trig_brk = aw8622x->dts_info.trig_config[7 + 6];
	aw_dev_info("%s: trig2 date init ok!\n", __func__);

	/* trig3 date */
	aw8622x->trig[2].trig_level = aw8622x->dts_info.trig_config[14 + 0];
	aw8622x->trig[2].trig_polar = aw8622x->dts_info.trig_config[14 + 1];
	aw8622x->trig[2].pos_enable = aw8622x->dts_info.trig_config[14 + 2];
	aw8622x->trig[2].pos_sequence = aw8622x->dts_info.trig_config[14 + 3];
	aw8622x->trig[2].neg_enable = aw8622x->dts_info.trig_config[14 + 4];
	aw8622x->trig[2].neg_sequence = aw8622x->dts_info.trig_config[14 + 5];
	aw8622x->trig[2].trig_brk = aw8622x->dts_info.trig_config[14 + 6];
	aw_dev_info("%s: trig3 date init ok!\n", __func__);

	return 0;
}

static int aw8622x_haptic_trig_param_config(struct aw8622x *aw8622x)
{
	unsigned char trig1_polar_lev_brk = 0x00;
	unsigned char trig2_polar_lev_brk = 0x00;
	unsigned char trig3_polar_lev_brk = 0x00;
	unsigned char trig1_pos_seq = 0x00;
	unsigned char trig2_pos_seq = 0x00;
	unsigned char trig3_pos_seq = 0x00;
	unsigned char trig1_neg_seq = 0x00;
	unsigned char trig2_neg_seq = 0x00;
	unsigned char trig3_neg_seq = 0x00;

	aw_dev_info("%s enter\n", __func__);

	if ((aw8622x->name == AW86224_5) && (aw8622x->isUsedIntn))
		return 0;
	/* trig1 config */
	trig1_polar_lev_brk = aw8622x->trig[0].trig_polar << 2 |
				aw8622x->trig[0].trig_level << 1 |
				aw8622x->trig[0].trig_brk;
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_TRGCFG7,
				AW8622X_BIT_TRGCFG7_TRG1_POR_LEV_BRK_MASK,
				trig1_polar_lev_brk << 5);

	trig1_pos_seq = aw8622x->trig[0].pos_enable << 7 |
			aw8622x->trig[0].pos_sequence;
	aw8622x_i2c_write(aw8622x, AW8622X_REG_TRGCFG1, &trig1_pos_seq,
			  AW_I2C_BYTE_ONE);

	trig1_neg_seq = aw8622x->trig[0].neg_enable << 7 |
			aw8622x->trig[0].neg_sequence;
	aw8622x_i2c_write(aw8622x, AW8622X_REG_TRGCFG4, &trig1_neg_seq,
			  AW_I2C_BYTE_ONE);

	aw_dev_info("%s: trig1 date config ok!\n", __func__);

	if ((aw8622x->name == AW86224_5) && (!aw8622x->isUsedIntn)) {
		aw_dev_info("%s: intn pin is trig.\n", __func__);
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL2,
				AW8622X_BIT_SYSCTRL2_INTN_PIN_MASK,
				AW8622X_BIT_SYSCTRL2_TRIG1);
		return 0;
	}
	/* trig2 config */
	trig2_polar_lev_brk = aw8622x->trig[1].trig_polar << 2 |
				aw8622x->trig[1].trig_level << 1 |
				aw8622x->trig[1].trig_brk;
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_TRGCFG7,
				AW8622X_BIT_TRGCFG7_TRG2_POR_LEV_BRK_MASK,
				trig2_polar_lev_brk << 1);
	trig2_pos_seq = aw8622x->trig[1].pos_enable << 7 |
			aw8622x->trig[1].pos_sequence;
	aw8622x_i2c_write(aw8622x, AW8622X_REG_TRGCFG2, &trig2_pos_seq,
			  AW_I2C_BYTE_ONE);
	trig2_neg_seq = aw8622x->trig[1].neg_enable << 7 |
			aw8622x->trig[1].neg_sequence;
	aw8622x_i2c_write(aw8622x, AW8622X_REG_TRGCFG5, &trig2_neg_seq,
			  AW_I2C_BYTE_ONE);
	aw_dev_info("%s: trig2 date config ok!\n", __func__);

	/* trig3 config */
	trig3_polar_lev_brk = aw8622x->trig[2].trig_polar << 2 |
				aw8622x->trig[2].trig_level << 1 |
				aw8622x->trig[2].trig_brk;
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_TRGCFG8,
				AW8622X_BIT_TRGCFG8_TRG3_POR_LEV_BRK_MASK,
				trig3_polar_lev_brk << 5);
	trig3_pos_seq = aw8622x->trig[2].pos_enable << 7 |
			aw8622x->trig[2].pos_sequence;
	aw8622x_i2c_write(aw8622x, AW8622X_REG_TRGCFG3, &trig3_pos_seq,
			  AW_I2C_BYTE_ONE);
	trig3_neg_seq = aw8622x->trig[2].neg_enable << 7 |
			aw8622x->trig[2].neg_sequence;
	aw8622x_i2c_write(aw8622x, AW8622X_REG_TRGCFG6, &trig3_neg_seq,
			  AW_I2C_BYTE_ONE);
	aw_dev_info("%s: trig3 date config ok!\n", __func__);

	return 0;
}

/*****************************************************
 *
 * motor protect
 *
 *****************************************************/
static int aw8622x_haptic_swicth_motor_protect_config(struct aw8622x *aw8622x,
						      unsigned char addr,
						      unsigned char val)
{
	aw_dev_info("%s enter\n", __func__);
	if (addr == 1) {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_DETCFG1,
				       AW8622X_BIT_DETCFG1_PRCT_MODE_MASK,
				       AW8622X_BIT_DETCFG1_PRCT_MODE_VALID);
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_PWMCFG1,
				       AW8622X_BIT_PWMCFG1_PRC_EN_MASK,
				       AW8622X_BIT_PWMCFG1_PRC_ENABLE);
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_PWMCFG3,
				       AW8622X_BIT_PWMCFG3_PR_EN_MASK,
				       AW8622X_BIT_PWMCFG3_PR_ENABLE);
	} else if (addr == 0) {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_DETCFG1,
				       AW8622X_BIT_DETCFG1_PRCT_MODE_MASK,
				       AW8622X_BIT_DETCFG1_PRCT_MODE_INVALID);
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_PWMCFG1,
				       AW8622X_BIT_PWMCFG1_PRC_EN_MASK,
				       AW8622X_BIT_PWMCFG1_PRC_DISABLE);
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_PWMCFG3,
				       AW8622X_BIT_PWMCFG3_PR_EN_MASK,
				       AW8622X_BIT_PWMCFG3_PR_DISABLE);
	} else if (addr == 0x2d) {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_PWMCFG1,
				       AW8622X_BIT_PWMCFG1_PRCTIME_MASK, val);
	} else if (addr == 0x3e) {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_PWMCFG3,
				       AW8622X_BIT_PWMCFG3_PRLVL_MASK, val);
	} else if (addr == 0x3f) {
		aw8622x_i2c_write(aw8622x, AW8622X_REG_PWMCFG4, &val,
				  AW_I2C_BYTE_ONE);
	}
	return 0;
}

static int aw8622x_haptic_f0_calibration(struct aw8622x *aw8622x)
{
	unsigned char reg_val = 0;
	char f0_cali_lra = 0;
	unsigned int f0_limit = 0;
	unsigned int f0_cali_min = aw8622x->dts_info.f0_ref *
				(100 - aw8622x->dts_info.f0_cali_percent) / 100;
	unsigned int f0_cali_max =  aw8622x->dts_info.f0_ref *
				(100 + aw8622x->dts_info.f0_cali_percent) / 100;
	int ret = 0;
	int f0_cali_step = 0;

	aw_dev_info("%s enter\n", __func__);
	/*
	 * aw8622x_haptic_upload_lra(aw8622x, AW8622X_WRITE_ZERO);
	 */
	if (aw8622x_haptic_cont_get_f0(aw8622x)) {
		aw_dev_err("%s get f0 error, user defafult f0\n",
			   __func__);
	} else {
		/* max and min limit */
		f0_limit = aw8622x->f0;
		aw_dev_info("%s f0_ref = %d, f0_cali_min = %d, f0_cali_max = %d, f0 = %d\n",
			    __func__, aw8622x->dts_info.f0_ref,
			    f0_cali_min, f0_cali_max, aw8622x->f0);

		if ((aw8622x->f0 < f0_cali_min) || aw8622x->f0 > f0_cali_max) {
			aw_dev_err("%s f0 calibration out of range = %d!\n",
				   __func__, aw8622x->f0);
			f0_limit = aw8622x->dts_info.f0_ref;
			return -ERANGE;
		}
		aw_dev_info("%s f0_limit = %d\n", __func__,
			    (int)f0_limit);
		/* calculate cali step */
		f0_cali_step = 100000 * ((int)f0_limit -
					 (int)aw8622x->dts_info.f0_ref) /
		    ((int)f0_limit * 24);
		aw_dev_info("%s f0_cali_step = %d\n", __func__,
			    f0_cali_step);
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
		aw8622x->f0_cali_data = (int)f0_cali_lra;
		aw8622x_haptic_upload_lra(aw8622x, AW8622X_F0_CALI);

		aw8622x_i2c_read(aw8622x, AW8622X_REG_TRIMCFG3, &reg_val,
				 AW_I2C_BYTE_ONE);

		aw_dev_info("%s final trim_lra=0x%02x\n",
			__func__, reg_val);
	}
	/* restore standby work mode */
	aw8622x_haptic_stop_l(aw8622x);
	return ret;
}

/*****************************************************
 *
 * haptic cont
 *
 *****************************************************/
static int aw8622x_haptic_cont(struct aw8622x *aw8622x)
{
	unsigned char reg_val = 0;

	aw_dev_info("%s enter\n", __func__);

	/* work mode */
	aw8622x_haptic_play_mode(aw8622x, AW8622X_HAPTIC_CONT_MODE);
	/* cont config */
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_CONTCFG6,
			       (AW8622X_BIT_CONTCFG6_TRACK_EN_MASK &
				AW8622X_BIT_CONTCFG6_DRV1_LVL_MASK),
			       (AW8622X_BIT_CONTCFG6_TRACK_ENABLE |
				(unsigned char)aw8622x->cont_drv1_lvl));
	reg_val = (unsigned char)aw8622x->cont_drv2_lvl;
	aw8622x_i2c_write(aw8622x, AW8622X_REG_CONTCFG7, &reg_val,
			  AW_I2C_BYTE_ONE);
	/* DRV2_TIME */
	reg_val = 0xFF;
	aw8622x_i2c_write(aw8622x, AW8622X_REG_CONTCFG9, &reg_val,
			  AW_I2C_BYTE_ONE);
	/* cont play go */
	aw8622x_haptic_play_go(aw8622x, true);
	return 0;
}

static int aw8622x_haptic_play_wav_seq(struct aw8622x *aw8622x,
				       unsigned char flag)
{
	aw_dev_info("%s enter\n", __func__);
	if (flag) {
		aw8622x_haptic_play_mode(aw8622x, AW8622X_HAPTIC_RAM_MODE);
		aw8622x_haptic_play_go(aw8622x, true);
	}
	return 0;
}

#ifdef TIMED_OUTPUT
static int aw8622x_vibrator_get_time(struct timed_output_dev *dev)
{
	struct aw8622x *aw8622x = container_of(dev, struct aw8622x, vib_dev);

	if (hrtimer_active(&aw8622x->timer)) {
		ktime_t r = hrtimer_get_remaining(&aw8622x->timer);

		return ktime_to_ms(r);
	}
	return 0;
}

static void aw8622x_vibrator_enable(struct timed_output_dev *dev, int value)
{
	struct aw8622x *aw8622x = container_of(dev, struct aw8622x, vib_dev);

	aw_dev_info("%s enter\n", __func__);
	if (!aw8622x->ram_init) {
		aw_dev_err("%s: ram init failed, not allow to play!\n",
			   __func__);
		return;
	}
	mutex_lock(&aw8622x->lock);
	aw8622x_haptic_stop_l(aw8622x);
	if (value > 0) {
		aw8622x_haptic_ram_vbat_comp(aw8622x, false);
		aw8622x_haptic_play_wav_seq(aw8622x, value);
	}
	mutex_unlock(&aw8622x->lock);
	aw_dev_info("%s exit\n", __func__);
}
#else
static enum led_brightness aw8622x_haptic_brightness_get(struct led_classdev
							 *cdev)
{
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	return aw8622x->amplitude;
}

static void aw8622x_haptic_brightness_set(struct led_classdev *cdev,
					  enum led_brightness level)
{
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	aw_dev_info("%s enter\n", __func__);
	if (!aw8622x->ram_init) {
		aw_dev_err("%s: ram init failed, not allow to play!\n",
			   __func__);
		return;
	}
	if (aw8622x->ram_update_flag < 0)
		return;
	aw8622x->amplitude = level;
	mutex_lock(&aw8622x->lock);
	aw8622x_haptic_stop_l(aw8622x);
	if (aw8622x->amplitude > 0) {
		aw8622x_haptic_upload_lra(aw8622x, AW8622X_F0_CALI);
		aw8622x_haptic_ram_vbat_comp(aw8622x, false);
		aw8622x_haptic_play_wav_seq(aw8622x, aw8622x->amplitude);
	}
	mutex_unlock(&aw8622x->lock);
}
#endif

static int
aw8622x_haptic_audio_ctr_list_insert(struct haptic_audio *haptic_audio,
				     struct haptic_ctr *haptic_ctr,
				     struct device *dev)
{
	struct haptic_ctr *p_new = NULL;

	p_new = kzalloc(sizeof(struct haptic_ctr), GFP_KERNEL);
	if (p_new == NULL)
		return -ENOMEM;
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

static int
aw8622x_haptic_audio_ctr_list_clear(struct haptic_audio *haptic_audio)
{
	struct haptic_ctr *p_ctr = NULL;
	struct haptic_ctr *p_ctr_bak = NULL;

	list_for_each_entry_safe_reverse(p_ctr, p_ctr_bak,
					&(haptic_audio->ctr_list),
					list) {
		list_del(&p_ctr->list);
		kfree(p_ctr);
	}

	return 0;
}

static int aw8622x_haptic_audio_off(struct aw8622x *aw8622x)
{
	aw_dev_dbg("%s: enter\n", __func__);
	mutex_lock(&aw8622x->lock);
	aw8622x_haptic_set_gain(aw8622x, 0x80);
	aw8622x_haptic_stop_l(aw8622x);
	aw8622x->gun_type = AW_GUN_TYPE_DEF_VAL;
	aw8622x->bullet_nr = AW_BULLET_NR_DEF_VAL;
	aw8622x_haptic_audio_ctr_list_clear(&aw8622x->haptic_audio);
	mutex_unlock(&aw8622x->lock);
	return 0;
}

static int aw8622x_haptic_audio_init(struct aw8622x *aw8622x)
{

	aw_dev_dbg("%s enter\n", __func__);
	aw8622x_haptic_set_wav_seq(aw8622x, 0x01, 0x00);

	return 0;
}

static int aw8622x_haptic_activate(struct aw8622x *aw8622x)
{
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL2,
			       AW8622X_BIT_SYSCTRL2_STANDBY_MASK,
			       AW8622X_BIT_SYSCTRL2_STANDBY_OFF);
	aw8622x_interrupt_clear(aw8622x);
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSINTM,
			       AW8622X_BIT_SYSINTM_UVLM_MASK,
			       AW8622X_BIT_SYSINTM_UVLM_ON);
	return 0;
}

static int aw8622x_haptic_start(struct aw8622x *aw8622x)
{
	aw8622x_haptic_activate(aw8622x);
	aw8622x_haptic_play_go(aw8622x, true);
	return 0;
}

static void aw8622x_ctr_list_config(struct aw8622x *aw8622x,
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
			(AW8622X_HAPTIC_CMD_ENABLE ==
				(AW8622X_HAPTIC_CMD_HAPTIC & p_ctr->cmd))) {
				list_del(&p_ctr->list);
				kfree(p_ctr);
				list_del_cnt++;
			}
			if (list_del_cnt == list_diff_cnt)
				break;
		}
	}
}

static void aw8622x_haptic_audio_work_routine(struct work_struct *work)
{
	struct aw8622x *aw8622x = container_of(work,
					struct aw8622x,
					haptic_audio.work);
	struct haptic_audio *haptic_audio = NULL;
	struct haptic_ctr *p_ctr = NULL;
	struct haptic_ctr *p_ctr_bak = NULL;
	unsigned int ctr_list_flag = 0;
	int rtp_is_going_on = 0;

	aw_dev_dbg("%s enter\n", __func__);

	haptic_audio = &(aw8622x->haptic_audio);
	mutex_lock(&aw8622x->haptic_audio.lock);
	memset(&aw8622x->haptic_audio.ctr, 0, sizeof(struct haptic_ctr));
	ctr_list_flag = 0;
	list_for_each_entry_safe_reverse(p_ctr, p_ctr_bak,
			&(haptic_audio->ctr_list), list) {
		ctr_list_flag = 1;
		break;
	}
	if (ctr_list_flag == 0)
		aw_dev_info("%s: ctr list empty\n", __func__);

	if (ctr_list_flag == 1)
		aw8622x_ctr_list_config(aw8622x, p_ctr,
					p_ctr_bak, haptic_audio);

	/* get the last data from list */
	list_for_each_entry_safe_reverse(p_ctr, p_ctr_bak,
				&(haptic_audio->ctr_list), list) {
		aw8622x->haptic_audio.ctr.cnt = p_ctr->cnt;
		aw8622x->haptic_audio.ctr.cmd = p_ctr->cmd;
		aw8622x->haptic_audio.ctr.play = p_ctr->play;
		aw8622x->haptic_audio.ctr.wavseq = p_ctr->wavseq;
		aw8622x->haptic_audio.ctr.loop = p_ctr->loop;
		aw8622x->haptic_audio.ctr.gain = p_ctr->gain;
		list_del(&p_ctr->list);
		kfree(p_ctr);
		break;
	}

	if (aw8622x->haptic_audio.ctr.play) {
		aw_dev_info("%s: cnt=%d, cmd=%d, play=%d, wavseq=%d, loop=%d, gain=%d\n",
			__func__,
			aw8622x->haptic_audio.ctr.cnt,
			aw8622x->haptic_audio.ctr.cmd,
			aw8622x->haptic_audio.ctr.play,
			aw8622x->haptic_audio.ctr.wavseq,
			aw8622x->haptic_audio.ctr.loop,
			aw8622x->haptic_audio.ctr.gain);
	}

	/* rtp mode jump */
	rtp_is_going_on = aw8622x_haptic_juge_RTP_is_going_on(aw8622x);
	if (rtp_is_going_on) {
		mutex_unlock(&aw8622x->haptic_audio.lock);
		return;
	}
	mutex_unlock(&aw8622x->haptic_audio.lock);

	/*haptic play control*/
	if (AW8622X_HAPTIC_CMD_ENABLE ==
	   (AW8622X_HAPTIC_CMD_HAPTIC & aw8622x->haptic_audio.ctr.cmd)) {
		if (aw8622x->haptic_audio.ctr.play ==
			AW8622X_HAPTIC_PLAY_ENABLE) {
			aw_dev_info("%s: haptic_audio_play_start\n", __func__);
			aw_dev_info("%s: normal haptic start\n", __func__);
			mutex_lock(&aw8622x->lock);
			aw8622x_haptic_stop_l(aw8622x);
			aw8622x_haptic_play_mode(aw8622x,
				AW8622X_HAPTIC_RAM_MODE);
			aw8622x_haptic_set_wav_seq(aw8622x, 0x00,
				aw8622x->haptic_audio.ctr.wavseq);
			aw8622x_haptic_set_wav_loop(aw8622x, 0x00,
				aw8622x->haptic_audio.ctr.loop);
			aw8622x_haptic_set_gain(aw8622x,
				aw8622x->haptic_audio.ctr.gain);
			aw8622x_haptic_start(aw8622x);
			mutex_unlock(&aw8622x->lock);
		} else if (AW8622X_HAPTIC_PLAY_STOP ==
			   aw8622x->haptic_audio.ctr.play) {
			mutex_lock(&aw8622x->lock);
			aw8622x_haptic_stop_l(aw8622x);
			mutex_unlock(&aw8622x->lock);
		} else if (AW8622X_HAPTIC_PLAY_GAIN ==
			   aw8622x->haptic_audio.ctr.play) {
			mutex_lock(&aw8622x->lock);
			aw8622x_haptic_set_gain(aw8622x,
					       aw8622x->haptic_audio.ctr.gain);
			mutex_unlock(&aw8622x->lock);
		}
	}
}

static ssize_t aw8622x_cont_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	aw8622x_haptic_read_cont_f0(aw8622x);
	len += snprintf(buf + len, PAGE_SIZE - len,
			"%d\n", aw8622x->cont_f0);
	return len;
}

static ssize_t aw8622x_cont_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	unsigned int val = 0;
	int rc = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);


	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;

	aw8622x_haptic_stop_l(aw8622x);
	if (val)
		aw8622x_haptic_cont(aw8622x);
	return count;
}

static ssize_t aw8622x_f0_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	unsigned char reg = 0;
	ssize_t len = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	mutex_lock(&aw8622x->lock);

	/* set d2s_gain to max to get better performance when cat f0 .*/
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL7,
				AW8622X_BIT_SYSCTRL7_D2S_GAIN_MASK,
				AW8622X_BIT_SYSCTRL7_D2S_GAIN_40);
	aw8622x_i2c_read(aw8622x, AW8622X_REG_TRIMCFG3, &reg, AW_I2C_BYTE_ONE);
	aw8622x_haptic_upload_lra(aw8622x, AW8622X_WRITE_ZERO);
	aw8622x_haptic_cont_get_f0(aw8622x);
	aw8622x_i2c_write(aw8622x, AW8622X_REG_TRIMCFG3, &reg, AW_I2C_BYTE_ONE);
	/* set d2s_gain to default when cat f0 is finished.*/
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL7,
				AW8622X_BIT_SYSCTRL7_D2S_GAIN_MASK,
				aw8622x->dts_info.d2s_gain);
	mutex_unlock(&aw8622x->lock);
	len += snprintf(buf + len, PAGE_SIZE - len,
			"%d\n", aw8622x->f0);
	return len;
}

static ssize_t aw8622x_f0_store(struct device *dev,
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

static ssize_t aw8622x_reg_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	unsigned char i = 0;
	unsigned char size = 0;
	unsigned char cnt = 0;
	unsigned char reg_array[AW_REG_MAX] = {0};
	ssize_t len = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	aw_dev_dbg("%s: enter!\n", __func__);

	for (i = 0; i <= (AW8622X_REG_TRIMCFG3 + 1); i++) {

		if (i == aw8622x_reg_list[cnt] &&
		    (cnt < sizeof(aw8622x_reg_list))) {
			size++;
			cnt++;
			continue;
		} else {
			if (size != 0) {
				aw8622x_i2c_read(aw8622x,
						 aw8622x_reg_list[cnt-size],
						 &reg_array[cnt-size], size);
				size = 0;

			}
		}
	}

	for (i = 0; i < sizeof(aw8622x_reg_list); i++) {
		len += snprintf(buf + len, PAGE_SIZE - len,
				"reg:0x%02X=0x%02X\n", aw8622x_reg_list[i],
				reg_array[i]);
	}
	return len;
}

static ssize_t aw8622x_reg_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	unsigned char reg_val = 0;
	unsigned int databuf[2] = { 0, 0 };
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		reg_val = (unsigned char)databuf[1];
		aw8622x_i2c_write(aw8622x, (unsigned char)databuf[0],
				  &reg_val, AW_I2C_BYTE_ONE);
	}

	return count;
}

static ssize_t aw8622x_duration_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);
	ktime_t time_rem;
	s64 time_ms = 0;

	if (hrtimer_active(&aw8622x->timer)) {
		time_rem = hrtimer_get_remaining(&aw8622x->timer);
		time_ms = ktime_to_ms(time_rem);
	}
	return snprintf(buf, PAGE_SIZE, "%lld\n", time_ms);
}

static ssize_t aw8622x_duration_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	unsigned int val = 0;
	int rc = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;
	/* setting 0 on duration is NOP for now */
	if (val <= 0)
		return count;
	rc = aw8622x_haptic_ram_config(aw8622x, val);
	if (rc < 0) {
		aw_dev_info("%s: ram config failed!\n", __func__);
		return count;
	}
	aw8622x->duration = val;
	return count;
}

static ssize_t aw8622x_activate_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	/* For now nothing to show */
	return snprintf(buf, PAGE_SIZE, "activate = %d\n", aw8622x->state);
}

static ssize_t aw8622x_activate_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	unsigned int val = 0;
	int rc = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	if (!aw8622x->ram_init) {
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
	mutex_lock(&aw8622x->lock);
	hrtimer_cancel(&aw8622x->timer);
	aw8622x->state = val;
	mutex_unlock(&aw8622x->lock);
	schedule_work(&aw8622x->vibrator_work);
	return count;
}

static ssize_t aw8622x_seq_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	unsigned char i = 0;
	unsigned char reg_val[AW8622X_SEQUENCER_SIZE] = {0};
	size_t count = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	aw8622x_i2c_read(aw8622x, AW8622X_REG_WAVCFG1, reg_val,
			 AW8622X_SEQUENCER_SIZE);
	for (i = 0; i < AW8622X_SEQUENCER_SIZE; i++) {
		count += snprintf(buf+count, PAGE_SIZE-count,
			"seq%d: 0x%02x\n", i+1, reg_val[i]);
		aw8622x->seq[i] = reg_val[i];
	}
	return count;
}

static ssize_t aw8622x_seq_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	unsigned int databuf[2] = { 0, 0 };
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		if (databuf[0] > AW8622X_SEQUENCER_SIZE ||
		    databuf[1] > aw8622x->ram.ram_num) {
			aw_dev_err("%s input value out of range\n",
				__func__);
			return count;
		}
		aw_dev_info("%s: seq%d=0x%02X\n", __func__,
			    databuf[0], databuf[1]);
		mutex_lock(&aw8622x->lock);
		aw8622x->seq[databuf[0]] = (unsigned char)databuf[1];
		aw8622x_haptic_set_wav_seq(aw8622x, (unsigned char)databuf[0],
					   aw8622x->seq[databuf[0]]);
		mutex_unlock(&aw8622x->lock);
	}
	return count;
}

static ssize_t aw8622x_loop_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	unsigned char i = 0;
	unsigned char reg_val[AW8622X_SEQUENCER_LOOP_SIZE] = {0};
	size_t count = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	aw8622x_i2c_read(aw8622x, AW8622X_REG_WAVCFG9, reg_val,
			 AW8622X_SEQUENCER_LOOP_SIZE);

	for (i = 0; i < AW8622X_SEQUENCER_LOOP_SIZE; i++) {
		aw8622x->loop[i*2+0] = (reg_val[i] >> 4) & 0x0F;
		aw8622x->loop[i*2+1] = (reg_val[i] >> 0) & 0x0F;

		count += snprintf(buf+count, PAGE_SIZE-count,
			"seq%d_loop: 0x%02x\n", i*2+1, aw8622x->loop[i*2+0]);
		count += snprintf(buf+count, PAGE_SIZE-count,
			"seq%d_loop: 0x%02x\n", i*2+2, aw8622x->loop[i*2+1]);
	}
	return count;
}

static ssize_t aw8622x_loop_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	unsigned int databuf[2] = { 0, 0 };
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		aw_dev_info("%s: seq%d loop=0x%02X\n", __func__,
			    databuf[0], databuf[1]);
		mutex_lock(&aw8622x->lock);
		aw8622x->loop[databuf[0]] = (unsigned char)databuf[1];
		aw8622x_haptic_set_wav_loop(aw8622x, (unsigned char)databuf[0],
					    aw8622x->loop[databuf[0]]);
		mutex_unlock(&aw8622x->lock);
	}
	return count;
}

static ssize_t aw8622x_rtp_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	len += snprintf(buf + len, PAGE_SIZE - len,
			"rtp_cnt = %d\n",
			aw8622x->rtp_cnt);
	return len;
}

static ssize_t aw8622x_rtp_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	unsigned int val = 0;
	int rc = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev,
				struct aw8622x, vib_dev);


	rc = kstrtouint(buf, 0, &val);
	if (rc < 0) {
		aw_dev_info("%s: kstrtouint fail\n", __func__);
		return count;
	}
	mutex_lock(&aw8622x->lock);
	aw8622x_haptic_stop_l(aw8622x);
	aw8622x_haptic_set_rtp_aei(aw8622x, false);
	aw8622x_interrupt_clear(aw8622x);
	if (val < (sizeof(aw8622x_rtp_name) / AW8622X_RTP_NAME_MAX)) {
		aw8622x->rtp_file_num = val;
		if (val) {
			aw_dev_info("%s: aw8622x_rtp_name[%d]: %s\n", __func__,
				val, aw8622x_rtp_name[val]);

			schedule_work(&aw8622x->rtp_work);
		} else {
			aw_dev_err("%s: rtp_file_num 0x%02X over max value\n",
				   __func__, aw8622x->rtp_file_num);
		}
	}
	mutex_unlock(&aw8622x->lock);
	return count;
}

static ssize_t aw8622x_state_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", aw8622x->state);
}

static ssize_t aw8622x_state_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	return count;
}

static ssize_t aw8622x_activate_mode_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	return snprintf(buf, PAGE_SIZE, "%d\n",
			aw8622x->activate_mode);
}

static ssize_t aw8622x_activate_mode_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	unsigned int val = 0;
	int rc = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);


	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;
	mutex_lock(&aw8622x->lock);
	aw8622x->activate_mode = val;
	mutex_unlock(&aw8622x->lock);
	return count;
}

static ssize_t aw8622x_index_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	unsigned char reg_val = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	aw8622x_i2c_read(aw8622x, AW8622X_REG_WAVCFG1, &reg_val,
			 AW_I2C_BYTE_ONE);
	aw8622x->index = reg_val;
	return snprintf(buf, PAGE_SIZE, "index = %d\n", aw8622x->index);
}

static ssize_t aw8622x_index_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	unsigned int val = 0;
	int rc = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;
	if (val > aw8622x->ram.ram_num) {
		aw_dev_err("%s: input value out of range!\n", __func__);
		return count;
	}
	aw_dev_info("%s: value=%d\n", __func__, val);
	mutex_lock(&aw8622x->lock);
	aw8622x->index = val;
	aw8622x_haptic_set_repeat_wav_seq(aw8622x, aw8622x->index);
	mutex_unlock(&aw8622x->lock);
	return count;
}

static ssize_t aw8622x_sram_size_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	unsigned char reg_val = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	aw8622x_i2c_read(aw8622x, AW8622X_REG_RTPCFG1, &reg_val,
			 AW_I2C_BYTE_ONE);
	if ((reg_val & 0x30) == 0x20)
		return snprintf(buf, PAGE_SIZE, "sram_size = 2K\n");
	else if ((reg_val & 0x30) == 0x10)
		return snprintf(buf, PAGE_SIZE, "sram_size = 1K\n");
	else if ((reg_val & 0x30) == 0x30)
		return snprintf(buf, PAGE_SIZE, "sram_size = 3K\n");
	return snprintf(buf, PAGE_SIZE,
			"sram_size = 0x%02x error, plz check reg.\n",
			reg_val & 0x30);
}

static ssize_t aw8622x_sram_size_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned int val = 0;
	int rc = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;
	aw_dev_info("%s: value=%d\n", __func__, val);
	mutex_lock(&aw8622x->lock);
	if (val == AW8622X_HAPTIC_SRAM_2K)
		aw8622x_sram_size(aw8622x, AW8622X_HAPTIC_SRAM_2K);
	else if (val == AW8622X_HAPTIC_SRAM_1K)
		aw8622x_sram_size(aw8622x, AW8622X_HAPTIC_SRAM_1K);
	else if (val == AW8622X_HAPTIC_SRAM_3K)
		aw8622x_sram_size(aw8622x, AW8622X_HAPTIC_SRAM_3K);
	mutex_unlock(&aw8622x->lock);
	return count;
}

static ssize_t aw8622x_osc_cali_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	len += snprintf(buf + len, PAGE_SIZE - len, "osc_cali_data = 0x%02X\n",
			aw8622x->osc_cali_data);

	return len;
}

static ssize_t aw8622x_osc_cali_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	unsigned int val = 0;
	int rc = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;
	mutex_lock(&aw8622x->lock);
	if (val == 1) {
		aw8622x_haptic_upload_lra(aw8622x, AW8622X_WRITE_ZERO);
		aw8622x_rtp_osc_calibration(aw8622x);
		aw8622x_rtp_trim_lra_calibration(aw8622x);
	} else if (val == 2) {
		aw8622x_haptic_upload_lra(aw8622x, AW8622X_OSC_CALI);
		aw8622x_rtp_osc_calibration(aw8622x);
	} else {
		aw_dev_err("%s input value out of range\n",  __func__);
	}
	/* osc calibration flag end, other behaviors are permitted */
	mutex_unlock(&aw8622x->lock);

	return count;
}

static ssize_t aw8622x_gain_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	unsigned char reg = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	aw8622x_i2c_read(aw8622x, AW8622X_REG_PLAYCFG2, &reg, AW_I2C_BYTE_ONE);

	return snprintf(buf, PAGE_SIZE, "0x%02X\n", reg);
}

static ssize_t aw8622x_gain_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	unsigned int val = 0;
	int rc = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;

	aw_dev_info("%s: value=%d\n", __func__, val);
	if (val >= AW8622X_GAIN_MAX)
		val = AW8622X_GAIN_MAX;
	mutex_lock(&aw8622x->lock);
	aw8622x->gain = val;
	aw8622x_haptic_set_gain(aw8622x, aw8622x->gain);
	mutex_unlock(&aw8622x->lock);
	return count;
}

static ssize_t aw8622x_ram_update_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	int cnt = 0;
	int i = 0;
	int size = 0;
	ssize_t len = 0;
	unsigned char ram_data[AW_RAMDATA_RD_BUFFER_SIZE] = {0};
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	/* RAMINIT Enable */
	aw8622x_haptic_raminit(aw8622x, true);
	aw8622x_haptic_stop_l(aw8622x);
	aw8622x_set_ram_addr(aw8622x);
	len += snprintf(buf + len, PAGE_SIZE - len,
			"haptic_ram len = %d\n", aw8622x->ram.len);
	while (cnt < aw8622x->ram.len) {
		if ((aw8622x->ram.len - cnt) < AW_RAMDATA_RD_BUFFER_SIZE)
			size = aw8622x->ram.len - cnt;
		else
			size = AW_RAMDATA_RD_BUFFER_SIZE;

		aw8622x_i2c_read(aw8622x, AW8622X_REG_RAMDATA,
				 ram_data, size);
		for (i = 0; i < size; i++) {
			len += snprintf(buf + len, PAGE_SIZE - len,
					"0x%02X,", ram_data[i]);
		}
		cnt += size;
	}
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	/* RAMINIT Disable */
	aw8622x_haptic_raminit(aw8622x, false);
	return len;
}

static ssize_t aw8622x_ram_update_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned int val = 0;
	int rc = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;
	if (val)
		aw8622x_ram_update(aw8622x);
	return count;
}

static ssize_t aw8622x_f0_save_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	len += snprintf(buf + len, PAGE_SIZE - len, "f0_cali_data = 0x%02X\n",
			aw8622x->f0_cali_data);

	return len;
}

static ssize_t aw8622x_f0_save_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	unsigned int val = 0;
	int rc = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;
	aw8622x->f0_cali_data = val;
	return count;
}

static ssize_t aw8622x_osc_save_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	len += snprintf(buf + len, PAGE_SIZE - len, "osc_cali_data = 0x%02X\n",
			aw8622x->osc_cali_data);

	return len;
}

static ssize_t aw8622x_osc_save_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	unsigned int val = 0;
	int rc = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;
	aw8622x->osc_cali_data = val;
	return count;
}

static ssize_t aw8622x_trig_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);
	unsigned char i = 0;
	unsigned char trig_num = 3;

	if (aw8622x->name == AW86224_5)
		trig_num = 1;

	for (i = 0; i < trig_num; i++) {
		len += snprintf(buf + len, PAGE_SIZE - len,
				"trig%d: trig_level=%d, trig_polar=%d",
				i + 1,
				aw8622x->trig[i].trig_level,
				aw8622x->trig[i].trig_polar);
		len += snprintf(buf + len, PAGE_SIZE - len,
				"pos_enable=%d, pos_sequence=%d,",
				aw8622x->trig[i].pos_enable,
				aw8622x->trig[i].pos_sequence);
		len += snprintf(buf + len, PAGE_SIZE - len,
				"neg_enable=%d, neg_sequence=%d trig_brk=%d\n",
				aw8622x->trig[i].neg_enable,
				aw8622x->trig[i].neg_sequence,
				aw8622x->trig[i].trig_brk);
	}

	return len;
}



static ssize_t aw8622x_trig_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);
	unsigned int databuf[9] = { 0 };

	if (sscanf(buf, "%d %d %d %d %d %d %d %d",
		&databuf[0], &databuf[1], &databuf[2],
		&databuf[3], &databuf[4], &databuf[5],
		&databuf[6], &databuf[7]) == 8) {
		aw_dev_info("%s: %d, %d, %d, %d, %d, %d, %d, %d\n",
			    __func__, databuf[0], databuf[1], databuf[2],
			    databuf[3], databuf[4], databuf[5], databuf[6],
			    databuf[7]);
		if ((aw8622x->name == AW86224_5) && (databuf[0])) {
			aw_dev_err("%s: input seq value out of range!\n",
				   __func__);
			return count;
		}
		if (databuf[0] < 0 || databuf[0] > 2) {
			aw_dev_err("%s: input seq value out of range!\n",
				   __func__);
			return count;
		}
		if (!aw8622x->ram_init) {
			aw_dev_err("%s: ram init failed, not allow to play!\n",
				   __func__);
			return count;
		}
		if (databuf[4] > aw8622x->ram.ram_num ||
		    databuf[6] > aw8622x->ram.ram_num) {
			aw_dev_err("%s: input seq value out of range!\n",
				   __func__);
			return count;
		}
		aw8622x->trig[databuf[0]].trig_level = databuf[1];
		aw8622x->trig[databuf[0]].trig_polar = databuf[2];
		aw8622x->trig[databuf[0]].pos_enable = databuf[3];
		aw8622x->trig[databuf[0]].pos_sequence = databuf[4];
		aw8622x->trig[databuf[0]].neg_enable = databuf[5];
		aw8622x->trig[databuf[0]].neg_sequence = databuf[6];
		aw8622x->trig[databuf[0]].trig_brk = databuf[7];
		mutex_lock(&aw8622x->lock);
		aw8622x_haptic_trig_param_config(aw8622x);
		mutex_unlock(&aw8622x->lock);
	} else
		aw_dev_err("%s: please input eight parameters\n", __func__);
	return count;
}

static ssize_t aw8622x_ram_vbat_compensate_show(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	ssize_t len = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "ram_vbat_comp = %d\n",
		     aw8622x->ram_vbat_compensate);

	return len;
}

static ssize_t aw8622x_ram_vbat_compensate_store(struct device *dev,
						 struct device_attribute *attr,
						 const char *buf, size_t count)
{
	unsigned int val = 0;
	int rc = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;

	mutex_lock(&aw8622x->lock);
	if (val)
		aw8622x->ram_vbat_compensate =
		    AW8622X_HAPTIC_RAM_VBAT_COMP_ENABLE;
	else
		aw8622x->ram_vbat_compensate =
		    AW8622X_HAPTIC_RAM_VBAT_COMP_DISABLE;
	mutex_unlock(&aw8622x->lock);

	return count;
}

static ssize_t aw8622x_cali_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);
	unsigned char reg = 0;

	mutex_lock(&aw8622x->lock);
	aw8622x_i2c_read(aw8622x, AW8622X_REG_TRIMCFG3, &reg,
			 AW_I2C_BYTE_ONE);
	aw8622x_haptic_upload_lra(aw8622x, AW8622X_F0_CALI);
	aw8622x_haptic_cont_get_f0(aw8622x);
	aw8622x_i2c_write(aw8622x, AW8622X_REG_TRIMCFG3, &reg,
			  AW_I2C_BYTE_ONE);
	mutex_unlock(&aw8622x->lock);
	len += snprintf(buf + len, PAGE_SIZE - len, "%d\n", aw8622x->f0);
	return len;
}

static ssize_t aw8622x_cali_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	unsigned int val = 0;
	int rc = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;
	if (val) {
		mutex_lock(&aw8622x->lock);
		aw8622x_haptic_upload_lra(aw8622x, AW8622X_WRITE_ZERO);
		aw8622x_haptic_f0_calibration(aw8622x);
		mutex_unlock(&aw8622x->lock);
	}
	return count;
}

static ssize_t aw8622x_cont_wait_num_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	ssize_t len = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	len += snprintf(buf + len, PAGE_SIZE - len,
			"cont_wait_num = 0x%02X\n", aw8622x->cont_wait_num);
	return len;
}

static ssize_t aw8622x_cont_wait_num_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	unsigned char reg_val = 0;
	int err, val;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	err = kstrtoint(buf, 16, &val);
	if (err != 0) {
		aw_dev_err("%s format not match!", __func__);
		return count;
	}
	aw8622x->cont_wait_num = val;
	reg_val = aw8622x->cont_wait_num;
	aw8622x_i2c_write(aw8622x, AW8622X_REG_CONTCFG4, &reg_val,
			  AW_I2C_BYTE_ONE);
	return count;
}

static ssize_t aw8622x_cont_drv_lvl_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	ssize_t len = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	len += snprintf(buf + len, PAGE_SIZE - len,
			"cont_drv1_lvl = 0x%02X\n",
			aw8622x->cont_drv1_lvl);
	len += snprintf(buf + len, PAGE_SIZE - len,
			"cont_drv2_lvl = 0x%02X\n",
			aw8622x->cont_drv2_lvl);
	return len;
}

static ssize_t aw8622x_cont_drv_lvl_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	unsigned char reg_val = 0;
	unsigned int databuf[2] = { 0, 0 };
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		aw8622x->cont_drv1_lvl = databuf[0];
		aw8622x->cont_drv2_lvl = databuf[1];
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_CONTCFG6,
				       AW8622X_BIT_CONTCFG6_DRV1_LVL_MASK,
				       aw8622x->cont_drv1_lvl);
		reg_val = (unsigned char)aw8622x->cont_drv2_lvl;
		aw8622x_i2c_write(aw8622x, AW8622X_REG_CONTCFG7, &reg_val,
				  AW_I2C_BYTE_ONE);
	}
	return count;
}

static ssize_t aw8622x_cont_drv_time_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	ssize_t len = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	len += snprintf(buf + len, PAGE_SIZE - len,
			"cont_drv1_time = 0x%02X\n",
			aw8622x->cont_drv1_time);
	len += snprintf(buf + len, PAGE_SIZE - len,
			"cont_drv2_time = 0x%02X\n",
			aw8622x->cont_drv2_time);
	return len;
}

static ssize_t aw8622x_cont_drv_time_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	unsigned char reg_val[2] = {0};
	unsigned int databuf[2] = { 0, 0 };
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		aw8622x->cont_drv1_time = databuf[0];
		aw8622x->cont_drv2_time = databuf[1];
		reg_val[0] = (unsigned char)aw8622x->cont_drv1_time;
		reg_val[1] = (unsigned char)aw8622x->cont_drv2_time;
		aw8622x_i2c_write(aw8622x, AW8622X_REG_CONTCFG8, reg_val,
				  AW_I2C_BYTE_TWO);
	}
	return count;
}

static ssize_t aw8622x_cont_brk_time_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	ssize_t len = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	len += snprintf(buf + len, PAGE_SIZE - len, "cont_brk_time = 0x%02X\n",
			aw8622x->cont_brk_time);
	return len;
}

static ssize_t aw8622x_cont_brk_time_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	unsigned char reg_val = 0;
	int err, val;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	err = kstrtoint(buf, 16, &val);
	if (err != 0) {
		aw_dev_err("%s format not match!", __func__);
		return count;
	}
	aw8622x->cont_brk_time = val;
	reg_val = aw8622x->cont_brk_time;
	aw8622x_i2c_write(aw8622x, AW8622X_REG_CONTCFG10, &reg_val,
			  AW_I2C_BYTE_ONE);
	return count;
}

static ssize_t aw8622x_vbat_monitor_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	ssize_t len = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	mutex_lock(&aw8622x->lock);
	aw8622x_haptic_get_vbat(aw8622x);
	len += snprintf(buf + len, PAGE_SIZE - len, "vbat_monitor = %d\n",
			aw8622x->vbat);
	mutex_unlock(&aw8622x->lock);

	return len;
}

static ssize_t aw8622x_vbat_monitor_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	return count;
}

static ssize_t aw8622x_lra_resistance_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	ssize_t len = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	aw8622x_haptic_get_lra_resistance(aw8622x);
	len += snprintf(buf + len, PAGE_SIZE - len, "lra_resistance = %d\n",
			aw8622x->lra);
	return len;
}

static ssize_t aw8622x_lra_resistance_store(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t count)
{
	return count;
}

static ssize_t aw8622x_prctmode_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	unsigned char reg_val = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	aw8622x_i2c_read(aw8622x, AW8622X_REG_DETCFG1, &reg_val,
			 AW_I2C_BYTE_ONE);

	len += snprintf(buf + len, PAGE_SIZE - len, "prctmode = %d\n",
			reg_val & 0x08);
	return len;
}

static ssize_t aw8622x_prctmode_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	unsigned int databuf[2] = { 0, 0 };
	unsigned int addr = 0;
	unsigned int val = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		addr = databuf[0];
		val = databuf[1];
		mutex_lock(&aw8622x->lock);
		aw8622x_haptic_swicth_motor_protect_config(aw8622x, addr, val);
		mutex_unlock(&aw8622x->lock);
	}
	return count;
}

static ssize_t aw8622x_gun_type_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	return snprintf(buf, PAGE_SIZE, "0x%02x\n", aw8622x->gun_type);

}

static ssize_t aw8622x_gun_type_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	unsigned int val = 0;
	int rc = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;
	aw_dev_info("%s: value=%d\n", __func__, val);
	mutex_lock(&aw8622x->lock);
	aw8622x->gun_type = val;
	mutex_unlock(&aw8622x->lock);
	return count;
}

static ssize_t aw8622x_bullet_nr_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	return snprintf(buf, PAGE_SIZE, "0x%02x\n", aw8622x->bullet_nr);
}

static ssize_t aw8622x_bullet_nr_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	unsigned int val = 0;
	int rc = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;
	aw_dev_info("%s: value=%d\n", __func__, val);
	mutex_lock(&aw8622x->lock);
	aw8622x->bullet_nr = val;
	mutex_unlock(&aw8622x->lock);
	return count;
}

static ssize_t aw8622x_haptic_audio_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	len += snprintf(buf+len, PAGE_SIZE-len,
			"%d\n", aw8622x->haptic_audio.ctr.cnt);
	return len;
}

static int aw8622x_audio_config(struct aw8622x *aw8622x,
				unsigned int databuf[])
{
	struct haptic_ctr *hap_ctr = NULL;

	hap_ctr = kzalloc(sizeof(struct haptic_ctr), GFP_KERNEL);
	if (hap_ctr == NULL)
		return -ENOMEM;
	mutex_lock(&aw8622x->haptic_audio.lock);
	hap_ctr->cnt = (unsigned char)databuf[0];
	hap_ctr->cmd = (unsigned char)databuf[1];
	hap_ctr->play = (unsigned char)databuf[2];
	hap_ctr->wavseq = (unsigned char)databuf[3];
	hap_ctr->loop = (unsigned char)databuf[4];
	hap_ctr->gain = (unsigned char)databuf[5];
	aw8622x_haptic_audio_ctr_list_insert(&aw8622x->haptic_audio,
					hap_ctr, aw8622x->dev);
	if (hap_ctr->cmd == 0xff) {
		aw_dev_info("%s: haptic_audio stop\n", __func__);
		if (hrtimer_active(&aw8622x->haptic_audio.timer)) {
			aw_dev_info(    "%s: cancel haptic_audio_timer\n",
				    __func__);
			hrtimer_cancel(&aw8622x->haptic_audio.timer);
			aw8622x->haptic_audio.ctr.cnt = 0;
			aw8622x_haptic_audio_off(aw8622x);
		}
	} else {
		if (hrtimer_active(&aw8622x->haptic_audio.timer)) {
		} else {
			aw_dev_info(    "%s: start haptic_audio_timer\n",
				    __func__);
			aw8622x_haptic_audio_init(aw8622x);
			hrtimer_start(&aw8622x->haptic_audio.timer,
			ktime_set(aw8622x->haptic_audio.delay_val/1000000,
				(aw8622x->haptic_audio.delay_val%1000000)*1000),
			HRTIMER_MODE_REL);
		}
	}
	kfree(hap_ctr);
	mutex_unlock(&aw8622x->haptic_audio.lock);
	return 0;
}

static ssize_t aw8622x_haptic_audio_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	unsigned int databuf[6] = {0};
	int rtp_is_going_on = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);


	rtp_is_going_on = aw8622x_haptic_juge_RTP_is_going_on(aw8622x);
	if (rtp_is_going_on) {
		aw_dev_info("%s: RTP is runing, stop audio haptic\n", __func__);
		return count;
	}
	if (!aw8622x->ram_init)
		return count;

	if (sscanf(buf, "%d %d %d %d %d %d",
		&databuf[0], &databuf[1], &databuf[2],
		&databuf[3], &databuf[4], &databuf[5]) == 6) {
		if (databuf[2]) {
			aw_dev_info("%s: cnt=%d, cmd=%d, play=%d\n",
				__func__, databuf[0], databuf[1], databuf[2]);
			aw_dev_info("%s: wavseq=%d, loop=%d, gain=%d\n",
				__func__, databuf[3], databuf[4], databuf[5]);
			aw8622x_audio_config(aw8622x, databuf);

		}
	}
	return count;
}

static ssize_t aw8622x_ram_num_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	aw8622x_haptic_get_ram_number(aw8622x);
	len += snprintf(buf+len, PAGE_SIZE-len,
			"ram_num = %d\n", aw8622x->ram.ram_num);
	return len;
}

static ssize_t aw8622x_awrw_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);
	unsigned char i = 0;
	unsigned char reg_num = aw8622x->aw_i2c_package.reg_num;
	unsigned char flag = aw8622x->aw_i2c_package.flag;
	unsigned char *reg_data = aw8622x->aw_i2c_package.reg_data;
	ssize_t len = 0;

	if (!reg_num) {
		aw_dev_err("%s: awrw parameter error\n",
			   __func__);
		return len;
	}
	if (flag == AW8622X_READ) {
		for (i = 0; i < reg_num; i++) {
			len += snprintf(buf + len, PAGE_SIZE - len,
					"0x%02x,", reg_data[i]);
		}

		len += snprintf(buf + len - 1, PAGE_SIZE - len, "\n");
	}

	return len;
}

static ssize_t aw8622x_awrw_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	unsigned int datatype[3] = { 0 };
	int ret = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw8622x *aw8622x = container_of(cdev, struct aw8622x, vib_dev);

	if (sscanf(buf, "%x %x %x", &datatype[0], &datatype[1],
				       &datatype[2]) == 3) {
		if (!datatype[1]) {
			aw_dev_err("%s: awrw parameter error\n",
				   __func__);
			return count;
		}
		aw8622x->aw_i2c_package.flag = (unsigned char)datatype[0];
		aw8622x->aw_i2c_package.reg_num = (unsigned char)datatype[1];
		aw8622x->aw_i2c_package.first_addr = (unsigned char)datatype[2];
		if (aw8622x->aw_i2c_package.flag == AW8622X_WRITE) {
			ret = aw8622x_parse_data(aw8622x, buf);
			if (ret < 0)
				return count;
			aw8622x_i2c_write(aw8622x,
					 aw8622x->aw_i2c_package.first_addr,
					 aw8622x->aw_i2c_package.reg_data,
					 aw8622x->aw_i2c_package.reg_num);
		}
		if (aw8622x->aw_i2c_package.flag == AW8622X_READ)
			aw8622x_i2c_read(aw8622x,
					aw8622x->aw_i2c_package.first_addr,
					aw8622x->aw_i2c_package.reg_data,
					aw8622x->aw_i2c_package.reg_num);
	} else
		aw_dev_err("%s: missing number of parameters\n",
			   __func__);

	return count;
}

static DEVICE_ATTR(f0, 0644, aw8622x_f0_show, aw8622x_f0_store);
static DEVICE_ATTR(cont, 0644, aw8622x_cont_show, aw8622x_cont_store);
static DEVICE_ATTR(register, 0644, aw8622x_reg_show, aw8622x_reg_store);
static DEVICE_ATTR(duration, 0644, aw8622x_duration_show,
		   aw8622x_duration_store);
static DEVICE_ATTR(index, 0644, aw8622x_index_show, aw8622x_index_store);
static DEVICE_ATTR(activate, 0644, aw8622x_activate_show,
		   aw8622x_activate_store);
static DEVICE_ATTR(activate_mode, 0644, aw8622x_activate_mode_show,
		   aw8622x_activate_mode_store);
static DEVICE_ATTR(seq, 0644, aw8622x_seq_show, aw8622x_seq_store);
static DEVICE_ATTR(loop, 0644, aw8622x_loop_show, aw8622x_loop_store);
static DEVICE_ATTR(rtp, 0644, aw8622x_rtp_show, aw8622x_rtp_store);
static DEVICE_ATTR(state, 0644, aw8622x_state_show, aw8622x_state_store);
static DEVICE_ATTR(sram_size, 0644, aw8622x_sram_size_show,
		   aw8622x_sram_size_store);
static DEVICE_ATTR(osc_cali, 0644, aw8622x_osc_cali_show,
		   aw8622x_osc_cali_store);
static DEVICE_ATTR(gain, 0644, aw8622x_gain_show, aw8622x_gain_store);
static DEVICE_ATTR(ram_update, 0644, aw8622x_ram_update_show,
		   aw8622x_ram_update_store);
static DEVICE_ATTR(f0_save, 0644, aw8622x_f0_save_show, aw8622x_f0_save_store);
static DEVICE_ATTR(osc_save, 0644, aw8622x_osc_save_show,
		   aw8622x_osc_save_store);
static DEVICE_ATTR(trig, 0644, aw8622x_trig_show, aw8622x_trig_store);
static DEVICE_ATTR(ram_vbat_comp, 0644, aw8622x_ram_vbat_compensate_show,
		   aw8622x_ram_vbat_compensate_store);
static DEVICE_ATTR(cali, 0644, aw8622x_cali_show, aw8622x_cali_store);
static DEVICE_ATTR(cont_wait_num, 0644, aw8622x_cont_wait_num_show,
		   aw8622x_cont_wait_num_store);
static DEVICE_ATTR(cont_drv_lvl, 0644, aw8622x_cont_drv_lvl_show,
		   aw8622x_cont_drv_lvl_store);
static DEVICE_ATTR(cont_drv_time, 0644, aw8622x_cont_drv_time_show,
		   aw8622x_cont_drv_time_store);
static DEVICE_ATTR(cont_brk_time, 0644, aw8622x_cont_brk_time_show,
		   aw8622x_cont_brk_time_store);
static DEVICE_ATTR(vbat_monitor, 0644, aw8622x_vbat_monitor_show,
		   aw8622x_vbat_monitor_store);
static DEVICE_ATTR(lra_resistance, 0644, aw8622x_lra_resistance_show,
		   aw8622x_lra_resistance_store);
static DEVICE_ATTR(prctmode, 0644, aw8622x_prctmode_show,
		   aw8622x_prctmode_store);
static DEVICE_ATTR(gun_type, 0644, aw8622x_gun_type_show,
		   aw8622x_gun_type_store);
static DEVICE_ATTR(bullet_nr, 0644, aw8622x_bullet_nr_show,
		   aw8622x_bullet_nr_store);
static DEVICE_ATTR(haptic_audio, 0644, aw8622x_haptic_audio_show,
		   aw8622x_haptic_audio_store);
static DEVICE_ATTR(ram_num, 0644, aw8622x_ram_num_show, NULL);
static DEVICE_ATTR(awrw, 0644, aw8622x_awrw_show, aw8622x_awrw_store);
static struct attribute *aw8622x_vibrator_attributes[] = {
	&dev_attr_state.attr,
	&dev_attr_duration.attr,
	&dev_attr_activate.attr,
	&dev_attr_activate_mode.attr,
	&dev_attr_index.attr,
	&dev_attr_gain.attr,
	&dev_attr_seq.attr,
	&dev_attr_loop.attr,
	&dev_attr_register.attr,
	&dev_attr_rtp.attr,
	&dev_attr_ram_update.attr,
	&dev_attr_f0.attr,
	&dev_attr_cali.attr,
	&dev_attr_f0_save.attr,
	&dev_attr_osc_save.attr,
	&dev_attr_cont.attr,
	&dev_attr_cont_wait_num.attr,
	&dev_attr_cont_drv_lvl.attr,
	&dev_attr_cont_drv_time.attr,
	&dev_attr_cont_brk_time.attr,
	&dev_attr_vbat_monitor.attr,
	&dev_attr_lra_resistance.attr,
	&dev_attr_prctmode.attr,
	&dev_attr_sram_size.attr,
	&dev_attr_ram_vbat_comp.attr,
	&dev_attr_osc_cali.attr,
	&dev_attr_gun_type.attr,
	&dev_attr_bullet_nr.attr,
	&dev_attr_haptic_audio.attr,
	&dev_attr_trig.attr,
	&dev_attr_ram_num.attr,
	&dev_attr_awrw.attr,
	NULL
};

struct attribute_group aw8622x_vibrator_attribute_group_l = {
	.attrs = aw8622x_vibrator_attributes
};

static void aw8622x_haptic_ram_play(struct aw8622x *aw8622x, unsigned char type)
{
	aw_dev_info("%s index = %d\n", __func__, aw8622x->index);

	mutex_lock(&aw8622x->ram_lock);
	if (type == AW8622X_HAPTIC_RAM_LOOP_MODE)
		aw8622x_haptic_set_repeat_wav_seq(aw8622x, aw8622x->index);

	if (type == AW8622X_HAPTIC_RAM_MODE) {
		aw8622x_haptic_set_wav_seq(aw8622x, 0x00, aw8622x->index);
		aw8622x_haptic_set_wav_seq(aw8622x, 0x01, 0x00);
		aw8622x_haptic_set_wav_loop(aw8622x, 0x00, 0x00);
	}
	aw8622x_haptic_play_mode(aw8622x, type);
	aw8622x_haptic_start(aw8622x);
	mutex_unlock(&aw8622x->ram_lock);
}

static void aw8622x_vibrator_work_routine(struct work_struct *work)
{
	struct aw8622x *aw8622x = container_of(work, struct aw8622x,
					       vibrator_work);

	//aw_dev_info("%s enter\n", __func__);
	uint8_t pwmcfg2_val = 0;

	mutex_lock(&aw8622x->lock);
	/* Enter standby mode */
	aw8622x_haptic_stop_l(aw8622x);
	aw8622x_haptic_upload_lra(aw8622x, AW8622X_F0_CALI);
    aw8622x_i2c_read(aw8622x, 0x4d, &pwmcfg2_val, 1);
	aw_dev_info("%s enter pwmcfg2:0x%02X\n", __func__, pwmcfg2_val);
	if (aw8622x->state) {
		if (aw8622x->activate_mode == AW8622X_HAPTIC_RAM_MODE) {
			aw8622x_haptic_ram_vbat_comp(aw8622x, false);
			aw8622x_haptic_ram_play(aw8622x,
						AW8622X_HAPTIC_RAM_MODE);
		} else if (aw8622x->activate_mode ==
					AW8622X_HAPTIC_RAM_LOOP_MODE) {
			aw8622x_haptic_ram_vbat_comp(aw8622x, true);
			aw8622x_haptic_ram_play(aw8622x,
					       AW8622X_HAPTIC_RAM_LOOP_MODE);
			/* run ms timer */
			hrtimer_start(&aw8622x->timer,
				      ktime_set(aw8622x->duration / 1000,
				      (aw8622x->duration % 1000) * 1000000),
				      HRTIMER_MODE_REL);
		} else if (aw8622x->activate_mode ==
					AW8622X_HAPTIC_CONT_MODE) {
			aw8622x_haptic_cont(aw8622x);
			/* run ms timer */
			hrtimer_start(&aw8622x->timer,
				      ktime_set(aw8622x->duration / 1000,
				      (aw8622x->duration % 1000) * 1000000),
				      HRTIMER_MODE_REL);
		} else {
			 aw_dev_err("%s: activate_mode error\n",
				   __func__);
		}
	}
	mutex_unlock(&aw8622x->lock);
}

int aw8622x_vibrator_init_l(struct aw8622x *aw8622x)
{
	int ret = 0;

	aw_dev_info("%s enter\n", __func__);

#ifdef TIMED_OUTPUT
	aw_dev_info("%s: TIMED_OUT FRAMEWORK!\n", __func__);
	aw8622x->vib_dev.name = "awinic_vibrator_l";
	aw8622x->vib_dev.get_time = aw8622x_vibrator_get_time;
	aw8622x->vib_dev.enable = aw8622x_vibrator_enable;

	ret = timed_output_dev_register(&(aw8622x->vib_dev));
	if (ret < 0) {
		aw_dev_err("%s: fail to create timed output dev\n", __func__);
		return ret;
	}
	ret = sysfs_create_group(&aw8622x->vib_dev.dev->kobj,
				 &aw8622x_vibrator_attribute_group_l);
	if (ret < 0) {
		aw_dev_err("%s error creating sysfs attr files\n", __func__);
		return ret;
	}
#else
	aw_dev_info("%s: loaded in leds_cdev framework!\n",
		    __func__);
	aw8622x->vib_dev.name = "awinic_vibrator_l";
	aw8622x->vib_dev.brightness_get = aw8622x_haptic_brightness_get;
	aw8622x->vib_dev.brightness_set = aw8622x_haptic_brightness_set;

	ret = devm_led_classdev_register(&aw8622x->i2c->dev, &aw8622x->vib_dev);
	if (ret < 0) {
		aw_dev_err("%s: fail to create led dev\n", __func__);
		return ret;
	}
	ret = sysfs_create_group(&aw8622x->vib_dev.dev->kobj,
				 &aw8622x_vibrator_attribute_group_l);
	if (ret < 0) {
		aw_dev_err("%s error creating sysfs attr files\n",
			   __func__);
		return ret;
	}
#endif
	hrtimer_init(&aw8622x->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	aw8622x->timer.function = aw8622x_vibrator_timer_func;
	INIT_WORK(&aw8622x->vibrator_work,
		  aw8622x_vibrator_work_routine);
	INIT_WORK(&aw8622x->rtp_work, aw8622x_rtp_work_routine);
	mutex_init(&aw8622x->lock);
	mutex_init(&aw8622x->rtp_lock);
	mutex_init(&aw8622x->ram_lock);

	return 0;
}

static int aw8622x_haptic_set_pwm(struct aw8622x *aw8622x, unsigned char mode)
{
	switch (mode) {
	case AW8622X_PWM_48K:
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL2,
				       AW8622X_BIT_SYSCTRL2_WAVDAT_MODE_MASK,
				       AW8622X_BIT_SYSCTRL2_RATE_48K);
		break;
	case AW8622X_PWM_24K:
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL2,
				       AW8622X_BIT_SYSCTRL2_WAVDAT_MODE_MASK,
				       AW8622X_BIT_SYSCTRL2_RATE_24K);
		break;
	case AW8622X_PWM_12K:
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL2,
				       AW8622X_BIT_SYSCTRL2_WAVDAT_MODE_MASK,
				       AW8622X_BIT_SYSCTRL2_RATE_12K);
		break;
	default:
		break;
	}
	return 0;
}

static void aw8622x_haptic_misc_para_init(struct aw8622x *aw8622x)
{
	unsigned char reg_array[4] = {0};
	unsigned char reg_val = 0;

	aw_dev_info("%s enter\n", __func__);
	aw8622x->cont_drv1_lvl = aw8622x->dts_info.cont_drv1_lvl_dt;
	aw8622x->cont_drv2_lvl = aw8622x->dts_info.cont_drv2_lvl_dt;
	aw8622x->cont_drv1_time = aw8622x->dts_info.cont_drv1_time_dt;
	aw8622x->cont_drv2_time = aw8622x->dts_info.cont_drv2_time_dt;
	aw8622x->cont_brk_time = aw8622x->dts_info.cont_brk_time_dt;
	aw8622x->cont_wait_num = aw8622x->dts_info.cont_wait_num_dt;

	/* SIN_H */
	reg_array[0] = (unsigned char)aw8622x->dts_info.sine_array[0];
	/* SIN_L */
	reg_array[1] = (unsigned char)aw8622x->dts_info.sine_array[1];
	/* COS_H */
	reg_array[2] = (unsigned char)aw8622x->dts_info.sine_array[2];
	/* COS_L */
	reg_array[3] = (unsigned char)aw8622x->dts_info.sine_array[3];

	aw8622x_i2c_write(aw8622x, AW8622X_REG_SYSCTRL3, reg_array,
			  AW_I2C_BYTE_FOUR);

	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_TRGCFG8,
			       AW8622X_BIT_TRGCFG8_TRG_TRIG1_MODE_MASK,
			       AW8622X_BIT_TRGCFG8_TRIG1);
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_ANACFG8,
			       AW8622X_BIT_ANACFG8_TRTF_CTRL_HDRV_MASK,
			       AW8622X_BIT_ANACFG8_TRTF_CTRL_HDRV);

	/* d2s_gain */
	if (!aw8622x->dts_info.d2s_gain) {
		aw_dev_err("%s aw8622x->dts_info.d2s_gain = 0!\n",
			   __func__);
	} else {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL7,
				       AW8622X_BIT_SYSCTRL7_D2S_GAIN_MASK,
				       aw8622x->dts_info.d2s_gain);
	}

	if (aw8622x->dts_info.cont_tset && aw8622x->dts_info.cont_bemf_set) {
		reg_array[0] = (unsigned char)aw8622x->dts_info.cont_tset;
		reg_array[1] = (unsigned char)aw8622x->dts_info.cont_bemf_set;
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_CONTCFG13,
				       (AW8622X_BIT_CONTCFG13_TSET_MASK &
					AW8622X_BIT_CONTCFG13_BEME_SET_MASK),
				       ((reg_array[0] << 4)|
					reg_array[1]));
	}

	/* cont_brk_time */
	if (!aw8622x->cont_brk_time) {
		aw_dev_err("%s aw8622x->cont_brk_time = 0!\n",
			   __func__);
	} else {
		reg_val = (unsigned char)aw8622x->cont_brk_time;
		aw8622x_i2c_write(aw8622x, AW8622X_REG_CONTCFG10, &reg_val,
				  AW_I2C_BYTE_ONE);
	}

	/* cont_bst_brk_gain */
	/*
	** if (!aw8622x->dts_info.cont_bst_brk_gain) {
	**	aw_dev_err("%s aw8622x->dts_info.cont_bst_brk_gain = 0!\n",
	**		   __func__);
	** } else {
	**	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_CONTCFG5,
	**			       AW8622X_BIT_CONTCFG5_BST_BRK_GAIN_MASK,
	**			       aw8622x->dts_info.cont_bst_brk_gain);
	** }
	*/

	/* cont_brk_gain */
	if (!aw8622x->dts_info.cont_brk_gain) {
		aw_dev_err("%s aw8622x->dts_info.cont_brk_gain = 0!\n",
			   __func__);
	} else {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_CONTCFG5,
				       AW8622X_BIT_CONTCFG5_BRK_GAIN_MASK,
				       aw8622x->dts_info.cont_brk_gain);
	}
}

/*****************************************************
 *
 * offset calibration
 *
 *****************************************************/
static int aw8622x_haptic_offset_calibration(struct aw8622x *aw8622x)
{
	unsigned int cont = 2000;
	unsigned char reg_val = 0;

	aw_dev_info("%s enter\n", __func__);

	aw8622x_haptic_raminit(aw8622x, true);

	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_DETCFG2,
			       AW8622X_BIT_DETCFG2_DIAG_GO_MASK,
			       AW8622X_BIT_DETCFG2_DIAG_GO_ON);
	while (1) {
		aw8622x_i2c_read(aw8622x, AW8622X_REG_DETCFG2, &reg_val,
				 AW_I2C_BYTE_ONE);
		if ((reg_val & 0x01) == 0 || cont == 0)
			break;
		cont--;
	}
	if (cont == 0)
		aw_dev_err("%s calibration offset failed!\n",
			   __func__);
	aw8622x_haptic_raminit(aw8622x, false);
	return 0;
}

/*****************************************************
 *
 * vbat mode
 *
 *****************************************************/
static int aw8622x_haptic_vbat_mode_config(struct aw8622x *aw8622x,
					   unsigned char flag)
{
	if (flag == AW8622X_HAPTIC_CONT_VBAT_HW_ADJUST_MODE) {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL1,
				       AW8622X_BIT_SYSCTRL1_VBAT_MODE_MASK,
				       AW8622X_BIT_SYSCTRL1_VBAT_MODE_HW);
	} else {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL1,
				       AW8622X_BIT_SYSCTRL1_VBAT_MODE_MASK,
				       AW8622X_BIT_SYSCTRL1_VBAT_MODE_SW);
	}
	return 0;
}

static void aw8622x_ram_work_routine(struct work_struct *work)
{
	struct aw8622x *aw8622x = container_of(work, struct aw8622x,
						ram_work.work);

	aw_dev_info("%s enter\n", __func__);
	aw8622x_ram_update(aw8622x);
}

int aw8622x_ram_work_init_l(struct aw8622x *aw8622x)
{
	int ram_timer_val = 8000;

	aw_dev_info("%s enter\n", __func__);
	INIT_DELAYED_WORK(&aw8622x->ram_work, aw8622x_ram_work_routine);
	schedule_delayed_work(&aw8622x->ram_work,
				msecs_to_jiffies(ram_timer_val));
	return 0;
}
static enum hrtimer_restart
aw8622x_haptic_audio_timer_func(struct hrtimer *timer)
{
	struct aw8622x *aw8622x = container_of(timer,
					struct aw8622x, haptic_audio.timer);

	aw_dev_dbg("%s enter\n", __func__);
	schedule_work(&aw8622x->haptic_audio.work);

	hrtimer_start(&aw8622x->haptic_audio.timer,
		ktime_set(aw8622x->haptic_audio.timer_val/1000000,
			(aw8622x->haptic_audio.timer_val%1000000)*1000),
		HRTIMER_MODE_REL);
	return HRTIMER_NORESTART;
}

static void
aw8622x_haptic_auto_brk_enable(struct aw8622x *aw8622x, unsigned char flag)
{
	if (flag) {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_PLAYCFG3,
				AW8622X_BIT_PLAYCFG3_BRK_EN_MASK,
				AW8622X_BIT_PLAYCFG3_BRK_ENABLE);
	} else {
		aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_PLAYCFG3,
				AW8622X_BIT_PLAYCFG3_BRK_EN_MASK,
				AW8622X_BIT_PLAYCFG3_BRK_DISABLE);
	}
}
int aw8622x_haptic_init_l(struct aw8622x *aw8622x)
{
	unsigned char reg_val = 0;
	unsigned char reg_array[8] = {0};
	int ret = 0;

	aw_dev_info("%s enter\n", __func__);
	/* haptic audio */
	aw8622x->haptic_audio.delay_val = 1;
	aw8622x->haptic_audio.timer_val = 21318;
	INIT_LIST_HEAD(&(aw8622x->haptic_audio.ctr_list));
	hrtimer_init(&aw8622x->haptic_audio.timer,
		CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	aw8622x->haptic_audio.timer.function = aw8622x_haptic_audio_timer_func;
	INIT_WORK(&aw8622x->haptic_audio.work,
		aw8622x_haptic_audio_work_routine);
	mutex_init(&aw8622x->haptic_audio.lock);
	aw8622x->gun_type = AW_GUN_TYPE_DEF_VAL;
	aw8622x->bullet_nr = AW_BULLET_NR_DEF_VAL;

	mutex_lock(&aw8622x->lock);
	/* haptic init */
	aw8622x->ram_state = 0;
	aw8622x->activate_mode = aw8622x->dts_info.mode;
	ret = aw8622x_i2c_read(aw8622x, AW8622X_REG_WAVCFG1, &reg_val,
			       AW_I2C_BYTE_ONE);
	aw8622x->index = reg_val & AW8622X_BIT_WAVCFG_SEQ;
	ret = aw8622x_i2c_read(aw8622x, AW8622X_REG_PLAYCFG2, &reg_val,
			       AW_I2C_BYTE_ONE);
	aw8622x->gain = reg_val;
	aw_dev_info("%s aw8622x->gain =0x%02X\n", __func__, aw8622x->gain);
	aw8622x_i2c_read(aw8622x, AW8622X_REG_WAVCFG1, reg_array,
			AW8622X_SEQUENCER_SIZE);
	memcpy(aw8622x->seq, reg_array, AW8622X_SEQUENCER_SIZE);

	aw8622x_haptic_play_mode(aw8622x, AW8622X_HAPTIC_STANDBY_MODE);
	aw8622x_haptic_set_pwm(aw8622x, AW8622X_PWM_12K);
	/* misc value init */
	aw8622x_haptic_misc_para_init(aw8622x);
	/* set motor protect */
	aw8622x_haptic_swicth_motor_protect_config(aw8622x, 0x00, 0x00);
	aw8622x_haptic_trig_param_init(aw8622x);
	aw8622x_haptic_trig_param_config(aw8622x);
	aw8622x_haptic_offset_calibration(aw8622x);
	/*config auto_brake*/
	aw8622x_haptic_auto_brk_enable(aw8622x,
				       aw8622x->dts_info.is_enabled_auto_brk);
	/* vbat compensation */
	aw8622x_haptic_vbat_mode_config(aw8622x,
				AW8622X_HAPTIC_CONT_VBAT_HW_ADJUST_MODE);
	aw8622x->ram_vbat_compensate = AW8622X_HAPTIC_RAM_VBAT_COMP_ENABLE;

	/* f0 calibration */
	/*LRA trim source select register*/
	aw8622x_i2c_write_bits(aw8622x,
				AW8622X_REG_TRIMCFG1,
				AW8622X_BIT_TRIMCFG1_RL_TRIM_SRC_MASK,
				AW8622X_BIT_TRIMCFG1_RL_TRIM_SRC_REG);
	if (aw8622x->dts_info.lk_f0_cali) {
		aw8622x->f0_cali_data = aw8622x->dts_info.lk_f0_cali;
		aw8622x_haptic_upload_lra(aw8622x, AW8622X_F0_CALI);
	} else {
		aw8622x_haptic_upload_lra(aw8622x, AW8622X_WRITE_ZERO);
		aw8622x_haptic_f0_calibration(aw8622x);
	}
	mutex_unlock(&aw8622x->lock);
	return ret;
}

void aw8622x_interrupt_setup_l(struct aw8622x *aw8622x)
{
	unsigned char reg_val = 0;

	aw_dev_info("%s enter\n", __func__);

	aw8622x_i2c_read(aw8622x, AW8622X_REG_SYSINT, &reg_val,
			 AW_I2C_BYTE_ONE);

	aw_dev_info("%s: reg SYSINT=0x%02X\n", __func__, reg_val);

	/* edge int mode */
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSCTRL7,
			       (AW8622X_BIT_SYSCTRL7_INT_MODE_MASK &
				AW8622X_BIT_SYSCTRL7_INT_EDGE_MODE_MASK),
			       (AW8622X_BIT_SYSCTRL7_INT_MODE_EDGE |
				AW8622X_BIT_SYSCTRL7_INT_EDGE_MODE_POS));
	/* int enable */
	/*
	*aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSINTM,
	*			AW8622X_BIT_SYSINTM_BST_SCPM_MASK,
	*			AW8622X_BIT_SYSINTM_BST_SCPM_OFF);
	*aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSINTM,
	*			AW8622X_BIT_SYSINTM_BST_OVPM_MASK,
	*		AW8622X_BIT_SYSINTM_BST_OVPM_ON);
	*/
	aw8622x_i2c_write_bits(aw8622x, AW8622X_REG_SYSINTM,
			       (AW8622X_BIT_SYSINTM_UVLM_MASK &
				AW8622X_BIT_SYSINTM_OCDM_MASK &
				AW8622X_BIT_SYSINTM_OTM_MASK),
			       (AW8622X_BIT_SYSINTM_UVLM_ON |
				AW8622X_BIT_SYSINTM_OCDM_ON |
				AW8622X_BIT_SYSINTM_OTM_ON));
}
static int aw8622x_is_rtp_load_end(struct aw8622x *aw8622x)
{
	unsigned char glb_st = 0;
	int ret = -1;

	aw8622x_i2c_read(aw8622x, AW8622X_REG_GLBRD5,
			 &glb_st, AW_I2C_BYTE_ONE);
	if ((aw8622x->rtp_cnt == aw8622x->rtp_container->len)
	    || ((glb_st & 0x0f) == 0)) {
		if (aw8622x->rtp_cnt ==
			aw8622x->rtp_container->len)
			aw_dev_info("%s: rtp load completely!\n",
				    __func__);
		else
			aw_dev_err("%s rtp load failed!!\n",
				   __func__);
		aw8622x_haptic_set_rtp_aei(aw8622x,
					false);
		aw8622x->rtp_cnt = 0;
		aw8622x->rtp_init = 0;
		ret = 0;
	}
	return ret;

}

static int aw8622x_write_rtp_data(struct aw8622x *aw8622x)
{
	unsigned int buf_len = 0;
	int ret = -1;

	if (!aw8622x->rtp_cnt) {
		aw_dev_info("%s:aw8622x->rtp_cnt is 0!\n",
			__func__);
		return ret;
	}
	if (!aw8622x->rtp_container) {
		aw_dev_info("%s:aw8622x->rtp_container is null, break!\n",
			__func__);
		return ret;
	}
	if ((aw8622x->rtp_container->len - aw8622x->rtp_cnt) <
	    (aw8622x->ram.base_addr >> 1)) {
		buf_len =
		    aw8622x->rtp_container->len - aw8622x->rtp_cnt;
	} else {
		buf_len = (aw8622x->ram.base_addr >> 2);
	}
	aw8622x_i2c_write(aw8622x, AW8622X_REG_RTPDATA,
			   &aw8622x->rtp_container->data[aw8622x->rtp_cnt],
			   buf_len);
	aw8622x->rtp_cnt += buf_len;
	return 0;
}

irqreturn_t aw8622x_irq_l(int irq, void *data)
{
	struct aw8622x *aw8622x = data;
	unsigned char reg_val = 0;
	int ret = 0;

	aw_dev_dbg("%s enter\n", __func__);
	aw8622x_i2c_read(aw8622x, AW8622X_REG_SYSINT, &reg_val,
			 AW_I2C_BYTE_ONE);
	aw_dev_dbg("%s: reg SYSINT=0x%02X\n", __func__, reg_val);
	if (reg_val & AW8622X_BIT_SYSINT_UVLI)
		aw_dev_err("%s chip uvlo int error\n", __func__);
	if (reg_val & AW8622X_BIT_SYSINT_OCDI)
		aw_dev_err("%s chip over current int error\n", __func__);
	if (reg_val & AW8622X_BIT_SYSINT_OTI)
		aw_dev_err("%s chip over temperature int error\n", __func__);
	if (reg_val & AW8622X_BIT_SYSINT_DONEI)
		aw_dev_info("%s chip playback done\n", __func__);

	if (reg_val & AW8622X_BIT_SYSINT_FF_AEI) {
		aw_dev_dbg("%s: aw8622x rtp fifo almost empty\n", __func__);
		if (aw8622x->rtp_init) {
			while ((!aw8622x_haptic_rtp_get_fifo_afs(aw8622x)) &&
			       (aw8622x->play_mode ==
				AW8622X_HAPTIC_RTP_MODE)) {
				mutex_lock(&aw8622x->rtp_lock);
#ifdef AW_ENABLE_RTP_PRINT_LOG
				aw_dev_info(	"%s: aw8622x rtp mode fifo update, cnt=%d\n",
					__func__, aw8622x->rtp_cnt);
#endif
				ret = aw8622x_write_rtp_data(aw8622x);
				if (ret < 0) {
					mutex_unlock(&aw8622x->rtp_lock);
					break;
				}
				ret = aw8622x_is_rtp_load_end(aw8622x);
				if (!ret) {
					mutex_unlock(&aw8622x->rtp_lock);
					break;
				}
				mutex_unlock(&aw8622x->rtp_lock);
			}
		} else {
			aw_dev_info("%s: aw8622x rtp init = %d, init error\n",
				    __func__, aw8622x->rtp_init);
		}
	}

	if (reg_val & AW8622X_BIT_SYSINT_FF_AFI)
		aw_dev_dbg("%s: aw8622x rtp mode fifo almost full!\n",
			    __func__);

	if (aw8622x->play_mode != AW8622X_HAPTIC_RTP_MODE)
		aw8622x_haptic_set_rtp_aei(aw8622x, false);

	aw_dev_dbg("%s exit\n", __func__);

	return IRQ_HANDLED;
}

char aw8622x_check_qualify_l(struct aw8622x *aw8622x)
{
	unsigned char reg = 0;
	int ret = 0;

	ret = aw8622x_i2c_read(aw8622x, AW8622X_REG_EFRD9, &reg,
			       AW_I2C_BYTE_ONE);
	if (ret < 0) {
		aw_dev_err("%s: failed to read register 0x64: %d\n",
			   __func__, ret);
		return ret;
	}
	if ((reg & 0x80) == 0x80)
		return 1;
	aw_dev_err("%s: register 0x64 error: 0x%02x\n",
			__func__, reg);
	return 0;
}
