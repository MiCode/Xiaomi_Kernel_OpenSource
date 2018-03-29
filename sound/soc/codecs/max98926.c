/*
 * max98926.c -- ALSA SoC Stereo max98926 driver
 * Copyright 2013-15 Maxim Integrated Products
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <linux/dma-mapping.h>
#include <linux/regulator/consumer.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include "max98926.h"
#define MAX98926_I2C_Channel (0)

static struct i2c_client *new_client;

static int max98926_WriteReg(u16 a_u2Addr, u16 a_u2Data);
static int max98926_ReadReg(u16 a_u2Addr, unsigned short *a_pu2Result);
static void max98926_set_sense_data(struct max98926_priv *max98926);

/*
extern int mtk_i2c_transfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num,
					u32 ext_flag, u32 timing);
*/
/*
static int max98926_regulator_config(struct i2c_client *i2c, bool pullup, bool on);
*/


static const char *const dai_text[] = {
	"Left", "Right", "LeftRight", "LeftRightDiv2",
};

static const char *const dai_input_text[] = {
	"Pcm", "Analog",
};

static const char *const pdm_1_text[] = {
	"Current", "Voltage",
};

static const char *const pdm_0_text[] = {
	"Current", "Voltage",
};

static const char *const max98926_boost_voltage_text[] = {
	"8.5V", "8.25V", "8.0V", "7.75V", "7.5V", "7.25V", "7.0V", "6.75V",
	"6.5V", "6.5V", "6.5V", "6.5V", "6.5V", "6.5V", "6.5V", "6.5V"
};

static SOC_ENUM_SINGLE_DECL(max98926_boost_voltage,
			    max98926_CONFIGURATION, M98926_BST_VOUT_SHIFT, max98926_boost_voltage_text);

static const char *const hpf_text[] = {
	"Disable", "DC Block", "100Hz", "200Hz", "400Hz", "800Hz",
};

static int max98926_spk_zcd_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int max98926_spk_zcd_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec =  component->codec;
	int ret;

	ret = snd_soc_read(codec,
			   max98926_DAI_CLK_DIV_N_LSBS);
	pr_err("%s 0x1f read 0x%x\n", __func__, ret);
	return 1;
}

/*
static int reg_set_optimum_mode_check(struct regulator *reg, int load_uA)
{
	return (regulator_count_voltages(reg) > 0) ?
	       regulator_set_optimum_mode(reg, load_uA) : 0;
}
*/

/*
static int max98926_regulator_config(struct i2c_client *i2c, bool pullup, bool on)
{
	struct regulator *max98926_vcc_i2c;
	int rc;
#define VCC_I2C_MIN_UV  1800000
#define VCC_I2C_MAX_UV  1800000
#define I2C_LOAD_UA     300000

	pr_info("%s: enter\n", __func__);

	if (pullup) {
		pr_info("%s: I2C PULL UP.\n", __func__);

		max98926_vcc_i2c = regulator_get(&i2c->dev, "vcc_i2c");
		if (IS_ERR(max98926_vcc_i2c)) {
			rc = PTR_ERR(max98926_vcc_i2c);
			pr_info("%s: regulator get failed rc=%d\n", __func__, rc);
			goto error_get_vtg_i2c;
		}
		if (regulator_count_voltages(max98926_vcc_i2c) > 0) {
			rc = regulator_set_voltage(max98926_vcc_i2c,
						   VCC_I2C_MIN_UV, VCC_I2C_MAX_UV);
			if (rc) {
				pr_info("%s: regulator set_vtg failed rc=%d\n", __func__, rc);
				goto error_set_vtg_i2c;
			}
		}

		rc = reg_set_optimum_mode_check(max98926_vcc_i2c, I2C_LOAD_UA);
		if (rc < 0) {
			pr_info("%s: regulator vcc_i2c set_opt failed rc=%d\n", __func__, rc);
			goto error_reg_opt_i2c;
		}

		rc = regulator_enable(max98926_vcc_i2c);
		if (rc) {
			pr_info("%s: regulator vcc_i2c enable failed rc=%d\n", __func__, rc);
			goto error_reg_en_vcc_i2c;
		}

	}

	return 0;

error_set_vtg_i2c:
	regulator_put(max98926_vcc_i2c);
error_get_vtg_i2c:
	if (regulator_count_voltages(max98926_vcc_i2c) > 0)
		regulator_set_voltage(max98926_vcc_i2c, 0, VCC_I2C_MAX_UV);
error_reg_en_vcc_i2c:
	if (pullup)
		reg_set_optimum_mode_check(max98926_vcc_i2c, 0);
error_reg_opt_i2c:
	regulator_disable(max98926_vcc_i2c);

	return rc;
}
*/

