/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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

#include "mt_afe_def.h"
#include "mt_afe_reg.h"
#include "mt_afe_clk.h"
#include "mt_afe_control.h"
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <sound/soc.h>

enum {
	AP_LOOPBACK_NONE = 0,
	AP_LOOPBACK_AMIC_TO_SPK,
	AP_LOOPBACK_AMIC_TO_HP,
	AP_LOOPBACK_DMIC_TO_SPK,
	AP_LOOPBACK_DMIC_TO_HP,
	AP_LOOPBACK_HEADSET_MIC_TO_SPK,
	AP_LOOPBACK_HEADSET_MIC_TO_HP,
	AP_LOOPBACK_AMIC_TO_I2S0,
	AP_LOOPBACK_HEADSET_MIC_TO_I2S0,
	AP_LOOPBACK_EXTADC2_TO_SPK,
	AP_LOOPBACK_EXTADC2_TO_HP,
};

enum {
	AFE_SGEN_OFF = 0,
	AFE_SGEN_I0I1,
	AFE_SGEN_I2,
	AFE_SGEN_I3I4,
	AFE_SGEN_I5I6,
	AFE_SGEN_I7I8,
	AFE_SGEN_I9,
	AFE_SGEN_I10I11,
	AFE_SGEN_I12I13,
	AFE_SGEN_I14,
	AFE_SGEN_I15I16,
	AFE_SGEN_I17I18,
	AFE_SGEN_I19I20,
	AFE_SGEN_I21I22,

	AFE_SGEN_O0O1,
	AFE_SGEN_O2,
	AFE_SGEN_O3,
	AFE_SGEN_O4,
	AFE_SGEN_O3O4,
	AFE_SGEN_O5O6,
	AFE_SGEN_O7O8,
	AFE_SGEN_O9O10,
	AFE_SGEN_O11,
	AFE_SGEN_O12,
	AFE_SGEN_O13O14,
	AFE_SGEN_O15O16,
	AFE_SGEN_O17O18,
	AFE_SGEN_O19O20,
	AFE_SGEN_O21O22,
	AFE_SGEN_O23O24,
};

struct mt_pcm_routing_priv {
	uint32_t ap_loopback_type;
	uint32_t afe_sinegen_type;
};

static int ap_loopback_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt_pcm_routing_priv *priv = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = priv->ap_loopback_type;
	return 0;
}

