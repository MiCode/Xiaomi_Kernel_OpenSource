/*
 * Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/printk.h>
#include <linux/ratelimit.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/regmap.h>
#include <linux/mfd/wcd9xxx/core.h>
#include <linux/mfd/wcd9xxx/wcd9xxx-irq.h>
#include <linux/mfd/wcd9xxx/wcd9xxx_registers.h>
#include <linux/mfd/wcd934x/registers.h>
#include <linux/mfd/wcd9xxx/pdata.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include "wcd934x.h"
#include "wcd934x-mbhc.h"
#include "../wcdcal-hwdep.h"

#define TAVIL_ZDET_SUPPORTED          true
/* Z value defined in milliohm */
#define TAVIL_ZDET_VAL_32             32000
#define TAVIL_ZDET_VAL_400            400000
#define TAVIL_ZDET_VAL_1200           1200000
#define TAVIL_ZDET_VAL_100K           100000000
/* Z floating defined in ohms */
#define TAVIL_ZDET_FLOATING_IMPEDANCE 0x0FFFFFFE

#define TAVIL_ZDET_NUM_MEASUREMENTS   150
#define TAVIL_MBHC_GET_C1(c)          ((c & 0xC000) >> 14)
#define TAVIL_MBHC_GET_X1(x)          (x & 0x3FFF)
/* Z value compared in milliOhm */
#define TAVIL_MBHC_IS_SECOND_RAMP_REQUIRED(z) ((z > 400000) || (z < 32000))
#define TAVIL_MBHC_ZDET_CONST         (86 * 16384)
#define TAVIL_MBHC_MOISTURE_RREF      R_24_KOHM

static struct wcd_mbhc_register
	wcd_mbhc_registers[WCD_MBHC_REG_FUNC_MAX] = {
	WCD_MBHC_REGISTER("WCD_MBHC_L_DET_EN",
			  WCD934X_ANA_MBHC_MECH, 0x80, 7, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_GND_DET_EN",
			  WCD934X_ANA_MBHC_MECH, 0x40, 6, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_MECH_DETECTION_TYPE",
			  WCD934X_ANA_MBHC_MECH, 0x20, 5, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_MIC_CLAMP_CTL",
			  WCD934X_MBHC_NEW_PLUG_DETECT_CTL, 0x30, 4, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_ELECT_DETECTION_TYPE",
			  WCD934X_ANA_MBHC_ELECT, 0x08, 3, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HS_L_DET_PULL_UP_CTRL",
			  WCD934X_MBHC_NEW_PLUG_DETECT_CTL, 0xC0, 6, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HS_L_DET_PULL_UP_COMP_CTRL",
			  WCD934X_ANA_MBHC_MECH, 0x04, 2, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHL_PLUG_TYPE",
			  WCD934X_ANA_MBHC_MECH, 0x10, 4, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_GND_PLUG_TYPE",
			  WCD934X_ANA_MBHC_MECH, 0x08, 3, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_SW_HPH_LP_100K_TO_GND",
			  WCD934X_ANA_MBHC_MECH, 0x01, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_ELECT_SCHMT_ISRC",
			  WCD934X_ANA_MBHC_ELECT, 0x06, 1, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_FSM_EN",
			  WCD934X_ANA_MBHC_ELECT, 0x80, 7, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_INSREM_DBNC",
			  WCD934X_MBHC_NEW_PLUG_DETECT_CTL, 0x0F, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_BTN_DBNC",
			  WCD934X_MBHC_NEW_CTL_1, 0x03, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HS_VREF",
			  WCD934X_MBHC_NEW_CTL_2, 0x03, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HS_COMP_RESULT",
			  WCD934X_ANA_MBHC_RESULT_3, 0x08, 3, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_MIC_SCHMT_RESULT",
			  WCD934X_ANA_MBHC_RESULT_3, 0x20, 5, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHL_SCHMT_RESULT",
			  WCD934X_ANA_MBHC_RESULT_3, 0x80, 7, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHR_SCHMT_RESULT",
			  WCD934X_ANA_MBHC_RESULT_3, 0x40, 6, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_OCP_FSM_EN",
			  WCD934X_HPH_OCP_CTL, 0x10, 4, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_BTN_RESULT",
			  WCD934X_ANA_MBHC_RESULT_3, 0x07, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_BTN_ISRC_CTL",
			  WCD934X_ANA_MBHC_ELECT, 0x70, 4, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_ELECT_RESULT",
			  WCD934X_ANA_MBHC_RESULT_3, 0xFF, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_MICB_CTRL",
			  WCD934X_ANA_MICB2, 0xC0, 6, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPH_CNP_WG_TIME",
			  WCD934X_HPH_CNP_WG_TIME, 0xFF, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHR_PA_EN",
			  WCD934X_ANA_HPH, 0x40, 6, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHL_PA_EN",
			  WCD934X_ANA_HPH, 0x80, 7, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPH_PA_EN",
			  WCD934X_ANA_HPH, 0xC0, 6, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_SWCH_LEVEL_REMOVE",
			  WCD934X_ANA_MBHC_RESULT_3, 0x10, 4, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_PULLDOWN_CTRL",
			  0, 0, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_ANC_DET_EN",
			  WCD934X_ANA_MBHC_ZDET, 0x01, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_FSM_STATUS",
			  WCD934X_MBHC_STATUS_SPARE_1, 0x01, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_MUX_CTL",
			  WCD934X_MBHC_NEW_CTL_2, 0x70, 4, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHL_OCP_DET_EN",
			  WCD934X_HPH_L_TEST, 0x01, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHR_OCP_DET_EN",
			  WCD934X_HPH_R_TEST, 0x01, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHL_OCP_STATUS",
			  WCD934X_INTR_PIN1_STATUS0, 0x04, 2, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHR_OCP_STATUS",
			  WCD934X_INTR_PIN1_STATUS0, 0x08, 3, 0),
};