#if 0
static struct reg_default max98926_reg[] = {
	{ 0x0B, 0x00 }, /* IRQ Enable0 */
	{ 0x0C, 0x00 }, /* IRQ Enable1 */
	{ 0x0D, 0x00 }, /* IRQ Enable2 */
	{ 0x0E, 0x00 }, /* IRQ Clear0 */
	{ 0x0F, 0x00 }, /* IRQ Clear1 */
	{ 0x10, 0x00 }, /* IRQ Clear2 */
	{ 0x11, 0xC0 }, /* Map0 */
	{ 0x12, 0x00 }, /* Map1 */
	{ 0x13, 0x00 }, /* Map2 */
	{ 0x14, 0xF0 }, /* Map3 */
	{ 0x15, 0x00 }, /* Map4 */
	{ 0x16, 0xAB }, /* Map5 */
	{ 0x17, 0x89 }, /* Map6 */
	{ 0x18, 0x00 }, /* Map7 */
	{ 0x19, 0x00 }, /* Map8 */
	{ 0x1A, 0x06 }, /* DAI Clock Mode 1 */
	{ 0x1B, 0xC0 }, /* DAI Clock Mode 2 */
	{ 0x1C, 0x00 }, /* DAI Clock Divider Denominator MSBs */
	{ 0x1D, 0x00 }, /* DAI Clock Divider Denominator LSBs */
	{ 0x1E, 0xF0 }, /* DAI Clock Divider Numerator MSBs */
	{ 0x1F, 0x00 }, /* DAI Clock Divider Numerator LSBs */
	{ 0x20, 0x50 }, /* Format */
	{ 0x21, 0x00 }, /* TDM Slot Select */
	{ 0x22, 0x00 }, /* DOUT Configuration VMON */
	{ 0x23, 0x00 }, /* DOUT Configuration IMON */
	{ 0x24, 0x00 }, /* DOUT Configuration VBAT */
	{ 0x25, 0x00 }, /* DOUT Configuration VBST */
	{ 0x26, 0x00 }, /* DOUT Configuration FLAG */
	{ 0x27, 0xFF }, /* DOUT HiZ Configuration 1 */
	{ 0x28, 0xFF }, /* DOUT HiZ Configuration 2 */
	{ 0x29, 0xFF }, /* DOUT HiZ Configuration 3 */
	{ 0x2A, 0xFF }, /* DOUT HiZ Configuration 4 */
	{ 0x2B, 0x02 }, /* DOUT Drive Strength */
	{ 0x2C, 0x90 }, /* Filters */
	{ 0x2D, 0x00 }, /* Gain */
	{ 0x2E, 0x02 }, /* Gain Ramping */
	{ 0x2F, 0x00 }, /* Speaker Amplifier */
	{ 0x30, 0x0A }, /* Threshold */
	{ 0x31, 0x00 }, /* ALC Attack */
	{ 0x32, 0x80 }, /* ALC Atten and Release */
	{ 0x33, 0x00 }, /* ALC Infinite Hold Release */
	{ 0x34, 0x92 }, /* ALC Configuration */
	{ 0x35, 0x01 }, /* Boost Converter */
	{ 0x36, 0x00 }, /* Block Enable */
	{ 0x37, 0x00 }, /* Configuration */
	{ 0x38, 0x00 }, /* Global Enable */
	{ 0x3A, 0x00 }, /* Boost Limiter */
};
#endif

static const struct soc_enum max98926_dai_enum =
	SOC_ENUM_SINGLE(max98926_GAIN, 5, ARRAY_SIZE(dai_text), dai_text);

static const struct soc_enum max98926_dai_input_enum =
	SOC_ENUM_SINGLE(max98926_SPK_AMP, 1, ARRAY_SIZE(dai_input_text), dai_input_text);

static const struct soc_enum max98926_pdm_1_enum =
	SOC_ENUM_SINGLE(max98926_DAI_CLK_DIV_N_LSBS, 4, ARRAY_SIZE(pdm_1_text), pdm_1_text);

static const struct soc_enum max98926_pdm_0_enum =
	SOC_ENUM_SINGLE(max98926_DAI_CLK_DIV_N_LSBS, 0, ARRAY_SIZE(pdm_0_text), pdm_0_text);

static const struct soc_enum max98926_hpf_enum =
	SOC_ENUM_SINGLE(max98926_FILTERS, 0, ARRAY_SIZE(hpf_text), hpf_text);

static const struct snd_kcontrol_new max98926_hpf_sel_mux =
	SOC_DAPM_ENUM("Rc Filter MUX Mux", max98926_hpf_enum);

static const struct snd_kcontrol_new max98926_dai_sel_mux =
	SOC_DAPM_ENUM("DAI IN MUX Mux", max98926_dai_enum);

static const struct snd_kcontrol_new max98926_input_sel_mux =
	SOC_DAPM_ENUM("INPUT SEL MUX Mux", max98926_dai_input_enum);

static const struct snd_kcontrol_new max98926_pdm_1_sel_mux =
	SOC_DAPM_ENUM("PDM CHANNEL_1 MUX Mux", max98926_pdm_1_enum);

static const struct snd_kcontrol_new max98926_pdm_0_sel_mux =
	SOC_DAPM_ENUM("PDM CHANNEL_0 MUX Mux", max98926_pdm_0_enum);

