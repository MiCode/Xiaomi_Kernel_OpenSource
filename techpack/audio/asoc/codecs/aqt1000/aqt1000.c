// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/printk.h>
#include <linux/ratelimit.h>
#include <linux/debugfs.h>
#include <linux/wait.h>
#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/pm_runtime.h>
#include <linux/gpio.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include <sound/info.h>
#include <asoc/wcdcal-hwdep.h>
#include "aqt1000-registers.h"
#include "aqt1000.h"
#include "aqt1000-api.h"
#include "aqt1000-mbhc.h"
#include "aqt1000-routing.h"
#include "aqt1000-internal.h"

#define DRV_NAME "aqt_codec"

#define AQT1000_TX_UNMUTE_DELAY_MS 40
#define  TX_HPF_CUT_OFF_FREQ_MASK 0x60
#define  CF_MIN_3DB_4HZ     0x0
#define  CF_MIN_3DB_75HZ    0x1
#define  CF_MIN_3DB_150HZ   0x2

#define AQT_VERSION_ENTRY_SIZE 17
#define AQT_VOUT_CTL_TO_MICB(x) (1000 + x *50)

static struct interp_sample_rate sr_val_tbl[] = {
	{8000, 0x0}, {16000, 0x1}, {32000, 0x3}, {48000, 0x4}, {96000, 0x5},
	{192000, 0x6}, {384000, 0x7}, {44100, 0x9}, {88200, 0xA},
	{176400, 0xB}, {352800, 0xC},
};

static int tx_unmute_delay = AQT1000_TX_UNMUTE_DELAY_MS;
module_param(tx_unmute_delay, int, 0664);
MODULE_PARM_DESC(tx_unmute_delay, "delay to unmute the tx path");

static void aqt_codec_set_tx_hold(struct snd_soc_component *, u16, bool);

/* Cutoff frequency for high pass filter */
static const char * const cf_text[] = {
	"CF_NEG_3DB_4HZ", "CF_NEG_3DB_75HZ", "CF_NEG_3DB_150HZ"
};

static const char * const rx_cf_text[] = {
	"CF_NEG_3DB_4HZ", "CF_NEG_3DB_75HZ", "CF_NEG_3DB_150HZ",
	"CF_NEG_3DB_0P48HZ"
};

struct aqt1000_anc_header {
	u32 reserved[3];
	u32 num_anc_slots;
};

static SOC_ENUM_SINGLE_DECL(cf_dec0_enum, AQT1000_CDC_TX0_TX_PATH_CFG0, 5,
							cf_text);
static SOC_ENUM_SINGLE_DECL(cf_dec1_enum, AQT1000_CDC_TX1_TX_PATH_CFG0, 5,
							cf_text);
static SOC_ENUM_SINGLE_DECL(cf_dec2_enum, AQT1000_CDC_TX2_TX_PATH_CFG0, 5,
							cf_text);
static SOC_ENUM_SINGLE_DECL(cf_int1_1_enum, AQT1000_CDC_RX1_RX_PATH_CFG2, 0,
							rx_cf_text);
static SOC_ENUM_SINGLE_DECL(cf_int1_2_enum, AQT1000_CDC_RX1_RX_PATH_MIX_CFG, 2,
							rx_cf_text);
static SOC_ENUM_SINGLE_DECL(cf_int2_1_enum, AQT1000_CDC_RX2_RX_PATH_CFG2, 0,
							rx_cf_text);
static SOC_ENUM_SINGLE_DECL(cf_int2_2_enum, AQT1000_CDC_RX2_RX_PATH_MIX_CFG, 2,
							rx_cf_text);

static const DECLARE_TLV_DB_SCALE(hph_gain, -3000, 150, 0);
static const DECLARE_TLV_DB_SCALE(analog_gain, 0, 150, 0);
static const DECLARE_TLV_DB_SCALE(digital_gain, 0, 1, 0);

static int aqt_get_anc_slot(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct aqt1000 *aqt = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = aqt->anc_slot;
	return 0;
}

static int aqt_put_anc_slot(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct aqt1000 *aqt = snd_soc_component_get_drvdata(component);

	aqt->anc_slot = ucontrol->value.integer.value[0];
	return 0;
}

static int aqt_get_anc_func(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct aqt1000 *aqt = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = (aqt->anc_func == true ? 1 : 0);
	return 0;
}

static int aqt_put_anc_func(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct aqt1000 *aqt = snd_soc_component_get_drvdata(component);
	struct snd_soc_dapm_context *dapm =
			snd_soc_component_get_dapm(component);

	mutex_lock(&aqt->codec_mutex);
	aqt->anc_func = (!ucontrol->value.integer.value[0] ? false : true);
	dev_dbg(component->dev, "%s: anc_func %x", __func__, aqt->anc_func);

	if (aqt->anc_func == true) {
		snd_soc_dapm_enable_pin(dapm, "ANC HPHL PA");
		snd_soc_dapm_enable_pin(dapm, "ANC HPHR PA");
		snd_soc_dapm_enable_pin(dapm, "ANC HPHL");
		snd_soc_dapm_enable_pin(dapm, "ANC HPHR");
		snd_soc_dapm_disable_pin(dapm, "HPHL PA");
		snd_soc_dapm_disable_pin(dapm, "HPHR PA");
		snd_soc_dapm_disable_pin(dapm, "HPHL");
		snd_soc_dapm_disable_pin(dapm, "HPHR");
	} else {
		snd_soc_dapm_disable_pin(dapm, "ANC HPHL PA");
		snd_soc_dapm_disable_pin(dapm, "ANC HPHR PA");
		snd_soc_dapm_disable_pin(dapm, "ANC HPHL");
		snd_soc_dapm_disable_pin(dapm, "ANC HPHR");
		snd_soc_dapm_enable_pin(dapm, "HPHL");
		snd_soc_dapm_enable_pin(dapm, "HPHR");
		snd_soc_dapm_enable_pin(dapm, "HPHL PA");
		snd_soc_dapm_enable_pin(dapm, "HPHR PA");
	}
	mutex_unlock(&aqt->codec_mutex);

	snd_soc_dapm_sync(dapm);
	return 0;
}

static const char *const aqt_anc_func_text[] = {"OFF", "ON"};
static const struct soc_enum aqt_anc_func_enum =
	SOC_ENUM_SINGLE_EXT(2, aqt_anc_func_text);

static int aqt_rx_hph_mode_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct aqt1000 *aqt = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = aqt->hph_mode;
	return 0;
}

static int aqt_rx_hph_mode_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct aqt1000 *aqt = snd_soc_component_get_drvdata(component);
	u32 mode_val;

	mode_val = ucontrol->value.enumerated.item[0];

	dev_dbg(component->dev, "%s: mode: %d\n", __func__, mode_val);

	if (mode_val == 0) {
		dev_warn(component->dev, "%s:Invalid HPH Mode, default to Cls-H LOHiFi\n",
			__func__);
		mode_val = CLS_H_LOHIFI;
	}
	aqt->hph_mode = mode_val;
	return 0;
}

static const char * const rx_hph_mode_mux_text[] = {
	"CLS_H_INVALID", "CLS_H_HIFI", "CLS_H_LP", "CLS_AB", "CLS_H_LOHIFI",
	"CLS_H_ULP", "CLS_AB_HIFI",
};

static const struct soc_enum rx_hph_mode_mux_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(rx_hph_mode_mux_text),
			    rx_hph_mode_mux_text);

static int aqt_iir_enable_audio_mixer_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	int band_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;

	ucontrol->value.integer.value[0] = (snd_soc_component_read32(component,
					    AQT1000_CDC_SIDETONE_IIR0_IIR_CTL) &
					    (1 << band_idx)) != 0;

	dev_dbg(component->dev, "%s: IIR0 band #%d enable %d\n", __func__,
		band_idx, (uint32_t)ucontrol->value.integer.value[0]);

	return 0;
}

static int aqt_iir_enable_audio_mixer_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	int band_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;
	bool iir_band_en_status;
	int value = ucontrol->value.integer.value[0];

	/* Mask first 5 bits, 6-8 are reserved */
	snd_soc_component_update_bits(component,
			AQT1000_CDC_SIDETONE_IIR0_IIR_CTL,
			(1 << band_idx), (value << band_idx));

	iir_band_en_status = ((snd_soc_component_read32(component,
				AQT1000_CDC_SIDETONE_IIR0_IIR_CTL) &
			      (1 << band_idx)) != 0);
	dev_dbg(component->dev, "%s: IIR0 band #%d enable %d\n", __func__,
		band_idx, iir_band_en_status);

	return 0;
}

static uint32_t aqt_get_iir_band_coeff(struct snd_soc_component *component,
					int band_idx, int coeff_idx)
{
	uint32_t value = 0;

	/* Address does not automatically update if reading */
	snd_soc_component_write(component,
		AQT1000_CDC_SIDETONE_IIR0_IIR_COEF_B1_CTL,
		((band_idx * BAND_MAX + coeff_idx)
		* sizeof(uint32_t)) & 0x7F);

	value |= snd_soc_component_read32(component,
			AQT1000_CDC_SIDETONE_IIR0_IIR_COEF_B2_CTL);

	snd_soc_component_write(component,
		AQT1000_CDC_SIDETONE_IIR0_IIR_COEF_B1_CTL,
		((band_idx * BAND_MAX + coeff_idx)
		* sizeof(uint32_t) + 1) & 0x7F);

	value |= (snd_soc_component_read32(component,
			AQT1000_CDC_SIDETONE_IIR0_IIR_COEF_B2_CTL) << 8);

	snd_soc_component_write(component,
		AQT1000_CDC_SIDETONE_IIR0_IIR_COEF_B1_CTL,
		((band_idx * BAND_MAX + coeff_idx)
		* sizeof(uint32_t) + 2) & 0x7F);

	value |= (snd_soc_component_read32(component,
			AQT1000_CDC_SIDETONE_IIR0_IIR_COEF_B2_CTL) << 16);

	snd_soc_component_write(component,
		AQT1000_CDC_SIDETONE_IIR0_IIR_COEF_B1_CTL,
		((band_idx * BAND_MAX + coeff_idx)
		* sizeof(uint32_t) + 3) & 0x7F);

	/* Mask bits top 2 bits since they are reserved */
	value |= ((snd_soc_component_read32(component,
				AQT1000_CDC_SIDETONE_IIR0_IIR_COEF_B2_CTL)
				& 0x3F) << 24);

	return value;
}

static int aqt_iir_band_audio_mixer_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	int band_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;

	ucontrol->value.integer.value[0] =
		aqt_get_iir_band_coeff(component, band_idx, 0);
	ucontrol->value.integer.value[1] =
		aqt_get_iir_band_coeff(component, band_idx, 1);
	ucontrol->value.integer.value[2] =
		aqt_get_iir_band_coeff(component, band_idx, 2);
	ucontrol->value.integer.value[3] =
		aqt_get_iir_band_coeff(component, band_idx, 3);
	ucontrol->value.integer.value[4] =
		aqt_get_iir_band_coeff(component, band_idx, 4);

	dev_dbg(component->dev, "%s: IIR band #%d b0 = 0x%x\n"
		"%s: IIR band #%d b1 = 0x%x\n"
		"%s: IIR band #%d b2 = 0x%x\n"
		"%s: IIR band #%d a1 = 0x%x\n"
		"%s: IIR band #%d a2 = 0x%x\n",
		__func__, band_idx,
		(uint32_t)ucontrol->value.integer.value[0],
		__func__, band_idx,
		(uint32_t)ucontrol->value.integer.value[1],
		__func__, band_idx,
		(uint32_t)ucontrol->value.integer.value[2],
		__func__, band_idx,
		(uint32_t)ucontrol->value.integer.value[3],
		__func__, band_idx,
		(uint32_t)ucontrol->value.integer.value[4]);

	return 0;
}

static void aqt_set_iir_band_coeff(struct snd_soc_component *component,
				   int band_idx, uint32_t value)
{
	snd_soc_component_write(component,
		(AQT1000_CDC_SIDETONE_IIR0_IIR_COEF_B2_CTL),
		(value & 0xFF));

	snd_soc_component_write(component,
		(AQT1000_CDC_SIDETONE_IIR0_IIR_COEF_B2_CTL),
		(value >> 8) & 0xFF);

	snd_soc_component_write(component,
		(AQT1000_CDC_SIDETONE_IIR0_IIR_COEF_B2_CTL),
		(value >> 16) & 0xFF);

	/* Mask top 2 bits, 7-8 are reserved */
	snd_soc_component_write(component,
		(AQT1000_CDC_SIDETONE_IIR0_IIR_COEF_B2_CTL),
		(value >> 24) & 0x3F);
}

static int aqt_iir_band_audio_mixer_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	int band_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;
	int coeff_idx;

	/*
	 * Mask top bit it is reserved
	 * Updates addr automatically for each B2 write
	 */
	snd_soc_component_write(component,
		(AQT1000_CDC_SIDETONE_IIR0_IIR_COEF_B1_CTL),
		(band_idx * BAND_MAX * sizeof(uint32_t)) & 0x7F);

	for (coeff_idx = 0; coeff_idx < AQT1000_CDC_SIDETONE_IIR_COEFF_MAX;
		coeff_idx++) {
		aqt_set_iir_band_coeff(component, band_idx,
				   ucontrol->value.integer.value[coeff_idx]);
	}

	dev_dbg(component->dev, "%s: IIR band #%d b0 = 0x%x\n"
		"%s: IIR band #%d b1 = 0x%x\n"
		"%s: IIR band #%d b2 = 0x%x\n"
		"%s: IIR band #%d a1 = 0x%x\n"
		"%s: IIR band #%d a2 = 0x%x\n",
		__func__, band_idx,
		aqt_get_iir_band_coeff(component, band_idx, 0),
		__func__, band_idx,
		aqt_get_iir_band_coeff(component, band_idx, 1),
		__func__, band_idx,
		aqt_get_iir_band_coeff(component, band_idx, 2),
		__func__, band_idx,
		aqt_get_iir_band_coeff(component, band_idx, 3),
		__func__, band_idx,
		aqt_get_iir_band_coeff(component, band_idx, 4));

	return 0;
}

static int aqt_compander_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{

	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	int comp = ((struct soc_multi_mixer_control *)
		    kcontrol->private_value)->shift;
	struct aqt1000 *aqt = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = aqt->comp_enabled[comp];
	return 0;
}

static int aqt_compander_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct aqt1000 *aqt = snd_soc_component_get_drvdata(component);
	int comp = ((struct soc_multi_mixer_control *)
		    kcontrol->private_value)->shift;
	int value = ucontrol->value.integer.value[0];

	dev_dbg(component->dev, "%s: Compander %d enable current %d, new %d\n",
		 __func__, comp + 1, aqt->comp_enabled[comp], value);
	aqt->comp_enabled[comp] = value;

	/* Any specific register configuration for compander */
	switch (comp) {
	case COMPANDER_1:
		/* Set Gain Source Select based on compander enable/disable */
		snd_soc_component_update_bits(component,
				AQT1000_HPH_L_EN, 0x20,
				(value ? 0x00 : 0x20));
		break;
	case COMPANDER_2:
		snd_soc_component_update_bits(component,
				AQT1000_HPH_R_EN, 0x20,
				(value ? 0x00 : 0x20));
		break;
	default:
		/*
		 * if compander is not enabled for any interpolator,
		 * it does not cause any audio failure, so do not
		 * return error in this case, but just print a log
		 */
		dev_warn(component->dev, "%s: unknown compander: %d\n",
			__func__, comp);
	};
	return 0;
}

static int aqt_hph_asrc_mode_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			 snd_soc_kcontrol_component(kcontrol);
	struct aqt1000 *aqt = snd_soc_component_get_drvdata(component);
	int index = -EINVAL;

	if (!strcmp(kcontrol->id.name, "AQT ASRC0 Output Mode"))
		index = ASRC0;
	if (!strcmp(kcontrol->id.name, "AQT ASRC1 Output Mode"))
		index = ASRC1;

	if (aqt && (index >= 0) && (index < ASRC_MAX))
		aqt->asrc_output_mode[index] =
			ucontrol->value.integer.value[0];

	return 0;
}

static int aqt_hph_asrc_mode_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct aqt1000 *aqt = snd_soc_component_get_drvdata(component);
	int val = 0;
	int index = -EINVAL;

	if (!strcmp(kcontrol->id.name, "AQT ASRC0 Output Mode"))
		index = ASRC0;
	if (!strcmp(kcontrol->id.name, "AQT ASRC1 Output Mode"))
		index = ASRC1;

	if (aqt && (index >= 0) && (index < ASRC_MAX))
		val = aqt->asrc_output_mode[index];

	ucontrol->value.integer.value[0] = val;

	return 0;
}

static const char * const asrc_mode_text[] = {
	"INT", "FRAC"
};
static SOC_ENUM_SINGLE_EXT_DECL(asrc_mode_enum, asrc_mode_text);

