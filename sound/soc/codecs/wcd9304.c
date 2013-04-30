/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/wait.h>
#include "wcd9304.h"

#define WCD9304_RATES (SNDRV_PCM_RATE_8000|SNDRV_PCM_RATE_16000|\
			SNDRV_PCM_RATE_32000|SNDRV_PCM_RATE_48000)
#define ADC_DMIC_SEL_ADC	0
#define	ADC_DMIC_SEL_DMIC	1

#define NUM_AMIC 3
#define NUM_DECIMATORS 4
#define NUM_INTERPOLATORS 3
#define BITS_PER_REG 8
#define SITAR_RX_PORT_START_NUMBER 10

enum {
	AIF1_PB = 0,
	AIF1_CAP,
	NUM_CODEC_DAIS,
};

struct wcd9xxx_ch sitar_rx_chs[SITAR_RX_MAX] = {
	WCD9XXX_CH(SITAR_RX_PORT_START_NUMBER, 0),
	WCD9XXX_CH(SITAR_RX_PORT_START_NUMBER + 1, 1),
	WCD9XXX_CH(SITAR_RX_PORT_START_NUMBER + 2, 2),
	WCD9XXX_CH(SITAR_RX_PORT_START_NUMBER + 3, 3),
	WCD9XXX_CH(SITAR_RX_PORT_START_NUMBER + 4, 4)
};

struct wcd9xxx_ch sitar_tx_chs[SITAR_TX_MAX] = {
	WCD9XXX_CH(0, 0),
	WCD9XXX_CH(1, 1),
	WCD9XXX_CH(2, 2),
	WCD9XXX_CH(3, 3),
	WCD9XXX_CH(4, 4),
};

#define SITAR_CFILT_FAST_MODE 0x00
#define SITAR_CFILT_SLOW_MODE 0x40
#define MBHC_FW_READ_ATTEMPTS 15
#define MBHC_FW_READ_TIMEOUT 2000000

#define SLIM_CLOSE_TIMEOUT 1000

#define SITAR_JACK_MASK (SND_JACK_HEADSET | SND_JACK_OC_HPHL | SND_JACK_OC_HPHR)

#define SITAR_I2S_MASTER_MODE_MASK 0x08

#define SITAR_OCP_ATTEMPT 1

#define COMP_DIGITAL_DB_GAIN_APPLY(a, b) \
	(((a) <= 0) ? ((a) - b) : (a))
/* The wait time value comes from codec HW specification */
#define COMP_BRINGUP_WAIT_TIME  3000

#define SITAR_MCLK_RATE_12288KHZ 12288000
#define SITAR_MCLK_RATE_9600KHZ 9600000

#define SITAR_FAKE_INS_THRESHOLD_MS 2500
#define SITAR_FAKE_REMOVAL_MIN_PERIOD_MS 50
#define SITAR_MBHC_BUTTON_MIN 0x8000
#define SITAR_GPIO_IRQ_DEBOUNCE_TIME_US 5000

#define SITAR_ACQUIRE_LOCK(x) do { mutex_lock(&x); } while (0)
#define SITAR_RELEASE_LOCK(x) do { mutex_unlock(&x); } while (0)

#define MBHC_NUM_DCE_PLUG_DETECT 3
#define SITAR_MBHC_FAKE_INSERT_LOW 10
#define SITAR_MBHC_FAKE_INSERT_HIGH 80
#define SITAR_MBHC_FAKE_INSERT_VOLT_DELTA_MV 500
#define SITAR_HS_DETECT_PLUG_TIME_MS (5 * 1000)
#define SITAR_HS_DETECT_PLUG_INERVAL_MS 100
#define NUM_ATTEMPTS_TO_REPORT 5
#define SITAR_MBHC_STATUS_REL_DETECTION 0x0C
#define SITAR_MBHC_GPIO_REL_DEBOUNCE_TIME_MS 200

#define CUT_OF_FREQ_MASK 0x30
#define CF_MIN_3DB_4HZ 0x0
#define CF_MIN_3DB_75HZ 0x01
#define CF_MIN_3DB_150HZ 0x02


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
enum sitar_priv_ack_flags {
	SITAR_HPHL_PA_OFF_ACK = 0,
	SITAR_HPHR_PA_OFF_ACK,
	SITAR_HPHL_DAC_OFF_ACK,
	SITAR_HPHR_DAC_OFF_ACK
};

struct comp_sample_dependent_params {
	u32 peak_det_timeout;
	u32 rms_meter_div_fact;
	u32 rms_meter_resamp_fact;
};

struct comp_dgtl_gain_offset {
	u8 whole_db_gain;
	u8 half_db_gain;
};

static const struct comp_dgtl_gain_offset comp_dgtl_gain[] = {
	{0, 0},
	{1, 1},
	{3, 0},
	{4, 1},
	{6, 0},
	{7, 1},
	{9, 0},
	{10, 1},
	{12, 0},
	{13, 1},
	{15, 0},
	{16, 1},
	{18, 0},
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
};

enum sitar_mbhc_plug_type {
	PLUG_TYPE_INVALID = -1,
	PLUG_TYPE_NONE,
	PLUG_TYPE_HEADSET,
	PLUG_TYPE_HEADPHONE,
	PLUG_TYPE_HIGH_HPH,
};

enum sitar_mbhc_state {
	MBHC_STATE_NONE = -1,
	MBHC_STATE_POTENTIAL,
	MBHC_STATE_POTENTIAL_RECOVERY,
	MBHC_STATE_RELEASE,
};

static const u32 vport_check_table[NUM_CODEC_DAIS] = {
	0,					/* AIF1_PB */
	0,					/* AIF1_CAP */
};

struct hpf_work {
	struct sitar_priv *sitar;
	u32 decimator;
	u8 tx_hpf_cut_of_freq;
	struct delayed_work dwork;
};

static struct hpf_work tx_hpf_work[NUM_DECIMATORS];

struct sitar_priv {
	struct snd_soc_codec *codec;
	u32 mclk_freq;
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
	unsigned long mbhc_fake_ins_start;
	int buttons_pressed;

	enum sitar_micbias_num micbias;
	/* void* calibration contains:
	 *  struct sitar_mbhc_general_cfg generic;
	 *  struct sitar_mbhc_plug_detect_cfg plug_det;
	 *  struct sitar_mbhc_plug_type_cfg plug_type;
	 *  struct sitar_mbhc_btn_detect_cfg btn_det;
	 *  struct sitar_mbhc_imped_detect_cfg imped_det;
	 * Note: various size depends on btn_det->num_btn
	 */
	void *calibration;
	struct mbhc_internal_cal_data mbhc_data;

	struct wcd9xxx_pdata *pdata;
	u32 anc_slot;

	bool no_mic_headset_override;

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

	u8 hphlocp_cnt; /* headphone left ocp retry */
	u8 hphrocp_cnt; /* headphone right ocp retry */

	/* Callback function to enable MCLK */
	int (*mclk_cb) (struct snd_soc_codec*, int);

	/* Work to perform MBHC Firmware Read */
	struct delayed_work mbhc_firmware_dwork;
	const struct firmware *mbhc_fw;

	/* num of slim ports required */
	struct wcd9xxx_codec_dai_data dai[NUM_CODEC_DAIS];

	/*compander*/
	int comp_enabled[COMPANDER_MAX];
	u32 comp_fs[COMPANDER_MAX];
	u8  comp_gain_offset[NUM_INTERPOLATORS];

	/* Currently, only used for mbhc purpose, to protect
	 * concurrent execution of mbhc threaded irq handlers and
	 * kill race between DAPM and MBHC.But can serve as a
	 * general lock to protect codec resource
	 */
	struct mutex codec_resource_lock;

	struct sitar_mbhc_config mbhc_cfg;
	bool in_gpio_handler;
	u8 current_plug;
	bool lpi_enabled;
	enum sitar_mbhc_state mbhc_state;
	struct work_struct hs_correct_plug_work;
	bool hs_detect_work_stop;
	struct delayed_work mbhc_btn_dwork;
	unsigned long mbhc_last_resume; /* in jiffies */
};

#ifdef CONFIG_DEBUG_FS
struct sitar_priv *debug_sitar_priv;
#endif

static const int comp_rx_path[] = {
	COMPANDER_2,
	COMPANDER_1,
	COMPANDER_1,
	COMPANDER_MAX,
};

static const struct comp_sample_dependent_params
		    comp_samp_params[COMPANDER_FS_MAX] = {
	{
		.peak_det_timeout = 0x6,
		.rms_meter_div_fact = 0x9 << 4,
		.rms_meter_resamp_fact = 0x06,
	},
	{
		.peak_det_timeout = 0x7,
		.rms_meter_div_fact = 0xA << 4,
		.rms_meter_resamp_fact = 0x0C,
	},
	{
		.peak_det_timeout = 0x8,
		.rms_meter_div_fact = 0xB << 4,
		.rms_meter_resamp_fact = 0x30,
	},
	{
		.peak_det_timeout = 0x9,
		.rms_meter_div_fact = 0xB << 4,
		.rms_meter_resamp_fact = 0x28,
	},
	{
		.peak_det_timeout = 0xA,
		.rms_meter_div_fact = 0xC << 4,
		.rms_meter_resamp_fact = 0x50,
	},
	{
		.peak_det_timeout = 0xB,
		.rms_meter_div_fact = 0xC << 4,
		.rms_meter_resamp_fact = 0x50,
	},
};

static int sitar_get_anc_slot(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);
	ucontrol->value.integer.value[0] = sitar->anc_slot;
	return 0;
}

static int sitar_put_anc_slot(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);
	sitar->anc_slot = ucontrol->value.integer.value[0];
	return 0;
}

static int sitar_pa_gain_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	u8 ear_pa_gain;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	ear_pa_gain = snd_soc_read(codec, SITAR_A_RX_EAR_GAIN);

	ear_pa_gain &= 0xE0;

	if (ear_pa_gain == 0x00) {
		ucontrol->value.integer.value[0] = 0;
	} else if (ear_pa_gain == 0x80) {
		ucontrol->value.integer.value[0] = 1;
	} else if (ear_pa_gain == 0xA0) {
		ucontrol->value.integer.value[0] = 2;
	} else if (ear_pa_gain == 0xE0) {
		ucontrol->value.integer.value[0] = 3;
	} else  {
		pr_err("%s: ERROR: Unsupported Ear Gain = 0x%x\n",
				__func__, ear_pa_gain);
		return -EINVAL;
	}

	pr_debug("%s: ear_pa_gain = 0x%x\n", __func__, ear_pa_gain);

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
	case 2:
		ear_pa_gain = 0xA0;
		break;
	case 3:
		ear_pa_gain = 0xE0;
		break;
	default:
		return -EINVAL;
	}

	snd_soc_update_bits(codec, SITAR_A_RX_EAR_GAIN, 0xE0, ear_pa_gain);
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

	pr_debug("%s: IIR #%d band #%d enable %d\n", __func__,
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
		(SITAR_A_CDC_IIR1_COEF_B1_CTL + 16 * iir_idx),
		(band_idx * BAND_MAX + coeff_idx) & 0x1F);

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
		(SITAR_A_CDC_IIR1_COEF_B1_CTL + 16 * iir_idx),
		(band_idx * BAND_MAX + coeff_idx) & 0x1F);

	/* Mask top 2 bits, 7-8 are reserved */
	snd_soc_write(codec,
		(SITAR_A_CDC_IIR1_COEF_B2_CTL + 16 * iir_idx),
		(value >> 24) & 0x3F);

	/* Isolate 8bits at a time */
	snd_soc_write(codec,
		(SITAR_A_CDC_IIR1_COEF_B3_CTL + 16 * iir_idx),
		(value >> 16) & 0xFF);

	snd_soc_write(codec,
		(SITAR_A_CDC_IIR1_COEF_B4_CTL + 16 * iir_idx),
		(value >> 8) & 0xFF);

	snd_soc_write(codec,
		(SITAR_A_CDC_IIR1_COEF_B5_CTL + 16 * iir_idx),
		value & 0xFF);
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

static int sitar_compander_gain_offset(
	struct snd_soc_codec *codec, u32 enable,
	unsigned int pa_reg, unsigned int vol_reg,
	int mask, int event,
	struct comp_dgtl_gain_offset *gain_offset,
	int index)
{
	unsigned int pa_gain = snd_soc_read(codec, pa_reg);
	unsigned int digital_vol = snd_soc_read(codec, vol_reg);
	int pa_mode = pa_gain & mask;
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s: pa_gain(0x%x=0x%x)digital_vol(0x%x=0x%x)event(0x%x) index(%d)\n",
		 __func__, pa_reg, pa_gain, vol_reg, digital_vol, event, index);
	if (((pa_gain & 0xF) + 1) > ARRAY_SIZE(comp_dgtl_gain) ||
		(index >= ARRAY_SIZE(sitar->comp_gain_offset))) {
		pr_err("%s: Out of array boundary\n", __func__);
		return -EINVAL;
	}

	if (SND_SOC_DAPM_EVENT_ON(event) && (enable != 0)) {
		gain_offset->whole_db_gain = COMP_DIGITAL_DB_GAIN_APPLY(
		  (digital_vol - comp_dgtl_gain[pa_gain & 0xF].whole_db_gain),
		  comp_dgtl_gain[pa_gain & 0xF].half_db_gain);
		pr_debug("%s: listed whole_db_gain:0x%x, adjusted whole_db_gain:0x%x\n",
			 __func__, comp_dgtl_gain[pa_gain & 0xF].whole_db_gain,
			 gain_offset->whole_db_gain);
		gain_offset->half_db_gain =
				comp_dgtl_gain[pa_gain & 0xF].half_db_gain;
		sitar->comp_gain_offset[index] = digital_vol -
						 gain_offset->whole_db_gain ;
	}
	if (SND_SOC_DAPM_EVENT_OFF(event) && (pa_mode == 0)) {
		gain_offset->whole_db_gain = digital_vol +
					     sitar->comp_gain_offset[index];
		pr_debug("%s: listed whole_db_gain:0x%x, adjusted whole_db_gain:0x%x\n",
			 __func__, comp_dgtl_gain[pa_gain & 0xF].whole_db_gain,
			 gain_offset->whole_db_gain);
		gain_offset->half_db_gain = 0;
	}

	pr_debug("%s: half_db_gain(%d)whole_db_gain(0x%x)comp_gain_offset[%d](%d)\n",
		 __func__, gain_offset->half_db_gain,
		 gain_offset->whole_db_gain, index,
		 sitar->comp_gain_offset[index]);
	return 0;
}

static int sitar_config_gain_compander(
				struct snd_soc_codec *codec,
				u32 compander, u32 enable, int event)
{
	int value = 0;
	int mask = 1 << 4;
	struct comp_dgtl_gain_offset gain_offset = {0, 0};
	if (compander >= COMPANDER_MAX) {
		pr_err("%s: Error, invalid compander channel\n", __func__);
		return -EINVAL;
	}

	if ((enable == 0) || SND_SOC_DAPM_EVENT_OFF(event))
		value = 1 << 4;

	if (compander == COMPANDER_1) {
		sitar_compander_gain_offset(codec, enable,
				SITAR_A_RX_HPH_L_GAIN,
				SITAR_A_CDC_RX2_VOL_CTL_B2_CTL,
				mask, event, &gain_offset, 1);
		snd_soc_update_bits(codec, SITAR_A_RX_HPH_L_GAIN, mask, value);
		snd_soc_update_bits(codec, SITAR_A_CDC_RX2_VOL_CTL_B2_CTL,
				    0xFF, gain_offset.whole_db_gain);
		snd_soc_update_bits(codec, SITAR_A_CDC_RX2_B6_CTL,
				    0x02, gain_offset.half_db_gain);
		sitar_compander_gain_offset(codec, enable,
				SITAR_A_RX_HPH_R_GAIN,
				SITAR_A_CDC_RX3_VOL_CTL_B2_CTL,
				mask, event, &gain_offset, 2);
		snd_soc_update_bits(codec, SITAR_A_RX_HPH_R_GAIN, mask, value);
		snd_soc_update_bits(codec, SITAR_A_CDC_RX3_VOL_CTL_B2_CTL,
				    0xFF, gain_offset.whole_db_gain);
		snd_soc_update_bits(codec, SITAR_A_CDC_RX3_B6_CTL,
				    0x02, gain_offset.half_db_gain);
	} else if (compander == COMPANDER_2) {
		sitar_compander_gain_offset(codec, enable,
				SITAR_A_RX_LINE_1_GAIN,
				SITAR_A_CDC_RX1_VOL_CTL_B2_CTL,
				mask, event, &gain_offset, 0);
		snd_soc_update_bits(codec, SITAR_A_RX_LINE_1_GAIN, mask, value);
		snd_soc_update_bits(codec, SITAR_A_RX_LINE_2_GAIN, mask, value);
		snd_soc_update_bits(codec, SITAR_A_CDC_RX1_VOL_CTL_B2_CTL,
				    0xFF, gain_offset.whole_db_gain);
		snd_soc_update_bits(codec, SITAR_A_CDC_RX1_B6_CTL,
				    0x02, gain_offset.half_db_gain);
	}
	return 0;
}

static int sitar_get_compander(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	int comp = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = sitar->comp_enabled[comp];

	return 0;
}

static int sitar_set_compander(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);
	int comp = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;
	int value = ucontrol->value.integer.value[0];

	pr_debug("%s: compander #%d enable %d\n",
		 __func__, comp + 1, value);
	if (value == sitar->comp_enabled[comp]) {
		pr_debug("%s: compander #%d enable %d no change\n",
			 __func__, comp + 1, value);
		return 0;
	}
	sitar->comp_enabled[comp] = value;
	return 0;
}

static int sitar_config_compander(struct snd_soc_dapm_widget *w,
				  struct snd_kcontrol *kcontrol,
				  int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);
	u32 rate = sitar->comp_fs[w->shift];
	u32 value;

	pr_debug("%s: compander #%d enable %d event %d widget name %s\n",
		 __func__, w->shift + 1,
		 sitar->comp_enabled[w->shift], event , w->name);
	if (sitar->comp_enabled[w->shift] == 0)
		goto rtn;
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Update compander sample rate */
		snd_soc_update_bits(codec, SITAR_A_CDC_COMP1_FS_CFG +
				    w->shift * 8, 0x07, rate);
		/* Enable compander clock */
		snd_soc_update_bits(codec,
				    SITAR_A_CDC_CLK_RX_B2_CTL,
				    1 << w->shift,
				    1 << w->shift);
		/* Toggle compander reset bits */
		snd_soc_update_bits(codec,
				    SITAR_A_CDC_CLK_OTHR_RESET_CTL,
				    1 << w->shift,
				    1 << w->shift);
		snd_soc_update_bits(codec,
				    SITAR_A_CDC_CLK_OTHR_RESET_CTL,
				    1 << w->shift, 0);
		sitar_config_gain_compander(codec, w->shift, 1, event);
		/* Compander enable -> 0x370/0x378 */
		snd_soc_update_bits(codec, SITAR_A_CDC_COMP1_B1_CTL +
				    w->shift * 8, 0x03, 0x03);
		/* Update the RMS meter resampling */
		snd_soc_update_bits(codec,
				    SITAR_A_CDC_COMP1_B3_CTL +
				    w->shift * 8, 0xFF, 0x01);
		snd_soc_update_bits(codec,
				    SITAR_A_CDC_COMP1_B2_CTL +
				    w->shift * 8, 0xF0, 0x50);
		usleep_range(COMP_BRINGUP_WAIT_TIME, COMP_BRINGUP_WAIT_TIME);
		break;
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec,
				    SITAR_A_CDC_CLSG_CTL,
				    0x11, 0x00);
		if (w->shift == COMPANDER_1)
			value = 0x22;
		else
			value = 0x11;
		snd_soc_write(codec,
			      SITAR_A_CDC_CONN_CLSG_CTL, value);

		snd_soc_update_bits(codec, SITAR_A_CDC_COMP1_B2_CTL +
				    w->shift * 8, 0x0F,
				    comp_samp_params[rate].peak_det_timeout);
		snd_soc_update_bits(codec, SITAR_A_CDC_COMP1_B2_CTL +
				    w->shift * 8, 0xF0,
				    comp_samp_params[rate].rms_meter_div_fact);
		snd_soc_update_bits(codec, SITAR_A_CDC_COMP1_B3_CTL +
				w->shift * 8, 0xFF,
				comp_samp_params[rate].rms_meter_resamp_fact);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, SITAR_A_CDC_COMP1_B1_CTL +
				    w->shift * 8, 0x03, 0x00);
		/* Toggle compander reset bits */
		snd_soc_update_bits(codec,
				    SITAR_A_CDC_CLK_OTHR_RESET_CTL,
				    1 << w->shift,
				    1 << w->shift);
		snd_soc_update_bits(codec,
				    SITAR_A_CDC_CLK_OTHR_RESET_CTL,
				    1 << w->shift, 0);
		/* Disable compander clock */
		snd_soc_update_bits(codec,
				    SITAR_A_CDC_CLK_RX_B2_CTL,
				    1 << w->shift,
				    0);
		/* Restore the gain */
		sitar_config_gain_compander(codec, w->shift,
					    sitar->comp_enabled[w->shift],
					    event);
		snd_soc_update_bits(codec,
				    SITAR_A_CDC_CLSG_CTL,
				    0x11, 0x11);
		snd_soc_write(codec,
			      SITAR_A_CDC_CONN_CLSG_CTL, 0x14);
		break;
	}
rtn:
	return 0;
}

static int sitar_codec_dem_input_selection(struct snd_soc_dapm_widget *w,
						struct snd_kcontrol *kcontrol,
						int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);
	pr_debug("%s: compander#1->enable(%d) compander#2->enable(%d) reg(0x%x = 0x%x) event(%d)\n",
		__func__, sitar->comp_enabled[COMPANDER_1],
		sitar->comp_enabled[COMPANDER_2],
		SITAR_A_CDC_RX1_B6_CTL + w->shift * 8,
		snd_soc_read(codec, SITAR_A_CDC_RX1_B6_CTL + w->shift * 8),
		event);
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		if (sitar->comp_enabled[COMPANDER_1] ||
		    sitar->comp_enabled[COMPANDER_2])
			snd_soc_update_bits(codec,
					    SITAR_A_CDC_RX1_B6_CTL +
					    w->shift * 8,
					    1 << 5, 0);
		else
			snd_soc_update_bits(codec,
					    SITAR_A_CDC_RX1_B6_CTL +
					    w->shift * 8,
					    1 << 5, 0x20);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec,
				    SITAR_A_CDC_RX1_B6_CTL + w->shift * 8,
				    1 << 5, 0);
		break;
	}
	return 0;
}

static const char * const sitar_ear_pa_gain_text[] = {"POS_6_DB",
					"POS_2_DB", "NEG_2P5_DB", "NEG_12_DB"};

