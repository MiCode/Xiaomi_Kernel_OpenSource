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
#include <linux/mfd/wcd9xxx/core.h>
#include <linux/mfd/wcd9xxx/wcd9xxx_registers.h>
#include <linux/mfd/wcd9xxx/wcd9310_registers.h>
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
#include <linux/irq.h>
#include <linux/wakelock.h>
#include <linux/suspend.h>
#include "wcd9310.h"

static int cfilt_adjust_ms = 10;
module_param(cfilt_adjust_ms, int, 0644);
MODULE_PARM_DESC(cfilt_adjust_ms, "delay after adjusting cfilt voltage in ms");

#define WCD9310_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |\
			SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |\
			SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_192000)


#define NUM_DECIMATORS 10
#define NUM_INTERPOLATORS 7
#define BITS_PER_REG 8
#define TABLA_CFILT_FAST_MODE 0x00
#define TABLA_CFILT_SLOW_MODE 0x40
#define MBHC_FW_READ_ATTEMPTS 15
#define MBHC_FW_READ_TIMEOUT 2000000
#define MBHC_VDDIO_SWITCH_WAIT_MS 10
#define COMP_DIGITAL_DB_GAIN_APPLY(a, b) \
	(((a) <= 0) ? ((a) - b) : (a))

#define SLIM_CLOSE_TIMEOUT 1000
/* The wait time value comes from codec HW specification */
#define COMP_BRINGUP_WAIT_TIME  2000
enum {
	MBHC_USE_HPHL_TRIGGER = 1,
	MBHC_USE_MB_TRIGGER = 2
};

#define MBHC_NUM_DCE_PLUG_DETECT 3
#define NUM_ATTEMPTS_INSERT_DETECT 25
#define NUM_ATTEMPTS_TO_REPORT 5

#define TABLA_JACK_MASK (SND_JACK_HEADSET | SND_JACK_OC_HPHL | \
			 SND_JACK_OC_HPHR | SND_JACK_LINEOUT | \
			 SND_JACK_UNSUPPORTED)

#define TABLA_I2S_MASTER_MODE_MASK 0x08

#define TABLA_OCP_ATTEMPT 1

enum {
	AIF1_PB = 0,
	AIF1_CAP,
	AIF2_PB,
	AIF2_CAP,
	AIF3_PB,
	AIF3_CAP,
	NUM_CODEC_DAIS,
};

enum {
	RX_MIX1_INP_SEL_ZERO = 0,
	RX_MIX1_INP_SEL_SRC1,
	RX_MIX1_INP_SEL_SRC2,
	RX_MIX1_INP_SEL_IIR1,
	RX_MIX1_INP_SEL_IIR2,
	RX_MIX1_INP_SEL_RX1,
	RX_MIX1_INP_SEL_RX2,
	RX_MIX1_INP_SEL_RX3,
	RX_MIX1_INP_SEL_RX4,
	RX_MIX1_INP_SEL_RX5,
	RX_MIX1_INP_SEL_RX6,
	RX_MIX1_INP_SEL_RX7,
};
#define MAX_PA_GAIN_OPTIONS  13

#define TABLA_MCLK_RATE_12288KHZ 12288000
#define TABLA_MCLK_RATE_9600KHZ 9600000

#define TABLA_FAKE_INS_THRESHOLD_MS 2500
#define TABLA_FAKE_REMOVAL_MIN_PERIOD_MS 50

#define TABLA_MBHC_BUTTON_MIN 0x8000

#define TABLA_MBHC_FAKE_INSERT_LOW 10
#define TABLA_MBHC_FAKE_INSERT_HIGH 80
#define TABLA_MBHC_FAKE_INS_HIGH_NO_GPIO 150

#define TABLA_MBHC_STATUS_REL_DETECTION 0x0C

#define TABLA_MBHC_GPIO_REL_DEBOUNCE_TIME_MS 50

#define TABLA_MBHC_FAKE_INS_DELTA_MV 200
#define TABLA_MBHC_FAKE_INS_DELTA_SCALED_MV 300

#define TABLA_HS_DETECT_PLUG_TIME_MS (5 * 1000)
#define TABLA_HS_DETECT_PLUG_INERVAL_MS 100

#define TABLA_GPIO_IRQ_DEBOUNCE_TIME_US 5000

#define TABLA_MBHC_GND_MIC_SWAP_THRESHOLD 2
#define TABLA_RX_PORT_START_NUMBER	10


#define TABLA_ACQUIRE_LOCK(x) do { \
	mutex_lock_nested(&x, SINGLE_DEPTH_NESTING); \
} while (0)
#define TABLA_RELEASE_LOCK(x) do { mutex_unlock(&x); } while (0)

static const DECLARE_TLV_DB_SCALE(digital_gain, 0, 1, 0);
static const DECLARE_TLV_DB_SCALE(line_gain, 0, 7, 1);
static const DECLARE_TLV_DB_SCALE(analog_gain, 0, 25, 1);
static struct snd_soc_dai_driver tabla_dai[];
static const DECLARE_TLV_DB_SCALE(aux_pga_gain, 0, 2, 0);
static int tabla_codec_enable_slimrx(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event);
static int tabla_codec_enable_slimtx(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event);


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

enum {
	COMP_SHUTDWN_TIMEOUT_PCM_1 = 0,
	COMP_SHUTDWN_TIMEOUT_PCM_240,
	COMP_SHUTDWN_TIMEOUT_PCM_480,
	COMP_SHUTDWN_TIMEOUT_PCM_960,
	COMP_SHUTDWN_TIMEOUT_PCM_1440,
	COMP_SHUTDWN_TIMEOUT_PCM_2880,
	COMP_SHUTDWN_TIMEOUT_PCM_5760,
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


struct comp_sample_dependent_params {
	u32 peak_det_timeout;
	u32 rms_meter_div_fact;
	u32 rms_meter_resamp_fact;
	u32 shutdown_timeout;
};

struct comp_dgtl_gain_offset {
	u8 whole_db_gain;
	u8 half_db_gain;
};

static const struct comp_dgtl_gain_offset
			comp_dgtl_gain[MAX_PA_GAIN_OPTIONS] = {
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
	s16 adj_v_hs_max;
	u16 adj_v_ins_hu;
	u16 adj_v_ins_h;
	s16 v_inval_ins_low;
	s16 v_inval_ins_high;
};

struct tabla_reg_address {
	u16 micb_4_ctl;
	u16 micb_4_int_rbias;
	u16 micb_4_mbhc;
};

enum tabla_mbhc_plug_type {
	PLUG_TYPE_INVALID = -1,
	PLUG_TYPE_NONE,
	PLUG_TYPE_HEADSET,
	PLUG_TYPE_HEADPHONE,
	PLUG_TYPE_HIGH_HPH,
	PLUG_TYPE_GND_MIC_SWAP,
};

enum tabla_mbhc_state {
	MBHC_STATE_NONE = -1,
	MBHC_STATE_POTENTIAL,
	MBHC_STATE_POTENTIAL_RECOVERY,
	MBHC_STATE_RELEASE,
};

struct hpf_work {
	struct tabla_priv *tabla;
	u32 decimator;
	u8 tx_hpf_cut_of_freq;
	struct delayed_work dwork;
};

static struct hpf_work tx_hpf_work[NUM_DECIMATORS];

static const struct wcd9xxx_ch tabla_rx_chs[TABLA_RX_MAX] = {
	WCD9XXX_CH(TABLA_RX_PORT_START_NUMBER, 0),
	WCD9XXX_CH(TABLA_RX_PORT_START_NUMBER + 1, 1),
	WCD9XXX_CH(TABLA_RX_PORT_START_NUMBER + 2, 2),
	WCD9XXX_CH(TABLA_RX_PORT_START_NUMBER + 3, 3),
	WCD9XXX_CH(TABLA_RX_PORT_START_NUMBER + 4, 4),
	WCD9XXX_CH(TABLA_RX_PORT_START_NUMBER + 5, 5),
	WCD9XXX_CH(TABLA_RX_PORT_START_NUMBER + 6, 6)
};

static const struct wcd9xxx_ch tabla_tx_chs[TABLA_TX_MAX] = {
	WCD9XXX_CH(0, 0),
	WCD9XXX_CH(1, 1),
	WCD9XXX_CH(2, 2),
	WCD9XXX_CH(3, 3),
	WCD9XXX_CH(4, 4),
	WCD9XXX_CH(5, 5),
	WCD9XXX_CH(6, 6),
	WCD9XXX_CH(7, 7),
	WCD9XXX_CH(8, 8),
	WCD9XXX_CH(9, 9)
};

static const u32 vport_check_table[NUM_CODEC_DAIS] = {
	0,					/* AIF1_PB */
	(1 << AIF2_CAP) | (1 << AIF3_CAP),	/* AIF1_CAP */
	0,					/* AIF2_PB */
	(1 << AIF1_CAP) | (1 << AIF3_CAP),	/* AIF2_CAP */
	0,					/* AIF2_PB */
	(1 << AIF1_CAP) | (1 << AIF2_CAP),	/* AIF2_CAP */
};

static const u32 vport_i2s_check_table[NUM_CODEC_DAIS] = {
	0, /* AIF1_PB */
	0, /* AIF1_CAP */
};

struct tabla_priv {
	struct snd_soc_codec *codec;
	struct tabla_reg_address reg_addr;
	u32 adc_count;
	u32 cfilt1_cnt;
	u32 cfilt2_cnt;
	u32 cfilt3_cnt;
	u32 rx_bias_count;
	s32 dmic_1_2_clk_cnt;
	s32 dmic_3_4_clk_cnt;
	s32 dmic_5_6_clk_cnt;

	enum tabla_bandgap_type bandgap_type;
	bool mclk_enabled;
	bool clock_active;
	bool config_mode_active;
	bool mbhc_polling_active;
	unsigned long mbhc_fake_ins_start;
	int buttons_pressed;
	enum tabla_mbhc_state mbhc_state;
	struct tabla_mbhc_config mbhc_cfg;
	struct mbhc_internal_cal_data mbhc_data;
	u32 ldo_h_count;
	u32 micbias_enable_count[TABLA_NUM_MICBIAS];

	struct wcd9xxx_pdata *pdata;
	u32 anc_slot;
	bool anc_func;
	bool no_mic_headset_override;
	/* Delayed work to report long button press */
	struct delayed_work mbhc_btn_dwork;

	struct mbhc_micbias_regs mbhc_bias_regs;
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

	u8 hphlocp_cnt; /* headphone left ocp retry */
	u8 hphrocp_cnt; /* headphone right ocp retry */

	/* Work to perform MBHC Firmware Read */
	struct delayed_work mbhc_firmware_dwork;
	const struct firmware *mbhc_fw;

	/* num of slim ports required */
	struct wcd9xxx_codec_dai_data dai[NUM_CODEC_DAIS];

	/*compander*/
	int comp_enabled[COMPANDER_MAX];
	u32 comp_fs[COMPANDER_MAX];
	u8  comp_gain_offset[TABLA_SB_PGD_MAX_NUMBER_OF_RX_SLAVE_DEV_PORTS - 1];

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

	/* Work to perform polling on microphone voltage
	 * in order to correct plug type once plug type
	 * is detected as headphone
	 */
	struct work_struct hs_correct_plug_work_nogpio;

	bool gpio_irq_resend;
	struct wake_lock irq_resend_wlock;

#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs_poke;
	struct dentry *debugfs_mbhc;
#endif
};

static const u32 comp_shift[] = {
	0,
	1,
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

static const struct comp_sample_dependent_params
		    comp_samp_params[COMPANDER_FS_MAX] = {
	{
		.peak_det_timeout = 0x6,
		.rms_meter_div_fact = 0x9 << 4,
		.rms_meter_resamp_fact = 0x06,
		.shutdown_timeout = COMP_SHUTDWN_TIMEOUT_PCM_240 << 3,
	},
	{
		.peak_det_timeout = 0x7,
		.rms_meter_div_fact = 0xA << 4,
		.rms_meter_resamp_fact = 0x0C,
		.shutdown_timeout = COMP_SHUTDWN_TIMEOUT_PCM_480 << 3,
	},
	{
		.peak_det_timeout = 0x8,
		.rms_meter_div_fact = 0xB << 4,
		.rms_meter_resamp_fact = 0x30,
		.shutdown_timeout = COMP_SHUTDWN_TIMEOUT_PCM_960 << 3,
	},
	{
		.peak_det_timeout = 0x9,
		.rms_meter_div_fact = 0xB << 4,
		.rms_meter_resamp_fact = 0x28,
		.shutdown_timeout = COMP_SHUTDWN_TIMEOUT_PCM_1440 << 3,
	},
	{
		.peak_det_timeout = 0xA,
		.rms_meter_div_fact = 0xC << 4,
		.rms_meter_resamp_fact = 0x50,
		.shutdown_timeout = COMP_SHUTDWN_TIMEOUT_PCM_2880 << 3,
	},
	{
		.peak_det_timeout = 0xB,
		.rms_meter_div_fact = 0xC << 4,
		.rms_meter_resamp_fact = 0x50,
		.shutdown_timeout = COMP_SHUTDWN_TIMEOUT_PCM_5760 << 3,
	},
};

static unsigned short rx_digital_gain_reg[] = {
	TABLA_A_CDC_RX1_VOL_CTL_B2_CTL,
	TABLA_A_CDC_RX2_VOL_CTL_B2_CTL,
	TABLA_A_CDC_RX3_VOL_CTL_B2_CTL,
	TABLA_A_CDC_RX4_VOL_CTL_B2_CTL,
	TABLA_A_CDC_RX5_VOL_CTL_B2_CTL,
	TABLA_A_CDC_RX6_VOL_CTL_B2_CTL,
	TABLA_A_CDC_RX7_VOL_CTL_B2_CTL,
};


static unsigned short tx_digital_gain_reg[] = {
	TABLA_A_CDC_TX1_VOL_CTL_GAIN,
	TABLA_A_CDC_TX2_VOL_CTL_GAIN,
	TABLA_A_CDC_TX3_VOL_CTL_GAIN,
	TABLA_A_CDC_TX4_VOL_CTL_GAIN,
	TABLA_A_CDC_TX5_VOL_CTL_GAIN,
	TABLA_A_CDC_TX6_VOL_CTL_GAIN,
	TABLA_A_CDC_TX7_VOL_CTL_GAIN,
	TABLA_A_CDC_TX8_VOL_CTL_GAIN,
	TABLA_A_CDC_TX9_VOL_CTL_GAIN,
	TABLA_A_CDC_TX10_VOL_CTL_GAIN,
};

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
		snd_soc_update_bits(codec, TABLA_A_CDC_CLK_OTHR_RESET_CTL, 0x10,
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

static int tabla_get_anc_func(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&codec->dapm.codec->mutex);
	ucontrol->value.integer.value[0] = (tabla->anc_func == true ? 1 : 0);
	mutex_unlock(&codec->dapm.codec->mutex);
	return 0;
}

static int tabla_put_anc_func(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	mutex_lock(&dapm->codec->mutex);

	tabla->anc_func = (!ucontrol->value.integer.value[0] ? false : true);

	dev_dbg(codec->dev, "%s: anc_func %x", __func__, tabla->anc_func);

	if (tabla->anc_func == true) {
		snd_soc_dapm_enable_pin(dapm, "ANC HPHR");
		snd_soc_dapm_enable_pin(dapm, "ANC HPHL");
		snd_soc_dapm_enable_pin(dapm, "ANC HEADPHONE");
		snd_soc_dapm_disable_pin(dapm, "HPHR");
		snd_soc_dapm_disable_pin(dapm, "HPHL");
		snd_soc_dapm_disable_pin(dapm, "HEADPHONE");
	} else {
		snd_soc_dapm_disable_pin(dapm, "ANC HPHR");
		snd_soc_dapm_disable_pin(dapm, "ANC HPHL");
		snd_soc_dapm_disable_pin(dapm, "ANC HEADPHONE");
		snd_soc_dapm_enable_pin(dapm, "HPHR");
		snd_soc_dapm_enable_pin(dapm, "HPHL");
		snd_soc_dapm_enable_pin(dapm, "HEADPHONE");
	}
	snd_soc_dapm_sync(dapm);
	mutex_unlock(&dapm->codec->mutex);
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
	snd_soc_write(codec,
		(TABLA_A_CDC_IIR1_COEF_B1_CTL + 16 * iir_idx),
		(band_idx * BAND_MAX + coeff_idx) & 0x1F);

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
	snd_soc_write(codec,
		(TABLA_A_CDC_IIR1_COEF_B1_CTL + 16 * iir_idx),
		(band_idx * BAND_MAX + coeff_idx) & 0x1F);

	/* Mask top 2 bits, 7-8 are reserved */
	snd_soc_write(codec,
		(TABLA_A_CDC_IIR1_COEF_B2_CTL + 16 * iir_idx),
		(value >> 24) & 0x3F);

	/* Isolate 8bits at a time */
	snd_soc_write(codec,
		(TABLA_A_CDC_IIR1_COEF_B3_CTL + 16 * iir_idx),
		(value >> 16) & 0xFF);

	snd_soc_write(codec,
		(TABLA_A_CDC_IIR1_COEF_B4_CTL + 16 * iir_idx),
		(value >> 8) & 0xFF);

	snd_soc_write(codec,
		(TABLA_A_CDC_IIR1_COEF_B5_CTL + 16 * iir_idx),
		value & 0xFF);
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

static int tabla_compander_gain_offset(
	struct snd_soc_codec *codec, u32 enable,
	unsigned int pa_reg, unsigned int vol_reg,
	int mask, int event,
	struct comp_dgtl_gain_offset *gain_offset,
	int index)
{
	unsigned int pa_gain = snd_soc_read(codec, pa_reg);
	unsigned int digital_vol = snd_soc_read(codec, vol_reg);
	int pa_mode = pa_gain & mask;
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s: pa_gain(0x%x=0x%x)digital_vol(0x%x=0x%x)event(0x%x) index(%d)\n",
		 __func__, pa_reg, pa_gain, vol_reg, digital_vol, event, index);
	if (((pa_gain & 0xF) + 1) > ARRAY_SIZE(comp_dgtl_gain) ||
		(index >= ARRAY_SIZE(tabla->comp_gain_offset))) {
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
		tabla->comp_gain_offset[index] = digital_vol -
						 gain_offset->whole_db_gain ;
	}
	if (SND_SOC_DAPM_EVENT_OFF(event) && (pa_mode == 0)) {
		gain_offset->whole_db_gain = digital_vol +
					     tabla->comp_gain_offset[index];
		pr_debug("%s: listed whole_db_gain:0x%x, adjusted whole_db_gain:0x%x\n",
			 __func__, comp_dgtl_gain[pa_gain & 0xF].whole_db_gain,
			 gain_offset->whole_db_gain);
		gain_offset->half_db_gain = 0;
	}

	pr_debug("%s: half_db_gain(%d)whole_db_gain(%d)comp_gain_offset[%d](%d)\n",
		 __func__, gain_offset->half_db_gain,
		 gain_offset->whole_db_gain, index,
		 tabla->comp_gain_offset[index]);
	return 0;
}

static int tabla_config_gain_compander(
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
		tabla_compander_gain_offset(codec, enable,
				TABLA_A_RX_HPH_L_GAIN,
				TABLA_A_CDC_RX1_VOL_CTL_B2_CTL,
				mask, event, &gain_offset, 0);
		snd_soc_update_bits(codec, TABLA_A_RX_HPH_L_GAIN, mask, value);
		snd_soc_update_bits(codec, TABLA_A_CDC_RX1_VOL_CTL_B2_CTL,
				    0xFF, gain_offset.whole_db_gain);
		snd_soc_update_bits(codec, TABLA_A_CDC_RX1_B6_CTL,
				    0x02, gain_offset.half_db_gain);
		tabla_compander_gain_offset(codec, enable,
				TABLA_A_RX_HPH_R_GAIN,
				TABLA_A_CDC_RX2_VOL_CTL_B2_CTL,
				mask, event, &gain_offset, 1);
		snd_soc_update_bits(codec, TABLA_A_RX_HPH_R_GAIN, mask, value);
		snd_soc_update_bits(codec, TABLA_A_CDC_RX2_VOL_CTL_B2_CTL,
				    0xFF, gain_offset.whole_db_gain);
		snd_soc_update_bits(codec, TABLA_A_CDC_RX2_B6_CTL,
				    0x02, gain_offset.half_db_gain);
	} else if (compander == COMPANDER_2) {
		tabla_compander_gain_offset(codec, enable,
				TABLA_A_RX_LINE_1_GAIN,
				TABLA_A_CDC_RX3_VOL_CTL_B2_CTL,
				mask, event, &gain_offset, 2);
		snd_soc_update_bits(codec, TABLA_A_RX_LINE_1_GAIN, mask, value);
		snd_soc_update_bits(codec, TABLA_A_CDC_RX3_VOL_CTL_B2_CTL,
				    0xFF, gain_offset.whole_db_gain);
		snd_soc_update_bits(codec, TABLA_A_CDC_RX3_B6_CTL,
				    0x02, gain_offset.half_db_gain);
		tabla_compander_gain_offset(codec, enable,
				TABLA_A_RX_LINE_3_GAIN,
				TABLA_A_CDC_RX4_VOL_CTL_B2_CTL,
				mask, event, &gain_offset, 3);
		snd_soc_update_bits(codec, TABLA_A_RX_LINE_3_GAIN, mask, value);
		snd_soc_update_bits(codec, TABLA_A_CDC_RX4_VOL_CTL_B2_CTL,
				    0xFF, gain_offset.whole_db_gain);
		snd_soc_update_bits(codec, TABLA_A_CDC_RX4_B6_CTL,
				    0x02, gain_offset.half_db_gain);
		tabla_compander_gain_offset(codec, enable,
				TABLA_A_RX_LINE_2_GAIN,
				TABLA_A_CDC_RX5_VOL_CTL_B2_CTL,
				mask, event, &gain_offset, 4);
		snd_soc_update_bits(codec, TABLA_A_RX_LINE_2_GAIN, mask, value);
		snd_soc_update_bits(codec, TABLA_A_CDC_RX5_VOL_CTL_B2_CTL,
				    0xFF, gain_offset.whole_db_gain);
		snd_soc_update_bits(codec, TABLA_A_CDC_RX5_B6_CTL,
				    0x02, gain_offset.half_db_gain);
		tabla_compander_gain_offset(codec, enable,
				TABLA_A_RX_LINE_4_GAIN,
				TABLA_A_CDC_RX6_VOL_CTL_B2_CTL,
				mask, event, &gain_offset, 5);
		snd_soc_update_bits(codec, TABLA_A_RX_LINE_4_GAIN, mask, value);
		snd_soc_update_bits(codec, TABLA_A_CDC_RX6_VOL_CTL_B2_CTL,
				    0xFF, gain_offset.whole_db_gain);
		snd_soc_update_bits(codec, TABLA_A_CDC_RX6_B6_CTL,
				    0x02, gain_offset.half_db_gain);
	}
	return 0;
}
static int tabla_get_compander(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{

	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	int comp = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->max;
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = tabla->comp_enabled[comp];

	return 0;
}

static int tabla_set_compander(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);
	int comp = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->max;
	int value = ucontrol->value.integer.value[0];
	pr_debug("%s: compander #%d enable %d\n",
		 __func__, comp + 1, value);
	if (value == tabla->comp_enabled[comp]) {
		pr_debug("%s: compander #%d enable %d no change\n",
			 __func__, comp + 1, value);
		return 0;
	}
	tabla->comp_enabled[comp] = value;
	return 0;
}

static int tabla_config_compander(struct snd_soc_dapm_widget *w,
						  struct snd_kcontrol *kcontrol,
						  int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);
	u32 rate = tabla->comp_fs[w->shift];

	pr_debug("%s: compander #%d enable %d event %d widget name %s\n",
		 __func__, w->shift + 1,
		 tabla->comp_enabled[w->shift], event , w->name);
	if (tabla->comp_enabled[w->shift] == 0)
		goto rtn;
	if ((w->shift == COMPANDER_1) && (tabla->anc_func)) {
		pr_debug("%s: ANC is enabled so compander #%d cannot be enabled\n",
			 __func__, w->shift + 1);
		goto rtn;
	}
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Update compander sample rate */
		snd_soc_update_bits(codec, TABLA_A_CDC_COMP1_FS_CFG +
				    w->shift * 8, 0x07, rate);
		/* Enable both L/R compander clocks */
		snd_soc_update_bits(codec,
				    TABLA_A_CDC_CLK_RX_B2_CTL,
				    1 << comp_shift[w->shift],
				    1 << comp_shift[w->shift]);
		/* Toggle compander reset bits */
		snd_soc_update_bits(codec,
				    TABLA_A_CDC_CLK_OTHR_RESET_CTL,
				    1 << comp_shift[w->shift],
				    1 << comp_shift[w->shift]);
		snd_soc_update_bits(codec,
				    TABLA_A_CDC_CLK_OTHR_RESET_CTL,
				    1 << comp_shift[w->shift], 0);
		tabla_config_gain_compander(codec, w->shift, 1, event);
		/* Compander enable -> 0x370/0x378 */
		snd_soc_update_bits(codec, TABLA_A_CDC_COMP1_B1_CTL +
				    w->shift * 8, 0x03, 0x03);
		/* Update the RMS meter resampling */
		snd_soc_update_bits(codec,
				    TABLA_A_CDC_COMP1_B3_CTL +
				    w->shift * 8, 0xFF, 0x01);
		snd_soc_update_bits(codec,
				    TABLA_A_CDC_COMP1_B2_CTL +
				    w->shift * 8, 0xF0, 0x50);
		usleep_range(COMP_BRINGUP_WAIT_TIME, COMP_BRINGUP_WAIT_TIME);
		break;
	case SND_SOC_DAPM_POST_PMU:
		/* Set sample rate dependent paramater */
		if (w->shift == COMPANDER_1) {
			snd_soc_update_bits(codec,
					    TABLA_A_CDC_CLSG_CTL,
					    0x11, 0x00);
			snd_soc_write(codec,
				      TABLA_A_CDC_CONN_CLSG_CTL, 0x11);
		}
		snd_soc_update_bits(codec, TABLA_A_CDC_COMP1_B2_CTL +
				    w->shift * 8, 0x0F,
				    comp_samp_params[rate].peak_det_timeout);
		snd_soc_update_bits(codec, TABLA_A_CDC_COMP1_B2_CTL +
				    w->shift * 8, 0xF0,
				    comp_samp_params[rate].rms_meter_div_fact);
		snd_soc_update_bits(codec, TABLA_A_CDC_COMP1_B3_CTL +
				w->shift * 8, 0xFF,
				comp_samp_params[rate].rms_meter_resamp_fact);
		snd_soc_update_bits(codec, TABLA_A_CDC_COMP1_B1_CTL +
				    w->shift * 8, 0x38,
				    comp_samp_params[rate].shutdown_timeout);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		break;
	case SND_SOC_DAPM_POST_PMD:
		/* Disable the compander */
		snd_soc_update_bits(codec, TABLA_A_CDC_COMP1_B1_CTL +
				    w->shift * 8, 0x03, 0x00);
		/* Toggle compander reset bits */
		snd_soc_update_bits(codec,
				    TABLA_A_CDC_CLK_OTHR_RESET_CTL,
				    1 << comp_shift[w->shift],
				    1 << comp_shift[w->shift]);
		snd_soc_update_bits(codec,
				    TABLA_A_CDC_CLK_OTHR_RESET_CTL,
				    1 << comp_shift[w->shift], 0);
		/* Turn off the clock for compander in pair */
		snd_soc_update_bits(codec, TABLA_A_CDC_CLK_RX_B2_CTL,
				    0x03 << comp_shift[w->shift], 0);
		/* Restore the gain */
		tabla_config_gain_compander(codec, w->shift,
					    tabla->comp_enabled[w->shift],
					    event);
		if (w->shift == COMPANDER_1) {
			snd_soc_update_bits(codec,
					    TABLA_A_CDC_CLSG_CTL,
					    0x11, 0x11);
			snd_soc_write(codec,
				      TABLA_A_CDC_CONN_CLSG_CTL, 0x14);
		}
		break;
	}
rtn:
	return 0;
}

static int tabla_codec_hphr_dem_input_selection(struct snd_soc_dapm_widget *w,
						struct snd_kcontrol *kcontrol,
						int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s: compander#1->enable(%d) reg(0x%x = 0x%x) event(%d)\n",
		__func__, tabla->comp_enabled[COMPANDER_1],
		TABLA_A_CDC_RX1_B6_CTL,
		snd_soc_read(codec, TABLA_A_CDC_RX1_B6_CTL), event);
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		if (tabla->comp_enabled[COMPANDER_1] && !tabla->anc_func)
			snd_soc_update_bits(codec, TABLA_A_CDC_RX1_B6_CTL,
					    1 << w->shift, 0);
		else
			snd_soc_update_bits(codec, TABLA_A_CDC_RX1_B6_CTL,
					    1 << w->shift, 1 << w->shift);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, TABLA_A_CDC_RX1_B6_CTL,
				    1 << w->shift, 0);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int tabla_codec_hphl_dem_input_selection(struct snd_soc_dapm_widget *w,
						struct snd_kcontrol *kcontrol,
						int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s: compander#1->enable(%d) reg(0x%x = 0x%x) event(%d)\n",
		__func__, tabla->comp_enabled[COMPANDER_1],
		TABLA_A_CDC_RX2_B6_CTL,
		snd_soc_read(codec, TABLA_A_CDC_RX2_B6_CTL), event);
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		if (tabla->comp_enabled[COMPANDER_1] && !tabla->anc_func)
			snd_soc_update_bits(codec, TABLA_A_CDC_RX2_B6_CTL,
					    1 << w->shift, 0);
		else
			snd_soc_update_bits(codec, TABLA_A_CDC_RX2_B6_CTL,
					    1 << w->shift, 1 << w->shift);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, TABLA_A_CDC_RX2_B6_CTL,
				    1 << w->shift, 0);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static const char *const tabla_anc_func_text[] = {"OFF", "ON"};
static const struct soc_enum tabla_anc_func_enum =
	SOC_ENUM_SINGLE_EXT(2, tabla_anc_func_text);

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

	SOC_SINGLE_SX_TLV("RX1 Digital Volume", TABLA_A_CDC_RX1_VOL_CTL_B2_CTL,
		0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("RX2 Digital Volume", TABLA_A_CDC_RX2_VOL_CTL_B2_CTL,
		0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("RX3 Digital Volume", TABLA_A_CDC_RX3_VOL_CTL_B2_CTL,
		0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("RX4 Digital Volume", TABLA_A_CDC_RX4_VOL_CTL_B2_CTL,
		0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("RX5 Digital Volume", TABLA_A_CDC_RX5_VOL_CTL_B2_CTL,
		0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("RX6 Digital Volume", TABLA_A_CDC_RX6_VOL_CTL_B2_CTL,
		0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("RX7 Digital Volume", TABLA_A_CDC_RX7_VOL_CTL_B2_CTL,
		0, -84, 40, digital_gain),

	SOC_SINGLE_SX_TLV("DEC1 Volume", TABLA_A_CDC_TX1_VOL_CTL_GAIN, 0, -84,
		40, digital_gain),
	SOC_SINGLE_SX_TLV("DEC2 Volume", TABLA_A_CDC_TX2_VOL_CTL_GAIN, 0, -84,
		40, digital_gain),
	SOC_SINGLE_SX_TLV("DEC3 Volume", TABLA_A_CDC_TX3_VOL_CTL_GAIN, 0, -84,
		40, digital_gain),
	SOC_SINGLE_SX_TLV("DEC4 Volume", TABLA_A_CDC_TX4_VOL_CTL_GAIN, 0, -84,
		40, digital_gain),
	SOC_SINGLE_SX_TLV("DEC5 Volume", TABLA_A_CDC_TX5_VOL_CTL_GAIN, 0, -84,
		40, digital_gain),
	SOC_SINGLE_SX_TLV("DEC6 Volume", TABLA_A_CDC_TX6_VOL_CTL_GAIN, 0, -84,
		40, digital_gain),
	SOC_SINGLE_SX_TLV("DEC7 Volume", TABLA_A_CDC_TX7_VOL_CTL_GAIN, 0, -84,
		40, digital_gain),
	SOC_SINGLE_SX_TLV("DEC8 Volume", TABLA_A_CDC_TX8_VOL_CTL_GAIN, 0, -84,
		40, digital_gain),
	SOC_SINGLE_SX_TLV("DEC9 Volume", TABLA_A_CDC_TX9_VOL_CTL_GAIN, 0, -84,
		40, digital_gain),
	SOC_SINGLE_SX_TLV("DEC10 Volume", TABLA_A_CDC_TX10_VOL_CTL_GAIN, 0,
		-84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("IIR1 INP1 Volume", TABLA_A_CDC_IIR1_GAIN_B1_CTL, 0,
		-84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("IIR1 INP2 Volume", TABLA_A_CDC_IIR1_GAIN_B2_CTL, 0,
		-84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("IIR1 INP3 Volume", TABLA_A_CDC_IIR1_GAIN_B3_CTL, 0,
		-84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("IIR1 INP4 Volume", TABLA_A_CDC_IIR1_GAIN_B4_CTL, 0,
		-84, 40, digital_gain),
	SOC_SINGLE_TLV("ADC1 Volume", TABLA_A_TX_1_2_EN, 5, 3, 0, analog_gain),
	SOC_SINGLE_TLV("ADC2 Volume", TABLA_A_TX_1_2_EN, 1, 3, 0, analog_gain),
	SOC_SINGLE_TLV("ADC3 Volume", TABLA_A_TX_3_4_EN, 5, 3, 0, analog_gain),
	SOC_SINGLE_TLV("ADC4 Volume", TABLA_A_TX_3_4_EN, 1, 3, 0, analog_gain),
	SOC_SINGLE_TLV("ADC5 Volume", TABLA_A_TX_5_6_EN, 5, 3, 0, analog_gain),
	SOC_SINGLE_TLV("ADC6 Volume", TABLA_A_TX_5_6_EN, 1, 3, 0, analog_gain),

	SOC_SINGLE_TLV("AUX_PGA_LEFT Volume", TABLA_A_AUX_L_GAIN, 0, 39, 0,
		aux_pga_gain),
	SOC_SINGLE_TLV("AUX_PGA_RIGHT Volume", TABLA_A_AUX_R_GAIN, 0, 39, 0,
		aux_pga_gain),

	SOC_SINGLE("MICBIAS1 CAPLESS Switch", TABLA_A_MICB_1_CTL, 4, 1, 1),
	SOC_SINGLE("MICBIAS2 CAPLESS Switch", TABLA_A_MICB_2_CTL, 4, 1, 1),
	SOC_SINGLE("MICBIAS3 CAPLESS Switch", TABLA_A_MICB_3_CTL, 4, 1, 1),

	SOC_SINGLE_EXT("ANC Slot", SND_SOC_NOPM, 0, 0, 100, tabla_get_anc_slot,
		tabla_put_anc_slot),
	SOC_ENUM_EXT("ANC Function", tabla_anc_func_enum, tabla_get_anc_func,
		tabla_put_anc_func),
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
	SOC_SINGLE_EXT("COMP1 Switch", SND_SOC_NOPM, 1, COMPANDER_1, 0,
				   tabla_get_compander, tabla_set_compander),
	SOC_SINGLE_EXT("COMP2 Switch", SND_SOC_NOPM, 0, COMPANDER_2, 0,
				   tabla_get_compander, tabla_set_compander),
};

static const struct snd_kcontrol_new tabla_1_x_snd_controls[] = {
	SOC_SINGLE("MICBIAS4 CAPLESS Switch", TABLA_1_A_MICB_4_CTL, 4, 1, 1),
};

static const struct snd_kcontrol_new tabla_2_higher_snd_controls[] = {
	SOC_SINGLE("MICBIAS4 CAPLESS Switch", TABLA_2_A_MICB_4_CTL, 4, 1, 1),
};

static const char *rx_mix1_text[] = {
	"ZERO", "SRC1", "SRC2", "IIR1", "IIR2", "RX1", "RX2", "RX3", "RX4",
		"RX5", "RX6", "RX7"
};

static const char *rx_mix2_text[] = {
	"ZERO", "SRC1", "SRC2", "IIR1", "IIR2"
};

static const char *rx_dsm_text[] = {
	"CIC_OUT", "DSM_INV"
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

static const char *const iir_inp1_text[] = {
	"ZERO", "DEC1", "DEC2", "DEC3", "DEC4", "DEC5", "DEC6", "DEC7", "DEC8",
	"DEC9", "DEC10", "RX1", "RX2", "RX3", "RX4", "RX5", "RX6", "RX7"
};

static const struct soc_enum rx_mix1_inp1_chain_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_RX1_B1_CTL, 0, 12, rx_mix1_text);

static const struct soc_enum rx_mix1_inp2_chain_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_RX1_B1_CTL, 4, 12, rx_mix1_text);

static const struct soc_enum rx_mix1_inp3_chain_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_RX1_B2_CTL, 0, 12, rx_mix1_text);

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

static const struct soc_enum rx1_mix2_inp1_chain_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_RX1_B3_CTL, 0, 5, rx_mix2_text);

static const struct soc_enum rx1_mix2_inp2_chain_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_RX1_B3_CTL, 3, 5, rx_mix2_text);

static const struct soc_enum rx2_mix2_inp1_chain_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_RX2_B3_CTL, 0, 5, rx_mix2_text);

static const struct soc_enum rx2_mix2_inp2_chain_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_RX2_B3_CTL, 3, 5, rx_mix2_text);

static const struct soc_enum rx3_mix2_inp1_chain_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_RX3_B3_CTL, 0, 5, rx_mix2_text);

static const struct soc_enum rx3_mix2_inp2_chain_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_RX3_B3_CTL, 3, 5, rx_mix2_text);

static const struct soc_enum rx4_dsm_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_RX4_B6_CTL, 4, 2, rx_dsm_text);

