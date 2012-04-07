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
#include <linux/mfd/wcd9xxx/core.h>
#include <linux/mfd/wcd9xxx/wcd9xxx_registers.h>
#include <linux/mfd/wcd9xxx/wcd9304_registers.h>
#include <linux/mfd/wcd9xxx/pdata.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/jack.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/pm_runtime.h>
#include "wcd9304.h"

#define WCD9304_RATES (SNDRV_PCM_RATE_8000|SNDRV_PCM_RATE_16000|\
			SNDRV_PCM_RATE_32000|SNDRV_PCM_RATE_48000)

#define NUM_DECIMATORS 4
#define NUM_INTERPOLATORS 3
#define BITS_PER_REG 8
#define AIF1_PB 1
#define AIF1_CAP 2
#define NUM_CODEC_DAIS 2

struct sitar_codec_dai_data {
	u32 rate;
	u32 *ch_num;
	u32 ch_act;
	u32 ch_tot;
};
#define SITAR_CFILT_FAST_MODE 0x00
#define SITAR_CFILT_SLOW_MODE 0x40


#define SITAR_JACK_MASK (SND_JACK_HEADSET | SND_JACK_OC_HPHL | SND_JACK_OC_HPHR)

#define SITAR_I2S_MASTER_MODE_MASK 0x08

#define SITAR_OCP_ATTEMPT 1

static const DECLARE_TLV_DB_SCALE(digital_gain, 0, 1, 0);
static const DECLARE_TLV_DB_SCALE(line_gain, 0, 7, 1);
static const DECLARE_TLV_DB_SCALE(analog_gain, 0, 25, 1);
static struct snd_soc_dai_driver sitar_dai[];
static int sitar_codec_enable_slimtx(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event);
static int sitar_codec_enable_slimrx(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event);

enum sitar_bandgap_type {
	SITAR_BANDGAP_OFF = 0,
	SITAR_BANDGAP_AUDIO_MODE,
	SITAR_BANDGAP_MBHC_MODE,
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
enum sitar_priv_ack_flags {
	SITAR_HPHL_PA_OFF_ACK = 0,
	SITAR_HPHR_PA_OFF_ACK,
	SITAR_HPHL_DAC_OFF_ACK,
	SITAR_HPHR_DAC_OFF_ACK
};

struct sitar_priv {
	struct snd_soc_codec *codec;
	u32 adc_count;
	u32 cfilt1_cnt;
	u32 cfilt2_cnt;
	u32 cfilt3_cnt;
	u32 rx_bias_count;
	enum sitar_bandgap_type bandgap_type;
	bool mclk_enabled;
	bool clock_active;
	bool config_mode_active;
	bool mbhc_polling_active;
	bool fake_insert_context;
	int buttons_pressed;

	struct sitar_mbhc_calibration *calibration;

	struct snd_soc_jack *headset_jack;
	struct snd_soc_jack *button_jack;

	struct wcd9xxx_pdata *pdata;
	u32 anc_slot;

	bool no_mic_headset_override;
	/* Delayed work to report long button press */
	struct delayed_work btn0_dwork;

	struct mbhc_micbias_regs mbhc_bias_regs;
	u8 cfilt_k_value;
	bool mbhc_micbias_switched;

	/* track PA/DAC state */
	unsigned long hph_pa_dac_state;

	/*track sitar interface type*/
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
	/* num of slim ports required */
	struct sitar_codec_dai_data dai[NUM_CODEC_DAIS];
};

#ifdef CONFIG_DEBUG_FS
struct sitar_priv *debug_sitar_priv;
#endif


static int sitar_pa_gain_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	u8 ear_pa_gain;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	ear_pa_gain = snd_soc_read(codec, SITAR_A_RX_EAR_GAIN);

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

	pr_err("%s: ear_pa_gain = 0x%x\n", __func__, ear_pa_gain);

	return 0;
}

static int sitar_pa_gain_put(struct snd_kcontrol *kcontrol,
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

	snd_soc_write(codec, SITAR_A_RX_EAR_GAIN, ear_pa_gain);
	return 0;
}

static int sitar_get_iir_enable_audio_mixer(
					struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	int iir_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int band_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;

	ucontrol->value.integer.value[0] =
		snd_soc_read(codec, (SITAR_A_CDC_IIR1_CTL + 16 * iir_idx)) &
		(1 << band_idx);

	pr_err("%s: IIR #%d band #%d enable %d\n", __func__,
		iir_idx, band_idx,
		(uint32_t)ucontrol->value.integer.value[0]);
	return 0;
}

static int sitar_put_iir_enable_audio_mixer(
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
	snd_soc_update_bits(codec, (SITAR_A_CDC_IIR1_CTL + 16 * iir_idx),
		(1 << band_idx), (value << band_idx));

	pr_err("%s: IIR #%d band #%d enable %d\n", __func__,
		iir_idx, band_idx, value);
	return 0;
}
static uint32_t get_iir_band_coeff(struct snd_soc_codec *codec,
				int iir_idx, int band_idx,
				int coeff_idx)
{
	/* Address does not automatically update if reading */
	snd_soc_update_bits(codec,
		(SITAR_A_CDC_IIR1_COEF_B1_CTL + 16 * iir_idx),
		0x1F, band_idx * BAND_MAX + coeff_idx);

	/* Mask bits top 2 bits since they are reserved */
	return ((snd_soc_read(codec,
		(SITAR_A_CDC_IIR1_COEF_B2_CTL + 16 * iir_idx)) << 24) |
		(snd_soc_read(codec,
		(SITAR_A_CDC_IIR1_COEF_B3_CTL + 16 * iir_idx)) << 16) |
		(snd_soc_read(codec,
		(SITAR_A_CDC_IIR1_COEF_B4_CTL + 16 * iir_idx)) << 8) |
		(snd_soc_read(codec,
		(SITAR_A_CDC_IIR1_COEF_B5_CTL + 16 * iir_idx)))) &
		0x3FFFFFFF;
}

static int sitar_get_iir_band_audio_mixer(
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

	pr_err("%s: IIR #%d band #%d b0 = 0x%x\n"
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
		(SITAR_A_CDC_IIR1_COEF_B1_CTL + 16 * iir_idx),
		0x1F, band_idx * BAND_MAX + coeff_idx);

	/* Mask top 2 bits, 7-8 are reserved */
	snd_soc_update_bits(codec,
		(SITAR_A_CDC_IIR1_COEF_B2_CTL + 16 * iir_idx),
		0x3F, (value >> 24) & 0x3F);

	/* Isolate 8bits at a time */
	snd_soc_update_bits(codec,
		(SITAR_A_CDC_IIR1_COEF_B3_CTL + 16 * iir_idx),
		0xFF, (value >> 16) & 0xFF);

	snd_soc_update_bits(codec,
		(SITAR_A_CDC_IIR1_COEF_B4_CTL + 16 * iir_idx),
		0xFF, (value >> 8) & 0xFF);

	snd_soc_update_bits(codec,
		(SITAR_A_CDC_IIR1_COEF_B5_CTL + 16 * iir_idx),
		0xFF, value & 0xFF);
}

static int sitar_put_iir_band_audio_mixer(
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

	pr_err("%s: IIR #%d band #%d b0 = 0x%x\n"
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

static const char *sitar_ear_pa_gain_text[] = {"POS_6_DB", "POS_2_DB"};
static const struct soc_enum sitar_ear_pa_gain_enum[] = {
		SOC_ENUM_SINGLE_EXT(2, sitar_ear_pa_gain_text),
};

/*cut of frequency for high pass filter*/
static const char *cf_text[] = {
	"MIN_3DB_4Hz", "MIN_3DB_75Hz", "MIN_3DB_150Hz"
};

static const struct soc_enum cf_dec1_enum =
	SOC_ENUM_SINGLE(SITAR_A_CDC_TX1_MUX_CTL, 4, 3, cf_text);

static const struct soc_enum cf_rxmix1_enum =
	SOC_ENUM_SINGLE(SITAR_A_CDC_RX1_B4_CTL, 1, 3, cf_text);

static const struct snd_kcontrol_new sitar_snd_controls[] = {

	SOC_ENUM_EXT("EAR PA Gain", sitar_ear_pa_gain_enum[0],
		sitar_pa_gain_get, sitar_pa_gain_put),

	SOC_SINGLE_TLV("LINEOUT1 Volume", SITAR_A_RX_LINE_1_GAIN, 0, 12, 1,
		line_gain),
	SOC_SINGLE_TLV("LINEOUT2 Volume", SITAR_A_RX_LINE_2_GAIN, 0, 12, 1,
		line_gain),

	SOC_SINGLE_TLV("HPHL Volume", SITAR_A_RX_HPH_L_GAIN, 0, 12, 1,
		line_gain),
	SOC_SINGLE_TLV("HPHR Volume", SITAR_A_RX_HPH_R_GAIN, 0, 12, 1,
		line_gain),

	SOC_SINGLE_S8_TLV("RX1 Digital Volume", SITAR_A_CDC_RX1_VOL_CTL_B2_CTL,
		-84, 40, digital_gain),

	SOC_SINGLE_S8_TLV("DEC1 Volume", SITAR_A_CDC_TX1_VOL_CTL_GAIN, -84, 40,
		digital_gain),
	SOC_SINGLE_S8_TLV("IIR1 INP1 Volume", SITAR_A_CDC_IIR1_GAIN_B1_CTL, -84,
		40, digital_gain),
	SOC_SINGLE_S8_TLV("IIR1 INP2 Volume", SITAR_A_CDC_IIR1_GAIN_B2_CTL, -84,
		40, digital_gain),
	SOC_SINGLE_S8_TLV("IIR1 INP3 Volume", SITAR_A_CDC_IIR1_GAIN_B3_CTL, -84,
		40, digital_gain),
	SOC_SINGLE_S8_TLV("IIR1 INP4 Volume", SITAR_A_CDC_IIR1_GAIN_B4_CTL, -84,
		40, digital_gain),
	SOC_SINGLE_TLV("ADC1 Volume", SITAR_A_TX_1_2_EN, 5, 3, 0, analog_gain),
	SOC_SINGLE_TLV("ADC2 Volume", SITAR_A_TX_1_2_EN, 1, 3, 0, analog_gain),

	SOC_SINGLE("MICBIAS1 CAPLESS Switch", SITAR_A_MICB_1_CTL, 4, 1, 1),
	SOC_SINGLE("MICBIAS2 CAPLESS Switch", SITAR_A_MICB_2_CTL, 4, 1, 1),

	SOC_ENUM("TX1 HPF cut off", cf_dec1_enum),

	SOC_SINGLE("TX1 HPF Switch", SITAR_A_CDC_TX1_MUX_CTL, 3, 1, 0),

	SOC_SINGLE("RX1 HPF Switch", SITAR_A_CDC_RX1_B5_CTL, 2, 1, 0),

	SOC_ENUM("RX1 HPF cut off", cf_rxmix1_enum),

	SOC_SINGLE_EXT("IIR1 Enable Band1", IIR1, BAND1, 1, 0,
	sitar_get_iir_enable_audio_mixer, sitar_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR1 Enable Band2", IIR1, BAND2, 1, 0,
	sitar_get_iir_enable_audio_mixer, sitar_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR1 Enable Band3", IIR1, BAND3, 1, 0,
	sitar_get_iir_enable_audio_mixer, sitar_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR1 Enable Band4", IIR1, BAND4, 1, 0,
	sitar_get_iir_enable_audio_mixer, sitar_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR1 Enable Band5", IIR1, BAND5, 1, 0,
	sitar_get_iir_enable_audio_mixer, sitar_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR2 Enable Band1", IIR2, BAND1, 1, 0,
	sitar_get_iir_enable_audio_mixer, sitar_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR2 Enable Band2", IIR2, BAND2, 1, 0,
	sitar_get_iir_enable_audio_mixer, sitar_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR2 Enable Band3", IIR2, BAND3, 1, 0,
	sitar_get_iir_enable_audio_mixer, sitar_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR2 Enable Band4", IIR2, BAND4, 1, 0,
	sitar_get_iir_enable_audio_mixer, sitar_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR2 Enable Band5", IIR2, BAND5, 1, 0,
	sitar_get_iir_enable_audio_mixer, sitar_put_iir_enable_audio_mixer),

	SOC_SINGLE_MULTI_EXT("IIR1 Band1", IIR1, BAND1, 255, 0, 5,
	sitar_get_iir_band_audio_mixer, sitar_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR1 Band2", IIR1, BAND2, 255, 0, 5,
	sitar_get_iir_band_audio_mixer, sitar_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR1 Band3", IIR1, BAND3, 255, 0, 5,
	sitar_get_iir_band_audio_mixer, sitar_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR1 Band4", IIR1, BAND4, 255, 0, 5,
	sitar_get_iir_band_audio_mixer, sitar_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR1 Band5", IIR1, BAND5, 255, 0, 5,
	sitar_get_iir_band_audio_mixer, sitar_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR2 Band1", IIR2, BAND1, 255, 0, 5,
	sitar_get_iir_band_audio_mixer, sitar_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR2 Band2", IIR2, BAND2, 255, 0, 5,
	sitar_get_iir_band_audio_mixer, sitar_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR2 Band3", IIR2, BAND3, 255, 0, 5,
	sitar_get_iir_band_audio_mixer, sitar_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR2 Band4", IIR2, BAND4, 255, 0, 5,
	sitar_get_iir_band_audio_mixer, sitar_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR2 Band5", IIR2, BAND5, 255, 0, 5,
	sitar_get_iir_band_audio_mixer, sitar_put_iir_band_audio_mixer),
};

static const char *rx_mix1_text[] = {
	"ZERO", "SRC1", "SRC2", "IIR1", "IIR2", "RX1", "RX2", "RX3", "RX4",
		"RX5"
};

static const char *rx_dac1_text[] = {
	"ZERO", "RX1", "RX2"
};

static const char *rx_dac2_text[] = {
	"ZERO", "RX1",
};

static const char *rx_dac3_text[] = {
	"ZERO", "RX1", "RX1_INV", "RX2"
};

static const char *rx_dac4_text[] = {
	"ZERO", "ON"
};

static const char *sb_tx1_mux_text[] = {
	"ZERO", "RMIX1", "RMIX2", "RMIX3", "RMIX4", "RMIX5", "RMIX6", "RMIX7",
		"DEC1"
};

static const char *sb_tx2_mux_text[] = {
	"ZERO", "RMIX1", "RMIX2", "RMIX3", "RMIX4", "RMIX5", "RMIX6", "RMIX7",
		"DEC2"
};

static const char *sb_tx3_mux_text[] = {
	"ZERO", "RMIX1", "RMIX2", "RMIX3", "RMIX4", "RMIX5", "RMIX6", "RMIX7",
		"DEC3"
};

static const char *sb_tx5_mux_text[] = {
	"ZERO", "RMIX1", "RMIX2", "RMIX3", "RMIX4", "RMIX5", "RMIX6", "RMIX7",
		"DEC5"
};

static const char *dec1_mux_text[] = {
	"ZERO", "DMIC1", "ADC1", "ADC2", "ADC3", "MBADC", "DMIC4", "ANCFB1",
};

static const char *dec2_mux_text[] = {
	"ZERO", "DMIC2", "ADC1", "ADC2", "ADC3", "MBADC", "DMIC3", "ANCFB2",
};

static const char *dec3_mux_text[] = {
	"ZERO", "DMIC2", "DMIC3", "DMIC4", "ADC1", "ADC2", "ADC3", "MBADC",
};

static const char *dec4_mux_text[] = {
	"ZERO", "DMIC1", "DMIC2", "DMIC3", "DMIC4", "ADC1", "ADC2", "ADC3",
};

static const char *iir1_inp1_text[] = {
	"ZERO", "DEC1", "DEC2", "DEC3", "DEC4", "ZERO", "ZERO", "ZERO",
	"ZERO", "ZERO", "ZERO", "RX1", "RX2", "RX3", "RX4", "RX5",
};

static const struct soc_enum rx_mix1_inp1_chain_enum =
	SOC_ENUM_SINGLE(SITAR_A_CDC_CONN_RX1_B1_CTL, 0, 10, rx_mix1_text);

static const struct soc_enum rx_mix1_inp2_chain_enum =
	SOC_ENUM_SINGLE(SITAR_A_CDC_CONN_RX1_B1_CTL, 4, 10, rx_mix1_text);

static const struct soc_enum rx2_mix1_inp1_chain_enum =
	SOC_ENUM_SINGLE(SITAR_A_CDC_CONN_RX2_B1_CTL, 0, 10, rx_mix1_text);

static const struct soc_enum rx2_mix1_inp2_chain_enum =
	SOC_ENUM_SINGLE(SITAR_A_CDC_CONN_RX2_B1_CTL, 4, 10, rx_mix1_text);

static const struct soc_enum rx3_mix1_inp1_chain_enum =
	SOC_ENUM_SINGLE(SITAR_A_CDC_CONN_RX3_B1_CTL, 0, 10, rx_mix1_text);

static const struct soc_enum rx3_mix1_inp2_chain_enum =
	SOC_ENUM_SINGLE(SITAR_A_CDC_CONN_RX3_B1_CTL, 4, 10, rx_mix1_text);

static const struct soc_enum rx_dac1_enum =
	SOC_ENUM_SINGLE(SITAR_A_CDC_TOP_RDAC_DOUT_CTL, 6, 3, rx_dac1_text);

static const struct soc_enum rx_dac2_enum =
	SOC_ENUM_SINGLE(SITAR_A_CDC_TOP_RDAC_DOUT_CTL, 4, 2, rx_dac2_text);

static const struct soc_enum rx_dac3_enum =
	SOC_ENUM_SINGLE(SITAR_A_CDC_TOP_RDAC_DOUT_CTL, 2, 4, rx_dac3_text);

static const struct soc_enum rx_dac4_enum =
	SOC_ENUM_SINGLE(SITAR_A_CDC_TOP_RDAC_DOUT_CTL, 0, 2, rx_dac4_text);

static const struct soc_enum sb_tx5_mux_enum =
	SOC_ENUM_SINGLE(SITAR_A_CDC_CONN_TX_SB_B5_CTL, 0, 9, sb_tx5_mux_text);

static const struct soc_enum sb_tx3_mux_enum =
	SOC_ENUM_SINGLE(SITAR_A_CDC_CONN_TX_SB_B3_CTL, 0, 9, sb_tx3_mux_text);

static const struct soc_enum sb_tx2_mux_enum =
	SOC_ENUM_SINGLE(SITAR_A_CDC_CONN_TX_SB_B2_CTL, 0, 9, sb_tx2_mux_text);

static const struct soc_enum sb_tx1_mux_enum =
	SOC_ENUM_SINGLE(SITAR_A_CDC_CONN_TX_SB_B1_CTL, 0, 9, sb_tx1_mux_text);

static const struct soc_enum dec1_mux_enum =
	SOC_ENUM_SINGLE(SITAR_A_CDC_CONN_TX_B1_CTL, 0, 8, dec1_mux_text);

static const struct soc_enum dec2_mux_enum =
	SOC_ENUM_SINGLE(SITAR_A_CDC_CONN_TX_B1_CTL, 2, 8, dec2_mux_text);

static const struct soc_enum dec3_mux_enum =
	SOC_ENUM_SINGLE(SITAR_A_CDC_CONN_TX_B1_CTL, 4, 8, dec3_mux_text);

static const struct soc_enum dec4_mux_enum =
	SOC_ENUM_SINGLE(SITAR_A_CDC_CONN_TX_B1_CTL, 6, 8, dec4_mux_text);

static const struct soc_enum iir1_inp1_mux_enum =
	SOC_ENUM_SINGLE(SITAR_A_CDC_CONN_EQ1_B1_CTL, 0, 16, iir1_inp1_text);

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

static const struct snd_kcontrol_new rx_dac1_mux =
	SOC_DAPM_ENUM("RX1 DAC Mux", rx_dac1_enum);

static const struct snd_kcontrol_new rx_dac2_mux =
	SOC_DAPM_ENUM("RX2 DAC Mux", rx_dac2_enum);

static const struct snd_kcontrol_new rx_dac3_mux =
	SOC_DAPM_ENUM("RX3 DAC Mux", rx_dac3_enum);

static const struct snd_kcontrol_new rx_dac4_mux =
	SOC_DAPM_ENUM("RX4 DAC Mux", rx_dac4_enum);

static const struct snd_kcontrol_new sb_tx5_mux =
	SOC_DAPM_ENUM("SLIM TX5 MUX Mux", sb_tx5_mux_enum);

static const struct snd_kcontrol_new sb_tx3_mux =
	SOC_DAPM_ENUM("SLIM TX3 MUX Mux", sb_tx3_mux_enum);

static const struct snd_kcontrol_new sb_tx2_mux =
	SOC_DAPM_ENUM("SLIM TX2 MUX Mux", sb_tx2_mux_enum);

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

static const struct snd_kcontrol_new iir1_inp1_mux =
	SOC_DAPM_ENUM("IIR1 INP1 Mux", iir1_inp1_mux_enum);

static const struct snd_kcontrol_new dac1_switch[] = {
	SOC_DAPM_SINGLE("Switch", SITAR_A_RX_EAR_EN, 5, 1, 0),
};

static const struct snd_kcontrol_new hphl_switch[] = {
	SOC_DAPM_SINGLE("Switch", SITAR_A_RX_HPH_L_DAC_CTL, 6, 1, 0)
};

static void sitar_codec_enable_adc_block(struct snd_soc_codec *codec,
	int enable)
{
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);

	pr_err("%s %d\n", __func__, enable);

	if (enable) {
		sitar->adc_count++;
		snd_soc_update_bits(codec, SITAR_A_TX_COM_BIAS, 0xE0, 0xE0);

	} else {
		sitar->adc_count--;
		if (!sitar->adc_count) {
			if (!sitar->mbhc_polling_active)
				snd_soc_update_bits(codec, SITAR_A_TX_COM_BIAS,
					0xE0, 0x0);
		}
	}
}

static int sitar_codec_enable_adc(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	u16 adc_reg;

	pr_err("%s %d\n", __func__, event);

	if (w->reg == SITAR_A_TX_1_2_EN)
		adc_reg = SITAR_A_TX_1_2_TEST_CTL;
	else {
		pr_err("%s: Error, invalid adc register\n", __func__);
		return -EINVAL;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		sitar_codec_enable_adc_block(codec, 1);
		break;
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, adc_reg, 1 << w->shift,
			1 << w->shift);
		usleep_range(1000, 1000);
		snd_soc_update_bits(codec, adc_reg, 1 << w->shift, 0x00);
		usleep_range(1000, 1000);
		snd_soc_update_bits(codec, adc_reg, 0x08, 0x08);
		break;
	case SND_SOC_DAPM_POST_PMD:
		sitar_codec_enable_adc_block(codec, 0);
		break;
	}
	return 0;
}

