/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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
#include <linux/mfd/wcd9xxx/wcd9306_registers.h>
#include <linux/mfd/wcd9xxx/pdata.h>
#include <linux/regulator/consumer.h>
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
#include "wcd9306.h"
#include "wcd9xxx-resmgr.h"
#include "wcd9xxx-common.h"
#include "wcdcal-hwdep.h"

#define TAPAN_HPH_PA_SETTLE_COMP_ON 5000
#define TAPAN_HPH_PA_SETTLE_COMP_OFF 13000

#define DAPM_MICBIAS2_EXTERNAL_STANDALONE "MIC BIAS2 External Standalone"
#define TAPAN_VALIDATE_RX_SBPORT_RANGE(port) ((port >= 16) && (port <= 20))
#define TAPAN_CONVERT_RX_SBPORT_ID(port) (port - 16) /* RX1 port ID = 0 */

#define TAPAN_VDD_CX_OPTIMAL_UA 10000
#define TAPAN_VDD_CX_SLEEP_UA 2000

/* RX_HPH_CNP_WG_TIME increases by 0.24ms */
#define TAPAN_WG_TIME_FACTOR_US  240

#define TAPAN_SB_PGD_PORT_RX_BASE   0x40
#define TAPAN_SB_PGD_PORT_TX_BASE   0x50
#define TAPAN_REGISTER_START_OFFSET 0x800

#define CODEC_REG_CFG_MINOR_VER 1

#define BUS_DOWN 1

static struct regulator *tapan_codec_find_regulator(
	struct snd_soc_codec *codec,
	const char *name);

static atomic_t kp_tapan_priv;
static int spkr_drv_wrnd_param_set(const char *val,
				   const struct kernel_param *kp);
static int spkr_drv_wrnd = 1;

static struct kernel_param_ops spkr_drv_wrnd_param_ops = {
	.set = spkr_drv_wrnd_param_set,
	.get = param_get_int,
};
module_param_cb(spkr_drv_wrnd, &spkr_drv_wrnd_param_ops, &spkr_drv_wrnd, 0644);
MODULE_PARM_DESC(spkr_drv_wrnd,
	       "Run software workaround to avoid leakage on the speaker drive");

#define WCD9306_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |\
			SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |\
			SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_192000)

#define WCD9302_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |\
			SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000)

#define NUM_DECIMATORS 4
#define NUM_INTERPOLATORS 4
#define BITS_PER_REG 8
/* This actual number of TX ports supported in slimbus slave */
#define TAPAN_TX_PORT_NUMBER	16
#define TAPAN_RX_PORT_START_NUMBER	16

/* Nummer of TX ports actually connected from Slimbus slave to codec Digital */
#define TAPAN_SLIM_CODEC_TX_PORTS 5

#define TAPAN_I2S_MASTER_MODE_MASK 0x08
#define TAPAN_MCLK_CLK_12P288MHZ 12288000
#define TAPAN_MCLK_CLK_9P6MHZ 9600000

#define TAPAN_SLIM_CLOSE_TIMEOUT 1000
#define TAPAN_SLIM_IRQ_OVERFLOW (1 << 0)
#define TAPAN_SLIM_IRQ_UNDERFLOW (1 << 1)
#define TAPAN_SLIM_IRQ_PORT_CLOSED (1 << 2)

/*
 * Multiplication factor to compute impedance on Tapan
 * This is computed from (Vx / (m*Ical)) = (10mV/(180*30uA))
 */
#define TAPAN_ZDET_MUL_FACTOR 1852

static struct afe_param_cdc_reg_cfg audio_reg_cfg[] = {
	{
		CODEC_REG_CFG_MINOR_VER,
		(TAPAN_REGISTER_START_OFFSET + TAPAN_SB_PGD_PORT_TX_BASE),
		SB_PGD_PORT_TX_WATERMARK_N, 0x1E, 8, 0x1
	},
	{
		CODEC_REG_CFG_MINOR_VER,
		(TAPAN_REGISTER_START_OFFSET + TAPAN_SB_PGD_PORT_TX_BASE),
		SB_PGD_PORT_TX_ENABLE_N, 0x1, 8, 0x1
	},
	{
		CODEC_REG_CFG_MINOR_VER,
		(TAPAN_REGISTER_START_OFFSET + TAPAN_SB_PGD_PORT_RX_BASE),
		SB_PGD_PORT_RX_WATERMARK_N, 0x1E, 8, 0x1
	},
	{
		CODEC_REG_CFG_MINOR_VER,
		(TAPAN_REGISTER_START_OFFSET + TAPAN_SB_PGD_PORT_RX_BASE),
		SB_PGD_PORT_RX_ENABLE_N, 0x1, 8, 0x1
	},
	{
		CODEC_REG_CFG_MINOR_VER,
		(TAPAN_REGISTER_START_OFFSET + TAPAN_A_CDC_ANC1_IIR_B1_CTL),
		AANC_FF_GAIN_ADAPTIVE, 0x4, 8, 0
	},
	{
		CODEC_REG_CFG_MINOR_VER,
		(TAPAN_REGISTER_START_OFFSET + TAPAN_A_CDC_ANC1_IIR_B1_CTL),
		AANC_FFGAIN_ADAPTIVE_EN, 0x8, 8, 0
	},
	{
		CODEC_REG_CFG_MINOR_VER,
		(TAPAN_REGISTER_START_OFFSET + TAPAN_A_CDC_ANC1_GAIN_CTL),
		AANC_GAIN_CONTROL, 0xFF, 8, 0
	},
};

static struct afe_param_cdc_reg_cfg_data tapan_audio_reg_cfg = {
	.num_registers = ARRAY_SIZE(audio_reg_cfg),
	.reg_data = audio_reg_cfg,
};

static struct afe_param_id_cdc_aanc_version tapan_cdc_aanc_version = {
	.cdc_aanc_minor_version = AFE_API_VERSION_CDC_AANC_VERSION,
	.aanc_hw_version        = AANC_HW_BLOCK_VERSION_2,
};

enum tapan_codec_type {
	WCD9306,
	WCD9302,
};

static enum tapan_codec_type codec_ver;

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
	RX_MIX1_INP_SEL_AUXRX,
};

#define TAPAN_COMP_DIGITAL_GAIN_OFFSET 3

static const DECLARE_TLV_DB_SCALE(digital_gain, 0, 1, 0);
static const DECLARE_TLV_DB_SCALE(line_gain, 0, 7, 1);
static const DECLARE_TLV_DB_SCALE(analog_gain, 0, 25, 1);
static struct snd_soc_dai_driver tapan_dai[];
static const DECLARE_TLV_DB_SCALE(aux_pga_gain, 0, 2, 0);

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
	COMPANDER_0,
	COMPANDER_1,
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

struct comp_sample_dependent_params {
	u32 peak_det_timeout;
	u32 rms_meter_div_fact;
	u32 rms_meter_resamp_fact;
};

struct hpf_work {
	struct tapan_priv *tapan;
	u32 decimator;
	u8 tx_hpf_cut_of_freq;
	struct delayed_work dwork;
};

static struct hpf_work tx_hpf_work[NUM_DECIMATORS];

static const struct wcd9xxx_ch tapan_rx_chs[TAPAN_RX_MAX] = {
	WCD9XXX_CH(TAPAN_RX_PORT_START_NUMBER, 0),
	WCD9XXX_CH(TAPAN_RX_PORT_START_NUMBER + 1, 1),
	WCD9XXX_CH(TAPAN_RX_PORT_START_NUMBER + 2, 2),
	WCD9XXX_CH(TAPAN_RX_PORT_START_NUMBER + 3, 3),
	WCD9XXX_CH(TAPAN_RX_PORT_START_NUMBER + 4, 4),
};

static const struct wcd9xxx_ch tapan_tx_chs[TAPAN_TX_MAX] = {
	WCD9XXX_CH(0, 0),
	WCD9XXX_CH(1, 1),
	WCD9XXX_CH(2, 2),
	WCD9XXX_CH(3, 3),
	WCD9XXX_CH(4, 4),
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
	0,	/* AIF1_PB */
	0,	/* AIF1_CAP */
};

enum {
	CP_REG_BUCK = 0,
	CP_REG_BHELPER,
	CP_REG_MAX,
};

struct tapan_priv {
	struct snd_soc_codec *codec;
	u32 adc_count;
	u32 rx_bias_count;
	s32 dmic_1_2_clk_cnt;
	s32 dmic_3_4_clk_cnt;
	s32 dmic_5_6_clk_cnt;
	s32 ldo_h_users;
	s32 micb_2_users;

	u32 anc_slot;
	bool anc_func;

	/* cal info for codec */
	struct fw_info *fw_data;

	/*track adie loopback mode*/
	bool lb_mode;

	/*track tapan interface type*/
	u8 intf_type;

	/* num of slim ports required */
	struct wcd9xxx_codec_dai_data  dai[NUM_CODEC_DAIS];

	/*compander*/
	int comp_enabled[COMPANDER_MAX];
	u32 comp_fs[COMPANDER_MAX];

	/* Maintain the status of AUX PGA */
	int aux_pga_cnt;
	u8 aux_l_gain;
	u8 aux_r_gain;

	bool dec_active[NUM_DECIMATORS];

	bool spkr_pa_widget_on;
	struct regulator *spkdrv_reg;

	struct afe_param_cdc_slimbus_slave_cfg slimbus_slave_cfg;

	/* resmgr module */
	struct wcd9xxx_resmgr resmgr;
	/* mbhc module */
	struct wcd9xxx_mbhc mbhc;

	/* class h specific data */
	struct wcd9xxx_clsh_cdc_data clsh_d;

	/* pointers to regulators required for chargepump */
	struct regulator *cp_regulators[CP_REG_MAX];

	/*
	 * list used to save/restore registers at start and
	 * end of impedance measurement
	 */
	struct list_head reg_save_restore;

	int (*machine_codec_event_cb)(struct snd_soc_codec *codec,
					enum wcd9xxx_codec_event);
	unsigned long status_mask;
};

static const u32 comp_shift[] = {
	0,
	1,
	2,
};

static const int comp_rx_path[] = {
	COMPANDER_1,
	COMPANDER_1,
	COMPANDER_2,
	COMPANDER_2,
	COMPANDER_MAX,
};

static const struct comp_sample_dependent_params comp_samp_params[] = {
	{
		/* 8 Khz */
		.peak_det_timeout = 0x06,
		.rms_meter_div_fact = 0x09,
		.rms_meter_resamp_fact = 0x06,
	},
	{
		/* 16 Khz */
		.peak_det_timeout = 0x07,
		.rms_meter_div_fact = 0x0A,
		.rms_meter_resamp_fact = 0x0C,
	},
	{
		/* 32 Khz */
		.peak_det_timeout = 0x08,
		.rms_meter_div_fact = 0x0B,
		.rms_meter_resamp_fact = 0x1E,
	},
	{
		/* 48 Khz */
		.peak_det_timeout = 0x09,
		.rms_meter_div_fact = 0x0B,
		.rms_meter_resamp_fact = 0x28,
	},
	{
		/* 96 Khz */
		.peak_det_timeout = 0x0A,
		.rms_meter_div_fact = 0x0C,
		.rms_meter_resamp_fact = 0x50,
	},
	{
		/* 192 Khz */
		.peak_det_timeout = 0x0B,
		.rms_meter_div_fact = 0xC,
		.rms_meter_resamp_fact = 0xA0,
	},
};

static unsigned short rx_digital_gain_reg[] = {
	TAPAN_A_CDC_RX1_VOL_CTL_B2_CTL,
	TAPAN_A_CDC_RX2_VOL_CTL_B2_CTL,
	TAPAN_A_CDC_RX3_VOL_CTL_B2_CTL,
	TAPAN_A_CDC_RX4_VOL_CTL_B2_CTL,
};

static unsigned short tx_digital_gain_reg[] = {
	TAPAN_A_CDC_TX1_VOL_CTL_GAIN,
	TAPAN_A_CDC_TX2_VOL_CTL_GAIN,
	TAPAN_A_CDC_TX3_VOL_CTL_GAIN,
	TAPAN_A_CDC_TX4_VOL_CTL_GAIN,
};

static int spkr_drv_wrnd_param_set(const char *val,
				   const struct kernel_param *kp)
{
	struct snd_soc_codec *codec;
	int ret, old;
	struct tapan_priv *priv;

	priv = (struct tapan_priv *)(unsigned long)atomic_read(&kp_tapan_priv);
	if (!priv) {
		pr_debug("%s: codec isn't yet registered\n", __func__);
		return 0;
	}

	codec = priv->codec;
	mutex_lock(&codec->mutex);
	old = spkr_drv_wrnd;
	ret = param_set_int(val, kp);
	if (ret) {
		mutex_unlock(&codec->mutex);
		return ret;
	}

	dev_dbg(codec->dev, "%s: spkr_drv_wrnd %d -> %d\n",
			__func__, old, spkr_drv_wrnd);
	if ((old == -1 || old == 0) && spkr_drv_wrnd == 1) {
		WCD9XXX_BG_CLK_LOCK(&priv->resmgr);
		wcd9xxx_resmgr_get_bandgap(&priv->resmgr,
					   WCD9XXX_BANDGAP_AUDIO_MODE);
		WCD9XXX_BG_CLK_UNLOCK(&priv->resmgr);
		snd_soc_update_bits(codec, TAPAN_A_SPKR_DRV_EN, 0x80, 0x80);
	} else if (old == 1 && spkr_drv_wrnd == 0) {
		WCD9XXX_BG_CLK_LOCK(&priv->resmgr);
		wcd9xxx_resmgr_put_bandgap(&priv->resmgr,
					   WCD9XXX_BANDGAP_AUDIO_MODE);
		WCD9XXX_BG_CLK_UNLOCK(&priv->resmgr);
		if (!priv->spkr_pa_widget_on)
			snd_soc_update_bits(codec, TAPAN_A_SPKR_DRV_EN, 0x80,
					    0x00);
	}
	mutex_unlock(&codec->mutex);

	return 0;
}

static int tapan_get_anc_slot(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct tapan_priv *tapan = snd_soc_codec_get_drvdata(codec);
	ucontrol->value.integer.value[0] = tapan->anc_slot;
	return 0;
}

static int tapan_put_anc_slot(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct tapan_priv *tapan = snd_soc_codec_get_drvdata(codec);
	tapan->anc_slot = ucontrol->value.integer.value[0];
	return 0;
}

static int tapan_get_anc_func(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct tapan_priv *tapan = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = (tapan->anc_func == true ? 1 : 0);
	return 0;
}

static int tapan_put_anc_func(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct tapan_priv *tapan = snd_soc_codec_get_drvdata(codec);
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	mutex_lock(&dapm->codec->mutex);
	tapan->anc_func = (!ucontrol->value.integer.value[0] ? false : true);

	dev_err(codec->dev, "%s: anc_func %x", __func__, tapan->anc_func);

	if (tapan->anc_func == true) {
		pr_info("enable anc virtual widgets");
		snd_soc_dapm_enable_pin(dapm, "ANC HPHR");
		snd_soc_dapm_enable_pin(dapm, "ANC HPHL");
		snd_soc_dapm_enable_pin(dapm, "ANC HEADPHONE");
		snd_soc_dapm_enable_pin(dapm, "ANC EAR PA");
		snd_soc_dapm_enable_pin(dapm, "ANC EAR");
		snd_soc_dapm_disable_pin(dapm, "HPHR");
		snd_soc_dapm_disable_pin(dapm, "HPHL");
		snd_soc_dapm_disable_pin(dapm, "HEADPHONE");
		snd_soc_dapm_disable_pin(dapm, "EAR PA");
		snd_soc_dapm_disable_pin(dapm, "EAR");
	} else {
		pr_info("disable anc virtual widgets");
		snd_soc_dapm_disable_pin(dapm, "ANC HPHR");
		snd_soc_dapm_disable_pin(dapm, "ANC HPHL");
		snd_soc_dapm_disable_pin(dapm, "ANC HEADPHONE");
		snd_soc_dapm_disable_pin(dapm, "ANC EAR PA");
		snd_soc_dapm_disable_pin(dapm, "ANC EAR");
		snd_soc_dapm_enable_pin(dapm, "HPHR");
		snd_soc_dapm_enable_pin(dapm, "HPHL");
		snd_soc_dapm_enable_pin(dapm, "HEADPHONE");
		snd_soc_dapm_enable_pin(dapm, "EAR PA");
		snd_soc_dapm_enable_pin(dapm, "EAR");
	}
	mutex_unlock(&dapm->codec->mutex);
	snd_soc_dapm_sync(dapm);
	return 0;
}

static int tapan_loopback_mode_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct tapan_priv *tapan = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = tapan->lb_mode;
	dev_dbg(codec->dev, "%s: lb_mode = %d\n",
		__func__, tapan->lb_mode);

	return 0;
}

static int tapan_loopback_mode_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct tapan_priv *tapan = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s: ucontrol->value.integer.value[0] = %ld\n",
		__func__, ucontrol->value.integer.value[0]);

	switch (ucontrol->value.integer.value[0]) {
	case 0:
		tapan->lb_mode = false;
		break;
	case 1:
		tapan->lb_mode = true;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}


static int tapan_pa_gain_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	u8 ear_pa_gain;
	int rc = 0;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	ear_pa_gain = snd_soc_read(codec, TAPAN_A_RX_EAR_GAIN);
	ear_pa_gain = ear_pa_gain >> 5;

	switch (ear_pa_gain) {
	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
		ucontrol->value.integer.value[0] = ear_pa_gain;
		break;
	case 7:
		ucontrol->value.integer.value[0] = (ear_pa_gain - 1);
		break;
	default:
		rc = -EINVAL;
		pr_err("%s: ERROR: Unsupported Ear Gain = 0x%x\n",
		       __func__, ear_pa_gain);
		break;
	}

	dev_dbg(codec->dev, "%s: ear_pa_gain = 0x%x\n", __func__, ear_pa_gain);

	return rc;
}

static int tapan_pa_gain_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	u8 ear_pa_gain;
	int rc = 0;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	dev_dbg(codec->dev, "%s: ucontrol->value.integer.value[0]  = %ld\n",
			 __func__, ucontrol->value.integer.value[0]);

	switch (ucontrol->value.integer.value[0]) {
	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
		ear_pa_gain = ucontrol->value.integer.value[0];
		break;
	case 6:
		ear_pa_gain = 0x07;
		break;
	default:
		rc = -EINVAL;
		break;
	}
	if (!rc)
		snd_soc_update_bits(codec, TAPAN_A_RX_EAR_GAIN,
				    0xE0, ear_pa_gain << 5);
	return rc;
}

static int tapan_get_iir_enable_audio_mixer(
					struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	int iir_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int band_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;

	ucontrol->value.integer.value[0] =
		(snd_soc_read(codec, (TAPAN_A_CDC_IIR1_CTL + 16 * iir_idx)) &
		(1 << band_idx)) != 0;

	dev_dbg(codec->dev, "%s: IIR #%d band #%d enable %d\n", __func__,
		iir_idx, band_idx,
		(uint32_t)ucontrol->value.integer.value[0]);
	return 0;
}

static int tapan_put_iir_enable_audio_mixer(
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
	snd_soc_update_bits(codec, (TAPAN_A_CDC_IIR1_CTL + 16 * iir_idx),
		(1 << band_idx), (value << band_idx));

	pr_debug("%s: IIR #%d band #%d enable %d\n", __func__,
		iir_idx, band_idx,
		((snd_soc_read(codec, (TAPAN_A_CDC_IIR1_CTL + 16 * iir_idx)) &
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
		(TAPAN_A_CDC_IIR1_COEF_B1_CTL + 16 * iir_idx),
		((band_idx * BAND_MAX + coeff_idx)
		* sizeof(uint32_t)) & 0x7F);

	value |= snd_soc_read(codec,
		(TAPAN_A_CDC_IIR1_COEF_B2_CTL + 16 * iir_idx));

	snd_soc_write(codec,
		(TAPAN_A_CDC_IIR1_COEF_B1_CTL + 16 * iir_idx),
		((band_idx * BAND_MAX + coeff_idx)
		* sizeof(uint32_t) + 1) & 0x7F);

	value |= (snd_soc_read(codec,
		(TAPAN_A_CDC_IIR1_COEF_B2_CTL + 16 * iir_idx)) << 8);

	snd_soc_write(codec,
		(TAPAN_A_CDC_IIR1_COEF_B1_CTL + 16 * iir_idx),
		((band_idx * BAND_MAX + coeff_idx)
		* sizeof(uint32_t) + 2) & 0x7F);

	value |= (snd_soc_read(codec,
		(TAPAN_A_CDC_IIR1_COEF_B2_CTL + 16 * iir_idx)) << 16);

	snd_soc_write(codec,
		(TAPAN_A_CDC_IIR1_COEF_B1_CTL + 16 * iir_idx),
		((band_idx * BAND_MAX + coeff_idx)
		* sizeof(uint32_t) + 3) & 0x7F);

	/* Mask bits top 2 bits since they are reserved */
	value |= ((snd_soc_read(codec,
		(TAPAN_A_CDC_IIR1_COEF_B2_CTL + 16 * iir_idx)) & 0x3F) << 24);

	return value;

}

static int tapan_get_iir_band_audio_mixer(
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
		(TAPAN_A_CDC_IIR1_COEF_B2_CTL + 16 * iir_idx),
		(value & 0xFF));

	snd_soc_write(codec,
		(TAPAN_A_CDC_IIR1_COEF_B2_CTL + 16 * iir_idx),
		(value >> 8) & 0xFF);

	snd_soc_write(codec,
		(TAPAN_A_CDC_IIR1_COEF_B2_CTL + 16 * iir_idx),
		(value >> 16) & 0xFF);

	/* Mask top 2 bits, 7-8 are reserved */
	snd_soc_write(codec,
		(TAPAN_A_CDC_IIR1_COEF_B2_CTL + 16 * iir_idx),
		(value >> 24) & 0x3F);

}

static int tapan_put_iir_band_audio_mixer(
					struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	int iir_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->reg;
	int band_idx = ((struct soc_multi_mixer_control *)
					kcontrol->private_value)->shift;

	/* Mask top bit it is reserved */
	/* Updates addr automatically for each B2 write */
	snd_soc_write(codec,
		(TAPAN_A_CDC_IIR1_COEF_B1_CTL + 16 * iir_idx),
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

static int tapan_get_compander(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{

	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	int comp = ((struct soc_multi_mixer_control *)
		    kcontrol->private_value)->shift;
	struct tapan_priv *tapan = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = tapan->comp_enabled[comp];
	return 0;
}

static int tapan_set_compander(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct tapan_priv *tapan = snd_soc_codec_get_drvdata(codec);
	int comp = ((struct soc_multi_mixer_control *)
		    kcontrol->private_value)->shift;
	int value = ucontrol->value.integer.value[0];

	dev_dbg(codec->dev, "%s: Compander %d enable current %d, new %d\n",
		 __func__, comp, tapan->comp_enabled[comp], value);
	tapan->comp_enabled[comp] = value;

	if (comp == COMPANDER_1 &&
			tapan->comp_enabled[comp] == 1) {
		/* Wavegen to 5 msec */
		snd_soc_write(codec, TAPAN_A_RX_HPH_CNP_WG_CTL, 0xDA);
		snd_soc_write(codec, TAPAN_A_RX_HPH_CNP_WG_TIME, 0x15);
		snd_soc_write(codec, TAPAN_A_RX_HPH_BIAS_WG_OCP, 0x2A);

		/* Enable Chopper */
		snd_soc_update_bits(codec,
			TAPAN_A_RX_HPH_CHOP_CTL, 0x80, 0x80);

		snd_soc_write(codec, TAPAN_A_NCP_DTEST, 0x20);
		pr_debug("%s: Enabled Chopper and set wavegen to 5 msec\n",
				__func__);
	} else if (comp == COMPANDER_1 &&
			tapan->comp_enabled[comp] == 0) {
		/* Wavegen to 20 msec */
		snd_soc_write(codec, TAPAN_A_RX_HPH_CNP_WG_CTL, 0xDB);
		snd_soc_write(codec, TAPAN_A_RX_HPH_CNP_WG_TIME, 0x58);
		snd_soc_write(codec, TAPAN_A_RX_HPH_BIAS_WG_OCP, 0x1A);

		/* Disable CHOPPER block */
		snd_soc_update_bits(codec,
			TAPAN_A_RX_HPH_CHOP_CTL, 0x80, 0x00);

		snd_soc_write(codec, TAPAN_A_NCP_DTEST, 0x10);
		pr_debug("%s: Disabled Chopper and set wavegen to 20 msec\n",
				__func__);
	}

	return 0;
}

static int tapan_config_gain_compander(struct snd_soc_codec *codec,
				       int comp, bool enable)
{
	int ret = 0;

	switch (comp) {
	case COMPANDER_0:
		snd_soc_update_bits(codec, TAPAN_A_SPKR_DRV_GAIN,
				    1 << 2, !enable << 2);
		break;
	case COMPANDER_1:
		snd_soc_update_bits(codec, TAPAN_A_RX_HPH_L_GAIN,
				    1 << 5, !enable << 5);
		snd_soc_update_bits(codec, TAPAN_A_RX_HPH_R_GAIN,
				    1 << 5, !enable << 5);
		break;
	case COMPANDER_2:
		snd_soc_update_bits(codec, TAPAN_A_RX_LINE_1_GAIN,
				    1 << 5, !enable << 5);
		snd_soc_update_bits(codec, TAPAN_A_RX_LINE_2_GAIN,
				    1 << 5, !enable << 5);
		break;
	default:
		WARN_ON(1);
		ret = -EINVAL;
	}

	return ret;
}

static void tapan_discharge_comp(struct snd_soc_codec *codec, int comp)
{
	/* Level meter DIV Factor to 5*/
	snd_soc_update_bits(codec, TAPAN_A_CDC_COMP0_B2_CTL + (comp * 8), 0xF0,
			    0x05 << 4);
	/* RMS meter Sampling to 0x01 */
	snd_soc_write(codec, TAPAN_A_CDC_COMP0_B3_CTL + (comp * 8), 0x01);

	/* Worst case timeout for compander CnP sleep timeout */
	usleep_range(3000, 3000);
}

static enum wcd9xxx_buck_volt tapan_codec_get_buck_mv(
	struct snd_soc_codec *codec)
{
	int buck_volt = WCD9XXX_CDC_BUCK_UNSUPPORTED;
	struct tapan_priv *tapan = snd_soc_codec_get_drvdata(codec);
	struct wcd9xxx_pdata *pdata = tapan->resmgr.pdata;
	int i;
	bool found_regulator = false;

	for (i = 0; i < ARRAY_SIZE(pdata->regulator); i++) {
		if (pdata->regulator[i].name == NULL)
			continue;

		if (!strncmp(pdata->regulator[i].name,
			     WCD9XXX_SUPPLY_BUCK_NAME,
			     sizeof(WCD9XXX_SUPPLY_BUCK_NAME))) {
			found_regulator = true;
			if ((pdata->regulator[i].min_uV ==
			     WCD9XXX_CDC_BUCK_MV_1P8) ||
			    (pdata->regulator[i].min_uV ==
			     WCD9XXX_CDC_BUCK_MV_2P15))
				buck_volt = pdata->regulator[i].min_uV;
			break;
		}
	}

	if (!found_regulator)
		dev_err(codec->dev,
			"%s: Failed to find regulator for %s\n",
			__func__, WCD9XXX_SUPPLY_BUCK_NAME);
	else
		dev_dbg(codec->dev,
			"%s: S4 voltage requested is %d\n",
			__func__, buck_volt);

	return buck_volt;
}

static int tapan_config_compander(struct snd_soc_dapm_widget *w,
				  struct snd_kcontrol *kcontrol, int event)
{
	int mask, enable_mask;
	u8 rdac5_mux;
	struct snd_soc_codec *codec = w->codec;
	struct tapan_priv *tapan = snd_soc_codec_get_drvdata(codec);
	const int comp = w->shift;
	const u32 rate = tapan->comp_fs[comp];
	const struct comp_sample_dependent_params *comp_params =
	    &comp_samp_params[rate];
	enum wcd9xxx_buck_volt buck_mv;

	dev_dbg(codec->dev, "%s: %s event %d compander %d, enabled %d",
		__func__, w->name, event, comp, tapan->comp_enabled[comp]);

	if (!tapan->comp_enabled[comp])
		return 0;

	/* Compander 0 has single channel */
	mask = (comp == COMPANDER_0 ? 0x01 : 0x03);
	buck_mv = tapan_codec_get_buck_mv(codec);

	rdac5_mux = snd_soc_read(codec, TAPAN_A_CDC_CONN_MISC);
	rdac5_mux = (rdac5_mux & 0x04) >> 2;

	if (comp == COMPANDER_0) {  /* SPK compander */
		enable_mask = 0x02;
	} else if (comp == COMPANDER_1) { /* HPH compander */
		enable_mask = 0x03;
	} else if (comp == COMPANDER_2) { /* LO compander */

		if (rdac5_mux == 0) { /* DEM4 */

			/* for LO Stereo SE, enable Compander 2 left
			 * channel on RX3 interpolator Path and Compander 2
			 * rigt channel on RX4 interpolator Path.
			 */
			enable_mask = 0x03;
		} else if (rdac5_mux == 1) { /* DEM3_INV */

			/* for LO mono differential only enable Compander 2
			 * left channel on RX3 interpolator Path.
			 */
			enable_mask = 0x02;
		} else {
			dev_err(codec->dev, "%s: invalid rdac5_mux val %d",
					__func__, rdac5_mux);
			return -EINVAL;
		}
	} else {
		dev_err(codec->dev, "%s: invalid compander %d", __func__, comp);
		return -EINVAL;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Set compander Sample rate */
		snd_soc_update_bits(codec,
				    TAPAN_A_CDC_COMP0_FS_CFG + (comp * 8),
				    0x07, rate);
		/* Set the static gain offset for HPH Path */
		if (comp == COMPANDER_1) {
			if (buck_mv == WCD9XXX_CDC_BUCK_MV_2P15)
				snd_soc_update_bits(codec,
					TAPAN_A_CDC_COMP0_B4_CTL + (comp * 8),
					0x80, 0x00);
			else
				snd_soc_update_bits(codec,
					TAPAN_A_CDC_COMP0_B4_CTL + (comp * 8),
					0x80, 0x80);
		}
		/* Enable RX interpolation path compander clocks */
		snd_soc_update_bits(codec, TAPAN_A_CDC_CLK_RX_B2_CTL,
				    0x01 << comp_shift[comp],
				    0x01 << comp_shift[comp]);

		/* Toggle compander reset bits */
		snd_soc_update_bits(codec, TAPAN_A_CDC_CLK_OTHR_RESET_B2_CTL,
				    0x01 << comp_shift[comp],
				    0x01 << comp_shift[comp]);
		snd_soc_update_bits(codec, TAPAN_A_CDC_CLK_OTHR_RESET_B2_CTL,
				    0x01 << comp_shift[comp], 0);

		/* Set gain source to compander */
		tapan_config_gain_compander(codec, comp, true);

		/* Compander enable */
		snd_soc_update_bits(codec, TAPAN_A_CDC_COMP0_B1_CTL +
				    (comp * 8), enable_mask, enable_mask);

		tapan_discharge_comp(codec, comp);

		/* Set sample rate dependent paramater */
		snd_soc_write(codec, TAPAN_A_CDC_COMP0_B3_CTL + (comp * 8),
			      comp_params->rms_meter_resamp_fact);
		snd_soc_update_bits(codec,
				    TAPAN_A_CDC_COMP0_B2_CTL + (comp * 8),
				    0xF0, comp_params->rms_meter_div_fact << 4);
		snd_soc_update_bits(codec,
					TAPAN_A_CDC_COMP0_B2_CTL + (comp * 8),
					0x0F, comp_params->peak_det_timeout);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		/* Disable compander */
		snd_soc_update_bits(codec,
				    TAPAN_A_CDC_COMP0_B1_CTL + (comp * 8),
				    enable_mask, 0x00);

		/* Toggle compander reset bits */
		snd_soc_update_bits(codec, TAPAN_A_CDC_CLK_OTHR_RESET_B2_CTL,
				    mask << comp_shift[comp],
				    mask << comp_shift[comp]);
		snd_soc_update_bits(codec, TAPAN_A_CDC_CLK_OTHR_RESET_B2_CTL,
				    mask << comp_shift[comp], 0);

		/* Turn off the clock for compander in pair */
		snd_soc_update_bits(codec, TAPAN_A_CDC_CLK_RX_B2_CTL,
				    mask << comp_shift[comp], 0);

		/* Set gain source to register */
		tapan_config_gain_compander(codec, comp, false);
		break;
	}
	return 0;
}

static const char * const tapan_loopback_mode_ctrl_text[] = {
		"DISABLE", "ENABLE"};
static const struct soc_enum tapan_loopback_mode_ctl_enum[] = {
		SOC_ENUM_SINGLE_EXT(2, tapan_loopback_mode_ctrl_text),
};


static const char * const tapan_ear_pa_gain_text[] = {"POS_6_DB", "POS_4P5_DB",
						      "POS_3_DB", "POS_1P5_DB",
						      "POS_0_DB", "NEG_2P5_DB",
						      "NEG_12_DB"};
static const struct soc_enum tapan_ear_pa_gain_enum[] = {
		SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(tapan_ear_pa_gain_text),
				    tapan_ear_pa_gain_text),
};

static const char *const tapan_anc_func_text[] = {"OFF", "ON"};
static const struct soc_enum tapan_anc_func_enum =
		SOC_ENUM_SINGLE_EXT(2, tapan_anc_func_text);

/*cut of frequency for high pass filter*/
static const char * const cf_text[] = {
	"MIN_3DB_4Hz", "MIN_3DB_75Hz", "MIN_3DB_150Hz"
};

static const struct soc_enum cf_dec1_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_TX1_MUX_CTL, 4, 3, cf_text);

static const struct soc_enum cf_dec2_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_TX2_MUX_CTL, 4, 3, cf_text);

static const struct soc_enum cf_dec3_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_TX3_MUX_CTL, 4, 3, cf_text);

