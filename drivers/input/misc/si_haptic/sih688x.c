/*
 *  Silicon Integrated Co., Ltd haptic sih688x driver file
 *
 *  Copyright (c) 2021 kugua <canzhen.peng@si-in.com>
 *  Copyright (c) 2021 tianchi <tianchi.zheng@si-in.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation
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

#include "sih688x_reg.h"
#include "sih688x.h"
#include "haptic.h"
#include "haptic_mid.h"
#include "haptic_regmap.h"
#include "sih688x_func_config.h"
#include "xm-haptic.h"

/***********************************************
*
* chip reg config
*
***********************************************/
static void sih688x_software_reset(sih_haptic_t *sih_haptic)
{
	uint8_t reg_value = SIH_688X_ID_SOFTWARE_RESET;

	haptic_regmap_write(sih_haptic->regmapp.regmapping,
		SIH688X_REG_ID, SIH_I2C_OPERA_BYTE_ONE, &reg_value);

	usleep_range(3500, 4000);
}

static void sih688x_hardware_reset(sih_haptic_t *sih_haptic)
{
	uint8_t reg_value = SIH_688X_ID_HARDWARE_RESET;

	haptic_regmap_write(sih_haptic->regmapp.regmapping,
		SIH688X_REG_ID, SIH_I2C_OPERA_BYTE_ONE, &reg_value);

	usleep_range(3500, 4000);
}

static int sih688x_probe(sih_haptic_t *sih_haptic)
{
	int ret = -1;
	uint8_t i;
	uint8_t chip_id_value = 0;

	for (i = 0; i < SIH688X_READ_CHIP_ID_MAX_TRY; i++) {
		ret = i2c_read_bytes(sih_haptic, SIH688X_CHIPID_REG_ADDR,
			&chip_id_value, SIH_I2C_OPERA_BYTE_ONE);
		if (ret < 0) {
			hp_err("%s:i2c read id failed\n", __func__);
		} else {
			if (chip_id_value == SIH688X_CHIPID_REG_VALUE) {
				hp_info("%s:i2c read id success, id is %d\n",
					__func__, chip_id_value);
				return 0;
			}
		}
		usleep_range(2000, 2500);
	}

	return -ENODEV;
}

static void sih688x_ram_init(sih_haptic_t *sih_haptic, bool flag)
{
	hp_info("%s:enter! flag:%d\n", __func__, flag);
	if (flag) {
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			SIH688X_REG_SYSCTRL1, SIH_SYSCTRL1_BIT_ENRAMINIT_MASK,
			SIH_SYSCTRL1_BIT_RAMINIT_EN);
	} else {
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			SIH688X_REG_SYSCTRL1, SIH_SYSCTRL1_BIT_ENRAMINIT_MASK,
			SIH_SYSCTRL1_BIT_RAMINIT_OFF);
	}
}

static void sih688x_detect_fifo_ctrl(sih_haptic_t *sih_haptic, bool flag)
{
	hp_info("%s:enter! flag:%d\n", __func__, flag);
	if (flag) {
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			SIH688X_REG_SMOOTH_F0_WINDOW_OUT, SIH_DETECT_FIFO_CTRL_MASK,
			SIH_DETECT_FIFO_CTRL_EN);
	} else {
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			SIH688X_REG_SMOOTH_F0_WINDOW_OUT, SIH_DETECT_FIFO_CTRL_MASK,
			SIH_DETECT_FIFO_CTRL_OFF);
	}
}

static void sih688x_f0_tracking(sih_haptic_t *sih_haptic, bool flag)
{
	hp_info("%s:enter! flag:%d\n", __func__, flag);
	if (flag) {
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			SIH688X_REG_MAIN_STATE_CTRL, SIH_MODECTRL_BIT_TRACK_F0_MASK,
			SIH_MODECTRL_BIT_TRACK_F0_EN);
	} else {
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			SIH688X_REG_MAIN_STATE_CTRL, SIH_MODECTRL_BIT_TRACK_F0_MASK,
			SIH_MODECTRL_BIT_TRACK_F0_OFF);
	}
}

static void sih688x_detect_done_int(sih_haptic_t *sih_haptic, bool flag)
{
	hp_info("%s:enter! flag:%d\n", __func__, flag);
	if (flag) {
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			SIH688X_REG_SYSINTM2, SIH_SYSINT2_BIT_F0_DETECT_DONE_INT_MASK,
			SIH_SYSINT2_BIT_F0_DETECT_DONE_INT_EN);
	} else {
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			SIH688X_REG_SYSINTM2, SIH_SYSINT2_BIT_F0_DETECT_DONE_INT_MASK,
			SIH_SYSINT2_BIT_F0_DETECT_DONE_INT_OFF);
	}
}

static void sih688x_set_boost_mode(sih_haptic_t *sih_haptic, bool flag)
{
	hp_info("%s:haptic boost mode %d\n", __func__, flag);
	if (flag) {
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			SIH688X_REG_SYSCTRL2, SIH_SYSCTRL2_BIT_BOOST_BYPASS_MASK,
			SIH_SYSCTRL2_BIT_BOOST_ENABLE);
	} else {
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			SIH688X_REG_SYSCTRL2, SIH_SYSCTRL2_BIT_BOOST_BYPASS_MASK,
			SIH_SYSCTRL2_BIT_BOOST_BYPASS);
	}
}

static void sih688x_set_go_enable(sih_haptic_t *sih_haptic,
	uint8_t play_mode)
{
	hp_info("%s:enter!go mode:%d\n", __func__, play_mode);
	switch (play_mode) {
	case SIH_RAM_MODE:
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			SIH688X_REG_GO, SIH_GO_BIT_RAM_GO_MASK, SIH_GO_BIT_RAM_GO_ENABLE);
		break;
	case SIH_RTP_MODE:
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			SIH688X_REG_GO, SIH_GO_BIT_RTP_GO_MASK, SIH_GO_BIT_RTP_GO_ENABLE);
		break;
	case SIH_CONT_MODE:
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			SIH688X_REG_GO, SIH_GO_BIT_F0_SEQ_GO_MASK,
			SIH_GO_BIT_F0_SEQ_GO_ENABLE);
		break;
	case SIH_RAM_LOOP_MODE:
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			SIH688X_REG_GO, SIH_GO_BIT_RAM_GO_MASK, SIH_GO_BIT_RAM_GO_ENABLE);
		break;
	default:
		hp_err("%s:play mode %d no need to go\n",  __func__,
			sih_haptic->chip_ipara.play_mode);
		break;
	}
}

static void sih688x_set_go_disable(sih_haptic_t *sih_haptic)
{
    uint8_t reg_val = 0x10;

	hp_info("%s:enter!\n", __func__);
    haptic_regmap_write(sih_haptic->regmapp.regmapping,
		SIH688X_REG_GO, SIH_I2C_OPERA_BYTE_ONE, &reg_val);
}

static void sih688x_play_go(sih_haptic_t *sih_haptic, bool flag)
{
	hp_info("%s:enter, play_mode = %d, flag = %d\n", __func__,
		sih_haptic->chip_ipara.play_mode, flag);
	if (flag) {
		sih688x_set_go_enable(sih_haptic, sih_haptic->chip_ipara.play_mode);
		sih_haptic->chip_ipara.kpre_time = ktime_get();
	} else {
		sih_haptic->chip_ipara.kcur_time = ktime_get();
		sih_haptic->chip_ipara.interval_us =
			ktime_to_us(ktime_sub(sih_haptic->chip_ipara.kcur_time,
			sih_haptic->chip_ipara.kpre_time));
		if (sih_haptic->chip_ipara.interval_us < 2000) {
			hp_info("%s:sih688x->interval_us = %d < 2000\n",
				__func__, sih_haptic->chip_ipara.interval_us);
			usleep_range(1000, 1200);
		}
		sih688x_set_go_disable(sih_haptic);
	}
}

static void sih688x_clear_interrupt_state(sih_haptic_t *sih_haptic)
{
	uint8_t reg_val = 0;

	hp_info("%s:enter\n", __func__);
	haptic_regmap_read(sih_haptic->regmapp.regmapping,
		SIH688X_REG_SYSINT, SIH_I2C_OPERA_BYTE_ONE, &reg_val);
}

static void sih688x_set_gain(sih_haptic_t *sih_haptic, uint8_t gain)
{
	hp_info("%s:set gain:0x%02x\n", __func__, gain);
	haptic_regmap_write(sih_haptic->regmapp.regmapping,
		SIH688X_REG_PWM_PRE_GAIN, SIH_I2C_OPERA_BYTE_ONE, &gain);
}

static void sih688x_f0_cali(sih_haptic_t *sih_haptic)
{
	int32_t tmp = 0;
	int32_t code = 0;

	xm_hap_driver_init(true);
	hp_info("%s:sih_haptic->detect.tracking_f0:%d\n", __func__, sih_haptic->detect.tracking_f0);
	if (sih_haptic->detect.tracking_f0 > SIH688X_F0_VAL_MAX ||
		sih_haptic->detect.tracking_f0 < SIH688X_F0_VAL_MIN) {
		code = 0;
		XM_HAP_F0_CAL_EXCEPTION(sih_haptic->detect.tracking_f0, sih_haptic->detect.cali_target_value, "F0 Calibration failed");
	} else {
		tmp = (int32_t)(sih_haptic->detect.tracking_f0 * SIH688X_F0_CAL_COE
			/ sih_haptic->detect.cali_target_value);
		code = (tmp - SIH688X_F0_CAL_COE) / SIH688X_F0_CALI_DELTA;
		/*
		* f0 calibration formulation:
		*
		* code = (tracking_f0 / target_f0 - 1) / 0.00288
		*
		* 0.00288 is calc coefficient
		*/
	}
	hp_info("%s:cali data:0x%02x\n", __func__, code);
	sih_haptic->detect.f0_cali_data = (uint8_t)code;
}

