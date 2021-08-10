// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2020, The Linux Foundation. All rights reserved.
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
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include "rouleur-registers.h"
#include <asoc/wcdcal-hwdep.h>
#include <asoc/wcd-mbhc-v2-api.h>
#include "internal.h"

#define ROULEUR_ZDET_SUPPORTED          true
/* Z value defined in milliohm */
#define ROULEUR_ZDET_VAL_100K           100000000
/* Z floating defined in ohms */
#define ROULEUR_ZDET_FLOATING_IMPEDANCE 0x0FFFFFFE

#define ROULEUR_ZDET_NUM_MEASUREMENTS   100
#define ROULEUR_ZDET_RMAX               1280000
#define ROULEUR_ZDET_C1                 7500000
#define ROULEUR_ZDET_C2                 187
#define ROULEUR_ZDET_C3                 4500

/* Cross connection thresholds in mV */
#define ROULEUR_HPHL_CROSS_CONN_THRESHOLD 350
#define ROULEUR_HPHR_CROSS_CONN_THRESHOLD 350

#define IMPED_NUM_RETRY 5

static struct wcd_mbhc_register
	wcd_mbhc_registers[WCD_MBHC_REG_FUNC_MAX] = {
	WCD_MBHC_REGISTER("WCD_MBHC_L_DET_EN",
			  ROULEUR_ANA_MBHC_MECH, 0x80, 7, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_GND_DET_EN",
			  ROULEUR_ANA_MBHC_MECH, 0x40, 6, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_MECH_DETECTION_TYPE",
			  ROULEUR_ANA_MBHC_MECH, 0x20, 5, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_MIC_CLAMP_CTL",
			  ROULEUR_ANA_MBHC_PLUG_DETECT_CTL, 0x30, 4, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_ELECT_DETECTION_TYPE",
			  ROULEUR_ANA_MBHC_ELECT, 0x08, 3, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HS_L_DET_PULL_UP_CTRL",
			  ROULEUR_ANA_MBHC_PLUG_DETECT_CTL, 0xC0, 6, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HS_L_DET_PULL_UP_COMP_CTRL",
			  ROULEUR_ANA_MBHC_MECH, 0x04, 2, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHL_PLUG_TYPE",
			  ROULEUR_ANA_MBHC_MECH, 0x10, 4, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_GND_PLUG_TYPE",
			  ROULEUR_ANA_MBHC_MECH, 0x08, 3, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_SW_HPH_LP_100K_TO_GND",
			  ROULEUR_ANA_MBHC_MECH, 0x01, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_ELECT_SCHMT_ISRC",
			  ROULEUR_ANA_MBHC_ELECT, 0x06, 1, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_FSM_EN",
			  ROULEUR_ANA_MBHC_ELECT, 0x80, 7, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_INSREM_DBNC",
			  ROULEUR_ANA_MBHC_PLUG_DETECT_CTL, 0x0F, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_BTN_DBNC",
			  ROULEUR_ANA_MBHC_CTL_1, 0x03, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HS_VREF",
			  ROULEUR_ANA_MBHC_CTL_2, 0x03, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HS_COMP_RESULT",
			  ROULEUR_ANA_MBHC_RESULT_3, 0x08, 3, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_IN2P_CLAMP_STATE",
			  ROULEUR_ANA_MBHC_RESULT_3, 0x10, 4, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_MIC_SCHMT_RESULT",
			  ROULEUR_ANA_MBHC_RESULT_3, 0x20, 5, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHL_SCHMT_RESULT",
			  ROULEUR_ANA_MBHC_RESULT_3, 0x80, 7, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHR_SCHMT_RESULT",
			  ROULEUR_ANA_MBHC_RESULT_3, 0x40, 6, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_OCP_FSM_EN",
			  SND_SOC_NOPM, 0x00, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_BTN_RESULT",
			  ROULEUR_ANA_MBHC_RESULT_3, 0x07, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_BTN_ISRC_CTL",
			  ROULEUR_ANA_MBHC_ELECT, 0x70, 4, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_ELECT_RESULT",
			  ROULEUR_ANA_MBHC_RESULT_3, 0xFF, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_MICB_CTRL",
			  ROULEUR_ANA_MICBIAS_MICB_1_2_EN, 0x06, 1, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPH_CNP_WG_TIME",
			  SND_SOC_NOPM, 0x00, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHR_PA_EN",
			  ROULEUR_ANA_HPHPA_CNP_CTL_2, 0x40, 6, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHL_PA_EN",
			  ROULEUR_ANA_HPHPA_CNP_CTL_2, 0x80, 7, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPH_PA_EN",
			  ROULEUR_ANA_HPHPA_CNP_CTL_2, 0xC0, 6, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_SWCH_LEVEL_REMOVE",
			  ROULEUR_ANA_MBHC_RESULT_3, 0x10, 4, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_PULLDOWN_CTRL",
			  0, 0, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_ANC_DET_EN",
			  SND_SOC_NOPM, 0x00, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_FSM_STATUS",
			  ROULEUR_ANA_MBHC_FSM_STATUS, 0x01, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_MUX_CTL",
			  ROULEUR_ANA_MBHC_CTL_2, 0x70, 4, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_MOISTURE_STATUS",
			  ROULEUR_ANA_MBHC_FSM_STATUS, 0x20, 5, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHR_GND",
			  SND_SOC_NOPM, 0x00, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHL_GND",
			  SND_SOC_NOPM, 0x00, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHL_OCP_DET_EN",
			  ROULEUR_ANA_HPHPA_CNP_CTL_2, 0x02, 1, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHR_OCP_DET_EN",
			  ROULEUR_ANA_HPHPA_CNP_CTL_2, 0x01, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHL_OCP_STATUS",
			  ROULEUR_DIG_SWR_INTR_STATUS_0, 0x80, 7, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHR_OCP_STATUS",
			  ROULEUR_DIG_SWR_INTR_STATUS_0, 0x20, 5, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_ADC_EN",
			  ROULEUR_ANA_MBHC_CTL_1, 0x08, 3, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_ADC_COMPLETE", ROULEUR_ANA_MBHC_FSM_STATUS,
			  0x40, 6, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_ADC_TIMEOUT", ROULEUR_ANA_MBHC_FSM_STATUS,
			  0x80, 7, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_ADC_RESULT", ROULEUR_ANA_MBHC_ADC_RESULT,
			  0xFF, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_MICB2_VOUT",
			  ROULEUR_ANA_MICBIAS_LDO_1_SETTING, 0xF8, 3, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_ADC_MODE",
			  ROULEUR_ANA_MBHC_CTL_1, 0x10, 4, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_DETECTION_DONE",
			  ROULEUR_ANA_MBHC_CTL_1, 0x04, 2, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_ELECT_ISRC_EN",
			  ROULEUR_ANA_MBHC_ZDET, 0x02, 1, 0),
};

