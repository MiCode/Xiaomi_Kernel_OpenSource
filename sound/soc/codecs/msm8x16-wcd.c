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
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/printk.h>
#include <linux/ratelimit.h>
#include <linux/debugfs.h>
#include <linux/io.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/pm_runtime.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/spmi.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/mfd/wcd9xxx/core.h>
#include <linux/qdsp6v2/apr.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <sound/q6afe-v2.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include <sound/q6core.h>
#include <soc/qcom/subsystem_notif.h>
#include "msm8x16-wcd.h"
#include "wcd-mbhc-v2.h"
#include "msm8916-wcd-irq.h"
#include "msm8x16_wcd_registers.h"

#define MSM8X16_WCD_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |\
			SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000)
#define MSM8X16_WCD_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
		SNDRV_PCM_FMTBIT_S24_LE |\
		SNDRV_PCM_FMTBIT_S24_3LE)

#define NUM_INTERPOLATORS	3
#define BITS_PER_REG		8
#define MSM8X16_WCD_TX_PORT_NUMBER	4

#define MSM8X16_WCD_I2S_MASTER_MODE_MASK	0x08
#define MSM8X16_DIGITAL_CODEC_BASE_ADDR		0x771C000
#define TOMBAK_CORE_0_SPMI_ADDR			0xf000
#define TOMBAK_CORE_1_SPMI_ADDR			0xf100
#define PMIC_SLAVE_ID_0		0
#define PMIC_SLAVE_ID_1		1

#define PMIC_MBG_OK		0x2C08
#define PMIC_LDO7_EN_CTL	0x4646
#define MASK_MSB_BIT		0x80

#define CODEC_DT_MAX_PROP_SIZE			40
#define MSM8X16_DIGITAL_CODEC_REG_SIZE		0x400
#define MAX_ON_DEMAND_SUPPLY_NAME_LENGTH	64

#define MCLK_RATE_9P6MHZ	9600000
#define MCLK_RATE_12P288MHZ	12288000

#define BUS_DOWN 1

/*
 *50 Milliseconds sufficient for DSP bring up in the modem
 * after Sub System Restart
 */
#define ADSP_STATE_READY_TIMEOUT_MS 50

#define HPHL_PA_DISABLE (0x01 << 1)
#define HPHR_PA_DISABLE (0x01 << 2)
#define EAR_PA_DISABLE (0x01 << 3)
#define SPKR_PA_DISABLE (0x01 << 4)

enum {
	BOOST_SWITCH = 0,
	BOOST_ALWAYS,
	BYPASS_ALWAYS,
	BOOST_ON_FOREVER,
};

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

#define MSM8X16_WCD_MBHC_BTN_COARSE_ADJ  100 /* in mV */
#define MSM8X16_WCD_MBHC_BTN_FINE_ADJ 12 /* in mV */

#define VOLTAGE_CONVERTER(value, min_value, step_size)\
	((value - min_value)/step_size)

enum {
	AIF1_PB = 0,
	AIF1_CAP,
	AIF2_VIFEED,
	NUM_CODEC_DAIS,
};

enum {
	RX_MIX1_INP_SEL_ZERO = 0,
	RX_MIX1_INP_SEL_IIR1,
	RX_MIX1_INP_SEL_IIR2,
	RX_MIX1_INP_SEL_RX1,
	RX_MIX1_INP_SEL_RX2,
	RX_MIX1_INP_SEL_RX3,
};

static const DECLARE_TLV_DB_SCALE(digital_gain, 0, 1, 0);
static const DECLARE_TLV_DB_SCALE(analog_gain, 0, 25, 1);
static struct snd_soc_dai_driver msm8x16_wcd_i2s_dai[];
/* By default enable the internal speaker boost */
static bool spkr_boost_en = true;

#define MSM8X16_WCD_ACQUIRE_LOCK(x) \
	mutex_lock_nested(&x, SINGLE_DEPTH_NESTING)

#define MSM8X16_WCD_RELEASE_LOCK(x) mutex_unlock(&x)


/* Codec supports 2 IIR filters */
enum {
	IIR1 = 0,
	IIR2,
	IIR_MAX,
};

/* Codec supports 5 bands */
enum {
	BAND1 = 0,
	BAND2,
	BAND3,
	BAND4,
	BAND5,
	BAND_MAX,
};

struct hpf_work {
	struct msm8x16_wcd_priv *msm8x16_wcd;
	u32 decimator;
	u8 tx_hpf_cut_of_freq;
	struct delayed_work dwork;
};

static struct hpf_work tx_hpf_work[NUM_DECIMATORS];

static char on_demand_supply_name[][MAX_ON_DEMAND_SUPPLY_NAME_LENGTH] = {
	"cdc-vdd-mic-bias",
};

static unsigned long rx_digital_gain_reg[] = {
	MSM8X16_WCD_A_CDC_RX1_VOL_CTL_B2_CTL,
	MSM8X16_WCD_A_CDC_RX2_VOL_CTL_B2_CTL,
	MSM8X16_WCD_A_CDC_RX3_VOL_CTL_B2_CTL,
};

static unsigned long tx_digital_gain_reg[] = {
	MSM8X16_WCD_A_CDC_TX1_VOL_CTL_GAIN,
	MSM8X16_WCD_A_CDC_TX2_VOL_CTL_GAIN,
};

enum {
	MSM8X16_WCD_SPMI_DIGITAL = 0,
	MSM8X16_WCD_SPMI_ANALOG,
	MAX_MSM8X16_WCD_DEVICE
};

static struct wcd_mbhc_register
	wcd_mbhc_registers[WCD_MBHC_REG_FUNC_MAX] = {

	WCD_MBHC_REGISTER("WCD_MBHC_L_DET_EN",
			  MSM8X16_WCD_A_ANALOG_MBHC_DET_CTL_1, 0x80, 7, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_GND_DET_EN",
			  MSM8X16_WCD_A_ANALOG_MBHC_DET_CTL_1, 0x40, 6, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_MECH_DETECTION_TYPE",
			  MSM8X16_WCD_A_ANALOG_MBHC_DET_CTL_1, 0x20, 5, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_MIC_CLAMP_CTL",
			  MSM8X16_WCD_A_ANALOG_MBHC_DET_CTL_1, 0x18, 3, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_ELECT_DETECTION_TYPE",
			  MSM8X16_WCD_A_ANALOG_MBHC_DET_CTL_1, 0x01, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HS_L_DET_PULL_UP_CTRL",
			  MSM8X16_WCD_A_ANALOG_MBHC_DET_CTL_2, 0xC0, 6, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HS_L_DET_PULL_UP_COMP_CTRL",
			  MSM8X16_WCD_A_ANALOG_MBHC_DET_CTL_2, 0x20, 5, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHL_PLUG_TYPE",
			  MSM8X16_WCD_A_ANALOG_MBHC_DET_CTL_2, 0x10, 4, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_GND_PLUG_TYPE",
			  MSM8X16_WCD_A_ANALOG_MBHC_DET_CTL_2, 0x08, 3, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_SW_HPH_LP_100K_TO_GND",
			  MSM8X16_WCD_A_ANALOG_MBHC_DET_CTL_2, 0x01, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_ELECT_SCHMT_ISRC",
			  MSM8X16_WCD_A_ANALOG_MBHC_DET_CTL_2, 0x06, 1, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_FSM_EN",
			  MSM8X16_WCD_A_ANALOG_MBHC_FSM_CTL, 0x80, 7, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_INSREM_DBNC",
			  MSM8X16_WCD_A_ANALOG_MBHC_DBNC_TIMER, 0xF0, 4, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_BTN_DBNC",
			  MSM8X16_WCD_A_ANALOG_MBHC_DBNC_TIMER, 0x0C, 2, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HS_VREF",
			  MSM8X16_WCD_A_ANALOG_MBHC_BTN3_CTL, 0x03, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HS_COMP_RESULT",
			  MSM8X16_WCD_A_ANALOG_MBHC_ZDET_ELECT_RESULT, 0x01,
			  0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_MIC_SCHMT_RESULT",
			  MSM8X16_WCD_A_ANALOG_MBHC_ZDET_ELECT_RESULT, 0x02,
			  1, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHL_SCHMT_RESULT",
			  MSM8X16_WCD_A_ANALOG_MBHC_ZDET_ELECT_RESULT, 0x08,
			  3, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHR_SCHMT_RESULT",
			  MSM8X16_WCD_A_ANALOG_MBHC_ZDET_ELECT_RESULT, 0x04,
			  2, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_OCP_FSM_EN",
			  MSM8X16_WCD_A_ANALOG_RX_COM_OCP_CTL, 0x10, 4, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_BTN_RESULT",
			  MSM8X16_WCD_A_ANALOG_MBHC_BTN_RESULT, 0xFF, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_BTN_ISRC_CTL",
			  MSM8X16_WCD_A_ANALOG_MBHC_FSM_CTL, 0x70, 4, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_ELECT_RESULT",
			  MSM8X16_WCD_A_ANALOG_MBHC_ZDET_ELECT_RESULT, 0xFF,
			  0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_MICB_CTRL",
			  MSM8X16_WCD_A_ANALOG_MICB_2_EN, 0xC0, 6, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPH_CNP_WG_TIME",
			  MSM8X16_WCD_A_ANALOG_RX_HPH_CNP_WG_TIME, 0xFC, 2, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHR_PA_EN",
			  MSM8X16_WCD_A_ANALOG_RX_HPH_CNP_EN, 0x10, 4, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPHL_PA_EN",
			  MSM8X16_WCD_A_ANALOG_RX_HPH_CNP_EN, 0x20, 5, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_HPH_PA_EN",
			  MSM8X16_WCD_A_ANALOG_RX_HPH_CNP_EN, 0x30, 4, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_SWCH_LEVEL_REMOVE",
			  MSM8X16_WCD_A_ANALOG_MBHC_ZDET_ELECT_RESULT,
			  0x10, 4, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_MOISTURE_VREF",
			  0, 0, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_PULLDOWN_CTRL",
			  MSM8X16_WCD_A_ANALOG_MICB_2_EN, 0x20, 5, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_ANC_DET_EN",
			  0, 0, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_FSM_STATUS",
			  0, 0, 0, 0),
	WCD_MBHC_REGISTER("WCD_MBHC_MUX_CTL",
			  0, 0, 0, 0),
};

struct msm8x16_wcd_spmi {
	struct spmi_device *spmi;
	int base;
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
	.mbhc_sw_intr =  MSM8X16_WCD_IRQ_MBHC_HS_DET,
	.mbhc_btn_press_intr = MSM8X16_WCD_IRQ_MBHC_PRESS,
	.mbhc_btn_release_intr = MSM8X16_WCD_IRQ_MBHC_RELEASE,
	.mbhc_hs_ins_intr = MSM8X16_WCD_IRQ_MBHC_INSREM_DET1,
	.mbhc_hs_rem_intr = MSM8X16_WCD_IRQ_MBHC_INSREM_DET,
	.hph_left_ocp = MSM8X16_WCD_IRQ_HPHL_OCP,
	.hph_right_ocp = MSM8X16_WCD_IRQ_HPHR_OCP,
};

static int msm8x16_wcd_dt_parse_vreg_info(struct device *dev,
	struct msm8x16_wcd_regulator *vreg,
	const char *vreg_name, bool ondemand);
static struct msm8x16_wcd_pdata *msm8x16_wcd_populate_dt_pdata(
	struct device *dev);
static int msm8x16_wcd_enable_ext_mb_source(struct snd_soc_codec *codec,
					    bool turn_on);
static void msm8x16_trim_btn_reg(struct snd_soc_codec *codec);
static void msm8x16_wcd_set_micb_v(struct snd_soc_codec *codec);
static void msm8x16_wcd_set_boost_v(struct snd_soc_codec *codec);
static void msm8x16_wcd_set_auto_zeroing(struct snd_soc_codec *codec,
		bool enable);
static void msm8x16_wcd_configure_cap(struct snd_soc_codec *codec,
		bool micbias1, bool micbias2);
static bool msm8x16_wcd_use_mb(struct snd_soc_codec *codec);

struct msm8x16_wcd_spmi msm8x16_wcd_modules[MAX_MSM8X16_WCD_DEVICE];

static void *adsp_state_notifier;

static struct snd_soc_codec *registered_codec;

static int get_codec_version(struct msm8x16_wcd_priv *msm8x16_wcd)
{
	if (msm8x16_wcd->codec_version == DIANGU)
		return DIANGU;
	else if (msm8x16_wcd->codec_version == CAJON_2_0)
		return CAJON_2_0;
	else if (msm8x16_wcd->codec_version == CAJON)
		return CAJON;
	else if (msm8x16_wcd->codec_version == CONGA)
		return CONGA;
	else if (msm8x16_wcd->pmic_rev == TOMBAK_2_0)
		return TOMBAK_2_0;
	else if (msm8x16_wcd->pmic_rev == TOMBAK_1_0)
		return TOMBAK_1_0;

	pr_err("%s: unsupported codec version\n", __func__);
	return UNSUPPORTED;
}

static void wcd_mbhc_meas_imped(struct snd_soc_codec *codec,
				s16 *impedance_l, s16 *impedance_r)
{
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	if ((msm8x16_wcd->imped_det_pin == WCD_MBHC_DET_BOTH) ||
		(msm8x16_wcd->imped_det_pin == WCD_MBHC_DET_HPHL)) {
		/* Enable ZDET_L_MEAS_EN */
		snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_MBHC_FSM_CTL,
				0x08, 0x08);
		/* Wait for 2ms for measurement to complete */
		usleep_range(2000, 2100);
		/* Read Left impedance value from Result1 */
		*impedance_l = snd_soc_read(codec,
				MSM8X16_WCD_A_ANALOG_MBHC_BTN_RESULT);
		/* Enable ZDET_R_MEAS_EN */
		snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_MBHC_FSM_CTL,
				0x08, 0x00);
	}
	if ((msm8x16_wcd->imped_det_pin == WCD_MBHC_DET_BOTH) ||
		(msm8x16_wcd->imped_det_pin == WCD_MBHC_DET_HPHR)) {
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_MBHC_FSM_CTL,
				0x04, 0x04);
		/* Wait for 2ms for measurement to complete */
		usleep_range(2000, 2100);
		/* Read Right impedance value from Result1 */
		*impedance_r = snd_soc_read(codec,
				MSM8X16_WCD_A_ANALOG_MBHC_BTN_RESULT);
		snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_MBHC_FSM_CTL,
				0x04, 0x00);
	}
}

static void msm8x16_set_ref_current(struct snd_soc_codec *codec,
				enum wcd_curr_ref curr_ref)
{
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s: curr_ref: %d\n", __func__, curr_ref);

	if (get_codec_version(msm8x16_wcd) < CAJON)
		pr_debug("%s: Setting ref current not required\n", __func__);

	msm8x16_wcd->imped_i_ref = imped_i_ref[curr_ref];

	switch (curr_ref) {
	case I_h4_UA:
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MICB_2_EN,
			0x07, 0x01);
		break;
	case I_pt5_UA:
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MICB_2_EN,
			0x07, 0x04);
		break;
	case I_14_UA:
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MICB_2_EN,
			0x07, 0x03);
		break;
	case I_l4_UA:
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MICB_2_EN,
			0x07, 0x01);
		break;
	case I_1_UA:
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MICB_2_EN,
			0x07, 0x00);
		break;
	default:
		pr_debug("%s: No ref current set\n", __func__);
		break;
	}
}

static bool msm8x16_adj_ref_current(struct snd_soc_codec *codec,
					s16 *impedance_l, s16 *impedance_r)
{
	int i = 2;
	s16 compare_imp = 0;

	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	if (msm8x16_wcd->imped_det_pin == WCD_MBHC_DET_HPHR)
		compare_imp = *impedance_r;
	else
		compare_imp = *impedance_l;

	if (get_codec_version(msm8x16_wcd) < CAJON) {
		pr_debug("%s: Reference current adjustment not required\n",
			 __func__);
		return false;
	}

	while (compare_imp < imped_i_ref[i].min_val) {
		msm8x16_set_ref_current(codec,
					imped_i_ref[++i].curr_ref);
		wcd_mbhc_meas_imped(codec,
				impedance_l, impedance_r);
		compare_imp = (msm8x16_wcd->imped_det_pin == WCD_MBHC_DET_HPHR)
				? *impedance_r : *impedance_l;
		if (i >= I_1_UA)
			break;
	}

	return true;
}

void msm8x16_wcd_spk_ext_pa_cb(
		int (*codec_spk_ext_pa)(struct snd_soc_codec *codec,
			int enable), struct snd_soc_codec *codec)
{
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s: Enter\n", __func__);
	msm8x16_wcd->codec_spk_ext_pa_cb = codec_spk_ext_pa;
}

void msm8x16_wcd_hph_comp_cb(
	int (*codec_hph_comp_gpio)(bool enable), struct snd_soc_codec *codec)
{
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s: Enter\n", __func__);
	msm8x16_wcd->codec_hph_comp_gpio = codec_hph_comp_gpio;
}

static void msm8x16_wcd_compute_impedance(struct snd_soc_codec *codec, s16 l,
				s16 r, uint32_t *zl, uint32_t *zr, bool high)
{
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);
	uint32_t rl = 0, rr = 0;
	struct wcd_imped_i_ref R = msm8x16_wcd->imped_i_ref;
	int codec_ver = get_codec_version(msm8x16_wcd);

	switch (codec_ver) {
	case TOMBAK_1_0:
	case TOMBAK_2_0:
	case CONGA:
		if (high) {
			pr_debug("%s: This plug has high range impedance\n",
				 __func__);
			rl = (uint32_t)(((100 * (l * 400 - 200))/96) - 230);
			rr = (uint32_t)(((100 * (r * 400 - 200))/96) - 230);
		} else {
			pr_debug("%s: This plug has low range impedance\n",
				 __func__);
			rl = (uint32_t)(((1000 * (l * 2 - 1))/1165) - (13/10));
			rr = (uint32_t)(((1000 * (r * 2 - 1))/1165) - (13/10));
		}
		break;
	case CAJON:
	case CAJON_2_0:
	case DIANGU:
		if (msm8x16_wcd->imped_det_pin == WCD_MBHC_DET_HPHL) {
			rr = (uint32_t)(((DEFAULT_MULTIPLIER * (10 * r - 5)) -
			   (DEFAULT_OFFSET * DEFAULT_GAIN))/DEFAULT_GAIN);
			rl = (uint32_t)(((10000 * (R.multiplier * (10 * l - 5)))
			      - R.offset * R.gain_adj)/(R.gain_adj * 100));
		} else if (msm8x16_wcd->imped_det_pin == WCD_MBHC_DET_HPHR) {
			rr = (uint32_t)(((10000 * (R.multiplier * (10 * r - 5)))
			      - R.offset * R.gain_adj)/(R.gain_adj * 100));
			rl = (uint32_t)(((DEFAULT_MULTIPLIER * (10 * l - 5))-
			   (DEFAULT_OFFSET * DEFAULT_GAIN))/DEFAULT_GAIN);
		} else if (msm8x16_wcd->imped_det_pin == WCD_MBHC_DET_NONE) {
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
		pr_debug("%s: No codec mentioned\n", __func__);
		break;
	}
	*zl = rl;
	*zr = rr;
}

static struct firmware_cal *msm8x16_wcd_get_hwdep_fw_cal(
		struct snd_soc_codec *codec,
		enum wcd_cal_type type)
{
	struct msm8x16_wcd_priv *msm8x16_wcd;
	struct firmware_cal *hwdep_cal;

	if (!codec) {
		pr_err("%s: NULL codec pointer\n", __func__);
		return NULL;
	}
	msm8x16_wcd = snd_soc_codec_get_drvdata(codec);
	hwdep_cal = wcdcal_get_fw_cal(msm8x16_wcd->fw_data, type);
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

static void msm8x16_mbhc_clk_setup(struct snd_soc_codec *codec,
				   bool enable)
{
	if (enable)
		snd_soc_update_bits(codec,
				MSM8X16_WCD_A_DIGITAL_CDC_DIG_CLK_CTL,
				0x08, 0x08);
	else
		snd_soc_update_bits(codec,
				MSM8X16_WCD_A_DIGITAL_CDC_DIG_CLK_CTL,
				0x08, 0x00);
}

static int msm8x16_mbhc_map_btn_code_to_num(struct snd_soc_codec *codec)
{
	int btn_code;
	int btn;

	btn_code = snd_soc_read(codec, MSM8X16_WCD_A_ANALOG_MBHC_BTN_RESULT);

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

static bool msm8x16_spmi_lock_sleep(struct wcd_mbhc *mbhc, bool lock)
{
	if (lock)
		return wcd9xxx_spmi_lock_sleep();
	wcd9xxx_spmi_unlock_sleep();
	return 0;
}

static bool msm8x16_wcd_micb_en_status(struct wcd_mbhc *mbhc, int micb_num)
{
	if (micb_num == MIC_BIAS_1)
		return (snd_soc_read(mbhc->codec,
				     MSM8X16_WCD_A_ANALOG_MICB_1_EN) &
			0x80);
	if (micb_num == MIC_BIAS_2)
		return (snd_soc_read(mbhc->codec,
				     MSM8X16_WCD_A_ANALOG_MICB_2_EN) &
			0x80);
	return false;
}

static void msm8x16_wcd_enable_master_bias(struct snd_soc_codec *codec,
					   bool enable)
{
	if (enable)
		snd_soc_update_bits(codec, MSM8X16_WCD_A_ANALOG_MASTER_BIAS_CTL,
				    0x30, 0x30);
	else
		snd_soc_update_bits(codec, MSM8X16_WCD_A_ANALOG_MASTER_BIAS_CTL,
				    0x30, 0x00);
}

static void msm8x16_wcd_mbhc_common_micb_ctrl(struct snd_soc_codec *codec,
					      int event, bool enable)
{
	u16 reg;
	u8 mask;
	u8 val;

	switch (event) {
	case MBHC_COMMON_MICB_PRECHARGE:
		reg = MSM8X16_WCD_A_ANALOG_MICB_1_CTL;
		mask = 0x60;
		val = (enable ? 0x60 : 0x00);
		break;
	case MBHC_COMMON_MICB_SET_VAL:
		reg = MSM8X16_WCD_A_ANALOG_MICB_1_VAL;
		mask = 0xFF;
		val = (enable ? 0xC0 : 0x00);
		break;
	case MBHC_COMMON_MICB_TAIL_CURR:
		reg = MSM8X16_WCD_A_ANALOG_MICB_1_EN;
		mask = 0x04;
		val = (enable ? 0x04 : 0x00);
		break;
	default:
		pr_err("%s: Invalid event received\n", __func__);
		return;
	};
	snd_soc_update_bits(codec, reg, mask, val);
}

static void msm8x16_wcd_mbhc_internal_micbias_ctrl(struct snd_soc_codec *codec,
						   int micbias_num, bool enable)
{
	if (micbias_num == 1) {
		if (enable)
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_MICB_1_INT_RBIAS,
				0x10, 0x10);
		else
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_MICB_1_INT_RBIAS,
				0x10, 0x00);
	}
}

static bool msm8x16_wcd_mbhc_hph_pa_on_status(struct snd_soc_codec *codec)
{
	return (snd_soc_read(codec, MSM8X16_WCD_A_ANALOG_RX_HPH_CNP_EN) &
		0x30) ? true : false;
}

static void msm8x16_wcd_mbhc_program_btn_thr(struct snd_soc_codec *codec,
					     s16 *btn_low, s16 *btn_high,
					     int num_btn, bool is_micbias)
{
	int i;
	u32 course, fine, reg_val;
	u16 reg_addr = MSM8X16_WCD_A_ANALOG_MBHC_BTN0_ZDETL_CTL;
	s16 *btn_voltage;

	btn_voltage = ((is_micbias) ? btn_high : btn_low);

	for (i = 0; i <  num_btn; i++) {
		course = (btn_voltage[i] / MSM8X16_WCD_MBHC_BTN_COARSE_ADJ);
		fine = ((btn_voltage[i] % MSM8X16_WCD_MBHC_BTN_COARSE_ADJ) /
				MSM8X16_WCD_MBHC_BTN_FINE_ADJ);

		reg_val = (course << 5) | (fine << 2);
		snd_soc_update_bits(codec, reg_addr, 0xFC, reg_val);
		pr_debug("%s: course: %d fine: %d reg_addr: %x reg_val: %x\n",
			  __func__, course, fine, reg_addr, reg_val);
		reg_addr++;
	}
}