static int max98926_get_switch_mixer(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int max98926_put_switch_mixer(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	const struct snd_soc_dapm_widget *widget = component->dapm_widgets;

	if (ucontrol->value.integer.value[0])
		snd_soc_dapm_mixer_update_power(widget->dapm, kcontrol, 1, NULL);
	else
		snd_soc_dapm_mixer_update_power(widget->dapm, kcontrol, 0 , NULL);
	return 1;
}

static const struct snd_kcontrol_new  max98926_pdm_path_1_control =
	SOC_SINGLE_EXT("Switch", SND_SOC_NOPM, 0, 1, 0, max98926_get_switch_mixer, max98926_put_switch_mixer);

static const struct snd_kcontrol_new  max98926_pdm_path_0_control =
	SOC_SINGLE_EXT("Switch", SND_SOC_NOPM, 0, 1, 0, max98926_get_switch_mixer, max98926_put_switch_mixer);

static int max98926_dac_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec,
				    max98926_BLOCK_ENABLE,
				    M98926_BST_EN_MASK |
				    M98926_ADC_IMON_EN_MASK | M98926_ADC_VMON_EN_MASK,
				    M98926_BST_EN_MASK |
				    M98926_ADC_IMON_EN_MASK | M98926_ADC_VMON_EN_MASK);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec,
				    max98926_BLOCK_ENABLE, M98926_BST_EN_MASK |
				    M98926_ADC_IMON_EN_MASK | M98926_ADC_VMON_EN_MASK, 0);
		break;
	default:
		return 0;
	}
	return 0;
}

static int pdm_enable_channel_ev(struct snd_soc_dapm_widget *w,
				 struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	int ret;

	ret = snd_soc_read(codec,
			   max98926_DAI_CLK_DIV_N_LSBS);
	pr_err("%s 0x1f read 0x%x\n", __func__, ret);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* enable current measurement */
		snd_soc_update_bits(codec,
				    max98926_DAI_CLK_DIV_N_LSBS,
				    max98926_PDM_CURRENT_MASK, max98926_PDM_CURRENT_MASK);
		/* enable voltage measurement */
		snd_soc_update_bits(codec,
				    max98926_DAI_CLK_DIV_N_LSBS,
				    max98926_PDM_VOLTAGE_MASK, max98926_PDM_VOLTAGE_MASK);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec,
				    max98926_DAI_CLK_DIV_N_LSBS,
				    max98926_PDM_CURRENT_MASK, 0);
		snd_soc_update_bits(codec,
				    max98926_DAI_CLK_DIV_N_LSBS,
				    max98926_PDM_VOLTAGE_MASK, 0);
		break;
	}
	ret = snd_soc_read(codec,
			   max98926_DAI_CLK_DIV_N_LSBS);
	pr_err("%s 0x1f read 0x%x\n", __func__, ret);
	return 0;
}