static const struct wcd_mbhc_intr intr_ids = {
	.mbhc_sw_intr =  ROULEUR_IRQ_MBHC_SW_DET,
	.mbhc_btn_press_intr = ROULEUR_IRQ_MBHC_BUTTON_PRESS_DET,
	.mbhc_btn_release_intr = ROULEUR_IRQ_MBHC_BUTTON_RELEASE_DET,
	.mbhc_hs_ins_intr = ROULEUR_IRQ_MBHC_ELECT_INS_REM_LEG_DET,
	.mbhc_hs_rem_intr = ROULEUR_IRQ_MBHC_ELECT_INS_REM_DET,
	.hph_left_ocp = ROULEUR_IRQ_HPHL_OCP_INT,
	.hph_right_ocp = ROULEUR_IRQ_HPHR_OCP_INT,
};

struct rouleur_mbhc_zdet_param {
	u16 ldo_ctl;
	u16 noff;
	u16 nshift;
};

static int rouleur_mbhc_request_irq(struct snd_soc_component *component,
				  int irq, irq_handler_t handler,
				  const char *name, void *data)
{
	struct rouleur_priv *rouleur = dev_get_drvdata(component->dev);

	return wcd_request_irq(&rouleur->irq_info, irq, name, handler, data);
}

static void rouleur_mbhc_irq_control(struct snd_soc_component *component,
				   int irq, bool enable)
{
	struct rouleur_priv *rouleur = dev_get_drvdata(component->dev);

	if (enable)
		wcd_enable_irq(&rouleur->irq_info, irq);
	else
		wcd_disable_irq(&rouleur->irq_info, irq);
}

static int rouleur_mbhc_free_irq(struct snd_soc_component *component,
			       int irq, void *data)
{
	struct rouleur_priv *rouleur = dev_get_drvdata(component->dev);

	wcd_free_irq(&rouleur->irq_info, irq, data);

	return 0;
}

static void rouleur_mbhc_clk_setup(struct snd_soc_component *component,
				 bool enable)
{
	if (enable)
		snd_soc_component_update_bits(component, ROULEUR_ANA_MBHC_CTL_1,
				    0x80, 0x80);
	else
		snd_soc_component_update_bits(component, ROULEUR_ANA_MBHC_CTL_1,
				    0x80, 0x00);
}

static int rouleur_mbhc_btn_to_num(struct snd_soc_component *component)
{
	return snd_soc_component_read32(component, ROULEUR_ANA_MBHC_RESULT_3) &
				0x7;
}

static void rouleur_mbhc_mbhc_bias_control(struct snd_soc_component *component,
					 bool enable)
{
	if (enable)
		snd_soc_component_update_bits(component, ROULEUR_ANA_MBHC_ELECT,
				    0x01, 0x01);
	else
		snd_soc_component_update_bits(component, ROULEUR_ANA_MBHC_ELECT,
				    0x01, 0x00);
}

static void rouleur_mbhc_program_btn_thr(struct snd_soc_component *component,
				       s16 *btn_low, s16 *btn_high,
				       int num_btn, bool is_micbias)
{
	int i;
	int vth;

	if (num_btn > WCD_MBHC_DEF_BUTTONS) {
		dev_err(component->dev, "%s: invalid number of buttons: %d\n",
			__func__, num_btn);
		return;
	}

	for (i = 0; i < num_btn; i++) {
		vth = ((btn_high[i] * 2) / 25) & 0x3F;
		snd_soc_component_update_bits(component,
				ROULEUR_ANA_MBHC_BTN0_ZDET_VREF1 + i,
				0xFC, vth << 2);
		dev_dbg(component->dev, "%s: btn_high[%d]: %d, vth: %d\n",
			__func__, i, btn_high[i], vth);
	}
}

static bool rouleur_mbhc_lock_sleep(struct wcd_mbhc *mbhc, bool lock)
{
	struct snd_soc_component *component = mbhc->component;
	struct rouleur_priv *rouleur = dev_get_drvdata(component->dev);

	rouleur->wakeup((void *)rouleur, lock);
	return true;
}

static int rouleur_mbhc_register_notifier(struct wcd_mbhc *mbhc,
					struct notifier_block *nblock,
					bool enable)
{
	struct rouleur_mbhc *rouleur_mbhc;

	rouleur_mbhc = container_of(mbhc, struct rouleur_mbhc, wcd_mbhc);

	if (enable)
		return blocking_notifier_chain_register(&rouleur_mbhc->notifier,
							nblock);
	else
		return blocking_notifier_chain_unregister(
				&rouleur_mbhc->notifier, nblock);
}

static bool rouleur_mbhc_micb_en_status(struct wcd_mbhc *mbhc, int micb_num)
{
	u8 val = 0;

	if (micb_num == MIC_BIAS_2) {
		val = ((snd_soc_component_read32(mbhc->component,
				ROULEUR_ANA_MICBIAS_MICB_1_2_EN) & 0x04)
				>> 2);
		if (val == 0x01)
			return true;
	}
	return false;
}