static void msm8x16_wcd_mbhc_calc_impedance(struct wcd_mbhc *mbhc, uint32_t *zl,
					    uint32_t *zr)
{
	struct snd_soc_codec *codec = mbhc->codec;
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);
	s16 impedance_l, impedance_r;
	s16 impedance_l_fixed;
	s16 reg0, reg1, reg2, reg3, reg4;
	bool high = false;
	bool min_range_used =  false;

	WCD_MBHC_RSC_ASSERT_LOCKED(mbhc);
	reg0 = snd_soc_read(codec, MSM8X16_WCD_A_ANALOG_MBHC_DBNC_TIMER);
	reg1 = snd_soc_read(codec, MSM8X16_WCD_A_ANALOG_MBHC_BTN2_ZDETH_CTL);
	reg2 = snd_soc_read(codec, MSM8X16_WCD_A_ANALOG_MBHC_DET_CTL_2);
	reg3 = snd_soc_read(codec, MSM8X16_WCD_A_ANALOG_MICB_2_EN);
	reg4 = snd_soc_read(codec, MSM8X16_WCD_A_ANALOG_MBHC_FSM_CTL);

	msm8x16_wcd->imped_det_pin = WCD_MBHC_DET_BOTH;
	mbhc->hph_type = WCD_MBHC_HPH_NONE;

	/* disable FSM and micbias and enable pullup*/
	snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MBHC_FSM_CTL,
			0x80, 0x00);
	snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MICB_2_EN,
			0xA5, 0x25);
	/*
	 * Enable legacy electrical detection current sources
	 * and disable fast ramp and enable manual switching
	 * of extra capacitance
	 */
	pr_debug("%s: Setup for impedance det\n", __func__);

	msm8x16_set_ref_current(codec, I_h4_UA);

	snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MBHC_DET_CTL_2,
			0x06, 0x02);
	snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MBHC_DBNC_TIMER,
			0x02, 0x02);
	snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MBHC_BTN2_ZDETH_CTL,
			0x02, 0x00);

	pr_debug("%s: Start performing impedance detection\n",
		 __func__);

	wcd_mbhc_meas_imped(codec, &impedance_l, &impedance_r);

	if (impedance_l > 2 || impedance_r > 2) {
		high = true;
		if (!mbhc->mbhc_cfg->mono_stero_detection) {
			/* Set ZDET_CHG to 0  to discharge ramp */
			snd_soc_update_bits(codec,
					MSM8X16_WCD_A_ANALOG_MBHC_FSM_CTL,
					0x02, 0x00);
			/* wait 40ms for the discharge ramp to complete */
			usleep_range(40000, 40100);
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_MBHC_BTN0_ZDETL_CTL,
				0x03, 0x00);
			msm8x16_wcd->imped_det_pin = (impedance_l > 2 &&
						      impedance_r > 2) ?
						      WCD_MBHC_DET_NONE :
						      ((impedance_l > 2) ?
						      WCD_MBHC_DET_HPHR :
						      WCD_MBHC_DET_HPHL);
			if (msm8x16_wcd->imped_det_pin == WCD_MBHC_DET_NONE)
				goto exit;
		} else {
			if (get_codec_version(msm8x16_wcd) >= CAJON) {
				if (impedance_l == 63 && impedance_r == 63) {
					pr_debug("%s: HPHL and HPHR are floating\n",
						 __func__);
					msm8x16_wcd->imped_det_pin =
							WCD_MBHC_DET_NONE;
					mbhc->hph_type = WCD_MBHC_HPH_NONE;
				} else if (impedance_l == 63
					   && impedance_r < 63) {
					pr_debug("%s: Mono HS with HPHL floating\n",
						 __func__);
					msm8x16_wcd->imped_det_pin =
							WCD_MBHC_DET_HPHR;
					mbhc->hph_type = WCD_MBHC_HPH_MONO;
				} else if (impedance_r == 63 &&
					   impedance_l < 63) {
					pr_debug("%s: Mono HS with HPHR floating\n",
						 __func__);
					msm8x16_wcd->imped_det_pin =
							WCD_MBHC_DET_HPHL;
					mbhc->hph_type = WCD_MBHC_HPH_MONO;
				} else if (impedance_l > 3 && impedance_r > 3 &&
					(impedance_l == impedance_r)) {
					snd_soc_update_bits(codec,
					MSM8X16_WCD_A_ANALOG_MBHC_DET_CTL_2,
					0x06, 0x06);
					wcd_mbhc_meas_imped(codec, &impedance_l,
							    &impedance_r);
					if (impedance_r == impedance_l)
						pr_debug("%s: Mono Headset\n",
							  __func__);
						msm8x16_wcd->imped_det_pin =
							WCD_MBHC_DET_NONE;
						mbhc->hph_type =
							WCD_MBHC_HPH_MONO;
				} else {
					pr_debug("%s: STEREO headset is found\n",
						 __func__);
					msm8x16_wcd->imped_det_pin =
							WCD_MBHC_DET_BOTH;
					mbhc->hph_type = WCD_MBHC_HPH_STEREO;
				}
			}
		}
	}

	msm8x16_set_ref_current(codec, I_pt5_UA);
	msm8x16_set_ref_current(codec, I_14_UA);

	/* Enable RAMP_L , RAMP_R & ZDET_CHG*/
	snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MBHC_BTN0_ZDETL_CTL,
			0x03, 0x03);
	snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MBHC_FSM_CTL,
			0x02, 0x02);
	/* wait for 50msec for the HW to apply ramp on HPHL and HPHR */
	usleep_range(50000, 50100);
	/* Enable ZDET_DISCHG_CAP_CTL  to add extra capacitance */
	snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MBHC_FSM_CTL,
			0x01, 0x01);
	/* wait for 5msec for the voltage to get stable */
	usleep_range(5000, 5100);


	wcd_mbhc_meas_imped(codec, &impedance_l, &impedance_r);

	min_range_used = msm8x16_adj_ref_current(codec,
						&impedance_l, &impedance_r);
	if (!mbhc->mbhc_cfg->mono_stero_detection) {
		/* Set ZDET_CHG to 0  to discharge ramp */
		snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_MBHC_FSM_CTL,
				0x02, 0x00);
		/* wait for 40msec for the capacitor to discharge */
		usleep_range(40000, 40100);
		snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_MBHC_BTN0_ZDETL_CTL,
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
	    msm8x16_wcd->imped_det_pin == WCD_MBHC_DET_HPHL ||
	    msm8x16_wcd->imped_det_pin == WCD_MBHC_DET_HPHR)
		goto exit;


	/* Disable Set ZDET_CONN_RAMP_L and enable ZDET_CONN_FIXED_L */
	snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MBHC_BTN0_ZDETL_CTL,
			0x02, 0x00);
	snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MBHC_BTN1_ZDETM_CTL,
			0x02, 0x02);
	/* Set ZDET_CHG to 0  */
	snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MBHC_FSM_CTL,
			0x02, 0x00);
	/* wait for 40msec for the capacitor to discharge */
	usleep_range(40000, 40100);

	/* Set ZDET_CONN_RAMP_R to 0  */
	snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MBHC_BTN0_ZDETL_CTL,
			0x01, 0x00);
	/* Enable ZDET_L_MEAS_EN */
	snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MBHC_FSM_CTL,
			0x08, 0x08);
	/* wait for 2msec for the HW to compute left inpedance value */
	usleep_range(2000, 2100);
	/* Read Left impedance value from Result1 */
	impedance_l_fixed = snd_soc_read(codec,
			MSM8X16_WCD_A_ANALOG_MBHC_BTN_RESULT);
	/* Disable ZDET_L_MEAS_EN */
	snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MBHC_FSM_CTL,
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
		pr_debug("%s: STEREO plug type detected\n",
			 __func__);
		mbhc->hph_type = WCD_MBHC_HPH_STEREO;
	} else {
		pr_debug("%s: MONO plug type detected\n",
			__func__);
		mbhc->hph_type = WCD_MBHC_HPH_MONO;
		impedance_l = impedance_l_fixed;
	}
	/* Enable ZDET_CHG  */
	snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MBHC_FSM_CTL,
			0x02, 0x02);
	/* wait for 10msec for the capacitor to charge */
	usleep_range(10000, 10100);
	snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MBHC_BTN0_ZDETL_CTL,
			0x02, 0x02);
	snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MBHC_BTN1_ZDETM_CTL,
			0x02, 0x00);
	/* Set ZDET_CHG to 0  to discharge HPHL */
	snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MBHC_FSM_CTL,
			0x02, 0x00);
	/* wait for 40msec for the capacitor to discharge */
	usleep_range(40000, 40100);
	snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MBHC_BTN0_ZDETL_CTL,
			0x02, 0x00);

exit:
	snd_soc_write(codec, MSM8X16_WCD_A_ANALOG_MBHC_FSM_CTL, reg4);
	snd_soc_write(codec, MSM8X16_WCD_A_ANALOG_MICB_2_EN, reg3);
	snd_soc_write(codec, MSM8X16_WCD_A_ANALOG_MBHC_BTN2_ZDETH_CTL, reg1);
	snd_soc_write(codec, MSM8X16_WCD_A_ANALOG_MBHC_DBNC_TIMER, reg0);
	snd_soc_write(codec, MSM8X16_WCD_A_ANALOG_MBHC_DET_CTL_2, reg2);
	msm8x16_wcd_compute_impedance(codec, impedance_l, impedance_r,
				      zl, zr, high);

	pr_debug("%s: RL %d ohm, RR %d ohm\n", __func__, *zl, *zr);
	pr_debug("%s: Impedance detection completed\n", __func__);
}

static int msm8x16_register_notifier(struct snd_soc_codec *codec,
				     struct notifier_block *nblock,
				     bool enable)
{
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	if (enable)
		return blocking_notifier_chain_register(&msm8x16_wcd->notifier,
							nblock);
	return blocking_notifier_chain_unregister(
			&msm8x16_wcd->notifier,	nblock);
}

static int msm8x16_wcd_request_irq(struct snd_soc_codec *codec,
				   int irq, irq_handler_t handler,
				   const char *name, void *data)
{
	return wcd9xxx_spmi_request_irq(irq, handler, name, data);
}

static int msm8x16_wcd_free_irq(struct snd_soc_codec *codec,
				int irq, void *data)
{
	return wcd9xxx_spmi_free_irq(irq, data);
}

static const struct wcd_mbhc_cb mbhc_cb = {
	.enable_mb_source = msm8x16_wcd_enable_ext_mb_source,
	.trim_btn_reg = msm8x16_trim_btn_reg,
	.compute_impedance = msm8x16_wcd_mbhc_calc_impedance,
	.set_micbias_value = msm8x16_wcd_set_micb_v,
	.set_auto_zeroing = msm8x16_wcd_set_auto_zeroing,
	.get_hwdep_fw_cal = msm8x16_wcd_get_hwdep_fw_cal,
	.set_cap_mode = msm8x16_wcd_configure_cap,
	.register_notifier = msm8x16_register_notifier,
	.request_irq = msm8x16_wcd_request_irq,
	.irq_control = wcd9xxx_spmi_irq_control,
	.free_irq = msm8x16_wcd_free_irq,
	.clk_setup = msm8x16_mbhc_clk_setup,
	.map_btn_code_to_num = msm8x16_mbhc_map_btn_code_to_num,
	.lock_sleep = msm8x16_spmi_lock_sleep,
	.micbias_enable_status = msm8x16_wcd_micb_en_status,
	.mbhc_bias = msm8x16_wcd_enable_master_bias,
	.mbhc_common_micb_ctrl = msm8x16_wcd_mbhc_common_micb_ctrl,
	.micb_internal = msm8x16_wcd_mbhc_internal_micbias_ctrl,
	.hph_pa_on_status = msm8x16_wcd_mbhc_hph_pa_on_status,
	.set_btn_thr = msm8x16_wcd_mbhc_program_btn_thr,
	.extn_use_mb = msm8x16_wcd_use_mb,
};

static const uint32_t wcd_imped_val[] = {4, 8, 12, 13, 16,
					20, 24, 28, 32,
					36, 40, 44, 48};

void msm8x16_notifier_call(struct snd_soc_codec *codec,
				  const enum wcd_notify_event event)
{
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s: notifier call event %d\n", __func__, event);
	blocking_notifier_call_chain(&msm8x16_wcd->notifier, event,
				     &msm8x16_wcd->mbhc);
}

static int get_spmi_msm8x16_wcd_device_info(u16 *reg,
			struct msm8x16_wcd_spmi **msm8x16_wcd)
{
	int rtn = 0;
	int value = ((*reg & 0x0f00) >> 8) & 0x000f;

	*reg = *reg - (value * 0x100);
	switch (value) {
	case 0:
	case 1:
		*msm8x16_wcd = &msm8x16_wcd_modules[value];
		break;
	default:
		rtn = -EINVAL;
		break;
	}
	return rtn;
}

static int msm8x16_wcd_ahb_write_device(struct msm8x16_wcd *msm8x16_wcd,
					u16 reg, u8 *value, u32 bytes)
{
	u32 temp = ((u32)(*value)) & 0x000000FF;
	u16 offset = (reg - 0x0200) & 0x03FF;
	bool q6_state = false;

	q6_state = q6core_is_adsp_ready();
	if (q6_state != true) {
		pr_debug("%s: q6 not ready %d\n", __func__, q6_state);
		return 0;
	}

	pr_debug("%s: DSP is ready %d\n", __func__, q6_state);
	iowrite32(temp, msm8x16_wcd->dig_base + offset);
	return 0;
}

static int msm8x16_wcd_ahb_read_device(struct msm8x16_wcd *msm8x16_wcd,
					u16 reg, u32 bytes, u8 *value)
{
	u32 temp;
	u16 offset = (reg - 0x0200) & 0x03FF;
	bool q6_state = false;

	q6_state = q6core_is_adsp_ready();
	if (q6_state != true) {
		pr_debug("%s: q6 not ready %d\n", __func__, q6_state);
		return 0;
	}
	pr_debug("%s: DSP is ready %d\n", __func__, q6_state);

	temp = ioread32(msm8x16_wcd->dig_base + offset);
	*value = (u8)temp;
	return 0;
}

static int msm8x16_wcd_spmi_write_device(u16 reg, u8 *value, u32 bytes)
{

	int ret;
	struct msm8x16_wcd_spmi *wcd = NULL;

	ret = get_spmi_msm8x16_wcd_device_info(&reg, &wcd);
	if (ret) {
		pr_err("%s: Invalid register address\n", __func__);
		return ret;
	}

	if (wcd == NULL) {
		pr_err("%s: Failed to get device info\n", __func__);
		return -ENODEV;
	}
	ret = spmi_ext_register_writel(wcd->spmi->ctrl, wcd->spmi->sid,
						wcd->base + reg, value, bytes);
	if (ret)
		pr_err_ratelimited("Unable to write to addr=%x, ret(%d)\n",
				reg, ret);
	/* Try again if the write fails */
	if (ret != 0) {
		usleep_range(10, 11);
		ret = spmi_ext_register_writel(wcd->spmi->ctrl, wcd->spmi->sid,
						wcd->base + reg, value, 1);
		if (ret != 0) {
			pr_err_ratelimited("failed to write the device\n");
			return ret;
		}
	}
	pr_debug("write sucess register = %x val = %x\n", reg, *value);
	return 0;
}


int msm8x16_wcd_spmi_read_device(u16 reg, u32 bytes, u8 *dest)
{
	int ret = 0;
	struct msm8x16_wcd_spmi *wcd = NULL;

	ret = get_spmi_msm8x16_wcd_device_info(&reg, &wcd);
	if (ret) {
		pr_err("%s: Invalid register address\n", __func__);
		return ret;
	}

	if (wcd == NULL) {
		pr_err("%s: Failed to get device info\n", __func__);
		return -ENODEV;
	}

	ret = spmi_ext_register_readl(wcd->spmi->ctrl, wcd->spmi->sid,
						wcd->base + reg, dest, bytes);
	if (ret != 0) {
		pr_err("failed to read the device\n");
		return ret;
	}
	pr_debug("%s: reg 0x%x = 0x%x\n", __func__, reg, *dest);
	return 0;
}

int msm8x16_wcd_spmi_read(unsigned short reg, int bytes, void *dest)
{
	return msm8x16_wcd_spmi_read_device(reg, bytes, dest);
}

int msm8x16_wcd_spmi_write(unsigned short reg, int bytes, void *src)
{
	return msm8x16_wcd_spmi_write_device(reg, src, bytes);
}

static int __msm8x16_wcd_reg_read(struct snd_soc_codec *codec,
				unsigned short reg)
{
	int ret = -EINVAL;
	u8 temp = 0;
	struct msm8x16_wcd *msm8x16_wcd = codec->control_data;
	struct msm8916_asoc_mach_data *pdata = NULL;

	pr_debug("%s reg = %x\n", __func__, reg);
	mutex_lock(&msm8x16_wcd->io_lock);
	pdata = snd_soc_card_get_drvdata(codec->component.card);
	if (MSM8X16_WCD_IS_TOMBAK_REG(reg))
		ret = msm8x16_wcd_spmi_read(reg, 1, &temp);
	else if (MSM8X16_WCD_IS_DIGITAL_REG(reg)) {
		mutex_lock(&pdata->cdc_mclk_mutex);
		if (atomic_read(&pdata->mclk_enabled) == false) {
			if (pdata->afe_clk_ver == AFE_CLK_VERSION_V1) {
				pdata->digital_cdc_clk.clk_val =
							pdata->mclk_freq;
				ret = afe_set_digital_codec_core_clock(
						AFE_PORT_ID_PRIMARY_MI2S_RX,
						&pdata->digital_cdc_clk);
			} else {
				pdata->digital_cdc_core_clk.enable = 1;
				ret = afe_set_lpass_clock_v2(
						AFE_PORT_ID_PRIMARY_MI2S_RX,
						&pdata->digital_cdc_core_clk);
			}
			if (ret < 0) {
				pr_err("failed to enable the MCLK\n");
				goto err;
			}
			pr_debug("enabled digital codec core clk\n");
			ret = msm8x16_wcd_ahb_read_device(
					msm8x16_wcd, reg, 1, &temp);
			atomic_set(&pdata->mclk_enabled, true);
			schedule_delayed_work(&pdata->disable_mclk_work, 50);
err:
			mutex_unlock(&pdata->cdc_mclk_mutex);
			mutex_unlock(&msm8x16_wcd->io_lock);
			return temp;
		}
		ret = msm8x16_wcd_ahb_read_device(msm8x16_wcd, reg, 1, &temp);
		mutex_unlock(&pdata->cdc_mclk_mutex);
	}
	mutex_unlock(&msm8x16_wcd->io_lock);

	if (ret < 0) {
		dev_err_ratelimited(msm8x16_wcd->dev,
				"%s: codec read failed for reg 0x%x\n",
				__func__, reg);
		return ret;
	}
	dev_dbg(msm8x16_wcd->dev, "Read 0x%02x from 0x%x\n", temp, reg);

	return temp;
}

static int __msm8x16_wcd_reg_write(struct snd_soc_codec *codec,
			unsigned short reg, u8 val)
{
	int ret = -EINVAL;
	struct msm8x16_wcd *msm8x16_wcd = codec->control_data;
	struct msm8916_asoc_mach_data *pdata = NULL;

	mutex_lock(&msm8x16_wcd->io_lock);
	pdata = snd_soc_card_get_drvdata(codec->component.card);
	if (MSM8X16_WCD_IS_TOMBAK_REG(reg))
		ret = msm8x16_wcd_spmi_write(reg, 1, &val);
	else if (MSM8X16_WCD_IS_DIGITAL_REG(reg)) {
		mutex_lock(&pdata->cdc_mclk_mutex);
		if (atomic_read(&pdata->mclk_enabled) == false) {
			pr_debug("enable MCLK for AHB write\n");
			if (pdata->afe_clk_ver == AFE_CLK_VERSION_V1) {
				pdata->digital_cdc_clk.clk_val =
							pdata->mclk_freq;
				ret = afe_set_digital_codec_core_clock(
						AFE_PORT_ID_PRIMARY_MI2S_RX,
						&pdata->digital_cdc_clk);
			} else {
				pdata->digital_cdc_core_clk.enable = 1;
				ret = afe_set_lpass_clock_v2(
						AFE_PORT_ID_PRIMARY_MI2S_RX,
						&pdata->digital_cdc_core_clk);
			}
			if (ret < 0) {
				pr_err("failed to enable the MCLK\n");
				ret = 0;
				goto err;
			}
			pr_debug("enabled digital codec core clk\n");
			ret = msm8x16_wcd_ahb_write_device(
						msm8x16_wcd, reg, &val, 1);
			atomic_set(&pdata->mclk_enabled, true);
			schedule_delayed_work(&pdata->disable_mclk_work, 50);
err:
			mutex_unlock(&pdata->cdc_mclk_mutex);
			mutex_unlock(&msm8x16_wcd->io_lock);
			return ret;
		}
		ret = msm8x16_wcd_ahb_write_device(msm8x16_wcd, reg, &val, 1);
		mutex_unlock(&pdata->cdc_mclk_mutex);
	}
	mutex_unlock(&msm8x16_wcd->io_lock);

	return ret;
}

static int msm8x16_wcd_volatile(struct snd_soc_codec *codec, unsigned int reg)
{
	dev_dbg(codec->dev, "%s: reg 0x%x\n", __func__, reg);

	return msm8x16_wcd_reg_readonly[reg];
}

static int msm8x16_wcd_readable(struct snd_soc_codec *ssc, unsigned int reg)
{
	return msm8x16_wcd_reg_readable[reg];
}

static int msm8x16_wcd_write(struct snd_soc_codec *codec, unsigned int reg,
			     unsigned int value)
{
	int ret;
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s: Write from reg 0x%x val 0x%x\n",
					__func__, reg, value);
	if (reg == SND_SOC_NOPM)
		return 0;

	BUG_ON(reg > MSM8X16_WCD_MAX_REGISTER);
	if (!msm8x16_wcd_volatile(codec, reg)) {
		ret = snd_soc_cache_write(codec, reg, value);
		if (ret != 0)
			dev_err_ratelimited(codec->dev, "Cache write to %x failed: %d\n",
				reg, ret);
	}
	if (unlikely(test_bit(BUS_DOWN, &msm8x16_wcd->status_mask))) {
		pr_err_ratelimited("write 0x%02x while offline\n",
				reg);
		return -ENODEV;
	}
	return __msm8x16_wcd_reg_write(codec, reg, (u8)value);
}

static unsigned int msm8x16_wcd_read(struct snd_soc_codec *codec,
				unsigned int reg)
{
	unsigned int val;
	int ret;
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	if (reg == SND_SOC_NOPM)
		return 0;

	BUG_ON(reg > MSM8X16_WCD_MAX_REGISTER);

	if (!msm8x16_wcd_volatile(codec, reg) &&
	    msm8x16_wcd_readable(codec, reg) &&
		reg < codec->driver->reg_cache_size) {
		ret = snd_soc_cache_read(codec, reg, &val);
		if (ret >= 0)
			return val;
		dev_err_ratelimited(codec->dev, "Cache read from %x failed: %d\n",
				reg, ret);
	}
	if (unlikely(test_bit(BUS_DOWN, &msm8x16_wcd->status_mask))) {
		pr_err_ratelimited("write 0x%02x while offline\n",
				reg);
		return -ENODEV;
	}
	val = __msm8x16_wcd_reg_read(codec, reg);
	/*
	 * If register is 0x16 or 0x116 return read value as 0, so that
	 * SW can disable only the required interrupts. Which will
	 * ensure other interrupts are not effected.
	 */
	if ((reg == MSM8X16_WCD_A_DIGITAL_INT_EN_CLR) ||
			(reg == MSM8X16_WCD_A_ANALOG_INT_EN_CLR))
		val = 0;
	dev_dbg(codec->dev, "%s: Read from reg 0x%x val 0x%x\n",
					__func__, reg, val);
	return val;
}

static void msm8x16_wcd_boost_on(struct snd_soc_codec *codec)
{
	int ret;
	u8 dest;
	struct msm8x16_wcd_spmi *wcd = &msm8x16_wcd_modules[0];
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	ret = spmi_ext_register_readl(wcd->spmi->ctrl, PMIC_SLAVE_ID_1,
					PMIC_LDO7_EN_CTL, &dest, 1);
	if (ret != 0) {
		pr_err("%s: failed to read the device:%d\n", __func__, ret);
		return;
	}
	pr_debug("%s: LDO state: 0x%x\n", __func__, dest);

	if ((dest & MASK_MSB_BIT) == 0) {
		pr_err("LDO7 not enabled return!\n");
		return;
	}
	ret = spmi_ext_register_readl(wcd->spmi->ctrl, PMIC_SLAVE_ID_0,
						PMIC_MBG_OK, &dest, 1);
	if (ret != 0) {
		pr_err("%s: failed to read the device:%d\n", __func__, ret);
		return;
	}
	pr_debug("%s: PMIC BG state: 0x%x\n", __func__, dest);

	if ((dest & MASK_MSB_BIT) == 0) {
		pr_err("PMIC MBG not ON, enable codec hw_en MB bit again\n");
		snd_soc_write(codec,
		MSM8X16_WCD_A_ANALOG_MASTER_BIAS_CTL, 0x30);
		/* Allow 1ms for PMIC MBG state to be updated */
		usleep_range(CODEC_DELAY_1_MS, CODEC_DELAY_1_1_MS);
		ret = spmi_ext_register_readl(wcd->spmi->ctrl, PMIC_SLAVE_ID_0,
						PMIC_MBG_OK, &dest, 1);
		if (ret != 0) {
			pr_err("%s: failed to read the device:%d\n",
						__func__, ret);
			return;
		}
		if ((dest & MASK_MSB_BIT) == 0) {
			pr_err("PMIC MBG still not ON after retry return!\n");
			return;
		}
	}
	snd_soc_update_bits(codec,
		MSM8X16_WCD_A_DIGITAL_PERPH_RESET_CTL3,
		0x0F, 0x0F);
	snd_soc_write(codec,
		MSM8X16_WCD_A_ANALOG_SEC_ACCESS,
		0xA5);
	snd_soc_write(codec,
		MSM8X16_WCD_A_ANALOG_PERPH_RESET_CTL3,
		0x0F);
	snd_soc_write(codec,
		MSM8X16_WCD_A_ANALOG_MASTER_BIAS_CTL,
		0x30);
	if (get_codec_version(msm8x16_wcd) < CAJON_2_0) {
		snd_soc_write(codec,
			MSM8X16_WCD_A_ANALOG_CURRENT_LIMIT,
			0x82);
	} else {
		snd_soc_write(codec,
			MSM8X16_WCD_A_ANALOG_CURRENT_LIMIT,
			0xA2);
	}
	snd_soc_update_bits(codec,
		MSM8X16_WCD_A_ANALOG_SPKR_DRV_CTL,
		0x69, 0x69);
	snd_soc_update_bits(codec,
		MSM8X16_WCD_A_ANALOG_SPKR_DRV_DBG,
		0x01, 0x01);
	snd_soc_update_bits(codec,
		MSM8X16_WCD_A_ANALOG_SLOPE_COMP_IP_ZERO,
		0x88, 0x88);
	snd_soc_update_bits(codec,
		MSM8X16_WCD_A_ANALOG_SPKR_DAC_CTL,
		0x03, 0x03);
	snd_soc_update_bits(codec,
		MSM8X16_WCD_A_ANALOG_SPKR_OCP_CTL,
		0xE1, 0xE1);
	if (get_codec_version(msm8x16_wcd) < CAJON_2_0) {
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_DIGITAL_CDC_DIG_CLK_CTL,
			0x20, 0x20);
		usleep_range(CODEC_DELAY_1_MS, CODEC_DELAY_1_1_MS);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_BOOST_EN_CTL,
			0xDF, 0xDF);
		usleep_range(CODEC_DELAY_1_MS, CODEC_DELAY_1_1_MS);
	} else {
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_BOOST_EN_CTL,
			0x40, 0x00);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_DIGITAL_CDC_DIG_CLK_CTL,
			0x20, 0x20);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_BOOST_EN_CTL,
			0x80, 0x80);
		usleep_range(500, 510);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_BOOST_EN_CTL,
			0x40, 0x40);
		usleep_range(500, 510);
	}
}

static void msm8x16_wcd_boost_off(struct snd_soc_codec *codec)
{
	snd_soc_update_bits(codec,
		MSM8X16_WCD_A_ANALOG_BOOST_EN_CTL,
		0xDF, 0x5F);
	snd_soc_update_bits(codec,
		MSM8X16_WCD_A_DIGITAL_CDC_DIG_CLK_CTL,
		0x20, 0x00);
}

static void msm8x16_wcd_bypass_on(struct snd_soc_codec *codec)
{
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	if (get_codec_version(msm8x16_wcd) < CAJON_2_0) {
		snd_soc_write(codec,
			MSM8X16_WCD_A_ANALOG_SEC_ACCESS,
			0xA5);
		snd_soc_write(codec,
			MSM8X16_WCD_A_ANALOG_PERPH_RESET_CTL3,
			0x07);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_BYPASS_MODE,
			0x02, 0x02);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_BYPASS_MODE,
			0x01, 0x00);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_BYPASS_MODE,
			0x40, 0x40);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_BYPASS_MODE,
			0x80, 0x80);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_BOOST_EN_CTL,
			0xDF, 0xDF);
	} else {
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_DIGITAL_CDC_DIG_CLK_CTL,
			0x20, 0x20);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_BYPASS_MODE,
			0x20, 0x20);
	}
}

static void msm8x16_wcd_bypass_off(struct snd_soc_codec *codec)
{
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	if (get_codec_version(msm8x16_wcd) < CAJON_2_0) {
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_BOOST_EN_CTL,
			0x80, 0x00);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_BYPASS_MODE,
			0x80, 0x00);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_BYPASS_MODE,
			0x02, 0x00);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_BYPASS_MODE,
			0x40, 0x00);
	} else {
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_BYPASS_MODE,
			0x20, 0x00);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_DIGITAL_CDC_DIG_CLK_CTL,
			0x20, 0x00);
	}
}

static void msm8x16_wcd_boost_mode_sequence(struct snd_soc_codec *codec,
					int flag)
{
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	if (flag == EAR_PMU) {
		switch (msm8x16_wcd->boost_option) {
		case BOOST_SWITCH:
			if (msm8x16_wcd->ear_pa_boost_set) {
				msm8x16_wcd_boost_off(codec);
				msm8x16_wcd_bypass_on(codec);
			}
			break;
		case BOOST_ALWAYS:
			msm8x16_wcd_boost_on(codec);
			break;
		case BYPASS_ALWAYS:
			msm8x16_wcd_bypass_on(codec);
			break;
		case BOOST_ON_FOREVER:
			msm8x16_wcd_boost_on(codec);
			break;
		default:
			pr_err("%s: invalid boost option: %d\n", __func__,
						msm8x16_wcd->boost_option);
			break;
		}
	} else if (flag == EAR_PMD) {
		switch (msm8x16_wcd->boost_option) {
		case BOOST_SWITCH:
			if (msm8x16_wcd->ear_pa_boost_set)
				msm8x16_wcd_bypass_off(codec);
			break;
		case BOOST_ALWAYS:
			msm8x16_wcd_boost_off(codec);
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
			pr_err("%s: invalid boost option: %d\n", __func__,
						msm8x16_wcd->boost_option);
			break;
		}
	} else if (flag == SPK_PMU) {
		switch (msm8x16_wcd->boost_option) {
		case BOOST_SWITCH:
			if (msm8x16_wcd->spk_boost_set) {
				msm8x16_wcd_bypass_off(codec);
				msm8x16_wcd_boost_on(codec);
			}
			break;
		case BOOST_ALWAYS:
			msm8x16_wcd_boost_on(codec);
			break;
		case BYPASS_ALWAYS:
			msm8x16_wcd_bypass_on(codec);
			break;
		case BOOST_ON_FOREVER:
			msm8x16_wcd_boost_on(codec);
			break;
		default:
			pr_err("%s: invalid boost option: %d\n", __func__,
						msm8x16_wcd->boost_option);
			break;
		}
	} else if (flag == SPK_PMD) {
		switch (msm8x16_wcd->boost_option) {
		case BOOST_SWITCH:
			if (msm8x16_wcd->spk_boost_set) {
				msm8x16_wcd_boost_off(codec);
				/*
				 * Add 40 ms sleep for the spk
				 * boost to settle down
				 */
				msleep(40);
			}
			break;
		case BOOST_ALWAYS:
			msm8x16_wcd_boost_off(codec);
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
			pr_err("%s: invalid boost option: %d\n", __func__,
						msm8x16_wcd->boost_option);
			break;
		}
	}
}