static const struct wcd_mbhc_intr intr_ids = {
	.mbhc_sw_intr =  WCD934X_IRQ_MBHC_SW_DET,
	.mbhc_btn_press_intr = WCD934X_IRQ_MBHC_BUTTON_PRESS_DET,
	.mbhc_btn_release_intr = WCD934X_IRQ_MBHC_BUTTON_RELEASE_DET,
	.mbhc_hs_ins_intr = WCD934X_IRQ_MBHC_ELECT_INS_REM_LEG_DET,
	.mbhc_hs_rem_intr = WCD934X_IRQ_MBHC_ELECT_INS_REM_DET,
	.hph_left_ocp = WCD934X_IRQ_HPH_PA_OCPL_FAULT,
	.hph_right_ocp = WCD934X_IRQ_HPH_PA_OCPR_FAULT,
};


static char on_demand_supply_name[][MAX_ON_DEMAND_SUPPLY_NAME_LENGTH] = {
	"cdc-vdd-mic-bias",
};

struct tavil_mbhc_zdet_param {
	u16 ldo_ctl;
	u16 noff;
	u16 nshift;
	u16 btn5;
	u16 btn6;
	u16 btn7;
};

static int tavil_mbhc_request_irq(struct snd_soc_codec *codec,
				  int irq, irq_handler_t handler,
				  const char *name, void *data)
{
	struct wcd9xxx *wcd9xxx = dev_get_drvdata(codec->dev->parent);
	struct wcd9xxx_core_resource *core_res =
				&wcd9xxx->core_res;

	return wcd9xxx_request_irq(core_res, irq, handler, name, data);
}

static void tavil_mbhc_irq_control(struct snd_soc_codec *codec,
				   int irq, bool enable)
{
	struct wcd9xxx *wcd9xxx = dev_get_drvdata(codec->dev->parent);
	struct wcd9xxx_core_resource *core_res =
				&wcd9xxx->core_res;
	if (enable)
		wcd9xxx_enable_irq(core_res, irq);
	else
		wcd9xxx_disable_irq(core_res, irq);
}

static int tavil_mbhc_free_irq(struct snd_soc_codec *codec,
			       int irq, void *data)
{
	struct wcd9xxx *wcd9xxx = dev_get_drvdata(codec->dev->parent);
	struct wcd9xxx_core_resource *core_res =
				&wcd9xxx->core_res;

	wcd9xxx_free_irq(core_res, irq, data);
	return 0;
}

static void tavil_mbhc_clk_setup(struct snd_soc_codec *codec,
				 bool enable)
{
	if (enable)
		snd_soc_update_bits(codec, WCD934X_MBHC_NEW_CTL_1,
				    0x80, 0x80);
	else
		snd_soc_update_bits(codec, WCD934X_MBHC_NEW_CTL_1,
				    0x80, 0x00);
}

static int tavil_mbhc_btn_to_num(struct snd_soc_codec *codec)
{
	return snd_soc_read(codec, WCD934X_ANA_MBHC_RESULT_3) & 0x7;
}

static int tavil_enable_ext_mb_source(struct wcd_mbhc *mbhc,
				      bool turn_on)
{
	struct wcd934x_mbhc *wcd934x_mbhc;
	struct snd_soc_codec *codec = mbhc->codec;
	struct wcd934x_on_demand_supply *supply;
	int ret = 0;

	wcd934x_mbhc = container_of(mbhc, struct wcd934x_mbhc, wcd_mbhc);

	supply =  &wcd934x_mbhc->on_demand_list[WCD934X_ON_DEMAND_MICBIAS];
	if (!supply->supply) {
		dev_dbg(codec->dev, "%s: warning supply not present ond for %s\n",
				__func__, "onDemand Micbias");
		return ret;
	}

	dev_dbg(codec->dev, "%s turn_on: %d count: %d\n", __func__, turn_on,
		supply->ondemand_supply_count);

	if (turn_on) {
		if (!(supply->ondemand_supply_count)) {
			ret = snd_soc_dapm_force_enable_pin(
				snd_soc_codec_get_dapm(codec),
				"MICBIAS_REGULATOR");
			snd_soc_dapm_sync(snd_soc_codec_get_dapm(codec));
		}
		supply->ondemand_supply_count++;
	} else {
		if (supply->ondemand_supply_count > 0)
			supply->ondemand_supply_count--;
		if (!(supply->ondemand_supply_count)) {
			ret = snd_soc_dapm_disable_pin(
				snd_soc_codec_get_dapm(codec),
				"MICBIAS_REGULATOR");
		snd_soc_dapm_sync(snd_soc_codec_get_dapm(codec));
		}
	}

	if (ret)
		dev_err(codec->dev, "%s: Failed to %s external micbias source\n",
			__func__, turn_on ? "enable" : "disabled");
	else
		dev_dbg(codec->dev, "%s: %s external micbias source\n",
			__func__, turn_on ? "Enabled" : "Disabled");

	return ret;
}

static void tavil_mbhc_mbhc_bias_control(struct snd_soc_codec *codec,
					 bool enable)
{
	if (enable)
		snd_soc_update_bits(codec, WCD934X_ANA_MBHC_ELECT,
				    0x01, 0x01);
	else
		snd_soc_update_bits(codec, WCD934X_ANA_MBHC_ELECT,
				    0x01, 0x00);
}

