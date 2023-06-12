/*
 * aw_init.c   aw882xx codec module
 *
 *
 * Copyright (c) 2019 AWINIC Technology CO., LTD
 *
 *  Author: Nick Li <liweilei@awinic.com.cn>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

/* #define DEBUG */

#include <linux/module.h>
#include <linux/i2c.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/debugfs.h>
#include <linux/version.h>
#include <linux/workqueue.h>
#include <linux/syscalls.h>
#include <sound/control.h>
#include <linux/uaccess.h>

#include "aw882xx.h"
#include "aw_pid_1852_reg.h"
#include "aw_pid_2013_reg.h"
#include "aw_pid_2032_reg.h"
#include "aw_pid_2055_reg.h"
#include "aw_pid_2071_reg.h"
#include "aw_pid_2113_reg.h"
#include "aw_log.h"

int aw882xx_dev_i2c_write_bits(struct aw_device *aw_dev,
	unsigned char reg_addr, unsigned int mask, unsigned int reg_data)
{
	struct aw882xx *aw882xx = (struct aw882xx *)aw_dev->private_data;

	return aw882xx_i2c_write_bits(aw882xx, reg_addr, mask, reg_data);
}

int aw882xx_dev_i2c_write(struct aw_device *aw_dev,
	unsigned char reg_addr, unsigned int reg_data)
{
	struct aw882xx *aw882xx = (struct aw882xx *)aw_dev->private_data;

	return aw882xx_i2c_write(aw882xx, reg_addr, reg_data);
}

int aw882xx_dev_i2c_read(struct aw_device *aw_dev,
	unsigned char reg_addr, unsigned int *reg_data)
{
	struct aw882xx *aw882xx = (struct aw882xx *)aw_dev->private_data;

	return aw882xx_i2c_read(aw882xx, reg_addr, reg_data);
}

void aw882xx_dev_set_algo_en(struct aw_device *aw_dev)
{
	struct aw882xx *aw882xx = (struct aw882xx *)aw_dev->private_data;

	aw882xx_kcontorl_set(aw882xx);
}

/* [7 : 4]: -6DB ; [3 : 0]: 0.5DB  real_value = value * 2 : 0.5db --> 1 */
static unsigned int aw_pid_1852_reg_val_to_db(unsigned int value)
{
	return ((value >> 4) * AW_PID_1852_VOL_STEP_DB + (value & 0x0f));
}

/* [7 : 4]: -6DB ; [3 : 0]: -0.5DB reg_value = value / step << 4 + value % step ; step = 6 * 2 */
static unsigned int aw_pid_1852_db_val_to_reg(unsigned int value)
{
	return (((value / AW_PID_1852_VOL_STEP_DB) << 4) + (value % AW_PID_1852_VOL_STEP_DB));
}

static int aw_pid_1852_set_volume(struct aw_device *aw_dev, unsigned int value)
{
	struct aw882xx *aw882xx = (struct aw882xx *)aw_dev->private_data;
	unsigned int reg_value = 0;
	unsigned int real_value = aw_pid_1852_db_val_to_reg(value);

	/* cal real value */
	aw882xx_i2c_read(aw882xx, AW_PID_1852_HAGCCFG4_REG, &reg_value);

	aw_dev_dbg(aw882xx->dev, "value %d , 0x%x", value, real_value);

	/* 15 : 8] volume */
	real_value = (real_value << 8) | (reg_value & 0x00ff);

	/* write value */
	aw882xx_i2c_write(aw882xx, AW_PID_1852_HAGCCFG4_REG, real_value);
	return 0;
}

static int aw_pid_1852_get_volume(struct aw_device *aw_dev, unsigned int *value)
{
	unsigned int reg_value = 0;
	unsigned int real_value = 0;
	struct aw882xx *aw882xx = (struct aw882xx *)aw_dev->private_data;

	/* read value */
	aw882xx_i2c_read(aw882xx, AW_PID_1852_HAGCCFG4_REG, &reg_value);

	/* [15 : 8] volume */
	real_value = reg_value >> 8;

	real_value = aw_pid_1852_reg_val_to_db(real_value);
	*value = real_value;

	return 0;
}

static void aw_pid_1852_i2s_enable(struct aw_device *aw_dev, bool flag)
{
	struct aw882xx *aw882xx = (struct aw882xx *)aw_dev->private_data;
	struct aw_cali_desc *desc = &aw_dev->cali_desc;

	aw_dev_dbg(aw882xx->dev, "enter");

	if (!desc->mode) {
		aw_dev_dbg(aw_dev->dev, "cali mode is none, needn't set i2s status");
		return;
	}

	if (flag) {
		aw882xx_i2c_write_bits(aw882xx, AW_PID_1852_I2SCFG1_REG,
				AW_PID_1852_I2STXEN_MASK,
				AW_PID_1852_I2STXEN_ENABLE_VALUE);
	} else {
		aw882xx_i2c_write_bits(aw882xx, AW_PID_1852_I2SCFG1_REG,
				AW_PID_1852_I2STXEN_MASK,
				AW_PID_1852_I2STXEN_DISABLE_VALUE);
	}
}

static bool aw_pid_1852_check_rd_access(int reg)
{
	if (reg >= AW_PID_1852_REG_MAX)
		return false;


	if (aw_pid_1852_reg_access[reg] & AW_PID_1852_REG_RD_ACCESS)
		return true;
	else
		return false;
}

static bool aw_pid_1852_check_wr_access(int reg)
{
	if (reg >= AW_PID_1852_REG_MAX)
		return false;


	if (aw_pid_1852_reg_access[reg] & AW_PID_1852_REG_WR_ACCESS)
		return true;
	else
		return false;
}

static int aw_pid_1852_get_reg_num(void)
{
	return AW_PID_1852_REG_MAX;
}

static unsigned int aw_pid_1852_get_irq_type(struct aw_device *aw_dev,
				unsigned int value)
{
	unsigned int ret = INT_TYPE_NONE;
	/* UVL0 */
	if (value & (~AW_PID_1852_UVLI_MASK)) {
		aw_dev_info(aw_dev->dev, "UVLO: occur");
		ret |= INT_TYPE_UVLO;
	}

	/* BSTOCM */
	if (value & (~AW_PID_1852_BSTOCI_MASK)) {
		aw_dev_info(aw_dev->dev, "BSTOCI: occur");
		ret |= INT_TYPE_BSTOC;
	}

	/* OCDI */
	if (value & (~AW_PID_1852_OCDI_MASK)) {
		aw_dev_info(aw_dev->dev, "OCDI: occur");
		ret |= INT_TYPE_OCDI;
	}

	/* OTHI */
	if (value & (~AW_PID_1852_OTHI_MASK)) {
		aw_dev_info(aw_dev->dev, "OTHI: occur");
		ret |= INT_TYPE_OTHI;
	}

	return ret;
}

