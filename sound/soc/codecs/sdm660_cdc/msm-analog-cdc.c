/* Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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
#include <linux/printk.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/qdsp6v2/apr.h>
#include <linux/workqueue.h>
#include <linux/regmap.h>
#include <linux/qdsp6v2/audio_notifier.h>
#include <sound/q6afe-v2.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include <sound/q6core.h>
#include "msm-analog-cdc.h"
#include "sdm660-cdc-irq.h"
#include "sdm660-cdc-registers.h"
#include "msm-cdc-common.h"
#include "../../msm/sdm660-common.h"
#include "../wcd-mbhc-v2.h"

#define DRV_NAME "pmic_analog_codec"
#define SDM660_CDC_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |\
			SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 |\
			SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_96000 |\
			SNDRV_PCM_RATE_192000)
#define SDM660_CDC_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
		SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S24_3LE)
#define MSM_DIG_CDC_STRING_LEN 80
#define MSM_ANLG_CDC_VERSION_ENTRY_SIZE 32

#define CODEC_DT_MAX_PROP_SIZE			40
#define MAX_ON_DEMAND_SUPPLY_NAME_LENGTH	64
#define BUS_DOWN 1

/*
 * 200 Milliseconds sufficient for DSP bring up in the lpass
 * after Sub System Restart
 */
#define ADSP_STATE_READY_TIMEOUT_MS 200

#define EAR_PMD 0
#define EAR_PMU 1
#define SPK_PMD 2
#define SPK_PMU 3

#define MICBIAS_DEFAULT_VAL 1800000
#define MICBIAS_MIN_VAL 1600000
#define MICBIAS_STEP_SIZE 50000

#define DEFAULT_BOOST_VOLTAGE 5000
#define MIN_BOOST_VOLTAGE 4000
#define MAX_BOOST_VOLTAGE 5550
#define BOOST_VOLTAGE_STEP 50

#define SDM660_CDC_MBHC_BTN_COARSE_ADJ  100 /* in mV */
#define SDM660_CDC_MBHC_BTN_FINE_ADJ 12 /* in mV */

#define VOLTAGE_CONVERTER(value, min_value, step_size)\
	((value - min_value)/step_size)

enum {
	BOOST_SWITCH = 0,
	BOOST_ALWAYS,
	BYPASS_ALWAYS,
	BOOST_ON_FOREVER,
};

static const DECLARE_TLV_DB_SCALE(analog_gain, 0, 25, 1);
static struct snd_soc_dai_driver msm_anlg_cdc_i2s_dai[];
/* By default enable the internal speaker boost */
static bool spkr_boost_en = true;

static char on_demand_supply_name[][MAX_ON_DEMAND_SUPPLY_NAME_LENGTH] = {
	"cdc-vdd-mic-bias",
};

static struct wcd_mbhc_register
	wcd_mbhc_registers[WCD_MBHC_REG_FUNC_MAX] = {
	WCD_MBHC_REGISTER("WCD_MBHC_L_DET_EN",
			  MSM89XX_PMIC_ANALOG_MBHC_DET_CTL_1, 0x80, 7, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_GND_DET_EN",
			  MSM89XX_PMIC_ANALOG_MBHC_DET_CTL_1, 0x40, 6, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_MECH_DETECTION_TYPE",
			  MSM89XX_PMIC_ANALOG_MBHC_DET_CTL_1, 0x20, 5, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_MIC_CLAMP_CTL",
			  MSM89XX_PMIC_ANALOG_MBHC_DET_CTL_1, 0x18, 3, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_ELECT_DETECTION_TYPE",
			  MSM89XX_PMIC_ANALOG_MBHC_DET_CTL_2, 0x01, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HS_L_DET_PULL_UP_CTRL",
			  MSM89XX_PMIC_ANALOG_MBHC_DET_CTL_2, 0xC0, 6, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HS_L_DET_PULL_UP_COMP_CTRL",
			  MSM89XX_PMIC_ANALOG_MBHC_DET_CTL_2, 0x20, 5, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHL_PLUG_TYPE",
			  MSM89XX_PMIC_ANALOG_MBHC_DET_CTL_2, 0x10, 4, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_GND_PLUG_TYPE",
			  MSM89XX_PMIC_ANALOG_MBHC_DET_CTL_2, 0x08, 3, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_SW_HPH_LP_100K_TO_GND",
			  MSM89XX_PMIC_ANALOG_MBHC_DET_CTL_2, 0x01, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_ELECT_SCHMT_ISRC",
			  MSM89XX_PMIC_ANALOG_MBHC_DET_CTL_2, 0x06, 1, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_FSM_EN",
			  MSM89XX_PMIC_ANALOG_MBHC_FSM_CTL, 0x80, 7, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_INSREM_DBNC",
			  MSM89XX_PMIC_ANALOG_MBHC_DBNC_TIMER, 0xF0, 4, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_BTN_DBNC",
			  MSM89XX_PMIC_ANALOG_MBHC_DBNC_TIMER, 0x0C, 2, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HS_VREF",
			  MSM89XX_PMIC_ANALOG_MBHC_BTN3_CTL, 0x03, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HS_COMP_RESULT",
			  MSM89XX_PMIC_ANALOG_MBHC_ZDET_ELECT_RESULT, 0x01,
			  0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_MIC_SCHMT_RESULT",
			  MSM89XX_PMIC_ANALOG_MBHC_ZDET_ELECT_RESULT, 0x02,
			  1, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHL_SCHMT_RESULT",
			  MSM89XX_PMIC_ANALOG_MBHC_ZDET_ELECT_RESULT, 0x08,
			  3, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHR_SCHMT_RESULT",
			  MSM89XX_PMIC_ANALOG_MBHC_ZDET_ELECT_RESULT, 0x04,
			  2, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_OCP_FSM_EN",
			  MSM89XX_PMIC_ANALOG_RX_COM_OCP_CTL, 0x10, 4, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_BTN_RESULT",
			  MSM89XX_PMIC_ANALOG_MBHC_BTN_RESULT, 0xFF, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_BTN_ISRC_CTL",
			  MSM89XX_PMIC_ANALOG_MBHC_FSM_CTL, 0x70, 4, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_ELECT_RESULT",
			  MSM89XX_PMIC_ANALOG_MBHC_ZDET_ELECT_RESULT, 0xFF,
			  0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_MICB_CTRL",
			  MSM89XX_PMIC_ANALOG_MICB_2_EN, 0xC0, 6, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPH_CNP_WG_TIME",
			  MSM89XX_PMIC_ANALOG_RX_HPH_CNP_WG_TIME, 0xFC, 2, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHR_PA_EN",
			  MSM89XX_PMIC_ANALOG_RX_HPH_CNP_EN, 0x10, 4, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHL_PA_EN",
			  MSM89XX_PMIC_ANALOG_RX_HPH_CNP_EN, 0x20, 5, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPH_PA_EN",
			  MSM89XX_PMIC_ANALOG_RX_HPH_CNP_EN, 0x30, 4, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_SWCH_LEVEL_REMOVE",
			  MSM89XX_PMIC_ANALOG_MBHC_ZDET_ELECT_RESULT,
			  0x10, 4, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_PULLDOWN_CTRL",
			  MSM89XX_PMIC_ANALOG_MICB_2_EN, 0x20, 5, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_ANC_DET_EN", 0, 0, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_FSM_STATUS", 0, 0, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_MUX_CTL", 0, 0, 0, 0),
};

/* Multiply gain_adj and offset by 1000 and 100 to avoid float arithmetic */
static const struct wcd_imped_i_ref imped_i_ref[] = {
	{I_h4_UA, 8, 800, 9000, 10000},
	{I_pt5_UA, 10, 100, 990, 4600},
	{I_14_UA, 17, 14, 1050, 700},
	{I_l4_UA, 10, 4, 1165, 110},
	{I_1_UA, 0, 1, 1200, 65},
};

static const struct wcd_mbhc_intr intr_ids = {
	.mbhc_sw_intr =  MSM89XX_IRQ_MBHC_HS_DET,
	.mbhc_btn_press_intr = MSM89XX_IRQ_MBHC_PRESS,
	.mbhc_btn_release_intr = MSM89XX_IRQ_MBHC_RELEASE,
	.mbhc_hs_ins_intr = MSM89XX_IRQ_MBHC_INSREM_DET1,
	.mbhc_hs_rem_intr = MSM89XX_IRQ_MBHC_INSREM_DET,
	.hph_left_ocp = MSM89XX_IRQ_HPHL_OCP,
	.hph_right_ocp = MSM89XX_IRQ_HPHR_OCP,
};

static int msm_anlg_cdc_dt_parse_vreg_info(struct device *dev,
					   struct sdm660_cdc_regulator *vreg,
					   const char *vreg_name,
					   bool ondemand);
static struct sdm660_cdc_pdata *msm_anlg_cdc_populate_dt_pdata(
						struct device *dev);
static int msm_anlg_cdc_enable_ext_mb_source(struct wcd_mbhc *wcd_mbhc,
					     bool turn_on);
static void msm_anlg_cdc_trim_btn_reg(struct snd_soc_codec *codec);
static void msm_anlg_cdc_set_micb_v(struct snd_soc_codec *codec);
static void msm_anlg_cdc_set_boost_v(struct snd_soc_codec *codec);
static void msm_anlg_cdc_set_auto_zeroing(struct snd_soc_codec *codec,
					  bool enable);
static void msm_anlg_cdc_configure_cap(struct snd_soc_codec *codec,
				       bool micbias1, bool micbias2);
static bool msm_anlg_cdc_use_mb(struct snd_soc_codec *codec);

static int get_codec_version(struct sdm660_cdc_priv *sdm660_cdc)
{
	if (sdm660_cdc->codec_version == DRAX_CDC)
		return DRAX_CDC;
	else if (sdm660_cdc->codec_version == DIANGU)
		return DIANGU;
	else if (sdm660_cdc->codec_version == CAJON_2_0)
		return CAJON_2_0;
	else if (sdm660_cdc->codec_version == CAJON)
		return CAJON;
	else if (sdm660_cdc->codec_version == CONGA)
		return CONGA;
	else if (sdm660_cdc->pmic_rev == TOMBAK_2_0)
		return TOMBAK_2_0;
	else if (sdm660_cdc->pmic_rev == TOMBAK_1_0)
		return TOMBAK_1_0;

	pr_err("%s: unsupported codec version\n", __func__);
	return UNSUPPORTED;
}

static void wcd_mbhc_meas_imped(struct snd_soc_codec *codec,
				s16 *impedance_l, s16 *impedance_r)
{
	struct sdm660_cdc_priv *sdm660_cdc =
					snd_soc_codec_get_drvdata(codec);

	if ((sdm660_cdc->imped_det_pin == WCD_MBHC_DET_BOTH) ||
	    (sdm660_cdc->imped_det_pin == WCD_MBHC_DET_HPHL)) {
		/* Enable ZDET_L_MEAS_EN */
		snd_soc_update_bits(codec,
				MSM89XX_PMIC_ANALOG_MBHC_FSM_CTL,
				0x08, 0x08);
		/* Wait for 2ms for measurement to complete */
		usleep_range(2000, 2100);
		/* Read Left impedance value from Result1 */
		*impedance_l = snd_soc_read(codec,
				MSM89XX_PMIC_ANALOG_MBHC_BTN_RESULT);
		/* Enable ZDET_R_MEAS_EN */
		snd_soc_update_bits(codec,
				MSM89XX_PMIC_ANALOG_MBHC_FSM_CTL,
				0x08, 0x00);
	}
	if ((sdm660_cdc->imped_det_pin == WCD_MBHC_DET_BOTH) ||
	    (sdm660_cdc->imped_det_pin == WCD_MBHC_DET_HPHR)) {
		snd_soc_update_bits(codec,
				MSM89XX_PMIC_ANALOG_MBHC_FSM_CTL,
				0x04, 0x04);
		/* Wait for 2ms for measurement to complete */
		usleep_range(2000, 2100);
		/* Read Right impedance value from Result1 */
		*impedance_r = snd_soc_read(codec,
				MSM89XX_PMIC_ANALOG_MBHC_BTN_RESULT);
		snd_soc_update_bits(codec,
				MSM89XX_PMIC_ANALOG_MBHC_FSM_CTL,
				0x04, 0x00);
	}
}

static void msm_anlg_cdc_set_ref_current(struct snd_soc_codec *codec,
					 enum wcd_curr_ref curr_ref)
{
	struct sdm660_cdc_priv *sdm660_cdc =
					snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s: curr_ref: %d\n", __func__, curr_ref);

	if (get_codec_version(sdm660_cdc) < CAJON)
		dev_dbg(codec->dev, "%s: Setting ref current not required\n",
			__func__);

	sdm660_cdc->imped_i_ref = imped_i_ref[curr_ref];

	switch (curr_ref) {
	case I_h4_UA:
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_MICB_2_EN,
			0x07, 0x01);
		break;
	case I_pt5_UA:
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_MICB_2_EN,
			0x07, 0x04);
		break;
	case I_14_UA:
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_MICB_2_EN,
			0x07, 0x03);
		break;
	case I_l4_UA:
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_MICB_2_EN,
			0x07, 0x01);
		break;
	case I_1_UA:
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_MICB_2_EN,
			0x07, 0x00);
		break;
	default:
		pr_debug("%s: No ref current set\n", __func__);
		break;
	}
}

static bool msm_anlg_cdc_adj_ref_current(struct snd_soc_codec *codec,
					 s16 *impedance_l, s16 *impedance_r)
{
	int i = 2;
	s16 compare_imp = 0;
	struct sdm660_cdc_priv *sdm660_cdc =
					snd_soc_codec_get_drvdata(codec);

	if (sdm660_cdc->imped_det_pin == WCD_MBHC_DET_HPHR)
		compare_imp = *impedance_r;
	else
		compare_imp = *impedance_l;

	if (get_codec_version(sdm660_cdc) < CAJON) {
		dev_dbg(codec->dev,
			"%s: Reference current adjustment not required\n",
			 __func__);
		return false;
	}

	while (compare_imp < imped_i_ref[i].min_val) {
		msm_anlg_cdc_set_ref_current(codec, imped_i_ref[++i].curr_ref);
		wcd_mbhc_meas_imped(codec, impedance_l, impedance_r);
		compare_imp = (sdm660_cdc->imped_det_pin ==
			       WCD_MBHC_DET_HPHR) ? *impedance_r : *impedance_l;
		if (i >= I_1_UA)
			break;
	}
	return true;
}

void msm_anlg_cdc_spk_ext_pa_cb(
		int (*codec_spk_ext_pa)(struct snd_soc_codec *codec,
			int enable), struct snd_soc_codec *codec)
{
	struct sdm660_cdc_priv *sdm660_cdc;

	if (!codec) {
		pr_err("%s: NULL codec pointer!\n", __func__);
		return;
	}

	sdm660_cdc = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s: Enter\n", __func__);
	sdm660_cdc->codec_spk_ext_pa_cb = codec_spk_ext_pa;
}

static void msm_anlg_cdc_compute_impedance(struct snd_soc_codec *codec, s16 l,
					   s16 r, uint32_t *zl, uint32_t *zr,
					   bool high)
{
	struct sdm660_cdc_priv *sdm660_cdc =
					snd_soc_codec_get_drvdata(codec);
	uint32_t rl = 0, rr = 0;
	struct wcd_imped_i_ref R = sdm660_cdc->imped_i_ref;
	int codec_ver = get_codec_version(sdm660_cdc);

	switch (codec_ver) {
	case TOMBAK_1_0:
	case TOMBAK_2_0:
	case CONGA:
		if (high) {
			dev_dbg(codec->dev,
				"%s: This plug has high range impedance\n",
				 __func__);
			rl = (uint32_t)(((100 * (l * 400 - 200))/96) - 230);
			rr = (uint32_t)(((100 * (r * 400 - 200))/96) - 230);
		} else {
			dev_dbg(codec->dev,
				"%s: This plug has low range impedance\n",
				 __func__);
			rl = (uint32_t)(((1000 * (l * 2 - 1))/1165) - (13/10));
			rr = (uint32_t)(((1000 * (r * 2 - 1))/1165) - (13/10));
		}
		break;
	case CAJON:
	case CAJON_2_0:
	case DIANGU:
	case DRAX_CDC:
		if (sdm660_cdc->imped_det_pin == WCD_MBHC_DET_HPHL) {
			rr = (uint32_t)(((DEFAULT_MULTIPLIER * (10 * r - 5)) -
			   (DEFAULT_OFFSET * DEFAULT_GAIN))/DEFAULT_GAIN);
			rl = (uint32_t)(((10000 * (R.multiplier * (10 * l - 5)))
			      - R.offset * R.gain_adj)/(R.gain_adj * 100));
		} else if (sdm660_cdc->imped_det_pin == WCD_MBHC_DET_HPHR) {
			rr = (uint32_t)(((10000 * (R.multiplier * (10 * r - 5)))
			      - R.offset * R.gain_adj)/(R.gain_adj * 100));
			rl = (uint32_t)(((DEFAULT_MULTIPLIER * (10 * l - 5))-
			   (DEFAULT_OFFSET * DEFAULT_GAIN))/DEFAULT_GAIN);
		} else if (sdm660_cdc->imped_det_pin == WCD_MBHC_DET_NONE) {
			rr = (uint32_t)(((DEFAULT_MULTIPLIER * (10 * r - 5)) -
			   (DEFAULT_OFFSET * DEFAULT_GAIN))/DEFAULT_GAIN);
			rl = (uint32_t)(((DEFAULT_MULTIPLIER * (10 * l - 5))-
			   (DEFAULT_OFFSET * DEFAULT_GAIN))/DEFAULT_GAIN);
		} else {
			rr = (uint32_t)(((10000 * (R.multiplier * (10 * r - 5)))
			      - R.offset * R.gain_adj)/(R.gain_adj * 100));
			rl = (uint32_t)(((10000 * (R.multiplier * (10 * l - 5)))
			      - R.offset * R.gain_adj)/(R.gain_adj * 100));
		}
		break;
	default:
		dev_dbg(codec->dev, "%s: No codec mentioned\n", __func__);
		break;
	}
	*zl = rl;
	*zr = rr;
}

static struct firmware_cal *msm_anlg_cdc_get_hwdep_fw_cal(
		struct wcd_mbhc *wcd_mbhc,
		enum wcd_cal_type type)
{
	struct sdm660_cdc_priv *sdm660_cdc;
	struct firmware_cal *hwdep_cal;
	struct snd_soc_codec *codec = wcd_mbhc->codec;

	if (!codec) {
		pr_err("%s: NULL codec pointer\n", __func__);
		return NULL;
	}
	sdm660_cdc = snd_soc_codec_get_drvdata(codec);
	hwdep_cal = wcdcal_get_fw_cal(sdm660_cdc->fw_data, type);
	if (!hwdep_cal) {
		dev_err(codec->dev, "%s: cal not sent by %d\n",
				__func__, type);
		return NULL;
	}
	return hwdep_cal;
}

static void wcd9xxx_spmi_irq_control(struct snd_soc_codec *codec,
				     int irq, bool enable)
{
	if (enable)
		wcd9xxx_spmi_enable_irq(irq);
	else
		wcd9xxx_spmi_disable_irq(irq);
}

static void msm_anlg_cdc_mbhc_clk_setup(struct snd_soc_codec *codec,
					bool enable)
{
	if (enable)
		snd_soc_update_bits(codec,
				MSM89XX_PMIC_DIGITAL_CDC_DIG_CLK_CTL,
				0x08, 0x08);
	else
		snd_soc_update_bits(codec,
				MSM89XX_PMIC_DIGITAL_CDC_DIG_CLK_CTL,
				0x08, 0x00);
}

static int msm_anlg_cdc_mbhc_map_btn_code_to_num(struct snd_soc_codec *codec)
{
	int btn_code;
	int btn;

	btn_code = snd_soc_read(codec, MSM89XX_PMIC_ANALOG_MBHC_BTN_RESULT);

	switch (btn_code) {
	case 0:
		btn = 0;
		break;
	case 1:
		btn = 1;
		break;
	case 3:
		btn = 2;
		break;
	case 7:
		btn = 3;
		break;
	case 15:
		btn = 4;
		break;
	default:
		btn = -EINVAL;
		break;
	};

	return btn;
}

static bool msm_anlg_cdc_spmi_lock_sleep(struct wcd_mbhc *mbhc, bool lock)
{
	if (lock)
		return wcd9xxx_spmi_lock_sleep();
	wcd9xxx_spmi_unlock_sleep();
	return 0;
}

static bool msm_anlg_cdc_micb_en_status(struct wcd_mbhc *mbhc, int micb_num)
{
	if (micb_num == MIC_BIAS_1)
		return (snd_soc_read(mbhc->codec,
				     MSM89XX_PMIC_ANALOG_MICB_1_EN) &
			0x80);
	if (micb_num == MIC_BIAS_2)
		return (snd_soc_read(mbhc->codec,
				     MSM89XX_PMIC_ANALOG_MICB_2_EN) &
			0x80);
	return false;
}

static void msm_anlg_cdc_enable_master_bias(struct snd_soc_codec *codec,
					    bool enable)
{
	if (enable)
		snd_soc_update_bits(codec, MSM89XX_PMIC_ANALOG_MASTER_BIAS_CTL,
				    0x30, 0x30);
	else
		snd_soc_update_bits(codec, MSM89XX_PMIC_ANALOG_MASTER_BIAS_CTL,
				    0x30, 0x00);
}

static void msm_anlg_cdc_mbhc_common_micb_ctrl(struct snd_soc_codec *codec,
					       int event, bool enable)
{
	u16 reg;
	u8 mask;
	u8 val;

	switch (event) {
	case MBHC_COMMON_MICB_PRECHARGE:
		reg = MSM89XX_PMIC_ANALOG_MICB_1_CTL;
		mask = 0x60;
		val = (enable ? 0x60 : 0x00);
		break;
	case MBHC_COMMON_MICB_SET_VAL:
		reg = MSM89XX_PMIC_ANALOG_MICB_1_VAL;
		mask = 0xFF;
		val = (enable ? 0xC0 : 0x00);
		break;
	case MBHC_COMMON_MICB_TAIL_CURR:
		reg = MSM89XX_PMIC_ANALOG_MICB_1_EN;
		mask = 0x04;
		val = (enable ? 0x04 : 0x00);
		break;
	default:
		dev_err(codec->dev,
			"%s: Invalid event received\n", __func__);
		return;
	};
	snd_soc_update_bits(codec, reg, mask, val);
}

static void msm_anlg_cdc_mbhc_internal_micbias_ctrl(struct snd_soc_codec *codec,
						    int micbias_num,
						    bool enable)
{
	if (micbias_num == 1) {
		if (enable)
			snd_soc_update_bits(codec,
				MSM89XX_PMIC_ANALOG_MICB_1_INT_RBIAS,
				0x10, 0x10);
		else
			snd_soc_update_bits(codec,
				MSM89XX_PMIC_ANALOG_MICB_1_INT_RBIAS,
				0x10, 0x00);
	}
}