static int sitar_codec_enable_lineout(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	u16 lineout_gain_reg;

	pr_err("%s %d %s\n", __func__, event, w->name);

	switch (w->shift) {
	case 0:
		lineout_gain_reg = SITAR_A_RX_LINE_1_GAIN;
		break;
	case 1:
		lineout_gain_reg = SITAR_A_RX_LINE_2_GAIN;
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
		pr_err("%s: sleeping 16 ms after %s PA turn on\n",
				__func__, w->name);
		usleep_range(16000, 16000);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, lineout_gain_reg, 0x40, 0x00);
		break;
	}
	return 0;
}

static int sitar_codec_enable_dmic(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	u16 tx_mux_ctl_reg, tx_dmic_ctl_reg;
	u8 dmic_clk_sel, dmic_clk_en;
	unsigned int dmic;
	int ret;

	ret = kstrtouint(strpbrk(w->name, "12"), 10, &dmic);
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

		break;

	default:
		pr_err("%s: Invalid DMIC Selection\n", __func__);
		return -EINVAL;
	}

	tx_mux_ctl_reg = SITAR_A_CDC_TX1_MUX_CTL + 8 * (dmic - 1);
	tx_dmic_ctl_reg = SITAR_A_CDC_TX1_DMIC_CTL + 8 * (dmic - 1);

	pr_err("%s %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, tx_mux_ctl_reg, 0x1, 0x1);

		snd_soc_update_bits(codec, SITAR_A_CDC_CLK_DMIC_CTL,
				dmic_clk_sel, dmic_clk_sel);

		snd_soc_update_bits(codec, tx_dmic_ctl_reg, 0x1, 0x1);

		snd_soc_update_bits(codec, SITAR_A_CDC_CLK_DMIC_CTL,
				dmic_clk_en, dmic_clk_en);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, SITAR_A_CDC_CLK_DMIC_CTL,
				dmic_clk_en, 0);
		break;
	}
	return 0;
}


static void sitar_codec_disable_button_presses(struct snd_soc_codec *codec)
{
	snd_soc_write(codec, SITAR_A_CDC_MBHC_VOLT_B4_CTL, 0x80);
	snd_soc_write(codec, SITAR_A_CDC_MBHC_VOLT_B3_CTL, 0x00);
}

static void sitar_codec_start_hs_polling(struct snd_soc_codec *codec)
{
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);

	snd_soc_write(codec, SITAR_A_MBHC_SCALING_MUX_1, 0x84);
	wcd9xxx_enable_irq(codec->control_data, SITAR_IRQ_MBHC_REMOVAL);
	if (!sitar->no_mic_headset_override) {
		wcd9xxx_enable_irq(codec->control_data,
				SITAR_IRQ_MBHC_POTENTIAL);
		wcd9xxx_enable_irq(codec->control_data,
				SITAR_IRQ_MBHC_RELEASE);
	} else {
		sitar_codec_disable_button_presses(codec);
	}
}

static void sitar_codec_pause_hs_polling(struct snd_soc_codec *codec)
{
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);

	snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_CLK_CTL, 0x8, 0x8);
	wcd9xxx_disable_irq(codec->control_data, SITAR_IRQ_MBHC_REMOVAL);
	if (!sitar->no_mic_headset_override) {
		wcd9xxx_disable_irq(codec->control_data,
			SITAR_IRQ_MBHC_POTENTIAL);
		wcd9xxx_disable_irq(codec->control_data,
			SITAR_IRQ_MBHC_RELEASE);
	}
}

static void sitar_codec_switch_cfilt_mode(struct snd_soc_codec *codec,
		int mode)
{
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);
	u8 reg_mode_val, cur_mode_val;
	bool mbhc_was_polling = false;

	if (mode)
		reg_mode_val = SITAR_CFILT_FAST_MODE;
	else
		reg_mode_val = SITAR_CFILT_SLOW_MODE;

	cur_mode_val = snd_soc_read(codec,
			sitar->mbhc_bias_regs.cfilt_ctl) & 0x40;

	if (cur_mode_val != reg_mode_val) {
		if (sitar->mbhc_polling_active) {
			sitar_codec_pause_hs_polling(codec);
			mbhc_was_polling = true;
		}
		snd_soc_update_bits(codec,
			sitar->mbhc_bias_regs.cfilt_ctl, 0x40, reg_mode_val);
		if (mbhc_was_polling)
			sitar_codec_start_hs_polling(codec);
		pr_err("%s: CFILT mode change (%x to %x)\n", __func__,
			cur_mode_val, reg_mode_val);
	} else {
		pr_err("%s: CFILT Value is already %x\n",
			__func__, cur_mode_val);
	}
}

static void sitar_codec_update_cfilt_usage(struct snd_soc_codec *codec,
					  u8 cfilt_sel, int inc)
{
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);
	u32 *cfilt_cnt_ptr = NULL;
	u16 micb_cfilt_reg;

	switch (cfilt_sel) {
	case SITAR_CFILT1_SEL:
		cfilt_cnt_ptr = &sitar->cfilt1_cnt;
		micb_cfilt_reg = SITAR_A_MICB_CFILT_1_CTL;
		break;
	case SITAR_CFILT2_SEL:
		cfilt_cnt_ptr = &sitar->cfilt2_cnt;
		micb_cfilt_reg = SITAR_A_MICB_CFILT_2_CTL;
		break;
	default:
		return; /* should not happen */
	}

	if (inc) {
		if (!(*cfilt_cnt_ptr)++) {
			/* Switch CFILT to slow mode if MBHC CFILT being used */
			if (cfilt_sel == sitar->mbhc_bias_regs.cfilt_sel)
				sitar_codec_switch_cfilt_mode(codec, 0);

			snd_soc_update_bits(codec, micb_cfilt_reg, 0x80, 0x80);
		}
	} else {
		/* check if count not zero, decrement
		* then check if zero, go ahead disable cfilter
		*/
		if ((*cfilt_cnt_ptr) && !--(*cfilt_cnt_ptr)) {
			snd_soc_update_bits(codec, micb_cfilt_reg, 0x80, 0);

			/* Switch CFILT to fast mode if MBHC CFILT being used */
			if (cfilt_sel == sitar->mbhc_bias_regs.cfilt_sel)
				sitar_codec_switch_cfilt_mode(codec, 1);
		}
	}
}