static int aw_pid_1852_dev_init(struct aw882xx *aw882xx, int index)
{
	int ret;
	struct aw_device *aw_pa = NULL;

	aw_pa = devm_kzalloc(aw882xx->dev, sizeof(struct aw_device), GFP_KERNEL);
	if (aw_pa == NULL) {
		aw_dev_err(aw882xx->dev, "dev kalloc failed");
		return -ENOMEM;
	}

	/* call aw device init func */
	memset(aw_pa->acf_name, 0, AW_NAME_MAX);
	memcpy(aw_pa->acf_name, AW_PID_1852_ACF_FILE, strlen(AW_PID_1852_ACF_FILE));
	memset(aw_pa->monitor_name, 0, AW_NAME_MAX);
	memcpy(aw_pa->monitor_name, AW_PID_1852_MONITOR_FILE, strlen(AW_PID_1852_MONITOR_FILE));

	aw_pa->prof_info.prof_desc = NULL;
	aw_pa->prof_info.count = 0;
	aw_pa->channel = 0;
	aw_pa->bstcfg_enable = AW_BSTCFG_DISABLE;
	aw_pa->bop_en = AW_BOP_DISABLE;
	aw_pa->vol_step = AW_PID_1852_VOL_STEP;

	aw_pa->index = index;
	aw_pa->private_data = (void *)aw882xx;
	aw_pa->dev = aw882xx->dev;
	aw_pa->i2c = aw882xx->i2c;
	aw_pa->ops.aw_get_version = aw882xx_get_version;
	aw_pa->ops.aw_get_dev_num = aw882xx_get_dev_num;
	aw_pa->ops.aw_set_algo = aw882xx_dev_set_algo_en;
	aw_pa->ops.aw_i2c_read = aw882xx_dev_i2c_read;
	aw_pa->ops.aw_i2c_write = aw882xx_dev_i2c_write;
	aw_pa->ops.aw_i2c_write_bits = aw882xx_dev_i2c_write_bits;
	aw_pa->ops.aw_get_volume = aw_pid_1852_get_volume;
	aw_pa->ops.aw_set_volume = aw_pid_1852_set_volume;
	aw_pa->ops.aw_reg_val_to_db = aw_pid_1852_reg_val_to_db;
	aw_pa->ops.aw_i2s_enable = aw_pid_1852_i2s_enable;
	aw_pa->ops.aw_check_rd_access = aw_pid_1852_check_rd_access;
	aw_pa->ops.aw_check_wr_access = aw_pid_1852_check_wr_access;
	aw_pa->ops.aw_get_reg_num = aw_pid_1852_get_reg_num;
	aw_pa->ops.aw_get_irq_type = aw_pid_1852_get_irq_type;

	aw_pa->int_desc.mask_reg = AW_PID_1852_SYSINTM_REG;
	aw_pa->int_desc.mask_default = AW_PID_1852_SYSINTM_DEFAULT;
	aw_pa->int_desc.int_mask = AW_PID_1852_SYSINTM_DEFAULT;
	aw_pa->int_desc.st_reg = AW_PID_1852_SYSINT_REG;

	aw_pa->pwd_desc.reg = AW_PID_1852_SYSCTRL_REG;
	aw_pa->pwd_desc.mask = AW_PID_1852_PWDN_MASK;
	aw_pa->pwd_desc.enable = AW_PID_1852_PWDN_POWER_DOWN_VALUE;
	aw_pa->pwd_desc.disable = AW_PID_1852_PWDN_NORMAL_WORKING_VALUE;

	aw_pa->amppd_desc.reg = AW_PID_1852_SYSCTRL_REG;
	aw_pa->amppd_desc.mask = AW_PID_1852_AMPPD_MASK;
	aw_pa->amppd_desc.enable = AW_PID_1852_AMPPD_POWER_DOWN_VALUE;
	aw_pa->amppd_desc.disable = AW_PID_1852_AMPPD_NORMAL_WORKING_VALUE;

	aw_pa->mute_desc.reg = AW_PID_1852_SYSCTRL2_REG;
	aw_pa->mute_desc.mask = AW_PID_1852_HMUTE_MASK;
	aw_pa->mute_desc.enable = AW_PID_1852_HMUTE_ENABLE_VALUE;
	aw_pa->mute_desc.disable = AW_PID_1852_HMUTE_DISABLE_VALUE;

	aw_pa->vcalb_desc.vcalb_reg = AW_PID_1852_VTMCTRL3_REG;
	aw_pa->vcalb_desc.vcal_factor = AW_PID_1852_VCAL_FACTOR;
	aw_pa->vcalb_desc.cabl_base_value = AW_PID_1852_CABL_BASE_VALUE;

	aw_pa->vcalb_desc.icalk_value_factor = AW_PID_1852_ICABLK_FACTOR;
	aw_pa->vcalb_desc.icalk_reg = AW_PID_1852_EFRM1_REG;
	aw_pa->vcalb_desc.icalkl_reg = AW_REG_NONE;
	aw_pa->vcalb_desc.icalk_reg_mask = AW_PID_1852_EF_ISN_GESLP_MASK;
	aw_pa->vcalb_desc.icalk_sign_mask = AW_PID_1852_EF_ISN_GESLP_SIGN_MASK;
	aw_pa->vcalb_desc.icalk_neg_mask = AW_PID_1852_EF_ISN_GESLP_NEG;

	aw_pa->vcalb_desc.vcalk_reg = AW_PID_1852_EFRH_REG;
	aw_pa->vcalb_desc.vcalk_reg_mask = AW_PID_1852_EF_VSN_GESLP_MASK;
	aw_pa->vcalb_desc.vcalkl_reg = AW_REG_NONE;
	aw_pa->vcalb_desc.vcalk_sign_mask = AW_PID_1852_EF_VSN_GESLP_SIGN_MASK;
	aw_pa->vcalb_desc.vcalk_neg_mask = AW_PID_1852_EF_VSN_GESLP_NEG;
	aw_pa->vcalb_desc.vcalk_value_factor = AW_PID_1852_VCABLK_FACTOR;

	aw_pa->sysst_desc.reg = AW_PID_1852_SYSST_REG;
	aw_pa->sysst_desc.mask = AW_PID_1852_SYSST_CHECK_MASK;
	aw_pa->sysst_desc.st_check = AW_PID_1852_SYSST_CHECK;
	aw_pa->sysst_desc.pll_check = AW_PID_1852_PLLS_LOCKED_VALUE;

	aw_pa->cco_mux_desc.reg = AW_PID_1852_PLLCTRL1_REG;
	aw_pa->cco_mux_desc.mask = AW_PID_1852_I2S_CCO_MUX_MASK;
	aw_pa->cco_mux_desc.divided_val = AW_PID_1852_I2S_CCO_MUX_8_16_32KHZ_VALUE;
	aw_pa->cco_mux_desc.bypass_val = AW_PID_1852_I2S_CCO_MUX_EXC_8_16_32KHZ_VALUE;

	aw_pa->voltage_desc.reg = AW_PID_1852_VBAT_REG;
	aw_pa->voltage_desc.int_bit = AW_PID_1852_MONITOR_INT_10BIT;
	aw_pa->voltage_desc.vbat_range = AW_PID_1852_MONITOR_VBAT_RANGE;

	aw_pa->temp_desc.reg = AW_PID_1852_TEMP_REG;
	aw_pa->temp_desc.neg_mask = AW_PID_1852_MONITOR_TEMP_NEG_MASK;
	aw_pa->temp_desc.sign_mask = AW_PID_1852_MONITOR_TEMP_SIGN_MASK;

	aw_pa->ipeak_desc.reg = AW_PID_1852_SYSCTRL2_REG;
	aw_pa->ipeak_desc.mask = AW_PID_1852_BST_IPEAK_MASK;

	aw_pa->volume_desc.reg = AW_PID_1852_HAGCCFG4_REG;
	aw_pa->volume_desc.mask = AW_PID_1852_VOL_MASK;
	aw_pa->volume_desc.shift = AW_PID_1852_VOL_START_BIT;
	aw_pa->volume_desc.mute_volume = AW_PID_1852_MUTE_VOL;

	aw_pa->bop_desc.reg = AW_REG_NONE;

	aw_pa->soft_rst.reg = AW_PID_1852_ID_REG;
	aw_pa->soft_rst.reg_value = AW882XX_SOFT_RESET_VALUE;

	ret = aw_device_probe(aw_pa);

	aw882xx->aw_pa = aw_pa;
	return ret;
}

/******************************************************
 *
 * A2013 init
 *
 ******************************************************/
/*[9 : 6]: -6DB ; [5 : 0]: 0.125DB  real_value = value * 8 : 0.125db --> 1*/
static unsigned int aw_pid_2013_reg_val_to_db(unsigned int value)
{
	return ((value >> 6) * AW_PID_2013_VOL_STEP_DB + (value & 0x003f));
}

/*[9 : 6]: -6DB ; [5 : 0]: -0.125DB reg_value = value / step << 6 + value % step ; step = 6 * 8*/
static unsigned int aw_pid_2013_db_val_to_reg(unsigned int value)
{
	return (((value / AW_PID_2013_VOL_STEP_DB) << 6) + (value % AW_PID_2013_VOL_STEP_DB));
}

static int aw_pid_2013_set_volume(struct aw_device *aw_dev, unsigned int value)
{
	struct aw882xx *aw882xx = (struct aw882xx *)aw_dev->private_data;
	unsigned int reg_value = 0;
	unsigned int real_value = aw_pid_2013_db_val_to_reg(value);

	/* cal real value */
	aw882xx_i2c_read(aw882xx, AW_PID_2013_SYSCTRL2_REG, &reg_value);

	aw_dev_dbg(aw882xx->dev, "value %d , 0x%x", value, real_value);

	/*9 : 0] volume*/
	real_value = real_value | (reg_value & 0xfc00);

	/* write value */
	aw882xx_i2c_write(aw882xx, AW_PID_2013_SYSCTRL2_REG, real_value);
	return 0;
}

static int aw_pid_2013_get_volume(struct aw_device *aw_dev, unsigned int* value)
{
	unsigned int reg_value = 0;
	unsigned int real_value = 0;
	struct aw882xx *aw882xx = (struct aw882xx *)aw_dev->private_data;

	/* read value */
	aw882xx_i2c_read(aw882xx, AW_PID_2013_SYSCTRL2_REG, &reg_value);

	/*[9 : 0] volume*/
	real_value = reg_value & 0x03ff;

	real_value = aw_pid_2013_reg_val_to_db(real_value);
	*value = real_value;

	return 0;
}

static void aw_pid_2013_i2s_enable(struct aw_device *aw_dev, bool flag)
{
	struct aw882xx *aw882xx = (struct aw882xx *)aw_dev->private_data;
	aw_dev_dbg(aw882xx->dev, "enter");

	if (flag)
		aw882xx_i2c_write_bits(aw882xx, AW_PID_2013_I2SCTRL1_REG,
				AW_PID_2013_I2STXEN_MASK,
				AW_PID_2013_I2STXEN_ENABLE_VALUE);
	else
		aw882xx_i2c_write_bits(aw882xx, AW_PID_2013_I2SCFG1_REG,
				AW_PID_2013_I2STXEN_MASK,
				AW_PID_2013_I2STXEN_DISABLE_VALUE);
}

static bool aw_pid_2013_check_rd_access(int reg)
{
	if (reg >= AW_PID_2013_REG_MAX)
		return false;

	if (aw_pid_2013_reg_access[reg] & AW_PID_2013_REG_RD_ACCESS)
		return true;
	else
		return false;
}

static bool aw_pid_2013_check_wr_access(int reg)
{
	if (reg >= AW_PID_2013_REG_MAX)
		return false;

	if (aw_pid_2013_reg_access[reg] & AW_PID_2013_REG_WR_ACCESS)
		return true;
	else
		return false;
}

static int aw_pid_2013_get_reg_num(void)
{
	return AW_PID_2013_REG_MAX;
}

static unsigned int aw_pid_2013_get_irq_type(struct aw_device *aw_dev,
					unsigned int value)
{
	unsigned int ret = INT_TYPE_NONE;

	/*UVL0*/
	if (value & (~AW_PID_2013_UVLI_MASK)) {
		aw_dev_info(aw_dev->dev, "UVLO: occur");
		ret |= INT_TYPE_UVLO;
	}

	/*BSTOCM*/
	if (value & (~AW_PID_2013_BSTOCI_MASK)) {
		aw_dev_info(aw_dev->dev, "BSTOCI: occur");
		ret |= INT_TYPE_BSTOC;
	}

	/*OCDI*/
	if (value & (~AW_PID_2013_OCDI_MASK)) {
		aw_dev_info(aw_dev->dev, "OCDI: occur");
		ret |= INT_TYPE_OCDI;
	}

	/*OTHI*/
	if (value & (~AW_PID_2013_OTHI_MASK)) {
		aw_dev_info(aw_dev->dev, "OTHI: occur");
		ret |= INT_TYPE_OTHI;
	}

	return ret;
}

static void aw_pid_2013_efver_check(struct aw_device *aw_dev)
{
	unsigned int reg_val;
	unsigned int efverh;
	unsigned int efverl;

	aw_dev->ops.aw_i2c_read(aw_dev,
			AW_PID_2013_EFRM1_REG, &reg_val);

	efverh = (((reg_val & (~AW_PID_2013_EFVERH_MASK)) >>
			AW_PID_2013_EFVERH_START_BIT) ^
			AW_PID_2013_EFVER_CHECK);
	efverl = (((reg_val & (~AW_PID_2013_EFVERL_MASK)) >>
			AW_PID_2013_EFVERL_START_BIT) ^
			AW_PID_2013_EFVER_CHECK);

	aw_dev_dbg(aw_dev->dev, "efverh: 0x%0x, efverl: 0x%0x", efverh, efverl);

	if (efverh && efverl) {
		aw_dev->bstcfg_enable = AW_BSTCFG_ENABLE;
		aw_dev_info(aw_dev->dev, "A2013 EFVER A");
	} else if (efverh && !efverl) {
		aw_dev->bstcfg_enable = AW_BSTCFG_DISABLE;
		aw_dev_info(aw_dev->dev, "A2013 EFVER B");
	} else {
		aw_dev->bstcfg_enable = AW_BSTCFG_DISABLE;
		aw_dev_info(aw_dev->dev, "unsupport A2013 EFVER");
	}
	aw_dev_info(aw_dev->dev, "bstcfg enable: %d", aw_dev->bstcfg_enable);
}