static bool msm_anlg_cdc_mbhc_hph_pa_on_status(struct snd_soc_codec *codec)
{
	return (snd_soc_read(codec, MSM89XX_PMIC_ANALOG_RX_HPH_CNP_EN) &
		0x30) ? true : false;
}

static void msm_anlg_cdc_mbhc_program_btn_thr(struct snd_soc_codec *codec,
					      s16 *btn_low, s16 *btn_high,
					      int num_btn, bool is_micbias)
{
	int i;
	u32 course, fine, reg_val;
	u16 reg_addr = MSM89XX_PMIC_ANALOG_MBHC_BTN0_ZDETL_CTL;
	s16 *btn_voltage;

	btn_voltage = ((is_micbias) ? btn_high : btn_low);

	for (i = 0; i <  num_btn; i++) {
		course = (btn_voltage[i] / SDM660_CDC_MBHC_BTN_COARSE_ADJ);
		fine = ((btn_voltage[i] % SDM660_CDC_MBHC_BTN_COARSE_ADJ) /
				SDM660_CDC_MBHC_BTN_FINE_ADJ);

		reg_val = (course << 5) | (fine << 2);
		snd_soc_update_bits(codec, reg_addr, 0xFC, reg_val);
		dev_dbg(codec->dev,
			"%s: course: %d fine: %d reg_addr: %x reg_val: %x\n",
			  __func__, course, fine, reg_addr, reg_val);
		reg_addr++;
	}
}

static void msm_anlg_cdc_mbhc_calc_impedance(struct wcd_mbhc *mbhc,
					     uint32_t *zl, uint32_t *zr)
{
	struct snd_soc_codec *codec = mbhc->codec;
	struct sdm660_cdc_priv *sdm660_cdc =
					snd_soc_codec_get_drvdata(codec);
	s16 impedance_l, impedance_r;
	s16 impedance_l_fixed;
	s16 reg0, reg1, reg2, reg3, reg4;
	bool high = false;
	bool min_range_used =  false;

	WCD_MBHC_RSC_ASSERT_LOCKED(mbhc);
	reg0 = snd_soc_read(codec, MSM89XX_PMIC_ANALOG_MBHC_DBNC_TIMER);
	reg1 = snd_soc_read(codec, MSM89XX_PMIC_ANALOG_MBHC_BTN2_ZDETH_CTL);
	reg2 = snd_soc_read(codec, MSM89XX_PMIC_ANALOG_MBHC_DET_CTL_2);
	reg3 = snd_soc_read(codec, MSM89XX_PMIC_ANALOG_MICB_2_EN);
	reg4 = snd_soc_read(codec, MSM89XX_PMIC_ANALOG_MBHC_FSM_CTL);

	sdm660_cdc->imped_det_pin = WCD_MBHC_DET_BOTH;
	mbhc->hph_type = WCD_MBHC_HPH_NONE;

	/* disable FSM and micbias and enable pullup*/
	snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_MBHC_FSM_CTL,
			0x80, 0x00);
	snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_MICB_2_EN,
			0xA5, 0x25);
	/*
	 * Enable legacy electrical detection current sources
	 * and disable fast ramp and enable manual switching
	 * of extra capacitance
	 */
	dev_dbg(codec->dev, "%s: Setup for impedance det\n", __func__);

	msm_anlg_cdc_set_ref_current(codec, I_h4_UA);

	snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_MBHC_DET_CTL_2,
			0x06, 0x02);
	snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_MBHC_DBNC_TIMER,
			0x02, 0x02);
	snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_MBHC_BTN2_ZDETH_CTL,
			0x02, 0x00);

	dev_dbg(codec->dev, "%s: Start performing impedance detection\n",
		 __func__);

	wcd_mbhc_meas_imped(codec, &impedance_l, &impedance_r);

	if (impedance_l > 2 || impedance_r > 2) {
		high = true;
		if (!mbhc->mbhc_cfg->mono_stero_detection) {
			/* Set ZDET_CHG to 0  to discharge ramp */
			snd_soc_update_bits(codec,
					MSM89XX_PMIC_ANALOG_MBHC_FSM_CTL,
					0x02, 0x00);
			/* wait 40ms for the discharge ramp to complete */
			usleep_range(40000, 40100);
			snd_soc_update_bits(codec,
				MSM89XX_PMIC_ANALOG_MBHC_BTN0_ZDETL_CTL,
				0x03, 0x00);
			sdm660_cdc->imped_det_pin = (impedance_l > 2 &&
						      impedance_r > 2) ?
						      WCD_MBHC_DET_NONE :
						      ((impedance_l > 2) ?
						      WCD_MBHC_DET_HPHR :
						      WCD_MBHC_DET_HPHL);
			if (sdm660_cdc->imped_det_pin == WCD_MBHC_DET_NONE)
				goto exit;
		} else {
			if (get_codec_version(sdm660_cdc) >= CAJON) {
				if (impedance_l == 63 && impedance_r == 63) {
					dev_dbg(codec->dev,
						"%s: HPHL and HPHR are floating\n",
						 __func__);
					sdm660_cdc->imped_det_pin =
							WCD_MBHC_DET_NONE;
					mbhc->hph_type = WCD_MBHC_HPH_NONE;
				} else if (impedance_l == 63
					   && impedance_r < 63) {
					dev_dbg(codec->dev,
						"%s: Mono HS with HPHL floating\n",
						 __func__);
					sdm660_cdc->imped_det_pin =
							WCD_MBHC_DET_HPHR;
					mbhc->hph_type = WCD_MBHC_HPH_MONO;
				} else if (impedance_r == 63 &&
					   impedance_l < 63) {
					dev_dbg(codec->dev,
						"%s: Mono HS with HPHR floating\n",
						 __func__);
					sdm660_cdc->imped_det_pin =
							WCD_MBHC_DET_HPHL;
					mbhc->hph_type = WCD_MBHC_HPH_MONO;
				} else if (impedance_l > 3 && impedance_r > 3 &&
					(impedance_l == impedance_r)) {
					snd_soc_update_bits(codec,
					MSM89XX_PMIC_ANALOG_MBHC_DET_CTL_2,
					0x06, 0x06);
					wcd_mbhc_meas_imped(codec, &impedance_l,
							    &impedance_r);
					if (impedance_r == impedance_l)
						dev_dbg(codec->dev,
							"%s: Mono Headset\n",
							__func__);
						sdm660_cdc->imped_det_pin =
							WCD_MBHC_DET_NONE;
						mbhc->hph_type =
							WCD_MBHC_HPH_MONO;
				} else {
					dev_dbg(codec->dev,
						"%s: STEREO headset is found\n",
						 __func__);
					sdm660_cdc->imped_det_pin =
							WCD_MBHC_DET_BOTH;
					mbhc->hph_type = WCD_MBHC_HPH_STEREO;
				}
			}
		}
	}

	msm_anlg_cdc_set_ref_current(codec, I_pt5_UA);
	msm_anlg_cdc_set_ref_current(codec, I_14_UA);

	/* Enable RAMP_L , RAMP_R & ZDET_CHG*/
	snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_MBHC_BTN0_ZDETL_CTL,
			0x03, 0x03);
	snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_MBHC_FSM_CTL,
			0x02, 0x02);
	/* wait for 50msec for the HW to apply ramp on HPHL and HPHR */
	usleep_range(50000, 50100);
	/* Enable ZDET_DISCHG_CAP_CTL  to add extra capacitance */
	snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_MBHC_FSM_CTL,
			0x01, 0x01);
	/* wait for 5msec for the voltage to get stable */
	usleep_range(5000, 5100);

	wcd_mbhc_meas_imped(codec, &impedance_l, &impedance_r);

	min_range_used = msm_anlg_cdc_adj_ref_current(codec,
						&impedance_l, &impedance_r);
	if (!mbhc->mbhc_cfg->mono_stero_detection) {
		/* Set ZDET_CHG to 0  to discharge ramp */
		snd_soc_update_bits(codec,
				MSM89XX_PMIC_ANALOG_MBHC_FSM_CTL,
				0x02, 0x00);
		/* wait for 40msec for the capacitor to discharge */
		usleep_range(40000, 40100);
		snd_soc_update_bits(codec,
				MSM89XX_PMIC_ANALOG_MBHC_BTN0_ZDETL_CTL,
				0x03, 0x00);
		goto exit;
	}

	/* we are setting ref current to the minimun range or the measured
	 * value larger than the minimum value, so min_range_used is true.
	 * If the headset is mono headset with either HPHL or HPHR floating
	 * then we have already done the mono stereo detection and do not
	 * need to continue further.
	 */

	if (!min_range_used ||
	    sdm660_cdc->imped_det_pin == WCD_MBHC_DET_HPHL ||
	    sdm660_cdc->imped_det_pin == WCD_MBHC_DET_HPHR)
		goto exit;


	/* Disable Set ZDET_CONN_RAMP_L and enable ZDET_CONN_FIXED_L */
	snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_MBHC_BTN0_ZDETL_CTL,
			0x02, 0x00);
	snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_MBHC_BTN1_ZDETM_CTL,
			0x02, 0x02);
	/* Set ZDET_CHG to 0  */
	snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_MBHC_FSM_CTL,
			0x02, 0x00);
	/* wait for 40msec for the capacitor to discharge */
	usleep_range(40000, 40100);

	/* Set ZDET_CONN_RAMP_R to 0  */
	snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_MBHC_BTN0_ZDETL_CTL,
			0x01, 0x00);
	/* Enable ZDET_L_MEAS_EN */
	snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_MBHC_FSM_CTL,
			0x08, 0x08);
	/* wait for 2msec for the HW to compute left inpedance value */
	usleep_range(2000, 2100);
	/* Read Left impedance value from Result1 */
	impedance_l_fixed = snd_soc_read(codec,
			MSM89XX_PMIC_ANALOG_MBHC_BTN_RESULT);
	/* Disable ZDET_L_MEAS_EN */
	snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_MBHC_FSM_CTL,
			0x08, 0x00);
	/*
	 * Assume impedance_l is L1, impedance_l_fixed is L2.
	 * If the following condition is met, we can take this
	 * headset as mono one with impedance of L2.
	 * Otherwise, take it as stereo with impedance of L1.
	 * Condition:
	 * abs[(L2-0.5L1)/(L2+0.5L1)] < abs [(L2-L1)/(L2+L1)]
	 */
	if ((abs(impedance_l_fixed - impedance_l/2) *
		(impedance_l_fixed + impedance_l)) >=
		(abs(impedance_l_fixed - impedance_l) *
		(impedance_l_fixed + impedance_l/2))) {
		dev_dbg(codec->dev,
			"%s: STEREO plug type detected\n",
			 __func__);
		mbhc->hph_type = WCD_MBHC_HPH_STEREO;
	} else {
		dev_dbg(codec->dev,
			"%s: MONO plug type detected\n",
			__func__);
		mbhc->hph_type = WCD_MBHC_HPH_MONO;
		impedance_l = impedance_l_fixed;
	}
	/* Enable ZDET_CHG  */
	snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_MBHC_FSM_CTL,
			0x02, 0x02);
	/* wait for 10msec for the capacitor to charge */
	usleep_range(10000, 10100);
	snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_MBHC_BTN0_ZDETL_CTL,
			0x02, 0x02);
	snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_MBHC_BTN1_ZDETM_CTL,
			0x02, 0x00);
	/* Set ZDET_CHG to 0  to discharge HPHL */
	snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_MBHC_FSM_CTL,
			0x02, 0x00);
	/* wait for 40msec for the capacitor to discharge */
	usleep_range(40000, 40100);
	snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_MBHC_BTN0_ZDETL_CTL,
			0x02, 0x00);

exit:
	snd_soc_write(codec, MSM89XX_PMIC_ANALOG_MBHC_FSM_CTL, reg4);
	snd_soc_write(codec, MSM89XX_PMIC_ANALOG_MICB_2_EN, reg3);
	snd_soc_write(codec, MSM89XX_PMIC_ANALOG_MBHC_BTN2_ZDETH_CTL, reg1);
	snd_soc_write(codec, MSM89XX_PMIC_ANALOG_MBHC_DBNC_TIMER, reg0);
	snd_soc_write(codec, MSM89XX_PMIC_ANALOG_MBHC_DET_CTL_2, reg2);
	msm_anlg_cdc_compute_impedance(codec, impedance_l, impedance_r,
				      zl, zr, high);

	dev_dbg(codec->dev, "%s: RL %d ohm, RR %d ohm\n", __func__, *zl, *zr);
	dev_dbg(codec->dev, "%s: Impedance detection completed\n", __func__);
}

static int msm_anlg_cdc_dig_register_notifier(void *handle,
					      struct notifier_block *nblock,
					      bool enable)
{
	struct sdm660_cdc_priv *handle_cdc = handle;

	if (enable)
		return blocking_notifier_chain_register(&handle_cdc->notifier,
							nblock);

	return blocking_notifier_chain_unregister(&handle_cdc->notifier,
						  nblock);
}

static int msm_anlg_cdc_mbhc_register_notifier(struct wcd_mbhc *wcd_mbhc,
					       struct notifier_block *nblock,
					       bool enable)
{
	struct snd_soc_codec *codec = wcd_mbhc->codec;
	struct sdm660_cdc_priv *sdm660_cdc =
					snd_soc_codec_get_drvdata(codec);

	if (enable)
		return blocking_notifier_chain_register(
						&sdm660_cdc->notifier_mbhc,
						nblock);

	return blocking_notifier_chain_unregister(&sdm660_cdc->notifier_mbhc,
						  nblock);
}

static int msm_anlg_cdc_request_irq(struct snd_soc_codec *codec,
				    int irq, irq_handler_t handler,
				    const char *name, void *data)
{
	return wcd9xxx_spmi_request_irq(irq, handler, name, data);
}

static int msm_anlg_cdc_free_irq(struct snd_soc_codec *codec,
				 int irq, void *data)
{
	return wcd9xxx_spmi_free_irq(irq, data);
}

static const struct wcd_mbhc_cb mbhc_cb = {
	.enable_mb_source = msm_anlg_cdc_enable_ext_mb_source,
	.trim_btn_reg = msm_anlg_cdc_trim_btn_reg,
	.compute_impedance = msm_anlg_cdc_mbhc_calc_impedance,
	.set_micbias_value = msm_anlg_cdc_set_micb_v,
	.set_auto_zeroing = msm_anlg_cdc_set_auto_zeroing,
	.get_hwdep_fw_cal = msm_anlg_cdc_get_hwdep_fw_cal,
	.set_cap_mode = msm_anlg_cdc_configure_cap,
	.register_notifier = msm_anlg_cdc_mbhc_register_notifier,
	.request_irq = msm_anlg_cdc_request_irq,
	.irq_control = wcd9xxx_spmi_irq_control,
	.free_irq = msm_anlg_cdc_free_irq,
	.clk_setup = msm_anlg_cdc_mbhc_clk_setup,
	.map_btn_code_to_num = msm_anlg_cdc_mbhc_map_btn_code_to_num,
	.lock_sleep = msm_anlg_cdc_spmi_lock_sleep,
	.micbias_enable_status = msm_anlg_cdc_micb_en_status,
	.mbhc_bias = msm_anlg_cdc_enable_master_bias,
	.mbhc_common_micb_ctrl = msm_anlg_cdc_mbhc_common_micb_ctrl,
	.micb_internal = msm_anlg_cdc_mbhc_internal_micbias_ctrl,
	.hph_pa_on_status = msm_anlg_cdc_mbhc_hph_pa_on_status,
	.set_btn_thr = msm_anlg_cdc_mbhc_program_btn_thr,
	.extn_use_mb = msm_anlg_cdc_use_mb,
};

static const uint32_t wcd_imped_val[] = {4, 8, 12, 13, 16,
					20, 24, 28, 32,
					36, 40, 44, 48};

static void msm_anlg_cdc_dig_notifier_call(struct snd_soc_codec *codec,
					const enum dig_cdc_notify_event event)
{
	struct sdm660_cdc_priv *sdm660_cdc = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s: notifier call event %d\n", __func__, event);
	blocking_notifier_call_chain(&sdm660_cdc->notifier,
				     event, NULL);
}

static void msm_anlg_cdc_notifier_call(struct snd_soc_codec *codec,
				       const enum wcd_notify_event event)
{
	struct sdm660_cdc_priv *sdm660_cdc =
					snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s: notifier call event %d\n", __func__, event);
	blocking_notifier_call_chain(&sdm660_cdc->notifier_mbhc, event,
				     &sdm660_cdc->mbhc);
}

static void msm_anlg_cdc_boost_on(struct snd_soc_codec *codec)
{
	struct sdm660_cdc_priv *sdm660_cdc =
					snd_soc_codec_get_drvdata(codec);

	snd_soc_update_bits(codec,
			MSM89XX_PMIC_DIGITAL_PERPH_RESET_CTL3, 0x0F, 0x0F);
	snd_soc_write(codec, MSM89XX_PMIC_ANALOG_SEC_ACCESS, 0xA5);
	snd_soc_write(codec, MSM89XX_PMIC_ANALOG_PERPH_RESET_CTL3, 0x0F);
	snd_soc_write(codec, MSM89XX_PMIC_ANALOG_MASTER_BIAS_CTL, 0x30);
	if (get_codec_version(sdm660_cdc) < CAJON_2_0)
		snd_soc_write(codec, MSM89XX_PMIC_ANALOG_CURRENT_LIMIT, 0x82);
	else
		snd_soc_write(codec, MSM89XX_PMIC_ANALOG_CURRENT_LIMIT, 0xA2);
	snd_soc_update_bits(codec, MSM89XX_PMIC_ANALOG_SPKR_DRV_CTL,
			    0x69, 0x69);
	snd_soc_update_bits(codec, MSM89XX_PMIC_ANALOG_SPKR_DRV_DBG,
			    0x01, 0x01);
	snd_soc_update_bits(codec, MSM89XX_PMIC_ANALOG_SLOPE_COMP_IP_ZERO,
			    0x88, 0x88);
	snd_soc_update_bits(codec, MSM89XX_PMIC_ANALOG_SPKR_DAC_CTL,
			    0x03, 0x03);
	snd_soc_update_bits(codec, MSM89XX_PMIC_ANALOG_SPKR_OCP_CTL,
			    0xE1, 0xE1);
	if (get_codec_version(sdm660_cdc) < CAJON_2_0) {
		snd_soc_update_bits(codec, MSM89XX_PMIC_DIGITAL_CDC_DIG_CLK_CTL,
				    0x20, 0x20);
		/* Wait for 1ms after clock ctl enable */
		usleep_range(CODEC_DELAY_1_MS, CODEC_DELAY_1_1_MS);
		snd_soc_update_bits(codec, MSM89XX_PMIC_ANALOG_BOOST_EN_CTL,
				    0xDF, 0xDF);
		usleep_range(CODEC_DELAY_1_MS, CODEC_DELAY_1_1_MS);
	} else {
		snd_soc_update_bits(codec, MSM89XX_PMIC_ANALOG_BOOST_EN_CTL,
				    0x40, 0x00);
		snd_soc_update_bits(codec, MSM89XX_PMIC_DIGITAL_CDC_DIG_CLK_CTL,
				    0x20, 0x20);
		snd_soc_update_bits(codec, MSM89XX_PMIC_ANALOG_BOOST_EN_CTL,
				    0x80, 0x80);
		/* Wait for 500us after BOOST_EN to happen */
		usleep_range(500, 510);
		snd_soc_update_bits(codec, MSM89XX_PMIC_ANALOG_BOOST_EN_CTL,
				    0x40, 0x40);
		/* Wait for 500us after BOOST pulse_skip */
		usleep_range(500, 510);
	}
}

static void msm_anlg_cdc_boost_off(struct snd_soc_codec *codec)
{
	snd_soc_update_bits(codec, MSM89XX_PMIC_ANALOG_BOOST_EN_CTL,
			    0xDF, 0x5F);
	snd_soc_update_bits(codec, MSM89XX_PMIC_DIGITAL_CDC_DIG_CLK_CTL,
			    0x20, 0x00);
}

static void msm_anlg_cdc_bypass_on(struct snd_soc_codec *codec)
{
	struct sdm660_cdc_priv *sdm660_cdc =
					snd_soc_codec_get_drvdata(codec);

	if (get_codec_version(sdm660_cdc) < CAJON_2_0) {
		snd_soc_write(codec,
			MSM89XX_PMIC_ANALOG_SEC_ACCESS,
			0xA5);
		snd_soc_write(codec,
			MSM89XX_PMIC_ANALOG_PERPH_RESET_CTL3,
			0x07);
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_BYPASS_MODE,
			0x02, 0x02);
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_BYPASS_MODE,
			0x01, 0x00);
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_BYPASS_MODE,
			0x40, 0x40);
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_BYPASS_MODE,
			0x80, 0x80);
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_BOOST_EN_CTL,
			0xDF, 0xDF);
	} else {
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_DIGITAL_CDC_DIG_CLK_CTL,
			0x20, 0x20);
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_BYPASS_MODE,
			0x20, 0x20);
	}
}

static void msm_anlg_cdc_bypass_off(struct snd_soc_codec *codec)
{
	struct sdm660_cdc_priv *sdm660_cdc =
					snd_soc_codec_get_drvdata(codec);

	if (get_codec_version(sdm660_cdc) < CAJON_2_0) {
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_BOOST_EN_CTL,
			0x80, 0x00);
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_BYPASS_MODE,
			0x80, 0x00);
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_BYPASS_MODE,
			0x02, 0x00);
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_BYPASS_MODE,
			0x40, 0x00);
	} else {
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_BYPASS_MODE,
			0x20, 0x00);
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_DIGITAL_CDC_DIG_CLK_CTL,
			0x20, 0x00);
	}
}