static bool rouleur_mbhc_hph_pa_on_status(struct snd_soc_component *component)
{
	return (snd_soc_component_read32(component, ROULEUR_ANA_HPHPA_PA_STATUS)
					& 0xFF) ? true : false;
}

static void rouleur_mbhc_hph_l_pull_up_control(
				struct snd_soc_component *component,
				int pull_up_cur)
{
	/* Default pull up current to 2uA */
	if (pull_up_cur < I_OFF || pull_up_cur > I_3P0_UA ||
	    pull_up_cur == I_DEFAULT)
		pull_up_cur = I_3P0_UA;

	dev_dbg(component->dev, "%s: HS pull up current:%d\n",
		__func__, pull_up_cur);

	snd_soc_component_update_bits(component,
				ROULEUR_ANA_MBHC_PLUG_DETECT_CTL,
				0xC0, pull_up_cur << 6);
}

static int rouleur_mbhc_request_micbias(struct snd_soc_component *component,
					int micb_num, int req)
{
	int ret = 0;

	ret = rouleur_micbias_control(component, micb_num, req, false);

	return ret;
}

static void rouleur_mbhc_micb_ramp_control(struct snd_soc_component *component,
					   bool enable)
{
	if (enable) {
		snd_soc_component_update_bits(component,
					ROULEUR_ANA_MBHC_MICB2_RAMP,
					0x1C, 0x0C);
		snd_soc_component_update_bits(component,
					ROULEUR_ANA_MBHC_MICB2_RAMP,
					0x80, 0x80);
	} else {
		snd_soc_component_update_bits(component,
					ROULEUR_ANA_MBHC_MICB2_RAMP,
					0x80, 0x00);
		snd_soc_component_update_bits(component,
					ROULEUR_ANA_MBHC_MICB2_RAMP,
					0x1C, 0x00);
	}
}

static struct firmware_cal *rouleur_get_hwdep_fw_cal(struct wcd_mbhc *mbhc,
						   enum wcd_cal_type type)
{
	struct rouleur_mbhc *rouleur_mbhc;
	struct firmware_cal *hwdep_cal;
	struct snd_soc_component *component = mbhc->component;

	rouleur_mbhc = container_of(mbhc, struct rouleur_mbhc, wcd_mbhc);

	if (!component) {
		pr_err("%s: NULL component pointer\n", __func__);
		return NULL;
	}
	hwdep_cal = wcdcal_get_fw_cal(rouleur_mbhc->fw_data, type);
	if (!hwdep_cal)
		dev_err(component->dev, "%s: cal not sent by %d\n",
			__func__, type);

	return hwdep_cal;
}

static int rouleur_mbhc_micb_ctrl_threshold_mic(
					struct snd_soc_component *component,
					int micb_num, bool req_en)
{
	struct rouleur_pdata *pdata = dev_get_platdata(component->dev);
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

	rc = rouleur_mbhc_micb_adjust_voltage(component, micb_mv, MIC_BIAS_2);

	return rc;
}

static void rouleur_mbhc_get_result_params(struct rouleur_priv *rouleur,
					   struct snd_soc_component *component,
					   int32_t *zdet)
{
	int i;
	int zcode = 0, zcode1 = 0, zdet_cal_result = 0, zdet_est_range = 0;
	int noff = 0, ndac = 14;
	int zdet_cal_coeff = 0, div_ratio = 0;
	int num = 0, denom = 0;

	/* Charge enable and wait for zcode to be updated */
	regmap_update_bits(rouleur->regmap, ROULEUR_ANA_MBHC_ZDET, 0x20, 0x20);
	for (i = 0; i < ROULEUR_ZDET_NUM_MEASUREMENTS; i++) {
		regmap_read(rouleur->regmap, ROULEUR_ANA_MBHC_RESULT_2, &zcode);
		if (zcode & 0x80)
			break;
		usleep_range(200, 210);
	}

	/* If zcode updation is not complete, give additional 10ms */
	if (!(zcode & 0x80))
		usleep_range(10000, 10100);

	regmap_read(rouleur->regmap, ROULEUR_ANA_MBHC_RESULT_2, &zcode);
	if (!(zcode & 0x80)) {
		dev_dbg(rouleur->dev,
			"%s: Impedance detect calculation error, zcode=0x%x\n",
			__func__, zcode);
		regmap_update_bits(rouleur->regmap, ROULEUR_ANA_MBHC_ZDET,
				   0x20, 0x00);
		return;
	}
	zcode = zcode << 0x8;
	zcode = zcode & 0x3FFF;
	regmap_read(rouleur->regmap, ROULEUR_ANA_MBHC_RESULT_1, &zcode1);
	zcode |= zcode1;

	dev_dbg(rouleur->dev,
		"%s: zcode: %d, zcode1: %d\n", __func__, zcode, zcode1);

	/* Calculate calibration coefficient */
	zdet_cal_result = (snd_soc_component_read32(component,
				ROULEUR_ANA_MBHC_ZDET_CALIB_RESULT)) & 0x1F;
	zdet_cal_coeff = ROULEUR_ZDET_C1 /
			((ROULEUR_ZDET_C2 * zdet_cal_result) + ROULEUR_ZDET_C3);
	/* Rload calculation */
	zdet_est_range = (snd_soc_component_read32(component,
			  ROULEUR_ANA_MBHC_ZDET_CALIB_RESULT) & 0x60) >> 5;

	dev_dbg(rouleur->dev,
		"%s: zdet_cal_result: %d, zdet_cal_coeff: %d, zdet_est_range: %d\n",
		__func__, zdet_cal_result, zdet_cal_coeff, zdet_est_range);
	switch (zdet_est_range) {
	case 0:
	default:
		noff = 0;
		div_ratio = 320;
		break;
	case 1:
		noff = 0;
		div_ratio = 64;
		break;
	case 2:
		noff = 4;
		div_ratio = 64;
		break;
	case 3:
		noff = 5;
		div_ratio = 40;
		break;
	}

	num = zdet_cal_coeff * ROULEUR_ZDET_RMAX;
	denom = ((zcode * div_ratio * 100) - (1 << (ndac - noff)) * 1000);
	dev_dbg(rouleur->dev,
		"%s: num: %d, denom: %d\n", __func__, num, denom);
	if (denom > 0)
		*zdet = (int32_t) ((num / denom) * 1000);
	else
		*zdet = ROULEUR_ZDET_FLOATING_IMPEDANCE;

	dev_dbg(rouleur->dev, "%s: z_val=%d(milliOhm)\n",
		__func__, *zdet);
	/* Start discharge */
	regmap_update_bits(rouleur->regmap, ROULEUR_ANA_MBHC_ZDET, 0x20, 0x00);
}

