/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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
#include <linux/mfd/wcd9310/core.h>
#include <linux/mfd/wcd9310/registers.h>
#include <linux/mfd/wcd9310/pdata.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/jack.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include "wcd9310.h"

#define WCD9310_RATES (SNDRV_PCM_RATE_8000|SNDRV_PCM_RATE_16000|\
			SNDRV_PCM_RATE_32000|SNDRV_PCM_RATE_48000)

#define NUM_DECIMATORS 10
#define NUM_INTERPOLATORS 7
#define BITS_PER_REG 8
#define TABLA_RX_DAI_ID 1
#define TABLA_TX_DAI_ID 2
#define TABLA_CFILT_FAST_MODE 0x00
#define TABLA_CFILT_SLOW_MODE 0x40

#define TABLA_JACK_MASK (SND_JACK_HEADSET | SND_JACK_OC_HPHL | SND_JACK_OC_HPHR)

#define TABLA_I2S_MASTER_MODE_MASK 0x08

#define TABLA_OCP_ATTEMPT 1

static const DECLARE_TLV_DB_SCALE(digital_gain, 0, 1, 0);
static const DECLARE_TLV_DB_SCALE(line_gain, 0, 7, 1);
static const DECLARE_TLV_DB_SCALE(analog_gain, 0, 25, 1);

enum tabla_bandgap_type {
	TABLA_BANDGAP_OFF = 0,
	TABLA_BANDGAP_AUDIO_MODE,
	TABLA_BANDGAP_MBHC_MODE,
};

struct mbhc_micbias_regs {
	u16 cfilt_val;
	u16 cfilt_ctl;
	u16 mbhc_reg;
	u16 int_rbias;
	u16 ctl_reg;
	u8 cfilt_sel;
};

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

/* Flags to track of PA and DAC state.
 * PA and DAC should be tracked separately as AUXPGA loopback requires
 * only PA to be turned on without DAC being on. */
enum tabla_priv_ack_flags {
	TABLA_HPHL_PA_OFF_ACK = 0,
	TABLA_HPHR_PA_OFF_ACK,
	TABLA_HPHL_DAC_OFF_ACK,
	TABLA_HPHR_DAC_OFF_ACK
};

struct tabla_priv {
	struct snd_soc_codec *codec;
	u32 adc_count;
	u32 cfilt1_cnt;
	u32 cfilt2_cnt;
	u32 cfilt3_cnt;
	u32 rx_bias_count;
	enum tabla_bandgap_type bandgap_type;
	bool mclk_enabled;
	bool clock_active;
	bool config_mode_active;
	bool mbhc_polling_active;
	bool fake_insert_context;
	int buttons_pressed;

	struct tabla_mbhc_calibration *calibration;

	struct snd_soc_jack *headset_jack;
	struct snd_soc_jack *button_jack;

	struct tabla_pdata *pdata;
	u32 anc_slot;

	bool no_mic_headset_override;
	/* Delayed work to report long button press */
	struct delayed_work btn0_dwork;

	struct mbhc_micbias_regs mbhc_bias_regs;
	u8 cfilt_k_value;
	bool mbhc_micbias_switched;

	/* track PA/DAC state */
	unsigned long hph_pa_dac_state;

	/*track tabla interface type*/
	u8 intf_type;

	u32 hph_status; /* track headhpone status */
	/* define separate work for left and right headphone OCP to avoid
	 * additional checking on which OCP event to report so no locking
	 * to ensure synchronization is required
	 */
	struct work_struct hphlocp_work; /* reporting left hph ocp off */
	struct work_struct hphrocp_work; /* reporting right hph ocp off */

	/* pm_cnt holds number of sleep lock holders + 1
	 * so if pm_cnt is 1 system is sleep-able. */
	atomic_t pm_cnt;
	wait_queue_head_t pm_wq;

	u8 hphlocp_cnt; /* headphone left ocp retry */
	u8 hphrocp_cnt; /* headphone right ocp retry */
};

#ifdef CONFIG_DEBUG_FS
struct tabla_priv *debug_tabla_priv;
#endif

static int tabla_codec_enable_charge_pump(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	pr_debug("%s %d\n", __func__, event);
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, TABLA_A_CDC_CLK_OTHR_CTL, 0x01,
			0x01);
		snd_soc_update_bits(codec, TABLA_A_CDC_CLSG_CTL, 0x08, 0x08);
		usleep_range(200, 200);
		snd_soc_update_bits(codec, TABLA_A_CP_STATIC, 0x10, 0x00);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, TABLA_A_CDC_CLK_OTHR_RESET_CTL, 0x10,
			0x10);
		usleep_range(20, 20);
		snd_soc_update_bits(codec, TABLA_A_CP_STATIC, 0x08, 0x08);
		snd_soc_update_bits(codec, TABLA_A_CP_STATIC, 0x10, 0x10);
		snd_soc_update_bits(codec, TABLA_A_CDC_CLSG_CTL, 0x08, 0x00);
		snd_soc_update_bits(codec, TABLA_A_CDC_CLK_OTHR_CTL, 0x01,
			0x00);
		snd_soc_update_bits(codec, TABLA_A_CP_STATIC, 0x08, 0x00);
		break;
	}
	return 0;
}

static int tabla_get_anc_slot(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);
	ucontrol->value.integer.value[0] = tabla->anc_slot;
	return 0;
}

static int tabla_put_anc_slot(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);
	tabla->anc_slot = ucontrol->value.integer.value[0];
	return 0;
}

static int tabla_pa_gain_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	u8 ear_pa_gain;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	ear_pa_gain = snd_soc_read(codec, TABLA_A_RX_EAR_GAIN);

	ear_pa_gain = ear_pa_gain >> 5;

	if (ear_pa_gain == 0x00) {
		ucontrol->value.integer.value[0] = 0;
	} else if (ear_pa_gain == 0x04) {
		ucontrol->value.integer.value[0] = 1;
	} else  {
		pr_err("%s: ERROR: Unsupported Ear Gain = 0x%x\n",
				__func__, ear_pa_gain);
		return -EINVAL;
	}

	pr_debug("%s: ear_pa_gain = 0x%x\n", __func__, ear_pa_gain);

	return 0;
}

static int tabla_pa_gain_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	u8 ear_pa_gain;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	pr_debug("%s: ucontrol->value.integer.value[0]  = %ld\n", __func__,
			ucontrol->value.integer.value[0]);

	switch (ucontrol->value.integer.value[0]) {
	case 0:
		ear_pa_gain = 0x00;
		break;
	case 1:
		ear_pa_gain = 0x80;
		break;
	default:
		return -EINVAL;
	}

	snd_soc_update_bits(codec, TABLA_A_RX_EAR_GAIN, 0xE0, ear_pa_gain);
	return 0;
}

static int tabla_get_iir_enable_audio_mixer(
					struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	int iir_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int band_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;

	ucontrol->value.integer.value[0] =
		snd_soc_read(codec, (TABLA_A_CDC_IIR1_CTL + 16 * iir_idx)) &
		(1 << band_idx);

	pr_debug("%s: IIR #%d band #%d enable %d\n", __func__,
		iir_idx, band_idx,
		(uint32_t)ucontrol->value.integer.value[0]);
	return 0;
}

static int tabla_put_iir_enable_audio_mixer(
					struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	int iir_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int band_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;
	int value = ucontrol->value.integer.value[0];

	/* Mask first 5 bits, 6-8 are reserved */
	snd_soc_update_bits(codec, (TABLA_A_CDC_IIR1_CTL + 16 * iir_idx),
		(1 << band_idx), (value << band_idx));

	pr_debug("%s: IIR #%d band #%d enable %d\n", __func__,
		iir_idx, band_idx, value);
	return 0;
}
static uint32_t get_iir_band_coeff(struct snd_soc_codec *codec,
				int iir_idx, int band_idx,
				int coeff_idx)
{
	/* Address does not automatically update if reading */
	snd_soc_update_bits(codec,
		(TABLA_A_CDC_IIR1_COEF_B1_CTL + 16 * iir_idx),
		0x1F, band_idx * BAND_MAX + coeff_idx);

	/* Mask bits top 2 bits since they are reserved */
	return ((snd_soc_read(codec,
		(TABLA_A_CDC_IIR1_COEF_B2_CTL + 16 * iir_idx)) << 24) |
		(snd_soc_read(codec,
		(TABLA_A_CDC_IIR1_COEF_B3_CTL + 16 * iir_idx)) << 16) |
		(snd_soc_read(codec,
		(TABLA_A_CDC_IIR1_COEF_B4_CTL + 16 * iir_idx)) << 8) |
		(snd_soc_read(codec,
		(TABLA_A_CDC_IIR1_COEF_B5_CTL + 16 * iir_idx)))) &
		0x3FFFFFFF;
}

static int tabla_get_iir_band_audio_mixer(
					struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
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

	pr_debug("%s: IIR #%d band #%d b0 = 0x%x\n"
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
				int coeff_idx, uint32_t value)
{
	/* Mask top 3 bits, 6-8 are reserved */
	/* Update address manually each time */
	snd_soc_update_bits(codec,
		(TABLA_A_CDC_IIR1_COEF_B1_CTL + 16 * iir_idx),
		0x1F, band_idx * BAND_MAX + coeff_idx);

	/* Mask top 2 bits, 7-8 are reserved */
	snd_soc_update_bits(codec,
		(TABLA_A_CDC_IIR1_COEF_B2_CTL + 16 * iir_idx),
		0x3F, (value >> 24) & 0x3F);

	/* Isolate 8bits at a time */
	snd_soc_update_bits(codec,
		(TABLA_A_CDC_IIR1_COEF_B3_CTL + 16 * iir_idx),
		0xFF, (value >> 16) & 0xFF);

	snd_soc_update_bits(codec,
		(TABLA_A_CDC_IIR1_COEF_B4_CTL + 16 * iir_idx),
		0xFF, (value >> 8) & 0xFF);

	snd_soc_update_bits(codec,
		(TABLA_A_CDC_IIR1_COEF_B5_CTL + 16 * iir_idx),
		0xFF, value & 0xFF);
}

static int tabla_put_iir_band_audio_mixer(
					struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	int iir_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int band_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;

	set_iir_band_coeff(codec, iir_idx, band_idx, 0,
				ucontrol->value.integer.value[0]);
	set_iir_band_coeff(codec, iir_idx, band_idx, 1,
				ucontrol->value.integer.value[1]);
	set_iir_band_coeff(codec, iir_idx, band_idx, 2,
				ucontrol->value.integer.value[2]);
	set_iir_band_coeff(codec, iir_idx, band_idx, 3,
				ucontrol->value.integer.value[3]);
	set_iir_band_coeff(codec, iir_idx, band_idx, 4,
				ucontrol->value.integer.value[4]);

	pr_debug("%s: IIR #%d band #%d b0 = 0x%x\n"
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

static const char *tabla_ear_pa_gain_text[] = {"POS_6_DB", "POS_2_DB"};
static const struct soc_enum tabla_ear_pa_gain_enum[] = {
		SOC_ENUM_SINGLE_EXT(2, tabla_ear_pa_gain_text),
};

/*cut of frequency for high pass filter*/
static const char *cf_text[] = {
	"MIN_3DB_4Hz", "MIN_3DB_75Hz", "MIN_3DB_150Hz"
};

static const struct soc_enum cf_dec1_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_TX1_MUX_CTL, 4, 3, cf_text);

static const struct soc_enum cf_dec2_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_TX2_MUX_CTL, 4, 3, cf_text);

static const struct soc_enum cf_dec3_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_TX3_MUX_CTL, 4, 3, cf_text);

static const struct soc_enum cf_dec4_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_TX4_MUX_CTL, 4, 3, cf_text);

static const struct soc_enum cf_dec5_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_TX5_MUX_CTL, 4, 3, cf_text);

static const struct soc_enum cf_dec6_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_TX6_MUX_CTL, 4, 3, cf_text);

static const struct soc_enum cf_dec7_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_TX7_MUX_CTL, 4, 3, cf_text);

static const struct soc_enum cf_dec8_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_TX8_MUX_CTL, 4, 3, cf_text);

static const struct soc_enum cf_dec9_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_TX9_MUX_CTL, 4, 3, cf_text);

static const struct soc_enum cf_dec10_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_TX10_MUX_CTL, 4, 3, cf_text);

static const struct soc_enum cf_rxmix1_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_RX1_B4_CTL, 1, 3, cf_text);

static const struct soc_enum cf_rxmix2_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_RX2_B4_CTL, 1, 3, cf_text);

static const struct soc_enum cf_rxmix3_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_RX3_B4_CTL, 1, 3, cf_text);

static const struct soc_enum cf_rxmix4_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_RX4_B4_CTL, 1, 3, cf_text);

static const struct soc_enum cf_rxmix5_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_RX5_B4_CTL, 1, 3, cf_text)
;
static const struct soc_enum cf_rxmix6_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_RX6_B4_CTL, 1, 3, cf_text);

static const struct soc_enum cf_rxmix7_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_RX7_B4_CTL, 1, 3, cf_text);

static const struct snd_kcontrol_new tabla_snd_controls[] = {

	SOC_ENUM_EXT("EAR PA Gain", tabla_ear_pa_gain_enum[0],
		tabla_pa_gain_get, tabla_pa_gain_put),

	SOC_SINGLE_TLV("LINEOUT1 Volume", TABLA_A_RX_LINE_1_GAIN, 0, 12, 1,
		line_gain),
	SOC_SINGLE_TLV("LINEOUT2 Volume", TABLA_A_RX_LINE_2_GAIN, 0, 12, 1,
		line_gain),
	SOC_SINGLE_TLV("LINEOUT3 Volume", TABLA_A_RX_LINE_3_GAIN, 0, 12, 1,
		line_gain),
	SOC_SINGLE_TLV("LINEOUT4 Volume", TABLA_A_RX_LINE_4_GAIN, 0, 12, 1,
		line_gain),
	SOC_SINGLE_TLV("LINEOUT5 Volume", TABLA_A_RX_LINE_5_GAIN, 0, 12, 1,
		line_gain),

	SOC_SINGLE_TLV("HPHL Volume", TABLA_A_RX_HPH_L_GAIN, 0, 12, 1,
		line_gain),
	SOC_SINGLE_TLV("HPHR Volume", TABLA_A_RX_HPH_R_GAIN, 0, 12, 1,
		line_gain),

	SOC_SINGLE_S8_TLV("RX1 Digital Volume", TABLA_A_CDC_RX1_VOL_CTL_B2_CTL,
		-84, 40, digital_gain),
	SOC_SINGLE_S8_TLV("RX2 Digital Volume", TABLA_A_CDC_RX2_VOL_CTL_B2_CTL,
		-84, 40, digital_gain),
	SOC_SINGLE_S8_TLV("RX3 Digital Volume", TABLA_A_CDC_RX3_VOL_CTL_B2_CTL,
		-84, 40, digital_gain),
	SOC_SINGLE_S8_TLV("RX4 Digital Volume", TABLA_A_CDC_RX4_VOL_CTL_B2_CTL,
		-84, 40, digital_gain),
	SOC_SINGLE_S8_TLV("RX5 Digital Volume", TABLA_A_CDC_RX5_VOL_CTL_B2_CTL,
		-84, 40, digital_gain),
	SOC_SINGLE_S8_TLV("RX6 Digital Volume", TABLA_A_CDC_RX6_VOL_CTL_B2_CTL,
		-84, 40, digital_gain),

	SOC_SINGLE_S8_TLV("DEC1 Volume", TABLA_A_CDC_TX1_VOL_CTL_GAIN, -84, 40,
		digital_gain),
	SOC_SINGLE_S8_TLV("DEC2 Volume", TABLA_A_CDC_TX2_VOL_CTL_GAIN, -84, 40,
		digital_gain),
	SOC_SINGLE_S8_TLV("DEC3 Volume", TABLA_A_CDC_TX3_VOL_CTL_GAIN, -84, 40,
		digital_gain),
	SOC_SINGLE_S8_TLV("DEC4 Volume", TABLA_A_CDC_TX4_VOL_CTL_GAIN, -84, 40,
		digital_gain),
	SOC_SINGLE_S8_TLV("DEC5 Volume", TABLA_A_CDC_TX5_VOL_CTL_GAIN, -84, 40,
		digital_gain),
	SOC_SINGLE_S8_TLV("DEC6 Volume", TABLA_A_CDC_TX6_VOL_CTL_GAIN, -84, 40,
		digital_gain),
	SOC_SINGLE_S8_TLV("DEC7 Volume", TABLA_A_CDC_TX7_VOL_CTL_GAIN, -84, 40,
		digital_gain),
	SOC_SINGLE_S8_TLV("DEC8 Volume", TABLA_A_CDC_TX8_VOL_CTL_GAIN, -84, 40,
		digital_gain),
	SOC_SINGLE_S8_TLV("DEC9 Volume", TABLA_A_CDC_TX9_VOL_CTL_GAIN, -84, 40,
		digital_gain),
	SOC_SINGLE_S8_TLV("DEC10 Volume", TABLA_A_CDC_TX10_VOL_CTL_GAIN, -84,
		40, digital_gain),
	SOC_SINGLE_S8_TLV("IIR1 INP1 Volume", TABLA_A_CDC_IIR1_GAIN_B1_CTL, -84,
		40, digital_gain),
	SOC_SINGLE_S8_TLV("IIR1 INP2 Volume", TABLA_A_CDC_IIR1_GAIN_B2_CTL, -84,
		40, digital_gain),
	SOC_SINGLE_S8_TLV("IIR1 INP3 Volume", TABLA_A_CDC_IIR1_GAIN_B3_CTL, -84,
		40, digital_gain),
	SOC_SINGLE_S8_TLV("IIR1 INP4 Volume", TABLA_A_CDC_IIR1_GAIN_B4_CTL, -84,
		40, digital_gain),
	SOC_SINGLE_TLV("ADC1 Volume", TABLA_A_TX_1_2_EN, 5, 3, 0, analog_gain),
	SOC_SINGLE_TLV("ADC2 Volume", TABLA_A_TX_1_2_EN, 1, 3, 0, analog_gain),
	SOC_SINGLE_TLV("ADC3 Volume", TABLA_A_TX_3_4_EN, 5, 3, 0, analog_gain),
	SOC_SINGLE_TLV("ADC4 Volume", TABLA_A_TX_3_4_EN, 1, 3, 0, analog_gain),
	SOC_SINGLE_TLV("ADC5 Volume", TABLA_A_TX_5_6_EN, 5, 3, 0, analog_gain),
	SOC_SINGLE_TLV("ADC6 Volume", TABLA_A_TX_5_6_EN, 1, 3, 0, analog_gain),

	SOC_SINGLE("MICBIAS1 CAPLESS Switch", TABLA_A_MICB_1_CTL, 4, 1, 1),
	SOC_SINGLE("MICBIAS2 CAPLESS Switch", TABLA_A_MICB_2_CTL, 4, 1, 1),
	SOC_SINGLE("MICBIAS3 CAPLESS Switch", TABLA_A_MICB_3_CTL, 4, 1, 1),
	SOC_SINGLE("MICBIAS4 CAPLESS Switch", TABLA_A_MICB_4_CTL, 4, 1, 1),

	SOC_SINGLE_EXT("ANC Slot", SND_SOC_NOPM, 0, 0, 100, tabla_get_anc_slot,
		tabla_put_anc_slot),
	SOC_ENUM("TX1 HPF cut off", cf_dec1_enum),
	SOC_ENUM("TX2 HPF cut off", cf_dec2_enum),
	SOC_ENUM("TX3 HPF cut off", cf_dec3_enum),
	SOC_ENUM("TX4 HPF cut off", cf_dec4_enum),
	SOC_ENUM("TX5 HPF cut off", cf_dec5_enum),
	SOC_ENUM("TX6 HPF cut off", cf_dec6_enum),
	SOC_ENUM("TX7 HPF cut off", cf_dec7_enum),
	SOC_ENUM("TX8 HPF cut off", cf_dec8_enum),
	SOC_ENUM("TX9 HPF cut off", cf_dec9_enum),
	SOC_ENUM("TX10 HPF cut off", cf_dec10_enum),

	SOC_SINGLE("TX1 HPF Switch", TABLA_A_CDC_TX1_MUX_CTL, 3, 1, 0),
	SOC_SINGLE("TX2 HPF Switch", TABLA_A_CDC_TX2_MUX_CTL, 3, 1, 0),
	SOC_SINGLE("TX3 HPF Switch", TABLA_A_CDC_TX3_MUX_CTL, 3, 1, 0),
	SOC_SINGLE("TX4 HPF Switch", TABLA_A_CDC_TX4_MUX_CTL, 3, 1, 0),
	SOC_SINGLE("TX5 HPF Switch", TABLA_A_CDC_TX5_MUX_CTL, 3, 1, 0),
	SOC_SINGLE("TX6 HPF Switch", TABLA_A_CDC_TX6_MUX_CTL, 3, 1, 0),
	SOC_SINGLE("TX7 HPF Switch", TABLA_A_CDC_TX7_MUX_CTL, 3, 1, 0),
	SOC_SINGLE("TX8 HPF Switch", TABLA_A_CDC_TX8_MUX_CTL, 3, 1, 0),
	SOC_SINGLE("TX9 HPF Switch", TABLA_A_CDC_TX9_MUX_CTL, 3, 1, 0),
	SOC_SINGLE("TX10 HPF Switch", TABLA_A_CDC_TX10_MUX_CTL, 3, 1, 0),

	SOC_SINGLE("RX1 HPF Switch", TABLA_A_CDC_RX1_B5_CTL, 2, 1, 0),
	SOC_SINGLE("RX2 HPF Switch", TABLA_A_CDC_RX2_B5_CTL, 2, 1, 0),
	SOC_SINGLE("RX3 HPF Switch", TABLA_A_CDC_RX3_B5_CTL, 2, 1, 0),
	SOC_SINGLE("RX4 HPF Switch", TABLA_A_CDC_RX4_B5_CTL, 2, 1, 0),
	SOC_SINGLE("RX5 HPF Switch", TABLA_A_CDC_RX5_B5_CTL, 2, 1, 0),
	SOC_SINGLE("RX6 HPF Switch", TABLA_A_CDC_RX6_B5_CTL, 2, 1, 0),
	SOC_SINGLE("RX7 HPF Switch", TABLA_A_CDC_RX7_B5_CTL, 2, 1, 0),

	SOC_ENUM("RX1 HPF cut off", cf_rxmix1_enum),
	SOC_ENUM("RX2 HPF cut off", cf_rxmix2_enum),
	SOC_ENUM("RX3 HPF cut off", cf_rxmix3_enum),
	SOC_ENUM("RX4 HPF cut off", cf_rxmix4_enum),
	SOC_ENUM("RX5 HPF cut off", cf_rxmix5_enum),
	SOC_ENUM("RX6 HPF cut off", cf_rxmix6_enum),
	SOC_ENUM("RX7 HPF cut off", cf_rxmix7_enum),

	SOC_SINGLE_EXT("IIR1 Enable Band1", IIR1, BAND1, 1, 0,
	tabla_get_iir_enable_audio_mixer, tabla_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR1 Enable Band2", IIR1, BAND2, 1, 0,
	tabla_get_iir_enable_audio_mixer, tabla_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR1 Enable Band3", IIR1, BAND3, 1, 0,
	tabla_get_iir_enable_audio_mixer, tabla_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR1 Enable Band4", IIR1, BAND4, 1, 0,
	tabla_get_iir_enable_audio_mixer, tabla_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR1 Enable Band5", IIR1, BAND5, 1, 0,
	tabla_get_iir_enable_audio_mixer, tabla_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR2 Enable Band1", IIR2, BAND1, 1, 0,
	tabla_get_iir_enable_audio_mixer, tabla_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR2 Enable Band2", IIR2, BAND2, 1, 0,
	tabla_get_iir_enable_audio_mixer, tabla_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR2 Enable Band3", IIR2, BAND3, 1, 0,
	tabla_get_iir_enable_audio_mixer, tabla_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR2 Enable Band4", IIR2, BAND4, 1, 0,
	tabla_get_iir_enable_audio_mixer, tabla_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR2 Enable Band5", IIR2, BAND5, 1, 0,
	tabla_get_iir_enable_audio_mixer, tabla_put_iir_enable_audio_mixer),

	SOC_SINGLE_MULTI_EXT("IIR1 Band1", IIR1, BAND1, 255, 0, 5,
	tabla_get_iir_band_audio_mixer, tabla_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR1 Band2", IIR1, BAND2, 255, 0, 5,
	tabla_get_iir_band_audio_mixer, tabla_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR1 Band3", IIR1, BAND3, 255, 0, 5,
	tabla_get_iir_band_audio_mixer, tabla_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR1 Band4", IIR1, BAND4, 255, 0, 5,
	tabla_get_iir_band_audio_mixer, tabla_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR1 Band5", IIR1, BAND5, 255, 0, 5,
	tabla_get_iir_band_audio_mixer, tabla_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR2 Band1", IIR2, BAND1, 255, 0, 5,
	tabla_get_iir_band_audio_mixer, tabla_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR2 Band2", IIR2, BAND2, 255, 0, 5,
	tabla_get_iir_band_audio_mixer, tabla_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR2 Band3", IIR2, BAND3, 255, 0, 5,
	tabla_get_iir_band_audio_mixer, tabla_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR2 Band4", IIR2, BAND4, 255, 0, 5,
	tabla_get_iir_band_audio_mixer, tabla_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR2 Band5", IIR2, BAND5, 255, 0, 5,
	tabla_get_iir_band_audio_mixer, tabla_put_iir_band_audio_mixer),
};