static int sitar_find_k_value(unsigned int ldoh_v, unsigned int cfilt_mv)
{
	int rc = -EINVAL;
	unsigned min_mv, max_mv;

	switch (ldoh_v) {
	case SITAR_LDOH_1P95_V:
		min_mv = 160;
		max_mv = 1800;
		break;
	case SITAR_LDOH_2P35_V:
		min_mv = 200;
		max_mv = 2200;
		break;
	case SITAR_LDOH_2P75_V:
		min_mv = 240;
		max_mv = 2600;
		break;
	case SITAR_LDOH_2P85_V:
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

static bool sitar_is_hph_pa_on(struct snd_soc_codec *codec)
{
	u8 hph_reg_val = 0;
	hph_reg_val = snd_soc_read(codec, SITAR_A_RX_HPH_CNP_EN);

	return (hph_reg_val & 0x30) ? true : false;
}

static bool sitar_is_hph_dac_on(struct snd_soc_codec *codec, int left)
{
	u8 hph_reg_val = 0;
	if (left)
		hph_reg_val = snd_soc_read(codec,
					  SITAR_A_RX_HPH_L_DAC_CTL);
	else
		hph_reg_val = snd_soc_read(codec,
					  SITAR_A_RX_HPH_R_DAC_CTL);

	return (hph_reg_val & 0xC0) ? true : false;
}

static void sitar_codec_switch_micbias(struct snd_soc_codec *codec,
	int vddio_switch)
{
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);
	int cfilt_k_val;
	bool mbhc_was_polling =  false;

	switch (vddio_switch) {
	case 1:
		if (sitar->mbhc_polling_active) {

			sitar_codec_pause_hs_polling(codec);
			/* Enable Mic Bias switch to VDDIO */
			sitar->cfilt_k_value = snd_soc_read(codec,
					sitar->mbhc_bias_regs.cfilt_val);
			cfilt_k_val = sitar_find_k_value(
					sitar->pdata->micbias.ldoh_v, 1800);
			snd_soc_update_bits(codec,
				sitar->mbhc_bias_regs.cfilt_val,
				0xFC, (cfilt_k_val << 2));

			snd_soc_update_bits(codec,
				sitar->mbhc_bias_regs.mbhc_reg,	0x80, 0x80);
			snd_soc_update_bits(codec,
				sitar->mbhc_bias_regs.mbhc_reg,	0x10, 0x00);
			sitar_codec_start_hs_polling(codec);

			sitar->mbhc_micbias_switched = true;
			pr_err("%s: Enabled MBHC Mic bias to VDDIO Switch\n",
				__func__);
		}
		break;

	case 0:
		if (sitar->mbhc_micbias_switched) {
			if (sitar->mbhc_polling_active) {
				sitar_codec_pause_hs_polling(codec);
				mbhc_was_polling = true;
			}
			/* Disable Mic Bias switch to VDDIO */
			if (sitar->cfilt_k_value != 0)
				snd_soc_update_bits(codec,
					sitar->mbhc_bias_regs.cfilt_val, 0XFC,
					sitar->cfilt_k_value);
			snd_soc_update_bits(codec,
				sitar->mbhc_bias_regs.mbhc_reg,	0x80, 0x00);
			snd_soc_update_bits(codec,
				sitar->mbhc_bias_regs.mbhc_reg,	0x10, 0x00);

			if (mbhc_was_polling)
				sitar_codec_start_hs_polling(codec);

			sitar->mbhc_micbias_switched = false;
			pr_err("%s: Disabled MBHC Mic bias to VDDIO Switch\n",
				__func__);
		}
		break;
	}
}

static int sitar_codec_enable_micbias(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);
	u16 micb_int_reg;
	int micb_line;
	u8 cfilt_sel_val = 0;
	char *internal1_text = "Internal1";
	char *internal2_text = "Internal2";

	pr_err("%s %d\n", __func__, event);
	switch (w->reg) {
	case SITAR_A_MICB_1_CTL:
		micb_int_reg = SITAR_A_MICB_1_INT_RBIAS;
		cfilt_sel_val = sitar->pdata->micbias.bias1_cfilt_sel;
		micb_line = SITAR_MICBIAS1;
		break;
	case SITAR_A_MICB_2_CTL:
		micb_int_reg = SITAR_A_MICB_2_INT_RBIAS;
		cfilt_sel_val = sitar->pdata->micbias.bias2_cfilt_sel;
		micb_line = SITAR_MICBIAS2;
		break;
	default:
		pr_err("%s: Error, invalid micbias register\n", __func__);
		return -EINVAL;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Decide whether to switch the micbias for MBHC */
		if ((w->reg == sitar->mbhc_bias_regs.ctl_reg)
				&& sitar->mbhc_micbias_switched)
			sitar_codec_switch_micbias(codec, 0);

		snd_soc_update_bits(codec, w->reg, 0x1E, 0x0A);
		sitar_codec_update_cfilt_usage(codec, cfilt_sel_val, 1);

		if (strnstr(w->name, internal1_text, 30))
			snd_soc_update_bits(codec, micb_int_reg, 0xFF, 0xA4);
		else if (strnstr(w->name, internal2_text, 30))
			snd_soc_update_bits(codec, micb_int_reg, 0x1C, 0x1C);
		break;
	case SND_SOC_DAPM_POST_PMU:
		if (sitar->mbhc_polling_active &&
			(sitar->calibration->bias == micb_line)) {
			sitar_codec_pause_hs_polling(codec);
			sitar_codec_start_hs_polling(codec);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:

		if ((w->reg == sitar->mbhc_bias_regs.ctl_reg)
				&& sitar_is_hph_pa_on(codec))
			sitar_codec_switch_micbias(codec, 1);

		if (strnstr(w->name, internal1_text, 30))
			snd_soc_update_bits(codec, micb_int_reg, 0x80, 0x00);
		else if (strnstr(w->name, internal2_text, 30))
			snd_soc_update_bits(codec, micb_int_reg, 0x10, 0x00);
		sitar_codec_update_cfilt_usage(codec, cfilt_sel_val, 0);
		break;
	}

	return 0;
}

static int sitar_codec_enable_dec(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	u16 dec_reset_reg;

	pr_err("%s %d\n", __func__, event);

	if (w->reg == SITAR_A_CDC_CLK_TX_CLK_EN_B1_CTL)
		dec_reset_reg = SITAR_A_CDC_CLK_TX_RESET_B1_CTL;
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

static int sitar_codec_reset_interpolator(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	pr_err("%s %d %s\n", __func__, event, w->name);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, SITAR_A_CDC_CLK_RX_RESET_CTL,
			1 << w->shift, 1 << w->shift);
		snd_soc_update_bits(codec, SITAR_A_CDC_CLK_RX_RESET_CTL,
			1 << w->shift, 0x0);
		break;
	}
	return 0;
}

static int sitar_codec_enable_ldo_h(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
	case SND_SOC_DAPM_POST_PMD:
		usleep_range(1000, 1000);
		pr_debug("LDO_H\n");
		break;
	}
	return 0;
}

static void sitar_enable_rx_bias(struct snd_soc_codec *codec, u32  enable)
{
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);

	if (enable) {
		sitar->rx_bias_count++;
		if (sitar->rx_bias_count == 1)
			snd_soc_update_bits(codec, SITAR_A_RX_COM_BIAS,
				0x80, 0x80);
	} else {
		sitar->rx_bias_count--;
		if (!sitar->rx_bias_count)
			snd_soc_update_bits(codec, SITAR_A_RX_COM_BIAS,
				0x80, 0x00);
	}
}

static int sitar_codec_enable_rx_bias(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	pr_err("%s %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		sitar_enable_rx_bias(codec, 1);
		break;
	case SND_SOC_DAPM_POST_PMD:
		sitar_enable_rx_bias(codec, 0);
		break;
	}
	return 0;
}
static int sitar_hphr_dac_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	pr_err("%s %s %d\n", __func__, w->name, event);

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

static void sitar_snd_soc_jack_report(struct sitar_priv *sitar,
				     struct snd_soc_jack *jack, int status,
				     int mask)
{
	/* XXX: wake_lock_timeout()? */
	snd_soc_jack_report(jack, status, mask);
}

static void hphocp_off_report(struct sitar_priv *sitar,
	u32 jack_status, int irq)
{
	struct snd_soc_codec *codec;

	if (sitar) {
		pr_info("%s: clear ocp status %x\n", __func__, jack_status);
		codec = sitar->codec;
		sitar->hph_status &= ~jack_status;
		if (sitar->headset_jack)
			sitar_snd_soc_jack_report(sitar, sitar->headset_jack,
						 sitar->hph_status,
						 SITAR_JACK_MASK);
		snd_soc_update_bits(codec, SITAR_A_RX_HPH_OCP_CTL, 0x10,
		0x00);
		snd_soc_update_bits(codec, SITAR_A_RX_HPH_OCP_CTL, 0x10,
		0x10);
		/* reset retry counter as PA is turned off signifying
		* start of new OCP detection session
		*/
		if (SITAR_IRQ_HPH_PA_OCPL_FAULT)
			sitar->hphlocp_cnt = 0;
		else
			sitar->hphrocp_cnt = 0;
		wcd9xxx_enable_irq(codec->control_data, irq);
	} else {
		pr_err("%s: Bad sitar private data\n", __func__);
	}
}

static void hphlocp_off_report(struct work_struct *work)
{
	struct sitar_priv *sitar = container_of(work, struct sitar_priv,
		hphlocp_work);
	hphocp_off_report(sitar, SND_JACK_OC_HPHL, SITAR_IRQ_HPH_PA_OCPL_FAULT);
}

static void hphrocp_off_report(struct work_struct *work)
{
	struct sitar_priv *sitar = container_of(work, struct sitar_priv,
		hphrocp_work);
	hphocp_off_report(sitar, SND_JACK_OC_HPHR, SITAR_IRQ_HPH_PA_OCPR_FAULT);
}

static int sitar_hph_pa_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);
	u8 mbhc_micb_ctl_val;
	pr_err("%s: event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		mbhc_micb_ctl_val = snd_soc_read(codec,
				sitar->mbhc_bias_regs.ctl_reg);

		if (!(mbhc_micb_ctl_val & 0x80)
				&& !sitar->mbhc_micbias_switched)
			sitar_codec_switch_micbias(codec, 1);

		break;

	case SND_SOC_DAPM_POST_PMD:
		/* schedule work is required because at the time HPH PA DAPM
		* event callback is called by DAPM framework, CODEC dapm mutex
		* would have been locked while snd_soc_jack_report also
		* attempts to acquire same lock.
		*/
		if (w->shift == 5) {
			clear_bit(SITAR_HPHL_PA_OFF_ACK,
				 &sitar->hph_pa_dac_state);
			clear_bit(SITAR_HPHL_DAC_OFF_ACK,
				 &sitar->hph_pa_dac_state);
			if (sitar->hph_status & SND_JACK_OC_HPHL)
				schedule_work(&sitar->hphlocp_work);
		} else if (w->shift == 4) {
			clear_bit(SITAR_HPHR_PA_OFF_ACK,
				 &sitar->hph_pa_dac_state);
			clear_bit(SITAR_HPHR_DAC_OFF_ACK,
				 &sitar->hph_pa_dac_state);
			if (sitar->hph_status & SND_JACK_OC_HPHR)
				schedule_work(&sitar->hphrocp_work);
		}

		if (sitar->mbhc_micbias_switched)
			sitar_codec_switch_micbias(codec, 0);

		pr_err("%s: sleep 10 ms after %s PA disable.\n", __func__,
				w->name);
		usleep_range(10000, 10000);

		break;
	}
	return 0;
}

static void sitar_get_mbhc_micbias_regs(struct snd_soc_codec *codec,
		struct mbhc_micbias_regs *micbias_regs)
{
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);
	struct sitar_mbhc_calibration *calibration = sitar->calibration;
	unsigned int cfilt;

	switch (calibration->bias) {
	case SITAR_MICBIAS1:
		cfilt = sitar->pdata->micbias.bias1_cfilt_sel;
		micbias_regs->mbhc_reg = SITAR_A_MICB_1_MBHC;
		micbias_regs->int_rbias = SITAR_A_MICB_1_INT_RBIAS;
		micbias_regs->ctl_reg = SITAR_A_MICB_1_CTL;
		break;
	case SITAR_MICBIAS2:
		cfilt = sitar->pdata->micbias.bias2_cfilt_sel;
		micbias_regs->mbhc_reg = SITAR_A_MICB_2_MBHC;
		micbias_regs->int_rbias = SITAR_A_MICB_2_INT_RBIAS;
		micbias_regs->ctl_reg = SITAR_A_MICB_2_CTL;
		break;
	default:
		/* Should never reach here */
		pr_err("%s: Invalid MIC BIAS for MBHC\n", __func__);
		return;
	}

	micbias_regs->cfilt_sel = cfilt;

	switch (cfilt) {
	case SITAR_CFILT1_SEL:
		micbias_regs->cfilt_val = SITAR_A_MICB_CFILT_1_VAL;
		micbias_regs->cfilt_ctl = SITAR_A_MICB_CFILT_1_CTL;
		break;
	case SITAR_CFILT2_SEL:
		micbias_regs->cfilt_val = SITAR_A_MICB_CFILT_2_VAL;
		micbias_regs->cfilt_ctl = SITAR_A_MICB_CFILT_2_CTL;
		break;
	}
}

static int sitar_codec_enable_charge_pump(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	pr_err("%s %d\n", __func__, event);
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, SITAR_A_CDC_CLK_OTHR_CTL, 0x01,
			0x01);
		snd_soc_update_bits(codec, SITAR_A_CDC_CLSG_CTL, 0x08, 0x08);
		usleep_range(200, 200);
		snd_soc_update_bits(codec, SITAR_A_CP_STATIC, 0x10, 0x00);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, SITAR_A_CDC_CLK_OTHR_RESET_CTL, 0x10,
			0x10);
		usleep_range(20, 20);
		snd_soc_update_bits(codec, SITAR_A_CP_STATIC, 0x08, 0x08);
		snd_soc_update_bits(codec, SITAR_A_CP_STATIC, 0x10, 0x10);
		snd_soc_update_bits(codec, SITAR_A_CDC_CLSG_CTL, 0x08, 0x00);
		snd_soc_update_bits(codec, SITAR_A_CDC_CLK_OTHR_CTL, 0x01,
			0x00);
		snd_soc_update_bits(codec, SITAR_A_CP_STATIC, 0x08, 0x00);
		break;
	}
	return 0;
}