static const struct soc_enum rx6_dsm_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_RX6_B6_CTL, 4, 2, rx_dsm_text);

static const struct soc_enum sb_tx1_mux_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_TX_SB_B1_CTL, 0, 9, sb_tx1_mux_text);

static const struct soc_enum sb_tx2_mux_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_TX_SB_B2_CTL, 0, 9, sb_tx2_mux_text);

static const struct soc_enum sb_tx3_mux_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_TX_SB_B3_CTL, 0, 9, sb_tx3_mux_text);

static const struct soc_enum sb_tx4_mux_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_TX_SB_B4_CTL, 0, 9, sb_tx4_mux_text);

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

static const struct soc_enum sb_tx9_mux_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_TX_SB_B9_CTL, 0, 18,
			sb_tx7_to_tx10_mux_text);

static const struct soc_enum sb_tx10_mux_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_TX_SB_B10_CTL, 0, 18,
			sb_tx7_to_tx10_mux_text);

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
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_EQ1_B1_CTL, 0, 18, iir_inp1_text);

static const struct soc_enum iir2_inp1_mux_enum =
	SOC_ENUM_SINGLE(TABLA_A_CDC_CONN_EQ2_B1_CTL, 0, 18, iir_inp1_text);

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

static const struct snd_kcontrol_new rx3_mix2_inp1_mux =
	SOC_DAPM_ENUM("RX3 MIX2 INP1 Mux", rx3_mix2_inp1_chain_enum);

static const struct snd_kcontrol_new rx3_mix2_inp2_mux =
	SOC_DAPM_ENUM("RX3 MIX2 INP2 Mux", rx3_mix2_inp2_chain_enum);

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


static int wcd9310_put_dec_enum(struct snd_kcontrol *kcontrol,
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

	dev_dbg(w->dapm->dev, "%s(): widget = %s  dec_name = %s decimator = %u"
		" dec_mux = %u\n", __func__, w->name, dec_name, decimator,
		dec_mux);


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

	tx_mux_ctl_reg = TABLA_A_CDC_TX1_MUX_CTL + 8 * (decimator - 1);

	snd_soc_update_bits(codec, tx_mux_ctl_reg, 0x1, adc_dmic_sel);

	ret = snd_soc_dapm_put_enum_double(kcontrol, ucontrol);

out:
	kfree(widget_name);
	return ret;
}

#define WCD9310_DEC_ENUM(xname, xenum) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
	.info = snd_soc_info_enum_double, \
	.get = snd_soc_dapm_get_enum_double, \
	.put = wcd9310_put_dec_enum, \
	.private_value = (unsigned long)&xenum }

static const struct snd_kcontrol_new dec1_mux =
	WCD9310_DEC_ENUM("DEC1 MUX Mux", dec1_mux_enum);

static const struct snd_kcontrol_new dec2_mux =
	WCD9310_DEC_ENUM("DEC2 MUX Mux", dec2_mux_enum);

static const struct snd_kcontrol_new dec3_mux =
	WCD9310_DEC_ENUM("DEC3 MUX Mux", dec3_mux_enum);

static const struct snd_kcontrol_new dec4_mux =
	WCD9310_DEC_ENUM("DEC4 MUX Mux", dec4_mux_enum);

static const struct snd_kcontrol_new dec5_mux =
	WCD9310_DEC_ENUM("DEC5 MUX Mux", dec5_mux_enum);

static const struct snd_kcontrol_new dec6_mux =
	WCD9310_DEC_ENUM("DEC6 MUX Mux", dec6_mux_enum);

static const struct snd_kcontrol_new dec7_mux =
	WCD9310_DEC_ENUM("DEC7 MUX Mux", dec7_mux_enum);

static const struct snd_kcontrol_new dec8_mux =
	WCD9310_DEC_ENUM("DEC8 MUX Mux", dec8_mux_enum);

static const struct snd_kcontrol_new dec9_mux =
	WCD9310_DEC_ENUM("DEC9 MUX Mux", dec9_mux_enum);

static const struct snd_kcontrol_new dec10_mux =
	WCD9310_DEC_ENUM("DEC10 MUX Mux", dec10_mux_enum);

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
	SOC_DAPM_SINGLE("Switch", TABLA_A_RX_EAR_EN, 5, 1, 0)
};
static const struct snd_kcontrol_new hphl_switch[] = {
	SOC_DAPM_SINGLE("Switch", TABLA_A_RX_HPH_L_DAC_CTL, 6, 1, 0)
};

static const struct snd_kcontrol_new hphl_pa_mix[] = {
	SOC_DAPM_SINGLE("AUX_PGA_L Switch", TABLA_A_AUX_L_PA_CONN,
					7, 1, 0),
	SOC_DAPM_SINGLE("AUX_PGA_R Switch", TABLA_A_AUX_R_PA_CONN,
					7, 1, 0),
	SOC_DAPM_SINGLE("AUX_PGA_L_INV Switch",
					TABLA_A_AUX_L_PA_CONN_INV, 7, 1, 0),
	SOC_DAPM_SINGLE("AUX_PGA_R_INV Switch",
					TABLA_A_AUX_R_PA_CONN_INV, 7, 1, 0),
};

static const struct snd_kcontrol_new hphr_pa_mix[] = {
	SOC_DAPM_SINGLE("AUX_PGA_L Switch", TABLA_A_AUX_L_PA_CONN,
					6, 1, 0),
	SOC_DAPM_SINGLE("AUX_PGA_R Switch", TABLA_A_AUX_R_PA_CONN,
					6, 1, 0),
	SOC_DAPM_SINGLE("AUX_PGA_L_INV Switch",
					TABLA_A_AUX_L_PA_CONN_INV, 6, 1, 0),
	SOC_DAPM_SINGLE("AUX_PGA_R_INV Switch",
					TABLA_A_AUX_R_PA_CONN_INV, 6, 1, 0),
};

static const struct snd_kcontrol_new lineout1_pa_mix[] = {
	SOC_DAPM_SINGLE("AUX_PGA_L Switch", TABLA_A_AUX_L_PA_CONN,
					5, 1, 0),
	SOC_DAPM_SINGLE("AUX_PGA_R Switch", TABLA_A_AUX_R_PA_CONN,
					5, 1, 0),
	SOC_DAPM_SINGLE("AUX_PGA_L_INV Switch",
					TABLA_A_AUX_L_PA_CONN_INV, 5, 1, 0),
	SOC_DAPM_SINGLE("AUX_PGA_R_INV Switch",
					TABLA_A_AUX_R_PA_CONN_INV, 5, 1, 0),
};

static const struct snd_kcontrol_new lineout2_pa_mix[] = {
	SOC_DAPM_SINGLE("AUX_PGA_L Switch", TABLA_A_AUX_L_PA_CONN,
					4, 1, 0),
	SOC_DAPM_SINGLE("AUX_PGA_R Switch", TABLA_A_AUX_R_PA_CONN,
					4, 1, 0),
	SOC_DAPM_SINGLE("AUX_PGA_L_INV Switch",
					TABLA_A_AUX_L_PA_CONN_INV, 4, 1, 0),
	SOC_DAPM_SINGLE("AUX_PGA_R_INV Switch",
					TABLA_A_AUX_R_PA_CONN_INV, 4, 1, 0),
};

static const struct snd_kcontrol_new lineout3_pa_mix[] = {
	SOC_DAPM_SINGLE("AUX_PGA_L Switch", TABLA_A_AUX_L_PA_CONN,
					3, 1, 0),
	SOC_DAPM_SINGLE("AUX_PGA_R Switch", TABLA_A_AUX_R_PA_CONN,
					3, 1, 0),
	SOC_DAPM_SINGLE("AUX_PGA_L_INV Switch",
					TABLA_A_AUX_L_PA_CONN_INV, 3, 1, 0),
	SOC_DAPM_SINGLE("AUX_PGA_R_INV Switch",
					TABLA_A_AUX_R_PA_CONN_INV, 3, 1, 0),
};

static const struct snd_kcontrol_new lineout4_pa_mix[] = {
	SOC_DAPM_SINGLE("AUX_PGA_L Switch", TABLA_A_AUX_L_PA_CONN,
					2, 1, 0),
	SOC_DAPM_SINGLE("AUX_PGA_R Switch", TABLA_A_AUX_R_PA_CONN,
					2, 1, 0),
	SOC_DAPM_SINGLE("AUX_PGA_L_INV Switch",
					TABLA_A_AUX_L_PA_CONN_INV, 2, 1, 0),
	SOC_DAPM_SINGLE("AUX_PGA_R_INV Switch",
					TABLA_A_AUX_R_PA_CONN_INV, 2, 1, 0),
};

static const struct snd_kcontrol_new lineout5_pa_mix[] = {
	SOC_DAPM_SINGLE("AUX_PGA_L Switch", TABLA_A_AUX_L_PA_CONN,
					1, 1, 0),
	SOC_DAPM_SINGLE("AUX_PGA_R Switch", TABLA_A_AUX_R_PA_CONN,
					1, 1, 0),
	SOC_DAPM_SINGLE("AUX_PGA_L_INV Switch",
					TABLA_A_AUX_L_PA_CONN_INV, 1, 1, 0),
	SOC_DAPM_SINGLE("AUX_PGA_R_INV Switch",
					TABLA_A_AUX_R_PA_CONN_INV, 1, 1, 0),
};

static const struct snd_kcontrol_new ear_pa_mix[] = {
	SOC_DAPM_SINGLE("AUX_PGA_L Switch", TABLA_A_AUX_L_PA_CONN,
					0, 1, 0),
	SOC_DAPM_SINGLE("AUX_PGA_R Switch", TABLA_A_AUX_R_PA_CONN,
					0, 1, 0),
	SOC_DAPM_SINGLE("AUX_PGA_L_INV Switch",
					TABLA_A_AUX_L_PA_CONN_INV, 0, 1, 0),
	SOC_DAPM_SINGLE("AUX_PGA_R_INV Switch",
					TABLA_A_AUX_R_PA_CONN_INV, 0, 1, 0),
};

static const struct snd_kcontrol_new lineout3_ground_switch =
	SOC_DAPM_SINGLE("Switch", TABLA_A_RX_LINE_3_DAC_CTL, 6, 1, 0);

static const struct snd_kcontrol_new lineout4_ground_switch =
	SOC_DAPM_SINGLE("Switch", TABLA_A_RX_LINE_4_DAC_CTL, 6, 1, 0);

/* virtual port entries */
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
	struct tabla_priv *tabla_p = snd_soc_codec_get_drvdata(codec);
	struct wcd9xxx *core = dev_get_drvdata(codec->dev->parent);
	struct soc_multi_mixer_control *mixer =
		((struct soc_multi_mixer_control *)kcontrol->private_value);
	u32 dai_id = widget->shift;
	u32 port_id = mixer->shift;
	u32 enable = ucontrol->value.integer.value[0];
	u32 vtable = vport_check_table[dai_id];

	pr_debug("%s: wname %s cname %s value %u shift %d item %ld\n", __func__,
		widget->name, ucontrol->id.name, widget->value, widget->shift,
		ucontrol->value.integer.value[0]);

	mutex_lock(&codec->mutex);
	if (tabla_p->intf_type != WCD9XXX_INTERFACE_TYPE_SLIMBUS) {
		if (dai_id != AIF1_CAP) {
			dev_err(codec->dev, "%s: invalid AIF for I2C mode\n",
				__func__);
			mutex_unlock(&codec->mutex);
			return -EINVAL;
		}
	}
	switch (dai_id) {
	case AIF1_CAP:
	case AIF2_CAP:
	case AIF3_CAP:
		/* only add to the list if value not set
		 */
		if (enable && !(widget->value & 1 << port_id)) {
			if (tabla_p->intf_type ==
				WCD9XXX_INTERFACE_TYPE_SLIMBUS)
				vtable = vport_check_table[dai_id];
			if (tabla_p->intf_type == WCD9XXX_INTERFACE_TYPE_I2C)
				vtable = vport_i2s_check_table[dai_id];
			if (wcd9xxx_tx_vport_validation(
						vtable,
						port_id,
						tabla_p->dai)) {
				dev_dbg(codec->dev, "%s: TX%u is used by other virtual port\n",
					__func__, port_id + 1);
				mutex_unlock(&codec->mutex);
				return 0;
			}
			widget->value |= 1 << port_id;
			list_add_tail(&core->tx_chs[port_id].list,
				      &tabla_p->dai[dai_id].wcd9xxx_ch_list
				      );
		} else if (!enable && (widget->value & 1 << port_id)) {
			widget->value &= ~(1 << port_id);
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

static const char *const slim_rx_mux_text[] = {
	"ZERO", "AIF1_PB", "AIF2_PB", "AIF3_PB"
};

static int slim_rx_mux_put(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget_list *wlist = snd_kcontrol_chip(kcontrol);
	struct snd_soc_dapm_widget *widget = wlist->widgets[0];
	struct snd_soc_codec *codec = widget->codec;
	struct tabla_priv *tabla_p = snd_soc_codec_get_drvdata(codec);
	struct wcd9xxx *core = dev_get_drvdata(codec->dev->parent);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 port_id = widget->shift;

	pr_debug("%s: wname %s cname %s value %u shift %d item %u\n", __func__,
		widget->name, ucontrol->id.name, widget->value, widget->shift,
		ucontrol->value.enumerated.item[0]);

	widget->value = ucontrol->value.enumerated.item[0];

	mutex_lock(&codec->mutex);

	if (tabla_p->intf_type != WCD9XXX_INTERFACE_TYPE_SLIMBUS) {
		if (widget->value > 1) {
			dev_err(codec->dev, "%s: invalid AIF for I2C mode\n",
				__func__);
			goto err;
		}
	}
	/* value need to match the Virtual port and AIF number
	 */
	switch (widget->value) {
	case 0:
		list_del_init(&core->rx_chs[port_id].list);
	break;
	case 1:
		if (wcd9xxx_rx_vport_validation(port_id +
			TABLA_RX_PORT_START_NUMBER,
			&tabla_p->dai[AIF1_PB].wcd9xxx_ch_list)) {
			dev_dbg(codec->dev, "%s: RX%u is used by current requesting AIF_PB itself\n",
				__func__, port_id + 1);
			goto rtn;
		}
		list_add_tail(&core->rx_chs[port_id].list,
			      &tabla_p->dai[AIF1_PB].wcd9xxx_ch_list);
	break;
	case 2:
		if (wcd9xxx_rx_vport_validation(port_id +
			TABLA_RX_PORT_START_NUMBER,
			&tabla_p->dai[AIF1_PB].wcd9xxx_ch_list)) {
			dev_dbg(codec->dev, "%s: RX%u is used by current requesting AIF_PB itself\n",
				__func__, port_id + 1);
			goto rtn;
		}
		list_add_tail(&core->rx_chs[port_id].list,
			      &tabla_p->dai[AIF2_PB].wcd9xxx_ch_list);
	break;
	case 3:
		if (wcd9xxx_rx_vport_validation(port_id +
			TABLA_RX_PORT_START_NUMBER,
			&tabla_p->dai[AIF1_PB].wcd9xxx_ch_list)) {
			dev_dbg(codec->dev, "%s: RX%u is used by current requesting AIF_PB itself\n",
				__func__, port_id + 1);
			goto rtn;
		}
		list_add_tail(&core->rx_chs[port_id].list,
			      &tabla_p->dai[AIF3_PB].wcd9xxx_ch_list);
	break;
	default:
		pr_err("Unknown AIF %d\n", widget->value);
		goto err;
	}
rtn:
	snd_soc_dapm_mux_update_power(widget, kcontrol, widget->value, e);
	mutex_unlock(&codec->mutex);
	return 0;
err:
	mutex_unlock(&codec->mutex);
	return -EINVAL;
}

static const struct soc_enum slim_rx_mux_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(slim_rx_mux_text), slim_rx_mux_text);

static const struct snd_kcontrol_new slim_rx_mux[TABLA_RX_MAX] = {
	SOC_DAPM_ENUM_EXT("SLIM RX1 Mux", slim_rx_mux_enum,
			  slim_rx_mux_get, slim_rx_mux_put),
	SOC_DAPM_ENUM_EXT("SLIM RX2 Mux", slim_rx_mux_enum,
			  slim_rx_mux_get, slim_rx_mux_put),
	SOC_DAPM_ENUM_EXT("SLIM RX3 Mux", slim_rx_mux_enum,
			  slim_rx_mux_get, slim_rx_mux_put),
	SOC_DAPM_ENUM_EXT("SLIM RX4 Mux", slim_rx_mux_enum,
			  slim_rx_mux_get, slim_rx_mux_put),
	SOC_DAPM_ENUM_EXT("SLIM RX5 Mux", slim_rx_mux_enum,
			  slim_rx_mux_get, slim_rx_mux_put),
	SOC_DAPM_ENUM_EXT("SLIM RX6 Mux", slim_rx_mux_enum,
			  slim_rx_mux_get, slim_rx_mux_put),
	SOC_DAPM_ENUM_EXT("SLIM RX7 Mux", slim_rx_mux_enum,
			  slim_rx_mux_get, slim_rx_mux_put),
};

static const struct snd_kcontrol_new aif_cap_mixer[] = {
	SOC_SINGLE_EXT("SLIM TX1", SND_SOC_NOPM, TABLA_TX1, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX2", SND_SOC_NOPM, TABLA_TX2, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX3", SND_SOC_NOPM, TABLA_TX3, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX4", SND_SOC_NOPM, TABLA_TX4, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX5", SND_SOC_NOPM, TABLA_TX5, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX6", SND_SOC_NOPM, TABLA_TX6, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX7", SND_SOC_NOPM, TABLA_TX7, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX8", SND_SOC_NOPM, TABLA_TX8, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX9", SND_SOC_NOPM, TABLA_TX9, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX10", SND_SOC_NOPM, TABLA_TX10, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
};

static void tabla_codec_enable_adc_block(struct snd_soc_codec *codec,
					 int enable)
{
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s %d\n", __func__, enable);

	if (enable) {
		tabla->adc_count++;
		snd_soc_update_bits(codec, TABLA_A_CDC_CLK_OTHR_CTL, 0x2, 0x2);
	} else {
		tabla->adc_count--;
		if (!tabla->adc_count)
			snd_soc_update_bits(codec, TABLA_A_CDC_CLK_OTHR_CTL,
					    0x2, 0x0);
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

static void tabla_codec_enable_audio_mode_bandgap(struct snd_soc_codec *codec)
{
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
	} else if (choice == TABLA_BANDGAP_MBHC_MODE) {
		/* bandgap mode becomes fast,
		 * mclk should be off or clk buff source souldn't be VBG
		 * Let's turn off mclk always */
		WARN_ON(snd_soc_read(codec, TABLA_A_CLK_BUFF_EN2) & (1 << 2));
		snd_soc_update_bits(codec, TABLA_A_BIAS_CENTRAL_BG_CTL, 0x2,
			0x2);
		snd_soc_update_bits(codec, TABLA_A_BIAS_CENTRAL_BG_CTL, 0x80,
			0x80);
		snd_soc_update_bits(codec, TABLA_A_BIAS_CENTRAL_BG_CTL, 0x4,
			0x4);
		snd_soc_update_bits(codec, TABLA_A_BIAS_CENTRAL_BG_CTL, 0x01,
			0x01);
		usleep_range(1000, 1000);
		snd_soc_update_bits(codec, TABLA_A_BIAS_CENTRAL_BG_CTL, 0x80,
			0x00);
	} else if ((tabla->bandgap_type == TABLA_BANDGAP_MBHC_MODE) &&
		(choice == TABLA_BANDGAP_AUDIO_MODE)) {
		snd_soc_write(codec, TABLA_A_BIAS_CENTRAL_BG_CTL, 0x50);
		usleep_range(100, 100);
		tabla_codec_enable_audio_mode_bandgap(codec);
	} else if (choice == TABLA_BANDGAP_OFF) {
		snd_soc_write(codec, TABLA_A_BIAS_CENTRAL_BG_CTL, 0x50);
	} else {
		pr_err("%s: Error, Invalid bandgap settings\n", __func__);
	}
	tabla->bandgap_type = choice;
}

static void tabla_codec_disable_clock_block(struct snd_soc_codec *codec)
{
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);
	pr_debug("%s\n", __func__);
	snd_soc_update_bits(codec, TABLA_A_CLK_BUFF_EN2, 0x04, 0x00);
	usleep_range(50, 50);
	snd_soc_update_bits(codec, TABLA_A_CLK_BUFF_EN2, 0x02, 0x02);
	snd_soc_update_bits(codec, TABLA_A_CLK_BUFF_EN1, 0x05, 0x00);
	usleep_range(50, 50);
	tabla->clock_active = false;
}

static int tabla_codec_mclk_index(const struct tabla_priv *tabla)
{
	if (tabla->mbhc_cfg.mclk_rate == TABLA_MCLK_RATE_12288KHZ)
		return 0;
	else if (tabla->mbhc_cfg.mclk_rate == TABLA_MCLK_RATE_9600KHZ)
		return 1;
	else {
		BUG_ON(1);
		return -EINVAL;
	}
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

static int tabla_codec_enable_config_mode(struct snd_soc_codec *codec,
	int enable)
{
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s: enable = %d\n", __func__, enable);
	if (enable) {
		snd_soc_update_bits(codec, TABLA_A_CONFIG_MODE_FREQ, 0x10, 0);
		/* bandgap mode to fast */
		snd_soc_write(codec, TABLA_A_BIAS_CONFIG_MODE_BG_CTL, 0x17);
		usleep_range(5, 5);
		snd_soc_update_bits(codec, TABLA_A_CONFIG_MODE_FREQ, 0x80,
				    0x80);
		snd_soc_update_bits(codec, TABLA_A_CONFIG_MODE_TEST, 0x80,
				    0x80);
		usleep_range(10, 10);
		snd_soc_update_bits(codec, TABLA_A_CONFIG_MODE_TEST, 0x80, 0);
		usleep_range(10000, 10000);
		snd_soc_update_bits(codec, TABLA_A_CLK_BUFF_EN1, 0x08, 0x08);
	} else {
		snd_soc_update_bits(codec, TABLA_A_BIAS_CONFIG_MODE_BG_CTL, 0x1,
				    0);
		snd_soc_update_bits(codec, TABLA_A_CONFIG_MODE_FREQ, 0x80, 0);
		/* clk source to ext clk and clk buff ref to VBG */
		snd_soc_update_bits(codec, TABLA_A_CLK_BUFF_EN1, 0x0C, 0x04);
	}
	tabla->config_mode_active = enable ? true : false;

	return 0;
}

static int tabla_codec_enable_clock_block(struct snd_soc_codec *codec,
					  int config_mode)
{
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s: config_mode = %d\n", __func__, config_mode);

	/* transit to RCO requires mclk off */
	WARN_ON(snd_soc_read(codec, TABLA_A_CLK_BUFF_EN2) & (1 << 2));
	if (config_mode) {
		/* enable RCO and switch to it */
		tabla_codec_enable_config_mode(codec, 1);
		snd_soc_write(codec, TABLA_A_CLK_BUFF_EN2, 0x02);
		usleep_range(1000, 1000);
	} else {
		/* switch to MCLK */
		snd_soc_update_bits(codec, TABLA_A_CLK_BUFF_EN1, 0x08, 0x00);

		if (tabla->mbhc_polling_active) {
			snd_soc_write(codec, TABLA_A_CLK_BUFF_EN2, 0x02);
			tabla_codec_enable_config_mode(codec, 0);
		}
	}

	snd_soc_update_bits(codec, TABLA_A_CLK_BUFF_EN1, 0x01, 0x01);
	snd_soc_update_bits(codec, TABLA_A_CLK_BUFF_EN2, 0x02, 0x00);
	/* on MCLK */
	snd_soc_update_bits(codec, TABLA_A_CLK_BUFF_EN2, 0x04, 0x04);
	snd_soc_update_bits(codec, TABLA_A_CDC_CLK_MCLK_CTL, 0x01, 0x01);
	usleep_range(50, 50);
	tabla->clock_active = true;
	return 0;
}

static int tabla_codec_enable_aux_pga(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s: %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		tabla_codec_enable_bandgap(codec,
					TABLA_BANDGAP_AUDIO_MODE);
		tabla_enable_rx_bias(codec, 1);

		snd_soc_update_bits(codec, TABLA_A_AUX_COM_CTL,
						0x08, 0x08);
		/* Enable Zero Cross detect for AUX PGA channel
		 * and set the initial AUX PGA gain to NEG_0P0_DB
		 * to avoid glitches.
		 */
		if (w->reg == TABLA_A_AUX_L_EN) {
			snd_soc_update_bits(codec, TABLA_A_AUX_L_EN,
						0x20, 0x20);
			tabla->aux_l_gain = snd_soc_read(codec,
							TABLA_A_AUX_L_GAIN);
			snd_soc_write(codec, TABLA_A_AUX_L_GAIN, 0x1F);
		} else {
			snd_soc_update_bits(codec, TABLA_A_AUX_R_EN,
						0x20, 0x20);
			tabla->aux_r_gain = snd_soc_read(codec,
							TABLA_A_AUX_R_GAIN);
			snd_soc_write(codec, TABLA_A_AUX_R_GAIN, 0x1F);
		}
		if (tabla->aux_pga_cnt++ == 1
			&& !tabla->mclk_enabled) {
			tabla_codec_enable_clock_block(codec, 1);
			pr_debug("AUX PGA enabled RC osc\n");
		}
		break;

	case SND_SOC_DAPM_POST_PMU:
		if (w->reg == TABLA_A_AUX_L_EN)
			snd_soc_write(codec, TABLA_A_AUX_L_GAIN,
				tabla->aux_l_gain);
		else
			snd_soc_write(codec, TABLA_A_AUX_R_GAIN,
				tabla->aux_r_gain);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		/* Mute AUX PGA channel in use before disabling AUX PGA */
		if (w->reg == TABLA_A_AUX_L_EN) {
			tabla->aux_l_gain = snd_soc_read(codec,
							TABLA_A_AUX_L_GAIN);
			snd_soc_write(codec, TABLA_A_AUX_L_GAIN, 0x1F);
		} else {
			tabla->aux_r_gain = snd_soc_read(codec,
							TABLA_A_AUX_R_GAIN);
			snd_soc_write(codec, TABLA_A_AUX_R_GAIN, 0x1F);
		}
		break;

	case SND_SOC_DAPM_POST_PMD:
		tabla_enable_rx_bias(codec, 0);

		snd_soc_update_bits(codec, TABLA_A_AUX_COM_CTL,
						0x08, 0x00);
		if (w->reg == TABLA_A_AUX_L_EN) {
			snd_soc_write(codec, TABLA_A_AUX_L_GAIN,
					tabla->aux_l_gain);
			snd_soc_update_bits(codec, TABLA_A_AUX_L_EN,
							0x20, 0x00);
		} else {
			snd_soc_write(codec, TABLA_A_AUX_R_GAIN,
					tabla->aux_r_gain);
			snd_soc_update_bits(codec, TABLA_A_AUX_R_EN,
						0x20, 0x00);
		}

		if (tabla->aux_pga_cnt-- == 0) {
			if (tabla->mbhc_polling_active)
				tabla_codec_enable_bandgap(codec,
					TABLA_BANDGAP_MBHC_MODE);
			else
				tabla_codec_enable_bandgap(codec,
					TABLA_BANDGAP_OFF);

			if (!tabla->mclk_enabled &&
				!tabla->mbhc_polling_active) {
				tabla_codec_enable_clock_block(codec, 0);
			}
		}
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
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);
	u8  dmic_clk_en;
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
		dmic_clk_cnt = &(tabla->dmic_1_2_clk_cnt);

		pr_debug("%s() event %d DMIC%d dmic_1_2_clk_cnt %d\n",
			__func__, event,  dmic, *dmic_clk_cnt);

		break;

	case 3:
	case 4:
		dmic_clk_en = 0x04;
		dmic_clk_cnt = &(tabla->dmic_3_4_clk_cnt);

		pr_debug("%s() event %d DMIC%d dmic_3_4_clk_cnt %d\n",
			__func__, event,  dmic, *dmic_clk_cnt);
		break;

	case 5:
	case 6:
		dmic_clk_en = 0x10;
		dmic_clk_cnt = &(tabla->dmic_5_6_clk_cnt);

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
		if (*dmic_clk_cnt == 1) {
			snd_soc_update_bits(codec,
					TABLA_A_CDC_DMIC_CLK0_MODE, 0x7, 0x0);
			snd_soc_update_bits(codec,
					TABLA_A_CDC_DMIC_CLK1_MODE, 0x7, 0x0);
			snd_soc_update_bits(codec,
					TABLA_A_CDC_DMIC_CLK2_MODE, 0x7, 0x0);
			snd_soc_update_bits(codec, TABLA_A_CDC_CLK_DMIC_CTL,
					dmic_clk_en, dmic_clk_en);
		}

		break;
	case SND_SOC_DAPM_POST_PMD:

		(*dmic_clk_cnt)--;
		if (*dmic_clk_cnt  == 0) {
			snd_soc_update_bits(codec, TABLA_A_CDC_CLK_DMIC_CTL,
					dmic_clk_en, 0);
			snd_soc_update_bits(codec,
					TABLA_A_CDC_DMIC_CLK0_MODE, 0x7, 0x4);
			snd_soc_update_bits(codec,
					TABLA_A_CDC_DMIC_CLK1_MODE, 0x7, 0x4);
			snd_soc_update_bits(codec,
					TABLA_A_CDC_DMIC_CLK2_MODE, 0x7, 0x4);
		}
		break;
	}
	return 0;
}


/* called under codec_resource_lock acquisition */
static void tabla_codec_start_hs_polling(struct snd_soc_codec *codec)
{
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);
	struct wcd9xxx *tabla_core = dev_get_drvdata(codec->dev->parent);
	int mbhc_state = tabla->mbhc_state;

	pr_debug("%s: enter\n", __func__);
	if (!tabla->mbhc_polling_active) {
		pr_debug("Polling is not active, do not start polling\n");
		return;
	}
	snd_soc_write(codec, TABLA_A_MBHC_SCALING_MUX_1, 0x84);

	if (tabla->no_mic_headset_override) {
		pr_debug("%s setting button threshold to min", __func__);
		/* set to min */
		snd_soc_write(codec, TABLA_A_CDC_MBHC_VOLT_B4_CTL, 0x80);
		snd_soc_write(codec, TABLA_A_CDC_MBHC_VOLT_B3_CTL, 0x00);
		snd_soc_write(codec, TABLA_A_CDC_MBHC_VOLT_B6_CTL, 0x80);
		snd_soc_write(codec, TABLA_A_CDC_MBHC_VOLT_B5_CTL, 0x00);
	} else if (unlikely(mbhc_state == MBHC_STATE_POTENTIAL)) {
		pr_debug("%s recovering MBHC state machine\n", __func__);
		tabla->mbhc_state = MBHC_STATE_POTENTIAL_RECOVERY;
		/* set to max button press threshold */
		snd_soc_write(codec, TABLA_A_CDC_MBHC_VOLT_B2_CTL, 0x7F);
		snd_soc_write(codec, TABLA_A_CDC_MBHC_VOLT_B1_CTL, 0xFF);
		snd_soc_write(codec, TABLA_A_CDC_MBHC_VOLT_B4_CTL,
			      (TABLA_IS_1_X(tabla_core->version) ?
			       0x07 : 0x7F));
		snd_soc_write(codec, TABLA_A_CDC_MBHC_VOLT_B3_CTL, 0xFF);
		/* set to max */
		snd_soc_write(codec, TABLA_A_CDC_MBHC_VOLT_B6_CTL, 0x7F);
		snd_soc_write(codec, TABLA_A_CDC_MBHC_VOLT_B5_CTL, 0xFF);
	}

	snd_soc_write(codec, TABLA_A_CDC_MBHC_EN_CTL, 0x1);
	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_CLK_CTL, 0x8, 0x0);
	snd_soc_write(codec, TABLA_A_CDC_MBHC_EN_CTL, 0x1);
	pr_debug("%s: leave\n", __func__);
}

/* called under codec_resource_lock acquisition */
static void tabla_codec_pause_hs_polling(struct snd_soc_codec *codec)
{
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s: enter\n", __func__);
	if (!tabla->mbhc_polling_active) {
		pr_debug("polling not active, nothing to pause\n");
		return;
	}

	snd_soc_update_bits(codec, tabla->mbhc_bias_regs.ctl_reg, 0x01, 0x01);
	msleep(20);
	snd_soc_update_bits(codec, tabla->mbhc_bias_regs.ctl_reg, 0x01, 0x00);
	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_CLK_CTL, 0x8, 0x8);
	pr_debug("%s: leave\n", __func__);
}

static void tabla_codec_switch_cfilt_mode(struct snd_soc_codec *codec, int mode)
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
		TABLA_ACQUIRE_LOCK(tabla->codec_resource_lock);
		if (tabla->mbhc_polling_active) {
			tabla_codec_pause_hs_polling(codec);
			mbhc_was_polling = true;
		}
		snd_soc_update_bits(codec,
			tabla->mbhc_bias_regs.cfilt_ctl, 0x40, reg_mode_val);
		if (mbhc_was_polling)
			tabla_codec_start_hs_polling(codec);
		TABLA_RELEASE_LOCK(tabla->codec_resource_lock);
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

static void tabla_turn_onoff_override(struct snd_soc_codec *codec, bool on)
{
	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_B1_CTL, 0x04, on << 2);
}

/* called under codec_resource_lock acquisition */
static void tabla_codec_drive_v_to_micbias(struct snd_soc_codec *codec,
					   int usec)
{
	int cfilt_k_val;
	bool set = true;
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);

	if (tabla->mbhc_data.micb_mv != VDDIO_MICBIAS_MV &&
	    tabla->mbhc_micbias_switched) {
		pr_debug("%s: set mic V to micbias V\n", __func__);
		snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_CLK_CTL, 0x2, 0x2);
		tabla_turn_onoff_override(codec, true);
		while (1) {
			cfilt_k_val = tabla_find_k_value(
						tabla->pdata->micbias.ldoh_v,
						set ? tabla->mbhc_data.micb_mv :
						      VDDIO_MICBIAS_MV);
			snd_soc_update_bits(codec,
					    tabla->mbhc_bias_regs.cfilt_val,
					    0xFC, (cfilt_k_val << 2));
			if (!set)
				break;
			usleep_range(usec, usec);
			set = false;
		}
		tabla_turn_onoff_override(codec, false);
	}
}

/* called under codec_resource_lock acquisition */
static void __tabla_codec_switch_micbias(struct snd_soc_codec *codec,
					 int vddio_switch, bool restartpolling,
					 bool checkpolling)
{
	int cfilt_k_val;
	bool override;
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);

	if (vddio_switch && !tabla->mbhc_micbias_switched &&
	    (!checkpolling || tabla->mbhc_polling_active)) {
		if (restartpolling)
			tabla_codec_pause_hs_polling(codec);
		override = snd_soc_read(codec, TABLA_A_CDC_MBHC_B1_CTL) & 0x04;
		if (!override)
			tabla_turn_onoff_override(codec, true);
		/* Adjust threshold if Mic Bias voltage changes */
		if (tabla->mbhc_data.micb_mv != VDDIO_MICBIAS_MV) {
			cfilt_k_val = tabla_find_k_value(
						   tabla->pdata->micbias.ldoh_v,
						   VDDIO_MICBIAS_MV);
			snd_soc_update_bits(codec,
					    tabla->mbhc_bias_regs.cfilt_val,
					    0xFC, (cfilt_k_val << 2));
			usleep_range(cfilt_adjust_ms * 1000,
				     cfilt_adjust_ms * 1000);
			snd_soc_write(codec, TABLA_A_CDC_MBHC_VOLT_B1_CTL,
				      tabla->mbhc_data.adj_v_ins_hu & 0xFF);
			snd_soc_write(codec, TABLA_A_CDC_MBHC_VOLT_B2_CTL,
				      (tabla->mbhc_data.adj_v_ins_hu >> 8) &
				       0xFF);
			pr_debug("%s: Programmed MBHC thresholds to VDDIO\n",
				 __func__);
		}

		/* enable MIC BIAS Switch to VDDIO */
		snd_soc_update_bits(codec, tabla->mbhc_bias_regs.mbhc_reg,
				    0x80, 0x80);
		snd_soc_update_bits(codec, tabla->mbhc_bias_regs.mbhc_reg,
				    0x10, 0x00);
		if (!override)
			tabla_turn_onoff_override(codec, false);
		if (restartpolling)
			tabla_codec_start_hs_polling(codec);

		tabla->mbhc_micbias_switched = true;
		pr_debug("%s: VDDIO switch enabled\n", __func__);
	} else if (!vddio_switch && tabla->mbhc_micbias_switched) {
		if ((!checkpolling || tabla->mbhc_polling_active) &&
		    restartpolling)
			tabla_codec_pause_hs_polling(codec);
		/* Reprogram thresholds */
		if (tabla->mbhc_data.micb_mv != VDDIO_MICBIAS_MV) {
			cfilt_k_val = tabla_find_k_value(
						   tabla->pdata->micbias.ldoh_v,
						   tabla->mbhc_data.micb_mv);
			snd_soc_update_bits(codec,
					    tabla->mbhc_bias_regs.cfilt_val,
					    0xFC, (cfilt_k_val << 2));
			usleep_range(cfilt_adjust_ms * 1000,
				     cfilt_adjust_ms * 1000);
			snd_soc_write(codec, TABLA_A_CDC_MBHC_VOLT_B1_CTL,
				      tabla->mbhc_data.v_ins_hu & 0xFF);
			snd_soc_write(codec, TABLA_A_CDC_MBHC_VOLT_B2_CTL,
				      (tabla->mbhc_data.v_ins_hu >> 8) & 0xFF);
			pr_debug("%s: Programmed MBHC thresholds to MICBIAS\n",
				 __func__);
		}

		/* Disable MIC BIAS Switch to VDDIO */
		snd_soc_update_bits(codec, tabla->mbhc_bias_regs.mbhc_reg,
				    0x80, 0x00);
		snd_soc_update_bits(codec, tabla->mbhc_bias_regs.mbhc_reg,
				    0x10, 0x00);

		if ((!checkpolling || tabla->mbhc_polling_active) &&
		    restartpolling)
			tabla_codec_start_hs_polling(codec);

		tabla->mbhc_micbias_switched = false;
		pr_debug("%s: VDDIO switch disabled\n", __func__);
	}
}

