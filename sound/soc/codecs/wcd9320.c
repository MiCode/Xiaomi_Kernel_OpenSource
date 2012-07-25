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
#include <linux/mfd/wcd9xxx/wcd9320_registers.h>
#include <linux/mfd/wcd9xxx/pdata.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/pm_runtime.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include "wcd9320.h"

#define WCD9320_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |\
			SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |\
			SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_192000)


#define NUM_DECIMATORS 10
#define NUM_INTERPOLATORS 7
#define BITS_PER_REG 8
#define TAIKO_CFILT_FAST_MODE 0x00
#define TAIKO_CFILT_SLOW_MODE 0x40
#define MBHC_FW_READ_ATTEMPTS 15
#define MBHC_FW_READ_TIMEOUT 2000000

enum {
	MBHC_USE_HPHL_TRIGGER = 1,
	MBHC_USE_MB_TRIGGER = 2
};

#define MBHC_NUM_DCE_PLUG_DETECT 3
#define NUM_ATTEMPTS_INSERT_DETECT 25
#define NUM_ATTEMPTS_TO_REPORT 5

#define TAIKO_JACK_MASK (SND_JACK_HEADSET | SND_JACK_OC_HPHL | \
			 SND_JACK_OC_HPHR | SND_JACK_UNSUPPORTED)

#define TAIKO_I2S_MASTER_MODE_MASK 0x08

#define TAIKO_OCP_ATTEMPT 1

#define AIF1_PB 1
#define AIF1_CAP 2
#define AIF2_PB 3
#define AIF2_CAP 4
#define AIF3_CAP 5
#define AIF3_PB  6

#define NUM_CODEC_DAIS 6
#define TAIKO_COMP_DIGITAL_GAIN_OFFSET 3

struct taiko_codec_dai_data {
	u32 rate;
	u32 *ch_num;
	u32 ch_act;
	u32 ch_tot;
};

#define TAIKO_MCLK_RATE_12288KHZ 12288000
#define TAIKO_MCLK_RATE_9600KHZ 9600000

#define TAIKO_FAKE_INS_THRESHOLD_MS 2500
#define TAIKO_FAKE_REMOVAL_MIN_PERIOD_MS 50

#define TAIKO_MBHC_BUTTON_MIN 0x8000

#define TAIKO_MBHC_FAKE_INSERT_LOW 10
#define TAIKO_MBHC_FAKE_INSERT_HIGH 80
#define TAIKO_MBHC_FAKE_INS_HIGH_NO_GPIO 150

#define TAIKO_MBHC_STATUS_REL_DETECTION 0x0C

#define TAIKO_MBHC_GPIO_REL_DEBOUNCE_TIME_MS 200

#define TAIKO_MBHC_FAKE_INS_DELTA_MV 200
#define TAIKO_MBHC_FAKE_INS_DELTA_SCALED_MV 300

#define TAIKO_HS_DETECT_PLUG_TIME_MS (5 * 1000)
#define TAIKO_HS_DETECT_PLUG_INERVAL_MS 100

#define TAIKO_GPIO_IRQ_DEBOUNCE_TIME_US 5000

#define TAIKO_MBHC_GND_MIC_SWAP_THRESHOLD 2

#define TAIKO_ACQUIRE_LOCK(x) do { mutex_lock(&x); } while (0)
#define TAIKO_RELEASE_LOCK(x) do { mutex_unlock(&x); } while (0)

static const DECLARE_TLV_DB_SCALE(digital_gain, 0, 1, 0);
static const DECLARE_TLV_DB_SCALE(line_gain, 0, 7, 1);
static const DECLARE_TLV_DB_SCALE(analog_gain, 0, 25, 1);
static struct snd_soc_dai_driver taiko_dai[];
static const DECLARE_TLV_DB_SCALE(aux_pga_gain, 0, 2, 0);

enum taiko_bandgap_type {
	TAIKO_BANDGAP_OFF = 0,
	TAIKO_BANDGAP_AUDIO_MODE,
	TAIKO_BANDGAP_MBHC_MODE,
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

enum {
	COMPANDER_1 = 0,
	COMPANDER_2,
	COMPANDER_MAX,
};

enum {
	COMPANDER_FS_8KHZ = 0,
	COMPANDER_FS_16KHZ,
	COMPANDER_FS_32KHZ,
	COMPANDER_FS_48KHZ,
	COMPANDER_FS_96KHZ,
	COMPANDER_FS_192KHZ,
	COMPANDER_FS_MAX,
};

/* Flags to track of PA and DAC state.
 * PA and DAC should be tracked separately as AUXPGA loopback requires
 * only PA to be turned on without DAC being on. */
enum taiko_priv_ack_flags {
	TAIKO_HPHL_PA_OFF_ACK = 0,
	TAIKO_HPHR_PA_OFF_ACK,
	TAIKO_HPHL_DAC_OFF_ACK,
	TAIKO_HPHR_DAC_OFF_ACK
};


struct comp_sample_dependent_params {
	u32 peak_det_timeout;
	u32 rms_meter_div_fact;
	u32 rms_meter_resamp_fact;
};

/* Data used by MBHC */
struct mbhc_internal_cal_data {
	u16 dce_z;
	u16 dce_mb;
	u16 sta_z;
	u16 sta_mb;
	u32 t_sta_dce;
	u32 t_dce;
	u32 t_sta;
	u32 micb_mv;
	u16 v_ins_hu;
	u16 v_ins_h;
	u16 v_b1_hu;
	u16 v_b1_h;
	u16 v_b1_huc;
	u16 v_brh;
	u16 v_brl;
	u16 v_no_mic;
	u8 npoll;
	u8 nbounce_wait;
	s16 adj_v_hs_max;
	u16 adj_v_ins_hu;
	u16 adj_v_ins_h;
	s16 v_inval_ins_low;
	s16 v_inval_ins_high;
};

struct taiko_reg_address {
	u16 micb_4_ctl;
	u16 micb_4_int_rbias;
	u16 micb_4_mbhc;
};

enum taiko_mbhc_plug_type {
	PLUG_TYPE_INVALID = -1,
	PLUG_TYPE_NONE,
	PLUG_TYPE_HEADSET,
	PLUG_TYPE_HEADPHONE,
	PLUG_TYPE_HIGH_HPH,
	PLUG_TYPE_GND_MIC_SWAP,
};

enum taiko_mbhc_state {
	MBHC_STATE_NONE = -1,
	MBHC_STATE_POTENTIAL,
	MBHC_STATE_POTENTIAL_RECOVERY,
	MBHC_STATE_RELEASE,
};

struct hpf_work {
	struct taiko_priv *taiko;
	u32 decimator;
	u8 tx_hpf_cut_of_freq;
	struct delayed_work dwork;
};

static struct hpf_work tx_hpf_work[NUM_DECIMATORS];

struct taiko_priv {
	struct snd_soc_codec *codec;
	struct taiko_reg_address reg_addr;
	u32 adc_count;
	u32 cfilt1_cnt;
	u32 cfilt2_cnt;
	u32 cfilt3_cnt;
	u32 rx_bias_count;
	s32 dmic_1_2_clk_cnt;
	s32 dmic_3_4_clk_cnt;
	s32 dmic_5_6_clk_cnt;

	enum taiko_bandgap_type bandgap_type;
	bool mclk_enabled;
	bool clock_active;
	bool config_mode_active;
	bool mbhc_polling_active;
	unsigned long mbhc_fake_ins_start;
	int buttons_pressed;
	enum taiko_mbhc_state mbhc_state;
	struct taiko_mbhc_config mbhc_cfg;
	struct mbhc_internal_cal_data mbhc_data;

	struct wcd9xxx_pdata *pdata;
	u32 anc_slot;

	bool no_mic_headset_override;
	/* Delayed work to report long button press */
	struct delayed_work mbhc_btn_dwork;

	struct mbhc_micbias_regs mbhc_bias_regs;
	bool mbhc_micbias_switched;

	/* track PA/DAC state */
	unsigned long hph_pa_dac_state;

	/*track taiko interface type*/
	u8 intf_type;

	u32 hph_status; /* track headhpone status */
	/* define separate work for left and right headphone OCP to avoid
	 * additional checking on which OCP event to report so no locking
	 * to ensure synchronization is required
	 */
	struct work_struct hphlocp_work; /* reporting left hph ocp off */
	struct work_struct hphrocp_work; /* reporting right hph ocp off */

	u8 hphlocp_cnt; /* headphone left ocp retry */
	u8 hphrocp_cnt; /* headphone right ocp retry */

	/* Work to perform MBHC Firmware Read */
	struct delayed_work mbhc_firmware_dwork;
	const struct firmware *mbhc_fw;

	/* num of slim ports required */
	struct taiko_codec_dai_data dai[NUM_CODEC_DAIS];

	/*compander*/
	int comp_enabled[COMPANDER_MAX];
	u32 comp_fs[COMPANDER_MAX];

	/* Maintain the status of AUX PGA */
	int aux_pga_cnt;
	u8 aux_l_gain;
	u8 aux_r_gain;

	struct delayed_work mbhc_insert_dwork;
	unsigned long mbhc_last_resume; /* in jiffies */

	u8 current_plug;
	struct work_struct hs_correct_plug_work;
	bool hs_detect_work_stop;
	bool hs_polling_irq_prepared;
	bool lpi_enabled; /* low power insertion detection */
	bool in_gpio_handler;
	/* Currently, only used for mbhc purpose, to protect
	 * concurrent execution of mbhc threaded irq handlers and
	 * kill race between DAPM and MBHC.But can serve as a
	 * general lock to protect codec resource
	 */
	struct mutex codec_resource_lock;

#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs_poke;
	struct dentry *debugfs_mbhc;
#endif
};


static const u32 comp_shift[] = {
	0,
	2,
};

static const int comp_rx_path[] = {
	COMPANDER_1,
	COMPANDER_1,
	COMPANDER_2,
	COMPANDER_2,
	COMPANDER_2,
	COMPANDER_2,
	COMPANDER_MAX,
};

static const struct comp_sample_dependent_params comp_samp_params[] = {
	{
		.peak_det_timeout = 0x2,
		.rms_meter_div_fact = 0x8 << 4,
		.rms_meter_resamp_fact = 0x21,
	},
	{
		.peak_det_timeout = 0x3,
		.rms_meter_div_fact = 0x9 << 4,
		.rms_meter_resamp_fact = 0x28,
	},

	{
		.peak_det_timeout = 0x5,
		.rms_meter_div_fact = 0xB << 4,
		.rms_meter_resamp_fact = 0x28,
	},

	{
		.peak_det_timeout = 0x5,
		.rms_meter_div_fact = 0xB << 4,
		.rms_meter_resamp_fact = 0x28,
	},
};

static unsigned short rx_digital_gain_reg[] = {
	TAIKO_A_CDC_RX1_VOL_CTL_B2_CTL,
	TAIKO_A_CDC_RX2_VOL_CTL_B2_CTL,
	TAIKO_A_CDC_RX3_VOL_CTL_B2_CTL,
	TAIKO_A_CDC_RX4_VOL_CTL_B2_CTL,
	TAIKO_A_CDC_RX5_VOL_CTL_B2_CTL,
	TAIKO_A_CDC_RX6_VOL_CTL_B2_CTL,
	TAIKO_A_CDC_RX7_VOL_CTL_B2_CTL,
};


static unsigned short tx_digital_gain_reg[] = {
	TAIKO_A_CDC_TX1_VOL_CTL_GAIN,
	TAIKO_A_CDC_TX2_VOL_CTL_GAIN,
	TAIKO_A_CDC_TX3_VOL_CTL_GAIN,
	TAIKO_A_CDC_TX4_VOL_CTL_GAIN,
	TAIKO_A_CDC_TX5_VOL_CTL_GAIN,
	TAIKO_A_CDC_TX6_VOL_CTL_GAIN,
	TAIKO_A_CDC_TX7_VOL_CTL_GAIN,
	TAIKO_A_CDC_TX8_VOL_CTL_GAIN,
	TAIKO_A_CDC_TX9_VOL_CTL_GAIN,
	TAIKO_A_CDC_TX10_VOL_CTL_GAIN,
};

static int taiko_codec_enable_class_h_clk(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	pr_debug("%s %s  %d\n", __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, TAIKO_A_CDC_CLSH_B1_CTL, 0x01, 0x01);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, TAIKO_A_BUCK_MODE_1, 0x80, 0x00);
		snd_soc_update_bits(codec, TAIKO_A_CDC_CLSH_B1_CTL, 0x01, 0x00);
		break;
	}
	return 0;
}

static int taiko_codec_enable_class_h(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	pr_debug("%s %s  %d\n", __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, TAIKO_A_BUCK_MODE_5, 0x02, 0x02);
		snd_soc_update_bits(codec, TAIKO_A_BUCK_MODE_4, 0xFF, 0xFF);
		snd_soc_update_bits(codec, TAIKO_A_BUCK_MODE_1, 0x04, 0x04);
		snd_soc_update_bits(codec, TAIKO_A_BUCK_MODE_3, 0x04, 0x00);
		snd_soc_update_bits(codec, TAIKO_A_BUCK_MODE_3, 0x08, 0x00);
		snd_soc_update_bits(codec, TAIKO_A_BUCK_MODE_1, 0x80, 0x80);
		usleep_range(1000, 1000);
		break;
	}
	return 0;
}

static int taiko_codec_enable_charge_pump(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	pr_debug("%s %s %d\n", __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		usleep_range(1000, 1000);
		break;
	}
	return 0;
}


static int taiko_get_anc_slot(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct taiko_priv *taiko = snd_soc_codec_get_drvdata(codec);
	ucontrol->value.integer.value[0] = taiko->anc_slot;
	return 0;
}

static int taiko_put_anc_slot(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct taiko_priv *taiko = snd_soc_codec_get_drvdata(codec);
	taiko->anc_slot = ucontrol->value.integer.value[0];
	return 0;
}

static int taiko_pa_gain_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	u8 ear_pa_gain;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	ear_pa_gain = snd_soc_read(codec, TAIKO_A_RX_EAR_GAIN);

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

static int taiko_pa_gain_put(struct snd_kcontrol *kcontrol,
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

	snd_soc_update_bits(codec, TAIKO_A_RX_EAR_GAIN, 0xE0, ear_pa_gain);
	return 0;
}

static int taiko_get_iir_enable_audio_mixer(
					struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	int iir_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int band_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;

	ucontrol->value.integer.value[0] =
		snd_soc_read(codec, (TAIKO_A_CDC_IIR1_CTL + 16 * iir_idx)) &
		(1 << band_idx);

	pr_debug("%s: IIR #%d band #%d enable %d\n", __func__,
		iir_idx, band_idx,
		(uint32_t)ucontrol->value.integer.value[0]);
	return 0;
}

static int taiko_put_iir_enable_audio_mixer(
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
	snd_soc_update_bits(codec, (TAIKO_A_CDC_IIR1_CTL + 16 * iir_idx),
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
	snd_soc_write(codec,
		(TAIKO_A_CDC_IIR1_COEF_B1_CTL + 16 * iir_idx),
		(band_idx * BAND_MAX + coeff_idx) & 0x1F);

	/* Mask bits top 2 bits since they are reserved */
	return ((snd_soc_read(codec,
		(TAIKO_A_CDC_IIR1_COEF_B2_CTL + 16 * iir_idx)) << 24)) &
		0x3FFFFFFF;
}

static int taiko_get_iir_band_audio_mixer(
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
	snd_soc_write(codec,
		(TAIKO_A_CDC_IIR1_COEF_B1_CTL + 16 * iir_idx),
		(band_idx * BAND_MAX + coeff_idx) & 0x1F);

	/* Mask top 2 bits, 7-8 are reserved */
	snd_soc_write(codec,
		(TAIKO_A_CDC_IIR1_COEF_B2_CTL + 16 * iir_idx),
		(value >> 24) & 0x3F);

}

static int taiko_put_iir_band_audio_mixer(
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

static int taiko_compander_gain_offset(
	struct snd_soc_codec *codec, u32 enable,
	unsigned int reg, int mask,	int event)
{
	int pa_mode = snd_soc_read(codec, reg) & mask;
	int gain_offset = 0;
	/*  if PMU && enable is 1-> offset is 3
	 *  if PMU && enable is 0-> offset is 0
	 *  if PMD && pa_mode is PA -> offset is 0: PMU compander is off
	 *  if PMD && pa_mode is comp -> offset is -3: PMU compander is on.
	 */

	if (SND_SOC_DAPM_EVENT_ON(event) && (enable != 0))
		gain_offset = TAIKO_COMP_DIGITAL_GAIN_OFFSET;
	if (SND_SOC_DAPM_EVENT_OFF(event) && (pa_mode == 0))
		gain_offset = -TAIKO_COMP_DIGITAL_GAIN_OFFSET;
	return gain_offset;
}


static int taiko_config_gain_compander(
				struct snd_soc_codec *codec,
				u32 compander, u32 enable, int event)
{
	int value = 0;
	int mask = 1 << 4;
	int gain = 0;
	int gain_offset;
	if (compander >= COMPANDER_MAX) {
		pr_err("%s: Error, invalid compander channel\n", __func__);
		return -EINVAL;
	}

	if ((enable == 0) || SND_SOC_DAPM_EVENT_OFF(event))
		value = 1 << 4;

	if (compander == COMPANDER_1) {
		gain_offset = taiko_compander_gain_offset(codec, enable,
				TAIKO_A_RX_HPH_L_GAIN, mask, event);
		snd_soc_update_bits(codec, TAIKO_A_RX_HPH_L_GAIN, mask, value);
		gain = snd_soc_read(codec, TAIKO_A_CDC_RX1_VOL_CTL_B2_CTL);
		snd_soc_update_bits(codec, TAIKO_A_CDC_RX1_VOL_CTL_B2_CTL,
				0xFF, gain - gain_offset);
		gain_offset = taiko_compander_gain_offset(codec, enable,
				TAIKO_A_RX_HPH_R_GAIN, mask, event);
		snd_soc_update_bits(codec, TAIKO_A_RX_HPH_R_GAIN, mask, value);
		gain = snd_soc_read(codec, TAIKO_A_CDC_RX2_VOL_CTL_B2_CTL);
		snd_soc_update_bits(codec, TAIKO_A_CDC_RX2_VOL_CTL_B2_CTL,
				0xFF, gain - gain_offset);
	} else if (compander == COMPANDER_2) {
		gain_offset = taiko_compander_gain_offset(codec, enable,
				TAIKO_A_RX_LINE_1_GAIN, mask, event);
		snd_soc_update_bits(codec, TAIKO_A_RX_LINE_1_GAIN, mask, value);
		gain = snd_soc_read(codec, TAIKO_A_CDC_RX3_VOL_CTL_B2_CTL);
		snd_soc_update_bits(codec, TAIKO_A_CDC_RX3_VOL_CTL_B2_CTL,
				0xFF, gain - gain_offset);
		gain_offset = taiko_compander_gain_offset(codec, enable,
				TAIKO_A_RX_LINE_3_GAIN, mask, event);
		snd_soc_update_bits(codec, TAIKO_A_RX_LINE_3_GAIN, mask, value);
		gain = snd_soc_read(codec, TAIKO_A_CDC_RX4_VOL_CTL_B2_CTL);
		snd_soc_update_bits(codec, TAIKO_A_CDC_RX4_VOL_CTL_B2_CTL,
				0xFF, gain - gain_offset);
		gain_offset = taiko_compander_gain_offset(codec, enable,
				TAIKO_A_RX_LINE_2_GAIN, mask, event);
		snd_soc_update_bits(codec, TAIKO_A_RX_LINE_2_GAIN, mask, value);
		gain = snd_soc_read(codec, TAIKO_A_CDC_RX5_VOL_CTL_B2_CTL);
		snd_soc_update_bits(codec, TAIKO_A_CDC_RX5_VOL_CTL_B2_CTL,
				0xFF, gain - gain_offset);
		gain_offset = taiko_compander_gain_offset(codec, enable,
				TAIKO_A_RX_LINE_4_GAIN, mask, event);
		snd_soc_update_bits(codec, TAIKO_A_RX_LINE_4_GAIN, mask, value);
		gain = snd_soc_read(codec, TAIKO_A_CDC_RX6_VOL_CTL_B2_CTL);
		snd_soc_update_bits(codec, TAIKO_A_CDC_RX6_VOL_CTL_B2_CTL,
				0xFF, gain - gain_offset);
	}
	return 0;
}
static int taiko_get_compander(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{

	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	int comp = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->max;
	struct taiko_priv *taiko = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = taiko->comp_enabled[comp];

	return 0;
}

static int taiko_set_compander(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct taiko_priv *taiko = snd_soc_codec_get_drvdata(codec);
	int comp = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->max;
	int value = ucontrol->value.integer.value[0];

	if (value == taiko->comp_enabled[comp]) {
		pr_debug("%s: compander #%d enable %d no change\n",
			    __func__, comp, value);
		return 0;
	}
	taiko->comp_enabled[comp] = value;
	return 0;
}


static int taiko_config_compander(struct snd_soc_dapm_widget *w,
						  struct snd_kcontrol *kcontrol,
						  int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct taiko_priv *taiko = snd_soc_codec_get_drvdata(codec);
	u32 rate = taiko->comp_fs[w->shift];

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (taiko->comp_enabled[w->shift] != 0) {
			/* Enable both L/R compander clocks */
			snd_soc_update_bits(codec,
					TAIKO_A_CDC_CLK_RX_B2_CTL,
					0x03 << comp_shift[w->shift],
					0x03 << comp_shift[w->shift]);
			/* Clar the HALT for the compander*/
			snd_soc_update_bits(codec,
					TAIKO_A_CDC_COMP1_B1_CTL +
					w->shift * 8, 1 << 2, 0);
			/* Toggle compander reset bits*/
			snd_soc_update_bits(codec,
					TAIKO_A_CDC_CLK_OTHR_RESET_B2_CTL,
					0x03 << comp_shift[w->shift],
					0x03 << comp_shift[w->shift]);
			snd_soc_update_bits(codec,
					TAIKO_A_CDC_CLK_OTHR_RESET_B2_CTL,
					0x03 << comp_shift[w->shift], 0);
			taiko_config_gain_compander(codec, w->shift, 1, event);
			/* Update the RMS meter resampling*/
			snd_soc_update_bits(codec,
					TAIKO_A_CDC_COMP1_B3_CTL +
					w->shift * 8, 0xFF, 0x01);
			/* Wait for 1ms*/
			usleep_range(1000, 1000);
		}
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* Set sample rate dependent paramater*/
		if (taiko->comp_enabled[w->shift] != 0) {
			snd_soc_update_bits(codec, TAIKO_A_CDC_COMP1_FS_CFG +
			w->shift * 8, 0x03,	rate);
			snd_soc_update_bits(codec, TAIKO_A_CDC_COMP1_B2_CTL +
			w->shift * 8, 0x0F,
			comp_samp_params[rate].peak_det_timeout);
			snd_soc_update_bits(codec, TAIKO_A_CDC_COMP1_B2_CTL +
			w->shift * 8, 0xF0,
			comp_samp_params[rate].rms_meter_div_fact);
			snd_soc_update_bits(codec, TAIKO_A_CDC_COMP1_B3_CTL +
			w->shift * 8, 0xFF,
			comp_samp_params[rate].rms_meter_resamp_fact);
			/* Compander enable -> 0x370/0x378*/
			snd_soc_update_bits(codec, TAIKO_A_CDC_COMP1_B1_CTL +
			w->shift * 8, 0x03, 0x03);
		}
		break;
	case SND_SOC_DAPM_PRE_PMD:
		/* Halt the compander*/
		snd_soc_update_bits(codec, TAIKO_A_CDC_COMP1_B1_CTL +
			w->shift * 8, 1 << 2, 1 << 2);
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* Restore the gain */
		taiko_config_gain_compander(codec, w->shift,
				taiko->comp_enabled[w->shift], event);
		/* Disable the compander*/
		snd_soc_update_bits(codec, TAIKO_A_CDC_COMP1_B1_CTL +
			w->shift * 8, 0x03, 0x00);
		/* Turn off the clock for compander in pair*/
		snd_soc_update_bits(codec, TAIKO_A_CDC_CLK_RX_B2_CTL,
			0x03 << comp_shift[w->shift], 0);
		break;
	}
	return 0;
}

static const char * const taiko_ear_pa_gain_text[] = {"POS_6_DB", "POS_2_DB"};
static const struct soc_enum taiko_ear_pa_gain_enum[] = {
		SOC_ENUM_SINGLE_EXT(2, taiko_ear_pa_gain_text),
};

/*cut of frequency for high pass filter*/
static const char * const cf_text[] = {
	"MIN_3DB_4Hz", "MIN_3DB_75Hz", "MIN_3DB_150Hz"
};

static const struct soc_enum cf_dec1_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_TX1_MUX_CTL, 4, 3, cf_text);

static const struct soc_enum cf_dec2_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_TX2_MUX_CTL, 4, 3, cf_text);

static const struct soc_enum cf_dec3_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_TX3_MUX_CTL, 4, 3, cf_text);

static const struct soc_enum cf_dec4_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_TX4_MUX_CTL, 4, 3, cf_text);

static const struct soc_enum cf_dec5_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_TX5_MUX_CTL, 4, 3, cf_text);

static const struct soc_enum cf_dec6_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_TX6_MUX_CTL, 4, 3, cf_text);

static const struct soc_enum cf_dec7_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_TX7_MUX_CTL, 4, 3, cf_text);

static const struct soc_enum cf_dec8_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_TX8_MUX_CTL, 4, 3, cf_text);

static const struct soc_enum cf_dec9_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_TX9_MUX_CTL, 4, 3, cf_text);

static const struct soc_enum cf_dec10_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_TX10_MUX_CTL, 4, 3, cf_text);

static const struct soc_enum cf_rxmix1_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_RX1_B4_CTL, 1, 3, cf_text);

static const struct soc_enum cf_rxmix2_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_RX2_B4_CTL, 1, 3, cf_text);

static const struct soc_enum cf_rxmix3_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_RX3_B4_CTL, 1, 3, cf_text);

static const struct soc_enum cf_rxmix4_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_RX4_B4_CTL, 1, 3, cf_text);

static const struct soc_enum cf_rxmix5_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_RX5_B4_CTL, 1, 3, cf_text)
;
static const struct soc_enum cf_rxmix6_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_RX6_B4_CTL, 1, 3, cf_text);

static const struct soc_enum cf_rxmix7_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_RX7_B4_CTL, 1, 3, cf_text);

static const struct snd_kcontrol_new taiko_snd_controls[] = {

	SOC_ENUM_EXT("EAR PA Gain", taiko_ear_pa_gain_enum[0],
		taiko_pa_gain_get, taiko_pa_gain_put),

	SOC_SINGLE_TLV("LINEOUT1 Volume", TAIKO_A_RX_LINE_1_GAIN, 0, 12, 1,
		line_gain),
	SOC_SINGLE_TLV("LINEOUT2 Volume", TAIKO_A_RX_LINE_2_GAIN, 0, 12, 1,
		line_gain),
	SOC_SINGLE_TLV("LINEOUT3 Volume", TAIKO_A_RX_LINE_3_GAIN, 0, 12, 1,
		line_gain),
	SOC_SINGLE_TLV("LINEOUT4 Volume", TAIKO_A_RX_LINE_4_GAIN, 0, 12, 1,
		line_gain),

	SOC_SINGLE_TLV("HPHL Volume", TAIKO_A_RX_HPH_L_GAIN, 0, 12, 1,
		line_gain),
	SOC_SINGLE_TLV("HPHR Volume", TAIKO_A_RX_HPH_R_GAIN, 0, 12, 1,
		line_gain),

	SOC_SINGLE_S8_TLV("RX1 Digital Volume", TAIKO_A_CDC_RX1_VOL_CTL_B2_CTL,
		-84, 40, digital_gain),
	SOC_SINGLE_S8_TLV("RX2 Digital Volume", TAIKO_A_CDC_RX2_VOL_CTL_B2_CTL,
		-84, 40, digital_gain),
	SOC_SINGLE_S8_TLV("RX3 Digital Volume", TAIKO_A_CDC_RX3_VOL_CTL_B2_CTL,
		-84, 40, digital_gain),
	SOC_SINGLE_S8_TLV("RX4 Digital Volume", TAIKO_A_CDC_RX4_VOL_CTL_B2_CTL,
		-84, 40, digital_gain),
	SOC_SINGLE_S8_TLV("RX5 Digital Volume", TAIKO_A_CDC_RX5_VOL_CTL_B2_CTL,
		-84, 40, digital_gain),
	SOC_SINGLE_S8_TLV("RX6 Digital Volume", TAIKO_A_CDC_RX6_VOL_CTL_B2_CTL,
		-84, 40, digital_gain),
	SOC_SINGLE_S8_TLV("RX7 Digital Volume", TAIKO_A_CDC_RX7_VOL_CTL_B2_CTL,
		-84, 40, digital_gain),

	SOC_SINGLE_S8_TLV("DEC1 Volume", TAIKO_A_CDC_TX1_VOL_CTL_GAIN, -84, 40,
		digital_gain),
	SOC_SINGLE_S8_TLV("DEC2 Volume", TAIKO_A_CDC_TX2_VOL_CTL_GAIN, -84, 40,
		digital_gain),
	SOC_SINGLE_S8_TLV("DEC3 Volume", TAIKO_A_CDC_TX3_VOL_CTL_GAIN, -84, 40,
		digital_gain),
	SOC_SINGLE_S8_TLV("DEC4 Volume", TAIKO_A_CDC_TX4_VOL_CTL_GAIN, -84, 40,
		digital_gain),
	SOC_SINGLE_S8_TLV("DEC5 Volume", TAIKO_A_CDC_TX5_VOL_CTL_GAIN, -84, 40,
		digital_gain),
	SOC_SINGLE_S8_TLV("DEC6 Volume", TAIKO_A_CDC_TX6_VOL_CTL_GAIN, -84, 40,
		digital_gain),
	SOC_SINGLE_S8_TLV("DEC7 Volume", TAIKO_A_CDC_TX7_VOL_CTL_GAIN, -84, 40,
		digital_gain),
	SOC_SINGLE_S8_TLV("DEC8 Volume", TAIKO_A_CDC_TX8_VOL_CTL_GAIN, -84, 40,
		digital_gain),
	SOC_SINGLE_S8_TLV("DEC9 Volume", TAIKO_A_CDC_TX9_VOL_CTL_GAIN, -84, 40,
		digital_gain),
	SOC_SINGLE_S8_TLV("DEC10 Volume", TAIKO_A_CDC_TX10_VOL_CTL_GAIN, -84,
		40, digital_gain),
	SOC_SINGLE_S8_TLV("IIR1 INP1 Volume", TAIKO_A_CDC_IIR1_GAIN_B1_CTL, -84,
		40, digital_gain),
	SOC_SINGLE_S8_TLV("IIR1 INP2 Volume", TAIKO_A_CDC_IIR1_GAIN_B2_CTL, -84,
		40, digital_gain),
	SOC_SINGLE_S8_TLV("IIR1 INP3 Volume", TAIKO_A_CDC_IIR1_GAIN_B3_CTL, -84,
		40, digital_gain),
	SOC_SINGLE_S8_TLV("IIR1 INP4 Volume", TAIKO_A_CDC_IIR1_GAIN_B4_CTL, -84,
		40, digital_gain),
	SOC_SINGLE_TLV("ADC1 Volume", TAIKO_A_TX_1_2_EN, 5, 3, 0, analog_gain),
	SOC_SINGLE_TLV("ADC2 Volume", TAIKO_A_TX_1_2_EN, 1, 3, 0, analog_gain),
	SOC_SINGLE_TLV("ADC3 Volume", TAIKO_A_TX_3_4_EN, 5, 3, 0, analog_gain),
	SOC_SINGLE_TLV("ADC4 Volume", TAIKO_A_TX_3_4_EN, 1, 3, 0, analog_gain),
	SOC_SINGLE_TLV("ADC5 Volume", TAIKO_A_TX_5_6_EN, 5, 3, 0, analog_gain),
	SOC_SINGLE_TLV("ADC6 Volume", TAIKO_A_TX_5_6_EN, 1, 3, 0, analog_gain),


	SOC_SINGLE("MICBIAS1 CAPLESS Switch", TAIKO_A_MICB_1_CTL, 4, 1, 1),
	SOC_SINGLE("MICBIAS2 CAPLESS Switch", TAIKO_A_MICB_2_CTL, 4, 1, 1),
	SOC_SINGLE("MICBIAS3 CAPLESS Switch", TAIKO_A_MICB_3_CTL, 4, 1, 1),
	SOC_SINGLE("MICBIAS4 CAPLESS Switch", TAIKO_A_MICB_4_CTL, 4, 1, 1),

	SOC_SINGLE_EXT("ANC Slot", SND_SOC_NOPM, 0, 0, 100, taiko_get_anc_slot,
		taiko_put_anc_slot),
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

	SOC_SINGLE("TX1 HPF Switch", TAIKO_A_CDC_TX1_MUX_CTL, 3, 1, 0),
	SOC_SINGLE("TX2 HPF Switch", TAIKO_A_CDC_TX2_MUX_CTL, 3, 1, 0),
	SOC_SINGLE("TX3 HPF Switch", TAIKO_A_CDC_TX3_MUX_CTL, 3, 1, 0),
	SOC_SINGLE("TX4 HPF Switch", TAIKO_A_CDC_TX4_MUX_CTL, 3, 1, 0),
	SOC_SINGLE("TX5 HPF Switch", TAIKO_A_CDC_TX5_MUX_CTL, 3, 1, 0),
	SOC_SINGLE("TX6 HPF Switch", TAIKO_A_CDC_TX6_MUX_CTL, 3, 1, 0),
	SOC_SINGLE("TX7 HPF Switch", TAIKO_A_CDC_TX7_MUX_CTL, 3, 1, 0),
	SOC_SINGLE("TX8 HPF Switch", TAIKO_A_CDC_TX8_MUX_CTL, 3, 1, 0),
	SOC_SINGLE("TX9 HPF Switch", TAIKO_A_CDC_TX9_MUX_CTL, 3, 1, 0),
	SOC_SINGLE("TX10 HPF Switch", TAIKO_A_CDC_TX10_MUX_CTL, 3, 1, 0),

	SOC_SINGLE("RX1 HPF Switch", TAIKO_A_CDC_RX1_B5_CTL, 2, 1, 0),
	SOC_SINGLE("RX2 HPF Switch", TAIKO_A_CDC_RX2_B5_CTL, 2, 1, 0),
	SOC_SINGLE("RX3 HPF Switch", TAIKO_A_CDC_RX3_B5_CTL, 2, 1, 0),
	SOC_SINGLE("RX4 HPF Switch", TAIKO_A_CDC_RX4_B5_CTL, 2, 1, 0),
	SOC_SINGLE("RX5 HPF Switch", TAIKO_A_CDC_RX5_B5_CTL, 2, 1, 0),
	SOC_SINGLE("RX6 HPF Switch", TAIKO_A_CDC_RX6_B5_CTL, 2, 1, 0),
	SOC_SINGLE("RX7 HPF Switch", TAIKO_A_CDC_RX7_B5_CTL, 2, 1, 0),

	SOC_ENUM("RX1 HPF cut off", cf_rxmix1_enum),
	SOC_ENUM("RX2 HPF cut off", cf_rxmix2_enum),
	SOC_ENUM("RX3 HPF cut off", cf_rxmix3_enum),
	SOC_ENUM("RX4 HPF cut off", cf_rxmix4_enum),
	SOC_ENUM("RX5 HPF cut off", cf_rxmix5_enum),
	SOC_ENUM("RX6 HPF cut off", cf_rxmix6_enum),
	SOC_ENUM("RX7 HPF cut off", cf_rxmix7_enum),

	SOC_SINGLE_EXT("IIR1 Enable Band1", IIR1, BAND1, 1, 0,
	taiko_get_iir_enable_audio_mixer, taiko_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR1 Enable Band2", IIR1, BAND2, 1, 0,
	taiko_get_iir_enable_audio_mixer, taiko_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR1 Enable Band3", IIR1, BAND3, 1, 0,
	taiko_get_iir_enable_audio_mixer, taiko_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR1 Enable Band4", IIR1, BAND4, 1, 0,
	taiko_get_iir_enable_audio_mixer, taiko_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR1 Enable Band5", IIR1, BAND5, 1, 0,
	taiko_get_iir_enable_audio_mixer, taiko_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR2 Enable Band1", IIR2, BAND1, 1, 0,
	taiko_get_iir_enable_audio_mixer, taiko_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR2 Enable Band2", IIR2, BAND2, 1, 0,
	taiko_get_iir_enable_audio_mixer, taiko_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR2 Enable Band3", IIR2, BAND3, 1, 0,
	taiko_get_iir_enable_audio_mixer, taiko_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR2 Enable Band4", IIR2, BAND4, 1, 0,
	taiko_get_iir_enable_audio_mixer, taiko_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR2 Enable Band5", IIR2, BAND5, 1, 0,
	taiko_get_iir_enable_audio_mixer, taiko_put_iir_enable_audio_mixer),

	SOC_SINGLE_MULTI_EXT("IIR1 Band1", IIR1, BAND1, 255, 0, 5,
	taiko_get_iir_band_audio_mixer, taiko_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR1 Band2", IIR1, BAND2, 255, 0, 5,
	taiko_get_iir_band_audio_mixer, taiko_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR1 Band3", IIR1, BAND3, 255, 0, 5,
	taiko_get_iir_band_audio_mixer, taiko_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR1 Band4", IIR1, BAND4, 255, 0, 5,
	taiko_get_iir_band_audio_mixer, taiko_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR1 Band5", IIR1, BAND5, 255, 0, 5,
	taiko_get_iir_band_audio_mixer, taiko_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR2 Band1", IIR2, BAND1, 255, 0, 5,
	taiko_get_iir_band_audio_mixer, taiko_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR2 Band2", IIR2, BAND2, 255, 0, 5,
	taiko_get_iir_band_audio_mixer, taiko_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR2 Band3", IIR2, BAND3, 255, 0, 5,
	taiko_get_iir_band_audio_mixer, taiko_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR2 Band4", IIR2, BAND4, 255, 0, 5,
	taiko_get_iir_band_audio_mixer, taiko_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR2 Band5", IIR2, BAND5, 255, 0, 5,
	taiko_get_iir_band_audio_mixer, taiko_put_iir_band_audio_mixer),

	SOC_SINGLE_EXT("COMP1 Switch", SND_SOC_NOPM, 1, COMPANDER_1, 0,
				   taiko_get_compander, taiko_set_compander),
	SOC_SINGLE_EXT("COMP2 Switch", SND_SOC_NOPM, 0, COMPANDER_2, 0,
				   taiko_get_compander, taiko_set_compander),

};

static const char * const rx_mix1_text[] = {
	"ZERO", "SRC1", "SRC2", "IIR1", "IIR2", "RX1", "RX2", "RX3", "RX4",
		"RX5", "RX6", "RX7"
};

static const char * const rx_mix2_text[] = {
	"ZERO", "SRC1", "SRC2", "IIR1", "IIR2"
};