static const struct snd_soc_dapm_widget sitar_dapm_i2s_widgets[] = {
	SND_SOC_DAPM_SUPPLY("RX_I2S_CLK", SITAR_A_CDC_CLK_RX_I2S_CTL,
	4, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("TX_I2S_CLK", SITAR_A_CDC_CLK_TX_I2S_CTL, 4,
	0, NULL, 0),
};

static const struct snd_soc_dapm_widget sitar_dapm_widgets[] = {
	/*RX stuff */
	SND_SOC_DAPM_OUTPUT("EAR"),

	SND_SOC_DAPM_PGA("EAR PA", SITAR_A_RX_EAR_EN, 4, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("DAC1", SITAR_A_RX_EAR_EN, 6, 0, dac1_switch,
		ARRAY_SIZE(dac1_switch)),
	SND_SOC_DAPM_SUPPLY("EAR DRIVER", SITAR_A_RX_EAR_EN, 3, 0, NULL, 0),
	SND_SOC_DAPM_AIF_IN_E("SLIM RX1", "AIF1 Playback", 0, SND_SOC_NOPM, 0,
				0, sitar_codec_enable_slimrx,
				SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_IN_E("SLIM RX2", "AIF1 Playback", 0, SND_SOC_NOPM, 0,
				0, sitar_codec_enable_slimrx,
				SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_IN("SLIM RX3", "AIF1 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("SLIM RX4", "AIF1 Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("SLIM RX5", "AIF1 Playback", 0, SND_SOC_NOPM, 0, 0),

	/* Headphone */
	SND_SOC_DAPM_OUTPUT("HEADPHONE"),
	SND_SOC_DAPM_PGA_E("HPHL", SITAR_A_RX_HPH_CNP_EN, 5, 0, NULL, 0,
		sitar_hph_pa_event, SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER("HPHL DAC", SITAR_A_RX_HPH_L_DAC_CTL, 7, 0,
		hphl_switch, ARRAY_SIZE(hphl_switch)),

	SND_SOC_DAPM_PGA_E("HPHR", SITAR_A_RX_HPH_CNP_EN, 4, 0, NULL, 0,
		sitar_hph_pa_event, SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_DAC_E("HPHR DAC", NULL, SITAR_A_RX_HPH_R_DAC_CTL, 7, 0,
		sitar_hphr_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	/* Speaker */
	SND_SOC_DAPM_OUTPUT("LINEOUT1"),
	SND_SOC_DAPM_OUTPUT("LINEOUT2"),

	SND_SOC_DAPM_PGA_E("LINEOUT1 PA", SITAR_A_RX_LINE_CNP_EN, 0, 0, NULL,
			0, sitar_codec_enable_lineout, SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("LINEOUT2 PA", SITAR_A_RX_LINE_CNP_EN, 1, 0, NULL,
			0, sitar_codec_enable_lineout, SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MIXER_E("RX1 MIX1", SITAR_A_CDC_CLK_RX_B1_CTL, 0, 0, NULL,
		0, sitar_codec_reset_interpolator, SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_MIXER_E("RX2 MIX1", SITAR_A_CDC_CLK_RX_B1_CTL, 1, 0, NULL,
		0, sitar_codec_reset_interpolator, SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_MIXER_E("RX3 MIX1", SITAR_A_CDC_CLK_RX_B1_CTL, 2, 0, NULL,
		0, sitar_codec_reset_interpolator, SND_SOC_DAPM_PRE_PMU),

	SND_SOC_DAPM_MUX("DAC1 MUX", SND_SOC_NOPM, 0, 0,
		&rx_dac1_mux),
	SND_SOC_DAPM_MUX("DAC2 MUX", SND_SOC_NOPM, 0, 0,
		&rx_dac2_mux),
	SND_SOC_DAPM_MUX("DAC3 MUX", SND_SOC_NOPM, 0, 0,
		&rx_dac3_mux),
	SND_SOC_DAPM_MUX("DAC4 MUX", SND_SOC_NOPM, 0, 0,
		&rx_dac4_mux),

	SND_SOC_DAPM_MIXER("RX1 CHAIN", SITAR_A_CDC_RX1_B6_CTL, 5, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX2 CHAIN", SITAR_A_CDC_RX2_B6_CTL, 5, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX3 CHAIN", SITAR_A_CDC_RX3_B6_CTL, 5, 0, NULL, 0),

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

	SND_SOC_DAPM_SUPPLY("CP", SITAR_A_CP_EN, 0, 0,
		sitar_codec_enable_charge_pump, SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_SUPPLY("RX_BIAS", SND_SOC_NOPM, 0, 0,
		sitar_codec_enable_rx_bias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY("LDO_H", SITAR_A_LDO_H_MODE_1, 7, 0,
		sitar_codec_enable_ldo_h, SND_SOC_DAPM_POST_PMU),
	/* TX */
	SND_SOC_DAPM_SUPPLY("CDC_CONN", SITAR_A_CDC_CLK_OTHR_CTL, 2, 0, NULL,
		0),
	SND_SOC_DAPM_INPUT("AMIC1"),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS1 External", SITAR_A_MICB_1_CTL, 7, 0,
		sitar_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS1 Internal1", SITAR_A_MICB_1_CTL, 7, 0,
		sitar_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_INPUT("AMIC2"),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS2 External", SITAR_A_MICB_2_CTL, 7, 0,
		sitar_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS2 Internal1", SITAR_A_MICB_2_CTL, 7, 0,
		sitar_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS2 Internal2", SITAR_A_MICB_2_CTL, 7, 0,
		sitar_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("ADC1", NULL, SITAR_A_TX_1_2_EN, 7, 0,
		sitar_codec_enable_adc, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("ADC2", NULL, SITAR_A_TX_1_2_EN, 3, 0,
		sitar_codec_enable_adc, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("DEC1 MUX", SITAR_A_CDC_CLK_TX_CLK_EN_B1_CTL, 0, 0,
		&dec1_mux, sitar_codec_enable_dec, SND_SOC_DAPM_PRE_PMU),

	SND_SOC_DAPM_MUX("SLIM TX1 MUX", SND_SOC_NOPM, 0, 0, &sb_tx1_mux),
	SND_SOC_DAPM_MUX("SLIM TX2 MUX", SND_SOC_NOPM, 0, 0, &sb_tx2_mux),
	SND_SOC_DAPM_MUX("SLIM TX3 MUX", SND_SOC_NOPM, 0, 0, &sb_tx3_mux),

	SND_SOC_DAPM_AIF_OUT_E("SLIM TX1", "AIF1 Capture", 0, SND_SOC_NOPM, 0,
				0, sitar_codec_enable_slimtx,
				SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_AIF_OUT_E("SLIM TX2", "AIF1 Capture", 0, SND_SOC_NOPM, 0,
				0, sitar_codec_enable_slimtx,
				SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_AIF_OUT_E("SLIM TX3", "AIF1 Capture", 0, SND_SOC_NOPM, 0,
				0, sitar_codec_enable_slimtx,
				SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("DEC2 MUX", SITAR_A_CDC_CLK_TX_CLK_EN_B1_CTL, 1, 0,
		&dec2_mux, sitar_codec_enable_dec, SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_MUX_E("DEC3 MUX", SITAR_A_CDC_CLK_TX_CLK_EN_B1_CTL, 2, 0,
		&dec3_mux, sitar_codec_enable_dec, SND_SOC_DAPM_PRE_PMU),
	SND_SOC_DAPM_MUX_E("DEC4 MUX", SITAR_A_CDC_CLK_TX_CLK_EN_B1_CTL, 3, 0,
		&dec4_mux, sitar_codec_enable_dec, SND_SOC_DAPM_PRE_PMU),

	/* Digital Mic Inputs */
	SND_SOC_DAPM_ADC_E("DMIC1", NULL, SND_SOC_NOPM, 0, 0,
		sitar_codec_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("DMIC2", NULL, SND_SOC_NOPM, 0, 0,
		sitar_codec_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("DMIC3", NULL, SND_SOC_NOPM, 0, 0,
		sitar_codec_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("DMIC4", NULL, SND_SOC_NOPM, 0, 0,
		sitar_codec_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	/* Sidetone */
	SND_SOC_DAPM_MUX("IIR1 INP1 MUX", SND_SOC_NOPM, 0, 0, &iir1_inp1_mux),
	SND_SOC_DAPM_PGA("IIR1", SITAR_A_CDC_CLK_SD_CTL, 0, 0, NULL, 0),

};

static const struct snd_soc_dapm_route audio_map[] = {
	/* Earpiece (RX MIX1) */
	{"EAR", NULL, "EAR PA"},
	{"EAR PA", "NULL", "DAC1"},
	{"DAC1", "Switch", "DAC1 MUX"},
	{"DAC1", NULL, "CP"},
	{"DAC1", NULL, "EAR DRIVER"},

	{"LINEOUT1", NULL, "CP"},
	{"LINEOUT2", NULL, "CP"},

	{"LINEOUT2", NULL, "LINEOUT2 PA"},
	{"LINEOUT2 PA", "NULL", "DAC3 MUX"},

	{"LINEOUT1", NULL, "LINEOUT1 PA"},
	{"LINEOUT1 PA", "NULL", "DAC2 MUX"},

	/* Headset (RX MIX1 and RX MIX2) */
	{"HEADPHONE", NULL, "HPHL"},
	{"HEADPHONE", NULL, "HPHR"},

	{"HPHL", NULL, "HPHL DAC"},
	{"HPHL DAC", NULL, "DAC4 MUX"},
	{"HPHR", NULL, "HPHR DAC"},
	{"HPHL DAC", NULL, "RX3 MIX1"},

	{"DAC1 MUX", "RX1", "RX1 CHAIN"},
	{"DAC2 MUX", "RX1", "RX1 CHAIN"},
	{"DAC3 MUX", "RX1", "RX1 CHAIN"},
	{"DAC3 MUX", "RX1_INV", "RX1 CHAIN"},
	{"DAC3 MUX", "RX2", "RX2 MIX1"},
	{"DAC4 MUX", "ON", "RX2 MIX1"},

	{"RX1 CHAIN", NULL, "RX1 MIX1"},

	{"CP", NULL, "RX_BIAS"},

	{"RX1 MIX1", NULL, "RX1 MIX1 INP1"},
	{"RX1 MIX1", NULL, "RX1 MIX1 INP2"},
	{"RX2 MIX1", NULL, "RX2 MIX1 INP1"},
	{"RX2 MIX1", NULL, "RX2 MIX1 INP2"},
	{"RX3 MIX1", NULL, "RX3 MIX1 INP1"},
	{"RX3 MIX1", NULL, "RX3 MIX1 INP2"},

	/* SLIMBUS Connections */

	/* Slimbus port 5 is non functional in Sitar 1.0 */
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


	/* TX */
	{"SLIM TX1", NULL, "SLIM TX1 MUX"},
	{"MIC BIAS2 Internal1", NULL, "DEC1 MUX"},

	{"SLIM TX2", NULL, "SLIM TX2 MUX"},
	{"MIC BIAS2 Internal1", NULL, "DEC1 MUX"},

	{"SLIM TX1", NULL, "SLIM TX1 MUX"},
	{"SLIM TX2", NULL, "SLIM TX2 MUX"},
	{"SLIM TX3", NULL, "SLIM TX3 MUX"},

	{"SLIM TX1 MUX", "DEC1", "DEC1 MUX"},
	{"SLIM TX2 MUX", "DEC2", "DEC2 MUX"},
	{"SLIM TX3 MUX", "DEC3", "DEC3 MUX"},

	/* Decimator Inputs */
	{"DEC1 MUX", "DMIC1", "DMIC1"},
	{"DEC1 MUX", "DMIC4", "DMIC4"},
	{"DEC1 MUX", "ADC1", "ADC1"},
	{"DEC1 MUX", "ADC2", "ADC2"},

	{"DEC2 MUX", "DMIC2", "DMIC2"},
	{"DEC2 MUX", "DMIC3", "DMIC3"},
	{"DEC2 MUX", "ADC1", "ADC1"},
	{"DEC2 MUX", "ADC2", "ADC2"},

	{"DEC3 MUX", "DMIC3", "DMIC3"},
	{"DEC3 MUX", "ADC1", "ADC1"},
	{"DEC3 MUX", "ADC2", "ADC2"},
	{"DEC3 MUX", "DMIC2", "DMIC2"},
	{"DEC3 MUX", "DMIC3", "DMIC4"},

	{"DEC4 MUX", "DMIC4", "DMIC4"},
	{"DEC4 MUX", "ADC1", "ADC1"},
	{"DEC4 MUX", "ADC2", "ADC2"},
	{"DEC4 MUX", "DMIC3", "DMIC3"},
	{"DEC4 MUX", "DMIC2", "DMIC2"},
	{"DEC4 MUX", "DMIC1", "DMIC1"},

	/* ADC Connections */
	{"ADC1", NULL, "AMIC1"},
	{"ADC2", NULL, "AMIC2"},

	/* IIR */
	{"IIR1", NULL, "IIR1 INP1 MUX"},
	{"IIR1 INP1 MUX", "DEC1", "DEC1 MUX"},
	{"MIC BIAS1 Internal1", NULL, "LDO_H"},
	{"MIC BIAS1 External", NULL, "LDO_H"},
	{"MIC BIAS2 Internal1", NULL, "LDO_H"},
	{"MIC BIAS2 External", NULL, "LDO_H"},
};

static int sitar_readable(struct snd_soc_codec *ssc, unsigned int reg)
{
	return sitar_reg_readable[reg];
}

static int sitar_volatile(struct snd_soc_codec *ssc, unsigned int reg)
{
	/* Registers lower than 0x100 are top level registers which can be
	* written by the Sitar core driver.
	*/

	if ((reg >= SITAR_A_CDC_MBHC_EN_CTL) || (reg < 0x100))
		return 1;

	/* IIR Coeff registers are not cacheable */
	if ((reg >= SITAR_A_CDC_IIR1_COEF_B1_CTL) &&
		(reg <= SITAR_A_CDC_IIR1_COEF_B5_CTL))
		return 1;

	return 0;
}

#define SITAR_FORMATS (SNDRV_PCM_FMTBIT_S16_LE)
static int sitar_write(struct snd_soc_codec *codec, unsigned int reg,
	unsigned int value)
{
	int ret;

	BUG_ON(reg > SITAR_MAX_REGISTER);

	if (!sitar_volatile(codec, reg)) {
		ret = snd_soc_cache_write(codec, reg, value);
		if (ret != 0)
			dev_err(codec->dev, "Cache write to %x failed: %d\n",
				reg, ret);
	}

	return wcd9xxx_reg_write(codec->control_data, reg, value);
}
static unsigned int sitar_read(struct snd_soc_codec *codec,
				unsigned int reg)
{
	unsigned int val;
	int ret;

	BUG_ON(reg > SITAR_MAX_REGISTER);

	if (!sitar_volatile(codec, reg) && sitar_readable(codec, reg) &&
		reg < codec->driver->reg_cache_size) {
		ret = snd_soc_cache_read(codec, reg, &val);
		if (ret >= 0) {
			return val;
		} else
			dev_err(codec->dev, "Cache read from %x failed: %d\n",
				reg, ret);
	}

	val = wcd9xxx_reg_read(codec->control_data, reg);
	return val;
}

static void sitar_codec_enable_audio_mode_bandgap(struct snd_soc_codec *codec)
{

	snd_soc_update_bits(codec, SITAR_A_LDO_H_MODE_1, 0x0C, 0x61);
	snd_soc_update_bits(codec, SITAR_A_LDO_H_MODE_1, 0x80, 0x80);
	usleep_range(1000, 1000);
	snd_soc_write(codec, SITAR_A_BIAS_REF_CTL, 0x1C);
	snd_soc_update_bits(codec, SITAR_A_BIAS_CENTRAL_BG_CTL, 0x80,
		0x80);
	snd_soc_update_bits(codec, SITAR_A_BIAS_CENTRAL_BG_CTL, 0x04,
		0x04);
	snd_soc_update_bits(codec, SITAR_A_BIAS_CENTRAL_BG_CTL, 0x01,
		0x01);
	usleep_range(1000, 1000);
	snd_soc_update_bits(codec, SITAR_A_BIAS_CENTRAL_BG_CTL, 0x80,
		0x00);
}

static void sitar_codec_enable_bandgap(struct snd_soc_codec *codec,
	enum sitar_bandgap_type choice)
{
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);

	/* TODO lock resources accessed by audio streams and threaded
	* interrupt handlers
	*/

	pr_err("%s, choice is %d, current is %d\n", __func__, choice,
		sitar->bandgap_type);

	if (sitar->bandgap_type == choice)
		return;

	if ((sitar->bandgap_type == SITAR_BANDGAP_OFF) &&
		(choice == SITAR_BANDGAP_AUDIO_MODE)) {
		sitar_codec_enable_audio_mode_bandgap(codec);
	} else if (choice == SITAR_BANDGAP_MBHC_MODE) {
		snd_soc_update_bits(codec, SITAR_A_BIAS_CENTRAL_BG_CTL, 0x2,
			0x2);
		snd_soc_update_bits(codec, SITAR_A_BIAS_CENTRAL_BG_CTL, 0x80,
			0x80);
		snd_soc_update_bits(codec, SITAR_A_BIAS_CENTRAL_BG_CTL, 0x4,
			0x4);
		snd_soc_update_bits(codec, SITAR_A_BIAS_CENTRAL_BG_CTL, 0x01,
			0x1);
		usleep_range(1000, 1000);
		snd_soc_update_bits(codec, SITAR_A_BIAS_CENTRAL_BG_CTL, 0x80,
			0x00);
	} else if ((sitar->bandgap_type == SITAR_BANDGAP_MBHC_MODE) &&
		(choice == SITAR_BANDGAP_AUDIO_MODE)) {
		snd_soc_write(codec, SITAR_A_BIAS_CENTRAL_BG_CTL, 0x50);
		usleep_range(100, 100);
		sitar_codec_enable_audio_mode_bandgap(codec);
	} else if (choice == SITAR_BANDGAP_OFF) {
		snd_soc_write(codec, SITAR_A_BIAS_CENTRAL_BG_CTL, 0x50);
	} else {
		pr_err("%s: Error, Invalid bandgap settings\n", __func__);
	}
	sitar->bandgap_type = choice;
}


static int sitar_codec_enable_clock_block(struct snd_soc_codec *codec,
	int config_mode)
{
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);

	pr_err("%s\n", __func__);

	if (config_mode) {
		snd_soc_write(codec, SITAR_A_CLK_BUFF_EN2, 0x00);
		snd_soc_write(codec, SITAR_A_CLK_BUFF_EN2, 0x02);
		snd_soc_write(codec, SITAR_A_CLK_BUFF_EN1, 0x0D);
		usleep_range(1000, 1000);
	} else
		snd_soc_update_bits(codec, SITAR_A_CLK_BUFF_EN1, 0x08, 0x00);


	snd_soc_update_bits(codec, SITAR_A_CLK_BUFF_EN1, 0x05, 0x05);
	snd_soc_update_bits(codec, SITAR_A_CLK_BUFF_EN2, 0x02, 0x00);
	snd_soc_update_bits(codec, SITAR_A_CLK_BUFF_EN2, 0x04, 0x04);
	snd_soc_update_bits(codec, SITAR_A_CDC_CLK_MCLK_CTL, 0x01, 0x01);
	snd_soc_update_bits(codec, SITAR_A_CDC_CLK_OTHR_CTL, 0x04, 0x04);
	usleep_range(50, 50);
	sitar->clock_active = true;
	return 0;
}
static void sitar_codec_disable_clock_block(struct snd_soc_codec *codec)
{
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);
	pr_err("%s\n", __func__);
	snd_soc_update_bits(codec, SITAR_A_CDC_CLK_OTHR_CTL, 0x04, 0x04);
	snd_soc_update_bits(codec, SITAR_A_CLK_BUFF_EN2, 0x04, 0x00);
	ndelay(160);
	snd_soc_update_bits(codec, SITAR_A_CLK_BUFF_EN2, 0x02, 0x02);
	snd_soc_update_bits(codec, SITAR_A_CLK_BUFF_EN1, 0x05, 0x00);
	sitar->clock_active = false;
}

static void sitar_codec_calibrate_hs_polling(struct snd_soc_codec *codec)
{
	/* TODO store register values in calibration */
	snd_soc_write(codec, SITAR_A_CDC_MBHC_VOLT_B5_CTL, 0x20);
	snd_soc_write(codec, SITAR_A_CDC_MBHC_VOLT_B6_CTL, 0xFF);

	snd_soc_write(codec, SITAR_A_CDC_MBHC_VOLT_B10_CTL, 0xFF);
	snd_soc_write(codec, SITAR_A_CDC_MBHC_VOLT_B9_CTL, 0x20);

	snd_soc_write(codec, SITAR_A_CDC_MBHC_VOLT_B4_CTL, 0xF8);
	snd_soc_write(codec, SITAR_A_CDC_MBHC_VOLT_B3_CTL, 0xEE);
	snd_soc_write(codec, SITAR_A_CDC_MBHC_VOLT_B2_CTL, 0xFC);
	snd_soc_write(codec, SITAR_A_CDC_MBHC_VOLT_B1_CTL, 0xCE);

	snd_soc_write(codec, SITAR_A_CDC_MBHC_TIMER_B1_CTL, 3);
	snd_soc_write(codec, SITAR_A_CDC_MBHC_TIMER_B2_CTL, 9);
	snd_soc_write(codec, SITAR_A_CDC_MBHC_TIMER_B3_CTL, 30);
	snd_soc_write(codec, SITAR_A_CDC_MBHC_TIMER_B6_CTL, 120);
	snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_TIMER_B1_CTL, 0x78, 0x58);
	snd_soc_write(codec, SITAR_A_CDC_MBHC_B2_CTL, 11);
}

static int sitar_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct wcd9xxx *wcd9xxx = dev_get_drvdata(dai->codec->dev->parent);
	if ((wcd9xxx != NULL) && (wcd9xxx->dev != NULL) &&
			(wcd9xxx->dev->parent != NULL))
		pm_runtime_get_sync(wcd9xxx->dev->parent);
	pr_err("%s(): substream = %s  stream = %d\n" , __func__,
		substream->name, substream->stream);

	return 0;
}

static void sitar_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct wcd9xxx *wcd9xxx = dev_get_drvdata(dai->codec->dev->parent);
	if ((wcd9xxx != NULL) && (wcd9xxx->dev != NULL) &&
			(wcd9xxx->dev->parent != NULL)) {
		pm_runtime_mark_last_busy(wcd9xxx->dev->parent);
		pm_runtime_put(wcd9xxx->dev->parent);
	}
	pr_err("%s(): substream = %s  stream = %d\n" , __func__,
		substream->name, substream->stream);
}

int sitar_mclk_enable(struct snd_soc_codec *codec, int mclk_enable)
{
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);

	pr_err("%s() mclk_enable = %u\n", __func__, mclk_enable);

	if (mclk_enable) {
		sitar->mclk_enabled = true;

		if (sitar->mbhc_polling_active && (sitar->mclk_enabled)) {
			sitar_codec_pause_hs_polling(codec);
			sitar_codec_enable_bandgap(codec,
					SITAR_BANDGAP_AUDIO_MODE);
			sitar_codec_enable_clock_block(codec, 0);
			sitar_codec_calibrate_hs_polling(codec);
			sitar_codec_start_hs_polling(codec);
		} else {
			sitar_codec_enable_bandgap(codec,
					SITAR_BANDGAP_AUDIO_MODE);
			sitar_codec_enable_clock_block(codec, 0);
		}
	} else {

		if (!sitar->mclk_enabled) {
			pr_err("Error, MCLK already diabled\n");
			return -EINVAL;
		}
		sitar->mclk_enabled = false;

		if (sitar->mbhc_polling_active) {
			if (!sitar->mclk_enabled) {
				sitar_codec_pause_hs_polling(codec);
				sitar_codec_enable_bandgap(codec,
					SITAR_BANDGAP_MBHC_MODE);
				sitar_enable_rx_bias(codec, 1);
				sitar_codec_enable_clock_block(codec, 1);
				sitar_codec_calibrate_hs_polling(codec);
				sitar_codec_start_hs_polling(codec);
			}
			snd_soc_update_bits(codec, SITAR_A_CLK_BUFF_EN1,
					0x05, 0x01);
		} else {
			sitar_codec_disable_clock_block(codec);
			sitar_codec_enable_bandgap(codec,
				SITAR_BANDGAP_OFF);
		}
	}
	return 0;
}

static int sitar_set_dai_sysclk(struct snd_soc_dai *dai,
		int clk_id, unsigned int freq, int dir)
{
	pr_err("%s\n", __func__);
	return 0;
}

static int sitar_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	u8 val = 0;
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(dai->codec);

	pr_err("%s\n", __func__);
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		/* CPU is master */
		if (sitar->intf_type == WCD9XXX_INTERFACE_TYPE_I2C) {
			if (dai->id == AIF1_CAP)
				snd_soc_update_bits(dai->codec,
					SITAR_A_CDC_CLK_TX_I2S_CTL,
					SITAR_I2S_MASTER_MODE_MASK, 0);
			else if (dai->id == AIF1_PB)
				snd_soc_update_bits(dai->codec,
					SITAR_A_CDC_CLK_RX_I2S_CTL,
					SITAR_I2S_MASTER_MODE_MASK, 0);
		}
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
	/* CPU is slave */
		if (sitar->intf_type == WCD9XXX_INTERFACE_TYPE_I2C) {
			val = SITAR_I2S_MASTER_MODE_MASK;
			if (dai->id == AIF1_CAP)
				snd_soc_update_bits(dai->codec,
					SITAR_A_CDC_CLK_TX_I2S_CTL, val, val);
			else if (dai->id == AIF1_PB)
				snd_soc_update_bits(dai->codec,
					SITAR_A_CDC_CLK_RX_I2S_CTL, val, val);
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}
static int sitar_set_channel_map(struct snd_soc_dai *dai,
			unsigned int tx_num, unsigned int *tx_slot,
			unsigned int rx_num, unsigned int *rx_slot)

{
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(dai->codec);
	u32 i = 0;
	if (!tx_slot && !rx_slot) {
		pr_err("%s: Invalid\n", __func__);
		return -EINVAL;
	}
	pr_debug("%s: DAI-ID %x %d %d\n", __func__, dai->id, tx_num, rx_num);

	if (dai->id == AIF1_PB) {
		for (i = 0; i < rx_num; i++) {
			sitar->dai[dai->id - 1].ch_num[i]  = rx_slot[i];
			sitar->dai[dai->id - 1].ch_act = 0;
			sitar->dai[dai->id - 1].ch_tot = rx_num;
		}
	} else if (dai->id == AIF1_CAP) {
		for (i = 0; i < tx_num; i++) {
			sitar->dai[dai->id - 1].ch_num[i]  = tx_slot[i];
			sitar->dai[dai->id - 1].ch_act = 0;
			sitar->dai[dai->id - 1].ch_tot = tx_num;
		}
	}
	return 0;
}

static int sitar_get_channel_map(struct snd_soc_dai *dai,
			unsigned int *tx_num, unsigned int *tx_slot,
			unsigned int *rx_num, unsigned int *rx_slot)

{
	struct wcd9xxx *sitar = dev_get_drvdata(dai->codec->control_data);

	u32 cnt = 0;
	u32 tx_ch[SLIM_MAX_TX_PORTS];
	u32 rx_ch[SLIM_MAX_RX_PORTS];

	if (!rx_slot && !tx_slot) {
		pr_err("%s: Invalid\n", __func__);
		return -EINVAL;
	}
	pr_debug("%s: DAI-ID %x\n", __func__, dai->id);
	/* for virtual port, codec driver needs to do
	* housekeeping, for now should be ok
	*/
	wcd9xxx_get_channel(sitar, rx_ch, tx_ch);
	if (dai->id == AIF1_PB) {
		*rx_num = sitar_dai[dai->id - 1].playback.channels_max;
		while (cnt < *rx_num) {
			rx_slot[cnt] = rx_ch[cnt];
			cnt++;
		}
	} else if (dai->id == AIF1_CAP) {
		*tx_num = sitar_dai[dai->id - 1].capture.channels_max;
		while (cnt < *tx_num) {
			tx_slot[cnt] = tx_ch[6 + cnt];
			cnt++;
		}
	}
	return 0;
}

static int sitar_hw_params(struct snd_pcm_substream *substream,
			   struct snd_pcm_hw_params *params,
			   struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(dai->codec);
	u8 path, shift;
	u16 tx_fs_reg, rx_fs_reg;
	u8 tx_fs_rate, rx_fs_rate, rx_state, tx_state;

	pr_err("%s: DAI-ID %x\n", __func__, dai->id);

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
	if (dai->id == AIF1_CAP) {

		tx_state = snd_soc_read(codec,
				SITAR_A_CDC_CLK_TX_CLK_EN_B1_CTL);

		for (path = 1, shift = 0;
				path <= NUM_DECIMATORS; path++, shift++) {

			if (!(tx_state & (1 << shift))) {
				tx_fs_reg = SITAR_A_CDC_TX1_CLK_FS_CTL
						+ (BITS_PER_REG*(path-1));
				snd_soc_update_bits(codec, tx_fs_reg,
							0x03, tx_fs_rate);
			}
		}
		if (sitar->intf_type == WCD9XXX_INTERFACE_TYPE_I2C) {
			switch (params_format(params)) {
			case SNDRV_PCM_FORMAT_S16_LE:
				snd_soc_update_bits(codec,
					SITAR_A_CDC_CLK_TX_I2S_CTL,
					0x20, 0x20);
				break;
			case SNDRV_PCM_FORMAT_S32_LE:
				snd_soc_update_bits(codec,
					SITAR_A_CDC_CLK_TX_I2S_CTL,
					0x20, 0x00);
				break;
			default:
				pr_err("invalid format\n");
				break;
			}
			snd_soc_update_bits(codec, SITAR_A_CDC_CLK_TX_I2S_CTL,
						0x03, tx_fs_rate);
		}
	} else {
		sitar->dai[dai->id - 1].rate   = params_rate(params);
	}

	/**
	* TODO: Need to handle case where same RX chain takes 2 or more inputs
	* with varying sample rates
	*/

	/**
	* If current dai is a rx dai, set sample rate to
	* all the rx paths that are currently not active
	*/
	if (dai->id == AIF1_PB) {

		rx_state = snd_soc_read(codec,
			SITAR_A_CDC_CLK_RX_B1_CTL);

		for (path = 1, shift = 0;
				path <= NUM_INTERPOLATORS; path++, shift++) {

			if (!(rx_state & (1 << shift))) {
				rx_fs_reg = SITAR_A_CDC_RX1_B5_CTL
						+ (BITS_PER_REG*(path-1));
				snd_soc_update_bits(codec, rx_fs_reg,
						0xE0, rx_fs_rate);
			}
		}
		if (sitar->intf_type == WCD9XXX_INTERFACE_TYPE_I2C) {
			switch (params_format(params)) {
			case SNDRV_PCM_FORMAT_S16_LE:
				snd_soc_update_bits(codec,
					SITAR_A_CDC_CLK_RX_I2S_CTL,
					0x20, 0x20);
				break;
			case SNDRV_PCM_FORMAT_S32_LE:
				snd_soc_update_bits(codec,
					SITAR_A_CDC_CLK_RX_I2S_CTL,
					0x20, 0x00);
				break;
			default:
				pr_err("invalid format\n");
				break;
			}
			snd_soc_update_bits(codec, SITAR_A_CDC_CLK_RX_I2S_CTL,
						0x03, (rx_fs_rate >> 0x05));
		}
	} else {
		sitar->dai[dai->id - 1].rate   = params_rate(params);
	}

	return 0;
}

static struct snd_soc_dai_ops sitar_dai_ops = {
	.startup = sitar_startup,
	.shutdown = sitar_shutdown,
	.hw_params = sitar_hw_params,
	.set_sysclk = sitar_set_dai_sysclk,
	.set_fmt = sitar_set_dai_fmt,
	.set_channel_map = sitar_set_channel_map,
	.get_channel_map = sitar_get_channel_map,
};

static struct snd_soc_dai_driver sitar_dai[] = {
	{
		.name = "sitar_rx1",
		.id = AIF1_PB,
		.playback = {
			.stream_name = "AIF1 Playback",
			.rates = WCD9304_RATES,
			.formats = SITAR_FORMATS,
			.rate_max = 48000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &sitar_dai_ops,
	},
	{
		.name = "sitar_tx1",
		.id = AIF1_CAP,
		.capture = {
			.stream_name = "AIF1 Capture",
			.rates = WCD9304_RATES,
			.formats = SITAR_FORMATS,
			.rate_max = 48000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &sitar_dai_ops,
	},
};

static int sitar_codec_enable_slimrx(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct wcd9xxx *sitar;
	struct snd_soc_codec *codec = w->codec;
	struct sitar_priv *sitar_p = snd_soc_codec_get_drvdata(codec);
	u32  j = 0;
	codec->control_data = dev_get_drvdata(codec->dev->parent);
	sitar = codec->control_data;
	/* Execute the callback only if interface type is slimbus */
	if (sitar_p->intf_type != WCD9XXX_INTERFACE_TYPE_SLIMBUS)
		return 0;
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		for (j = 0; j < ARRAY_SIZE(sitar_dai); j++) {
			if (sitar_dai[j].id == AIF1_CAP)
				continue;
			if (!strncmp(w->sname,
				sitar_dai[j].playback.stream_name, 13)) {
				++sitar_p->dai[j].ch_act;
				break;
			}
		}
		if (sitar_p->dai[j].ch_act == sitar_p->dai[j].ch_tot)
			wcd9xxx_cfg_slim_sch_rx(sitar,
					sitar_p->dai[j].ch_num,
					sitar_p->dai[j].ch_tot,
					sitar_p->dai[j].rate);
		break;
	case SND_SOC_DAPM_POST_PMD:
		for (j = 0; j < ARRAY_SIZE(sitar_dai); j++) {
			if (sitar_dai[j].id == AIF1_CAP)
				continue;
			if (!strncmp(w->sname,
				sitar_dai[j].playback.stream_name, 13)) {
				--sitar_p->dai[j].ch_act;
				break;
			}
		}
		if (!sitar_p->dai[j].ch_act) {
			wcd9xxx_close_slim_sch_rx(sitar,
					sitar_p->dai[j].ch_num,
					sitar_p->dai[j].ch_tot);
			/* Wait for remove channel to complete
			 * before derouting Rx path
			 */
			usleep_range(15000, 15000);
			sitar_p->dai[j].rate = 0;
			memset(sitar_p->dai[j].ch_num, 0, (sizeof(u32)*
					sitar_p->dai[j].ch_tot));
			sitar_p->dai[j].ch_tot = 0;
		}
	}
	return 0;
}

static int sitar_codec_enable_slimtx(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct wcd9xxx *sitar;
	struct snd_soc_codec *codec = w->codec;
	struct sitar_priv *sitar_p = snd_soc_codec_get_drvdata(codec);
	/* index to the DAI ID, for now hardcoding */
	u32  j = 0;

	codec->control_data = dev_get_drvdata(codec->dev->parent);
	sitar = codec->control_data;

	/* Execute the callback only if interface type is slimbus */
	if (sitar_p->intf_type != WCD9XXX_INTERFACE_TYPE_SLIMBUS)
		return 0;
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		for (j = 0; j < ARRAY_SIZE(sitar_dai); j++) {
			if (sitar_dai[j].id == AIF1_PB)
				continue;
			if (!strncmp(w->sname,
				sitar_dai[j].capture.stream_name, 13)) {
				++sitar_p->dai[j].ch_act;
				break;
			}
		}
		if (sitar_p->dai[j].ch_act == sitar_p->dai[j].ch_tot)
			wcd9xxx_cfg_slim_sch_tx(sitar,
					sitar_p->dai[j].ch_num,
					sitar_p->dai[j].ch_tot,
					sitar_p->dai[j].rate);
		break;
	case SND_SOC_DAPM_POST_PMD:
		for (j = 0; j < ARRAY_SIZE(sitar_dai); j++) {
			if (sitar_dai[j].id == AIF1_PB)
				continue;
			if (!strncmp(w->sname,
				sitar_dai[j].capture.stream_name, 13)) {
				--sitar_p->dai[j].ch_act;
				break;
			}
		}
		if (!sitar_p->dai[j].ch_act) {
			wcd9xxx_close_slim_sch_tx(sitar,
					sitar_p->dai[j].ch_num,
					sitar_p->dai[j].ch_tot);
			sitar_p->dai[j].rate = 0;
			memset(sitar_p->dai[j].ch_num, 0, (sizeof(u32)*
					sitar_p->dai[j].ch_tot));
			sitar_p->dai[j].ch_tot = 0;
		}
	}
	return 0;
}


static short sitar_codec_read_sta_result(struct snd_soc_codec *codec)
{
	u8 bias_msb, bias_lsb;
	short bias_value;

	bias_msb = snd_soc_read(codec, SITAR_A_CDC_MBHC_B3_STATUS);
	bias_lsb = snd_soc_read(codec, SITAR_A_CDC_MBHC_B2_STATUS);
	bias_value = (bias_msb << 8) | bias_lsb;
	return bias_value;
}

static short sitar_codec_read_dce_result(struct snd_soc_codec *codec)
{
	u8 bias_msb, bias_lsb;
	short bias_value;

	bias_msb = snd_soc_read(codec, SITAR_A_CDC_MBHC_B5_STATUS);
	bias_lsb = snd_soc_read(codec, SITAR_A_CDC_MBHC_B4_STATUS);
	bias_value = (bias_msb << 8) | bias_lsb;
	return bias_value;
}

static short sitar_codec_measure_micbias_voltage(struct snd_soc_codec *codec,
	int dce)
{
	short bias_value;

	if (dce) {
		snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_CLK_CTL, 0x8, 0x8);
		snd_soc_write(codec, SITAR_A_CDC_MBHC_EN_CTL, 0x4);
		snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_CLK_CTL, 0x8, 0x0);
		snd_soc_write(codec, SITAR_A_CDC_MBHC_EN_CTL, 0x4);
		usleep_range(60000, 60000);
		bias_value = sitar_codec_read_dce_result(codec);
	} else {
		snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_CLK_CTL, 0x8, 0x8);
		snd_soc_write(codec, SITAR_A_CDC_MBHC_EN_CTL, 0x2);
		snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_CLK_CTL, 0x8, 0x0);
		usleep_range(5000, 5000);
		snd_soc_write(codec, SITAR_A_CDC_MBHC_EN_CTL, 0x2);
		usleep_range(50, 50);
		bias_value = sitar_codec_read_sta_result(codec);
		snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_CLK_CTL, 0x8, 0x8);
		snd_soc_write(codec, SITAR_A_CDC_MBHC_EN_CTL, 0x0);
	}

	pr_err("read microphone bias value %x\n", bias_value);
	return bias_value;
}

static short sitar_codec_setup_hs_polling(struct snd_soc_codec *codec)
{
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);
	struct sitar_mbhc_calibration *calibration = sitar->calibration;
	short bias_value;
	u8 cfilt_mode;

	if (!calibration) {
		pr_err("Error, no sitar calibration\n");
		return -ENODEV;
	}

	sitar->mbhc_polling_active = true;

	if (!sitar->mclk_enabled) {
		sitar_codec_enable_bandgap(codec, SITAR_BANDGAP_MBHC_MODE);
		sitar_enable_rx_bias(codec, 1);
		sitar_codec_enable_clock_block(codec, 1);
	}

	snd_soc_update_bits(codec, SITAR_A_CLK_BUFF_EN1, 0x05, 0x01);

	snd_soc_update_bits(codec, SITAR_A_TX_COM_BIAS, 0xE0, 0xE0);

	/* Make sure CFILT is in fast mode, save current mode */
	cfilt_mode = snd_soc_read(codec,
		sitar->mbhc_bias_regs.cfilt_ctl);
	snd_soc_update_bits(codec, sitar->mbhc_bias_regs.cfilt_ctl,
		0x70, 0x00);

	snd_soc_update_bits(codec,
		sitar->mbhc_bias_regs.ctl_reg, 0x1F, 0x16);

	snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_CLK_CTL, 0x2, 0x2);
	snd_soc_write(codec, SITAR_A_MBHC_SCALING_MUX_1, 0x84);

	snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_CLK_CTL, 0x8, 0x8);
	snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_CLK_CTL, 0x8, 0x00);

	snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_B1_CTL, 0x6, 0x6);
	snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_CLK_CTL, 0x8, 0x8);

	sitar_codec_calibrate_hs_polling(codec);

	bias_value = sitar_codec_measure_micbias_voltage(codec, 0);
	snd_soc_update_bits(codec,
		sitar->mbhc_bias_regs.cfilt_ctl, 0x40, cfilt_mode);
	snd_soc_update_bits(codec, SITAR_A_MBHC_HPH, 0x13, 0x00);

	return bias_value;
}

static int sitar_codec_enable_hs_detect(struct snd_soc_codec *codec,
		int insertion)
{
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);
	struct sitar_mbhc_calibration *calibration = sitar->calibration;
	int central_bias_enabled = 0;
	u8 wg_time;

	if (!calibration) {
		pr_err("Error, no sitar calibration\n");
		return -EINVAL;
	}

	snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_INT_CTL, 0x1, 0);

	if (insertion) {
		/* Make sure mic bias and Mic line schmitt trigger
		* are turned OFF
		*/
		snd_soc_update_bits(codec, sitar->mbhc_bias_regs.ctl_reg,
			0x81, 0x01);
		snd_soc_update_bits(codec, sitar->mbhc_bias_regs.mbhc_reg,
			0x90, 0x00);
		wg_time = snd_soc_read(codec, SITAR_A_RX_HPH_CNP_WG_TIME) ;
		wg_time += 1;

		/* Enable HPH Schmitt Trigger */
		snd_soc_update_bits(codec, SITAR_A_MBHC_HPH, 0x11, 0x11);
		snd_soc_update_bits(codec, SITAR_A_MBHC_HPH, 0x0C,
			calibration->hph_current << 2);

		/* Turn off HPH PAs and DAC's during insertion detection to
		* avoid false insertion interrupts
		*/
		if (sitar->mbhc_micbias_switched)
			sitar_codec_switch_micbias(codec, 0);
		snd_soc_update_bits(codec, SITAR_A_RX_HPH_CNP_EN, 0x30, 0x00);
		snd_soc_update_bits(codec, SITAR_A_RX_HPH_L_DAC_CTL,
			0xC0, 0x00);
		snd_soc_update_bits(codec, SITAR_A_RX_HPH_R_DAC_CTL,
			0xC0, 0x00);
		usleep_range(wg_time * 1000, wg_time * 1000);

		/* setup for insetion detection */
		snd_soc_update_bits(codec, SITAR_A_MBHC_HPH, 0x02, 0x02);
		snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_INT_CTL, 0x2, 0);
	} else {
		/* Make sure the HPH schmitt trigger is OFF */
		snd_soc_update_bits(codec, SITAR_A_MBHC_HPH, 0x12, 0x00);

		/* enable the mic line schmitt trigger */
		snd_soc_update_bits(codec, sitar->mbhc_bias_regs.mbhc_reg, 0x60,
			calibration->mic_current << 5);
		snd_soc_update_bits(codec, sitar->mbhc_bias_regs.mbhc_reg,
			0x80, 0x80);
		usleep_range(calibration->mic_pid, calibration->mic_pid);
		snd_soc_update_bits(codec, sitar->mbhc_bias_regs.mbhc_reg,
			0x10, 0x10);

		/* Setup for low power removal detection */
		snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_INT_CTL, 0x2, 0x2);
	}

	if (snd_soc_read(codec, SITAR_A_CDC_MBHC_B1_CTL) & 0x4) {
		if (!(sitar->clock_active)) {
			snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_B1_CTL,
				0x06, 0);
			usleep_range(calibration->shutdown_plug_removal,
				calibration->shutdown_plug_removal);
		} else
			snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_B1_CTL,
				0x06, 0);
	}

	snd_soc_update_bits(codec, sitar->mbhc_bias_regs.int_rbias, 0x80, 0);

	/* If central bandgap disabled */
	if (!(snd_soc_read(codec, SITAR_A_PIN_CTL_OE1) & 1)) {
		snd_soc_update_bits(codec, SITAR_A_PIN_CTL_OE1, 0x3, 0x3);
		usleep_range(calibration->bg_fast_settle,
			calibration->bg_fast_settle);
		central_bias_enabled = 1;
	}

	/* If LDO_H disabled */
	if (snd_soc_read(codec, SITAR_A_PIN_CTL_OE0) & 0x80) {
		snd_soc_update_bits(codec, SITAR_A_PIN_CTL_OE0, 0x10, 0);
		snd_soc_update_bits(codec, SITAR_A_PIN_CTL_OE0, 0x80, 0x80);
		usleep_range(calibration->tldoh, calibration->tldoh);
		snd_soc_update_bits(codec, SITAR_A_PIN_CTL_OE0, 0x80, 0);

		if (central_bias_enabled)
			snd_soc_update_bits(codec, SITAR_A_PIN_CTL_OE1, 0x1, 0);
	}

	wcd9xxx_enable_irq(codec->control_data, SITAR_IRQ_MBHC_INSERTION);
	snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_INT_CTL, 0x1, 0x1);
	return 0;
}