static int aqt_hph_idle_detect_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct aqt1000 *aqt = snd_soc_component_get_drvdata(component);
	int val = 0;

	if (aqt)
		val = aqt->idle_det_cfg.hph_idle_detect_en;

	ucontrol->value.integer.value[0] = val;

	return 0;
}

static int aqt_hph_idle_detect_put(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct aqt1000 *aqt = snd_soc_component_get_drvdata(component);

	if (aqt)
		aqt->idle_det_cfg.hph_idle_detect_en =
			ucontrol->value.integer.value[0];

	return 0;
}

static const char * const hph_idle_detect_text[] = {
	"OFF", "ON"
};

static SOC_ENUM_SINGLE_EXT_DECL(hph_idle_detect_enum, hph_idle_detect_text);

static int aqt_amic_pwr_lvl_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	u16 amic_reg = 0;

	if (!strcmp(kcontrol->id.name, "AQT AMIC_1_2 PWR MODE"))
		amic_reg = AQT1000_ANA_AMIC1;
	if (!strcmp(kcontrol->id.name, "AQT AMIC_3 PWR MODE"))
		amic_reg = AQT1000_ANA_AMIC3;

	if (amic_reg)
		ucontrol->value.integer.value[0] =
			(snd_soc_component_read32(component, amic_reg) &
			 AQT1000_AMIC_PWR_LVL_MASK) >>
			  AQT1000_AMIC_PWR_LVL_SHIFT;
	return 0;
}

static int aqt_amic_pwr_lvl_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	u32 mode_val;
	u16 amic_reg = 0;

	mode_val = ucontrol->value.enumerated.item[0];

	dev_dbg(component->dev, "%s: mode: %d\n", __func__, mode_val);

	if (!strcmp(kcontrol->id.name, "AQT AMIC_1_2 PWR MODE"))
		amic_reg = AQT1000_ANA_AMIC1;
	if (!strcmp(kcontrol->id.name, "AQT AMIC_3 PWR MODE"))
		amic_reg = AQT1000_ANA_AMIC3;

	if (amic_reg)
		snd_soc_component_update_bits(component, amic_reg,
				AQT1000_AMIC_PWR_LVL_MASK,
				mode_val << AQT1000_AMIC_PWR_LVL_SHIFT);
	return 0;
}

static const char * const amic_pwr_lvl_text[] = {
	"LOW_PWR", "DEFAULT", "HIGH_PERF", "HYBRID"
};

static SOC_ENUM_SINGLE_EXT_DECL(amic_pwr_lvl_enum, amic_pwr_lvl_text);