static int aw_pid_2013_dev_init(struct aw882xx *aw882xx, int index)
{
	int ret;
	struct aw_device *aw_pa = NULL;
	aw_pa = devm_kzalloc(aw882xx->dev, sizeof(struct aw_device), GFP_KERNEL);
	if (aw_pa == NULL) {
		aw_dev_err(aw882xx->dev, "dev kalloc failed");
		return -ENOMEM;
	}

	/*call aw device init func*/
	memset(aw_pa->acf_name, 0, AW_NAME_MAX);
	memcpy(aw_pa->acf_name, AW_PID_2013_ACF_FILE, strlen(AW_PID_2013_ACF_FILE));
	memset(aw_pa->monitor_name, 0, AW_NAME_MAX);
	memcpy(aw_pa->monitor_name, AW_PID_2013_MONITOR_FILE, strlen(AW_PID_2013_MONITOR_FILE));

	aw_pa->prof_info.prof_desc = NULL;
	aw_pa->prof_info.count = 0;
	aw_pa->channel = 0;
	aw_pa->bop_en = AW_BOP_DISABLE;
	aw_pa->vol_step = AW_PID_2013_VOL_STEP;

	aw_pa->index = index;
	aw_pa->private_data = (void *)aw882xx;
	aw_pa->dev = aw882xx->dev;
	aw_pa->i2c = aw882xx->i2c;
	aw_pa->ops.aw_get_version = aw882xx_get_version;
	aw_pa->ops.aw_get_dev_num = aw882xx_get_dev_num;
	aw_pa->ops.aw_set_algo = aw882xx_dev_set_algo_en;
	aw_pa->ops.aw_i2c_read = aw882xx_dev_i2c_read;
	aw_pa->ops.aw_i2c_write = aw882xx_dev_i2c_write;
	aw_pa->ops.aw_i2c_write_bits = aw882xx_dev_i2c_write_bits;
	aw_pa->ops.aw_get_volume = aw_pid_2013_get_volume;
	aw_pa->ops.aw_set_volume = aw_pid_2013_set_volume;
	aw_pa->ops.aw_reg_val_to_db = aw_pid_2013_reg_val_to_db;
	aw_pa->ops.aw_i2s_enable = aw_pid_2013_i2s_enable;
	aw_pa->ops.aw_check_rd_access = aw_pid_2013_check_rd_access;
	aw_pa->ops.aw_check_wr_access = aw_pid_2013_check_wr_access;
	aw_pa->ops.aw_get_reg_num = aw_pid_2013_get_reg_num;
	aw_pa->ops.aw_get_irq_type =aw_pid_2013_get_irq_type;

	aw_pa->int_desc.mask_reg = AW_PID_2013_SYSINTM_REG;
	aw_pa->int_desc.mask_default = AW_PID_2013_SYSINTM_DEFAULT;
	aw_pa->int_desc.int_mask = AW_PID_2013_SYSINTM_DEFAULT;
	aw_pa->int_desc.st_reg = AW_PID_2013_SYSINT_REG;

	aw_pa->pwd_desc.reg = AW_PID_2013_SYSCTRL_REG;
	aw_pa->pwd_desc.mask = AW_PID_2013_PWDN_MASK;
	aw_pa->pwd_desc.enable = AW_PID_2013_PWDN_POWER_DOWN_VALUE;
	aw_pa->pwd_desc.disable = AW_PID_2013_PWDN_WORKING_VALUE;

	aw_pa->amppd_desc.reg = AW_PID_2013_SYSCTRL_REG;
	aw_pa->amppd_desc.mask = AW_PID_2013_AMPPD_MASK;
	aw_pa->amppd_desc.enable = AW_PID_2013_AMPPD_POWER_DOWN_VALUE;
	aw_pa->amppd_desc.disable = AW_PID_2013_AMPPD_WORKING_VALUE;

	aw_pa->mute_desc.reg = AW_PID_2013_SYSCTRL_REG;
	aw_pa->mute_desc.mask = AW_PID_2013_HMUTE_MASK;
	aw_pa->mute_desc.enable = AW_PID_2013_HMUTE_ENABLE_VALUE;
	aw_pa->mute_desc.disable = AW_PID_2013_HMUTE_DISABLE_VALUE;

	aw_pa->vcalb_desc.vcalb_reg = AW_PID_2013_VSNTM1_REG;
	aw_pa->vcalb_desc.vcal_factor = AW_PID_2013_VCAL_FACTOR;
	aw_pa->vcalb_desc.cabl_base_value = AW_PID_2013_CABL_BASE_VALUE;

	aw_pa->vcalb_desc.icalk_value_factor = AW_PID_2013_ICABLK_FACTOR;
	aw_pa->vcalb_desc.icalk_reg = AW_PID_2013_EFRH_REG;
	aw_pa->vcalb_desc.icalkl_reg = AW_REG_NONE;
	aw_pa->vcalb_desc.icalk_reg_mask = AW_PID_2013_EF_ISN_GESLP_MASK;
	aw_pa->vcalb_desc.icalk_sign_mask = AW_PID_2013_EF_ISN_GESLP_SIGN_MASK;
	aw_pa->vcalb_desc.icalk_neg_mask = AW_PID_2013_EF_ISN_GESLP_NEG;

	aw_pa->vcalb_desc.vcalk_reg = AW_PID_2013_EFRM2_REG;
	aw_pa->vcalb_desc.vcalk_reg_mask = AW_PID_2013_EF_VSN_GESLP_MASK;
	aw_pa->vcalb_desc.vcalkl_reg = AW_REG_NONE;
	aw_pa->vcalb_desc.vcalk_sign_mask = AW_PID_2013_EF_ISN_GESLP2_SIGN_MASK;
	aw_pa->vcalb_desc.vcalk_neg_mask = AW_PID_2013_EF_ISN_GESLP2_NEG;
	aw_pa->vcalb_desc.vcalk_value_factor = AW_PID_2013_VCABLK_FACTOR;

	aw_pa->sysst_desc.reg = AW_PID_2013_SYSST_REG;
	aw_pa->sysst_desc.mask = AW_PID_2013_SYSST_CHECK_MASK;
	aw_pa->sysst_desc.st_check = AW_PID_2013_SYSST_CHECK;
	aw_pa->sysst_desc.pll_check = AW_PID_2013_PLLS_LOCKED_VALUE;

	aw_pa->profctrl_desc.reg = AW_PID_2013_SYSCTRL_REG;
	aw_pa->profctrl_desc.mask = AW_PID_2013_EN_TRAN_MASK;
	aw_pa->profctrl_desc.spk_mode = AW_PID_2013_EN_TRAN_SPK_VALUE;

	aw_pa->bstctrl_desc.reg = AW_PID_2013_BSTCTRL2_REG;
	aw_pa->bstctrl_desc.mask = AW_PID_2013_BST_MODE_MASK;
	aw_pa->bstctrl_desc.frc_bst = AW_PID_2013_BST_MODE_FORCE_BOOST_VALUE;
	aw_pa->bstctrl_desc.tsp_type = AW_PID_2013_BST_MODE_TRANSPARENT_VALUE;

	aw_pa->voltage_desc.reg = AW_PID_2013_VBAT_REG;
	aw_pa->voltage_desc.int_bit = AW_PID_2013_MONITOR_INT_10BIT;
	aw_pa->voltage_desc.vbat_range = AW_PID_2013_MONITOR_VBAT_RANGE;

	aw_pa->temp_desc.reg = AW_PID_2013_TEMP_REG;
	aw_pa->temp_desc.neg_mask = AW_PID_2013_MONITOR_TEMP_NEG_MASK;
	aw_pa->temp_desc.sign_mask = AW_PID_2013_MONITOR_TEMP_SIGN_MASK;

	aw_pa->cco_mux_desc.reg = AW_PID_2013_PLLCTRL3_REG;
	aw_pa->cco_mux_desc.mask = AW_PID_2013_CCO_MUX_MASK;
	aw_pa->cco_mux_desc.divided_val = AW_PID_2013_CCO_MUX_DIVIDED_VALUE;
	aw_pa->cco_mux_desc.bypass_val = AW_PID_2013_CCO_MUX_BYPASS_VALUE;

	aw_pa->ipeak_desc.reg = AW_PID_2013_BSTCTRL2_REG;
	aw_pa->ipeak_desc.mask = AW_PID_2013_BST_IPEAK_MASK;

	aw_pa->volume_desc.reg = AW_PID_2013_SYSCTRL2_REG;
	aw_pa->volume_desc.mask = AW_PID_2013_VOL_MASK;
	aw_pa->volume_desc.shift = AW_PID_2013_VOL_START_BIT;
	aw_pa->volume_desc.mute_volume = AW_PID_2013_MUTE_VOL;

	aw_pa->bop_desc.reg = AW_REG_NONE;

	aw_pa->soft_rst.reg = AW_PID_2013_ID_REG;
	aw_pa->soft_rst.reg_value = AW882XX_SOFT_RESET_VALUE;

	aw_pid_2013_efver_check(aw_pa);
	ret = aw_device_probe(aw_pa);

	aw882xx->aw_pa = aw_pa;
	return ret;
}

/******************************************************
 *
 * A2032 init
 *
 ******************************************************/
/*[7 : 4]: -6DB ; [3 : 0]: 0.5DB  real_value = value * 2 : 0.5db --> 1*/
static unsigned int aw_pid_2032_reg_val_to_db(unsigned int value)
{
	return ((value >> 6) * AW_PID_2032_VOL_STEP_DB + (value & 0x3f));
}

/* [7 : 4]: -6DB ; [3 : 0]: -0.5DB reg_value = value / step << 4 + value % step ; step = 6 * 2 */
static unsigned int aw_pid_2032_db_val_to_reg(unsigned int value)
{
	return (((value / AW_PID_2032_VOL_STEP_DB) << 6) + (value % AW_PID_2032_VOL_STEP_DB));
}


static int aw_pid_2032_set_volume(struct aw_device *aw_dev, unsigned int value)
{
	struct aw882xx *aw882xx = (struct aw882xx *)aw_dev->private_data;
	unsigned int reg_value = 0;
	unsigned int real_value = aw_pid_2032_db_val_to_reg(value);

	/* cal real value */
	aw882xx_i2c_read(aw882xx, AW_PID_2032_SYSCTRL2_REG, &reg_value);

	aw_dev_dbg(aw882xx->dev, "value %d , 0x%x", value, real_value);

	/* [15 : 6] volume */
	real_value = (real_value << 6) | (reg_value & 0x003f);

	/* write value */
	aw882xx_i2c_write(aw882xx, AW_PID_2032_SYSCTRL2_REG, real_value);
	return 0;
}