static void btn0_lpress_fn(struct work_struct *work)
{
	struct delayed_work *delayed_work;
	struct sitar_priv *sitar;

	pr_err("%s:\n", __func__);

	delayed_work = to_delayed_work(work);
	sitar = container_of(delayed_work, struct sitar_priv, btn0_dwork);

	if (sitar) {
		if (sitar->button_jack) {
			pr_err("%s: Reporting long button press event\n",
					__func__);
			sitar_snd_soc_jack_report(sitar, sitar->button_jack,
						 SND_JACK_BTN_0,
						 SND_JACK_BTN_0);
		}
	} else {
		pr_err("%s: Bad sitar private data\n", __func__);
	}

}

int sitar_hs_detect(struct snd_soc_codec *codec,
	struct snd_soc_jack *headset_jack, struct snd_soc_jack *button_jack,
	struct sitar_mbhc_calibration *calibration)
{
	struct sitar_priv *sitar;
	int rc;

	if (!codec || !calibration) {
		pr_err("Error: no codec or calibration\n");
		return -EINVAL;
	}
	sitar = snd_soc_codec_get_drvdata(codec);
	sitar->headset_jack = headset_jack;
	sitar->button_jack = button_jack;
	sitar->calibration = calibration;
	sitar_get_mbhc_micbias_regs(codec, &sitar->mbhc_bias_regs);