static const struct soc_enum cf_dec4_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_TX4_MUX_CTL, 4, 3, cf_text);

static const struct soc_enum cf_rxmix1_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_RX1_B4_CTL, 0, 3, cf_text);

static const struct soc_enum cf_rxmix2_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_RX2_B4_CTL, 0, 3, cf_text);

static const struct soc_enum cf_rxmix3_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_RX3_B4_CTL, 0, 3, cf_text);

static const struct soc_enum cf_rxmix4_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_RX4_B4_CTL, 0, 3, cf_text);

static const char * const class_h_dsm_text[] = {
	"ZERO", "RX_HPHL", "RX_SPKR"
};

static const struct soc_enum class_h_dsm_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_CONN_CLSH_CTL, 2, 3, class_h_dsm_text);

static const struct snd_kcontrol_new class_h_dsm_mux =
	SOC_DAPM_ENUM("CLASS_H_DSM MUX Mux", class_h_dsm_enum);

static const char * const rx1_interpolator_text[] = {
	"ZERO", "RX1 MIX2"
};

static const struct soc_enum rx1_interpolator_enum =
	SOC_ENUM_SINGLE(0, 0, 2, rx1_interpolator_text);

static const struct snd_kcontrol_new rx1_interpolator =
	SOC_DAPM_ENUM_VIRT("RX1 INTERPOLATOR Mux", rx1_interpolator_enum);

static const char * const rx2_interpolator_text[] = {
	"ZERO", "RX2 MIX2"
};

static const struct soc_enum rx2_interpolator_enum =
	SOC_ENUM_SINGLE(0, 1, 2, rx2_interpolator_text);

static const struct snd_kcontrol_new rx2_interpolator =
	SOC_DAPM_ENUM_VIRT("RX2 INTERPOLATOR Mux", rx2_interpolator_enum);

static int tapan_hph_impedance_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	uint32_t zl, zr;
	bool hphr;
	struct soc_multi_mixer_control *mc;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct tapan_priv *priv = snd_soc_codec_get_drvdata(codec);

	mc = (struct soc_multi_mixer_control *)(kcontrol->private_value);

	hphr = mc->shift;
	wcd9xxx_mbhc_get_impedance(&priv->mbhc, &zl, &zr);
	pr_debug("%s: zl %u, zr %u\n", __func__, zl, zr);
	ucontrol->value.integer.value[0] = hphr ? zr : zl;

	return 0;
}

static const struct snd_kcontrol_new tapan_common_snd_controls[] = {

	SOC_ENUM_EXT("EAR PA Gain", tapan_ear_pa_gain_enum[0],
		tapan_pa_gain_get, tapan_pa_gain_put),

	SOC_ENUM_EXT("LOOPBACK Mode", tapan_loopback_mode_ctl_enum[0],
		tapan_loopback_mode_get, tapan_loopback_mode_put),

	SOC_SINGLE_TLV("HPHL Volume", TAPAN_A_RX_HPH_L_GAIN, 0, 20, 1,
		line_gain),
	SOC_SINGLE_TLV("HPHR Volume", TAPAN_A_RX_HPH_R_GAIN, 0, 20, 1,
		line_gain),

	SOC_SINGLE_TLV("LINEOUT1 Volume", TAPAN_A_RX_LINE_1_GAIN, 0, 20, 1,
		line_gain),
	SOC_SINGLE_TLV("LINEOUT2 Volume", TAPAN_A_RX_LINE_2_GAIN, 0, 20, 1,
		line_gain),

	SOC_SINGLE_TLV("SPK DRV Volume", TAPAN_A_SPKR_DRV_GAIN, 3, 8, 1,
		line_gain),

	SOC_SINGLE_TLV("ADC1 Volume", TAPAN_A_TX_1_EN, 2, 19, 0, analog_gain),
	SOC_SINGLE_TLV("ADC2 Volume", TAPAN_A_TX_2_EN, 2, 19, 0, analog_gain),
	SOC_SINGLE_TLV("ADC3 Volume", TAPAN_A_TX_3_EN, 2, 19, 0, analog_gain),
	SOC_SINGLE_TLV("ADC4 Volume", TAPAN_A_TX_4_EN, 2, 19, 0, analog_gain),

	SOC_SINGLE_SX_TLV("RX1 Digital Volume", TAPAN_A_CDC_RX1_VOL_CTL_B2_CTL,
		0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("RX2 Digital Volume", TAPAN_A_CDC_RX2_VOL_CTL_B2_CTL,
		0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("RX3 Digital Volume", TAPAN_A_CDC_RX3_VOL_CTL_B2_CTL,
		0, -84, 40, digital_gain),

	SOC_SINGLE_SX_TLV("DEC1 Volume", TAPAN_A_CDC_TX1_VOL_CTL_GAIN,
		0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("DEC2 Volume", TAPAN_A_CDC_TX2_VOL_CTL_GAIN,
		0, -84, 40, digital_gain),

	SOC_SINGLE_SX_TLV("IIR1 INP1 Volume", TAPAN_A_CDC_IIR1_GAIN_B1_CTL,
		0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("IIR1 INP2 Volume", TAPAN_A_CDC_IIR1_GAIN_B2_CTL,
		0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("IIR1 INP3 Volume", TAPAN_A_CDC_IIR1_GAIN_B3_CTL,
		0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("IIR1 INP4 Volume", TAPAN_A_CDC_IIR1_GAIN_B4_CTL,
		0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("IIR2 INP1 Volume", TAPAN_A_CDC_IIR2_GAIN_B1_CTL,
		0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("IIR2 INP2 Volume", TAPAN_A_CDC_IIR2_GAIN_B2_CTL,
		0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("IIR2 INP3 Volume", TAPAN_A_CDC_IIR2_GAIN_B3_CTL,
		0, 84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("IIR2 INP4 Volume", TAPAN_A_CDC_IIR2_GAIN_B4_CTL,
		0, -84, 40, digital_gain),

	SOC_ENUM("TX1 HPF cut off", cf_dec1_enum),
	SOC_ENUM("TX2 HPF cut off", cf_dec2_enum),
	SOC_ENUM("TX3 HPF cut off", cf_dec3_enum),
	SOC_ENUM("TX4 HPF cut off", cf_dec4_enum),

	SOC_SINGLE("TX1 HPF Switch", TAPAN_A_CDC_TX1_MUX_CTL, 3, 1, 0),
	SOC_SINGLE("TX2 HPF Switch", TAPAN_A_CDC_TX2_MUX_CTL, 3, 1, 0),
	SOC_SINGLE("TX3 HPF Switch", TAPAN_A_CDC_TX3_MUX_CTL, 3, 1, 0),
	SOC_SINGLE("TX4 HPF Switch", TAPAN_A_CDC_TX4_MUX_CTL, 3, 1, 0),

	SOC_SINGLE("RX1 HPF Switch", TAPAN_A_CDC_RX1_B5_CTL, 2, 1, 0),
	SOC_SINGLE("RX2 HPF Switch", TAPAN_A_CDC_RX2_B5_CTL, 2, 1, 0),
	SOC_SINGLE("RX3 HPF Switch", TAPAN_A_CDC_RX3_B5_CTL, 2, 1, 0),

	SOC_ENUM("RX1 HPF cut off", cf_rxmix1_enum),
	SOC_ENUM("RX2 HPF cut off", cf_rxmix2_enum),
	SOC_ENUM("RX3 HPF cut off", cf_rxmix3_enum),

	SOC_SINGLE_EXT("IIR1 Enable Band1", IIR1, BAND1, 1, 0,
	tapan_get_iir_enable_audio_mixer, tapan_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR1 Enable Band2", IIR1, BAND2, 1, 0,
	tapan_get_iir_enable_audio_mixer, tapan_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR1 Enable Band3", IIR1, BAND3, 1, 0,
	tapan_get_iir_enable_audio_mixer, tapan_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR1 Enable Band4", IIR1, BAND4, 1, 0,
	tapan_get_iir_enable_audio_mixer, tapan_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR1 Enable Band5", IIR1, BAND5, 1, 0,
	tapan_get_iir_enable_audio_mixer, tapan_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR2 Enable Band1", IIR2, BAND1, 1, 0,
	tapan_get_iir_enable_audio_mixer, tapan_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR2 Enable Band2", IIR2, BAND2, 1, 0,
	tapan_get_iir_enable_audio_mixer, tapan_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR2 Enable Band3", IIR2, BAND3, 1, 0,
	tapan_get_iir_enable_audio_mixer, tapan_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR2 Enable Band4", IIR2, BAND4, 1, 0,
	tapan_get_iir_enable_audio_mixer, tapan_put_iir_enable_audio_mixer),
	SOC_SINGLE_EXT("IIR2 Enable Band5", IIR2, BAND5, 1, 0,
	tapan_get_iir_enable_audio_mixer, tapan_put_iir_enable_audio_mixer),

	SOC_SINGLE_MULTI_EXT("IIR1 Band1", IIR1, BAND1, 255, 0, 5,
	tapan_get_iir_band_audio_mixer, tapan_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR1 Band2", IIR1, BAND2, 255, 0, 5,
	tapan_get_iir_band_audio_mixer, tapan_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR1 Band3", IIR1, BAND3, 255, 0, 5,
	tapan_get_iir_band_audio_mixer, tapan_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR1 Band4", IIR1, BAND4, 255, 0, 5,
	tapan_get_iir_band_audio_mixer, tapan_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR1 Band5", IIR1, BAND5, 255, 0, 5,
	tapan_get_iir_band_audio_mixer, tapan_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR2 Band1", IIR2, BAND1, 255, 0, 5,
	tapan_get_iir_band_audio_mixer, tapan_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR2 Band2", IIR2, BAND2, 255, 0, 5,
	tapan_get_iir_band_audio_mixer, tapan_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR2 Band3", IIR2, BAND3, 255, 0, 5,
	tapan_get_iir_band_audio_mixer, tapan_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR2 Band4", IIR2, BAND4, 255, 0, 5,
	tapan_get_iir_band_audio_mixer, tapan_put_iir_band_audio_mixer),
	SOC_SINGLE_MULTI_EXT("IIR2 Band5", IIR2, BAND5, 255, 0, 5,
	tapan_get_iir_band_audio_mixer, tapan_put_iir_band_audio_mixer),

	SOC_SINGLE_EXT("HPHL Impedance", 0, 0, UINT_MAX, 0,
		       tapan_hph_impedance_get, NULL),
	SOC_SINGLE_EXT("HPHR Impedance", 0, 1, UINT_MAX, 0,
		       tapan_hph_impedance_get, NULL),
};

static const struct snd_kcontrol_new tapan_9306_snd_controls[] = {
	SOC_SINGLE_TLV("ADC5 Volume", TAPAN_A_TX_5_EN, 2, 19, 0, analog_gain),

	SOC_SINGLE_SX_TLV("RX4 Digital Volume", TAPAN_A_CDC_RX4_VOL_CTL_B2_CTL,
		0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("DEC3 Volume", TAPAN_A_CDC_TX3_VOL_CTL_GAIN,
		0, -84, 40, digital_gain),
	SOC_SINGLE_SX_TLV("DEC4 Volume", TAPAN_A_CDC_TX4_VOL_CTL_GAIN,
		0, -84, 40, digital_gain),
	SOC_SINGLE_EXT("ANC Slot", SND_SOC_NOPM, 0, 100, 0, tapan_get_anc_slot,
		tapan_put_anc_slot),
	SOC_ENUM_EXT("ANC Function", tapan_anc_func_enum, tapan_get_anc_func,
		tapan_put_anc_func),
	SOC_SINGLE("RX4 HPF Switch", TAPAN_A_CDC_RX4_B5_CTL, 2, 1, 0),
	SOC_ENUM("RX4 HPF cut off", cf_rxmix4_enum),

	SOC_SINGLE_EXT("COMP0 Switch", SND_SOC_NOPM, COMPANDER_0, 1, 0,
		       tapan_get_compander, tapan_set_compander),
	SOC_SINGLE_EXT("COMP1 Switch", SND_SOC_NOPM, COMPANDER_1, 1, 0,
		       tapan_get_compander, tapan_set_compander),
	SOC_SINGLE_EXT("COMP2 Switch", SND_SOC_NOPM, COMPANDER_2, 1, 0,
		       tapan_get_compander, tapan_set_compander),
};

static const char * const rx_1_2_mix1_text[] = {
	"ZERO", "SRC1", "SRC2", "IIR1", "IIR2", "RX1", "RX2", "RX3", "RX4",
		"RX5", "AUXRX", "AUXTX1"
};

static const char * const rx_3_4_mix1_text[] = {
	"ZERO", "SRC1", "SRC2", "IIR1", "IIR2", "RX1", "RX2", "RX3", "RX4",
		"RX5", "AUXRX", "AUXTX1", "AUXTX2"
};

static const char * const rx_mix2_text[] = {
	"ZERO", "SRC1", "SRC2", "IIR1", "IIR2"
};

static const char * const rx_rdac3_text[] = {
	"DEM1", "DEM2"
};

static const char * const rx_rdac4_text[] = {
	"DEM3", "DEM2"
};

static const char * const rx_rdac5_text[] = {
	"DEM4", "DEM3_INV"
};

static const char * const sb_tx_1_2_mux_text[] = {
	"ZERO", "RMIX1", "RMIX2", "RMIX3", "RMIX4",
	"RSVD", "RSVD", "RSVD",
	"DEC1", "DEC2", "DEC3", "DEC4"
};

static const char * const sb_tx3_mux_text[] = {
	"ZERO", "RMIX1", "RMIX2", "RMIX3", "RMIX4",
	"RSVD", "RSVD", "RSVD", "RSVD", "RSVD",
	"DEC3"
};

static const char * const sb_tx4_mux_text[] = {
	"ZERO", "RMIX1", "RMIX2", "RMIX3", "RMIX4",
	"RSVD", "RSVD", "RSVD", "RSVD", "RSVD", "RSVD",
	"DEC4"
};

static const char * const sb_tx5_mux_text[] = {
	"ZERO", "RMIX1", "RMIX2", "RMIX3", "RMIX4",
	"RSVD", "RSVD", "RSVD",
	"DEC1"
};

static const char * const dec_1_2_mux_text[] = {
	"ZERO", "ADC1", "ADC2", "ADC3", "ADC4",  "ADCMB",
	"DMIC1", "DMIC2", "DMIC3", "DMIC4"
};

static const char * const dec3_mux_text[] = {
	"ZERO", "ADC1", "ADC2", "ADC3", "ADC4", "ADC5", "ADCMB",
	"DMIC1", "DMIC2", "DMIC3", "DMIC4",
	"ANCFBTUNE1"
};

static const char * const dec4_mux_text[] = {
	"ZERO", "ADC1", "ADC2", "ADC3", "ADC4", "ADC5", "ADCMB",
	"DMIC1", "DMIC2", "DMIC3", "DMIC4",
	"ANCFBTUNE2"
};

static const char * const anc_mux_text[] = {
	"ZERO", "ADC1", "ADC2", "ADC3", "ADC4", "ADC5",
	"RSVD", "RSVD", "RSVD",
	"DMIC1", "DMIC2", "DMIC3", "DMIC4",
	"RSVD", "RSVD"
};

static const char * const anc1_fb_mux_text[] = {
	"ZERO", "EAR_HPH_L", "EAR_LINE_1",
};

static const char * const iir_inp_text[] = {
	"ZERO", "DEC1", "DEC2", "DEC3", "DEC4",
	"RX1", "RX2", "RX3", "RX4", "RX5"
};

static const struct soc_enum rx_mix1_inp1_chain_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_CONN_RX1_B1_CTL, 0, 12, rx_1_2_mix1_text);

static const struct soc_enum rx_mix1_inp2_chain_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_CONN_RX1_B1_CTL, 4, 12, rx_1_2_mix1_text);

static const struct soc_enum rx_mix1_inp3_chain_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_CONN_RX1_B2_CTL, 0, 12, rx_1_2_mix1_text);

static const struct soc_enum rx2_mix1_inp1_chain_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_CONN_RX2_B1_CTL, 0, 12, rx_1_2_mix1_text);

static const struct soc_enum rx2_mix1_inp2_chain_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_CONN_RX2_B1_CTL, 4, 12, rx_1_2_mix1_text);

static const struct soc_enum rx3_mix1_inp1_chain_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_CONN_RX3_B1_CTL, 0, 13, rx_3_4_mix1_text);

static const struct soc_enum rx3_mix1_inp2_chain_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_CONN_RX3_B1_CTL, 4, 13, rx_3_4_mix1_text);

static const struct soc_enum rx3_mix1_inp3_chain_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_CONN_RX3_B2_CTL, 0, 13, rx_3_4_mix1_text);

static const struct soc_enum rx4_mix1_inp1_chain_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_CONN_RX4_B1_CTL, 0, 13, rx_3_4_mix1_text);

static const struct soc_enum rx4_mix1_inp2_chain_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_CONN_RX4_B1_CTL, 4, 13, rx_3_4_mix1_text);

static const struct soc_enum rx4_mix1_inp3_chain_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_CONN_RX4_B2_CTL, 0, 13, rx_3_4_mix1_text);

static const struct soc_enum rx1_mix2_inp1_chain_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_CONN_RX1_B3_CTL, 0, 5, rx_mix2_text);

static const struct soc_enum rx1_mix2_inp2_chain_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_CONN_RX1_B3_CTL, 3, 5, rx_mix2_text);

static const struct soc_enum rx2_mix2_inp1_chain_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_CONN_RX2_B3_CTL, 0, 5, rx_mix2_text);

static const struct soc_enum rx2_mix2_inp2_chain_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_CONN_RX2_B3_CTL, 3, 5, rx_mix2_text);

static const struct soc_enum rx4_mix2_inp1_chain_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_CONN_RX4_B3_CTL, 0, 5, rx_mix2_text);

static const struct soc_enum rx4_mix2_inp2_chain_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_CONN_RX4_B3_CTL, 3, 5, rx_mix2_text);

static const struct soc_enum rx_rdac3_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_CONN_RX2_B2_CTL, 4, 2, rx_rdac3_text);

static const struct soc_enum rx_rdac4_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_CONN_MISC, 1, 2, rx_rdac4_text);

static const struct soc_enum rx_rdac5_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_CONN_MISC, 2, 2, rx_rdac5_text);

static const struct soc_enum sb_tx1_mux_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_CONN_TX_SB_B1_CTL, 0, 12,
					sb_tx_1_2_mux_text);

static const struct soc_enum sb_tx2_mux_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_CONN_TX_SB_B2_CTL, 0, 12,
					sb_tx_1_2_mux_text);

static const struct soc_enum sb_tx3_mux_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_CONN_TX_SB_B3_CTL, 0, 11, sb_tx3_mux_text);

static const struct soc_enum sb_tx4_mux_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_CONN_TX_SB_B4_CTL, 0, 12, sb_tx4_mux_text);

static const struct soc_enum sb_tx5_mux_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_CONN_TX_SB_B5_CTL, 0, 9, sb_tx5_mux_text);

static const struct soc_enum dec1_mux_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_CONN_TX_B1_CTL, 0, 10, dec_1_2_mux_text);

static const struct soc_enum dec2_mux_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_CONN_TX_B1_CTL, 4, 10, dec_1_2_mux_text);

static const struct soc_enum dec3_mux_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_CONN_TX_B2_CTL, 0, 12, dec3_mux_text);

static const struct soc_enum dec4_mux_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_CONN_TX_B2_CTL, 4, 12, dec4_mux_text);

static const struct soc_enum anc1_mux_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_CONN_ANC_B1_CTL, 0, 15, anc_mux_text);

static const struct soc_enum anc2_mux_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_CONN_ANC_B1_CTL, 4, 15, anc_mux_text);

static const struct soc_enum anc1_fb_mux_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_CONN_ANC_B2_CTL, 0, 3, anc1_fb_mux_text);

static const struct soc_enum iir1_inp1_mux_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_CONN_EQ1_B1_CTL, 0, 10, iir_inp_text);

static const struct soc_enum iir1_inp2_mux_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_CONN_EQ1_B2_CTL, 0, 10, iir_inp_text);

static const struct soc_enum iir1_inp3_mux_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_CONN_EQ1_B3_CTL, 0, 10, iir_inp_text);

static const struct soc_enum iir1_inp4_mux_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_CONN_EQ1_B4_CTL, 0, 10, iir_inp_text);

static const struct soc_enum iir2_inp1_mux_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_CONN_EQ2_B1_CTL, 0, 10, iir_inp_text);

static const struct soc_enum iir2_inp2_mux_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_CONN_EQ2_B2_CTL, 0, 10, iir_inp_text);

static const struct soc_enum iir2_inp3_mux_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_CONN_EQ2_B3_CTL, 0, 10, iir_inp_text);

static const struct soc_enum iir2_inp4_mux_enum =
	SOC_ENUM_SINGLE(TAPAN_A_CDC_CONN_EQ2_B4_CTL, 0, 10, iir_inp_text);

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

static const struct snd_kcontrol_new rx3_mix1_inp3_mux =
	SOC_DAPM_ENUM("RX3 MIX1 INP3 Mux", rx3_mix1_inp3_chain_enum);

static const struct snd_kcontrol_new rx4_mix1_inp1_mux =
	SOC_DAPM_ENUM("RX4 MIX1 INP1 Mux", rx4_mix1_inp1_chain_enum);

static const struct snd_kcontrol_new rx4_mix1_inp2_mux =
	SOC_DAPM_ENUM("RX4 MIX1 INP2 Mux", rx4_mix1_inp2_chain_enum);

static const struct snd_kcontrol_new rx4_mix1_inp3_mux =
	SOC_DAPM_ENUM("RX4 MIX1 INP3 Mux", rx4_mix1_inp3_chain_enum);

static const struct snd_kcontrol_new rx1_mix2_inp1_mux =
	SOC_DAPM_ENUM("RX1 MIX2 INP1 Mux", rx1_mix2_inp1_chain_enum);

static const struct snd_kcontrol_new rx1_mix2_inp2_mux =
	SOC_DAPM_ENUM("RX1 MIX2 INP2 Mux", rx1_mix2_inp2_chain_enum);

static const struct snd_kcontrol_new rx2_mix2_inp1_mux =
	SOC_DAPM_ENUM("RX2 MIX2 INP1 Mux", rx2_mix2_inp1_chain_enum);

static const struct snd_kcontrol_new rx2_mix2_inp2_mux =
	SOC_DAPM_ENUM("RX2 MIX2 INP2 Mux", rx2_mix2_inp2_chain_enum);

static const struct snd_kcontrol_new rx4_mix2_inp1_mux =
	SOC_DAPM_ENUM("RX4 MIX2 INP1 Mux", rx4_mix2_inp1_chain_enum);

static const struct snd_kcontrol_new rx4_mix2_inp2_mux =
	SOC_DAPM_ENUM("RX4 MIX2 INP2 Mux", rx4_mix2_inp2_chain_enum);

static const struct snd_kcontrol_new rx_dac3_mux =
	SOC_DAPM_ENUM("RDAC3 MUX Mux", rx_rdac3_enum);

static const struct snd_kcontrol_new rx_dac4_mux =
	SOC_DAPM_ENUM("RDAC4 MUX Mux", rx_rdac4_enum);

static const struct snd_kcontrol_new rx_dac5_mux =
	SOC_DAPM_ENUM("RDAC5 MUX Mux", rx_rdac5_enum);

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

static int wcd9306_put_dec_enum(struct snd_kcontrol *kcontrol,
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
	char *srch = NULL;

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
	srch = strpbrk(dec_name, "1234");
	if (srch == NULL) {
		pr_err("%s: Invalid decimator name %s\n", __func__, dec_name);
		return -EINVAL;
	}
	ret = kstrtouint(srch, 10, &decimator);
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
		if ((dec_mux >= 1) && (dec_mux <= 5))
			adc_dmic_sel = 0x0;
		else if ((dec_mux >= 6) && (dec_mux <= 9))
			adc_dmic_sel = 0x1;
		break;
	case 3:
	case 4:
		if ((dec_mux >= 1) && (dec_mux <= 6))
			adc_dmic_sel = 0x0;
		else if ((dec_mux >= 7) && (dec_mux <= 10))
			adc_dmic_sel = 0x1;
		break;
	default:
		pr_err("%s: Invalid Decimator = %u\n", __func__, decimator);
		ret = -EINVAL;
		goto out;
	}

	tx_mux_ctl_reg = TAPAN_A_CDC_TX1_MUX_CTL + 8 * (decimator - 1);

	snd_soc_update_bits(codec, tx_mux_ctl_reg, 0x1, adc_dmic_sel);

	ret = snd_soc_dapm_put_enum_double(kcontrol, ucontrol);

out:
	kfree(widget_name);
	return ret;
}

#define WCD9306_DEC_ENUM(xname, xenum) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
	.info = snd_soc_info_enum_double, \
	.get = snd_soc_dapm_get_enum_double, \
	.put = wcd9306_put_dec_enum, \
	.private_value = (unsigned long)&xenum }

static const struct snd_kcontrol_new dec1_mux =
	WCD9306_DEC_ENUM("DEC1 MUX Mux", dec1_mux_enum);

static const struct snd_kcontrol_new dec2_mux =
	WCD9306_DEC_ENUM("DEC2 MUX Mux", dec2_mux_enum);

static const struct snd_kcontrol_new dec3_mux =
	WCD9306_DEC_ENUM("DEC3 MUX Mux", dec3_mux_enum);

static const struct snd_kcontrol_new dec4_mux =
	WCD9306_DEC_ENUM("DEC4 MUX Mux", dec4_mux_enum);

static const struct snd_kcontrol_new iir1_inp1_mux =
	SOC_DAPM_ENUM("IIR1 INP1 Mux", iir1_inp1_mux_enum);

static const struct snd_kcontrol_new iir1_inp2_mux =
	SOC_DAPM_ENUM("IIR1 INP2 Mux", iir1_inp2_mux_enum);

static const struct snd_kcontrol_new iir1_inp3_mux =
	SOC_DAPM_ENUM("IIR1 INP3 Mux", iir1_inp3_mux_enum);

static const struct snd_kcontrol_new iir1_inp4_mux =
	SOC_DAPM_ENUM("IIR1 INP4 Mux", iir1_inp4_mux_enum);

static const struct snd_kcontrol_new iir2_inp1_mux =
	SOC_DAPM_ENUM("IIR2 INP1 Mux", iir2_inp1_mux_enum);

static const struct snd_kcontrol_new iir2_inp2_mux =
	SOC_DAPM_ENUM("IIR2 INP2 Mux", iir2_inp2_mux_enum);

static const struct snd_kcontrol_new iir2_inp3_mux =
	SOC_DAPM_ENUM("IIR2 INP3 Mux", iir2_inp3_mux_enum);

static const struct snd_kcontrol_new iir2_inp4_mux =
	SOC_DAPM_ENUM("IIR2 INP4 Mux", iir2_inp4_mux_enum);

static const struct snd_kcontrol_new anc1_mux =
	SOC_DAPM_ENUM("ANC1 MUX Mux", anc1_mux_enum);

static const struct snd_kcontrol_new anc2_mux =
	SOC_DAPM_ENUM("ANC2 MUX Mux", anc2_mux_enum);

static const struct snd_kcontrol_new anc1_fb_mux =
	SOC_DAPM_ENUM("ANC1 FB MUX Mux", anc1_fb_mux_enum);

static const struct snd_kcontrol_new dac1_switch[] = {
	SOC_DAPM_SINGLE("Switch", TAPAN_A_RX_EAR_EN, 5, 1, 0)
};
static const struct snd_kcontrol_new hphl_switch[] = {
	SOC_DAPM_SINGLE("Switch", TAPAN_A_RX_HPH_L_DAC_CTL, 6, 1, 0)
};

static const struct snd_kcontrol_new spk_dac_switch[] = {
	SOC_DAPM_SINGLE("Switch", TAPAN_A_SPKR_DRV_DAC_CTL, 2, 1, 0)
};

static const struct snd_kcontrol_new hphl_pa_mix[] = {
	SOC_DAPM_SINGLE("AUX_PGA_L Switch", TAPAN_A_RX_PA_AUX_IN_CONN,
					7, 1, 0),
};

static const struct snd_kcontrol_new hphr_pa_mix[] = {
	SOC_DAPM_SINGLE("AUX_PGA_R Switch", TAPAN_A_RX_PA_AUX_IN_CONN,
					6, 1, 0),
};

static const struct snd_kcontrol_new ear_pa_mix[] = {
	SOC_DAPM_SINGLE("AUX_PGA_L Switch", TAPAN_A_RX_PA_AUX_IN_CONN,
					5, 1, 0),
};
static const struct snd_kcontrol_new lineout1_pa_mix[] = {
	SOC_DAPM_SINGLE("AUX_PGA_L Switch", TAPAN_A_RX_PA_AUX_IN_CONN,
					4, 1, 0),
};