static const struct snd_kcontrol_new aqt_snd_controls[] = {
	SOC_SINGLE_TLV("AQT HPHL Volume", AQT1000_HPH_L_EN, 0, 24, 1, hph_gain),
	SOC_SINGLE_TLV("AQT HPHR Volume", AQT1000_HPH_R_EN, 0, 24, 1, hph_gain),
	SOC_SINGLE_TLV("AQT ADC1 Volume", AQT1000_ANA_AMIC1, 0, 20, 0,
			analog_gain),
	SOC_SINGLE_TLV("AQT ADC2 Volume", AQT1000_ANA_AMIC2, 0, 20, 0,
			analog_gain),
	SOC_SINGLE_TLV("AQT ADC3 Volume", AQT1000_ANA_AMIC3, 0, 20, 0,
			analog_gain),

	SOC_SINGLE_SX_TLV("AQT RX1 Digital Volume", AQT1000_CDC_RX1_RX_VOL_CTL,
		0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("AQT RX2 Digital Volume", AQT1000_CDC_RX2_RX_VOL_CTL,
		0, -84, 40, digital_gain),

	SOC_SINGLE_SX_TLV("AQT DEC0 Volume", AQT1000_CDC_TX0_TX_VOL_CTL, 0,
		-84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("AQT DEC1 Volume", AQT1000_CDC_TX1_TX_VOL_CTL, 0,
		-84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("AQT DEC2 Volume", AQT1000_CDC_TX2_TX_VOL_CTL, 0,
		-84, 40, digital_gain),

	SOC_SINGLE_SX_TLV("AQT IIR0 INP0 Volume",
		AQT1000_CDC_SIDETONE_IIR0_IIR_GAIN_B1_CTL, 0, -84, 40,
		digital_gain),
	SOC_SINGLE_SX_TLV("AQT IIR0 INP1 Volume",
		AQT1000_CDC_SIDETONE_IIR0_IIR_GAIN_B2_CTL, 0, -84, 40,
		digital_gain),
	SOC_SINGLE_SX_TLV("AQT IIR0 INP2 Volume",
		AQT1000_CDC_SIDETONE_IIR0_IIR_GAIN_B3_CTL, 0, -84, 40,
		digital_gain),
	SOC_SINGLE_SX_TLV("AQT IIR0 INP3 Volume",
		AQT1000_CDC_SIDETONE_IIR0_IIR_GAIN_B4_CTL, 0, -84, 40,
		digital_gain),
	SOC_SINGLE_EXT("AQT ANC Slot", SND_SOC_NOPM, 0, 100, 0,
			aqt_get_anc_slot, aqt_put_anc_slot),
	SOC_ENUM_EXT("AQT ANC Function", aqt_anc_func_enum, aqt_get_anc_func,
		aqt_put_anc_func),

	SOC_ENUM("AQT TX0 HPF cut off", cf_dec0_enum),
	SOC_ENUM("AQT TX1 HPF cut off", cf_dec1_enum),
	SOC_ENUM("AQT TX2 HPF cut off", cf_dec2_enum),

	SOC_ENUM("AQT RX INT1_1 HPF cut off", cf_int1_1_enum),
	SOC_ENUM("AQT RX INT1_2 HPF cut off", cf_int1_2_enum),
	SOC_ENUM("AQT RX INT2_1 HPF cut off", cf_int2_1_enum),
	SOC_ENUM("AQT RX INT2_2 HPF cut off", cf_int2_2_enum),

	SOC_ENUM_EXT("AQT RX HPH Mode", rx_hph_mode_mux_enum,
		aqt_rx_hph_mode_get, aqt_rx_hph_mode_put),

	SOC_SINGLE_EXT("AQT IIR0 Enable Band1", IIR0, BAND1, 1, 0,
		aqt_iir_enable_audio_mixer_get,
		aqt_iir_enable_audio_mixer_put),
	SOC_SINGLE_EXT("AQT IIR0 Enable Band2", IIR0, BAND2, 1, 0,
		aqt_iir_enable_audio_mixer_get,
		aqt_iir_enable_audio_mixer_put),
	SOC_SINGLE_EXT("AQT IIR0 Enable Band3", IIR0, BAND3, 1, 0,
		aqt_iir_enable_audio_mixer_get,
		aqt_iir_enable_audio_mixer_put),
	SOC_SINGLE_EXT("AQT IIR0 Enable Band4", IIR0, BAND4, 1, 0,
		aqt_iir_enable_audio_mixer_get,
		aqt_iir_enable_audio_mixer_put),
	SOC_SINGLE_EXT("AQT IIR0 Enable Band5", IIR0, BAND5, 1, 0,
		aqt_iir_enable_audio_mixer_get,
		aqt_iir_enable_audio_mixer_put),

	SOC_SINGLE_MULTI_EXT("AQT IIR0 Band1", IIR0, BAND1, 255, 0, 5,
		aqt_iir_band_audio_mixer_get, aqt_iir_band_audio_mixer_put),
	SOC_SINGLE_MULTI_EXT("AQT IIR0 Band2", IIR0, BAND2, 255, 0, 5,
		aqt_iir_band_audio_mixer_get, aqt_iir_band_audio_mixer_put),
	SOC_SINGLE_MULTI_EXT("AQT IIR0 Band3", IIR0, BAND3, 255, 0, 5,
		aqt_iir_band_audio_mixer_get, aqt_iir_band_audio_mixer_put),
	SOC_SINGLE_MULTI_EXT("AQT IIR0 Band4", IIR0, BAND4, 255, 0, 5,
		aqt_iir_band_audio_mixer_get, aqt_iir_band_audio_mixer_put),
	SOC_SINGLE_MULTI_EXT("AQT IIR0 Band5", IIR0, BAND5, 255, 0, 5,
		aqt_iir_band_audio_mixer_get, aqt_iir_band_audio_mixer_put),

	SOC_SINGLE_EXT("AQT COMP1 Switch", SND_SOC_NOPM, COMPANDER_1, 1, 0,
		aqt_compander_get, aqt_compander_put),
	SOC_SINGLE_EXT("AQT COMP2 Switch", SND_SOC_NOPM, COMPANDER_2, 1, 0,
		aqt_compander_get, aqt_compander_put),

	SOC_ENUM_EXT("AQT ASRC0 Output Mode", asrc_mode_enum,
		aqt_hph_asrc_mode_get, aqt_hph_asrc_mode_put),
	SOC_ENUM_EXT("AQT ASRC1 Output Mode", asrc_mode_enum,
		aqt_hph_asrc_mode_get, aqt_hph_asrc_mode_put),

	SOC_ENUM_EXT("AQT HPH Idle Detect", hph_idle_detect_enum,
		aqt_hph_idle_detect_get, aqt_hph_idle_detect_put),

	SOC_ENUM_EXT("AQT AMIC_1_2 PWR MODE", amic_pwr_lvl_enum,
		aqt_amic_pwr_lvl_get, aqt_amic_pwr_lvl_put),
	SOC_ENUM_EXT("AQT AMIC_3 PWR MODE", amic_pwr_lvl_enum,
		aqt_amic_pwr_lvl_get, aqt_amic_pwr_lvl_put),
};

static int aqt_codec_enable_rx_bias(struct snd_soc_dapm_widget *w,
				      struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
			snd_soc_dapm_to_component(w->dapm);
	struct aqt1000 *aqt = snd_soc_component_get_drvdata(component);

	dev_dbg(component->dev, "%s %s %d\n", __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		aqt->rx_bias_count++;
		if (aqt->rx_bias_count == 1) {
			snd_soc_component_update_bits(component,
					AQT1000_ANA_RX_SUPPLIES,
					0x01, 0x01);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		aqt->rx_bias_count--;
		if (!aqt->rx_bias_count)
			snd_soc_component_update_bits(component,
					AQT1000_ANA_RX_SUPPLIES,
					0x01, 0x00);
		break;
	};
	dev_dbg(component->dev, "%s: Current RX BIAS user count: %d\n",
		__func__, aqt->rx_bias_count);

	return 0;
}

/*
 * aqt_mbhc_micb_adjust_voltage: adjust specific micbias voltage
 * @component: handle to snd_soc_component *
 * @req_volt: micbias voltage to be set
 * @micb_num: micbias to be set, e.g. micbias1 or micbias2
 *
 * return 0 if adjustment is success or error code in case of failure
 */
int aqt_mbhc_micb_adjust_voltage(struct snd_soc_component *component,
				   int req_volt, int micb_num)
{
	struct aqt1000 *aqt;
	int cur_vout_ctl, req_vout_ctl;
	int micb_reg, micb_val, micb_en;
	int ret = 0;

	if (!component) {
		pr_err("%s: Invalid component pointer\n", __func__);
		return -EINVAL;
	}

	if (micb_num != MIC_BIAS_1)
		return -EINVAL;
	else
		micb_reg = AQT1000_ANA_MICB1;

	aqt = snd_soc_component_get_drvdata(component);
	mutex_lock(&aqt->micb_lock);

	/*
	 * If requested micbias voltage is same as current micbias
	 * voltage, then just return. Otherwise, adjust voltage as
	 * per requested value. If micbias is already enabled, then
	 * to avoid slow micbias ramp-up or down enable pull-up
	 * momentarily, change the micbias value and then re-enable
	 * micbias.
	 */
	micb_val = snd_soc_component_read32(component, micb_reg);
	micb_en = (micb_val & 0xC0) >> 6;
	cur_vout_ctl = micb_val & 0x3F;

	req_vout_ctl = aqt_get_micb_vout_ctl_val(req_volt);
	if (req_vout_ctl < 0) {
		ret = -EINVAL;
		goto exit;
	}
	if (cur_vout_ctl == req_vout_ctl) {
		ret = 0;
		goto exit;
	}

	dev_dbg(component->dev, "%s: micb_num: %d, cur_mv: %d, req_mv: %d, micb_en: %d\n",
		 __func__, micb_num, AQT_VOUT_CTL_TO_MICB(cur_vout_ctl),
		 req_volt, micb_en);

	if (micb_en == 0x1)
		snd_soc_component_update_bits(component, micb_reg, 0xC0, 0x80);

	snd_soc_component_update_bits(component, micb_reg, 0x3F, req_vout_ctl);

	if (micb_en == 0x1) {
		snd_soc_component_update_bits(component, micb_reg, 0xC0, 0x40);
		/*
		 * Add 2ms delay as per HW requirement after enabling
		 * micbias
		 */
		usleep_range(2000, 2100);
	}
exit:
	mutex_unlock(&aqt->micb_lock);

	return ret;
}
EXPORT_SYMBOL(aqt_mbhc_micb_adjust_voltage);

/*
 * aqt_micbias_control: enable/disable micbias
 * @component: handle to snd_soc_component *
 * @micb_num: micbias to be enabled/disabled, e.g. micbias1 or micbias2
 * @req: control requested, enable/disable or pullup enable/disable
 * @is_dapm: triggered by dapm or not
 *
 * return 0 if control is success or error code in case of failure
 */
int aqt_micbias_control(struct snd_soc_component *component,
			  int micb_num, int req, bool is_dapm)
{
	struct aqt1000 *aqt = snd_soc_component_get_drvdata(component);
	u16 micb_reg;
	int pre_off_event = 0, post_off_event = 0;
	int post_on_event = 0, post_dapm_off = 0;
	int post_dapm_on = 0;
	int ret = 0;

	switch (micb_num) {
	case MIC_BIAS_1:
		micb_reg = AQT1000_ANA_MICB1;
		pre_off_event = AQT_EVENT_PRE_MICBIAS_1_OFF;
		post_off_event = AQT_EVENT_POST_MICBIAS_1_OFF;
		post_on_event = AQT_EVENT_POST_MICBIAS_1_ON;
		post_dapm_on = AQT_EVENT_POST_DAPM_MICBIAS_1_ON;
		post_dapm_off = AQT_EVENT_POST_DAPM_MICBIAS_1_OFF;
		break;
	default:
		dev_err(component->dev, "%s: Invalid micbias number: %d\n",
			__func__, micb_num);
		return -EINVAL;
	}
	mutex_lock(&aqt->micb_lock);

	switch (req) {
	case MICB_PULLUP_ENABLE:
		aqt->pullup_ref++;
		if ((aqt->pullup_ref == 1) &&
		    (aqt->micb_ref == 0))
			snd_soc_component_update_bits(component, micb_reg,
						      0xC0, 0x80);
		break;
	case MICB_PULLUP_DISABLE:
		if (aqt->pullup_ref > 0)
			aqt->pullup_ref--;
		if ((aqt->pullup_ref == 0) &&
		    (aqt->micb_ref == 0))
			snd_soc_component_update_bits(component, micb_reg,
						      0xC0, 0x00);
		break;
	case MICB_ENABLE:
		aqt->micb_ref++;
		if (aqt->micb_ref == 1) {
			snd_soc_component_update_bits(component, micb_reg,
						      0xC0, 0x40);
			if (post_on_event && aqt->mbhc)
				blocking_notifier_call_chain(
						&aqt->mbhc->notifier,
						post_on_event,
						&aqt->mbhc->wcd_mbhc);
		}
		if (is_dapm && post_dapm_on && aqt->mbhc)
			blocking_notifier_call_chain(&aqt->mbhc->notifier,
					post_dapm_on, &aqt->mbhc->wcd_mbhc);
		break;
	case MICB_DISABLE:
		if (aqt->micb_ref > 0)
			aqt->micb_ref--;
		if ((aqt->micb_ref == 0) &&
		    (aqt->pullup_ref > 0))
			snd_soc_component_update_bits(component, micb_reg,
						      0xC0, 0x80);
		else if ((aqt->micb_ref == 0) &&
			 (aqt->pullup_ref == 0)) {
			if (pre_off_event && aqt->mbhc)
				blocking_notifier_call_chain(
						&aqt->mbhc->notifier,
						pre_off_event,
						&aqt->mbhc->wcd_mbhc);
			snd_soc_component_update_bits(component, micb_reg,
						      0xC0, 0x00);
			if (post_off_event && aqt->mbhc)
				blocking_notifier_call_chain(
						&aqt->mbhc->notifier,
						post_off_event,
						&aqt->mbhc->wcd_mbhc);
		}
		if (is_dapm && post_dapm_off && aqt->mbhc)
			blocking_notifier_call_chain(&aqt->mbhc->notifier,
					post_dapm_off, &aqt->mbhc->wcd_mbhc);
		break;
	default:
		dev_err(component->dev, "%s: Invalid micbias request: %d\n",
			__func__, req);
		ret = -EINVAL;
		break;
	};

	if (!ret)
		dev_dbg(component->dev,
			"%s: micb_num:%d, micb_ref: %d, pullup_ref: %d\n",
			__func__, micb_num, aqt->micb_ref, aqt->pullup_ref);

	mutex_unlock(&aqt->micb_lock);

	return ret;
}
EXPORT_SYMBOL(aqt_micbias_control);

static int __aqt_codec_enable_micbias(struct snd_soc_dapm_widget *w,
					int event)
{
	struct snd_soc_component *component =
			snd_soc_dapm_to_component(w->dapm);
	int micb_num;

	dev_dbg(component->dev, "%s: wname: %s, event: %d\n",
		__func__, w->name, event);

	if (strnstr(w->name, "AQT MIC BIAS1", sizeof("AQT MIC BIAS1")))
		micb_num = MIC_BIAS_1;
	else
		return -EINVAL;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/*
		 * MIC BIAS can also be requested by MBHC,
		 * so use ref count to handle micbias pullup
		 * and enable requests
		 */
		aqt_micbias_control(component, micb_num, MICB_ENABLE, true);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* wait for cnp time */
		usleep_range(1000, 1100);
		break;
	case SND_SOC_DAPM_POST_PMD:
		aqt_micbias_control(component, micb_num, MICB_DISABLE, true);
		break;
	};

	return 0;
}

static int aqt_codec_enable_micbias(struct snd_soc_dapm_widget *w,
				      struct snd_kcontrol *kcontrol, int event)
{
	return __aqt_codec_enable_micbias(w, event);
}

static int aqt_codec_enable_i2s_block(struct snd_soc_component *component)
{
	struct aqt1000 *aqt = snd_soc_component_get_drvdata(component);

	mutex_lock(&aqt->i2s_lock);
	if (++aqt->i2s_users == 1)
		snd_soc_component_update_bits(component, AQT1000_I2S_I2S_0_CTL,
					      0x01, 0x01);
	mutex_unlock(&aqt->i2s_lock);

	return 0;
}

static int aqt_codec_disable_i2s_block(struct snd_soc_component *component)
{
	struct aqt1000 *aqt = snd_soc_component_get_drvdata(component);

	mutex_lock(&aqt->i2s_lock);
	if (--aqt->i2s_users == 0)
		snd_soc_component_update_bits(component, AQT1000_I2S_I2S_0_CTL,
					      0x01, 0x00);

	if (aqt->i2s_users < 0)
		dev_warn(component->dev, "%s: i2s_users count (%d) < 0\n",
			 __func__, aqt->i2s_users);
	mutex_unlock(&aqt->i2s_lock);

	return 0;
}

static int aqt_codec_enable_i2s_tx(struct snd_soc_dapm_widget *w,
				   struct snd_kcontrol *kcontrol,
				   int event)
{
	struct snd_soc_component *component =
			snd_soc_dapm_to_component(w->dapm);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		aqt_codec_enable_i2s_block(component);
		break;
	case SND_SOC_DAPM_POST_PMD:
		aqt_codec_disable_i2s_block(component);
		break;
	}
	dev_dbg(component->dev, "%s: event: %d\n", __func__, event);

	return 0;
}

static int aqt_codec_enable_i2s_rx(struct snd_soc_dapm_widget *w,
				   struct snd_kcontrol *kcontrol,
				   int event)
{
	struct snd_soc_component *component =
			snd_soc_dapm_to_component(w->dapm);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		aqt_codec_enable_i2s_block(component);
		break;
	case SND_SOC_DAPM_POST_PMD:
		aqt_codec_disable_i2s_block(component);
		break;
	}
	dev_dbg(component->dev, "%s: event: %d\n", __func__, event);

	return 0;
}

static const char * const tx_mux_text[] = {
	"ZERO", "DEC_L", "DEC_R", "DEC_V",
};
AQT_DAPM_ENUM(tx0, AQT1000_CDC_IF_ROUTER_TX_MUX_CFG0, 0, tx_mux_text);
AQT_DAPM_ENUM(tx1, AQT1000_CDC_IF_ROUTER_TX_MUX_CFG0, 2, tx_mux_text);

static const char * const tx_adc_mux_text[] = {
	"AMIC", "ANC_FB0", "ANC_FB1",
};
AQT_DAPM_ENUM(tx_adc0, AQT1000_CDC_TX_INP_MUX_ADC_MUX0_CFG1, 0,
	      tx_adc_mux_text);
AQT_DAPM_ENUM(tx_adc1, AQT1000_CDC_TX_INP_MUX_ADC_MUX1_CFG1, 0,
	      tx_adc_mux_text);
AQT_DAPM_ENUM(tx_adc2, AQT1000_CDC_TX_INP_MUX_ADC_MUX2_CFG1, 0,
	      tx_adc_mux_text);

static int aqt_find_amic_input(struct snd_soc_component *component,
			       int adc_mux_n)
{
	u8 mask;
	u16 adc_mux_in_reg = 0, amic_mux_sel_reg = 0;
	bool is_amic;

	if (adc_mux_n > 2)
		return 0;

	if (adc_mux_n < 3) {
		adc_mux_in_reg = AQT1000_CDC_TX_INP_MUX_ADC_MUX0_CFG1 +
				 adc_mux_n;
		mask = 0x03;
		amic_mux_sel_reg = AQT1000_CDC_TX_INP_MUX_ADC_MUX0_CFG0 +
				   2 * adc_mux_n;
	}
	is_amic = (
		((snd_soc_component_read32(component, adc_mux_in_reg)
		  & mask)) == 0);
	if (!is_amic)
		return 0;

	return snd_soc_component_read32(component, amic_mux_sel_reg) & 0x07;
}

static u16 aqt_codec_get_amic_pwlvl_reg(
		struct snd_soc_component *component, int amic)
{
	u16 pwr_level_reg = 0;

	switch (amic) {
	case 1:
	case 2:
		pwr_level_reg = AQT1000_ANA_AMIC1;
		break;
	case 3:
		pwr_level_reg = AQT1000_ANA_AMIC3;
		break;
	default:
		dev_dbg(component->dev, "%s: invalid amic: %d\n",
			__func__, amic);
		break;
	}

	return pwr_level_reg;
}

static void aqt_tx_hpf_corner_freq_callback(struct work_struct *work)
{
	struct delayed_work *hpf_delayed_work;
	struct hpf_work *hpf_work;
	struct aqt1000 *aqt;
	struct snd_soc_component *component;
	u16 dec_cfg_reg, amic_reg, go_bit_reg;
	u8 hpf_cut_off_freq;
	int amic_n;

	hpf_delayed_work = to_delayed_work(work);
	hpf_work = container_of(hpf_delayed_work, struct hpf_work, dwork);
	aqt = hpf_work->aqt;
	component = aqt->component;
	hpf_cut_off_freq = hpf_work->hpf_cut_off_freq;

	dec_cfg_reg = AQT1000_CDC_TX0_TX_PATH_CFG0 + 16 * hpf_work->decimator;
	go_bit_reg = dec_cfg_reg + 7;

	dev_dbg(component->dev, "%s: decimator %u hpf_cut_of_freq 0x%x\n",
		__func__, hpf_work->decimator, hpf_cut_off_freq);

	amic_n = aqt_find_amic_input(component, hpf_work->decimator);
	if (amic_n) {
		amic_reg = AQT1000_ANA_AMIC1 + amic_n - 1;
		aqt_codec_set_tx_hold(component, amic_reg, false);
	}
	snd_soc_component_update_bits(component, dec_cfg_reg,
			TX_HPF_CUT_OFF_FREQ_MASK,
			hpf_cut_off_freq << 5);
	snd_soc_component_update_bits(component, go_bit_reg, 0x02, 0x02);
	/* Minimum 1 clk cycle delay is required as per HW spec */
	usleep_range(1000, 1010);
	snd_soc_component_update_bits(component, go_bit_reg, 0x02, 0x00);
}

static void aqt_tx_mute_update_callback(struct work_struct *work)
{
	struct tx_mute_work *tx_mute_dwork;
	struct aqt1000 *aqt;
	struct delayed_work *delayed_work;
	struct snd_soc_component *component;
	u16 tx_vol_ctl_reg, hpf_gate_reg;

	delayed_work = to_delayed_work(work);
	tx_mute_dwork = container_of(delayed_work, struct tx_mute_work, dwork);
	aqt = tx_mute_dwork->aqt;
	component = aqt->component;

	tx_vol_ctl_reg = AQT1000_CDC_TX0_TX_PATH_CTL +
			 16 * tx_mute_dwork->decimator;
	hpf_gate_reg = AQT1000_CDC_TX0_TX_PATH_SEC2 +
		       16 * tx_mute_dwork->decimator;
	snd_soc_component_update_bits(component, tx_vol_ctl_reg, 0x10, 0x00);
}

static int aqt_codec_enable_dec(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
			snd_soc_dapm_to_component(w->dapm);
	struct aqt1000 *aqt = snd_soc_component_get_drvdata(component);
	char *widget_name = NULL;
	char *dec = NULL;
	unsigned int decimator = 0;
	u8 amic_n = 0;
	u16 tx_vol_ctl_reg, pwr_level_reg = 0, dec_cfg_reg, hpf_gate_reg;
	u16 tx_gain_ctl_reg;
	int ret = 0;
	u8 hpf_cut_off_freq;

	dev_dbg(component->dev, "%s: event: %d\n", __func__, event);
	widget_name = kstrndup(w->name, 15, GFP_KERNEL);
	if (!widget_name)
		return -ENOMEM;

	dec = strpbrk(widget_name, "012");
	if (!dec) {
		dev_err(component->dev, "%s: decimator index not found\n",
			__func__);
		ret =  -EINVAL;
		goto out;
	}

	ret = kstrtouint(dec, 10, &decimator);
	if (ret < 0) {
		dev_err(component->dev, "%s: Invalid decimator = %s\n",
			__func__, widget_name);
		ret =  -EINVAL;
		goto out;
	}
	dev_dbg(component->dev, "%s(): widget = %s decimator = %u\n", __func__,
			w->name, decimator);

	tx_vol_ctl_reg = AQT1000_CDC_TX0_TX_PATH_CTL + 16 * decimator;
	hpf_gate_reg = AQT1000_CDC_TX0_TX_PATH_SEC2 + 16 * decimator;
	dec_cfg_reg = AQT1000_CDC_TX0_TX_PATH_CFG0 + 16 * decimator;
	tx_gain_ctl_reg = AQT1000_CDC_TX0_TX_VOL_CTL + 16 * decimator;

	amic_n = aqt_find_amic_input(component, decimator);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (amic_n)
			pwr_level_reg = aqt_codec_get_amic_pwlvl_reg(component,
								     amic_n);
		if (pwr_level_reg) {
			switch ((snd_soc_component_read32(
					component, pwr_level_reg) &
					AQT1000_AMIC_PWR_LVL_MASK) >>
					AQT1000_AMIC_PWR_LVL_SHIFT) {
			case AQT1000_AMIC_PWR_LEVEL_LP:
				snd_soc_component_update_bits(
						component, dec_cfg_reg,
						AQT1000_DEC_PWR_LVL_MASK,
						AQT1000_DEC_PWR_LVL_LP);
				break;

			case AQT1000_AMIC_PWR_LEVEL_HP:
				snd_soc_component_update_bits(
						component, dec_cfg_reg,
						AQT1000_DEC_PWR_LVL_MASK,
						AQT1000_DEC_PWR_LVL_HP);
				break;
			case AQT1000_AMIC_PWR_LEVEL_DEFAULT:
			default:
				snd_soc_component_update_bits(
						component, dec_cfg_reg,
						AQT1000_DEC_PWR_LVL_MASK,
						AQT1000_DEC_PWR_LVL_DF);
				break;
			}
		}
		/* Enable TX PGA Mute */
		snd_soc_component_update_bits(component, tx_vol_ctl_reg,
					      0x10, 0x10);
		break;
	case SND_SOC_DAPM_POST_PMU:
		hpf_cut_off_freq = (snd_soc_component_read32(
				    component, dec_cfg_reg) &
				    TX_HPF_CUT_OFF_FREQ_MASK) >> 5;

		aqt->tx_hpf_work[decimator].hpf_cut_off_freq =
							hpf_cut_off_freq;
		if (hpf_cut_off_freq != CF_MIN_3DB_150HZ) {
			snd_soc_component_update_bits(component, dec_cfg_reg,
					    TX_HPF_CUT_OFF_FREQ_MASK,
					    CF_MIN_3DB_150HZ << 5);
			snd_soc_component_update_bits(component, hpf_gate_reg,
						      0x02, 0x02);
			/*
			 * Minimum 1 clk cycle delay is required as per
			 * HW spec.
			 */
			usleep_range(1000, 1010);
			snd_soc_component_update_bits(component, hpf_gate_reg,
						      0x02, 0x00);
		}
		/* schedule work queue to Remove Mute */
		schedule_delayed_work(&aqt->tx_mute_dwork[decimator].dwork,
				      msecs_to_jiffies(tx_unmute_delay));
		if (aqt->tx_hpf_work[decimator].hpf_cut_off_freq !=
							CF_MIN_3DB_150HZ)
			schedule_delayed_work(
					&aqt->tx_hpf_work[decimator].dwork,
					msecs_to_jiffies(300));
		/* apply gain after decimator is enabled */
		snd_soc_component_write(component, tx_gain_ctl_reg,
			      snd_soc_component_read32(
					component, tx_gain_ctl_reg));
		break;
	case SND_SOC_DAPM_PRE_PMD:
		hpf_cut_off_freq =
			aqt->tx_hpf_work[decimator].hpf_cut_off_freq;
		snd_soc_component_update_bits(component, tx_vol_ctl_reg,
					      0x10, 0x10);
		if (cancel_delayed_work_sync(
		    &aqt->tx_hpf_work[decimator].dwork)) {
			if (hpf_cut_off_freq != CF_MIN_3DB_150HZ) {
				snd_soc_component_update_bits(
						component, dec_cfg_reg,
						TX_HPF_CUT_OFF_FREQ_MASK,
						hpf_cut_off_freq << 5);
				snd_soc_component_update_bits(
						component, hpf_gate_reg,
						0x02, 0x02);
				/*
				 * Minimum 1 clk cycle delay is required as per
				 * HW spec.
				 */
				usleep_range(1000, 1010);
				snd_soc_component_update_bits(
						component, hpf_gate_reg,
						0x02, 0x00);
			}
		}
		cancel_delayed_work_sync(
				&aqt->tx_mute_dwork[decimator].dwork);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_component_update_bits(component, tx_vol_ctl_reg,
					      0x10, 0x00);
		snd_soc_component_update_bits(component, dec_cfg_reg,
				    AQT1000_DEC_PWR_LVL_MASK,
				    AQT1000_DEC_PWR_LVL_DF);
		break;
	}

out:
	kfree(widget_name);
	return ret;
}

static const char * const tx_amic_text[] = {
	"ZERO", "ADC_L", "ADC_R", "ADC_V",
};
AQT_DAPM_ENUM(tx_amic0, AQT1000_CDC_TX_INP_MUX_ADC_MUX0_CFG0, 0, tx_amic_text);
AQT_DAPM_ENUM(tx_amic1, AQT1000_CDC_TX_INP_MUX_ADC_MUX1_CFG0, 0, tx_amic_text);
AQT_DAPM_ENUM(tx_amic2, AQT1000_CDC_TX_INP_MUX_ADC_MUX2_CFG0, 0, tx_amic_text);

AQT_DAPM_ENUM(tx_amic10, AQT1000_CDC_TX_INP_MUX_ADC_MUX10_CFG0, 0,
	      tx_amic_text);
AQT_DAPM_ENUM(tx_amic11, AQT1000_CDC_TX_INP_MUX_ADC_MUX11_CFG0, 0,
	      tx_amic_text);
AQT_DAPM_ENUM(tx_amic12, AQT1000_CDC_TX_INP_MUX_ADC_MUX12_CFG0, 0,
	      tx_amic_text);
AQT_DAPM_ENUM(tx_amic13, AQT1000_CDC_TX_INP_MUX_ADC_MUX13_CFG0, 0,
	      tx_amic_text);

static int aqt_codec_enable_adc(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
			snd_soc_dapm_to_component(w->dapm);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		aqt_codec_set_tx_hold(component, w->reg, true);
		break;
	default:
		break;
	}

	return 0;
}

static const struct snd_kcontrol_new anc_hphl_pa_switch =
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0);

static const struct snd_kcontrol_new anc_hphr_pa_switch =
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0);