static void msm_anlg_cdc_boost_mode_sequence(struct snd_soc_codec *codec,
					     int flag)
{
	struct sdm660_cdc_priv *sdm660_cdc =
					snd_soc_codec_get_drvdata(codec);

	if (flag == EAR_PMU) {
		switch (sdm660_cdc->boost_option) {
		case BOOST_SWITCH:
			if (sdm660_cdc->ear_pa_boost_set) {
				msm_anlg_cdc_boost_off(codec);
				msm_anlg_cdc_bypass_on(codec);
			}
			break;
		case BOOST_ALWAYS:
			msm_anlg_cdc_boost_on(codec);
			break;
		case BYPASS_ALWAYS:
			msm_anlg_cdc_bypass_on(codec);
			break;
		case BOOST_ON_FOREVER:
			msm_anlg_cdc_boost_on(codec);
			break;
		default:
			dev_err(codec->dev,
				"%s: invalid boost option: %d\n", __func__,
				sdm660_cdc->boost_option);
			break;
		}
	} else if (flag == EAR_PMD) {
		switch (sdm660_cdc->boost_option) {
		case BOOST_SWITCH:
			if (sdm660_cdc->ear_pa_boost_set)
				msm_anlg_cdc_bypass_off(codec);
			break;
		case BOOST_ALWAYS:
			msm_anlg_cdc_boost_off(codec);
			/* 80ms for EAR boost to settle down */
			msleep(80);
			break;
		case BYPASS_ALWAYS:
			/* nothing to do as bypass on always */
			break;
		case BOOST_ON_FOREVER:
			/* nothing to do as boost on forever */
			break;
		default:
			dev_err(codec->dev,
				"%s: invalid boost option: %d\n", __func__,
				sdm660_cdc->boost_option);
			break;
		}
	} else if (flag == SPK_PMU) {
		switch (sdm660_cdc->boost_option) {
		case BOOST_SWITCH:
			if (sdm660_cdc->spk_boost_set) {
				msm_anlg_cdc_bypass_off(codec);
				msm_anlg_cdc_boost_on(codec);
			}
			break;
		case BOOST_ALWAYS:
			msm_anlg_cdc_boost_on(codec);
			break;
		case BYPASS_ALWAYS:
			msm_anlg_cdc_bypass_on(codec);
			break;
		case BOOST_ON_FOREVER:
			msm_anlg_cdc_boost_on(codec);
			break;
		default:
			dev_err(codec->dev,
				"%s: invalid boost option: %d\n", __func__,
				sdm660_cdc->boost_option);
			break;
		}
	} else if (flag == SPK_PMD) {
		switch (sdm660_cdc->boost_option) {
		case BOOST_SWITCH:
			if (sdm660_cdc->spk_boost_set) {
				msm_anlg_cdc_boost_off(codec);
				/*
				 * Add 40 ms sleep for the spk
				 * boost to settle down
				 */
				msleep(40);
			}
			break;
		case BOOST_ALWAYS:
			msm_anlg_cdc_boost_off(codec);
			/*
			 * Add 40 ms sleep for the spk
			 * boost to settle down
			 */
			msleep(40);
			break;
		case BYPASS_ALWAYS:
			/* nothing to do as bypass on always */
			break;
		case BOOST_ON_FOREVER:
			/* nothing to do as boost on forever */
			break;
		default:
			dev_err(codec->dev,
				"%s: invalid boost option: %d\n", __func__,
				sdm660_cdc->boost_option);
			break;
		}
	}
}

static int msm_anlg_cdc_dt_parse_vreg_info(struct device *dev,
	struct sdm660_cdc_regulator *vreg, const char *vreg_name,
	bool ondemand)
{
	int len, ret = 0;
	const __be32 *prop;
	char prop_name[CODEC_DT_MAX_PROP_SIZE];
	struct device_node *regnode = NULL;
	u32 prop_val;

	snprintf(prop_name, CODEC_DT_MAX_PROP_SIZE, "%s-supply",
		vreg_name);
	regnode = of_parse_phandle(dev->of_node, prop_name, 0);

	if (!regnode) {
		dev_err(dev, "Looking up %s property in node %s failed\n",
			prop_name, dev->of_node->full_name);
		return -ENODEV;
	}

	dev_dbg(dev, "Looking up %s property in node %s\n",
		prop_name, dev->of_node->full_name);

	vreg->name = vreg_name;
	vreg->ondemand = ondemand;

	snprintf(prop_name, CODEC_DT_MAX_PROP_SIZE,
		"qcom,%s-voltage", vreg_name);
	prop = of_get_property(dev->of_node, prop_name, &len);

	if (!prop || (len != (2 * sizeof(__be32)))) {
		dev_err(dev, "%s %s property\n",
			prop ? "invalid format" : "no", prop_name);
		return -EINVAL;
	}
	vreg->min_uv = be32_to_cpup(&prop[0]);
	vreg->max_uv = be32_to_cpup(&prop[1]);

	snprintf(prop_name, CODEC_DT_MAX_PROP_SIZE,
		"qcom,%s-current", vreg_name);

	ret = of_property_read_u32(dev->of_node, prop_name, &prop_val);
	if (ret) {
		dev_err(dev, "Looking up %s property in node %s failed",
			prop_name, dev->of_node->full_name);
		return -EFAULT;
	}
	vreg->optimum_ua = prop_val;

	dev_dbg(dev, "%s: vol=[%d %d]uV, curr=[%d]uA, ond %d\n\n", vreg->name,
		 vreg->min_uv, vreg->max_uv, vreg->optimum_ua, vreg->ondemand);
	return 0;
}

static void msm_anlg_cdc_dt_parse_boost_info(struct snd_soc_codec *codec)
{
	struct sdm660_cdc_priv *sdm660_cdc_priv =
		snd_soc_codec_get_drvdata(codec);
	const char *prop_name = "qcom,cdc-boost-voltage";
	int boost_voltage, ret;

	ret = of_property_read_u32(codec->dev->of_node, prop_name,
			&boost_voltage);
	if (ret) {
		dev_dbg(codec->dev, "Looking up %s property in node %s failed\n",
			prop_name, codec->dev->of_node->full_name);
		boost_voltage = DEFAULT_BOOST_VOLTAGE;
	}
	if (boost_voltage < MIN_BOOST_VOLTAGE ||
			boost_voltage > MAX_BOOST_VOLTAGE) {
		dev_err(codec->dev,
				"Incorrect boost voltage. Reverting to default\n");
		boost_voltage = DEFAULT_BOOST_VOLTAGE;
	}

	sdm660_cdc_priv->boost_voltage =
		VOLTAGE_CONVERTER(boost_voltage, MIN_BOOST_VOLTAGE,
				BOOST_VOLTAGE_STEP);
	dev_dbg(codec->dev, "Boost voltage value is: %d\n",
			boost_voltage);
}

static void msm_anlg_cdc_dt_parse_micbias_info(struct device *dev,
				struct wcd_micbias_setting *micbias)
{
	const char *prop_name = "qcom,cdc-micbias-cfilt-mv";
	int ret;

	ret = of_property_read_u32(dev->of_node, prop_name,
			&micbias->cfilt1_mv);
	if (ret) {
		dev_dbg(dev, "Looking up %s property in node %s failed",
			prop_name, dev->of_node->full_name);
		micbias->cfilt1_mv = MICBIAS_DEFAULT_VAL;
	}
}

static struct sdm660_cdc_pdata *msm_anlg_cdc_populate_dt_pdata(
						struct device *dev)
{
	struct sdm660_cdc_pdata *pdata;
	int ret, static_cnt, ond_cnt, idx, i;
	const char *name = NULL;
	const char *static_prop_name = "qcom,cdc-static-supplies";
	const char *ond_prop_name = "qcom,cdc-on-demand-supplies";

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return NULL;

	static_cnt = of_property_count_strings(dev->of_node, static_prop_name);
	if (IS_ERR_VALUE(static_cnt)) {
		dev_err(dev, "%s: Failed to get static supplies %d\n", __func__,
			static_cnt);
		ret = -EINVAL;
		goto err;
	}

	/* On-demand supply list is an optional property */
	ond_cnt = of_property_count_strings(dev->of_node, ond_prop_name);
	if (IS_ERR_VALUE(ond_cnt))
		ond_cnt = 0;

	WARN_ON(static_cnt <= 0 || ond_cnt < 0);
	if ((static_cnt + ond_cnt) > ARRAY_SIZE(pdata->regulator)) {
		dev_err(dev, "%s: Num of supplies %u > max supported %zd\n",
				__func__, (static_cnt + ond_cnt),
					ARRAY_SIZE(pdata->regulator));
		ret = -EINVAL;
		goto err;
	}

	for (idx = 0; idx < static_cnt; idx++) {
		ret = of_property_read_string_index(dev->of_node,
						    static_prop_name, idx,
						    &name);
		if (ret) {
			dev_err(dev, "%s: of read string %s idx %d error %d\n",
				__func__, static_prop_name, idx, ret);
			goto err;
		}

		dev_dbg(dev, "%s: Found static cdc supply %s\n", __func__,
			name);
		ret = msm_anlg_cdc_dt_parse_vreg_info(dev,
						&pdata->regulator[idx],
						name, false);
		if (ret) {
			dev_err(dev, "%s:err parsing vreg for %s idx %d\n",
				__func__, name, idx);
			goto err;
		}
	}

	for (i = 0; i < ond_cnt; i++, idx++) {
		ret = of_property_read_string_index(dev->of_node, ond_prop_name,
						    i, &name);
		if (ret) {
			dev_err(dev, "%s: err parsing on_demand for %s idx %d\n",
				__func__, ond_prop_name, i);
			goto err;
		}

		dev_dbg(dev, "%s: Found on-demand cdc supply %s\n", __func__,
			name);
		ret = msm_anlg_cdc_dt_parse_vreg_info(dev,
						&pdata->regulator[idx],
						name, true);
		if (ret) {
			dev_err(dev, "%s: err parsing vreg on_demand for %s idx %d\n",
				__func__, name, idx);
			goto err;
		}
	}
	msm_anlg_cdc_dt_parse_micbias_info(dev, &pdata->micbias);

	return pdata;
err:
	devm_kfree(dev, pdata);
	dev_err(dev, "%s: Failed to populate DT data ret = %d\n",
		__func__, ret);
	return NULL;
}

static int msm_anlg_cdc_codec_enable_on_demand_supply(
		struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	int ret = 0;
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct sdm660_cdc_priv *sdm660_cdc =
					snd_soc_codec_get_drvdata(codec);
	struct on_demand_supply *supply;

	if (w->shift >= ON_DEMAND_SUPPLIES_MAX) {
		dev_err(codec->dev, "%s: error index > MAX Demand supplies",
			__func__);
		ret = -EINVAL;
		goto out;
	}
	dev_dbg(codec->dev, "%s: supply: %s event: %d ref: %d\n",
		__func__, on_demand_supply_name[w->shift], event,
		atomic_read(&sdm660_cdc->on_demand_list[w->shift].ref));

	supply = &sdm660_cdc->on_demand_list[w->shift];
	WARN_ONCE(!supply->supply, "%s isn't defined\n",
		  on_demand_supply_name[w->shift]);
	if (!supply->supply) {
		dev_err(codec->dev, "%s: err supply not present ond for %d",
			__func__, w->shift);
		goto out;
	}
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (atomic_inc_return(&supply->ref) == 1) {
			ret = regulator_set_voltage(supply->supply,
						    supply->min_uv,
						    supply->max_uv);
			if (ret) {
				dev_err(codec->dev,
					"Setting regulator voltage(en) for micbias with err = %d\n",
					ret);
				goto out;
			}
			ret = regulator_set_load(supply->supply,
						 supply->optimum_ua);
			if (ret < 0) {
				dev_err(codec->dev,
					"Setting regulator optimum mode(en) failed for micbias with err = %d\n",
					ret);
				goto out;
			}
			ret = regulator_enable(supply->supply);
		}
		if (ret)
			dev_err(codec->dev, "%s: Failed to enable %s\n",
				__func__,
				on_demand_supply_name[w->shift]);
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (atomic_read(&supply->ref) == 0) {
			dev_dbg(codec->dev, "%s: %s supply has been disabled.\n",
				 __func__, on_demand_supply_name[w->shift]);
			goto out;
		}
		if (atomic_dec_return(&supply->ref) == 0) {
			ret = regulator_disable(supply->supply);
			if (ret)
				dev_err(codec->dev, "%s: Failed to disable %s\n",
					__func__,
					on_demand_supply_name[w->shift]);
			ret = regulator_set_voltage(supply->supply,
						    0,
						    supply->max_uv);
			if (ret) {
				dev_err(codec->dev,
					"Setting regulator voltage(dis) failed for micbias with err = %d\n",
					ret);
				goto out;
			}
			ret = regulator_set_load(supply->supply, 0);
			if (ret < 0)
				dev_err(codec->dev,
					"Setting regulator optimum mode(dis) failed for micbias with err = %d\n",
					ret);
		}
		break;
	default:
		break;
	}
out:
	return ret;
}

static int msm_anlg_cdc_codec_enable_clock_block(struct snd_soc_codec *codec,
						 int enable)
{
	struct msm_asoc_mach_data *pdata = NULL;

	pdata = snd_soc_card_get_drvdata(codec->component.card);
	if (enable) {
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_MASTER_BIAS_CTL, 0x30, 0x30);
		msm_anlg_cdc_dig_notifier_call(codec, DIG_CDC_EVENT_CLK_ON);
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_DIGITAL_CDC_RST_CTL, 0x80, 0x80);
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_DIGITAL_CDC_TOP_CLK_CTL, 0x0C, 0x0C);
	} else {
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_DIGITAL_CDC_TOP_CLK_CTL, 0x0C, 0x00);
	}
	return 0;
}

static int msm_anlg_cdc_codec_enable_charge_pump(struct snd_soc_dapm_widget *w,
						 struct snd_kcontrol *kcontrol,
						 int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct sdm660_cdc_priv *sdm660_cdc =
					snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s: event = %d\n", __func__, event);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		msm_anlg_cdc_codec_enable_clock_block(codec, 1);
		if (!(strcmp(w->name, "EAR CP"))) {
			snd_soc_update_bits(codec,
					MSM89XX_PMIC_DIGITAL_CDC_DIG_CLK_CTL,
					0x80, 0x80);
			msm_anlg_cdc_boost_mode_sequence(codec, EAR_PMU);
		} else if (get_codec_version(sdm660_cdc) >= DIANGU) {
			snd_soc_update_bits(codec,
					MSM89XX_PMIC_DIGITAL_CDC_DIG_CLK_CTL,
					0x80, 0x80);
		} else {
			snd_soc_update_bits(codec,
					MSM89XX_PMIC_DIGITAL_CDC_DIG_CLK_CTL,
					0xC0, 0xC0);
		}
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* Wait for 1ms post powerup of chargepump */
		usleep_range(CODEC_DELAY_1_MS, CODEC_DELAY_1_1_MS);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* Wait for 1ms post powerdown of chargepump */
		usleep_range(CODEC_DELAY_1_MS, CODEC_DELAY_1_1_MS);
		if (!(strcmp(w->name, "EAR CP"))) {
			snd_soc_update_bits(codec,
					MSM89XX_PMIC_DIGITAL_CDC_DIG_CLK_CTL,
					0x80, 0x00);
			if (sdm660_cdc->boost_option != BOOST_ALWAYS) {
				dev_dbg(codec->dev,
					"%s: boost_option:%d, tear down ear\n",
					__func__, sdm660_cdc->boost_option);
				msm_anlg_cdc_boost_mode_sequence(codec,
								 EAR_PMD);
			}
			/*
			 * Reset pa select bit from ear to hph after ear pa
			 * is disabled and HPH DAC disable to reduce ear
			 * turn off pop and avoid HPH pop in concurrency
			 */
			snd_soc_update_bits(codec,
				MSM89XX_PMIC_ANALOG_RX_EAR_CTL, 0x80, 0x00);
		} else {
			if (get_codec_version(sdm660_cdc) < DIANGU)
				snd_soc_update_bits(codec,
					MSM89XX_PMIC_DIGITAL_CDC_DIG_CLK_CTL,
					0x40, 0x00);
			if (sdm660_cdc->rx_bias_count == 0)
				snd_soc_update_bits(codec,
					MSM89XX_PMIC_DIGITAL_CDC_DIG_CLK_CTL,
					0x80, 0x00);
			dev_dbg(codec->dev, "%s: rx_bias_count = %d\n",
					__func__, sdm660_cdc->rx_bias_count);
		}
		break;
	}
	return 0;
}

static int msm_anlg_cdc_ear_pa_boost_get(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sdm660_cdc_priv *sdm660_cdc =
					snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] =
		(sdm660_cdc->ear_pa_boost_set ? 1 : 0);
	dev_dbg(codec->dev, "%s: sdm660_cdc->ear_pa_boost_set = %d\n",
			__func__, sdm660_cdc->ear_pa_boost_set);
	return 0;
}

static int msm_anlg_cdc_ear_pa_boost_set(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sdm660_cdc_priv *sdm660_cdc =
					snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s: ucontrol->value.integer.value[0] = %ld\n",
		__func__, ucontrol->value.integer.value[0]);
	sdm660_cdc->ear_pa_boost_set =
		(ucontrol->value.integer.value[0] ? true : false);
	return 0;
}

static int msm_anlg_cdc_loopback_mode_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct msm_asoc_mach_data *pdata = NULL;

	pdata = snd_soc_card_get_drvdata(codec->component.card);
	dev_dbg(codec->dev, "%s: ucontrol->value.integer.value[0] = %ld\n",
		__func__, ucontrol->value.integer.value[0]);

	return pdata->lb_mode;
}

static int msm_anlg_cdc_loopback_mode_put(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct msm_asoc_mach_data *pdata = NULL;

	pdata = snd_soc_card_get_drvdata(codec->component.card);
	dev_dbg(codec->dev, "%s: ucontrol->value.integer.value[0] = %ld\n",
		__func__, ucontrol->value.integer.value[0]);

	switch (ucontrol->value.integer.value[0]) {
	case 0:
		pdata->lb_mode = false;
		break;
	case 1:
		pdata->lb_mode = true;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int msm_anlg_cdc_pa_gain_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	u8 ear_pa_gain;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sdm660_cdc_priv *sdm660_cdc =
					snd_soc_codec_get_drvdata(codec);

	if (get_codec_version(sdm660_cdc) >= DIANGU) {
		ear_pa_gain = snd_soc_read(codec,
					MSM89XX_PMIC_ANALOG_RX_COM_BIAS_DAC);
		ear_pa_gain = (ear_pa_gain >> 1) & 0x3;

		if (ear_pa_gain == 0x00) {
			ucontrol->value.integer.value[0] = 3;
		} else if (ear_pa_gain == 0x01) {
			ucontrol->value.integer.value[1] = 2;
		} else if (ear_pa_gain == 0x02) {
			ucontrol->value.integer.value[2] = 1;
		} else if (ear_pa_gain == 0x03) {
			ucontrol->value.integer.value[3] = 0;
		} else {
			dev_err(codec->dev,
				"%s: ERROR: Unsupported Ear Gain = 0x%x\n",
				__func__, ear_pa_gain);
			return -EINVAL;
		}
	} else {
		ear_pa_gain = snd_soc_read(codec,
					   MSM89XX_PMIC_ANALOG_RX_EAR_CTL);
		ear_pa_gain = (ear_pa_gain >> 5) & 0x1;
		if (ear_pa_gain == 0x00) {
			ucontrol->value.integer.value[0] = 0;
		} else if (ear_pa_gain == 0x01) {
			ucontrol->value.integer.value[0] = 3;
		} else  {
			dev_err(codec->dev,
				"%s: ERROR: Unsupported Ear Gain = 0x%x\n",
				__func__, ear_pa_gain);
			return -EINVAL;
		}
	}
	ucontrol->value.integer.value[0] = ear_pa_gain;
	dev_dbg(codec->dev, "%s: ear_pa_gain = 0x%x\n", __func__, ear_pa_gain);
	return 0;
}

static int msm_anlg_cdc_pa_gain_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	u8 ear_pa_gain;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sdm660_cdc_priv *sdm660_cdc =
					snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s: ucontrol->value.integer.value[0] = %ld\n",
		__func__, ucontrol->value.integer.value[0]);

	if (get_codec_version(sdm660_cdc) >= DIANGU) {
		switch (ucontrol->value.integer.value[0]) {
		case 0:
			ear_pa_gain = 0x06;
			break;
		case 1:
			ear_pa_gain = 0x04;
			break;
		case 2:
			ear_pa_gain = 0x02;
			break;
		case 3:
			ear_pa_gain = 0x00;
			break;
		default:
			return -EINVAL;
		}
		snd_soc_update_bits(codec, MSM89XX_PMIC_ANALOG_RX_COM_BIAS_DAC,
			    0x06, ear_pa_gain);
	} else {
		switch (ucontrol->value.integer.value[0]) {
		case 0:
			ear_pa_gain = 0x00;
			break;
		case 3:
			ear_pa_gain = 0x20;
			break;
		case 1:
		case 2:
		default:
			return -EINVAL;
		}
		snd_soc_update_bits(codec, MSM89XX_PMIC_ANALOG_RX_EAR_CTL,
			    0x20, ear_pa_gain);
	}
	return 0;
}

static int msm_anlg_cdc_hph_mode_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sdm660_cdc_priv *sdm660_cdc =
					snd_soc_codec_get_drvdata(codec);

	if (sdm660_cdc->hph_mode == NORMAL_MODE) {
		ucontrol->value.integer.value[0] = 0;
	} else if (sdm660_cdc->hph_mode == HD2_MODE) {
		ucontrol->value.integer.value[0] = 1;
	} else  {
		dev_err(codec->dev, "%s: ERROR: Default HPH Mode= %d\n",
			__func__, sdm660_cdc->hph_mode);
	}

	dev_dbg(codec->dev, "%s: sdm660_cdc->hph_mode = %d\n", __func__,
			sdm660_cdc->hph_mode);
	return 0;
}

static int msm_anlg_cdc_hph_mode_set(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sdm660_cdc_priv *sdm660_cdc =
					snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s: ucontrol->value.integer.value[0] = %ld\n",
		__func__, ucontrol->value.integer.value[0]);

	switch (ucontrol->value.integer.value[0]) {
	case 0:
		sdm660_cdc->hph_mode = NORMAL_MODE;
		break;
	case 1:
		if (get_codec_version(sdm660_cdc) >= DIANGU)
			sdm660_cdc->hph_mode = HD2_MODE;
		break;
	default:
		sdm660_cdc->hph_mode = NORMAL_MODE;
		break;
	}
	dev_dbg(codec->dev, "%s: sdm660_cdc->hph_mode_set = %d\n",
		__func__, sdm660_cdc->hph_mode);
	return 0;
}

static int msm_anlg_cdc_boost_option_get(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sdm660_cdc_priv *sdm660_cdc =
					snd_soc_codec_get_drvdata(codec);

	if (sdm660_cdc->boost_option == BOOST_SWITCH) {
		ucontrol->value.integer.value[0] = 0;
	} else if (sdm660_cdc->boost_option == BOOST_ALWAYS) {
		ucontrol->value.integer.value[0] = 1;
	} else if (sdm660_cdc->boost_option == BYPASS_ALWAYS) {
		ucontrol->value.integer.value[0] = 2;
	} else if (sdm660_cdc->boost_option == BOOST_ON_FOREVER) {
		ucontrol->value.integer.value[0] = 3;
	} else  {
		dev_err(codec->dev, "%s: ERROR: Unsupported Boost option= %d\n",
			__func__, sdm660_cdc->boost_option);
		return -EINVAL;
	}

	dev_dbg(codec->dev, "%s: sdm660_cdc->boost_option = %d\n", __func__,
			sdm660_cdc->boost_option);
	return 0;
}