static const struct snd_kcontrol_new lineout2_pa_mix[] = {
	SOC_DAPM_SINGLE("AUX_PGA_R Switch", TAPAN_A_RX_PA_AUX_IN_CONN,
					3, 1, 0),
};


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
	struct tapan_priv *tapan_p = snd_soc_codec_get_drvdata(codec);
	struct wcd9xxx *core = dev_get_drvdata(codec->dev->parent);
	struct soc_multi_mixer_control *mixer =
		((struct soc_multi_mixer_control *)kcontrol->private_value);
	u32 dai_id = widget->shift;
	u32 port_id = mixer->shift;
	u32 enable = ucontrol->value.integer.value[0];
	u32 vtable = vport_check_table[dai_id];

	dev_dbg(codec->dev, "%s: wname %s cname %s\n",
		__func__, widget->name,	ucontrol->id.name);
	dev_dbg(codec->dev, "%s: value %u shift %d item %ld\n",
		__func__, widget->value, widget->shift,
		ucontrol->value.integer.value[0]);

	mutex_lock(&codec->mutex);

	if (tapan_p->intf_type != WCD9XXX_INTERFACE_TYPE_SLIMBUS) {
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
			if (tapan_p->intf_type ==
					WCD9XXX_INTERFACE_TYPE_SLIMBUS)
				vtable = vport_check_table[dai_id];
			if (tapan_p->intf_type ==
					WCD9XXX_INTERFACE_TYPE_I2C)
				vtable = vport_i2s_check_table[dai_id];

			if (wcd9xxx_tx_vport_validation(
						vtable,
						port_id,
						tapan_p->dai, NUM_CODEC_DAIS)) {
				dev_dbg(codec->dev, "%s: TX%u is used by other virtual port\n",
					__func__, port_id + 1);
				mutex_unlock(&codec->mutex);
				return 0;
			}
			widget->value |= 1 << port_id;
			list_add_tail(&core->tx_chs[port_id].list,
				      &tapan_p->dai[dai_id].wcd9xxx_ch_list
					      );
		} else if (!enable && (widget->value & 1 << port_id)) {
			widget->value &= ~(1 << port_id);
			list_del_init(&core->tx_chs[port_id].list);
		} else {
			if (enable)
				dev_dbg(codec->dev, "%s: TX%u port is used by\n"
					"this virtual port\n",
					__func__, port_id + 1);
			else
				dev_dbg(codec->dev, "%s: TX%u port is not used by\n"
					"this virtual port\n",
					__func__, port_id + 1);
			/* avoid update power function */
			mutex_unlock(&codec->mutex);
			return 0;
		}
		break;
	default:
		dev_err(codec->dev, "Unknown AIF %d\n", dai_id);
		mutex_unlock(&codec->mutex);
		return -EINVAL;
	}
	dev_dbg(codec->dev, "%s: name %s sname %s updated value %u shift %d\n",
		 __func__, widget->name, widget->sname,
		 widget->value, widget->shift);

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
	struct tapan_priv *tapan_p = snd_soc_codec_get_drvdata(codec);
	struct wcd9xxx *core = dev_get_drvdata(codec->dev->parent);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	u32 port_id = widget->shift;

	dev_dbg(codec->dev, "%s: wname %s cname %s value %u shift %d item %ld\n",
		 __func__, widget->name, ucontrol->id.name, widget->value,
		 widget->shift, ucontrol->value.integer.value[0]);

	widget->value = ucontrol->value.enumerated.item[0];

	mutex_lock(&codec->mutex);

	if (tapan_p->intf_type != WCD9XXX_INTERFACE_TYPE_SLIMBUS) {
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
			TAPAN_RX_PORT_START_NUMBER,
			&tapan_p->dai[AIF1_PB].wcd9xxx_ch_list)) {
			dev_dbg(codec->dev, "%s: RX%u is used by current requesting AIF_PB itself\n",
				__func__, port_id + 1);
			goto rtn;
		}
		list_add_tail(&core->rx_chs[port_id].list,
			      &tapan_p->dai[AIF1_PB].wcd9xxx_ch_list);
	break;
	case 2:
		if (wcd9xxx_rx_vport_validation(port_id +
			TAPAN_RX_PORT_START_NUMBER,
			&tapan_p->dai[AIF2_PB].wcd9xxx_ch_list)) {
				dev_dbg(codec->dev, "%s: RX%u is used by current requesting AIF_PB itself\n",
					__func__, port_id + 1);
				goto rtn;
			}
		list_add_tail(&core->rx_chs[port_id].list,
			      &tapan_p->dai[AIF2_PB].wcd9xxx_ch_list);
	break;
	case 3:
		if (wcd9xxx_rx_vport_validation(port_id +
			TAPAN_RX_PORT_START_NUMBER,
			&tapan_p->dai[AIF3_PB].wcd9xxx_ch_list)) {
				dev_dbg(codec->dev, "%s: RX%u is used by current requesting AIF_PB itself\n",
					__func__, port_id + 1);
				goto rtn;
			}
		list_add_tail(&core->rx_chs[port_id].list,
			      &tapan_p->dai[AIF3_PB].wcd9xxx_ch_list);
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

static const struct snd_kcontrol_new slim_rx_mux[TAPAN_RX_MAX] = {
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
};

static const struct snd_kcontrol_new aif1_cap_mixer[] = {
	SOC_SINGLE_EXT("SLIM TX1", SND_SOC_NOPM, TAPAN_TX1, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX2", SND_SOC_NOPM, TAPAN_TX2, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX3", SND_SOC_NOPM, TAPAN_TX3, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX4", SND_SOC_NOPM, TAPAN_TX4, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX5", SND_SOC_NOPM, TAPAN_TX5, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
};

static const struct snd_kcontrol_new aif2_cap_mixer[] = {
	SOC_SINGLE_EXT("SLIM TX1", SND_SOC_NOPM, TAPAN_TX1, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX2", SND_SOC_NOPM, TAPAN_TX2, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX3", SND_SOC_NOPM, TAPAN_TX3, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX4", SND_SOC_NOPM, TAPAN_TX4, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX5", SND_SOC_NOPM, TAPAN_TX5, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
};

static const struct snd_kcontrol_new aif3_cap_mixer[] = {
	SOC_SINGLE_EXT("SLIM TX1", SND_SOC_NOPM, TAPAN_TX1, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX2", SND_SOC_NOPM, TAPAN_TX2, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX3", SND_SOC_NOPM, TAPAN_TX3, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX4", SND_SOC_NOPM, TAPAN_TX4, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
	SOC_SINGLE_EXT("SLIM TX5", SND_SOC_NOPM, TAPAN_TX5, 1, 0,
			slim_tx_mixer_get, slim_tx_mixer_put),
};

static int tapan_codec_enable_adc(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct tapan_priv *tapan = snd_soc_codec_get_drvdata(codec);
	u16 adc_reg;
	u8 init_bit_shift;

	dev_dbg(codec->dev, "%s(): %s %d\n", __func__, w->name, event);

	if (w->reg == TAPAN_A_TX_1_EN) {
		init_bit_shift = 7;
		adc_reg = TAPAN_A_TX_1_2_TEST_CTL;
	} else if (w->reg == TAPAN_A_TX_2_EN) {
		init_bit_shift = 6;
		adc_reg = TAPAN_A_TX_1_2_TEST_CTL;
	} else if (w->reg == TAPAN_A_TX_3_EN) {
		init_bit_shift = 6;
		adc_reg = TAPAN_A_TX_1_2_TEST_CTL;
	} else if (w->reg == TAPAN_A_TX_4_EN) {
		init_bit_shift = 7;
		adc_reg = TAPAN_A_TX_4_5_TEST_CTL;
	} else if (w->reg == TAPAN_A_TX_5_EN) {
		init_bit_shift = 6;
		adc_reg = TAPAN_A_TX_4_5_TEST_CTL;
	} else {
		pr_err("%s: Error, invalid adc register\n", __func__);
		return -EINVAL;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (w->reg == TAPAN_A_TX_3_EN ||
		    w->reg == TAPAN_A_TX_1_EN)
			wcd9xxx_resmgr_notifier_call(&tapan->resmgr,
						WCD9XXX_EVENT_PRE_TX_1_3_ON);
		snd_soc_update_bits(codec, adc_reg, 1 << init_bit_shift,
				1 << init_bit_shift);
		break;
	case SND_SOC_DAPM_POST_PMU:
		usleep_range(2000, 2010);
		snd_soc_update_bits(codec, adc_reg, 1 << init_bit_shift, 0x00);

		break;
	case SND_SOC_DAPM_POST_PMD:
		if (w->reg == TAPAN_A_TX_3_EN ||
		    w->reg == TAPAN_A_TX_1_EN)
			wcd9xxx_resmgr_notifier_call(&tapan->resmgr,
						WCD9XXX_EVENT_POST_TX_1_3_OFF);
		break;
	}
	return 0;
}

static int tapan_codec_enable_aux_pga(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct tapan_priv *tapan = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s: %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		WCD9XXX_BG_CLK_LOCK(&tapan->resmgr);
		wcd9xxx_resmgr_get_bandgap(&tapan->resmgr,
					   WCD9XXX_BANDGAP_AUDIO_MODE);
		/* AUX PGA requires RCO or MCLK */
		wcd9xxx_resmgr_get_clk_block(&tapan->resmgr, WCD9XXX_CLK_RCO);
		WCD9XXX_BG_CLK_UNLOCK(&tapan->resmgr);
		wcd9xxx_resmgr_enable_rx_bias(&tapan->resmgr, 1);
		break;

	case SND_SOC_DAPM_POST_PMD:
		wcd9xxx_resmgr_enable_rx_bias(&tapan->resmgr, 0);
		WCD9XXX_BG_CLK_LOCK(&tapan->resmgr);
		wcd9xxx_resmgr_put_bandgap(&tapan->resmgr,
					   WCD9XXX_BANDGAP_AUDIO_MODE);
		wcd9xxx_resmgr_put_clk_block(&tapan->resmgr, WCD9XXX_CLK_RCO);
		WCD9XXX_BG_CLK_UNLOCK(&tapan->resmgr);
		break;
	}
	return 0;
}

static int tapan_codec_enable_lineout(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct tapan_priv *tapan = snd_soc_codec_get_drvdata(codec);
	u16 lineout_gain_reg;

	dev_dbg(codec->dev, "%s %d %s\n", __func__, event, w->name);

	switch (w->shift) {
	case 0:
		lineout_gain_reg = TAPAN_A_RX_LINE_1_GAIN;
		break;
	case 1:
		lineout_gain_reg = TAPAN_A_RX_LINE_2_GAIN;
		break;
	default:
		pr_err("%s: Error, incorrect lineout register value\n",
			__func__);
		return -EINVAL;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		break;
	case SND_SOC_DAPM_POST_PMU:
		wcd9xxx_clsh_fsm(codec, &tapan->clsh_d,
						 WCD9XXX_CLSH_STATE_LO,
						 WCD9XXX_CLSH_REQ_ENABLE,
						 WCD9XXX_CLSH_EVENT_POST_PA);
		dev_dbg(codec->dev, "%s: sleeping 3 ms after %s PA turn on\n",
				__func__, w->name);
		usleep_range(3000, 3010);
		break;
	case SND_SOC_DAPM_POST_PMD:
		wcd9xxx_clsh_fsm(codec, &tapan->clsh_d,
						 WCD9XXX_CLSH_STATE_LO,
						 WCD9XXX_CLSH_REQ_DISABLE,
						 WCD9XXX_CLSH_EVENT_POST_PA);
		break;
	}
	return 0;
}

static int tapan_codec_enable_spk_pa(struct snd_soc_dapm_widget *w,
				     struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct tapan_priv *tapan = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s: %s %d\n", __func__, w->name, event);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		tapan->spkr_pa_widget_on = true;
		snd_soc_update_bits(codec, TAPAN_A_SPKR_DRV_EN, 0x80, 0x80);
		break;
	case SND_SOC_DAPM_POST_PMD:
		tapan->spkr_pa_widget_on = false;
		snd_soc_update_bits(codec, TAPAN_A_SPKR_DRV_EN, 0x80, 0x00);
		break;
	}
	return 0;
}

static int tapan_codec_enable_dmic(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct tapan_priv *tapan = snd_soc_codec_get_drvdata(codec);
	u8  dmic_clk_en;
	u16 dmic_clk_reg;
	s32 *dmic_clk_cnt;
	unsigned int dmic;
	int ret;
	char *srch = NULL;

	srch = strpbrk(w->name, "1234");
	if (srch == NULL) {
		pr_err("%s: Invalid widget name %s\n", __func__, w->name);
		return -EINVAL;
	}

	ret = kstrtouint(srch, 10, &dmic);
	if (ret < 0) {
		pr_err("%s: Invalid DMIC line on the codec\n", __func__);
		return -EINVAL;
	}

	switch (dmic) {
	case 1:
	case 2:
		dmic_clk_en = 0x01;
		dmic_clk_cnt = &(tapan->dmic_1_2_clk_cnt);
		dmic_clk_reg = TAPAN_A_CDC_CLK_DMIC_B1_CTL;
		dev_dbg(codec->dev, "%s() event %d DMIC%d dmic_1_2_clk_cnt %d\n",
			__func__, event,  dmic, *dmic_clk_cnt);

		break;

	case 3:
	case 4:
		dmic_clk_en = 0x10;
		dmic_clk_cnt = &(tapan->dmic_3_4_clk_cnt);
		dmic_clk_reg = TAPAN_A_CDC_CLK_DMIC_B1_CTL;

		dev_dbg(codec->dev, "%s() event %d DMIC%d dmic_3_4_clk_cnt %d\n",
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

static int tapan_codec_enable_anc(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	const char *filename;
	const struct firmware *fw;
	int i;
	int ret = 0;
	int num_anc_slots;
	struct wcd9xxx_anc_header *anc_head;
	struct tapan_priv *tapan = snd_soc_codec_get_drvdata(codec);
	struct firmware_cal *hwdep_cal = NULL;
	u32 anc_writes_size = 0;
	int anc_size_remaining;
	u32 *anc_ptr;
	u16 reg;
	u8 mask, val, old_val;
	size_t cal_size;
	const void *data;

	dev_dbg(codec->dev, "%s %d\n", __func__, event);
	if (tapan->anc_func == 0)
		return 0;
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:

		filename = "wcd9306/wcd9306_anc.bin";

		hwdep_cal = wcdcal_get_fw_cal(tapan->fw_data, WCD9XXX_ANC_CAL);
		if (hwdep_cal) {
			data = hwdep_cal->data;
			cal_size = hwdep_cal->size;
			dev_dbg(codec->dev, "%s: using hwdep calibration\n",
				__func__);
		} else {
			ret = request_firmware(&fw, filename, codec->dev);
			if (ret != 0) {
				dev_err(codec->dev, "Failed to acquire ANC data: %d\n",
					ret);
				return -ENODEV;
			}
			if (!fw) {
				dev_err(codec->dev, "failed to get anc fw");
				return -ENODEV;
			}
			data = fw->data;
			cal_size = fw->size;
			dev_dbg(codec->dev, "%s: using request_firmware calibration\n",
				__func__);
		}
		if (cal_size < sizeof(struct wcd9xxx_anc_header)) {
			dev_err(codec->dev, "Not enough data\n");
			ret = -ENOMEM;
			goto err;
		}

		/* First number is the number of register writes */
		anc_head = (struct wcd9xxx_anc_header *)(data);
		anc_ptr = (u32 *)(data +
				  sizeof(struct wcd9xxx_anc_header));
		anc_size_remaining = cal_size -
				     sizeof(struct wcd9xxx_anc_header);
		num_anc_slots = anc_head->num_anc_slots;

		if (tapan->anc_slot >= num_anc_slots) {
			dev_err(codec->dev, "Invalid ANC slot selected\n");
			ret = -EINVAL;
			goto err;
		}

		for (i = 0; i < num_anc_slots; i++) {

			if (anc_size_remaining < TAPAN_PACKED_REG_SIZE) {
				dev_err(codec->dev, "Invalid register format\n");
				ret = -EINVAL;
				goto err;
			}
			anc_writes_size = (u32)(*anc_ptr);
			anc_size_remaining -= sizeof(u32);
			anc_ptr += 1;

			if (anc_writes_size * TAPAN_PACKED_REG_SIZE
				> anc_size_remaining) {
				dev_err(codec->dev, "Invalid register format\n");
				ret = -EINVAL;
				goto err;
			}

			if (tapan->anc_slot == i)
				break;

			anc_size_remaining -= (anc_writes_size *
				TAPAN_PACKED_REG_SIZE);
			anc_ptr += anc_writes_size;
		}
		if (i == num_anc_slots) {
			dev_err(codec->dev, "Selected ANC slot not present\n");
			ret = -EINVAL;
			goto err;
		}

		for (i = 0; i < anc_writes_size; i++) {
			TAPAN_CODEC_UNPACK_ENTRY(anc_ptr[i], reg,
				mask, val);
			old_val = snd_soc_read(codec, reg);
			snd_soc_write(codec, reg, (old_val & ~mask) |
					(val & mask));
		}
		if (!hwdep_cal)
			release_firmware(fw);

		break;
	case SND_SOC_DAPM_PRE_PMD:
		msleep(40);
		snd_soc_update_bits(codec, TAPAN_A_CDC_ANC1_B1_CTL, 0x01, 0x00);
		snd_soc_update_bits(codec, TAPAN_A_CDC_ANC2_B1_CTL, 0x02, 0x00);
		msleep(20);
		snd_soc_write(codec, TAPAN_A_CDC_CLK_ANC_RESET_CTL, 0x0F);
		snd_soc_write(codec, TAPAN_A_CDC_CLK_ANC_CLK_EN_CTL, 0);
		snd_soc_write(codec, TAPAN_A_CDC_CLK_ANC_RESET_CTL, 0xFF);
		break;
	}
	return 0;
err:
	if (!hwdep_cal)
		release_firmware(fw);
	return ret;
}

static int tapan_codec_enable_micbias(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct tapan_priv *tapan = snd_soc_codec_get_drvdata(codec);
	u16 micb_int_reg = 0, micb_ctl_reg = 0;
	u8 cfilt_sel_val = 0;
	char *internal1_text = "Internal1";
	char *internal2_text = "Internal2";
	char *internal3_text = "Internal3";
	enum wcd9xxx_notify_event e_post_off, e_pre_on, e_post_on;

	pr_debug("%s: w->name %s event %d\n", __func__, w->name, event);
	if (strnstr(w->name, "MIC BIAS1", sizeof("MIC BIAS1"))) {
		micb_ctl_reg = TAPAN_A_MICB_1_CTL;
		micb_int_reg = TAPAN_A_MICB_1_INT_RBIAS;
		cfilt_sel_val = tapan->resmgr.pdata->micbias.bias1_cfilt_sel;
		e_pre_on = WCD9XXX_EVENT_PRE_MICBIAS_1_ON;
		e_post_on = WCD9XXX_EVENT_POST_MICBIAS_1_ON;
		e_post_off = WCD9XXX_EVENT_POST_MICBIAS_1_OFF;
	} else if (strnstr(w->name, "MIC BIAS2", sizeof("MIC BIAS2"))) {
		micb_ctl_reg = TAPAN_A_MICB_2_CTL;
		micb_int_reg = TAPAN_A_MICB_2_INT_RBIAS;
		cfilt_sel_val = tapan->resmgr.pdata->micbias.bias2_cfilt_sel;
		e_pre_on = WCD9XXX_EVENT_PRE_MICBIAS_2_ON;
		e_post_on = WCD9XXX_EVENT_POST_MICBIAS_2_ON;
		e_post_off = WCD9XXX_EVENT_POST_MICBIAS_2_OFF;
	} else if (strnstr(w->name, "MIC BIAS3", sizeof("MIC BIAS3"))) {
		micb_ctl_reg = TAPAN_A_MICB_3_CTL;
		micb_int_reg = TAPAN_A_MICB_3_INT_RBIAS;
		cfilt_sel_val = tapan->resmgr.pdata->micbias.bias3_cfilt_sel;
		e_pre_on = WCD9XXX_EVENT_PRE_MICBIAS_3_ON;
		e_post_on = WCD9XXX_EVENT_POST_MICBIAS_3_ON;
		e_post_off = WCD9XXX_EVENT_POST_MICBIAS_3_OFF;
	} else {
		pr_err("%s: Error, invalid micbias %s\n", __func__, w->name);
		return -EINVAL;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Let MBHC module know so micbias switch to be off */
		wcd9xxx_resmgr_notifier_call(&tapan->resmgr, e_pre_on);

		/* Get cfilt */
		wcd9xxx_resmgr_cfilt_get(&tapan->resmgr, cfilt_sel_val);

		if (strnstr(w->name, internal1_text, 30))
			snd_soc_update_bits(codec, micb_int_reg, 0xE0, 0xE0);
		else if (strnstr(w->name, internal2_text, 30))
			snd_soc_update_bits(codec, micb_int_reg, 0x1C, 0x1C);
		else if (strnstr(w->name, internal3_text, 30))
			snd_soc_update_bits(codec, micb_int_reg, 0x3, 0x3);

		if (micb_ctl_reg == TAPAN_A_MICB_2_CTL) {
			if (++tapan->micb_2_users == 1)
				wcd9xxx_resmgr_add_cond_update_bits(
						&tapan->resmgr,
						WCD9XXX_COND_HPH_MIC,
						micb_ctl_reg, w->shift,
						false);
			pr_debug("%s: micb_2_users %d\n", __func__,
				 tapan->micb_2_users);
		} else
			snd_soc_update_bits(codec, micb_ctl_reg, 1 << w->shift,
						1 << w->shift);


		break;
	case SND_SOC_DAPM_POST_PMU:
		usleep_range(20000, 20000);
		/* Let MBHC module know so micbias is on */
		wcd9xxx_resmgr_notifier_call(&tapan->resmgr, e_post_on);
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (micb_ctl_reg == TAPAN_A_MICB_2_CTL) {
			if (--tapan->micb_2_users == 0)
				wcd9xxx_resmgr_rm_cond_update_bits(
						&tapan->resmgr,
						WCD9XXX_COND_HPH_MIC,
						micb_ctl_reg, 7,
						false);
			pr_debug("%s: micb_2_users %d\n", __func__,
				 tapan->micb_2_users);
			WARN(tapan->micb_2_users < 0,
				"Unexpected micbias users %d\n",
				tapan->micb_2_users);
		} else
			snd_soc_update_bits(codec, micb_ctl_reg, 1 << w->shift,
					    0);

		/* Let MBHC module know so micbias switch to be off */
		wcd9xxx_resmgr_notifier_call(&tapan->resmgr, e_post_off);

		if (strnstr(w->name, internal1_text, 30))
			snd_soc_update_bits(codec, micb_int_reg, 0x80, 0x00);
		else if (strnstr(w->name, internal2_text, 30))
			snd_soc_update_bits(codec, micb_int_reg, 0x10, 0x00);
		else if (strnstr(w->name, internal3_text, 30))
			snd_soc_update_bits(codec, micb_int_reg, 0x2, 0x0);

		/* Put cfilt */
		wcd9xxx_resmgr_cfilt_put(&tapan->resmgr, cfilt_sel_val);
		break;
	}

	return 0;
}

/* called under codec_resource_lock acquisition */
static int tapan_enable_mbhc_micbias(struct snd_soc_codec *codec, bool enable,
				     enum wcd9xxx_micbias_num micb_num)
{
	int rc;
	const char *micbias;

	if (micb_num == MBHC_MICBIAS2)
		micbias = DAPM_MICBIAS2_EXTERNAL_STANDALONE;
	else
		return -EINVAL;

	if (enable)
		rc = snd_soc_dapm_force_enable_pin(&codec->dapm,
						   micbias);
	else
		rc = snd_soc_dapm_disable_pin(&codec->dapm,
					      micbias);
	if (!rc)
		snd_soc_dapm_sync(&codec->dapm);
	pr_debug("%s: leave ret %d\n", __func__, rc);
	return rc;
}

static void tx_hpf_corner_freq_callback(struct work_struct *work)
{
	struct delayed_work *hpf_delayed_work;
	struct hpf_work *hpf_work;
	struct tapan_priv *tapan;
	struct snd_soc_codec *codec;
	u16 tx_mux_ctl_reg;
	u8 hpf_cut_of_freq;

	hpf_delayed_work = to_delayed_work(work);
	hpf_work = container_of(hpf_delayed_work, struct hpf_work, dwork);
	tapan = hpf_work->tapan;
	codec = hpf_work->tapan->codec;
	hpf_cut_of_freq = hpf_work->tx_hpf_cut_of_freq;

	tx_mux_ctl_reg = TAPAN_A_CDC_TX1_MUX_CTL +
			(hpf_work->decimator - 1) * 8;

	dev_dbg(codec->dev, "%s(): decimator %u hpf_cut_of_freq 0x%x\n",
		 __func__, hpf_work->decimator, (unsigned int)hpf_cut_of_freq);

	snd_soc_update_bits(codec, TAPAN_A_TX_1_2_TXFE_CLKDIV, 0x55, 0x55);
	snd_soc_update_bits(codec, tx_mux_ctl_reg, 0x30, hpf_cut_of_freq << 4);
}

#define  TX_MUX_CTL_CUT_OFF_FREQ_MASK	0x30
#define  CF_MIN_3DB_4HZ			0x0
#define  CF_MIN_3DB_75HZ		0x1
#define  CF_MIN_3DB_150HZ		0x2

static int tapan_codec_enable_dec(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	unsigned int decimator;
	struct tapan_priv *tapan_p = snd_soc_codec_get_drvdata(codec);
	char *dec_name = NULL;
	char *widget_name = NULL;
	char *temp;
	int ret = 0, i;
	u16 dec_reset_reg, tx_vol_ctl_reg, tx_mux_ctl_reg;
	u8 dec_hpf_cut_of_freq;
	int offset;
	char *srch = NULL;

	dev_dbg(codec->dev, "%s %d\n", __func__, event);

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
	srch = strpbrk(dec_name, "123456789");
	if (srch == NULL) {
		pr_err("%s: Invalid decimator name %s\n", __func__, dec_name);
		return -EINVAL;
	}
	ret = kstrtouint(srch, 10, &decimator);
	if (ret < 0) {
		pr_err("%s: Invalid decimator = %s\n", __func__, dec_name);
		ret =  -EINVAL;
		goto out;
	}

	dev_dbg(codec->dev, "%s(): widget = %s dec_name = %s decimator = %u\n",
			__func__, w->name, dec_name, decimator);

	if (w->reg == TAPAN_A_CDC_CLK_TX_CLK_EN_B1_CTL) {
		dec_reset_reg = TAPAN_A_CDC_CLK_TX_RESET_B1_CTL;
		offset = 0;
	} else if (w->reg == TAPAN_A_CDC_CLK_TX_CLK_EN_B2_CTL) {
		dec_reset_reg = TAPAN_A_CDC_CLK_TX_RESET_B2_CTL;
		offset = 8;
	} else {
		pr_err("%s: Error, incorrect dec\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	tx_vol_ctl_reg = TAPAN_A_CDC_TX1_VOL_CTL_CFG + 8 * (decimator - 1);
	tx_mux_ctl_reg = TAPAN_A_CDC_TX1_MUX_CTL + 8 * (decimator - 1);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		for (i = 0; i < NUM_DECIMATORS; i++) {
			if (decimator == i + 1)
				tapan_p->dec_active[i] = true;
		}

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
		snd_soc_update_bits(codec, TAPAN_A_TX_1_2_TXFE_CLKDIV,
				0x55, 0x44);
		break;

	case SND_SOC_DAPM_POST_PMU:

		if (tapan_p->lb_mode) {
			pr_debug("%s: loopback mode unmute the DEC\n",
							__func__);
			snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x01, 0x00);
		}

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
		for (i = 0; i < NUM_DECIMATORS; i++) {
			if (decimator == i + 1)
				tapan_p->dec_active[i] = false;
		}
		break;
	}
out:
	kfree(widget_name);
	return ret;
}

static int tapan_codec_enable_vdd_spkr(struct snd_soc_dapm_widget *w,
				       struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct wcd9xxx *core = dev_get_drvdata(codec->dev->parent);
	struct tapan_priv *priv = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	dev_dbg(codec->dev, "%s: %s %d\n", __func__, w->name, event);

	WARN_ONCE(!priv->spkdrv_reg, "SPKDRV supply %s isn't defined\n",
		WCD9XXX_VDD_SPKDRV_NAME);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (priv->spkdrv_reg) {
			ret = regulator_enable(priv->spkdrv_reg);
			if (ret)
				dev_err(codec->dev,
					"%s Failed to enable spkdrv reg %s\n",
					__func__, WCD9XXX_VDD_SPKDRV_NAME);
		}
		if (spkr_drv_wrnd > 0) {
			WARN_ON(!(snd_soc_read(codec, TAPAN_A_SPKR_DRV_EN) &
				  0x80));
			snd_soc_update_bits(codec, TAPAN_A_SPKR_DRV_EN, 0x80,
					    0x00);
		}
		if (TAPAN_IS_1_0(core->version))
			snd_soc_update_bits(codec, TAPAN_A_SPKR_DRV_DBG_PWRSTG,
					    0x24, 0x00);
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (TAPAN_IS_1_0(core->version))
			snd_soc_update_bits(codec, TAPAN_A_SPKR_DRV_DBG_PWRSTG,
					    0x24, 0x24);
		if (spkr_drv_wrnd > 0) {
			WARN_ON(!!(snd_soc_read(codec, TAPAN_A_SPKR_DRV_EN) &
				   0x80));
			snd_soc_update_bits(codec, TAPAN_A_SPKR_DRV_EN, 0x80,
					    0x80);
		}
		if (priv->spkdrv_reg) {
			ret = regulator_disable(priv->spkdrv_reg);
			if (ret)
				dev_err(codec->dev,
					"%s: Failed to disable spkdrv_reg %s\n",
					__func__, WCD9XXX_VDD_SPKDRV_NAME);
		}
		break;
	}
	return 0;
}

static int tapan_codec_rx_dem_select(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{

	struct snd_soc_codec *codec = w->codec;

	pr_debug("%s %d %s\n", __func__, event, w->name);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (codec_ver == WCD9306)
			snd_soc_update_bits(codec, TAPAN_A_CDC_RX2_B6_CTL,
					    1 << 5, 1 << 5);
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (codec_ver == WCD9306)
			snd_soc_update_bits(codec, TAPAN_A_CDC_RX2_B6_CTL,
					    1 << 5, 0);
		break;
	}

	return 0;
}

static int tapan_codec_enable_interpolator(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	dev_dbg(codec->dev, "%s %d %s\n", __func__, event, w->name);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, TAPAN_A_CDC_CLK_RX_RESET_CTL,
			1 << w->shift, 1 << w->shift);
		snd_soc_update_bits(codec, TAPAN_A_CDC_CLK_RX_RESET_CTL,
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

/* called under codec_resource_lock acquisition */
static int __tapan_codec_enable_ldo_h(struct snd_soc_dapm_widget *w,
				      struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct tapan_priv *priv = snd_soc_codec_get_drvdata(codec);

	pr_debug("%s: enter\n", __func__);
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/*
		 * ldo_h_users is protected by codec->mutex, don't need
		 * additional mutex
		 */
		if (++priv->ldo_h_users == 1) {
			WCD9XXX_BG_CLK_LOCK(&priv->resmgr);
			wcd9xxx_resmgr_get_bandgap(&priv->resmgr,
						   WCD9XXX_BANDGAP_AUDIO_MODE);
			wcd9xxx_resmgr_get_clk_block(&priv->resmgr,
						     WCD9XXX_CLK_RCO);
			snd_soc_update_bits(codec, TAPAN_A_LDO_H_MODE_1, 1 << 7,
					    1 << 7);
			wcd9xxx_resmgr_put_clk_block(&priv->resmgr,
						     WCD9XXX_CLK_RCO);
			WCD9XXX_BG_CLK_UNLOCK(&priv->resmgr);
			pr_debug("%s: ldo_h_users %d\n", __func__,
				 priv->ldo_h_users);
			/* LDO enable requires 1ms to settle down */
			usleep_range(1000, 1010);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (--priv->ldo_h_users == 0) {
			WCD9XXX_BG_CLK_LOCK(&priv->resmgr);
			wcd9xxx_resmgr_get_clk_block(&priv->resmgr,
						     WCD9XXX_CLK_RCO);
			snd_soc_update_bits(codec, TAPAN_A_LDO_H_MODE_1, 1 << 7,
					    0);
			wcd9xxx_resmgr_put_clk_block(&priv->resmgr,
						     WCD9XXX_CLK_RCO);
			wcd9xxx_resmgr_put_bandgap(&priv->resmgr,
						   WCD9XXX_BANDGAP_AUDIO_MODE);
			WCD9XXX_BG_CLK_UNLOCK(&priv->resmgr);
			pr_debug("%s: ldo_h_users %d\n", __func__,
				 priv->ldo_h_users);
		}
		WARN(priv->ldo_h_users < 0, "Unexpected ldo_h users %d\n",
		     priv->ldo_h_users);
		break;
	}
	pr_debug("%s: leave\n", __func__);
	return 0;
}

static int tapan_codec_enable_ldo_h(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	int rc;
	rc = __tapan_codec_enable_ldo_h(w, kcontrol, event);
	return rc;
}

static int tapan_codec_enable_rx_bias(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct tapan_priv *tapan = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		wcd9xxx_resmgr_enable_rx_bias(&tapan->resmgr, 1);
		break;
	case SND_SOC_DAPM_POST_PMD:
		wcd9xxx_resmgr_enable_rx_bias(&tapan->resmgr, 0);
		break;
	}
	return 0;
}


static int tapan_hphl_dac_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct tapan_priv *tapan_p = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s %s %d\n", __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, TAPAN_A_CDC_CLK_RDAC_CLK_EN_CTL,
							0x02, 0x02);
		wcd9xxx_clsh_fsm(codec, &tapan_p->clsh_d,
						 WCD9XXX_CLSH_STATE_HPHL,
						 WCD9XXX_CLSH_REQ_ENABLE,
						 WCD9XXX_CLSH_EVENT_PRE_DAC);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, TAPAN_A_CDC_CLK_RDAC_CLK_EN_CTL,
							0x02, 0x00);
	}
	return 0;
}

static int tapan_hphr_dac_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct tapan_priv *tapan_p = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s %s %d\n", __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		snd_soc_update_bits(codec, TAPAN_A_CDC_CLK_RDAC_CLK_EN_CTL,
							0x04, 0x04);
		snd_soc_update_bits(codec, w->reg, 0x40, 0x40);
		wcd9xxx_clsh_fsm(codec, &tapan_p->clsh_d,
						 WCD9XXX_CLSH_STATE_HPHR,
						 WCD9XXX_CLSH_REQ_ENABLE,
						 WCD9XXX_CLSH_EVENT_PRE_DAC);
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, TAPAN_A_CDC_CLK_RDAC_CLK_EN_CTL,
							0x04, 0x00);
		snd_soc_update_bits(codec, w->reg, 0x40, 0x00);
		break;
	}
	return 0;
}