static int aqt_config_compander(struct snd_soc_component *component,
				int interp_n, int event)
{
	struct aqt1000 *aqt = snd_soc_component_get_drvdata(component);
	int comp;
	u16 comp_ctl0_reg, rx_path_cfg0_reg;

	comp = interp_n;
	dev_dbg(component->dev, "%s: event %d compander %d, enabled %d\n",
		__func__, event, comp, aqt->comp_enabled[comp]);

	if (!aqt->comp_enabled[comp])
		return 0;

	comp_ctl0_reg = AQT1000_CDC_COMPANDER1_CTL0 + (comp * 8);
	rx_path_cfg0_reg = AQT1000_CDC_RX1_RX_PATH_CFG0 + (comp * 20);

	if (SND_SOC_DAPM_EVENT_ON(event)) {
		/* Enable Compander Clock */
		snd_soc_component_update_bits(
				component, comp_ctl0_reg, 0x01, 0x01);
		snd_soc_component_update_bits(
				component, comp_ctl0_reg, 0x02, 0x02);
		snd_soc_component_update_bits(
				component, comp_ctl0_reg, 0x02, 0x00);
		snd_soc_component_update_bits(
				component, rx_path_cfg0_reg, 0x02, 0x02);
	}

	if (SND_SOC_DAPM_EVENT_OFF(event)) {
		snd_soc_component_update_bits(
				component, rx_path_cfg0_reg, 0x02, 0x00);
		snd_soc_component_update_bits(
				component, comp_ctl0_reg, 0x04, 0x04);
		snd_soc_component_update_bits(
				component, comp_ctl0_reg, 0x02, 0x02);
		snd_soc_component_update_bits(
				component, comp_ctl0_reg, 0x02, 0x00);
		snd_soc_component_update_bits(
				component, comp_ctl0_reg, 0x01, 0x00);
		snd_soc_component_update_bits(
				component, comp_ctl0_reg, 0x04, 0x00);
	}

	return 0;
}

static void aqt_codec_idle_detect_control(struct snd_soc_component *component,
					    int interp, int event)
{
	int reg = 0, mask, val;
	struct aqt1000 *aqt = snd_soc_component_get_drvdata(component);

	if (!aqt->idle_det_cfg.hph_idle_detect_en)
		return;

	if (interp == INTERP_HPHL) {
		reg = AQT1000_CDC_RX_IDLE_DET_PATH_CTL;
		mask = 0x01;
		val = 0x01;
	}
	if (interp == INTERP_HPHR) {
		reg = AQT1000_CDC_RX_IDLE_DET_PATH_CTL;
		mask = 0x02;
		val = 0x02;
	}

	if (reg && SND_SOC_DAPM_EVENT_ON(event))
		snd_soc_component_update_bits(component, reg, mask, val);

	if (reg && SND_SOC_DAPM_EVENT_OFF(event)) {
		snd_soc_component_update_bits(component, reg, mask, 0x00);
		aqt->idle_det_cfg.hph_idle_thr = 0;
		snd_soc_component_write(component,
				AQT1000_CDC_RX_IDLE_DET_CFG3, 0x0);
	}
}

static void aqt_codec_hphdelay_lutbypass(struct snd_soc_component *component,
				    u16 interp_idx, int event)
{
	struct aqt1000 *aqt = snd_soc_component_get_drvdata(component);
	u8 hph_dly_mask;
	u16 hph_lut_bypass_reg = 0;
	u16 hph_comp_ctrl7 = 0;


	switch (interp_idx) {
	case INTERP_HPHL:
		hph_dly_mask = 1;
		hph_lut_bypass_reg = AQT1000_CDC_TOP_HPHL_COMP_LUT;
		hph_comp_ctrl7 = AQT1000_CDC_COMPANDER1_CTL7;
		break;
	case INTERP_HPHR:
		hph_dly_mask = 2;
		hph_lut_bypass_reg = AQT1000_CDC_TOP_HPHR_COMP_LUT;
		hph_comp_ctrl7 = AQT1000_CDC_COMPANDER2_CTL7;
		break;
	default:
		break;
	}

	if (hph_lut_bypass_reg && SND_SOC_DAPM_EVENT_ON(event)) {
		snd_soc_component_update_bits(component, AQT1000_CDC_CLSH_TEST0,
				hph_dly_mask, 0x0);
		snd_soc_component_update_bits(component, hph_lut_bypass_reg,
				0x80, 0x80);
		if (aqt->hph_mode == CLS_H_ULP)
			snd_soc_component_update_bits(component, hph_comp_ctrl7,
				0x20, 0x20);
	}

	if (hph_lut_bypass_reg && SND_SOC_DAPM_EVENT_OFF(event)) {
		snd_soc_component_update_bits(component, AQT1000_CDC_CLSH_TEST0,
				hph_dly_mask, hph_dly_mask);
		snd_soc_component_update_bits(component, hph_lut_bypass_reg,
				0x80, 0x00);
		snd_soc_component_update_bits(component, hph_comp_ctrl7,
				0x20, 0x0);
	}
}

static int aqt_codec_enable_interp_clk(struct snd_soc_component *component,
				       int event, int interp_idx)
{
	struct aqt1000 *aqt;
	u16 main_reg, dsm_reg;

	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return -EINVAL;
	}

	aqt = snd_soc_component_get_drvdata(component);
	main_reg = AQT1000_CDC_RX1_RX_PATH_CTL + (interp_idx * 20);
	dsm_reg = AQT1000_CDC_RX1_RX_PATH_DSMDEM_CTL + (interp_idx * 20);

	if (SND_SOC_DAPM_EVENT_ON(event)) {
		if (aqt->main_clk_users[interp_idx] == 0) {
			/* Main path PGA mute enable */
			snd_soc_component_update_bits(component, main_reg,
						0x10, 0x10);
			/* Clk enable */
			snd_soc_component_update_bits(component, dsm_reg,
						0x01, 0x01);
			snd_soc_component_update_bits(component, main_reg,
						0x20, 0x20);
			aqt_codec_idle_detect_control(component, interp_idx,
							event);
			aqt_codec_hphdelay_lutbypass(component, interp_idx,
						       event);
			aqt_config_compander(component, interp_idx, event);
		}
		aqt->main_clk_users[interp_idx]++;
	}

	if (SND_SOC_DAPM_EVENT_OFF(event)) {
		aqt->main_clk_users[interp_idx]--;
		if (aqt->main_clk_users[interp_idx] <= 0) {
			aqt->main_clk_users[interp_idx] = 0;
			aqt_config_compander(component, interp_idx, event);
			aqt_codec_hphdelay_lutbypass(component, interp_idx,
						       event);
			aqt_codec_idle_detect_control(component, interp_idx,
							event);
			/* Clk Disable */
			snd_soc_component_update_bits(component, main_reg,
						0x20, 0x00);
			snd_soc_component_update_bits(component, dsm_reg,
						0x01, 0x00);
			/* Reset enable and disable */
			snd_soc_component_update_bits(component, main_reg,
						0x40, 0x40);
			snd_soc_component_update_bits(component, main_reg,
						0x40, 0x00);
			/* Reset rate to 48K*/
			snd_soc_component_update_bits(component, main_reg,
						0x0F, 0x04);
		}
	}

	dev_dbg(component->dev, "%s event %d main_clk_users %d\n",
		__func__,  event, aqt->main_clk_users[interp_idx]);

	return aqt->main_clk_users[interp_idx];
}

static int aqt_anc_out_switch_cb(struct snd_soc_dapm_widget *w,
				 struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(w->dapm);

	aqt_codec_enable_interp_clk(component, event, w->shift);

	return 0;
}

static const char * const anc0_fb_mux_text[] = {
	"ZERO", "ANC_IN_HPHL",
};

static const char * const anc1_fb_mux_text[] = {
	"ZERO", "ANC_IN_HPHR",
};

AQT_DAPM_ENUM(anc0_fb, AQT1000_CDC_RX_INP_MUX_ANC_CFG0, 0, anc0_fb_mux_text);
AQT_DAPM_ENUM(anc1_fb, AQT1000_CDC_RX_INP_MUX_ANC_CFG0, 3, anc1_fb_mux_text);

static const char *const rx_int1_1_mux_text[] = {
	"ZERO", "MAIN_DMA_L", "I2S0_L", "I2S0_R", "DEC_L", "DEC_R", "DEC_V",
	"SHADOW_I2S0_L", "MAIN_DMA_R"
};

static const char *const rx_int1_2_mux_text[] = {
	"ZERO", "MIX_DMA_L", "I2S0_L", "I2S0_R", "DEC_L", "DEC_R", "DEC_V",
	"IIR0", "MIX_DMA_R"
};

static const char *const rx_int2_1_mux_text[] = {
	"ZERO", "MAIN_DMA_R", "I2S0_L", "I2S0_R", "DEC_L", "DEC_R", "DEC_V",
	"SHADOW_I2S0_R", "MAIN_DMA_L"
};

static const char *const rx_int2_2_mux_text[] = {
	"ZERO", "MIX_DMA_R", "I2S0_L", "I2S0_R", "DEC_L", "DEC_R", "DEC_V",
	"IIR0", "MIX_DMA_L"
};

AQT_DAPM_ENUM(rx_int1_1, AQT1000_CDC_RX_INP_MUX_RX_INT1_CFG0, 0,
		rx_int1_1_mux_text);
AQT_DAPM_ENUM(rx_int1_2, AQT1000_CDC_RX_INP_MUX_RX_INT1_CFG1, 0,
		rx_int1_2_mux_text);
AQT_DAPM_ENUM(rx_int2_1, AQT1000_CDC_RX_INP_MUX_RX_INT2_CFG0, 0,
		rx_int2_1_mux_text);
AQT_DAPM_ENUM(rx_int2_2, AQT1000_CDC_RX_INP_MUX_RX_INT2_CFG1, 0,
		rx_int2_2_mux_text);

static int aqt_codec_set_idle_detect_thr(struct snd_soc_component *component,
					   int interp, int path_type)
{
	int port_id[4] = { 0, 0, 0, 0 };
	int *port_ptr, num_ports;
	int bit_width = 0;
	int mux_reg = 0, mux_reg_val = 0;
	struct aqt1000 *aqt = snd_soc_component_get_drvdata(component);
	int idle_thr;

	if ((interp != INTERP_HPHL) && (interp != INTERP_HPHR))
		return 0;

	if (!aqt->idle_det_cfg.hph_idle_detect_en)
		return 0;

	port_ptr = &port_id[0];
	num_ports = 0;

	if (path_type == INTERP_MIX_PATH) {
		if (interp == INTERP_HPHL)
			mux_reg =  AQT1000_CDC_RX_INP_MUX_RX_INT1_CFG1;
		else
			mux_reg = AQT1000_CDC_RX_INP_MUX_RX_INT2_CFG1;
	}

	if (path_type == INTERP_MAIN_PATH) {
		if (interp == INTERP_HPHL)
			mux_reg = AQT1000_CDC_RX_INP_MUX_RX_INT1_CFG0;
		else
			mux_reg = AQT1000_CDC_RX_INP_MUX_RX_INT2_CFG0;
	}
	mux_reg_val = snd_soc_component_read32(component, mux_reg);

	/* Read bit width from I2S reg if mux is set to I2S0_L or I2S0_R */
	if (mux_reg_val == 0x02 || mux_reg_val == 0x03)
		bit_width = ((snd_soc_component_read32(
				component, AQT1000_I2S_I2S_0_CTL) &
				0x40) >> 6);

	switch (bit_width) {
	case 1: /* 16 bit */
		idle_thr = 0xff; /* F16 */
		break;
	case 0: /* 32 bit */
	default:
		idle_thr = 0x03; /* F22 */
		break;
	}

	dev_dbg(component->dev, "%s: (new) idle_thr: %d, (cur) idle_thr: %d\n",
		__func__, idle_thr, aqt->idle_det_cfg.hph_idle_thr);

	if ((aqt->idle_det_cfg.hph_idle_thr == 0) ||
	    (idle_thr < aqt->idle_det_cfg.hph_idle_thr)) {
		snd_soc_component_write(component, AQT1000_CDC_RX_IDLE_DET_CFG3,
					idle_thr);
		aqt->idle_det_cfg.hph_idle_thr = idle_thr;
	}

	return 0;
}

static int aqt_codec_enable_main_path(struct snd_soc_dapm_widget *w,
					struct snd_kcontrol *kcontrol,
					int event)
{
	struct snd_soc_component *component =
			snd_soc_dapm_to_component(w->dapm);
	u16 gain_reg = 0;
	int val = 0;

	dev_dbg(component->dev, "%s %d %s\n", __func__, event, w->name);

	if (w->shift >= AQT1000_NUM_INTERPOLATORS) {
		dev_err(component->dev, "%s: Invalid Interpolator value %d for name %s\n",
			__func__, w->shift, w->name);
		return -EINVAL;
	};

	gain_reg = AQT1000_CDC_RX1_RX_VOL_CTL + (w->shift *
						 AQT1000_RX_PATH_CTL_OFFSET);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		aqt_codec_enable_interp_clk(component, event, w->shift);
		break;
	case SND_SOC_DAPM_POST_PMU:
		aqt_codec_set_idle_detect_thr(component, w->shift,
						INTERP_MAIN_PATH);
		/* apply gain after int clk is enabled */
		val = snd_soc_component_read32(component, gain_reg);
		snd_soc_component_write(component, gain_reg, val);
		break;
	case SND_SOC_DAPM_POST_PMD:
		aqt_codec_enable_interp_clk(component, event, w->shift);
		break;
	};

	return 0;
}

static int aqt_codec_enable_mix_path(struct snd_soc_dapm_widget *w,
				       struct snd_kcontrol *kcontrol,
				       int event)
{
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(w->dapm);
	u16 gain_reg = 0;
	u16 mix_reg = 0;

	if (w->shift >= AQT1000_NUM_INTERPOLATORS) {
		dev_err(component->dev, "%s: Invalid Interpolator value %d for name %s\n",
			__func__, w->shift, w->name);
		return -EINVAL;
	};
	gain_reg = AQT1000_CDC_RX1_RX_VOL_MIX_CTL +
					(w->shift * AQT1000_RX_PATH_CTL_OFFSET);
	mix_reg = AQT1000_CDC_RX1_RX_PATH_MIX_CTL +
					(w->shift * AQT1000_RX_PATH_CTL_OFFSET);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		aqt_codec_enable_interp_clk(component, event, w->shift);
		/* Clk enable */
		snd_soc_component_update_bits(component, mix_reg, 0x20, 0x20);
		break;
	case SND_SOC_DAPM_POST_PMU:
		aqt_codec_set_idle_detect_thr(component, w->shift,
						INTERP_MIX_PATH);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* Clk Disable */
		snd_soc_component_update_bits(component, mix_reg, 0x20, 0x00);
		aqt_codec_enable_interp_clk(component, event, w->shift);
		/* Reset enable and disable */
		snd_soc_component_update_bits(component, mix_reg, 0x40, 0x40);
		snd_soc_component_update_bits(component, mix_reg, 0x40, 0x00);

		break;
	};
	dev_dbg(component->dev, "%s event %d name %s\n", __func__,
		event, w->name);

	return 0;
}

static const char * const rx_int1_1_interp_mux_text[] = {
	"ZERO", "RX INT1_1 MUX",
};

static const char * const rx_int2_1_interp_mux_text[] = {
	"ZERO", "RX INT2_1 MUX",
};

static const char * const rx_int1_2_interp_mux_text[] = {
	"ZERO", "RX INT1_2 MUX",
};

static const char * const rx_int2_2_interp_mux_text[] = {
	"ZERO", "RX INT2_2 MUX",
};

AQT_DAPM_ENUM(rx_int1_1_interp, SND_SOC_NOPM, 0, rx_int1_1_interp_mux_text);
AQT_DAPM_ENUM(rx_int2_1_interp, SND_SOC_NOPM, 0, rx_int2_1_interp_mux_text);

AQT_DAPM_ENUM(rx_int1_2_interp, SND_SOC_NOPM, 0, rx_int1_2_interp_mux_text);
AQT_DAPM_ENUM(rx_int2_2_interp, SND_SOC_NOPM, 0, rx_int2_2_interp_mux_text);

static const char * const asrc0_mux_text[] = {
	"ZERO", "ASRC_IN_HPHL",
};

static const char * const asrc1_mux_text[] = {
	"ZERO", "ASRC_IN_HPHR",
};

AQT_DAPM_ENUM(asrc0, AQT1000_CDC_RX_INP_MUX_SPLINE_ASRC_CFG0, 0,
	asrc0_mux_text);
AQT_DAPM_ENUM(asrc1, AQT1000_CDC_RX_INP_MUX_SPLINE_ASRC_CFG0, 2,
	asrc1_mux_text);

static int aqt_get_asrc_mode(struct aqt1000 *aqt, int asrc,
			       u8 main_sr, u8 mix_sr)
{
	u8 asrc_output_mode;
	int asrc_mode = CONV_88P2K_TO_384K;

	if ((asrc < 0) || (asrc >= ASRC_MAX))
		return 0;

	asrc_output_mode = aqt->asrc_output_mode[asrc];

	if (asrc_output_mode) {
		/*
		 * If Mix sample rate is < 96KHz, use 96K to 352.8K
		 * conversion, or else use 384K to 352.8K conversion
		 */
		if (mix_sr < 5)
			asrc_mode = CONV_96K_TO_352P8K;
		else
			asrc_mode = CONV_384K_TO_352P8K;
	} else {
		/* Integer main and Fractional mix path */
		if (main_sr < 8 && mix_sr > 9) {
			asrc_mode = CONV_352P8K_TO_384K;
		} else if (main_sr > 8 && mix_sr < 8) {
			/* Fractional main and Integer mix path */
			if (mix_sr < 5)
				asrc_mode = CONV_96K_TO_352P8K;
			else
				asrc_mode = CONV_384K_TO_352P8K;
		} else if (main_sr < 8 && mix_sr < 8) {
			/* Integer main and Integer mix path */
			asrc_mode = CONV_96K_TO_384K;
		}
	}

	return asrc_mode;
}