static int aw_pid_2032_get_volume(struct aw_device *aw_dev, unsigned int *value)
{
	unsigned int reg_value = 0;
	unsigned int real_value = 0;
	struct aw882xx *aw882xx = (struct aw882xx *)aw_dev->private_data;

	/* read value */
	aw882xx_i2c_read(aw882xx, AW_PID_2032_SYSCTRL2_REG, &reg_value);

	/* [15 : 6] volume */
	real_value = reg_value >> 6;

	real_value = aw_pid_2032_reg_val_to_db(real_value);
	*value = real_value;

	return 0;
}

static void aw_pid_2032_i2s_enable(struct aw_device *aw_dev, bool flag)
{
	struct aw882xx *aw882xx = (struct aw882xx *)aw_dev->private_data;
	struct aw_cali_desc *desc = &aw_dev->cali_desc;
	aw_dev_dbg(aw882xx->dev, "enter");

	if (!desc->mode) {
		aw_dev_dbg(aw_dev->dev, "cali mode is none, needn't set i2s status");
		return;
	}

	if (flag) {
		aw882xx_i2c_write_bits(aw882xx, AW_PID_2032_I2SCFG1_REG,
				AW_PID_2032_I2STXEN_MASK,
				AW_PID_2032_I2STXEN_ENABLE_VALUE);
	} else {
		aw882xx_i2c_write_bits(aw882xx, AW_PID_2032_I2SCFG1_REG,
				AW_PID_2032_I2STXEN_MASK,
				AW_PID_2032_I2STXEN_DISABLE_VALUE);
	}
}

static bool aw_pid_2032_check_rd_access(int reg)
{
	if (reg >= AW_PID_2032_REG_MAX)
		return false;

	if (aw_pid_2032_reg_access[reg] & AW_PID_2032_REG_RD_ACCESS)
		return true;
	else
		return false;
}

static bool aw_pid_2032_check_wr_access(int reg)
{
	if (reg >= AW_PID_2032_REG_MAX)
		return false;

	if (aw_pid_2032_reg_access[reg] &
			AW_PID_2032_REG_WR_ACCESS)
		return true;
	else
		return false;
}

static int aw_pid_2032_get_reg_num(void)
{
	return AW_PID_2032_REG_MAX;
}

static unsigned int aw_pid_2032_get_irq_type(struct aw_device *aw_dev,
					unsigned int value)
{
	unsigned int ret = INT_TYPE_NONE;

	/* UVL0 */
	if (value & (~AW_PID_2032_UVLI_MASK)) {
		aw_dev_info(aw_dev->dev, "UVLO: occur");
		ret |= INT_TYPE_UVLO;
	}

	/* BSTOCM */
	if (value & (~AW_PID_2032_BSTOCI_MASK)) {
		aw_dev_info(aw_dev->dev, "BSTOCI: occur");
		ret |= INT_TYPE_BSTOC;
	}

	/* OCDI */
	if (value & (~AW_PID_2032_OCDI_MASK)) {
		aw_dev_info(aw_dev->dev, "OCDI: occur");
		ret |= INT_TYPE_OCDI;
	}

	/* OTHI */
	if (value & (~AW_PID_2032_OTHI_MASK)) {
		aw_dev_info(aw_dev->dev, "OTHI: occur");
		ret |= INT_TYPE_OTHI;
	}

	return ret;
}

static int aw_pid_2032_dev_init(struct aw882xx *aw882xx, int index)
{
	int ret;
	struct aw_device *aw_pa = NULL;

	aw_pa = devm_kzalloc(aw882xx->dev, sizeof(struct aw_device), GFP_KERNEL);
	if (aw_pa == NULL) {
		aw_dev_err(aw882xx->dev, "dev kalloc failed");
		return -ENOMEM;
	}

	/* call aw device init func */
	memset(aw_pa->acf_name, 0, AW_NAME_MAX);
	memcpy(aw_pa->acf_name, AW_PID_2032_ACF_FILE, strlen(AW_PID_2032_ACF_FILE));
	memset(aw_pa->monitor_name, 0, AW_NAME_MAX);
	memcpy(aw_pa->monitor_name, AW_PID_2032_MONITOR_FILE, strlen(AW_PID_2032_MONITOR_FILE));

	aw_pa->prof_info.prof_desc = NULL;
	aw_pa->prof_info.count = 0;
	aw_pa->channel = 0;
	aw_pa->bstcfg_enable = AW_BSTCFG_DISABLE;
	aw_pa->bop_en = AW_BOP_DISABLE;
	aw_pa->vol_step = AW_PID_2032_VOL_STEP;

	aw_pa->index = index;
	aw_pa->private_data = (void *)aw882xx;
	aw_pa->dev = aw882xx->dev;
	aw_pa->i2c = aw882xx->i2c;
	aw_pa->ops.aw_get_version = aw882xx_get_version;
	aw_pa->ops.aw_get_dev_num = aw882xx_get_dev_num;
	aw_pa->ops.aw_set_algo = aw882xx_dev_set_algo_en;
	aw_pa->ops.aw_i2c_read = aw882xx_dev_i2c_read;
	aw_pa->ops.aw_i2c_write = aw882xx_dev_i2c_write;
	aw_pa->ops.aw_i2c_write_bits = aw882xx_dev_i2c_write_bits;
	aw_pa->ops.aw_get_volume = aw_pid_2032_get_volume;
	aw_pa->ops.aw_set_volume = aw_pid_2032_set_volume;
	aw_pa->ops.aw_reg_val_to_db = aw_pid_2032_reg_val_to_db;
	aw_pa->ops.aw_i2s_enable = aw_pid_2032_i2s_enable;
	aw_pa->ops.aw_check_rd_access = aw_pid_2032_check_rd_access;
	aw_pa->ops.aw_check_wr_access = aw_pid_2032_check_wr_access;
	aw_pa->ops.aw_get_reg_num = aw_pid_2032_get_reg_num;
	aw_pa->ops.aw_get_irq_type = aw_pid_2032_get_irq_type;

	aw_pa->int_desc.mask_reg = AW_PID_2032_SYSINTM_REG;
	aw_pa->int_desc.mask_default = AW_PID_2032_SYSINTM_DEFAULT;
	aw_pa->int_desc.int_mask = AW_PID_2032_SYSINTM_DEFAULT;
	aw_pa->int_desc.st_reg = AW_PID_2032_SYSINT_REG;

	aw_pa->pwd_desc.reg = AW_PID_2032_SYSCTRL_REG;
	aw_pa->pwd_desc.mask = AW_PID_2032_PWDN_MASK;
	aw_pa->pwd_desc.enable = AW_PID_2032_PWDN_POWER_DOWN_VALUE;
	aw_pa->pwd_desc.disable = AW_PID_2032_PWDN_WORKING_VALUE;

	aw_pa->amppd_desc.reg = AW_PID_2032_SYSCTRL_REG;
	aw_pa->amppd_desc.mask = AW_PID_2032_AMPPD_MASK;
	aw_pa->amppd_desc.enable = AW_PID_2032_AMPPD_POWER_DOWN_VALUE;
	aw_pa->amppd_desc.disable = AW_PID_2032_AMPPD_WORKING_VALUE;

	aw_pa->mute_desc.reg = AW_PID_2032_SYSCTRL2_REG;
	aw_pa->mute_desc.mask = AW_PID_2032_HMUTE_MASK;
	aw_pa->mute_desc.enable = AW_PID_2032_HMUTE_ENABLE_VALUE;
	aw_pa->mute_desc.disable = AW_PID_2032_HMUTE_DISABLE_VALUE;

	aw_pa->vcalb_desc.vcalb_reg = AW_PID_2032_VTMCTRL3_REG;
	aw_pa->vcalb_desc.vcal_factor = AW_PID_2032_VCAL_FACTOR;
	aw_pa->vcalb_desc.cabl_base_value = AW_PID_2032_CABL_BASE_VALUE;

	aw_pa->vcalb_desc.icalk_value_factor = AW_PID_2032_ICABLK_FACTOR;
	aw_pa->vcalb_desc.icalk_reg = AW_PID_2032_EFRM1_REG;
	aw_pa->vcalb_desc.icalkl_reg = AW_REG_NONE;
	aw_pa->vcalb_desc.icalk_reg_mask = AW_PID_2032_EF_ISN_GESLP_MASK;
	aw_pa->vcalb_desc.icalk_sign_mask = AW_PID_2032_EF_ISN_GESLP_SIGN_MASK;
	aw_pa->vcalb_desc.icalk_neg_mask = AW_PID_2032_EF_ISN_GESLP_NEG;

	aw_pa->vcalb_desc.vcalk_reg = AW_PID_2032_EFRH_REG;
	aw_pa->vcalb_desc.vcalk_reg_mask = AW_PID_2032_EF_VSN_GESLP_MASK;
	aw_pa->vcalb_desc.vcalkl_reg = AW_REG_NONE;
	aw_pa->vcalb_desc.vcalk_sign_mask = AW_PID_2032_EF_VSN_GESLP_SIGN_MASK;
	aw_pa->vcalb_desc.vcalk_neg_mask = AW_PID_2032_EF_VSN_GESLP_NEG;
	aw_pa->vcalb_desc.vcalk_value_factor = AW_PID_2032_VCABLK_FACTOR;

	aw_pa->sysst_desc.reg = AW_PID_2032_SYSST_REG;
	aw_pa->sysst_desc.mask = AW_PID_2032_SYSST_CHECK_MASK;
	aw_pa->sysst_desc.st_check = AW_PID_2032_SYSST_CHECK;
	aw_pa->sysst_desc.pll_check = AW_PID_2032_PLLS_LOCKED_VALUE;

	aw_pa->cco_mux_desc.reg = AW_PID_2032_PLLCTRL1_REG;
	aw_pa->cco_mux_desc.mask = AW_PID_2032_CCO_MUX_MASK;
	aw_pa->cco_mux_desc.divided_val = AW_PID_2032_CCO_MUX_DIVIDED_VALUE;
	aw_pa->cco_mux_desc.bypass_val = AW_PID_2032_CCO_MUX_BYPASS_VALUE;

	aw_pa->voltage_desc.reg = AW_PID_2032_VBAT_REG;
	aw_pa->voltage_desc.int_bit = AW_PID_2032_MONITOR_INT_10BIT;
	aw_pa->voltage_desc.vbat_range = AW_PID_2032_MONITOR_VBAT_RANGE;

	aw_pa->temp_desc.reg = AW_PID_2032_TEMP_REG;
	aw_pa->temp_desc.neg_mask = AW_PID_2032_MONITOR_TEMP_NEG_MASK;
	aw_pa->temp_desc.sign_mask = AW_PID_2032_MONITOR_TEMP_SIGN_MASK;

	aw_pa->ipeak_desc.reg = AW_PID_2032_SYSCTRL2_REG;
	aw_pa->ipeak_desc.mask = AW_PID_2032_BST_IPEAK_MASK;

	aw_pa->volume_desc.reg = AW_PID_2032_SYSCTRL2_REG;
	aw_pa->volume_desc.mask = AW_PID_2032_VOL_MASK;
	aw_pa->volume_desc.shift = AW_PID_2032_VOL_START_BIT;
	aw_pa->volume_desc.mute_volume = AW_PID_2032_MUTE_VOL;

	aw_pa->bop_desc.reg = AW_REG_NONE;

	aw_pa->soft_rst.reg = AW_PID_2032_ID_REG;
	aw_pa->soft_rst.reg_value = AW882XX_SOFT_RESET_VALUE;

	ret = aw_device_probe(aw_pa);

	aw882xx->aw_pa = aw_pa;
	return ret;
}