static const struct snd_soc_dapm_widget max98926_dapm_widgets[] = {
	SND_SOC_DAPM_AIF_IN("DAI_OUT", "HiFi Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_OUT("DAI_IN", "HiFi Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_MUX("DAI IN MUX", SND_SOC_NOPM, 0, 0,
	&max98926_dai_sel_mux),
	SND_SOC_DAPM_SWITCH("PDM PATH CH_1", SND_SOC_NOPM, 0, 0,
	&max98926_pdm_path_1_control),
	SND_SOC_DAPM_SWITCH("PDM PATH CH_0", SND_SOC_NOPM, 0, 0,
	&max98926_pdm_path_0_control),
	SND_SOC_DAPM_MUX("INPUT SEL MUX", SND_SOC_NOPM, 0, 0,
	&max98926_input_sel_mux),
	SND_SOC_DAPM_MUX_E("PDM CHANNEL_1 MUX", max98926_DAI_CLK_DIV_N_LSBS, 6, 0,
	&max98926_pdm_1_sel_mux, pdm_enable_channel_ev, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("PDM CHANNEL_0 MUX", max98926_DAI_CLK_DIV_N_LSBS, 2, 0,
	&max98926_pdm_0_sel_mux, pdm_enable_channel_ev, SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX("Rc Filter MUX", SND_SOC_NOPM, 0, 0,
	&max98926_hpf_sel_mux),
	SND_SOC_DAPM_DAC_E("Amp Enable", NULL, max98926_BLOCK_ENABLE,
	M98926_SPK_EN_SHIFT, 0, max98926_dac_event,
	SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY("Global Enable", max98926_GLOBAL_ENABLE,
	M98926_EN_SHIFT, 0, NULL, 0),
	SND_SOC_DAPM_OUTPUT("BE_OUT"),
	SND_SOC_DAPM_INPUT("Speaker_Pdm"),
};


static const struct snd_soc_dapm_route max98926_audio_map[] = {
	{"DAI IN MUX", "Left", "DAI_OUT"},
	{"DAI IN MUX", "Right", "DAI_OUT"},
	{"DAI IN MUX", "LeftRight", "DAI_OUT"},
	{"DAI IN MUX", "LeftRightDiv2", "DAI_OUT"},
	{"Rc Filter MUX", "Disable", "DAI IN MUX"},
	{"Rc Filter MUX", "DC Block", "DAI IN MUX"},
	{"Rc Filter MUX", "100Hz", "DAI IN MUX"},
	{"Rc Filter MUX", "200Hz", "DAI IN MUX"},
	{"Rc Filter MUX", "400Hz", "DAI IN MUX"},
	{"Rc Filter MUX", "800Hz", "DAI IN MUX"},
	{"Amp Enable", NULL, "Rc Filter MUX"},
	{"Amp Enable", NULL, "Global Enable"},
	{"BE_OUT", NULL, "Amp Enable"},
	{"INPUT SEL MUX", "Pcm", "DAI_OUT"},
	{"INPUT SEL MUX", "Analog", "DAI_OUT"},
	{"Amp Enable", NULL, "INPUT SEL MUX"},

#if 0
	{"PDM CHANNEL_1 MUX", "Voltage", "DAI_IN"},
	{"PDM CHANNEL_1 MUX", "Current", "DAI_IN"},
	{"PDM CHANNEL_0 MUX", "Voltage", "DAI_IN"},
	{"PDM CHANNEL_0 MUX", "Current", "DAI_IN"},
	{"PDM PATH CH_1", "Switch", "PDM CHANNEL_1 MUX"},
	{"PDM PATH CH_0", "Switch", "PDM CHANNEL_0 MUX"},
	{"Speaker_Pdm", NULL, "PDM PATH CH_1"},
	{"Speaker_Pdm", NULL, "PDM PATH CH_0"},
#endif

#if 1
	{"PDM CHANNEL_1 MUX", "Voltage", "DAI_OUT"},
	{"PDM CHANNEL_1 MUX", "Current", "DAI_OUT"},
	{"PDM CHANNEL_0 MUX", "Voltage", "DAI_OUT"},
	{"PDM CHANNEL_0 MUX", "Current", "DAI_OUT"},
	{"PDM PATH CH_1", "Switch", "PDM CHANNEL_1 MUX"},
	{"PDM PATH CH_0", "Switch", "PDM CHANNEL_0 MUX"},
	{"Amp Enable", NULL, "PDM PATH CH_1"},
	{"Amp Enable", NULL, "PDM PATH CH_0"},
#endif
};

/*
static bool max98926_volatile_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case max98926_VBAT_DATA:
	case max98926_VBST_DATA:
	case max98926_LIVE_STATUS0:
	case max98926_LIVE_STATUS1:
	case max98926_LIVE_STATUS2:
	case max98926_STATE0:
	case max98926_STATE1:
	case max98926_STATE2:
	case max98926_FLAG0:
	case max98926_FLAG1:
	case max98926_FLAG2:
	case max98926_REV_VERSION:
	case max98926_DAI_CLK_DIV_N_LSBS:
		return true;
	default:
		return false;
	}
}
*/

/*
static bool max98926_readable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case max98926_IRQ_CLEAR0:
	case max98926_IRQ_CLEAR1:
	case max98926_IRQ_CLEAR2:
	case max98926_ALC_HOLD_RLS:
		return false;
	default:
		return true;
	}
}
*/

DECLARE_TLV_DB_SCALE(max98926_spk_tlv, -600, 100, 0);

/*
static int max98926_reg_put(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol, unsigned int reg,
			    unsigned int mask, unsigned int shift)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = component->codec;
	unsigned int sel = ucontrol->value.integer.value[0];

	pr_err("%s codec = %p\n", __func__, codec);

	snd_soc_update_bits(codec, reg, mask, sel << shift);
	pr_err("%s: register 0x%02X, value 0x%02X\n",
	       __func__, reg, sel);
	return 0;
}
*/


static int speaker_enable;
static int max98926_spk_enable_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	pr_err("%s , speaker_enable = %d\n", __func__, speaker_enable);
	ucontrol->value.integer.value[0] = speaker_enable;
	return 0;
}

static int max98926_spk_enable_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct snd_soc_codec *codec = component->codec;
	struct max98926_priv *max98926 = snd_soc_codec_get_drvdata(codec);

	pr_err("%s , codec = %p\n", __func__, codec);

	if (ucontrol->value.integer.value[0] == 1) {
		snd_soc_update_bits(codec, max98926_BLOCK_ENABLE,
				    (M98926_SPK_EN_MASK | M98926_BST_EN_MASK |
				     M98926_ADC_IMON_EN_MASK | M98926_ADC_VMON_EN_MASK|
				     M98926_SPK_EN_MASK | M98926_BST_EN_MASK |
				     M98926_ADC_IMON_EN_MASK | M98926_ADC_VMON_EN_MASK),
				    (M98926_SPK_EN_MASK | M98926_BST_EN_MASK |
				     M98926_ADC_IMON_EN_MASK | M98926_ADC_VMON_EN_MASK|
				     M98926_SPK_EN_MASK | M98926_BST_EN_MASK |
				     M98926_ADC_IMON_EN_MASK | M98926_ADC_VMON_EN_MASK)
				   );
		snd_soc_update_bits(codec, max98926_BLOCK_ENABLE,
				    (M98926_SPK_EN_MASK | M98926_BST_EN_MASK |
				     M98926_ADC_IMON_EN_MASK | M98926_ADC_VMON_EN_MASK|
				     M98926_SPK_EN_MASK | M98926_BST_EN_MASK |
				     M98926_ADC_IMON_EN_MASK | M98926_ADC_VMON_EN_MASK),
				    (M98926_SPK_EN_MASK | M98926_BST_EN_MASK |
				     M98926_ADC_IMON_EN_MASK | M98926_ADC_VMON_EN_MASK|
				     M98926_SPK_EN_MASK | M98926_BST_EN_MASK |
				     M98926_ADC_IMON_EN_MASK | M98926_ADC_VMON_EN_MASK)
				   );
		snd_soc_update_bits(codec, max98926_GLOBAL_ENABLE,
				    M98926_EN_MASK, M98926_EN_MASK);
		max98926_set_sense_data(max98926);
	} else if (ucontrol->value.integer.value[0] == 0) {
		snd_soc_update_bits(codec, max98926_BLOCK_ENABLE
				    , M98926_SPK_EN_MASK | M98926_BST_EN_MASK |
				    M98926_ADC_IMON_EN_MASK | M98926_ADC_VMON_EN_MASK,
				    0);
		snd_soc_update_bits(codec, max98926_GLOBAL_ENABLE,
				    M98926_EN_MASK, 0);
	}
	speaker_enable = ucontrol->value.integer.value[0];
	return 1;
}


static const char *const spk_enable_text[] = {"Off", "On"};

static const struct soc_enum max98926_global_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(spk_enable_text), spk_enable_text),
};