static void tabla_codec_switch_micbias(struct snd_soc_codec *codec,
				       int vddio_switch)
{
	return __tabla_codec_switch_micbias(codec, vddio_switch, true, true);
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
	const char *micbias1_text = "MIC BIAS1 ";
	const char *micbias2_text = "MIC BIAS2 ";
	const char *micbias3_text = "MIC BIAS3 ";
	const char *micbias4_text = "MIC BIAS4 ";
	u32 *micbias_enable_count;
	u16 wreg;

	pr_debug("%s %d\n", __func__, event);
	if (strnstr(w->name, micbias1_text, strlen(micbias1_text))) {
		wreg = TABLA_A_MICB_1_CTL;
		micb_int_reg = TABLA_A_MICB_1_INT_RBIAS;
		cfilt_sel_val = tabla->pdata->micbias.bias1_cfilt_sel;
		micb_line = TABLA_MICBIAS1;
	} else if (strnstr(w->name, micbias2_text, strlen(micbias2_text))) {
		wreg = TABLA_A_MICB_2_CTL;
		micb_int_reg = TABLA_A_MICB_2_INT_RBIAS;
		cfilt_sel_val = tabla->pdata->micbias.bias2_cfilt_sel;
		micb_line = TABLA_MICBIAS2;
	} else if (strnstr(w->name, micbias3_text, strlen(micbias3_text))) {
		wreg = TABLA_A_MICB_3_CTL;
		micb_int_reg = TABLA_A_MICB_3_INT_RBIAS;
		cfilt_sel_val = tabla->pdata->micbias.bias3_cfilt_sel;
		micb_line = TABLA_MICBIAS3;
	} else if (strnstr(w->name, micbias4_text, strlen(micbias4_text))) {
		wreg = tabla->reg_addr.micb_4_ctl;
		micb_int_reg = tabla->reg_addr.micb_4_int_rbias;
		cfilt_sel_val = tabla->pdata->micbias.bias4_cfilt_sel;
		micb_line = TABLA_MICBIAS4;
	} else {
		pr_err("%s: Error, invalid micbias register\n", __func__);
		return -EINVAL;
	}

	micbias_enable_count = &tabla->micbias_enable_count[micb_line];
	pr_debug("%s: counter %d\n", __func__, *micbias_enable_count);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (++*micbias_enable_count > 1) {
			pr_debug("%s: do nothing, counter %d\n",
				 __func__, *micbias_enable_count);
			break;
		}
		/* Decide whether to switch the micbias for MBHC */
		if (wreg == tabla->mbhc_bias_regs.ctl_reg) {
			TABLA_ACQUIRE_LOCK(tabla->codec_resource_lock);
			tabla_codec_switch_micbias(codec, 0);
			TABLA_RELEASE_LOCK(tabla->codec_resource_lock);
		}

		snd_soc_update_bits(codec, wreg, 0x0E, 0x0A);
		tabla_codec_update_cfilt_usage(codec, cfilt_sel_val, 1);

		if (strnstr(w->name, internal1_text, 30))
			snd_soc_update_bits(codec, micb_int_reg, 0xE0, 0xE0);
		else if (strnstr(w->name, internal2_text, 30))
			snd_soc_update_bits(codec, micb_int_reg, 0x1C, 0x1C);
		else if (strnstr(w->name, internal3_text, 30))
			snd_soc_update_bits(codec, micb_int_reg, 0x3, 0x3);

		snd_soc_update_bits(codec, wreg, 1 << 7, 1 << 7);

		break;
	case SND_SOC_DAPM_POST_PMU:
		if (*micbias_enable_count > 1) {
			pr_debug("%s: do nothing, counter %d\n",
				 __func__, *micbias_enable_count);
			break;
		}
		usleep_range(20000, 20000);

		if (tabla->mbhc_polling_active &&
		    tabla->mbhc_cfg.micbias == micb_line) {
			TABLA_ACQUIRE_LOCK(tabla->codec_resource_lock);
			tabla_codec_pause_hs_polling(codec);
			tabla_codec_start_hs_polling(codec);
			TABLA_RELEASE_LOCK(tabla->codec_resource_lock);
		}
		break;

	case SND_SOC_DAPM_POST_PMD:
		if (--*micbias_enable_count > 0) {
			pr_debug("%s: do nothing, counter %d\n",
				 __func__, *micbias_enable_count);
			break;
		}

		snd_soc_update_bits(codec, wreg, 1 << 7, 0);

		if ((wreg == tabla->mbhc_bias_regs.ctl_reg) &&
		    tabla_is_hph_pa_on(codec)) {
			TABLA_ACQUIRE_LOCK(tabla->codec_resource_lock);
			tabla_codec_switch_micbias(codec, 1);
			TABLA_RELEASE_LOCK(tabla->codec_resource_lock);
		}

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


static void tx_hpf_corner_freq_callback(struct work_struct *work)
{
	struct delayed_work *hpf_delayed_work;
	struct hpf_work *hpf_work;
	struct tabla_priv *tabla;
	struct snd_soc_codec *codec;
	u16 tx_mux_ctl_reg;
	u8 hpf_cut_of_freq;

	hpf_delayed_work = to_delayed_work(work);
	hpf_work = container_of(hpf_delayed_work, struct hpf_work, dwork);
	tabla = hpf_work->tabla;
	codec = hpf_work->tabla->codec;
	hpf_cut_of_freq = hpf_work->tx_hpf_cut_of_freq;

	tx_mux_ctl_reg = TABLA_A_CDC_TX1_MUX_CTL +
			(hpf_work->decimator - 1) * 8;

	pr_debug("%s(): decimator %u hpf_cut_of_freq 0x%x\n", __func__,
		hpf_work->decimator, (unsigned int)hpf_cut_of_freq);

	snd_soc_update_bits(codec, tx_mux_ctl_reg, 0x30, hpf_cut_of_freq << 4);
}

#define  TX_MUX_CTL_CUT_OFF_FREQ_MASK	0x30
#define  CF_MIN_3DB_4HZ			0x0
#define  CF_MIN_3DB_75HZ		0x1
#define  CF_MIN_3DB_150HZ		0x2

static int tabla_codec_enable_ldo_h(struct snd_soc_dapm_widget *w,
				    struct snd_kcontrol *kcontrol, int event);

static int tabla_codec_enable_micbias_power(struct snd_soc_dapm_widget *w,
					    struct snd_kcontrol *kcontrol,
					    int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		tabla->mbhc_cfg.mclk_cb_fn(codec, 1, true);
		tabla_codec_enable_ldo_h(w, kcontrol, event);
		tabla_codec_enable_micbias(w, kcontrol, event);
		break;
	case SND_SOC_DAPM_POST_PMU:
		tabla->mbhc_cfg.mclk_cb_fn(codec, 0, true);
		break;
	case SND_SOC_DAPM_POST_PMD:
		tabla_codec_enable_micbias(w, kcontrol, event);
		tabla_codec_enable_ldo_h(w, kcontrol, event);
		break;
	}
	return 0;
}

static int tabla_codec_enable_dec(struct snd_soc_dapm_widget *w,
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

	if (w->reg == TABLA_A_CDC_CLK_TX_CLK_EN_B1_CTL) {
		dec_reset_reg = TABLA_A_CDC_CLK_TX_RESET_B1_CTL;
		offset = 0;
	} else if (w->reg == TABLA_A_CDC_CLK_TX_CLK_EN_B2_CTL) {
		dec_reset_reg = TABLA_A_CDC_CLK_TX_RESET_B2_CTL;
		offset = 8;
	} else {
		pr_err("%s: Error, incorrect dec\n", __func__);
		return -EINVAL;
	}

	tx_vol_ctl_reg = TABLA_A_CDC_TX1_VOL_CTL_CFG + 8 * (decimator -1);
	tx_mux_ctl_reg = TABLA_A_CDC_TX1_MUX_CTL + 8 * (decimator - 1);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:

		// Enableable TX digital mute */
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
		if ((w->shift + offset) < ARRAY_SIZE(tx_digital_gain_reg))
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

static void tabla_enable_ldo_h(struct snd_soc_codec *codec, u32  enable)
{
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);

	if (enable) {
		if (++tabla->ldo_h_count == 1)
			snd_soc_update_bits(codec, TABLA_A_LDO_H_MODE_1,
					 0x80, 0x80);
	} else {
		if (--tabla->ldo_h_count == 0)
			snd_soc_update_bits(codec, TABLA_A_LDO_H_MODE_1,
				0x80, 0x00);
	}
}

static int tabla_codec_enable_ldo_h(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	pr_debug("%s %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		tabla_enable_ldo_h(codec, 1);
		usleep_range(1000, 1000);
		break;
	case SND_SOC_DAPM_POST_PMD:
		tabla_enable_ldo_h(codec, 0);
		usleep_range(1000, 1000);
		break;
	}
	return 0;
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
	snd_soc_jack_report_no_dapm(jack, status, mask);
}

static void hphocp_off_report(struct tabla_priv *tabla,
	u32 jack_status, int irq)
{
	struct snd_soc_codec *codec;
	if (!tabla) {
		pr_err("%s: Bad tabla private data\n", __func__);
		return;
	}

	pr_debug("%s: clear ocp status %x\n", __func__, jack_status);
	codec = tabla->codec;
	if (tabla->hph_status & jack_status) {
		tabla->hph_status &= ~jack_status;
		if (tabla->mbhc_cfg.headset_jack)
			tabla_snd_soc_jack_report(tabla,
						  tabla->mbhc_cfg.headset_jack,
						  tabla->hph_status,
						  TABLA_JACK_MASK);
		snd_soc_update_bits(codec, TABLA_A_RX_HPH_OCP_CTL, 0x10, 0x00);
		snd_soc_update_bits(codec, TABLA_A_RX_HPH_OCP_CTL, 0x10, 0x10);
		/* reset retry counter as PA is turned off signifying
		 * start of new OCP detection session
		 */
		if (WCD9XXX_IRQ_HPH_PA_OCPL_FAULT)
			tabla->hphlocp_cnt = 0;
		else
			tabla->hphrocp_cnt = 0;
		wcd9xxx_enable_irq(codec->control_data, irq);
	}
}

static void hphlocp_off_report(struct work_struct *work)
{
	struct tabla_priv *tabla = container_of(work, struct tabla_priv,
		hphlocp_work);
	hphocp_off_report(tabla, SND_JACK_OC_HPHL,
			  WCD9XXX_IRQ_HPH_PA_OCPL_FAULT);
}

static void hphrocp_off_report(struct work_struct *work)
{
	struct tabla_priv *tabla = container_of(work, struct tabla_priv,
		hphrocp_work);
	hphocp_off_report(tabla, SND_JACK_OC_HPHR,
			  WCD9XXX_IRQ_HPH_PA_OCPR_FAULT);
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
	u8 mbhc_micb_ctl_val;

	pr_debug("%s: DAPM Event %d ANC func is %d\n",
		 __func__, event, tabla->anc_func);

	if (tabla->anc_func == 0)
		return 0;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		mbhc_micb_ctl_val = snd_soc_read(codec,
			tabla->mbhc_bias_regs.ctl_reg);

		if (!(mbhc_micb_ctl_val & 0x80)) {
			TABLA_ACQUIRE_LOCK(tabla->codec_resource_lock);
			tabla_codec_switch_micbias(codec, 1);
			TABLA_RELEASE_LOCK(tabla->codec_resource_lock);
		}

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
		usleep_range(10000, 10000);
		snd_soc_update_bits(codec, TABLA_A_RX_HPH_CNP_EN, 0x30, 0x30);
		msleep(30);
		release_firmware(fw);
		TABLA_ACQUIRE_LOCK(tabla->codec_resource_lock);
		/* if MBHC polling is active, set TX7_MBHC_EN bit 7 */
		if (tabla->mbhc_polling_active)
			snd_soc_update_bits(codec, TABLA_A_TX_7_MBHC_EN, 0x80,
						0x80);
		TABLA_RELEASE_LOCK(tabla->codec_resource_lock);
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

		TABLA_ACQUIRE_LOCK(tabla->codec_resource_lock);
		tabla_codec_switch_micbias(codec, 0);
		TABLA_RELEASE_LOCK(tabla->codec_resource_lock);

		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, TABLA_A_RX_HPH_CNP_EN, 0x30, 0x00);
		msleep(40);
		/* unset TX7_MBHC_EN bit 7 */
		snd_soc_update_bits(codec, TABLA_A_TX_7_MBHC_EN, 0x80, 0x00);
		snd_soc_update_bits(codec, TABLA_A_CDC_ANC1_CTL, 0x01, 0x00);
		snd_soc_update_bits(codec, TABLA_A_CDC_ANC2_CTL, 0x01, 0x00);
		msleep(20);
		snd_soc_write(codec, TABLA_A_CDC_CLK_ANC_RESET_CTL, 0x0F);
		snd_soc_write(codec, TABLA_A_CDC_CLK_ANC_CLK_EN_CTL, 0);
		snd_soc_write(codec, TABLA_A_CDC_CLK_ANC_RESET_CTL, 0xFF);
		break;
	}
	return 0;
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

		if (!(mbhc_micb_ctl_val & 0x80)) {
			TABLA_ACQUIRE_LOCK(tabla->codec_resource_lock);
			tabla_codec_switch_micbias(codec, 1);
			TABLA_RELEASE_LOCK(tabla->codec_resource_lock);
		}
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

		TABLA_ACQUIRE_LOCK(tabla->codec_resource_lock);
		tabla_codec_switch_micbias(codec, 0);
		TABLA_RELEASE_LOCK(tabla->codec_resource_lock);

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
	unsigned int cfilt;

	switch (tabla->mbhc_cfg.micbias) {
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
		micbias_regs->mbhc_reg = tabla->reg_addr.micb_4_mbhc;
		micbias_regs->int_rbias = tabla->reg_addr.micb_4_int_rbias;
		micbias_regs->ctl_reg = tabla->reg_addr.micb_4_ctl;
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
		tabla->mbhc_data.micb_mv = tabla->pdata->micbias.cfilt1_mv;
		break;
	case TABLA_CFILT2_SEL:
		micbias_regs->cfilt_val = TABLA_A_MICB_CFILT_2_VAL;
		micbias_regs->cfilt_ctl = TABLA_A_MICB_CFILT_2_CTL;
		tabla->mbhc_data.micb_mv = tabla->pdata->micbias.cfilt2_mv;
		break;
	case TABLA_CFILT3_SEL:
		micbias_regs->cfilt_val = TABLA_A_MICB_CFILT_3_VAL;
		micbias_regs->cfilt_ctl = TABLA_A_MICB_CFILT_3_CTL;
		tabla->mbhc_data.micb_mv = tabla->pdata->micbias.cfilt3_mv;
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

static int tabla_ear_pa_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	pr_debug("%s %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, TABLA_A_RX_EAR_EN, 0x50, 0x50);
		break;

	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, TABLA_A_RX_EAR_EN, 0x10, 0x00);
		snd_soc_update_bits(codec, TABLA_A_RX_EAR_EN, 0x40, 0x00);
		break;
	}
	return 0;
}

static const struct snd_soc_dapm_widget tabla_1_x_dapm_widgets[] = {
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS4 External", SND_SOC_NOPM, 0,
				0, tabla_codec_enable_micbias,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
				SND_SOC_DAPM_POST_PMD),
};

static const struct snd_soc_dapm_widget tabla_2_higher_dapm_widgets[] = {
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS4 External", SND_SOC_NOPM, 0,
				0, tabla_codec_enable_micbias,
				SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
				SND_SOC_DAPM_POST_PMD),
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

	{"AIF1 CAP", NULL, "AIF1_CAP Mixer"},
	{"AIF2 CAP", NULL, "AIF2_CAP Mixer"},
	{"AIF3 CAP", NULL, "AIF3_CAP Mixer"},

	/* SLIM_MIXER("AIF1_CAP Mixer"),*/
	{"AIF1_CAP Mixer", "SLIM TX1", "SLIM TX1 MUX"},
	{"AIF1_CAP Mixer", "SLIM TX2", "SLIM TX2 MUX"},
	{"AIF1_CAP Mixer", "SLIM TX3", "SLIM TX3 MUX"},
	{"AIF1_CAP Mixer", "SLIM TX4", "SLIM TX4 MUX"},
	{"AIF1_CAP Mixer", "SLIM TX5", "SLIM TX5 MUX"},
	{"AIF1_CAP Mixer", "SLIM TX6", "SLIM TX6 MUX"},
	{"AIF1_CAP Mixer", "SLIM TX7", "SLIM TX7 MUX"},
	{"AIF1_CAP Mixer", "SLIM TX8", "SLIM TX8 MUX"},
	{"AIF1_CAP Mixer", "SLIM TX9", "SLIM TX9 MUX"},
	{"AIF1_CAP Mixer", "SLIM TX10", "SLIM TX10 MUX"},
	/* SLIM_MIXER("AIF2_CAP Mixer"),*/
	{"AIF2_CAP Mixer", "SLIM TX1", "SLIM TX1 MUX"},
	{"AIF2_CAP Mixer", "SLIM TX2", "SLIM TX2 MUX"},
	{"AIF2_CAP Mixer", "SLIM TX3", "SLIM TX3 MUX"},
	{"AIF2_CAP Mixer", "SLIM TX4", "SLIM TX4 MUX"},
	{"AIF2_CAP Mixer", "SLIM TX5", "SLIM TX5 MUX"},
	{"AIF2_CAP Mixer", "SLIM TX6", "SLIM TX6 MUX"},
	{"AIF2_CAP Mixer", "SLIM TX7", "SLIM TX7 MUX"},
	{"AIF2_CAP Mixer", "SLIM TX8", "SLIM TX8 MUX"},
	{"AIF2_CAP Mixer", "SLIM TX9", "SLIM TX9 MUX"},
	{"AIF2_CAP Mixer", "SLIM TX10", "SLIM TX10 MUX"},
	/* SLIM_MIXER("AIF3_CAP Mixer"),*/
	{"AIF3_CAP Mixer", "SLIM TX1", "SLIM TX1 MUX"},
	{"AIF3_CAP Mixer", "SLIM TX2", "SLIM TX2 MUX"},
	{"AIF3_CAP Mixer", "SLIM TX3", "SLIM TX3 MUX"},
	{"AIF3_CAP Mixer", "SLIM TX4", "SLIM TX4 MUX"},
	{"AIF3_CAP Mixer", "SLIM TX5", "SLIM TX5 MUX"},
	{"AIF3_CAP Mixer", "SLIM TX6", "SLIM TX6 MUX"},
	{"AIF3_CAP Mixer", "SLIM TX7", "SLIM TX7 MUX"},
	{"AIF3_CAP Mixer", "SLIM TX8", "SLIM TX8 MUX"},
	{"AIF3_CAP Mixer", "SLIM TX9", "SLIM TX9 MUX"},
	{"AIF3_CAP Mixer", "SLIM TX10", "SLIM TX10 MUX"},

	{"SLIM TX1 MUX", "DEC1", "DEC1 MUX"},
	{"SLIM TX2 MUX", "DEC2", "DEC2 MUX"},

	{"SLIM TX3 MUX", "DEC3", "DEC3 MUX"},
	{"SLIM TX3 MUX", "RMIX1", "RX1 MIX1"},
	{"SLIM TX3 MUX", "RMIX2", "RX2 MIX1"},
	{"SLIM TX3 MUX", "RMIX3", "RX3 MIX1"},
	{"SLIM TX3 MUX", "RMIX4", "RX4 MIX1"},
	{"SLIM TX3 MUX", "RMIX5", "RX5 MIX1"},
	{"SLIM TX3 MUX", "RMIX6", "RX6 MIX1"},
	{"SLIM TX3 MUX", "RMIX7", "RX7 MIX1"},

	{"SLIM TX4 MUX", "DEC4", "DEC4 MUX"},

	{"SLIM TX5 MUX", "DEC5", "DEC5 MUX"},
	{"SLIM TX5 MUX", "RMIX1", "RX1 MIX1"},
	{"SLIM TX5 MUX", "RMIX2", "RX2 MIX1"},
	{"SLIM TX5 MUX", "RMIX3", "RX3 MIX1"},
	{"SLIM TX5 MUX", "RMIX4", "RX4 MIX1"},
	{"SLIM TX5 MUX", "RMIX5", "RX5 MIX1"},
	{"SLIM TX5 MUX", "RMIX6", "RX6 MIX1"},
	{"SLIM TX5 MUX", "RMIX7", "RX7 MIX1"},

	{"SLIM TX6 MUX", "DEC6", "DEC6 MUX"},

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

	{"ANC1 FB MUX", "EAR_HPH_L", "RX1 MIX2"},
	{"ANC1 FB MUX", "EAR_LINE_1", "RX2 MIX2"},

	/* Headset (RX MIX1 and RX MIX2) */
	{"HEADPHONE", NULL, "HPHL"},
	{"HEADPHONE", NULL, "HPHR"},

	{"HPHL", NULL, "HPHL_PA_MIXER"},
	{"HPHL_PA_MIXER", NULL, "HPHL DAC"},

	{"HPHR", NULL, "HPHR_PA_MIXER"},
	{"HPHR_PA_MIXER", NULL, "HPHR DAC"},

	{"HPHL DAC", NULL, "CP"},
	{"HPHR DAC", NULL, "CP"},

	{"ANC HEADPHONE", NULL, "ANC HPHL"},
	{"ANC HEADPHONE", NULL, "ANC HPHR"},

	{"ANC HPHL", NULL, "HPHL_PA_MIXER"},
	{"ANC HPHR", NULL, "HPHR_PA_MIXER"},

	{"ANC1 MUX", "ADC1", "ADC1"},
	{"ANC1 MUX", "ADC2", "ADC2"},
	{"ANC1 MUX", "ADC3", "ADC3"},
	{"ANC1 MUX", "ADC4", "ADC4"},
	{"ANC1 MUX", "DMIC1", "DMIC1"},
	{"ANC1 MUX", "DMIC2", "DMIC2"},
	{"ANC1 MUX", "DMIC3", "DMIC3"},
	{"ANC1 MUX", "DMIC4", "DMIC4"},
	{"ANC2 MUX", "ADC1", "ADC1"},
	{"ANC2 MUX", "ADC2", "ADC2"},
	{"ANC2 MUX", "ADC3", "ADC3"},
	{"ANC2 MUX", "ADC4", "ADC4"},

	{"ANC HPHR", NULL, "CDC_CONN"},
	{"DAC1", "Switch", "RX1 CHAIN"},
	{"HPHL DAC", "Switch", "RX1 CHAIN"},
	{"HPHR DAC", NULL, "RX2 CHAIN"},

	{"LINEOUT1", NULL, "LINEOUT1 PA"},
	{"LINEOUT2", NULL, "LINEOUT2 PA"},
	{"LINEOUT3", NULL, "LINEOUT3 PA"},
	{"LINEOUT4", NULL, "LINEOUT4 PA"},
	{"LINEOUT5", NULL, "LINEOUT5 PA"},

	{"LINEOUT1 PA", NULL, "LINEOUT1_PA_MIXER"},
	{"LINEOUT1_PA_MIXER", NULL, "LINEOUT1 DAC"},
	{"LINEOUT2 PA", NULL, "LINEOUT2_PA_MIXER"},
	{"LINEOUT2_PA_MIXER", NULL, "LINEOUT2 DAC"},
	{"LINEOUT3 PA", NULL, "LINEOUT3_PA_MIXER"},
	{"LINEOUT3_PA_MIXER", NULL, "LINEOUT3 DAC"},
	{"LINEOUT4 PA", NULL, "LINEOUT4_PA_MIXER"},
	{"LINEOUT4_PA_MIXER", NULL, "LINEOUT4 DAC"},
	{"LINEOUT5 PA", NULL, "LINEOUT5_PA_MIXER"},
	{"LINEOUT5_PA_MIXER", NULL, "LINEOUT5 DAC"},

	{"LINEOUT1 DAC", NULL, "RX3 MIX2"},
	{"LINEOUT5 DAC", NULL, "RX7 MIX1"},

	{"RX1 CHAIN", NULL, "RX1 MIX2"},
	{"RX2 CHAIN", NULL, "RX2 MIX2"},
	{"RX1 MIX2", NULL, "ANC1 MUX"},
	{"RX2 MIX2", NULL, "ANC2 MUX"},

	{"CP", NULL, "RX_BIAS"},
	{"LINEOUT1 DAC", NULL, "RX_BIAS"},
	{"LINEOUT2 DAC", NULL, "RX_BIAS"},
	{"LINEOUT3 DAC", NULL, "RX_BIAS"},
	{"LINEOUT4 DAC", NULL, "RX_BIAS"},
	{"LINEOUT5 DAC", NULL, "RX_BIAS"},

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
	{"RX3 MIX2", NULL, "RX3 MIX1"},
	{"RX3 MIX2", NULL, "RX3 MIX2 INP1"},
	{"RX3 MIX2", NULL, "RX3 MIX2 INP2"},

	/* SLIM_MUX("AIF1_PB", "AIF1 PB"),*/
	{"SLIM RX1 MUX", "AIF1_PB", "AIF1 PB"},
	{"SLIM RX2 MUX", "AIF1_PB", "AIF1 PB"},
	{"SLIM RX3 MUX", "AIF1_PB", "AIF1 PB"},
	{"SLIM RX4 MUX", "AIF1_PB", "AIF1 PB"},
	{"SLIM RX5 MUX", "AIF1_PB", "AIF1 PB"},
	{"SLIM RX6 MUX", "AIF1_PB", "AIF1 PB"},
	{"SLIM RX7 MUX", "AIF1_PB", "AIF1 PB"},
	/* SLIM_MUX("AIF2_PB", "AIF2 PB"),*/
	{"SLIM RX1 MUX", "AIF2_PB", "AIF2 PB"},
	{"SLIM RX2 MUX", "AIF2_PB", "AIF2 PB"},
	{"SLIM RX3 MUX", "AIF2_PB", "AIF2 PB"},
	{"SLIM RX4 MUX", "AIF2_PB", "AIF2 PB"},
	{"SLIM RX5 MUX", "AIF2_PB", "AIF2 PB"},
	{"SLIM RX6 MUX", "AIF2_PB", "AIF2 PB"},
	{"SLIM RX7 MUX", "AIF2_PB", "AIF2 PB"},
	/* SLIM_MUX("AIF3_PB", "AIF3 PB"),*/
	{"SLIM RX1 MUX", "AIF3_PB", "AIF3 PB"},
	{"SLIM RX2 MUX", "AIF3_PB", "AIF3 PB"},
	{"SLIM RX3 MUX", "AIF3_PB", "AIF3 PB"},
	{"SLIM RX4 MUX", "AIF3_PB", "AIF3 PB"},
	{"SLIM RX5 MUX", "AIF3_PB", "AIF3 PB"},
	{"SLIM RX6 MUX", "AIF3_PB", "AIF3 PB"},
	{"SLIM RX7 MUX", "AIF3_PB", "AIF3 PB"},

	{"SLIM RX1", NULL, "SLIM RX1 MUX"},
	{"SLIM RX2", NULL, "SLIM RX2 MUX"},
	{"SLIM RX3", NULL, "SLIM RX3 MUX"},
	{"SLIM RX4", NULL, "SLIM RX4 MUX"},
	{"SLIM RX5", NULL, "SLIM RX5 MUX"},
	{"SLIM RX6", NULL, "SLIM RX6 MUX"},
	{"SLIM RX7", NULL, "SLIM RX7 MUX"},

	/* Mixer control for output path */
	{"RX1 MIX1 INP1", "RX1", "SLIM RX1"},
	{"RX1 MIX1 INP1", "RX2", "SLIM RX2"},
	{"RX1 MIX1 INP1", "RX3", "SLIM RX3"},
	{"RX1 MIX1 INP1", "RX4", "SLIM RX4"},
	{"RX1 MIX1 INP1", "RX5", "SLIM RX5"},
	{"RX1 MIX1 INP1", "RX6", "SLIM RX6"},
	{"RX1 MIX1 INP1", "RX7", "SLIM RX7"},
	{"RX1 MIX1 INP1", "IIR1", "IIR1"},
	{"RX1 MIX1 INP1", "IIR2", "IIR2"},
	{"RX1 MIX1 INP2", "RX1", "SLIM RX1"},
	{"RX1 MIX1 INP2", "RX2", "SLIM RX2"},
	{"RX1 MIX1 INP2", "RX3", "SLIM RX3"},
	{"RX1 MIX1 INP2", "RX4", "SLIM RX4"},
	{"RX1 MIX1 INP2", "RX5", "SLIM RX5"},
	{"RX1 MIX1 INP2", "RX6", "SLIM RX6"},
	{"RX1 MIX1 INP2", "RX7", "SLIM RX7"},
	{"RX1 MIX1 INP2", "IIR1", "IIR1"},
	{"RX1 MIX1 INP2", "IIR2", "IIR2"},
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
	{"RX2 MIX1 INP1", "IIR2", "IIR2"},
	{"RX2 MIX1 INP2", "RX1", "SLIM RX1"},
	{"RX2 MIX1 INP2", "RX2", "SLIM RX2"},
	{"RX2 MIX1 INP2", "RX3", "SLIM RX3"},
	{"RX2 MIX1 INP2", "RX4", "SLIM RX4"},
	{"RX2 MIX1 INP2", "RX5", "SLIM RX5"},
	{"RX2 MIX1 INP2", "RX6", "SLIM RX6"},
	{"RX2 MIX1 INP2", "RX7", "SLIM RX7"},
	{"RX2 MIX1 INP2", "IIR1", "IIR1"},
	{"RX2 MIX1 INP2", "IIR2", "IIR2"},
	{"RX3 MIX1 INP1", "RX1", "SLIM RX1"},
	{"RX3 MIX1 INP1", "RX2", "SLIM RX2"},
	{"RX3 MIX1 INP1", "RX3", "SLIM RX3"},
	{"RX3 MIX1 INP1", "RX4", "SLIM RX4"},
	{"RX3 MIX1 INP1", "RX5", "SLIM RX5"},
	{"RX3 MIX1 INP1", "RX6", "SLIM RX6"},
	{"RX3 MIX1 INP1", "RX7", "SLIM RX7"},
	{"RX3 MIX1 INP1", "IIR1", "IIR1"},
	{"RX3 MIX1 INP1", "IIR2", "IIR2"},
	{"RX3 MIX1 INP2", "RX1", "SLIM RX1"},
	{"RX3 MIX1 INP2", "RX2", "SLIM RX2"},
	{"RX3 MIX1 INP2", "RX3", "SLIM RX3"},
	{"RX3 MIX1 INP2", "RX4", "SLIM RX4"},
	{"RX3 MIX1 INP2", "RX5", "SLIM RX5"},
	{"RX3 MIX1 INP2", "RX6", "SLIM RX6"},
	{"RX3 MIX1 INP2", "RX7", "SLIM RX7"},
	{"RX3 MIX1 INP2", "IIR1", "IIR1"},
	{"RX3 MIX1 INP2", "IIR2", "IIR2"},
	{"RX4 MIX1 INP1", "RX1", "SLIM RX1"},
	{"RX4 MIX1 INP1", "RX2", "SLIM RX2"},
	{"RX4 MIX1 INP1", "RX3", "SLIM RX3"},
	{"RX4 MIX1 INP1", "RX4", "SLIM RX4"},
	{"RX4 MIX1 INP1", "RX5", "SLIM RX5"},
	{"RX4 MIX1 INP1", "RX6", "SLIM RX6"},
	{"RX4 MIX1 INP1", "RX7", "SLIM RX7"},
	{"RX4 MIX1 INP1", "IIR1", "IIR1"},
	{"RX4 MIX1 INP1", "IIR2", "IIR2"},
	{"RX4 MIX1 INP2", "RX1", "SLIM RX1"},
	{"RX4 MIX1 INP2", "RX2", "SLIM RX2"},
	{"RX4 MIX1 INP2", "RX3", "SLIM RX3"},
	{"RX4 MIX1 INP2", "RX5", "SLIM RX5"},
	{"RX4 MIX1 INP2", "RX4", "SLIM RX4"},
	{"RX4 MIX1 INP2", "RX6", "SLIM RX6"},
	{"RX4 MIX1 INP2", "RX7", "SLIM RX7"},
	{"RX4 MIX1 INP2", "IIR1", "IIR1"},
	{"RX4 MIX1 INP2", "IIR2", "IIR2"},
	{"RX5 MIX1 INP1", "RX1", "SLIM RX1"},
	{"RX5 MIX1 INP1", "RX2", "SLIM RX2"},
	{"RX5 MIX1 INP1", "RX3", "SLIM RX3"},
	{"RX5 MIX1 INP1", "RX4", "SLIM RX4"},
	{"RX5 MIX1 INP1", "RX5", "SLIM RX5"},
	{"RX5 MIX1 INP1", "RX6", "SLIM RX6"},
	{"RX5 MIX1 INP1", "RX7", "SLIM RX7"},
	{"RX5 MIX1 INP1", "IIR1", "IIR1"},
	{"RX5 MIX1 INP1", "IIR2", "IIR2"},
	{"RX5 MIX1 INP2", "RX1", "SLIM RX1"},
	{"RX5 MIX1 INP2", "RX2", "SLIM RX2"},
	{"RX5 MIX1 INP2", "RX3", "SLIM RX3"},
	{"RX5 MIX1 INP2", "RX4", "SLIM RX4"},
	{"RX5 MIX1 INP2", "RX5", "SLIM RX5"},
	{"RX5 MIX1 INP2", "RX6", "SLIM RX6"},
	{"RX5 MIX1 INP2", "RX7", "SLIM RX7"},
	{"RX5 MIX1 INP2", "IIR1", "IIR1"},
	{"RX5 MIX1 INP2", "IIR2", "IIR2"},
	{"RX6 MIX1 INP1", "RX1", "SLIM RX1"},
	{"RX6 MIX1 INP1", "RX2", "SLIM RX2"},
	{"RX6 MIX1 INP1", "RX3", "SLIM RX3"},
	{"RX6 MIX1 INP1", "RX4", "SLIM RX4"},
	{"RX6 MIX1 INP1", "RX5", "SLIM RX5"},
	{"RX6 MIX1 INP1", "RX6", "SLIM RX6"},
	{"RX6 MIX1 INP1", "RX7", "SLIM RX7"},
	{"RX6 MIX1 INP1", "IIR1", "IIR1"},
	{"RX6 MIX1 INP1", "IIR2", "IIR2"},
	{"RX6 MIX1 INP2", "RX1", "SLIM RX1"},
	{"RX6 MIX1 INP2", "RX2", "SLIM RX2"},
	{"RX6 MIX1 INP2", "RX3", "SLIM RX3"},
	{"RX6 MIX1 INP2", "RX4", "SLIM RX4"},
	{"RX6 MIX1 INP2", "RX5", "SLIM RX5"},
	{"RX6 MIX1 INP2", "RX6", "SLIM RX6"},
	{"RX6 MIX1 INP2", "RX7", "SLIM RX7"},
	{"RX6 MIX1 INP2", "IIR1", "IIR1"},
	{"RX6 MIX1 INP2", "IIR2", "IIR2"},
	{"RX7 MIX1 INP1", "RX1", "SLIM RX1"},
	{"RX7 MIX1 INP1", "RX2", "SLIM RX2"},
	{"RX7 MIX1 INP1", "RX3", "SLIM RX3"},
	{"RX7 MIX1 INP1", "RX4", "SLIM RX4"},
	{"RX7 MIX1 INP1", "RX5", "SLIM RX5"},
	{"RX7 MIX1 INP1", "RX6", "SLIM RX6"},
	{"RX7 MIX1 INP1", "RX7", "SLIM RX7"},
	{"RX7 MIX1 INP1", "IIR1", "IIR1"},
	{"RX7 MIX1 INP1", "IIR2", "IIR2"},
	{"RX7 MIX1 INP2", "RX1", "SLIM RX1"},
	{"RX7 MIX1 INP2", "RX2", "SLIM RX2"},
	{"RX7 MIX1 INP2", "RX3", "SLIM RX3"},
	{"RX7 MIX1 INP2", "RX4", "SLIM RX4"},
	{"RX7 MIX1 INP2", "RX5", "SLIM RX5"},
	{"RX7 MIX1 INP2", "RX6", "SLIM RX6"},
	{"RX7 MIX1 INP2", "RX7", "SLIM RX7"},
	{"RX7 MIX1 INP2", "IIR1", "IIR1"},
	{"RX7 MIX1 INP2", "IIR2", "IIR2"},

	{"RX1 MIX2 INP1", "IIR1", "IIR1"},
	{"RX1 MIX2 INP1", "IIR2", "IIR2"},
	{"RX1 MIX2 INP2", "IIR1", "IIR1"},
	{"RX1 MIX2 INP2", "IIR2", "IIR2"},
	{"RX2 MIX2 INP1", "IIR1", "IIR1"},
	{"RX2 MIX2 INP1", "IIR2", "IIR2"},
	{"RX2 MIX2 INP2", "IIR1", "IIR1"},
	{"RX2 MIX2 INP2", "IIR2", "IIR2"},
	{"RX3 MIX2 INP1", "IIR1", "IIR1"},
	{"RX3 MIX2 INP1", "IIR2", "IIR2"},
	{"RX3 MIX2 INP2", "IIR1", "IIR1"},
	{"RX3 MIX2 INP2", "IIR2", "IIR2"},

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
	{"HPHL_PA_MIXER", "AUX_PGA_L Switch", "AUX_PGA_Left"},
	{"HPHL_PA_MIXER", "AUX_PGA_R Switch", "AUX_PGA_Right"},
	{"HPHL_PA_MIXER", "AUX_PGA_L_INV Switch", "AUX_PGA_Left"},
	{"HPHL_PA_MIXER", "AUX_PGA_R_INV Switch", "AUX_PGA_Right"},
	{"HPHR_PA_MIXER", "AUX_PGA_L Switch", "AUX_PGA_Left"},
	{"HPHR_PA_MIXER", "AUX_PGA_R Switch", "AUX_PGA_Right"},
	{"HPHR_PA_MIXER", "AUX_PGA_L_INV Switch", "AUX_PGA_Left"},
	{"HPHR_PA_MIXER", "AUX_PGA_R_INV Switch", "AUX_PGA_Right"},
	{"LINEOUT1_PA_MIXER", "AUX_PGA_L Switch", "AUX_PGA_Left"},
	{"LINEOUT1_PA_MIXER", "AUX_PGA_R Switch", "AUX_PGA_Right"},
	{"LINEOUT1_PA_MIXER", "AUX_PGA_L_INV Switch", "AUX_PGA_Left"},
	{"LINEOUT1_PA_MIXER", "AUX_PGA_R_INV Switch", "AUX_PGA_Right"},
	{"LINEOUT2_PA_MIXER", "AUX_PGA_L Switch", "AUX_PGA_Left"},
	{"LINEOUT2_PA_MIXER", "AUX_PGA_R Switch", "AUX_PGA_Right"},
	{"LINEOUT2_PA_MIXER", "AUX_PGA_L_INV Switch", "AUX_PGA_Left"},
	{"LINEOUT2_PA_MIXER", "AUX_PGA_R_INV Switch", "AUX_PGA_Right"},
	{"LINEOUT3_PA_MIXER", "AUX_PGA_L Switch", "AUX_PGA_Left"},
	{"LINEOUT3_PA_MIXER", "AUX_PGA_R Switch", "AUX_PGA_Right"},
	{"LINEOUT3_PA_MIXER", "AUX_PGA_L_INV Switch", "AUX_PGA_Left"},
	{"LINEOUT3_PA_MIXER", "AUX_PGA_R_INV Switch", "AUX_PGA_Right"},
	{"LINEOUT4_PA_MIXER", "AUX_PGA_L Switch", "AUX_PGA_Left"},
	{"LINEOUT4_PA_MIXER", "AUX_PGA_R Switch", "AUX_PGA_Right"},
	{"LINEOUT4_PA_MIXER", "AUX_PGA_L_INV Switch", "AUX_PGA_Left"},
	{"LINEOUT4_PA_MIXER", "AUX_PGA_R_INV Switch", "AUX_PGA_Right"},
	{"LINEOUT5_PA_MIXER", "AUX_PGA_L Switch", "AUX_PGA_Left"},
	{"LINEOUT5_PA_MIXER", "AUX_PGA_R Switch", "AUX_PGA_Right"},
	{"LINEOUT5_PA_MIXER", "AUX_PGA_L_INV Switch", "AUX_PGA_Left"},
	{"LINEOUT5_PA_MIXER", "AUX_PGA_R_INV Switch", "AUX_PGA_Right"},
	{"EAR_PA_MIXER", "AUX_PGA_L Switch", "AUX_PGA_Left"},
	{"EAR_PA_MIXER", "AUX_PGA_R Switch", "AUX_PGA_Right"},
	{"EAR_PA_MIXER", "AUX_PGA_L_INV Switch", "AUX_PGA_Left"},
	{"EAR_PA_MIXER", "AUX_PGA_R_INV Switch", "AUX_PGA_Right"},
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

	{"IIR2", NULL, "IIR2 INP1 MUX"},
	{"IIR2 INP1 MUX", "DEC1", "DEC1 MUX"},
	{"IIR2 INP1 MUX", "DEC2", "DEC2 MUX"},
	{"IIR2 INP1 MUX", "DEC3", "DEC3 MUX"},
	{"IIR2 INP1 MUX", "DEC4", "DEC4 MUX"},
	{"IIR2 INP1 MUX", "DEC5", "DEC5 MUX"},
	{"IIR2 INP1 MUX", "DEC6", "DEC6 MUX"},
	{"IIR2 INP1 MUX", "DEC7", "DEC7 MUX"},
	{"IIR2 INP1 MUX", "DEC8", "DEC8 MUX"},
	{"IIR2 INP1 MUX", "DEC9", "DEC9 MUX"},
	{"IIR2 INP1 MUX", "DEC10", "DEC10 MUX"},

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

	{"RX4 DSM MUX", "DSM_INV", "RX3 MIX2"},
	{"RX4 DSM MUX", "CIC_OUT", "RX4 MIX1"},

	{"LINEOUT2 DAC", NULL, "RX4 DSM MUX"},

	{"LINEOUT3 DAC", NULL, "RX5 MIX1"},
	{"LINEOUT3 DAC GROUND", "Switch", "RX3 MIX2"},
	{"LINEOUT3 DAC", NULL, "LINEOUT3 DAC GROUND"},

	{"RX6 DSM MUX", "DSM_INV", "RX5 MIX1"},
	{"RX6 DSM MUX", "CIC_OUT", "RX6 MIX1"},

	{"LINEOUT4 DAC", NULL, "RX6 DSM MUX"},
	{"LINEOUT4 DAC GROUND", "Switch", "RX4 DSM MUX"},
	{"LINEOUT4 DAC", NULL, "LINEOUT4 DAC GROUND"},
};