static int msm8x16_wcd_dt_parse_vreg_info(struct device *dev,
	struct msm8x16_wcd_regulator *vreg, const char *vreg_name,
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

static void msm8x16_wcd_dt_parse_boost_info(struct snd_soc_codec *codec)
{
	struct msm8x16_wcd_priv *msm8x16_wcd_priv =
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

	msm8x16_wcd_priv->boost_voltage =
		VOLTAGE_CONVERTER(boost_voltage, MIN_BOOST_VOLTAGE,
				BOOST_VOLTAGE_STEP);
	dev_dbg(codec->dev, "Boost voltage value is: %d\n",
			boost_voltage);
}

static void msm8x16_wcd_dt_parse_micbias_info(struct device *dev,
			struct wcd9xxx_micbias_setting *micbias)
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

static struct msm8x16_wcd_pdata *msm8x16_wcd_populate_dt_pdata(
						struct device *dev)
{
	struct msm8x16_wcd_pdata *pdata;
	int ret, static_cnt, ond_cnt, idx, i;
	const char *name = NULL;
	const char *static_prop_name = "qcom,cdc-static-supplies";
	const char *ond_prop_name = "qcom,cdc-on-demand-supplies";
	const char *addr_prop_name = "qcom,dig-cdc-base-addr";

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

	BUG_ON(static_cnt <= 0 || ond_cnt < 0);
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
		ret = msm8x16_wcd_dt_parse_vreg_info(dev,
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
		ret = msm8x16_wcd_dt_parse_vreg_info(dev,
						&pdata->regulator[idx],
						name, true);
		if (ret) {
			dev_err(dev, "%s: err parsing vreg on_demand for %s idx %d\n",
				__func__, name, idx);
			goto err;
		}
	}
	msm8x16_wcd_dt_parse_micbias_info(dev, &pdata->micbias);
	ret = of_property_read_u32(dev->of_node, addr_prop_name,
			&pdata->dig_cdc_addr);
	if (ret) {
		dev_dbg(dev, "%s: could not find %s entry in dt\n",
				__func__, addr_prop_name);
		pdata->dig_cdc_addr = MSM8X16_DIGITAL_CODEC_BASE_ADDR;
	}

	return pdata;
err:
	devm_kfree(dev, pdata);
	dev_err(dev, "%s: Failed to populate DT data ret = %d\n",
		__func__, ret);
	return NULL;
}

static int msm8x16_wcd_codec_enable_on_demand_supply(
		struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	int ret = 0;
	struct snd_soc_codec *codec = w->codec;
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);
	struct on_demand_supply *supply;

	if (w->shift >= ON_DEMAND_SUPPLIES_MAX) {
		dev_err(codec->dev, "%s: error index > MAX Demand supplies",
			__func__);
		ret = -EINVAL;
		goto out;
	}
	dev_dbg(codec->dev, "%s: supply: %s event: %d ref: %d\n",
		__func__, on_demand_supply_name[w->shift], event,
		atomic_read(&msm8x16_wcd->on_demand_list[w->shift].ref));

	supply = &msm8x16_wcd->on_demand_list[w->shift];
	WARN_ONCE(!supply->supply, "%s isn't defined\n",
		  on_demand_supply_name[w->shift]);
	if (!supply->supply) {
		dev_err(codec->dev, "%s: err supply not present ond for %d",
			__func__, w->shift);
		goto out;
	}
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (atomic_inc_return(&supply->ref) == 1)
			ret = regulator_enable(supply->supply);
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
		if (atomic_dec_return(&supply->ref) == 0)
			ret = regulator_disable(supply->supply);
			if (ret)
				dev_err(codec->dev, "%s: Failed to disable %s\n",
					__func__,
					on_demand_supply_name[w->shift]);
		break;
	default:
		break;
	}
out:
	return ret;
}

static int msm8x16_wcd_codec_enable_clock_block(struct snd_soc_codec *codec,
						int enable)
{
	struct msm8916_asoc_mach_data *pdata = NULL;

	pdata = snd_soc_card_get_drvdata(codec->component.card);
	if (enable) {
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_CDC_CLK_MCLK_CTL, 0x01, 0x01);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_CDC_CLK_PDM_CTL, 0x03, 0x03);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MASTER_BIAS_CTL, 0x30, 0x30);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_DIGITAL_CDC_RST_CTL, 0x80, 0x80);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_DIGITAL_CDC_TOP_CLK_CTL, 0x0C, 0x0C);
		if (pdata->mclk_freq == MCLK_RATE_12P288MHZ)
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_TOP_CTL, 0x01, 0x00);
		else if (pdata->mclk_freq == MCLK_RATE_9P6MHZ)
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_TOP_CTL, 0x01, 0x01);
	} else {
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_DIGITAL_CDC_TOP_CLK_CTL, 0x0C, 0x00);
		snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_CLK_PDM_CTL, 0x03, 0x00);

	}
	return 0;
}

static int msm8x16_wcd_codec_enable_charge_pump(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s: event = %d\n", __func__, event);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		msm8x16_wcd_codec_enable_clock_block(codec, 1);
		if (!(strcmp(w->name, "EAR CP"))) {
			snd_soc_update_bits(codec,
					MSM8X16_WCD_A_DIGITAL_CDC_DIG_CLK_CTL,
					0x80, 0x80);
			msm8x16_wcd_boost_mode_sequence(codec, EAR_PMU);
		} else if (get_codec_version(msm8x16_wcd) == DIANGU) {
			snd_soc_update_bits(codec,
					MSM8X16_WCD_A_DIGITAL_CDC_DIG_CLK_CTL,
					0x80, 0x80);
		} else {
			snd_soc_update_bits(codec,
					MSM8X16_WCD_A_DIGITAL_CDC_DIG_CLK_CTL,
					0xC0, 0xC0);
		}
		break;
	case SND_SOC_DAPM_POST_PMU:
		usleep_range(CODEC_DELAY_1_MS, CODEC_DELAY_1_1_MS);
		break;
	case SND_SOC_DAPM_POST_PMD:
		usleep_range(CODEC_DELAY_1_MS, CODEC_DELAY_1_1_MS);
		if (!(strcmp(w->name, "EAR CP"))) {
			snd_soc_update_bits(codec,
					MSM8X16_WCD_A_DIGITAL_CDC_DIG_CLK_CTL,
					0x80, 0x00);
			if (msm8x16_wcd->boost_option != BOOST_ALWAYS) {
				dev_dbg(codec->dev,
					"%s: boost_option:%d, tear down ear\n",
					__func__, msm8x16_wcd->boost_option);
				msm8x16_wcd_boost_mode_sequence(codec, EAR_PMD);
			}
			/*
			 * Reset pa select bit from ear to hph after ear pa
			 * is disabled and HPH DAC disable to reduce ear
			 * turn off pop and avoid HPH pop in concurrency
			 */
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_RX_EAR_CTL, 0x80, 0x00);
		} else {
			if (get_codec_version(msm8x16_wcd) < DIANGU)
				snd_soc_update_bits(codec,
					MSM8X16_WCD_A_DIGITAL_CDC_DIG_CLK_CTL,
					0x40, 0x00);
			if (msm8x16_wcd->rx_bias_count == 0)
				snd_soc_update_bits(codec,
					MSM8X16_WCD_A_DIGITAL_CDC_DIG_CLK_CTL,
					0x80, 0x00);
			dev_dbg(codec->dev, "%s: rx_bias_count = %d\n",
					__func__, msm8x16_wcd->rx_bias_count);
		}
		break;
	}
	return 0;
}

static int msm8x16_wcd_ear_pa_boost_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] =
		(msm8x16_wcd->ear_pa_boost_set ? 1 : 0);
	dev_dbg(codec->dev, "%s: msm8x16_wcd->ear_pa_boost_set = %d\n",
			__func__, msm8x16_wcd->ear_pa_boost_set);
	return 0;
}

static int msm8x16_wcd_ear_pa_boost_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s: ucontrol->value.integer.value[0] = %ld\n",
		__func__, ucontrol->value.integer.value[0]);
	msm8x16_wcd->ear_pa_boost_set =
		(ucontrol->value.integer.value[0] ? true : false);
	return 0;
}

static int msm8x16_wcd_pa_gain_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	u8 ear_pa_gain;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);

	ear_pa_gain = snd_soc_read(codec, MSM8X16_WCD_A_ANALOG_RX_EAR_CTL);

	ear_pa_gain = (ear_pa_gain >> 5) & 0x1;

	if (ear_pa_gain == 0x00) {
		ucontrol->value.integer.value[0] = 0;
	} else if (ear_pa_gain == 0x01) {
		ucontrol->value.integer.value[0] = 1;
	} else  {
		dev_err(codec->dev, "%s: ERROR: Unsupported Ear Gain = 0x%x\n",
			__func__, ear_pa_gain);
		return -EINVAL;
	}

	ucontrol->value.integer.value[0] = ear_pa_gain;
	dev_dbg(codec->dev, "%s: ear_pa_gain = 0x%x\n", __func__, ear_pa_gain);
	return 0;
}

static int msm8x16_wcd_loopback_mode_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct msm8916_asoc_mach_data *pdata = NULL;

	pdata = snd_soc_card_get_drvdata(codec->component.card);
	dev_dbg(codec->dev, "%s: ucontrol->value.integer.value[0] = %ld\n",
		__func__, ucontrol->value.integer.value[0]);

	return pdata->lb_mode;
}

static int msm8x16_wcd_loopback_mode_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct msm8916_asoc_mach_data *pdata = NULL;

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

static int msm8x16_wcd_pa_gain_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	u8 ear_pa_gain;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);

	dev_dbg(codec->dev, "%s: ucontrol->value.integer.value[0] = %ld\n",
		__func__, ucontrol->value.integer.value[0]);

	switch (ucontrol->value.integer.value[0]) {
	case 0:
		ear_pa_gain = 0x00;
		break;
	case 1:
		ear_pa_gain = 0x20;
		break;
	default:
		return -EINVAL;
	}

	snd_soc_update_bits(codec, MSM8X16_WCD_A_ANALOG_RX_EAR_CTL,
			    0x20, ear_pa_gain);
	return 0;
}

static int msm8x16_wcd_hph_mode_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	if (msm8x16_wcd->hph_mode == NORMAL_MODE) {
		ucontrol->value.integer.value[0] = 0;
	} else if (msm8x16_wcd->hph_mode == HD2_MODE) {
		ucontrol->value.integer.value[0] = 1;
	} else  {
		dev_err(codec->dev, "%s: ERROR: Default HPH Mode= %d\n",
			__func__, msm8x16_wcd->hph_mode);
	}

	dev_dbg(codec->dev, "%s: msm8x16_wcd->hph_mode = %d\n", __func__,
			msm8x16_wcd->hph_mode);
	return 0;
}

static int msm8x16_wcd_hph_mode_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s: ucontrol->value.integer.value[0] = %ld\n",
		__func__, ucontrol->value.integer.value[0]);

	switch (ucontrol->value.integer.value[0]) {
	case 0:
		msm8x16_wcd->hph_mode = NORMAL_MODE;
		break;
	case 1:
		if (get_codec_version(msm8x16_wcd) >= DIANGU)
			msm8x16_wcd->hph_mode = HD2_MODE;
		break;
	default:
		msm8x16_wcd->hph_mode = NORMAL_MODE;
		break;
	}
	dev_dbg(codec->dev, "%s: msm8x16_wcd->hph_mode_set = %d\n",
		__func__, msm8x16_wcd->hph_mode);
	return 0;
}

static int msm8x16_wcd_boost_option_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	if (msm8x16_wcd->boost_option == BOOST_SWITCH) {
		ucontrol->value.integer.value[0] = 0;
	} else if (msm8x16_wcd->boost_option == BOOST_ALWAYS) {
		ucontrol->value.integer.value[0] = 1;
	} else if (msm8x16_wcd->boost_option == BYPASS_ALWAYS) {
		ucontrol->value.integer.value[0] = 2;
	} else if (msm8x16_wcd->boost_option == BOOST_ON_FOREVER) {
		ucontrol->value.integer.value[0] = 3;
	} else  {
		dev_err(codec->dev, "%s: ERROR: Unsupported Boost option= %d\n",
			__func__, msm8x16_wcd->boost_option);
		return -EINVAL;
	}

	dev_dbg(codec->dev, "%s: msm8x16_wcd->boost_option = %d\n", __func__,
			msm8x16_wcd->boost_option);
	return 0;
}

static int msm8x16_wcd_boost_option_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s: ucontrol->value.integer.value[0] = %ld\n",
		__func__, ucontrol->value.integer.value[0]);

	switch (ucontrol->value.integer.value[0]) {
	case 0:
		msm8x16_wcd->boost_option = BOOST_SWITCH;
		break;
	case 1:
		msm8x16_wcd->boost_option = BOOST_ALWAYS;
		break;
	case 2:
		msm8x16_wcd->boost_option = BYPASS_ALWAYS;
		msm8x16_wcd_bypass_on(codec);
		break;
	case 3:
		msm8x16_wcd->boost_option = BOOST_ON_FOREVER;
		msm8x16_wcd_boost_on(codec);
		break;
	default:
		pr_err("%s: invalid boost option: %d\n", __func__,
					msm8x16_wcd->boost_option);
		return -EINVAL;
	}
	dev_dbg(codec->dev, "%s: msm8x16_wcd->boost_option_set = %d\n",
		__func__, msm8x16_wcd->boost_option);
	return 0;
}

static int msm8x16_wcd_spk_boost_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	if (msm8x16_wcd->spk_boost_set == false) {
		ucontrol->value.integer.value[0] = 0;
	} else if (msm8x16_wcd->spk_boost_set == true) {
		ucontrol->value.integer.value[0] = 1;
	} else  {
		dev_err(codec->dev, "%s: ERROR: Unsupported Speaker Boost = %d\n",
				__func__, msm8x16_wcd->spk_boost_set);
		return -EINVAL;
	}

	dev_dbg(codec->dev, "%s: msm8x16_wcd->spk_boost_set = %d\n", __func__,
			msm8x16_wcd->spk_boost_set);
	return 0;
}

static int msm8x16_wcd_spk_boost_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s: ucontrol->value.integer.value[0] = %ld\n",
			__func__, ucontrol->value.integer.value[0]);

	switch (ucontrol->value.integer.value[0]) {
	case 0:
		msm8x16_wcd->spk_boost_set = false;
		break;
	case 1:
		msm8x16_wcd->spk_boost_set = true;
		break;
	default:
		return -EINVAL;
	}
	dev_dbg(codec->dev, "%s: msm8x16_wcd->spk_boost_set = %d\n",
		__func__, msm8x16_wcd->spk_boost_set);
	return 0;
}

static int msm8x16_wcd_ext_spk_boost_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	if (msm8x16_wcd->ext_spk_boost_set == false)
		ucontrol->value.integer.value[0] = 0;
	else
		ucontrol->value.integer.value[0] = 1;

	dev_dbg(codec->dev, "%s: msm8x16_wcd->ext_spk_boost_set = %d\n",
				__func__, msm8x16_wcd->ext_spk_boost_set);
	return 0;
}

static int msm8x16_wcd_ext_spk_boost_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s: ucontrol->value.integer.value[0] = %ld\n",
		__func__, ucontrol->value.integer.value[0]);

	switch (ucontrol->value.integer.value[0]) {
	case 0:
		msm8x16_wcd->ext_spk_boost_set = false;
		break;
	case 1:
		msm8x16_wcd->ext_spk_boost_set = true;
		break;
	default:
		return -EINVAL;
	}
	dev_dbg(codec->dev, "%s: msm8x16_wcd->spk_boost_set = %d\n",
		__func__, msm8x16_wcd->spk_boost_set);
	return 0;
}
static int msm8x16_wcd_get_iir_enable_audio_mixer(
					struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	int iir_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int band_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;

	ucontrol->value.integer.value[0] =
		(snd_soc_read(codec,
			    (MSM8X16_WCD_A_CDC_IIR1_CTL + 64 * iir_idx)) &
		(1 << band_idx)) != 0;

	dev_dbg(codec->dev, "%s: IIR #%d band #%d enable %d\n", __func__,
		iir_idx, band_idx,
		(uint32_t)ucontrol->value.integer.value[0]);
	return 0;
}

static int msm8x16_wcd_put_iir_enable_audio_mixer(
					struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	int iir_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int band_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;
	int value = ucontrol->value.integer.value[0];

	/* Mask first 5 bits, 6-8 are reserved */
	snd_soc_update_bits(codec,
		(MSM8X16_WCD_A_CDC_IIR1_CTL + 64 * iir_idx),
			    (1 << band_idx), (value << band_idx));

	dev_dbg(codec->dev, "%s: IIR #%d band #%d enable %d\n", __func__,
	  iir_idx, band_idx,
		((snd_soc_read(codec,
		(MSM8X16_WCD_A_CDC_IIR1_CTL + 64 * iir_idx)) &
	  (1 << band_idx)) != 0));

	return 0;
}
static uint32_t get_iir_band_coeff(struct snd_soc_codec *codec,
				   int iir_idx, int band_idx,
				   int coeff_idx)
{
	uint32_t value = 0;

	/* Address does not automatically update if reading */
	snd_soc_write(codec,
		(MSM8X16_WCD_A_CDC_IIR1_COEF_B1_CTL + 64 * iir_idx),
		((band_idx * BAND_MAX + coeff_idx)
		* sizeof(uint32_t)) & 0x7F);

	value |= snd_soc_read(codec,
		(MSM8X16_WCD_A_CDC_IIR1_COEF_B2_CTL + 64 * iir_idx));

	snd_soc_write(codec,
		(MSM8X16_WCD_A_CDC_IIR1_COEF_B1_CTL + 64 * iir_idx),
		((band_idx * BAND_MAX + coeff_idx)
		* sizeof(uint32_t) + 1) & 0x7F);

	value |= (snd_soc_read(codec,
		(MSM8X16_WCD_A_CDC_IIR1_COEF_B2_CTL + 64 * iir_idx)) << 8);

	snd_soc_write(codec,
		(MSM8X16_WCD_A_CDC_IIR1_COEF_B1_CTL + 64 * iir_idx),
		((band_idx * BAND_MAX + coeff_idx)
		* sizeof(uint32_t) + 2) & 0x7F);

	value |= (snd_soc_read(codec,
		(MSM8X16_WCD_A_CDC_IIR1_COEF_B2_CTL + 64 * iir_idx)) << 16);

	snd_soc_write(codec,
		(MSM8X16_WCD_A_CDC_IIR1_COEF_B1_CTL + 64 * iir_idx),
		((band_idx * BAND_MAX + coeff_idx)
		* sizeof(uint32_t) + 3) & 0x7F);

	/* Mask bits top 2 bits since they are reserved */
	value |= ((snd_soc_read(codec, (MSM8X16_WCD_A_CDC_IIR1_COEF_B2_CTL
		+ 64 * iir_idx)) & 0x3f) << 24);

	return value;

}

static int msm8x16_wcd_get_iir_band_audio_mixer(
					struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	int iir_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int band_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;

	ucontrol->value.integer.value[0] =
		get_iir_band_coeff(codec, iir_idx, band_idx, 0);
	ucontrol->value.integer.value[1] =
		get_iir_band_coeff(codec, iir_idx, band_idx, 1);
	ucontrol->value.integer.value[2] =
		get_iir_band_coeff(codec, iir_idx, band_idx, 2);
	ucontrol->value.integer.value[3] =
		get_iir_band_coeff(codec, iir_idx, band_idx, 3);
	ucontrol->value.integer.value[4] =
		get_iir_band_coeff(codec, iir_idx, band_idx, 4);

	dev_dbg(codec->dev, "%s: IIR #%d band #%d b0 = 0x%x\n"
		"%s: IIR #%d band #%d b1 = 0x%x\n"
		"%s: IIR #%d band #%d b2 = 0x%x\n"
		"%s: IIR #%d band #%d a1 = 0x%x\n"
		"%s: IIR #%d band #%d a2 = 0x%x\n",
		__func__, iir_idx, band_idx,
		(uint32_t)ucontrol->value.integer.value[0],
		__func__, iir_idx, band_idx,
		(uint32_t)ucontrol->value.integer.value[1],
		__func__, iir_idx, band_idx,
		(uint32_t)ucontrol->value.integer.value[2],
		__func__, iir_idx, band_idx,
		(uint32_t)ucontrol->value.integer.value[3],
		__func__, iir_idx, band_idx,
		(uint32_t)ucontrol->value.integer.value[4]);
	return 0;
}

static void set_iir_band_coeff(struct snd_soc_codec *codec,
				int iir_idx, int band_idx,
				uint32_t value)
{
	snd_soc_write(codec,
		(MSM8X16_WCD_A_CDC_IIR1_COEF_B2_CTL + 64 * iir_idx),
		(value & 0xFF));

	snd_soc_write(codec,
		(MSM8X16_WCD_A_CDC_IIR1_COEF_B2_CTL + 64 * iir_idx),
		(value >> 8) & 0xFF);

	snd_soc_write(codec,
		(MSM8X16_WCD_A_CDC_IIR1_COEF_B2_CTL + 64 * iir_idx),
		(value >> 16) & 0xFF);

	/* Mask top 2 bits, 7-8 are reserved */
	snd_soc_write(codec,
		(MSM8X16_WCD_A_CDC_IIR1_COEF_B2_CTL + 64 * iir_idx),
		(value >> 24) & 0x3F);

}

static int msm8x16_wcd_put_iir_band_audio_mixer(
					struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	int iir_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int band_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;

	/* Mask top bit it is reserved */
	/* Updates addr automatically for each B2 write */
	snd_soc_write(codec,
		(MSM8X16_WCD_A_CDC_IIR1_COEF_B1_CTL + 64 * iir_idx),
		(band_idx * BAND_MAX * sizeof(uint32_t)) & 0x7F);


	set_iir_band_coeff(codec, iir_idx, band_idx,
			   ucontrol->value.integer.value[0]);
	set_iir_band_coeff(codec, iir_idx, band_idx,
			   ucontrol->value.integer.value[1]);
	set_iir_band_coeff(codec, iir_idx, band_idx,
			   ucontrol->value.integer.value[2]);
	set_iir_band_coeff(codec, iir_idx, band_idx,
			   ucontrol->value.integer.value[3]);
	set_iir_band_coeff(codec, iir_idx, band_idx,
			   ucontrol->value.integer.value[4]);

	dev_dbg(codec->dev, "%s: IIR #%d band #%d b0 = 0x%x\n"
		"%s: IIR #%d band #%d b1 = 0x%x\n"
		"%s: IIR #%d band #%d b2 = 0x%x\n"
		"%s: IIR #%d band #%d a1 = 0x%x\n"
		"%s: IIR #%d band #%d a2 = 0x%x\n",
		__func__, iir_idx, band_idx,
		get_iir_band_coeff(codec, iir_idx, band_idx, 0),
		__func__, iir_idx, band_idx,
		get_iir_band_coeff(codec, iir_idx, band_idx, 1),
		__func__, iir_idx, band_idx,
		get_iir_band_coeff(codec, iir_idx, band_idx, 2),
		__func__, iir_idx, band_idx,
		get_iir_band_coeff(codec, iir_idx, band_idx, 3),
		__func__, iir_idx, band_idx,
		get_iir_band_coeff(codec, iir_idx, band_idx, 4));
	return 0;
}

static int msm8x16_wcd_compander_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);
	int comp_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int rx_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;

	dev_dbg(codec->dev, "%s: msm8x16_wcd->comp[%d]_enabled[%d] = %d\n",
			__func__, comp_idx, rx_idx,
			msm8x16_wcd->comp_enabled[rx_idx]);

	ucontrol->value.integer.value[0] = msm8x16_wcd->comp_enabled[rx_idx];

	dev_dbg(codec->dev, "%s: ucontrol->value.integer.value[0] = %ld\n",
		__func__, ucontrol->value.integer.value[0]);

	return 0;
}

static int msm8x16_wcd_compander_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);
	int comp_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int rx_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;
	int value = ucontrol->value.integer.value[0];

	dev_dbg(codec->dev, "%s: ucontrol->value.integer.value[0] = %ld\n",
		__func__, ucontrol->value.integer.value[0]);

	if (get_codec_version(msm8x16_wcd) >= DIANGU) {
		if (!value)
			msm8x16_wcd->comp_enabled[rx_idx] = 0;
		else
			msm8x16_wcd->comp_enabled[rx_idx] = comp_idx;
	}

	dev_dbg(codec->dev, "%s: msm8x16_wcd->comp[%d]_enabled[%d] = %d\n",
		__func__, comp_idx, rx_idx,
		msm8x16_wcd->comp_enabled[rx_idx]);

	return 0;
}

static const char * const msm8x16_wcd_loopback_mode_ctrl_text[] = {
		"DISABLE", "ENABLE"};
static const struct soc_enum msm8x16_wcd_loopback_mode_ctl_enum[] = {
		SOC_ENUM_SINGLE_EXT(2, msm8x16_wcd_loopback_mode_ctrl_text),
};

static const char * const msm8x16_wcd_ear_pa_boost_ctrl_text[] = {
		"DISABLE", "ENABLE"};
static const struct soc_enum msm8x16_wcd_ear_pa_boost_ctl_enum[] = {
		SOC_ENUM_SINGLE_EXT(2, msm8x16_wcd_ear_pa_boost_ctrl_text),
};

static const char * const msm8x16_wcd_ear_pa_gain_text[] = {
		"POS_1P5_DB", "POS_6_DB"};
static const struct soc_enum msm8x16_wcd_ear_pa_gain_enum[] = {
		SOC_ENUM_SINGLE_EXT(2, msm8x16_wcd_ear_pa_gain_text),
};

static const char * const msm8x16_wcd_boost_option_ctrl_text[] = {
		"BOOST_SWITCH", "BOOST_ALWAYS", "BYPASS_ALWAYS",
		"BOOST_ON_FOREVER"};
static const struct soc_enum msm8x16_wcd_boost_option_ctl_enum[] = {
		SOC_ENUM_SINGLE_EXT(4, msm8x16_wcd_boost_option_ctrl_text),
};
static const char * const msm8x16_wcd_spk_boost_ctrl_text[] = {
		"DISABLE", "ENABLE"};
static const struct soc_enum msm8x16_wcd_spk_boost_ctl_enum[] = {
		SOC_ENUM_SINGLE_EXT(2, msm8x16_wcd_spk_boost_ctrl_text),
};

static const char * const msm8x16_wcd_ext_spk_boost_ctrl_text[] = {
		"DISABLE", "ENABLE"};
static const struct soc_enum msm8x16_wcd_ext_spk_boost_ctl_enum[] = {
		SOC_ENUM_SINGLE_EXT(2, msm8x16_wcd_ext_spk_boost_ctrl_text),
};

static const char * const msm8x16_wcd_hph_mode_ctrl_text[] = {
		"NORMAL", "HD2"};
static const struct soc_enum msm8x16_wcd_hph_mode_ctl_enum[] = {
		SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(msm8x16_wcd_hph_mode_ctrl_text),
			msm8x16_wcd_hph_mode_ctrl_text),
};

/*cut of frequency for high pass filter*/
static const char * const cf_text[] = {
	"MIN_3DB_4Hz", "MIN_3DB_75Hz", "MIN_3DB_150Hz"
};

static const struct soc_enum cf_dec1_enum =
	SOC_ENUM_SINGLE(MSM8X16_WCD_A_CDC_TX1_MUX_CTL, 4, 3, cf_text);

static const struct soc_enum cf_dec2_enum =
	SOC_ENUM_SINGLE(MSM8X16_WCD_A_CDC_TX2_MUX_CTL, 4, 3, cf_text);

static const struct soc_enum cf_rxmix1_enum =
	SOC_ENUM_SINGLE(MSM8X16_WCD_A_CDC_RX1_B4_CTL, 0, 3, cf_text);

static const struct soc_enum cf_rxmix2_enum =
	SOC_ENUM_SINGLE(MSM8X16_WCD_A_CDC_RX2_B4_CTL, 0, 3, cf_text);

static const struct soc_enum cf_rxmix3_enum =
	SOC_ENUM_SINGLE(MSM8X16_WCD_A_CDC_RX3_B4_CTL, 0, 3, cf_text);