static const struct snd_kcontrol_new max98926_snd_controls[] = {
	SOC_SINGLE_TLV("Speaker Volume", max98926_GAIN,
	M98926_SPK_GAIN_SHIFT, (1 << M98926_SPK_GAIN_WIDTH) - 1, 0,
	max98926_spk_tlv),
	SOC_SINGLE("Ramp Switch", max98926_GAIN_RAMPING,
	M98926_SPK_RMP_EN_SHIFT, 1, 0),
	SOC_SINGLE("ZCD Switch", max98926_GAIN_RAMPING,
	M98926_SPK_ZCD_EN_SHIFT, 1, 0),
	SOC_SINGLE("ALC Switch", max98926_THRESHOLD,
	M98926_ALC_EN_SHIFT, 1, 0),
	SOC_SINGLE("ALC Threshold", max98926_THRESHOLD, M98926_ALC_TH_SHIFT,
	(1 << M98926_ALC_TH_WIDTH) - 1, 0),
	SOC_ENUM("Boost Output Voltage", max98926_boost_voltage),
	SOC_SINGLE_EXT("Speaker ZCD", 0, 0, 1, 0,
	max98926_spk_zcd_get, max98926_spk_zcd_put),
	SOC_ENUM_EXT("Spk_Enable", max98926_global_enum[0], max98926_spk_enable_get, max98926_spk_enable_put),
};

/* codec sample rate and n/m dividers parameter table */
static const struct {
	int rate;
	int  sr;
	int divisors[3][2];
} rate_table[] = {
	{
		.rate = 8000,
		.sr = 0,
		.divisors = { {1, 375}, {5, 1764}, {1, 384} }
	},
	{
		.rate = 11025,
		.sr = 1,
		.divisors = { {147, 40000}, {1, 256}, {147, 40960} }
	},
	{
		.rate = 12000,
		.sr = 2,
		.divisors = { {1, 250}, {5, 1176}, {1, 256} }
	},
	{
		.rate = 16000,
		.sr = 3,
		.divisors = { {2, 375}, {5, 882}, {1, 192} }
	},
	{
		.rate = 22050,
		.sr = 4,
		.divisors = { {147, 20000}, {1, 128}, {147, 20480} }
	},
	{
		.rate = 24000,
		.sr = 5,
		.divisors = { {1, 125}, {5, 588}, {1, 128} }
	},
	{
		.rate = 32000,
		.sr = 6,
		.divisors = { {4, 375}, {5, 441}, {1, 96} }
	},
	{
		.rate = 44100,
		.sr = 7,
		.divisors = { {147, 10000}, {1, 64}, {147, 10240} }
	},
	{
		.rate = 48000,
		.sr = 8,
		.divisors = { {2, 125}, {5, 294}, {1, 64} }
	},
};

static inline int max98926_rate_value(struct snd_soc_codec *codec,
				      int rate, int clock, int *value, int *n, int *m)
{
	int ret = -EINVAL;
	int i;

	for (i = 0; i < ARRAY_SIZE(rate_table); i++) {
		if (rate_table[i].rate >= rate) {
			*value = rate_table[i].sr;
			*n = rate_table[i].divisors[clock][0];
			*m = rate_table[i].divisors[clock][1];
			ret = 0;
			dev_dbg(codec->dev, "%s: sample rate is %d, returning %d\n",
				__func__, rate_table[i].rate, *value);
			break;
		}
	}
	return ret;
}

static void max98926_set_sense_data(struct max98926_priv *max98926)
{
	struct snd_soc_codec *codec = max98926->codec;

	pr_err("%s %d %d\n", __func__, max98926->i_slot, max98926->v_slot);
	/* set VMON slots */
	snd_soc_update_bits(codec,
			    max98926_DOUT_CFG_VMON,
			    M98926_DAI_VMON_EN_MASK, M98926_DAI_VMON_EN_MASK);
	snd_soc_update_bits(codec,
			    max98926_DOUT_CFG_VMON,
			    M98926_DAI_VMON_SLOT_MASK, max98926->v_slot);
	/* set IMON slots */
	snd_soc_update_bits(codec,
			    max98926_DOUT_CFG_IMON,
			    M98926_DAI_IMON_EN_MASK, M98926_DAI_IMON_EN_MASK);
	snd_soc_update_bits(codec,
			    max98926_DOUT_CFG_IMON,
			    M98926_DAI_IMON_SLOT_MASK, max98926->i_slot);
	pr_err("%s codec = %p", __func__, codec);
}