static const char * const rx_dsm_text[] = {
	"CIC_OUT", "DSM_INV"
};

static const char * const sb_tx1_mux_text[] = {
	"ZERO", "RMIX1", "RMIX2", "RMIX3", "RMIX4", "RMIX5", "RMIX6", "RMIX7",
		"DEC1"
};

static const char * const sb_tx2_mux_text[] = {
	"ZERO", "RMIX1", "RMIX2", "RMIX3", "RMIX4", "RMIX5", "RMIX6", "RMIX7",
		"DEC2"
};

static const char * const sb_tx3_mux_text[] = {
	"ZERO", "RMIX1", "RMIX2", "RMIX3", "RMIX4", "RMIX5", "RMIX6", "RMIX7",
		"DEC3"
};

static const char * const sb_tx4_mux_text[] = {
	"ZERO", "RMIX1", "RMIX2", "RMIX3", "RMIX4", "RMIX5", "RMIX6", "RMIX7",
		"DEC4"
};

static const char * const sb_tx5_mux_text[] = {
	"ZERO", "RMIX1", "RMIX2", "RMIX3", "RMIX4", "RMIX5", "RMIX6", "RMIX7",
		"DEC5"
};

static const char * const sb_tx6_mux_text[] = {
	"ZERO", "RMIX1", "RMIX2", "RMIX3", "RMIX4", "RMIX5", "RMIX6", "RMIX7",
		"DEC6"
};

static const char * const sb_tx7_to_tx10_mux_text[] = {
	"ZERO", "RMIX1", "RMIX2", "RMIX3", "RMIX4", "RMIX5", "RMIX6", "RMIX7",
		"DEC1", "DEC2", "DEC3", "DEC4", "DEC5", "DEC6", "DEC7", "DEC8",
		"DEC9", "DEC10"
};

static const char * const dec1_mux_text[] = {
	"ZERO", "DMIC1", "ADC6",
};

static const char * const dec2_mux_text[] = {
	"ZERO", "DMIC2", "ADC5",
};

static const char * const dec3_mux_text[] = {
	"ZERO", "DMIC3", "ADC4",
};

static const char * const dec4_mux_text[] = {
	"ZERO", "DMIC4", "ADC3",
};

static const char * const dec5_mux_text[] = {
	"ZERO", "DMIC5", "ADC2",
};

static const char * const dec6_mux_text[] = {
	"ZERO", "DMIC6", "ADC1",
};

static const char * const dec7_mux_text[] = {
	"ZERO", "DMIC1", "DMIC6", "ADC1", "ADC6", "ANC1_FB", "ANC2_FB",
};

static const char * const dec8_mux_text[] = {
	"ZERO", "DMIC2", "DMIC5", "ADC2", "ADC5",
};

static const char * const dec9_mux_text[] = {
	"ZERO", "DMIC4", "DMIC5", "ADC2", "ADC3", "ADCMB", "ANC1_FB", "ANC2_FB",
};

static const char * const dec10_mux_text[] = {
	"ZERO", "DMIC3", "DMIC6", "ADC1", "ADC4", "ADCMB", "ANC1_FB", "ANC2_FB",
};

static const char * const anc_mux_text[] = {
	"ZERO", "ADC1", "ADC2", "ADC3", "ADC4", "ADC5", "ADC6", "ADC_MB",
		"RSVD_1", "DMIC1", "DMIC2", "DMIC3", "DMIC4", "DMIC5", "DMIC6"
};

static const char * const anc1_fb_mux_text[] = {
	"ZERO", "EAR_HPH_L", "EAR_LINE_1",
};

static const char * const iir1_inp1_text[] = {
	"ZERO", "DEC1", "DEC2", "DEC3", "DEC4", "DEC5", "DEC6", "DEC7", "DEC8",
	"DEC9", "DEC10", "RX1", "RX2", "RX3", "RX4", "RX5", "RX6", "RX7"
};

static const struct soc_enum rx_mix1_inp1_chain_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_CONN_RX1_B1_CTL, 0, 12, rx_mix1_text);

static const struct soc_enum rx_mix1_inp2_chain_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_CONN_RX1_B1_CTL, 4, 12, rx_mix1_text);

static const struct soc_enum rx_mix1_inp3_chain_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_CONN_RX1_B2_CTL, 0, 12, rx_mix1_text);

static const struct soc_enum rx2_mix1_inp1_chain_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_CONN_RX2_B1_CTL, 0, 12, rx_mix1_text);

static const struct soc_enum rx2_mix1_inp2_chain_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_CONN_RX2_B1_CTL, 4, 12, rx_mix1_text);

static const struct soc_enum rx3_mix1_inp1_chain_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_CONN_RX3_B1_CTL, 0, 12, rx_mix1_text);

static const struct soc_enum rx3_mix1_inp2_chain_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_CONN_RX3_B1_CTL, 4, 12, rx_mix1_text);

static const struct soc_enum rx4_mix1_inp1_chain_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_CONN_RX4_B1_CTL, 0, 12, rx_mix1_text);

static const struct soc_enum rx4_mix1_inp2_chain_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_CONN_RX4_B1_CTL, 4, 12, rx_mix1_text);

static const struct soc_enum rx5_mix1_inp1_chain_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_CONN_RX5_B1_CTL, 0, 12, rx_mix1_text);

static const struct soc_enum rx5_mix1_inp2_chain_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_CONN_RX5_B1_CTL, 4, 12, rx_mix1_text);

static const struct soc_enum rx6_mix1_inp1_chain_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_CONN_RX6_B1_CTL, 0, 12, rx_mix1_text);

static const struct soc_enum rx6_mix1_inp2_chain_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_CONN_RX6_B1_CTL, 4, 12, rx_mix1_text);

static const struct soc_enum rx7_mix1_inp1_chain_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_CONN_RX7_B1_CTL, 0, 12, rx_mix1_text);

static const struct soc_enum rx7_mix1_inp2_chain_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_CONN_RX7_B1_CTL, 4, 12, rx_mix1_text);

static const struct soc_enum rx1_mix2_inp1_chain_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_CONN_RX1_B3_CTL, 0, 5, rx_mix2_text);

static const struct soc_enum rx1_mix2_inp2_chain_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_CONN_RX1_B3_CTL, 3, 5, rx_mix2_text);

static const struct soc_enum rx2_mix2_inp1_chain_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_CONN_RX2_B3_CTL, 0, 5, rx_mix2_text);

static const struct soc_enum rx2_mix2_inp2_chain_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_CONN_RX2_B3_CTL, 3, 5, rx_mix2_text);

static const struct soc_enum rx7_mix2_inp1_chain_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_CONN_RX7_B3_CTL, 0, 5, rx_mix2_text);

static const struct soc_enum rx7_mix2_inp2_chain_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_CONN_RX7_B3_CTL, 3, 5, rx_mix2_text);

static const struct soc_enum rx4_dsm_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_RX4_B6_CTL, 4, 2, rx_dsm_text);

static const struct soc_enum rx6_dsm_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_RX6_B6_CTL, 4, 2, rx_dsm_text);

static const struct soc_enum sb_tx1_mux_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_CONN_TX_SB_B1_CTL, 0, 9, sb_tx1_mux_text);

static const struct soc_enum sb_tx2_mux_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_CONN_TX_SB_B2_CTL, 0, 9, sb_tx2_mux_text);

static const struct soc_enum sb_tx3_mux_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_CONN_TX_SB_B3_CTL, 0, 9, sb_tx3_mux_text);

static const struct soc_enum sb_tx4_mux_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_CONN_TX_SB_B4_CTL, 0, 9, sb_tx4_mux_text);

static const struct soc_enum sb_tx5_mux_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_CONN_TX_SB_B5_CTL, 0, 9, sb_tx5_mux_text);

static const struct soc_enum sb_tx6_mux_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_CONN_TX_SB_B6_CTL, 0, 9, sb_tx6_mux_text);

static const struct soc_enum sb_tx7_mux_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_CONN_TX_SB_B7_CTL, 0, 18,
			sb_tx7_to_tx10_mux_text);

static const struct soc_enum sb_tx8_mux_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_CONN_TX_SB_B8_CTL, 0, 18,
			sb_tx7_to_tx10_mux_text);

static const struct soc_enum sb_tx9_mux_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_CONN_TX_SB_B9_CTL, 0, 18,
			sb_tx7_to_tx10_mux_text);

static const struct soc_enum sb_tx10_mux_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_CONN_TX_SB_B10_CTL, 0, 18,
			sb_tx7_to_tx10_mux_text);

static const struct soc_enum dec1_mux_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_CONN_TX_B1_CTL, 0, 3, dec1_mux_text);

static const struct soc_enum dec2_mux_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_CONN_TX_B1_CTL, 2, 3, dec2_mux_text);

static const struct soc_enum dec3_mux_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_CONN_TX_B1_CTL, 4, 3, dec3_mux_text);

static const struct soc_enum dec4_mux_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_CONN_TX_B1_CTL, 6, 3, dec4_mux_text);

static const struct soc_enum dec5_mux_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_CONN_TX_B2_CTL, 0, 3, dec5_mux_text);

static const struct soc_enum dec6_mux_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_CONN_TX_B2_CTL, 2, 3, dec6_mux_text);

static const struct soc_enum dec7_mux_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_CONN_TX_B2_CTL, 4, 7, dec7_mux_text);

static const struct soc_enum dec8_mux_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_CONN_TX_B3_CTL, 0, 7, dec8_mux_text);

static const struct soc_enum dec9_mux_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_CONN_TX_B3_CTL, 3, 8, dec9_mux_text);

static const struct soc_enum dec10_mux_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_CONN_TX_B4_CTL, 0, 8, dec10_mux_text);

static const struct soc_enum anc1_mux_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_CONN_ANC_B1_CTL, 0, 16, anc_mux_text);

static const struct soc_enum anc2_mux_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_CONN_ANC_B1_CTL, 4, 16, anc_mux_text);

static const struct soc_enum anc1_fb_mux_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_CONN_ANC_B2_CTL, 0, 3, anc1_fb_mux_text);

static const struct soc_enum iir1_inp1_mux_enum =
	SOC_ENUM_SINGLE(TAIKO_A_CDC_CONN_EQ1_B1_CTL, 0, 18, iir1_inp1_text);

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

static const struct snd_kcontrol_new rx1_mix2_inp1_mux =
	SOC_DAPM_ENUM("RX1 MIX2 INP1 Mux", rx1_mix2_inp1_chain_enum);

static const struct snd_kcontrol_new rx1_mix2_inp2_mux =
	SOC_DAPM_ENUM("RX1 MIX2 INP2 Mux", rx1_mix2_inp2_chain_enum);

static const struct snd_kcontrol_new rx2_mix2_inp1_mux =
	SOC_DAPM_ENUM("RX2 MIX2 INP1 Mux", rx2_mix2_inp1_chain_enum);

static const struct snd_kcontrol_new rx2_mix2_inp2_mux =
	SOC_DAPM_ENUM("RX2 MIX2 INP2 Mux", rx2_mix2_inp2_chain_enum);

static const struct snd_kcontrol_new rx7_mix2_inp1_mux =
	SOC_DAPM_ENUM("RX7 MIX2 INP1 Mux", rx7_mix2_inp1_chain_enum);

static const struct snd_kcontrol_new rx7_mix2_inp2_mux =
	SOC_DAPM_ENUM("RX7 MIX2 INP2 Mux", rx7_mix2_inp2_chain_enum);

static const struct snd_kcontrol_new rx4_dsm_mux =
	SOC_DAPM_ENUM("RX4 DSM MUX Mux", rx4_dsm_enum);

static const struct snd_kcontrol_new rx6_dsm_mux =
	SOC_DAPM_ENUM("RX6 DSM MUX Mux", rx6_dsm_enum);

static const struct snd_kcontrol_new sb_tx1_mux =
	SOC_DAPM_ENUM("SLIM TX1 MUX Mux", sb_tx1_mux_enum);

static const struct snd_kcontrol_new sb_tx2_mux =
	SOC_DAPM_ENUM("SLIM TX2 MUX Mux", sb_tx2_mux_enum);

static const struct snd_kcontrol_new sb_tx3_mux =
	SOC_DAPM_ENUM("SLIM TX3 MUX Mux", sb_tx3_mux_enum);

static const struct snd_kcontrol_new sb_tx4_mux =
	SOC_DAPM_ENUM("SLIM TX4 MUX Mux", sb_tx4_mux_enum);

static const struct snd_kcontrol_new sb_tx5_mux =
	SOC_DAPM_ENUM("SLIM TX5 MUX Mux", sb_tx5_mux_enum);

static const struct snd_kcontrol_new sb_tx6_mux =
	SOC_DAPM_ENUM("SLIM TX6 MUX Mux", sb_tx6_mux_enum);

static const struct snd_kcontrol_new sb_tx7_mux =
	SOC_DAPM_ENUM("SLIM TX7 MUX Mux", sb_tx7_mux_enum);

static const struct snd_kcontrol_new sb_tx8_mux =
	SOC_DAPM_ENUM("SLIM TX8 MUX Mux", sb_tx8_mux_enum);

static const struct snd_kcontrol_new sb_tx9_mux =
	SOC_DAPM_ENUM("SLIM TX9 MUX Mux", sb_tx9_mux_enum);

static const struct snd_kcontrol_new sb_tx10_mux =
	SOC_DAPM_ENUM("SLIM TX10 MUX Mux", sb_tx10_mux_enum);


static int wcd9320_put_dec_enum(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist = snd_kcontrol_chip(kcontrol);
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

	if (ucontrol->value.enumerated.item[0] > e->max - 1)
		return -EINVAL;

	dec_mux = ucontrol->value.enumerated.item[0];

	widget_name = kstrndup(w->name, 15, GFP_KERNEL);
	if (!widget_name)
		return -ENOMEM;
	temp = widget_name;

	dec_name = strsep(&widget_name, " ");
	widget_name = temp;
	if (!dec_name) {
		pr_err("%s: Invalid decimator = %s\n", __func__, w->name);
		ret =  -EINVAL;
		goto out;
	}

	ret = kstrtouint(strpbrk(dec_name, "123456789"), 10, &decimator);
	if (ret < 0) {
		pr_err("%s: Invalid decimator = %s\n", __func__, dec_name);
		ret =  -EINVAL;
		goto out;
	}

	dev_dbg(w->dapm->dev, "%s(): widget = %s decimator = %u dec_mux = %u\n"
		, __func__, w->name, decimator, dec_mux);


	switch (decimator) {
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
		if (dec_mux == 1)
			adc_dmic_sel = 0x1;
		else
			adc_dmic_sel = 0x0;
		break;
	case 7:
	case 8:
	case 9:
	case 10:
		if ((dec_mux == 1) || (dec_mux == 2))
			adc_dmic_sel = 0x1;
		else
			adc_dmic_sel = 0x0;
		break;
	default:
		pr_err("%s: Invalid Decimator = %u\n", __func__, decimator);
		ret = -EINVAL;
		goto out;
	}

	tx_mux_ctl_reg = TAIKO_A_CDC_TX1_MUX_CTL + 8 * (decimator - 1);

	snd_soc_update_bits(codec, tx_mux_ctl_reg, 0x1, adc_dmic_sel);

	ret = snd_soc_dapm_put_enum_double(kcontrol, ucontrol);

out:
	kfree(widget_name);
	return ret;
}

#define WCD9320_DEC_ENUM(xname, xenum) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
	.info = snd_soc_info_enum_double, \
	.get = snd_soc_dapm_get_enum_double, \
	.put = wcd9320_put_dec_enum, \
	.private_value = (unsigned long)&xenum }

static const struct snd_kcontrol_new dec1_mux =
	WCD9320_DEC_ENUM("DEC1 MUX Mux", dec1_mux_enum);

static const struct snd_kcontrol_new dec2_mux =
	WCD9320_DEC_ENUM("DEC2 MUX Mux", dec2_mux_enum);

static const struct snd_kcontrol_new dec3_mux =
	WCD9320_DEC_ENUM("DEC3 MUX Mux", dec3_mux_enum);

static const struct snd_kcontrol_new dec4_mux =
	WCD9320_DEC_ENUM("DEC4 MUX Mux", dec4_mux_enum);

static const struct snd_kcontrol_new dec5_mux =
	WCD9320_DEC_ENUM("DEC5 MUX Mux", dec5_mux_enum);

static const struct snd_kcontrol_new dec6_mux =
	WCD9320_DEC_ENUM("DEC6 MUX Mux", dec6_mux_enum);

static const struct snd_kcontrol_new dec7_mux =
	WCD9320_DEC_ENUM("DEC7 MUX Mux", dec7_mux_enum);

static const struct snd_kcontrol_new dec8_mux =
	WCD9320_DEC_ENUM("DEC8 MUX Mux", dec8_mux_enum);

static const struct snd_kcontrol_new dec9_mux =
	WCD9320_DEC_ENUM("DEC9 MUX Mux", dec9_mux_enum);

static const struct snd_kcontrol_new dec10_mux =
	WCD9320_DEC_ENUM("DEC10 MUX Mux", dec10_mux_enum);

static const struct snd_kcontrol_new iir1_inp1_mux =
	SOC_DAPM_ENUM("IIR1 INP1 Mux", iir1_inp1_mux_enum);

static const struct snd_kcontrol_new anc1_mux =
	SOC_DAPM_ENUM("ANC1 MUX Mux", anc1_mux_enum);

static const struct snd_kcontrol_new anc2_mux =
	SOC_DAPM_ENUM("ANC2 MUX Mux", anc2_mux_enum);

static const struct snd_kcontrol_new anc1_fb_mux =
	SOC_DAPM_ENUM("ANC1 FB MUX Mux", anc1_fb_mux_enum);

static const struct snd_kcontrol_new dac1_switch[] = {
	SOC_DAPM_SINGLE("Switch", TAIKO_A_RX_EAR_EN, 5, 1, 0)
};
static const struct snd_kcontrol_new hphl_switch[] = {
	SOC_DAPM_SINGLE("Switch", TAIKO_A_RX_HPH_L_DAC_CTL, 6, 1, 0)
};

static const struct snd_kcontrol_new hphl_pa_mix[] = {
	SOC_DAPM_SINGLE("AUX_PGA_L Switch", TAIKO_A_RX_PA_AUX_IN_CONN,
					7, 1, 0),
};

static const struct snd_kcontrol_new hphr_pa_mix[] = {
	SOC_DAPM_SINGLE("AUX_PGA_R Switch", TAIKO_A_RX_PA_AUX_IN_CONN,
					6, 1, 0),
};

static const struct snd_kcontrol_new ear_pa_mix[] = {
	SOC_DAPM_SINGLE("AUX_PGA_L Switch", TAIKO_A_RX_PA_AUX_IN_CONN,
					5, 1, 0),
};
static const struct snd_kcontrol_new lineout1_pa_mix[] = {
	SOC_DAPM_SINGLE("AUX_PGA_L Switch", TAIKO_A_RX_PA_AUX_IN_CONN,
					4, 1, 0),
};

static const struct snd_kcontrol_new lineout2_pa_mix[] = {
	SOC_DAPM_SINGLE("AUX_PGA_R Switch", TAIKO_A_RX_PA_AUX_IN_CONN,
					3, 1, 0),
};

static const struct snd_kcontrol_new lineout3_pa_mix[] = {
	SOC_DAPM_SINGLE("AUX_PGA_L Switch", TAIKO_A_RX_PA_AUX_IN_CONN,
					2, 1, 0),
};

static const struct snd_kcontrol_new lineout4_pa_mix[] = {
	SOC_DAPM_SINGLE("AUX_PGA_R Switch", TAIKO_A_RX_PA_AUX_IN_CONN,
					1, 1, 0),
};

static const struct snd_kcontrol_new lineout3_ground_switch =
	SOC_DAPM_SINGLE("Switch", TAIKO_A_RX_LINE_3_DAC_CTL, 6, 1, 0);

static const struct snd_kcontrol_new lineout4_ground_switch =
	SOC_DAPM_SINGLE("Switch", TAIKO_A_RX_LINE_4_DAC_CTL, 6, 1, 0);

static void taiko_codec_enable_adc_block(struct snd_soc_codec *codec,
					 int enable)
{
	struct taiko_priv *taiko = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s %d\n", __func__, enable);

	if (enable) {
		taiko->adc_count++;
		snd_soc_update_bits(codec, TAIKO_A_CDC_CLK_OTHR_CTL, 0x2, 0x2);
	} else {
		taiko->adc_count--;
		if (!taiko->adc_count)
			snd_soc_update_bits(codec, TAIKO_A_CDC_CLK_OTHR_CTL,
					    0x2, 0x0);
	}
}

static int taiko_codec_enable_adc(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	u16 adc_reg;
	u8 init_bit_shift;

	pr_debug("%s %d\n", __func__, event);

	if (w->reg == TAIKO_A_TX_1_2_EN)
		adc_reg = TAIKO_A_TX_1_2_TEST_CTL;
	else if (w->reg == TAIKO_A_TX_3_4_EN)
		adc_reg = TAIKO_A_TX_3_4_TEST_CTL;
	else if (w->reg == TAIKO_A_TX_5_6_EN)
		adc_reg = TAIKO_A_TX_5_6_TEST_CTL;
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
		taiko_codec_enable_adc_block(codec, 1);
		snd_soc_update_bits(codec, adc_reg, 1 << init_bit_shift,
				1 << init_bit_shift);
		break;
	case SND_SOC_DAPM_POST_PMU:

		snd_soc_update_bits(codec, adc_reg, 1 << init_bit_shift, 0x00);

		break;
	case SND_SOC_DAPM_POST_PMD:
		taiko_codec_enable_adc_block(codec, 0);
		break;
	}
	return 0;
}

static void taiko_codec_enable_audio_mode_bandgap(struct snd_soc_codec *codec)
{
	snd_soc_update_bits(codec, TAIKO_A_BIAS_CENTRAL_BG_CTL, 0x80,
		0x80);
	snd_soc_update_bits(codec, TAIKO_A_BIAS_CENTRAL_BG_CTL, 0x04,
		0x04);
	snd_soc_update_bits(codec, TAIKO_A_BIAS_CENTRAL_BG_CTL, 0x01,
		0x01);
	usleep_range(1000, 1000);
	snd_soc_update_bits(codec, TAIKO_A_BIAS_CENTRAL_BG_CTL, 0x80,
		0x00);
}

static void taiko_codec_enable_bandgap(struct snd_soc_codec *codec,
	enum taiko_bandgap_type choice)
{
	struct taiko_priv *taiko = snd_soc_codec_get_drvdata(codec);

	/* TODO lock resources accessed by audio streams and threaded
	 * interrupt handlers
	 */

	pr_debug("%s, choice is %d, current is %d\n", __func__, choice,
		taiko->bandgap_type);

	if (taiko->bandgap_type == choice)
		return;

	if ((taiko->bandgap_type == TAIKO_BANDGAP_OFF) &&
		(choice == TAIKO_BANDGAP_AUDIO_MODE)) {
		taiko_codec_enable_audio_mode_bandgap(codec);
	} else if (choice == TAIKO_BANDGAP_MBHC_MODE) {
		/* bandgap mode becomes fast,
		 * mclk should be off or clk buff source souldn't be VBG
		 * Let's turn off mclk always */
		WARN_ON(snd_soc_read(codec, TAIKO_A_CLK_BUFF_EN2) & (1 << 2));
		snd_soc_update_bits(codec, TAIKO_A_BIAS_CENTRAL_BG_CTL, 0x2,
			0x2);
		snd_soc_update_bits(codec, TAIKO_A_BIAS_CENTRAL_BG_CTL, 0x80,
			0x80);
		snd_soc_update_bits(codec, TAIKO_A_BIAS_CENTRAL_BG_CTL, 0x4,
			0x4);
		snd_soc_update_bits(codec, TAIKO_A_BIAS_CENTRAL_BG_CTL, 0x01,
			0x01);
		usleep_range(1000, 1000);
		snd_soc_update_bits(codec, TAIKO_A_BIAS_CENTRAL_BG_CTL, 0x80,
			0x00);
	} else if ((taiko->bandgap_type == TAIKO_BANDGAP_MBHC_MODE) &&
		(choice == TAIKO_BANDGAP_AUDIO_MODE)) {
		snd_soc_write(codec, TAIKO_A_BIAS_CENTRAL_BG_CTL, 0x00);
		usleep_range(100, 100);
		taiko_codec_enable_audio_mode_bandgap(codec);
	} else if (choice == TAIKO_BANDGAP_OFF) {
		snd_soc_write(codec, TAIKO_A_BIAS_CENTRAL_BG_CTL, 0x00);
	} else {
		pr_err("%s: Error, Invalid bandgap settings\n", __func__);
	}
	taiko->bandgap_type = choice;
}

static void taiko_codec_disable_clock_block(struct snd_soc_codec *codec)
{
	struct taiko_priv *taiko = snd_soc_codec_get_drvdata(codec);
	pr_debug("%s\n", __func__);
	snd_soc_update_bits(codec, TAIKO_A_CLK_BUFF_EN2, 0x04, 0x00);
	usleep_range(50, 50);
	snd_soc_update_bits(codec, TAIKO_A_CLK_BUFF_EN2, 0x02, 0x02);
	snd_soc_update_bits(codec, TAIKO_A_CLK_BUFF_EN1, 0x05, 0x00);
	usleep_range(50, 50);
	taiko->clock_active = false;
}

static int taiko_codec_mclk_index(const struct taiko_priv *taiko)
{
	if (taiko->mbhc_cfg.mclk_rate == TAIKO_MCLK_RATE_12288KHZ)
		return 0;
	else if (taiko->mbhc_cfg.mclk_rate == TAIKO_MCLK_RATE_9600KHZ)
		return 1;
	else {
		BUG_ON(1);
		return -EINVAL;
	}
}

static void taiko_enable_rx_bias(struct snd_soc_codec *codec, u32  enable)
{
	struct taiko_priv *taiko = snd_soc_codec_get_drvdata(codec);

	if (enable) {
		taiko->rx_bias_count++;
		if (taiko->rx_bias_count == 1)
			snd_soc_update_bits(codec, TAIKO_A_RX_COM_BIAS,
				0x80, 0x80);
	} else {
		taiko->rx_bias_count--;
		if (!taiko->rx_bias_count)
			snd_soc_update_bits(codec, TAIKO_A_RX_COM_BIAS,
				0x80, 0x00);
	}
}

static int taiko_codec_enable_config_mode(struct snd_soc_codec *codec,
	int enable)
{
	struct taiko_priv *taiko = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s: enable = %d\n", __func__, enable);
	if (enable) {

		snd_soc_update_bits(codec, TAIKO_A_RC_OSC_FREQ, 0x10, 0);
		/* bandgap mode to fast */
		snd_soc_write(codec, TAIKO_A_BIAS_OSC_BG_CTL, 0x17);
		usleep_range(5, 5);
		snd_soc_update_bits(codec, TAIKO_A_RC_OSC_FREQ, 0x80,
				    0x80);
		snd_soc_update_bits(codec, TAIKO_A_RC_OSC_TEST, 0x80,
				    0x80);
		usleep_range(10, 10);
		snd_soc_update_bits(codec, TAIKO_A_RC_OSC_TEST, 0x80, 0);
		usleep_range(10000, 10000);
		snd_soc_update_bits(codec, TAIKO_A_CLK_BUFF_EN1, 0x08, 0x08);

	} else {
		snd_soc_update_bits(codec, TAIKO_A_BIAS_OSC_BG_CTL, 0x1,
				    0);
		snd_soc_update_bits(codec, TAIKO_A_RC_OSC_FREQ, 0x80, 0);
		/* clk source to ext clk and clk buff ref to VBG */
		snd_soc_update_bits(codec, TAIKO_A_CLK_BUFF_EN1, 0x0C, 0x04);
	}
	taiko->config_mode_active = enable ? true : false;

	return 0;
}

static int taiko_codec_enable_clock_block(struct snd_soc_codec *codec,
					  int config_mode)
{
	struct taiko_priv *taiko = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s: config_mode = %d\n", __func__, config_mode);

	/* transit to RCO requires mclk off */
	WARN_ON(snd_soc_read(codec, TAIKO_A_CLK_BUFF_EN2) & (1 << 2));
	if (config_mode) {
		/* enable RCO and switch to it */
		taiko_codec_enable_config_mode(codec, 1);
		snd_soc_write(codec, TAIKO_A_CLK_BUFF_EN2, 0x02);
		usleep_range(1000, 1000);
	} else {
		/* switch to MCLK */
		snd_soc_update_bits(codec, TAIKO_A_CLK_BUFF_EN1, 0x08, 0x00);

		if (taiko->mbhc_polling_active) {
			snd_soc_write(codec, TAIKO_A_CLK_BUFF_EN2, 0x02);
			taiko_codec_enable_config_mode(codec, 0);
		}
	}

	snd_soc_update_bits(codec, TAIKO_A_CLK_BUFF_EN1, 0x01, 0x01);
	snd_soc_update_bits(codec, TAIKO_A_CLK_BUFF_EN2, 0x02, 0x00);
	/* on MCLK */
	snd_soc_update_bits(codec, TAIKO_A_CLK_BUFF_EN2, 0x04, 0x04);
	snd_soc_update_bits(codec, TAIKO_A_CDC_CLK_MCLK_CTL, 0x01, 0x01);
	usleep_range(50, 50);
	taiko->clock_active = true;
	return 0;
}

static int taiko_codec_enable_aux_pga(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct taiko_priv *taiko = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s: %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		taiko_codec_enable_bandgap(codec, TAIKO_BANDGAP_AUDIO_MODE);
		taiko_enable_rx_bias(codec, 1);

		if (taiko->aux_pga_cnt++ == 1
			&& !taiko->mclk_enabled) {
			taiko_codec_enable_clock_block(codec, 1);
			pr_debug("AUX PGA enabled RC osc\n");
		}
		break;

	case SND_SOC_DAPM_POST_PMD:
		taiko_enable_rx_bias(codec, 0);

		if (taiko->aux_pga_cnt-- == 0) {
			if (taiko->mbhc_polling_active)
				taiko_codec_enable_bandgap(codec,
					TAIKO_BANDGAP_MBHC_MODE);
			else
				taiko_codec_enable_bandgap(codec,
					TAIKO_BANDGAP_OFF);

			if (!taiko->mclk_enabled &&
				!taiko->mbhc_polling_active) {
				taiko_codec_enable_clock_block(codec, 0);
			}
		}
		break;
	}
	return 0;
}