static int msm_anlg_cdc_boost_option_set(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sdm660_cdc_priv *sdm660_cdc =
					snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s: ucontrol->value.integer.value[0] = %ld\n",
		__func__, ucontrol->value.integer.value[0]);

	switch (ucontrol->value.integer.value[0]) {
	case 0:
		sdm660_cdc->boost_option = BOOST_SWITCH;
		break;
	case 1:
		sdm660_cdc->boost_option = BOOST_ALWAYS;
		break;
	case 2:
		sdm660_cdc->boost_option = BYPASS_ALWAYS;
		msm_anlg_cdc_bypass_on(codec);
		break;
	case 3:
		sdm660_cdc->boost_option = BOOST_ON_FOREVER;
		msm_anlg_cdc_boost_on(codec);
		break;
	default:
		pr_err("%s: invalid boost option: %d\n", __func__,
					sdm660_cdc->boost_option);
		return -EINVAL;
	}
	dev_dbg(codec->dev, "%s: sdm660_cdc->boost_option_set = %d\n",
		__func__, sdm660_cdc->boost_option);
	return 0;
}

static int msm_anlg_cdc_spk_boost_get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sdm660_cdc_priv *sdm660_cdc =
					snd_soc_codec_get_drvdata(codec);

	if (sdm660_cdc->spk_boost_set == false) {
		ucontrol->value.integer.value[0] = 0;
	} else if (sdm660_cdc->spk_boost_set == true) {
		ucontrol->value.integer.value[0] = 1;
	} else  {
		dev_err(codec->dev, "%s: ERROR: Unsupported Speaker Boost = %d\n",
				__func__, sdm660_cdc->spk_boost_set);
		return -EINVAL;
	}

	dev_dbg(codec->dev, "%s: sdm660_cdc->spk_boost_set = %d\n", __func__,
			sdm660_cdc->spk_boost_set);
	return 0;
}

static int msm_anlg_cdc_spk_boost_set(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sdm660_cdc_priv *sdm660_cdc =
					snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s: ucontrol->value.integer.value[0] = %ld\n",
			__func__, ucontrol->value.integer.value[0]);

	switch (ucontrol->value.integer.value[0]) {
	case 0:
		sdm660_cdc->spk_boost_set = false;
		break;
	case 1:
		sdm660_cdc->spk_boost_set = true;
		break;
	default:
		return -EINVAL;
	}
	dev_dbg(codec->dev, "%s: sdm660_cdc->spk_boost_set = %d\n",
		__func__, sdm660_cdc->spk_boost_set);
	return 0;
}

static int msm_anlg_cdc_ext_spk_boost_get(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sdm660_cdc_priv *sdm660_cdc =
					snd_soc_codec_get_drvdata(codec);

	if (sdm660_cdc->ext_spk_boost_set == false)
		ucontrol->value.integer.value[0] = 0;
	else
		ucontrol->value.integer.value[0] = 1;

	dev_dbg(codec->dev, "%s: sdm660_cdc->ext_spk_boost_set = %d\n",
				__func__, sdm660_cdc->ext_spk_boost_set);
	return 0;
}

static int msm_anlg_cdc_ext_spk_boost_set(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sdm660_cdc_priv *sdm660_cdc =
					snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s: ucontrol->value.integer.value[0] = %ld\n",
		__func__, ucontrol->value.integer.value[0]);

	switch (ucontrol->value.integer.value[0]) {
	case 0:
		sdm660_cdc->ext_spk_boost_set = false;
		break;
	case 1:
		sdm660_cdc->ext_spk_boost_set = true;
		break;
	default:
		return -EINVAL;
	}
	dev_dbg(codec->dev, "%s: sdm660_cdc->spk_boost_set = %d\n",
		__func__, sdm660_cdc->spk_boost_set);
	return 0;
}


static const char * const msm_anlg_cdc_loopback_mode_ctrl_text[] = {
		"DISABLE", "ENABLE"};
static const struct soc_enum msm_anlg_cdc_loopback_mode_ctl_enum[] = {
		SOC_ENUM_SINGLE_EXT(2, msm_anlg_cdc_loopback_mode_ctrl_text),
};

static const char * const msm_anlg_cdc_ear_pa_boost_ctrl_text[] = {
		"DISABLE", "ENABLE"};
static const struct soc_enum msm_anlg_cdc_ear_pa_boost_ctl_enum[] = {
		SOC_ENUM_SINGLE_EXT(2, msm_anlg_cdc_ear_pa_boost_ctrl_text),
};

static const char * const msm_anlg_cdc_ear_pa_gain_text[] = {
		"POS_1P5_DB", "POS_3_DB", "POS_4P5_DB", "POS_6_DB"};
static const struct soc_enum msm_anlg_cdc_ear_pa_gain_enum[] = {
		SOC_ENUM_SINGLE_EXT(4, msm_anlg_cdc_ear_pa_gain_text),
};

static const char * const msm_anlg_cdc_boost_option_ctrl_text[] = {
		"BOOST_SWITCH", "BOOST_ALWAYS", "BYPASS_ALWAYS",
		"BOOST_ON_FOREVER"};
static const struct soc_enum msm_anlg_cdc_boost_option_ctl_enum[] = {
		SOC_ENUM_SINGLE_EXT(4, msm_anlg_cdc_boost_option_ctrl_text),
};
static const char * const msm_anlg_cdc_spk_boost_ctrl_text[] = {
		"DISABLE", "ENABLE"};
static const struct soc_enum msm_anlg_cdc_spk_boost_ctl_enum[] = {
		SOC_ENUM_SINGLE_EXT(2, msm_anlg_cdc_spk_boost_ctrl_text),
};

static const char * const msm_anlg_cdc_ext_spk_boost_ctrl_text[] = {
		"DISABLE", "ENABLE"};
static const struct soc_enum msm_anlg_cdc_ext_spk_boost_ctl_enum[] = {
		SOC_ENUM_SINGLE_EXT(2, msm_anlg_cdc_ext_spk_boost_ctrl_text),
};

static const char * const msm_anlg_cdc_hph_mode_ctrl_text[] = {
		"NORMAL", "HD2"};
static const struct soc_enum msm_anlg_cdc_hph_mode_ctl_enum[] = {
		SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(msm_anlg_cdc_hph_mode_ctrl_text),
			msm_anlg_cdc_hph_mode_ctrl_text),
};

/*cut of frequency for high pass filter*/
static const char * const cf_text[] = {
	"MIN_3DB_4Hz", "MIN_3DB_75Hz", "MIN_3DB_150Hz"
};


static const struct snd_kcontrol_new msm_anlg_cdc_snd_controls[] = {

	SOC_ENUM_EXT("RX HPH Mode", msm_anlg_cdc_hph_mode_ctl_enum[0],
		msm_anlg_cdc_hph_mode_get, msm_anlg_cdc_hph_mode_set),

	SOC_ENUM_EXT("Boost Option", msm_anlg_cdc_boost_option_ctl_enum[0],
		msm_anlg_cdc_boost_option_get, msm_anlg_cdc_boost_option_set),

	SOC_ENUM_EXT("EAR PA Boost", msm_anlg_cdc_ear_pa_boost_ctl_enum[0],
		msm_anlg_cdc_ear_pa_boost_get, msm_anlg_cdc_ear_pa_boost_set),

	SOC_ENUM_EXT("EAR PA Gain", msm_anlg_cdc_ear_pa_gain_enum[0],
		msm_anlg_cdc_pa_gain_get, msm_anlg_cdc_pa_gain_put),

	SOC_ENUM_EXT("Speaker Boost", msm_anlg_cdc_spk_boost_ctl_enum[0],
		msm_anlg_cdc_spk_boost_get, msm_anlg_cdc_spk_boost_set),

	SOC_ENUM_EXT("Ext Spk Boost", msm_anlg_cdc_ext_spk_boost_ctl_enum[0],
		msm_anlg_cdc_ext_spk_boost_get, msm_anlg_cdc_ext_spk_boost_set),

	SOC_ENUM_EXT("LOOPBACK Mode", msm_anlg_cdc_loopback_mode_ctl_enum[0],
		msm_anlg_cdc_loopback_mode_get, msm_anlg_cdc_loopback_mode_put),
	SOC_SINGLE_TLV("ADC1 Volume", MSM89XX_PMIC_ANALOG_TX_1_EN, 3,
					8, 0, analog_gain),
	SOC_SINGLE_TLV("ADC2 Volume", MSM89XX_PMIC_ANALOG_TX_2_EN, 3,
					8, 0, analog_gain),
	SOC_SINGLE_TLV("ADC3 Volume", MSM89XX_PMIC_ANALOG_TX_3_EN, 3,
					8, 0, analog_gain),


};

static int tombak_hph_impedance_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	int ret;
	uint32_t zl, zr;
	bool hphr;
	struct soc_multi_mixer_control *mc;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sdm660_cdc_priv *priv = snd_soc_codec_get_drvdata(codec);

	mc = (struct soc_multi_mixer_control *)(kcontrol->private_value);

	hphr = mc->shift;
	ret = wcd_mbhc_get_impedance(&priv->mbhc, &zl, &zr);
	if (ret)
		dev_dbg(codec->dev, "%s: Failed to get mbhc imped", __func__);
	dev_dbg(codec->dev, "%s: zl %u, zr %u\n", __func__, zl, zr);
	ucontrol->value.integer.value[0] = hphr ? zr : zl;

	return 0;
}

static const struct snd_kcontrol_new impedance_detect_controls[] = {
	SOC_SINGLE_EXT("HPHL Impedance", 0, 0, UINT_MAX, 0,
			tombak_hph_impedance_get, NULL),
	SOC_SINGLE_EXT("HPHR Impedance", 0, 1, UINT_MAX, 0,
			tombak_hph_impedance_get, NULL),
};

static int tombak_get_hph_type(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct sdm660_cdc_priv *priv = snd_soc_codec_get_drvdata(codec);
	struct wcd_mbhc *mbhc;

	if (!priv) {
		dev_err(codec->dev,
			"%s: sdm660_cdc-wcd private data is NULL\n",
			 __func__);
		return -EINVAL;
	}

	mbhc = &priv->mbhc;
	if (!mbhc) {
		dev_err(codec->dev, "%s: mbhc not initialized\n", __func__);
		return -EINVAL;
	}

	ucontrol->value.integer.value[0] = (u32) mbhc->hph_type;
	dev_dbg(codec->dev, "%s: hph_type = %u\n", __func__, mbhc->hph_type);

	return 0;
}

static const struct snd_kcontrol_new hph_type_detect_controls[] = {
	SOC_SINGLE_EXT("HPH Type", 0, 0, UINT_MAX, 0,
	tombak_get_hph_type, NULL),
};

static const char * const rdac2_mux_text[] = {
	"ZERO", "RX2", "RX1"
};

static const struct snd_kcontrol_new adc1_switch =
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0);

static const struct soc_enum rdac2_mux_enum =
	SOC_ENUM_SINGLE(MSM89XX_PMIC_DIGITAL_CDC_CONN_HPHR_DAC_CTL,
		0, 3, rdac2_mux_text);

static const char * const adc2_mux_text[] = {
	"ZERO", "INP2", "INP3"
};

static const char * const ext_spk_text[] = {
	"Off", "On"
};

static const char * const wsa_spk_text[] = {
	"ZERO", "WSA"
};

static const struct soc_enum adc2_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0,
		ARRAY_SIZE(adc2_mux_text), adc2_mux_text);

static const struct soc_enum ext_spk_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0,
		ARRAY_SIZE(ext_spk_text), ext_spk_text);

static const struct soc_enum wsa_spk_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0,
		ARRAY_SIZE(wsa_spk_text), wsa_spk_text);



static const struct snd_kcontrol_new ext_spk_mux =
	SOC_DAPM_ENUM("Ext Spk Switch Mux", ext_spk_enum);



static const struct snd_kcontrol_new tx_adc2_mux =
	SOC_DAPM_ENUM("ADC2 MUX Mux", adc2_enum);


static const struct snd_kcontrol_new rdac2_mux =
	SOC_DAPM_ENUM("RDAC2 MUX Mux", rdac2_mux_enum);

static const char * const ear_text[] = {
	"ZERO", "Switch",
};

static const struct soc_enum ear_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(ear_text), ear_text);

static const struct snd_kcontrol_new ear_pa_mux[] = {
	SOC_DAPM_ENUM("EAR_S", ear_enum)
};

static const struct snd_kcontrol_new wsa_spk_mux[] = {
	SOC_DAPM_ENUM("WSA Spk Switch", wsa_spk_enum)
};



static const char * const hph_text[] = {
	"ZERO", "Switch",
};

static const struct soc_enum hph_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(hph_text), hph_text);

static const struct snd_kcontrol_new hphl_mux[] = {
	SOC_DAPM_ENUM("HPHL", hph_enum)
};

static const struct snd_kcontrol_new hphr_mux[] = {
	SOC_DAPM_ENUM("HPHR", hph_enum)
};

static const struct snd_kcontrol_new spkr_mux[] = {
	SOC_DAPM_ENUM("SPK", hph_enum)
};

static const char * const lo_text[] = {
	"ZERO", "Switch",
};

static const struct soc_enum lo_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(hph_text), hph_text);

static const struct snd_kcontrol_new lo_mux[] = {
	SOC_DAPM_ENUM("LINE_OUT", lo_enum)
};

static void msm_anlg_cdc_codec_enable_adc_block(struct snd_soc_codec *codec,
					 int enable)
{
	struct sdm660_cdc_priv *wcd8x16 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s %d\n", __func__, enable);

	if (enable) {
		wcd8x16->adc_count++;
		snd_soc_update_bits(codec,
				    MSM89XX_PMIC_DIGITAL_CDC_ANA_CLK_CTL,
				    0x20, 0x20);
		snd_soc_update_bits(codec,
				    MSM89XX_PMIC_DIGITAL_CDC_DIG_CLK_CTL,
				    0x10, 0x10);
	} else {
		wcd8x16->adc_count--;
		if (!wcd8x16->adc_count) {
			snd_soc_update_bits(codec,
				    MSM89XX_PMIC_DIGITAL_CDC_DIG_CLK_CTL,
				    0x10, 0x00);
			snd_soc_update_bits(codec,
				    MSM89XX_PMIC_DIGITAL_CDC_ANA_CLK_CTL,
					    0x20, 0x0);
		}
	}
}

static int msm_anlg_cdc_codec_enable_adc(struct snd_soc_dapm_widget *w,
					 struct snd_kcontrol *kcontrol,
					 int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	u16 adc_reg;
	u8 init_bit_shift;

	dev_dbg(codec->dev, "%s %d\n", __func__, event);

	adc_reg = MSM89XX_PMIC_ANALOG_TX_1_2_TEST_CTL_2;

	if (w->reg == MSM89XX_PMIC_ANALOG_TX_1_EN)
		init_bit_shift = 5;
	else if ((w->reg == MSM89XX_PMIC_ANALOG_TX_2_EN) ||
		 (w->reg == MSM89XX_PMIC_ANALOG_TX_3_EN))
		init_bit_shift = 4;
	else {
		dev_err(codec->dev, "%s: Error, invalid adc register\n",
			__func__);
		return -EINVAL;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		msm_anlg_cdc_codec_enable_adc_block(codec, 1);
		if (w->reg == MSM89XX_PMIC_ANALOG_TX_2_EN)
			snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_MICB_1_CTL, 0x02, 0x02);
		/*
		 * Add delay of 10 ms to give sufficient time for the voltage
		 * to shoot up and settle so that the txfe init does not
		 * happen when the input voltage is changing too much.
		 */
		usleep_range(10000, 10010);
		snd_soc_update_bits(codec, adc_reg, 1 << init_bit_shift,
				1 << init_bit_shift);
		if (w->reg == MSM89XX_PMIC_ANALOG_TX_1_EN)
			snd_soc_update_bits(codec,
				MSM89XX_PMIC_DIGITAL_CDC_CONN_TX1_CTL,
				0x03, 0x00);
		else if ((w->reg == MSM89XX_PMIC_ANALOG_TX_2_EN) ||
			(w->reg == MSM89XX_PMIC_ANALOG_TX_3_EN))
			snd_soc_update_bits(codec,
				MSM89XX_PMIC_DIGITAL_CDC_CONN_TX2_CTL,
				0x03, 0x00);
		/* Wait for 1ms to allow txfe settling time */
		usleep_range(CODEC_DELAY_1_MS, CODEC_DELAY_1_1_MS);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/*
		 * Add delay of 12 ms before deasserting the init
		 * to reduce the tx pop
		 */
		usleep_range(12000, 12010);
		snd_soc_update_bits(codec, adc_reg, 1 << init_bit_shift, 0x00);
		/* Wait for 1ms to allow txfe settling time post powerup */
		usleep_range(CODEC_DELAY_1_MS, CODEC_DELAY_1_1_MS);
		break;
	case SND_SOC_DAPM_POST_PMD:
		msm_anlg_cdc_codec_enable_adc_block(codec, 0);
		if (w->reg == MSM89XX_PMIC_ANALOG_TX_2_EN)
			snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_MICB_1_CTL, 0x02, 0x00);
		if (w->reg == MSM89XX_PMIC_ANALOG_TX_1_EN)
			snd_soc_update_bits(codec,
				MSM89XX_PMIC_DIGITAL_CDC_CONN_TX1_CTL,
				0x03, 0x02);
		else if ((w->reg == MSM89XX_PMIC_ANALOG_TX_2_EN) ||
			(w->reg == MSM89XX_PMIC_ANALOG_TX_3_EN))
			snd_soc_update_bits(codec,
				MSM89XX_PMIC_DIGITAL_CDC_CONN_TX2_CTL,
				0x03, 0x02);

		break;
	}
	return 0;
}

static int msm_anlg_cdc_codec_enable_spk_pa(struct snd_soc_dapm_widget *w,
					    struct snd_kcontrol *kcontrol,
					    int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct sdm660_cdc_priv *sdm660_cdc =
					snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s %d %s\n", __func__, event, w->name);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_DIGITAL_CDC_ANA_CLK_CTL, 0x10, 0x10);
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_SPKR_PWRSTG_CTL, 0x01, 0x01);
		switch (sdm660_cdc->boost_option) {
		case BOOST_SWITCH:
			if (!sdm660_cdc->spk_boost_set)
				snd_soc_update_bits(codec,
					MSM89XX_PMIC_ANALOG_SPKR_DAC_CTL,
					0x10, 0x10);
			break;
		case BOOST_ALWAYS:
		case BOOST_ON_FOREVER:
			break;
		case BYPASS_ALWAYS:
			snd_soc_update_bits(codec,
				MSM89XX_PMIC_ANALOG_SPKR_DAC_CTL,
				0x10, 0x10);
			break;
		default:
			dev_err(codec->dev,
				"%s: invalid boost option: %d\n", __func__,
				sdm660_cdc->boost_option);
			break;
		}
		/* Wait for 1ms after SPK_DAC CTL setting */
		usleep_range(CODEC_DELAY_1_MS, CODEC_DELAY_1_1_MS);
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_SPKR_PWRSTG_CTL, 0xE0, 0xE0);
		if (get_codec_version(sdm660_cdc) != TOMBAK_1_0)
			snd_soc_update_bits(codec,
				MSM89XX_PMIC_ANALOG_RX_EAR_CTL, 0x01, 0x01);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* Wait for 1ms after SPK_VBAT_LDO Enable */
		usleep_range(CODEC_DELAY_1_MS, CODEC_DELAY_1_1_MS);
		switch (sdm660_cdc->boost_option) {
		case BOOST_SWITCH:
			if (sdm660_cdc->spk_boost_set)
				snd_soc_update_bits(codec,
					MSM89XX_PMIC_ANALOG_SPKR_DRV_CTL,
					0xEF, 0xEF);
			else
				snd_soc_update_bits(codec,
					MSM89XX_PMIC_ANALOG_SPKR_DAC_CTL,
					0x10, 0x00);
			break;
		case BOOST_ALWAYS:
		case BOOST_ON_FOREVER:
			snd_soc_update_bits(codec,
				MSM89XX_PMIC_ANALOG_SPKR_DRV_CTL,
				0xEF, 0xEF);
			break;
		case BYPASS_ALWAYS:
			snd_soc_update_bits(codec,
				MSM89XX_PMIC_ANALOG_SPKR_DAC_CTL, 0x10, 0x00);
			break;
		default:
			dev_err(codec->dev,
				"%s: invalid boost option: %d\n", __func__,
				sdm660_cdc->boost_option);
			break;
		}
		msm_anlg_cdc_dig_notifier_call(codec,
					       DIG_CDC_EVENT_RX3_MUTE_OFF);
		snd_soc_update_bits(codec, w->reg, 0x80, 0x80);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		msm_anlg_cdc_dig_notifier_call(codec,
					       DIG_CDC_EVENT_RX3_MUTE_ON);
		/*
		 * Add 1 ms sleep for the mute to take effect
		 */
		usleep_range(CODEC_DELAY_1_MS, CODEC_DELAY_1_1_MS);
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_SPKR_DAC_CTL, 0x10, 0x10);
		if (get_codec_version(sdm660_cdc) < CAJON_2_0)
			msm_anlg_cdc_boost_mode_sequence(codec, SPK_PMD);
		snd_soc_update_bits(codec, w->reg, 0x80, 0x00);
		switch (sdm660_cdc->boost_option) {
		case BOOST_SWITCH:
			if (sdm660_cdc->spk_boost_set)
				snd_soc_update_bits(codec,
					MSM89XX_PMIC_ANALOG_SPKR_DRV_CTL,
					0xEF, 0x69);
			break;
		case BOOST_ALWAYS:
		case BOOST_ON_FOREVER:
			snd_soc_update_bits(codec,
				MSM89XX_PMIC_ANALOG_SPKR_DRV_CTL,
				0xEF, 0x69);
			break;
		case BYPASS_ALWAYS:
			break;
		default:
			dev_err(codec->dev,
				"%s: invalid boost option: %d\n", __func__,
				sdm660_cdc->boost_option);
			break;
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_SPKR_PWRSTG_CTL, 0xE0, 0x00);
		/* Wait for 1ms to allow setting time for spkr path disable */
		usleep_range(CODEC_DELAY_1_MS, CODEC_DELAY_1_1_MS);
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_SPKR_PWRSTG_CTL, 0x01, 0x00);
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_SPKR_DAC_CTL, 0x10, 0x00);
		if (get_codec_version(sdm660_cdc) != TOMBAK_1_0)
			snd_soc_update_bits(codec,
				MSM89XX_PMIC_ANALOG_RX_EAR_CTL, 0x01, 0x00);
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_DIGITAL_CDC_ANA_CLK_CTL, 0x10, 0x00);
		if (get_codec_version(sdm660_cdc) >= CAJON_2_0)
			msm_anlg_cdc_boost_mode_sequence(codec, SPK_PMD);
		break;
	}
	return 0;
}