static const struct snd_soc_dapm_route tabla_2_x_lineout_2_to_4_map[] = {

	{"RX4 DSM MUX", "DSM_INV", "RX3 MIX2"},
	{"RX4 DSM MUX", "CIC_OUT", "RX4 MIX1"},

	{"LINEOUT3 DAC", NULL, "RX4 DSM MUX"},

	{"LINEOUT2 DAC", NULL, "RX5 MIX1"},

	{"RX6 DSM MUX", "DSM_INV", "RX5 MIX1"},
	{"RX6 DSM MUX", "CIC_OUT", "RX6 MIX1"},

	{"LINEOUT4 DAC", NULL, "RX6 DSM MUX"},
};

static int tabla_readable(struct snd_soc_codec *ssc, unsigned int reg)
{
	int i;
	struct wcd9xxx *tabla_core = dev_get_drvdata(ssc->dev->parent);

	if (TABLA_IS_1_X(tabla_core->version)) {
		for (i = 0; i < ARRAY_SIZE(tabla_1_reg_readable); i++) {
			if (tabla_1_reg_readable[i] == reg)
				return 1;
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(tabla_2_reg_readable); i++) {
			if (tabla_2_reg_readable[i] == reg)
				return 1;
		}
	}

	return tabla_reg_readable[reg];
}
static bool tabla_is_digital_gain_register(unsigned int reg)
{
	bool rtn = false;
	switch (reg) {
	case TABLA_A_CDC_RX1_VOL_CTL_B2_CTL:
	case TABLA_A_CDC_RX2_VOL_CTL_B2_CTL:
	case TABLA_A_CDC_RX3_VOL_CTL_B2_CTL:
	case TABLA_A_CDC_RX4_VOL_CTL_B2_CTL:
	case TABLA_A_CDC_RX5_VOL_CTL_B2_CTL:
	case TABLA_A_CDC_RX6_VOL_CTL_B2_CTL:
	case TABLA_A_CDC_RX7_VOL_CTL_B2_CTL:
	case TABLA_A_CDC_TX1_VOL_CTL_GAIN:
	case TABLA_A_CDC_TX2_VOL_CTL_GAIN:
	case TABLA_A_CDC_TX3_VOL_CTL_GAIN:
	case TABLA_A_CDC_TX4_VOL_CTL_GAIN:
	case TABLA_A_CDC_TX5_VOL_CTL_GAIN:
	case TABLA_A_CDC_TX6_VOL_CTL_GAIN:
	case TABLA_A_CDC_TX7_VOL_CTL_GAIN:
	case TABLA_A_CDC_TX8_VOL_CTL_GAIN:
	case TABLA_A_CDC_TX9_VOL_CTL_GAIN:
	case TABLA_A_CDC_TX10_VOL_CTL_GAIN:
		rtn = true;
		break;
	default:
		break;
	}
	return rtn;
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

	/* ANC filter registers are not cacheable */
	if ((reg >= TABLA_A_CDC_ANC1_FILT1_B1_CTL) &&
		(reg <= TABLA_A_CDC_ANC1_FILT2_B3_CTL))
		return 1;
	if ((reg >= TABLA_A_CDC_ANC2_FILT1_B1_CTL) &&
		(reg <= TABLA_A_CDC_ANC2_FILT2_B3_CTL))
		return 1;

	/* Digital gain register is not cacheable so we have to write
	 * the setting even it is the same
	 */
	if (tabla_is_digital_gain_register(reg))
		return 1;

	/* HPH status registers */
	if (reg == TABLA_A_RX_HPH_L_STATUS || reg == TABLA_A_RX_HPH_R_STATUS)
		return 1;

	if (reg == TABLA_A_CDC_COMP1_SHUT_DOWN_STATUS ||
	    reg == TABLA_A_CDC_COMP2_SHUT_DOWN_STATUS)
		return 1;

	return 0;
}

#define TABLA_FORMATS (SNDRV_PCM_FMTBIT_S16_LE)
static int tabla_write(struct snd_soc_codec *codec, unsigned int reg,
	unsigned int value)
{
	int ret;

	if (reg == SND_SOC_NOPM)
		return 0;

	BUG_ON(reg > TABLA_MAX_REGISTER);

	if (!tabla_volatile(codec, reg)) {
		ret = snd_soc_cache_write(codec, reg, value);
		if (ret != 0)
			dev_err(codec->dev, "Cache write to %x failed: %d\n",
				reg, ret);
	}

	return wcd9xxx_reg_write(codec->control_data, reg, value);
}
static unsigned int tabla_read(struct snd_soc_codec *codec,
				unsigned int reg)
{
	unsigned int val;
	int ret;

	if (reg == SND_SOC_NOPM)
		return 0;

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

	val = wcd9xxx_reg_read(codec->control_data, reg);
	return val;
}

static s16 tabla_get_current_v_ins(struct tabla_priv *tabla, bool hu)
{
	s16 v_ins;
	if ((tabla->mbhc_data.micb_mv != VDDIO_MICBIAS_MV) &&
	    tabla->mbhc_micbias_switched)
		v_ins = hu ? (s16)tabla->mbhc_data.adj_v_ins_hu :
			     (s16)tabla->mbhc_data.adj_v_ins_h;
	else
		v_ins = hu ? (s16)tabla->mbhc_data.v_ins_hu :
			     (s16)tabla->mbhc_data.v_ins_h;
	return v_ins;
}

static s16 tabla_get_current_v_hs_max(struct tabla_priv *tabla)
{
	s16 v_hs_max;
	struct tabla_mbhc_plug_type_cfg *plug_type;

	plug_type = TABLA_MBHC_CAL_PLUG_TYPE_PTR(tabla->mbhc_cfg.calibration);
	if ((tabla->mbhc_data.micb_mv != VDDIO_MICBIAS_MV) &&
	    tabla->mbhc_micbias_switched)
		v_hs_max = tabla->mbhc_data.adj_v_hs_max;
	else
		v_hs_max = plug_type->v_hs_max;
	return v_hs_max;
}

static void tabla_codec_calibrate_rel(struct snd_soc_codec *codec)
{
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);

	snd_soc_write(codec, TABLA_A_CDC_MBHC_VOLT_B3_CTL,
		      tabla->mbhc_data.v_b1_hu & 0xFF);
	snd_soc_write(codec, TABLA_A_CDC_MBHC_VOLT_B4_CTL,
		      (tabla->mbhc_data.v_b1_hu >> 8) & 0xFF);

	snd_soc_write(codec, TABLA_A_CDC_MBHC_VOLT_B5_CTL,
		      tabla->mbhc_data.v_b1_h & 0xFF);
	snd_soc_write(codec, TABLA_A_CDC_MBHC_VOLT_B6_CTL,
		      (tabla->mbhc_data.v_b1_h >> 8) & 0xFF);

	snd_soc_write(codec, TABLA_A_CDC_MBHC_VOLT_B9_CTL,
		      tabla->mbhc_data.v_brh & 0xFF);
	snd_soc_write(codec, TABLA_A_CDC_MBHC_VOLT_B10_CTL,
		      (tabla->mbhc_data.v_brh >> 8) & 0xFF);

	snd_soc_write(codec, TABLA_A_CDC_MBHC_VOLT_B11_CTL,
		      tabla->mbhc_data.v_brl & 0xFF);
	snd_soc_write(codec, TABLA_A_CDC_MBHC_VOLT_B12_CTL,
		      (tabla->mbhc_data.v_brl >> 8) & 0xFF);
}

static void tabla_codec_calibrate_hs_polling(struct snd_soc_codec *codec)
{
	u8 *n_ready, *n_cic;
	struct tabla_mbhc_btn_detect_cfg *btn_det;
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);
	const s16 v_ins_hu = tabla_get_current_v_ins(tabla, true);

	btn_det = TABLA_MBHC_CAL_BTN_DET_PTR(tabla->mbhc_cfg.calibration);

	snd_soc_write(codec, TABLA_A_CDC_MBHC_VOLT_B1_CTL,
		      v_ins_hu & 0xFF);
	snd_soc_write(codec, TABLA_A_CDC_MBHC_VOLT_B2_CTL,
		      (v_ins_hu >> 8) & 0xFF);

	tabla_codec_calibrate_rel(codec);

	n_ready = tabla_mbhc_cal_btn_det_mp(btn_det, TABLA_BTN_DET_N_READY);
	snd_soc_write(codec, TABLA_A_CDC_MBHC_TIMER_B1_CTL,
		      n_ready[tabla_codec_mclk_index(tabla)]);
	snd_soc_write(codec, TABLA_A_CDC_MBHC_TIMER_B2_CTL,
		      tabla->mbhc_data.npoll);
	snd_soc_write(codec, TABLA_A_CDC_MBHC_TIMER_B3_CTL,
		      tabla->mbhc_data.nbounce_wait);
	n_cic = tabla_mbhc_cal_btn_det_mp(btn_det, TABLA_BTN_DET_N_CIC);
	snd_soc_write(codec, TABLA_A_CDC_MBHC_TIMER_B6_CTL,
		      n_cic[tabla_codec_mclk_index(tabla)]);
}

static int tabla_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct wcd9xxx *tabla_core = dev_get_drvdata(dai->codec->dev->parent);
	pr_debug("%s(): substream = %s  stream = %d\n" , __func__,
		 substream->name, substream->stream);
	if ((tabla_core != NULL) &&
	    (tabla_core->dev != NULL) &&
	    (tabla_core->dev->parent != NULL))
		pm_runtime_get_sync(tabla_core->dev->parent);

	return 0;
}

static void tabla_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct wcd9xxx *tabla_core = dev_get_drvdata(dai->codec->dev->parent);
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(dai->codec);
	u32 active = 0;

	pr_debug("%s(): substream = %s  stream = %d\n" , __func__,
		 substream->name, substream->stream);
	if (tabla->intf_type != WCD9XXX_INTERFACE_TYPE_SLIMBUS)
		return;

	if (dai->id <= NUM_CODEC_DAIS) {
		if (tabla->dai[dai->id].ch_mask) {
			active = 1;
			pr_debug("%s(): Codec DAI: chmask[%d] = 0x%lx\n",
				__func__, dai->id, tabla->dai[dai->id].ch_mask);
		}
	}

	if ((tabla_core != NULL) &&
	    (tabla_core->dev != NULL) &&
	    (tabla_core->dev->parent != NULL) &&
	    (active == 0)) {
		pm_runtime_mark_last_busy(tabla_core->dev->parent);
		pm_runtime_put(tabla_core->dev->parent);
	}
}

int tabla_mclk_enable(struct snd_soc_codec *codec, int mclk_enable, bool dapm)
{
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s: mclk_enable = %u, dapm = %d\n", __func__, mclk_enable,
		 dapm);
	if (dapm)
		TABLA_ACQUIRE_LOCK(tabla->codec_resource_lock);
	if (mclk_enable) {
		tabla->mclk_enabled = true;

		if (tabla->mbhc_polling_active) {
			tabla_codec_pause_hs_polling(codec);
			tabla_codec_disable_clock_block(codec);
			tabla_codec_enable_bandgap(codec,
						   TABLA_BANDGAP_AUDIO_MODE);
			tabla_codec_enable_clock_block(codec, 0);
			tabla_codec_calibrate_hs_polling(codec);
			tabla_codec_start_hs_polling(codec);
		} else {
			tabla_codec_disable_clock_block(codec);
			tabla_codec_enable_bandgap(codec,
						   TABLA_BANDGAP_AUDIO_MODE);
			tabla_codec_enable_clock_block(codec, 0);
		}
	} else {

		if (!tabla->mclk_enabled) {
			if (dapm)
				TABLA_RELEASE_LOCK(tabla->codec_resource_lock);
			pr_err("Error, MCLK already diabled\n");
			return -EINVAL;
		}
		tabla->mclk_enabled = false;

		if (tabla->mbhc_polling_active) {
			tabla_codec_pause_hs_polling(codec);
			tabla_codec_disable_clock_block(codec);
			tabla_codec_enable_bandgap(codec,
						  TABLA_BANDGAP_MBHC_MODE);
			tabla_enable_rx_bias(codec, 1);
			tabla_codec_enable_clock_block(codec, 1);
			tabla_codec_calibrate_hs_polling(codec);
			tabla_codec_start_hs_polling(codec);
			snd_soc_update_bits(codec, TABLA_A_CLK_BUFF_EN1,
					0x05, 0x01);
		} else {
			tabla_codec_disable_clock_block(codec);
			tabla_codec_enable_bandgap(codec,
						   TABLA_BANDGAP_OFF);
		}
	}
	if (dapm)
		TABLA_RELEASE_LOCK(tabla->codec_resource_lock);
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
		if (tabla->intf_type == WCD9XXX_INTERFACE_TYPE_I2C) {
			if (dai->id == AIF1_CAP)
				snd_soc_update_bits(dai->codec,
					TABLA_A_CDC_CLK_TX_I2S_CTL,
					TABLA_I2S_MASTER_MODE_MASK, 0);
			else if (dai->id == AIF1_PB)
				snd_soc_update_bits(dai->codec,
					TABLA_A_CDC_CLK_RX_I2S_CTL,
					TABLA_I2S_MASTER_MODE_MASK, 0);
		}
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
	/* CPU is slave */
		if (tabla->intf_type == WCD9XXX_INTERFACE_TYPE_I2C) {
			val = TABLA_I2S_MASTER_MODE_MASK;
			if (dai->id == AIF1_CAP)
				snd_soc_update_bits(dai->codec,
					TABLA_A_CDC_CLK_TX_I2S_CTL, val, val);
			else if (dai->id == AIF1_PB)
				snd_soc_update_bits(dai->codec,
					TABLA_A_CDC_CLK_RX_I2S_CTL, val, val);
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int tabla_set_channel_map(struct snd_soc_dai *dai,
				unsigned int tx_num, unsigned int *tx_slot,
				unsigned int rx_num, unsigned int *rx_slot)

{
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(dai->codec);
	struct wcd9xxx *core = dev_get_drvdata(dai->codec->dev->parent);

	if (!tx_slot && !rx_slot) {
		pr_err("%s: Invalid\n", __func__);
		return -EINVAL;
	}
	pr_debug("%s(): dai_name = %s DAI-ID %x tx_ch %d rx_ch %d\n"
		 "tabla->intf_type %d\n",
		 __func__, dai->name, dai->id, tx_num, rx_num,
		 tabla->intf_type);

	if (tabla->intf_type == WCD9XXX_INTERFACE_TYPE_SLIMBUS)
		wcd9xxx_init_slimslave(core, core->slim->laddr,
				       tx_num, tx_slot, rx_num, rx_slot);
	return 0;
}

static int tabla_get_channel_map(struct snd_soc_dai *dai,
				unsigned int *tx_num, unsigned int *tx_slot,
				unsigned int *rx_num, unsigned int *rx_slot)

{
	struct tabla_priv *tabla_p = snd_soc_codec_get_drvdata(dai->codec);
	u32 i = 0;
	struct wcd9xxx_ch *ch;

	switch (dai->id) {
	case AIF1_PB:
	case AIF2_PB:
	case AIF3_PB:
		if (!rx_slot || !rx_num) {
			pr_err("%s: Invalid rx_slot %d or rx_num %d\n",
				 __func__, (u32) rx_slot, (u32) rx_num);
			return -EINVAL;
		}
		list_for_each_entry(ch, &tabla_p->dai[dai->id].wcd9xxx_ch_list,
				    list) {
			rx_slot[i++] = ch->ch_num;
		}
		*rx_num = i;
		break;
	case AIF1_CAP:
	case AIF2_CAP:
	case AIF3_CAP:
		if (!tx_slot || !tx_num) {
			pr_err("%s: Invalid tx_slot %d or tx_num %d\n",
				 __func__, (u32) tx_slot, (u32) tx_num);
			return -EINVAL;
		}
		list_for_each_entry(ch, &tabla_p->dai[dai->id].wcd9xxx_ch_list,
				    list) {
			tx_slot[i++] = ch->ch_num;
		}
		*tx_num = i;
		break;

	default:
		pr_err("%s: Invalid DAI ID %x\n", __func__, dai->id);
		break;
	}
	return 0;
}


static int tabla_set_interpolator_rate(struct snd_soc_dai *dai,
				       u8 rx_fs_rate_reg_val,
				       u32 compander_fs,
				       u32 sample_rate)
{
	u32 j;
	u8 rx_mix1_inp;
	u16 rx_mix_1_reg_1, rx_mix_1_reg_2;
	u16 rx_fs_reg;
	u8 rx_mix_1_reg_1_val, rx_mix_1_reg_2_val;
	struct snd_soc_codec *codec = dai->codec;
	struct wcd9xxx_ch *ch;
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);

	list_for_each_entry(ch, &tabla->dai[dai->id].wcd9xxx_ch_list, list) {

		rx_mix1_inp = ch->port - RX_MIX1_INP_SEL_RX1;

		if ((rx_mix1_inp < RX_MIX1_INP_SEL_RX1) ||
			(rx_mix1_inp > RX_MIX1_INP_SEL_RX7)) {
			pr_err("%s: Invalid TABLA_RX%u port. Dai ID is %d\n",
				__func__,  rx_mix1_inp - 5 , dai->id);
			return -EINVAL;
		}

		rx_mix_1_reg_1 = TABLA_A_CDC_CONN_RX1_B1_CTL;

		for (j = 0; j < NUM_INTERPOLATORS; j++) {
			rx_mix_1_reg_2 = rx_mix_1_reg_1 + 1;

			rx_mix_1_reg_1_val = snd_soc_read(codec,
							  rx_mix_1_reg_1);
			rx_mix_1_reg_2_val = snd_soc_read(codec,
							  rx_mix_1_reg_2);

			if (((rx_mix_1_reg_1_val & 0x0F) == rx_mix1_inp) ||
			(((rx_mix_1_reg_1_val >> 4) & 0x0F) == rx_mix1_inp) ||
			((rx_mix_1_reg_2_val & 0x0F) == rx_mix1_inp)) {

				rx_fs_reg = TABLA_A_CDC_RX1_B5_CTL + 8 * j;

				pr_debug("%s: AIF_PB DAI(%d) connected to RX%u\n",
					__func__, dai->id, j + 1);

				pr_debug("%s: set RX%u sample rate to %u\n",
					__func__, j + 1, sample_rate);

				snd_soc_update_bits(codec, rx_fs_reg,
						    0xE0, rx_fs_rate_reg_val);

				if (comp_rx_path[j] < COMPANDER_MAX)
					tabla->comp_fs[comp_rx_path[j]]
					= compander_fs;
			}
			if (j <= 2)
				rx_mix_1_reg_1 += 3;
			else
				rx_mix_1_reg_1 += 2;
		}
	}
	return 0;
}

static int tabla_set_decimator_rate(struct snd_soc_dai *dai,
				    u8 tx_fs_rate_reg_val,
				    u32 sample_rate)
{
	struct snd_soc_codec *codec = dai->codec;
	struct wcd9xxx_ch *ch;
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);
	u32 tx_port;
	u16 tx_port_reg, tx_fs_reg;
	u8 tx_port_reg_val;
	s8 decimator;

	list_for_each_entry(ch, &tabla->dai[dai->id].wcd9xxx_ch_list, list) {

		tx_port = ch->port + 1;
		pr_debug("%s: dai->id = %d, tx_port = %d",
			__func__, dai->id, tx_port);

		if ((tx_port < 1) || (tx_port > NUM_DECIMATORS)) {
			pr_err("%s: Invalid SLIM TX%u port. DAI ID is %d\n",
				__func__, tx_port, dai->id);
			return -EINVAL;
		}

		tx_port_reg = TABLA_A_CDC_CONN_TX_SB_B1_CTL + (tx_port - 1);
		tx_port_reg_val =  snd_soc_read(codec, tx_port_reg);

		decimator = 0;

		if ((tx_port >= 1) && (tx_port <= 6)) {

			tx_port_reg_val =  tx_port_reg_val & 0x0F;
			if (tx_port_reg_val == 0x8)
				decimator = tx_port;

		} else if ((tx_port >= 7) && (tx_port <= NUM_DECIMATORS)) {

			tx_port_reg_val =  tx_port_reg_val & 0x1F;

			if ((tx_port_reg_val >= 0x8) &&
			    (tx_port_reg_val <= 0x11)) {

				decimator = (tx_port_reg_val - 0x8) + 1;
			}
		}

		if (decimator) { /* SLIM_TX port has a DEC as input */

			tx_fs_reg = TABLA_A_CDC_TX1_CLK_FS_CTL +
				    8 * (decimator - 1);

			pr_debug("%s: set DEC%u (-> SLIM_TX%u) rate to %u\n",
				__func__, decimator, tx_port, sample_rate);

			snd_soc_update_bits(codec, tx_fs_reg, 0x07,
					    tx_fs_rate_reg_val);

		} else {
			if ((tx_port_reg_val >= 0x1) &&
			    (tx_port_reg_val <= 0x7)) {

				pr_debug("%s: RMIX%u going to SLIM TX%u\n",
					__func__, tx_port_reg_val, tx_port);

			} else if  ((tx_port_reg_val >= 0x8) &&
				    (tx_port_reg_val <= 0x11)) {

				pr_err("%s: ERROR: Should not be here\n",
					__func__);
				pr_err("%s: ERROR: DEC connected to SLIM TX%u\n",
					__func__, tx_port);
				return -EINVAL;

			} else if (tx_port_reg_val == 0) {
				pr_debug("%s: no signal to SLIM TX%u\n",
					__func__, tx_port);
			} else {
				pr_err("%s: ERROR: wrong signal to SLIM TX%u\n",
					__func__, tx_port);
				pr_err("%s: ERROR: wrong signal = %u\n",
					__func__, tx_port_reg_val);
				return -EINVAL;
			}
		}
	}
	return 0;
}

static int tabla_hw_params(struct snd_pcm_substream *substream,
			   struct snd_pcm_hw_params *params,
			   struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(dai->codec);
	u8 tx_fs_rate_reg_val, rx_fs_rate_reg_val;
	u32 compander_fs;
	int ret;

	pr_debug("%s: dai_name = %s DAI-ID %x rate %d num_ch %d\n", __func__,
		 dai->name, dai->id, params_rate(params),
		 params_channels(params));

	switch (params_rate(params)) {
	case 8000:
		tx_fs_rate_reg_val = 0x00;
		rx_fs_rate_reg_val = 0x00;
		compander_fs = COMPANDER_FS_8KHZ;
		break;
	case 16000:
		tx_fs_rate_reg_val = 0x01;
		rx_fs_rate_reg_val = 0x20;
		compander_fs = COMPANDER_FS_16KHZ;
		break;
	case 32000:
		tx_fs_rate_reg_val = 0x02;
		rx_fs_rate_reg_val = 0x40;
		compander_fs = COMPANDER_FS_32KHZ;
		break;
	case 48000:
		tx_fs_rate_reg_val = 0x03;
		rx_fs_rate_reg_val = 0x60;
		compander_fs = COMPANDER_FS_48KHZ;
		break;
	case 96000:
		tx_fs_rate_reg_val = 0x04;
		rx_fs_rate_reg_val = 0x80;
		compander_fs = COMPANDER_FS_96KHZ;
		break;
	case 192000:
		tx_fs_rate_reg_val = 0x05;
		rx_fs_rate_reg_val = 0xA0;
		compander_fs = COMPANDER_FS_192KHZ;
		break;
	default:
		pr_err("%s: Invalid sampling rate %d\n", __func__,
			params_rate(params));
		return -EINVAL;
	}

	switch (substream->stream) {
	case SNDRV_PCM_STREAM_CAPTURE:

		ret = tabla_set_decimator_rate(dai, tx_fs_rate_reg_val,
					       params_rate(params));
		if (ret < 0) {
			pr_err("%s: set decimator rate failed %d\n", __func__,
			       ret);
			return ret;
		}

		if (tabla->intf_type == WCD9XXX_INTERFACE_TYPE_I2C) {
			switch (params_format(params)) {
			case SNDRV_PCM_FORMAT_S16_LE:
				snd_soc_update_bits(codec,
					TABLA_A_CDC_CLK_TX_I2S_CTL, 0x20, 0x20);
				break;
			case SNDRV_PCM_FORMAT_S32_LE:
				snd_soc_update_bits(codec,
					TABLA_A_CDC_CLK_TX_I2S_CTL, 0x20, 0x00);
				break;
			default:
				pr_err("%s: Invalid format %d\n", __func__,
					params_format(params));
				return -EINVAL;
			}
			snd_soc_update_bits(codec, TABLA_A_CDC_CLK_TX_I2S_CTL,
					    0x07, tx_fs_rate_reg_val);
		} else {
			switch (params_format(params)) {
			case SNDRV_PCM_FORMAT_S16_LE:
				tabla->dai[dai->id].bit_width = 16;
				break;
			default:
				pr_err("%s: Invalid TX format %d\n", __func__,
					params_format(params));
				return -EINVAL;
			}
			tabla->dai[dai->id].rate   = params_rate(params);
		}
		break;

	case SNDRV_PCM_STREAM_PLAYBACK:

		ret = tabla_set_interpolator_rate(dai, rx_fs_rate_reg_val,
						  compander_fs,
						  params_rate(params));
		if (ret < 0) {
			pr_err("%s: set decimator rate failed %d\n", __func__,
			       ret);
			return ret;
		}

		if (tabla->intf_type == WCD9XXX_INTERFACE_TYPE_I2C) {
			switch (params_format(params)) {
			case SNDRV_PCM_FORMAT_S16_LE:
				snd_soc_update_bits(codec,
					TABLA_A_CDC_CLK_RX_I2S_CTL, 0x20, 0x20);
				break;
			case SNDRV_PCM_FORMAT_S32_LE:
				snd_soc_update_bits(codec,
					TABLA_A_CDC_CLK_RX_I2S_CTL, 0x20, 0x00);
				break;
			default:
				pr_err("%s: Invalid RX format %d\n", __func__,
					params_format(params));
				return -EINVAL;
			}
			snd_soc_update_bits(codec, TABLA_A_CDC_CLK_RX_I2S_CTL,
					0x03, (rx_fs_rate_reg_val >> 0x05));
		} else {
			switch (params_format(params)) {
			case SNDRV_PCM_FORMAT_S16_LE:
				tabla->dai[dai->id].bit_width = 16;
				break;
			default:
				pr_err("%s: Invalid format %d\n", __func__,
					params_format(params));
				return -EINVAL;
			}
			tabla->dai[dai->id].rate = params_rate(params);
		}
		break;

	default:
		pr_err("%s: Invalid stream type %d\n", __func__,
			substream->stream);
		return -EINVAL;
	}
	return 0;
}

static struct snd_soc_dai_ops tabla_dai_ops = {
	.startup = tabla_startup,
	.shutdown = tabla_shutdown,
	.hw_params = tabla_hw_params,
	.set_sysclk = tabla_set_dai_sysclk,
	.set_fmt = tabla_set_dai_fmt,
	.set_channel_map = tabla_set_channel_map,
	.get_channel_map = tabla_get_channel_map,
};