static void tavil_mbhc_program_btn_thr(struct snd_soc_codec *codec,
				       s16 *btn_low, s16 *btn_high,
				       int num_btn, bool is_micbias)
{
	int i;
	int vth;

	if (num_btn > WCD_MBHC_DEF_BUTTONS) {
		dev_err(codec->dev, "%s: invalid number of buttons: %d\n",
			__func__, num_btn);
		return;
	}
	/*
	 * Tavil just needs one set of thresholds for button detection
	 * due to micbias voltage ramp to pullup upon button press. So
	 * btn_low and is_micbias are ignored and always program button
	 * thresholds using btn_high.
	 */
	for (i = 0; i < num_btn; i++) {
		vth = ((btn_high[i] * 2) / 25) & 0x3F;
		snd_soc_update_bits(codec, WCD934X_ANA_MBHC_BTN0 + i,
				    0xFC, vth << 2);
		dev_dbg(codec->dev, "%s: btn_high[%d]: %d, vth: %d\n",
			__func__, i, btn_high[i], vth);
	}
}

static bool tavil_mbhc_lock_sleep(struct wcd_mbhc *mbhc, bool lock)
{
	struct snd_soc_codec *codec = mbhc->codec;
	struct wcd9xxx *wcd9xxx = dev_get_drvdata(codec->dev->parent);
	struct wcd9xxx_core_resource *core_res =
				&wcd9xxx->core_res;
	bool ret = 0;

	if (lock)
		ret = wcd9xxx_lock_sleep(core_res);
	else
		wcd9xxx_unlock_sleep(core_res);

	return ret;
}

static int tavil_mbhc_register_notifier(struct wcd_mbhc *mbhc,
					struct notifier_block *nblock,
					bool enable)
{
	struct wcd934x_mbhc *wcd934x_mbhc;

	wcd934x_mbhc = container_of(mbhc, struct wcd934x_mbhc, wcd_mbhc);

	if (enable)
		return blocking_notifier_chain_register(&wcd934x_mbhc->notifier,
							nblock);
	else
		return blocking_notifier_chain_unregister(
				&wcd934x_mbhc->notifier, nblock);
}

static bool tavil_mbhc_micb_en_status(struct wcd_mbhc *mbhc, int micb_num)
{
	u8 val;

	if (micb_num == MIC_BIAS_2) {
		val = (snd_soc_read(mbhc->codec, WCD934X_ANA_MICB2) >> 6);
		if (val == 0x01)
			return true;
	}
	return false;
}

static bool tavil_mbhc_hph_pa_on_status(struct snd_soc_codec *codec)
{
	return (snd_soc_read(codec, WCD934X_ANA_HPH) & 0xC0) ? true : false;
}

static void tavil_mbhc_hph_l_pull_up_control(
		struct snd_soc_codec *codec,
		enum mbhc_hs_pullup_iref pull_up_cur)
{
	/* Default pull up current to 2uA */
	if (pull_up_cur < I_OFF || pull_up_cur > I_3P0_UA ||
	    pull_up_cur == I_DEFAULT)
		pull_up_cur = I_2P0_UA;

	dev_dbg(codec->dev, "%s: HS pull up current:%d\n",
		__func__, pull_up_cur);

	snd_soc_update_bits(codec, WCD934X_MBHC_NEW_PLUG_DETECT_CTL,
			    0xC0, pull_up_cur << 6);
}

static int tavil_mbhc_request_micbias(struct snd_soc_codec *codec,
				      int micb_num, int req)
{
	int ret;

	/*
	 * If micbias is requested, make sure that there
	 * is vote to enable mclk
	 */
	if (req == MICB_ENABLE)
		tavil_cdc_mclk_enable(codec, true);

	ret = tavil_micbias_control(codec, micb_num, req, false);

	/*
	 * Release vote for mclk while requesting for
	 * micbias disable
	 */
	if (req == MICB_DISABLE)
		tavil_cdc_mclk_enable(codec, false);

	return ret;
}

static void tavil_mbhc_micb_ramp_control(struct snd_soc_codec *codec,
					 bool enable)
{
	if (enable) {
		snd_soc_update_bits(codec, WCD934X_ANA_MICB2_RAMP,
				    0x1C, 0x0C);
		snd_soc_update_bits(codec, WCD934X_ANA_MICB2_RAMP,
				    0x80, 0x80);
	} else {
		snd_soc_update_bits(codec, WCD934X_ANA_MICB2_RAMP,
				    0x80, 0x00);
		snd_soc_update_bits(codec, WCD934X_ANA_MICB2_RAMP,
				    0x1C, 0x00);
	}
}

static struct firmware_cal *tavil_get_hwdep_fw_cal(struct wcd_mbhc *mbhc,
						   enum wcd_cal_type type)
{
	struct wcd934x_mbhc *wcd934x_mbhc;
	struct firmware_cal *hwdep_cal;
	struct snd_soc_codec *codec = mbhc->codec;

	wcd934x_mbhc = container_of(mbhc, struct wcd934x_mbhc, wcd_mbhc);

	if (!codec) {
		pr_err("%s: NULL codec pointer\n", __func__);
		return NULL;
	}
	hwdep_cal = wcdcal_get_fw_cal(wcd934x_mbhc->fw_data, type);
	if (!hwdep_cal)
		dev_err(codec->dev, "%s: cal not sent by %d\n",
			__func__, type);

	return hwdep_cal;
}