static int msm_anlg_cdc_codec_enable_dig_clk(struct snd_soc_dapm_widget *w,
					     struct snd_kcontrol *kcontrol,
					     int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct sdm660_cdc_priv *sdm660_cdc =
					snd_soc_codec_get_drvdata(codec);
	struct msm_asoc_mach_data *pdata = NULL;

	pdata = snd_soc_card_get_drvdata(codec->component.card);

	dev_dbg(codec->dev, "%s event %d w->name %s\n", __func__,
			event, w->name);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		msm_anlg_cdc_codec_enable_clock_block(codec, 1);
		snd_soc_update_bits(codec, w->reg, 0x80, 0x80);
		msm_anlg_cdc_boost_mode_sequence(codec, SPK_PMU);
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (sdm660_cdc->rx_bias_count == 0)
			snd_soc_update_bits(codec,
					MSM89XX_PMIC_DIGITAL_CDC_DIG_CLK_CTL,
					0x80, 0x00);
	}
	return 0;
}



static bool msm_anlg_cdc_use_mb(struct snd_soc_codec *codec)
{
	struct sdm660_cdc_priv *sdm660_cdc =
					snd_soc_codec_get_drvdata(codec);

	if (get_codec_version(sdm660_cdc) < CAJON)
		return true;
	else
		return false;
}

static void msm_anlg_cdc_set_auto_zeroing(struct snd_soc_codec *codec,
					  bool enable)
{
	struct sdm660_cdc_priv *sdm660_cdc =
					snd_soc_codec_get_drvdata(codec);

	if (get_codec_version(sdm660_cdc) < CONGA) {
		if (enable)
			/*
			 * Set autozeroing for special headset detection and
			 * buttons to work.
			 */
			snd_soc_update_bits(codec,
				MSM89XX_PMIC_ANALOG_MICB_2_EN,
				0x18, 0x10);
		else
			snd_soc_update_bits(codec,
				MSM89XX_PMIC_ANALOG_MICB_2_EN,
				0x18, 0x00);

	} else {
		dev_dbg(codec->dev,
			"%s: Auto Zeroing is not required from CONGA\n",
			__func__);
	}
}

static void msm_anlg_cdc_trim_btn_reg(struct snd_soc_codec *codec)
{
	struct sdm660_cdc_priv *sdm660_cdc =
					snd_soc_codec_get_drvdata(codec);

	if (get_codec_version(sdm660_cdc) == TOMBAK_1_0) {
		pr_debug("%s: This device needs to be trimmed\n", __func__);
		/*
		 * Calculate the trim value for each device used
		 * till is comes in production by hardware team
		 */
		snd_soc_update_bits(codec,
				MSM89XX_PMIC_ANALOG_SEC_ACCESS,
				0xA5, 0xA5);
		snd_soc_update_bits(codec,
				MSM89XX_PMIC_ANALOG_TRIM_CTRL2,
				0xFF, 0x30);
	} else {
		dev_dbg(codec->dev, "%s: This device is trimmed at ATE\n",
			__func__);
	}
}

static int msm_anlg_cdc_enable_ext_mb_source(struct wcd_mbhc *wcd_mbhc,
					     bool turn_on)
{
	int ret = 0;
	static int count;
	struct snd_soc_codec *codec = wcd_mbhc->codec;
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(codec);

	dev_dbg(codec->dev, "%s turn_on: %d count: %d\n", __func__, turn_on,
			count);
	if (turn_on) {
		if (!count) {
			ret = snd_soc_dapm_force_enable_pin(dapm,
				"MICBIAS_REGULATOR");
			snd_soc_dapm_sync(dapm);
		}
		count++;
	} else {
		if (count > 0)
			count--;
		if (!count) {
			ret = snd_soc_dapm_disable_pin(dapm,
				"MICBIAS_REGULATOR");
			snd_soc_dapm_sync(dapm);
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

static int msm_anlg_cdc_codec_enable_micbias(struct snd_soc_dapm_widget *w,
					     struct snd_kcontrol *kcontrol,
					     int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct sdm660_cdc_priv *sdm660_cdc =
				snd_soc_codec_get_drvdata(codec);
	u16 micb_int_reg;
	char *internal1_text = "Internal1";
	char *internal2_text = "Internal2";
	char *internal3_text = "Internal3";
	char *external2_text = "External2";
	char *external_text = "External";
	bool micbias2;

	dev_dbg(codec->dev, "%s %d\n", __func__, event);
	switch (w->reg) {
	case MSM89XX_PMIC_ANALOG_MICB_1_EN:
	case MSM89XX_PMIC_ANALOG_MICB_2_EN:
		micb_int_reg = MSM89XX_PMIC_ANALOG_MICB_1_INT_RBIAS;
		break;
	default:
		dev_err(codec->dev,
			"%s: Error, invalid micbias register 0x%x\n",
			__func__, w->reg);
		return -EINVAL;
	}

	micbias2 = (snd_soc_read(codec, MSM89XX_PMIC_ANALOG_MICB_2_EN) & 0x80);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (strnstr(w->name, internal1_text, strlen(w->name))) {
			if (get_codec_version(sdm660_cdc) >= CAJON)
				snd_soc_update_bits(codec,
					MSM89XX_PMIC_ANALOG_TX_1_2_ATEST_CTL_2,
					0x02, 0x02);
			snd_soc_update_bits(codec, micb_int_reg, 0x80, 0x80);
		} else if (strnstr(w->name, internal2_text, strlen(w->name))) {
			snd_soc_update_bits(codec, micb_int_reg, 0x10, 0x10);
			snd_soc_update_bits(codec, w->reg, 0x60, 0x00);
		} else if (strnstr(w->name, internal3_text, strlen(w->name))) {
			snd_soc_update_bits(codec, micb_int_reg, 0x2, 0x2);
		/*
		 * update MSM89XX_PMIC_ANALOG_TX_1_2_ATEST_CTL_2
		 * for external bias only, not for external2.
		*/
		} else if (!strnstr(w->name, external2_text, strlen(w->name)) &&
					strnstr(w->name, external_text,
						strlen(w->name))) {
			snd_soc_update_bits(codec,
					MSM89XX_PMIC_ANALOG_TX_1_2_ATEST_CTL_2,
					0x02, 0x02);
		}
		if (!strnstr(w->name, external_text, strlen(w->name)))
			snd_soc_update_bits(codec,
				MSM89XX_PMIC_ANALOG_MICB_1_EN, 0x05, 0x04);
		if (w->reg == MSM89XX_PMIC_ANALOG_MICB_1_EN)
			msm_anlg_cdc_configure_cap(codec, true, micbias2);

		break;
	case SND_SOC_DAPM_POST_PMU:
		if (get_codec_version(sdm660_cdc) <= TOMBAK_2_0)
			/*
			 * Wait for 20ms post micbias enable
			 * for version < tombak 2.0.
			 */
			usleep_range(20000, 20100);
		if (strnstr(w->name, internal1_text, strlen(w->name))) {
			snd_soc_update_bits(codec, micb_int_reg, 0x40, 0x40);
		} else if (strnstr(w->name, internal2_text,  strlen(w->name))) {
			snd_soc_update_bits(codec, micb_int_reg, 0x08, 0x08);
			msm_anlg_cdc_notifier_call(codec,
					WCD_EVENT_POST_MICBIAS_2_ON);
		} else if (strnstr(w->name, internal3_text, 30)) {
			snd_soc_update_bits(codec, micb_int_reg, 0x01, 0x01);
		} else if (strnstr(w->name, external2_text, strlen(w->name))) {
			msm_anlg_cdc_notifier_call(codec,
					WCD_EVENT_POST_MICBIAS_2_ON);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (strnstr(w->name, internal1_text, strlen(w->name))) {
			snd_soc_update_bits(codec, micb_int_reg, 0xC0, 0x40);
		} else if (strnstr(w->name, internal2_text, strlen(w->name))) {
			msm_anlg_cdc_notifier_call(codec,
					WCD_EVENT_POST_MICBIAS_2_OFF);
		} else if (strnstr(w->name, internal3_text, 30)) {
			snd_soc_update_bits(codec, micb_int_reg, 0x2, 0x0);
		} else if (strnstr(w->name, external2_text, strlen(w->name))) {
			/*
			 * send micbias turn off event to mbhc driver and then
			 * break, as no need to set MICB_1_EN register.
			 */
			msm_anlg_cdc_notifier_call(codec,
					WCD_EVENT_POST_MICBIAS_2_OFF);
			break;
		}
		if (w->reg == MSM89XX_PMIC_ANALOG_MICB_1_EN)
			msm_anlg_cdc_configure_cap(codec, false, micbias2);
		break;
	}
	return 0;
}

static void set_compander_mode(void *handle, int val)
{
	struct sdm660_cdc_priv *handle_cdc = handle;
	struct snd_soc_codec *codec = handle_cdc->codec;

	if (get_codec_version(handle_cdc) >= DIANGU) {
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_RX_COM_BIAS_DAC,
			0x08, val);
	};
}
static void update_clkdiv(void *handle, int val)
{
	struct sdm660_cdc_priv *handle_cdc = handle;
	struct snd_soc_codec *codec = handle_cdc->codec;

	snd_soc_update_bits(codec,
			    MSM89XX_PMIC_ANALOG_TX_1_2_TXFE_CLKDIV,
			    0xFF, val);
}

static int get_cdc_version(void *handle)
{
	struct sdm660_cdc_priv *sdm660_cdc = handle;

	return get_codec_version(sdm660_cdc);
}

static int sdm660_wcd_codec_enable_vdd_spkr(struct snd_soc_dapm_widget *w,
					       struct snd_kcontrol *kcontrol,
					       int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct sdm660_cdc_priv *sdm660_cdc =
					snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	if (!sdm660_cdc->ext_spk_boost_set) {
		dev_dbg(codec->dev, "%s: ext_boost not supported/disabled\n",
								__func__);
		return 0;
	}
	dev_dbg(codec->dev, "%s: %s %d\n", __func__, w->name, event);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (sdm660_cdc->spkdrv_reg) {
			ret = regulator_enable(sdm660_cdc->spkdrv_reg);
			if (ret)
				dev_err(codec->dev,
					"%s Failed to enable spkdrv reg %s\n",
					__func__, MSM89XX_VDD_SPKDRV_NAME);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (sdm660_cdc->spkdrv_reg) {
			ret = regulator_disable(sdm660_cdc->spkdrv_reg);
			if (ret)
				dev_err(codec->dev,
					"%s: Failed to disable spkdrv_reg %s\n",
					__func__, MSM89XX_VDD_SPKDRV_NAME);
		}
		break;
	}
	return 0;
}


/* The register address is the same as other codec so it can use resmgr */
static int msm_anlg_cdc_codec_enable_rx_bias(struct snd_soc_dapm_widget *w,
					     struct snd_kcontrol *kcontrol,
					     int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct sdm660_cdc_priv *sdm660_cdc =
					snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		sdm660_cdc->rx_bias_count++;
		if (sdm660_cdc->rx_bias_count == 1) {
			snd_soc_update_bits(codec,
					MSM89XX_PMIC_ANALOG_RX_COM_BIAS_DAC,
					0x80, 0x80);
			snd_soc_update_bits(codec,
					MSM89XX_PMIC_ANALOG_RX_COM_BIAS_DAC,
					0x01, 0x01);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		sdm660_cdc->rx_bias_count--;
		if (sdm660_cdc->rx_bias_count == 0) {
			snd_soc_update_bits(codec,
					MSM89XX_PMIC_ANALOG_RX_COM_BIAS_DAC,
					0x01, 0x00);
			snd_soc_update_bits(codec,
					MSM89XX_PMIC_ANALOG_RX_COM_BIAS_DAC,
					0x80, 0x00);
		}
		break;
	}
	dev_dbg(codec->dev, "%s rx_bias_count = %d\n",
			__func__, sdm660_cdc->rx_bias_count);
	return 0;
}

static uint32_t wcd_get_impedance_value(uint32_t imped)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(wcd_imped_val) - 1; i++) {
		if (imped >= wcd_imped_val[i] &&
			imped < wcd_imped_val[i + 1])
			break;
	}

	pr_debug("%s: selected impedance value = %d\n",
		 __func__, wcd_imped_val[i]);
	return wcd_imped_val[i];
}

static void wcd_imped_config(struct snd_soc_codec *codec,
			     uint32_t imped, bool set_gain)
{
	uint32_t value;
	int codec_version;
	struct sdm660_cdc_priv *sdm660_cdc =
				snd_soc_codec_get_drvdata(codec);

	value = wcd_get_impedance_value(imped);

	if (value < wcd_imped_val[0]) {
		dev_dbg(codec->dev,
			"%s, detected impedance is less than 4 Ohm\n",
			 __func__);
		return;
	}

	codec_version = get_codec_version(sdm660_cdc);

	if (set_gain) {
		switch (codec_version) {
		case TOMBAK_1_0:
		case TOMBAK_2_0:
		case CONGA:
			/*
			 * For 32Ohm load and higher loads, Set 0x19E
			 * bit 5 to 1 (POS_0_DB_DI). For loads lower
			 * than 32Ohm (such as 16Ohm load), Set 0x19E
			 * bit 5 to 0 (POS_M4P5_DB_DI)
			 */
			if (value >= 32)
				snd_soc_update_bits(codec,
					MSM89XX_PMIC_ANALOG_RX_EAR_CTL,
					0x20, 0x20);
			else
				snd_soc_update_bits(codec,
					MSM89XX_PMIC_ANALOG_RX_EAR_CTL,
					0x20, 0x00);
			break;
		case CAJON:
		case CAJON_2_0:
		case DIANGU:
		case DRAX_CDC:
			if (value >= 13) {
				snd_soc_update_bits(codec,
					MSM89XX_PMIC_ANALOG_RX_EAR_CTL,
					0x20, 0x20);
				snd_soc_update_bits(codec,
					MSM89XX_PMIC_ANALOG_NCP_VCTRL,
					0x07, 0x07);
			} else {
				snd_soc_update_bits(codec,
					MSM89XX_PMIC_ANALOG_RX_EAR_CTL,
					0x20, 0x00);
				snd_soc_update_bits(codec,
					MSM89XX_PMIC_ANALOG_NCP_VCTRL,
					0x07, 0x04);
			}
			break;
		}
	} else {
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_RX_EAR_CTL,
			0x20, 0x00);
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_NCP_VCTRL,
			0x07, 0x04);
	}

	dev_dbg(codec->dev, "%s: Exit\n", __func__);
}

static int msm_anlg_cdc_hphl_dac_event(struct snd_soc_dapm_widget *w,
				       struct snd_kcontrol *kcontrol,
				       int event)
{
	uint32_t impedl, impedr;
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct sdm660_cdc_priv *sdm660_cdc =
					snd_soc_codec_get_drvdata(codec);
	int ret;

	dev_dbg(codec->dev, "%s %s %d\n", __func__, w->name, event);
	ret = wcd_mbhc_get_impedance(&sdm660_cdc->mbhc,
			&impedl, &impedr);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (get_codec_version(sdm660_cdc) > CAJON)
			snd_soc_update_bits(codec,
				MSM89XX_PMIC_ANALOG_RX_HPH_CNP_EN,
				0x08, 0x08);
		if (get_codec_version(sdm660_cdc) == CAJON ||
			get_codec_version(sdm660_cdc) == CAJON_2_0) {
			snd_soc_update_bits(codec,
				MSM89XX_PMIC_ANALOG_RX_HPH_L_TEST,
				0x80, 0x80);
			snd_soc_update_bits(codec,
				MSM89XX_PMIC_ANALOG_RX_HPH_R_TEST,
				0x80, 0x80);
		}
		if (get_codec_version(sdm660_cdc) > CAJON)
			snd_soc_update_bits(codec,
				MSM89XX_PMIC_ANALOG_RX_HPH_CNP_EN,
				0x08, 0x00);
		if (sdm660_cdc->hph_mode == HD2_MODE)
			msm_anlg_cdc_dig_notifier_call(codec,
					DIG_CDC_EVENT_PRE_RX1_INT_ON);
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_RX_HPH_L_PA_DAC_CTL, 0x02, 0x02);
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_DIGITAL_CDC_DIG_CLK_CTL, 0x01, 0x01);
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_DIGITAL_CDC_ANA_CLK_CTL, 0x02, 0x02);
		if (!ret)
			wcd_imped_config(codec, impedl, true);
		else
			dev_dbg(codec->dev, "Failed to get mbhc impedance %d\n",
				ret);
		break;
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_RX_HPH_L_PA_DAC_CTL, 0x02, 0x00);
		break;
	case SND_SOC_DAPM_POST_PMD:
		wcd_imped_config(codec, impedl, false);
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_DIGITAL_CDC_ANA_CLK_CTL, 0x02, 0x00);
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_DIGITAL_CDC_DIG_CLK_CTL, 0x01, 0x00);
		if (sdm660_cdc->hph_mode == HD2_MODE)
			msm_anlg_cdc_dig_notifier_call(codec,
					DIG_CDC_EVENT_POST_RX1_INT_OFF);
		break;
	}
	return 0;
}

static int msm_anlg_cdc_lo_dac_event(struct snd_soc_dapm_widget *w,
				     struct snd_kcontrol *kcontrol,
				     int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	dev_dbg(codec->dev, "%s %s %d\n", __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_DIGITAL_CDC_ANA_CLK_CTL, 0x10, 0x10);
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_RX_LO_EN_CTL, 0x20, 0x20);
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_RX_LO_EN_CTL, 0x80, 0x80);
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_RX_LO_DAC_CTL, 0x08, 0x08);
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_RX_LO_DAC_CTL, 0x40, 0x40);
		break;
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_RX_LO_DAC_CTL, 0x80, 0x80);
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_RX_LO_DAC_CTL, 0x08, 0x00);
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_RX_LO_EN_CTL, 0x40, 0x40);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* Wait for 20ms before powerdown of lineout_dac */
		usleep_range(20000, 20100);
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_RX_LO_DAC_CTL, 0x80, 0x00);
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_RX_LO_DAC_CTL, 0x40, 0x00);
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_RX_LO_DAC_CTL, 0x08, 0x00);
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_RX_LO_EN_CTL, 0x80, 0x00);
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_RX_LO_EN_CTL, 0x40, 0x00);
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_RX_LO_EN_CTL, 0x20, 0x00);
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_DIGITAL_CDC_ANA_CLK_CTL, 0x10, 0x00);
		break;
	}
	return 0;
}

static int msm_anlg_cdc_hphr_dac_event(struct snd_soc_dapm_widget *w,
				       struct snd_kcontrol *kcontrol,
				       int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct sdm660_cdc_priv *sdm660_cdc =
					snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s %s %d\n", __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (sdm660_cdc->hph_mode == HD2_MODE)
			msm_anlg_cdc_dig_notifier_call(codec,
					DIG_CDC_EVENT_PRE_RX2_INT_ON);
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_RX_HPH_R_PA_DAC_CTL, 0x02, 0x02);
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_DIGITAL_CDC_DIG_CLK_CTL, 0x02, 0x02);
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_DIGITAL_CDC_ANA_CLK_CTL, 0x01, 0x01);
		break;
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_RX_HPH_R_PA_DAC_CTL, 0x02, 0x00);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_DIGITAL_CDC_ANA_CLK_CTL, 0x01, 0x00);
		snd_soc_update_bits(codec,
			MSM89XX_PMIC_DIGITAL_CDC_DIG_CLK_CTL, 0x02, 0x00);
		if (sdm660_cdc->hph_mode == HD2_MODE)
			msm_anlg_cdc_dig_notifier_call(codec,
					DIG_CDC_EVENT_POST_RX2_INT_OFF);
		break;
	}
	return 0;
}

static int msm_anlg_cdc_hph_pa_event(struct snd_soc_dapm_widget *w,
				     struct snd_kcontrol *kcontrol,
				     int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct sdm660_cdc_priv *sdm660_cdc =
					snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s: %s event = %d\n", __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (w->shift == 5)
			msm_anlg_cdc_notifier_call(codec,
					WCD_EVENT_PRE_HPHL_PA_ON);
		else if (w->shift == 4)
			msm_anlg_cdc_notifier_call(codec,
					WCD_EVENT_PRE_HPHR_PA_ON);
		snd_soc_update_bits(codec,
				MSM89XX_PMIC_ANALOG_NCP_FBCTRL, 0x20, 0x20);
		break;

	case SND_SOC_DAPM_POST_PMU:
		/* Wait for 7ms to allow setting time for HPH_PA Enable */
		usleep_range(7000, 7100);
		if (w->shift == 5) {
			snd_soc_update_bits(codec,
				MSM89XX_PMIC_ANALOG_RX_HPH_L_TEST, 0x04, 0x04);
			msm_anlg_cdc_dig_notifier_call(codec,
					       DIG_CDC_EVENT_RX1_MUTE_OFF);
		} else if (w->shift == 4) {
			snd_soc_update_bits(codec,
				MSM89XX_PMIC_ANALOG_RX_HPH_R_TEST, 0x04, 0x04);
			msm_anlg_cdc_dig_notifier_call(codec,
					       DIG_CDC_EVENT_RX2_MUTE_OFF);
		}
		break;

	case SND_SOC_DAPM_PRE_PMD:
		if (w->shift == 5) {
			msm_anlg_cdc_dig_notifier_call(codec,
					       DIG_CDC_EVENT_RX1_MUTE_ON);
			/* Wait for 20ms after HPHL RX digital mute */
			msleep(20);
			snd_soc_update_bits(codec,
				MSM89XX_PMIC_ANALOG_RX_HPH_L_TEST, 0x04, 0x00);
			msm_anlg_cdc_notifier_call(codec,
					WCD_EVENT_PRE_HPHL_PA_OFF);
		} else if (w->shift == 4) {
			msm_anlg_cdc_dig_notifier_call(codec,
					       DIG_CDC_EVENT_RX2_MUTE_ON);
			/* Wait for 20ms after HPHR RX digital mute */
			msleep(20);
			snd_soc_update_bits(codec,
				MSM89XX_PMIC_ANALOG_RX_HPH_R_TEST, 0x04, 0x00);
			msm_anlg_cdc_notifier_call(codec,
					WCD_EVENT_PRE_HPHR_PA_OFF);
		}
		if (get_codec_version(sdm660_cdc) >= CAJON) {
			snd_soc_update_bits(codec,
				MSM89XX_PMIC_ANALOG_RX_HPH_BIAS_CNP,
				0xF0, 0x30);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (w->shift == 5) {
			clear_bit(WCD_MBHC_HPHL_PA_OFF_ACK,
				&sdm660_cdc->mbhc.hph_pa_dac_state);
			msm_anlg_cdc_notifier_call(codec,
					WCD_EVENT_POST_HPHL_PA_OFF);
		} else if (w->shift == 4) {
			clear_bit(WCD_MBHC_HPHR_PA_OFF_ACK,
				&sdm660_cdc->mbhc.hph_pa_dac_state);
			msm_anlg_cdc_notifier_call(codec,
					WCD_EVENT_POST_HPHR_PA_OFF);
		}
		/* Wait for 15ms after HPH RX teardown */
		usleep_range(15000, 15100);
		break;
	}
	return 0;
}