static void sih688x_upload_f0(sih_haptic_t *sih_haptic, uint8_t flag)
{
	uint8_t reg_val = 0;

	switch (flag) {
	case SIH_WRITE_ZERO:
		reg_val = 0;
		break;
	case SIH_F0_CALI_LRA:
		sih688x_f0_cali(sih_haptic);
		reg_val = sih_haptic->detect.f0_cali_data;
		break;
	case SIH_OSC_CALI_LRA:
		reg_val = sih_haptic->osc_para.osc_data;
		break;
	default:
		hp_err("%s:err flag\n", __func__);
		break;
	}
	hp_info("%s:trim code:0x%02x\n", __func__, reg_val);
	haptic_regmap_write(sih_haptic->regmapp.regmapping, SIH688X_REG_TRIM1,
		SIH_I2C_OPERA_BYTE_ONE, &reg_val);

}


static void sih688x_set_play_mode(sih_haptic_t *sih_haptic,
	uint8_t play_mode)
{
	uint8_t reg_val = 0;

	hp_info("%s:enter!play mode = %d\n", __func__, play_mode);

	switch (play_mode) {
	case SIH_IDLE_MODE:
		sih_haptic->chip_ipara.play_mode = SIH_IDLE_MODE;
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			SIH688X_REG_SYSINTM, SIH_SYSINT_BIT_UVP_FLAG_INT_MASK,
			SIH_SYSINT_BIT_UVP_FLAG_INT_OFF);
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			SIH688X_REG_SYSCTRL1, SIH_SYSCTRL1_BIT_STOP_MODE_MASK,
			SIH_SYSCTRL1_BIT_STOP_RIGHT_NOW);
		reg_val = SIH_GO_BIT_RAM_GO_DISABLE | SIH_GO_BIT_RTP_GO_DISABLE |
			SIH_GO_BIT_F0_SEQ_GO_DISABLE | SIH_GO_BIT_STOP_TRIG_EN;
		haptic_regmap_write(sih_haptic->regmapp.regmapping,
			SIH688X_REG_GO, SIH_I2C_OPERA_BYTE_ONE, &reg_val);
		usleep_range(2000, 2500);
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			SIH688X_REG_SYSCTRL1, SIH_SYSCTRL1_BIT_STOP_MODE_MASK,
			SIH_SYSCTRL1_BIT_STOP_CUR_OVER);
		hp_info("%s:now chip is stanby\n", __func__);
		break;
	case SIH_RAM_MODE:
		sih_haptic->chip_ipara.play_mode = SIH_RAM_MODE;
		sih688x_upload_f0(sih_haptic, SIH_F0_CALI_LRA);
		sih688x_set_boost_mode(sih_haptic, true);
		break;
	case SIH_RTP_MODE:
		sih_haptic->chip_ipara.play_mode = SIH_RTP_MODE;
		sih688x_upload_f0(sih_haptic, SIH_OSC_CALI_LRA);
		sih688x_set_boost_mode(sih_haptic, true);
		break;
	case SIH_TRIG_MODE:
		sih_haptic->chip_ipara.play_mode = SIH_TRIG_MODE;
		break;
	case SIH_CONT_MODE:
		sih_haptic->chip_ipara.play_mode = SIH_CONT_MODE;
		break;
	case SIH_RAM_LOOP_MODE:
		sih_haptic->chip_ipara.play_mode = SIH_RAM_LOOP_MODE;
		sih688x_upload_f0(sih_haptic, SIH_F0_CALI_LRA);
		sih688x_set_boost_mode(sih_haptic, false);
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			SIH688X_REG_WAVELOOP1, SIH_WAVELOOP1_BIT_SEQ1_MASK,
			SIH_WAVELOOP1_BIT_SEQ1_INFINITE);
		break;
	default:
		hp_err("%s:play mode %d err\n", __func__, play_mode);
		break;
	}
	sih688x_clear_interrupt_state(sih_haptic);
}

static void sih688x_set_brk_bst_vol(sih_haptic_t *sih_haptic,
	uint32_t bst_vol)
{
	uint32_t tmp = 0;
	uint8_t write_value = 0;

	tmp = bst_vol * SIH688X_VBOOST_MUL_COE / SIH688X_BRK_VBOOST_COE;
	write_value = (uint8_t)((tmp >> 8) & 0xff);
	/* digital and analog module need to be set separately */
	haptic_regmap_write(sih_haptic->regmapp.regmapping,
		SIH688X_REG_V_BOOST, SIH_I2C_OPERA_BYTE_ONE, &write_value);
	haptic_regmap_write(sih_haptic->regmapp.regmapping,
		SIH688X_REG_V_BOOST, SIH_I2C_OPERA_BYTE_ONE, &write_value);
}

static void sih688x_set_drv_bst_vol(sih_haptic_t *sih_haptic,
	uint32_t drv_bst)
{
	uint8_t bst_vol = 0;
	uint8_t bst_reg_val = 0;

	/*
	* drv boost calc formulation:
	*
	* reg_val = drv_bst - 3.5 / 0.0625
	*
	* 3.5 is base voltage
	* 0.00625 is step
	*
	*/
	bst_reg_val = (uint8_t)(((drv_bst - SIH688X_DRV_BOOST_BASE) *
		SIH688X_DRV_BOOST_SETP_COE) / SIH688X_DRV_BOOST_SETP);

	haptic_regmap_write(sih_haptic->regmapp.regmapping,
		SIH688X_REG_ANA_CTRL3, SIH_I2C_OPERA_BYTE_ONE, &bst_reg_val);

	bst_vol = drv_bst / SIH688X_DRV_VBOOST_COEFFICIENT;

	if (drv_bst >= SIH_ANA_CTRL_BST_LEVEL_6 &&
		drv_bst <= SIH_ANA_CTRL_BST_LEVEL_8) {
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			SIH688X_REG_ANA_CTRL5, SIH_ANA_CTRL5_BST_IOS_SEL_O_MASK,
			SIH_ANA_CTRL5_BST_IOS_SEL_6_8);
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			SIH688X_REG_ANA_CTRL5, SIH_ANA_CTRL5_BST_OCP_VRSEL_O_MASK,
			SIH_ANA_CTRL5_BST_OCP_VRSEL_6_8);
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			SIH688X_REG_ANA_CTRL6, SIH_ANA_CTRL6_BST_ZCD_IOS_O_MASK,
			SIH_ANA_CTRL5_BST_ZCD_IOS_6_8);
	} else if (drv_bst > SIH_ANA_CTRL_BST_LEVEL_8 &&
		drv_bst <= SIH_ANA_CTRL_BST_LEVEL_10) {
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			SIH688X_REG_ANA_CTRL5, SIH_ANA_CTRL5_BST_IOS_SEL_O_MASK,
			SIH_ANA_CTRL5_BST_IOS_SEL_8_10);
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			SIH688X_REG_ANA_CTRL5, SIH_ANA_CTRL5_BST_OCP_VRSEL_O_MASK,
			SIH_ANA_CTRL5_BST_OCP_VRSEL_8_10);
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			SIH688X_REG_ANA_CTRL6, SIH_ANA_CTRL6_BST_ZCD_IOS_O_MASK,
			SIH_ANA_CTRL5_BST_ZCD_IOS_8_11);
	} else if (drv_bst > SIH_ANA_CTRL_BST_LEVEL_10 &&
		drv_bst <= SIH_ANA_CTRL_BST_LEVEL_11){
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			SIH688X_REG_ANA_CTRL5, SIH_ANA_CTRL5_BST_IOS_SEL_O_MASK,
			SIH_ANA_CTRL5_BST_IOS_SEL_10_11);
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			SIH688X_REG_ANA_CTRL5, SIH_ANA_CTRL5_BST_OCP_VRSEL_O_MASK,
			SIH_ANA_CTRL5_BST_OCP_VRSEL_10_11);
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			SIH688X_REG_ANA_CTRL6, SIH_ANA_CTRL6_BST_ZCD_IOS_O_MASK,
			SIH_ANA_CTRL5_BST_ZCD_IOS_8_11);
	}
}

static void sih688x_set_auto_pvdd(sih_haptic_t *sih_haptic, bool flag)
{
	sih_haptic->chip_ipara.auto_pvdd_en = flag;
	if (flag) {
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			SIH688X_REG_SYSCTRL1, SIH_SYSCTRL1_BIT_AUTO_PVDD_MASK,
			SIH_SYSCTRL1_BIT_AUTO_PVDD_EN);
	} else {
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			SIH688X_REG_SYSCTRL1, SIH_SYSCTRL1_BIT_AUTO_PVDD_MASK,
			SIH_SYSCTRL1_BIT_AUTO_PVDD_OFF);
	}
}

static void sih688x_interrupt_state_init(sih_haptic_t *sih_haptic)
{
	uint8_t reg_val = 0;
	uint8_t reg_mask = 0;

	haptic_regmap_read(sih_haptic->regmapp.regmapping,
		SIH688X_REG_SYSINT, SIH_I2C_OPERA_BYTE_ONE, &reg_val);

	hp_info("%s:int state 0x02 = 0x%02x\n", __func__, reg_val);

	/* int enable */
	reg_mask = SIH_SYSINT_BIT_OCP_FLAG_INT_MASK | SIH_SYSINTM_BIT_DONE_MASK |
		SIH_SYSINT_BIT_UVP_FLAG_INT_MASK | SIH_SYSINT_BIT_OTP_FLAG_INT_MASK |
		SIH_SYSINT_BIT_MODE_SWITCH_INT_MASK | SIH_SYSINTM_BIT_FF_AEI_MASK |
		SIH_SYSINT_BIT_BRK_LONG_TIMEOUT_MASK | SIH_SYSINTM_BIT_FF_AFI_MASK;
	reg_val = SIH_SYSINT_BIT_OCP_FLAG_INT_OFF | SIH_SYSINTM_BIT_DONE_OFF |
		SIH_SYSINT_BIT_UVP_FLAG_INT_OFF | SIH_SYSINT_BIT_OTP_FLAG_INT_OFF |
		SIH_SYSINT_BIT_MODE_SWITCH_INT_OFF | SIH_SYSINTM_BIT_FF_AEI_OFF |
		SIH_SYSINT_BIT_BRK_LONG_TIMEOUT_OFF | SIH_SYSINTM_BIT_FF_AFI_OFF;

	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH688X_REG_SYSINTM, reg_mask, reg_val);
}