static const struct snd_kcontrol_new msm8x16_wcd_snd_controls[] = {

	SOC_ENUM_EXT("RX HPH Mode", msm8x16_wcd_hph_mode_ctl_enum[0],
		msm8x16_wcd_hph_mode_get, msm8x16_wcd_hph_mode_set),

	SOC_ENUM_EXT("Boost Option", msm8x16_wcd_boost_option_ctl_enum[0],
		msm8x16_wcd_boost_option_get, msm8x16_wcd_boost_option_set),

	SOC_ENUM_EXT("EAR PA Boost", msm8x16_wcd_ear_pa_boost_ctl_enum[0],
		msm8x16_wcd_ear_pa_boost_get, msm8x16_wcd_ear_pa_boost_set),

	SOC_ENUM_EXT("EAR PA Gain", msm8x16_wcd_ear_pa_gain_enum[0],
		msm8x16_wcd_pa_gain_get, msm8x16_wcd_pa_gain_put),

	SOC_ENUM_EXT("Speaker Boost", msm8x16_wcd_spk_boost_ctl_enum[0],
		msm8x16_wcd_spk_boost_get, msm8x16_wcd_spk_boost_set),

	SOC_ENUM_EXT("Ext Spk Boost", msm8x16_wcd_ext_spk_boost_ctl_enum[0],
		msm8x16_wcd_ext_spk_boost_get, msm8x16_wcd_ext_spk_boost_set),

	SOC_ENUM_EXT("LOOPBACK Mode", msm8x16_wcd_loopback_mode_ctl_enum[0],
		msm8x16_wcd_loopback_mode_get, msm8x16_wcd_loopback_mode_put),

	SOC_SINGLE_TLV("ADC1 Volume", MSM8X16_WCD_A_ANALOG_TX_1_EN, 3,
					8, 0, analog_gain),
	SOC_SINGLE_TLV("ADC2 Volume", MSM8X16_WCD_A_ANALOG_TX_2_EN, 3,
					8, 0, analog_gain),
	SOC_SINGLE_TLV("ADC3 Volume", MSM8X16_WCD_A_ANALOG_TX_3_EN, 3,
					8, 0, analog_gain),

	SOC_SINGLE_SX_TLV("RX1 Digital Volume",
			  MSM8X16_WCD_A_CDC_RX1_VOL_CTL_B2_CTL,
			0,  -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("RX2 Digital Volume",
			  MSM8X16_WCD_A_CDC_RX2_VOL_CTL_B2_CTL,
			0,  -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("RX3 Digital Volume",
			  MSM8X16_WCD_A_CDC_RX3_VOL_CTL_B2_CTL,
			0,  -84, 40, digital_gain),

	SOC_SINGLE_SX_TLV("DEC1 Volume",
			  MSM8X16_WCD_A_CDC_TX1_VOL_CTL_GAIN,
			0,  -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("DEC2 Volume",
			  MSM8X16_WCD_A_CDC_TX2_VOL_CTL_GAIN,
			0,  -84, 40, digital_gain),

	SOC_SINGLE_SX_TLV("IIR1 INP1 Volume",
			  MSM8X16_WCD_A_CDC_IIR1_GAIN_B1_CTL,
			0,  -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("IIR1 INP2 Volume",
			  MSM8X16_WCD_A_CDC_IIR1_GAIN_B2_CTL,
			0,  -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("IIR1 INP3 Volume",
			  MSM8X16_WCD_A_CDC_IIR1_GAIN_B3_CTL,
			0,  -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("IIR1 INP4 Volume",
			  MSM8X16_WCD_A_CDC_IIR1_GAIN_B4_CTL,
			0,  -84,	40, digital_gain),
	SOC_SINGLE_SX_TLV("IIR2 INP1 Volume",
			  MSM8X16_WCD_A_CDC_IIR2_GAIN_B1_CTL,
			0,  -84, 40, digital_gain),

	SOC_ENUM("TX1 HPF cut off", cf_dec1_enum),
	SOC_ENUM("TX2 HPF cut off", cf_dec2_enum),

	SOC_SINGLE("TX1 HPF Switch",
		MSM8X16_WCD_A_CDC_TX1_MUX_CTL, 3, 1, 0),
	SOC_SINGLE("TX2 HPF Switch",
		MSM8X16_WCD_A_CDC_TX2_MUX_CTL, 3, 1, 0),

	SOC_SINGLE("RX1 HPF Switch",
		MSM8X16_WCD_A_CDC_RX1_B5_CTL, 2, 1, 0),
	SOC_SINGLE("RX2 HPF Switch",
		MSM8X16_WCD_A_CDC_RX2_B5_CTL, 2, 1, 0),
	SOC_SINGLE("RX3 HPF Switch",
		MSM8X16_WCD_A_CDC_RX3_B5_CTL, 2, 1, 0),

	SOC_ENUM("RX1 HPF cut off", cf_rxmix1_enum),
	SOC_ENUM("RX2 HPF cut off", cf_rxmix2_enum),
	SOC_ENUM("RX3 HPF cut off", cf_rxmix3_enum),

	SOC_SINGLE_EXT("IIR1 Enable Band1", IIR1, BAND1, 1, 0,
	msm8x16_wcd_get_iir_enable_audio_mixer,
	msm8x16_wcd_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR1 Enable Band2", IIR1, BAND2, 1, 0,
	msm8x16_wcd_get_iir_enable_audio_mixer,
	msm8x16_wcd_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR1 Enable Band3", IIR1, BAND3, 1, 0,
	msm8x16_wcd_get_iir_enable_audio_mixer,
	msm8x16_wcd_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR1 Enable Band4", IIR1, BAND4, 1, 0,
	msm8x16_wcd_get_iir_enable_audio_mixer,
	msm8x16_wcd_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR1 Enable Band5", IIR1, BAND5, 1, 0,
	msm8x16_wcd_get_iir_enable_audio_mixer,
	msm8x16_wcd_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR2 Enable Band1", IIR2, BAND1, 1, 0,
	msm8x16_wcd_get_iir_enable_audio_mixer,
	msm8x16_wcd_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR2 Enable Band2", IIR2, BAND2, 1, 0,
	msm8x16_wcd_get_iir_enable_audio_mixer,
	msm8x16_wcd_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR2 Enable Band3", IIR2, BAND3, 1, 0,
	msm8x16_wcd_get_iir_enable_audio_mixer,
	msm8x16_wcd_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR2 Enable Band4", IIR2, BAND4, 1, 0,
	msm8x16_wcd_get_iir_enable_audio_mixer,
	msm8x16_wcd_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR2 Enable Band5", IIR2, BAND5, 1, 0,
	msm8x16_wcd_get_iir_enable_audio_mixer,
	msm8x16_wcd_put_iir_enable_audio_mixer),

	SOC_SINGLE_MULTI_EXT("IIR1 Band1", IIR1, BAND1, 255, 0, 5,
	msm8x16_wcd_get_iir_band_audio_mixer,
	msm8x16_wcd_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR1 Band2", IIR1, BAND2, 255, 0, 5,
	msm8x16_wcd_get_iir_band_audio_mixer,
	msm8x16_wcd_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR1 Band3", IIR1, BAND3, 255, 0, 5,
	msm8x16_wcd_get_iir_band_audio_mixer,
	msm8x16_wcd_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR1 Band4", IIR1, BAND4, 255, 0, 5,
	msm8x16_wcd_get_iir_band_audio_mixer,
	msm8x16_wcd_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR1 Band5", IIR1, BAND5, 255, 0, 5,
	msm8x16_wcd_get_iir_band_audio_mixer,
	msm8x16_wcd_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR2 Band1", IIR2, BAND1, 255, 0, 5,
	msm8x16_wcd_get_iir_band_audio_mixer,
	msm8x16_wcd_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR2 Band2", IIR2, BAND2, 255, 0, 5,
	msm8x16_wcd_get_iir_band_audio_mixer,
	msm8x16_wcd_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR2 Band3", IIR2, BAND3, 255, 0, 5,
	msm8x16_wcd_get_iir_band_audio_mixer,
	msm8x16_wcd_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR2 Band4", IIR2, BAND4, 255, 0, 5,
	msm8x16_wcd_get_iir_band_audio_mixer,
	msm8x16_wcd_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR2 Band5", IIR2, BAND5, 255, 0, 5,
	msm8x16_wcd_get_iir_band_audio_mixer,
	msm8x16_wcd_put_iir_band_audio_mixer),

	SOC_SINGLE_EXT("COMP0 RX1", COMPANDER_1, MSM8X16_WCD_RX1, 1, 0,
	msm8x16_wcd_compander_get, msm8x16_wcd_compander_set),

	SOC_SINGLE_EXT("COMP0 RX2", COMPANDER_1, MSM8X16_WCD_RX2, 1, 0,
	msm8x16_wcd_compander_get, msm8x16_wcd_compander_set),
};

static int tombak_hph_impedance_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int ret;
	uint32_t zl, zr;
	bool hphr;
	struct soc_multi_mixer_control *mc;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct msm8x16_wcd_priv *priv = snd_soc_codec_get_drvdata(codec);

	mc = (struct soc_multi_mixer_control *)(kcontrol->private_value);

	hphr = mc->shift;
	ret = wcd_mbhc_get_impedance(&priv->mbhc, &zl, &zr);
	if (ret)
		pr_debug("%s: Failed to get mbhc imped", __func__);
	pr_debug("%s: zl %u, zr %u\n", __func__, zl, zr);
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
	struct msm8x16_wcd_priv *priv = snd_soc_codec_get_drvdata(codec);
	struct wcd_mbhc *mbhc;

	if (!priv) {
		pr_debug("%s: msm8x16-wcd private data is NULL\n",
			 __func__);
		return -EINVAL;
	}

	mbhc = &priv->mbhc;
	if (!mbhc) {
		pr_debug("%s: mbhc not initialized\n", __func__);
		return -EINVAL;
	}

	ucontrol->value.integer.value[0] = (u32) mbhc->hph_type;
	pr_debug("%s: hph_type = %u\n", __func__, mbhc->hph_type);

	return 0;
}

static const struct snd_kcontrol_new hph_type_detect_controls[] = {
	SOC_SINGLE_EXT("HPH Type", 0, 0, UINT_MAX, 0,
	tombak_get_hph_type, NULL),
};

static const char * const rx_mix1_text[] = {
	"ZERO", "IIR1", "IIR2", "RX1", "RX2", "RX3"
};

static const char * const rx_mix2_text[] = {
	"ZERO", "IIR1", "IIR2"
};

static const char * const dec_mux_text[] = {
	"ZERO", "ADC1", "ADC2", "ADC3", "DMIC1", "DMIC2"
};

static const char * const dec3_mux_text[] = {
	"ZERO", "DMIC3"
};

static const char * const dec4_mux_text[] = {
	"ZERO", "DMIC4"
};

static const char * const adc2_mux_text[] = {
	"ZERO", "INP2", "INP3"
};

static const char * const ext_spk_text[] = {
	"Off", "On"
};

static const char * const wsa_spk_text[] = {
	"ZERO", "WSA"
};

static const char * const rdac2_mux_text[] = {
	"ZERO", "RX2", "RX1"
};

static const char * const iir_inp1_text[] = {
	"ZERO", "DEC1", "DEC2", "RX1", "RX2", "RX3"
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

/* RX1 MIX1 */
static const struct soc_enum rx_mix1_inp1_chain_enum =
	SOC_ENUM_SINGLE(MSM8X16_WCD_A_CDC_CONN_RX1_B1_CTL,
		0, 6, rx_mix1_text);

static const struct soc_enum rx_mix1_inp2_chain_enum =
	SOC_ENUM_SINGLE(MSM8X16_WCD_A_CDC_CONN_RX1_B1_CTL,
		3, 6, rx_mix1_text);

static const struct soc_enum rx_mix1_inp3_chain_enum =
	SOC_ENUM_SINGLE(MSM8X16_WCD_A_CDC_CONN_RX1_B2_CTL,
		0, 6, rx_mix1_text);

/* RX1 MIX2 */
static const struct soc_enum rx_mix2_inp1_chain_enum =
	SOC_ENUM_SINGLE(MSM8X16_WCD_A_CDC_CONN_RX1_B3_CTL,
		0, 3, rx_mix2_text);

/* RX2 MIX1 */
static const struct soc_enum rx2_mix1_inp1_chain_enum =
	SOC_ENUM_SINGLE(MSM8X16_WCD_A_CDC_CONN_RX2_B1_CTL,
		0, 6, rx_mix1_text);

static const struct soc_enum rx2_mix1_inp2_chain_enum =
	SOC_ENUM_SINGLE(MSM8X16_WCD_A_CDC_CONN_RX2_B1_CTL,
		3, 6, rx_mix1_text);

static const struct soc_enum rx2_mix1_inp3_chain_enum =
	SOC_ENUM_SINGLE(MSM8X16_WCD_A_CDC_CONN_RX2_B1_CTL,
		0, 6, rx_mix1_text);

/* RX2 MIX2 */
static const struct soc_enum rx2_mix2_inp1_chain_enum =
	SOC_ENUM_SINGLE(MSM8X16_WCD_A_CDC_CONN_RX2_B3_CTL,
		0, 3, rx_mix2_text);

/* RX3 MIX1 */
static const struct soc_enum rx3_mix1_inp1_chain_enum =
	SOC_ENUM_SINGLE(MSM8X16_WCD_A_CDC_CONN_RX3_B1_CTL,
		0, 6, rx_mix1_text);

static const struct soc_enum rx3_mix1_inp2_chain_enum =
	SOC_ENUM_SINGLE(MSM8X16_WCD_A_CDC_CONN_RX3_B1_CTL,
		3, 6, rx_mix1_text);

static const struct soc_enum rx3_mix1_inp3_chain_enum =
	SOC_ENUM_SINGLE(MSM8X16_WCD_A_CDC_CONN_RX3_B1_CTL,
		0, 6, rx_mix1_text);

/* DEC */
static const struct soc_enum dec1_mux_enum =
	SOC_ENUM_SINGLE(MSM8X16_WCD_A_CDC_CONN_TX_B1_CTL,
		0, 6, dec_mux_text);

static const struct soc_enum dec2_mux_enum =
	SOC_ENUM_SINGLE(MSM8X16_WCD_A_CDC_CONN_TX_B1_CTL,
		3, 6, dec_mux_text);

static const struct soc_enum dec3_mux_enum =
	SOC_ENUM_SINGLE(MSM8X16_WCD_A_CDC_TX3_MUX_CTL, 0,
				ARRAY_SIZE(dec3_mux_text), dec3_mux_text);

static const struct soc_enum dec4_mux_enum =
	SOC_ENUM_SINGLE(MSM8X16_WCD_A_CDC_TX4_MUX_CTL, 0,
				ARRAY_SIZE(dec4_mux_text), dec4_mux_text);

static const struct soc_enum rdac2_mux_enum =
	SOC_ENUM_SINGLE(MSM8X16_WCD_A_DIGITAL_CDC_CONN_HPHR_DAC_CTL,
		0, 3, rdac2_mux_text);

static const struct soc_enum iir1_inp1_mux_enum =
	SOC_ENUM_SINGLE(MSM8X16_WCD_A_CDC_CONN_EQ1_B1_CTL,
		0, 6, iir_inp1_text);

static const struct soc_enum iir2_inp1_mux_enum =
	SOC_ENUM_SINGLE(MSM8X16_WCD_A_CDC_CONN_EQ2_B1_CTL,
		0, 6, iir_inp1_text);

static const struct snd_kcontrol_new ext_spk_mux =
	SOC_DAPM_ENUM("Ext Spk Switch Mux", ext_spk_enum);

static const struct snd_kcontrol_new rx_mix1_inp1_mux =
	SOC_DAPM_ENUM("RX1 MIX1 INP1 Mux", rx_mix1_inp1_chain_enum);

static const struct snd_kcontrol_new rx_mix1_inp2_mux =
	SOC_DAPM_ENUM("RX1 MIX1 INP2 Mux", rx_mix1_inp2_chain_enum);

static const struct snd_kcontrol_new rx_mix1_inp3_mux =
	SOC_DAPM_ENUM("RX1 MIX1 INP3 Mux", rx_mix1_inp3_chain_enum);

static const struct snd_kcontrol_new rx2_mix1_inp1_mux =
	SOC_DAPM_ENUM("RX2 MIX1 INP1 Mux", rx2_mix1_inp1_chain_enum);

static const struct snd_kcontrol_new rx2_mix1_inp2_mux =
	SOC_DAPM_ENUM("RX2 MIX1 INP2 Mux", rx2_mix1_inp2_chain_enum);

static const struct snd_kcontrol_new rx2_mix1_inp3_mux =
	SOC_DAPM_ENUM("RX2 MIX1 INP3 Mux", rx2_mix1_inp3_chain_enum);

static const struct snd_kcontrol_new rx3_mix1_inp1_mux =
	SOC_DAPM_ENUM("RX3 MIX1 INP1 Mux", rx3_mix1_inp1_chain_enum);

static const struct snd_kcontrol_new rx3_mix1_inp2_mux =
	SOC_DAPM_ENUM("RX3 MIX1 INP2 Mux", rx3_mix1_inp2_chain_enum);

static const struct snd_kcontrol_new rx3_mix1_inp3_mux =
	SOC_DAPM_ENUM("RX3 MIX1 INP3 Mux", rx3_mix1_inp3_chain_enum);

static const struct snd_kcontrol_new rx1_mix2_inp1_mux =
	SOC_DAPM_ENUM("RX1 MIX2 INP1 Mux", rx_mix2_inp1_chain_enum);

static const struct snd_kcontrol_new rx2_mix2_inp1_mux =
	SOC_DAPM_ENUM("RX2 MIX2 INP1 Mux", rx2_mix2_inp1_chain_enum);

static const struct snd_kcontrol_new tx_adc2_mux =
	SOC_DAPM_ENUM("ADC2 MUX Mux", adc2_enum);

static int msm8x16_wcd_put_dec_enum(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist =
			dapm_kcontrol_get_wlist(kcontrol);
	struct snd_soc_dapm_widget *w = wlist->widgets[0];
	struct snd_soc_codec *codec = w->codec;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int dec_mux, decimator;
	char *dec_name = NULL;
	char *widget_name = NULL;
	char *temp;
	u16 tx_mux_ctl_reg;
	u8 adc_dmic_sel = 0x0;
	int ret = 0;
	char *dec_num;

	if (ucontrol->value.enumerated.item[0] > e->items) {
		dev_err(codec->dev, "%s: Invalid enum value: %d\n",
			__func__, ucontrol->value.enumerated.item[0]);
		return -EINVAL;
	}
	dec_mux = ucontrol->value.enumerated.item[0];

	widget_name = kstrndup(w->name, 15, GFP_KERNEL);
	if (!widget_name) {
		dev_err(codec->dev, "%s: failed to copy string\n",
			__func__);
		return -ENOMEM;
	}
	temp = widget_name;

	dec_name = strsep(&widget_name, " ");
	widget_name = temp;
	if (!dec_name) {
		dev_err(codec->dev, "%s: Invalid decimator = %s\n",
			__func__, w->name);
		ret =  -EINVAL;
		goto out;
	}

	dec_num = strpbrk(dec_name, "12");
	if (dec_num == NULL) {
		dev_err(codec->dev, "%s: Invalid DEC selected\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	ret = kstrtouint(dec_num, 10, &decimator);
	if (ret < 0) {
		dev_err(codec->dev, "%s: Invalid decimator = %s\n",
			__func__, dec_name);
		ret =  -EINVAL;
		goto out;
	}

	dev_dbg(w->dapm->dev, "%s(): widget = %s decimator = %u dec_mux = %u\n"
		, __func__, w->name, decimator, dec_mux);

	switch (decimator) {
	case 1:
	case 2:
		if ((dec_mux == 4) || (dec_mux == 5))
			adc_dmic_sel = 0x1;
		else
			adc_dmic_sel = 0x0;
		break;
	default:
		dev_err(codec->dev, "%s: Invalid Decimator = %u\n",
			__func__, decimator);
		ret = -EINVAL;
		goto out;
	}

	tx_mux_ctl_reg =
		MSM8X16_WCD_A_CDC_TX1_MUX_CTL + 32 * (decimator - 1);

	snd_soc_update_bits(codec, tx_mux_ctl_reg, 0x1, adc_dmic_sel);

	ret = snd_soc_dapm_put_enum_double(kcontrol, ucontrol);

out:
	kfree(widget_name);
	return ret;
}

#define MSM8X16_WCD_DEC_ENUM(xname, xenum) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
	.info = snd_soc_info_enum_double, \
	.get = snd_soc_dapm_get_enum_double, \
	.put = msm8x16_wcd_put_dec_enum, \
	.private_value = (unsigned long)&xenum }

static const struct snd_kcontrol_new dec1_mux =
	MSM8X16_WCD_DEC_ENUM("DEC1 MUX Mux", dec1_mux_enum);

static const struct snd_kcontrol_new dec2_mux =
	MSM8X16_WCD_DEC_ENUM("DEC2 MUX Mux", dec2_mux_enum);

static const struct snd_kcontrol_new dec3_mux =
	SOC_DAPM_ENUM("DEC3 MUX Mux", dec3_mux_enum);

static const struct snd_kcontrol_new dec4_mux =
	SOC_DAPM_ENUM("DEC4 MUX Mux", dec4_mux_enum);

static const struct snd_kcontrol_new rdac2_mux =
	SOC_DAPM_ENUM("RDAC2 MUX Mux", rdac2_mux_enum);

static const struct snd_kcontrol_new iir1_inp1_mux =
	SOC_DAPM_ENUM("IIR1 INP1 Mux", iir1_inp1_mux_enum);

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

static const struct snd_kcontrol_new iir2_inp1_mux =
	SOC_DAPM_ENUM("IIR2 INP1 Mux", iir2_inp1_mux_enum);

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

static void msm8x16_wcd_codec_enable_adc_block(struct snd_soc_codec *codec,
					 int enable)
{
	struct msm8x16_wcd_priv *wcd8x16 = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s %d\n", __func__, enable);

	if (enable) {
		wcd8x16->adc_count++;
		snd_soc_update_bits(codec,
				    MSM8X16_WCD_A_DIGITAL_CDC_ANA_CLK_CTL,
				    0x20, 0x20);
		snd_soc_update_bits(codec,
				    MSM8X16_WCD_A_DIGITAL_CDC_DIG_CLK_CTL,
				    0x10, 0x10);
	} else {
		wcd8x16->adc_count--;
		if (!wcd8x16->adc_count) {
			snd_soc_update_bits(codec,
				    MSM8X16_WCD_A_DIGITAL_CDC_DIG_CLK_CTL,
				    0x10, 0x00);
			snd_soc_update_bits(codec,
				    MSM8X16_WCD_A_DIGITAL_CDC_ANA_CLK_CTL,
					    0x20, 0x0);
		}
	}
}

static int msm8x16_wcd_codec_enable_adc(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	u16 adc_reg;
	u8 init_bit_shift;

	dev_dbg(codec->dev, "%s %d\n", __func__, event);

	adc_reg = MSM8X16_WCD_A_ANALOG_TX_1_2_TEST_CTL_2;

	if (w->reg == MSM8X16_WCD_A_ANALOG_TX_1_EN)
		init_bit_shift = 5;
	else if ((w->reg == MSM8X16_WCD_A_ANALOG_TX_2_EN) ||
		 (w->reg == MSM8X16_WCD_A_ANALOG_TX_3_EN))
		init_bit_shift = 4;
	else {
		dev_err(codec->dev, "%s: Error, invalid adc register\n",
			__func__);
		return -EINVAL;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		msm8x16_wcd_codec_enable_adc_block(codec, 1);
		if (w->reg == MSM8X16_WCD_A_ANALOG_TX_2_EN)
			snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MICB_1_CTL, 0x02, 0x02);
		/*
		 * Add delay of 10 ms to give sufficient time for the voltage
		 * to shoot up and settle so that the txfe init does not
		 * happen when the input voltage is changing too much.
		 */
		usleep_range(10000, 10010);
		snd_soc_update_bits(codec, adc_reg, 1 << init_bit_shift,
				1 << init_bit_shift);
		if (w->reg == MSM8X16_WCD_A_ANALOG_TX_1_EN)
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_DIGITAL_CDC_CONN_TX1_CTL,
				0x03, 0x00);
		else if ((w->reg == MSM8X16_WCD_A_ANALOG_TX_2_EN) ||
			(w->reg == MSM8X16_WCD_A_ANALOG_TX_3_EN))
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_DIGITAL_CDC_CONN_TX2_CTL,
				0x03, 0x00);
		usleep_range(CODEC_DELAY_1_MS, CODEC_DELAY_1_1_MS);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/*
		 * Add delay of 12 ms before deasserting the init
		 * to reduce the tx pop
		 */
	usleep_range(12000, 12010);
		snd_soc_update_bits(codec, adc_reg, 1 << init_bit_shift, 0x00);
		usleep_range(CODEC_DELAY_1_MS, CODEC_DELAY_1_1_MS);
		break;
	case SND_SOC_DAPM_POST_PMD:
		msm8x16_wcd_codec_enable_adc_block(codec, 0);
		if (w->reg == MSM8X16_WCD_A_ANALOG_TX_2_EN)
			snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MICB_1_CTL, 0x02, 0x00);
		if (w->reg == MSM8X16_WCD_A_ANALOG_TX_1_EN)
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_DIGITAL_CDC_CONN_TX1_CTL,
				0x03, 0x02);
		else if ((w->reg == MSM8X16_WCD_A_ANALOG_TX_2_EN) ||
			(w->reg == MSM8X16_WCD_A_ANALOG_TX_3_EN))
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_DIGITAL_CDC_CONN_TX2_CTL,
				0x03, 0x02);

		break;
	}
	return 0;
}

static int msm8x16_wcd_codec_enable_spk_pa(struct snd_soc_dapm_widget *w,
				     struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	dev_dbg(w->codec->dev, "%s %d %s\n", __func__, event, w->name);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_DIGITAL_CDC_ANA_CLK_CTL, 0x10, 0x10);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_SPKR_PWRSTG_CTL, 0x01, 0x01);
		switch (msm8x16_wcd->boost_option) {
		case BOOST_SWITCH:
			if (!msm8x16_wcd->spk_boost_set)
				snd_soc_update_bits(codec,
					MSM8X16_WCD_A_ANALOG_SPKR_DAC_CTL,
					0x10, 0x10);
			break;
		case BOOST_ALWAYS:
		case BOOST_ON_FOREVER:
			break;
		case BYPASS_ALWAYS:
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_SPKR_DAC_CTL,
				0x10, 0x10);
			break;
		default:
			pr_err("%s: invalid boost option: %d\n", __func__,
						msm8x16_wcd->boost_option);
			break;
		}
		usleep_range(CODEC_DELAY_1_MS, CODEC_DELAY_1_1_MS);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_SPKR_PWRSTG_CTL, 0xE0, 0xE0);
		if (get_codec_version(msm8x16_wcd) != TOMBAK_1_0)
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_RX_EAR_CTL, 0x01, 0x01);
		break;
	case SND_SOC_DAPM_POST_PMU:
		usleep_range(CODEC_DELAY_1_MS, CODEC_DELAY_1_1_MS);
		switch (msm8x16_wcd->boost_option) {
		case BOOST_SWITCH:
			if (msm8x16_wcd->spk_boost_set)
				snd_soc_update_bits(codec,
					MSM8X16_WCD_A_ANALOG_SPKR_DRV_CTL,
					0xEF, 0xEF);
			else
				snd_soc_update_bits(codec,
					MSM8X16_WCD_A_ANALOG_SPKR_DAC_CTL,
					0x10, 0x00);
			break;
		case BOOST_ALWAYS:
		case BOOST_ON_FOREVER:
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_SPKR_DRV_CTL,
				0xEF, 0xEF);
			break;
		case BYPASS_ALWAYS:
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_SPKR_DAC_CTL, 0x10, 0x00);
			break;
		default:
			pr_err("%s: invalid boost option: %d\n", __func__,
						msm8x16_wcd->boost_option);
			break;
		}
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_CDC_RX3_B6_CTL, 0x01, 0x00);
		snd_soc_update_bits(codec, w->reg, 0x80, 0x80);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_CDC_RX3_B6_CTL, 0x01, 0x01);
		msm8x16_wcd->mute_mask |= SPKR_PA_DISABLE;
		/*
		 * Add 1 ms sleep for the mute to take effect
		 */
		usleep_range(CODEC_DELAY_1_MS, CODEC_DELAY_1_1_MS);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_SPKR_DAC_CTL, 0x10, 0x10);
		if (get_codec_version(msm8x16_wcd) < CAJON_2_0)
			msm8x16_wcd_boost_mode_sequence(codec, SPK_PMD);
		snd_soc_update_bits(codec, w->reg, 0x80, 0x00);
		switch (msm8x16_wcd->boost_option) {
		case BOOST_SWITCH:
			if (msm8x16_wcd->spk_boost_set)
				snd_soc_update_bits(codec,
					MSM8X16_WCD_A_ANALOG_SPKR_DRV_CTL,
					0xEF, 0x69);
			break;
		case BOOST_ALWAYS:
		case BOOST_ON_FOREVER:
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_SPKR_DRV_CTL,
				0xEF, 0x69);
			break;
		case BYPASS_ALWAYS:
			break;
		default:
			pr_err("%s: invalid boost option: %d\n", __func__,
						msm8x16_wcd->boost_option);
			break;
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_SPKR_PWRSTG_CTL, 0xE0, 0x00);
		usleep_range(CODEC_DELAY_1_MS, CODEC_DELAY_1_1_MS);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_SPKR_PWRSTG_CTL, 0x01, 0x00);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_SPKR_DAC_CTL, 0x10, 0x00);
		if (get_codec_version(msm8x16_wcd) != TOMBAK_1_0)
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_RX_EAR_CTL, 0x01, 0x00);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_DIGITAL_CDC_ANA_CLK_CTL, 0x10, 0x00);
		if (get_codec_version(msm8x16_wcd) >= CAJON_2_0)
			msm8x16_wcd_boost_mode_sequence(codec, SPK_PMD);
		break;
	}
	return 0;
}