static void rouleur_mbhc_zdet_start(struct snd_soc_component *component,
				 int32_t *zl, int32_t *zr)
{
	struct rouleur_priv *rouleur = dev_get_drvdata(component->dev);
	int32_t zdet = 0;

	if (!zl)
		goto z_right;

	/* HPHL pull down switch to force OFF */
	regmap_update_bits(rouleur->regmap,
			  ROULEUR_ANA_HPHPA_CNP_CTL_2, 0x30, 0x00);
	/* Averaging enable for reliable results */
	regmap_update_bits(rouleur->regmap,
			   ROULEUR_ANA_MBHC_ZDET_ANA_CTL, 0x80, 0x80);
	/* ZDET left measurement enable */
	regmap_update_bits(rouleur->regmap,
			   ROULEUR_ANA_MBHC_ZDET, 0x80, 0x80);
	/* Calculate the left Rload result */
	rouleur_mbhc_get_result_params(rouleur, component, &zdet);

	regmap_update_bits(rouleur->regmap,
			   ROULEUR_ANA_MBHC_ZDET, 0x80, 0x00);
	regmap_update_bits(rouleur->regmap,
			   ROULEUR_ANA_MBHC_ZDET_ANA_CTL, 0x80, 0x00);
	regmap_update_bits(rouleur->regmap,
			  ROULEUR_ANA_HPHPA_CNP_CTL_2, 0x30, 0x20);

	*zl = zdet;

z_right:
	if (!zr)
		return;
	/* HPHR pull down switch to force OFF */
	regmap_update_bits(rouleur->regmap,
			  ROULEUR_ANA_HPHPA_CNP_CTL_2, 0x0C, 0x00);
	/* Averaging enable for reliable results */
	regmap_update_bits(rouleur->regmap,
			   ROULEUR_ANA_MBHC_ZDET_ANA_CTL, 0x80, 0x80);
	/* ZDET right measurement enable */
	regmap_update_bits(rouleur->regmap,
			   ROULEUR_ANA_MBHC_ZDET, 0x40, 0x40);

	/* Calculate the right Rload result */
	rouleur_mbhc_get_result_params(rouleur, component, &zdet);

	regmap_update_bits(rouleur->regmap,
			   ROULEUR_ANA_MBHC_ZDET, 0x40, 0x00);
	regmap_update_bits(rouleur->regmap,
			   ROULEUR_ANA_MBHC_ZDET_ANA_CTL, 0x80, 0x00);
	regmap_update_bits(rouleur->regmap,
			  ROULEUR_ANA_HPHPA_CNP_CTL_2, 0x0C, 0x08);

	*zr = zdet;
}

static void rouleur_mbhc_impedance_fn(struct snd_soc_component *component,
				      int32_t *z1L, int32_t *z1R,
				      int32_t *zl, int32_t *zr)
{
	int i;
	for (i = 0; i < IMPED_NUM_RETRY; i++) {
		/* Start of left ch impedance calculation */
		rouleur_mbhc_zdet_start(component, z1L, NULL);
		if ((*z1L == ROULEUR_ZDET_FLOATING_IMPEDANCE) ||
		    (*z1L > ROULEUR_ZDET_VAL_100K))
			*zl = ROULEUR_ZDET_FLOATING_IMPEDANCE;
		else
			*zl = *z1L/1000;

		/* Start of right ch impedance calculation */
		rouleur_mbhc_zdet_start(component, NULL, z1R);
		if ((*z1R == ROULEUR_ZDET_FLOATING_IMPEDANCE) ||
		    (*z1R > ROULEUR_ZDET_VAL_100K))
			*zr = ROULEUR_ZDET_FLOATING_IMPEDANCE;
		else
			*zr = *z1R/1000;
	}

	dev_dbg(component->dev, "%s: impedance on HPH_L = %d(ohms)\n",
		__func__, *zl);
	dev_dbg(component->dev, "%s: impedance on HPH_R = %d(ohms)\n",
		__func__, *zr);
}