static const char *rx_mix1_text[] = {
	"ZERO", "SRC1", "SRC2", "IIR1", "IIR2", "RX1", "RX2", "RX3", "RX4",
		"RX5", "RX6", "RX7"
};

static const char *rx_dsm_text[] = {
	"CIC_OUT", "DSM_INV"
};

static const char *sb_tx1_mux_text[] = {
	"ZERO", "RMIX1", "RMIX2", "RMIX3", "RMIX4", "RMIX5", "RMIX6", "RMIX7",
		"DEC1"
};

static const char *sb_tx5_mux_text[] = {
	"ZERO", "RMIX1", "RMIX2", "RMIX3", "RMIX4", "RMIX5", "RMIX6", "RMIX7",
		"DEC5"
};

static const char *sb_tx6_mux_text[] = {
	"ZERO", "RMIX1", "RMIX2", "RMIX3", "RMIX4", "RMIX5", "RMIX6", "RMIX7",
		"DEC6"
};

static const char const *sb_tx7_to_tx10_mux_text[] = {
	"ZERO", "RMIX1", "RMIX2", "RMIX3", "RMIX4", "RMIX5", "RMIX6", "RMIX7",
		"DEC1", "DEC2", "DEC3", "DEC4", "DEC5", "DEC6", "DEC7", "DEC8",
		"DEC9", "DEC10"
};

static const char *dec1_mux_text[] = {
	"ZERO", "DMIC1", "ADC6",
};

static const char *dec2_mux_text[] = {
	"ZERO", "DMIC2", "ADC5",
};

static const char *dec3_mux_text[] = {
	"ZERO", "DMIC3", "ADC4",
};

static const char *dec4_mux_text[] = {
	"ZERO", "DMIC4", "ADC3",
};

static const char *dec5_mux_text[] = {
	"ZERO", "DMIC5", "ADC2",
};

static const char *dec6_mux_text[] = {
	"ZERO", "DMIC6", "ADC1",
};

static const char const *dec7_mux_text[] = {
	"ZERO", "DMIC1", "DMIC6", "ADC1", "ADC6", "ANC1_FB", "ANC2_FB",
};

static const char *dec8_mux_text[] = {
	"ZERO", "DMIC2", "DMIC5", "ADC2", "ADC5",
};

static const char *dec9_mux_text[] = {
	"ZERO", "DMIC4", "DMIC5", "ADC2", "ADC3", "ADCMB", "ANC1_FB", "ANC2_FB",
};

static const char *dec10_mux_text[] = {
	"ZERO", "DMIC3", "DMIC6", "ADC1", "ADC4", "ADCMB", "ANC1_FB", "ANC2_FB",
};

static const char const *anc_mux_text[] = {
	"ZERO", "ADC1", "ADC2", "ADC3", "ADC4", "ADC5", "ADC6", "ADC_MB",
		"RSVD_1", "DMIC1", "DMIC2", "DMIC3", "DMIC4", "DMIC5", "DMIC6"
};

static const char const *anc1_fb_mux_text[] = {
	"ZERO", "EAR_HPH_L", "EAR_LINE_1",
};

static const char *iir1_inp1_text[] = {
	"ZERO", "DEC1", "DEC2", "DEC3", "DEC4", "DEC5", "DEC6", "DEC7", "DEC8",
	"DEC9", "DEC10", "RX1", "RX2", "RX3", "RX4", "RX5", "RX6", "RX7"
};

static const struct soc_enum rx_mix1_inp1_chain_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_RX1_B1_CTL, 0, 12, rx_mix1_text);

static const struct soc_enum rx_mix1_inp2_chain_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_RX1_B1_CTL, 4, 12, rx_mix1_text);

static const struct soc_enum rx2_mix1_inp1_chain_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_RX2_B1_CTL, 0, 12, rx_mix1_text);

static const struct soc_enum rx2_mix1_inp2_chain_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_RX2_B1_CTL, 4, 12, rx_mix1_text);

static const struct soc_enum rx3_mix1_inp1_chain_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_RX3_B1_CTL, 0, 12, rx_mix1_text);

static const struct soc_enum rx3_mix1_inp2_chain_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_RX3_B1_CTL, 4, 12, rx_mix1_text);

static const struct soc_enum rx4_mix1_inp1_chain_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_RX4_B1_CTL, 0, 12, rx_mix1_text);

static const struct soc_enum rx4_mix1_inp2_chain_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_RX4_B1_CTL, 4, 12, rx_mix1_text);

static const struct soc_enum rx5_mix1_inp1_chain_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_RX5_B1_CTL, 0, 12, rx_mix1_text);

static const struct soc_enum rx5_mix1_inp2_chain_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_RX5_B1_CTL, 4, 12, rx_mix1_text);

static const struct soc_enum rx6_mix1_inp1_chain_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_RX6_B1_CTL, 0, 12, rx_mix1_text);

static const struct soc_enum rx6_mix1_inp2_chain_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_RX6_B1_CTL, 4, 12, rx_mix1_text);

static const struct soc_enum rx7_mix1_inp1_chain_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_RX7_B1_CTL, 0, 12, rx_mix1_text);

static const struct soc_enum rx7_mix1_inp2_chain_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_RX7_B1_CTL, 4, 12, rx_mix1_text);

static const struct soc_enum rx4_dsm_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_RX4_B6_CTL, 4, 2, rx_dsm_text);

static const struct soc_enum rx6_dsm_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_RX6_B6_CTL, 4, 2, rx_dsm_text);

static const struct soc_enum sb_tx5_mux_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_TX_SB_B5_CTL, 0, 9, sb_tx5_mux_text);

static const struct soc_enum sb_tx6_mux_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_TX_SB_B6_CTL, 0, 9, sb_tx6_mux_text);

static const struct soc_enum sb_tx7_mux_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_TX_SB_B7_CTL, 0, 18,
			sb_tx7_to_tx10_mux_text);

static const struct soc_enum sb_tx8_mux_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_TX_SB_B8_CTL, 0, 18,
			sb_tx7_to_tx10_mux_text);

static const struct soc_enum sb_tx1_mux_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_TX_SB_B1_CTL, 0, 9, sb_tx1_mux_text);

static const struct soc_enum dec1_mux_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_TX_B1_CTL, 0, 3, dec1_mux_text);

static const struct soc_enum dec2_mux_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_TX_B1_CTL, 2, 3, dec2_mux_text);

static const struct soc_enum dec3_mux_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_TX_B1_CTL, 4, 3, dec3_mux_text);

static const struct soc_enum dec4_mux_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_TX_B1_CTL, 6, 3, dec4_mux_text);

static const struct soc_enum dec5_mux_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_TX_B2_CTL, 0, 3, dec5_mux_text);

static const struct soc_enum dec6_mux_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_TX_B2_CTL, 2, 3, dec6_mux_text);

static const struct soc_enum dec7_mux_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_TX_B2_CTL, 4, 7, dec7_mux_text);

static const struct soc_enum dec8_mux_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_TX_B3_CTL, 0, 7, dec8_mux_text);

static const struct soc_enum dec9_mux_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_TX_B3_CTL, 3, 8, dec9_mux_text);

static const struct soc_enum dec10_mux_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_TX_B4_CTL, 0, 8, dec10_mux_text);

static const struct soc_enum anc1_mux_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_ANC_B1_CTL, 0, 16, anc_mux_text);

static const struct soc_enum anc2_mux_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_ANC_B1_CTL, 4, 16, anc_mux_text);

static const struct soc_enum anc1_fb_mux_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_ANC_B2_CTL, 0, 3, anc1_fb_mux_text);

static const struct soc_enum iir1_inp1_mux_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_EQ1_B1_CTL, 0, 18, iir1_inp1_text);

static const struct snd_kcontrol_new rx_mix1_inp1_mux =
	SOC_DAPM_ENUM("RX1 MIX1 INP1 Mux", rx_mix1_inp1_chain_enum);

static const struct snd_kcontrol_new rx_mix1_inp2_mux =
	SOC_DAPM_ENUM("RX1 MIX1 INP2 Mux", rx_mix1_inp2_chain_enum);

static const struct snd_kcontrol_new rx2_mix1_inp1_mux =
	SOC_DAPM_ENUM("RX2 MIX1 INP1 Mux", rx2_mix1_inp1_chain_enum);

static const struct snd_kcontrol_new rx2_mix1_inp2_mux =
	SOC_DAPM_ENUM("RX2 MIX1 INP2 Mux", rx2_mix1_inp2_chain_enum);

static const struct snd_kcontrol_new rx3_mix1_inp1_mux =
	SOC_DAPM_ENUM("RX3 MIX1 INP1 Mux", rx3_mix1_inp1_chain_enum);

static const struct snd_kcontrol_new rx3_mix1_inp2_mux =
	SOC_DAPM_ENUM("RX3 MIX1 INP2 Mux", rx3_mix1_inp2_chain_enum);

static const struct snd_kcontrol_new rx4_mix1_inp1_mux =
	SOC_DAPM_ENUM("RX4 MIX1 INP1 Mux", rx4_mix1_inp1_chain_enum);

static const struct snd_kcontrol_new rx4_mix1_inp2_mux =
	SOC_DAPM_ENUM("RX4 MIX1 INP2 Mux", rx4_mix1_inp2_chain_enum);

static const struct snd_kcontrol_new rx5_mix1_inp1_mux =
	SOC_DAPM_ENUM("RX5 MIX1 INP1 Mux", rx5_mix1_inp1_chain_enum);

static const struct snd_kcontrol_new rx5_mix1_inp2_mux =
	SOC_DAPM_ENUM("RX5 MIX1 INP2 Mux", rx5_mix1_inp2_chain_enum);

static const struct snd_kcontrol_new rx6_mix1_inp1_mux =
	SOC_DAPM_ENUM("RX6 MIX1 INP1 Mux", rx6_mix1_inp1_chain_enum);

static const struct snd_kcontrol_new rx6_mix1_inp2_mux =
	SOC_DAPM_ENUM("RX6 MIX1 INP2 Mux", rx6_mix1_inp2_chain_enum);

static const struct snd_kcontrol_new rx7_mix1_inp1_mux =
	SOC_DAPM_ENUM("RX7 MIX1 INP1 Mux", rx7_mix1_inp1_chain_enum);

static const struct snd_kcontrol_new rx7_mix1_inp2_mux =
	SOC_DAPM_ENUM("RX7 MIX1 INP2 Mux", rx7_mix1_inp2_chain_enum);

static const struct snd_kcontrol_new rx4_dsm_mux =
	SOC_DAPM_ENUM("RX4 DSM MUX Mux", rx4_dsm_enum);

static const struct snd_kcontrol_new rx6_dsm_mux =
	SOC_DAPM_ENUM("RX6 DSM MUX Mux", rx6_dsm_enum);

static const struct snd_kcontrol_new sb_tx5_mux =
	SOC_DAPM_ENUM("SLIM TX5 MUX Mux", sb_tx5_mux_enum);

static const struct snd_kcontrol_new sb_tx6_mux =
	SOC_DAPM_ENUM("SLIM TX6 MUX Mux", sb_tx6_mux_enum);

static const struct snd_kcontrol_new sb_tx7_mux =
	SOC_DAPM_ENUM("SLIM TX7 MUX Mux", sb_tx7_mux_enum);

static const struct snd_kcontrol_new sb_tx8_mux =
	SOC_DAPM_ENUM("SLIM TX8 MUX Mux", sb_tx8_mux_enum);

static const struct snd_kcontrol_new sb_tx1_mux =
	SOC_DAPM_ENUM("SLIM TX1 MUX Mux", sb_tx1_mux_enum);

static const struct snd_kcontrol_new dec1_mux =
	SOC_DAPM_ENUM("DEC1 MUX Mux", dec1_mux_enum);

static const struct snd_kcontrol_new dec2_mux =
	SOC_DAPM_ENUM("DEC2 MUX Mux", dec2_mux_enum);

static const struct snd_kcontrol_new dec3_mux =
	SOC_DAPM_ENUM("DEC3 MUX Mux", dec3_mux_enum);

static const struct snd_kcontrol_new dec4_mux =
	SOC_DAPM_ENUM("DEC4 MUX Mux", dec4_mux_enum);

static const struct snd_kcontrol_new dec5_mux =
	SOC_DAPM_ENUM("DEC5 MUX Mux", dec5_mux_enum);

static const struct snd_kcontrol_new dec6_mux =
	SOC_DAPM_ENUM("DEC6 MUX Mux", dec6_mux_enum);

static const struct snd_kcontrol_new dec7_mux =
	SOC_DAPM_ENUM("DEC7 MUX Mux", dec7_mux_enum);

static const struct snd_kcontrol_new anc1_mux =
	SOC_DAPM_ENUM("ANC1 MUX Mux", anc1_mux_enum);
static const struct snd_kcontrol_new dec8_mux =
	SOC_DAPM_ENUM("DEC8 MUX Mux", dec8_mux_enum);

static const struct snd_kcontrol_new dec9_mux =
	SOC_DAPM_ENUM("DEC9 MUX Mux", dec9_mux_enum);

static const struct snd_kcontrol_new dec10_mux =
	SOC_DAPM_ENUM("DEC10 MUX Mux", dec10_mux_enum);

static const struct snd_kcontrol_new iir1_inp1_mux =
	SOC_DAPM_ENUM("IIR1 INP1 Mux", iir1_inp1_mux_enum);

static const struct snd_kcontrol_new anc2_mux =
	SOC_DAPM_ENUM("ANC2 MUX Mux", anc2_mux_enum);

static const struct snd_kcontrol_new anc1_fb_mux =
	SOC_DAPM_ENUM("ANC1 FB MUX Mux", anc1_fb_mux_enum);

static const struct snd_kcontrol_new dac1_switch[] = {
	SOC_DAPM_SINGLE("Switch", TABLA_A_RX_EAR_EN, 5, 1, 0)
};
static const struct snd_kcontrol_new hphl_switch[] = {
	SOC_DAPM_SINGLE("Switch", TABLA_A_RX_HPH_L_DAC_CTL, 6, 1, 0)
};

static const struct snd_kcontrol_new lineout3_ground_switch =
	SOC_DAPM_SINGLE("Switch", TABLA_A_RX_LINE_3_DAC_CTL, 6, 1, 0);

static const struct snd_kcontrol_new lineout4_ground_switch =
	SOC_DAPM_SINGLE("Switch", TABLA_A_RX_LINE_4_DAC_CTL, 6, 1, 0);

static void tabla_codec_enable_adc_block(struct snd_soc_codec *codec,
	int enable)
{
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s %d\n", __func__, enable);

	if (enable) {
		tabla->adc_count++;
		snd_soc_update_bits(codec, TABLA_A_TX_COM_BIAS, 0xE0, 0xE0);
		snd_soc_update_bits(codec, TABLA_A_CDC_CLK_OTHR_CTL, 0x2, 0x2);
	} else {
		tabla->adc_count--;
		if (!tabla->adc_count) {
			snd_soc_update_bits(codec, TABLA_A_CDC_CLK_OTHR_CTL,
				0x2, 0x0);
			if (!tabla->mbhc_polling_active)
				snd_soc_update_bits(codec, TABLA_A_TX_COM_BIAS,
					0xE0, 0x0);
		}
	}
}

static int tabla_codec_enable_adc(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	u16 adc_reg;
	u8 init_bit_shift;

	pr_debug("%s %d\n", __func__, event);

	if (w->reg == TABLA_A_TX_1_2_EN)
		adc_reg = TABLA_A_TX_1_2_TEST_CTL;
	else if (w->reg == TABLA_A_TX_3_4_EN)
		adc_reg = TABLA_A_TX_3_4_TEST_CTL;
	else if (w->reg == TABLA_A_TX_5_6_EN)
		adc_reg = TABLA_A_TX_5_6_TEST_CTL;
	else {
		pr_err("%s: Error, invalid adc register\n", __func__);
		return -EINVAL;
	}

	if (w->shift == 3)
		init_bit_shift = 6;
	else if  (w->shift == 7)
		init_bit_shift = 7;
	else {
		pr_err("%s: Error, invalid init bit postion adc register\n",
				__func__);
		return -EINVAL;
	}



	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		tabla_codec_enable_adc_block(codec, 1);
		snd_soc_update_bits(codec, adc_reg, 1 << init_bit_shift,
				1 << init_bit_shift);
		break;
	case SND_SOC_DAPM_POST_PMU:

		snd_soc_update_bits(codec, adc_reg, 1 << init_bit_shift, 0x00);

		break;
	case SND_SOC_DAPM_POST_PMD:
		tabla_codec_enable_adc_block(codec, 0);
		break;
	}
	return 0;
}

static int tabla_codec_enable_lineout(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	u16 lineout_gain_reg;

	pr_debug("%s %d %s\n", __func__, event, w->name);

	switch (w->shift) {
	case 0:
		lineout_gain_reg = TABLA_A_RX_LINE_1_GAIN;
		break;
	case 1:
		lineout_gain_reg = TABLA_A_RX_LINE_2_GAIN;
		break;
	case 2:
		lineout_gain_reg = TABLA_A_RX_LINE_3_GAIN;
		break;
	case 3:
		lineout_gain_reg = TABLA_A_RX_LINE_4_GAIN;
		break;
	case 4:
		lineout_gain_reg = TABLA_A_RX_LINE_5_GAIN;
		break;
	default:
		pr_err("%s: Error, incorrect lineout register value\n",
			__func__);
		return -EINVAL;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, lineout_gain_reg, 0x40, 0x40);
		break;
	case SND_SOC_DAPM_POST_PMU:
		pr_debug("%s: sleeping 16 ms after %s PA turn on\n",
				__func__, w->name);
		usleep_range(16000, 16000);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, lineout_gain_reg, 0x40, 0x00);
		break;
	}
	return 0;
}


static int tabla_codec_enable_dmic(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	u16 tx_mux_ctl_reg, tx_dmic_ctl_reg;
	u8 dmic_clk_sel, dmic_clk_en;
	unsigned int dmic;
	int ret;

	ret = kstrtouint(strpbrk(w->name, "123456"), 10, &dmic);
	if (ret < 0) {
		pr_err("%s: Invalid DMIC line on the codec\n", __func__);
		return -EINVAL;
	}

	switch (dmic) {
	case 1:
	case 2:
		dmic_clk_sel = 0x02;
		dmic_clk_en = 0x01;
		break;

	case 3:
	case 4:
		dmic_clk_sel = 0x08;
		dmic_clk_en = 0x04;
		break;

	case 5:
	case 6:
		dmic_clk_sel = 0x20;
		dmic_clk_en = 0x10;
		break;

	default:
		pr_err("%s: Invalid DMIC Selection\n", __func__);
		return -EINVAL;
	}

	tx_mux_ctl_reg = TABLA_A_CDC_TX1_MUX_CTL + 8 * (dmic - 1);
	tx_dmic_ctl_reg = TABLA_A_CDC_TX1_DMIC_CTL + 8 * (dmic - 1);

	pr_debug("%s %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, tx_mux_ctl_reg, 0x1, 0x1);

		snd_soc_update_bits(codec, TABLA_A_CDC_CLK_DMIC_CTL,
				dmic_clk_sel, dmic_clk_sel);

		snd_soc_update_bits(codec, tx_dmic_ctl_reg, 0x1, 0x1);

		snd_soc_update_bits(codec, TABLA_A_CDC_CLK_DMIC_CTL,
				dmic_clk_en, dmic_clk_en);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, TABLA_A_CDC_CLK_DMIC_CTL,
				dmic_clk_en, 0);
		break;
	}
	return 0;
}