static int msm8x16_wcd_codec_enable_dig_clk(struct snd_soc_dapm_widget *w,
				     struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);
	struct msm8916_asoc_mach_data *pdata = NULL;

	pdata = snd_soc_card_get_drvdata(codec->component.card);

	dev_dbg(w->codec->dev, "%s event %d w->name %s\n", __func__,
			event, w->name);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		msm8x16_wcd_codec_enable_clock_block(codec, 1);
		snd_soc_update_bits(codec, w->reg, 0x80, 0x80);
		msm8x16_wcd_boost_mode_sequence(codec, SPK_PMU);
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (msm8x16_wcd->rx_bias_count == 0)
			snd_soc_update_bits(codec,
					MSM8X16_WCD_A_DIGITAL_CDC_DIG_CLK_CTL,
					0x80, 0x00);
	}
	return 0;
}

static int msm8x16_wcd_codec_enable_dmic(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);
	u8  dmic_clk_en;
	u16 dmic_clk_reg;
	s32 *dmic_clk_cnt;
	unsigned int dmic;
	int ret;
	char *dec_num = strpbrk(w->name, "12");

	if (dec_num == NULL) {
		dev_err(codec->dev, "%s: Invalid DMIC\n", __func__);
		return -EINVAL;
	}

	ret = kstrtouint(dec_num, 10, &dmic);
	if (ret < 0) {
		dev_err(codec->dev,
			"%s: Invalid DMIC line on the codec\n", __func__);
		return -EINVAL;
	}

	switch (dmic) {
	case 1:
	case 2:
		dmic_clk_en = 0x01;
		dmic_clk_cnt = &(msm8x16_wcd->dmic_1_2_clk_cnt);
		dmic_clk_reg = MSM8X16_WCD_A_CDC_CLK_DMIC_B1_CTL;
		dev_dbg(codec->dev,
			"%s() event %d DMIC%d dmic_1_2_clk_cnt %d\n",
			__func__, event,  dmic, *dmic_clk_cnt);
		break;
	default:
		dev_err(codec->dev, "%s: Invalid DMIC Selection\n", __func__);
		return -EINVAL;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		(*dmic_clk_cnt)++;
		if (*dmic_clk_cnt == 1) {
			snd_soc_update_bits(codec, dmic_clk_reg,
					0x0E, 0x02);
			snd_soc_update_bits(codec, dmic_clk_reg,
					dmic_clk_en, dmic_clk_en);
		}
		if (dmic == 1)
			snd_soc_update_bits(codec,
			MSM8X16_WCD_A_CDC_TX1_DMIC_CTL, 0x07, 0x01);
		if (dmic == 2)
			snd_soc_update_bits(codec,
			MSM8X16_WCD_A_CDC_TX2_DMIC_CTL, 0x07, 0x01);
		break;
	case SND_SOC_DAPM_POST_PMD:
		(*dmic_clk_cnt)--;
		if (*dmic_clk_cnt  == 0)
			snd_soc_update_bits(codec, dmic_clk_reg,
					dmic_clk_en, 0);
		break;
	}
	return 0;
}

static bool msm8x16_wcd_use_mb(struct snd_soc_codec *codec)
{
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	if (get_codec_version(msm8x16_wcd) < CAJON)
		return true;
	else
		return false;
}

static void msm8x16_wcd_set_auto_zeroing(struct snd_soc_codec *codec,
					bool enable)
{
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	if (get_codec_version(msm8x16_wcd) < CONGA) {
		if (enable)
			/*
			 * Set autozeroing for special headset detection and
			 * buttons to work.
			 */
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_MICB_2_EN,
				0x18, 0x10);
		else
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_MICB_2_EN,
				0x18, 0x00);

	} else {
		pr_debug("%s: Auto Zeroing is not required from CONGA\n",
				__func__);
	}
}

static void msm8x16_trim_btn_reg(struct snd_soc_codec *codec)
{
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	if (get_codec_version(msm8x16_wcd) == TOMBAK_1_0) {
		pr_debug("%s: This device needs to be trimmed\n", __func__);
		/*
		 * Calculate the trim value for each device used
		 * till is comes in production by hardware team
		 */
		snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_SEC_ACCESS,
				0xA5, 0xA5);
		snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_TRIM_CTRL2,
				0xFF, 0x30);
	} else {
		pr_debug("%s: This device is trimmed at ATE\n", __func__);
	}
}
static int msm8x16_wcd_enable_ext_mb_source(struct snd_soc_codec *codec,
					    bool turn_on)
{
	int ret = 0;
	static int count;

	dev_dbg(codec->dev, "%s turn_on: %d count: %d\n", __func__, turn_on,
			count);
	if (turn_on) {
		if (!count) {
			ret = snd_soc_dapm_force_enable_pin(&codec->dapm,
				"MICBIAS_REGULATOR");
			snd_soc_dapm_sync(&codec->dapm);
		}
		count++;
	} else {
		if (count > 0)
			count--;
		if (!count) {
			ret = snd_soc_dapm_disable_pin(&codec->dapm,
				"MICBIAS_REGULATOR");
			snd_soc_dapm_sync(&codec->dapm);
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

static int msm8x16_wcd_codec_enable_micbias(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct msm8x16_wcd_priv *msm8x16_wcd =
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
	case MSM8X16_WCD_A_ANALOG_MICB_1_EN:
	case MSM8X16_WCD_A_ANALOG_MICB_2_EN:
		micb_int_reg = MSM8X16_WCD_A_ANALOG_MICB_1_INT_RBIAS;
		break;
	default:
		dev_err(codec->dev,
			"%s: Error, invalid micbias register 0x%x\n",
			__func__, w->reg);
		return -EINVAL;
	}

	micbias2 = (snd_soc_read(codec, MSM8X16_WCD_A_ANALOG_MICB_2_EN) & 0x80);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (strnstr(w->name, internal1_text, strlen(w->name))) {
			if (get_codec_version(msm8x16_wcd) >= CAJON)
				snd_soc_update_bits(codec,
					MSM8X16_WCD_A_ANALOG_TX_1_2_ATEST_CTL_2,
					0x02, 0x02);
			snd_soc_update_bits(codec, micb_int_reg, 0x80, 0x80);
		} else if (strnstr(w->name, internal2_text, strlen(w->name))) {
			snd_soc_update_bits(codec, micb_int_reg, 0x10, 0x10);
			snd_soc_update_bits(codec, w->reg, 0x60, 0x00);
		} else if (strnstr(w->name, internal3_text, strlen(w->name))) {
			snd_soc_update_bits(codec, micb_int_reg, 0x2, 0x2);
		/*
		 * update MSM8X16_WCD_A_ANALOG_TX_1_2_ATEST_CTL_2
		 * for external bias only, not for external2.
		*/
		} else if (!strnstr(w->name, external2_text, strlen(w->name)) &&
					strnstr(w->name, external_text,
						strlen(w->name))) {
			snd_soc_update_bits(codec,
					MSM8X16_WCD_A_ANALOG_TX_1_2_ATEST_CTL_2,
					0x02, 0x02);
		}
		if (!strnstr(w->name, external_text, strlen(w->name)))
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_MICB_1_EN, 0x05, 0x04);
		if (w->reg == MSM8X16_WCD_A_ANALOG_MICB_1_EN)
			msm8x16_wcd_configure_cap(codec, true, micbias2);

		break;
	case SND_SOC_DAPM_POST_PMU:
		if (get_codec_version(msm8x16_wcd) <= TOMBAK_2_0)
			usleep_range(20000, 20100);
		if (strnstr(w->name, internal1_text, strlen(w->name))) {
			snd_soc_update_bits(codec, micb_int_reg, 0x40, 0x40);
		} else if (strnstr(w->name, internal2_text,  strlen(w->name))) {
			snd_soc_update_bits(codec, micb_int_reg, 0x08, 0x08);
			msm8x16_notifier_call(codec,
					WCD_EVENT_POST_MICBIAS_2_ON);
		} else if (strnstr(w->name, internal3_text, 30)) {
			snd_soc_update_bits(codec, micb_int_reg, 0x01, 0x01);
		} else if (strnstr(w->name, external2_text, strlen(w->name))) {
			msm8x16_notifier_call(codec,
					WCD_EVENT_POST_MICBIAS_2_ON);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (strnstr(w->name, internal1_text, strlen(w->name))) {
			snd_soc_update_bits(codec, micb_int_reg, 0xC0, 0x40);
		} else if (strnstr(w->name, internal2_text, strlen(w->name))) {
			msm8x16_notifier_call(codec,
					WCD_EVENT_POST_MICBIAS_2_OFF);
		} else if (strnstr(w->name, internal3_text, 30)) {
			snd_soc_update_bits(codec, micb_int_reg, 0x2, 0x0);
		} else if (strnstr(w->name, external2_text, strlen(w->name))) {
			/*
			 * send micbias turn off event to mbhc driver and then
			 * break, as no need to set MICB_1_EN register.
			 */
			msm8x16_notifier_call(codec,
					WCD_EVENT_POST_MICBIAS_2_OFF);
			break;
		}
		if (w->reg == MSM8X16_WCD_A_ANALOG_MICB_1_EN)
			msm8x16_wcd_configure_cap(codec, false, micbias2);
		break;
	}
	return 0;
}

static void tx_hpf_corner_freq_callback(struct work_struct *work)
{
	struct delayed_work *hpf_delayed_work;
	struct hpf_work *hpf_work;
	struct msm8x16_wcd_priv *msm8x16_wcd;
	struct snd_soc_codec *codec;
	u16 tx_mux_ctl_reg;
	u8 hpf_cut_of_freq;

	hpf_delayed_work = to_delayed_work(work);
	hpf_work = container_of(hpf_delayed_work, struct hpf_work, dwork);
	msm8x16_wcd = hpf_work->msm8x16_wcd;
	codec = hpf_work->msm8x16_wcd->codec;
	hpf_cut_of_freq = hpf_work->tx_hpf_cut_of_freq;

	tx_mux_ctl_reg = MSM8X16_WCD_A_CDC_TX1_MUX_CTL +
			(hpf_work->decimator - 1) * 32;

	dev_dbg(codec->dev, "%s(): decimator %u hpf_cut_of_freq 0x%x\n",
		 __func__, hpf_work->decimator, (unsigned int)hpf_cut_of_freq);
	snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_TX_1_2_TXFE_CLKDIV, 0xFF, 0x51);

	snd_soc_update_bits(codec, tx_mux_ctl_reg, 0x30, hpf_cut_of_freq << 4);
}


#define  TX_MUX_CTL_CUT_OFF_FREQ_MASK	0x30
#define  CF_MIN_3DB_4HZ			0x0
#define  CF_MIN_3DB_75HZ		0x1
#define  CF_MIN_3DB_150HZ		0x2

static int msm8x16_wcd_codec_set_iir_gain(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	int value = 0, reg;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		if (w->shift == 0)
			reg = MSM8X16_WCD_A_CDC_IIR1_GAIN_B1_CTL;
		else if (w->shift == 1)
			reg = MSM8X16_WCD_A_CDC_IIR2_GAIN_B1_CTL;
		value = snd_soc_read(codec, reg);
		snd_soc_write(codec, reg, value);
		break;
	default:
		pr_err("%s: event = %d not expected\n", __func__, event);
	}
	return 0;
}

static int msm8x16_wcd_codec_enable_dec(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct msm8916_asoc_mach_data *pdata = NULL;
	unsigned int decimator;
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);
	char *dec_name = NULL;
	char *widget_name = NULL;
	char *temp;
	int ret = 0, i;
	u16 dec_reset_reg, tx_vol_ctl_reg, tx_mux_ctl_reg;
	u8 dec_hpf_cut_of_freq;
	int offset;
	char *dec_num;

	pdata = snd_soc_card_get_drvdata(codec->component.card);
	dev_dbg(codec->dev, "%s %d\n", __func__, event);

	widget_name = kstrndup(w->name, 15, GFP_KERNEL);
	if (!widget_name)
		return -ENOMEM;
	temp = widget_name;

	dec_name = strsep(&widget_name, " ");
	widget_name = temp;
	if (!dec_name) {
		dev_err(codec->dev,
			"%s: Invalid decimator = %s\n", __func__, w->name);
		ret = -EINVAL;
		goto out;
	}

	dec_num = strpbrk(dec_name, "1234");
	if (dec_num == NULL) {
		dev_err(codec->dev, "%s: Invalid Decimator\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	ret = kstrtouint(dec_num, 10, &decimator);
	if (ret < 0) {
		dev_err(codec->dev,
			"%s: Invalid decimator = %s\n", __func__, dec_name);
		ret = -EINVAL;
		goto out;
	}

	dev_dbg(codec->dev,
		"%s(): widget = %s dec_name = %s decimator = %u\n", __func__,
		w->name, dec_name, decimator);

	if (w->reg == MSM8X16_WCD_A_CDC_CLK_TX_CLK_EN_B1_CTL) {
		dec_reset_reg = MSM8X16_WCD_A_CDC_CLK_TX_RESET_B1_CTL;
		offset = 0;
	} else {
		dev_err(codec->dev, "%s: Error, incorrect dec\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	tx_vol_ctl_reg = MSM8X16_WCD_A_CDC_TX1_VOL_CTL_CFG +
			 32 * (decimator - 1);
	tx_mux_ctl_reg = MSM8X16_WCD_A_CDC_TX1_MUX_CTL +
			  32 * (decimator - 1);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (decimator == 3 || decimator == 4) {
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_CLK_WSA_VI_B1_CTL,
				0xFF, 0x5);
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_TX1_DMIC_CTL +
					(decimator - 1) * 0x20, 0x7, 0x2);
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_TX1_DMIC_CTL +
					(decimator - 1) * 0x20, 0x7, 0x2);
		}
		/* Enableable TX digital mute */
		snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x01, 0x01);
		for (i = 0; i < NUM_DECIMATORS; i++) {
			if (decimator == i + 1)
				msm8x16_wcd->dec_active[i] = true;
		}

		dec_hpf_cut_of_freq = snd_soc_read(codec, tx_mux_ctl_reg);

		dec_hpf_cut_of_freq = (dec_hpf_cut_of_freq & 0x30) >> 4;

		tx_hpf_work[decimator - 1].tx_hpf_cut_of_freq =
			dec_hpf_cut_of_freq;

		if (dec_hpf_cut_of_freq != CF_MIN_3DB_150HZ) {

			/* set cut of freq to CF_MIN_3DB_150HZ (0x1); */
			snd_soc_update_bits(codec, tx_mux_ctl_reg, 0x30,
					    CF_MIN_3DB_150HZ << 4);
		}
		snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_TX_1_2_TXFE_CLKDIV,
				0xFF, 0x42);

		break;
	case SND_SOC_DAPM_POST_PMU:
		/* enable HPF */
		snd_soc_update_bits(codec, tx_mux_ctl_reg , 0x08, 0x00);

		if (tx_hpf_work[decimator - 1].tx_hpf_cut_of_freq !=
				CF_MIN_3DB_150HZ) {

			schedule_delayed_work(&tx_hpf_work[decimator - 1].dwork,
					msecs_to_jiffies(300));
		}
		/* apply the digital gain after the decimator is enabled*/
		if ((w->shift) < ARRAY_SIZE(tx_digital_gain_reg))
			snd_soc_write(codec,
				  tx_digital_gain_reg[w->shift + offset],
				  snd_soc_read(codec,
				  tx_digital_gain_reg[w->shift + offset])
				  );
		if (pdata->lb_mode) {
			pr_debug("%s: loopback mode unmute the DEC\n",
							__func__);
			snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x01, 0x00);
		}
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x01, 0x01);
		msleep(20);
		snd_soc_update_bits(codec, tx_mux_ctl_reg, 0x08, 0x08);
		cancel_delayed_work_sync(&tx_hpf_work[decimator - 1].dwork);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, dec_reset_reg, 1 << w->shift,
			1 << w->shift);
		snd_soc_update_bits(codec, dec_reset_reg, 1 << w->shift, 0x0);
		snd_soc_update_bits(codec, tx_mux_ctl_reg, 0x08, 0x08);
		snd_soc_update_bits(codec, tx_mux_ctl_reg, 0x30,
			(tx_hpf_work[decimator - 1].tx_hpf_cut_of_freq) << 4);
		snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x01, 0x00);
		for (i = 0; i < NUM_DECIMATORS; i++) {
			if (decimator == i + 1)
				msm8x16_wcd->dec_active[i] = false;
		}
		if (decimator == 3 || decimator == 4) {
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_CLK_WSA_VI_B1_CTL,
				0xFF, 0x0);
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_TX1_DMIC_CTL +
					(decimator - 1) * 0x20, 0x7, 0x0);
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_TX1_DMIC_CTL +
					(decimator - 1) * 0x20, 0x7, 0x0);
		}
		break;
	}
out:
	kfree(widget_name);
	return ret;
}

static int msm89xx_wcd_codec_enable_vdd_spkr(struct snd_soc_dapm_widget *w,
				       struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	if (!msm8x16_wcd->ext_spk_boost_set) {
		dev_dbg(codec->dev, "%s: ext_boost not supported/disabled\n",
								__func__);
		return 0;
	}
	dev_dbg(codec->dev, "%s: %s %d\n", __func__, w->name, event);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (msm8x16_wcd->spkdrv_reg) {
			ret = regulator_enable(msm8x16_wcd->spkdrv_reg);
			if (ret)
				dev_err(codec->dev,
					"%s Failed to enable spkdrv reg %s\n",
					__func__, MSM89XX_VDD_SPKDRV_NAME);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (msm8x16_wcd->spkdrv_reg) {
			ret = regulator_disable(msm8x16_wcd->spkdrv_reg);
			if (ret)
				dev_err(codec->dev,
					"%s: Failed to disable spkdrv_reg %s\n",
					__func__, MSM89XX_VDD_SPKDRV_NAME);
		}
		break;
	}
	return 0;
}

static int msm8x16_wcd_codec_config_compander(struct snd_soc_codec *codec,
					int interp_n, int event)
{
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s: event %d shift %d, enabled %d\n",
		__func__, event, interp_n,
		msm8x16_wcd->comp_enabled[interp_n]);

	/* compander is not enabled */
	if (!msm8x16_wcd->comp_enabled[interp_n]) {
		if (interp_n < MSM8X16_WCD_RX3)
			if (get_codec_version(msm8x16_wcd) >= DIANGU)
				snd_soc_update_bits(codec,
					MSM8X16_WCD_A_ANALOG_RX_COM_BIAS_DAC,
					0x08, 0x00);
		return 0;
	}

	switch (msm8x16_wcd->comp_enabled[interp_n]) {
	case COMPANDER_1:
		if (SND_SOC_DAPM_EVENT_ON(event)) {
			if (get_codec_version(msm8x16_wcd) >= DIANGU)
				snd_soc_update_bits(codec,
					MSM8X16_WCD_A_ANALOG_RX_COM_BIAS_DAC,
					0x08, 0x08);
			/* Enable Compander Clock */
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_COMP0_B2_CTL, 0x0F, 0x09);
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_CLK_RX_B2_CTL, 0x01, 0x01);
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_COMP0_B1_CTL,
				1 << interp_n, 1 << interp_n);
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_COMP0_B3_CTL, 0xFF, 0x01);
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_COMP0_B2_CTL, 0xF0, 0x50);
			/* add sleep for compander to settle */
			usleep_range(1000, 1100);
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_COMP0_B3_CTL, 0xFF, 0x28);
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_COMP0_B2_CTL, 0xF0, 0xB0);

			/* Enable Compander GPIO */
			if (msm8x16_wcd->codec_hph_comp_gpio)
				msm8x16_wcd->codec_hph_comp_gpio(1);
		} else if (SND_SOC_DAPM_EVENT_OFF(event)) {
			/* Disable Compander GPIO */
			if (msm8x16_wcd->codec_hph_comp_gpio)
				msm8x16_wcd->codec_hph_comp_gpio(0);

			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_COMP0_B2_CTL, 0x0F, 0x05);
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_COMP0_B1_CTL,
				1 << interp_n, 0);
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_CLK_RX_B2_CTL, 0x01, 0x00);
		}
		break;
	default:
		dev_dbg(codec->dev, "%s: Invalid compander %d\n", __func__,
				msm8x16_wcd->comp_enabled[interp_n]);
		break;
	};

	return 0;
}

static int msm8x16_wcd_codec_enable_interpolator(struct snd_soc_dapm_widget *w,
						 struct snd_kcontrol *kcontrol,
						 int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s %d %s\n", __func__, event, w->name);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		msm8x16_wcd_codec_config_compander(codec, w->shift, event);
		/* apply the digital gain after the interpolator is enabled*/
		if ((w->shift) < ARRAY_SIZE(rx_digital_gain_reg))
			snd_soc_write(codec,
				  rx_digital_gain_reg[w->shift],
				  snd_soc_read(codec,
				  rx_digital_gain_reg[w->shift])
				  );
		break;
	case SND_SOC_DAPM_POST_PMD:
		msm8x16_wcd_codec_config_compander(codec, w->shift, event);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_CDC_CLK_RX_RESET_CTL,
			1 << w->shift, 1 << w->shift);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_CDC_CLK_RX_RESET_CTL,
			1 << w->shift, 0x0);
		/*
		 * disable the mute enabled during the PMD of this device
		 */
		if ((w->shift == 0) &&
			(msm8x16_wcd->mute_mask & HPHL_PA_DISABLE)) {
			pr_debug("disabling HPHL mute\n");
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_RX1_B6_CTL, 0x01, 0x00);
			if (get_codec_version(msm8x16_wcd) >= CAJON)
				snd_soc_update_bits(codec,
					MSM8X16_WCD_A_ANALOG_RX_HPH_BIAS_CNP,
					0xF0, 0x20);
			msm8x16_wcd->mute_mask &= ~(HPHL_PA_DISABLE);
		} else if ((w->shift == 1) &&
				(msm8x16_wcd->mute_mask & HPHR_PA_DISABLE)) {
			pr_debug("disabling HPHR mute\n");
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_RX2_B6_CTL, 0x01, 0x00);
			if (get_codec_version(msm8x16_wcd) >= CAJON)
				snd_soc_update_bits(codec,
					MSM8X16_WCD_A_ANALOG_RX_HPH_BIAS_CNP,
					0xF0, 0x20);
			msm8x16_wcd->mute_mask &= ~(HPHR_PA_DISABLE);
		} else if ((w->shift == 2) &&
				(msm8x16_wcd->mute_mask & SPKR_PA_DISABLE)) {
			pr_debug("disabling SPKR mute\n");
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_RX3_B6_CTL, 0x01, 0x00);
			msm8x16_wcd->mute_mask &= ~(SPKR_PA_DISABLE);
		} else if ((w->shift == 0) &&
				(msm8x16_wcd->mute_mask & EAR_PA_DISABLE)) {
			pr_debug("disabling EAR mute\n");
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_RX1_B6_CTL, 0x01, 0x00);
			msm8x16_wcd->mute_mask &= ~(EAR_PA_DISABLE);
		}
	}
	return 0;
}


/* The register address is the same as other codec so it can use resmgr */
static int msm8x16_wcd_codec_enable_rx_bias(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		msm8x16_wcd->rx_bias_count++;
		if (msm8x16_wcd->rx_bias_count == 1) {
			snd_soc_update_bits(codec,
					MSM8X16_WCD_A_ANALOG_RX_COM_BIAS_DAC,
					0x80, 0x80);
			snd_soc_update_bits(codec,
					MSM8X16_WCD_A_ANALOG_RX_COM_BIAS_DAC,
					0x01, 0x01);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		msm8x16_wcd->rx_bias_count--;
		if (msm8x16_wcd->rx_bias_count == 0) {
			snd_soc_update_bits(codec,
					MSM8X16_WCD_A_ANALOG_RX_COM_BIAS_DAC,
					0x01, 0x00);
			snd_soc_update_bits(codec,
					MSM8X16_WCD_A_ANALOG_RX_COM_BIAS_DAC,
					0x80, 0x00);
		}
		break;
	}
	dev_dbg(codec->dev, "%s rx_bias_count = %d\n",
			__func__, msm8x16_wcd->rx_bias_count);
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

void wcd_imped_config(struct snd_soc_codec *codec,
			uint32_t imped, bool set_gain)
{
	uint32_t value;
	int codec_version;
	struct msm8x16_wcd_priv *msm8x16_wcd =
				snd_soc_codec_get_drvdata(codec);

	value = wcd_get_impedance_value(imped);

	if (value < wcd_imped_val[0]) {
		pr_debug("%s, detected impedance is less than 4 Ohm\n",
			 __func__);
		return;
	}

	codec_version = get_codec_version(msm8x16_wcd);

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
					MSM8X16_WCD_A_ANALOG_RX_EAR_CTL,
					0x20, 0x20);
			else
				snd_soc_update_bits(codec,
					MSM8X16_WCD_A_ANALOG_RX_EAR_CTL,
					0x20, 0x00);
			break;
		case CAJON:
		case CAJON_2_0:
		case DIANGU:
			if (value >= 13) {
				snd_soc_update_bits(codec,
					MSM8X16_WCD_A_ANALOG_RX_EAR_CTL,
					0x20, 0x20);
				snd_soc_update_bits(codec,
					MSM8X16_WCD_A_ANALOG_NCP_VCTRL,
					0x07, 0x07);
			} else {
				snd_soc_update_bits(codec,
					MSM8X16_WCD_A_ANALOG_RX_EAR_CTL,
					0x20, 0x00);
				snd_soc_update_bits(codec,
					MSM8X16_WCD_A_ANALOG_NCP_VCTRL,
					0x07, 0x04);
			}
			break;
		}
	} else {
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_RX_EAR_CTL,
			0x20, 0x00);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_NCP_VCTRL,
			0x07, 0x04);
	}

	pr_debug("%s: Exit\n", __func__);
}

static int msm8x16_wcd_hphl_dac_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	uint32_t impedl, impedr;
	struct snd_soc_codec *codec = w->codec;
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);
	int ret;

	dev_dbg(codec->dev, "%s %s %d\n", __func__, w->name, event);
	ret = wcd_mbhc_get_impedance(&msm8x16_wcd->mbhc,
			&impedl, &impedr);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (get_codec_version(msm8x16_wcd) > CAJON)
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_RX_HPH_CNP_EN,
				0x08, 0x08);
		if (get_codec_version(msm8x16_wcd) == CAJON ||
			get_codec_version(msm8x16_wcd) == CAJON_2_0) {
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_RX_HPH_L_TEST,
				0x80, 0x80);
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_RX_HPH_R_TEST,
				0x80, 0x80);
		}
		if (get_codec_version(msm8x16_wcd) > CAJON)
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_RX_HPH_CNP_EN,
				0x08, 0x00);
		if (HD2_MODE == msm8x16_wcd->hph_mode) {
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_RX1_B3_CTL, 0x1C, 0x14);
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_RX1_B4_CTL, 0x18, 0x10);
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_RX1_B3_CTL, 0x80, 0x80);
		}
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_RX_HPH_L_PA_DAC_CTL, 0x02, 0x02);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_DIGITAL_CDC_DIG_CLK_CTL, 0x01, 0x01);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_DIGITAL_CDC_ANA_CLK_CTL, 0x02, 0x02);
		if (!ret)
			wcd_imped_config(codec, impedl, true);
		else
			dev_dbg(codec->dev, "Failed to get mbhc impedance %d\n",
				ret);
		break;
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_RX_HPH_L_PA_DAC_CTL, 0x02, 0x00);
		break;
	case SND_SOC_DAPM_POST_PMD:
		wcd_imped_config(codec, impedl, false);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_DIGITAL_CDC_ANA_CLK_CTL, 0x02, 0x00);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_DIGITAL_CDC_DIG_CLK_CTL, 0x01, 0x00);
		if (HD2_MODE == msm8x16_wcd->hph_mode) {
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_RX1_B3_CTL, 0x1C, 0x00);
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_RX1_B4_CTL, 0x18, 0xFF);
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_RX1_B3_CTL, 0x80, 0x00);
		}
		break;
	}
	return 0;
}