static void rouleur_wcd_mbhc_calc_impedance(struct wcd_mbhc *mbhc, uint32_t *zl,
					  uint32_t *zr)
{
	struct snd_soc_component *component = mbhc->component;
	struct rouleur_priv *rouleur = dev_get_drvdata(component->dev);
	s16 reg0;
	int32_t z1L, z1R, z1Ls;
	int zMono, z_diff1, z_diff2;
	bool is_fsm_disable = false;

	WCD_MBHC_RSC_ASSERT_LOCKED(mbhc);

	reg0 = snd_soc_component_read32(component, ROULEUR_ANA_MBHC_ELECT);

	if (reg0 & 0x80) {
		is_fsm_disable = true;
		regmap_update_bits(rouleur->regmap,
				   ROULEUR_ANA_MBHC_ELECT, 0x80, 0x00);
	}

	/* Enable electrical bias */
	snd_soc_component_update_bits(component, ROULEUR_ANA_MBHC_ELECT,
				      0x01, 0x01);

	/* Enable codec main bias */
	rouleur_global_mbias_enable(component);

	/* Enable RCO clock */
	snd_soc_component_update_bits(component, ROULEUR_ANA_MBHC_CTL_1,
				      0x80, 0x80);

	/* For NO-jack, disable L_DET_EN before Z-det measurements */
	if (mbhc->hphl_swh)
		regmap_update_bits(rouleur->regmap,
				   ROULEUR_ANA_MBHC_MECH, 0x80, 0x00);

	/* Turn off 100k pull down on HPHL */
	regmap_update_bits(rouleur->regmap,
			   ROULEUR_ANA_MBHC_MECH, 0x01, 0x00);

	/*
	 * Disable surge protection before impedance detection.
	 * This is done to give correct value for high impedance.
	 */
	snd_soc_component_update_bits(component, ROULEUR_ANA_SURGE_EN,
					0xC0, 0x00);
	/* 1ms delay needed after disable surge protection */
	usleep_range(1000, 1010);

	/*
	 * Call impedance detection routine multiple times
	 * in order to avoid wrong impedance values.
	 */
	rouleur_mbhc_impedance_fn(component, &z1L, &z1R, zl, zr);

	/* Mono/stereo detection */
	if ((*zl == ROULEUR_ZDET_FLOATING_IMPEDANCE) &&
		(*zr == ROULEUR_ZDET_FLOATING_IMPEDANCE)) {
		dev_dbg(component->dev,
			"%s: plug type is invalid or extension cable\n",
			__func__);
		goto zdet_complete;
	}
	if ((*zl == ROULEUR_ZDET_FLOATING_IMPEDANCE) ||
	    (*zr == ROULEUR_ZDET_FLOATING_IMPEDANCE) ||
	    ((*zl < WCD_MONO_HS_MIN_THR) && (*zr > WCD_MONO_HS_MIN_THR)) ||
	    ((*zl > WCD_MONO_HS_MIN_THR) && (*zr < WCD_MONO_HS_MIN_THR))) {
		dev_dbg(component->dev,
			"%s: Mono plug type with one ch floating or shorted to GND\n",
			__func__);
		mbhc->hph_type = WCD_MBHC_HPH_MONO;
		goto zdet_complete;
	}

	z1Ls = z1L/1000;
	/* Parallel of left Z and 20 ohm pull down resistor */
	zMono = ((*zl) * 20) / ((*zl) + 20);
	z_diff1 = (z1Ls > zMono) ? (z1Ls - zMono) : (zMono - z1Ls);
	z_diff2 = ((*zl) > z1Ls) ? ((*zl) - z1Ls) : (z1Ls - (*zl));
	if ((z_diff1 * (*zl + z1Ls)) > (z_diff2 * (z1Ls + zMono))) {
		dev_dbg(component->dev, "%s: stereo plug type detected\n",
			__func__);
		mbhc->hph_type = WCD_MBHC_HPH_STEREO;
	} else {
		dev_dbg(component->dev, "%s: MONO plug type detected\n",
			__func__);
		mbhc->hph_type = WCD_MBHC_HPH_MONO;
	}

zdet_complete:
	/* Enable surge protection again after impedance detection */
	regmap_update_bits(rouleur->regmap,
			   ROULEUR_ANA_SURGE_EN, 0xC0, 0xC0);
	/* Turn on 100k pull down on HPHL */
	regmap_update_bits(rouleur->regmap,
			   ROULEUR_ANA_MBHC_MECH, 0x01, 0x01);

	/* For NO-jack, re-enable L_DET_EN after Z-det measurements */
	if (mbhc->hphl_swh)
		regmap_update_bits(rouleur->regmap,
				   ROULEUR_ANA_MBHC_MECH, 0x80, 0x80);

	/* Restore electrical bias state */
	snd_soc_component_update_bits(component, ROULEUR_ANA_MBHC_ELECT, 0x01,
				      reg0 >> 7);
	if (is_fsm_disable)
		regmap_update_bits(rouleur->regmap,
				   ROULEUR_ANA_MBHC_ELECT, 0x80, 0x80);
	rouleur_global_mbias_disable(component);
}

static void rouleur_mbhc_gnd_det_ctrl(struct snd_soc_component *component,
			bool enable)
{
	if (enable) {
		snd_soc_component_update_bits(component, ROULEUR_ANA_MBHC_MECH,
				    0x02, 0x02);
		snd_soc_component_update_bits(component, ROULEUR_ANA_MBHC_MECH,
				    0x40, 0x40);
	} else {
		snd_soc_component_update_bits(component, ROULEUR_ANA_MBHC_MECH,
				    0x40, 0x00);
		snd_soc_component_update_bits(component, ROULEUR_ANA_MBHC_MECH,
				    0x02, 0x00);
	}
}

static void rouleur_mbhc_hph_pull_down_ctrl(struct snd_soc_component *component,
					  bool enable)
{
	if (enable) {
		snd_soc_component_update_bits(component,
				    ROULEUR_ANA_HPHPA_CNP_CTL_2,
				    0x30, 0x20);
		snd_soc_component_update_bits(component,
				    ROULEUR_ANA_HPHPA_CNP_CTL_2,
				    0x0C, 0x08);
	} else {
		snd_soc_component_update_bits(component,
				    ROULEUR_ANA_HPHPA_CNP_CTL_2,
				    0x30, 0x00);
		snd_soc_component_update_bits(component,
				    ROULEUR_ANA_HPHPA_CNP_CTL_2,
				    0x0C, 0x00);
	}
}