static int aqt_codec_enable_asrc_resampler(struct snd_soc_dapm_widget *w,
					     struct snd_kcontrol *kcontrol,
					     int event)
{
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(w->dapm);
	struct aqt1000 *aqt = snd_soc_component_get_drvdata(component);
	int asrc = 0, ret = 0;
	u8 cfg;
	u16 cfg_reg = 0;
	u16 ctl_reg = 0;
	u16 clk_reg = 0;
	u16 asrc_ctl = 0;
	u16 mix_ctl_reg = 0;
	u16 paired_reg = 0;
	u8 main_sr, mix_sr, asrc_mode = 0;

	cfg = snd_soc_component_read32(component,
			AQT1000_CDC_RX_INP_MUX_SPLINE_ASRC_CFG0);
	if (!(cfg & 0xFF)) {
		dev_err(component->dev, "%s: ASRC%u input not selected\n",
			__func__, w->shift);
		return -EINVAL;
	}

	switch (w->shift) {
	case ASRC0:
		if ((cfg & 0x03) == 0x01) {
			cfg_reg = AQT1000_CDC_RX1_RX_PATH_CFG0;
			ctl_reg = AQT1000_CDC_RX1_RX_PATH_CTL;
			clk_reg = AQT1000_MIXING_ASRC0_CLK_RST_CTL;
			paired_reg = AQT1000_MIXING_ASRC1_CLK_RST_CTL;
			asrc_ctl = AQT1000_MIXING_ASRC0_CTL1;
		}
		break;
	case ASRC1:
		if ((cfg & 0x0C) == 0x4) {
			cfg_reg = AQT1000_CDC_RX2_RX_PATH_CFG0;
			ctl_reg = AQT1000_CDC_RX2_RX_PATH_CTL;
			clk_reg = AQT1000_MIXING_ASRC1_CLK_RST_CTL;
			paired_reg = AQT1000_MIXING_ASRC0_CLK_RST_CTL;
			asrc_ctl = AQT1000_MIXING_ASRC1_CTL1;
		}
		break;
	default:
		dev_err(component->dev, "%s: Invalid asrc:%u\n", __func__,
			w->shift);
		ret = -EINVAL;
		break;
	};

	if ((cfg_reg == 0) || (ctl_reg == 0) || (clk_reg == 0) ||
		 (asrc_ctl == 0) || ret)
		goto done;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if ((snd_soc_component_read32(component, clk_reg) & 0x02) ||
		    (snd_soc_component_read32(component, paired_reg) & 0x02)) {
			snd_soc_component_update_bits(component, clk_reg,
					0x02, 0x00);
			snd_soc_component_update_bits(component, paired_reg,
					0x02, 0x00);
		}
		snd_soc_component_update_bits(component, cfg_reg, 0x80, 0x80);
		snd_soc_component_update_bits(component, clk_reg, 0x01, 0x01);
		main_sr = snd_soc_component_read32(component, ctl_reg) & 0x0F;
		mix_ctl_reg = ctl_reg + 5;
		mix_sr = snd_soc_component_read32(
				component, mix_ctl_reg) & 0x0F;
		asrc_mode = aqt_get_asrc_mode(aqt, asrc,
						main_sr, mix_sr);
		dev_dbg(component->dev, "%s: main_sr:%d mix_sr:%d asrc_mode %d\n",
			__func__, main_sr, mix_sr, asrc_mode);
		snd_soc_component_update_bits(
				component, asrc_ctl, 0x07, asrc_mode);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_component_update_bits(component, asrc_ctl, 0x07, 0x00);
		snd_soc_component_update_bits(component, cfg_reg, 0x80, 0x00);
		snd_soc_component_update_bits(component, clk_reg, 0x03, 0x02);
		break;
	};

done:
	return ret;
}

static int aqt_codec_enable_anc(struct snd_soc_dapm_widget *w,
				  struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(w->dapm);
	struct aqt1000 *aqt = snd_soc_component_get_drvdata(component);
	const char *filename;
	const struct firmware *fw;
	int i;
	int ret = 0;
	int num_anc_slots;
	struct aqt1000_anc_header *anc_head;
	struct firmware_cal *hwdep_cal = NULL;
	u32 anc_writes_size = 0;
	u32 anc_cal_size = 0;
	int anc_size_remaining;
	u32 *anc_ptr;
	u16 reg;
	u8 mask, val;
	size_t cal_size;
	const void *data;

	if (!aqt->anc_func)
		return 0;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		hwdep_cal = wcdcal_get_fw_cal(aqt->fw_data, WCD9XXX_ANC_CAL);
		if (hwdep_cal) {
			data = hwdep_cal->data;
			cal_size = hwdep_cal->size;
			dev_dbg(component->dev, "%s: using hwdep calibration, cal_size %zd",
				__func__, cal_size);
		} else {
			filename = "AQT1000/AQT1000_anc.bin";
			ret = request_firmware(&fw, filename, component->dev);
			if (ret < 0) {
				dev_err(component->dev, "%s: Failed to acquire ANC data: %d\n",
					__func__, ret);
				return ret;
			}
			if (!fw) {
				dev_err(component->dev, "%s: Failed to get anc fw\n",
					__func__);
				return -ENODEV;
			}
			data = fw->data;
			cal_size = fw->size;
			dev_dbg(component->dev, "%s: using request_firmware calibration\n",
				__func__);
		}
		if (cal_size < sizeof(struct aqt1000_anc_header)) {
			dev_err(component->dev, "%s: Invalid cal_size %zd\n",
				__func__, cal_size);
			ret = -EINVAL;
			goto err;
		}
		/* First number is the number of register writes */
		anc_head = (struct aqt1000_anc_header *)(data);
		anc_ptr = (u32 *)(data + sizeof(struct aqt1000_anc_header));
		anc_size_remaining = cal_size -
				     sizeof(struct aqt1000_anc_header);
		num_anc_slots = anc_head->num_anc_slots;

		if (aqt->anc_slot >= num_anc_slots) {
			dev_err(component->dev, "%s: Invalid ANC slot selected\n",
				__func__);
			ret = -EINVAL;
			goto err;
		}
		for (i = 0; i < num_anc_slots; i++) {
			if (anc_size_remaining < AQT1000_PACKED_REG_SIZE) {
				dev_err(component->dev, "%s: Invalid register format\n",
					__func__);
				ret = -EINVAL;
				goto err;
			}
			anc_writes_size = (u32)(*anc_ptr);
			anc_size_remaining -= sizeof(u32);
			anc_ptr += 1;

			if ((anc_writes_size * AQT1000_PACKED_REG_SIZE) >
			    anc_size_remaining) {
				dev_err(component->dev, "%s: Invalid register format\n",
					__func__);
				ret = -EINVAL;
				goto err;
			}

			if (aqt->anc_slot == i)
				break;

			anc_size_remaining -= (anc_writes_size *
				AQT1000_PACKED_REG_SIZE);
			anc_ptr += anc_writes_size;
		}
		if (i == num_anc_slots) {
			dev_err(component->dev, "%s: Selected ANC slot not present\n",
				__func__);
			ret = -EINVAL;
			goto err;
		}

		i = 0;
		anc_cal_size = anc_writes_size;
		/* Rate converter clk enable and set bypass mode */
		if (!strcmp(w->name, "AQT RX INT1 DAC")) {
			snd_soc_component_update_bits(component,
					    AQT1000_CDC_ANC0_RC_COMMON_CTL,
					    0x05, 0x05);
			snd_soc_component_update_bits(component,
					AQT1000_CDC_ANC0_FIFO_COMMON_CTL,
					0x66, 0x66);
			anc_writes_size = anc_cal_size / 2;
			snd_soc_component_update_bits(component,
				AQT1000_CDC_ANC0_CLK_RESET_CTL, 0x39, 0x39);
		} else if (!strcmp(w->name, "AQT RX INT2 DAC")) {
			snd_soc_component_update_bits(component,
					    AQT1000_CDC_ANC1_RC_COMMON_CTL,
					    0x05, 0x05);
			snd_soc_component_update_bits(component,
					    AQT1000_CDC_ANC1_FIFO_COMMON_CTL,
					    0x66, 0x66);
			i = anc_cal_size / 2;
			snd_soc_component_update_bits(component,
				AQT1000_CDC_ANC1_CLK_RESET_CTL, 0x39, 0x39);
		}

		for (; i < anc_writes_size; i++) {
			AQT1000_CODEC_UNPACK_ENTRY(anc_ptr[i], reg, mask, val);
			snd_soc_component_write(component, reg, (val & mask));
		}
		if (!strcmp(w->name, "AQT RX INT1 DAC"))
			snd_soc_component_update_bits(component,
				AQT1000_CDC_ANC0_CLK_RESET_CTL, 0x08, 0x08);
		else if (!strcmp(w->name, "AQT RX INT2 DAC"))
			snd_soc_component_update_bits(component,
				AQT1000_CDC_ANC1_CLK_RESET_CTL, 0x08, 0x08);

		if (!hwdep_cal)
			release_firmware(fw);
		break;

	case SND_SOC_DAPM_POST_PMU:
		/* Remove ANC Rx from reset */
		snd_soc_component_update_bits(component,
				    AQT1000_CDC_ANC0_CLK_RESET_CTL,
				    0x08, 0x00);
		snd_soc_component_update_bits(component,
				    AQT1000_CDC_ANC1_CLK_RESET_CTL,
				    0x08, 0x00);
		break;

	case SND_SOC_DAPM_POST_PMD:
		snd_soc_component_update_bits(component,
				AQT1000_CDC_ANC0_RC_COMMON_CTL,
				0x05, 0x00);
		if (!strcmp(w->name, "AQT ANC HPHL PA")) {
			snd_soc_component_update_bits(component,
					AQT1000_CDC_ANC0_MODE_1_CTL,
					0x30, 0x00);
			/* 50 msec sleep is needed to avoid click and pop as
			 * per HW requirement
			 */
			msleep(50);
			snd_soc_component_update_bits(component,
					AQT1000_CDC_ANC0_MODE_1_CTL,
					0x01, 0x00);
			snd_soc_component_update_bits(component,
					AQT1000_CDC_ANC0_CLK_RESET_CTL,
					0x38, 0x38);
			snd_soc_component_update_bits(component,
					AQT1000_CDC_ANC0_CLK_RESET_CTL,
					0x07, 0x00);
			snd_soc_component_update_bits(component,
					AQT1000_CDC_ANC0_CLK_RESET_CTL,
					0x38, 0x00);
		} else if (!strcmp(w->name, "AQT ANC HPHR PA")) {
			snd_soc_component_update_bits(component,
					AQT1000_CDC_ANC1_MODE_1_CTL,
					0x30, 0x00);
			/* 50 msec sleep is needed to avoid click and pop as
			 * per HW requirement
			 */
			msleep(50);
			snd_soc_component_update_bits(component,
					AQT1000_CDC_ANC1_MODE_1_CTL,
					0x01, 0x00);
			snd_soc_component_update_bits(component,
					AQT1000_CDC_ANC1_CLK_RESET_CTL,
					0x38, 0x38);
			snd_soc_component_update_bits(component,
					AQT1000_CDC_ANC1_CLK_RESET_CTL,
					0x07, 0x00);
			snd_soc_component_update_bits(component,
					AQT1000_CDC_ANC1_CLK_RESET_CTL,
					0x38, 0x00);
		}
		break;
	}

	return 0;
err:
	if (!hwdep_cal)
		release_firmware(fw);
	return ret;
}

static void aqt_codec_override(struct snd_soc_component *component, int mode,
				 int event)
{
	if (mode == CLS_AB || mode == CLS_AB_HIFI) {
		switch (event) {
		case SND_SOC_DAPM_PRE_PMU:
		case SND_SOC_DAPM_POST_PMU:
			snd_soc_component_update_bits(component,
				AQT1000_ANA_RX_SUPPLIES, 0x02, 0x02);
		break;
		case SND_SOC_DAPM_POST_PMD:
			snd_soc_component_update_bits(component,
				AQT1000_ANA_RX_SUPPLIES, 0x02, 0x00);
		break;
		}
	}
}

static void aqt_codec_set_tx_hold(struct snd_soc_component *component,
				    u16 amic_reg, bool set)
{
	u8 mask = 0x20;
	u8 val;

	if (amic_reg == AQT1000_ANA_AMIC1 ||
	    amic_reg == AQT1000_ANA_AMIC3)
		mask = 0x40;

	val = set ? mask : 0x00;

	switch (amic_reg) {
	case AQT1000_ANA_AMIC1:
	case AQT1000_ANA_AMIC2:
		snd_soc_component_update_bits(component, AQT1000_ANA_AMIC2,
					mask, val);
		break;
	case AQT1000_ANA_AMIC3:
		snd_soc_component_update_bits(component, AQT1000_ANA_AMIC3_HPF,
					mask, val);
		break;
	default:
		dev_dbg(component->dev, "%s: invalid amic: %d\n",
			__func__, amic_reg);
		break;
	}
}

static void aqt_codec_clear_anc_tx_hold(struct aqt1000 *aqt)
{
	if (test_and_clear_bit(ANC_MIC_AMIC1, &aqt->status_mask))
		aqt_codec_set_tx_hold(aqt->component, AQT1000_ANA_AMIC1, false);
	if (test_and_clear_bit(ANC_MIC_AMIC2, &aqt->status_mask))
		aqt_codec_set_tx_hold(aqt->component, AQT1000_ANA_AMIC2, false);
	if (test_and_clear_bit(ANC_MIC_AMIC3, &aqt->status_mask))
		aqt_codec_set_tx_hold(aqt->component, AQT1000_ANA_AMIC3, false);
}

static const char * const rx_int_dem_inp_mux_text[] = {
	"NORMAL_DSM_OUT", "CLSH_DSM_OUT",
};

static int aqt_int_dem_inp_mux_put(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget =
		snd_soc_dapm_kcontrol_widget(kcontrol);
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(widget->dapm);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int val;
	unsigned short look_ahead_dly_reg = AQT1000_CDC_RX1_RX_PATH_CFG0;

	val = ucontrol->value.enumerated.item[0];
	if (val >= e->items)
		return -EINVAL;

	dev_dbg(component->dev, "%s: wname: %s, val: 0x%x\n", __func__,
		widget->name, val);

	if (e->reg == AQT1000_CDC_RX1_RX_PATH_SEC0)
		look_ahead_dly_reg = AQT1000_CDC_RX1_RX_PATH_CFG0;
	else if (e->reg == AQT1000_CDC_RX2_RX_PATH_SEC0)
		look_ahead_dly_reg = AQT1000_CDC_RX2_RX_PATH_CFG0;

	/* Set Look Ahead Delay */
	snd_soc_component_update_bits(component, look_ahead_dly_reg,
			    0x08, (val ? 0x08 : 0x00));
	/* Set DEM INP Select */
	return snd_soc_dapm_put_enum_double(kcontrol, ucontrol);
}

AQT_DAPM_ENUM_EXT(rx_int1_dem, AQT1000_CDC_RX1_RX_PATH_SEC0, 0,
	rx_int_dem_inp_mux_text, snd_soc_dapm_get_enum_double,
	aqt_int_dem_inp_mux_put);
AQT_DAPM_ENUM_EXT(rx_int2_dem, AQT1000_CDC_RX2_RX_PATH_SEC0, 0,
	rx_int_dem_inp_mux_text, snd_soc_dapm_get_enum_double,
	aqt_int_dem_inp_mux_put);

