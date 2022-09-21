/*
 * aw86214.c
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

#include "aw86214.h"
#include "haptic_nv.h"

/******************************************************
 *
 * Register Access
 *
 ******************************************************/
static const unsigned char aw86214_reg_list[] = {
	AW86214_REG_ID,
	AW86214_REG_SYSST,
	AW86214_REG_SYSINT,
	AW86214_REG_SYSINTM,
	AW86214_REG_SYSST2,
	AW86214_REG_PLAYCFG2,
	AW86214_REG_PLAYCFG3,
	AW86214_REG_PLAYCFG4,
	AW86214_REG_WAVCFG1,
	AW86214_REG_WAVCFG2,
	AW86214_REG_WAVCFG3,
	AW86214_REG_WAVCFG4,
	AW86214_REG_WAVCFG5,
	AW86214_REG_WAVCFG6,
	AW86214_REG_WAVCFG7,
	AW86214_REG_WAVCFG8,
	AW86214_REG_WAVCFG9,
	AW86214_REG_WAVCFG10,
	AW86214_REG_WAVCFG11,
	AW86214_REG_WAVCFG12,
	AW86214_REG_WAVCFG13,
	AW86214_REG_CONTCFG1,
	AW86214_REG_CONTCFG2,
	AW86214_REG_CONTCFG3,
	AW86214_REG_CONTCFG4,
	AW86214_REG_CONTCFG5,
	AW86214_REG_CONTCFG6,
	AW86214_REG_CONTCFG7,
	AW86214_REG_CONTCFG8,
	AW86214_REG_CONTCFG9,
	AW86214_REG_CONTCFG10,
	AW86214_REG_CONTCFG11,
	AW86214_REG_CONTCFG13,
	AW86214_REG_CONTRD14,
	AW86214_REG_CONTRD15,
	AW86214_REG_CONTRD16,
	AW86214_REG_CONTRD17,
	AW86214_REG_RTPCFG1,
	AW86214_REG_RTPCFG2,
	AW86214_REG_RTPCFG3,
	AW86214_REG_RTPCFG4,
	AW86214_REG_RTPCFG5,
	AW86214_REG_TRGCFG1,
	AW86214_REG_TRGCFG4,
	AW86214_REG_TRGCFG7,
	AW86214_REG_TRGCFG8,
	AW86214_REG_GLBCFG4,
	AW86214_REG_GLBRD5,
	AW86214_REG_RAMADDRH,
	AW86214_REG_RAMADDRL,
	AW86214_REG_SYSCTRL1,
	AW86214_REG_SYSCTRL2,
	AW86214_REG_SYSCTRL3,
	AW86214_REG_SYSCTRL4,
	AW86214_REG_SYSCTRL5,
	AW86214_REG_SYSCTRL6,
	AW86214_REG_SYSCTRL7,
	AW86214_REG_PWMCFG1,
	AW86214_REG_PWMCFG3,
	AW86214_REG_PWMCFG4,
	AW86214_REG_DETCFG1,
	AW86214_REG_DETCFG2,
	AW86214_REG_DET_RL,
	AW86214_REG_DET_VBAT,
	AW86214_REG_DET_LO,
	AW86214_REG_TRIMCFG1,
	AW86214_REG_TRIMCFG3,
	AW86214_REG_ANACFG8,
};

/******************************************************
 *
 * value
 *
 ******************************************************/
static char *aw86214_ram_name = "aw86214l_haptic.bin";

/******************************************************
 *
 * functions declaration
 *
 ******************************************************/
static int aw86214_analyse_duration_range(struct aw86214 *aw86214);

 /******************************************************
 *
 * aw86214 i2c write/read
 *
 ******************************************************/
static int aw86214_i2c_read(struct aw86214 *aw86214, unsigned char reg_addr,
			   unsigned char *buf, unsigned int len)
{
	int ret = 0;

	struct i2c_msg msg[2] = {
		[0] = {
				.addr = aw86214->i2c->addr,
				.flags = 0,
				.len = sizeof(uint8_t),
				.buf = &reg_addr,
				},
		[1] = {
				.addr = aw86214->i2c->addr,
				.flags = I2C_M_RD,
				.len = len,
				.buf = buf,
				},
	};

	ret = i2c_transfer(aw86214->i2c->adapter, msg, ARRAY_SIZE(msg));
	if (ret < 0) {
		aw_dev_err("%s: i2c_transfer failed\n", __func__);
		return ret;
	} else if (ret != AW_I2C_READ_MSG_NUM) {
		aw_dev_err("%s: transfer failed(size error)\n", __func__);
		return -ENXIO;
	}
	return ret;
}

static int aw86214_i2c_write(struct aw86214 *aw86214, unsigned char reg_addr,
			    unsigned char *buf, unsigned int len)
{
	unsigned char *data = NULL;
	int ret = -1;

	data = kmalloc(len + 1, GFP_KERNEL);
	data[0] = reg_addr;
	memcpy(&data[1], buf, len);
	ret = i2c_master_send(aw86214->i2c, data, len + 1);
	if (ret < 0)
		aw_dev_err("%s: i2c master send 0x%02x error\n",
			   __func__, reg_addr);
	kfree(data);
	return ret;
}

static int
aw86214_i2c_write_bits(struct aw86214 *aw86214, unsigned char reg_addr,
		       unsigned int mask,
		       unsigned char reg_data)
{
	unsigned char reg_val = 0;
	int ret = -1;

	ret = aw86214_i2c_read(aw86214, reg_addr, &reg_val, AW_I2C_BYTE_ONE);
	if (ret < 0) {
		aw_dev_err("%s: i2c read error, ret=%d\n", __func__, ret);
		return ret;
	}
	reg_val &= mask;
	reg_val |= reg_data;
	ret = aw86214_i2c_write(aw86214, reg_addr, &reg_val, AW_I2C_BYTE_ONE);
	if (ret < 0) {
		aw_dev_err("%s: i2c write error, ret=%d\n", __func__, ret);
		return ret;
	}
	return 0;
}

static int aw86214_parse_data(struct aw86214 *aw86214, const char *buf)
{
	unsigned char reg_num = aw86214->aw_i2c_package.reg_num;
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
		aw86214->aw_i2c_package.reg_data[i] = (unsigned char)value;
	}
	return 0;
}

irqreturn_t aw86214_irq_l(int irq, void *data)
{
	struct aw86214 *aw86214 = data;
	unsigned char reg_val = 0;

	aw_dev_info("%s enter\n", __func__);
	aw86214_i2c_read(aw86214, AW86214_REG_SYSINT, &reg_val,
			 AW_I2C_BYTE_ONE);
	aw_dev_info("%s: reg SYSINT=0x%02X\n", __func__, reg_val);
	if (reg_val & AW86214_BIT_SYSINT_BST_OVPI)
		aw_dev_err("%s chip ov int error\n", __func__);
	if (reg_val & AW86214_BIT_SYSINT_UVLI)
		aw_dev_err("%s chip uvlo int error\n", __func__);
	if (reg_val & AW86214_BIT_SYSINT_OCDI)
		aw_dev_err("%s chip over current int error\n",
			   __func__);
	if (reg_val & AW86214_BIT_SYSINT_OTI)
		aw_dev_err("%s chip over temperature int error\n",
			   __func__);
	if (reg_val & AW86214_BIT_SYSINT_DONEI)
		aw_dev_info("%s chip playback done\n", __func__);

	aw_dev_info("%s exit\n", __func__);

	return IRQ_HANDLED;
}