static const struct soc_enum sitar_ear_pa_gain_enum[] = {
		SOC_ENUM_SINGLE_EXT(4, sitar_ear_pa_gain_text),
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
	SOC_SINGLE_S8_TLV("RX2 Digital Volume", SITAR_A_CDC_RX2_VOL_CTL_B2_CTL,
		-84, 40, digital_gain),
	SOC_SINGLE_S8_TLV("RX3 Digital Volume", SITAR_A_CDC_RX3_VOL_CTL_B2_CTL,
		-84, 40, digital_gain),

	SOC_SINGLE_S8_TLV("DEC1 Volume", SITAR_A_CDC_TX1_VOL_CTL_GAIN, -84, 40,
		digital_gain),
	SOC_SINGLE_S8_TLV("DEC2 Volume", SITAR_A_CDC_TX2_VOL_CTL_GAIN, -84, 40,
		digital_gain),
	SOC_SINGLE_S8_TLV("DEC3 Volume", SITAR_A_CDC_TX3_VOL_CTL_GAIN, -84, 40,
		digital_gain),
	SOC_SINGLE_S8_TLV("DEC4 Volume", SITAR_A_CDC_TX4_VOL_CTL_GAIN, -84, 40,
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
	SOC_SINGLE_TLV("ADC3 Volume", SITAR_A_TX_3_EN, 5, 3, 0, analog_gain),

	SOC_SINGLE_EXT("ANC Slot", SND_SOC_NOPM, 0, 0, 100, sitar_get_anc_slot,
				   sitar_put_anc_slot),

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
	SOC_SINGLE_EXT("COMP1 Switch", SND_SOC_NOPM, COMPANDER_1, 1, 0,
				sitar_get_compander, sitar_set_compander),
	SOC_SINGLE_EXT("COMP2 Switch", SND_SOC_NOPM, COMPANDER_2, 1, 0,
				sitar_get_compander, sitar_set_compander),
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
	"ZERO", "RX1", "INV_RX1", "RX2"
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

static const char *sb_tx4_mux_text[] = {
	"ZERO", "RMIX1", "RMIX2", "RMIX3", "RMIX4", "RMIX5", "RMIX6", "RMIX7",
		"DEC4"
};

static const char *sb_tx5_mux_text[] = {
	"ZERO", "RMIX1", "RMIX2", "RMIX3", "DEC1", "DEC2", "DEC3", "DEC4"
};

static const char *dec1_mux_text[] = {
	"ZERO", "DMIC1", "ADC1", "ADC2", "ADC3", "MBADC", "DMIC4", "ANC1_FB",
};

static const char *dec2_mux_text[] = {
	"ZERO", "DMIC2", "ADC1", "ADC2", "ADC3", "MBADC", "DMIC3", "ANC2_FB",
};

static const char *dec3_mux_text[] = {
	"ZERO", "DMIC3", "ADC1", "ADC2", "ADC3", "MBADC", "DMIC2", "DMIC4"
};

static const char *dec4_mux_text[] = {
	"ZERO", "DMIC4", "ADC1", "ADC2", "ADC3", "DMIC3", "DMIC2", "DMIC1"
};

static const char const *anc_mux_text[] = {
	"ZERO", "ADC1", "ADC2", "ADC3", "RSVD1", "RSVD2", "RSVD3",
	"MBADC", "RSVD4", "DMIC1", "DMIC2",	"DMIC3", "DMIC4"
};

static const char const *anc1_fb_mux_text[] = {
	"ZERO", "EAR_HPH_L", "EAR_LINE_1",
};

static const char const *iir_inp1_text[] = {
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

static const struct soc_enum sb_tx4_mux_enum =
	SOC_ENUM_SINGLE(SITAR_A_CDC_CONN_TX_SB_B4_CTL, 0, 9, sb_tx4_mux_text);

static const struct soc_enum sb_tx3_mux_enum =
	SOC_ENUM_SINGLE(SITAR_A_CDC_CONN_TX_SB_B3_CTL, 0, 9, sb_tx3_mux_text);

static const struct soc_enum sb_tx2_mux_enum =
	SOC_ENUM_SINGLE(SITAR_A_CDC_CONN_TX_SB_B2_CTL, 0, 9, sb_tx2_mux_text);

static const struct soc_enum sb_tx1_mux_enum =
	SOC_ENUM_SINGLE(SITAR_A_CDC_CONN_TX_SB_B1_CTL, 0, 9, sb_tx1_mux_text);

static const struct soc_enum dec1_mux_enum =
	SOC_ENUM_SINGLE(SITAR_A_CDC_CONN_TX_B1_CTL, 0, 8, dec1_mux_text);

static const struct soc_enum dec2_mux_enum =
	SOC_ENUM_SINGLE(SITAR_A_CDC_CONN_TX_B1_CTL, 3, 8, dec2_mux_text);

static const struct soc_enum dec3_mux_enum =
	SOC_ENUM_SINGLE(SITAR_A_CDC_CONN_TX_B2_CTL, 0, 8, dec3_mux_text);

static const struct soc_enum dec4_mux_enum =
	SOC_ENUM_SINGLE(SITAR_A_CDC_CONN_TX_B2_CTL, 3, 8, dec4_mux_text);

static const struct soc_enum anc1_mux_enum =
	SOC_ENUM_SINGLE(SITAR_A_CDC_CONN_ANC_B1_CTL, 0, 13, anc_mux_text);

static const struct soc_enum anc2_mux_enum =
	SOC_ENUM_SINGLE(SITAR_A_CDC_CONN_ANC_B1_CTL, 4, 13, anc_mux_text);

static const struct soc_enum anc1_fb_mux_enum =
	SOC_ENUM_SINGLE(SITAR_A_CDC_CONN_ANC_B2_CTL, 0, 3, anc1_fb_mux_text);

static const struct soc_enum iir1_inp1_mux_enum =
	SOC_ENUM_SINGLE(SITAR_A_CDC_CONN_EQ1_B1_CTL, 0, 16, iir_inp1_text);

static const struct soc_enum iir2_inp1_mux_enum =
	SOC_ENUM_SINGLE(SITAR_A_CDC_CONN_EQ2_B1_CTL, 0, 16, iir_inp1_text);

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
	SOC_DAPM_ENUM("RX DAC1 Mux", rx_dac1_enum);

static const struct snd_kcontrol_new rx_dac2_mux =
	SOC_DAPM_ENUM("RX DAC2 Mux", rx_dac2_enum);

static const struct snd_kcontrol_new rx_dac3_mux =
	SOC_DAPM_ENUM("RX DAC3 Mux", rx_dac3_enum);

static const struct snd_kcontrol_new rx_dac4_mux =
	SOC_DAPM_ENUM("RX DAC4 Mux", rx_dac4_enum);

static const struct snd_kcontrol_new sb_tx5_mux =
	SOC_DAPM_ENUM("SLIM TX5 MUX Mux", sb_tx5_mux_enum);

static const struct snd_kcontrol_new sb_tx4_mux =
	SOC_DAPM_ENUM("SLIM TX4 MUX Mux", sb_tx4_mux_enum);

static const struct snd_kcontrol_new sb_tx3_mux =
	SOC_DAPM_ENUM("SLIM TX3 MUX Mux", sb_tx3_mux_enum);

static const struct snd_kcontrol_new sb_tx2_mux =
	SOC_DAPM_ENUM("SLIM TX2 MUX Mux", sb_tx2_mux_enum);

static const struct snd_kcontrol_new sb_tx1_mux =
	SOC_DAPM_ENUM("SLIM TX1 MUX Mux", sb_tx1_mux_enum);

static int wcd9304_put_dec_enum(struct snd_kcontrol *kcontrol,
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
		ret = -EINVAL;
		goto out;
	}

	ret = kstrtouint(strpbrk(dec_name, "1234"), 10, &decimator);
	if (ret < 0) {
		pr_err("%s: Invalid decimator = %s\n", __func__, dec_name);
		ret = -EINVAL;
		goto out;
	}

	dev_dbg(w->dapm->dev, "%s(): widget = %s dec_name = %s decimator = %u"\
		"dec_mux = %u\n", __func__, w->name, dec_name, decimator,
		dec_mux);


	switch (decimator) {
	case 1:
	case 2:
		if ((dec_mux == 1) || (dec_mux == 6))
			adc_dmic_sel = ADC_DMIC_SEL_DMIC;
		else
			adc_dmic_sel = ADC_DMIC_SEL_ADC;
		break;
	case 3:
		if ((dec_mux == 1) || (dec_mux == 6) || (dec_mux == 7))
			adc_dmic_sel = ADC_DMIC_SEL_DMIC;
		else
			adc_dmic_sel = ADC_DMIC_SEL_ADC;
		break;
	case 4:
		if ((dec_mux == 1) || (dec_mux == 5)
			|| (dec_mux == 6) || (dec_mux == 7))
			adc_dmic_sel = ADC_DMIC_SEL_DMIC;
		else
			adc_dmic_sel = ADC_DMIC_SEL_ADC;
		break;
	default:
		pr_err("%s: Invalid Decimator = %u\n", __func__, decimator);
		ret = -EINVAL;
		goto out;
	}

	tx_mux_ctl_reg = SITAR_A_CDC_TX1_MUX_CTL + 8 * (decimator - 1);

	snd_soc_update_bits(codec, tx_mux_ctl_reg, 0x1, adc_dmic_sel);

	ret = snd_soc_dapm_put_enum_double(kcontrol, ucontrol);
out:
	kfree(widget_name);
	return ret;
}

#define WCD9304_DEC_ENUM(xname, xenum) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
	.info = snd_soc_info_enum_double, \
	.get = snd_soc_dapm_get_enum_double, \
	.put = wcd9304_put_dec_enum, \
	.private_value = (unsigned long)&xenum }

static const struct snd_kcontrol_new dec1_mux =
	WCD9304_DEC_ENUM("DEC1 MUX Mux", dec1_mux_enum);

static const struct snd_kcontrol_new dec2_mux =
	WCD9304_DEC_ENUM("DEC2 MUX Mux", dec2_mux_enum);

static const struct snd_kcontrol_new dec3_mux =
	WCD9304_DEC_ENUM("DEC3 MUX Mux", dec3_mux_enum);

static const struct snd_kcontrol_new dec4_mux =
	WCD9304_DEC_ENUM("DEC4 MUX Mux", dec4_mux_enum);

static const struct snd_kcontrol_new iir1_inp1_mux =
	SOC_DAPM_ENUM("IIR1 INP1 Mux", iir1_inp1_mux_enum);

static const struct snd_kcontrol_new iir2_inp1_mux =
	SOC_DAPM_ENUM("IIR2 INP1 Mux", iir2_inp1_mux_enum);

static const struct snd_kcontrol_new anc1_mux =
	SOC_DAPM_ENUM("ANC1 MUX Mux", anc1_mux_enum);

static const struct snd_kcontrol_new anc2_mux =
	SOC_DAPM_ENUM("ANC2 MUX Mux", anc2_mux_enum);

static const struct snd_kcontrol_new anc1_fb_mux =
	SOC_DAPM_ENUM("ANC1 FB MUX Mux", anc1_fb_mux_enum);

static const struct snd_kcontrol_new dac1_switch[] = {
	SOC_DAPM_SINGLE("Switch", SITAR_A_RX_EAR_EN, 5, 1, 0),
};

static int slim_tx_mixer_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist = snd_kcontrol_chip(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];

	ucontrol->value.integer.value[0] = widget->value;
	return 0;
}

static int slim_tx_mixer_put(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist = snd_kcontrol_chip(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct snd_soc_codec *codec = widget->codec;
	struct sitar_priv *sitar_p = snd_soc_codec_get_drvdata(codec);
	struct wcd9xxx *core = dev_get_drvdata(codec->dev->parent);
	struct soc_multi_mixer_control *mixer =
		((struct soc_multi_mixer_control *)kcontrol->private_value);
	u32 dai_id = widget->shift;
	u32 port_id = mixer->shift;
	u32 enable = ucontrol->value.integer.value[0];

	mutex_lock(&codec->mutex);

	if (sitar_p->intf_type != WCD9XXX_INTERFACE_TYPE_SLIMBUS) {
		if (dai_id != AIF1_CAP) {
			dev_err(codec->dev, "%s: invalid AIF for I2C mode\n",
				__func__);
			mutex_unlock(&codec->mutex);
			return -EINVAL;
		}
	}

	switch (dai_id) {
	case AIF1_CAP:
		if (enable && !(widget->value & 1 << port_id)) {
			if (wcd9xxx_tx_vport_validation(
						vport_check_table[dai_id],
						port_id,
						sitar_p->dai)) {
				dev_dbg(codec->dev, "%s: TX%u is used by other virtual port\n",
					__func__, port_id + 1);
				mutex_unlock(&codec->mutex);
				return 0;
			}
			widget->value |= 1 << port_id;
			list_add_tail(&core->tx_chs[port_id].list,
			&sitar_p->dai[dai_id].wcd9xxx_ch_list);
		} else if (!enable && (widget->value & 1 << port_id)) {
			widget->value &= ~(1<<port_id);
			list_del_init(&core->tx_chs[port_id].list);
		} else {
			if (enable)
				dev_dbg(codec->dev, "%s: TX%u port is used by this virtual port\n",
					__func__, port_id + 1);
			else
				dev_dbg(codec->dev, "%s: TX%u port is not used by this virtual port\n",
					__func__, port_id + 1);
			/* avoid update power function */
			mutex_unlock(&codec->mutex);
			return 0;
		}
	break;
	default:
		pr_err("Unknown AIF %d\n", dai_id);
		mutex_unlock(&codec->mutex);
		return -EINVAL;
	}

	pr_debug("%s: name %s sname %s updated value %u shift %d\n", __func__,
		widget->name, widget->sname, widget->value, widget->shift);
	snd_soc_dapm_mixer_update_power(widget, kcontrol, enable);
	mutex_unlock(&codec->mutex);
	return 0;
}

static int slim_rx_mux_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist = snd_kcontrol_chip(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];

	ucontrol->value.enumerated.item[0] = widget->value;
	return 0;
}

static const char * const slim_rx_mux_text[] = {
	"ZERO", "AIF1_PB"
};

static int slim_rx_mux_put(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist = snd_kcontrol_chip(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct snd_soc_codec *codec = widget->codec;
	struct sitar_priv *sitar_p = snd_soc_codec_get_drvdata(codec);
	struct wcd9xxx *core = dev_get_drvdata(codec->dev->parent);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 port_id = widget->shift;

	widget->value = ucontrol->value.enumerated.item[0];

	mutex_lock(&codec->mutex);

	if (sitar_p->intf_type != WCD9XXX_INTERFACE_TYPE_SLIMBUS) {
		if (widget->value > 1) {
			dev_err(codec->dev, "%s: invalid AIF for I2C mode\n",
				__func__);
			goto err;
		}
	}

	switch (widget->value) {
	case 0:
		list_del_init(&core->rx_chs[port_id].list);
		break;
	case 1:
		if (wcd9xxx_rx_vport_validation(port_id +
			SITAR_RX_PORT_START_NUMBER,
			&sitar_p->dai[AIF1_PB].wcd9xxx_ch_list)) {
			dev_dbg(codec->dev, "%s: RX%u is used by current requesting AIF_PB itself\n",
				__func__, port_id + 1);
			goto rtn;
		}
		list_add_tail(&core->rx_chs[port_id].list,
			      &sitar_p->dai[AIF1_PB].wcd9xxx_ch_list);
		break;
	break;
	default:
		pr_err("Unknown AIF %d\n", widget->value);
		goto err;
	}

rtn:
	snd_soc_dapm_mux_update_power(widget, kcontrol, 1, widget->value, e);
	mutex_unlock(&codec->mutex);
	return 0;
err:
	mutex_unlock(&codec->mutex);
	return -EINVAL;
}

static const struct soc_enum slim_rx_mux_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(slim_rx_mux_text), slim_rx_mux_text);

static const struct snd_kcontrol_new sitar_aif_pb_mux[SITAR_RX_MAX] = {
	SOC_DAPM_ENUM_EXT("SLIM RX1 MUX", slim_rx_mux_enum,
			  slim_rx_mux_get, slim_rx_mux_put),
	SOC_DAPM_ENUM_EXT("SLIM RX2 MUX", slim_rx_mux_enum,
			  slim_rx_mux_get, slim_rx_mux_put),
	SOC_DAPM_ENUM_EXT("SLIM RX3 MUX", slim_rx_mux_enum,
			  slim_rx_mux_get, slim_rx_mux_put),
	SOC_DAPM_ENUM_EXT("SLIM RX4 MUX", slim_rx_mux_enum,
			  slim_rx_mux_get, slim_rx_mux_put),
	SOC_DAPM_ENUM_EXT("SLIM RX5 MUX", slim_rx_mux_enum,
			  slim_rx_mux_get, slim_rx_mux_put)
};

static const struct snd_kcontrol_new sitar_aif_cap_mixer[SITAR_TX_MAX] = {
	SOC_SINGLE_EXT("SLIM TX1", SND_SOC_NOPM, SITAR_TX1, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX2", SND_SOC_NOPM, SITAR_TX2, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX3", SND_SOC_NOPM, SITAR_TX3, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX4", SND_SOC_NOPM, SITAR_TX4, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX5", SND_SOC_NOPM, SITAR_TX5, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
};


static void sitar_codec_enable_adc_block(struct snd_soc_codec *codec,
	int enable)
{
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s %d\n", __func__, enable);

	if (enable) {
		sitar->adc_count++;
		snd_soc_update_bits(codec, SITAR_A_CDC_CLK_OTHR_CTL,
				0x02, 0x02);
	} else {
		sitar->adc_count--;
		if (!sitar->adc_count) {
			if (!sitar->mbhc_polling_active)
				snd_soc_update_bits(codec,
					SITAR_A_CDC_CLK_OTHR_CTL, 0xE0, 0x0);
		}
	}
}

static int sitar_codec_enable_adc(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	u16 adc_reg;
	u8 init_bit_shift;

	pr_debug("%s %d\n", __func__, event);

	if (w->reg == SITAR_A_TX_1_2_EN)
		adc_reg = SITAR_A_TX_1_2_TEST_CTL;
	else if (w->reg == SITAR_A_TX_3_EN)
		adc_reg = SITAR_A_TX_3_TEST_CTL;
	else {
		pr_err("%s: Error, invalid adc register\n", __func__);
		return -EINVAL;
	}

	if (w->shift == 3)
		init_bit_shift = 6;
	else if (w->shift == 7)
		init_bit_shift = 7;
	else {
		pr_err("%s: Error, invalid init bit postion adc register\n",
				__func__);
		return -EINVAL;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		sitar_codec_enable_adc_block(codec, 1);
		snd_soc_update_bits(codec, adc_reg, 1 << init_bit_shift,
							1 << init_bit_shift);
		break;
	case SND_SOC_DAPM_POST_PMU:

		snd_soc_update_bits(codec, adc_reg, 1 << init_bit_shift, 0x00);

		break;
	case SND_SOC_DAPM_POST_PMD:
		sitar_codec_enable_adc_block(codec, 0);
		break;
	}
	return 0;
}

static int sitar_lineout_dac_event(struct snd_soc_dapm_widget *w,
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

static void sitar_enable_classg(struct snd_soc_codec *codec,
	bool enable)
{

	if (enable) {
		snd_soc_update_bits(codec,
			SITAR_A_CDC_CLK_OTHR_RESET_CTL, 0x10, 0x00);
		snd_soc_update_bits(codec, SITAR_A_CP_STATIC, 0x07, 0x00);
		snd_soc_update_bits(codec, SITAR_A_CP_STATIC, 0x08, 0x00);
		snd_soc_update_bits(codec, SITAR_A_CP_STATIC, 0x10, 0x00);

	} else {
		snd_soc_update_bits(codec,
			SITAR_A_CDC_CLK_OTHR_RESET_CTL, 0x10, 0x10);
		snd_soc_update_bits(codec, SITAR_A_CP_STATIC, 0x07, 0x03);
		snd_soc_update_bits(codec, SITAR_A_CP_STATIC, 0x08, 0x08);
		snd_soc_update_bits(codec, SITAR_A_CP_STATIC, 0x10, 0x10);
	}
}

static bool sitar_is_hph_pa_on(struct snd_soc_codec *codec)
{
	u8 hph_reg_val = 0;
	hph_reg_val = snd_soc_read(codec, SITAR_A_RX_HPH_CNP_EN);

	return (hph_reg_val & 0x30) ? true : false;
}

static bool sitar_is_line_pa_on(struct snd_soc_codec *codec)
{
	u8 line_reg_val = 0;
	line_reg_val = snd_soc_read(codec, SITAR_A_RX_LINE_CNP_EN);

	return (line_reg_val & 0x03) ? true : false;
}

static int sitar_codec_enable_lineout(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);
	u16 lineout_gain_reg;

	pr_debug("%s %d %s comp2 enable %d\n", __func__, event, w->name,
		 sitar->comp_enabled[COMPANDER_2]);

	if (sitar->comp_enabled[COMPANDER_2])
		goto rtn;

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
		if (sitar_is_hph_pa_on(codec)) {
			snd_soc_update_bits(codec, SITAR_A_CDC_RX1_B6_CTL,
				0x20, 0x00);
			sitar_enable_classg(codec, false);
		} else {
			snd_soc_update_bits(codec, SITAR_A_CDC_RX1_B6_CTL,
				0x20, 0x20);
			sitar_enable_classg(codec, true);
		}
		snd_soc_update_bits(codec, lineout_gain_reg, 0x10, 0x10);
		break;
	case SND_SOC_DAPM_POST_PMU:
		pr_debug("%s: sleeping 32 ms after %s PA turn on\n",
				__func__, w->name);
		usleep_range(32000, 32000);
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (sitar_is_hph_pa_on(codec))
			sitar_enable_classg(codec, true);
		else
			sitar_enable_classg(codec, false);

		snd_soc_update_bits(codec, lineout_gain_reg, 0x10, 0x00);
		break;
	}
rtn:
	return 0;
}

static int sitar_codec_enable_dmic(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	u16 tx_dmic_ctl_reg;
	u8 dmic_clk_sel, dmic_clk_en;
	unsigned int dmic;
	int ret;

	ret = kstrtouint(strpbrk(w->name, "1234"), 10, &dmic);
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

	tx_dmic_ctl_reg = SITAR_A_CDC_TX1_DMIC_CTL + 8 * (dmic - 1);

	pr_debug("%s %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
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

static int sitar_codec_enable_anc(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	const char *filename;
	const struct firmware *fw;
	int i;
	int ret;
	int num_anc_slots;
	struct anc_header *anc_head;
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);
	u32 anc_writes_size = 0;
	int anc_size_remaining;
	u32 *anc_ptr;
	u16 reg;
	u8 mask, val, old_val;

	pr_debug("%s %d\n", __func__, event);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:

		/* Use the same firmware file as that of WCD9310,
		 * since the register sequences are same for
		 * WCD9310 and WCD9304
		 */
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

		if (sitar->anc_slot >= num_anc_slots) {
			dev_err(codec->dev, "Invalid ANC slot selected\n");
			release_firmware(fw);
			return -EINVAL;
		}

		for (i = 0; i < num_anc_slots; i++) {

			if (anc_size_remaining < SITAR_PACKED_REG_SIZE) {
				dev_err(codec->dev, "Invalid register format\n");
				release_firmware(fw);
				return -EINVAL;
			}
			anc_writes_size = (u32)(*anc_ptr);
			anc_size_remaining -= sizeof(u32);
			anc_ptr += 1;

			if (anc_writes_size * SITAR_PACKED_REG_SIZE
				> anc_size_remaining) {
				dev_err(codec->dev, "Invalid register format\n");
				release_firmware(fw);
				return -ENOMEM;
			}

			if (sitar->anc_slot == i)
				break;

			anc_size_remaining -= (anc_writes_size *
				SITAR_PACKED_REG_SIZE);
			anc_ptr += anc_writes_size;
		}
		if (i == num_anc_slots) {
			dev_err(codec->dev, "Selected ANC slot not present\n");
			release_firmware(fw);
			return -ENOMEM;
		}

		for (i = 0; i < anc_writes_size; i++) {
			SITAR_CODEC_UNPACK_ENTRY(anc_ptr[i], reg,
				mask, val);
			old_val = snd_soc_read(codec, reg);
			snd_soc_write(codec, reg, (old_val & ~mask) |
				(val & mask));
		}

		release_firmware(fw);

		/* For Sitar, it is required to enable both Feed-forward
		 * and Feed back clocks to enable ANC
		 */
		snd_soc_write(codec, SITAR_A_CDC_CLK_ANC_CLK_EN_CTL, 0x0F);

		break;

	case SND_SOC_DAPM_POST_PMD:
		snd_soc_write(codec, SITAR_A_CDC_CLK_ANC_RESET_CTL, 0xFF);
		snd_soc_write(codec, SITAR_A_CDC_CLK_ANC_CLK_EN_CTL, 0x00);
		break;
	}
	return 0;
}