static int tavil_mbhc_micb_ctrl_threshold_mic(struct snd_soc_codec *codec,
					      int micb_num, bool req_en)
{
	struct wcd9xxx_pdata *pdata = dev_get_platdata(codec->dev->parent);
	int rc, micb_mv;

	if (micb_num != MIC_BIAS_2)
		return -EINVAL;

	/*
	 * If device tree micbias level is already above the minimum
	 * voltage needed to detect threshold microphone, then do
	 * not change the micbias, just return.
	 */
	if (pdata->micbias.micb2_mv >= WCD_MBHC_THR_HS_MICB_MV)
		return 0;

	micb_mv = req_en ? WCD_MBHC_THR_HS_MICB_MV : pdata->micbias.micb2_mv;

	rc = tavil_mbhc_micb_adjust_voltage(codec, micb_mv, MIC_BIAS_2);

	return rc;
}

static inline void tavil_mbhc_get_result_params(struct wcd9xxx *wcd9xxx,
						s16 *d1_a, u16 noff,
						int32_t *zdet)
{
	int i;
	int val, val1;
	s16 c1;
	s32 x1, d1;
	int32_t denom;
	int minCode_param[] = {
			3277, 1639, 820, 410, 205, 103, 52, 26
	};

	regmap_update_bits(wcd9xxx->regmap, WCD934X_ANA_MBHC_ZDET, 0x20, 0x20);
	for (i = 0; i < TAVIL_ZDET_NUM_MEASUREMENTS; i++) {
		regmap_read(wcd9xxx->regmap, WCD934X_ANA_MBHC_RESULT_2, &val);
		if (val & 0x80)
			break;
	}
	val = val << 0x8;
	regmap_read(wcd9xxx->regmap, WCD934X_ANA_MBHC_RESULT_1, &val1);
	val |= val1;
	regmap_update_bits(wcd9xxx->regmap, WCD934X_ANA_MBHC_ZDET, 0x20, 0x00);
	x1 = TAVIL_MBHC_GET_X1(val);
	c1 = TAVIL_MBHC_GET_C1(val);
	/* If ramp is not complete, give additional 5ms */
	if ((c1 < 2) && x1)
		usleep_range(5000, 5050);

	if (!c1 || !x1) {
		dev_dbg(wcd9xxx->dev,
			"%s: Impedance detect ramp error, c1=%d, x1=0x%x\n",
			__func__, c1, x1);
		goto ramp_down;
	}
	d1 = d1_a[c1];
	denom = (x1 * d1) - (1 << (14 - noff));
	if (denom > 0)
		*zdet = (TAVIL_MBHC_ZDET_CONST * 1000) / denom;
	else if (x1 < minCode_param[noff])
		*zdet = TAVIL_ZDET_FLOATING_IMPEDANCE;

	dev_dbg(wcd9xxx->dev, "%s: d1=%d, c1=%d, x1=0x%x, z_val=%d(milliOhm)\n",
		__func__, d1, c1, x1, *zdet);
ramp_down:
	i = 0;
	while (x1) {
		regmap_bulk_read(wcd9xxx->regmap,
				 WCD934X_ANA_MBHC_RESULT_1, (u8 *)&val, 2);
		x1 = TAVIL_MBHC_GET_X1(val);
		i++;
		if (i == TAVIL_ZDET_NUM_MEASUREMENTS)
			break;
	}
}

static void tavil_mbhc_zdet_ramp(struct snd_soc_codec *codec,
				 struct tavil_mbhc_zdet_param *zdet_param,
				 int32_t *zl, int32_t *zr, s16 *d1_a)
{
	struct wcd9xxx *wcd9xxx = dev_get_drvdata(codec->dev->parent);
	int32_t zdet = 0;

	snd_soc_update_bits(codec, WCD934X_MBHC_NEW_ZDET_ANA_CTL, 0x70,
			    zdet_param->ldo_ctl << 4);
	snd_soc_update_bits(codec, WCD934X_ANA_MBHC_BTN5, 0xFC,
			    zdet_param->btn5);
	snd_soc_update_bits(codec, WCD934X_ANA_MBHC_BTN6, 0xFC,
			    zdet_param->btn6);
	snd_soc_update_bits(codec, WCD934X_ANA_MBHC_BTN7, 0xFC,
			    zdet_param->btn7);
	snd_soc_update_bits(codec, WCD934X_MBHC_NEW_ZDET_ANA_CTL, 0x0F,
			    zdet_param->noff);
	snd_soc_update_bits(codec, WCD934X_MBHC_NEW_ZDET_RAMP_CTL, 0x0F,
			    zdet_param->nshift);

	if (!zl)
		goto z_right;
	/* Start impedance measurement for HPH_L */
	regmap_update_bits(wcd9xxx->regmap,
			   WCD934X_ANA_MBHC_ZDET, 0x80, 0x80);
	dev_dbg(wcd9xxx->dev, "%s: ramp for HPH_L, noff = %d\n",
		__func__, zdet_param->noff);
	tavil_mbhc_get_result_params(wcd9xxx, d1_a, zdet_param->noff, &zdet);
	regmap_update_bits(wcd9xxx->regmap,
			   WCD934X_ANA_MBHC_ZDET, 0x80, 0x00);

	*zl = zdet;

z_right:
	if (!zr)
		return;
	/* Start impedance measurement for HPH_R */
	regmap_update_bits(wcd9xxx->regmap,
			   WCD934X_ANA_MBHC_ZDET, 0x40, 0x40);
	dev_dbg(wcd9xxx->dev, "%s: ramp for HPH_R, noff = %d\n",
		__func__, zdet_param->noff);
	tavil_mbhc_get_result_params(wcd9xxx, d1_a, zdet_param->noff, &zdet);
	regmap_update_bits(wcd9xxx->regmap,
			   WCD934X_ANA_MBHC_ZDET, 0x40, 0x00);

	*zr = zdet;
}