/* [9 : 6]: -6DB ; [5 : 0]: 0.125DB  real_value = value * 8 : 0.125db --> 1 */
static unsigned int aw_pid_2055_reg_val_to_db(unsigned int value)
{
	return ((value >> 6) * AW_PID_2055_VOL_STEP_DB + (value & 0x3f));
}

/* [9 : 6]: -6DB ; [5 : 0]: 0.125DB reg_value = value / step << 6 + value % step ; step = 6 * 8 */
static unsigned int aw_pid_2055_db_val_to_reg(unsigned int value)
{
	return (((value / AW_PID_2055_VOL_STEP_DB) << 6) + (value % AW_PID_2055_VOL_STEP_DB));
}

static int aw_pid_2055_set_volume(struct aw_device *aw_dev, unsigned int value)
{
	struct aw882xx *aw882xx = (struct aw882xx *)aw_dev->private_data;
	unsigned int reg_value = 0;
	unsigned int real_value = aw_pid_2055_db_val_to_reg(value);

	/* cal real value */
	aw882xx_i2c_read(aw882xx, AW_PID_2055_SYSCTRL2_REG, &reg_value);

	aw_dev_dbg(aw882xx->dev, "value %d , 0x%x", value, real_value);

	/*[9 : 0] volume*/
	real_value = (real_value | (reg_value & 0xfc00));

	/* write value */
	aw882xx_i2c_write(aw882xx, AW_PID_2055_SYSCTRL2_REG, real_value);
	return 0;
}

static int aw_pid_2055_get_volume(struct aw_device *aw_dev, unsigned int *value)
{
	unsigned int reg_value = 0;
	unsigned int real_value = 0;
	struct aw882xx *aw882xx = (struct aw882xx *)aw_dev->private_data;

	/* read value */
	aw882xx_i2c_read(aw882xx, AW_PID_2055_SYSCTRL2_REG, &reg_value);

	/* [9 : 0] volume */
	real_value = (reg_value & 0x03ff);

	real_value = aw_pid_2055_reg_val_to_db(real_value);
	*value = real_value;

	return 0;
}

static bool aw_pid_2055_check_rd_access(int reg)
{
	if (reg >= AW_PID_2055_REG_MAX) {
		return false;
	}

	if (aw_pid_2055_reg_access[reg] & AW_PID_2055_REG_RD_ACCESS) {
		return true;
	} else {
		return false;
	}
}

static bool aw_pid_2055_check_wr_access(int reg)
{
	if (reg >= AW_PID_2055_REG_MAX)
		return false;

	if (aw_pid_2055_reg_access[reg] &
			AW_PID_2055_REG_WR_ACCESS)
		return true;
	else
		return false;
}

static int aw_pid_2055_get_reg_num(void)
{
	return AW_PID_2055_REG_MAX;
}

static unsigned int aw_pid_2055_get_irq_type(struct aw_device *aw_dev,
					unsigned int value)
{
	unsigned int ret = INT_TYPE_NONE;

	/* UVL0 */
	if (value & (~AW_PID_2055_UVLI_MASK)) {
		aw_dev_info(aw_dev->dev, "UVLO: occur");
		ret |= INT_TYPE_UVLO;
	}

	/* BSTOCM */
	if (value & (~AW_PID_2055_BSTOCI_MASK)) {
		aw_dev_info(aw_dev->dev, "BSTOCI: occur");
		ret |= INT_TYPE_BSTOC;
	}

	/* OCDI */
	if (value & (~AW_PID_2055_OCDI_MASK)) {
		aw_dev_info(aw_dev->dev, "OCDI: occur");
		ret |= INT_TYPE_OCDI;
	}

	/* OTHI */
	if (value & (~AW_PID_2055_OTHI_MASK)) {
		aw_dev_info(aw_dev->dev, "OTHI: occur");
		ret |= INT_TYPE_OTHI;
	}

	return ret;
}

static int aw_pid_2055_dev_init(struct aw882xx *aw882xx, int index)
{
	int ret;
	struct aw_device *aw_pa = NULL;
	aw_pa = devm_kzalloc(aw882xx->dev, sizeof(struct aw_device), GFP_KERNEL);
	if (aw_pa == NULL) {
		aw_dev_err(aw882xx->dev, "dev kalloc failed");
		return -ENOMEM;
	}

	/*call aw device init func*/
	memset(aw_pa->acf_name, 0, AW_NAME_MAX);
	memcpy(aw_pa->acf_name, AW_PID_2055_ACF_FILE, strlen(AW_PID_2055_ACF_FILE));
	memset(aw_pa->monitor_name, 0, AW_NAME_MAX);
	memcpy(aw_pa->monitor_name, AW_PID_2055_MONITOR_FILE, strlen(AW_PID_2055_MONITOR_FILE));

	aw_pa->prof_info.prof_desc = NULL;
	aw_pa->prof_info.count = 0;
	aw_pa->channel = 0;
	aw_pa->bstcfg_enable = AW_BSTCFG_DISABLE;
	aw_pa->bop_en = AW_BOP_DISABLE;
	aw_pa->vol_step = AW_PID_2055_VOL_STEP;

	aw_pa->index = index;
	aw_pa->private_data = (void *)aw882xx;
	aw_pa->dev = aw882xx->dev;
	aw_pa->i2c = aw882xx->i2c;
	aw_pa->ops.aw_get_version = aw882xx_get_version;
	aw_pa->ops.aw_get_dev_num = aw882xx_get_dev_num;
	aw_pa->ops.aw_set_algo = aw882xx_dev_set_algo_en;
	aw_pa->ops.aw_i2c_read = aw882xx_dev_i2c_read;
	aw_pa->ops.aw_i2c_write = aw882xx_dev_i2c_write;
	aw_pa->ops.aw_i2c_write_bits = aw882xx_dev_i2c_write_bits;
	aw_pa->ops.aw_get_volume = aw_pid_2055_get_volume;
	aw_pa->ops.aw_set_volume = aw_pid_2055_set_volume;
	aw_pa->ops.aw_reg_val_to_db = aw_pid_2055_reg_val_to_db;
	aw_pa->ops.aw_check_rd_access = aw_pid_2055_check_rd_access;
	aw_pa->ops.aw_check_wr_access = aw_pid_2055_check_wr_access;
	aw_pa->ops.aw_get_reg_num = aw_pid_2055_get_reg_num;
	aw_pa->ops.aw_get_irq_type =aw_pid_2055_get_irq_type;

	aw_pa->int_desc.mask_reg = AW_PID_2055_SYSINTM_REG;
	aw_pa->int_desc.mask_default = AW_PID_2055_SYSINTM_DEFAULT;
	aw_pa->int_desc.int_mask = AW_PID_2055_SYSINTM_DEFAULT;
	aw_pa->int_desc.st_reg = AW_PID_2055_SYSINT_REG;

	aw_pa->pwd_desc.reg = AW_PID_2055_SYSCTRL_REG;
	aw_pa->pwd_desc.mask = AW_PID_2055_PWDN_MASK;
	aw_pa->pwd_desc.enable = AW_PID_2055_PWDN_POWER_DOWN_VALUE;
	aw_pa->pwd_desc.disable = AW_PID_2055_PWDN_WORKING_VALUE;

	aw_pa->amppd_desc.reg = AW_PID_2055_SYSCTRL_REG;
	aw_pa->amppd_desc.mask = AW_PID_2055_AMPPD_MASK;
	aw_pa->amppd_desc.enable = AW_PID_2055_AMPPD_POWER_DOWN_VALUE;
	aw_pa->amppd_desc.disable = AW_PID_2055_AMPPD_WORKING_VALUE;

	aw_pa->mute_desc.reg = AW_PID_2055_SYSCTRL_REG;
	aw_pa->mute_desc.mask = AW_PID_2055_HMUTE_MASK;
	aw_pa->mute_desc.enable = AW_PID_2055_HMUTE_ENABLE_VALUE;
	aw_pa->mute_desc.disable = AW_PID_2055_HMUTE_DISABLE_VALUE;

	aw_pa->vcalb_desc.vcalb_reg = AW_REG_NONE;
	aw_pa->vcalb_desc.vcalk_reg = AW_REG_NONE;
	aw_pa->vcalb_desc.icalk_reg = AW_REG_NONE;

	aw_pa->sysst_desc.reg = AW_PID_2055_SYSST_REG;
	aw_pa->sysst_desc.mask = AW_PID_2055_SYSST_CHECK_MASK;
	aw_pa->sysst_desc.st_check = AW_PID_2055_SYSST_CHECK;
	aw_pa->sysst_desc.pll_check = AW_PID_2055_PLLS_LOCKED_VALUE;

	aw_pa->voltage_desc.reg = AW_REG_NONE;
	aw_pa->temp_desc.reg = AW_REG_NONE;

	aw_pa->ipeak_desc.reg = AW_PID_2055_BSTCTRL2_REG;
	aw_pa->ipeak_desc.mask = AW_PID_2055_BST_IPEAK_MASK;

	aw_pa->volume_desc.reg = AW_PID_2055_SYSCTRL2_REG;
	aw_pa->volume_desc.mask = AW_PID_2055_VOL_MASK;
	aw_pa->volume_desc.shift = AW_PID_2055_VOL_START_BIT;
	aw_pa->volume_desc.mute_volume = AW_PID_2055_MUTE_VOL;

	aw_pa->cco_mux_desc.reg = AW_PID_2055_PLLCTRL3_REG;
	aw_pa->cco_mux_desc.mask = AW_PID_2055_CCO_MUX_MASK;
	aw_pa->cco_mux_desc.divided_val = AW_PID_2055_CCO_MUX_DIVIDED_VALUE;
	aw_pa->cco_mux_desc.bypass_val = AW_PID_2055_CCO_MUX_BYPASS_VALUE;

	aw_pa->bop_desc.reg = AW_REG_NONE;

	aw_pa->soft_rst.reg = AW_PID_2055_ID_REG;
	aw_pa->soft_rst.reg_value = AW882XX_SOFT_RESET_VALUE;

	ret = aw_device_probe(aw_pa);

	aw882xx->aw_pa = aw_pa;
	return ret;
}