static int tabla_codec_enable_anc(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	const char *filename;
	const struct firmware *fw;
	int i;
	int ret;
	int num_anc_slots;
	struct anc_header *anc_head;
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);
	u32 anc_writes_size = 0;
	int anc_size_remaining;
	u32 *anc_ptr;
	u16 reg;
	u8 mask, val, old_val;

	pr_debug("%s %d\n", __func__, event);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:

		filename = "wcd9310/wcd9310_anc.bin";

		ret = request_firmware(&fw, filename, codec->dev);
		if (ret != 0) {
			dev_err(codec->dev, "Failed to acquire ANC data: %d\n",
				ret);
			return -ENODEV;
		}

		if (fw->size < sizeof(struct anc_header)) {
			dev_err(codec->dev, "Not enough data\n");
			release_firmware(fw);
			return -ENOMEM;
		}

		/* First number is the number of register writes */
		anc_head = (struct anc_header *)(fw->data);
		anc_ptr = (u32 *)((u32)fw->data + sizeof(struct anc_header));
		anc_size_remaining = fw->size - sizeof(struct anc_header);
		num_anc_slots = anc_head->num_anc_slots;

		if (tabla->anc_slot >= num_anc_slots) {
			dev_err(codec->dev, "Invalid ANC slot selected\n");
			release_firmware(fw);
			return -EINVAL;
		}

		for (i = 0; i < num_anc_slots; i++) {

			if (anc_size_remaining < TABLA_PACKED_REG_SIZE) {
				dev_err(codec->dev, "Invalid register format\n");
				release_firmware(fw);
				return -EINVAL;
			}
			anc_writes_size = (u32)(*anc_ptr);
			anc_size_remaining -= sizeof(u32);
			anc_ptr += 1;

			if (anc_writes_size * TABLA_PACKED_REG_SIZE
				> anc_size_remaining) {
				dev_err(codec->dev, "Invalid register format\n");
				release_firmware(fw);
				return -ENOMEM;
			}

			if (tabla->anc_slot == i)
				break;

			anc_size_remaining -= (anc_writes_size *
				TABLA_PACKED_REG_SIZE);
			anc_ptr += anc_writes_size;
		}
		if (i == num_anc_slots) {
			dev_err(codec->dev, "Selected ANC slot not present\n");
			release_firmware(fw);
			return -ENOMEM;
		}

		for (i = 0; i < anc_writes_size; i++) {
			TABLA_CODEC_UNPACK_ENTRY(anc_ptr[i], reg,
				mask, val);
			old_val = snd_soc_read(codec, reg);
			snd_soc_write(codec, reg, (old_val & ~mask) |
				(val & mask));
		}
		release_firmware(fw);

		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_write(codec, TABLA_A_CDC_CLK_ANC_RESET_CTL, 0xFF);
		snd_soc_write(codec, TABLA_A_CDC_CLK_ANC_CLK_EN_CTL, 0);
		break;
	}
	return 0;
}


static void tabla_codec_disable_button_presses(struct snd_soc_codec *codec)
{
	snd_soc_write(codec, TABLA_A_CDC_MBHC_VOLT_B4_CTL, 0x80);
	snd_soc_write(codec, TABLA_A_CDC_MBHC_VOLT_B3_CTL, 0x00);
}

static void tabla_codec_start_hs_polling(struct snd_soc_codec *codec)
{
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);

	snd_soc_write(codec, TABLA_A_MBHC_SCALING_MUX_1, 0x84);
	tabla_enable_irq(codec->control_data, TABLA_IRQ_MBHC_REMOVAL);
	if (!tabla->no_mic_headset_override) {
		tabla_enable_irq(codec->control_data, TABLA_IRQ_MBHC_POTENTIAL);
		tabla_enable_irq(codec->control_data, TABLA_IRQ_MBHC_RELEASE);
	} else {
		tabla_codec_disable_button_presses(codec);
	}
	snd_soc_write(codec, TABLA_A_CDC_MBHC_EN_CTL, 0x1);
	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_CLK_CTL, 0x8, 0x0);
	snd_soc_write(codec, TABLA_A_CDC_MBHC_EN_CTL, 0x1);
}

static void tabla_codec_pause_hs_polling(struct snd_soc_codec *codec)
{
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);

	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_CLK_CTL, 0x8, 0x8);
	tabla_disable_irq(codec->control_data, TABLA_IRQ_MBHC_REMOVAL);
	if (!tabla->no_mic_headset_override) {
		tabla_disable_irq(codec->control_data,
			TABLA_IRQ_MBHC_POTENTIAL);
		tabla_disable_irq(codec->control_data, TABLA_IRQ_MBHC_RELEASE);
	}
}

static void tabla_codec_switch_cfilt_mode(struct snd_soc_codec *codec,
		int mode)
{
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);
	u8 reg_mode_val, cur_mode_val;
	bool mbhc_was_polling = false;

	if (mode)
		reg_mode_val = TABLA_CFILT_FAST_MODE;
	else
		reg_mode_val = TABLA_CFILT_SLOW_MODE;

	cur_mode_val = snd_soc_read(codec,
					tabla->mbhc_bias_regs.cfilt_ctl) & 0x40;

	if (cur_mode_val != reg_mode_val) {
		if (tabla->mbhc_polling_active) {
			tabla_codec_pause_hs_polling(codec);
			mbhc_was_polling = true;
		}
		snd_soc_update_bits(codec,
			tabla->mbhc_bias_regs.cfilt_ctl, 0x40, reg_mode_val);
		if (mbhc_was_polling)
			tabla_codec_start_hs_polling(codec);
		pr_debug("%s: CFILT mode change (%x to %x)\n", __func__,
			cur_mode_val, reg_mode_val);
	} else {
		pr_debug("%s: CFILT Value is already %x\n",
			__func__, cur_mode_val);
	}
}

static void tabla_codec_update_cfilt_usage(struct snd_soc_codec *codec,
					   u8 cfilt_sel, int inc)
{
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);
	u32 *cfilt_cnt_ptr = NULL;
	u16 micb_cfilt_reg;

	switch (cfilt_sel) {
	case TABLA_CFILT1_SEL:
		cfilt_cnt_ptr = &tabla->cfilt1_cnt;
		micb_cfilt_reg = TABLA_A_MICB_CFILT_1_CTL;
		break;
	case TABLA_CFILT2_SEL:
		cfilt_cnt_ptr = &tabla->cfilt2_cnt;
		micb_cfilt_reg = TABLA_A_MICB_CFILT_2_CTL;
		break;
	case TABLA_CFILT3_SEL:
		cfilt_cnt_ptr = &tabla->cfilt3_cnt;
		micb_cfilt_reg = TABLA_A_MICB_CFILT_3_CTL;
		break;
	default:
		return; /* should not happen */
	}

	if (inc) {
		if (!(*cfilt_cnt_ptr)++) {
			/* Switch CFILT to slow mode if MBHC CFILT being used */
			if (cfilt_sel == tabla->mbhc_bias_regs.cfilt_sel)
				tabla_codec_switch_cfilt_mode(codec, 0);

			snd_soc_update_bits(codec, micb_cfilt_reg, 0x80, 0x80);
		}
	} else {
		/* check if count not zero, decrement
		 * then check if zero, go ahead disable cfilter
		 */
		if ((*cfilt_cnt_ptr) && !--(*cfilt_cnt_ptr)) {
			snd_soc_update_bits(codec, micb_cfilt_reg, 0x80, 0);

			/* Switch CFILT to fast mode if MBHC CFILT being used */
			if (cfilt_sel == tabla->mbhc_bias_regs.cfilt_sel)
				tabla_codec_switch_cfilt_mode(codec, 1);
		}
	}
}

static int tabla_find_k_value(unsigned int ldoh_v, unsigned int cfilt_mv)
{
	int rc = -EINVAL;
	unsigned min_mv, max_mv;

	switch (ldoh_v) {
	case TABLA_LDOH_1P95_V:
		min_mv = 160;
		max_mv = 1800;
		break;
	case TABLA_LDOH_2P35_V:
		min_mv = 200;
		max_mv = 2200;
		break;
	case TABLA_LDOH_2P75_V:
		min_mv = 240;
		max_mv = 2600;
		break;
	case TABLA_LDOH_2P85_V:
		min_mv = 250;
		max_mv = 2700;
		break;
	default:
		goto done;
	}

	if (cfilt_mv < min_mv || cfilt_mv > max_mv)
		goto done;

	for (rc = 4; rc <= 44; rc++) {
		min_mv = max_mv * (rc) / 44;
		if (min_mv >= cfilt_mv) {
			rc -= 4;
			break;
		}
	}
done:
	return rc;
}

static bool tabla_is_hph_pa_on(struct snd_soc_codec *codec)
{
	u8 hph_reg_val = 0;
	hph_reg_val = snd_soc_read(codec, TABLA_A_RX_HPH_CNP_EN);

	return (hph_reg_val & 0x30) ? true : false;
}

static bool tabla_is_hph_dac_on(struct snd_soc_codec *codec, int left)
{
	u8 hph_reg_val = 0;
	if (left)
		hph_reg_val = snd_soc_read(codec,
					   TABLA_A_RX_HPH_L_DAC_CTL);
	else
		hph_reg_val = snd_soc_read(codec,
					   TABLA_A_RX_HPH_R_DAC_CTL);

	return (hph_reg_val & 0xC0) ? true : false;
}

static void tabla_codec_switch_micbias(struct snd_soc_codec *codec,
	int vddio_switch)
{
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);
	int cfilt_k_val;
	bool mbhc_was_polling =  false;

	switch (vddio_switch) {
	case 1:
		if (tabla->mbhc_polling_active) {

			tabla_codec_pause_hs_polling(codec);
			/* Enable Mic Bias switch to VDDIO */
			tabla->cfilt_k_value = snd_soc_read(codec,
					tabla->mbhc_bias_regs.cfilt_val);
			cfilt_k_val = tabla_find_k_value(
					tabla->pdata->micbias.ldoh_v, 1800);
			snd_soc_update_bits(codec,
				tabla->mbhc_bias_regs.cfilt_val,
				0xFC, (cfilt_k_val << 2));

			snd_soc_update_bits(codec,
				tabla->mbhc_bias_regs.mbhc_reg,	0x80, 0x80);
			snd_soc_update_bits(codec,
				tabla->mbhc_bias_regs.mbhc_reg,	0x10, 0x00);
			tabla_codec_start_hs_polling(codec);

			tabla->mbhc_micbias_switched = true;
			pr_debug("%s: Enabled MBHC Mic bias to VDDIO Switch\n",
				__func__);
		}
		break;

	case 0:
		if (tabla->mbhc_micbias_switched) {
			if (tabla->mbhc_polling_active) {
				tabla_codec_pause_hs_polling(codec);
				mbhc_was_polling = true;
			}
			/* Disable Mic Bias switch to VDDIO */
			if (tabla->cfilt_k_value != 0)
				snd_soc_update_bits(codec,
					tabla->mbhc_bias_regs.cfilt_val, 0XFC,
					tabla->cfilt_k_value);
			snd_soc_update_bits(codec,
				tabla->mbhc_bias_regs.mbhc_reg,	0x80, 0x00);
			snd_soc_update_bits(codec,
				tabla->mbhc_bias_regs.mbhc_reg,	0x10, 0x00);

			if (mbhc_was_polling)
				tabla_codec_start_hs_polling(codec);

			tabla->mbhc_micbias_switched = false;
			pr_debug("%s: Disabled MBHC Mic bias to VDDIO Switch\n",
				__func__);
		}
		break;
	}
}

static int tabla_codec_enable_micbias(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);
	u16 micb_int_reg;
	int micb_line;
	u8 cfilt_sel_val = 0;
	char *internal1_text = "Internal1";
	char *internal2_text = "Internal2";
	char *internal3_text = "Internal3";

	pr_debug("%s %d\n", __func__, event);
	switch (w->reg) {
	case TABLA_A_MICB_1_CTL:
		micb_int_reg = TABLA_A_MICB_1_INT_RBIAS;
		cfilt_sel_val = tabla->pdata->micbias.bias1_cfilt_sel;
		micb_line = TABLA_MICBIAS1;
		break;
	case TABLA_A_MICB_2_CTL:
		micb_int_reg = TABLA_A_MICB_2_INT_RBIAS;
		cfilt_sel_val = tabla->pdata->micbias.bias2_cfilt_sel;
		micb_line = TABLA_MICBIAS2;
		break;
	case TABLA_A_MICB_3_CTL:
		micb_int_reg = TABLA_A_MICB_3_INT_RBIAS;
		cfilt_sel_val = tabla->pdata->micbias.bias3_cfilt_sel;
		micb_line = TABLA_MICBIAS3;
		break;
	case TABLA_A_MICB_4_CTL:
		micb_int_reg = TABLA_A_MICB_4_INT_RBIAS;
		cfilt_sel_val = tabla->pdata->micbias.bias4_cfilt_sel;
		micb_line = TABLA_MICBIAS4;
		break;
	default:
		pr_err("%s: Error, invalid micbias register\n", __func__);
		return -EINVAL;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Decide whether to switch the micbias for MBHC */
		if ((w->reg == tabla->mbhc_bias_regs.ctl_reg)
				&& tabla->mbhc_micbias_switched)
			tabla_codec_switch_micbias(codec, 0);

		snd_soc_update_bits(codec, w->reg, 0x0E, 0x0A);
		tabla_codec_update_cfilt_usage(codec, cfilt_sel_val, 1);

		if (strnstr(w->name, internal1_text, 30))
			snd_soc_update_bits(codec, micb_int_reg, 0xE0, 0xE0);
		else if (strnstr(w->name, internal2_text, 30))
			snd_soc_update_bits(codec, micb_int_reg, 0x1C, 0x1C);
		else if (strnstr(w->name, internal3_text, 30))
			snd_soc_update_bits(codec, micb_int_reg, 0x3, 0x3);

		break;
	case SND_SOC_DAPM_POST_PMU:
		if (tabla->mbhc_polling_active &&
			(tabla->calibration->bias == micb_line)) {
			tabla_codec_pause_hs_polling(codec);
			tabla_codec_start_hs_polling(codec);
		}
		break;

	case SND_SOC_DAPM_POST_PMD:

		if ((w->reg == tabla->mbhc_bias_regs.ctl_reg)
				&& tabla_is_hph_pa_on(codec))
			tabla_codec_switch_micbias(codec, 1);

		if (strnstr(w->name, internal1_text, 30))
			snd_soc_update_bits(codec, micb_int_reg, 0x80, 0x00);
		else if (strnstr(w->name, internal2_text, 30))
			snd_soc_update_bits(codec, micb_int_reg, 0x10, 0x00);
		else if (strnstr(w->name, internal3_text, 30))
			snd_soc_update_bits(codec, micb_int_reg, 0x2, 0x0);

		tabla_codec_update_cfilt_usage(codec, cfilt_sel_val, 0);
		break;
	}

	return 0;
}

static int tabla_codec_enable_dec(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	u16 dec_reset_reg;

	pr_debug("%s %d\n", __func__, event);

	if (w->reg == TABLA_A_CDC_CLK_TX_CLK_EN_B1_CTL)
		dec_reset_reg = TABLA_A_CDC_CLK_TX_RESET_B1_CTL;
	else if (w->reg == TABLA_A_CDC_CLK_TX_CLK_EN_B2_CTL)
		dec_reset_reg = TABLA_A_CDC_CLK_TX_RESET_B2_CTL;
	else {
		pr_err("%s: Error, incorrect dec\n", __func__);
		return -EINVAL;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, dec_reset_reg, 1 << w->shift,
			1 << w->shift);
		snd_soc_update_bits(codec, dec_reset_reg, 1 << w->shift, 0x0);
		break;
	}
	return 0;
}

static int tabla_codec_reset_interpolator(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	pr_debug("%s %d %s\n", __func__, event, w->name);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, TABLA_A_CDC_CLK_RX_RESET_CTL,
			1 << w->shift, 1 << w->shift);
		snd_soc_update_bits(codec, TABLA_A_CDC_CLK_RX_RESET_CTL,
			1 << w->shift, 0x0);
		break;
	}
	return 0;
}

static int tabla_codec_enable_ldo_h(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
	case SND_SOC_DAPM_POST_PMD:
		usleep_range(1000, 1000);
		break;
	}
	return 0;
}


static void tabla_enable_rx_bias(struct snd_soc_codec *codec, u32  enable)
{
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);

	if (enable) {
		tabla->rx_bias_count++;
		if (tabla->rx_bias_count == 1)
			snd_soc_update_bits(codec, TABLA_A_RX_COM_BIAS,
				0x80, 0x80);
	} else {
		tabla->rx_bias_count--;
		if (!tabla->rx_bias_count)
			snd_soc_update_bits(codec, TABLA_A_RX_COM_BIAS,
				0x80, 0x00);
	}
}

static int tabla_codec_enable_rx_bias(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	pr_debug("%s %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		tabla_enable_rx_bias(codec, 1);
		break;
	case SND_SOC_DAPM_POST_PMD:
		tabla_enable_rx_bias(codec, 0);
		break;
	}
	return 0;
}
static int tabla_hphr_dac_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	pr_debug("%s %s %d\n", __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, w->reg, 0x40, 0x40);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, w->reg, 0x40, 0x00);
		break;
	}
	return 0;
}

static void tabla_snd_soc_jack_report(struct tabla_priv *tabla,
				      struct snd_soc_jack *jack, int status,
				      int mask)
{
	/* XXX: wake_lock_timeout()? */
	snd_soc_jack_report(jack, status, mask);
}

static void hphocp_off_report(struct tabla_priv *tabla,
	u32 jack_status, int irq)
{
	struct snd_soc_codec *codec;

	if (tabla) {
		pr_info("%s: clear ocp status %x\n", __func__, jack_status);
		codec = tabla->codec;
		tabla->hph_status &= ~jack_status;
		if (tabla->headset_jack)
			tabla_snd_soc_jack_report(tabla, tabla->headset_jack,
						  tabla->hph_status,
						  TABLA_JACK_MASK);
		snd_soc_update_bits(codec, TABLA_A_RX_HPH_OCP_CTL, 0x10,
		0x00);
		snd_soc_update_bits(codec, TABLA_A_RX_HPH_OCP_CTL, 0x10,
		0x10);
		/* reset retry counter as PA is turned off signifying
		 * start of new OCP detection session
		 */
		if (TABLA_IRQ_HPH_PA_OCPL_FAULT)
			tabla->hphlocp_cnt = 0;
		else
			tabla->hphrocp_cnt = 0;
		tabla_enable_irq(codec->control_data, irq);
	} else {
		pr_err("%s: Bad tabla private data\n", __func__);
	}
}

static void hphlocp_off_report(struct work_struct *work)
{
	struct tabla_priv *tabla = container_of(work, struct tabla_priv,
		hphlocp_work);
	hphocp_off_report(tabla, SND_JACK_OC_HPHL, TABLA_IRQ_HPH_PA_OCPL_FAULT);
}

static void hphrocp_off_report(struct work_struct *work)
{
	struct tabla_priv *tabla = container_of(work, struct tabla_priv,
		hphrocp_work);
	hphocp_off_report(tabla, SND_JACK_OC_HPHR, TABLA_IRQ_HPH_PA_OCPR_FAULT);
}

static int tabla_hph_pa_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);
	u8 mbhc_micb_ctl_val;
	pr_debug("%s: event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		mbhc_micb_ctl_val = snd_soc_read(codec,
				tabla->mbhc_bias_regs.ctl_reg);

		if (!(mbhc_micb_ctl_val & 0x80)
				&& !tabla->mbhc_micbias_switched)
			tabla_codec_switch_micbias(codec, 1);

		break;

	case SND_SOC_DAPM_POST_PMD:
		/* schedule work is required because at the time HPH PA DAPM
		 * event callback is called by DAPM framework, CODEC dapm mutex
		 * would have been locked while snd_soc_jack_report also
		 * attempts to acquire same lock.
		 */
		if (w->shift == 5) {
			clear_bit(TABLA_HPHL_PA_OFF_ACK,
				  &tabla->hph_pa_dac_state);
			clear_bit(TABLA_HPHL_DAC_OFF_ACK,
				  &tabla->hph_pa_dac_state);
			if (tabla->hph_status & SND_JACK_OC_HPHL)
				schedule_work(&tabla->hphlocp_work);
		} else if (w->shift == 4) {
			clear_bit(TABLA_HPHR_PA_OFF_ACK,
				  &tabla->hph_pa_dac_state);
			clear_bit(TABLA_HPHR_DAC_OFF_ACK,
				  &tabla->hph_pa_dac_state);
			if (tabla->hph_status & SND_JACK_OC_HPHR)
				schedule_work(&tabla->hphrocp_work);
		}

		if (tabla->mbhc_micbias_switched)
			tabla_codec_switch_micbias(codec, 0);

		pr_debug("%s: sleep 10 ms after %s PA disable.\n", __func__,
				w->name);
		usleep_range(10000, 10000);

		break;
	}
	return 0;
}