	/* Put CFILT in fast mode by default */
	snd_soc_update_bits(codec, sitar->mbhc_bias_regs.cfilt_ctl,
		0x40, SITAR_CFILT_FAST_MODE);

	INIT_DELAYED_WORK(&sitar->btn0_dwork, btn0_lpress_fn);
	INIT_WORK(&sitar->hphlocp_work, hphlocp_off_report);
	INIT_WORK(&sitar->hphrocp_work, hphrocp_off_report);
	rc =  sitar_codec_enable_hs_detect(codec, 1);

	if (!IS_ERR_VALUE(rc)) {
		snd_soc_update_bits(codec, SITAR_A_RX_HPH_OCP_CTL, 0x10,
			0x10);
		wcd9xxx_enable_irq(codec->control_data,
			SITAR_IRQ_HPH_PA_OCPL_FAULT);
		wcd9xxx_enable_irq(codec->control_data,
			SITAR_IRQ_HPH_PA_OCPR_FAULT);
	}

	return rc;
}
EXPORT_SYMBOL_GPL(sitar_hs_detect);

static irqreturn_t sitar_dce_handler(int irq, void *data)
{
	struct sitar_priv *priv = data;
	struct snd_soc_codec *codec = priv->codec;
	short bias_value;
	struct wcd9xxx *core = dev_get_drvdata(priv->codec->dev->parent);

	wcd9xxx_disable_irq(codec->control_data, SITAR_IRQ_MBHC_REMOVAL);
	wcd9xxx_disable_irq(codec->control_data, SITAR_IRQ_MBHC_POTENTIAL);

	bias_value = sitar_codec_read_dce_result(codec);
	pr_err("%s: button press interrupt, bias value(DCE Read)=%d\n",
			__func__, bias_value);

	bias_value = sitar_codec_read_sta_result(codec);
	pr_err("%s: button press interrupt, bias value(STA Read)=%d\n",
			__func__, bias_value);
	/*
	* TODO: If button pressed is not button 0,
	* report the button press event immediately.
	*/
	priv->buttons_pressed |= SND_JACK_BTN_0;

	msleep(100);

	wcd9xxx_lock_sleep(core);
	if (schedule_delayed_work(&priv->btn0_dwork,
				 msecs_to_jiffies(400)) == 0) {
		WARN(1, "Button pressed twice without release event\n");
		wcd9xxx_unlock_sleep(core);
	}

	return IRQ_HANDLED;
}

static irqreturn_t sitar_release_handler(int irq, void *data)
{
	struct sitar_priv *priv = data;
	struct snd_soc_codec *codec = priv->codec;
	int ret, mic_voltage;
	struct wcd9xxx *core = dev_get_drvdata(priv->codec->dev->parent);

	pr_err("%s\n", __func__);
	wcd9xxx_disable_irq(codec->control_data, SITAR_IRQ_MBHC_RELEASE);

	mic_voltage = sitar_codec_read_dce_result(codec);
	pr_err("%s: Microphone Voltage on release(DCE Read) = %d\n",
		__func__, mic_voltage);

	if (priv->buttons_pressed & SND_JACK_BTN_0) {
		ret = cancel_delayed_work(&priv->btn0_dwork);

		if (ret == 0) {

			pr_err("%s: Reporting long button release event\n",
					__func__);
			if (priv->button_jack) {
				sitar_snd_soc_jack_report(priv,
							 priv->button_jack, 0,
							 SND_JACK_BTN_0);
			}

		} else {
			/* if scheduled btn0_dwork is canceled from here,
			* we have to unlock from here instead btn0_work */
			wcd9xxx_unlock_sleep(core);
			mic_voltage =
				sitar_codec_measure_micbias_voltage(codec, 0);
			pr_err("%s: Mic Voltage on release(new STA) = %d\n",
						__func__, mic_voltage);

			if (mic_voltage < -2000 || mic_voltage > -670) {
				pr_err("%s: Fake buttton press interrupt\n",
						__func__);
			} else {

				if (priv->button_jack) {
					pr_err("%s:reporting short button press and release\n",
							__func__);

					sitar_snd_soc_jack_report(priv,
							     priv->button_jack,
						SND_JACK_BTN_0, SND_JACK_BTN_0);
					sitar_snd_soc_jack_report(priv,
							    priv->button_jack,
							    0, SND_JACK_BTN_0);
				}
			}
		}

		priv->buttons_pressed &= ~SND_JACK_BTN_0;
	}

	sitar_codec_start_hs_polling(codec);
	return IRQ_HANDLED;
}