static inline void tavil_wcd_mbhc_qfuse_cal(struct snd_soc_codec *codec,
					    int32_t *z_val, int flag_l_r)
{
	s16 q1;
	int q1_cal;

	if (*z_val < (TAVIL_ZDET_VAL_400/1000))
		q1 = snd_soc_read(codec,
			WCD934X_CHIP_TIER_CTRL_EFUSE_VAL_OUT1 + (2 * flag_l_r));
	else
		q1 = snd_soc_read(codec,
			WCD934X_CHIP_TIER_CTRL_EFUSE_VAL_OUT2 + (2 * flag_l_r));
	if (q1 & 0x80)
		q1_cal = (10000 - ((q1 & 0x7F) * 25));
	else
		q1_cal = (10000 + (q1 * 25));
	if (q1_cal > 0)
		*z_val = ((*z_val) * 10000) / q1_cal;
}

static void tavil_wcd_mbhc_calc_impedance(struct wcd_mbhc *mbhc, uint32_t *zl,
					  uint32_t *zr)
{
	struct snd_soc_codec *codec = mbhc->codec;
	struct wcd9xxx *wcd9xxx = dev_get_drvdata(codec->dev->parent);
	s16 reg0, reg1, reg2, reg3, reg4;
	int32_t z1L, z1R, z1Ls;
	int zMono, z_diff1, z_diff2;
	bool is_fsm_disable = false;
	struct tavil_mbhc_zdet_param zdet_param[] = {
		{4, 0, 4, 0x08, 0x14, 0x18}, /* < 32ohm */
		{2, 0, 3, 0x18, 0x7C, 0x90}, /* 32ohm < Z < 400ohm */
		{1, 4, 5, 0x18, 0x7C, 0x90}, /* 400ohm < Z < 1200ohm */
		{1, 6, 7, 0x18, 0x7C, 0x90}, /* >1200ohm */
	};
	struct tavil_mbhc_zdet_param *zdet_param_ptr = NULL;
	s16 d1_a[][4] = {
		{0, 30, 90, 30},
		{0, 30, 30, 5},
		{0, 30, 30, 5},
		{0, 30, 30, 5},
	};
	s16 *d1 = NULL;

	WCD_MBHC_RSC_ASSERT_LOCKED(mbhc);

	reg0 = snd_soc_read(codec, WCD934X_ANA_MBHC_BTN5);
	reg1 = snd_soc_read(codec, WCD934X_ANA_MBHC_BTN6);
	reg2 = snd_soc_read(codec, WCD934X_ANA_MBHC_BTN7);
	reg3 = snd_soc_read(codec, WCD934X_MBHC_CTL_CLK);
	reg4 = snd_soc_read(codec, WCD934X_MBHC_NEW_ZDET_ANA_CTL);

	if (snd_soc_read(codec, WCD934X_ANA_MBHC_ELECT) & 0x80) {
		is_fsm_disable = true;
		regmap_update_bits(wcd9xxx->regmap,
				   WCD934X_ANA_MBHC_ELECT, 0x80, 0x00);
	}

	/* For NO-jack, disable L_DET_EN before Z-det measurements */
	if (mbhc->hphl_swh)
		regmap_update_bits(wcd9xxx->regmap,
				   WCD934X_ANA_MBHC_MECH, 0x80, 0x00);

	/* Turn off 100k pull down on HPHL */
	regmap_update_bits(wcd9xxx->regmap,
			   WCD934X_ANA_MBHC_MECH, 0x01, 0x00);

	/* First get impedance on Left */
	d1 = d1_a[1];
	zdet_param_ptr = &zdet_param[1];
	tavil_mbhc_zdet_ramp(codec, zdet_param_ptr, &z1L, NULL, d1);

	if (!TAVIL_MBHC_IS_SECOND_RAMP_REQUIRED(z1L))
		goto left_ch_impedance;

	/* Second ramp for left ch */
	if (z1L < TAVIL_ZDET_VAL_32) {
		zdet_param_ptr = &zdet_param[0];
		d1 = d1_a[0];
	} else if ((z1L > TAVIL_ZDET_VAL_400) && (z1L <= TAVIL_ZDET_VAL_1200)) {
		zdet_param_ptr = &zdet_param[2];
		d1 = d1_a[2];
	} else if (z1L > TAVIL_ZDET_VAL_1200) {
		zdet_param_ptr = &zdet_param[3];
		d1 = d1_a[3];
	}
	tavil_mbhc_zdet_ramp(codec, zdet_param_ptr, &z1L, NULL, d1);

left_ch_impedance:
	if ((z1L == TAVIL_ZDET_FLOATING_IMPEDANCE) ||
		(z1L > TAVIL_ZDET_VAL_100K)) {
		*zl = TAVIL_ZDET_FLOATING_IMPEDANCE;
		zdet_param_ptr = &zdet_param[1];
		d1 = d1_a[1];
	} else {
		*zl = z1L/1000;
		tavil_wcd_mbhc_qfuse_cal(codec, zl, 0);
	}
	dev_dbg(codec->dev, "%s: impedance on HPH_L = %d(ohms)\n",
		__func__, *zl);

	/* Start of right impedance ramp and calculation */
	tavil_mbhc_zdet_ramp(codec, zdet_param_ptr, NULL, &z1R, d1);
	if (TAVIL_MBHC_IS_SECOND_RAMP_REQUIRED(z1R)) {
		if (((z1R > TAVIL_ZDET_VAL_1200) &&
			(zdet_param_ptr->noff == 0x6)) ||
			((*zl) != TAVIL_ZDET_FLOATING_IMPEDANCE))
			goto right_ch_impedance;
		/* Second ramp for right ch */
		if (z1R < TAVIL_ZDET_VAL_32) {
			zdet_param_ptr = &zdet_param[0];
			d1 = d1_a[0];
		} else if ((z1R > TAVIL_ZDET_VAL_400) &&
			(z1R <= TAVIL_ZDET_VAL_1200)) {
			zdet_param_ptr = &zdet_param[2];
			d1 = d1_a[2];
		} else if (z1R > TAVIL_ZDET_VAL_1200) {
			zdet_param_ptr = &zdet_param[3];
			d1 = d1_a[3];
		}
		tavil_mbhc_zdet_ramp(codec, zdet_param_ptr, NULL, &z1R, d1);
	}
right_ch_impedance:
	if ((z1R == TAVIL_ZDET_FLOATING_IMPEDANCE) ||
		(z1R > TAVIL_ZDET_VAL_100K)) {
		*zr = TAVIL_ZDET_FLOATING_IMPEDANCE;
	} else {
		*zr = z1R/1000;
		tavil_wcd_mbhc_qfuse_cal(codec, zr, 1);
	}
	dev_dbg(codec->dev, "%s: impedance on HPH_R = %d(ohms)\n",
		__func__, *zr);

	/* Mono/stereo detection */
	if ((*zl == TAVIL_ZDET_FLOATING_IMPEDANCE) &&
		(*zr == TAVIL_ZDET_FLOATING_IMPEDANCE)) {
		dev_dbg(codec->dev,
			"%s: plug type is invalid or extension cable\n",
			__func__);
		goto zdet_complete;
	}
	if ((*zl == TAVIL_ZDET_FLOATING_IMPEDANCE) ||
	    (*zr == TAVIL_ZDET_FLOATING_IMPEDANCE) ||
	    ((*zl < WCD_MONO_HS_MIN_THR) && (*zr > WCD_MONO_HS_MIN_THR)) ||
	    ((*zl > WCD_MONO_HS_MIN_THR) && (*zr < WCD_MONO_HS_MIN_THR))) {
		dev_dbg(codec->dev,
			"%s: Mono plug type with one ch floating or shorted to GND\n",
			__func__);
		mbhc->hph_type = WCD_MBHC_HPH_MONO;
		goto zdet_complete;
	}
	snd_soc_update_bits(codec, WCD934X_HPH_R_ATEST, 0x02, 0x02);
	snd_soc_update_bits(codec, WCD934X_HPH_PA_CTL2, 0x40, 0x01);
	if (*zl < (TAVIL_ZDET_VAL_32/1000))
		tavil_mbhc_zdet_ramp(codec, &zdet_param[0], &z1Ls, NULL, d1);
	else
		tavil_mbhc_zdet_ramp(codec, &zdet_param[1], &z1Ls, NULL, d1);
	snd_soc_update_bits(codec, WCD934X_HPH_PA_CTL2, 0x40, 0x00);
	snd_soc_update_bits(codec, WCD934X_HPH_R_ATEST, 0x02, 0x00);
	z1Ls /= 1000;
	tavil_wcd_mbhc_qfuse_cal(codec, &z1Ls, 0);
	/* Parallel of left Z and 9 ohm pull down resistor */
	zMono = ((*zl) * 9) / ((*zl) + 9);
	z_diff1 = (z1Ls > zMono) ? (z1Ls - zMono) : (zMono - z1Ls);
	z_diff2 = ((*zl) > z1Ls) ? ((*zl) - z1Ls) : (z1Ls - (*zl));
	if ((z_diff1 * (*zl + z1Ls)) > (z_diff2 * (z1Ls + zMono))) {
		dev_dbg(codec->dev, "%s: stereo plug type detected\n",
			__func__);
		mbhc->hph_type = WCD_MBHC_HPH_STEREO;
	} else {
		dev_dbg(codec->dev, "%s: MONO plug type detected\n",
			__func__);
		mbhc->hph_type = WCD_MBHC_HPH_MONO;
	}

zdet_complete:
	snd_soc_write(codec, WCD934X_ANA_MBHC_BTN5, reg0);
	snd_soc_write(codec, WCD934X_ANA_MBHC_BTN6, reg1);
	snd_soc_write(codec, WCD934X_ANA_MBHC_BTN7, reg2);
	/* Turn on 100k pull down on HPHL */
	regmap_update_bits(wcd9xxx->regmap,
			   WCD934X_ANA_MBHC_MECH, 0x01, 0x01);

	/* For NO-jack, re-enable L_DET_EN after Z-det measurements */
	if (mbhc->hphl_swh)
		regmap_update_bits(wcd9xxx->regmap,
				   WCD934X_ANA_MBHC_MECH, 0x80, 0x80);

	snd_soc_write(codec, WCD934X_MBHC_NEW_ZDET_ANA_CTL, reg4);
	snd_soc_write(codec, WCD934X_MBHC_CTL_CLK, reg3);
	if (is_fsm_disable)
		regmap_update_bits(wcd9xxx->regmap,
				   WCD934X_ANA_MBHC_ELECT, 0x80, 0x80);
}