static void sitar_codec_start_hs_polling(struct snd_soc_codec *codec)
{
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);
	int mbhc_state = sitar->mbhc_state;

	pr_debug("%s: enter\n", __func__);
	if (!sitar->mbhc_polling_active) {
		pr_debug("Polling is not active, do not start polling\n");
		return;
	}
	snd_soc_write(codec, SITAR_A_MBHC_SCALING_MUX_1, 0x84);


	if (!sitar->no_mic_headset_override) {
		if (mbhc_state == MBHC_STATE_POTENTIAL) {
			pr_debug("%s recovering MBHC state macine\n", __func__);
			sitar->mbhc_state = MBHC_STATE_POTENTIAL_RECOVERY;
			/* set to max button press threshold */
			snd_soc_write(codec, SITAR_A_CDC_MBHC_VOLT_B2_CTL,
				      0x7F);
			snd_soc_write(codec, SITAR_A_CDC_MBHC_VOLT_B1_CTL,
				      0xFF);
			snd_soc_write(codec, SITAR_A_CDC_MBHC_VOLT_B4_CTL,
				       0x7F);
			snd_soc_write(codec, SITAR_A_CDC_MBHC_VOLT_B3_CTL,
				      0xFF);
			/* set to max */
			snd_soc_write(codec, SITAR_A_CDC_MBHC_VOLT_B6_CTL,
				      0x7F);
			snd_soc_write(codec, SITAR_A_CDC_MBHC_VOLT_B5_CTL,
				      0xFF);
		}
	}

	snd_soc_write(codec, SITAR_A_MBHC_SCALING_MUX_1, 0x84);
	snd_soc_write(codec, SITAR_A_CDC_MBHC_EN_CTL, 0x1);
	snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_CLK_CTL, 0x8, 0x0);
	snd_soc_write(codec, SITAR_A_CDC_MBHC_EN_CTL, 0x1);
}

static void sitar_codec_pause_hs_polling(struct snd_soc_codec *codec)
{
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s: enter\n", __func__);
	if (!sitar->mbhc_polling_active) {
		pr_debug("polling not active, nothing to pause\n");
		return;
	}

	snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_CLK_CTL, 0x8, 0x8);
	pr_debug("%s: leave\n", __func__);

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
		SITAR_ACQUIRE_LOCK(sitar->codec_resource_lock);
		if (sitar->mbhc_polling_active) {
			sitar_codec_pause_hs_polling(codec);
			mbhc_was_polling = true;
		}
		snd_soc_update_bits(codec,
			sitar->mbhc_bias_regs.cfilt_ctl, 0x40, reg_mode_val);
		if (mbhc_was_polling)
			sitar_codec_start_hs_polling(codec);
		SITAR_RELEASE_LOCK(sitar->codec_resource_lock);
		pr_debug("%s: CFILT mode change (%x to %x)\n", __func__,
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
		if (sitar->mbhc_micbias_switched == 0 &&
			sitar->mbhc_polling_active) {

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
			pr_debug("%s: Enabled MBHC Mic bias to VDDIO Switch\n",
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
			pr_debug("%s: Disabled MBHC Mic bias to VDDIO Switch\n",
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

	pr_debug("%s %d\n", __func__, event);
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
		if (w->reg == sitar->mbhc_bias_regs.ctl_reg) {
			SITAR_ACQUIRE_LOCK(sitar->codec_resource_lock);
			sitar_codec_switch_micbias(codec, 0);
			SITAR_RELEASE_LOCK(sitar->codec_resource_lock);
		}

		snd_soc_update_bits(codec, w->reg, 0x1E, 0x00);
		sitar_codec_update_cfilt_usage(codec, cfilt_sel_val, 1);

		if (strnstr(w->name, internal1_text, 30))
			snd_soc_update_bits(codec, micb_int_reg, 0xFF, 0xA4);
		else if (strnstr(w->name, internal2_text, 30))
			snd_soc_update_bits(codec, micb_int_reg, 0x1C, 0x1C);
		break;
	case SND_SOC_DAPM_POST_PMU:

		usleep_range(20000, 20000);
		if (sitar->mbhc_polling_active &&
		    sitar->mbhc_cfg.micbias == micb_line) {
			SITAR_ACQUIRE_LOCK(sitar->codec_resource_lock);
			sitar_codec_pause_hs_polling(codec);
			sitar_codec_start_hs_polling(codec);
			SITAR_RELEASE_LOCK(sitar->codec_resource_lock);
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

static void tx_hpf_corner_freq_callback(struct work_struct *work)
{
	struct delayed_work *hpf_delayed_work;
	struct hpf_work *hpf_work;
	struct sitar_priv *sitar;
	struct snd_soc_codec *codec;
	u16 tx_mux_ctl_reg;
	u8 hpf_cut_of_freq;

	hpf_delayed_work = to_delayed_work(work);
	hpf_work = container_of(hpf_delayed_work, struct hpf_work, dwork);
	sitar = hpf_work->sitar;
	codec = hpf_work->sitar->codec;
	hpf_cut_of_freq = hpf_work->tx_hpf_cut_of_freq;

	tx_mux_ctl_reg = SITAR_A_CDC_TX1_MUX_CTL +
				(hpf_work->decimator - 1) * 8;

	pr_debug("%s(): decimator %u hpf_cut_of_freq 0x%x\n", __func__,
			hpf_work->decimator, (unsigned int)hpf_cut_of_freq);

	snd_soc_update_bits(codec, tx_mux_ctl_reg,
			CUT_OF_FREQ_MASK, hpf_cut_of_freq << 4);
}

static int sitar_codec_enable_dec(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	u16 dec_reset_reg, gain_reg, tx_vol_ctl_reg, tx_mux_ctl_reg;
	unsigned int decimator;
	char *dec_name = NULL;
	char *widget_name = NULL;
	char *temp;
	int ret = 0;
	u8 dec_hpf_cut_of_freq, current_gain;

	pr_debug("%s %d\n", __func__, event);

	widget_name = kstrndup(w->name, 15, GFP_KERNEL);
	if (!widget_name)
		return -ENOMEM;
	temp = widget_name;

	dec_name = strsep(&widget_name, " ");
	widget_name = temp;
	if (!dec_name) {
		pr_err("%s: Invalid decimator = %s\n", __func__, w->name);
		ret = -EINVAL;
		goto out;
	}

	ret = kstrtouint(strpbrk(dec_name, "1234"), 10, &decimator);
	if (ret < 0) {
		pr_err("%s: Invalid decimator = %s\n", __func__, dec_name);
		ret = -EINVAL;
		goto out;
	}

	pr_debug("%s(): widget = %s dec_name = %s decimator = %u\n", __func__,
		w->name, dec_name, decimator);

	if (w->reg == SITAR_A_CDC_CLK_TX_CLK_EN_B1_CTL)
		dec_reset_reg = SITAR_A_CDC_CLK_TX_RESET_B1_CTL;
	else {
		pr_err("%s: Error, incorrect dec\n", __func__);
		ret = EINVAL;
		goto out;
	}

	tx_vol_ctl_reg = SITAR_A_CDC_TX1_VOL_CTL_CFG + 8 * (decimator - 1);
	tx_mux_ctl_reg = SITAR_A_CDC_TX1_MUX_CTL + 8 * (decimator - 1);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Enable TX Digital Mute */
		snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x01, 0x01);

		snd_soc_update_bits(codec, dec_reset_reg, 1 << w->shift,
			1 << w->shift);
		snd_soc_update_bits(codec, dec_reset_reg, 1 << w->shift, 0x0);

		dec_hpf_cut_of_freq = snd_soc_read(codec, tx_mux_ctl_reg);
		dec_hpf_cut_of_freq = (dec_hpf_cut_of_freq &
						CUT_OF_FREQ_MASK) >> 4;

		tx_hpf_work[decimator - 1].tx_hpf_cut_of_freq =
						dec_hpf_cut_of_freq;

		if ((dec_hpf_cut_of_freq != CF_MIN_3DB_150HZ)) {
			/* Set cut off freq to CF_MIN_3DB_150HZ (0x01) */
			snd_soc_update_bits(codec, tx_mux_ctl_reg,
				CUT_OF_FREQ_MASK, CF_MIN_3DB_150HZ << 4);
		}

		/* enable HPF */
		snd_soc_update_bits(codec, tx_mux_ctl_reg, 0x08, 0x00);

		break;

	case SND_SOC_DAPM_POST_PMU:
		/* Disable TX Digital Mute */
		snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x01, 0x00);

		if (tx_hpf_work[decimator - 1].tx_hpf_cut_of_freq !=
				CF_MIN_3DB_150HZ) {
			schedule_delayed_work(&tx_hpf_work[decimator - 1].dwork,
				msecs_to_jiffies(300));
		}

		/* Reprogram the digital gain after power up of Decimator */
		gain_reg = SITAR_A_CDC_TX1_VOL_CTL_GAIN + (8 * w->shift);
		current_gain = snd_soc_read(codec, gain_reg);
		snd_soc_write(codec, gain_reg, current_gain);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		/* Enable Digital Mute, Cancel possibly scheduled work */
		snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x01, 0x01);
		cancel_delayed_work_sync(&tx_hpf_work[decimator - 1].dwork);

		break;

	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, tx_mux_ctl_reg, 0x08, 0x08);
		snd_soc_update_bits(codec, tx_mux_ctl_reg, CUT_OF_FREQ_MASK,
			(tx_hpf_work[decimator - 1].tx_hpf_cut_of_freq) << 4);
		break;

	}

out:
	kfree(widget_name);
	return ret;

}

static int sitar_codec_reset_interpolator(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	u16 gain_reg;
	u8 current_gain;

	pr_debug("%s %d %s\n", __func__, event, w->name);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, SITAR_A_CDC_CLK_RX_RESET_CTL,
			1 << w->shift, 1 << w->shift);
		snd_soc_update_bits(codec, SITAR_A_CDC_CLK_RX_RESET_CTL,
			1 << w->shift, 0x0);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* Reprogram gain after power up interpolator */
		gain_reg = SITAR_A_CDC_RX1_VOL_CTL_B2_CTL + (8 * w->shift);
		current_gain = snd_soc_read(codec, gain_reg);
		snd_soc_write(codec, gain_reg, current_gain);
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

	pr_debug("%s %d\n", __func__, event);

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
static int sitar_hph_dac_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s %s %d comp#1 enable %d\n", __func__,
		 w->name, event, sitar->comp_enabled[COMPANDER_1]);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (w->reg == SITAR_A_RX_HPH_L_DAC_CTL) {
			if (!sitar->comp_enabled[COMPANDER_1]) {
				snd_soc_update_bits(codec,
						    SITAR_A_CDC_CONN_CLSG_CTL,
						    0x30, 0x20);
				snd_soc_update_bits(codec,
						    SITAR_A_CDC_CONN_CLSG_CTL,
						    0x0C, 0x08);
			}
		}
		snd_soc_update_bits(codec, w->reg, 0x40, 0x40);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, w->reg, 0x40, 0x00);
		if (w->reg == SITAR_A_RX_HPH_L_DAC_CTL) {
			snd_soc_update_bits(codec, SITAR_A_CDC_CONN_CLSG_CTL,
				0x30, 0x10);
			snd_soc_update_bits(codec, SITAR_A_CDC_CONN_CLSG_CTL,
				0x0C, 0x04);
		}
		break;
	}
	return 0;
}

static void sitar_snd_soc_jack_report(struct sitar_priv *sitar,
				     struct snd_soc_jack *jack, int status,
				     int mask)
{
	/* XXX: wake_lock_timeout()? */
	snd_soc_jack_report_no_dapm(jack, status, mask);
}

static void hphocp_off_report(struct sitar_priv *sitar,
	u32 jack_status, int irq)
{
	struct snd_soc_codec *codec;

	if (!sitar) {
		pr_err("%s: Bad sitar private data\n", __func__);
		return;
	}

	pr_info("%s: clear ocp status %x\n", __func__, jack_status);
	codec = sitar->codec;
	if (sitar->hph_status & jack_status) {
		sitar->hph_status &= ~jack_status;
		if (sitar->mbhc_cfg.headset_jack)
			sitar_snd_soc_jack_report(sitar,
						sitar->mbhc_cfg.headset_jack,
						sitar->hph_status,
						SITAR_JACK_MASK);
		snd_soc_update_bits(codec, SITAR_A_RX_HPH_OCP_CTL, 0x10, 0x00);
		snd_soc_update_bits(codec, SITAR_A_RX_HPH_OCP_CTL, 0x10, 0x10);
		/* reset retry counter as PA is turned off signifying
		* start of new OCP detection session
		*/
		if (WCD9XXX_IRQ_HPH_PA_OCPL_FAULT)
			sitar->hphlocp_cnt = 0;
		else
			sitar->hphrocp_cnt = 0;
		wcd9xxx_enable_irq(codec->control_data, irq);
	}
}

static void hphlocp_off_report(struct work_struct *work)
{
	struct sitar_priv *sitar = container_of(work, struct sitar_priv,
		hphlocp_work);
	hphocp_off_report(sitar, SND_JACK_OC_HPHL,
			  WCD9XXX_IRQ_HPH_PA_OCPL_FAULT);
}

static void hphrocp_off_report(struct work_struct *work)
{
	struct sitar_priv *sitar = container_of(work, struct sitar_priv,
		hphrocp_work);
	hphocp_off_report(sitar, SND_JACK_OC_HPHR,
			  WCD9XXX_IRQ_HPH_PA_OCPR_FAULT);
}

static int sitar_hph_pa_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);
	u8 mbhc_micb_ctl_val;
	pr_debug("%s: event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		mbhc_micb_ctl_val = snd_soc_read(codec,
				sitar->mbhc_bias_regs.ctl_reg);

		if (!(mbhc_micb_ctl_val & 0x80)) {
			SITAR_ACQUIRE_LOCK(sitar->codec_resource_lock);
			sitar_codec_switch_micbias(codec, 1);
			SITAR_RELEASE_LOCK(sitar->codec_resource_lock);
		}

		if (sitar_is_line_pa_on(codec))
			sitar_enable_classg(codec, false);
		else
			sitar_enable_classg(codec, true);

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

		SITAR_ACQUIRE_LOCK(sitar->codec_resource_lock);
		sitar_codec_switch_micbias(codec, 0);
		SITAR_RELEASE_LOCK(sitar->codec_resource_lock);

		pr_debug("%s: sleep 10 ms after %s PA disable.\n", __func__,
				w->name);
		usleep_range(10000, 10000);

		if (sitar_is_line_pa_on(codec))
			sitar_enable_classg(codec, true);
		else
			sitar_enable_classg(codec, false);

		break;
	}
	return 0;
}

static void sitar_get_mbhc_micbias_regs(struct snd_soc_codec *codec,
		struct mbhc_micbias_regs *micbias_regs)
{
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);
	unsigned int cfilt;

	switch (sitar->mbhc_cfg.micbias) {
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
		sitar->mbhc_data.micb_mv = sitar->pdata->micbias.cfilt1_mv;
		break;
	case SITAR_CFILT2_SEL:
		micbias_regs->cfilt_val = SITAR_A_MICB_CFILT_2_VAL;
		micbias_regs->cfilt_ctl = SITAR_A_MICB_CFILT_2_CTL;
		sitar->mbhc_data.micb_mv = sitar->pdata->micbias.cfilt2_mv;
		break;
	}
}

static int sitar_codec_enable_charge_pump(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	pr_debug("%s %d\n", __func__, event);
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_update_bits(codec, SITAR_A_CDC_CLK_OTHR_CTL, 0x01,
			0x01);
		snd_soc_update_bits(codec, SITAR_A_CDC_CLSG_CTL, 0x08, 0x08);
		usleep_range(200, 200);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, SITAR_A_CDC_CLSG_CTL, 0x08, 0x00);
		/*
		 * This delay is for the class G controller to settle down
		 * after turn OFF. The delay is as per the hardware spec for
		 * the codec
		 */
		usleep_range(20, 20);
		snd_soc_update_bits(codec, SITAR_A_CDC_CLK_OTHR_CTL, 0x01,
			0x00);
		break;
	}
	return 0;
}