static int aw86214_analyse_duration_range(struct aw86214 *aw86214)
{
	int i = 0;
	int ret = 0;
	int len = 0;
	int *duration_time = NULL;

	len = ARRAY_SIZE(aw86214->dts_info.duration_time);
	duration_time = aw86214->dts_info.duration_time;
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
aw86214_analyse_duration_array_size(struct aw86214 *aw86214,
				    struct device_node *np)
{
	int ret = 0;

	ret = of_property_count_elems_of_size(np, "aw86214_vib_duration_time",
					      4);
	if (ret < 0) {
		aw86214->duration_time_flag = -1;
		aw_dev_info("%s vib_duration_time not found\n", __func__);
		return ret;
	}
	aw86214->duration_time_size = ret;
	if (aw86214->duration_time_size > 3) {
		aw86214->duration_time_flag = -1;
		aw_dev_info("%s vib_duration_time error, array size = %d\n",
			    __func__, aw86214->duration_time_size);
		return -ERANGE;
	}
	return 0;
}

/*****************************************************
 *
 * device tree
 *
 *****************************************************/
int aw86214_parse_dt_l(struct aw86214 *aw86214, struct device *dev,
		     struct device_node *np)
{
	unsigned int val = 0;
	unsigned int prctmode_temp[3];
	unsigned int sine_array_temp[4] = { 0x05, 0xB2, 0xFF, 0xEF };
	unsigned int duration_time[3];
	int ret = 0;

	val = of_property_read_u32(np, "aw86214_vib_lk_f0_cali",
			&aw86214->dts_info.lk_f0_cali);
	if (val != 0)
		aw_dev_info("aw86214_vib_mode not found\n");
	aw_dev_info("%s: aw86214_vib_lk_f0_cali = 0x%02x\n",
		    __func__, aw86214->dts_info.lk_f0_cali);
	val = of_property_read_u32(np, "aw86214_vib_mode",
				   &aw86214->dts_info.mode);
	if (val != 0)
		aw_dev_info("%s vib_mode not found\n", __func__);
	val = of_property_read_u32(np, "aw86214_vib_f0_ref",
				   &aw86214->dts_info.f0_ref);
	if (val != 0)
		aw_dev_info("%s vib_f0_ref not found\n", __func__);
	val =
	    of_property_read_u32(np, "aw86214_vib_f0_cali_percent",
				 &aw86214->dts_info.f0_cali_percent);
	if (val != 0)
		aw_dev_info("%s vib_f0_cali_percent not found\n", __func__);
	val = of_property_read_u32(np, "aw86214_vib_cont_drv1_lvl",
				   &aw86214->dts_info.cont_drv1_lvl_dt);
	if (val != 0)
		aw_dev_info("%s vib_cont_drv1_lvl not found\n", __func__);
	val =
	    of_property_read_u32(np, "aw86214_vib_cont_drv2_lvl",
				 &aw86214->dts_info.cont_drv2_lvl_dt);
	if (val != 0)
		aw_dev_info("%s vib_cont_drv2_lvl not found\n", __func__);
	val =
	    of_property_read_u32(np, "aw86214_vib_cont_drv1_time",
				 &aw86214->dts_info.cont_drv1_time_dt);
	if (val != 0)
		aw_dev_info("%s vib_cont_drv1_time not found\n", __func__);
	val =
	    of_property_read_u32(np, "aw86214_vib_cont_drv2_time",
				 &aw86214->dts_info.cont_drv2_time_dt);
	if (val != 0)
		aw_dev_info("%s vib_cont_drv2_time not found\n", __func__);
	val =
	    of_property_read_u32(np, "aw86214_vib_cont_drv_width",
				 &aw86214->dts_info.cont_drv_width);
	if (val != 0)
		aw_dev_info("%s vib_cont_drv_width not found\n", __func__);
	val =
	    of_property_read_u32(np, "aw86214_vib_cont_brk_time",
				 &aw86214->dts_info.cont_brk_time_dt);
	if (val != 0)
		aw_dev_info("%s vib_cont_brk_time_dt not found\n", __func__);
	val =
	    of_property_read_u32(np, "aw86214_vib_cont_wait_num",
				 &aw86214->dts_info.cont_wait_num_dt);
	if (val != 0)
		aw_dev_info("%s vib_cont_wait_num not found\n", __func__);
	val =
	    of_property_read_u32(np, "aw86214_vib_cont_tset",
				 &aw86214->dts_info.cont_tset);
	if (val != 0)
		aw_dev_info("%s vib_cont_tset not found\n", __func__);
	val =
	    of_property_read_u32(np, "aw86214_vib_cont_bemf_set",
				 &aw86214->dts_info.cont_bemf_set);
	if (val != 0)
		aw_dev_info("%s vib_cont_bemf_set not found\n", __func__);
	val =
	    of_property_read_u32(np, "aw86214_vib_d2s_gain",
				 &aw86214->dts_info.d2s_gain);
	if (val != 0)
		aw_dev_info("%s vib_d2s_gain not found\n", __func__);
	val =
	    of_property_read_u32(np, "aw86214_vib_cont_track_margin",
				 &aw86214->dts_info.cont_track_margin);
	if (val != 0)
		aw_dev_info("%s vib_cont_track_margin not found\n", __func__);
	val = of_property_read_u32_array(np, "aw86214_vib_prctmode",
					 prctmode_temp,
					 ARRAY_SIZE(prctmode_temp));
	if (val != 0)
		aw_dev_info("%s vib_prctmode not found\n", __func__);
	memcpy(aw86214->dts_info.prctmode, prctmode_temp,
					sizeof(prctmode_temp));
	val =
	    of_property_read_u32_array(np,
				       "aw86214_vib_sine_array",
				       sine_array_temp,
				       ARRAY_SIZE(sine_array_temp));
	if (val != 0)
		aw_dev_info("%s vib_sine_array not found\n", __func__);
	memcpy(aw86214->dts_info.sine_array, sine_array_temp,
	       sizeof(sine_array_temp));
	val = of_property_read_u32_array(np, "aw86214_vib_duration_time",
		duration_time, ARRAY_SIZE(duration_time));
	if (val != 0)
		aw_dev_info("%s vib_duration_time not found\n", __func__);
	ret = aw86214_analyse_duration_array_size(aw86214, np);
	if (!ret)
		memcpy(aw86214->dts_info.duration_time,
			duration_time, sizeof(duration_time));
	return 0;
}

static void aw86214_haptic_upload_lra(struct aw86214 *aw86214,
				      unsigned int flag)
{
	switch (flag) {
	case AW86214_WRITE_ZERO:
		aw_dev_info("%s write zero to trim_lra!\n", __func__);
		aw86214_i2c_write_bits(aw86214, AW86214_REG_TRIMCFG3,
				       AW86214_BIT_TRIMCFG3_TRIM_LRA_MASK,
				       0x00);
		break;
	case AW86214_F0_CALI:
		aw_dev_info("%s write f0_cali_data to trim_lra = 0x%02X\n",
			    __func__, aw86214->f0_cali_data);
		aw86214_i2c_write_bits(aw86214, AW86214_REG_TRIMCFG3,
				       AW86214_BIT_TRIMCFG3_TRIM_LRA_MASK,
				       (char)aw86214->f0_cali_data);
		break;
	default:
		break;
	}
}

static void aw86214_brk_en(struct aw86214 *aw86214, bool isEnable)
{
	unsigned char reg_val = 0;

	if (isEnable == false)
		reg_val = 0x00;
	else
		reg_val = (unsigned char)aw86214->dts_info.cont_brk_time_dt;

	aw86214_i2c_write(aw86214, AW86214_REG_CONTCFG10, &reg_val,
			  AW_I2C_BYTE_ONE);

}

static void aw86214_ver_a_init(struct aw86214 *aw86214)
{
	unsigned char reg_val = 0;

	aw86214_i2c_write_bits(aw86214, AW86214_REG_RTPCFG1,
			       (AW86214_BIT_RTPCFG1_SRAM_SIZE_2K_MASK &
				AW86214_BIT_RTPCFG1_SRAM_SIZE_1K_MASK),
			       (AW86214_BIT_RTPCFG1_SRAM_SIZE_2K_DIS |
				AW86214_BIT_RTPCFG1_SRAM_SIZE_1K_EN));
	aw86214_i2c_write_bits(aw86214, AW86214_REG_PLAYCFG3,
				AW86214_BIT_PLAYCFG3_BRK_EN_MASK,
				AW86214_BIT_PLAYCFG3_BRK_ENABLE);
	reg_val = 0x00;
	aw86214_i2c_write(aw86214, AW86214_REG_CONTCFG10, &reg_val,
			  AW_I2C_BYTE_ONE);
}

int aw86214_haptic_stop_l(struct aw86214 *aw86214)
{
	unsigned char cnt = 40;
	unsigned char reg_val = 0;
	bool force_flag = true;

	aw_dev_info("%s enter\n", __func__);
	aw86214->play_mode = AW86214_HAPTIC_STANDBY_MODE;
	reg_val = AW86214_BIT_PLAYCFG4_STOP_ON;
	aw86214_i2c_write(aw86214, AW86214_REG_PLAYCFG4, &reg_val,
			  AW_I2C_BYTE_ONE);
	while (cnt) {
		aw86214_i2c_read(aw86214, AW86214_REG_GLBRD5, &reg_val,
				 AW_I2C_BYTE_ONE);
		if ((reg_val & 0x0f) == 0x00
		    || (reg_val & 0x0f) == 0x0A) {
			cnt = 0;
			force_flag = false;
			aw_dev_info("%s entered standby! glb_state=0x%02X\n",
				    __func__, reg_val);
		} else {
			cnt--;
			pr_debug
			    ("%s wait for standby, glb_state=0x%02X\n",
			     __func__, reg_val);
		}
		usleep_range(AW86214_STOP_DELAY_MIN, AW86214_STOP_DELAY_MAX);
	}
	if (force_flag) {
		aw_dev_err("%s force to enter standby mode!\n", __func__);
		aw86214_i2c_write_bits(aw86214, AW86214_REG_SYSCTRL2,
				       AW86214_BIT_SYSCTRL2_STANDBY_MASK,
				       AW86214_BIT_SYSCTRL2_STANDBY_ON);
	}
	return 0;
}

static void aw86214_haptic_raminit(struct aw86214 *aw86214, bool flag)
{
	if (flag) {
		aw86214_i2c_write_bits(aw86214, AW86214_REG_SYSCTRL1,
				       AW86214_BIT_SYSCTRL1_RAMINIT_MASK,
				       AW86214_BIT_SYSCTRL1_RAMINIT_ON);
	} else {
		aw86214_i2c_write_bits(aw86214, AW86214_REG_SYSCTRL1,
				       AW86214_BIT_SYSCTRL1_RAMINIT_MASK,
				       AW86214_BIT_SYSCTRL1_RAMINIT_OFF);
	}
}

static int aw86214_haptic_get_vbat(struct aw86214 *aw86214)
{
	unsigned char reg_val = 0;
	unsigned int vbat_code = 0;

	aw86214_haptic_stop_l(aw86214);
	aw86214_haptic_raminit(aw86214, true);
	aw86214_i2c_write_bits(aw86214, AW86214_REG_DETCFG2,
			       AW86214_BIT_DETCFG2_VBAT_GO_MASK,
			       AW86214_BIT_DETCFG2_VABT_GO_ON);
	usleep_range(AW86214_VBAT_DELAY_MIN, AW86214_VBAT_DELAY_MAX);
	aw86214_i2c_read(aw86214, AW86214_REG_DET_VBAT, &reg_val,
			 AW_I2C_BYTE_ONE);
	vbat_code = (vbat_code | reg_val) << 2;
	aw86214_i2c_read(aw86214, AW86214_REG_DET_LO, &reg_val,
			 AW_I2C_BYTE_ONE);
	vbat_code = vbat_code | ((reg_val & AW86214_BIT_DET_LO_VBAT) >> 4);
	aw86214->vbat = AW86214_VBAT_FORMULA(vbat_code);
	if (aw86214->vbat > AW86214_VBAT_MAX) {
		aw86214->vbat = AW86214_VBAT_MAX;
		aw_dev_info("%s vbat max limit = %dmV\n", __func__,
			    aw86214->vbat);
	}
	if (aw86214->vbat < AW86214_VBAT_MIN) {
		aw86214->vbat = AW86214_VBAT_MIN;
		aw_dev_info("%s vbat min limit = %dmV\n", __func__,
			    aw86214->vbat);
	}
	aw_dev_info("%s aw86214->vbat=%dmV, vbat_code=0x%02X\n", __func__,
		    aw86214->vbat, vbat_code);
	aw86214_haptic_raminit(aw86214, false);
	return 0;
}
static int aw86214_haptic_set_gain(struct aw86214 *aw86214, unsigned char gain)
{
	aw86214_i2c_write(aw86214, AW86214_REG_PLAYCFG2, &gain,
			  AW_I2C_BYTE_ONE);
	return 0;
}

static int aw86214_haptic_ram_vbat_comp(struct aw86214 *aw86214,
					      bool flag)
{
	int temp_gain = 0;

	if (flag) {
		if (aw86214->ram_vbat_compensate ==
		    AW86214_HAPTIC_RAM_VBAT_COMP_ENABLE) {
			aw86214_haptic_get_vbat(aw86214);
			temp_gain =
			    aw86214->gain * AW86214_VBAT_REFER / aw86214->vbat;
			if (temp_gain >
			    (128 * AW86214_VBAT_REFER / AW86214_VBAT_MIN)) {
				temp_gain =
				    128 * AW86214_VBAT_REFER / AW86214_VBAT_MIN;
				aw_dev_dbg("%s gain limit=%d\n", __func__,
					   temp_gain);
			}
			aw86214_haptic_set_gain(aw86214, temp_gain);
		} else {
			aw86214_haptic_set_gain(aw86214, aw86214->gain);
		}
	} else {
		aw86214_haptic_set_gain(aw86214, aw86214->gain);
	}
	return 0;
}

static int aw86214_haptic_play_mode(struct aw86214 *aw86214,
				    unsigned char play_mode)
{
	aw_dev_info("%s enter\n", __func__);

	switch (play_mode) {
	case AW86214_HAPTIC_STANDBY_MODE:
		aw_dev_info("%s: enter standby mode\n", __func__);
		aw86214->play_mode = AW86214_HAPTIC_STANDBY_MODE;
		aw86214_haptic_stop_l(aw86214);
		break;
	case AW86214_HAPTIC_RAM_MODE:
		aw_dev_info("%s: enter ram mode\n", __func__);
		aw86214->play_mode = AW86214_HAPTIC_RAM_MODE;
		aw86214_i2c_write_bits(aw86214, AW86214_REG_PLAYCFG3,
				       AW86214_BIT_PLAYCFG3_PLAY_MODE_MASK,
				       AW86214_BIT_PLAYCFG3_PLAY_MODE_RAM);
		break;
	case AW86214_HAPTIC_RAM_LOOP_MODE:
		aw_dev_info("%s: enter ram loop mode\n", __func__);
		aw86214->play_mode = AW86214_HAPTIC_RAM_LOOP_MODE;
		aw86214_i2c_write_bits(aw86214, AW86214_REG_PLAYCFG3,
				       AW86214_BIT_PLAYCFG3_PLAY_MODE_MASK,
				       AW86214_BIT_PLAYCFG3_PLAY_MODE_RAM);
		break;
	case AW86214_HAPTIC_CONT_MODE:
		aw_dev_info("%s: enter cont mode\n", __func__);
		aw86214->play_mode = AW86214_HAPTIC_CONT_MODE;
		aw86214_i2c_write_bits(aw86214, AW86214_REG_PLAYCFG3,
				       AW86214_BIT_PLAYCFG3_PLAY_MODE_MASK,
				       AW86214_BIT_PLAYCFG3_PLAY_MODE_CONT);
		break;
	default:
		aw_dev_err("%s: play mode %d error", __func__, play_mode);
		break;
	}
	return 0;
}

static void aw86214_haptic_play_go(struct aw86214 *aw86214)
{
	unsigned char reg_val = AW86214_BIT_PLAYCFG4_GO_ON;

	aw86214_i2c_write(aw86214, AW86214_REG_PLAYCFG4, &reg_val,
			  AW_I2C_BYTE_ONE);
}

static int aw86214_haptic_set_wav_seq(struct aw86214 *aw86214,
				      unsigned char wav, unsigned char seq)
{
	aw86214_i2c_write(aw86214, AW86214_REG_WAVCFG1 + wav, &seq,
			  AW_I2C_BYTE_ONE);
	return 0;
}

static int aw86214_haptic_set_wav_loop(struct aw86214 *aw86214,
				       unsigned char wav, unsigned char loop)
{
	unsigned char tmp = 0;

	if (wav % 2) {
		tmp = loop << 0;
		aw86214_i2c_write_bits(aw86214, AW86214_REG_WAVCFG9 + (wav / 2),
				       AW86214_BIT_WAVLOOP_SEQ_EVEN_MASK, tmp);
	} else {
		tmp = loop << 4;
		aw86214_i2c_write_bits(aw86214, AW86214_REG_WAVCFG9 + (wav / 2),
				       AW86214_BIT_WAVLOOP_SEQ_ODD_MASK, tmp);
	}
	return 0;
}

/*****************************************************
 *
 * haptic f0 cali
 *
 *****************************************************/
static int aw86214_haptic_read_lra_f0(struct aw86214 *aw86214)
{
	unsigned char reg_val[2] = {0};
	unsigned int f0_reg = 0;
	unsigned long f0_tmp = 0;

	aw_dev_info("%s enter\n", __func__);
	/* F_LRA_F0 */
	aw86214_i2c_read(aw86214, AW86214_REG_CONTRD14, reg_val,
			 AW_I2C_BYTE_TWO);
	f0_reg = (f0_reg | reg_val[0]) << 8;
	f0_reg |= (reg_val[1] << 0);
	if (!f0_reg) {
		aw_dev_err("%s didn't get lra f0 because f0_reg value is 0!\n",
			   __func__);
		aw86214->f0 = aw86214->dts_info.f0_ref;
		return -ERANGE;
	}
	f0_tmp = 384000 * 10 / f0_reg;
	aw86214->f0 = (unsigned int)f0_tmp;
	aw_dev_info("%s lra_f0=%d\n", __func__, aw86214->f0);
	return 0;
}

static int aw86214_haptic_read_cont_f0(struct aw86214 *aw86214)
{
	unsigned char reg_val[2] = {0};
	unsigned int f0_reg = 0;
	unsigned long f0_tmp = 0;

	aw_dev_info("%s enter\n", __func__);
	aw86214_i2c_read(aw86214, AW86214_REG_CONTRD16, reg_val,
			 AW_I2C_BYTE_TWO);
	f0_reg = (f0_reg | reg_val[0]) << 8;
	f0_reg |= (reg_val[1] << 0);
	if (!f0_reg) {
		aw_dev_err("%s didn't get cont f0 because f0_reg value is 0!\n",
			   __func__);
		aw86214->cont_f0 = aw86214->dts_info.f0_ref;
		return -ERANGE;
	}
	f0_tmp = 384000 * 10 / f0_reg;
	aw86214->cont_f0 = (unsigned int)f0_tmp;
	aw_dev_info("%s cont_f0=%d\n", __func__, aw86214->cont_f0);
	return 0;
}

static int aw86214_haptic_cont_get_f0(struct aw86214 *aw86214)
{
	int ret = 0;
	unsigned char reg_val = 0;
	unsigned char reg_array[3] = {0};
	unsigned int cnt = 200;
	bool get_f0_flag = false;
	unsigned char brk_en_temp = 0;

	aw_dev_info("%s enter\n", __func__);
	aw86214->f0 = aw86214->dts_info.f0_ref;
	/* enter standby mode */
	aw86214_haptic_stop_l(aw86214);
	/* f0 calibrate work mode */
	aw86214_haptic_play_mode(aw86214, AW86214_HAPTIC_CONT_MODE);
	/* enable f0 detect */
	aw86214_i2c_write_bits(aw86214, AW86214_REG_CONTCFG1,
			       AW86214_BIT_CONTCFG1_EN_F0_DET_MASK,
			       AW86214_BIT_CONTCFG1_F0_DET_ENABLE);
	/* cont config */
	aw86214_i2c_write_bits(aw86214, AW86214_REG_CONTCFG6,
			       AW86214_BIT_CONTCFG6_TRACK_EN_MASK,
			       AW86214_BIT_CONTCFG6_TRACK_ENABLE);
	/* enable auto brake */
	aw86214_i2c_read(aw86214, AW86214_REG_PLAYCFG3, &reg_val,
			 AW_I2C_BYTE_ONE);
	brk_en_temp = AW86214_BIT_PLAYCFG3_BRK & reg_val;
	aw86214_i2c_write_bits(aw86214, AW86214_REG_PLAYCFG3,
			       AW86214_BIT_PLAYCFG3_BRK_EN_MASK,
			       AW86214_BIT_PLAYCFG3_BRK_ENABLE);
	/* f0 driver level */
	aw86214_i2c_write_bits(aw86214, AW86214_REG_CONTCFG6,
			       AW86214_BIT_CONTCFG6_DRV1_LVL_MASK,
			       aw86214->dts_info.cont_drv1_lvl_dt);
	reg_array[0] = (unsigned char)aw86214->dts_info.cont_drv2_lvl_dt;
	reg_array[1] = (unsigned char)aw86214->dts_info.cont_drv1_time_dt;
	reg_array[2] = (unsigned char)aw86214->dts_info.cont_drv2_time_dt;
	aw86214_i2c_write(aw86214, AW86214_REG_CONTCFG7, reg_array,
			  AW_I2C_BYTE_THREE);
	/* TRACK_MARGIN */
	if (!aw86214->dts_info.cont_track_margin) {
		aw_dev_err("%s aw86214->dts_info.cont_track_margin = 0!\n",
			   __func__);
	} else {
		reg_val = (unsigned char)aw86214->dts_info.cont_track_margin;
		aw86214_i2c_write(aw86214, AW86214_REG_CONTCFG11, &reg_val,
				  AW_I2C_BYTE_ONE);
	}
	/* cont play go */
	aw86214_haptic_play_go(aw86214);
	/* 300ms */
	while (cnt) {
		aw86214_i2c_read(aw86214, AW86214_REG_GLBRD5, &reg_val,
				 AW_I2C_BYTE_ONE);
		if ((reg_val & AW86214_BIT_GLBRD5_STATE) ==
		    AW86214_BIT_GLBRD5_STATE_STANDBY) {
			cnt = 0;
			get_f0_flag = true;
			aw_dev_info("%s entered standby mode! glb_state=0x%02X\n",
				    __func__, reg_val);
		} else {
			cnt--;
			aw_dev_info("%s waitting for standby, glb_state=0x%02X\n",
				    __func__, reg_val);
		}
		usleep_range(AW86214_F0_DELAY_MIN, AW86214_F0_DELAY_MAX);
	}
	if (get_f0_flag) {
		aw86214_haptic_read_lra_f0(aw86214);
		aw86214_haptic_read_cont_f0(aw86214);
	} else {
		aw_dev_err("%s enter standby mode failed, stop reading f0!\n",
			   __func__);
	}
	/* restore default config */
	aw86214_i2c_write_bits(aw86214, AW86214_REG_CONTCFG1,
			       AW86214_BIT_CONTCFG1_EN_F0_DET_MASK,
			       AW86214_BIT_CONTCFG1_F0_DET_DISABLE);
	/* recover auto break config */
	aw86214_i2c_write_bits(aw86214, AW86214_REG_PLAYCFG3,
			       AW86214_BIT_PLAYCFG3_BRK_EN_MASK,
			       brk_en_temp);
	return ret;
}

static int aw86214_haptic_set_repeat_wav_seq(struct aw86214 *aw86214,
					     unsigned char seq)
{
	aw86214_haptic_set_wav_seq(aw86214, 0x00, seq);
	aw86214_haptic_set_wav_seq(aw86214, 0x01, 0x00);
	aw86214_haptic_set_wav_loop(aw86214, 0x00,
				    AW86214_BIT_WAVLOOP_INIFINITELY);
	return 0;
}

static int aw86214_haptic_get_lra_resistance(struct aw86214 *aw86214)
{
	unsigned char reg_val = 0;
	unsigned int lra_code = 0;
	unsigned int lra = 0;
	unsigned char d2s_gain_temp = 0;

	mutex_lock(&aw86214->lock);
	aw86214_haptic_stop_l(aw86214);
	aw86214_i2c_read(aw86214, AW86214_REG_SYSCTRL7, &reg_val,
			 AW_I2C_BYTE_ONE);
	d2s_gain_temp = 0x07 & reg_val;
	aw86214_i2c_write_bits(aw86214, AW86214_REG_SYSCTRL7,
			       AW86214_BIT_SYSCTRL7_D2S_GAIN_MASK,
			       aw86214->dts_info.d2s_gain);
	aw86214_haptic_raminit(aw86214, true);
	/* enter standby mode */
	aw86214_haptic_stop_l(aw86214);
	usleep_range(AW86214_STOP_DELAY_MIN, AW86214_STOP_DELAY_MAX);
	aw86214_i2c_write_bits(aw86214, AW86214_REG_SYSCTRL2,
			       AW86214_BIT_SYSCTRL2_STANDBY_MASK,
			       AW86214_BIT_SYSCTRL2_STANDBY_OFF);
	aw86214_i2c_write_bits(aw86214, AW86214_REG_DETCFG1,
			       AW86214_BIT_DETCFG1_RL_OS_MASK,
			       AW86214_BIT_DETCFG1_RL);
	aw86214_i2c_write_bits(aw86214, AW86214_REG_DETCFG2,
			       AW86214_BIT_DETCFG2_DIAG_GO_MASK,
			       AW86214_BIT_DETCFG2_DIAG_GO_ON);
	usleep_range(AW86214_LRA_DELAY_MIN, AW86214_LRA_DELAY_MAX);
	aw86214_i2c_read(aw86214, AW86214_REG_DET_RL, &reg_val,
			 AW_I2C_BYTE_ONE);
	lra_code = (lra_code | reg_val) << 2;
	aw86214_i2c_read(aw86214, AW86214_REG_DET_LO, &reg_val,
			 AW_I2C_BYTE_ONE);
	lra_code = lra_code | (reg_val & AW86214_BIT_DET_LO_RL);
	/* 2num */
	lra = AW86214_LRA_FORMULA(lra_code);
	/* Keep up with aw8624 driver */
	aw86214->lra = lra * 10;
	aw86214_haptic_raminit(aw86214, false);
	aw86214_i2c_write_bits(aw86214, AW86214_REG_SYSCTRL7,
			       AW86214_BIT_SYSCTRL7_D2S_GAIN_MASK,
			       d2s_gain_temp);
	mutex_unlock(&aw86214->lock);
	return 0;
}

static void aw86214_set_ram_addr(struct aw86214 *aw86214)
{
	unsigned char ram_addr[2] = {0};

	ram_addr[0] = (unsigned char)(aw86214->ram.base_addr >> 8);
	ram_addr[1] = (unsigned char)(aw86214->ram.base_addr & 0x00ff);

	aw86214_i2c_write(aw86214, AW86214_REG_RAMADDRH, ram_addr,
			 AW_I2C_BYTE_TWO);
}

#ifdef AW_CHECK_RAM_DATA
static int aw86214_check_ram_data(struct aw86214 *aw86214,
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

static int aw86214_container_update(struct aw86214 *aw86214,
				     struct aw86214_container *aw86214_cont)
{
	unsigned int shift = 0;
	int i = 0;
	int ret = 0;
	int len = 0;
#ifdef AW_CHECK_RAM_DATA
	unsigned char ram_data[AW_RAMDATA_RD_BUFFER_SIZE] = {0};
#endif

	aw_dev_info("%s enter\n", __func__);
	mutex_lock(&aw86214->lock);
	aw86214->ram.baseaddr_shift = 2;
	aw86214->ram.ram_shift = 4;
	/* RAMINIT Enable */
	aw86214_haptic_raminit(aw86214, true);
	/* Enter standby mode */
	aw86214_haptic_stop_l(aw86214);
	/* set base addr */
	shift = aw86214->ram.baseaddr_shift;
	aw86214->ram.base_addr =
	    (unsigned int)((aw86214_cont->data[0 + shift] << 8) |
			   (aw86214_cont->data[1 + shift]));
	aw86214_i2c_write_bits(aw86214, AW86214_REG_RTPCFG1, /*ADDRH*/
			AW86214_BIT_RTPCFG1_ADDRH_MASK,
			aw86214_cont->data[0 + shift]);

	aw86214_i2c_write(aw86214, AW86214_REG_RTPCFG2, /*ADDRL*/
			  &aw86214_cont->data[1 + shift],
			  AW_I2C_BYTE_ONE);

	/* ram */
	aw86214_set_ram_addr(aw86214);
	i = aw86214->ram.ram_shift;
	while (i < aw86214_cont->len) {
		if ((aw86214_cont->len - i) < AW_RAMDATA_WR_BUFFER_SIZE)
			len = aw86214_cont->len - i;
		else
			len = AW_RAMDATA_WR_BUFFER_SIZE;

		aw86214_i2c_write(aw86214, AW86214_REG_RAMDATA,
				 &aw86214_cont->data[i], len);
		i += len;
	}

#ifdef	AW_CHECK_RAM_DATA
	aw86214_set_ram_addr(aw86214);
	i = aw86214->ram.ram_shift;
	while (i < aw86214_cont->len) {
		if ((aw86214_cont->len - i) < AW_RAMDATA_RD_BUFFER_SIZE)
			len = aw86214_cont->len - i;
		else
			len = AW_RAMDATA_RD_BUFFER_SIZE;

		aw86214_i2c_read(aw86214, AW86214_REG_RAMDATA, ram_data, len);
		ret = aw86214_check_ram_data(aw86214, &aw86214_cont->data[i],
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
	aw86214_haptic_raminit(aw86214, false);
	mutex_unlock(&aw86214->lock);
	aw_dev_info("%s exit\n", __func__);
	return ret;
}

static int aw86214_haptic_ram_config(struct aw86214 *aw86214, int duration)
{
	int ret = 0;

	if (aw86214->duration_time_flag < 0) {
		aw_dev_err("%s: duration time error, array size = %d\n",
			   __func__, aw86214->duration_time_size);
		return -ERANGE;
	}
	ret = aw86214_analyse_duration_range(aw86214);
	if (ret < 0)
		return ret;

	if ((duration > 0) && (duration <
				aw86214->dts_info.duration_time[0])) {
		aw86214->index = 3;	/*3*/
		aw86214->activate_mode = AW86214_HAPTIC_RAM_MODE;
	} else if ((duration >= aw86214->dts_info.duration_time[0]) &&
		(duration < aw86214->dts_info.duration_time[1])) {
		aw86214->index = 2;	/*2*/
		aw86214->activate_mode = AW86214_HAPTIC_RAM_MODE;
	} else if ((duration >= aw86214->dts_info.duration_time[1]) &&
		(duration < aw86214->dts_info.duration_time[2])) {
		aw86214->index = 1;	/*1*/
		aw86214->activate_mode = AW86214_HAPTIC_RAM_MODE;
	} else if (duration >= aw86214->dts_info.duration_time[2]) {
		aw86214->index = 4;	/*4*/
		aw86214->activate_mode = AW86214_HAPTIC_RAM_LOOP_MODE;
	} else {
		aw_dev_err("%s: duration time error, duration= %d\n",
			   __func__, duration);
		aw86214->index = 0;
		aw86214->activate_mode = AW86214_HAPTIC_NULL;
		ret = -ERANGE;
	}

	return ret;
}

static int aw86214_haptic_get_ram_number(struct aw86214 *aw86214)
{
	unsigned char ram_data[3];
	unsigned int first_wave_addr = 0;

	aw_dev_info("%s enter!\n", __func__);
	if (!aw86214->ram_init) {
		aw_dev_err("%s: ram init faild, ram_num = 0!\n", __func__);
		return -EPERM;
	}

	mutex_lock(&aw86214->lock);
	/* RAMINIT Enable */
	aw86214_haptic_raminit(aw86214, true);
	aw86214_haptic_stop_l(aw86214);
	aw86214_set_ram_addr(aw86214);
	aw86214_i2c_read(aw86214, AW86214_REG_RAMDATA, ram_data,
			 AW_I2C_BYTE_THREE);
	first_wave_addr = (ram_data[1] << 8 | ram_data[2]);
	aw86214->ram.ram_num =
			(first_wave_addr - aw86214->ram.base_addr - 1) / 4;
	aw_dev_info("%s: ram_version = 0x%02x\n", __func__, ram_data[0]);
	aw_dev_info("%s: first waveform addr = 0x%04x\n", __func__,
		    first_wave_addr);
	aw_dev_info("%s: ram_num = %d\n", __func__, aw86214->ram.ram_num);
	/* RAMINIT Disable */
	aw86214_haptic_raminit(aw86214, false);
	mutex_unlock(&aw86214->lock);

	return 0;
}


static void aw86214_ram_check(const struct firmware *cont, void *context)
{
	struct aw86214 *aw86214 = context;
	struct aw86214_container *aw86214_fw;
	unsigned short check_sum = 0;
	int i = 0;
	int ret = 0;
#ifdef AW_READ_BIN_FLEXBALLY
	static unsigned char load_cont;
	int ram_timer_val = 1000;

	load_cont++;
#endif
	aw_dev_info("%s enter\n", __func__);
	if (!cont) {
		aw_dev_err("%s: failed to read %s\n", __func__,
			   aw86214_ram_name);
		release_firmware(cont);
#ifdef AW_READ_BIN_FLEXBALLY
	if (load_cont <= 20) {
		schedule_delayed_work(&aw86214->ram_work,
					msecs_to_jiffies(ram_timer_val));
		aw_dev_info("%s:start hrtimer: load_cont=%d\n", __func__,
			    load_cont);
	}
#endif
		return;
	}
	aw_dev_info("%s: loaded %s - size: %zu bytes\n", __func__,
		    aw86214_ram_name, cont ? cont->size : 0);
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

		aw_dev_info("%s: check sum pass: 0x%04x\n", __func__,
			    check_sum);
		aw86214->ram.check_sum = check_sum;
	} else {
		aw_dev_err("%s: check sum err: check_sum=0x%04x\n", __func__,
			   check_sum);
		return;
	}

	/* aw86214 ram update less then 128kB */
	aw86214_fw = kzalloc(cont->size + sizeof(int), GFP_KERNEL);
	if (!aw86214_fw) {
		release_firmware(cont);
		aw_dev_err("%s: Error allocating memory\n", __func__);
		return;
	}
	aw86214_fw->len = cont->size;
	memcpy(aw86214_fw->data, cont->data, cont->size);
	release_firmware(cont);
	ret = aw86214_container_update(aw86214, aw86214_fw);
	if (ret) {
		kfree(aw86214_fw);
		aw86214->ram.len = 0;
		aw_dev_err("%s: ram firmware update failed!\n", __func__);
	} else {
		aw86214->ram_init = 1;
		aw86214->ram.len = aw86214_fw->len;
		kfree(aw86214_fw);
		aw_dev_info("%s: ram firmware update complete!\n", __func__);
	}
	aw86214_haptic_get_ram_number(aw86214);
}

static int aw86214_ram_update(struct aw86214 *aw86214)
{
	aw86214->ram_init = 0;

	return request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
				       aw86214_ram_name, aw86214->dev,
				       GFP_KERNEL, aw86214, aw86214_ram_check);
}

static enum hrtimer_restart aw86214_vibrator_timer_func(struct hrtimer *timer)
{
	struct aw86214 *aw86214 = container_of(timer, struct aw86214, timer);

	aw_dev_info("%s enter\n", __func__);
	aw86214->state = 0;
	schedule_work(&aw86214->vibrator_work);

	return HRTIMER_NORESTART;
}

/*****************************************************
 *
 * motor protect
 *
 *****************************************************/
static int aw86214_haptic_swicth_motor_protect_config(struct aw86214 *aw86214,
						      unsigned char addr,
						      unsigned char val)
{
	aw_dev_info("%s enter\n", __func__);
	if (addr == 1) {
		aw86214_i2c_write_bits(aw86214, AW86214_REG_DETCFG1,
				       AW86214_BIT_DETCFG1_PRCT_MODE_MASK,
				       AW86214_BIT_DETCFG1_PRCT_MODE_VALID);
		aw86214_i2c_write_bits(aw86214, AW86214_REG_PWMCFG1,
				       AW86214_BIT_PWMCFG1_PRC_EN_MASK,
				       AW86214_BIT_PWMCFG1_PRC_ENABLE);
		aw86214_i2c_write_bits(aw86214, AW86214_REG_PWMCFG3,
				       AW86214_BIT_PWMCFG3_PR_EN_MASK,
				       AW86214_BIT_PWMCFG3_PR_ENABLE);
	} else if (addr == 0) {
		aw86214_i2c_write_bits(aw86214, AW86214_REG_DETCFG1,
				       AW86214_BIT_DETCFG1_PRCT_MODE_MASK,
				       AW86214_BIT_DETCFG1_PRCT_MODE_INVALID);
		aw86214_i2c_write_bits(aw86214, AW86214_REG_PWMCFG1,
				       AW86214_BIT_PWMCFG1_PRC_EN_MASK,
				       AW86214_BIT_PWMCFG1_PRC_DISABLE);
		aw86214_i2c_write_bits(aw86214, AW86214_REG_PWMCFG3,
				       AW86214_BIT_PWMCFG3_PR_EN_MASK,
				       AW86214_BIT_PWMCFG3_PR_DISABLE);
	} else if (addr == 0x2d) {
		aw86214_i2c_write_bits(aw86214, AW86214_REG_PWMCFG1,
				       AW86214_BIT_PWMCFG1_PRCTIME_MASK, val);
	} else if (addr == 0x3e) {
		aw86214_i2c_write_bits(aw86214, AW86214_REG_PWMCFG3,
				       AW86214_BIT_PWMCFG3_PRLVL_MASK, val);
	} else if (addr == 0x3f) {
		aw86214_i2c_write(aw86214, AW86214_REG_PWMCFG4, &val,
				  AW_I2C_BYTE_ONE);
	}
	return 0;
}

static int aw86214_haptic_f0_calibration(struct aw86214 *aw86214)
{
	int ret = 0;
	unsigned char reg_val = 0;
	unsigned int f0_limit = 0;
	char f0_cali_lra = 0;
	int f0_cali_step = 0;
	unsigned int f0_cali_min = aw86214->dts_info.f0_ref *
				(100 - aw86214->dts_info.f0_cali_percent) / 100;
	unsigned int f0_cali_max =  aw86214->dts_info.f0_ref *
				(100 + aw86214->dts_info.f0_cali_percent) / 100;

	aw_dev_info("%s enter\n", __func__);
	/*
	 * aw86214_haptic_upload_lra(aw86214, AW86214_WRITE_ZERO);
	 */
	if (aw86214_haptic_cont_get_f0(aw86214)) {
		aw_dev_err("%s get f0 error, user defafult f0\n", __func__);
	} else {
		/* max and min limit */
		f0_limit = aw86214->f0;
		aw_dev_info("%s f0_ref = %d, f0_cali_min = %d, f0_cali_max = %d, f0 = %d\n",
			    __func__, aw86214->dts_info.f0_ref,
			    f0_cali_min, f0_cali_max, aw86214->f0);

		if ((aw86214->f0 < f0_cali_min) || aw86214->f0 > f0_cali_max) {
			aw_dev_err("%s f0 calibration out of range = %d!\n",
				   __func__, aw86214->f0);
			f0_limit = aw86214->dts_info.f0_ref;
			return -ERANGE;
		}
		aw_dev_info("%s f0_limit = %d\n", __func__, (int)f0_limit);
		/* calculate cali step */
		f0_cali_step = 100000 * ((int)f0_limit -
					 (int)aw86214->dts_info.f0_ref) /
					((int)f0_limit * 24);
		aw_dev_info("%s f0_cali_step = %d\n", __func__, f0_cali_step);
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
		aw86214->f0_cali_data = (int)f0_cali_lra;
		aw86214_haptic_upload_lra(aw86214, AW86214_F0_CALI);
		aw86214_i2c_read(aw86214, AW86214_REG_TRIMCFG3, &reg_val,
				 AW_I2C_BYTE_ONE);
		aw_dev_info("%s: final f0_cali_data = 0x%02X\n",
			    __func__, reg_val);
	}
	/* restore standby work mode */
	aw86214_haptic_stop_l(aw86214);
	return ret;
}

/*****************************************************
 *
 * haptic cont
 *
 *****************************************************/
static int aw86214_haptic_cont(struct aw86214 *aw86214)
{
	unsigned char reg_val = 0;

	aw_dev_info("%s enter\n", __func__);

	/* work mode */
	aw86214_haptic_play_mode(aw86214, AW86214_HAPTIC_CONT_MODE);
	/* cont config */
	aw86214_i2c_write_bits(aw86214, AW86214_REG_CONTCFG6,
			       AW86214_BIT_CONTCFG6_TRACK_EN_MASK,
			       AW86214_BIT_CONTCFG6_TRACK_ENABLE);
	/* f0 driver level */
	aw86214_i2c_write_bits(aw86214, AW86214_REG_CONTCFG6,
			       AW86214_BIT_CONTCFG6_DRV1_LVL_MASK,
			       aw86214->cont_drv1_lvl);
	reg_val = (unsigned char)aw86214->cont_drv2_lvl;
	aw86214_i2c_write(aw86214, AW86214_REG_CONTCFG7, &reg_val,
			  AW_I2C_BYTE_ONE);
	/* DRV2_TIME */
	reg_val = 0xFF;
	aw86214_i2c_write(aw86214, AW86214_REG_CONTCFG9, &reg_val,
			  AW_I2C_BYTE_ONE);
	/* cont play go */
	aw86214_haptic_play_go(aw86214);
	return 0;
}

static int aw86214_haptic_play_wav_seq(struct aw86214 *aw86214,
				       unsigned char flag)
{
	aw_dev_info("%s enter\n", __func__);
	if (flag) {
		aw86214_haptic_play_mode(aw86214, AW86214_HAPTIC_RAM_MODE);
		aw86214_haptic_play_go(aw86214);
	}
	return 0;
}


static void aw86214_haptic_misc_para_init(struct aw86214 *aw86214)
{
	unsigned char reg_array[4] = {0};

	aw_dev_info("%s enter\n", __func__);

	aw86214_ver_a_init(aw86214);

	aw86214->cont_drv1_lvl = aw86214->dts_info.cont_drv1_lvl_dt;
	aw86214->cont_drv2_lvl = aw86214->dts_info.cont_drv2_lvl_dt;
	aw86214->cont_drv1_time = aw86214->dts_info.cont_drv1_time_dt;
	aw86214->cont_drv2_time = aw86214->dts_info.cont_drv2_time_dt;
	aw86214->cont_wait_num = aw86214->dts_info.cont_wait_num_dt;

	/* SIN_H */
	reg_array[0] = (unsigned char)aw86214->dts_info.sine_array[0];
	/* SIN_L */
	reg_array[1] = (unsigned char)aw86214->dts_info.sine_array[1];
	/* COS_H */
	reg_array[2] = (unsigned char)aw86214->dts_info.sine_array[2];
	/* COS_L */
	reg_array[3] = (unsigned char)aw86214->dts_info.sine_array[3];

	aw86214_i2c_write(aw86214, AW86214_REG_SYSCTRL3, reg_array,
			  AW_I2C_BYTE_FOUR);

	/* d2s_gain */
	if (!aw86214->dts_info.d2s_gain) {
		aw_dev_err("%s aw86214->dts_info.d2s_gain = 0!\n", __func__);
	} else {
		aw86214_i2c_write_bits(aw86214, AW86214_REG_SYSCTRL7,
				       AW86214_BIT_SYSCTRL7_D2S_GAIN_MASK,
				       aw86214->dts_info.d2s_gain);
	}

	if (aw86214->dts_info.cont_tset && aw86214->dts_info.cont_bemf_set) {
		reg_array[0] = (unsigned char)aw86214->dts_info.cont_tset;
		reg_array[1] = (unsigned char)aw86214->dts_info.cont_bemf_set;
		aw86214_i2c_write_bits(aw86214, AW86214_REG_CONTCFG13,
				       (AW86214_BIT_CONTCFG13_TSET_MASK &
					AW86214_BIT_CONTCFG13_BEME_SET_MASK),
				       ((reg_array[0] << 4)|
					reg_array[1]));
	}
}

/*****************************************************
 *
 * offset calibration
 *
 *****************************************************/
static int aw86214_haptic_offset_calibration(struct aw86214 *aw86214)
{
	unsigned int cont = 2000;
	unsigned char reg_val = 0;

	aw_dev_info("%s enter\n", __func__);

	aw86214_haptic_raminit(aw86214, true);

	aw86214_i2c_write_bits(aw86214, AW86214_REG_DETCFG2,
			       AW86214_BIT_DETCFG2_DIAG_GO_MASK,
			       AW86214_BIT_DETCFG2_DIAG_GO_ON);
	while (1) {
		aw86214_i2c_read(aw86214, AW86214_REG_DETCFG2, &reg_val,
				 AW_I2C_BYTE_ONE);
		if ((reg_val & 0x01) == 0 || cont == 0)
			break;
		cont--;
	}
	if (cont == 0)
		aw_dev_err("%s calibration offset failed!\n", __func__);
	aw86214_haptic_raminit(aw86214, false);
	return 0;
}

static int aw86214_haptic_set_pwm(struct aw86214 *aw86214, unsigned char mode)
{
	switch (mode) {
	case AW86214_PWM_48K:
		aw86214_i2c_write_bits(aw86214, AW86214_REG_SYSCTRL2,
				       AW86214_BIT_SYSCTRL2_WAVDAT_MODE_MASK,
				       AW86214_BIT_SYSCTRL2_RATE_48K);
		break;
	case AW86214_PWM_24K:
		aw86214_i2c_write_bits(aw86214, AW86214_REG_SYSCTRL2,
				       AW86214_BIT_SYSCTRL2_WAVDAT_MODE_MASK,
				       AW86214_BIT_SYSCTRL2_RATE_24K);
		break;
	case AW86214_PWM_12K:
		aw86214_i2c_write_bits(aw86214, AW86214_REG_SYSCTRL2,
				       AW86214_BIT_SYSCTRL2_WAVDAT_MODE_MASK,
				       AW86214_BIT_SYSCTRL2_RATE_12K);
		break;
	default:
		break;
	}
	return 0;
}

/*****************************************************
 *
 * vbat mode
 *
 *****************************************************/
static int aw86214_haptic_vbat_mode_config(struct aw86214 *aw86214,
					   unsigned char flag)
{
	if (flag == AW86214_HAPTIC_CONT_VBAT_HW_ADJUST_MODE) {
		aw86214_i2c_write_bits(aw86214, AW86214_REG_SYSCTRL1,
				       AW86214_BIT_SYSCTRL1_VBAT_MODE_MASK,
				       AW86214_BIT_SYSCTRL1_VBAT_MODE_HW);
	} else {
		aw86214_i2c_write_bits(aw86214, AW86214_REG_SYSCTRL1,
				       AW86214_BIT_SYSCTRL1_VBAT_MODE_MASK,
				       AW86214_BIT_SYSCTRL1_VBAT_MODE_SW);
	}
	return 0;
}

static void aw86214_ram_work_routine(struct work_struct *work)
{
	struct aw86214 *aw86214 = container_of(work, struct aw86214,
					       ram_work.work);

	aw_dev_info("%s enter\n", __func__);
	aw86214_ram_update(aw86214);
}

int aw86214_ram_work_init_l(struct aw86214 *aw86214)
{
	int ram_timer_val = 8000;

	aw_dev_info("%s enter\n", __func__);
	INIT_DELAYED_WORK(&aw86214->ram_work, aw86214_ram_work_routine);
	schedule_delayed_work(&aw86214->ram_work,
			      msecs_to_jiffies(ram_timer_val));
	return 0;
}


static int
aw86214_haptic_audio_ctr_list_insert(struct haptic_audio *haptic_audio,
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
aw86214_haptic_audio_ctr_list_clear(struct haptic_audio *haptic_audio)
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

static int aw86214_haptic_audio_off(struct aw86214 *aw86214)
{
	aw_dev_dbg("%s: enter\n", __func__);
	mutex_lock(&aw86214->lock);
	aw86214_haptic_set_gain(aw86214, 0x80);
	aw86214_haptic_stop_l(aw86214);
	aw86214->gun_type = AW_GUN_TYPE_DEF_VAL;
	aw86214->bullet_nr = AW_BULLET_NR_DEF_VAL;
	aw86214_haptic_audio_ctr_list_clear(&aw86214->haptic_audio);
	mutex_unlock(&aw86214->lock);
	return 0;
}

static int aw86214_haptic_audio_init(struct aw86214 *aw86214)
{

	aw_dev_dbg("%s enter\n", __func__);
	aw86214_haptic_set_wav_seq(aw86214, 0x01, 0x00);

	return 0;
}

static void aw86214_interrupt_clear(struct aw86214 *aw86214)
{
	unsigned char reg_val = 0;

	aw86214_i2c_read(aw86214, AW86214_REG_SYSINT, &reg_val,
			 AW_I2C_BYTE_ONE);
	aw_dev_dbg("%s: SYSINT=0x%02X\n", __func__, reg_val);
}

static int aw86214_haptic_activate(struct aw86214 *aw86214)
{
	aw86214_i2c_write_bits(aw86214, AW86214_REG_SYSCTRL2,
			       AW86214_BIT_SYSCTRL2_STANDBY_MASK,
			       AW86214_BIT_SYSCTRL2_STANDBY_OFF);
	aw86214_interrupt_clear(aw86214);
	aw86214_i2c_write_bits(aw86214, AW86214_REG_SYSINTM,
			       AW86214_BIT_SYSINTM_UVLM_MASK,
			       AW86214_BIT_SYSINTM_UVLM_ON);
	return 0;
}

static int aw86214_haptic_start(struct aw86214 *aw86214)
{
	aw86214_haptic_activate(aw86214);
	aw86214_haptic_play_go(aw86214);
	return 0;
}

static void aw86214_ctr_list_config(struct aw86214 *aw86214,
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
			(AW86214_HAPTIC_CMD_ENABLE ==
				(AW86214_HAPTIC_CMD_HAPTIC & p_ctr->cmd))) {
				list_del(&p_ctr->list);
				kfree(p_ctr);
				list_del_cnt++;
			}
			if (list_del_cnt == list_diff_cnt)
				break;
		}
	}
}