static void rouleur_mbhc_moisture_config(struct wcd_mbhc *mbhc)
{
	struct snd_soc_component *component = mbhc->component;

	if ((mbhc->moist_rref == R_OFF) ||
	    (mbhc->mbhc_cfg->enable_usbc_analog)) {
		snd_soc_component_update_bits(component, ROULEUR_ANA_MBHC_CTL_2,
				    0x0C, R_OFF << 2);
		return;
	}

	/* Do not enable moisture detection if jack type is NC */
	if (!mbhc->hphl_swh) {
		dev_dbg(component->dev, "%s: disable moisture detection for NC\n",
			__func__);
		snd_soc_component_update_bits(component, ROULEUR_ANA_MBHC_CTL_2,
				    0x0C, R_OFF << 2);
		return;
	}

	snd_soc_component_update_bits(component, ROULEUR_ANA_MBHC_CTL_2,
			    0x0C, mbhc->moist_rref << 2);
}

static void rouleur_mbhc_moisture_detect_en(struct wcd_mbhc *mbhc, bool enable)
{
	struct snd_soc_component *component = mbhc->component;

	if (enable)
		snd_soc_component_update_bits(component, ROULEUR_ANA_MBHC_CTL_2,
					0x0C, mbhc->moist_rref << 2);
	else
		snd_soc_component_update_bits(component, ROULEUR_ANA_MBHC_CTL_2,
				    0x0C, R_OFF << 2);
}

static bool rouleur_mbhc_get_moisture_status(struct wcd_mbhc *mbhc)
{
	struct snd_soc_component *component = mbhc->component;
	bool ret = false;

	if ((mbhc->moist_rref == R_OFF) ||
	    (mbhc->mbhc_cfg->enable_usbc_analog)) {
		snd_soc_component_update_bits(component, ROULEUR_ANA_MBHC_CTL_2,
				    0x0C, R_OFF << 2);
		goto done;
	}

	/* Do not enable moisture detection if jack type is NC */
	if (!mbhc->hphl_swh) {
		dev_dbg(component->dev, "%s: disable moisture detection for NC\n",
			__func__);
		snd_soc_component_update_bits(component, ROULEUR_ANA_MBHC_CTL_2,
				    0x0C, R_OFF << 2);
		goto done;
	}

	/* If moisture_en is already enabled, then skip to plug type
	 * detection.
	 */
	if ((snd_soc_component_read32(component, ROULEUR_ANA_MBHC_CTL_2) &
			0x0C))
		goto done;

	rouleur_mbhc_moisture_detect_en(mbhc, true);
	/* Read moisture comparator status */
	ret = ((snd_soc_component_read32(component, ROULEUR_ANA_MBHC_FSM_STATUS)
				& 0x20) ? 0 : 1);

done:
	return ret;

}

static void rouleur_mbhc_bcs_enable(struct wcd_mbhc *mbhc,
						  bool bcs_enable)
{
	if (bcs_enable)
		rouleur_disable_bcs_before_slow_insert(mbhc->component, false);
	else
		rouleur_disable_bcs_before_slow_insert(mbhc->component, true);
}

static void rouleur_mbhc_get_micbias_val(struct wcd_mbhc *mbhc, int *mb)
{
	u8 vout_ctl = 0;

	/* Read MBHC Micbias (Mic Bias2) voltage */
	WCD_MBHC_REG_READ(WCD_MBHC_MICB2_VOUT, vout_ctl);

	/* Formula for getting micbias from vout
	 * micbias = 1.6V + VOUT_CTL * 50mV
	 */
	*mb = 1600 + (vout_ctl * 50);
	pr_debug("%s: vout_ctl: %d, micbias: %d\n", __func__, vout_ctl, *mb);
}

static void rouleur_mbhc_comp_autozero_control(struct wcd_mbhc *mbhc,
						bool az_enable)
{
	if (az_enable)
		snd_soc_component_update_bits(mbhc->component,
				ROULEUR_ANA_MBHC_CTL_CLK, 0x08, 0x08);
	else
		snd_soc_component_update_bits(mbhc->component,
				ROULEUR_ANA_MBHC_CTL_CLK, 0x08, 0x00);

}

static void rouleur_mbhc_surge_control(struct wcd_mbhc *mbhc,
						bool surge_enable)
{
	if (surge_enable)
		snd_soc_component_update_bits(mbhc->component,
				ROULEUR_ANA_SURGE_EN, 0xC0, 0xC0);
	else
		snd_soc_component_update_bits(mbhc->component,
				ROULEUR_ANA_SURGE_EN, 0xC0, 0x00);

}

static void rouleur_mbhc_update_cross_conn_thr(struct wcd_mbhc *mbhc)
{
	mbhc->hphl_cross_conn_thr = ROULEUR_HPHL_CROSS_CONN_THRESHOLD;
	mbhc->hphr_cross_conn_thr = ROULEUR_HPHR_CROSS_CONN_THRESHOLD;

	pr_debug("%s: Cross connection threshold for hphl: %d, hphr: %d\n",
			__func__, mbhc->hphl_cross_conn_thr,
			mbhc->hphr_cross_conn_thr);
}