static struct snd_soc_dai_driver tabla_dai[] = {
	{
		.name = "tabla_rx1",
		.id = AIF1_PB,
		.playback = {
			.stream_name = "AIF1 Playback",
			.rates = WCD9310_RATES,
			.formats = TABLA_FORMATS,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &tabla_dai_ops,
	},
	{
		.name = "tabla_tx1",
		.id = AIF1_CAP,
		.capture = {
			.stream_name = "AIF1 Capture",
			.rates = WCD9310_RATES,
			.formats = TABLA_FORMATS,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 4,
		},
		.ops = &tabla_dai_ops,
	},
	{
		.name = "tabla_rx2",
		.id = AIF2_PB,
		.playback = {
			.stream_name = "AIF2 Playback",
			.rates = WCD9310_RATES,
			.formats = TABLA_FORMATS,
			.rate_min = 8000,
			.rate_max = 192000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &tabla_dai_ops,
	},
	{
		.name = "tabla_tx2",
		.id = AIF2_CAP,
		.capture = {
			.stream_name = "AIF2 Capture",
			.rates = WCD9310_RATES,
			.formats = TABLA_FORMATS,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 4,
		},
		.ops = &tabla_dai_ops,
	},
	{
		.name = "tabla_tx3",
		.id = AIF3_CAP,
		.capture = {
			.stream_name = "AIF3 Capture",
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
		.name = "tabla_rx3",
		.id = AIF3_PB,
		.playback = {
			.stream_name = "AIF3 Playback",
			.rates = WCD9310_RATES,
			.formats = TABLA_FORMATS,
			.rate_min = 8000,
			.rate_max = 192000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &tabla_dai_ops,
	},
};

static struct snd_soc_dai_driver tabla_i2s_dai[] = {
	{
		.name = "tabla_i2s_rx1",
		.id = AIF1_PB,
		.playback = {
			.stream_name = "AIF1 Playback",
			.rates = WCD9310_RATES,
			.formats = TABLA_FORMATS,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 4,
		},
		.ops = &tabla_dai_ops,
	},
	{
		.name = "tabla_i2s_tx1",
		.id = AIF1_CAP,
		.capture = {
			.stream_name = "AIF1 Capture",
			.rates = WCD9310_RATES,
			.formats = TABLA_FORMATS,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 4,
		},
		.ops = &tabla_dai_ops,
	},
};

static int tabla_codec_enable_chmask(struct tabla_priv *tabla_p,
				     int event, int index)
{
	int  ret = 0;
	struct wcd9xxx_ch *ch;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		list_for_each_entry(ch,
			&tabla_p->dai[index].wcd9xxx_ch_list, list) {
			ret = wcd9xxx_get_slave_port(ch->ch_num);
			if (ret < 0) {
				pr_err("%s: Invalid slave port ID: %d\n",
					__func__, ret);
				ret = -EINVAL;
				break;
			}
			tabla_p->dai[index].ch_mask |= 1 << ret;
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		ret = wait_event_timeout(tabla_p->dai[index].dai_wait,
					(tabla_p->dai[index].ch_mask == 0),
				msecs_to_jiffies(SLIM_CLOSE_TIMEOUT));
		if (!ret) {
			pr_err("%s: Slim close tx/rx wait timeout\n",
				__func__);
			ret = -EINVAL;
		}
		break;
	}
	return ret;
}

static int tabla_codec_enable_slimrx(struct snd_soc_dapm_widget *w,
				     struct snd_kcontrol *kcontrol,
				     int event)
{
	struct wcd9xxx *core;
	struct snd_soc_codec *codec = w->codec;
	struct tabla_priv *tabla_p = snd_soc_codec_get_drvdata(codec);
	u32  ret = 0;
	struct wcd9xxx_codec_dai_data *dai;

	core = dev_get_drvdata(codec->dev->parent);

	pr_debug("%s: event called! codec name %s num_dai %d\n"
		"stream name %s event %d\n",
		__func__, w->codec->name, w->codec->num_dai,
		w->sname, event);

	/* Execute the callback only if interface type is slimbus */
	if (tabla_p->intf_type != WCD9XXX_INTERFACE_TYPE_SLIMBUS) {
		if (event == SND_SOC_DAPM_POST_PMD && (core != NULL) &&
		    (core->dev != NULL) &&
		    (core->dev->parent != NULL)) {
			pm_runtime_mark_last_busy(core->dev->parent);
			pm_runtime_put(core->dev->parent);
		}
		return 0;
	}
	pr_debug("%s: w->name %s w->shift %d event %d\n",
		__func__, w->name, w->shift, event);
	dai = &tabla_p->dai[w->shift];

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		ret = tabla_codec_enable_chmask(tabla_p, SND_SOC_DAPM_POST_PMU,
						w->shift);
		ret = wcd9xxx_cfg_slim_sch_rx(core, &dai->wcd9xxx_ch_list,
					      dai->rate, dai->bit_width,
					      &dai->grph);
		break;
	case SND_SOC_DAPM_POST_PMD:
		ret = wcd9xxx_close_slim_sch_rx(core,
						&dai->wcd9xxx_ch_list,
						dai->grph);
		ret = tabla_codec_enable_chmask(tabla_p, SND_SOC_DAPM_POST_PMD,
						w->shift);
		if (ret < 0) {
			ret = wcd9xxx_disconnect_port(core,
						      &dai->wcd9xxx_ch_list,
						      dai->grph);
			pr_info("%s: Disconnect RX port, ret = %d\n",
				__func__, ret);
		}
		if ((core != NULL) &&
			(core->dev != NULL) &&
			(core->dev->parent != NULL)) {
			pm_runtime_mark_last_busy(core->dev->parent);
			pm_runtime_put(core->dev->parent);
		}
		break;
	}

	return ret;
}

static int tabla_codec_enable_slimtx(struct snd_soc_dapm_widget *w,
				     struct snd_kcontrol *kcontrol,
				     int event)
{
	struct wcd9xxx *core;
	struct snd_soc_codec *codec = w->codec;
	struct tabla_priv *tabla_p = snd_soc_codec_get_drvdata(codec);
	u32  ret = 0;
	struct wcd9xxx_codec_dai_data *dai;

	core = dev_get_drvdata(codec->dev->parent);

	pr_debug("%s: event called! codec name %s num_dai %d\n"
		 "stream name %s\n", __func__, w->codec->name,
		 w->codec->num_dai, w->sname);

	/* Execute the callback only if interface type is slimbus */
	if (tabla_p->intf_type != WCD9XXX_INTERFACE_TYPE_SLIMBUS) {
		if (event == SND_SOC_DAPM_POST_PMD && (core != NULL) &&
		    (core->dev != NULL) &&
		    (core->dev->parent != NULL)) {
			pm_runtime_mark_last_busy(core->dev->parent);
			pm_runtime_put(core->dev->parent);
		}
		return 0;
	}

	pr_debug("%s(): %s %d\n", __func__, w->name, event);

	dai = &tabla_p->dai[w->shift];
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		ret = tabla_codec_enable_chmask(tabla_p, SND_SOC_DAPM_POST_PMU,
						w->shift);
		ret = wcd9xxx_cfg_slim_sch_tx(core, &dai->wcd9xxx_ch_list,
					      dai->rate,
					      dai->bit_width,
					      &dai->grph);
		break;
	case SND_SOC_DAPM_POST_PMD:
		ret = wcd9xxx_close_slim_sch_tx(core, &dai->wcd9xxx_ch_list,
						dai->grph);
		ret = tabla_codec_enable_chmask(tabla_p, SND_SOC_DAPM_POST_PMD,
						w->shift);
		if (ret < 0) {
			ret = wcd9xxx_disconnect_port(core,
						      &dai->wcd9xxx_ch_list,
						      dai->grph);
			pr_info("%s: Disconnect TX port, ret = %d\n",
				__func__, ret);
		}
		if ((core != NULL) &&
			(core->dev != NULL) &&
			(core->dev->parent != NULL)) {
			pm_runtime_mark_last_busy(core->dev->parent);
			pm_runtime_put(core->dev->parent);
		}
		break;
	}
	return ret;
}

/* Todo: Have seperate dapm widgets for I2S and Slimbus.
 * Might Need to have callbacks registered only for slimbus
 */
static const struct snd_soc_dapm_widget tabla_dapm_widgets[] = {
	/*RX stuff */
	SND_SOC_DAPM_OUTPUT("EAR"),

	SND_SOC_DAPM_PGA_E("EAR PA", SND_SOC_NOPM, 0, 0, NULL,
			0, tabla_ear_pa_event, SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_MIXER("DAC1", SND_SOC_NOPM, 0, 0, dac1_switch,
		ARRAY_SIZE(dac1_switch)),

	SND_SOC_DAPM_AIF_IN_E("AIF1 PB", "AIF1 Playback", 0, SND_SOC_NOPM,
				AIF1_PB, 0, tabla_codec_enable_slimrx,
				SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_IN_E("AIF2 PB", "AIF2 Playback", 0, SND_SOC_NOPM,
				AIF2_PB, 0, tabla_codec_enable_slimrx,
				SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_IN_E("AIF3 PB", "AIF3 Playback", 0, SND_SOC_NOPM,
				AIF3_PB, 0, tabla_codec_enable_slimrx,
				SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX("SLIM RX1 MUX", SND_SOC_NOPM, TABLA_RX1, 0,
				&slim_rx_mux[TABLA_RX1]),
	SND_SOC_DAPM_MUX("SLIM RX2 MUX", SND_SOC_NOPM, TABLA_RX2, 0,
				&slim_rx_mux[TABLA_RX2]),
	SND_SOC_DAPM_MUX("SLIM RX3 MUX", SND_SOC_NOPM, TABLA_RX3, 0,
				&slim_rx_mux[TABLA_RX3]),
	SND_SOC_DAPM_MUX("SLIM RX4 MUX", SND_SOC_NOPM, TABLA_RX4, 0,
				&slim_rx_mux[TABLA_RX4]),
	SND_SOC_DAPM_MUX("SLIM RX5 MUX", SND_SOC_NOPM, TABLA_RX5, 0,
				&slim_rx_mux[TABLA_RX5]),
	SND_SOC_DAPM_MUX("SLIM RX6 MUX", SND_SOC_NOPM, TABLA_RX6, 0,
				&slim_rx_mux[TABLA_RX6]),
	SND_SOC_DAPM_MUX("SLIM RX7 MUX", SND_SOC_NOPM, TABLA_RX7, 0,
				&slim_rx_mux[TABLA_RX7]),

	SND_SOC_DAPM_MIXER("SLIM RX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SLIM RX2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SLIM RX3", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SLIM RX4", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SLIM RX5", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SLIM RX6", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SLIM RX7", SND_SOC_NOPM, 0, 0, NULL, 0),
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

	SND_SOC_DAPM_MIXER_E("RX1 MIX2", TABLA_A_CDC_CLK_RX_B1_CTL, 0, 0, NULL,
		0, tabla_codec_reset_interpolator, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MIXER_E("RX2 MIX2", TABLA_A_CDC_CLK_RX_B1_CTL, 1, 0, NULL,
		0, tabla_codec_reset_interpolator, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MIXER_E("RX3 MIX2", TABLA_A_CDC_CLK_RX_B1_CTL, 2, 0, NULL,
		0, tabla_codec_reset_interpolator, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MIXER_E("RX4 MIX1", TABLA_A_CDC_CLK_RX_B1_CTL, 3, 0, NULL,
		0, tabla_codec_reset_interpolator, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MIXER_E("RX5 MIX1", TABLA_A_CDC_CLK_RX_B1_CTL, 4, 0, NULL,
		0, tabla_codec_reset_interpolator, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MIXER_E("RX6 MIX1", TABLA_A_CDC_CLK_RX_B1_CTL, 5, 0, NULL,
		0, tabla_codec_reset_interpolator, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_MIXER_E("RX7 MIX1", TABLA_A_CDC_CLK_RX_B1_CTL, 6, 0, NULL,
		0, tabla_codec_reset_interpolator, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_MIXER("RX1 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX2 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX3 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MUX_E("RX4 DSM MUX", TABLA_A_CDC_CLK_RX_B1_CTL, 3, 0,
		&rx4_dsm_mux, tabla_codec_reset_interpolator,
		SND_SOC_DAPM_PRE_PMU),

	SND_SOC_DAPM_MUX_E("RX6 DSM MUX", TABLA_A_CDC_CLK_RX_B1_CTL, 5, 0,
		&rx6_dsm_mux, tabla_codec_reset_interpolator,
		SND_SOC_DAPM_PRE_PMU),

	SND_SOC_DAPM_MIXER_E("RX1 CHAIN", SND_SOC_NOPM, 5, 0, NULL,
		0, tabla_codec_hphr_dem_input_selection,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_POST_PMD),
	SND_SOC_DAPM_MIXER_E("RX2 CHAIN", SND_SOC_NOPM, 5, 0, NULL,
		0, tabla_codec_hphl_dem_input_selection,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_POST_PMD),

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
	SND_SOC_DAPM_MUX("RX3 MIX2 INP1", SND_SOC_NOPM, 0, 0,
		&rx3_mix2_inp1_mux),
	SND_SOC_DAPM_MUX("RX3 MIX2 INP2", SND_SOC_NOPM, 0, 0,
		&rx3_mix2_inp2_mux),

	SND_SOC_DAPM_SUPPLY("CP", TABLA_A_CP_EN, 0, 0,
		tabla_codec_enable_charge_pump, SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_SUPPLY("RX_BIAS", SND_SOC_NOPM, 0, 0,
		tabla_codec_enable_rx_bias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	/* TX */

	SND_SOC_DAPM_SUPPLY("CDC_CONN", TABLA_A_CDC_CLK_OTHR_CTL, 2, 0, NULL,
		0),

	SND_SOC_DAPM_SUPPLY("LDO_H", SND_SOC_NOPM, 0, 0,
		tabla_codec_enable_ldo_h, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY("COMP1_CLK", SND_SOC_NOPM, 0, 0,
		tabla_config_compander, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_POST_PMD),
	SND_SOC_DAPM_SUPPLY("COMP2_CLK", SND_SOC_NOPM, 1, 0,
		tabla_config_compander, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_POST_PMD),

	SND_SOC_DAPM_INPUT("AMIC1"),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS1 External", SND_SOC_NOPM, 0, 0,
		tabla_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS1 Internal1", SND_SOC_NOPM, 0, 0,
		tabla_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS1 Internal2", SND_SOC_NOPM, 0, 0,
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

	SND_SOC_DAPM_INPUT("AMIC5"),
	SND_SOC_DAPM_ADC_E("ADC5", NULL, TABLA_A_TX_5_6_EN, 7, 0,
		tabla_codec_enable_adc, SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_INPUT("AMIC6"),
	SND_SOC_DAPM_ADC_E("ADC6", NULL, TABLA_A_TX_5_6_EN, 3, 0,
		tabla_codec_enable_adc, SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_MUX_E("DEC1 MUX", TABLA_A_CDC_CLK_TX_CLK_EN_B1_CTL, 0, 0,
		&dec1_mux, tabla_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("DEC2 MUX", TABLA_A_CDC_CLK_TX_CLK_EN_B1_CTL, 1, 0,
		&dec2_mux, tabla_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("DEC3 MUX", TABLA_A_CDC_CLK_TX_CLK_EN_B1_CTL, 2, 0,
		&dec3_mux, tabla_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("DEC4 MUX", TABLA_A_CDC_CLK_TX_CLK_EN_B1_CTL, 3, 0,
		&dec4_mux, tabla_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("DEC5 MUX", TABLA_A_CDC_CLK_TX_CLK_EN_B1_CTL, 4, 0,
		&dec5_mux, tabla_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("DEC6 MUX", TABLA_A_CDC_CLK_TX_CLK_EN_B1_CTL, 5, 0,
		&dec6_mux, tabla_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("DEC7 MUX", TABLA_A_CDC_CLK_TX_CLK_EN_B1_CTL, 6, 0,
		&dec7_mux, tabla_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("DEC8 MUX", TABLA_A_CDC_CLK_TX_CLK_EN_B1_CTL, 7, 0,
		&dec8_mux, tabla_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("DEC9 MUX", TABLA_A_CDC_CLK_TX_CLK_EN_B2_CTL, 0, 0,
		&dec9_mux, tabla_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("DEC10 MUX", TABLA_A_CDC_CLK_TX_CLK_EN_B2_CTL, 1, 0,
		&dec10_mux, tabla_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX("ANC1 MUX", SND_SOC_NOPM, 0, 0, &anc1_mux),
	SND_SOC_DAPM_MUX("ANC2 MUX", SND_SOC_NOPM, 0, 0, &anc2_mux),

	SND_SOC_DAPM_OUTPUT("ANC HEADPHONE"),
	SND_SOC_DAPM_PGA_E("ANC HPHL", SND_SOC_NOPM, 0, 0, NULL, 0,
		tabla_codec_enable_anc,
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("ANC HPHR", SND_SOC_NOPM, 0, 0, NULL, 0,
		tabla_codec_enable_anc, SND_SOC_DAPM_PRE_PMU),


	SND_SOC_DAPM_MUX("ANC1 FB MUX", SND_SOC_NOPM, 0, 0, &anc1_fb_mux),

	SND_SOC_DAPM_INPUT("AMIC2"),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS2 External", SND_SOC_NOPM, 0, 0,
		tabla_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
	SND_SOC_DAPM_POST_PMU |	SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS2 Power External",
	TABLA_A_MICB_2_CTL, 7, 0,
			       tabla_codec_enable_micbias_power,
			       SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			       SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MICBIAS_E("MIC BIAS2 Internal1", SND_SOC_NOPM, 0, 0,
		tabla_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS2 Internal2", SND_SOC_NOPM, 0, 0,
		tabla_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS2 Internal3", SND_SOC_NOPM, 0, 0,
		tabla_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS3 External", SND_SOC_NOPM, 0, 0,
		tabla_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS3 Internal1", SND_SOC_NOPM, 0, 0,
		tabla_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS3 Internal2", SND_SOC_NOPM, 0, 0,
		tabla_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("ADC2", NULL, TABLA_A_TX_1_2_EN, 3, 0,
		tabla_codec_enable_adc, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_AIF_OUT_E("AIF1 CAP", "AIF1 Capture", 0, SND_SOC_NOPM,
		AIF1_CAP, 0, tabla_codec_enable_slimtx,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_AIF_OUT_E("AIF2 CAP", "AIF2 Capture", 0, SND_SOC_NOPM,
		AIF2_CAP, 0, tabla_codec_enable_slimtx,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_AIF_OUT_E("AIF3 CAP", "AIF3 Capture", 0, SND_SOC_NOPM,
		AIF3_CAP, 0, tabla_codec_enable_slimtx,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MIXER("AIF1_CAP Mixer", SND_SOC_NOPM, AIF1_CAP, 0,
		aif_cap_mixer, ARRAY_SIZE(aif_cap_mixer)),

	SND_SOC_DAPM_MIXER("AIF2_CAP Mixer", SND_SOC_NOPM, AIF2_CAP, 0,
		aif_cap_mixer, ARRAY_SIZE(aif_cap_mixer)),

	SND_SOC_DAPM_MIXER("AIF3_CAP Mixer", SND_SOC_NOPM, AIF3_CAP, 0,
		aif_cap_mixer, ARRAY_SIZE(aif_cap_mixer)),

	SND_SOC_DAPM_MUX("SLIM TX1 MUX", SND_SOC_NOPM, TABLA_TX1, 0,
		&sb_tx1_mux),
	SND_SOC_DAPM_MUX("SLIM TX2 MUX", SND_SOC_NOPM, TABLA_TX2, 0,
		&sb_tx2_mux),
	SND_SOC_DAPM_MUX("SLIM TX3 MUX", SND_SOC_NOPM, TABLA_TX3, 0,
		&sb_tx3_mux),
	SND_SOC_DAPM_MUX("SLIM TX4 MUX", SND_SOC_NOPM, TABLA_TX4, 0,
		&sb_tx4_mux),
	SND_SOC_DAPM_MUX("SLIM TX5 MUX", SND_SOC_NOPM, TABLA_TX5, 0,
		&sb_tx5_mux),
	SND_SOC_DAPM_MUX("SLIM TX6 MUX", SND_SOC_NOPM, TABLA_TX6, 0,
		&sb_tx6_mux),
	SND_SOC_DAPM_MUX("SLIM TX7 MUX", SND_SOC_NOPM, TABLA_TX7, 0,
		&sb_tx7_mux),
	SND_SOC_DAPM_MUX("SLIM TX8 MUX", SND_SOC_NOPM, TABLA_TX8, 0,
		&sb_tx8_mux),
	SND_SOC_DAPM_MUX("SLIM TX9 MUX", SND_SOC_NOPM, TABLA_TX9, 0,
		&sb_tx9_mux),
	SND_SOC_DAPM_MUX("SLIM TX10 MUX", SND_SOC_NOPM, TABLA_TX10, 0,
		&sb_tx10_mux),

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

	SND_SOC_DAPM_MUX("IIR2 INP1 MUX", SND_SOC_NOPM, 0, 0, &iir2_inp1_mux),
	SND_SOC_DAPM_PGA("IIR2", TABLA_A_CDC_CLK_SD_CTL, 1, 0, NULL, 0),

	/* AUX PGA */
	SND_SOC_DAPM_ADC_E("AUX_PGA_Left", NULL, TABLA_A_AUX_L_EN, 7, 0,
		tabla_codec_enable_aux_pga, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU |	SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("AUX_PGA_Right", NULL, TABLA_A_AUX_R_EN, 7, 0,
		tabla_codec_enable_aux_pga, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMD),

	/* Lineout, ear and HPH PA Mixers */
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

	SND_SOC_DAPM_MIXER("LINEOUT5_PA_MIXER", SND_SOC_NOPM, 0, 0,
		lineout5_pa_mix, ARRAY_SIZE(lineout5_pa_mix)),

	SND_SOC_DAPM_MIXER("EAR_PA_MIXER", SND_SOC_NOPM, 0, 0,
		ear_pa_mix, ARRAY_SIZE(ear_pa_mix)),
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

static void tabla_turn_onoff_rel_detection(struct snd_soc_codec *codec, bool on)
{
	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_B1_CTL, 0x02, on << 1);
}

static short __tabla_codec_sta_dce(struct snd_soc_codec *codec, int dce,
				   bool override_bypass, bool noreldetection)
{
	short bias_value;
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);

	wcd9xxx_disable_irq(codec->control_data, WCD9XXX_IRQ_MBHC_POTENTIAL);
	if (noreldetection)
		tabla_turn_onoff_rel_detection(codec, false);

	/* Turn on the override */
	if (!override_bypass)
		snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_B1_CTL, 0x4, 0x4);
	if (dce) {
		snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_CLK_CTL, 0x8, 0x8);
		snd_soc_write(codec, TABLA_A_CDC_MBHC_EN_CTL, 0x4);
		snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_CLK_CTL, 0x8, 0x0);
		usleep_range(tabla->mbhc_data.t_sta_dce,
			     tabla->mbhc_data.t_sta_dce);
		snd_soc_write(codec, TABLA_A_CDC_MBHC_EN_CTL, 0x4);
		usleep_range(tabla->mbhc_data.t_dce,
			     tabla->mbhc_data.t_dce);
		bias_value = tabla_codec_read_dce_result(codec);
	} else {
		snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_CLK_CTL, 0x8, 0x8);
		snd_soc_write(codec, TABLA_A_CDC_MBHC_EN_CTL, 0x2);
		snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_CLK_CTL, 0x8, 0x0);
		usleep_range(tabla->mbhc_data.t_sta_dce,
			     tabla->mbhc_data.t_sta_dce);
		snd_soc_write(codec, TABLA_A_CDC_MBHC_EN_CTL, 0x2);
		usleep_range(tabla->mbhc_data.t_sta,
			     tabla->mbhc_data.t_sta);
		bias_value = tabla_codec_read_sta_result(codec);
		snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_CLK_CTL, 0x8, 0x8);
		snd_soc_write(codec, TABLA_A_CDC_MBHC_EN_CTL, 0x0);
	}
	/* Turn off the override after measuring mic voltage */
	if (!override_bypass)
		snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_B1_CTL, 0x04, 0x00);

	if (noreldetection)
		tabla_turn_onoff_rel_detection(codec, true);
	wcd9xxx_enable_irq(codec->control_data, WCD9XXX_IRQ_MBHC_POTENTIAL);

	return bias_value;
}

static short tabla_codec_sta_dce(struct snd_soc_codec *codec, int dce,
				 bool norel)
{
	return __tabla_codec_sta_dce(codec, dce, false, norel);
}

/* called only from interrupt which is under codec_resource_lock acquisition */
static short tabla_codec_setup_hs_polling(struct snd_soc_codec *codec)
{
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);
	short bias_value;
	u8 cfilt_mode = 0;

	pr_debug("%s: enter, mclk_enabled %d\n", __func__, tabla->mclk_enabled);
	if (!tabla->mbhc_cfg.calibration) {
		pr_err("Error, no tabla calibration\n");
		return -ENODEV;
	}

	if (!tabla->mclk_enabled) {
		tabla_codec_disable_clock_block(codec);
		tabla_codec_enable_bandgap(codec, TABLA_BANDGAP_MBHC_MODE);
		tabla_enable_rx_bias(codec, 1);
		tabla_codec_enable_clock_block(codec, 1);
	}

	snd_soc_update_bits(codec, TABLA_A_CLK_BUFF_EN1, 0x05, 0x01);
	if (!tabla->mbhc_cfg.micbias_always_on) {
		/* Make sure CFILT is in fast mode, save current mode */
		cfilt_mode = snd_soc_read(codec,
					  tabla->mbhc_bias_regs.cfilt_ctl);
		snd_soc_update_bits(codec, tabla->mbhc_bias_regs.cfilt_ctl,
				    0x70, 0x00);
	}

	snd_soc_update_bits(codec, tabla->mbhc_bias_regs.ctl_reg, 0x1F, 0x16);

	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_CLK_CTL, 0x2, 0x2);
	snd_soc_write(codec, TABLA_A_MBHC_SCALING_MUX_1, 0x84);

	snd_soc_update_bits(codec, TABLA_A_TX_7_MBHC_EN, 0x80, 0x80);
	snd_soc_update_bits(codec, TABLA_A_TX_7_MBHC_EN, 0x1F, 0x1C);
	snd_soc_update_bits(codec, TABLA_A_TX_7_MBHC_TEST_CTL, 0x40, 0x40);

	snd_soc_update_bits(codec, TABLA_A_TX_7_MBHC_EN, 0x80, 0x00);
	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_CLK_CTL, 0x8, 0x8);
	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_CLK_CTL, 0x8, 0x00);

	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_B1_CTL, 0x2, 0x2);
	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_CLK_CTL, 0x8, 0x8);

	tabla_codec_calibrate_hs_polling(codec);

	/* don't flip override */
	bias_value = __tabla_codec_sta_dce(codec, 1, true, true);
	if (!tabla->mbhc_cfg.micbias_always_on)
		snd_soc_update_bits(codec, tabla->mbhc_bias_regs.cfilt_ctl,
					0x40, cfilt_mode);
	snd_soc_update_bits(codec, TABLA_A_MBHC_HPH, 0x13, 0x00);

	return bias_value;
}

static int tabla_cancel_btn_work(struct tabla_priv *tabla)
{
	int r = 0;
	struct wcd9xxx *core = dev_get_drvdata(tabla->codec->dev->parent);
	struct wcd9xxx_core_resource *core_res = &core->core_res;

	if (cancel_delayed_work_sync(&tabla->mbhc_btn_dwork)) {
		/* if scheduled mbhc_btn_dwork is canceled from here,
		* we have to unlock from here instead btn_work */
		wcd9xxx_unlock_sleep(core_res);
		r = 1;
	}
	return r;
}

/* called under codec_resource_lock acquisition */
void tabla_set_and_turnoff_hph_padac(struct snd_soc_codec *codec)
{
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);
	u8 wg_time;

	wg_time = snd_soc_read(codec, TABLA_A_RX_HPH_CNP_WG_TIME) ;
	wg_time += 1;

	/* If headphone PA is on, check if userspace receives
	 * removal event to sync-up PA's state */
	if (tabla_is_hph_pa_on(codec)) {
		pr_debug("%s PA is on, setting PA_OFF_ACK\n", __func__);
		set_bit(TABLA_HPHL_PA_OFF_ACK, &tabla->hph_pa_dac_state);
		set_bit(TABLA_HPHR_PA_OFF_ACK, &tabla->hph_pa_dac_state);
	} else {
		pr_debug("%s PA is off\n", __func__);
	}

	if (tabla_is_hph_dac_on(codec, 1))
		set_bit(TABLA_HPHL_DAC_OFF_ACK, &tabla->hph_pa_dac_state);
	if (tabla_is_hph_dac_on(codec, 0))
		set_bit(TABLA_HPHR_DAC_OFF_ACK, &tabla->hph_pa_dac_state);

	snd_soc_update_bits(codec, TABLA_A_RX_HPH_CNP_EN, 0x30, 0x00);
	snd_soc_update_bits(codec, TABLA_A_RX_HPH_L_DAC_CTL,
			0x80, 0x00);
	snd_soc_update_bits(codec, TABLA_A_RX_HPH_R_DAC_CTL,
			    0xC0, 0x00);
	usleep_range(wg_time * 1000, wg_time * 1000);
}

static void tabla_clr_and_turnon_hph_padac(struct tabla_priv *tabla)
{
	bool pa_turned_on = false;
	struct snd_soc_codec *codec = tabla->codec;
	u8 wg_time;

	wg_time = snd_soc_read(codec, TABLA_A_RX_HPH_CNP_WG_TIME) ;
	wg_time += 1;

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

	if (test_and_clear_bit(TABLA_HPHR_PA_OFF_ACK,
			       &tabla->hph_pa_dac_state)) {
		pr_debug("%s: HPHR clear flag and enable PA\n", __func__);
		snd_soc_update_bits(tabla->codec, TABLA_A_RX_HPH_CNP_EN, 0x10,
				    1 << 4);
		pa_turned_on = true;
	}
	if (test_and_clear_bit(TABLA_HPHL_PA_OFF_ACK,
			       &tabla->hph_pa_dac_state)) {
		pr_debug("%s: HPHL clear flag and enable PA\n", __func__);
		snd_soc_update_bits(tabla->codec, TABLA_A_RX_HPH_CNP_EN, 0x20,
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
static void tabla_codec_enable_mbhc_micbias(struct snd_soc_codec *codec,
					    bool enable)
{
	int r;
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);

	if (!tabla->mbhc_cfg.micbias_always_on)
		return;
	if (enable) {
		TABLA_RELEASE_LOCK(tabla->codec_resource_lock);
		tabla_codec_update_cfilt_usage(codec,
				tabla->mbhc_bias_regs.cfilt_sel, 1);
		r = snd_soc_dapm_force_enable_pin(&codec->dapm,
					    "MIC BIAS2 Power External");
		snd_soc_dapm_sync(&codec->dapm);
		TABLA_ACQUIRE_LOCK(tabla->codec_resource_lock);
		pr_debug("%s: Turning on MICBIAS2 r %d\n", __func__, r);
	} else {
		TABLA_RELEASE_LOCK(tabla->codec_resource_lock);
		r = snd_soc_dapm_disable_pin(&codec->dapm,
					     "MIC BIAS2 Power External");
		snd_soc_dapm_sync(&codec->dapm);
		tabla_codec_update_cfilt_usage(codec,
				tabla->mbhc_bias_regs.cfilt_sel, 0);
		TABLA_ACQUIRE_LOCK(tabla->codec_resource_lock);
		pr_debug("%s: Turning off MICBIAS2 r %d\n", __func__, r);
	}
}

/* called under codec_resource_lock acquisition */
static void tabla_codec_report_plug(struct snd_soc_codec *codec, int insertion,
				    enum snd_jack_types jack_type)
{
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);
	pr_debug("%s: enter insertion %d hph_status %x\n",
		 __func__, insertion, tabla->hph_status);
	if (!insertion) {
		/* Report removal */
		tabla->hph_status &= ~jack_type;
		if (tabla->mbhc_cfg.headset_jack) {
			/* cancel possibly scheduled btn work and
			* report release if we reported button press */
			if (tabla_cancel_btn_work(tabla)) {
				pr_debug("%s: button press is canceled\n",
					__func__);
			} else if (tabla->buttons_pressed) {
				pr_debug("%s: Reporting release for reported "
					 "button press %d\n", __func__,
					 jack_type);
				tabla_snd_soc_jack_report(tabla,
						 tabla->mbhc_cfg.button_jack, 0,
						 tabla->buttons_pressed);
				tabla->buttons_pressed &=
							~TABLA_JACK_BUTTON_MASK;
			}
			if (jack_type == SND_JACK_HEADSET)
				tabla_codec_enable_mbhc_micbias(codec, false);
			pr_debug("%s: Reporting removal %d(%x)\n", __func__,
				 jack_type, tabla->hph_status);
			tabla_snd_soc_jack_report(tabla,
						  tabla->mbhc_cfg.headset_jack,
						  tabla->hph_status,
						  TABLA_JACK_MASK);
		}
		tabla_set_and_turnoff_hph_padac(codec);
		hphocp_off_report(tabla, SND_JACK_OC_HPHR,
				  WCD9XXX_IRQ_HPH_PA_OCPR_FAULT);
		hphocp_off_report(tabla, SND_JACK_OC_HPHL,
				  WCD9XXX_IRQ_HPH_PA_OCPL_FAULT);
		tabla->current_plug = PLUG_TYPE_NONE;
		tabla->mbhc_polling_active = false;
	} else {
		if (tabla->mbhc_cfg.detect_extn_cable) {
			/* Report removal of current jack type */
			if (tabla->hph_status != jack_type &&
			    tabla->mbhc_cfg.headset_jack) {
				pr_debug("%s: Reporting removal (%x)\n",
					 __func__, tabla->hph_status);
				tabla_snd_soc_jack_report(tabla,
						tabla->mbhc_cfg.headset_jack,
						0, TABLA_JACK_MASK);
				tabla->hph_status = 0;
			}
		}
		/* Report insertion */
		tabla->hph_status |= jack_type;

		if (jack_type == SND_JACK_HEADPHONE)
			tabla->current_plug = PLUG_TYPE_HEADPHONE;
		else if (jack_type == SND_JACK_UNSUPPORTED)
			tabla->current_plug = PLUG_TYPE_GND_MIC_SWAP;
		else if (jack_type == SND_JACK_HEADSET) {
			tabla->mbhc_polling_active = true;
			tabla->current_plug = PLUG_TYPE_HEADSET;
			tabla_codec_enable_mbhc_micbias(codec, true);
		} else if (jack_type == SND_JACK_LINEOUT)
			tabla->current_plug = PLUG_TYPE_HIGH_HPH;
		if (tabla->mbhc_cfg.headset_jack) {
			pr_debug("%s: Reporting insertion %d(%x)\n", __func__,
				 jack_type, tabla->hph_status);
			tabla_snd_soc_jack_report(tabla,
						  tabla->mbhc_cfg.headset_jack,
						  tabla->hph_status,
						  TABLA_JACK_MASK);
		}
		tabla_clr_and_turnon_hph_padac(tabla);
	}
	pr_debug("%s: leave hph_status %x\n", __func__, tabla->hph_status);
}

static int tabla_codec_enable_hs_detect(struct snd_soc_codec *codec,
					int insertion, int trigger,
					bool padac_off)
{
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);
	int central_bias_enabled = 0;
	const struct tabla_mbhc_general_cfg *generic =
	    TABLA_MBHC_CAL_GENERAL_PTR(tabla->mbhc_cfg.calibration);
	const struct tabla_mbhc_plug_detect_cfg *plug_det =
	    TABLA_MBHC_CAL_PLUG_DET_PTR(tabla->mbhc_cfg.calibration);

	pr_debug("%s: enter insertion(%d) trigger(0x%x)\n",
		 __func__, insertion, trigger);

	if (!tabla->mbhc_cfg.calibration) {
		pr_err("Error, no tabla calibration\n");
		return -EINVAL;
	}

	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_INT_CTL, 0x1, 0);

	/* Make sure mic bias and Mic line schmitt trigger
	 * are turned OFF
	 */
	snd_soc_update_bits(codec, tabla->mbhc_bias_regs.ctl_reg, 0x01, 0x01);
	snd_soc_update_bits(codec, tabla->mbhc_bias_regs.mbhc_reg, 0x90, 0x00);

	if (insertion) {
		pr_debug("%s: setup for insertion\n", __func__);
		tabla_codec_switch_micbias(codec, 0);

		/* DAPM can manipulate PA/DAC bits concurrently */
		if (padac_off == true) {
			tabla_set_and_turnoff_hph_padac(codec);
		}

		if (trigger & MBHC_USE_HPHL_TRIGGER) {
			/* Enable HPH Schmitt Trigger */
			snd_soc_update_bits(codec, TABLA_A_MBHC_HPH, 0x11,
					    0x11);
			snd_soc_update_bits(codec, TABLA_A_MBHC_HPH, 0x0C,
					    plug_det->hph_current << 2);
			snd_soc_update_bits(codec, TABLA_A_MBHC_HPH, 0x02,
					    0x02);
		}
		if (trigger & MBHC_USE_MB_TRIGGER) {
			/* enable the mic line schmitt trigger */
			snd_soc_update_bits(codec,
					    tabla->mbhc_bias_regs.mbhc_reg,
					    0x60, plug_det->mic_current << 5);
			snd_soc_update_bits(codec,
					    tabla->mbhc_bias_regs.mbhc_reg,
					    0x80, 0x80);
			usleep_range(plug_det->t_mic_pid, plug_det->t_mic_pid);
			snd_soc_update_bits(codec,
					    tabla->mbhc_bias_regs.ctl_reg, 0x01,
					    0x00);
			snd_soc_update_bits(codec,
					    tabla->mbhc_bias_regs.mbhc_reg,
					    0x10, 0x10);
		}

		/* setup for insetion detection */
		snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_INT_CTL, 0x2, 0);
	} else {
		pr_debug("setup for removal detection\n");
		/* Make sure the HPH schmitt trigger is OFF */
		snd_soc_update_bits(codec, TABLA_A_MBHC_HPH, 0x12, 0x00);

		/* enable the mic line schmitt trigger */
		snd_soc_update_bits(codec, tabla->mbhc_bias_regs.ctl_reg,
				    0x01, 0x00);
		snd_soc_update_bits(codec, tabla->mbhc_bias_regs.mbhc_reg, 0x60,
				    plug_det->mic_current << 5);
		snd_soc_update_bits(codec, tabla->mbhc_bias_regs.mbhc_reg,
			0x80, 0x80);
		usleep_range(plug_det->t_mic_pid, plug_det->t_mic_pid);
		snd_soc_update_bits(codec, tabla->mbhc_bias_regs.mbhc_reg,
			0x10, 0x10);

		/* Setup for low power removal detection */
		snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_INT_CTL, 0x2, 0x2);
	}

	if (snd_soc_read(codec, TABLA_A_CDC_MBHC_B1_CTL) & 0x4) {
		/* called called by interrupt */
		if (!(tabla->clock_active)) {
			tabla_codec_enable_config_mode(codec, 1);
			snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_B1_CTL,
				0x06, 0);
			usleep_range(generic->t_shutdown_plug_rem,
				     generic->t_shutdown_plug_rem);
			tabla_codec_enable_config_mode(codec, 0);
		} else
			snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_B1_CTL,
				0x06, 0);
	}

	snd_soc_update_bits(codec, tabla->mbhc_bias_regs.int_rbias, 0x80, 0);

	/* If central bandgap disabled */
	if (!(snd_soc_read(codec, TABLA_A_PIN_CTL_OE1) & 1)) {
		snd_soc_update_bits(codec, TABLA_A_PIN_CTL_OE1, 0x3, 0x3);
		usleep_range(generic->t_bg_fast_settle,
			     generic->t_bg_fast_settle);
		central_bias_enabled = 1;
	}

	/* If LDO_H disabled */
	if (snd_soc_read(codec, TABLA_A_PIN_CTL_OE0) & 0x80) {
		snd_soc_update_bits(codec, TABLA_A_PIN_CTL_OE0, 0x10, 0);
		snd_soc_update_bits(codec, TABLA_A_PIN_CTL_OE0, 0x80, 0x80);
		usleep_range(generic->t_ldoh, generic->t_ldoh);
		snd_soc_update_bits(codec, TABLA_A_PIN_CTL_OE0, 0x80, 0);

		if (central_bias_enabled)
			snd_soc_update_bits(codec, TABLA_A_PIN_CTL_OE1, 0x1, 0);
	}

	snd_soc_update_bits(codec, tabla->reg_addr.micb_4_mbhc, 0x3,
			    tabla->mbhc_cfg.micbias);

	wcd9xxx_enable_irq(codec->control_data, WCD9XXX_IRQ_MBHC_INSERTION);
	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_INT_CTL, 0x1, 0x1);
	pr_debug("%s: leave\n", __func__);
	return 0;
}

static u16 tabla_codec_v_sta_dce(struct snd_soc_codec *codec, bool dce,
				 s16 vin_mv)
{
	struct tabla_priv *tabla;
	s16 diff, zero;
	u32 mb_mv, in;
	u16 value;

	tabla = snd_soc_codec_get_drvdata(codec);
	mb_mv = tabla->mbhc_data.micb_mv;

	if (mb_mv == 0) {
		pr_err("%s: Mic Bias voltage is set to zero\n", __func__);
		return -EINVAL;
	}

	if (dce) {
		diff = (tabla->mbhc_data.dce_mb) - (tabla->mbhc_data.dce_z);
		zero = (tabla->mbhc_data.dce_z);
	} else {
		diff = (tabla->mbhc_data.sta_mb) - (tabla->mbhc_data.sta_z);
		zero = (tabla->mbhc_data.sta_z);
	}
	in = (u32) diff * vin_mv;

	value = (u16) (in / mb_mv) + zero;
	return value;
}

static s32 tabla_codec_sta_dce_v(struct snd_soc_codec *codec, s8 dce,
				 u16 bias_value)
{
	struct tabla_priv *tabla;
	s16 value, z, mb;
	s32 mv;

	tabla = snd_soc_codec_get_drvdata(codec);
	value = bias_value;
	if (dce) {
		z = (tabla->mbhc_data.dce_z);
		mb = (tabla->mbhc_data.dce_mb);
		mv = (value - z) * (s32)tabla->mbhc_data.micb_mv / (mb - z);
	} else {
		z = (tabla->mbhc_data.sta_z);
		mb = (tabla->mbhc_data.sta_mb);
		mv = (value - z) * (s32)tabla->mbhc_data.micb_mv / (mb - z);
	}

	return mv;
}

static void btn_lpress_fn(struct work_struct *work)
{
	struct delayed_work *delayed_work;
	struct tabla_priv *tabla;
	short bias_value;
	int dce_mv, sta_mv;
	struct wcd9xxx *core;
	struct wcd9xxx_core_resource *core_res;

	pr_debug("%s:\n", __func__);

	delayed_work = to_delayed_work(work);
	tabla = container_of(delayed_work, struct tabla_priv, mbhc_btn_dwork);
	core = dev_get_drvdata(tabla->codec->dev->parent);
	core_res = &core->core_res;

	if (tabla) {
		if (tabla->mbhc_cfg.button_jack) {
			bias_value = tabla_codec_read_sta_result(tabla->codec);
			sta_mv = tabla_codec_sta_dce_v(tabla->codec, 0,
						bias_value);
			bias_value = tabla_codec_read_dce_result(tabla->codec);
			dce_mv = tabla_codec_sta_dce_v(tabla->codec, 1,
						bias_value);
			pr_debug("%s: Reporting long button press event"
				 " STA: %d, DCE: %d\n", __func__,
				 sta_mv, dce_mv);
			tabla_snd_soc_jack_report(tabla,
						  tabla->mbhc_cfg.button_jack,
						  tabla->buttons_pressed,
						  tabla->buttons_pressed);
		}
	} else {
		pr_err("%s: Bad tabla private data\n", __func__);
	}

	pr_debug("%s: leave\n", __func__);
	wcd9xxx_unlock_sleep(core_res);
}

static u16 tabla_get_cfilt_reg(struct snd_soc_codec *codec, u8 cfilt)
{
	u16 reg;

	switch (cfilt) {
	case TABLA_CFILT1_SEL:
		reg = TABLA_A_MICB_CFILT_1_CTL;
		break;
	case TABLA_CFILT2_SEL:
		reg = TABLA_A_MICB_CFILT_2_CTL;
		break;
	case TABLA_CFILT3_SEL:
		reg = TABLA_A_MICB_CFILT_3_CTL;
		break;
	default:
		BUG();
	}
	return reg;
}

void tabla_mbhc_cal(struct snd_soc_codec *codec)
{
	struct tabla_priv *tabla;
	struct tabla_mbhc_btn_detect_cfg *btn_det;
	u8 cfilt_mode, micbias2_cfilt_mode, bg_mode;
	u8 ncic, nmeas, navg;
	u32 mclk_rate;
	u32 dce_wait, sta_wait;
	u8 *n_cic;
	void *calibration;
	u16 bias2_ctl;

	tabla = snd_soc_codec_get_drvdata(codec);
	calibration = tabla->mbhc_cfg.calibration;

	wcd9xxx_disable_irq(codec->control_data, WCD9XXX_IRQ_MBHC_POTENTIAL);
	tabla_turn_onoff_rel_detection(codec, false);

	/* First compute the DCE / STA wait times
	 * depending on tunable parameters.
	 * The value is computed in microseconds
	 */
	btn_det = TABLA_MBHC_CAL_BTN_DET_PTR(calibration);
	n_cic = tabla_mbhc_cal_btn_det_mp(btn_det, TABLA_BTN_DET_N_CIC);
	ncic = n_cic[tabla_codec_mclk_index(tabla)];
	nmeas = TABLA_MBHC_CAL_BTN_DET_PTR(calibration)->n_meas;
	navg = TABLA_MBHC_CAL_GENERAL_PTR(calibration)->mbhc_navg;
	mclk_rate = tabla->mbhc_cfg.mclk_rate;
	dce_wait = (1000 * 512 * ncic * (nmeas + 1)) / (mclk_rate / 1000);
	sta_wait = (1000 * 128 * (navg + 1)) / (mclk_rate / 1000);

	tabla->mbhc_data.t_dce = dce_wait;
	tabla->mbhc_data.t_sta = sta_wait;

	/* LDOH and CFILT are already configured during pdata handling.
	 * Only need to make sure CFILT and bandgap are in Fast mode.
	 * Need to restore defaults once calculation is done.
	 */
	cfilt_mode = snd_soc_read(codec, tabla->mbhc_bias_regs.cfilt_ctl);
	micbias2_cfilt_mode =
	    snd_soc_read(codec, tabla_get_cfilt_reg(codec,
					tabla->pdata->micbias.bias2_cfilt_sel));
	snd_soc_update_bits(codec, tabla->mbhc_bias_regs.cfilt_ctl, 0x40,
			    TABLA_CFILT_FAST_MODE);
	snd_soc_update_bits(codec,
			    tabla_get_cfilt_reg(codec,
					 tabla->pdata->micbias.bias2_cfilt_sel),
			    0x40, TABLA_CFILT_FAST_MODE);

	bg_mode = snd_soc_update_bits(codec, TABLA_A_BIAS_CENTRAL_BG_CTL, 0x02,
				      0x02);

	/* Micbias, CFILT, LDOH, MBHC MUX mode settings
	 * to perform ADC calibration
	 */
	snd_soc_update_bits(codec, tabla->mbhc_bias_regs.ctl_reg, 0x60,
			    tabla->mbhc_cfg.micbias << 5);
	snd_soc_update_bits(codec, tabla->mbhc_bias_regs.ctl_reg, 0x01, 0x00);
	snd_soc_update_bits(codec, TABLA_A_LDO_H_MODE_1, 0x60, 0x60);
	snd_soc_write(codec, TABLA_A_TX_7_MBHC_TEST_CTL, 0x78);
	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_B1_CTL, 0x04, 0x04);

	/* MICBIAS2 routing for calibration */
	bias2_ctl = snd_soc_read(codec, TABLA_A_MICB_2_CTL);
	snd_soc_update_bits(codec, TABLA_A_MICB_1_MBHC, 0x03, TABLA_MICBIAS2);
	snd_soc_write(codec, TABLA_A_MICB_2_CTL,
		      snd_soc_read(codec, tabla->mbhc_bias_regs.ctl_reg));

	/* DCE measurement for 0 volts */
	snd_soc_write(codec, TABLA_A_CDC_MBHC_CLK_CTL, 0x0A);
	snd_soc_write(codec, TABLA_A_CDC_MBHC_EN_CTL, 0x04);
	snd_soc_write(codec, TABLA_A_CDC_MBHC_CLK_CTL, 0x02);
	snd_soc_write(codec, TABLA_A_MBHC_SCALING_MUX_1, 0x81);
	usleep_range(100, 100);
	snd_soc_write(codec, TABLA_A_CDC_MBHC_EN_CTL, 0x04);
	usleep_range(tabla->mbhc_data.t_dce, tabla->mbhc_data.t_dce);
	tabla->mbhc_data.dce_z = tabla_codec_read_dce_result(codec);

	/* DCE measurment for MB voltage */
	snd_soc_write(codec, TABLA_A_CDC_MBHC_CLK_CTL, 0x0A);
	snd_soc_write(codec, TABLA_A_CDC_MBHC_CLK_CTL, 0x02);
	snd_soc_write(codec, TABLA_A_MBHC_SCALING_MUX_1, 0x82);
	usleep_range(100, 100);
	snd_soc_write(codec, TABLA_A_CDC_MBHC_EN_CTL, 0x04);
	usleep_range(tabla->mbhc_data.t_dce, tabla->mbhc_data.t_dce);
	tabla->mbhc_data.dce_mb = tabla_codec_read_dce_result(codec);

	/* Sta measuremnt for 0 volts */
	snd_soc_write(codec, TABLA_A_CDC_MBHC_CLK_CTL, 0x0A);
	snd_soc_write(codec, TABLA_A_CDC_MBHC_EN_CTL, 0x02);
	snd_soc_write(codec, TABLA_A_CDC_MBHC_CLK_CTL, 0x02);
	snd_soc_write(codec, TABLA_A_MBHC_SCALING_MUX_1, 0x81);
	usleep_range(100, 100);
	snd_soc_write(codec, TABLA_A_CDC_MBHC_EN_CTL, 0x02);
	usleep_range(tabla->mbhc_data.t_sta, tabla->mbhc_data.t_sta);
	tabla->mbhc_data.sta_z = tabla_codec_read_sta_result(codec);

	/* STA Measurement for MB Voltage */
	snd_soc_write(codec, TABLA_A_MBHC_SCALING_MUX_1, 0x82);
	usleep_range(100, 100);
	snd_soc_write(codec, TABLA_A_CDC_MBHC_EN_CTL, 0x02);
	usleep_range(tabla->mbhc_data.t_sta, tabla->mbhc_data.t_sta);
	tabla->mbhc_data.sta_mb = tabla_codec_read_sta_result(codec);

	/* Restore default settings. */
	snd_soc_write(codec, TABLA_A_MICB_2_CTL, bias2_ctl);
	snd_soc_update_bits(codec, TABLA_A_MICB_1_MBHC, 0x03,
			    tabla->mbhc_cfg.micbias);

	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_B1_CTL, 0x04, 0x00);
	snd_soc_update_bits(codec,
			    tabla_get_cfilt_reg(codec,
				   tabla->pdata->micbias.bias2_cfilt_sel), 0x40,
			    micbias2_cfilt_mode);
	snd_soc_update_bits(codec, tabla->mbhc_bias_regs.cfilt_ctl, 0x40,
			    cfilt_mode);
	snd_soc_update_bits(codec, TABLA_A_BIAS_CENTRAL_BG_CTL, 0x02, bg_mode);

	snd_soc_write(codec, TABLA_A_MBHC_SCALING_MUX_1, 0x84);
	usleep_range(100, 100);

	wcd9xxx_enable_irq(codec->control_data, WCD9XXX_IRQ_MBHC_POTENTIAL);
	tabla_turn_onoff_rel_detection(codec, true);
}

void *tabla_mbhc_cal_btn_det_mp(const struct tabla_mbhc_btn_detect_cfg* btn_det,
				const enum tabla_mbhc_btn_det_mem mem)
{
	void *ret = &btn_det->_v_btn_low;

	switch (mem) {
	case TABLA_BTN_DET_GAIN:
		ret += sizeof(btn_det->_n_cic);
	case TABLA_BTN_DET_N_CIC:
		ret += sizeof(btn_det->_n_ready);
	case TABLA_BTN_DET_N_READY:
		ret += sizeof(btn_det->_v_btn_high[0]) * btn_det->num_btn;
	case TABLA_BTN_DET_V_BTN_HIGH:
		ret += sizeof(btn_det->_v_btn_low[0]) * btn_det->num_btn;
	case TABLA_BTN_DET_V_BTN_LOW:
		/* do nothing */
		break;
	default:
		ret = NULL;
	}

	return ret;
}

static s16 tabla_scale_v_micb_vddio(struct tabla_priv *tabla, int v,
				    bool tovddio)
{
	int r;
	int vddio_k, mb_k;
	vddio_k = tabla_find_k_value(tabla->pdata->micbias.ldoh_v,
				     VDDIO_MICBIAS_MV);
	mb_k = tabla_find_k_value(tabla->pdata->micbias.ldoh_v,
				  tabla->mbhc_data.micb_mv);
	if (tovddio)
		r = v * vddio_k / mb_k;
	else
		r = v * mb_k / vddio_k;
	return r;
}

static void tabla_mbhc_calc_rel_thres(struct snd_soc_codec *codec, s16 mv)
{
	s16 deltamv;
	struct tabla_priv *tabla;
	struct tabla_mbhc_btn_detect_cfg *btn_det;

	tabla = snd_soc_codec_get_drvdata(codec);
	btn_det = TABLA_MBHC_CAL_BTN_DET_PTR(tabla->mbhc_cfg.calibration);

	tabla->mbhc_data.v_b1_h =
	    tabla_codec_v_sta_dce(codec, DCE,
				  mv + btn_det->v_btn_press_delta_cic);

	tabla->mbhc_data.v_brh = tabla->mbhc_data.v_b1_h;

	tabla->mbhc_data.v_brl = TABLA_MBHC_BUTTON_MIN;

	deltamv = mv + btn_det->v_btn_press_delta_sta;
	tabla->mbhc_data.v_b1_hu = tabla_codec_v_sta_dce(codec, STA, deltamv);

	deltamv = mv + btn_det->v_btn_press_delta_cic;
	tabla->mbhc_data.v_b1_huc = tabla_codec_v_sta_dce(codec, DCE, deltamv);
}

static void tabla_mbhc_set_rel_thres(struct snd_soc_codec *codec, s16 mv)
{
	tabla_mbhc_calc_rel_thres(codec, mv);
	tabla_codec_calibrate_rel(codec);
}

static s16 tabla_mbhc_highest_btn_mv(struct snd_soc_codec *codec)
{
	struct tabla_priv *tabla;
	struct tabla_mbhc_btn_detect_cfg *btn_det;
	u16 *btn_high;

	tabla = snd_soc_codec_get_drvdata(codec);
	btn_det = TABLA_MBHC_CAL_BTN_DET_PTR(tabla->mbhc_cfg.calibration);
	btn_high = tabla_mbhc_cal_btn_det_mp(btn_det, TABLA_BTN_DET_V_BTN_HIGH);

	return btn_high[btn_det->num_btn - 1];
}

static void tabla_mbhc_calc_thres(struct snd_soc_codec *codec)
{
	struct tabla_priv *tabla;
	struct tabla_mbhc_btn_detect_cfg *btn_det;
	struct tabla_mbhc_plug_type_cfg *plug_type;
	u8 *n_ready;

	tabla = snd_soc_codec_get_drvdata(codec);
	btn_det = TABLA_MBHC_CAL_BTN_DET_PTR(tabla->mbhc_cfg.calibration);
	plug_type = TABLA_MBHC_CAL_PLUG_TYPE_PTR(tabla->mbhc_cfg.calibration);

	n_ready = tabla_mbhc_cal_btn_det_mp(btn_det, TABLA_BTN_DET_N_READY);
	if (tabla->mbhc_cfg.mclk_rate == TABLA_MCLK_RATE_12288KHZ) {
		tabla->mbhc_data.npoll = 4;
		tabla->mbhc_data.nbounce_wait = 30;
	} else if (tabla->mbhc_cfg.mclk_rate == TABLA_MCLK_RATE_9600KHZ) {
		tabla->mbhc_data.npoll = 7;
		tabla->mbhc_data.nbounce_wait = 23;
	}

	tabla->mbhc_data.t_sta_dce = ((1000 * 256) /
				      (tabla->mbhc_cfg.mclk_rate / 1000) *
				      n_ready[tabla_codec_mclk_index(tabla)]) +
				     10;
	tabla->mbhc_data.v_ins_hu =
	    tabla_codec_v_sta_dce(codec, STA, plug_type->v_hs_max);
	tabla->mbhc_data.v_ins_h =
	    tabla_codec_v_sta_dce(codec, DCE, plug_type->v_hs_max);

	tabla->mbhc_data.v_inval_ins_low = TABLA_MBHC_FAKE_INSERT_LOW;
	if (tabla->mbhc_cfg.gpio)
		tabla->mbhc_data.v_inval_ins_high =
		    TABLA_MBHC_FAKE_INSERT_HIGH;
	else
		tabla->mbhc_data.v_inval_ins_high =
		    TABLA_MBHC_FAKE_INS_HIGH_NO_GPIO;

	if (tabla->mbhc_data.micb_mv != VDDIO_MICBIAS_MV) {
		tabla->mbhc_data.adj_v_hs_max =
		    tabla_scale_v_micb_vddio(tabla, plug_type->v_hs_max, true);
		tabla->mbhc_data.adj_v_ins_hu =
		    tabla_codec_v_sta_dce(codec, STA,
					  tabla->mbhc_data.adj_v_hs_max);
		tabla->mbhc_data.adj_v_ins_h =
		    tabla_codec_v_sta_dce(codec, DCE,
					  tabla->mbhc_data.adj_v_hs_max);
		tabla->mbhc_data.v_inval_ins_low =
		    tabla_scale_v_micb_vddio(tabla,
					     tabla->mbhc_data.v_inval_ins_low,
					     false);
		tabla->mbhc_data.v_inval_ins_high =
		    tabla_scale_v_micb_vddio(tabla,
					     tabla->mbhc_data.v_inval_ins_high,
					     false);
	}

	tabla_mbhc_calc_rel_thres(codec, tabla_mbhc_highest_btn_mv(codec));

	tabla->mbhc_data.v_no_mic =
	    tabla_codec_v_sta_dce(codec, STA, plug_type->v_no_mic);
}

void tabla_mbhc_init(struct snd_soc_codec *codec)
{
	struct tabla_priv *tabla;
	struct tabla_mbhc_general_cfg *generic;
	struct tabla_mbhc_btn_detect_cfg *btn_det;
	int n;
	u8 *n_cic, *gain;
	struct wcd9xxx *tabla_core = dev_get_drvdata(codec->dev->parent);

	tabla = snd_soc_codec_get_drvdata(codec);
	generic = TABLA_MBHC_CAL_GENERAL_PTR(tabla->mbhc_cfg.calibration);
	btn_det = TABLA_MBHC_CAL_BTN_DET_PTR(tabla->mbhc_cfg.calibration);

	for (n = 0; n < 8; n++) {
		if ((!TABLA_IS_1_X(tabla_core->version)) || n != 7) {
			snd_soc_update_bits(codec,
					    TABLA_A_CDC_MBHC_FEATURE_B1_CFG,
					    0x07, n);
			snd_soc_write(codec, TABLA_A_CDC_MBHC_FEATURE_B2_CFG,
				      btn_det->c[n]);
		}
	}
	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_B2_CTL, 0x07,
			    btn_det->nc);

	n_cic = tabla_mbhc_cal_btn_det_mp(btn_det, TABLA_BTN_DET_N_CIC);
	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_TIMER_B6_CTL, 0xFF,
			    n_cic[tabla_codec_mclk_index(tabla)]);

	gain = tabla_mbhc_cal_btn_det_mp(btn_det, TABLA_BTN_DET_GAIN);
	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_B2_CTL, 0x78,
			    gain[tabla_codec_mclk_index(tabla)] << 3);

	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_TIMER_B4_CTL, 0x70,
			    generic->mbhc_nsa << 4);

	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_TIMER_B4_CTL, 0x0F,
			    btn_det->n_meas);

	snd_soc_write(codec, TABLA_A_CDC_MBHC_TIMER_B5_CTL, generic->mbhc_navg);

	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_B1_CTL, 0x80, 0x80);

	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_B1_CTL, 0x78,
			    btn_det->mbhc_nsc << 3);

	snd_soc_update_bits(codec, tabla->reg_addr.micb_4_mbhc, 0x03,
			    TABLA_MICBIAS2);

	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_B1_CTL, 0x02, 0x02);

	snd_soc_update_bits(codec, TABLA_A_MBHC_SCALING_MUX_2, 0xF0, 0xF0);

	/* override mbhc's micbias */
	snd_soc_update_bits(codec, TABLA_A_MICB_1_MBHC, 0x03,
			    tabla->mbhc_cfg.micbias);
}

static bool tabla_mbhc_fw_validate(const struct firmware *fw)
{
	u32 cfg_offset;
	struct tabla_mbhc_imped_detect_cfg *imped_cfg;
	struct tabla_mbhc_btn_detect_cfg *btn_cfg;

	if (fw->size < TABLA_MBHC_CAL_MIN_SIZE)
		return false;

	/* previous check guarantees that there is enough fw data up
	 * to num_btn
	 */
	btn_cfg = TABLA_MBHC_CAL_BTN_DET_PTR(fw->data);
	cfg_offset = (u32) ((void *) btn_cfg - (void *) fw->data);
	if (fw->size < (cfg_offset + TABLA_MBHC_CAL_BTN_SZ(btn_cfg)))
		return false;

	/* previous check guarantees that there is enough fw data up
	 * to start of impedance detection configuration
	 */
	imped_cfg = TABLA_MBHC_CAL_IMPED_DET_PTR(fw->data);
	cfg_offset = (u32) ((void *) imped_cfg - (void *) fw->data);

	if (fw->size < (cfg_offset + TABLA_MBHC_CAL_IMPED_MIN_SZ))
		return false;

	if (fw->size < (cfg_offset + TABLA_MBHC_CAL_IMPED_SZ(imped_cfg)))
		return false;

	return true;
}

/* called under codec_resource_lock acquisition */
static int tabla_determine_button(const struct tabla_priv *priv,
				  const s32 micmv)
{
	s16 *v_btn_low, *v_btn_high;
	struct tabla_mbhc_btn_detect_cfg *btn_det;
	int i, btn = -1;

	btn_det = TABLA_MBHC_CAL_BTN_DET_PTR(priv->mbhc_cfg.calibration);
	v_btn_low = tabla_mbhc_cal_btn_det_mp(btn_det, TABLA_BTN_DET_V_BTN_LOW);
	v_btn_high = tabla_mbhc_cal_btn_det_mp(btn_det,
				TABLA_BTN_DET_V_BTN_HIGH);

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

static int tabla_get_button_mask(const int btn)
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

static irqreturn_t tabla_dce_handler(int irq, void *data)
{
	int i, mask;
	short dce, sta;
	s32 mv, mv_s, stamv, stamv_s;
	bool vddio;
	u16 *btn_high;
	int btn = -1, meas = 0;
	struct tabla_priv *priv = data;
	const struct tabla_mbhc_btn_detect_cfg *d =
	    TABLA_MBHC_CAL_BTN_DET_PTR(priv->mbhc_cfg.calibration);
	short btnmeas[d->n_btn_meas + 1];
	struct snd_soc_codec *codec = priv->codec;
	struct wcd9xxx *core = dev_get_drvdata(priv->codec->dev->parent);
	struct wcd9xxx_core_resource *core_res = &core->core_res;
	int n_btn_meas = d->n_btn_meas;
	u8 mbhc_status = snd_soc_read(codec, TABLA_A_CDC_MBHC_B1_STATUS) & 0x3E;

	pr_debug("%s: enter\n", __func__);

	btn_high = tabla_mbhc_cal_btn_det_mp(d, TABLA_BTN_DET_V_BTN_HIGH);
	TABLA_ACQUIRE_LOCK(priv->codec_resource_lock);
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

	dce = tabla_codec_read_dce_result(codec);
	mv = tabla_codec_sta_dce_v(codec, 1, dce);

	/* If GPIO interrupt already kicked in, ignore button press */
	if (priv->in_gpio_handler) {
		pr_debug("%s: GPIO State Changed, ignore button press\n",
			 __func__);
		btn = -1;
		goto done;
	}
	vddio = !priv->mbhc_cfg.micbias_always_on &&
		(priv->mbhc_data.micb_mv != VDDIO_MICBIAS_MV &&
		 priv->mbhc_micbias_switched);
	mv_s = vddio ? tabla_scale_v_micb_vddio(priv, mv, false) : mv;

	if (mbhc_status != TABLA_MBHC_STATUS_REL_DETECTION) {
		if (priv->mbhc_last_resume &&
		    !time_after(jiffies, priv->mbhc_last_resume + HZ)) {
			pr_debug("%s: Button is already released shortly after "
				 "resume\n", __func__);
			n_btn_meas = 0;
		}
	}

	/* save hw dce */
	btnmeas[meas++] = tabla_determine_button(priv, mv_s);
	pr_debug("%s: meas HW - DCE %x,%d,%d button %d\n", __func__,
		 dce, mv, mv_s, btnmeas[0]);
	if (n_btn_meas == 0) {
		sta = tabla_codec_read_sta_result(codec);
		stamv_s = stamv = tabla_codec_sta_dce_v(codec, 0, sta);
		if (vddio)
			stamv_s = tabla_scale_v_micb_vddio(priv, stamv, false);
		btn = tabla_determine_button(priv, stamv_s);
		pr_debug("%s: meas HW - STA %x,%d,%d button %d\n", __func__,
			 sta, stamv, stamv_s, btn);
		BUG_ON(meas != 1);
		if (btnmeas[0] != btn)
			btn = -1;
	}

	/* determine pressed button */
	for (; ((d->n_btn_meas) && (meas < (d->n_btn_meas + 1))); meas++) {
		dce = tabla_codec_sta_dce(codec, 1, false);
		mv = tabla_codec_sta_dce_v(codec, 1, dce);
		mv_s = vddio ? tabla_scale_v_micb_vddio(priv, mv, false) : mv;

		btnmeas[meas] = tabla_determine_button(priv, mv_s);
		pr_debug("%s: meas %d - DCE %x,%d,%d button %d\n",
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
			pr_debug("%s: GPIO already triggered, ignore button "
				 "press\n", __func__);
			goto done;
		}
		/* narrow down release threshold */
		tabla_mbhc_set_rel_thres(codec, btn_high[btn]);
		mask = tabla_get_button_mask(btn);
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
	TABLA_RELEASE_LOCK(priv->codec_resource_lock);
	return IRQ_HANDLED;
}

static int tabla_is_fake_press(struct tabla_priv *priv)
{
	int i;
	int r = 0;
	struct snd_soc_codec *codec = priv->codec;
	const int dces = MBHC_NUM_DCE_PLUG_DETECT;
	s16 mb_v, v_ins_hu, v_ins_h;

	v_ins_hu = tabla_get_current_v_ins(priv, true);
	v_ins_h = tabla_get_current_v_ins(priv, false);

	for (i = 0; i < dces; i++) {
		usleep_range(10000, 10000);
		if (i == 0) {
			mb_v = tabla_codec_sta_dce(codec, 0, true);
			pr_debug("%s: STA[0]: %d,%d\n", __func__, mb_v,
				 tabla_codec_sta_dce_v(codec, 0, mb_v));
			if (mb_v < (s16)priv->mbhc_data.v_b1_hu ||
			    mb_v > v_ins_hu) {
				r = 1;
				break;
			}
		} else {
			mb_v = tabla_codec_sta_dce(codec, 1, true);
			pr_debug("%s: DCE[%d]: %d,%d\n", __func__, i, mb_v,
				 tabla_codec_sta_dce_v(codec, 1, mb_v));
			if (mb_v < (s16)priv->mbhc_data.v_b1_h ||
			    mb_v > v_ins_h) {
				r = 1;
				break;
			}
		}
	}

	return r;
}

static irqreturn_t tabla_release_handler(int irq, void *data)
{
	int ret;
	struct tabla_priv *priv = data;
	struct snd_soc_codec *codec = priv->codec;

	pr_debug("%s: enter\n", __func__);

	TABLA_ACQUIRE_LOCK(priv->codec_resource_lock);
	priv->mbhc_state = MBHC_STATE_RELEASE;

	tabla_codec_drive_v_to_micbias(codec, 10000);

	if (priv->buttons_pressed & TABLA_JACK_BUTTON_MASK) {
		ret = tabla_cancel_btn_work(priv);
		if (ret == 0) {
			pr_debug("%s: Reporting long button release event\n",
				 __func__);
			if (priv->mbhc_cfg.button_jack)
				tabla_snd_soc_jack_report(priv,
						  priv->mbhc_cfg.button_jack, 0,
						  priv->buttons_pressed);
		} else {
			if (tabla_is_fake_press(priv)) {
				pr_debug("%s: Fake button press interrupt\n",
					 __func__);
			} else if (priv->mbhc_cfg.button_jack) {
				if (priv->in_gpio_handler) {
					pr_debug("%s: GPIO kicked in, ignore\n",
						 __func__);
				} else {
					pr_debug("%s: Reporting short button "
						 "press and release\n",
						 __func__);
					tabla_snd_soc_jack_report(priv,
						     priv->mbhc_cfg.button_jack,
						     priv->buttons_pressed,
						     priv->buttons_pressed);
					tabla_snd_soc_jack_report(priv,
						  priv->mbhc_cfg.button_jack, 0,
						  priv->buttons_pressed);
				}
			}
		}

		priv->buttons_pressed &= ~TABLA_JACK_BUTTON_MASK;
	}

	/* revert narrowed release threshold */
	tabla_mbhc_calc_rel_thres(codec, tabla_mbhc_highest_btn_mv(codec));
	tabla_codec_calibrate_hs_polling(codec);

	if (priv->mbhc_cfg.gpio)
		msleep(TABLA_MBHC_GPIO_REL_DEBOUNCE_TIME_MS);

	tabla_codec_start_hs_polling(codec);

	pr_debug("%s: leave\n", __func__);
	TABLA_RELEASE_LOCK(priv->codec_resource_lock);
	return IRQ_HANDLED;
}

static void tabla_codec_shutdown_hs_removal_detect(struct snd_soc_codec *codec)
{
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);
	const struct tabla_mbhc_general_cfg *generic =
	    TABLA_MBHC_CAL_GENERAL_PTR(tabla->mbhc_cfg.calibration);

	if (!tabla->mclk_enabled && !tabla->mbhc_polling_active)
		tabla_codec_enable_config_mode(codec, 1);

	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_CLK_CTL, 0x2, 0x2);
	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_B1_CTL, 0x6, 0x0);

	snd_soc_update_bits(codec, tabla->mbhc_bias_regs.mbhc_reg, 0x80, 0x00);

	usleep_range(generic->t_shutdown_plug_rem,
		     generic->t_shutdown_plug_rem);

	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_CLK_CTL, 0xA, 0x8);
	if (!tabla->mclk_enabled && !tabla->mbhc_polling_active)
		tabla_codec_enable_config_mode(codec, 0);

	snd_soc_write(codec, TABLA_A_MBHC_SCALING_MUX_1, 0x00);
}

static void tabla_codec_cleanup_hs_polling(struct snd_soc_codec *codec)
{
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);