static void aw86214_haptic_audio_work_routine(struct work_struct *work)
{
	struct aw86214 *aw86214 = container_of(work,
					struct aw86214,
					haptic_audio.work);
	struct haptic_audio *haptic_audio = NULL;
	struct haptic_ctr *p_ctr = NULL;
	struct haptic_ctr *p_ctr_bak = NULL;
	unsigned int ctr_list_flag = 0;

	aw_dev_dbg("%s enter\n", __func__);

	haptic_audio = &(aw86214->haptic_audio);
	mutex_lock(&aw86214->haptic_audio.lock);
	memset(&aw86214->haptic_audio.ctr, 0,
	       sizeof(struct haptic_ctr));
	ctr_list_flag = 0;
	list_for_each_entry_safe_reverse(p_ctr, p_ctr_bak,
			&(haptic_audio->ctr_list), list) {
		ctr_list_flag = 1;
		break;
	}
	if (ctr_list_flag == 0)
		aw_dev_info("%s: ctr list empty\n", __func__);

	if (ctr_list_flag == 1)
		aw86214_ctr_list_config(aw86214, p_ctr,
					p_ctr_bak, haptic_audio);


	/* get the last data from list */
	list_for_each_entry_safe_reverse(p_ctr, p_ctr_bak,
				&(haptic_audio->ctr_list), list) {
		aw86214->haptic_audio.ctr.cnt = p_ctr->cnt;
		aw86214->haptic_audio.ctr.cmd = p_ctr->cmd;
		aw86214->haptic_audio.ctr.play = p_ctr->play;
		aw86214->haptic_audio.ctr.wavseq = p_ctr->wavseq;
		aw86214->haptic_audio.ctr.loop = p_ctr->loop;
		aw86214->haptic_audio.ctr.gain = p_ctr->gain;
		list_del(&p_ctr->list);
		kfree(p_ctr);
		break;
	}

	if (aw86214->haptic_audio.ctr.play) {
		aw_dev_info("%s: cnt=%d, cmd=%d, play=%d, wavseq=%d, loop=%d, gain=%d\n",
			__func__,
			aw86214->haptic_audio.ctr.cnt,
			aw86214->haptic_audio.ctr.cmd,
			aw86214->haptic_audio.ctr.play,
			aw86214->haptic_audio.ctr.wavseq,
			aw86214->haptic_audio.ctr.loop,
			aw86214->haptic_audio.ctr.gain);
	}
	mutex_unlock(&aw86214->haptic_audio.lock);

	/*haptic play control*/
	if (AW86214_HAPTIC_CMD_ENABLE ==
	   (AW86214_HAPTIC_CMD_HAPTIC & aw86214->haptic_audio.ctr.cmd)) {
		if (aw86214->haptic_audio.ctr.play ==
			AW86214_HAPTIC_PLAY_ENABLE) {
			aw_dev_info("%s: haptic_audio_play_start\n", __func__);
			aw_dev_info("%s: normal haptic start\n", __func__);
			mutex_lock(&aw86214->lock);
			aw86214_haptic_stop_l(aw86214);
			aw86214_haptic_play_mode(aw86214,
				AW86214_HAPTIC_RAM_MODE);
			aw86214_haptic_set_wav_seq(aw86214, 0x00,
				aw86214->haptic_audio.ctr.wavseq);
			aw86214_haptic_set_wav_loop(aw86214, 0x00,
				aw86214->haptic_audio.ctr.loop);
			aw86214_haptic_set_gain(aw86214,
				aw86214->haptic_audio.ctr.gain);
			aw86214_haptic_start(aw86214);
			mutex_unlock(&aw86214->lock);
		} else if (AW86214_HAPTIC_PLAY_STOP ==
			   aw86214->haptic_audio.ctr.play) {
			mutex_lock(&aw86214->lock);
			aw86214_haptic_stop_l(aw86214);
			mutex_unlock(&aw86214->lock);
		} else if (AW86214_HAPTIC_PLAY_GAIN ==
			   aw86214->haptic_audio.ctr.play) {
			mutex_lock(&aw86214->lock);
			aw86214_haptic_set_gain(aw86214,
					       aw86214->haptic_audio.ctr.gain);
			mutex_unlock(&aw86214->lock);
		}
	}


}