static const struct wcd_mbhc_cb mbhc_cb = {
	.request_irq = rouleur_mbhc_request_irq,
	.irq_control = rouleur_mbhc_irq_control,
	.free_irq = rouleur_mbhc_free_irq,
	.clk_setup = rouleur_mbhc_clk_setup,
	.map_btn_code_to_num = rouleur_mbhc_btn_to_num,
	.mbhc_bias = rouleur_mbhc_mbhc_bias_control,
	.set_btn_thr = rouleur_mbhc_program_btn_thr,
	.lock_sleep = rouleur_mbhc_lock_sleep,
	.register_notifier = rouleur_mbhc_register_notifier,
	.micbias_enable_status = rouleur_mbhc_micb_en_status,
	.hph_pa_on_status = rouleur_mbhc_hph_pa_on_status,
	.hph_pull_up_control = rouleur_mbhc_hph_l_pull_up_control,
	.mbhc_micbias_control = rouleur_mbhc_request_micbias,
	.mbhc_micb_ramp_control = rouleur_mbhc_micb_ramp_control,
	.get_hwdep_fw_cal = rouleur_get_hwdep_fw_cal,
	.mbhc_micb_ctrl_thr_mic = rouleur_mbhc_micb_ctrl_threshold_mic,
	.compute_impedance = rouleur_wcd_mbhc_calc_impedance,
	.mbhc_gnd_det_ctrl = rouleur_mbhc_gnd_det_ctrl,
	.hph_pull_down_ctrl = rouleur_mbhc_hph_pull_down_ctrl,
	.mbhc_moisture_config = rouleur_mbhc_moisture_config,
	.mbhc_get_moisture_status = rouleur_mbhc_get_moisture_status,
	.mbhc_moisture_detect_en = rouleur_mbhc_moisture_detect_en,
	.bcs_enable = rouleur_mbhc_bcs_enable,
	.get_micbias_val = rouleur_mbhc_get_micbias_val,
	.mbhc_comp_autozero_control = rouleur_mbhc_comp_autozero_control,
	.mbhc_surge_ctl = rouleur_mbhc_surge_control,
	.update_cross_conn_thr = rouleur_mbhc_update_cross_conn_thr,
};

static int rouleur_get_hph_type(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct rouleur_mbhc *rouleur_mbhc = rouleur_soc_get_mbhc(component);
	struct wcd_mbhc *mbhc;

	if (!rouleur_mbhc) {
		dev_err(component->dev, "%s: mbhc not initialized!\n",
			__func__);
		return -EINVAL;
	}

	mbhc = &rouleur_mbhc->wcd_mbhc;

	ucontrol->value.integer.value[0] = (u32) mbhc->hph_type;
	dev_dbg(component->dev, "%s: hph_type = %u\n", __func__,
		mbhc->hph_type);

	return 0;
}

static int rouleur_hph_impedance_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	uint32_t zl = 0, zr = 0;
	bool hphr;
	struct soc_multi_mixer_control *mc;
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct rouleur_mbhc *rouleur_mbhc = rouleur_soc_get_mbhc(component);

	if (!rouleur_mbhc) {
		dev_err(component->dev, "%s: mbhc not initialized!\n",
			__func__);
		return -EINVAL;
	}

	mc = (struct soc_multi_mixer_control *)(kcontrol->private_value);
	hphr = mc->shift;
	wcd_mbhc_get_impedance(&rouleur_mbhc->wcd_mbhc, &zl, &zr);
	dev_dbg(component->dev, "%s: zl=%u(ohms), zr=%u(ohms)\n", __func__,
		zl, zr);
	ucontrol->value.integer.value[0] = hphr ? zr : zl;

	return 0;
}

static const struct snd_kcontrol_new hph_type_detect_controls[] = {
	SOC_SINGLE_EXT("HPH Type", 0, 0, UINT_MAX, 0,
		       rouleur_get_hph_type, NULL),
};

static const struct snd_kcontrol_new impedance_detect_controls[] = {
	SOC_SINGLE_EXT("HPHL Impedance", 0, 0, UINT_MAX, 0,
		       rouleur_hph_impedance_get, NULL),
	SOC_SINGLE_EXT("HPHR Impedance", 0, 1, UINT_MAX, 0,
		       rouleur_hph_impedance_get, NULL),
};

/*
 * rouleur_mbhc_get_impedance: get impedance of headphone
 * left and right channels
 * @rouleur_mbhc: handle to struct rouleur_mbhc *
 * @zl: handle to left-ch impedance
 * @zr: handle to right-ch impedance
 * return 0 for success or error code in case of failure
 */
int rouleur_mbhc_get_impedance(struct rouleur_mbhc *rouleur_mbhc,
			     uint32_t *zl, uint32_t *zr)
{
	if (!rouleur_mbhc) {
		pr_err("%s: mbhc not initialized!\n", __func__);
		return -EINVAL;
	}
	if (!zl || !zr) {
		pr_err("%s: zl or zr null!\n", __func__);
		return -EINVAL;
	}

	return wcd_mbhc_get_impedance(&rouleur_mbhc->wcd_mbhc, zl, zr);
}
EXPORT_SYMBOL(rouleur_mbhc_get_impedance);

/*
 * rouleur_mbhc_hs_detect: starts mbhc insertion/removal functionality
 * @component: handle to snd_soc_component *
 * @mbhc_cfg: handle to mbhc configuration structure
 * return 0 if mbhc_start is success or error code in case of failure
 */
int rouleur_mbhc_hs_detect(struct snd_soc_component *component,
			 struct wcd_mbhc_config *mbhc_cfg)
{
	struct rouleur_priv *rouleur = NULL;
	struct rouleur_mbhc *rouleur_mbhc = NULL;

	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return -EINVAL;
	}

	rouleur = snd_soc_component_get_drvdata(component);
	if (!rouleur) {
		pr_err("%s: rouleur is NULL\n", __func__);
		return -EINVAL;
	}

	rouleur_mbhc = rouleur->mbhc;
	if (!rouleur_mbhc) {
		dev_err(component->dev, "%s: mbhc not initialized!\n",
			__func__);
		return -EINVAL;
	}

	return wcd_mbhc_start(&rouleur_mbhc->wcd_mbhc, mbhc_cfg);
}
EXPORT_SYMBOL(rouleur_mbhc_hs_detect);

/*
 * rouleur_mbhc_hs_detect_exit: stop mbhc insertion/removal functionality
 * @component: handle to snd_soc_component *
 */
void rouleur_mbhc_hs_detect_exit(struct snd_soc_component *component)
{
	struct rouleur_priv *rouleur = NULL;
	struct rouleur_mbhc *rouleur_mbhc = NULL;

	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return;
	}

	rouleur = snd_soc_component_get_drvdata(component);
	if (!rouleur) {
		pr_err("%s: rouleur is NULL\n", __func__);
		return;
	}

	rouleur_mbhc = rouleur->mbhc;
	if (!rouleur_mbhc) {
		dev_err(component->dev, "%s: mbhc not initialized!\n",
			__func__);
		return;
	}
	wcd_mbhc_stop(&rouleur_mbhc->wcd_mbhc);
}
EXPORT_SYMBOL(rouleur_mbhc_hs_detect_exit);