static void tabla_get_mbhc_micbias_regs(struct snd_soc_codec *codec,
		struct mbhc_micbias_regs *micbias_regs)
{
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);
	struct tabla_mbhc_calibration *calibration = tabla->calibration;
	unsigned int cfilt;

	switch (calibration->bias) {
	case TABLA_MICBIAS1:
		cfilt = tabla->pdata->micbias.bias1_cfilt_sel;
		micbias_regs->mbhc_reg = TABLA_A_MICB_1_MBHC;
		micbias_regs->int_rbias = TABLA_A_MICB_1_INT_RBIAS;
		micbias_regs->ctl_reg = TABLA_A_MICB_1_CTL;
		break;
	case TABLA_MICBIAS2:
		cfilt = tabla->pdata->micbias.bias2_cfilt_sel;
		micbias_regs->mbhc_reg = TABLA_A_MICB_2_MBHC;
		micbias_regs->int_rbias = TABLA_A_MICB_2_INT_RBIAS;
		micbias_regs->ctl_reg = TABLA_A_MICB_2_CTL;
		break;
	case TABLA_MICBIAS3:
		cfilt = tabla->pdata->micbias.bias3_cfilt_sel;
		micbias_regs->mbhc_reg = TABLA_A_MICB_3_MBHC;
		micbias_regs->int_rbias = TABLA_A_MICB_3_INT_RBIAS;
		micbias_regs->ctl_reg = TABLA_A_MICB_3_CTL;
		break;
	case TABLA_MICBIAS4:
		cfilt = tabla->pdata->micbias.bias4_cfilt_sel;
		micbias_regs->mbhc_reg = TABLA_A_MICB_4_MBHC;
		micbias_regs->int_rbias = TABLA_A_MICB_4_INT_RBIAS;
		micbias_regs->ctl_reg = TABLA_A_MICB_4_CTL;
		break;
	default:
		/* Should never reach here */
		pr_err("%s: Invalid MIC BIAS for MBHC\n", __func__);
		return;
	}

	micbias_regs->cfilt_sel = cfilt;

	switch (cfilt) {
	case TABLA_CFILT1_SEL:
		micbias_regs->cfilt_val = TABLA_A_MICB_CFILT_1_VAL;
		micbias_regs->cfilt_ctl = TABLA_A_MICB_CFILT_1_CTL;
		break;
	case TABLA_CFILT2_SEL:
		micbias_regs->cfilt_val = TABLA_A_MICB_CFILT_2_VAL;
		micbias_regs->cfilt_ctl = TABLA_A_MICB_CFILT_2_CTL;
		break;
	case TABLA_CFILT3_SEL:
		micbias_regs->cfilt_val = TABLA_A_MICB_CFILT_3_VAL;
		micbias_regs->cfilt_ctl = TABLA_A_MICB_CFILT_3_CTL;
		break;
	}
}
static const struct snd_soc_dapm_widget tabla_dapm_i2s_widgets[] = {
	SND_SOC_DAPM_SUPPLY("RX_I2S_CLK", TABLA_A_CDC_CLK_RX_I2S_CTL,
	4, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("TX_I2S_CLK", TABLA_A_CDC_CLK_TX_I2S_CTL, 4,
	0, NULL, 0),
};

static int tabla_lineout_dac_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	pr_debug("%s %s %d\n", __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, w->reg, 0x40, 0x40);
		break;

	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, w->reg, 0x40, 0x00);
		break;
	}
	return 0;
}