static int max98926_dai_set_fmt(struct snd_soc_dai *codec_dai,
				unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct max98926_priv *max98926 = snd_soc_codec_get_drvdata(codec);
	unsigned int invert = 0;

	dev_dbg(codec->dev, "%s: fmt 0x%08X\n", __func__, fmt);
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		max98926_set_sense_data(max98926);
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
	case SND_SOC_DAIFMT_CBS_CFM:
	case SND_SOC_DAIFMT_CBM_CFS:
	default:
		dev_err(codec->dev, "DAI clock mode unsupported");
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_NB_IF:
		invert = M98926_DAI_WCI_MASK;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		invert = M98926_DAI_BCI_MASK;
		break;
	case SND_SOC_DAIFMT_IB_IF:
		invert = M98926_DAI_BCI_MASK | M98926_DAI_WCI_MASK;
		break;
	default:
		dev_err(codec->dev, "DAI invert mode unsupported");
		return -EINVAL;
	}

	snd_soc_update_bits(codec, max98926_FORMAT,
			    M98926_DAI_BCI_MASK, invert);
	return 0;
}

static int max98926_set_clock(struct max98926_priv *max98926,
			      struct snd_pcm_hw_params *params)
{
	unsigned int dai_sr = 0, clock, n, m;
	struct snd_soc_codec *codec = max98926->codec;
	int rate = params_rate(params);

	/* BCLK/LRCLK ratio calculation */
	int blr_clk_ratio = params_channels(params) * max98926->ch_size;

	pr_err("%s %d %d\n", __func__, params_channels(params), max98926->ch_size);

	switch (blr_clk_ratio) {
	case 32:
		snd_soc_update_bits(codec,
				    max98926_DAI_CLK_MODE2,
				    M98926_DAI_BSEL_MASK, M98926_DAI_BSEL_32);
		break;
	case 48:
		snd_soc_update_bits(codec,
				    max98926_DAI_CLK_MODE2,
				    M98926_DAI_BSEL_MASK, M98926_DAI_BSEL_48);
		break;
	case 64:
		snd_soc_update_bits(codec,
				    max98926_DAI_CLK_MODE2,
				    M98926_DAI_BSEL_MASK, M98926_DAI_BSEL_64);
		break;
	default:
		return -EINVAL;
	}

	switch (max98926->sysclk) {
	case 6000000:
		clock = 0;
		break;
	case 11289600:
		clock = 1;
		break;
	case 12000000:
		clock = 0;
		break;
	case 12288000:
		clock = 2;
		break;
	default:
		pr_err("unsupported sysclk %d\n",
		       max98926->sysclk);
		break;
	}

	if (max98926_rate_value(codec, rate, clock, &dai_sr, &n, &m)) {
		pr_err("%s return -EINVAL\n", __func__);
		return -EINVAL;
	}

	/* set DAI_SR to correct LRCLK frequency */
	snd_soc_update_bits(codec,
			    max98926_DAI_CLK_MODE2,
			    M98926_DAI_SR_MASK, dai_sr << M98926_DAI_SR_SHIFT);
	pr_err("%s return 0\n", __func__);
	return 0;
}

static int max98926_dai_hw_params(struct snd_pcm_substream *substream,
				  struct snd_pcm_hw_params *params,
				  struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct max98926_priv *max98926 = snd_soc_codec_get_drvdata(codec);
	int ret;

	ret = snd_soc_read(codec,
			   max98926_DAI_CLK_DIV_N_LSBS);

	pr_err("%s 0x1f read 0x%x\n", __func__, ret);
	pr_err("%s %d\n", __func__, snd_pcm_format_width(params_format(params)));
	switch (snd_pcm_format_width(params_format(params))) {

	case 16:
		snd_soc_update_bits(codec,
				    max98926_FORMAT,
				    M98926_DAI_CHANSZ_MASK, M98926_DAI_CHANSZ_16);
		max98926->ch_size = 16;
		break;
	case 24:
		snd_soc_update_bits(codec,
				    max98926_FORMAT,
				    M98926_DAI_CHANSZ_MASK, M98926_DAI_CHANSZ_32);
		max98926->ch_size = 32;
		break;
	case 32:
		snd_soc_update_bits(codec,
				    max98926_FORMAT,
				    M98926_DAI_CHANSZ_MASK, M98926_DAI_CHANSZ_32);
		max98926->ch_size = 32;
		break;
	default:
		pr_err("%s: format unsupported %d",
		       __func__, params_format(params));
		return -EINVAL;
	}
	dev_dbg(codec->dev, "%s: format supported %d",
		__func__, params_format(params));
	return max98926_set_clock(max98926, params);
}

static int max98926_dai_set_sysclk(struct snd_soc_dai *dai,
				   int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = dai->codec;
	struct max98926_priv *max98926 = snd_soc_codec_get_drvdata(codec);

	max98926->sysclk = freq;
	return 0;
}

#define max98926_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_ops max98926_dai_ops = {
	.set_sysclk = max98926_dai_set_sysclk,
	.set_fmt = max98926_dai_set_fmt,
	.hw_params = max98926_dai_hw_params,
};

static struct snd_soc_dai_driver max98926_dai[] = {
	{
		.name = "max98926-aif1",
		.playback = {
			.stream_name = "Speaker_PLayback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = max98926_FORMATS,
		},
		.capture = {
			.stream_name = "Speaker_PLayback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = max98926_FORMATS,
		},
		.ops = &max98926_dai_ops,
	}
};