static void tavil_mbhc_gnd_det_ctrl(struct snd_soc_codec *codec, bool enable)
{
	if (enable) {
		snd_soc_update_bits(codec, WCD934X_ANA_MBHC_MECH,
				    0x02, 0x02);
		snd_soc_update_bits(codec, WCD934X_ANA_MBHC_MECH,
				    0x40, 0x40);
	} else {
		snd_soc_update_bits(codec, WCD934X_ANA_MBHC_MECH,
				    0x40, 0x00);
		snd_soc_update_bits(codec, WCD934X_ANA_MBHC_MECH,
				    0x02, 0x00);
	}
}

static void tavil_mbhc_hph_pull_down_ctrl(struct snd_soc_codec *codec,
					  bool enable)
{
	if (enable) {
		snd_soc_update_bits(codec, WCD934X_HPH_PA_CTL2,
				    0x40, 0x40);
		snd_soc_update_bits(codec, WCD934X_HPH_PA_CTL2,
				    0x10, 0x10);
	} else {
		snd_soc_update_bits(codec, WCD934X_HPH_PA_CTL2,
				    0x40, 0x00);
		snd_soc_update_bits(codec, WCD934X_HPH_PA_CTL2,
				    0x10, 0x00);
	}
}
static void tavil_mbhc_moisture_config(struct wcd_mbhc *mbhc)
{
	struct snd_soc_codec *codec = mbhc->codec;

	if (TAVIL_MBHC_MOISTURE_RREF == R_OFF)
		return;

	/* Donot enable moisture detection if jack type is NC */
	if (!mbhc->hphl_swh) {
		dev_dbg(codec->dev, "%s: disable moisture detection for NC\n",
			__func__);
		return;
	}

	snd_soc_update_bits(codec, WCD934X_MBHC_NEW_CTL_2,
			    0x0C, TAVIL_MBHC_MOISTURE_RREF << 2);
}