static const struct snd_soc_dapm_route audio_map[] = {
	/* RDAC Connections */
	{"HPHR DAC", NULL, "RDAC2 MUX"},
	{"RDAC2 MUX", "RX1", "PDM_IN_RX1"},
	{"RDAC2 MUX", "RX2", "PDM_IN_RX2"},

	/* WSA */
	{"WSA_SPK OUT", NULL, "WSA Spk Switch"},
	{"WSA Spk Switch", "WSA", "EAR PA"},

	/* Earpiece (RX MIX1) */
	{"EAR", NULL, "EAR_S"},
	{"EAR_S", "Switch", "EAR PA"},
	{"EAR PA", NULL, "RX_BIAS"},
	{"EAR PA", NULL, "HPHL DAC"},
	{"EAR PA", NULL, "HPHR DAC"},
	{"EAR PA", NULL, "EAR CP"},

	/* Headset (RX MIX1 and RX MIX2) */
	{"HEADPHONE", NULL, "HPHL PA"},
	{"HEADPHONE", NULL, "HPHR PA"},

	{"Ext Spk", NULL, "Ext Spk Switch"},
	{"Ext Spk Switch", "On", "HPHL PA"},
	{"Ext Spk Switch", "On", "HPHR PA"},

	{"HPHL PA", NULL, "HPHL"},
	{"HPHR PA", NULL, "HPHR"},
	{"HPHL", "Switch", "HPHL DAC"},
	{"HPHR", "Switch", "HPHR DAC"},
	{"HPHL PA", NULL, "CP"},
	{"HPHL PA", NULL, "RX_BIAS"},
	{"HPHR PA", NULL, "CP"},
	{"HPHR PA", NULL, "RX_BIAS"},
	{"HPHL DAC", NULL, "PDM_IN_RX1"},

	{"SPK_OUT", NULL, "SPK PA"},
	{"SPK PA", NULL, "SPK_RX_BIAS"},
	{"SPK PA", NULL, "SPK"},
	{"SPK", "Switch", "SPK DAC"},
	{"SPK DAC", NULL, "PDM_IN_RX3"},
	{"SPK DAC", NULL, "VDD_SPKDRV"},

	/* lineout */
	{"LINEOUT", NULL, "LINEOUT PA"},
	{"LINEOUT PA", NULL, "SPK_RX_BIAS"},
	{"LINEOUT PA", NULL, "LINE_OUT"},
	{"LINE_OUT", "Switch", "LINEOUT DAC"},
	{"LINEOUT DAC", NULL, "PDM_IN_RX3"},

	/* lineout to WSA */
	{"WSA_SPK OUT", NULL, "LINEOUT PA"},

	{"PDM_IN_RX1", NULL, "RX1 CLK"},
	{"PDM_IN_RX2", NULL, "RX2 CLK"},
	{"PDM_IN_RX3", NULL, "RX3 CLK"},

	{"ADC1_OUT", NULL, "ADC1"},
	{"ADC2_OUT", NULL, "ADC2"},
	{"ADC3_OUT", NULL, "ADC3"},

	/* ADC Connections */
	{"ADC2", NULL, "ADC2 MUX"},
	{"ADC3", NULL, "ADC2 MUX"},
	{"ADC2 MUX", "INP2", "ADC2_INP2"},
	{"ADC2 MUX", "INP3", "ADC2_INP3"},

	{"ADC1", NULL, "ADC1_INP1"},
	{"ADC1_INP1", "Switch", "AMIC1"},
	{"ADC2_INP2", NULL, "AMIC2"},
	{"ADC2_INP3", NULL, "AMIC3"},

	{"MIC BIAS Internal1", NULL, "INT_LDO_H"},
	{"MIC BIAS Internal2", NULL, "INT_LDO_H"},
	{"MIC BIAS External", NULL, "INT_LDO_H"},
	{"MIC BIAS External2", NULL, "INT_LDO_H"},
	{"MIC BIAS Internal1", NULL, "MICBIAS_REGULATOR"},
	{"MIC BIAS Internal2", NULL, "MICBIAS_REGULATOR"},
	{"MIC BIAS External", NULL, "MICBIAS_REGULATOR"},
	{"MIC BIAS External2", NULL, "MICBIAS_REGULATOR"},
};

static int msm_anlg_cdc_startup(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct sdm660_cdc_priv *sdm660_cdc =
		snd_soc_codec_get_drvdata(dai->codec);

	dev_dbg(dai->codec->dev, "%s(): substream = %s  stream = %d\n",
		__func__,
		substream->name, substream->stream);
	/*
	 * If status_mask is BUS_DOWN it means SSR is not complete.
	 * So return error.
	 */
	if (test_bit(BUS_DOWN, &sdm660_cdc->status_mask)) {
		dev_err(dai->codec->dev, "Error, Device is not up post SSR\n");
		return -EINVAL;
	}
	return 0;
}

static void msm_anlg_cdc_shutdown(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	dev_dbg(dai->codec->dev,
		"%s(): substream = %s  stream = %d\n", __func__,
		substream->name, substream->stream);
}

int msm_anlg_cdc_mclk_enable(struct snd_soc_codec *codec,
			     int mclk_enable, bool dapm)
{
	struct sdm660_cdc_priv *sdm660_cdc =
					snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s: mclk_enable = %u, dapm = %d\n",
		__func__, mclk_enable, dapm);
	if (mclk_enable) {
		sdm660_cdc->int_mclk0_enabled = true;
		msm_anlg_cdc_codec_enable_clock_block(codec, 1);
	} else {
		if (!sdm660_cdc->int_mclk0_enabled) {
			dev_err(codec->dev, "Error, MCLK already diabled\n");
			return -EINVAL;
		}
		sdm660_cdc->int_mclk0_enabled = false;
		msm_anlg_cdc_codec_enable_clock_block(codec, 0);
	}
	return 0;
}

static int msm_anlg_cdc_set_dai_sysclk(struct snd_soc_dai *dai,
		int clk_id, unsigned int freq, int dir)
{
	dev_dbg(dai->codec->dev, "%s\n", __func__);
	return 0;
}

static int msm_anlg_cdc_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	dev_dbg(dai->codec->dev, "%s\n", __func__);
	return 0;
}

static int msm_anlg_cdc_set_channel_map(struct snd_soc_dai *dai,
				unsigned int tx_num, unsigned int *tx_slot,
				unsigned int rx_num, unsigned int *rx_slot)

{
	dev_dbg(dai->codec->dev, "%s\n", __func__);
	return 0;
}

static int msm_anlg_cdc_get_channel_map(struct snd_soc_dai *dai,
				 unsigned int *tx_num, unsigned int *tx_slot,
				 unsigned int *rx_num, unsigned int *rx_slot)

{
	dev_dbg(dai->codec->dev, "%s\n", __func__);
	return 0;
}

static struct snd_soc_dai_ops msm_anlg_cdc_dai_ops = {
	.startup = msm_anlg_cdc_startup,
	.shutdown = msm_anlg_cdc_shutdown,
	.set_sysclk = msm_anlg_cdc_set_dai_sysclk,
	.set_fmt = msm_anlg_cdc_set_dai_fmt,
	.set_channel_map = msm_anlg_cdc_set_channel_map,
	.get_channel_map = msm_anlg_cdc_get_channel_map,
};

static struct snd_soc_dai_driver msm_anlg_cdc_i2s_dai[] = {
	{
		.name = "msm_anlg_cdc_i2s_rx1",
		.id = AIF1_PB,
		.playback = {
			.stream_name = "PDM Playback",
			.rates = SDM660_CDC_RATES,
			.formats = SDM660_CDC_FORMATS,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 3,
		},
		.ops = &msm_anlg_cdc_dai_ops,
	},
	{
		.name = "msm_anlg_cdc_i2s_tx1",
		.id = AIF1_CAP,
		.capture = {
			.stream_name = "PDM Capture",
			.rates = SDM660_CDC_RATES,
			.formats = SDM660_CDC_FORMATS,
			.rate_max = 48000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 4,
		},
		.ops = &msm_anlg_cdc_dai_ops,
	},
	{
		.name = "msm_anlg_cdc_i2s_tx2",
		.id = AIF3_SVA,
		.capture = {
			.stream_name = "RecordSVA",
			.rates = SDM660_CDC_RATES,
			.formats = SDM660_CDC_FORMATS,
			.rate_max = 48000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &msm_anlg_cdc_dai_ops,
	},
	{
		.name = "msm_anlg_vifeedback",
		.id = AIF2_VIFEED,
		.capture = {
			.stream_name = "VIfeed",
			.rates = SDM660_CDC_RATES,
			.formats = SDM660_CDC_FORMATS,
			.rate_max = 48000,
			.rate_min = 48000,
			.channels_min = 2,
			.channels_max = 2,
		},
		.ops = &msm_anlg_cdc_dai_ops,
	},
};


static int msm_anlg_cdc_codec_enable_lo_pa(struct snd_soc_dapm_widget *w,
					   struct snd_kcontrol *kcontrol,
					   int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);

	dev_dbg(codec->dev, "%s: %d %s\n", __func__, event, w->name);
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		msm_anlg_cdc_dig_notifier_call(codec,
				       DIG_CDC_EVENT_RX3_MUTE_OFF);
		break;
	case SND_SOC_DAPM_POST_PMD:
		msm_anlg_cdc_dig_notifier_call(codec,
				       DIG_CDC_EVENT_RX3_MUTE_ON);
		break;
	}

	return 0;
}

static int msm_anlg_cdc_codec_enable_spk_ext_pa(struct snd_soc_dapm_widget *w,
						struct snd_kcontrol *kcontrol,
						int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct sdm660_cdc_priv *sdm660_cdc =
					snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s: %s event = %d\n", __func__, w->name, event);
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		dev_dbg(codec->dev,
			"%s: enable external speaker PA\n", __func__);
		if (sdm660_cdc->codec_spk_ext_pa_cb)
			sdm660_cdc->codec_spk_ext_pa_cb(codec, 1);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		dev_dbg(codec->dev,
			"%s: enable external speaker PA\n", __func__);
		if (sdm660_cdc->codec_spk_ext_pa_cb)
			sdm660_cdc->codec_spk_ext_pa_cb(codec, 0);
		break;
	}
	return 0;
}

static int msm_anlg_cdc_codec_enable_ear_pa(struct snd_soc_dapm_widget *w,
					    struct snd_kcontrol *kcontrol,
					    int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct sdm660_cdc_priv *sdm660_cdc =
					snd_soc_codec_get_drvdata(codec);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		dev_dbg(codec->dev,
			"%s: Sleeping 20ms after select EAR PA\n",
			__func__);
		snd_soc_update_bits(codec, MSM89XX_PMIC_ANALOG_RX_EAR_CTL,
			    0x80, 0x80);
		if (get_codec_version(sdm660_cdc) < CONGA)
			snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_RX_HPH_CNP_WG_TIME, 0xFF, 0x2A);
		if (get_codec_version(sdm660_cdc) >= DIANGU) {
			snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_RX_COM_BIAS_DAC, 0x08, 0x00);
			snd_soc_update_bits(codec,
				MSM89XX_PMIC_ANALOG_RX_HPH_L_TEST, 0x04, 0x04);
			snd_soc_update_bits(codec,
				MSM89XX_PMIC_ANALOG_RX_HPH_R_TEST, 0x04, 0x04);
		}
		break;
	case SND_SOC_DAPM_POST_PMU:
		dev_dbg(codec->dev,
			"%s: Sleeping 20ms after enabling EAR PA\n",
			__func__);
		snd_soc_update_bits(codec, MSM89XX_PMIC_ANALOG_RX_EAR_CTL,
			    0x40, 0x40);
		/* Wait for 7ms after EAR PA enable */
		usleep_range(7000, 7100);
		msm_anlg_cdc_dig_notifier_call(codec,
				       DIG_CDC_EVENT_RX1_MUTE_OFF);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		msm_anlg_cdc_dig_notifier_call(codec,
				       DIG_CDC_EVENT_RX1_MUTE_ON);
		/* Wait for 20ms for RX digital mute to take effect */
		msleep(20);
		if (sdm660_cdc->boost_option == BOOST_ALWAYS) {
			dev_dbg(codec->dev,
				"%s: boost_option:%d, tear down ear\n",
				__func__, sdm660_cdc->boost_option);
			msm_anlg_cdc_boost_mode_sequence(codec, EAR_PMD);
		}
		if (get_codec_version(sdm660_cdc) >= DIANGU) {
			snd_soc_update_bits(codec,
				MSM89XX_PMIC_ANALOG_RX_HPH_L_TEST, 0x04, 0x0);
			snd_soc_update_bits(codec,
				MSM89XX_PMIC_ANALOG_RX_HPH_R_TEST, 0x04, 0x0);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		dev_dbg(codec->dev,
			"%s: Sleeping 7ms after disabling EAR PA\n",
			__func__);
		snd_soc_update_bits(codec, MSM89XX_PMIC_ANALOG_RX_EAR_CTL,
			    0x40, 0x00);
		/* Wait for 7ms after EAR PA teardown */
		usleep_range(7000, 7100);
		if (get_codec_version(sdm660_cdc) < CONGA)
			snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_RX_HPH_CNP_WG_TIME, 0xFF, 0x16);
		if (get_codec_version(sdm660_cdc) >= DIANGU)
			snd_soc_update_bits(codec,
			MSM89XX_PMIC_ANALOG_RX_COM_BIAS_DAC, 0x08, 0x08);
		break;
	}
	return 0;
}

static const struct snd_soc_dapm_widget msm_anlg_cdc_dapm_widgets[] = {
	SND_SOC_DAPM_PGA_E("EAR PA", SND_SOC_NOPM,
			0, 0, NULL, 0, msm_anlg_cdc_codec_enable_ear_pa,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("HPHL PA", MSM89XX_PMIC_ANALOG_RX_HPH_CNP_EN,
		5, 0, NULL, 0,
		msm_anlg_cdc_hph_pa_event, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("HPHR PA", MSM89XX_PMIC_ANALOG_RX_HPH_CNP_EN,
		4, 0, NULL, 0,
		msm_anlg_cdc_hph_pa_event, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("SPK PA", SND_SOC_NOPM,
			0, 0, NULL, 0, msm_anlg_cdc_codec_enable_spk_pa,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("LINEOUT PA", MSM89XX_PMIC_ANALOG_RX_LO_EN_CTL,
			5, 0, NULL, 0, msm_anlg_cdc_codec_enable_lo_pa,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX("EAR_S", SND_SOC_NOPM, 0, 0, ear_pa_mux),
	SND_SOC_DAPM_MUX("SPK", SND_SOC_NOPM, 0, 0, spkr_mux),
	SND_SOC_DAPM_MUX("HPHL", SND_SOC_NOPM, 0, 0, hphl_mux),
	SND_SOC_DAPM_MUX("HPHR", SND_SOC_NOPM, 0, 0, hphr_mux),
	SND_SOC_DAPM_MUX("RDAC2 MUX", SND_SOC_NOPM, 0, 0, &rdac2_mux),
	SND_SOC_DAPM_MUX("WSA Spk Switch", SND_SOC_NOPM, 0, 0, wsa_spk_mux),
	SND_SOC_DAPM_MUX("Ext Spk Switch", SND_SOC_NOPM, 0, 0, &ext_spk_mux),
	SND_SOC_DAPM_MUX("LINE_OUT", SND_SOC_NOPM, 0, 0, lo_mux),
	SND_SOC_DAPM_MUX("ADC2 MUX", SND_SOC_NOPM, 0, 0, &tx_adc2_mux),

	SND_SOC_DAPM_MIXER_E("HPHL DAC",
		MSM89XX_PMIC_ANALOG_RX_HPH_L_PA_DAC_CTL, 3, 0, NULL,
		0, msm_anlg_cdc_hphl_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("HPHR DAC",
		MSM89XX_PMIC_ANALOG_RX_HPH_R_PA_DAC_CTL, 3, 0, NULL,
		0, msm_anlg_cdc_hphr_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER("ADC2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("ADC3", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_DAC("SPK DAC", NULL, MSM89XX_PMIC_ANALOG_SPKR_DAC_CTL,
			 7, 0),
	SND_SOC_DAPM_DAC_E("LINEOUT DAC", NULL,
		SND_SOC_NOPM, 0, 0, msm_anlg_cdc_lo_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SPK("Ext Spk", msm_anlg_cdc_codec_enable_spk_ext_pa),

	SND_SOC_DAPM_SWITCH("ADC1_INP1", SND_SOC_NOPM, 0, 0,
			    &adc1_switch),
	SND_SOC_DAPM_SUPPLY("RX1 CLK", MSM89XX_PMIC_DIGITAL_CDC_DIG_CLK_CTL,
			    0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("RX2 CLK", MSM89XX_PMIC_DIGITAL_CDC_DIG_CLK_CTL,
			    1, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("RX3 CLK", MSM89XX_PMIC_DIGITAL_CDC_DIG_CLK_CTL,
			    2, 0, msm_anlg_cdc_codec_enable_dig_clk,
			    SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY("CP", MSM89XX_PMIC_ANALOG_NCP_EN, 0, 0,
			    msm_anlg_cdc_codec_enable_charge_pump,
			    SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			    SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY("EAR CP", MSM89XX_PMIC_ANALOG_NCP_EN, 4, 0,
			    msm_anlg_cdc_codec_enable_charge_pump,
			    SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			    SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("RX_BIAS", 1, SND_SOC_NOPM,
		0, 0, msm_anlg_cdc_codec_enable_rx_bias,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY_S("SPK_RX_BIAS", 1, SND_SOC_NOPM, 0, 0,
		msm_anlg_cdc_codec_enable_rx_bias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY("VDD_SPKDRV", SND_SOC_NOPM, 0, 0,
			    sdm660_wcd_codec_enable_vdd_spkr,
			    SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY("INT_LDO_H", SND_SOC_NOPM, 1, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("MICBIAS_REGULATOR", SND_SOC_NOPM,
		ON_DEMAND_MICBIAS, 0,
		msm_anlg_cdc_codec_enable_on_demand_supply,
		SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MICBIAS_E("MIC BIAS Internal1",
		MSM89XX_PMIC_ANALOG_MICB_1_EN, 7, 0,
		msm_anlg_cdc_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS Internal2",
		MSM89XX_PMIC_ANALOG_MICB_2_EN, 7, 0,
		msm_anlg_cdc_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS Internal3",
		MSM89XX_PMIC_ANALOG_MICB_1_EN, 7, 0,
		msm_anlg_cdc_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("ADC1", NULL, MSM89XX_PMIC_ANALOG_TX_1_EN, 7, 0,
		msm_anlg_cdc_codec_enable_adc, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("ADC2_INP2",
		NULL, MSM89XX_PMIC_ANALOG_TX_2_EN, 7, 0,
		msm_anlg_cdc_codec_enable_adc, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("ADC2_INP3",
		NULL, MSM89XX_PMIC_ANALOG_TX_3_EN, 7, 0,
		msm_anlg_cdc_codec_enable_adc, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MICBIAS_E("MIC BIAS External",
		MSM89XX_PMIC_ANALOG_MICB_1_EN, 7, 0,
		msm_anlg_cdc_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS External2",
		MSM89XX_PMIC_ANALOG_MICB_2_EN, 7, 0,
		msm_anlg_cdc_codec_enable_micbias, SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_INPUT("AMIC1"),
	SND_SOC_DAPM_INPUT("AMIC2"),
	SND_SOC_DAPM_INPUT("AMIC3"),
	SND_SOC_DAPM_AIF_IN("PDM_IN_RX1", "PDM Playback",
		0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("PDM_IN_RX2", "PDM Playback",
		0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("PDM_IN_RX3", "PDM Playback",
		0, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_OUTPUT("EAR"),
	SND_SOC_DAPM_OUTPUT("WSA_SPK OUT"),
	SND_SOC_DAPM_OUTPUT("HEADPHONE"),
	SND_SOC_DAPM_OUTPUT("SPK_OUT"),
	SND_SOC_DAPM_OUTPUT("LINEOUT"),
	SND_SOC_DAPM_AIF_OUT("ADC1_OUT", "PDM Capture",
		0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("ADC2_OUT", "PDM Capture",
		0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("ADC3_OUT", "PDM Capture",
		0, SND_SOC_NOPM, 0, 0),
};

static const struct sdm660_cdc_reg_mask_val msm_anlg_cdc_reg_defaults[] = {
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_SPKR_DAC_CTL, 0x03),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_CURRENT_LIMIT, 0x82),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_SPKR_OCP_CTL, 0xE1),
};

static const struct sdm660_cdc_reg_mask_val
					msm_anlg_cdc_reg_defaults_2_0[] = {
	MSM89XX_REG_VAL(MSM89XX_PMIC_DIGITAL_SEC_ACCESS, 0xA5),
	MSM89XX_REG_VAL(MSM89XX_PMIC_DIGITAL_PERPH_RESET_CTL3, 0x0F),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_TX_1_2_OPAMP_BIAS, 0x4F),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_NCP_FBCTRL, 0x28),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_SPKR_DRV_CTL, 0x69),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_SPKR_DRV_DBG, 0x01),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_BOOST_EN_CTL, 0x5F),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_SLOPE_COMP_IP_ZERO, 0x88),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_SEC_ACCESS, 0xA5),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_PERPH_RESET_CTL3, 0x0F),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_CURRENT_LIMIT, 0x82),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_SPKR_DAC_CTL, 0x03),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_SPKR_OCP_CTL, 0xE1),
	MSM89XX_REG_VAL(MSM89XX_PMIC_DIGITAL_CDC_RST_CTL, 0x80),
};

static const struct sdm660_cdc_reg_mask_val conga_wcd_reg_defaults[] = {
	MSM89XX_REG_VAL(MSM89XX_PMIC_DIGITAL_SEC_ACCESS, 0xA5),
	MSM89XX_REG_VAL(MSM89XX_PMIC_DIGITAL_PERPH_RESET_CTL3, 0x0F),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_SEC_ACCESS, 0xA5),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_PERPH_RESET_CTL3, 0x0F),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_TX_1_2_OPAMP_BIAS, 0x4C),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_NCP_FBCTRL, 0x28),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_SPKR_DRV_CTL, 0x69),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_SPKR_DRV_DBG, 0x01),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_PERPH_SUBTYPE, 0x0A),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_SPKR_DAC_CTL, 0x03),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_SPKR_OCP_CTL, 0xE1),
	MSM89XX_REG_VAL(MSM89XX_PMIC_DIGITAL_CDC_RST_CTL, 0x80),
};

static const struct sdm660_cdc_reg_mask_val cajon_wcd_reg_defaults[] = {
	MSM89XX_REG_VAL(MSM89XX_PMIC_DIGITAL_SEC_ACCESS, 0xA5),
	MSM89XX_REG_VAL(MSM89XX_PMIC_DIGITAL_PERPH_RESET_CTL3, 0x0F),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_SEC_ACCESS, 0xA5),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_PERPH_RESET_CTL3, 0x0F),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_TX_1_2_OPAMP_BIAS, 0x4C),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_CURRENT_LIMIT, 0x82),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_NCP_FBCTRL, 0xA8),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_NCP_VCTRL, 0xA4),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_SPKR_ANA_BIAS_SET, 0x41),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_SPKR_DRV_CTL, 0x69),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_SPKR_DRV_DBG, 0x01),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_SPKR_OCP_CTL, 0xE1),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_SPKR_DAC_CTL, 0x03),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_RX_HPH_BIAS_PA, 0xFA),
	MSM89XX_REG_VAL(MSM89XX_PMIC_DIGITAL_CDC_RST_CTL, 0x80),
};