static void sih688x_set_low_power_mode(sih_haptic_t *sih_haptic,
	bool flag)
{
	if (flag) {
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			SIH688X_REG_SYSCTRL1, SIH_SYSCTRL1_BIT_LOWPOWER_MASK,
			SIH_SYSCTRL1_BIT_LOWPOWER_EN);
		sih_haptic->chip_ipara.low_power = true;
	} else {
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			SIH688X_REG_SYSCTRL1, SIH_SYSCTRL1_BIT_LOWPOWER_MASK,
			SIH_SYSCTRL1_BIT_LOWPOWER_OFF);
		sih_haptic->chip_ipara.low_power = false;
	}
	hp_info("sih_haptic->chip_ipara.low_power = %d\n",
		sih_haptic->chip_ipara.low_power);
}

static void sih688x_set_brk_state(sih_haptic_t *sih_haptic,
	uint8_t mode, bool flag)
{
	uint8_t brk_en;
	uint8_t brk_mask;

	switch (mode) {
	case SIH_RAM_MODE:
		sih_haptic->brake_para.ram_brake_en = flag;
		if (flag)
			brk_en = SIH_MODECTRL_BIT_RAM_BRK_EN;
		else
			brk_en = SIH_MODECTRL_BIT_RAM_BRK_OFF;
		brk_mask = SIH_MODECTRL_BIT_RAM_BRK_MASK;
		break;
	case SIH_RTP_MODE:
		sih_haptic->brake_para.rtp_brake_en = flag;
		if (flag)
			brk_en = SIH_MODECTRL_BIT_RTP_BRK_EN;
		else
			brk_en = SIH_MODECTRL_BIT_RTP_BRK_OFF;
		brk_mask = SIH_MODECTRL_BIT_RTP_BRK_MASK;
		break;
	case SIH_TRIG_MODE:
		sih_haptic->brake_para.trig_brake_en = flag;
		if (flag)
			brk_en = SIH_MODECTRL_BIT_TRIG_BRK_EN;
		else
			brk_en = SIH_MODECTRL_BIT_TRIG_BRK_OFF;
		brk_mask = SIH_MODECTRL_BIT_TRIG_BRK_MASK;
		break;
	case SIH_CONT_MODE:
		sih_haptic->brake_para.cont_brake_en = flag;
		if (flag)
			brk_en = SIH_MODECTRL_BIT_TRACK_BRK_EN;
		else
			brk_en = SIH_MODECTRL_BIT_TRACK_BRK_OFF;
		brk_mask = SIH_MODECTRL_BIT_TRACK_BRK_MASK;
		break;
	default:
		hp_err("%s: play mode %d err\n", __func__, mode);
		return;
	}
	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH688X_REG_MAIN_STATE_CTRL, brk_mask, brk_en);
}

static size_t sih688x_get_brk_state(sih_haptic_t *sih_haptic, char *buf)
{
	uint8_t reg_val = 0;
	size_t len = 0;

	hp_info("%s:enter\n", __func__);

	haptic_regmap_read(sih_haptic->regmapp.regmapping,
		SIH688X_REG_MAIN_STATE_CTRL, SIH_I2C_OPERA_BYTE_ONE, &reg_val);

	len += snprintf(buf + len, PAGE_SIZE - len, "brake_state=%d\n", reg_val);

	return len;
}

static void sih688x_set_pwm_rate(sih_haptic_t *sih_haptic,
	uint8_t sample_rpt, uint8_t sample_en)
{
	switch (sample_rpt) {
	case SIH_SAMPLE_RPT_ONE_TIME:
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			SIH688X_REG_PWM_UP_SAMPLE_CTRL,
			SIH_PWM_UP_SAMPLE_CTRL_BIT_PWM_UP_RPT_MASK,
			SIH_PWM_UP_SAMPLE_CTRL_BIT_PWM_UP_ONE_TIME);
		break;
	case SIH_SAMPLE_RPT_TWO_TIME:
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			SIH688X_REG_PWM_UP_SAMPLE_CTRL,
			SIH_PWM_UP_SAMPLE_CTRL_BIT_PWM_UP_RPT_MASK,
			SIH_PWM_UP_SAMPLE_CTRL_BIT_PWM_UP_TWO_TIME);
		break;
	case SIH_SAMPLE_RPT_FOUR_TIME:
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			SIH688X_REG_PWM_UP_SAMPLE_CTRL,
			SIH_PWM_UP_SAMPLE_CTRL_BIT_PWM_UP_RPT_MASK,
			SIH_PWM_UP_SAMPLE_CTRL_BIT_PWM_UP_FOUR_TIME);
		break;
	default:
		hp_err("%s:pwm_state sample rpt %d err\n", __func__, sample_rpt);
		break;
	}

	if (sample_en) {
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			SIH688X_REG_PWM_UP_SAMPLE_CTRL,
			SIH_PWM_UP_SAMPLE_CTRL_BIT_PWM_SPLIT_MASK,
			SIH_PWM_UP_SAMPLE_CTRL_BIT_PWM_SPLIT_EN);
	} else {
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			SIH688X_REG_PWM_UP_SAMPLE_CTRL,
			SIH_PWM_UP_SAMPLE_CTRL_BIT_PWM_SPLIT_MASK,
			SIH_PWM_UP_SAMPLE_CTRL_BIT_PWM_SPLIT_OFF);
	}
}

static size_t sih688x_get_pwm_rate(sih_haptic_t *sih_haptic, char *buf)
{
	uint8_t pwm_state = 0;
	size_t len = 0;

	hp_info("%s:enter\n", __func__);

	haptic_regmap_read(sih_haptic->regmapp.regmapping,
		SIH688X_REG_PWM_UP_SAMPLE_CTRL, SIH_I2C_OPERA_BYTE_ONE, &pwm_state);

	len += snprintf(buf + len, PAGE_SIZE - len, "pwm_state=%d\n", pwm_state);

	return len;
}

/***********************************************
*
* chip state check
*
***********************************************/

static bool sih688x_if_chip_is_done(sih_haptic_t *sih_haptic)
{
	bool flag = false;
	uint8_t reg_val = 0;

	haptic_regmap_read(sih_haptic->regmapp.regmapping,
		SIH688X_REG_SYSINT, SIH_I2C_OPERA_BYTE_ONE, &reg_val);
	flag = (reg_val & SIH_SYSINT_BIT_DONE) == SIH_SYSINT_BIT_DONE;

	return flag;
}

static bool sih688x_if_chip_is_standby(sih_haptic_t *sih_haptic)
{
	bool flag = false;
	uint8_t reg_val = 0;

	haptic_regmap_read(sih_haptic->regmapp.regmapping,
		SIH688X_REG_SYSSST, SIH_I2C_OPERA_BYTE_ONE, &reg_val);
	flag = (reg_val & SIH_SYSSST_BIT_STANDBY) == SIH_SYSSST_BIT_STANDBY;

	return flag;
}

static bool sih688x_if_chip_is_detect_done(sih_haptic_t *sih_haptic)
{
	bool flag = false;
	uint8_t reg_val = 0;

	haptic_regmap_read(sih_haptic->regmapp.regmapping,
		SIH688X_REG_SYSINT2, SIH_I2C_OPERA_BYTE_ONE, &reg_val);
	flag = (reg_val & SIH_SYSINT2_BIT_F0_DETECT_DONE_INT) ==
		SIH_SYSINT2_BIT_F0_DETECT_DONE_INT;

	return flag;
}

static bool sih688x_if_chip_is_mode(sih_haptic_t *sih_haptic, uint8_t mode)
{
	uint8_t reg_val = 0;
	bool flag = false;

	haptic_regmap_read(sih_haptic->regmapp.regmapping,
		SIH688X_REG_SYSSST, SIH_I2C_OPERA_BYTE_ONE, &reg_val);

	switch (mode) {
	case SIH_IDLE_MODE:
		flag = (reg_val & SIH_SYSSST_BIT_STANDBY) == SIH_SYSSST_BIT_STANDBY;
		break;
	case SIH_RAM_MODE:
	case SIH_RAM_LOOP_MODE:
		flag = (reg_val & SIH_SYSSST_BIT_RAM_STATE) == SIH_SYSSST_BIT_RAM_STATE;
		break;
	case SIH_RTP_MODE:
		flag = (reg_val & SIH_SYSSST_BIT_RTP_STATE) == SIH_SYSSST_BIT_RTP_STATE;
		break;
	case SIH_TRIG_MODE:
		flag = (reg_val & SIH_SYSSST_BIT_TRIG_STATE) ==
			SIH_SYSSST_BIT_TRIG_STATE;
		break;
	case SIH_CONT_MODE:
		flag = (reg_val & SIH_SYSSST_BIT_F0_TRACK_STATE) ==
			SIH_SYSSST_BIT_F0_TRACK_STATE;
		break;
	default:
		hp_err("%s:err mode!\n", __func__);
		break;
	}

	return flag;
}

static bool sih688x_get_rtp_fifo_full_state(sih_haptic_t *sih_haptic)
{
	bool flag = false;
	uint8_t reg_val = 0;

	haptic_regmap_read(sih_haptic->regmapp.regmapping,
		SIH688X_REG_SYSSST, SIH_I2C_OPERA_BYTE_ONE, &reg_val);
	flag = (reg_val & SIH_SYSSST_BIT_FIF0_AF) == SIH_SYSSST_BIT_FIF0_AF;

	return flag;
}

static bool sih688x_get_rtp_fifo_empty_state(sih_haptic_t *sih_haptic)
{
	bool flag = false;
	uint8_t reg_val = 0;

	haptic_regmap_read(sih_haptic->regmapp.regmapping,
		SIH688X_REG_SYSINT, SIH_I2C_OPERA_BYTE_ONE, &reg_val);
	flag = (reg_val & SIH_SYSINT_BIT_FF_AEI) == SIH_SYSINT_BIT_FF_AEI;

	return flag;
}