static enum hrtimer_restart
aw86214_haptic_audio_timer_func(struct hrtimer *timer)
{
	struct aw86214 *aw86214 = container_of(timer,
					struct aw86214, haptic_audio.timer);

	aw_dev_dbg("%s enter\n", __func__);
	schedule_work(&aw86214->haptic_audio.work);

	hrtimer_start(&aw86214->haptic_audio.timer,
		ktime_set(aw86214->haptic_audio.timer_val/1000000,
			(aw86214->haptic_audio.timer_val%1000000)*1000),
		HRTIMER_MODE_REL);
	return HRTIMER_NORESTART;
}


#ifdef TIMED_OUTPUT
static int aw86214_vibrator_get_time(struct timed_output_dev *dev)
{
	struct aw86214 *aw86214 = container_of(dev, struct aw86214, vib_dev);

	if (hrtimer_active(&aw86214->timer)) {
		ktime_t r = hrtimer_get_remaining(&aw86214->timer);

		return ktime_to_ms(r);
	}
	return 0;
}

static void aw86214_vibrator_enable(struct timed_output_dev *dev, int value)
{
	struct aw86214 *aw86214 = container_of(dev, struct aw86214, vib_dev);

	aw_dev_info("%s enter\n", __func__);
	if (!aw86214->ram_init) {
		aw_dev_err("%s: ram init failed, not allow to play!\n",
			   __func__);
		return;
	}
	mutex_lock(&aw86214->lock);
	aw86214_haptic_stop_l(aw86214);
	if (value > 0) {
		aw86214_brk_en(aw86214, false);
		aw86214_haptic_ram_vbat_comp(aw86214, false);
		aw86214_haptic_play_wav_seq(aw86214, value);
	}
	mutex_unlock(&aw86214->lock);
	aw_dev_info("%s exit\n", __func__);
}
#else
static enum led_brightness aw86214_haptic_brightness_get(struct led_classdev
							 *cdev)
{
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);

	return aw86214->amplitude;
}