static const struct sdm660_cdc_reg_mask_val cajon2p0_wcd_reg_defaults[] = {
	MSM89XX_REG_VAL(MSM89XX_PMIC_DIGITAL_SEC_ACCESS, 0xA5),
	MSM89XX_REG_VAL(MSM89XX_PMIC_DIGITAL_PERPH_RESET_CTL3, 0x0F),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_SEC_ACCESS, 0xA5),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_PERPH_RESET_CTL3, 0x0F),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_TX_1_2_OPAMP_BIAS, 0x4C),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_CURRENT_LIMIT, 0xA2),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_NCP_FBCTRL, 0xA8),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_NCP_VCTRL, 0xA4),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_SPKR_ANA_BIAS_SET, 0x41),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_SPKR_DRV_CTL, 0x69),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_SPKR_DRV_DBG, 0x01),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_SPKR_OCP_CTL, 0xE1),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_SPKR_DAC_CTL, 0x03),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_RX_EAR_STATUS, 0x10),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_BYPASS_MODE, 0x18),
	MSM89XX_REG_VAL(MSM89XX_PMIC_ANALOG_RX_HPH_BIAS_PA, 0xFA),
	MSM89XX_REG_VAL(MSM89XX_PMIC_DIGITAL_CDC_RST_CTL, 0x80),
};

static void msm_anlg_cdc_update_reg_defaults(struct snd_soc_codec *codec)
{
	u32 i, version;
	struct sdm660_cdc_priv *sdm660_cdc =
					snd_soc_codec_get_drvdata(codec);

	version = get_codec_version(sdm660_cdc);
	if (version == TOMBAK_1_0) {
		for (i = 0; i < ARRAY_SIZE(msm_anlg_cdc_reg_defaults); i++)
			snd_soc_write(codec, msm_anlg_cdc_reg_defaults[i].reg,
					msm_anlg_cdc_reg_defaults[i].val);
	} else if (version == TOMBAK_2_0) {
		for (i = 0; i < ARRAY_SIZE(msm_anlg_cdc_reg_defaults_2_0); i++)
			snd_soc_write(codec,
				msm_anlg_cdc_reg_defaults_2_0[i].reg,
				msm_anlg_cdc_reg_defaults_2_0[i].val);
	} else if (version == CONGA) {
		for (i = 0; i < ARRAY_SIZE(conga_wcd_reg_defaults); i++)
			snd_soc_write(codec,
				conga_wcd_reg_defaults[i].reg,
				conga_wcd_reg_defaults[i].val);
	} else if (version == CAJON) {
		for (i = 0; i < ARRAY_SIZE(cajon_wcd_reg_defaults); i++)
			snd_soc_write(codec,
				cajon_wcd_reg_defaults[i].reg,
				cajon_wcd_reg_defaults[i].val);
	} else if (version == CAJON_2_0 || version == DIANGU
				|| version == DRAX_CDC) {
		for (i = 0; i < ARRAY_SIZE(cajon2p0_wcd_reg_defaults); i++)
			snd_soc_write(codec,
				cajon2p0_wcd_reg_defaults[i].reg,
				cajon2p0_wcd_reg_defaults[i].val);
	}
}

static const struct sdm660_cdc_reg_mask_val
	msm_anlg_cdc_codec_reg_init_val[] = {

	/* Initialize current threshold to 350MA
	 * number of wait and run cycles to 4096
	 */
	{MSM89XX_PMIC_ANALOG_RX_COM_OCP_CTL, 0xFF, 0x12},
	{MSM89XX_PMIC_ANALOG_RX_COM_OCP_COUNT, 0xFF, 0xFF},
};

static void msm_anlg_cdc_codec_init_reg(struct snd_soc_codec *codec)
{
	u32 i;

	for (i = 0; i < ARRAY_SIZE(msm_anlg_cdc_codec_reg_init_val); i++)
		snd_soc_update_bits(codec,
				    msm_anlg_cdc_codec_reg_init_val[i].reg,
				    msm_anlg_cdc_codec_reg_init_val[i].mask,
				    msm_anlg_cdc_codec_reg_init_val[i].val);
}

static int msm_anlg_cdc_bringup(struct snd_soc_codec *codec)
{
	snd_soc_write(codec,
		MSM89XX_PMIC_DIGITAL_SEC_ACCESS,
		0xA5);
	snd_soc_write(codec, MSM89XX_PMIC_DIGITAL_PERPH_RESET_CTL4, 0x01);
	snd_soc_write(codec,
		MSM89XX_PMIC_ANALOG_SEC_ACCESS,
		0xA5);
	snd_soc_write(codec, MSM89XX_PMIC_ANALOG_PERPH_RESET_CTL4, 0x01);
	snd_soc_write(codec,
		MSM89XX_PMIC_DIGITAL_SEC_ACCESS,
		0xA5);
	snd_soc_write(codec, MSM89XX_PMIC_DIGITAL_PERPH_RESET_CTL4, 0x00);
	snd_soc_write(codec,
		MSM89XX_PMIC_ANALOG_SEC_ACCESS,
		0xA5);
	snd_soc_write(codec, MSM89XX_PMIC_ANALOG_PERPH_RESET_CTL4, 0x00);

	return 0;
}

static struct regulator *msm_anlg_cdc_find_regulator(
				const struct sdm660_cdc_priv *sdm660_cdc,
				const char *name)
{
	int i;

	for (i = 0; i < sdm660_cdc->num_of_supplies; i++) {
		if (sdm660_cdc->supplies[i].supply &&
		    !strcmp(sdm660_cdc->supplies[i].supply, name))
			return sdm660_cdc->supplies[i].consumer;
	}

	dev_dbg(sdm660_cdc->dev, "Error: regulator not found:%s\n"
				, name);
	return NULL;
}

static void msm_anlg_cdc_update_micbias_regulator(
				const struct sdm660_cdc_priv *sdm660_cdc,
				const char *name,
				struct on_demand_supply *micbias_supply)
{
	int i;
	struct sdm660_cdc_pdata *pdata = sdm660_cdc->dev->platform_data;

	for (i = 0; i < sdm660_cdc->num_of_supplies; i++) {
		if (sdm660_cdc->supplies[i].supply &&
		    !strcmp(sdm660_cdc->supplies[i].supply, name)) {
			micbias_supply->supply =
				sdm660_cdc->supplies[i].consumer;
			micbias_supply->min_uv = pdata->regulator[i].min_uv;
			micbias_supply->max_uv = pdata->regulator[i].max_uv;
			micbias_supply->optimum_ua =
					pdata->regulator[i].optimum_ua;
			return;
		}
	}

	dev_err(sdm660_cdc->dev, "Error: regulator not found:%s\n", name);
}

static int msm_anlg_cdc_device_down(struct snd_soc_codec *codec)
{
	struct msm_asoc_mach_data *pdata = NULL;
	struct sdm660_cdc_priv *sdm660_cdc_priv =
		snd_soc_codec_get_drvdata(codec);
	unsigned int tx_1_en;
	unsigned int tx_2_en;

	pdata = snd_soc_card_get_drvdata(codec->component.card);
	dev_dbg(codec->dev, "%s: device down!\n", __func__);

	tx_1_en = snd_soc_read(codec, MSM89XX_PMIC_ANALOG_TX_1_EN);
	tx_2_en = snd_soc_read(codec, MSM89XX_PMIC_ANALOG_TX_2_EN);
	tx_1_en = tx_1_en & 0x7f;
	tx_2_en = tx_2_en & 0x7f;
	snd_soc_write(codec,
		MSM89XX_PMIC_ANALOG_TX_1_EN, tx_1_en);
	snd_soc_write(codec,
		MSM89XX_PMIC_ANALOG_TX_2_EN, tx_2_en);
	if (sdm660_cdc_priv->boost_option == BOOST_ON_FOREVER) {
		if ((snd_soc_read(codec, MSM89XX_PMIC_ANALOG_SPKR_DRV_CTL)
			& 0x80) == 0) {
			msm_anlg_cdc_dig_notifier_call(codec,
						       DIG_CDC_EVENT_CLK_ON);
			snd_soc_write(codec,
				MSM89XX_PMIC_ANALOG_MASTER_BIAS_CTL, 0x30);
			snd_soc_update_bits(codec,
				MSM89XX_PMIC_DIGITAL_CDC_RST_CTL, 0x80, 0x80);
			snd_soc_update_bits(codec,
				MSM89XX_PMIC_DIGITAL_CDC_TOP_CLK_CTL,
				0x0C, 0x0C);
			snd_soc_update_bits(codec,
				MSM89XX_PMIC_DIGITAL_CDC_DIG_CLK_CTL,
				0x84, 0x84);
			snd_soc_update_bits(codec,
				MSM89XX_PMIC_DIGITAL_CDC_ANA_CLK_CTL,
				0x10, 0x10);
			snd_soc_update_bits(codec,
				MSM89XX_PMIC_ANALOG_SPKR_PWRSTG_CTL,
				0x1F, 0x1F);
			snd_soc_update_bits(codec,
				MSM89XX_PMIC_ANALOG_RX_COM_BIAS_DAC,
				0x90, 0x90);
			snd_soc_update_bits(codec,
				MSM89XX_PMIC_ANALOG_RX_EAR_CTL,
				0xFF, 0xFF);
			/* Wait for 20us for boost settings to take effect */
			usleep_range(20, 21);
			snd_soc_update_bits(codec,
				MSM89XX_PMIC_ANALOG_SPKR_PWRSTG_CTL,
				0xFF, 0xFF);
			snd_soc_update_bits(codec,
				MSM89XX_PMIC_ANALOG_SPKR_DRV_CTL,
				0xE9, 0xE9);
		}
	}
	msm_anlg_cdc_boost_off(codec);
	sdm660_cdc_priv->hph_mode = NORMAL_MODE;

	/* 40ms to allow boost to discharge */
	msleep(40);
	/* Disable PA to avoid pop during codec bring up */
	snd_soc_update_bits(codec, MSM89XX_PMIC_ANALOG_RX_HPH_CNP_EN,
			0x30, 0x00);
	snd_soc_update_bits(codec, MSM89XX_PMIC_ANALOG_SPKR_DRV_CTL,
			0x80, 0x00);
	snd_soc_write(codec,
		MSM89XX_PMIC_ANALOG_RX_HPH_L_PA_DAC_CTL, 0x20);
	snd_soc_write(codec,
		MSM89XX_PMIC_ANALOG_RX_HPH_R_PA_DAC_CTL, 0x20);
	snd_soc_write(codec,
		MSM89XX_PMIC_ANALOG_RX_EAR_CTL, 0x12);
	snd_soc_write(codec,
		MSM89XX_PMIC_ANALOG_SPKR_DAC_CTL, 0x93);

	msm_anlg_cdc_dig_notifier_call(codec, DIG_CDC_EVENT_SSR_DOWN);
	atomic_set(&pdata->int_mclk0_enabled, false);
	set_bit(BUS_DOWN, &sdm660_cdc_priv->status_mask);
	snd_soc_card_change_online_state(codec->component.card, 0);

	return 0;
}

static int msm_anlg_cdc_device_up(struct snd_soc_codec *codec)
{
	struct sdm660_cdc_priv *sdm660_cdc_priv =
		snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s: device up!\n", __func__);

	msm_anlg_cdc_dig_notifier_call(codec, DIG_CDC_EVENT_SSR_UP);
	clear_bit(BUS_DOWN, &sdm660_cdc_priv->status_mask);
	snd_soc_card_change_online_state(codec->component.card, 1);
	/* delay is required to make sure sound card state updated */
	usleep_range(5000, 5100);

	snd_soc_write(codec, MSM89XX_PMIC_DIGITAL_INT_EN_SET,
				MSM89XX_PMIC_DIGITAL_INT_EN_SET__POR);
	snd_soc_write(codec, MSM89XX_PMIC_DIGITAL_INT_EN_CLR,
				MSM89XX_PMIC_DIGITAL_INT_EN_CLR__POR);

	msm_anlg_cdc_set_boost_v(codec);
	msm_anlg_cdc_set_micb_v(codec);
	if (sdm660_cdc_priv->boost_option == BOOST_ON_FOREVER)
		msm_anlg_cdc_boost_on(codec);
	else if (sdm660_cdc_priv->boost_option == BYPASS_ALWAYS)
		msm_anlg_cdc_bypass_on(codec);

	return 0;
}

static int sdm660_cdc_notifier_service_cb(struct notifier_block *nb,
					     unsigned long opcode, void *ptr)
{
	struct snd_soc_codec *codec;
	struct sdm660_cdc_priv *sdm660_cdc_priv =
				container_of(nb, struct sdm660_cdc_priv,
					     audio_ssr_nb);
	bool adsp_ready = false;
	bool timedout;
	unsigned long timeout;
	static bool initial_boot = true;

	codec = sdm660_cdc_priv->codec;
	dev_dbg(codec->dev, "%s: Service opcode 0x%lx\n", __func__, opcode);

	switch (opcode) {
	case AUDIO_NOTIFIER_SERVICE_DOWN:
		if (initial_boot) {
			initial_boot = false;
			break;
		}
		dev_dbg(codec->dev,
			"ADSP is about to power down. teardown/reset codec\n");
		msm_anlg_cdc_device_down(codec);
		break;
	case AUDIO_NOTIFIER_SERVICE_UP:
		if (initial_boot)
			initial_boot = false;
		dev_dbg(codec->dev,
			"ADSP is about to power up. bring up codec\n");

		if (!q6core_is_adsp_ready()) {
			dev_dbg(codec->dev,
				"ADSP isn't ready\n");
			timeout = jiffies +
				  msecs_to_jiffies(ADSP_STATE_READY_TIMEOUT_MS);
			while (!(timedout = time_after(jiffies, timeout))) {
				if (!q6core_is_adsp_ready()) {
					dev_dbg(codec->dev,
						"ADSP isn't ready\n");
				} else {
					dev_dbg(codec->dev,
						"ADSP is ready\n");
					adsp_ready = true;
					goto powerup;
				}
			}
		} else {
			adsp_ready = true;
			dev_dbg(codec->dev, "%s: DSP is ready\n", __func__);
		}
powerup:
		if (adsp_ready)
			msm_anlg_cdc_device_up(codec);
		break;
	default:
		break;
	}
	return NOTIFY_OK;
}

int msm_anlg_cdc_hs_detect(struct snd_soc_codec *codec,
			   struct wcd_mbhc_config *mbhc_cfg)
{
	struct sdm660_cdc_priv *sdm660_cdc_priv =
		snd_soc_codec_get_drvdata(codec);

	return wcd_mbhc_start(&sdm660_cdc_priv->mbhc, mbhc_cfg);
}
EXPORT_SYMBOL(msm_anlg_cdc_hs_detect);

void msm_anlg_cdc_hs_detect_exit(struct snd_soc_codec *codec)
{
	struct sdm660_cdc_priv *sdm660_cdc_priv =
		snd_soc_codec_get_drvdata(codec);

	wcd_mbhc_stop(&sdm660_cdc_priv->mbhc);
}
EXPORT_SYMBOL(msm_anlg_cdc_hs_detect_exit);

void msm_anlg_cdc_update_int_spk_boost(bool enable)
{
	pr_debug("%s: enable = %d\n", __func__, enable);
	spkr_boost_en = enable;
}
EXPORT_SYMBOL(msm_anlg_cdc_update_int_spk_boost);

static void msm_anlg_cdc_set_micb_v(struct snd_soc_codec *codec)
{

	struct sdm660_cdc_priv *sdm660_cdc = snd_soc_codec_get_drvdata(codec);
	struct sdm660_cdc_pdata *pdata = sdm660_cdc->dev->platform_data;
	u8 reg_val;

	reg_val = VOLTAGE_CONVERTER(pdata->micbias.cfilt1_mv, MICBIAS_MIN_VAL,
			MICBIAS_STEP_SIZE);
	dev_dbg(codec->dev, "cfilt1_mv %d reg_val %x\n",
			(u32)pdata->micbias.cfilt1_mv, reg_val);
	snd_soc_update_bits(codec, MSM89XX_PMIC_ANALOG_MICB_1_VAL,
			0xF8, (reg_val << 3));
}

static void msm_anlg_cdc_set_boost_v(struct snd_soc_codec *codec)
{
	struct sdm660_cdc_priv *sdm660_cdc_priv =
				snd_soc_codec_get_drvdata(codec);

	snd_soc_update_bits(codec, MSM89XX_PMIC_ANALOG_OUTPUT_VOLTAGE,
			0x1F, sdm660_cdc_priv->boost_voltage);
}

static void msm_anlg_cdc_configure_cap(struct snd_soc_codec *codec,
				       bool micbias1, bool micbias2)
{

	struct msm_asoc_mach_data *pdata = NULL;

	pdata = snd_soc_card_get_drvdata(codec->component.card);

	pr_debug("\n %s: micbias1 %x micbias2 = %d\n", __func__, micbias1,
			micbias2);
	if (micbias1 && micbias2) {
		if ((pdata->micbias1_cap_mode
		     == MICBIAS_EXT_BYP_CAP) ||
		    (pdata->micbias2_cap_mode
		     == MICBIAS_EXT_BYP_CAP))
			snd_soc_update_bits(codec,
				MSM89XX_PMIC_ANALOG_MICB_1_EN,
				0x40, (MICBIAS_EXT_BYP_CAP << 6));
		else
			snd_soc_update_bits(codec,
				MSM89XX_PMIC_ANALOG_MICB_1_EN,
				0x40, (MICBIAS_NO_EXT_BYP_CAP << 6));
	} else if (micbias2) {
		snd_soc_update_bits(codec, MSM89XX_PMIC_ANALOG_MICB_1_EN,
				0x40, (pdata->micbias2_cap_mode << 6));
	} else if (micbias1) {
		snd_soc_update_bits(codec, MSM89XX_PMIC_ANALOG_MICB_1_EN,
				0x40, (pdata->micbias1_cap_mode << 6));
	} else {
		snd_soc_update_bits(codec, MSM89XX_PMIC_ANALOG_MICB_1_EN,
				0x40, 0x00);
	}
}

static ssize_t msm_anlg_codec_version_read(struct snd_info_entry *entry,
					   void *file_private_data,
					   struct file *file,
					   char __user *buf, size_t count,
					   loff_t pos)
{
	struct sdm660_cdc_priv *sdm660_cdc_priv;
	char buffer[MSM_ANLG_CDC_VERSION_ENTRY_SIZE];
	int len = 0;

	sdm660_cdc_priv = (struct sdm660_cdc_priv *) entry->private_data;
	if (!sdm660_cdc_priv) {
		pr_err("%s: sdm660_cdc_priv is null\n", __func__);
		return -EINVAL;
	}

	switch (get_codec_version(sdm660_cdc_priv)) {
	case DRAX_CDC:
	    len = snprintf(buffer, sizeof(buffer), "DRAX-CDC_1_0\n");
	    break;
	default:
	    len = snprintf(buffer, sizeof(buffer), "VER_UNDEFINED\n");
	}

	return simple_read_from_buffer(buf, count, &pos, buffer, len);
}

static struct snd_info_entry_ops msm_anlg_codec_info_ops = {
	.read = msm_anlg_codec_version_read,
};

/*
 * msm_anlg_codec_info_create_codec_entry - creates pmic_analog module
 * @codec_root: The parent directory
 * @codec: Codec instance
 *
 * Creates pmic_analog module and version entry under the given
 * parent directory.
 *
 * Return: 0 on success or negative error code on failure.
 */
int msm_anlg_codec_info_create_codec_entry(struct snd_info_entry *codec_root,
					   struct snd_soc_codec *codec)
{
	struct snd_info_entry *version_entry;
	struct sdm660_cdc_priv *sdm660_cdc_priv;
	struct snd_soc_card *card;
	int ret;

	if (!codec_root || !codec)
		return -EINVAL;

	sdm660_cdc_priv = snd_soc_codec_get_drvdata(codec);
	card = codec->component.card;
	sdm660_cdc_priv->entry = snd_register_module_info(codec_root->module,
							     "spmi0-03",
							     codec_root);
	if (!sdm660_cdc_priv->entry) {
		dev_dbg(codec->dev, "%s: failed to create pmic_analog entry\n",
			__func__);
		return -ENOMEM;
	}

	version_entry = snd_info_create_card_entry(card->snd_card,
						   "version",
						   sdm660_cdc_priv->entry);
	if (!version_entry) {
		dev_dbg(codec->dev, "%s: failed to create pmic_analog version entry\n",
			__func__);
		return -ENOMEM;
	}

	version_entry->private_data = sdm660_cdc_priv;
	version_entry->size = MSM_ANLG_CDC_VERSION_ENTRY_SIZE;
	version_entry->content = SNDRV_INFO_CONTENT_DATA;
	version_entry->c.ops = &msm_anlg_codec_info_ops;

	if (snd_info_register(version_entry) < 0) {
		snd_info_free_entry(version_entry);
		return -ENOMEM;
	}
	sdm660_cdc_priv->version_entry = version_entry;