/***********************************************
*
* chip function config
*
***********************************************/
static void sih688x_stop(sih_haptic_t *sih_haptic)
{
	uint32_t cnt = SIH_WAIT_FOR_STANDBY_MAX_TRY;

	hp_info("%s:enter\n", __func__);
	/* wait for last short vibration over */
	while (1) {
		if (!sih688x_if_chip_is_standby(sih_haptic)) {
			sih_haptic->chip_ipara.kcur_time = ktime_get();
			sih_haptic->chip_ipara.interval_us =
				ktime_to_us(ktime_sub(sih_haptic->chip_ipara.kcur_time,
				sih_haptic->chip_ipara.kpre_time));
			if (sih_haptic->chip_ipara.interval_us > SIH_PROTECTION_TIME)
				break;
			hp_info("%s:play time us = %d, less than 30ms, wait\n",
				__func__, sih_haptic->chip_ipara.interval_us);
			usleep_range(5000, 5500);
		} else {
			break;
		}
	}
	/* stop current vibration */
	sih688x_play_go(sih_haptic, false);
	/* wait for auto brake over */
	while (cnt--) {
		if (!sih688x_if_chip_is_standby(sih_haptic))
			hp_info("%s:wait for standby\n", __func__);
		else
			break;
		usleep_range(2000, 2500);
	}
	/* stop chip */
	sih688x_set_play_mode(sih_haptic, SIH_IDLE_MODE);
}

static void sih688x_update_chip_state(sih_haptic_t *sih_haptic)
{
	uint8_t reg_val = 0;

	haptic_regmap_read(sih_haptic->regmapp.regmapping,
		SIH688X_REG_SYSSST, SIH_I2C_OPERA_BYTE_ONE, &reg_val);

	if ((reg_val & SIH_SYSSST_BIT_STANDBY) == SIH_SYSSST_BIT_STANDBY)
		sih_haptic->chip_ipara.state = SIH_STANDBY_MODE;
	else
		sih_haptic->chip_ipara.state = SIH_ACTIVE_MODE;

	switch (reg_val & SIH_SYSSST_BIT_CENTRAL_STATE_MASK) {
	case SIH_SYSSST_BIT_RAM_STATE:
		sih_haptic->chip_ipara.play_mode = SIH_RAM_MODE;
		break;
	case SIH_SYSSST_BIT_RTP_STATE:
		sih_haptic->chip_ipara.play_mode = SIH_RTP_MODE;
		break;
	case SIH_SYSSST_BIT_TRIG_STATE:
		sih_haptic->chip_ipara.play_mode = SIH_TRIG_MODE;
		break;
	case SIH_SYSSST_BIT_F0_TRACK_STATE:
		sih_haptic->chip_ipara.play_mode = SIH_CONT_MODE;
		break;
	default:
		sih_haptic->chip_ipara.play_mode = SIH_IDLE_MODE;
		break;
	}
}

static int sih688x_efuse_check(sih_haptic_t *sih_haptic)
{
	uint8_t write_value = 0x02;
	uint8_t efuse_data[5] = {0};
	uint8_t crc_result = 0;
	uint8_t crc4_value;
	int ret = 0;

	haptic_regmap_write(sih_haptic->regmapp.regmapping,
		SIH688X_REG_EFUSE_CTRL, SIH_I2C_OPERA_BYTE_ONE, &write_value);
	haptic_regmap_bulk_read(sih_haptic->regmapp.regmapping,
		SIH688X_REG_EFUSE_RDATA0, SIH_I2C_OPERA_BYTE_FOUR, efuse_data);
	efuse_data[3] = efuse_data[3] & 0x0F;
	haptic_regmap_read(sih_haptic->regmapp.regmapping,
		SIH688X_REG_EFUSE_RDATA4, SIH_I2C_OPERA_BYTE_ONE, &efuse_data[4]);
	haptic_regmap_read(sih_haptic->regmapp.regmapping,
		SIH688X_REG_EFUSE_RDATA3, SIH_I2C_OPERA_BYTE_ONE, &crc4_value);
	efuse_data[4] = efuse_data[4] & 0x1F;
	crc4_value = (crc4_value & 0xE0) >> 5;
	crc_result = crc4_itu(efuse_data, sizeof(efuse_data)/sizeof(uint8_t));
	crc_result = crc_result >> 1;
	if (crc_result != crc4_value) {
		hp_err("%s: crc4 check failed\n", __func__);
		//ret = -1;
	}

	hp_info("crc_result:0x%02x crc_write:0x%02x\n", crc_result, crc4_value);
	return ret;
}

/***********************************************
*
* chip ram config
*
***********************************************/

static int sih688x_check_ram_data(sih_haptic_t *sih_haptic,
	uint8_t *cont_data, uint8_t *ram_data, uint32_t len)
{
	int i;

	for (i = 0; i < len; i++) {
		if (ram_data[i] != cont_data[i]) {
			hp_err("%s:check err,addr=0x%02x,ram=0x%02x,file=0x%02x\n",
				__func__, i, ram_data[i], cont_data[i]);
			return -ERANGE;
		}
	}
	return 0;
}

static int sih688x_update_ram_config(sih_haptic_t *sih_haptic,
	haptic_container_t *sih_cont)
{
	uint8_t fifo_addr[4] = {0};
	uint32_t shift = 0;
	int i = 0;
	int len = 0;
	int ret = -1;
	char *ram_data = NULL;

	hp_info("%s:enter\n", __func__);

	mutex_lock(&sih_haptic->lock);

	sih_haptic->ram.baseaddr_shift = 2;
	sih_haptic->ram.ram_shift = 4;
	/* RAMINIT Enable */
	sih688x_ram_init(sih_haptic, true);
	/* base addr */
	shift = sih_haptic->ram.baseaddr_shift;
	sih_haptic->ram.base_addr = (uint32_t)(sih_cont->data[0 + shift] << 8) |
		(sih_cont->data[1 + shift]);
	fifo_addr[0] = (uint8_t)SIH_FIFO_AF_ADDR_L(sih_haptic->ram.base_addr);
	fifo_addr[1] = (uint8_t)SIH_FIFO_AE_ADDR_L(sih_haptic->ram.base_addr);
	fifo_addr[2] = (uint8_t)SIH_FIFO_AF_ADDR_H(sih_haptic->ram.base_addr);
	fifo_addr[3] = (uint8_t)SIH_FIFO_AE_ADDR_H(sih_haptic->ram.base_addr);

	hp_info("%s:base_addr = 0x%04x\n", __func__, sih_haptic->ram.base_addr);
	hp_info("%s:fifo[0] = %d,[1] = %d\n", __func__, fifo_addr[0], fifo_addr[1]);
	hp_info("%s:fifo[2] = %d,[3] = %d\n", __func__, fifo_addr[2], fifo_addr[3]);

	haptic_regmap_bulk_write(sih_haptic->regmapp.regmapping,
		SIH688X_REG_BASE_ADDRH, SIH_I2C_OPERA_BYTE_TWO, &sih_cont->data[shift]);
	haptic_regmap_bulk_write(sih_haptic->regmapp.regmapping,
		SIH688X_REG_RTPCFG1, SIH_I2C_OPERA_BYTE_TWO, fifo_addr);
	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH688X_REG_RTPCFG3, SIH_RTPCFG3_BIT_FIFO_AFH_MASK, (fifo_addr[2]<<4));
	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH688X_REG_RTPCFG3, SIH_RTPCFG3_BIT_FIFO_AEH_MASK, fifo_addr[3]);

	/* ram */
	haptic_regmap_bulk_write(sih_haptic->regmapp.regmapping,
		SIH688X_REG_RAMADDRH, SIH_I2C_OPERA_BYTE_TWO, &sih_cont->data[shift]);

	i = sih_haptic->ram.ram_shift;

	if (sih_cont->len > SIH_RAMDATA_BUFFER_SIZE)
		sih_cont->len = SIH_RAMDATA_BUFFER_SIZE;

	while (i < sih_cont->len) {
		if ((sih_cont->len - i) <= SIH_RAMDATA_READ_SIZE)
			len = sih_cont->len - i;
		else
			len = SIH_RAMDATA_READ_SIZE;

		haptic_regmap_write(sih_haptic->regmapp.regmapping,
			SIH688X_REG_RAMDATA, len, &sih_cont->data[i]);

		i += len;
	}

	sih688x_ram_init(sih_haptic, false);
	sih688x_ram_init(sih_haptic, true);

	i = sih_haptic->ram.ram_shift;
	ram_data = vmalloc(SIH_RAMDATA_BUFFER_SIZE);
	if (!ram_data) {
		hp_err("%s:ram_data vmalloc failed\n", __func__);
	} else {
		while (i < sih_cont->len) {
			if ((sih_cont->len - i) <= SIH_RAMDATA_READ_SIZE)
				len = sih_cont->len - i;
			else
				len = SIH_RAMDATA_READ_SIZE;

			haptic_regmap_bulk_read(sih_haptic->regmapp.regmapping,
				SIH688X_REG_RAMDATA, len, ram_data);
			ret = sih688x_check_ram_data(sih_haptic, &sih_cont->data[i],
				ram_data, len);
			if (ret < 0)
				break;
			i += len;
		}
		if (ret)
			hp_err("%s:ram data check sum error\n", __func__);
		else
			hp_info("%s:ram data check sum pass\n", __func__);

		vfree(ram_data);
	}

	/* RAMINIT Disable */
	sih688x_ram_init(sih_haptic, false);
	mutex_unlock(&sih_haptic->lock);

	return ret;
}

static void sih688x_set_wav_seq(sih_haptic_t *sih_haptic,
	uint8_t seq, uint8_t wave)
{
	hp_info("%s:seq:%d,wave:%d\n", __func__, seq, wave);
	haptic_regmap_write(sih_haptic->regmapp.regmapping,
		SIH688X_REG_WAVESEQ1 + seq, SIH_I2C_OPERA_BYTE_ONE, &wave);
}

static void sih688x_get_wav_seq(sih_haptic_t *sih_haptic, uint32_t len)
{
	uint8_t i;
	uint8_t reg_val[SIH_HAPTIC_SEQUENCER_SIZE] = {0};

	if (len > SIH_HAPTIC_SEQUENCER_SIZE)
		len = SIH_HAPTIC_SEQUENCER_SIZE;

	haptic_regmap_bulk_read(sih_haptic->regmapp.regmapping,
		SIH688X_REG_WAVESEQ1, len, reg_val);
	for (i = 0; i < len; i++)
		sih_haptic->ram.seq[i] = reg_val[i];
}