static bool tavil_hph_register_recovery(struct wcd_mbhc *mbhc)
{
	struct snd_soc_codec *codec = mbhc->codec;
	struct wcd934x_mbhc *wcd934x_mbhc = tavil_soc_get_mbhc(codec);

	if (!wcd934x_mbhc)
		return false;

	wcd934x_mbhc->is_hph_recover = false;
	snd_soc_dapm_force_enable_pin(snd_soc_codec_get_dapm(codec),
				      "RESET_HPH_REGISTERS");
	snd_soc_dapm_sync(snd_soc_codec_get_dapm(codec));

	snd_soc_dapm_disable_pin(snd_soc_codec_get_dapm(codec),
				 "RESET_HPH_REGISTERS");
	snd_soc_dapm_sync(snd_soc_codec_get_dapm(codec));

	return wcd934x_mbhc->is_hph_recover;
}

static const struct wcd_mbhc_cb mbhc_cb = {
	.request_irq = tavil_mbhc_request_irq,
	.irq_control = tavil_mbhc_irq_control,
	.free_irq = tavil_mbhc_free_irq,
	.clk_setup = tavil_mbhc_clk_setup,
	.map_btn_code_to_num = tavil_mbhc_btn_to_num,
	.enable_mb_source = tavil_enable_ext_mb_source,
	.mbhc_bias = tavil_mbhc_mbhc_bias_control,
	.set_btn_thr = tavil_mbhc_program_btn_thr,
	.lock_sleep = tavil_mbhc_lock_sleep,
	.register_notifier = tavil_mbhc_register_notifier,
	.micbias_enable_status = tavil_mbhc_micb_en_status,
	.hph_pa_on_status = tavil_mbhc_hph_pa_on_status,
	.hph_pull_up_control = tavil_mbhc_hph_l_pull_up_control,
	.mbhc_micbias_control = tavil_mbhc_request_micbias,
	.mbhc_micb_ramp_control = tavil_mbhc_micb_ramp_control,
	.get_hwdep_fw_cal = tavil_get_hwdep_fw_cal,
	.mbhc_micb_ctrl_thr_mic = tavil_mbhc_micb_ctrl_threshold_mic,
	.compute_impedance = tavil_wcd_mbhc_calc_impedance,
	.mbhc_gnd_det_ctrl = tavil_mbhc_gnd_det_ctrl,
	.hph_pull_down_ctrl = tavil_mbhc_hph_pull_down_ctrl,
	.mbhc_moisture_config = tavil_mbhc_moisture_config,
	.hph_register_recovery = tavil_hph_register_recovery,
};

static struct regulator *tavil_codec_find_ondemand_regulator(
		struct snd_soc_codec *codec, const char *name)
{
	int i;
	struct wcd9xxx *wcd9xxx = dev_get_drvdata(codec->dev->parent);
	struct wcd9xxx_pdata *pdata = dev_get_platdata(codec->dev->parent);

	for (i = 0; i < wcd9xxx->num_of_supplies; ++i) {
		if (pdata->regulator[i].ondemand &&
		    wcd9xxx->supplies[i].supply &&
		    !strcmp(wcd9xxx->supplies[i].supply, name))
			return wcd9xxx->supplies[i].consumer;
	}

	dev_dbg(codec->dev, "Warning: regulator not found:%s\n",
		name);
	return NULL;
}

static int tavil_get_hph_type(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct wcd934x_mbhc *wcd934x_mbhc = tavil_soc_get_mbhc(codec);
	struct wcd_mbhc *mbhc;

	if (!wcd934x_mbhc) {
		dev_err(codec->dev, "%s: mbhc not initialized!\n", __func__);
		return -EINVAL;
	}

	mbhc = &wcd934x_mbhc->wcd_mbhc;

	ucontrol->value.integer.value[0] = (u32) mbhc->hph_type;
	dev_dbg(codec->dev, "%s: hph_type = %u\n", __func__, mbhc->hph_type);

	return 0;
}

static const struct snd_kcontrol_new hph_type_detect_controls[] = {
	SOC_SINGLE_EXT("HPH Type", 0, 0, UINT_MAX, 0,
		       tavil_get_hph_type, NULL),
};

/*
 * tavil_mbhc_hs_detect: starts mbhc insertion/removal functionality
 * @codec: handle to snd_soc_codec *
 * @mbhc_cfg: handle to mbhc configuration structure
 * return 0 if mbhc_start is success or error code in case of failure
 */