static void aw86214_haptic_brightness_set(struct led_classdev *cdev,
					  enum led_brightness level)
{
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);

	aw_dev_info("%s enter\n", __func__);
	if (!aw86214->ram_init) {
		aw_dev_err("%s: ram init failed, not allow to play!\n",
			   __func__);
		return;
	}
	if (aw86214->ram_update_flag < 0)
		return;
	aw86214->amplitude = level;
	mutex_lock(&aw86214->lock);
	aw86214_haptic_stop_l(aw86214);
	if (aw86214->amplitude > 0) {
		aw86214_brk_en(aw86214, false);
		aw86214_haptic_ram_vbat_comp(aw86214, false);
		aw86214_haptic_play_wav_seq(aw86214, aw86214->amplitude);
	}
	mutex_unlock(&aw86214->lock);
}
#endif

static ssize_t aw86214_cont_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);
	ssize_t len = 0;

	aw86214_haptic_read_cont_f0(aw86214);
	len += snprintf(buf + len, PAGE_SIZE - len,
			"%d\n", aw86214->cont_f0);
	return len;
}

static ssize_t aw86214_cont_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;

	aw86214_haptic_stop_l(aw86214);
	aw86214_brk_en(aw86214, true);
	if (val)
		aw86214_haptic_cont(aw86214);
	return count;
}