static ssize_t sih688x_get_ram_data(sih_haptic_t *sih_haptic, char *buf)
{
	uint8_t *ram_data;
	int i = 0;
	int size = 0;
	ssize_t len = 0;

	ram_data = vmalloc(SIH_RAMDATA_BUFFER_SIZE);
	if (!ram_data)
		return len;

	if (sih_haptic->ram.len < SIH_RAMDATA_BUFFER_SIZE)
		size = sih_haptic->ram.len;
	else
		size = SIH_RAMDATA_BUFFER_SIZE;

	while (i < size) {
		if ((size - i) <= SIH_RAMDATA_READ_SIZE)
			len = size - i;
		else
			len = SIH_RAMDATA_READ_SIZE;

		haptic_regmap_bulk_read(sih_haptic->regmapp.regmapping,
			SIH688X_REG_RAMDATA, len, &ram_data[i]);

		i += len;
	}

	for (i = 1; i < size; i++)
		len += snprintf(buf + len, PAGE_SIZE - len, "0x%02x,", ram_data[i]);

	vfree(ram_data);

	return len;
}

static void sih688x_get_first_wave_addr(sih_haptic_t *sih_haptic,
	uint8_t *wave_addr)
{
	uint8_t reg_array[3] = {0, 0, 0};

	haptic_regmap_bulk_read(sih_haptic->regmapp.regmapping,
		SIH688X_REG_RAMDATA, SIH_I2C_OPERA_BYTE_THREE, reg_array);

	wave_addr[0] = reg_array[1];
	wave_addr[1] = reg_array[2];

	hp_info("%s:wave_addr[0] = 0x%02x wave_addr[1] = 0x%02x\n",
		__func__, wave_addr[0], wave_addr[1]);
}

static void sih688x_set_ram_addr(sih_haptic_t *sih_haptic)
{
	uint8_t ram_addr[2] = {0, 0};

	ram_addr[0] = (uint8_t)SIH_RAM_ADDR_H(sih_haptic->ram.base_addr);
	ram_addr[1] = (uint8_t)SIH_RAM_ADDR_L(sih_haptic->ram.base_addr);
	haptic_regmap_bulk_write(sih_haptic->regmapp.regmapping,
		SIH688X_REG_BASE_ADDRH, SIH_I2C_OPERA_BYTE_TWO, ram_addr);
}

static void sih688x_set_wav_loop(sih_haptic_t *sih_haptic,
	uint8_t seq, uint8_t loop)
{
	uint8_t offset;

	hp_info("%s:seq = 0x%02x, loop = 0x%02x\n", __func__, seq, loop);
	offset = ((seq + 1) % 2) * 4;

	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH688X_REG_WAVELOOP1 + (seq / 2), WAVELOOP_SEQ_EVEN_MASK << offset,
		loop << offset);
}

static void sih688x_get_wav_loop(sih_haptic_t *sih_haptic)
{
	uint8_t i;
	uint8_t reg_val[4] = {0};

	haptic_regmap_bulk_read(sih_haptic->regmapp.regmapping,
		SIH688X_REG_WAVELOOP1, SIH_I2C_OPERA_BYTE_FOUR, reg_val);

	for (i = 0; i < SIH_HAPTIC_SEQUENCER_SIZE / 2; i++) {
		sih_haptic->ram.loop[i * 2 + 0] =
			(reg_val[i] >> 4) & WAVELOOP_SEQ_EVEN_MASK;
		sih_haptic->ram.loop[i * 2 + 1] =
			(reg_val[i] >> 0) & WAVELOOP_SEQ_EVEN_MASK;
	}
}

static void sih688x_set_wav_main_loop(sih_haptic_t *sih_haptic,
	uint8_t loop)
{
	hp_info("%s:main loop = %d\n", __func__, loop);
	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH688X_REG_MAINLOOP, SIH_MAINLOOP_BIT_MAIN_LOOP_MASK, loop);
}

static void sih688x_get_wav_main_loop(sih_haptic_t *sih_haptic)
{
	uint8_t reg_val = 0;

	haptic_regmap_read(sih_haptic->regmapp.regmapping,
		SIH688X_REG_MAINLOOP, SIH_I2C_OPERA_BYTE_ONE, &reg_val);
	sih_haptic->ram.main_loop = reg_val;
}

static void sih688x_set_repeat_seq(sih_haptic_t *sih_haptic, uint8_t seq)
{
	uint8_t first_wave_index = 0;

	sih688x_set_wav_seq(sih_haptic, first_wave_index, seq);
	sih688x_set_wav_loop(sih_haptic, first_wave_index,
		WAVELOOP_SEQ_ODD_INFINNTE_TIME);
}

static void sih688x_set_ram_seq_gain(sih_haptic_t *sih_haptic,
	uint8_t wav, uint8_t gain)
{
	uint8_t offset = 0;

	offset = (wav % 2) * 4;

	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH688X_REG_GAIN_SET_SEQ1_0 + (wav / 2),
		WAVEGAIN_SEQ_EVEN_MASK << offset, gain << offset);
}

static size_t sih688x_get_ram_seq_gain(sih_haptic_t *sih_haptic, char *buf)
{
	uint8_t i;
	uint8_t reg_val[4] = {0};
	size_t count = 0;

	haptic_regmap_bulk_read(sih_haptic->regmapp.regmapping,
		SIH688X_REG_GAIN_SET_SEQ1_0, SIH_I2C_OPERA_BYTE_FOUR, reg_val);

	for (i = 0; i < SIH_HAPTIC_SEQUENCER_GAIN_SIZE; i++) {
		sih_haptic->ram.gain[i * 2 + 0] = (reg_val[i] >> 0) & 0x0F;
		sih_haptic->ram.gain[i * 2 + 1] = (reg_val[i] >> 4) & 0x0F;
		count += snprintf(buf + count, PAGE_SIZE - count,
			"seq%d gain: 0x%02x\n", i * 2 + 0,
			sih_haptic->ram.gain[i * 2 + 0]);
		count += snprintf(buf + count, PAGE_SIZE - count,
			"seq%d gain: 0x%02x\n", i * 2 + 1,
			sih_haptic->ram.gain[i * 2 + 1]);
	}
	return count;
}

/***********************************************
*
* chip rtp config
*
***********************************************/

static size_t sih688x_write_rtp_data(sih_haptic_t *sih_haptic,
	uint8_t *data, uint32_t len)
{
	int ret = -1;

	ret = haptic_regmap_write(sih_haptic->regmapp.regmapping,
		SIH688X_REG_RTPDATA, len, data);

	return ret;
}

static void sih688x_set_rtp_aei(sih_haptic_t *sih_haptic, bool flag)
{
	hp_info("%s:%d\n", __func__, flag);

	if (flag) {
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			SIH688X_REG_SYSINTM, SIH_SYSINTM_BIT_FF_AEI_MASK,
			SIH_SYSINTM_BIT_FF_AEI_EN);
	} else {
		haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
			SIH688X_REG_SYSINTM, SIH_SYSINTM_BIT_FF_AEI_MASK,
			SIH_SYSINTM_BIT_FF_AEI_OFF);
	}
}

static void sih688x_start_thres(sih_haptic_t *sih_haptic)
{
	hp_info("%s:rtp start thres:%d", __func__,
		sih_haptic->rtp.rtp_start_thres);

	haptic_regmap_write(sih_haptic->regmapp.regmapping,
		SIH688X_REG_RTP_START_THRES, SIH_I2C_OPERA_BYTE_ONE,
		&sih_haptic->rtp.rtp_start_thres);
}

/***********************************************
*
* chip cont config
*
***********************************************/
static void sih688x_set_cont_para(sih_haptic_t *sih_haptic, uint8_t flag,
	uint8_t *data)
{
	uint8_t reg_addr_start = SIH688X_CONT_PARA_SEQ0;

	switch (flag) {
	case SIH688X_CONT_PARA_SEQ0:
		reg_addr_start = SIH688X_REG_SEQ0_T_DRIVER;
		break;
	case SIH688X_CONT_PARA_SEQ1:
		reg_addr_start = SIH688X_REG_SEQ1_T_DRIVER;
		break;
	case SIH688X_CONT_PARA_SEQ2:
		reg_addr_start = SIH688X_REG_SEQ2_T_DRIVER;
		break;
	case SIH688X_CONT_PARA_ASMOOTH:
		reg_addr_start = SIH688X_REG_SMOOTH_CONST_ALGO_DATA_0;
		break;
	case SIH688X_CONT_PARA_TH_LEN:
		reg_addr_start = SIH688X_REG_T_0;
		break;
	case SIH688X_CONT_PARA_TH_NUM:
		reg_addr_start = SIH688X_REG_CYCLE0;
		break;
	case SIH688X_CONT_PARA_AMPLI:
		reg_addr_start = SIH688X_REG_VREF0;
		break;
	default:
		hp_err("%s:err flag:%d\n", __func__, flag);
		break;
	}
	hp_info("%s:data 0x%02x 0x%02x 0x%02x\n", __func__, data[0], data[1], data[2]);
	haptic_regmap_bulk_write(sih_haptic->regmapp.regmapping, reg_addr_start,
		SIH_I2C_OPERA_BYTE_THREE, data);

	hp_info("%s:reg_start:0x%02x\n", __func__, reg_addr_start);
}