static int msm8x16_wcd_lo_dac_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	dev_dbg(codec->dev, "%s %s %d\n", __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_DIGITAL_CDC_ANA_CLK_CTL, 0x10, 0x10);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_RX_LO_EN_CTL, 0x20, 0x20);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_RX_LO_EN_CTL, 0x80, 0x80);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_RX_LO_DAC_CTL, 0x08, 0x08);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_RX_LO_DAC_CTL, 0x40, 0x40);
		break;
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_RX_LO_DAC_CTL, 0x80, 0x80);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_RX_LO_DAC_CTL, 0x08, 0x00);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_RX_LO_EN_CTL, 0x40, 0x40);
		break;
	case SND_SOC_DAPM_POST_PMD:
		usleep_range(20000, 20100);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_RX_LO_DAC_CTL, 0x80, 0x00);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_RX_LO_DAC_CTL, 0x40, 0x00);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_RX_LO_DAC_CTL, 0x08, 0x00);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_RX_LO_EN_CTL, 0x80, 0x00);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_RX_LO_EN_CTL, 0x40, 0x00);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_RX_LO_EN_CTL, 0x20, 0x00);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_DIGITAL_CDC_ANA_CLK_CTL, 0x10, 0x00);
		break;
	}
	return 0;
}

static int msm8x16_wcd_hphr_dac_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s %s %d\n", __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (HD2_MODE == msm8x16_wcd->hph_mode) {
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_RX2_B3_CTL, 0x1C, 0x14);
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_RX2_B4_CTL, 0x18, 0x10);
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_RX2_B3_CTL, 0x80, 0x80);
		}
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_RX_HPH_R_PA_DAC_CTL, 0x02, 0x02);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_DIGITAL_CDC_DIG_CLK_CTL, 0x02, 0x02);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_DIGITAL_CDC_ANA_CLK_CTL, 0x01, 0x01);
		break;
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_RX_HPH_R_PA_DAC_CTL, 0x02, 0x00);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_DIGITAL_CDC_ANA_CLK_CTL, 0x01, 0x00);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_DIGITAL_CDC_DIG_CLK_CTL, 0x02, 0x00);
		if (HD2_MODE == msm8x16_wcd->hph_mode) {
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_RX2_B3_CTL, 0x1C, 0x00);
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_RX2_B4_CTL, 0x18, 0xFF);
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_RX2_B3_CTL, 0x80, 0x00);
		}
		break;
	}
	return 0;
}

static int msm8x16_wcd_hph_pa_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s: %s event = %d\n", __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (w->shift == 5)
			msm8x16_notifier_call(codec,
					WCD_EVENT_PRE_HPHL_PA_ON);
		else if (w->shift == 4)
			msm8x16_notifier_call(codec,
					WCD_EVENT_PRE_HPHR_PA_ON);
		snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_NCP_FBCTRL, 0x20, 0x20);
		break;

	case SND_SOC_DAPM_POST_PMU:
		usleep_range(7000, 7100);
		if (w->shift == 5) {
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_RX_HPH_L_TEST, 0x04, 0x04);
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_RX1_B6_CTL, 0x01, 0x00);
		} else if (w->shift == 4) {
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_RX_HPH_R_TEST, 0x04, 0x04);
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_RX2_B6_CTL, 0x01, 0x00);
		}
		break;

	case SND_SOC_DAPM_PRE_PMD:
		if (w->shift == 5) {
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_RX1_B6_CTL, 0x01, 0x01);
			msleep(20);
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_RX_HPH_L_TEST, 0x04, 0x00);
			msm8x16_wcd->mute_mask |= HPHL_PA_DISABLE;
			msm8x16_notifier_call(codec,
					WCD_EVENT_PRE_HPHL_PA_OFF);
		} else if (w->shift == 4) {
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_RX2_B6_CTL, 0x01, 0x01);
			msleep(20);
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_RX_HPH_R_TEST, 0x04, 0x00);
			msm8x16_wcd->mute_mask |= HPHR_PA_DISABLE;
			msm8x16_notifier_call(codec,
					WCD_EVENT_PRE_HPHR_PA_OFF);
		}
		if (get_codec_version(msm8x16_wcd) >= CAJON) {
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_RX_HPH_BIAS_CNP,
				0xF0, 0x30);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (w->shift == 5) {
			clear_bit(WCD_MBHC_HPHL_PA_OFF_ACK,
				&msm8x16_wcd->mbhc.hph_pa_dac_state);
			msm8x16_notifier_call(codec,
					WCD_EVENT_POST_HPHL_PA_OFF);
		} else if (w->shift == 4) {
			clear_bit(WCD_MBHC_HPHR_PA_OFF_ACK,
				&msm8x16_wcd->mbhc.hph_pa_dac_state);
			msm8x16_notifier_call(codec,
					WCD_EVENT_POST_HPHR_PA_OFF);
		}
		usleep_range(4000, 4100);
		usleep_range(CODEC_DELAY_1_MS, CODEC_DELAY_1_1_MS);

		dev_dbg(codec->dev,
			"%s: sleep 10 ms after %s PA disable.\n", __func__,
			w->name);
		usleep_range(10000, 10100);
		break;
	}
	return 0;
}

static const struct snd_soc_dapm_route audio_map[] = {
	{"RX_I2S_CLK", NULL, "CDC_CONN"},
	{"I2S RX1", NULL, "RX_I2S_CLK"},
	{"I2S RX2", NULL, "RX_I2S_CLK"},
	{"I2S RX3", NULL, "RX_I2S_CLK"},

	{"I2S TX1", NULL, "TX_I2S_CLK"},
	{"I2S TX2", NULL, "TX_I2S_CLK"},
	{"AIF2 VI", NULL, "TX_I2S_CLK"},

	{"I2S TX1", NULL, "DEC1 MUX"},
	{"I2S TX2", NULL, "DEC2 MUX"},
	{"AIF2 VI", NULL, "DEC3 MUX"},
	{"AIF2 VI", NULL, "DEC4 MUX"},

	/* RDAC Connections */
	{"HPHR DAC", NULL, "RDAC2 MUX"},
	{"RDAC2 MUX", "RX1", "RX1 CHAIN"},
	{"RDAC2 MUX", "RX2", "RX2 CHAIN"},

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
	{"HPHL DAC", NULL, "RX1 CHAIN"},

	{"SPK_OUT", NULL, "SPK PA"},
	{"SPK PA", NULL, "SPK_RX_BIAS"},
	{"SPK PA", NULL, "SPK"},
	{"SPK", "Switch", "SPK DAC"},
	{"SPK DAC", NULL, "RX3 CHAIN"},
	{"SPK DAC", NULL, "VDD_SPKDRV"},

	/* lineout */
	{"LINEOUT", NULL, "LINEOUT PA"},
	{"LINEOUT PA", NULL, "SPK_RX_BIAS"},
	{"LINEOUT PA", NULL, "LINE_OUT"},
	{"LINE_OUT", "Switch", "LINEOUT DAC"},
	{"LINEOUT DAC", NULL, "RX3 CHAIN"},

	/* lineout to WSA */
	{"WSA_SPK OUT", NULL, "LINEOUT PA"},

	{"RX1 CHAIN", NULL, "RX1 CLK"},
	{"RX2 CHAIN", NULL, "RX2 CLK"},
	{"RX3 CHAIN", NULL, "RX3 CLK"},
	{"RX1 CHAIN", NULL, "RX1 MIX2"},
	{"RX2 CHAIN", NULL, "RX2 MIX2"},
	{"RX3 CHAIN", NULL, "RX3 MIX1"},

	{"RX1 MIX1", NULL, "RX1 MIX1 INP1"},
	{"RX1 MIX1", NULL, "RX1 MIX1 INP2"},
	{"RX1 MIX1", NULL, "RX1 MIX1 INP3"},
	{"RX2 MIX1", NULL, "RX2 MIX1 INP1"},
	{"RX2 MIX1", NULL, "RX2 MIX1 INP2"},
	{"RX3 MIX1", NULL, "RX3 MIX1 INP1"},
	{"RX3 MIX1", NULL, "RX3 MIX1 INP2"},
	{"RX1 MIX2", NULL, "RX1 MIX1"},
	{"RX1 MIX2", NULL, "RX1 MIX2 INP1"},
	{"RX2 MIX2", NULL, "RX2 MIX1"},
	{"RX2 MIX2", NULL, "RX2 MIX2 INP1"},

	{"RX1 MIX1 INP1", "RX1", "I2S RX1"},
	{"RX1 MIX1 INP1", "RX2", "I2S RX2"},
	{"RX1 MIX1 INP1", "RX3", "I2S RX3"},
	{"RX1 MIX1 INP1", "IIR1", "IIR1"},
	{"RX1 MIX1 INP1", "IIR2", "IIR2"},
	{"RX1 MIX1 INP2", "RX1", "I2S RX1"},
	{"RX1 MIX1 INP2", "RX2", "I2S RX2"},
	{"RX1 MIX1 INP2", "RX3", "I2S RX3"},
	{"RX1 MIX1 INP2", "IIR1", "IIR1"},
	{"RX1 MIX1 INP2", "IIR2", "IIR2"},
	{"RX1 MIX1 INP3", "RX1", "I2S RX1"},
	{"RX1 MIX1 INP3", "RX2", "I2S RX2"},
	{"RX1 MIX1 INP3", "RX3", "I2S RX3"},

	{"RX2 MIX1 INP1", "RX1", "I2S RX1"},
	{"RX2 MIX1 INP1", "RX2", "I2S RX2"},
	{"RX2 MIX1 INP1", "RX3", "I2S RX3"},
	{"RX2 MIX1 INP1", "IIR1", "IIR1"},
	{"RX2 MIX1 INP1", "IIR2", "IIR2"},
	{"RX2 MIX1 INP2", "RX1", "I2S RX1"},
	{"RX2 MIX1 INP2", "RX2", "I2S RX2"},
	{"RX2 MIX1 INP2", "RX3", "I2S RX3"},
	{"RX2 MIX1 INP2", "IIR1", "IIR1"},
	{"RX2 MIX1 INP2", "IIR2", "IIR2"},
	{"RX2 MIX1 INP3", "RX1", "I2S RX1"},
	{"RX2 MIX1 INP3", "RX2", "I2S RX2"},
	{"RX2 MIX1 INP3", "RX3", "I2S RX3"},

	{"RX3 MIX1 INP1", "RX1", "I2S RX1"},
	{"RX3 MIX1 INP1", "RX2", "I2S RX2"},
	{"RX3 MIX1 INP1", "RX3", "I2S RX3"},
	{"RX3 MIX1 INP1", "IIR1", "IIR1"},
	{"RX3 MIX1 INP1", "IIR2", "IIR2"},
	{"RX3 MIX1 INP2", "RX1", "I2S RX1"},
	{"RX3 MIX1 INP2", "RX2", "I2S RX2"},
	{"RX3 MIX1 INP2", "RX3", "I2S RX3"},
	{"RX3 MIX1 INP2", "IIR1", "IIR1"},
	{"RX3 MIX1 INP2", "IIR2", "IIR2"},
	{"RX3 MIX1 INP3", "RX1", "I2S RX1"},
	{"RX3 MIX1 INP3", "RX2", "I2S RX2"},
	{"RX3 MIX1 INP3", "RX3", "I2S RX3"},

	{"RX1 MIX2 INP1", "IIR1", "IIR1"},
	{"RX2 MIX2 INP1", "IIR1", "IIR1"},
	{"RX1 MIX2 INP1", "IIR2", "IIR2"},
	{"RX2 MIX2 INP1", "IIR2", "IIR2"},

	/* Decimator Inputs */
	{"DEC1 MUX", "DMIC1", "DMIC1"},
	{"DEC1 MUX", "DMIC2", "DMIC2"},
	{"DEC1 MUX", "ADC1", "ADC1"},
	{"DEC1 MUX", "ADC2", "ADC2"},
	{"DEC1 MUX", "ADC3", "ADC3"},
	{"DEC1 MUX", NULL, "CDC_CONN"},

	{"DEC2 MUX", "DMIC1", "DMIC1"},
	{"DEC2 MUX", "DMIC2", "DMIC2"},
	{"DEC2 MUX", "ADC1", "ADC1"},
	{"DEC2 MUX", "ADC2", "ADC2"},
	{"DEC2 MUX", "ADC3", "ADC3"},
	{"DEC2 MUX", NULL, "CDC_CONN"},

	{"DEC3 MUX", "DMIC3", "DMIC3"},
	{"DEC4 MUX", "DMIC4", "DMIC4"},
	{"DEC3 MUX", NULL, "CDC_CONN"},
	{"DEC4 MUX", NULL, "CDC_CONN"},
	/* ADC Connections */
	{"ADC2", NULL, "ADC2 MUX"},
	{"ADC3", NULL, "ADC2 MUX"},
	{"ADC2 MUX", "INP2", "ADC2_INP2"},
	{"ADC2 MUX", "INP3", "ADC2_INP3"},

	{"ADC1", NULL, "AMIC1"},
	{"ADC2_INP2", NULL, "AMIC2"},
	{"ADC2_INP3", NULL, "AMIC3"},

	/* TODO: Fix this */
	{"IIR1", NULL, "IIR1 INP1 MUX"},
	{"IIR1 INP1 MUX", "DEC1", "DEC1 MUX"},
	{"IIR1 INP1 MUX", "DEC2", "DEC2 MUX"},
	{"IIR2", NULL, "IIR2 INP1 MUX"},
	{"IIR2 INP1 MUX", "DEC1", "DEC1 MUX"},
	{"IIR2 INP1 MUX", "DEC2", "DEC2 MUX"},
	{"MIC BIAS Internal1", NULL, "INT_LDO_H"},
	{"MIC BIAS Internal2", NULL, "INT_LDO_H"},
	{"MIC BIAS External", NULL, "INT_LDO_H"},
	{"MIC BIAS External2", NULL, "INT_LDO_H"},
	{"MIC BIAS Internal1", NULL, "MICBIAS_REGULATOR"},
	{"MIC BIAS Internal2", NULL, "MICBIAS_REGULATOR"},
	{"MIC BIAS External", NULL, "MICBIAS_REGULATOR"},
	{"MIC BIAS External2", NULL, "MICBIAS_REGULATOR"},
};

static int msm8x16_wcd_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct msm8x16_wcd_priv *msm8x16_wcd =
		snd_soc_codec_get_drvdata(dai->codec);

	dev_dbg(dai->codec->dev, "%s(): substream = %s  stream = %d\n",
		__func__,
		substream->name, substream->stream);
	/*
	 * If status_mask is BU_DOWN it means SSR is not complete.
	 * So retun error.
	 */
	if (test_bit(BUS_DOWN, &msm8x16_wcd->status_mask)) {
		dev_err(dai->codec->dev, "Error, Device is not up post SSR\n");
		return -EINVAL;
	}
	return 0;
}

static void msm8x16_wcd_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	dev_dbg(dai->codec->dev,
		"%s(): substream = %s  stream = %d\n" , __func__,
		substream->name, substream->stream);
}

int msm8x16_wcd_mclk_enable(struct snd_soc_codec *codec,
			    int mclk_enable, bool dapm)
{
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s: mclk_enable = %u, dapm = %d\n",
		__func__, mclk_enable, dapm);
	if (mclk_enable) {
		msm8x16_wcd->mclk_enabled = true;
		msm8x16_wcd_codec_enable_clock_block(codec, 1);
	} else {
		if (!msm8x16_wcd->mclk_enabled) {
			dev_err(codec->dev, "Error, MCLK already diabled\n");
			return -EINVAL;
		}
		msm8x16_wcd->mclk_enabled = false;
		msm8x16_wcd_codec_enable_clock_block(codec, 0);
	}
	return 0;
}

static int msm8x16_wcd_set_dai_sysclk(struct snd_soc_dai *dai,
		int clk_id, unsigned int freq, int dir)
{
	dev_dbg(dai->codec->dev, "%s\n", __func__);
	return 0;
}

static int msm8x16_wcd_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	dev_dbg(dai->codec->dev, "%s\n", __func__);
	return 0;
}

static int msm8x16_wcd_set_channel_map(struct snd_soc_dai *dai,
				unsigned int tx_num, unsigned int *tx_slot,
				unsigned int rx_num, unsigned int *rx_slot)

{
	dev_dbg(dai->codec->dev, "%s\n", __func__);
	return 0;
}

static int msm8x16_wcd_get_channel_map(struct snd_soc_dai *dai,
				 unsigned int *tx_num, unsigned int *tx_slot,
				 unsigned int *rx_num, unsigned int *rx_slot)

{
	dev_dbg(dai->codec->dev, "%s\n", __func__);
	return 0;
}

static int msm8x16_wcd_set_interpolator_rate(struct snd_soc_dai *dai,
	u8 rx_fs_rate_reg_val, u32 sample_rate)
{
	snd_soc_update_bits(dai->codec,
			MSM8X16_WCD_A_CDC_RX1_B5_CTL, 0xF0, rx_fs_rate_reg_val);
	snd_soc_update_bits(dai->codec,
			MSM8X16_WCD_A_CDC_RX2_B5_CTL, 0xF0, rx_fs_rate_reg_val);
	return 0;
}

static int msm8x16_wcd_set_decimator_rate(struct snd_soc_dai *dai,
	u8 tx_fs_rate_reg_val, u32 sample_rate)
{
	return 0;
}

static int msm8x16_wcd_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params,
			    struct snd_soc_dai *dai)
{
	u8 tx_fs_rate, rx_fs_rate, rx_clk_fs_rate;
	int ret;

	dev_dbg(dai->codec->dev,
		"%s: dai_name = %s DAI-ID %x rate %d num_ch %d format %d\n",
		__func__, dai->name, dai->id, params_rate(params),
		params_channels(params), params_format(params));

	switch (params_rate(params)) {
	case 8000:
		tx_fs_rate = 0x00;
		rx_fs_rate = 0x00;
		rx_clk_fs_rate = 0x00;
		break;
	case 16000:
		tx_fs_rate = 0x20;
		rx_fs_rate = 0x20;
		rx_clk_fs_rate = 0x01;
		break;
	case 32000:
		tx_fs_rate = 0x40;
		rx_fs_rate = 0x40;
		rx_clk_fs_rate = 0x02;
		break;
	case 48000:
		tx_fs_rate = 0x60;
		rx_fs_rate = 0x60;
		rx_clk_fs_rate = 0x03;
		break;
	case 96000:
		tx_fs_rate = 0x80;
		rx_fs_rate = 0x80;
		rx_clk_fs_rate = 0x04;
		break;
	case 192000:
		tx_fs_rate = 0xA0;
		rx_fs_rate = 0xA0;
		rx_clk_fs_rate = 0x05;
		break;
	default:
		dev_err(dai->codec->dev,
			"%s: Invalid sampling rate %d\n", __func__,
			params_rate(params));
		return -EINVAL;
	}

	snd_soc_update_bits(dai->codec,
			MSM8X16_WCD_A_CDC_CLK_RX_I2S_CTL, 0x0F, rx_clk_fs_rate);

	switch (substream->stream) {
	case SNDRV_PCM_STREAM_CAPTURE:
		ret = msm8x16_wcd_set_decimator_rate(dai, tx_fs_rate,
					       params_rate(params));
		if (ret < 0) {
			dev_err(dai->codec->dev,
				"%s: set decimator rate failed %d\n", __func__,
				ret);
			return ret;
		}
		break;
	case SNDRV_PCM_STREAM_PLAYBACK:
		ret = msm8x16_wcd_set_interpolator_rate(dai, rx_fs_rate,
						  params_rate(params));
		if (ret < 0) {
			dev_err(dai->codec->dev,
				"%s: set decimator rate failed %d\n", __func__,
				ret);
			return ret;
		}
		break;
	default:
		dev_err(dai->codec->dev,
			"%s: Invalid stream type %d\n", __func__,
			substream->stream);
		return -EINVAL;
	}
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		snd_soc_update_bits(dai->codec,
				MSM8X16_WCD_A_CDC_CLK_RX_I2S_CTL, 0x20, 0x20);
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_S24_3LE:
		snd_soc_update_bits(dai->codec,
				MSM8X16_WCD_A_CDC_CLK_RX_I2S_CTL, 0x20, 0x00);
		break;
	default:
		dev_err(dai->codec->dev, "%s: wrong format selected\n",
				__func__);
		return -EINVAL;
	}

	return 0;
}

int msm8x16_wcd_digital_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = NULL;
	u16 tx_vol_ctl_reg = 0;
	u8 decimator = 0, i;
	struct msm8x16_wcd_priv *msm8x16_wcd;

	pr_debug("%s: Digital Mute val = %d\n", __func__, mute);

	if (!dai || !dai->codec) {
		pr_err("%s: Invalid params\n", __func__);
		return -EINVAL;
	}
	codec = dai->codec;
	msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	if ((dai->id != AIF1_CAP) && (dai->id != AIF2_VIFEED)) {
		dev_dbg(codec->dev, "%s: Not capture use case skip\n",
		__func__);
		return 0;
	}

	mute = (mute) ? 1 : 0;
	if (!mute) {
		/*
		 * 15 ms is an emperical value for the mute time
		 * that was arrived by checking the pop level
		 * to be inaudible
		 */
		usleep_range(15000, 15010);
	}

	for (i = 0; i < NUM_DECIMATORS; i++) {
		if (msm8x16_wcd->dec_active[i])
			decimator = i + 1;
		if (decimator && decimator <= NUM_DECIMATORS) {
			/* mute/unmute decimators corresponding to Tx DAI's */
			if (dai->id == AIF2_VIFEED && decimator > 2) {
				tx_vol_ctl_reg =
					MSM8X16_WCD_A_CDC_TX1_VOL_CTL_CFG +
					32 * (decimator - 1);
				snd_soc_update_bits(codec, tx_vol_ctl_reg,
						0x01, mute);
			} else if (dai->id == AIF1_CAP && decimator < 3) {
				tx_vol_ctl_reg =
					MSM8X16_WCD_A_CDC_TX1_VOL_CTL_CFG +
					32 * (decimator - 1);
				snd_soc_update_bits(codec, tx_vol_ctl_reg,
						0x01, mute);
			}
		}
		decimator = 0;
	}

	return 0;
}

static struct snd_soc_dai_ops msm8x16_wcd_dai_ops = {
	.startup = msm8x16_wcd_startup,
	.shutdown = msm8x16_wcd_shutdown,
	.hw_params = msm8x16_wcd_hw_params,
	.set_sysclk = msm8x16_wcd_set_dai_sysclk,
	.set_fmt = msm8x16_wcd_set_dai_fmt,
	.set_channel_map = msm8x16_wcd_set_channel_map,
	.get_channel_map = msm8x16_wcd_get_channel_map,
	.digital_mute = msm8x16_wcd_digital_mute,
};

static struct snd_soc_dai_driver msm8x16_wcd_i2s_dai[] = {
	{
		.name = "msm8x16_wcd_i2s_rx1",
		.id = AIF1_PB,
		.playback = {
			.stream_name = "AIF1 Playback",
			.rates = MSM8X16_WCD_RATES,
			.formats = MSM8X16_WCD_FORMATS,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 3,
		},
		.ops = &msm8x16_wcd_dai_ops,
	},
	{
		.name = "msm8x16_wcd_i2s_tx1",
		.id = AIF1_CAP,
		.capture = {
			.stream_name = "AIF1 Capture",
			.rates = MSM8X16_WCD_RATES,
			.formats = MSM8X16_WCD_FORMATS,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 4,
		},
		.ops = &msm8x16_wcd_dai_ops,
	},
	{
		.name = "cajon_vifeedback",
		.id = AIF2_VIFEED,
		.capture = {
			.stream_name = "VIfeed",
			.rates = MSM8X16_WCD_RATES,
			.formats = MSM8X16_WCD_FORMATS,
			.rate_max = 48000,
			.rate_min = 48000,
			.channels_min = 2,
			.channels_max = 2,
		},
		.ops = &msm8x16_wcd_dai_ops,
	},
};

static int msm8x16_wcd_codec_enable_rx_chain(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		dev_dbg(w->codec->dev,
			"%s: PMU:Sleeping 20ms after disabling mute\n",
			__func__);
		break;
	case SND_SOC_DAPM_POST_PMD:
		dev_dbg(w->codec->dev,
			"%s: PMD:Sleeping 20ms after disabling mute\n",
			__func__);
		snd_soc_update_bits(codec, w->reg,
			    1 << w->shift, 0x00);
		msleep(20);
		break;
	}
	return 0;
}

static int msm8x16_wcd_codec_enable_lo_pa(struct snd_soc_dapm_widget *w,
				     struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	dev_dbg(w->codec->dev, "%s: %d %s\n", __func__, event, w->name);
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_CDC_RX3_B6_CTL, 0x01, 0x00);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_CDC_RX3_B6_CTL, 0x01, 0x00);
		break;
	}

	return 0;
}

static int msm8x16_wcd_codec_enable_spk_ext_pa(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s: %s event = %d\n", __func__, w->name, event);
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		dev_dbg(w->codec->dev,
			"%s: enable external speaker PA\n", __func__);
		if (msm8x16_wcd->codec_spk_ext_pa_cb)
			msm8x16_wcd->codec_spk_ext_pa_cb(codec, 1);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		dev_dbg(w->codec->dev,
			"%s: enable external speaker PA\n", __func__);
		if (msm8x16_wcd->codec_spk_ext_pa_cb)
			msm8x16_wcd->codec_spk_ext_pa_cb(codec, 0);
		break;
	}
	return 0;
}

static int msm8x16_wcd_codec_enable_ear_pa(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		dev_dbg(w->codec->dev,
			"%s: Sleeping 20ms after select EAR PA\n",
			__func__);
		snd_soc_update_bits(codec, MSM8X16_WCD_A_ANALOG_RX_EAR_CTL,
			    0x80, 0x80);
		if (get_codec_version(msm8x16_wcd) < CONGA)
			snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_RX_HPH_CNP_WG_TIME, 0xFF, 0x2A);
		if (get_codec_version(msm8x16_wcd) >= DIANGU) {
			snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_RX_COM_BIAS_DAC, 0x08, 0x00);
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_RX_HPH_L_TEST, 0x04, 0x04);
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_RX_HPH_R_TEST, 0x04, 0x04);
		}
		break;
	case SND_SOC_DAPM_POST_PMU:
		dev_dbg(w->codec->dev,
			"%s: Sleeping 20ms after enabling EAR PA\n",
			__func__);
		snd_soc_update_bits(codec, MSM8X16_WCD_A_ANALOG_RX_EAR_CTL,
			    0x40, 0x40);
		usleep_range(7000, 7100);
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_CDC_RX1_B6_CTL, 0x01, 0x00);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec,
			MSM8X16_WCD_A_CDC_RX1_B6_CTL, 0x01, 0x01);
		msleep(20);
		msm8x16_wcd->mute_mask |= EAR_PA_DISABLE;
		if (msm8x16_wcd->boost_option == BOOST_ALWAYS) {
			dev_dbg(w->codec->dev,
				"%s: boost_option:%d, tear down ear\n",
				__func__, msm8x16_wcd->boost_option);
			msm8x16_wcd_boost_mode_sequence(codec, EAR_PMD);
		}
		if (get_codec_version(msm8x16_wcd) >= DIANGU) {
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_RX_HPH_L_TEST, 0x04, 0x0);
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_RX_HPH_R_TEST, 0x04, 0x0);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		dev_dbg(w->codec->dev,
			"%s: Sleeping 7ms after disabling EAR PA\n",
			__func__);
		snd_soc_update_bits(codec, MSM8X16_WCD_A_ANALOG_RX_EAR_CTL,
			    0x40, 0x00);
		usleep_range(7000, 7100);
		if (get_codec_version(msm8x16_wcd) < CONGA)
			snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_RX_HPH_CNP_WG_TIME, 0xFF, 0x16);
		if (get_codec_version(msm8x16_wcd) >= DIANGU)
			snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_RX_COM_BIAS_DAC, 0x08, 0x08);
		break;
	}
	return 0;
}