static int aqt_codec_hphl_dac_event(struct snd_soc_dapm_widget *w,
				      struct snd_kcontrol *kcontrol,
				      int event)
{
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(w->dapm);
	struct aqt1000 *aqt = snd_soc_component_get_drvdata(component);
	int hph_mode = aqt->hph_mode;
	u8 dem_inp;
	int ret = 0;
	uint32_t impedl = 0;
	uint32_t impedr = 0;

	dev_dbg(component->dev, "%s wname: %s event: %d hph_mode: %d\n",
		__func__, w->name, event, hph_mode);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (aqt->anc_func) {
			ret = aqt_codec_enable_anc(w, kcontrol, event);
			/* 40 msec delay is needed to avoid click and pop */
			msleep(40);
		}
		/* Read DEM INP Select */
		dem_inp = snd_soc_component_read32(
				component, AQT1000_CDC_RX1_RX_PATH_SEC0) &
				0x03;
		if (((hph_mode == CLS_H_HIFI) || (hph_mode == CLS_H_LOHIFI) ||
		     (hph_mode == CLS_H_LP)) && (dem_inp != 0x01)) {
			dev_err(component->dev, "%s: DEM Input not set correctly, hph_mode: %d\n",
					__func__, hph_mode);
			return -EINVAL;
		}
		/* Disable AutoChop timer during power up */
		snd_soc_component_update_bits(component,
					AQT1000_HPH_NEW_INT_HPH_TIMER1,
					0x02, 0x00);

		aqt_clsh_fsm(component, &aqt->clsh_d,
			     AQT_CLSH_EVENT_PRE_DAC,
			     AQT_CLSH_STATE_HPHL,
			     hph_mode);

		if (aqt->anc_func)
			snd_soc_component_update_bits(component,
					    AQT1000_CDC_RX1_RX_PATH_CFG0,
					    0x10, 0x10);

		ret = aqt_mbhc_get_impedance(aqt->mbhc,
					       &impedl, &impedr);
		if (!ret) {
			aqt_clsh_imped_config(component, impedl, false);
			set_bit(CLSH_Z_CONFIG, &aqt->status_mask);
		} else {
			dev_dbg(component->dev, "%s: Failed to get mbhc impedance %d\n",
				__func__, ret);
			ret = 0;
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* 1000us required as per HW requirement */
		usleep_range(1000, 1100);
		aqt_clsh_fsm(component, &aqt->clsh_d,
			     AQT_CLSH_EVENT_POST_PA,
			     AQT_CLSH_STATE_HPHL,
			     hph_mode);
		if (test_bit(CLSH_Z_CONFIG, &aqt->status_mask)) {
			aqt_clsh_imped_config(component, impedl, true);
			clear_bit(CLSH_Z_CONFIG, &aqt->status_mask);
		}
		break;
	default:
		break;
	};

	return ret;
}

static int aqt_codec_hphr_dac_event(struct snd_soc_dapm_widget *w,
				      struct snd_kcontrol *kcontrol,
				      int event)
{
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(w->dapm);
	struct aqt1000 *aqt = snd_soc_component_get_drvdata(component);
	int hph_mode = aqt->hph_mode;
	u8 dem_inp;
	int ret = 0;

	dev_dbg(component->dev, "%s wname: %s event: %d hph_mode: %d\n",
		__func__, w->name, event, hph_mode);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (aqt->anc_func) {
			ret = aqt_codec_enable_anc(w, kcontrol, event);
			/* 40 msec delay is needed to avoid click and pop */
			msleep(40);
		}
		/* Read DEM INP Select */
		dem_inp = snd_soc_component_read32(
				component, AQT1000_CDC_RX2_RX_PATH_SEC0) &
				0x03;
		if (((hph_mode == CLS_H_HIFI) || (hph_mode == CLS_H_LOHIFI) ||
		     (hph_mode == CLS_H_LP)) && (dem_inp != 0x01)) {
			dev_err(component->dev, "%s: DEM Input not set correctly, hph_mode: %d\n",
					__func__, hph_mode);
			return -EINVAL;
		}
		/* Disable AutoChop timer during power up */
		snd_soc_component_update_bits(component,
					AQT1000_HPH_NEW_INT_HPH_TIMER1,
					0x02, 0x00);
		aqt_clsh_fsm(component, &aqt->clsh_d,
			     AQT_CLSH_EVENT_PRE_DAC,
			     AQT_CLSH_STATE_HPHR,
			     hph_mode);
		if (aqt->anc_func)
			snd_soc_component_update_bits(component,
					    AQT1000_CDC_RX2_RX_PATH_CFG0,
					    0x10, 0x10);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* 1000us required as per HW requirement */
		usleep_range(1000, 1100);
		aqt_clsh_fsm(component, &aqt->clsh_d,
			     AQT_CLSH_EVENT_POST_PA,
			     AQT_CLSH_STATE_HPHR,
			     hph_mode);
		break;
	default:
		break;
	};

	return 0;
}

static int aqt_codec_enable_hphr_pa(struct snd_soc_dapm_widget *w,
				      struct snd_kcontrol *kcontrol,
				      int event)
{
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(w->dapm);
	struct aqt1000 *aqt = snd_soc_component_get_drvdata(component);
	int ret = 0;

	dev_dbg(component->dev, "%s %s %d\n", __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if ((!(strcmp(w->name, "AQT ANC HPHR PA"))) &&
		    (test_bit(HPH_PA_DELAY, &aqt->status_mask)))
			snd_soc_component_update_bits(component,
					AQT1000_ANA_HPH, 0xC0, 0xC0);

		set_bit(HPH_PA_DELAY, &aqt->status_mask);
		break;
	case SND_SOC_DAPM_POST_PMU:
		if ((!(strcmp(w->name, "AQT ANC HPHR PA")))) {
			if ((snd_soc_component_read32(
					component, AQT1000_ANA_HPH) & 0xC0)
					!= 0xC0)
				/*
				 * If PA_EN is not set (potentially in ANC case)
				 * then do nothing for POST_PMU and let left
				 * channel handle everything.
				 */
				break;
		}
		/*
		 * 7ms sleep is required after PA is enabled as per
		 * HW requirement. If compander is disabled, then
		 * 20ms delay is needed.
		 */
		if (test_bit(HPH_PA_DELAY, &aqt->status_mask)) {
			if (!aqt->comp_enabled[COMPANDER_2])
				usleep_range(20000, 20100);
			else
				usleep_range(7000, 7100);
			clear_bit(HPH_PA_DELAY, &aqt->status_mask);
		}
		if (aqt->anc_func) {
			/* Clear Tx FE HOLD if both PAs are enabled */
			if ((snd_soc_component_read32(
					aqt->component, AQT1000_ANA_HPH) &
					0xC0) == 0xC0)
				aqt_codec_clear_anc_tx_hold(aqt);
		}

		snd_soc_component_update_bits(
				component, AQT1000_HPH_R_TEST, 0x01, 0x01);

		/* Remove mute */
		snd_soc_component_update_bits(
					component, AQT1000_CDC_RX2_RX_PATH_CTL,
					0x10, 0x00);
		/* Enable GM3 boost */
		snd_soc_component_update_bits(
					component, AQT1000_HPH_CNP_WG_CTL,
					0x80, 0x80);
		/* Enable AutoChop timer at the end of power up */
		snd_soc_component_update_bits(component,
					AQT1000_HPH_NEW_INT_HPH_TIMER1,
					0x02, 0x02);
		/* Remove mix path mute if it is enabled */
		if ((snd_soc_component_read32(
				component, AQT1000_CDC_RX2_RX_PATH_MIX_CTL)) &
				0x10)
			snd_soc_component_update_bits(component,
					    AQT1000_CDC_RX2_RX_PATH_MIX_CTL,
					    0x10, 0x00);
		if (!(strcmp(w->name, "AQT ANC HPHR PA"))) {
			dev_dbg(component->dev,
				"%s:Do everything needed for left channel\n",
				__func__);
			/* Do everything needed for left channel */
			snd_soc_component_update_bits(
						component, AQT1000_HPH_L_TEST,
						0x01, 0x01);

			/* Remove mute */
			snd_soc_component_update_bits(component,
						AQT1000_CDC_RX1_RX_PATH_CTL,
						0x10, 0x00);

			/* Remove mix path mute if it is enabled */
			if ((snd_soc_component_read32(component,
					AQT1000_CDC_RX1_RX_PATH_MIX_CTL)) &
					0x10)
				snd_soc_component_update_bits(component,
					AQT1000_CDC_RX1_RX_PATH_MIX_CTL,
					0x10, 0x00);

			/* Remove ANC Rx from reset */
			ret = aqt_codec_enable_anc(w, kcontrol, event);
		}
		aqt_codec_override(component, aqt->hph_mode, event);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		blocking_notifier_call_chain(&aqt->mbhc->notifier,
					     AQT_EVENT_PRE_HPHR_PA_OFF,
					     &aqt->mbhc->wcd_mbhc);
		snd_soc_component_update_bits(component,
					AQT1000_HPH_R_TEST, 0x01, 0x00);
		snd_soc_component_update_bits(component,
					AQT1000_CDC_RX2_RX_PATH_CTL,
					0x10, 0x10);
		snd_soc_component_update_bits(component,
					AQT1000_CDC_RX2_RX_PATH_MIX_CTL,
					0x10, 0x10);
		if (!(strcmp(w->name, "AQT ANC HPHR PA")))
			snd_soc_component_update_bits(component,
					AQT1000_ANA_HPH, 0x40, 0x00);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/*
		 * 5ms sleep is required after PA disable. If compander is
		 * disabled, then 20ms delay is needed after PA disable.
		 */
		if (!aqt->comp_enabled[COMPANDER_2])
			usleep_range(20000, 20100);
		else
			usleep_range(5000, 5100);
		aqt_codec_override(component, aqt->hph_mode, event);
		blocking_notifier_call_chain(&aqt->mbhc->notifier,
					     AQT_EVENT_POST_HPHR_PA_OFF,
					     &aqt->mbhc->wcd_mbhc);
		if (!(strcmp(w->name, "AQT ANC HPHR PA"))) {
			ret = aqt_codec_enable_anc(w, kcontrol, event);
			snd_soc_component_update_bits(component,
					    AQT1000_CDC_RX2_RX_PATH_CFG0,
					    0x10, 0x00);
		}
		break;
	};

	return ret;
}

static int aqt_codec_enable_hphl_pa(struct snd_soc_dapm_widget *w,
				      struct snd_kcontrol *kcontrol,
				      int event)
{
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(w->dapm);
	struct aqt1000 *aqt = snd_soc_component_get_drvdata(component);
	int ret = 0;

	dev_dbg(component->dev, "%s %s %d\n", __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if ((!(strcmp(w->name, "AQT ANC HPHL PA"))) &&
		    (test_bit(HPH_PA_DELAY, &aqt->status_mask)))
			snd_soc_component_update_bits(component,
					AQT1000_ANA_HPH,
					0xC0, 0xC0);
		set_bit(HPH_PA_DELAY, &aqt->status_mask);
		break;
	case SND_SOC_DAPM_POST_PMU:
		if (!(strcmp(w->name, "AQT ANC HPHL PA"))) {
			if ((snd_soc_component_read32(
					component, AQT1000_ANA_HPH) & 0xC0)
								!= 0xC0)
				/*
				 * If PA_EN is not set (potentially in ANC
				 * case) then do nothing for POST_PMU and
				 * let right channel handle everything.
				 */
				break;
		}
		/*
		 * 7ms sleep is required after PA is enabled as per
		 * HW requirement. If compander is disabled, then
		 * 20ms delay is needed.
		 */
		if (test_bit(HPH_PA_DELAY, &aqt->status_mask)) {
			if (!aqt->comp_enabled[COMPANDER_1])
				usleep_range(20000, 20100);
			else
				usleep_range(7000, 7100);
			clear_bit(HPH_PA_DELAY, &aqt->status_mask);
		}
		if (aqt->anc_func) {
			/* Clear Tx FE HOLD if both PAs are enabled */
			if ((snd_soc_component_read32(
					aqt->component, AQT1000_ANA_HPH) &
					0xC0) == 0xC0)
				aqt_codec_clear_anc_tx_hold(aqt);
		}

		snd_soc_component_update_bits(component,
					AQT1000_HPH_L_TEST, 0x01, 0x01);
		/* Remove Mute on primary path */
		snd_soc_component_update_bits(component,
					AQT1000_CDC_RX1_RX_PATH_CTL,
					0x10, 0x00);
		/* Enable GM3 boost */
		snd_soc_component_update_bits(component,
					AQT1000_HPH_CNP_WG_CTL,
					0x80, 0x80);
		/* Enable AutoChop timer at the end of power up */
		snd_soc_component_update_bits(component,
					AQT1000_HPH_NEW_INT_HPH_TIMER1,
					0x02, 0x02);
		/* Remove mix path mute if it is enabled */
		if ((snd_soc_component_read32(component,
				AQT1000_CDC_RX1_RX_PATH_MIX_CTL)) &
				0x10)
			snd_soc_component_update_bits(component,
					    AQT1000_CDC_RX1_RX_PATH_MIX_CTL,
					    0x10, 0x00);
		if (!(strcmp(w->name, "AQT ANC HPHL PA"))) {
			dev_dbg(component->dev,
				"%s:Do everything needed for right channel\n",
				__func__);

			/* Do everything needed for right channel */
			snd_soc_component_update_bits(component,
						AQT1000_HPH_R_TEST,
						0x01, 0x01);

			/* Remove mute */
			snd_soc_component_update_bits(component,
						AQT1000_CDC_RX2_RX_PATH_CTL,
						0x10, 0x00);

			/* Remove mix path mute if it is enabled */
			if ((snd_soc_component_read32(component,
					AQT1000_CDC_RX2_RX_PATH_MIX_CTL)) &
					0x10)
				snd_soc_component_update_bits(component,
						AQT1000_CDC_RX2_RX_PATH_MIX_CTL,
						0x10, 0x00);
			/* Remove ANC Rx from reset */
			ret = aqt_codec_enable_anc(w, kcontrol, event);
		}
		aqt_codec_override(component, aqt->hph_mode, event);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		blocking_notifier_call_chain(&aqt->mbhc->notifier,
					     AQT_EVENT_PRE_HPHL_PA_OFF,
					     &aqt->mbhc->wcd_mbhc);
		snd_soc_component_update_bits(component,
				AQT1000_HPH_L_TEST, 0x01, 0x00);
		snd_soc_component_update_bits(component,
				AQT1000_CDC_RX1_RX_PATH_CTL, 0x10, 0x10);
		snd_soc_component_update_bits(component,
				AQT1000_CDC_RX1_RX_PATH_MIX_CTL, 0x10, 0x10);
		if (!(strcmp(w->name, "AQT ANC HPHL PA")))
			snd_soc_component_update_bits(component,
					AQT1000_ANA_HPH, 0x80, 0x00);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/*
		 * 5ms sleep is required after PA disable. If compander is
		 * disabled, then 20ms delay is needed after PA disable.
		 */
		if (!aqt->comp_enabled[COMPANDER_1])
			usleep_range(20000, 20100);
		else
			usleep_range(5000, 5100);
		aqt_codec_override(component, aqt->hph_mode, event);
		blocking_notifier_call_chain(&aqt->mbhc->notifier,
					     AQT_EVENT_POST_HPHL_PA_OFF,
					     &aqt->mbhc->wcd_mbhc);
		if (!(strcmp(w->name, "AQT ANC HPHL PA"))) {
			ret = aqt_codec_enable_anc(w, kcontrol, event);
			snd_soc_component_update_bits(component,
				AQT1000_CDC_RX1_RX_PATH_CFG0, 0x10, 0x00);
		}
		break;
	};

	return ret;
}

static int aqt_codec_set_iir_gain(struct snd_soc_dapm_widget *w,
				    struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(w->dapm);

	dev_dbg(component->dev, "%s: event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU: /* fall through */
	case SND_SOC_DAPM_PRE_PMD:
		if (strnstr(w->name, "AQT IIR0", sizeof("AQT IIR0"))) {
			snd_soc_component_write(component,
				AQT1000_CDC_SIDETONE_IIR0_IIR_GAIN_B1_CTL,
			snd_soc_component_read32(component,
				AQT1000_CDC_SIDETONE_IIR0_IIR_GAIN_B1_CTL));
			snd_soc_component_write(component,
				AQT1000_CDC_SIDETONE_IIR0_IIR_GAIN_B2_CTL,
			snd_soc_component_read32(component,
				AQT1000_CDC_SIDETONE_IIR0_IIR_GAIN_B2_CTL));
			snd_soc_component_write(component,
				AQT1000_CDC_SIDETONE_IIR0_IIR_GAIN_B3_CTL,
			snd_soc_component_read32(component,
				AQT1000_CDC_SIDETONE_IIR0_IIR_GAIN_B3_CTL));
			snd_soc_component_write(component,
				AQT1000_CDC_SIDETONE_IIR0_IIR_GAIN_B4_CTL,
			snd_soc_component_read32(component,
				AQT1000_CDC_SIDETONE_IIR0_IIR_GAIN_B4_CTL));
		}
		break;
	}
	return 0;
}

static int aqt_enable_native_supply(struct snd_soc_dapm_widget *w,
				      struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(w->dapm);
	struct aqt1000 *aqt = snd_soc_component_get_drvdata(component);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (++aqt->native_clk_users == 1) {
			snd_soc_component_update_bits(component,
					AQT1000_CLK_SYS_PLL_ENABLES,
					0x01, 0x01);
			/* 100usec is needed as per HW requirement */
			usleep_range(100, 120);
			snd_soc_component_update_bits(component,
					AQT1000_CDC_CLK_RST_CTRL_MCLK_CONTROL,
					0x02, 0x02);
			snd_soc_component_update_bits(component,
					AQT1000_CDC_CLK_RST_CTRL_FS_CNT_CONTROL,
					0x10, 0x10);
		}
		break;
	case SND_SOC_DAPM_PRE_PMD:
		if (aqt->native_clk_users &&
		    (--aqt->native_clk_users == 0)) {
			snd_soc_component_update_bits(component,
					AQT1000_CDC_CLK_RST_CTRL_FS_CNT_CONTROL,
					0x10, 0x00);
			snd_soc_component_update_bits(component,
					AQT1000_CDC_CLK_RST_CTRL_MCLK_CONTROL,
					0x02, 0x00);
			snd_soc_component_update_bits(component,
					AQT1000_CLK_SYS_PLL_ENABLES,
					0x01, 0x00);
		}
		break;
	}

	dev_dbg(component->dev, "%s: native_clk_users: %d, event: %d\n",
		__func__, aqt->native_clk_users, event);

	return 0;
}