static int taiko_codec_enable_lineout(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	u16 lineout_gain_reg;

	pr_debug("%s %d %s\n", __func__, event, w->name);

	switch (w->shift) {
	case 0:
		lineout_gain_reg = TAIKO_A_RX_LINE_1_GAIN;
		break;
	case 1:
		lineout_gain_reg = TAIKO_A_RX_LINE_2_GAIN;
		break;
	case 2:
		lineout_gain_reg = TAIKO_A_RX_LINE_3_GAIN;
		break;
	case 3:
		lineout_gain_reg = TAIKO_A_RX_LINE_4_GAIN;
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

static int taiko_codec_enable_spk_pa(struct snd_soc_dapm_widget *w,
				     struct snd_kcontrol *kcontrol, int event)
{
	pr_debug("%s %d %s\n", __func__, event, w->name);
	return 0;
}

static int taiko_codec_enable_dmic(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct taiko_priv *taiko = snd_soc_codec_get_drvdata(codec);
	u8  dmic_clk_en;
	u16 dmic_clk_reg;
	s32 *dmic_clk_cnt;
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
		dmic_clk_en = 0x01;
		dmic_clk_cnt = &(taiko->dmic_1_2_clk_cnt);
		dmic_clk_reg = TAIKO_A_CDC_CLK_DMIC_B1_CTL;
		pr_debug("%s() event %d DMIC%d dmic_1_2_clk_cnt %d\n",
			__func__, event,  dmic, *dmic_clk_cnt);

		break;

	case 3:
	case 4:
		dmic_clk_en = 0x10;
		dmic_clk_cnt = &(taiko->dmic_3_4_clk_cnt);
		dmic_clk_reg = TAIKO_A_CDC_CLK_DMIC_B1_CTL;

		pr_debug("%s() event %d DMIC%d dmic_3_4_clk_cnt %d\n",
			__func__, event,  dmic, *dmic_clk_cnt);
		break;

	case 5:
	case 6:
		dmic_clk_en = 0x01;
		dmic_clk_cnt = &(taiko->dmic_5_6_clk_cnt);
		dmic_clk_reg = TAIKO_A_CDC_CLK_DMIC_B2_CTL;

		pr_debug("%s() event %d DMIC%d dmic_5_6_clk_cnt %d\n",
			__func__, event,  dmic, *dmic_clk_cnt);

		break;

	default:
		pr_err("%s: Invalid DMIC Selection\n", __func__);
		return -EINVAL;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:

		(*dmic_clk_cnt)++;
		if (*dmic_clk_cnt == 1)
			snd_soc_update_bits(codec, dmic_clk_reg,
					dmic_clk_en, dmic_clk_en);

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

static int taiko_codec_enable_anc(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	const char *filename;
	const struct firmware *fw;
	int i;
	int ret;
	int num_anc_slots;
	struct anc_header *anc_head;
	struct taiko_priv *taiko = snd_soc_codec_get_drvdata(codec);
	u32 anc_writes_size = 0;
	int anc_size_remaining;
	u32 *anc_ptr;
	u16 reg;
	u8 mask, val, old_val;

	pr_debug("%s %d\n", __func__, event);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:

		filename = "wcd9320/wcd9320_anc.bin";

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

		if (taiko->anc_slot >= num_anc_slots) {
			dev_err(codec->dev, "Invalid ANC slot selected\n");
			release_firmware(fw);
			return -EINVAL;
		}

		for (i = 0; i < num_anc_slots; i++) {

			if (anc_size_remaining < TAIKO_PACKED_REG_SIZE) {
				dev_err(codec->dev, "Invalid register format\n");
				release_firmware(fw);
				return -EINVAL;
			}
			anc_writes_size = (u32)(*anc_ptr);
			anc_size_remaining -= sizeof(u32);
			anc_ptr += 1;

			if (anc_writes_size * TAIKO_PACKED_REG_SIZE
				> anc_size_remaining) {
				dev_err(codec->dev, "Invalid register format\n");
				release_firmware(fw);
				return -ENOMEM;
			}

			if (taiko->anc_slot == i)
				break;

			anc_size_remaining -= (anc_writes_size *
				TAIKO_PACKED_REG_SIZE);
			anc_ptr += anc_writes_size;
		}
		if (i == num_anc_slots) {
			dev_err(codec->dev, "Selected ANC slot not present\n");
			release_firmware(fw);
			return -ENOMEM;
		}

		for (i = 0; i < anc_writes_size; i++) {
			TAIKO_CODEC_UNPACK_ENTRY(anc_ptr[i], reg,
				mask, val);
			old_val = snd_soc_read(codec, reg);
			snd_soc_write(codec, reg, (old_val & ~mask) |
				(val & mask));
		}
		release_firmware(fw);

		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_write(codec, TAIKO_A_CDC_CLK_ANC_RESET_CTL, 0xFF);
		snd_soc_write(codec, TAIKO_A_CDC_CLK_ANC_CLK_EN_CTL, 0);
		break;
	}
	return 0;
}

/* called under codec_resource_lock acquisition */
static void taiko_codec_start_hs_polling(struct snd_soc_codec *codec)
{
	struct taiko_priv *taiko = snd_soc_codec_get_drvdata(codec);
	int mbhc_state = taiko->mbhc_state;

	pr_debug("%s: enter\n", __func__);
	if (!taiko->mbhc_polling_active) {
		pr_debug("Polling is not active, do not start polling\n");
		return;
	}
	snd_soc_write(codec, TAIKO_A_MBHC_SCALING_MUX_1, 0x84);

	if (!taiko->no_mic_headset_override) {
		if (mbhc_state == MBHC_STATE_POTENTIAL) {
			pr_debug("%s recovering MBHC state macine\n", __func__);
			taiko->mbhc_state = MBHC_STATE_POTENTIAL_RECOVERY;
			/* set to max button press threshold */
			snd_soc_write(codec, TAIKO_A_CDC_MBHC_VOLT_B2_CTL,
				      0x7F);
			snd_soc_write(codec, TAIKO_A_CDC_MBHC_VOLT_B1_CTL,
				      0xFF);
			snd_soc_write(codec, TAIKO_A_CDC_MBHC_VOLT_B4_CTL,
				       0x7F);
			snd_soc_write(codec, TAIKO_A_CDC_MBHC_VOLT_B3_CTL,
				      0xFF);
			/* set to max */
			snd_soc_write(codec, TAIKO_A_CDC_MBHC_VOLT_B6_CTL,
				      0x7F);
			snd_soc_write(codec, TAIKO_A_CDC_MBHC_VOLT_B5_CTL,
				      0xFF);
		}
	}

	snd_soc_write(codec, TAIKO_A_CDC_MBHC_EN_CTL, 0x1);
	snd_soc_update_bits(codec, TAIKO_A_CDC_MBHC_CLK_CTL, 0x8, 0x0);
	snd_soc_write(codec, TAIKO_A_CDC_MBHC_EN_CTL, 0x1);
	pr_debug("%s: leave\n", __func__);
}

/* called under codec_resource_lock acquisition */
static void taiko_codec_pause_hs_polling(struct snd_soc_codec *codec)
{
	struct taiko_priv *taiko = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s: enter\n", __func__);
	if (!taiko->mbhc_polling_active) {
		pr_debug("polling not active, nothing to pause\n");
		return;
	}

	snd_soc_update_bits(codec, TAIKO_A_CDC_MBHC_CLK_CTL, 0x8, 0x8);
	pr_debug("%s: leave\n", __func__);
}

static void taiko_codec_switch_cfilt_mode(struct snd_soc_codec *codec, int mode)
{
	struct taiko_priv *taiko = snd_soc_codec_get_drvdata(codec);
	u8 reg_mode_val, cur_mode_val;
	bool mbhc_was_polling = false;

	if (mode)
		reg_mode_val = TAIKO_CFILT_FAST_MODE;
	else
		reg_mode_val = TAIKO_CFILT_SLOW_MODE;

	cur_mode_val = snd_soc_read(codec,
					taiko->mbhc_bias_regs.cfilt_ctl) & 0x40;

	if (cur_mode_val != reg_mode_val) {
		TAIKO_ACQUIRE_LOCK(taiko->codec_resource_lock);
		if (taiko->mbhc_polling_active) {
			taiko_codec_pause_hs_polling(codec);
			mbhc_was_polling = true;
		}
		snd_soc_update_bits(codec,
			taiko->mbhc_bias_regs.cfilt_ctl, 0x40, reg_mode_val);
		if (mbhc_was_polling)
			taiko_codec_start_hs_polling(codec);
		TAIKO_RELEASE_LOCK(taiko->codec_resource_lock);
		pr_debug("%s: CFILT mode change (%x to %x)\n", __func__,
			cur_mode_val, reg_mode_val);
	} else {
		pr_debug("%s: CFILT Value is already %x\n",
			__func__, cur_mode_val);
	}
}

static void taiko_codec_update_cfilt_usage(struct snd_soc_codec *codec,
					   u8 cfilt_sel, int inc)
{
	struct taiko_priv *taiko = snd_soc_codec_get_drvdata(codec);
	u32 *cfilt_cnt_ptr = NULL;
	u16 micb_cfilt_reg;

	switch (cfilt_sel) {
	case TAIKO_CFILT1_SEL:
		cfilt_cnt_ptr = &taiko->cfilt1_cnt;
		micb_cfilt_reg = TAIKO_A_MICB_CFILT_1_CTL;
		break;
	case TAIKO_CFILT2_SEL:
		cfilt_cnt_ptr = &taiko->cfilt2_cnt;
		micb_cfilt_reg = TAIKO_A_MICB_CFILT_2_CTL;
		break;
	case TAIKO_CFILT3_SEL:
		cfilt_cnt_ptr = &taiko->cfilt3_cnt;
		micb_cfilt_reg = TAIKO_A_MICB_CFILT_3_CTL;
		break;
	default:
		return; /* should not happen */
	}

	if (inc) {
		if (!(*cfilt_cnt_ptr)++) {
			/* Switch CFILT to slow mode if MBHC CFILT being used */
			if (cfilt_sel == taiko->mbhc_bias_regs.cfilt_sel)
				taiko_codec_switch_cfilt_mode(codec, 0);

			snd_soc_update_bits(codec, micb_cfilt_reg, 0x80, 0x80);
		}
	} else {
		/* check if count not zero, decrement
		 * then check if zero, go ahead disable cfilter
		 */
		if ((*cfilt_cnt_ptr) && !--(*cfilt_cnt_ptr)) {
			snd_soc_update_bits(codec, micb_cfilt_reg, 0x80, 0);

			/* Switch CFILT to fast mode if MBHC CFILT being used */
			if (cfilt_sel == taiko->mbhc_bias_regs.cfilt_sel)
				taiko_codec_switch_cfilt_mode(codec, 1);
		}
	}
}

static int taiko_find_k_value(unsigned int ldoh_v, unsigned int cfilt_mv)
{
	int rc = -EINVAL;
	unsigned min_mv, max_mv;

	switch (ldoh_v) {
	case TAIKO_LDOH_1P95_V:
		min_mv = 160;
		max_mv = 1800;
		break;
	case TAIKO_LDOH_2P35_V:
		min_mv = 200;
		max_mv = 2200;
		break;
	case TAIKO_LDOH_2P75_V:
		min_mv = 240;
		max_mv = 2600;
		break;
	case TAIKO_LDOH_2P85_V:
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

static bool taiko_is_hph_pa_on(struct snd_soc_codec *codec)
{
	u8 hph_reg_val = 0;
	hph_reg_val = snd_soc_read(codec, TAIKO_A_RX_HPH_CNP_EN);

	return (hph_reg_val & 0x30) ? true : false;
}

static bool taiko_is_hph_dac_on(struct snd_soc_codec *codec, int left)
{
	u8 hph_reg_val = 0;
	if (left)
		hph_reg_val = snd_soc_read(codec,
					   TAIKO_A_RX_HPH_L_DAC_CTL);
	else
		hph_reg_val = snd_soc_read(codec,
					   TAIKO_A_RX_HPH_R_DAC_CTL);

	return (hph_reg_val & 0xC0) ? true : false;
}

static void taiko_turn_onoff_override(struct snd_soc_codec *codec, bool on)
{
	snd_soc_update_bits(codec, TAIKO_A_CDC_MBHC_B1_CTL, 0x04, on << 2);
}

/* called under codec_resource_lock acquisition */
static void taiko_codec_drive_v_to_micbias(struct snd_soc_codec *codec,
					   int usec)
{
	int cfilt_k_val;
	bool set = true;
	struct taiko_priv *taiko = snd_soc_codec_get_drvdata(codec);

	if (taiko->mbhc_data.micb_mv != VDDIO_MICBIAS_MV &&
	    taiko->mbhc_micbias_switched) {
		pr_debug("%s: set mic V to micbias V\n", __func__);
		snd_soc_update_bits(codec, TAIKO_A_CDC_MBHC_CLK_CTL, 0x2, 0x2);
		taiko_turn_onoff_override(codec, true);
		while (1) {
			cfilt_k_val = taiko_find_k_value(
						taiko->pdata->micbias.ldoh_v,
						set ? taiko->mbhc_data.micb_mv :
						      VDDIO_MICBIAS_MV);
			snd_soc_update_bits(codec,
					    taiko->mbhc_bias_regs.cfilt_val,
					    0xFC, (cfilt_k_val << 2));
			if (!set)
				break;
			usleep_range(usec, usec);
			set = false;
		}
		taiko_turn_onoff_override(codec, false);
	}
}

/* called under codec_resource_lock acquisition */
static void __taiko_codec_switch_micbias(struct snd_soc_codec *codec,
					 int vddio_switch, bool restartpolling,
					 bool checkpolling)
{
	int cfilt_k_val;
	bool override;
	struct taiko_priv *taiko = snd_soc_codec_get_drvdata(codec);

	if (vddio_switch && !taiko->mbhc_micbias_switched &&
	    (!checkpolling || taiko->mbhc_polling_active)) {
		if (restartpolling)
			taiko_codec_pause_hs_polling(codec);
		override = snd_soc_read(codec, TAIKO_A_CDC_MBHC_B1_CTL) & 0x04;
		if (!override)
			taiko_turn_onoff_override(codec, true);
		/* Adjust threshold if Mic Bias voltage changes */
		if (taiko->mbhc_data.micb_mv != VDDIO_MICBIAS_MV) {
			cfilt_k_val = taiko_find_k_value(
						   taiko->pdata->micbias.ldoh_v,
						   VDDIO_MICBIAS_MV);
			usleep_range(10000, 10000);
			snd_soc_update_bits(codec,
					    taiko->mbhc_bias_regs.cfilt_val,
					    0xFC, (cfilt_k_val << 2));
			usleep_range(10000, 10000);
			snd_soc_write(codec, TAIKO_A_CDC_MBHC_VOLT_B1_CTL,
				      taiko->mbhc_data.adj_v_ins_hu & 0xFF);
			snd_soc_write(codec, TAIKO_A_CDC_MBHC_VOLT_B2_CTL,
				      (taiko->mbhc_data.adj_v_ins_hu >> 8) &
				       0xFF);
			pr_debug("%s: Programmed MBHC thresholds to VDDIO\n",
				 __func__);
		}

		/* enable MIC BIAS Switch to VDDIO */
		snd_soc_update_bits(codec, taiko->mbhc_bias_regs.mbhc_reg,
				    0x80, 0x80);
		snd_soc_update_bits(codec, taiko->mbhc_bias_regs.mbhc_reg,
				    0x10, 0x00);
		if (!override)
			taiko_turn_onoff_override(codec, false);
		if (restartpolling)
			taiko_codec_start_hs_polling(codec);

		taiko->mbhc_micbias_switched = true;
		pr_debug("%s: VDDIO switch enabled\n", __func__);
	} else if (!vddio_switch && taiko->mbhc_micbias_switched) {
		if ((!checkpolling || taiko->mbhc_polling_active) &&
		    restartpolling)
			taiko_codec_pause_hs_polling(codec);
		/* Reprogram thresholds */
		if (taiko->mbhc_data.micb_mv != VDDIO_MICBIAS_MV) {
			cfilt_k_val = taiko_find_k_value(
						   taiko->pdata->micbias.ldoh_v,
						   taiko->mbhc_data.micb_mv);
			snd_soc_update_bits(codec,
					    taiko->mbhc_bias_regs.cfilt_val,
					    0xFC, (cfilt_k_val << 2));
			usleep_range(10000, 10000);
			snd_soc_write(codec, TAIKO_A_CDC_MBHC_VOLT_B1_CTL,
				      taiko->mbhc_data.v_ins_hu & 0xFF);
			snd_soc_write(codec, TAIKO_A_CDC_MBHC_VOLT_B2_CTL,
				      (taiko->mbhc_data.v_ins_hu >> 8) & 0xFF);
			pr_debug("%s: Programmed MBHC thresholds to MICBIAS\n",
				 __func__);
		}

		/* Disable MIC BIAS Switch to VDDIO */
		snd_soc_update_bits(codec, taiko->mbhc_bias_regs.mbhc_reg,
				    0x80, 0x00);
		snd_soc_update_bits(codec, taiko->mbhc_bias_regs.mbhc_reg,
				    0x10, 0x00);

		if ((!checkpolling || taiko->mbhc_polling_active) &&
		    restartpolling)
			taiko_codec_start_hs_polling(codec);

		taiko->mbhc_micbias_switched = false;
		pr_debug("%s: VDDIO switch disabled\n", __func__);
	}
}

static void taiko_codec_switch_micbias(struct snd_soc_codec *codec,
				       int vddio_switch)
{
	return __taiko_codec_switch_micbias(codec, vddio_switch, true, true);
}

static int taiko_codec_enable_micbias(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct taiko_priv *taiko = snd_soc_codec_get_drvdata(codec);
	u16 micb_int_reg;
	int micb_line;
	u8 cfilt_sel_val = 0;
	char *internal1_text = "Internal1";
	char *internal2_text = "Internal2";
	char *internal3_text = "Internal3";

	pr_debug("%s %d\n", __func__, event);
	switch (w->reg) {
	case TAIKO_A_MICB_1_CTL:
		micb_int_reg = TAIKO_A_MICB_1_INT_RBIAS;
		cfilt_sel_val = taiko->pdata->micbias.bias1_cfilt_sel;
		micb_line = TAIKO_MICBIAS1;
		break;
	case TAIKO_A_MICB_2_CTL:
		micb_int_reg = TAIKO_A_MICB_2_INT_RBIAS;
		cfilt_sel_val = taiko->pdata->micbias.bias2_cfilt_sel;
		micb_line = TAIKO_MICBIAS2;
		break;
	case TAIKO_A_MICB_3_CTL:
		micb_int_reg = TAIKO_A_MICB_3_INT_RBIAS;
		cfilt_sel_val = taiko->pdata->micbias.bias3_cfilt_sel;
		micb_line = TAIKO_MICBIAS3;
		break;
	case TAIKO_A_MICB_4_CTL:
		micb_int_reg = taiko->reg_addr.micb_4_int_rbias;
		cfilt_sel_val = taiko->pdata->micbias.bias4_cfilt_sel;
		micb_line = TAIKO_MICBIAS4;
		break;
	default:
		pr_err("%s: Error, invalid micbias register\n", __func__);
		return -EINVAL;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Decide whether to switch the micbias for MBHC */
		if (w->reg == taiko->mbhc_bias_regs.ctl_reg) {
			TAIKO_ACQUIRE_LOCK(taiko->codec_resource_lock);
			taiko_codec_switch_micbias(codec, 0);
			TAIKO_RELEASE_LOCK(taiko->codec_resource_lock);
		}

		snd_soc_update_bits(codec, w->reg, 0x0E, 0x0A);
		taiko_codec_update_cfilt_usage(codec, cfilt_sel_val, 1);

		if (strnstr(w->name, internal1_text, 30))
			snd_soc_update_bits(codec, micb_int_reg, 0xE0, 0xE0);
		else if (strnstr(w->name, internal2_text, 30))
			snd_soc_update_bits(codec, micb_int_reg, 0x1C, 0x1C);
		else if (strnstr(w->name, internal3_text, 30))
			snd_soc_update_bits(codec, micb_int_reg, 0x3, 0x3);

		break;
	case SND_SOC_DAPM_POST_PMU:

		usleep_range(20000, 20000);

		if (taiko->mbhc_polling_active &&
		    taiko->mbhc_cfg.micbias == micb_line) {
			TAIKO_ACQUIRE_LOCK(taiko->codec_resource_lock);
			taiko_codec_pause_hs_polling(codec);
			taiko_codec_start_hs_polling(codec);
			TAIKO_RELEASE_LOCK(taiko->codec_resource_lock);
		}
		break;

	case SND_SOC_DAPM_POST_PMD:
		if ((w->reg == taiko->mbhc_bias_regs.ctl_reg) &&
		    taiko_is_hph_pa_on(codec)) {
			TAIKO_ACQUIRE_LOCK(taiko->codec_resource_lock);
			taiko_codec_switch_micbias(codec, 1);
			TAIKO_RELEASE_LOCK(taiko->codec_resource_lock);
		}

		if (strnstr(w->name, internal1_text, 30))
			snd_soc_update_bits(codec, micb_int_reg, 0x80, 0x00);
		else if (strnstr(w->name, internal2_text, 30))
			snd_soc_update_bits(codec, micb_int_reg, 0x10, 0x00);
		else if (strnstr(w->name, internal3_text, 30))
			snd_soc_update_bits(codec, micb_int_reg, 0x2, 0x0);

		taiko_codec_update_cfilt_usage(codec, cfilt_sel_val, 0);
		break;
	}

	return 0;
}


static void tx_hpf_corner_freq_callback(struct work_struct *work)
{
	struct delayed_work *hpf_delayed_work;
	struct hpf_work *hpf_work;
	struct taiko_priv *taiko;
	struct snd_soc_codec *codec;
	u16 tx_mux_ctl_reg;
	u8 hpf_cut_of_freq;

	hpf_delayed_work = to_delayed_work(work);
	hpf_work = container_of(hpf_delayed_work, struct hpf_work, dwork);
	taiko = hpf_work->taiko;
	codec = hpf_work->taiko->codec;
	hpf_cut_of_freq = hpf_work->tx_hpf_cut_of_freq;

	tx_mux_ctl_reg = TAIKO_A_CDC_TX1_MUX_CTL +
			(hpf_work->decimator - 1) * 8;

	pr_debug("%s(): decimator %u hpf_cut_of_freq 0x%x\n", __func__,
		hpf_work->decimator, (unsigned int)hpf_cut_of_freq);

	snd_soc_update_bits(codec, tx_mux_ctl_reg, 0x30, hpf_cut_of_freq << 4);
}

#define  TX_MUX_CTL_CUT_OFF_FREQ_MASK	0x30
#define  CF_MIN_3DB_4HZ			0x0
#define  CF_MIN_3DB_75HZ		0x1
#define  CF_MIN_3DB_150HZ		0x2

static int taiko_codec_enable_dec(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	unsigned int decimator;
	char *dec_name = NULL;
	char *widget_name = NULL;
	char *temp;
	int ret = 0;
	u16 dec_reset_reg, tx_vol_ctl_reg, tx_mux_ctl_reg;
	u8 dec_hpf_cut_of_freq;
	int offset;


	pr_debug("%s %d\n", __func__, event);

	widget_name = kstrndup(w->name, 15, GFP_KERNEL);
	if (!widget_name)
		return -ENOMEM;
	temp = widget_name;

	dec_name = strsep(&widget_name, " ");
	widget_name = temp;
	if (!dec_name) {
		pr_err("%s: Invalid decimator = %s\n", __func__, w->name);
		ret =  -EINVAL;
		goto out;
	}

	ret = kstrtouint(strpbrk(dec_name, "123456789"), 10, &decimator);
	if (ret < 0) {
		pr_err("%s: Invalid decimator = %s\n", __func__, dec_name);
		ret =  -EINVAL;
		goto out;
	}

	pr_debug("%s(): widget = %s dec_name = %s decimator = %u\n", __func__,
			w->name, dec_name, decimator);

	if (w->reg == TAIKO_A_CDC_CLK_TX_CLK_EN_B1_CTL) {
		dec_reset_reg = TAIKO_A_CDC_CLK_TX_RESET_B1_CTL;
		offset = 0;
	} else if (w->reg == TAIKO_A_CDC_CLK_TX_CLK_EN_B2_CTL) {
		dec_reset_reg = TAIKO_A_CDC_CLK_TX_RESET_B2_CTL;
		offset = 8;
	} else {
		pr_err("%s: Error, incorrect dec\n", __func__);
		return -EINVAL;
	}

	tx_vol_ctl_reg = TAIKO_A_CDC_TX1_VOL_CTL_CFG + 8 * (decimator - 1);
	tx_mux_ctl_reg = TAIKO_A_CDC_TX1_MUX_CTL + 8 * (decimator - 1);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:

		/* Enableable TX digital mute */
		snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x01, 0x01);

		snd_soc_update_bits(codec, dec_reset_reg, 1 << w->shift,
			1 << w->shift);
		snd_soc_update_bits(codec, dec_reset_reg, 1 << w->shift, 0x0);

		dec_hpf_cut_of_freq = snd_soc_read(codec, tx_mux_ctl_reg);

		dec_hpf_cut_of_freq = (dec_hpf_cut_of_freq & 0x30) >> 4;

		tx_hpf_work[decimator - 1].tx_hpf_cut_of_freq =
			dec_hpf_cut_of_freq;

		if ((dec_hpf_cut_of_freq != CF_MIN_3DB_150HZ)) {

			/* set cut of freq to CF_MIN_3DB_150HZ (0x1); */
			snd_soc_update_bits(codec, tx_mux_ctl_reg, 0x30,
					    CF_MIN_3DB_150HZ << 4);
		}

		/* enable HPF */
		snd_soc_update_bits(codec, tx_mux_ctl_reg , 0x08, 0x00);

		break;

	case SND_SOC_DAPM_POST_PMU:

		/* Disable TX digital mute */
		snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x01, 0x00);

		if (tx_hpf_work[decimator - 1].tx_hpf_cut_of_freq !=
				CF_MIN_3DB_150HZ) {

			schedule_delayed_work(&tx_hpf_work[decimator - 1].dwork,
					msecs_to_jiffies(300));
		}
		/* apply the digital gain after the decimator is enabled*/
		if ((w->shift) < ARRAY_SIZE(rx_digital_gain_reg))
			snd_soc_write(codec,
				  tx_digital_gain_reg[w->shift + offset],
				  snd_soc_read(codec,
				  tx_digital_gain_reg[w->shift + offset])
				  );

		break;

	case SND_SOC_DAPM_PRE_PMD:

		snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x01, 0x01);
		cancel_delayed_work_sync(&tx_hpf_work[decimator - 1].dwork);
		break;

	case SND_SOC_DAPM_POST_PMD:

		snd_soc_update_bits(codec, tx_mux_ctl_reg, 0x08, 0x08);
		snd_soc_update_bits(codec, tx_mux_ctl_reg, 0x30,
			(tx_hpf_work[decimator - 1].tx_hpf_cut_of_freq) << 4);

		break;
	}
out:
	kfree(widget_name);
	return ret;
}

static int taiko_codec_reset_interpolator(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	pr_debug("%s %d %s\n", __func__, event, w->name);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, TAIKO_A_CDC_CLK_RX_RESET_CTL,
			1 << w->shift, 1 << w->shift);
		snd_soc_update_bits(codec, TAIKO_A_CDC_CLK_RX_RESET_CTL,
			1 << w->shift, 0x0);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* apply the digital gain after the interpolator is enabled*/
		if ((w->shift) < ARRAY_SIZE(rx_digital_gain_reg))
			snd_soc_write(codec,
				  rx_digital_gain_reg[w->shift],
				  snd_soc_read(codec,
				  rx_digital_gain_reg[w->shift])
				  );
		break;
	}
	return 0;
}