static const struct snd_soc_dapm_widget tabla_dapm_widgets[] = {
	/*RX stuff */
	SND_SOC_DAPM_OUTPUT("EAR"),

	SND_SOC_DAPM_PGA("EAR PA", TABLA_A_RX_EAR_EN, 4, 0, NULL, 0),

	SND_SOC_DAPM_MIXER("DAC1", TABLA_A_RX_EAR_EN, 6, 0, dac1_switch,
		ARRAY_SIZE(dac1_switch)),

	SND_SOC_DAPM_AIF_IN("SLIM RX1", "AIF1 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("SLIM RX2", "AIF1 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("SLIM RX3", "AIF1 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("SLIM RX4", "AIF1 Playback", 0, SND_SOC_NOPM, 0, 0),

	/* Headphone */
	SND_SOC_DAPM_OUTPUT("HEADPHONE"),
	SND_SOC_DAPM_PGA_E("HPHL", TABLA_A_RX_HPH_CNP_EN, 5, 0, NULL, 0,
		tabla_hph_pa_event, SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER("HPHL DAC", TABLA_A_RX_HPH_L_DAC_CTL, 7, 0,
		hphl_switch, ARRAY_SIZE(hphl_switch)),

	SND_SOC_DAPM_PGA_E("HPHR", TABLA_A_RX_HPH_CNP_EN, 4, 0, NULL, 0,
		tabla_hph_pa_event, SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_DAC_E("HPHR DAC", NULL, TABLA_A_RX_HPH_R_DAC_CTL, 7, 0,
		tabla_hphr_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	/* Speaker */
	SND_SOC_DAPM_OUTPUT("LINEOUT1"),
	SND_SOC_DAPM_OUTPUT("LINEOUT2"),
	SND_SOC_DAPM_OUTPUT("LINEOUT3"),
	SND_SOC_DAPM_OUTPUT("LINEOUT4"),
	SND_SOC_DAPM_OUTPUT("LINEOUT5"),

	SND_SOC_DAPM_PGA_E("LINEOUT1 PA", TABLA_A_RX_LINE_CNP_EN, 0, 0, NULL,
			0, tabla_codec_enable_lineout, SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("LINEOUT2 PA", TABLA_A_RX_LINE_CNP_EN, 1, 0, NULL,
			0, tabla_codec_enable_lineout, SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("LINEOUT3 PA", TABLA_A_RX_LINE_CNP_EN, 2, 0, NULL,
			0, tabla_codec_enable_lineout, SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("LINEOUT4 PA", TABLA_A_RX_LINE_CNP_EN, 3, 0, NULL,
			0, tabla_codec_enable_lineout, SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("LINEOUT5 PA", TABLA_A_RX_LINE_CNP_EN, 4, 0, NULL, 0,
		tabla_codec_enable_lineout, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_DAC_E("LINEOUT1 DAC", NULL, TABLA_A_RX_LINE_1_DAC_CTL, 7, 0
		, tabla_lineout_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("LINEOUT2 DAC", NULL, TABLA_A_RX_LINE_2_DAC_CTL, 7, 0
		, tabla_lineout_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("LINEOUT3 DAC", NULL, TABLA_A_RX_LINE_3_DAC_CTL, 7, 0
		, tabla_lineout_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SWITCH("LINEOUT3 DAC GROUND", SND_SOC_NOPM, 0, 0,
				&lineout3_ground_switch),
	SND_SOC_DAPM_DAC_E("LINEOUT4 DAC", NULL, TABLA_A_RX_LINE_4_DAC_CTL, 7, 0
		, tabla_lineout_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SWITCH("LINEOUT4 DAC GROUND", SND_SOC_NOPM, 0, 0,
				&lineout4_ground_switch),
	SND_SOC_DAPM_DAC_E("LINEOUT5 DAC", NULL, TABLA_A_RX_LINE_5_DAC_CTL, 7, 0
		, tabla_lineout_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MIXER_E("RX1 MIX1", TABLA_A_CDC_CLK_RX_B1_CTL, 0, 0, NULL,
		0, tabla_codec_reset_interpolator, SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_MIXER_E("RX2 MIX1", TABLA_A_CDC_CLK_RX_B1_CTL, 1, 0, NULL,
		0, tabla_codec_reset_interpolator, SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_MIXER_E("RX3 MIX1", TABLA_A_CDC_CLK_RX_B1_CTL, 2, 0, NULL,
		0, tabla_codec_reset_interpolator, SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_MIXER_E("RX4 MIX1", TABLA_A_CDC_CLK_RX_B1_CTL, 3, 0, NULL,
		0, tabla_codec_reset_interpolator, SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_MIXER_E("RX5 MIX1", TABLA_A_CDC_CLK_RX_B1_CTL, 4, 0, NULL,
		0, tabla_codec_reset_interpolator, SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_MIXER_E("RX6 MIX1", TABLA_A_CDC_CLK_RX_B1_CTL, 5, 0, NULL,
		0, tabla_codec_reset_interpolator, SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_MIXER_E("RX7 MIX1", TABLA_A_CDC_CLK_RX_B1_CTL, 6, 0, NULL,
		0, tabla_codec_reset_interpolator, SND_SOC_DAPM_PRE_PMU),


	SND_SOC_DAPM_MUX_E("RX4 DSM MUX", TABLA_A_CDC_CLK_RX_B1_CTL, 3, 0,
		&rx4_dsm_mux, tabla_codec_reset_interpolator,
		SND_SOC_DAPM_PRE_PMU),

	SND_SOC_DAPM_MUX_E("RX6 DSM MUX", TABLA_A_CDC_CLK_RX_B1_CTL, 5, 0,
		&rx6_dsm_mux, tabla_codec_reset_interpolator,
		SND_SOC_DAPM_PRE_PMU),

	SND_SOC_DAPM_MIXER("RX1 CHAIN", TABLA_A_CDC_RX1_B6_CTL, 5, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX2 CHAIN", TABLA_A_CDC_RX2_B6_CTL, 5, 0, NULL, 0),

	SND_SOC_DAPM_MUX("RX1 MIX1 INP1", SND_SOC_NOPM, 0, 0,
		&rx_mix1_inp1_mux),
	SND_SOC_DAPM_MUX("RX1 MIX1 INP2", SND_SOC_NOPM, 0, 0,
		&rx_mix1_inp2_mux),
	SND_SOC_DAPM_MUX("RX2 MIX1 INP1", SND_SOC_NOPM, 0, 0,
		&rx2_mix1_inp1_mux),
	SND_SOC_DAPM_MUX("RX2 MIX1 INP2", SND_SOC_NOPM, 0, 0,
		&rx2_mix1_inp2_mux),
	SND_SOC_DAPM_MUX("RX3 MIX1 INP1", SND_SOC_NOPM, 0, 0,
		&rx3_mix1_inp1_mux),
	SND_SOC_DAPM_MUX("RX3 MIX1 INP2", SND_SOC_NOPM, 0, 0,
		&rx3_mix1_inp2_mux),
	SND_SOC_DAPM_MUX("RX4 MIX1 INP1", SND_SOC_NOPM, 0, 0,
		&rx4_mix1_inp1_mux),
	SND_SOC_DAPM_MUX("RX4 MIX1 INP2", SND_SOC_NOPM, 0, 0,
		&rx4_mix1_inp2_mux),
	SND_SOC_DAPM_MUX("RX5 MIX1 INP1", SND_SOC_NOPM, 0, 0,
		&rx5_mix1_inp1_mux),
	SND_SOC_DAPM_MUX("RX5 MIX1 INP2", SND_SOC_NOPM, 0, 0,
		&rx5_mix1_inp2_mux),
	SND_SOC_DAPM_MUX("RX6 MIX1 INP1", SND_SOC_NOPM, 0, 0,
		&rx6_mix1_inp1_mux),
	SND_SOC_DAPM_MUX("RX6 MIX1 INP2", SND_SOC_NOPM, 0, 0,
		&rx6_mix1_inp2_mux),
	SND_SOC_DAPM_MUX("RX7 MIX1 INP1", SND_SOC_NOPM, 0, 0,
		&rx7_mix1_inp1_mux),
	SND_SOC_DAPM_MUX("RX7 MIX1 INP2", SND_SOC_NOPM, 0, 0,
		&rx7_mix1_inp2_mux),

	SND_SOC_DAPM_SUPPLY("CP", TABLA_A_CP_EN, 0, 0,
		tabla_codec_enable_charge_pump, SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_SUPPLY("RX_BIAS", SND_SOC_NOPM, 0, 0,
		tabla_codec_enable_rx_bias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	/* TX */

	SND_SOC_DAPM_SUPPLY("CDC_CONN", TABLA_A_CDC_CLK_OTHR_CTL, 2, 0, NULL,
		0),

	SND_SOC_DAPM_SUPPLY("LDO_H", TABLA_A_LDO_H_MODE_1, 7, 0,
		tabla_codec_enable_ldo_h, SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_INPUT("AMIC1"),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS1 External", TABLA_A_MICB_1_CTL, 7, 0,
		tabla_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS1 Internal1", TABLA_A_MICB_1_CTL, 7, 0,
		tabla_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS1 Internal2", TABLA_A_MICB_1_CTL, 7, 0,
		tabla_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("ADC1", NULL, TABLA_A_TX_1_2_EN, 7, 0,
		tabla_codec_enable_adc, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_INPUT("AMIC3"),
	SND_SOC_DAPM_ADC_E("ADC3", NULL, TABLA_A_TX_3_4_EN, 7, 0,
		tabla_codec_enable_adc, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_INPUT("AMIC4"),
	SND_SOC_DAPM_ADC_E("ADC4", NULL, TABLA_A_TX_3_4_EN, 3, 0,
		tabla_codec_enable_adc, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MICBIAS_E("MIC BIAS4 External", TABLA_A_MICB_4_CTL, 7, 0,
		tabla_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_INPUT("AMIC5"),
	SND_SOC_DAPM_ADC_E("ADC5", NULL, TABLA_A_TX_5_6_EN, 7, 0,
		tabla_codec_enable_adc, SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_INPUT("AMIC6"),
	SND_SOC_DAPM_ADC_E("ADC6", NULL, TABLA_A_TX_5_6_EN, 3, 0,
		tabla_codec_enable_adc, SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_MUX_E("DEC1 MUX", TABLA_A_CDC_CLK_TX_CLK_EN_B1_CTL, 0, 0,
		&dec1_mux, tabla_codec_enable_dec, SND_SOC_DAPM_PRE_PMU),

	SND_SOC_DAPM_MUX_E("DEC2 MUX", TABLA_A_CDC_CLK_TX_CLK_EN_B1_CTL, 1, 0,
		&dec2_mux, tabla_codec_enable_dec, SND_SOC_DAPM_PRE_PMU),

	SND_SOC_DAPM_MUX_E("DEC3 MUX", TABLA_A_CDC_CLK_TX_CLK_EN_B1_CTL, 2, 0,
		&dec3_mux, tabla_codec_enable_dec, SND_SOC_DAPM_PRE_PMU),

	SND_SOC_DAPM_MUX_E("DEC4 MUX", TABLA_A_CDC_CLK_TX_CLK_EN_B1_CTL, 3, 0,
		&dec4_mux, tabla_codec_enable_dec, SND_SOC_DAPM_PRE_PMU),

	SND_SOC_DAPM_MUX_E("DEC5 MUX", TABLA_A_CDC_CLK_TX_CLK_EN_B1_CTL, 4, 0,
		&dec5_mux, tabla_codec_enable_dec, SND_SOC_DAPM_PRE_PMU),

	SND_SOC_DAPM_MUX_E("DEC6 MUX", TABLA_A_CDC_CLK_TX_CLK_EN_B1_CTL, 5, 0,
		&dec6_mux, tabla_codec_enable_dec, SND_SOC_DAPM_PRE_PMU),

	SND_SOC_DAPM_MUX_E("DEC7 MUX", TABLA_A_CDC_CLK_TX_CLK_EN_B1_CTL, 6, 0,
		&dec7_mux, tabla_codec_enable_dec, SND_SOC_DAPM_PRE_PMU),

	SND_SOC_DAPM_MUX_E("DEC8 MUX", TABLA_A_CDC_CLK_TX_CLK_EN_B1_CTL, 7, 0,
		&dec8_mux, tabla_codec_enable_dec, SND_SOC_DAPM_PRE_PMU),

	SND_SOC_DAPM_MUX_E("DEC9 MUX", TABLA_A_CDC_CLK_TX_CLK_EN_B2_CTL, 0, 0,
		&dec9_mux, tabla_codec_enable_dec, SND_SOC_DAPM_PRE_PMU),

	SND_SOC_DAPM_MUX_E("DEC10 MUX", TABLA_A_CDC_CLK_TX_CLK_EN_B2_CTL, 1, 0,
		&dec10_mux, tabla_codec_enable_dec, SND_SOC_DAPM_PRE_PMU),

	SND_SOC_DAPM_MUX("ANC1 MUX", SND_SOC_NOPM, 0, 0, &anc1_mux),
	SND_SOC_DAPM_MUX("ANC2 MUX", SND_SOC_NOPM, 0, 0, &anc2_mux),

	SND_SOC_DAPM_MIXER_E("ANC", SND_SOC_NOPM, 0, 0, NULL, 0,
		tabla_codec_enable_anc, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX("ANC1 FB MUX", SND_SOC_NOPM, 0, 0, &anc1_fb_mux),

	SND_SOC_DAPM_INPUT("AMIC2"),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS2 External", TABLA_A_MICB_2_CTL, 7, 0,
		tabla_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU |	SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS2 Internal1", TABLA_A_MICB_2_CTL, 7, 0,
		tabla_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS2 Internal2", TABLA_A_MICB_2_CTL, 7, 0,
		tabla_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS2 Internal3", TABLA_A_MICB_2_CTL, 7, 0,
		tabla_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS3 External", TABLA_A_MICB_3_CTL, 7, 0,
		tabla_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS3 Internal1", TABLA_A_MICB_3_CTL, 7, 0,
		tabla_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS3 Internal2", TABLA_A_MICB_3_CTL, 7, 0,
		tabla_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("ADC2", NULL, TABLA_A_TX_1_2_EN, 3, 0,
		tabla_codec_enable_adc, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX("SLIM TX1 MUX", SND_SOC_NOPM, 0, 0, &sb_tx1_mux),
	SND_SOC_DAPM_AIF_OUT("SLIM TX1", "AIF1 Capture", NULL, SND_SOC_NOPM,
			0, 0),

	SND_SOC_DAPM_MUX("SLIM TX5 MUX", SND_SOC_NOPM, 0, 0, &sb_tx5_mux),
	SND_SOC_DAPM_AIF_OUT("SLIM TX5", "AIF1 Capture", NULL, SND_SOC_NOPM,
			4, 0),

	SND_SOC_DAPM_MUX("SLIM TX6 MUX", SND_SOC_NOPM, 0, 0, &sb_tx6_mux),
	SND_SOC_DAPM_AIF_OUT("SLIM TX6", "AIF1 Capture", NULL, SND_SOC_NOPM,
			5, 0),

	SND_SOC_DAPM_MUX("SLIM TX7 MUX", SND_SOC_NOPM, 0, 0, &sb_tx7_mux),
	SND_SOC_DAPM_AIF_OUT("SLIM TX7", "AIF1 Capture", NULL, SND_SOC_NOPM,
			0, 0),

	SND_SOC_DAPM_MUX("SLIM TX8 MUX", SND_SOC_NOPM, 0, 0, &sb_tx8_mux),
	SND_SOC_DAPM_AIF_OUT("SLIM TX8", "AIF1 Capture", NULL, SND_SOC_NOPM,
			0, 0),

	/* Digital Mic Inputs */
	SND_SOC_DAPM_ADC_E("DMIC1", NULL, SND_SOC_NOPM, 0, 0,
		tabla_codec_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("DMIC2", NULL, SND_SOC_NOPM, 0, 0,
		tabla_codec_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("DMIC3", NULL, SND_SOC_NOPM, 0, 0,
		tabla_codec_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("DMIC4", NULL, SND_SOC_NOPM, 0, 0,
		tabla_codec_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("DMIC5", NULL, SND_SOC_NOPM, 0, 0,
		tabla_codec_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("DMIC6", NULL, SND_SOC_NOPM, 0, 0,
		tabla_codec_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	/* Sidetone */
	SND_SOC_DAPM_MUX("IIR1 INP1 MUX", SND_SOC_NOPM, 0, 0, &iir1_inp1_mux),
	SND_SOC_DAPM_PGA("IIR1", TABLA_A_CDC_CLK_SD_CTL, 0, 0, NULL, 0),
};

static const struct snd_soc_dapm_route audio_i2s_map[] = {
	{"RX_I2S_CLK", NULL, "CDC_CONN"},
	{"SLIM RX1", NULL, "RX_I2S_CLK"},
	{"SLIM RX2", NULL, "RX_I2S_CLK"},
	{"SLIM RX3", NULL, "RX_I2S_CLK"},
	{"SLIM RX4", NULL, "RX_I2S_CLK"},

	{"SLIM TX7", NULL, "TX_I2S_CLK"},
	{"SLIM TX8", NULL, "TX_I2S_CLK"},
	{"SLIM TX9", NULL, "TX_I2S_CLK"},
	{"SLIM TX10", NULL, "TX_I2S_CLK"},
};

static const struct snd_soc_dapm_route audio_map[] = {
	/* SLIMBUS Connections */

	{"SLIM TX1", NULL, "SLIM TX1 MUX"},
	{"SLIM TX1 MUX", "DEC1", "DEC1 MUX"},

	{"SLIM TX5", NULL, "SLIM TX5 MUX"},
	{"SLIM TX5 MUX", "DEC5", "DEC5 MUX"},

	{"SLIM TX6", NULL, "SLIM TX6 MUX"},
	{"SLIM TX6 MUX", "DEC6", "DEC6 MUX"},

	{"SLIM TX7", NULL, "SLIM TX7 MUX"},
	{"SLIM TX7 MUX", "DEC1", "DEC1 MUX"},
	{"SLIM TX7 MUX", "DEC2", "DEC2 MUX"},
	{"SLIM TX7 MUX", "DEC3", "DEC3 MUX"},
	{"SLIM TX7 MUX", "DEC4", "DEC4 MUX"},
	{"SLIM TX7 MUX", "DEC5", "DEC5 MUX"},
	{"SLIM TX7 MUX", "DEC6", "DEC6 MUX"},
	{"SLIM TX7 MUX", "DEC7", "DEC7 MUX"},
	{"SLIM TX7 MUX", "DEC8", "DEC8 MUX"},
	{"SLIM TX7 MUX", "DEC9", "DEC9 MUX"},
	{"SLIM TX7 MUX", "DEC10", "DEC10 MUX"},

	{"SLIM TX8", NULL, "SLIM TX8 MUX"},
	{"SLIM TX8 MUX", "DEC1", "DEC1 MUX"},
	{"SLIM TX8 MUX", "DEC2", "DEC2 MUX"},
	{"SLIM TX8 MUX", "DEC3", "DEC3 MUX"},
	{"SLIM TX8 MUX", "DEC4", "DEC4 MUX"},
	{"SLIM TX8 MUX", "DEC5", "DEC5 MUX"},
	{"SLIM TX8 MUX", "DEC6", "DEC6 MUX"},

	/* Earpiece (RX MIX1) */
	{"EAR", NULL, "EAR PA"},
	{"EAR PA", NULL, "DAC1"},
	{"DAC1", NULL, "CP"},

	{"ANC1 FB MUX", "EAR_HPH_L", "RX1 MIX1"},
	{"ANC1 FB MUX", "EAR_LINE_1", "RX2 MIX1"},
	{"ANC", NULL, "ANC1 FB MUX"},

	/* Headset (RX MIX1 and RX MIX2) */
	{"HEADPHONE", NULL, "HPHL"},
	{"HEADPHONE", NULL, "HPHR"},

	{"HPHL", NULL, "HPHL DAC"},
	{"HPHR", NULL, "HPHR DAC"},

	{"HPHL DAC", NULL, "CP"},
	{"HPHR DAC", NULL, "CP"},

	{"ANC", NULL, "ANC1 MUX"},
	{"ANC", NULL, "ANC2 MUX"},
	{"ANC1 MUX", "ADC1", "ADC1"},
	{"ANC1 MUX", "ADC2", "ADC2"},
	{"ANC1 MUX", "ADC3", "ADC3"},
	{"ANC1 MUX", "ADC4", "ADC4"},
	{"ANC2 MUX", "ADC1", "ADC1"},
	{"ANC2 MUX", "ADC2", "ADC2"},
	{"ANC2 MUX", "ADC3", "ADC3"},
	{"ANC2 MUX", "ADC4", "ADC4"},

	{"ANC", NULL, "CDC_CONN"},

	{"DAC1", "Switch", "RX1 CHAIN"},
	{"HPHL DAC", "Switch", "RX1 CHAIN"},
	{"HPHR DAC", NULL, "RX2 CHAIN"},

	{"LINEOUT1", NULL, "LINEOUT1 PA"},
	{"LINEOUT2", NULL, "LINEOUT2 PA"},
	{"LINEOUT3", NULL, "LINEOUT3 PA"},
	{"LINEOUT4", NULL, "LINEOUT4 PA"},
	{"LINEOUT5", NULL, "LINEOUT5 PA"},

	{"LINEOUT1 PA", NULL, "LINEOUT1 DAC"},
	{"LINEOUT2 PA", NULL, "LINEOUT2 DAC"},
	{"LINEOUT3 PA", NULL, "LINEOUT3 DAC"},
	{"LINEOUT4 PA", NULL, "LINEOUT4 DAC"},
	{"LINEOUT5 PA", NULL, "LINEOUT5 DAC"},

	{"LINEOUT1 DAC", NULL, "RX3 MIX1"},
	{"LINEOUT5 DAC", NULL, "RX7 MIX1"},

	{"RX1 CHAIN", NULL, "RX1 MIX1"},
	{"RX2 CHAIN", NULL, "RX2 MIX1"},
	{"RX1 CHAIN", NULL, "ANC"},
	{"RX2 CHAIN", NULL, "ANC"},

	{"CP", NULL, "RX_BIAS"},
	{"LINEOUT1 DAC", NULL, "RX_BIAS"},
	{"LINEOUT2 DAC", NULL, "RX_BIAS"},
	{"LINEOUT3 DAC", NULL, "RX_BIAS"},
	{"LINEOUT4 DAC", NULL, "RX_BIAS"},
	{"LINEOUT5 DAC", NULL, "RX_BIAS"},

	{"RX1 MIX1", NULL, "RX1 MIX1 INP1"},
	{"RX1 MIX1", NULL, "RX1 MIX1 INP2"},
	{"RX2 MIX1", NULL, "RX2 MIX1 INP1"},
	{"RX2 MIX1", NULL, "RX2 MIX1 INP2"},
	{"RX3 MIX1", NULL, "RX3 MIX1 INP1"},
	{"RX3 MIX1", NULL, "RX3 MIX1 INP2"},
	{"RX4 MIX1", NULL, "RX4 MIX1 INP1"},
	{"RX4 MIX1", NULL, "RX4 MIX1 INP2"},
	{"RX5 MIX1", NULL, "RX5 MIX1 INP1"},
	{"RX5 MIX1", NULL, "RX5 MIX1 INP2"},
	{"RX6 MIX1", NULL, "RX6 MIX1 INP1"},
	{"RX6 MIX1", NULL, "RX6 MIX1 INP2"},
	{"RX7 MIX1", NULL, "RX7 MIX1 INP1"},
	{"RX7 MIX1", NULL, "RX7 MIX1 INP2"},

	{"RX1 MIX1 INP1", "RX1", "SLIM RX1"},
	{"RX1 MIX1 INP1", "RX2", "SLIM RX2"},
	{"RX1 MIX1 INP1", "RX3", "SLIM RX3"},
	{"RX1 MIX1 INP1", "RX4", "SLIM RX4"},
	{"RX1 MIX1 INP1", "IIR1", "IIR1"},
	{"RX1 MIX1 INP2", "RX1", "SLIM RX1"},
	{"RX1 MIX1 INP2", "RX2", "SLIM RX2"},
	{"RX1 MIX1 INP2", "RX3", "SLIM RX3"},
	{"RX1 MIX1 INP2", "RX4", "SLIM RX4"},
	{"RX1 MIX1 INP2", "IIR1", "IIR1"},
	{"RX2 MIX1 INP1", "RX1", "SLIM RX1"},
	{"RX2 MIX1 INP1", "RX2", "SLIM RX2"},
	{"RX2 MIX1 INP1", "RX3", "SLIM RX3"},
	{"RX2 MIX1 INP1", "RX4", "SLIM RX4"},
	{"RX2 MIX1 INP1", "IIR1", "IIR1"},
	{"RX2 MIX1 INP2", "RX1", "SLIM RX1"},
	{"RX2 MIX1 INP2", "RX2", "SLIM RX2"},
	{"RX2 MIX1 INP2", "RX3", "SLIM RX3"},
	{"RX2 MIX1 INP2", "RX4", "SLIM RX4"},
	{"RX2 MIX1 INP2", "IIR1", "IIR1"},
	{"RX3 MIX1 INP1", "RX1", "SLIM RX1"},
	{"RX3 MIX1 INP1", "RX2", "SLIM RX2"},
	{"RX3 MIX1 INP1", "RX3", "SLIM RX3"},
	{"RX3 MIX1 INP1", "RX4", "SLIM RX4"},
	{"RX3 MIX1 INP1", "IIR1", "IIR1"},
	{"RX3 MIX1 INP2", "RX1", "SLIM RX1"},
	{"RX3 MIX1 INP2", "RX2", "SLIM RX2"},
	{"RX3 MIX1 INP2", "RX3", "SLIM RX3"},
	{"RX3 MIX1 INP2", "RX4", "SLIM RX4"},
	{"RX3 MIX1 INP2", "IIR1", "IIR1"},
	{"RX4 MIX1 INP1", "RX1", "SLIM RX1"},
	{"RX4 MIX1 INP1", "RX2", "SLIM RX2"},
	{"RX4 MIX1 INP1", "RX3", "SLIM RX3"},
	{"RX4 MIX1 INP1", "RX4", "SLIM RX4"},
	{"RX4 MIX1 INP1", "IIR1", "IIR1"},
	{"RX4 MIX1 INP2", "RX1", "SLIM RX1"},
	{"RX4 MIX1 INP2", "RX2", "SLIM RX2"},
	{"RX4 MIX1 INP2", "RX3", "SLIM RX3"},
	{"RX4 MIX1 INP2", "RX4", "SLIM RX4"},
	{"RX4 MIX1 INP2", "IIR1", "IIR1"},
	{"RX5 MIX1 INP1", "RX1", "SLIM RX1"},
	{"RX5 MIX1 INP1", "RX2", "SLIM RX2"},
	{"RX5 MIX1 INP1", "RX3", "SLIM RX3"},
	{"RX5 MIX1 INP1", "RX4", "SLIM RX4"},
	{"RX5 MIX1 INP1", "IIR1", "IIR1"},
	{"RX5 MIX1 INP2", "RX1", "SLIM RX1"},
	{"RX5 MIX1 INP2", "RX2", "SLIM RX2"},
	{"RX5 MIX1 INP2", "RX3", "SLIM RX3"},
	{"RX5 MIX1 INP2", "RX4", "SLIM RX4"},
	{"RX5 MIX1 INP2", "IIR1", "IIR1"},
	{"RX6 MIX1 INP1", "RX1", "SLIM RX1"},
	{"RX6 MIX1 INP1", "RX2", "SLIM RX2"},
	{"RX6 MIX1 INP1", "RX3", "SLIM RX3"},
	{"RX6 MIX1 INP1", "RX4", "SLIM RX4"},
	{"RX6 MIX1 INP1", "IIR1", "IIR1"},
	{"RX6 MIX1 INP2", "RX1", "SLIM RX1"},
	{"RX6 MIX1 INP2", "RX2", "SLIM RX2"},
	{"RX6 MIX1 INP2", "RX3", "SLIM RX3"},
	{"RX6 MIX1 INP2", "RX4", "SLIM RX4"},
	{"RX6 MIX1 INP2", "IIR1", "IIR1"},
	{"RX7 MIX1 INP1", "RX1", "SLIM RX1"},
	{"RX7 MIX1 INP1", "RX2", "SLIM RX2"},
	{"RX7 MIX1 INP1", "RX3", "SLIM RX3"},
	{"RX7 MIX1 INP1", "RX4", "SLIM RX4"},
	{"RX7 MIX1 INP1", "IIR1", "IIR1"},
	{"RX7 MIX1 INP2", "RX1", "SLIM RX1"},
	{"RX7 MIX1 INP2", "RX2", "SLIM RX2"},
	{"RX7 MIX1 INP2", "RX3", "SLIM RX3"},
	{"RX7 MIX1 INP2", "RX4", "SLIM RX4"},
	{"RX7 MIX1 INP2", "IIR1", "IIR1"},

	/* Decimator Inputs */
	{"DEC1 MUX", "DMIC1", "DMIC1"},
	{"DEC1 MUX", "ADC6", "ADC6"},
	{"DEC1 MUX", NULL, "CDC_CONN"},
	{"DEC2 MUX", "DMIC2", "DMIC2"},
	{"DEC2 MUX", "ADC5", "ADC5"},
	{"DEC2 MUX", NULL, "CDC_CONN"},
	{"DEC3 MUX", "DMIC3", "DMIC3"},
	{"DEC3 MUX", "ADC4", "ADC4"},
	{"DEC3 MUX", NULL, "CDC_CONN"},
	{"DEC4 MUX", "DMIC4", "DMIC4"},
	{"DEC4 MUX", "ADC3", "ADC3"},
	{"DEC4 MUX", NULL, "CDC_CONN"},
	{"DEC5 MUX", "DMIC5", "DMIC5"},
	{"DEC5 MUX", "ADC2", "ADC2"},
	{"DEC5 MUX", NULL, "CDC_CONN"},
	{"DEC6 MUX", "DMIC6", "DMIC6"},
	{"DEC6 MUX", "ADC1", "ADC1"},
	{"DEC6 MUX", NULL, "CDC_CONN"},
	{"DEC7 MUX", "DMIC1", "DMIC1"},
	{"DEC7 MUX", "ADC6", "ADC6"},
	{"DEC7 MUX", NULL, "CDC_CONN"},
	{"DEC8 MUX", "ADC5", "ADC5"},
	{"DEC8 MUX", NULL, "CDC_CONN"},
	{"DEC9 MUX", "ADC3", "ADC3"},
	{"DEC9 MUX", NULL, "CDC_CONN"},
	{"DEC10 MUX", "ADC4", "ADC4"},
	{"DEC10 MUX", NULL, "CDC_CONN"},

	/* ADC Connections */
	{"ADC1", NULL, "AMIC1"},
	{"ADC2", NULL, "AMIC2"},
	{"ADC3", NULL, "AMIC3"},
	{"ADC4", NULL, "AMIC4"},
	{"ADC5", NULL, "AMIC5"},
	{"ADC6", NULL, "AMIC6"},

	{"IIR1", NULL, "IIR1 INP1 MUX"},
	{"IIR1 INP1 MUX", "DEC1", "DEC1 MUX"},
	{"IIR1 INP1 MUX", "DEC2", "DEC2 MUX"},
	{"IIR1 INP1 MUX", "DEC3", "DEC3 MUX"},
	{"IIR1 INP1 MUX", "DEC4", "DEC4 MUX"},
	{"IIR1 INP1 MUX", "DEC5", "DEC5 MUX"},
	{"IIR1 INP1 MUX", "DEC6", "DEC6 MUX"},
	{"IIR1 INP1 MUX", "DEC7", "DEC7 MUX"},
	{"IIR1 INP1 MUX", "DEC8", "DEC8 MUX"},
	{"IIR1 INP1 MUX", "DEC9", "DEC9 MUX"},
	{"IIR1 INP1 MUX", "DEC10", "DEC10 MUX"},

	{"MIC BIAS1 Internal1", NULL, "LDO_H"},
	{"MIC BIAS1 Internal2", NULL, "LDO_H"},
	{"MIC BIAS1 External", NULL, "LDO_H"},
	{"MIC BIAS2 Internal1", NULL, "LDO_H"},
	{"MIC BIAS2 Internal2", NULL, "LDO_H"},
	{"MIC BIAS2 Internal3", NULL, "LDO_H"},
	{"MIC BIAS2 External", NULL, "LDO_H"},
	{"MIC BIAS3 Internal1", NULL, "LDO_H"},
	{"MIC BIAS3 Internal2", NULL, "LDO_H"},
	{"MIC BIAS3 External", NULL, "LDO_H"},
	{"MIC BIAS4 External", NULL, "LDO_H"},
};

static const struct snd_soc_dapm_route tabla_1_x_lineout_2_to_4_map[] = {

	{"RX4 DSM MUX", "DSM_INV", "RX3 MIX1"},
	{"RX4 DSM MUX", "CIC_OUT", "RX4 MIX1"},

	{"LINEOUT2 DAC", NULL, "RX4 DSM MUX"},

	{"LINEOUT3 DAC", NULL, "RX5 MIX1"},
	{"LINEOUT3 DAC GROUND", "Switch", "RX3 MIX1"},
	{"LINEOUT3 DAC", NULL, "LINEOUT3 DAC GROUND"},

	{"RX6 DSM MUX", "DSM_INV", "RX5 MIX1"},
	{"RX6 DSM MUX", "CIC_OUT", "RX6 MIX1"},

	{"LINEOUT4 DAC", NULL, "RX6 DSM MUX"},
	{"LINEOUT4 DAC GROUND", "Switch", "RX4 DSM MUX"},
	{"LINEOUT4 DAC", NULL, "LINEOUT4 DAC GROUND"},
};


static const struct snd_soc_dapm_route tabla_2_x_lineout_2_to_4_map[] = {

	{"RX4 DSM MUX", "DSM_INV", "RX3 MIX1"},
	{"RX4 DSM MUX", "CIC_OUT", "RX4 MIX1"},

	{"LINEOUT3 DAC", NULL, "RX4 DSM MUX"},

	{"LINEOUT2 DAC", NULL, "RX5 MIX1"},

	{"RX6 DSM MUX", "DSM_INV", "RX5 MIX1"},
	{"RX6 DSM MUX", "CIC_OUT", "RX6 MIX1"},

	{"LINEOUT4 DAC", NULL, "RX6 DSM MUX"},
};

static int tabla_readable(struct snd_soc_codec *ssc, unsigned int reg)
{
	return tabla_reg_readable[reg];
}

static int tabla_volatile(struct snd_soc_codec *ssc, unsigned int reg)
{
	/* Registers lower than 0x100 are top level registers which can be
	 * written by the Tabla core driver.
	 */

	if ((reg >= TABLA_A_CDC_MBHC_EN_CTL) || (reg < 0x100))
		return 1;

	/* IIR Coeff registers are not cacheable */
	if ((reg >= TABLA_A_CDC_IIR1_COEF_B1_CTL) &&
		(reg <= TABLA_A_CDC_IIR2_COEF_B5_CTL))
		return 1;

	return 0;
}

#define TABLA_FORMATS (SNDRV_PCM_FMTBIT_S16_LE)
static int tabla_write(struct snd_soc_codec *codec, unsigned int reg,
	unsigned int value)
{
	int ret;

	BUG_ON(reg > TABLA_MAX_REGISTER);

	if (!tabla_volatile(codec, reg)) {
		ret = snd_soc_cache_write(codec, reg, value);
		if (ret != 0)
			dev_err(codec->dev, "Cache write to %x failed: %d\n",
				reg, ret);
	}

	return tabla_reg_write(codec->control_data, reg, value);
}
static unsigned int tabla_read(struct snd_soc_codec *codec,
				unsigned int reg)
{
	unsigned int val;
	int ret;

	BUG_ON(reg > TABLA_MAX_REGISTER);

	if (!tabla_volatile(codec, reg) && tabla_readable(codec, reg) &&
		reg < codec->driver->reg_cache_size) {
		ret = snd_soc_cache_read(codec, reg, &val);
		if (ret >= 0) {
			return val;
		} else
			dev_err(codec->dev, "Cache read from %x failed: %d\n",
				reg, ret);
	}

	val = tabla_reg_read(codec->control_data, reg);
	return val;
}

static void tabla_codec_enable_audio_mode_bandgap(struct snd_soc_codec *codec)
{
	snd_soc_write(codec, TABLA_A_BIAS_REF_CTL, 0x1C);
	snd_soc_update_bits(codec, TABLA_A_BIAS_CENTRAL_BG_CTL, 0x80,
		0x80);
	snd_soc_update_bits(codec, TABLA_A_BIAS_CENTRAL_BG_CTL, 0x04,
		0x04);
	snd_soc_update_bits(codec, TABLA_A_BIAS_CENTRAL_BG_CTL, 0x01,
		0x01);
	usleep_range(1000, 1000);
	snd_soc_update_bits(codec, TABLA_A_BIAS_CENTRAL_BG_CTL, 0x80,
		0x00);
}

static void tabla_codec_enable_bandgap(struct snd_soc_codec *codec,
	enum tabla_bandgap_type choice)
{
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);

	/* TODO lock resources accessed by audio streams and threaded
	 * interrupt handlers
	 */

	pr_debug("%s, choice is %d, current is %d\n", __func__, choice,
		tabla->bandgap_type);

	if (tabla->bandgap_type == choice)
		return;

	if ((tabla->bandgap_type == TABLA_BANDGAP_OFF) &&
		(choice == TABLA_BANDGAP_AUDIO_MODE)) {
		tabla_codec_enable_audio_mode_bandgap(codec);
	} else if ((tabla->bandgap_type == TABLA_BANDGAP_AUDIO_MODE) &&
		(choice == TABLA_BANDGAP_MBHC_MODE)) {
		snd_soc_update_bits(codec, TABLA_A_BIAS_CENTRAL_BG_CTL, 0x2,
			0x2);
		snd_soc_update_bits(codec, TABLA_A_BIAS_CENTRAL_BG_CTL, 0x80,
			0x80);
		snd_soc_update_bits(codec, TABLA_A_BIAS_CENTRAL_BG_CTL, 0x4,
			0x4);
		usleep_range(1000, 1000);
		snd_soc_update_bits(codec, TABLA_A_BIAS_CENTRAL_BG_CTL, 0x80,
			0x00);
	} else if ((tabla->bandgap_type == TABLA_BANDGAP_MBHC_MODE) &&
		(choice == TABLA_BANDGAP_AUDIO_MODE)) {
		snd_soc_write(codec, TABLA_A_BIAS_CENTRAL_BG_CTL, 0x00);
		usleep_range(100, 100);
		tabla_codec_enable_audio_mode_bandgap(codec);
	} else if (choice == TABLA_BANDGAP_OFF) {
		snd_soc_write(codec, TABLA_A_BIAS_CENTRAL_BG_CTL, 0x00);
	} else {
		pr_err("%s: Error, Invalid bandgap settings\n", __func__);
	}
	tabla->bandgap_type = choice;
}

static int tabla_codec_enable_config_mode(struct snd_soc_codec *codec,
	int enable)
{
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);

	if (enable) {
		snd_soc_update_bits(codec, TABLA_A_CONFIG_MODE_FREQ, 0x10, 0);
		snd_soc_write(codec, TABLA_A_BIAS_CONFIG_MODE_BG_CTL, 0x17);
		usleep_range(5, 5);
		snd_soc_update_bits(codec, TABLA_A_CONFIG_MODE_FREQ, 0x80,
			0x80);
		snd_soc_update_bits(codec, TABLA_A_CONFIG_MODE_TEST, 0x80,
			0x80);
		usleep_range(10, 10);
		snd_soc_update_bits(codec, TABLA_A_CONFIG_MODE_TEST, 0x80, 0);
		usleep_range(20, 20);
		snd_soc_update_bits(codec, TABLA_A_CLK_BUFF_EN1, 0x08, 0x08);
	} else {
		snd_soc_update_bits(codec, TABLA_A_BIAS_CONFIG_MODE_BG_CTL, 0x1,
			0);
		snd_soc_update_bits(codec, TABLA_A_CONFIG_MODE_FREQ, 0x80, 0);
	}
	tabla->config_mode_active = enable ? true : false;

	return 0;
}

static int tabla_codec_enable_clock_block(struct snd_soc_codec *codec,
	int config_mode)
{
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s\n", __func__);

	if (config_mode) {
		tabla_codec_enable_config_mode(codec, 1);
		snd_soc_write(codec, TABLA_A_CLK_BUFF_EN2, 0x00);
		snd_soc_write(codec, TABLA_A_CLK_BUFF_EN2, 0x02);
		snd_soc_write(codec, TABLA_A_CLK_BUFF_EN1, 0x0D);
		usleep_range(1000, 1000);
	} else
		snd_soc_update_bits(codec, TABLA_A_CLK_BUFF_EN1, 0x08, 0x00);

	if (!config_mode && tabla->mbhc_polling_active) {
		snd_soc_write(codec, TABLA_A_CLK_BUFF_EN2, 0x02);
		tabla_codec_enable_config_mode(codec, 0);

	}

	snd_soc_update_bits(codec, TABLA_A_CLK_BUFF_EN1, 0x05, 0x05);
	snd_soc_update_bits(codec, TABLA_A_CLK_BUFF_EN2, 0x02, 0x00);
	snd_soc_update_bits(codec, TABLA_A_CLK_BUFF_EN2, 0x04, 0x04);
	snd_soc_update_bits(codec, TABLA_A_CDC_CLK_MCLK_CTL, 0x01, 0x01);
	usleep_range(50, 50);
	tabla->clock_active = true;
	return 0;
}
static void tabla_codec_disable_clock_block(struct snd_soc_codec *codec)
{
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);
	pr_debug("%s\n", __func__);
	snd_soc_update_bits(codec, TABLA_A_CLK_BUFF_EN2, 0x04, 0x00);
	ndelay(160);
	snd_soc_update_bits(codec, TABLA_A_CLK_BUFF_EN2, 0x02, 0x02);
	snd_soc_update_bits(codec, TABLA_A_CLK_BUFF_EN1, 0x05, 0x00);
	tabla->clock_active = false;
}

static void tabla_codec_calibrate_hs_polling(struct snd_soc_codec *codec)
{
	/* TODO store register values in calibration */
	snd_soc_write(codec, TABLA_A_CDC_MBHC_VOLT_B5_CTL, 0x20);
	snd_soc_write(codec, TABLA_A_CDC_MBHC_VOLT_B6_CTL, 0xFF);

	snd_soc_write(codec, TABLA_A_CDC_MBHC_VOLT_B10_CTL, 0xFF);
	snd_soc_write(codec, TABLA_A_CDC_MBHC_VOLT_B9_CTL, 0x20);

	snd_soc_write(codec, TABLA_A_CDC_MBHC_VOLT_B4_CTL, 0xF8);
	snd_soc_write(codec, TABLA_A_CDC_MBHC_VOLT_B3_CTL, 0xEE);
	snd_soc_write(codec, TABLA_A_CDC_MBHC_VOLT_B2_CTL, 0xFC);
	snd_soc_write(codec, TABLA_A_CDC_MBHC_VOLT_B1_CTL, 0xCE);

	snd_soc_write(codec, TABLA_A_CDC_MBHC_TIMER_B1_CTL, 3);
	snd_soc_write(codec, TABLA_A_CDC_MBHC_TIMER_B2_CTL, 9);
	snd_soc_write(codec, TABLA_A_CDC_MBHC_TIMER_B3_CTL, 30);
	snd_soc_write(codec, TABLA_A_CDC_MBHC_TIMER_B6_CTL, 120);
	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_TIMER_B1_CTL, 0x78, 0x58);
	snd_soc_write(codec, TABLA_A_CDC_MBHC_B2_CTL, 11);
}

static int tabla_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	pr_debug("%s(): substream = %s  stream = %d\n" , __func__,
		 substream->name, substream->stream);

	return 0;
}

static void tabla_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	pr_debug("%s(): substream = %s  stream = %d\n" , __func__,
		 substream->name, substream->stream);
}

int tabla_mclk_enable(struct snd_soc_codec *codec, int mclk_enable)
{
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s() mclk_enable = %u\n", __func__, mclk_enable);

	if (mclk_enable) {
		tabla->mclk_enabled = true;

		if (tabla->mbhc_polling_active && (tabla->mclk_enabled)) {
			tabla_codec_pause_hs_polling(codec);
			tabla_codec_enable_bandgap(codec,
					TABLA_BANDGAP_AUDIO_MODE);
			tabla_codec_enable_clock_block(codec, 0);
			tabla_codec_calibrate_hs_polling(codec);
			tabla_codec_start_hs_polling(codec);
		}
	} else {

		if (!tabla->mclk_enabled) {
			pr_err("Error, MCLK already diabled\n");
			return -EINVAL;
		}
		tabla->mclk_enabled = false;

		if (tabla->mbhc_polling_active) {
			if (!tabla->mclk_enabled) {
				tabla_codec_pause_hs_polling(codec);
				tabla_codec_enable_bandgap(codec,
					TABLA_BANDGAP_MBHC_MODE);
				tabla_enable_rx_bias(codec, 1);
				tabla_codec_enable_clock_block(codec, 1);
				tabla_codec_calibrate_hs_polling(codec);
				tabla_codec_start_hs_polling(codec);
			}
			snd_soc_update_bits(codec, TABLA_A_CLK_BUFF_EN1,
					0x05, 0x01);
		}
	}
	return 0;
}

static int tabla_set_dai_sysclk(struct snd_soc_dai *dai,
		int clk_id, unsigned int freq, int dir)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static int tabla_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	u8 val = 0;
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(dai->codec);

	pr_debug("%s\n", __func__);
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		/* CPU is master */
		if (tabla->intf_type == TABLA_INTERFACE_TYPE_I2C) {
			if (dai->id == TABLA_TX_DAI_ID)
				snd_soc_update_bits(dai->codec,
					TABLA_A_CDC_CLK_TX_I2S_CTL,
					TABLA_I2S_MASTER_MODE_MASK, 0);
			else if (dai->id == TABLA_RX_DAI_ID)
				snd_soc_update_bits(dai->codec,
					TABLA_A_CDC_CLK_RX_I2S_CTL,
					TABLA_I2S_MASTER_MODE_MASK, 0);
		}
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
	/* CPU is slave */
		if (tabla->intf_type == TABLA_INTERFACE_TYPE_I2C) {
			val = TABLA_I2S_MASTER_MODE_MASK;
			if (dai->id == TABLA_TX_DAI_ID)
				snd_soc_update_bits(dai->codec,
					TABLA_A_CDC_CLK_TX_I2S_CTL, val, val);
			else if (dai->id == TABLA_RX_DAI_ID)
				snd_soc_update_bits(dai->codec,
					TABLA_A_CDC_CLK_RX_I2S_CTL, val, val);
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int tabla_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params,
			    struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(dai->codec);
	u8 path, shift;
	u16 tx_fs_reg, rx_fs_reg;
	u8 tx_fs_rate, rx_fs_rate, rx_state, tx_state;

	pr_debug("%s: DAI-ID %x\n", __func__, dai->id);

	switch (params_rate(params)) {
	case 8000:
		tx_fs_rate = 0x00;
		rx_fs_rate = 0x00;
		break;
	case 16000:
		tx_fs_rate = 0x01;
		rx_fs_rate = 0x20;
		break;
	case 32000:
		tx_fs_rate = 0x02;
		rx_fs_rate = 0x40;
		break;
	case 48000:
		tx_fs_rate = 0x03;
		rx_fs_rate = 0x60;
		break;
	default:
		pr_err("%s: Invalid sampling rate %d\n", __func__,
				params_rate(params));
		return -EINVAL;
	}


	/**
	 * If current dai is a tx dai, set sample rate to
	 * all the txfe paths that are currently not active
	 */
	if (dai->id == TABLA_TX_DAI_ID) {

		tx_state = snd_soc_read(codec,
				TABLA_A_CDC_CLK_TX_CLK_EN_B1_CTL);

		for (path = 1, shift = 0;
				path <= NUM_DECIMATORS; path++, shift++) {

			if (path == BITS_PER_REG + 1) {
				shift = 0;
				tx_state = snd_soc_read(codec,
					TABLA_A_CDC_CLK_TX_CLK_EN_B2_CTL);
			}

			if (!(tx_state & (1 << shift))) {
				tx_fs_reg = TABLA_A_CDC_TX1_CLK_FS_CTL
						+ (BITS_PER_REG*(path-1));
				snd_soc_update_bits(codec, tx_fs_reg,
							0x03, tx_fs_rate);
			}
		}
		if (tabla->intf_type == TABLA_INTERFACE_TYPE_I2C) {
			switch (params_format(params)) {
			case SNDRV_PCM_FORMAT_S16_LE:
				snd_soc_update_bits(codec,
					TABLA_A_CDC_CLK_TX_I2S_CTL,
					0x20, 0x20);
				break;
			case SNDRV_PCM_FORMAT_S32_LE:
				snd_soc_update_bits(codec,
					TABLA_A_CDC_CLK_TX_I2S_CTL,
					0x20, 0x00);
				break;
			default:
				pr_err("invalid format\n");
				break;
			}
			snd_soc_update_bits(codec, TABLA_A_CDC_CLK_TX_I2S_CTL,
						0x03, tx_fs_rate);
		}
	}

	/**
	 * TODO: Need to handle case where same RX chain takes 2 or more inputs
	 * with varying sample rates
	 */

	/**
	 * If current dai is a rx dai, set sample rate to
	 * all the rx paths that are currently not active
	 */
	if (dai->id == TABLA_RX_DAI_ID) {

		rx_state = snd_soc_read(codec,
			TABLA_A_CDC_CLK_RX_B1_CTL);

		for (path = 1, shift = 0;
				path <= NUM_INTERPOLATORS; path++, shift++) {

			if (!(rx_state & (1 << shift))) {
				rx_fs_reg = TABLA_A_CDC_RX1_B5_CTL
						+ (BITS_PER_REG*(path-1));
				snd_soc_update_bits(codec, rx_fs_reg,
						0xE0, rx_fs_rate);
			}
		}
		if (tabla->intf_type == TABLA_INTERFACE_TYPE_I2C) {
			switch (params_format(params)) {
			case SNDRV_PCM_FORMAT_S16_LE:
				snd_soc_update_bits(codec,
					TABLA_A_CDC_CLK_RX_I2S_CTL,
					0x20, 0x20);
				break;
			case SNDRV_PCM_FORMAT_S32_LE:
				snd_soc_update_bits(codec,
					TABLA_A_CDC_CLK_RX_I2S_CTL,
					0x20, 0x00);
				break;
			default:
				pr_err("invalid format\n");
				break;
			}
			snd_soc_update_bits(codec, TABLA_A_CDC_CLK_RX_I2S_CTL,
						0x03, (rx_fs_rate >> 0x05));
		}
	}

	return 0;
}

static struct snd_soc_dai_ops tabla_dai_ops = {
	.startup = tabla_startup,
	.shutdown = tabla_shutdown,
	.hw_params = tabla_hw_params,
	.set_sysclk = tabla_set_dai_sysclk,
	.set_fmt = tabla_set_dai_fmt,
};

static struct snd_soc_dai_driver tabla_dai[] = {
	{
		.name = "tabla_rx1",
		.id = 1,
		.playback = {
			.stream_name = "AIF1 Playback",
			.rates = WCD9310_RATES,
			.formats = TABLA_FORMATS,
			.rate_max = 48000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &tabla_dai_ops,
	},
	{
		.name = "tabla_tx1",
		.id = 2,
		.capture = {
			.stream_name = "AIF1 Capture",
			.rates = WCD9310_RATES,
			.formats = TABLA_FORMATS,
			.rate_max = 48000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &tabla_dai_ops,
	},
};

static struct snd_soc_dai_driver tabla_i2s_dai[] = {
	{
		.name = "tabla_i2s_rx1",
		.id = 1,
		.playback = {
			.stream_name = "AIF1 Playback",
			.rates = WCD9310_RATES,
			.formats = TABLA_FORMATS,
			.rate_max = 48000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 4,
		},
		.ops = &tabla_dai_ops,
	},
	{
		.name = "tabla_i2s_tx1",
		.id = 2,
		.capture = {
			.stream_name = "AIF1 Capture",
			.rates = WCD9310_RATES,
			.formats = TABLA_FORMATS,
			.rate_max = 48000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 4,
		},
		.ops = &tabla_dai_ops,
	},
};
static short tabla_codec_read_sta_result(struct snd_soc_codec *codec)
{
	u8 bias_msb, bias_lsb;
	short bias_value;

	bias_msb = snd_soc_read(codec, TABLA_A_CDC_MBHC_B3_STATUS);
	bias_lsb = snd_soc_read(codec, TABLA_A_CDC_MBHC_B2_STATUS);
	bias_value = (bias_msb << 8) | bias_lsb;
	return bias_value;
}

static short tabla_codec_read_dce_result(struct snd_soc_codec *codec)
{
	u8 bias_msb, bias_lsb;
	short bias_value;

	bias_msb = snd_soc_read(codec, TABLA_A_CDC_MBHC_B5_STATUS);
	bias_lsb = snd_soc_read(codec, TABLA_A_CDC_MBHC_B4_STATUS);
	bias_value = (bias_msb << 8) | bias_lsb;
	return bias_value;
}

static short tabla_codec_measure_micbias_voltage(struct snd_soc_codec *codec,
	int dce)
{
	short bias_value;

	if (dce) {
		snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_CLK_CTL, 0x8, 0x8);
		snd_soc_write(codec, TABLA_A_CDC_MBHC_EN_CTL, 0x4);
		snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_CLK_CTL, 0x8, 0x0);
		snd_soc_write(codec, TABLA_A_CDC_MBHC_EN_CTL, 0x4);
		usleep_range(60000, 60000);
		bias_value = tabla_codec_read_dce_result(codec);
	} else {
		snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_CLK_CTL, 0x8, 0x8);
		snd_soc_write(codec, TABLA_A_CDC_MBHC_EN_CTL, 0x2);
		snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_CLK_CTL, 0x8, 0x0);
		usleep_range(5000, 5000);
		snd_soc_write(codec, TABLA_A_CDC_MBHC_EN_CTL, 0x2);
		usleep_range(50, 50);
		bias_value = tabla_codec_read_sta_result(codec);
		snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_CLK_CTL, 0x8, 0x8);
		snd_soc_write(codec, TABLA_A_CDC_MBHC_EN_CTL, 0x0);
	}

	pr_debug("read microphone bias value %x\n", bias_value);
	return bias_value;
}

static short tabla_codec_setup_hs_polling(struct snd_soc_codec *codec)
{
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);
	struct tabla_mbhc_calibration *calibration = tabla->calibration;
	short bias_value;
	u8 cfilt_mode;

	if (!calibration) {
		pr_err("Error, no tabla calibration\n");
		return -ENODEV;
	}

	tabla->mbhc_polling_active = true;

	if (!tabla->mclk_enabled) {
		tabla_codec_enable_bandgap(codec, TABLA_BANDGAP_MBHC_MODE);
		tabla_enable_rx_bias(codec, 1);
		tabla_codec_enable_clock_block(codec, 1);
	}

	snd_soc_update_bits(codec, TABLA_A_CLK_BUFF_EN1, 0x05, 0x01);

	snd_soc_update_bits(codec, TABLA_A_TX_COM_BIAS, 0xE0, 0xE0);

	/* Make sure CFILT is in fast mode, save current mode */
	cfilt_mode = snd_soc_read(codec,
		tabla->mbhc_bias_regs.cfilt_ctl);
	snd_soc_update_bits(codec, tabla->mbhc_bias_regs.cfilt_ctl,
		0x70, 0x00);

	snd_soc_update_bits(codec,
		tabla->mbhc_bias_regs.ctl_reg, 0x1F, 0x16);

	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_CLK_CTL, 0x2, 0x2);
	snd_soc_write(codec, TABLA_A_MBHC_SCALING_MUX_1, 0x84);

	snd_soc_update_bits(codec, TABLA_A_TX_7_MBHC_EN, 0x80, 0x80);
	snd_soc_update_bits(codec, TABLA_A_TX_7_MBHC_EN, 0x1F, 0x1C);
	snd_soc_update_bits(codec, TABLA_A_TX_7_MBHC_TEST_CTL, 0x40, 0x40);

	snd_soc_update_bits(codec, TABLA_A_TX_7_MBHC_EN, 0x80, 0x00);
	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_CLK_CTL, 0x8, 0x8);
	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_CLK_CTL, 0x8, 0x00);

	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_B1_CTL, 0x6, 0x6);
	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_CLK_CTL, 0x8, 0x8);

	tabla_codec_calibrate_hs_polling(codec);

	bias_value = tabla_codec_measure_micbias_voltage(codec, 0);
	snd_soc_update_bits(codec,
		tabla->mbhc_bias_regs.cfilt_ctl, 0x40, cfilt_mode);
	snd_soc_update_bits(codec, TABLA_A_MBHC_HPH, 0x13, 0x00);

	return bias_value;
}

static int tabla_codec_enable_hs_detect(struct snd_soc_codec *codec,
		int insertion)
{
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);
	struct tabla_mbhc_calibration *calibration = tabla->calibration;
	int central_bias_enabled = 0;
	u8 wg_time;

	if (!calibration) {
		pr_err("Error, no tabla calibration\n");
		return -EINVAL;
	}

	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_INT_CTL, 0x1, 0);

	if (insertion) {
		/* Make sure mic bias and Mic line schmitt trigger
		 * are turned OFF
		 */
		snd_soc_update_bits(codec, tabla->mbhc_bias_regs.ctl_reg,
			0x81, 0x01);
		snd_soc_update_bits(codec, tabla->mbhc_bias_regs.mbhc_reg,
			0x90, 0x00);
		wg_time = snd_soc_read(codec, TABLA_A_RX_HPH_CNP_WG_TIME) ;
		wg_time += 1;

		/* Enable HPH Schmitt Trigger */
		snd_soc_update_bits(codec, TABLA_A_MBHC_HPH, 0x11, 0x11);
		snd_soc_update_bits(codec, TABLA_A_MBHC_HPH, 0x0C,
			calibration->hph_current << 2);

		/* Turn off HPH PAs and DAC's during insertion detection to
		 * avoid false insertion interrupts
		 */
		if (tabla->mbhc_micbias_switched)
			tabla_codec_switch_micbias(codec, 0);
		snd_soc_update_bits(codec, TABLA_A_RX_HPH_CNP_EN, 0x30, 0x00);
		snd_soc_update_bits(codec, TABLA_A_RX_HPH_L_DAC_CTL,
			0xC0, 0x00);
		snd_soc_update_bits(codec, TABLA_A_RX_HPH_R_DAC_CTL,
			0xC0, 0x00);
		usleep_range(wg_time * 1000, wg_time * 1000);

		/* setup for insetion detection */
		snd_soc_update_bits(codec, TABLA_A_MBHC_HPH, 0x02, 0x02);
		snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_INT_CTL, 0x2, 0);
	} else {
		/* Make sure the HPH schmitt trigger is OFF */
		snd_soc_update_bits(codec, TABLA_A_MBHC_HPH, 0x12, 0x00);

		/* enable the mic line schmitt trigger */
		snd_soc_update_bits(codec, tabla->mbhc_bias_regs.mbhc_reg, 0x60,
			calibration->mic_current << 5);
		snd_soc_update_bits(codec, tabla->mbhc_bias_regs.mbhc_reg,
			0x80, 0x80);
		usleep_range(calibration->mic_pid, calibration->mic_pid);
		snd_soc_update_bits(codec, tabla->mbhc_bias_regs.mbhc_reg,
			0x10, 0x10);

		/* Setup for low power removal detection */
		snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_INT_CTL, 0x2, 0x2);
	}

	if (snd_soc_read(codec, TABLA_A_CDC_MBHC_B1_CTL) & 0x4) {
		if (!(tabla->clock_active)) {
			tabla_codec_enable_config_mode(codec, 1);
			snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_B1_CTL,
				0x06, 0);
			usleep_range(calibration->shutdown_plug_removal,
				calibration->shutdown_plug_removal);
			tabla_codec_enable_config_mode(codec, 0);
		} else
			snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_B1_CTL,
				0x06, 0);
	}

	snd_soc_update_bits(codec, tabla->mbhc_bias_regs.int_rbias, 0x80, 0);

	/* If central bandgap disabled */
	if (!(snd_soc_read(codec, TABLA_A_PIN_CTL_OE1) & 1)) {
		snd_soc_update_bits(codec, TABLA_A_PIN_CTL_OE1, 0x3, 0x3);
		usleep_range(calibration->bg_fast_settle,
			calibration->bg_fast_settle);
		central_bias_enabled = 1;
	}

	/* If LDO_H disabled */
	if (snd_soc_read(codec, TABLA_A_PIN_CTL_OE0) & 0x80) {
		snd_soc_update_bits(codec, TABLA_A_PIN_CTL_OE0, 0x10, 0);
		snd_soc_update_bits(codec, TABLA_A_PIN_CTL_OE0, 0x80, 0x80);
		usleep_range(calibration->tldoh, calibration->tldoh);
		snd_soc_update_bits(codec, TABLA_A_PIN_CTL_OE0, 0x80, 0);

		if (central_bias_enabled)
			snd_soc_update_bits(codec, TABLA_A_PIN_CTL_OE1, 0x1, 0);
	}

	snd_soc_update_bits(codec, TABLA_A_MICB_4_MBHC, 0x3, calibration->bias);

	tabla_enable_irq(codec->control_data, TABLA_IRQ_MBHC_INSERTION);
	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_INT_CTL, 0x1, 0x1);
	return 0;
}

static void tabla_lock_sleep(struct tabla_priv *tabla)
{
	int ret;
	while (!(ret = wait_event_timeout(tabla->pm_wq,
					  atomic_inc_not_zero(&tabla->pm_cnt),
					  2 * HZ))) {
		pr_err("%s: didn't wake up for 2000ms (%d), pm_cnt %d\n",
		       __func__, ret, atomic_read(&tabla->pm_cnt));
		WARN_ON_ONCE(1);
	}
}

static void tabla_unlock_sleep(struct tabla_priv *tabla)
{
	atomic_dec(&tabla->pm_cnt);
	wake_up(&tabla->pm_wq);
}

static void btn0_lpress_fn(struct work_struct *work)
{
	struct delayed_work *delayed_work;
	struct tabla_priv *tabla;

	pr_debug("%s:\n", __func__);

	delayed_work = to_delayed_work(work);
	tabla = container_of(delayed_work, struct tabla_priv, btn0_dwork);

	if (tabla) {
		if (tabla->button_jack) {
			pr_debug("%s: Reporting long button press event\n",
					__func__);
			tabla_snd_soc_jack_report(tabla, tabla->button_jack,
						  SND_JACK_BTN_0,
						  SND_JACK_BTN_0);
		}
	} else {
		pr_err("%s: Bad tabla private data\n", __func__);
	}

	tabla_unlock_sleep(tabla);
}

int tabla_hs_detect(struct snd_soc_codec *codec,
	struct snd_soc_jack *headset_jack, struct snd_soc_jack *button_jack,
	struct tabla_mbhc_calibration *calibration)
{
	struct tabla_priv *tabla;
	int rc;

	if (!codec || !calibration) {
		pr_err("Error: no codec or calibration\n");
		return -EINVAL;
	}
	tabla = snd_soc_codec_get_drvdata(codec);
	tabla->headset_jack = headset_jack;
	tabla->button_jack = button_jack;
	tabla->calibration = calibration;
	tabla_get_mbhc_micbias_regs(codec, &tabla->mbhc_bias_regs);

	/* Put CFILT in fast mode by default */
	snd_soc_update_bits(codec, tabla->mbhc_bias_regs.cfilt_ctl,
		0x40, TABLA_CFILT_FAST_MODE);

	INIT_DELAYED_WORK(&tabla->btn0_dwork, btn0_lpress_fn);
	INIT_WORK(&tabla->hphlocp_work, hphlocp_off_report);
	INIT_WORK(&tabla->hphrocp_work, hphrocp_off_report);
	rc =  tabla_codec_enable_hs_detect(codec, 1);

	if (!IS_ERR_VALUE(rc)) {
		snd_soc_update_bits(codec, TABLA_A_RX_HPH_OCP_CTL, 0x10,
			0x10);
		tabla_enable_irq(codec->control_data,
			TABLA_IRQ_HPH_PA_OCPL_FAULT);
		tabla_enable_irq(codec->control_data,
			TABLA_IRQ_HPH_PA_OCPR_FAULT);
	}

	return rc;
}
EXPORT_SYMBOL_GPL(tabla_hs_detect);

static irqreturn_t tabla_dce_handler(int irq, void *data)
{
	struct tabla_priv *priv = data;
	struct snd_soc_codec *codec = priv->codec;
	short bias_value;

	tabla_disable_irq(codec->control_data, TABLA_IRQ_MBHC_REMOVAL);
	tabla_disable_irq(codec->control_data, TABLA_IRQ_MBHC_POTENTIAL);
	tabla_lock_sleep(priv);

	bias_value = tabla_codec_read_dce_result(codec);
	pr_debug("%s: button press interrupt, bias value(DCE Read)=%d\n",
			__func__, bias_value);

	bias_value = tabla_codec_read_sta_result(codec);
	pr_debug("%s: button press interrupt, bias value(STA Read)=%d\n",
			__func__, bias_value);
	/*
	 * TODO: If button pressed is not button 0,
	 * report the button press event immediately.
	 */
	priv->buttons_pressed |= SND_JACK_BTN_0;

	msleep(100);

	if (schedule_delayed_work(&priv->btn0_dwork,
				  msecs_to_jiffies(400)) == 0) {
		WARN(1, "Button pressed twice without release event\n");
		tabla_unlock_sleep(priv);
	}

	return IRQ_HANDLED;
}

static irqreturn_t tabla_release_handler(int irq, void *data)
{
	struct tabla_priv *priv = data;
	struct snd_soc_codec *codec = priv->codec;
	int ret, mic_voltage;

	pr_debug("%s\n", __func__);
	tabla_disable_irq(codec->control_data, TABLA_IRQ_MBHC_RELEASE);
	tabla_lock_sleep(priv);

	mic_voltage = tabla_codec_read_dce_result(codec);
	pr_debug("%s: Microphone Voltage on release(DCE Read) = %d\n",
		__func__, mic_voltage);

	if (priv->buttons_pressed & SND_JACK_BTN_0) {
		ret = cancel_delayed_work(&priv->btn0_dwork);

		if (ret == 0) {

			pr_debug("%s: Reporting long button release event\n",
					__func__);
			if (priv->button_jack) {
				tabla_snd_soc_jack_report(priv,
							  priv->button_jack, 0,
							  SND_JACK_BTN_0);
			}

		} else {
			/* if scheduled btn0_dwork is canceled from here,
			 * we have to unlock from here instead btn0_work */
			tabla_unlock_sleep(priv);
			mic_voltage =
				tabla_codec_measure_micbias_voltage(codec, 0);
			pr_debug("%s: Mic Voltage on release(new STA) = %d\n",
						__func__, mic_voltage);

			if (mic_voltage < -2000 || mic_voltage > -670) {
				pr_debug("%s: Fake buttton press interrupt\n",
						__func__);
			} else {

				if (priv->button_jack) {
					pr_debug("%s:reporting short button press and release\n",
							__func__);

					tabla_snd_soc_jack_report(priv,
							      priv->button_jack,
						SND_JACK_BTN_0, SND_JACK_BTN_0);
					tabla_snd_soc_jack_report(priv,
							     priv->button_jack,
							     0, SND_JACK_BTN_0);
				}
			}
		}

		priv->buttons_pressed &= ~SND_JACK_BTN_0;
	}

	tabla_codec_start_hs_polling(codec);
	tabla_unlock_sleep(priv);
	return IRQ_HANDLED;
}

static void tabla_codec_shutdown_hs_removal_detect(struct snd_soc_codec *codec)
{
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);
	struct tabla_mbhc_calibration *calibration = tabla->calibration;

	if (!tabla->mclk_enabled && !tabla->mbhc_polling_active)
		tabla_codec_enable_config_mode(codec, 1);

	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_CLK_CTL, 0x2, 0x2);
	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_B1_CTL, 0x6, 0x0);

	snd_soc_update_bits(codec,
		tabla->mbhc_bias_regs.mbhc_reg, 0x80, 0x00);
	usleep_range(calibration->shutdown_plug_removal,
		calibration->shutdown_plug_removal);

	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_CLK_CTL, 0xA, 0x8);
	if (!tabla->mclk_enabled && !tabla->mbhc_polling_active)
		tabla_codec_enable_config_mode(codec, 0);

	snd_soc_write(codec, TABLA_A_MBHC_SCALING_MUX_1, 0x00);
}

static void tabla_codec_shutdown_hs_polling(struct snd_soc_codec *codec)
{
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);

	tabla_codec_shutdown_hs_removal_detect(codec);

	if (!tabla->mclk_enabled) {
		snd_soc_update_bits(codec, TABLA_A_TX_COM_BIAS, 0xE0, 0x00);
		tabla_codec_enable_bandgap(codec, TABLA_BANDGAP_AUDIO_MODE);
		tabla_codec_enable_clock_block(codec, 0);
	}

	tabla->mbhc_polling_active = false;
}

static irqreturn_t tabla_hphl_ocp_irq(int irq, void *data)
{
	struct tabla_priv *tabla = data;
	struct snd_soc_codec *codec;

	pr_info("%s: received HPHL OCP irq\n", __func__);

	if (tabla) {
		codec = tabla->codec;
		if (tabla->hphlocp_cnt++ < TABLA_OCP_ATTEMPT) {
			pr_info("%s: retry\n", __func__);
			snd_soc_update_bits(codec, TABLA_A_RX_HPH_OCP_CTL, 0x10,
					    0x00);
			snd_soc_update_bits(codec, TABLA_A_RX_HPH_OCP_CTL, 0x10,
					    0x10);
		} else {
			tabla_disable_irq(codec->control_data,
					  TABLA_IRQ_HPH_PA_OCPL_FAULT);
			tabla->hphlocp_cnt = 0;
			tabla->hph_status |= SND_JACK_OC_HPHL;
			if (tabla->headset_jack)
				tabla_snd_soc_jack_report(tabla,
							  tabla->headset_jack,
							  tabla->hph_status,
							  TABLA_JACK_MASK);
		}
	} else {
		pr_err("%s: Bad tabla private data\n", __func__);
	}

	return IRQ_HANDLED;
}

static irqreturn_t tabla_hphr_ocp_irq(int irq, void *data)
{
	struct tabla_priv *tabla = data;
	struct snd_soc_codec *codec;

	pr_info("%s: received HPHR OCP irq\n", __func__);

	if (tabla) {
		codec = tabla->codec;
		if (tabla->hphrocp_cnt++ < TABLA_OCP_ATTEMPT) {
			pr_info("%s: retry\n", __func__);
			snd_soc_update_bits(codec, TABLA_A_RX_HPH_OCP_CTL, 0x10,
					    0x00);
			snd_soc_update_bits(codec, TABLA_A_RX_HPH_OCP_CTL, 0x10,
					    0x10);
		} else {
			tabla_disable_irq(codec->control_data,
					  TABLA_IRQ_HPH_PA_OCPR_FAULT);
			tabla->hphrocp_cnt = 0;
			tabla->hph_status |= SND_JACK_OC_HPHR;
			if (tabla->headset_jack)
				tabla_snd_soc_jack_report(tabla,
							  tabla->headset_jack,
							  tabla->hph_status,
							  TABLA_JACK_MASK);
		}
	} else {
		pr_err("%s: Bad tabla private data\n", __func__);
	}

	return IRQ_HANDLED;
}

static void tabla_sync_hph_state(struct tabla_priv *tabla)
{
	if (test_and_clear_bit(TABLA_HPHR_PA_OFF_ACK,
			       &tabla->hph_pa_dac_state)) {
		pr_debug("%s: HPHR clear flag and enable PA\n", __func__);
		snd_soc_update_bits(tabla->codec, TABLA_A_RX_HPH_CNP_EN, 0x10,
				    1 << 4);
	}
	if (test_and_clear_bit(TABLA_HPHL_PA_OFF_ACK,
			       &tabla->hph_pa_dac_state)) {
		pr_debug("%s: HPHL clear flag and enable PA\n", __func__);
		snd_soc_update_bits(tabla->codec, TABLA_A_RX_HPH_CNP_EN, 0x20,
				    1 << 5);
	}

	if (test_and_clear_bit(TABLA_HPHR_DAC_OFF_ACK,
			       &tabla->hph_pa_dac_state)) {
		pr_debug("%s: HPHR clear flag and enable DAC\n", __func__);
		snd_soc_update_bits(tabla->codec, TABLA_A_RX_HPH_R_DAC_CTL,
				    0xC0, 0xC0);
	}
	if (test_and_clear_bit(TABLA_HPHL_DAC_OFF_ACK,
			       &tabla->hph_pa_dac_state)) {
		pr_debug("%s: HPHL clear flag and enable DAC\n", __func__);
		snd_soc_update_bits(tabla->codec, TABLA_A_RX_HPH_L_DAC_CTL,
				    0xC0, 0xC0);
	}
}

static irqreturn_t tabla_hs_insert_irq(int irq, void *data)
{
	struct tabla_priv *priv = data;
	struct snd_soc_codec *codec = priv->codec;
	int ldo_h_on, micb_cfilt_on;
	short mic_voltage;
	short threshold_no_mic = 0xF7F6;
	short threshold_fake_insert = 0xFD30;
	u8 is_removal;

	pr_debug("%s\n", __func__);
	tabla_disable_irq(codec->control_data, TABLA_IRQ_MBHC_INSERTION);
	tabla_lock_sleep(priv);

	is_removal = snd_soc_read(codec, TABLA_A_CDC_MBHC_INT_CTL) & 0x02;
	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_INT_CTL, 0x03, 0x00);

	/* Turn off both HPH and MIC line schmitt triggers */
	snd_soc_update_bits(codec, priv->mbhc_bias_regs.mbhc_reg,
			0x90, 0x00);
	snd_soc_update_bits(codec, TABLA_A_MBHC_HPH, 0x13, 0x00);

	if (priv->fake_insert_context) {
		pr_debug("%s: fake context interrupt, reset insertion\n",
			__func__);
		priv->fake_insert_context = false;
		tabla_codec_shutdown_hs_polling(codec);
		tabla_codec_enable_hs_detect(codec, 1);
		return IRQ_HANDLED;
	}


	ldo_h_on = snd_soc_read(codec, TABLA_A_LDO_H_MODE_1) & 0x80;
	micb_cfilt_on = snd_soc_read(codec,
					priv->mbhc_bias_regs.cfilt_ctl) & 0x80;

	if (!ldo_h_on)
		snd_soc_update_bits(codec, TABLA_A_LDO_H_MODE_1, 0x80, 0x80);
	if (!micb_cfilt_on)
		snd_soc_update_bits(codec, priv->mbhc_bias_regs.cfilt_ctl,
							0x80, 0x80);

	usleep_range(priv->calibration->setup_plug_removal_delay,
		priv->calibration->setup_plug_removal_delay);

	if (!ldo_h_on)
		snd_soc_update_bits(codec, TABLA_A_LDO_H_MODE_1, 0x80, 0x0);
	if (!micb_cfilt_on)
		snd_soc_update_bits(codec, priv->mbhc_bias_regs.cfilt_ctl,
							0x80, 0x0);

	if (is_removal) {
		/*
		 * If headphone is removed while playback is in progress,
		 * it is possible that micbias will be switched to VDDIO.
		 */
		if (priv->mbhc_micbias_switched)
			tabla_codec_switch_micbias(codec, 0);
		priv->hph_status &= ~SND_JACK_HEADPHONE;

		/* If headphone PA is on, check if userspace receives
		 * removal event to sync-up PA's state */
		if (tabla_is_hph_pa_on(codec)) {
			set_bit(TABLA_HPHL_PA_OFF_ACK, &priv->hph_pa_dac_state);
			set_bit(TABLA_HPHR_PA_OFF_ACK, &priv->hph_pa_dac_state);
		}

		if (tabla_is_hph_dac_on(codec, 1))
			set_bit(TABLA_HPHL_DAC_OFF_ACK,
				&priv->hph_pa_dac_state);
		if (tabla_is_hph_dac_on(codec, 0))
			set_bit(TABLA_HPHR_DAC_OFF_ACK,
				&priv->hph_pa_dac_state);

		if (priv->headset_jack) {
			pr_debug("%s: Reporting removal\n", __func__);
			tabla_snd_soc_jack_report(priv, priv->headset_jack,
						  priv->hph_status,
						  TABLA_JACK_MASK);
		}
		tabla_codec_shutdown_hs_removal_detect(codec);
		tabla_codec_enable_hs_detect(codec, 1);
		tabla_unlock_sleep(priv);
		return IRQ_HANDLED;
	}

	mic_voltage = tabla_codec_setup_hs_polling(codec);

	if (mic_voltage > threshold_fake_insert) {
		pr_debug("%s: Fake insertion interrupt, mic_voltage = %x\n",
			__func__, mic_voltage);

		/* Disable HPH trigger and enable MIC line trigger */
		snd_soc_update_bits(codec, TABLA_A_MBHC_HPH, 0x12, 0x00);

		snd_soc_update_bits(codec, priv->mbhc_bias_regs.mbhc_reg, 0x60,
			priv->calibration->mic_current << 5);
		snd_soc_update_bits(codec, priv->mbhc_bias_regs.mbhc_reg,
			0x80, 0x80);
		usleep_range(priv->calibration->mic_pid,
			priv->calibration->mic_pid);
		snd_soc_update_bits(codec, priv->mbhc_bias_regs.mbhc_reg,
			0x10, 0x10);

		/* Setup for insertion detection */
		snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_INT_CTL, 0x2, 0);
		priv->fake_insert_context = true;
		tabla_enable_irq(codec->control_data, TABLA_IRQ_MBHC_INSERTION);
		snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_INT_CTL, 0x1, 0x1);

	} else if (mic_voltage < threshold_no_mic) {
		pr_debug("%s: Headphone Detected, mic_voltage = %x\n",
			__func__, mic_voltage);
		priv->hph_status |= SND_JACK_HEADPHONE;
		if (priv->headset_jack) {
			pr_debug("%s: Reporting insertion %d\n", __func__,
				SND_JACK_HEADPHONE);
			tabla_snd_soc_jack_report(priv, priv->headset_jack,
						  priv->hph_status,
						  TABLA_JACK_MASK);
		}
		tabla_codec_shutdown_hs_polling(codec);
		tabla_codec_enable_hs_detect(codec, 0);
		tabla_sync_hph_state(priv);
	} else {
		pr_debug("%s: Headset detected, mic_voltage = %x\n",
			__func__, mic_voltage);
		priv->hph_status |= SND_JACK_HEADSET;
		if (priv->headset_jack) {
			pr_debug("%s: Reporting insertion %d\n", __func__,
				SND_JACK_HEADSET);
			tabla_snd_soc_jack_report(priv, priv->headset_jack,
						  priv->hph_status,
						  TABLA_JACK_MASK);
		}
		tabla_codec_start_hs_polling(codec);
		tabla_sync_hph_state(priv);
	}

	tabla_unlock_sleep(priv);
	return IRQ_HANDLED;
}

static irqreturn_t tabla_hs_remove_irq(int irq, void *data)
{
	struct tabla_priv *priv = data;
	struct snd_soc_codec *codec = priv->codec;
	short bias_value;

	tabla_disable_irq(codec->control_data, TABLA_IRQ_MBHC_REMOVAL);
	tabla_disable_irq(codec->control_data, TABLA_IRQ_MBHC_POTENTIAL);
	tabla_disable_irq(codec->control_data, TABLA_IRQ_MBHC_RELEASE);
	tabla_lock_sleep(priv);

	usleep_range(priv->calibration->shutdown_plug_removal,
		priv->calibration->shutdown_plug_removal);

	bias_value = tabla_codec_measure_micbias_voltage(codec, 1);
	pr_debug("removal interrupt, bias value is %d\n", bias_value);

	if (bias_value < -90) {
		pr_debug("False alarm, headset not actually removed\n");
		tabla_codec_start_hs_polling(codec);
	} else {
		/*
		 * If this removal is not false, first check the micbias
		 * switch status and switch it to LDOH if it is already
		 * switched to VDDIO.
		 */
		if (priv->mbhc_micbias_switched)
			tabla_codec_switch_micbias(codec, 0);
		priv->hph_status &= ~SND_JACK_HEADSET;
		if (priv->headset_jack) {
			pr_debug("%s: Reporting removal\n", __func__);
			tabla_snd_soc_jack_report(priv, priv->headset_jack, 0,
						  TABLA_JACK_MASK);
		}
		tabla_codec_shutdown_hs_polling(codec);

		tabla_codec_enable_hs_detect(codec, 1);
	}

	tabla_unlock_sleep(priv);
	return IRQ_HANDLED;
}

static unsigned long slimbus_value;

static irqreturn_t tabla_slimbus_irq(int irq, void *data)
{
	struct tabla_priv *priv = data;
	struct snd_soc_codec *codec = priv->codec;
	int i, j;
	u8 val;

	tabla_lock_sleep(priv);

	for (i = 0; i < TABLA_SLIM_NUM_PORT_REG; i++) {
		slimbus_value = tabla_interface_reg_read(codec->control_data,
			TABLA_SLIM_PGD_PORT_INT_STATUS0 + i);
		for_each_set_bit(j, &slimbus_value, BITS_PER_BYTE) {
			val = tabla_interface_reg_read(codec->control_data,
				TABLA_SLIM_PGD_PORT_INT_SOURCE0 + i*8 + j);
			if (val & 0x1)
				pr_err_ratelimited("overflow error on port %x,"
					" value %x\n", i*8 + j, val);
			if (val & 0x2)
				pr_err_ratelimited("underflow error on port %x,"
					" value %x\n", i*8 + j, val);
		}
		tabla_interface_reg_write(codec->control_data,
			TABLA_SLIM_PGD_PORT_INT_CLR0 + i, 0xFF);
	}

	tabla_unlock_sleep(priv);
	return IRQ_HANDLED;
}


static int tabla_handle_pdata(struct tabla_priv *tabla)
{
	struct snd_soc_codec *codec = tabla->codec;
	struct tabla_pdata *pdata = tabla->pdata;
	int k1, k2, k3, rc = 0;
	u8 leg_mode = pdata->amic_settings.legacy_mode;
	u8 txfe_bypass = pdata->amic_settings.txfe_enable;
	u8 txfe_buff = pdata->amic_settings.txfe_buff;
	u8 flag = pdata->amic_settings.use_pdata;
	u8 i = 0, j = 0;
	u8 val_txfe = 0, value = 0;

	if (!pdata) {
		rc = -ENODEV;
		goto done;
	}

	/* Make sure settings are correct */
	if ((pdata->micbias.ldoh_v > TABLA_LDOH_2P85_V) ||
	    (pdata->micbias.bias1_cfilt_sel > TABLA_CFILT3_SEL) ||
	    (pdata->micbias.bias2_cfilt_sel > TABLA_CFILT3_SEL) ||
	    (pdata->micbias.bias3_cfilt_sel > TABLA_CFILT3_SEL) ||
	    (pdata->micbias.bias4_cfilt_sel > TABLA_CFILT3_SEL)) {
		rc = -EINVAL;
		goto done;
	}

	/* figure out k value */
	k1 = tabla_find_k_value(pdata->micbias.ldoh_v,
		pdata->micbias.cfilt1_mv);
	k2 = tabla_find_k_value(pdata->micbias.ldoh_v,
		pdata->micbias.cfilt2_mv);
	k3 = tabla_find_k_value(pdata->micbias.ldoh_v,
		pdata->micbias.cfilt3_mv);

	if (IS_ERR_VALUE(k1) || IS_ERR_VALUE(k2) || IS_ERR_VALUE(k3)) {
		rc = -EINVAL;
		goto done;
	}

	/* Set voltage level and always use LDO */
	snd_soc_update_bits(codec, TABLA_A_LDO_H_MODE_1, 0x0C,
		(pdata->micbias.ldoh_v << 2));

	snd_soc_update_bits(codec, TABLA_A_MICB_CFILT_1_VAL, 0xFC,
		(k1 << 2));
	snd_soc_update_bits(codec, TABLA_A_MICB_CFILT_2_VAL, 0xFC,
		(k2 << 2));
	snd_soc_update_bits(codec, TABLA_A_MICB_CFILT_3_VAL, 0xFC,
		(k3 << 2));

	snd_soc_update_bits(codec, TABLA_A_MICB_1_CTL, 0x60,
		(pdata->micbias.bias1_cfilt_sel << 5));
	snd_soc_update_bits(codec, TABLA_A_MICB_2_CTL, 0x60,
		(pdata->micbias.bias2_cfilt_sel << 5));
	snd_soc_update_bits(codec, TABLA_A_MICB_3_CTL, 0x60,
		(pdata->micbias.bias3_cfilt_sel << 5));
	snd_soc_update_bits(codec, TABLA_A_MICB_4_CTL, 0x60,
		(pdata->micbias.bias4_cfilt_sel << 5));

	for (i = 0; i < 6; j++, i += 2) {
		if (flag & (0x01 << i)) {
			value = (leg_mode & (0x01 << i)) ? 0x10 : 0x00;
			val_txfe = (txfe_bypass & (0x01 << i)) ? 0x20 : 0x00;
			val_txfe = val_txfe |
				((txfe_buff & (0x01 << i)) ? 0x10 : 0x00);
			snd_soc_update_bits(codec, TABLA_A_TX_1_2_EN + j * 10,
				0x10, value);
			snd_soc_update_bits(codec,
				TABLA_A_TX_1_2_TEST_EN + j * 10,
				0x30, val_txfe);
		}
		if (flag & (0x01 << (i + 1))) {
			value = (leg_mode & (0x01 << (i + 1))) ? 0x01 : 0x00;
			val_txfe = (txfe_bypass &
					(0x01 << (i + 1))) ? 0x02 : 0x00;
			val_txfe |= (txfe_buff &
					(0x01 << (i + 1))) ? 0x01 : 0x00;
			snd_soc_update_bits(codec, TABLA_A_TX_1_2_EN + j * 10,
				0x01, value);
			snd_soc_update_bits(codec,
				TABLA_A_TX_1_2_TEST_EN + j * 10,
				0x03, val_txfe);
		}
	}
	if (flag & 0x40) {
		value = (leg_mode & 0x40) ? 0x10 : 0x00;
		value = value | ((txfe_bypass & 0x40) ? 0x02 : 0x00);
		value = value | ((txfe_buff & 0x40) ? 0x01 : 0x00);
		snd_soc_update_bits(codec, TABLA_A_TX_7_MBHC_EN,
			0x13, value);
	}

	if (pdata->ocp.use_pdata) {
		/* not defined in CODEC specification */
		if (pdata->ocp.hph_ocp_limit == 1 ||
			pdata->ocp.hph_ocp_limit == 5) {
			rc = -EINVAL;
			goto done;
		}
		snd_soc_update_bits(codec, TABLA_A_RX_COM_OCP_CTL,
			0x0F, pdata->ocp.num_attempts);
		snd_soc_write(codec, TABLA_A_RX_COM_OCP_COUNT,
			((pdata->ocp.run_time << 4) | pdata->ocp.wait_time));
		snd_soc_update_bits(codec, TABLA_A_RX_HPH_OCP_CTL,
			0xE0, (pdata->ocp.hph_ocp_limit << 5));
	}
done:
	return rc;
}

static const struct tabla_reg_mask_val tabla_1_1_reg_defaults[] = {

	/* Tabla 1.1 MICBIAS changes */
	TABLA_REG_VAL(TABLA_A_MICB_1_INT_RBIAS, 0x24),
	TABLA_REG_VAL(TABLA_A_MICB_2_INT_RBIAS, 0x24),
	TABLA_REG_VAL(TABLA_A_MICB_3_INT_RBIAS, 0x24),
	TABLA_REG_VAL(TABLA_A_MICB_4_INT_RBIAS, 0x24),

	/* Tabla 1.1 HPH changes */
	TABLA_REG_VAL(TABLA_A_RX_HPH_BIAS_PA, 0x57),
	TABLA_REG_VAL(TABLA_A_RX_HPH_BIAS_LDO, 0x56),

	/* Tabla 1.1 EAR PA changes */
	TABLA_REG_VAL(TABLA_A_RX_EAR_BIAS_PA, 0xA6),
	TABLA_REG_VAL(TABLA_A_RX_EAR_GAIN, 0x02),
	TABLA_REG_VAL(TABLA_A_RX_EAR_VCM, 0x03),

	/* Tabla 1.1 Lineout_5 Changes */
	TABLA_REG_VAL(TABLA_A_RX_LINE_5_GAIN, 0x10),

	/* Tabla 1.1 RX Changes */
	TABLA_REG_VAL(TABLA_A_CDC_RX1_B5_CTL, 0x78),
	TABLA_REG_VAL(TABLA_A_CDC_RX2_B5_CTL, 0x78),
	TABLA_REG_VAL(TABLA_A_CDC_RX3_B5_CTL, 0x78),
	TABLA_REG_VAL(TABLA_A_CDC_RX4_B5_CTL, 0x78),
	TABLA_REG_VAL(TABLA_A_CDC_RX5_B5_CTL, 0x78),
	TABLA_REG_VAL(TABLA_A_CDC_RX6_B5_CTL, 0x78),
	TABLA_REG_VAL(TABLA_A_CDC_RX7_B5_CTL, 0x78),

	/* Tabla 1.1 RX1 and RX2 Changes */
	TABLA_REG_VAL(TABLA_A_CDC_RX1_B6_CTL, 0xA0),
	TABLA_REG_VAL(TABLA_A_CDC_RX2_B6_CTL, 0xA0),

	/* Tabla 1.1 RX3 to RX7 Changes */
	TABLA_REG_VAL(TABLA_A_CDC_RX3_B6_CTL, 0x80),
	TABLA_REG_VAL(TABLA_A_CDC_RX4_B6_CTL, 0x80),
	TABLA_REG_VAL(TABLA_A_CDC_RX5_B6_CTL, 0x80),
	TABLA_REG_VAL(TABLA_A_CDC_RX6_B6_CTL, 0x80),
	TABLA_REG_VAL(TABLA_A_CDC_RX7_B6_CTL, 0x80),

	/* Tabla 1.1 CLASSG Changes */
	TABLA_REG_VAL(TABLA_A_CDC_CLSG_FREQ_THRESH_B3_CTL, 0x1B),
};

static const struct tabla_reg_mask_val tabla_2_0_reg_defaults[] = {

	/* Tabla 2.0 MICBIAS changes */
	TABLA_REG_VAL(TABLA_A_MICB_2_MBHC, 0x02),
};

static void tabla_update_reg_defaults(struct snd_soc_codec *codec)
{
	u32 i;

	for (i = 0; i < ARRAY_SIZE(tabla_1_1_reg_defaults); i++)
		snd_soc_write(codec, tabla_1_1_reg_defaults[i].reg,
				tabla_1_1_reg_defaults[i].val);

	for (i = 0; i < ARRAY_SIZE(tabla_2_0_reg_defaults); i++)
		snd_soc_write(codec, tabla_2_0_reg_defaults[i].reg,
				tabla_2_0_reg_defaults[i].val);
}

static const struct tabla_reg_mask_val tabla_codec_reg_init_val[] = {
	/* Initialize current threshold to 350MA
	 * number of wait and run cycles to 4096
	 */
	{TABLA_A_RX_HPH_OCP_CTL, 0xE0, 0x60},
	{TABLA_A_RX_COM_OCP_COUNT, 0xFF, 0xFF},

	{TABLA_A_QFUSE_CTL, 0xFF, 0x03},

	/* Initialize gain registers to use register gain */
	{TABLA_A_RX_HPH_L_GAIN, 0x10, 0x10},
	{TABLA_A_RX_HPH_R_GAIN, 0x10, 0x10},
	{TABLA_A_RX_LINE_1_GAIN, 0x10, 0x10},
	{TABLA_A_RX_LINE_2_GAIN, 0x10, 0x10},
	{TABLA_A_RX_LINE_3_GAIN, 0x10, 0x10},
	{TABLA_A_RX_LINE_4_GAIN, 0x10, 0x10},

	/* Initialize mic biases to differential mode */
	{TABLA_A_MICB_1_INT_RBIAS, 0x24, 0x24},
	{TABLA_A_MICB_2_INT_RBIAS, 0x24, 0x24},
	{TABLA_A_MICB_3_INT_RBIAS, 0x24, 0x24},
	{TABLA_A_MICB_4_INT_RBIAS, 0x24, 0x24},

	{TABLA_A_CDC_CONN_CLSG_CTL, 0x3C, 0x14},

	/* Use 16 bit sample size for TX1 to TX6 */
	{TABLA_A_CDC_CONN_TX_SB_B1_CTL, 0x30, 0x20},
	{TABLA_A_CDC_CONN_TX_SB_B2_CTL, 0x30, 0x20},
	{TABLA_A_CDC_CONN_TX_SB_B3_CTL, 0x30, 0x20},
	{TABLA_A_CDC_CONN_TX_SB_B4_CTL, 0x30, 0x20},
	{TABLA_A_CDC_CONN_TX_SB_B5_CTL, 0x30, 0x20},
	{TABLA_A_CDC_CONN_TX_SB_B6_CTL, 0x30, 0x20},

	/* Use 16 bit sample size for TX7 to TX10 */
	{TABLA_A_CDC_CONN_TX_SB_B7_CTL, 0x60, 0x40},
	{TABLA_A_CDC_CONN_TX_SB_B8_CTL, 0x60, 0x40},
	{TABLA_A_CDC_CONN_TX_SB_B9_CTL, 0x60, 0x40},
	{TABLA_A_CDC_CONN_TX_SB_B10_CTL, 0x60, 0x40},

	/* Use 16 bit sample size for RX */
	{TABLA_A_CDC_CONN_RX_SB_B1_CTL, 0xFF, 0xAA},
	{TABLA_A_CDC_CONN_RX_SB_B2_CTL, 0xFF, 0xAA},

	/*enable HPF filter for TX paths */
	{TABLA_A_CDC_TX1_MUX_CTL, 0x8, 0x0},
	{TABLA_A_CDC_TX2_MUX_CTL, 0x8, 0x0},
	{TABLA_A_CDC_TX3_MUX_CTL, 0x8, 0x0},
	{TABLA_A_CDC_TX4_MUX_CTL, 0x8, 0x0},
	{TABLA_A_CDC_TX5_MUX_CTL, 0x8, 0x0},
	{TABLA_A_CDC_TX6_MUX_CTL, 0x8, 0x0},
	{TABLA_A_CDC_TX7_MUX_CTL, 0x8, 0x0},
	{TABLA_A_CDC_TX8_MUX_CTL, 0x8, 0x0},
	{TABLA_A_CDC_TX9_MUX_CTL, 0x8, 0x0},
	{TABLA_A_CDC_TX10_MUX_CTL, 0x8, 0x0},
};

static void tabla_codec_init_reg(struct snd_soc_codec *codec)
{
	u32 i;

	for (i = 0; i < ARRAY_SIZE(tabla_codec_reg_init_val); i++)
		snd_soc_update_bits(codec, tabla_codec_reg_init_val[i].reg,
				tabla_codec_reg_init_val[i].mask,
				tabla_codec_reg_init_val[i].val);
}

static int tabla_codec_probe(struct snd_soc_codec *codec)
{
	struct tabla *control;
	struct tabla_priv *tabla;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	int ret = 0;
	int i;
	u8 tabla_version;

	codec->control_data = dev_get_drvdata(codec->dev->parent);
	control = codec->control_data;

	tabla = kzalloc(sizeof(struct tabla_priv), GFP_KERNEL);
	if (!tabla) {
		dev_err(codec->dev, "Failed to allocate private data\n");
		return -ENOMEM;
	}

	/* Make sure mbhc micbias register addresses are zeroed out */
	memset(&tabla->mbhc_bias_regs, 0,
		sizeof(struct mbhc_micbias_regs));
	tabla->cfilt_k_value = 0;
	tabla->mbhc_micbias_switched = false;

	snd_soc_codec_set_drvdata(codec, tabla);

	tabla->mclk_enabled = false;
	tabla->bandgap_type = TABLA_BANDGAP_OFF;
	tabla->clock_active = false;
	tabla->config_mode_active = false;
	tabla->mbhc_polling_active = false;
	tabla->fake_insert_context = false;
	tabla->no_mic_headset_override = false;
	tabla->codec = codec;
	tabla->pdata = dev_get_platdata(codec->dev->parent);
	tabla->intf_type = tabla_get_intf_type();
	atomic_set(&tabla->pm_cnt, 1);
	init_waitqueue_head(&tabla->pm_wq);

	tabla_update_reg_defaults(codec);
	tabla_codec_init_reg(codec);

	ret = tabla_handle_pdata(tabla);
	if (IS_ERR_VALUE(ret)) {
		pr_err("%s: bad pdata\n", __func__);
		goto err_pdata;
	}

	/* TODO only enable bandgap when necessary in order to save power */
	tabla_codec_enable_bandgap(codec, TABLA_BANDGAP_AUDIO_MODE);
	tabla_codec_enable_clock_block(codec, 0);

	snd_soc_add_controls(codec, tabla_snd_controls,
		ARRAY_SIZE(tabla_snd_controls));
	snd_soc_dapm_new_controls(dapm, tabla_dapm_widgets,
		ARRAY_SIZE(tabla_dapm_widgets));
	if (tabla->intf_type == TABLA_INTERFACE_TYPE_I2C) {
		snd_soc_dapm_new_controls(dapm, tabla_dapm_i2s_widgets,
			ARRAY_SIZE(tabla_dapm_i2s_widgets));
		snd_soc_dapm_add_routes(dapm, audio_i2s_map,
			ARRAY_SIZE(audio_i2s_map));
	}
	snd_soc_dapm_add_routes(dapm, audio_map, ARRAY_SIZE(audio_map));

	tabla_version = snd_soc_read(codec, TABLA_A_CHIP_VERSION);
	pr_info("%s : Tabla version reg 0x%2x\n", __func__, (u32)tabla_version);

	tabla_version &=  0x1F;
	pr_info("%s : Tabla version %u\n", __func__, (u32)tabla_version);

	if ((tabla_version == TABLA_VERSION_1_0) ||
		(tabla_version == TABLA_VERSION_1_1)) {
		snd_soc_dapm_add_routes(dapm, tabla_1_x_lineout_2_to_4_map,
			 ARRAY_SIZE(tabla_1_x_lineout_2_to_4_map));

	} else if (tabla_version == TABLA_VERSION_2_0) {
		snd_soc_dapm_add_routes(dapm, tabla_2_x_lineout_2_to_4_map,
			 ARRAY_SIZE(tabla_2_x_lineout_2_to_4_map));
	} else  {
		pr_err("%s : ERROR.  Unsupported Tabla version 0x%2x\n",
				__func__, (u32)tabla_version);
		goto err_pdata;
	}

	snd_soc_dapm_sync(dapm);

	ret = tabla_request_irq(codec->control_data, TABLA_IRQ_MBHC_INSERTION,
		tabla_hs_insert_irq, "Headset insert detect", tabla);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
			TABLA_IRQ_MBHC_INSERTION);
		goto err_insert_irq;
	}
	tabla_disable_irq(codec->control_data, TABLA_IRQ_MBHC_INSERTION);

	ret = tabla_request_irq(codec->control_data, TABLA_IRQ_MBHC_REMOVAL,
		tabla_hs_remove_irq, "Headset remove detect", tabla);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
			TABLA_IRQ_MBHC_REMOVAL);
		goto err_remove_irq;
	}
	tabla_disable_irq(codec->control_data, TABLA_IRQ_MBHC_REMOVAL);

	ret = tabla_request_irq(codec->control_data, TABLA_IRQ_MBHC_POTENTIAL,
		tabla_dce_handler, "DC Estimation detect", tabla);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
			TABLA_IRQ_MBHC_POTENTIAL);
		goto err_potential_irq;
	}
	tabla_disable_irq(codec->control_data, TABLA_IRQ_MBHC_POTENTIAL);

	ret = tabla_request_irq(codec->control_data, TABLA_IRQ_MBHC_RELEASE,
		tabla_release_handler, "Button Release detect", tabla);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
			TABLA_IRQ_MBHC_RELEASE);
		goto err_release_irq;
	}
	tabla_disable_irq(codec->control_data, TABLA_IRQ_MBHC_RELEASE);

	ret = tabla_request_irq(codec->control_data, TABLA_IRQ_SLIMBUS,
		tabla_slimbus_irq, "SLIMBUS Slave", tabla);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
			TABLA_IRQ_SLIMBUS);
		goto err_slimbus_irq;
	}

	for (i = 0; i < TABLA_SLIM_NUM_PORT_REG; i++)
		tabla_interface_reg_write(codec->control_data,
			TABLA_SLIM_PGD_PORT_INT_EN0 + i, 0xFF);

	ret = tabla_request_irq(codec->control_data,
		TABLA_IRQ_HPH_PA_OCPL_FAULT, tabla_hphl_ocp_irq,
		"HPH_L OCP detect", tabla);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
			TABLA_IRQ_HPH_PA_OCPL_FAULT);
		goto err_hphl_ocp_irq;
	}
	tabla_disable_irq(codec->control_data, TABLA_IRQ_HPH_PA_OCPL_FAULT);

	ret = tabla_request_irq(codec->control_data,
		TABLA_IRQ_HPH_PA_OCPR_FAULT, tabla_hphr_ocp_irq,
		"HPH_R OCP detect", tabla);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
			TABLA_IRQ_HPH_PA_OCPR_FAULT);
		goto err_hphr_ocp_irq;
	}
	tabla_disable_irq(codec->control_data, TABLA_IRQ_HPH_PA_OCPR_FAULT);