static int tapan_hph_pa_event(struct snd_soc_dapm_widget *w,
			      struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct tapan_priv *tapan = snd_soc_codec_get_drvdata(codec);
	enum wcd9xxx_notify_event e_pre_on, e_post_off;
	u8 req_clsh_state;
	u32 pa_settle_time = TAPAN_HPH_PA_SETTLE_COMP_OFF;

	dev_dbg(codec->dev, "%s: %s event = %d\n", __func__, w->name, event);
	if (w->shift == 5) {
		e_pre_on = WCD9XXX_EVENT_PRE_HPHL_PA_ON;
		e_post_off = WCD9XXX_EVENT_POST_HPHL_PA_OFF;
		req_clsh_state = WCD9XXX_CLSH_STATE_HPHR;
	} else if (w->shift == 4) {
		e_pre_on = WCD9XXX_EVENT_PRE_HPHR_PA_ON;
		e_post_off = WCD9XXX_EVENT_POST_HPHR_PA_OFF;
		req_clsh_state = WCD9XXX_CLSH_STATE_HPHL;
	} else {
		pr_err("%s: Invalid w->shift %d\n", __func__, w->shift);
		return -EINVAL;
	}

	if (tapan->comp_enabled[COMPANDER_1])
		pa_settle_time = TAPAN_HPH_PA_SETTLE_COMP_ON;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		/* Let MBHC module know PA is turning on */
		wcd9xxx_resmgr_notifier_call(&tapan->resmgr, e_pre_on);
		break;
	case SND_SOC_DAPM_POST_PMU:
		dev_dbg(codec->dev, "%s: sleep %d ms after %s PA enable.\n",
			__func__, pa_settle_time / 1000, w->name);
		/* Time needed for PA to settle */
		usleep_range(pa_settle_time, pa_settle_time + 1000);

		wcd9xxx_clsh_fsm(codec, &tapan->clsh_d,
						 req_clsh_state,
						 WCD9XXX_CLSH_REQ_ENABLE,
						 WCD9XXX_CLSH_EVENT_POST_PA);

		break;
	case SND_SOC_DAPM_POST_PMD:
		dev_dbg(codec->dev, "%s: sleep %d ms after %s PA disable.\n",
			__func__, pa_settle_time / 1000, w->name);
		/* Time needed for PA to settle */
		usleep_range(pa_settle_time, pa_settle_time + 1000);

		/* Let MBHC module know PA turned off */
		wcd9xxx_resmgr_notifier_call(&tapan->resmgr, e_post_off);

		wcd9xxx_clsh_fsm(codec, &tapan->clsh_d,
						 req_clsh_state,
						 WCD9XXX_CLSH_REQ_DISABLE,
						 WCD9XXX_CLSH_EVENT_POST_PA);
		break;
	}
	return 0;
}

static int tapan_codec_enable_anc_hph(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	int ret = 0;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		ret = tapan_hph_pa_event(w, kcontrol, event);
		if (w->shift == 4) {
			ret |= tapan_codec_enable_anc(w, kcontrol, event);
			msleep(50);
		}
		break;
	case SND_SOC_DAPM_POST_PMU:
		if (w->shift == 4) {
			snd_soc_update_bits(codec,
					TAPAN_A_RX_HPH_CNP_EN, 0x30, 0x30);
			msleep(30);
		}
		ret = tapan_hph_pa_event(w, kcontrol, event);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		if (w->shift == 5) {
			snd_soc_update_bits(codec,
					TAPAN_A_RX_HPH_CNP_EN, 0x30, 0x00);
			msleep(40);
			snd_soc_update_bits(codec,
					TAPAN_A_TX_7_MBHC_EN, 0x80, 00);
			ret |= tapan_codec_enable_anc(w, kcontrol, event);
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		ret = tapan_hph_pa_event(w, kcontrol, event);
		break;
	}
	return ret;
}

static const struct snd_soc_dapm_widget tapan_dapm_i2s_widgets[] = {
	SND_SOC_DAPM_SUPPLY("I2S_CLK", TAPAN_A_CDC_CLK_I2S_CTL,
	4, 0, NULL, 0),
};

static int tapan_lineout_dac_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct tapan_priv *tapan = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s %s %d\n", __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		wcd9xxx_clsh_fsm(codec, &tapan->clsh_d,
						 WCD9XXX_CLSH_STATE_LO,
						 WCD9XXX_CLSH_REQ_ENABLE,
						 WCD9XXX_CLSH_EVENT_PRE_DAC);
		snd_soc_update_bits(codec, w->reg, 0x40, 0x40);
		break;

	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, w->reg, 0x40, 0x00);
		break;
	}
	return 0;
}

static int tapan_spk_dac_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;

	dev_dbg(codec->dev, "%s %s %d\n", __func__, w->name, event);
	return 0;
}

static const struct snd_soc_dapm_route audio_i2s_map[] = {
	{"I2S_CLK", NULL, "CDC_CONN"},
	{"SLIM RX1", NULL, "I2S_CLK"},
	{"SLIM RX2", NULL, "I2S_CLK"},

	{"SLIM TX1 MUX", NULL, "I2S_CLK"},
	{"SLIM TX2 MUX", NULL, "I2S_CLK"},
};

static const struct snd_soc_dapm_route wcd9306_map[] = {
	{"SLIM TX1 MUX", "RMIX4", "RX4 MIX1"},
	{"SLIM TX2 MUX", "RMIX4", "RX4 MIX1"},
	{"SLIM TX3 MUX", "RMIX4", "RX4 MIX1"},
	{"SLIM TX4 MUX", "RMIX4", "RX4 MIX1"},
	{"SLIM TX5 MUX", "RMIX4", "RX4 MIX1"},
	{"SLIM TX1 MUX", "DEC3", "DEC3 MUX"},
	{"SLIM TX1 MUX", "DEC4", "DEC4 MUX"},
	{"SLIM TX2 MUX", "DEC3", "DEC3 MUX"},
	{"SLIM TX2 MUX", "DEC4", "DEC4 MUX"},
	{"SLIM TX3 MUX", "DEC3", "DEC3 MUX"},
	{"SLIM TX4 MUX", "DEC4", "DEC4 MUX"},

	{"ANC EAR", NULL, "ANC EAR PA"},
	{"ANC EAR PA", NULL, "EAR_PA_MIXER"},
	{"ANC1 FB MUX", "EAR_HPH_L", "RX1 MIX2"},
	{"ANC1 FB MUX", "EAR_LINE_1", "RX2 MIX2"},

	{"ANC HEADPHONE", NULL, "ANC HPHL"},
	{"ANC HEADPHONE", NULL, "ANC HPHR"},

	{"ANC HPHL", NULL, "HPHL_PA_MIXER"},
	{"ANC HPHR", NULL, "HPHR_PA_MIXER"},

	{"ANC1 MUX", "ADC1", "ADC1"},
	{"ANC1 MUX", "ADC2", "ADC2"},
	{"ANC1 MUX", "ADC3", "ADC3"},
	{"ANC1 MUX", "ADC4", "ADC4"},
	{"ANC1 MUX", "ADC5", "ADC5"},
	{"ANC1 MUX", "DMIC1", "DMIC1"},
	{"ANC1 MUX", "DMIC2", "DMIC2"},
	{"ANC1 MUX", "DMIC3", "DMIC3"},
	{"ANC1 MUX", "DMIC4", "DMIC4"},
	{"ANC2 MUX", "ADC1", "ADC1"},
	{"ANC2 MUX", "ADC2", "ADC2"},
	{"ANC2 MUX", "ADC3", "ADC3"},
	{"ANC2 MUX", "ADC4", "ADC4"},
	{"ANC2 MUX", "ADC5", "ADC5"},
	{"ANC2 MUX", "DMIC1", "DMIC1"},
	{"ANC2 MUX", "DMIC2", "DMIC2"},
	{"ANC2 MUX", "DMIC3", "DMIC3"},
	{"ANC2 MUX", "DMIC4", "DMIC4"},

	{"ANC HPHR", NULL, "CDC_CONN"},

	{"RDAC5 MUX", "DEM4", "RX4 MIX2"},
	{"SPK DAC", "Switch", "RX4 MIX2"},

	{"RX1 MIX2", NULL, "ANC1 MUX"},
	{"RX2 MIX2", NULL, "ANC2 MUX"},

	{"RX1 MIX1", NULL, "COMP1_CLK"},
	{"RX2 MIX1", NULL, "COMP1_CLK"},
	{"RX3 MIX1", NULL, "COMP2_CLK"},
	{"RX4 MIX1", NULL, "COMP0_CLK"},

	{"RX4 MIX1", NULL, "RX4 MIX1 INP1"},
	{"RX4 MIX1", NULL, "RX4 MIX1 INP2"},
	{"RX4 MIX2", NULL, "RX4 MIX1"},
	{"RX4 MIX2", NULL, "RX4 MIX2 INP1"},
	{"RX4 MIX2", NULL, "RX4 MIX2 INP2"},

	{"RX4 MIX1 INP1", "RX1", "SLIM RX1"},
	{"RX4 MIX1 INP1", "RX2", "SLIM RX2"},
	{"RX4 MIX1 INP1", "RX3", "SLIM RX3"},
	{"RX4 MIX1 INP1", "RX4", "SLIM RX4"},
	{"RX4 MIX1 INP1", "RX5", "SLIM RX5"},
	{"RX4 MIX1 INP1", "IIR1", "IIR1"},
	{"RX4 MIX1 INP2", "RX1", "SLIM RX1"},
	{"RX4 MIX1 INP2", "RX2", "SLIM RX2"},
	{"RX4 MIX1 INP2", "RX3", "SLIM RX3"},
	{"RX4 MIX1 INP2", "RX5", "SLIM RX5"},
	{"RX4 MIX1 INP2", "RX4", "SLIM RX4"},
	{"RX4 MIX1 INP2", "IIR1", "IIR1"},
	{"RX4 MIX2 INP1", "IIR1", "IIR1"},
	{"RX4 MIX2 INP2", "IIR1", "IIR1"},

	{"DEC1 MUX", "DMIC3", "DMIC3"},
	{"DEC1 MUX", "DMIC4", "DMIC4"},
	{"DEC2 MUX", "DMIC3", "DMIC3"},
	{"DEC2 MUX", "DMIC4", "DMIC4"},

	{"DEC3 MUX", "ADC1", "ADC1"},
	{"DEC3 MUX", "ADC2", "ADC2"},
	{"DEC3 MUX", "ADC3", "ADC3"},
	{"DEC3 MUX", "ADC4", "ADC4"},
	{"DEC3 MUX", "ADC5", "ADC5"},
	{"DEC3 MUX", "DMIC1", "DMIC1"},
	{"DEC3 MUX", "DMIC2", "DMIC2"},
	{"DEC3 MUX", "DMIC3", "DMIC3"},
	{"DEC3 MUX", "DMIC4", "DMIC4"},
	{"DEC3 MUX", "ANCFBTUNE1", "ANC1 MUX"},
	{"DEC3 MUX", NULL, "CDC_CONN"},

	{"DEC4 MUX", "ADC1", "ADC1"},
	{"DEC4 MUX", "ADC2", "ADC2"},
	{"DEC4 MUX", "ADC3", "ADC3"},
	{"DEC4 MUX", "ADC4", "ADC4"},
	{"DEC4 MUX", "ADC5", "ADC5"},
	{"DEC4 MUX", "DMIC1", "DMIC1"},
	{"DEC4 MUX", "DMIC2", "DMIC2"},
	{"DEC4 MUX", "DMIC3", "DMIC3"},
	{"DEC4 MUX", "DMIC4", "DMIC4"},
	{"DEC4 MUX", "ANCFBTUNE2", "ANC2 MUX"},
	{"DEC4 MUX", NULL, "CDC_CONN"},

	{"ADC5", NULL, "AMIC5"},

	{"AUX_PGA_Left", NULL, "AMIC5"},

	{"IIR1 INP1 MUX", "DEC3", "DEC3 MUX"},
	{"IIR1 INP1 MUX", "DEC4", "DEC4 MUX"},

	{"MIC BIAS3 Internal1", NULL, "LDO_H"},
	{"MIC BIAS3 Internal2", NULL, "LDO_H"},
	{"MIC BIAS3 External", NULL, "LDO_H"},
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
	/* SLIM_MIXER("AIF2_CAP Mixer"),*/
	{"AIF2_CAP Mixer", "SLIM TX1", "SLIM TX1 MUX"},
	{"AIF2_CAP Mixer", "SLIM TX2", "SLIM TX2 MUX"},
	{"AIF2_CAP Mixer", "SLIM TX3", "SLIM TX3 MUX"},
	{"AIF2_CAP Mixer", "SLIM TX4", "SLIM TX4 MUX"},
	{"AIF2_CAP Mixer", "SLIM TX5", "SLIM TX5 MUX"},
	/* SLIM_MIXER("AIF3_CAP Mixer"),*/
	{"AIF3_CAP Mixer", "SLIM TX1", "SLIM TX1 MUX"},
	{"AIF3_CAP Mixer", "SLIM TX2", "SLIM TX2 MUX"},
	{"AIF3_CAP Mixer", "SLIM TX3", "SLIM TX3 MUX"},
	{"AIF3_CAP Mixer", "SLIM TX4", "SLIM TX4 MUX"},
	{"AIF3_CAP Mixer", "SLIM TX5", "SLIM TX5 MUX"},

	{"SLIM TX1 MUX", "DEC1", "DEC1 MUX"},
	{"SLIM TX1 MUX", "DEC2", "DEC2 MUX"},
	{"SLIM TX1 MUX", "RMIX1", "RX1 MIX1"},
	{"SLIM TX1 MUX", "RMIX2", "RX2 MIX1"},
	{"SLIM TX1 MUX", "RMIX3", "RX3 MIX1"},

	{"SLIM TX2 MUX", "DEC1", "DEC1 MUX"},
	{"SLIM TX2 MUX", "DEC2", "DEC2 MUX"},
	{"SLIM TX2 MUX", "RMIX1", "RX1 MIX1"},
	{"SLIM TX2 MUX", "RMIX2", "RX2 MIX1"},
	{"SLIM TX2 MUX", "RMIX3", "RX3 MIX1"},

	{"SLIM TX3 MUX", "RMIX1", "RX1 MIX1"},
	{"SLIM TX3 MUX", "RMIX2", "RX2 MIX1"},
	{"SLIM TX3 MUX", "RMIX3", "RX3 MIX1"},

	{"SLIM TX4 MUX", "RMIX1", "RX1 MIX1"},
	{"SLIM TX4 MUX", "RMIX2", "RX2 MIX1"},
	{"SLIM TX4 MUX", "RMIX3", "RX3 MIX1"},

	{"SLIM TX5 MUX", "DEC1", "DEC1 MUX"},
	{"SLIM TX5 MUX", "RMIX1", "RX1 MIX1"},
	{"SLIM TX5 MUX", "RMIX2", "RX2 MIX1"},
	{"SLIM TX5 MUX", "RMIX3", "RX3 MIX1"},

	/* Earpiece (RX MIX1) */
	{"EAR", NULL, "EAR PA"},
	{"EAR PA", NULL, "EAR_PA_MIXER"},
	{"EAR_PA_MIXER", NULL, "DAC1"},
	{"DAC1", NULL, "RX_BIAS"},
	{"DAC1", NULL, "CDC_CP_VDD"},


	/* Headset (RX MIX1 and RX MIX2) */
	{"HEADPHONE", NULL, "HPHL"},
	{"HEADPHONE", NULL, "HPHR"},

	{"HPHL", NULL, "HPHL_PA_MIXER"},
	{"HPHL_PA_MIXER", NULL, "HPHL DAC"},
	{"HPHL DAC", NULL, "RX_BIAS"},
	{"HPHL DAC", NULL, "CDC_CP_VDD"},

	{"HPHR", NULL, "HPHR_PA_MIXER"},
	{"HPHR_PA_MIXER", NULL, "HPHR DAC"},
	{"HPHR DAC", NULL, "RX_BIAS"},
	{"HPHR DAC", NULL, "CDC_CP_VDD"},


	{"DAC1", "Switch", "CLASS_H_DSM MUX"},
	{"HPHL DAC", "Switch", "CLASS_H_DSM MUX"},
	{"HPHR DAC", NULL, "RDAC3 MUX"},

	{"LINEOUT1", NULL, "LINEOUT1 PA"},
	{"LINEOUT2", NULL, "LINEOUT2 PA"},
	{"SPK_OUT", NULL, "SPK PA"},

	{"LINEOUT1 PA", NULL, "LINEOUT1_PA_MIXER"},
	{"LINEOUT1_PA_MIXER", NULL, "LINEOUT1 DAC"},
	{"LINEOUT2 PA", NULL, "LINEOUT2_PA_MIXER"},
	{"LINEOUT2_PA_MIXER", NULL, "LINEOUT2 DAC"},


	{"RDAC5 MUX", "DEM3_INV", "RX3 MIX1"},
	{"LINEOUT2 DAC", NULL, "RDAC5 MUX"},

	{"RDAC4 MUX", "DEM3", "RX3 MIX1"},
	{"RDAC4 MUX", "DEM2", "RX2 CHAIN"},
	{"LINEOUT1 DAC", NULL, "RDAC4 MUX"},

	{"SPK PA", NULL, "SPK DAC"},
	{"SPK DAC", NULL, "VDD_SPKDRV"},

	{"RX1 INTERPOLATOR", NULL, "RX1 MIX2"},
	{"RX1 CHAIN", NULL, "RX1 INTERPOLATOR"},
	{"RX2 INTERPOLATOR", NULL, "RX2 MIX2"},
	{"RX2 CHAIN", NULL, "RX2 INTERPOLATOR"},
	{"CLASS_H_DSM MUX", "RX_HPHL", "RX1 CHAIN"},

	{"LINEOUT1 DAC", NULL, "RX_BIAS"},
	{"LINEOUT2 DAC", NULL, "RX_BIAS"},
	{"LINEOUT1 DAC", NULL, "CDC_CP_VDD"},
	{"LINEOUT2 DAC", NULL, "CDC_CP_VDD"},

	{"RDAC3 MUX", "DEM2", "RX2 CHAIN"},
	{"RDAC3 MUX", "DEM1", "RX1 CHAIN"},

	{"RX1 MIX1", NULL, "RX1 MIX1 INP1"},
	{"RX1 MIX1", NULL, "RX1 MIX1 INP2"},
	{"RX1 MIX1", NULL, "RX1 MIX1 INP3"},
	{"RX2 MIX1", NULL, "RX2 MIX1 INP1"},
	{"RX2 MIX1", NULL, "RX2 MIX1 INP2"},
	{"RX3 MIX1", NULL, "RX3 MIX1 INP1"},
	{"RX3 MIX1", NULL, "RX3 MIX1 INP2"},
	{"RX1 MIX2", NULL, "RX1 MIX1"},
	{"RX1 MIX2", NULL, "RX1 MIX2 INP1"},
	{"RX1 MIX2", NULL, "RX1 MIX2 INP2"},
	{"RX2 MIX2", NULL, "RX2 MIX1"},
	{"RX2 MIX2", NULL, "RX2 MIX2 INP1"},
	{"RX2 MIX2", NULL, "RX2 MIX2 INP2"},

	/* SLIM_MUX("AIF1_PB", "AIF1 PB"),*/
	{"SLIM RX1 MUX", "AIF1_PB", "AIF1 PB"},
	{"SLIM RX2 MUX", "AIF1_PB", "AIF1 PB"},
	{"SLIM RX3 MUX", "AIF1_PB", "AIF1 PB"},
	{"SLIM RX4 MUX", "AIF1_PB", "AIF1 PB"},
	{"SLIM RX5 MUX", "AIF1_PB", "AIF1 PB"},
	/* SLIM_MUX("AIF2_PB", "AIF2 PB"),*/
	{"SLIM RX1 MUX", "AIF2_PB", "AIF2 PB"},
	{"SLIM RX2 MUX", "AIF2_PB", "AIF2 PB"},
	{"SLIM RX3 MUX", "AIF2_PB", "AIF2 PB"},
	{"SLIM RX4 MUX", "AIF2_PB", "AIF2 PB"},
	{"SLIM RX5 MUX", "AIF2_PB", "AIF2 PB"},
	/* SLIM_MUX("AIF3_PB", "AIF3 PB"),*/
	{"SLIM RX1 MUX", "AIF3_PB", "AIF3 PB"},
	{"SLIM RX2 MUX", "AIF3_PB", "AIF3 PB"},
	{"SLIM RX3 MUX", "AIF3_PB", "AIF3 PB"},
	{"SLIM RX4 MUX", "AIF3_PB", "AIF3 PB"},
	{"SLIM RX5 MUX", "AIF3_PB", "AIF3 PB"},

	{"SLIM RX1", NULL, "SLIM RX1 MUX"},
	{"SLIM RX2", NULL, "SLIM RX2 MUX"},
	{"SLIM RX3", NULL, "SLIM RX3 MUX"},
	{"SLIM RX4", NULL, "SLIM RX4 MUX"},
	{"SLIM RX5", NULL, "SLIM RX5 MUX"},

	{"RX1 MIX1 INP1", "RX1", "SLIM RX1"},
	{"RX1 MIX1 INP1", "RX2", "SLIM RX2"},
	{"RX1 MIX1 INP1", "RX3", "SLIM RX3"},
	{"RX1 MIX1 INP1", "RX4", "SLIM RX4"},
	{"RX1 MIX1 INP1", "RX5", "SLIM RX5"},
	{"RX1 MIX1 INP1", "IIR1", "IIR1"},
	{"RX1 MIX1 INP1", "IIR2", "IIR2"},
	{"RX1 MIX1 INP2", "RX1", "SLIM RX1"},
	{"RX1 MIX1 INP2", "RX2", "SLIM RX2"},
	{"RX1 MIX1 INP2", "RX3", "SLIM RX3"},
	{"RX1 MIX1 INP2", "RX4", "SLIM RX4"},
	{"RX1 MIX1 INP2", "RX5", "SLIM RX5"},
	{"RX1 MIX1 INP2", "IIR1", "IIR1"},
	{"RX1 MIX1 INP2", "IIR2", "IIR2"},
	{"RX1 MIX1 INP3", "RX1", "SLIM RX1"},
	{"RX1 MIX1 INP3", "RX2", "SLIM RX2"},
	{"RX1 MIX1 INP3", "RX3", "SLIM RX3"},
	{"RX1 MIX1 INP3", "RX4", "SLIM RX4"},
	{"RX1 MIX1 INP3", "RX5", "SLIM RX5"},
	{"RX2 MIX1 INP1", "RX1", "SLIM RX1"},
	{"RX2 MIX1 INP1", "RX2", "SLIM RX2"},
	{"RX2 MIX1 INP1", "RX3", "SLIM RX3"},
	{"RX2 MIX1 INP1", "RX4", "SLIM RX4"},
	{"RX2 MIX1 INP1", "RX5", "SLIM RX5"},
	{"RX2 MIX1 INP1", "IIR1", "IIR1"},
	{"RX2 MIX1 INP1", "IIR2", "IIR2"},
	{"RX2 MIX1 INP2", "RX1", "SLIM RX1"},
	{"RX2 MIX1 INP2", "RX2", "SLIM RX2"},
	{"RX2 MIX1 INP2", "RX3", "SLIM RX3"},
	{"RX2 MIX1 INP2", "RX4", "SLIM RX4"},
	{"RX2 MIX1 INP2", "RX5", "SLIM RX5"},
	{"RX2 MIX1 INP2", "IIR1", "IIR1"},
	{"RX2 MIX1 INP2", "IIR2", "IIR2"},
	{"RX3 MIX1 INP1", "RX1", "SLIM RX1"},
	{"RX3 MIX1 INP1", "RX2", "SLIM RX2"},
	{"RX3 MIX1 INP1", "RX3", "SLIM RX3"},
	{"RX3 MIX1 INP1", "RX4", "SLIM RX4"},
	{"RX3 MIX1 INP1", "RX5", "SLIM RX5"},
	{"RX3 MIX1 INP1", "IIR1", "IIR1"},
	{"RX3 MIX1 INP1", "IIR2", "IIR2"},
	{"RX3 MIX1 INP2", "RX1", "SLIM RX1"},
	{"RX3 MIX1 INP2", "RX2", "SLIM RX2"},
	{"RX3 MIX1 INP2", "RX3", "SLIM RX3"},
	{"RX3 MIX1 INP2", "RX4", "SLIM RX4"},
	{"RX3 MIX1 INP2", "RX5", "SLIM RX5"},
	{"RX3 MIX1 INP2", "IIR1", "IIR1"},
	{"RX3 MIX1 INP2", "IIR2", "IIR2"},

	{"RX1 MIX2 INP1", "IIR1", "IIR1"},
	{"RX1 MIX2 INP2", "IIR1", "IIR1"},
	{"RX2 MIX2 INP1", "IIR1", "IIR1"},
	{"RX2 MIX2 INP2", "IIR1", "IIR1"},

	{"RX1 MIX2 INP1", "IIR2", "IIR2"},
	{"RX1 MIX2 INP2", "IIR2", "IIR2"},
	{"RX2 MIX2 INP1", "IIR2", "IIR2"},
	{"RX2 MIX2 INP2", "IIR2", "IIR2"},

	/* Decimator Inputs */
	{"DEC1 MUX", "ADC1", "ADC1"},
	{"DEC1 MUX", "ADC2", "ADC2"},
	{"DEC1 MUX", "ADC3", "ADC3"},
	{"DEC1 MUX", "ADC4", "ADC4"},
	{"DEC1 MUX", "DMIC1", "DMIC1"},
	{"DEC1 MUX", "DMIC2", "DMIC2"},
	{"DEC1 MUX", NULL, "CDC_CONN"},

	{"DEC2 MUX", "ADC1", "ADC1"},
	{"DEC2 MUX", "ADC2", "ADC2"},
	{"DEC2 MUX", "ADC3", "ADC3"},
	{"DEC2 MUX", "ADC4", "ADC4"},
	{"DEC2 MUX", "DMIC1", "DMIC1"},
	{"DEC2 MUX", "DMIC2", "DMIC2"},
	{"DEC2 MUX", NULL, "CDC_CONN"},

	/* ADC Connections */
	{"ADC1", NULL, "AMIC1"},
	{"ADC2", NULL, "AMIC2"},
	{"ADC3", NULL, "AMIC3"},
	{"ADC4", NULL, "AMIC4"},

	/* AUX PGA Connections */
	{"EAR_PA_MIXER", "AUX_PGA_L Switch", "AUX_PGA_Left"},
	{"HPHL_PA_MIXER", "AUX_PGA_L Switch", "AUX_PGA_Left"},
	{"HPHR_PA_MIXER", "AUX_PGA_R Switch", "AUX_PGA_Right"},
	{"LINEOUT1_PA_MIXER", "AUX_PGA_L Switch", "AUX_PGA_Left"},
	{"LINEOUT2_PA_MIXER", "AUX_PGA_R Switch", "AUX_PGA_Right"},

	{"MIC BIAS1 Internal1", NULL, "LDO_H"},
	{"MIC BIAS1 Internal2", NULL, "LDO_H"},
	{"MIC BIAS1 External", NULL, "LDO_H"},
	{"MIC BIAS2 Internal1", NULL, "LDO_H"},
	{"MIC BIAS2 Internal2", NULL, "LDO_H"},
	{"MIC BIAS2 Internal3", NULL, "LDO_H"},
	{"MIC BIAS2 External", NULL, "LDO_H"},
	{DAPM_MICBIAS2_EXTERNAL_STANDALONE, NULL, "LDO_H Standalone"},

	/*sidetone path enable*/
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

	{"IIR1", NULL, "IIR1 INP2 MUX"},
	{"IIR1 INP2 MUX", "DEC1", "DEC1 MUX"},
	{"IIR1 INP2 MUX", "DEC2", "DEC2 MUX"},
	{"IIR1 INP2 MUX", "DEC3", "DEC3 MUX"},
	{"IIR1 INP2 MUX", "DEC4", "DEC4 MUX"},
	{"IIR1 INP2 MUX", "RX1", "SLIM RX1"},
	{"IIR1 INP2 MUX", "RX2", "SLIM RX2"},
	{"IIR1 INP2 MUX", "RX3", "SLIM RX3"},
	{"IIR1 INP2 MUX", "RX4", "SLIM RX4"},
	{"IIR1 INP2 MUX", "RX5", "SLIM RX5"},

	{"IIR1", NULL, "IIR1 INP3 MUX"},
	{"IIR1 INP3 MUX", "DEC1", "DEC1 MUX"},
	{"IIR1 INP3 MUX", "DEC2", "DEC2 MUX"},
	{"IIR1 INP3 MUX", "DEC3", "DEC3 MUX"},
	{"IIR1 INP3 MUX", "DEC4", "DEC4 MUX"},
	{"IIR1 INP3 MUX", "RX1", "SLIM RX1"},
	{"IIR1 INP3 MUX", "RX2", "SLIM RX2"},
	{"IIR1 INP3 MUX", "RX3", "SLIM RX3"},
	{"IIR1 INP3 MUX", "RX4", "SLIM RX4"},
	{"IIR1 INP3 MUX", "RX5", "SLIM RX5"},

	{"IIR1", NULL, "IIR1 INP4 MUX"},
	{"IIR1 INP4 MUX", "DEC1", "DEC1 MUX"},
	{"IIR1 INP4 MUX", "DEC2", "DEC2 MUX"},
	{"IIR1 INP4 MUX", "DEC3", "DEC3 MUX"},
	{"IIR1 INP4 MUX", "DEC4", "DEC4 MUX"},
	{"IIR1 INP4 MUX", "RX1", "SLIM RX1"},
	{"IIR1 INP4 MUX", "RX2", "SLIM RX2"},
	{"IIR1 INP4 MUX", "RX3", "SLIM RX3"},
	{"IIR1 INP4 MUX", "RX4", "SLIM RX4"},
	{"IIR1 INP4 MUX", "RX5", "SLIM RX5"},

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

	{"IIR2", NULL, "IIR2 INP2 MUX"},
	{"IIR2 INP2 MUX", "DEC1", "DEC1 MUX"},
	{"IIR2 INP2 MUX", "DEC2", "DEC2 MUX"},
	{"IIR2 INP2 MUX", "DEC3", "DEC3 MUX"},
	{"IIR2 INP2 MUX", "DEC4", "DEC4 MUX"},
	{"IIR2 INP2 MUX", "RX1", "SLIM RX1"},
	{"IIR2 INP2 MUX", "RX2", "SLIM RX2"},
	{"IIR2 INP2 MUX", "RX3", "SLIM RX3"},
	{"IIR2 INP2 MUX", "RX4", "SLIM RX4"},
	{"IIR2 INP2 MUX", "RX5", "SLIM RX5"},

	{"IIR2", NULL, "IIR2 INP3 MUX"},
	{"IIR2 INP3 MUX", "DEC1", "DEC1 MUX"},
	{"IIR2 INP3 MUX", "DEC2", "DEC2 MUX"},
	{"IIR2 INP3 MUX", "DEC3", "DEC3 MUX"},
	{"IIR2 INP3 MUX", "DEC4", "DEC4 MUX"},
	{"IIR2 INP3 MUX", "RX1", "SLIM RX1"},
	{"IIR2 INP3 MUX", "RX2", "SLIM RX2"},
	{"IIR2 INP3 MUX", "RX3", "SLIM RX3"},
	{"IIR2 INP3 MUX", "RX4", "SLIM RX4"},
	{"IIR2 INP3 MUX", "RX5", "SLIM RX5"},

	{"IIR2", NULL, "IIR2 INP4 MUX"},
	{"IIR2 INP4 MUX", "DEC1", "DEC1 MUX"},
	{"IIR2 INP4 MUX", "DEC2", "DEC2 MUX"},
	{"IIR2 INP4 MUX", "DEC3", "DEC3 MUX"},
	{"IIR2 INP4 MUX", "DEC4", "DEC4 MUX"},
	{"IIR2 INP4 MUX", "RX1", "SLIM RX1"},
	{"IIR2 INP4 MUX", "RX2", "SLIM RX2"},
	{"IIR2 INP4 MUX", "RX3", "SLIM RX3"},
	{"IIR2 INP4 MUX", "RX4", "SLIM RX4"},
	{"IIR2 INP4 MUX", "RX5", "SLIM RX5"},
};