/*
 * rouleur_mbhc_ssr_down: stop mbhc during
 * rouleur subsystem restart
 * @mbhc: pointer to rouleur_mbhc structure
 * @component: handle to snd_soc_component *
 */
void rouleur_mbhc_ssr_down(struct rouleur_mbhc *mbhc,
			   struct snd_soc_component *component)
{
	struct wcd_mbhc *wcd_mbhc = NULL;

	if (!mbhc || !component)
		return;

	wcd_mbhc = &mbhc->wcd_mbhc;
	if (wcd_mbhc == NULL) {
		dev_err(component->dev, "%s: wcd_mbhc is NULL\n", __func__);
		return;
	}

	rouleur_mbhc_hs_detect_exit(component);
	wcd_mbhc_deinit(wcd_mbhc);
}
EXPORT_SYMBOL(rouleur_mbhc_ssr_down);

/*
 * rouleur_mbhc_post_ssr_init: initialize mbhc for
 * rouleur post subsystem restart
 * @mbhc: poniter to rouleur_mbhc structure
 * @component: handle to snd_soc_component *
 *
 * return 0 if mbhc_init is success or error code in case of failure
 */
int rouleur_mbhc_post_ssr_init(struct rouleur_mbhc *mbhc,
			     struct snd_soc_component *component)
{
	int ret = 0;
	struct wcd_mbhc *wcd_mbhc = NULL;

	if (!mbhc || !component)
		return -EINVAL;

	wcd_mbhc = &mbhc->wcd_mbhc;
	if (wcd_mbhc == NULL) {
		pr_err("%s: wcd_mbhc is NULL\n", __func__);
		return -EINVAL;
	}

	snd_soc_component_update_bits(component, ROULEUR_ANA_MBHC_MECH,
				0x20, 0x20);
	ret = wcd_mbhc_init(wcd_mbhc, component, &mbhc_cb, &intr_ids,
			    wcd_mbhc_registers, ROULEUR_ZDET_SUPPORTED);
	if (ret)
		dev_err(component->dev, "%s: mbhc initialization failed\n",
			__func__);

	return ret;
}
EXPORT_SYMBOL(rouleur_mbhc_post_ssr_init);

/*
 * rouleur_mbhc_init: initialize mbhc for rouleur
 * @mbhc: poniter to rouleur_mbhc struct pointer to store the configs
 * @component: handle to snd_soc_component *
 * @fw_data: handle to firmware data
 *
 * return 0 if mbhc_init is success or error code in case of failure
 */
int rouleur_mbhc_init(struct rouleur_mbhc **mbhc,
		      struct snd_soc_component *component,
		      struct fw_info *fw_data)
{
	struct rouleur_mbhc *rouleur_mbhc = NULL;
	struct wcd_mbhc *wcd_mbhc = NULL;
	struct rouleur_pdata *pdata;
	int ret = 0;

	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return -EINVAL;
	}

	rouleur_mbhc = devm_kzalloc(component->dev, sizeof(struct rouleur_mbhc),
				    GFP_KERNEL);
	if (!rouleur_mbhc)
		return -ENOMEM;

	rouleur_mbhc->fw_data = fw_data;
	BLOCKING_INIT_NOTIFIER_HEAD(&rouleur_mbhc->notifier);
	wcd_mbhc = &rouleur_mbhc->wcd_mbhc;
	if (wcd_mbhc == NULL) {
		pr_err("%s: wcd_mbhc is NULL\n", __func__);
		ret = -EINVAL;
		goto err;
	}


	/* Setting default mbhc detection logic to ADC */
	wcd_mbhc->mbhc_detection_logic = WCD_DETECTION_ADC;

	pdata = dev_get_platdata(component->dev);
	if (!pdata) {
		dev_err(component->dev, "%s: pdata pointer is NULL\n",
			__func__);
		ret = -EINVAL;
		goto err;
	}
	wcd_mbhc->micb_mv = pdata->micbias.micb2_mv;

	ret = wcd_mbhc_init(wcd_mbhc, component, &mbhc_cb,
				&intr_ids, wcd_mbhc_registers,
				ROULEUR_ZDET_SUPPORTED);
	if (ret) {
		dev_err(component->dev, "%s: mbhc initialization failed\n",
			__func__);
		goto err;
	}

	(*mbhc) = rouleur_mbhc;
	snd_soc_add_component_controls(component, impedance_detect_controls,
				   ARRAY_SIZE(impedance_detect_controls));
	snd_soc_add_component_controls(component, hph_type_detect_controls,
				   ARRAY_SIZE(hph_type_detect_controls));

	return 0;
err:
	devm_kfree(component->dev, rouleur_mbhc);
	return ret;
}
EXPORT_SYMBOL(rouleur_mbhc_init);

/*
 * rouleur_mbhc_deinit: deinitialize mbhc for rouleur
 * @component: handle to snd_soc_component *
 */
void rouleur_mbhc_deinit(struct snd_soc_component *component)
{
	struct rouleur_priv *rouleur;
	struct rouleur_mbhc *rouleur_mbhc;

	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return;
	}

	rouleur = snd_soc_component_get_drvdata(component);
	if (!rouleur) {
		pr_err("%s: rouleur is NULL\n", __func__);
		return;
	}

	rouleur_mbhc = rouleur->mbhc;
	if (rouleur_mbhc) {
		wcd_mbhc_deinit(&rouleur_mbhc->wcd_mbhc);
		devm_kfree(component->dev, rouleur_mbhc);
	}
}
EXPORT_SYMBOL(rouleur_mbhc_deinit);