static ssize_t sih688x_get_cont_para(sih_haptic_t *sih_haptic,
	uint8_t flag, char *buf)
{
	ssize_t len = 0;
	uint8_t reg_addr_start = SIH688X_CONT_PARA_SEQ0;
	uint8_t reg_val[3] = {0};

	switch (flag) {
	case SIH688X_CONT_PARA_SEQ0:
		reg_addr_start = SIH688X_REG_SEQ0_T_DRIVER;
		break;
	case SIH688X_CONT_PARA_SEQ1:
		reg_addr_start = SIH688X_REG_SEQ1_T_DRIVER;
		break;
	case SIH688X_CONT_PARA_SEQ2:
		reg_addr_start = SIH688X_REG_SEQ2_T_DRIVER;
		break;
	case SIH688X_CONT_PARA_ASMOOTH:
		reg_addr_start = SIH688X_REG_SMOOTH_CONST_ALGO_DATA_0;
		break;
	case SIH688X_CONT_PARA_TH_LEN:
		reg_addr_start = SIH688X_REG_T_0;
		break;
	case SIH688X_CONT_PARA_TH_NUM:
		reg_addr_start = SIH688X_REG_CYCLE0;
		break;
	case SIH688X_CONT_PARA_AMPLI:
		reg_addr_start = SIH688X_REG_VREF0;
		break;
	default:
		hp_err("%s:err flag:%d\n", __func__, flag);
		break;
	}
	haptic_regmap_bulk_read(sih_haptic->regmapp.regmapping, reg_addr_start,
		SIH_I2C_OPERA_BYTE_THREE, reg_val);

	len += snprintf(buf + len, PAGE_SIZE - len,
		"data1 = 0x%02x data2 = 0x%02x data3 = 0x%02x\n",
		reg_val[0], reg_val[1], reg_val[2]);

	return len;
}
/***********************************************
*
* chip trig config
*
***********************************************/
static void sih688x_trig_para_set(sih_haptic_t *sih_haptic, uint32_t *val)
{/* index enable polar trig_mode boost_bypass p_id n_id */
	uint8_t index = 0;
	uint8_t ctrl1 = 0;
	uint8_t ctrl2 = 0;
	uint8_t boost = 0;
	uint8_t pose_id = 0;
	uint8_t nege_id = 0;

	hp_info("%s:enter\n", __func__);
	/* trig index */
	if (val[0] < SIH_TRIG_NUM) {
		index = val[0];
	} else {
		hp_err("error index=%d\n", val[0]);
		return;
	}

	/* trig enable */
	sih_haptic->trig_para[index].enable = (bool)val[1];
	ctrl1 |= (uint8_t)((bool)val[1] << index);

	/* trig polar */
	sih_haptic->trig_para[index].polar = (bool)val[2];
	ctrl1 |= (uint8_t)((bool)val[2] << (index + 4));

	/* trig mode */
	sih_haptic->trig_para[index].mode = val[3];
	ctrl2 |= val[3] << (index * 2);

	/* trig boost */
	sih_haptic->trig_para[index].boost_bypass = (bool)val[4];
	boost |= (bool)val[4] << 7;

	pose_id = val[5];
	nege_id = val[6];

	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH688X_REG_TRIG_CTRL1, SIH_TRIG_CTRL1_BIT_TPOLAR0_MASK << index,
		ctrl1 & (SIH_TRIG_CTRL1_BIT_TPOLAR0_MASK << index));
	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH688X_REG_TRIG_CTRL1, SIH_TRIG_CTRL1_BIT_TRIG0_EN_MASK << index,
		ctrl1 & (SIH_TRIG_CTRL1_BIT_TRIG0_EN_MASK << index));
	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH688X_REG_TRIG_CTRL2, SIH_TRIG_CTRL2_BIT_TRIG0_MODE_MASK << index * 2,
		ctrl2 & (SIH_TRIG_CTRL2_BIT_TRIG0_MODE_MASK << index * 2));
	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH688X_REG_TRIG0_PACK_P + index * 2,
		SIH_TRIG_PACK_P_BIT_BOOST_BYPASS_MASK,
		boost & SIH_TRIG_PACK_P_BIT_BOOST_BYPASS_MASK);
	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH688X_REG_TRIG0_PACK_P + index * 2,
		SIH_TRIG_PACK_P_BIT_TRIG_PACK_MASK,
		pose_id & SIH_TRIG_PACK_P_BIT_TRIG_PACK_MASK);
	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH688X_REG_TRIG0_PACK_N + index * 2,
		SIH_TRIG_PACK_N_BIT_TRIG_PACK_MASK,
		nege_id & SIH_TRIG_PACK_N_BIT_TRIG_PACK_MASK);

	hp_info("%s:ctrl1:0x%02x, ctrl2:0x%02x\n", __func__, ctrl1, ctrl2);
}

static size_t sih688x_trig_para_get(sih_haptic_t *sih_haptic, char *buf)
{
	uint8_t trig_ctrl[2] = {0, 0};
	size_t len = 0;

	hp_info("%s:enter\n", __func__);

	haptic_regmap_bulk_read(sih_haptic->regmapp.regmapping,
		SIH688X_REG_TRIG_CTRL1, SIH_I2C_OPERA_BYTE_TWO, trig_ctrl);

	len += snprintf(buf + len, PAGE_SIZE - len,
		"trig ctrl1:0x%02x, trig ctrl2:0x%02x\n", trig_ctrl[0], trig_ctrl[1]);

	return len;
}

/***********************************************
*
* chip detect config
*
***********************************************/
static void sih688x_clear_detect_done_int(sih_haptic_t *sih_haptic)
{
	uint8_t reg_val = 0;

	hp_info("%s:enter\n", __func__);
	haptic_regmap_read(sih_haptic->regmapp.regmapping,
		SIH688X_REG_SYSINT2, SIH_I2C_OPERA_BYTE_ONE, &reg_val);
}

static void sih688x_set_detect_state(sih_haptic_t *sih_haptic,
	uint8_t mode, bool flag)
{
	uint8_t f0_en;
	uint8_t f0_mask;
	bool detect_int_en = false;

	switch (mode) {
	case SIH_RAM_MODE:
		sih_haptic->detect.ram_detect_en = flag;
		if (flag)
			f0_en = SIH_MODECTRL_BIT_RAM_F0_EN;
		else
			f0_en = SIH_MODECTRL_BIT_RAM_F0_OFF;
		f0_mask = SIH_MODECTRL_BIT_RAM_F0_MASK;
		break;
	case SIH_RTP_MODE:
		sih_haptic->detect.rtp_detect_en = flag;
		if (flag)
			f0_en = SIH_MODECTRL_BIT_RTP_F0_EN;
		else
			f0_en = SIH_MODECTRL_BIT_RTP_F0_OFF;
		f0_mask = SIH_MODECTRL_BIT_RTP_F0_MASK;
		break;
	case SIH_TRIG_MODE:
		sih_haptic->detect.trig_detect_en = flag;
		if (flag)
			f0_en = SIH_MODECTRL_BIT_TRIG_F0_EN;
		else
			f0_en = SIH_MODECTRL_BIT_TRIG_F0_OFF;
		f0_mask = SIH_MODECTRL_BIT_TRIG_F0_MASK;
		break;
	case SIH_CONT_MODE:
		sih_haptic->detect.cont_detect_en = flag;
		if (flag)
			f0_en = SIH_MODECTRL_BIT_TRACK_F0_EN;
		else
			f0_en = SIH_MODECTRL_BIT_TRACK_F0_OFF;
		f0_mask = SIH_MODECTRL_BIT_TRACK_F0_MASK;
		break;
	default:
		hp_err("%s:mode parameter invalid\n", __func__);
		return;
	}

	detect_int_en =
		sih_haptic->detect.cont_detect_en | sih_haptic->detect.trig_detect_en |
		sih_haptic->detect.rtp_detect_en | sih_haptic->detect.ram_detect_en;

	sih688x_detect_done_int(sih_haptic, detect_int_en);

	hp_info("%s:mask:0x%02x state:0x%02x\n", __func__, f0_mask, f0_en);
	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH688X_REG_MAIN_STATE_CTRL, f0_mask, f0_en);
	/* clear detect done int */
	sih688x_clear_detect_done_int(sih_haptic);
}


static void sih688x_check_detect_state(sih_haptic_t *sih_haptic, 
	uint8_t play_mode)
{
	uint8_t reg_val[2] = {0x8b, 0x36};
	bool detect_flag = false;

	hp_info("%s:enter\n", __func__);
	switch (play_mode) {
	case SIH_RAM_MODE:
	case SIH_RAM_LOOP_MODE:
		detect_flag = sih_haptic->detect.ram_detect_en;
		break;
	case SIH_RTP_MODE:
		detect_flag = sih_haptic->detect.rtp_detect_en;
		break;
	case SIH_CONT_MODE:
		detect_flag = sih_haptic->detect.cont_detect_en;
		break;
	default:
		hp_err("%s:play mode %d err\n", __func__, play_mode);
		break;
	}
	if (detect_flag) {
		haptic_regmap_bulk_write(sih_haptic->regmapp.regmapping,
			SIH688X_REG_T_LAST_MONITOR_L, SIH_I2C_OPERA_BYTE_TWO, reg_val);
		sih688x_detect_fifo_ctrl(sih_haptic, false);
		sih688x_ram_init(sih_haptic, false);
		sih688x_ram_init(sih_haptic, true);
		sih688x_ram_init(sih_haptic, false);
		sih688x_detect_fifo_ctrl(sih_haptic, true);
	}
}

static size_t sih688x_get_detect_state(sih_haptic_t *sih_haptic, char *buf)
{
	uint8_t reg_val = 0;
	size_t len = 0;

	hp_info("%s:enter\n", __func__);

	haptic_regmap_read(sih_haptic->regmapp.regmapping,
		SIH688X_REG_MAIN_STATE_CTRL, SIH_I2C_OPERA_BYTE_ONE, &reg_val);

	len += snprintf(buf + len, PAGE_SIZE - len, "detect_state=%d\n", reg_val);

	return len;
}

static void sih688x_read_detect_fifo(sih_haptic_t *sih_haptic)
{
	uint8_t i;
	uint32_t f0_raw_value;
	uint32_t f0_result = 0;
	uint8_t fifo_pack = SIH688X_FIFO_PACK_SIZE;
	uint8_t fifo_raw_data[SIH688X_DETECT_FIFO_ARRAY_MAX] = {0};
	uint8_t buf[SIH688X_FIFO_PACK_SIZE] = {0xff, 0xff, 0xff, 0xff, 0xff};

	/* F0 detect read */
	for (i = 0; i < SIH688X_READ_FIFO_MAX_DATA_LEN; ++i) {
		haptic_regmap_read(sih_haptic->regmapp.regmapping,
			SIH688X_REG_RAMDATA, fifo_pack,
			&fifo_raw_data[i * fifo_pack]);
		if (!memcmp(buf, &fifo_raw_data[i * fifo_pack], fifo_pack))
			break;
	}

	/*F0 detect calc*/
	for (i = 2; i < SIH688X_FIFO_READ_DATA_LEN; ++i) {
		f0_raw_value =
			(uint32_t)(fifo_raw_data[i * fifo_pack + 2] << 16 |
			fifo_raw_data[i * fifo_pack + 3] << 8 |
			fifo_raw_data[i * fifo_pack + 4]);
		f0_result += f0_raw_value;
		hp_info("f0_raw:%d\n", f0_raw_value);
	}
	hp_info("%s:f0 result:%d\n", __func__, f0_result);
	sih_haptic->detect.detect_f0 = SIH688X_DETECT_F0_COE /
		(f0_result / SIH688X_DETECT_F0_AMPLI_COE);

	hp_info("%s:detect f0:%d\n", __func__, sih_haptic->detect.detect_f0);
}