static const struct snd_soc_dapm_route wcd9302_map[] = {
	{"SPK DAC", "Switch", "RX3 MIX1"},


	{"RDAC5 MUX", "DEM4", "RX3 MIX1"},
	{"RDAC5 MUX", "DEM3_INV", "RDAC4 MUX"},
};

static int tapan_readable(struct snd_soc_codec *ssc, unsigned int reg)
{
	return tapan_reg_readable[reg];
}

static bool tapan_is_digital_gain_register(unsigned int reg)
{
	bool rtn = false;
	switch (reg) {
	case TAPAN_A_CDC_RX1_VOL_CTL_B2_CTL:
	case TAPAN_A_CDC_RX2_VOL_CTL_B2_CTL:
	case TAPAN_A_CDC_RX3_VOL_CTL_B2_CTL:
	case TAPAN_A_CDC_RX4_VOL_CTL_B2_CTL:
	case TAPAN_A_CDC_TX1_VOL_CTL_GAIN:
	case TAPAN_A_CDC_TX2_VOL_CTL_GAIN:
	case TAPAN_A_CDC_TX3_VOL_CTL_GAIN:
	case TAPAN_A_CDC_TX4_VOL_CTL_GAIN:
		rtn = true;
		break;
	default:
		break;
	}
	return rtn;
}

static int tapan_volatile(struct snd_soc_codec *ssc, unsigned int reg)
{

	int i = 0;

	/* Registers lower than 0x100 are top level registers which can be
	 * written by the Tapan core driver.
	 */

	if ((reg >= TAPAN_A_CDC_MBHC_EN_CTL) || (reg < 0x100))
		return 1;

	/* IIR Coeff registers are not cacheable */
	if ((reg >= TAPAN_A_CDC_IIR1_COEF_B1_CTL) &&
		(reg <= TAPAN_A_CDC_IIR2_COEF_B2_CTL))
		return 1;

	/* ANC filter registers are not cacheable */
	if ((reg >= TAPAN_A_CDC_ANC1_IIR_B1_CTL) &&
		(reg <= TAPAN_A_CDC_ANC1_LPF_B2_CTL))
		return 1;
	if ((reg >= TAPAN_A_CDC_ANC2_IIR_B1_CTL) &&
		(reg <= TAPAN_A_CDC_ANC2_LPF_B2_CTL))
		return 1;

	/* Digital gain register is not cacheable so we have to write
	 * the setting even it is the same
	 */
	if (tapan_is_digital_gain_register(reg))
		return 1;

	/* HPH status registers */
	if (reg == TAPAN_A_RX_HPH_L_STATUS || reg == TAPAN_A_RX_HPH_R_STATUS)
		return 1;

	if (reg == TAPAN_A_MBHC_INSERT_DET_STATUS)
		return 1;

	for (i = 0; i < ARRAY_SIZE(audio_reg_cfg); i++)
		if (audio_reg_cfg[i].reg_logical_addr -
			TAPAN_REGISTER_START_OFFSET == reg)
			return 1;

	return 0;
}

#define TAPAN_FORMATS (SNDRV_PCM_FMTBIT_S16_LE)
#define TAPAN_FORMATS_S16_S24_LE (SNDRV_PCM_FMTBIT_S16_LE | \
				  SNDRV_PCM_FORMAT_S24_LE)
static int tapan_write(struct snd_soc_codec *codec, unsigned int reg,
	unsigned int value)
{
	int ret;
	struct wcd9xxx *wcd9xxx = codec->control_data;
	struct tapan_priv *tapan_p = snd_soc_codec_get_drvdata(codec);

	if (reg == SND_SOC_NOPM)
		return 0;

	BUG_ON(reg > TAPAN_MAX_REGISTER);

	if (!tapan_volatile(codec, reg)) {
		ret = snd_soc_cache_write(codec, reg, value);
		if (ret != 0)
			dev_err(codec->dev, "Cache write to %x failed: %d\n",
				reg, ret);
	}

	if (unlikely(test_bit(BUS_DOWN, &tapan_p->status_mask))) {
		printk_ratelimited(KERN_ERR "write 0x%02x while offline\n",
				reg);
		return -ENODEV;
	} else
		return wcd9xxx_reg_write(&wcd9xxx->core_res, reg, value);
}
static unsigned int tapan_read(struct snd_soc_codec *codec,
				unsigned int reg)
{
	unsigned int val;
	int ret;
	struct wcd9xxx *wcd9xxx = codec->control_data;
	struct tapan_priv *tapan_p = snd_soc_codec_get_drvdata(codec);

	if (reg == SND_SOC_NOPM)
		return 0;

	BUG_ON(reg > TAPAN_MAX_REGISTER);

	if (!tapan_volatile(codec, reg) && tapan_readable(codec, reg) &&
		reg < codec->driver->reg_cache_size) {
		ret = snd_soc_cache_read(codec, reg, &val);
		if (ret >= 0) {
			return val;
		} else
			dev_err(codec->dev, "Cache read from %x failed: %d\n",
				reg, ret);
	}

	if (unlikely(test_bit(BUS_DOWN, &tapan_p->status_mask))) {
		printk_ratelimited(KERN_ERR "write 0x%02x while offline\n",
				reg);
		return -ENODEV;
	} else {
		val = wcd9xxx_reg_read(&wcd9xxx->core_res, reg);
		return val;
	}
}

static int tapan_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	dev_dbg(dai->codec->dev, "%s(): substream = %s  stream = %d\n",
		 __func__, substream->name, substream->stream);

	return 0;
}

static void tapan_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	dev_dbg(dai->codec->dev, "%s(): substream = %s  stream = %d\n",
		 __func__, substream->name, substream->stream);
}

static void tapan_set_vdd_cx_current(struct snd_soc_codec *codec,
			int current_uA)
{
	struct regulator *cx_regulator;
	int ret;

	cx_regulator  = tapan_codec_find_regulator(codec,
				"cdc-vdd-cx");

	if (!cx_regulator) {
		dev_err(codec->dev, "%s: Regulator %s not defined\n",
			__func__, "cdc-vdd-cx-supply");
		return;
	}

	ret = regulator_set_optimum_mode(cx_regulator, current_uA);
	if (ret < 0)
		dev_err(codec->dev,
			"%s: Failed to set vdd_cx current to %d\n",
			__func__, current_uA);
}

int tapan_mclk_enable(struct snd_soc_codec *codec, int mclk_enable, bool dapm)
{
	struct tapan_priv *tapan = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s: mclk_enable = %u, dapm = %d\n", __func__,
		 mclk_enable, dapm);

	WCD9XXX_BG_CLK_LOCK(&tapan->resmgr);
	if (mclk_enable) {
		tapan_set_vdd_cx_current(codec, TAPAN_VDD_CX_OPTIMAL_UA);
		wcd9xxx_resmgr_get_bandgap(&tapan->resmgr,
					   WCD9XXX_BANDGAP_AUDIO_MODE);
		wcd9xxx_resmgr_get_clk_block(&tapan->resmgr, WCD9XXX_CLK_MCLK);
	} else {
		/* Put clock and BG */
		wcd9xxx_resmgr_put_clk_block(&tapan->resmgr, WCD9XXX_CLK_MCLK);
		wcd9xxx_resmgr_put_bandgap(&tapan->resmgr,
					   WCD9XXX_BANDGAP_AUDIO_MODE);
		/* Set the vdd cx power rail sleep mode current */
		tapan_set_vdd_cx_current(codec, TAPAN_VDD_CX_SLEEP_UA);
	}
	WCD9XXX_BG_CLK_UNLOCK(&tapan->resmgr);

	return 0;
}

static int tapan_set_dai_sysclk(struct snd_soc_dai *dai,
		int clk_id, unsigned int freq, int dir)
{
	dev_dbg(dai->codec->dev, "%s\n", __func__);
	return 0;
}