static int ap_loopback_set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt_pcm_routing_priv *priv = snd_soc_component_get_drvdata(component);
	uint32_t sample_rate = 48000;
	long set_value = ucontrol->value.integer.value[0];

	if (priv->ap_loopback_type == set_value) {
		pr_debug("%s dummy operation for %u", __func__, priv->ap_loopback_type);
		return 0;
	}

	if (priv->ap_loopback_type != AP_LOOPBACK_NONE) {
		if (priv->ap_loopback_type == AP_LOOPBACK_AMIC_TO_I2S0 ||
		    priv->ap_loopback_type == AP_LOOPBACK_HEADSET_MIC_TO_I2S0) {
			mt_afe_disable_memory_path(MT_AFE_DIGITAL_BLOCK_I2S_OUT_2);
			if (!mt_afe_get_memory_path_state(MT_AFE_DIGITAL_BLOCK_I2S_OUT_2))
				mt_afe_disable_2nd_i2s_out();

			mt_afe_disable_memory_path(MT_AFE_DIGITAL_BLOCK_I2S_IN_2);
			if (!mt_afe_get_memory_path_state(MT_AFE_DIGITAL_BLOCK_I2S_IN_2))
				mt_afe_disable_2nd_i2s_in();

			mt_afe_disable_memory_path(MT_AFE_DIGITAL_BLOCK_I2S_IN_ADC);
			if (mt_afe_get_memory_path_state(MT_AFE_DIGITAL_BLOCK_I2S_IN_ADC) == false)
				mt_afe_disable_mtkif_adc();

			/*single mic:I03-->O03 & O04*/
			mt_afe_set_connection(INTER_DISCONNECT, INTER_CONN_I03, INTER_CONN_O00);
			mt_afe_set_connection(INTER_DISCONNECT, INTER_CONN_I03, INTER_CONN_O01);
		} else if (priv->ap_loopback_type ==  AP_LOOPBACK_EXTADC2_TO_SPK ||
			priv->ap_loopback_type ==  AP_LOOPBACK_EXTADC2_TO_HP) {
			mt_afe_disable_memory_path(MT_AFE_DIGITAL_BLOCK_I2S_OUT_DAC);
			if (!mt_afe_get_memory_path_state(MT_AFE_DIGITAL_BLOCK_I2S_OUT_DAC))
				mt_afe_disable_i2s_dac();

			mt_afe_disable_memory_path(MT_AFE_DIGITAL_BLOCK_I2S_IN_ADC2);
			if (!mt_afe_get_memory_path_state(MT_AFE_DIGITAL_BLOCK_I2S_IN_ADC2))
				mt_afe_disable_i2s_adc2();

			/*single mic:I17-->O03 & O04*/
			mt_afe_set_connection(INTER_DISCONNECT, INTER_CONN_I17, INTER_CONN_O03);
			mt_afe_set_connection(INTER_DISCONNECT, INTER_CONN_I17, INTER_CONN_O04);

			mt_afe_disable_apll_div_power(MT_AFE_I2S2, sample_rate);
			mt_afe_disable_apll_div_power(MT_AFE_ENGEN, sample_rate);
			mt_afe_disable_apll_tuner(sample_rate);
			mt_afe_disable_apll(sample_rate);
		} else {
			mt_afe_disable_memory_path(MT_AFE_DIGITAL_BLOCK_I2S_OUT_DAC);
			if (!mt_afe_get_memory_path_state(MT_AFE_DIGITAL_BLOCK_I2S_OUT_DAC))
				mt_afe_disable_i2s_dac();

			mt_afe_disable_memory_path(MT_AFE_DIGITAL_BLOCK_I2S_IN_ADC);
			if (mt_afe_get_memory_path_state(MT_AFE_DIGITAL_BLOCK_I2S_IN_ADC) == false)
				mt_afe_disable_mtkif_adc();

			mt_afe_set_connection(INTER_DISCONNECT, INTER_CONN_I03, INTER_CONN_O03);
			mt_afe_set_connection(INTER_DISCONNECT, INTER_CONN_I03, INTER_CONN_O04);
		}
		mt_afe_enable_afe(false);
		mt_afe_adc_clk_off();
		mt_afe_dac_clk_off();
		mt_afe_main_clk_off();
	}

	if (set_value == AP_LOOPBACK_AMIC_TO_SPK ||
	    set_value == AP_LOOPBACK_AMIC_TO_HP ||
	    set_value == AP_LOOPBACK_DMIC_TO_SPK ||
	    set_value == AP_LOOPBACK_DMIC_TO_HP ||
	    set_value == AP_LOOPBACK_HEADSET_MIC_TO_SPK ||
	    set_value == AP_LOOPBACK_HEADSET_MIC_TO_HP) {

		if (set_value == AP_LOOPBACK_DMIC_TO_SPK ||
		    set_value == AP_LOOPBACK_DMIC_TO_HP) {
			sample_rate = 32000;
		}

		mt_afe_main_clk_on();
		mt_afe_dac_clk_on();
		mt_afe_adc_clk_on();

		/*single mic:I03-->O03 & O04*/
		mt_afe_set_connection(INTER_CONNECT, INTER_CONN_I03, INTER_CONN_O03);
		mt_afe_set_connection(INTER_CONNECT, INTER_CONN_I03, INTER_CONN_O04);

		mt_afe_set_out_conn_format(MT_AFE_CONN_OUTPUT_16BIT, INTER_CONN_O03);
		mt_afe_set_out_conn_format(MT_AFE_CONN_OUTPUT_16BIT, INTER_CONN_O04);

		/* configure uplink */
		mt_afe_set_mtkif_adc_in(sample_rate);
		if (mt_afe_get_memory_path_state(MT_AFE_DIGITAL_BLOCK_I2S_IN_ADC) == false) {
			mt_afe_enable_memory_path(MT_AFE_DIGITAL_BLOCK_I2S_IN_ADC);
			mt_afe_enable_mtkif_adc();
		} else {
			mt_afe_enable_memory_path(MT_AFE_DIGITAL_BLOCK_I2S_IN_ADC);
		}

		/* configure downlink */
		if (mt_afe_get_memory_path_state(MT_AFE_DIGITAL_BLOCK_I2S_OUT_DAC) == false) {
			mt_afe_enable_memory_path(MT_AFE_DIGITAL_BLOCK_I2S_OUT_DAC);
			mt_afe_set_i2s_dac_out(sample_rate, MT_AFE_NORMAL_CLOCK,
					MT_AFE_I2S_WLEN_16BITS);
			mt_afe_enable_i2s_dac();
		} else {
			mt_afe_enable_memory_path(MT_AFE_DIGITAL_BLOCK_I2S_OUT_DAC);
		}

		mt_afe_enable_afe(true);
	} else if (set_value == AP_LOOPBACK_AMIC_TO_I2S0 ||
		set_value == AP_LOOPBACK_HEADSET_MIC_TO_I2S0) {
		mt_afe_main_clk_on();
		mt_afe_dac_clk_on();
		mt_afe_adc_clk_on();

		mt_afe_set_connection(INTER_CONNECT, INTER_CONN_I03, INTER_CONN_O00);
		mt_afe_set_connection(INTER_CONNECT, INTER_CONN_I03, INTER_CONN_O01);

		mt_afe_set_out_conn_format(MT_AFE_CONN_OUTPUT_16BIT, INTER_CONN_O00);
		mt_afe_set_out_conn_format(MT_AFE_CONN_OUTPUT_16BIT, INTER_CONN_O01);

		/* configure uplink */
		mt_afe_set_mtkif_adc_in(sample_rate);
		if (mt_afe_get_memory_path_state(MT_AFE_DIGITAL_BLOCK_I2S_IN_ADC) == false) {
			mt_afe_enable_memory_path(MT_AFE_DIGITAL_BLOCK_I2S_IN_ADC);
			mt_afe_enable_mtkif_adc();
		} else {
			mt_afe_enable_memory_path(MT_AFE_DIGITAL_BLOCK_I2S_IN_ADC);
		}

		/* configure downlink */
		/* i2s0 soft reset begin */
		mt_afe_set_reg(AUDIO_TOP_CON1, 0x2, 0x2);

		if (mt_afe_get_memory_path_state(MT_AFE_DIGITAL_BLOCK_I2S_IN_2)) {
			mt_afe_enable_memory_path(MT_AFE_DIGITAL_BLOCK_I2S_IN_2);
			mt_afe_disable_2nd_i2s_in();
		} else {
			mt_afe_enable_memory_path(MT_AFE_DIGITAL_BLOCK_I2S_IN_2);
		}

		mt_afe_set_sample_rate(MT_AFE_DIGITAL_BLOCK_MEM_I2S, sample_rate);

		mt_afe_set_2nd_i2s_in(MT_AFE_I2S_WLEN_16BITS,
				MT_AFE_I2S_SRC_MASTER_MODE,
				MT_AFE_BCK_INV_NO_INVERSE,
				MT_AFE_NORMAL_CLOCK);

		mt_afe_enable_2nd_i2s_in();

		if (mt_afe_get_memory_path_state(MT_AFE_DIGITAL_BLOCK_I2S_OUT_2)) {
			mt_afe_enable_memory_path(MT_AFE_DIGITAL_BLOCK_I2S_OUT_2);
			mt_afe_disable_2nd_i2s_out();
		} else {
			mt_afe_enable_memory_path(MT_AFE_DIGITAL_BLOCK_I2S_OUT_2);
		}

		mt_afe_set_2nd_i2s_out(sample_rate, MT_AFE_NORMAL_CLOCK, MT_AFE_I2S_WLEN_16BITS);
		mt_afe_enable_2nd_i2s_out();

		/* i2s0 soft reset end */
		udelay(1);
		mt_afe_set_reg(AUDIO_TOP_CON1, 0x0, 0x2);
		mt_afe_enable_afe(true);
	} else if (set_value == AP_LOOPBACK_EXTADC2_TO_SPK ||
			set_value == AP_LOOPBACK_EXTADC2_TO_HP) {
		mt_afe_main_clk_on();
		mt_afe_dac_clk_on();
		mt_afe_adc_clk_on();

		/*single mic:I17-->O03 & O04*/
		mt_afe_set_connection(INTER_CONNECT, INTER_CONN_I17, INTER_CONN_O03);
		mt_afe_set_connection(INTER_CONNECT, INTER_CONN_I17, INTER_CONN_O04);

		mt_afe_set_out_conn_format(MT_AFE_CONN_OUTPUT_16BIT, INTER_CONN_O03);
		mt_afe_set_out_conn_format(MT_AFE_CONN_OUTPUT_16BIT, INTER_CONN_O04);

		/* configure uplink */
		mt_afe_enable_apll(sample_rate);
		mt_afe_enable_apll_tuner(sample_rate);
		mt_afe_set_mclk(MT_AFE_I2S2, sample_rate);
		mt_afe_set_mclk(MT_AFE_ENGEN, sample_rate);
		mt_afe_enable_apll_div_power(MT_AFE_I2S2, sample_rate);
		mt_afe_enable_apll_div_power(MT_AFE_ENGEN, sample_rate);

		mt_afe_set_i2s_adc2_in(sample_rate, MT_AFE_LOW_JITTER_CLOCK);
		if (mt_afe_get_memory_path_state(MT_AFE_DIGITAL_BLOCK_I2S_IN_ADC2) == false) {
			mt_afe_enable_memory_path(MT_AFE_DIGITAL_BLOCK_I2S_IN_ADC2);
			mt_afe_enable_i2s_adc2();
		} else {
			mt_afe_enable_memory_path(MT_AFE_DIGITAL_BLOCK_I2S_IN_ADC2);
		}

		/* configure downlink */
		if (mt_afe_get_memory_path_state(MT_AFE_DIGITAL_BLOCK_I2S_OUT_DAC) == false) {
			mt_afe_enable_memory_path(MT_AFE_DIGITAL_BLOCK_I2S_OUT_DAC);
			mt_afe_set_i2s_dac_out(sample_rate, MT_AFE_NORMAL_CLOCK,
					MT_AFE_I2S_WLEN_16BITS);
			mt_afe_enable_i2s_dac();
		} else {
			mt_afe_enable_memory_path(MT_AFE_DIGITAL_BLOCK_I2S_OUT_DAC);
		}

		mt_afe_enable_afe(true);
	}
	priv->ap_loopback_type = ucontrol->value.integer.value[0];

	return 0;
}