static ssize_t aw86214_f0_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);
	ssize_t len = 0;
	unsigned char reg = 0;

	mutex_lock(&aw86214->lock);
	/* set d2s_gain to max to get better performance when cat f0 .*/
	aw86214_i2c_write_bits(aw86214, AW86214_REG_SYSCTRL7,
				AW86214_BIT_SYSCTRL7_D2S_GAIN_MASK,
				AW86214_BIT_SYSCTRL7_D2S_GAIN_40);
	aw86214_i2c_read(aw86214, AW86214_REG_TRIMCFG3, &reg, AW_I2C_BYTE_ONE);
	aw86214_haptic_upload_lra(aw86214, AW86214_WRITE_ZERO);
	aw86214_haptic_cont_get_f0(aw86214);
	aw86214_i2c_write(aw86214, AW86214_REG_TRIMCFG3, &reg, AW_I2C_BYTE_ONE);
	/* set d2s_gain to default when cat f0 is finished.*/
	aw86214_i2c_write_bits(aw86214, AW86214_REG_SYSCTRL7,
				AW86214_BIT_SYSCTRL7_D2S_GAIN_MASK,
				aw86214->dts_info.d2s_gain);
	mutex_unlock(&aw86214->lock);
	len += snprintf(buf + len, PAGE_SIZE - len,
			"%d\n", aw86214->f0);
	return len;
}

static ssize_t aw86214_f0_store(struct device *dev,
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

static ssize_t aw86214_reg_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	unsigned char i = 0;
	unsigned char size = 0;
	unsigned char cnt = 0;
	unsigned char reg_array[AW_REG_MAX] = {0};
	ssize_t len = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);

	aw_dev_dbg("%s: enter!\n", __func__);
	for (i = 0; i <= (AW86214_REG_TRIMCFG3 + 1); i++) {

		if (i == aw86214_reg_list[cnt] &&
		    (cnt < sizeof(aw86214_reg_list))) {
			size++;
			cnt++;
			continue;
		} else {
			if (size != 0) {
				aw86214_i2c_read(aw86214,
						 aw86214_reg_list[cnt-size],
						 &reg_array[cnt-size], size);
				size = 0;

			}
		}
	}

	for (i = 0; i < sizeof(aw86214_reg_list); i++) {
		len += snprintf(buf + len, PAGE_SIZE - len,
				"reg:0x%02X=0x%02X\n", aw86214_reg_list[i],
				reg_array[i]);
	}

	return len;
}

static ssize_t aw86214_reg_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	unsigned char reg_val = 0;
	unsigned int databuf[2] = { 0, 0 };
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		reg_val = (unsigned char)databuf[1];
		aw86214_i2c_write(aw86214, (unsigned char)databuf[0],
				  &reg_val, AW_I2C_BYTE_ONE);
	}

	return count;
}

static ssize_t aw86214_duration_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);
	ktime_t time_rem;
	s64 time_ms = 0;

	if (hrtimer_active(&aw86214->timer)) {
		time_rem = hrtimer_get_remaining(&aw86214->timer);
		time_ms = ktime_to_ms(time_rem);
	}
	return snprintf(buf, PAGE_SIZE, "%lldms\n", time_ms);
}

static ssize_t aw86214_duration_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;
	/* setting 0 on duration is NOP for now */
	if (val <= 0)
		return count;
	rc = aw86214_haptic_ram_config(aw86214, val);
	if (rc < 0) {
		aw_dev_info("%s: ram config failed!\n", __func__);
		return count;
	}
	aw86214->duration = val;
	return count;
}

static ssize_t aw86214_activate_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);

	/* For now nothing to show */
	return snprintf(buf, PAGE_SIZE, "%d\n", aw86214->state);
}

static ssize_t aw86214_activate_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);
	unsigned int val = 0;
	int rc = 0;

	if (!aw86214->ram_init) {
		aw_dev_err("%s: ram init failed, not allow to play!\n",
			   __func__);
		return count;
	}
	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;
	aw_dev_info("%s: value=%d\n", __func__, val);
	mutex_lock(&aw86214->lock);
	hrtimer_cancel(&aw86214->timer);
	aw86214->state = val;
	mutex_unlock(&aw86214->lock);
	schedule_work(&aw86214->vibrator_work);
	return count;
}

static ssize_t aw86214_seq_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);
	size_t count = 0;
	unsigned char i = 0;
	unsigned char reg_val[AW86214_SEQUENCER_SIZE] = {0};

	aw86214_i2c_read(aw86214, AW86214_REG_WAVCFG1, reg_val,
			 AW86214_SEQUENCER_SIZE);
	for (i = 0; i < AW86214_SEQUENCER_SIZE; i++) {
		count += snprintf(buf+count, PAGE_SIZE-count,
			"seq%d: 0x%02x\n", i+1, reg_val[i]);
		aw86214->seq[i] = reg_val[i];
	}
	return count;
}

static ssize_t aw86214_seq_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);
	unsigned int databuf[2] = { 0, 0 };

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		if (databuf[0] > AW86214_SEQUENCER_SIZE ||
		    databuf[1] > aw86214->ram.ram_num) {
			aw_dev_err("%s input value out of range\n", __func__);
			return count;
		}
		aw_dev_info("%s: seq%d=0x%02X\n", __func__,
			    databuf[0], databuf[1]);
		mutex_lock(&aw86214->lock);
		aw86214->seq[databuf[0]] = (unsigned char)databuf[1];
		aw86214_haptic_set_wav_seq(aw86214, (unsigned char)databuf[0],
					   aw86214->seq[databuf[0]]);
		mutex_unlock(&aw86214->lock);
	}
	return count;
}

static ssize_t aw86214_loop_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);
	size_t count = 0;
	unsigned char i = 0;
	unsigned char reg_val[AW86214_SEQUENCER_LOOP_SIZE] = {0};

	aw86214_i2c_read(aw86214, AW86214_REG_WAVCFG9, reg_val,
			 AW86214_SEQUENCER_LOOP_SIZE);

	for (i = 0; i < AW86214_SEQUENCER_LOOP_SIZE; i++) {
		aw86214->loop[i*2+0] = (reg_val[i] >> 4) & 0x0F;
		aw86214->loop[i*2+1] = (reg_val[i] >> 0) & 0x0F;

		count += snprintf(buf+count, PAGE_SIZE-count,
			"seq%d_loop: 0x%02x\n", i*2+1, aw86214->loop[i*2+0]);
		count += snprintf(buf+count, PAGE_SIZE-count,
			"seq%d_loop: 0x%02x\n", i*2+2, aw86214->loop[i*2+1]);
	}
	return count;
}

static ssize_t aw86214_loop_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);
	unsigned int databuf[2] = { 0, 0 };

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		aw_dev_info("%s: seq%d loop=0x%02X\n", __func__,
			    databuf[0], databuf[1]);
		mutex_lock(&aw86214->lock);
		aw86214->loop[databuf[0]] = (unsigned char)databuf[1];
		aw86214_haptic_set_wav_loop(aw86214, (unsigned char)databuf[0],
					    aw86214->loop[databuf[0]]);
		mutex_unlock(&aw86214->lock);
	}
	return count;
}

static ssize_t aw86214_state_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", aw86214->state);
}

static ssize_t aw86214_state_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	return count;
}

static ssize_t aw86214_activate_mode_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);

	return snprintf(buf, PAGE_SIZE, "%d\n",
			aw86214->activate_mode);
}

static ssize_t aw86214_activate_mode_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;
	mutex_lock(&aw86214->lock);
	aw86214->activate_mode = val;
	mutex_unlock(&aw86214->lock);
	return count;
}

static ssize_t aw86214_index_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);
	unsigned char reg_val = 0;

	aw86214_i2c_read(aw86214, AW86214_REG_WAVCFG1, &reg_val,
			 AW_I2C_BYTE_ONE);
	aw86214->index = reg_val;
	return snprintf(buf, PAGE_SIZE, "%d\n", aw86214->index);
}

static ssize_t aw86214_index_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;
	if (val > aw86214->ram.ram_num) {
		aw_dev_err("%s: input value out of range!\n", __func__);
		return count;
	}
	aw_dev_info("%s: value=%d\n", __func__, val);
	mutex_lock(&aw86214->lock);
	aw86214->index = val;
	aw86214_haptic_set_repeat_wav_seq(aw86214, aw86214->index);
	mutex_unlock(&aw86214->lock);
	return count;
}

static ssize_t aw86214_gain_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);

	return snprintf(buf, PAGE_SIZE, "0x%02x\n", aw86214->gain);
}

static ssize_t aw86214_gain_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;

	aw_dev_info("%s: value=%d\n", __func__, val);
	if (val >= 0x80)
		val = 0x80;
	mutex_lock(&aw86214->lock);
	aw86214->gain = val;
	aw86214_haptic_set_gain(aw86214, aw86214->gain);
	mutex_unlock(&aw86214->lock);
	return count;
}

static ssize_t aw86214_ram_update_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	int cnt = 0;
	int i = 0;
	int size = 0;
	ssize_t len = 0;
	unsigned char ram_data[AW_RAMDATA_RD_BUFFER_SIZE] = {0};
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);

	/* RAMINIT Enable */
	aw86214_haptic_raminit(aw86214, true);
	aw86214_haptic_stop_l(aw86214);
	aw86214_set_ram_addr(aw86214);
	len += snprintf(buf + len, PAGE_SIZE - len,
			"haptic_ram len = %d\n", aw86214->ram.len);
	while (cnt < aw86214->ram.len) {
		if ((aw86214->ram.len - cnt) < AW_RAMDATA_RD_BUFFER_SIZE)
			size = aw86214->ram.len - cnt;
		else
			size = AW_RAMDATA_RD_BUFFER_SIZE;

		aw86214_i2c_read(aw86214, AW86214_REG_RAMDATA,
				 ram_data, size);
		for (i = 0; i < size; i++) {
			len += snprintf(buf + len, PAGE_SIZE - len,
					"0x%02X,", ram_data[i]);
		}
		cnt += size;
	}
	len += snprintf(buf + len, PAGE_SIZE - len, "\n");
	/* RAMINIT Disable */
	aw86214_haptic_raminit(aw86214, false);
	return len;
}

static ssize_t aw86214_ram_update_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;
	if (val)
		aw86214_ram_update(aw86214);
	return count;
}

static ssize_t aw86214_f0_save_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "f0_cali_data = 0x%02X\n",
			aw86214->f0_cali_data);

	return len;
}

static ssize_t aw86214_f0_save_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;
	aw86214->f0_cali_data = val;
	return count;
}

static ssize_t aw86214_ram_vbat_compensate_show(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);
	ssize_t len = 0;

	len +=
	    snprintf(buf + len, PAGE_SIZE - len, "ram_vbat_compensate = %d\n",
		     aw86214->ram_vbat_compensate);

	return len;
}

static ssize_t aw86214_ram_vbat_compensate_store(struct device *dev,
						 struct device_attribute *attr,
						 const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;

	mutex_lock(&aw86214->lock);
	if (val)
		aw86214->ram_vbat_compensate =
		    AW86214_HAPTIC_RAM_VBAT_COMP_ENABLE;
	else
		aw86214->ram_vbat_compensate =
		    AW86214_HAPTIC_RAM_VBAT_COMP_DISABLE;
	mutex_unlock(&aw86214->lock);

	return count;
}