static void sitar_codec_shutdown_hs_removal_detect(struct snd_soc_codec *codec)
{
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);
	struct sitar_mbhc_calibration *calibration = sitar->calibration;

	snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_CLK_CTL, 0x2, 0x2);
	snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_B1_CTL, 0x6, 0x0);

	snd_soc_update_bits(codec,
		sitar->mbhc_bias_regs.mbhc_reg, 0x80, 0x00);
	usleep_range(calibration->shutdown_plug_removal,
		calibration->shutdown_plug_removal);

	snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_CLK_CTL, 0xA, 0x8);

	snd_soc_write(codec, SITAR_A_MBHC_SCALING_MUX_1, 0x00);
}

static void sitar_codec_shutdown_hs_polling(struct snd_soc_codec *codec)
{
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);

	sitar_codec_shutdown_hs_removal_detect(codec);

	if (!sitar->mclk_enabled) {
		snd_soc_update_bits(codec, SITAR_A_TX_COM_BIAS, 0xE0, 0x00);
		sitar_codec_enable_bandgap(codec, SITAR_BANDGAP_AUDIO_MODE);
		sitar_codec_enable_clock_block(codec, 0);
	}

	sitar->mbhc_polling_active = false;
}

static irqreturn_t sitar_hphl_ocp_irq(int irq, void *data)
{
	struct sitar_priv *sitar = data;
	struct snd_soc_codec *codec;

	pr_info("%s: received HPHL OCP irq\n", __func__);

	if (sitar) {
		codec = sitar->codec;
		if (sitar->hphlocp_cnt++ < SITAR_OCP_ATTEMPT) {
			pr_info("%s: retry\n", __func__);
			snd_soc_update_bits(codec, SITAR_A_RX_HPH_OCP_CTL, 0x10,
					   0x00);
			snd_soc_update_bits(codec, SITAR_A_RX_HPH_OCP_CTL, 0x10,
					   0x10);
		} else {
			wcd9xxx_disable_irq(codec->control_data,
					 SITAR_IRQ_HPH_PA_OCPL_FAULT);
			sitar->hphlocp_cnt = 0;
			sitar->hph_status |= SND_JACK_OC_HPHL;
			if (sitar->headset_jack)
				sitar_snd_soc_jack_report(sitar,
							 sitar->headset_jack,
							 sitar->hph_status,
							 SITAR_JACK_MASK);
		}
	} else {
		pr_err("%s: Bad sitar private data\n", __func__);
	}

	return IRQ_HANDLED;
}

static irqreturn_t sitar_hphr_ocp_irq(int irq, void *data)
{
	struct sitar_priv *sitar = data;
	struct snd_soc_codec *codec;

	pr_info("%s: received HPHR OCP irq\n", __func__);

	if (sitar) {
		codec = sitar->codec;
		if (sitar->hphrocp_cnt++ < SITAR_OCP_ATTEMPT) {
			pr_info("%s: retry\n", __func__);
			snd_soc_update_bits(codec, SITAR_A_RX_HPH_OCP_CTL, 0x10,
					   0x00);
			snd_soc_update_bits(codec, SITAR_A_RX_HPH_OCP_CTL, 0x10,
					   0x10);
		} else {
			wcd9xxx_disable_irq(codec->control_data,
					 SITAR_IRQ_HPH_PA_OCPR_FAULT);
			sitar->hphrocp_cnt = 0;
			sitar->hph_status |= SND_JACK_OC_HPHR;
			if (sitar->headset_jack)
				sitar_snd_soc_jack_report(sitar,
							 sitar->headset_jack,
							 sitar->hph_status,
							 SITAR_JACK_MASK);
		}
	} else {
		pr_err("%s: Bad sitar private data\n", __func__);
	}

	return IRQ_HANDLED;
}

static void sitar_sync_hph_state(struct sitar_priv *sitar)
{
	if (test_and_clear_bit(SITAR_HPHR_PA_OFF_ACK,
				&sitar->hph_pa_dac_state)) {
		pr_err("%s: HPHR clear flag and enable PA\n", __func__);
		snd_soc_update_bits(sitar->codec, SITAR_A_RX_HPH_CNP_EN, 0x10,
				   1 << 4);
	}
	if (test_and_clear_bit(SITAR_HPHL_PA_OFF_ACK,
				&sitar->hph_pa_dac_state)) {
		pr_err("%s: HPHL clear flag and enable PA\n", __func__);
		snd_soc_update_bits(sitar->codec, SITAR_A_RX_HPH_CNP_EN, 0x20,
				   1 << 5);
	}

	if (test_and_clear_bit(SITAR_HPHR_DAC_OFF_ACK,
				&sitar->hph_pa_dac_state)) {
		pr_err("%s: HPHR clear flag and enable DAC\n", __func__);
		snd_soc_update_bits(sitar->codec, SITAR_A_RX_HPH_R_DAC_CTL,
				   0xC0, 0xC0);
	}
	if (test_and_clear_bit(SITAR_HPHL_DAC_OFF_ACK,
				&sitar->hph_pa_dac_state)) {
		pr_err("%s: HPHL clear flag and enable DAC\n", __func__);
		snd_soc_update_bits(sitar->codec, SITAR_A_RX_HPH_L_DAC_CTL,
				   0xC0, 0xC0);
	}
}

static irqreturn_t sitar_hs_insert_irq(int irq, void *data)
{
	struct sitar_priv *priv = data;
	struct snd_soc_codec *codec = priv->codec;
	int ldo_h_on, micb_cfilt_on;
	short mic_voltage;
	short threshold_no_mic = 0xF7F6;
	short threshold_fake_insert = 0xFD30;
	u8 is_removal;

	pr_err("%s\n", __func__);
	wcd9xxx_disable_irq(codec->control_data, SITAR_IRQ_MBHC_INSERTION);

	is_removal = snd_soc_read(codec, SITAR_A_CDC_MBHC_INT_CTL) & 0x02;
	snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_INT_CTL, 0x03, 0x00);

	/* Turn off both HPH and MIC line schmitt triggers */
	snd_soc_update_bits(codec, priv->mbhc_bias_regs.mbhc_reg,
			0x90, 0x00);
	snd_soc_update_bits(codec, SITAR_A_MBHC_HPH, 0x13, 0x00);

	if (priv->fake_insert_context) {
		pr_err("%s: fake context interrupt, reset insertion\n",
			__func__);
		priv->fake_insert_context = false;
		sitar_codec_shutdown_hs_polling(codec);
		sitar_codec_enable_hs_detect(codec, 1);
		return IRQ_HANDLED;
	}


	ldo_h_on = snd_soc_read(codec, SITAR_A_LDO_H_MODE_1) & 0x80;
	micb_cfilt_on = snd_soc_read(codec,
					priv->mbhc_bias_regs.cfilt_ctl) & 0x80;

	if (!ldo_h_on)
		snd_soc_update_bits(codec, SITAR_A_LDO_H_MODE_1, 0x80, 0x80);
	if (!micb_cfilt_on)
		snd_soc_update_bits(codec, priv->mbhc_bias_regs.cfilt_ctl,
							0x80, 0x80);

	usleep_range(priv->calibration->setup_plug_removal_delay,
		priv->calibration->setup_plug_removal_delay);

	if (!ldo_h_on)
		snd_soc_update_bits(codec, SITAR_A_LDO_H_MODE_1, 0x80, 0x0);
	if (!micb_cfilt_on)
		snd_soc_update_bits(codec, priv->mbhc_bias_regs.cfilt_ctl,
							0x80, 0x0);

	if (is_removal) {
		/*
		* If headphone is removed while playback is in progress,
		* it is possible that micbias will be switched to VDDIO.
		*/
		if (priv->mbhc_micbias_switched)
			sitar_codec_switch_micbias(codec, 0);
		priv->hph_status &= ~SND_JACK_HEADPHONE;

		/* If headphone PA is on, check if userspace receives
		* removal event to sync-up PA's state */
		if (sitar_is_hph_pa_on(codec)) {
			set_bit(SITAR_HPHL_PA_OFF_ACK, &priv->hph_pa_dac_state);
			set_bit(SITAR_HPHR_PA_OFF_ACK, &priv->hph_pa_dac_state);
		}

		if (sitar_is_hph_dac_on(codec, 1))
			set_bit(SITAR_HPHL_DAC_OFF_ACK,
				&priv->hph_pa_dac_state);
		if (sitar_is_hph_dac_on(codec, 0))
			set_bit(SITAR_HPHR_DAC_OFF_ACK,
				&priv->hph_pa_dac_state);

		if (priv->headset_jack) {
			pr_err("%s: Reporting removal\n", __func__);
			sitar_snd_soc_jack_report(priv, priv->headset_jack,
						 priv->hph_status,
						 SITAR_JACK_MASK);
		}
		sitar_codec_shutdown_hs_removal_detect(codec);
		sitar_codec_enable_hs_detect(codec, 1);
		return IRQ_HANDLED;
	}

	mic_voltage = sitar_codec_setup_hs_polling(codec);

	if (mic_voltage > threshold_fake_insert) {
		pr_err("%s: Fake insertion interrupt, mic_voltage = %x\n",
			__func__, mic_voltage);

		/* Disable HPH trigger and enable MIC line trigger */
		snd_soc_update_bits(codec, SITAR_A_MBHC_HPH, 0x12, 0x00);

		snd_soc_update_bits(codec, priv->mbhc_bias_regs.mbhc_reg, 0x60,
			priv->calibration->mic_current << 5);
		snd_soc_update_bits(codec, priv->mbhc_bias_regs.mbhc_reg,
			0x80, 0x80);
		usleep_range(priv->calibration->mic_pid,
			priv->calibration->mic_pid);
		snd_soc_update_bits(codec, priv->mbhc_bias_regs.mbhc_reg,
			0x10, 0x10);

		/* Setup for insertion detection */
		snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_INT_CTL, 0x2, 0);
		priv->fake_insert_context = true;
		wcd9xxx_enable_irq(codec->control_data,
				SITAR_IRQ_MBHC_INSERTION);
		snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_INT_CTL, 0x1, 0x1);

	} else if (mic_voltage < threshold_no_mic) {
		pr_err("%s: Headphone Detected, mic_voltage = %x\n",
			__func__, mic_voltage);
		priv->hph_status |= SND_JACK_HEADPHONE;
		if (priv->headset_jack) {
			pr_err("%s: Reporting insertion %d\n", __func__,
				SND_JACK_HEADPHONE);
			sitar_snd_soc_jack_report(priv, priv->headset_jack,
						 priv->hph_status,
						 SITAR_JACK_MASK);
		}
		sitar_codec_shutdown_hs_polling(codec);
		sitar_codec_enable_hs_detect(codec, 0);
		sitar_sync_hph_state(priv);
	} else {
		pr_err("%s: Headset detected, mic_voltage = %x\n",
			__func__, mic_voltage);
		priv->hph_status |= SND_JACK_HEADSET;
		if (priv->headset_jack) {
			pr_err("%s: Reporting insertion %d\n", __func__,
				SND_JACK_HEADSET);
			sitar_snd_soc_jack_report(priv, priv->headset_jack,
						 priv->hph_status,
						 SITAR_JACK_MASK);
		}
		sitar_codec_start_hs_polling(codec);
		sitar_sync_hph_state(priv);
	}

	return IRQ_HANDLED;
}

static irqreturn_t sitar_hs_remove_irq(int irq, void *data)
{
	struct sitar_priv *priv = data;
	struct snd_soc_codec *codec = priv->codec;
	short bias_value;

	wcd9xxx_disable_irq(codec->control_data, SITAR_IRQ_MBHC_REMOVAL);
	wcd9xxx_disable_irq(codec->control_data, SITAR_IRQ_MBHC_POTENTIAL);
	wcd9xxx_disable_irq(codec->control_data, SITAR_IRQ_MBHC_RELEASE);

	usleep_range(priv->calibration->shutdown_plug_removal,
		priv->calibration->shutdown_plug_removal);

	bias_value = sitar_codec_measure_micbias_voltage(codec, 1);
	pr_err("removal interrupt, bias value is %d\n", bias_value);

	if (bias_value < -90) {
		pr_err("False alarm, headset not actually removed\n");
		sitar_codec_start_hs_polling(codec);
	} else {
		/*
		* If this removal is not false, first check the micbias
		* switch status and switch it to LDOH if it is already
		* switched to VDDIO.
		*/
		if (priv->mbhc_micbias_switched)
			sitar_codec_switch_micbias(codec, 0);
		priv->hph_status &= ~SND_JACK_HEADSET;
		if (priv->headset_jack) {
			pr_err("%s: Reporting removal\n", __func__);
			sitar_snd_soc_jack_report(priv, priv->headset_jack, 0,
						 SITAR_JACK_MASK);
		}
		sitar_codec_shutdown_hs_polling(codec);

		sitar_codec_enable_hs_detect(codec, 1);
	}

	return IRQ_HANDLED;
}


static unsigned long slimbus_value;

static irqreturn_t sitar_slimbus_irq(int irq, void *data)
{
	struct sitar_priv *priv = data;
	struct snd_soc_codec *codec = priv->codec;
	int i, j;
	u8 val;


	for (i = 0; i < WCD9XXX_SLIM_NUM_PORT_REG; i++) {
		slimbus_value = wcd9xxx_interface_reg_read(codec->control_data,
			SITAR_SLIM_PGD_PORT_INT_STATUS0 + i);
		for_each_set_bit(j, &slimbus_value, BITS_PER_BYTE) {
			val = wcd9xxx_interface_reg_read(codec->control_data,
				SITAR_SLIM_PGD_PORT_INT_SOURCE0 + i*8 + j);
			if (val & 0x1)
				pr_err_ratelimited("overflow error on port %x,"
					" value %x\n", i*8 + j, val);
			if (val & 0x2)
				pr_err_ratelimited("underflow error on port %x,"
					" value %x\n", i*8 + j, val);
		}
		wcd9xxx_interface_reg_write(codec->control_data,
			SITAR_SLIM_PGD_PORT_INT_CLR0 + i, 0xFF);
	}

	return IRQ_HANDLED;
}


static int sitar_handle_pdata(struct sitar_priv *sitar)
{
	struct snd_soc_codec *codec = sitar->codec;
	struct wcd9xxx_pdata *pdata = sitar->pdata;
	int k1, k2, rc = 0;
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
	if ((pdata->micbias.ldoh_v > SITAR_LDOH_2P85_V) ||
	   (pdata->micbias.bias1_cfilt_sel > SITAR_CFILT2_SEL) ||
	   (pdata->micbias.bias2_cfilt_sel > SITAR_CFILT2_SEL)) {
		rc = -EINVAL;
		goto done;
	}

	/* figure out k value */
	k1 = sitar_find_k_value(pdata->micbias.ldoh_v,
		pdata->micbias.cfilt1_mv);
	k2 = sitar_find_k_value(pdata->micbias.ldoh_v,
		pdata->micbias.cfilt2_mv);

	if (IS_ERR_VALUE(k1) || IS_ERR_VALUE(k2)) {
		rc = -EINVAL;
		goto done;
	}

	/* Set voltage level and always use LDO */

	snd_soc_update_bits(codec, SITAR_A_MICB_CFILT_1_VAL, 0xFC,
		(k1 << 2));
	snd_soc_update_bits(codec, SITAR_A_MICB_CFILT_2_VAL, 0xFC,
		(k2 << 2));

	snd_soc_update_bits(codec, SITAR_A_MICB_1_CTL, 0x60,
		(pdata->micbias.bias1_cfilt_sel << 5));
	snd_soc_update_bits(codec, SITAR_A_MICB_2_CTL, 0x60,
		(pdata->micbias.bias2_cfilt_sel << 5));

	for (i = 0; i < 6; j++, i += 2) {
		if (flag & (0x01 << i)) {
			value = (leg_mode & (0x01 << i)) ? 0x10 : 0x00;
			val_txfe = (txfe_bypass & (0x01 << i)) ? 0x20 : 0x00;
			val_txfe = val_txfe |
				((txfe_buff & (0x01 << i)) ? 0x10 : 0x00);
			snd_soc_update_bits(codec, SITAR_A_TX_1_2_EN + j * 10,
				0x10, value);
			snd_soc_update_bits(codec,
				SITAR_A_TX_1_2_TEST_EN + j * 10,
				0x30, val_txfe);
		}
		if (flag & (0x01 << (i + 1))) {
			value = (leg_mode & (0x01 << (i + 1))) ? 0x01 : 0x00;
			val_txfe = (txfe_bypass &
					(0x01 << (i + 1))) ? 0x02 : 0x00;
			val_txfe |= (txfe_buff &
					(0x01 << (i + 1))) ? 0x01 : 0x00;
			snd_soc_update_bits(codec, SITAR_A_TX_1_2_EN + j * 10,
				0x01, value);
			snd_soc_update_bits(codec,
				SITAR_A_TX_1_2_TEST_EN + j * 10,
				0x03, val_txfe);
		}
	}

	if (pdata->ocp.use_pdata) {
		/* not defined in CODEC specification */
		if (pdata->ocp.hph_ocp_limit == 1 ||
			pdata->ocp.hph_ocp_limit == 5) {
			rc = -EINVAL;
			goto done;
		}
		snd_soc_update_bits(codec, SITAR_A_RX_COM_OCP_CTL,
			0x0F, pdata->ocp.num_attempts);
		snd_soc_write(codec, SITAR_A_RX_COM_OCP_COUNT,
			((pdata->ocp.run_time << 4) | pdata->ocp.wait_time));
		snd_soc_update_bits(codec, SITAR_A_RX_HPH_OCP_CTL,
			0xE0, (pdata->ocp.hph_ocp_limit << 5));
	}
done:
	return rc;
}