static const struct snd_soc_dapm_widget msm8x16_wcd_dapm_widgets[] = {
	/*RX stuff */
	SND_SOC_DAPM_OUTPUT("EAR"),
	SND_SOC_DAPM_OUTPUT("WSA_SPK OUT"),

	SND_SOC_DAPM_PGA_E("EAR PA", SND_SOC_NOPM,
			0, 0, NULL, 0, msm8x16_wcd_codec_enable_ear_pa,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX("EAR_S", SND_SOC_NOPM, 0, 0,
		ear_pa_mux),

	SND_SOC_DAPM_MUX("WSA Spk Switch", SND_SOC_NOPM, 0, 0,
		wsa_spk_mux),

	SND_SOC_DAPM_AIF_IN("I2S RX1", "AIF1 Playback", 0, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_AIF_IN("I2S RX2", "AIF1 Playback", 0, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_AIF_IN("I2S RX3", "AIF1 Playback", 0, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_SUPPLY("INT_LDO_H", SND_SOC_NOPM, 1, 0, NULL, 0),

	SND_SOC_DAPM_SPK("Ext Spk", msm8x16_wcd_codec_enable_spk_ext_pa),

	SND_SOC_DAPM_OUTPUT("HEADPHONE"),
	SND_SOC_DAPM_PGA_E("HPHL PA", MSM8X16_WCD_A_ANALOG_RX_HPH_CNP_EN,
		5, 0, NULL, 0,
		msm8x16_wcd_hph_pa_event, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX("HPHL", SND_SOC_NOPM, 0, 0,
		hphl_mux),

	SND_SOC_DAPM_MIXER_E("HPHL DAC",
		MSM8X16_WCD_A_ANALOG_RX_HPH_L_PA_DAC_CTL, 3, 0, NULL,
		0, msm8x16_wcd_hphl_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA_E("HPHR PA", MSM8X16_WCD_A_ANALOG_RX_HPH_CNP_EN,
		4, 0, NULL, 0,
		msm8x16_wcd_hph_pa_event, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX("HPHR", SND_SOC_NOPM, 0, 0,
		hphr_mux),

	SND_SOC_DAPM_MIXER_E("HPHR DAC",
		MSM8X16_WCD_A_ANALOG_RX_HPH_R_PA_DAC_CTL, 3, 0, NULL,
		0, msm8x16_wcd_hphr_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX("SPK", SND_SOC_NOPM, 0, 0,
		spkr_mux),

	SND_SOC_DAPM_DAC("SPK DAC", NULL,
		MSM8X16_WCD_A_ANALOG_SPKR_DAC_CTL, 7, 0),

	SND_SOC_DAPM_MUX("LINE_OUT",
		SND_SOC_NOPM, 0, 0, lo_mux),

	SND_SOC_DAPM_DAC_E("LINEOUT DAC", NULL,
		SND_SOC_NOPM, 0, 0, msm8x16_wcd_lo_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),

	/* Speaker */
	SND_SOC_DAPM_OUTPUT("SPK_OUT"),

	/* Lineout */
	SND_SOC_DAPM_OUTPUT("LINEOUT"),

	SND_SOC_DAPM_PGA_E("SPK PA", MSM8X16_WCD_A_ANALOG_SPKR_DRV_CTL,
			6, 0 , NULL, 0, msm8x16_wcd_codec_enable_spk_pa,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA_E("LINEOUT PA", MSM8X16_WCD_A_ANALOG_RX_LO_EN_CTL,
			5, 0 , NULL, 0, msm8x16_wcd_codec_enable_lo_pa,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY("VDD_SPKDRV", SND_SOC_NOPM, 0, 0,
			    msm89xx_wcd_codec_enable_vdd_spkr,
			    SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX("Ext Spk Switch", SND_SOC_NOPM, 0, 0,
		&ext_spk_mux),

	SND_SOC_DAPM_MIXER("RX1 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX2 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MIXER_E("RX1 MIX2",
		MSM8X16_WCD_A_CDC_CLK_RX_B1_CTL, MSM8X16_WCD_RX1, 0, NULL,
		0, msm8x16_wcd_codec_enable_interpolator,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("RX2 MIX2",
		MSM8X16_WCD_A_CDC_CLK_RX_B1_CTL, MSM8X16_WCD_RX2, 0, NULL,
		0, msm8x16_wcd_codec_enable_interpolator,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("RX3 MIX1",
		MSM8X16_WCD_A_CDC_CLK_RX_B1_CTL, MSM8X16_WCD_RX3, 0, NULL,
		0, msm8x16_wcd_codec_enable_interpolator,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY("RX1 CLK", MSM8X16_WCD_A_DIGITAL_CDC_DIG_CLK_CTL,
		0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("RX2 CLK", MSM8X16_WCD_A_DIGITAL_CDC_DIG_CLK_CTL,
		1, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("RX3 CLK", MSM8X16_WCD_A_DIGITAL_CDC_DIG_CLK_CTL,
		2, 0, msm8x16_wcd_codec_enable_dig_clk, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("RX1 CHAIN", MSM8X16_WCD_A_CDC_RX1_B6_CTL, 0, 0,
		NULL, 0,
		msm8x16_wcd_codec_enable_rx_chain,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("RX2 CHAIN", MSM8X16_WCD_A_CDC_RX2_B6_CTL, 0, 0,
		NULL, 0,
		msm8x16_wcd_codec_enable_rx_chain,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER_E("RX3 CHAIN", MSM8X16_WCD_A_CDC_RX3_B6_CTL, 0, 0,
		NULL, 0,
		msm8x16_wcd_codec_enable_rx_chain,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX("RX1 MIX1 INP1", SND_SOC_NOPM, 0, 0,
		&rx_mix1_inp1_mux),
	SND_SOC_DAPM_MUX("RX1 MIX1 INP2", SND_SOC_NOPM, 0, 0,
		&rx_mix1_inp2_mux),
	SND_SOC_DAPM_MUX("RX1 MIX1 INP3", SND_SOC_NOPM, 0, 0,
		&rx_mix1_inp3_mux),

	SND_SOC_DAPM_MUX("RX2 MIX1 INP1", SND_SOC_NOPM, 0, 0,
		&rx2_mix1_inp1_mux),
	SND_SOC_DAPM_MUX("RX2 MIX1 INP2", SND_SOC_NOPM, 0, 0,
		&rx2_mix1_inp2_mux),
	SND_SOC_DAPM_MUX("RX2 MIX1 INP3", SND_SOC_NOPM, 0, 0,
		&rx2_mix1_inp3_mux),

	SND_SOC_DAPM_MUX("RX3 MIX1 INP1", SND_SOC_NOPM, 0, 0,
		&rx3_mix1_inp1_mux),
	SND_SOC_DAPM_MUX("RX3 MIX1 INP2", SND_SOC_NOPM, 0, 0,
		&rx3_mix1_inp2_mux),
	SND_SOC_DAPM_MUX("RX3 MIX1 INP3", SND_SOC_NOPM, 0, 0,
		&rx3_mix1_inp3_mux),

	SND_SOC_DAPM_MUX("RX1 MIX2 INP1", SND_SOC_NOPM, 0, 0,
		&rx1_mix2_inp1_mux),
	SND_SOC_DAPM_MUX("RX2 MIX2 INP1", SND_SOC_NOPM, 0, 0,
		&rx2_mix2_inp1_mux),

	SND_SOC_DAPM_SUPPLY("MICBIAS_REGULATOR", SND_SOC_NOPM,
		ON_DEMAND_MICBIAS, 0,
		msm8x16_wcd_codec_enable_on_demand_supply,
		SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY("CP", MSM8X16_WCD_A_ANALOG_NCP_EN, 0, 0,
		msm8x16_wcd_codec_enable_charge_pump, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU |	SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY("EAR CP", MSM8X16_WCD_A_ANALOG_NCP_EN, 4, 0,
		msm8x16_wcd_codec_enable_charge_pump, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY_S("RX_BIAS", 1, SND_SOC_NOPM,
		0, 0, msm8x16_wcd_codec_enable_rx_bias,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY_S("SPK_RX_BIAS", 1, SND_SOC_NOPM, 0, 0,
		msm8x16_wcd_codec_enable_rx_bias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	/* TX */

	SND_SOC_DAPM_SUPPLY_S("CDC_CONN", -2, MSM8X16_WCD_A_CDC_CLK_OTHR_CTL,
		2, 0, NULL, 0),


	SND_SOC_DAPM_INPUT("AMIC1"),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS Internal1",
		MSM8X16_WCD_A_ANALOG_MICB_1_EN, 7, 0,
		msm8x16_wcd_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS Internal2",
		MSM8X16_WCD_A_ANALOG_MICB_2_EN, 7, 0,
		msm8x16_wcd_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS Internal3",
		MSM8X16_WCD_A_ANALOG_MICB_1_EN, 7, 0,
		msm8x16_wcd_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("ADC1", NULL, MSM8X16_WCD_A_ANALOG_TX_1_EN, 7, 0,
		msm8x16_wcd_codec_enable_adc, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("ADC2_INP2",
		NULL, MSM8X16_WCD_A_ANALOG_TX_2_EN, 7, 0,
		msm8x16_wcd_codec_enable_adc, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("ADC2_INP3",
		NULL, MSM8X16_WCD_A_ANALOG_TX_3_EN, 7, 0,
		msm8x16_wcd_codec_enable_adc, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MIXER("ADC2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("ADC3", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MUX("ADC2 MUX", SND_SOC_NOPM, 0, 0,
		&tx_adc2_mux),

	SND_SOC_DAPM_MICBIAS_E("MIC BIAS External",
		MSM8X16_WCD_A_ANALOG_MICB_1_EN, 7, 0,
		msm8x16_wcd_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MICBIAS_E("MIC BIAS External2",
		MSM8X16_WCD_A_ANALOG_MICB_2_EN, 7, 0,
		msm8x16_wcd_codec_enable_micbias, SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),


	SND_SOC_DAPM_INPUT("AMIC3"),

	SND_SOC_DAPM_MUX_E("DEC1 MUX",
		MSM8X16_WCD_A_CDC_CLK_TX_CLK_EN_B1_CTL, 0, 0,
		&dec1_mux, msm8x16_wcd_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("DEC2 MUX",
		MSM8X16_WCD_A_CDC_CLK_TX_CLK_EN_B1_CTL, 1, 0,
		&dec2_mux, msm8x16_wcd_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("DEC3 MUX",
		MSM8X16_WCD_A_CDC_CLK_TX_CLK_EN_B1_CTL, 2, 0,
		&dec3_mux, msm8x16_wcd_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("DEC4 MUX",
		MSM8X16_WCD_A_CDC_CLK_TX_CLK_EN_B1_CTL, 3, 0,
		&dec4_mux, msm8x16_wcd_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX("RDAC2 MUX", SND_SOC_NOPM, 0, 0, &rdac2_mux),

	SND_SOC_DAPM_INPUT("AMIC2"),

	SND_SOC_DAPM_AIF_OUT("I2S TX1", "AIF1 Capture", 0, SND_SOC_NOPM,
		0, 0),
	SND_SOC_DAPM_AIF_OUT("I2S TX2", "AIF1 Capture", 0, SND_SOC_NOPM,
		0, 0),
	SND_SOC_DAPM_AIF_OUT("I2S TX3", "AIF1 Capture", 0, SND_SOC_NOPM,
		0, 0),

	SND_SOC_DAPM_AIF_OUT("AIF2 VI", "VIfeed", 0, SND_SOC_NOPM,
		0, 0),
	/* Digital Mic Inputs */
	SND_SOC_DAPM_ADC_E("DMIC1", NULL, SND_SOC_NOPM, 0, 0,
		msm8x16_wcd_codec_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("DMIC2", NULL, SND_SOC_NOPM, 0, 0,
		msm8x16_wcd_codec_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_INPUT("DMIC3"),

	SND_SOC_DAPM_INPUT("DMIC4"),

	/* Sidetone */
	SND_SOC_DAPM_MUX("IIR1 INP1 MUX", SND_SOC_NOPM, 0, 0, &iir1_inp1_mux),
	SND_SOC_DAPM_PGA_E("IIR1", MSM8X16_WCD_A_CDC_CLK_SD_CTL, 0, 0, NULL, 0,
		msm8x16_wcd_codec_set_iir_gain, SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_MUX("IIR2 INP1 MUX", SND_SOC_NOPM, 0, 0, &iir2_inp1_mux),
	SND_SOC_DAPM_PGA_E("IIR2", MSM8X16_WCD_A_CDC_CLK_SD_CTL, 1, 0, NULL, 0,
		msm8x16_wcd_codec_set_iir_gain, SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_SUPPLY("RX_I2S_CLK",
		MSM8X16_WCD_A_CDC_CLK_RX_I2S_CTL,	4, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("TX_I2S_CLK",
		MSM8X16_WCD_A_CDC_CLK_TX_I2S_CTL, 4, 0,
		NULL, 0),
};

static const struct msm8x16_wcd_reg_mask_val msm8x16_wcd_reg_defaults[] = {
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_SPKR_DAC_CTL, 0x03),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_CURRENT_LIMIT, 0x82),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_SPKR_OCP_CTL, 0xE1),
};

static const struct msm8x16_wcd_reg_mask_val msm8x16_wcd_reg_defaults_2_0[] = {
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_DIGITAL_SEC_ACCESS, 0xA5),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_DIGITAL_PERPH_RESET_CTL3, 0x0F),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_TX_1_2_OPAMP_BIAS, 0x4F),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_NCP_FBCTRL, 0x28),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_SPKR_DRV_CTL, 0x69),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_SPKR_DRV_DBG, 0x01),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_BOOST_EN_CTL, 0x5F),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_SLOPE_COMP_IP_ZERO, 0x88),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_SEC_ACCESS, 0xA5),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_PERPH_RESET_CTL3, 0x0F),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_CURRENT_LIMIT, 0x82),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_SPKR_DAC_CTL, 0x03),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_SPKR_OCP_CTL, 0xE1),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_DIGITAL_CDC_RST_CTL, 0x80),
};

static const struct msm8x16_wcd_reg_mask_val msm8909_wcd_reg_defaults[] = {
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_DIGITAL_SEC_ACCESS, 0xA5),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_DIGITAL_PERPH_RESET_CTL3, 0x0F),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_SEC_ACCESS, 0xA5),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_PERPH_RESET_CTL3, 0x0F),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_TX_1_2_OPAMP_BIAS, 0x4C),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_NCP_FBCTRL, 0x28),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_SPKR_DRV_CTL, 0x69),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_SPKR_DRV_DBG, 0x01),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_PERPH_SUBTYPE, 0x0A),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_SPKR_DAC_CTL, 0x03),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_SPKR_OCP_CTL, 0xE1),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_DIGITAL_CDC_RST_CTL, 0x80),
};

static const struct msm8x16_wcd_reg_mask_val cajon_wcd_reg_defaults[] = {
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_DIGITAL_SEC_ACCESS, 0xA5),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_DIGITAL_PERPH_RESET_CTL3, 0x0F),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_SEC_ACCESS, 0xA5),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_PERPH_RESET_CTL3, 0x0F),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_TX_1_2_OPAMP_BIAS, 0x4C),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_CURRENT_LIMIT, 0x82),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_NCP_FBCTRL, 0xA8),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_NCP_VCTRL, 0xA4),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_SPKR_ANA_BIAS_SET, 0x41),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_SPKR_DRV_CTL, 0x69),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_SPKR_DRV_DBG, 0x01),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_SPKR_OCP_CTL, 0xE1),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_SPKR_DAC_CTL, 0x03),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_RX_HPH_BIAS_PA, 0xFA),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_DIGITAL_CDC_RST_CTL, 0x80),
};

static const struct msm8x16_wcd_reg_mask_val cajon2p0_wcd_reg_defaults[] = {
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_DIGITAL_SEC_ACCESS, 0xA5),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_DIGITAL_PERPH_RESET_CTL3, 0x0F),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_SEC_ACCESS, 0xA5),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_PERPH_RESET_CTL3, 0x0F),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_TX_1_2_OPAMP_BIAS, 0x4C),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_CURRENT_LIMIT, 0xA2),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_NCP_FBCTRL, 0xA8),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_NCP_VCTRL, 0xA4),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_SPKR_ANA_BIAS_SET, 0x41),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_SPKR_DRV_CTL, 0x69),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_SPKR_DRV_DBG, 0x01),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_SPKR_OCP_CTL, 0xE1),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_SPKR_DAC_CTL, 0x03),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_RX_EAR_STATUS, 0x10),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_BYPASS_MODE, 0x18),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_ANALOG_RX_HPH_BIAS_PA, 0xFA),
	MSM8X16_WCD_REG_VAL(MSM8X16_WCD_A_DIGITAL_CDC_RST_CTL, 0x80),
};

static void msm8x16_wcd_update_reg_defaults(struct snd_soc_codec *codec)
{
	u32 i, version;
	struct msm8x16_wcd_priv *msm8x16_wcd = snd_soc_codec_get_drvdata(codec);

	version = get_codec_version(msm8x16_wcd);
	if (version == TOMBAK_1_0) {
		for (i = 0; i < ARRAY_SIZE(msm8x16_wcd_reg_defaults); i++)
			snd_soc_write(codec, msm8x16_wcd_reg_defaults[i].reg,
					msm8x16_wcd_reg_defaults[i].val);
	} else if (version == TOMBAK_2_0) {
		for (i = 0; i < ARRAY_SIZE(msm8x16_wcd_reg_defaults_2_0); i++)
			snd_soc_write(codec,
				msm8x16_wcd_reg_defaults_2_0[i].reg,
				msm8x16_wcd_reg_defaults_2_0[i].val);
	} else if (version == CONGA) {
		for (i = 0; i < ARRAY_SIZE(msm8909_wcd_reg_defaults); i++)
			snd_soc_write(codec,
				msm8909_wcd_reg_defaults[i].reg,
				msm8909_wcd_reg_defaults[i].val);
	} else if (version == CAJON) {
		for (i = 0; i < ARRAY_SIZE(cajon_wcd_reg_defaults); i++)
			snd_soc_write(codec,
				cajon_wcd_reg_defaults[i].reg,
				cajon_wcd_reg_defaults[i].val);
	} else if (version == CAJON_2_0 || version == DIANGU) {
		for (i = 0; i < ARRAY_SIZE(cajon2p0_wcd_reg_defaults); i++)
			snd_soc_write(codec,
				cajon2p0_wcd_reg_defaults[i].reg,
				cajon2p0_wcd_reg_defaults[i].val);
	}
}

static const struct msm8x16_wcd_reg_mask_val
	msm8x16_wcd_codec_reg_init_val[] = {

	/* Initialize current threshold to 350MA
	 * number of wait and run cycles to 4096
	 */
	{MSM8X16_WCD_A_ANALOG_RX_COM_OCP_CTL, 0xFF, 0x12},
	{MSM8X16_WCD_A_ANALOG_RX_COM_OCP_COUNT, 0xFF, 0xFF},
};

static void msm8x16_wcd_codec_init_reg(struct snd_soc_codec *codec)
{
	u32 i;

	for (i = 0; i < ARRAY_SIZE(msm8x16_wcd_codec_reg_init_val); i++)
		snd_soc_update_bits(codec,
				    msm8x16_wcd_codec_reg_init_val[i].reg,
				    msm8x16_wcd_codec_reg_init_val[i].mask,
				    msm8x16_wcd_codec_reg_init_val[i].val);
}

static int msm8x16_wcd_bringup(struct snd_soc_codec *codec)
{
	snd_soc_write(codec,
		MSM8X16_WCD_A_DIGITAL_SEC_ACCESS,
		0xA5);
	snd_soc_write(codec, MSM8X16_WCD_A_DIGITAL_PERPH_RESET_CTL4, 0x01);
	snd_soc_write(codec,
		MSM8X16_WCD_A_ANALOG_SEC_ACCESS,
		0xA5);
	snd_soc_write(codec, MSM8X16_WCD_A_ANALOG_PERPH_RESET_CTL4, 0x01);
	snd_soc_write(codec,
		MSM8X16_WCD_A_DIGITAL_SEC_ACCESS,
		0xA5);
	snd_soc_write(codec, MSM8X16_WCD_A_DIGITAL_PERPH_RESET_CTL4, 0x00);
	snd_soc_write(codec,
		MSM8X16_WCD_A_ANALOG_SEC_ACCESS,
		0xA5);
	snd_soc_write(codec, MSM8X16_WCD_A_ANALOG_PERPH_RESET_CTL4, 0x00);
	return 0;
}

static struct regulator *wcd8x16_wcd_codec_find_regulator(
				const struct msm8x16_wcd *msm8x16,
				const char *name)
{
	int i;

	for (i = 0; i < msm8x16->num_of_supplies; i++) {
		if (msm8x16->supplies[i].supply &&
		    !strcmp(msm8x16->supplies[i].supply, name))
			return msm8x16->supplies[i].consumer;
	}

	dev_dbg(msm8x16->dev, "Error: regulator not found:%s\n"
				, name);
	return NULL;
}

static int msm8x16_wcd_device_down(struct snd_soc_codec *codec)
{
	struct msm8916_asoc_mach_data *pdata = NULL;
	struct msm8x16_wcd_priv *msm8x16_wcd_priv =
		snd_soc_codec_get_drvdata(codec);
	int i;
	unsigned int tx_1_en;
	unsigned int tx_2_en;

	pdata = snd_soc_card_get_drvdata(codec->component.card);
	dev_dbg(codec->dev, "%s: device down!\n", __func__);

	tx_1_en = msm8x16_wcd_read(codec, MSM8X16_WCD_A_ANALOG_TX_1_EN);
	tx_2_en = msm8x16_wcd_read(codec, MSM8X16_WCD_A_ANALOG_TX_2_EN);
	tx_1_en = tx_1_en & 0x7f;
	tx_2_en = tx_2_en & 0x7f;
	msm8x16_wcd_write(codec,
		MSM8X16_WCD_A_ANALOG_TX_1_EN, tx_1_en);
	msm8x16_wcd_write(codec,
		MSM8X16_WCD_A_ANALOG_TX_2_EN, tx_2_en);
	if (msm8x16_wcd_priv->boost_option == BOOST_ON_FOREVER) {
		if ((snd_soc_read(codec, MSM8X16_WCD_A_ANALOG_SPKR_DRV_CTL)
			& 0x80) == 0) {
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_CLK_MCLK_CTL,	0x01, 0x01);
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_CDC_CLK_PDM_CTL, 0x03, 0x03);
			snd_soc_write(codec,
				MSM8X16_WCD_A_ANALOG_MASTER_BIAS_CTL, 0x30);
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_DIGITAL_CDC_RST_CTL, 0x80, 0x80);
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_DIGITAL_CDC_TOP_CLK_CTL,
				0x0C, 0x0C);
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_DIGITAL_CDC_DIG_CLK_CTL,
				0x84, 0x84);
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_DIGITAL_CDC_ANA_CLK_CTL,
				0x10, 0x10);
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_SPKR_PWRSTG_CTL,
				0x1F, 0x1F);
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_RX_COM_BIAS_DAC,
				0x90, 0x90);
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_RX_EAR_CTL,
				0xFF, 0xFF);
			usleep_range(20, 21);
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_SPKR_PWRSTG_CTL,
				0xFF, 0xFF);
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_SPKR_DRV_CTL,
				0xE9, 0xE9);
		}
	}
	msm8x16_wcd_boost_off(codec);
	msm8x16_wcd_priv->hph_mode = NORMAL_MODE;
	for (i = 0; i < MSM8X16_WCD_RX_MAX; i++)
		msm8x16_wcd_priv->comp_enabled[i] = COMPANDER_NONE;

	/* 40ms to allow boost to discharge */
	msleep(40);
	/* Disable PA to avoid pop during codec bring up */
	snd_soc_update_bits(codec, MSM8X16_WCD_A_ANALOG_RX_HPH_CNP_EN,
			0x30, 0x00);
	snd_soc_update_bits(codec, MSM8X16_WCD_A_ANALOG_SPKR_DRV_CTL,
			0x80, 0x00);
	msm8x16_wcd_write(codec,
		MSM8X16_WCD_A_ANALOG_RX_HPH_L_PA_DAC_CTL, 0x20);
	msm8x16_wcd_write(codec,
		MSM8X16_WCD_A_ANALOG_RX_HPH_R_PA_DAC_CTL, 0x20);
	msm8x16_wcd_write(codec,
		MSM8X16_WCD_A_ANALOG_RX_EAR_CTL, 0x12);
	msm8x16_wcd_write(codec,
		MSM8X16_WCD_A_ANALOG_SPKR_DAC_CTL, 0x93);

	msm8x16_wcd_bringup(codec);
	atomic_set(&pdata->mclk_enabled, false);
	set_bit(BUS_DOWN, &msm8x16_wcd_priv->status_mask);
	snd_soc_card_change_online_state(codec->component.card, 0);
	return 0;
}

static int msm8x16_wcd_device_up(struct snd_soc_codec *codec)
{
	struct msm8x16_wcd_priv *msm8x16_wcd_priv =
		snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	dev_dbg(codec->dev, "%s: device up!\n", __func__);

	mutex_lock(&codec->mutex);

	clear_bit(BUS_DOWN, &msm8x16_wcd_priv->status_mask);



	snd_soc_card_change_online_state(codec->component.card, 1);
	/* delay is required to make sure sound card state updated */
	usleep_range(5000, 5100);

	msm8x16_wcd_codec_init_reg(codec);
	msm8x16_wcd_update_reg_defaults(codec);

	codec->cache_sync = true;
	snd_soc_cache_sync(codec);
	codec->cache_sync = false;

	msm8x16_wcd_write(codec, MSM8X16_WCD_A_DIGITAL_INT_EN_SET,
				MSM8X16_WCD_A_DIGITAL_INT_EN_SET__POR);
	msm8x16_wcd_write(codec, MSM8X16_WCD_A_DIGITAL_INT_EN_CLR,
				MSM8X16_WCD_A_DIGITAL_INT_EN_CLR__POR);

	msm8x16_wcd_set_boost_v(codec);

	msm8x16_wcd_set_micb_v(codec);
	if (msm8x16_wcd_priv->boost_option == BOOST_ON_FOREVER)
		msm8x16_wcd_boost_on(codec);
	else if (msm8x16_wcd_priv->boost_option == BYPASS_ALWAYS)
		msm8x16_wcd_bypass_on(codec);

	msm8x16_wcd_configure_cap(codec, false, false);
	wcd_mbhc_stop(&msm8x16_wcd_priv->mbhc);
	wcd_mbhc_deinit(&msm8x16_wcd_priv->mbhc);
	ret = wcd_mbhc_init(&msm8x16_wcd_priv->mbhc, codec, &mbhc_cb, &intr_ids,
			wcd_mbhc_registers, true);
	if (ret)
		dev_err(codec->dev, "%s: mbhc initialization failed\n",
			__func__);
	else
		wcd_mbhc_start(&msm8x16_wcd_priv->mbhc,
			msm8x16_wcd_priv->mbhc.mbhc_cfg);

	mutex_unlock(&codec->mutex);

	return 0;
}

static int adsp_state_callback(struct notifier_block *nb, unsigned long value,
			       void *priv)
{
	bool timedout;
	unsigned long timeout;

	if (value == SUBSYS_BEFORE_SHUTDOWN)
		msm8x16_wcd_device_down(registered_codec);
	else if (value == SUBSYS_AFTER_POWERUP) {

		dev_dbg(registered_codec->dev,
			"ADSP is about to power up. bring up codec\n");

		if (!q6core_is_adsp_ready()) {
			dev_dbg(registered_codec->dev,
				"ADSP isn't ready\n");
			timeout = jiffies +
				  msecs_to_jiffies(ADSP_STATE_READY_TIMEOUT_MS);
			while (!(timedout = time_after(jiffies, timeout))) {
				if (!q6core_is_adsp_ready()) {
					dev_dbg(registered_codec->dev,
						"ADSP isn't ready\n");
				} else {
					dev_dbg(registered_codec->dev,
						"ADSP is ready\n");
					break;
				}
			}
		} else {
			dev_dbg(registered_codec->dev,
				"%s: DSP is ready\n", __func__);
		}

		msm8x16_wcd_device_up(registered_codec);
	}
	return NOTIFY_OK;
}

static struct notifier_block adsp_state_notifier_block = {
	.notifier_call = adsp_state_callback,
	.priority = -INT_MAX,
};

int msm8x16_wcd_hs_detect(struct snd_soc_codec *codec,
		    struct wcd_mbhc_config *mbhc_cfg)
{
	struct msm8x16_wcd_priv *msm8x16_wcd_priv =
		snd_soc_codec_get_drvdata(codec);

	return wcd_mbhc_start(&msm8x16_wcd_priv->mbhc, mbhc_cfg);
}
EXPORT_SYMBOL(msm8x16_wcd_hs_detect);