/* [9 : 6]: -6DB ; [5 : 0]: 0.125DB  real_value = value * 8 : 0.125db --> 1 */
static unsigned int aw_pid_2071_reg_val_to_db(unsigned int value)
{
	return ((value >> 6) * AW_PID_2071_VOL_STEP_DB + (value & 0x3f));
}

/* [9 : 6]: -6DB ; [5 : 0]: 0.125DB reg_value = value / step << 6 + value % step ; step = 6 * 8 */
static unsigned int aw_pid_2071_db_val_to_reg(unsigned int value)
{
	return (((value / AW_PID_2071_VOL_STEP_DB) << 6) + (value % AW_PID_2071_VOL_STEP_DB));
}


static int aw_pid_2071_set_volume(struct aw_device *aw_dev, unsigned int value)
{
	struct aw882xx *aw882xx = (struct aw882xx *)aw_dev->private_data;
	unsigned int reg_value = 0;
	unsigned int real_value = aw_pid_2071_db_val_to_reg(value);

	/* cal real value */
	aw882xx_i2c_read(aw882xx, AW_PID_2071_SYSCTRL2_REG, &reg_value);

	aw_dev_dbg(aw882xx->dev, "value %d , 0x%x", value, real_value);

	/* [15 : 6] volume */
	real_value = (real_value << 6) | (reg_value & 0x003f);

	/* write value */
	aw882xx_i2c_write(aw882xx, AW_PID_2071_SYSCTRL2_REG, real_value);
	return 0;
}

static int aw_pid_2071_get_volume(struct aw_device *aw_dev, unsigned int *value)
{
	unsigned int reg_value = 0;
	unsigned int real_value = 0;
	struct aw882xx *aw882xx = (struct aw882xx *)aw_dev->private_data;

	/* read value */
	aw882xx_i2c_read(aw882xx, AW_PID_2071_SYSCTRL2_REG, &reg_value);

	/* [15 : 6] volume */
	real_value = reg_value >> 6;

	real_value = aw_pid_2071_reg_val_to_db(real_value);
	*value = real_value;

	return 0;
}

static void aw_pid_2071_i2s_enable(struct aw_device *aw_dev, bool flag)
{
	struct aw882xx *aw882xx = (struct aw882xx *)aw_dev->private_data;
	struct aw_cali_desc *desc = &aw_dev->cali_desc;
	aw_dev_dbg(aw882xx->dev, "enter");

	if (!desc->mode) {
		aw_dev_dbg(aw_dev->dev, "cali mode is none, needn't set i2s status");
		return;
	}

	if (flag) {
		aw882xx_i2c_write_bits(aw882xx, AW_PID_2071_I2SCFG1_REG,
				AW_PID_2071_I2STXEN_MASK,
				AW_PID_2071_I2STXEN_ENABLE_VALUE);
	} else {
		aw882xx_i2c_write_bits(aw882xx, AW_PID_2071_I2SCFG1_REG,
				AW_PID_2071_I2STXEN_MASK,
				AW_PID_2071_I2STXEN_DISABLE_VALUE);
	}
}

static bool aw_pid_2071_check_rd_access(int reg)
{
	if (reg >= AW_PID_2071_REG_MAX)
		return false;

	if (aw_pid_2071_reg_access[reg] & AW_PID_2071_REG_RD_ACCESS)
		return true;
	else
		return false;
}

static bool aw_pid_2071_check_wr_access(int reg)
{
	if (reg >= AW_PID_2071_REG_MAX)
		return false;

	if (aw_pid_2071_reg_access[reg] &
			AW_PID_2071_REG_WR_ACCESS)
		return true;
	else
		return false;
}

static int aw_pid_2071_get_reg_num(void)
{
	return AW_PID_2071_REG_MAX;
}

static unsigned int aw_pid_2071_get_irq_type(struct aw_device *aw_dev,
					unsigned int value)
{
	unsigned int ret = INT_TYPE_NONE;

	/* UVL0 */
	if (value & (~AW_PID_2071_UVLI_MASK)) {
		aw_dev_info(aw_dev->dev, "UVLO: occur");
		ret |= INT_TYPE_UVLO;
	}

	/* BSTOCM */
	if (value & (~AW_PID_2071_BSTOCI_MASK)) {
		aw_dev_info(aw_dev->dev, "BSTOCI: occur");
		ret |= INT_TYPE_BSTOC;
	}

	/* OCDI */
	if (value & (~AW_PID_2071_OCDI_MASK)) {
		aw_dev_info(aw_dev->dev, "OCDI: occur");
		ret |= INT_TYPE_OCDI;
	}

	/* OTHI */
	if (value & (~AW_PID_2071_OTHI_MASK)) {
		aw_dev_info(aw_dev->dev, "OTHI: occur");
		ret |= INT_TYPE_OTHI;
	}

	return ret;
}

static int aw_pid_2071_dev_init(struct aw882xx *aw882xx, int index)
{
	int ret;
	struct aw_device *aw_pa = NULL;
	aw_pa = devm_kzalloc(aw882xx->dev, sizeof(struct aw_device), GFP_KERNEL);
	if (aw_pa == NULL) {
		aw_dev_err(aw882xx->dev, "dev kalloc failed");
		return -ENOMEM;
	}

	/*call aw device init func*/
	memset(aw_pa->acf_name, 0, AW_NAME_MAX);
	memcpy(aw_pa->acf_name, AW_PID_2071_ACF_FILE, strlen(AW_PID_2071_ACF_FILE));
	memset(aw_pa->monitor_name, 0, AW_NAME_MAX);
	memcpy(aw_pa->monitor_name, AW_PID_2071_MONITOR_FILE, strlen(AW_PID_2071_MONITOR_FILE));

	aw_pa->prof_info.prof_desc = NULL;
	aw_pa->prof_info.count = 0;
	aw_pa->channel = 0;
	aw_pa->bstcfg_enable = AW_BSTCFG_DISABLE;
	aw_pa->bop_en = AW_BOP_DISABLE;
	aw_pa->vol_step = AW_PID_2071_VOL_STEP;

	aw_pa->index = index;
	aw_pa->private_data = (void *)aw882xx;
	aw_pa->dev = aw882xx->dev;
	aw_pa->i2c = aw882xx->i2c;
	aw_pa->ops.aw_get_version = aw882xx_get_version;
	aw_pa->ops.aw_get_dev_num = aw882xx_get_dev_num;
	aw_pa->ops.aw_set_algo = aw882xx_dev_set_algo_en;
	aw_pa->ops.aw_i2c_read = aw882xx_dev_i2c_read;
	aw_pa->ops.aw_i2c_write = aw882xx_dev_i2c_write;
	aw_pa->ops.aw_i2c_write_bits = aw882xx_dev_i2c_write_bits;
	aw_pa->ops.aw_get_volume = aw_pid_2071_get_volume;
	aw_pa->ops.aw_set_volume = aw_pid_2071_set_volume;
	aw_pa->ops.aw_reg_val_to_db = aw_pid_2071_reg_val_to_db;
	aw_pa->ops.aw_i2s_enable = aw_pid_2071_i2s_enable;
	aw_pa->ops.aw_check_rd_access = aw_pid_2071_check_rd_access;
	aw_pa->ops.aw_check_wr_access = aw_pid_2071_check_wr_access;
	aw_pa->ops.aw_get_reg_num = aw_pid_2071_get_reg_num;
	aw_pa->ops.aw_get_irq_type = aw_pid_2071_get_irq_type;

	aw_pa->int_desc.mask_reg = AW_PID_2071_SYSINTM_REG;
	aw_pa->int_desc.mask_default = AW_PID_2071_SYSINTM_DEFAULT;
	aw_pa->int_desc.int_mask = AW_PID_2071_SYSINTM_DEFAULT;
	aw_pa->int_desc.st_reg = AW_PID_2071_SYSINT_REG;

	aw_pa->pwd_desc.reg = AW_PID_2071_SYSCTRL_REG;
	aw_pa->pwd_desc.mask = AW_PID_2071_PWDN_MASK;
	aw_pa->pwd_desc.enable = AW_PID_2071_PWDN_POWER_DOWN_VALUE;
	aw_pa->pwd_desc.disable = AW_PID_2071_PWDN_WORKING_VALUE;

	aw_pa->amppd_desc.reg = AW_PID_2071_SYSCTRL_REG;
	aw_pa->amppd_desc.mask = AW_PID_2071_AMPPD_MASK;
	aw_pa->amppd_desc.enable = AW_PID_2071_AMPPD_POWER_DOWN_VALUE;
	aw_pa->amppd_desc.disable = AW_PID_2071_AMPPD_WORKING_VALUE;

	aw_pa->mute_desc.reg = AW_PID_2071_SYSCTRL2_REG;
	aw_pa->mute_desc.mask = AW_PID_2071_HMUTE_MASK;
	aw_pa->mute_desc.enable = AW_PID_2071_HMUTE_ENABLE_VALUE;
	aw_pa->mute_desc.disable = AW_PID_2071_HMUTE_DISABLE_VALUE;

	aw_pa->vcalb_desc.vcalb_reg = AW_PID_2071_VTMCTRL3_REG;
	aw_pa->vcalb_desc.vcal_factor = AW_PID_2071_VCAL_FACTOR;
	aw_pa->vcalb_desc.cabl_base_value = AW_PID_2071_CABL_BASE_VALUE;

	aw_pa->vcalb_desc.icalk_value_factor = AW_PID_2071_ICABLK_FACTOR;
	aw_pa->vcalb_desc.icalk_reg = AW_PID_2071_EFRH_REG;
	aw_pa->vcalb_desc.icalk_reg_mask = AW_PID_2071_EF_VSN_OFFSET_MASK;
	aw_pa->vcalb_desc.icalk_shift = AW_PID_2071_ICALK_SHIFT;
	aw_pa->vcalb_desc.icalkl_reg = AW_PID_2071_EFRM1_REG;
	aw_pa->vcalb_desc.icalkl_reg_mask = AW_PID_2071_EF_ISN_OFFSET_MASK;
	aw_pa->vcalb_desc.icalkl_shift = AW_PID_2071_ICALKL_SHIFT;
	aw_pa->vcalb_desc.icalk_sign_mask = AW_PID_2071_EF_ISN_GESLP_SIGN_MASK;
	aw_pa->vcalb_desc.icalk_neg_mask = AW_PID_2071_EF_ISN_GESLP_NEG;

	aw_pa->vcalb_desc.vcalk_reg = AW_PID_2071_EFRH_REG;
	aw_pa->vcalb_desc.vcalk_reg_mask = AW_PID_2071_EF_VSN_GESLP_MASK;
	aw_pa->vcalb_desc.vcalkl_reg = AW_REG_NONE;
	aw_pa->vcalb_desc.vcalk_sign_mask = AW_PID_2071_EF_VSN_GESLP_SIGN_MASK;
	aw_pa->vcalb_desc.vcalk_neg_mask = AW_PID_2071_EF_VSN_GESLP_NEG;
	aw_pa->vcalb_desc.vcalk_value_factor = AW_PID_2071_VCABLK_FACTOR;

	aw_pa->sysst_desc.reg = AW_PID_2071_SYSST_REG;
	aw_pa->sysst_desc.mask = AW_PID_2071_SYSST_CHECK_MASK;
	aw_pa->sysst_desc.st_check = AW_PID_2071_SYSST_CHECK;
	aw_pa->sysst_desc.pll_check = AW_PID_2071_PLLS_LOCKED_VALUE;

	aw_pa->cco_mux_desc.reg = AW_PID_2071_PLLCTRL1_REG;
	aw_pa->cco_mux_desc.mask = AW_PID_2071_CCO_MUX_MASK;
	aw_pa->cco_mux_desc.divided_val = AW_PID_2071_CCO_MUX_DIVIDED_VALUE;
	aw_pa->cco_mux_desc.bypass_val = AW_PID_2071_CCO_MUX_BYPASS_VALUE;

	aw_pa->voltage_desc.reg = AW_PID_2071_VBAT_REG;
	aw_pa->voltage_desc.int_bit = AW_PID_2071_MONITOR_INT_10BIT;
	aw_pa->voltage_desc.vbat_range = AW_PID_2071_MONITOR_VBAT_RANGE;

	aw_pa->temp_desc.reg = AW_PID_2071_TEMP_REG;
	aw_pa->temp_desc.neg_mask = AW_PID_2071_MONITOR_TEMP_NEG_MASK;
	aw_pa->temp_desc.sign_mask = AW_PID_2071_MONITOR_TEMP_SIGN_MASK;

	aw_pa->ipeak_desc.reg = AW_PID_2071_SYSCTRL2_REG;
	aw_pa->ipeak_desc.mask = AW_PID_2071_BST_IPEAK_MASK;

	aw_pa->volume_desc.reg = AW_PID_2071_SYSCTRL2_REG;
	aw_pa->volume_desc.mask = AW_PID_2071_VOL_MASK;
	aw_pa->volume_desc.shift = AW_PID_2071_VOL_START_BIT;
	aw_pa->volume_desc.mute_volume = AW_PID_2071_MUTE_VOL;

	aw_pa->bop_desc.reg = AW_REG_NONE;

	aw_pa->soft_rst.reg = AW_PID_2071_ID_REG;
	aw_pa->soft_rst.reg_value = AW882XX_SOFT_RESET_VALUE;

	ret = aw_device_probe(aw_pa);

	aw882xx->aw_pa = aw_pa;
	return ret;
}