static int max98926_probe(struct snd_soc_codec *codec)
{
	int ret = 0;
	struct max98926_priv *max98926 = snd_soc_codec_get_drvdata(codec);

	max98926->codec = codec;
	codec->control_data = codec;

	pr_err("%s codec = %p", __func__, codec);
	speaker_enable = false;

	/*
	ret = snd_soc_codec_set_cache_io(codec, 8, 8, SND_SOC_I2C);
	if (ret != 0) {
	    dev_err(codec->dev, "Failed to set cache I/O: %d\n", ret);
	    return ret;
	}
	*/

	ret = snd_soc_read(codec,
			   max98926_REV_VERSION);

	if ((ret < 0) ||
			((ret != max98926_VERSION) &&
			 (ret != max98926_VERSION1))) {
		pr_err("device initialization error (%d )\n",
		       ret);
	}
	pr_err("device version 0x%02X\n", ret);

	/* can be configured to any other value supported by this chip */
	max98926->sysclk = 12288000;
	max98926->spk_gain = 0x14;

	snd_soc_write(codec, max98926_GLOBAL_ENABLE, 0x00);
	/* It's not the default but we need to set DAI_DLY */
	snd_soc_write(codec,
		      max98926_FORMAT, M98926_DAI_DLY_MASK);
	snd_soc_write(codec, max98926_TDM_SLOT_SELECT, 0xC8);
	snd_soc_write(codec, max98926_DOUT_HIZ_CFG1, 0xFF);
	snd_soc_write(codec, max98926_DOUT_HIZ_CFG2, 0xFF);
	snd_soc_write(codec, max98926_DOUT_HIZ_CFG3, 0xFF);
	snd_soc_write(codec, max98926_DOUT_HIZ_CFG4, 0xCC);
	snd_soc_write(codec, max98926_FILTERS, 0xD8);
	snd_soc_write(codec, max98926_ALC_CONFIGURATION, 0xF8);
	snd_soc_write(codec, max98926_GAIN, 0x0D);

	/* Disable ALC muting */

	snd_soc_write(codec, max98926_BOOST_LIMITER, 0xF8);

	if (codec->dev->of_node)
		dev_set_name(codec->dev, "%s", "MAX98926_MT");

	return 0;
}

static unsigned int max98926_read(struct snd_soc_codec *codec, unsigned int addr)
{
	unsigned short Ret = 0;
	max98926_ReadReg((unsigned short)addr, (unsigned short *)&Ret);
	return Ret;
}

static int max98926_write(struct snd_soc_codec *codec, unsigned int adrress, unsigned int value)
{
	int ret = 0;
	ret = max98926_WriteReg(adrress , value);
	return ret;
}


static struct snd_soc_codec_driver soc_codec_dev_max98926 = {
	.probe            = max98926_probe,
	.controls = max98926_snd_controls,
	.num_controls = ARRAY_SIZE(max98926_snd_controls),
	.dapm_routes = max98926_audio_map,
	.num_dapm_routes = ARRAY_SIZE(max98926_audio_map),
	.dapm_widgets = max98926_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(max98926_dapm_widgets),
	.read = max98926_read,
	.write = max98926_write,
};

/*
static struct regmap_config max98926_regmap = {
	.reg_bits         = 8,
	.val_bits         = 8,
	.max_register     = max98926_REV_VERSION,
	.reg_defaults     = max98926_reg,
	.num_reg_defaults = ARRAY_SIZE(max98926_reg),
	.volatile_reg     = max98926_volatile_register,
	.readable_reg     = max98926_readable_register,
	.cache_type       = REGCACHE_RBTREE,
};
*/


static int max98926_WriteReg(u16 a_u2Addr, u16 a_u2Data)
{
	int  i4RetValue = 0;
	char puSendCmd[2] = {(char)a_u2Addr , (char)a_u2Data};
	pr_warn("%s\n", __func__);

#ifdef CONFIG_MTK_I2C_EXTENSION
	new_client->ext_flag = 0;
#endif

	i4RetValue = i2c_master_send(new_client, puSendCmd, 2);

	if (i4RetValue < 0) {
		pr_err("max98926_WriteReg  I2C send failed!!\n");
		return -1;
	}
	return 0;
}

static int max98926_ReadReg(u16 a_u2Addr, unsigned short *a_pu2Result)
{

#ifndef CONFIG_MTK_I2C_EXTENSION
	unsigned char buffer[2];
	struct i2c_adapter *adap = new_client->adapter;
	int ret;
#endif

#ifdef CONFIG_MTK_I2C_EXTENSION
	new_client->ext_flag  = (((new_client->ext_flag) & I2C_MASK_FLAG) | I2C_WR_FLAG | I2C_RS_FLAG);

	if (i2c_master_send(new_client, (const char*)&a_u2Addr, 1 << 8 | 1) != 1)
		pr_err("max98926_ReadReg  I2C send failed!!\n");

	*a_pu2Result = a_u2Addr;
#else

	struct i2c_msg wr_msgs[2];
	buffer[0] = a_u2Addr;

	wr_msgs[0].addr = new_client->addr;
	wr_msgs[0].flags = 0x00;
	wr_msgs[0].len = 1;
	wr_msgs[0].buf = &buffer[0];

	wr_msgs[1].addr = new_client->addr;
	wr_msgs[1].flags |= I2C_M_RD;
	wr_msgs[1].len = 1;
	wr_msgs[1].buf = &buffer[1];

	ret = i2c_transfer(adap, &wr_msgs[0], 2);

	*a_pu2Result = buffer[1];
#endif

	return 0;
}