static int sitar_ear_pa_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		pr_debug("%s: Sleeping 20ms after enabling EAR PA\n",
				 __func__);
		msleep(20);
		break;
	case SND_SOC_DAPM_POST_PMD:
		pr_debug("%s: Sleeping 20ms after disabling EAR PA\n",
				 __func__);
		msleep(20);
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

	SND_SOC_DAPM_PGA_E("EAR PA", SITAR_A_RX_EAR_EN, 4, 0, NULL, 0,
				sitar_ear_pa_event, SND_SOC_DAPM_POST_PMU |
				SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MIXER("DAC1", SITAR_A_RX_EAR_EN, 6, 0, dac1_switch,
		ARRAY_SIZE(dac1_switch)),
	SND_SOC_DAPM_SUPPLY("EAR DRIVER", SITAR_A_RX_EAR_EN, 3, 0, NULL, 0),

	SND_SOC_DAPM_AIF_IN_E("AIF1 PB", "AIF1 Playback", 0, SND_SOC_NOPM,
		AIF1_PB, 0, sitar_codec_enable_slimrx,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX("SLIM RX1 MUX", SND_SOC_NOPM, SITAR_RX1, 0,
		&sitar_aif_pb_mux[SITAR_RX1]),
	SND_SOC_DAPM_MUX("SLIM RX2 MUX", SND_SOC_NOPM, SITAR_RX2, 0,
		&sitar_aif_pb_mux[SITAR_RX2]),
	SND_SOC_DAPM_MUX("SLIM RX3 MUX", SND_SOC_NOPM, SITAR_RX3, 0,
		&sitar_aif_pb_mux[SITAR_RX3]),
	SND_SOC_DAPM_MUX("SLIM RX4 MUX", SND_SOC_NOPM, SITAR_RX4, 0,
		&sitar_aif_pb_mux[SITAR_RX4]),
	SND_SOC_DAPM_MUX("SLIM RX5 MUX", SND_SOC_NOPM, SITAR_RX5, 0,
		&sitar_aif_pb_mux[SITAR_RX5]),

	SND_SOC_DAPM_MIXER("SLIM RX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SLIM RX2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SLIM RX3", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SLIM RX4", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SLIM RX5", SND_SOC_NOPM, 0, 0, NULL, 0),

	/* Headphone */
	SND_SOC_DAPM_OUTPUT("HEADPHONE"),
	SND_SOC_DAPM_PGA_E("HPHL", SITAR_A_RX_HPH_CNP_EN, 5, 0, NULL, 0,
		sitar_hph_pa_event, SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA_E("HPHR", SITAR_A_RX_HPH_CNP_EN, 4, 0, NULL, 0,
		sitar_hph_pa_event, SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_DAC_E("HPHL DAC", NULL, SITAR_A_RX_HPH_L_DAC_CTL, 7, 0,
		sitar_hph_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("HPHR DAC", NULL, SITAR_A_RX_HPH_R_DAC_CTL, 7, 0,
		sitar_hph_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	/* Speaker */
	SND_SOC_DAPM_OUTPUT("LINEOUT1"),
	SND_SOC_DAPM_OUTPUT("LINEOUT2"),

	SND_SOC_DAPM_DAC_E("LINEOUT1 DAC", NULL, SITAR_A_RX_LINE_1_DAC_CTL, 7, 0
		, sitar_lineout_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DAC_E("LINEOUT2 DAC", NULL, SITAR_A_RX_LINE_2_DAC_CTL, 7, 0
		, sitar_lineout_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA_E("LINEOUT1 PA", SITAR_A_RX_LINE_CNP_EN, 0, 0, NULL,
			0, sitar_codec_enable_lineout, SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("LINEOUT2 PA", SITAR_A_RX_LINE_CNP_EN, 1, 0, NULL,
			0, sitar_codec_enable_lineout, SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MIXER_E("RX1 MIX1", SITAR_A_CDC_CLK_RX_B1_CTL, 0, 0, NULL,
		0, sitar_codec_reset_interpolator,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MIXER_E("RX2 MIX1", SITAR_A_CDC_CLK_RX_B1_CTL, 1, 0, NULL,
		0, sitar_codec_reset_interpolator,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MIXER_E("RX3 MIX1", SITAR_A_CDC_CLK_RX_B1_CTL, 2, 0, NULL,
		0, sitar_codec_reset_interpolator,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_MUX("DAC1 MUX", SND_SOC_NOPM, 0, 0,
		&rx_dac1_mux),
	SND_SOC_DAPM_MUX("DAC2 MUX", SND_SOC_NOPM, 0, 0,
		&rx_dac2_mux),
	SND_SOC_DAPM_MUX("DAC3 MUX", SND_SOC_NOPM, 0, 0,
		&rx_dac3_mux),
	SND_SOC_DAPM_MUX("DAC4 MUX", SND_SOC_NOPM, 0, 0,
		&rx_dac4_mux),

	SND_SOC_DAPM_MIXER_E("RX1 CHAIN", SND_SOC_NOPM, 0, 0, NULL,
		0, sitar_codec_dem_input_selection,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_POST_PMD),
	SND_SOC_DAPM_MIXER_E("RX2 CHAIN", SND_SOC_NOPM, 1, 0, NULL,
		0, sitar_codec_dem_input_selection,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_POST_PMD),
	SND_SOC_DAPM_MIXER_E("RX3 CHAIN", SND_SOC_NOPM, 2, 0, NULL,
		0, sitar_codec_dem_input_selection,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_POST_PMD),

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
	SND_SOC_DAPM_INPUT("AMIC2"),
	SND_SOC_DAPM_INPUT("AMIC3"),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS1 External", SITAR_A_MICB_1_CTL, 7, 0,
		sitar_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS1 Internal1", SITAR_A_MICB_1_CTL, 7, 0,
		sitar_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

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
	SND_SOC_DAPM_ADC_E("ADC3", NULL, SITAR_A_TX_3_EN, 7, 0,
		sitar_codec_enable_adc, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("DEC1 MUX", SITAR_A_CDC_CLK_TX_CLK_EN_B1_CTL, 0, 0,
		&dec1_mux, sitar_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("DEC2 MUX", SITAR_A_CDC_CLK_TX_CLK_EN_B1_CTL, 1, 0,
		&dec2_mux, sitar_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("DEC3 MUX", SITAR_A_CDC_CLK_TX_CLK_EN_B1_CTL, 2, 0,
		&dec3_mux, sitar_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("DEC4 MUX", SITAR_A_CDC_CLK_TX_CLK_EN_B1_CTL, 3, 0,
		&dec4_mux, sitar_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX("ANC1 MUX", SND_SOC_NOPM, 0, 0, &anc1_mux),
	SND_SOC_DAPM_MUX("ANC2 MUX", SND_SOC_NOPM, 0, 0, &anc2_mux),

	SND_SOC_DAPM_MIXER_E("ANC", SND_SOC_NOPM, 0, 0, NULL, 0,
		sitar_codec_enable_anc, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX("ANC1 FB MUX", SND_SOC_NOPM, 0, 0, &anc1_fb_mux),

	SND_SOC_DAPM_AIF_OUT_E("AIF1 CAP", "AIF1 Capture", 0, SND_SOC_NOPM,
				AIF1_CAP, 0, sitar_codec_enable_slimtx,
				SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MIXER("AIF1_CAP Mixer", SND_SOC_NOPM, AIF1_CAP, 0,
		sitar_aif_cap_mixer, ARRAY_SIZE(sitar_aif_cap_mixer)),

	SND_SOC_DAPM_MUX("SLIM TX1 MUX", SND_SOC_NOPM, SITAR_TX1, 0,
		&sb_tx1_mux),
	SND_SOC_DAPM_MUX("SLIM TX2 MUX", SND_SOC_NOPM, SITAR_TX2, 0,
		&sb_tx2_mux),
	SND_SOC_DAPM_MUX("SLIM TX3 MUX", SND_SOC_NOPM, SITAR_TX3, 0,
		&sb_tx3_mux),
	SND_SOC_DAPM_MUX("SLIM TX4 MUX", SND_SOC_NOPM, SITAR_TX3, 0,
		&sb_tx4_mux),
	SND_SOC_DAPM_MUX("SLIM TX5 MUX", SND_SOC_NOPM, SITAR_TX3, 0,
		&sb_tx5_mux),

	SND_SOC_DAPM_AIF_OUT_E("SLIM TX5", "AIF1 Capture", 0, SND_SOC_NOPM, 0,
				0, sitar_codec_enable_slimtx,
				SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

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

	SND_SOC_DAPM_SUPPLY("COMP1_CLK", SND_SOC_NOPM, COMPANDER_1, 0,
		sitar_config_compander, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_SUPPLY("COMP2_CLK", SND_SOC_NOPM, COMPANDER_2, 0,
		sitar_config_compander, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	/* Sidetone */
	SND_SOC_DAPM_MUX("IIR1 INP1 MUX", SND_SOC_NOPM, 0, 0, &iir1_inp1_mux),
	SND_SOC_DAPM_PGA("IIR1", SITAR_A_CDC_CLK_SD_CTL, 0, 0, NULL, 0),
	SND_SOC_DAPM_MUX("IIR2 INP1 MUX", SND_SOC_NOPM, 0, 0, &iir2_inp1_mux),
	SND_SOC_DAPM_PGA("IIR2", SITAR_A_CDC_CLK_SD_CTL, 1, 0, NULL, 0),

};

static const struct snd_soc_dapm_route audio_i2s_map[] = {
	{"RX_I2S_CLK", NULL, "CP"},
	{"RX_I2S_CLK", NULL, "CDC_CONN"},
	{"SLIM RX1", NULL, "RX_I2S_CLK"},
	{"SLIM RX2", NULL, "RX_I2S_CLK"},
	{"SLIM RX3", NULL, "RX_I2S_CLK"},
	{"SLIM RX4", NULL, "RX_I2S_CLK"},

	{"SLIM TX1", NULL, "TX_I2S_CLK"},
	{"SLIM TX2", NULL, "TX_I2S_CLK"},
	{"SLIM TX3", NULL, "TX_I2S_CLK"},
	{"SLIM TX4", NULL, "TX_I2S_CLK"},
};
#define SLIM_MIXER(x) (\
	{x, "SLIM TX1", "SLIM TX1 MUX"}, \
	{x, "SLIM TX2", "SLIM TX2 MUX"}, \
	{x, "SLIM TX3", "SLIM TX3 MUX"}, \
	{x, "SLIM TX4", "SLIM TX4 MUX"})


#define SLIM_MUX(x, y) (\
	{"SLIM RX1 MUX", x, y}, \
	{"SLIM RX2 MUX", x, y}, \
	{"SLIM RX3 MUX", x, y}, \
	{"SLIM RX4 MUX", x, y})

static const struct snd_soc_dapm_route audio_map[] = {
	/* Earpiece (RX MIX1) */
	{"EAR", NULL, "EAR PA"},
	{"EAR PA", "NULL", "DAC1"},
	{"DAC1", "Switch", "DAC1 MUX"},
	{"DAC1", NULL, "CP"},
	{"DAC1", NULL, "EAR DRIVER"},

	{"CP", NULL, "RX_BIAS"},

	{"LINEOUT1 DAC", NULL, "RX_BIAS"},
	{"LINEOUT2 DAC", NULL, "RX_BIAS"},

	{"LINEOUT2", NULL, "LINEOUT2 PA"},
	{"LINEOUT2 PA", NULL, "CP"},
	{"LINEOUT2 PA", NULL, "LINEOUT2 DAC"},
	{"LINEOUT2 DAC", NULL, "DAC3 MUX"},

	{"LINEOUT1", NULL, "LINEOUT1 PA"},
	{"LINEOUT2 PA", NULL, "CP"},
	{"LINEOUT1 PA", NULL, "LINEOUT1 DAC"},
	{"LINEOUT1 DAC", NULL, "DAC2 MUX"},

	{"ANC1 FB MUX", "EAR_HPH_L", "RX2 MIX1"},
	{"ANC1 FB MUX", "EAR_LINE_1", "RX3 MIX1"},
	{"ANC", NULL, "ANC1 FB MUX"},


	/* Headset (RX MIX1 and RX MIX2) */
	{"HEADPHONE", NULL, "HPHL"},
	{"HEADPHONE", NULL, "HPHR"},


	{"HPHL DAC", NULL, "CP"},
	{"HPHR DAC", NULL, "CP"},

	{"HPHL", NULL, "HPHL DAC"},
	{"HPHL DAC", "NULL", "RX2 CHAIN"},
	{"RX2 CHAIN", NULL, "DAC4 MUX"},
	{"HPHR", NULL, "HPHR DAC"},
	{"HPHR DAC", NULL, "RX3 CHAIN"},
	{"RX3 CHAIN", NULL, "RX3 MIX1"},

	{"DAC1 MUX", "RX1", "RX1 CHAIN"},
	{"DAC2 MUX", "RX1", "RX1 CHAIN"},

	{"DAC3 MUX", "RX1", "RX1 CHAIN"},
	{"DAC3 MUX", "INV_RX1", "RX1 CHAIN"},
	{"DAC3 MUX", "RX2", "RX2 MIX1"},

	{"DAC4 MUX", "ON", "RX2 MIX1"},

	{"RX1 CHAIN", NULL, "RX1 MIX1"},

	{"RX1 MIX1", NULL, "RX1 MIX1 INP1"},
	{"RX1 MIX1", NULL, "RX1 MIX1 INP2"},
	{"RX2 MIX1", NULL, "RX2 MIX1 INP1"},
	{"RX2 MIX1", NULL, "RX2 MIX1 INP2"},
	{"RX3 MIX1", NULL, "RX3 MIX1 INP1"},
	{"RX3 MIX1", NULL, "RX3 MIX1 INP2"},

	/* ANC */
	{"ANC", NULL, "ANC1 MUX"},
	{"ANC", NULL, "ANC2 MUX"},
	{"ANC1 MUX", "ADC1", "ADC1"},
	{"ANC1 MUX", "ADC2", "ADC2"},
	{"ANC1 MUX", "ADC3", "ADC3"},
	{"ANC2 MUX", "ADC1", "ADC1"},
	{"ANC2 MUX", "ADC2", "ADC2"},
	{"ANC2 MUX", "ADC3", "ADC3"},

	{"ANC", NULL, "CDC_CONN"},

	{"RX2 MIX1", NULL, "ANC"},
	{"RX3 MIX1", NULL, "ANC"},

	/* SLIMBUS Connections */
	{"AIF1 CAP", NULL, "AIF1_CAP Mixer"},

	/* SLIM_MIXER("AIF1_CAP Mixer"),*/
	{"AIF1_CAP Mixer", "SLIM TX1", "SLIM TX1 MUX"},
	{"AIF1_CAP Mixer", "SLIM TX2", "SLIM TX2 MUX"},
	{"AIF1_CAP Mixer", "SLIM TX3", "SLIM TX3 MUX"},
	{"AIF1_CAP Mixer", "SLIM TX4", "SLIM TX4 MUX"},
	/* SLIM_MUX("AIF1_PB", "AIF1 PB"), */
	{"SLIM RX1 MUX", "AIF1_PB", "AIF1 PB"},
	{"SLIM RX2 MUX", "AIF1_PB", "AIF1 PB"},
	{"SLIM RX3 MUX", "AIF1_PB", "AIF1 PB"},
	{"SLIM RX4 MUX", "AIF1_PB", "AIF1 PB"},

	{"SLIM RX1", NULL, "SLIM RX1 MUX"},
	{"SLIM RX2", NULL, "SLIM RX2 MUX"},
	{"SLIM RX3", NULL, "SLIM RX3 MUX"},
	{"SLIM RX4", NULL, "SLIM RX4 MUX"},

	{"RX1 MIX1", NULL, "COMP2_CLK"},
	{"RX2 MIX1", NULL, "COMP1_CLK"},
	{"RX3 MIX1", NULL, "COMP1_CLK"},

	/* Slimbus port 5 is non functional in Sitar 1.0 */
	{"RX1 MIX1 INP1", "RX1", "SLIM RX1"},
	{"RX1 MIX1 INP1", "RX2", "SLIM RX2"},
	{"RX1 MIX1 INP1", "RX3", "SLIM RX3"},
	{"RX1 MIX1 INP1", "RX4", "SLIM RX4"},
	{"RX1 MIX1 INP1", "IIR1", "IIR1"},
	{"RX1 MIX1 INP1", "IIR2", "IIR2"},
	{"RX1 MIX1 INP2", "RX1", "SLIM RX1"},
	{"RX1 MIX1 INP2", "RX2", "SLIM RX2"},
	{"RX1 MIX1 INP2", "RX3", "SLIM RX3"},
	{"RX1 MIX1 INP2", "RX4", "SLIM RX4"},
	{"RX1 MIX1 INP2", "IIR1", "IIR1"},
	{"RX1 MIX1 INP2", "IIR2", "IIR2"},
	{"RX2 MIX1 INP1", "RX1", "SLIM RX1"},
	{"RX2 MIX1 INP1", "RX2", "SLIM RX2"},
	{"RX2 MIX1 INP1", "RX3", "SLIM RX3"},
	{"RX2 MIX1 INP1", "RX4", "SLIM RX4"},
	{"RX2 MIX1 INP1", "IIR1", "IIR1"},
	{"RX2 MIX1 INP1", "IIR2", "IIR2"},
	{"RX2 MIX1 INP2", "RX1", "SLIM RX1"},
	{"RX2 MIX1 INP2", "RX2", "SLIM RX2"},
	{"RX2 MIX1 INP2", "RX3", "SLIM RX3"},
	{"RX2 MIX1 INP2", "RX4", "SLIM RX4"},
	{"RX2 MIX1 INP2", "IIR1", "IIR1"},
	{"RX2 MIX1 INP2", "IIR2", "IIR2"},
	{"RX3 MIX1 INP1", "RX1", "SLIM RX1"},
	{"RX3 MIX1 INP1", "RX2", "SLIM RX2"},
	{"RX3 MIX1 INP1", "RX3", "SLIM RX3"},
	{"RX3 MIX1 INP1", "RX4", "SLIM RX4"},
	{"RX3 MIX1 INP1", "IIR1", "IIR1"},
	{"RX3 MIX1 INP1", "IIR2", "IIR2"},
	{"RX3 MIX1 INP2", "RX1", "SLIM RX1"},
	{"RX3 MIX1 INP2", "RX2", "SLIM RX2"},
	{"RX3 MIX1 INP2", "RX3", "SLIM RX3"},
	{"RX3 MIX1 INP2", "RX4", "SLIM RX4"},
	{"RX3 MIX1 INP2", "IIR1", "IIR1"},
	{"RX3 MIX1 INP2", "IIR2", "IIR2"},


	/* TX */
	{"SLIM TX1 MUX", "DEC1", "DEC1 MUX"},
	{"SLIM TX2 MUX", "DEC2", "DEC2 MUX"},
	{"SLIM TX3 MUX", "DEC3", "DEC3 MUX"},
	{"SLIM TX4 MUX", "DEC4", "DEC4 MUX"},
	{"SLIM TX5 MUX", "DEC1", "DEC1 MUX"},
	{"SLIM TX5 MUX", "DEC2", "DEC2 MUX"},
	{"SLIM TX5 MUX", "DEC3", "DEC3 MUX"},
	{"SLIM TX5 MUX", "DEC4", "DEC4 MUX"},

	/* Decimator Inputs */
	{"DEC1 MUX", "DMIC1", "DMIC1"},
	{"DEC1 MUX", "DMIC4", "DMIC4"},
	{"DEC1 MUX", "ADC1", "ADC1"},
	{"DEC1 MUX", "ADC2", "ADC2"},
	{"DEC1 MUX", "ADC3", "ADC3"},
	{"DEC1 MUX", NULL, "CDC_CONN"},
	{"DEC2 MUX", "DMIC2", "DMIC2"},
	{"DEC2 MUX", "DMIC3", "DMIC3"},
	{"DEC2 MUX", "ADC1", "ADC1"},
	{"DEC2 MUX", "ADC2", "ADC2"},
	{"DEC2 MUX", "ADC3", "ADC3"},
	{"DEC2 MUX", NULL, "CDC_CONN"},
	{"DEC3 MUX", "DMIC3", "DMIC3"},
	{"DEC3 MUX", "ADC1", "ADC1"},
	{"DEC3 MUX", "ADC2", "ADC2"},
	{"DEC3 MUX", "ADC3", "ADC3"},
	{"DEC3 MUX", "DMIC2", "DMIC2"},
	{"DEC3 MUX", "DMIC4", "DMIC4"},
	{"DEC3 MUX", NULL, "CDC_CONN"},
	{"DEC4 MUX", "DMIC4", "DMIC4"},
	{"DEC4 MUX", "ADC1", "ADC1"},
	{"DEC4 MUX", "ADC2", "ADC2"},
	{"DEC4 MUX", "ADC3", "ADC3"},
	{"DEC4 MUX", "DMIC3", "DMIC3"},
	{"DEC4 MUX", "DMIC2", "DMIC2"},
	{"DEC4 MUX", "DMIC1", "DMIC1"},
	{"DEC4 MUX", NULL, "CDC_CONN"},

	/* ADC Connections */
	{"ADC1", NULL, "AMIC1"},
	{"ADC2", NULL, "AMIC2"},
	{"ADC3", NULL, "AMIC3"},

	/* IIR */
	{"IIR1", NULL, "IIR1 INP1 MUX"},
	{"IIR1 INP1 MUX", "DEC1", "DEC1 MUX"},
	{"IIR1 INP1 MUX", "DEC2", "DEC2 MUX"},
	{"IIR1 INP1 MUX", "DEC3", "DEC3 MUX"},
	{"IIR1 INP1 MUX", "DEC4", "DEC4 MUX"},
	{"IIR1 INP1 MUX", "RX1", "SLIM RX1"},
	{"IIR1 INP1 MUX", "RX2", "SLIM RX2"},
	{"IIR1 INP1 MUX", "RX3", "SLIM RX3"},
	{"IIR1 INP1 MUX", "RX4", "SLIM RX4"},
	{"IIR1 INP1 MUX", "RX5", "SLIM RX5"},

	{"IIR2", NULL, "IIR2 INP1 MUX"},
	{"IIR2 INP1 MUX", "DEC1", "DEC1 MUX"},
	{"IIR2 INP1 MUX", "DEC2", "DEC2 MUX"},
	{"IIR2 INP1 MUX", "DEC3", "DEC3 MUX"},
	{"IIR2 INP1 MUX", "DEC4", "DEC4 MUX"},
	{"IIR2 INP1 MUX", "RX1", "SLIM RX1"},
	{"IIR2 INP1 MUX", "RX2", "SLIM RX2"},
	{"IIR2 INP1 MUX", "RX3", "SLIM RX3"},
	{"IIR2 INP1 MUX", "RX4", "SLIM RX4"},
	{"IIR2 INP1 MUX", "RX5", "SLIM RX5"},

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
	int i;

	/* Registers lower than 0x100 are top level registers which can be
	* written by the Sitar core driver.
	*/
	if ((reg >= SITAR_A_CDC_MBHC_EN_CTL) || (reg < 0x100))
		return 1;

	/* IIR Coeff registers are not cacheable */
	if ((reg >= SITAR_A_CDC_IIR1_COEF_B1_CTL) &&
		(reg <= SITAR_A_CDC_IIR1_COEF_B5_CTL))
		return 1;

	for (i = 0; i < NUM_DECIMATORS; i++) {
		if (reg == SITAR_A_CDC_TX1_VOL_CTL_GAIN + (8 * i))
			return 1;
	}

	for (i = 0; i < NUM_INTERPOLATORS; i++) {
		if (reg == SITAR_A_CDC_RX1_VOL_CTL_B2_CTL + (8 * i))
			return 1;
	}

	if ((reg == SITAR_A_CDC_COMP1_SHUT_DOWN_STATUS) ||
		(reg == SITAR_A_CDC_COMP2_SHUT_DOWN_STATUS))
			return 1;
	return 0;
}

#define SITAR_FORMATS (SNDRV_PCM_FMTBIT_S16_LE)
static int sitar_write(struct snd_soc_codec *codec, unsigned int reg,
	unsigned int value)
{
	int ret;

	if (reg == SND_SOC_NOPM)
		return 0;

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

	if (reg == SND_SOC_NOPM)
		return 0;

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
	struct wcd9xxx *sitar_core = dev_get_drvdata(codec->dev->parent);

	if (SITAR_IS_1P0(sitar_core->version))
		snd_soc_update_bits(codec, SITAR_A_LDO_H_MODE_1, 0x80, 0x80);

	snd_soc_update_bits(codec, SITAR_A_BIAS_CURR_CTL_2, 0x0C, 0x08);
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
	struct wcd9xxx *sitar_core = dev_get_drvdata(codec->dev->parent);

	/* TODO lock resources accessed by audio streams and threaded
	* interrupt handlers
	*/

	pr_debug("%s, choice is %d, current is %d\n", __func__, choice,
		sitar->bandgap_type);

	if (sitar->bandgap_type == choice)
		return;

	if ((sitar->bandgap_type == SITAR_BANDGAP_OFF) &&
		(choice == SITAR_BANDGAP_AUDIO_MODE)) {
		sitar_codec_enable_audio_mode_bandgap(codec);
	} else if (choice == SITAR_BANDGAP_MBHC_MODE) {
		snd_soc_update_bits(codec, SITAR_A_BIAS_CURR_CTL_2, 0x0C, 0x08);
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
		snd_soc_update_bits(codec, SITAR_A_BIAS_CURR_CTL_2, 0x0C, 0x00);
		snd_soc_write(codec, SITAR_A_BIAS_CENTRAL_BG_CTL, 0x50);
		if (SITAR_IS_1P0(sitar_core->version))
			snd_soc_update_bits(codec, SITAR_A_LDO_H_MODE_1,
								0xF3, 0x61);
		usleep_range(1000, 1000);
	} else {
		pr_err("%s: Error, Invalid bandgap settings\n", __func__);
	}
	sitar->bandgap_type = choice;
}

static int sitar_codec_enable_config_mode(struct snd_soc_codec *codec,
	int enable)
{
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);

	if (enable) {
		snd_soc_update_bits(codec, SITAR_A_RC_OSC_FREQ, 0x10, 0);
		snd_soc_write(codec, SITAR_A_BIAS_OSC_BG_CTL, 0x17);
		usleep_range(5, 5);
		snd_soc_update_bits(codec, SITAR_A_RC_OSC_FREQ, 0x80,
			0x80);
		snd_soc_update_bits(codec, SITAR_A_RC_OSC_TEST, 0x80,
			0x80);
		usleep_range(10, 10);
		snd_soc_update_bits(codec, SITAR_A_RC_OSC_TEST, 0x80, 0);
		usleep_range(20, 20);
		snd_soc_update_bits(codec, SITAR_A_CLK_BUFF_EN1, 0x08, 0x08);
	} else {
		snd_soc_update_bits(codec, SITAR_A_BIAS_OSC_BG_CTL, 0x1,
			0);
		snd_soc_update_bits(codec, SITAR_A_RC_OSC_FREQ, 0x80, 0);
		snd_soc_update_bits(codec, SITAR_A_CLK_BUFF_EN1, 0x08, 0x00);
	}
	sitar->config_mode_active = enable ? true : false;

	return 0;
}

static int sitar_codec_enable_clock_block(struct snd_soc_codec *codec,
	int config_mode)
{
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s\n", __func__);

	if (config_mode) {
		sitar_codec_enable_config_mode(codec, 1);
		snd_soc_write(codec, SITAR_A_CLK_BUFF_EN2, 0x00);
		snd_soc_write(codec, SITAR_A_CLK_BUFF_EN2, 0x02);
		snd_soc_write(codec, SITAR_A_CLK_BUFF_EN1, 0x0D);
		usleep_range(1000, 1000);
	} else
		snd_soc_update_bits(codec, SITAR_A_CLK_BUFF_EN1, 0x08, 0x00);

	if (!config_mode && sitar->mbhc_polling_active) {
		snd_soc_write(codec, SITAR_A_CLK_BUFF_EN2, 0x02);
		sitar_codec_enable_config_mode(codec, 0);

	}

	snd_soc_update_bits(codec, SITAR_A_CLK_BUFF_EN1, 0x05, 0x05);
	snd_soc_update_bits(codec, SITAR_A_CLK_BUFF_EN2, 0x02, 0x00);
	snd_soc_update_bits(codec, SITAR_A_CLK_BUFF_EN2, 0x04, 0x04);
	usleep_range(50, 50);
	sitar->clock_active = true;
	return 0;
}
static void sitar_codec_disable_clock_block(struct snd_soc_codec *codec)
{
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);
	pr_debug("%s\n", __func__);
	snd_soc_update_bits(codec, SITAR_A_CLK_BUFF_EN2, 0x04, 0x00);
	ndelay(160);
	snd_soc_update_bits(codec, SITAR_A_CLK_BUFF_EN2, 0x02, 0x02);
	snd_soc_update_bits(codec, SITAR_A_CLK_BUFF_EN1, 0x05, 0x00);
	sitar->clock_active = false;
}

static int sitar_codec_mclk_index(const struct sitar_priv *sitar)
{
	if (sitar->mbhc_cfg.mclk_rate == SITAR_MCLK_RATE_12288KHZ)
		return 0;
	else if (sitar->mbhc_cfg.mclk_rate == SITAR_MCLK_RATE_9600KHZ)
		return 1;
	else {
		BUG_ON(1);
		return -EINVAL;
	}
}

static void sitar_codec_calibrate_hs_polling(struct snd_soc_codec *codec)
{
	u8 *n_ready, *n_cic;
	struct sitar_mbhc_btn_detect_cfg *btn_det;
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);

	btn_det = SITAR_MBHC_CAL_BTN_DET_PTR(sitar->mbhc_cfg.calibration);

	snd_soc_write(codec, SITAR_A_CDC_MBHC_VOLT_B1_CTL,
		      sitar->mbhc_data.v_ins_hu & 0xFF);
	snd_soc_write(codec, SITAR_A_CDC_MBHC_VOLT_B2_CTL,
		      (sitar->mbhc_data.v_ins_hu >> 8) & 0xFF);

	snd_soc_write(codec, SITAR_A_CDC_MBHC_VOLT_B3_CTL,
		      sitar->mbhc_data.v_b1_hu & 0xFF);
	snd_soc_write(codec, SITAR_A_CDC_MBHC_VOLT_B4_CTL,
		      (sitar->mbhc_data.v_b1_hu >> 8) & 0xFF);

	snd_soc_write(codec, SITAR_A_CDC_MBHC_VOLT_B5_CTL,
		      sitar->mbhc_data.v_b1_h & 0xFF);
	snd_soc_write(codec, SITAR_A_CDC_MBHC_VOLT_B6_CTL,
		      (sitar->mbhc_data.v_b1_h >> 8) & 0xFF);

	snd_soc_write(codec, SITAR_A_CDC_MBHC_VOLT_B9_CTL,
		      sitar->mbhc_data.v_brh & 0xFF);
	snd_soc_write(codec, SITAR_A_CDC_MBHC_VOLT_B10_CTL,
		      (sitar->mbhc_data.v_brh >> 8) & 0xFF);

	snd_soc_write(codec, SITAR_A_CDC_MBHC_VOLT_B11_CTL,
		      sitar->mbhc_data.v_brl & 0xFF);
	snd_soc_write(codec, SITAR_A_CDC_MBHC_VOLT_B12_CTL,
		      (sitar->mbhc_data.v_brl >> 8) & 0xFF);

	n_ready = sitar_mbhc_cal_btn_det_mp(btn_det, SITAR_BTN_DET_N_READY);
	snd_soc_write(codec, SITAR_A_CDC_MBHC_TIMER_B1_CTL,
		      n_ready[sitar_codec_mclk_index(sitar)]);
	snd_soc_write(codec, SITAR_A_CDC_MBHC_TIMER_B2_CTL,
		      sitar->mbhc_data.npoll);
	snd_soc_write(codec, SITAR_A_CDC_MBHC_TIMER_B3_CTL,
		      sitar->mbhc_data.nbounce_wait);
	n_cic = sitar_mbhc_cal_btn_det_mp(btn_det, SITAR_BTN_DET_N_CIC);
	snd_soc_write(codec, SITAR_A_CDC_MBHC_TIMER_B6_CTL,
		      n_cic[sitar_codec_mclk_index(sitar)]);
}

static int sitar_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct wcd9xxx *wcd9xxx = dev_get_drvdata(dai->codec->dev->parent);
	if ((wcd9xxx != NULL) && (wcd9xxx->dev != NULL) &&
			(wcd9xxx->dev->parent != NULL))
		pm_runtime_get_sync(wcd9xxx->dev->parent);
	pr_debug("%s(): substream = %s  stream = %d\n" , __func__,
		substream->name, substream->stream);

	return 0;
}

static void sitar_codec_pm_runtime_put(struct wcd9xxx *sitar)
{
	if (sitar->dev != NULL &&
			sitar->dev->parent != NULL) {
		pm_runtime_mark_last_busy(sitar->dev->parent);
		pm_runtime_put(sitar->dev->parent);
	}
}

static void sitar_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct wcd9xxx *sitar_core = dev_get_drvdata(dai->codec->dev->parent);
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(dai->codec);
	u32 active = 0;

	pr_debug("%s(): substream = %s  stream = %d\n" , __func__,
		substream->name, substream->stream);
	if (sitar->intf_type != WCD9XXX_INTERFACE_TYPE_SLIMBUS)
		return;

	if (dai->id <= NUM_CODEC_DAIS) {
		if (sitar->dai[dai->id].ch_mask) {
			active = 1;
			pr_debug("%s(): Codec DAI: chmask[%d] = 0x%lx\n",
				__func__, dai->id,
				sitar->dai[dai->id].ch_mask);
		}
	}

	if (sitar_core != NULL && active == 0)
		sitar_codec_pm_runtime_put(sitar_core);
}

int sitar_mclk_enable(struct snd_soc_codec *codec, int mclk_enable, bool dapm)
{
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s() mclk_enable = %u\n", __func__, mclk_enable);

	if (dapm)
		SITAR_ACQUIRE_LOCK(sitar->codec_resource_lock);
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
			if (dapm)
				SITAR_RELEASE_LOCK(sitar->codec_resource_lock);
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
	if (dapm)
		SITAR_RELEASE_LOCK(sitar->codec_resource_lock);
	return 0;
}

static int sitar_set_dai_sysclk(struct snd_soc_dai *dai,
		int clk_id, unsigned int freq, int dir)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static int sitar_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	u8 val = 0;
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(dai->codec);

	pr_debug("%s\n", __func__);
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
	struct wcd9xxx *core = dev_get_drvdata(dai->codec->dev->parent);
	if (!tx_slot && !rx_slot) {
		pr_err("%s: Invalid\n", __func__);
		return -EINVAL;
	}
	pr_debug("%s: DAI-ID %x %d %d\n", __func__, dai->id, tx_num, rx_num);

	if (sitar->intf_type == WCD9XXX_INTERFACE_TYPE_SLIMBUS)
		wcd9xxx_init_slimslave(core, core->slim->laddr,
			tx_num, tx_slot, rx_num, rx_slot);
	return 0;
}

static int sitar_get_channel_map(struct snd_soc_dai *dai,
			unsigned int *tx_num, unsigned int *tx_slot,
			unsigned int *rx_num, unsigned int *rx_slot)

{
	struct sitar_priv *sitar_p = snd_soc_codec_get_drvdata(dai->codec);
	u32 i = 0;
	struct wcd9xxx_ch *ch;

	switch (dai->id) {
	case AIF1_PB:
		if (!rx_slot || !rx_num) {
			pr_err("%s: Invalid rx_slot 0x%x or rx_num 0x%x\n",
				__func__, (u32) rx_slot, (u32) rx_num);
			return -EINVAL;
		}
		list_for_each_entry(ch, &sitar_p->dai[dai->id].wcd9xxx_ch_list,
				    list) {
			rx_slot[i++] = ch->ch_num;
		}
		*rx_num = i;
		break;
	case AIF1_CAP:
		if (!tx_slot || !tx_num) {
			pr_err("%s: Invalid tx_slot 0x%x or tx_num 0x%x\n",
				__func__, (u32) tx_slot, (u32) tx_num);
			return -EINVAL;
		}
		list_for_each_entry(ch, &sitar_p->dai[dai->id].wcd9xxx_ch_list,
				    list) {
			tx_slot[i++] = ch->ch_num;
		}
		*tx_num = i;
		break;
	default:
		pr_err("%s: Invalid dai %d", __func__, dai->id);
		return -EINVAL;
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
	u32 compander_fs;
	u16 tx_fs_reg, rx_fs_reg;
	u8 tx_fs_rate, rx_fs_rate, rx_state, tx_state;

	pr_debug("%s: DAI-ID %x\n", __func__, dai->id);

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
		rx_fs_rate = 0xa0;
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
				pr_err("%s: Unsupport format %d\n", __func__,
					params_format(params));
				return -EINVAL;
			}
			snd_soc_update_bits(codec, SITAR_A_CDC_CLK_TX_I2S_CTL,
						0x03, tx_fs_rate);
		} else {
			sitar->dai[dai->id].rate   = params_rate(params);
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
				if (comp_rx_path[shift] < COMPANDER_MAX)
					sitar->comp_fs[comp_rx_path[shift]]
						= compander_fs;
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
				pr_err("%s: Unsupport format %d\n", __func__,
					params_format(params));
				break;
			}
			snd_soc_update_bits(codec, SITAR_A_CDC_CLK_RX_I2S_CTL,
						0x03, (rx_fs_rate >> 0x05));
		} else {
			sitar->dai[dai->id].rate   = params_rate(params);
		}
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

static struct snd_soc_dai_driver sitar_i2s_dai[] = {
	{
		.name = "sitar_i2s_rx1",
		.id = AIF1_PB,
		.playback = {
			.stream_name = "AIF1 Playback",
			.rates = WCD9304_RATES,
			.formats = SITAR_FORMATS,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 4,
		},
		.ops = &sitar_dai_ops,
	},
	{
		.name = "sitar_i2s_tx1",
		.id = AIF1_CAP,
		.capture = {
			.stream_name = "AIF1 Capture",
			.rates = WCD9304_RATES,
			.formats = SITAR_FORMATS,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 4,
		},
		.ops = &sitar_dai_ops,
	},
};

static int sitar_codec_enable_chmask(struct sitar_priv *sitar,
	int event, int index)
{
	int  ret = 0;
	struct wcd9xxx_ch *ch;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		list_for_each_entry(ch,
			&sitar->dai[index].wcd9xxx_ch_list, list) {
			ret = wcd9xxx_get_slave_port(ch->ch_num);
			if (ret < 0) {
				pr_err("%s: Invalid slave port ID: %d\n",
					__func__, ret);
				ret = -EINVAL;
				break;
			}
			sitar->dai[index].ch_mask |= 1 << ret;
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		ret = wait_event_timeout(sitar->dai[index].dai_wait,
					(sitar->dai[index].ch_mask == 0),
					 msecs_to_jiffies(SLIM_CLOSE_TIMEOUT));
		if (!ret) {
			pr_err("%s: Slim close tx/rx wait timeout\n",
				__func__);
			ret = -EINVAL;
		} else {
			ret = 0;
		}
		break;
	}
	return ret;
}

static int sitar_codec_enable_slimrx(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct wcd9xxx *core;
	struct snd_soc_codec *codec = w->codec;
	struct sitar_priv *sitar_p = snd_soc_codec_get_drvdata(codec);
	int ret  = 0;
	struct wcd9xxx_codec_dai_data *dai;

	core = dev_get_drvdata(codec->dev->parent);

	/* Execute the callback only if interface type is slimbus */
	if (sitar_p->intf_type != WCD9XXX_INTERFACE_TYPE_SLIMBUS) {
		if (event == SND_SOC_DAPM_POST_PMD && (core != NULL))
			sitar_codec_pm_runtime_put(core);
		return 0;
	}

	dai = &sitar_p->dai[w->shift];

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		ret = sitar_codec_enable_chmask(sitar_p, SND_SOC_DAPM_POST_PMU,
						w->shift);
		ret = wcd9xxx_cfg_slim_sch_rx(core, &dai->wcd9xxx_ch_list,
					      dai->rate, dai->bit_width,
					      &dai->grph);
		break;
	case SND_SOC_DAPM_POST_PMD:
		ret = wcd9xxx_close_slim_sch_rx(core, &dai->wcd9xxx_ch_list,
						dai->grph);
		ret = sitar_codec_enable_chmask(sitar_p, SND_SOC_DAPM_POST_PMD,
						w->shift);
		if (ret < 0) {
			ret = wcd9xxx_disconnect_port(core,
						      &dai->wcd9xxx_ch_list,
						      dai->grph);
				pr_info("%s: Disconnect RX port ret = %d\n",
					__func__, ret);
		}
		if (core != NULL)
			sitar_codec_pm_runtime_put(core);
		break;
	}
	return ret;
}

static int sitar_codec_enable_slimtx(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct wcd9xxx *core;
	struct snd_soc_codec *codec = w->codec;
	struct sitar_priv *sitar_p = snd_soc_codec_get_drvdata(codec);
	struct wcd9xxx_codec_dai_data *dai;
	int ret = 0;

	core = dev_get_drvdata(codec->dev->parent);

	/* Execute the callback only if interface type is slimbus */
	if (sitar_p->intf_type != WCD9XXX_INTERFACE_TYPE_SLIMBUS) {
		if (event == SND_SOC_DAPM_POST_PMD && (core != NULL))
			sitar_codec_pm_runtime_put(core);
		return 0;
	}

	dai = &sitar_p->dai[w->shift];
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		ret = sitar_codec_enable_chmask(sitar_p, SND_SOC_DAPM_POST_PMU,
						w->shift);
		ret = wcd9xxx_cfg_slim_sch_tx(core, &dai->wcd9xxx_ch_list,
					      dai->rate, dai->bit_width,
					      &dai->grph);
		break;
	case SND_SOC_DAPM_POST_PMD:
		ret = wcd9xxx_close_slim_sch_tx(core, &dai->wcd9xxx_ch_list,
						dai->grph);
		ret = sitar_codec_enable_chmask(sitar_p, SND_SOC_DAPM_POST_PMD,
						w->shift);
		if (ret < 0) {
			ret = wcd9xxx_disconnect_port(core,
						      &dai->wcd9xxx_ch_list,
						      dai->grph);
				pr_info("%s: Disconnect RX port ret = %d\n",
					__func__, ret);
		}
		if (core != NULL)
			sitar_codec_pm_runtime_put(core);
		break;
	}
	return ret;
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

static void sitar_turn_onoff_rel_detection(struct snd_soc_codec *codec,
				bool on)
{
	snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_B1_CTL, 0x02, on << 1);
}

static short __sitar_codec_sta_dce(struct snd_soc_codec *codec, int dce,
				   bool override_bypass, bool noreldetection)
{
	short bias_value;
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);
	struct wcd9xxx *core = codec->control_data;
	struct wcd9xxx_core_resource *core_res = &core->core_res;

	wcd9xxx_disable_irq(core_res, WCD9XXX_IRQ_MBHC_POTENTIAL);
	if (noreldetection)
		sitar_turn_onoff_rel_detection(codec, false);

	/* Turn on the override */
	if (!override_bypass)
		snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_B1_CTL, 0x4, 0x4);
	if (dce) {
		snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_CLK_CTL, 0x8, 0x8);
		snd_soc_write(codec, SITAR_A_CDC_MBHC_EN_CTL, 0x4);
		snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_CLK_CTL, 0x8, 0x0);
		usleep_range(sitar->mbhc_data.t_sta_dce,
			     sitar->mbhc_data.t_sta_dce);
		snd_soc_write(codec, SITAR_A_CDC_MBHC_EN_CTL, 0x4);
		usleep_range(sitar->mbhc_data.t_dce,
			     sitar->mbhc_data.t_dce);
		bias_value = sitar_codec_read_dce_result(codec);
	} else {
		snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_CLK_CTL, 0x8, 0x8);
		snd_soc_write(codec, SITAR_A_CDC_MBHC_EN_CTL, 0x2);
		snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_CLK_CTL, 0x8, 0x0);
		usleep_range(sitar->mbhc_data.t_sta_dce,
			     sitar->mbhc_data.t_sta_dce);
		snd_soc_write(codec, SITAR_A_CDC_MBHC_EN_CTL, 0x2);
		usleep_range(sitar->mbhc_data.t_sta,
			     sitar->mbhc_data.t_sta);
		bias_value = sitar_codec_read_sta_result(codec);
		snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_CLK_CTL, 0x8, 0x8);
		snd_soc_write(codec, SITAR_A_CDC_MBHC_EN_CTL, 0x0);
	}
	/* Turn off the override after measuring mic voltage */
	if (!override_bypass)
		snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_B1_CTL, 0x04, 0x00);

	if (noreldetection)
		sitar_turn_onoff_rel_detection(codec, true);
	wcd9xxx_enable_irq(codec->control_data, WCD9XXX_IRQ_MBHC_POTENTIAL);

	return bias_value;
}

static short sitar_codec_sta_dce(struct snd_soc_codec *codec, int dce,
				bool norel)
{
	return __sitar_codec_sta_dce(codec, dce, false, norel);
}

static void sitar_codec_shutdown_hs_removal_detect(struct snd_soc_codec *codec)
{
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);
	const struct sitar_mbhc_general_cfg *generic =
	    SITAR_MBHC_CAL_GENERAL_PTR(sitar->mbhc_cfg.calibration);

	if (!sitar->mclk_enabled && !sitar->mbhc_polling_active)
		sitar_codec_enable_config_mode(codec, 1);

	snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_CLK_CTL, 0x2, 0x2);
	snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_B1_CTL, 0x6, 0x0);

	snd_soc_update_bits(codec, sitar->mbhc_bias_regs.mbhc_reg, 0x80, 0x00);

	usleep_range(generic->t_shutdown_plug_rem,
		     generic->t_shutdown_plug_rem);

	snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_CLK_CTL, 0xA, 0x8);
	if (!sitar->mclk_enabled && !sitar->mbhc_polling_active)
		sitar_codec_enable_config_mode(codec, 0);

	snd_soc_write(codec, SITAR_A_MBHC_SCALING_MUX_1, 0x00);
}