static const struct sitar_reg_mask_val sitar_1_1_reg_defaults[] = {

	/* Sitar 1.1 MICBIAS changes */
	SITAR_REG_VAL(SITAR_A_MICB_1_INT_RBIAS, 0x24),
	SITAR_REG_VAL(SITAR_A_MICB_2_INT_RBIAS, 0x24),

	/* Sitar 1.1 HPH changes */
	SITAR_REG_VAL(SITAR_A_RX_HPH_BIAS_PA, 0x57),
	SITAR_REG_VAL(SITAR_A_RX_HPH_BIAS_LDO, 0x56),

	/* Sitar 1.1 EAR PA changes */
	SITAR_REG_VAL(SITAR_A_RX_EAR_BIAS_PA, 0xA6),
	SITAR_REG_VAL(SITAR_A_RX_EAR_GAIN, 0x02),
	SITAR_REG_VAL(SITAR_A_RX_EAR_VCM, 0x03),

	/* Sitar 1.1 RX Changes */
	SITAR_REG_VAL(SITAR_A_CDC_RX1_B5_CTL, 0x78),

	/* Sitar 1.1 RX1 and RX2 Changes */
	SITAR_REG_VAL(SITAR_A_CDC_RX1_B6_CTL, 0x80),

	SITAR_REG_VAL(SITAR_A_CDC_CLSG_FREQ_THRESH_B3_CTL, 0x1B),

};

static void sitar_update_reg_defaults(struct snd_soc_codec *codec)
{
	u32 i;
	for (i = 0; i < ARRAY_SIZE(sitar_1_1_reg_defaults); i++)
		snd_soc_write(codec, sitar_1_1_reg_defaults[i].reg,
				sitar_1_1_reg_defaults[i].val);

}
static const struct sitar_reg_mask_val sitar_codec_reg_init_val[] = {
	/* Initialize current threshold to 350MA
	* number of wait and run cycles to 4096
	*/
	{SITAR_A_RX_HPH_OCP_CTL, 0xF8, 0x60},
	{SITAR_A_RX_COM_OCP_COUNT, 0xFF, 0xFF},

	{SITAR_A_QFUSE_CTL, 0xFF, 0x03},

	/* Initialize gain registers to use register gain */
	{SITAR_A_RX_HPH_L_GAIN, 0x10, 0x10},
	{SITAR_A_RX_HPH_R_GAIN, 0x10, 0x10},
	{SITAR_A_RX_LINE_1_GAIN, 0x10, 0x10},
	{SITAR_A_RX_LINE_2_GAIN, 0x10, 0x10},

	/* Initialize mic biases to differential mode */
	{SITAR_A_MICB_1_INT_RBIAS, 0x24, 0x24},
	{SITAR_A_MICB_2_INT_RBIAS, 0x24, 0x24},

	{SITAR_A_CDC_CONN_CLSG_CTL, 0x3C, 0x14},

	/* Use 16 bit sample size for TX1 to TX6 */
	{SITAR_A_CDC_CONN_TX_SB_B1_CTL, 0x30, 0x28},
	{SITAR_A_CDC_CONN_TX_SB_B2_CTL, 0x30, 0x20},
	{SITAR_A_CDC_CONN_TX_SB_B3_CTL, 0x30, 0x20},
	{SITAR_A_CDC_CONN_TX_SB_B4_CTL, 0x30, 0x20},
	{SITAR_A_CDC_CONN_TX_SB_B5_CTL, 0x30, 0x20},

	/* Use 16 bit sample size for RX */
	{SITAR_A_CDC_CONN_RX_SB_B1_CTL, 0xFF, 0xAA},
	{SITAR_A_CDC_CONN_RX_SB_B2_CTL, 0x02, 0x02},

	/*enable HPF filter for TX paths */
	{SITAR_A_CDC_TX1_MUX_CTL, 0x8, 0x0},
};

static void sitar_codec_init_reg(struct snd_soc_codec *codec)
{
	u32 i;

	for (i = 0; i < ARRAY_SIZE(sitar_codec_reg_init_val); i++)
		snd_soc_update_bits(codec, sitar_codec_reg_init_val[i].reg,
			sitar_codec_reg_init_val[i].mask,
			sitar_codec_reg_init_val[i].val);
}

static int sitar_codec_probe(struct snd_soc_codec *codec)
{
	struct sitar *control;
	struct sitar_priv *sitar;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	int ret = 0;
	int i;
	u8 sitar_version;
	int ch_cnt;

	codec->control_data = dev_get_drvdata(codec->dev->parent);
	control = codec->control_data;

	sitar = kzalloc(sizeof(struct sitar_priv), GFP_KERNEL);
	if (!sitar) {
		dev_err(codec->dev, "Failed to allocate private data\n");
		return -ENOMEM;
	}

	/* Make sure mbhc micbias register addresses are zeroed out */
	memset(&sitar->mbhc_bias_regs, 0,
		sizeof(struct mbhc_micbias_regs));
	sitar->cfilt_k_value = 0;
	sitar->mbhc_micbias_switched = false;

	snd_soc_codec_set_drvdata(codec, sitar);

	sitar->mclk_enabled = false;
	sitar->bandgap_type = SITAR_BANDGAP_OFF;
	sitar->clock_active = false;
	sitar->config_mode_active = false;
	sitar->mbhc_polling_active = false;
	sitar->fake_insert_context = false;
	sitar->no_mic_headset_override = false;
	sitar->codec = codec;
	sitar->pdata = dev_get_platdata(codec->dev->parent);
	atomic_set(&sitar->pm_cnt, 1);
	init_waitqueue_head(&sitar->pm_wq);

	sitar_update_reg_defaults(codec);
	sitar_codec_init_reg(codec);

	ret = sitar_handle_pdata(sitar);
	if (IS_ERR_VALUE(ret)) {
		pr_err("%s: bad pdata\n", __func__);
		goto err_pdata;
	}

	snd_soc_add_controls(codec, sitar_snd_controls,
		ARRAY_SIZE(sitar_snd_controls));
	snd_soc_dapm_new_controls(dapm, sitar_dapm_widgets,
		ARRAY_SIZE(sitar_dapm_widgets));
	snd_soc_dapm_add_routes(dapm, audio_map, ARRAY_SIZE(audio_map));

	sitar_version = snd_soc_read(codec, WCD9XXX_A_CHIP_VERSION);
	pr_info("%s : Sitar version reg 0x%2x\n", __func__, (u32)sitar_version);

	sitar_version &=  0x1F;
	pr_info("%s : Sitar version %u\n", __func__, (u32)sitar_version);

	snd_soc_dapm_sync(dapm);


	ret = wcd9xxx_request_irq(codec->control_data, SITAR_IRQ_MBHC_INSERTION,
		sitar_hs_insert_irq, "Headset insert detect", sitar);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
			SITAR_IRQ_MBHC_INSERTION);
		goto err_insert_irq;
	}
	wcd9xxx_disable_irq(codec->control_data, SITAR_IRQ_MBHC_INSERTION);

	ret = wcd9xxx_request_irq(codec->control_data, SITAR_IRQ_MBHC_REMOVAL,
		sitar_hs_remove_irq, "Headset remove detect", sitar);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
			SITAR_IRQ_MBHC_REMOVAL);
		goto err_remove_irq;
	}
	wcd9xxx_disable_irq(codec->control_data, SITAR_IRQ_MBHC_REMOVAL);

	ret = wcd9xxx_request_irq(codec->control_data, SITAR_IRQ_MBHC_POTENTIAL,
		sitar_dce_handler, "DC Estimation detect", sitar);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
			SITAR_IRQ_MBHC_POTENTIAL);
		goto err_potential_irq;
	}
	wcd9xxx_disable_irq(codec->control_data, SITAR_IRQ_MBHC_POTENTIAL);

	ret = wcd9xxx_request_irq(codec->control_data, SITAR_IRQ_MBHC_RELEASE,
		sitar_release_handler, "Button Release detect", sitar);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
			SITAR_IRQ_MBHC_RELEASE);
		goto err_release_irq;
	}
	wcd9xxx_disable_irq(codec->control_data, SITAR_IRQ_MBHC_RELEASE);

	ret = wcd9xxx_request_irq(codec->control_data, SITAR_IRQ_SLIMBUS,
		sitar_slimbus_irq, "SLIMBUS Slave", sitar);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
			SITAR_IRQ_SLIMBUS);
		goto err_slimbus_irq;
	}

	for (i = 0; i < WCD9XXX_SLIM_NUM_PORT_REG; i++)
		wcd9xxx_interface_reg_write(codec->control_data,
			SITAR_SLIM_PGD_PORT_INT_EN0 + i, 0xFF);


	ret = wcd9xxx_request_irq(codec->control_data,
		SITAR_IRQ_HPH_PA_OCPL_FAULT, sitar_hphl_ocp_irq,
		"HPH_L OCP detect", sitar);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
			SITAR_IRQ_HPH_PA_OCPL_FAULT);
		goto err_hphl_ocp_irq;
	}
	wcd9xxx_disable_irq(codec->control_data, SITAR_IRQ_HPH_PA_OCPL_FAULT);

	ret = wcd9xxx_request_irq(codec->control_data,
		SITAR_IRQ_HPH_PA_OCPR_FAULT, sitar_hphr_ocp_irq,
		"HPH_R OCP detect", sitar);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
			SITAR_IRQ_HPH_PA_OCPR_FAULT);
		goto err_hphr_ocp_irq;
	}
	wcd9xxx_disable_irq(codec->control_data, SITAR_IRQ_HPH_PA_OCPR_FAULT);

	for (i = 0; i < ARRAY_SIZE(sitar_dai); i++) {
		switch (sitar_dai[i].id) {
		case AIF1_PB:
			ch_cnt = sitar_dai[i].playback.channels_max;
			break;
		case AIF1_CAP:
			ch_cnt = sitar_dai[i].capture.channels_max;
			break;
		default:
			continue;
		}
		sitar->dai[i].ch_num = kzalloc((sizeof(unsigned int)*
					ch_cnt), GFP_KERNEL);
	}

#ifdef CONFIG_DEBUG_FS
	debug_sitar_priv = sitar;
#endif

	return ret;

err_hphr_ocp_irq:
	wcd9xxx_free_irq(codec->control_data,
			SITAR_IRQ_HPH_PA_OCPL_FAULT, sitar);
err_hphl_ocp_irq:
	wcd9xxx_free_irq(codec->control_data,
			SITAR_IRQ_SLIMBUS, sitar);
err_slimbus_irq:
	wcd9xxx_free_irq(codec->control_data,
			SITAR_IRQ_MBHC_RELEASE, sitar);
err_release_irq:
	wcd9xxx_free_irq(codec->control_data,
			SITAR_IRQ_MBHC_POTENTIAL, sitar);
err_potential_irq:
	wcd9xxx_free_irq(codec->control_data,
			SITAR_IRQ_MBHC_REMOVAL, sitar);
err_remove_irq:
	wcd9xxx_free_irq(codec->control_data,
			SITAR_IRQ_MBHC_INSERTION, sitar);
err_insert_irq:
err_pdata:
	kfree(sitar);
	return ret;
}
static int sitar_codec_remove(struct snd_soc_codec *codec)
{
	int i;
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);
	wcd9xxx_free_irq(codec->control_data, SITAR_IRQ_SLIMBUS, sitar);
	wcd9xxx_free_irq(codec->control_data, SITAR_IRQ_MBHC_RELEASE, sitar);
	wcd9xxx_free_irq(codec->control_data, SITAR_IRQ_MBHC_POTENTIAL, sitar);
	wcd9xxx_free_irq(codec->control_data, SITAR_IRQ_MBHC_REMOVAL, sitar);
	wcd9xxx_free_irq(codec->control_data, SITAR_IRQ_MBHC_INSERTION, sitar);
	sitar_codec_disable_clock_block(codec);
	sitar_codec_enable_bandgap(codec, SITAR_BANDGAP_OFF);
	for (i = 0; i < ARRAY_SIZE(sitar_dai); i++)
		kfree(sitar->dai[i].ch_num);
	kfree(sitar);
	return 0;
}
static struct snd_soc_codec_driver soc_codec_dev_sitar = {
	.probe	= sitar_codec_probe,
	.remove	= sitar_codec_remove,
	.read = sitar_read,
	.write = sitar_write,

	.readable_register = sitar_readable,
	.volatile_register = sitar_volatile,

	.reg_cache_size = SITAR_CACHE_SIZE,
	.reg_cache_default = sitar_reg_defaults,
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
	debug_sitar_priv->no_mic_headset_override = (*strsep(&buf, " ") == '0')
		? false : true;

	return rc;
}

static const struct file_operations codec_debug_ops = {
	.open = codec_debug_open,
	.write = codec_debug_write,
};
#endif

#ifdef CONFIG_PM
static int sitar_suspend(struct device *dev)
{
	dev_dbg(dev, "%s: system suspend\n", __func__);
	return 0;
}

static int sitar_resume(struct device *dev)
{
	dev_dbg(dev, "%s: system resume\n", __func__);
	return 0;
}

static const struct dev_pm_ops sitar_pm_ops = {
	.suspend	= sitar_suspend,
	.resume		= sitar_resume,
};
#endif

static int __devinit sitar_probe(struct platform_device *pdev)
{
	int ret = 0;
	pr_err("%s\n", __func__);
#ifdef CONFIG_DEBUG_FS
	debugfs_poke = debugfs_create_file("TRRS",
		S_IFREG | S_IRUGO, NULL, (void *) "TRRS", &codec_debug_ops);

#endif
	ret = snd_soc_register_codec(&pdev->dev, &soc_codec_dev_sitar,
			sitar_dai, ARRAY_SIZE(sitar_dai));
	return ret;
}
static int __devexit sitar_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);

#ifdef CONFIG_DEBUG_FS
	debugfs_remove(debugfs_poke);
#endif
	return 0;
}
static struct platform_driver sitar_codec_driver = {
	.probe = sitar_probe,
	.remove = sitar_remove,
	.driver = {
		.name = "sitar_codec",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &sitar_pm_ops,
#endif
	},
};

static int __init sitar_codec_init(void)
{
	return platform_driver_register(&sitar_codec_driver);
}

static void __exit sitar_codec_exit(void)
{
	platform_driver_unregister(&sitar_codec_driver);
}

module_init(sitar_codec_init);
module_exit(sitar_codec_exit);

MODULE_DESCRIPTION("Sitar codec driver");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL v2");