static int max98926_i2c_probe(struct i2c_client *i2c,
			      const struct i2c_device_id *id)
{
	int ret;
	u32 value;
	struct max98926_priv *max98926 = NULL;

	new_client = i2c;
#ifdef CONFIG_MTK_I2C_EXTENSION
	new_client->timing = 100;
	new_client->ext_flag  = ((new_client->ext_flag) & I2C_MASK_FLAG) | I2C_WR_FLAG | I2C_RS_FLAG;
#endif

	pr_err("%s\n", __func__);
	/*
	max98926_regulator_config(i2c, of_property_read_bool(i2c->dev.of_node,
		"max98926,i2c-pull-up"), 1);
	*/
	max98926 = devm_kzalloc(&i2c->dev,
				sizeof(*max98926), GFP_KERNEL);

	if (!max98926)
		return -ENOMEM;

	pr_err("max98926 = %p\n", max98926);

	i2c_set_clientdata(i2c, max98926);

	/*
	max98926->regmap = devm_regmap_init_i2c(i2c, &max98926_regmap);
	if (IS_ERR(max98926->regmap))
	{
	    ret = PTR_ERR(max98926->regmap);
	    dev_err(&i2c->dev,"Failed to allocate regmap: %d\n", ret);
	    goto err_out;
	}
	*/

	if (!of_property_read_u32(i2c->dev.of_node, "vmon-slot-no", &value)) {
		if (value > M98926_DAI_VMON_SLOT_1E_1F) {
			dev_err(&i2c->dev, "vmon slot number is wrong:\n");
			return -EINVAL;
		}
		max98926->v_slot = value;
	} else {
		max98926->v_slot = 0;
	}
	if (!of_property_read_u32(i2c->dev.of_node, "imon-slot-no", &value)) {
		if (value > M98926_DAI_IMON_SLOT_1E_1F) {
			dev_err(&i2c->dev, "imon slot number is wrong:\n");
			return -EINVAL;
		}
		max98926->i_slot = value;
	} else {
		max98926->i_slot = 4;
	}
	pr_err("4 %s max98926->v_slot  = %d max98926->i_slot = %d\n", __func__, max98926->v_slot, max98926->i_slot);

	if (&i2c->dev.of_node)
		dev_set_name(&i2c->dev, "%s", "MAX98926_MT");

	ret = snd_soc_register_codec(&i2c->dev, &soc_codec_dev_max98926,
				     max98926_dai, ARRAY_SIZE(max98926_dai));
	if (ret < 0)
		dev_err(&i2c->dev, "Failed to register codec: %d\n", ret);
	pr_err("%s ret = %d\n", __func__, ret);

	return ret;
}

static int max98926_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_codec(&client->dev);
	return 0;
}

static const struct i2c_device_id max98926_i2c_id[] = {
	{ "max98926L", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, max98926_i2c_id);

static const struct of_device_id max98926_of_match[] = {
	{ .compatible = "mediatek,speaker_amp", },
	{ }
};

MODULE_DEVICE_TABLE(of, max98926_of_match);

static struct i2c_driver max98926_i2c_driver = {
	.driver = {
		.name = "speaker_amp",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(max98926_of_match),
		.pm = NULL,
	},
	.probe  = max98926_i2c_probe,
	.remove = max98926_i2c_remove,
	.id_table = max98926_i2c_id,
};

/*
static struct i2c_board_info __initdata max_i2c_board_info[] = {
	{
		I2C_BOARD_INFO("max98926L", 0x31),  // if 0x31 is your device address_space
	},
};
*/

static struct platform_device *Max98926_dev;

static int __init max98926_init(void)
{
	int ret = 0;

	pr_err("+max98926_init\n");
	Max98926_dev = platform_device_alloc("max98926L", -1);

	if (!Max98926_dev)
		return -ENOMEM;

	ret = platform_device_add(Max98926_dev);
	if (ret != 0) {
		platform_device_put(Max98926_dev);
		return ret;
	}

	/*
	i2c_register_board_info(MAX98926_I2C_Channel, max_i2c_board_info, ARRAY_SIZE(max_i2c_board_info));
	*/

	if (i2c_add_driver(&max98926_i2c_driver)) {
		pr_err("fail to add device into i2c");
		return -1;
	}
	pr_err("-max98926_init\n");
	return 0;
}

subsys_initcall(max98926_init);

static void __exit max98926_exit(void)
{
	i2c_del_driver(&max98926_i2c_driver);
}
module_exit(max98926_exit);

/* replace with init_call and module exit.
module_i2c_driver(max98926_i2c_driver)
*/

MODULE_DESCRIPTION("ALSA SoC max98926 driver");
MODULE_AUTHOR("Ralph Birt <rdbirt@gmail.com>, Anish kumar <anish.kumar@maximintegrated.com>");
MODULE_LICENSE("GPL");