	tabla_codec_shutdown_hs_removal_detect(codec);

	if (!tabla->mclk_enabled) {
		tabla_codec_disable_clock_block(codec);
		tabla_codec_enable_bandgap(codec, TABLA_BANDGAP_OFF);
	}

	tabla->mbhc_polling_active = false;
	tabla->mbhc_state = MBHC_STATE_NONE;
}

static irqreturn_t tabla_hphl_ocp_irq(int irq, void *data)
{
	struct tabla_priv *tabla = data;
	struct snd_soc_codec *codec;

	pr_info("%s: received HPHL OCP irq\n", __func__);

	if (tabla) {
		codec = tabla->codec;
		if ((tabla->hphlocp_cnt < TABLA_OCP_ATTEMPT) &&
		    (!tabla->hphrocp_cnt)) {
			pr_info("%s: retry\n", __func__);
			tabla->hphlocp_cnt++;
			snd_soc_update_bits(codec, TABLA_A_RX_HPH_OCP_CTL, 0x10,
					    0x00);
			snd_soc_update_bits(codec, TABLA_A_RX_HPH_OCP_CTL, 0x10,
					    0x10);
		} else {
			wcd9xxx_disable_irq(codec->control_data,
					  WCD9XXX_IRQ_HPH_PA_OCPL_FAULT);
			tabla->hph_status |= SND_JACK_OC_HPHL;
			if (tabla->mbhc_cfg.headset_jack)
				tabla_snd_soc_jack_report(tabla,
						   tabla->mbhc_cfg.headset_jack,
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
		if ((tabla->hphrocp_cnt < TABLA_OCP_ATTEMPT) &&
		    (!tabla->hphlocp_cnt)) {
			pr_info("%s: retry\n", __func__);
			tabla->hphrocp_cnt++;
			snd_soc_update_bits(codec, TABLA_A_RX_HPH_OCP_CTL, 0x10,
					    0x00);
			snd_soc_update_bits(codec, TABLA_A_RX_HPH_OCP_CTL, 0x10,
					    0x10);
		} else {
			wcd9xxx_disable_irq(codec->control_data,
					  WCD9XXX_IRQ_HPH_PA_OCPR_FAULT);
			tabla->hph_status |= SND_JACK_OC_HPHR;
			if (tabla->mbhc_cfg.headset_jack)
				tabla_snd_soc_jack_report(tabla,
						   tabla->mbhc_cfg.headset_jack,
						   tabla->hph_status,
						   TABLA_JACK_MASK);
		}
	} else {
		pr_err("%s: Bad tabla private data\n", __func__);
	}

	return IRQ_HANDLED;
}

static bool tabla_is_inval_ins_range(struct snd_soc_codec *codec,
				     s32 mic_volt, bool highhph, bool *highv)
{
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);
	bool invalid = false;
	s16 v_hs_max;

	/* Perform this check only when the high voltage headphone
	 * needs to be considered as invalid
	 */
	v_hs_max = tabla_get_current_v_hs_max(tabla);
	*highv = mic_volt > v_hs_max;
	if (!highhph && *highv)
		invalid = true;
	else if (mic_volt < tabla->mbhc_data.v_inval_ins_high &&
		 (mic_volt > tabla->mbhc_data.v_inval_ins_low))
		invalid = true;

	return invalid;
}

static bool tabla_is_inval_ins_delta(struct snd_soc_codec *codec,
				     int mic_volt, int mic_volt_prev,
				     int threshold)
{
	return abs(mic_volt - mic_volt_prev) > threshold;
}

/* called under codec_resource_lock acquisition */
void tabla_find_plug_and_report(struct snd_soc_codec *codec,
				enum tabla_mbhc_plug_type plug_type)
{
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s: enter current_plug(%d) new_plug(%d)\n",
		 __func__, tabla->current_plug, plug_type);

	if (plug_type == PLUG_TYPE_HEADPHONE &&
	    tabla->current_plug == PLUG_TYPE_NONE) {
		/* Nothing was reported previously
		 * report a headphone or unsupported
		 */
		tabla_codec_report_plug(codec, 1, SND_JACK_HEADPHONE);
		tabla_codec_cleanup_hs_polling(codec);
	} else if (plug_type == PLUG_TYPE_GND_MIC_SWAP) {
		if (!tabla->mbhc_cfg.detect_extn_cable) {
			if (tabla->current_plug == PLUG_TYPE_HEADSET)
				tabla_codec_report_plug(codec, 0,
							SND_JACK_HEADSET);
			else if (tabla->current_plug == PLUG_TYPE_HEADPHONE)
				tabla_codec_report_plug(codec, 0,
							SND_JACK_HEADPHONE);
		}
		tabla_codec_report_plug(codec, 1, SND_JACK_UNSUPPORTED);
		tabla_codec_cleanup_hs_polling(codec);
	} else if (plug_type == PLUG_TYPE_HEADSET) {
		/* If Headphone was reported previously, this will
		 * only report the mic line
		 */
		tabla_codec_report_plug(codec, 1, SND_JACK_HEADSET);
		if (!tabla->mbhc_micbias_switched &&
			tabla_is_hph_pa_on(codec)) {
			/*If the headphone path is on, switch the micbias
			to VDDIO to avoid noise due to button polling */
			tabla_codec_switch_micbias(codec, 1);
			pr_debug("%s: HPH path is still up\n", __func__);
		}
		msleep(100);
		tabla_codec_start_hs_polling(codec);
	} else if (plug_type == PLUG_TYPE_HIGH_HPH) {
		if (tabla->mbhc_cfg.detect_extn_cable) {
			/* High impedance device found. Report as LINEOUT*/
			tabla_codec_report_plug(codec, 1, SND_JACK_LINEOUT);
			tabla_codec_cleanup_hs_polling(codec);
			pr_debug("%s: setup mic trigger for further detection\n",
				 __func__);
			tabla->lpi_enabled = true;
			/*
			 * Do not enable HPHL trigger. If playback is active,
			 * it might lead to continuous false HPHL triggers
			 */
			tabla_codec_enable_hs_detect(codec, 1,
						     MBHC_USE_MB_TRIGGER,
						     false);
		} else {
			if (tabla->current_plug == PLUG_TYPE_NONE)
				tabla_codec_report_plug(codec, 1,
							SND_JACK_HEADPHONE);
			tabla_codec_cleanup_hs_polling(codec);
			pr_debug("setup mic trigger for further detection\n");
			tabla->lpi_enabled = true;
			tabla_codec_enable_hs_detect(codec, 1,
						     MBHC_USE_MB_TRIGGER |
						     MBHC_USE_HPHL_TRIGGER,
						     false);
		}
	} else {
		WARN(1, "Unexpected current plug_type %d, plug_type %d\n",
		     tabla->current_plug, plug_type);
	}
	pr_debug("%s: leave\n", __func__);
}

/* should be called under interrupt context that hold suspend */
static void tabla_schedule_hs_detect_plug(struct tabla_priv *tabla,
	struct work_struct *correct_plug_work)
{
	struct wcd9xxx *core = tabla->codec->control_data;
	struct wcd9xxx_core_resource *core_res = &core->core_res;
	pr_debug("%s: scheduling tabla_hs_correct_gpio_plug\n", __func__);
	tabla->hs_detect_work_stop = false;
	wcd9xxx_lock_sleep(core_res);
	schedule_work(correct_plug_work);
}

/* called under codec_resource_lock acquisition */
static void tabla_cancel_hs_detect_plug(struct tabla_priv *tabla,
		struct work_struct *correct_plug_work)
{
	struct wcd9xxx *core = tabla->codec->control_data;
	struct wcd9xxx_core_resource *core_res = &core->core_res;
	pr_debug("%s: canceling hs_correct_plug_work\n", __func__);
	tabla->hs_detect_work_stop = true;
	wmb();
	TABLA_RELEASE_LOCK(tabla->codec_resource_lock);
	if (cancel_work_sync(correct_plug_work)) {
		pr_debug("%s: hs_correct_plug_work is canceled\n", __func__);
		wcd9xxx_unlock_sleep(core_res);
	}
	TABLA_ACQUIRE_LOCK(tabla->codec_resource_lock);
}

static bool tabla_hs_gpio_level_remove(struct tabla_priv *tabla)
{
	return (gpio_get_value_cansleep(tabla->mbhc_cfg.gpio) !=
		tabla->mbhc_cfg.gpio_level_insert);
}

/* called under codec_resource_lock acquisition */
static void tabla_codec_hphr_gnd_switch(struct snd_soc_codec *codec, bool on)
{
	snd_soc_update_bits(codec, TABLA_A_MBHC_HPH, 0x01, on);
	if (on)
		usleep_range(5000, 5000);
}


static void tabla_codec_onoff_vddio_switch(struct snd_soc_codec *codec, bool on)
{
	bool override;
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s: enter\n", __func__);
	if (on) {
		override = snd_soc_read(codec, TABLA_A_CDC_MBHC_B1_CTL) & 0x04;
		if (!override)
			tabla_turn_onoff_override(codec, true);

		/* enable the vddio switch */
		snd_soc_update_bits(codec, tabla->mbhc_bias_regs.mbhc_reg,
				    0x91, 0x81);

		/* deroute the override from MicBias2 to MicBias4 */
		snd_soc_update_bits(codec, TABLA_A_MICB_1_MBHC,
				    0x03, 0x03);

		usleep_range(MBHC_VDDIO_SWITCH_WAIT_MS * 1000,
				MBHC_VDDIO_SWITCH_WAIT_MS * 1000);

		if (!override)
			tabla_turn_onoff_override(codec, false);
		tabla->mbhc_micbias_switched = true;
		pr_debug("%s: VDDIO switch enabled\n", __func__);

	} else {

		snd_soc_update_bits(codec, tabla->mbhc_bias_regs.mbhc_reg,
				    0x91, 0x00);

		/* reroute the override to MicBias2 */
		snd_soc_update_bits(codec, TABLA_A_MICB_1_MBHC,
				    0x03, 0x01);

		tabla->mbhc_micbias_switched = false;
		pr_debug("%s: VDDIO switch disabled\n", __func__);
	}
}

/* called under codec_resource_lock acquisition and mbhc override = 1 */
static enum tabla_mbhc_plug_type
tabla_codec_get_plug_type(struct snd_soc_codec *codec, bool highhph)
{
	int i;
	bool gndswitch, vddioswitch;
	int scaled;
	struct tabla_mbhc_plug_type_cfg *plug_type_ptr;
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);
	const bool vddio = !tabla->mbhc_cfg.micbias_always_on &&
			   (tabla->mbhc_data.micb_mv != VDDIO_MICBIAS_MV);
	int num_det = (MBHC_NUM_DCE_PLUG_DETECT + vddio);
	enum tabla_mbhc_plug_type plug_type[num_det];
	s16 mb_v[num_det];
	s32 mic_mv[num_det];
	bool inval;
	bool highdelta = false;
	bool ahighv = false, highv;
	bool gndmicswapped = false;