static void sih688x_read_tracking_f0(sih_haptic_t *sih_haptic)
{
	uint8_t data[3] = {0};
	uint32_t tracking_f0_value;

	haptic_regmap_read(sih_haptic->regmapp.regmapping,
		SIH688X_REG_T_HALF_TRACKING_0, SIH_I2C_OPERA_BYTE_THREE, data);
	tracking_f0_value = data[0] | data[1] << 8 | data[2] << 16;
	hp_info("%s:data 0x%02x 0x%02x 0x%02x\n", __func__,
		data[0], data[1], data[2]);
	sih_haptic->detect.tracking_f0 = SIH688X_TRACKING_F0_COE /
		tracking_f0_value;
}

static void sih688x_get_lra_resistance(sih_haptic_t *sih_haptic)
{
	uint8_t save_reg_addr[SIH688X_RL_CONFIG_REG_NUM] = {
		SIH688X_REG_ADC_EN_CNT, SIH688X_REG_ANA_CTRL1, SIH688X_REG_ANA_CTRL2};
	uint8_t save_reg_val[SIH688X_RL_CONFIG_REG_NUM] = {0};
	uint8_t time = SIH688X_RL_DETECT_MAX_TRY;
	uint8_t rl_high;
	uint8_t rl_low;
	uint64_t rl_rawdata = 0;
	uint8_t i;

	hp_info("%s:enter\n", __func__);
	/* read regs */
	for (i = 0; i < SIH688X_RL_CONFIG_REG_NUM; i++) {
		haptic_regmap_read(sih_haptic->regmapp.regmapping,
			save_reg_addr[i], SIH_I2C_OPERA_BYTE_ONE, &save_reg_val[i]);
	}
	/* load detect rl config */
	sih_load_reg_config(sih_haptic, REG_FUNC_RL);

	while (time--) {
		if (sih688x_if_chip_is_done(sih_haptic)) {
			usleep_range(5000, 5500);
			/* read raw data */
			haptic_regmap_read(sih_haptic->regmapp.regmapping,
				SIH688X_REG_ADC_RL_DATA_H, SIH_I2C_OPERA_BYTE_ONE, &rl_high);
			haptic_regmap_read(sih_haptic->regmapp.regmapping,
				SIH688X_REG_ADC_RL_DATA_L, SIH_I2C_OPERA_BYTE_ONE, &rl_low);
			rl_rawdata = (uint64_t)rl_high << 8 | rl_low;

			if (rl_rawdata > SIH688X_RL_SAR_CODE_DIVIDER) {
				/* calc rl */
				/*
				* rl calc formulation:
				*
				* rl = rl_raw_data / 10.5 / 1800 * 1000
				*
				* 1800 is current repair value
				*
				*/
				sih_haptic->detect.resistance = rl_rawdata *
					SIH688X_B0_RL_AMP_COE / SIH688X_B0_RL_DIV_COE;
			} else {
				/* calc rl */
				/*
				* rl calc formulation:
				*
				* rl = rl_raw_data / 2048 * 1.6 / 8 / 0.42 * 1000
				*
				* 1.6 is adc vref voltage
				* 8 is lpf amplify coefficient
				* 0.42 is rl detect current repair value
				*
				*/
				sih_haptic->detect.resistance = rl_rawdata *
					SIH688X_RL_AMP_COE / SIH688X_RL_DIV_COE -
					sih_haptic->detect.rl_offset;
			}
			hp_info("%s:0x5a:0x%02x 0x5b:0x%02x\n", __func__, rl_high, rl_low);
			break;
		}

		hp_info("%s:wait for done int\n", __func__);
		usleep_range(2000, 2500);
	}
	/* recovery regs */
	for (i = 0; i < SIH688X_RL_CONFIG_REG_NUM; i++) {
		haptic_regmap_write(sih_haptic->regmapp.regmapping,
			save_reg_addr[i], SIH_I2C_OPERA_BYTE_ONE, &save_reg_val[i]);
	}
    haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
        SIH688X_REG_RL_VBAT_CTRL,
        SIH_RL_VBAT_CTRL_BIT_DET_MODE_MASK,
        SIH_RL_VBAT_CTRL_BIT_DET_MODE_OFF);

	hp_info("%s:detect_rl:%d\n", __func__,(uint32_t)sih_haptic->detect.resistance);
}

static void sih688x_get_vbat(sih_haptic_t *sih_haptic)
{
	uint8_t time = SIH688X_GET_VBAT_MAX_TRY;
	uint8_t vbat_high;
	uint8_t vbat_low;
	uint32_t vbat_raw_data;

	hp_info("%s:enter\n", __func__);
	/* load detect vbat config */
	sih_load_reg_config(sih_haptic, REG_FUNC_VBAT);

	while (time--) {
		if (sih688x_if_chip_is_done(sih_haptic)) {
			usleep_range(5000, 5500);
			/* read raw data */
			haptic_regmap_read(sih_haptic->regmapp.regmapping,
				SIH688X_REG_ADC_OC_DATA_H, SIH_I2C_OPERA_BYTE_ONE, &vbat_high);
			haptic_regmap_read(sih_haptic->regmapp.regmapping,
				SIH688X_REG_ADC_VBAT_DATA_L, SIH_I2C_OPERA_BYTE_ONE, &vbat_low);
			vbat_raw_data = (uint32_t)((vbat_high & 0xf0) << 4 | vbat_low);
			/* calc vbat */
			/*
			* vbat calc formulation:
			*
			* VBAT = raw_data / 2048 * 1.6 * 4
			*
			* 1.6 is adc vref voltage
			* 4 is lpf amplify coefficient
			*
			*/
			sih_haptic->detect.vbat = vbat_raw_data *
				SIH688X_ADC_AMPLIFY_COE / SIH688X_ADC_COE;
			hp_info("%s:0x58:0x%02x 0x59:0x%02x\n", __func__, 
				vbat_high, vbat_low);
			break;
		}

		hp_info("%s:wait for detect done int\n", __func__);
		usleep_range(2000, 2500);
	}

	hp_info("detect_vbat:%d\n", sih_haptic->detect.vbat);
}

static void sih688x_get_detect_f0(sih_haptic_t *sih_haptic)
{
	uint8_t cnt = SIH_WAIT_FOR_STANDBY_MAX_TRY;
	uint8_t cont_detect_flag = false;

	hp_info("%s:enter\n", __func__);
	/* enter standby mode */
	sih688x_stop(sih_haptic);
	sih_haptic->detect.detect_f0 = SIH_F0_PRE_VALUE;
	sih_haptic->detect.detect_f0_read_done = false;
	if (!sih_haptic->detect.cont_detect_en) {
		sih_haptic->detect.cont_detect_en = true;
		cont_detect_flag = false;
	} else {
		cont_detect_flag = true;
	}
	/* enable detect done int */
	sih688x_clear_detect_done_int(sih_haptic);
	sih688x_detect_done_int(sih_haptic, true);
	/* load detect f0 config */
	sih_load_reg_config(sih_haptic, REG_FUNC_CONT);
	sih688x_upload_f0(sih_haptic, SIH_WRITE_ZERO);
	/* detect config */
	sih688x_detect_fifo_ctrl(sih_haptic, false);
	sih688x_ram_init(sih_haptic, false);
	sih688x_ram_init(sih_haptic, true);
	sih688x_ram_init(sih_haptic, false);
	sih688x_detect_fifo_ctrl(sih_haptic, true);
	sih688x_f0_tracking(sih_haptic, true);
	/* play go */
	sih688x_set_play_mode(sih_haptic, SIH_CONT_MODE);
	sih688x_play_go(sih_haptic, true);
	/* wait for read done */
	while (cnt--) {
		if (sih_haptic->detect.detect_f0_read_done)
			break;
		usleep_range(2000, 2500);
	}
	sih_haptic->detect.cont_detect_en = cont_detect_flag;
	/* recovery done int state */
	if (sih_haptic->detect.rtp_detect_en | sih_haptic->detect.ram_detect_en |
		sih_haptic->detect.cont_detect_en | sih_haptic->detect.trig_detect_en) {
		sih688x_detect_done_int(sih_haptic, true);
	} else {
		sih688x_detect_done_int(sih_haptic, false);
	}
	hp_info("f0:%d\n", sih_haptic->detect.detect_f0);
}

static void sih688x_get_tracking_f0(sih_haptic_t *sih_haptic)
{
	uint8_t cnt = SIH_WAIT_FOR_STANDBY_MAX_TRY;

	hp_info("%s:enter\n", __func__);
	sih_haptic->f0_cali_status = false;
	sih688x_stop(sih_haptic);
	sih_haptic->detect.tracking_f0 = SIH_F0_PRE_VALUE;
	/* load tracking f0 config */
	sih_load_reg_config(sih_haptic, REG_FUNC_CONT);
	sih688x_upload_f0(sih_haptic, SIH_WRITE_ZERO);
	sih688x_f0_tracking(sih_haptic, false);
	/* play go */
	sih688x_set_play_mode(sih_haptic, SIH_CONT_MODE);
	sih688x_play_go(sih_haptic, true);
	/* wait for standby */
	while (cnt--) {
		if (sih688x_if_chip_is_mode(sih_haptic, SIH_IDLE_MODE))
			break;
		usleep_range(2000, 2500);
	}
	/* read f0 data */
	sih688x_read_tracking_f0(sih_haptic);
	sih_haptic->f0_cali_status = true;
	hp_info("tracking_f0:%d\n", sih_haptic->detect.tracking_f0);
}