static int afe_sinegen_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt_pcm_routing_priv *priv = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = priv->afe_sinegen_type;
	return 0;
}

static int afe_sinegen_set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mt_pcm_routing_priv *priv = snd_soc_component_get_drvdata(component);

	if (priv->afe_sinegen_type == ucontrol->value.integer.value[0]) {
		pr_debug("%s dummy operation for %u", __func__, priv->afe_sinegen_type);
		return 0;
	}

	if (priv->afe_sinegen_type != AFE_SGEN_OFF)
		mt_afe_disable_sinegen_hw();

	switch (ucontrol->value.integer.value[0]) {
	case AFE_SGEN_I0I1:
		mt_afe_enable_sinegen_hw(INTER_CONN_I00, MT_AFE_MEMIF_DIRECTION_INPUT);
		break;
	case AFE_SGEN_I2:
		mt_afe_enable_sinegen_hw(INTER_CONN_I02, MT_AFE_MEMIF_DIRECTION_INPUT);
		break;
	case AFE_SGEN_I3I4:
		mt_afe_enable_sinegen_hw(INTER_CONN_I03, MT_AFE_MEMIF_DIRECTION_INPUT);
		break;
	case AFE_SGEN_I5I6:
		mt_afe_enable_sinegen_hw(INTER_CONN_I05, MT_AFE_MEMIF_DIRECTION_INPUT);
		break;
	case AFE_SGEN_I7I8:
		mt_afe_enable_sinegen_hw(INTER_CONN_I07, MT_AFE_MEMIF_DIRECTION_INPUT);
		break;
	case AFE_SGEN_I9:
		mt_afe_enable_sinegen_hw(INTER_CONN_I09, MT_AFE_MEMIF_DIRECTION_INPUT);
		break;
	case AFE_SGEN_I10I11:
		mt_afe_enable_sinegen_hw(INTER_CONN_I10, MT_AFE_MEMIF_DIRECTION_INPUT);
		break;
	case AFE_SGEN_I12I13:
		mt_afe_enable_sinegen_hw(INTER_CONN_I12, MT_AFE_MEMIF_DIRECTION_INPUT);
		break;
	case AFE_SGEN_I15I16:
		mt_afe_enable_sinegen_hw(INTER_CONN_I15, MT_AFE_MEMIF_DIRECTION_INPUT);
		break;
	case AFE_SGEN_I17I18:
		mt_afe_enable_sinegen_hw(INTER_CONN_I17, MT_AFE_MEMIF_DIRECTION_INPUT);
		break;
	case AFE_SGEN_I19I20:
		mt_afe_enable_sinegen_hw(INTER_CONN_I19, MT_AFE_MEMIF_DIRECTION_INPUT);
		break;
	case AFE_SGEN_O0O1:
		mt_afe_enable_sinegen_hw(INTER_CONN_O01, MT_AFE_MEMIF_DIRECTION_OUTPUT);
		break;
	case AFE_SGEN_O2:
		mt_afe_enable_sinegen_hw(INTER_CONN_O02, MT_AFE_MEMIF_DIRECTION_OUTPUT);
		break;
	case AFE_SGEN_O3:
		mt_afe_set_reg(AFE_SGEN_CON0, 0x2e8c28c2, 0xffffffff);
		break;
	case AFE_SGEN_O4:
		mt_afe_set_reg(AFE_SGEN_CON0, 0x2d8c28c2, 0xffffffff);
		break;
	case AFE_SGEN_O3O4:
		mt_afe_enable_sinegen_hw(INTER_CONN_O03, MT_AFE_MEMIF_DIRECTION_OUTPUT);
		break;
	case AFE_SGEN_O5O6:
		mt_afe_enable_sinegen_hw(INTER_CONN_O05, MT_AFE_MEMIF_DIRECTION_OUTPUT);
		break;
	case AFE_SGEN_O7O8:
		mt_afe_enable_sinegen_hw(INTER_CONN_O07, MT_AFE_MEMIF_DIRECTION_OUTPUT);
		break;
	case AFE_SGEN_O9O10:
		mt_afe_enable_sinegen_hw(INTER_CONN_O09, MT_AFE_MEMIF_DIRECTION_OUTPUT);
		break;
	case AFE_SGEN_O11:
		mt_afe_enable_sinegen_hw(INTER_CONN_O11, MT_AFE_MEMIF_DIRECTION_OUTPUT);
		break;
	case AFE_SGEN_O12:
		mt_afe_enable_sinegen_hw(INTER_CONN_O12, MT_AFE_MEMIF_DIRECTION_OUTPUT);
		break;
	case AFE_SGEN_O13O14:
		mt_afe_enable_sinegen_hw(INTER_CONN_O13, MT_AFE_MEMIF_DIRECTION_OUTPUT);
		break;
	case AFE_SGEN_O15O16:
		mt_afe_enable_sinegen_hw(INTER_CONN_O15, MT_AFE_MEMIF_DIRECTION_OUTPUT);
		break;
	case AFE_SGEN_O19O20:
		mt_afe_enable_sinegen_hw(INTER_CONN_O19, MT_AFE_MEMIF_DIRECTION_OUTPUT);
		break;
	case AFE_SGEN_O21O22:
		mt_afe_enable_sinegen_hw(INTER_CONN_O21, MT_AFE_MEMIF_DIRECTION_OUTPUT);
		break;
	case AFE_SGEN_I14:
	case AFE_SGEN_I21I22:
	case AFE_SGEN_O17O18:
	case AFE_SGEN_O23O24:
		/* not supported */
		break;
	default:
		mt_afe_disable_sinegen_hw();
		break;
	}

	priv->afe_sinegen_type = ucontrol->value.integer.value[0];

	return 0;
}