/* [9 : 6]: -6DB ; [5 : 0]: 0.125DB  real_value = value * 8 : 0.125db --> 1 */
static unsigned int aw_pid_2113_reg_val_to_db(unsigned int value)
{
	return ((value >> 6) * AW_PID_2113_VOL_STEP_DB + (value & 0x3f));
}

/* [9 : 6]: -6DB ; [5 : 0]: 0.125DB reg_value = value / step << 6 + value % step ; step = 6 * 8 */
static unsigned int aw_pid_2113_db_val_to_reg(unsigned int value)
{
	return (((value / AW_PID_2113_VOL_STEP_DB) << 6) + (value % AW_PID_2113_VOL_STEP_DB));
}

static int aw_pid_2113_set_volume(struct aw_device *aw_dev, unsigned int value)
{
	struct aw882xx *aw882xx = (struct aw882xx *)aw_dev->private_data;
	unsigned int reg_value = 0;
	unsigned int real_value = aw_pid_2113_db_val_to_reg(value);

	/* cal real value */
	aw882xx_i2c_read(aw882xx, AW_PID_2113_SYSCTRL2_REG, &reg_value);

	aw_dev_dbg(aw882xx->dev, "value %d , 0x%x", value, real_value);

	/*[9 : 0] volume*/
	real_value = (real_value | (reg_value & 0xfc00));

	/* write value */
	aw882xx_i2c_write(aw882xx, AW_PID_2113_SYSCTRL2_REG, real_value);
	return 0;
}

static int aw_pid_2113_get_volume(struct aw_device *aw_dev, unsigned int *value)
{
	unsigned int reg_value = 0;
	unsigned int real_value = 0;
	struct aw882xx *aw882xx = (struct aw882xx *)aw_dev->private_data;

	/* read value */
	aw882xx_i2c_read(aw882xx, AW_PID_2113_SYSCTRL2_REG, &reg_value);

	/* [9 : 0] volume */
	real_value = (reg_value & 0x03ff);

	real_value = aw_pid_2113_reg_val_to_db(real_value);
	*value = real_value;

	return 0;
}

static void aw_pid_2113_i2s_enable(struct aw_device *aw_dev, bool flag)
{
	struct aw882xx *aw882xx = (struct aw882xx *)aw_dev->private_data;
	struct aw_cali_desc *desc = &aw_dev->cali_desc;
	aw_dev_dbg(aw882xx->dev, "enter");

	if (!desc->mode) {
		aw_dev_dbg(aw_dev->dev, "cali mode is none, needn't set i2s status");
		return;
	}

	if (flag) {
		aw882xx_i2c_write_bits(aw882xx, AW_PID_2113_I2SCTRL3_REG,
				AW_PID_2113_I2STXEN_MASK,
				AW_PID_2113_I2STXEN_ENABLE_VALUE);
	} else {
		aw882xx_i2c_write_bits(aw882xx, AW_PID_2113_I2SCTRL3_REG,
				AW_PID_2113_I2STXEN_MASK,
				AW_PID_2113_I2STXEN_DISABLE_VALUE);
	}
}

static bool aw_pid_2113_check_rd_access(int reg)
{
	if (reg >= AW_PID_2113_REG_MAX)
		return false;

	if (aw_pid_2113_reg_access[reg] & AW_PID_2113_REG_RD_ACCESS)
		return true;
	else
		return false;
}

static bool aw_pid_2113_check_wr_access(int reg)
{
	if (reg >= AW_PID_2113_REG_MAX)
		return false;

	if (aw_pid_2113_reg_access[reg] &
			AW_PID_2113_REG_WR_ACCESS)
		return true;
	else
		return false;
}

static int aw_pid_2113_get_reg_num(void)
{
	return AW_PID_2113_REG_MAX;
}

static unsigned int aw_pid_2113_get_irq_type(struct aw_device *aw_dev,
					unsigned int value)
{
	unsigned int ret = INT_TYPE_NONE;

	/* UVL0 */
	if (value & (~AW_PID_2113_UVLI_MASK)) {
		aw_dev_info(aw_dev->dev, "UVLO: occur");
		ret |= INT_TYPE_UVLO;
	}

	/* BSTOCM */
	if (value & (~AW_PID_2113_BSTOCI_MASK)) {
		aw_dev_info(aw_dev->dev, "BSTOCI: occur");
		ret |= INT_TYPE_BSTOC;
	}

	/* OCDI */
	if (value & (~AW_PID_2113_OCDI_MASK)) {
		aw_dev_info(aw_dev->dev, "OCDI: occur");
		ret |= INT_TYPE_OCDI;
	}

	/* OTHI */
	if (value & (~AW_PID_2113_OTHI_MASK)) {
		aw_dev_info(aw_dev->dev, "OTHI: occur");
		ret |= INT_TYPE_OTHI;
	}

	return ret;
}

static void aw_pid_2113_efver_check(struct aw_device *aw_dev)
{
	unsigned int reg_val = 0;
	uint16_t temh = 0;
	uint16_t teml = 0;

	aw_dev->ops.aw_i2c_read(aw_dev,
			AW_PID_2113_EFRH3_REG, &reg_val);
	temh = ((uint16_t)reg_val & (~AW_PID_2113_TEMH_MASK));

	aw_dev->ops.aw_i2c_read(aw_dev,
			AW_PID_2113_EFRL3_REG, &reg_val);
	teml = ((uint16_t)reg_val & (~AW_PID_2113_TEML_MASK));

	if ((temh | teml) == AW_PID_2113_DEFAULT_CFG)
		aw_dev->frcset_en = AW_FRCSET_ENABLE;
	else
		aw_dev->frcset_en = AW_FRCSET_DISABLE;

	aw_dev_info(aw_dev->dev, "tem is 0x%04x, frcset_en is %d",
						(temh | teml), aw_dev->frcset_en);
}

static void aw_pid_2113_reg_force_set(struct aw_device *aw_dev)
{
	aw_dev_dbg(aw_dev->dev, "enter");

	if (aw_dev->frcset_en == AW_FRCSET_ENABLE) {
		/*set FORCE_PWM*/
		aw_dev->ops.aw_i2c_write_bits(aw_dev, AW_PID_2113_BSTCTRL3_REG,
				AW_PID_2113_FORCE_PWM_MASK, AW_PID_2113_FORCE_PWM_FORCEMINUS_PWM_VALUE);
		/*set BOOST_OC_WIDTH*/
		aw_dev->ops.aw_i2c_write_bits(aw_dev, AW_PID_2113_BSTCTRL5_REG,
				AW_PID_2113_BST_OS_WIDTH_MASK, AW_PID_2113_BST_OS_WIDTH_50NS_VALUE);
		/*set BURST_LOOPR*/
		aw_dev->ops.aw_i2c_write_bits(aw_dev, AW_PID_2113_BSTCTRL6_REG,
				AW_PID_2113_BST_LOOPR_MASK, AW_PID_2113_BST_LOOPR_340K_VALUE);
		/*set RSQN_DLY*/
		aw_dev->ops.aw_i2c_write_bits(aw_dev, AW_PID_2113_BSTCTRL7_REG,
				AW_PID_2113_RSQN_DLY_MASK, AW_PID_2113_RSQN_DLY_35NS_VALUE);
		/*set BURST_SSMODE*/
		aw_dev->ops.aw_i2c_write_bits(aw_dev, AW_PID_2113_BSTCTRL8_REG,
				AW_PID_2113_BURST_SSMODE_MASK, AW_PID_2113_BURST_SSMODE_FAST_VALUE);
		/*set BST_BURST*/
		aw_dev->ops.aw_i2c_write_bits(aw_dev, AW_PID_2113_BSTCTRL9_REG,
				AW_PID_2113_BST_BURST_MASK, AW_PID_2113_BST_BURST_30MA_VALUE);
		aw_dev_dbg(aw_dev->dev, "force set reg done!");
	}else {
		aw_dev_info(aw_dev->dev, "needn't set reg value");
	}
}