static int taiko_codec_enable_ldo_h(struct snd_soc_dapm_widget *w,
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

static int taiko_codec_enable_rx_bias(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	pr_debug("%s %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		taiko_enable_rx_bias(codec, 1);
		break;
	case SND_SOC_DAPM_POST_PMD:
		taiko_enable_rx_bias(codec, 0);
		break;
	}
	return 0;
}
static int taiko_hphr_dac_event(struct snd_soc_dapm_widget *w,
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

static void taiko_snd_soc_jack_report(struct taiko_priv *taiko,
				      struct snd_soc_jack *jack, int status,
				      int mask)
{
	/* XXX: wake_lock_timeout()? */
	snd_soc_jack_report_no_dapm(jack, status, mask);
}

static void hphocp_off_report(struct taiko_priv *taiko,
	u32 jack_status, int irq)
{
	struct snd_soc_codec *codec;
	if (!taiko) {
		pr_err("%s: Bad taiko private data\n", __func__);
		return;
	}

	pr_debug("%s: clear ocp status %x\n", __func__, jack_status);
	codec = taiko->codec;
	if (taiko->hph_status & jack_status) {
		taiko->hph_status &= ~jack_status;
		if (taiko->mbhc_cfg.headset_jack)
			taiko_snd_soc_jack_report(taiko,
						  taiko->mbhc_cfg.headset_jack,
						  taiko->hph_status,
						  TAIKO_JACK_MASK);
		snd_soc_update_bits(codec, TAIKO_A_RX_HPH_OCP_CTL, 0x10, 0x00);
		snd_soc_update_bits(codec, TAIKO_A_RX_HPH_OCP_CTL, 0x10, 0x10);
		/* reset retry counter as PA is turned off signifying
		 * start of new OCP detection session
		 */
		if (TAIKO_IRQ_HPH_PA_OCPL_FAULT)
			taiko->hphlocp_cnt = 0;
		else
			taiko->hphrocp_cnt = 0;
		wcd9xxx_enable_irq(codec->control_data, irq);
	}
}

static void hphlocp_off_report(struct work_struct *work)
{
	struct taiko_priv *taiko = container_of(work, struct taiko_priv,
		hphlocp_work);
	hphocp_off_report(taiko, SND_JACK_OC_HPHL, TAIKO_IRQ_HPH_PA_OCPL_FAULT);
}

static void hphrocp_off_report(struct work_struct *work)
{
	struct taiko_priv *taiko = container_of(work, struct taiko_priv,
		hphrocp_work);
	hphocp_off_report(taiko, SND_JACK_OC_HPHR, TAIKO_IRQ_HPH_PA_OCPR_FAULT);
}

static int taiko_hph_pa_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct taiko_priv *taiko = snd_soc_codec_get_drvdata(codec);
	u8 mbhc_micb_ctl_val;
	pr_debug("%s: %s event = %d\n", __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		mbhc_micb_ctl_val = snd_soc_read(codec,
				taiko->mbhc_bias_regs.ctl_reg);

		if (!(mbhc_micb_ctl_val & 0x80)) {
			TAIKO_ACQUIRE_LOCK(taiko->codec_resource_lock);
			taiko_codec_switch_micbias(codec, 1);
			TAIKO_RELEASE_LOCK(taiko->codec_resource_lock);
		}
		break;

	case SND_SOC_DAPM_POST_PMU:

		usleep_range(10000, 10000);

		snd_soc_update_bits(codec, TAIKO_A_BUCK_MODE_5, 0x02, 0x00);
		snd_soc_update_bits(codec, TAIKO_A_NCP_STATIC, 0x20, 0x00);
		snd_soc_update_bits(codec, TAIKO_A_BUCK_MODE_3, 0x04, 0x04);
		snd_soc_update_bits(codec, TAIKO_A_BUCK_MODE_3, 0x08, 0x00);

		usleep_range(10, 10);

		break;

	case SND_SOC_DAPM_POST_PMD:
		/* schedule work is required because at the time HPH PA DAPM
		 * event callback is called by DAPM framework, CODEC dapm mutex
		 * would have been locked while snd_soc_jack_report also
		 * attempts to acquire same lock.
		 */
		if (w->shift == 5) {
			clear_bit(TAIKO_HPHL_PA_OFF_ACK,
				  &taiko->hph_pa_dac_state);
			clear_bit(TAIKO_HPHL_DAC_OFF_ACK,
				  &taiko->hph_pa_dac_state);
			if (taiko->hph_status & SND_JACK_OC_HPHL)
				schedule_work(&taiko->hphlocp_work);
		} else if (w->shift == 4) {
			clear_bit(TAIKO_HPHR_PA_OFF_ACK,
				  &taiko->hph_pa_dac_state);
			clear_bit(TAIKO_HPHR_DAC_OFF_ACK,
				  &taiko->hph_pa_dac_state);
			if (taiko->hph_status & SND_JACK_OC_HPHR)
				schedule_work(&taiko->hphrocp_work);
		}

		TAIKO_ACQUIRE_LOCK(taiko->codec_resource_lock);
		taiko_codec_switch_micbias(codec, 0);
		TAIKO_RELEASE_LOCK(taiko->codec_resource_lock);

		pr_debug("%s: sleep 10 ms after %s PA disable.\n", __func__,
				w->name);
		usleep_range(10000, 10000);
		break;
	}
	return 0;
}

static void taiko_get_mbhc_micbias_regs(struct snd_soc_codec *codec,
					struct mbhc_micbias_regs *micbias_regs)
{
	struct taiko_priv *taiko = snd_soc_codec_get_drvdata(codec);
	unsigned int cfilt;

	switch (taiko->mbhc_cfg.micbias) {
	case TAIKO_MICBIAS1:
		cfilt = taiko->pdata->micbias.bias1_cfilt_sel;
		micbias_regs->mbhc_reg = TAIKO_A_MICB_1_MBHC;
		micbias_regs->int_rbias = TAIKO_A_MICB_1_INT_RBIAS;
		micbias_regs->ctl_reg = TAIKO_A_MICB_1_CTL;
		break;
	case TAIKO_MICBIAS2:
		cfilt = taiko->pdata->micbias.bias2_cfilt_sel;
		micbias_regs->mbhc_reg = TAIKO_A_MICB_2_MBHC;
		micbias_regs->int_rbias = TAIKO_A_MICB_2_INT_RBIAS;
		micbias_regs->ctl_reg = TAIKO_A_MICB_2_CTL;
		break;
	case TAIKO_MICBIAS3:
		cfilt = taiko->pdata->micbias.bias3_cfilt_sel;
		micbias_regs->mbhc_reg = TAIKO_A_MICB_3_MBHC;
		micbias_regs->int_rbias = TAIKO_A_MICB_3_INT_RBIAS;
		micbias_regs->ctl_reg = TAIKO_A_MICB_3_CTL;
		break;
	case TAIKO_MICBIAS4:
		cfilt = taiko->pdata->micbias.bias4_cfilt_sel;
		micbias_regs->mbhc_reg = taiko->reg_addr.micb_4_mbhc;
		micbias_regs->int_rbias = taiko->reg_addr.micb_4_int_rbias;
		micbias_regs->ctl_reg = taiko->reg_addr.micb_4_ctl;
		break;
	default:
		/* Should never reach here */
		pr_err("%s: Invalid MIC BIAS for MBHC\n", __func__);
		return;
	}

	micbias_regs->cfilt_sel = cfilt;

	switch (cfilt) {
	case TAIKO_CFILT1_SEL:
		micbias_regs->cfilt_val = TAIKO_A_MICB_CFILT_1_VAL;
		micbias_regs->cfilt_ctl = TAIKO_A_MICB_CFILT_1_CTL;
		taiko->mbhc_data.micb_mv = taiko->pdata->micbias.cfilt1_mv;
		break;
	case TAIKO_CFILT2_SEL:
		micbias_regs->cfilt_val = TAIKO_A_MICB_CFILT_2_VAL;
		micbias_regs->cfilt_ctl = TAIKO_A_MICB_CFILT_2_CTL;
		taiko->mbhc_data.micb_mv = taiko->pdata->micbias.cfilt2_mv;
		break;
	case TAIKO_CFILT3_SEL:
		micbias_regs->cfilt_val = TAIKO_A_MICB_CFILT_3_VAL;
		micbias_regs->cfilt_ctl = TAIKO_A_MICB_CFILT_3_CTL;
		taiko->mbhc_data.micb_mv = taiko->pdata->micbias.cfilt3_mv;
		break;
	}
}
static const struct snd_soc_dapm_widget taiko_dapm_i2s_widgets[] = {
	SND_SOC_DAPM_SUPPLY("RX_I2S_CLK", TAIKO_A_CDC_CLK_RX_I2S_CTL,
	4, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("TX_I2S_CLK", TAIKO_A_CDC_CLK_TX_I2S_CTL, 4,
	0, NULL, 0),
};

static int taiko_lineout_dac_event(struct snd_soc_dapm_widget *w,
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

static int taiko_spk_dac_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	pr_debug("%s %s %d\n", __func__, w->name, event);
	return 0;
}

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

	{"SLIM TX2", NULL, "SLIM TX2 MUX"},
	{"SLIM TX2 MUX", "DEC2", "DEC2 MUX"},

	{"SLIM TX3", NULL, "SLIM TX3 MUX"},
	{"SLIM TX3 MUX", "DEC3", "DEC3 MUX"},
	{"SLIM TX3 MUX", "RMIX1", "RX1 MIX1"},
	{"SLIM TX3 MUX", "RMIX2", "RX2 MIX1"},
	{"SLIM TX3 MUX", "RMIX3", "RX3 MIX1"},
	{"SLIM TX3 MUX", "RMIX4", "RX4 MIX1"},
	{"SLIM TX3 MUX", "RMIX5", "RX5 MIX1"},
	{"SLIM TX3 MUX", "RMIX6", "RX6 MIX1"},
	{"SLIM TX3 MUX", "RMIX7", "RX7 MIX1"},

	{"SLIM TX4", NULL, "SLIM TX4 MUX"},
	{"SLIM TX4 MUX", "DEC4", "DEC4 MUX"},

	{"SLIM TX5", NULL, "SLIM TX5 MUX"},
	{"SLIM TX5 MUX", "DEC5", "DEC5 MUX"},
	{"SLIM TX5 MUX", "RMIX1", "RX1 MIX1"},
	{"SLIM TX5 MUX", "RMIX2", "RX2 MIX1"},
	{"SLIM TX5 MUX", "RMIX3", "RX3 MIX1"},
	{"SLIM TX5 MUX", "RMIX4", "RX4 MIX1"},
	{"SLIM TX5 MUX", "RMIX5", "RX5 MIX1"},
	{"SLIM TX5 MUX", "RMIX6", "RX6 MIX1"},
	{"SLIM TX5 MUX", "RMIX7", "RX7 MIX1"},

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
	{"SLIM TX7 MUX", "RMIX1", "RX1 MIX1"},
	{"SLIM TX7 MUX", "RMIX2", "RX2 MIX1"},
	{"SLIM TX7 MUX", "RMIX3", "RX3 MIX1"},
	{"SLIM TX7 MUX", "RMIX4", "RX4 MIX1"},
	{"SLIM TX7 MUX", "RMIX5", "RX5 MIX1"},
	{"SLIM TX7 MUX", "RMIX6", "RX6 MIX1"},
	{"SLIM TX7 MUX", "RMIX7", "RX7 MIX1"},

	{"SLIM TX8", NULL, "SLIM TX8 MUX"},
	{"SLIM TX8 MUX", "DEC1", "DEC1 MUX"},
	{"SLIM TX8 MUX", "DEC2", "DEC2 MUX"},
	{"SLIM TX8 MUX", "DEC3", "DEC3 MUX"},
	{"SLIM TX8 MUX", "DEC4", "DEC4 MUX"},
	{"SLIM TX8 MUX", "DEC5", "DEC5 MUX"},
	{"SLIM TX8 MUX", "DEC6", "DEC6 MUX"},
	{"SLIM TX8 MUX", "DEC7", "DEC7 MUX"},
	{"SLIM TX8 MUX", "DEC8", "DEC8 MUX"},
	{"SLIM TX8 MUX", "DEC9", "DEC9 MUX"},
	{"SLIM TX8 MUX", "DEC10", "DEC10 MUX"},

	{"SLIM TX9", NULL, "SLIM TX9 MUX"},
	{"SLIM TX9 MUX", "DEC1", "DEC1 MUX"},
	{"SLIM TX9 MUX", "DEC2", "DEC2 MUX"},
	{"SLIM TX9 MUX", "DEC3", "DEC3 MUX"},
	{"SLIM TX9 MUX", "DEC4", "DEC4 MUX"},
	{"SLIM TX9 MUX", "DEC5", "DEC5 MUX"},
	{"SLIM TX9 MUX", "DEC6", "DEC6 MUX"},
	{"SLIM TX9 MUX", "DEC7", "DEC7 MUX"},
	{"SLIM TX9 MUX", "DEC8", "DEC8 MUX"},
	{"SLIM TX9 MUX", "DEC9", "DEC9 MUX"},
	{"SLIM TX9 MUX", "DEC10", "DEC10 MUX"},

	{"SLIM TX10", NULL, "SLIM TX10 MUX"},
	{"SLIM TX10 MUX", "DEC1", "DEC1 MUX"},
	{"SLIM TX10 MUX", "DEC2", "DEC2 MUX"},
	{"SLIM TX10 MUX", "DEC3", "DEC3 MUX"},
	{"SLIM TX10 MUX", "DEC4", "DEC4 MUX"},
	{"SLIM TX10 MUX", "DEC5", "DEC5 MUX"},
	{"SLIM TX10 MUX", "DEC6", "DEC6 MUX"},
	{"SLIM TX10 MUX", "DEC7", "DEC7 MUX"},
	{"SLIM TX10 MUX", "DEC8", "DEC8 MUX"},
	{"SLIM TX10 MUX", "DEC9", "DEC9 MUX"},
	{"SLIM TX10 MUX", "DEC10", "DEC10 MUX"},

	/* Earpiece (RX MIX1) */
	{"EAR", NULL, "EAR PA"},
	{"EAR PA", NULL, "EAR_PA_MIXER"},
	{"EAR_PA_MIXER", NULL, "DAC1"},
	{"DAC1", NULL, "CP"},
	{"CP", NULL, "CLASS_H_EAR"},
	{"CLASS_H_EAR", NULL, "CLASS_H_CLK"},

	{"ANC1 FB MUX", "EAR_HPH_L", "RX1 MIX2"},
	{"ANC1 FB MUX", "EAR_LINE_1", "RX2 MIX2"},
	{"ANC", NULL, "ANC1 FB MUX"},

	/* Headset (RX MIX1 and RX MIX2) */
	{"HEADPHONE", NULL, "HPHL"},
	{"HEADPHONE", NULL, "HPHR"},

	{"HPHL", NULL, "HPHL_PA_MIXER"},
	{"HPHL_PA_MIXER", NULL, "HPHL DAC"},

	{"HPHR", NULL, "HPHR_PA_MIXER"},
	{"HPHR_PA_MIXER", NULL, "HPHR DAC"},

	{"HPHL DAC", NULL, "CP"},
	{"CP", NULL, "CLASS_H_HPH_L"},
	{"CLASS_H_HPH_L", NULL, "CLASS_H_CLK"},

	{"HPHR DAC", NULL, "CP"},
	{"CP", NULL, "CLASS_H_HPH_R"},
	{"CLASS_H_HPH_R", NULL, "CLASS_H_CLK"},

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
	{"SPK_OUT", NULL, "SPK PA"},

	{"LINEOUT1 PA", NULL, "LINEOUT1_PA_MIXER"},
	{"LINEOUT1_PA_MIXER", NULL, "LINEOUT1 DAC"},
	{"LINEOUT2 PA", NULL, "LINEOUT2_PA_MIXER"},
	{"LINEOUT2_PA_MIXER", NULL, "LINEOUT2 DAC"},
	{"LINEOUT3 PA", NULL, "LINEOUT3_PA_MIXER"},
	{"LINEOUT3_PA_MIXER", NULL, "LINEOUT3 DAC"},
	{"LINEOUT4 PA", NULL, "LINEOUT4_PA_MIXER"},
	{"LINEOUT4_PA_MIXER", NULL, "LINEOUT4 DAC"},

	{"LINEOUT1 DAC", NULL, "RX3 MIX1"},

	{"RX4 DSM MUX", "DSM_INV", "RX3 MIX1"},
	{"RX4 DSM MUX", "CIC_OUT", "RX4 MIX1"},
	{"LINEOUT3 DAC", NULL, "RX4 DSM MUX"},

	{"LINEOUT2 DAC", NULL, "RX5 MIX1"},

	{"RX6 DSM MUX", "DSM_INV", "RX5 MIX1"},
	{"RX6 DSM MUX", "CIC_OUT", "RX6 MIX1"},
	{"LINEOUT4 DAC", NULL, "RX6 DSM MUX"},

	{"SPK PA", NULL, "SPK DAC"},
	{"SPK DAC", NULL, "RX7 MIX1"},

	{"RX1 CHAIN", NULL, "RX1 MIX2"},
	{"RX2 CHAIN", NULL, "RX2 MIX2"},
	{"RX1 CHAIN", NULL, "ANC"},
	{"RX2 CHAIN", NULL, "ANC"},

	{"CLASS_H_CLK", NULL, "RX_BIAS"},
	{"LINEOUT1 DAC", NULL, "RX_BIAS"},
	{"LINEOUT2 DAC", NULL, "RX_BIAS"},
	{"LINEOUT3 DAC", NULL, "RX_BIAS"},
	{"LINEOUT4 DAC", NULL, "RX_BIAS"},
	{"SPK DAC", NULL, "RX_BIAS"},

	{"RX1 MIX1", NULL, "COMP1_CLK"},
	{"RX2 MIX1", NULL, "COMP1_CLK"},
	{"RX3 MIX1", NULL, "COMP2_CLK"},
	{"RX5 MIX1", NULL, "COMP2_CLK"},

	{"RX1 MIX1", NULL, "RX1 MIX1 INP1"},
	{"RX1 MIX1", NULL, "RX1 MIX1 INP2"},
	{"RX1 MIX1", NULL, "RX1 MIX1 INP3"},
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
	{"RX1 MIX2", NULL, "RX1 MIX1"},
	{"RX1 MIX2", NULL, "RX1 MIX2 INP1"},
	{"RX1 MIX2", NULL, "RX1 MIX2 INP2"},
	{"RX2 MIX2", NULL, "RX2 MIX1"},
	{"RX2 MIX2", NULL, "RX2 MIX2 INP1"},
	{"RX2 MIX2", NULL, "RX2 MIX2 INP2"},
	{"RX7 MIX2", NULL, "RX7 MIX1"},
	{"RX7 MIX2", NULL, "RX7 MIX2 INP1"},
	{"RX7 MIX2", NULL, "RX7 MIX2 INP2"},

	{"RX1 MIX1 INP1", "RX1", "SLIM RX1"},
	{"RX1 MIX1 INP1", "RX2", "SLIM RX2"},
	{"RX1 MIX1 INP1", "RX3", "SLIM RX3"},
	{"RX1 MIX1 INP1", "RX4", "SLIM RX4"},
	{"RX1 MIX1 INP1", "RX5", "SLIM RX5"},
	{"RX1 MIX1 INP1", "RX6", "SLIM RX6"},
	{"RX1 MIX1 INP1", "RX7", "SLIM RX7"},
	{"RX1 MIX1 INP1", "IIR1", "IIR1"},
	{"RX1 MIX1 INP2", "RX1", "SLIM RX1"},
	{"RX1 MIX1 INP2", "RX2", "SLIM RX2"},
	{"RX1 MIX1 INP2", "RX3", "SLIM RX3"},
	{"RX1 MIX1 INP2", "RX4", "SLIM RX4"},
	{"RX1 MIX1 INP2", "RX5", "SLIM RX5"},
	{"RX1 MIX1 INP2", "RX6", "SLIM RX6"},
	{"RX1 MIX1 INP2", "RX7", "SLIM RX7"},
	{"RX1 MIX1 INP2", "IIR1", "IIR1"},
	{"RX1 MIX1 INP3", "RX1", "SLIM RX1"},
	{"RX1 MIX1 INP3", "RX2", "SLIM RX2"},
	{"RX1 MIX1 INP3", "RX3", "SLIM RX3"},
	{"RX1 MIX1 INP3", "RX4", "SLIM RX4"},
	{"RX1 MIX1 INP3", "RX5", "SLIM RX5"},
	{"RX1 MIX1 INP3", "RX6", "SLIM RX6"},
	{"RX1 MIX1 INP3", "RX7", "SLIM RX7"},
	{"RX2 MIX1 INP1", "RX1", "SLIM RX1"},
	{"RX2 MIX1 INP1", "RX2", "SLIM RX2"},
	{"RX2 MIX1 INP1", "RX3", "SLIM RX3"},
	{"RX2 MIX1 INP1", "RX4", "SLIM RX4"},
	{"RX2 MIX1 INP1", "RX5", "SLIM RX5"},
	{"RX2 MIX1 INP1", "RX6", "SLIM RX6"},
	{"RX2 MIX1 INP1", "RX7", "SLIM RX7"},
	{"RX2 MIX1 INP1", "IIR1", "IIR1"},
	{"RX2 MIX1 INP2", "RX1", "SLIM RX1"},
	{"RX2 MIX1 INP2", "RX2", "SLIM RX2"},
	{"RX2 MIX1 INP2", "RX3", "SLIM RX3"},
	{"RX2 MIX1 INP2", "RX4", "SLIM RX4"},
	{"RX2 MIX1 INP2", "RX5", "SLIM RX5"},
	{"RX2 MIX1 INP2", "RX6", "SLIM RX6"},
	{"RX2 MIX1 INP2", "RX7", "SLIM RX7"},
	{"RX2 MIX1 INP2", "IIR1", "IIR1"},
	{"RX3 MIX1 INP1", "RX1", "SLIM RX1"},
	{"RX3 MIX1 INP1", "RX2", "SLIM RX2"},
	{"RX3 MIX1 INP1", "RX3", "SLIM RX3"},
	{"RX3 MIX1 INP1", "RX4", "SLIM RX4"},
	{"RX3 MIX1 INP1", "RX5", "SLIM RX5"},
	{"RX3 MIX1 INP1", "RX6", "SLIM RX6"},
	{"RX3 MIX1 INP1", "RX7", "SLIM RX7"},
	{"RX3 MIX1 INP1", "IIR1", "IIR1"},
	{"RX3 MIX1 INP2", "RX1", "SLIM RX1"},
	{"RX3 MIX1 INP2", "RX2", "SLIM RX2"},
	{"RX3 MIX1 INP2", "RX3", "SLIM RX3"},
	{"RX3 MIX1 INP2", "RX4", "SLIM RX4"},
	{"RX3 MIX1 INP2", "RX5", "SLIM RX5"},
	{"RX3 MIX1 INP2", "RX6", "SLIM RX6"},
	{"RX3 MIX1 INP2", "RX7", "SLIM RX7"},
	{"RX3 MIX1 INP2", "IIR1", "IIR1"},
	{"RX4 MIX1 INP1", "RX1", "SLIM RX1"},
	{"RX4 MIX1 INP1", "RX2", "SLIM RX2"},
	{"RX4 MIX1 INP1", "RX3", "SLIM RX3"},
	{"RX4 MIX1 INP1", "RX4", "SLIM RX4"},
	{"RX4 MIX1 INP1", "RX5", "SLIM RX5"},
	{"RX4 MIX1 INP1", "RX6", "SLIM RX6"},
	{"RX4 MIX1 INP1", "RX7", "SLIM RX7"},
	{"RX4 MIX1 INP1", "IIR1", "IIR1"},
	{"RX4 MIX1 INP2", "RX1", "SLIM RX1"},
	{"RX4 MIX1 INP2", "RX2", "SLIM RX2"},
	{"RX4 MIX1 INP2", "RX3", "SLIM RX3"},
	{"RX4 MIX1 INP2", "RX5", "SLIM RX5"},
	{"RX4 MIX1 INP2", "RX4", "SLIM RX4"},
	{"RX4 MIX1 INP2", "RX6", "SLIM RX6"},
	{"RX4 MIX1 INP2", "RX7", "SLIM RX7"},
	{"RX4 MIX1 INP2", "IIR1", "IIR1"},
	{"RX5 MIX1 INP1", "RX1", "SLIM RX1"},
	{"RX5 MIX1 INP1", "RX2", "SLIM RX2"},
	{"RX5 MIX1 INP1", "RX3", "SLIM RX3"},
	{"RX5 MIX1 INP1", "RX4", "SLIM RX4"},
	{"RX5 MIX1 INP1", "RX5", "SLIM RX5"},
	{"RX5 MIX1 INP1", "RX6", "SLIM RX6"},
	{"RX5 MIX1 INP1", "RX7", "SLIM RX7"},
	{"RX5 MIX1 INP1", "IIR1", "IIR1"},
	{"RX5 MIX1 INP2", "RX1", "SLIM RX1"},
	{"RX5 MIX1 INP2", "RX2", "SLIM RX2"},
	{"RX5 MIX1 INP2", "RX3", "SLIM RX3"},
	{"RX5 MIX1 INP2", "RX4", "SLIM RX4"},
	{"RX5 MIX1 INP2", "RX5", "SLIM RX5"},
	{"RX5 MIX1 INP2", "RX6", "SLIM RX6"},
	{"RX5 MIX1 INP2", "RX7", "SLIM RX7"},
	{"RX5 MIX1 INP2", "IIR1", "IIR1"},
	{"RX6 MIX1 INP1", "RX1", "SLIM RX1"},
	{"RX6 MIX1 INP1", "RX2", "SLIM RX2"},
	{"RX6 MIX1 INP1", "RX3", "SLIM RX3"},
	{"RX6 MIX1 INP1", "RX4", "SLIM RX4"},
	{"RX6 MIX1 INP1", "RX5", "SLIM RX5"},
	{"RX6 MIX1 INP1", "RX6", "SLIM RX6"},
	{"RX6 MIX1 INP1", "RX7", "SLIM RX7"},
	{"RX6 MIX1 INP1", "IIR1", "IIR1"},
	{"RX6 MIX1 INP2", "RX1", "SLIM RX1"},
	{"RX6 MIX1 INP2", "RX2", "SLIM RX2"},
	{"RX6 MIX1 INP2", "RX3", "SLIM RX3"},
	{"RX6 MIX1 INP2", "RX4", "SLIM RX4"},
	{"RX6 MIX1 INP2", "RX5", "SLIM RX5"},
	{"RX6 MIX1 INP2", "RX6", "SLIM RX6"},
	{"RX6 MIX1 INP2", "RX7", "SLIM RX7"},
	{"RX6 MIX1 INP2", "IIR1", "IIR1"},
	{"RX7 MIX1 INP1", "RX1", "SLIM RX1"},
	{"RX7 MIX1 INP1", "RX2", "SLIM RX2"},
	{"RX7 MIX1 INP1", "RX3", "SLIM RX3"},
	{"RX7 MIX1 INP1", "RX4", "SLIM RX4"},
	{"RX7 MIX1 INP1", "RX5", "SLIM RX5"},
	{"RX7 MIX1 INP1", "RX6", "SLIM RX6"},
	{"RX7 MIX1 INP1", "RX7", "SLIM RX7"},
	{"RX7 MIX1 INP1", "IIR1", "IIR1"},
	{"RX7 MIX1 INP2", "RX1", "SLIM RX1"},
	{"RX7 MIX1 INP2", "RX2", "SLIM RX2"},
	{"RX7 MIX1 INP2", "RX3", "SLIM RX3"},
	{"RX7 MIX1 INP2", "RX4", "SLIM RX4"},
	{"RX7 MIX1 INP2", "RX5", "SLIM RX5"},
	{"RX7 MIX1 INP2", "RX6", "SLIM RX6"},
	{"RX7 MIX1 INP2", "RX7", "SLIM RX7"},
	{"RX7 MIX1 INP2", "IIR1", "IIR1"},
	{"RX1 MIX2 INP1", "IIR1", "IIR1"},
	{"RX1 MIX2 INP2", "IIR1", "IIR1"},
	{"RX2 MIX2 INP1", "IIR1", "IIR1"},
	{"RX2 MIX2 INP2", "IIR1", "IIR1"},
	{"RX7 MIX2 INP1", "IIR1", "IIR1"},
	{"RX7 MIX2 INP2", "IIR1", "IIR1"},

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
	{"DEC7 MUX", "DMIC6", "DMIC6"},
	{"DEC7 MUX", "ADC1", "ADC1"},
	{"DEC7 MUX", "ADC6", "ADC6"},
	{"DEC7 MUX", NULL, "CDC_CONN"},
	{"DEC8 MUX", "DMIC2", "DMIC2"},
	{"DEC8 MUX", "DMIC5", "DMIC5"},
	{"DEC8 MUX", "ADC2", "ADC2"},
	{"DEC8 MUX", "ADC5", "ADC5"},
	{"DEC8 MUX", NULL, "CDC_CONN"},
	{"DEC9 MUX", "DMIC4", "DMIC4"},
	{"DEC9 MUX", "DMIC5", "DMIC5"},
	{"DEC9 MUX", "ADC2", "ADC2"},
	{"DEC9 MUX", "ADC3", "ADC3"},
	{"DEC9 MUX", NULL, "CDC_CONN"},
	{"DEC10 MUX", "DMIC3", "DMIC3"},
	{"DEC10 MUX", "DMIC6", "DMIC6"},
	{"DEC10 MUX", "ADC1", "ADC1"},
	{"DEC10 MUX", "ADC4", "ADC4"},
	{"DEC10 MUX", NULL, "CDC_CONN"},

	/* ADC Connections */
	{"ADC1", NULL, "AMIC1"},
	{"ADC2", NULL, "AMIC2"},
	{"ADC3", NULL, "AMIC3"},
	{"ADC4", NULL, "AMIC4"},
	{"ADC5", NULL, "AMIC5"},
	{"ADC6", NULL, "AMIC6"},

	/* AUX PGA Connections */
	{"EAR_PA_MIXER", "AUX_PGA_L Switch", "AUX_PGA_Left"},
	{"HPHL_PA_MIXER", "AUX_PGA_L Switch", "AUX_PGA_Left"},
	{"HPHR_PA_MIXER", "AUX_PGA_R Switch", "AUX_PGA_Right"},
	{"LINEOUT1_PA_MIXER", "AUX_PGA_L Switch", "AUX_PGA_Left"},
	{"LINEOUT2_PA_MIXER", "AUX_PGA_R Switch", "AUX_PGA_Right"},
	{"LINEOUT3_PA_MIXER", "AUX_PGA_L Switch", "AUX_PGA_Left"},
	{"LINEOUT4_PA_MIXER", "AUX_PGA_R Switch", "AUX_PGA_Right"},
	{"AUX_PGA_Left", NULL, "AMIC5"},
	{"AUX_PGA_Right", NULL, "AMIC6"},

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

static int taiko_readable(struct snd_soc_codec *ssc, unsigned int reg)
{
	return taiko_reg_readable[reg];
}

static bool taiko_is_digital_gain_register(unsigned int reg)
{
	bool rtn = false;
	switch (reg) {
	case TAIKO_A_CDC_RX1_VOL_CTL_B2_CTL:
	case TAIKO_A_CDC_RX2_VOL_CTL_B2_CTL:
	case TAIKO_A_CDC_RX3_VOL_CTL_B2_CTL:
	case TAIKO_A_CDC_RX4_VOL_CTL_B2_CTL:
	case TAIKO_A_CDC_RX5_VOL_CTL_B2_CTL:
	case TAIKO_A_CDC_RX6_VOL_CTL_B2_CTL:
	case TAIKO_A_CDC_RX7_VOL_CTL_B2_CTL:
	case TAIKO_A_CDC_TX1_VOL_CTL_GAIN:
	case TAIKO_A_CDC_TX2_VOL_CTL_GAIN:
	case TAIKO_A_CDC_TX3_VOL_CTL_GAIN:
	case TAIKO_A_CDC_TX4_VOL_CTL_GAIN:
	case TAIKO_A_CDC_TX5_VOL_CTL_GAIN:
	case TAIKO_A_CDC_TX6_VOL_CTL_GAIN:
	case TAIKO_A_CDC_TX7_VOL_CTL_GAIN:
	case TAIKO_A_CDC_TX8_VOL_CTL_GAIN:
	case TAIKO_A_CDC_TX9_VOL_CTL_GAIN:
	case TAIKO_A_CDC_TX10_VOL_CTL_GAIN:
		rtn = true;
		break;
	default:
		break;
	}
	return rtn;
}

static int taiko_volatile(struct snd_soc_codec *ssc, unsigned int reg)
{
	/* Registers lower than 0x100 are top level registers which can be
	 * written by the Taiko core driver.
	 */

	if ((reg >= TAIKO_A_CDC_MBHC_EN_CTL) || (reg < 0x100))
		return 1;

	/* IIR Coeff registers are not cacheable */
	if ((reg >= TAIKO_A_CDC_IIR1_COEF_B1_CTL) &&
		(reg <= TAIKO_A_CDC_IIR2_COEF_B2_CTL))
		return 1;

	/* Digital gain register is not cacheable so we have to write
	 * the setting even it is the same
	 */
	if (taiko_is_digital_gain_register(reg))
		return 1;

	/* HPH status registers */
	if (reg == TAIKO_A_RX_HPH_L_STATUS || reg == TAIKO_A_RX_HPH_R_STATUS)
		return 1;

	return 0;
}

#define TAIKO_FORMATS (SNDRV_PCM_FMTBIT_S16_LE)
static int taiko_write(struct snd_soc_codec *codec, unsigned int reg,
	unsigned int value)
{
	int ret;
	BUG_ON(reg > TAIKO_MAX_REGISTER);

	if (!taiko_volatile(codec, reg)) {
		ret = snd_soc_cache_write(codec, reg, value);
		if (ret != 0)
			dev_err(codec->dev, "Cache write to %x failed: %d\n",
				reg, ret);
	}

	return wcd9xxx_reg_write(codec->control_data, reg, value);
}
static unsigned int taiko_read(struct snd_soc_codec *codec,
				unsigned int reg)
{
	unsigned int val;
	int ret;

	BUG_ON(reg > TAIKO_MAX_REGISTER);

	if (!taiko_volatile(codec, reg) && taiko_readable(codec, reg) &&
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

static s16 taiko_get_current_v_ins(struct taiko_priv *taiko, bool hu)
{
	s16 v_ins;
	if ((taiko->mbhc_data.micb_mv != VDDIO_MICBIAS_MV) &&
	    taiko->mbhc_micbias_switched)
		v_ins = hu ? (s16)taiko->mbhc_data.adj_v_ins_hu :
			     (s16)taiko->mbhc_data.adj_v_ins_h;
	else
		v_ins = hu ? (s16)taiko->mbhc_data.v_ins_hu :
			     (s16)taiko->mbhc_data.v_ins_h;
	return v_ins;
}

static s16 taiko_get_current_v_hs_max(struct taiko_priv *taiko)
{
	s16 v_hs_max;
	struct taiko_mbhc_plug_type_cfg *plug_type;

	plug_type = TAIKO_MBHC_CAL_PLUG_TYPE_PTR(taiko->mbhc_cfg.calibration);
	if ((taiko->mbhc_data.micb_mv != VDDIO_MICBIAS_MV) &&
	    taiko->mbhc_micbias_switched)
		v_hs_max = taiko->mbhc_data.adj_v_hs_max;
	else
		v_hs_max = plug_type->v_hs_max;
	return v_hs_max;
}

static void taiko_codec_calibrate_hs_polling(struct snd_soc_codec *codec)
{
	u8 *n_ready, *n_cic;
	struct taiko_mbhc_btn_detect_cfg *btn_det;
	struct taiko_priv *taiko = snd_soc_codec_get_drvdata(codec);
	const s16 v_ins_hu = taiko_get_current_v_ins(taiko, true);

	btn_det = TAIKO_MBHC_CAL_BTN_DET_PTR(taiko->mbhc_cfg.calibration);

	snd_soc_write(codec, TAIKO_A_CDC_MBHC_VOLT_B1_CTL,
		      v_ins_hu & 0xFF);
	snd_soc_write(codec, TAIKO_A_CDC_MBHC_VOLT_B2_CTL,
		      (v_ins_hu >> 8) & 0xFF);

	snd_soc_write(codec, TAIKO_A_CDC_MBHC_VOLT_B3_CTL,
		      taiko->mbhc_data.v_b1_hu & 0xFF);
	snd_soc_write(codec, TAIKO_A_CDC_MBHC_VOLT_B4_CTL,
		      (taiko->mbhc_data.v_b1_hu >> 8) & 0xFF);

	snd_soc_write(codec, TAIKO_A_CDC_MBHC_VOLT_B5_CTL,
		      taiko->mbhc_data.v_b1_h & 0xFF);
	snd_soc_write(codec, TAIKO_A_CDC_MBHC_VOLT_B6_CTL,
		      (taiko->mbhc_data.v_b1_h >> 8) & 0xFF);

	snd_soc_write(codec, TAIKO_A_CDC_MBHC_VOLT_B9_CTL,
		      taiko->mbhc_data.v_brh & 0xFF);
	snd_soc_write(codec, TAIKO_A_CDC_MBHC_VOLT_B10_CTL,
		      (taiko->mbhc_data.v_brh >> 8) & 0xFF);

	snd_soc_write(codec, TAIKO_A_CDC_MBHC_VOLT_B11_CTL,
		      taiko->mbhc_data.v_brl & 0xFF);
	snd_soc_write(codec, TAIKO_A_CDC_MBHC_VOLT_B12_CTL,
		      (taiko->mbhc_data.v_brl >> 8) & 0xFF);

	n_ready = taiko_mbhc_cal_btn_det_mp(btn_det, TAIKO_BTN_DET_N_READY);
	snd_soc_write(codec, TAIKO_A_CDC_MBHC_TIMER_B1_CTL,
		      n_ready[taiko_codec_mclk_index(taiko)]);
	snd_soc_write(codec, TAIKO_A_CDC_MBHC_TIMER_B2_CTL,
		      taiko->mbhc_data.npoll);
	snd_soc_write(codec, TAIKO_A_CDC_MBHC_TIMER_B3_CTL,
		      taiko->mbhc_data.nbounce_wait);
	n_cic = taiko_mbhc_cal_btn_det_mp(btn_det, TAIKO_BTN_DET_N_CIC);
	snd_soc_write(codec, TAIKO_A_CDC_MBHC_TIMER_B6_CTL,
		      n_cic[taiko_codec_mclk_index(taiko)]);
}

static int taiko_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct wcd9xxx *taiko_core = dev_get_drvdata(dai->codec->dev->parent);
	pr_debug("%s(): substream = %s  stream = %d\n" , __func__,
		 substream->name, substream->stream);
	if ((taiko_core != NULL) &&
	    (taiko_core->dev != NULL) &&
	    (taiko_core->dev->parent != NULL))
		pm_runtime_get_sync(taiko_core->dev->parent);

	return 0;
}

static void taiko_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct wcd9xxx *taiko_core = dev_get_drvdata(dai->codec->dev->parent);
	pr_debug("%s(): substream = %s  stream = %d\n" , __func__,
		 substream->name, substream->stream);
	if ((taiko_core != NULL) &&
	    (taiko_core->dev != NULL) &&
	    (taiko_core->dev->parent != NULL)) {
		pm_runtime_mark_last_busy(taiko_core->dev->parent);
		pm_runtime_put(taiko_core->dev->parent);
	}
}

int taiko_mclk_enable(struct snd_soc_codec *codec, int mclk_enable, bool dapm)
{
	struct taiko_priv *taiko = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s: mclk_enable = %u, dapm = %d\n", __func__, mclk_enable,
		 dapm);
	if (dapm)
		TAIKO_ACQUIRE_LOCK(taiko->codec_resource_lock);
	if (mclk_enable) {
		taiko->mclk_enabled = true;

		if (taiko->mbhc_polling_active) {
			taiko_codec_pause_hs_polling(codec);
			taiko_codec_disable_clock_block(codec);
			taiko_codec_enable_bandgap(codec,
						   TAIKO_BANDGAP_AUDIO_MODE);
			taiko_codec_enable_clock_block(codec, 0);
			taiko_codec_calibrate_hs_polling(codec);
			taiko_codec_start_hs_polling(codec);
		} else {
			taiko_codec_disable_clock_block(codec);
			taiko_codec_enable_bandgap(codec,
						   TAIKO_BANDGAP_AUDIO_MODE);
			taiko_codec_enable_clock_block(codec, 0);
		}
	} else {

		if (!taiko->mclk_enabled) {
			if (dapm)
				TAIKO_RELEASE_LOCK(taiko->codec_resource_lock);
			pr_err("Error, MCLK already diabled\n");
			return -EINVAL;
		}
		taiko->mclk_enabled = false;

		if (taiko->mbhc_polling_active) {
			taiko_codec_pause_hs_polling(codec);
			taiko_codec_disable_clock_block(codec);
			taiko_codec_enable_bandgap(codec,
						   TAIKO_BANDGAP_MBHC_MODE);
			taiko_enable_rx_bias(codec, 1);
			taiko_codec_enable_clock_block(codec, 1);
			taiko_codec_calibrate_hs_polling(codec);
			taiko_codec_start_hs_polling(codec);
			snd_soc_update_bits(codec, TAIKO_A_CLK_BUFF_EN1,
					0x05, 0x01);
		} else {
			taiko_codec_disable_clock_block(codec);
			taiko_codec_enable_bandgap(codec,
						   TAIKO_BANDGAP_OFF);
		}
	}
	if (dapm)
		TAIKO_RELEASE_LOCK(taiko->codec_resource_lock);
	return 0;
}

static int taiko_set_dai_sysclk(struct snd_soc_dai *dai,
		int clk_id, unsigned int freq, int dir)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static int taiko_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	u8 val = 0;
	struct taiko_priv *taiko = snd_soc_codec_get_drvdata(dai->codec);

	pr_debug("%s\n", __func__);
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		/* CPU is master */
		if (taiko->intf_type == WCD9XXX_INTERFACE_TYPE_I2C) {
			if (dai->id == AIF1_CAP)
				snd_soc_update_bits(dai->codec,
					TAIKO_A_CDC_CLK_TX_I2S_CTL,
					TAIKO_I2S_MASTER_MODE_MASK, 0);
			else if (dai->id == AIF1_PB)
				snd_soc_update_bits(dai->codec,
					TAIKO_A_CDC_CLK_RX_I2S_CTL,
					TAIKO_I2S_MASTER_MODE_MASK, 0);
		}
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
	/* CPU is slave */
		if (taiko->intf_type == WCD9XXX_INTERFACE_TYPE_I2C) {
			val = TAIKO_I2S_MASTER_MODE_MASK;
			if (dai->id == AIF1_CAP)
				snd_soc_update_bits(dai->codec,
					TAIKO_A_CDC_CLK_TX_I2S_CTL, val, val);
			else if (dai->id == AIF1_PB)
				snd_soc_update_bits(dai->codec,
					TAIKO_A_CDC_CLK_RX_I2S_CTL, val, val);
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int taiko_set_channel_map(struct snd_soc_dai *dai,
				unsigned int tx_num, unsigned int *tx_slot,
				unsigned int rx_num, unsigned int *rx_slot)

{
	struct taiko_priv *taiko = snd_soc_codec_get_drvdata(dai->codec);
	u32 i = 0;
	if (!tx_slot && !rx_slot) {
		pr_err("%s: Invalid\n", __func__);
		return -EINVAL;
	}
	pr_debug("%s(): dai_name = %s DAI-ID %x tx_ch %d rx_ch %d\n",
			__func__, dai->name, dai->id, tx_num, rx_num);

	if (dai->id == AIF1_PB || dai->id == AIF2_PB || dai->id == AIF3_PB) {
		for (i = 0; i < rx_num; i++) {
			taiko->dai[dai->id - 1].ch_num[i]  = rx_slot[i];
			taiko->dai[dai->id - 1].ch_act = 0;
			taiko->dai[dai->id - 1].ch_tot = rx_num;
		}
	} else if (dai->id == AIF1_CAP || dai->id == AIF2_CAP ||
		   dai->id == AIF3_CAP) {
		for (i = 0; i < tx_num; i++) {
			taiko->dai[dai->id - 1].ch_num[i]  = tx_slot[i];
			taiko->dai[dai->id - 1].ch_act = 0;
			taiko->dai[dai->id - 1].ch_tot = tx_num;
		}
	}
	return 0;
}

static int taiko_get_channel_map(struct snd_soc_dai *dai,
				unsigned int *tx_num, unsigned int *tx_slot,
				unsigned int *rx_num, unsigned int *rx_slot)

{
	struct wcd9xxx *taiko = dev_get_drvdata(dai->codec->control_data);

	u32 cnt = 0;
	u32 tx_ch[SLIM_MAX_TX_PORTS];
	u32 rx_ch[SLIM_MAX_RX_PORTS];

	if (!rx_slot && !tx_slot) {
		pr_err("%s: Invalid\n", __func__);
		return -EINVAL;
	}

	/* for virtual port, codec driver needs to do
	 * housekeeping, for now should be ok
	 */
	wcd9xxx_get_channel(taiko, rx_ch, tx_ch);
	if (dai->id == AIF1_PB) {
		*rx_num = taiko_dai[dai->id - 1].playback.channels_max;
		while (cnt < *rx_num) {
			rx_slot[cnt] = rx_ch[cnt];
			cnt++;
		}
	} else if (dai->id == AIF1_CAP) {
		*tx_num = taiko_dai[dai->id - 1].capture.channels_max;
		while (cnt < *tx_num) {
			tx_slot[cnt] = tx_ch[6 + cnt];
			cnt++;
		}
	} else if (dai->id == AIF2_PB) {
		*rx_num = taiko_dai[dai->id - 1].playback.channels_max;
		while (cnt < *rx_num) {
			rx_slot[cnt] = rx_ch[5 + cnt];
			cnt++;
		}
	} else if (dai->id == AIF2_CAP) {
		*tx_num = taiko_dai[dai->id - 1].capture.channels_max;
		tx_slot[0] = tx_ch[cnt];
		tx_slot[1] = tx_ch[1 + cnt];
		tx_slot[2] = tx_ch[5 + cnt];
		tx_slot[3] = tx_ch[3 + cnt];

	} else if (dai->id == AIF3_PB) {
		*rx_num = taiko_dai[dai->id - 1].playback.channels_max;
		rx_slot[0] = rx_ch[3];
		rx_slot[1] = rx_ch[4];

	} else if (dai->id == AIF3_CAP) {
		*tx_num = taiko_dai[dai->id - 1].capture.channels_max;
		tx_slot[cnt] = tx_ch[2 + cnt];
		tx_slot[cnt + 1] = tx_ch[4 + cnt];
	}
	pr_debug("%s(): dai_name = %s DAI-ID %x tx_ch %d rx_ch %d\n",
			__func__, dai->name, dai->id, *tx_num, *rx_num);


	return 0;
}

static int taiko_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params,
			    struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct taiko_priv *taiko = snd_soc_codec_get_drvdata(dai->codec);
	u8 path, shift;
	u16 tx_fs_reg, rx_fs_reg;
	u8 tx_fs_rate, rx_fs_rate, rx_state, tx_state;
	u32 compander_fs;

	pr_debug("%s: dai_name = %s DAI-ID %x rate %d num_ch %d\n", __func__,
		 dai->name, dai->id, params_rate(params),
		 params_channels(params));

	switch (params_rate(params)) {
	case 8000:
		tx_fs_rate = 0x00;
		rx_fs_rate = 0x00;
		compander_fs = COMPANDER_FS_8KHZ;
		break;
	case 16000:
		tx_fs_rate = 0x01;
		rx_fs_rate = 0x20;
		compander_fs = COMPANDER_FS_16KHZ;
		break;
	case 32000:
		tx_fs_rate = 0x02;
		rx_fs_rate = 0x40;
		compander_fs = COMPANDER_FS_32KHZ;
		break;
	case 48000:
		tx_fs_rate = 0x03;
		rx_fs_rate = 0x60;
		compander_fs = COMPANDER_FS_48KHZ;
		break;
	case 96000:
		tx_fs_rate = 0x04;
		rx_fs_rate = 0x80;
		compander_fs = COMPANDER_FS_96KHZ;
		break;
	case 192000:
		tx_fs_rate = 0x05;
		rx_fs_rate = 0xA0;
		compander_fs = COMPANDER_FS_192KHZ;
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
	if ((dai->id == AIF1_CAP) || (dai->id == AIF2_CAP) ||
	    (dai->id == AIF3_CAP)) {

		tx_state = snd_soc_read(codec,
				TAIKO_A_CDC_CLK_TX_CLK_EN_B1_CTL);

		for (path = 1, shift = 0;
				path <= NUM_DECIMATORS; path++, shift++) {

			if (path == BITS_PER_REG + 1) {
				shift = 0;
				tx_state = snd_soc_read(codec,
					TAIKO_A_CDC_CLK_TX_CLK_EN_B2_CTL);
			}

			if (!(tx_state & (1 << shift))) {
				tx_fs_reg = TAIKO_A_CDC_TX1_CLK_FS_CTL
						+ (BITS_PER_REG*(path-1));
				snd_soc_update_bits(codec, tx_fs_reg,
							0x07, tx_fs_rate);
			}
		}
		if (taiko->intf_type == WCD9XXX_INTERFACE_TYPE_I2C) {
			switch (params_format(params)) {
			case SNDRV_PCM_FORMAT_S16_LE:
				snd_soc_update_bits(codec,
					TAIKO_A_CDC_CLK_TX_I2S_CTL,
					0x20, 0x20);
				break;
			case SNDRV_PCM_FORMAT_S32_LE:
				snd_soc_update_bits(codec,
					TAIKO_A_CDC_CLK_TX_I2S_CTL,
					0x20, 0x00);
				break;
			default:
				pr_err("invalid format\n");
				break;
			}
			snd_soc_update_bits(codec, TAIKO_A_CDC_CLK_TX_I2S_CTL,
						0x07, tx_fs_rate);
		} else {
			taiko->dai[dai->id - 1].rate   = params_rate(params);
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
	if (dai->id == AIF1_PB || dai->id == AIF2_PB || dai->id == AIF3_PB) {

		rx_state = snd_soc_read(codec,
			TAIKO_A_CDC_CLK_RX_B1_CTL);

		for (path = 1, shift = 0;
				path <= NUM_INTERPOLATORS; path++, shift++) {

			if (!(rx_state & (1 << shift))) {
				rx_fs_reg = TAIKO_A_CDC_RX1_B5_CTL
						+ (BITS_PER_REG*(path-1));
				snd_soc_update_bits(codec, rx_fs_reg,
						0xE0, rx_fs_rate);
				if (comp_rx_path[shift] < COMPANDER_MAX)
					taiko->comp_fs[comp_rx_path[shift]]
					= compander_fs;
			}
		}
		if (taiko->intf_type == WCD9XXX_INTERFACE_TYPE_I2C) {
			switch (params_format(params)) {
			case SNDRV_PCM_FORMAT_S16_LE:
				snd_soc_update_bits(codec,
					TAIKO_A_CDC_CLK_RX_I2S_CTL,
					0x20, 0x20);
				break;
			case SNDRV_PCM_FORMAT_S32_LE:
				snd_soc_update_bits(codec,
					TAIKO_A_CDC_CLK_RX_I2S_CTL,
					0x20, 0x00);
				break;
			default:
				pr_err("invalid format\n");
				break;
			}
			snd_soc_update_bits(codec, TAIKO_A_CDC_CLK_RX_I2S_CTL,
						0x03, (rx_fs_rate >> 0x05));
		} else {
			taiko->dai[dai->id - 1].rate   = params_rate(params);
		}
	}

	return 0;
}

static struct snd_soc_dai_ops taiko_dai_ops = {
	.startup = taiko_startup,
	.shutdown = taiko_shutdown,
	.hw_params = taiko_hw_params,
	.set_sysclk = taiko_set_dai_sysclk,
	.set_fmt = taiko_set_dai_fmt,
	.set_channel_map = taiko_set_channel_map,
	.get_channel_map = taiko_get_channel_map,
};

static struct snd_soc_dai_driver taiko_dai[] = {
	{
		.name = "taiko_rx1",
		.id = AIF1_PB,
		.playback = {
			.stream_name = "AIF1 Playback",
			.rates = WCD9320_RATES,
			.formats = TAIKO_FORMATS,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &taiko_dai_ops,
	},
	{
		.name = "taiko_tx1",
		.id = AIF1_CAP,
		.capture = {
			.stream_name = "AIF1 Capture",
			.rates = WCD9320_RATES,
			.formats = TAIKO_FORMATS,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 4,
		},
		.ops = &taiko_dai_ops,
	},
	{
		.name = "taiko_rx2",
		.id = AIF2_PB,
		.playback = {
			.stream_name = "AIF2 Playback",
			.rates = WCD9320_RATES,
			.formats = TAIKO_FORMATS,
			.rate_min = 8000,
			.rate_max = 192000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &taiko_dai_ops,
	},
	{
		.name = "taiko_tx2",
		.id = AIF2_CAP,
		.capture = {
			.stream_name = "AIF2 Capture",
			.rates = WCD9320_RATES,
			.formats = TAIKO_FORMATS,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 4,
		},
		.ops = &taiko_dai_ops,
	},
	{
		.name = "taiko_tx3",
		.id = AIF3_CAP,
		.capture = {
			.stream_name = "AIF3 Capture",
			.rates = WCD9320_RATES,
			.formats = TAIKO_FORMATS,
			.rate_max = 48000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &taiko_dai_ops,
	},
	{
		.name = "taiko_rx3",
		.id = AIF3_PB,
		.playback = {
			.stream_name = "AIF3 Playback",
			.rates = WCD9320_RATES,
			.formats = TAIKO_FORMATS,
			.rate_min = 8000,
			.rate_max = 192000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &taiko_dai_ops,
	},
};

static struct snd_soc_dai_driver taiko_i2s_dai[] = {
	{
		.name = "taiko_i2s_rx1",
		.id = 1,
		.playback = {
			.stream_name = "AIF1 Playback",
			.rates = WCD9320_RATES,
			.formats = TAIKO_FORMATS,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 4,
		},
		.ops = &taiko_dai_ops,
	},
	{
		.name = "taiko_i2s_tx1",
		.id = 2,
		.capture = {
			.stream_name = "AIF1 Capture",
			.rates = WCD9320_RATES,
			.formats = TAIKO_FORMATS,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 4,
		},
		.ops = &taiko_dai_ops,
	},
};

static int taiko_codec_enable_slimrx(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct wcd9xxx *taiko;
	struct snd_soc_codec *codec = w->codec;
	struct taiko_priv *taiko_p = snd_soc_codec_get_drvdata(codec);
	u32  j = 0;
	u32  ret = 0;
	codec->control_data = dev_get_drvdata(codec->dev->parent);
	taiko = codec->control_data;
	/* Execute the callback only if interface type is slimbus */
	if (taiko_p->intf_type != WCD9XXX_INTERFACE_TYPE_SLIMBUS)
		return 0;

	pr_debug("%s: %s %d\n", __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		for (j = 0; j < ARRAY_SIZE(taiko_dai); j++) {
			if ((taiko_dai[j].id == AIF1_CAP) ||
			    (taiko_dai[j].id == AIF2_CAP) ||
			    (taiko_dai[j].id == AIF3_CAP))
				continue;
			if (!strncmp(w->sname,
				taiko_dai[j].playback.stream_name, 13)) {
				++taiko_p->dai[j].ch_act;
				break;
			}
		}
		if (taiko_p->dai[j].ch_act == taiko_p->dai[j].ch_tot)
			ret = wcd9xxx_cfg_slim_sch_rx(taiko,
					taiko_p->dai[j].ch_num,
					taiko_p->dai[j].ch_tot,
					taiko_p->dai[j].rate);
		break;
	case SND_SOC_DAPM_POST_PMD:
		for (j = 0; j < ARRAY_SIZE(taiko_dai); j++) {
			if ((taiko_dai[j].id == AIF1_CAP) ||
			    (taiko_dai[j].id == AIF2_CAP) ||
			    (taiko_dai[j].id == AIF3_CAP))
				continue;
			if (!strncmp(w->sname,
				taiko_dai[j].playback.stream_name, 13)) {
				--taiko_p->dai[j].ch_act;
				break;
			}
		}
		if (!taiko_p->dai[j].ch_act) {
			ret = wcd9xxx_close_slim_sch_rx(taiko,
						taiko_p->dai[j].ch_num,
						taiko_p->dai[j].ch_tot);
			usleep_range(15000, 15000);
			taiko_p->dai[j].rate = 0;
			memset(taiko_p->dai[j].ch_num, 0, (sizeof(u32)*
					taiko_p->dai[j].ch_tot));
			taiko_p->dai[j].ch_tot = 0;
		}
	}
	return ret;
}

static int taiko_codec_enable_slimtx(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct wcd9xxx *taiko;
	struct snd_soc_codec *codec = w->codec;
	struct taiko_priv *taiko_p = snd_soc_codec_get_drvdata(codec);
	/* index to the DAI ID, for now hardcoding */
	u32  j = 0;
	u32  ret = 0;

	codec->control_data = dev_get_drvdata(codec->dev->parent);
	taiko = codec->control_data;

	/* Execute the callback only if interface type is slimbus */
	if (taiko_p->intf_type != WCD9XXX_INTERFACE_TYPE_SLIMBUS)
		return 0;

	pr_debug("%s(): %s %d\n", __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		for (j = 0; j < ARRAY_SIZE(taiko_dai); j++) {
			if (taiko_dai[j].id == AIF1_PB ||
				taiko_dai[j].id == AIF2_PB ||
				taiko_dai[j].id == AIF3_PB)
				continue;
			if (!strncmp(w->sname,
				taiko_dai[j].capture.stream_name, 13)) {
				++taiko_p->dai[j].ch_act;
				break;
			}
		}
		if (taiko_p->dai[j].ch_act == taiko_p->dai[j].ch_tot)
			ret = wcd9xxx_cfg_slim_sch_tx(taiko,
						taiko_p->dai[j].ch_num,
						taiko_p->dai[j].ch_tot,
						taiko_p->dai[j].rate);
		break;
	case SND_SOC_DAPM_POST_PMD:
		for (j = 0; j < ARRAY_SIZE(taiko_dai); j++) {
			if (taiko_dai[j].id == AIF1_PB ||
				taiko_dai[j].id == AIF2_PB ||
				taiko_dai[j].id == AIF3_PB)
				continue;
			if (!strncmp(w->sname,
				taiko_dai[j].capture.stream_name, 13)) {
				--taiko_p->dai[j].ch_act;
				break;
			}
		}
		if (!taiko_p->dai[j].ch_act) {
			ret = wcd9xxx_close_slim_sch_tx(taiko,
						taiko_p->dai[j].ch_num,
						taiko_p->dai[j].ch_tot);
			taiko_p->dai[j].rate = 0;
			memset(taiko_p->dai[j].ch_num, 0, (sizeof(u32)*
					taiko_p->dai[j].ch_tot));
			taiko_p->dai[j].ch_tot = 0;
		}
	}
	return ret;
}

static int taiko_codec_enable_ear_pa(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	pr_debug("%s %s %d\n", __func__, w->name, event);

	switch (event) {
		break;
	case SND_SOC_DAPM_POST_PMU:

		snd_soc_update_bits(codec, TAIKO_A_BUCK_MODE_5, 0x02, 0x00);
		snd_soc_update_bits(codec, TAIKO_A_NCP_STATIC, 0x20, 0x00);
		snd_soc_update_bits(codec, TAIKO_A_BUCK_MODE_3, 0x04, 0x04);
		snd_soc_update_bits(codec, TAIKO_A_BUCK_MODE_3, 0x08, 0x00);

		usleep_range(5000, 5000);
		break;
	}
	return 0;
}

/* Todo: Have seperate dapm widgets for I2S and Slimbus.
 * Might Need to have callbacks registered only for slimbus
 */
static const struct snd_soc_dapm_widget taiko_dapm_widgets[] = {
	/*RX stuff */
	SND_SOC_DAPM_OUTPUT("EAR"),

	SND_SOC_DAPM_PGA_E("EAR PA", TAIKO_A_RX_EAR_EN, 4, 0, NULL, 0,
			taiko_codec_enable_ear_pa, SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_MIXER("DAC1", TAIKO_A_RX_EAR_EN, 6, 0, dac1_switch,
		ARRAY_SIZE(dac1_switch)),

	SND_SOC_DAPM_AIF_IN_E("SLIM RX1", "AIF1 Playback", 0, SND_SOC_NOPM, 0,
				0, taiko_codec_enable_slimrx,
				SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_IN_E("SLIM RX2", "AIF1 Playback", 0, SND_SOC_NOPM, 0,
				0, taiko_codec_enable_slimrx,
				SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_IN_E("SLIM RX3", "AIF1 Playback", 0, SND_SOC_NOPM, 0,
				0, taiko_codec_enable_slimrx,
				SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_AIF_IN_E("SLIM RX4", "AIF3 Playback", 0, SND_SOC_NOPM, 0,
				0, taiko_codec_enable_slimrx,
				SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_IN_E("SLIM RX5", "AIF3 Playback", 0, SND_SOC_NOPM, 0,
				0, taiko_codec_enable_slimrx,
				SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_AIF_IN_E("SLIM RX6", "AIF2 Playback", 0, SND_SOC_NOPM, 0,
				0, taiko_codec_enable_slimrx,
				SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_IN_E("SLIM RX7", "AIF2 Playback", 0, SND_SOC_NOPM, 0,
				0, taiko_codec_enable_slimrx,
				SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	/* Headphone */
	SND_SOC_DAPM_OUTPUT("HEADPHONE"),
	SND_SOC_DAPM_PGA_E("HPHL", TAIKO_A_RX_HPH_CNP_EN, 5, 0, NULL, 0,
		taiko_hph_pa_event, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER("HPHL DAC", TAIKO_A_RX_HPH_L_DAC_CTL, 7, 0,
		hphl_switch, ARRAY_SIZE(hphl_switch)),

	SND_SOC_DAPM_PGA_E("HPHR", TAIKO_A_RX_HPH_CNP_EN, 4, 0, NULL, 0,
		taiko_hph_pa_event, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU |	SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_DAC_E("HPHR DAC", NULL, TAIKO_A_RX_HPH_R_DAC_CTL, 7, 0,
		taiko_hphr_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	/* Speaker */
	SND_SOC_DAPM_OUTPUT("LINEOUT1"),
	SND_SOC_DAPM_OUTPUT("LINEOUT2"),
	SND_SOC_DAPM_OUTPUT("LINEOUT3"),
	SND_SOC_DAPM_OUTPUT("LINEOUT4"),
	SND_SOC_DAPM_OUTPUT("SPK_OUT"),

	SND_SOC_DAPM_PGA_E("LINEOUT1 PA", TAIKO_A_RX_LINE_CNP_EN, 0, 0, NULL,
			0, taiko_codec_enable_lineout, SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("LINEOUT2 PA", TAIKO_A_RX_LINE_CNP_EN, 1, 0, NULL,
			0, taiko_codec_enable_lineout, SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("LINEOUT3 PA", TAIKO_A_RX_LINE_CNP_EN, 2, 0, NULL,
			0, taiko_codec_enable_lineout, SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("LINEOUT4 PA", TAIKO_A_RX_LINE_CNP_EN, 3, 0, NULL,
			0, taiko_codec_enable_lineout, SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("SPK PA", TAIKO_A_SPKR_DRV_EN, 7, 0 , NULL,
			   0, taiko_codec_enable_spk_pa, SND_SOC_DAPM_PRE_PMU |
			   SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_DAC_E("LINEOUT1 DAC", NULL, TAIKO_A_RX_LINE_1_DAC_CTL, 7, 0
		, taiko_lineout_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("LINEOUT2 DAC", NULL, TAIKO_A_RX_LINE_2_DAC_CTL, 7, 0
		, taiko_lineout_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("LINEOUT3 DAC", NULL, TAIKO_A_RX_LINE_3_DAC_CTL, 7, 0
		, taiko_lineout_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SWITCH("LINEOUT3 DAC GROUND", SND_SOC_NOPM, 0, 0,
				&lineout3_ground_switch),
	SND_SOC_DAPM_DAC_E("LINEOUT4 DAC", NULL, TAIKO_A_RX_LINE_4_DAC_CTL, 7, 0
		, taiko_lineout_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SWITCH("LINEOUT4 DAC GROUND", SND_SOC_NOPM, 0, 0,
				&lineout4_ground_switch),

	SND_SOC_DAPM_DAC_E("SPK DAC", NULL, SND_SOC_NOPM, 0, 0,
			   taiko_spk_dac_event,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MIXER_E("RX1 MIX2", TAIKO_A_CDC_CLK_RX_B1_CTL, 0, 0, NULL,
		0, taiko_codec_reset_interpolator, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MIXER_E("RX2 MIX2", TAIKO_A_CDC_CLK_RX_B1_CTL, 1, 0, NULL,
		0, taiko_codec_reset_interpolator, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MIXER_E("RX7 MIX2", TAIKO_A_CDC_CLK_RX_B1_CTL, 2, 0, NULL,
		0, taiko_codec_reset_interpolator, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MIXER_E("RX4 MIX1", TAIKO_A_CDC_CLK_RX_B1_CTL, 3, 0, NULL,
		0, taiko_codec_reset_interpolator, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MIXER_E("RX5 MIX1", TAIKO_A_CDC_CLK_RX_B1_CTL, 4, 0, NULL,
		0, taiko_codec_reset_interpolator, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MIXER_E("RX6 MIX1", TAIKO_A_CDC_CLK_RX_B1_CTL, 5, 0, NULL,
		0, taiko_codec_reset_interpolator, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MIXER_E("RX7 MIX1", TAIKO_A_CDC_CLK_RX_B1_CTL, 6, 0, NULL,
		0, taiko_codec_reset_interpolator, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_MIXER("RX1 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX2 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX3 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MUX_E("RX4 DSM MUX", TAIKO_A_CDC_CLK_RX_B1_CTL, 3, 0,
		&rx4_dsm_mux, taiko_codec_reset_interpolator,
		SND_SOC_DAPM_PRE_PMU),

	SND_SOC_DAPM_MUX_E("RX6 DSM MUX", TAIKO_A_CDC_CLK_RX_B1_CTL, 5, 0,
		&rx6_dsm_mux, taiko_codec_reset_interpolator,
		SND_SOC_DAPM_PRE_PMU),

	SND_SOC_DAPM_MIXER("RX1 CHAIN", TAIKO_A_CDC_RX1_B6_CTL, 5, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX2 CHAIN", TAIKO_A_CDC_RX2_B6_CTL, 5, 0, NULL, 0),

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
	SND_SOC_DAPM_MUX("RX1 MIX2 INP1", SND_SOC_NOPM, 0, 0,
		&rx1_mix2_inp1_mux),
	SND_SOC_DAPM_MUX("RX1 MIX2 INP2", SND_SOC_NOPM, 0, 0,
		&rx1_mix2_inp2_mux),
	SND_SOC_DAPM_MUX("RX2 MIX2 INP1", SND_SOC_NOPM, 0, 0,
		&rx2_mix2_inp1_mux),
	SND_SOC_DAPM_MUX("RX2 MIX2 INP2", SND_SOC_NOPM, 0, 0,
		&rx2_mix2_inp2_mux),
	SND_SOC_DAPM_MUX("RX7 MIX2 INP1", SND_SOC_NOPM, 0, 0,
		&rx7_mix2_inp1_mux),
	SND_SOC_DAPM_MUX("RX7 MIX2 INP2", SND_SOC_NOPM, 0, 0,
		&rx7_mix2_inp2_mux),

	SND_SOC_DAPM_SUPPLY("CLASS_H_CLK", TAIKO_A_CDC_CLK_OTHR_CTL, 0, 0,
		taiko_codec_enable_class_h_clk, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_SUPPLY("CLASS_H_EAR", TAIKO_A_CDC_CLSH_B1_CTL, 4, 0,
		taiko_codec_enable_class_h, SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_SUPPLY("CLASS_H_HPH_R", TAIKO_A_CDC_CLSH_B1_CTL, 3, 0,
		taiko_codec_enable_class_h, SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_SUPPLY("CLASS_H_HPH_L", TAIKO_A_CDC_CLSH_B1_CTL, 2, 0,
		taiko_codec_enable_class_h, SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_SUPPLY("CP", TAIKO_A_NCP_EN, 0, 0,
		taiko_codec_enable_charge_pump, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_SUPPLY("RX_BIAS", SND_SOC_NOPM, 0, 0,
		taiko_codec_enable_rx_bias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	/* TX */

	SND_SOC_DAPM_SUPPLY("CDC_CONN", TAIKO_A_CDC_CLK_OTHR_CTL, 2, 0, NULL,
		0),

	SND_SOC_DAPM_SUPPLY("LDO_H", TAIKO_A_LDO_H_MODE_1, 7, 0,
		taiko_codec_enable_ldo_h, SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_SUPPLY("COMP1_CLK", SND_SOC_NOPM, 0, 0,
		taiko_config_compander, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_POST_PMD),
	SND_SOC_DAPM_SUPPLY("COMP2_CLK", SND_SOC_NOPM, 1, 0,
		taiko_config_compander, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_POST_PMD),


	SND_SOC_DAPM_INPUT("AMIC1"),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS1 External", TAIKO_A_MICB_1_CTL, 7, 0,
		taiko_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS1 Internal1", TAIKO_A_MICB_1_CTL, 7, 0,
		taiko_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS1 Internal2", TAIKO_A_MICB_1_CTL, 7, 0,
		taiko_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("ADC1", NULL, TAIKO_A_TX_1_2_EN, 7, 0,
		taiko_codec_enable_adc, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_INPUT("AMIC3"),
	SND_SOC_DAPM_ADC_E("ADC3", NULL, TAIKO_A_TX_3_4_EN, 7, 0,
		taiko_codec_enable_adc, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_INPUT("AMIC4"),
	SND_SOC_DAPM_ADC_E("ADC4", NULL, TAIKO_A_TX_3_4_EN, 3, 0,
		taiko_codec_enable_adc, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_INPUT("AMIC5"),
	SND_SOC_DAPM_ADC_E("ADC5", NULL, TAIKO_A_TX_5_6_EN, 7, 0,
		taiko_codec_enable_adc, SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_INPUT("AMIC6"),
	SND_SOC_DAPM_ADC_E("ADC6", NULL, TAIKO_A_TX_5_6_EN, 3, 0,
		taiko_codec_enable_adc, SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_MUX_E("DEC1 MUX", TAIKO_A_CDC_CLK_TX_CLK_EN_B1_CTL, 0, 0,
		&dec1_mux, taiko_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("DEC2 MUX", TAIKO_A_CDC_CLK_TX_CLK_EN_B1_CTL, 1, 0,
		&dec2_mux, taiko_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("DEC3 MUX", TAIKO_A_CDC_CLK_TX_CLK_EN_B1_CTL, 2, 0,
		&dec3_mux, taiko_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("DEC4 MUX", TAIKO_A_CDC_CLK_TX_CLK_EN_B1_CTL, 3, 0,
		&dec4_mux, taiko_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("DEC5 MUX", TAIKO_A_CDC_CLK_TX_CLK_EN_B1_CTL, 4, 0,
		&dec5_mux, taiko_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("DEC6 MUX", TAIKO_A_CDC_CLK_TX_CLK_EN_B1_CTL, 5, 0,
		&dec6_mux, taiko_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("DEC7 MUX", TAIKO_A_CDC_CLK_TX_CLK_EN_B1_CTL, 6, 0,
		&dec7_mux, taiko_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("DEC8 MUX", TAIKO_A_CDC_CLK_TX_CLK_EN_B1_CTL, 7, 0,
		&dec8_mux, taiko_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("DEC9 MUX", TAIKO_A_CDC_CLK_TX_CLK_EN_B2_CTL, 0, 0,
		&dec9_mux, taiko_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("DEC10 MUX", TAIKO_A_CDC_CLK_TX_CLK_EN_B2_CTL, 1, 0,
		&dec10_mux, taiko_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX("ANC1 MUX", SND_SOC_NOPM, 0, 0, &anc1_mux),
	SND_SOC_DAPM_MUX("ANC2 MUX", SND_SOC_NOPM, 0, 0, &anc2_mux),

	SND_SOC_DAPM_MIXER_E("ANC", SND_SOC_NOPM, 0, 0, NULL, 0,
		taiko_codec_enable_anc, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX("ANC1 FB MUX", SND_SOC_NOPM, 0, 0, &anc1_fb_mux),

	SND_SOC_DAPM_INPUT("AMIC2"),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS2 External", TAIKO_A_MICB_2_CTL, 7, 0,
		taiko_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU |	SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS2 Internal1", TAIKO_A_MICB_2_CTL, 7, 0,
		taiko_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS2 Internal2", TAIKO_A_MICB_2_CTL, 7, 0,
		taiko_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS2 Internal3", TAIKO_A_MICB_2_CTL, 7, 0,
		taiko_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS3 External", TAIKO_A_MICB_3_CTL, 7, 0,
		taiko_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS3 Internal1", TAIKO_A_MICB_3_CTL, 7, 0,
		taiko_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS3 Internal2", TAIKO_A_MICB_3_CTL, 7, 0,
		taiko_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS4 External", TAIKO_A_MICB_4_CTL, 7,
				0, taiko_codec_enable_micbias,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
				SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("ADC2", NULL, TAIKO_A_TX_1_2_EN, 3, 0,
		taiko_codec_enable_adc, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX("SLIM TX1 MUX", SND_SOC_NOPM, 0, 0, &sb_tx1_mux),
	SND_SOC_DAPM_AIF_OUT_E("SLIM TX1", "AIF2 Capture", 0, SND_SOC_NOPM, 0,
				0, taiko_codec_enable_slimtx,
				SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX("SLIM TX2 MUX", SND_SOC_NOPM, 0, 0, &sb_tx2_mux),
	SND_SOC_DAPM_AIF_OUT_E("SLIM TX2", "AIF2 Capture", 0, SND_SOC_NOPM, 0,
				0, taiko_codec_enable_slimtx,
				SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX("SLIM TX3 MUX", SND_SOC_NOPM, 0, 0, &sb_tx3_mux),
	SND_SOC_DAPM_AIF_OUT_E("SLIM TX3", "AIF3 Capture", 0, SND_SOC_NOPM, 0,
				0, taiko_codec_enable_slimtx,
				SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX("SLIM TX4 MUX", SND_SOC_NOPM, 0, 0, &sb_tx4_mux),
	SND_SOC_DAPM_AIF_OUT_E("SLIM TX4", "AIF2 Capture", 0, SND_SOC_NOPM, 0,
				0, taiko_codec_enable_slimtx,
				SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX("SLIM TX5 MUX", SND_SOC_NOPM, 0, 0, &sb_tx5_mux),
	SND_SOC_DAPM_AIF_OUT_E("SLIM TX5", "AIF3 Capture", 0, SND_SOC_NOPM, 0,
				0, taiko_codec_enable_slimtx,
				SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX("SLIM TX6 MUX", SND_SOC_NOPM, 0, 0, &sb_tx6_mux),
	SND_SOC_DAPM_AIF_OUT_E("SLIM TX6", "AIF2 Capture", 0, SND_SOC_NOPM, 0,
				0, taiko_codec_enable_slimtx,
				SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX("SLIM TX7 MUX", SND_SOC_NOPM, 0, 0, &sb_tx7_mux),
	SND_SOC_DAPM_AIF_OUT_E("SLIM TX7", "AIF1 Capture", 0, SND_SOC_NOPM, 0,
				0, taiko_codec_enable_slimtx,
				SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX("SLIM TX8 MUX", SND_SOC_NOPM, 0, 0, &sb_tx8_mux),
	SND_SOC_DAPM_AIF_OUT_E("SLIM TX8", "AIF1 Capture", 0, SND_SOC_NOPM, 0,
				0, taiko_codec_enable_slimtx,
				SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX("SLIM TX9 MUX", SND_SOC_NOPM, 0, 0, &sb_tx9_mux),
	SND_SOC_DAPM_AIF_OUT_E("SLIM TX9", "AIF1 Capture", NULL, SND_SOC_NOPM,
			0, 0, taiko_codec_enable_slimtx,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX("SLIM TX10 MUX", SND_SOC_NOPM, 0, 0, &sb_tx10_mux),
	SND_SOC_DAPM_AIF_OUT_E("SLIM TX10", "AIF1 Capture", NULL, SND_SOC_NOPM,
			0, 0, taiko_codec_enable_slimtx,
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	/* Digital Mic Inputs */
	SND_SOC_DAPM_ADC_E("DMIC1", NULL, SND_SOC_NOPM, 0, 0,
		taiko_codec_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("DMIC2", NULL, SND_SOC_NOPM, 0, 0,
		taiko_codec_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("DMIC3", NULL, SND_SOC_NOPM, 0, 0,
		taiko_codec_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("DMIC4", NULL, SND_SOC_NOPM, 0, 0,
		taiko_codec_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("DMIC5", NULL, SND_SOC_NOPM, 0, 0,
		taiko_codec_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("DMIC6", NULL, SND_SOC_NOPM, 0, 0,
		taiko_codec_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	/* Sidetone */
	SND_SOC_DAPM_MUX("IIR1 INP1 MUX", SND_SOC_NOPM, 0, 0, &iir1_inp1_mux),
	SND_SOC_DAPM_PGA("IIR1", TAIKO_A_CDC_CLK_SD_CTL, 0, 0, NULL, 0),

	/* AUX PGA */
	SND_SOC_DAPM_ADC_E("AUX_PGA_Left", NULL, TAIKO_A_RX_AUX_SW_CTL, 7, 0,
		taiko_codec_enable_aux_pga, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("AUX_PGA_Right", NULL, TAIKO_A_RX_AUX_SW_CTL, 6, 0,
		taiko_codec_enable_aux_pga, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	/* Lineout, ear and HPH PA Mixers */

	SND_SOC_DAPM_MIXER("EAR_PA_MIXER", SND_SOC_NOPM, 0, 0,
		ear_pa_mix, ARRAY_SIZE(ear_pa_mix)),

	SND_SOC_DAPM_MIXER("HPHL_PA_MIXER", SND_SOC_NOPM, 0, 0,
		hphl_pa_mix, ARRAY_SIZE(hphl_pa_mix)),

	SND_SOC_DAPM_MIXER("HPHR_PA_MIXER", SND_SOC_NOPM, 0, 0,
		hphr_pa_mix, ARRAY_SIZE(hphr_pa_mix)),

	SND_SOC_DAPM_MIXER("LINEOUT1_PA_MIXER", SND_SOC_NOPM, 0, 0,
		lineout1_pa_mix, ARRAY_SIZE(lineout1_pa_mix)),

	SND_SOC_DAPM_MIXER("LINEOUT2_PA_MIXER", SND_SOC_NOPM, 0, 0,
		lineout2_pa_mix, ARRAY_SIZE(lineout2_pa_mix)),

	SND_SOC_DAPM_MIXER("LINEOUT3_PA_MIXER", SND_SOC_NOPM, 0, 0,
		lineout3_pa_mix, ARRAY_SIZE(lineout3_pa_mix)),

	SND_SOC_DAPM_MIXER("LINEOUT4_PA_MIXER", SND_SOC_NOPM, 0, 0,
		lineout4_pa_mix, ARRAY_SIZE(lineout4_pa_mix)),

};

static short taiko_codec_read_sta_result(struct snd_soc_codec *codec)
{
	u8 bias_msb, bias_lsb;
	short bias_value;

	bias_msb = snd_soc_read(codec, TAIKO_A_CDC_MBHC_B3_STATUS);
	bias_lsb = snd_soc_read(codec, TAIKO_A_CDC_MBHC_B2_STATUS);
	bias_value = (bias_msb << 8) | bias_lsb;
	return bias_value;
}

static short taiko_codec_read_dce_result(struct snd_soc_codec *codec)
{
	u8 bias_msb, bias_lsb;
	short bias_value;

	bias_msb = snd_soc_read(codec, TAIKO_A_CDC_MBHC_B5_STATUS);
	bias_lsb = snd_soc_read(codec, TAIKO_A_CDC_MBHC_B4_STATUS);
	bias_value = (bias_msb << 8) | bias_lsb;
	return bias_value;
}

static void taiko_turn_onoff_rel_detection(struct snd_soc_codec *codec, bool on)
{
	snd_soc_update_bits(codec, TAIKO_A_CDC_MBHC_B1_CTL, 0x02, on << 1);
}

static short __taiko_codec_sta_dce(struct snd_soc_codec *codec, int dce,
				   bool override_bypass, bool noreldetection)
{
	short bias_value;
	struct taiko_priv *taiko = snd_soc_codec_get_drvdata(codec);

	wcd9xxx_disable_irq(codec->control_data, TAIKO_IRQ_MBHC_POTENTIAL);
	if (noreldetection)
		taiko_turn_onoff_rel_detection(codec, false);

	/* Turn on the override */
	if (!override_bypass)
		snd_soc_update_bits(codec, TAIKO_A_CDC_MBHC_B1_CTL, 0x4, 0x4);
	if (dce) {
		snd_soc_update_bits(codec, TAIKO_A_CDC_MBHC_CLK_CTL, 0x8, 0x8);
		snd_soc_write(codec, TAIKO_A_CDC_MBHC_EN_CTL, 0x4);
		snd_soc_update_bits(codec, TAIKO_A_CDC_MBHC_CLK_CTL, 0x8, 0x0);
		usleep_range(taiko->mbhc_data.t_sta_dce,
			     taiko->mbhc_data.t_sta_dce);
		snd_soc_write(codec, TAIKO_A_CDC_MBHC_EN_CTL, 0x4);
		usleep_range(taiko->mbhc_data.t_dce,
			     taiko->mbhc_data.t_dce);
		bias_value = taiko_codec_read_dce_result(codec);
	} else {
		snd_soc_update_bits(codec, TAIKO_A_CDC_MBHC_CLK_CTL, 0x8, 0x8);
		snd_soc_write(codec, TAIKO_A_CDC_MBHC_EN_CTL, 0x2);
		snd_soc_update_bits(codec, TAIKO_A_CDC_MBHC_CLK_CTL, 0x8, 0x0);
		usleep_range(taiko->mbhc_data.t_sta_dce,
			     taiko->mbhc_data.t_sta_dce);
		snd_soc_write(codec, TAIKO_A_CDC_MBHC_EN_CTL, 0x2);
		usleep_range(taiko->mbhc_data.t_sta,
			     taiko->mbhc_data.t_sta);
		bias_value = taiko_codec_read_sta_result(codec);
		snd_soc_update_bits(codec, TAIKO_A_CDC_MBHC_CLK_CTL, 0x8, 0x8);
		snd_soc_write(codec, TAIKO_A_CDC_MBHC_EN_CTL, 0x0);
	}
	/* Turn off the override after measuring mic voltage */
	if (!override_bypass)
		snd_soc_update_bits(codec, TAIKO_A_CDC_MBHC_B1_CTL, 0x04, 0x00);

	if (noreldetection)
		taiko_turn_onoff_rel_detection(codec, true);
	wcd9xxx_enable_irq(codec->control_data, TAIKO_IRQ_MBHC_POTENTIAL);

	return bias_value;
}

static short taiko_codec_sta_dce(struct snd_soc_codec *codec, int dce,
				 bool norel)
{
	return __taiko_codec_sta_dce(codec, dce, false, norel);
}

/* called only from interrupt which is under codec_resource_lock acquisition */
static short taiko_codec_setup_hs_polling(struct snd_soc_codec *codec)
{
	struct taiko_priv *taiko = snd_soc_codec_get_drvdata(codec);
	short bias_value;
	u8 cfilt_mode;

	pr_debug("%s: enter, mclk_enabled %d\n", __func__, taiko->mclk_enabled);
	if (!taiko->mbhc_cfg.calibration) {
		pr_err("Error, no taiko calibration\n");
		return -ENODEV;
	}

	if (!taiko->mclk_enabled) {
		taiko_codec_disable_clock_block(codec);
		taiko_codec_enable_bandgap(codec, TAIKO_BANDGAP_MBHC_MODE);
		taiko_enable_rx_bias(codec, 1);
		taiko_codec_enable_clock_block(codec, 1);
	}

	snd_soc_update_bits(codec, TAIKO_A_CLK_BUFF_EN1, 0x05, 0x01);

	/* Make sure CFILT is in fast mode, save current mode */
	cfilt_mode = snd_soc_read(codec, taiko->mbhc_bias_regs.cfilt_ctl);
	snd_soc_update_bits(codec, taiko->mbhc_bias_regs.cfilt_ctl, 0x70, 0x00);

	snd_soc_update_bits(codec, taiko->mbhc_bias_regs.ctl_reg, 0x1F, 0x16);

	snd_soc_update_bits(codec, TAIKO_A_CDC_MBHC_CLK_CTL, 0x2, 0x2);
	snd_soc_write(codec, TAIKO_A_MBHC_SCALING_MUX_1, 0x84);

	snd_soc_update_bits(codec, TAIKO_A_TX_7_MBHC_EN, 0x80, 0x80);
	snd_soc_update_bits(codec, TAIKO_A_TX_7_MBHC_EN, 0x1F, 0x1C);
	snd_soc_update_bits(codec, TAIKO_A_TX_7_MBHC_TEST_CTL, 0x40, 0x40);

	snd_soc_update_bits(codec, TAIKO_A_TX_7_MBHC_EN, 0x80, 0x00);
	snd_soc_update_bits(codec, TAIKO_A_CDC_MBHC_CLK_CTL, 0x8, 0x8);
	snd_soc_update_bits(codec, TAIKO_A_CDC_MBHC_CLK_CTL, 0x8, 0x00);

	snd_soc_update_bits(codec, TAIKO_A_CDC_MBHC_B1_CTL, 0x2, 0x2);
	snd_soc_update_bits(codec, TAIKO_A_CDC_MBHC_CLK_CTL, 0x8, 0x8);

	taiko_codec_calibrate_hs_polling(codec);

	/* don't flip override */
	bias_value = __taiko_codec_sta_dce(codec, 1, true, true);
	snd_soc_update_bits(codec, taiko->mbhc_bias_regs.cfilt_ctl, 0x40,
			    cfilt_mode);
	snd_soc_update_bits(codec, TAIKO_A_MBHC_HPH, 0x13, 0x00);

	return bias_value;
}

static int taiko_cancel_btn_work(struct taiko_priv *taiko)
{
	int r = 0;
	struct wcd9xxx *core = dev_get_drvdata(taiko->codec->dev->parent);

	if (cancel_delayed_work_sync(&taiko->mbhc_btn_dwork)) {
		/* if scheduled mbhc_btn_dwork is canceled from here,
		* we have to unlock from here instead btn_work */
		wcd9xxx_unlock_sleep(core);
		r = 1;
	}
	return r;
}

/* called under codec_resource_lock acquisition */
void taiko_set_and_turnoff_hph_padac(struct snd_soc_codec *codec)
{
	struct taiko_priv *taiko = snd_soc_codec_get_drvdata(codec);
	u8 wg_time;

	wg_time = snd_soc_read(codec, TAIKO_A_RX_HPH_CNP_WG_TIME) ;
	wg_time += 1;

	/* If headphone PA is on, check if userspace receives
	 * removal event to sync-up PA's state */
	if (taiko_is_hph_pa_on(codec)) {
		pr_debug("%s PA is on, setting PA_OFF_ACK\n", __func__);
		set_bit(TAIKO_HPHL_PA_OFF_ACK, &taiko->hph_pa_dac_state);
		set_bit(TAIKO_HPHR_PA_OFF_ACK, &taiko->hph_pa_dac_state);
	} else {
		pr_debug("%s PA is off\n", __func__);
	}

	if (taiko_is_hph_dac_on(codec, 1))
		set_bit(TAIKO_HPHL_DAC_OFF_ACK, &taiko->hph_pa_dac_state);
	if (taiko_is_hph_dac_on(codec, 0))
		set_bit(TAIKO_HPHR_DAC_OFF_ACK, &taiko->hph_pa_dac_state);

	snd_soc_update_bits(codec, TAIKO_A_RX_HPH_CNP_EN, 0x30, 0x00);
	snd_soc_update_bits(codec, TAIKO_A_RX_HPH_L_DAC_CTL,
			    0xC0, 0x00);
	snd_soc_update_bits(codec, TAIKO_A_RX_HPH_R_DAC_CTL,
			    0xC0, 0x00);
	usleep_range(wg_time * 1000, wg_time * 1000);
}

static void taiko_clr_and_turnon_hph_padac(struct taiko_priv *taiko)
{
	bool pa_turned_on = false;
	struct snd_soc_codec *codec = taiko->codec;
	u8 wg_time;

	wg_time = snd_soc_read(codec, TAIKO_A_RX_HPH_CNP_WG_TIME) ;
	wg_time += 1;

	if (test_and_clear_bit(TAIKO_HPHR_DAC_OFF_ACK,
			       &taiko->hph_pa_dac_state)) {
		pr_debug("%s: HPHR clear flag and enable DAC\n", __func__);
		snd_soc_update_bits(taiko->codec, TAIKO_A_RX_HPH_R_DAC_CTL,
				    0xC0, 0xC0);
	}
	if (test_and_clear_bit(TAIKO_HPHL_DAC_OFF_ACK,
			       &taiko->hph_pa_dac_state)) {
		pr_debug("%s: HPHL clear flag and enable DAC\n", __func__);
		snd_soc_update_bits(taiko->codec, TAIKO_A_RX_HPH_L_DAC_CTL,
				    0xC0, 0xC0);
	}

	if (test_and_clear_bit(TAIKO_HPHR_PA_OFF_ACK,
			       &taiko->hph_pa_dac_state)) {
		pr_debug("%s: HPHR clear flag and enable PA\n", __func__);
		snd_soc_update_bits(taiko->codec, TAIKO_A_RX_HPH_CNP_EN, 0x10,
				    1 << 4);
		pa_turned_on = true;
	}
	if (test_and_clear_bit(TAIKO_HPHL_PA_OFF_ACK,
			       &taiko->hph_pa_dac_state)) {
		pr_debug("%s: HPHL clear flag and enable PA\n", __func__);
		snd_soc_update_bits(taiko->codec, TAIKO_A_RX_HPH_CNP_EN, 0x20,
				    1 << 5);
		pa_turned_on = true;
	}

	if (pa_turned_on) {
		pr_debug("%s: PA was turned off by MBHC and not by DAPM\n",
				__func__);
		usleep_range(wg_time * 1000, wg_time * 1000);
	}
}

/* called under codec_resource_lock acquisition */
static void taiko_codec_report_plug(struct snd_soc_codec *codec, int insertion,
				    enum snd_jack_types jack_type)
{
	struct taiko_priv *taiko = snd_soc_codec_get_drvdata(codec);

	if (!insertion) {
		/* Report removal */
		taiko->hph_status &= ~jack_type;
		if (taiko->mbhc_cfg.headset_jack) {
			/* cancel possibly scheduled btn work and
			* report release if we reported button press */
			if (taiko_cancel_btn_work(taiko)) {
				pr_debug("%s: button press is canceled\n",
					__func__);
			} else if (taiko->buttons_pressed) {
				pr_debug("%s: release of button press%d\n",
					  __func__, jack_type);
				taiko_snd_soc_jack_report(taiko,
						 taiko->mbhc_cfg.button_jack, 0,
						 taiko->buttons_pressed);
				taiko->buttons_pressed &=
							~TAIKO_JACK_BUTTON_MASK;
			}
			pr_debug("%s: Reporting removal %d(%x)\n", __func__,
				 jack_type, taiko->hph_status);
			taiko_snd_soc_jack_report(taiko,
						  taiko->mbhc_cfg.headset_jack,
						  taiko->hph_status,
						  TAIKO_JACK_MASK);
		}
		taiko_set_and_turnoff_hph_padac(codec);
		hphocp_off_report(taiko, SND_JACK_OC_HPHR,
				  TAIKO_IRQ_HPH_PA_OCPR_FAULT);
		hphocp_off_report(taiko, SND_JACK_OC_HPHL,
				  TAIKO_IRQ_HPH_PA_OCPL_FAULT);
		taiko->current_plug = PLUG_TYPE_NONE;
		taiko->mbhc_polling_active = false;
	} else {
		/* Report insertion */
		taiko->hph_status |= jack_type;

		if (jack_type == SND_JACK_HEADPHONE)
			taiko->current_plug = PLUG_TYPE_HEADPHONE;
		else if (jack_type == SND_JACK_UNSUPPORTED)
			taiko->current_plug = PLUG_TYPE_GND_MIC_SWAP;
		else if (jack_type == SND_JACK_HEADSET) {
			taiko->mbhc_polling_active = true;
			taiko->current_plug = PLUG_TYPE_HEADSET;
		}
		if (taiko->mbhc_cfg.headset_jack) {
			pr_debug("%s: Reporting insertion %d(%x)\n", __func__,
				 jack_type, taiko->hph_status);
			taiko_snd_soc_jack_report(taiko,
						  taiko->mbhc_cfg.headset_jack,
						  taiko->hph_status,
						  TAIKO_JACK_MASK);
		}
		taiko_clr_and_turnon_hph_padac(taiko);
	}
}

static int taiko_codec_enable_hs_detect(struct snd_soc_codec *codec,
					int insertion, int trigger,
					bool padac_off)
{
	struct taiko_priv *taiko = snd_soc_codec_get_drvdata(codec);
	int central_bias_enabled = 0;
	const struct taiko_mbhc_general_cfg *generic =
	    TAIKO_MBHC_CAL_GENERAL_PTR(taiko->mbhc_cfg.calibration);
	const struct taiko_mbhc_plug_detect_cfg *plug_det =
	    TAIKO_MBHC_CAL_PLUG_DET_PTR(taiko->mbhc_cfg.calibration);

	if (!taiko->mbhc_cfg.calibration) {
		pr_err("Error, no taiko calibration\n");
		return -EINVAL;
	}

	snd_soc_update_bits(codec, TAIKO_A_CDC_MBHC_INT_CTL, 0x1, 0);

	/* Make sure mic bias and Mic line schmitt trigger
	 * are turned OFF
	 */
	snd_soc_update_bits(codec, taiko->mbhc_bias_regs.ctl_reg, 0x01, 0x01);
	snd_soc_update_bits(codec, taiko->mbhc_bias_regs.mbhc_reg, 0x90, 0x00);

	if (insertion) {
		taiko_codec_switch_micbias(codec, 0);

		/* DAPM can manipulate PA/DAC bits concurrently */
		if (padac_off == true)
			taiko_set_and_turnoff_hph_padac(codec);

		if (trigger & MBHC_USE_HPHL_TRIGGER) {
			/* Enable HPH Schmitt Trigger */
			snd_soc_update_bits(codec, TAIKO_A_MBHC_HPH, 0x11,
					    0x11);
			snd_soc_update_bits(codec, TAIKO_A_MBHC_HPH, 0x0C,
					    plug_det->hph_current << 2);
			snd_soc_update_bits(codec, TAIKO_A_MBHC_HPH, 0x02,
					    0x02);
		}
		if (trigger & MBHC_USE_MB_TRIGGER) {
			/* enable the mic line schmitt trigger */
			snd_soc_update_bits(codec,
					    taiko->mbhc_bias_regs.mbhc_reg,
					    0x60, plug_det->mic_current << 5);
			snd_soc_update_bits(codec,
					    taiko->mbhc_bias_regs.mbhc_reg,
					    0x80, 0x80);
			usleep_range(plug_det->t_mic_pid, plug_det->t_mic_pid);
			snd_soc_update_bits(codec,
					    taiko->mbhc_bias_regs.ctl_reg, 0x01,
					    0x00);
			snd_soc_update_bits(codec,
					    taiko->mbhc_bias_regs.mbhc_reg,
					    0x10, 0x10);
		}

		/* setup for insetion detection */
		snd_soc_update_bits(codec, TAIKO_A_CDC_MBHC_INT_CTL, 0x2, 0);
	} else {
		pr_debug("setup for removal detection\n");
		/* Make sure the HPH schmitt trigger is OFF */
		snd_soc_update_bits(codec, TAIKO_A_MBHC_HPH, 0x12, 0x00);

		/* enable the mic line schmitt trigger */
		snd_soc_update_bits(codec, taiko->mbhc_bias_regs.ctl_reg,
				    0x01, 0x00);
		snd_soc_update_bits(codec, taiko->mbhc_bias_regs.mbhc_reg, 0x60,
				    plug_det->mic_current << 5);
		snd_soc_update_bits(codec, taiko->mbhc_bias_regs.mbhc_reg,
			0x80, 0x80);
		usleep_range(plug_det->t_mic_pid, plug_det->t_mic_pid);
		snd_soc_update_bits(codec, taiko->mbhc_bias_regs.mbhc_reg,
			0x10, 0x10);

		/* Setup for low power removal detection */
		snd_soc_update_bits(codec, TAIKO_A_CDC_MBHC_INT_CTL, 0x2, 0x2);
	}

	if (snd_soc_read(codec, TAIKO_A_CDC_MBHC_B1_CTL) & 0x4) {
		/* called called by interrupt */
		if (!(taiko->clock_active)) {
			taiko_codec_enable_config_mode(codec, 1);
			snd_soc_update_bits(codec, TAIKO_A_CDC_MBHC_B1_CTL,
				0x06, 0);
			usleep_range(generic->t_shutdown_plug_rem,
				     generic->t_shutdown_plug_rem);
			taiko_codec_enable_config_mode(codec, 0);
		} else
			snd_soc_update_bits(codec, TAIKO_A_CDC_MBHC_B1_CTL,
				0x06, 0);
	}

	snd_soc_update_bits(codec, taiko->mbhc_bias_regs.int_rbias, 0x80, 0);

	/* If central bandgap disabled */
	if (!(snd_soc_read(codec, TAIKO_A_PIN_CTL_OE1) & 1)) {
		snd_soc_update_bits(codec, TAIKO_A_PIN_CTL_OE1, 0x3, 0x3);
		usleep_range(generic->t_bg_fast_settle,
			     generic->t_bg_fast_settle);
		central_bias_enabled = 1;
	}

	/* If LDO_H disabled */
	if (snd_soc_read(codec, TAIKO_A_PIN_CTL_OE0) & 0x80) {
		snd_soc_update_bits(codec, TAIKO_A_PIN_CTL_OE0, 0x10, 0);
		snd_soc_update_bits(codec, TAIKO_A_PIN_CTL_OE0, 0x80, 0x80);
		usleep_range(generic->t_ldoh, generic->t_ldoh);
		snd_soc_update_bits(codec, TAIKO_A_PIN_CTL_OE0, 0x80, 0);

		if (central_bias_enabled)
			snd_soc_update_bits(codec, TAIKO_A_PIN_CTL_OE1, 0x1, 0);
	}

	snd_soc_update_bits(codec, taiko->reg_addr.micb_4_mbhc, 0x3,
			    taiko->mbhc_cfg.micbias);

	wcd9xxx_enable_irq(codec->control_data, TAIKO_IRQ_MBHC_INSERTION);
	snd_soc_update_bits(codec, TAIKO_A_CDC_MBHC_INT_CTL, 0x1, 0x1);
	return 0;
}

static u16 taiko_codec_v_sta_dce(struct snd_soc_codec *codec, bool dce,
				 s16 vin_mv)
{
	struct taiko_priv *taiko;
	s16 diff, zero;
	u32 mb_mv, in;
	u16 value;

	taiko = snd_soc_codec_get_drvdata(codec);
	mb_mv = taiko->mbhc_data.micb_mv;

	if (mb_mv == 0) {
		pr_err("%s: Mic Bias voltage is set to zero\n", __func__);
		return -EINVAL;
	}

	if (dce) {
		diff = (taiko->mbhc_data.dce_mb) - (taiko->mbhc_data.dce_z);
		zero = (taiko->mbhc_data.dce_z);
	} else {
		diff = (taiko->mbhc_data.sta_mb) - (taiko->mbhc_data.sta_z);
		zero = (taiko->mbhc_data.sta_z);
	}
	in = (u32) diff * vin_mv;

	value = (u16) (in / mb_mv) + zero;
	return value;
}

static s32 taiko_codec_sta_dce_v(struct snd_soc_codec *codec, s8 dce,
				 u16 bias_value)
{
	struct taiko_priv *taiko;
	s16 value, z, mb;
	s32 mv;

	taiko = snd_soc_codec_get_drvdata(codec);
	value = bias_value;
	if (dce) {
		z = (taiko->mbhc_data.dce_z);
		mb = (taiko->mbhc_data.dce_mb);
		mv = (value - z) * (s32)taiko->mbhc_data.micb_mv / (mb - z);
	} else {
		z = (taiko->mbhc_data.sta_z);
		mb = (taiko->mbhc_data.sta_mb);
		mv = (value - z) * (s32)taiko->mbhc_data.micb_mv / (mb - z);
	}

	return mv;
}

static void btn_lpress_fn(struct work_struct *work)
{
	struct delayed_work *delayed_work;
	struct taiko_priv *taiko;
	short bias_value;
	int dce_mv, sta_mv;
	struct wcd9xxx *core;

	pr_debug("%s:\n", __func__);

	delayed_work = to_delayed_work(work);
	taiko = container_of(delayed_work, struct taiko_priv, mbhc_btn_dwork);
	core = dev_get_drvdata(taiko->codec->dev->parent);

	if (taiko) {
		if (taiko->mbhc_cfg.button_jack) {
			bias_value = taiko_codec_read_sta_result(taiko->codec);
			sta_mv = taiko_codec_sta_dce_v(taiko->codec, 0,
						bias_value);
			bias_value = taiko_codec_read_dce_result(taiko->codec);
			dce_mv = taiko_codec_sta_dce_v(taiko->codec, 1,
						bias_value);
			pr_debug("%s: Reporting long button press event\n",
				__func__);
			pr_debug("%s: STA: %d, DCE: %d\n", __func__, sta_mv,
					dce_mv);
			taiko_snd_soc_jack_report(taiko,
						  taiko->mbhc_cfg.button_jack,
						  taiko->buttons_pressed,
						  taiko->buttons_pressed);
		}
	} else {
		pr_err("%s: Bad taiko private data\n", __func__);
	}

	pr_debug("%s: leave\n", __func__);
	wcd9xxx_unlock_sleep(core);
}

void taiko_mbhc_cal(struct snd_soc_codec *codec)
{
	struct taiko_priv *taiko;
	struct taiko_mbhc_btn_detect_cfg *btn_det;
	u8 cfilt_mode, bg_mode;
	u8 ncic, nmeas, navg;
	u32 mclk_rate;
	u32 dce_wait, sta_wait;
	u8 *n_cic;
	void *calibration;

	taiko = snd_soc_codec_get_drvdata(codec);
	calibration = taiko->mbhc_cfg.calibration;

	wcd9xxx_disable_irq(codec->control_data, TAIKO_IRQ_MBHC_POTENTIAL);
	taiko_turn_onoff_rel_detection(codec, false);

	/* First compute the DCE / STA wait times
	 * depending on tunable parameters.
	 * The value is computed in microseconds
	 */
	btn_det = TAIKO_MBHC_CAL_BTN_DET_PTR(calibration);
	n_cic = taiko_mbhc_cal_btn_det_mp(btn_det, TAIKO_BTN_DET_N_CIC);
	ncic = n_cic[taiko_codec_mclk_index(taiko)];
	nmeas = TAIKO_MBHC_CAL_BTN_DET_PTR(calibration)->n_meas;
	navg = TAIKO_MBHC_CAL_GENERAL_PTR(calibration)->mbhc_navg;
	mclk_rate = taiko->mbhc_cfg.mclk_rate;
	dce_wait = (1000 * 512 * ncic * (nmeas + 1)) / (mclk_rate / 1000);
	sta_wait = (1000 * 128 * (navg + 1)) / (mclk_rate / 1000);

	taiko->mbhc_data.t_dce = dce_wait;
	taiko->mbhc_data.t_sta = sta_wait;

	/* LDOH and CFILT are already configured during pdata handling.
	 * Only need to make sure CFILT and bandgap are in Fast mode.
	 * Need to restore defaults once calculation is done.
	 */
	cfilt_mode = snd_soc_read(codec, taiko->mbhc_bias_regs.cfilt_ctl);
	snd_soc_update_bits(codec, taiko->mbhc_bias_regs.cfilt_ctl, 0x40, 0x00);
	bg_mode = snd_soc_update_bits(codec, TAIKO_A_BIAS_CENTRAL_BG_CTL, 0x02,
				      0x02);

	/* Micbias, CFILT, LDOH, MBHC MUX mode settings
	 * to perform ADC calibration
	 */
	snd_soc_update_bits(codec, taiko->mbhc_bias_regs.ctl_reg, 0x60,
			    taiko->mbhc_cfg.micbias << 5);
	snd_soc_update_bits(codec, taiko->mbhc_bias_regs.ctl_reg, 0x01, 0x00);
	snd_soc_update_bits(codec, TAIKO_A_LDO_H_MODE_1, 0x60, 0x60);
	snd_soc_write(codec, TAIKO_A_TX_7_MBHC_TEST_CTL, 0x78);
	snd_soc_update_bits(codec, TAIKO_A_CDC_MBHC_B1_CTL, 0x04, 0x04);

	/* DCE measurement for 0 volts */
	snd_soc_write(codec, TAIKO_A_CDC_MBHC_CLK_CTL, 0x0A);
	snd_soc_write(codec, TAIKO_A_CDC_MBHC_EN_CTL, 0x04);
	snd_soc_write(codec, TAIKO_A_CDC_MBHC_CLK_CTL, 0x02);
	snd_soc_write(codec, TAIKO_A_MBHC_SCALING_MUX_1, 0x81);
	usleep_range(100, 100);
	snd_soc_write(codec, TAIKO_A_CDC_MBHC_EN_CTL, 0x04);
	usleep_range(taiko->mbhc_data.t_dce, taiko->mbhc_data.t_dce);
	taiko->mbhc_data.dce_z = taiko_codec_read_dce_result(codec);

	/* DCE measurment for MB voltage */
	snd_soc_write(codec, TAIKO_A_CDC_MBHC_CLK_CTL, 0x0A);
	snd_soc_write(codec, TAIKO_A_CDC_MBHC_CLK_CTL, 0x02);
	snd_soc_write(codec, TAIKO_A_MBHC_SCALING_MUX_1, 0x82);
	usleep_range(100, 100);
	snd_soc_write(codec, TAIKO_A_CDC_MBHC_EN_CTL, 0x04);
	usleep_range(taiko->mbhc_data.t_dce, taiko->mbhc_data.t_dce);
	taiko->mbhc_data.dce_mb = taiko_codec_read_dce_result(codec);

	/* Sta measuremnt for 0 volts */
	snd_soc_write(codec, TAIKO_A_CDC_MBHC_CLK_CTL, 0x0A);
	snd_soc_write(codec, TAIKO_A_CDC_MBHC_EN_CTL, 0x02);
	snd_soc_write(codec, TAIKO_A_CDC_MBHC_CLK_CTL, 0x02);
	snd_soc_write(codec, TAIKO_A_MBHC_SCALING_MUX_1, 0x81);
	usleep_range(100, 100);
	snd_soc_write(codec, TAIKO_A_CDC_MBHC_EN_CTL, 0x02);
	usleep_range(taiko->mbhc_data.t_sta, taiko->mbhc_data.t_sta);
	taiko->mbhc_data.sta_z = taiko_codec_read_sta_result(codec);

	/* STA Measurement for MB Voltage */
	snd_soc_write(codec, TAIKO_A_MBHC_SCALING_MUX_1, 0x82);
	usleep_range(100, 100);
	snd_soc_write(codec, TAIKO_A_CDC_MBHC_EN_CTL, 0x02);
	usleep_range(taiko->mbhc_data.t_sta, taiko->mbhc_data.t_sta);
	taiko->mbhc_data.sta_mb = taiko_codec_read_sta_result(codec);

	/* Restore default settings. */
	snd_soc_update_bits(codec, TAIKO_A_CDC_MBHC_B1_CTL, 0x04, 0x00);
	snd_soc_update_bits(codec, taiko->mbhc_bias_regs.cfilt_ctl, 0x40,
			    cfilt_mode);
	snd_soc_update_bits(codec, TAIKO_A_BIAS_CENTRAL_BG_CTL, 0x02, bg_mode);

	snd_soc_write(codec, TAIKO_A_MBHC_SCALING_MUX_1, 0x84);
	usleep_range(100, 100);

	wcd9xxx_enable_irq(codec->control_data, TAIKO_IRQ_MBHC_POTENTIAL);
	taiko_turn_onoff_rel_detection(codec, true);
}

void *taiko_mbhc_cal_btn_det_mp(const struct taiko_mbhc_btn_detect_cfg *btn_det,
				const enum taiko_mbhc_btn_det_mem mem)
{
	void *ret = &btn_det->_v_btn_low;

	switch (mem) {
	case TAIKO_BTN_DET_GAIN:
		ret += sizeof(btn_det->_n_cic);
	case TAIKO_BTN_DET_N_CIC:
		ret += sizeof(btn_det->_n_ready);
	case TAIKO_BTN_DET_N_READY:
		ret += sizeof(btn_det->_v_btn_high[0]) * btn_det->num_btn;
	case TAIKO_BTN_DET_V_BTN_HIGH:
		ret += sizeof(btn_det->_v_btn_low[0]) * btn_det->num_btn;
	case TAIKO_BTN_DET_V_BTN_LOW:
		/* do nothing */
		break;
	default:
		ret = NULL;
	}

	return ret;
}

static s16 taiko_scale_v_micb_vddio(struct taiko_priv *taiko, int v,
				    bool tovddio)
{
	int r;
	int vddio_k, mb_k;
	vddio_k = taiko_find_k_value(taiko->pdata->micbias.ldoh_v,
				     VDDIO_MICBIAS_MV);
	mb_k = taiko_find_k_value(taiko->pdata->micbias.ldoh_v,
				  taiko->mbhc_data.micb_mv);
	if (tovddio)
		r = v * vddio_k / mb_k;
	else
		r = v * mb_k / vddio_k;
	return r;
}

static void taiko_mbhc_calc_thres(struct snd_soc_codec *codec)
{
	struct taiko_priv *taiko;
	s16 btn_mv = 0, btn_delta_mv;
	struct taiko_mbhc_btn_detect_cfg *btn_det;
	struct taiko_mbhc_plug_type_cfg *plug_type;
	u16 *btn_high;
	u8 *n_ready;
	int i;

	taiko = snd_soc_codec_get_drvdata(codec);
	btn_det = TAIKO_MBHC_CAL_BTN_DET_PTR(taiko->mbhc_cfg.calibration);
	plug_type = TAIKO_MBHC_CAL_PLUG_TYPE_PTR(taiko->mbhc_cfg.calibration);

	n_ready = taiko_mbhc_cal_btn_det_mp(btn_det, TAIKO_BTN_DET_N_READY);
	if (taiko->mbhc_cfg.mclk_rate == TAIKO_MCLK_RATE_12288KHZ) {
		taiko->mbhc_data.npoll = 4;
		taiko->mbhc_data.nbounce_wait = 30;
	} else if (taiko->mbhc_cfg.mclk_rate == TAIKO_MCLK_RATE_9600KHZ) {
		taiko->mbhc_data.npoll = 7;
		taiko->mbhc_data.nbounce_wait = 23;
	}

	taiko->mbhc_data.t_sta_dce = ((1000 * 256) /
				      (taiko->mbhc_cfg.mclk_rate / 1000) *
				      n_ready[taiko_codec_mclk_index(taiko)]) +
				     10;
	taiko->mbhc_data.v_ins_hu =
	    taiko_codec_v_sta_dce(codec, STA, plug_type->v_hs_max);
	taiko->mbhc_data.v_ins_h =
	    taiko_codec_v_sta_dce(codec, DCE, plug_type->v_hs_max);

	taiko->mbhc_data.v_inval_ins_low = TAIKO_MBHC_FAKE_INSERT_LOW;
	if (taiko->mbhc_cfg.gpio)
		taiko->mbhc_data.v_inval_ins_high =
		    TAIKO_MBHC_FAKE_INSERT_HIGH;
	else
		taiko->mbhc_data.v_inval_ins_high =
		    TAIKO_MBHC_FAKE_INS_HIGH_NO_GPIO;

	if (taiko->mbhc_data.micb_mv != VDDIO_MICBIAS_MV) {
		taiko->mbhc_data.adj_v_hs_max =
		    taiko_scale_v_micb_vddio(taiko, plug_type->v_hs_max, true);
		taiko->mbhc_data.adj_v_ins_hu =
		    taiko_codec_v_sta_dce(codec, STA,
					  taiko->mbhc_data.adj_v_hs_max);
		taiko->mbhc_data.adj_v_ins_h =
		    taiko_codec_v_sta_dce(codec, DCE,
					  taiko->mbhc_data.adj_v_hs_max);
		taiko->mbhc_data.v_inval_ins_low =
		    taiko_scale_v_micb_vddio(taiko,
					     taiko->mbhc_data.v_inval_ins_low,
					     false);
		taiko->mbhc_data.v_inval_ins_high =
		    taiko_scale_v_micb_vddio(taiko,
					     taiko->mbhc_data.v_inval_ins_high,
					     false);
	}

	btn_high = taiko_mbhc_cal_btn_det_mp(btn_det, TAIKO_BTN_DET_V_BTN_HIGH);
	for (i = 0; i < btn_det->num_btn; i++)
		btn_mv = btn_high[i] > btn_mv ? btn_high[i] : btn_mv;

	taiko->mbhc_data.v_b1_h = taiko_codec_v_sta_dce(codec, DCE, btn_mv);
	btn_delta_mv = btn_mv + btn_det->v_btn_press_delta_sta;
	taiko->mbhc_data.v_b1_hu =
	    taiko_codec_v_sta_dce(codec, STA, btn_delta_mv);

	btn_delta_mv = btn_mv + btn_det->v_btn_press_delta_cic;

	taiko->mbhc_data.v_b1_huc =
	    taiko_codec_v_sta_dce(codec, DCE, btn_delta_mv);

	taiko->mbhc_data.v_brh = taiko->mbhc_data.v_b1_h;
	taiko->mbhc_data.v_brl = TAIKO_MBHC_BUTTON_MIN;

	taiko->mbhc_data.v_no_mic =
	    taiko_codec_v_sta_dce(codec, STA, plug_type->v_no_mic);
}

void taiko_mbhc_init(struct snd_soc_codec *codec)
{
	struct taiko_priv *taiko;
	struct taiko_mbhc_general_cfg *generic;
	struct taiko_mbhc_btn_detect_cfg *btn_det;
	int n;
	u8 *n_cic, *gain;

	taiko = snd_soc_codec_get_drvdata(codec);
	generic = TAIKO_MBHC_CAL_GENERAL_PTR(taiko->mbhc_cfg.calibration);
	btn_det = TAIKO_MBHC_CAL_BTN_DET_PTR(taiko->mbhc_cfg.calibration);

	for (n = 0; n < 8; n++) {
			snd_soc_update_bits(codec,
					    TAIKO_A_CDC_MBHC_FIR_B1_CFG,
					    0x07, n);
			snd_soc_write(codec, TAIKO_A_CDC_MBHC_FIR_B2_CFG,
				      btn_det->c[n]);
	}

	snd_soc_update_bits(codec, TAIKO_A_CDC_MBHC_B2_CTL, 0x07,
			    btn_det->nc);

	n_cic = taiko_mbhc_cal_btn_det_mp(btn_det, TAIKO_BTN_DET_N_CIC);
	snd_soc_update_bits(codec, TAIKO_A_CDC_MBHC_TIMER_B6_CTL, 0xFF,
			    n_cic[taiko_codec_mclk_index(taiko)]);

	gain = taiko_mbhc_cal_btn_det_mp(btn_det, TAIKO_BTN_DET_GAIN);
	snd_soc_update_bits(codec, TAIKO_A_CDC_MBHC_B2_CTL, 0x78,
			    gain[taiko_codec_mclk_index(taiko)] << 3);

	snd_soc_update_bits(codec, TAIKO_A_CDC_MBHC_TIMER_B4_CTL, 0x70,
			    generic->mbhc_nsa << 4);

	snd_soc_update_bits(codec, TAIKO_A_CDC_MBHC_TIMER_B4_CTL, 0x0F,
			    btn_det->n_meas);

	snd_soc_write(codec, TAIKO_A_CDC_MBHC_TIMER_B5_CTL, generic->mbhc_navg);

	snd_soc_update_bits(codec, TAIKO_A_CDC_MBHC_B1_CTL, 0x80, 0x80);

	snd_soc_update_bits(codec, TAIKO_A_CDC_MBHC_B1_CTL, 0x78,
			    btn_det->mbhc_nsc << 3);

	snd_soc_update_bits(codec, taiko->reg_addr.micb_4_mbhc, 0x03,
			    TAIKO_MICBIAS2);

	snd_soc_update_bits(codec, TAIKO_A_CDC_MBHC_B1_CTL, 0x02, 0x02);

	snd_soc_update_bits(codec, TAIKO_A_MBHC_SCALING_MUX_2, 0xF0, 0xF0);
}

static bool taiko_mbhc_fw_validate(const struct firmware *fw)
{
	u32 cfg_offset;
	struct taiko_mbhc_imped_detect_cfg *imped_cfg;
	struct taiko_mbhc_btn_detect_cfg *btn_cfg;

	if (fw->size < TAIKO_MBHC_CAL_MIN_SIZE)
		return false;

	/* previous check guarantees that there is enough fw data up
	 * to num_btn
	 */
	btn_cfg = TAIKO_MBHC_CAL_BTN_DET_PTR(fw->data);
	cfg_offset = (u32) ((void *) btn_cfg - (void *) fw->data);
	if (fw->size < (cfg_offset + TAIKO_MBHC_CAL_BTN_SZ(btn_cfg)))
		return false;

	/* previous check guarantees that there is enough fw data up
	 * to start of impedance detection configuration
	 */
	imped_cfg = TAIKO_MBHC_CAL_IMPED_DET_PTR(fw->data);
	cfg_offset = (u32) ((void *) imped_cfg - (void *) fw->data);

	if (fw->size < (cfg_offset + TAIKO_MBHC_CAL_IMPED_MIN_SZ))
		return false;

	if (fw->size < (cfg_offset + TAIKO_MBHC_CAL_IMPED_SZ(imped_cfg)))
		return false;

	return true;
}

/* called under codec_resource_lock acquisition */
static int taiko_determine_button(const struct taiko_priv *priv,
				  const s32 micmv)
{
	s16 *v_btn_low, *v_btn_high;
	struct taiko_mbhc_btn_detect_cfg *btn_det;
	int i, btn = -1;

	btn_det = TAIKO_MBHC_CAL_BTN_DET_PTR(priv->mbhc_cfg.calibration);
	v_btn_low = taiko_mbhc_cal_btn_det_mp(btn_det, TAIKO_BTN_DET_V_BTN_LOW);
	v_btn_high = taiko_mbhc_cal_btn_det_mp(btn_det,
				TAIKO_BTN_DET_V_BTN_HIGH);

	for (i = 0; i < btn_det->num_btn; i++) {
		if ((v_btn_low[i] <= micmv) && (v_btn_high[i] >= micmv)) {
			btn = i;
			break;
		}
	}

	if (btn == -1)
		pr_debug("%s: couldn't find button number for mic mv %d\n",
			 __func__, micmv);

	return btn;
}

static int taiko_get_button_mask(const int btn)
{
	int mask = 0;
	switch (btn) {
	case 0:
		mask = SND_JACK_BTN_0;
		break;
	case 1:
		mask = SND_JACK_BTN_1;
		break;
	case 2:
		mask = SND_JACK_BTN_2;
		break;
	case 3:
		mask = SND_JACK_BTN_3;
		break;
	case 4:
		mask = SND_JACK_BTN_4;
		break;
	case 5:
		mask = SND_JACK_BTN_5;
		break;
	case 6:
		mask = SND_JACK_BTN_6;
		break;
	case 7:
		mask = SND_JACK_BTN_7;
		break;
	}
	return mask;
}

static irqreturn_t taiko_dce_handler(int irq, void *data)
{
	int i, mask;
	short dce, sta;
	s32 mv, mv_s, stamv_s;
	bool vddio;
	int btn = -1, meas = 0;
	struct taiko_priv *priv = data;
	const struct taiko_mbhc_btn_detect_cfg *d =
	    TAIKO_MBHC_CAL_BTN_DET_PTR(priv->mbhc_cfg.calibration);
	short btnmeas[d->n_btn_meas + 1];
	struct snd_soc_codec *codec = priv->codec;
	struct wcd9xxx *core = dev_get_drvdata(priv->codec->dev->parent);
	int n_btn_meas = d->n_btn_meas;
	u8 mbhc_status = snd_soc_read(codec, TAIKO_A_CDC_MBHC_B1_STATUS) & 0x3E;

	pr_debug("%s: enter\n", __func__);

	TAIKO_ACQUIRE_LOCK(priv->codec_resource_lock);
	if (priv->mbhc_state == MBHC_STATE_POTENTIAL_RECOVERY) {
		pr_debug("%s: mbhc is being recovered, skip button press\n",
			 __func__);
		goto done;
	}

	priv->mbhc_state = MBHC_STATE_POTENTIAL;

	if (!priv->mbhc_polling_active) {
		pr_warn("%s: mbhc polling is not active, skip button press\n",
			__func__);
		goto done;
	}

	dce = taiko_codec_read_dce_result(codec);
	mv = taiko_codec_sta_dce_v(codec, 1, dce);

	/* If GPIO interrupt already kicked in, ignore button press */
	if (priv->in_gpio_handler) {
		pr_debug("%s: GPIO State Changed, ignore button press\n",
			 __func__);
		btn = -1;
		goto done;
	}

	vddio = (priv->mbhc_data.micb_mv != VDDIO_MICBIAS_MV &&
		 priv->mbhc_micbias_switched);
	mv_s = vddio ? taiko_scale_v_micb_vddio(priv, mv, false) : mv;

	if (mbhc_status != TAIKO_MBHC_STATUS_REL_DETECTION) {
		if (priv->mbhc_last_resume &&
		    !time_after(jiffies, priv->mbhc_last_resume + HZ)) {
			pr_debug("%s: Button is already released shortly after resume\n",
				__func__);
			n_btn_meas = 0;
		} else {
			pr_debug("%s: Button is already released without resume",
				__func__);
			sta = taiko_codec_read_sta_result(codec);
			stamv_s = taiko_codec_sta_dce_v(codec, 0, sta);
			if (vddio)
				stamv_s = taiko_scale_v_micb_vddio(priv,
								   stamv_s,
								   false);
			btn = taiko_determine_button(priv, mv_s);
			if (btn != taiko_determine_button(priv, stamv_s))
				btn = -1;
			goto done;
		}
	}

	/* determine pressed button */
	btnmeas[meas++] = taiko_determine_button(priv, mv_s);
	pr_debug("%s: meas %d - DCE %d,%d,%d button %d\n", __func__,
		 meas - 1, dce, mv, mv_s, btnmeas[meas - 1]);
	if (n_btn_meas == 0)
		btn = btnmeas[0];
	for (; ((d->n_btn_meas) && (meas < (d->n_btn_meas + 1))); meas++) {
		dce = taiko_codec_sta_dce(codec, 1, false);
		mv = taiko_codec_sta_dce_v(codec, 1, dce);
		mv_s = vddio ? taiko_scale_v_micb_vddio(priv, mv, false) : mv;

		btnmeas[meas] = taiko_determine_button(priv, mv_s);
		pr_debug("%s: meas %d - DCE %d,%d,%d button %d\n",
			 __func__, meas, dce, mv, mv_s, btnmeas[meas]);
		/* if large enough measurements are collected,
		 * start to check if last all n_btn_con measurements were
		 * in same button low/high range */
		if (meas + 1 >= d->n_btn_con) {
			for (i = 0; i < d->n_btn_con; i++)
				if ((btnmeas[meas] < 0) ||
				    (btnmeas[meas] != btnmeas[meas - i]))
					break;
			if (i == d->n_btn_con) {
				/* button pressed */
				btn = btnmeas[meas];
				break;
			} else if ((n_btn_meas - meas) < (d->n_btn_con - 1)) {
				/* if left measurements are less than n_btn_con,
				 * it's impossible to find button number */
				break;
			}
		}
	}

	if (btn >= 0) {
		if (priv->in_gpio_handler) {
			pr_debug(
			"%s: GPIO already triggered, ignore button press\n",
			__func__);
			goto done;
		}
		mask = taiko_get_button_mask(btn);
		priv->buttons_pressed |= mask;
		wcd9xxx_lock_sleep(core);
		if (schedule_delayed_work(&priv->mbhc_btn_dwork,
					  msecs_to_jiffies(400)) == 0) {
			WARN(1, "Button pressed twice without release event\n");
			wcd9xxx_unlock_sleep(core);
		}
	} else {
		pr_debug("%s: bogus button press, too short press?\n",
			 __func__);
	}

 done:
	pr_debug("%s: leave\n", __func__);
	TAIKO_RELEASE_LOCK(priv->codec_resource_lock);
	return IRQ_HANDLED;
}

static int taiko_is_fake_press(struct taiko_priv *priv)
{
	int i;
	int r = 0;
	struct snd_soc_codec *codec = priv->codec;
	const int dces = MBHC_NUM_DCE_PLUG_DETECT;
	s16 mb_v, v_ins_hu, v_ins_h;

	v_ins_hu = taiko_get_current_v_ins(priv, true);
	v_ins_h = taiko_get_current_v_ins(priv, false);

	for (i = 0; i < dces; i++) {
		usleep_range(10000, 10000);
		if (i == 0) {
			mb_v = taiko_codec_sta_dce(codec, 0, true);
			pr_debug("%s: STA[0]: %d,%d\n", __func__, mb_v,
				 taiko_codec_sta_dce_v(codec, 0, mb_v));
			if (mb_v < (s16)priv->mbhc_data.v_b1_hu ||
			    mb_v > v_ins_hu) {
				r = 1;
				break;
			}
		} else {
			mb_v = taiko_codec_sta_dce(codec, 1, true);
			pr_debug("%s: DCE[%d]: %d,%d\n", __func__, i, mb_v,
				 taiko_codec_sta_dce_v(codec, 1, mb_v));
			if (mb_v < (s16)priv->mbhc_data.v_b1_h ||
			    mb_v > v_ins_h) {
				r = 1;
				break;
			}
		}
	}

	return r;
}

static irqreturn_t taiko_release_handler(int irq, void *data)
{
	int ret;
	struct taiko_priv *priv = data;
	struct snd_soc_codec *codec = priv->codec;

	pr_debug("%s: enter\n", __func__);

	TAIKO_ACQUIRE_LOCK(priv->codec_resource_lock);
	priv->mbhc_state = MBHC_STATE_RELEASE;

	taiko_codec_drive_v_to_micbias(codec, 10000);

	if (priv->buttons_pressed & TAIKO_JACK_BUTTON_MASK) {
		ret = taiko_cancel_btn_work(priv);
		if (ret == 0) {
			pr_debug("%s: Reporting long button release event\n",
				 __func__);
			if (priv->mbhc_cfg.button_jack)
				taiko_snd_soc_jack_report(priv,
						  priv->mbhc_cfg.button_jack, 0,
						  priv->buttons_pressed);
		} else {
			if (taiko_is_fake_press(priv)) {
				pr_debug("%s: Fake button press interrupt\n",
					 __func__);
			} else if (priv->mbhc_cfg.button_jack) {
				if (priv->in_gpio_handler) {
					pr_debug("%s: GPIO kicked in, ignore\n",
						 __func__);
				} else {
					pr_debug(
					"%s: Reporting short button press and release\n",
					 __func__);
					taiko_snd_soc_jack_report(priv,
						     priv->mbhc_cfg.button_jack,
						     priv->buttons_pressed,
						     priv->buttons_pressed);
					taiko_snd_soc_jack_report(priv,
						  priv->mbhc_cfg.button_jack, 0,
						  priv->buttons_pressed);
				}
			}
		}

		priv->buttons_pressed &= ~TAIKO_JACK_BUTTON_MASK;
	}

	taiko_codec_calibrate_hs_polling(codec);

	if (priv->mbhc_cfg.gpio)
		msleep(TAIKO_MBHC_GPIO_REL_DEBOUNCE_TIME_MS);

	taiko_codec_start_hs_polling(codec);

	pr_debug("%s: leave\n", __func__);
	TAIKO_RELEASE_LOCK(priv->codec_resource_lock);
	return IRQ_HANDLED;
}

static void taiko_codec_shutdown_hs_removal_detect(struct snd_soc_codec *codec)
{
	struct taiko_priv *taiko = snd_soc_codec_get_drvdata(codec);
	const struct taiko_mbhc_general_cfg *generic =
	    TAIKO_MBHC_CAL_GENERAL_PTR(taiko->mbhc_cfg.calibration);

	if (!taiko->mclk_enabled && !taiko->mbhc_polling_active)
		taiko_codec_enable_config_mode(codec, 1);

	snd_soc_update_bits(codec, TAIKO_A_CDC_MBHC_CLK_CTL, 0x2, 0x2);
	snd_soc_update_bits(codec, TAIKO_A_CDC_MBHC_B1_CTL, 0x6, 0x0);

	snd_soc_update_bits(codec, taiko->mbhc_bias_regs.mbhc_reg, 0x80, 0x00);

	usleep_range(generic->t_shutdown_plug_rem,
		     generic->t_shutdown_plug_rem);

	snd_soc_update_bits(codec, TAIKO_A_CDC_MBHC_CLK_CTL, 0xA, 0x8);
	if (!taiko->mclk_enabled && !taiko->mbhc_polling_active)
		taiko_codec_enable_config_mode(codec, 0);

	snd_soc_write(codec, TAIKO_A_MBHC_SCALING_MUX_1, 0x00);
}

static void taiko_codec_cleanup_hs_polling(struct snd_soc_codec *codec)
{
	struct taiko_priv *taiko = snd_soc_codec_get_drvdata(codec);

	taiko_codec_shutdown_hs_removal_detect(codec);

	if (!taiko->mclk_enabled) {
		taiko_codec_disable_clock_block(codec);
		taiko_codec_enable_bandgap(codec, TAIKO_BANDGAP_OFF);
	}

	taiko->mbhc_polling_active = false;
	taiko->mbhc_state = MBHC_STATE_NONE;
}

static irqreturn_t taiko_hphl_ocp_irq(int irq, void *data)
{
	struct taiko_priv *taiko = data;
	struct snd_soc_codec *codec;

	pr_info("%s: received HPHL OCP irq\n", __func__);

	if (taiko) {
		codec = taiko->codec;
		if (taiko->hphlocp_cnt++ < TAIKO_OCP_ATTEMPT) {
			pr_info("%s: retry\n", __func__);
			snd_soc_update_bits(codec, TAIKO_A_RX_HPH_OCP_CTL, 0x10,
					    0x00);
			snd_soc_update_bits(codec, TAIKO_A_RX_HPH_OCP_CTL, 0x10,
					    0x10);
		} else {
			wcd9xxx_disable_irq(codec->control_data,
					  TAIKO_IRQ_HPH_PA_OCPL_FAULT);
			taiko->hphlocp_cnt = 0;
			taiko->hph_status |= SND_JACK_OC_HPHL;
			if (taiko->mbhc_cfg.headset_jack)
				taiko_snd_soc_jack_report(taiko,
						   taiko->mbhc_cfg.headset_jack,
						   taiko->hph_status,
						   TAIKO_JACK_MASK);
		}
	} else {
		pr_err("%s: Bad taiko private data\n", __func__);
	}

	return IRQ_HANDLED;
}

static irqreturn_t taiko_hphr_ocp_irq(int irq, void *data)
{
	struct taiko_priv *taiko = data;
	struct snd_soc_codec *codec;

	pr_info("%s: received HPHR OCP irq\n", __func__);

	if (taiko) {
		codec = taiko->codec;
		if (taiko->hphrocp_cnt++ < TAIKO_OCP_ATTEMPT) {
			pr_info("%s: retry\n", __func__);
			snd_soc_update_bits(codec, TAIKO_A_RX_HPH_OCP_CTL, 0x10,
					    0x00);
			snd_soc_update_bits(codec, TAIKO_A_RX_HPH_OCP_CTL, 0x10,
					    0x10);
		} else {
			wcd9xxx_disable_irq(codec->control_data,
					  TAIKO_IRQ_HPH_PA_OCPR_FAULT);
			taiko->hphrocp_cnt = 0;
			taiko->hph_status |= SND_JACK_OC_HPHR;
			if (taiko->mbhc_cfg.headset_jack)
				taiko_snd_soc_jack_report(taiko,
						   taiko->mbhc_cfg.headset_jack,
						   taiko->hph_status,
						   TAIKO_JACK_MASK);
		}
	} else {
		pr_err("%s: Bad taiko private data\n", __func__);
	}

	return IRQ_HANDLED;
}

static bool taiko_is_inval_ins_range(struct snd_soc_codec *codec,
				     s32 mic_volt, bool highhph, bool *highv)
{
	struct taiko_priv *taiko = snd_soc_codec_get_drvdata(codec);
	bool invalid = false;
	s16 v_hs_max;

	/* Perform this check only when the high voltage headphone
	 * needs to be considered as invalid
	 */
	v_hs_max = taiko_get_current_v_hs_max(taiko);
	*highv = mic_volt > v_hs_max;
	if (!highhph && *highv)
		invalid = true;
	else if (mic_volt < taiko->mbhc_data.v_inval_ins_high &&
		 (mic_volt > taiko->mbhc_data.v_inval_ins_low))
		invalid = true;

	return invalid;
}

static bool taiko_is_inval_ins_delta(struct snd_soc_codec *codec,
				     int mic_volt, int mic_volt_prev,
				     int threshold)
{
	return abs(mic_volt - mic_volt_prev) > threshold;
}

/* called under codec_resource_lock acquisition */
void taiko_find_plug_and_report(struct snd_soc_codec *codec,
				enum taiko_mbhc_plug_type plug_type)
{
	struct taiko_priv *taiko = snd_soc_codec_get_drvdata(codec);

	if (plug_type == PLUG_TYPE_HEADPHONE &&
	    taiko->current_plug == PLUG_TYPE_NONE) {
		/* Nothing was reported previously
		 * report a headphone or unsupported
		 */
		taiko_codec_report_plug(codec, 1, SND_JACK_HEADPHONE);
		taiko_codec_cleanup_hs_polling(codec);
	} else if (plug_type == PLUG_TYPE_GND_MIC_SWAP) {
		if (taiko->current_plug == PLUG_TYPE_HEADSET)
			taiko_codec_report_plug(codec, 0, SND_JACK_HEADSET);
		else if (taiko->current_plug == PLUG_TYPE_HEADPHONE)
			taiko_codec_report_plug(codec, 0, SND_JACK_HEADPHONE);

		taiko_codec_report_plug(codec, 1, SND_JACK_UNSUPPORTED);
		taiko_codec_cleanup_hs_polling(codec);
	} else if (plug_type == PLUG_TYPE_HEADSET) {
		/* If Headphone was reported previously, this will
		 * only report the mic line
		 */
		taiko_codec_report_plug(codec, 1, SND_JACK_HEADSET);
		msleep(100);
		taiko_codec_start_hs_polling(codec);
	} else if (plug_type == PLUG_TYPE_HIGH_HPH) {
		if (taiko->current_plug == PLUG_TYPE_NONE)
			taiko_codec_report_plug(codec, 1, SND_JACK_HEADPHONE);
		taiko_codec_cleanup_hs_polling(codec);
		pr_debug("setup mic trigger for further detection\n");
		taiko->lpi_enabled = true;
		taiko_codec_enable_hs_detect(codec, 1,
					     MBHC_USE_MB_TRIGGER |
					     MBHC_USE_HPHL_TRIGGER,
					     false);
	} else {
		WARN(1, "Unexpected current plug_type %d, plug_type %d\n",
		     taiko->current_plug, plug_type);
	}
}

/* should be called under interrupt context that hold suspend */
static void taiko_schedule_hs_detect_plug(struct taiko_priv *taiko)
{
	pr_debug("%s: scheduling taiko_hs_correct_gpio_plug\n", __func__);
	taiko->hs_detect_work_stop = false;
	wcd9xxx_lock_sleep(taiko->codec->control_data);
	schedule_work(&taiko->hs_correct_plug_work);
}

/* called under codec_resource_lock acquisition */
static void taiko_cancel_hs_detect_plug(struct taiko_priv *taiko)
{
	pr_debug("%s: canceling hs_correct_plug_work\n", __func__);
	taiko->hs_detect_work_stop = true;
	wmb();
	TAIKO_RELEASE_LOCK(taiko->codec_resource_lock);
	if (cancel_work_sync(&taiko->hs_correct_plug_work)) {
		pr_debug("%s: hs_correct_plug_work is canceled\n", __func__);
		wcd9xxx_unlock_sleep(taiko->codec->control_data);
	}
	TAIKO_ACQUIRE_LOCK(taiko->codec_resource_lock);
}

static bool taiko_hs_gpio_level_remove(struct taiko_priv *taiko)
{
	return (gpio_get_value_cansleep(taiko->mbhc_cfg.gpio) !=
		taiko->mbhc_cfg.gpio_level_insert);
}

/* called under codec_resource_lock acquisition */
static void taiko_codec_hphr_gnd_switch(struct snd_soc_codec *codec, bool on)
{
	snd_soc_update_bits(codec, TAIKO_A_MBHC_HPH, 0x01, on);
	if (on)
		usleep_range(5000, 5000);
}

/* called under codec_resource_lock acquisition and mbhc override = 1 */
static enum taiko_mbhc_plug_type
taiko_codec_get_plug_type(struct snd_soc_codec *codec, bool highhph)
{
	int i;
	bool gndswitch, vddioswitch;
	int scaled;
	struct taiko_mbhc_plug_type_cfg *plug_type_ptr;
	struct taiko_priv *taiko = snd_soc_codec_get_drvdata(codec);
	const bool vddio = (taiko->mbhc_data.micb_mv != VDDIO_MICBIAS_MV);
	int num_det = (MBHC_NUM_DCE_PLUG_DETECT + vddio);
	enum taiko_mbhc_plug_type plug_type[num_det];
	s16 mb_v[num_det];
	s32 mic_mv[num_det];
	bool inval;
	bool highdelta;
	bool ahighv = false, highv;

	/* make sure override is on */
	WARN_ON(!(snd_soc_read(codec, TAIKO_A_CDC_MBHC_B1_CTL) & 0x04));

	/* GND and MIC swap detection requires at least 2 rounds of DCE */
	BUG_ON(num_det < 2);

	plug_type_ptr =
	    TAIKO_MBHC_CAL_PLUG_TYPE_PTR(taiko->mbhc_cfg.calibration);

	plug_type[0] = PLUG_TYPE_INVALID;

	/* performs DCEs for N times
	 * 1st: check if voltage is in invalid range
	 * 2nd - N-2nd: check voltage range and delta
	 * N-1st: check voltage range, delta with HPHR GND switch
	 * Nth: check voltage range with VDDIO switch if micbias V != vddio V*/
	for (i = 0; i < num_det; i++) {
		gndswitch = (i == (num_det - 1 - vddio));
		vddioswitch = (vddio && ((i == num_det - 1) ||
					 (i == num_det - 2)));
		if (i == 0) {
			mb_v[i] = taiko_codec_setup_hs_polling(codec);
			mic_mv[i] = taiko_codec_sta_dce_v(codec, 1 , mb_v[i]);
			inval = taiko_is_inval_ins_range(codec, mic_mv[i],
							 highhph, &highv);
			ahighv |= highv;
			scaled = mic_mv[i];
		} else {
			if (vddioswitch)
				__taiko_codec_switch_micbias(taiko->codec, 1,
							     false, false);
			if (gndswitch)
				taiko_codec_hphr_gnd_switch(codec, true);
			mb_v[i] = __taiko_codec_sta_dce(codec, 1, true, true);
			mic_mv[i] = taiko_codec_sta_dce_v(codec, 1 , mb_v[i]);
			if (vddioswitch)
				scaled = taiko_scale_v_micb_vddio(taiko,
								  mic_mv[i],
								  false);
			else
				scaled = mic_mv[i];
			/* !gndswitch & vddioswitch means the previous DCE
			 * was done with gndswitch, don't compare with DCE
			 * with gndswitch */
			highdelta = taiko_is_inval_ins_delta(codec, scaled,
					mic_mv[i - !gndswitch - vddioswitch],
					TAIKO_MBHC_FAKE_INS_DELTA_SCALED_MV);
			inval = (taiko_is_inval_ins_range(codec, mic_mv[i],
							  highhph, &highv) ||
				 highdelta);
			ahighv |= highv;
			if (gndswitch)
				taiko_codec_hphr_gnd_switch(codec, false);
			if (vddioswitch)
				__taiko_codec_switch_micbias(taiko->codec, 0,
							     false, false);
			/* claim UNSUPPORTED plug insertion when
			 * good headset is detected but HPHR GND switch makes
			 * delta difference */
			if (i == (num_det - 2) && highdelta && !ahighv)
				plug_type[0] = PLUG_TYPE_GND_MIC_SWAP;
			else if (i == (num_det - 1) && inval)
				plug_type[0] = PLUG_TYPE_INVALID;
		}
		pr_debug("%s: DCE #%d, %04x, V %d, scaled V %d, GND %d, VDDIO %d, inval %d\n",
			__func__, i + 1, mb_v[i] & 0xffff, mic_mv[i], scaled,
			gndswitch, vddioswitch, inval);
		/* don't need to run further DCEs */
		if (ahighv && inval)
			break;
		mic_mv[i] = scaled;
	}

	for (i = 0; (plug_type[0] != PLUG_TYPE_GND_MIC_SWAP && !inval) &&
		     i < num_det; i++) {
		/*
		 * If we are here, means none of the all
		 * measurements are fake, continue plug type detection.
		 * If all three measurements do not produce same
		 * plug type, restart insertion detection
		 */
		if (mic_mv[i] < plug_type_ptr->v_no_mic) {
			plug_type[i] = PLUG_TYPE_HEADPHONE;
			pr_debug("%s: Detect attempt %d, detected Headphone\n",
				 __func__, i);
		} else if (highhph && (mic_mv[i] > plug_type_ptr->v_hs_max)) {
			plug_type[i] = PLUG_TYPE_HIGH_HPH;
			pr_debug(
			"%s: Detect attempt %d, detected High Headphone\n",
			__func__, i);
		} else {
			plug_type[i] = PLUG_TYPE_HEADSET;
			pr_debug("%s: Detect attempt %d, detected Headset\n",
				 __func__, i);
		}

		if (i > 0 && (plug_type[i - 1] != plug_type[i])) {
			pr_err("%s: Detect attempt %d and %d are not same",
			       __func__, i - 1, i);
			plug_type[0] = PLUG_TYPE_INVALID;
			inval = true;
			break;
		}
	}

	pr_debug("%s: Detected plug type %d\n", __func__, plug_type[0]);
	return plug_type[0];
}

static void taiko_hs_correct_gpio_plug(struct work_struct *work)
{
	struct taiko_priv *taiko;
	struct snd_soc_codec *codec;
	int retry = 0, pt_gnd_mic_swap_cnt = 0;
	bool correction = false;
	enum taiko_mbhc_plug_type plug_type;
	unsigned long timeout;

	taiko = container_of(work, struct taiko_priv, hs_correct_plug_work);
	codec = taiko->codec;

	pr_debug("%s: enter\n", __func__);
	taiko->mbhc_cfg.mclk_cb_fn(codec, 1, false);

	/* Keep override on during entire plug type correction work.
	 *
	 * This is okay under the assumption that any GPIO irqs which use
	 * MBHC block cancel and sync this work so override is off again
	 * prior to GPIO interrupt handler's MBHC block usage.
	 * Also while this correction work is running, we can guarantee
	 * DAPM doesn't use any MBHC block as this work only runs with
	 * headphone detection.
	 */
	taiko_turn_onoff_override(codec, true);

	timeout = jiffies + msecs_to_jiffies(TAIKO_HS_DETECT_PLUG_TIME_MS);
	while (!time_after(jiffies, timeout)) {
		++retry;
		rmb();
		if (taiko->hs_detect_work_stop) {
			pr_debug("%s: stop requested\n", __func__);
			break;
		}

		msleep(TAIKO_HS_DETECT_PLUG_INERVAL_MS);
		if (taiko_hs_gpio_level_remove(taiko)) {
			pr_debug("%s: GPIO value is low\n", __func__);
			break;
		}

		/* can race with removal interrupt */
		TAIKO_ACQUIRE_LOCK(taiko->codec_resource_lock);
		plug_type = taiko_codec_get_plug_type(codec, true);
		TAIKO_RELEASE_LOCK(taiko->codec_resource_lock);

		if (plug_type == PLUG_TYPE_INVALID) {
			pr_debug("Invalid plug in attempt # %d\n", retry);
			if (retry == NUM_ATTEMPTS_TO_REPORT &&
			    taiko->current_plug == PLUG_TYPE_NONE) {
				taiko_codec_report_plug(codec, 1,
							SND_JACK_HEADPHONE);
			}
		} else if (plug_type == PLUG_TYPE_HEADPHONE) {
			pr_debug("Good headphone detected, continue polling mic\n");
			if (taiko->current_plug == PLUG_TYPE_NONE)
				taiko_codec_report_plug(codec, 1,
							SND_JACK_HEADPHONE);
		} else {
			if (plug_type == PLUG_TYPE_GND_MIC_SWAP) {
				pt_gnd_mic_swap_cnt++;
				if (pt_gnd_mic_swap_cnt <
				    TAIKO_MBHC_GND_MIC_SWAP_THRESHOLD)
					continue;
				else if (pt_gnd_mic_swap_cnt >
					 TAIKO_MBHC_GND_MIC_SWAP_THRESHOLD) {
					/* This is due to GND/MIC switch didn't
					 * work,  Report unsupported plug */
				} else if (taiko->mbhc_cfg.swap_gnd_mic) {
					/* if switch is toggled, check again,
					 * otherwise report unsupported plug */
					if (taiko->mbhc_cfg.swap_gnd_mic(codec))
						continue;
				}
			} else
				pt_gnd_mic_swap_cnt = 0;

			TAIKO_ACQUIRE_LOCK(taiko->codec_resource_lock);
			/* Turn off override */
			taiko_turn_onoff_override(codec, false);
			/* The valid plug also includes PLUG_TYPE_GND_MIC_SWAP
			 */
			taiko_find_plug_and_report(codec, plug_type);
			TAIKO_RELEASE_LOCK(taiko->codec_resource_lock);
			pr_debug("Attempt %d found correct plug %d\n", retry,
				 plug_type);
			correction = true;
			break;
		}
	}

	/* Turn off override */
	if (!correction)
		taiko_turn_onoff_override(codec, false);

	taiko->mbhc_cfg.mclk_cb_fn(codec, 0, false);
	pr_debug("%s: leave\n", __func__);
	/* unlock sleep */
	wcd9xxx_unlock_sleep(taiko->codec->control_data);
}

/* called under codec_resource_lock acquisition */
static void taiko_codec_decide_gpio_plug(struct snd_soc_codec *codec)
{
	enum taiko_mbhc_plug_type plug_type;
	struct taiko_priv *taiko = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s: enter\n", __func__);

	taiko_turn_onoff_override(codec, true);
	plug_type = taiko_codec_get_plug_type(codec, true);
	taiko_turn_onoff_override(codec, false);

	if (taiko_hs_gpio_level_remove(taiko)) {
		pr_debug("%s: GPIO value is low when determining plug\n",
			 __func__);
		return;
	}

	if (plug_type == PLUG_TYPE_INVALID ||
	    plug_type == PLUG_TYPE_GND_MIC_SWAP) {
		taiko_schedule_hs_detect_plug(taiko);
	} else if (plug_type == PLUG_TYPE_HEADPHONE) {
		taiko_codec_report_plug(codec, 1, SND_JACK_HEADPHONE);

		taiko_schedule_hs_detect_plug(taiko);
	} else {
		pr_debug("%s: Valid plug found, determine plug type %d\n",
			 __func__, plug_type);
		taiko_find_plug_and_report(codec, plug_type);
	}
}

/* called under codec_resource_lock acquisition */
static void taiko_codec_detect_plug_type(struct snd_soc_codec *codec)
{
	enum taiko_mbhc_plug_type plug_type;
	struct taiko_priv *taiko = snd_soc_codec_get_drvdata(codec);
	const struct taiko_mbhc_plug_detect_cfg *plug_det =
	    TAIKO_MBHC_CAL_PLUG_DET_PTR(taiko->mbhc_cfg.calibration);

	/* Turn on the override,
	 * taiko_codec_setup_hs_polling requires override on */
	taiko_turn_onoff_override(codec, true);

	if (plug_det->t_ins_complete > 20)
		msleep(plug_det->t_ins_complete);
	else
		usleep_range(plug_det->t_ins_complete * 1000,
			     plug_det->t_ins_complete * 1000);

	if (taiko->mbhc_cfg.gpio) {
		/* Turn off the override */
		taiko_turn_onoff_override(codec, false);
		if (taiko_hs_gpio_level_remove(taiko))
			pr_debug(
			"%s: GPIO value is low when determining plug\n",
			__func__);
		else
			taiko_codec_decide_gpio_plug(codec);
		return;
	}

	plug_type = taiko_codec_get_plug_type(codec, false);
	taiko_turn_onoff_override(codec, false);

	if (plug_type == PLUG_TYPE_INVALID) {
		pr_debug("%s: Invalid plug type detected\n", __func__);
		snd_soc_update_bits(codec, TAIKO_A_CDC_MBHC_B1_CTL, 0x02, 0x02);
		taiko_codec_cleanup_hs_polling(codec);
		taiko_codec_enable_hs_detect(codec, 1,
					     MBHC_USE_MB_TRIGGER |
					     MBHC_USE_HPHL_TRIGGER, false);
	} else if (plug_type == PLUG_TYPE_GND_MIC_SWAP) {
		pr_debug("%s: GND-MIC swapped plug type detected\n", __func__);
		taiko_codec_report_plug(codec, 1, SND_JACK_UNSUPPORTED);
		taiko_codec_cleanup_hs_polling(codec);
		taiko_codec_enable_hs_detect(codec, 0, 0, false);
	} else if (plug_type == PLUG_TYPE_HEADPHONE) {
		pr_debug("%s: Headphone Detected\n", __func__);
		taiko_codec_report_plug(codec, 1, SND_JACK_HEADPHONE);
		taiko_codec_cleanup_hs_polling(codec);
		taiko_codec_enable_hs_detect(codec, 0, 0, false);
	} else if (plug_type == PLUG_TYPE_HEADSET) {
		pr_debug("%s: Headset detected\n", __func__);
		taiko_codec_report_plug(codec, 1, SND_JACK_HEADSET);

		/* avoid false button press detect */
		msleep(50);
		taiko_codec_start_hs_polling(codec);
	}
}

/* called only from interrupt which is under codec_resource_lock acquisition */
static void taiko_hs_insert_irq_gpio(struct taiko_priv *priv, bool is_removal)
{
	struct snd_soc_codec *codec = priv->codec;

	if (!is_removal) {
		pr_debug("%s: MIC trigger insertion interrupt\n", __func__);

		rmb();
		if (priv->lpi_enabled)
			msleep(100);

		rmb();
		if (!priv->lpi_enabled) {
			pr_debug("%s: lpi is disabled\n", __func__);
		} else if (gpio_get_value_cansleep(priv->mbhc_cfg.gpio) ==
			   priv->mbhc_cfg.gpio_level_insert) {
			pr_debug(
			"%s: Valid insertion, detect plug type\n", __func__);
			taiko_codec_decide_gpio_plug(codec);
		} else {
			pr_debug(
			"%s: Invalid insertion stop plug detection\n",
			__func__);
		}
	} else {
		pr_err("%s: GPIO used, invalid MBHC Removal\n", __func__);
	}
}

/* called only from interrupt which is under codec_resource_lock acquisition */
static void taiko_hs_insert_irq_nogpio(struct taiko_priv *priv, bool is_removal,
				       bool is_mb_trigger)
{
	int ret;
	struct snd_soc_codec *codec = priv->codec;
	struct wcd9xxx *core = dev_get_drvdata(priv->codec->dev->parent);

	if (is_removal) {
		/* cancel possiblely running hs detect work */
		taiko_cancel_hs_detect_plug(priv);

		/*
		 * If headphone is removed while playback is in progress,
		 * it is possible that micbias will be switched to VDDIO.
		 */
		taiko_codec_switch_micbias(codec, 0);
		if (priv->current_plug == PLUG_TYPE_HEADPHONE)
			taiko_codec_report_plug(codec, 0, SND_JACK_HEADPHONE);
		else if (priv->current_plug == PLUG_TYPE_GND_MIC_SWAP)
			taiko_codec_report_plug(codec, 0, SND_JACK_UNSUPPORTED);
		else
			WARN(1, "%s: Unexpected current plug type %d\n",
			     __func__, priv->current_plug);
		taiko_codec_shutdown_hs_removal_detect(codec);
		taiko_codec_enable_hs_detect(codec, 1,
					     MBHC_USE_MB_TRIGGER |
					     MBHC_USE_HPHL_TRIGGER,
					     true);
	} else if (is_mb_trigger && !is_removal) {
		pr_debug("%s: Waiting for Headphone left trigger\n",
			__func__);
		wcd9xxx_lock_sleep(core);
		if (schedule_delayed_work(&priv->mbhc_insert_dwork,
					  usecs_to_jiffies(1000000)) == 0) {
			pr_err("%s: mbhc_insert_dwork is already scheduled\n",
			       __func__);
			wcd9xxx_unlock_sleep(core);
		}
		taiko_codec_enable_hs_detect(codec, 1, MBHC_USE_HPHL_TRIGGER,
					     false);
	} else  {
		ret = cancel_delayed_work(&priv->mbhc_insert_dwork);
		if (ret != 0) {
			pr_debug(
			"%s: Complete plug insertion, Detecting plug type\n",
			__func__);
			taiko_codec_detect_plug_type(codec);
			wcd9xxx_unlock_sleep(core);
		} else {
			wcd9xxx_enable_irq(codec->control_data,
					   TAIKO_IRQ_MBHC_INSERTION);
			pr_err("%s: Error detecting plug insertion\n",
			       __func__);
		}
	}
}

static irqreturn_t taiko_hs_insert_irq(int irq, void *data)
{
	bool is_mb_trigger, is_removal;
	struct taiko_priv *priv = data;
	struct snd_soc_codec *codec = priv->codec;

	pr_debug("%s: enter\n", __func__);
	TAIKO_ACQUIRE_LOCK(priv->codec_resource_lock);
	wcd9xxx_disable_irq(codec->control_data, TAIKO_IRQ_MBHC_INSERTION);

	is_mb_trigger = !!(snd_soc_read(codec, priv->mbhc_bias_regs.mbhc_reg) &
					0x10);
	is_removal = !!(snd_soc_read(codec, TAIKO_A_CDC_MBHC_INT_CTL) & 0x02);
	snd_soc_update_bits(codec, TAIKO_A_CDC_MBHC_INT_CTL, 0x03, 0x00);

	/* Turn off both HPH and MIC line schmitt triggers */
	snd_soc_update_bits(codec, priv->mbhc_bias_regs.mbhc_reg, 0x90, 0x00);
	snd_soc_update_bits(codec, TAIKO_A_MBHC_HPH, 0x13, 0x00);
	snd_soc_update_bits(codec, priv->mbhc_bias_regs.ctl_reg, 0x01, 0x00);

	if (priv->mbhc_cfg.gpio)
		taiko_hs_insert_irq_gpio(priv, is_removal);
	else
		taiko_hs_insert_irq_nogpio(priv, is_removal, is_mb_trigger);

	TAIKO_RELEASE_LOCK(priv->codec_resource_lock);
	return IRQ_HANDLED;
}

static bool is_valid_mic_voltage(struct snd_soc_codec *codec, s32 mic_mv)
{
	struct taiko_priv *taiko = snd_soc_codec_get_drvdata(codec);
	const struct taiko_mbhc_plug_type_cfg *plug_type =
	    TAIKO_MBHC_CAL_PLUG_TYPE_PTR(taiko->mbhc_cfg.calibration);
	const s16 v_hs_max = taiko_get_current_v_hs_max(taiko);

	return (!(mic_mv > 10 && mic_mv < 80) && (mic_mv > plug_type->v_no_mic)
		&& (mic_mv < v_hs_max)) ? true : false;
}

/* called under codec_resource_lock acquisition
 * returns true if mic voltage range is back to normal insertion
 * returns false either if timedout or removed */
static bool taiko_hs_remove_settle(struct snd_soc_codec *codec)
{
	int i;
	bool timedout, settled = false;
	s32 mic_mv[MBHC_NUM_DCE_PLUG_DETECT];
	short mb_v[MBHC_NUM_DCE_PLUG_DETECT];
	unsigned long retry = 0, timeout;
	struct taiko_priv *taiko = snd_soc_codec_get_drvdata(codec);
	const s16 v_hs_max = taiko_get_current_v_hs_max(taiko);

	timeout = jiffies + msecs_to_jiffies(TAIKO_HS_DETECT_PLUG_TIME_MS);
	while (!(timedout = time_after(jiffies, timeout))) {
		retry++;
		if (taiko->mbhc_cfg.gpio && taiko_hs_gpio_level_remove(taiko)) {
			pr_debug("%s: GPIO indicates removal\n", __func__);
			break;
		}

		if (taiko->mbhc_cfg.gpio) {
			if (retry > 1)
				msleep(250);
			else
				msleep(50);
		}

		if (taiko->mbhc_cfg.gpio && taiko_hs_gpio_level_remove(taiko)) {
			pr_debug("%s: GPIO indicates removal\n", __func__);
			break;
		}

		for (i = 0; i < MBHC_NUM_DCE_PLUG_DETECT; i++) {
			mb_v[i] = taiko_codec_sta_dce(codec, 1,  true);
			mic_mv[i] = taiko_codec_sta_dce_v(codec, 1 , mb_v[i]);
			pr_debug("%s : DCE run %lu, mic_mv = %d(%x)\n",
				 __func__, retry, mic_mv[i], mb_v[i]);
		}

		if (taiko->mbhc_cfg.gpio && taiko_hs_gpio_level_remove(taiko)) {
			pr_debug("%s: GPIO indicates removal\n", __func__);
			break;
		}

		if (taiko->current_plug == PLUG_TYPE_NONE) {
			pr_debug("%s : headset/headphone is removed\n",
				 __func__);
			break;
		}

		for (i = 0; i < MBHC_NUM_DCE_PLUG_DETECT; i++)
			if (!is_valid_mic_voltage(codec, mic_mv[i]))
				break;

		if (i == MBHC_NUM_DCE_PLUG_DETECT) {
			pr_debug("%s: MIC voltage settled\n", __func__);
			settled = true;
			msleep(200);
			break;
		}

		/* only for non-GPIO remove irq */
		if (!taiko->mbhc_cfg.gpio) {
			for (i = 0; i < MBHC_NUM_DCE_PLUG_DETECT; i++)
				if (mic_mv[i] < v_hs_max)
					break;
			if (i == MBHC_NUM_DCE_PLUG_DETECT) {
				pr_debug("%s: Headset is removed\n", __func__);
				break;
			}
		}
	}

	if (timedout)
		pr_debug("%s: Microphone did not settle in %d seconds\n",
			 __func__, TAIKO_HS_DETECT_PLUG_TIME_MS);
	return settled;
}

/* called only from interrupt which is under codec_resource_lock acquisition */
static void taiko_hs_remove_irq_gpio(struct taiko_priv *priv)
{
	struct snd_soc_codec *codec = priv->codec;

	if (taiko_hs_remove_settle(codec))
		taiko_codec_start_hs_polling(codec);
	pr_debug("%s: remove settle done\n", __func__);
}

/* called only from interrupt which is under codec_resource_lock acquisition */
static void taiko_hs_remove_irq_nogpio(struct taiko_priv *priv)
{
	short bias_value;
	bool removed = true;
	struct snd_soc_codec *codec = priv->codec;
	const struct taiko_mbhc_general_cfg *generic =
	    TAIKO_MBHC_CAL_GENERAL_PTR(priv->mbhc_cfg.calibration);
	int min_us = TAIKO_FAKE_REMOVAL_MIN_PERIOD_MS * 1000;

	if (priv->current_plug != PLUG_TYPE_HEADSET) {
		pr_debug("%s(): Headset is not inserted, ignore removal\n",
			 __func__);
		snd_soc_update_bits(codec, TAIKO_A_CDC_MBHC_CLK_CTL,
				    0x08, 0x08);
		return;
	}

	usleep_range(generic->t_shutdown_plug_rem,
		     generic->t_shutdown_plug_rem);

	do {
		bias_value = taiko_codec_sta_dce(codec, 1,  true);
		pr_debug("%s: DCE %d,%d, %d us left\n", __func__, bias_value,
			 taiko_codec_sta_dce_v(codec, 1, bias_value), min_us);
		if (bias_value < taiko_get_current_v_ins(priv, false)) {
			pr_debug("%s: checking false removal\n", __func__);
			msleep(500);
			removed = !taiko_hs_remove_settle(codec);
			pr_debug("%s: headset %sactually removed\n", __func__,
				 removed ? "" : "not ");
			break;
		}
		min_us -= priv->mbhc_data.t_dce;
	} while (min_us > 0);

	if (removed) {
		/* cancel possiblely running hs detect work */
		taiko_cancel_hs_detect_plug(priv);
		/*
		 * If this removal is not false, first check the micbias
		 * switch status and switch it to LDOH if it is already
		 * switched to VDDIO.
		 */
		taiko_codec_switch_micbias(codec, 0);

		taiko_codec_report_plug(codec, 0, SND_JACK_HEADSET);
		taiko_codec_cleanup_hs_polling(codec);
		taiko_codec_enable_hs_detect(codec, 1,
					     MBHC_USE_MB_TRIGGER |
					     MBHC_USE_HPHL_TRIGGER,
					     true);
	} else {
		taiko_codec_start_hs_polling(codec);
	}
}

static irqreturn_t taiko_hs_remove_irq(int irq, void *data)
{
	struct taiko_priv *priv = data;
	bool vddio;
	pr_debug("%s: enter, removal interrupt\n", __func__);

	TAIKO_ACQUIRE_LOCK(priv->codec_resource_lock);
	vddio = (priv->mbhc_data.micb_mv != VDDIO_MICBIAS_MV &&
		 priv->mbhc_micbias_switched);
	if (vddio)
		__taiko_codec_switch_micbias(priv->codec, 0, false, true);

	if (priv->mbhc_cfg.gpio)
		taiko_hs_remove_irq_gpio(priv);
	else
		taiko_hs_remove_irq_nogpio(priv);

	/* if driver turned off vddio switch and headset is not removed,
	 * turn on the vddio switch back, if headset is removed then vddio
	 * switch is off by time now and shouldn't be turn on again from here */
	if (vddio && priv->current_plug == PLUG_TYPE_HEADSET)
		__taiko_codec_switch_micbias(priv->codec, 1, true, true);
	TAIKO_RELEASE_LOCK(priv->codec_resource_lock);

	return IRQ_HANDLED;
}

static void taiko_mbhc_insert_work(struct work_struct *work)
{
	struct delayed_work *dwork;
	struct taiko_priv *taiko;
	struct snd_soc_codec *codec;
	struct wcd9xxx *taiko_core;

	dwork = to_delayed_work(work);
	taiko = container_of(dwork, struct taiko_priv, mbhc_insert_dwork);
	codec = taiko->codec;
	taiko_core = dev_get_drvdata(codec->dev->parent);

	pr_debug("%s:\n", __func__);

	/* Turn off both HPH and MIC line schmitt triggers */
	snd_soc_update_bits(codec, taiko->mbhc_bias_regs.mbhc_reg, 0x90, 0x00);
	snd_soc_update_bits(codec, TAIKO_A_MBHC_HPH, 0x13, 0x00);
	snd_soc_update_bits(codec, taiko->mbhc_bias_regs.ctl_reg, 0x01, 0x00);
	wcd9xxx_disable_irq_sync(codec->control_data, TAIKO_IRQ_MBHC_INSERTION);
	taiko_codec_detect_plug_type(codec);
	wcd9xxx_unlock_sleep(taiko_core);
}

static void taiko_hs_gpio_handler(struct snd_soc_codec *codec)
{
	bool insert;
	struct taiko_priv *taiko = snd_soc_codec_get_drvdata(codec);
	bool is_removed = false;

	pr_debug("%s: enter\n", __func__);

	taiko->in_gpio_handler = true;
	/* Wait here for debounce time */
	usleep_range(TAIKO_GPIO_IRQ_DEBOUNCE_TIME_US,
		     TAIKO_GPIO_IRQ_DEBOUNCE_TIME_US);

	TAIKO_ACQUIRE_LOCK(taiko->codec_resource_lock);

	/* cancel pending button press */
	if (taiko_cancel_btn_work(taiko))
		pr_debug("%s: button press is canceled\n", __func__);

	insert = (gpio_get_value_cansleep(taiko->mbhc_cfg.gpio) ==
		  taiko->mbhc_cfg.gpio_level_insert);
	if ((taiko->current_plug == PLUG_TYPE_NONE) && insert) {
		taiko->lpi_enabled = false;
		wmb();

		/* cancel detect plug */
		taiko_cancel_hs_detect_plug(taiko);

		/* Disable Mic Bias pull down and HPH Switch to GND */
		snd_soc_update_bits(codec, taiko->mbhc_bias_regs.ctl_reg, 0x01,
				    0x00);
		snd_soc_update_bits(codec, TAIKO_A_MBHC_HPH, 0x01, 0x00);
		taiko_codec_detect_plug_type(codec);
	} else if ((taiko->current_plug != PLUG_TYPE_NONE) && !insert) {
		taiko->lpi_enabled = false;
		wmb();

		/* cancel detect plug */
		taiko_cancel_hs_detect_plug(taiko);

		if (taiko->current_plug == PLUG_TYPE_HEADPHONE) {
			taiko_codec_report_plug(codec, 0, SND_JACK_HEADPHONE);
			is_removed = true;
		} else if (taiko->current_plug == PLUG_TYPE_GND_MIC_SWAP) {
			taiko_codec_report_plug(codec, 0, SND_JACK_UNSUPPORTED);
			is_removed = true;
		} else if (taiko->current_plug == PLUG_TYPE_HEADSET) {
			taiko_codec_pause_hs_polling(codec);
			taiko_codec_cleanup_hs_polling(codec);
			taiko_codec_report_plug(codec, 0, SND_JACK_HEADSET);
			is_removed = true;
		}

		if (is_removed) {
			/* Enable Mic Bias pull down and HPH Switch to GND */
			snd_soc_update_bits(codec,
					    taiko->mbhc_bias_regs.ctl_reg, 0x01,
					    0x01);
			snd_soc_update_bits(codec, TAIKO_A_MBHC_HPH, 0x01,
					    0x01);
			/* Make sure mic trigger is turned off */
			snd_soc_update_bits(codec,
					    taiko->mbhc_bias_regs.ctl_reg,
					    0x01, 0x01);
			snd_soc_update_bits(codec,
					    taiko->mbhc_bias_regs.mbhc_reg,
					    0x90, 0x00);
			/* Reset MBHC State Machine */
			snd_soc_update_bits(codec, TAIKO_A_CDC_MBHC_CLK_CTL,
					    0x08, 0x08);
			snd_soc_update_bits(codec, TAIKO_A_CDC_MBHC_CLK_CTL,
					    0x08, 0x00);
			/* Turn off override */
			taiko_turn_onoff_override(codec, false);
		}
	}

	taiko->in_gpio_handler = false;
	TAIKO_RELEASE_LOCK(taiko->codec_resource_lock);
	pr_debug("%s: leave\n", __func__);
}

static irqreturn_t taiko_mechanical_plug_detect_irq(int irq, void *data)
{
	int r = IRQ_HANDLED;
	struct snd_soc_codec *codec = data;

	if (unlikely(wcd9xxx_lock_sleep(codec->control_data) == false)) {
		pr_warn("%s: failed to hold suspend\n", __func__);
		r = IRQ_NONE;
	} else {
		taiko_hs_gpio_handler(codec);
		wcd9xxx_unlock_sleep(codec->control_data);
	}

	return r;
}

static int taiko_mbhc_init_and_calibrate(struct taiko_priv *taiko)
{
	int ret = 0;
	struct snd_soc_codec *codec = taiko->codec;

	taiko->mbhc_cfg.mclk_cb_fn(codec, 1, false);
	taiko_mbhc_init(codec);
	taiko_mbhc_cal(codec);
	taiko_mbhc_calc_thres(codec);
	taiko->mbhc_cfg.mclk_cb_fn(codec, 0, false);
	taiko_codec_calibrate_hs_polling(codec);
	if (!taiko->mbhc_cfg.gpio) {
		ret = taiko_codec_enable_hs_detect(codec, 1,
						   MBHC_USE_MB_TRIGGER |
						   MBHC_USE_HPHL_TRIGGER,
						   false);

		if (IS_ERR_VALUE(ret))
			pr_err("%s: Failed to setup MBHC detection\n",
			       __func__);
	} else {
		/* Enable Mic Bias pull down and HPH Switch to GND */
		snd_soc_update_bits(codec, taiko->mbhc_bias_regs.ctl_reg,
				    0x01, 0x01);
		snd_soc_update_bits(codec, TAIKO_A_MBHC_HPH, 0x01, 0x01);
		INIT_WORK(&taiko->hs_correct_plug_work,
			  taiko_hs_correct_gpio_plug);
	}

	if (!IS_ERR_VALUE(ret)) {
		snd_soc_update_bits(codec, TAIKO_A_RX_HPH_OCP_CTL, 0x10, 0x10);
		wcd9xxx_enable_irq(codec->control_data,
				 TAIKO_IRQ_HPH_PA_OCPL_FAULT);
		wcd9xxx_enable_irq(codec->control_data,
				 TAIKO_IRQ_HPH_PA_OCPR_FAULT);

		if (taiko->mbhc_cfg.gpio) {
			ret = request_threaded_irq(taiko->mbhc_cfg.gpio_irq,
					       NULL,
					       taiko_mechanical_plug_detect_irq,
					       (IRQF_TRIGGER_RISING |
						IRQF_TRIGGER_FALLING),
					       "taiko-gpio", codec);
			if (!IS_ERR_VALUE(ret)) {
				ret = enable_irq_wake(taiko->mbhc_cfg.gpio_irq);
				/* Bootup time detection */
				taiko_hs_gpio_handler(codec);
			}
		}
	}

	return ret;
}

static void mbhc_fw_read(struct work_struct *work)
{
	struct delayed_work *dwork;
	struct taiko_priv *taiko;
	struct snd_soc_codec *codec;
	const struct firmware *fw;
	int ret = -1, retry = 0;

	dwork = to_delayed_work(work);
	taiko = container_of(dwork, struct taiko_priv, mbhc_firmware_dwork);
	codec = taiko->codec;

	while (retry < MBHC_FW_READ_ATTEMPTS) {
		retry++;
		pr_info("%s:Attempt %d to request MBHC firmware\n",
			__func__, retry);
		ret = request_firmware(&fw, "wcd9320/wcd9320_mbhc.bin",
					codec->dev);

		if (ret != 0) {
			usleep_range(MBHC_FW_READ_TIMEOUT,
				     MBHC_FW_READ_TIMEOUT);
		} else {
			pr_info("%s: MBHC Firmware read succesful\n", __func__);
			break;
		}
	}

	if (ret != 0) {
		pr_err("%s: Cannot load MBHC firmware use default cal\n",
			__func__);
	} else if (taiko_mbhc_fw_validate(fw) == false) {
		pr_err("%s: Invalid MBHC cal data size use default cal\n",
			 __func__);
		release_firmware(fw);
	} else {
		taiko->mbhc_cfg.calibration = (void *)fw->data;
		taiko->mbhc_fw = fw;
	}

	(void) taiko_mbhc_init_and_calibrate(taiko);
}

int taiko_hs_detect(struct snd_soc_codec *codec,
		    const struct taiko_mbhc_config *cfg)
{
	struct taiko_priv *taiko;
	int rc = 0;

	if (!codec) {
		pr_err("%s: no codec\n", __func__);
		return -EINVAL;
	}

	if (!cfg->calibration) {
		pr_warn("%s: mbhc is not configured\n", __func__);
		return 0;
	}

	if (cfg->mclk_rate != TAIKO_MCLK_RATE_12288KHZ) {
		if (cfg->mclk_rate == TAIKO_MCLK_RATE_9600KHZ)
			pr_err("Error: clock rate %dHz is not yet supported\n",
			       cfg->mclk_rate);
		else
			pr_err("Error: unsupported clock rate %d\n",
			       cfg->mclk_rate);
		return -EINVAL;
	}

	taiko = snd_soc_codec_get_drvdata(codec);
	taiko->mbhc_cfg = *cfg;
	taiko->in_gpio_handler = false;
	taiko->current_plug = PLUG_TYPE_NONE;
	taiko->lpi_enabled = false;
	taiko_get_mbhc_micbias_regs(codec, &taiko->mbhc_bias_regs);

	/* Put CFILT in fast mode by default */
	snd_soc_update_bits(codec, taiko->mbhc_bias_regs.cfilt_ctl,
			    0x40, TAIKO_CFILT_FAST_MODE);
	INIT_DELAYED_WORK(&taiko->mbhc_firmware_dwork, mbhc_fw_read);
	INIT_DELAYED_WORK(&taiko->mbhc_btn_dwork, btn_lpress_fn);
	INIT_WORK(&taiko->hphlocp_work, hphlocp_off_report);
	INIT_WORK(&taiko->hphrocp_work, hphrocp_off_report);
	INIT_DELAYED_WORK(&taiko->mbhc_insert_dwork, taiko_mbhc_insert_work);

	if (!taiko->mbhc_cfg.read_fw_bin)
		rc = taiko_mbhc_init_and_calibrate(taiko);
	else
		schedule_delayed_work(&taiko->mbhc_firmware_dwork,
				      usecs_to_jiffies(MBHC_FW_READ_TIMEOUT));

	return rc;
}
EXPORT_SYMBOL_GPL(taiko_hs_detect);

static unsigned long slimbus_value;

static irqreturn_t taiko_slimbus_irq(int irq, void *data)
{
	struct taiko_priv *priv = data;
	struct snd_soc_codec *codec = priv->codec;
	int i, j;
	u8 val;

	for (i = 0; i < WCD9XXX_SLIM_NUM_PORT_REG; i++) {
		slimbus_value = wcd9xxx_interface_reg_read(codec->control_data,
			TAIKO_SLIM_PGD_PORT_INT_STATUS0 + i);
		for_each_set_bit(j, &slimbus_value, BITS_PER_BYTE) {
			val = wcd9xxx_interface_reg_read(codec->control_data,
				TAIKO_SLIM_PGD_PORT_INT_SOURCE0 + i*8 + j);
			if (val & 0x1)
				pr_err_ratelimited(
				"overflow error on port %x, value %x\n",
				i*8 + j, val);
			if (val & 0x2)
				pr_err_ratelimited(
				"underflow error on port %x, value %x\n",
				i*8 + j, val);
		}
		wcd9xxx_interface_reg_write(codec->control_data,
			TAIKO_SLIM_PGD_PORT_INT_CLR0 + i, 0xFF);
	}

	return IRQ_HANDLED;
}

static const struct taiko_reg_mask_val taiko_1_0_class_h_ear[] = {

	/* CLASS-H EAR  IDLE_THRESHOLD Table */
	TAIKO_REG_VAL(TAIKO_A_CDC_CLSH_IDLE_EAR_THSD, 0x26),
	TAIKO_REG_VAL(TAIKO_A_CDC_CLSH_FCLKONLY_EAR_THSD, 0x2C),

	/* CLASS-H EAR I_PA_FACT Table. */
	TAIKO_REG_VAL(TAIKO_A_CDC_CLSH_I_PA_FACT_EAR_L,	0xA9),
	TAIKO_REG_VAL(TAIKO_A_CDC_CLSH_I_PA_FACT_EAR_U, 0x07),

	/* CLASS-H EAR Voltage Headroom , Voltage Min. */
	TAIKO_REG_VAL(TAIKO_A_CDC_CLSH_V_PA_HD_EAR, 0x0D),
	TAIKO_REG_VAL(TAIKO_A_CDC_CLSH_V_PA_MIN_EAR, 0x3A),

	/* CLASS-H EAR K values --chnages from load. */
	TAIKO_REG_VAL(TAIKO_A_CDC_CLSH_K_ADDR, 0x08),
	TAIKO_REG_VAL(TAIKO_A_CDC_CLSH_K_DATA, 0x1B),
	TAIKO_REG_VAL(TAIKO_A_CDC_CLSH_K_DATA, 0x00),
	TAIKO_REG_VAL(TAIKO_A_CDC_CLSH_K_DATA, 0x2D),
	TAIKO_REG_VAL(TAIKO_A_CDC_CLSH_K_DATA, 0x00),
	TAIKO_REG_VAL(TAIKO_A_CDC_CLSH_K_DATA, 0x36),
	TAIKO_REG_VAL(TAIKO_A_CDC_CLSH_K_DATA, 0x00),
	TAIKO_REG_VAL(TAIKO_A_CDC_CLSH_K_DATA, 0x37),
	TAIKO_REG_VAL(TAIKO_A_CDC_CLSH_K_DATA, 0x00),
	/** end of Ear PA load 32 */
};


static const struct taiko_reg_mask_val taiko_1_0_class_h_hph[] = {

	/* CLASS-H HPH  IDLE_THRESHOLD Table */
	TAIKO_REG_VAL(TAIKO_A_CDC_CLSH_IDLE_HPH_THSD, 0x13),
	TAIKO_REG_VAL(TAIKO_A_CDC_CLSH_FCLKONLY_HPH_THSD, 0x19),

	/* CLASS-H HPH I_PA_FACT Table */
	TAIKO_REG_VAL(TAIKO_A_CDC_CLSH_I_PA_FACT_HPH_L,	0x9A),
	TAIKO_REG_VAL(TAIKO_A_CDC_CLSH_I_PA_FACT_HPH_U, 0x06),

	/* CLASS-H HPH Voltage Headroom , Voltage Min */
	TAIKO_REG_VAL(TAIKO_A_CDC_CLSH_V_PA_HD_HPH, 0x0D),
	TAIKO_REG_VAL(TAIKO_A_CDC_CLSH_V_PA_MIN_HPH, 0x1D),

	/* CLASS-H HPH K values --chnages from load .*/
	TAIKO_REG_VAL(TAIKO_A_CDC_CLSH_K_ADDR, 0x00),
	TAIKO_REG_VAL(TAIKO_A_CDC_CLSH_K_DATA, 0xAE),
	TAIKO_REG_VAL(TAIKO_A_CDC_CLSH_K_DATA, 0x01),
	TAIKO_REG_VAL(TAIKO_A_CDC_CLSH_K_DATA, 0x1C),
	TAIKO_REG_VAL(TAIKO_A_CDC_CLSH_K_DATA, 0x00),
	TAIKO_REG_VAL(TAIKO_A_CDC_CLSH_K_DATA, 0x25),
	TAIKO_REG_VAL(TAIKO_A_CDC_CLSH_K_DATA, 0x00),
	TAIKO_REG_VAL(TAIKO_A_CDC_CLSH_K_DATA, 0x27),
	TAIKO_REG_VAL(TAIKO_A_CDC_CLSH_K_DATA, 0x00),
};

static int taiko_config_ear_class_h(struct snd_soc_codec *codec, u32 ear_load)
{
	u32 i;

	if (ear_load  != 32)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(taiko_1_0_class_h_ear); i++)
		snd_soc_write(codec, taiko_1_0_class_h_ear[i].reg,
				taiko_1_0_class_h_ear[i].val);
	return 0;
}

static int taiko_config_hph_class_h(struct snd_soc_codec *codec, u32 hph_load)
{
	u32 i;
	if (hph_load  != 16)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(taiko_1_0_class_h_hph); i++)
		snd_soc_write(codec, taiko_1_0_class_h_hph[i].reg,
				taiko_1_0_class_h_hph[i].val);
	return 0;
}

static int taiko_handle_pdata(struct taiko_priv *taiko)
{
	struct snd_soc_codec *codec = taiko->codec;
	struct wcd9xxx_pdata *pdata = taiko->pdata;
	int k1, k2, k3, rc = 0;
	u8 leg_mode, txfe_bypass, txfe_buff, flag;
	u8 i = 0, j = 0;
	u8 val_txfe = 0, value = 0;

	if (!pdata) {
		pr_err("%s: NULL pdata\n", __func__);
		rc = -ENODEV;
		goto done;
	}

	leg_mode = pdata->amic_settings.legacy_mode;
	txfe_bypass = pdata->amic_settings.txfe_enable;
	txfe_buff = pdata->amic_settings.txfe_buff;
	flag = pdata->amic_settings.use_pdata;

	/* Make sure settings are correct */
	if ((pdata->micbias.ldoh_v > TAIKO_LDOH_2P85_V) ||
	    (pdata->micbias.bias1_cfilt_sel > TAIKO_CFILT3_SEL) ||
	    (pdata->micbias.bias2_cfilt_sel > TAIKO_CFILT3_SEL) ||
	    (pdata->micbias.bias3_cfilt_sel > TAIKO_CFILT3_SEL) ||
	    (pdata->micbias.bias4_cfilt_sel > TAIKO_CFILT3_SEL)) {
		rc = -EINVAL;
		goto done;
	}

	/* figure out k value */
	k1 = taiko_find_k_value(pdata->micbias.ldoh_v,
		pdata->micbias.cfilt1_mv);
	k2 = taiko_find_k_value(pdata->micbias.ldoh_v,
		pdata->micbias.cfilt2_mv);
	k3 = taiko_find_k_value(pdata->micbias.ldoh_v,
		pdata->micbias.cfilt3_mv);

	if (IS_ERR_VALUE(k1) || IS_ERR_VALUE(k2) || IS_ERR_VALUE(k3)) {
		rc = -EINVAL;
		goto done;
	}

	/* Set voltage level and always use LDO */
	snd_soc_update_bits(codec, TAIKO_A_LDO_H_MODE_1, 0x0C,
		(pdata->micbias.ldoh_v << 2));

	snd_soc_update_bits(codec, TAIKO_A_MICB_CFILT_1_VAL, 0xFC,
		(k1 << 2));
	snd_soc_update_bits(codec, TAIKO_A_MICB_CFILT_2_VAL, 0xFC,
		(k2 << 2));
	snd_soc_update_bits(codec, TAIKO_A_MICB_CFILT_3_VAL, 0xFC,
		(k3 << 2));

	snd_soc_update_bits(codec, TAIKO_A_MICB_1_CTL, 0x60,
		(pdata->micbias.bias1_cfilt_sel << 5));
	snd_soc_update_bits(codec, TAIKO_A_MICB_2_CTL, 0x60,
		(pdata->micbias.bias2_cfilt_sel << 5));
	snd_soc_update_bits(codec, TAIKO_A_MICB_3_CTL, 0x60,
		(pdata->micbias.bias3_cfilt_sel << 5));
	snd_soc_update_bits(codec, taiko->reg_addr.micb_4_ctl, 0x60,
			    (pdata->micbias.bias4_cfilt_sel << 5));

	for (i = 0; i < 6; j++, i += 2) {
		if (flag & (0x01 << i)) {
			value = (leg_mode & (0x01 << i)) ? 0x10 : 0x00;
			val_txfe = (txfe_bypass & (0x01 << i)) ? 0x20 : 0x00;
			val_txfe = val_txfe |
				((txfe_buff & (0x01 << i)) ? 0x10 : 0x00);
			snd_soc_update_bits(codec, TAIKO_A_TX_1_2_EN + j * 10,
				0x10, value);
			snd_soc_update_bits(codec,
				TAIKO_A_TX_1_2_TEST_EN + j * 10,
				0x30, val_txfe);
		}
		if (flag & (0x01 << (i + 1))) {
			value = (leg_mode & (0x01 << (i + 1))) ? 0x01 : 0x00;
			val_txfe = (txfe_bypass &
					(0x01 << (i + 1))) ? 0x02 : 0x00;
			val_txfe |= (txfe_buff &
					(0x01 << (i + 1))) ? 0x01 : 0x00;
			snd_soc_update_bits(codec, TAIKO_A_TX_1_2_EN + j * 10,
				0x01, value);
			snd_soc_update_bits(codec,
				TAIKO_A_TX_1_2_TEST_EN + j * 10,
				0x03, val_txfe);
		}
	}
	if (flag & 0x40) {
		value = (leg_mode & 0x40) ? 0x10 : 0x00;
		value = value | ((txfe_bypass & 0x40) ? 0x02 : 0x00);
		value = value | ((txfe_buff & 0x40) ? 0x01 : 0x00);
		snd_soc_update_bits(codec, TAIKO_A_TX_7_MBHC_EN,
			0x13, value);
	}

	if (pdata->ocp.use_pdata) {
		/* not defined in CODEC specification */
		if (pdata->ocp.hph_ocp_limit == 1 ||
			pdata->ocp.hph_ocp_limit == 5) {
			rc = -EINVAL;
			goto done;
		}
		snd_soc_update_bits(codec, TAIKO_A_RX_COM_OCP_CTL,
			0x0F, pdata->ocp.num_attempts);
		snd_soc_write(codec, TAIKO_A_RX_COM_OCP_COUNT,
			((pdata->ocp.run_time << 4) | pdata->ocp.wait_time));
		snd_soc_update_bits(codec, TAIKO_A_RX_HPH_OCP_CTL,
			0xE0, (pdata->ocp.hph_ocp_limit << 5));
	}

	for (i = 0; i < ARRAY_SIZE(pdata->regulator); i++) {
		if (!strncmp(pdata->regulator[i].name, "CDC_VDDA_RX", 11)) {
			if (pdata->regulator[i].min_uV == 1800000 &&
			    pdata->regulator[i].max_uV == 1800000) {
				snd_soc_write(codec, TAIKO_A_BIAS_REF_CTL,
					      0x1C);
			} else if (pdata->regulator[i].min_uV == 2200000 &&
				   pdata->regulator[i].max_uV == 2200000) {
				snd_soc_write(codec, TAIKO_A_BIAS_REF_CTL,
					      0x1E);
			} else {
				pr_err("%s: unsupported CDC_VDDA_RX voltage\n"
				       "min %d, max %d\n", __func__,
				       pdata->regulator[i].min_uV,
				       pdata->regulator[i].max_uV);
				rc = -EINVAL;
			}
			break;
		}
	}

	taiko_config_ear_class_h(codec, 32);
	taiko_config_hph_class_h(codec, 16);

done:
	return rc;
}

static const struct taiko_reg_mask_val taiko_1_0_reg_defaults[] = {

	/* set MCLk to 9.6 */
	TAIKO_REG_VAL(TAIKO_A_CHIP_CTL, 0x0A),
	TAIKO_REG_VAL(TAIKO_A_CDC_CLK_POWER_CTL, 0x03),

	/* EAR PA deafults  */
	TAIKO_REG_VAL(TAIKO_A_RX_EAR_CMBUFF, 0x05),
	/* HPH PA */
	TAIKO_REG_VAL(TAIKO_A_RX_HPH_BIAS_PA, 0x7A),

	/** BUCK and NCP defaults for EAR and HS */
	TAIKO_REG_VAL(TAIKO_A_BUCK_CTRL_CCL_4, 0x50),
	TAIKO_REG_VAL(TAIKO_A_BUCK_CTRL_VCL_1, 0x08),
	TAIKO_REG_VAL(TAIKO_A_BUCK_CTRL_CCL_1, 0x5B),
	TAIKO_REG_VAL(TAIKO_A_NCP_CLK, 0xFC),

	/* CLASS-H defaults for EAR and HS */
	TAIKO_REG_VAL(TAIKO_A_CDC_CLSH_BUCK_NCP_VARS, 0x00),
	TAIKO_REG_VAL(TAIKO_A_CDC_CLSH_BUCK_NCP_VARS, 0x04),
	TAIKO_REG_VAL(TAIKO_A_CDC_CLSH_B2_CTL, 0x01),
	TAIKO_REG_VAL(TAIKO_A_CDC_CLSH_B2_CTL, 0x05),
	TAIKO_REG_VAL(TAIKO_A_CDC_CLSH_B2_CTL, 0x35),
	TAIKO_REG_VAL(TAIKO_A_CDC_CLSH_B3_CTL, 0x30),
	TAIKO_REG_VAL(TAIKO_A_CDC_CLSH_B3_CTL, 0x3B),

	/*
	 * For CLASS-H, Enable ANC delay buffer,
	 * set HPHL and EAR PA ref gain to 0 DB.
	 */
	TAIKO_REG_VAL(TAIKO_A_CDC_CLSH_B1_CTL, 0x26),


	/* RX deafults */
	TAIKO_REG_VAL(TAIKO_A_CDC_RX1_B5_CTL, 0x78),
	TAIKO_REG_VAL(TAIKO_A_CDC_RX2_B5_CTL, 0x78),
	TAIKO_REG_VAL(TAIKO_A_CDC_RX3_B5_CTL, 0x78),
	TAIKO_REG_VAL(TAIKO_A_CDC_RX4_B5_CTL, 0x78),
	TAIKO_REG_VAL(TAIKO_A_CDC_RX5_B5_CTL, 0x78),
	TAIKO_REG_VAL(TAIKO_A_CDC_RX6_B5_CTL, 0x78),
	TAIKO_REG_VAL(TAIKO_A_CDC_RX7_B5_CTL, 0x78),

	/* RX1 and RX2 defaults */
	TAIKO_REG_VAL(TAIKO_A_CDC_RX1_B6_CTL, 0xA0),
	TAIKO_REG_VAL(TAIKO_A_CDC_RX2_B6_CTL, 0xA0),

	/* RX3 to RX7 defaults */
	TAIKO_REG_VAL(TAIKO_A_CDC_RX3_B6_CTL, 0x80),
	TAIKO_REG_VAL(TAIKO_A_CDC_RX4_B6_CTL, 0x80),
	TAIKO_REG_VAL(TAIKO_A_CDC_RX5_B6_CTL, 0x80),
	TAIKO_REG_VAL(TAIKO_A_CDC_RX6_B6_CTL, 0x80),
	TAIKO_REG_VAL(TAIKO_A_CDC_RX7_B6_CTL, 0x80),
};

static void taiko_update_reg_defaults(struct snd_soc_codec *codec)
{
	u32 i;

	for (i = 0; i < ARRAY_SIZE(taiko_1_0_reg_defaults); i++)
		snd_soc_write(codec, taiko_1_0_reg_defaults[i].reg,
				taiko_1_0_reg_defaults[i].val);
}

static const struct taiko_reg_mask_val taiko_codec_reg_init_val[] = {
	/* Initialize current threshold to 350MA
	 * number of wait and run cycles to 4096
	 */
	{TAIKO_A_RX_HPH_OCP_CTL, 0xE0, 0x60},
	{TAIKO_A_RX_COM_OCP_COUNT, 0xFF, 0xFF},

	/* Initialize gain registers to use register gain */
	{TAIKO_A_RX_HPH_L_GAIN, 0x20, 0x20},
	{TAIKO_A_RX_HPH_R_GAIN, 0x20, 0x20},
	{TAIKO_A_RX_LINE_1_GAIN, 0x20, 0x20},
	{TAIKO_A_RX_LINE_2_GAIN, 0x20, 0x20},
	{TAIKO_A_RX_LINE_3_GAIN, 0x20, 0x20},
	{TAIKO_A_RX_LINE_4_GAIN, 0x20, 0x20},

	/* CLASS H config */
	{TAIKO_A_CDC_CONN_CLSH_CTL, 0x3C, 0x14},

	/* Use 16 bit sample size for TX1 to TX6 */
	{TAIKO_A_CDC_CONN_TX_SB_B1_CTL, 0x30, 0x20},
	{TAIKO_A_CDC_CONN_TX_SB_B2_CTL, 0x30, 0x20},
	{TAIKO_A_CDC_CONN_TX_SB_B3_CTL, 0x30, 0x20},
	{TAIKO_A_CDC_CONN_TX_SB_B4_CTL, 0x30, 0x20},
	{TAIKO_A_CDC_CONN_TX_SB_B5_CTL, 0x30, 0x20},
	{TAIKO_A_CDC_CONN_TX_SB_B6_CTL, 0x30, 0x20},

	/* Use 16 bit sample size for TX7 to TX10 */
	{TAIKO_A_CDC_CONN_TX_SB_B7_CTL, 0x60, 0x40},
	{TAIKO_A_CDC_CONN_TX_SB_B8_CTL, 0x60, 0x40},
	{TAIKO_A_CDC_CONN_TX_SB_B9_CTL, 0x60, 0x40},
	{TAIKO_A_CDC_CONN_TX_SB_B10_CTL, 0x60, 0x40},

	/* Use 16 bit sample size for RX */
	{TAIKO_A_CDC_CONN_RX_SB_B1_CTL, 0xFF, 0xAA},
	{TAIKO_A_CDC_CONN_RX_SB_B2_CTL, 0xFF, 0xAA},

	/*enable HPF filter for TX paths */
	{TAIKO_A_CDC_TX1_MUX_CTL, 0x8, 0x0},
	{TAIKO_A_CDC_TX2_MUX_CTL, 0x8, 0x0},
	{TAIKO_A_CDC_TX3_MUX_CTL, 0x8, 0x0},
	{TAIKO_A_CDC_TX4_MUX_CTL, 0x8, 0x0},
	{TAIKO_A_CDC_TX5_MUX_CTL, 0x8, 0x0},
	{TAIKO_A_CDC_TX6_MUX_CTL, 0x8, 0x0},
	{TAIKO_A_CDC_TX7_MUX_CTL, 0x8, 0x0},
	{TAIKO_A_CDC_TX8_MUX_CTL, 0x8, 0x0},
	{TAIKO_A_CDC_TX9_MUX_CTL, 0x8, 0x0},
	{TAIKO_A_CDC_TX10_MUX_CTL, 0x8, 0x0},

	/* config Decimator for DMIC CLK_MODE_1(3.2Mhz@9.6Mhz mclk) */
	{TAIKO_A_CDC_TX1_DMIC_CTL, 0x7, 0x1},
	{TAIKO_A_CDC_TX2_DMIC_CTL, 0x7, 0x1},
	{TAIKO_A_CDC_TX3_DMIC_CTL, 0x7, 0x1},
	{TAIKO_A_CDC_TX4_DMIC_CTL, 0x7, 0x1},
	{TAIKO_A_CDC_TX5_DMIC_CTL, 0x7, 0x1},
	{TAIKO_A_CDC_TX6_DMIC_CTL, 0x7, 0x1},
	{TAIKO_A_CDC_TX7_DMIC_CTL, 0x7, 0x1},
	{TAIKO_A_CDC_TX8_DMIC_CTL, 0x7, 0x1},
	{TAIKO_A_CDC_TX9_DMIC_CTL, 0x7, 0x1},
	{TAIKO_A_CDC_TX10_DMIC_CTL, 0x7, 0x1},

	/* config DMIC clk to CLK_MODE_1 (3.2Mhz@9.6Mhz mclk) */
	{TAIKO_A_CDC_CLK_DMIC_B1_CTL, 0xEE, 0x22},
	{TAIKO_A_CDC_CLK_DMIC_B2_CTL, 0x0E, 0x02},

};

static void taiko_codec_init_reg(struct snd_soc_codec *codec)
{
	u32 i;

	for (i = 0; i < ARRAY_SIZE(taiko_codec_reg_init_val); i++)
		snd_soc_update_bits(codec, taiko_codec_reg_init_val[i].reg,
				taiko_codec_reg_init_val[i].mask,
				taiko_codec_reg_init_val[i].val);
}

static void taiko_update_reg_address(struct taiko_priv *priv)
{
	struct taiko_reg_address *reg_addr = &priv->reg_addr;
	reg_addr->micb_4_mbhc = TAIKO_A_MICB_4_MBHC;
	reg_addr->micb_4_int_rbias = TAIKO_A_MICB_4_INT_RBIAS;
	reg_addr->micb_4_ctl = TAIKO_A_MICB_4_CTL;

}

#ifdef CONFIG_DEBUG_FS
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
	struct taiko_priv *taiko = filp->private_data;

	if (cnt > sizeof(lbuf) - 1)
		return -EINVAL;

	rc = copy_from_user(lbuf, ubuf, cnt);
	if (rc)
		return -EFAULT;

	lbuf[cnt] = '\0';
	buf = (char *)lbuf;
	taiko->no_mic_headset_override = (*strsep(&buf, " ") == '0') ?
					     false : true;
	return rc;
}

static ssize_t codec_mbhc_debug_read(struct file *file, char __user *buf,
				     size_t count, loff_t *pos)
{
	const int size = 768;
	char buffer[size];
	int n = 0;
	struct taiko_priv *taiko = file->private_data;
	struct snd_soc_codec *codec = taiko->codec;
	const struct mbhc_internal_cal_data *p = &taiko->mbhc_data;
	const s16 v_ins_hu_cur = taiko_get_current_v_ins(taiko, true);
	const s16 v_ins_h_cur = taiko_get_current_v_ins(taiko, false);

	n = scnprintf(buffer, size - n, "dce_z = %x(%dmv)\n",  p->dce_z,
		     taiko_codec_sta_dce_v(codec, 1, p->dce_z));
	n += scnprintf(buffer + n, size - n, "dce_mb = %x(%dmv)\n",
		       p->dce_mb, taiko_codec_sta_dce_v(codec, 1, p->dce_mb));
	n += scnprintf(buffer + n, size - n, "sta_z = %x(%dmv)\n",
		       p->sta_z, taiko_codec_sta_dce_v(codec, 0, p->sta_z));
	n += scnprintf(buffer + n, size - n, "sta_mb = %x(%dmv)\n",
		       p->sta_mb, taiko_codec_sta_dce_v(codec, 0, p->sta_mb));
	n += scnprintf(buffer + n, size - n, "t_dce = %x\n",  p->t_dce);
	n += scnprintf(buffer + n, size - n, "t_sta = %x\n",  p->t_sta);
	n += scnprintf(buffer + n, size - n, "micb_mv = %dmv\n",
		       p->micb_mv);
	n += scnprintf(buffer + n, size - n, "v_ins_hu = %x(%dmv)%s\n",
		       p->v_ins_hu,
		       taiko_codec_sta_dce_v(codec, 0, p->v_ins_hu),
		       p->v_ins_hu == v_ins_hu_cur ? "*" : "");
	n += scnprintf(buffer + n, size - n, "v_ins_h = %x(%dmv)%s\n",
		       p->v_ins_h, taiko_codec_sta_dce_v(codec, 1, p->v_ins_h),
		       p->v_ins_h == v_ins_h_cur ? "*" : "");
	n += scnprintf(buffer + n, size - n, "adj_v_ins_hu = %x(%dmv)%s\n",
		       p->adj_v_ins_hu,
		       taiko_codec_sta_dce_v(codec, 0, p->adj_v_ins_hu),
		       p->adj_v_ins_hu == v_ins_hu_cur ? "*" : "");
	n += scnprintf(buffer + n, size - n, "adj_v_ins_h = %x(%dmv)%s\n",
		       p->adj_v_ins_h,
		       taiko_codec_sta_dce_v(codec, 1, p->adj_v_ins_h),
		       p->adj_v_ins_h == v_ins_h_cur ? "*" : "");
	n += scnprintf(buffer + n, size - n, "v_b1_hu = %x(%dmv)\n",
		       p->v_b1_hu, taiko_codec_sta_dce_v(codec, 0, p->v_b1_hu));
	n += scnprintf(buffer + n, size - n, "v_b1_h = %x(%dmv)\n",
		       p->v_b1_h, taiko_codec_sta_dce_v(codec, 1, p->v_b1_h));
	n += scnprintf(buffer + n, size - n, "v_b1_huc = %x(%dmv)\n",
		       p->v_b1_huc,
		       taiko_codec_sta_dce_v(codec, 1, p->v_b1_huc));
	n += scnprintf(buffer + n, size - n, "v_brh = %x(%dmv)\n",
		       p->v_brh, taiko_codec_sta_dce_v(codec, 1, p->v_brh));
	n += scnprintf(buffer + n, size - n, "v_brl = %x(%dmv)\n",  p->v_brl,
		       taiko_codec_sta_dce_v(codec, 0, p->v_brl));
	n += scnprintf(buffer + n, size - n, "v_no_mic = %x(%dmv)\n",
		       p->v_no_mic,
		       taiko_codec_sta_dce_v(codec, 0, p->v_no_mic));
	n += scnprintf(buffer + n, size - n, "npoll = %d\n",  p->npoll);
	n += scnprintf(buffer + n, size - n, "nbounce_wait = %d\n",
		       p->nbounce_wait);
	n += scnprintf(buffer + n, size - n, "v_inval_ins_low = %d\n",
		       p->v_inval_ins_low);
	n += scnprintf(buffer + n, size - n, "v_inval_ins_high = %d\n",
		       p->v_inval_ins_high);
	if (taiko->mbhc_cfg.gpio)
		n += scnprintf(buffer + n, size - n, "GPIO insert = %d\n",
			       taiko_hs_gpio_level_remove(taiko));
	buffer[n] = 0;

	return simple_read_from_buffer(buf, count, pos, buffer, n);
}

static const struct file_operations codec_debug_ops = {
	.open = codec_debug_open,
	.write = codec_debug_write,
};

static const struct file_operations codec_mbhc_debug_ops = {
	.open = codec_debug_open,
	.read = codec_mbhc_debug_read,
};
#endif

static int taiko_setup_irqs(struct taiko_priv *taiko)
{
	int ret;
	int i;
	struct snd_soc_codec *codec = taiko->codec;

	ret = wcd9xxx_request_irq(codec->control_data, TAIKO_IRQ_MBHC_INSERTION,
				  taiko_hs_insert_irq, "Headset insert detect",
				  taiko);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
			TAIKO_IRQ_MBHC_INSERTION);
		goto err_insert_irq;
	}
	wcd9xxx_disable_irq(codec->control_data, TAIKO_IRQ_MBHC_INSERTION);

	ret = wcd9xxx_request_irq(codec->control_data, TAIKO_IRQ_MBHC_REMOVAL,
				  taiko_hs_remove_irq, "Headset remove detect",
				  taiko);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
			TAIKO_IRQ_MBHC_REMOVAL);
		goto err_remove_irq;
	}

	ret = wcd9xxx_request_irq(codec->control_data, TAIKO_IRQ_MBHC_POTENTIAL,
				  taiko_dce_handler, "DC Estimation detect",
				  taiko);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
			TAIKO_IRQ_MBHC_POTENTIAL);
		goto err_potential_irq;
	}

	ret = wcd9xxx_request_irq(codec->control_data, TAIKO_IRQ_MBHC_RELEASE,
				 taiko_release_handler, "Button Release detect",
				 taiko);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
			TAIKO_IRQ_MBHC_RELEASE);
		goto err_release_irq;
	}

	ret = wcd9xxx_request_irq(codec->control_data, TAIKO_IRQ_SLIMBUS,
				  taiko_slimbus_irq, "SLIMBUS Slave", taiko);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
			TAIKO_IRQ_SLIMBUS);
		goto err_slimbus_irq;
	}

	for (i = 0; i < WCD9XXX_SLIM_NUM_PORT_REG; i++)
		wcd9xxx_interface_reg_write(codec->control_data,
					   TAIKO_SLIM_PGD_PORT_INT_EN0 + i,
					   0xFF);

	ret = wcd9xxx_request_irq(codec->control_data,
				  TAIKO_IRQ_HPH_PA_OCPL_FAULT,
				  taiko_hphl_ocp_irq,
				  "HPH_L OCP detect", taiko);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
			TAIKO_IRQ_HPH_PA_OCPL_FAULT);
		goto err_hphl_ocp_irq;
	}
	wcd9xxx_disable_irq(codec->control_data, TAIKO_IRQ_HPH_PA_OCPL_FAULT);

	ret = wcd9xxx_request_irq(codec->control_data,
				  TAIKO_IRQ_HPH_PA_OCPR_FAULT,
				  taiko_hphr_ocp_irq,
				  "HPH_R OCP detect", taiko);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
		       TAIKO_IRQ_HPH_PA_OCPR_FAULT);
		goto err_hphr_ocp_irq;
	}
	wcd9xxx_disable_irq(codec->control_data, TAIKO_IRQ_HPH_PA_OCPR_FAULT);

err_hphr_ocp_irq:
	wcd9xxx_free_irq(codec->control_data, TAIKO_IRQ_HPH_PA_OCPL_FAULT,
			 taiko);
err_hphl_ocp_irq:
	wcd9xxx_free_irq(codec->control_data, TAIKO_IRQ_SLIMBUS, taiko);
err_slimbus_irq:
	wcd9xxx_free_irq(codec->control_data, TAIKO_IRQ_MBHC_RELEASE, taiko);
err_release_irq:
	wcd9xxx_free_irq(codec->control_data, TAIKO_IRQ_MBHC_POTENTIAL, taiko);
err_potential_irq:
	wcd9xxx_free_irq(codec->control_data, TAIKO_IRQ_MBHC_REMOVAL, taiko);
err_remove_irq:
	wcd9xxx_free_irq(codec->control_data, TAIKO_IRQ_MBHC_INSERTION, taiko);
err_insert_irq:

	return ret;
}

static int taiko_codec_probe(struct snd_soc_codec *codec)
{
	struct wcd9xxx *control;
	struct taiko_priv *taiko;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	int ret = 0;
	int i;
	int ch_cnt;

	codec->control_data = dev_get_drvdata(codec->dev->parent);
	control = codec->control_data;

	dev_info(codec->dev, "%s()\n", __func__);

	taiko = kzalloc(sizeof(struct taiko_priv), GFP_KERNEL);
	if (!taiko) {
		dev_err(codec->dev, "Failed to allocate private data\n");
		return -ENOMEM;
	}
	for (i = 0 ; i < NUM_DECIMATORS; i++) {
		tx_hpf_work[i].taiko = taiko;
		tx_hpf_work[i].decimator = i + 1;
		INIT_DELAYED_WORK(&tx_hpf_work[i].dwork,
			tx_hpf_corner_freq_callback);
	}

	/* Make sure mbhc micbias register addresses are zeroed out */
	memset(&taiko->mbhc_bias_regs, 0,
		sizeof(struct mbhc_micbias_regs));
	taiko->mbhc_micbias_switched = false;

	/* Make sure mbhc intenal calibration data is zeroed out */
	memset(&taiko->mbhc_data, 0,
		sizeof(struct mbhc_internal_cal_data));
	taiko->mbhc_data.t_sta_dce = DEFAULT_DCE_STA_WAIT;
	taiko->mbhc_data.t_dce = DEFAULT_DCE_WAIT;
	taiko->mbhc_data.t_sta = DEFAULT_STA_WAIT;
	snd_soc_codec_set_drvdata(codec, taiko);

	taiko->mclk_enabled = false;
	taiko->bandgap_type = TAIKO_BANDGAP_OFF;
	taiko->clock_active = false;
	taiko->config_mode_active = false;
	taiko->mbhc_polling_active = false;
	taiko->mbhc_fake_ins_start = 0;
	taiko->no_mic_headset_override = false;
	taiko->hs_polling_irq_prepared = false;
	mutex_init(&taiko->codec_resource_lock);
	taiko->codec = codec;
	taiko->mbhc_state = MBHC_STATE_NONE;
	taiko->mbhc_last_resume = 0;
	for (i = 0; i < COMPANDER_MAX; i++) {
		taiko->comp_enabled[i] = 0;
		taiko->comp_fs[i] = COMPANDER_FS_48KHZ;
	}
	taiko->pdata = dev_get_platdata(codec->dev->parent);
	taiko->intf_type = wcd9xxx_get_intf_type();
	taiko->aux_pga_cnt = 0;
	taiko->aux_l_gain = 0x1F;
	taiko->aux_r_gain = 0x1F;
	taiko_update_reg_address(taiko);
	taiko_update_reg_defaults(codec);
	taiko_codec_init_reg(codec);
	ret = taiko_handle_pdata(taiko);
	if (IS_ERR_VALUE(ret)) {
		pr_err("%s: bad pdata\n", __func__);
		goto err_pdata;
	}

	if (taiko->intf_type == WCD9XXX_INTERFACE_TYPE_I2C) {
		snd_soc_dapm_new_controls(dapm, taiko_dapm_i2s_widgets,
			ARRAY_SIZE(taiko_dapm_i2s_widgets));
		snd_soc_dapm_add_routes(dapm, audio_i2s_map,
			ARRAY_SIZE(audio_i2s_map));
	}

	snd_soc_dapm_sync(dapm);

	(void) taiko_setup_irqs(taiko);

	for (i = 0; i < ARRAY_SIZE(taiko_dai); i++) {
		switch (taiko_dai[i].id) {
		case AIF1_PB:
			ch_cnt = taiko_dai[i].playback.channels_max;
			break;
		case AIF1_CAP:
			ch_cnt = taiko_dai[i].capture.channels_max;
			break;
		case AIF2_PB:
			ch_cnt = taiko_dai[i].playback.channels_max;
			break;
		case AIF2_CAP:
			ch_cnt = taiko_dai[i].capture.channels_max;
			break;
		case AIF3_PB:
			ch_cnt = taiko_dai[i].playback.channels_max;
			break;
		case AIF3_CAP:
			ch_cnt = taiko_dai[i].capture.channels_max;
			break;
		default:
			continue;
		}
		taiko->dai[i].ch_num = kzalloc((sizeof(unsigned int)*
					ch_cnt), GFP_KERNEL);
	}

#ifdef CONFIG_DEBUG_FS
	if (ret == 0) {
		taiko->debugfs_poke =
		    debugfs_create_file("TRRS", S_IFREG | S_IRUGO, NULL, taiko,
					&codec_debug_ops);
		taiko->debugfs_mbhc =
		    debugfs_create_file("taiko_mbhc", S_IFREG | S_IRUGO,
					NULL, taiko, &codec_mbhc_debug_ops);
	}
#endif
	codec->ignore_pmdown_time = 1;
	return ret;

err_pdata:
	mutex_destroy(&taiko->codec_resource_lock);
	kfree(taiko);
	return ret;
}
static int taiko_codec_remove(struct snd_soc_codec *codec)
{
	int i;
	struct taiko_priv *taiko = snd_soc_codec_get_drvdata(codec);
	wcd9xxx_free_irq(codec->control_data, TAIKO_IRQ_SLIMBUS, taiko);
	wcd9xxx_free_irq(codec->control_data, TAIKO_IRQ_MBHC_RELEASE, taiko);
	wcd9xxx_free_irq(codec->control_data, TAIKO_IRQ_MBHC_POTENTIAL, taiko);
	wcd9xxx_free_irq(codec->control_data, TAIKO_IRQ_MBHC_REMOVAL, taiko);
	wcd9xxx_free_irq(codec->control_data, TAIKO_IRQ_MBHC_INSERTION, taiko);
	TAIKO_ACQUIRE_LOCK(taiko->codec_resource_lock);
	taiko_codec_disable_clock_block(codec);
	TAIKO_RELEASE_LOCK(taiko->codec_resource_lock);
	taiko_codec_enable_bandgap(codec, TAIKO_BANDGAP_OFF);
	if (taiko->mbhc_fw)
		release_firmware(taiko->mbhc_fw);
	for (i = 0; i < ARRAY_SIZE(taiko_dai); i++)
		kfree(taiko->dai[i].ch_num);
	mutex_destroy(&taiko->codec_resource_lock);
#ifdef CONFIG_DEBUG_FS
	debugfs_remove(taiko->debugfs_poke);
	debugfs_remove(taiko->debugfs_mbhc);
#endif
	kfree(taiko);
	return 0;
}
static struct snd_soc_codec_driver soc_codec_dev_taiko = {
	.probe	= taiko_codec_probe,
	.remove	= taiko_codec_remove,

	.read = taiko_read,
	.write = taiko_write,

	.readable_register = taiko_readable,
	.volatile_register = taiko_volatile,

	.reg_cache_size = TAIKO_CACHE_SIZE,
	.reg_cache_default = taiko_reg_defaults,
	.reg_word_size = 1,

	.controls = taiko_snd_controls,
	.num_controls = ARRAY_SIZE(taiko_snd_controls),
	.dapm_widgets = taiko_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(taiko_dapm_widgets),
	.dapm_routes = audio_map,
	.num_dapm_routes = ARRAY_SIZE(audio_map),
};

#ifdef CONFIG_PM
static int taiko_suspend(struct device *dev)
{
	dev_dbg(dev, "%s: system suspend\n", __func__);
	return 0;
}

static int taiko_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct taiko_priv *taiko = platform_get_drvdata(pdev);
	dev_dbg(dev, "%s: system resume\n", __func__);
	taiko->mbhc_last_resume = jiffies;
	return 0;
}

static const struct dev_pm_ops taiko_pm_ops = {
	.suspend	= taiko_suspend,
	.resume		= taiko_resume,
};
#endif

static int __devinit taiko_probe(struct platform_device *pdev)
{
	int ret = 0;
	if (wcd9xxx_get_intf_type() == WCD9XXX_INTERFACE_TYPE_SLIMBUS)
		ret = snd_soc_register_codec(&pdev->dev, &soc_codec_dev_taiko,
			taiko_dai, ARRAY_SIZE(taiko_dai));
	else if (wcd9xxx_get_intf_type() == WCD9XXX_INTERFACE_TYPE_I2C)
		ret = snd_soc_register_codec(&pdev->dev, &soc_codec_dev_taiko,
			taiko_i2s_dai, ARRAY_SIZE(taiko_i2s_dai));
	return ret;
}
static int __devexit taiko_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}
static struct platform_driver taiko_codec_driver = {
	.probe = taiko_probe,
	.remove = taiko_remove,
	.driver = {
		.name = "taiko_codec",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &taiko_pm_ops,
#endif
	},
};

static int __init taiko_codec_init(void)
{
	return platform_driver_register(&taiko_codec_driver);
}

static void __exit taiko_codec_exit(void)
{
	platform_driver_unregister(&taiko_codec_driver);
}

module_init(taiko_codec_init);
module_exit(taiko_codec_exit);

MODULE_DESCRIPTION("Taiko codec driver");
MODULE_LICENSE("GPL v2");