int tavil_mbhc_hs_detect(struct snd_soc_codec *codec,
			 struct wcd_mbhc_config *mbhc_cfg)
{
	struct wcd934x_mbhc *wcd934x_mbhc = tavil_soc_get_mbhc(codec);

	if (!wcd934x_mbhc) {
		dev_err(codec->dev, "%s: mbhc not initialized!\n", __func__);
		return -EINVAL;
	}

	return wcd_mbhc_start(&wcd934x_mbhc->wcd_mbhc, mbhc_cfg);
}
EXPORT_SYMBOL(tavil_mbhc_hs_detect);

/*
 * tavil_mbhc_hs_detect_exit: stop mbhc insertion/removal functionality
 * @codec: handle to snd_soc_codec *
 */
void tavil_mbhc_hs_detect_exit(struct snd_soc_codec *codec)
{
	struct wcd934x_mbhc *wcd934x_mbhc = tavil_soc_get_mbhc(codec);

	if (!wcd934x_mbhc) {
		dev_err(codec->dev, "%s: mbhc not initialized!\n", __func__);
		return;
	}
	wcd_mbhc_stop(&wcd934x_mbhc->wcd_mbhc);
}
EXPORT_SYMBOL(tavil_mbhc_hs_detect_exit);

/*
 * tavil_mbhc_post_ssr_init: initialize mbhc for tavil post subsystem restart
 * @mbhc: poniter to wcd934x_mbhc structure
 * @codec: handle to snd_soc_codec *
 *
 * return 0 if mbhc_init is success or error code in case of failure
 */
int tavil_mbhc_post_ssr_init(struct wcd934x_mbhc *mbhc,
			     struct snd_soc_codec *codec)
{
	int ret;

	if (!mbhc || !codec)
		return -EINVAL;

	wcd_mbhc_deinit(&mbhc->wcd_mbhc);
	ret = wcd_mbhc_init(&mbhc->wcd_mbhc, codec, &mbhc_cb, &intr_ids,
			    wcd_mbhc_registers, TAVIL_ZDET_SUPPORTED);
	if (ret) {
		dev_err(codec->dev, "%s: mbhc initialization failed\n",
			__func__);
		goto done;
	}
	snd_soc_update_bits(codec, WCD934X_MBHC_NEW_CTL_1, 0x04, 0x04);
	snd_soc_update_bits(codec, WCD934X_MBHC_CTL_BCS, 0x01, 0x01);

done:
	return ret;
}
EXPORT_SYMBOL(tavil_mbhc_post_ssr_init);

/*
 * tavil_mbhc_init: initialize mbhc for tavil
 * @mbhc: poniter to wcd934x_mbhc struct pointer to store the configs
 * @codec: handle to snd_soc_codec *
 * @fw_data: handle to firmware data
 *
 * return 0 if mbhc_init is success or error code in case of failure
 */
int tavil_mbhc_init(struct wcd934x_mbhc **mbhc, struct snd_soc_codec *codec,
		    struct fw_info *fw_data)
{
	struct regulator *supply;
	struct wcd934x_mbhc *wcd934x_mbhc;
	int ret;

	wcd934x_mbhc = devm_kzalloc(codec->dev, sizeof(struct wcd934x_mbhc),
				    GFP_KERNEL);
	if (!wcd934x_mbhc)
		return -ENOMEM;

	wcd934x_mbhc->wcd9xxx = dev_get_drvdata(codec->dev->parent);
	wcd934x_mbhc->fw_data = fw_data;
	BLOCKING_INIT_NOTIFIER_HEAD(&wcd934x_mbhc->notifier);

	ret = wcd_mbhc_init(&wcd934x_mbhc->wcd_mbhc, codec, &mbhc_cb, &intr_ids,
			    wcd_mbhc_registers, TAVIL_ZDET_SUPPORTED);
	if (ret) {
		dev_err(codec->dev, "%s: mbhc initialization failed\n",
			__func__);
		goto err;
	}

	supply = tavil_codec_find_ondemand_regulator(codec,
			on_demand_supply_name[WCD934X_ON_DEMAND_MICBIAS]);
	if (supply) {
		wcd934x_mbhc->on_demand_list[
			WCD934X_ON_DEMAND_MICBIAS].supply =
				supply;
		wcd934x_mbhc->on_demand_list[
			WCD934X_ON_DEMAND_MICBIAS].ondemand_supply_count =
				0;
	}

	snd_soc_add_codec_controls(codec, hph_type_detect_controls,
				   ARRAY_SIZE(hph_type_detect_controls));

	snd_soc_update_bits(codec, WCD934X_MBHC_NEW_CTL_1, 0x04, 0x04);
	snd_soc_update_bits(codec, WCD934X_MBHC_CTL_BCS, 0x01, 0x01);

	(*mbhc) = wcd934x_mbhc;

	return 0;
err:
	devm_kfree(codec->dev, wcd934x_mbhc);
	return ret;
}
EXPORT_SYMBOL(tavil_mbhc_init);

/*
 * tavil_mbhc_deinit: deinitialize mbhc for tavil
 * @codec: handle to snd_soc_codec *
 */
void tavil_mbhc_deinit(struct snd_soc_codec *codec)
{
	struct wcd934x_mbhc *wcd934x_mbhc = tavil_soc_get_mbhc(codec);

	if (wcd934x_mbhc) {
		wcd_mbhc_deinit(&wcd934x_mbhc->wcd_mbhc);
		devm_kfree(codec->dev, wcd934x_mbhc);
	}
}
EXPORT_SYMBOL(tavil_mbhc_deinit);