static void sih688x_vbat_comp(sih_haptic_t *sih_haptic)
{
	uint32_t comp_gain = 0x80;
	uint32_t curr_vbat = 0;

	hp_info("%s\n", __func__);

	sih688x_get_vbat(sih_haptic);
	curr_vbat = sih_haptic->detect.vbat;

	if (curr_vbat < SIH688X_VBAT_MIN) {
		curr_vbat = SIH688X_VBAT_MIN;
		hp_info("%s:vbat is lower than min, set to min\n", __func__);
	} else if (curr_vbat > SIH688X_VBAT_MAX) {
		curr_vbat = SIH688X_VBAT_MAX;
		hp_info("%s:vbat is higher than max, set to max\n", __func__);
	}
	/*
	* vbat compensation formulation:
	*
	* comp_gain * curreng_vbat = current_gain * standard_vbat
	*
	*/
	comp_gain = SIH688X_STANDARD_VBAT * sih_haptic->chip_ipara.gain / curr_vbat;
	if (comp_gain > SIH_HAPTIC_MAX_GAIN) {
		comp_gain = SIH_HAPTIC_MAX_GAIN;
		hp_info("%s:comp_gain is higher than max gain, set to max\n", __func__);
	}
	sih688x_set_gain(sih_haptic, comp_gain);
}

/***********************************************
*
* chip cali config
*
***********************************************/
static void sih688x_osc_cali(sih_haptic_t *sih_haptic)
{
	uint8_t reg_val = 0;
	int32_t code = 0;
	int32_t tmp = 0;

	hp_info("%s:enter\n", __func__);

	haptic_regmap_read(sih_haptic->regmapp.regmapping,
		SIH688X_REG_PWM_UP_SAMPLE_CTRL, SIH_I2C_OPERA_BYTE_ONE, &reg_val);

	switch (reg_val & SIH_PWM_UP_SAMPLE_CTRL_BIT_PWM_UP_RPT_MASK) {
	case SIH_PWM_UP_SAMPLE_CTRL_BIT_PWM_UP_ONE_TIME:
		sih_haptic->osc_para.theory_time =
			sih_haptic->osc_para.osc_rtp_len * SIH688X_OSC_RTL_DATA_LEN /
				SIh688X_PWM_SAMPLE_48KHZ;
		break;
	case SIH_PWM_UP_SAMPLE_CTRL_BIT_PWM_UP_TWO_TIME:
		sih_haptic->osc_para.theory_time =
			sih_haptic->osc_para.osc_rtp_len * SIH688X_OSC_RTL_DATA_LEN /
				SIh688X_PWM_SAMPLE_24KHZ;
		break;
	case SIH_PWM_UP_SAMPLE_CTRL_BIT_PWM_UP_FOUR_TIME:
		sih_haptic->osc_para.theory_time =
			sih_haptic->osc_para.osc_rtp_len * SIH688X_OSC_RTL_DATA_LEN /
				SIh688X_PWM_SAMPLE_12KHZ;
		break;
	default:
		break;
	}

	hp_info("%s:actual_time:%d theory_time:%d\n", __func__,
		sih_haptic->osc_para.actual_time, sih_haptic->osc_para.theory_time);

	tmp = (int32_t)((int64_t)sih_haptic->osc_para.actual_time *
		SIH688X_OSC_CALI_COE / sih_haptic->osc_para.theory_time);

	code = (tmp - SIH688X_OSC_CALI_COE) * SIH688X_OSC_CALI_COE /
		SIH688X_F0_DELTA;

	sih_haptic->osc_para.osc_data = (uint8_t)code;

	haptic_regmap_write(sih_haptic->regmapp.regmapping,
		SIH688X_REG_TRIM1, SIH_I2C_OPERA_BYTE_ONE,
		&sih_haptic->osc_para.osc_data);
}

static void sih688x_init(sih_haptic_t *sih_haptic)
{
	uint8_t reg_val = 0;

	/* idle cnt reg init */
	haptic_regmap_write(sih_haptic->regmapp.regmapping,
		SIH688X_REG_IDLE_DEL_CNT, SIH_I2C_OPERA_BYTE_ONE, &reg_val);

	/* gain init */
	haptic_regmap_read(sih_haptic->regmapp.regmapping,
		SIH688X_REG_PWM_PRE_GAIN, SIH_I2C_OPERA_BYTE_ONE, &reg_val);
	sih_haptic->chip_ipara.gain = reg_val;

	/* wait detect fifo off */
	haptic_regmap_update_bits(sih_haptic->regmapp.regmapping,
		SIH688X_REG_SMOOTH_F0_WINDOW_OUT, SIH_WAIT_FIFO_DETECT_MASK,
		SIH_WAIT_FIFO_DETECT_OFF);

	/* brk vboost init */
	haptic_regmap_read(sih_haptic->regmapp.regmapping,
		SIH688X_REG_V_BOOST, SIH_I2C_OPERA_BYTE_ONE, &reg_val);
	/**
	* brk vboost calc formulation:
	* 1bit = 0.0625v
	* brk vboost has increased by ten times
	*/
	sih_haptic->chip_ipara.brk_vboost = (((uint32_t)reg_val) << 8) *
		SIH688X_BRK_VBOOST_COE / SIH688X_VBOOST_MUL_COE;

	/* ram wave init */
	sih_haptic->ram.lib_index = SIH_INIT_ZERO_VALUE;

	/* f0 pre init */
	sih_haptic->detect.detect_f0 = SIH_F0_PRE_VALUE;
	sih_haptic->detect.tracking_f0 = SIH_F0_PRE_VALUE;

	/* osc data init */
	sih_haptic->osc_para.osc_data = SIH_INIT_ZERO_VALUE;
	/* rl pre init */
	sih_haptic->detect.resistance = SIH_INIT_ZERO_VALUE;
	sih_haptic->detect.rl_offset = SIH688X_RL_OFFSET;

	/* vbat init */
	sih_haptic->detect.vbat = SIH_INIT_ZERO_VALUE;

	/* rtp start thres init */
	sih_haptic->rtp.rtp_start_thres = SIH_RTP_START_DEFAULT_THRES;

	/* state init */
	sih_haptic->chip_ipara.state = SIH_STANDBY_MODE;
	sih_haptic->chip_ipara.play_mode = SIH_IDLE_MODE;

	/* drv vboost init */
	sih688x_set_drv_bst_vol(sih_haptic, sih_haptic->chip_ipara.drv_vboost);

	/* low power mode init */
	sih688x_set_low_power_mode(sih_haptic, true);

	/* update chip para */
	sih688x_get_vbat(sih_haptic);
	sih688x_get_lra_resistance(sih_haptic);
}

haptic_func_t sih_688x_func_list = {
	.probe = sih688x_probe,
	.init = sih688x_init,
	.ram_init = sih688x_ram_init,
	.detect_fifo_ctrl = sih688x_detect_fifo_ctrl,
	.interrupt_state_init = sih688x_interrupt_state_init,
	.chip_software_reset = sih688x_software_reset,
	.chip_hardware_reset = sih688x_hardware_reset,
	.update_chip_state = sih688x_update_chip_state,
	.update_ram_config = sih688x_update_ram_config,
	.clear_interrupt_state = sih688x_clear_interrupt_state,
	.play_go = sih688x_play_go,
	.stop = sih688x_stop,
	.get_vbat = sih688x_get_vbat,
	.get_detect_f0 = sih688x_get_detect_f0,
	.get_tracking_f0 = sih688x_get_tracking_f0,
	.get_lra_resistance = sih688x_get_lra_resistance,
	.set_play_mode = sih688x_set_play_mode,
	.set_drv_bst_vol = sih688x_set_drv_bst_vol,
	.set_brk_bst_vol = sih688x_set_brk_bst_vol,
	.set_repeat_seq = sih688x_set_repeat_seq,
	.get_wav_seq = sih688x_get_wav_seq,
	.set_wav_seq = sih688x_set_wav_seq,
	.get_wav_loop = sih688x_get_wav_loop,
	.set_wav_loop = sih688x_set_wav_loop,
	.set_wav_main_loop = sih688x_set_wav_main_loop,
	.get_wav_main_loop = sih688x_get_wav_main_loop,
	.get_first_wave_addr = sih688x_get_first_wave_addr,
	.get_ram_data = sih688x_get_ram_data,
	.set_ram_addr = sih688x_set_ram_addr,
	.set_rtp_aei = sih688x_set_rtp_aei,
	.set_start_thres = sih688x_start_thres,
	.write_rtp_data = sih688x_write_rtp_data,
	.if_chip_is_mode = sih688x_if_chip_is_mode,
	.if_chip_is_detect_done = sih688x_if_chip_is_detect_done,
	.get_rtp_fifo_empty_state = sih688x_get_rtp_fifo_empty_state,
	.get_rtp_fifo_full_state = sih688x_get_rtp_fifo_full_state,
	.set_gain = sih688x_set_gain,
	.vbat_comp = sih688x_vbat_comp,
	.upload_f0 = sih688x_upload_f0,
	.set_boost_mode = sih688x_set_boost_mode,
	.set_auto_pvdd = sih688x_set_auto_pvdd,
	.set_low_power_mode = sih688x_set_low_power_mode,
	.get_trig_para = sih688x_trig_para_get,
	.set_trig_para = sih688x_trig_para_set,
	.get_ram_seq_gain = sih688x_get_ram_seq_gain,
	.set_ram_seq_gain = sih688x_set_ram_seq_gain,
	.get_brk_state = sih688x_get_brk_state,
	.set_brk_state = sih688x_set_brk_state,
	.get_detect_state = sih688x_get_detect_state,
	.set_detect_state = sih688x_set_detect_state,
	.check_detect_state = sih688x_check_detect_state,
	.get_pwm_rate = sih688x_get_pwm_rate,
	.set_pwm_rate = sih688x_set_pwm_rate,
	.osc_cali = sih688x_osc_cali,
	.efuse_check = sih688x_efuse_check,
	.read_detect_fifo = sih688x_read_detect_fifo,
	.get_cont_para = sih688x_get_cont_para,
	.set_cont_para = sih688x_set_cont_para,
};