#ifdef CONFIG_DEBUG_FS
	debug_tabla_priv = tabla;
#endif

	return ret;

err_hphr_ocp_irq:
	tabla_free_irq(codec->control_data, TABLA_IRQ_HPH_PA_OCPL_FAULT, tabla);
err_hphl_ocp_irq:
	tabla_free_irq(codec->control_data, TABLA_IRQ_SLIMBUS, tabla);
err_slimbus_irq:
	tabla_free_irq(codec->control_data, TABLA_IRQ_MBHC_RELEASE, tabla);
err_release_irq:
	tabla_free_irq(codec->control_data, TABLA_IRQ_MBHC_POTENTIAL, tabla);
err_potential_irq:
	tabla_free_irq(codec->control_data, TABLA_IRQ_MBHC_REMOVAL, tabla);
err_remove_irq:
	tabla_free_irq(codec->control_data, TABLA_IRQ_MBHC_INSERTION, tabla);
err_insert_irq:
err_pdata:
	kfree(tabla);
	return ret;
}
static int tabla_codec_remove(struct snd_soc_codec *codec)
{
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);
	tabla_free_irq(codec->control_data, TABLA_IRQ_SLIMBUS, tabla);
	tabla_free_irq(codec->control_data, TABLA_IRQ_MBHC_RELEASE, tabla);
	tabla_free_irq(codec->control_data, TABLA_IRQ_MBHC_POTENTIAL, tabla);
	tabla_free_irq(codec->control_data, TABLA_IRQ_MBHC_REMOVAL, tabla);
	tabla_free_irq(codec->control_data, TABLA_IRQ_MBHC_INSERTION, tabla);
	tabla_codec_disable_clock_block(codec);
	tabla_codec_enable_bandgap(codec, TABLA_BANDGAP_OFF);
	kfree(tabla);
	return 0;
}
static struct snd_soc_codec_driver soc_codec_dev_tabla = {
	.probe	= tabla_codec_probe,
	.remove	= tabla_codec_remove,
	.read = tabla_read,
	.write = tabla_write,