static ssize_t aw86214_cali_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);
	ssize_t len = 0;

	mutex_lock(&aw86214->lock);
	aw86214_haptic_upload_lra(aw86214, AW86214_F0_CALI);
	aw86214_haptic_cont_get_f0(aw86214);
	mutex_unlock(&aw86214->lock);
	len += snprintf(buf + len, PAGE_SIZE - len,
			"%d\n", aw86214->f0);
	return len;
}

static ssize_t aw86214_cali_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);
	unsigned int val = 0;
	int i;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;
	if (val) {
		mutex_lock(&aw86214->lock);
		aw86214_haptic_upload_lra(aw86214, AW86214_WRITE_ZERO);
		for (i = 0; i < 2; i++) {
			aw86214_haptic_f0_calibration(aw86214);
			usleep_range(20000, 20500);
		}
		mutex_unlock(&aw86214->lock);
	}
	return count;
}

static ssize_t aw86214_cont_wait_num_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len,
			"cont_wait_num = 0x%02X\n", aw86214->cont_wait_num);
	return len;
}

static ssize_t aw86214_cont_wait_num_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	unsigned char reg_val = 0;
	int err, val;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);

	err = kstrtoint(buf, 16, &val);
	if (err != 0) {
		aw_dev_err("%s format not match!", __func__);
		return count;
	}
	aw86214->cont_wait_num = val;
	reg_val = aw86214->cont_wait_num;
	aw86214_i2c_write(aw86214, AW86214_REG_CONTCFG4, &reg_val,
			  AW_I2C_BYTE_ONE);
	return count;
}

static ssize_t aw86214_cont_drv_lvl_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len,
			"cont_drv1_lvl = 0x%02X, cont_drv2_lvl = 0x%02X\n",
			aw86214->cont_drv1_lvl, aw86214->cont_drv2_lvl);
	return len;
}

static ssize_t aw86214_cont_drv_lvl_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);
	unsigned char reg_val = 0;
	unsigned int databuf[2] = { 0, 0 };

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		aw86214->cont_drv1_lvl = databuf[0];
		aw86214->cont_drv2_lvl = databuf[1];
		aw86214_i2c_write_bits(aw86214, AW86214_REG_CONTCFG6,
				       AW86214_BIT_CONTCFG6_DRV1_LVL_MASK,
				       aw86214->cont_drv1_lvl);
		reg_val = (unsigned char)aw86214->cont_drv2_lvl;
		aw86214_i2c_write(aw86214, AW86214_REG_CONTCFG7, &reg_val,
				  AW_I2C_BYTE_ONE);
	}
	return count;
}

static ssize_t aw86214_cont_drv_time_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len,
			"cont_drv1_time = 0x%02X, cont_drv2_time = 0x%02X\n",
			aw86214->cont_drv1_time, aw86214->cont_drv2_time);
	return len;
}

static ssize_t aw86214_cont_drv_time_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);
	unsigned char reg_val[2] = {0};
	unsigned int databuf[2] = { 0, 0 };

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		aw86214->cont_drv1_time = databuf[0];
		aw86214->cont_drv2_time = databuf[1];
		reg_val[0] = (unsigned char)aw86214->cont_drv1_time;
		reg_val[1] = (unsigned char)aw86214->cont_drv2_time;
		aw86214_i2c_write(aw86214, AW86214_REG_CONTCFG8, reg_val,
				  AW_I2C_BYTE_TWO);
	}
	return count;
}

static ssize_t aw86214_vbat_monitor_show(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);
	ssize_t len = 0;

	mutex_lock(&aw86214->lock);
	aw86214_haptic_get_vbat(aw86214);
	len += snprintf(buf + len, PAGE_SIZE - len, "vbat_monitor = %d\n",
			aw86214->vbat);
	mutex_unlock(&aw86214->lock);

	return len;
}

static ssize_t aw86214_vbat_monitor_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	return count;
}

static ssize_t aw86214_lra_resistance_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);
	ssize_t len = 0;

	aw86214_haptic_get_lra_resistance(aw86214);
	len += snprintf(buf + len, PAGE_SIZE - len, "lra_resistance = %d\n",
			aw86214->lra);
	return len;
}

static ssize_t aw86214_lra_resistance_store(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t count)
{
	return count;
}

static ssize_t aw86214_prctmode_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);
	ssize_t len = 0;
	unsigned char reg_val = 0;

	aw86214_i2c_read(aw86214, AW86214_REG_DETCFG1, &reg_val,
			 AW_I2C_BYTE_ONE);

	len += snprintf(buf + len, PAGE_SIZE - len, "prctmode = %d\n",
			reg_val & 0x08);
	return len;
}

static ssize_t aw86214_prctmode_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);
	unsigned int databuf[2] = { 0, 0 };
	unsigned int addr = 0;
	unsigned int val = 0;

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		addr = databuf[0];
		val = databuf[1];
		mutex_lock(&aw86214->lock);
		aw86214_haptic_swicth_motor_protect_config(aw86214, addr, val);
		mutex_unlock(&aw86214->lock);
	}
	return count;
}

static ssize_t aw86214_gun_type_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);

	return snprintf(buf, PAGE_SIZE, "0x%02x\n", aw86214->gun_type);

}

static ssize_t aw86214_gun_type_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;
	aw_dev_info("%s: value=%d\n", __func__, val);
	mutex_lock(&aw86214->lock);
	aw86214->gun_type = val;
	mutex_unlock(&aw86214->lock);
	return count;
}

static ssize_t aw86214_bullet_nr_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);

	return snprintf(buf, PAGE_SIZE, "0x%02x\n", aw86214->bullet_nr);
}

static ssize_t aw86214_bullet_nr_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);
	unsigned int val = 0;
	int rc = 0;

	rc = kstrtouint(buf, 0, &val);
	if (rc < 0)
		return count;
	aw_dev_info("%s: value=%d\n", __func__, val);
	mutex_lock(&aw86214->lock);
	aw86214->bullet_nr = val;
	mutex_unlock(&aw86214->lock);
	return count;
}

static ssize_t aw86214_haptic_audio_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);
	ssize_t len = 0;

	len += snprintf(buf+len, PAGE_SIZE-len,
			"%d\n", aw86214->haptic_audio.ctr.cnt);
	return len;
}

static int aw86214_haptic_audio_config(struct aw86214 *aw86214,
				      unsigned int databuf[])
{
	struct haptic_ctr *hap_ctr = NULL;

	hap_ctr = kzalloc(sizeof(struct haptic_ctr), GFP_KERNEL);
	if (hap_ctr == NULL)
		return -ENOMEM;
	mutex_lock(&aw86214->haptic_audio.lock);
	hap_ctr->cnt = (unsigned char)databuf[0];
	hap_ctr->cmd = (unsigned char)databuf[1];
	hap_ctr->play = (unsigned char)databuf[2];
	hap_ctr->wavseq = (unsigned char)databuf[3];
	hap_ctr->loop = (unsigned char)databuf[4];
	hap_ctr->gain = (unsigned char)databuf[5];
	aw86214_haptic_audio_ctr_list_insert(&aw86214->haptic_audio,
					hap_ctr, aw86214->dev);
	if (hap_ctr->cmd == 0xff) {
		aw_dev_info("%s: haptic_audio stop\n", __func__);
		if (hrtimer_active(&aw86214->haptic_audio.timer)) {
			aw_dev_info("%s: cancel haptic_audio_timer\n",
				    __func__);
			hrtimer_cancel(&aw86214->haptic_audio.timer);
			aw86214->haptic_audio.ctr.cnt = 0;
			aw86214_haptic_audio_off(aw86214);
		}
	} else {
		if (hrtimer_active(&aw86214->haptic_audio.timer)) {
		} else {
			aw_dev_info("%s: start haptic_audio_timer\n",
				    __func__);
			aw86214_haptic_audio_init(aw86214);
			hrtimer_start(&aw86214->haptic_audio.timer,
			ktime_set(aw86214->haptic_audio.delay_val/1000000,
				(aw86214->haptic_audio.delay_val%1000000)*1000),
			HRTIMER_MODE_REL);
		}
	}
	mutex_unlock(&aw86214->haptic_audio.lock);
	kfree(hap_ctr);
	return 0;
}

static ssize_t aw86214_haptic_audio_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);
	unsigned int databuf[6] = {0};

	if (!aw86214->ram_init)
		return count;

	if (sscanf(buf, "%d %d %d %d %d %d",
		&databuf[0], &databuf[1], &databuf[2],
		&databuf[3], &databuf[4], &databuf[5]) == 6) {
		if (databuf[2]) {
			aw_dev_dbg("%s: cnt=%d, cmd=%d, play=%d\n", __func__,
				   databuf[0], databuf[1], databuf[2]);
			aw_dev_dbg("%s: wavseq=%d, loop=%d, gain=%d\n",
				   __func__, databuf[3], databuf[4],
				   databuf[5]);
			aw86214_haptic_audio_config(aw86214, databuf);
		}

	}
	return count;
}

static ssize_t aw86214_ram_num_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);
	ssize_t len = 0;

	aw86214_haptic_get_ram_number(aw86214);
	len += snprintf(buf+len, PAGE_SIZE-len,
			"ram_num = %d\n", aw86214->ram.ram_num);
	return len;
}

static ssize_t aw86214_awrw_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);
	unsigned char i = 0;
	unsigned char reg_num = aw86214->aw_i2c_package.reg_num;
	unsigned char flag = aw86214->aw_i2c_package.flag;
	unsigned char *reg_data = aw86214->aw_i2c_package.reg_data;
	ssize_t len = 0;

	if (!reg_num) {
		aw_dev_err("%s: awrw parameter error\n", __func__);
		return len;
	}
	if (flag == AW86214_READ) {
		for (i = 0; i < reg_num; i++) {
			len += snprintf(buf + len, PAGE_SIZE - len,
					"0x%02x,", reg_data[i]);
		}

		len += snprintf(buf + len - 1, PAGE_SIZE - len, "\n");
	}

	return len;
}

static ssize_t aw86214_awrw_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	unsigned int datatype[3] = { 0 };
	int ret = 0;
	cdev_t *cdev = dev_get_drvdata(dev);
	struct aw86214 *aw86214 = container_of(cdev, struct aw86214, vib_dev);

	if (sscanf(buf, "%x %x %x", &datatype[0], &datatype[1],
				    &datatype[2]) == 3) {
		if (!datatype[1]) {
			aw_dev_err("%s: awrw parameter error\n", __func__);
			return count;
		}
		aw86214->aw_i2c_package.flag = (unsigned char)datatype[0];
		aw86214->aw_i2c_package.reg_num = (unsigned char)datatype[1];
		aw86214->aw_i2c_package.first_addr = (unsigned char)datatype[2];

		if (aw86214->aw_i2c_package.flag == AW86214_WRITE) {
			ret = aw86214_parse_data(aw86214, buf);
			if (ret < 0)
				return count;
			aw86214_i2c_write(aw86214,
					 aw86214->aw_i2c_package.first_addr,
					 aw86214->aw_i2c_package.reg_data,
					 aw86214->aw_i2c_package.reg_num);
		}
		if (aw86214->aw_i2c_package.flag == AW86214_READ)
			aw86214_i2c_read(aw86214,
					aw86214->aw_i2c_package.first_addr,
					aw86214->aw_i2c_package.reg_data,
					aw86214->aw_i2c_package.reg_num);
	} else
		aw_dev_err("%s: missing number of parameters\n", __func__);

	return count;
}

static DEVICE_ATTR(f0, 0644, aw86214_f0_show, aw86214_f0_store);
static DEVICE_ATTR(cont, 0644, aw86214_cont_show, aw86214_cont_store);
static DEVICE_ATTR(reg, 0644, aw86214_reg_show, aw86214_reg_store);
static DEVICE_ATTR(duration, 0644, aw86214_duration_show,
		   aw86214_duration_store);
static DEVICE_ATTR(index, 0644, aw86214_index_show, aw86214_index_store);
static DEVICE_ATTR(activate, 0644, aw86214_activate_show,
		   aw86214_activate_store);
static DEVICE_ATTR(activate_mode, 0644, aw86214_activate_mode_show,
		   aw86214_activate_mode_store);
static DEVICE_ATTR(seq, 0644, aw86214_seq_show, aw86214_seq_store);
static DEVICE_ATTR(loop, 0644, aw86214_loop_show, aw86214_loop_store);
static DEVICE_ATTR(state, 0644, aw86214_state_show, aw86214_state_store);
static DEVICE_ATTR(gain, 0644, aw86214_gain_show, aw86214_gain_store);
static DEVICE_ATTR(ram_update, 0644, aw86214_ram_update_show,
		   aw86214_ram_update_store);
static DEVICE_ATTR(f0_save, 0644, aw86214_f0_save_show, aw86214_f0_save_store);
static DEVICE_ATTR(ram_vbat_comp, 0644, aw86214_ram_vbat_compensate_show,
		   aw86214_ram_vbat_compensate_store);