	sdm660_cdc_priv->audio_ssr_nb.notifier_call =
				sdm660_cdc_notifier_service_cb;
	ret = audio_notifier_register("pmic_analog_cdc",
				      AUDIO_NOTIFIER_ADSP_DOMAIN,
				      &sdm660_cdc_priv->audio_ssr_nb);
	if (ret < 0) {
		pr_err("%s: Audio notifier register failed ret = %d\n",
			__func__, ret);
		return ret;
	}
	return 0;
}
EXPORT_SYMBOL(msm_anlg_codec_info_create_codec_entry);

static int msm_anlg_cdc_soc_probe(struct snd_soc_codec *codec)
{
	struct sdm660_cdc_priv *sdm660_cdc;
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(codec);
	int ret;

	sdm660_cdc = dev_get_drvdata(codec->dev);
	sdm660_cdc->codec = codec;

	/* codec resmgr module init */
	sdm660_cdc->spkdrv_reg =
				msm_anlg_cdc_find_regulator(sdm660_cdc,
						MSM89XX_VDD_SPKDRV_NAME);
	sdm660_cdc->pmic_rev =
				snd_soc_read(codec,
					     MSM89XX_PMIC_DIGITAL_REVISION1);
	sdm660_cdc->codec_version =
				snd_soc_read(codec,
					MSM89XX_PMIC_DIGITAL_PERPH_SUBTYPE);
	sdm660_cdc->analog_major_rev =
				snd_soc_read(codec,
					     MSM89XX_PMIC_ANALOG_REVISION4);

	if (sdm660_cdc->codec_version == CONGA) {
		dev_dbg(codec->dev, "%s :Conga REV: %d\n", __func__,
					sdm660_cdc->codec_version);
		sdm660_cdc->ext_spk_boost_set = true;
	} else {
		dev_dbg(codec->dev, "%s :PMIC REV: %d\n", __func__,
					sdm660_cdc->pmic_rev);
		if (sdm660_cdc->pmic_rev == TOMBAK_1_0 &&
			sdm660_cdc->codec_version == CAJON_2_0) {
			if (sdm660_cdc->analog_major_rev == 0x02) {
				sdm660_cdc->codec_version = DRAX_CDC;
				dev_dbg(codec->dev,
					"%s : Drax codec detected\n", __func__);
			} else {
				sdm660_cdc->codec_version = DIANGU;
				dev_dbg(codec->dev, "%s : Diangu detected\n",
					__func__);
			}
		} else if (sdm660_cdc->pmic_rev == TOMBAK_1_0 &&
			(snd_soc_read(codec, MSM89XX_PMIC_ANALOG_NCP_FBCTRL)
			 & 0x80)) {
			sdm660_cdc->codec_version = CAJON;
			dev_dbg(codec->dev, "%s : Cajon detected\n", __func__);
		} else if (sdm660_cdc->pmic_rev == TOMBAK_2_0 &&
			(snd_soc_read(codec, MSM89XX_PMIC_ANALOG_NCP_FBCTRL)
			 & 0x80)) {
			sdm660_cdc->codec_version = CAJON_2_0;
			dev_dbg(codec->dev, "%s : Cajon 2.0 detected\n",
						__func__);
		}
	}
	/*
	 * set to default boost option BOOST_SWITCH, user mixer path can change
	 * it to BOOST_ALWAYS or BOOST_BYPASS based on solution chosen.
	 */
	sdm660_cdc->boost_option = BOOST_SWITCH;
	sdm660_cdc->hph_mode = NORMAL_MODE;

	msm_anlg_cdc_dt_parse_boost_info(codec);
	msm_anlg_cdc_set_boost_v(codec);

	snd_soc_add_codec_controls(codec, impedance_detect_controls,
				   ARRAY_SIZE(impedance_detect_controls));
	snd_soc_add_codec_controls(codec, hph_type_detect_controls,
				  ARRAY_SIZE(hph_type_detect_controls));

	msm_anlg_cdc_bringup(codec);
	msm_anlg_cdc_codec_init_reg(codec);
	msm_anlg_cdc_update_reg_defaults(codec);

	wcd9xxx_spmi_set_codec(codec);

	msm_anlg_cdc_update_micbias_regulator(
				sdm660_cdc,
				on_demand_supply_name[ON_DEMAND_MICBIAS],
				&sdm660_cdc->on_demand_list[ON_DEMAND_MICBIAS]);
	atomic_set(&sdm660_cdc->on_demand_list[ON_DEMAND_MICBIAS].ref,
		   0);

	sdm660_cdc->fw_data = devm_kzalloc(codec->dev,
					sizeof(*(sdm660_cdc->fw_data)),
					GFP_KERNEL);
	if (!sdm660_cdc->fw_data)
		return -ENOMEM;

	set_bit(WCD9XXX_MBHC_CAL, sdm660_cdc->fw_data->cal_bit);
	ret = wcd_cal_create_hwdep(sdm660_cdc->fw_data,
			WCD9XXX_CODEC_HWDEP_NODE, codec);
	if (ret < 0) {
		dev_err(codec->dev, "%s hwdep failed %d\n", __func__, ret);
		return ret;
	}

	wcd_mbhc_init(&sdm660_cdc->mbhc, codec, &mbhc_cb, &intr_ids,
		      wcd_mbhc_registers, true);

	sdm660_cdc->int_mclk0_enabled = false;
	/*Update speaker boost configuration*/
	sdm660_cdc->spk_boost_set = spkr_boost_en;
	pr_debug("%s: speaker boost configured = %d\n",
			__func__, sdm660_cdc->spk_boost_set);

	/* Set initial MICBIAS voltage level */
	msm_anlg_cdc_set_micb_v(codec);

	/* Set initial cap mode */
	msm_anlg_cdc_configure_cap(codec, false, false);

	snd_soc_dapm_ignore_suspend(dapm, "PDM Playback");
	snd_soc_dapm_ignore_suspend(dapm, "PDM Capture");

	snd_soc_dapm_sync(dapm);

	return 0;
}

static int msm_anlg_cdc_soc_remove(struct snd_soc_codec *codec)
{
	struct sdm660_cdc_priv *sdm660_cdc_priv =
					dev_get_drvdata(codec->dev);

	sdm660_cdc_priv->spkdrv_reg = NULL;
	sdm660_cdc_priv->on_demand_list[ON_DEMAND_MICBIAS].supply = NULL;
	atomic_set(&sdm660_cdc_priv->on_demand_list[ON_DEMAND_MICBIAS].ref,
		   0);
	wcd_mbhc_deinit(&sdm660_cdc_priv->mbhc);

	return 0;
}

static int msm_anlg_cdc_enable_static_supplies_to_optimum(
				struct sdm660_cdc_priv *sdm660_cdc,
				struct sdm660_cdc_pdata *pdata)
{
	int i;
	int ret = 0;

	for (i = 0; i < sdm660_cdc->num_of_supplies; i++) {
		if (pdata->regulator[i].ondemand)
			continue;
		if (regulator_count_voltages(
				sdm660_cdc->supplies[i].consumer) <= 0)
			continue;

		ret = regulator_set_voltage(
				sdm660_cdc->supplies[i].consumer,
				pdata->regulator[i].min_uv,
				pdata->regulator[i].max_uv);
		if (ret) {
			dev_err(sdm660_cdc->dev,
				"Setting volt failed for regulator %s err %d\n",
				sdm660_cdc->supplies[i].supply, ret);
		}

		ret = regulator_set_load(sdm660_cdc->supplies[i].consumer,
			pdata->regulator[i].optimum_ua);
		dev_dbg(sdm660_cdc->dev, "Regulator %s set optimum mode\n",
			 sdm660_cdc->supplies[i].supply);
	}

	return ret;
}

static int msm_anlg_cdc_disable_static_supplies_to_optimum(
			struct sdm660_cdc_priv *sdm660_cdc,
			struct sdm660_cdc_pdata *pdata)
{
	int i;
	int ret = 0;

	for (i = 0; i < sdm660_cdc->num_of_supplies; i++) {
		if (pdata->regulator[i].ondemand)
			continue;
		if (regulator_count_voltages(
				sdm660_cdc->supplies[i].consumer) <= 0)
			continue;
		regulator_set_voltage(sdm660_cdc->supplies[i].consumer, 0,
				pdata->regulator[i].max_uv);
		regulator_set_load(sdm660_cdc->supplies[i].consumer, 0);
		dev_dbg(sdm660_cdc->dev, "Regulator %s set optimum mode\n",
				 sdm660_cdc->supplies[i].supply);
	}

	return ret;
}

static int msm_anlg_cdc_suspend(struct snd_soc_codec *codec)
{
	struct sdm660_cdc_priv *sdm660_cdc = snd_soc_codec_get_drvdata(codec);
	struct sdm660_cdc_pdata *sdm660_cdc_pdata =
					sdm660_cdc->dev->platform_data;

	msm_anlg_cdc_disable_static_supplies_to_optimum(sdm660_cdc,
							sdm660_cdc_pdata);
	return 0;
}

static int msm_anlg_cdc_resume(struct snd_soc_codec *codec)
{
	struct msm_asoc_mach_data *pdata = NULL;
	struct sdm660_cdc_priv *sdm660_cdc = snd_soc_codec_get_drvdata(codec);
	struct sdm660_cdc_pdata *sdm660_cdc_pdata =
					sdm660_cdc->dev->platform_data;

	pdata = snd_soc_card_get_drvdata(codec->component.card);
	msm_anlg_cdc_enable_static_supplies_to_optimum(sdm660_cdc,
						       sdm660_cdc_pdata);
	return 0;
}

static struct regmap *msm_anlg_get_regmap(struct device *dev)
{
	return dev_get_regmap(dev->parent, NULL);
}

static struct snd_soc_codec_driver soc_codec_dev_sdm660_cdc = {
	.probe	= msm_anlg_cdc_soc_probe,
	.remove	= msm_anlg_cdc_soc_remove,
	.suspend = msm_anlg_cdc_suspend,
	.resume = msm_anlg_cdc_resume,
	.reg_word_size = 1,
	.controls = msm_anlg_cdc_snd_controls,
	.num_controls = ARRAY_SIZE(msm_anlg_cdc_snd_controls),
	.dapm_widgets = msm_anlg_cdc_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(msm_anlg_cdc_dapm_widgets),
	.dapm_routes = audio_map,
	.num_dapm_routes = ARRAY_SIZE(audio_map),
	.get_regmap = msm_anlg_get_regmap,
};

static int msm_anlg_cdc_init_supplies(struct sdm660_cdc_priv *sdm660_cdc,
				struct sdm660_cdc_pdata *pdata)
{
	int ret;
	int i;

	sdm660_cdc->supplies = devm_kzalloc(sdm660_cdc->dev,
					sizeof(struct regulator_bulk_data) *
					ARRAY_SIZE(pdata->regulator),
					GFP_KERNEL);
	if (!sdm660_cdc->supplies) {
		ret = -ENOMEM;
		goto err;
	}

	sdm660_cdc->num_of_supplies = 0;
	if (ARRAY_SIZE(pdata->regulator) > MAX_REGULATOR) {
		dev_err(sdm660_cdc->dev, "%s: Array Size out of bound\n",
			__func__);
		ret = -EINVAL;
		goto err;
	}

	for (i = 0; i < ARRAY_SIZE(pdata->regulator); i++) {
		if (pdata->regulator[i].name) {
			sdm660_cdc->supplies[i].supply =
						pdata->regulator[i].name;
			sdm660_cdc->num_of_supplies++;
		}
	}

	ret = devm_regulator_bulk_get(sdm660_cdc->dev,
				      sdm660_cdc->num_of_supplies,
				      sdm660_cdc->supplies);
	if (ret != 0) {
		dev_err(sdm660_cdc->dev,
			"Failed to get supplies: err = %d\n",
			ret);
		goto err_supplies;
	}

	for (i = 0; i < sdm660_cdc->num_of_supplies; i++) {
		if (regulator_count_voltages(
			sdm660_cdc->supplies[i].consumer) <= 0)
			continue;
		if (pdata->regulator[i].ondemand) {
			ret = regulator_set_voltage(
					sdm660_cdc->supplies[i].consumer,
					0, pdata->regulator[i].max_uv);
			if (ret) {
				dev_err(sdm660_cdc->dev,
					"Setting regulator voltage failed for regulator %s err = %d\n",
					sdm660_cdc->supplies[i].supply, ret);
				goto err_supplies;
			}
			ret = regulator_set_load(
				sdm660_cdc->supplies[i].consumer, 0);
			if (ret < 0) {
				dev_err(sdm660_cdc->dev,
					"Setting regulator optimum mode failed for regulator %s err = %d\n",
					sdm660_cdc->supplies[i].supply, ret);
				goto err_supplies;
			} else {
				ret = 0;
				continue;
			}
		}
		ret = regulator_set_voltage(sdm660_cdc->supplies[i].consumer,
					    pdata->regulator[i].min_uv,
					    pdata->regulator[i].max_uv);
		if (ret) {
			dev_err(sdm660_cdc->dev,
				"Setting regulator voltage failed for regulator %s err = %d\n",
				sdm660_cdc->supplies[i].supply, ret);
			goto err_supplies;
		}
		ret = regulator_set_load(sdm660_cdc->supplies[i].consumer,
					 pdata->regulator[i].optimum_ua);
		if (ret < 0) {
			dev_err(sdm660_cdc->dev,
				"Setting regulator optimum mode failed for regulator %s err = %d\n",
				sdm660_cdc->supplies[i].supply, ret);
			goto err_supplies;
		} else {
			ret = 0;
		}
	}

	return ret;

err_supplies:
	kfree(sdm660_cdc->supplies);
err:
	return ret;
}

static int msm_anlg_cdc_enable_static_supplies(
					struct sdm660_cdc_priv *sdm660_cdc,
					struct sdm660_cdc_pdata *pdata)
{
	int i;
	int ret = 0;

	for (i = 0; i < sdm660_cdc->num_of_supplies; i++) {
		if (pdata->regulator[i].ondemand)
			continue;
		ret = regulator_enable(sdm660_cdc->supplies[i].consumer);
		if (ret) {
			dev_err(sdm660_cdc->dev, "Failed to enable %s\n",
			       sdm660_cdc->supplies[i].supply);
			break;
		}
		dev_dbg(sdm660_cdc->dev, "Enabled regulator %s\n",
				 sdm660_cdc->supplies[i].supply);
	}

	while (ret && --i)
		if (!pdata->regulator[i].ondemand)
			regulator_disable(sdm660_cdc->supplies[i].consumer);
	return ret;
}

static void msm_anlg_cdc_disable_supplies(struct sdm660_cdc_priv *sdm660_cdc,
				     struct sdm660_cdc_pdata *pdata)
{
	int i;

	regulator_bulk_disable(sdm660_cdc->num_of_supplies,
			       sdm660_cdc->supplies);
	for (i = 0; i < sdm660_cdc->num_of_supplies; i++) {
		if (regulator_count_voltages(
				sdm660_cdc->supplies[i].consumer) <= 0)
			continue;
		regulator_set_voltage(sdm660_cdc->supplies[i].consumer, 0,
				pdata->regulator[i].max_uv);
		regulator_set_load(sdm660_cdc->supplies[i].consumer, 0);
	}
	regulator_bulk_free(sdm660_cdc->num_of_supplies,
			    sdm660_cdc->supplies);
	kfree(sdm660_cdc->supplies);
}

static const struct of_device_id sdm660_codec_of_match[] = {
	{ .compatible = "qcom,pmic-analog-codec", },
	{},
};

static void msm_anlg_add_child_devices(struct work_struct *work)
{
	struct sdm660_cdc_priv *pdata;
	struct platform_device *pdev;
	struct device_node *node;
	struct msm_dig_ctrl_data *dig_ctrl_data = NULL, *temp;
	int ret, ctrl_num = 0;
	struct msm_dig_ctrl_platform_data *platdata;
	char plat_dev_name[MSM_DIG_CDC_STRING_LEN];

	pdata = container_of(work, struct sdm660_cdc_priv,
			     msm_anlg_add_child_devices_work);
	if (!pdata) {
		pr_err("%s: Memory for pdata does not exist\n",
			__func__);
		return;
	}
	if (!pdata->dev->of_node) {
		dev_err(pdata->dev,
			"%s: DT node for pdata does not exist\n", __func__);
		return;
	}

	platdata = &pdata->dig_plat_data;

	for_each_child_of_node(pdata->dev->of_node, node) {
		if (!strcmp(node->name, "msm-dig-codec"))
			strlcpy(plat_dev_name, "msm_digital_codec",
				(MSM_DIG_CDC_STRING_LEN - 1));
		else
			continue;

		pdev = platform_device_alloc(plat_dev_name, -1);
		if (!pdev) {
			dev_err(pdata->dev, "%s: pdev memory alloc failed\n",
				__func__);
			ret = -ENOMEM;
			goto err;
		}
		pdev->dev.parent = pdata->dev;
		pdev->dev.of_node = node;

		if (!strcmp(node->name, "msm-dig-codec")) {
			ret = platform_device_add_data(pdev, platdata,
						       sizeof(*platdata));
			if (ret) {
				dev_err(&pdev->dev,
					"%s: cannot add plat data ctrl:%d\n",
					__func__, ctrl_num);
				goto fail_pdev_add;
			}
		}

		ret = platform_device_add(pdev);
		if (ret) {
			dev_err(&pdev->dev,
				"%s: Cannot add platform device\n",
				__func__);
			goto fail_pdev_add;
		}

		if (!strcmp(node->name, "msm-dig-codec")) {
			temp = krealloc(dig_ctrl_data,
					(ctrl_num + 1) * sizeof(
					struct msm_dig_ctrl_data),
					GFP_KERNEL);
			if (!temp) {
				dev_err(&pdev->dev, "out of memory\n");
				ret = -ENOMEM;
				goto err;
			}
			dig_ctrl_data = temp;
			dig_ctrl_data[ctrl_num].dig_pdev = pdev;
			ctrl_num++;
			dev_dbg(&pdev->dev,
				"%s: Added digital codec device(s)\n",
				__func__);
			pdata->dig_ctrl_data = dig_ctrl_data;
		}
	}

	return;
fail_pdev_add:
	platform_device_put(pdev);
err:
	return;
}

static int msm_anlg_cdc_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct sdm660_cdc_priv *sdm660_cdc = NULL;
	struct sdm660_cdc_pdata *pdata;
	int adsp_state;

	adsp_state = apr_get_subsys_state();
	if (adsp_state != APR_SUBSYS_LOADED) {
		dev_err(&pdev->dev, "Adsp is not loaded yet %d\n",
			adsp_state);
		return -EPROBE_DEFER;
	}
	device_init_wakeup(&pdev->dev, true);

	if (pdev->dev.of_node) {
		dev_dbg(&pdev->dev, "%s:Platform data from device tree\n",
			__func__);
		pdata = msm_anlg_cdc_populate_dt_pdata(&pdev->dev);
		pdev->dev.platform_data = pdata;
	} else {
		dev_dbg(&pdev->dev, "%s:Platform data from board file\n",
			__func__);
		pdata = pdev->dev.platform_data;
	}
	if (pdata == NULL) {
		dev_err(&pdev->dev, "%s:Platform data failed to populate\n",
			__func__);
		goto rtn;
	}
	sdm660_cdc = devm_kzalloc(&pdev->dev, sizeof(struct sdm660_cdc_priv),
				     GFP_KERNEL);
	if (sdm660_cdc == NULL) {
		ret = -ENOMEM;
		goto rtn;
	}

	sdm660_cdc->dev = &pdev->dev;
	ret = msm_anlg_cdc_init_supplies(sdm660_cdc, pdata);
	if (ret) {
		dev_err(&pdev->dev, "%s: Fail to enable Codec supplies\n",
			__func__);
		goto rtn;
	}
	ret = msm_anlg_cdc_enable_static_supplies(sdm660_cdc, pdata);
	if (ret) {
		dev_err(&pdev->dev,
			"%s: Fail to enable Codec pre-reset supplies\n",
			__func__);
		goto rtn;
	}
	/* Allow supplies to be ready */
	usleep_range(5, 6);

	wcd9xxx_spmi_set_dev(pdev, 0);
	wcd9xxx_spmi_set_dev(pdev, 1);
	if (wcd9xxx_spmi_irq_init()) {
		dev_err(&pdev->dev,
			"%s: irq initialization failed\n", __func__);
	} else {
		dev_dbg(&pdev->dev,
			"%s: irq initialization passed\n", __func__);
	}
	dev_set_drvdata(&pdev->dev, sdm660_cdc);

	ret = snd_soc_register_codec(&pdev->dev,
				     &soc_codec_dev_sdm660_cdc,
				     msm_anlg_cdc_i2s_dai,
				     ARRAY_SIZE(msm_anlg_cdc_i2s_dai));
	if (ret) {
		dev_err(&pdev->dev,
			"%s:snd_soc_register_codec failed with error %d\n",
			__func__, ret);
		goto err_supplies;
	}
	BLOCKING_INIT_NOTIFIER_HEAD(&sdm660_cdc->notifier);
	BLOCKING_INIT_NOTIFIER_HEAD(&sdm660_cdc->notifier_mbhc);

	sdm660_cdc->dig_plat_data.handle = (void *) sdm660_cdc;
	sdm660_cdc->dig_plat_data.set_compander_mode = set_compander_mode;
	sdm660_cdc->dig_plat_data.update_clkdiv = update_clkdiv;
	sdm660_cdc->dig_plat_data.get_cdc_version = get_cdc_version;
	sdm660_cdc->dig_plat_data.register_notifier =
					msm_anlg_cdc_dig_register_notifier;
	INIT_WORK(&sdm660_cdc->msm_anlg_add_child_devices_work,
		  msm_anlg_add_child_devices);
	schedule_work(&sdm660_cdc->msm_anlg_add_child_devices_work);

	return ret;
err_supplies:
	msm_anlg_cdc_disable_supplies(sdm660_cdc, pdata);
rtn:
	return ret;
}

static int msm_anlg_cdc_remove(struct platform_device *pdev)
{
	struct sdm660_cdc_priv *sdm660_cdc = dev_get_drvdata(&pdev->dev);
	struct sdm660_cdc_pdata *pdata = sdm660_cdc->dev->platform_data;

	snd_soc_unregister_codec(&pdev->dev);
	msm_anlg_cdc_disable_supplies(sdm660_cdc, pdata);
	return 0;
}

static struct platform_driver msm_anlg_codec_driver = {
	.driver		= {
		.owner          = THIS_MODULE,
		.name           = DRV_NAME,
		.of_match_table = of_match_ptr(sdm660_codec_of_match)
	},
	.probe          = msm_anlg_cdc_probe,
	.remove         = msm_anlg_cdc_remove,
};
module_platform_driver(msm_anlg_codec_driver);

MODULE_DESCRIPTION("MSM Audio Analog codec driver");
MODULE_LICENSE("GPL v2");