	.readable_register = tabla_readable,
	.volatile_register = tabla_volatile,

	.reg_cache_size = TABLA_CACHE_SIZE,
	.reg_cache_default = tabla_reg_defaults,
	.reg_word_size = 1,
};

#ifdef CONFIG_DEBUG_FS
static struct dentry *debugfs_poke;

static int codec_debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t codec_debug_write(struct file *filp,
	const char __user *ubuf, size_t cnt, loff_t *ppos)
{
	char lbuf[32];
	char *buf;
	int rc;

	if (cnt > sizeof(lbuf) - 1)
		return -EINVAL;

	rc = copy_from_user(lbuf, ubuf, cnt);
	if (rc)
		return -EFAULT;

	lbuf[cnt] = '\0';
	buf = (char *)lbuf;
	debug_tabla_priv->no_mic_headset_override = (*strsep(&buf, " ") == '0')
		? false : true;

	return rc;
}

static const struct file_operations codec_debug_ops = {
	.open = codec_debug_open,
	.write = codec_debug_write,
};
#endif

#ifdef CONFIG_PM
static int tabla_suspend(struct device *dev)
{
	int ret = 0, cnt;
	struct platform_device *pdev = to_platform_device(dev);
	struct tabla_priv *tabla = platform_get_drvdata(pdev);

	cnt = atomic_read(&tabla->pm_cnt);
	if (cnt > 0) {
		if (wait_event_timeout(tabla->pm_wq,
				       (atomic_cmpxchg(&tabla->pm_cnt, 1, 0)
						== 1), 5 * HZ)) {
			dev_dbg(dev, "system suspend pm_cnt %d\n",
				atomic_read(&tabla->pm_cnt));
		} else {
			dev_err(dev, "%s timed out pm_cnt = %d\n",
				__func__, atomic_read(&tabla->pm_cnt));
			WARN_ON_ONCE(1);
			ret = -EBUSY;
		}
	} else if (cnt == 0)
		dev_warn(dev, "system is already in suspend, pm_cnt %d\n",
			 atomic_read(&tabla->pm_cnt));
	else {
		WARN(1, "unexpected pm_cnt %d\n", cnt);
		ret = -EFAULT;
	}

	return ret;
}