static const char * const native_mux_text[] = {
	"OFF", "ON",
};

AQT_DAPM_ENUM(int1_1_native, SND_SOC_NOPM, 0, native_mux_text);
AQT_DAPM_ENUM(int2_1_native, SND_SOC_NOPM, 0, native_mux_text);

static int aqt_mclk_event(struct snd_soc_dapm_widget *w,
			  struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
				snd_soc_dapm_to_component(w->dapm);
	int ret = 0;

	dev_dbg(component->dev, "%s: event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		ret = aqt_cdc_mclk_enable(component, true);
		break;
	case SND_SOC_DAPM_POST_PMD:
		ret = aqt_cdc_mclk_enable(component, false);
		break;
	}

	return ret;
}

static int aif_cap_mixer_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int aif_cap_mixer_put(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static const struct snd_kcontrol_new aif1_cap_mixer[] = {
	SOC_SINGLE_EXT("TX0", SND_SOC_NOPM, AQT_TX0, 1, 0,
			aif_cap_mixer_get, aif_cap_mixer_put),
	SOC_SINGLE_EXT("TX1", SND_SOC_NOPM, AQT_TX1, 1, 0,
			aif_cap_mixer_get, aif_cap_mixer_put),
};

static const char * const rx_inp_st_mux_text[] = {
	"ZERO", "SRC0",
};
AQT_DAPM_ENUM(rx_inp_st, AQT1000_CDC_RX_INP_MUX_SIDETONE_SRC_CFG0, 4,
	      rx_inp_st_mux_text);

static const struct snd_soc_dapm_widget aqt_dapm_widgets[] = {

	SND_SOC_DAPM_SUPPLY("AQT MCLK", SND_SOC_NOPM, 0, 0, aqt_mclk_event,
			    SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_AIF_OUT_E("AQT AIF1 CAP", "AQT AIF1 Capture", 0,
		SND_SOC_NOPM, AIF1_CAP, 0, aqt_codec_enable_i2s_tx,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MIXER("AQT AIF1 CAP Mixer", SND_SOC_NOPM, AIF1_CAP, 0,
			   aif1_cap_mixer, ARRAY_SIZE(aif1_cap_mixer)),

	AQT_DAPM_MUX("AQT TX0_MUX", 0, tx0),
	AQT_DAPM_MUX("AQT TX1_MUX", 0, tx1),

	SND_SOC_DAPM_MUX_E("AQT ADC0 MUX", AQT1000_CDC_TX0_TX_PATH_CTL, 5, 0,
		&tx_adc0_mux, aqt_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("AQT ADC1 MUX", AQT1000_CDC_TX1_TX_PATH_CTL, 5, 0,
		&tx_adc1_mux, aqt_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("AQT ADC2 MUX", AQT1000_CDC_TX2_TX_PATH_CTL, 5, 0,
		&tx_adc2_mux, aqt_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	AQT_DAPM_MUX("AQT AMIC0_MUX", 0, tx_amic0),
	AQT_DAPM_MUX("AQT AMIC1_MUX", 0, tx_amic1),
	AQT_DAPM_MUX("AQT AMIC2_MUX", 0, tx_amic2),

	SND_SOC_DAPM_ADC_E("AQT ADC_L", NULL, AQT1000_ANA_AMIC1, 7, 0,
		aqt_codec_enable_adc, SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_ADC_E("AQT ADC_R", NULL, AQT1000_ANA_AMIC2, 7, 0,
		aqt_codec_enable_adc, SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_ADC_E("AQT ADC_V", NULL, AQT1000_ANA_AMIC3, 7, 0,
		aqt_codec_enable_adc, SND_SOC_DAPM_PRE_PMU),

	AQT_DAPM_MUX("AQT AMIC10_MUX", 0, tx_amic10),
	AQT_DAPM_MUX("AQT AMIC11_MUX", 0, tx_amic11),
	AQT_DAPM_MUX("AQT AMIC12_MUX", 0, tx_amic12),
	AQT_DAPM_MUX("AQT AMIC13_MUX", 0, tx_amic13),

	SND_SOC_DAPM_SWITCH_E("AQT ANC OUT HPHL Enable", SND_SOC_NOPM,
		INTERP_HPHL, 0, &anc_hphl_pa_switch, aqt_anc_out_switch_cb,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	 SND_SOC_DAPM_SWITCH_E("AQT ANC OUT HPHR Enable", SND_SOC_NOPM,
		INTERP_HPHR, 0, &anc_hphr_pa_switch, aqt_anc_out_switch_cb,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_MIXER("AQT RX INT1 MIX2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("AQT RX INT2 MIX2", SND_SOC_NOPM, 0, 0, NULL, 0),

	AQT_DAPM_MUX("AQT ANC0 FB MUX", 0, anc0_fb),
	AQT_DAPM_MUX("AQT ANC1 FB MUX", 0, anc1_fb),

	SND_SOC_DAPM_INPUT("AQT AMIC1"),
	SND_SOC_DAPM_INPUT("AQT AMIC2"),
	SND_SOC_DAPM_INPUT("AQT AMIC3"),

	SND_SOC_DAPM_MIXER("AQT I2S_L RX", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("AQT I2S_R RX", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_AIF_IN_E("AQT AIF1 PB", "AQT AIF1 Playback", 0,
		SND_SOC_NOPM, AIF1_PB, 0, aqt_codec_enable_i2s_rx,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("AQT RX INT1_1 MUX", SND_SOC_NOPM, INTERP_HPHL, 0,
		&rx_int1_1_mux, aqt_codec_enable_main_path,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("AQT RX INT2_1 MUX", SND_SOC_NOPM, INTERP_HPHR, 0,
		&rx_int2_1_mux, aqt_codec_enable_main_path,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("AQT RX INT1_2 MUX", SND_SOC_NOPM, INTERP_HPHL, 0,
		&rx_int1_2_mux, aqt_codec_enable_mix_path,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("AQT RX INT2_2 MUX", SND_SOC_NOPM, INTERP_HPHR, 0,
		&rx_int2_2_mux, aqt_codec_enable_mix_path,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMD),

	AQT_DAPM_MUX("AQT RX INT1_1 INTERP", 0, rx_int1_1_interp),
	AQT_DAPM_MUX("AQT RX INT1_2 INTERP", 0, rx_int1_2_interp),
	AQT_DAPM_MUX("AQT RX INT2_1 INTERP", 0, rx_int2_1_interp),
	AQT_DAPM_MUX("AQT RX INT2_2 INTERP", 0, rx_int2_2_interp),

	SND_SOC_DAPM_MIXER("AQT RX INT1 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("AQT RX INT2 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MUX_E("AQT ASRC0 MUX", SND_SOC_NOPM, ASRC0, 0,
		&asrc0_mux, aqt_codec_enable_asrc_resampler,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("AQT ASRC1 MUX", SND_SOC_NOPM, ASRC1, 0,
		&asrc1_mux, aqt_codec_enable_asrc_resampler,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	AQT_DAPM_MUX("AQT RX INT1 DEM MUX", 0, rx_int1_dem),
	AQT_DAPM_MUX("AQT RX INT2 DEM MUX", 0, rx_int2_dem),

	SND_SOC_DAPM_DAC_E("AQT RX INT1 DAC", NULL, AQT1000_ANA_HPH,
		5, 0, aqt_codec_hphl_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("AQT RX INT2 DAC", NULL, AQT1000_ANA_HPH,
		4, 0, aqt_codec_hphr_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA_E("AQT HPHL PA", AQT1000_ANA_HPH, 7, 0, NULL, 0,
		aqt_codec_enable_hphl_pa,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("AQT HPHR PA", AQT1000_ANA_HPH, 6, 0, NULL, 0,
		aqt_codec_enable_hphr_pa,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("AQT ANC HPHL PA", SND_SOC_NOPM, 0, 0, NULL, 0,
		aqt_codec_enable_hphl_pa,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("AQT ANC HPHR PA", SND_SOC_NOPM, 0, 0, NULL, 0,
		aqt_codec_enable_hphr_pa,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_OUTPUT("AQT HPHL"),
	SND_SOC_DAPM_OUTPUT("AQT HPHR"),
	SND_SOC_DAPM_OUTPUT("AQT ANC HPHL"),
	SND_SOC_DAPM_OUTPUT("AQT ANC HPHR"),

	SND_SOC_DAPM_MIXER_E("AQT IIR0", AQT1000_CDC_SIDETONE_IIR0_IIR_PATH_CTL,
		4, 0, NULL, 0, aqt_codec_set_iir_gain,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_MIXER("AQT SRC0",
			AQT1000_CDC_SIDETONE_SRC0_ST_SRC_PATH_CTL,
			4, 0, NULL, 0),

	SND_SOC_DAPM_MICBIAS_E("AQT MIC BIAS1", SND_SOC_NOPM, 0, 0,
		aqt_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY("AQT RX_BIAS", SND_SOC_NOPM, 0, 0,
		aqt_codec_enable_rx_bias,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY("AQT RX INT1 NATIVE SUPPLY", SND_SOC_NOPM,
		INTERP_HPHL, 0, aqt_enable_native_supply,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_SUPPLY("AQT RX INT2 NATIVE SUPPLY", SND_SOC_NOPM,
		INTERP_HPHR, 0, aqt_enable_native_supply,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD),

	AQT_DAPM_MUX("AQT RX INT1_1 NATIVE MUX", 0, int1_1_native),
	AQT_DAPM_MUX("AQT RX INT2_1 NATIVE MUX", 0, int2_1_native),

	SND_SOC_DAPM_MUX("AQT RX ST MUX",
			 AQT1000_CDC_RX_INP_MUX_SIDETONE_SRC_CFG0, 2, 0,
			 &rx_inp_st_mux),
};

static int aqt_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);

	return 0;
}

static void aqt_shutdown(struct snd_pcm_substream *substream,
			   struct snd_soc_dai *dai)
{
	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);
}

static int aqt_set_decimator_rate(struct snd_soc_dai *dai,
				    u32 sample_rate)
{
	struct snd_soc_component *component = dai->component;
	u8 tx_fs_rate = 0;
	u8 tx_mux_sel = 0, tx0_mux_sel = 0, tx1_mux_sel = 0;
	u16 tx_path_ctl_reg = 0;

	switch (sample_rate) {
	case 8000:
		tx_fs_rate = 0;
		break;
	case 16000:
		tx_fs_rate = 1;
		break;
	case 32000:
		tx_fs_rate = 3;
		break;
	case 48000:
		tx_fs_rate = 4;
		break;
	case 96000:
		tx_fs_rate = 5;
		break;
	case 192000:
		tx_fs_rate = 6;
		break;
	default:
		dev_err(component->dev, "%s: Invalid TX sample rate: %d\n",
			__func__, sample_rate);
		return -EINVAL;

	};

	/* Find which decimator path is enabled */
	tx_mux_sel = snd_soc_component_read32(component,
					AQT1000_CDC_IF_ROUTER_TX_MUX_CFG0);
	tx0_mux_sel = (tx_mux_sel & 0x03);
	tx1_mux_sel = (tx_mux_sel & 0xC0);

	if (tx0_mux_sel) {
		tx_path_ctl_reg = AQT1000_CDC_TX0_TX_PATH_CTL +
					((tx0_mux_sel - 1) * 16);
		snd_soc_component_update_bits(component, tx_path_ctl_reg,
					0x0F, tx_fs_rate);
	}

	if (tx1_mux_sel) {
		tx_path_ctl_reg = AQT1000_CDC_TX0_TX_PATH_CTL +
					((tx1_mux_sel - 1) * 16);
		snd_soc_component_update_bits(component, tx_path_ctl_reg,
					0x0F, tx_fs_rate);
	}

	return 0;
}

static int aqt_set_interpolator_rate(struct snd_soc_dai *dai,
				       u32 sample_rate)
{
	struct snd_soc_component *component = dai->component;
	int rate_val = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(sr_val_tbl); i++) {
		if (sample_rate == sr_val_tbl[i].sample_rate) {
			rate_val = sr_val_tbl[i].rate_val;
			break;
		}
	}
	if ((i == ARRAY_SIZE(sr_val_tbl)) || (rate_val < 0)) {
		dev_err(component->dev, "%s: Unsupported sample rate: %d\n",
			__func__, sample_rate);
		return -EINVAL;
	}

	/* TODO - Set the rate only to enabled path */
	/* Set Primary interpolator rate */
	snd_soc_component_update_bits(component, AQT1000_CDC_RX1_RX_PATH_CTL,
			    0x0F, (u8)rate_val);
	snd_soc_component_update_bits(component, AQT1000_CDC_RX2_RX_PATH_CTL,
			    0x0F, (u8)rate_val);

	/* Set mixing path interpolator rate */
	snd_soc_component_update_bits(component,
			AQT1000_CDC_RX1_RX_PATH_MIX_CTL,
			0x0F, (u8)rate_val);
	snd_soc_component_update_bits(component,
			AQT1000_CDC_RX2_RX_PATH_MIX_CTL,
			0x0F, (u8)rate_val);

	return 0;
}

static int aqt_prepare(struct snd_pcm_substream *substream,
			 struct snd_soc_dai *dai)
{
	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);
	return 0;
}

static int aqt_hw_params(struct snd_pcm_substream *substream,
			   struct snd_pcm_hw_params *params,
			   struct snd_soc_dai *dai)
{
	struct aqt1000 *aqt = snd_soc_component_get_drvdata(dai->component);
	int ret = 0;

	dev_dbg(aqt->dev, "%s: dai_name = %s DAI-ID %x rate %d num_ch %d\n",
		 __func__, dai->name, dai->id, params_rate(params),
		 params_channels(params));

	switch (substream->stream) {
	case SNDRV_PCM_STREAM_PLAYBACK:
		ret = aqt_set_interpolator_rate(dai, params_rate(params));
		if (ret) {
			dev_err(aqt->dev, "%s: cannot set sample rate: %u\n",
				__func__, params_rate(params));
			return ret;
		}
		switch (params_width(params)) {
		case 16:
			aqt->dai[dai->id].bit_width = 16;
			break;
		case 24:
			aqt->dai[dai->id].bit_width = 24;
			break;
		case 32:
			aqt->dai[dai->id].bit_width = 32;
			break;
		default:
			return -EINVAL;
		}
		aqt->dai[dai->id].rate = params_rate(params);
		break;
	case SNDRV_PCM_STREAM_CAPTURE:
		ret = aqt_set_decimator_rate(dai, params_rate(params));
		if (ret) {
			dev_err(aqt->dev,
				"%s: cannot set TX Decimator rate: %d\n",
				__func__, ret);
			return ret;
		}
		switch (params_width(params)) {
		case 16:
			aqt->dai[dai->id].bit_width = 16;
			break;
		case 24:
			aqt->dai[dai->id].bit_width = 24;
			break;
		default:
			dev_err(aqt->dev, "%s: Invalid format 0x%x\n",
				__func__, params_width(params));
			return -EINVAL;
		};
		aqt->dai[dai->id].rate = params_rate(params);
		break;
	default:
		dev_err(aqt->dev, "%s: Invalid stream type %d\n", __func__,
			substream->stream);
		return -EINVAL;
	};

	return 0;
}

static struct snd_soc_dai_ops aqt_dai_ops = {
	.startup = aqt_startup,
	.shutdown = aqt_shutdown,
	.hw_params = aqt_hw_params,
	.prepare = aqt_prepare,
};

struct snd_soc_dai_driver aqt_dai[] = {
	{
		.name = "aqt_rx1",
		.id = AIF1_PB,
		.playback = {
			.stream_name = "AQT AIF1 Playback",
			.rates = AQT1000_RATES_MASK | AQT1000_FRAC_RATES_MASK,
			.formats = AQT1000_FORMATS_S16_S24_S32_LE,
			.rate_min = 8000,
			.rate_max = 384000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &aqt_dai_ops,
	},
	{
		.name = "aqt_tx1",
		.id = AIF1_CAP,
		.capture = {
			.stream_name = "AQT AIF1 Capture",
			.rates = AQT1000_RATES_MASK,
			.formats = AQT1000_FORMATS_S16_S24_LE,
			.rate_min = 8000,
			.rate_max = 192000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &aqt_dai_ops,
	},
};

static int aqt_enable_mclk(struct aqt1000 *aqt)
{
	struct snd_soc_component *component = aqt->component;

	/* Enable mclk requires master bias to be enabled first */
	if (aqt->master_bias_users <= 0) {
		dev_err(aqt->dev,
			"%s: Cannot turn on MCLK, BG is not enabled\n",
			__func__);
		return -EINVAL;
	}

	if (++aqt->mclk_users == 1) {
		/* Set clock div 2 */
		snd_soc_component_update_bits(component,
				AQT1000_CLK_SYS_MCLK1_PRG, 0x0C, 0x04);
		snd_soc_component_update_bits(component,
				AQT1000_CLK_SYS_MCLK1_PRG, 0x10, 0x10);
		snd_soc_component_update_bits(component,
				AQT1000_CDC_CLK_RST_CTRL_FS_CNT_CONTROL,
				0x01, 0x01);
		snd_soc_component_update_bits(component,
				AQT1000_CDC_CLK_RST_CTRL_MCLK_CONTROL,
				0x01, 0x01);
		/*
		 * 10us sleep is required after clock is enabled
		 * as per HW requirement
		 */
		usleep_range(10, 15);
	}

	dev_dbg(aqt->dev, "%s: mclk_users: %d\n", __func__, aqt->mclk_users);

	return 0;
}

static int aqt_disable_mclk(struct aqt1000 *aqt)
{
	struct snd_soc_component *component = aqt->component;

	if (aqt->mclk_users <= 0) {
		dev_err(aqt->dev, "%s: No mclk users, cannot disable mclk\n",
			__func__);
		return -EINVAL;
	}

	if (--aqt->mclk_users == 0) {
		snd_soc_component_update_bits(component,
				AQT1000_CDC_CLK_RST_CTRL_MCLK_CONTROL,
				0x01, 0x00);
		snd_soc_component_update_bits(component,
				AQT1000_CDC_CLK_RST_CTRL_FS_CNT_CONTROL,
				0x01, 0x00);
		snd_soc_component_update_bits(component,
				AQT1000_CLK_SYS_MCLK1_PRG, 0x10, 0x00);
	}

	dev_dbg(component->dev, "%s: mclk_users: %d\n", __func__,
		aqt->mclk_users);

	return 0;
}

static int aqt_enable_master_bias(struct aqt1000 *aqt)
{
	struct snd_soc_component *component = aqt->component;

	mutex_lock(&aqt->master_bias_lock);

	aqt->master_bias_users++;
	if (aqt->master_bias_users == 1) {
		snd_soc_component_update_bits(component, AQT1000_ANA_BIAS,
					      0x80, 0x80);
		snd_soc_component_update_bits(component, AQT1000_ANA_BIAS,
					      0x40, 0x40);
		/*
		 * 1ms delay is required after pre-charge is enabled
		 * as per HW requirement
		 */
		usleep_range(1000, 1100);
		snd_soc_component_update_bits(component, AQT1000_ANA_BIAS,
					      0x40, 0x00);
	}

	mutex_unlock(&aqt->master_bias_lock);

	return 0;
}

static int aqt_disable_master_bias(struct aqt1000 *aqt)
{
	struct snd_soc_component *component = aqt->component;

	mutex_lock(&aqt->master_bias_lock);
	if (aqt->master_bias_users <= 0) {
		mutex_unlock(&aqt->master_bias_lock);
		return -EINVAL;
	}

	aqt->master_bias_users--;
	if (aqt->master_bias_users == 0)
		snd_soc_component_update_bits(component, AQT1000_ANA_BIAS,
					      0x80, 0x00);
	mutex_unlock(&aqt->master_bias_lock);

	return 0;
}

static int aqt_cdc_req_mclk_enable(struct aqt1000 *aqt,
				     bool enable)
{
	int ret = 0;

	if (enable) {
		ret = clk_prepare_enable(aqt->ext_clk);
		if (ret) {
			dev_err(aqt->dev, "%s: ext clk enable failed\n",
				__func__);
			goto done;
		}
		/* Get BG */
		aqt_enable_master_bias(aqt);
		/* Get MCLK */
		aqt_enable_mclk(aqt);
	} else {
		/* put MCLK */
		aqt_disable_mclk(aqt);
		/* put BG */
		if (aqt_disable_master_bias(aqt))
			dev_err(aqt->dev, "%s: master bias disable failed\n",
				__func__);
		clk_disable_unprepare(aqt->ext_clk);
	}

done:
	return ret;
}

static int __aqt_cdc_mclk_enable_locked(struct aqt1000 *aqt,
					  bool enable)
{
	int ret = 0;

	dev_dbg(aqt->dev, "%s: mclk_enable = %u\n", __func__, enable);

	if (enable)
		ret = aqt_cdc_req_mclk_enable(aqt, true);
	else
		aqt_cdc_req_mclk_enable(aqt, false);

	return ret;
}

static int __aqt_cdc_mclk_enable(struct aqt1000 *aqt,
				   bool enable)
{
	int ret;

	mutex_lock(&aqt->cdc_bg_clk_lock);
	ret = __aqt_cdc_mclk_enable_locked(aqt, enable);
	mutex_unlock(&aqt->cdc_bg_clk_lock);

	return ret;
}

/**
 * aqt_cdc_mclk_enable - Enable/disable codec mclk
 *
 * @component: codec component instance
 * @enable: Indicates clk enable or disable
 *
 * Returns 0 on Success and error on failure
 */
int aqt_cdc_mclk_enable(struct snd_soc_component *component, bool enable)
{
	struct aqt1000 *aqt = snd_soc_component_get_drvdata(component);

	return __aqt_cdc_mclk_enable(aqt, enable);
}
EXPORT_SYMBOL(aqt_cdc_mclk_enable);

/*
 * aqt_get_micb_vout_ctl_val: converts micbias from volts to register value
 * @micb_mv: micbias in mv
 *
 * return register value converted
 */
int aqt_get_micb_vout_ctl_val(u32 micb_mv)
{
	/* min micbias voltage is 1V and maximum is 2.85V */
	if (micb_mv < 1000 || micb_mv > 2850) {
		pr_err("%s: unsupported micbias voltage\n", __func__);
		return -EINVAL;
	}

	return (micb_mv - 1000) / 50;
}
EXPORT_SYMBOL(aqt_get_micb_vout_ctl_val);

static int aqt_set_micbias(struct aqt1000 *aqt,
			   struct aqt1000_pdata *pdata)
{
	struct snd_soc_component *component = aqt->component;
	int vout_ctl_1;

	if (!pdata) {
		dev_err(component->dev, "%s: NULL pdata\n", __func__);
		return -ENODEV;
	}

	/* set micbias voltage */
	vout_ctl_1 = aqt_get_micb_vout_ctl_val(pdata->micbias.micb1_mv);
	if (vout_ctl_1 < 0)
		return -EINVAL;

	snd_soc_component_update_bits(component, AQT1000_ANA_MICB1,
				      0x3F, vout_ctl_1);

	return 0;
}

static ssize_t aqt_codec_version_read(struct snd_info_entry *entry,
					void *file_private_data,
					struct file *file,
					char __user *buf, size_t count,
					loff_t pos)
{
	char buffer[AQT_VERSION_ENTRY_SIZE];
	int len = 0;

	len = snprintf(buffer, sizeof(buffer), "AQT1000_1_0\n");

	return simple_read_from_buffer(buf, count, &pos, buffer, len);
}

static struct snd_info_entry_ops aqt_codec_info_ops = {
	.read = aqt_codec_version_read,
};

/*
 * aqt_codec_info_create_codec_entry - creates aqt1000 module
 * @codec_root: The parent directory
 * @component: Codec component instance
 *
 * Creates aqt1000 module and version entry under the given
 * parent directory.
 *
 * Return: 0 on success or negative error code on failure.
 */
int aqt_codec_info_create_codec_entry(struct snd_info_entry *codec_root,
				struct snd_soc_component *component)
{
	struct snd_info_entry *version_entry;
	struct aqt1000 *aqt;
	struct snd_soc_card *card;

	if (!codec_root || !component)
		return -EINVAL;

	aqt = snd_soc_component_get_drvdata(component);
	if (!aqt) {
		dev_dbg(component->dev, "%s: aqt is NULL\n", __func__);
		return -EINVAL;
	}
	card = component->card;
	aqt->entry = snd_info_create_subdir(codec_root->module,
					   "aqt1000", codec_root);
	if (!aqt->entry) {
		dev_dbg(component->dev, "%s: failed to create aqt1000 entry\n",
			__func__);
		return -ENOMEM;
	}

	version_entry = snd_info_create_card_entry(card->snd_card,
						  "version",
						   aqt->entry);
	if (!version_entry) {
		dev_dbg(component->dev, "%s: failed to create aqt1000 version entry\n",
			__func__);
		return -ENOMEM;
	}

	version_entry->private_data = aqt;
	version_entry->size = AQT_VERSION_ENTRY_SIZE;
	version_entry->content = SNDRV_INFO_CONTENT_DATA;
	version_entry->c.ops = &aqt_codec_info_ops;

	if (snd_info_register(version_entry) < 0) {
		snd_info_free_entry(version_entry);
		return -ENOMEM;
	}
	aqt->version_entry = version_entry;

	return 0;
}
EXPORT_SYMBOL(aqt_codec_info_create_codec_entry);

static const struct aqt_reg_mask_val aqt_codec_reg_init[] = {
	{AQT1000_CHIP_CFG0_EFUSE_CTL, 0x01, 0x01},
};

static const struct aqt_reg_mask_val aqt_codec_reg_update[] = {
	{AQT1000_LDOH_MODE, 0x1F, 0x0B},
	{AQT1000_MICB1_TEST_CTL_2, 0x07, 0x01},
	{AQT1000_MICB1_MISC_MICB1_INM_RES_BIAS, 0x03, 0x02},
	{AQT1000_MICB1_MISC_MICB1_INM_RES_BIAS, 0x0C, 0x08},
	{AQT1000_MICB1_MISC_MICB1_INM_RES_BIAS, 0x30, 0x20},
	{AQT1000_CDC_TX0_TX_PATH_CFG1, 0x01, 0x00},
	{AQT1000_CDC_TX1_TX_PATH_CFG1, 0x01, 0x00},
	{AQT1000_CDC_TX2_TX_PATH_CFG1, 0x01, 0x00},
};

static void aqt_codec_init_reg(struct aqt1000 *priv)
{
	struct snd_soc_component *component = priv->component;
	u32 i;

	for (i = 0; i < ARRAY_SIZE(aqt_codec_reg_init); i++)
		snd_soc_component_update_bits(component,
				    aqt_codec_reg_init[i].reg,
				    aqt_codec_reg_init[i].mask,
				    aqt_codec_reg_init[i].val);
}

static void aqt_codec_update_reg(struct aqt1000 *priv)
{
	struct snd_soc_component *component = priv->component;
	u32 i;

	for (i = 0; i < ARRAY_SIZE(aqt_codec_reg_update); i++)
		snd_soc_component_update_bits(component,
				    aqt_codec_reg_update[i].reg,
				    aqt_codec_reg_update[i].mask,
				    aqt_codec_reg_update[i].val);

}

static int aqt_soc_codec_probe(struct snd_soc_component *component)
{
	struct aqt1000 *aqt;
	struct aqt1000_pdata *pdata;
	struct snd_soc_dapm_context *dapm =
				snd_soc_component_get_dapm(component);
	int i, ret = 0;

	dev_dbg(component->dev, "%s()\n", __func__);
	aqt = snd_soc_component_get_drvdata(component);

	snd_soc_component_init_regmap(component, aqt->regmap);

	mutex_init(&aqt->codec_mutex);
	mutex_init(&aqt->i2s_lock);
	/* Class-H Init */
	aqt_clsh_init(&aqt->clsh_d);
	/* Default HPH Mode to Class-H Low HiFi */
	aqt->hph_mode = CLS_H_LOHIFI;

	aqt->fw_data = devm_kzalloc(component->dev, sizeof(*(aqt->fw_data)),
				      GFP_KERNEL);
	if (!aqt->fw_data)
		goto err;

	set_bit(WCD9XXX_ANC_CAL, aqt->fw_data->cal_bit);
	set_bit(WCD9XXX_MBHC_CAL, aqt->fw_data->cal_bit);

	/* Register for Clock */
	aqt->ext_clk = clk_get(aqt->dev, "aqt_clk");
	if (IS_ERR(aqt->ext_clk)) {
		dev_err(aqt->dev, "%s: clk get %s failed\n",
			__func__, "aqt_ext_clk");
		goto err_clk;
	}

	ret = wcd_cal_create_hwdep(aqt->fw_data,
				   AQT1000_CODEC_HWDEP_NODE, component);
	if (ret < 0) {
		dev_err(component->dev, "%s hwdep failed %d\n", __func__, ret);
		goto err_hwdep;
	}

	/* Initialize MBHC module */
	ret = aqt_mbhc_init(&aqt->mbhc, component, aqt->fw_data);
	if (ret) {
		pr_err("%s: mbhc initialization failed\n", __func__);
		goto err_hwdep;
	}
	aqt->component = component;
	for (i = 0; i < COMPANDER_MAX; i++)
		aqt->comp_enabled[i] = 0;

	aqt_cdc_mclk_enable(component, true);
	aqt_codec_init_reg(aqt);
	aqt_cdc_mclk_enable(component, false);

	/* Add 100usec delay as per HW requirement */
	usleep_range(100, 110);

	aqt_codec_update_reg(aqt);

	pdata = dev_get_platdata(component->dev);

	/* If 1.8v is supplied externally, then disable internal 1.8v supply */
	for (i = 0; i < pdata->num_supplies; i++) {
		if (!strcmp(pdata->regulator->name, "aqt_vdd1p8")) {
			snd_soc_component_update_bits(component,
					AQT1000_BUCK_5V_EN_CTL,
					0x03, 0x00);
			dev_dbg(component->dev, "%s: Disabled internal supply\n",
				__func__);
			break;
		}
	}

	aqt_set_micbias(aqt, pdata);

	snd_soc_dapm_add_routes(dapm, aqt_audio_map,
			ARRAY_SIZE(aqt_audio_map));

	for (i = 0; i < NUM_CODEC_DAIS; i++) {
		INIT_LIST_HEAD(&aqt->dai[i].ch_list);
		init_waitqueue_head(&aqt->dai[i].dai_wait);
	}

	for (i = 0; i < AQT1000_NUM_DECIMATORS; i++) {
		aqt->tx_hpf_work[i].aqt = aqt;
		aqt->tx_hpf_work[i].decimator = i;
		INIT_DELAYED_WORK(&aqt->tx_hpf_work[i].dwork,
				  aqt_tx_hpf_corner_freq_callback);

		aqt->tx_mute_dwork[i].aqt = aqt;
		aqt->tx_mute_dwork[i].decimator = i;
		INIT_DELAYED_WORK(&aqt->tx_mute_dwork[i].dwork,
				  aqt_tx_mute_update_callback);
	}

	mutex_lock(&aqt->codec_mutex);
	snd_soc_dapm_disable_pin(dapm, "AQT ANC HPHL PA");
	snd_soc_dapm_disable_pin(dapm, "AQT ANC HPHR PA");
	snd_soc_dapm_disable_pin(dapm, "AQT ANC HPHL");
	snd_soc_dapm_disable_pin(dapm, "AQT ANC HPHR");
	mutex_unlock(&aqt->codec_mutex);

	snd_soc_dapm_ignore_suspend(dapm, "AQT AIF1 Playback");
	snd_soc_dapm_ignore_suspend(dapm, "AQT AIF1 Capture");

	snd_soc_dapm_sync(dapm);

	return ret;

err_hwdep:
	clk_put(aqt->ext_clk);
err_clk:
	devm_kfree(component->dev, aqt->fw_data);
	aqt->fw_data = NULL;
err:
	mutex_destroy(&aqt->i2s_lock);
	mutex_destroy(&aqt->codec_mutex);
	return ret;
}

static void aqt_soc_codec_remove(struct snd_soc_component *component)
{
	struct aqt1000 *aqt = snd_soc_component_get_drvdata(component);

	/* Deinitialize MBHC module */
	aqt_mbhc_deinit(component);
	aqt->mbhc = NULL;
	mutex_destroy(&aqt->i2s_lock);
	mutex_destroy(&aqt->codec_mutex);
	clk_put(aqt->ext_clk);

	return;
}

static const struct snd_soc_component_driver snd_cdc_dev_aqt = {
	.name = DRV_NAME,
	.probe = aqt_soc_codec_probe,
	.remove = aqt_soc_codec_remove,
	.controls = aqt_snd_controls,
	.num_controls = ARRAY_SIZE(aqt_snd_controls),
	.dapm_widgets = aqt_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(aqt_dapm_widgets),
	.dapm_routes = aqt_audio_map,
	.num_dapm_routes = ARRAY_SIZE(aqt_audio_map),
};

/*
 * aqt_register_codec: Register the device to ASoC
 * @dev: device
 *
 * return 0 success or error code in case of failure
 */
int aqt_register_codec(struct device *dev)
{
	return snd_soc_register_component(dev, &snd_cdc_dev_aqt, aqt_dai,
					ARRAY_SIZE(aqt_dai));
}
EXPORT_SYMBOL(aqt_register_codec);