static void sitar_codec_cleanup_hs_polling(struct snd_soc_codec *codec)
{
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);

	sitar_codec_shutdown_hs_removal_detect(codec);

	if (!sitar->mclk_enabled) {
		sitar_codec_disable_clock_block(codec);
		sitar_codec_enable_bandgap(codec, SITAR_BANDGAP_OFF);
	}

	sitar->mbhc_polling_active = false;
	sitar->mbhc_state = MBHC_STATE_NONE;
}

/* called only from interrupt which is under codec_resource_lock acquisition */
static short sitar_codec_setup_hs_polling(struct snd_soc_codec *codec)
{
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);
	short bias_value;
	u8 cfilt_mode;

	if (!sitar->mbhc_cfg.calibration) {
		pr_err("Error, no sitar calibration\n");
		return -ENODEV;
	}

	if (!sitar->mclk_enabled) {
		sitar_codec_enable_bandgap(codec, SITAR_BANDGAP_MBHC_MODE);
		sitar_enable_rx_bias(codec, 1);
		sitar_codec_enable_clock_block(codec, 1);
	}

	snd_soc_update_bits(codec, SITAR_A_CLK_BUFF_EN1, 0x05, 0x01);

	/* Make sure CFILT is in fast mode, save current mode */
	cfilt_mode = snd_soc_read(codec, sitar->mbhc_bias_regs.cfilt_ctl);
	snd_soc_update_bits(codec, sitar->mbhc_bias_regs.cfilt_ctl, 0x70, 0x00);

	snd_soc_update_bits(codec, sitar->mbhc_bias_regs.ctl_reg, 0x1F, 0x16);

	snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_CLK_CTL, 0x2, 0x2);
	snd_soc_write(codec, SITAR_A_MBHC_SCALING_MUX_1, 0x84);

	snd_soc_update_bits(codec, SITAR_A_TX_4_MBHC_EN, 0x80, 0x80);
	snd_soc_update_bits(codec, SITAR_A_TX_4_MBHC_EN, 0x1F, 0x1C);
	snd_soc_update_bits(codec, SITAR_A_TX_4_MBHC_TEST_CTL, 0x40, 0x40);

	snd_soc_update_bits(codec, SITAR_A_TX_4_MBHC_EN, 0x80, 0x00);
	snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_CLK_CTL, 0x8, 0x8);
	snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_CLK_CTL, 0x8, 0x00);

	snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_B1_CTL, 0x2, 0x2);
	snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_CLK_CTL, 0x8, 0x8);

	sitar_codec_calibrate_hs_polling(codec);

	/* don't flip override */
	bias_value = __sitar_codec_sta_dce(codec, 1, true, true);
	snd_soc_update_bits(codec, sitar->mbhc_bias_regs.cfilt_ctl, 0x40,
			    cfilt_mode);
	snd_soc_update_bits(codec, SITAR_A_MBHC_HPH, 0x13, 0x00);

	return bias_value;
}

static int sitar_cancel_btn_work(struct sitar_priv *sitar)
{
	int r = 0;
	struct wcd9xxx *core = dev_get_drvdata(sitar->codec->dev->parent);
	struct wcd9xxx_core_resource *core_res = &core->core_res;

	if (cancel_delayed_work_sync(&sitar->mbhc_btn_dwork)) {
		/* if scheduled mbhc_btn_dwork is canceled from here,
		 * we have to unlock from here instead btn_work */
		wcd9xxx_unlock_sleep(core_res);
		r = 1;
	}
	return r;
}


static u16 sitar_codec_v_sta_dce(struct snd_soc_codec *codec, bool dce,
				 s16 vin_mv)
{
	short diff, zero;
	struct sitar_priv *sitar;
	u32 mb_mv, in;

	sitar = snd_soc_codec_get_drvdata(codec);
	mb_mv = sitar->mbhc_data.micb_mv;

	if (mb_mv == 0) {
		pr_err("%s: Mic Bias voltage is set to zero\n", __func__);
		return -EINVAL;
	}

	if (dce) {
		diff = sitar->mbhc_data.dce_mb - sitar->mbhc_data.dce_z;
		zero = sitar->mbhc_data.dce_z;
	} else {
		diff = sitar->mbhc_data.sta_mb - sitar->mbhc_data.sta_z;
		zero = sitar->mbhc_data.sta_z;
	}
	in = (u32) diff * vin_mv;

	return (u16) (in / mb_mv) + zero;
}

static s32 sitar_codec_sta_dce_v(struct snd_soc_codec *codec, s8 dce,
				 u16 bias_value)
{
	struct sitar_priv *sitar;
	s16 value, z, mb;
	s32 mv;

	sitar = snd_soc_codec_get_drvdata(codec);
	value = bias_value;

	if (dce) {
		z = (sitar->mbhc_data.dce_z);
		mb = (sitar->mbhc_data.dce_mb);
		mv = (value - z) * (s32)sitar->mbhc_data.micb_mv / (mb - z);
	} else {
		z = (sitar->mbhc_data.sta_z);
		mb = (sitar->mbhc_data.sta_mb);
		mv = (value - z) * (s32)sitar->mbhc_data.micb_mv / (mb - z);
	}

	return mv;
}

static void btn_lpress_fn(struct work_struct *work)
{
	struct delayed_work *delayed_work;
	struct sitar_priv *sitar;
	short bias_value;
	int dce_mv, sta_mv;
	struct wcd9xxx *core;
	struct wcd9xxx_core_resource *core_res;

	pr_debug("%s:\n", __func__);

	delayed_work = to_delayed_work(work);
	sitar = container_of(delayed_work, struct sitar_priv, mbhc_btn_dwork);
	core = dev_get_drvdata(sitar->codec->dev->parent);
	core_res = &core->core_res;

	if (sitar) {
		if (sitar->mbhc_cfg.button_jack) {
			bias_value = sitar_codec_read_sta_result(sitar->codec);
			sta_mv = sitar_codec_sta_dce_v(sitar->codec, 0,
							bias_value);
			bias_value = sitar_codec_read_dce_result(sitar->codec);
			dce_mv = sitar_codec_sta_dce_v(sitar->codec, 1,
							bias_value);
			pr_debug("%s: Reporting long button press event"
			" STA: %d, DCE: %d\n", __func__, sta_mv, dce_mv);
			sitar_snd_soc_jack_report(sitar,
						sitar->mbhc_cfg.button_jack,
						sitar->buttons_pressed,
						sitar->buttons_pressed);
		}
	} else {
		pr_err("%s: Bad sitar private data\n", __func__);
	}

	pr_debug("%s: leave\n", __func__);
	wcd9xxx_unlock_sleep(core_res);
}