static const char *const ap_loopback_function[] = {
	ENUM_TO_STR(AP_LOOPBACK_NONE),
	ENUM_TO_STR(AP_LOOPBACK_AMIC_TO_SPK),
	ENUM_TO_STR(AP_LOOPBACK_AMIC_TO_HP),
	ENUM_TO_STR(AP_LOOPBACK_DMIC_TO_SPK),
	ENUM_TO_STR(AP_LOOPBACK_DMIC_TO_HP),
	ENUM_TO_STR(AP_LOOPBACK_HEADSET_MIC_TO_SPK),
	ENUM_TO_STR(AP_LOOPBACK_HEADSET_MIC_TO_HP),
	ENUM_TO_STR(AP_LOOPBACK_AMIC_TO_I2S0),
	ENUM_TO_STR(AP_LOOPBACK_HEADSET_MIC_TO_I2S0),
	ENUM_TO_STR(AP_LOOPBACK_EXTADC2_TO_SPK),
	ENUM_TO_STR(AP_LOOPBACK_EXTADC2_TO_HP),
};

static const char *const afe_sgen_function[] = {
	ENUM_TO_STR(AFE_SGEN_OFF),
	ENUM_TO_STR(AFE_SGEN_I0I1),
	ENUM_TO_STR(AFE_SGEN_I2),
	ENUM_TO_STR(AFE_SGEN_I3I4),
	ENUM_TO_STR(AFE_SGEN_I5I6),
	ENUM_TO_STR(AFE_SGEN_I7I8),
	ENUM_TO_STR(AFE_SGEN_I9),
	ENUM_TO_STR(AFE_SGEN_I10I11),
	ENUM_TO_STR(AFE_SGEN_I12I13),
	ENUM_TO_STR(AFE_SGEN_I14),
	ENUM_TO_STR(AFE_SGEN_I15I16),
	ENUM_TO_STR(AFE_SGEN_I17I18),
	ENUM_TO_STR(AFE_SGEN_I19I20),
	ENUM_TO_STR(AFE_SGEN_I21I22),
	ENUM_TO_STR(AFE_SGEN_O0O1),
	ENUM_TO_STR(AFE_SGEN_O2),
	ENUM_TO_STR(AFE_SGEN_O3),
	ENUM_TO_STR(AFE_SGEN_O4),
	ENUM_TO_STR(AFE_SGEN_O3O4),
	ENUM_TO_STR(AFE_SGEN_O5O6),
	ENUM_TO_STR(AFE_SGEN_O7O8),
	ENUM_TO_STR(AFE_SGEN_O9O10),
	ENUM_TO_STR(AFE_SGEN_O11),
	ENUM_TO_STR(AFE_SGEN_O12),
	ENUM_TO_STR(AFE_SGEN_O13O14),
	ENUM_TO_STR(AFE_SGEN_O15O16),
	ENUM_TO_STR(AFE_SGEN_O17O18),
	ENUM_TO_STR(AFE_SGEN_O19O20),
	ENUM_TO_STR(AFE_SGEN_O21O22),
	ENUM_TO_STR(AFE_SGEN_O23O24),
};