static int tapan_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	u8 val = 0;
	struct snd_soc_codec *codec = dai->codec;
	struct tapan_priv *tapan = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s\n", __func__);
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		/* CPU is master */
		if (tapan->intf_type == WCD9XXX_INTERFACE_TYPE_I2C) {
			if (dai->id == AIF1_CAP)
				snd_soc_update_bits(codec,
					TAPAN_A_CDC_CLK_I2S_CTL,
					TAPAN_I2S_MASTER_MODE_MASK, 0);
			else if (dai->id == AIF1_PB)
				snd_soc_update_bits(codec,
					TAPAN_A_CDC_CLK_I2S_CTL,
					TAPAN_I2S_MASTER_MODE_MASK, 0);
		}
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
	/* CPU is slave */
		if (tapan->intf_type == WCD9XXX_INTERFACE_TYPE_I2C) {
			val = TAPAN_I2S_MASTER_MODE_MASK;
			if (dai->id == AIF1_CAP)
				snd_soc_update_bits(codec,
					TAPAN_A_CDC_CLK_I2S_CTL, val, val);
			else if (dai->id == AIF1_PB)
				snd_soc_update_bits(codec,
					TAPAN_A_CDC_CLK_I2S_CTL, val, val);
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int tapan_set_channel_map(struct snd_soc_dai *dai,
				unsigned int tx_num, unsigned int *tx_slot,
				unsigned int rx_num, unsigned int *rx_slot)

{
	struct tapan_priv *tapan = snd_soc_codec_get_drvdata(dai->codec);
	struct wcd9xxx *core = dev_get_drvdata(dai->codec->dev->parent);
	if (!tx_slot || !rx_slot) {
		pr_err("%s: Invalid\n", __func__);
		return -EINVAL;
	}
	dev_dbg(dai->codec->dev, "%s(): dai_name = %s DAI-ID %x\n",
		 __func__, dai->name, dai->id);
	dev_dbg(dai->codec->dev, "%s(): tx_ch %d rx_ch %d\n intf_type %d\n",
		 __func__, tx_num, rx_num, tapan->intf_type);

	if (tapan->intf_type == WCD9XXX_INTERFACE_TYPE_SLIMBUS)
		wcd9xxx_init_slimslave(core, core->slim->laddr,
				       tx_num, tx_slot, rx_num, rx_slot);
	return 0;
}

static int tapan_get_channel_map(struct snd_soc_dai *dai,
				 unsigned int *tx_num, unsigned int *tx_slot,
				 unsigned int *rx_num, unsigned int *rx_slot)

{
	struct tapan_priv *tapan_p = snd_soc_codec_get_drvdata(dai->codec);
	u32 i = 0;
	struct wcd9xxx_ch *ch;

	switch (dai->id) {
	case AIF1_PB:
	case AIF2_PB:
	case AIF3_PB:
		if (!rx_slot || !rx_num) {
			pr_err("%s: Invalid rx_slot %pK or rx_num %pK\n",
				 __func__, rx_slot, rx_num);
			return -EINVAL;
		}
		list_for_each_entry(ch, &tapan_p->dai[dai->id].wcd9xxx_ch_list,
				    list) {
			dev_dbg(dai->codec->dev, "%s: rx_slot[%d] %d, ch->ch_num %d\n",
				 __func__, i, rx_slot[i], ch->ch_num);
			rx_slot[i++] = ch->ch_num;
		}
		dev_dbg(dai->codec->dev, "%s: rx_num %d\n", __func__, i);
		*rx_num = i;
		break;
	case AIF1_CAP:
	case AIF2_CAP:
	case AIF3_CAP:
		if (!tx_slot || !tx_num) {
			pr_err("%s: Invalid tx_slot %pK or tx_num %pK\n",
				 __func__, tx_slot, tx_num);
			return -EINVAL;
		}
		list_for_each_entry(ch, &tapan_p->dai[dai->id].wcd9xxx_ch_list,
				    list) {
			dev_dbg(dai->codec->dev, "%s: tx_slot[%d] %d, ch->ch_num %d\n",
				 __func__, i, tx_slot[i], ch->ch_num);
			tx_slot[i++] = ch->ch_num;
		}
		dev_dbg(dai->codec->dev, "%s: tx_num %d\n", __func__, i);
		*tx_num = i;
		break;

	default:
		pr_err("%s: Invalid DAI ID %x\n", __func__, dai->id);
		break;
	}

	return 0;
}

static int tapan_set_interpolator_rate(struct snd_soc_dai *dai,
	u8 rx_fs_rate_reg_val, u32 compander_fs, u32 sample_rate)
{
	u32 j;
	u8 rx_mix1_inp;
	u16 rx_mix_1_reg_1, rx_mix_1_reg_2;
	u16 rx_fs_reg;
	u8 rx_mix_1_reg_1_val, rx_mix_1_reg_2_val;
	u8 rdac5_mux;
	struct snd_soc_codec *codec = dai->codec;
	struct wcd9xxx_ch *ch;
	struct tapan_priv *tapan = snd_soc_codec_get_drvdata(codec);

	list_for_each_entry(ch, &tapan->dai[dai->id].wcd9xxx_ch_list, list) {
		/* for RX port starting from 16 instead of 10 like tabla */
		rx_mix1_inp = ch->port + RX_MIX1_INP_SEL_RX1 -
			      TAPAN_TX_PORT_NUMBER;
		if ((rx_mix1_inp < RX_MIX1_INP_SEL_RX1) ||
			(rx_mix1_inp > RX_MIX1_INP_SEL_RX5)) {
			pr_err("%s: Invalid TAPAN_RX%u port. Dai ID is %d\n",
				__func__,  rx_mix1_inp - 5 , dai->id);
			return -EINVAL;
		}

		rx_mix_1_reg_1 = TAPAN_A_CDC_CONN_RX1_B1_CTL;

		rdac5_mux = snd_soc_read(codec, TAPAN_A_CDC_CONN_MISC);
		rdac5_mux = (rdac5_mux & 0x04) >> 2;

		for (j = 0; j < NUM_INTERPOLATORS; j++) {
			rx_mix_1_reg_2 = rx_mix_1_reg_1 + 1;

			rx_mix_1_reg_1_val = snd_soc_read(codec,
							  rx_mix_1_reg_1);
			rx_mix_1_reg_2_val = snd_soc_read(codec,
							  rx_mix_1_reg_2);

			if (((rx_mix_1_reg_1_val & 0x0F) == rx_mix1_inp) ||
			    (((rx_mix_1_reg_1_val >> 4) & 0x0F)
				== rx_mix1_inp) ||
			    ((rx_mix_1_reg_2_val & 0x0F) == rx_mix1_inp)) {

				rx_fs_reg = TAPAN_A_CDC_RX1_B5_CTL + 8 * j;

				dev_dbg(codec->dev, "%s: AIF_PB DAI(%d) connected to RX%u\n",
					__func__, dai->id, j + 1);

				dev_dbg(codec->dev, "%s: set RX%u sample rate to %u\n",
					__func__, j + 1, sample_rate);

				snd_soc_update_bits(codec, rx_fs_reg,
						0xE0, rx_fs_rate_reg_val);

				if (comp_rx_path[j] < COMPANDER_MAX) {
					if ((j == 3) && (rdac5_mux == 1))
						tapan->comp_fs[COMPANDER_0] =
							compander_fs;
					else
						tapan->comp_fs[comp_rx_path[j]]
							= compander_fs;
				}
			}
			if (j <= 1)
				rx_mix_1_reg_1 += 3;
			else
				rx_mix_1_reg_1 += 2;
		}
	}
	return 0;
}

static int tapan_set_decimator_rate(struct snd_soc_dai *dai,
	u8 tx_fs_rate_reg_val, u32 sample_rate)
{
	struct snd_soc_codec *codec = dai->codec;
	struct wcd9xxx_ch *ch;
	struct tapan_priv *tapan = snd_soc_codec_get_drvdata(codec);
	u32 tx_port;
	u16 tx_port_reg, tx_fs_reg;
	u8 tx_port_reg_val;
	s8 decimator;

	list_for_each_entry(ch, &tapan->dai[dai->id].wcd9xxx_ch_list, list) {

		tx_port = ch->port + 1;
		dev_dbg(codec->dev, "%s: dai->id = %d, tx_port = %d",
			__func__, dai->id, tx_port);

		if ((tx_port < 1) || (tx_port > TAPAN_SLIM_CODEC_TX_PORTS)) {
			pr_err("%s: Invalid SLIM TX%u port. DAI ID is %d\n",
				__func__, tx_port, dai->id);
			return -EINVAL;
		}

		tx_port_reg = TAPAN_A_CDC_CONN_TX_SB_B1_CTL + (tx_port - 1);
		tx_port_reg_val =  snd_soc_read(codec, tx_port_reg);

		decimator = 0;

		tx_port_reg_val =  tx_port_reg_val & 0x0F;

		if ((tx_port_reg_val >= 0x8) &&
			    (tx_port_reg_val <= 0x11)) {

				decimator = (tx_port_reg_val - 0x8) + 1;
		}


		if (decimator) { /* SLIM_TX port has a DEC as input */

			tx_fs_reg = TAPAN_A_CDC_TX1_CLK_FS_CTL +
				    8 * (decimator - 1);

			dev_dbg(codec->dev, "%s: set DEC%u (-> SLIM_TX%u) rate to %u\n",
				__func__, decimator, tx_port, sample_rate);

			snd_soc_update_bits(codec, tx_fs_reg, 0x07,
					    tx_fs_rate_reg_val);

		} else {
			if ((tx_port_reg_val >= 0x1) &&
			    (tx_port_reg_val <= 0x4)) {

				dev_dbg(codec->dev, "%s: RMIX%u going to SLIM TX%u\n",
					__func__, tx_port_reg_val, tx_port);

			} else if  ((tx_port_reg_val >= 0x8) &&
				    (tx_port_reg_val <= 0x11)) {

				pr_err("%s: ERROR: Should not be here\n",
				       __func__);
				pr_err("%s: ERROR: DEC connected to SLIM TX%u\n",
					__func__, tx_port);
				return -EINVAL;

			} else if (tx_port_reg_val == 0) {
				dev_dbg(codec->dev, "%s: no signal to SLIM TX%u\n",
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

static void tapan_set_rxsb_port_format(struct snd_pcm_hw_params *params,
				       struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tapan_priv *tapan_p = snd_soc_codec_get_drvdata(codec);
	struct wcd9xxx_codec_dai_data *cdc_dai;
	struct wcd9xxx_ch *ch;
	int port;
	u8 bit_sel;
	u16 sb_ctl_reg, field_shift;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		bit_sel = 0x2;
		tapan_p->dai[dai->id].bit_width = 16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		bit_sel = 0x0;
		tapan_p->dai[dai->id].bit_width = 24;
		break;
	default:
		dev_err(codec->dev, "Invalid format %x\n",
			params_format(params));
		return;
	}

	cdc_dai = &tapan_p->dai[dai->id];

	list_for_each_entry(ch, &cdc_dai->wcd9xxx_ch_list, list) {
		port = wcd9xxx_get_slave_port(ch->ch_num);

		if (IS_ERR_VALUE(port) ||
		    !TAPAN_VALIDATE_RX_SBPORT_RANGE(port)) {
			dev_warn(codec->dev,
				 "%s: invalid port ID %d returned for RX DAI\n",
				 __func__, port);
			return;
		}

		port = TAPAN_CONVERT_RX_SBPORT_ID(port);

		if (port <= 3) {
			sb_ctl_reg = TAPAN_A_CDC_CONN_RX_SB_B1_CTL;
			field_shift = port << 1;
		} else if (port <= 4) {
			sb_ctl_reg = TAPAN_A_CDC_CONN_RX_SB_B2_CTL;
			field_shift = (port - 4) << 1;
		} else { /* should not happen */
			dev_warn(codec->dev,
				 "%s: bad port ID %d\n", __func__, port);
			return;
		}

		dev_dbg(codec->dev, "%s: sb_ctl_reg %x field_shift %x\n"
			"bit_sel %x\n", __func__, sb_ctl_reg, field_shift,
			bit_sel);
		snd_soc_update_bits(codec, sb_ctl_reg, 0x3 << field_shift,
				    bit_sel << field_shift);
	}
}


static int tapan_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params,
			    struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tapan_priv *tapan = snd_soc_codec_get_drvdata(dai->codec);
	u8 tx_fs_rate, rx_fs_rate;
	u32 compander_fs;
	int ret;

	dev_dbg(dai->codec->dev, "%s: dai_name = %s DAI-ID %x rate %d num_ch %d\n",
		 __func__, dai->name, dai->id,
		 params_rate(params), params_channels(params));

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

	switch (substream->stream) {
	case SNDRV_PCM_STREAM_CAPTURE:
		ret = tapan_set_decimator_rate(dai, tx_fs_rate,
					       params_rate(params));
		if (ret < 0) {
			pr_err("%s: set decimator rate failed %d\n", __func__,
				ret);
			return ret;
		}

		if (tapan->intf_type == WCD9XXX_INTERFACE_TYPE_I2C) {
			switch (params_format(params)) {
			case SNDRV_PCM_FORMAT_S16_LE:
				snd_soc_update_bits(codec,
					TAPAN_A_CDC_CLK_I2S_CTL,
					0x20, 0x20);
				break;
			case SNDRV_PCM_FORMAT_S32_LE:
				snd_soc_update_bits(codec,
					TAPAN_A_CDC_CLK_I2S_CTL,
					0x20, 0x00);
				break;
			default:
				pr_err("invalid format\n");
				break;
			}
			snd_soc_update_bits(codec, TAPAN_A_CDC_CLK_I2S_CTL,
					    0x07, tx_fs_rate);
		} else {
			tapan->dai[dai->id].rate   = params_rate(params);
		}
		break;

	case SNDRV_PCM_STREAM_PLAYBACK:
		ret = tapan_set_interpolator_rate(dai, rx_fs_rate,
						  compander_fs,
						  params_rate(params));
		if (ret < 0) {
			dev_err(codec->dev, "%s: set decimator rate failed %d\n",
				__func__, ret);
			return ret;
		}
		if (tapan->intf_type == WCD9XXX_INTERFACE_TYPE_I2C) {
			switch (params_format(params)) {
			case SNDRV_PCM_FORMAT_S16_LE:
				snd_soc_update_bits(codec,
					TAPAN_A_CDC_CLK_I2S_CTL,
					0x20, 0x20);
				break;
			case SNDRV_PCM_FORMAT_S32_LE:
				snd_soc_update_bits(codec,
					TAPAN_A_CDC_CLK_I2S_CTL,
					0x20, 0x00);
				break;
			default:
				dev_err(codec->dev, "invalid format\n");
				break;
			}
			snd_soc_update_bits(codec, TAPAN_A_CDC_CLK_I2S_CTL,
					    0x03, (rx_fs_rate >> 0x05));
		} else {
			tapan_set_rxsb_port_format(params, dai);
			tapan->dai[dai->id].rate   = params_rate(params);
		}
		break;
	default:
		dev_err(codec->dev, "%s: Invalid stream type %d\n", __func__,
			substream->stream);
		return -EINVAL;
	}

	return 0;
}

int tapan_digital_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = NULL;
	u16 tx_vol_ctl_reg = 0;
	u8 decimator = 0, i;
	struct tapan_priv *tapan_p;

	pr_debug("%s: Digital Mute val = %d\n", __func__, mute);

	if (!dai || !dai->codec) {
		pr_err("%s: Invalid params\n", __func__);
		return -EINVAL;
	}
	codec = dai->codec;
	tapan_p = snd_soc_codec_get_drvdata(codec);

	if (dai->id != AIF1_CAP) {
		dev_dbg(codec->dev, "%s: Not capture use case skip\n",
		__func__);
		return 0;
	}

	mute = (mute) ? 1 : 0;
	if (!mute) {
		/*
		 * 5 ms is an emperical value for the mute time
		 * that was arrived by checking the pop level
		 * to be inaudible
		 */
		usleep_range(5000, 5010);
	}

	for (i = 0; i < NUM_DECIMATORS; i++) {
		if (tapan_p->dec_active[i])
			decimator = i + 1;
		if (decimator && decimator <= NUM_DECIMATORS) {
			pr_debug("%s: Mute = %d Decimator = %d", __func__,
					mute, decimator);
			tx_vol_ctl_reg = TAPAN_A_CDC_TX1_VOL_CTL_CFG +
				8 * (decimator - 1);
			snd_soc_update_bits(codec, tx_vol_ctl_reg, 0x01, mute);
		}
		decimator = 0;
	}
	return 0;
}

static struct snd_soc_dai_ops tapan_dai_ops = {
	.startup = tapan_startup,
	.shutdown = tapan_shutdown,
	.hw_params = tapan_hw_params,
	.set_sysclk = tapan_set_dai_sysclk,
	.set_fmt = tapan_set_dai_fmt,
	.set_channel_map = tapan_set_channel_map,
	.get_channel_map = tapan_get_channel_map,
	.digital_mute = tapan_digital_mute,
};

static struct snd_soc_dai_driver tapan9302_dai[] = {
	{
		.name = "tapan9302_rx1",
		.id = AIF1_PB,
		.playback = {
			.stream_name = "AIF1 Playback",
			.rates = WCD9302_RATES,
			.formats = TAPAN_FORMATS,
			.rate_max = 48000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &tapan_dai_ops,
	},
	{
		.name = "tapan9302_tx1",
		.id = AIF1_CAP,
		.capture = {
			.stream_name = "AIF1 Capture",
			.rates = WCD9302_RATES,
			.formats = TAPAN_FORMATS,
			.rate_max = 48000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 4,
		},
		.ops = &tapan_dai_ops,
	},
	{
		.name = "tapan9302_rx2",
		.id = AIF2_PB,
		.playback = {
			.stream_name = "AIF2 Playback",
			.rates = WCD9302_RATES,
			.formats = TAPAN_FORMATS,
			.rate_min = 8000,
			.rate_max = 48000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &tapan_dai_ops,
	},
	{
		.name = "tapan9302_tx2",
		.id = AIF2_CAP,
		.capture = {
			.stream_name = "AIF2 Capture",
			.rates = WCD9302_RATES,
			.formats = TAPAN_FORMATS,
			.rate_max = 48000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 4,
		},
		.ops = &tapan_dai_ops,
	},
	{
		.name = "tapan9302_rx3",
		.id = AIF3_PB,
		.playback = {
			.stream_name = "AIF3 Playback",
			.rates = WCD9302_RATES,
			.formats = TAPAN_FORMATS,
			.rate_min = 8000,
			.rate_max = 48000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &tapan_dai_ops,
	},
	{
		.name = "tapan9302_tx3",
		.id = AIF3_CAP,
		.capture = {
			.stream_name = "AIF3 Capture",
			.rates = WCD9302_RATES,
			.formats = TAPAN_FORMATS,
			.rate_max = 48000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &tapan_dai_ops,
	},
};

static struct snd_soc_dai_driver tapan_dai[] = {
	{
		.name = "tapan_rx1",
		.id = AIF1_PB,
		.playback = {
			.stream_name = "AIF1 Playback",
			.rates = WCD9306_RATES,
			.formats = TAPAN_FORMATS_S16_S24_LE,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &tapan_dai_ops,
	},
	{
		.name = "tapan_tx1",
		.id = AIF1_CAP,
		.capture = {
			.stream_name = "AIF1 Capture",
			.rates = WCD9306_RATES,
			.formats = TAPAN_FORMATS,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 4,
		},
		.ops = &tapan_dai_ops,
	},
	{
		.name = "tapan_rx2",
		.id = AIF2_PB,
		.playback = {
			.stream_name = "AIF2 Playback",
			.rates = WCD9306_RATES,
			.formats = TAPAN_FORMATS_S16_S24_LE,
			.rate_min = 8000,
			.rate_max = 192000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &tapan_dai_ops,
	},
	{
		.name = "tapan_tx2",
		.id = AIF2_CAP,
		.capture = {
			.stream_name = "AIF2 Capture",
			.rates = WCD9306_RATES,
			.formats = TAPAN_FORMATS,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 4,
		},
		.ops = &tapan_dai_ops,
	},
	{
		.name = "tapan_rx3",
		.id = AIF3_PB,
		.playback = {
			.stream_name = "AIF3 Playback",
			.rates = WCD9306_RATES,
			.formats = TAPAN_FORMATS_S16_S24_LE,
			.rate_min = 8000,
			.rate_max = 192000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &tapan_dai_ops,
	},
	{
		.name = "tapan_tx3",
		.id = AIF3_CAP,
		.capture = {
			.stream_name = "AIF3 Capture",
			.rates = WCD9306_RATES,
			.formats = TAPAN_FORMATS,
			.rate_max = 48000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &tapan_dai_ops,
	},
};

static struct snd_soc_dai_driver tapan_i2s_dai[] = {
	{
		.name = "tapan_i2s_rx1",
		.id = AIF1_PB,
		.playback = {
			.stream_name = "AIF1 Playback",
			.rates = WCD9306_RATES,
			.formats = TAPAN_FORMATS,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 4,
		},
		.ops = &tapan_dai_ops,
	},
	{
		.name = "tapan_i2s_tx1",
		.id = AIF1_CAP,
		.capture = {
			.stream_name = "AIF1 Capture",
			.rates = WCD9306_RATES,
			.formats = TAPAN_FORMATS,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 4,
		},
		.ops = &tapan_dai_ops,
	},
};

static int tapan_codec_enable_slim_chmask(struct wcd9xxx_codec_dai_data *dai,
					  bool up)
{
	int ret = 0;
	struct wcd9xxx_ch *ch;

	if (up) {
		list_for_each_entry(ch, &dai->wcd9xxx_ch_list, list) {
			ret = wcd9xxx_get_slave_port(ch->ch_num);
			if (ret < 0) {
				pr_debug("%s: Invalid slave port ID: %d\n",
				       __func__, ret);
				ret = -EINVAL;
			} else {
				set_bit(ret, &dai->ch_mask);
			}
		}
	} else {
		ret = wait_event_timeout(dai->dai_wait, (dai->ch_mask == 0),
					 msecs_to_jiffies(
						     TAPAN_SLIM_CLOSE_TIMEOUT));
		if (!ret) {
			pr_debug("%s: Slim close tx/rx wait timeout\n",
					 __func__);
			ret = -ETIMEDOUT;
		} else {
			ret = 0;
		}
	}
	return ret;
}

static int tapan_codec_enable_slimrx(struct snd_soc_dapm_widget *w,
				     struct snd_kcontrol *kcontrol,
				     int event)
{
	struct wcd9xxx *core;
	struct snd_soc_codec *codec = w->codec;
	struct tapan_priv *tapan_p = snd_soc_codec_get_drvdata(codec);
	int ret = 0;
	struct wcd9xxx_codec_dai_data *dai;

	core = dev_get_drvdata(codec->dev->parent);

	if (core == NULL) {
		dev_err(codec->dev, "%s: core is null\n",
				__func__);
		return -EINVAL;
	}

	dev_dbg(codec->dev, "%s: event called! codec name %s\n",
		__func__, w->codec->name);
	dev_dbg(codec->dev, "%s: num_dai %d stream name %s event %d\n",
		__func__, w->codec->num_dai, w->sname, event);

	/* Execute the callback only if interface type is slimbus */
	if (tapan_p->intf_type != WCD9XXX_INTERFACE_TYPE_SLIMBUS)
		return 0;

	dai = &tapan_p->dai[w->shift];
	dev_dbg(codec->dev, "%s: w->name %s w->shift %d event %d\n",
		 __func__, w->name, w->shift, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		dai->bus_down_in_recovery = false;
		(void) tapan_codec_enable_slim_chmask(dai, true);
		ret = wcd9xxx_cfg_slim_sch_rx(core, &dai->wcd9xxx_ch_list,
					      dai->rate, dai->bit_width,
					      &dai->grph);
		break;
	case SND_SOC_DAPM_POST_PMD:
		ret = wcd9xxx_close_slim_sch_rx(core, &dai->wcd9xxx_ch_list,
						dai->grph);
		if (!dai->bus_down_in_recovery)
			ret = tapan_codec_enable_slim_chmask(dai, false);
		if (ret < 0) {
			ret = wcd9xxx_disconnect_port(core,
						      &dai->wcd9xxx_ch_list,
						      dai->grph);
			dev_dbg(codec->dev, "%s: Disconnect RX port, ret = %d\n",
				 __func__, ret);
		}
		dai->bus_down_in_recovery = false;
		break;
	}
	return ret;
}

static int tapan_codec_enable_slimtx(struct snd_soc_dapm_widget *w,
				     struct snd_kcontrol *kcontrol,
				     int event)
{
	struct wcd9xxx *core;
	struct snd_soc_codec *codec = w->codec;
	struct tapan_priv *tapan_p = snd_soc_codec_get_drvdata(codec);
	u32  ret = 0;
	struct wcd9xxx_codec_dai_data *dai;

	core = dev_get_drvdata(codec->dev->parent);

	dev_dbg(codec->dev, "%s: event called! codec name %s\n",
		__func__, w->codec->name);
	dev_dbg(codec->dev, "%s: num_dai %d stream name %s\n",
		__func__, w->codec->num_dai, w->sname);
	/* Execute the callback only if interface type is slimbus */
	if (tapan_p->intf_type != WCD9XXX_INTERFACE_TYPE_SLIMBUS)
		return 0;

	dev_dbg(codec->dev, "%s(): w->name %s event %d w->shift %d\n",
		__func__, w->name, event, w->shift);

	dai = &tapan_p->dai[w->shift];
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		dai->bus_down_in_recovery = false;
		(void) tapan_codec_enable_slim_chmask(dai, true);
		ret = wcd9xxx_cfg_slim_sch_tx(core, &dai->wcd9xxx_ch_list,
					      dai->rate, dai->bit_width,
					      &dai->grph);
		break;
	case SND_SOC_DAPM_POST_PMD:
		ret = wcd9xxx_close_slim_sch_tx(core, &dai->wcd9xxx_ch_list,
						dai->grph);
		if (!dai->bus_down_in_recovery)
			ret = tapan_codec_enable_slim_chmask(dai, false);
		if (ret < 0) {
			ret = wcd9xxx_disconnect_port(core,
						      &dai->wcd9xxx_ch_list,
						      dai->grph);
			dev_dbg(codec->dev, "%s: Disconnect RX port, ret = %d\n",
				 __func__, ret);
		}
		dai->bus_down_in_recovery = false;
		break;
	}
	return ret;
}


static int tapan_codec_enable_ear_pa(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct tapan_priv *tapan_p = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s %s %d\n", __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		wcd9xxx_clsh_fsm(codec, &tapan_p->clsh_d,
						 WCD9XXX_CLSH_STATE_EAR,
						 WCD9XXX_CLSH_REQ_ENABLE,
						 WCD9XXX_CLSH_EVENT_POST_PA);

		usleep_range(5000, 5010);
		break;
	case SND_SOC_DAPM_POST_PMD:
		usleep_range(5000, 5010);
		snd_soc_update_bits(codec, TAPAN_A_RX_EAR_EN, 0x40, 0x00);
		wcd9xxx_clsh_fsm(codec, &tapan_p->clsh_d,
						 WCD9XXX_CLSH_STATE_EAR,
						 WCD9XXX_CLSH_REQ_DISABLE,
						 WCD9XXX_CLSH_EVENT_POST_PA);
	}
	return 0;
}

static int tapan_codec_ear_dac_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct tapan_priv *tapan_p = snd_soc_codec_get_drvdata(codec);

	dev_dbg(codec->dev, "%s %s %d\n", __func__, w->name, event);

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		wcd9xxx_clsh_fsm(codec, &tapan_p->clsh_d,
						 WCD9XXX_CLSH_STATE_EAR,
						 WCD9XXX_CLSH_REQ_ENABLE,
						 WCD9XXX_CLSH_EVENT_PRE_DAC);
		break;
	}

	return 0;
}

static int tapan_codec_iir_mux_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	pr_debug("%s: event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		snd_soc_write(codec, w->reg, snd_soc_read(codec, w->reg));
		break;
	case SND_SOC_DAPM_POST_PMD:
		snd_soc_write(codec, w->reg, snd_soc_read(codec, w->reg));
		break;
	}
	return 0;
}

static int tapan_codec_dsm_mux_event(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	u8 reg_val, zoh_mux_val = 0x00;

	dev_dbg(codec->dev, "%s: event = %d\n", __func__, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		reg_val = snd_soc_read(codec, TAPAN_A_CDC_CONN_CLSH_CTL);

		if ((reg_val & 0x30) == 0x10)
			zoh_mux_val = 0x04;
		else if ((reg_val & 0x30) == 0x20)
			zoh_mux_val = 0x08;

		if (zoh_mux_val != 0x00)
			snd_soc_update_bits(codec,
					TAPAN_A_CDC_CONN_CLSH_CTL,
					0x0C, zoh_mux_val);
		break;

	case SND_SOC_DAPM_POST_PMD:
		snd_soc_update_bits(codec, TAPAN_A_CDC_CONN_CLSH_CTL,
							0x0C, 0x00);
		break;
	}
	return 0;
}

static int tapan_codec_enable_anc_ear(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	int ret = 0;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		ret = tapan_codec_enable_anc(w, kcontrol, event);
		msleep(50);
		snd_soc_update_bits(codec, TAPAN_A_RX_EAR_EN, 0x10, 0x10);
		break;
	case SND_SOC_DAPM_POST_PMU:
		ret = tapan_codec_enable_ear_pa(w, kcontrol, event);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		snd_soc_update_bits(codec, TAPAN_A_RX_EAR_EN, 0x10, 0x00);
		msleep(40);
		ret |= tapan_codec_enable_anc(w, kcontrol, event);
		break;
	case SND_SOC_DAPM_POST_PMD:
		ret = tapan_codec_enable_ear_pa(w, kcontrol, event);
		break;
	}
	return ret;
}

static int tapan_codec_chargepump_vdd_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct tapan_priv *priv = snd_soc_codec_get_drvdata(codec);
	int ret = 0, i;

	pr_info("%s: event = %d\n", __func__, event);


	if (!priv->cp_regulators[CP_REG_BUCK]
			&& !priv->cp_regulators[CP_REG_BHELPER]) {
		pr_err("%s: No power supply defined for ChargePump\n",
				__func__);
		return -EINVAL;
	}

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		for (i = 0; i < CP_REG_MAX ; i++) {
			if (!priv->cp_regulators[i])
				continue;

			ret = regulator_enable(priv->cp_regulators[i]);
			if (ret) {
				pr_err("%s: CP Regulator enable failed, index = %d\n",
						__func__, i);
				continue;
			} else {
				pr_debug("%s: Enabled CP regulator, index %d\n",
					__func__, i);
			}
		}
		break;
	case SND_SOC_DAPM_POST_PMD:
		for (i = 0; i < CP_REG_MAX; i++) {
			if (!priv->cp_regulators[i])
				continue;

			ret = regulator_disable(priv->cp_regulators[i]);
			if (ret) {
				pr_err("%s: CP Regulator disable failed, index = %d\n",
						__func__, i);
				return ret;
			} else {
				pr_debug("%s: Disabled CP regulator %d\n",
						__func__, i);
			}
		}
		break;
	}
	return 0;
}

static int tapan_codec_set_iir_gain(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	int value = 0;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		value = snd_soc_read(codec, TAPAN_A_CDC_IIR1_GAIN_B1_CTL);
		snd_soc_write(codec, TAPAN_A_CDC_IIR1_GAIN_B1_CTL, value);
		break;
	default:
		pr_err("%s: event = %d not expected\n", __func__, event);
	}
	return 0;
}

static const struct snd_soc_dapm_widget tapan_9306_dapm_widgets[] = {
	/* RX4 MIX1 mux inputs */
	SND_SOC_DAPM_MUX("RX4 MIX1 INP1", SND_SOC_NOPM, 0, 0,
		&rx4_mix1_inp1_mux),
	SND_SOC_DAPM_MUX("RX4 MIX1 INP2", SND_SOC_NOPM, 0, 0,
		&rx4_mix1_inp2_mux),
	SND_SOC_DAPM_MUX("RX4 MIX1 INP3", SND_SOC_NOPM, 0, 0,
		&rx4_mix1_inp3_mux),

	/* RX4 MIX2 mux inputs */
	SND_SOC_DAPM_MUX("RX4 MIX2 INP1", SND_SOC_NOPM, 0, 0,
		&rx4_mix2_inp1_mux),
	SND_SOC_DAPM_MUX("RX4 MIX2 INP2", SND_SOC_NOPM, 0, 0,
		&rx4_mix2_inp2_mux),

	SND_SOC_DAPM_MIXER("RX4 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MIXER_E("RX4 MIX2", TAPAN_A_CDC_CLK_RX_B1_CTL, 3, 0, NULL,
		0, tapan_codec_enable_interpolator, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_MUX_E("DEC3 MUX", TAPAN_A_CDC_CLK_TX_CLK_EN_B1_CTL, 2, 0,
		&dec3_mux, tapan_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("DEC4 MUX", TAPAN_A_CDC_CLK_TX_CLK_EN_B1_CTL, 3, 0,
		&dec4_mux, tapan_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY("COMP0_CLK", SND_SOC_NOPM, 0, 0,
		tapan_config_compander, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_SUPPLY("COMP1_CLK", SND_SOC_NOPM, 1, 0,
		tapan_config_compander, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_SUPPLY("COMP2_CLK", SND_SOC_NOPM, 2, 0,
		tapan_config_compander, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_INPUT("AMIC5"),
	SND_SOC_DAPM_ADC_E("ADC5", NULL, TAPAN_A_TX_5_EN, 7, 0,
		tapan_codec_enable_adc, SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_MUX("ANC1 MUX", SND_SOC_NOPM, 0, 0, &anc1_mux),
	SND_SOC_DAPM_MUX("ANC2 MUX", SND_SOC_NOPM, 0, 0, &anc2_mux),

	SND_SOC_DAPM_OUTPUT("ANC HEADPHONE"),
	SND_SOC_DAPM_PGA_E("ANC HPHL", SND_SOC_NOPM, 5, 0, NULL, 0,
		tapan_codec_enable_anc_hph,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMD | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_PGA_E("ANC HPHR", SND_SOC_NOPM, 4, 0, NULL, 0,
		tapan_codec_enable_anc_hph, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD |
		SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_OUTPUT("ANC EAR"),
	SND_SOC_DAPM_PGA_E("ANC EAR PA", SND_SOC_NOPM, 0, 0, NULL, 0,
		tapan_codec_enable_anc_ear,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_PRE_PMD |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX("ANC1 FB MUX", SND_SOC_NOPM, 0, 0, &anc1_fb_mux),

	SND_SOC_DAPM_MICBIAS_E("MIC BIAS3 External", SND_SOC_NOPM, 7, 0,
		tapan_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS3 Internal1", SND_SOC_NOPM, 7, 0,
		tapan_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS3 Internal2", SND_SOC_NOPM, 7, 0,
		tapan_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("DMIC3", NULL, SND_SOC_NOPM, 0, 0,
		tapan_codec_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("DMIC4", NULL, SND_SOC_NOPM, 0, 0,
		tapan_codec_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),
};

/* Todo: Have seperate dapm widgets for I2S and Slimbus.
 * Might Need to have callbacks registered only for slimbus
 */
static const struct snd_soc_dapm_widget tapan_common_dapm_widgets[] = {

	SND_SOC_DAPM_AIF_IN_E("AIF1 PB", "AIF1 Playback", 0, SND_SOC_NOPM,
				AIF1_PB, 0, tapan_codec_enable_slimrx,
				SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_IN_E("AIF2 PB", "AIF2 Playback", 0, SND_SOC_NOPM,
				AIF2_PB, 0, tapan_codec_enable_slimrx,
				SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_AIF_IN_E("AIF3 PB", "AIF3 Playback", 0, SND_SOC_NOPM,
				AIF3_PB, 0, tapan_codec_enable_slimrx,
				SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX("SLIM RX1 MUX", SND_SOC_NOPM, TAPAN_RX1, 0,
				&slim_rx_mux[TAPAN_RX1]),
	SND_SOC_DAPM_MUX("SLIM RX2 MUX", SND_SOC_NOPM, TAPAN_RX2, 0,
				&slim_rx_mux[TAPAN_RX2]),
	SND_SOC_DAPM_MUX("SLIM RX3 MUX", SND_SOC_NOPM, TAPAN_RX3, 0,
				&slim_rx_mux[TAPAN_RX3]),
	SND_SOC_DAPM_MUX("SLIM RX4 MUX", SND_SOC_NOPM, TAPAN_RX4, 0,
				&slim_rx_mux[TAPAN_RX4]),
	SND_SOC_DAPM_MUX("SLIM RX5 MUX", SND_SOC_NOPM, TAPAN_RX5, 0,
				&slim_rx_mux[TAPAN_RX5]),

	SND_SOC_DAPM_MIXER("SLIM RX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SLIM RX2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SLIM RX3", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SLIM RX4", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("SLIM RX5", SND_SOC_NOPM, 0, 0, NULL, 0),


	/* RX1 MIX1 mux inputs */
	SND_SOC_DAPM_MUX("RX1 MIX1 INP1", SND_SOC_NOPM, 0, 0,
		&rx_mix1_inp1_mux),
	SND_SOC_DAPM_MUX("RX1 MIX1 INP2", SND_SOC_NOPM, 0, 0,
		&rx_mix1_inp2_mux),
	SND_SOC_DAPM_MUX("RX1 MIX1 INP3", SND_SOC_NOPM, 0, 0,
		&rx_mix1_inp3_mux),

	/* RX2 MIX1 mux inputs */
	SND_SOC_DAPM_MUX("RX2 MIX1 INP1", SND_SOC_NOPM, 0, 0,
		&rx2_mix1_inp1_mux),
	SND_SOC_DAPM_MUX("RX2 MIX1 INP2", SND_SOC_NOPM, 0, 0,
		&rx2_mix1_inp2_mux),
	SND_SOC_DAPM_MUX("RX2 MIX1 INP3", SND_SOC_NOPM, 0, 0,
		&rx2_mix1_inp2_mux),

	/* RX3 MIX1 mux inputs */
	SND_SOC_DAPM_MUX("RX3 MIX1 INP1", SND_SOC_NOPM, 0, 0,
		&rx3_mix1_inp1_mux),
	SND_SOC_DAPM_MUX("RX3 MIX1 INP2", SND_SOC_NOPM, 0, 0,
		&rx3_mix1_inp2_mux),
	SND_SOC_DAPM_MUX("RX3 MIX1 INP3", SND_SOC_NOPM, 0, 0,
		&rx3_mix1_inp3_mux),

	/* RX1 MIX2 mux inputs */
	SND_SOC_DAPM_MUX("RX1 MIX2 INP1", SND_SOC_NOPM, 0, 0,
		&rx1_mix2_inp1_mux),
	SND_SOC_DAPM_MUX("RX1 MIX2 INP2", SND_SOC_NOPM, 0, 0,
		&rx1_mix2_inp2_mux),

	/* RX2 MIX2 mux inputs */
	SND_SOC_DAPM_MUX("RX2 MIX2 INP1", SND_SOC_NOPM, 0, 0,
		&rx2_mix2_inp1_mux),
	SND_SOC_DAPM_MUX("RX2 MIX2 INP2", SND_SOC_NOPM, 0, 0,
		&rx2_mix2_inp2_mux),

	SND_SOC_DAPM_MIXER("RX1 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX2 MIX1", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MIXER("RX1 MIX2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER("RX2 MIX2", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_MIXER_E("RX3 MIX1", TAPAN_A_CDC_CLK_RX_B1_CTL, 2, 0, NULL,
		0, tapan_codec_enable_interpolator, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_VIRT_MUX_E("RX1 INTERPOLATOR",
		TAPAN_A_CDC_CLK_RX_B1_CTL, 0, 0,
		&rx1_interpolator, tapan_codec_enable_interpolator,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),
	SND_SOC_DAPM_VIRT_MUX_E("RX2 INTERPOLATOR",
		TAPAN_A_CDC_CLK_RX_B1_CTL, 1, 0,
		&rx2_interpolator, tapan_codec_enable_interpolator,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_MIXER("RX1 CHAIN", TAPAN_A_CDC_RX1_B6_CTL, 5, 0,
						NULL, 0),

	SND_SOC_DAPM_MIXER_E("RX2 CHAIN", SND_SOC_NOPM, 0, 0, NULL,
		0, tapan_codec_rx_dem_select, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("CLASS_H_DSM MUX", SND_SOC_NOPM, 0, 0,
		&class_h_dsm_mux, tapan_codec_dsm_mux_event,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	/* RX Bias */
	SND_SOC_DAPM_SUPPLY("RX_BIAS", SND_SOC_NOPM, 0, 0,
		tapan_codec_enable_rx_bias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	/* CDC_CP_VDD */
	SND_SOC_DAPM_SUPPLY("CDC_CP_VDD", SND_SOC_NOPM, 0, 0,
		tapan_codec_chargepump_vdd_event, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	/*EAR */
	SND_SOC_DAPM_PGA_E("EAR PA", TAPAN_A_RX_EAR_EN, 4, 0, NULL, 0,
			tapan_codec_enable_ear_pa, SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MIXER_E("DAC1", TAPAN_A_RX_EAR_EN, 6, 0, dac1_switch,
		ARRAY_SIZE(dac1_switch), tapan_codec_ear_dac_event,
		SND_SOC_DAPM_PRE_PMU),

	/* Headphone Left */
	SND_SOC_DAPM_PGA_E("HPHL", TAPAN_A_RX_HPH_CNP_EN, 5, 0, NULL, 0,
		tapan_hph_pa_event, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MIXER_E("HPHL DAC", TAPAN_A_RX_HPH_L_DAC_CTL, 7, 0,
		hphl_switch, ARRAY_SIZE(hphl_switch), tapan_hphl_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	/* Headphone Right */
	SND_SOC_DAPM_PGA_E("HPHR", TAPAN_A_RX_HPH_CNP_EN, 4, 0, NULL, 0,
		tapan_hph_pa_event, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU |	SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_DAC_E("HPHR DAC", NULL, TAPAN_A_RX_HPH_R_DAC_CTL, 7, 0,
		tapan_hphr_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	/* LINEOUT1*/
	SND_SOC_DAPM_DAC_E("LINEOUT1 DAC", NULL, TAPAN_A_RX_LINE_1_DAC_CTL, 7, 0
		, tapan_lineout_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA_E("LINEOUT1 PA", TAPAN_A_RX_LINE_CNP_EN, 0, 0, NULL,
			0, tapan_codec_enable_lineout, SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	/* LINEOUT2*/
	SND_SOC_DAPM_MUX("RDAC5 MUX", SND_SOC_NOPM, 0, 0,
		&rx_dac5_mux),

	/* LINEOUT1*/
	SND_SOC_DAPM_MUX("RDAC4 MUX", SND_SOC_NOPM, 0, 0,
		&rx_dac4_mux),

	SND_SOC_DAPM_MUX("RDAC3 MUX", SND_SOC_NOPM, 0, 0,
		&rx_dac3_mux),

	SND_SOC_DAPM_DAC_E("LINEOUT2 DAC", NULL, TAPAN_A_RX_LINE_2_DAC_CTL, 7, 0
		, tapan_lineout_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA_E("LINEOUT2 PA", TAPAN_A_RX_LINE_CNP_EN, 1, 0, NULL,
			0, tapan_codec_enable_lineout, SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	/* CLASS-D SPK */
	SND_SOC_DAPM_MIXER_E("SPK DAC", SND_SOC_NOPM, 0, 0,
		spk_dac_switch, ARRAY_SIZE(spk_dac_switch), tapan_spk_dac_event,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA_E("SPK PA", SND_SOC_NOPM, 0, 0 , NULL,
			   0, tapan_codec_enable_spk_pa,
			   SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY("VDD_SPKDRV", SND_SOC_NOPM, 0, 0,
			    tapan_codec_enable_vdd_spkr,
			    SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_OUTPUT("EAR"),
	SND_SOC_DAPM_OUTPUT("HEADPHONE"),
	SND_SOC_DAPM_OUTPUT("LINEOUT1"),
	SND_SOC_DAPM_OUTPUT("LINEOUT2"),
	SND_SOC_DAPM_OUTPUT("SPK_OUT"),

	/* TX Path*/
	SND_SOC_DAPM_MIXER("AIF1_CAP Mixer", SND_SOC_NOPM, AIF1_CAP, 0,
		aif1_cap_mixer, ARRAY_SIZE(aif1_cap_mixer)),

	SND_SOC_DAPM_MIXER("AIF2_CAP Mixer", SND_SOC_NOPM, AIF2_CAP, 0,
		aif2_cap_mixer, ARRAY_SIZE(aif2_cap_mixer)),

	SND_SOC_DAPM_MIXER("AIF3_CAP Mixer", SND_SOC_NOPM, AIF3_CAP, 0,
		aif3_cap_mixer, ARRAY_SIZE(aif3_cap_mixer)),

	SND_SOC_DAPM_MUX("SLIM TX1 MUX", SND_SOC_NOPM, TAPAN_TX1, 0,
		&sb_tx1_mux),
	SND_SOC_DAPM_MUX("SLIM TX2 MUX", SND_SOC_NOPM, TAPAN_TX2, 0,
		&sb_tx2_mux),
	SND_SOC_DAPM_MUX("SLIM TX3 MUX", SND_SOC_NOPM, TAPAN_TX3, 0,
		&sb_tx3_mux),
	SND_SOC_DAPM_MUX("SLIM TX4 MUX", SND_SOC_NOPM, TAPAN_TX4, 0,
		&sb_tx4_mux),
	SND_SOC_DAPM_MUX("SLIM TX5 MUX", SND_SOC_NOPM, TAPAN_TX5, 0,
		&sb_tx5_mux),

	SND_SOC_DAPM_SUPPLY("CDC_CONN", WCD9XXX_A_CDC_CLK_OTHR_CTL, 2, 0, NULL,
		0),

	/* Decimator MUX */
	SND_SOC_DAPM_MUX_E("DEC1 MUX", TAPAN_A_CDC_CLK_TX_CLK_EN_B1_CTL, 0, 0,
		&dec1_mux, tapan_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("DEC2 MUX", TAPAN_A_CDC_CLK_TX_CLK_EN_B1_CTL, 1, 0,
		&dec2_mux, tapan_codec_enable_dec,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SUPPLY("LDO_H", SND_SOC_NOPM, 7, 0,
		tapan_codec_enable_ldo_h,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	/*
	 * DAPM 'LDO_H Standalone' is to be powered by mbhc driver after
	 * acquring codec_resource lock.
	 * So call __tapan_codec_enable_ldo_h instead and avoid deadlock.
	 */
	SND_SOC_DAPM_SUPPLY("LDO_H Standalone", SND_SOC_NOPM, 7, 0,
			    __tapan_codec_enable_ldo_h,
			    SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_INPUT("AMIC1"),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS1 External", SND_SOC_NOPM, 7, 0,
		tapan_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS1 Internal1", SND_SOC_NOPM, 7, 0,
		tapan_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS1 Internal2", SND_SOC_NOPM, 7, 0,
		tapan_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("ADC1", NULL, TAPAN_A_TX_1_EN, 7, 0,
		tapan_codec_enable_adc, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_ADC_E("ADC2", NULL, TAPAN_A_TX_2_EN, 7, 0,
		tapan_codec_enable_adc, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_INPUT("AMIC3"),
	SND_SOC_DAPM_ADC_E("ADC3", NULL, TAPAN_A_TX_3_EN, 7, 0,
		tapan_codec_enable_adc, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_INPUT("AMIC4"),
	SND_SOC_DAPM_ADC_E("ADC4", NULL, TAPAN_A_TX_4_EN, 7, 0,
		tapan_codec_enable_adc, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_INPUT("AMIC2"),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS2 External", SND_SOC_NOPM, 7, 0,
		tapan_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU |	SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS2 Internal1", SND_SOC_NOPM, 7, 0,
		tapan_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS2 Internal2", SND_SOC_NOPM, 7, 0,
		tapan_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MICBIAS_E("MIC BIAS2 Internal3", SND_SOC_NOPM, 7, 0,
		tapan_codec_enable_micbias, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MICBIAS_E(DAPM_MICBIAS2_EXTERNAL_STANDALONE, SND_SOC_NOPM,
			       7, 0, tapan_codec_enable_micbias,
			       SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
			       SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_AIF_OUT_E("AIF1 CAP", "AIF1 Capture", 0, SND_SOC_NOPM,
		AIF1_CAP, 0, tapan_codec_enable_slimtx,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_AIF_OUT_E("AIF2 CAP", "AIF2 Capture", 0, SND_SOC_NOPM,
		AIF2_CAP, 0, tapan_codec_enable_slimtx,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_AIF_OUT_E("AIF3 CAP", "AIF3 Capture", 0, SND_SOC_NOPM,
		AIF3_CAP, 0, tapan_codec_enable_slimtx,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

		/* Digital Mic Inputs */
	SND_SOC_DAPM_ADC_E("DMIC1", NULL, SND_SOC_NOPM, 0, 0,
		tapan_codec_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("DMIC2", NULL, SND_SOC_NOPM, 0, 0,
		tapan_codec_enable_dmic, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	/* Sidetone */
	SND_SOC_DAPM_MUX_E("IIR1 INP1 MUX", TAPAN_A_CDC_IIR1_GAIN_B1_CTL, 0, 0,
		&iir1_inp1_mux, tapan_codec_iir_mux_event,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA_E("IIR1", TAPAN_A_CDC_CLK_SD_CTL, 0, 0, NULL, 0,
		tapan_codec_set_iir_gain, SND_SOC_DAPM_POST_PMU),

	SND_SOC_DAPM_MUX_E("IIR1 INP2 MUX", TAPAN_A_CDC_IIR1_GAIN_B2_CTL, 0, 0,
		&iir1_inp2_mux, tapan_codec_iir_mux_event,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("IIR1 INP3 MUX", TAPAN_A_CDC_IIR1_GAIN_B3_CTL, 0, 0,
		&iir1_inp3_mux, tapan_codec_iir_mux_event,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("IIR1 INP4 MUX", TAPAN_A_CDC_IIR1_GAIN_B4_CTL, 0, 0,
		&iir1_inp4_mux, tapan_codec_iir_mux_event,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("IIR2 INP1 MUX", TAPAN_A_CDC_IIR2_GAIN_B1_CTL, 0, 0,
		&iir2_inp1_mux, tapan_codec_iir_mux_event,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("IIR2 INP2 MUX", TAPAN_A_CDC_IIR2_GAIN_B2_CTL, 0, 0,
		&iir2_inp2_mux, tapan_codec_iir_mux_event,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("IIR2 INP3 MUX", TAPAN_A_CDC_IIR2_GAIN_B3_CTL, 0, 0,
		&iir2_inp3_mux, tapan_codec_iir_mux_event,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX_E("IIR2 INP4 MUX", TAPAN_A_CDC_IIR2_GAIN_B4_CTL, 0, 0,
		&iir2_inp4_mux, tapan_codec_iir_mux_event,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA("IIR2", TAPAN_A_CDC_CLK_SD_CTL, 1, 0, NULL, 0),

	/* AUX PGA */
	SND_SOC_DAPM_ADC_E("AUX_PGA_Left", NULL, TAPAN_A_RX_AUX_SW_CTL, 7, 0,
		tapan_codec_enable_aux_pga, SND_SOC_DAPM_PRE_PMU |
		SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_ADC_E("AUX_PGA_Right", NULL, TAPAN_A_RX_AUX_SW_CTL, 6, 0,
		tapan_codec_enable_aux_pga, SND_SOC_DAPM_PRE_PMU |
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
};

static irqreturn_t tapan_slimbus_irq(int irq, void *data)
{
	struct tapan_priv *priv = data;
	struct snd_soc_codec *codec = priv->codec;
	unsigned long status = 0;
	int i, j, port_id, k;
	u32 bit;
	u8 val;
	bool tx, cleared;

	for (i = TAPAN_SLIM_PGD_PORT_INT_STATUS_RX_0, j = 0;
	     i <= TAPAN_SLIM_PGD_PORT_INT_STATUS_TX_1; i++, j++) {
		val = wcd9xxx_interface_reg_read(codec->control_data, i);
		status |= ((u32)val << (8 * j));
	}

	for_each_set_bit(j, &status, 32) {
		tx = (j >= 16 ? true : false);
		port_id = (tx ? j - 16 : j);
		val = wcd9xxx_interface_reg_read(codec->control_data,
					TAPAN_SLIM_PGD_PORT_INT_RX_SOURCE0 + j);
		if (val & TAPAN_SLIM_IRQ_OVERFLOW)
			pr_err_ratelimited(
			    "%s: overflow error on %s port %d, value %x\n",
			    __func__, (tx ? "TX" : "RX"), port_id, val);
		if (val & TAPAN_SLIM_IRQ_UNDERFLOW)
			pr_err_ratelimited(
			    "%s: underflow error on %s port %d, value %x\n",
			    __func__, (tx ? "TX" : "RX"), port_id, val);
		if (val & TAPAN_SLIM_IRQ_PORT_CLOSED) {
			/*
			 * INT SOURCE register starts from RX to TX
			 * but port number in the ch_mask is in opposite way
			 */
			bit = (tx ? j - 16 : j + 16);
			dev_dbg(codec->dev, "%s: %s port %d closed value %x, bit %u\n",
				 __func__, (tx ? "TX" : "RX"), port_id, val,
				 bit);
			for (k = 0, cleared = false; k < NUM_CODEC_DAIS; k++) {
				dev_dbg(codec->dev, "%s: priv->dai[%d].ch_mask = 0x%lx\n",
					 __func__, k, priv->dai[k].ch_mask);
				if (test_and_clear_bit(bit,
						       &priv->dai[k].ch_mask)) {
					cleared = true;
					if (!priv->dai[k].ch_mask)
						wake_up(&priv->dai[k].dai_wait);
					/*
					 * There are cases when multiple DAIs
					 * might be using the same slimbus
					 * channel. Hence don't break here.
					 */
				}
			}
			WARN(!cleared,
			     "Couldn't find slimbus %s port %d for closing\n",
			     (tx ? "TX" : "RX"), port_id);
		}
		wcd9xxx_interface_reg_write(codec->control_data,
					    TAPAN_SLIM_PGD_PORT_INT_CLR_RX_0 +
					    (j / 8),
					    1 << (j % 8));
	}

	return IRQ_HANDLED;
}

static int tapan_handle_pdata(struct tapan_priv *tapan)
{
	struct snd_soc_codec *codec = tapan->codec;
	struct wcd9xxx_pdata *pdata = tapan->resmgr.pdata;
	int k1, k2, k3, rc = 0;
	u8 txfe_bypass;
	u8 txfe_buff;
	u8 flag;
	u8 i = 0, j = 0;
	u8 val_txfe = 0, value = 0;
	u8 dmic_sample_rate_value = 0;
	u8 dmic_b1_ctl_value = 0;
	u8 anc_ctl_value = 0;

	if (!pdata) {
		dev_err(codec->dev, "%s: NULL pdata\n", __func__);
		rc = -ENODEV;
		goto done;
	}
	txfe_bypass = pdata->amic_settings.txfe_enable;
	txfe_buff = pdata->amic_settings.txfe_buff;
	flag = pdata->amic_settings.use_pdata;

	/* Make sure settings are correct */
	if ((pdata->micbias.ldoh_v > WCD9XXX_LDOH_3P0_V) ||
	    (pdata->micbias.bias1_cfilt_sel > WCD9XXX_CFILT3_SEL) ||
	    (pdata->micbias.bias2_cfilt_sel > WCD9XXX_CFILT3_SEL) ||
	    (pdata->micbias.bias3_cfilt_sel > WCD9XXX_CFILT3_SEL)) {
		dev_err(codec->dev, "%s: Invalid ldoh voltage or bias cfilt\n",
				__func__);
		rc = -EINVAL;
		goto done;
	}
	/* figure out k value */
	k1 = wcd9xxx_resmgr_get_k_val(&tapan->resmgr, pdata->micbias.cfilt1_mv);
	k2 = wcd9xxx_resmgr_get_k_val(&tapan->resmgr, pdata->micbias.cfilt2_mv);
	k3 = wcd9xxx_resmgr_get_k_val(&tapan->resmgr, pdata->micbias.cfilt3_mv);

	if (IS_ERR_VALUE(k1) || IS_ERR_VALUE(k2) || IS_ERR_VALUE(k3)) {
		dev_err(codec->dev,
			"%s: could not get K value. k1 = %d k2 = %d k3 = %d\n",
			__func__, k1, k2, k3);
		rc = -EINVAL;
		goto done;
	}
	/* Set voltage level and always use LDO */
	snd_soc_update_bits(codec, TAPAN_A_LDO_H_MODE_1, 0x0C,
			    (pdata->micbias.ldoh_v << 2));

	snd_soc_update_bits(codec, TAPAN_A_MICB_CFILT_1_VAL, 0xFC, (k1 << 2));
	snd_soc_update_bits(codec, TAPAN_A_MICB_CFILT_2_VAL, 0xFC, (k2 << 2));
	snd_soc_update_bits(codec, TAPAN_A_MICB_CFILT_3_VAL, 0xFC, (k3 << 2));

	i = 0;
	while (i < 5) {
		if (flag & (0x01 << i)) {
			val_txfe = (txfe_bypass & (0x01 << i)) ? 0x20 : 0x00;
			val_txfe = val_txfe |
				((txfe_buff & (0x01 << i)) ? 0x10 : 0x00);
			snd_soc_update_bits(codec,
				TAPAN_A_TX_1_2_TEST_EN + j * 10,
				0x30, val_txfe);
		}
		if (flag & (0x01 << (i + 1))) {
			val_txfe = (txfe_bypass &
					(0x01 << (i + 1))) ? 0x02 : 0x00;
			val_txfe |= (txfe_buff &
					(0x01 << (i + 1))) ? 0x01 : 0x00;
			snd_soc_update_bits(codec,
				TAPAN_A_TX_1_2_TEST_EN + j * 10,
				0x03, val_txfe);
		}
		/* Tapan only has TAPAN_A_TX_1_2_TEST_EN  and
		   TAPAN_A_TX_4_5_TEST_EN  reg */

		if (i == 0) {
			i = 3;
			continue;
		} else if (i == 3) {
			break;
		}
	}

	if (pdata->ocp.use_pdata) {
		/* not defined in CODEC specification */
		if (pdata->ocp.hph_ocp_limit == 1 ||
			pdata->ocp.hph_ocp_limit == 5) {
			rc = -EINVAL;
			goto done;
		}
		snd_soc_update_bits(codec, TAPAN_A_RX_COM_OCP_CTL,
			0x0F, pdata->ocp.num_attempts);
		snd_soc_write(codec, TAPAN_A_RX_COM_OCP_COUNT,
			((pdata->ocp.run_time << 4) | pdata->ocp.wait_time));
		snd_soc_update_bits(codec, TAPAN_A_RX_HPH_OCP_CTL,
			0xE0, (pdata->ocp.hph_ocp_limit << 5));
	}

	/* Set micbias capless mode with tail current */
	value = (pdata->micbias.bias1_cap_mode == MICBIAS_EXT_BYP_CAP ?
		 0x00 : 0x10);
	snd_soc_update_bits(codec, TAPAN_A_MICB_1_CTL, 0x10, value);
	value = (pdata->micbias.bias2_cap_mode == MICBIAS_EXT_BYP_CAP ?
		 0x00 : 0x10);
	snd_soc_update_bits(codec, TAPAN_A_MICB_2_CTL, 0x10, value);
	value = (pdata->micbias.bias3_cap_mode == MICBIAS_EXT_BYP_CAP ?
		 0x00 : 0x10);
	snd_soc_update_bits(codec, TAPAN_A_MICB_3_CTL, 0x10, value);

	/* Set the DMIC sample rate */
	if (pdata->mclk_rate == TAPAN_MCLK_CLK_9P6MHZ) {
		switch (pdata->dmic_sample_rate) {
		case WCD9XXX_DMIC_SAMPLE_RATE_2P4MHZ:
			dmic_sample_rate_value = WCD9XXX_DMIC_SAMPLE_RATE_DIV_4;
			dmic_b1_ctl_value = WCD9XXX_DMIC_B1_CTL_DIV_4;
			anc_ctl_value = WCD9XXX_ANC_DMIC_X2_OFF;
			break;
		case WCD9XXX_DMIC_SAMPLE_RATE_4P8MHZ:
			dmic_sample_rate_value = WCD9XXX_DMIC_SAMPLE_RATE_DIV_2;
			dmic_b1_ctl_value = WCD9XXX_DMIC_B1_CTL_DIV_2;
			anc_ctl_value = WCD9XXX_ANC_DMIC_X2_ON;
			break;
		case WCD9XXX_DMIC_SAMPLE_RATE_3P2MHZ:
		case WCD9XXX_DMIC_SAMPLE_RATE_UNDEFINED:
			dmic_sample_rate_value = WCD9XXX_DMIC_SAMPLE_RATE_DIV_3;
			dmic_b1_ctl_value = WCD9XXX_DMIC_B1_CTL_DIV_3;
			anc_ctl_value = WCD9XXX_ANC_DMIC_X2_OFF;
			break;
		default:
			dev_err(codec->dev,
				"%s Invalid sample rate %d for mclk %d\n",
				__func__, pdata->dmic_sample_rate,
				pdata->mclk_rate);
			rc = -EINVAL;
			goto done;
		}
	} else if (pdata->mclk_rate == TAPAN_MCLK_CLK_12P288MHZ) {
		switch (pdata->dmic_sample_rate) {
		case WCD9XXX_DMIC_SAMPLE_RATE_3P072MHZ:
			dmic_sample_rate_value = WCD9XXX_DMIC_SAMPLE_RATE_DIV_4;
			dmic_b1_ctl_value = WCD9XXX_DMIC_B1_CTL_DIV_4;
			anc_ctl_value = WCD9XXX_ANC_DMIC_X2_OFF;
			break;
		case WCD9XXX_DMIC_SAMPLE_RATE_6P144MHZ:
			dmic_sample_rate_value = WCD9XXX_DMIC_SAMPLE_RATE_DIV_2;
			dmic_b1_ctl_value = WCD9XXX_DMIC_B1_CTL_DIV_2;
			anc_ctl_value = WCD9XXX_ANC_DMIC_X2_ON;
			break;
		case WCD9XXX_DMIC_SAMPLE_RATE_4P096MHZ:
		case WCD9XXX_DMIC_SAMPLE_RATE_UNDEFINED:
			dmic_sample_rate_value = WCD9XXX_DMIC_SAMPLE_RATE_DIV_3;
			dmic_b1_ctl_value = WCD9XXX_DMIC_B1_CTL_DIV_3;
			anc_ctl_value = WCD9XXX_ANC_DMIC_X2_OFF;
			break;
		default:
			dev_err(codec->dev,
				"%s Invalid sample rate %d for mclk %d\n",
				__func__, pdata->dmic_sample_rate,
				pdata->mclk_rate);
			rc = -EINVAL;
			goto done;
		}
	} else {
		dev_err(codec->dev, "%s MCLK is not set!\n", __func__);
		rc = -EINVAL;
		goto done;
	}

	snd_soc_update_bits(codec, TAPAN_A_CDC_TX1_DMIC_CTL,
		0x7, dmic_sample_rate_value);
	snd_soc_update_bits(codec, TAPAN_A_CDC_TX2_DMIC_CTL,
		0x7, dmic_sample_rate_value);
	snd_soc_update_bits(codec, TAPAN_A_CDC_TX3_DMIC_CTL,
		0x7, dmic_sample_rate_value);
	snd_soc_update_bits(codec, TAPAN_A_CDC_TX4_DMIC_CTL,
		0x7, dmic_sample_rate_value);
	snd_soc_update_bits(codec, TAPAN_A_CDC_CLK_DMIC_B1_CTL,
		0xEE, dmic_b1_ctl_value);
	snd_soc_update_bits(codec, TAPAN_A_CDC_ANC1_B2_CTL,
		0x1, anc_ctl_value);
	snd_soc_update_bits(codec, TAPAN_A_CDC_ANC2_B2_CTL,
		0x1, anc_ctl_value);

done:
	return rc;
}

static const struct tapan_reg_mask_val tapan_reg_defaults[] = {

	/* enable QFUSE for wcd9306 */
	TAPAN_REG_VAL(TAPAN_A_QFUSE_CTL, 0x03),

	/* PROGRAM_THE_0P85V_VBG_REFERENCE = V_0P858V */
	TAPAN_REG_VAL(TAPAN_A_BIAS_CURR_CTL_2, 0x04),

	TAPAN_REG_VAL(TAPAN_A_CDC_CLK_POWER_CTL, 0x03),

	/* EAR PA deafults  */
	TAPAN_REG_VAL(TAPAN_A_RX_EAR_CMBUFF, 0x05),

	/* RX1 and RX2 defaults */
	TAPAN_REG_VAL(TAPAN_A_CDC_RX1_B6_CTL, 0xA0),
	TAPAN_REG_VAL(TAPAN_A_CDC_RX2_B6_CTL, 0x80),

	/* Heaset set Right from RX2 */
	TAPAN_REG_VAL(TAPAN_A_CDC_CONN_RX2_B2_CTL, 0x10),


	/*
	 * The following only need to be written for Tapan 1.0 parts.
	 * Tapan 2.0 will have appropriate defaults for these registers.
	 */

	/* Required defaults for class H operation */
	/* borrowed from Taiko class-h */
	TAPAN_REG_VAL(TAPAN_A_RX_HPH_CHOP_CTL, 0xF4),
	TAPAN_REG_VAL(TAPAN_A_BIAS_CURR_CTL_2, 0x08),
	TAPAN_REG_VAL(WCD9XXX_A_BUCK_CTRL_CCL_1, 0x5B),
	TAPAN_REG_VAL(WCD9XXX_A_BUCK_CTRL_CCL_3, 0x6F),

	/* TODO: Check below reg writes conflict with above */
	/* PROGRAM_THE_0P85V_VBG_REFERENCE = V_0P858V */
	TAPAN_REG_VAL(TAPAN_A_BIAS_CURR_CTL_2, 0x04),
	TAPAN_REG_VAL(TAPAN_A_RX_HPH_CHOP_CTL, 0x74),
	TAPAN_REG_VAL(TAPAN_A_RX_BUCK_BIAS1, 0x62),

	/* Choose max non-overlap time for NCP */
	TAPAN_REG_VAL(TAPAN_A_NCP_CLK, 0xFC),
	/* Use 25mV/50mV for deltap/m to reduce ripple */
	TAPAN_REG_VAL(WCD9XXX_A_BUCK_CTRL_VCL_1, 0x08),
	/*
	 * Set DISABLE_MODE_SEL<1:0> to 0b10 (disable PWM in auto mode).
	 * Note that the other bits of this register will be changed during
	 * Rx PA bring up.
	 */
	TAPAN_REG_VAL(WCD9XXX_A_BUCK_MODE_3, 0xCE),
	/* Reduce HPH DAC bias to 70% */
	TAPAN_REG_VAL(TAPAN_A_RX_HPH_BIAS_PA, 0x7A),
	/*Reduce EAR DAC bias to 70% */
	TAPAN_REG_VAL(TAPAN_A_RX_EAR_BIAS_PA, 0x76),
	/* Reduce LINE DAC bias to 70% */
	TAPAN_REG_VAL(TAPAN_A_RX_LINE_BIAS_PA, 0x78),

	/*
	 * There is a diode to pull down the micbias while doing
	 * insertion detection.  This diode can cause leakage.
	 * Set bit 0 to 1 to prevent leakage.
	 * Setting this bit of micbias 2 prevents leakage for all other micbias.
	 */
	TAPAN_REG_VAL(TAPAN_A_MICB_2_MBHC, 0x41),

	/*
	 * Default register settings to support dynamic change of
	 * vdd_buck between 1.8 volts and 2.15 volts.
	 */
	TAPAN_REG_VAL(TAPAN_A_BUCK_MODE_2, 0xAA),

};

static const struct tapan_reg_mask_val tapan_2_x_reg_reset_values[] = {

	TAPAN_REG_VAL(TAPAN_A_TX_7_MBHC_EN, 0x6C),
	TAPAN_REG_VAL(TAPAN_A_BUCK_CTRL_CCL_4, 0x51),
	TAPAN_REG_VAL(TAPAN_A_RX_HPH_CNP_WG_CTL, 0xDA),
	TAPAN_REG_VAL(TAPAN_A_RX_EAR_CNP, 0xC0),
	TAPAN_REG_VAL(TAPAN_A_RX_LINE_1_TEST, 0x02),
	TAPAN_REG_VAL(TAPAN_A_RX_LINE_2_TEST, 0x02),
	TAPAN_REG_VAL(TAPAN_A_SPKR_DRV_OCP_CTL, 0x97),
	TAPAN_REG_VAL(TAPAN_A_SPKR_DRV_CLIP_DET, 0x01),
	TAPAN_REG_VAL(TAPAN_A_SPKR_DRV_IEC, 0x00),
	TAPAN_REG_VAL(TAPAN_A_CDC_CLSH_B1_CTL, 0xE4),
	TAPAN_REG_VAL(TAPAN_A_CDC_CLSH_B2_CTL, 0x00),
	TAPAN_REG_VAL(TAPAN_A_CDC_CLSH_B3_CTL, 0x00),
	TAPAN_REG_VAL(TAPAN_A_CDC_CLSH_BUCK_NCP_VARS, 0x00),
	TAPAN_REG_VAL(TAPAN_A_CDC_CLSH_V_PA_HD_EAR, 0x00),
	TAPAN_REG_VAL(TAPAN_A_CDC_CLSH_V_PA_HD_HPH, 0x00),
	TAPAN_REG_VAL(TAPAN_A_CDC_CLSH_V_PA_MIN_EAR, 0x00),
	TAPAN_REG_VAL(TAPAN_A_CDC_CLSH_V_PA_MIN_HPH, 0x00),
};

static const struct tapan_reg_mask_val tapan_1_0_reg_defaults[] = {
	/* Close leakage on the spkdrv */
	TAPAN_REG_VAL(TAPAN_A_SPKR_DRV_DBG_PWRSTG, 0x24),
	TAPAN_REG_VAL(TAPAN_A_SPKR_DRV_DBG_DAC, 0xE5),

};

static void tapan_update_reg_defaults(struct snd_soc_codec *codec)
{
	u32 i;
	struct wcd9xxx *tapan_core = dev_get_drvdata(codec->dev->parent);

	if (!TAPAN_IS_1_0(tapan_core->version)) {
		for (i = 0; i < ARRAY_SIZE(tapan_2_x_reg_reset_values); i++)
			snd_soc_write(codec, tapan_2_x_reg_reset_values[i].reg,
				tapan_2_x_reg_reset_values[i].val);
	}

	for (i = 0; i < ARRAY_SIZE(tapan_reg_defaults); i++)
		snd_soc_write(codec, tapan_reg_defaults[i].reg,
				tapan_reg_defaults[i].val);

	if (TAPAN_IS_1_0(tapan_core->version)) {
		for (i = 0; i < ARRAY_SIZE(tapan_1_0_reg_defaults); i++)
			snd_soc_write(codec, tapan_1_0_reg_defaults[i].reg,
				tapan_1_0_reg_defaults[i].val);
	}

	if (!TAPAN_IS_1_0(tapan_core->version))
		spkr_drv_wrnd = -1;
	else if (spkr_drv_wrnd == 1)
		snd_soc_write(codec, TAPAN_A_SPKR_DRV_EN, 0xEF);
}

static void tapan_update_reg_mclk_rate(struct wcd9xxx *wcd9xxx)
{
	struct snd_soc_codec *codec;

	codec = (struct snd_soc_codec *)(wcd9xxx->ssr_priv);
	dev_dbg(codec->dev, "%s: MCLK Rate = %x\n",
			__func__, wcd9xxx->mclk_rate);

	if (wcd9xxx->mclk_rate == TAPAN_MCLK_CLK_12P288MHZ) {
		snd_soc_update_bits(codec, TAPAN_A_CHIP_CTL, 0x06, 0x0);
		snd_soc_update_bits(codec, TAPAN_A_RX_COM_TIMER_DIV, 0x01,
				0x01);
	} else if (wcd9xxx->mclk_rate == TAPAN_MCLK_CLK_9P6MHZ) {
		snd_soc_update_bits(codec, TAPAN_A_CHIP_CTL, 0x06, 0x2);
	}
}

static const struct tapan_reg_mask_val tapan_codec_reg_init_val[] = {
	/* Initialize current threshold to 365MA
	 * number of wait and run cycles to 4096
	 */
	{TAPAN_A_RX_HPH_OCP_CTL, 0xE9, 0x69},
	{TAPAN_A_RX_COM_OCP_COUNT, 0xFF, 0xFF},
	{TAPAN_A_RX_HPH_L_TEST, 0x01, 0x01},
	{TAPAN_A_RX_HPH_R_TEST, 0x01, 0x01},

	/* Initialize gain registers to use register gain */
	{TAPAN_A_RX_HPH_L_GAIN, 0x20, 0x20},
	{TAPAN_A_RX_HPH_R_GAIN, 0x20, 0x20},
	{TAPAN_A_RX_LINE_1_GAIN, 0x20, 0x20},
	{TAPAN_A_RX_LINE_2_GAIN, 0x20, 0x20},
	{TAPAN_A_SPKR_DRV_GAIN, 0x04, 0x04},

	/*  Set RDAC5 MUX to take input from DEM3_INV.
	 *  This sets LO2 DAC to get input from DEM3_INV
	 *  for LO1 and LO2 to work as differential outputs.
	 */
	{TAPAN_A_CDC_CONN_MISC, 0x04, 0x04},

	/* CLASS H config */
	{TAPAN_A_CDC_CONN_CLSH_CTL, 0x30, 0x10},

	/* Use 16 bit sample size for TX1 to TX5 */
	{TAPAN_A_CDC_CONN_TX_SB_B1_CTL, 0x30, 0x20},
	{TAPAN_A_CDC_CONN_TX_SB_B2_CTL, 0x30, 0x20},
	{TAPAN_A_CDC_CONN_TX_SB_B3_CTL, 0x30, 0x20},
	{TAPAN_A_CDC_CONN_TX_SB_B4_CTL, 0x30, 0x20},
	{TAPAN_A_CDC_CONN_TX_SB_B5_CTL, 0x30, 0x20},

	/* Disable SPK SWITCH */
	{TAPAN_A_SPKR_DRV_DAC_CTL, 0x04, 0x00},

	/* Use 16 bit sample size for RX */
	{TAPAN_A_CDC_CONN_RX_SB_B1_CTL, 0xFF, 0xAA},
	{TAPAN_A_CDC_CONN_RX_SB_B2_CTL, 0xFF, 0x2A},

	/*enable HPF filter for TX paths */
	{TAPAN_A_CDC_TX1_MUX_CTL, 0x8, 0x0},
	{TAPAN_A_CDC_TX2_MUX_CTL, 0x8, 0x0},
	{TAPAN_A_CDC_TX3_MUX_CTL, 0x8, 0x0},
	{TAPAN_A_CDC_TX4_MUX_CTL, 0x8, 0x0},

	/* Compander zone selection */
	{TAPAN_A_CDC_COMP0_B4_CTL, 0x3F, 0x37},
	{TAPAN_A_CDC_COMP1_B4_CTL, 0x3F, 0x37},
	{TAPAN_A_CDC_COMP2_B4_CTL, 0x3F, 0x37},
	{TAPAN_A_CDC_COMP0_B5_CTL, 0x7F, 0x7F},
	{TAPAN_A_CDC_COMP1_B5_CTL, 0x7F, 0x7F},
	{TAPAN_A_CDC_COMP2_B5_CTL, 0x7F, 0x7F},

	/*
	 * Setup wavegen timer to 20msec and disable chopper
	 * as default. This corresponds to Compander OFF
	 */
	{TAPAN_A_RX_HPH_CNP_WG_CTL, 0xFF, 0xDB},
	{TAPAN_A_RX_HPH_CNP_WG_TIME, 0xFF, 0x58},
	{TAPAN_A_RX_HPH_BIAS_WG_OCP, 0xFF, 0x1A},
	{TAPAN_A_RX_HPH_CHOP_CTL, 0xFF, 0x24},
};

void *tapan_get_afe_config(struct snd_soc_codec *codec,
			   enum afe_config_type config_type)
{
	struct tapan_priv *priv = snd_soc_codec_get_drvdata(codec);

	switch (config_type) {
	case AFE_SLIMBUS_SLAVE_CONFIG:
		return &priv->slimbus_slave_cfg;
	case AFE_CDC_REGISTERS_CONFIG:
		return &tapan_audio_reg_cfg;
	case AFE_AANC_VERSION:
		return &tapan_cdc_aanc_version;
	default:
		pr_err("%s: Unknown config_type 0x%x\n", __func__, config_type);
		return NULL;
	}
}

static void tapan_init_slim_slave_cfg(struct snd_soc_codec *codec)
{
	struct tapan_priv *priv = snd_soc_codec_get_drvdata(codec);
	struct afe_param_cdc_slimbus_slave_cfg *cfg;
	struct wcd9xxx *wcd9xxx = codec->control_data;
	uint64_t eaddr = 0;

	pr_debug("%s\n", __func__);
	cfg = &priv->slimbus_slave_cfg;
	cfg->minor_version = 1;
	cfg->tx_slave_port_offset = 0;
	cfg->rx_slave_port_offset = 16;

	memcpy(&eaddr, &wcd9xxx->slim->e_addr, sizeof(wcd9xxx->slim->e_addr));
	/* e-addr is 6-byte elemental address of the device */
	WARN_ON(sizeof(wcd9xxx->slim->e_addr) != 6);
	cfg->device_enum_addr_lsw = eaddr & 0xFFFFFFFF;
	cfg->device_enum_addr_msw = eaddr >> 32;

	pr_debug("%s: slimbus logical address 0x%llx\n", __func__, eaddr);
}

static void tapan_codec_init_reg(struct snd_soc_codec *codec)
{
	u32 i;

	for (i = 0; i < ARRAY_SIZE(tapan_codec_reg_init_val); i++)
		snd_soc_update_bits(codec, tapan_codec_reg_init_val[i].reg,
				tapan_codec_reg_init_val[i].mask,
				tapan_codec_reg_init_val[i].val);
}
static void tapan_slim_interface_init_reg(struct snd_soc_codec *codec)
{
	int i;

	for (i = 0; i < WCD9XXX_SLIM_NUM_PORT_REG; i++)
		wcd9xxx_interface_reg_write(codec->control_data,
					    TAPAN_SLIM_PGD_PORT_INT_EN0 + i,
					    0xFF);
}

static int tapan_setup_irqs(struct tapan_priv *tapan)
{
	int ret = 0;
	struct snd_soc_codec *codec = tapan->codec;
	struct wcd9xxx *wcd9xxx = codec->control_data;
	struct wcd9xxx_core_resource *core_res = &wcd9xxx->core_res;

	ret = wcd9xxx_request_irq(core_res, WCD9XXX_IRQ_SLIMBUS,
				  tapan_slimbus_irq, "SLIMBUS Slave", tapan);
	if (ret)
		pr_err("%s: Failed to request irq %d\n", __func__,
		       WCD9XXX_IRQ_SLIMBUS);
	else
		tapan_slim_interface_init_reg(codec);

	return ret;
}

static void tapan_cleanup_irqs(struct tapan_priv *tapan)
{
	struct snd_soc_codec *codec = tapan->codec;
	struct wcd9xxx *wcd9xxx = codec->control_data;
	struct wcd9xxx_core_resource *core_res = &wcd9xxx->core_res;
	wcd9xxx_free_irq(core_res, WCD9XXX_IRQ_SLIMBUS, tapan);
}


static void tapan_enable_mux_bias_block(struct snd_soc_codec *codec)
{
	snd_soc_update_bits(codec, WCD9XXX_A_MBHC_SCALING_MUX_1,
			    0x80, 0x00);
}

static void tapan_put_cfilt_fast_mode(struct snd_soc_codec *codec,
				      struct wcd9xxx_mbhc *mbhc)
{
	snd_soc_update_bits(codec, mbhc->mbhc_bias_regs.cfilt_ctl,
			    0x30, 0x30);
}

static void tapan_codec_specific_cal_setup(struct snd_soc_codec *codec,
					   struct wcd9xxx_mbhc *mbhc)
{
	snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_B1_CTL,
			    0x04, 0x04);
	snd_soc_update_bits(codec, WCD9XXX_A_TX_7_MBHC_EN, 0xE0, 0xE0);
}

static struct wcd9xxx_cfilt_mode tapan_codec_switch_cfilt_mode(
				 struct wcd9xxx_mbhc *mbhc,
				 bool fast)
{
	struct snd_soc_codec *codec = mbhc->codec;
	struct wcd9xxx_cfilt_mode cfilt_mode;

	if (fast)
		cfilt_mode.reg_mode_val = WCD9XXX_CFILT_EXT_PRCHG_EN;
	else
		cfilt_mode.reg_mode_val = WCD9XXX_CFILT_EXT_PRCHG_DSBL;

	cfilt_mode.cur_mode_val =
		snd_soc_read(codec, mbhc->mbhc_bias_regs.cfilt_ctl) & 0x30;
	cfilt_mode.reg_mask = 0x30;

	return cfilt_mode;
}

static void tapan_select_cfilt(struct snd_soc_codec *codec,
			       struct wcd9xxx_mbhc *mbhc)
{
	snd_soc_update_bits(codec, mbhc->mbhc_bias_regs.ctl_reg, 0x60, 0x00);
}

enum wcd9xxx_cdc_type tapan_get_cdc_type(void)
{
	return WCD9XXX_CDC_TYPE_TAPAN;
}

static void wcd9xxx_prepare_hph_pa(struct wcd9xxx_mbhc *mbhc,
				   struct list_head *lh)
{
	int i;
	struct snd_soc_codec *codec = mbhc->codec;
	u32 delay;

	const struct wcd9xxx_reg_mask_val reg_set_paon[] = {
		{WCD9XXX_A_CDC_CLSH_B1_CTL, 0x0F, 0x00},
		{WCD9XXX_A_RX_HPH_CHOP_CTL, 0xFF, 0xA4},
		{WCD9XXX_A_RX_HPH_OCP_CTL, 0xFF, 0x67},
		{WCD9XXX_A_RX_HPH_L_TEST, 0x1, 0x0},
		{WCD9XXX_A_RX_HPH_R_TEST, 0x1, 0x0},
		{WCD9XXX_A_RX_HPH_BIAS_WG_OCP, 0xFF, 0x1A},
		{WCD9XXX_A_RX_HPH_CNP_WG_CTL, 0xFF, 0xDB},
		{WCD9XXX_A_RX_HPH_CNP_WG_TIME, 0xFF, 0x2A},
		{TAPAN_A_CDC_CONN_RX2_B2_CTL, 0xFF, 0x10},
		{WCD9XXX_A_CDC_CLK_OTHR_CTL, 0xFF, 0x05},
		{WCD9XXX_A_CDC_RX1_B6_CTL, 0xFF, 0x81},
		{WCD9XXX_A_CDC_CLK_RX_B1_CTL, 0x03, 0x03},
		{WCD9XXX_A_RX_HPH_L_GAIN, 0xFF, 0x2C},
		{WCD9XXX_A_CDC_RX2_B6_CTL, 0xFF, 0x81},
		{WCD9XXX_A_RX_HPH_R_GAIN, 0xFF, 0x2C},
		{WCD9XXX_A_BUCK_CTRL_CCL_4, 0xFF, 0x50},
		{WCD9XXX_A_BUCK_CTRL_VCL_1, 0xFF, 0x08},
		{WCD9XXX_A_BUCK_CTRL_CCL_1, 0xFF, 0x5B},
		{WCD9XXX_A_NCP_CLK, 0xFF, 0x9C},
		{WCD9XXX_A_NCP_CLK, 0xFF, 0xFC},
		{WCD9XXX_A_BUCK_MODE_3, 0xFF, 0xCE},
		{WCD9XXX_A_BUCK_CTRL_CCL_3, 0xFF, 0x6B},
		{WCD9XXX_A_BUCK_CTRL_CCL_3, 0xFF, 0x6F},
		{TAPAN_A_RX_BUCK_BIAS1, 0xFF, 0x62},
		{TAPAN_A_RX_HPH_BIAS_PA, 0xFF, 0x7A},
		{TAPAN_A_CDC_CLK_RDAC_CLK_EN_CTL, 0xFF, 0x02},
		{TAPAN_A_CDC_CLK_RDAC_CLK_EN_CTL, 0xFF, 0x06},
		{WCD9XXX_A_RX_COM_BIAS, 0xFF, 0x80},
		{WCD9XXX_A_BUCK_MODE_3, 0xFF, 0xC6},
		{WCD9XXX_A_BUCK_MODE_4, 0xFF, 0xE6},
		{WCD9XXX_A_BUCK_MODE_5, 0xFF, 0x02},
		{WCD9XXX_A_BUCK_MODE_1, 0xFF, 0xA1},
		/* Delay 1ms */
		{WCD9XXX_A_NCP_EN, 0xFF, 0xFF},
		/* Delay 1ms */
		{WCD9XXX_A_BUCK_MODE_5, 0xFF, 0x03},
		{WCD9XXX_A_BUCK_MODE_5, 0xFF, 0x7B},
		{WCD9XXX_A_CDC_CLSH_B1_CTL, 0xFF, 0xE6},
		{WCD9XXX_A_RX_HPH_L_DAC_CTL, 0xFF, 0x40},
		{WCD9XXX_A_RX_HPH_L_DAC_CTL, 0xFF, 0xC0},
		{WCD9XXX_A_RX_HPH_R_DAC_CTL, 0xFF, 0x40},
		{WCD9XXX_A_RX_HPH_R_DAC_CTL, 0xFF, 0xC0},
		{WCD9XXX_A_NCP_STATIC, 0xFF, 0x08},
		{WCD9XXX_A_RX_HPH_L_DAC_CTL, 0x03, 0x01},
		{WCD9XXX_A_RX_HPH_R_DAC_CTL, 0x03, 0x01},
	};

	/*
	 * Configure PA in class-AB, -18dB gain,
	 * companding off, OCP off, Chopping ON
	 */
	for (i = 0; i < ARRAY_SIZE(reg_set_paon); i++) {
		/*
		 * Some of the codec registers like BUCK_MODE_1
		 * and NCP_EN requires 1ms wait time for them
		 * to take effect. Other register writes for
		 * PA configuration do not require any wait time.
		 */
		if (reg_set_paon[i].reg == WCD9XXX_A_BUCK_MODE_1 ||
		    reg_set_paon[i].reg == WCD9XXX_A_NCP_EN)
			delay = 1000;
		else
			delay = 0;
		wcd9xxx_soc_update_bits_push(codec, lh,
					     reg_set_paon[i].reg,
					     reg_set_paon[i].mask,
					     reg_set_paon[i].val, delay);
	}
	pr_debug("%s: PAs are prepared\n", __func__);
	return;
}

static int wcd9xxx_enable_static_pa(struct wcd9xxx_mbhc *mbhc, bool enable)
{
	struct snd_soc_codec *codec = mbhc->codec;
	int wg_time = snd_soc_read(codec, WCD9XXX_A_RX_HPH_CNP_WG_TIME) *
				   TAPAN_WG_TIME_FACTOR_US;
	/*
	 * Tapan requires additional time to enable PA.
	 * It is observed during experiments that we need
	 * an additional wait time about 0.35 times of
	 * the WG_TIME
	 */
	wg_time += (int) (wg_time * 35) / 100;

	snd_soc_update_bits(codec, WCD9XXX_A_RX_HPH_CNP_EN, 0x30,
			    enable ? 0x30 : 0x0);
	/* Wait for wave gen time to avoid pop noise */
	usleep_range(wg_time, wg_time + WCD9XXX_USLEEP_RANGE_MARGIN_US);
	pr_debug("%s: PAs are %s as static mode (wg_time %d)\n", __func__,
		 enable ? "enabled" : "disabled", wg_time);
	return 0;
}

static int tapan_setup_zdet(struct wcd9xxx_mbhc *mbhc,
			    enum mbhc_impedance_detect_stages stage)
{

	int ret = 0;
	struct snd_soc_codec *codec = mbhc->codec;
	struct tapan_priv *tapan = snd_soc_codec_get_drvdata(codec);
	const int mux_wait_us = 25;

	switch (stage) {

	case MBHC_ZDET_PRE_MEASURE:
		INIT_LIST_HEAD(&tapan->reg_save_restore);
		/* Configure PA */
		wcd9xxx_prepare_hph_pa(mbhc, &tapan->reg_save_restore);

#define __wr(reg, mask, value)						  \
	do {								  \
		ret = wcd9xxx_soc_update_bits_push(codec,		  \
						   &tapan->reg_save_restore, \
						   reg, mask, value, 0);  \
		if (ret < 0)						  \
			return ret;					  \
	} while (0)

		/* Setup MBHC */
		__wr(WCD9XXX_A_MBHC_SCALING_MUX_1, 0x7F, 0x40);
		__wr(WCD9XXX_A_MBHC_SCALING_MUX_2, 0xFF, 0xF0);
		__wr(WCD9XXX_A_TX_7_MBHC_TEST_CTL, 0xFF, 0x78);
		__wr(WCD9XXX_A_TX_7_MBHC_EN, 0xFF, 0xEC);
		__wr(WCD9XXX_A_CDC_MBHC_TIMER_B4_CTL, 0xFF, 0x45);
		__wr(WCD9XXX_A_CDC_MBHC_TIMER_B5_CTL, 0xFF, 0x80);

		__wr(WCD9XXX_A_CDC_MBHC_CLK_CTL, 0xFF, 0x0A);
		snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_EN_CTL, 0x2);
		__wr(WCD9XXX_A_CDC_MBHC_CLK_CTL, 0xFF, 0x02);

		/* Enable Impedance Detection */
		__wr(WCD9XXX_A_MBHC_HPH, 0xFF, 0xC8);

		/*
		 * CnP setup for 0mV
		 * Route static data as input to noise shaper
		 */
		__wr(TAPAN_A_CDC_RX1_B3_CTL, 0xFF, 0x02);
		__wr(TAPAN_A_CDC_RX2_B3_CTL, 0xFF, 0x02);

		snd_soc_update_bits(codec, WCD9XXX_A_RX_HPH_L_TEST,
				    0x02, 0x00);
		snd_soc_update_bits(codec, WCD9XXX_A_RX_HPH_R_TEST,
				    0x02, 0x00);

		/* Reset the HPHL static data pointer */
		__wr(TAPAN_A_CDC_RX1_B2_CTL, 0xFF, 0x00);
		/* Four consecutive writes to set 0V as static data input */
		snd_soc_write(codec, TAPAN_A_CDC_RX1_B1_CTL, 0x00);
		snd_soc_write(codec, TAPAN_A_CDC_RX1_B1_CTL, 0x00);
		snd_soc_write(codec, TAPAN_A_CDC_RX1_B1_CTL, 0x00);
		snd_soc_write(codec, TAPAN_A_CDC_RX1_B1_CTL, 0x00);

		/* Reset the HPHR static data pointer */
		__wr(TAPAN_A_CDC_RX2_B2_CTL, 0xFF, 0x00);
		/* Four consecutive writes to set 0V as static data input */
		snd_soc_write(codec, TAPAN_A_CDC_RX2_B1_CTL, 0x00);
		snd_soc_write(codec, TAPAN_A_CDC_RX2_B1_CTL, 0x00);
		snd_soc_write(codec, TAPAN_A_CDC_RX2_B1_CTL, 0x00);
		snd_soc_write(codec, TAPAN_A_CDC_RX2_B1_CTL, 0x00);

		/* Enable the HPHL and HPHR PA */
		wcd9xxx_enable_static_pa(mbhc, true);
		break;
	case MBHC_ZDET_POST_MEASURE:
		/* Turn off ICAL */
		snd_soc_write(codec, WCD9XXX_A_MBHC_SCALING_MUX_2, 0xF0);

		wcd9xxx_enable_static_pa(mbhc, false);

		/*
		 * Setup CnP wavegen to ramp to the desired
		 * output using a 40ms ramp
		 */

		/* CnP wavegen current to 0.5uA */
		snd_soc_write(codec, WCD9XXX_A_RX_HPH_BIAS_WG_OCP, 0x1A);
		/* Set the current division ratio to 2000 */
		snd_soc_write(codec, WCD9XXX_A_RX_HPH_CNP_WG_CTL, 0xDF);
		/* Set the wavegen timer to max (60msec) */
		snd_soc_write(codec, WCD9XXX_A_RX_HPH_CNP_WG_TIME, 0xA0);
		/* Set the CnP reference current to sc_bias */
		snd_soc_write(codec, WCD9XXX_A_RX_HPH_OCP_CTL, 0x6D);

		snd_soc_write(codec, TAPAN_A_CDC_RX1_B2_CTL, 0x00);
		/* Four consecutive writes to set -10mV as static data input */
		snd_soc_write(codec, TAPAN_A_CDC_RX1_B1_CTL, 0x00);
		snd_soc_write(codec, TAPAN_A_CDC_RX1_B1_CTL, 0x1F);
		snd_soc_write(codec, TAPAN_A_CDC_RX1_B1_CTL, 0x19);
		snd_soc_write(codec, TAPAN_A_CDC_RX1_B1_CTL, 0xAA);

		snd_soc_write(codec, TAPAN_A_CDC_RX2_B2_CTL, 0x00);
		/* Four consecutive writes to set -10mV as static data input */
		snd_soc_write(codec, TAPAN_A_CDC_RX2_B1_CTL, 0x00);
		snd_soc_write(codec, TAPAN_A_CDC_RX2_B1_CTL, 0x1F);
		snd_soc_write(codec, TAPAN_A_CDC_RX2_B1_CTL, 0x19);
		snd_soc_write(codec, TAPAN_A_CDC_RX2_B1_CTL, 0xAA);

		snd_soc_update_bits(codec, WCD9XXX_A_RX_HPH_L_TEST,
				    0x02, 0x02);
		snd_soc_update_bits(codec, WCD9XXX_A_RX_HPH_R_TEST,
				    0x02, 0x02);
		/* Enable the HPHL and HPHR PA and wait for 60mS */
		wcd9xxx_enable_static_pa(mbhc, true);

		snd_soc_update_bits(codec, WCD9XXX_A_MBHC_SCALING_MUX_1,
				    0x7F, 0x40);
		usleep_range(mux_wait_us,
				mux_wait_us + WCD9XXX_USLEEP_RANGE_MARGIN_US);
		break;
	case MBHC_ZDET_PA_DISABLE:
		if (!mbhc->hph_pa_dac_state &&
			(!(test_bit(MBHC_EVENT_PA_HPHL, &mbhc->event_state) ||
			test_bit(MBHC_EVENT_PA_HPHR, &mbhc->event_state))))
			wcd9xxx_enable_static_pa(mbhc, false);
		wcd9xxx_restore_registers(codec, &tapan->reg_save_restore);
		break;
	default:
		dev_dbg(codec->dev, "%s: Case %d not supported\n",
			__func__, stage);
		break;
	}
#undef __wr

	return ret;
}

static void tapan_compute_impedance(struct wcd9xxx_mbhc *mbhc, s16 *l, s16 *r,
					uint32_t *zl, uint32_t *zr)
{
	int zln, zld;
	int zrn, zrd;
	int rl = 0, rr = 0;

	if (!mbhc) {
		pr_err("%s: NULL pointer for MBHC", __func__);
		return;
	}

	zln = (l[1] - l[0]) * TAPAN_ZDET_MUL_FACTOR;
	zld = (l[2] - l[0]);
	if (zld)
		rl = zln / zld;

	zrn = (r[1] - r[0]) * TAPAN_ZDET_MUL_FACTOR;
	zrd = (r[2] - r[0]);
	if (zrd)
		rr = zrn / zrd;

	*zl = rl;
	*zr = rr;
}

static struct firmware_cal *tapan_get_hwdep_fw_cal(struct snd_soc_codec *codec,
		enum wcd_cal_type type)
{
	struct tapan_priv *tapan;
	struct firmware_cal *hwdep_cal;
	if (!codec) {
		pr_err("%s: NULL codec pointer\n", __func__);
		return NULL;
	}
	tapan = snd_soc_codec_get_drvdata(codec);
	hwdep_cal = wcdcal_get_fw_cal(tapan->fw_data, type);
	if (!hwdep_cal) {
		dev_err(codec->dev, "%s: cal not sent by %d\n",
				__func__, type);
		return NULL;
	} else {
		return hwdep_cal;
	}
}

static const struct wcd9xxx_mbhc_cb mbhc_cb = {
	.enable_mux_bias_block = tapan_enable_mux_bias_block,
	.cfilt_fast_mode = tapan_put_cfilt_fast_mode,
	.codec_specific_cal = tapan_codec_specific_cal_setup,
	.switch_cfilt_mode = tapan_codec_switch_cfilt_mode,
	.select_cfilt = tapan_select_cfilt,
	.get_cdc_type = tapan_get_cdc_type,
	.setup_zdet = tapan_setup_zdet,
	.compute_impedance = tapan_compute_impedance,
	.get_hwdep_fw_cal = tapan_get_hwdep_fw_cal,
};

int tapan_hs_detect(struct snd_soc_codec *codec,
		    struct wcd9xxx_mbhc_config *mbhc_cfg)
{
	struct tapan_priv *tapan = snd_soc_codec_get_drvdata(codec);
	return wcd9xxx_mbhc_start(&tapan->mbhc, mbhc_cfg);
}
EXPORT_SYMBOL(tapan_hs_detect);

void tapan_hs_detect_exit(struct snd_soc_codec *codec)
{
	struct tapan_priv *tapan = snd_soc_codec_get_drvdata(codec);
	wcd9xxx_mbhc_stop(&tapan->mbhc);
}
EXPORT_SYMBOL(tapan_hs_detect_exit);

void tapan_event_register(
	int (*machine_event_cb)(struct snd_soc_codec *codec,
				enum wcd9xxx_codec_event),
	struct snd_soc_codec *codec)
{
	struct tapan_priv *tapan = snd_soc_codec_get_drvdata(codec);
	tapan->machine_codec_event_cb = machine_event_cb;
}
EXPORT_SYMBOL(tapan_event_register);

static int tapan_device_down(struct wcd9xxx *wcd9xxx)
{
	struct snd_soc_codec *codec;
	struct tapan_priv *tapan;

	codec = (struct snd_soc_codec *)(wcd9xxx->ssr_priv);
	tapan = snd_soc_codec_get_drvdata(codec);

	snd_soc_card_change_online_state(codec->card, 0);
	set_bit(BUS_DOWN, &tapan->status_mask);

	return 0;
}

static const struct wcd9xxx_mbhc_intr cdc_intr_ids = {
	.poll_plug_rem = WCD9XXX_IRQ_MBHC_REMOVAL,
	.shortavg_complete = WCD9XXX_IRQ_MBHC_SHORT_TERM,
	.potential_button_press = WCD9XXX_IRQ_MBHC_PRESS,
	.button_release = WCD9XXX_IRQ_MBHC_RELEASE,
	.dce_est_complete = WCD9XXX_IRQ_MBHC_POTENTIAL,
	.insertion = WCD9XXX_IRQ_MBHC_INSERTION,
	.hph_left_ocp = WCD9306_IRQ_HPH_PA_OCPL_FAULT,
	.hph_right_ocp = WCD9306_IRQ_HPH_PA_OCPR_FAULT,
	.hs_jack_switch = WCD9306_IRQ_MBHC_JACK_SWITCH,
};

static int tapan_post_reset_cb(struct wcd9xxx *wcd9xxx)
{
	int ret = 0;
	int rco_clk_rate;
	struct snd_soc_codec *codec;
	struct tapan_priv *tapan;
	int count;

	codec = (struct snd_soc_codec *)(wcd9xxx->ssr_priv);
	tapan = snd_soc_codec_get_drvdata(codec);

	mutex_lock(&codec->mutex);

	clear_bit(BUS_DOWN, &tapan->status_mask);

	if (codec->reg_def_copy) {
		pr_debug("%s: Update ASOC cache", __func__);
		kfree(codec->reg_cache);
		codec->reg_cache = kmemdup(codec->reg_def_copy,
						codec->reg_size, GFP_KERNEL);
		if (!codec->reg_cache) {
			pr_err("%s: Cache update failed!\n", __func__);
			mutex_unlock(&codec->mutex);
			return -ENOMEM;
		}
	}

	tapan->machine_codec_event_cb(codec, WCD9XXX_CODEC_EVENT_CODEC_UP);

	snd_soc_card_change_online_state(codec->card, 1);

	if (spkr_drv_wrnd == 1)
		snd_soc_update_bits(codec, TAPAN_A_SPKR_DRV_EN, 0x80, 0x80);

	tapan_update_reg_defaults(codec);
	tapan_update_reg_mclk_rate(wcd9xxx);
	tapan_codec_init_reg(codec);
	ret = tapan_handle_pdata(tapan);
	if (IS_ERR_VALUE(ret))
		pr_err("%s: bad pdata\n", __func__);

	tapan_slim_interface_init_reg(codec);

	wcd9xxx_resmgr_post_ssr(&tapan->resmgr);

	wcd9xxx_resmgr_rm_cond_update_bits(
				&tapan->resmgr,
				WCD9XXX_COND_HPH_MIC,
				TAPAN_A_MICB_2_CTL, 7,
				false);

	wcd9xxx_mbhc_deinit(&tapan->mbhc);

	if (TAPAN_IS_1_0(wcd9xxx->version))
		rco_clk_rate = TAPAN_MCLK_CLK_12P288MHZ;
	else
		rco_clk_rate = TAPAN_MCLK_CLK_9P6MHZ;

	ret = wcd9xxx_mbhc_init(&tapan->mbhc, &tapan->resmgr, codec,
				tapan_enable_mbhc_micbias,
				&mbhc_cb, &cdc_intr_ids, rco_clk_rate,
				TAPAN_CDC_ZDET_SUPPORTED);
	if (ret)
		pr_err("%s: mbhc init failed %d\n", __func__, ret);
	else
		wcd9xxx_mbhc_start(&tapan->mbhc, tapan->mbhc.mbhc_cfg);

	tapan_cleanup_irqs(tapan);
	ret = tapan_setup_irqs(tapan);
	if (ret)
		pr_err("%s: Failed to setup irq: %d\n", __func__, ret);

	for (count = 0; count < NUM_CODEC_DAIS; count++)
		tapan->dai[count].bus_down_in_recovery = true;

	mutex_unlock(&codec->mutex);
	return ret;
}

static struct wcd9xxx_reg_address tapan_reg_address = {
};

static int wcd9xxx_ssr_register(struct wcd9xxx *control,
				int (*device_down_cb)(struct wcd9xxx *wcd9xxx),
				int (*device_up_cb)(struct wcd9xxx *wcd9xxx),
				void *priv)
{
	control->dev_down = device_down_cb;
	control->post_reset = device_up_cb;
	control->ssr_priv = priv;
	return 0;
}

static struct regulator *tapan_codec_find_regulator(
	struct snd_soc_codec *codec,
	const char *name)
{
	int i;
	struct wcd9xxx *core = NULL;

	if (codec == NULL) {
		pr_err("%s: codec not initialized\n", __func__);
		return NULL;
	}
	core = dev_get_drvdata(codec->dev->parent);
	if (core == NULL) {
		dev_err(codec->dev, "%s: core not initialized\n", __func__);
		return NULL;
	}

	for (i = 0; i < core->num_of_supplies; i++) {
		if (core->supplies[i].supply &&
			!strcmp(core->supplies[i].supply, name))
				return core->supplies[i].consumer;
	}
	return NULL;
}

static void tapan_enable_config_rco(struct wcd9xxx *core, bool enable)
{
	struct wcd9xxx_core_resource *core_res = &core->core_res;

	if (enable) {
		/*
		 * Enable Bandgap
		 * sleep required by codec hardware to enable bandgap
		 */
		wcd9xxx_reg_update(core, WCD9XXX_A_BIAS_CENTRAL_BG_CTL,
				   0x80, 0x80);
		wcd9xxx_reg_update(core, WCD9XXX_A_BIAS_CENTRAL_BG_CTL,
				   0x04, 0x04);
		wcd9xxx_reg_update(core, WCD9XXX_A_BIAS_CENTRAL_BG_CTL,
				   0x01, 0x01);
		usleep_range(1000, 1100);
		wcd9xxx_reg_update(core, WCD9XXX_A_BIAS_CENTRAL_BG_CTL,
				   0x80, 0x00);

		/*
		 * Enable RC Oscillator
		 * Add sleep required by codec hardware after each register
		 * write to enable RC oscillator
		 */
		wcd9xxx_reg_update(core, WCD9XXX_A_RC_OSC_FREQ, 0x10, 0x00);
		wcd9xxx_reg_write(core_res, WCD9XXX_A_BIAS_OSC_BG_CTL, 0x17);
		usleep_range(5, 5);
		wcd9xxx_reg_update(core, WCD9XXX_A_RC_OSC_FREQ, 0x80, 0x80);
		wcd9xxx_reg_update(core, WCD9XXX_A_RC_OSC_TEST, 0x80, 0x80);
		usleep_range(10, 10);
		wcd9xxx_reg_update(core, WCD9XXX_A_RC_OSC_TEST, 0x80, 0x00);
		usleep_range(20, 20);
		wcd9xxx_reg_update(core, WCD9XXX_A_CLK_BUFF_EN1, 0x08, 0x08);
		/* Enable MCLK and wait 1ms till it gets enabled */
		wcd9xxx_reg_write(core_res, WCD9XXX_A_CLK_BUFF_EN2, 0x02);
		usleep_range(1000, 1000);
		/* Enable CLK BUFF and wait for 1.2ms */
		wcd9xxx_reg_update(core, WCD9XXX_A_CLK_BUFF_EN1, 0x01, 0x01);
		usleep_range(1000, 1200);

		wcd9xxx_reg_update(core, WCD9XXX_A_CLK_BUFF_EN2, 0x02, 0x00);
		wcd9xxx_reg_update(core, WCD9XXX_A_CLK_BUFF_EN2, 0x04, 0x04);
		wcd9xxx_reg_update(core, WCD9XXX_A_CDC_CLK_MCLK_CTL,
				   0x01, 0x01);
		usleep_range(50, 50);
	} else {
		/*
		 * Disable RC oscillator, reset the registers to their
		 * default values
		 * Add sleep required by codec hardware after each register
		 * write to disable RC oscillator
		 */
		wcd9xxx_reg_update(core, WCD9XXX_A_CLK_BUFF_EN2, 0x04, 0x00);
		usleep_range(50, 100);
		wcd9xxx_reg_update(core, WCD9XXX_A_CLK_BUFF_EN2, 0x02, 0x02);
		wcd9xxx_reg_update(core, WCD9XXX_A_CLK_BUFF_EN1, 0x05, 0x00);
		usleep_range(50, 100);

		wcd9xxx_reg_update(core, WCD9XXX_A_RC_OSC_FREQ, 0x80, 0x00);
		usleep_range(10, 50);
		wcd9xxx_reg_write(core_res, WCD9XXX_A_BIAS_OSC_BG_CTL, 0x16);
		/* Disable Bandgap and wait 100us till it gets disabled */
		wcd9xxx_reg_update(core, WCD9XXX_A_BIAS_CENTRAL_BG_CTL,
				   0x03, 0x00);
		usleep_range(100, 150);
	}

}

static enum tapan_codec_type tapan_get_codec_ver(struct device *cdc_dev,
						 bool sensed)
{
	struct wcd9xxx *core = dev_get_drvdata(cdc_dev->parent);
	u8 reg_val;
	enum tapan_codec_type cdc_type = WCD9306;
	unsigned long timeout;
	bool timedout;
	struct wcd9xxx_core_resource *core_res = &core->core_res;

	if (!core) {
		dev_err(cdc_dev, "%s: core not initialized\n", __func__);
		return cdc_type;
	}

	tapan_enable_config_rco(core, 1);

	if (sensed == false) {
		reg_val = wcd9xxx_reg_read(core_res, TAPAN_A_QFUSE_CTL);
		wcd9xxx_reg_write(core_res, TAPAN_A_QFUSE_CTL,
					(reg_val | 0x03));
	}

	timeout = jiffies + HZ;
	do {
		if ((wcd9xxx_reg_read(core_res, TAPAN_A_QFUSE_STATUS)))
			break;
	} while (!(timedout = time_after(jiffies, timeout)));

	if (wcd9xxx_reg_read(core_res, TAPAN_A_QFUSE_DATA_OUT1) ||
	    wcd9xxx_reg_read(core_res, TAPAN_A_QFUSE_DATA_OUT2)) {
		dev_info(cdc_dev, "%s: wcd9302 detected\n", __func__);
		cdc_type = WCD9302;
	} else
		dev_info(cdc_dev, "%s: wcd9306 detected\n", __func__);

	tapan_enable_config_rco(core, 0);

	return cdc_type;
};

static int tapan_codec_probe(struct snd_soc_codec *codec)
{
	struct wcd9xxx *control;
	struct tapan_priv *tapan;
	struct wcd9xxx_pdata *pdata;
	struct wcd9xxx *wcd9xxx;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	int ret = 0;
	int i, rco_clk_rate;
	void *ptr = NULL;
	struct wcd9xxx_core_resource *core_res;

	codec->control_data = dev_get_drvdata(codec->dev->parent);
	control = codec->control_data;

	wcd9xxx_ssr_register(control, tapan_device_down,
			     tapan_post_reset_cb, (void *)codec);

	dev_info(codec->dev, "%s()\n", __func__);

	tapan = kzalloc(sizeof(struct tapan_priv), GFP_KERNEL);
	if (!tapan) {
		dev_err(codec->dev, "Failed to allocate private data\n");
		return -ENOMEM;
	}
	for (i = 0 ; i < NUM_DECIMATORS; i++) {
		tx_hpf_work[i].tapan = tapan;
		tx_hpf_work[i].decimator = i + 1;
		INIT_DELAYED_WORK(&tx_hpf_work[i].dwork,
			tx_hpf_corner_freq_callback);
	}

	snd_soc_codec_set_drvdata(codec, tapan);

	/* codec resmgr module init */
	wcd9xxx = codec->control_data;
	core_res = &wcd9xxx->core_res;
	pdata = dev_get_platdata(codec->dev->parent);
	ret = wcd9xxx_resmgr_init(&tapan->resmgr, codec, core_res, pdata,
				  &pdata->micbias, &tapan_reg_address,
				  NULL, WCD9XXX_CDC_TYPE_TAPAN);
	if (ret) {
		pr_err("%s: wcd9xxx init failed %d\n", __func__, ret);
		return ret;
	}

	tapan->cp_regulators[CP_REG_BUCK] = tapan_codec_find_regulator(codec,
					WCD9XXX_SUPPLY_BUCK_NAME);
	tapan->cp_regulators[CP_REG_BHELPER] = tapan_codec_find_regulator(codec,
					"cdc-vdd-buckhelper");

	tapan->clsh_d.buck_mv = tapan_codec_get_buck_mv(codec);
	/*
	 * If 1.8 volts is requested on the vdd_cp line, then
	 * assume that S4 is in a dynamically switchable state
	 * and can switch between 1.8 volts and 2.15 volts
	 */
	if (tapan->clsh_d.buck_mv == WCD9XXX_CDC_BUCK_MV_1P8)
		tapan->clsh_d.is_dynamic_vdd_cp = true;
	wcd9xxx_clsh_init(&tapan->clsh_d, &tapan->resmgr);

	if (TAPAN_IS_1_0(control->version))
		rco_clk_rate = TAPAN_MCLK_CLK_12P288MHZ;
	else
		rco_clk_rate = TAPAN_MCLK_CLK_9P6MHZ;


	tapan->fw_data = kzalloc(sizeof(*(tapan->fw_data)), GFP_KERNEL);
	if (!tapan->fw_data) {
		dev_err(codec->dev, "Failed to allocate fw_data\n");
		goto err_nomem_slimch;
	}
	set_bit(WCD9XXX_ANC_CAL, tapan->fw_data->cal_bit);
	set_bit(WCD9XXX_MAD_CAL, tapan->fw_data->cal_bit);
	set_bit(WCD9XXX_MBHC_CAL, tapan->fw_data->cal_bit);
	ret = wcd_cal_create_hwdep(tapan->fw_data,
				WCD9XXX_CODEC_HWDEP_NODE, codec);
	if (ret < 0) {
		dev_err(codec->dev, "%s hwdep failed %d\n", __func__, ret);
		goto err_hwdep;
	}
	ret = wcd9xxx_mbhc_init(&tapan->mbhc, &tapan->resmgr, codec,
				tapan_enable_mbhc_micbias,
				&mbhc_cb, &cdc_intr_ids, rco_clk_rate,
				TAPAN_CDC_ZDET_SUPPORTED);

	if (ret) {
		pr_err("%s: mbhc init failed %d\n", __func__, ret);
		return ret;
	}

	tapan->codec = codec;
	for (i = 0; i < COMPANDER_MAX; i++) {
		tapan->comp_enabled[i] = 0;
		tapan->comp_fs[i] = COMPANDER_FS_48KHZ;
	}
	tapan->intf_type = wcd9xxx_get_intf_type();
	tapan->aux_pga_cnt = 0;
	tapan->aux_l_gain = 0x1F;
	tapan->aux_r_gain = 0x1F;
	tapan->ldo_h_users = 0;
	tapan->micb_2_users = 0;
	tapan->lb_mode = false;
	tapan_update_reg_defaults(codec);
	tapan_update_reg_mclk_rate(wcd9xxx);
	tapan_codec_init_reg(codec);
	ret = tapan_handle_pdata(tapan);
	if (IS_ERR_VALUE(ret)) {
		dev_err(codec->dev, "%s: bad pdata\n", __func__);
		goto err_pdata;
	}

	tapan->spkdrv_reg =
		tapan_codec_find_regulator(codec, WCD9XXX_VDD_SPKDRV_NAME);
	if (spkr_drv_wrnd > 0) {
		WCD9XXX_BG_CLK_LOCK(&tapan->resmgr);
		wcd9xxx_resmgr_get_bandgap(&tapan->resmgr,
					   WCD9XXX_BANDGAP_AUDIO_MODE);
		WCD9XXX_BG_CLK_UNLOCK(&tapan->resmgr);
	}

	ptr = kmalloc((sizeof(tapan_rx_chs) +
		       sizeof(tapan_tx_chs)), GFP_KERNEL);
	if (!ptr) {
		pr_err("%s: no mem for slim chan ctl data\n", __func__);
		ret = -ENOMEM;
		goto err_hwdep;
	}

	if (tapan->intf_type == WCD9XXX_INTERFACE_TYPE_I2C) {
		snd_soc_dapm_new_controls(dapm, tapan_dapm_i2s_widgets,
			ARRAY_SIZE(tapan_dapm_i2s_widgets));
		snd_soc_dapm_add_routes(dapm, audio_i2s_map,
			ARRAY_SIZE(audio_i2s_map));
		for (i = 0; i < ARRAY_SIZE(tapan_i2s_dai); i++)
			INIT_LIST_HEAD(&tapan->dai[i].wcd9xxx_ch_list);
	} else if (tapan->intf_type == WCD9XXX_INTERFACE_TYPE_SLIMBUS) {
		for (i = 0; i < NUM_CODEC_DAIS; i++) {
			INIT_LIST_HEAD(&tapan->dai[i].wcd9xxx_ch_list);
			init_waitqueue_head(&tapan->dai[i].dai_wait);
		}
		tapan_init_slim_slave_cfg(codec);
	}

	if (codec_ver == WCD9306) {
		snd_soc_add_codec_controls(codec, tapan_9306_snd_controls,
					   ARRAY_SIZE(tapan_9306_snd_controls));
		snd_soc_dapm_new_controls(dapm, tapan_9306_dapm_widgets,
					  ARRAY_SIZE(tapan_9306_dapm_widgets));
		snd_soc_dapm_add_routes(dapm, wcd9306_map,
					ARRAY_SIZE(wcd9306_map));
	} else {
		snd_soc_dapm_add_routes(dapm, wcd9302_map,
					ARRAY_SIZE(wcd9302_map));
	}

	control->num_rx_port = TAPAN_RX_MAX;
	control->rx_chs = ptr;
	memcpy(control->rx_chs, tapan_rx_chs, sizeof(tapan_rx_chs));
	control->num_tx_port = TAPAN_TX_MAX;
	control->tx_chs = ptr + sizeof(tapan_rx_chs);
	memcpy(control->tx_chs, tapan_tx_chs, sizeof(tapan_tx_chs));

	snd_soc_dapm_sync(dapm);

	(void) tapan_setup_irqs(tapan);

	atomic_set(&kp_tapan_priv, (unsigned long)tapan);
	mutex_lock(&dapm->codec->mutex);
	if (codec_ver == WCD9306) {
		snd_soc_dapm_disable_pin(dapm, "ANC HPHL");
		snd_soc_dapm_disable_pin(dapm, "ANC HPHR");
		snd_soc_dapm_disable_pin(dapm, "ANC HEADPHONE");
		snd_soc_dapm_disable_pin(dapm, "ANC EAR PA");
		snd_soc_dapm_disable_pin(dapm, "ANC EAR");
	}
	mutex_unlock(&dapm->codec->mutex);
	snd_soc_dapm_sync(dapm);

	codec->ignore_pmdown_time = 1;

	if (ret)
		tapan_cleanup_irqs(tapan);

	return ret;

err_pdata:
	kfree(ptr);
err_hwdep:
	kfree(tapan->fw_data);
err_nomem_slimch:
	kfree(tapan);
	return ret;
}

static int tapan_codec_remove(struct snd_soc_codec *codec)
{
	struct tapan_priv *tapan = snd_soc_codec_get_drvdata(codec);
	int index = 0;

	WCD9XXX_BG_CLK_LOCK(&tapan->resmgr);
	atomic_set(&kp_tapan_priv, 0);

	if (spkr_drv_wrnd > 0)
		wcd9xxx_resmgr_put_bandgap(&tapan->resmgr,
					   WCD9XXX_BANDGAP_AUDIO_MODE);
	WCD9XXX_BG_CLK_UNLOCK(&tapan->resmgr);

	tapan_cleanup_irqs(tapan);

	/* cleanup MBHC */
	wcd9xxx_mbhc_deinit(&tapan->mbhc);
	/* cleanup resmgr */
	wcd9xxx_resmgr_deinit(&tapan->resmgr);

	for (index = 0; index < CP_REG_MAX; index++)
		tapan->cp_regulators[index] = NULL;

	tapan->spkdrv_reg = NULL;

	kfree(tapan);
	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_tapan = {
	.probe	= tapan_codec_probe,
	.remove	= tapan_codec_remove,

	.read = tapan_read,
	.write = tapan_write,

	.readable_register = tapan_readable,
	.volatile_register = tapan_volatile,

	.reg_cache_size = TAPAN_CACHE_SIZE,
	.reg_cache_default = tapan_reset_reg_defaults,
	.reg_word_size = 1,

	.controls = tapan_common_snd_controls,
	.num_controls = ARRAY_SIZE(tapan_common_snd_controls),
	.dapm_widgets = tapan_common_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(tapan_common_dapm_widgets),
	.dapm_routes = audio_map,
	.num_dapm_routes = ARRAY_SIZE(audio_map),
};

#ifdef CONFIG_PM
static int tapan_suspend(struct device *dev)
{
	dev_dbg(dev, "%s: system suspend\n", __func__);
	return 0;
}

static int tapan_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tapan_priv *tapan = platform_get_drvdata(pdev);
	dev_dbg(dev, "%s: system resume\n", __func__);
	/* Notify */
	wcd9xxx_resmgr_notifier_call(&tapan->resmgr, WCD9XXX_EVENT_POST_RESUME);
	return 0;
}

static const struct dev_pm_ops tapan_pm_ops = {
	.suspend	= tapan_suspend,
	.resume		= tapan_resume,
};
#endif

static int tapan_probe(struct platform_device *pdev)
{
	int ret = 0;

	codec_ver = tapan_get_codec_ver(&pdev->dev, false);

	if (codec_ver == WCD9302) {
		if (wcd9xxx_get_intf_type() == WCD9XXX_INTERFACE_TYPE_SLIMBUS)
			ret = snd_soc_register_codec(&pdev->dev,
				&soc_codec_dev_tapan,
				tapan9302_dai, ARRAY_SIZE(tapan9302_dai));
		else if (wcd9xxx_get_intf_type() == WCD9XXX_INTERFACE_TYPE_I2C)
			ret = snd_soc_register_codec(&pdev->dev,
				&soc_codec_dev_tapan,
				tapan_i2s_dai, ARRAY_SIZE(tapan_i2s_dai));
	} else {
		if (wcd9xxx_get_intf_type() == WCD9XXX_INTERFACE_TYPE_SLIMBUS)
			ret = snd_soc_register_codec(&pdev->dev,
				&soc_codec_dev_tapan,
				tapan_dai, ARRAY_SIZE(tapan_dai));
		else if (wcd9xxx_get_intf_type() == WCD9XXX_INTERFACE_TYPE_I2C)
			ret = snd_soc_register_codec(&pdev->dev,
				&soc_codec_dev_tapan,
				tapan_i2s_dai, ARRAY_SIZE(tapan_i2s_dai));
	}

	return ret;
}
static int tapan_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}
static struct platform_driver tapan_codec_driver = {
	.probe = tapan_probe,
	.remove = tapan_remove,
	.driver = {
		.name = "tapan_codec",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &tapan_pm_ops,
#endif
	},
};

static int __init tapan_codec_init(void)
{
	return platform_driver_register(&tapan_codec_driver);
}

static void __exit tapan_codec_exit(void)
{
	platform_driver_unregister(&tapan_codec_driver);
}

module_init(tapan_codec_init);
module_exit(tapan_codec_exit);

MODULE_DESCRIPTION("Tapan codec driver");
MODULE_LICENSE("GPL v2");