void sitar_mbhc_cal(struct snd_soc_codec *codec)
{
	struct sitar_priv *sitar;
	struct sitar_mbhc_btn_detect_cfg *btn_det;
	u8 cfilt_mode, bg_mode;
	u8 ncic, nmeas, navg;
	u32 mclk_rate;
	u32 dce_wait, sta_wait;
	u8 *n_cic;
	void *calibration;
	struct wcd9xxx *core = codec->control_data;
	struct wcd9xxx_core_resource *core_res = &core->core_res;

	sitar = snd_soc_codec_get_drvdata(codec);
	calibration = sitar->mbhc_cfg.calibration;

	wcd9xxx_disable_irq(core_res, WCD9XXX_IRQ_MBHC_POTENTIAL);
	sitar_turn_onoff_rel_detection(codec, false);

	/* First compute the DCE / STA wait times
	 * depending on tunable parameters.
	 * The value is computed in microseconds
	 */
	btn_det = SITAR_MBHC_CAL_BTN_DET_PTR(calibration);
	n_cic = sitar_mbhc_cal_btn_det_mp(btn_det, SITAR_BTN_DET_N_CIC);
	ncic = n_cic[sitar_codec_mclk_index(sitar)];
	nmeas = SITAR_MBHC_CAL_BTN_DET_PTR(calibration)->n_meas;
	navg = SITAR_MBHC_CAL_GENERAL_PTR(calibration)->mbhc_navg;
	mclk_rate = sitar->mbhc_cfg.mclk_rate;
	dce_wait = (1000 * 512 * 60 * (nmeas + 1)) / (mclk_rate / 1000);
	sta_wait = (1000 * 128 * (navg + 1)) / (mclk_rate / 1000);

	sitar->mbhc_data.t_dce = DEFAULT_DCE_WAIT;
	sitar->mbhc_data.t_sta = DEFAULT_STA_WAIT;

	/* LDOH and CFILT are already configured during pdata handling.
	 * Only need to make sure CFILT and bandgap are in Fast mode.
	 * Need to restore defaults once calculation is done.
	 */
	cfilt_mode = snd_soc_read(codec, sitar->mbhc_bias_regs.cfilt_ctl);
	snd_soc_update_bits(codec, sitar->mbhc_bias_regs.cfilt_ctl, 0x40, 0x00);
	bg_mode = snd_soc_update_bits(codec, SITAR_A_BIAS_CENTRAL_BG_CTL, 0x02,
				      0x02);

	/* Micbias, CFILT, LDOH, MBHC MUX mode settings
	 * to perform ADC calibration
	 */
	snd_soc_update_bits(codec, sitar->mbhc_bias_regs.ctl_reg, 0x60,
			    sitar->mbhc_cfg.micbias << 5);
	snd_soc_update_bits(codec, sitar->mbhc_bias_regs.ctl_reg, 0x01, 0x00);
	snd_soc_update_bits(codec, SITAR_A_LDO_H_MODE_1, 0x60, 0x60);
	snd_soc_write(codec, SITAR_A_TX_4_MBHC_TEST_CTL, 0x78);
	snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_B1_CTL, 0x04, 0x04);

	/* DCE measurement for 0 volts */
	snd_soc_write(codec, SITAR_A_CDC_MBHC_CLK_CTL, 0x0A);
	snd_soc_write(codec, SITAR_A_CDC_MBHC_EN_CTL, 0x04);
	snd_soc_write(codec, SITAR_A_CDC_MBHC_CLK_CTL, 0x02);
	snd_soc_write(codec, SITAR_A_MBHC_SCALING_MUX_1, 0x81);
	usleep_range(100, 100);
	snd_soc_write(codec, SITAR_A_CDC_MBHC_EN_CTL, 0x04);
	usleep_range(sitar->mbhc_data.t_dce, sitar->mbhc_data.t_dce);
	sitar->mbhc_data.dce_z = sitar_codec_read_dce_result(codec);

	/* DCE measurment for MB voltage */
	snd_soc_write(codec, SITAR_A_CDC_MBHC_CLK_CTL, 0x0A);
	snd_soc_write(codec, SITAR_A_CDC_MBHC_CLK_CTL, 0x02);
	snd_soc_write(codec, SITAR_A_MBHC_SCALING_MUX_1, 0x82);
	usleep_range(100, 100);
	snd_soc_write(codec, SITAR_A_CDC_MBHC_EN_CTL, 0x04);
	usleep_range(sitar->mbhc_data.t_dce, sitar->mbhc_data.t_dce);
	sitar->mbhc_data.dce_mb = sitar_codec_read_dce_result(codec);

	/* Sta measuremnt for 0 volts */
	snd_soc_write(codec, SITAR_A_CDC_MBHC_CLK_CTL, 0x0A);
	snd_soc_write(codec, SITAR_A_CDC_MBHC_EN_CTL, 0x02);
	snd_soc_write(codec, SITAR_A_CDC_MBHC_CLK_CTL, 0x02);
	snd_soc_write(codec, SITAR_A_MBHC_SCALING_MUX_1, 0x81);
	usleep_range(100, 100);
	snd_soc_write(codec, SITAR_A_CDC_MBHC_EN_CTL, 0x02);
	usleep_range(sitar->mbhc_data.t_sta, sitar->mbhc_data.t_sta);
	sitar->mbhc_data.sta_z = sitar_codec_read_sta_result(codec);

	/* STA Measurement for MB Voltage */
	snd_soc_write(codec, SITAR_A_MBHC_SCALING_MUX_1, 0x82);
	usleep_range(100, 100);
	snd_soc_write(codec, SITAR_A_CDC_MBHC_EN_CTL, 0x02);
	usleep_range(sitar->mbhc_data.t_sta, sitar->mbhc_data.t_sta);
	sitar->mbhc_data.sta_mb = sitar_codec_read_sta_result(codec);

	/* Restore default settings. */
	snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_B1_CTL, 0x04, 0x00);
	snd_soc_update_bits(codec, sitar->mbhc_bias_regs.cfilt_ctl, 0x40,
			    cfilt_mode);
	snd_soc_update_bits(codec, SITAR_A_BIAS_CENTRAL_BG_CTL, 0x02, bg_mode);

	snd_soc_write(codec, SITAR_A_MBHC_SCALING_MUX_1, 0x84);
	usleep_range(100, 100);

	wcd9xxx_enable_irq(codec->control_data, WCD9XXX_IRQ_MBHC_POTENTIAL);
	sitar_turn_onoff_rel_detection(codec, true);
}

void *sitar_mbhc_cal_btn_det_mp(const struct sitar_mbhc_btn_detect_cfg* btn_det,
				const enum sitar_mbhc_btn_det_mem mem)
{
	void *ret = &btn_det->_v_btn_low;

	switch (mem) {
	case SITAR_BTN_DET_GAIN:
		ret += sizeof(btn_det->_n_cic);
	case SITAR_BTN_DET_N_CIC:
		ret += sizeof(btn_det->_n_ready);
	case SITAR_BTN_DET_N_READY:
		ret += sizeof(btn_det->_v_btn_high[0]) * btn_det->num_btn;
	case SITAR_BTN_DET_V_BTN_HIGH:
		ret += sizeof(btn_det->_v_btn_low[0]) * btn_det->num_btn;
	case SITAR_BTN_DET_V_BTN_LOW:
		/* do nothing */
		break;
	default:
		ret = NULL;
	}

	return ret;
}

static void sitar_mbhc_calc_thres(struct snd_soc_codec *codec)
{
	struct sitar_priv *sitar;
	s16 btn_mv = 0, btn_delta_mv;
	struct sitar_mbhc_btn_detect_cfg *btn_det;
	struct sitar_mbhc_plug_type_cfg *plug_type;
	u16 *btn_high;
	u8 *n_ready;
	int i;

	sitar = snd_soc_codec_get_drvdata(codec);
	btn_det = SITAR_MBHC_CAL_BTN_DET_PTR(sitar->mbhc_cfg.calibration);
	plug_type = SITAR_MBHC_CAL_PLUG_TYPE_PTR(sitar->mbhc_cfg.calibration);

	n_ready = sitar_mbhc_cal_btn_det_mp(btn_det, SITAR_BTN_DET_N_READY);
	if (sitar->mbhc_cfg.mclk_rate == SITAR_MCLK_RATE_12288KHZ) {
		sitar->mbhc_data.npoll = 9;
		sitar->mbhc_data.nbounce_wait = 30;
	} else if (sitar->mbhc_cfg.mclk_rate == SITAR_MCLK_RATE_9600KHZ) {
		sitar->mbhc_data.npoll = 7;
		sitar->mbhc_data.nbounce_wait = 23;
	}

	sitar->mbhc_data.t_sta_dce = ((1000 * 256) /
				(sitar->mbhc_cfg.mclk_rate / 1000) *
				n_ready[sitar_codec_mclk_index(sitar)]) +
				10;
	sitar->mbhc_data.v_ins_hu =
	    sitar_codec_v_sta_dce(codec, STA, plug_type->v_hs_max);
	sitar->mbhc_data.v_ins_h =
	    sitar_codec_v_sta_dce(codec, DCE, plug_type->v_hs_max);

	btn_high = sitar_mbhc_cal_btn_det_mp(btn_det, SITAR_BTN_DET_V_BTN_HIGH);
	for (i = 0; i < btn_det->num_btn; i++)
		btn_mv = btn_high[i] > btn_mv ? btn_high[i] : btn_mv;

	sitar->mbhc_data.v_b1_h = sitar_codec_v_sta_dce(codec, DCE, btn_mv);
	btn_delta_mv = btn_mv + btn_det->v_btn_press_delta_sta;

	sitar->mbhc_data.v_b1_hu =
	    sitar_codec_v_sta_dce(codec, STA, btn_delta_mv);

	btn_delta_mv = btn_mv + btn_det->v_btn_press_delta_cic;

	sitar->mbhc_data.v_b1_huc =
	    sitar_codec_v_sta_dce(codec, DCE, btn_delta_mv);

	sitar->mbhc_data.v_brh = sitar->mbhc_data.v_b1_h;
	sitar->mbhc_data.v_brl = SITAR_MBHC_BUTTON_MIN;

	sitar->mbhc_data.v_no_mic =
	    sitar_codec_v_sta_dce(codec, STA, plug_type->v_no_mic);
}

void sitar_mbhc_init(struct snd_soc_codec *codec)
{
	struct sitar_priv *sitar;
	struct sitar_mbhc_general_cfg *generic;
	struct sitar_mbhc_btn_detect_cfg *btn_det;
	int n;
	u8 *n_cic, *gain;

	pr_err("%s(): ENTER\n", __func__);
	sitar = snd_soc_codec_get_drvdata(codec);
	generic = SITAR_MBHC_CAL_GENERAL_PTR(sitar->mbhc_cfg.calibration);
	btn_det = SITAR_MBHC_CAL_BTN_DET_PTR(sitar->mbhc_cfg.calibration);

	for (n = 0; n < 8; n++) {
		if (n != 7) {
			snd_soc_update_bits(codec,
					    SITAR_A_CDC_MBHC_FIR_B1_CFG,
					    0x07, n);
			snd_soc_write(codec, SITAR_A_CDC_MBHC_FIR_B2_CFG,
				      btn_det->c[n]);
		}
	}
	snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_B2_CTL, 0x07,
			    btn_det->nc);

	n_cic = sitar_mbhc_cal_btn_det_mp(btn_det, SITAR_BTN_DET_N_CIC);
	snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_TIMER_B6_CTL, 0xFF,
			    n_cic[sitar_codec_mclk_index(sitar)]);

	gain = sitar_mbhc_cal_btn_det_mp(btn_det, SITAR_BTN_DET_GAIN);
	snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_B2_CTL, 0x78,
			    gain[sitar_codec_mclk_index(sitar)] << 3);

	snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_TIMER_B4_CTL, 0x70,
			    generic->mbhc_nsa << 4);

	snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_TIMER_B4_CTL, 0x0F,
			    btn_det->n_meas);

	snd_soc_write(codec, SITAR_A_CDC_MBHC_TIMER_B5_CTL, generic->mbhc_navg);

	snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_B1_CTL, 0x80, 0x80);

	snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_B1_CTL, 0x78,
			    btn_det->mbhc_nsc << 3);

	snd_soc_update_bits(codec, SITAR_A_MICB_1_MBHC, 0x03,
						sitar->mbhc_cfg.micbias);

	snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_B1_CTL, 0x02, 0x02);

	snd_soc_update_bits(codec, SITAR_A_MBHC_SCALING_MUX_2, 0xF0, 0xF0);

}

static bool sitar_mbhc_fw_validate(const struct firmware *fw)
{
	u32 cfg_offset;
	struct sitar_mbhc_imped_detect_cfg *imped_cfg;
	struct sitar_mbhc_btn_detect_cfg *btn_cfg;

	if (fw->size < SITAR_MBHC_CAL_MIN_SIZE)
		return false;

	/* previous check guarantees that there is enough fw data up
	 * to num_btn
	 */
	btn_cfg = SITAR_MBHC_CAL_BTN_DET_PTR(fw->data);
	cfg_offset = (u32) ((void *) btn_cfg - (void *) fw->data);
	if (fw->size < (cfg_offset + SITAR_MBHC_CAL_BTN_SZ(btn_cfg)))
		return false;

	/* previous check guarantees that there is enough fw data up
	 * to start of impedance detection configuration
	 */
	imped_cfg = SITAR_MBHC_CAL_IMPED_DET_PTR(fw->data);
	cfg_offset = (u32) ((void *) imped_cfg - (void *) fw->data);

	if (fw->size < (cfg_offset + SITAR_MBHC_CAL_IMPED_MIN_SZ))
		return false;

	if (fw->size < (cfg_offset + SITAR_MBHC_CAL_IMPED_SZ(imped_cfg)))
		return false;

	return true;
}


static void sitar_turn_onoff_override(struct snd_soc_codec *codec, bool on)
{
	snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_B1_CTL, 0x04, on << 2);
}

/* called under codec_resource_lock acquisition */
void sitar_set_and_turnoff_hph_padac(struct snd_soc_codec *codec)
{
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);
	u8 wg_time;

	wg_time = snd_soc_read(codec, SITAR_A_RX_HPH_CNP_WG_TIME) ;
	wg_time += 1;

	/* If headphone PA is on, check if userspace receives
	 * removal event to sync-up PA's state */
	if (sitar_is_hph_pa_on(codec)) {
		pr_debug("%s PA is on, setting PA_OFF_ACK\n", __func__);
		set_bit(SITAR_HPHL_PA_OFF_ACK, &sitar->hph_pa_dac_state);
		set_bit(SITAR_HPHR_PA_OFF_ACK, &sitar->hph_pa_dac_state);
	} else {
		pr_debug("%s PA is off\n", __func__);
	}

	if (sitar_is_hph_dac_on(codec, 1))
		set_bit(SITAR_HPHL_DAC_OFF_ACK, &sitar->hph_pa_dac_state);
	if (sitar_is_hph_dac_on(codec, 0))
		set_bit(SITAR_HPHR_DAC_OFF_ACK, &sitar->hph_pa_dac_state);

	snd_soc_update_bits(codec, SITAR_A_RX_HPH_CNP_EN, 0x30, 0x00);
	snd_soc_update_bits(codec, SITAR_A_RX_HPH_L_DAC_CTL,
			    0xC0, 0x00);
	snd_soc_update_bits(codec, SITAR_A_RX_HPH_R_DAC_CTL,
			    0xC0, 0x00);
	usleep_range(wg_time * 1000, wg_time * 1000);
}

static void sitar_clr_and_turnon_hph_padac(struct sitar_priv *sitar)
{
	bool pa_turned_on = false;
	struct snd_soc_codec *codec = sitar->codec;
	u8 wg_time;

	wg_time = snd_soc_read(codec, SITAR_A_RX_HPH_CNP_WG_TIME) ;
	wg_time += 1;

	if (test_and_clear_bit(SITAR_HPHR_DAC_OFF_ACK,
			       &sitar->hph_pa_dac_state)) {
		pr_debug("%s: HPHR clear flag and enable DAC\n", __func__);
		snd_soc_update_bits(sitar->codec, SITAR_A_RX_HPH_R_DAC_CTL,
				    0xC0, 0xC0);
	}
	if (test_and_clear_bit(SITAR_HPHL_DAC_OFF_ACK,
			       &sitar->hph_pa_dac_state)) {
		pr_debug("%s: HPHL clear flag and enable DAC\n", __func__);
		snd_soc_update_bits(sitar->codec, SITAR_A_RX_HPH_L_DAC_CTL,
				    0xC0, 0xC0);
	}

	if (test_and_clear_bit(SITAR_HPHR_PA_OFF_ACK,
			       &sitar->hph_pa_dac_state)) {
		pr_debug("%s: HPHR clear flag and enable PA\n", __func__);
		snd_soc_update_bits(sitar->codec, SITAR_A_RX_HPH_CNP_EN, 0x10,
				    1 << 4);
		pa_turned_on = true;
	}
	if (test_and_clear_bit(SITAR_HPHL_PA_OFF_ACK,
			       &sitar->hph_pa_dac_state)) {
		pr_debug("%s: HPHL clear flag and enable PA\n", __func__);
		snd_soc_update_bits(sitar->codec, SITAR_A_RX_HPH_CNP_EN, 0x20,
				    1 << 5);
		pa_turned_on = true;
	}

	if (pa_turned_on) {
		pr_debug("%s: PA was turned off by MBHC and not by DAPM\n",
				__func__);
		usleep_range(wg_time * 1000, wg_time * 1000);
	}
}

static void sitar_codec_report_plug(struct snd_soc_codec *codec, int insertion,
				    enum snd_jack_types jack_type)
{
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);

	if (!insertion) {
		/* Report removal */
		sitar->hph_status &= ~jack_type;
		if (sitar->mbhc_cfg.headset_jack) {
			/* cancel possibly scheduled btn work and
			* report release if we reported button press */
			if (sitar_cancel_btn_work(sitar)) {
				pr_debug("%s: button press is canceled\n",
					__func__);
			} else if (sitar->buttons_pressed) {
				pr_debug("%s: Reporting release for reported "
					 "button press %d\n", __func__,
					 jack_type);
				sitar_snd_soc_jack_report(sitar,
						 sitar->mbhc_cfg.button_jack, 0,
						 sitar->buttons_pressed);
				sitar->buttons_pressed &=
							~SITAR_JACK_BUTTON_MASK;
			}
			pr_debug("%s: Reporting removal %d\n", __func__,
				 jack_type);
			sitar_snd_soc_jack_report(sitar,
						  sitar->mbhc_cfg.headset_jack,
						  sitar->hph_status,
						  SITAR_JACK_MASK);
		}
		sitar_set_and_turnoff_hph_padac(codec);
		hphocp_off_report(sitar, SND_JACK_OC_HPHR,
				  WCD9XXX_IRQ_HPH_PA_OCPR_FAULT);
		hphocp_off_report(sitar, SND_JACK_OC_HPHL,
				  WCD9XXX_IRQ_HPH_PA_OCPL_FAULT);
		sitar->current_plug = PLUG_TYPE_NONE;
		sitar->mbhc_polling_active = false;
	} else {
		/* Report insertion */
		sitar->hph_status |= jack_type;

		if (jack_type == SND_JACK_HEADPHONE)
			sitar->current_plug = PLUG_TYPE_HEADPHONE;
		else if (jack_type == SND_JACK_HEADSET) {
			sitar->mbhc_polling_active = true;
			sitar->current_plug = PLUG_TYPE_HEADSET;
		}
		if (sitar->mbhc_cfg.headset_jack) {
			pr_debug("%s: Reporting insertion %d\n", __func__,
				 jack_type);
			sitar_snd_soc_jack_report(sitar,
						  sitar->mbhc_cfg.headset_jack,
						  sitar->hph_status,
						  SITAR_JACK_MASK);
		}
		sitar_clr_and_turnon_hph_padac(sitar);
	}
}


static bool sitar_hs_gpio_level_remove(struct sitar_priv *sitar)
{
	return (gpio_get_value_cansleep(sitar->mbhc_cfg.gpio) !=
		sitar->mbhc_cfg.gpio_level_insert);
}

static bool sitar_is_invalid_insert_delta(struct snd_soc_codec *codec,
					int mic_volt, int mic_volt_prev)
{
	int delta = abs(mic_volt - mic_volt_prev);
	if (delta > SITAR_MBHC_FAKE_INSERT_VOLT_DELTA_MV) {
		pr_debug("%s: volt delta %dmv\n", __func__, delta);
		return true;
	}
	return false;
}

static bool sitar_is_invalid_insertion_range(struct snd_soc_codec *codec,
				       s32 mic_volt)
{
	bool invalid = false;

	if (mic_volt < SITAR_MBHC_FAKE_INSERT_HIGH
			&& (mic_volt > SITAR_MBHC_FAKE_INSERT_LOW)) {
		invalid = true;
	}

	return invalid;
}

static bool sitar_codec_is_invalid_plug(struct snd_soc_codec *codec,
	s32 mic_mv[MBHC_NUM_DCE_PLUG_DETECT],
	enum sitar_mbhc_plug_type plug_type[MBHC_NUM_DCE_PLUG_DETECT])
{
	int i;
	bool r = false;
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);
	struct sitar_mbhc_plug_type_cfg *plug_type_ptr =
		SITAR_MBHC_CAL_PLUG_TYPE_PTR(sitar->mbhc_cfg.calibration);

	for (i = 0 ; i < MBHC_NUM_DCE_PLUG_DETECT && !r; i++) {
		if (mic_mv[i] < plug_type_ptr->v_no_mic)
			plug_type[i] = PLUG_TYPE_HEADPHONE;
		else if (mic_mv[i] < plug_type_ptr->v_hs_max)
			plug_type[i] = PLUG_TYPE_HEADSET;
		else if (mic_mv[i] > plug_type_ptr->v_hs_max)
			plug_type[i] = PLUG_TYPE_HIGH_HPH;

		r = sitar_is_invalid_insertion_range(codec, mic_mv[i]);
		if (!r && i > 0) {
			if (plug_type[i-1] != plug_type[i])
				r = true;
			else
				r = sitar_is_invalid_insert_delta(codec,
							mic_mv[i],
							mic_mv[i - 1]);
		}
	}

	return r;
}

/* called under codec_resource_lock acquisition */
void sitar_find_plug_and_report(struct snd_soc_codec *codec,
				enum sitar_mbhc_plug_type plug_type)
{
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);

	if (plug_type == PLUG_TYPE_HEADPHONE
		&& sitar->current_plug == PLUG_TYPE_NONE) {
		/* Nothing was reported previously
		 * reporte a headphone
		 */
		sitar_codec_report_plug(codec, 1, SND_JACK_HEADPHONE);
		sitar_codec_cleanup_hs_polling(codec);
	} else if (plug_type == PLUG_TYPE_HEADSET) {
		/* If Headphone was reported previously, this will
		 * only report the mic line
		 */
		sitar_codec_report_plug(codec, 1, SND_JACK_HEADSET);
		msleep(100);
		sitar_codec_start_hs_polling(codec);
	} else if (plug_type == PLUG_TYPE_HIGH_HPH) {
		if (sitar->current_plug == PLUG_TYPE_NONE)
			sitar_codec_report_plug(codec, 1, SND_JACK_HEADPHONE);
		sitar_codec_cleanup_hs_polling(codec);
		pr_debug("setup mic trigger for further detection\n");
		sitar->lpi_enabled = true;
		/* TODO ::: sitar_codec_enable_hs_detect */
		pr_err("%s(): High impedence hph not supported\n", __func__);
	}
}

/* should be called under interrupt context that hold suspend */
static void sitar_schedule_hs_detect_plug(struct sitar_priv *sitar)
{
	struct wcd9xxx *core = sitar->codec->control_data;
	struct wcd9xxx_core_resource *core_res = &core->core_res;

	pr_debug("%s: scheduling sitar_hs_correct_gpio_plug\n", __func__);
	sitar->hs_detect_work_stop = false;
	wcd9xxx_lock_sleep(core_res);
	schedule_work(&sitar->hs_correct_plug_work);
}

/* called under codec_resource_lock acquisition */
static void sitar_cancel_hs_detect_plug(struct sitar_priv *sitar)
{
	struct wcd9xxx *core = sitar->codec->control_data;
	struct wcd9xxx_core_resource *core_res = &core->core_res;

	pr_debug("%s: canceling hs_correct_plug_work\n", __func__);
	sitar->hs_detect_work_stop = true;
	wmb();
	SITAR_RELEASE_LOCK(sitar->codec_resource_lock);
	if (cancel_work_sync(&sitar->hs_correct_plug_work)) {
		pr_debug("%s: hs_correct_plug_work is canceled\n", __func__);
		wcd9xxx_unlock_sleep(core_res);
	}
	SITAR_ACQUIRE_LOCK(sitar->codec_resource_lock);
}

static void sitar_hs_correct_gpio_plug(struct work_struct *work)
{
	struct sitar_priv *sitar;
	struct snd_soc_codec *codec;
	int retry = 0, i;
	bool correction = false;
	s32 mic_mv[MBHC_NUM_DCE_PLUG_DETECT];
	short mb_v[MBHC_NUM_DCE_PLUG_DETECT];
	enum sitar_mbhc_plug_type plug_type[MBHC_NUM_DCE_PLUG_DETECT];
	unsigned long timeout;
	struct wcd9xxx *core;
	struct wcd9xxx_core_resource *core_res;

	sitar = container_of(work, struct sitar_priv, hs_correct_plug_work);
	codec = sitar->codec;
	core = sitar->codec->control_data;
	core_res = &core->core_res;

	pr_debug("%s: enter\n", __func__);
	sitar->mbhc_cfg.mclk_cb_fn(codec, 1, false);

	/* Keep override on during entire plug type correction work.
	 *
	 * This is okay under the assumption that any GPIO irqs which use
	 * MBHC block cancel and sync this work so override is off again
	 * prior to GPIO interrupt handler's MBHC block usage.
	 * Also while this correction work is running, we can guarantee
	 * DAPM doesn't use any MBHC block as this work only runs with
	 * headphone detection.
	 */
	sitar_turn_onoff_override(codec, true);

	timeout = jiffies + msecs_to_jiffies(SITAR_HS_DETECT_PLUG_TIME_MS);
	while (!time_after(jiffies, timeout)) {
		++retry;
		rmb();
		if (sitar->hs_detect_work_stop) {
			pr_debug("%s: stop requested\n", __func__);
			break;
		}

		msleep(SITAR_HS_DETECT_PLUG_INERVAL_MS);
		if (sitar_hs_gpio_level_remove(sitar)) {
			pr_debug("%s: GPIO value is low\n", __func__);
			break;
		}

		/* can race with removal interrupt */
		SITAR_ACQUIRE_LOCK(sitar->codec_resource_lock);
		for (i = 0; i < MBHC_NUM_DCE_PLUG_DETECT; i++) {
			mb_v[i] = __sitar_codec_sta_dce(codec, 1, true, true);
			mic_mv[i] = sitar_codec_sta_dce_v(codec, 1 , mb_v[i]);
			pr_debug("%s : DCE run %d, mic_mv = %d(%x)\n",
				 __func__, retry, mic_mv[i], mb_v[i]);
		}
		SITAR_RELEASE_LOCK(sitar->codec_resource_lock);

		if (sitar_codec_is_invalid_plug(codec, mic_mv, plug_type)) {
			pr_debug("Invalid plug in attempt # %d\n", retry);
			if (retry == NUM_ATTEMPTS_TO_REPORT &&
			    sitar->current_plug == PLUG_TYPE_NONE) {
				sitar_codec_report_plug(codec, 1,
							SND_JACK_HEADPHONE);
			}
		} else if (!sitar_codec_is_invalid_plug(codec, mic_mv,
							plug_type) &&
			   plug_type[0] == PLUG_TYPE_HEADPHONE) {
			pr_debug("Good headphone detected, continue polling mic\n");
			if (sitar->current_plug == PLUG_TYPE_NONE) {
				sitar_codec_report_plug(codec, 1,
							SND_JACK_HEADPHONE);
			}
		} else {
			SITAR_ACQUIRE_LOCK(sitar->codec_resource_lock);
			/* Turn off override */
			sitar_turn_onoff_override(codec, false);
			sitar_find_plug_and_report(codec, plug_type[0]);
			SITAR_RELEASE_LOCK(sitar->codec_resource_lock);
			pr_debug("Attempt %d found correct plug %d\n", retry,
				 plug_type[0]);
			correction = true;
			break;
		}
	}

	/* Turn off override */
	if (!correction)
		sitar_turn_onoff_override(codec, false);

	sitar->mbhc_cfg.mclk_cb_fn(codec, 0, false);
	pr_debug("%s: leave\n", __func__);
	/* unlock sleep */
	wcd9xxx_unlock_sleep(core_res);
}