static const struct soc_enum mt_pcm_routing_control_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(ap_loopback_function), ap_loopback_function),
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(afe_sgen_function), afe_sgen_function),
};

static const struct snd_kcontrol_new mt_pcm_routing_controls[] = {
	SOC_ENUM_EXT("AP_Loopback_Select", mt_pcm_routing_control_enum[0], ap_loopback_get,
		     ap_loopback_set),
	SOC_ENUM_EXT("Audio_SideGen_Switch", mt_pcm_routing_control_enum[1], afe_sinegen_get,
		     afe_sinegen_set),
};

static int mt_pcm_routing_probe(struct snd_soc_platform *platform)
{
	snd_soc_add_platform_controls(platform, mt_pcm_routing_controls,
				      ARRAY_SIZE(mt_pcm_routing_controls));
	return 0;
}

static int mt_pcm_routing_open(struct snd_pcm_substream *substream)
{
	return 0;
}

static int mt_pcm_routing_close(struct snd_pcm_substream *substream)
{
	return 0;
}


#ifdef CONFIG_PM
static int mt_pcm_routing_suspend(struct snd_soc_dai *dai)
{
	mt_afe_suspend();
	return 0;
}

static int mt_pcm_routing_resume(struct snd_soc_dai *dai)
{
	mt_afe_resume();
	return 0;
}
#else
#define mt_pcm_routing_suspend	NULL
#define mt_pcm_routing_resume	NULL
#endif