	pr_debug("%s: enter\n", __func__);

	/* make sure override is on */
	WARN_ON(!(snd_soc_read(codec, TABLA_A_CDC_MBHC_B1_CTL) & 0x04));

	/* GND and MIC swap detection requires at least 2 rounds of DCE */
	BUG_ON(num_det < 2);

	plug_type_ptr =
	    TABLA_MBHC_CAL_PLUG_TYPE_PTR(tabla->mbhc_cfg.calibration);

	plug_type[0] = PLUG_TYPE_INVALID;

	/* performs DCEs for N times
	 * 1st: check if voltage is in invalid range
	 * 2nd - N-2nd: check voltage range and delta
	 * N-1st: check voltage range, delta with HPHR GND switch
	 * Nth: check voltage range with VDDIO switch */
	for (i = 0; i < num_det; i++) {
		gndswitch = (i == (num_det - 2));
		vddioswitch = (i == (num_det - 1));
		if (i == 0) {
			mb_v[i] = tabla_codec_setup_hs_polling(codec);
			mic_mv[i] = tabla_codec_sta_dce_v(codec, 1 , mb_v[i]);
			inval = tabla_is_inval_ins_range(codec, mic_mv[i],
							 highhph, &highv);
			ahighv |= highv;
			scaled = mic_mv[i];
		} else {
			if (vddioswitch)
				tabla_codec_onoff_vddio_switch(codec, true);

			if (gndswitch)
				tabla_codec_hphr_gnd_switch(codec, true);
			mb_v[i] = __tabla_codec_sta_dce(codec, 1, true, true);
			mic_mv[i] = tabla_codec_sta_dce_v(codec, 1 , mb_v[i]);
			if (vddioswitch)
				scaled = tabla_scale_v_micb_vddio(tabla,
								  mic_mv[i],
								  false);
			else
				scaled = mic_mv[i];
			/* !gndswitch & vddioswitch means the previous DCE
			 * was done with gndswitch, don't compare with DCE
			 * with gndswitch */
			highdelta = tabla_is_inval_ins_delta(codec, scaled,
					mic_mv[i - 1],
					TABLA_MBHC_FAKE_INS_DELTA_SCALED_MV);
			inval = (tabla_is_inval_ins_range(codec, mic_mv[i],
							  highhph, &highv) ||
				 highdelta);
			ahighv |= highv;
			if (gndswitch)
				tabla_codec_hphr_gnd_switch(codec, false);
			if (vddioswitch)
				tabla_codec_onoff_vddio_switch(codec, false);
		}
		pr_debug("%s: DCE #%d, %04x, V %d, scaled V %d, GND %d, "
			 "VDDIO %d, inval %d\n", __func__,
			 i + 1, mb_v[i] & 0xffff, mic_mv[i], scaled, gndswitch,
			 vddioswitch, inval);
		/* don't need to run further DCEs */
		if ((ahighv || !vddioswitch) && inval)
			break;
		mic_mv[i] = scaled;

		/*
		 * claim UNSUPPORTED plug insertion when
		 * good headset is detected but HPHR GND switch makes
		 * delta difference
		 */
		if (i == (num_det - 2) && highdelta && !ahighv)
			gndmicswapped = true;
		else if (i == (num_det - 1) && inval) {
			if (gndmicswapped)
				plug_type[0] = PLUG_TYPE_GND_MIC_SWAP;
			else
				plug_type[0] = PLUG_TYPE_INVALID;
		}
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
			pr_debug("%s: Detect attempt %d, detected High "
				 "Headphone\n", __func__, i);
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
	pr_debug("%s: leave\n", __func__);
	return plug_type[0];
}

static void tabla_hs_correct_gpio_plug(struct work_struct *work)
{
	struct tabla_priv *tabla;
	struct snd_soc_codec *codec;
	int retry = 0, pt_gnd_mic_swap_cnt = 0;
	bool correction = false;
	enum tabla_mbhc_plug_type plug_type = PLUG_TYPE_INVALID;
	unsigned long timeout;
	struct wcd9xxx *core;
	struct wcd9xxx_core_resource *core_res;

	tabla = container_of(work, struct tabla_priv, hs_correct_plug_work);
	codec = tabla->codec;
	core = tabla->codec->control_data;
	core_res = &core->core_res;

	pr_debug("%s: enter\n", __func__);
	tabla->mbhc_cfg.mclk_cb_fn(codec, 1, false);

	/* Keep override on during entire plug type correction work.
	 *
	 * This is okay under the assumption that any GPIO irqs which use
	 * MBHC block cancel and sync this work so override is off again
	 * prior to GPIO interrupt handler's MBHC block usage.
	 * Also while this correction work is running, we can guarantee
	 * DAPM doesn't use any MBHC block as this work only runs with
	 * headphone detection.
	 */
	tabla_turn_onoff_override(codec, true);

	timeout = jiffies + msecs_to_jiffies(TABLA_HS_DETECT_PLUG_TIME_MS);
	while (!time_after(jiffies, timeout)) {
		++retry;
		rmb();
		if (tabla->hs_detect_work_stop) {
			pr_debug("%s: stop requested\n", __func__);
			break;
		}

		msleep(TABLA_HS_DETECT_PLUG_INERVAL_MS);
		if (tabla_hs_gpio_level_remove(tabla)) {
			pr_debug("%s: GPIO value is low\n", __func__);
			break;
		}

		/* can race with removal interrupt */
		TABLA_ACQUIRE_LOCK(tabla->codec_resource_lock);
		plug_type = tabla_codec_get_plug_type(codec, true);
		TABLA_RELEASE_LOCK(tabla->codec_resource_lock);

		pr_debug("%s: attempt(%d) current_plug(%d) new_plug(%d)\n",
			 __func__, retry, tabla->current_plug, plug_type);
		if (plug_type == PLUG_TYPE_INVALID) {
			pr_debug("Invalid plug in attempt # %d\n", retry);
			if (!tabla->mbhc_cfg.detect_extn_cable &&
			    retry == NUM_ATTEMPTS_TO_REPORT &&
			    tabla->current_plug == PLUG_TYPE_NONE) {
				tabla_codec_report_plug(codec, 1,
							SND_JACK_HEADPHONE);
			}
		} else if (plug_type == PLUG_TYPE_HEADPHONE) {
			pr_debug("Good headphone detected, continue polling mic\n");
			if (tabla->mbhc_cfg.detect_extn_cable) {
				if (tabla->current_plug != plug_type)
					tabla_codec_report_plug(codec, 1,
							SND_JACK_HEADPHONE);
			} else if (tabla->current_plug == PLUG_TYPE_NONE)
				tabla_codec_report_plug(codec, 1,
							SND_JACK_HEADPHONE);
		} else {
			if (plug_type == PLUG_TYPE_GND_MIC_SWAP) {
				pt_gnd_mic_swap_cnt++;
				if (pt_gnd_mic_swap_cnt <
				    TABLA_MBHC_GND_MIC_SWAP_THRESHOLD)
					continue;
				else if (pt_gnd_mic_swap_cnt >
					 TABLA_MBHC_GND_MIC_SWAP_THRESHOLD) {
					/* This is due to GND/MIC switch didn't
					 * work,  Report unsupported plug */
				} else if (tabla->mbhc_cfg.swap_gnd_mic) {
					/* if switch is toggled, check again,
					 * otherwise report unsupported plug */
					if (tabla->mbhc_cfg.swap_gnd_mic(codec))
						continue;
				}
			} else
				pt_gnd_mic_swap_cnt = 0;

			TABLA_ACQUIRE_LOCK(tabla->codec_resource_lock);
			/* Turn off override */
			tabla_turn_onoff_override(codec, false);
			/* The valid plug also includes PLUG_TYPE_GND_MIC_SWAP
			 */
			tabla_find_plug_and_report(codec, plug_type);
			TABLA_RELEASE_LOCK(tabla->codec_resource_lock);
			pr_debug("Attempt %d found correct plug %d\n", retry,
				 plug_type);
			correction = true;
			break;
		}
	}

	/* Turn off override */
	if (!correction)
		tabla_turn_onoff_override(codec, false);

	tabla->mbhc_cfg.mclk_cb_fn(codec, 0, false);

	if (tabla->mbhc_cfg.detect_extn_cable) {
		TABLA_ACQUIRE_LOCK(tabla->codec_resource_lock);
		if (tabla->current_plug == PLUG_TYPE_HEADPHONE ||
		    tabla->current_plug == PLUG_TYPE_GND_MIC_SWAP ||
		    tabla->current_plug == PLUG_TYPE_INVALID ||
		    plug_type == PLUG_TYPE_INVALID) {
			/* Enable removal detection */
			tabla_codec_cleanup_hs_polling(codec);
			tabla_codec_enable_hs_detect(codec, 0, 0, false);
		}
		TABLA_RELEASE_LOCK(tabla->codec_resource_lock);
	}
	pr_debug("%s: leave current_plug(%d)\n",
		 __func__, tabla->current_plug);
	/* unlock sleep */
	wcd9xxx_unlock_sleep(core_res);
}

/* called under codec_resource_lock acquisition */
static void tabla_codec_decide_gpio_plug(struct snd_soc_codec *codec)
{
	enum tabla_mbhc_plug_type plug_type;
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s: enter\n", __func__);

	tabla_turn_onoff_override(codec, true);
	plug_type = tabla_codec_get_plug_type(codec, true);
	tabla_turn_onoff_override(codec, false);

	if (tabla_hs_gpio_level_remove(tabla)) {
		pr_debug("%s: GPIO value is low when determining plug\n",
			 __func__);
		return;
	}

	if (plug_type == PLUG_TYPE_INVALID ||
	    plug_type == PLUG_TYPE_GND_MIC_SWAP) {
		tabla_schedule_hs_detect_plug(tabla,
					&tabla->hs_correct_plug_work);
	} else if (plug_type == PLUG_TYPE_HEADPHONE) {
		tabla_codec_report_plug(codec, 1, SND_JACK_HEADPHONE);

		tabla_schedule_hs_detect_plug(tabla,
					&tabla->hs_correct_plug_work);
	} else {
		pr_debug("%s: Valid plug found, determine plug type %d\n",
			 __func__, plug_type);
		tabla_find_plug_and_report(codec, plug_type);
	}
	pr_debug("%s: leave\n", __func__);
}

/* called under codec_resource_lock acquisition */
static void tabla_codec_detect_plug_type(struct snd_soc_codec *codec)
{
	enum tabla_mbhc_plug_type plug_type;
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);
	const struct tabla_mbhc_plug_detect_cfg *plug_det =
	    TABLA_MBHC_CAL_PLUG_DET_PTR(tabla->mbhc_cfg.calibration);
	pr_debug("%s: enter\n", __func__);
	/* Turn on the override,
	 * tabla_codec_setup_hs_polling requires override on */
	tabla_turn_onoff_override(codec, true);

	if (plug_det->t_ins_complete > 20)
		msleep(plug_det->t_ins_complete);
	else
		usleep_range(plug_det->t_ins_complete * 1000,
			     plug_det->t_ins_complete * 1000);

	if (tabla->mbhc_cfg.gpio) {
		/* Turn off the override */
		tabla_turn_onoff_override(codec, false);
		if (tabla_hs_gpio_level_remove(tabla))
			pr_debug("%s: GPIO value is low when determining "
				 "plug\n", __func__);
		else
			tabla_codec_decide_gpio_plug(codec);
		pr_debug("%s: leave\n", __func__);
		return;
	}

	plug_type = tabla_codec_get_plug_type(codec, false);
	tabla_turn_onoff_override(codec, false);

	if (plug_type == PLUG_TYPE_INVALID) {
		pr_debug("%s: Invalid plug type detected\n", __func__);
		snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_B1_CTL, 0x02, 0x02);
		tabla_codec_cleanup_hs_polling(codec);
		tabla_codec_enable_hs_detect(codec, 1,
					     MBHC_USE_MB_TRIGGER |
					     MBHC_USE_HPHL_TRIGGER, false);
	} else if (plug_type == PLUG_TYPE_GND_MIC_SWAP) {
		pr_debug("%s: GND-MIC swapped plug type detected\n", __func__);
		tabla_codec_report_plug(codec, 1, SND_JACK_UNSUPPORTED);
		tabla_codec_cleanup_hs_polling(codec);
		tabla_codec_enable_hs_detect(codec, 0, 0, false);
	} else if (plug_type == PLUG_TYPE_HEADPHONE) {
		pr_debug("%s: Headphone Detected\n", __func__);
		tabla_codec_report_plug(codec, 1, SND_JACK_HEADPHONE);
		tabla_codec_cleanup_hs_polling(codec);
		tabla_schedule_hs_detect_plug(tabla,
					&tabla->hs_correct_plug_work_nogpio);
	} else if (plug_type == PLUG_TYPE_HEADSET) {
		pr_debug("%s: Headset detected\n", __func__);
		tabla_codec_report_plug(codec, 1, SND_JACK_HEADSET);
		/* avoid false button press detect */
		msleep(50);
		tabla_codec_start_hs_polling(codec);
	} else if (tabla->mbhc_cfg.detect_extn_cable &&
		   plug_type == PLUG_TYPE_HIGH_HPH) {
		pr_debug("%s: High impedance plug type detected\n", __func__);
		tabla_codec_report_plug(codec, 1, SND_JACK_LINEOUT);
		/* Enable insertion detection on the other end of cable */
		tabla_codec_cleanup_hs_polling(codec);
		tabla_codec_enable_hs_detect(codec, 1,
					     MBHC_USE_MB_TRIGGER, false);
	}
	pr_debug("%s: leave\n", __func__);
}

/* called only from interrupt which is under codec_resource_lock acquisition */
static void tabla_hs_insert_irq_gpio(struct tabla_priv *priv, bool is_removal)
{
	struct snd_soc_codec *codec = priv->codec;
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);

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
			pr_debug("%s: Valid insertion, "
				 "detect plug type\n", __func__);
			tabla_codec_decide_gpio_plug(codec);
		} else {
			pr_debug("%s: Invalid insertion, "
				 "stop plug detection\n", __func__);
		}
	} else if (tabla->mbhc_cfg.detect_extn_cable) {
		pr_debug("%s: Removal\n", __func__);
		if (!tabla_hs_gpio_level_remove(tabla)) {
			/*
			 * gpio says, something is still inserted, could be
			 * extension cable i.e. headset is removed from
			 * extension cable
			 */
			/* cancel detect plug */
			tabla_cancel_hs_detect_plug(tabla,
						&tabla->hs_correct_plug_work);
			tabla_codec_decide_gpio_plug(codec);
		}
	} else {
		pr_err("%s: GPIO used, invalid MBHC Removal\n", __func__);
	}
}

/* called only from interrupt which is under codec_resource_lock acquisition */
static void tabla_hs_insert_irq_nogpio(struct tabla_priv *priv, bool is_removal,
				       bool is_mb_trigger)
{
	int ret;
	struct snd_soc_codec *codec = priv->codec;
	struct wcd9xxx *core = dev_get_drvdata(priv->codec->dev->parent);
	struct wcd9xxx_core_resource *core_res = &core->core_res;
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);

	/* Cancel possibly running hs_detect_work */
	tabla_cancel_hs_detect_plug(tabla,
			&tabla->hs_correct_plug_work_nogpio);

	if (is_removal) {

		/*
		 * If headphone is removed while playback is in progress,
		 * it is possible that micbias will be switched to VDDIO.
		 */
		tabla_codec_switch_micbias(codec, 0);
		if (priv->current_plug == PLUG_TYPE_HEADPHONE)
			tabla_codec_report_plug(codec, 0, SND_JACK_HEADPHONE);
		else if (priv->current_plug == PLUG_TYPE_GND_MIC_SWAP)
			tabla_codec_report_plug(codec, 0, SND_JACK_UNSUPPORTED);
		else
			WARN(1, "%s: Unexpected current plug type %d\n",
			     __func__, priv->current_plug);
		tabla_codec_shutdown_hs_removal_detect(codec);
		tabla_codec_enable_hs_detect(codec, 1,
					     MBHC_USE_MB_TRIGGER |
					     MBHC_USE_HPHL_TRIGGER,
					     true);
	} else if (is_mb_trigger && !is_removal) {
		pr_debug("%s: Waiting for Headphone left trigger\n",
			__func__);
		wcd9xxx_lock_sleep(core_res);
		if (schedule_delayed_work(&priv->mbhc_insert_dwork,
					  usecs_to_jiffies(1000000)) == 0) {
			pr_err("%s: mbhc_insert_dwork is already scheduled\n",
			       __func__);
			wcd9xxx_unlock_sleep(core_res);
		}
		tabla_codec_enable_hs_detect(codec, 1, MBHC_USE_HPHL_TRIGGER,
					     false);
	} else  {
		ret = cancel_delayed_work(&priv->mbhc_insert_dwork);
		if (ret != 0) {
			pr_debug("%s: Complete plug insertion, Detecting plug "
				 "type\n", __func__);
			tabla_codec_detect_plug_type(codec);
			wcd9xxx_unlock_sleep(core_res);
		} else {
			wcd9xxx_enable_irq(codec->control_data,
					   WCD9XXX_IRQ_MBHC_INSERTION);
			pr_err("%s: Error detecting plug insertion\n",
			       __func__);
		}
	}
}

/* called only from interrupt which is under codec_resource_lock acquisition */
static void tabla_hs_insert_irq_extn(struct tabla_priv *priv,
				     bool is_mb_trigger)
{
	struct snd_soc_codec *codec = priv->codec;
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);

	/* Cancel possibly running hs_detect_work */
	tabla_cancel_hs_detect_plug(tabla,
			&tabla->hs_correct_plug_work);

	if (is_mb_trigger) {
		pr_debug("%s: Waiting for Headphone left trigger\n",
			__func__);
		tabla_codec_enable_hs_detect(codec, 1, MBHC_USE_HPHL_TRIGGER,
					     false);
	} else  {
		pr_debug("%s: HPHL trigger received, detecting plug type\n",
			__func__);
		tabla_codec_detect_plug_type(codec);
	}
}

static irqreturn_t tabla_hs_insert_irq(int irq, void *data)
{
	bool is_mb_trigger, is_removal;
	struct tabla_priv *priv = data;
	struct snd_soc_codec *codec = priv->codec;

	pr_debug("%s: enter\n", __func__);
	TABLA_ACQUIRE_LOCK(priv->codec_resource_lock);
	wcd9xxx_disable_irq(codec->control_data, WCD9XXX_IRQ_MBHC_INSERTION);

	is_mb_trigger = !!(snd_soc_read(codec, priv->mbhc_bias_regs.mbhc_reg) &
					0x10);
	is_removal = !!(snd_soc_read(codec, TABLA_A_CDC_MBHC_INT_CTL) & 0x02);
	snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_INT_CTL, 0x03, 0x00);

	/* Turn off both HPH and MIC line schmitt triggers */
	snd_soc_update_bits(codec, priv->mbhc_bias_regs.mbhc_reg, 0x90, 0x00);
	snd_soc_update_bits(codec, TABLA_A_MBHC_HPH, 0x13, 0x00);
	snd_soc_update_bits(codec, priv->mbhc_bias_regs.ctl_reg, 0x01, 0x00);

	if (priv->mbhc_cfg.detect_extn_cable &&
	    priv->current_plug == PLUG_TYPE_HIGH_HPH)
		tabla_hs_insert_irq_extn(priv, is_mb_trigger);
	else if (priv->mbhc_cfg.gpio)
		tabla_hs_insert_irq_gpio(priv, is_removal);
	else
		tabla_hs_insert_irq_nogpio(priv, is_removal, is_mb_trigger);

	TABLA_RELEASE_LOCK(priv->codec_resource_lock);
	return IRQ_HANDLED;
}

static bool is_valid_mic_voltage(struct snd_soc_codec *codec, s32 mic_mv)
{
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);
	const struct tabla_mbhc_plug_type_cfg *plug_type =
	    TABLA_MBHC_CAL_PLUG_TYPE_PTR(tabla->mbhc_cfg.calibration);
	const s16 v_hs_max = tabla_get_current_v_hs_max(tabla);

	return (!(mic_mv > 10 && mic_mv < 80) && (mic_mv > plug_type->v_no_mic)
		&& (mic_mv < v_hs_max)) ? true : false;
}

/* called under codec_resource_lock acquisition
 * returns true if mic voltage range is back to normal insertion
 * returns false either if timedout or removed */
static bool tabla_hs_remove_settle(struct snd_soc_codec *codec)
{
	int i;
	bool timedout, settled = false;
	s32 mic_mv[MBHC_NUM_DCE_PLUG_DETECT];
	short mb_v[MBHC_NUM_DCE_PLUG_DETECT];
	unsigned long retry = 0, timeout;
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);
	const s16 v_hs_max = tabla_get_current_v_hs_max(tabla);

	timeout = jiffies + msecs_to_jiffies(TABLA_HS_DETECT_PLUG_TIME_MS);
	while (!(timedout = time_after(jiffies, timeout))) {
		retry++;
		if (tabla->mbhc_cfg.gpio && tabla_hs_gpio_level_remove(tabla)) {
			pr_debug("%s: GPIO indicates removal\n", __func__);
			break;
		}

		if (tabla->mbhc_cfg.gpio) {
			if (retry > 1)
				msleep(250);
			else
				msleep(50);
		}

		if (tabla->mbhc_cfg.gpio && tabla_hs_gpio_level_remove(tabla)) {
			pr_debug("%s: GPIO indicates removal\n", __func__);
			break;
		}

		for (i = 0; i < MBHC_NUM_DCE_PLUG_DETECT; i++) {
			mb_v[i] = tabla_codec_sta_dce(codec, 1,  true);
			mic_mv[i] = tabla_codec_sta_dce_v(codec, 1 , mb_v[i]);
			pr_debug("%s : DCE run %lu, mic_mv = %d(%x)\n",
				 __func__, retry, mic_mv[i], mb_v[i]);
		}

		if (tabla->mbhc_cfg.gpio && tabla_hs_gpio_level_remove(tabla)) {
			pr_debug("%s: GPIO indicates removal\n", __func__);
			break;
		}

		if (tabla->current_plug == PLUG_TYPE_NONE) {
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
		if (!tabla->mbhc_cfg.gpio) {
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
			 __func__, TABLA_HS_DETECT_PLUG_TIME_MS);
	return settled;
}

/* called only from interrupt which is under codec_resource_lock acquisition */
static void tabla_hs_remove_irq_gpio(struct tabla_priv *priv)
{
	struct snd_soc_codec *codec = priv->codec;
	pr_debug("%s: enter\n", __func__);
	if (tabla_hs_remove_settle(codec))
		tabla_codec_start_hs_polling(codec);
	pr_debug("%s: leave\n", __func__);
}

/* called only from interrupt which is under codec_resource_lock acquisition */
static void tabla_hs_remove_irq_nogpio(struct tabla_priv *priv)
{
	short bias_value;
	bool removed = true;
	struct snd_soc_codec *codec = priv->codec;
	const struct tabla_mbhc_general_cfg *generic =
	    TABLA_MBHC_CAL_GENERAL_PTR(priv->mbhc_cfg.calibration);
	int min_us = TABLA_FAKE_REMOVAL_MIN_PERIOD_MS * 1000;

	pr_debug("%s: enter\n", __func__);
	if (priv->current_plug != PLUG_TYPE_HEADSET) {
		pr_debug("%s(): Headset is not inserted, ignore removal\n",
			 __func__);
		snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_CLK_CTL,
				    0x08, 0x08);
		return;
	}

	usleep_range(generic->t_shutdown_plug_rem,
		     generic->t_shutdown_plug_rem);

	do {
		bias_value = tabla_codec_sta_dce(codec, 1,  true);
		pr_debug("%s: DCE %d,%d, %d us left\n", __func__, bias_value,
			 tabla_codec_sta_dce_v(codec, 1, bias_value), min_us);
		if (bias_value < tabla_get_current_v_ins(priv, false)) {
			pr_debug("%s: checking false removal\n", __func__);
			msleep(500);
			removed = !tabla_hs_remove_settle(codec);
			pr_debug("%s: headset %sactually removed\n", __func__,
				 removed ? "" : "not ");
			break;
		}
		min_us -= priv->mbhc_data.t_dce;
	} while (min_us > 0);

	if (removed) {
		if (priv->mbhc_cfg.detect_extn_cable) {
			if (!tabla_hs_gpio_level_remove(priv)) {
				/*
				 * extension cable is still plugged in
				 * report it as LINEOUT device
				 */
				tabla_codec_report_plug(codec, 1,
							SND_JACK_LINEOUT);
				tabla_codec_cleanup_hs_polling(codec);
				tabla_codec_enable_hs_detect(codec, 1,
							MBHC_USE_MB_TRIGGER,
							false);
			}
		} else {
			/* Cancel possibly running hs_detect_work */
			tabla_cancel_hs_detect_plug(priv,
					&priv->hs_correct_plug_work_nogpio);
			/*
			 * If this removal is not false, first check the micbias
			 * switch status and switch it to LDOH if it is already
			 * switched to VDDIO.
			 */
			tabla_codec_switch_micbias(codec, 0);

			tabla_codec_report_plug(codec, 0, SND_JACK_HEADSET);
			tabla_codec_cleanup_hs_polling(codec);
			tabla_codec_enable_hs_detect(codec, 1,
						     MBHC_USE_MB_TRIGGER |
						     MBHC_USE_HPHL_TRIGGER,
						     true);
		}
	} else {
		tabla_codec_start_hs_polling(codec);
	}
	pr_debug("%s: leave\n", __func__);
}

static irqreturn_t tabla_hs_remove_irq(int irq, void *data)
{
	struct tabla_priv *priv = data;
	bool vddio;
	pr_debug("%s: enter, removal interrupt\n", __func__);

	TABLA_ACQUIRE_LOCK(priv->codec_resource_lock);
	vddio = !priv->mbhc_cfg.micbias_always_on &&
		(priv->mbhc_data.micb_mv != VDDIO_MICBIAS_MV &&
		 priv->mbhc_micbias_switched);
	if (vddio)
		__tabla_codec_switch_micbias(priv->codec, 0, false, true);

	if ((priv->mbhc_cfg.detect_extn_cable &&
	     !tabla_hs_gpio_level_remove(priv)) ||
	    !priv->mbhc_cfg.gpio) {
		tabla_hs_remove_irq_nogpio(priv);
	} else
		tabla_hs_remove_irq_gpio(priv);

	/* if driver turned off vddio switch and headset is not removed,
	 * turn on the vddio switch back, if headset is removed then vddio
	 * switch is off by time now and shouldn't be turn on again from here */
	if (vddio && priv->current_plug == PLUG_TYPE_HEADSET)
		__tabla_codec_switch_micbias(priv->codec, 1, true, true);
	TABLA_RELEASE_LOCK(priv->codec_resource_lock);

	return IRQ_HANDLED;
}

void mbhc_insert_work(struct work_struct *work)
{
	struct delayed_work *dwork;
	struct tabla_priv *tabla;
	struct snd_soc_codec *codec;
	struct wcd9xxx *tabla_core;
	struct wcd9xxx_core_resource *core_res;

	dwork = to_delayed_work(work);
	tabla = container_of(dwork, struct tabla_priv, mbhc_insert_dwork);
	codec = tabla->codec;
	tabla_core = dev_get_drvdata(codec->dev->parent);
	core_res = &tabla_core->core_res;

	pr_debug("%s:\n", __func__);

	/* Turn off both HPH and MIC line schmitt triggers */
	snd_soc_update_bits(codec, tabla->mbhc_bias_regs.mbhc_reg, 0x90, 0x00);
	snd_soc_update_bits(codec, TABLA_A_MBHC_HPH, 0x13, 0x00);
	snd_soc_update_bits(codec, tabla->mbhc_bias_regs.ctl_reg, 0x01, 0x00);
	wcd9xxx_disable_irq_sync(codec->control_data,
				 WCD9XXX_IRQ_MBHC_INSERTION);
	tabla_codec_detect_plug_type(codec);
	wcd9xxx_unlock_sleep(core_res);
}

static void tabla_hs_gpio_handler(struct snd_soc_codec *codec)
{
	bool insert;
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);
	struct wcd9xxx *core = dev_get_drvdata(codec->dev->parent);
	bool is_removed = false;

	pr_debug("%s: enter\n", __func__);

	tabla->in_gpio_handler = true;
	/* Wait here for debounce time */
	usleep_range(TABLA_GPIO_IRQ_DEBOUNCE_TIME_US,
		     TABLA_GPIO_IRQ_DEBOUNCE_TIME_US);

	wcd9xxx_nested_irq_lock(&core->core_res);
	TABLA_ACQUIRE_LOCK(tabla->codec_resource_lock);

	/* cancel pending button press */
	if (tabla_cancel_btn_work(tabla))
		pr_debug("%s: button press is canceled\n", __func__);

	insert = (gpio_get_value_cansleep(tabla->mbhc_cfg.gpio) ==
		  tabla->mbhc_cfg.gpio_level_insert);
	if ((tabla->current_plug == PLUG_TYPE_NONE) && insert) {
		tabla->lpi_enabled = false;
		wmb();

		/* cancel detect plug */
		tabla_cancel_hs_detect_plug(tabla,
					&tabla->hs_correct_plug_work);

		/* Disable Mic Bias pull down and HPH Switch to GND */
		snd_soc_update_bits(codec, tabla->mbhc_bias_regs.ctl_reg, 0x01,
				    0x00);
		snd_soc_update_bits(codec, TABLA_A_MBHC_HPH, 0x01, 0x00);
		tabla_codec_detect_plug_type(codec);
	} else if ((tabla->current_plug != PLUG_TYPE_NONE) && !insert) {
		tabla->lpi_enabled = false;
		wmb();

		/* cancel detect plug */
		tabla_cancel_hs_detect_plug(tabla,
					&tabla->hs_correct_plug_work);

		if (tabla->current_plug == PLUG_TYPE_HEADPHONE) {
			tabla_codec_report_plug(codec, 0, SND_JACK_HEADPHONE);
			is_removed = true;
		} else if (tabla->current_plug == PLUG_TYPE_GND_MIC_SWAP) {
			tabla_codec_report_plug(codec, 0, SND_JACK_UNSUPPORTED);
			is_removed = true;
		} else if (tabla->current_plug == PLUG_TYPE_HEADSET) {
			tabla_codec_pause_hs_polling(codec);
			tabla_codec_cleanup_hs_polling(codec);
			tabla_codec_report_plug(codec, 0, SND_JACK_HEADSET);
			is_removed = true;
		} else if (tabla->current_plug == PLUG_TYPE_HIGH_HPH) {
			tabla_codec_report_plug(codec, 0, SND_JACK_LINEOUT);
			is_removed = true;
		}

		if (is_removed) {
			/* Enable Mic Bias pull down and HPH Switch to GND */
			snd_soc_update_bits(codec,
					    tabla->mbhc_bias_regs.ctl_reg, 0x01,
					    0x01);
			snd_soc_update_bits(codec, TABLA_A_MBHC_HPH, 0x01,
					    0x01);
			/* Make sure mic trigger is turned off */
			snd_soc_update_bits(codec,
					    tabla->mbhc_bias_regs.ctl_reg,
					    0x01, 0x01);
			snd_soc_update_bits(codec,
					    tabla->mbhc_bias_regs.mbhc_reg,
					    0x90, 0x00);
			/* Reset MBHC State Machine */
			snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_CLK_CTL,
					    0x08, 0x08);
			snd_soc_update_bits(codec, TABLA_A_CDC_MBHC_CLK_CTL,
					    0x08, 0x00);
			/* Turn off override */
			tabla_turn_onoff_override(codec, false);
			tabla_codec_switch_micbias(codec, 0);
		}
	}

	tabla->in_gpio_handler = false;
	TABLA_RELEASE_LOCK(tabla->codec_resource_lock);
	wcd9xxx_nested_irq_unlock(&core->core_res);
	pr_debug("%s: leave\n", __func__);
}

static irqreturn_t tabla_mechanical_plug_detect_irq(int irq, void *data)
{
	int r = IRQ_HANDLED;
	struct snd_soc_codec *codec = data;
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);
	struct wcd9xxx *core = codec->control_data;
	struct wcd9xxx_core_resource *core_res = &core->core_res;

	if (unlikely(wcd9xxx_lock_sleep(core_res) == false)) {
		pr_warn("%s: failed to hold suspend\n", __func__);
		/*
		 * Give up this IRQ for now and resend this IRQ so IRQ can be
		 * handled after system resume
		 */
		TABLA_ACQUIRE_LOCK(tabla->codec_resource_lock);
		tabla->gpio_irq_resend = true;
		TABLA_RELEASE_LOCK(tabla->codec_resource_lock);
		wake_lock_timeout(&tabla->irq_resend_wlock, HZ);
		r = IRQ_NONE;
	} else {
		tabla_hs_gpio_handler(codec);
		wcd9xxx_unlock_sleep(core_res);
	}

	return r;
}

static void tabla_hs_correct_plug_nogpio(struct work_struct *work)
{
	struct tabla_priv *tabla;
	struct snd_soc_codec *codec;
	unsigned long timeout;
	int retry = 0;
	enum tabla_mbhc_plug_type plug_type;
	bool is_headset = false;
	struct wcd9xxx *core;
	struct wcd9xxx_core_resource *core_res;

	pr_debug("%s(): Poll Microphone voltage for %d seconds\n",
			 __func__, TABLA_HS_DETECT_PLUG_TIME_MS / 1000);

	tabla = container_of(work, struct tabla_priv,
						 hs_correct_plug_work_nogpio);
	codec = tabla->codec;
	core = codec->control_data;
	core_res = &core->core_res;

	/* Make sure the MBHC mux is connected to MIC Path */
	snd_soc_write(codec, TABLA_A_MBHC_SCALING_MUX_1, 0x84);

	/* setup for microphone polling */
	tabla_turn_onoff_override(codec, true);
	tabla->mbhc_cfg.mclk_cb_fn(codec, 1, false);

	timeout = jiffies + msecs_to_jiffies(TABLA_HS_DETECT_PLUG_TIME_MS);
	while (!time_after(jiffies, timeout)) {
		++retry;

		msleep(TABLA_HS_DETECT_PLUG_INERVAL_MS);
		TABLA_ACQUIRE_LOCK(tabla->codec_resource_lock);
		plug_type = tabla_codec_get_plug_type(codec, false);
		TABLA_RELEASE_LOCK(tabla->codec_resource_lock);

		if (plug_type == PLUG_TYPE_HIGH_HPH
			|| plug_type == PLUG_TYPE_INVALID) {

			/* this means the plug is removed
			 * End microphone polling and setup
			 * for low power removal detection.
			 */
			pr_debug("%s(): Plug may be removed, setup removal\n",
					 __func__);
			break;
		} else if (plug_type == PLUG_TYPE_HEADSET) {
			/* Plug is corrected from headphone to headset,
			 * report headset and end the polling
			 */
			is_headset = true;
			TABLA_ACQUIRE_LOCK(tabla->codec_resource_lock);
			tabla_turn_onoff_override(codec, false);
			tabla_codec_report_plug(codec, 1, SND_JACK_HEADSET);
			tabla_codec_start_hs_polling(codec);
			TABLA_RELEASE_LOCK(tabla->codec_resource_lock);
			pr_debug("%s(): corrected from headphone to headset\n",
					 __func__);
			break;
		}
	}

	/* Undo setup for microphone polling depending
	 * result from polling
	 */
	tabla->mbhc_cfg.mclk_cb_fn(codec, 0, false);
	if (!is_headset) {
		pr_debug("%s: Inserted headphone is not a headset\n",
			__func__);
		tabla_turn_onoff_override(codec, false);
		tabla_codec_cleanup_hs_polling(codec);
		tabla_codec_enable_hs_detect(codec, 0, 0, false);
	}
	wcd9xxx_unlock_sleep(core_res);
}