void msm8x16_wcd_hs_detect_exit(struct snd_soc_codec *codec)
{
	struct msm8x16_wcd_priv *msm8x16_wcd_priv =
		snd_soc_codec_get_drvdata(codec);

	wcd_mbhc_stop(&msm8x16_wcd_priv->mbhc);
}
EXPORT_SYMBOL(msm8x16_wcd_hs_detect_exit);

void msm8x16_update_int_spk_boost(bool enable)
{
	pr_debug("%s: enable = %d\n", __func__, enable);
	spkr_boost_en = enable;
}
EXPORT_SYMBOL(msm8x16_update_int_spk_boost);

static void msm8x16_wcd_set_micb_v(struct snd_soc_codec *codec)
{

	struct msm8x16_wcd *msm8x16 = codec->control_data;
	struct msm8x16_wcd_pdata *pdata = msm8x16->dev->platform_data;
	u8 reg_val;

	reg_val = VOLTAGE_CONVERTER(pdata->micbias.cfilt1_mv, MICBIAS_MIN_VAL,
			MICBIAS_STEP_SIZE);
	dev_dbg(codec->dev, "cfilt1_mv %d reg_val %x\n",
			(u32)pdata->micbias.cfilt1_mv, reg_val);
	snd_soc_update_bits(codec, MSM8X16_WCD_A_ANALOG_MICB_1_VAL,
			0xF8, (reg_val << 3));
}

static void msm8x16_wcd_set_boost_v(struct snd_soc_codec *codec)
{
	struct msm8x16_wcd_priv *msm8x16_wcd_priv =
				snd_soc_codec_get_drvdata(codec);

	snd_soc_update_bits(codec, MSM8X16_WCD_A_ANALOG_OUTPUT_VOLTAGE,
			0x1F, msm8x16_wcd_priv->boost_voltage);
}

static void msm8x16_wcd_configure_cap(struct snd_soc_codec *codec,
		bool micbias1, bool micbias2)
{

	struct msm8916_asoc_mach_data *pdata = NULL;

	pdata = snd_soc_card_get_drvdata(codec->component.card);

	pr_debug("\n %s: micbias1 %x micbias2 = %d\n", __func__, micbias1,
			micbias2);
	if (micbias1 && micbias2) {
		if ((pdata->micbias1_cap_mode
		     == MICBIAS_EXT_BYP_CAP) ||
		    (pdata->micbias2_cap_mode
		     == MICBIAS_EXT_BYP_CAP))
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_MICB_1_EN,
				0x40, (MICBIAS_EXT_BYP_CAP << 6));
		else
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_MICB_1_EN,
				0x40, (MICBIAS_NO_EXT_BYP_CAP << 6));
	} else if (micbias2) {
		snd_soc_update_bits(codec, MSM8X16_WCD_A_ANALOG_MICB_1_EN,
				0x40, (pdata->micbias2_cap_mode << 6));
	} else if (micbias1) {
		snd_soc_update_bits(codec, MSM8X16_WCD_A_ANALOG_MICB_1_EN,
				0x40, (pdata->micbias1_cap_mode << 6));
	} else {
		snd_soc_update_bits(codec, MSM8X16_WCD_A_ANALOG_MICB_1_EN,
				0x40, 0x00);
	}
}

static int msm8x16_wcd_codec_probe(struct snd_soc_codec *codec)
{
	struct msm8x16_wcd_priv *msm8x16_wcd_priv;
	struct msm8x16_wcd *msm8x16_wcd;
	struct msm8x16_wcd_pdata *pdata;
	int i, ret;
	const char *subsys_name = NULL;

	dev_dbg(codec->dev, "%s()\n", __func__);

	msm8x16_wcd_priv = kzalloc(sizeof(struct msm8x16_wcd_priv), GFP_KERNEL);
	if (!msm8x16_wcd_priv)
		return -ENOMEM;

	for (i = 0; i < NUM_DECIMATORS; i++) {
		tx_hpf_work[i].msm8x16_wcd = msm8x16_wcd_priv;
		tx_hpf_work[i].decimator = i + 1;
		INIT_DELAYED_WORK(&tx_hpf_work[i].dwork,
			tx_hpf_corner_freq_callback);
	}

	codec->control_data = dev_get_drvdata(codec->dev);
	snd_soc_codec_set_drvdata(codec, msm8x16_wcd_priv);
	msm8x16_wcd_priv->codec = codec;

	/* codec resmgr module init */
	msm8x16_wcd = codec->control_data;
	pdata = msm8x16_wcd->dev->platform_data;
	msm8x16_wcd->dig_base = ioremap(pdata->dig_cdc_addr,
			MSM8X16_DIGITAL_CODEC_REG_SIZE);
	if (msm8x16_wcd->dig_base == NULL) {
		dev_err(codec->dev, "%s ioremap failed\n", __func__);
		kfree(msm8x16_wcd_priv);
		return -ENOMEM;
	}
	msm8x16_wcd_priv->spkdrv_reg =
		wcd8x16_wcd_codec_find_regulator(codec->control_data,
						MSM89XX_VDD_SPKDRV_NAME);
	msm8x16_wcd_priv->pmic_rev = snd_soc_read(codec,
					MSM8X16_WCD_A_DIGITAL_REVISION1);
	msm8x16_wcd_priv->codec_version = snd_soc_read(codec,
					MSM8X16_WCD_A_DIGITAL_PERPH_SUBTYPE);
	if (msm8x16_wcd_priv->codec_version == CONGA) {
		dev_dbg(codec->dev, "%s :Conga REV: %d\n", __func__,
					msm8x16_wcd_priv->codec_version);
		msm8x16_wcd_priv->ext_spk_boost_set = true;
	} else {
		dev_dbg(codec->dev, "%s :PMIC REV: %d\n", __func__,
					msm8x16_wcd_priv->pmic_rev);
		if (msm8x16_wcd_priv->pmic_rev == TOMBAK_1_0 &&
			msm8x16_wcd_priv->codec_version == CAJON_2_0) {
			msm8x16_wcd_priv->codec_version = DIANGU;
			dev_dbg(codec->dev, "%s : Diangu detected\n",
						__func__);
		} else if (msm8x16_wcd_priv->pmic_rev == TOMBAK_1_0 &&
			(snd_soc_read(codec, MSM8X16_WCD_A_ANALOG_NCP_FBCTRL)
			 & 0x80)) {
			msm8x16_wcd_priv->codec_version = CAJON;
			dev_dbg(codec->dev, "%s : Cajon detected\n", __func__);
		} else if (msm8x16_wcd_priv->pmic_rev == TOMBAK_2_0 &&
			(snd_soc_read(codec, MSM8X16_WCD_A_ANALOG_NCP_FBCTRL)
			 & 0x80)) {
			msm8x16_wcd_priv->codec_version = CAJON_2_0;
			dev_dbg(codec->dev, "%s : Cajon 2.0 detected\n",
						__func__);
		}
	}
	/*
	 * set to default boost option BOOST_SWITCH, user mixer path can change
	 * it to BOOST_ALWAYS or BOOST_BYPASS based on solution chosen.
	 */
	msm8x16_wcd_priv->boost_option = BOOST_SWITCH;
	msm8x16_wcd_priv->hph_mode = NORMAL_MODE;

	for (i = 0; i < MSM8X16_WCD_RX_MAX; i++)
		msm8x16_wcd_priv->comp_enabled[i] = COMPANDER_NONE;

	msm8x16_wcd_dt_parse_boost_info(codec);
	msm8x16_wcd_set_boost_v(codec);

	snd_soc_add_codec_controls(codec, impedance_detect_controls,
				   ARRAY_SIZE(impedance_detect_controls));
	snd_soc_add_codec_controls(codec, hph_type_detect_controls,
				  ARRAY_SIZE(hph_type_detect_controls));

	msm8x16_wcd_bringup(codec);
	msm8x16_wcd_codec_init_reg(codec);
	msm8x16_wcd_update_reg_defaults(codec);

	wcd9xxx_spmi_set_codec(codec);

	msm8x16_wcd_priv->on_demand_list[ON_DEMAND_MICBIAS].supply =
				wcd8x16_wcd_codec_find_regulator(
				codec->control_data,
				on_demand_supply_name[ON_DEMAND_MICBIAS]);
	atomic_set(&msm8x16_wcd_priv->on_demand_list[ON_DEMAND_MICBIAS].ref, 0);

	BLOCKING_INIT_NOTIFIER_HEAD(&msm8x16_wcd_priv->notifier);

	msm8x16_wcd_priv->fw_data = kzalloc(sizeof(*(msm8x16_wcd_priv->fw_data))
			, GFP_KERNEL);
	if (!msm8x16_wcd_priv->fw_data) {
		iounmap(msm8x16_wcd->dig_base);
		kfree(msm8x16_wcd_priv);
		return -ENOMEM;
	}

	set_bit(WCD9XXX_MBHC_CAL, msm8x16_wcd_priv->fw_data->cal_bit);
	ret = wcd_cal_create_hwdep(msm8x16_wcd_priv->fw_data,
			WCD9XXX_CODEC_HWDEP_NODE, codec);
	if (ret < 0) {
		dev_err(codec->dev, "%s hwdep failed %d\n", __func__, ret);
		iounmap(msm8x16_wcd->dig_base);
		kfree(msm8x16_wcd_priv->fw_data);
		kfree(msm8x16_wcd_priv);
		return ret;
	}

	wcd_mbhc_init(&msm8x16_wcd_priv->mbhc, codec, &mbhc_cb, &intr_ids,
		      wcd_mbhc_registers, true);

	msm8x16_wcd_priv->mclk_enabled = false;
	msm8x16_wcd_priv->clock_active = false;
	msm8x16_wcd_priv->config_mode_active = false;

	/*Update speaker boost configuration*/
	msm8x16_wcd_priv->spk_boost_set = spkr_boost_en;
	pr_debug("%s: speaker boost configured = %d\n",
			__func__, msm8x16_wcd_priv->spk_boost_set);

	/* Set initial MICBIAS voltage level */
	msm8x16_wcd_set_micb_v(codec);

	/* Set initial cap mode */
	msm8x16_wcd_configure_cap(codec, false, false);
	registered_codec = codec;
	ret = of_property_read_string(codec->dev->of_node,
				"qcom,subsys-name",
				&subsys_name);
	if (ret) {
		dev_dbg(codec->dev, "missing subsys-name entry in dt node\n");
		adsp_state_notifier =
			subsys_notif_register_notifier("adsp",
			&adsp_state_notifier_block);
	} else {
		adsp_state_notifier =
			subsys_notif_register_notifier(subsys_name,
			&adsp_state_notifier_block);
	}
	if (!adsp_state_notifier) {
		dev_err(codec->dev, "Failed to register adsp state notifier\n");
		iounmap(msm8x16_wcd->dig_base);
		kfree(msm8x16_wcd_priv->fw_data);
		kfree(msm8x16_wcd_priv);
		registered_codec = NULL;
		return -ENOMEM;
	}
	return 0;
}

static int msm8x16_wcd_codec_remove(struct snd_soc_codec *codec)
{
	struct msm8x16_wcd_priv *msm8x16_wcd_priv =
					snd_soc_codec_get_drvdata(codec);
	struct msm8x16_wcd *msm8x16_wcd;

	msm8x16_wcd = codec->control_data;
	msm8x16_wcd_priv->spkdrv_reg = NULL;
	msm8x16_wcd_priv->on_demand_list[ON_DEMAND_MICBIAS].supply = NULL;
	atomic_set(&msm8x16_wcd_priv->on_demand_list[ON_DEMAND_MICBIAS].ref, 0);
	iounmap(msm8x16_wcd->dig_base);
	kfree(msm8x16_wcd_priv->fw_data);
	kfree(msm8x16_wcd_priv);

	return 0;
}

static int msm8x16_wcd_enable_static_supplies_to_optimum(
				struct msm8x16_wcd *msm8x16,
				struct msm8x16_wcd_pdata *pdata)
{
	int i;
	int ret = 0;

	for (i = 0; i < msm8x16->num_of_supplies; i++) {
		if (pdata->regulator[i].ondemand)
			continue;
		if (regulator_count_voltages(msm8x16->supplies[i].consumer) <=
			0)
			continue;

		ret = regulator_set_voltage(msm8x16->supplies[i].consumer,
			pdata->regulator[i].min_uv,
			pdata->regulator[i].max_uv);
		if (ret) {
			dev_err(msm8x16->dev,
				"Setting volt failed for regulator %s err %d\n",
				msm8x16->supplies[i].supply, ret);
		}

		ret = regulator_set_optimum_mode(msm8x16->supplies[i].consumer,
			pdata->regulator[i].optimum_ua);
		dev_dbg(msm8x16->dev, "Regulator %s set optimum mode\n",
			 msm8x16->supplies[i].supply);
	}

	return ret;
}

static int msm8x16_wcd_disable_static_supplies_to_optimum(
			struct msm8x16_wcd *msm8x16,
			struct msm8x16_wcd_pdata *pdata)
{
	int i;
	int ret = 0;

	for (i = 0; i < msm8x16->num_of_supplies; i++) {
		if (pdata->regulator[i].ondemand)
			continue;
		if (regulator_count_voltages(msm8x16->supplies[i].consumer) <=
			0)
			continue;
		regulator_set_voltage(msm8x16->supplies[i].consumer, 0,
			pdata->regulator[i].max_uv);
		regulator_set_optimum_mode(msm8x16->supplies[i].consumer, 0);
		dev_dbg(msm8x16->dev, "Regulator %s set optimum mode\n",
				 msm8x16->supplies[i].supply);
	}

	return ret;
}

int msm8x16_wcd_suspend(struct snd_soc_codec *codec)
{
	struct msm8916_asoc_mach_data *pdata = NULL;
	struct msm8x16_wcd *msm8x16 = codec->control_data;
	struct msm8x16_wcd_pdata *msm8x16_pdata = msm8x16->dev->platform_data;

	pdata = snd_soc_card_get_drvdata(codec->component.card);
	pr_debug("%s: mclk cnt = %d, mclk_enabled = %d\n",
			__func__, atomic_read(&pdata->mclk_rsc_ref),
			atomic_read(&pdata->mclk_enabled));
	if (atomic_read(&pdata->mclk_enabled) == true) {
		cancel_delayed_work_sync(
				&pdata->disable_mclk_work);
		mutex_lock(&pdata->cdc_mclk_mutex);
		if (atomic_read(&pdata->mclk_enabled) == true) {
			if (pdata->afe_clk_ver == AFE_CLK_VERSION_V1) {
				pdata->digital_cdc_clk.clk_val = 0;
				afe_set_digital_codec_core_clock(
						AFE_PORT_ID_PRIMARY_MI2S_RX,
						&pdata->digital_cdc_clk);
			} else {
				pdata->digital_cdc_core_clk.enable = 0;
				afe_set_lpass_clock_v2(
						AFE_PORT_ID_PRIMARY_MI2S_RX,
						&pdata->digital_cdc_core_clk);
			}
			atomic_set(&pdata->mclk_enabled, false);
		}
		mutex_unlock(&pdata->cdc_mclk_mutex);
	}
	msm8x16_wcd_disable_static_supplies_to_optimum(msm8x16, msm8x16_pdata);
	return 0;
}

int msm8x16_wcd_resume(struct snd_soc_codec *codec)
{
	struct msm8916_asoc_mach_data *pdata = NULL;
	struct msm8x16_wcd *msm8x16 = codec->control_data;
	struct msm8x16_wcd_pdata *msm8x16_pdata = msm8x16->dev->platform_data;

	pdata = snd_soc_card_get_drvdata(codec->component.card);
	msm8x16_wcd_enable_static_supplies_to_optimum(msm8x16, msm8x16_pdata);
	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_msm8x16_wcd = {
	.probe	= msm8x16_wcd_codec_probe,
	.remove	= msm8x16_wcd_codec_remove,

	.read = msm8x16_wcd_read,
	.write = msm8x16_wcd_write,

	.suspend = msm8x16_wcd_suspend,
	.resume = msm8x16_wcd_resume,

	.readable_register = msm8x16_wcd_readable,
	.volatile_register = msm8x16_wcd_volatile,

	.reg_cache_size = MSM8X16_WCD_CACHE_SIZE,
	.reg_cache_default = msm8x16_wcd_reset_reg_defaults,
	.reg_word_size = 1,

	.controls = msm8x16_wcd_snd_controls,
	.num_controls = ARRAY_SIZE(msm8x16_wcd_snd_controls),
	.dapm_widgets = msm8x16_wcd_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(msm8x16_wcd_dapm_widgets),
	.dapm_routes = audio_map,
	.num_dapm_routes = ARRAY_SIZE(audio_map),
};

static int msm8x16_wcd_init_supplies(struct msm8x16_wcd *msm8x16,
				struct msm8x16_wcd_pdata *pdata)
{
	int ret;
	int i;

	msm8x16->supplies = kzalloc(sizeof(struct regulator_bulk_data) *
				   ARRAY_SIZE(pdata->regulator),
				   GFP_KERNEL);
	if (!msm8x16->supplies) {
		ret = -ENOMEM;
		goto err;
	}

	msm8x16->num_of_supplies = 0;

	if (ARRAY_SIZE(pdata->regulator) > MAX_REGULATOR) {
		dev_err(msm8x16->dev, "%s: Array Size out of bound\n",
			__func__);
		ret = -EINVAL;
		goto err;
	}

	for (i = 0; i < ARRAY_SIZE(pdata->regulator); i++) {
		if (pdata->regulator[i].name) {
			msm8x16->supplies[i].supply = pdata->regulator[i].name;
			msm8x16->num_of_supplies++;
		}
	}

	ret = regulator_bulk_get(msm8x16->dev, msm8x16->num_of_supplies,
				 msm8x16->supplies);
	if (ret != 0) {
		dev_err(msm8x16->dev, "Failed to get supplies: err = %d\n",
							ret);
		goto err_supplies;
	}

	for (i = 0; i < msm8x16->num_of_supplies; i++) {
		if (regulator_count_voltages(msm8x16->supplies[i].consumer) <=
			0)
			continue;

		ret = regulator_set_voltage(msm8x16->supplies[i].consumer,
			pdata->regulator[i].min_uv,
			pdata->regulator[i].max_uv);
		if (ret) {
			dev_err(msm8x16->dev, "Setting regulator voltage failed for regulator %s err = %d\n",
				msm8x16->supplies[i].supply, ret);
			goto err_get;
		}

		ret = regulator_set_optimum_mode(msm8x16->supplies[i].consumer,
			pdata->regulator[i].optimum_ua);
		if (ret < 0) {
			dev_err(msm8x16->dev, "Setting regulator optimum mode failed for regulator %s err = %d\n",
				msm8x16->supplies[i].supply, ret);
			goto err_get;
		} else {
			ret = 0;
		}
	}

	return ret;

err_get:
	regulator_bulk_free(msm8x16->num_of_supplies, msm8x16->supplies);
err_supplies:
	kfree(msm8x16->supplies);
err:
	return ret;
}

static int msm8x16_wcd_enable_static_supplies(struct msm8x16_wcd *msm8x16,
					  struct msm8x16_wcd_pdata *pdata)
{
	int i;
	int ret = 0;

	for (i = 0; i < msm8x16->num_of_supplies; i++) {
		if (pdata->regulator[i].ondemand)
			continue;
		ret = regulator_enable(msm8x16->supplies[i].consumer);
		if (ret) {
			dev_err(msm8x16->dev, "Failed to enable %s\n",
			       msm8x16->supplies[i].supply);
			break;
		}
		dev_dbg(msm8x16->dev, "Enabled regulator %s\n",
				 msm8x16->supplies[i].supply);
	}

	while (ret && --i)
		if (!pdata->regulator[i].ondemand)
			regulator_disable(msm8x16->supplies[i].consumer);

	return ret;
}



static void msm8x16_wcd_disable_supplies(struct msm8x16_wcd *msm8x16,
				     struct msm8x16_wcd_pdata *pdata)
{
	int i;

	regulator_bulk_disable(msm8x16->num_of_supplies,
				    msm8x16->supplies);
	for (i = 0; i < msm8x16->num_of_supplies; i++) {
		if (regulator_count_voltages(msm8x16->supplies[i].consumer) <=
			0)
			continue;
		regulator_set_voltage(msm8x16->supplies[i].consumer, 0,
			pdata->regulator[i].max_uv);
		regulator_set_optimum_mode(msm8x16->supplies[i].consumer, 0);
	}
	regulator_bulk_free(msm8x16->num_of_supplies, msm8x16->supplies);
	kfree(msm8x16->supplies);
}

static int msm8x16_wcd_device_init(struct msm8x16_wcd *msm8x16)
{
	mutex_init(&msm8x16->io_lock);
	return 0;
}

static int msm8x16_wcd_spmi_probe(struct spmi_device *spmi)
{
	int ret = 0;
	struct msm8x16_wcd *msm8x16 = NULL;
	struct msm8x16_wcd_pdata *pdata;
	struct resource *wcd_resource;
	int adsp_state;
	static int spmi_dev_registered_cnt;

	dev_dbg(&spmi->dev, "%s(%d):slave ID = 0x%x\n",
		__func__, __LINE__,  spmi->sid);

	adsp_state = apr_get_subsys_state();
	if (adsp_state != APR_SUBSYS_LOADED) {
		dev_dbg(&spmi->dev, "Adsp is not loaded yet %d\n",
				adsp_state);
		return -EPROBE_DEFER;
	}

	wcd_resource = spmi_get_resource(spmi, NULL, IORESOURCE_MEM, 0);
	if (!wcd_resource) {
		dev_err(&spmi->dev, "Unable to get Tombak base address\n");
		return -ENXIO;
	}

	switch (wcd_resource->start) {
	case TOMBAK_CORE_0_SPMI_ADDR:
		msm8x16_wcd_modules[0].spmi = spmi;
		msm8x16_wcd_modules[0].base = (spmi->sid << 16) +
						wcd_resource->start;
		wcd9xxx_spmi_set_dev(msm8x16_wcd_modules[0].spmi, 0);
		device_init_wakeup(&spmi->dev, true);
		break;
	case TOMBAK_CORE_1_SPMI_ADDR:
		msm8x16_wcd_modules[1].spmi = spmi;
		msm8x16_wcd_modules[1].base = (spmi->sid << 16) +
						wcd_resource->start;
		wcd9xxx_spmi_set_dev(msm8x16_wcd_modules[1].spmi, 1);
	if (wcd9xxx_spmi_irq_init()) {
		dev_err(&spmi->dev,
				"%s: irq initialization failed\n", __func__);
	} else {
		dev_dbg(&spmi->dev,
				"%s: irq initialization passed\n", __func__);
	}
		spmi_dev_registered_cnt++;
		goto register_codec;
	default:
		ret = -EINVAL;
		goto rtn;
	}


	dev_dbg(&spmi->dev, "%s(%d):start addr = 0x%pK\n",
		__func__, __LINE__,  &wcd_resource->start);

	if (wcd_resource->start != TOMBAK_CORE_0_SPMI_ADDR)
		goto rtn;

	if (spmi->dev.of_node) {
		dev_dbg(&spmi->dev, "%s:Platform data from device tree\n",
			__func__);
		pdata = msm8x16_wcd_populate_dt_pdata(&spmi->dev);
		spmi->dev.platform_data = pdata;
	} else {
		dev_dbg(&spmi->dev, "%s:Platform data from board file\n",
			__func__);
		pdata = spmi->dev.platform_data;
	}
	if (pdata == NULL) {
		dev_err(&spmi->dev, "%s:Platform data failed to populate\n",
			__func__);
		goto rtn;
	}

	msm8x16 = kzalloc(sizeof(struct msm8x16_wcd), GFP_KERNEL);
	if (msm8x16 == NULL) {
		ret = -ENOMEM;
		goto rtn;
	}

	msm8x16->dev = &spmi->dev;
	msm8x16->read_dev = __msm8x16_wcd_reg_read;
	msm8x16->write_dev = __msm8x16_wcd_reg_write;
	ret = msm8x16_wcd_init_supplies(msm8x16, pdata);
	if (ret) {
		dev_err(&spmi->dev, "%s: Fail to enable Codec supplies\n",
			__func__);
		goto err_codec;
	}

	ret = msm8x16_wcd_enable_static_supplies(msm8x16, pdata);
	if (ret) {
		dev_err(&spmi->dev,
			"%s: Fail to enable Codec pre-reset supplies\n",
			   __func__);
		goto err_codec;
	}
	usleep_range(5, 6);

	ret = msm8x16_wcd_device_init(msm8x16);
	if (ret) {
		dev_err(&spmi->dev,
			"%s:msm8x16_wcd_device_init failed with error %d\n",
			__func__, ret);
		goto err_supplies;
	}
	dev_set_drvdata(&spmi->dev, msm8x16);
	spmi_dev_registered_cnt++;
register_codec:
	if ((spmi_dev_registered_cnt == MAX_MSM8X16_WCD_DEVICE) && (!ret)) {
		if (msm8x16_wcd_modules[0].spmi) {
			ret = snd_soc_register_codec(
					&msm8x16_wcd_modules[0].spmi->dev,
					&soc_codec_dev_msm8x16_wcd,
					msm8x16_wcd_i2s_dai,
					ARRAY_SIZE(msm8x16_wcd_i2s_dai));
			if (ret) {
				dev_err(&spmi->dev,
				"%s:snd_soc_register_codec failed with error %d\n",
				__func__, ret);
				goto err_supplies;
			}
		}
	}
	return ret;
err_supplies:
	msm8x16_wcd_disable_supplies(msm8x16, pdata);
err_codec:
	kfree(msm8x16);
rtn:
	return ret;
}

static void msm8x16_wcd_device_exit(struct msm8x16_wcd *msm8x16)
{
	mutex_destroy(&msm8x16->io_lock);
	kfree(msm8x16);
}

static int msm8x16_wcd_spmi_remove(struct spmi_device *spmi)
{
	struct msm8x16_wcd *msm8x16 = dev_get_drvdata(&spmi->dev);

	msm8x16_wcd_device_exit(msm8x16);
	return 0;
}

#ifdef CONFIG_PM
static int msm8x16_wcd_spmi_resume(struct spmi_device *spmi)
{
	struct resource *wcd_resource;

	wcd_resource = spmi_get_resource(spmi, NULL, IORESOURCE_MEM, 0);
	if (!wcd_resource) {
		dev_err(&spmi->dev, "Unable to get CDC SPMI resource\n");
		return -ENXIO;
	}

	if (wcd_resource->start == TOMBAK_CORE_0_SPMI_ADDR)
		return wcd9xxx_spmi_resume();
	return 0;
}

static int msm8x16_wcd_spmi_suspend(struct spmi_device *spmi,
				    pm_message_t pmesg)
{
	struct resource *wcd_resource;

	wcd_resource = spmi_get_resource(spmi, NULL, IORESOURCE_MEM, 0);
	if (!wcd_resource) {
		dev_err(&spmi->dev, "Unable to get CDC SPMI resource\n");
		return -ENXIO;
	}

	if (wcd_resource->start == TOMBAK_CORE_0_SPMI_ADDR)
		return wcd9xxx_spmi_suspend(pmesg);
	return 0;
}
#endif

static struct spmi_device_id msm8x16_wcd_spmi_id_table[] = {
	{"wcd-spmi", MSM8X16_WCD_SPMI_DIGITAL},
	{"wcd-spmi", MSM8X16_WCD_SPMI_ANALOG},
	{}
};

static struct of_device_id msm8x16_wcd_spmi_of_match[] = {
	{ .compatible = "qcom,msm8x16_wcd_codec",},
	{ },
};

static struct spmi_driver wcd_spmi_driver = {
	.driver                 = {
		.owner          = THIS_MODULE,
		.name           = "wcd-spmi-core",
		.of_match_table = msm8x16_wcd_spmi_of_match
	},
#ifdef CONFIG_PM
		.suspend = msm8x16_wcd_spmi_suspend,
		.resume = msm8x16_wcd_spmi_resume,
#endif
	.id_table               = msm8x16_wcd_spmi_id_table,
	.probe                  = msm8x16_wcd_spmi_probe,
	.remove                 = msm8x16_wcd_spmi_remove,
};

static int __init msm8x16_wcd_codec_init(void)
{
	spmi_driver_register(&wcd_spmi_driver);
	return 0;
}
late_initcall(msm8x16_wcd_codec_init);

static void __exit msm8x16_wcd_codec_exit(void)
{
	spmi_driver_unregister(&wcd_spmi_driver);
}
module_exit(msm8x16_wcd_codec_exit);

MODULE_DESCRIPTION("MSM8x16 Audio codec driver");
MODULE_LICENSE("GPL v2");
MODULE_DEVICE_TABLE(of, msm8x16_wcd_spmi_id_table);