/* called under codec_resource_lock acquisition */
static void sitar_codec_decide_gpio_plug(struct snd_soc_codec *codec)
{
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);
	short mb_v[MBHC_NUM_DCE_PLUG_DETECT];
	s32 mic_mv[MBHC_NUM_DCE_PLUG_DETECT];
	enum sitar_mbhc_plug_type plug_type[MBHC_NUM_DCE_PLUG_DETECT];
	int i;

	pr_debug("%s: enter\n", __func__);

	sitar_turn_onoff_override(codec, true);
	mb_v[0] = sitar_codec_setup_hs_polling(codec);
	mic_mv[0] = sitar_codec_sta_dce_v(codec, 1, mb_v[0]);
	pr_debug("%s: DCE run 1, mic_mv = %d\n", __func__, mic_mv[0]);

	for (i = 1; i < MBHC_NUM_DCE_PLUG_DETECT; i++) {
		mb_v[i] = __sitar_codec_sta_dce(codec, 1, true, true);
		mic_mv[i] = sitar_codec_sta_dce_v(codec, 1 , mb_v[i]);
		pr_debug("%s: DCE run %d, mic_mv = %d\n", __func__, i + 1,
			 mic_mv[i]);
	}
	sitar_turn_onoff_override(codec, false);

	if (sitar_hs_gpio_level_remove(sitar)) {
		pr_debug("%s: GPIO value is low when determining plug\n",
			 __func__);
		return;
	}

	if (sitar_codec_is_invalid_plug(codec, mic_mv, plug_type)) {
		sitar_schedule_hs_detect_plug(sitar);
	} else if (plug_type[0] == PLUG_TYPE_HEADPHONE) {
		sitar_codec_report_plug(codec, 1, SND_JACK_HEADPHONE);
		sitar_schedule_hs_detect_plug(sitar);
	} else if (plug_type[0] == PLUG_TYPE_HEADSET) {
		pr_debug("%s: Valid plug found, determine plug type\n",
			 __func__);
		sitar_find_plug_and_report(codec, plug_type[0]);
	}

}

/* called under codec_resource_lock acquisition */
static void sitar_codec_detect_plug_type(struct snd_soc_codec *codec)
{
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);
	const struct sitar_mbhc_plug_detect_cfg *plug_det =
	    SITAR_MBHC_CAL_PLUG_DET_PTR(sitar->mbhc_cfg.calibration);

	if (plug_det->t_ins_complete > 20)
		msleep(plug_det->t_ins_complete);
	else
		usleep_range(plug_det->t_ins_complete * 1000,
			     plug_det->t_ins_complete * 1000);

	if (sitar_hs_gpio_level_remove(sitar))
		pr_debug("%s: GPIO value is low when determining "
				 "plug\n", __func__);
	else
		sitar_codec_decide_gpio_plug(codec);

	return;
}

static void sitar_hs_gpio_handler(struct snd_soc_codec *codec)
{
	bool insert;
	struct sitar_priv *priv = snd_soc_codec_get_drvdata(codec);
	bool is_removed = false;

	pr_debug("%s: enter\n", __func__);

	priv->in_gpio_handler = true;
	/* Wait here for debounce time */
	usleep_range(SITAR_GPIO_IRQ_DEBOUNCE_TIME_US,
		     SITAR_GPIO_IRQ_DEBOUNCE_TIME_US);

	SITAR_ACQUIRE_LOCK(priv->codec_resource_lock);

	/* cancel pending button press */
	if (sitar_cancel_btn_work(priv))
		pr_debug("%s: button press is canceled\n", __func__);

	insert = (gpio_get_value_cansleep(priv->mbhc_cfg.gpio) ==
		  priv->mbhc_cfg.gpio_level_insert);
	if ((priv->current_plug == PLUG_TYPE_NONE) && insert) {
		priv->lpi_enabled = false;
		wmb();

		/* cancel detect plug */
		sitar_cancel_hs_detect_plug(priv);

		/* Disable Mic Bias pull down and HPH Switch to GND */
		snd_soc_update_bits(codec, priv->mbhc_bias_regs.ctl_reg, 0x01,
				    0x00);
		snd_soc_update_bits(codec, SITAR_A_MBHC_HPH, 0x01, 0x00);
		sitar_codec_detect_plug_type(codec);
	} else if ((priv->current_plug != PLUG_TYPE_NONE) && !insert) {
		priv->lpi_enabled = false;
		wmb();

		/* cancel detect plug */
		sitar_cancel_hs_detect_plug(priv);

		if (priv->current_plug == PLUG_TYPE_HEADPHONE) {
			sitar_codec_report_plug(codec, 0, SND_JACK_HEADPHONE);
			is_removed = true;
		} else if (priv->current_plug == PLUG_TYPE_HEADSET) {
			sitar_codec_pause_hs_polling(codec);
			sitar_codec_cleanup_hs_polling(codec);
			sitar_codec_report_plug(codec, 0, SND_JACK_HEADSET);
			is_removed = true;
		}

		if (is_removed) {
			/* Enable Mic Bias pull down and HPH Switch to GND */
			snd_soc_update_bits(codec,
					    priv->mbhc_bias_regs.ctl_reg, 0x01,
					    0x01);
			snd_soc_update_bits(codec, SITAR_A_MBHC_HPH, 0x01,
					    0x01);
			/* Make sure mic trigger is turned off */
			snd_soc_update_bits(codec,
					    priv->mbhc_bias_regs.ctl_reg,
					    0x01, 0x01);
			snd_soc_update_bits(codec,
					    priv->mbhc_bias_regs.mbhc_reg,
					    0x90, 0x00);
			/* Reset MBHC State Machine */
			snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_CLK_CTL,
					    0x08, 0x08);
			snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_CLK_CTL,
					    0x08, 0x00);
			/* Turn off override */
			sitar_turn_onoff_override(codec, false);
		}
	}

	priv->in_gpio_handler = false;
	SITAR_RELEASE_LOCK(priv->codec_resource_lock);
	pr_debug("%s: leave\n", __func__);
}

static irqreturn_t sitar_mechanical_plug_detect_irq(int irq, void *data)
{
	int r = IRQ_HANDLED;
	struct snd_soc_codec *codec = data;
	struct wcd9xxx *core = codec->control_data;
	struct wcd9xxx_core_resource *core_res = &core->core_res;

	if (unlikely(wcd9xxx_lock_sleep(core_res) == false)) {
		pr_warn("%s(): Failed to hold suspend\n", __func__);
		r = IRQ_NONE;
	} else {
		sitar_hs_gpio_handler(codec);
		wcd9xxx_unlock_sleep(codec->control_data);
	}
	return r;
}

static int sitar_mbhc_init_and_calibrate(struct snd_soc_codec *codec)
{
	int rc = 0;
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);

	sitar->mbhc_cfg.mclk_cb_fn(codec, 1, false);
	sitar_mbhc_init(codec);
	sitar_mbhc_cal(codec);
	sitar_mbhc_calc_thres(codec);
	sitar->mbhc_cfg.mclk_cb_fn(codec, 0, false);
	sitar_codec_calibrate_hs_polling(codec);

	/* Enable Mic Bias pull down and HPH Switch to GND */
	snd_soc_update_bits(codec, sitar->mbhc_bias_regs.ctl_reg,
						0x01, 0x01);
	snd_soc_update_bits(codec, SITAR_A_MBHC_HPH,
						0x01, 0x01);

	rc = request_threaded_irq(sitar->mbhc_cfg.gpio_irq,
				NULL,
				sitar_mechanical_plug_detect_irq,
				(IRQF_TRIGGER_RISING |
				IRQF_TRIGGER_FALLING),
				"sitar-hs-gpio", codec);

	if (!IS_ERR_VALUE(rc)) {
		rc = enable_irq_wake(sitar->mbhc_cfg.gpio_irq);
		snd_soc_update_bits(codec, SITAR_A_RX_HPH_OCP_CTL,
							0x10, 0x10);
		wcd9xxx_enable_irq(codec->control_data,
				   WCD9XXX_IRQ_HPH_PA_OCPL_FAULT);
		wcd9xxx_enable_irq(codec->control_data,
				   WCD9XXX_IRQ_HPH_PA_OCPR_FAULT);
		/* Bootup time detection */
		sitar_hs_gpio_handler(codec);
	}

	return rc;
}

static void mbhc_fw_read(struct work_struct *work)
{
	struct delayed_work *dwork;
	struct sitar_priv *sitar;
	struct snd_soc_codec *codec;
	const struct firmware *fw;
	int ret = -1, retry = 0;

	dwork = to_delayed_work(work);
	sitar = container_of(dwork, struct sitar_priv,
				mbhc_firmware_dwork);
	codec = sitar->codec;

	while (retry < MBHC_FW_READ_ATTEMPTS) {
		retry++;
		pr_info("%s:Attempt %d to request MBHC firmware\n",
			__func__, retry);
		ret = request_firmware(&fw, "wcd9310/wcd9310_mbhc.bin",
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
	} else if (sitar_mbhc_fw_validate(fw) == false) {
		pr_err("%s: Invalid MBHC cal data size use default cal\n",
			 __func__);
		release_firmware(fw);
	} else {
		sitar->calibration = (void *)fw->data;
		sitar->mbhc_fw = fw;
	}

	sitar_mbhc_init_and_calibrate(codec);
}

int sitar_hs_detect(struct snd_soc_codec *codec,
			const struct sitar_mbhc_config *cfg)
{
	struct sitar_priv *sitar;
	int rc = 0;

	if (!codec || !cfg->calibration) {
		pr_err("Error: no codec or calibration\n");
		return -EINVAL;
	}

	if (cfg->mclk_rate != SITAR_MCLK_RATE_12288KHZ) {
		if (cfg->mclk_rate == SITAR_MCLK_RATE_9600KHZ)
			pr_err("Error: clock rate %dHz is not yet supported\n",
				cfg->mclk_rate);
		else
			pr_err("Error: unsupported clock rate %d\n",
				   cfg->mclk_rate);
		return -EINVAL;
	}

	sitar = snd_soc_codec_get_drvdata(codec);
	sitar->mbhc_cfg = *cfg;
	sitar->in_gpio_handler = false;
	sitar->current_plug = PLUG_TYPE_NONE;
	sitar->lpi_enabled = false;
	sitar_get_mbhc_micbias_regs(codec, &sitar->mbhc_bias_regs);

	/* Put CFILT in fast mode by default */
	snd_soc_update_bits(codec, sitar->mbhc_bias_regs.cfilt_ctl,
			    0x40, SITAR_CFILT_FAST_MODE);

	INIT_DELAYED_WORK(&sitar->mbhc_firmware_dwork, mbhc_fw_read);
	INIT_DELAYED_WORK(&sitar->mbhc_btn_dwork, btn_lpress_fn);
	INIT_WORK(&sitar->hphlocp_work, hphlocp_off_report);
	INIT_WORK(&sitar->hphrocp_work, hphrocp_off_report);
	INIT_WORK(&sitar->hs_correct_plug_work,
			  sitar_hs_correct_gpio_plug);

	if (!sitar->mbhc_cfg.read_fw_bin) {
		rc = sitar_mbhc_init_and_calibrate(codec);
	} else {
		schedule_delayed_work(&sitar->mbhc_firmware_dwork,
					usecs_to_jiffies(MBHC_FW_READ_TIMEOUT));
	}

	return rc;
}
EXPORT_SYMBOL_GPL(sitar_hs_detect);

static int sitar_determine_button(const struct sitar_priv *priv,
				  const s32 bias_mv)
{
	s16 *v_btn_low, *v_btn_high;
	struct sitar_mbhc_btn_detect_cfg *btn_det;
	int i, btn = -1;

	btn_det = SITAR_MBHC_CAL_BTN_DET_PTR(priv->mbhc_cfg.calibration);
	v_btn_low = sitar_mbhc_cal_btn_det_mp(btn_det, SITAR_BTN_DET_V_BTN_LOW);
	v_btn_high = sitar_mbhc_cal_btn_det_mp(btn_det,
				SITAR_BTN_DET_V_BTN_HIGH);
	for (i = 0; i < btn_det->num_btn; i++) {
		if ((v_btn_low[i] <= bias_mv) && (v_btn_high[i] >= bias_mv)) {
			btn = i;
			break;
		}
	}

	if (btn == -1)
		pr_debug("%s: couldn't find button number for mic mv %d\n",
			 __func__, bias_mv);

	return btn;
}

static int sitar_get_button_mask(const int btn)
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


static irqreturn_t sitar_dce_handler(int irq, void *data)
{
	int i, mask;
	short dce, sta, bias_value_dce;
	s32 mv, stamv, bias_mv_dce;
	int btn = -1, meas = 0;
	struct sitar_priv *priv = data;
	const struct sitar_mbhc_btn_detect_cfg *d =
	    SITAR_MBHC_CAL_BTN_DET_PTR(priv->mbhc_cfg.calibration);
	short btnmeas[d->n_btn_meas + 1];
	struct snd_soc_codec *codec = priv->codec;
	struct wcd9xxx *core = dev_get_drvdata(priv->codec->dev->parent);
	struct wcd9xxx_core_resource *core_res = &core->core_res;
	int n_btn_meas = d->n_btn_meas;
	u8 mbhc_status = snd_soc_read(codec, SITAR_A_CDC_MBHC_B1_STATUS) & 0x3E;

	pr_debug("%s: enter\n", __func__);

	SITAR_ACQUIRE_LOCK(priv->codec_resource_lock);
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

	dce = sitar_codec_read_dce_result(codec);
	mv = sitar_codec_sta_dce_v(codec, 1, dce);

	/* If GPIO interrupt already kicked in, ignore button press */
	if (priv->in_gpio_handler) {
		pr_debug("%s: GPIO State Changed, ignore button press\n",
			 __func__);
		btn = -1;
		goto done;
	}

	if (mbhc_status != SITAR_MBHC_STATUS_REL_DETECTION) {
		if (priv->mbhc_last_resume &&
		    !time_after(jiffies, priv->mbhc_last_resume + HZ)) {
			pr_debug("%s: Button is already released shortly after "
				 "resume\n", __func__);
			n_btn_meas = 0;
		} else {
			pr_debug("%s: Button is already released without "
				 "resume", __func__);
			sta = sitar_codec_read_sta_result(codec);
			stamv = sitar_codec_sta_dce_v(codec, 0, sta);
			btn = sitar_determine_button(priv, mv);
			if (btn != sitar_determine_button(priv, stamv))
				btn = -1;
			goto done;
		}
	}

	/* determine pressed button */
	btnmeas[meas++] = sitar_determine_button(priv, mv);
	pr_debug("%s: meas %d - DCE %d,%d, button %d\n", __func__,
		 meas - 1, dce, mv, btnmeas[meas - 1]);
	if (n_btn_meas == 0)
		btn = btnmeas[0];
	for (; ((d->n_btn_meas) && (meas < (d->n_btn_meas + 1))); meas++) {
		bias_value_dce = sitar_codec_sta_dce(codec, 1, false);
		bias_mv_dce = sitar_codec_sta_dce_v(codec, 1, bias_value_dce);
		btnmeas[meas] = sitar_determine_button(priv, bias_mv_dce);
		pr_debug("%s: meas %d - DCE %d,%d, button %d\n",
			 __func__, meas, bias_value_dce, bias_mv_dce,
			 btnmeas[meas]);
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
			pr_debug("%s: GPIO already triggered, ignore button "
				 "press\n", __func__);
			goto done;
		}
		mask = sitar_get_button_mask(btn);
		priv->buttons_pressed |= mask;
		wcd9xxx_lock_sleep(core_res);
		if (schedule_delayed_work(&priv->mbhc_btn_dwork,
					  msecs_to_jiffies(400)) == 0) {
			WARN(1, "Button pressed twice without release"
			     "event\n");
			wcd9xxx_unlock_sleep(core_res);
		}
	} else {
		pr_debug("%s: bogus button press, too short press?\n",
			 __func__);
	}

 done:
	pr_debug("%s: leave\n", __func__);
	SITAR_RELEASE_LOCK(priv->codec_resource_lock);
	return IRQ_HANDLED;
}

static int sitar_is_fake_press(struct sitar_priv *priv)
{
	int i;
	int r = 0;
	struct snd_soc_codec *codec = priv->codec;
	const int dces = MBHC_NUM_DCE_PLUG_DETECT;
	short mb_v;

	for (i = 0; i < dces; i++) {
		usleep_range(10000, 10000);
		if (i == 0) {
			mb_v = sitar_codec_sta_dce(codec, 0, true);
			pr_debug("%s: STA[0]: %d,%d\n", __func__, mb_v,
			sitar_codec_sta_dce_v(codec, 0, mb_v));
			if (mb_v < (short)priv->mbhc_data.v_b1_hu ||
				mb_v > (short)priv->mbhc_data.v_ins_hu) {
				r = 1;
				break;
			}
		} else {
			mb_v = sitar_codec_sta_dce(codec, 1, true);
			pr_debug("%s: DCE[%d]: %d,%d\n", __func__, i, mb_v,
			sitar_codec_sta_dce_v(codec, 1, mb_v));
			if (mb_v < (short)priv->mbhc_data.v_b1_h ||
				mb_v > (short)priv->mbhc_data.v_ins_h) {
				r = 1;
				break;
			}
		}
	}

	return r;
}

static irqreturn_t sitar_release_handler(int irq, void *data)
{
	int ret;
	struct sitar_priv *priv = data;
	struct snd_soc_codec *codec = priv->codec;

	pr_debug("%s: enter\n", __func__);

	SITAR_ACQUIRE_LOCK(priv->codec_resource_lock);
	priv->mbhc_state = MBHC_STATE_RELEASE;

	if (priv->buttons_pressed & SITAR_JACK_BUTTON_MASK) {
		ret = sitar_cancel_btn_work(priv);
		if (ret == 0) {
			pr_debug("%s: Reporting long button release event\n",
				 __func__);
			if (priv->mbhc_cfg.button_jack)
				sitar_snd_soc_jack_report(priv,
						  priv->mbhc_cfg.button_jack, 0,
						  priv->buttons_pressed);
		} else {
			if (sitar_is_fake_press(priv)) {
				pr_debug("%s: Fake button press interrupt\n",
					 __func__);
			} else if (priv->mbhc_cfg.button_jack) {
				if (priv->in_gpio_handler) {
					pr_debug("%s: GPIO kicked in, ignore\n",
						 __func__);
				} else {
					pr_debug("%s: Reporting short button 0 "
						 "press and release\n",
						 __func__);
					sitar_snd_soc_jack_report(priv,
						priv->mbhc_cfg.button_jack,
						priv->buttons_pressed,
						priv->buttons_pressed);
					sitar_snd_soc_jack_report(priv,
						priv->mbhc_cfg.button_jack, 0,
						priv->buttons_pressed);
				}
			}
		}

		priv->buttons_pressed &= ~SITAR_JACK_BUTTON_MASK;
	}

	sitar_codec_calibrate_hs_polling(codec);

	if (priv->mbhc_cfg.gpio)
		msleep(SITAR_MBHC_GPIO_REL_DEBOUNCE_TIME_MS);

	sitar_codec_start_hs_polling(codec);

	pr_debug("%s: leave\n", __func__);
	SITAR_RELEASE_LOCK(priv->codec_resource_lock);

	return IRQ_HANDLED;
}