static int aw_pid_2113_dev_init(struct aw882xx *aw882xx, int index)
{
	int ret;
	struct aw_device *aw_pa = NULL;
	aw_pa = devm_kzalloc(aw882xx->dev, sizeof(struct aw_device), GFP_KERNEL);
	if (aw_pa == NULL) {
		aw_dev_err(aw882xx->dev, "dev kalloc failed");
		return -ENOMEM;
	}

	/*call aw device init func*/
	memset(aw_pa->acf_name, 0, AW_NAME_MAX);
	memcpy(aw_pa->acf_name, AW_PID_2113_ACF_FILE, strlen(AW_PID_2113_ACF_FILE));
	memset(aw_pa->monitor_name, 0, AW_NAME_MAX);
	memcpy(aw_pa->monitor_name, AW_PID_2113_MONITOR_FILE, strlen(AW_PID_2113_MONITOR_FILE));

	aw_pa->prof_info.prof_desc = NULL;
	aw_pa->prof_info.count = 0;
	aw_pa->channel = 0;
	aw_pa->bstcfg_enable = AW_BSTCFG_DISABLE;
	aw_pa->bop_en = AW_BOP_DISABLE;
	aw_pa->vol_step = AW_PID_2113_VOL_STEP;

	aw_pa->index = index;
	aw_pa->private_data = (void *)aw882xx;
	aw_pa->dev = aw882xx->dev;
	aw_pa->i2c = aw882xx->i2c;
	aw_pa->ops.aw_get_version = aw882xx_get_version;
	aw_pa->ops.aw_get_dev_num = aw882xx_get_dev_num;
	aw_pa->ops.aw_set_algo = aw882xx_dev_set_algo_en;
	aw_pa->ops.aw_i2c_read = aw882xx_dev_i2c_read;
	aw_pa->ops.aw_i2c_write = aw882xx_dev_i2c_write;
	aw_pa->ops.aw_i2c_write_bits = aw882xx_dev_i2c_write_bits;
	aw_pa->ops.aw_get_volume = aw_pid_2113_get_volume;
	aw_pa->ops.aw_set_volume = aw_pid_2113_set_volume;
	aw_pa->ops.aw_reg_val_to_db = aw_pid_2113_reg_val_to_db;
	aw_pa->ops.aw_i2s_enable = aw_pid_2113_i2s_enable;
	aw_pa->ops.aw_check_rd_access = aw_pid_2113_check_rd_access;
	aw_pa->ops.aw_check_wr_access = aw_pid_2113_check_wr_access;
	aw_pa->ops.aw_get_reg_num = aw_pid_2113_get_reg_num;
	aw_pa->ops.aw_get_irq_type = aw_pid_2113_get_irq_type;
	aw_pa->ops.aw_reg_force_set = aw_pid_2113_reg_force_set;

	aw_pa->int_desc.mask_reg = AW_PID_2113_SYSINTM_REG;
	aw_pa->int_desc.mask_default = AW_PID_2113_SYSINTM_DEFAULT;
	aw_pa->int_desc.int_mask = AW_PID_2113_SYSINTM_DEFAULT;
	aw_pa->int_desc.st_reg = AW_PID_2113_SYSINT_REG;

	aw_pa->pwd_desc.reg = AW_PID_2113_SYSCTRL_REG;
	aw_pa->pwd_desc.mask = AW_PID_2113_PWDN_MASK;
	aw_pa->pwd_desc.enable = AW_PID_2113_PWDN_POWER_DOWN_VALUE;
	aw_pa->pwd_desc.disable = AW_PID_2113_PWDN_WORKING_VALUE;

	aw_pa->amppd_desc.reg = AW_PID_2113_SYSCTRL_REG;
	aw_pa->amppd_desc.mask = AW_PID_2113_AMPPD_MASK;
	aw_pa->amppd_desc.enable = AW_PID_2113_AMPPD_POWER_DOWN_VALUE;
	aw_pa->amppd_desc.disable = AW_PID_2113_AMPPD_WORKING_VALUE;

	aw_pa->mute_desc.reg = AW_PID_2113_SYSCTRL_REG;
	aw_pa->mute_desc.mask = AW_PID_2113_HMUTE_MASK;
	aw_pa->mute_desc.enable = AW_PID_2113_HMUTE_ENABLE_VALUE;
	aw_pa->mute_desc.disable = AW_PID_2113_HMUTE_DISABLE_VALUE;

	aw_pa->vcalb_desc.vcalb_reg = AW_PID_2113_VSNTM1_REG;
	aw_pa->vcalb_desc.vcal_factor = AW_PID_2113_VCAL_FACTOR;
	aw_pa->vcalb_desc.cabl_base_value = AW_PID_2113_CABL_BASE_VALUE;

	aw_pa->vcalb_desc.icalk_reg = AW_PID_2113_EFRH4_REG;
	aw_pa->vcalb_desc.icalk_reg_mask = AW_PID_2113_EF_ISN_GESLP_H_MASK;
	aw_pa->vcalb_desc.icalk_shift = AW_PID_2113_ICALK_SHIFT;
	aw_pa->vcalb_desc.icalkl_reg = AW_PID_2113_EFRL4_REG;
	aw_pa->vcalb_desc.icalkl_reg_mask = AW_PID_2113_EF_ISN_GESLP_L_MASK;
	aw_pa->vcalb_desc.icalkl_shift = AW_PID_2113_ICALKL_SHIFT;
	aw_pa->vcalb_desc.icalk_sign_mask = AW_PID_2113_EF_ISN_GESLP_SIGN_MASK;
	aw_pa->vcalb_desc.icalk_neg_mask = AW_PID_2113_EF_ISN_GESLP_NEG;
	aw_pa->vcalb_desc.icalk_value_factor = AW_PID_2113_ICABLK_FACTOR;

	aw_pa->vcalb_desc.vcalk_reg = AW_PID_2113_EFRH3_REG;
	aw_pa->vcalb_desc.vcalk_reg_mask = AW_PID_2113_EF_VSN_GESLP_H_MASK;
	aw_pa->vcalb_desc.vcalk_shift = AW_PID_2113_VCALK_SHIFT;
	aw_pa->vcalb_desc.vcalkl_reg = AW_PID_2113_EFRL3_REG;
	aw_pa->vcalb_desc.vcalkl_reg_mask = AW_PID_2113_EF_VSN_GESLP_L_MASK;
	aw_pa->vcalb_desc.vcalkl_shift = AW_PID_2113_VCALKL_SHIFT;
	aw_pa->vcalb_desc.vcalk_sign_mask = AW_PID_2113_EF_VSN_GESLP_SIGN_MASK;
	aw_pa->vcalb_desc.vcalk_neg_mask = AW_PID_2113_EF_VSN_GESLP_NEG;
	aw_pa->vcalb_desc.vcalk_value_factor = AW_PID_2113_VCABLK_FACTOR;

	aw_pa->sysst_desc.reg = AW_PID_2113_SYSST_REG;
	aw_pa->sysst_desc.mask = AW_PID_2113_SYSST_CHECK_MASK;
	aw_pa->sysst_desc.st_check = AW_PID_2113_SYSST_CHECK;
	aw_pa->sysst_desc.pll_check = AW_PID_2113_PLLS_LOCKED_VALUE;

	aw_pa->cco_mux_desc.reg = AW_PID_2113_PLLCTRL1_REG;
	aw_pa->cco_mux_desc.mask = AW_PID_2113_CCO_MUX_MASK;
	aw_pa->cco_mux_desc.divided_val = AW_PID_2113_CCO_MUX_DIVIDED_VALUE;
	aw_pa->cco_mux_desc.bypass_val = AW_PID_2113_CCO_MUX_BYPASS_VALUE;

	aw_pa->voltage_desc.reg = AW_PID_2113_VBAT_REG;
	aw_pa->voltage_desc.int_bit = AW_PID_2113_MONITOR_INT_10BIT;
	aw_pa->voltage_desc.vbat_range = AW_PID_2113_MONITOR_VBAT_RANGE;

	aw_pa->temp_desc.reg = AW_PID_2113_TEMP_REG;
	aw_pa->temp_desc.neg_mask = AW_PID_2113_MONITOR_TEMP_NEG_MASK;
	aw_pa->temp_desc.sign_mask = AW_PID_2113_MONITOR_TEMP_SIGN_MASK;

	aw_pa->ipeak_desc.reg = AW_PID_2113_BSTCTRL2_REG;
	aw_pa->ipeak_desc.mask = AW_PID_2113_BST_IPEAK_MASK;

	aw_pa->volume_desc.reg = AW_PID_2113_SYSCTRL2_REG;
	aw_pa->volume_desc.mask = AW_PID_2113_VOL_MASK;
	aw_pa->volume_desc.shift = AW_PID_2113_VOL_START_BIT;
	aw_pa->volume_desc.mute_volume = AW_PID_2113_MUTE_VOL;

	aw_pa->bop_desc.reg = AW_PID_2113_SADCCTRL3_REG;
	aw_pa->bop_desc.mask = AW_PID_2113_BOP_EN_MASK;
	aw_pa->bop_desc.enable = AW_PID_2113_BOP_EN_ENABLE_VALUE;
	aw_pa->bop_desc.disbale = AW_PID_2113_BOP_EN_DISABLE_VALUE;

	aw_pa->soft_rst.reg = AW_PID_2113_ID_REG;
	aw_pa->soft_rst.reg_value = AW882XX_SOFT_RESET_VALUE;
	aw_pid_2113_efver_check(aw_pa);

	ret = aw_device_probe(aw_pa);

	aw882xx->aw_pa = aw_pa;
	return ret;
}

int aw882xx_init(struct aw882xx *aw882xx, int index)
{
	if (aw882xx->chip_id == PID_1852_ID)
		return aw_pid_1852_dev_init(aw882xx, index);
	else if (aw882xx->chip_id == PID_2013_ID)
		return aw_pid_2013_dev_init(aw882xx, index);
	else if (aw882xx->chip_id == PID_2032_ID)
		return aw_pid_2032_dev_init(aw882xx, index);
	else if (aw882xx->chip_id == PID_2055_ID)
		return aw_pid_2055_dev_init(aw882xx, index);
	else if (aw882xx->chip_id == PID_2071_ID)
		return aw_pid_2071_dev_init(aw882xx, index);
	else if (aw882xx->chip_id == PID_2113_ID)
		return aw_pid_2113_dev_init(aw882xx, index);

	aw_dev_err(aw882xx->dev, "unsupported chip id 0x%04x", aw882xx->chip_id);
	return -EINVAL;
}