static int tabla_mbhc_init_and_calibrate(struct tabla_priv *tabla)
{
	int ret = 0;
	struct snd_soc_codec *codec = tabla->codec;

	tabla->mbhc_cfg.mclk_cb_fn(codec, 1, false);
	tabla_mbhc_init(codec);
	tabla_mbhc_cal(codec);
	tabla_mbhc_calc_thres(codec);
	tabla->mbhc_cfg.mclk_cb_fn(codec, 0, false);
	tabla_codec_calibrate_hs_polling(codec);
	if (!tabla->mbhc_cfg.gpio) {
		INIT_WORK(&tabla->hs_correct_plug_work_nogpio,
				  tabla_hs_correct_plug_nogpio);
		ret = tabla_codec_enable_hs_detect(codec, 1,
						   MBHC_USE_MB_TRIGGER |
						   MBHC_USE_HPHL_TRIGGER,
						   false);

		if (IS_ERR_VALUE(ret))
			pr_err("%s: Failed to setup MBHC detection\n",
			       __func__);
	} else {
		/* Enable Mic Bias pull down and HPH Switch to GND */
		snd_soc_update_bits(codec, tabla->mbhc_bias_regs.ctl_reg,
				    0x01, 0x01);
		snd_soc_update_bits(codec, TABLA_A_MBHC_HPH, 0x01, 0x01);
		INIT_WORK(&tabla->hs_correct_plug_work,
			  tabla_hs_correct_gpio_plug);
	}

	if (!IS_ERR_VALUE(ret)) {
		snd_soc_update_bits(codec, TABLA_A_RX_HPH_OCP_CTL, 0x10, 0x10);
		wcd9xxx_enable_irq(codec->control_data,
				 WCD9XXX_IRQ_HPH_PA_OCPL_FAULT);
		wcd9xxx_enable_irq(codec->control_data,
				 WCD9XXX_IRQ_HPH_PA_OCPR_FAULT);

		if (tabla->mbhc_cfg.gpio) {
			ret = request_threaded_irq(tabla->mbhc_cfg.gpio_irq,
					       NULL,
					       tabla_mechanical_plug_detect_irq,
					       (IRQF_TRIGGER_RISING |
						IRQF_TRIGGER_FALLING),
					       "tabla-gpio", codec);
			if (!IS_ERR_VALUE(ret)) {
				ret = enable_irq_wake(tabla->mbhc_cfg.gpio_irq);
				/* Bootup time detection */
				tabla_hs_gpio_handler(codec);
			}
		}
	}

	return ret;
}

static void mbhc_fw_read(struct work_struct *work)
{
	struct delayed_work *dwork;
	struct tabla_priv *tabla;
	struct snd_soc_codec *codec;
	const struct firmware *fw;
	int ret = -1, retry = 0;

	dwork = to_delayed_work(work);
	tabla = container_of(dwork, struct tabla_priv, mbhc_firmware_dwork);
	codec = tabla->codec;

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
	} else if (tabla_mbhc_fw_validate(fw) == false) {
		pr_err("%s: Invalid MBHC cal data size use default cal\n",
			 __func__);
		release_firmware(fw);
	} else {
		tabla->mbhc_cfg.calibration = (void *)fw->data;
		tabla->mbhc_fw = fw;
	}

	(void) tabla_mbhc_init_and_calibrate(tabla);
}

int tabla_hs_detect(struct snd_soc_codec *codec,
		    const struct tabla_mbhc_config *cfg)
{
	struct tabla_priv *tabla;
	int rc = 0;

	if (!codec || !cfg->calibration) {
		pr_err("Error: no codec or calibration\n");
		return -EINVAL;
	}

	if (cfg->mclk_rate != TABLA_MCLK_RATE_12288KHZ) {
		if (cfg->mclk_rate == TABLA_MCLK_RATE_9600KHZ)
			pr_err("Error: clock rate %dHz is not yet supported\n",
			       cfg->mclk_rate);
		else
			pr_err("Error: unsupported clock rate %d\n",
			       cfg->mclk_rate);
		return -EINVAL;
	}

	tabla = snd_soc_codec_get_drvdata(codec);
	tabla->mbhc_cfg = *cfg;
	tabla->in_gpio_handler = false;
	tabla->current_plug = PLUG_TYPE_NONE;
	tabla->lpi_enabled = false;
	tabla_get_mbhc_micbias_regs(codec, &tabla->mbhc_bias_regs);

	/* Put CFILT in fast mode by default */
	if (!tabla->mbhc_cfg.micbias_always_on)
		snd_soc_update_bits(codec, tabla->mbhc_bias_regs.cfilt_ctl,
			    0x40, TABLA_CFILT_FAST_MODE);
	INIT_DELAYED_WORK(&tabla->mbhc_firmware_dwork, mbhc_fw_read);
	INIT_DELAYED_WORK(&tabla->mbhc_btn_dwork, btn_lpress_fn);
	INIT_WORK(&tabla->hphlocp_work, hphlocp_off_report);
	INIT_WORK(&tabla->hphrocp_work, hphrocp_off_report);
	INIT_DELAYED_WORK(&tabla->mbhc_insert_dwork, mbhc_insert_work);

	if (!tabla->mbhc_cfg.read_fw_bin)
		rc = tabla_mbhc_init_and_calibrate(tabla);
	else
		schedule_delayed_work(&tabla->mbhc_firmware_dwork,
				      usecs_to_jiffies(MBHC_FW_READ_TIMEOUT));

	return rc;
}
EXPORT_SYMBOL_GPL(tabla_hs_detect);

static irqreturn_t tabla_slimbus_irq(int irq, void *data)
{
	struct tabla_priv *priv = data;
	struct snd_soc_codec *codec = priv->codec;
	struct tabla_priv *tabla_p = snd_soc_codec_get_drvdata(codec);
	int i, j, port_id, k, ch_mask_temp;
	unsigned long slimbus_value;
	u8 val;
	for (i = 0; i < WCD9XXX_SLIM_NUM_PORT_REG; i++) {
		slimbus_value = wcd9xxx_interface_reg_read(codec->control_data,
			TABLA_SLIM_PGD_PORT_INT_STATUS0 + i);
		for_each_set_bit(j, &slimbus_value, BITS_PER_BYTE) {
			val = wcd9xxx_interface_reg_read(codec->control_data,
				TABLA_SLIM_PGD_PORT_INT_SOURCE0 + i*8 + j);
			if (val & 0x1)
				pr_err_ratelimited("overflow error on port %x,"
					" value %x\n", i*8 + j, val);
			if (val & 0x2)
				pr_err_ratelimited("underflow error on port %x,"
					" value %x\n", i*8 + j, val);
			if (val & 0x4) {
				port_id = i*8 + j;
				for (k = 0; k < ARRAY_SIZE(tabla_dai); k++) {
					ch_mask_temp = 1 << port_id;
					pr_debug("%s: tabla_p->dai[%d].ch_mask = 0x%lx\n",
						 __func__, k,
						 tabla_p->dai[k].ch_mask);
					if (ch_mask_temp &
						tabla_p->dai[k].ch_mask) {
						tabla_p->dai[k].ch_mask &=
							~ch_mask_temp;
					if (!tabla_p->dai[k].ch_mask)
						wake_up(
						&tabla_p->dai[k].dai_wait);
					}
				}
			}
		}
		wcd9xxx_interface_reg_write(codec->control_data,
			TABLA_SLIM_PGD_PORT_INT_CLR0 + i, slimbus_value);
		val = 0x0;
	}

	return IRQ_HANDLED;
}

static int tabla_handle_pdata(struct tabla_priv *tabla)
{
	struct snd_soc_codec *codec = tabla->codec;
	struct wcd9xxx_pdata *pdata = tabla->pdata;
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
	snd_soc_update_bits(codec, tabla->reg_addr.micb_4_ctl, 0x60,
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

	for (i = 0; i < ARRAY_SIZE(pdata->regulator); i++) {
		if (!strncmp(pdata->regulator[i].name, "CDC_VDDA_RX", 11)) {
			if (pdata->regulator[i].min_uV == 1800000 &&
			    pdata->regulator[i].max_uV == 1800000) {
				snd_soc_write(codec, TABLA_A_BIAS_REF_CTL,
					      0x1C);
			} else if (pdata->regulator[i].min_uV == 2200000 &&
				   pdata->regulator[i].max_uV == 2200000) {
				snd_soc_write(codec, TABLA_A_BIAS_REF_CTL,
					      0x1E);
			} else {
				pr_err("%s: unsupported CDC_VDDA_RX voltage "
				       "min %d, max %d\n", __func__,
				       pdata->regulator[i].min_uV,
				       pdata->regulator[i].max_uV);
				rc = -EINVAL;
			}
			break;
		}
	}
done:
	return rc;
}

static const struct tabla_reg_mask_val tabla_1_1_reg_defaults[] = {

	/* Tabla 1.1 MICBIAS changes */
	TABLA_REG_VAL(TABLA_A_MICB_1_INT_RBIAS, 0x24),
	TABLA_REG_VAL(TABLA_A_MICB_2_INT_RBIAS, 0x24),
	TABLA_REG_VAL(TABLA_A_MICB_3_INT_RBIAS, 0x24),

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

static const struct tabla_reg_mask_val tabla_1_x_only_reg_2_0_defaults[] = {
	TABLA_REG_VAL(TABLA_1_A_MICB_4_INT_RBIAS, 0x24),
};

static const struct tabla_reg_mask_val tabla_2_only_reg_2_0_defaults[] = {
	TABLA_REG_VAL(TABLA_2_A_MICB_4_INT_RBIAS, 0x24),
};

static void tabla_update_reg_defaults(struct snd_soc_codec *codec)
{
	u32 i;
	struct wcd9xxx *tabla_core = dev_get_drvdata(codec->dev->parent);

	for (i = 0; i < ARRAY_SIZE(tabla_1_1_reg_defaults); i++)
		snd_soc_write(codec, tabla_1_1_reg_defaults[i].reg,
				tabla_1_1_reg_defaults[i].val);

	for (i = 0; i < ARRAY_SIZE(tabla_2_0_reg_defaults); i++)
		snd_soc_write(codec, tabla_2_0_reg_defaults[i].reg,
				tabla_2_0_reg_defaults[i].val);

	if (TABLA_IS_1_X(tabla_core->version)) {
		for (i = 0; i < ARRAY_SIZE(tabla_1_x_only_reg_2_0_defaults);
		     i++)
			snd_soc_write(codec,
				      tabla_1_x_only_reg_2_0_defaults[i].reg,
				      tabla_1_x_only_reg_2_0_defaults[i].val);
	} else {
		for (i = 0; i < ARRAY_SIZE(tabla_2_only_reg_2_0_defaults); i++)
			snd_soc_write(codec,
				      tabla_2_only_reg_2_0_defaults[i].reg,
				      tabla_2_only_reg_2_0_defaults[i].val);
	}
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

	/* Set the MICBIAS default output as pull down*/
	{TABLA_A_MICB_1_CTL, 0x01, 0x01},
	{TABLA_A_MICB_2_CTL, 0x01, 0x01},
	{TABLA_A_MICB_3_CTL, 0x01, 0x01},

	/* Initialize mic biases to differential mode */
	{TABLA_A_MICB_1_INT_RBIAS, 0x24, 0x24},
	{TABLA_A_MICB_2_INT_RBIAS, 0x24, 0x24},
	{TABLA_A_MICB_3_INT_RBIAS, 0x24, 0x24},

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

	/* config Decimator for DMIC CLK_MODE_1(3.072Mhz@12.88Mhz mclk) */
	{TABLA_A_CDC_TX1_DMIC_CTL, 0x1, 0x1},
	{TABLA_A_CDC_TX2_DMIC_CTL, 0x1, 0x1},
	{TABLA_A_CDC_TX3_DMIC_CTL, 0x1, 0x1},
	{TABLA_A_CDC_TX4_DMIC_CTL, 0x1, 0x1},
	{TABLA_A_CDC_TX5_DMIC_CTL, 0x1, 0x1},
	{TABLA_A_CDC_TX6_DMIC_CTL, 0x1, 0x1},
	{TABLA_A_CDC_TX7_DMIC_CTL, 0x1, 0x1},
	{TABLA_A_CDC_TX8_DMIC_CTL, 0x1, 0x1},
	{TABLA_A_CDC_TX9_DMIC_CTL, 0x1, 0x1},
	{TABLA_A_CDC_TX10_DMIC_CTL, 0x1, 0x1},

	/* config DMIC clk to CLK_MODE_1 (3.072Mhz@12.88Mhz mclk) */
	{TABLA_A_CDC_CLK_DMIC_CTL, 0x2A, 0x2A},

};

static const struct tabla_reg_mask_val tabla_1_x_codec_reg_init_val[] = {
	/* Set the MICBIAS default output as pull down*/
	{TABLA_1_A_MICB_4_CTL, 0x01, 0x01},
	/* Initialize mic biases to differential mode */
	{TABLA_1_A_MICB_4_INT_RBIAS, 0x24, 0x24},
};

static const struct tabla_reg_mask_val tabla_2_higher_codec_reg_init_val[] = {

	/* Set the MICBIAS default output as pull down*/
	{TABLA_2_A_MICB_4_CTL, 0x01, 0x01},
	/* Initialize mic biases to differential mode */
	{TABLA_2_A_MICB_4_INT_RBIAS, 0x24, 0x24},
};

static void tabla_codec_init_reg(struct snd_soc_codec *codec)
{
	u32 i;
	struct wcd9xxx *tabla_core = dev_get_drvdata(codec->dev->parent);

	for (i = 0; i < ARRAY_SIZE(tabla_codec_reg_init_val); i++)
		snd_soc_update_bits(codec, tabla_codec_reg_init_val[i].reg,
				tabla_codec_reg_init_val[i].mask,
				tabla_codec_reg_init_val[i].val);
	if (TABLA_IS_1_X(tabla_core->version)) {
		for (i = 0; i < ARRAY_SIZE(tabla_1_x_codec_reg_init_val); i++)
			snd_soc_update_bits(codec,
					   tabla_1_x_codec_reg_init_val[i].reg,
					   tabla_1_x_codec_reg_init_val[i].mask,
					   tabla_1_x_codec_reg_init_val[i].val);
	} else {
		for (i = 0; i < ARRAY_SIZE(tabla_2_higher_codec_reg_init_val);
		     i++)
			snd_soc_update_bits(codec,
				      tabla_2_higher_codec_reg_init_val[i].reg,
				      tabla_2_higher_codec_reg_init_val[i].mask,
				      tabla_2_higher_codec_reg_init_val[i].val);
	}
	snd_soc_update_bits(codec, TABLA_A_CDC_DMIC_CLK0_MODE, 0x7, 0x4);
	snd_soc_update_bits(codec, TABLA_A_CDC_DMIC_CLK1_MODE, 0x7, 0x4);
	snd_soc_update_bits(codec, TABLA_A_CDC_DMIC_CLK2_MODE, 0x7, 0x4);
	snd_soc_update_bits(codec, TABLA_A_PIN_CTL_OE0, 0x90, 0x90);
	snd_soc_update_bits(codec, TABLA_A_PIN_CTL_OE1, 0x8, 0x8);
	snd_soc_update_bits(codec, TABLA_A_PIN_CTL_DATA0, 0x90, 0x0);
	snd_soc_update_bits(codec, TABLA_A_PIN_CTL_DATA1, 0x8, 0x0);
}

static void tabla_update_reg_address(struct tabla_priv *priv)
{
	struct wcd9xxx *tabla_core = dev_get_drvdata(priv->codec->dev->parent);
	struct tabla_reg_address *reg_addr = &priv->reg_addr;

	if (TABLA_IS_1_X(tabla_core->version)) {
		reg_addr->micb_4_mbhc = TABLA_1_A_MICB_4_MBHC;
		reg_addr->micb_4_int_rbias = TABLA_1_A_MICB_4_INT_RBIAS;
		reg_addr->micb_4_ctl = TABLA_1_A_MICB_4_CTL;
	} else if (TABLA_IS_2_0(tabla_core->version)) {
		reg_addr->micb_4_mbhc = TABLA_2_A_MICB_4_MBHC;
		reg_addr->micb_4_int_rbias = TABLA_2_A_MICB_4_INT_RBIAS;
		reg_addr->micb_4_ctl = TABLA_2_A_MICB_4_CTL;
	}
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
	struct tabla_priv *tabla = filp->private_data;

	if (cnt > sizeof(lbuf) - 1)
		return -EINVAL;

	rc = copy_from_user(lbuf, ubuf, cnt);
	if (rc)
		return -EFAULT;

	lbuf[cnt] = '\0';
	buf = (char *)lbuf;
	TABLA_ACQUIRE_LOCK(tabla->codec_resource_lock);
	tabla->no_mic_headset_override =
	    (*strsep(&buf, " ") == '0') ? false : true;
	if (tabla->no_mic_headset_override && tabla->mbhc_polling_active) {
		tabla_codec_pause_hs_polling(tabla->codec);
		tabla_codec_start_hs_polling(tabla->codec);
	}
	TABLA_RELEASE_LOCK(tabla->codec_resource_lock);
	return cnt;
}

static ssize_t codec_mbhc_debug_read(struct file *file, char __user *buf,
				     size_t count, loff_t *pos)
{
	const int size = 768;
	char buffer[size];
	int n = 0;
	struct tabla_priv *tabla = file->private_data;
	struct snd_soc_codec *codec = tabla->codec;
	const struct mbhc_internal_cal_data *p = &tabla->mbhc_data;
	const s16 v_ins_hu_cur = tabla_get_current_v_ins(tabla, true);
	const s16 v_ins_h_cur = tabla_get_current_v_ins(tabla, false);

	n = scnprintf(buffer, size - n, "dce_z = %x(%dmv)\n",  p->dce_z,
		     tabla_codec_sta_dce_v(codec, 1, p->dce_z));
	n += scnprintf(buffer + n, size - n, "dce_mb = %x(%dmv)\n",
		       p->dce_mb, tabla_codec_sta_dce_v(codec, 1, p->dce_mb));
	n += scnprintf(buffer + n, size - n, "sta_z = %x(%dmv)\n",
		       p->sta_z, tabla_codec_sta_dce_v(codec, 0, p->sta_z));
	n += scnprintf(buffer + n, size - n, "sta_mb = %x(%dmv)\n",
		       p->sta_mb, tabla_codec_sta_dce_v(codec, 0, p->sta_mb));
	n += scnprintf(buffer + n, size - n, "t_dce = %x\n",  p->t_dce);
	n += scnprintf(buffer + n, size - n, "t_sta = %x\n",  p->t_sta);
	n += scnprintf(buffer + n, size - n, "micb_mv = %dmv\n",
		       p->micb_mv);
	n += scnprintf(buffer + n, size - n, "v_ins_hu = %x(%dmv)%s\n",
		       p->v_ins_hu,
		       tabla_codec_sta_dce_v(codec, 0, p->v_ins_hu),
		       p->v_ins_hu == v_ins_hu_cur ? "*" : "");
	n += scnprintf(buffer + n, size - n, "v_ins_h = %x(%dmv)%s\n",
		       p->v_ins_h, tabla_codec_sta_dce_v(codec, 1, p->v_ins_h),
		       p->v_ins_h == v_ins_h_cur ? "*" : "");
	n += scnprintf(buffer + n, size - n, "adj_v_ins_hu = %x(%dmv)%s\n",
		       p->adj_v_ins_hu,
		       tabla_codec_sta_dce_v(codec, 0, p->adj_v_ins_hu),
		       p->adj_v_ins_hu == v_ins_hu_cur ? "*" : "");
	n += scnprintf(buffer + n, size - n, "adj_v_ins_h = %x(%dmv)%s\n",
		       p->adj_v_ins_h,
		       tabla_codec_sta_dce_v(codec, 1, p->adj_v_ins_h),
		       p->adj_v_ins_h == v_ins_h_cur ? "*" : "");
	n += scnprintf(buffer + n, size - n, "v_b1_hu = %x(%dmv)\n",
		       p->v_b1_hu, tabla_codec_sta_dce_v(codec, 0, p->v_b1_hu));
	n += scnprintf(buffer + n, size - n, "v_b1_h = %x(%dmv)\n",
		       p->v_b1_h, tabla_codec_sta_dce_v(codec, 1, p->v_b1_h));
	n += scnprintf(buffer + n, size - n, "v_b1_huc = %x(%dmv)\n",
		       p->v_b1_huc,
		       tabla_codec_sta_dce_v(codec, 1, p->v_b1_huc));
	n += scnprintf(buffer + n, size - n, "v_brh = %x(%dmv)\n",
		       p->v_brh, tabla_codec_sta_dce_v(codec, 1, p->v_brh));
	n += scnprintf(buffer + n, size - n, "v_brl = %x(%dmv)\n",  p->v_brl,
		       tabla_codec_sta_dce_v(codec, 0, p->v_brl));
	n += scnprintf(buffer + n, size - n, "v_no_mic = %x(%dmv)\n",
		       p->v_no_mic,
		       tabla_codec_sta_dce_v(codec, 0, p->v_no_mic));
	n += scnprintf(buffer + n, size - n, "npoll = %d\n",  p->npoll);
	n += scnprintf(buffer + n, size - n, "nbounce_wait = %d\n",
		       p->nbounce_wait);
	n += scnprintf(buffer + n, size - n, "v_inval_ins_low = %d\n",
		       p->v_inval_ins_low);
	n += scnprintf(buffer + n, size - n, "v_inval_ins_high = %d\n",
		       p->v_inval_ins_high);
	if (tabla->mbhc_cfg.gpio)
		n += scnprintf(buffer + n, size - n, "GPIO insert = %d\n",
			       tabla_hs_gpio_level_remove(tabla));
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

static int tabla_codec_probe(struct snd_soc_codec *codec)
{
	struct wcd9xxx *control;
	struct tabla_priv *tabla;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	int ret = 0;
	int i;
	void *ptr = NULL;
	struct wcd9xxx_core_resource *core_res;

	codec->control_data = dev_get_drvdata(codec->dev->parent);
	control = codec->control_data;
	core_res = &control->core_res;

	tabla = kzalloc(sizeof(struct tabla_priv), GFP_KERNEL);
	if (!tabla) {
		dev_err(codec->dev, "Failed to allocate private data\n");
		return -ENOMEM;
	}
	for (i = 0 ; i < NUM_DECIMATORS; i++) {
		tx_hpf_work[i].tabla = tabla;
		tx_hpf_work[i].decimator = i + 1;
		INIT_DELAYED_WORK(&tx_hpf_work[i].dwork,
			tx_hpf_corner_freq_callback);
	}

	/* Make sure mbhc micbias register addresses are zeroed out */
	memset(&tabla->mbhc_bias_regs, 0,
		sizeof(struct mbhc_micbias_regs));
	tabla->mbhc_micbias_switched = false;

	/* Make sure mbhc intenal calibration data is zeroed out */
	memset(&tabla->mbhc_data, 0,
		sizeof(struct mbhc_internal_cal_data));
	tabla->mbhc_data.t_sta_dce = DEFAULT_DCE_STA_WAIT;
	tabla->mbhc_data.t_dce = DEFAULT_DCE_WAIT;
	tabla->mbhc_data.t_sta = DEFAULT_STA_WAIT;
	snd_soc_codec_set_drvdata(codec, tabla);

	tabla->mclk_enabled = false;
	tabla->bandgap_type = TABLA_BANDGAP_OFF;
	tabla->clock_active = false;
	tabla->config_mode_active = false;
	tabla->mbhc_polling_active = false;
	tabla->mbhc_fake_ins_start = 0;
	tabla->no_mic_headset_override = false;
	tabla->hs_polling_irq_prepared = false;
	mutex_init(&tabla->codec_resource_lock);
	tabla->codec = codec;
	tabla->mbhc_state = MBHC_STATE_NONE;
	tabla->mbhc_last_resume = 0;
	for (i = 0; i < COMPANDER_MAX; i++) {
		tabla->comp_enabled[i] = 0;
		tabla->comp_fs[i] = COMPANDER_FS_48KHZ;
	}
	tabla->pdata = dev_get_platdata(codec->dev->parent);
	tabla->intf_type = wcd9xxx_get_intf_type();
	tabla->aux_pga_cnt = 0;
	tabla->aux_l_gain = 0x1F;
	tabla->aux_r_gain = 0x1F;
	tabla_update_reg_address(tabla);
	tabla_update_reg_defaults(codec);
	tabla_codec_init_reg(codec);
	ret = tabla_handle_pdata(tabla);
	if (IS_ERR_VALUE(ret)) {
		pr_err("%s: bad pdata\n", __func__);
		goto err_pdata;
	}

	if (TABLA_IS_1_X(control->version))
		snd_soc_add_codec_controls(codec, tabla_1_x_snd_controls,
				     ARRAY_SIZE(tabla_1_x_snd_controls));
	else
		snd_soc_add_codec_controls(codec, tabla_2_higher_snd_controls,
				     ARRAY_SIZE(tabla_2_higher_snd_controls));

	if (TABLA_IS_1_X(control->version))
		snd_soc_dapm_new_controls(dapm, tabla_1_x_dapm_widgets,
					  ARRAY_SIZE(tabla_1_x_dapm_widgets));
	else
		snd_soc_dapm_new_controls(dapm, tabla_2_higher_dapm_widgets,
				    ARRAY_SIZE(tabla_2_higher_dapm_widgets));

	ptr = kmalloc((sizeof(tabla_rx_chs) +
		       sizeof(tabla_tx_chs)), GFP_KERNEL);
	if (!ptr) {
		pr_err("%s: no mem for slim chan ctl data\n", __func__);
		ret = -ENOMEM;
		goto err_nomem_slimch;
	}
	if (tabla->intf_type == WCD9XXX_INTERFACE_TYPE_I2C) {
		snd_soc_dapm_new_controls(dapm, tabla_dapm_i2s_widgets,
			ARRAY_SIZE(tabla_dapm_i2s_widgets));
		snd_soc_dapm_add_routes(dapm, audio_i2s_map,
			ARRAY_SIZE(audio_i2s_map));
		for (i = 0; i < ARRAY_SIZE(tabla_i2s_dai); i++)
			INIT_LIST_HEAD(&tabla->dai[i].wcd9xxx_ch_list);
	} else if (tabla->intf_type == WCD9XXX_INTERFACE_TYPE_SLIMBUS) {
		for (i = 0; i < NUM_CODEC_DAIS; i++) {
			INIT_LIST_HEAD(&tabla->dai[i].wcd9xxx_ch_list);
			init_waitqueue_head(&tabla->dai[i].dai_wait);
		}
	}

	control->num_rx_port = TABLA_RX_MAX;
	control->rx_chs = ptr;
	memcpy(control->rx_chs, tabla_rx_chs, sizeof(tabla_rx_chs));
	control->num_tx_port = TABLA_TX_MAX;
	control->tx_chs = ptr + sizeof(tabla_rx_chs);
	memcpy(control->tx_chs, tabla_tx_chs, sizeof(tabla_tx_chs));


	if (TABLA_IS_1_X(control->version)) {
		snd_soc_dapm_add_routes(dapm, tabla_1_x_lineout_2_to_4_map,
				      ARRAY_SIZE(tabla_1_x_lineout_2_to_4_map));
	} else if (TABLA_IS_2_0(control->version)) {
		snd_soc_dapm_add_routes(dapm, tabla_2_x_lineout_2_to_4_map,
				      ARRAY_SIZE(tabla_2_x_lineout_2_to_4_map));
	} else  {
		pr_err("%s : ERROR.  Unsupported Tabla version 0x%2x\n",
			__func__, control->version);
		goto err_pdata;
	}

	snd_soc_dapm_sync(dapm);

	ret = wcd9xxx_request_irq(core_res,
				  WCD9XXX_IRQ_MBHC_INSERTION,
		tabla_hs_insert_irq, "Headset insert detect", tabla);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
		       WCD9XXX_IRQ_MBHC_INSERTION);
		goto err_insert_irq;
	}
	wcd9xxx_disable_irq(core_res, WCD9XXX_IRQ_MBHC_INSERTION);

	ret = wcd9xxx_request_irq(core_res,
				  WCD9XXX_IRQ_MBHC_REMOVAL,
				  tabla_hs_remove_irq,
				  "Headset remove detect", tabla);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
		       WCD9XXX_IRQ_MBHC_REMOVAL);
		goto err_remove_irq;
	}

	ret = wcd9xxx_request_irq(core_res,
				  WCD9XXX_IRQ_MBHC_POTENTIAL,
				  tabla_dce_handler, "DC Estimation detect",
				  tabla);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
		       WCD9XXX_IRQ_MBHC_POTENTIAL);
		goto err_potential_irq;
	}

	ret = wcd9xxx_request_irq(core_res, WCD9XXX_IRQ_MBHC_RELEASE,
				  tabla_release_handler,
				  "Button Release detect", tabla);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
		       WCD9XXX_IRQ_MBHC_RELEASE);
		goto err_release_irq;
	}

	ret = wcd9xxx_request_irq(core_res, WCD9XXX_IRQ_SLIMBUS,
				  tabla_slimbus_irq, "SLIMBUS Slave", tabla);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
		       WCD9XXX_IRQ_SLIMBUS);
		goto err_slimbus_irq;
	}

	for (i = 0; i < WCD9XXX_SLIM_NUM_PORT_REG; i++)
		wcd9xxx_interface_reg_write(control,
				TABLA_SLIM_PGD_PORT_INT_EN0 + i, 0xFF);

	ret = wcd9xxx_request_irq(core_res,
				  WCD9XXX_IRQ_HPH_PA_OCPL_FAULT,
				  tabla_hphl_ocp_irq,
				  "HPH_L OCP detect", tabla);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
		       WCD9XXX_IRQ_HPH_PA_OCPL_FAULT);
		goto err_hphl_ocp_irq;
	}
	wcd9xxx_disable_irq(core_res, WCD9XXX_IRQ_HPH_PA_OCPL_FAULT);

	ret = wcd9xxx_request_irq(core_res,
				  WCD9XXX_IRQ_HPH_PA_OCPR_FAULT,
				  tabla_hphr_ocp_irq,
				  "HPH_R OCP detect", tabla);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
		       WCD9XXX_IRQ_HPH_PA_OCPR_FAULT);
		goto err_hphr_ocp_irq;
	}
	wcd9xxx_disable_irq(core_res, WCD9XXX_IRQ_HPH_PA_OCPR_FAULT);

	/*
	 * Register suspend lock and notifier to resend edge triggered
	 * gpio IRQs
	 */
	wake_lock_init(&tabla->irq_resend_wlock, WAKE_LOCK_SUSPEND,
		       "tabla_gpio_irq_resend");
	tabla->gpio_irq_resend = false;

	mutex_lock(&dapm->codec->mutex);
	snd_soc_dapm_disable_pin(dapm, "ANC HPHL");
	snd_soc_dapm_disable_pin(dapm, "ANC HPHR");
	snd_soc_dapm_disable_pin(dapm, "ANC HEADPHONE");
	snd_soc_dapm_sync(dapm);
	mutex_unlock(&dapm->codec->mutex);

#ifdef CONFIG_DEBUG_FS
	if (ret == 0) {
		tabla->debugfs_poke =
		    debugfs_create_file("TRRS", S_IFREG | S_IRUGO, NULL, tabla,
					&codec_debug_ops);
		tabla->debugfs_mbhc =
		    debugfs_create_file("tabla_mbhc", S_IFREG | S_IRUGO,
					NULL, tabla, &codec_mbhc_debug_ops);
	}
#endif
	codec->ignore_pmdown_time = 1;
	return ret;

err_hphr_ocp_irq:
	wcd9xxx_free_irq(core_res,
			WCD9XXX_IRQ_HPH_PA_OCPL_FAULT, tabla);
err_hphl_ocp_irq:
	wcd9xxx_free_irq(core_res, WCD9XXX_IRQ_SLIMBUS, tabla);
err_slimbus_irq:
	wcd9xxx_free_irq(core_res, WCD9XXX_IRQ_MBHC_RELEASE, tabla);
err_release_irq:
	wcd9xxx_free_irq(core_res, WCD9XXX_IRQ_MBHC_POTENTIAL,
			 tabla);
err_potential_irq:
	wcd9xxx_free_irq(core_res, WCD9XXX_IRQ_MBHC_REMOVAL, tabla);
err_remove_irq:
	wcd9xxx_free_irq(core_res, WCD9XXX_IRQ_MBHC_INSERTION,
			 tabla);
err_insert_irq:
err_pdata:
	kfree(ptr);
err_nomem_slimch:
	mutex_destroy(&tabla->codec_resource_lock);
	kfree(tabla);
	return ret;
}
static int tabla_codec_remove(struct snd_soc_codec *codec)
{
	struct tabla_priv *tabla = snd_soc_codec_get_drvdata(codec);

	wake_lock_destroy(&tabla->irq_resend_wlock);

	wcd9xxx_free_irq(codec->control_data, WCD9XXX_IRQ_SLIMBUS, tabla);
	wcd9xxx_free_irq(codec->control_data, WCD9XXX_IRQ_MBHC_RELEASE, tabla);
	wcd9xxx_free_irq(codec->control_data, WCD9XXX_IRQ_MBHC_POTENTIAL,
			 tabla);
	wcd9xxx_free_irq(codec->control_data, WCD9XXX_IRQ_MBHC_REMOVAL, tabla);
	wcd9xxx_free_irq(codec->control_data, WCD9XXX_IRQ_MBHC_INSERTION,
			 tabla);
	TABLA_ACQUIRE_LOCK(tabla->codec_resource_lock);
	tabla_codec_disable_clock_block(codec);
	TABLA_RELEASE_LOCK(tabla->codec_resource_lock);
	tabla_codec_enable_bandgap(codec, TABLA_BANDGAP_OFF);
	if (tabla->mbhc_fw)
		release_firmware(tabla->mbhc_fw);
	mutex_destroy(&tabla->codec_resource_lock);
#ifdef CONFIG_DEBUG_FS
	debugfs_remove(tabla->debugfs_poke);
	debugfs_remove(tabla->debugfs_mbhc);
#endif
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
        .controls = tabla_snd_controls,
        .num_controls = ARRAY_SIZE(tabla_snd_controls),
        .dapm_widgets = tabla_dapm_widgets,
        .num_dapm_widgets = ARRAY_SIZE(tabla_dapm_widgets),
        .dapm_routes = audio_map,
        .num_dapm_routes = ARRAY_SIZE(audio_map),
};

#ifdef CONFIG_PM
static int tabla_suspend(struct device *dev)
{
	dev_dbg(dev, "%s: system suspend\n", __func__);
	return 0;
}

static int tabla_resume(struct device *dev)
{
	int irq;
	struct platform_device *pdev = to_platform_device(dev);
	struct tabla_priv *tabla = platform_get_drvdata(pdev);

	dev_dbg(dev, "%s: system resume tabla %p\n", __func__, tabla);
	if (tabla) {
		TABLA_ACQUIRE_LOCK(tabla->codec_resource_lock);
		tabla->mbhc_last_resume = jiffies;
		if (tabla->gpio_irq_resend) {
			WARN_ON(!tabla->mbhc_cfg.gpio_irq);
			tabla->gpio_irq_resend = false;

			irq = tabla->mbhc_cfg.gpio_irq;
			pr_debug("%s: Resending GPIO IRQ %d\n", __func__, irq);
			irq_set_pending(irq);
			check_irq_resend(irq_to_desc(irq), irq);

			/* release suspend lock */
			wake_unlock(&tabla->irq_resend_wlock);
		}
		TABLA_RELEASE_LOCK(tabla->codec_resource_lock);
	}

	return 0;
}

static const struct dev_pm_ops tabla_pm_ops = {
	.suspend	= tabla_suspend,
	.resume		= tabla_resume,
};
#endif

static int __devinit tabla_probe(struct platform_device *pdev)
{
	int ret = 0;
	pr_err("tabla_probe\n");
	if (wcd9xxx_get_intf_type() == WCD9XXX_INTERFACE_TYPE_SLIMBUS)
		ret = snd_soc_register_codec(&pdev->dev, &soc_codec_dev_tabla,
			tabla_dai, ARRAY_SIZE(tabla_dai));
	else if (wcd9xxx_get_intf_type() == WCD9XXX_INTERFACE_TYPE_I2C)
		ret = snd_soc_register_codec(&pdev->dev, &soc_codec_dev_tabla,
			tabla_i2s_dai, ARRAY_SIZE(tabla_i2s_dai));
	return ret;
}
static int __devexit tabla_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
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

static struct platform_driver tabla1x_codec_driver = {
	.probe = tabla_probe,
	.remove = tabla_remove,
	.driver = {
		.name = "tabla1x_codec",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &tabla_pm_ops,
#endif
	},
};

static int __init tabla_codec_init(void)
{
	int rtn = platform_driver_register(&tabla_codec_driver);
	if (rtn == 0) {
		rtn = platform_driver_register(&tabla1x_codec_driver);
		if (rtn != 0)
			platform_driver_unregister(&tabla_codec_driver);
	}
	return rtn;
}

static void __exit tabla_codec_exit(void)
{
	platform_driver_unregister(&tabla1x_codec_driver);
	platform_driver_unregister(&tabla_codec_driver);
}

module_init(tabla_codec_init);
module_exit(tabla_codec_exit);

MODULE_DESCRIPTION("Tabla codec driver");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL v2");