static irqreturn_t sitar_hphl_ocp_irq(int irq, void *data)
{
	struct sitar_priv *sitar = data;
	struct snd_soc_codec *codec;
	struct wcd9xxx *core;
	struct wcd9xxx_core_resource *core_res;

	pr_info("%s: received HPHL OCP irq\n", __func__);

	if (sitar) {
		codec = sitar->codec;
		core = codec->control_data;
		core_res = &core->core_res;

		if ((sitar->hphlocp_cnt < SITAR_OCP_ATTEMPT) &&
		    (!sitar->hphrocp_cnt)) {
			pr_info("%s: retry\n", __func__);
			sitar->hphlocp_cnt++;
			snd_soc_update_bits(codec, SITAR_A_RX_HPH_OCP_CTL, 0x10,
					    0x00);
			snd_soc_update_bits(codec, SITAR_A_RX_HPH_OCP_CTL, 0x10,
					    0x10);
		} else {
			wcd9xxx_disable_irq(core_res,
					    WCD9XXX_IRQ_HPH_PA_OCPL_FAULT);
			sitar->hph_status |= SND_JACK_OC_HPHL;
			if (sitar->mbhc_cfg.headset_jack)
				sitar_snd_soc_jack_report(sitar,
						sitar->mbhc_cfg.headset_jack,
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
	struct wcd9xxx *core;
	struct wcd9xxx_core_resource *core_res;

	pr_info("%s: received HPHR OCP irq\n", __func__);

	if (sitar) {
		codec = sitar->codec;
		core = codec->control_data;
		core_res = &core->core_res;

		if ((sitar->hphrocp_cnt < SITAR_OCP_ATTEMPT) &&
		    (!sitar->hphlocp_cnt)) {
			pr_info("%s: retry\n", __func__);
			sitar->hphrocp_cnt++;
			snd_soc_update_bits(codec, SITAR_A_RX_HPH_OCP_CTL, 0x10,
					   0x00);
			snd_soc_update_bits(codec, SITAR_A_RX_HPH_OCP_CTL, 0x10,
					   0x10);
		} else {
			wcd9xxx_disable_irq(core_res,
					    WCD9XXX_IRQ_HPH_PA_OCPR_FAULT);
			sitar->hph_status |= SND_JACK_OC_HPHR;
			if (sitar->mbhc_cfg.headset_jack)
				sitar_snd_soc_jack_report(sitar,
						sitar->mbhc_cfg.headset_jack,
						sitar->hph_status,
						SITAR_JACK_MASK);
		}
	} else {
		pr_err("%s: Bad sitar private data\n", __func__);
	}

	return IRQ_HANDLED;
}

static irqreturn_t sitar_hs_insert_irq(int irq, void *data)
{
	struct sitar_priv *priv = data;
	struct snd_soc_codec *codec = priv->codec;
	struct wcd9xxx *core = codec->control_data;
	struct wcd9xxx_core_resource *core_res = &core->core_res;

	pr_debug("%s: enter\n", __func__);
	SITAR_ACQUIRE_LOCK(priv->codec_resource_lock);
	wcd9xxx_disable_irq(core_res, WCD9XXX_IRQ_MBHC_INSERTION);

	snd_soc_update_bits(codec, SITAR_A_CDC_MBHC_INT_CTL, 0x03, 0x00);

	/* Turn off both HPH and MIC line schmitt triggers */
	snd_soc_update_bits(codec, priv->mbhc_bias_regs.mbhc_reg, 0x90, 0x00);
	snd_soc_update_bits(codec, SITAR_A_MBHC_HPH, 0x13, 0x00);
	snd_soc_update_bits(codec, priv->mbhc_bias_regs.ctl_reg, 0x01, 0x00);

	pr_debug("%s: MIC trigger insertion interrupt\n", __func__);

	rmb();
	if (priv->lpi_enabled)
		msleep(100);

	rmb();
	if (!priv->lpi_enabled) {
		pr_debug("%s: lpi is disabled\n", __func__);
	} else if (gpio_get_value_cansleep(priv->mbhc_cfg.gpio) ==
		   priv->mbhc_cfg.gpio_level_insert) {
		pr_debug("%s: Valid insertion, "
			 "detect plug type\n", __func__);
		sitar_codec_decide_gpio_plug(codec);
	} else {
		pr_debug("%s: Invalid insertion, "
			 "stop plug detection\n", __func__);
	}
	SITAR_RELEASE_LOCK(priv->codec_resource_lock);
	return IRQ_HANDLED;
}

static bool is_valid_mic_voltage(struct snd_soc_codec *codec, s32 mic_mv)
{
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);
	struct sitar_mbhc_plug_type_cfg *plug_type =
		SITAR_MBHC_CAL_PLUG_TYPE_PTR(sitar->mbhc_cfg.calibration);

	return (!(mic_mv > SITAR_MBHC_FAKE_INSERT_LOW
				&& mic_mv < SITAR_MBHC_FAKE_INSERT_HIGH)
			&& (mic_mv > plug_type->v_no_mic)
			&& (mic_mv < plug_type->v_hs_max)) ? true : false;
}

/* called under codec_resource_lock acquisition
 * returns true if mic voltage range is back to normal insertion
 * returns false either if timedout or removed */
static bool sitar_hs_remove_settle(struct snd_soc_codec *codec)
{
	int i;
	bool timedout, settled = false;
	s32 mic_mv[MBHC_NUM_DCE_PLUG_DETECT];
	short mb_v[MBHC_NUM_DCE_PLUG_DETECT];
	unsigned long retry = 0, timeout;
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);

	timeout = jiffies + msecs_to_jiffies(SITAR_HS_DETECT_PLUG_TIME_MS);
	while (!(timedout = time_after(jiffies, timeout))) {
		retry++;
		if (sitar_hs_gpio_level_remove(sitar)) {
			pr_debug("%s: GPIO indicates removal\n", __func__);
			break;
		}

		if (retry > 1)
			msleep(250);
		else
			msleep(50);

		if (sitar_hs_gpio_level_remove(sitar)) {
			pr_debug("%s: GPIO indicates removal\n", __func__);
			break;
		}

		sitar_turn_onoff_override(codec, true);
		for (i = 0; i < MBHC_NUM_DCE_PLUG_DETECT; i++) {
			mb_v[i] = __sitar_codec_sta_dce(codec, 1,  true, true);
			mic_mv[i] = sitar_codec_sta_dce_v(codec, 1 , mb_v[i]);
			pr_debug("%s : DCE run %lu, mic_mv = %d(%x)\n",
				 __func__, retry, mic_mv[i], mb_v[i]);
		}
		sitar_turn_onoff_override(codec, false);

		if (sitar_hs_gpio_level_remove(sitar)) {
			pr_debug("%s: GPIO indicates removal\n", __func__);
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
	}

	if (timedout)
		pr_debug("%s: Microphone did not settle in %d seconds\n",
			 __func__, SITAR_HS_DETECT_PLUG_TIME_MS);
	return settled;
}

static irqreturn_t sitar_hs_remove_irq(int irq, void *data)
{
	struct sitar_priv *priv = data;
	struct snd_soc_codec *codec = priv->codec;

	pr_debug("%s: enter, removal interrupt\n", __func__);

	SITAR_ACQUIRE_LOCK(priv->codec_resource_lock);
	if (sitar_hs_remove_settle(codec))
		sitar_codec_start_hs_polling(codec);
	pr_debug("%s: remove settle done\n", __func__);

	SITAR_RELEASE_LOCK(priv->codec_resource_lock);
	return IRQ_HANDLED;
}



static irqreturn_t sitar_slimbus_irq(int irq, void *data)
{
	struct sitar_priv *priv = data;
	struct snd_soc_codec *codec = priv->codec;
	unsigned long slimbus_value;
	int i, j, k, port_id, ch_mask_temp;
	u8 val;


	for (i = 0; i < WCD9XXX_SLIM_NUM_PORT_REG; i++) {
		slimbus_value = wcd9xxx_interface_reg_read(codec->control_data,
			SITAR_SLIM_PGD_PORT_INT_STATUS0 + i);
		for_each_set_bit(j, &slimbus_value, BITS_PER_BYTE) {
			port_id = i*8 + j;
			val = wcd9xxx_interface_reg_read(codec->control_data,
				SITAR_SLIM_PGD_PORT_INT_SOURCE0 + port_id);
			if (val & 0x1)
				pr_err_ratelimited("overflow error on port %x, value %x\n",
						port_id, val);
			if (val & 0x2)
				pr_err_ratelimited("underflow error on port %x,value %x\n",
						port_id, val);
			if (val & 0x4) {
				pr_debug("%s: port %x disconnect value %x\n",
						 __func__, port_id, val);
				for (k = 0; k < ARRAY_SIZE(sitar_dai); k++) {
					ch_mask_temp = 1 << port_id;
					if (ch_mask_temp &
							priv->dai[k].ch_mask) {
						priv->dai[k].ch_mask &=
							~ch_mask_temp;
					    if (!priv->dai[k].ch_mask)
							wake_up(
						&priv->dai[k].dai_wait);
					}
				}
			}
		}
		wcd9xxx_interface_reg_write(codec->control_data,
			SITAR_SLIM_PGD_PORT_INT_CLR0 + i, slimbus_value);
		val = 0x0;
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
	int amic_reg_count = 0;

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
	snd_soc_update_bits(codec, SITAR_A_LDO_H_MODE_1, 0x0C,
		(pdata->micbias.ldoh_v << 2));

	snd_soc_update_bits(codec, SITAR_A_MICB_CFILT_1_VAL, 0xFC,
		(k1 << 2));
	snd_soc_update_bits(codec, SITAR_A_MICB_CFILT_2_VAL, 0xFC,
		(k2 << 2));

	snd_soc_update_bits(codec, SITAR_A_MICB_1_CTL, 0x60,
		(pdata->micbias.bias1_cfilt_sel << 5));
	snd_soc_update_bits(codec, SITAR_A_MICB_2_CTL, 0x60,
		(pdata->micbias.bias2_cfilt_sel << 5));

	/* Set micbias capless mode */
	snd_soc_update_bits(codec, SITAR_A_MICB_1_CTL, 0x10,
		(pdata->micbias.bias1_cap_mode << 4));
	snd_soc_update_bits(codec, SITAR_A_MICB_2_CTL, 0x10,
		(pdata->micbias.bias2_cap_mode << 4));

	amic_reg_count = (NUM_AMIC % 2) ? NUM_AMIC + 1 : NUM_AMIC;
	for (i = 0; i < amic_reg_count; j++, i += 2) {
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
	if (flag & 0x40) {
		value = (leg_mode & 0x40) ? 0x10 : 0x00;
		value = value | ((txfe_bypass & 0x40) ? 0x02 : 0x00);
		value = value | ((txfe_buff & 0x40) ? 0x01 : 0x00);
		snd_soc_update_bits(codec, SITAR_A_TX_4_MBHC_EN,
			0x13, value);
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

	SITAR_REG_VAL(SITAR_A_MICB_1_INT_RBIAS, 0x24),
	SITAR_REG_VAL(SITAR_A_MICB_2_INT_RBIAS, 0x24),

	SITAR_REG_VAL(SITAR_A_RX_HPH_BIAS_PA, 0x57),
	SITAR_REG_VAL(SITAR_A_RX_HPH_BIAS_LDO, 0x56),

	SITAR_REG_VAL(SITAR_A_RX_EAR_BIAS_PA, 0xA6),
	SITAR_REG_VAL(SITAR_A_RX_EAR_GAIN, 0x02),
	SITAR_REG_VAL(SITAR_A_RX_EAR_VCM, 0x03),

	SITAR_REG_VAL(SITAR_A_RX_LINE_BIAS_PA, 0xA7),

	SITAR_REG_VAL(SITAR_A_CDC_RX1_B5_CTL, 0x78),
	SITAR_REG_VAL(SITAR_A_CDC_RX2_B5_CTL, 0x78),
	SITAR_REG_VAL(SITAR_A_CDC_RX3_B5_CTL, 0x78),

	SITAR_REG_VAL(SITAR_A_CDC_RX1_B6_CTL, 0x80),

	SITAR_REG_VAL(SITAR_A_CDC_CLSG_FREQ_THRESH_B3_CTL, 0x1B),
	SITAR_REG_VAL(SITAR_A_CDC_CLSG_FREQ_THRESH_B4_CTL, 0x5B),

};

static void sitar_update_reg_defaults(struct snd_soc_codec *codec)
{
	u32 i;
	for (i = 0; i < ARRAY_SIZE(sitar_1_1_reg_defaults); i++)
		snd_soc_write(codec, sitar_1_1_reg_defaults[i].reg,
				sitar_1_1_reg_defaults[i].val);

}

static const struct sitar_reg_mask_val sitar_i2c_codec_reg_init_val[] = {
	{WCD9XXX_A_CHIP_CTL, 0x1, 0x1},
};

static const struct sitar_reg_mask_val sitar_codec_reg_init_val[] = {
	/* Initialize current threshold to 350MA
	* number of wait and run cycles to 4096
	*/
	{SITAR_A_RX_HPH_OCP_CTL, 0xE0, 0x60},
	{SITAR_A_RX_COM_OCP_COUNT, 0xFF, 0xFF},

	{SITAR_A_QFUSE_CTL, 0xFF, 0x03},

	/* Initialize gain registers to use register gain */
	{SITAR_A_RX_HPH_L_GAIN, 0x10, 0x10},
	{SITAR_A_RX_HPH_R_GAIN, 0x10, 0x10},
	{SITAR_A_RX_LINE_1_GAIN, 0x10, 0x10},
	{SITAR_A_RX_LINE_2_GAIN, 0x10, 0x10},

	/* Set the MICBIAS default output as pull down*/
	{SITAR_A_MICB_1_CTL, 0x01, 0x01},
	{SITAR_A_MICB_2_CTL, 0x01, 0x01},

	/* Initialize mic biases to differential mode */
	{SITAR_A_MICB_1_INT_RBIAS, 0x24, 0x24},
	{SITAR_A_MICB_2_INT_RBIAS, 0x24, 0x24},

	{SITAR_A_CDC_CONN_CLSG_CTL, 0x3C, 0x14},

	/* Use 16 bit sample size for TX1 to TX6 */
	{SITAR_A_CDC_CONN_TX_SB_B1_CTL, 0x30, 0x20},
	{SITAR_A_CDC_CONN_TX_SB_B2_CTL, 0x30, 0x20},
	{SITAR_A_CDC_CONN_TX_SB_B3_CTL, 0x30, 0x20},
	{SITAR_A_CDC_CONN_TX_SB_B4_CTL, 0x30, 0x20},
	{SITAR_A_CDC_CONN_TX_SB_B5_CTL, 0x30, 0x20},
	{SITAR_A_CDC_CLK_TX_CLK_EN_B1_CTL, 0x1, 0x1},

	/* Use 16 bit sample size for RX */
	{SITAR_A_CDC_CONN_RX_SB_B1_CTL, 0xFF, 0xAA},
	{SITAR_A_CDC_CONN_RX_SB_B2_CTL, 0x02, 0x02},

	/*enable HPF filter for TX paths */
	{SITAR_A_CDC_TX1_MUX_CTL, 0x8, 0x0},
	{SITAR_A_CDC_TX2_MUX_CTL, 0x8, 0x0},

	/*enable External clock select*/
	{SITAR_A_CDC_CLK_MCLK_CTL, 0x01, 0x01},
};

static void sitar_i2c_codec_init_reg(struct snd_soc_codec *codec)
{
	u32 i;
	for (i = 0; i < ARRAY_SIZE(sitar_i2c_codec_reg_init_val); i++)
		snd_soc_update_bits(codec, sitar_i2c_codec_reg_init_val[i].reg,
			sitar_i2c_codec_reg_init_val[i].mask,
			sitar_i2c_codec_reg_init_val[i].val);
}

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
	struct wcd9xxx *core;
	struct sitar_priv *sitar;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	int ret = 0;
	int i;
	u8 sitar_version;
	void *ptr = NULL;
	struct wcd9xxx_core_resource *core_res;

	codec->control_data = dev_get_drvdata(codec->dev->parent);
	core = codec->control_data;
	core_res = &core->core_res;

	sitar = kzalloc(sizeof(struct sitar_priv), GFP_KERNEL);
	if (!sitar) {
		dev_err(codec->dev, "Failed to allocate private data\n");
		return -ENOMEM;
	}

	for (i = 0; i < NUM_DECIMATORS; i++) {
		tx_hpf_work[i].sitar = sitar;
		tx_hpf_work[i].decimator = i + 1;
		INIT_DELAYED_WORK(&tx_hpf_work[i].dwork,
						  tx_hpf_corner_freq_callback);
	}


	/* Make sure mbhc micbias register addresses are zeroed out */
	memset(&sitar->mbhc_bias_regs, 0,
		sizeof(struct mbhc_micbias_regs));
	sitar->cfilt_k_value = 0;
	sitar->mbhc_micbias_switched = false;

	/* Make sure mbhc intenal calibration data is zeroed out */
	memset(&sitar->mbhc_data, 0,
		sizeof(struct mbhc_internal_cal_data));
	sitar->mbhc_data.t_sta_dce = DEFAULT_DCE_STA_WAIT;
	sitar->mbhc_data.t_dce = DEFAULT_DCE_WAIT;
	sitar->mbhc_data.t_sta = DEFAULT_STA_WAIT;
	snd_soc_codec_set_drvdata(codec, sitar);

	sitar->mclk_enabled = false;
	sitar->bandgap_type = SITAR_BANDGAP_OFF;
	sitar->clock_active = false;
	sitar->config_mode_active = false;
	sitar->mbhc_polling_active = false;
	sitar->no_mic_headset_override = false;
	mutex_init(&sitar->codec_resource_lock);
	sitar->codec = codec;
	sitar->mbhc_state = MBHC_STATE_NONE;
	sitar->mbhc_last_resume = 0;
	sitar->pdata = dev_get_platdata(codec->dev->parent);
	sitar_update_reg_defaults(codec);
	sitar_codec_init_reg(codec);
	sitar->intf_type = wcd9xxx_get_intf_type();
	if (sitar->intf_type == WCD9XXX_INTERFACE_TYPE_I2C)
		sitar_i2c_codec_init_reg(codec);

	for (i = 0; i < COMPANDER_MAX; i++) {
		sitar->comp_enabled[i] = 0;
		sitar->comp_fs[i] = COMPANDER_FS_48KHZ;
	}

	ret = sitar_handle_pdata(sitar);
	if (IS_ERR_VALUE(ret)) {
		pr_err("%s: bad pdata\n", __func__);
		goto err_pdata;
	}

	snd_soc_add_codec_controls(codec, sitar_snd_controls,
		ARRAY_SIZE(sitar_snd_controls));
	snd_soc_dapm_new_controls(dapm, sitar_dapm_widgets,
		ARRAY_SIZE(sitar_dapm_widgets));

	ptr = kmalloc((sizeof(sitar_rx_chs) +
		       sizeof(sitar_tx_chs)), GFP_KERNEL);
	if (!ptr) {
		pr_err("%s: no mem for slim chan ctl data\n", __func__);
		ret = -ENOMEM;
		goto err_nomem_slimch;
	}
	if (sitar->intf_type == WCD9XXX_INTERFACE_TYPE_I2C) {
		snd_soc_dapm_new_controls(dapm, sitar_dapm_i2s_widgets,
			ARRAY_SIZE(sitar_dapm_i2s_widgets));
		snd_soc_dapm_add_routes(dapm, audio_i2s_map,
		ARRAY_SIZE(audio_i2s_map));
		for (i = 0; i < ARRAY_SIZE(sitar_i2s_dai); i++)
			INIT_LIST_HEAD(&sitar->dai[i].wcd9xxx_ch_list);
	}
	if (sitar->intf_type == WCD9XXX_INTERFACE_TYPE_SLIMBUS) {
		for (i = 0; i < NUM_CODEC_DAIS; i++) {
			INIT_LIST_HEAD(&sitar->dai[i].wcd9xxx_ch_list);
			init_waitqueue_head(&sitar->dai[i].dai_wait);
		}
	}
	core->num_rx_port = SITAR_RX_MAX;
	core->rx_chs = ptr;
	memcpy(core->rx_chs, sitar_rx_chs, sizeof(sitar_rx_chs));
	core->num_tx_port = SITAR_TX_MAX;
	core->tx_chs = ptr + sizeof(sitar_rx_chs);
	memcpy(core->tx_chs, sitar_tx_chs, sizeof(sitar_tx_chs));

	snd_soc_dapm_add_routes(dapm, audio_map, ARRAY_SIZE(audio_map));

	sitar_version = snd_soc_read(codec, WCD9XXX_A_CHIP_VERSION);
	pr_info("%s : Sitar version reg 0x%2x\n", __func__, (u32)sitar_version);

	sitar_version &=  0x1F;
	pr_info("%s : Sitar version %u\n", __func__, (u32)sitar_version);

	snd_soc_dapm_sync(dapm);


	ret = wcd9xxx_request_irq(core_res,
				  WCD9XXX_IRQ_MBHC_INSERTION,
		sitar_hs_insert_irq, "Headset insert detect", sitar);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
		       WCD9XXX_IRQ_MBHC_INSERTION);
		goto err_insert_irq;
	}
	wcd9xxx_disable_irq(core_res, WCD9XXX_IRQ_MBHC_INSERTION);

	ret = wcd9xxx_request_irq(core_res,
				  WCD9XXX_IRQ_MBHC_REMOVAL,
		sitar_hs_remove_irq, "Headset remove detect", sitar);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
		       WCD9XXX_IRQ_MBHC_REMOVAL);
		goto err_remove_irq;
	}

	ret = wcd9xxx_request_irq(core_res,
				  WCD9XXX_IRQ_MBHC_POTENTIAL,
		sitar_dce_handler, "DC Estimation detect", sitar);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
		       WCD9XXX_IRQ_MBHC_POTENTIAL);
		goto err_potential_irq;
	}

	ret = wcd9xxx_request_irq(core_res,
				  WCD9XXX_IRQ_MBHC_RELEASE,
				  sitar_release_handler,
				  "Button Release detect", sitar);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
		       WCD9XXX_IRQ_MBHC_RELEASE);
		goto err_release_irq;
	}

	ret = wcd9xxx_request_irq(core_res, WCD9XXX_IRQ_SLIMBUS,
				  sitar_slimbus_irq, "SLIMBUS Slave", sitar);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
		       WCD9XXX_IRQ_SLIMBUS);
		goto err_slimbus_irq;
	}

	for (i = 0; i < WCD9XXX_SLIM_NUM_PORT_REG; i++)
		wcd9xxx_interface_reg_write(codec->control_data,
			SITAR_SLIM_PGD_PORT_INT_EN0 + i, 0xFF);


	ret = wcd9xxx_request_irq(core_res,
				  WCD9XXX_IRQ_HPH_PA_OCPL_FAULT,
				  sitar_hphl_ocp_irq,
				  "HPH_L OCP detect", sitar);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
		       WCD9XXX_IRQ_HPH_PA_OCPL_FAULT);
		goto err_hphl_ocp_irq;
	}
	wcd9xxx_disable_irq(core_res,
			    WCD9XXX_IRQ_HPH_PA_OCPL_FAULT);

	ret = wcd9xxx_request_irq(core_res,
				  WCD9XXX_IRQ_HPH_PA_OCPR_FAULT,
				  sitar_hphr_ocp_irq, "HPH_R OCP detect",
				  sitar);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
		       WCD9XXX_IRQ_HPH_PA_OCPR_FAULT);
		goto err_hphr_ocp_irq;
	}
	wcd9xxx_disable_irq(core_res, WCD9XXX_IRQ_HPH_PA_OCPR_FAULT);

	codec->ignore_pmdown_time = 1;

#ifdef CONFIG_DEBUG_FS
	debug_sitar_priv = sitar;
#endif

	return ret;

err_hphr_ocp_irq:
	wcd9xxx_free_irq(core_res, WCD9XXX_IRQ_HPH_PA_OCPL_FAULT,
			 sitar);
err_hphl_ocp_irq:
	wcd9xxx_free_irq(core_res, WCD9XXX_IRQ_SLIMBUS, sitar);
err_slimbus_irq:
	wcd9xxx_free_irq(core_res, WCD9XXX_IRQ_MBHC_RELEASE, sitar);
err_release_irq:
	wcd9xxx_free_irq(core_res, WCD9XXX_IRQ_MBHC_POTENTIAL,
			 sitar);
err_potential_irq:
	wcd9xxx_free_irq(core_res, WCD9XXX_IRQ_MBHC_REMOVAL, sitar);
err_remove_irq:
	wcd9xxx_free_irq(core_res, WCD9XXX_IRQ_MBHC_INSERTION,
			 sitar);
err_insert_irq:
	kfree(ptr);
err_nomem_slimch:
err_pdata:
	mutex_destroy(&sitar->codec_resource_lock);
	kfree(sitar);
	return ret;
}
static int sitar_codec_remove(struct snd_soc_codec *codec)
{
	struct sitar_priv *sitar = snd_soc_codec_get_drvdata(codec);
	struct wcd9xxx *core = codec->control_data;
	struct wcd9xxx_core_resource *core_res = &core->core_res;

	wcd9xxx_free_irq(core_res, WCD9XXX_IRQ_SLIMBUS, sitar);
	wcd9xxx_free_irq(core_res, WCD9XXX_IRQ_MBHC_RELEASE, sitar);
	wcd9xxx_free_irq(core_res, WCD9XXX_IRQ_MBHC_POTENTIAL,
			 sitar);
	wcd9xxx_free_irq(core_res, WCD9XXX_IRQ_MBHC_REMOVAL, sitar);
	wcd9xxx_free_irq(core_res, WCD9XXX_IRQ_MBHC_INSERTION,
			 sitar);
	SITAR_ACQUIRE_LOCK(sitar->codec_resource_lock);
	sitar_codec_disable_clock_block(codec);
	SITAR_RELEASE_LOCK(sitar->codec_resource_lock);
	sitar_codec_enable_bandgap(codec, SITAR_BANDGAP_OFF);
	if (sitar->mbhc_fw)
		release_firmware(sitar->mbhc_fw);
	mutex_destroy(&sitar->codec_resource_lock);
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
	struct platform_device *pdev = to_platform_device(dev);
	struct sitar_priv *sitar = platform_get_drvdata(pdev);
	dev_dbg(dev, "%s: system resume\n", __func__);
	sitar->mbhc_last_resume = jiffies;
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
	if (wcd9xxx_get_intf_type() == WCD9XXX_INTERFACE_TYPE_SLIMBUS)
		ret = snd_soc_register_codec(&pdev->dev, &soc_codec_dev_sitar,
			sitar_dai, ARRAY_SIZE(sitar_dai));
	else if (wcd9xxx_get_intf_type() == WCD9XXX_INTERFACE_TYPE_I2C)
		ret = snd_soc_register_codec(&pdev->dev, &soc_codec_dev_sitar,
			sitar_i2s_dai, ARRAY_SIZE(sitar_i2s_dai));
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