static int tabla_resume(struct device *dev)
{
	int ret = 0, cnt;
	struct platform_device *pdev = to_platform_device(dev);
	struct tabla_priv *tabla = platform_get_drvdata(pdev);

	cnt = atomic_cmpxchg(&tabla->pm_cnt, 0, 1);
	if (cnt == 0) {
		dev_dbg(dev, "system resume, pm_cnt %d\n",
			atomic_read(&tabla->pm_cnt));
		wake_up_all(&tabla->pm_wq);
	} else if (cnt > 0)
		dev_warn(dev, "system is already awake, pm_cnt %d\n", cnt);
	else {
		WARN(1, "unexpected pm_cnt %d\n", cnt);
		ret = -EFAULT;
	}

	return ret;
}

static const struct dev_pm_ops tabla_pm_ops = {
	.suspend	= tabla_suspend,
	.resume		= tabla_resume,
};
#endif

static int __devinit tabla_probe(struct platform_device *pdev)
{
	int ret = 0;
#ifdef CONFIG_DEBUG_FS
	debugfs_poke = debugfs_create_file("TRRS",
		S_IFREG | S_IRUGO, NULL, (void *) "TRRS", &codec_debug_ops);

#endif
	if (tabla_get_intf_type() == TABLA_INTERFACE_TYPE_SLIMBUS)
		ret = snd_soc_register_codec(&pdev->dev, &soc_codec_dev_tabla,
			tabla_dai, ARRAY_SIZE(tabla_dai));
	else if (tabla_get_intf_type() == TABLA_INTERFACE_TYPE_I2C)
		ret = snd_soc_register_codec(&pdev->dev, &soc_codec_dev_tabla,
			tabla_i2s_dai, ARRAY_SIZE(tabla_i2s_dai));
	return ret;
}
static int __devexit tabla_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);

#ifdef CONFIG_DEBUG_FS
	debugfs_remove(debugfs_poke);
#endif
	return 0;
}
static struct platform_driver tabla_codec_driver = {
	.probe = tabla_probe,
	.remove = tabla_remove,
	.driver = {
		.name = "tabla_codec",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &tabla_pm_ops,
#endif
	},
};

static int __init tabla_codec_init(void)
{
	return platform_driver_register(&tabla_codec_driver);
}

static void __exit tabla_codec_exit(void)
{
	platform_driver_unregister(&tabla_codec_driver);
}

module_init(tabla_codec_init);
module_exit(tabla_codec_exit);

MODULE_DESCRIPTION("Tabla codec driver");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL v2");