static struct snd_pcm_ops mt_pcm_routing_ops = {
	.open = mt_pcm_routing_open,
	.close = mt_pcm_routing_close,
};

static struct snd_soc_platform_driver mt_pcm_routing_platform = {
	.ops = &mt_pcm_routing_ops,
	.probe = mt_pcm_routing_probe,
	.suspend = mt_pcm_routing_suspend,
	.resume = mt_pcm_routing_resume,
};

static int mt_pcm_routing_dev_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mt_pcm_routing_priv *priv;

	pr_debug("%s dev name %s\n", __func__, dev_name(&pdev->dev));

	if (dev->of_node) {
		dev_set_name(dev, "%s", MT_SOC_ROUTING_PCM);
		pr_debug("%s set dev name %s\n", __func__, dev_name(dev));
	}

	priv = devm_kzalloc(dev, sizeof(struct mt_pcm_routing_priv), GFP_KERNEL);
	if (unlikely(!priv)) {
		pr_err("%s failed to allocate private data\n", __func__);
		return -ENOMEM;
	}

	dev_set_drvdata(dev, priv);

	return snd_soc_register_platform(dev, &mt_pcm_routing_platform);
}

static int mt_pcm_routing_dev_remove(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static const struct of_device_id mt_pcm_routing_dt_match[] = {
	{.compatible = "mediatek," MT_SOC_ROUTING_PCM,},
	{}
};

MODULE_DEVICE_TABLE(of, mt_pcm_routing_dt_match);

static struct platform_driver mt_pcm_routing_driver = {
	.driver = {
		   .name = MT_SOC_ROUTING_PCM,
		   .owner = THIS_MODULE,
		   .of_match_table = mt_pcm_routing_dt_match,
		   },
	.probe = mt_pcm_routing_dev_probe,
	.remove = mt_pcm_routing_dev_remove,
};

module_platform_driver(mt_pcm_routing_driver);

MODULE_DESCRIPTION("MTK PCM Routing platform driver");
MODULE_LICENSE("GPL");