static DEVICE_ATTR(cali, 0644, aw86214_cali_show, aw86214_cali_store);
static DEVICE_ATTR(cont_wait_num, 0644, aw86214_cont_wait_num_show,
		   aw86214_cont_wait_num_store);
static DEVICE_ATTR(cont_drv_lvl, 0644, aw86214_cont_drv_lvl_show,
		   aw86214_cont_drv_lvl_store);
static DEVICE_ATTR(cont_drv_time, 0644, aw86214_cont_drv_time_show,
		   aw86214_cont_drv_time_store);
static DEVICE_ATTR(vbat_monitor, 0644, aw86214_vbat_monitor_show,
		   aw86214_vbat_monitor_store);
static DEVICE_ATTR(lra_resistance, 0644, aw86214_lra_resistance_show,
		   aw86214_lra_resistance_store);
static DEVICE_ATTR(prctmode, 0644, aw86214_prctmode_show,
		   aw86214_prctmode_store);
static DEVICE_ATTR(gun_type, 0644, aw86214_gun_type_show,
		   aw86214_gun_type_store);
static DEVICE_ATTR(bullet_nr, 0644, aw86214_bullet_nr_show,
		   aw86214_bullet_nr_store);
static DEVICE_ATTR(haptic_audio, 0644, aw86214_haptic_audio_show,
		   aw86214_haptic_audio_store);
static DEVICE_ATTR(ram_num, 0644, aw86214_ram_num_show, NULL);
static DEVICE_ATTR(awrw, 0644, aw86214_awrw_show, aw86214_awrw_store);
static struct attribute *aw86214_vibrator_attributes[] = {
	&dev_attr_state.attr,
	&dev_attr_duration.attr,
	&dev_attr_activate.attr,
	&dev_attr_activate_mode.attr,
	&dev_attr_index.attr,
	&dev_attr_gain.attr,
	&dev_attr_seq.attr,
	&dev_attr_loop.attr,
	&dev_attr_reg.attr,
	&dev_attr_ram_update.attr,
	&dev_attr_f0.attr,
	&dev_attr_f0_save.attr,
	&dev_attr_cali.attr,
	&dev_attr_cont.attr,
	&dev_attr_cont_wait_num.attr,
	&dev_attr_cont_drv_lvl.attr,
	&dev_attr_cont_drv_time.attr,
	&dev_attr_vbat_monitor.attr,
	&dev_attr_lra_resistance.attr,
	&dev_attr_prctmode.attr,
	&dev_attr_ram_vbat_comp.attr,
	&dev_attr_gun_type.attr,
	&dev_attr_bullet_nr.attr,
	&dev_attr_haptic_audio.attr,
	&dev_attr_ram_num.attr,
	&dev_attr_awrw.attr,
	NULL
};

struct attribute_group aw86214_vibrator_attribute_group_l = {
	.attrs = aw86214_vibrator_attributes
};

static void aw86214_haptic_ram_play(struct aw86214 *aw86214, unsigned char type)
{
	aw_dev_info("%s index = %d\n", __func__, aw86214->index);

	mutex_lock(&aw86214->ram_lock);
	if (type == AW86214_HAPTIC_RAM_LOOP_MODE)
		aw86214_haptic_set_repeat_wav_seq(aw86214, aw86214->index);

	if (type == AW86214_HAPTIC_RAM_MODE) {
		aw86214_haptic_set_wav_seq(aw86214, 0x00, aw86214->index);
		aw86214_haptic_set_wav_seq(aw86214, 0x01, 0x00);
		aw86214_haptic_set_wav_loop(aw86214, 0x00, 0x00);
	}
	aw86214_haptic_play_mode(aw86214, type);
	aw86214_haptic_start(aw86214);
	mutex_unlock(&aw86214->ram_lock);
}

static void aw86214_vibrator_work_routine(struct work_struct *work)
{
	struct aw86214 *aw86214 = container_of(work, struct aw86214,
					       vibrator_work);

	aw_dev_info("%s enter\n", __func__);

	mutex_lock(&aw86214->lock);
	/* Enter standby mode */
	aw86214_haptic_stop_l(aw86214);
	aw86214_haptic_upload_lra(aw86214, AW86214_F0_CALI);
	if (aw86214->state) {
		if (aw86214->activate_mode == AW86214_HAPTIC_RAM_MODE) {
			aw86214_brk_en(aw86214, false);
			aw86214_haptic_ram_vbat_comp(aw86214, false);
			aw86214_haptic_ram_play(aw86214,
						AW86214_HAPTIC_RAM_MODE);
		} else if (aw86214->activate_mode ==
					AW86214_HAPTIC_RAM_LOOP_MODE) {
			aw86214_haptic_ram_vbat_comp(aw86214, true);
			aw86214_haptic_ram_play(aw86214,
					       AW86214_HAPTIC_RAM_LOOP_MODE);
			/* run ms timer */
			hrtimer_start(&aw86214->timer,
				      ktime_set(aw86214->duration / 1000,
				      (aw86214->duration % 1000) * 1000000),
				      HRTIMER_MODE_REL);
		} else if (aw86214->activate_mode ==
					AW86214_HAPTIC_CONT_MODE) {
			aw86214_haptic_cont(aw86214);
			/* run ms timer */
			hrtimer_start(&aw86214->timer,
				      ktime_set(aw86214->duration / 1000,
				      (aw86214->duration % 1000) * 1000000),
				      HRTIMER_MODE_REL);
		} else {
			 aw_dev_err("%s: activate_mode error\n",
				   __func__);
		}
	}
	mutex_unlock(&aw86214->lock);
}



int aw86214_vibrator_init_l(struct aw86214 *aw86214)
{
	int ret = 0;

	aw_dev_info("%s enter\n", __func__);

#ifdef TIMED_OUTPUT
	aw_dev_info("%s: TIMED_OUT FRAMEWORK!\n", __func__);
	aw86214->vib_dev.name = "awinic_vibrator_l";
	aw86214->vib_dev.get_time = aw86214_vibrator_get_time;
	aw86214->vib_dev.enable = aw86214_vibrator_enable;

	ret = timed_output_dev_register(&(aw86214->vib_dev));
	if (ret < 0) {
		aw_dev_err("%s: fail to create timed output dev\n", __func__);
		return ret;
	}
	ret = sysfs_create_group(&aw86214->vib_dev.dev->kobj,
				 &aw86214_vibrator_attribute_group_l);
	if (ret < 0) {
		aw_dev_err("%s error creating sysfs attr files\n",
			   __func__);
		return ret;
	}
#else
	aw_dev_info("%s: loaded in leds_cdev framework!\n",
		    __func__);
	aw86214->vib_dev.name = "awinic_vibrator_l";
	aw86214->vib_dev.brightness_get = aw86214_haptic_brightness_get;
	aw86214->vib_dev.brightness_set = aw86214_haptic_brightness_set;

	ret = devm_led_classdev_register(&aw86214->i2c->dev, &aw86214->vib_dev);
	if (ret < 0) {
		aw_dev_err("%s: fail to create led dev\n",  __func__);
		return ret;
	}
	ret = sysfs_create_group(&aw86214->vib_dev.dev->kobj,
				 &aw86214_vibrator_attribute_group_l);
	if (ret < 0) {
		aw_dev_err("%s error creating sysfs attr files\n", __func__);
		return ret;
	}
#endif
	hrtimer_init(&aw86214->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	aw86214->timer.function = aw86214_vibrator_timer_func;
	INIT_WORK(&aw86214->vibrator_work,
		  aw86214_vibrator_work_routine);

	mutex_init(&aw86214->lock);
	mutex_init(&aw86214->ram_lock);
	return 0;
}

int aw86214_haptic_init_l(struct aw86214 *aw86214)
{
	int ret = 0;
	unsigned char reg_val = 0;
	unsigned char reg_array[8] = {0};

	aw_dev_info("%s enter\n", __func__);
	/* haptic audio */
	aw86214->haptic_audio.delay_val = 1;
	aw86214->haptic_audio.timer_val = 21318;
	INIT_LIST_HEAD(&(aw86214->haptic_audio.ctr_list));
	hrtimer_init(&aw86214->haptic_audio.timer,
		CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	aw86214->haptic_audio.timer.function = aw86214_haptic_audio_timer_func;
	INIT_WORK(&aw86214->haptic_audio.work,
		aw86214_haptic_audio_work_routine);
	mutex_init(&aw86214->haptic_audio.lock);
	aw86214->gun_type = AW_GUN_TYPE_DEF_VAL;
	aw86214->bullet_nr = AW_BULLET_NR_DEF_VAL;
	/* haptic init */
	mutex_lock(&aw86214->lock);
	aw86214->activate_mode = aw86214->dts_info.mode;
	ret = aw86214_i2c_read(aw86214, AW86214_REG_WAVCFG1, &reg_val,
			       AW_I2C_BYTE_ONE);
	aw86214->index = reg_val & 0x7F;
	ret = aw86214_i2c_read(aw86214, AW86214_REG_PLAYCFG2, &reg_val,
			       AW_I2C_BYTE_ONE);
	aw86214->gain = reg_val & 0xFF;
	aw_dev_info("%s aw86214->gain =0x%02X\n", __func__, aw86214->gain);
	aw86214_i2c_read(aw86214, AW86214_REG_WAVCFG1, reg_array,
			AW86214_SEQUENCER_SIZE);
	memcpy(aw86214->seq, reg_array, AW86214_SEQUENCER_SIZE);
	aw86214_haptic_play_mode(aw86214, AW86214_HAPTIC_STANDBY_MODE);
	aw86214_haptic_set_pwm(aw86214, AW86214_PWM_12K);
	/* misc value init */
	aw86214_haptic_misc_para_init(aw86214);
	/* set motor protect */
	aw86214_haptic_swicth_motor_protect_config(aw86214, 0x00, 0x00);
	aw86214_haptic_offset_calibration(aw86214);
	/* vbat compensation */
	aw86214_haptic_vbat_mode_config(aw86214,
				AW86214_HAPTIC_CONT_VBAT_HW_ADJUST_MODE);
	aw86214->ram_vbat_compensate = AW86214_HAPTIC_RAM_VBAT_COMP_ENABLE;

	/* f0 calibration */
	/*LRA trim source select register*/
	aw86214_i2c_write_bits(aw86214,
				AW86214_REG_TRIMCFG1,
				AW86214_BIT_TRIMCFG1_RL_TRIM_SRC_MASK,
				AW86214_BIT_TRIMCFG1_RL_TRIM_SRC_REG);
	if (aw86214->dts_info.lk_f0_cali) {
		aw86214->f0_cali_data = aw86214->dts_info.lk_f0_cali;
		aw86214_haptic_upload_lra(aw86214, AW86214_F0_CALI);
	} else {
		aw86214_haptic_upload_lra(aw86214, AW86214_WRITE_ZERO);
		aw86214_haptic_f0_calibration(aw86214);
	}
	mutex_unlock(&aw86214->lock);
	return ret;
}

void aw86214_interrupt_setup_l(struct aw86214 *aw86214)
{
	unsigned char reg_val = 0;

	aw_dev_info("%s enter\n", __func__);

	aw86214_i2c_read(aw86214, AW86214_REG_SYSINT, &reg_val,
			 AW_I2C_BYTE_ONE);
	aw_dev_info("%s: reg SYSINT=0x%02X\n", __func__, reg_val);

	/* edge int mode */
	aw86214_i2c_write_bits(aw86214, AW86214_REG_SYSCTRL7,
			       (AW86214_BIT_SYSCTRL7_INT_MODE_MASK &
				AW86214_BIT_SYSCTRL7_INT_EDGE_MODE_MASK),
			       (AW86214_BIT_SYSCTRL7_INT_MODE_EDGE |
				AW86214_BIT_SYSCTRL7_INT_EDGE_MODE_POS));
	/* int enable */
	aw86214_i2c_write_bits(aw86214, AW86214_REG_SYSINTM,
			       (AW86214_BIT_SYSINTM_UVLM_MASK &
				AW86214_BIT_SYSINTM_OCDM_MASK &
				AW86214_BIT_SYSINTM_OTM_MASK),
			       (AW86214_BIT_SYSINTM_UVLM_ON |
				AW86214_BIT_SYSINTM_OCDM_ON |
				AW86214_BIT_SYSINTM_OTM_ON));
}

char aw86214_check_qualify_l(struct aw86214 *aw86214)
{
	unsigned char reg = 0;
	int ret = 0;

	ret = aw86214_i2c_read(aw86214, AW86214_REG_EFCFG5, &reg,
			       AW_I2C_BYTE_ONE);
	if (ret < 0) {
		aw_dev_err("%s: failed to read register 0x64: %d\n",
			   __func__, ret);
		return ret;
	}
	if ((reg & 0x80) == 0x80)
		return 1;

	return 0;
}
